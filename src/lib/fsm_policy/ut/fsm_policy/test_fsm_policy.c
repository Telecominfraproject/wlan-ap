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
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "fsm.h"
#include "log.h"
#include "os.h"
#include "os_types.h"
#include "fsm_policy.h"
#include "target.h"
#include "unity.h"
#include "schema.h"

const char *test_name = "fsm_policy_tests";

struct schema_FSM_Policy spolicies[] =
{
    { /* entry 0 */
        .name = "DefaultTable:Rule0",
        .idx = 0,
        .mac_op_exists = true,
        .mac_op = "in",
        .macs_len = 2,
        .macs =
        {
            "Bonjour",
            "Hello",
        },
        .fqdn_op_exists = false,
        .fqdncat_op_exists = false,
        .redirect_len = 0,
    },
    { /* entry 1 used to update entry 0 - same idx, different mac values */
        .name = "Rule0",
        .idx = 0,
        .mac_op_exists = true,
        .mac_op = "in",
        .macs_len = 3,
        .macs =
        {
            "Bonjour",
            "Hello",
            "Guten Tag"
        },
        .fqdn_op_exists = false,
        .fqdncat_op_exists = false,
        .redirect_len = 0,
    },
    { /* entry 2 */
        .policy_exists = true,
        .policy = "Table1",
        .name = "Table1:Rule0",
        .idx = 0,
        .mac_op_exists = true,
        .mac_op = "in",
        .macs_len = 3,
        .macs =
        {
            "Namaste",
            "Hola",
            "Ni hao"
        },
        .fqdn_op_exists = false,
        .fqdncat_op_exists = false,
        .redirect_len = 0,
        .next_len = 1,
        .next_keys = { "Table2", },
        .next = { 1,  },
    },
    { /* entry 3 */
        .policy_exists = true,
        .policy = "dev_webpulse",
        .name = "Rule0",
        .idx = 10,
        .mac_op_exists = true,
        .mac_op = "in",
        .macs_len = 3,
        .macs =
        {
            "00:00:00:00:00:00",
            "11:22:33:44:55:66",
            "22:33:44:55:66:77"
        },
        .fqdn_op_exists = false,
        .fqdncat_op_exists = true,
        .fqdncat_op = "in",
        .fqdncats_len = 4,
        .fqdncats = { 1, 2, 10, 20 },
        .risk_op_exists = true,
        .risk_op = "gte",
        .risk_level = 6,
        .redirect_len = 2,
        .redirect = { "A-1.2.3.4", "4A-::1" },
        .next_len = 0,
        .action_exists = true,
        .action = "drop",
        .log_exists = true,
        .log = "blocked",
        .other_config_len = 1,
        .other_config_keys = { "rd_ttl", },
        .other_config = { "5" },
    },
    { /* entry 4 */
        .policy_exists = true,
        .policy = "dev_webpulse_ipthreat",
        .name = "RuleIpThreat0",
        .idx = 10,
        .mac_op_exists = false,
        .fqdn_op_exists = false,
        .fqdncat_op_exists = false,
        .risk_op_exists = true,
        .risk_op = "lte",
        .risk_level = 7,
        .ipaddr_op_exists = true,
        .ipaddr_op = "out",
        .ipaddrs_len = 2,
        .ipaddrs =
        {
            "1.2.3.4",
            "::1",
        },
    }
};


void setUp(void)
{
    struct fsm_policy_session *mgr = get_mgr();

    if (!mgr->initialized) fsm_init_manager();
}

void tearDown(void)
{
    struct fsm_policy_session *mgr = get_mgr();
    struct policy_table *table, *t_to_remove;
    struct fsm_policy *fpolicy, *p_to_remove;
    ds_tree_t *tables_tree, *policies_tree;

    tables_tree = &mgr->policy_tables;
    table = ds_tree_head(tables_tree);
    while (table != NULL)
    {
        policies_tree = &table->policies;
        fpolicy = ds_tree_head(policies_tree);
        while (fpolicy != NULL)
        {
            p_to_remove = fpolicy;
            fpolicy = ds_tree_next(policies_tree, fpolicy);
            fsm_free_policy(p_to_remove);
        }
        t_to_remove = table;
        table = ds_tree_next(tables_tree, table);
        ds_tree_remove(tables_tree, t_to_remove);
        free(t_to_remove);
    }
}

void test_add_spolicy0(void)
{
    struct schema_FSM_Policy *spolicy;
    struct fsm_policy *fpolicy;
    struct policy_table *table;
    struct fsm_policy_rules *rules;
    struct str_set *macs_set;
    size_t i;

    /* Add the policy */
    spolicy = &spolicies[0];
    fsm_add_policy(spolicy);
    fpolicy = fsm_policy_lookup(spolicy);

    /* Validate access to the fsm policy */
    TEST_ASSERT_NOT_NULL(fpolicy);

    /* Validate rule name */
    TEST_ASSERT_EQUAL_STRING(spolicy->name, fpolicy->rule_name);

    /* Validate mac content */
    rules = &fpolicy->rules;
    macs_set = rules->macs;
    TEST_ASSERT_NOT_NULL(macs_set);
    TEST_ASSERT_EQUAL_INT(spolicy->macs_len, macs_set->nelems);
    for (i = 0; i < macs_set->nelems; i++)
    {
        TEST_ASSERT_EQUAL_STRING(spolicy->macs[i], macs_set->array[i]);
    }

    /* Validate table name */
    table = fpolicy->table;
    TEST_ASSERT_EQUAL_STRING("default", table->name);

    /* Free the policy */
    fsm_delete_policy(spolicy);

    /* Add the policy and validate all over again */
    spolicy = &spolicies[0];
    fsm_add_policy(spolicy);
    fpolicy = fsm_policy_lookup(spolicy);
    TEST_ASSERT_NOT_NULL(fpolicy);
    TEST_ASSERT_EQUAL_STRING(spolicy->name, fpolicy->rule_name);
    rules = &fpolicy->rules;
    macs_set = rules->macs;
    TEST_ASSERT_NOT_NULL(macs_set);
    TEST_ASSERT_EQUAL_INT(spolicy->macs_len, macs_set->nelems);
    for (i = 0; i < macs_set->nelems; i++)
    {
        TEST_ASSERT_EQUAL_STRING(spolicy->macs[i], macs_set->array[i]);
    }

    /* Check next table settings */
    TEST_ASSERT_FALSE(fpolicy->jump_table);

    /* Free the policy */
    fsm_delete_policy(spolicy);
}


void test_update_spolicy0(void)
{
    struct schema_FSM_Policy *spolicy;
    struct fsm_policy *fpolicy;
    struct fsm_policy_rules *rules;
    struct str_set *macs_set;
    size_t i;

    /* Add the policy */
    spolicy = &spolicies[0];
    fsm_add_policy(spolicy);
    fpolicy = fsm_policy_lookup(spolicy);

    /* Validate access to the fsm policy */
    TEST_ASSERT_NOT_NULL(fpolicy);

    spolicy = &spolicies[1];
    fsm_update_policy(spolicy);
    fpolicy = fsm_policy_lookup(spolicy);

    /* Validate mac content */
    rules = &fpolicy->rules;
    macs_set = rules->macs;
    TEST_ASSERT_NOT_NULL(macs_set);
    TEST_ASSERT_EQUAL_INT(spolicy->macs_len, macs_set->nelems);
    for (i = 0; i < macs_set->nelems; i++)
    {
        LOGT("%s: mac set %zu: %s", __func__, i, macs_set->array[i]);
        TEST_ASSERT_EQUAL_STRING(spolicy->macs[i], macs_set->array[i]);
    }
    /* Free the policy */
    fsm_delete_policy(spolicy);
}


void test_add_spolicy2(void)
{
    struct schema_FSM_Policy *spolicy;
    struct fsm_policy *fpolicy;
    struct policy_table *table;
    struct fsm_policy_rules *rules;
    struct str_set *macs_set;
    size_t i;

    /* Add the policy */
    spolicy = &spolicies[2];
    fsm_add_policy(spolicy);
    fpolicy = fsm_policy_lookup(spolicy);

    /* Validate access to the fsm policy */
    TEST_ASSERT_NOT_NULL(fpolicy);

    /* Validate rule name */
    TEST_ASSERT_EQUAL_STRING(spolicy->name, fpolicy->rule_name);

    /* Validate mac content */
    rules = &fpolicy->rules;
    macs_set = rules->macs;
    TEST_ASSERT_NOT_NULL(macs_set);
    TEST_ASSERT_EQUAL_INT(spolicy->macs_len, macs_set->nelems);
    for (i = 0; i < macs_set->nelems; i++)
    {
        LOGT("%s: mac set %zu: %s", __func__, i, macs_set->array[i]);
        TEST_ASSERT_EQUAL_STRING(spolicy->macs[i], macs_set->array[i]);
    }

    /* Validate table name */
    table = fpolicy->table;
    LOGT("%s: table: %s", __func__, table->name);
    TEST_ASSERT_EQUAL_STRING(spolicy->policy, table->name);

    /* Free the policy */
    fsm_delete_policy(spolicy);
}

void test_check_next_spolicy2(void)
{
    struct schema_FSM_Policy *spolicy;
    struct fsm_policy *fpolicy;
    struct policy_table *table;
    struct fsm_policy_session *mgr;

    /* Add the policy */
    spolicy = &spolicies[2];
    fsm_add_policy(spolicy);
    fpolicy = fsm_policy_lookup(spolicy);

    /* Validate access to the fsm policy */
    TEST_ASSERT_NOT_NULL(fpolicy);

    /* Check next table settings */
    TEST_ASSERT_TRUE(fpolicy->jump_table);
    TEST_ASSERT_EQUAL_STRING(spolicy->next_keys[0], fpolicy->next_table);
    TEST_ASSERT_EQUAL_INT(spolicy->next[0], fpolicy->next_table_index);

    /* Check that the next table was created */
    mgr = get_mgr();
    table = ds_tree_find(&mgr->policy_tables, fpolicy->next_table);
    TEST_ASSERT_NOT_NULL(table);
}

bool
test_cat_check(struct fsm_session *session,
               struct fsm_policy_req *req,
               struct fsm_policy *policy)
{
    struct fsm_policy_rules *rules;
    struct fqdn_pending_req *fqdn_req;
    struct fsm_url_request *req_info;
    struct fsm_url_reply *reply;
    bool rc;

    rules = &policy->rules;
    if (!rules->cat_rule_present) return true;

    fqdn_req = req->fqdn_req;
    req_info = fqdn_req->req_info;

    /* Allocate a reply container */
    reply = calloc(1, sizeof(*reply));
    if (reply == NULL) return false;

    req_info->reply = reply;

    reply->service_id = URL_WP_SVC;
    reply->nelems = 1;
    reply->categories[0] = 1; /* In policy blocked categories */
    reply->wb.risk_level = 1;
    fqdn_req->categorized = FSM_FQDN_CAT_SUCCESS;

    rc = fsm_fqdncats_in_set(req, policy);
    /*
     * If category in set and policy applies to categories out of the set,
     * no match
     */
    if (rc && (rules->cat_op == CAT_OP_OUT)) return false;

    /*
     * If category not in set and policy applies to categories in the set,
     * no match
     */
    if (!rc && (rules->cat_op == CAT_OP_IN)) return false;

    return true;
}


bool
test_risk_level(struct fsm_session *session,
                struct fsm_policy_req *req,
                struct fsm_policy *policy)
{
    return true;
}


void test_apply_policies(void)
{
    struct schema_FSM_Policy *spolicy;
    struct fsm_policy *fpolicy;
    struct fsm_policy_rules *rules;
    struct int_set *fqdncats;
    struct fqdn_pending_req fqdn_req;
    struct fsm_url_request req_info;
    struct fsm_policy_req req;
    os_macaddr_t dev_mac;
    struct fsm_policy_reply *reply;
    struct fsm_policy_session *mgr;
    struct policy_table *table;
    struct fsm_session session = { 0 };
    size_t i;

    /* Initialize local structures */
    memset(&fqdn_req, 0, sizeof(fqdn_req));
    memset(&req_info, 0, sizeof(req_info));
    memset(&req, 0, sizeof(req));
    memset(&dev_mac, 0, sizeof(dev_mac));

    /* Insert dev_webpulse policy */
    spolicy = &spolicies[3];
    fsm_add_policy(spolicy);
    fpolicy = fsm_policy_lookup(spolicy);

    mgr = get_mgr();
    table = ds_tree_find(&mgr->policy_tables, spolicy->policy);
    TEST_ASSERT_NOT_NULL(table);

    /* Validate access to the fsm policy */
    TEST_ASSERT_NOT_NULL(fpolicy);

    /* Validate fqdncats and risk level settings */
    rules = &fpolicy->rules;
    fqdncats = rules->categories;
    TEST_ASSERT_NOT_NULL(fqdncats);

    TEST_ASSERT_EQUAL_INT(spolicy->fqdncats_len, fqdncats->nelems);
    for (i = 0; i < fqdncats->nelems; i++)
    {
        LOGT("%s: category[%zu] = %d", __func__, i, fqdncats->array[i]);
        TEST_ASSERT_EQUAL_INT(spolicy->fqdncats[i], fqdncats->array[i]);
    }

    TEST_ASSERT_EQUAL_INT(spolicy->risk_level, rules->risk_level);
    req.device_id = &dev_mac;
    req.fqdn_req = &fqdn_req;

    /* Build the request elements */
    strncpy(req_info.url, "www.playboy.com", sizeof(req_info.url));
    fqdn_req.req_info = &req_info;
    fqdn_req.numq = 1;
    fqdn_req.policy_table = table;
    fqdn_req.categories_check = test_cat_check;
    fqdn_req.risk_level_check = test_risk_level;
    req.fqdn_req = &fqdn_req;
    fsm_apply_policies(&session, &req);
    reply = &req.reply;
    TEST_ASSERT_EQUAL_INT(FSM_BLOCK, reply->action);
    TEST_ASSERT_EQUAL_INT(FSM_REPORT_BLOCKED, reply->log);
    free(req_info.reply);
}

static void test_update_client(struct fsm_session *session,
                               struct policy_table *table)
{
    struct fsm_policy_client *client;

    client = (struct fsm_policy_client *)session->handler_ctxt;
    TEST_ASSERT_NOT_NULL(client);
    client->table = table;
}

void test_fsm_policy_client(void)
{
    struct fsm_session *session;
    struct schema_FSM_Policy *spolicy;
    struct fsm_policy_client *client;
    char *default_name = "default";
    struct policy_table *table;
    struct fsm_policy_session *mgr;

    /* Insert default policy */
    spolicy = &spolicies[0];
    fsm_add_policy(spolicy);

    session = calloc(1, sizeof(*session));
    TEST_ASSERT_NOT_NULL(session);

    client = calloc(1, sizeof(*client));
    TEST_ASSERT_NOT_NULL(client);
    session->handler_ctxt = client;
    client->session = session;
    client->update_client = test_update_client;

    /* Register the client. Its table pointer should be set */
    fsm_policy_register_client(client);
    TEST_ASSERT_NOT_NULL(client->table);

    mgr = get_mgr();
    table = ds_tree_find(&mgr->policy_tables, default_name);
    TEST_ASSERT_NOT_NULL(table);
    TEST_ASSERT_TRUE(table == client->table);

    fsm_policy_deregister_client(client);
    TEST_ASSERT_NULL(client->table);

    /* Register the client with no matching policy yet */
    spolicy = &spolicies[2];
    client->name = strdup(spolicy->policy);
    TEST_ASSERT_NOT_NULL(client->name);
    fsm_policy_register_client(client);
    TEST_ASSERT_NULL(client->table);

    /* Insert matching policy */
    fsm_add_policy(spolicy);

    table = ds_tree_find(&mgr->policy_tables, spolicy->policy);
    TEST_ASSERT_NOT_NULL(table);

    /* Validate that the client's table pointer was updated */
    TEST_ASSERT_NOT_NULL(client->table);
    TEST_ASSERT_TRUE(table == client->table);

    free(client->name);
    free(client);
    free(session);
}


/**
 * @brief test the translation of ip threat attributes from ovsdb to fsm policy
 */
void test_ip_threat_settings(void)
{
    struct schema_FSM_Policy *spolicy;
    struct fsm_policy_rules *rules;
    struct fsm_policy *fpolicy;
    struct str_set *ipaddrs;
    size_t i;

    /* Add the policy */
    spolicy = &spolicies[4];
    fsm_add_policy(spolicy);
    fpolicy = fsm_policy_lookup(spolicy);

    /* Validate access to the fsm policy */
    TEST_ASSERT_NOT_NULL(fpolicy);

    /* Validate ipaddrs and risk level settings */
    rules = &fpolicy->rules;
    ipaddrs = rules->ipaddrs;
    TEST_ASSERT_NOT_NULL(ipaddrs);

    TEST_ASSERT_EQUAL_INT(spolicy->ipaddrs_len, ipaddrs->nelems);
    for (i = 0; i < ipaddrs->nelems; i++)
    {
        LOGT("%s: ipaddrs[%zu] = %s", __func__, i, ipaddrs->array[i]);
        TEST_ASSERT_EQUAL_STRING(spolicy->ipaddrs[i], ipaddrs->array[i]);
    }

    TEST_ASSERT_EQUAL_INT(spolicy->risk_level, rules->risk_level);
}


int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    target_log_open("TEST", LOG_OPEN_STDOUT);
    log_severity_set(LOG_SEVERITY_TRACE);

    UnityBegin(test_name);

    RUN_TEST(test_add_spolicy0);
    RUN_TEST(test_update_spolicy0);
    RUN_TEST(test_add_spolicy2);
    RUN_TEST(test_check_next_spolicy2);
    RUN_TEST(test_apply_policies);
    RUN_TEST(test_fsm_policy_client);
    RUN_TEST(test_ip_threat_settings);

    return UNITY_END();
}

