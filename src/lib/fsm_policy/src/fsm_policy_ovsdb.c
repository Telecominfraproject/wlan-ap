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

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

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

ovsdb_table_t table_FSM_Policy;

static char redirect_prefix[RD_SZ][4] = {
    "A-", "4A-", "C-"
};

char * check_redirect(char *redirect, int id) {
    char *cmp = redirect_prefix[id];

    LOGT("%s: redirect: %s", __func__, redirect);
    if (strncmp(redirect, cmp, strlen(cmp)) == 0) {
        return redirect + strlen(cmp);
    }
    return NULL;
}


/**
 * fsm_redirects_fqdn_to_ip: resolve fqdn redirects
 * @policy: provided policy
 *
 */
void fsm_redirects_fqdn_to_ip(struct fsm_policy *p, int family)
{
#if 0
    struct str_set *set;
    struct addrinfo hints = { 0 }, *infoptr;
    char *fqdn = NULL;
    char host[256];
    char *cptr;
    int clen;
    char *pfx;
    int rc, idx, clen;
    bool loop;

    set = p->redirects;
    if (set == NULL) return;

    if (family == AF_INET) pfx = redirect_prefix[IPv4_REDIRECT];
    else if (family == AF_INET6) pfx = redirect_prefix[IPv6_REDIRECT];
    else return;

    idx = 0;
    do {
        fqdn = check_redirect(set->array[idx], FQDN_REDIRECT);
        loop = (fqdn == NULL);
        if (loop) idx++;
        loop = (idx < set->nelems);
    } while (loop);

    if (fqdn == NULL) return;

    hints.ai_family = family;
    rc = getaddrinfo(fqdn, NULL, &hints, &infoptr);
    if (rc != 0) return;

    if (infoptr == NULL) return;

    clen = strlen(pfx);
    strncpy(host, pfx, clen);
    cptr = host + clen;
    getnameinfo(infoptr->ai_addr, infoptr->ai_addrlen, cptr,
                sizeof(host) - clen, NULL, 0, NI_NUMERICHOST);
    LOGD("%s: host: %s", __func__, host);
    idx = (family == AF_INET) ? 0 : 1;

    freeaddrinfo(infoptr);
#endif
}

static int policy_id_cmp(void *id1, void *id2)
{
    int i = *(int *)id1;
    int j = *(int *)id2;

    return i - j;
}



/**
 * cat_cmp: compare 2 integers
 * @c1: first category value
 * @c2: second category value
 *
 * This function is passed to qsort() and bsearch()
 */
int fsm_cat_cmp(const void *c1, const void *c2)
{
    int i = *(int *)c1;
    int j = *(int *)c2;

    return i - j;
}


/**
 * fsm_policy_sort_cats: sorts a policy's set of categories in increase order
 * @p: the policy
 *
 */
void fsm_policy_sort_cats(struct fsm_policy *p)
{
    struct int_set *categories_set;
    void *base;
    size_t nmemb;
    size_t size;

    categories_set = p->rules.categories;
    if (categories_set == NULL) return;

    base = (void *)(categories_set->array);
    size = sizeof(categories_set->array[0]);
    nmemb = categories_set->nelems;
    qsort(base, nmemb, size, fsm_cat_cmp);
}


void fsm_prepare_policy(struct fsm_policy *p)
{
    fsm_policy_sort_cats(p);
    fsm_redirects_fqdn_to_ip(p, AF_INET);
    fsm_redirects_fqdn_to_ip(p, AF_INET6);
}


void fsm_policy_get_prev(struct policy_table *table,
                         struct fsm_policy *fpolicy)
{
    size_t idx;
    bool looking;

    fpolicy->lookup_prev = -1;
    idx = fpolicy->idx - 1;
    do {
        looking = ((idx != 0) && (table->lookup_array[idx] != NULL));
        if (looking) idx--;
    } while (looking);

    fpolicy->lookup_prev = idx;
}


void fsm_policy_get_next(struct policy_table *table,
                         struct fsm_policy *fpolicy)
{
    size_t idx, max;
    bool looking;

    max = FSM_MAX_POLICIES;
    fpolicy->lookup_next = max;
    idx = fpolicy->idx + 1;
    looking = ((idx >= max) && (table->lookup_array[idx] != NULL));
    do {
        looking = ((idx < max) && (table->lookup_array[idx] != NULL));
        if (looking) idx++;
    } while (looking);
    fpolicy->lookup_prev = idx;
}

/**
 * @brief reset a rule and free its memory resources
 *
 * Reset the rules fields, freeing memory used to instantiate
 * its various checks
 * @param rules the rule instance to reset
 */
void
fsm_free_rules(struct fsm_policy_rules *rules)
{
    /* Reset mac checks */
    rules->mac_rule_present = false;
    rules->mac_op = -1;
    free_str_set(rules->macs);

    /* Reset fqdn check */
    rules->fqdn_rule_present = false;
    rules->fqdn_op = -1;
    free_str_set(rules->fqdns);

    /* Reset web categorization check */
    rules->cat_rule_present = false;
    rules->cat_op = -1;
    free_int_set(rules->categories);

    /* Reset risk level check */
    rules->risk_rule_present = false;
    rules->risk_op = -1;
    rules->risk_level = -1;

    /* Reset ipddr check */
    rules->ip_rule_present = false;
    rules->ip_op = -1;
    free_str_set(rules->ipaddrs);
}


bool fsm_check_conversion(void *converted, int len)
{
    if (len == 0) return true;
    if (converted == NULL) return false;

    return true;
}


bool fsm_set_mac_rules(struct fsm_policy_rules *rules,
                       struct schema_FSM_Policy *spolicy)
{
    int cmp;
    bool check;

    rules->mac_rule_present = spolicy->mac_op_exists;
    if (!rules->mac_rule_present) return true;

    rules->mac_op = -1;
    cmp = strcmp(spolicy->mac_op, "in");
    if (!cmp) rules->mac_op = MAC_OP_IN;

    cmp = strcmp(spolicy->mac_op, "out");
    if (!cmp) rules->mac_op = MAC_OP_OUT;

    if (rules->mac_op == -1) return false;

    rules->macs = schema2str_set(sizeof(spolicy->macs[0]),
                                 spolicy->macs_len,
                                 spolicy->macs);
    check = fsm_check_conversion(rules->macs, spolicy->macs_len);
    return check;
}


bool fsm_set_fqdn_rules(struct fsm_policy_rules *rules,
                       struct schema_FSM_Policy *spolicy)
{
    int cmp;
    bool check;

    rules->fqdn_rule_present = spolicy->fqdn_op_exists;
    if (!rules->fqdn_rule_present) return true;

    rules->fqdn_op = -1;
    cmp = strcmp(spolicy->fqdn_op, "in");
    if (!cmp) rules->fqdn_op = FQDN_OP_IN;

    cmp = strcmp(spolicy->fqdn_op, "out");
    if (!cmp) rules->fqdn_op = FQDN_OP_OUT;

    cmp = strcmp(spolicy->fqdn_op, "sfr_in");
    if (!cmp) rules->fqdn_op = FQDN_OP_SFR_IN;

    cmp = strcmp(spolicy->fqdn_op, "sfr_out");
    if (!cmp) rules->fqdn_op = FQDN_OP_SFR_OUT;

    cmp = strcmp(spolicy->fqdn_op, "sfl_in");
    if (!cmp) rules->fqdn_op = FQDN_OP_SFL_IN;

    cmp = strcmp(spolicy->fqdn_op, "sfl_out");
    if (!cmp) rules->fqdn_op = FQDN_OP_SFL_OUT;

    if (rules->fqdn_op == -1) return false;

    rules->fqdns = schema2str_set(sizeof(spolicy->fqdns[0]),
                                  spolicy->fqdns_len,
                                  spolicy->fqdns);
    check = fsm_check_conversion(rules->fqdns, spolicy->fqdns_len);
    return check;
}

bool fsm_set_cats_rules(struct fsm_policy_rules *rules,
                        struct schema_FSM_Policy *spolicy)
{
    int cmp;
    bool check;

    rules->cat_rule_present = spolicy->fqdncat_op_exists;
    if (!rules->cat_rule_present) return true;

    rules->cat_op = -1;
    cmp = strcmp(spolicy->fqdncat_op, "in");
    if (!cmp) rules->cat_op = CAT_OP_IN;

    cmp = strcmp(spolicy->fqdncat_op, "out");
    if (!cmp) rules->cat_op = CAT_OP_OUT;

    if (rules->cat_op == -1) return false;

    rules->categories = schema2int_set(spolicy->fqdncats_len,
                                       spolicy->fqdncats);
    check = fsm_check_conversion(rules->categories, spolicy->fqdncats_len);
    return check;
}


/**
 * @brief instantiates the risk level check of a policy rule
 *
 * Translates the ovsdb representation of the risk level operations and
 * attributes in its policy representation
 * @param rules the policy rule representation
 * @param spolicy the ovsdb rule representation
 * @return true if the translation succeeeded, false otherwise.
 */
bool
fsm_set_risk_level_rules(struct fsm_policy_rules *rules,
                         struct schema_FSM_Policy *spolicy)
{
    int cmp;

    rules->risk_level = -1;

    rules->risk_rule_present = spolicy->risk_op_exists;
    if (!rules->risk_rule_present) return true;

    rules->risk_op = -1;
    cmp = strcmp(spolicy->risk_op, "eq");
    if (!cmp) rules->risk_op = RISK_OP_EQ;

    cmp = strcmp(spolicy->risk_op, "neq");
    if (!cmp) rules->risk_op = RISK_OP_NEQ;

    cmp = strcmp(spolicy->risk_op, "gt");
    if (!cmp) rules->risk_op = RISK_OP_GT;

    cmp = strcmp(spolicy->risk_op, "lt");
    if (!cmp) rules->risk_op = RISK_OP_LT;

    cmp = strcmp(spolicy->risk_op, "gte");
    if (!cmp) rules->risk_op = RISK_OP_GTE;

    cmp = strcmp(spolicy->risk_op, "lte");
    if (!cmp) rules->risk_op = RISK_OP_LTE;

    if (rules->risk_op == -1) return false;

    rules->risk_level = spolicy->risk_level;
    return true;
}


/**
 * @brief instantiates the ipaddr check of a policy rule
 *
 * Translates the ovsdb representation of the ip address operations and
 * attributes in its policy representation
 * @param rules the policy rule representation
 * @param spolicy the ovsdb rule representation
 * @return true if the translation succeeeded, false otherwise.
 */
bool
fsm_set_ip_rules(struct fsm_policy_rules *rules,
                 struct schema_FSM_Policy *spolicy)
{
    int cmp;
    bool check;

    rules->ip_rule_present = spolicy->ipaddr_op_exists;
    if (!rules->ip_rule_present) return true;

    rules->ip_op = -1;
    cmp = strcmp(spolicy->ipaddr_op, "in");
    if (!cmp) rules->ip_op = IP_OP_IN;

    cmp = strcmp(spolicy->ipaddr_op, "out");
    if (!cmp) rules->ip_op = IP_OP_OUT;

    if (rules->ip_op == -1) return false;

    rules->ipaddrs = schema2str_set(sizeof(spolicy->ipaddrs[0]),
                                    spolicy->ipaddrs_len,
                                    spolicy->ipaddrs);
    check = fsm_check_conversion(rules->ipaddrs, spolicy->ipaddrs_len);
    return check;
}


/**
 * @brief instantiates a policy rule
 *
 * Translates the ovsdb representation of the policy rule in its
 * fsm representation
 * @param rules the policy rule representation
 * @param spolicy the ovsdb rule representation
 * @return true if the translation succeeeded, false otherwise.
 */
bool
fsm_set_rules(struct fsm_policy_rules *rules,
              struct schema_FSM_Policy *spolicy)
{
    bool ret;

    ret = fsm_set_mac_rules(rules, spolicy);
    if (!ret) goto err_free_rules;

    ret = fsm_set_fqdn_rules(rules, spolicy);
    if (!ret) goto err_free_rules;

    ret = fsm_set_cats_rules(rules, spolicy);
    if (!ret) goto err_free_rules;

    ret = fsm_set_risk_level_rules(rules, spolicy);
    if (!ret) goto err_free_rules;

    ret = fsm_set_ip_rules(rules, spolicy);
    if (!ret) goto err_free_rules;

    return true;

err_free_rules:
    fsm_free_rules(rules);

    return false;
}


void fsm_policy_set_next(struct fsm_policy * fpolicy)
{
    struct fsm_policy_session *mgr;
    struct policy_table *table;
    ds_tree_t *tree;
    struct str_ipair *pair;

    fpolicy->jump_table = false;
    tree = fpolicy->next;
    if (tree == NULL) return;

    pair = ds_tree_head(tree);
    fpolicy->next_table = pair->key;
    fpolicy->next_table_index = pair->value;
    fpolicy->jump_table = true;

    mgr = get_mgr();
    tree = &mgr->policy_tables;
    table = ds_tree_find(tree, pair->key);
    if (!table)
    {
        table = calloc(1, sizeof(*table));
        if (table == NULL) return;

        strncpy(table->name, pair->key, sizeof(table->name));
        ds_tree_init(&table->policies, policy_id_cmp,
                     struct fsm_policy, policy_node);
        ds_tree_insert(tree, table, table->name);
    }
}


void fsm_policy_set_action(struct fsm_policy *fpolicy,
                           struct schema_FSM_Policy *spolicy)
{
    int cmp;

    fpolicy->action = FSM_ACTION_NONE;
    if (!spolicy->action_exists) return;

    cmp = strcmp(spolicy->action, "allow");
    if (cmp == 0)
    {
        fpolicy->action = FSM_ALLOW;
        return;
    }

    cmp = strcmp(spolicy->action, "drop");
    if (cmp == 0)
    {
        fpolicy->action = FSM_BLOCK;
        return;
    }
}


void fsm_policy_set_log(struct fsm_policy *fpolicy,
                        struct schema_FSM_Policy *spolicy)
{
    int cmp;

    if (!spolicy->log_exists)
    {
        fpolicy->report_type = FSM_REPORT_NONE;
        return;
    }

    cmp = strcmp(spolicy->log, "none");
    if (cmp == 0)
    {
        fpolicy->report_type = FSM_REPORT_NONE;
        return;
    }

    cmp = strcmp(spolicy->log, "all");
    if (cmp == 0)
    {
        fpolicy->report_type = FSM_REPORT_ALL;
        return;
    }

    cmp = strcmp(spolicy->log, "blocked");
    if (cmp == 0)
    {
        fpolicy->report_type = FSM_REPORT_BLOCKED;
        return;
    }
}


struct fsm_policy *fsm_policy_insert_schema_p(struct policy_table *table,
                                              struct schema_FSM_Policy *spolicy)
{
    struct fsm_policy *fpolicy;
    struct fsm_policy_rules *rules;
    size_t idx;
    bool check;

    fpolicy = calloc(1, sizeof(*fpolicy));
    if (fpolicy == NULL) return NULL;

    fpolicy->table_name = table->name;
    idx = (size_t)spolicy->idx;
    fpolicy->idx = idx;
    fpolicy->rule_name = strdup(spolicy->name);
    if (fpolicy->rule_name == NULL) goto err_free_fpolicy;

    fpolicy->redirects = schema2str_set(sizeof(spolicy->redirect[0]),
                                        spolicy->redirect_len,
                                        spolicy->redirect);
    check = fsm_check_conversion(fpolicy->redirects, spolicy->redirect_len);
    if (!check) goto err_free_rule_name;

    fpolicy->next = schema2itree(sizeof(spolicy->next[0]), spolicy->next_len,
                                 spolicy->next_keys, spolicy->next);
    check = fsm_check_conversion(fpolicy->next, spolicy->next_len);
    if (!check) goto err_free_redirects;
    fsm_policy_set_next(fpolicy);

    fpolicy->other_config = schema2tree(sizeof(spolicy->other_config_keys[0]),
                                        sizeof(spolicy->other_config[0]),
                                        spolicy->other_config_len,
                                        spolicy->other_config_keys,
                                        spolicy->other_config);
    check = fsm_check_conversion(fpolicy->other_config,
                                 spolicy->other_config_len);
    if (!check) goto err_free_next;

    fsm_policy_set_action(fpolicy, spolicy);
    fsm_policy_set_log(fpolicy, spolicy);

    rules = &fpolicy->rules;
    check = fsm_set_rules(rules, spolicy);
    if (!check) goto err_free_other_config;

    fpolicy->table = table;
    table->lookup_array[idx] = fpolicy;

    /* Set lookup indeces */
    fsm_policy_get_prev(table, fpolicy);
    fsm_policy_get_next(table, fpolicy);

    /* Insert policy in the table's policy tree */
    ds_tree_insert(&table->policies, fpolicy, &fpolicy->idx);

    LOGN("%s: loaded policy %s into table %s", __func__,
        fpolicy->rule_name, fpolicy->table_name);

    return fpolicy;

err_free_other_config:
    free_str_tree(fpolicy->other_config);

err_free_next:
    free_str_tree(fpolicy->next);

err_free_redirects:
    free_str_set(fpolicy->redirects);

err_free_rule_name:
    free(fpolicy->rule_name);

err_free_fpolicy:
    free(fpolicy);

    LOGE("%s: Failed to load policy %s into table %s",
        __func__, spolicy->name, table->name);

    return NULL;
}


struct fsm_policy * fsm_policy_lookup(struct schema_FSM_Policy *spolicy)
{
    struct fsm_policy_session *mgr;
    struct policy_table *table;
    struct fsm_policy *fpolicy;
    ds_tree_t *tree;
    char *name;
    int idx;

    mgr = get_mgr();
    name = spolicy->policy;
    tree = &mgr->policy_tables;
    table = ds_tree_find(tree, name);
    if (!table) return NULL;

    idx = spolicy->idx;
    fpolicy = table->lookup_array[idx];

    return fpolicy;
}


struct fsm_policy * fsm_policy_get(struct schema_FSM_Policy *spolicy)
{
    struct fsm_policy_session *mgr;
    struct policy_table *table;
    struct fsm_policy **fpolicy;
    ds_tree_t *tree;
    char *name;

    int idx;

    mgr = get_mgr();
    name = spolicy->policy;
    tree = &mgr->policy_tables;
    table = ds_tree_find(tree, name);
    if (!table)
    {
        table = calloc(1, sizeof(*table));
        if (table == NULL) return NULL;

        strncpy(table->name, name, sizeof(table->name));
        ds_tree_init(&table->policies, policy_id_cmp,
                     struct fsm_policy, policy_node);
        ds_tree_insert(tree, table, table->name);

        LOGN("%s: Loading policy table %s", __func__, name);

        /* Update policy clients */
        fsm_policy_update_clients(table);

    }

    idx = spolicy->idx;
    fpolicy = &table->lookup_array[idx];
    if (*fpolicy != NULL) return *fpolicy;

    *fpolicy = fsm_policy_insert_schema_p(table, spolicy);
    return *fpolicy;
}


/**
 * fsm_add_policy: add a FSM Policy
 * @policy: the policy to add
 */
void fsm_add_policy(struct schema_FSM_Policy *spolicy)
{
    struct fsm_policy *fpolicy;
    char *name = "default";
    bool no_name;
    int idx = spolicy->idx;

    if ((idx < 0) || (idx >= FSM_MAX_POLICIES))
    {
        LOGE("Invalid policy index %d", spolicy->idx);
        return;
    }

    no_name = !spolicy->policy_exists;
    if (no_name) strncpy(spolicy->policy, name, sizeof(spolicy->policy));

    fpolicy = fsm_policy_get(spolicy);
    if (fpolicy == NULL)
    {
        LOGE("%s: addition of policy %s:%s failed", __func__,
             spolicy->policy, spolicy->name);
        return;
    }

    fsm_prepare_policy(fpolicy);
}


void fsm_free_policy(struct fsm_policy *fpolicy)
{
    struct policy_table *table;
    struct fsm_policy_rules *rules;
    size_t idx;

    if (fpolicy == NULL) return;

    /* Free allocated resources */
    free(fpolicy->rule_name);
    free_str_set(fpolicy->redirects);
    free_str_itree(fpolicy->next);
    free_str_tree(fpolicy->other_config);

    rules = &fpolicy->rules;
    fsm_free_rules(rules);

    table = fpolicy->table;
    ds_tree_remove(&table->policies, fpolicy);
    idx = fpolicy->idx;
    table->lookup_array[idx] = NULL;
    free(fpolicy);
}


/**
 * fsm_delete_policy: add a FSM Policy
 * @policy: the policy to add
 */
void fsm_delete_policy(struct schema_FSM_Policy *spolicy)
{
    struct fsm_policy *fpolicy = NULL;
    char *name = "default";
    bool no_name;

    no_name = !spolicy->policy_exists;
    if (no_name) strncpy(spolicy->policy, name, sizeof(spolicy->policy));
    fpolicy = fsm_policy_lookup(spolicy);
    if (fpolicy ==  NULL) return;

    fsm_free_policy(fpolicy);
}


/**
 * fsm_remove_policy: add a FSM Policy
 * @policy: the policy to add
 */
void fsm_update_policy(struct schema_FSM_Policy *spolicy)
{
    int idx = spolicy->idx;

    LOGD("Updating policy index %d", idx);
    if ((idx < 0) || (idx >= FSM_MAX_POLICIES)) {
        LOGE("Invalid policy index %d", spolicy->idx);
        return;
    }

    /* Delete existing policy */
    fsm_delete_policy(spolicy);

    /* Add provided policy */
    fsm_add_policy(spolicy);
}


void callback_FSM_Policy(ovsdb_update_monitor_t *mon,
                         struct schema_FSM_Policy *old_rec,
                         struct schema_FSM_Policy *spolicy)
{
    if (mon->mon_type == OVSDB_UPDATE_NEW) {
        fsm_add_policy(spolicy);
    }

    if (mon->mon_type == OVSDB_UPDATE_DEL) {
        fsm_delete_policy(old_rec);
    }

    if (mon->mon_type == OVSDB_UPDATE_MODIFY) {
        fsm_update_policy(spolicy);
    }
}


int table_name_cmp(void *a, void *b)
{
    char *name_a = (char *)a;
    char *name_b = (char *)b;

    return strncmp(name_a, name_b, POLICY_NAME_SIZE);
}


void fsm_init_manager(void)
{
    struct fsm_policy_session *mgr;

    mgr = get_mgr();
    LOGD("%s: initializing", __func__);

    ds_tree_init(&mgr->policy_tables, table_name_cmp,
                 struct policy_table, table_node);

    fsm_policy_client_init();
    mgr->initialized = true;
}


void fsm_policy_init(void)
{
    struct fsm_policy_session *mgr = get_mgr();

    if (mgr->initialized)
    {
        LOGT("%s: already initialized", __func__);
        return;
    }

    fsm_init_manager();

    OVSDB_TABLE_INIT_NO_KEY(FSM_Policy);
    OVSDB_TABLE_MONITOR(FSM_Policy, false);
}
