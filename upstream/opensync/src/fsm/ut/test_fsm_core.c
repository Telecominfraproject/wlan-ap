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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <net/if.h>

#include "fsm.h"
#include "log.h"
#include "target.h"
#include "unity.h"

/**
 * @brief a set of sessions as delivered by the ovsdb API
 */
struct schema_Flow_Service_Manager_Config g_confs[] =
{
    /* parser plugin, no type provided */
    {
        .handler = "fsm_session_test_0",
        .plugin = "plugin_0",
        .pkt_capt_filter = "bpf_filter_0",
        .other_config_keys =
        {
            "mqtt_v",                       /* topic */
            "dso_init",                     /* plugin init routine */
            "policy_table",                 /* FSM policy entry */
        },
        .other_config =
        {
            "dev-test/fsm_core_ut/topic_0", /* topic */
            "test_dso_init",                /* plugin init routine */
            "test_policy",                  /* FSM policy entry */
        },
        .other_config_len = 3,
    },

    /* parser plugin, type provided */
    {
        .handler = "fsm_session_test_1",
        .plugin = "plugin_1",
        .pkt_capt_filter = "bpf_filter_1",
        .type = "parser",
        .other_config_keys =
        {
            "mqtt_v",                       /* topic */
            "dso_init",                     /* plugin init routine */
            "provider",                     /* service provider */
            "policy_table",                 /* FSM policy entry */
        },
        .other_config =
        {
            "dev-test/fsm_core_ut/topic_1", /* topic */
            "test_dso_init",                /* plugin init routine */
            "test_provider",                /* service provider */
            "test_policy",                  /* FSM policy entry */
        },
        .other_config_len = 4,
    },

    /* web categorization plugin */
    {
        .handler = "fsm_session_test_2",
        .plugin = "plugin_2",
        .type = "web_cat_provider",
        .other_config_keys =
        {
            "mqtt_v",                       /* topic */
            "dso_init",                     /* plugin init routine */
        },
        .other_config =
        {
            "dev-test/fsm_core_ut/topic_2", /* topic */
            "test_dso_init",                /* plugin init routine */
        },
        .other_config_len = 2,
    },

    /*dpi plugin */
    {
        .handler = "fsm_session_test_3",
        .plugin = "plugin_3",
        .type = "dpi",
        .other_config_keys =
        {
            "mqtt_v",                       /* topic */
            "dso_init",                     /* plugin init routine */
        },
        .other_config =
        {
            "dev-test/fsm_core_ut/topic_3", /* topic */
            "test_dso_init",                /* plugin init routine */
        },
        .other_config_len = 2,
    },

    /* bogus plugin */
    {
        .handler = "fsm_bogus_plugin",
        .plugin = "bogus",
        .type = "bogus",
        .other_config_len = 0,
    },

    {
        .handler = "fsm_session_test_5",
        .plugin = "plugin_5",
        .pkt_capt_filter = "bpf_filter_5",
        .other_config_keys =
        {
            "mqtt_v",                       /* topic */
            "dso_init",                     /* plugin init routine */
            "policy_table",                 /* FSM policy entry */
            "tx_intf",                      /* Plugin tx interface */
        },
        .other_config =
        {
            "dev-test/fsm_core_ut/topic_0", /* topic */
            "test_dso_init",                /* plugin init routine */
            "test_policy",                  /* FSM policy entry */
            "test_intf.tx",                 /* Plugin tx interface */
        },
        .other_config_len = 4,
    },
};

/**
 * @brief a AWLAN_Node structure
 */
struct schema_AWLAN_Node g_awlan_nodes[] =
{
    {
        .mqtt_headers_keys =
        {
            "locationId",
            "nodeId",
        },
        .mqtt_headers =
        {
            "59efd33d2c93832025330a3e",
            "4C718002B3",
        },
        .mqtt_headers_len = 2,
    },
    {
        .mqtt_headers_keys =
        {
            "locationId",
            "nodeId",
        },
        .mqtt_headers =
        {
            "59efd33d2c93832025330a3e",
            "4C7XXXXXXX",
        },
        .mqtt_headers_len = 2,
    },
};


const char *test_name = "ustack_tests";

struct fsm_mgr *g_mgr;


static bool
test_init_plugin(struct fsm_session *session)
{
    return true;
}


static bool
test_flood_mod(struct fsm_session *session)
{
    return true;
}

static const char g_test_br[16] = "test_br";
static int
test_get_br(char *if_name, char *bridge, size_t len)
{
    strscpy(bridge, g_test_br, len);
    return 0;
}


void
setUp(void)
{
    size_t nelems;
    size_t i;

    fsm_init_mgr(NULL);
    g_mgr->init_plugin = test_init_plugin;
    g_mgr->flood_mod = test_flood_mod;
    g_mgr->get_br = test_get_br;
    nelems = (sizeof(g_confs) / sizeof(g_confs[0]));
    for (i = 0; i < nelems; i++)
    {
        struct schema_Flow_Service_Manager_Config *conf;

        conf = &g_confs[i];
        memset(conf->if_name, 0, sizeof(conf->if_name));
    }
    fsm_get_awlan_headers(&g_awlan_nodes[0]);

    return;
}


void
tearDown(void)
{
    fsm_reset_mgr();
    g_mgr->init_plugin = NULL;

    return;
}


/**
 * @brief update mqtt_headers
 *
 * setUp() sets the mqtt headers. Validate the original values,
 * update and validate.
 */
void
test_add_awlan_headers(void)
{
    struct fsm_mgr *mgr;
    char *location_id;
    char *expected;
    char *node_id;

    mgr = fsm_get_mgr();

    /* Validate original headers */
    location_id = mgr->location_id;
    TEST_ASSERT_NOT_NULL(location_id);
    expected = g_awlan_nodes[0].mqtt_headers[0];
    TEST_ASSERT_EQUAL_STRING(expected, location_id);
    node_id = mgr->node_id;
    TEST_ASSERT_NOT_NULL(node_id);
    expected = g_awlan_nodes[0].mqtt_headers[1];
    TEST_ASSERT_EQUAL_STRING(expected, node_id);

    /* Update headers, validate again */
    fsm_get_awlan_headers(&g_awlan_nodes[1]);

    location_id = mgr->location_id;
    TEST_ASSERT_NOT_NULL(location_id);
    expected = g_awlan_nodes[1].mqtt_headers[0];
    TEST_ASSERT_EQUAL_STRING(expected, location_id);
    node_id = mgr->node_id;
    TEST_ASSERT_NOT_NULL(node_id);
    expected = g_awlan_nodes[1].mqtt_headers[1];
    TEST_ASSERT_EQUAL_STRING(expected, node_id);
}


/**
 * @brief create a fsm_session
 *
 * Create a fsm session after AWLAN_node was populated
 */
void
test_add_session_after_awlan(void)
{
    struct schema_Flow_Service_Manager_Config *conf;
    struct fsm_session_conf *fconf;
    struct fsm_session *session;
    ds_tree_t *sessions;
    char *topic;

    conf = &g_confs[0];
    fsm_add_session(conf);
    sessions = fsm_get_sessions();
    session = ds_tree_find(sessions, conf->handler);
    TEST_ASSERT_NOT_NULL(session);

    /* Validate session configuration */
    fconf = session->conf;
    TEST_ASSERT_NOT_NULL(fconf);
    TEST_ASSERT_EQUAL_STRING(conf->handler, fconf->handler);
    TEST_ASSERT_EQUAL_STRING(conf->handler, session->name);
    topic = session->ops.get_config(session, "mqtt_v");
    TEST_ASSERT_EQUAL_STRING(conf->other_config[0], topic);
}


/**
 * @brief validate plugin types
 */
void
test_plugin_types(void)
{
    struct schema_Flow_Service_Manager_Config *conf;
    int type;

    /* Parser type, not explicitly configured */
    conf = &g_confs[0];
    type = fsm_service_type(conf);
    TEST_ASSERT_EQUAL_INT(FSM_PARSER, type);

    /* Parser type, explicitly configured */
    conf = &g_confs[1];
    type = fsm_service_type(conf);
    TEST_ASSERT_EQUAL_INT(FSM_PARSER, type);

    /* web categorization type */
    conf = &g_confs[2];
    type = fsm_service_type(conf);
    TEST_ASSERT_EQUAL_INT(FSM_WEB_CAT, type);

    /* dpi type */
    conf = &g_confs[3];
    type = fsm_service_type(conf);
    TEST_ASSERT_EQUAL_INT(FSM_DPI, type);

    /* bogus type */
    conf = &g_confs[4];
    type = fsm_service_type(conf);
    TEST_ASSERT_EQUAL_INT(FSM_UNKNOWN_SERVICE, type);
}


/**
 * @brief validate session duplication
 */
void
test_duplicate_session(void)
{
    struct schema_Flow_Service_Manager_Config *conf;
    struct fsm_session *session;
    ds_tree_t *sessions;
    char *service_name;
    bool ret;

    /* Add a session with an explicit provider */
    conf = &g_confs[1];
    fsm_add_session(conf);
    sessions = fsm_get_sessions();
    session = ds_tree_find(sessions, conf->handler);
    TEST_ASSERT_NOT_NULL(session);

    /* Create the related service provider */
    ret = fsm_dup_web_cat_session(session);
    TEST_ASSERT_TRUE(ret);

    service_name = session->ops.get_config(session, "provider");
    TEST_ASSERT_NOT_NULL(service_name);
    session = ds_tree_find(sessions, service_name);
    TEST_ASSERT_NOT_NULL(session);
    TEST_ASSERT_EQUAL_INT(FSM_WEB_CAT, session->type);
    TEST_ASSERT_EQUAL_STRING("/usr/plume/lib/libfsm_test_provider.so",
                             session->dso);
}


/**
 * @brief validate session tx interface when provided by the controller
 */
void
test_tx_intf_controller(void)
{
    struct schema_Flow_Service_Manager_Config *conf;
    struct fsm_session *session;
    ds_tree_t *sessions;

    conf = &g_confs[5];
    fsm_add_session(conf);
    sessions = fsm_get_sessions();
    session = ds_tree_find(sessions, conf->handler);
    TEST_ASSERT_NOT_NULL(session);
    TEST_ASSERT_NOT_EQUAL(0, strlen(session->tx_intf));
    TEST_ASSERT_EQUAL_STRING("test_intf.tx", session->tx_intf);
}


/**
 * @brief validate session tx interface when provided through Kconfig
 */
void
test_tx_intf_kconfig(void)
{
    struct schema_Flow_Service_Manager_Config *conf;
    struct fsm_session *session;
    ds_tree_t *sessions;
    char tx_intf[IFNAMSIZ];

#if !defined(CONFIG_TARGET_LAN_BRIDGE_NAME)
#define FORCE_BRIDGE_NAME 1
#define CONFIG_TARGET_LAN_BRIDGE_NAME "kconfig_br"
#endif

    memset(tx_intf, 0, sizeof(tx_intf));
    snprintf(tx_intf, sizeof(tx_intf), "%s.tx", CONFIG_TARGET_LAN_BRIDGE_NAME);
    LOGI("%s: CONFIG_TARGET_LAN_BRIDGE_NAME == %s", __func__, CONFIG_TARGET_LAN_BRIDGE_NAME);

    conf = &g_confs[0];
    fsm_add_session(conf);
    sessions = fsm_get_sessions();
    session = ds_tree_find(sessions, conf->handler);
    TEST_ASSERT_NOT_NULL(session);
    TEST_ASSERT_NOT_EQUAL(0, strlen(session->tx_intf));
    TEST_ASSERT_EQUAL_STRING(tx_intf, session->tx_intf);

#if defined(FORCE_BRIDGE_NAME)
#undef CONFIG_TARGET_LAN_BRIDGE_NAME
#undef FORCE_BRIDGE_NAME
#endif
}


int
main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    g_mgr = fsm_get_mgr();

    target_log_open("TEST", LOG_OPEN_STDOUT);
    log_severity_set(LOG_SEVERITY_INFO);

    UnityBegin(test_name);

    RUN_TEST(test_add_awlan_headers);
    RUN_TEST(test_add_session_after_awlan);
    RUN_TEST(test_plugin_types);
    RUN_TEST(test_duplicate_session);
    RUN_TEST(test_tx_intf_controller);
    RUN_TEST(test_tx_intf_kconfig);

    return UNITY_END();
}
