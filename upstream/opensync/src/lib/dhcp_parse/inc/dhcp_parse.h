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

#ifndef __DHCP_PARSE_H__
#define __DHCP_PARSE_H__

#include <stdint.h>
#include <time.h>

#include "schema.h"
#include "inet.h"
#include "fsm.h"
#include "ds_tree.h"
#include "net_header_parse.h"
#include "os.h"
#include "os_types.h"


/******************************************************************************
* Struct Declarations
*******************************************************************************/
#define DHCP_MSG_DISCOVER   1
#define DHCP_MSG_OFFER      2
#define DHCP_MSG_REQUEST    3
#define DHCP_MSG_DECLINE    4
#define DHCP_MSG_ACK        5
#define DHCP_MSG_NACK       6
#define DHCP_MSG_RELEASE    7
#define DHCP_MSG_INFORM     8

#define DHCP_MAGIC          0x63825363

#ifndef MAC_STR_LEN
#define MAC_STR_LEN         18
#endif  /* MAC_STR_LEN */

#define MAX_LABEL_LEN_PER_DN 63
#define MAX_DN_LEN 253

struct dhcp_hdr
{
    uint8_t     dhcp_op;
    uint8_t     dhcp_htype;
    uint8_t     dhcp_hlen;
    uint8_t     dhcp_hops;
    uint32_t    dhcp_xid;
    uint16_t    dhcp_secs;
    uint16_t    dhcp_flags;
    os_ipaddr_t dhcp_ciaddr;
    os_ipaddr_t dhcp_yiaddr;
    os_ipaddr_t dhcp_siaddr;
    os_ipaddr_t dhcp_giaddr;
    union
    {
        os_macaddr_t    dhcp_chaddr;
        uint8_t         dhcp_padaddr[16];
    };
    uint8_t     dhcp_server[64];
    uint8_t     dhcp_boot_file[128];
    uint32_t    dhcp_magic;
    uint8_t     dhcp_options[0];
};

/*
 * Single DHCP leaese entry
 */
struct dhcp_lease
{
    int                             dhcp_msg_type;    // last dhcp message type seen
    struct schema_DHCP_leased_IP    dlip;

    ds_tree_node_t                  dhcp_node;        // tree node structure
};

struct dhcp_parser
{
    struct  net_header_parser *net_parser;
    size_t  dhcp_len;
    size_t  parsed;
    size_t  caplen;
    uint8_t *data;
};

struct dhcp_session
{
    struct fsm_session      *session;
    struct dhcp_parser      parser;
    ds_tree_t               dhcp_leases;
    ds_tree_node_t          session_node;
    bool                    initialized;
    ds_tree_t               dhcp_local_domains;
    size_t                  num_local_domains;
};

struct dhcp_parse_mgr
{
    bool                    initialized;
    ds_tree_t               fsm_sessions;
};

struct dhcp_local_domain
{
    char                    name[MAX_DN_LEN];
    ds_tree_node_t          local_domain_node;
};

struct dhcp_report
{
    ds_tree_t              *domain_list;
};

/******************************************************************************
* Function Declarations
******************************************************************************/
int         dhcp_plugin_init(struct fsm_session *session);
void        dhcp_plugin_exit(struct fsm_session *session);

void        dhcp_plugin_handler(struct fsm_session *session,
                                struct net_header_parser *net_parser);

struct      dhcp_session *dhcp_lookup_session(struct fsm_session *session);

struct      dhcp_parse_mgr *dhcp_get_mgr(void);
int         dhcp_lease_cmp(void *a, void *b);
int         dhcp_local_domain_cmp(void *a, void *b);

size_t      dhcp_parse_message(struct dhcp_parser *parser);
size_t      dhcp_parse_content(struct dhcp_parser *parser);

void        dhcp_process_message(struct dhcp_session *d_session);

bool        dp_fingerprint_to_str(uint8_t *fingerprint, char *s, size_t sz);
bool        dhcp_lease_update_table(struct dhcp_session *d_session,
                                    struct schema_DHCP_leased_IP *dlip);

#endif      /* __DHCP_PARSE_H__ */
