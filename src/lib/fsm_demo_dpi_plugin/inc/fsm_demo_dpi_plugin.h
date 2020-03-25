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

#ifndef __FSM_DEMO_PLUGIN_H__
#define __FSM_DEMO_PLUGIN_H__
#include <jansson.h>
#include <pcap.h>
#include <stdint.h>
#include <time.h>

#include "os_types.h"
#include "fsm.h"
#include "network_metadata.h"
#include "net_header_parse.h"

/**
 * @brief demo parser
 *
 * The parser contains the parsed info for the packet currently processed
 * It embeds:
 * - the network header,
 * - the data length which excludes the network header
 * - the amount of data parsed
 */
struct fsm_demo_parser
{
    struct net_header_parser *net_parser; /* network header parser */
    size_t data_len;                      /* Non-network related data length */
    uint8_t *data;                        /* Non-network data pointer */
    size_t parsed;                        /* Parsed bytes */
};

/**
 * @brief a node in the device tree managed by a session
 *
 * Each session manages a set of devices presented to the session.
 * The set is organized as a DS tree.
 */
struct fsm_demo_device
{
    os_macaddr_t device_mac;
    ds_tree_node_t device_node;
};


/**
 * @brief a session, instance of processing state and routines.
 *
 * The session provides an executing instance of the services'
 * provided by the plugin.
 * It embeds:
 * - a fsm session
 * - state information
 * - a packet parser
 * - a flow stats aggregator
 * - a set of devices presented to the session
 */
struct fsm_demo_session
{
    struct fsm_session *session;
    bool initialized;
    struct fsm_demo_parser parser;
    struct net_md_aggregator *aggr;
    ds_tree_t session_devices;
    ds_tree_node_t session_node;
};

/**
 * @brief the plugin cache, a singleton tracking instances
 *
 * The cache tracks the global initialization of the plugin
 * and the running sessions.
 */
struct fsm_demo_plugin_cache
{
    bool initialized;
    ds_tree_t fsm_sessions;
};


/**
 * @brief session initialization entry point
 *
 * Initializes the plugin specific fields of the session,
 * like the pcap handler and the periodic routines called
 * by fsm.
 * @param session pointer provided by fsm
 */
int
fsm_demo_plugin_init(struct fsm_session *session);


/**
 * @brief session exit point
 *
 * Frees up resources used by the session.
 * @param session pointer provided by fsm
 */
void
fsm_demo_plugin_exit(struct fsm_session *session);


/**
 * @brief session packet processing entry point
 *
 * packet processing handler.
 * @param args the fsm session
 * @param h the pcap capture header
 * @param bytes a pointer to the captured packet
 */
void
fsm_demo_plugin_handler(struct fsm_session *session,
                        struct net_header_parser *net_parser);


/**
 * @brief parses a http message
 *
 * @param parser the parsed data container
 * @return the size of the parsed message, or 0 on parsing error.
 */
size_t
fsm_demo_parse_message(struct fsm_demo_parser *parser);


/**
 * @brief parses the received message content
 *
 * @param parser the parsed data container
 * @return the size of the parsed message content, or 0 on parsing error.
 */
size_t
fsm_demo_parse_content(struct fsm_demo_parser *parser);


/**
 * @brief process the parsed message
 *
 * Place holder for message content processing
 * @param f_session the demo session pointing to the parsed message
 */
void
fsm_demo_process_message(struct fsm_demo_session *f_session);


/**
 * @brief session packet periodic processing entry point
 *
 * Periodically called by the fsm manager
 * @param session the fsm session
 */
void
fsm_demo_plugin_periodic(struct fsm_session *session);


/**
 * @brief looks up a session
 *
 * Looks up a session, and allocates it if not found.
 * @param session the session to lookup
 * @return the found/allocated session, or NULL if the allocation failed
 */
struct fsm_demo_session *
fsm_demo_lookup_session(struct fsm_session *session);


/**
 * @brief looks up a device in the device cache
 *
 * @param f_session the fsm demo session
 * @return the device context if found, NULL otherwise
 */
struct fsm_demo_device *
fsm_demo_lookup_device(struct fsm_demo_session *f_session);


/**
 * @brief looks up or allocate a device for the fsm demo session's device cache
 *
 * @param f_session the fsm demo session
 * @return the device context if found or allocated, NULL otherwise
 */
struct fsm_demo_device *
fsm_demo_get_device(struct fsm_demo_session *f_session);


/**
 * @brief frees a fsm demo device
 *
 * @param hdev the fsm demo device to delete
 */
void
fsm_demo_free_device(struct fsm_demo_device *fdev);


/**
 * @brief Frees a fsm demo session
 *
 * @param f_session the fsm demo session to delete
 */
void
fsm_demo_free_session(struct fsm_demo_session *f_session);


/**
 * @brief deletes a session
 *
 * @param session the fsm session keying the http session to delete
 */
void
fsm_demo_delete_session(struct fsm_session *session);


/**
 * @brief encode a demo report in json format
 *
 * Returns a pointer to a string string json encoded information.
 * The caller needs to free the pointer through a json_free() call.
 * @params session fsm session storing the header information
 * @params to_report http user agent information to report
 */
char *
demo_jencode_demo_event(struct fsm_session *session);


/**
 * @brief checks that a session has all the info to report through mqtt
 *
 * Validates that mqtt topics and location/node ids have been set
 * @param session fsm session storing the header information
 */
bool
demo_jcheck_header_info(struct fsm_session *session);

/**
 * jencode_header: encode the header section of a message
 * @json_report: json object getting filled
 * @session: fsm session storing the header information
 *
 * Fills up the header section of a json formatted mqtt report
 */
void
demo_jencode_header(struct fsm_session *session, json_t *json_report);
/**
 * @brief returns the plugin's session manager
 *
 * @return the plugin's session manager
 */
struct fsm_demo_plugin_cache *
fsm_demo_get_mgr(void);


#endif /* __FSM_DEMO_PLUGIN_H__ */
