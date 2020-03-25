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
#include <assert.h>
#include <ev.h>

#include "log.h"
#include "qm.h"
#include "qm_conn.h"
#include "os.h"


static int g_qm_sock = -1;
static ev_io g_qm_sock_ev;

typedef struct qm_async_ctx
{
    int fd;
    ev_io io;
    void *buf;
    int allocated;
    int size;
    bool used;
} qm_async_ctx_t;

#define QM_MAX_CTX 20
#define QM_BUF_CHUNK (64*1024)

qm_async_ctx_t g_qm_async[QM_MAX_CTX];


void qm_res_status(qm_response_t *res)
{
    res->qlen = qm_queue_length();
    res->qsize = qm_queue_size();
    if (!qm_mqtt_config_valid()) {
        res->conn_status = QM_CONN_STATUS_NO_CONF;
    }
    else if (!qm_mqtt_is_connected()) {
        res->conn_status = QM_CONN_STATUS_DISCONNECTED;
    }
    else {
        res->conn_status = QM_CONN_STATUS_CONNECTED;
    }
    res->log_size = g_qm_log_buf_size;
    res->log_drop = g_qm_log_drop_count;
}

void qm_enqueue_and_reply(int fd, qm_item_t *qi)
{
    qm_request_t *req = &qi->req;
    qm_response_t res;

    LOG(TRACE, "%s", __FUNCTION__);
    // enqueue
    qm_res_init(&res, req);
    if (req->cmd == QM_CMD_SEND && req->data_size) {
        if (req->flags & QM_REQ_FLAG_SEND_DIRECT) {
            qm_mqtt_send_message(qi, &res);
        } else {
            qm_queue_put(&qi, &res); // sets qi to NULL if successful
        }
    }
    // free queue item if not enqueued
    if (qi) qm_queue_item_free(qi);
    // reply
    if (!(req->flags & QM_REQ_FLAG_NO_RESPONSE)) {
        // send response if not disabled by flag
        qm_res_status(&res);
        qm_conn_write_res(fd, &res);
    }
}

qm_async_ctx_t* qm_ctx_new()
{
    int i;
    qm_async_ctx_t *ctx;
    for (i=0; i<QM_MAX_CTX; i++)
    {
        ctx = &g_qm_async[i];
        if (!ctx->used) {
            // found
            return ctx;
        }
    }
    return NULL;
}

int qm_ctx_idx(qm_async_ctx_t *ctx)
{
    return ((void*)ctx - (void*)&g_qm_async) / sizeof(*ctx);
}

void qm_ctx_freebuf(qm_async_ctx_t *ctx)
{
    if (ctx->buf) free(ctx->buf);
    ctx->buf = NULL;
    ctx->allocated = 0;
    ctx->size = 0;
}

void qm_ctx_shift_buf(qm_async_ctx_t *ctx, int size)
{
    assert(size <= ctx->size);
    ctx->size -= size;
    if (ctx->size == 0) {
        qm_ctx_freebuf(ctx);
    } else {
        memmove(ctx->buf, ctx->buf + size, ctx->size);
    }
}

void qm_ctx_release(qm_async_ctx_t *ctx)
{
    qm_ctx_freebuf(ctx);
    ev_io_stop(EV_DEFAULT, &ctx->io);
    close(ctx->fd);
    ctx->fd = -1;
    ctx->used = false;
}

// return false on error
bool qm_async_handle_req(qm_async_ctx_t *ctx)
{
    qm_item_t *qi = NULL;
    bool ret = false;
    bool complete;
    int size;

    LOG(TRACE, "%s", __FUNCTION__);

    for (;;) {
        complete = false;
        qi = calloc(sizeof(*qi), 1);
        if (!qi) return false;

        ret = qm_conn_parse_req(ctx->buf, ctx->size, &qi->req, &qi->topic, &qi->buf, &complete);
        if (ret && complete) {
            // shift consumed data in ctx buf
            size = sizeof(qi->req) + qi->req.topic_len + qi->req.data_size;
            qm_ctx_shift_buf(ctx, size);
            // enqueue
            qi->size = qi->req.data_size;
            qm_enqueue_and_reply(ctx->fd, qi);
        } else {
            qm_queue_item_free(qi);
            break;
        }
    }
    return ret;
}

void qm_async_callback(struct ev_loop *ev, struct ev_io *io, int event)
{
    qm_async_ctx_t *ctx = io->data;
    int i = qm_ctx_idx(ctx);
    int free = ctx->allocated - ctx->size;
    int ret;
    int new_size;
    void *new_buf;
    bool result;

    if (!(event & EV_READ)) return;

    if (free < QM_BUF_CHUNK) {
        new_size = ctx->allocated + QM_BUF_CHUNK;
        new_buf = realloc(ctx->buf, new_size);
        if (!new_buf) {
            LOG(ERR, "%s alloc %d", __FUNCTION__, new_size);
            goto release;
        }
        ctx->buf = new_buf;
        ctx->allocated = new_size;
    }
    free = ctx->allocated - ctx->size;

    ret = read(ctx->fd, ctx->buf + ctx->size, free);
    if (ret < 0) {
        LOG(ERR, "%s read %d %d %d", __FUNCTION__, ctx->size, ret, errno);
        goto release;
    }
    ctx->size += ret;
    LOG(TRACE, "%s ctx:%d fd:%d t:%d r:%d", __FUNCTION__, i, ctx->fd, ctx->size, ret);
    if (ret == 0) {
        // EOF
        goto release;
    }

    result = qm_async_handle_req(ctx);
    if (result) {
        // no error
        return;
    }
    // error: release ctx

release:
    qm_ctx_release(ctx);
}

bool qm_async_new(int fd)
{
    qm_async_ctx_t *ctx;
    ctx = qm_ctx_new();
    if (!ctx) {
        return false;
    }
    MEMZERO(*ctx);
    ctx->fd = fd;
    ev_io_init(&ctx->io, qm_async_callback, fd, EV_READ);
    ctx->io.data = ctx;
    ev_io_start(EV_DEFAULT, &ctx->io);
    LOG(TRACE, "%s ctx:%d fd:%d", __FUNCTION__, qm_ctx_idx(ctx), fd);
    ctx->used = true;
    return true;
}

void qm_sock_callback(struct ev_loop *ev, struct ev_io *io, int event)
{
    //void *data = io->data
    int fd;
    if (event & EV_READ)
    {
        if (!qm_conn_accept(g_qm_sock, &fd)) return;
        qm_async_new(fd);
    }
}

// server

bool qm_event_init()
{
    qm_queue_init();

    if (!qm_conn_server(&g_qm_sock)) {
        return false;
    }

    ev_io_init(&g_qm_sock_ev, qm_sock_callback, g_qm_sock, EV_READ);
    //g_qm_sock_ev.data = ...;
    ev_io_start(EV_DEFAULT, &g_qm_sock_ev);

    return true;
}

