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

#ifndef MOSQEV_H_INCLUDED
#define MOSQEV_H_INCLUDED

#include <stdbool.h>
#include <ev.h>
#include <mosquitto.h>
#include <limits.h>
#include <openssl/ssl.h>

#define MOSQEV_CID_SZ       64
#define MOSQEV_TLS_VERSION  "tlsv1.2"

static const char mosqev_ciphers[] = TLS1_TXT_DHE_DSS_WITH_AES_128_SHA256
                                  ":"TLS1_TXT_DHE_RSA_WITH_AES_128_SHA256
                                  ":"TLS1_TXT_DHE_RSA_WITH_AES_256_SHA256
                                  ":"TLS1_TXT_DHE_RSA_WITH_AES_128_GCM_SHA256
                                  ":"TLS1_TXT_DHE_DSS_WITH_AES_128_GCM_SHA256
                                  ":"TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_SHA256
                                  ":"TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_SHA384
                                  ":"TLS1_TXT_ECDHE_RSA_WITH_AES_128_SHA256
                                  ":"TLS1_TXT_ECDHE_RSA_WITH_AES_256_SHA384
                                  ":"TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
                                  ":"TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384
                                  ":"TLS1_TXT_ECDHE_RSA_WITH_AES_128_GCM_SHA256
                                  ":"TLS1_TXT_ECDHE_RSA_WITH_AES_256_GCM_SHA384;

/*
 * ===========================================================================
 * Mosquitto + libev library wrapper structure
 * ===========================================================================
 */

/*
 * Callback handlers
 */
typedef struct mosqev mosqev_t;

typedef void mosqev_log_cbk_t(mosqev_t *self, void *data, int level, const char *str);
typedef void mosqev_cbk_t(mosqev_t *self, void *data, int result);
typedef void mosqev_message_cbk_t(mosqev_t *self, void *data, const char *topic, void *msg, size_t msglen);
typedef void mosqev_subscribe_cbk_t(mosqev_t *self, void *data, int mid, int qos_n, const int *qos_v);
typedef void mosqev_pwd_cbk_t(char *buf, int size, int rwflags, void *data);

struct mosqev
{
    char                me_cid[MOSQEV_CID_SZ];  /* Client ID */
    char                me_host[HOST_NAME_MAX]; /* Broker hostname */
    int                 me_port;                /* Broker port */
    struct mosquitto   *me_mosq;                /* Mosquitto library instance */
    bool                me_connected;           /* Connected state - safe to listen for write requests */
    bool                me_connecting;          /* mosquitto_connect() called but CONNACK yet to be received */
    /* Event loop */
    struct ev_loop     *me_ev;                  /* Libev event loop structure */
    struct ev_timer     me_wtimer;              /* Periodic timer watcher */
    struct ev_io        me_wio;                 /* Socket watcher */
    int                 me_wio_fd;              /* I/O watcher file descriptor */
    int                 me_wio_flags;           /* I/O Watcher flags */
    /* Callbacks */
    void               *me_data;                /* Callback context data */
    mosqev_log_cbk_t   *me_log_cbk;             /* Logging handler */
    mosqev_cbk_t       *me_connect_cbk;         /* Connect handler */
    mosqev_cbk_t       *me_disconnect_cbk;      /* Disconnect handler */
    mosqev_cbk_t       *me_publish_cbk;         /* Publish handler */
    mosqev_message_cbk_t
                       *me_message_cbk;         /* Message handler */
    mosqev_subscribe_cbk_t
                       *me_subscribe_cbk;       /* Subscribe handler */
    mosqev_cbk_t       *me_unsubscribe_cbk;     /* Unsubscribe handler */
    /* Memoized settings for reinit */
    char              *me_cafile;
    char              *me_capath;
    char              *me_certfile;
    char              *me_keyfile;
    char              *me_tls_version;
    char              *me_ciphers;
    int                me_cert_reqs;
    mosqev_pwd_cbk_t  *me_pw_callback;
};


/*
 * Public API
 */
extern bool mosqev_init(mosqev_t *self, const char *cid, struct ev_loop *ev, void *data);
extern void mosqev_del(mosqev_t *self);
extern bool mosqev_connect(mosqev_t *self, char *host, int port);
extern bool mosqev_disconnect(mosqev_t *self);
extern bool mosqev_reconnect(mosqev_t *self);
extern bool mosqev_is_connected(mosqev_t *self);

extern bool mosqev_tls_set(mosqev_t *self, const char *cafile,
        const char *capath, const char *certfile, const char *keyfile,
        mosqev_pwd_cbk_t *pw_callback);

extern bool mosqev_tls_opts_set(mosqev_t *self, int cert_reqs,
        const char *tls_version, const char *ciphers);

extern bool mosqev_publish(mosqev_t *self, int *mid, const char *topic,
        size_t msglen, void *msg, int qos, bool retain);

extern void mosqev_connect_cbk_set(mosqev_t *self, mosqev_cbk_t *cbk);
extern void mosqev_disconnect_cbk_set(mosqev_t *self, mosqev_cbk_t *cbk);
extern void mosqev_publish_cbk_set(mosqev_t *self, mosqev_cbk_t *cbk);
extern void mosqev_message_cbk_set(mosqev_t *self, mosqev_message_cbk_t *cbk);
extern void mosqev_subscribe_cbk_set(mosqev_t *self, mosqev_subscribe_cbk_t *cbk);
extern void mosqev_unsubscribe_cbk_set(mosqev_t *self, mosqev_cbk_t *cbk);
extern void mosqev_log_cbk_set(mosqev_t *self, mosqev_log_cbk_t *cbk);

#endif /* MOSQEV_H_INCLUDED */
