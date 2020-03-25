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

#include "network_metadata.h"
#include "network_metadata.pb-c.h"
#include "unity.h"
#include "log.h"
#include "target.h"
#include "qm_conn.h"

#include "test_network_metadata.h"

/**
 * brief global variable initialized in @see setUp()
 */
struct test_network_data g_test;

extern struct test_network_data_report g_nd_test;

/**
 * @brief sets a node info.
 *
 * Called in @see @setUp()
 *
 * @param none
 * @return node info pointer if successful, NULL otherwise
 */
static struct node_info *set_node_info(void)
{
    struct node_info *node;

    node = calloc(1, sizeof(*node));
    if (node == NULL) return NULL;

    node->node_id = strdup("4C71000027");
    if (node->node_id == NULL) goto err_free_node;

    node->location_id = strdup("my_test_location");
    if (node->location_id == NULL) goto err_free_node_id;

    return node;

err_free_node_id:
    free(node->node_id);

err_free_node:
    free(node);

    return NULL;
}


/**
 * @brief sets a flow key structure
 *
 * Called in @see setUp(). If id is 1, set most fields to 0/NULL to test
 * optional fields output.
 *
 * @param key info to be set
 * @param id integer appended to string fields and added to integer fields
 */
static struct flow_key * set_flow_key(int id)
{
    char smac[32];
    char dmac[32];
    char src_ip[32];
    char dst_ip[32];
    struct flow_key *key;
    struct flow_tags *tag;

    key = calloc(1, sizeof(*key));
    if (key == NULL) return NULL;

    snprintf(smac, sizeof(smac), "aa:bb:cc:dd:e%d", id);
    key->smac = strndup(smac, sizeof(smac));
    if (key->smac == NULL) goto err_free_key;

    snprintf(dmac, sizeof(dmac), "aa:bb:cc:dd:%de", id);
    key->dmac = strndup(dmac, sizeof(dmac));
    if (key->dmac == NULL) goto err_free_smac;

    key->vlan_id = (id == 1 ? 0 : id);
    key->ethertype = (id == 1 ? 0 : (17 + id));

    if (id == 1) return key;

    snprintf(src_ip, sizeof(src_ip), "1.2.3.%d", id);
    key->src_ip = strndup(src_ip, sizeof(src_ip));
    if (key->src_ip == NULL) goto err_free_dmac;

    snprintf(dst_ip, sizeof(dst_ip), "1.2.3.10%d", id);
    key->dst_ip = strndup(dst_ip, sizeof(dst_ip));
    if (key->dst_ip == NULL) goto err_free_src_ip;

    key->sport = (id == 1 ? 0 : (1000 + id));
    key->dport = (id == 1 ? 0 : (2000 + id));

    key->num_tags = 1;
    key->tags = calloc(key->num_tags, sizeof(*key->tags));
    if (key->tags == NULL) goto err_free_dst_ip;

    tag = calloc(1, sizeof(*tag));
    if (tag == NULL) goto err_free_key_tags;

    tag->vendor = strdup("Plume");
    if (tag->vendor == NULL) goto err_free_tag;

    tag->app_name = strdup("Plume App");
    if (tag->vendor == NULL) goto err_free_vendor;

    tag->nelems = 2;
    tag->tags = calloc(tag->nelems, sizeof(tag->tags));
    if (tag->tags == NULL) goto err_free_app_name;
    tag->tags[0] = strdup("Plume Tag0");
    if (tag->tags[0] == NULL) goto err_free_tag_tags;

    tag->tags[1] = strdup("Plume Tag1");
    if (tag->tags[1] == NULL) goto err_free_tag_tags_0;

    *(key->tags) = tag;

    return key;

err_free_tag_tags_0:
    free(tag->tags[0]);

err_free_tag_tags:
    free(tag->tags);

err_free_app_name:
    free(tag->app_name);

err_free_vendor:
    free(tag->vendor);

err_free_tag:
    free(tag);

err_free_key_tags:
    free(key->tags);

err_free_dst_ip:
    free(key->src_ip);

err_free_src_ip:
    free(key->src_ip);

err_free_dmac:
    free(key->smac);

err_free_smac:
    free(key->smac);

err_free_key:
    free(key);

    return NULL;
}


/**
 * @brief sets a flow_counters structure
 *
 * Called in @see setUp()
 * @param key info to be set
 * @param id integer appended to string fields and added to integer fields
 */
static struct flow_counters* set_flow_counters(int id)
{
    struct flow_counters *counters;

    counters = calloc(1, sizeof(*counters));
    if (counters== NULL) return NULL;

    counters->packets_count = (id == 1 ? 0: (500 + id));
    counters->bytes_count = (id == 1 ? 0 : (50000 + id));

    return counters;
}


static struct flow_stats * set_flow_stats(int id)
{
    struct flow_key *key;
    struct flow_counters *counters;
    struct flow_stats *stats;

    key = set_flow_key(id);
    if (key == NULL) return NULL;

    counters =  set_flow_counters(1);
    if (counters == NULL) goto err_free_key;

    stats = calloc(1, sizeof(struct flow_stats));
    if (stats == NULL) goto err_free_counters;

    stats->owns_key = true;
    stats->key = key;
    stats->counters = counters;

    return stats;

err_free_counters:
    free_flow_counters(counters);

err_free_key:
    free_flow_key(key);

    return NULL;
}


static struct flow_window * set_flow_window(uint64_t start, uint64_t end,
                                            size_t num_stats)
{
    struct flow_window *window;

    window = calloc(1, sizeof(*window));
    if (window == NULL) return NULL;

    window->flow_stats = calloc(num_stats, sizeof(struct flow_stats *));
    if(window->flow_stats == NULL) goto err_free_window;

    window->num_stats = num_stats;
    window->started_at = start;
    window->ended_at = end;

    return window;

err_free_window:
    free(window);

    return NULL;
}

static struct flow_report * set_flow_report(size_t num_windows)
{
    struct flow_report *report;

    report = calloc(1, sizeof(*report));
    if (report == NULL) return NULL;

    report->node_info = set_node_info();
    if (report->node_info == NULL) goto err_free_report;

    report->flow_windows = calloc(num_windows, sizeof(struct flow_window *));
    if (report->flow_windows == NULL) goto err_free_node_info;

    report->num_windows = num_windows;
    report->reported_at = 40;

    return report;

err_free_node_info:
    free_node_info(report->node_info);

err_free_report:
    free(report);

    return NULL;
}


/**
 * @brief See unity documentation/exmaples
 */
void setUp(void)
{
    struct flow_window **report_windows;
    struct flow_stats **window_stats;

    g_test.initialized = false;

    memset(&g_test, 0, sizeof(g_test));

    g_test.f_name = strdup("/tmp/serial_proto");
    if (g_test.f_name == NULL) goto err;

    g_test.stats1 = set_flow_stats(1);
    if (g_test.stats1 == NULL) goto err_free_f_name;

    g_test.stats2 = set_flow_stats(2);
    if (g_test.stats2 == NULL) goto err_free_stats1;

    g_test.stats3 = set_flow_stats(3);
    if (g_test.stats3 == NULL) goto err_free_stats2;

    g_test.stats4 = set_flow_stats(4);
    if (g_test.stats4 == NULL) goto err_free_stats3;


    g_test.window1 = set_flow_window(1, 10, 1);
    if (g_test.window1 == NULL) goto err_free_stats4;

    window_stats = g_test.window1->flow_stats;
    *window_stats = g_test.stats1;

    g_test.window2 = set_flow_window(2, 20, 3);
    if (g_test.window1 == NULL) goto err_free_window1;

    window_stats = g_test.window2->flow_stats;
    *window_stats++ = g_test.stats2;
    *window_stats++ = g_test.stats3;
    *window_stats = g_test.stats4;

    g_test.report = set_flow_report(2);
    if (g_test.report == NULL) goto err_free_window2;

    report_windows = g_test.report->flow_windows;
    *report_windows++ = g_test.window1;
    *report_windows = g_test.window2;

    g_test.initialized = true;

    test_net_md_report_setup();

    return;

err_free_window2:
    free_report_window(g_test.window2);
    /* Stats pointers have been freed, set them to NULL */
    g_test.stats2 = NULL;
    g_test.stats3 = NULL;
    g_test.stats4 = NULL;

err_free_window1:
    free_report_window(g_test.window1);
    /* Stats pointers have been freed, set them to NULL */
    g_test.stats1 = NULL;

err_free_stats4:
    free_window_stats(g_test.stats4);

err_free_stats3:
    free_window_stats(g_test.stats3);

err_free_stats2:
    free_window_stats(g_test.stats2);

err_free_stats1:
    free_window_stats(g_test.stats1);

err_free_f_name:
    free(g_test.f_name);

err:
    return;
}


/**
 * @brief See unity documentation/exmaples
 */
void tearDown(void)
{
    free_flow_report(g_test.report);
    g_test.report = NULL;
    g_test.initialized = false;
    free(g_test.f_name);
    test_net_md_report_teardown();
}


/**
 * @brief writes the contents of a serialized buffer in a file
 *
 * @param pb serialized buffer to be written
 * @param fpath target file path
 *
 * @return returns the number of bytes written
 */
static size_t pb2file(struct packed_buffer *pb, char *fpath)
{
    FILE *f = fopen(fpath, "w");
    size_t nwrite = fwrite(pb->buf, 1, pb->len, f);
    fclose(f);

    return nwrite;
}


/**
 * @brief reads the contents of a serialized buffer from a file
 *
 * @param fpath target file path
 * @param pb serialized buffer to be filled
 *
 * @return returns the number of bytes written
 */
static size_t file2pb(char *fpath, struct packed_buffer *pb)
{
    FILE *f = fopen(fpath, "rb");
    size_t nread = fread(pb->buf, 1, pb->len, f);
    fclose(f);

    return nread;
}


/**
 * @brief validates the contents of an observation point protobuf
 *
 * @param node expected node info
 * @param op observation point protobuf to validate
 */
static void validate_node_info(struct node_info *node,
                               Traffic__ObservationPoint *op)
{
    TEST_ASSERT_EQUAL_STRING(node->node_id, op->nodeid);
    TEST_ASSERT_EQUAL_STRING(node->location_id, op->locationid);
}


/**
 * @brief validates the contents of a flow tags protobuf
 *
 * @param flow_tags expected flow_tags info
 * @param op flow tags protobuf to validate
 */
static void validate_flow_tags(struct flow_tags *flow_tags,
                                Traffic__FlowTags *flow_tags_pb)
{
    return;
}


/**
 * @brief validates the integer field of a protobuf
 *
 * If the value to validate is 0, expect the presence boolean to be false.
 * @param node expected node info
 * @param op observation point protobuf to validate
 */
static void validate_uint32_field(uint32_t expected, uint32_t given,
                                  protobuf_c_boolean present)
{
    TEST_ASSERT_EQUAL_UINT(expected, given);
    TEST_ASSERT_EQUAL_INT(expected != 0, present);
}


/**
 * @brief validates the contents of a flow key protobuf
 *
 * @param node expected node info
 * @param op observation point protobuf to validate
 */
static void validate_flow_key(struct flow_key *key,
                              Traffic__FlowKey *key_pb)
{
    TEST_ASSERT_EQUAL_STRING(key->smac, key_pb->srcmac);
    TEST_ASSERT_EQUAL_STRING(key->dmac, key_pb->dstmac);
    TEST_ASSERT_EQUAL_STRING(key->src_ip, key_pb->srcip);
    TEST_ASSERT_EQUAL_STRING(key->dst_ip, key_pb->dstip);
    validate_uint32_field((uint32_t)key->vlan_id, key_pb->vlanid,
                          key_pb->has_vlanid);
    validate_uint32_field((uint32_t)key->ethertype, key_pb->ethertype,
                          key_pb->has_ethertype);
    validate_uint32_field((uint32_t)key->protocol, key_pb->ipprotocol,
                          key_pb->has_ipprotocol);
    validate_uint32_field((uint32_t)key->sport, key_pb->tptsrcport,
                          key_pb->has_tptsrcport);
    validate_uint32_field((uint32_t)key->dport, key_pb->tptdstport,
                          key_pb->has_tptdstport);
}

/**
 * @brief validates the contents of a flow counters protobuf
 *
 * @param node expected node info
 * @param op observation point protobuf to validate
 */
static void validate_flow_counters(struct flow_counters *counters,
                                   Traffic__FlowCounters *counters_pb)
{
    TEST_ASSERT_EQUAL_UINT(counters->packets_count, counters_pb->packetscount);
    TEST_ASSERT_EQUAL_UINT(counters->bytes_count, counters_pb->bytescount);
}


/**
 * @brief validates the contents of a flow stats protobuf
 *
 * @param node expected node info
 * @param op observation point protobuf to validate
 */
static void validate_flow_stats(struct flow_stats *stats,
                                Traffic__FlowStats *stats_pb)
{
    validate_flow_key(stats->key, stats_pb->flowkey);
    validate_flow_counters(stats->counters, stats_pb->flowcount);
}


/**
 * @brief validates the contents of an observation window protobuf
 *
 * @param node expected node info
 * @param op observation point protobuf to validate
 */
static void validate_observation_window(struct flow_window *window,
                                        Traffic__ObservationWindow *window_pb)
{
    TEST_ASSERT_EQUAL_UINT(window->started_at, window_pb->startedat);
    TEST_ASSERT_EQUAL_UINT(window->ended_at, window_pb->endedat);
    TEST_ASSERT_EQUAL_UINT(window->num_stats, window_pb->n_flowstats);
}


/**
 * @brief validates the contents of a flow windows protobuf
 *
 * @param node expected flow report info
 * @param report_pb flow report protobuf to validate
 */
static void validate_flow_windows(struct flow_report *report,
                                  Traffic__FlowReport *report_pb)
{
    Traffic__ObservationWindow **windows_pb_tbl;
    struct flow_window **window;
    Traffic__ObservationWindow **window_pb;
    size_t i;

    TEST_ASSERT_EQUAL_UINT(report->num_windows, report_pb->n_observationwindow);

    windows_pb_tbl = set_pb_windows(report);
    window = report->flow_windows;
    window_pb = windows_pb_tbl;

    for (i = 0; i < report->num_windows; i++)
    {
        /* Validate the observation window protobuf content */
        validate_observation_window(*window, *window_pb);
        window++;
        window_pb++;
    }

    /* Free validation structure */
    for (i = 0; i < report->num_windows; i++)
    {
        free_pb_window(windows_pb_tbl[i]);
    }
    free(windows_pb_tbl);
}


/**
 * @brief validates the contents of a flow report protobuf
 *
 * @param node expected flow report info
 * @param report_pb flow report protobuf to validate
 */
static void validate_flow_report(struct flow_report *report,
                                 Traffic__FlowReport *report_pb)
{
    TEST_ASSERT_EQUAL_UINT(report->reported_at, report_pb->reportedat);
    validate_node_info(report->node_info, report_pb->observationpoint);
    validate_flow_windows(report, report_pb);
}


/**
 * @brief tests serialize_node_info() when passed a NULL pointer
 */
void test_serialize_node_info_null_ptr(void)
{
    struct node_info *node = NULL;
    struct packed_buffer *pb;

    pb = serialize_node_info(node);

    /* Basic validation */
    TEST_ASSERT_NULL(pb);
}


/**
 * @brief tests serialize_node_info() when provided an empty node info
 */
void test_serialize_node_info_no_field_set(void)
{
    struct node_info node = { 0 };
    struct packed_buffer *pb;

    /* Serialize the observation point */
    pb = serialize_node_info(&node);

    /* Basic validation */
    TEST_ASSERT_NULL(pb);
}


/**
 * @brief tests serialize_node_info() when provided a valid node info
 */
void test_serialize_node_info(void)
{
    struct node_info *node;
    struct packed_buffer *pb;
    struct packed_buffer pb_r = { 0 };
    uint8_t rbuf[4096];
    size_t nread = 0;
    Traffic__ObservationPoint *op;

    TEST_ASSERT_TRUE(g_test.initialized);

    node = g_test.report->node_info;

    /* Serialize the observation point */
    pb = serialize_node_info(node);

    /* Basic validation */
    TEST_ASSERT_NOT_NULL(pb);
    TEST_ASSERT_NOT_NULL(pb->buf);

    /* Save the serialized protobuf to file */
    pb2file(pb, g_test.f_name);

    /* Free the serialized container */
    free_packed_buffer(pb);

    /* Read back the serialized protobuf */
    pb_r.buf = rbuf;
    pb_r.len = sizeof(rbuf);
    nread = file2pb(g_test.f_name, &pb_r);
    op = traffic__observation_point__unpack(NULL, nread, rbuf);

    /* Validate the deserialized content */
    TEST_ASSERT_NOT_NULL(op);
    validate_node_info(node, op);

    /* Free the deserialized content */
    traffic__observation_point__free_unpacked(op, NULL);
}


/**
 * @brief tests serialize_flow_counters() when provided a valid counters pointer
 */
void test_serialize_flow_counters(void)
{
    struct flow_counters *counters;
    struct packed_buffer *pb;
    struct packed_buffer pb_r = { 0 };
    uint8_t rbuf[4096];
    size_t nread = 0;
    Traffic__FlowCounters *counters_pb;

    TEST_ASSERT_TRUE(g_test.initialized);

    counters = g_test.stats1->counters;

    /* Serialize the flow_counter data */
    pb = serialize_flow_counters(counters);

    /* Basic validation */
    TEST_ASSERT_NOT_NULL(pb);
    TEST_ASSERT_NOT_NULL(pb->buf);

    /* Save the serialized protobuf to file */
    pb2file(pb, g_test.f_name);

    /* Free the serialized container */
    free_packed_buffer(pb);

    /* Read back the serialized protobuf */
    pb_r.buf = rbuf;
    pb_r.len = sizeof(rbuf);
    nread = file2pb(g_test.f_name, &pb_r);
    counters_pb = traffic__flow_counters__unpack(NULL, nread, rbuf);

    /* Validate the deserialized content */
    TEST_ASSERT_NOT_NULL(counters_pb);
    validate_flow_counters(counters, counters_pb);

    /* Free the deserialized content */
    traffic__flow_counters__free_unpacked(counters_pb, NULL);
}


/**
 * @brief tests serialize_flow_key() when provided a valid key pointer
 */
void test_serialize_flow_key(void)
{
    struct flow_key *key;
    struct packed_buffer *pb;
    struct packed_buffer pb_r = { 0 };
    uint8_t rbuf[4096];
    size_t nread = 0;
    Traffic__FlowKey *key_pb;

    key = g_test.stats1->key;
    /* Serialize the flow_counter data */
    pb = serialize_flow_key(key);

    /* Basic validation */
    TEST_ASSERT_NOT_NULL(pb);
    TEST_ASSERT_NOT_NULL(pb->buf);

    /* Save the serialized protobuf to file */
    pb2file(pb, g_test.f_name);

    /* Free the serialized container */
    free_packed_buffer(pb);

    /* Read back the serialized protobuf */
    pb_r.buf = rbuf;
    pb_r.len = sizeof(rbuf);
    nread = file2pb(g_test.f_name, &pb_r);
    key_pb = traffic__flow_key__unpack(NULL, nread, rbuf);

    /* Validate the deserialized content */
    TEST_ASSERT_NOT_NULL(key_pb);
    validate_flow_key(key, key_pb);

    /* Free the deserialized content */
    traffic__flow_key__free_unpacked(key_pb, NULL);
}


/**
 * @brief tests serialize_flow_key() with optional fields not set
 */
void test_serialize_flow_key_with_optional_fields(void)
{
    struct flow_key test_key = {
        .smac = "test_smac",
        .dmac = "test_dmac",
        .vlan_id = 0,
        .ethertype = 0x8000,
        .src_ip = NULL,
        .dst_ip = NULL,
        .protocol = 0,
        .sport = 0,
        .dport = 0,
    };

    struct flow_key *key = &test_key;
    struct packed_buffer *pb;
    struct packed_buffer pb_r = { 0 };
    uint8_t rbuf[4096];
    size_t nread = 0;
    Traffic__FlowKey *key_pb;

    /* Serialize the flow_counter data */
    pb = serialize_flow_key(key);

    /* Basic validation */
    TEST_ASSERT_NOT_NULL(pb);
    TEST_ASSERT_NOT_NULL(pb->buf);

    /* Save the serialized protobuf to file */
    pb2file(pb, g_test.f_name);

    /* Free the serialized container */
    free_packed_buffer(pb);

    /* Read back the serialized protobuf */
    pb_r.buf = rbuf;
    pb_r.len = sizeof(rbuf);
    nread = file2pb(g_test.f_name, &pb_r);
    key_pb = traffic__flow_key__unpack(NULL, nread, rbuf);

    /* Validate the deserialized content */
    TEST_ASSERT_NOT_NULL(key_pb);
    validate_flow_key(key, key_pb);

    /* Free the deserialized content */
    traffic__flow_key__free_unpacked(key_pb, NULL);
}

/**
 * @brief tests serialize_flow_key() with tags
 */
void test_serialize_flow_key_with_tags(void)
{
    char *tags1[] = {
       "1_over_tcp",
       "2_over_https",
    };

    char *tags2[] = {
         "2_over_udp",
    };

    struct flow_tags flow_tags_1 = {
        .vendor = "Plume",
        .app_name = "ThePlumeApp",
        .nelems = 2,
        .tags = tags1,
    };

    struct flow_tags flow_tags_2 = {
        .vendor = "Plume",
        .app_name = "ThePlumeApp",
        .nelems = 1,
        .tags = tags2,
    };

    struct flow_tags *tags[] = {
        &flow_tags_1,
        &flow_tags_2,
    };

    struct flow_key test_key = {
        .smac = "test_smac",
        .dmac = "test_dmac",
        .vlan_id = 0,
        .ethertype = 0x8000,
        .src_ip = NULL,
        .dst_ip = NULL,
        .protocol = 0,
        .sport = 0,
        .dport = 0,
        .num_tags = 2,
        .tags = tags,
    };

    struct flow_key *key = &test_key;
    struct packed_buffer *pb;
    struct packed_buffer pb_r = { 0 };
    uint8_t rbuf[4096];
    size_t nread = 0;
    Traffic__FlowKey *key_pb;

    /* Serialize the flow_counter data */
    pb = serialize_flow_key(key);

    /* Basic validation */
    TEST_ASSERT_NOT_NULL(pb);
    TEST_ASSERT_NOT_NULL(pb->buf);

    /* Save the serialized protobuf to file */
    pb2file(pb, g_test.f_name);

    /* Free the serialized container */
    free_packed_buffer(pb);

    /* Read back the serialized protobuf */
    pb_r.buf = rbuf;
    pb_r.len = sizeof(rbuf);
    nread = file2pb(g_test.f_name, &pb_r);
    key_pb = traffic__flow_key__unpack(NULL, nread, rbuf);

    /* Validate the deserialized content */
    TEST_ASSERT_NOT_NULL(key_pb);
    validate_flow_key(key, key_pb);

    /* Free the deserialized content */
    traffic__flow_key__free_unpacked(key_pb, NULL);
}


/**
 * @brief tests serialize_flow_stats() when provided a valid stats pointer
 */
void test_flow_stats(struct flow_stats *stats)
{
    struct packed_buffer *pb;
    struct packed_buffer pb_r = { 0 };
    uint8_t rbuf[4096];
    size_t nread = 0;
    Traffic__FlowStats *stats_pb;

    /* Serialize the flow stats data */
    pb = serialize_flow_stats(stats);

    /* Basic validation */
    TEST_ASSERT_NOT_NULL(pb);
    TEST_ASSERT_NOT_NULL(pb->buf);

    /* Save the serialized protobuf to file */
    pb2file(pb, g_test.f_name);

    /* Free the serialized container */
    free_packed_buffer(pb);

    /* Read back the serialized protobuf */
    pb_r.buf = rbuf;
    pb_r.len = sizeof(rbuf);
    nread = file2pb(g_test.f_name, &pb_r);
    stats_pb = traffic__flow_stats__unpack(NULL, nread, rbuf);

    /* Validate the deserialized content */
    TEST_ASSERT_NOT_NULL(stats_pb);
    validate_flow_stats(stats, stats_pb);

    /* Free the deserialized content */
    traffic__flow_stats__free_unpacked(stats_pb, NULL);
}


/**
 * @brief tests serialize_flow_stats() when provided a valid single stats pointer
 */
void test_serialize_flow_stats(void)
{
    struct flow_stats *stats = g_test.stats1;
    test_flow_stats(stats);
}


/**
 * @brief tests serialize_flow_stats() when provided a table of stats pointers
 */
void test_set_flow_stats(void)
{
    struct flow_window *window = g_test.window2;
    Traffic__FlowStats **stats_pb_tbl;
    struct flow_stats **stats;
    Traffic__FlowStats **stats_pb;
    size_t i;

    /* Generate a table of flow stats pointers */
    stats_pb_tbl = set_pb_flow_stats(window);

    /* Basic validation */
    TEST_ASSERT_NOT_NULL(stats_pb_tbl);

    /* Validate each of the flow stats entries */
    stats = window->flow_stats;
    stats_pb = stats_pb_tbl;
    for (i = 0; i < window->num_stats; i++)
    {
        /* Validate the flow stats protobuf content */
        validate_flow_stats(*stats, *stats_pb);

        /* Free current stats entry */
        free_pb_flowstats(*stats_pb);

        stats++;
        stats_pb++;
    }

    /* Free the pointers table */
    free(stats_pb_tbl);
}


/**
 * @brief tests serialize_flow_windows() when provided a single window pointer
 */
void test_observation_window(struct flow_window *window)
{
    struct packed_buffer *pb;
    struct packed_buffer pb_r = { 0 };
    uint8_t rbuf[4096];
    size_t nread = 0;
    Traffic__ObservationWindow *window_pb;

    /* Serialize the observation window */
    pb = serialize_flow_window(window);

    /* Basic validation */
    TEST_ASSERT_NOT_NULL(pb);
    TEST_ASSERT_NOT_NULL(pb->buf);

    /* Save the serialized protobuf to file */
    pb2file(pb, g_test.f_name);

    /* Free the serialized container */
    free_packed_buffer(pb);

    /* Read back the serialized protobuf */
    pb_r.buf = rbuf;
    pb_r.len = sizeof(rbuf);
    nread = file2pb(g_test.f_name, &pb_r);
    window_pb = traffic__observation_window__unpack(NULL, nread, rbuf);

    /* Validate the deserialized content */
    TEST_ASSERT_NOT_NULL(window_pb);
    validate_observation_window(window, window_pb);

    /* Free the deserialized content */
    traffic__observation_window__free_unpacked(window_pb, NULL);
}


/**
 * @brief tests serialize_flow_window() when provided a valid  window pointer
 */
void test_serialize_observation_windows(void)
{
    struct flow_window *window = g_test.window1;
    test_observation_window(window);
}


/**
 * @brief tests serialize_flow_windows() when provided a table of window pointers
 */
void test_set_serialization_windows(void)
{
    struct flow_report *report = g_test.report;
    Traffic__ObservationWindow **windows_pb_tbl;
    struct flow_window **window;
    Traffic__ObservationWindow **window_pb;
    size_t i;

    /* Generate a table of flow winow pointers */
    windows_pb_tbl = set_pb_windows(report);

    /* Basic validation */
    TEST_ASSERT_NOT_NULL(windows_pb_tbl);

    /* Validate each of the flow stats entries */
    window = report->flow_windows;
    window_pb = windows_pb_tbl;
    for (i = 0; i < report->num_windows; i++)
    {
        /* Validate the observation window protobuf content */
        validate_observation_window(*window, *window_pb);

        /* Free current window entry */
        free_pb_window(*window_pb);

        window++;
        window_pb++;
    }

    /* Free the pointers table */
    free(windows_pb_tbl);
}


/**
 * @brief test flow tags serialization
 */
void
test_serialize_flow_tags(void)
{
    char *tags[] = {
        "over_tcp",
        "over_https",
    };

    struct flow_tags flow_tags = {
        .vendor = "Plume",
        .app_name = "ThePlumeApp",
        .nelems = 2,
        .tags = tags,
    };

    struct packed_buffer *pb;
    struct packed_buffer pb_r = { 0 };
    uint8_t rbuf[4096];
    size_t nread = 0;
    Traffic__FlowTags *pb_tags;

    pb = serialize_flow_tags(&flow_tags);
    /* Basic validation */
    TEST_ASSERT_NOT_NULL(pb);
    TEST_ASSERT_NOT_NULL(pb->buf);

    /* Save the serialized protobuf to file */
    pb2file(pb, g_test.f_name);

    /* Free the serialized container */
    free_packed_buffer(pb);

    /* Read back the serialized protobuf */
    pb_r.buf = rbuf;
    pb_r.len = sizeof(rbuf);
    nread = file2pb(g_test.f_name, &pb_r);
    pb_tags = traffic__flow_tags__unpack(NULL, nread, rbuf);
    TEST_ASSERT_NOT_NULL(pb_tags);

    /* Validate the deserialized content */
    validate_flow_tags(&flow_tags, pb_tags);

    /* Free the deserialized content */
    traffic__flow_tags__free_unpacked(pb_tags, NULL);
}


/**
 * test_serialize_flow_report: tests
 * serialize_flow_report() behavior when provided a valid node info
 */
void test_serialize_flow_report(void)
{
    struct flow_report *report = g_test.report;
    struct packed_buffer *pb;
    struct packed_buffer pb_r = { 0 };
    uint8_t rbuf[4096];
    size_t nread = 0;
    Traffic__FlowReport *pb_report;

#ifndef ARCH_X86
    qm_response_t res;
    bool ret = false;
#endif

    /* Validate the deserialized content */
    pb = serialize_flow_report(report);

#ifndef ARCH_X86
    ret = qm_conn_send_direct(QM_REQ_COMPRESS_DISABLE,
                              "network_metadata_test/4C71000027",
                              pb->buf, pb->len, &res);
    TEST_ASSERT_TRUE(ret);
#endif

    /* Basic validation */
    TEST_ASSERT_NOT_NULL(pb);
    TEST_ASSERT_NOT_NULL(pb->buf);

    /* Save the serialized protobuf to file */
    pb2file(pb, g_test.f_name);

    /* Free the serialized container */
    free_packed_buffer(pb);

    /* Read back the serialized protobuf */
    pb_r.buf = rbuf;
    pb_r.len = sizeof(rbuf);
    nread = file2pb(g_test.f_name, &pb_r);
    pb_report = traffic__flow_report__unpack(NULL, nread, rbuf);
    TEST_ASSERT_NOT_NULL(pb_report);

    /* Validate the deserialized content */
    validate_flow_report(report, pb_report);

    /* Free the deserialized content */
    traffic__flow_report__free_unpacked(pb_report, NULL);
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    target_log_open("TEST", LOG_OPEN_STDOUT);
    log_severity_set(LOG_SEVERITY_INFO);

    UnityBegin("network_metadata");

    /* Protobuf serialization testing */
    RUN_TEST(test_serialize_node_info_null_ptr);
    RUN_TEST(test_serialize_node_info_no_field_set);
    RUN_TEST(test_serialize_node_info);
    RUN_TEST(test_serialize_flow_key);
    RUN_TEST(test_serialize_flow_key_with_optional_fields);
    RUN_TEST(test_serialize_flow_counters);
    RUN_TEST(test_serialize_flow_stats);
    RUN_TEST(test_set_flow_stats);
    RUN_TEST(test_serialize_observation_windows);
    RUN_TEST(test_set_serialization_windows);
    RUN_TEST(test_serialize_flow_report);
    RUN_TEST(test_serialize_flow_tags);
    RUN_TEST(test_serialize_flow_key_with_tags);

    /* Sampling and reporting testing */
    RUN_TEST(test_str2mac);
    RUN_TEST(test_net_md_allocate_aggregator);
    RUN_TEST(test_activate_add_samples_close_send_report);
    RUN_TEST(test_add_2_samples_all_keys);
    RUN_TEST(test_ethernet_aggregate_one_key);
    RUN_TEST(test_ethernet_aggregate_two_keys);
    RUN_TEST(test_large_loop);
    RUN_TEST(test_add_remove_flows);
    RUN_TEST(test_multiple_windows);
    RUN_TEST(test_report_filter);
    RUN_TEST(test_activate_and_free_aggr);
    RUN_TEST(test_bogus_ttl);
    RUN_TEST(test_flow_tags_one_key);

    return UNITY_END();
}
