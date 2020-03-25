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

#include "fsm_demo_plugin.h"
#include "json_util.h"
#include "log.h"
#include "qm_conn.h"
#include "target.h"
#include "unity.h"
#include "network_metadata_report.h"
#include "pcap.c"

const char *test_name = "fsm_demo_plugin_tests";

#define OTHER_CONFIG_NELEMS 4
#define OTHER_CONFIG_NELEM_SIZE 128

char g_other_configs[][2][OTHER_CONFIG_NELEMS][OTHER_CONFIG_NELEM_SIZE] =
{
    {
        {
            "mqtt_v",
        },
        {
            "dev-test/fsm_demo_plugin_0/4C70D0007B",
        },
    },
    {
        {
            "mqtt_v",
        },
        {
            "dev-test/fsm_demo_plugin_1/4C70D0007B",
        },
    },
};


/**
 * @brief a set of sessions as delivered by the ovsdb API
 */
struct fsm_session_conf g_confs[2] =
{
    {
        .handler = "fsm_demo_test_0",
    },
    {
        .handler = "fsm_demo_test_1",
    }
};


union fsm_plugin_ops p_ops;


struct fsm_session g_sessions[2] =
{
    {
        .type = FSM_WEB_CAT,
        .conf = &g_confs[0],
    },
    {
        .type = FSM_WEB_CAT,
        .conf = &g_confs[1],
    }
};

/**
 * @brief sends a json mqtt report
 *
 * Sends and frees a json report over mqtt.
 * when running UTs on the pod, actually send the report
 * as QM is expected to be running.
 * When running UTs on X86 platforms, simply free the report.
 * @params session the fsm session owning the mqtt topic.
 * @params the json report.
 */
static void
send_report(struct fsm_session *session, char *report)
{
#ifndef ARCH_X86
    qm_response_t res;
    bool ret = false;
#endif

    LOGD("%s: msg len: %zu, msg: %s\n, topic: %s",
         __func__, report ? strlen(report) : 0,
         report ? report : "None", session->topic);
    if (report == NULL) return;

#ifndef ARCH_X86
    ret = qm_conn_send_direct(QM_REQ_COMPRESS_DISABLE, session->topic,
                                       report, strlen(report), &res);
    if (!ret) LOGE("error sending mqtt with topic %s", session->topic);
#endif
    json_free(report);

    return;
}


struct fsm_session_ops g_ops =
{
    .send_report = send_report,
};


struct fsm_demo_plugin_cache *g_mgr;

/**
 * @brief Converts a bytes array in a hex dump file wireshark can import.
 *
 * Dumps the array in a file that can then be imported by wireshark.
 * The file can also be translated to a pcap file using the text2pcap command.
 * Useful to visualize the packet content.
 * @param fname the file recipient of the hex dump
 * @param buf the buffer to dump
 * @param length the length of the buffer to dump
 */
void
create_hex_dump(const char *fname, const uint8_t *buf, size_t len)
{
    int line_number = 0;
    bool new_line = true;
    size_t i;
    FILE *f;

    f = fopen(fname, "w+");

    if (f == NULL) return;

    for (i = 0; i < len; i++)
    {
	 new_line = (i == 0 ? true : ((i % 8) == 0));
	 if (new_line)
	 {
	      if (line_number) fprintf(f, "\n");
	      fprintf(f, "%06x", line_number);
	      line_number += 8;
	 }
         fprintf(f, " %02x", buf[i]);
    }
    fprintf(f, "\n");
    fclose(f);

    return;
}


char *g_location_id = "foo";
char *g_node_id = "bar";

/**
 * @brief Convenient wrapper
 *
 * Dumps the packet content in /tmp/<tests_name>_<pkt name>.txtpcap
 * for wireshark consumption and sets the given parser's data fields.
 * @param pkt the C structure containing an exported packet capture
 * @param parser theparser structure to set
 */
#define PREPARE_UT(pkt, parser)                                 \
    {                                                           \
        char fname[128];                                        \
        size_t len = sizeof(pkt);                               \
                                                                \
        snprintf(fname, sizeof(fname), "/tmp/%s_%s.txtpcap",    \
                 test_name, #pkt);                              \
        create_hex_dump(fname, pkt, len);                       \
        parser->packet_len = len;                               \
        parser->data = (uint8_t *)pkt;                          \
    }

void global_test_init(void)
{
    size_t n_sessions, i;

    g_mgr = NULL;
    n_sessions = sizeof(g_sessions) / sizeof(struct fsm_session);

    /* Reset sessions, register them to the plugin */
    for (i = 0; i < n_sessions; i++)
    {
        struct fsm_session *session = &g_sessions[i];
        struct str_pair *pair;

        session->conf = &g_confs[i];
        session->ops  = g_ops;
        session->p_ops = &p_ops;
        session->name = g_confs[i].handler;
        session->conf->other_config = schema2tree(OTHER_CONFIG_NELEM_SIZE,
                                                  OTHER_CONFIG_NELEM_SIZE,
                                                  OTHER_CONFIG_NELEMS,
                                                  g_other_configs[i][0],
                                                  g_other_configs[i][1]);
        pair = ds_tree_find(session->conf->other_config, "mqtt_v");
        session->topic = pair->value;
        session->location_id = g_location_id;
        session->node_id = g_location_id;
    }
}

void global_test_exit(void)
{
    size_t n_sessions, i;

    g_mgr = NULL;
    n_sessions = sizeof(g_sessions) / sizeof(struct fsm_session);

    /* Reset sessions, register them to the plugin */
    for (i = 0; i < n_sessions; i++)
    {
        struct fsm_session *session = &g_sessions[i];

        free_str_tree(session->conf->other_config);
    }
}

/**
 * @brief called by the Unity framework before every single test
 */
void setUp(void)
{
    size_t n_sessions, i;

    g_mgr = NULL;
    n_sessions = sizeof(g_sessions) / sizeof(struct fsm_session);

    /* Reset sessions, register them to the plugin */
    for (i = 0; i < n_sessions; i++)
    {
        struct fsm_session *session = &g_sessions[i];

        fsm_demo_plugin_init(session);
    }
    g_mgr = fsm_demo_get_mgr();

    return;
}

/**
 * @brief called by the Unity framework after every single test
 */
void tearDown(void)
{
    size_t n_sessions, i;

    n_sessions = sizeof(g_sessions) / sizeof(struct fsm_session);

    /* Reset sessions, unregister them */
    for (i = 0; i < n_sessions; i++)
    {
        struct fsm_session *session = &g_sessions[i];

        fsm_demo_plugin_exit(session);
    }
    g_mgr = NULL;

    return;
}

/**
 * @brief emits a protobuf report
 *
 * Assumes the presence of QM to send the report on non native platforms,
 * simply resets the aggregator content for native target.
 * @param aggr the aggregator
 */
static void test_emit_report(struct fsm_session *session,
                             struct net_md_aggregator *aggr)
{
#ifndef ARCH_X86
    bool ret;

    /* Send the report */
    ret = net_md_send_report(aggr, session->topic);
    TEST_ASSERT_TRUE(ret);
#else
    struct packed_buffer *pb;
    pb = serialize_flow_report(aggr->report);

    /* Free the serialized container */
    free_packed_buffer(pb);
    net_md_reset_aggregator(aggr);
#endif
}

/**
 * @brief validate that no session provided is handled correctly
 */
void test_no_session(void)
{
    int ret;

    ret = fsm_demo_plugin_init(NULL);
    TEST_ASSERT_TRUE(ret == -1);
}


/**
 * @brief test plugin init()/exit() sequence
 *
 * Validate plugin reference counts and pointers
 */
void test_load_unload_plugin(void)
{
    /* SetUp() has called init(). Validate settings */
    TEST_ASSERT_NOT_NULL(g_mgr);

    /*
     * tearDown() will call exit().
     * ASAN enabled run of the test will validate that
     * there is no memory leak.
     */
}


/**
 * @brief validate the generation of a json mqtt report
 */
void test_json_report(void)
{
    struct fsm_session *session;
    char *report;

    /* Select the first active session */
    session = &g_sessions[0];

    /* Generate a report */
    report = demo_jencode_demo_event(session);

    /* Validate that the report was generated */
    TEST_ASSERT_NOT_NULL(report);

    /* Send the report */
    session->ops.send_report(session, report);
}

/**
 * @brief validate message parsing
 */
void test_process_msg(void)
{
    struct net_header_parser *net_parser;
    struct fsm_demo_session *f_session;
    struct fsm_demo_parser *parser;
    struct fsm_session *session;
    size_t len;

    /* Select the first active session */
    session = &g_sessions[0];
    f_session = fsm_demo_lookup_session(session);
    TEST_ASSERT_NOT_NULL(f_session);

    parser = &f_session->parser;
    net_parser = calloc(1, sizeof(*net_parser));
    TEST_ASSERT_NOT_NULL(net_parser);
    parser->net_parser = net_parser;
    PREPARE_UT(pkt9568, net_parser);
    len = net_header_parse(net_parser);
    TEST_ASSERT_TRUE(len != 0);
    len = fsm_demo_parse_message(parser);
    TEST_ASSERT_TRUE(len != 0);
    fsm_demo_process_message(f_session);
    net_md_close_active_window(f_session->aggr);
    test_emit_report(session, f_session->aggr);
    free(net_parser);
}

int main(int argc, char *argv[])
{
    /* Set the logs to stdout */
    target_log_open("TEST", LOG_OPEN_STDOUT);
    log_severity_set(LOG_SEVERITY_TRACE);

    UnityBegin(test_name);

    global_test_init();

    RUN_TEST(test_no_session);
    RUN_TEST(test_load_unload_plugin);
    RUN_TEST(test_json_report);
    RUN_TEST(test_process_msg);

    global_test_exit();

    return UNITY_END();
}
