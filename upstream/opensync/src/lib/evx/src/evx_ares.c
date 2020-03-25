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

#include <evx.h>
#include <ares.h>
#include "log.h"

static void io_cb (EV_P_ ev_io *w, int revents) {
    evx_ares * eares = (evx_ares *) w;
    ares_socket_t rfd = ARES_SOCKET_BAD, wfd = ARES_SOCKET_BAD;

    LOGI("%s: fd: %d revents: %d", __func__, w->fd, revents);

    if (revents & EV_READ)
        rfd = w->fd;
    if (revents & EV_WRITE)
        wfd = w->fd;

    ares_process_fd(eares->ares.channel, rfd, wfd);
}

static void evx_ares_sock_state_cb(void *data, int s, int read, int write) {
    evx_ares        *eares;

    eares = (evx_ares *) data;

    LOGI("%s: read: %d write: %d s: %d fd: %d", __func__, read, write, s, eares->io.fd);

    if (ev_is_active(&eares->io) && eares->io.fd != s) {
        LOGI("%s: Different socket id", __func__);
        return;
    }

    if (read || write) {
        ev_io_set(&eares->io, s, (read ? EV_READ : 0) | (write ? EV_WRITE : 0) );
        ev_io_start(eares->loop, &eares->io );
    }
    else {
        ev_io_stop(eares->loop, &eares->io);
        ev_io_set(&eares->io, -1, 0);
    }
}

int evx_init_ares(struct ev_loop * loop, evx_ares *eares_p) {
    int optmask;
    int status;

    memset(eares_p, 0,sizeof(*eares_p));

    /* In this place the good idea is used ares_library
     * initialized function.
     * This function was first introduced in c-ares version 1.11.0
     * It could be done if we switch all ares lib to version 1.11.0
     */
    status = ares_library_init(ARES_LIB_INIT_ALL);
    if (status != ARES_SUCCESS)
            return -1;

    optmask = ARES_OPT_SOCK_STATE_CB;
    eares_p->loop = loop;
    eares_p->ares.options.sock_state_cb_data = eares_p;
    eares_p->ares.options.sock_state_cb = evx_ares_sock_state_cb;
    eares_p->ares.options.flags =  optmask;

    ev_init(&eares_p->io, io_cb);
    eares_p->chan_initialized = 0;

    return 0;
}

int evx_init_default_chan_options(evx_ares *eares_p) {
    int status;

    if (!eares_p->chan_initialized) {
        status = ares_init_options(&eares_p->ares.channel,
                                   &eares_p->ares.options,
                                   ARES_OPT_SOCK_STATE_CB);
        if (status != ARES_SUCCESS) {
            LOGW("%s failed = %d", __func__, status);
            return -1;
        }
    }
    eares_p->chan_initialized = 1;
    return 0;
}

void evx_stop_ares(evx_ares *eares_p)
{
    ares_destroy(eares_p->ares.channel);
    eares_p->chan_initialized = 0;
    ares_library_cleanup();
}

void
evx_ares_trigger_ares_process(evx_ares *eares_p)
{
    struct timeval   tv, *tvp;
    fd_set           readers, writers;
    int              nfds, count;

    LOGI("evx: trigger ares process");

    FD_ZERO(&readers);
    FD_ZERO(&writers);
    nfds = ares_fds(eares_p->ares.channel, &readers, &writers);
    if (nfds == 0) {
        LOGI("evx: ares no nfds");
        evx_stop_ares(eares_p);
        return;
    }

    tvp = ares_timeout(eares_p->ares.channel, NULL, &tv);
    count = select(nfds, &readers, &writers, NULL, tvp);

    LOGI("evx: ares timeout: count = %d, tv: %ld.%06ld tvp: %ld.%06ld",
         count, tv.tv_sec, tv.tv_usec, tvp->tv_sec, tvp->tv_usec);

    ares_process(eares_p->ares.channel, &readers, &writers);
}
