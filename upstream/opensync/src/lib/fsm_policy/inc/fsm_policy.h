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

#ifndef FSM_POLICY_H_INCLUDED
#define FSM_POLICY_H_INCLUDED

#include <ev.h>
#include <time.h>

#include "ds_tree.h"
#include "ds_list.h"
#include "ovsdb_utils.h"
#include "os_types.h"
#include "schema.h"

enum {
    FSM_ACTION_NONE = 0,
    FSM_BLOCK,
    FSM_ALLOW,
    FSM_OBSERVED,
    FSM_NO_MATCH,
    FSM_REDIRECT,
    FSM_FORWARD,
    FSM_NUM_ACTIONS, /* always last */
};

/*
 * Report value order matters when multiple policies apply.
 * The highest will win.
 */
enum {
    FSM_REPORT_NONE = 0,
    FSM_REPORT_BLOCKED,
    FSM_REPORT_ALL,
};

enum {
    FSM_FQDN_CAT_NOP = 0,
    FSM_FQDN_CAT_FAILED,
    FSM_FQDN_CAT_PENDING,
    FSM_FQDN_CAT_SUCCESS,
};

enum {
    FSM_FQDN_OP_XM = 0, /* exact match */
    FSM_FQDN_OP_SFR,    /* start from right */
    FSM_FQDN_OP_SFL,    /* start from left */
};

enum {
    IPv4_REDIRECT = 0,
    IPv6_REDIRECT = 1,
    FQDN_REDIRECT = 2,
    RD_SZ = 3,
};


enum {
    MAC_OP_OUT = 0,
    MAC_OP_IN,
};

enum {
    FQDN_OP_IN = 0,
    FQDN_OP_SFR_IN,
    FQDN_OP_SFL_IN,
    FQDN_OP_OUT,
    FQDN_OP_SFR_OUT,
    FQDN_OP_SFL_OUT,
};

enum {
    CAT_OP_OUT = 0,
    CAT_OP_IN,
};

enum {
    RISK_OP_EQ = 0,
    RISK_OP_NEQ,
    RISK_OP_GT,
    RISK_OP_LT,
    RISK_OP_GTE,
    RISK_OP_LTE,
};

enum {
    IP_OP_OUT = 0,
    IP_OP_IN,
};

struct dns_device
{
    os_macaddr_t device_mac;
    ds_tree_t fqdn_pending_reqs;
    ds_tree_node_t device_node;
};

enum {
    URL_BC_SVC,
    URL_WP_SVC,
};

#define URL_REPORT_MAX_ELEMS 8

struct fsm_bc_info
{
    uint8_t confidence_levels[URL_REPORT_MAX_ELEMS];
    uint8_t reputation;
};

struct fsm_wp_info
{
    uint8_t risk_level;
};

struct fsm_url_reply
{
    int service_id;
    int lookup_status;
    bool connection_error;
    int error;
    size_t nelems;
    uint8_t categories[URL_REPORT_MAX_ELEMS];
    union
    {
        struct fsm_bc_info bc_info;
        struct fsm_wp_info wp_info;
    } reply_info;
#define bc reply_info.bc_info
#define wb reply_info.wp_info
};

struct fsm_url_request
{
    os_macaddr_t dev_id;
    char url[255];
    int req_id;
    struct fsm_url_reply *reply;
};

#define MAX_RESOLVED_ADDRS 32


struct fsm_policy_req;
struct fsm_policy;

struct fqdn_pending_req
{
    os_macaddr_t dev_id;
    uint16_t req_id;                   // DNS message ID
    int dedup;
    int numq;                          // Number of questions in the request
    uint8_t *dns_reply;                // reply messages
    struct fsm_url_request *req_info;  // FQDN questions
    struct fsm_session *fsm_context;
    int categorized;
    int cat_match;
    int risk_level;
    int action;
    bool redirect;
    int rd_ttl;
    char *policy;
    int policy_idx;
    char *rule_name;
    uint8_t *response;
    int response_len;
    struct dns_device *dev_session;
    time_t timestamp;
    bool to_report;
    bool fsm_checked;
    char redirects[2][256];
    void (*send_report)(struct fsm_session *, char *);
    char *report;
    int ipv4_cnt;
    int ipv6_cnt;
    char *ipv4_addrs[MAX_RESOLVED_ADDRS];
    char *ipv6_addrs[MAX_RESOLVED_ADDRS];
    struct policy_table *policy_table;
    char *provider;
    bool (*categories_check)(struct fsm_session *session,
                             struct fsm_policy_req *req,
                             struct fsm_policy *policy);
    bool (*risk_level_check)(struct fsm_session *session,
                             struct fsm_policy_req *req,
                             struct fsm_policy *policy);
    ds_tree_node_t req_node;           // DS tree node
};

struct fsm_policy_reply
{
    char *mac_tag_match; /* Tag including the device's mac if any */
    char *fqdn_match;    /* fqdn match if any */
    int cat_match;       /* category match */
    int action;          /* action to take */
    bool redirect;       /* Redirect dns reply */
    int rd_ttl;          /* redirected response's ttl */
    int categorized;     /* categorization status */
    int log;             /* log policy */
    char *policy;        /* the last matching policy */
    int policy_idx;      /* the policy index */
    char *rule_name;     /* the last matching rule name with the policy */
};


struct fsm_policy_req
{
    os_macaddr_t *device_id;
    char *url;
    struct fqdn_pending_req *fqdn_req;
    struct fsm_policy_reply reply;
};


#define FSM_MAX_POLICIES 60

/**
 * @brief representation of a policy rule.
 *
 * A Rule a sequence of checks of an object against a set of provisioned values:
 * Each check of a rule follows this pattern:
 * - Is the check enforced?
 * - Is the object included in the set of provisioned values?
 * - Is the rule targeting objects in or out of the set of provisioned values?
 * - If the object is included and the policy targets included objects,
 *   move on to the next check
 * - If the object is out of the set and the policy targets excluded objects,
 *   move on to the next check.
 * - Else the rule has failed.
 */
struct fsm_policy_rules
{
    bool mac_rule_present;
    int mac_op;
    struct str_set *macs;
    bool fqdn_rule_present;
    int fqdn_op;
    struct str_set *fqdns;
    bool cat_rule_present;
    int cat_op;
    struct int_set *categories;
    bool risk_rule_present;
    int risk_op;
    int risk_level;
    bool ip_rule_present;
    int ip_op;
    struct str_set *ipaddrs;
};


struct fsm_policy
{
    struct policy_table *table;
    char *table_name;
    size_t idx;
    char *rule_name;
    struct fsm_policy_rules rules;
    struct str_set *redirects;
    ds_tree_t *next;
    ds_tree_t *other_config;
    int action;
    int report_type;
    bool jump_table;
    char *next_table;
    int next_table_index;
    size_t lookup_prev;
    size_t lookup_next;
    ds_tree_node_t policy_node;
};

struct fsm_url_stats {
    int64_t cloud_lookups;           /* Cloud lookup requests */
    int64_t cloud_hits;              /* Cloud lookup processed */
    int64_t cache_lookups;           /* Cache lookup requests */
    int64_t cache_hits;              /* Cache hits */
    int64_t cloud_lookup_failures;   /* service not reached */
    int64_t cache_lookup_failures;   /* Cache lookup failures */
    int64_t categorization_failures; /* Service reports a processing error */
    int64_t uncategorized;           /* Service reached, uncategorized url */
    int64_t cache_entries;           /* number of cached entries */
    int64_t cache_size;              /* size of the cache */
    int64_t min_lookup_latency;      /* minimal lookup latency */
    int64_t max_lookup_latency;      /* maximal lookup latency */
    int64_t avg_lookup_latency;      /* average lookup latency */
};

#define POLICY_NAME_SIZE 32
struct policy_table
{
    char name[POLICY_NAME_SIZE];
    ds_tree_t policies;
    struct fsm_policy *lookup_array[FSM_MAX_POLICIES];
    ds_tree_node_t table_node;
};

struct fsm_policy_client
{
    struct fsm_session *session;
    char *name;
    struct policy_table *table;
    void (*update_client)(struct fsm_session *, struct policy_table *);
    ds_tree_node_t client_node;
};

struct fsm_policy_session
{
    bool initialized;
    ds_tree_t policy_tables;
    ds_tree_t clients;
};

void fsm_init_manager(void);
struct fsm_policy_session * get_mgr(void);
void fsm_walk_policy_macs(struct fsm_policy *p);
void fsm_policy_init(void);

bool fqdn_pre_validation(char *fqdn_string);
struct fsm_policy *fsm_policy_lookup(struct schema_FSM_Policy *policy);
struct fsm_policy *fsm_policy_get(struct schema_FSM_Policy *policy);
int fsm_cat_cmp(const void *c1, const void *c2);
void fsm_add_policy(struct schema_FSM_Policy *spolicy);
void fsm_delete_policy(struct schema_FSM_Policy *spolicy);
void fsm_update_policy(struct schema_FSM_Policy *spolicy);
void fsm_free_policy(struct fsm_policy *fpolicy);
struct policy_table *fsm_policy_select_table(void);
void fsm_apply_policies(struct fsm_session *session,
                        struct fsm_policy_req *req);
bool fsm_fqdncats_in_set(struct fsm_policy_req *req, struct fsm_policy *p);
bool fsm_device_in_set(struct fsm_policy_req *req, struct fsm_policy *p);
void fsm_policy_client_init(void);
void fsm_policy_register_client(struct fsm_policy_client *client);
void fsm_policy_deregister_client(struct fsm_policy_client *client);
void fsm_policy_update_clients(struct policy_table *table);

#endif /* FSM_H_INCLUDED */
