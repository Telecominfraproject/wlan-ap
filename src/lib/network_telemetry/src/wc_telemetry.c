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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "log.h"
#include "wc_telemetry.h"
#include "ip_dns_telemetry.pb-c.h"

#define MAX_STRLEN 256

/**
 * @brief Frees the pointer to serialized data and container
 *
 * Frees the dynamically allocated pointer to serialized data (pb->buf)
 * and the container (pb).
 *
 * @param pb a pointer to a serialized data container
 * @return none
 */
void
wc_free_packed_buffer(struct wc_packed_buffer *pb)
{
     if (pb == NULL) return;

     free(pb->buf);
     free(pb);
}


/**
 * @brief duplicates a string and returns true if successful
 *
 * wrapper around string duplication when the source string might be
 * a null pointer.
 *
 * @param src source string to duplicate. Might be NULL.
 * @param dst destination string pointer
 * @return true if duplicated, false otherwise
 */
static bool
wc_str_duplicate(char *src, char **dst)
{
    if (src == NULL)
    {
        *dst = NULL;
        return true;
    }

    *dst = strndup(src, MAX_STRLEN);
    if (*dst == NULL)
    {
        LOGE("%s: could not duplicate %s", __func__, src);
        return false;
    }

    return true;
}


/**
 * @brief Allocates and sets an observation point protobuf.
 *
 * Uses the node info to fill a dynamically allocated
 * observation point protobuf.
 * The caller is responsible for freeing the returned pointer,
 * @see free_pb_op() for this purpose.
 *
 * @param node info used to fill up the protobuf.
 * @return a pointer to a observation point protobuf structure
 */
static Wc__Stats__ObservationPoint *
wc_set_node_info(struct wc_observation_point *op)
{
    Wc__Stats__ObservationPoint *pb;
    bool ret;

    /* Allocate the protobuf structure */
    pb = calloc(1, sizeof(*pb));
    if (pb == NULL) return NULL;

    /* Initialize the protobuf structure */
    wc__stats__observation_point__init(pb);

    /* Set the protobuf fields */
    ret = wc_str_duplicate(op->node_id, &pb->nodeid);
    if (!ret) goto err_free_pb;

    ret = wc_str_duplicate(op->location_id, &pb->locationid);
    if (!ret) goto err_free_node_id;

    return pb;

err_free_node_id:
    free(pb->nodeid);

err_free_pb:
    free(pb);

    return NULL;
}


/**
 * @brief Free an observation point protobuf structure.
 *
 * Free dynamically allocated fields and the protobuf structure.
 *
 * @param pb observation point structure to free
 * @return none
 */
static void
wc_free_pb_op(Wc__Stats__ObservationPoint *pb)
{
    if (pb == NULL) return;

    free(pb->nodeid);
    free(pb->locationid);
    free(pb);
}


/**
 * @brief Generates an observation point serialized protobuf
 *
 * Uses the information pointed by the info parameter to generate
 * a serialized obervation point buffer.
 * The caller is responsible for freeing to the returned serialized data,
 * @see free_packed_buffer() for this purpose.
 *
 * @param op node info used to fill up the protobuf.
 * @return a pointer to the serialized data.
 */
struct wc_packed_buffer *
wc_serialize_observation_point(struct wc_observation_point *op)
{
    Wc__Stats__ObservationPoint *pb;
    struct wc_packed_buffer *serialized;
    void *buf;
    size_t len;

    if (op == NULL) return NULL;

    /* Allocate serialization output container */
    serialized = calloc(1, sizeof(*serialized));
    if (serialized == NULL) return NULL;

    /* Allocate and set observation point protobuf */
    pb = wc_set_node_info(op);
    if (pb == NULL) goto err_free_serialized;

    /* Get serialization length */
    len = wc__stats__observation_point__get_packed_size(pb);
    if (len == 0) goto err_free_pb;

    /* Allocate space for the serialized buffer */
    buf = malloc(len);
    if (buf == NULL) goto err_free_pb;

    /* Serialize protobuf */
    serialized->len = wc__stats__observation_point__pack(pb, buf);
    serialized->buf = buf;

    /* Free the protobuf structure */
    wc_free_pb_op(pb);

    /* Return serialized content */
    return serialized;

err_free_pb:
    wc_free_pb_op(pb);

err_free_serialized:
    free(serialized);

    return NULL;
}

/**
 * @brief Allocates and sets an observation window protobuf.
 *
 * Uses the observation window to fill a dynamically allocated
 * observation window protobuf.
 * The caller is responsible for freeing the returned pointer,
 * @see wc_free_pb_ow() for this purpose.
 *
 * @param observation window used to fill up the protobuf.
 * @return a pointer to a observation point protobuf structure
 */
static Wc__Stats__ObservationWindow *
wc_set_ow(struct wc_observation_window *ow)
{
    Wc__Stats__ObservationWindow *pb;

    /* Allocate the protobuf structure */
    pb = calloc(1, sizeof(*pb));
    if (pb == NULL) return NULL;

    /* Initialize the protobuf structure */
    wc__stats__observation_window__init(pb);

    /* Set the protobuf fields */
    pb->has_startedat = true;
    pb->startedat = ow->started_at;

    pb->has_endedat = true;
    pb->endedat = ow->ended_at;

    return pb;
}


/**
 * @brief Free an observation window protobuf structure.
 *
 * Free dynamically allocated fields and the protobuf structure.
 *
 * @param pb observation window structure to free
 * @return none
 */
static void
wc_free_pb_ow(Wc__Stats__ObservationWindow *pb)
{
    free(pb);
}


/**
 * @brief Generates an observation point serialized protobuf
 *
 * Uses the information pointed by the info parameter to generate
 * a serialized obervation point buffer.
 * The caller is responsible for freeing to the returned serialized data,
 * @see free_packed_buffer() for this purpose.
 *
 * @param op node info used to fill up the protobuf.
 * @return a pointer to the serialized data.
 */
struct wc_packed_buffer *
wc_serialize_observation_window(struct wc_observation_window *ow)
{
    Wc__Stats__ObservationWindow *pb;
    struct wc_packed_buffer *serialized;
    void *buf;
    size_t len;

    if (ow == NULL) return NULL;

    /* Allocate serialization output container */
    serialized = calloc(1, sizeof(*serialized));
    if (serialized == NULL) return NULL;

    /* Allocate and set observation point protobuf */
    pb = wc_set_ow(ow);
    if (pb == NULL) goto err_free_serialized;

    /* Get serialization length */
    len = wc__stats__observation_window__get_packed_size(pb);
    if (len == 0) goto err_free_pb;

    /* Allocate space for the serialized buffer */
    buf = malloc(len);
    if (buf == NULL) goto err_free_pb;

    /* Serialize protobuf */
    serialized->len = wc__stats__observation_window__pack(pb, buf);
    serialized->buf = buf;

    /* Free the protobuf structure */
    wc_free_pb_ow(pb);

    /* Return serialized content */
    return serialized;

err_free_pb:
    wc_free_pb_ow(pb);

err_free_serialized:
    free(serialized);

    return NULL;
}


/**
 * @brief Allocates and sets a wc health stats protobuf.
 *
 * Uses the health stats info to fill a dynamically allocated
 * wc health stats protobuf.
 * The caller is responsible for freeing the returned pointer,
 * @see wc_free_pb_health_stats() for this purpose.
 *
 * @param health stats info used to fill up the protobuf
 * @return a pointer to health stats protobuf structure
 */
static Wc__Stats__WCHealthStats *
wc_set_health_stats(struct wc_health_stats *hs)
{
    Wc__Stats__WCHealthStats *pb;

    /* Allocate the protobuf structure */
    pb = calloc(1, sizeof(*pb));
    if (pb == NULL) return NULL;

    /* Initialize the protobuf structure */
    wc__stats__wchealth_stats__init(pb);

    /* Set the protobuf fields */
    pb->has_totallookups = true;
    pb->totallookups = hs->total_lookups;

    pb->has_cachehits = true;
    pb->cachehits = hs->cache_hits;

    pb->has_remotelookups = true;
    pb->remotelookups = hs->remote_lookups;

    pb->has_connectivityfailures = true;
    pb->connectivityfailures = hs->connectivity_failures;

    pb->has_servicefailures = true;
    pb->servicefailures = hs->service_failures;

    pb->has_uncategorized = true;
    pb->uncategorized = hs->uncategorized;

    pb->has_minlatency = true;
    pb->minlatency = hs->min_latency;

    pb->has_maxlatency = true;
    pb->maxlatency = hs->max_latency;

    pb->has_averagelatency = true;
    pb->averagelatency = hs->avg_latency;

    pb->has_cachedentries = true;
    pb->cachedentries = hs->cached_entries;

    pb->has_cachesize = true;
    pb->cachesize = hs->cache_size;

    return pb;
}


/**
 * @brief Free a wc health stats protobuf structure.
 *
 * Free dynamically allocated fields and the protobuf structure.
 *
 * @param pb wc health stats structure to free
 * @return none
 */
static void
wc_free_pb_health_stats(Wc__Stats__WCHealthStats *pb)
{
    free(pb);
}


/**
 * @brief Generates a health stats serialized protobuf
 *
 * Uses the information pointed by the input parameter to generate
 * a serialized health stats buffer.
 * The caller is responsible for freeing to the returned serialized data,
 * @see free_packed_buffer() for this purpose.
 *
 * @param helthstate used to fill up the protobuf.
 * @return a pointer to the serialized data.
 */
struct wc_packed_buffer *
wc_serialize_health_stats(struct wc_health_stats *hs)
{
    Wc__Stats__WCHealthStats *pb;
    struct wc_packed_buffer *serialized;
    void *buf;
    size_t len;

    if (hs == NULL) return NULL;

    /* Allocate serialization output container */
    serialized = calloc(1, sizeof(*serialized));
    if (serialized == NULL) return NULL;

    /* Allocate and set a health stats counters protobuf */
    pb = wc_set_health_stats(hs);
    if (pb == NULL) goto err_free_serialized;

    /* Get serialization length */
    len = wc__stats__wchealth_stats__get_packed_size(pb);
    if (len == 0) goto err_free_pb;

    /* Allocate space for the serialized buffer */
    buf = malloc(len);
    if (buf == NULL) goto err_free_pb;

    /* Serialize protobuf */
    serialized->len = wc__stats__wchealth_stats__pack(pb, buf);
    serialized->buf = buf;

    /* Free the protobuf structure */
    wc_free_pb_health_stats(pb);

    /* Return serialized content */
    return serialized;

err_free_pb:
    wc_free_pb_health_stats(pb);

err_free_serialized:
    free(serialized);

    return NULL;
}


/**
 * @brief Allocates and sets a web classification report protobuf.
 *
 * Uses the report info to fill a dynamically allocated
 * wc health stats report protobuf.
 * The caller is responsible for freeing the returned pointer,
 * @see wc_free_pb_report() for this purpose.
 *
 * @param node info used to fill up the protobuf
 * @return a pointer to a observation point protobuf structure
 */
static Wc__Stats__WCStatsReport *
wc_set_pb_report(struct wc_stats_report *report)
{
    Wc__Stats__WCStatsReport *pb;

    /* Allocate protobuf */
    pb  = calloc(1, sizeof(*pb));
    if (pb == NULL) return NULL;

    /* Initialize protobuf */
    wc__stats__wcstats_report__init(pb);

    /* Set provider */
    pb->wcprovider = report->provider;

    /* Set protobuf fields */
    pb->observationpoint = wc_set_node_info(report->op);
    if (pb->observationpoint == NULL) goto err_free_pb_report;

    /* Allocate observation windows container */
    pb->observationwindow = wc_set_ow(report->ow);
    if (pb->observationwindow == NULL) goto err_free_pb_op;

    /* Allocate wc health stats container */
    pb->wchealthstats = wc_set_health_stats(report->health_stats);
    if (pb->wchealthstats == NULL) goto err_free_ow;
    pb->n_wcherostats = 0;

    return pb;

err_free_ow:
    wc_free_pb_ow(pb->observationwindow);

err_free_pb_op:
    wc_free_pb_op(pb->observationpoint);

err_free_pb_report:
    free(pb);

    return NULL;
}


/**
 * @brief Free a web classification report protobuf structure.
 *
 * Free dynamically allocated fields and the protobuf structure.
 *
 * @param pb web cliassification report structure to free
 * @return none
 */
static void
wc_free_pb_report(Wc__Stats__WCStatsReport *pb)
{
    if (pb == NULL) return;

    wc_free_pb_op(pb->observationpoint);
    wc_free_pb_ow(pb->observationwindow);
    wc_free_pb_health_stats(pb->wchealthstats);
    free(pb);
}

/**
 * @brief Generates a web classification report serialized protobuf
 *
 * Uses the information pointed by the report parameter to generate
 * a serialized web classification report buffer.
 * The caller is responsible for freeing to the returned serialized data,
 * @see free_packed_buffer() for this purpose.
 *
 * @param web classification container used to fill up the protobuf.
 * @return a pointer to the serialized data.
 */
struct wc_packed_buffer *
wc_serialize_wc_stats_report(struct wc_stats_report *report)
{
    struct wc_packed_buffer *serialized;
    Wc__Stats__WCStatsReport *pb;
    size_t len;
    void *buf;

    if (report == NULL) return NULL;

    /* Allocate serialization output structure */
    serialized = calloc(1, sizeof(*serialized));
    if (serialized == NULL) return NULL;

    /* Allocate and set web classification report protobuf */
    pb = wc_set_pb_report(report);
    if (pb == NULL) goto err_free_serialized;

    /* Get serialization length */
    len = wc__stats__wcstats_report__get_packed_size(pb);
    if (len == 0) goto err_free_pb;

    /* Allocate space for the serialized buffer */
    buf = malloc(len);
    if (buf == NULL) goto err_free_pb;

    serialized->len = wc__stats__wcstats_report__pack(pb, buf);
    serialized->buf = buf;

    /* Free the protobuf structure */
    wc_free_pb_report(pb);

    return serialized;

err_free_pb:
    free(pb);

err_free_serialized:
    free(serialized);

    return NULL;
}
