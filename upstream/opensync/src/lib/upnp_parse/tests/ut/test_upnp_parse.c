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

#include "upnp_parse.h"
#include "json_util.h"
#include "log.h"
#include "qm_conn.h"
#include "target.h"
#include "unity.h"

#include "pcap.c"

#define OTHER_CONFIG_NELEMS 4
#define OTHER_CONFIG_NELEM_SIZE 32

char g_other_configs[][2][OTHER_CONFIG_NELEMS][OTHER_CONFIG_NELEM_SIZE] =
{
    {
        {
            "mqtt_v",
        },
        {
            "dev-test/upnp_ut_topic",
        },
    },
};


struct fsm_session_conf g_confs[2] =
{
    {
        .handler = "upnp_test_session_0",
    },
    {
        .handler = "upnp_test_session_1",
    }
};


static void send_report(struct fsm_session *session, char *report)
{
#ifndef ARCH_X86
    qm_response_t res;
    bool ret = false;
#endif

    LOGT("%s: msg len: %zu, msg: %s\n, topic: %s",
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


struct upnp_cache *g_mgr;
const char *test_name = "upnp_tests";

/**
 * @brief Converts a bytes array in a hex dump file wireshark can import.
 *
 * Dumps the array in a file that can then be imported by wireshark.
 * The file can also be translated to a pcap file using the text2pcap command.
 * Useful to visualize the packet content.
 */
void create_hex_dump(const char *fname, const uint8_t *buf, size_t len)
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

/**
 * @brief Convenient wrapper
 *
 * Dumps the packet content in tmp/<tests_name>_<pkt name>.txtpcap
 * for wireshark consumption and sets g_parser data fields.
 * @params pkt the C structure containing an exported packet capture
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
                                                  g_other_configs[0][0],
                                                  g_other_configs[0][1]);
        pair = ds_tree_find(session->conf->other_config, "mqtt_v");
        session->topic = pair->value;
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

void setUp(void)
{
    size_t n_sessions, i;

    g_mgr = NULL;
    n_sessions = sizeof(g_sessions) / sizeof(struct fsm_session);

    /* Reset sessions, register them to the plugin */
    for (i = 0; i < n_sessions; i++)
    {
        struct fsm_session *session = &g_sessions[i];

        session->name = g_confs[i].handler;
        session->ops  = g_ops;
        upnp_plugin_init(session);
    }
    g_mgr = upnp_get_mgr();

    return;
}

void tearDown(void)
{
    size_t n_sessions, i;

    n_sessions = sizeof(g_sessions) / sizeof(struct fsm_session);

    /* Reset sessions, unregister them */
    for (i = 0; i < n_sessions; i++)
    {
        struct fsm_session *session = &g_sessions[i];

        upnp_plugin_exit(session);
    }
    g_mgr = NULL;

    return;
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

    /* More testing to come */
}


void test_upnp_get_url(void)
{
    struct fsm_session *session;
    struct upnp_session *u_session;
    struct upnp_parser *parser;
    struct net_header_parser *net_parser;
    struct upnp_device_url *url;
    char *expected_url = "http://10.1.0.48:8080/description.xml";
    size_t len;

    session = &g_sessions[0];
    u_session = upnp_lookup_session(session);
    TEST_ASSERT_NOT_NULL(u_session);

    parser = &u_session->parser;
    net_parser = calloc(1, sizeof(*net_parser));
    TEST_ASSERT_NOT_NULL(net_parser);
    parser->net_parser = net_parser;
    PREPARE_UT(pkt322, net_parser);
    len = net_header_parse(net_parser);
    TEST_ASSERT_TRUE(len != 0);
    len = upnp_parse_message(parser);
    TEST_ASSERT_TRUE(len != 0);
    TEST_ASSERT_EQUAL_UINT(sizeof(pkt322), net_parser->packet_len);

    url = upnp_get_url(u_session);
    TEST_ASSERT_NOT_NULL(url);
    TEST_ASSERT_EQUAL_STRING(expected_url, url->url);
    free(net_parser);
}


int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    target_log_open("TEST", LOG_OPEN_STDOUT);
    log_severity_set(LOG_SEVERITY_TRACE);

    UnityBegin(test_name);

    global_test_init();

    RUN_TEST(test_load_unload_plugin);
    RUN_TEST(test_upnp_get_url);

    global_test_exit();

    return UNITY_END();
}
