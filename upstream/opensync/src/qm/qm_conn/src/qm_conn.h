/*
Copyright (c) 2015, Plume Design Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the Plume Design Inc. nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef QM_CONN_H_INCLUDED
#define QM_CONN_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>

// request

#define QM_REQUEST_TAG "QREQ"
#define QM_REQUEST_VER 2

enum qm_req_cmd
{
    QM_CMD_STATUS = 1,
    QM_CMD_SEND   = 2,
};

// flag: skip sending response
#define QM_REQ_FLAG_NO_RESPONSE (1<<0)

// flag: send directly to mqtt broker, bypassing the queue and interval
#define QM_REQ_FLAG_SEND_DIRECT (1<<1)

typedef enum qm_req_compress
{
    QM_REQ_COMPRESS_IF_CFG  = 0, // enabled by ovsdb mqtt conf
    QM_REQ_COMPRESS_DISABLE = 1, // disable
    QM_REQ_COMPRESS_FORCE   = 2, // always compress
} qm_compress_t;

// message data type
typedef enum qm_req_data_type
{
    QM_DATA_RAW = 0,
    QM_DATA_TEXT,
    QM_DATA_STATS,
    QM_DATA_LOG,
} qm_data_type_t;

typedef struct qm_request
{
    char tag[4];
    uint32_t ver;
    uint32_t seq;
    uint32_t cmd;
    uint32_t flags;
    char sender[16]; // prog name

    uint8_t set_qos; // if 1 use qos_val instead of ovsdb cfg
    uint8_t qos_val;
    uint8_t compress;
    uint8_t data_type;

    uint32_t interval;
    uint32_t topic_len;
    uint32_t data_size;
    uint32_t reserved;
} qm_request_t;

// response

#define QM_RESPONSE_TAG "RESP"
#define QM_RESPONSE_VER 1

enum qm_response_type
{
    QM_RESPONSE_ERROR    = 0, // error response
    QM_RESPONSE_STATUS   = 1, // status response
    QM_RESPONSE_RECEIVED = 2, // message received confirmation
    QM_RESPONSE_IGNORED  = 3, // response ignored
};

// error type
enum qm_res_error
{
    QM_ERROR_NONE        = 0,   // no error
    QM_ERROR_GENERAL     = 100, // general error
    QM_ERROR_CONNECT     = 101, // error connecting to QM
    QM_ERROR_INVALID     = 102, // invalid response
    QM_ERROR_QUEUE       = 103, // error enqueuing message
    QM_ERROR_SEND        = 104, // error sending to mqtt (for immediate flag)
};

// status of connection from QM to the mqtt server
enum qm_res_conn_status
{
    QM_CONN_STATUS_NO_CONF      = 200,
    QM_CONN_STATUS_DISCONNECTED = 201,
    QM_CONN_STATUS_CONNECTED    = 202,
};

typedef struct qm_response
{
    char tag[4];
    uint32_t ver;
    uint32_t seq;
    uint32_t response;
    uint32_t error;
    uint32_t flags;
    uint32_t conn_status;
    // stats
    uint32_t qlen;  // queue length - number of messages
    uint32_t qsize; // queue size - bytes
    uint32_t qdrop; // num queued messages dropped due to queue full
    uint32_t log_size; // log buffer size
    uint32_t log_drop; // log dropped lines
} qm_response_t;

char *qm_data_type_str(enum qm_req_data_type type);
char *qm_response_str(enum qm_response_type x);
char *qm_error_str(enum qm_res_error x);
char *qm_conn_status_str(enum qm_res_conn_status x);

bool qm_conn_accept(int listen_fd, int *accept_fd);
bool qm_conn_server(int *pfd);
bool qm_conn_client(int *pfd);

void qm_req_init(qm_request_t *req);
bool qm_req_valid(qm_request_t *req);
bool qm_conn_write_req(int fd, qm_request_t *req, char *topic, void *data, int data_size);
bool qm_conn_read_req(int fd, qm_request_t *req, char **topic, void **data);
bool qm_conn_parse_req(void *buf, int buf_size, qm_request_t *req, char **topic, void **data, bool *complete);

void qm_res_init(qm_response_t *res, qm_request_t *req);
bool qm_res_valid(qm_response_t *res);
bool qm_conn_write_res(int fd, qm_response_t *res);
bool qm_conn_read_res(int fd, qm_response_t *res);
bool qm_conn_open_fd(int *fd, qm_response_t *res);
bool qm_conn_send_fd(int fd, qm_request_t *req, char *topic, void *data, int data_size, qm_response_t *res);

// simple api

bool qm_conn_get_status(qm_response_t *res);
bool qm_conn_send_req(qm_request_t *req, char *topic, void *data, int data_size, qm_response_t *res);
bool qm_conn_send_custom(
        qm_data_type_t data_type,
        qm_compress_t compress,
        uint32_t flags,
        char *topic,
        void *data,
        int data_size,
        qm_response_t *res);
bool qm_conn_send_raw(char *topic, void *data, int data_size, qm_response_t *res);
bool qm_conn_send_direct(qm_compress_t compress, char *topic, void *data, int data_size, qm_response_t *res);
bool qm_conn_send_stats(void *data, int data_size, qm_response_t *res);

// streaming api

typedef struct
{
    bool init;
    int  fd;
    qm_response_t res;
} qm_conn_t;

bool qm_conn_open(qm_conn_t *qc);
bool qm_conn_close(qm_conn_t *qc);
bool qm_conn_send_stream(qm_conn_t *qc, qm_request_t *req, char *topic,
        void *data, int data_size, qm_response_t *res);
bool qm_conn_send_log(char *msg, qm_response_t *res);
void qm_conn_log_close();

#endif
