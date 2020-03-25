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

#include <stdlib.h>
#include <stddef.h>
#include <time.h>
#include <string.h>

#include "const.h"
#include "log.h"
#include "json_mqtt.h"
#include "http_parse.h"
#include "assert.h"
#include "json_util.h"
#include "ovsdb.h"
#include "ovsdb_cache.h"
#include "ovsdb_table.h"
#include "schema.h"

static struct http_cache cache_mgr =
{
    .initialized = false,
};

struct http_cache *http_get_mgr(void)
{
    return &cache_mgr;
}


#undef strlcat
#undef strlcpy

/* strnlen() is a POSIX.2008 addition. Can't rely on it being available so
 * define it ourselves.
 */
size_t
strnlen(const char *s, size_t maxlen)
{
    const char *p;

    p = memchr(s, '\0', maxlen);
    if (p == NULL) return maxlen;

    return p - s;
}


size_t
strlncat(char *dst, size_t len, const char *src, size_t n)
{
    size_t slen;
    size_t dlen;
    size_t rlen;
    size_t ncpy;

    slen = strnlen(src, n);
    dlen = strnlen(dst, len);

    if (dlen < len)
    {
        rlen = len - dlen;
        ncpy = slen < rlen ? slen : (rlen - 1);
        memcpy(dst + dlen, src, ncpy);
        dst[dlen + ncpy] = '\0';
    }

    assert(len > slen + dlen);
    return slen + dlen;
}


size_t
strlcat(char *dst, const char *src, size_t len)
{
    return strlncat(dst, len, src, (size_t) -1);
}


size_t
strlncpy(char *dst, size_t len, const char *src, size_t n)
{
    size_t slen;
    size_t ncpy;

    slen = strnlen(src, n);

    if (len > 0)
    {
        ncpy = slen < len ? slen : (len - 1);
        memcpy(dst, src, ncpy);
        dst[ncpy] = '\0';
    }

    assert(len > slen);
    return slen;
}

size_t
strlcpy(char *dst, const char *src, size_t len)
{
    return strlncpy(dst, len, src, (size_t) -1);
}


static int
request_url_cb(http_parser *p, const char *buf, size_t len)
{
    struct message *m = p->data;

    strlncpy(m->request_url, sizeof(m->request_url), buf, len);

    return 0;
}


static int
header_field_cb(http_parser *p, const char *buf, size_t len)
{
    struct message *m = p->data;

    if (m->num_headers == MAX_HEADERS)
    {
        LOGD("%s: max headers %d reached", __func__, MAX_HEADERS);
        return 1;
    }

    if (m->last_header_element != FIELD) m->num_headers++;

    strlncpy(m->headers[m->num_headers-1][0],
             sizeof(m->headers[m->num_headers-1][0]),
             buf, len);

    m->last_header_element = FIELD;

    return 0;
}


static int
header_value_cb (http_parser *p, const char *buf, size_t len)
{
    struct message *m = p->data;

    strlncpy(m->headers[m->num_headers-1][1],
             sizeof(m->headers[m->num_headers-1][1]),
             buf, len);
    m->last_header_element = VALUE;

    return 0;
}

static http_parser_settings parser_callbacks =
{
    .on_message_begin = 0,
    .on_header_field = header_field_cb,
    .on_header_value = header_value_cb,
    .on_url = request_url_cb,
    .on_status = 0,
    .on_body = 0,
    .on_headers_complete = 0,
    .on_message_complete = 0,
    .on_chunk_header = 0,
    .on_chunk_complete = 0,
};


/**
 * @brief compare sessions
 *
 * @param a session pointer
 * @param b session pointer
 * @return 0 if sessions matches
 */
static int
http_session_cmp(void *a, void *b)
{
    uintptr_t p_a = (uintptr_t)a;
    uintptr_t p_b = (uintptr_t)b;

    if (p_a ==  p_b) return 0;
    if (p_a < p_b) return -1;
    return 1;
}


/**
 * @brief compare devices macs
 *
 * @param a mac address
 * @param b mac address
 * @return 0 if mac addresses match, an integer otherwise
 */
static int
http_dev_id_cmp(void *a, void *b)
{
    os_macaddr_t *dev_id_a = a;
    os_macaddr_t *dev_id_b = b;

    return memcmp(dev_id_a->addr, dev_id_b->addr, sizeof(dev_id_a->addr));
}

/**
 * @brief compare two user agents
 *
 * @param a user agent
 * @param b user agent
 *
 */
static int
http_ua_cmp(void *a, void *b) {
    char *ua_a = a;
    char *ua_b = b;

    return(strncmp(ua_a, ua_b, MAX_ELEMENT_SIZE));
}


void parser_init(struct fsm_http_parser *parser)
{
    struct http_parser *http_parser = &parser->parser;

    http_parser_init(http_parser, HTTP_REQUEST);
    http_parser->data = &parser->message;
}



size_t http_parse_content(struct fsm_http_parser *parser)
{
    struct http_parser *http_parser = &parser->parser;
    size_t parsed;

    parser_init(parser);
    parsed = http_parser_execute(http_parser, &parser_callbacks,
                                 (char *)parser->data, parser->http_len);

    return parsed;
}


/**
 * @brief parses a http message
 *
 * @param parser the parsed data container
 * @return the size of the parsed message, or 0 on parsing error.
 */
size_t
http_parse_message(struct fsm_http_parser *parser)
{
    struct net_header_parser *net_parser;
    size_t len;
    int ip_protocol;

    if (parser == NULL) return 0;

    net_parser = parser->net_parser;

    /* Some basic validation */
    ip_protocol = net_parser->ip_protocol;
    if (ip_protocol != IPPROTO_TCP) return 0;

    /* Parse the http content */
    parser->parsed = net_parser->parsed;
    parser->data = net_parser->data;
    parser->http_len = net_parser->packet_len - net_parser->parsed;
    len = http_parse_content(parser);

    return len;
}


void
http_process_message(struct http_session *h_session)
{
    struct fsm_http_parser *parser;
    char user_agent[] = "User-Agent";
    char *header_user_agent = NULL;
    struct message *m;
    int i, cmp = 0;

    parser = &h_session->parser;
    m = &parser->message;

    /* Fetch user agent */
    for (i = 0; i < m->num_headers; i++)
    {
        cmp = strcmp(m->headers[i][0], user_agent);
        if (cmp == 0)
        {
            header_user_agent = m->headers[i][1];
            break;
        }
    }

    /* Report user agent */
    process_report(h_session, header_user_agent);

    m->num_headers = 0;
}


static void
http_handler(struct fsm_session *session,
             struct net_header_parser *net_parser)
{
    struct http_session *h_session;
    struct fsm_http_parser *parser;
    size_t len;

    h_session = (struct http_session *)session->handler_ctxt;
    parser = &h_session->parser;
    parser->net_parser = net_parser;

    len = http_parse_message(parser);
    if (len == 0) return;

    http_process_message(h_session);

    return;
}


/**
 * @brief looks up a device in the device cache
 *
 * @param h_session the http session
 * @return the device context if found, NULL otherwise
 */
struct http_device *
http_lookup_device(struct http_session *h_session)
{
    struct fsm_http_parser *parser;
    struct net_header_parser *net_parser;
    struct eth_header *eth;
    struct http_device *hdev;
    ds_tree_t *tree;

    if (h_session == NULL) return NULL;

    parser = &h_session->parser;
    net_parser = parser->net_parser;
    eth = &net_parser->eth_header;
    tree = &h_session->session_devices;
    hdev = ds_tree_find(tree, eth->srcmac);

    return hdev;
}


/**
 * @brief looks up or allocate a device for the http session's device cache
 *
 * @param h_session the http session
 * @return the device context if found or allocated, NULL otherwise
 */
struct http_device *
http_get_device(struct http_session *h_session)
{
    struct http_device *hdev;
    struct fsm_http_parser *parser;
    struct net_header_parser *net_parser;
    struct eth_header *eth;
    ds_tree_t *tree;

    if (h_session == NULL) return NULL;

    hdev = http_lookup_device(h_session);
    if (hdev != NULL) return hdev;

    /* No match, allocate a new entry */
    parser = &h_session->parser;
    net_parser = parser->net_parser;
    eth = &net_parser->eth_header;
    hdev = calloc(1, sizeof(*hdev));
    if (hdev == NULL) return NULL;

    memcpy(&hdev->device_mac, eth->srcmac, sizeof(os_macaddr_t));
    ds_tree_init(&hdev->reports, http_ua_cmp,
                 struct http_parse_report, report_node);

    tree = &h_session->session_devices;
    ds_tree_insert(tree, hdev, &hdev->device_mac);

    return hdev;
}


/**
 * @brief looks up a user agent report in the report cache of a device
 *
 * @param hdev the device context
 * @user_agent the user agent to look up
 * @return the report context if found, NULL otherwise
 */
struct http_parse_report *
http_lookup_report(struct http_device *hdev,
                   char *user_agent)
{
    ds_tree_t *tree;
    struct http_parse_report *report;

    tree = &hdev->reports;
    report = ds_tree_find(tree, user_agent);
    return report;
}


/**
 * @brief looks up or allocate a user agent for the report cache of a device
 *
 * @param hdev the device context
 * @user_agent the user agent to look up
 * @return the report context if found or allocated, NULL otherwise
 */
struct http_parse_report *
http_get_report(struct http_device *hdev,
                char *user_agent)
{
    ds_tree_t *tree;
    struct http_parse_report *report;

    report = http_lookup_report(hdev, user_agent);
    if (report != NULL) return report;

    tree = &hdev->reports;
    report = calloc(1, sizeof(struct http_parse_report));
    if (report == NULL) return NULL;

    memcpy(report->user_agent, user_agent, MAX_ELEMENT_SIZE);
    memcpy(&report->src_mac, &hdev->device_mac, sizeof(os_macaddr_t));
    ds_tree_insert(tree, report, report->user_agent);
    hdev->cached_entries++;

    return report;
}


static void
http_lru_remove_report(struct http_device *hdev)
{
    struct http_parse_report *report = ds_tree_head(&hdev->reports);
    struct http_parse_report *candidate = report;

    if (ds_tree_is_empty(&hdev->reports) == true) return;

    while (report != NULL)
    {
        double cmp;

        cmp = difftime(report->timestamp, candidate->timestamp);
        if (cmp < 0)  candidate = report;

        report = ds_tree_next(&hdev->reports, report);
    }
    ds_tree_remove(&hdev->reports, candidate);
    free(candidate);
    hdev->cached_entries--;
}


/**
 * @brief Sends a mqtt report for the current device and user agent.
 *
 * @param h_session the http session
 * @param user_agent the collected user agent
 */
void
process_report(struct http_session *h_session, char *user_agent)
{
    struct fsm_session *session;
    struct http_device *hdev;
    struct http_parse_report *report;
    char *msg;

    if (h_session == NULL) return;
    if (user_agent == NULL) return;

    hdev = http_get_device(h_session);
    if (hdev == NULL) return;

    report = http_get_report(hdev, user_agent);
    if (report == NULL) return;

    session = h_session->session;
    report->counter++;
    report->timestamp = time(NULL);
    if (report->counter == 1)
    {
        msg = jencode_user_agent(session, report);
        session->ops.send_report(session, msg);
    }

    if (hdev->cached_entries >= MAX_CACHE_UAS) http_lru_remove_report(hdev);
}


void http_periodic(struct fsm_session *session)
{
    struct http_cache *mgr = http_get_mgr();

    if (!mgr->initialized) return;
}


/**
 * @brief looks up a session
 *
 * Looks up a session, and allocates it if not found.
 * @param session the session to lookup
 * @return the found/allocated session, or NULL if the allocation failed
 */
struct http_session *
http_lookup_session(struct fsm_session *session)
{
    struct http_cache *mgr;
    struct http_session *h_session;
    ds_tree_t *sessions;

    mgr = http_get_mgr();
    sessions = &mgr->fsm_sessions;

    h_session = ds_tree_find(sessions, session);
    if (h_session != NULL) return h_session;

    LOGD("%s: Adding new session %s", __func__, session->name);
    h_session = calloc(1, sizeof(struct http_session));
    if (h_session == NULL) return NULL;

    ds_tree_insert(sessions, h_session, session);

    return h_session;
}

/**
 * @brief frees a http device
 *
 * @param hdev the http device to delete
 */
void
http_free_device(struct http_device *hdev)
{
    struct http_parse_report *hreport, *remove;
    ds_tree_t *tree;

    tree = &hdev->reports;
    hreport = ds_tree_head(tree);
    while (hreport != NULL)
    {
        remove = hreport;
        hreport = ds_tree_next(tree, hreport);
        ds_tree_remove(tree, remove);
        free(remove);
    }

    free(hdev);
}



/**
 * @brief Frees a http session
 *
 * @param h_session the http session to delete
 */
void
http_free_session(struct http_session *h_session)
{
    struct http_device *hdev, *remove;
    ds_tree_t *tree;

    tree = &h_session->session_devices;
    hdev = ds_tree_head(tree);
    while (hdev != NULL)
    {
        remove = hdev;
        hdev = ds_tree_next(tree, hdev);
        ds_tree_remove(tree, remove);
        http_free_device(remove);
    }

    free(h_session);
}


/**
 * @brief deletes a session
 *
 * @param session the fsm session keying the http session to delete
 */
void
http_delete_session(struct fsm_session *session)
{
    struct http_cache *mgr;
    struct http_session *h_session;
    ds_tree_t *sessions;

    mgr = http_get_mgr();
    sessions = &mgr->fsm_sessions;

    h_session = ds_tree_find(sessions, session);
    if (h_session == NULL) return;

    LOGD("%s: removing session %s", __func__, session->name);
    ds_tree_remove(sessions, h_session);
    http_free_session(h_session);

    return;
}


/**
 * @brief plugin exit callback
 *
 * @param session the fsm session container
 * @return none
 */
void
http_plugin_exit(struct fsm_session *session)
{
    struct http_cache *mgr;

    mgr = http_get_mgr();
    if (!mgr->initialized) return;

    http_delete_session(session);
}


/**
 * @brief dso initialization entry point
 *
 * @param session pointer provided by fsm
 * @return 0 if successful, -1 otherwise
 *
 * Initializes the plugin specific fields of the session
 */
int
http_plugin_init(struct fsm_session *session)
{
    struct fsm_parser_ops *parser_ops;
    struct http_session *http_session;
    struct http_cache *mgr;

    mgr = http_get_mgr();

    /* Initialize the manager on first call */
    if (!mgr->initialized)
    {
        ds_tree_init(&mgr->fsm_sessions, http_session_cmp,
                     struct http_session, session_node);
        mgr->initialized = true;
    }

    /* Look up the http session */
    http_session = http_lookup_session(session);
    if (http_session == NULL)
    {
        LOGE("%s: could not allocate http parser", __func__);
        return -1;
    }

    /* Bail if the session is already initialized */
    if (http_session->initialized) return 0;

    /* Set the fsm session */
    session->ops.periodic = http_periodic;
    session->ops.exit = http_plugin_exit;
    session->handler_ctxt = http_session;

    /* Set the plugin specific ops */
    parser_ops = &session->p_ops->parser_ops;
    parser_ops->handler = http_handler;

    /* Wrap up the http session initialization */
    http_session->session = session;
    ds_tree_init(&http_session->session_devices, http_dev_id_cmp,
                 struct http_device, device_node);
    http_session->initialized = true;
    LOGD("%s: added session %s", __func__, session->name);

    return 0;
}
