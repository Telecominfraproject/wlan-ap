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

#ifndef QM_H_INCLUDED
#define QM_H_INCLUDED

#include "ev.h"

#include "schema.h"
#include "ds_list.h"
#include "ds_dlist.h"
#include "qm_conn.h"

#define QM_MAX_QUEUE_DEPTH (200)
#define QM_MAX_QUEUE_SIZE_BYTES (2*1024*1024)

#define QM_LOG_QUEUE_SIZE (100*1024) // 100k

// queue item

typedef struct qm_item
{
    qm_request_t req;
    char *topic;
    size_t size;
    void *buf;
    time_t timestamp;
    ds_dlist_node_t qnode;
} qm_item_t;

typedef struct qm_queue
{
    ds_dlist_t queue;
    int length;
    int size;
} qm_queue_t;

extern qm_queue_t g_qm_queue;
extern char *g_qm_log_buf;
extern int   g_qm_log_buf_size;
extern int   g_qm_log_drop_count;
extern bool  qm_log_enabled;

int qm_ovsdb_init(void);

bool qm_mqtt_init(void);
void qm_mqtt_stop(void);
void qm_mqtt_set(const char *broker, const char *port, const char *topic, const char *qos, int compress);
void qm_mqtt_set_log_interval(int log_interval);
bool qm_mqtt_is_connected();
bool qm_mqtt_config_valid();
bool qm_mqtt_send_message(qm_item_t *qi, qm_response_t *res);
void qm_mqtt_send_queue();
void qm_mqtt_reconnect();

void qm_queue_item_free_buf(qm_item_t *qi);
void qm_queue_item_free(qm_item_t *qi);
void qm_queue_init();
int qm_queue_length();
int qm_queue_size();
bool qm_queue_head(qm_item_t **qitem);
bool qm_queue_tail(qm_item_t **qitem);
bool qm_queue_remove(qm_item_t *qitem);
bool qm_queue_drop_head();
bool qm_queue_make_room(qm_item_t *qi, qm_response_t *res);
bool qm_queue_put(qm_item_t **qitem, qm_response_t *res);
bool qm_queue_get(qm_item_t **qitem);

bool qm_event_init();

#endif
