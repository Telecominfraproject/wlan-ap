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

#ifndef _HTTP_PARSE_H_
#define _HTTP_PARSE_H_

#include <stdint.h>
#include <time.h>

#include "os_types.h"
#include "http_parser.h"
#include "fsm.h"
#include "ds_tree.h"
#include "net_header_parse.h"

#define MAX_UA_SIZE 256

#define MAX_HEADERS 13
#define MAX_ELEMENT_SIZE 2048
#define MAX_CHUNKS 16

#define MIN(a,b) ((a) < (b) ? (a) : (b))

struct message
{
    const char *name;
    const char *raw;
    enum http_parser_type type;
    enum http_method method;
    int status_code;
    char response_status[MAX_ELEMENT_SIZE];
    char request_path[MAX_ELEMENT_SIZE];
    char request_url[MAX_ELEMENT_SIZE];
    char fragment[MAX_ELEMENT_SIZE];
    char query_string[MAX_ELEMENT_SIZE];
    char body[MAX_ELEMENT_SIZE];
    size_t body_size;
    const char *host;
    const char *userinfo;
    uint16_t port;
    int num_headers;
    enum { NONE=0, FIELD, VALUE } last_header_element;
    char headers [MAX_HEADERS][2][MAX_ELEMENT_SIZE];
    int should_keep_alive;

    int num_chunks;
    int num_chunks_complete;
    int chunk_lengths[MAX_CHUNKS];

    const char *upgrade; // upgraded body

    unsigned short http_major;
    unsigned short http_minor;

    int message_begin_cb_called;
    int headers_complete_cb_called;
    int message_complete_cb_called;
    int status_cb_called;
    int message_complete_on_eof;
    int body_is_final;
};


struct fsm_http_parser
{
    struct net_header_parser *net_parser; /* network header parser */
    struct http_parser parser;
    struct message message;
    size_t http_len;
    uint8_t *data;
    size_t parsed;
};


struct http_parse_report
{
    char user_agent[MAX_ELEMENT_SIZE];
    os_macaddr_t src_mac;
    time_t timestamp;
    int counter;
    ds_tree_node_t report_node;
};


#define MAX_CACHE_UAS 8

struct http_device
{
    os_macaddr_t device_mac;
    ds_tree_t reports;
    int cached_entries;
    ds_tree_node_t device_node;
};


struct http_session
{
    struct fsm_session *session;
    bool initialized;
    struct fsm_http_parser parser;
    ds_tree_t session_devices;
    ds_tree_node_t session_node;
};


struct http_cache
{
    bool initialized;
    ds_tree_t fsm_sessions;
};


int
http_plugin_init(struct fsm_session *session);

void
http_plugin_exit(struct fsm_session *session);

void
http_periodic(struct fsm_session *session);

void
process_report(struct http_session *h_session, char *user_agent);

size_t
http_parse_content(struct fsm_http_parser *parser);

size_t
http_parse_message(struct fsm_http_parser *parser);

void
http_process_message(struct http_session *h_session);

struct http_parse_report *
http_lookup_report(struct http_device *hdev,
                   char *user_agent);

struct http_parse_report *
http_get_report(struct http_device *hdev,
                char *user_agent);

struct http_device *
http_lookup_device(struct http_session *http_session);

struct http_device *
http_get_device(struct http_session *http_session);

void
parser_init(struct fsm_http_parser *parser);

struct http_cache *
http_get_mgr(void);

struct http_session *
http_lookup_session(struct fsm_session *session);

#endif /* _HTTP_PARSE_H_ */
