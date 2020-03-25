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

#ifndef _UPNP_PARSE_H_
#define _UPNP_PARSE_H_

#include <stdint.h>
#include <time.h>

#include "ds_tree.h"
#include "fsm.h"
#include "net_header_parse.h"
#include "os_types.h"
#include "upnp_curl.h"

struct upnp_parser
{
    struct net_header_parser *net_parser; /* network header parser */
    size_t upnp_len;
    uint8_t *data;
    size_t parsed;
    char location[FSM_UPNP_URL_MAX_SIZE];
};


struct upnp_session
{
    struct fsm_session *session;
    bool initialized;
    struct upnp_parser parser;
    ds_tree_t session_devices;
    time_t last_scan;
    ds_tree_node_t session_node;
};


struct upnp_cache
{
    bool initialized;
    ds_tree_t fsm_sessions;
};

struct upnp_report
{
    struct upnp_key_val *first;
    int nelems;
    struct upnp_device_url *url;
};


/**
 * @brief session initialization entry point
 *
 * Initializes the plugin specific fields of the session.
 *
 * @param session pointer provided by fsm
 * @return 0 if successful, -1 otherwise
 */
int
upnp_plugin_init(struct fsm_session *session);


/**
 * @brief plugin exit callback
 *
 * @param session the fsm session container
 * @return none
 */
void
upnp_plugin_exit(struct fsm_session *session);


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
             struct net_header_parser *net_parser);


/**
 * @brief looks up a session
 *
 * Looks up a session, and allocates it if not found.
 *
 * @param session the session to lookup
 * @return the found/allocated session, or NULL if the allocation failed
 */
struct upnp_session *
upnp_lookup_session(struct fsm_session *session);


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
upnp_parse_message(struct upnp_parser *parser);


/**
 * @brief process the parsed message
 *
 * Triggers an exchange with the advertizing device
 *
 * @param u_session the demo session pointing to the parsed message
 * @return none
 */
void
upnp_process_message(struct upnp_session *u_session);


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
upnp_get_url(struct upnp_session *u_session);


/**
 * @brief session periodic processing entry point
 *
 * Periodically called by the fsm manager
 *
 * @param session the fsm session
 * @return none
 */
void
upnp_periodic(struct fsm_session *session);

struct upnp_cache *upnp_get_mgr(void);
#endif /* _UPNP_PARSE_H_ */
