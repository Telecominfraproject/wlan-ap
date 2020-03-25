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
#include "network_metadata.h"
#include "network_metadata.pb-c.h"

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
void free_packed_buffer(struct packed_buffer *pb)
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
static bool str_duplicate(char *src, char **dst)
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
 * @brief set a protobuf integer field when its value is not zero.
 *
 * If the value to set is 0, the field is marked as non present.
 *
 * @param src source value.
 * @param dst destination value pointer
 * @param present destination presence pointer
 * @return none
 */
static void set_uint32(uint32_t src, uint32_t *dst,
                       protobuf_c_boolean *present)
{

    *present = false;
    if (src == 0) return;

    *dst = src;
    *present = true;

    return;
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
static Traffic__ObservationPoint * set_node_info(struct node_info *node)
{
    Traffic__ObservationPoint *pb;
    bool ret;

    /* Allocate the protobuf structure */
    pb = calloc(sizeof(Traffic__ObservationPoint), 1);
    if (pb == NULL) return NULL;

    /* Initialize the protobuf structure */
    traffic__observation_point__init(pb);

    /* Set the protobuf fields */
    ret = str_duplicate(node->node_id, &pb->nodeid);
    if (!ret) goto err_free_pb;

    ret = str_duplicate(node->location_id, &pb->locationid);
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
static void free_pb_op(Traffic__ObservationPoint *pb)
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
 * @param node info used to fill up the protobuf
 * @return a pointer to the serialized data.
 */
struct packed_buffer * serialize_node_info(struct node_info *node)
{
    Traffic__ObservationPoint *pb;
    struct packed_buffer *serialized;
    void *buf;
    size_t len;

    if (node == NULL) return NULL;

    /* Allocate serialization output container */
    serialized = calloc(sizeof(struct packed_buffer), 1);
    if (serialized == NULL) return NULL;

    /* Allocate and set observation point protobuf */
    pb = set_node_info(node);
    if (pb == NULL) goto err_free_serialized;

    /* Get serialization length */
    len = traffic__observation_point__get_packed_size(pb);
    if (len == 0) goto err_free_pb;

    /* Allocate space for the serialized buffer */
    buf = malloc(len);
    if (buf == NULL) goto err_free_pb;

    /* Serialize protobuf */
    serialized->len = traffic__observation_point__pack(pb, buf);
    serialized->buf = buf;

    /* Free the protobuf structure */
    free_pb_op(pb);

    /* Return serialized content */
    return serialized;

err_free_pb:
    free_pb_op(pb);

err_free_serialized:
    free(serialized);

    return NULL;
}


/**
 * @brief Generates a flow tags serialized protobuf
 *
 * Uses the information pointed by the flow_tags parameter to generate
 * a serialized flow tags buffer.
 * The caller is responsible for freeing to the returned serialized data,
 * @see free_packed_buffer() for this purpose.
 *
 * @param counters info used to fill up the protobuf.
 * @return a pointer to the serialized data.
 */
struct packed_buffer *
serialize_flow_tags(struct flow_tags *flow_tags)
{
    Traffic__FlowTags *pb;
    struct packed_buffer *serialized;
    void *buf;
    size_t len;

    if (flow_tags == NULL) return NULL;

    /* Allocate serialization output container */
    serialized = calloc(1, sizeof(struct packed_buffer));
    if (serialized == NULL) return NULL;

    /* Allocate and set flow stats protobuf */
    pb = set_flow_tags(flow_tags);
    if (pb == NULL) goto err_free_serialized;

    /* Get serialization length */
    len = traffic__flow_tags__get_packed_size(pb);
    if (len == 0) goto err_free_pb;

    /* Allocate space for the serialized buffer */
    buf = malloc(len);
    if (buf == NULL) goto err_free_pb;

    /* Serialize protobuf */
    serialized->len = traffic__flow_tags__pack(pb, buf);
    serialized->buf = buf;

    /* Free the protobuf structure */
    free_pb_flow_tags(pb);

    /* Return serialized content */
    return serialized;

err_free_pb:
    free_pb_flow_tags(pb);

err_free_serialized:
    free(serialized);

    return NULL;
}


/**
 * @brief Allocates and sets a flow tags protobuf.
 *
 * Uses the flow tags info to fill a dynamically allocated
 * flow key protobuf.
 * The caller is responsible for freeing the returned pointer,
 * @see free_pb_flow_tags() for this purpose.
 *
 * @param flow_tags info used to fill up the protobuf
 * @return a pointer to a flow tags protobuf structure
 */
Traffic__FlowTags *
set_flow_tags(struct flow_tags *flow_tags)
{
    Traffic__FlowTags *pb;
    size_t allocated;
    size_t nelems;
    char **pb_tag;
    char **tags;
    char **ftag;
    size_t i;
    bool ret;

    if (flow_tags == NULL) return NULL;

    /* Allocate the protobuf structure */
    pb = calloc(1, sizeof(Traffic__FlowTags));
    if (pb == NULL) return NULL;

    /* Initialize the protobuf structure */
    traffic__flow_tags__init(pb);

    /* Set the protobuf fields */
    ret = str_duplicate(flow_tags->vendor, &pb->vendor);
    if (!ret) goto err_free_pb;

    ret = str_duplicate(flow_tags->app_name, &pb->appname);
    if (!ret) goto err_free_vendor;

    /* Done if there are no tags */
    nelems = flow_tags->nelems;
    if (nelems == 0) return pb;

    tags = calloc(nelems, sizeof(*tags));
    if (tags == NULL) goto err_free_app;

    pb->apptags = tags;
    pb_tag = tags;
    ftag = flow_tags->tags;
    allocated = 0;
    for (i = 0; i < nelems; i++)
    {
        ret = str_duplicate(*ftag, pb_tag);
        if (!ret) goto err_free_tags;

        allocated++;
        pb_tag++;
        ftag++;
    }
    pb->n_apptags = nelems;

    return pb;

err_free_tags:
    pb_tag = pb->apptags;
    for (i = 0; i < allocated; i++)
    {
        free(*pb_tag);
        pb_tag++;
    }
    free(tags);

err_free_app:
    free(pb->appname);

err_free_vendor:
    free(pb->vendor);

err_free_pb:
    free(pb);

    return NULL;
}


/**
 * @brief Free a flow tag protobuf structure.
 *
 * Free dynamically allocated fields and the protobuf structure.
 *
 * @param pb flow tag structure to free
 * @return none
 */
void free_pb_flow_tags(Traffic__FlowTags *pb)
{
    char **pb_tag;
    size_t i;

    if (pb == NULL) return;

    free(pb->vendor);
    free(pb->appname);

    pb_tag = pb->apptags;
    for (i = 0; i < pb->n_apptags; i++)
    {
        free(*pb_tag);
        pb_tag++;
    }
    free(pb->apptags);
    free(pb);
}

/**
 * @brief Allocates and sets a table of tags protobufs
 *
 * Uses the key info to fill a dynamically allocated
 * table of flow stats protobufs.
 * The caller is responsible for freeing the returned pointer
 *
 * @param key info used to fill up the protobuf table
 * @return a flow tags protobuf pointers table
 */
Traffic__FlowTags
**set_pb_flow_tags(struct flow_key *key)
{
    Traffic__FlowTags **tags_pb_tbl;
    Traffic__FlowTags **tags_pb;
    struct flow_tags **tags;
    size_t i, allocated;

    if (key == NULL) return NULL;

    if (key->num_tags == 0) return NULL;

    /* Allocate the array of flow stats */
    tags_pb_tbl = calloc(key->num_tags, sizeof(Traffic__FlowTags *));
    if (tags_pb_tbl == NULL) return NULL;

    /* Set each of the stats protobuf */
    tags = key->tags;
    tags_pb = tags_pb_tbl;
    allocated = 0;
    for (i = 0; i < key->num_tags; i++)
    {
        *tags_pb = set_flow_tags(*tags);
        if (*tags_pb == NULL) goto err_free_pb_tags;

        allocated++;
        tags++;
        tags_pb++;
    }

    return tags_pb_tbl;

err_free_pb_tags:
    tags_pb = tags_pb_tbl;
    for (i = 0; i < allocated; i++)
    {
        free_pb_flow_tags(*tags_pb);
        tags_pb++;
    }
    free(tags_pb_tbl);

    return NULL;
}


/**
 * @brief Allocates and sets a flow key protobuf.
 *
 * Uses the key info to fill a dynamically allocated
 * flow key protobuf.
 * The caller is responsible for freeing the returned pointer,
 * @see free_pb_flowkey() for this purpose.
 *
 * @param key info used to fill up the protobuf
 * @return a pointer to a flow key protobuf structure
 */
static Traffic__FlowKey *set_flow_key(struct flow_key *key)
{
    Traffic__FlowKey *pb;
    bool ret;

    if (key == NULL) return NULL;

    /* Allocate the protobuf structure */
    pb = calloc(1, sizeof(Traffic__FlowKey));
    if (pb == NULL) return NULL;

    /* Initialize the protobuf structure */
    traffic__flow_key__init(pb);

    /* Set the protobuf fields */
    ret = str_duplicate(key->smac, &pb->srcmac);
    if (!ret) goto err_free_pb;

    ret = str_duplicate(key->dmac, &pb->dstmac);
    if (!ret) goto err_free_srcmac;

    ret = str_duplicate(key->src_ip, &pb->srcip);
    if (!ret) goto err_free_dstmac;

    ret = str_duplicate(key->dst_ip, &pb->dstip);
    if (!ret) goto err_free_srcip;

    set_uint32(key->vlan_id, &pb->vlanid, &pb->has_vlanid);
    set_uint32((uint32_t)key->ethertype, &pb->ethertype, &pb->has_ethertype);
    set_uint32((uint32_t)key->protocol, &pb->ipprotocol, &pb->has_ipprotocol);
    set_uint32((uint32_t)key->sport, &pb->tptsrcport, &pb->has_tptsrcport);
    set_uint32((uint32_t)key->dport, &pb->tptdstport, &pb->has_tptdstport);

    if (key->num_tags == 0) return pb;

    /* Add the flow tags */
    pb->flowtags = set_pb_flow_tags(key);
    if (pb->flowtags == NULL) goto err_free_dstip;

    pb->n_flowtags = key->num_tags;

    return pb;

err_free_dstip:
    free(pb->dstip);

err_free_srcip:
    free(pb->srcip);

err_free_dstmac:
    free(pb->dstmac);

err_free_srcmac:
    free(pb->srcmac);

err_free_pb:
    free(pb);

    return NULL;
}


/**
 * @brief Free a flow key protobuf structure.
 *
 * Free dynamically allocated fields and the protobuf structure.
 *
 * @param pb flow key structure to free
 * @return none
 */
static void free_pb_flowkey(Traffic__FlowKey *pb)
{
    size_t i;
    if (pb == NULL) return;

    free(pb->srcmac);
    free(pb->dstmac);
    free(pb->srcip);
    free(pb->dstip);

    for (i = 0; i < pb->n_flowtags; i++)
    {
        free_pb_flow_tags(pb->flowtags[i]);
    }
    free(pb->flowtags);
    free(pb);
}


/**
 * @brief Generates a flow_key serialized protobuf.
 *
 * Uses the information pointed by the key parameter to generate
 * a serialized flow key buffer.
 * The caller is responsible for freeing the returned serialized data,
 * @see free_packed_buffer() for this purpose.
 *
 * @param key info used to fill up the protobuf
 * @return a pointer to the serialized data.
 */
struct packed_buffer * serialize_flow_key(struct flow_key *key)
{
    Traffic__FlowKey *pb;
    struct packed_buffer *serialized;
    void *buf;
    size_t len;

    if (key == NULL) return NULL;

    /* Allocate serialization output container */
    serialized = calloc(sizeof(struct packed_buffer), 1);
    if (serialized == NULL) return NULL;

    /* Allocate and set flow key protobuf */
    pb = set_flow_key(key);
    if (pb == NULL) goto err_free_serialized;

    /* Get serialization length */
    len = traffic__flow_key__get_packed_size(pb);
    if (len == 0) goto err_free_pb;;

    /* Allocate space for the serialized buffer */
    buf = malloc(len);
    if (buf == NULL) goto err_free_pb;

    serialized->len = traffic__flow_key__pack(pb, buf);
    serialized->buf = buf;

    /* Free the protobuf structure */
    free_pb_flowkey(pb);

    /* Return serialized content */
    return serialized;

err_free_pb:
    free_pb_flowkey(pb);

err_free_serialized:
    free(serialized);

    return NULL;
}


/**
 * @brief Allocates and sets a flow counters protobuf.
 *
 * Uses the counters info to fill a dynamically allocated
 * flow counters protobuf.
 * The caller is responsible for freeing the returned pointer,
 * @see free_pb_flowcount() for this purpose.
 *
 * @param counters info used to fill up the protobuf
 * @return a pointer to a flow counters protobuf structure
 */
static Traffic__FlowCounters *
set_flow_counters(struct flow_counters *counters)
{
    Traffic__FlowCounters *pb;

    /* Allocate the protobuf structure */
    pb = calloc(sizeof(Traffic__FlowCounters), 1);
    if (pb == NULL) return NULL;

    /* Initialize the protobuf structure */
    traffic__flow_counters__init(pb);

    /* Set the protobuf fields */
    pb->has_packetscount = true;
    pb->packetscount = counters->packets_count;

    pb->has_bytescount = true;
    pb->bytescount = counters->bytes_count;

    return pb;
}


/**
 * @brief Free a flow counters protobuf structure.
 *
 * Free dynamically allocated fields and the protobuf structure.
 *
 * @param pb flow counters structure to free
 * @return none
 */
static void free_pb_flowcount(Traffic__FlowCounters *pb)
{
    free(pb);
}


/**
 * @brief Generates a flow counters serialized protobuf
 *
 * Uses the information pointed by the counter parameter to generate
 * a serialized flow counters buffer.
 * The caller is responsible for freeing to the returned serialized data,
 * @see free_packed_buffer() for this purpose.
 *
 * @param counters info used to fill up the protobuf.
 * @return a pointer to the serialized data.
 */
struct packed_buffer *
serialize_flow_counters(struct flow_counters *counters)
{
    Traffic__FlowCounters *pb;
    struct packed_buffer *serialized;
    void *buf;
    size_t len;

    if (counters == NULL) return NULL;

    /* Allocate serialization output container */
    serialized = calloc(sizeof(struct packed_buffer), 1);
    if (serialized == NULL) return NULL;

    /* Allocate and set a flow counters protobuf */
    pb = set_flow_counters(counters);
    if (pb == NULL) goto err_free_serialized;

    /* Get serialization length */
    len = traffic__flow_counters__get_packed_size(pb);
    if (len == 0) goto err_free_pb;

    /* Allocate space for the serialized buffer */
    buf = malloc(len);
    if (buf == NULL) goto err_free_pb;

    /* Serialize protobuf */
    serialized->len = traffic__flow_counters__pack(pb, buf);
    serialized->buf = buf;

    /* Free the protobuf structure */
    free_pb_flowcount(pb);

    /* Return serialized content */
    return serialized;

err_free_pb:
    free_pb_flowcount(pb);

err_free_serialized:
    free(serialized);

    return NULL;
}


/**
 * @brief Allocates and sets a flow stats protobuf.
 *
 * Uses the stats info to fill a dynamically allocated
 * flow stats protobuf.
 * The caller is responsible for freeing the returned pointer,
 * @see free_pb_stats() for this purpose.
 *
 * @param node info used to fill up the protobuf
 * @return a pointer to a observation point protobuf structure
 */
static Traffic__FlowStats *set_flow_stats(struct flow_stats *stats)
{
    Traffic__FlowStats *pb;

    /* Allocate the protobuf structure */
    pb = calloc(1, sizeof(Traffic__FlowStats));
    if (pb == NULL) return NULL;

    /* Initialize the protobuf structure */
    traffic__flow_stats__init(pb);

    /* Set the protobuf fields */
    pb->flowkey = set_flow_key(stats->key);
    if (pb->flowkey == NULL) goto err_free_pb;

    pb->flowcount = set_flow_counters(stats->counters);
    if (pb == NULL) goto err_free_flow_key;

    return pb;

err_free_flow_key:
    free_pb_flowkey(pb->flowkey);

err_free_pb:
    free(pb);

    return NULL;
}


/**
 * @brief Free a flow stats protobuf structure.
 *
 * Free dynamically allocated fields and the protobuf structure.
 *
 * @param pb flow stats structure to free
 * @return none
 */
void free_pb_flowstats(Traffic__FlowStats *pb)
{
    if (pb == NULL) return;

    free_pb_flowkey(pb->flowkey);
    free_pb_flowcount(pb->flowcount);
    free(pb);
}


/**
 * @brief Generates a flow stats serialized protobuf.
 *
 * Uses the information pointed by the stats parameter to generate
 * a serialized flow stats buffer.
 * The caller is responsible for freeing to the returned serialized data,
 * @see free_packed_buffer() for this purpose.
 *
 * @param stats info used to fill up the protobuf.
 * @return a pointer to the serialized data.
 */
struct packed_buffer * serialize_flow_stats(struct flow_stats *stats)
{
    Traffic__FlowStats *pb;
    struct packed_buffer *serialized;
    void *buf;
    size_t len;

    if (stats == NULL) return NULL;

    /* Allocate serialization output container */
    serialized = calloc(1, sizeof(struct packed_buffer));
    if (serialized == NULL) return NULL;

    /* Allocate and set flow stats protobuf */
    pb = set_flow_stats(stats);
    if (pb == NULL) goto err_free_serialized;

    /* Get serialization length */
    len = traffic__flow_stats__get_packed_size(pb);
    if (len == 0) goto err_free_pb;

    /* Allocate space for the serialized buffer */
    buf = malloc(len);
    if (buf == NULL) goto err_free_pb;

    /* Serialize protobuf */
    serialized->len = traffic__flow_stats__pack(pb, buf);
    serialized->buf = buf;

    /* Free the protobuf structure */
    free_pb_flowstats(pb);

    /* Return serialized content */
    return serialized;

err_free_pb:
    free_pb_flowstats(pb);

err_free_serialized:
    free(serialized);

    return NULL;
}


/**
 * @brief Allocates and sets table of stats protobufs
 *
 * Uses the window info to fill a dynamically allocated
 * table of flow stats protobufs.
 * The caller is responsible for freeing the returned pointer
 *
 * @param window info used to fill up the protobuf table
 * @return a flow stats protobuf pointers table
 */
Traffic__FlowStats **set_pb_flow_stats(struct flow_window *window)
{
    Traffic__FlowStats **stats_pb_tbl;
    struct flow_stats **stats;
    Traffic__FlowStats **stats_pb;
    size_t i, allocated;

    if (window == NULL) return NULL;

    if (window->num_stats == 0) return NULL;

    /* Allocate the array of flow stats */
    stats_pb_tbl = calloc(sizeof(Traffic__FlowStats *),
                          window->num_stats);
    if (stats_pb_tbl == NULL) return NULL;

    /* Set each of the stats protobuf */
    stats = window->flow_stats;
    stats_pb = stats_pb_tbl;
    allocated = 0;
    for (i = 0; i < window->num_stats; i++)
    {
        *stats_pb = set_flow_stats(*stats);
        if (*stats_pb == NULL) goto err_free_pb_stats;

        allocated++;
        stats++;
        stats_pb++;
    }

    return stats_pb_tbl;

err_free_pb_stats:
    stats_pb = stats_pb_tbl;
    for (i = 0; i < allocated; i++)
    {
        free_pb_flowstats(*stats_pb);
        stats_pb++;
    }
    free(stats_pb_tbl);

    return NULL;
}


/**
 * @brief Allocates and sets an observation window protobuf.
 *
 * Uses the stats info to fill a dynamically allocated
 * observation window protobuf.
 * The caller is responsible for freeing the returned pointer,
 * @see free_pb_window() for this purpose.
 *
 * @param node info used to fill up the protobuf
 * @return a pointer to a observation point protobuf structure
 */
static Traffic__ObservationWindow * set_pb_window(struct flow_window *window)
{
    Traffic__ObservationWindow *pb;

    /* Allocate protobuf */
    pb  = calloc(1, sizeof(Traffic__ObservationWindow));
    if (pb == NULL) return NULL;

    /* Initialize protobuf */
    traffic__observation_window__init(pb);

    /* Set protobuf fields */
    pb->has_startedat = true;
    pb->startedat = window->started_at;

    pb->has_endedat = true;
    pb->endedat = window->ended_at;

    /*
     * Accept windows with no stats, bail if stats are present and
     * the stats table setting failed.
     */
    if (window->num_stats == 0) return pb;

    /* Allocate flow_stats container */
    pb->flowstats = set_pb_flow_stats(window);
    if (pb->flowstats == NULL) goto err_free_pb_window;

    pb->n_flowstats = window->num_stats;

    return pb;

err_free_pb_window:
    free(pb);

    return NULL;
}


/**
 * @brief Free an observation window protobuf structure.
 *
 * Free dynamically allocated fields and the protobuf structure.
 *
 * @param pb flows window structure to free
 * @return none
 */
void free_pb_window(Traffic__ObservationWindow *pb)
{
    size_t i;

    if (pb == NULL) return;

    for (i = 0; i < pb->n_flowstats; i++)
    {
        free_pb_flowstats(pb->flowstats[i]);
    }
    free(pb->flowstats);
    free(pb);
}


/**
 * @brief Generates an observation window serialized protobuf
 *
 * Uses the information pointed by the window parameter to generate
 * a serialized obervation window buffer.
 * The caller is responsible for freeing to the returned serialized data,
 * @see free_packed_buffer() for this purpose.
 *
 * @param window info used to fill up the protobuf.
 * @return a pointer to the serialized data.
 */
struct packed_buffer * serialize_flow_window(struct flow_window *window)
{
    Traffic__ObservationWindow *pb;
    struct packed_buffer *serialized;
    void *buf;
    size_t len;

    if (window == NULL) return NULL;

    /* Allocate serialization output container */
    serialized = calloc(sizeof(struct packed_buffer), 1);
    if (serialized == NULL) return NULL;

    /* Allocate and set flow stats protobuf */
    pb = set_pb_window(window);
    if (pb == NULL) goto err_free_serialized;

    /* Get serialization length */
    len = traffic__observation_window__get_packed_size(pb);
    if (len == 0) goto err_free_pb;

    /* Allocate space for the serialized buffer */
    buf = malloc(len);
    if (buf == NULL) goto err_free_pb;

    /* Serialize protobuf */
    serialized->len = traffic__observation_window__pack(pb, buf);
    serialized->buf = buf;

    /* Free the protobuf structure */
    free_pb_window(pb);

    /* Return serialized content */
    return serialized;

err_free_pb:
    free_pb_window(pb);

err_free_serialized:
    free(serialized);

    return NULL;
}


/**
 * @brief Allocates and sets table of observation window protobufs
 *
 * Uses the report info to fill a dynamically allocated
 * table of observation window protobufs.
 * The caller is responsible for freeing the returned pointer
 *
 * @param window info used to fill up the protobuf table
 * @return a flow stats protobuf pointers table
 */
Traffic__ObservationWindow ** set_pb_windows(struct flow_report *report)
{
    Traffic__ObservationWindow **windows_pb_tbl;
    struct flow_window **window;
    Traffic__ObservationWindow **window_pb;
    size_t i, allocated;

    if (report == NULL) return NULL;

    if (report->num_windows == 0) return NULL;

    windows_pb_tbl = calloc(report->num_windows,
                            sizeof(Traffic__ObservationWindow *));
    if (windows_pb_tbl == NULL) return NULL;

    window = report->flow_windows;
    window_pb = windows_pb_tbl;
    allocated = 0;
    /* Set each of the window protobuf */
    for (i = 0; i < report->num_windows; i++)
    {
        *window_pb = set_pb_window(*window);
        if (*window_pb == NULL) goto err_free_pb_windows;

        allocated++;
        window++;
        window_pb++;
    }
    return windows_pb_tbl;

err_free_pb_windows:
    for (i = 0; i < allocated; i++)
    {
        free_pb_window(windows_pb_tbl[i]);
    }
    free(windows_pb_tbl);

    return NULL;
}


/**
 * @brief Allocates and sets a flow report protobuf.
 *
 * Uses the report info to fill a dynamically allocated
 * flow report protobuf.
 * The caller is responsible for freeing the returned pointer,
 * @see free_pb_report() for this purpose.
 *
 * @param node info used to fill up the protobuf
 * @return a pointer to a observation point protobuf structure
 */
static Traffic__FlowReport * set_pb_report(struct flow_report *report)
{
    Traffic__FlowReport *pb;

    /* Allocate protobuf */
    pb  = calloc(sizeof(Traffic__FlowReport), 1);
    if (pb == NULL) return NULL;

    /* Initialize protobuf */
    traffic__flow_report__init(pb);

    /* Set protobuf fields */
    pb->reportedat = report->reported_at;
    pb->has_reportedat = true;

    pb->observationpoint = set_node_info(report->node_info);
    if (pb->observationpoint == NULL) goto err_free_pb_report;

    /*
     * Accept report with no windows, bail if windows are present and
     * the windows table setting failed.
     */
    if (report->num_windows == 0) return pb;

    /* Allocate observation windows container */
    pb->observationwindow = set_pb_windows(report);
    if (pb->observationwindow == NULL) goto err_free_pb_op;

    pb->n_observationwindow = report->num_windows;

    return pb;

err_free_pb_op:
    free_pb_op(pb->observationpoint);

err_free_pb_report:
    free(pb);

    return NULL;
}


/**
 * @brief Free a flow report protobuf structure.
 *
 * Free dynamically allocated fields and the protobuf structure.
 *
 * @param pb flow report structure to free
 * @return none
 */
static void free_pb_report(Traffic__FlowReport *pb)
{
    size_t i;

    if (pb == NULL) return;

    free_pb_op(pb->observationpoint);

    for (i = 0; i < pb->n_observationwindow; i++)
    {
        free_pb_window(pb->observationwindow[i]);
    }

    free(pb->observationwindow);
    free(pb);
}

/**
 * @brief Generates a flow report serialized protobuf
 *
 * Uses the information pointed by the report parameter to generate
 * a serialized flow report buffer.
 * The caller is responsible for freeing to the returned serialized data,
 * @see free_packed_buffer() for this purpose.
 *
 * @param node info used to fill up the protobuf.
 * @return a pointer to the serialized data.
 */
struct packed_buffer * serialize_flow_report(struct flow_report *report)
{
    Traffic__FlowReport *pb = NULL;
    struct packed_buffer *serialized = NULL;
    void *buf;
    size_t len;

    if (report == NULL) return NULL;

    /* Allocate serialization output structure */
    serialized = calloc(sizeof(struct packed_buffer), 1);
    if (serialized == NULL) return NULL;

    /* Allocate and set flow report protobuf */
    pb = set_pb_report(report);
    if (pb == NULL) goto err_free_serialized;

    /* Get serialization length */
    len = traffic__flow_report__get_packed_size(pb);
    if (len == 0) goto err_free_pb;

    /* Allocate space for the serialized buffer */
    buf = malloc(len);
    if (buf == NULL) goto err_free_pb;

    serialized->len = traffic__flow_report__pack(pb, buf);
    serialized->buf = buf;

    /* Free the protobuf structure */
    free_pb_report(pb);

    return serialized;

err_free_pb:
    free(pb);

err_free_serialized:
    free(serialized);

    return NULL;
}
