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

// cm2 address resolution
#include <arpa/inet.h>
#include <netdb.h>

#include "log.h"
#include "cm2.h"

static int resolve_pending;

static int cm2_restart_ares_resolve(evx_ares *eares_p) {
    LOGI("ares: resolve_pending = %d chan_initialized = %d",
         resolve_pending, eares_p->chan_initialized);
    if (resolve_pending) {
        if (eares_p->chan_initialized) {
            LOGI("ares: channel is destroying...");
            ares_destroy(eares_p->ares.channel);
            eares_p->chan_initialized = 0;
        }
        resolve_pending = 0;
        return -1;
    } else if (!eares_p->chan_initialized) {
        if (evx_init_default_chan_options(eares_p) != 0) {
            LOGW("%s: init channel options failed", __func__);
            return -1;
        }
    }
    return 0;
}

void cm2_free_addr_list(cm2_addr_t *addr)
{
    int i;

    if (addr->h_addr_list) {
        for (i = 0; !addr->h_addr_list[i]; i++) {
            if (!addr->h_addr_list[i]) {
                free(addr->h_addr_list[i]);
                addr->h_addr_list[i] = NULL;
            }
        }
        free(addr->h_addr_list);
        addr->h_addr_list = NULL;
    }
}

static void
cm2_ares_host_cb(void *arg, int status, int timeouts, struct hostent *hostent)
{
    cm2_addr_t *addr;
    char       buf[INET6_ADDRSTRLEN];
    int        cnt;
    int        i;

    addr = (cm2_addr_t *) arg;

    LOGI("Ares cb: status[%d]: %s  Timeouts: %d\n", status, ares_strerror(status), timeouts);

    switch(status) {
        case ARES_SUCCESS:
            LOGN("Got address of host %s, timeouts: %d\n", hostent->h_name, timeouts);

            for (i = 0; hostent->h_addr_list[i]; ++i) {
                inet_ntop(hostent->h_addrtype, hostent->h_addr_list[i], buf, INET6_ADDRSTRLEN);
                LOGI("Addr%d: %s\n", i, buf);
             }

             cnt = i;
             addr->h_addr_list = (char **) malloc(sizeof(char*) * (cnt + 1));

            for (i = 0; i < cnt; i++) {
                addr->h_addr_list[i] = (char *) malloc(sizeof(char) * hostent->h_length);
                memcpy(addr->h_addr_list[i], hostent->h_addr_list[i], hostent->h_length);
            }
            addr->h_addr_list[i] = NULL;
            addr->resolved = true;
            addr->h_addrtype = hostent->h_addrtype;;
            addr->h_cur_idx = 0;
            break;
        case ARES_ETIMEOUT:
        case ARES_EDESTRUCTION:
        case ARES_ECANCELLED:
            cm2_update_state(CM2_REASON_OVS_INIT);
            break;
        case ARES_ECONNREFUSED:
        default:
            /* Keep resolve_pending set, in next interation
             * channel will be destroyed.
             */
            LOGI("Didn't got address reason: status = %d, %d timeouts\n", status, timeouts);
            return;
    }
    resolve_pending = 0;

    return;
}

bool cm2_resolve(cm2_dest_e dest)
{
    cm2_addr_t *addr = cm2_get_addr(dest);

    addr->updated = false;
    addr->resolved = false;

    if (!addr->valid)
        return false;

    LOGI("ares resolving:'%s'", addr->resource);

    if (cm2_restart_ares_resolve(&g_state.eares) < 0)
        return false;

    cm2_free_addr_list(addr);
    //ipv4
    resolve_pending = 1;
    LOGI("ares trigger get hostname");
    ares_gethostbyname(g_state.eares.ares.channel, addr->hostname, AF_INET, cm2_ares_host_cb, (void *) addr);
    //ipv6
    //ares_gethostbyname(g_state.eares.ares.channel, addr->hostname, AF_INET6, cm2_ares_host_cb, (void *) addr);
    return true;
}

bool cm2_resolve_handle_process(void)
{
    if (!g_state.eares.chan_initialized)
        return false;

    evx_ares_trigger_ares_process(&g_state.eares);
    return true;
}

static bool cm2_write_target_addr(cm2_addr_t *addr)
{
    char target[256];
    char *buf;

    if (!addr->h_addr_list)
        return false;

    buf = addr->h_addr_list[addr->h_cur_idx];
    if (!buf)
        return false;

    if (addr->h_addrtype == AF_INET) {
        char buffer[INET_ADDRSTRLEN] = "";
        const char* result = inet_ntop(AF_INET, buf, buffer, sizeof(buffer));

        if (result == 0)
            return false;

        snprintf(target, sizeof(target), "%s:%s:%d",
                addr->proto,
                buffer,
                addr->port);
    }
    else if (addr->h_addrtype == AF_INET6) {
        char buffer[INET6_ADDRSTRLEN] = "";
        const char* result = inet_ntop(AF_INET6, buf, buffer, sizeof(buffer));
        if (result == 0)
            return false;

        snprintf(target, sizeof(target), "%s:[%s]:%d",
                 addr->proto,
                 buffer,
                 addr->port);
    }
    else
        return false;

    bool ret = cm2_ovsdb_set_Manager_target(target);
    if (ret)
        LOGI("Trying to connect to: %s : %s", cm2_curr_dest_name(), target);

    return ret;
}

bool cm2_write_current_target_addr(void)
{
    cm2_addr_t *addr = cm2_curr_addr();
    return cm2_write_target_addr(addr);
}

bool cm2_write_next_target_addr(void)
{
    cm2_addr_t *addr;

    addr = cm2_curr_addr();
    addr->h_cur_idx++;
    return  cm2_write_target_addr(addr);
}
