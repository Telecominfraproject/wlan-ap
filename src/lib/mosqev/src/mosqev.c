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

#include "os.h"
#include "log.h"
#include "mosqev.h"
#include "util.h"

#include <resolv.h>
/*
 * ===========================================================================
 *  Defines
 * ===========================================================================
 */
#define MOSQEV_TIMER_POLL  0.2  /**< Timer poll in seconds -- float */
#define MOSQEV_KEEPALIVE   180  /**< Keep alive timer, see the keep-alive parameter to mosquitto_connect() */

#define MODULE_ID LOG_MODULE_ID_MQTT

/*
 * ===========================================================================
 *  Typedefs and static functions
 * ===========================================================================
 */
/* Most of Mosquitto callbacks are in this format */
typedef void mosquitto_cbk_t(mosqev_t *self, void *data, int result);

static void mosqev_wio_check(mosqev_t *self);
static bool mosqev_watcher_start(mosqev_t *self);
static bool mosqev_watcher_stop(mosqev_t *self);
static bool mosqev_watcher_io_set(mosqev_t *self, int fd, int flags);
static void mosqev_watcher_timer_cbk(struct ev_loop *ev, struct ev_timer *wtimer, int event);
static void mosqev_watcher_io_cbk(struct ev_loop *ev, struct ev_io *wio, int event);

static void mosqev_mosquitto_log_cbk(struct mosquitto *mosq,
        void *__self, int level, const char *msg);

static void mosqev_mosquitto_connect_cbk(struct mosquitto *mosq,
        void *__self, int rc);

static void mosqev_mosquitto_disconnect_cbk(struct mosquitto *mosq,
        void *__self, int rc);

static void mosqev_mosquitto_publish_cbk(struct mosquitto *mosq,
        void *__self, int mid);

static void mosqev_mosquitto_message_cbk(struct mosquitto *mosq,
        void *__self, const struct mosquitto_message *msg);

static void mosqev_mosquitto_subscribe_cbk(struct mosquitto *mosq,
        void *__self, int mid, int qos_n, const int *qos_v);

static void mosqev_mosquitto_subscribe_cbk(struct mosquitto *mosq,
        void *__self, int mid, int qos_cont, const int *qos_v);

static void mosqev_mosquitto_unsubscribe_cbk(struct mosquitto *mosq,
        void *__self, int mid);

static void mosqev_init_cbk(mosqev_t *self)
{
    /*
     * Take over mosquitto callbacks -- we need to handle some of them internally
     */
    mosquitto_log_callback_set(self->me_mosq, mosqev_mosquitto_log_cbk);
    mosquitto_connect_callback_set(self->me_mosq, mosqev_mosquitto_connect_cbk);
    mosquitto_disconnect_callback_set(self->me_mosq, mosqev_mosquitto_disconnect_cbk);
    mosquitto_publish_callback_set(self->me_mosq, mosqev_mosquitto_publish_cbk);
    mosquitto_message_callback_set(self->me_mosq, mosqev_mosquitto_message_cbk);
    mosquitto_subscribe_callback_set(self->me_mosq, mosqev_mosquitto_subscribe_cbk);
    mosquitto_unsubscribe_callback_set(self->me_mosq, mosqev_mosquitto_unsubscribe_cbk);
}

static int mosqev_reinit_settings(mosqev_t *self)
{
    int rc;

    LOG(TRACE, "Reinit: cafile=%s capath=%s certfile=%s keyfile=%s pwcb=%p certreqs=%d tls=%s ciphers=%s",
        self->me_cafile,
        self->me_capath,
        self->me_certfile,
        self->me_keyfile,
        (void *)self->me_pw_callback,
        self->me_cert_reqs,
        self->me_tls_version,
        self->me_ciphers);

    rc = mosquitto_tls_set(self->me_mosq,
                           self->me_cafile,
                           self->me_capath,
                           self->me_certfile,
                           self->me_keyfile,
                           (void *)self->me_pw_callback);
    if (rc) {
        LOG(ERR, "Failed to set tls: %s", mosquitto_strerror(rc));
        return rc;
    }

    rc = mosquitto_tls_opts_set(self->me_mosq,
                                self->me_cert_reqs,
                                self->me_tls_version,
                                self->me_ciphers);
    if (rc) {
        LOG(ERR, "Failed to set tls opts: %s", mosquitto_strerror(rc));
        return rc;
    }

    return 0;
}

static int mosqev_reinit(mosqev_t *self)
{
    int rc;

    mosquitto_reinitialise(self->me_mosq, self->me_cid, true, self);
    mosqev_init_cbk(self);

    rc = mosqev_reinit_settings(self);
    if (rc) {
        LOG(ERR, "Failed to reinit settings: %s", mosquitto_strerror(rc));
        return rc;
    }

    return 0;
}

/*
 * ===========================================================================
 *  Public API implementation
 * ===========================================================================
 */

/*
 * Initialize mosqev instance, pointed to by @p self; the @p ev argument is an libev event loop -- EV_DEFAULT can
 * be used.
 *
 * The @p cid parameter can be NULL
 */
bool mosqev_init(mosqev_t *self, const char *cid, struct ev_loop *ev, void *data)
{
    /*
     * Note that there's no way to check if libmosquitto is already initialized -- how to handle this properly?
     */
    memset(self, 0, sizeof(*self));

    self->me_ev = ev;
    STRSCPY(self->me_cid, cid);
    self->me_data = data;

    STRSCPY(self->me_host, "unkown");
    self->me_port = -1;

    /*
     * Initialize watchers
     * */
    ev_timer_init(&self->me_wtimer, mosqev_watcher_timer_cbk,
            MOSQEV_TIMER_POLL, MOSQEV_TIMER_POLL);

    self->me_wtimer.data = self;

    ev_io_init(&self->me_wio, mosqev_watcher_io_cbk,
            0, EV_READ);

    self->me_wio.data = self;

    /*
     * Instantiate a new Mosquitto structure --
     * The 2nd paramater is the "clean_session" parameter, needs to be true if cid is NULL
     */
    self->me_mosq = mosquitto_new(cid, true, self);
    if (self->me_mosq == NULL)
    {
        LOG(ERR, "Error initializing Mosquitto instance: CID: %s", cid);
        return false;
    }

    mosqev_init_cbk(self);

    return true;
}

/*
 * Delete a mosqev instance
 */
void mosqev_del(mosqev_t *self)
{
    mosqev_watcher_stop(self);

    if (self->me_mosq != NULL)
    {
        if (self->me_connected)
        {
            if (mosquitto_disconnect(self->me_mosq) != MOSQ_ERR_SUCCESS)
            {
                LOG(ERR, "Unable to disconnect Mosquitto connection: %s:%d:%s",
                        self->me_host, self->me_port, self->me_cid);
            }
        }

        mosquitto_destroy(self->me_mosq);
    }
}

/*
 * Enable TLS mode; must be called before mosqev_connect() -- just a wrapper around mosquitto_tls_set()
 */
bool mosqev_tls_set(mosqev_t *self,
                    const char *cafile,
                    const char *capath,
                    const char *certfile,
                    const char *keyfile,
                    mosqev_pwd_cbk_t *pw_callback)
{
    int rc;

    LOG(DEBUG, "CAFILE:%s CAPATH:%s CERTFILE:%s KEYFILE:%s PWCBK:%p", cafile, capath, certfile, keyfile, pw_callback);
    rc = mosquitto_tls_set(self->me_mosq, cafile, capath,
            certfile, keyfile, (void *)pw_callback);

#define SETSTR(l, r) do { if (l) free(l); l = NULL; if (r) l = strdup(r); } while (0)
    SETSTR(self->me_cafile, cafile);
    SETSTR(self->me_capath, capath);
    SETSTR(self->me_certfile, certfile);
    SETSTR(self->me_keyfile, keyfile);
    self->me_pw_callback = pw_callback;
#undef SETSTR

    if (rc != MOSQ_ERR_SUCCESS)
    {
        LOG(ERR, "Error setting TLS: %s", mosquitto_strerror(rc));
        return false;
    }

    return true;
}

/*
 * Set TLS options; must be called before mosqev_connect() -- just a wrapper around mosquitto_tls_opts_set()
 */
bool mosqev_tls_opts_set(mosqev_t   *self,
                         int         cert_reqs,
                         const char *tls_version,
                         const char *ciphers)
{
    int rc;

    rc = mosquitto_tls_opts_set(self->me_mosq, cert_reqs,
            tls_version, ciphers);

#define SETSTR(l, r) do { if (l) free(l); l = NULL; if (r) l = strdup(r); } while (0)
    self->me_cert_reqs = cert_reqs;
    SETSTR(self->me_tls_version, tls_version);
    SETSTR(self->me_ciphers, ciphers);
#undef SETSTR

    if (rc != MOSQ_ERR_SUCCESS)
    if (rc != MOSQ_ERR_SUCCESS)
    {
        LOG(ERR, "Error setting TLS options: %s", mosquitto_strerror(rc));
        return false;
    }

    return true;
}

/*
 * Connect to a mosquitto broker -- this is a BLOCKING call
 */
bool mosqev_connect(mosqev_t *self, char *host, int port)
{
    int rc;

    STRSCPY(self->me_host, host);
    self->me_port = port;

    LOG(INFO, "Connecting to: %s:%d:%s", host, port, self->me_cid);

    res_init();

    if (self->me_connecting)
        LOG(WARNING, "Previous session was still connecting");

    if (self->me_connected)
        LOG(WARNING, "Previous session was still connected");

    rc = mosqev_reinit(self);
    if (rc) {
        LOG(ERR, "Connection failed due to reinit: %s", mosquitto_strerror(rc));
        return rc;
    }

    self->me_connecting = false;
    self->me_connected = false;

    mosqev_watcher_stop(self);

    rc = mosquitto_connect(self->me_mosq, self->me_host, self->me_port, MOSQEV_KEEPALIVE);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        LOG(ERR, "Connection failed: %s:%d:%s: Error %s",
                host, port, self->me_cid, mosquitto_strerror(rc));

        return false;
    }

    self->me_connecting = true;

    /* Start watching the connection */
    if (!mosqev_watcher_start(self))
    {
        LOG(ERR, "Error starting watchers: %s:%d:%s",
                self->me_host, self->me_port, self->me_cid);

        return false;
    }

    return true;
}

/*
 * Reconnect
 */
bool mosqev_reconnect(mosqev_t *self)
{
    int rc;

    LOG(INFO, "Reconnecting to: %s:%d:%s", self->me_host, self->me_port, self->me_cid);

    rc = mosquitto_reconnect(self->me_mosq);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        LOG(ERR, "Reconnection failed: %s:%d:%s: Error %s", self->me_host, self->me_port, self->me_cid,
                 mosquitto_strerror(rc));
        return false;
    }

    return true;
}

/*
 * Disconnect
 */
bool mosqev_disconnect(mosqev_t *self)
{
    int rc;

    LOG(INFO, "Disconnecting from: %s:%d:%s", self->me_host, self->me_port, self->me_cid);

    rc = mosquitto_disconnect(self->me_mosq);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        LOG(ERR, "Re-connection failed: %s:%d:%s: Error %s", self->me_host, self->me_port, self->me_cid,
                 mosquitto_strerror(rc));
        return false;
    }

    return true;
}

/*
 * Returns true whether we have an active connection to the MQTT broker.
 */
bool mosqev_is_connected(mosqev_t *self)
{
    return self->me_connected;
}

/*
 * Publish a message
 */
bool mosqev_publish(mosqev_t *self,
                    int *mid,
                    const char *topic,
                    size_t msglen,
                    void *msg,
                    int qos,
                    bool retain)
{
    int rc;

    rc = mosquitto_publish(self->me_mosq, mid, topic, msglen, msg, qos, retain);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        LOG(ERR, "Message publish failed: Topic: %s", topic);
        return false;
    }

    mosquitto_loop(self->me_mosq, 0, 1);

    /* Start monitoring WRITE events if data is still pending to be sent */
    if (mosquitto_want_write(self->me_mosq))
    {
        mosqev_watcher_io_set(self, mosquitto_socket(self->me_mosq), EV_READ | EV_WRITE);
    }

    return true;
}

/*
 * Set the connection callback
 */
void mosqev_log_cbk_set(mosqev_t *self, mosqev_log_cbk_t *cbk)
{
    self->me_log_cbk = cbk;
}

/*
 * Set the connection callback
 */
void mosqev_connect_cbk_set(mosqev_t *self, mosqev_cbk_t *cbk)
{
    self->me_connect_cbk = cbk;
}

/*
 * Set the disconnect callback
 */
void mosqev_disconnect_cbk_set(mosqev_t *self, mosqev_cbk_t *cbk)
{
    self->me_disconnect_cbk = cbk;
}

/*
 * Set the publish callback
 */
void mosqev_publish_cbk_set(mosqev_t *self, mosqev_cbk_t *cbk)
{
    self->me_publish_cbk = cbk;
}

/*
 * Set the message callback
 */
void mosqev_message_cbk_set(mosqev_t *self, mosqev_message_cbk_t *cbk)
{
    self->me_message_cbk = cbk;
}

/*
 * Set the subscribe callback
 */
void mosqev_subscribe_cbk_set(mosqev_t *self, mosqev_subscribe_cbk_t *cbk)
{
    self->me_subscribe_cbk = cbk;
}

/*
 * Set the subscribe callback
 */
void mosqev_cbk_unsubscribe_set(mosqev_t *self, mosqev_cbk_t *cbk)
{
    self->me_unsubscribe_cbk = cbk;
}

/*
 * ===========================================================================
 *  Callback wrappers and handlers
 * ===========================================================================
 */

/*
 * Call userspace log
 */
void mosqev_mosquitto_log_cbk(struct mosquitto *mosq,
                              void *__self,
                              int level,
                              const char *msg)
{
    (void)mosq;

    mosqev_t *self = (mosqev_t *)__self;

    LOG(DEBUG, "Log (%s:%d:%s):%d: %s",
            self->me_host, self->me_port, self->me_cid,
            level, msg);

    if (self->me_log_cbk != NULL)
    {
        self->me_log_cbk(self, self->me_data, level, msg);
    }
}

/*
 * Mark the connection as connected -- this allows us to register for write events on the socket
 */
void mosqev_mosquitto_connect_cbk(struct mosquitto *mosq, void *__self, int rc)
{
    (void)mosq;

    mosqev_t *self = (mosqev_t *)__self;

    if (!self->me_connecting)
        LOG(WARNING, "Unexpected connect callback");

    self->me_connecting = false;

    if (rc == 0)
    {
        self->me_connected = true;
        LOG(INFO, "Connected to %s:%d -- %s", self->me_host, self->me_port, self->me_cid);
    }
    else
    {
        LOG(INFO, "Connection error: %s:%d -- %s", self->me_host, self->me_port, self->me_cid);
        self->me_connected = false;
    }

    if (self->me_connect_cbk != NULL)
    {
        self->me_connect_cbk(self, self->me_data, rc);
    }
}

/*
 * Mark the connection as connected -- this allows us to register for write events on the socket
 */
void mosqev_mosquitto_disconnect_cbk(struct mosquitto *mosq, void *__self, int rc)
{
    (void)mosq;

    mosqev_t *self = (mosqev_t *)__self;

    LOG(INFO, "Disconnected from %s:%d -- %s: Reason %s (%d)",
            self->me_host, self->me_port, self->me_cid, mosquitto_strerror(rc), rc);

    self->me_connected = false;
    self->me_connecting = false;

    if (self->me_disconnect_cbk != NULL)
    {
        self->me_disconnect_cbk(self, self->me_data, rc);
    }
}

/*
 * Call the publish callback
 */
void mosqev_mosquitto_publish_cbk(struct mosquitto *mosq, void *__self, int mid)
{
    (void)mosq;

    mosqev_t *self = (mosqev_t *)__self;

    if (self->me_publish_cbk != NULL)
    {
        self->me_publish_cbk(self, self->me_publish_cbk, mid);
    }
}

/*
 * Call the message callback
 */
void mosqev_mosquitto_message_cbk(struct mosquitto *mosq, void *__self, const struct mosquitto_message *msg)
{
    (void)mosq;

    mosqev_t *self = (mosqev_t *)__self;

    if (self->me_message_cbk != NULL)
    {
        self->me_message_cbk(self, self->me_data, msg->topic, msg->payload, msg->payloadlen);
    }
}

/*
 * Call the subscribe callback
 */
void mosqev_mosquitto_subscribe_cbk(struct mosquitto *mosq, void *__self, int mid, int qos_n, const int *qos_v)
{
    (void)mosq;

    mosqev_t *self = (mosqev_t *)__self;

    LOG(INFO, "Subscribe %s:%d -- %s, mid: %d",
            self->me_host, self->me_port, self->me_cid, mid);

    if (self->me_subscribe_cbk != NULL)
    {
        self->me_subscribe_cbk(self, self->me_publish_cbk, mid, qos_n, qos_v);
    }
}

/*
 * Call the subscribe callback
 */
void mosqev_mosquitto_unsubscribe_cbk(struct mosquitto *mosq, void *__self, int mid)
{
    (void)mosq;

    mosqev_t *self = (mosqev_t *)__self;

    LOG(INFO, "Unsubscribed %s:%d -- %s, mid: %d",
            self->me_host, self->me_port, self->me_cid, mid);

    if (self->me_unsubscribe_cbk != NULL)
    {
        self->me_unsubscribe_cbk(self, self->me_publish_cbk, mid);
    }
}

/*
 * ===========================================================================
 *  libev watcher handlers
 * ===========================================================================
 */

/*
 * Start watching a mosquitto connection; must be called after mosquitto_connect()
 */
bool mosqev_watcher_start(mosqev_t *self)
{
    (void)self;

    LOG(DEBUG, "Starting event monitoring.");

    ev_timer_start(self->me_ev, &self->me_wtimer);

    mosqev_watcher_io_set(self, mosquitto_socket(self->me_mosq), EV_READ);

    return true;
}


/*
 * Stop watching a connection
 */
bool mosqev_watcher_stop(struct mosqev *self)
{
    if (ev_is_active(&self->me_wtimer))
    {
        ev_timer_stop(self->me_ev, &self->me_wtimer);
    }

    if (ev_is_active(&self->me_wio))
    {
        ev_io_stop(self->me_ev, &self->me_wio);
    }

    return true;
}

/*
 * Restart the IO watcher flag in case some parameters like sockets and flags have changed
 */
bool mosqev_watcher_io_set(mosqev_t *self, int fd, int flags)
{
    /*
     * Check if we have to stop the watcher
     */
    if (ev_is_active(&self->me_wio))
    {
        if (self->me_wio_fd == fd &&
            self->me_wio_flags == flags)
        {
            /* Watcher is active and the FD and flags match, nothing to do */
            return true;
        }

        /* Stop the watcher */
        ev_io_stop(self->me_ev, &self->me_wio);
    }

    /*
     * At this point the watcher is stopped -- restart it
     */
    self->me_wio_fd = fd;
    self->me_wio_flags = flags;

    /* Check if the descriptor is valid */
    if (self->me_wio_fd >= 0)
    {
        ev_io_set(&self->me_wio, self->me_wio_fd, self->me_wio_flags);
        ev_io_start(self->me_ev, &self->me_wio);
    }

    return true;
}

/*
 * Check status of the Mosquitto connection -- set the I/O watcher modes accordingly
 */
void mosqev_wio_check(mosqev_t *self)
{
    /*
     * Restart the I/O watcher to include write requests
     */

    /*
     * mosquitto_can_write() returns always true during the whole process of a SSL connection, for
     * this reason we should stick to polling until we are connected (self->me_connected == true)
     */
    if (mosquitto_want_write(self->me_mosq) && self->me_connected)
    {
        mosqev_watcher_io_set(self, mosquitto_socket(self->me_mosq), EV_READ | EV_WRITE);
    }
    else
    {
        mosqev_watcher_io_set(self, mosquitto_socket(self->me_mosq), EV_READ);
    }
}

void mosqev_watcher_timer_cbk(struct ev_loop *ev, struct ev_timer *wtime, int event)
{
    (void)ev;
    (void)event;

    mosqev_t   *self = (mosqev_t *)(wtime->data);

    mosquitto_loop(self->me_mosq, 0, 1);

    mosqev_wio_check(self);
}

/*
 * Poll the socket status, update the I/O watcher, if necessary
 *
 * Do not use mosquitto_loop_misc/read/write -- they are broken for the TLS case!!!
 */
void mosqev_watcher_io_cbk(struct ev_loop *ev, struct ev_io *wio, int event)
{
    (void)ev;
    (void)event;

    mosqev_t *self = (mosqev_t *)(wio->data);

    mosquitto_loop(self->me_mosq, 0, 1);

    mosqev_wio_check(self);
}

