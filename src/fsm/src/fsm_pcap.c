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

#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <pcap.h>

#include "log.h"
#include "fsm.h"
#include "json_util.h"
#include "qm_conn.h"
#include "os_types.h"
#include "dppline.h"


static void
fsm_pcap_handler(uint8_t * args, const struct pcap_pkthdr *header,
                 const uint8_t *bytes)
{
    struct net_header_parser net_parser;
    struct fsm_parser_ops *parser_ops;
    struct fsm_session *session;
    size_t len;

    session = (struct fsm_session *)args;

    memset(&net_parser, 0, sizeof(net_parser));
    net_parser.packet_len = header->caplen;
    net_parser.caplen = header->caplen;
    net_parser.data = (uint8_t *)bytes;
    net_parser.pcap_datalink = session->pcaps->pcap_datalink;
    len = net_header_parse(&net_parser);
    if (len == 0) return;

    parser_ops = &session->p_ops->parser_ops;
    parser_ops->handler(session, &net_parser);
}


static void
fsm_pcap_recv_fn(EV_P_ ev_io *ev, int revents)
{
    (void)loop;
    (void)revents;

    struct fsm_session *session = ev->data;
    struct fsm_pcaps *pcaps = session->pcaps;
    pcap_t *pcap = pcaps->pcap;

    /* Ready to receive packets */
    pcap_dispatch(pcap, 1, fsm_pcap_handler, (void *)session);
}


bool fsm_pcap_open(struct fsm_session *session) {
    struct fsm_mgr *mgr = fsm_get_mgr();
    struct fsm_pcaps *pcaps = session->pcaps;
    pcap_t *pcap = NULL;
    char *iface = session->conf->if_name;
    struct bpf_program *bpf = pcaps->bpf;
    char *pkt_filter = session->conf->pkt_capt_filter;
    char pcap_err[PCAP_ERRBUF_SIZE];
    /* TODO: get the mtu from the interface */
    int set_snaplen = 65536;
    int rc;

    if (iface == NULL) return true;

    pcaps->pcap = pcap_create(iface, pcap_err);
    if (pcaps->pcap == NULL) {
        LOGN("PCAP initialization failed for interface %s.",
             iface);
        goto error;
    }

    pcap = pcaps->pcap;
    rc = pcap_set_immediate_mode(pcap, 1);
    if (rc != 0) {
        LOGW("Unable to set %s pcap immediate mode!", iface);
    }

    rc = pcap_set_snaplen(pcap, set_snaplen);
    if (rc != 0) {
        LOGE("Unable to set %s snaplen to %d", iface, set_snaplen);
        goto error;
    }

    rc = pcap_setnonblock(pcap, 1, pcap_err);
    if (rc == -1) {
        LOGE("Unable to set non-blocking mode: %s", pcap_err);
        goto error;
    }

    /* Activate the interface */
    rc = pcap_activate(pcap);
    if (rc != 0) {
        LOGE("Error activating interface %s: %s",
             iface, pcap_geterr(pcap));
        goto error;
    }

    if ((pcap_datalink(pcap) != DLT_EN10MB) &&
        (pcap_datalink(pcap) != DLT_LINUX_SLL))
    {
        LOGE("%s: unsupported data link layer: %d\n",
            __func__, pcap_datalink(pcap));
            goto error;
    }
    pcaps->pcap_datalink = pcap_datalink(pcap);

    rc = pcap_compile(pcap, bpf, pkt_filter, 0, PCAP_NETMASK_UNKNOWN);
    if (rc != 0) {
        LOGE("Error compiling capture filter: '%s'. PCAP error:\n>>> %s",
             pkt_filter, pcap_geterr(pcap));
        pcaps->bpf = NULL;
        goto error;
    }

    rc = pcap_setfilter(pcap, bpf);
    if (rc != 0) {
        LOGE("Error setting the capture filter, error: %s",
             pcap_geterr(pcap));
        goto error;
    }

    /* We need a selectable fd for libev */
    pcaps->pcap_fd = pcap_get_selectable_fd(pcap);
    if (pcaps->pcap_fd < 0) {
        LOGE("Error getting selectable FD (%d). PCAP error:\n>>> %s",
             pcaps->pcap_fd, pcap_geterr(pcap));
        goto error;
    }

    /* Register FD for libev events */
    ev_io_init(&pcaps->fsm_evio, fsm_pcap_recv_fn, pcaps->pcap_fd, EV_READ);
    /* Set user data */
    pcaps->fsm_evio.data = (void *)session;
    /* Start watching it on the default queue */
    ev_io_start(mgr->loop, &pcaps->fsm_evio);

    return true;

  error:
    LOGE("Interface %s registered for snooping returning error.", iface);

    return false;
}

void fsm_pcap_close(struct fsm_session *session) {
    struct fsm_mgr *mgr = fsm_get_mgr();
    struct fsm_pcaps *pcaps = session->pcaps;
    pcap_t *pcap = pcaps->pcap;

    if (ev_is_active(&pcaps->fsm_evio)) {
        ev_io_stop(mgr->loop, &pcaps->fsm_evio);
    }

    if (pcaps->bpf != NULL) {
        pcap_freecode(pcaps->bpf);
        free(pcaps->bpf);
    }

    if (pcap != NULL) {
        pcap_close(pcap);
        pcaps->pcap = NULL;
    }
}
