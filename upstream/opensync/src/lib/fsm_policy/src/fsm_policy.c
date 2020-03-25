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
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <limits.h>

#include "os.h"
#include "util.h"
#include "ovsdb.h"
#include "ovsdb_update.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"
#include "schema.h"
#include "log.h"
#include "ds.h"
#include "json_util.h"
#include "target.h"
#include "target_common.h"
#include "fsm.h"
#include "policy_tags.h"
#include "fsm_policy.h"


static char tag_marker[2] = "${";
static char gtag_marker[2] = "$[";


static struct fsm_policy_session policy_mgr =
{
    .initialized = false,
};


struct fsm_policy_session * get_mgr(void)
{
    return &policy_mgr;
}


/**
 * @brief walk through fsm policy macs values
 *
 * Used for debug purposes
 * @param p policy
 */
void fsm_walk_policy_macs(struct fsm_policy *p)
{
    struct str_set *macs_set;
    char mac_type[32] = { 0 };
    size_t i, nelems, mac_len;

    if (p == NULL) return;

    macs_set = p->rules.macs;
    nelems = macs_set->nelems;
    mac_len = sizeof(mac_type);

    for (i = 0; i < nelems; i++)
    {
        char *s;
        om_tag_t *tag;
        bool is_tag, is_gtag, tag_has_marker;

        s = macs_set->array[i];
        is_tag = !strncmp(s, tag_marker, sizeof(tag_marker));
        is_gtag = !strncmp(s, gtag_marker, sizeof(gtag_marker));

        if (is_tag) snprintf(mac_type, mac_len, "type: tag");
        else if (is_gtag) snprintf(mac_type, mac_len, "type: group tag");
        else snprintf(mac_type, mac_len, "type: mac address");

        LOGT("mac %zu: %s, %s", i, s, mac_type);
        if (is_tag || is_gtag)
        {
            om_tag_list_entry_t *e;
            char tag_name[256] = { 0 };
            char *tag_s = s + 2; /* pass tag marker */

            /* pass tag values marker */
            tag_has_marker = (*tag_s == TEMPLATE_DEVICE_CHAR);
            tag_has_marker |= (*tag_s == TEMPLATE_CLOUD_CHAR);
            if (tag_has_marker) tag_s += 1;

            /* Copy tag name, remove end marker */
            strncpy(tag_name, tag_s, strlen(tag_s) - 1);
            tag = om_tag_find_by_name(tag_name, is_gtag);
            if (tag == NULL) continue;

            LOGT("tag %s values", tag_name);
            e = ds_tree_head(&tag->values);
            while (e != NULL)
            {
                LOGT("%s", e->value);
                e = ds_tree_next(&tag->values, e);
            }
        }
    }
}

/**
 * @brief looks up a mac address in a tag's value set

 * Looks up a tag by its name, then looks up the mac in the tag values set.
 * @param mac_s string representation of a MAC
 * @param schema_tag tag name as read in ovsdb schema
 * @param is_gtag boolean indicating a tag group if true
 * @return true if found, false otherwise.
 */
static bool fsm_find_device_in_tag(char *mac_s, char *schema_tag, bool is_gtag)
{
    om_tag_t *tag = NULL;
    om_tag_list_entry_t *e = NULL;
    char tag_name[256] = { 0 };
    char *tag_s = schema_tag + 2; // pass tag marker
    bool tag_has_marker;

    /* pass tag values marker */
    tag_has_marker = (*tag_s == TEMPLATE_DEVICE_CHAR);
    tag_has_marker |= (*tag_s == TEMPLATE_CLOUD_CHAR);
    if (tag_has_marker) tag_s += 1;

    /* Copy tag name, remove end marker */
    strncpy(tag_name, tag_s, strlen(tag_s) - 1);

    tag = om_tag_find_by_name(tag_name, is_gtag);
    if (tag == NULL) return false;

    e = om_tag_list_entry_find_by_value(&tag->values, mac_s);
    if (e == NULL) return false;

    LOGT("%s: found %s in tag %s", __func__, mac_s, tag_name);
    return true;
}


/**
 * @brief looks up a mac address in a policy's macs set.
 *
 * Looks up a mac in the policy macs value set. An entry in the value set can be
 * the string representation of a MAC address (assumed to be using lower cases),
 * a tag or a tag group.
 * @param req the fqdn check request
 * @param p the policy
 * @return true if found, false otherwise.
 */
bool fsm_device_in_set(struct fsm_policy_req *req, struct fsm_policy *p)
{
    struct str_set *macs_set;
    os_macaddr_t *mac = req->device_id;
    char mac_s[32] = { 0 };
    size_t i, nelems;

    macs_set = p->rules.macs;
    if (macs_set == NULL) return false;

    mac = req->device_id;
    snprintf(mac_s, sizeof(mac_s), PRI_os_macaddr_lower_t,
             FMT_os_macaddr_pt(mac));

    nelems = macs_set->nelems;

    for (i = 0; i < nelems; i++)
    {
        char *set_entry;
        bool is_tag, is_gtag;

        set_entry = macs_set->array[i];
        is_tag = !strncmp(set_entry, tag_marker, sizeof(tag_marker));
        is_gtag = !strncmp(set_entry, gtag_marker, sizeof(gtag_marker));
        if (is_tag || is_gtag)
        {
            bool rc;

            rc = fsm_find_device_in_tag(mac_s, set_entry, is_gtag);
            if (rc == false) continue;

            /* Found device in tag */
            req->reply.mac_tag_match = set_entry;
            return true;
        }
        else
        {
            int rc;

            rc = strncmp(mac_s, set_entry, strlen(mac_s));
            if (rc != 0) continue;

            /* Found device */
            return true;
        }
    }

    return false;
}


/**
 * @brief check if a mac matches the policy's mac rule
 *
 * @param req the request being processed
 * @param policy the policy
 * @return true the the mac checks the policy's mac rule, false otherwise
 */
static bool fsm_mac_check(struct fsm_policy_req *req,
                          struct fsm_policy *policy)
{
    struct fsm_policy_rules *rules;
    bool rc;

    rules = &policy->rules;

    /* No mac rule. Consider the rule successful */
    if (!rules->mac_rule_present) return true;

    rc = fsm_device_in_set(req, policy);

    /*
     * If the device in set and the policy applies to devices out of the set,
     * the device does not match this policy
     */
    if (rc && (rules->mac_op == MAC_OP_OUT)) return false;

    /*
     * If the device is out of set and the policy applies to devices in set,
     * the device does not match this policy
     */
    if (!rc && (rules->mac_op == MAC_OP_IN)) return false;

    return true;
}


/**
 * fsm_fqdn_in_set: looks up a fqdn in a policy's fqdns values set.
 * @req: the policy request
 * @p: policy
 * @op: lookup
 *
 * Checks if the request's fqdn is either an exact match, start from right
 * or start form left superset of an entry in the policy's fqdn set entry.
 */
static bool fsm_fqdn_in_set(struct fsm_policy_req *req, struct fsm_policy *p,
                            int op)
{
    struct str_set *fqdns_set;
    size_t nelems, i;
    int rc;

    fqdns_set = p->rules.fqdns;
    if (fqdns_set == NULL) return false;
    nelems = fqdns_set->nelems;
    for (i = 0; i < nelems; i++)
    {
        char *fqdn_req = req->url;
        char *entry_set = fqdns_set->array[i];
        int entry_set_len = strlen(entry_set);
        int fqdn_req_len = strlen(fqdn_req);

        if (entry_set_len > fqdn_req_len) continue;

        if (op == FSM_FQDN_OP_SFR) fqdn_req += (fqdn_req_len - entry_set_len);

        rc = strncmp(fqdn_req, entry_set, entry_set_len);
        if (rc == 0) return true;
    }
    return false;
}

/**
 * fsm_fqdn_check: check if a fqdn matches the policy's fqdn rule
 * @req: the request being processed
 * @policy: the policy being checked against
 *
 */
static bool fsm_fqdn_check(struct fsm_policy_req *req,
                          struct fsm_policy *policy)
{
    struct fsm_policy_rules *rules;
    bool rc = false;
    bool in_policy, sfr, sfl;
    int op;

    op = FSM_FQDN_OP_XM;

    rules = &policy->rules;
    if (!rules->fqdn_rule_present) return true;

    /* set policy types */
    in_policy = (rules->fqdn_op == FQDN_OP_IN);
    in_policy |= (rules->fqdn_op == FQDN_OP_SFR_IN);
    in_policy |= (rules->fqdn_op == FQDN_OP_SFL_IN);

    sfr = (rules->fqdn_op == FQDN_OP_SFR_IN);
    sfr |= (rules->fqdn_op == FQDN_OP_SFR_OUT);
    if (sfr) op = FSM_FQDN_OP_SFR;

    sfl = (rules->fqdn_op == FQDN_OP_SFL_IN);
    sfl |= (rules->fqdn_op == FQDN_OP_SFL_OUT);
    if (sfl) op = FSM_FQDN_OP_SFL;

    rc = fsm_fqdn_in_set(req, policy, op);

    /* If fqdn in set and policy applies to fqdns out of set, no match */
    if ((rc) && (!in_policy)) return false;

    /* If fqdn out of set and policy applies to fqdns in set, no match */
    if ((!rc) && (in_policy)) return false;

    return true;
}

/**
 * cat_search: search a category within a policy setfilter
 * @val: category value to look for within the policy'sset of categories
 * @p: policy
 *
 * Returns true if the category is found.
 */
static inline bool cat_search(int val, struct fsm_policy *p)
{
    struct int_set *categories_set;
    void *base, *key, *res;
    size_t nmemb;
    size_t size;

    categories_set = p->rules.categories;
    if (categories_set == NULL) return false;

    base = (void *)(categories_set->array);
    size = sizeof(categories_set->array[0]);
    nmemb = categories_set->nelems;
    key = &val;
    res = bsearch(key, base, nmemb, size, fsm_cat_cmp);

    return (res != NULL);
}


/**
 * fsm_find_fqdncats_in_set: looks up a fqdn categories in a policy's
 *                           fqdn categories set.
 * @req: the policy request
 * @p: policy
 *
 * Looks up fqdn categories in the policy fqdn categories value set.
 * Returns true if found, false otherwise.
 */
bool fsm_fqdncats_in_set(struct fsm_policy_req *req, struct fsm_policy *p)
{
    struct fqdn_pending_req *fqdn_req;
    struct fsm_url_request *req_info;
    struct fsm_url_reply *reply;
    size_t i;
    bool rc;

    fqdn_req = req->fqdn_req;
    req_info = fqdn_req->req_info;
    reply = req_info->reply;

    for (i = 0; i < reply->nelems; i++)
    {
        rc = cat_search(reply->categories[i], p);
        if (!rc) continue;

        fqdn_req->cat_match = reply->categories[i];
        return true;
    }
    return false;
}


/**
 * set_action: set the request's action according to the policy
 * @req: the request being processed
 * @p: the matched policy
 */
static void set_action(struct fsm_policy_req *req, struct fsm_policy *p)
{
    if (p->action == FSM_ACTION_NONE)
    {
        req->reply.action = FSM_OBSERVED;
        return;
    }

    req->reply.action = p->action;
}

/**
 * set_reporting: set the request's reporting according to the policy
 * @req: the request being processed
 * @p: the matched policy
 *
 */
void set_reporting(struct fsm_policy_req *req, struct fsm_policy *p)
{
    int reporting;

    reporting = p->report_type;

    // Return the highest policy reporting policy
    req->reply.log = reporting > req->reply.log ? reporting : req->reply.log;
}

/**
 * set_policy_record: set the request's last matching policy record
 * @req: the request being processed
 * @p: the matched policy
 */
void set_policy_record(struct fsm_policy_req *req, struct fsm_policy *p)
{
    req->reply.policy = p->table_name;
    req->reply.policy_idx = p->idx;
    req->reply.rule_name = p->rule_name;
}

/**
 * set_policy_redirects: set the request redirects
 * @req: the request being processed
 * @p: the matched policy
 */
void set_policy_redirects(struct fsm_policy_req *req,
                          struct fsm_policy *p)
{
    ds_tree_t *tree;
    struct str_set *redirects;
    struct str_pair *ttl;
    size_t i, nelems;
    int rd_ttl = -1;

    req->reply.redirect = false;
    req->reply.rd_ttl = -1;
    rd_ttl = -1;

    /* Check if the policy's other_config map was set */
    tree = p->other_config;
    if (tree)
    {
        /* Check if a ttl value was passed to the policy */
        ttl = ds_tree_find(tree, "rd_ttl");
        if (ttl == NULL) return;
        LOGT("%s: key: %s, value: %s", __func__, ttl->key, ttl->value);


        /* Convert ttl string to integer */
        errno = 0;
        rd_ttl = strtol(ttl->value, NULL, 10);
        if (errno != 0) return;
    }

    redirects = p->redirects;
    if (redirects == NULL) return;

    /* Set reply's redirects for both IPv4 and IPv6 */
    nelems = redirects->nelems;
    for (i = 0; i < nelems; i++)
    {
        LOGT("%s: Policy %s: redirect to %s (ttl %d seconds)",
             __func__, p->table_name, redirects->array[i], rd_ttl);
        strncpy(req->fqdn_req->redirects[i], redirects->array[i],
                strlen(redirects->array[i]) + 1);
    }

    req->reply.redirect = true;
    req->reply.rd_ttl = rd_ttl;
}

/**
 * @brief Applies categorization policy
 *
 * @param session the requesting session
 * @param req the request
 */

bool fsm_cat_check(struct fsm_session *session,
                   struct fsm_policy_req *req,
                   struct fsm_policy *policy)
{
    struct fsm_policy_rules *rules;
    bool rc;

    /* If no categorization request, return success */
    rules = &policy->rules;
    if (!rules->cat_rule_present) return true;

    /*
     * The policy requires categorization, no web_cat provider.
     * Return failure.
     */
    if (req->fqdn_req->categories_check == NULL) return false;

    /* Apply the web_cat rules */
    rc = req->fqdn_req->categories_check(session, req, policy);

    return rc;
}


/**
 * @brief Applies risk level policy
 *
 * @param session the requesting session
 * @param req the request
 */

bool fsm_risk_level_check(struct fsm_session *session,
                          struct fsm_policy_req *req,
                          struct fsm_policy *policy)
{
    struct fsm_policy_rules *rules;
    bool rc;

    /* If no risk level request, return success */
    rules = &policy->rules;
    if (!rules->risk_rule_present) return true;

    /*
     * The policy requires risk checking, no web_risk provider.
     * Return failure.
     */
    if (req->fqdn_req->risk_level_check == NULL) return false;

    /* Apply the risk rules */
    rc = req->fqdn_req->risk_level_check(session, req, policy);

    return rc;
}


/**
 * fsm_apply_policies: check a request against stored policies
 * @req: policy checking (mac, fqdn, categories) request
 *
 * Walks through the policies table looking for a match,
 * combines the action and report to apply
 */
void fsm_apply_policies(struct fsm_session *session,
                        struct fsm_policy_req *req)
{
    struct policy_table *table;
    struct fsm_policy *p = NULL;
    int i;
    bool rc, matched = false;

    table = req->fqdn_req->policy_table;
    if (table == NULL)
    {
        req->reply.action = FSM_NO_MATCH;
        req->reply.log = FSM_REPORT_NONE;
        return;
    }

    for (i = 0; i < FSM_MAX_POLICIES; i++)
    {
        p = table->lookup_array[i];
        if (p == NULL) continue;

        /* Check if the device matches the policy's macs rule */
        rc = fsm_mac_check(req, p);
        if (!rc) continue;

        /* MAC rule passed. Check FQDN */
        rc = fsm_fqdn_check(req, p);
        if (!rc) continue;

        /* fqdn rule passed. Check categories */
        rc = fsm_cat_check(session, req, p);
        if (!rc) continue;

        /* categories rules passed. Check risk level */
        rc = fsm_risk_level_check(session, req, p);
        if (!rc) continue;

        LOGT("%s: %s:%s succeeded", __func__, p->table_name, p->rule_name);

        set_reporting(req, p);
        set_action(req, p);
        set_policy_record(req, p);
        set_policy_redirects(req, p);

        /*
         * No action implicitely means going to the next entry.
         * Though record we had a match
         */
        matched = true;
        if (p->action != FSM_ACTION_NONE) return;
    }
    if (!matched)
    {
        // No match. Report accordingly
        req->reply.action = FSM_NO_MATCH;
        req->reply.log = FSM_REPORT_NONE;
    }
    return;
}
