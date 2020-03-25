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
#include "assert.h"
#include "ovsdb.h"
#include "ovsdb_sync.h"
#include "schema.h"
#include "target.h"
#include "json_util.h"
#include "json_mqtt.h"

#include "dhcp_parse.h"

#define OVSDB_DHCP_TABLE    "DHCP_leased_IP"
static bool                 g_dhcp_table_init = false;

static struct dhcp_parse_mgr dhcp_mgr =
{
    .initialized = false,
};

struct dhcp_parse_mgr *dhcp_get_mgr(void)
{
    return &dhcp_mgr;
}

/**
 * @brief compare sessions
 *
 * @param a session pointer
 * @param b session pointer
 * @return 0 if sessions matches
 */
static int dhcp_session_cmp(void *a, void *b)
{
    uintptr_t p_a = (uintptr_t)a;
    uintptr_t p_b = (uintptr_t)b;

    if (p_a ==  p_b) return 0;
    if (p_a < p_b) return -1;
    return 1;
}


/**
 * @brief  parsing the dhcp option of the format below:
 *----------------------------------------------------------+

 119 9 3 'e' 'n' 'g' 5 'a' 'p' 'p' 'l' 'e' 2  'c' 'o' 'm' 0
 ----------------+------------------------------------------
 --------------+---+---------------------------------------------

 9 'm' 'a' 'r' 'k' 'e' 't' 'i' 'n' 'g' 5 'a' 'p' 'p' 'l' 'e' 2 'c'
 'o' 'm' 0
 ---------------+------------------------------------------------
 *
 * @param dhcp_session pointer.
 * @param popt the raw buffer from the packet.
 * @param optlen the length of the option..
 * @return  if success.
 */

static int dhcp_local_domain_option_processing(struct dhcp_session *d_session,
                                               uint8_t *popt, int optlen)
{
    char                        fqdn[MAX_DN_LEN] = {0};
    int                         npos = 0;
    char                        *pfqdn = fqdn;
    struct dhcp_local_domain    *domain;
    uint8_t                     label_len;

    pfqdn = fqdn;

   while (npos < optlen)
   {
       label_len = (int)popt[npos];
       if (label_len > MAX_LABEL_LEN_PER_DN)
           LOGW("%s: Received label length [%d] and max label length [%d]",
                __func__, label_len, MAX_LABEL_LEN_PER_DN);
       npos += 1;
       // Copying the label.
       while (label_len != 0)
       {
           *pfqdn++ = popt[npos];
           npos++;
           label_len--;
      }

      if (popt[npos] != 0) // Indicates there are more labels in this fqdn.
      {
          *pfqdn++ = '.'; // Append a '.' to the label.
          continue;
      }
      npos++; // Indicates end of a single fqdn in the list.
      LOGT("%s: local domain name[%s]", __func__, fqdn);
      // Lookup the current domain list using the parsed fqdn,
      // and use it to lookup the list structure
      domain = ds_tree_find(&d_session->dhcp_local_domains, &fqdn);
      if (domain == NULL)
      {
          // Allocate for new domain
          domain = calloc(1, sizeof(struct dhcp_local_domain));
          if (domain == NULL)
          {
              LOGE("%s: Unable to allocate memory for DHCP local domain"
                                                  " structure", __func__);
              return false;
          }

          memcpy(&domain->name, &fqdn, sizeof(fqdn));

          ds_tree_insert(&d_session->dhcp_local_domains, domain, &domain->name);
          d_session->num_local_domains++;
     }
     pfqdn = fqdn; // reset the pointer for next fqdn in the list.
     memset(fqdn, 0, MAX_DN_LEN);
  }

  return true;
}

int dhcp_lease_cmp(void *a, void *b)
{
    return memcmp(a, b, sizeof(os_macaddr_t));
}

/**
 * @brief compare domain names
 *
 * @param a domain name  pointer
 * @param b domain name pointer
 * @return 0 if domain names match.
 */
int dhcp_local_domain_cmp(void *a, void *b)
{
    return memcmp(a, b, MAX_DN_LEN);
}

/**
 * @brief send dhcp report.
 *
 * @param d_session dhcp_session pointer.
 */
static void dhcp_send_report(struct dhcp_session *d_session)
{
    struct fsm_session           *session;
    struct dhcp_report           to_report;
    char                        *report;
    if (d_session == NULL) return;

    session = d_session->session;

    if (session == NULL) return;

    to_report.domain_list = &d_session->dhcp_local_domains;
    report = jencode_dhcp_report(session, &to_report);
    session->ops.send_report(session, report);
    return;
}

size_t dhcp_parse_content(struct dhcp_parser *parser)
{
    struct   net_header_parser  *net_parser;
    uint16_t                    ethertype;
    int                         ip_protocol;
    struct                      udphdr *hdr;

    if (parser == NULL) return 0;

    net_parser = parser->net_parser;

    // Check ethertype
    ethertype = net_header_get_ethertype(net_parser);
    if (ethertype != ETH_P_IP) return 0;

    // Check for IP protocol
    if (net_parser->ip_version != 4) return 0;

    // Check for UDP protocol
    ip_protocol = net_parser->ip_protocol;
    if (ip_protocol != IPPROTO_UDP) return 0;

    // Check the UDP src and dst ports
    hdr = net_parser->ip_pld.udphdr;
    if ((ntohs(hdr->source) != 68 || ntohs(hdr->dest) != 67) &&
        (ntohs(hdr->source) != 67 || ntohs(hdr->dest) != 68))
    {
        LOGE("dhcp_parse_content: UDP src/dst port range mismatch,"
             " source port:%d, dest port:%d",
             ntohs(hdr->source), ntohs(hdr->dest));
        return 0;
    }

    return parser->dhcp_len;
}

/**
 * @brief parses a dhcp message
 *
 * @param parser the parsed data container
 * @return the size of the parsed message, or 0 on parsing error.
 */
size_t dhcp_parse_message(struct dhcp_parser *parser)
{
    struct net_header_parser    *net_parser;
    size_t                      len;
    int                         ip_protocol;

    if (parser == NULL) return 0;

    net_parser = parser->net_parser;

    /* Some basic validation */
    ip_protocol = net_parser->ip_protocol;
    if (ip_protocol != IPPROTO_UDP) return 0;

    /* Parse the dhcp content */
    parser->parsed = net_parser->parsed;
    parser->data = net_parser->data;
    parser->dhcp_len = net_parser->packet_len - net_parser->parsed;
    len = dhcp_parse_content(parser);
    if (len == 0) return 0;

    return len;
}

void dhcp_process_message(struct dhcp_session *d_session)
{
    struct  net_header_parser   *net_parser;
    struct  dhcp_parser         *parser;
    struct  dhcp_lease          *lease;
    struct  dhcp_hdr            *dhcp;

    uint8_t                     *popt     = NULL;
    uint8_t                     msg_type = 0;
    uint8_t                     fin[256];
    char                        mac_str[MAC_STR_LEN];

    if (d_session == NULL) return;

    parser = &d_session->parser;
    net_parser = parser->net_parser;

    dhcp = (void *)(net_parser->data);

    if (ntohl(dhcp->dhcp_magic) != DHCP_MAGIC)
    {
        LOGE("Magic number invalid, dropping packet");
        return;
    }

    LOGD("DHCP processing: DHCP packet htype: %d hlen:%d xid:%08x"
         " ClientAddr:"PRI(os_macaddr_t)" ClientIP:"PRI(os_ipaddr_t)" Magic:%08x",
         dhcp->dhcp_htype, dhcp->dhcp_hlen, ntohl(dhcp->dhcp_xid),
         FMT(os_macaddr_t, dhcp->dhcp_chaddr), FMT(os_ipaddr_t, dhcp->dhcp_yiaddr),
         ntohl(dhcp->dhcp_magic));

    sprintf(mac_str, PRI(os_macaddr_lower_t), FMT(os_macaddr_t, dhcp->dhcp_chaddr));

    // Lookup the current leases list using the client's hardware address,
    // and use it to lookup the leases structure
    lease = ds_tree_find(&d_session->dhcp_leases, &mac_str);
    if (lease == NULL)
    {
        // Allocate for new lease
        lease = calloc(1, sizeof(struct dhcp_lease));
        if (lease == NULL)
        {
            LOGE("%s: Unable to allocate memory for DHCP lease"
                 " structure", __func__);
            return;
        }

        memset(&lease->dlip, 0, sizeof(lease->dlip));

        memcpy(&lease->dlip.hwaddr, &mac_str, sizeof(mac_str));
        ds_tree_insert(&d_session->dhcp_leases, lease, &lease->dlip.hwaddr);
    }

    // Parse DHCP options, update the current lease
    popt = dhcp->dhcp_options;
    fin[0] = 0;

    while (popt < ((parser->data) + (parser->dhcp_len)))
    {
        uint8_t optid, optlen;
        uint8_t *pfin;

        // End option, break out
        if (*popt == 255) break;

        // Pad option, continue
        if (*popt == 0)
        {
            popt++;
            continue;
        }

        if (popt + 2 > ((parser->data) + (parser->dhcp_len)))
        {
            LOGE("%d: DHCP options truncated", __LINE__);
            break;
        }

        optid = *popt++;
        optlen = *popt++;

        if ((popt + optlen) > ((parser->data) + (parser->dhcp_len)))
        {
            LOGE("%d: DHCP options truncated", __LINE__);
            break;
        }

        LOGT("DHCP option id: %hhu", optid);

        switch (optid)
        {
            case DHCP_OPTION_ADDRESS_REQUEST:
            {
                // This is not actually the given IP address
                LOGD("%s: IPv4 Address = "PRI(os_ipaddr_t),
                      lease->dlip.hwaddr,
                      FMT(os_ipaddr_t, *(os_ipaddr_t *)popt));
                break;
            }

            case DHCP_OPTION_LEASE_TIME:
            {
                lease->dlip.lease_time = ntohl(*(uint32_t *)popt);
                LOGD("%s: Lease time = %ds",
                      lease->dlip.hwaddr,
                      (int)lease->dlip.lease_time);
                break;
            }

            case DHCP_OPTION_MSG_TYPE:
            {
                // Message type
                LOGD("%s: Message type = %d",
                      lease->dlip.hwaddr, *(uint8_t *)popt);

                msg_type = *popt;
                lease->dhcp_msg_type = msg_type;
                break;
            }

            case DHCP_OPTION_PARAM_LIST:
            {
                // parameter list - for fingerprinting
                pfin = fin;
                int ii;

                for (ii = 0; ii < optlen; ii++)
                {
                    if (popt[ii] == 0) continue;

                    if (pfin > fin + sizeof(fin) - 1) {
                        // Reached the size limit, ignore rest of options
                        break;
                    }

                    *pfin++ = popt[ii];
                }

                *pfin = 0;

                break;
            }

            case DHCP_OPTION_HOSTNAME:
            {
                // Update lease hostname
                if (optlen == 0)
                {
                    // In theory this should never happen
                    LOGW("%s: Client request empty hostname", lease->dlip.hwaddr);
                    break;
                }

                if (optlen >= sizeof(lease->dlip.hostname) - 1)
                {
                    LOGW("%s: Hostname option too long, discarding packet",
                                                        lease->dlip.hwaddr);
                    return;
                }

                // Client requested Hostname
                memcpy(lease->dlip.hostname, popt, optlen);
                lease->dlip.hostname[optlen] = '\0';
                break;
            }

            case DHCP_OPTION_VENDOR_CLASS:
            {
                if (optlen == 0)
                {
                    LOGW("%s: Client supplied empty vendor class",
                                                    lease->dlip.hwaddr);
                    break;
                }

                // Update the lease vendor-class
                if (optlen >= sizeof(lease->dlip.vendor_class) - 1)
                {
                    LOGW("%s: Vendor class option too long,"
                         " discarding packet", lease->dlip.hwaddr);
                    break;
                }

                memcpy(lease->dlip.vendor_class, popt, optlen);
                lease->dlip.vendor_class[optlen] = '\0';
                break;
            }
            case DHCP_OPTION_DOMAIN_SEARCH:
            {
                bool ret = false;
                if (optlen == 0)
                {
                    LOGW("%s: Server supplied empty  domain search list",
                         lease->dlip.hwaddr);
                }

                ret = dhcp_local_domain_option_processing(d_session, popt, optlen);
                if (ret != true)
                {
                    LOGW("%s: Couldn't parse DHCP option [%d]",
                         __func__, optid);
                }
                dhcp_send_report(d_session);
                break;
            }

            default:
            {
                LOGD("%s: Received DHCP Option: %d(%d)\n",
                                    lease->dlip.hwaddr, optid, optlen);
            }
        }

        popt += optlen;
    }

    // Check our current phase
    switch (msg_type)
    {
        case DHCP_MSG_DISCOVER:
        {
            // The fingerprint is valid only during this phase
            if (!dp_fingerprint_to_str( fin, lease->dlip.fingerprint,
                                        sizeof(lease->dlip.fingerprint)))
            {
                LOGE("%s: Unable to convert fingerprint to string",
                                                    lease->dlip.hwaddr);

                lease->dlip.fingerprint[0] = '\0';
            }

            break;
        }

        case DHCP_MSG_REQUEST:
        {
            if (lease->dlip.fingerprint[0] == '\0')
            {
                /*
                 * Get options from DHCP REQUEST if we didn't get them
                 * at DHCP_DISCOVER. This may happen if we did not catch
                 * the DHCP request, but we did catch the DHCP renewal.
                 * Note: The fingerbank fingerprint should be the one from
                 *       the discover phase.
                 */
                if (!dp_fingerprint_to_str(fin, lease->dlip.fingerprint,
                                           sizeof(lease->dlip.fingerprint)))
                {
                    LOGE("%s: Unable to convert fingerprint to string",
                                                        lease->dlip.hwaddr);

                    lease->dlip.fingerprint[0] = '\0';
                }
            }
            break;
        }

        case DHCP_MSG_OFFER:
        {
            // Save the IP Address
            sprintf(lease->dlip.inet_addr, "%d.%d.%d.%d",
                    dhcp->dhcp_yiaddr.addr[0], dhcp->dhcp_yiaddr.addr[1],
                    dhcp->dhcp_yiaddr.addr[2], dhcp->dhcp_yiaddr.addr[3]);

            LOGT( "DHCP MSG OFFER, Client IP '%s'", lease->dlip.inet_addr);
            break;
        }

        case DHCP_MSG_ACK:
        {
            // Update the IP Address
            sprintf(lease->dlip.inet_addr, "%d.%d.%d.%d",
                    dhcp->dhcp_yiaddr.addr[0], dhcp->dhcp_yiaddr.addr[1],
                    dhcp->dhcp_yiaddr.addr[2], dhcp->dhcp_yiaddr.addr[3]);

            LOGT( "DHCP MSG ACK, Client IP '%s'", lease->dlip.inet_addr);

            // call the ovsdb DHCP_leased_IP update function
            dhcp_lease_update_table(d_session, &lease->dlip);

            break;
        }

        case DHCP_MSG_NACK:
        case DHCP_MSG_RELEASE:
        {
            // call the ovsdb DHCP_leased_IP update function
            dhcp_lease_update_table(d_session, &lease->dlip);

            // Remove from the list
            ds_tree_remove(&d_session->dhcp_leases, lease);
            free(lease);

            break;
        }

        default:
            break;
    }

    return;
}

/*
 * Converts a binary representation of the fingerprint to a comma delimited string
 */
bool dp_fingerprint_to_str(uint8_t *finger, char *s, size_t sz)
{
    uint8_t *pfin   = finger;
    size_t  len     = 0;

    s[0] = '\0';
    for (pfin = finger; *pfin != 0; pfin++)
    {
        int rc;

        if (len == 0) {
            rc = snprintf(s, sz, "%d", *pfin);
        }
        else
        {
            rc = snprintf(s + len, sz - len, ",%d", *pfin);
        }

        if (rc < 0)
        {
            s[len] = '\0';
            return false;
        }

        // Overflow -- handle this properly by not adding partial strings or appending new entries after this
        if (len + rc >= sz)
        {
            s[len] = '\0';
            return false;
        }

        len += rc;
    }

    return true;
}

/*
 * OVSDB Table Sync for DHCP_leased_IP
 */

bool dhcp_lease_update_table(struct dhcp_session *d_session,
                             struct schema_DHCP_leased_IP *dlip)
{
    pjs_errmsg_t    perr;
    json_t          *where, *row, *cond;
    bool            ret;
    char            *update_ovsdb = NULL;
    int             val;
    bool            update = false;
    struct          fsm_session *session = d_session->session;

    // Skip deleting the empty entries at startup
    if (!g_dhcp_table_init && (dlip->lease_time == 0))
    {
        return true;
    }

    if (session->ops.get_config != NULL)
    {
        update_ovsdb = session->ops.get_config(session, "update_ovsdb");
    }

    if (update_ovsdb != NULL)
    {
        LOGT("%s:  update_ovsdb key value: %s",
             __func__, update_ovsdb);
       val = strcmp(update_ovsdb, "true");
       update = (val == 0);
    }

    //Skip updating DHCP_leased_IP table if "update_ovsdb" is set to "off"
    if (update != true)
    {
        LOGT("%s: Skipping updating DHCP_leased_IP table", __func__);
        return false;
    }

    // OVSDB transaction where multi condition
    where = json_array();

    cond = ovsdb_tran_cond_single("hwaddr", OFUNC_EQ, str_tolower(dlip->hwaddr));
    json_array_append_new(where, cond);

    cond = ovsdb_tran_cond_single("inet_addr", OFUNC_EQ, dlip->inet_addr);
    json_array_append_new(where, cond);

    if (dlip->lease_time == 0)
    {
        // Released or expired lease... remove from OVSDB
        ret = ovsdb_sync_delete_where(OVSDB_DHCP_TABLE, where);
        if (!ret)
        {
            LOGE("Updating DHCP lease %s (Failed to remove entry)", dlip->hwaddr);
            return false;
        }

        LOGN("Removed DHCP lease '%s' with '%s' '%s' '%d'",
             dlip->hwaddr, dlip->inet_addr, dlip->hostname, dlip->lease_time);
    }
    else
    {
        // New/active lease, upsert it into OVSDB
        row = schema_DHCP_leased_IP_to_json(dlip, perr);
        ret = ovsdb_sync_upsert_where(OVSDB_DHCP_TABLE, where, row, NULL);
        if (!ret)
        {
            LOGE("Updating DHCP lease %s (Failed to insert entry)", dlip->hwaddr);
            return false;
        }

        LOGN("Updated DHCP lease '%s' with '%s' '%s' '%d'",
             dlip->hwaddr, dlip->inet_addr, dlip->hostname, dlip->lease_time);
    }
    return true;
}

static void dhcp_handler(struct fsm_session *session,
                         struct net_header_parser *net_parser)
{
    struct dhcp_session         *d_session;
    struct dhcp_parser          *parser;
    size_t                      len;

    d_session = (struct dhcp_session *)session->handler_ctxt;

    parser = &d_session->parser;
    parser->caplen = net_parser->caplen;

    parser->net_parser = net_parser;
    len = dhcp_parse_message(parser);
    if (len == 0) return;

    dhcp_process_message(d_session);

    return;
}

void dhcp_periodic(struct fsm_session *session)
{
    struct dhcp_parse_mgr *mgr = dhcp_get_mgr();

    if (!mgr->initialized) return;
}

/**
 * @brief looks up a session
 *
 * Looks up a session, and allocates it if not found.
 * @param session the session to lookup
 * @return the found/allocated session, or NULL if the allocation failed
 */
struct dhcp_session * dhcp_lookup_session(struct fsm_session *session)
{
    struct dhcp_parse_mgr   *mgr;
    struct dhcp_session     *d_session;
    ds_tree_t               *sessions;

    mgr       = dhcp_get_mgr();
    sessions  = &mgr->fsm_sessions;

    d_session = ds_tree_find(sessions, session);
    if (d_session != NULL) return d_session;

    LOGD("%s: Adding new session %s", __func__, session->name);

    d_session = calloc(1, sizeof(struct dhcp_session));
    if (d_session == NULL) return NULL;

    ds_tree_insert(sessions, d_session, session);

    return d_session;
}

/**
 * @brief Frees a dhcp session
 *
 * @param d_session the dhcp session to delete
 */
void dhcp_free_session(struct dhcp_session *d_session)
{
    struct dhcp_lease   *lease, *remove;
    struct dhcp_local_domain *domain, *remove_ld;
    ds_tree_t           *tree;

    tree = &d_session->dhcp_leases;
    lease = ds_tree_head(tree);
    while (lease != NULL)
    {
        remove = lease;
        lease = ds_tree_next(tree, lease);
        ds_tree_remove(tree, remove);
        free(remove);
    }

    tree = &d_session->dhcp_local_domains;
    domain = ds_tree_head(tree);
    while (domain != NULL)
    {
        remove_ld = domain;
        domain = ds_tree_next(tree, domain);
        ds_tree_remove(tree, remove_ld);
        free(remove_ld);
    }
    free(d_session);
}

/**
 * @brief deletes a session
 *
 * @param session the fsm session keying the dhcp session to delete
 */
void dhcp_delete_session(struct fsm_session *session)
{
    struct dhcp_parse_mgr   *mgr;
    struct dhcp_session     *d_session;
    ds_tree_t               *sessions;

    mgr = dhcp_get_mgr();
    sessions = &mgr->fsm_sessions;

    d_session = ds_tree_find(sessions, session);
    if (d_session == NULL) return;

    LOGD("%s: removing session %s", __func__, session->name);
    ds_tree_remove(sessions, d_session);
    dhcp_free_session(d_session);

    return;
}

/**
 * @brief plugin exit callback
 *
 * @param session the fsm session container
 * @return none
 */
void dhcp_plugin_exit(struct fsm_session *session)
{
    struct dhcp_parse_mgr *mgr;

    mgr = dhcp_get_mgr();
    if (!mgr->initialized) return;

    dhcp_delete_session(session);
}

/**
 * dhcp_plugin_init: dso initialization entry point
 * @session: session pointer provided by fsm
 *
 * Initializes the plugin specific fields of the session,
 * like the pcap handler and the periodic routines called
 * by fsm.
 */
int dhcp_plugin_init(struct fsm_session *session)
{
    struct  dhcp_parse_mgr      *mgr;
    struct  dhcp_session        *dhcp_session;
    struct fsm_parser_ops       *parser_ops;

    mgr = dhcp_get_mgr();

    g_dhcp_table_init = true;

    /* Initialize the manager on first call */
    if (!mgr->initialized)
    {
        ds_tree_init(&mgr->fsm_sessions, dhcp_session_cmp,
                     struct dhcp_session, session_node);
        mgr->initialized = true;
    }

    /* Look up the dhcp session */
    dhcp_session = dhcp_lookup_session(session);
    if (dhcp_session == NULL)
    {
        LOGE("%s: could not allocate dhcp parser", __func__);
        return -1;
    }

    /* Bail if the session is already initialized */
    if (dhcp_session->initialized) return 0;

    /* Set the fsm session */
    session->ops.periodic = dhcp_periodic;
    session->ops.exit     = dhcp_plugin_exit;
    session->handler_ctxt = dhcp_session;

    /* Set the plugin specific ops */
    parser_ops            = &session->p_ops->parser_ops;
    parser_ops->handler   = dhcp_handler;

    /* Wrap up the dhcp session initialization */
    dhcp_session->session = session;

    ds_tree_init(&dhcp_session->dhcp_leases, dhcp_lease_cmp,
                 struct dhcp_lease, dhcp_node);
    ds_tree_init(&dhcp_session->dhcp_local_domains, dhcp_local_domain_cmp,
                 struct dhcp_local_domain, local_domain_node);
    dhcp_session->num_local_domains = 0;
    dhcp_session->initialized = true;

    LOGD("%s: added session %s", __func__, session->name);

    return 0;
}
