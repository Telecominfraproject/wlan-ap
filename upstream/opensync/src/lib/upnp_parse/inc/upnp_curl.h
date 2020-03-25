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

#ifndef __FSM_UPNP_CURL_H__
#define __FSM_UPNP_CURL_H__

#include <curl/curl.h>
#include <ev.h>

#include "ds_tree.h"
#include "fsm.h"
#include "os_types.h"

struct upnp_curl
{
    struct ev_loop *loop;
    struct ev_io fifo_event;
    struct ev_timer timer_event;
    CURLM *multi;
    int still_running;
};

typedef enum
{
    PLM_UPNP_INIT = 0,
    PLM_UPNP_STARTED,
    PLM_UPNP_COMPLETE,
} upnp_state_t;

#define FSM_UPNP_URL_MAX_SIZE 512
#define FSM_UPNP_UA_MAX_SIZE 1024

struct upnp_key_val
{
    char *key;
    char *value;
    size_t val_max_len;
};

#define REPORT_UPNP_TTL 20*60
#define PROBE_UPNP 20*60


/* The upnp spec defines most of the fields'lengths */
struct upnp_device_url
{
    char url[FSM_UPNP_URL_MAX_SIZE];
    upnp_state_t state;
    time_t timestamp;
    char dev_type[FSM_UPNP_URL_MAX_SIZE];
    char friendly_name[64];
    char manufacturer[256];
    char manufacturer_url[FSM_UPNP_URL_MAX_SIZE];
    char model_desc[128];
    char model_name[32];
    char model_num[32];
    char model_url[FSM_UPNP_URL_MAX_SIZE];
    char serial_num[64];
    char udn[164];
    char upc[12];
    struct upnp_device *udev;
    struct fsm_session *session;
    ds_tree_node_t url_node;
};

struct upnp_device_user_agent
{
    char user_agent[1024];
    ds_tree_node_t ua_node;
};

struct upnp_device
{
    os_macaddr_t device_mac;
    ds_tree_t urls;
    ds_tree_node_t device_node;
};


struct upnp_curl_buffer
{
    int size;
    char *buf;
};

struct conn_info
{
    CURL *easy;
    char *url;
    struct upnp_curl *global;
    char error[CURL_ERROR_SIZE];
    struct upnp_device_url *context;
    struct upnp_curl_buffer data;
};


/* Information associated with a specific socket */
struct sock_info
{
    curl_socket_t sockfd;
    CURL *easy;
    int action;
    long timeout;
    struct ev_io ev;
    int evset;
    struct upnp_curl *global;
};

void
upnp_curl_init(struct ev_loop *loop);

void
upnp_curl_exit(void);

void
new_conn(struct upnp_device_url *url);
#endif /* __FSM_UPNP_CURL_H__ */
