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

#include "plume_stats.pb-c.h"
#include "ds.h"
#include "ds_dlist.h"
#include "os_time.h"
#include "log.h"
#include "qm.h"

qm_queue_t g_qm_queue;

// log queue
char *g_qm_log_buf = NULL;
int g_qm_log_buf_size = 0;
int g_qm_log_drop_count = 0; // number of dropped lines

void qm_queue_item_free_buf(qm_item_t *qi)
{
    if (qi) {
        // cleanup
        if (qi->topic) {
            free(qi->topic);
            qi->topic = NULL;
        }
        if (qi->buf) {
            free(qi->buf);
            qi->buf = NULL;
        }
    }
}

void qm_queue_item_free(qm_item_t *qi)
{
    qm_queue_item_free_buf(qi);
    if (qi) {
        free(qi);
    }
}

void qm_queue_init()
{
    ds_dlist_init(&g_qm_queue.queue, qm_item_t, qnode);
}

int qm_queue_length()
{
    return g_qm_queue.length;
}

int qm_queue_size()
{
    return g_qm_queue.size;
}

bool qm_queue_head(qm_item_t **qitem)
{
    *qitem = ds_dlist_head(&g_qm_queue.queue);
    if (!*qitem) return false;
    return true;
}

bool qm_queue_tail(qm_item_t **qitem)
{
    *qitem = ds_dlist_tail(&g_qm_queue.queue);
    if (!*qitem) return false;
    return true;
}

bool qm_queue_remove(qm_item_t *qitem)
{
    if (!qitem) return false;
    ds_dlist_remove(&g_qm_queue.queue, qitem);
    g_qm_queue.length--;
    g_qm_queue.size -= qitem->size;
    qm_queue_item_free(qitem);
    return true;
}

bool qm_queue_drop_head()
{
    qm_item_t *qitem;
    if (!qm_queue_head(&qitem)) return false;
    return qm_queue_remove(qitem);
}

bool qm_queue_make_room(qm_item_t *qi, qm_response_t *res)
{
    if (qi->size > QM_MAX_QUEUE_SIZE_BYTES) {
        // message too big to fit in queue
        return false;
    }
    while (g_qm_queue.length >= QM_MAX_QUEUE_DEPTH
            || g_qm_queue.size + qi->size > QM_MAX_QUEUE_SIZE_BYTES)
    {
        qm_queue_drop_head();
        res->qdrop++;
    }
    return true;
}

bool qm_queue_append_item(qm_item_t **qitem, qm_response_t *res)
{
    qm_item_t *qi = *qitem;
    qi->size = qi->req.data_size;
    qi->timestamp = time_monotonic();
    if (!qm_queue_make_room(qi, res)) {
        return false;
    }
    ds_dlist_insert_tail(&g_qm_queue.queue, qi);
    g_qm_queue.length++;
    g_qm_queue.size += qi->size;
    // take ownership
    *qitem = NULL;
    return true;
}

bool qm_queue_append_log(qm_item_t **qitem, qm_response_t *res)
{
#ifdef BUILD_REMOTE_LOG
    if (!qm_log_enabled) {
        g_qm_log_drop_count++;
        return true;
    }
    qm_item_t *qi = *qitem;
    int new_size;
    char drop_str[64] = "";
    char *msg = (char*)qi->buf;
    int size = qi->req.data_size;
    // omit terminating nul if present
    if (size > 0 && msg[size - 1] == 0) {
        size--;
    }
    // omit terminating nl if present
    if (size > 0 && msg[size - 1] == '\n') {
        size--;
    }
    if (size == 0) {
        // no message - drop
        res->qdrop++;
        return true;
    }
    if (size >= QM_LOG_QUEUE_SIZE) {
        // too big - drop
        res->qdrop++;
        msg = "--- DROPPED TOO BIG ---";
        size = strlen(msg);
    }
    // size +1 for newline
    new_size = g_qm_log_buf_size + size + 1;
    if (new_size > QM_LOG_QUEUE_SIZE) {
        // log full - drop and count lines
        char *nl = g_qm_log_buf;
        g_qm_log_buf[g_qm_log_buf_size] = 0;
        while (nl && *nl) {
            g_qm_log_drop_count++;
            nl = strchr(nl + 1, '\n');
        }
        if (g_qm_log_drop_count) {
            snprintf(drop_str, sizeof(drop_str), "--- DROPPED %d LINES ---", g_qm_log_drop_count);
        }
        free(g_qm_log_buf);
        g_qm_log_buf = NULL;
        g_qm_log_buf_size = 0;
    }
    new_size = g_qm_log_buf_size + size;
    if (g_qm_log_buf_size) new_size++; // for newline
    if (*drop_str) {
        new_size += strlen(drop_str) + 1;
    }
    // resize buf
    g_qm_log_buf = realloc(g_qm_log_buf, new_size + 1); // +1 for nul term
    assert(g_qm_log_buf);
    // copy drop count
    if (*drop_str) {
        strscpy(g_qm_log_buf, drop_str, new_size);
        g_qm_log_buf_size = strlen(g_qm_log_buf);
    }
    // append newline, if any message already in buf
    if (g_qm_log_buf_size) {
        g_qm_log_buf[g_qm_log_buf_size] = '\n';
        g_qm_log_buf_size++;
        g_qm_log_buf[g_qm_log_buf_size] = 0;
    }
    // append log message
    if (g_qm_log_buf_size + size <= new_size) {
        memcpy(g_qm_log_buf + g_qm_log_buf_size, msg, size);
        g_qm_log_buf_size = new_size;
        g_qm_log_buf[g_qm_log_buf_size] = 0;
    }
#endif
    return true;
}

bool qm_queue_put(qm_item_t **qitem, qm_response_t *res)
{
    bool result;
    if ((*qitem)->req.data_type == QM_DATA_LOG) {
        result = qm_queue_append_log(qitem, res);
        qm_queue_item_free(*qitem);
        *qitem = NULL;
    } else {
        result = qm_queue_append_item(qitem, res);
        if (!result) {
            res->response = QM_RESPONSE_ERROR;
            res->error = QM_ERROR_QUEUE;
        }
    }
    return result;
}

bool qm_queue_get(qm_item_t **qitem)
{
    *qitem = ds_dlist_remove_head(&g_qm_queue.queue);
    if (!*qitem) return false;
    g_qm_queue.length--;
    g_qm_queue.size -= (*qitem)->size;
    return true;
}
