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

#include "wc_telemetry.h"
#include "ip_dns_telemetry.pb-c.h"
#include "unity.h"
#include "log.h"
#include "target.h"
#include "qm_conn.h"


static const char *g_fname = "/tmp/wc_serial_proto";

static struct wc_observation_point g_op =
{
    .location_id = "network_telemetry_test_location_id",
    .node_id = "4C71000027",
};


static struct wc_observation_window g_ow =
{
    .started_at = 10000,
    .ended_at = 10000,
};


static struct wc_health_stats g_hs =
{
    .total_lookups = 100,
    .cache_hits = 90,
    .remote_lookups = 10,
    .connectivity_failures = 0,
    .service_failures = 1,
    .uncategorized = 2,
    .min_latency = 3,
    .max_latency = 20,
    .avg_latency = 7,
    .cached_entries = 4,
    .cache_size = 20000,
};

static struct wc_stats_report g_report =
{
    .provider = "ut_wc_telemetry_provider",
    .op = &g_op,
    .ow = &g_ow,
    .health_stats = &g_hs,
};

/**
 * @brief See unity documentation/exmaples
 */
void
setUp(void) {}


/**
 * @brief See unity documentation/exmaples
 */
void
tearDown(void) {}


/**
 * @brief writes the contents of a serialized buffer in a file
 *
 * @param pb serialized buffer to be written
 * @param fpath target file path
 *
 * @return returns the number of bytes written
 */
static size_t
pb2file(struct wc_packed_buffer *pb, const char *fpath)
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
static size_t
file2pb(const char *fpath, struct wc_packed_buffer *pb)
{
    FILE *f = fopen(fpath, "rb");
    size_t nread = fread(pb->buf, 1, pb->len, f);
    fclose(f);

    return nread;
}


/**
 * @brief validates the contents of an observation point protobuf
 *
 * @param wc_op expected node info
 * @param op observation point protobuf to validate
 */
static void
wc_validate_op(struct wc_observation_point *wc_op,
               Wc__Stats__ObservationPoint *op)
{
    TEST_ASSERT_EQUAL_STRING(wc_op->node_id, op->nodeid);
    TEST_ASSERT_EQUAL_STRING(wc_op->location_id, op->locationid);
}


/**
 * @brief validates the contents of an observation window protobuf
 *
 * @param wc_ow expected observation window info
 * @param op observation window protobuf to validate
 */
static void
wc_validate_ow(struct wc_observation_window *wc_ow,
               Wc__Stats__ObservationWindow *ow)
{
    TEST_ASSERT_EQUAL_UINT(wc_ow->started_at, ow->startedat);
    TEST_ASSERT_EQUAL_UINT(wc_ow->ended_at, ow->endedat);
}


/**
 * @brief validates the integer field of a protobuf
 *
 * If the value to validate is 0, expect the presence boolean to be false.
 * @param node expected node info
 * @param op observation point protobuf to validate
 */
static void
validate_uint32_field(uint32_t expected, uint32_t given,
                      protobuf_c_boolean present)
{
    TEST_ASSERT_EQUAL_UINT(expected, given);
    TEST_ASSERT_EQUAL_INT(true, present);
}


/**
 * @brief validates the contents of a health stats protobuf
 *
 * @param wc_hs expected health stats info
 * @param hs health stats protobuf to validate
 */
static void
wc_validate_hs(struct wc_health_stats *wc_hs,
               Wc__Stats__WCHealthStats *hs)
{
    validate_uint32_field(wc_hs->total_lookups, hs->totallookups,
                          hs->has_totallookups);
    validate_uint32_field(wc_hs->cache_hits, hs->cachehits,
                          hs->has_cachehits);
    validate_uint32_field(wc_hs->remote_lookups, hs->remotelookups,
                          hs->has_remotelookups);
    validate_uint32_field(wc_hs->connectivity_failures, hs->connectivityfailures,
                          hs->has_connectivityfailures);
    validate_uint32_field(wc_hs->service_failures, hs->servicefailures,
                          hs->has_servicefailures);
    validate_uint32_field(wc_hs->uncategorized, hs->uncategorized,
                          hs->has_uncategorized);
    validate_uint32_field(wc_hs->min_latency, hs->minlatency,
                          hs->has_minlatency);
    validate_uint32_field(wc_hs->max_latency, hs->maxlatency,
                          hs->has_maxlatency);
    validate_uint32_field(wc_hs->avg_latency, hs->averagelatency,
                          hs->has_averagelatency);
    validate_uint32_field(wc_hs->cached_entries, hs->cachedentries,
                          hs->has_cachedentries);
    validate_uint32_field(wc_hs->cache_size, hs->cachesize,
                          hs->has_cachesize);
}


/**
 * @brief validates the contents of a web classification report
 *
 * @param wc_report expected web classification report
 * @param pb_report web classification protobuf to validate
 */
static void
wc_validate_report(struct wc_stats_report *wc_report,
                   Wc__Stats__WCStatsReport *pb_report)
{
    TEST_ASSERT_EQUAL_STRING(wc_report->provider, pb_report->wcprovider);
    wc_validate_op(wc_report->op, pb_report->observationpoint);
    wc_validate_ow(wc_report->ow, pb_report->observationwindow);
    wc_validate_hs(wc_report->health_stats, pb_report->wchealthstats);
}


/**
 * @brief tests observation point serialization when passed a NULL pointer
 */
void
test_serialize_op_null_ptr(void)
{
    struct wc_observation_point *node = NULL;
    struct wc_packed_buffer *pb;

    pb = wc_serialize_observation_point(node);

    /* Basic validation */
    TEST_ASSERT_NULL(pb);
}


/**
 * @brief tests observation point serialization when provided an empty node info
 */
void
test_serialize_op_no_field_set(void)
{
    struct wc_observation_point node = { 0 };
    struct wc_packed_buffer *pb;

    /* Serialize the observation point */
    pb = wc_serialize_observation_point(&node);

    /* Basic validation */
    TEST_ASSERT_NULL(pb);
}


/**
 * @brief tests observation point serialization when provided a valid node info
 */
void
test_serialize_op(void)
{
    struct wc_observation_point *node;
    struct wc_packed_buffer *pb;
    struct wc_packed_buffer pb_r = { 0 };
    uint8_t rbuf[4096];
    size_t nread = 0;
    Wc__Stats__ObservationPoint *op;

    node = &g_op;

    /* Serialize the observation point */
    pb = wc_serialize_observation_point(node);

    /* Basic validation */
    TEST_ASSERT_NOT_NULL(pb);
    TEST_ASSERT_NOT_NULL(pb->buf);

    /* Save the serialized protobuf to file */
    pb2file(pb, g_fname);

    /* Free the serialized container */
    wc_free_packed_buffer(pb);

    /* Read back the serialized protobuf */
    pb_r.buf = rbuf;
    pb_r.len = sizeof(rbuf);
    nread = file2pb(g_fname, &pb_r);
    op = wc__stats__observation_point__unpack(NULL, nread, rbuf);

    /* Validate the deserialized content */
    TEST_ASSERT_NOT_NULL(op);
    wc_validate_op(node, op);

    /* Free the deserialized content */
    wc__stats__observation_point__free_unpacked(op, NULL);
}


/**
 * @brief tests observation window serialization
 */
void
test_serialize_ow(void)
{
    struct wc_observation_window *wc_ow;
    struct wc_packed_buffer *pb;
    struct wc_packed_buffer pb_r = { 0 };
    uint8_t rbuf[4096];
    size_t nread = 0;
    Wc__Stats__ObservationWindow *ow;

    wc_ow = &g_ow;

    /* Serialize the observation point */
    pb = wc_serialize_observation_window(wc_ow);

    /* Basic validation */
    TEST_ASSERT_NOT_NULL(pb);
    TEST_ASSERT_NOT_NULL(pb->buf);

    /* Save the serialized protobuf to file */
    pb2file(pb, g_fname);

    /* Free the serialized container */
    wc_free_packed_buffer(pb);

    /* Read back the serialized protobuf */
    pb_r.buf = rbuf;
    pb_r.len = sizeof(rbuf);
    nread = file2pb(g_fname, &pb_r);
    ow = wc__stats__observation_window__unpack(NULL, nread, rbuf);

    /* Validate the deserialized content */
    TEST_ASSERT_NOT_NULL(ow);
    wc_validate_ow(wc_ow, ow);

    /* Free the deserialized content */
    wc__stats__observation_window__free_unpacked(ow, NULL);
}

/**
 * @brief tests health stats serialization
 */
void
test_serialize_hs(void)
{
    struct wc_health_stats *wc_hs;
    struct wc_packed_buffer *pb;
    struct wc_packed_buffer pb_r = { 0 };
    uint8_t rbuf[4096];
    size_t nread = 0;
    Wc__Stats__WCHealthStats *hs;

    wc_hs = &g_hs;

    /* Serialize the observation point */
    pb = wc_serialize_health_stats(wc_hs);

    /* Basic validation */
    TEST_ASSERT_NOT_NULL(pb);
    TEST_ASSERT_NOT_NULL(pb->buf);

    /* Save the serialized protobuf to file */
    pb2file(pb, g_fname);

    /* Free the serialized container */
    wc_free_packed_buffer(pb);

    /* Read back the serialized protobuf */
    pb_r.buf = rbuf;
    pb_r.len = sizeof(rbuf);
    nread = file2pb(g_fname, &pb_r);
    hs = wc__stats__wchealth_stats__unpack(NULL, nread, rbuf);

    /* Validate the deserialized content */
    TEST_ASSERT_NOT_NULL(hs);
    wc_validate_hs(wc_hs, hs);

    /* Free the deserialized content */
    wc__stats__wchealth_stats__free_unpacked(hs, NULL);
}


void
test_serialize_hs_report(void)
{
    struct wc_stats_report *report;
    struct wc_packed_buffer *pb;
    struct wc_packed_buffer pb_r = { 0 };
    uint8_t rbuf[4096];
    size_t nread = 0;
    Wc__Stats__WCStatsReport *pb_report;

#ifndef ARCH_X86
    qm_response_t res;
    bool ret = false;
#endif

    report = &g_report;

    /* Validate the deserialized content */
    pb = wc_serialize_wc_stats_report(report);

#ifndef ARCH_X86
    ret = qm_conn_send_direct(QM_REQ_COMPRESS_DISABLE,
                              "dev-test/network_telemetry_ut/4C71000027",
                              pb->buf, pb->len, &res);
    TEST_ASSERT_TRUE(ret);
#endif

    /* Basic validation */
    TEST_ASSERT_NOT_NULL(pb);
    TEST_ASSERT_NOT_NULL(pb->buf);

    /* Save the serialized protobuf to file */
    pb2file(pb, g_fname);

    /* Free the serialized container */
    wc_free_packed_buffer(pb);

    /* Read back the serialized protobuf */
    pb_r.buf = rbuf;
    pb_r.len = sizeof(rbuf);
    nread = file2pb(g_fname, &pb_r);
    pb_report = wc__stats__wcstats_report__unpack(NULL, nread, rbuf);
    TEST_ASSERT_NOT_NULL(pb_report);

    /* Validate the deserialized content */
    wc_validate_report(report, pb_report);

    /* Free the deserialized content */
    wc__stats__wcstats_report__free_unpacked(pb_report, NULL);
}

int
main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    target_log_open("TEST", LOG_OPEN_STDOUT);
    log_severity_set(LOG_SEVERITY_INFO);

    UnityBegin("network_telemetry");

    /* Protobuf serialization testing */
    RUN_TEST(test_serialize_op_null_ptr);
    RUN_TEST(test_serialize_op_no_field_set);
    RUN_TEST(test_serialize_op);
    RUN_TEST(test_serialize_ow);
    RUN_TEST(test_serialize_hs);
    RUN_TEST(test_serialize_hs_report);

    return UNITY_END();
}
