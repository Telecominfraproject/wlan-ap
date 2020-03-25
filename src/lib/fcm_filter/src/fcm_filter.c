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
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/stat.h>

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
#include "policy_tags.h"
#include "fcm_filter.h"
#include "ds_tree.h"
#include "ovsdb_utils.h"


#define DATA_TYPE fcm_filter_l2_info_t
#define DATA_TYPE_1 fcm_filter_l3_info_t
#define FCM_MAX_FILTER_BY_NAME 64


enum fcm_rule_op
{
    FCM_DEFAULT_FALSE = 0,
    FCM_DEFAULT_TRUE = 1,
    FCM_RULED_FALSE,
    FCM_RULED_TRUE,
    FCM_UNKNOWN_ACTIONS /* always last */
};

enum fcm_action
{
    FCM_EXCLUDE = 0,
    FCM_INCLUDE,
    FCM_DEFAULT_INCLUDE,
    FCM_MAX_ACTIONS,
};

enum fcm_operation
{
    FCM_OP_NONE = 0,
    FCM_OP_IN,
    FCM_OP_OUT,
    FCM_MAX_OP,
};

enum fcm_math
{
    FCM_MATH_NONE = 0,
    FCM_MATH_LT,
    FCM_MATH_LEQ,
    FCM_MATH_GT,
    FCM_MATH_GEQ,
    FCM_MATH_EQ,
    FCM_MATH_NEQ,
    FCM_MAX_MATH_OP,
};

typedef struct ip_port
{
    uint16_t port_min;
    uint16_t port_max;  /* if it set to 0 then no range */
}ip_port_t;

typedef struct _FCM_Filter_rule
{
    char *name;
    int index;

    struct str_set *smac;
    struct str_set *dmac;
    struct int_set *vlanid;
    struct str_set *src_ip;
    struct str_set *dst_ip;

    struct ip_port *src_port;
    int src_port_len;
    struct ip_port *dst_port;
    int dst_port_len;

    struct int_set *proto;

    enum fcm_operation dmac_op;
    enum fcm_operation smac_op;
    enum fcm_operation vlanid_op;
    enum fcm_operation src_ip_op;
    enum fcm_operation dst_ip_op;
    enum fcm_operation src_port_op;
    enum fcm_operation dst_port_op;
    enum fcm_operation proto_op;
    unsigned long pktcnt;
    enum fcm_math pktcnt_op;
    enum fcm_action action;
    void *other_config;
}schema_FCM_Filter_rule_t;


typedef struct fcm_filter
{
    schema_FCM_Filter_rule_t filter_rule;
    bool valid;
    ds_dlist_node_t dl_node;
}fcm_filter_t;

typedef struct rule_name_tree
{
    char *key;
    ds_dlist_t filter_type_list;
    ds_tree_node_t  dl_node;
}rule_name_tree_t;

struct fcm_filter_mgr
{
    int initialized;
    ds_dlist_t filter_type_list[FCM_MAX_FILTER_BY_NAME];
    ds_tree_t name_list;
    char pid[16];
    void (*ovsdb_init)(void);
};


ovsdb_table_t table_FCM_Filter;
ovsdb_table_t table_Openflow_Tag;
ovsdb_table_t table_Openflow_Tag_Group;


static char tag_marker[2] = "${";
static char gtag_marker[2] = "$[";


static
fcm_filter_t* fcm_filter_find_rule(ds_dlist_t *filter_list,
                                   int32_t index);
static
ds_dlist_t* get_list_by_name(char *filter_name, int *index);
static
int free_schema_struct(schema_FCM_Filter_rule_t *rule);

static struct fcm_filter_mgr filter_mgr = { 0 };

struct fcm_filter_mgr* get_filter_mgr(void)
{
    return &filter_mgr;
}


static
struct ip_port* port_to_int1d(int element_len, int element_witdh,
                              char array[][element_witdh])
{
    struct ip_port *arr = NULL;
    int i;
    char *pos = NULL;

    if (!array) return NULL;

    arr = (struct ip_port*) calloc(element_len, sizeof(struct ip_port));
    if (!arr) return NULL;

    for (i = 0; i < element_len; i++)
    {
        pos =  strstr(array[i], "-");
        arr[i].port_min = atoi(&array[i][0]);
        if (pos)
        {
            arr[i].port_max = atoi(pos+1);
        }
        LOGT("port min=%d max=%d \n", arr[i].port_min, arr[i].port_max);
    }
    return arr;
}


static
enum fcm_operation convert_str_enum(char *op_string)
{
     if (strncmp(op_string, "in", strlen("in")) == 0)
        return FCM_OP_IN;
    else if (strncmp(op_string, "out", strlen("out")) == 0)
        return FCM_OP_OUT;
    else
        return FCM_OP_NONE;
}


static
enum fcm_action convert_action_enum(char *op_string)
{
    if (strncmp(op_string, "include", strlen("include")) == 0)
        return FCM_INCLUDE;
    else if (strncmp(op_string, "exclude", strlen("exclude")) == 0)
        return FCM_EXCLUDE;
    else
        return FCM_DEFAULT_INCLUDE;
}


static
enum fcm_math convert_math_str_enum(char *op_string)
{
    if (strncmp(op_string, "leq", strlen("leq")) == 0)
        return FCM_MATH_LEQ;
    else if (strncmp(op_string, "lt", strlen("lt")) == 0)
        return FCM_MATH_LT;
    else if (strncmp(op_string, "gt", strlen("gt")) == 0)
        return FCM_MATH_GT;
    else if (strncmp(op_string, "geq", strlen("geq")) == 0)
        return FCM_MATH_GEQ;
    else if (strncmp(op_string, "eq", strlen("eq")) == 0)
        return FCM_MATH_EQ;
    else if (strncmp(op_string, "neq", strlen("neq")) == 0)
        return FCM_MATH_NEQ;
    else
        return FCM_MATH_NONE;
}


static
char* convert_enum_str(enum fcm_operation op)
{
    switch(op)
    {
        case FCM_OP_NONE:
            return "none";
        case FCM_OP_IN:
            return "in";
        case FCM_OP_OUT:
            return "out";
        default:
            return "unknown";
    }
}


static
int copy_from_schema_struct(schema_FCM_Filter_rule_t *rule,
                            struct schema_FCM_Filter *filter)
{
    if (!rule) return -1;

    /* copy filter name */
    rule->name = (char*) calloc(strlen(filter->name)+1, sizeof(char));
    if (!rule->name) return -1;
    strncpy(rule->name, filter->name, strlen(filter->name));

    /* copy index value; */
    rule->index =  filter->index;

    rule->smac = schema2str_set(sizeof(filter->smac[0]),
                                filter->smac_len,
                                filter->smac);
    if (!rule->smac && filter->smac_len != 0) goto free_rule;

    rule->dmac = schema2str_set(sizeof(filter->dmac[0]),
                                filter->dmac_len,
                                filter->dmac);
    if (!rule->dmac && filter->dmac_len != 0) goto free_rule;

    rule->vlanid = schema2int_set(filter->vlanid_len, filter->vlanid);
    if (!rule->vlanid && filter->vlanid_len != 0) goto free_rule;
    
    rule->src_ip = schema2str_set(sizeof(filter->src_ip[0]),
                                  filter->src_ip_len,
                                  filter->src_ip);
    if (!rule->src_ip && filter->src_ip_len != 0) goto free_rule;

    rule->dst_ip = schema2str_set(sizeof(filter->dst_ip[0]),
                                  filter->dst_ip_len,
                                  filter->dst_ip);
    if (!rule->dst_ip && filter->dst_ip_len != 0) goto free_rule;

    rule->src_port = port_to_int1d(filter->src_port_len,
                                   sizeof(filter->src_port[0]),
                                   filter->src_port);
    rule->src_port_len = filter->src_port_len;
    if (!rule->src_port && rule->src_port_len != 0) goto free_rule;

    rule->dst_port = port_to_int1d(filter->dst_port_len,
                                   sizeof(filter->dst_port[0]),
                                   filter->dst_port);
    rule->dst_port_len = filter->dst_port_len;
    if (!rule->dst_port && rule->dst_port_len != 0) goto free_rule;

    rule->proto = schema2int_set(filter->proto_len, filter->proto);
    if (!rule->proto && filter->proto_len != 0) goto free_rule;

    rule->other_config = schema2tree(sizeof(filter->other_config_keys[0]),
                                     sizeof(filter->other_config[0]),
                                     filter->other_config_len,
                                     filter->other_config_keys,
                                     filter->other_config);
    if (!rule->other_config && filter->other_config_len != 0) goto free_rule;
    
    rule->pktcnt = filter->pktcnt;

    rule->smac_op = convert_str_enum(filter->smac_op);
    rule->dmac_op = convert_str_enum(filter->dmac_op);
    rule->vlanid_op = convert_str_enum(filter->vlanid_op);
    rule->src_ip_op = convert_str_enum(filter->src_ip_op);
    rule->dst_ip_op = convert_str_enum(filter->dst_ip_op);
    rule->src_port_op = convert_str_enum(filter->src_port_op);
    rule->dst_port_op = convert_str_enum(filter->dst_port_op);
    rule->proto_op = convert_str_enum(filter->proto_op);
    rule->pktcnt_op = convert_math_str_enum(filter->pktcnt_op);
    rule->action = convert_action_enum(filter->action);

    return 0;

free_rule:
    free_schema_struct(rule);
    return -1;
}


static
int free_schema_struct(schema_FCM_Filter_rule_t *rule)
{
    if (!rule) return -1;

    free(rule->name);
    rule->name = NULL;

    rule->index = 0;

    free_str_set(rule->smac);
    rule->smac = NULL;

    free_str_set(rule->dmac);
    rule->dmac = NULL;

    free_int_set(rule->vlanid);
    rule->vlanid = NULL;

    free_str_set(rule->src_ip);
    rule->src_ip = NULL;

    free_str_set(rule->dst_ip);
    rule->dst_ip = NULL;

    free(rule->src_port);
    rule->src_port_len = 0;
    rule->src_port = NULL;

    free(rule->dst_port);
    rule->dst_port_len = 0;
    rule->dst_port = NULL;

    free_int_set(rule->proto);
    rule->proto = NULL;

    free_str_tree(rule->other_config);

    rule->smac_op = FCM_OP_NONE;
    rule->dmac_op = FCM_OP_NONE;
    rule->vlanid_op = FCM_OP_NONE;
    rule->src_ip_op = FCM_OP_NONE;
    rule->dst_ip_op = FCM_OP_NONE;
    rule->src_port_op = FCM_OP_NONE;
    rule->dst_port_op = FCM_OP_NONE;
    rule->proto_op = FCM_OP_NONE;
    rule->pktcnt = 0;
    rule->pktcnt_op = FCM_MATH_NONE;
    rule->action = 0;
    return 0;
}


/**
 * fcm_filter_find_rule: looks up a rule that matches unique index
 * @filter_list: head ds_dlist_t
 * @index: index tag
 *
 * Looks up a rule by its index.
 * Returns filter node if found, NULL otherwise.
 */
static
fcm_filter_t *fcm_filter_find_rule(ds_dlist_t *filter_list, int32_t index)
{
    struct fcm_filter *rule = NULL;
    ds_dlist_foreach(filter_list, rule)
    {
        if (rule->filter_rule.index == index) return rule;
    }
    return NULL;
}


static
void fcm_filter_insert_rule(ds_dlist_t *filter_list, fcm_filter_t *rule_new)
{
    struct fcm_filter *rule = NULL;
    /* check empty */
    if (filter_list == NULL || ds_dlist_is_empty(filter_list))
    {
        /* insert at tail */
        ds_dlist_insert_tail(filter_list, rule_new);
        return;
    }

    ds_dlist_foreach(filter_list, rule)
    {
        if (rule_new->filter_rule.index < rule->filter_rule.index)
        {
            ds_dlist_insert_before(filter_list, rule, rule_new);
            return;
        }
        else if (rule_new->filter_rule.index == rule->filter_rule.index)
        {
            LOGE("%s: Index number exist, rule not inserted,"
                 " filter name %s index no %d",
                 __func__, rule_new->filter_rule.name,
                 rule_new->filter_rule.index);
            return;
        }
    }
    ds_dlist_insert_tail(filter_list, rule_new);
}


/**
 * fcm_find_ip_addr_in_tag: looks up a mac address in a tag's value set
 * @ip_addr: string representation of a MAC
 * @schema_tag: tag name as read in ovsdb schema
 * @is_gtag: Boolean indicating a tag group if true
 * @prule:rule schema filter rule
 *
 * Looks up a tag by its name, then looks up the mac in the tag values set.
 * Returns true if found, false otherwise.
 */

static
bool fcm_find_ip_addr_in_tag(char *ip_addr, char *schema_tag,
                             bool is_gtag, schema_FCM_Filter_rule_t *rule)
{
    om_tag_t *tag = NULL;
    om_tag_list_entry_t *e = NULL;
    char tag_name[256] = { 0 };
    char *tag_s = schema_tag + 2; /* pass tag marker */

    /* pass tag values marker */
    if ((*tag_s == TEMPLATE_DEVICE_CHAR) || (*tag_s == TEMPLATE_CLOUD_CHAR))
    {
        tag_s += 1;
    }

    strncpy(tag_name, tag_s, strlen(tag_s) - 1); /* remove end marker */
    tag = om_tag_find_by_name(tag_name, is_gtag);
    if (tag == NULL)
    {
        LOGE("could not find tag %s", tag_name);
        return false;
    }

    e = om_tag_list_entry_find_by_value(&tag->values, ip_addr);
    if (e == NULL) return false;

    LOGT("%s: found %s in tag %s", __func__, ip_addr, tag_name);
    return true;
}


/**
 * fcm_find_device_in_tag: looks up a mac address in a tag's value set
 * @mac_s: string representation of a MAC
 * @schema_tag: tag name as read in ovsdb schema
 * @is_gtag: boolean indicating a tag group if true
 * @p:rule schema filter rule
 *
 * Looks up a tag by its name, then looks up the mac in the tag values set.
 * Returns true if found, false otherwise.
 */

static
bool fcm_find_device_in_tag(char *mac_s, char *schema_tag,
                            bool is_gtag, schema_FCM_Filter_rule_t *rule)
{
    om_tag_t *tag = NULL;
    om_tag_list_entry_t *e = NULL;
    char tag_name[256] = { 0 };
    char *tag_s = schema_tag + 2; /* pass tag marker */
    bool mark;

    /* pass tag values marker */
    mark = (*tag_s == TEMPLATE_DEVICE_CHAR) || (*tag_s == TEMPLATE_CLOUD_CHAR);
    if (mark) tag_s += 1;

    strncpy(tag_name, tag_s, strlen(tag_s) - 1); /* remove end marker */

    tag = om_tag_find_by_name(tag_name, is_gtag);
    if (tag == NULL)
    {
        LOGE("could not find tag %s gtag %d", tag_name, is_gtag);
        return false;
    }

    e = om_tag_list_entry_find_by_value(&tag->values, mac_s);
    if (e == NULL) return false;

    LOGT("%s: found %s in tag %s", __func__, mac_s, tag_name);
    return true;
}


/**
 * fcm_src_ip_in_set: looks up a rule that matches unique index
 * @rule: schema_FCM_Filter rule
 * @data: data, that need to be verified against rule
 *
 * Checking the dst ip address are present set of the rule
 * Returns true if found, false otherwise.
 */

static
bool fcm_src_ip_in_set(schema_FCM_Filter_rule_t *rule, DATA_TYPE_1 *data)
{
    char ipaddr[128] = { 0 };
    size_t i = 0;

    snprintf(ipaddr, sizeof(ipaddr), "%s", data->src_ip);

    for (i = 0; i < rule->src_ip->nelems; i++)
    {
        bool is_tag = !strncmp(rule->src_ip->array[i],
                               tag_marker,
                               sizeof(tag_marker));
        bool is_gtag = !strncmp(rule->src_ip->array[i],
                                gtag_marker,
                                sizeof(gtag_marker));

        if (is_tag || is_gtag)
        {
            bool rc;
            rc = fcm_find_ip_addr_in_tag(ipaddr,
                                         rule->src_ip->array[i],
                                         is_gtag,
                                         rule);
            if (rc) return true;
        }
        else
        {
            int rc;
            rc = strncmp(ipaddr, rule->src_ip->array[i], strlen(ipaddr));
            if (rc == 0) return true;
        }
    }
    return false;
}


static
enum fcm_rule_op fcm_check_option(enum fcm_operation option, bool present)
{

    /* No operation. Consider the rule successful. */
    if (option == FCM_OP_NONE) return FCM_DEFAULT_TRUE;
    if (present && option == FCM_OP_OUT) return FCM_RULED_FALSE;
    if (!present && option == FCM_OP_IN) return FCM_RULED_FALSE;

    return FCM_RULED_TRUE;
}


/**
 * fcm_src_ip_filter: looks up a rule that matches unique index
 * @mgr: fcm_filter_mgr pointer to context manager
 * @rule: schema_FCM_Filter rule
 * @data: data, that need to be verified against rule
 *
 * Checking the src ip option like in / out in the rule.
 *
 * Returns true if found ip is in set and op set to "in", false otherwise.
 */
static
enum fcm_rule_op fcm_src_ip_filter(struct fcm_filter_mgr *mgr,
                                schema_FCM_Filter_rule_t *rule,
                                DATA_TYPE_1 *data)
{
    bool rc = false;

    if (!rule->src_ip) return FCM_DEFAULT_TRUE;
    rc = fcm_src_ip_in_set(rule, data);
    return fcm_check_option(rule->src_ip_op, rc);
}


/**
 * fcm_dst_ip_in_set: looks up a rule that matches unique index
 * @rule: schema_FCM_Filter rule
 * @data: data, that need to be verified against rule
 *
 * Checking the dst ip address are present set of the rule
 * Returns true if found, false otherwise.
 */

static
bool fcm_dst_ip_in_set(schema_FCM_Filter_rule_t *rule, DATA_TYPE_1 *data)
{
    char ipaddr[128] = { 0 };
    size_t i = 0;

    snprintf(ipaddr, sizeof(ipaddr), "%s", data->dst_ip);

    for (i = 0; i < rule->dst_ip->nelems; i++)
    {
        bool is_tag = !strncmp(rule->dst_ip->array[i],
                               tag_marker,
                               sizeof(tag_marker));
        bool is_gtag = !strncmp(rule->dst_ip->array[i],
                                gtag_marker,
                                sizeof(gtag_marker));

        if (is_tag || is_gtag)
        {
            bool rc = false;
            rc = fcm_find_ip_addr_in_tag(ipaddr,
                                         rule->dst_ip->array[i],
                                         is_gtag,
                                         rule);
            if (rc) return true;
        }
        else
        {
            int rc = strncmp(ipaddr, rule->dst_ip->array[i], strlen(ipaddr));
            if (rc == 0) return true;
        }
    }
    return false;
}



/**
 * fcm_dst_ip_filter: looks up a rule that matches unique index
 * @mgr: fcm_filter_mgr pointer to context manager
 * @rule: schema_FCM_Filter rule
 * @data: data, that need to be verified against rule
 *
 * Checking the dst ip option like in / out in the rule.
 *
 * Returns true if found ip is in set and op set to "in", false otherwise.
 */

static
enum fcm_rule_op fcm_dst_ip_filter(struct fcm_filter_mgr *mgr,
                                schema_FCM_Filter_rule_t *rule,
                                DATA_TYPE_1 *data)
{
    bool rc = false;

    if (!rule->dst_ip) return FCM_DEFAULT_TRUE;
    rc = fcm_dst_ip_in_set(rule, data);
    return fcm_check_option(rule->dst_ip_op, rc);
}


/**
 * fcm_sport_in_set: looks up a rule that matches unique index
 * @rule: schema_FCM_Filter rule
 * @data: data, that need to be verified against rule
 *
 * Checking the sport are present set of the rule
 * Returns true if found, false otherwise.
 */

static
bool fcm_sport_in_set(schema_FCM_Filter_rule_t *rule, DATA_TYPE_1 *data)
{
    int i = 0;

    for (i = 0; i < rule->src_port_len; i++)
    {
        if ((rule->src_port[i].port_max != 0) &&
            (rule->src_port[i].port_min <= data->sport
             && data->sport <= rule->src_port[i].port_max ))
        {
            return true;
        }
        else if (rule->src_port[i].port_min == data->sport) return true;
    }

    return false;
}


static
enum fcm_rule_op fcm_sport_filter(struct fcm_filter_mgr *mgr,
                               schema_FCM_Filter_rule_t *rule,
                               DATA_TYPE_1 *data)
{
    bool rc = false;

    /* No dmacs operation. Consider the rule successful. */
    if (!rule->src_port) return FCM_DEFAULT_TRUE;
    rc = fcm_sport_in_set(rule, data);
    return fcm_check_option(rule->src_port_op, rc);
}


/**
 * fcm_dport_in_set: looks up a rule that matches unique index
 * @rule: schema_FCM_Filter rule
 * @data: data, that need to be verified against rule
 *
 * Checking the dst port are present set of the rule
 * Returns true if found, false otherwise.
 */

static
bool fcm_dport_in_set(schema_FCM_Filter_rule_t *rule, DATA_TYPE_1 *data)
{
    int i = 0;

    for (i = 0; i < rule->dst_port_len; i++)
    {
        if ((rule->dst_port[i].port_max != 0) &&
            (rule->dst_port[i].port_min <= data->dport
             && data->dport <= rule->dst_port[i].port_max ))
        {
            return true;
        }
        else if (rule->dst_port[i].port_min == data->dport) return true;
    }
    return false;
}


static
enum fcm_rule_op fcm_dport_filter(struct fcm_filter_mgr *mgr,
                               schema_FCM_Filter_rule_t *rule,
                               DATA_TYPE_1 *data)
{
    bool rc = false;

    /* No dmacs operation. Consider the rule successful. */
    if (!rule->dst_port) return FCM_DEFAULT_TRUE;
    rc = fcm_dport_in_set(rule, data);
    return fcm_check_option(rule->dst_port_op, rc);
}


/**
 * fcm_dport_in_set: looks up a rule that matches unique index
 * @rule: schema_FCM_Filter rule
 * @data: data, that need to be verified against rule
 *
 * Checking the l4 protocol are present set of the rule
 * Returns true if found, false otherwise.
 */
static
bool fcm_proto_in_set(schema_FCM_Filter_rule_t *rule, DATA_TYPE_1 *data)
{
    size_t i = 0;

    for (i = 0; i < rule->proto->nelems; i++)
    {
        if (rule->proto->array[i] == data->l4_proto) return true;
    }

    return false;
}


static
enum fcm_rule_op fcm_l4_proto_filter(struct fcm_filter_mgr *mgr,
                                  schema_FCM_Filter_rule_t *rule,
                                  DATA_TYPE_1 *data)
{
    bool rc = false;

    /* No dmacs operation. Consider the rule successful. */
    if (!rule->proto) return FCM_DEFAULT_TRUE;
    rc = fcm_proto_in_set(rule, data);
    return fcm_check_option(rule->proto_op, rc);
}


/**
 * fcm_smac_in_set: looks up a rule that matches unique index
 * @rule: schema_FCM_Filter rule
 * @data: data, that need to be verified against rule
 *
 * Checking the src mac address are present set of the rule
 * Returns true if found, false otherwise.
 */

static
int fcm_smac_in_set(schema_FCM_Filter_rule_t *rule, DATA_TYPE *data)
{
    char mac_s[32] = { 0 };
    size_t i = 0;

    snprintf(mac_s, sizeof(mac_s), "%s",data->src_mac);

    for (i = 0; i < rule->smac->nelems; i++)
    {
        bool is_tag = !strncmp(rule->smac->array[i], tag_marker,
                    sizeof(tag_marker));
        bool is_gtag = !strncmp(rule->smac->array[i], gtag_marker,
                    sizeof(gtag_marker));
        if (is_tag || is_gtag)
        {
            bool rc;
            rc = fcm_find_device_in_tag(mac_s,
                                        rule->smac->array[i],
                                        is_gtag, rule);
            if (rc) return true;
        }
        else
        {
            int rc = strncmp(mac_s, rule->smac->array[i], strlen(mac_s));
            if (rc == 0) return true;
        }
    }
    return false;
}


/**
 * fcm_dmac_in_set: looks up a rule that matches unique index
 * @rule: schema_FCM_Filter rule
 * @data: data, that need to be verified against rule
 *
 * Checking the dst mac address are present set of the rule
 * Returns true if found, false otherwise.
 */

static
bool fcm_dmac_in_set(schema_FCM_Filter_rule_t *rule, DATA_TYPE *data)
{
    char mac_s[32] = { 0 };
    size_t i = 0;

    snprintf(mac_s, sizeof(mac_s), "%s", data->dst_mac);

    for (i = 0; i < rule->dmac->nelems; i++)
    {
        bool is_tag = !strncmp(rule->dmac->array[i],
                               tag_marker,
                               sizeof(tag_marker));
        bool is_gtag = !strncmp(rule->dmac->array[i],
                                gtag_marker,
                                sizeof(gtag_marker));
        if (is_tag || is_gtag)
        {
            bool rc;
            rc = fcm_find_device_in_tag(mac_s,
                                        rule->dmac->array[i],
                                        is_gtag,
                                        rule);
            if (rc) return true;
        }
        else
        {
            int rc = strncmp(mac_s, rule->dmac->array[i], strlen(mac_s));
            if (rc == 0) return true;
        }
    }
    return false;
}


/**
 * fcm_vlanid_in_set: looks up a rule that matches unique index
 * @rule: schema_FCM_Filter rule
 * @data: data, that need to be verified against rule
 *
 * Checking the vlan_ids are present set of the rule
 * Returns true if found, false otherwise.
 */
static
bool fcm_vlanid_in_set(schema_FCM_Filter_rule_t *rule, DATA_TYPE *data)
{
    size_t i = 0;

    for (i = 0; i < rule->vlanid->nelems; i++)
    {
        if (rule->vlanid->array[i] == (int)data->vlan_id) return true;
    }
    return false;
}


/**
 * fcm_smacs_filter: looks up a rule that matches unique index
 * @mgr: fcm_filter_mgr pointer to context manager
 * @rule: schema_FCM_Filter rule
 * @data: data, that need to be verified against rule
 *
 * Checking the src mac option like in / out in the rule.
 *
 * Returns true if found mac is in set and op set to "in", false otherwise.
 */

static
enum fcm_rule_op fcm_smacs_filter(struct fcm_filter_mgr *mgr,
                               schema_FCM_Filter_rule_t *rule,
                               DATA_TYPE *data)
{
    bool rc = false;

    /* No smac operation. Consider the rule successful. */
    if (!rule->smac) return FCM_DEFAULT_TRUE;
    rc = fcm_smac_in_set(rule, data);
    return fcm_check_option(rule->smac_op, rc);
}


/**
 * fcm_dmacs_filter: looks up a rule that matches unique index
 * @mgr: fcm_filter_mgr pointer to context manager
 * @rule: schema_FCM_Filter rule
 * @data: data, that need to be verified against rule
 *
 * Checking the dst mac option like in / out in the rule.
 *
 * Returns true if found mac is in set and op set to "in", false otherwise.
 */
static
enum fcm_rule_op fcm_dmacs_filter(struct fcm_filter_mgr *mgr,
                               schema_FCM_Filter_rule_t *rule,
                               DATA_TYPE *data)
{
    bool rc = false;

    /* No dmacs operation. Consider the rule successful. */
    if (!rule->dmac) return FCM_DEFAULT_TRUE;
    rc = fcm_dmac_in_set(rule, data);
    return fcm_check_option(rule->dmac_op, rc);
}


static
enum fcm_rule_op fcm_vlanid_filter(struct fcm_filter_mgr *mgr,
                                schema_FCM_Filter_rule_t *rule,
                                DATA_TYPE *data)
{
    bool rc = false;

    /* No valid operation. Consider the rule successful. */
    if (!rule->vlanid) return FCM_DEFAULT_TRUE;
    rc = fcm_vlanid_in_set(rule, data);
    return fcm_check_option(rule->vlanid_op, rc);
}


static
enum fcm_rule_op fcm_pkt_cnt_filter(struct fcm_filter_mgr *mgr,
                             schema_FCM_Filter_rule_t *rule,
                             fcm_filter_stats_t *pkts)
{
    if (!pkts) return FCM_DEFAULT_TRUE;
    switch (rule->pktcnt_op)
    {
        case FCM_MATH_NONE:
            return FCM_DEFAULT_TRUE;
        case FCM_MATH_LEQ:
            if (pkts->pkt_cnt <= rule->pktcnt) return FCM_RULED_TRUE;
            break;
        case FCM_MATH_LT:
            if (pkts->pkt_cnt < rule->pktcnt) return FCM_RULED_TRUE;
            break;
        case FCM_MATH_GT:
            if (pkts->pkt_cnt > rule->pktcnt) return FCM_RULED_TRUE;
            break;
        case FCM_MATH_GEQ:
            if (pkts->pkt_cnt >= rule->pktcnt) return FCM_RULED_TRUE;
            break;
        case FCM_MATH_EQ:
            if (pkts->pkt_cnt == rule->pktcnt) return FCM_RULED_TRUE;
            break;
        case FCM_MATH_NEQ:
            if (pkts->pkt_cnt != rule->pktcnt) return FCM_RULED_TRUE;
            break;
        default:
            break;
    }
    return FCM_RULED_FALSE;
}


static
bool fcm_action_filter(struct fcm_filter_mgr *mgr,
                   schema_FCM_Filter_rule_t *rule)
{
    bool rc = false;

    rc = (rule->action == FCM_DEFAULT_INCLUDE);
    rc |= (rule->action == FCM_INCLUDE);

    return rc;
}


static
void fcm_print_filter (void)
{
    struct fcm_filter_mgr *mgr = get_filter_mgr();
    struct fcm_filter *rule = NULL;
    size_t i = 0;
    int j = 0;

    unsigned int head_index = 0;

    for (head_index = 0; head_index < FCM_MAX_FILTER_BY_NAME; head_index++)
    {
        ds_dlist_foreach(&mgr->filter_type_list[head_index], rule)
        {
            LOGT("** name : %s index %d", rule->filter_rule.name,
                rule->filter_rule.index);
            if (rule->filter_rule.src_ip)
            {
                LOGT("src_ip present : %zu", rule->filter_rule.src_ip->nelems);
                for (i = 0; i < rule->filter_rule.src_ip->nelems; i++)
                {
                    LOGT("  src_ip : %s", rule->filter_rule.src_ip->array[i]);
                }
            }
            if (rule->filter_rule.dst_ip)
            {
                LOGT("dst_ip present : %zu", rule->filter_rule.dst_ip->nelems);
                for (i = 0; i < rule->filter_rule.dst_ip->nelems; i++)
                {
                    LOGT("  dst_ip : %s", rule->filter_rule.dst_ip->array[i]);
                }
            }
            LOGT("src_port present : %d", rule->filter_rule.src_port_len);
            for (j = 0; j < rule->filter_rule.src_port_len; j++)
            {
                LOGT("  src_port : min %d to max %d",
                    rule->filter_rule.src_port[j].port_min,
                    rule->filter_rule.src_port[j].port_max);
            }
            LOGT("dst_port present : %d ", rule->filter_rule.dst_port_len);
            for (j = 0; j < rule->filter_rule.dst_port_len; j++)
            {
                LOGT("  dst_port : min %d to max %d",
                    rule->filter_rule.dst_port[j].port_min,
                    rule->filter_rule.dst_port[j].port_max);
            }
            if (rule->filter_rule.proto)
            {
                LOGT(
                    "l4 protocol present : %zu",
                    rule->filter_rule.proto->nelems);
                for (i = 0; i < rule->filter_rule.proto->nelems; i++)
                {
                    LOGT("  protocol : %d", rule->filter_rule.proto->array[i]);
                }
            }
            if (rule->filter_rule.smac)
            {
                LOGT("smac present : %zu", rule->filter_rule.smac->nelems);
                for (i = 0; i < rule->filter_rule.smac->nelems; i++)
                {
                    LOGT("  src_mac : %s", rule->filter_rule.smac->array[i]);
                }
            }
            if (rule->filter_rule.dmac)
            {
                LOGT("dmac present : %zu", rule->filter_rule.dmac->nelems);
                for (i = 0; i < rule->filter_rule.dmac->nelems; i++)
                {
                    LOGT("  dst_mac : %s", rule->filter_rule.dmac->array[i]);
                }
            }
            if (rule->filter_rule.vlanid)
            {
                LOGT("vlan id present : %zu", rule->filter_rule.vlanid->nelems);
                for (i = 0; i < rule->filter_rule.vlanid->nelems; i++)
                {
                    LOGT("  vlan_ids : %d", rule->filter_rule.vlanid->array[i]);
                }
            }
            LOGT("src_ip_op :  \t%s",
                convert_enum_str(rule->filter_rule.src_ip_op));
            LOGT("dst_ip_op :  \t%s",
                convert_enum_str(rule->filter_rule.dst_ip_op));
            LOGT("src_port_op :\t%s",
                convert_enum_str(rule->filter_rule.src_port_op));
            LOGT("dst_port_op :\t%s",
                convert_enum_str(rule->filter_rule.dst_port_op));
            LOGT("proto_op :   \t%s",
                convert_enum_str(rule->filter_rule.proto_op));
            LOGT("smac_op :    \t%s",
                convert_enum_str(rule->filter_rule.smac_op));
            LOGT("dmac_op :    \t%s",
                convert_enum_str(rule->filter_rule.dmac_op));
            LOGT("vlanid_op :  \t%s",
                convert_enum_str(rule->filter_rule.vlanid_op));
            LOGT("pktcnt : %ld pkt_op : %d", rule->filter_rule.pktcnt,
                rule->filter_rule.pktcnt_op);
            LOGT("action : %d ", rule->filter_rule.action);
        }
    }
}


static
ds_dlist_t* get_list_by_name(char *filter_name, int *index)
{
    struct fcm_filter_mgr *mgr = get_filter_mgr();
    struct fcm_filter *rule = NULL;
    int i = 0;

    for (i = 0; i < FCM_MAX_FILTER_BY_NAME; i++)
    {
        rule = ds_dlist_head(&mgr->filter_type_list[i]);
        if (!rule) break;
        else if (!strncmp(rule->filter_rule.name,
                          filter_name,
                          strlen(rule->filter_rule.name)))
        {
            *index = i;
            return &mgr->filter_type_list[i];
        }
    }
    *index = i;
    return NULL;
}


/**
 * fcm_add_filter: add a FCM Filter
 * @policy: the policy to add
 */

static
void fcm_add_filter(struct schema_FCM_Filter *filter)
{
    struct fcm_filter_mgr *mgr = get_filter_mgr();
    struct fcm_filter *rule = NULL;
    int type_index = 0;

    ds_dlist_t *filter_head = NULL;

    if (!filter) return;

    filter_head = get_list_by_name(filter->name, &type_index);
    if (!filter_head && type_index >= FCM_MAX_FILTER_BY_NAME )
    {
        LOGE(" Too many name filter to hold");
        return;
    }

    rule = calloc(1, sizeof(struct fcm_filter));
    if (!rule) return;

    if (copy_from_schema_struct(&rule->filter_rule, filter) < 0)
        goto free_rule;

    rule->valid = true;
    fcm_filter_insert_rule(&mgr->filter_type_list[type_index], rule);

    if (LOG_SEVERITY_ENABLED(LOG_SEVERITY_TRACE))
        fcm_print_filter();

    return;
free_rule:
    LOGE("unable to add rule");
    free(rule);
}


/**
 * fcm_delete_filter: remove rule from list that matching index
 * @policy: the policy to add
 */

static
void fcm_delete_filter(struct schema_FCM_Filter *filter)
{
    struct fcm_filter_mgr *mgr = get_filter_mgr();
    struct fcm_filter *rule = NULL;
    int type_index = 0;
    ds_dlist_t *filter_head = NULL;

    if (!filter) return;
    filter_head = get_list_by_name(filter->name, &type_index);
    if (!filter_head || type_index >= FCM_MAX_FILTER_BY_NAME)
    {
        LOGE("Filter head is NULL or too many filter name ");
        return;
    }

    rule = fcm_filter_find_rule(&mgr->filter_type_list[type_index],
        filter->index);

    LOGT("Removing filter index %d = index %d", rule->filter_rule.index,
        filter->index);
    ds_dlist_remove(&mgr->filter_type_list[type_index], rule);
    free_schema_struct(&rule->filter_rule);
    free(rule);

    if (LOG_SEVERITY_ENABLED(LOG_SEVERITY_TRACE))
        fcm_print_filter();
}


/**
 * fcm_update_filter: modify existing FCM filter rule
 * @filter: pass new filter details
 */

static
void fcm_update_filter(struct schema_FCM_Filter *old_rec,
                       struct schema_FCM_Filter *filter)
{
    struct fcm_filter_mgr *mgr = get_filter_mgr();
    struct fcm_filter *rule = NULL;
    int type_index = 0;
    ds_dlist_t *filter_head = NULL;

    if (!filter || !old_rec) return;
    filter_head = get_list_by_name(filter->name, &type_index);
    if (!filter_head || type_index >= FCM_MAX_FILTER_BY_NAME )
    {
        LOGE(" Too many name filter to hold");
        return;
    }

    /* find the rule based on index */
    rule = fcm_filter_find_rule(&mgr->filter_type_list[type_index],
        old_rec->index);

    free_schema_struct(&rule->filter_rule);

    if (copy_from_schema_struct(&rule->filter_rule, filter) < 0)
        goto free_rule;
    rule->valid = true;

    if (LOG_SEVERITY_ENABLED(LOG_SEVERITY_TRACE))
        fcm_print_filter();
    return;

free_rule:
    LOGE("unable to update rule");
    free(rule);
}


void callback_FCM_Filter(ovsdb_update_monitor_t *mon,
                         struct schema_FCM_Filter *old_rec,
                         struct schema_FCM_Filter *new_rec)
{
    if (mon->mon_type == OVSDB_UPDATE_NEW)
    {
        fcm_add_filter(new_rec);
    }

    if (mon->mon_type == OVSDB_UPDATE_DEL)
    {
        fcm_delete_filter(old_rec);
    }

    if (mon->mon_type == OVSDB_UPDATE_MODIFY)
    {
        fcm_update_filter(old_rec, new_rec);
    }
}


void callback_Openflow_Tag(ovsdb_update_monitor_t *mon,
                           struct schema_Openflow_Tag *old_rec,
                           struct schema_Openflow_Tag *tag)
{
    if (mon->mon_type == OVSDB_UPDATE_NEW)
    {
        om_tag_add_from_schema(tag);
    }

    if (mon->mon_type == OVSDB_UPDATE_DEL)
    {
        om_tag_remove_from_schema(old_rec);
    }

    if (mon->mon_type == OVSDB_UPDATE_MODIFY)
    {
        om_tag_update_from_schema(tag);
    }
}

void callback_Openflow_Tag_Group(ovsdb_update_monitor_t *mon,
                                 struct schema_Openflow_Tag_Group *old_rec,
                                 struct schema_Openflow_Tag_Group *tag)
{
    if (mon->mon_type == OVSDB_UPDATE_NEW)
    {
        om_tag_group_add_from_schema(tag);
    }

    if (mon->mon_type == OVSDB_UPDATE_DEL)
    {
        om_tag_group_remove_from_schema(old_rec);
    }

    if (mon->mon_type == OVSDB_UPDATE_MODIFY)
    {
        om_tag_group_update_from_schema(tag);
    }
}


void fcm_filter_layer2_apply(char *filter_name, DATA_TYPE *data,
                           struct fcm_filter_stats *pkts, bool *action)
{
    struct fcm_filter_mgr *mgr = get_filter_mgr();
    struct fcm_filter *rule = NULL;
    int type_index = 0;
    bool allow = true;
    bool action_op = true;

    enum fcm_rule_op smac_allow, dmac_allow;
    enum fcm_rule_op vlanid_allow, pktcnt_allow;

    ds_dlist_t *filter_head = NULL;

    if (!data)
    {
        *action = true;
        return;
    }
    filter_head = get_list_by_name(filter_name, &type_index);
    if (!filter_head || ds_dlist_is_empty(filter_head))
    {
        *action = true;
        return;
    }

    ds_dlist_foreach(&mgr->filter_type_list[type_index], rule)
    {
        allow = true;
        smac_allow = fcm_smacs_filter(mgr, &rule->filter_rule, data);
        allow &= (smac_allow == FCM_RULED_FALSE? false: true);

        dmac_allow = fcm_dmacs_filter(mgr, &rule->filter_rule, data);
        allow &= (dmac_allow == FCM_RULED_FALSE? false: true);

        vlanid_allow = fcm_vlanid_filter(mgr, &rule->filter_rule, data);
        allow &= (vlanid_allow == FCM_RULED_FALSE? false: true);

        pktcnt_allow = fcm_pkt_cnt_filter(mgr, &rule->filter_rule, pkts);
        allow &= (pktcnt_allow == FCM_RULED_FALSE? false: true);

        if (LOG_SEVERITY_ENABLED(LOG_SEVERITY_TRACE))
        {
            LOGT("rule index %d --> smac_allow %d dmac_allow %d"
                 "vlanid_allow %d pktcnt_allow %d"
                 " rule sucess %s ",
                 rule->filter_rule.index,
                 smac_allow,
                 dmac_allow,
                 vlanid_allow,
                 pktcnt_allow,
                 allow?"YES":"NO" );
        }
        action_op = fcm_action_filter(mgr, &rule->filter_rule);
        if (allow) goto out;
    }
    action_op = false;

out:
    allow &=action_op;
    *action = allow;
}


void fcm_filter_7tuple_apply(char *filter_name, DATA_TYPE *data,
                             DATA_TYPE_1 *data1,
                             struct fcm_filter_stats *pkts,
                             bool *action)
{
    struct fcm_filter_mgr *mgr = get_filter_mgr();
    struct fcm_filter *rule = NULL;
    int type_index = 0;
    bool allow = true;
    bool action_op = true;
    ds_dlist_t *filter_head = NULL;
    enum fcm_rule_op src_ip_allow, dst_ip_allow;
    enum fcm_rule_op sport_allow, dport_allow, proto_allow;

    enum fcm_rule_op smac_allow, dmac_allow;
    enum fcm_rule_op vlanid_allow, pktcnt_allow;

    if (!data && !data1)
    {
        *action = true;
        return;
    }

    filter_head = get_list_by_name(filter_name, &type_index);
    if (!filter_head || ds_dlist_is_empty(filter_head))
    {
        *action = true;
        return;
    }
    ds_dlist_foreach(&mgr->filter_type_list[type_index], rule)
    {
        allow = true;
        if (data1)
        {
            src_ip_allow = fcm_src_ip_filter(mgr, &rule->filter_rule, data1);
            allow &= (src_ip_allow == FCM_RULED_FALSE? false: true);

            dst_ip_allow = fcm_dst_ip_filter(mgr, &rule->filter_rule, data1);
            allow &= (dst_ip_allow == FCM_RULED_FALSE? false: true);

            sport_allow = fcm_sport_filter(mgr, &rule->filter_rule, data1);
            allow &= (sport_allow == FCM_RULED_FALSE? false: true);

            dport_allow = fcm_dport_filter(mgr, &rule->filter_rule, data1);
            allow &= (dport_allow == FCM_RULED_FALSE? false: true);

            proto_allow = fcm_l4_proto_filter(mgr, &rule->filter_rule, data1);
            allow &= (proto_allow == FCM_RULED_FALSE? false: true);
            if (LOG_SEVERITY_ENABLED(LOG_SEVERITY_TRACE))
            {
                LOGT("rule index %d --> src_ip_allow %d dst_ip_allow %d "
                     "sport_allow %d dport_allow %d proto_allow %d",
                     rule->filter_rule.index,
                     src_ip_allow,
                     dst_ip_allow,
                     sport_allow,
                     dport_allow,
                     proto_allow );
            }
        }
        if (data)
        {
            smac_allow = fcm_smacs_filter(mgr, &rule->filter_rule, data);
            allow &= (smac_allow == FCM_RULED_FALSE? false: true);

            dmac_allow = fcm_dmacs_filter(mgr, &rule->filter_rule, data);
            allow &= (dmac_allow == FCM_RULED_FALSE? false: true);

            vlanid_allow = fcm_vlanid_filter(mgr, &rule->filter_rule, data);
            allow &= (vlanid_allow == FCM_RULED_FALSE? false: true);

            if (LOG_SEVERITY_ENABLED(LOG_SEVERITY_TRACE))
            {
                LOGT("rule index %d --> smac_allow %d dmac_allow %d"
                     "vlanid_allow %d ",
                     rule->filter_rule.index,
                     smac_allow,
                     dmac_allow,
                     vlanid_allow);
            }
        }
        if (pkts)
        {
            pktcnt_allow = fcm_pkt_cnt_filter(mgr, &rule->filter_rule, pkts);
            allow &= (pktcnt_allow == FCM_RULED_FALSE? false: true);

            if (LOG_SEVERITY_ENABLED(LOG_SEVERITY_TRACE))
            {
                LOGT("rule index %d --> pktcnt_allow %d ",
                     rule->filter_rule.index,
                     pktcnt_allow);
            }
        }
        if (LOG_SEVERITY_ENABLED(LOG_SEVERITY_TRACE))
            LOGT("rule sucess %s", allow?"YES":"NO" );

        action_op = fcm_action_filter(mgr, &rule->filter_rule);
        if (allow) goto tuple_out;
    }
    action_op = false;

tuple_out:
    allow &=action_op;
    *action = allow;
}


void  fcm_filter_ovsdb_init(void)
{
    OVSDB_TABLE_INIT_NO_KEY(FCM_Filter);
    OVSDB_TABLE_INIT_NO_KEY(Openflow_Tag);
    OVSDB_TABLE_INIT_NO_KEY(Openflow_Tag_Group);

    OVSDB_TABLE_MONITOR(FCM_Filter, false);
    OVSDB_TABLE_MONITOR(Openflow_Tag, false);
    OVSDB_TABLE_MONITOR(Openflow_Tag_Group, false);
}


int fcm_filter_init(void)
{
    struct fcm_filter_mgr *mgr = get_filter_mgr();
    int i;

    if (mgr->initialized) return 1;

    for (i = 0; i < FCM_MAX_FILTER_BY_NAME; i++)
    {
        ds_dlist_init(&mgr->filter_type_list[i], fcm_filter_t, dl_node);
    }

    if (mgr->ovsdb_init == NULL) mgr->ovsdb_init = fcm_filter_ovsdb_init;

    mgr->ovsdb_init();
    mgr->initialized += 1;
    return 0;
}


void fcm_filter_cleanup(void)
{
    struct fcm_filter_mgr *mgr = get_filter_mgr();
    struct fcm_filter *rule = NULL;
    int i = 0;

    mgr->initialized -= 1;
    if (mgr->initialized) return;

    for (i = 0; i < FCM_MAX_FILTER_BY_NAME; i++)
    {
        while (!ds_dlist_is_empty(&mgr->filter_type_list[i]))
        {
            rule = ds_dlist_head(&mgr->filter_type_list[i]);
            free_schema_struct(&rule->filter_rule);
            ds_dlist_remove(&mgr->filter_type_list[i], rule);
            free(rule);
        }
    }
}
