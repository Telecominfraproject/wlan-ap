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
#include <unistd.h>

#include "const.h"
#include "log.h"
#include "json_mqtt.h"
#include "upnp_parse.h"
#include "assert.h"
#include "json_util.h"
#include "ovsdb.h"
#include "ovsdb_cache.h"
#include "ovsdb_table.h"
#include "schema.h"

static struct upnp_cache cache_mgr =
{
    .initialized = false,
};


struct upnp_cache *upnp_get_mgr(void) {
    return &cache_mgr;
}

/**
 * @brief compare sessions
 *
 * @param a session pointer
 * @param b session pointer
 * @return 0 if sessions matches
 */
static int
upnp_session_cmp(void *a, void *b)
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
upnp_dev_id_cmp(void *a, void *b)
{
    os_macaddr_t *dev_id_a = a;
    os_macaddr_t *dev_id_b = b;

    return memcmp(dev_id_a->addr, dev_id_b->addr, sizeof(dev_id_a->addr));
}


/**
 * @brief compare two urls
 *
 * @param a url
 * @param b url
 *
 */
static int
upnp_url_cmp(void *a, void *b)
{
    char *url_a = a;
    char *url_b = b;

    return strncmp(url_a, url_b, FSM_UPNP_URL_MAX_SIZE);
}


struct eth_header *
upnp_get_eth(struct upnp_session *u_session)
{
    struct upnp_parser *parser;
    struct net_header_parser *net_parser;

    if (u_session == NULL) return NULL;

    parser = &u_session->parser;
    net_parser = parser->net_parser;

    return net_header_get_eth(net_parser);
}


/**
 * @brief looks up a device in the device cache
 *
 * @param u_session the upnp session
 * @return the device context if found, NULL otherwise
 */
struct upnp_device *
upnp_lookup_device(struct upnp_session *u_session)
{
    struct eth_header *eth;
    struct upnp_device *udev;
    ds_tree_t *tree;

    if (u_session == NULL) return NULL;

    eth = upnp_get_eth(u_session);
    if (eth == NULL) return NULL;

    tree = &u_session->session_devices;
    udev = ds_tree_find(tree, eth->srcmac);

    return udev;
}


/**
 * @brief looks up or allocate a device for the upnp session's device cache
 *
 * @param u_session the upnp session
 * @return the device context if found or allocated, NULL otherwise
 */
struct upnp_device *
upnp_get_device(struct upnp_session *u_session)
{
    struct upnp_device *udev;
    struct eth_header *eth;
    ds_tree_t *tree;

    if (u_session == NULL) return NULL;

    udev = upnp_lookup_device(u_session);
    if (udev != NULL) return udev;

    /* No match, allocate a new entry */
    eth = upnp_get_eth(u_session);
    if (eth == NULL) return NULL;

    udev = calloc(1, sizeof(*udev));
    if (udev == NULL) return NULL;

    memcpy(&udev->device_mac, eth->srcmac, sizeof(os_macaddr_t));
    tree = &udev->urls;
    ds_tree_init(tree, upnp_url_cmp, struct upnp_device_url, url_node);

    tree = &u_session->session_devices;
    ds_tree_insert(tree, udev, &udev->device_mac);

    return udev;
}


/**
 * @brief looks up the url context advertized by a device
 *
 * Looks up both device and advertized URL contexts,
 * and allocates them  if not found.
 *
 * @param session the session holding the info to lookup
 * @return the found/allocated URL context, or NULL if any allocation failed
 */
struct upnp_device_url *
upnp_get_url(struct upnp_session *u_session)
{
    struct upnp_parser *parser;
    struct upnp_device *udev;
    struct upnp_device_url *url;
    ds_tree_t *tree;

    if (u_session == NULL) return NULL;

    parser = &u_session->parser;
    udev = upnp_get_device(u_session);
    if (udev == NULL) return NULL;

    tree = &udev->urls;
    url = ds_tree_find(tree, parser->location);
    if (url != NULL) return url;

    url = calloc(sizeof(*url), 1);
    if (url == NULL) return NULL;

    strncpy(url->url, parser->location, sizeof(parser->location));
    url->udev = udev;
    url->session = u_session->session;
    ds_tree_insert(tree, url, url->url);

    return url;
}


/**
 * @brief parses the received message content
 *
 * @param parser the parsed data container
 * @return the size of the parsed message content, or 0 on parsing error.
 */
size_t
upnp_parse_content(struct upnp_parser *parser)
{
    struct net_header_parser *net_parser;
    char *notify, *notify_search = "ssdp:alive";
    char *location, *loc_search = "http://", *sloc_search = "https://";
    char *locp;
    uintptr_t lookup_end, message_end;
    char ip_buf[INET6_ADDRSTRLEN] = { 0 };
    char *ip_loc;
    size_t i;
    bool ret;

    /* Fetch source IP address, will be looked up in the message content */
    net_parser = parser->net_parser;
    ret = net_header_srcip_str(net_parser, ip_buf, sizeof(ip_buf));
    if (!ret) return 0;

    /* search the notify marker in the message content */
    notify = strstr((char *)parser->data, notify_search);
    if (notify == NULL) return 0;

    /* Check if the notify field is within boundaries */
    message_end = (uintptr_t)parser->data + parser->upnp_len;
    lookup_end = (uintptr_t)notify + strlen(notify_search);
    if (lookup_end > message_end) return 0;

    /* Lookup the URL, either http or https */
    location = strstr((char *)parser->data, loc_search);
    if (location == NULL) location = strstr((char *)parser->data, sloc_search);
    if (location == NULL) return 0;

    size_t window = message_end - (uintptr_t)location;
    for (i = 0; i < window && location[i] != '\n'; i++);

    if (i == window) return 0;

    /* Stash retrieved url */
    locp = parser->location;
    strncpy(locp, location, i);

    locp[--i] = '\0';

    /* Check for source/arvertized ip mismatch */
    ip_loc = strstr(locp, ip_buf);
    if (ip_loc == NULL) return 0;

    return parser->upnp_len;
}


/**
 * @brief process the parsed message
 *
 * Triggers an exchange with the advertizing device
 *
 * @param u_session the demo session pointing to the parsed message
 * @return none
 */
void
upnp_process_message(struct upnp_session *u_session)
{
    struct upnp_device_url *url;

    url = upnp_get_url(u_session);

    /* If this URL was seen, bail */
    if (url->state != PLM_UPNP_INIT) return;

    url->state = PLM_UPNP_STARTED;
    new_conn(url);
}


/**
 * @brief parses a upnp notification message
 *
 * Parses the UPnP notification message, and trigger a http GET request
 * to the advertized URL if the URL was not yet seen.
 *
 * @param parser the parsed data container
 * @return the size of the parsed message, or 0 on parsing error
 */
size_t
upnp_parse_message(struct upnp_parser *parser)
{
    struct net_header_parser *net_parser;
    size_t len;
    int ip_protocol;

    if (parser == NULL) return 0;

    net_parser = parser->net_parser;

    /* Some basic validation */
    ip_protocol = net_parser->ip_protocol;
    if (ip_protocol != IPPROTO_UDP) return 0;

    /* Parse the http content */
    parser->parsed = net_parser->parsed;
    parser->data = net_parser->data;
    parser->upnp_len = net_parser->packet_len - net_parser->parsed;
    len = upnp_parse_content(parser);
    if (len == 0) return 0;

    return len;
}


/**
 * @brief session packet processing entry point
 *
 * packet processing handler.
 * @param session the fsm session
 * @param net_parser the container of parsed header and original packet
 * @return none
 */
void
upnp_handler(struct fsm_session *session,
                  struct net_header_parser *net_parser)
{
    struct upnp_session *u_session;
    struct upnp_parser *parser;
    size_t len;

    u_session = (struct upnp_session *)session->handler_ctxt;
    parser = &u_session->parser;
    parser->net_parser = net_parser;
    len = upnp_parse_message(parser);
    if (len == 0) return;

    upnp_process_message(u_session);

    return;
}


static void
probe_upnp(void)
{
    char *request =
        "M-SEARCH * HTTP/1.1\r\nHOST:239.255.255.250:1900\r\n"
        "MAN:\"ssdp:discover\"\r\nST:ssdp:all\r\nMX:1\r\n\r\n";
    struct sockaddr_in mcast;
    int sd;
    int so_broadcast = 1, ret = 0;

    sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sd <= 0)
    {
        LOGE("%s: could not open socket", __func__);
        return;
    }

    ret = setsockopt(sd, SOL_SOCKET, SO_BROADCAST,
                     &so_broadcast, sizeof(so_broadcast));
    if (ret)
    {
        LOGE("%s: Could not set socket to broadcast mode\n", __func__);
        goto fail;
    }

    memset(&mcast, 0, sizeof(mcast));
    mcast.sin_family = AF_INET;
    inet_pton(AF_INET, "239.255.255.250", &mcast.sin_addr);
    mcast.sin_port = htons(1900);

    /* Send the broadcast request */
    ret = sendto(sd, request, strlen(request), 0,
                 (struct sockaddr *)&mcast, sizeof(mcast));
    if (ret < 0)
    {
        LOGE("%s: Could not open send broadcast message\n", __func__);
        goto fail;
    }

fail:
    close(sd);
    return;
}


/**
 * @brief session periodic processing entry point
 *
 * Periodically called by the fsm manager
 *
 * @param session the fsm session
 * @return none
 */
void
upnp_periodic(struct fsm_session *session)
{
    struct upnp_session *u_session;
    struct upnp_cache *mgr;
    time_t now = time(NULL);
    double cmp;

    mgr = upnp_get_mgr();
    if (!mgr->initialized) return;

    u_session = upnp_lookup_session(session);
    if (u_session == NULL) return;

    /* Trigger a scan */
    cmp = difftime(now, u_session->last_scan);
    if (cmp > PROBE_UPNP)
    {
        LOGT("%s: trigger a upnp scan", __func__);
        probe_upnp();
        u_session->last_scan = time(NULL);
    }

    return;
}


/**
 * @brief looks up a session
 *
 * Looks up a session, and allocates it if not found.
 *
 * @param session the session to lookup
 * @return the found/allocated session, or NULL if the allocation failed
 */
struct upnp_session *
upnp_lookup_session(struct fsm_session *session)
{
    struct upnp_cache *mgr;
    struct upnp_session *u_session;
    ds_tree_t *sessions;

    mgr = upnp_get_mgr();
    sessions = &mgr->fsm_sessions;

    u_session = ds_tree_find(sessions, session);
    if (u_session != NULL) return u_session;

    LOGD("%s: Adding new session %s", __func__, session->name);
    u_session = calloc(1, sizeof(struct upnp_session));
    if (u_session == NULL) return NULL;

    ds_tree_insert(sessions, u_session, session);

    return u_session;
}


/**
 * @brief frees a upnp device
 *
 * @param udev the upnp device to delete
 * @return none
 */
void
upnp_free_device(struct upnp_device *udev)
{
    struct upnp_device_url *url, *remove;
    ds_tree_t *tree;

    tree = &udev->urls;
    url = ds_tree_head(tree);
    while (url != NULL)
    {
        remove = url;
        url = ds_tree_next(tree, url);
        ds_tree_remove(tree, remove);
        free(remove);
    }

    free(udev);
}


/**
 * @brief Frees a upnp session
 *
 * @param session the upnp session to delete
 * @return none
 */
void
upnp_free_session(struct upnp_session *u_session)
{
    struct upnp_device *udev, *remove;
    ds_tree_t *tree;

    tree = &u_session->session_devices;
    udev = ds_tree_head(tree);
    while (udev != NULL)
    {
        remove = udev;
        udev = ds_tree_next(tree, udev);
        ds_tree_remove(tree, remove);
        upnp_free_device(remove);
    }

    free(u_session);
}


/**
 * @brief deletes a session
 *
 * @param session the session to delete
 * @return none
 */
void
upnp_delete_session(struct fsm_session *session)
{
    struct upnp_cache *mgr;
    struct upnp_session *u_session;
    ds_tree_t *sessions;

    mgr = upnp_get_mgr();
    sessions = &mgr->fsm_sessions;

    u_session = ds_tree_find(sessions, session);
    if (u_session == NULL) return;

    LOGD("%s: removing session %s", __func__, session->name);
    ds_tree_remove(sessions, u_session);
    upnp_free_session(u_session);

    return;
}


/**
 * @brief plugin exit callback
 *
 * @param session the fsm session container
 * @return none
 */
void
upnp_plugin_exit(struct fsm_session *session)
{
    struct upnp_cache *mgr;

    mgr = upnp_get_mgr();
    if (!mgr->initialized) return;

    upnp_delete_session(session);
    if (!ds_tree_is_empty(&mgr->fsm_sessions)) return;

    upnp_curl_exit();
}


/**
 * @brief session initialization entry point
 *
 * Initializes the plugin specific fields of the session.
 *
 * @param session pointer provided by fsm
 * @return 0 if successful, -1 otherwise
 */
int
upnp_plugin_init(struct fsm_session *session)
{
    struct upnp_cache *mgr;
    struct fsm_parser_ops *parser_ops;
    struct upnp_session *upnp_session;

    mgr = upnp_get_mgr();

    /* Initialize the manager on first call */
    if (!mgr->initialized)
    {
        ds_tree_init(&mgr->fsm_sessions, upnp_session_cmp,
                     struct upnp_session, session_node);
    }

    if (ds_tree_is_empty(&mgr->fsm_sessions)) upnp_curl_init(session->loop);

    /* Look up the upnp session */
    upnp_session = upnp_lookup_session(session);
    if (upnp_session == NULL)
    {
        LOGE("%s: could not allocate upnp parser", __func__);
        if (ds_tree_is_empty(&mgr->fsm_sessions)) upnp_curl_exit();

        return -1;
    }

    /* Bail if the session is already initialized */
    if (upnp_session->initialized) return 0;

    /* Set the fsm session */
    session->ops.periodic = upnp_periodic;
    session->ops.exit = upnp_plugin_exit;
    session->handler_ctxt = upnp_session;

    /* Set the plugin specific ops */
    parser_ops = &session->p_ops->parser_ops;
    parser_ops->handler = upnp_handler;

    /* Wrap up the upnp session initialization */
    upnp_session->session = session;
    ds_tree_init(&upnp_session->session_devices, upnp_dev_id_cmp,
                 struct upnp_device, device_node);

    mgr->initialized = true;

    return 0;
}
