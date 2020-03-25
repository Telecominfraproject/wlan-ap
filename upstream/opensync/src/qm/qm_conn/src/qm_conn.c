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

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>
#include <poll.h>

#include "log.h"
#include "os.h"
#include "os_time.h"
#include "util.h"
#include "qm_conn.h"

#define QM_SOCK_DIR "/tmp/plume/"
#define QM_SOCK_FILENAME QM_SOCK_DIR"qm.sock"
#define QM_SOCK_MAX_PENDING 10
#define QM_COMPACT_SEND_SIZE (64*1024)

extern const char *log_get_name();

// server
bool qm_conn_server(int *pfd)
{
    struct sockaddr_un addr;
    char *path = QM_SOCK_FILENAME;
    int fd;

    mkdir(QM_SOCK_DIR, 0755);
    errno = 0; // ignore dir exist error

    *pfd = -1;
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        LOG(ERR, "socket");
        return false;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (*path == '\0') {
        // hidden
        *addr.sun_path = '\0';
        strscpy(addr.sun_path+1, path+1, sizeof(addr.sun_path)-1);
    } else {
        STRSCPY(addr.sun_path, path);
        unlink(path);
    }
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG(ERR, "bind");
        close(fd);
        return false;
    }
    if (listen(fd, QM_SOCK_MAX_PENDING) < 0) {
        LOG(ERR, "listen");
        close(fd);
        return false;
    }
    *pfd = fd;
    LOG(TRACE, "%s %s", __FUNCTION__, path);

    return true;
}

bool qm_conn_accept(int listen_fd, int *accept_fd)
{
    *accept_fd = accept(listen_fd, NULL, NULL);
    if (*accept_fd < 0) {
        LOG(ERR, "%s: accept %d", __FUNCTION__, errno);
        return false;
    }
    return true;
}

// client

bool qm_conn_client(int *pfd)
{
    struct sockaddr_un addr;
    char *path = QM_SOCK_FILENAME;
    int fd;
    mkdir(QM_SOCK_DIR, 0755);

    *pfd = -1;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if ( fd < 0) {
        LOG(ERR, "socket");
        return false;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (*path == '\0') {
        // hidden
        *addr.sun_path = '\0';
        strscpy(addr.sun_path+1, path+1, sizeof(addr.sun_path)-1);
    } else {
        STRSCPY(addr.sun_path, path);
    }

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        LOG(ERR, "connect %s", path);
        close(fd);
        return false;
    }
    LOG(TRACE, "%s %s", __FUNCTION__, path);

    *pfd = fd;

    return true;
}

// request

void qm_req_init(qm_request_t *req)
{
    static int seq = 0;
    if (!seq) {
        seq = time_monotonic();
    }
    memset(req, 0, sizeof(*req));
    memcpy(req->tag, QM_REQUEST_TAG, sizeof(req->tag));
    STRSCPY(req->sender, log_get_name());
    req->ver = QM_REQUEST_VER;
    req->seq = seq;
    seq++;
}

bool qm_req_valid(qm_request_t *req)
{
    return (memcmp(req->tag, QM_REQUEST_TAG, sizeof(req->tag)) == 0)
            && (req->ver == QM_REQUEST_VER);
}

bool qm_conn_write_req(int fd, qm_request_t *req, char *topic, void *data, int data_size)
{
    int ret;
    int size;
    int total = 0;

    if (topic && *topic) {
        req->topic_len = strlen(topic) + 1;
    } else {
        req->topic_len = 0;
    }

    req->data_size = data_size;

    total = sizeof(*req) + req->topic_len + req->data_size;
    if (total <= QM_COMPACT_SEND_SIZE)
    {
        // merge small messages (<64k) into a single send
        void *msgbuf = malloc(total);
        void *p = msgbuf;
        if (!msgbuf) {
            LOG(ERROR, "%s: out of mem (size:%d)", __FUNCTION__, total);
            return false;
        }
        memcpy(p, req, sizeof(*req));
        p += sizeof(*req);
        if (req->topic_len) {
            memcpy(p, topic, req->topic_len);
            p += req->topic_len;
        }
        if (data_size) {
            memcpy(p, data, data_size);
            p += data_size;
        }
        size = total;
        ret = send(fd, msgbuf, total, MSG_NOSIGNAL);
        free(msgbuf);
        if (ret != total) goto write_err;
    } else {
        size = sizeof(*req);
        ret = send(fd, req, size, MSG_NOSIGNAL);
        if (ret != size) goto write_err;
        total += size;

        size = req->topic_len;
        if (size) {
            ret = send(fd, topic, size, MSG_NOSIGNAL);
            if (ret != size) goto write_err;
        }
        total += size;

        size = req->data_size;
        if (size) {
            ret = send(fd, data, size, MSG_NOSIGNAL);
            if (ret != size) goto write_err;
        }
        total += size;
    }

    LOG(TRACE, "%s(%d t:%s ds:%d): b:%d", __FUNCTION__,
            req->cmd, topic ? topic : "null", data_size, total);

    return true;

write_err:
    LOG(ERR, "%s: write error %d / %d / %d", __FUNCTION__, size, ret, errno);
    return false;

}

bool qm_conn_read_req(int fd, qm_request_t *req, char **topic, void **data)
{
    int ret;
    int size;
    int total = 0;

    *topic = NULL;
    *data = NULL;

    // read req
    size = sizeof(*req);
    ret = read(fd, req, size);
    if (ret != size) goto read_err;
    total += size;

    // read topic
    size = req->topic_len;
    if (size) {
        *topic = calloc(size + 1, 1);
        if (!*topic) goto alloc_err;
        ret = read(fd, *topic, size);
        if (ret != size) goto read_err;
    }
    total += size;

    // read buf
    size = req->data_size;
    if (size) {
        *data = malloc(size);
        if (!*data) goto alloc_err;
        ret = read(fd, *data, size);
        if (ret != size) goto read_err;
    }
    total += size;

    LOG(TRACE, "%s: t:%s ds:%d b:%d", __FUNCTION__, *topic, size, total);

    return true;

alloc_err:
    LOG(ERR, "%s: alloc %d", __FUNCTION__, size);
    goto error;
read_err:
    LOG(ERR, "%s: read error %d / %d / %d", __FUNCTION__, size, ret, errno);
error:
    free(*topic);
    free(*data);
    *topic = NULL;
    *data = NULL;
    return false;
}

// for async call
// complete is set to true when buf has enough data for one request
bool qm_conn_parse_req(void *buf, int buf_size, qm_request_t *req, char **topic, void **data, bool *complete)
{
    int size;
    int total = 0;

    *topic = NULL;
    *data = NULL;
    *complete = false;

    // read req
    size = sizeof(*req);
    if (buf_size < size) {
        LOG(TRACE, "%s: incomplete %d/%d req", __FUNCTION__, buf_size, size);
        return true;
    }
    memcpy(req, buf, size);

    total = size + req->topic_len + req->data_size;
    if (buf_size < total) {
        LOG(TRACE, "%s: incomplete %d/%d total", __FUNCTION__, buf_size, total);
        return true;
    }

    // read topic
    size = req->topic_len;
    if (size) {
        *topic = calloc(size + 1, 1);
        if (!*topic) goto error;
        //ret = read(fd, *topic, size);
        memcpy(*topic, buf + sizeof(*req), size);
    }

    // read buf
    size = req->data_size;
    if (size) {
        *data = malloc(size);
        if (!*data) goto error;
        //ret = read(fd, *data, size);
        memcpy(*data, buf + sizeof(*req) + req->topic_len, size);
    }

    LOG(TRACE, "%s: complete from:%s c:%d to:%s dt:%d ds:%d b:%d", __FUNCTION__,
            req->sender, req->cmd, *topic ? *topic : "null", req->data_type, size, total);
    *complete = true;
    return true;

error:
    LOG(ERR, "%s: alloc %d", __FUNCTION__, size);
    free(*topic);
    free(*data);
    *topic = NULL;
    *data = NULL;
    return false;
}


// response

void qm_res_init(qm_response_t *res, qm_request_t *req)
{
    memset(res, 0, sizeof(*res));
    memcpy(res->tag, QM_RESPONSE_TAG, sizeof(res->tag));
    res->ver = QM_RESPONSE_VER;
    res->seq = req->seq;
    switch (req->cmd) {
        case QM_CMD_STATUS:
            res->response = QM_RESPONSE_STATUS;
            break;
        case QM_CMD_SEND:
            res->response = QM_RESPONSE_RECEIVED;
            break;
        default:
            break;
    }
}

bool qm_res_valid(qm_response_t *res)
{
    return (memcmp(res->tag, QM_RESPONSE_TAG, sizeof(res->tag)) == 0)
            && (res->ver == QM_RESPONSE_VER);
}

bool qm_conn_write_res(int fd, qm_response_t *res)
{
    int ret;
    int size = sizeof(*res);
    ret = write(fd, res, size);
    if (ret != size) {
        LOG(ERR, "write");
        return false;
    }
    LOG(TRACE, "%s: b:%d", __FUNCTION__, size);
    return true;
}

bool qm_conn_read_res(int fd, qm_response_t *res)
{
    int ret;
    int size;

    size = sizeof(*res);
    errno = 0;
    ret = read(fd, res, size);
    if (ret != size) {
        LOG(ERR, "%s: read error %d / %d / %d", __FUNCTION__, size, ret, errno);
        res->response = QM_RESPONSE_ERROR;
        res->error = QM_ERROR_CONNECT;
        return false;
    }
    if (!qm_res_valid(res)) {
        LOG(ERR, "%s: invalid response %.4s %d", __FUNCTION__, res->tag, res->ver);
        res->response = QM_RESPONSE_ERROR;
        res->error = QM_ERROR_INVALID;
        return false;
    }
    LOG(TRACE, "%s: b:%d", __FUNCTION__, size);
    return true;
}

// send

// res can be NULL
bool qm_conn_get_status(qm_response_t *res)
{
    qm_request_t req;
    qm_req_init(&req);
    req.cmd = QM_CMD_STATUS;
    return qm_conn_send_req(&req, NULL, NULL, 0, res);
}

char *qm_data_type_str(enum qm_req_data_type type)
{
    switch (type) {
        case QM_DATA_RAW:   return "raw";
        case QM_DATA_TEXT:  return "bs";
        case QM_DATA_STATS: return "stats";
        case QM_DATA_LOG:   return "log";
    }
    return "unk";
}

char *qm_response_str(enum qm_response_type x)
{
    switch (x) {
        case QM_RESPONSE_ERROR:    return "error";
        case QM_RESPONSE_STATUS:   return "status";
        case QM_RESPONSE_RECEIVED: return "ok";
        case QM_RESPONSE_IGNORED:  return "ignored";
    }
    return "unk";
}

char *qm_error_str(enum qm_res_error x)
{
    switch (x) {
        case QM_ERROR_NONE:    return "";
        case QM_ERROR_GENERAL: return "error";
        case QM_ERROR_CONNECT: return "connect";
        case QM_ERROR_INVALID: return "invalid";
        case QM_ERROR_QUEUE:   return "queue";
        case QM_ERROR_SEND:    return "send";
    }
    return "unk";
}

char *qm_conn_status_str(enum qm_res_conn_status x)
{
    switch (x) {
        case QM_CONN_STATUS_NO_CONF:      return "no-conf";
        case QM_CONN_STATUS_DISCONNECTED: return "diconnected";
        case QM_CONN_STATUS_CONNECTED:    return "connected";
    }
    return "unk";
}

bool qm_conn_open_fd(int *fd, qm_response_t *res)
{
    qm_response_t res1;

    if (!res) res = &res1;
    MEMZERO(*res);
    if (!qm_conn_client(fd)) {
        res->error = QM_ERROR_CONNECT;
        LOG(ERROR, "connecting to qm");
        return false;
    }
    return true;
}

bool qm_conn_send_fd(int fd, qm_request_t *req, char *topic, void *data, int data_size, qm_response_t *res)
{
    bool result = false;
    qm_response_t res1;
    int ll = LOG_SEVERITY_TRACE;

    if (!req) return false;
    if (!res) res = &res1;
    MEMZERO(*res);

    if (!qm_req_valid(req)) {
        LOG(ERR, "%s: invalid req", __FUNCTION__);
        res->error = QM_ERROR_GENERAL;
        goto out;
    }
    if (fd < 0) {
        res->error = QM_ERROR_CONNECT;
        goto out;
    }
    if (!qm_conn_write_req(fd, req, topic, data, data_size)) {
        res->error = QM_ERROR_CONNECT;
        goto out;
    }
    if (!(req->flags & QM_REQ_FLAG_NO_RESPONSE)) {
        if (!qm_conn_read_res(fd, res)) {
            goto out;
        }
    } else {
        res->response = QM_RESPONSE_IGNORED;
    }
    result = true;
out:
    if (!result || res->response == QM_RESPONSE_ERROR) {
        // on either error set both return value and response type to error
        result = false;
        res->response = QM_RESPONSE_ERROR;
        if (!res->error) res->error = QM_ERROR_GENERAL;
        if (req->cmd != QM_CMD_STATUS) {
            // elevate log to error unless cmd is status request
            ll = LOG_SEVERITY_ERROR;
        }
    }

    LOG_SEVERITY(ll, "%s: req c:%d dt:%d ds:%d to:%s result:%d response:%d err:%d", __FUNCTION__,
            req->cmd, req->data_type, req->data_size, topic ? topic : "null",
            result, res->response, res->error);

    if (result && req->cmd == QM_CMD_SEND) {
        LOG(DEBUG, "Sent message to QM (size: %d type: %s)",
                data_size, qm_data_type_str(req->data_type));
    }

    return result;
}

// all params except req can be NULL
// returns true if message exchange succesfull and response is not of error type
// on error details can be found in res->error
bool qm_conn_send_req(qm_request_t *req, char *topic, void *data, int data_size, qm_response_t *res)
{
    int fd = -1;
    bool result = false;
    if (!qm_conn_open_fd(&fd, res)) {
        return false;
    }
    result = qm_conn_send_fd(fd, req, topic, data, data_size, res);
    close(fd);
    return result;
}

bool qm_conn_send_custom(
        qm_data_type_t data_type,
        qm_compress_t compress,
        uint32_t flags,
        char *topic,
        void *data,
        int data_size,
        qm_response_t *res)
{
    qm_request_t req;
    qm_req_init(&req);
    req.cmd = QM_CMD_SEND;
    req.data_type = data_type;
    req.compress = compress;
    req.flags = flags;
    return qm_conn_send_req(&req, topic, data, data_size, res);
}

bool qm_conn_send_raw(char *topic, void *data, int data_size, qm_response_t *res)
{
    return qm_conn_send_custom(
            QM_DATA_RAW, QM_REQ_COMPRESS_DISABLE, 0,
            topic, data, data_size, res);
}

bool qm_conn_send_direct(qm_compress_t compress, char *topic,
        void *data, int data_size, qm_response_t *res)
{
    return qm_conn_send_custom(
            QM_DATA_RAW, compress,
            QM_REQ_FLAG_SEND_DIRECT,
            topic, data, data_size, res);
}

bool qm_conn_send_stats(void *data, int data_size, qm_response_t *res)
{
    qm_request_t req;
    qm_req_init(&req);
    req.cmd = QM_CMD_SEND;
    req.data_type = QM_DATA_STATS;
    req.compress = QM_REQ_COMPRESS_IF_CFG;
    return qm_conn_send_req(&req, NULL, data, data_size, res);
}

// streaming api
// persistent connection for less overhead
// auto-reconnect on connection error

bool qm_conn_open(qm_conn_t *qc)
{
    MEMZERO(*qc);
    qc->init = true;
    qc->fd = -1;
    return qm_conn_open_fd(&qc->fd, &qc->res);
}

bool qm_conn_reopen(qm_conn_t *qc)
{
    if (!qc->init) return false;
    if (qc->fd > 0) close(qc->fd);
    qc->fd = -1;
    return qm_conn_open_fd(&qc->fd, &qc->res);
}

bool qm_conn_check_reconnect(qm_conn_t *qc)
{
    if (!qc->init) return false;
    if (qc->fd < 0) {
        // fd not open - reopen
        return qm_conn_reopen(qc);
    }
    // check if socket in good state
    int ret;
    struct pollfd pfd = {0,0,0};
    pfd.fd = qc->fd;
    pfd.events = 0;
    errno = 0;
    ret = poll(&pfd, 1, 0);
    if ((ret == 1) && (pfd.revents & (POLLHUP | POLLERR | POLLNVAL))) {
        // socket in a bad state, reopen
        return qm_conn_reopen(qc);
    }
    return true;
}

bool qm_conn_close(qm_conn_t *qc)
{
    MEMZERO(qc->res);
    if (!qc->init) return false;
    if (qc->fd > 0) {
        close(qc->fd);
        qc->fd = -1;
    }
    qc->init = false;
    return true;
}

bool qm_conn_send_stream(qm_conn_t *qc, qm_request_t *req, char *topic, void *data, int data_size, qm_response_t *res)
{
    bool result = false;
    if (!qc || !qc->init) {
        if (res) MEMZERO(*res);
        return false;
    }
    // check if remote closed and try to reconnect
    if (!qm_conn_check_reconnect(qc)) {
        return false;
    }
    // send
    result = qm_conn_send_fd(qc->fd, req, topic, data, data_size, &qc->res);
    if (!result && (qc->res.error == QM_ERROR_CONNECT)) {
        // on connection error try to reconnect and resend
        if (!qm_conn_reopen(qc)) goto out;
        result = qm_conn_send_fd(qc->fd, req, topic, data, data_size, &qc->res);
    }
out:
    if (res) { *res = qc->res; }
    return result;
}

qm_conn_t qm_conn_log_handle;

bool qm_conn_send_log(char *msg, qm_response_t *res)
{
    qm_conn_t *qc = &qm_conn_log_handle;
    if (!qc->init) {
        qm_conn_open(qc);
    }
    qm_request_t req;
    qm_req_init(&req);
    req.cmd = QM_CMD_SEND;
    req.data_type = QM_DATA_LOG;
    req.compress = QM_REQ_COMPRESS_DISABLE;
    req.flags = QM_REQ_FLAG_NO_RESPONSE;
    return qm_conn_send_stream(qc, &req, NULL, msg, strlen(msg), res);
}

void qm_conn_log_close()
{
    qm_conn_t *qc = &qm_conn_log_handle;
    qm_conn_close(qc);
}


