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

#ifndef __WC_TELEMETRY_H__
#define __WC_TELEMETRY_H__

#include <stdint.h>

#include "ip_dns_telemetry.pb-c.h"

/**
 * @brief container of information needed to set an observation point protobuf.
 *
 * Stashes a pod's serial number and deployment location id.
 */
struct wc_observation_point
{
    char *node_id;     /*!< pod's serial number */
    char *location_id; /*!< pod's deployment location id */
};


/**
 * @brief start/end info container
 *
 * Stashes the sart and end times of an observation window
 */
struct wc_observation_window
{
    uint64_t started_at;             /*!< time window start (epoch) */
    uint64_t ended_at;               /*!< time window end (epoch) */
};


/**
 * @brief Web classification health stats container
 */
struct wc_health_stats
{
    uint32_t total_lookups;
    uint32_t cache_hits;
    uint32_t remote_lookups;
    uint32_t connectivity_failures;
    uint32_t service_failures;
    uint32_t uncategorized;
    uint32_t min_latency;
    uint32_t max_latency;
    uint32_t avg_latency;
    uint32_t cached_entries;
    uint32_t cache_size;
};


/**
 * @brief Hero risk stats container
 */
struct wc_risk_stats
{
    int32_t risk;
    uint32_t total_hits;
};


/**
 * @brief Hero category stats container
 */
struct wc_category_stats
{
    int32_t category_id;
    size_t num_risk_stats;
    struct wc_risk_stats **risk_stats;
};


/**
 * @brief Hero rules stats container
 */
struct wc_rules_stats
{
    char *policy_name;
    char *rule_name;
    size_t num_category_stats;
    struct wc_category_stats **cat_stats;
};

/**
 * @brief Web classification Hero stats
 */
struct wc_hero_stats
{
    char *device_id;
    size_t num_rules_stats;
    struct wc_rules_stats **rule_stats;
};


/**
 * @brief Web classification stats container
 */
struct wc_stats_report
{
    char *provider;
    struct wc_observation_point *op;
    struct wc_observation_window *ow;
    struct wc_health_stats *health_stats;
    size_t num_hero_stats;
    struct wc_hero_stats **hero_stats;
};


/**
 * @brief Container of protobuf serialization output
 *
 * Contains the information related to a serialized protobuf
 */
struct wc_packed_buffer
{
    size_t len; /*<! length of the serialized protobuf */
    void *buf;  /*<! dynamically allocated pointer to serialized data */
};


/**
 * @brief Frees the pointer to serialized data and container
 *
 * Frees the dynamically allocated pointer to serialized data (pb->buf)
 * and the container (pb)
 *
 * @param pb a pointer to a serialized data container
 * @return none
 */
void
wc_free_packed_buffer(struct wc_packed_buffer *pb);


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
wc_serialize_observation_point(struct wc_observation_point *op);


/**
 * @brief Generates an observation window serialized protobuf
 *
 * Uses the information pointed by the inpu parameter to generate
 * a serialized obervation window buffer.
 * The caller is responsible for freeing to the returned serialized data,
 * @see free_packed_buffer() for this purpose.
 *
 * @param ow observation window used to fill up the protobuf.
 * @return a pointer to the serialized data.
 */
struct wc_packed_buffer *
wc_serialize_observation_window(struct wc_observation_window *ow);


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
wc_serialize_health_stats(struct wc_health_stats *hs);


/**
 * @brief Generates a web classification telemetry serialized protobuf
 *
 * Uses the information pointed by the report parameter to generate
 * a serialized web classification telemetry buffer.
 * The caller is responsible for freeing to the returned serialized data,
 * @see free_packed_buffer() for this purpose.
 *
 * @param node info used to fill up the protobuf.
 * @return a pointer to the serialized data.
 */
struct wc_packed_buffer *
wc_serialize_wc_stats_report(struct wc_stats_report *report);

#endif /* __WC_TELEMETRY_H__ */
