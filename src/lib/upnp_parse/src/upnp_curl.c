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

#include <curl/curl.h>
#include <ev.h>
#include <time.h>
#include <mxml.h>

#include "log.h"
#include "json_mqtt.h"
#include "upnp_parse.h"

static struct upnp_curl conn_mgr;

struct upnp_curl *get_curl_mgr(void) {
    return &conn_mgr;
}

#define NUM_OF_ELEMENTS 11

static struct upnp_key_val elements[NUM_OF_ELEMENTS] =
{
    { "deviceType", NULL, FSM_UPNP_URL_MAX_SIZE },
    { "friendlyName", NULL, 64 },
    { "manufacturer", NULL, 256 },
    { "manufacturerURL", NULL, FSM_UPNP_URL_MAX_SIZE },
    { "modelDescription", NULL, 128 },
    { "modelName", NULL, 32 },
    { "modelNumber", NULL, 32 },
    { "modelURL", NULL, FSM_UPNP_URL_MAX_SIZE },
    { "serialNumber", NULL, 64 },
    { "UDN", NULL, 164 },
    { "UPC", NULL, 12 },
};

void timer_cb(EV_P_ struct ev_timer *w, int revents);


int
multi_timer_cb(CURLM *multi, long timeout_ms,
               struct upnp_curl *mgr)
{
    ev_timer_stop(mgr->loop, &mgr->timer_event);
    if (timeout_ms > 0)
    {
        double  t = timeout_ms / 1000;
        ev_timer_init(&mgr->timer_event, timer_cb, t, 0.);
        ev_timer_start(mgr->loop, &mgr->timer_event);
    }
    else if (timeout_ms == 0)
    {
        timer_cb(mgr->loop, &mgr->timer_event, 0);
    }
    return 0;
}


void
mcode_or_die(const char *where, CURLMcode code)
{
    const char *s;
    if (code == CURLM_OK) return;;

    switch(code)
    {
    case CURLM_BAD_HANDLE:
        s = "CURLM_BAD_HANDLE";
        break;
    case CURLM_BAD_EASY_HANDLE:
        s = "CURLM_BAD_EASY_HANDLE";
        break;
    case CURLM_OUT_OF_MEMORY:
        s = "CURLM_OUT_OF_MEMORY";
        break;
    case CURLM_INTERNAL_ERROR:
        s = "CURLM_INTERNAL_ERROR";
        break;
    case CURLM_UNKNOWN_OPTION:
        s = "CURLM_UNKNOWN_OPTION";
        break;
    case CURLM_LAST:
        s = "CURLM_LAST";
        break;
    default:
        s = "CURLM_unknown";
        break;
    case CURLM_BAD_SOCKET:
        s = "CURLM_BAD_SOCKET";
    }
    LOGE("%s returns %s\n", where, s);
}


void
init_elements(struct upnp_device_url *url)
{
    elements[0].value = url->dev_type;
    elements[1].value = url->friendly_name;
    elements[2].value = url->manufacturer;
    elements[3].value = url->manufacturer_url;
    elements[4].value = url->model_desc;
    elements[5].value = url->model_name;
    elements[6].value = url->model_num;
    elements[7].value = url->model_url;
    elements[8].value = url->serial_num;
    elements[9].value = url->udn;
    elements[10].value = url->upc;
};


void
upnp_scan_data(struct conn_info *conn)
{
    struct upnp_device_url *url = conn->context;
    struct upnp_curl_buffer *data = &conn->data;
    mxml_node_t *node = NULL;
    const char *temp = NULL;
    mxml_node_t *tree = mxmlLoadString(NULL, data->buf,
                                       MXML_OPAQUE_CALLBACK);
    struct upnp_report to_report = { 0 };
    char *report = NULL;
    size_t len, i;

    if (tree == NULL)
    {
        LOGE("%s: mxml parsing failed", __func__);
        goto fail;
    }

    init_elements(url);
    for (i = 0; i < NUM_OF_ELEMENTS; i++)
    {
        if (strlen(elements[i].key) == 0)
        {
            LOGT("%s: elements[%zu] key is null", __func__, i);
            continue;
        }
        LOGT("%s: looking up %s", __func__, elements[i].key);
        node = mxmlFindElement(tree, tree, elements[i].key,
                               NULL, NULL, MXML_DESCEND);
        if (node == NULL)
        {
            LOGT("%s: %s lookup failed", __func__,
                 elements[i].key);
            continue;
        }

        temp = mxmlGetOpaque(node);
        if (temp == NULL)
        {
            LOGT("%s: %s value lookup failed", __func__,
                 elements[i].key);
            continue;
        }

        LOGT("%s: key %s, value %s", __func__,
             elements[i].key, temp);
        len = (strlen(temp) < elements[i].val_max_len ?
            strlen(temp) : elements[i].val_max_len - 1);
        strncpy(elements[i].value, temp, len);
        elements[i].value[len] = '\0';
    }

    for (i = 0; i < NUM_OF_ELEMENTS; i++)
    {
        if (strlen(elements[i].key) == 0) continue;

        LOGT("%s: %s set to %s", __func__,
             elements[i].key,
             (strlen(elements[i].value) != 0) ? elements[i].value : "None");
    }

    mxmlDelete(tree);
    to_report.first = &elements[0];
    to_report.nelems = NUM_OF_ELEMENTS;
    to_report.url = url;
    url->timestamp = time(NULL);
    report = jencode_upnp_report(url->session, &to_report);
    url->session->ops.send_report(url->session, report);
    url->state = PLM_UPNP_COMPLETE;
    return;

  fail:
    url->state = PLM_UPNP_INIT;
    return;
}


void
upnp_curl_process_conn(struct conn_info *conn)
{
    struct upnp_curl_buffer *data = NULL;
    struct upnp_device_url *url = NULL;

    if (conn == NULL)
    {
        LOGT("%s: conn pointer is null", __func__);
        return;
    }
    url = conn->context;
    data = &conn->data;
    LOGT("%s: data for url %s:\n%s", __func__,
         url->url, data->buf);
    upnp_scan_data(conn);

    return;
}


void
upnp_free_conn(struct conn_info *conn)
{
    struct upnp_curl *mgr = get_curl_mgr();
    struct upnp_curl_buffer *data = &conn->data;

    free(data->buf);
    if (conn->easy)
    {
        curl_multi_remove_handle(mgr->multi, conn->easy);
        curl_easy_cleanup(conn->easy);
    }
    free(conn);
}

void
check_multi_info(struct upnp_curl *mgr)
{
    char *eff_url;
    CURLMsg *msg;
    int msgs_left;
    struct conn_info *conn;
    CURL *easy;
    CURLcode res;

    while ((msg = curl_multi_info_read(mgr->multi, &msgs_left)))
    {
        if (msg->msg != CURLMSG_DONE) continue;

        easy = msg->easy_handle;
        res = msg->data.result;
        curl_easy_getinfo(easy, CURLINFO_PRIVATE, (char **)&conn);
        curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &eff_url);
        LOGT("DONE: %s => (%d) %s", eff_url, res, conn->error);
        curl_multi_remove_handle(mgr->multi, easy);
        curl_easy_cleanup(easy);
        conn->easy = NULL;
        if (res == CURLE_OK) upnp_curl_process_conn(conn);
        else
        {
            struct upnp_device_url *url = conn->context;
            url->state = PLM_UPNP_INIT;
        }
        upnp_free_conn(conn);
    }
}


void
event_cb(EV_P_ struct ev_io *w, int revents)
{
    struct upnp_curl *mgr = (struct upnp_curl *) w->data;
    CURLMcode rc;
    int action;

    LOGT("%s  w %p revents %i", __PRETTY_FUNCTION__, w, revents);
    action = (revents & EV_READ ? CURL_POLL_IN : 0) | (revents & EV_WRITE ?
                                                       CURL_POLL_OUT : 0);
    rc = curl_multi_socket_action(mgr->multi, w->fd, action,
                                  &mgr->still_running);
    mcode_or_die("event_cb: curl_multi_socket_action", rc);
    check_multi_info(mgr);
    if (mgr->still_running <= 0)
    {
        LOGT("last transfer done, kill timeout");
        ev_timer_stop(mgr->loop, &mgr->timer_event);
    }
}


void
timer_cb(EV_P_ struct ev_timer *w, int revents)
{
    struct upnp_curl *mgr = w->data;
    CURLMcode rc;

    rc = curl_multi_socket_action(mgr->multi, CURL_SOCKET_TIMEOUT, 0,
                                  &mgr->still_running);
    mcode_or_die("timer_cb: curl_multi_socket_action", rc);
    check_multi_info(mgr);
}


void
remsock(struct sock_info *f, struct upnp_curl *mgr)
{
    if (f == NULL) return;

    if (f->evset) ev_io_stop(mgr->loop, &f->ev);

    free(f);
}


void
setsock(struct sock_info *f, curl_socket_t s, CURL *e, int act,
        struct upnp_curl *mgr)
{
    int kind = (act & CURL_POLL_IN ? EV_READ : 0) |
                (act&CURL_POLL_OUT ? EV_WRITE : 0);

    f->sockfd = s;
    f->action = act;
    f->easy = e;
    if (f->evset) ev_io_stop(mgr->loop, &f->ev);

    ev_io_init(&f->ev, event_cb, f->sockfd, kind);
    f->ev.data = mgr;
    f->evset = 1;
    ev_io_start(mgr->loop, &f->ev);
}


void
addsock(curl_socket_t s, CURL *easy, int action,
        struct upnp_curl *mgr)
{
    struct sock_info *fdp = calloc(sizeof(struct sock_info), 1);

    fdp->global = mgr;
    setsock(fdp, s, easy, action, mgr);
    curl_multi_assign(mgr->multi, s, fdp);
}


int
sock_cb(CURL *e, curl_socket_t s, int what, void *cbp, void *sockp)
{
    struct upnp_curl *mgr = cbp;
    struct sock_info *fdp = sockp;
    const char *whatstr[] = { "none", "IN", "OUT", "INOUT", "REMOVE"};

    LOGT("socket callback: s=%d e=%p what=%s ", s, e, whatstr[what]);
    if (what == CURL_POLL_REMOVE) remsock(fdp, mgr);
    else if (!fdp) addsock(s, e, what, mgr);
    else setsock(fdp, s, e, what, mgr);

    return 0;
}


size_t write_cb(void *ptr, size_t size, size_t nmemb, void *data)
{
    size_t realsize = size * nmemb;
    struct conn_info *conn = data;
    struct upnp_curl_buffer *upnp_data = NULL;

    if (conn == NULL) goto out;

    upnp_data = &conn->data;
    if (upnp_data->buf == NULL) goto out;

    upnp_data->buf = realloc(upnp_data->buf, upnp_data->size + realsize + 1);
    if (upnp_data->buf == NULL) goto out;

    memcpy(&upnp_data->buf[upnp_data->size], ptr, realsize);
    upnp_data->size += realsize;
    upnp_data->buf[upnp_data->size] = 0;

  out:
    return realsize;
}


void
new_conn(struct upnp_device_url *url)
{
    struct upnp_curl *mgr = get_curl_mgr();
    struct conn_info *conn;
    CURLMcode rc;

    conn = calloc(1, sizeof(struct conn_info));
    if (conn == NULL) return;

    conn->data.buf = malloc(1);
    if (conn->data.buf == NULL) goto err_free_conn;

    conn->data.size = 0;
    conn->error[0]='\0';
    conn->easy = curl_easy_init();
    if (!conn->easy) goto err_free_conn;

    conn->global = mgr;
    conn->url = url->url;
    conn->context = url;

    curl_easy_setopt(conn->easy, CURLOPT_URL, conn->url);
    curl_easy_setopt(conn->easy, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(conn->easy, CURLOPT_WRITEDATA, conn);
    curl_easy_setopt(conn->easy, CURLOPT_ERRORBUFFER, conn->error);
    curl_easy_setopt(conn->easy, CURLOPT_PRIVATE, conn);
    curl_easy_setopt(conn->easy, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(conn->easy, CURLOPT_PROGRESSDATA, conn);
    curl_easy_setopt(conn->easy, CURLOPT_LOW_SPEED_TIME, 3L);
    curl_easy_setopt(conn->easy, CURLOPT_LOW_SPEED_LIMIT, 10L);
    curl_easy_setopt(conn->easy, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(conn->easy, CURLOPT_SSL_VERIFYHOST, 0L);

    rc = curl_multi_add_handle(mgr->multi, conn->easy);
    mcode_or_die("new_conn: curl_multi_add_handle", rc);
    if (rc == CURLM_OK) return;

err_free_conn:
    upnp_free_conn(conn);

    return;
}


void
upnp_curl_init(struct ev_loop *loop)
{
    struct upnp_curl *mgr = get_curl_mgr();

    curl_global_init(CURL_GLOBAL_DEFAULT);
    memset(mgr, 0, sizeof(*mgr));
    mgr->loop = loop;
    mgr->multi = curl_multi_init();

    ev_timer_init(&mgr->timer_event, timer_cb, 0., 0.);
    mgr->timer_event.data = mgr;
    mgr->fifo_event.data = mgr;
    curl_multi_setopt(mgr->multi, CURLMOPT_SOCKETFUNCTION, sock_cb);
    curl_multi_setopt(mgr->multi, CURLMOPT_SOCKETDATA, mgr);
    curl_multi_setopt(mgr->multi, CURLMOPT_TIMERFUNCTION, multi_timer_cb);
    curl_multi_setopt(mgr->multi, CURLMOPT_TIMERDATA, mgr);
}


void
upnp_curl_exit(void)
{
    struct upnp_curl *mgr = get_curl_mgr();

    curl_multi_cleanup(mgr->multi);
    curl_global_cleanup();
}
