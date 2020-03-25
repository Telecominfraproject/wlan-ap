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

/*
 * Openflow Manager - Template Flow Handling
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>
#include <ev.h>
#include <syslog.h>

#include "schema.h"
#include "om.h"

/*****************************************************************************/
#define MODULE_ID LOG_MODULE_ID_MAIN

/******************************************************************************
 * Local Variables
 *****************************************************************************/
static ds_tree_t            om_template_flows = DS_TREE_INIT((ds_key_cmp_t *)strcmp,
                                                                 om_tflow_t, dst_node);


/******************************************************************************
 * Local Functions
 *****************************************************************************/

// Find a template flow by it's token
static om_tflow_t *
om_tflow_find_by_token(char *token)
{
    return (om_tflow_t *)ds_tree_find(&om_template_flows, token);
}

// Free a template flow structure
static void
om_tflow_free(om_tflow_t *tflow)
{
    if (tflow) {
        if (tflow->token) {
            free(tflow->token);
        }
        if (tflow->bridge) {
            free(tflow->bridge);
        }
        if (tflow->rule) {
            free(tflow->rule);
        }
        if (tflow->action) {
            free(tflow->action);
        }

        om_tag_list_free(&tflow->tags);
        free(tflow);
    }
    return;
}

// Detect vars within the rule
static bool
om_tflow_detect_vars(const char *token, const char *rule, char *what, char var_chr,
                           char begin, char end, ds_tree_t *list, uint8_t base_flags)
{
    uint8_t     flag;
    char        *mrule    = NULL;
    char        *p;
    char        *s;
    bool        ret       = true;

    // Make a copy of the rule we can modify
    mrule = strdup(rule);

    // Detect tags
    p = mrule;
    s = p;
    while((s = strchr(s, var_chr))) {
        s++;
        if (*s != begin) {
            continue;
        }
        s++;

        flag = base_flags;
        if (*s == TEMPLATE_DEVICE_CHAR) {
            s++;
            flag |= OM_TLE_FLAG_DEVICE;
        }
        else if (*s == TEMPLATE_CLOUD_CHAR) {
            s++;
            flag |= OM_TLE_FLAG_CLOUD;
        }
        if (!(p = strchr(s, end))) {
            LOGW("[%s] Template Flow has malformed %s (no ending '%c')",
                                                         token, what, end);
            continue;
        }
        *p++ = '\0';

        LOGT("[%s] Template Flow detected %s %s'%s'",
                    token, what,
                    (flag == OM_TLE_FLAG_DEVICE) ? "device " :
                            (flag == OM_TLE_FLAG_CLOUD) ? "cloud " : "",
                    s);

        if (!om_tag_list_entry_find_by_val_flags(list, s, base_flags)) {
            if (!om_tag_list_entry_add(list, s, flag)) {
                ret = false;
                goto exit;
            }
        }
        s = p;
    }

exit:
    free(mrule);
    if (ret == false) {
        om_tag_list_free(list);
    }
    return ret;
}

// Detect tags within the rule
static bool
om_tflow_detect_tags(const char *token, const char *rule, ds_tree_t *list)
{
    return om_tflow_detect_vars(token,
                                rule,
                                "tag",
                                TEMPLATE_VAR_CHAR,
                                TEMPLATE_TAG_BEGIN,
                                TEMPLATE_TAG_END,
                                list,
                                0);
}

// Detect groups within the rule
static bool
om_tflow_detect_groups(const char *token, const char *rule, ds_tree_t *list)
{
    return om_tflow_detect_vars(token,
                                rule,
                                "tag group",
                                TEMPLATE_VAR_CHAR,
                                TEMPLATE_GROUP_BEGIN,
                                TEMPLATE_GROUP_END,
                                list,
                                OM_TLE_FLAG_GROUP);
}

// Allocate a new template flow, using data from schema flow config row
static om_tflow_t *
om_tflow_alloc_from_schema(struct schema_Openflow_Config *sflow)
{
    om_tflow_t      *tflow;

    // For memory optimization, all string variables within the template flow
    // are dynamically allocated.

    if (!(tflow = calloc(1, sizeof(*tflow)))) {
        goto alloc_err;
    }

    om_tag_list_init(&tflow->tags);
    if (!om_tflow_detect_tags(sflow->token, sflow->rule, &tflow->tags)) {
        goto alloc_err;
    }
    if (!om_tflow_detect_groups(sflow->token, sflow->rule, &tflow->tags)) {
        goto alloc_err;
    }

    if (!(tflow->token = strdup(sflow->token))) {
        goto alloc_err;
    }
    if (!(tflow->bridge = strdup(sflow->bridge))) {
        goto alloc_err;
    }
    if (!(tflow->rule = strdup(sflow->rule))) {
        goto alloc_err;
    }
    if (!(tflow->action = strdup(sflow->action))) {
        goto alloc_err;
    }

    tflow->table_id = sflow->table;
    tflow->priority = sflow->priority;

    return tflow;

alloc_err:
    LOGEM("Failed to allocate memory for template flow '%s'", sflow->token);

    if (tflow) {
        om_tflow_free(tflow);
    }

    return NULL;
}


/******************************************************************************
 * Public Functions
 *****************************************************************************/

// Return template flows tree
ds_tree_t *
om_tflow_get_tree(void)
{
    return &om_template_flows;
}

// Check if provided rule includes tag references
bool
om_tflow_rule_is_template(const char *rule)
{
    const char          *s;

    if (rule) {
        s = rule;
        while((s = strchr(s, TEMPLATE_VAR_CHAR))) {
            s++;
            if (*s == TEMPLATE_TAG_BEGIN) {
                if (strchr(s, TEMPLATE_TAG_END)) {
                    return true;
                }
            }
            else if (*s == TEMPLATE_GROUP_BEGIN) {
                if (strchr(s, TEMPLATE_GROUP_END)) {
                    return true;
                }
            }
        }
    }

    return false;
}

// Add a new template flow from schema flow config row
bool
om_tflow_add_from_schema(struct schema_Openflow_Config *sflow)
{
    om_tflow_t          *tflow;
    char                tbuf[256];

    if (om_tflow_find_by_token(sflow->token)) {
        // Duplicate
        return false;
    }

    if (!(tflow = om_tflow_alloc_from_schema(sflow))) {
        // Allocation failure -- reported already
        return false;
    }
    ds_tree_insert(&om_template_flows, tflow, tflow->token);

    om_tag_list_to_buf(&tflow->tags, 0, tbuf, sizeof(tbuf)-1);
    LOGN("[%s] Template flow inserted (%s, %u, %u, \"%s\", \"%s\"), tags:%s",
                                tflow->token, tflow->bridge, tflow->table_id,
                                tflow->priority, tflow->rule, tflow->action,
                                tbuf);

    om_template_tflow_update(ADD, tflow);
    return true;
}

// Remove a flow from it's schema config row
bool
om_tflow_remove_from_schema(struct schema_Openflow_Config *sflow)
{
    om_tflow_t          *tflow;
    char                tbuf[256];

    if (!(tflow = om_tflow_find_by_token(sflow->token))) {
        // Not found
        return false;
    }
    ds_tree_remove(&om_template_flows, tflow);

    om_tag_list_to_buf(&tflow->tags, 0, tbuf, sizeof(tbuf)-1);
    LOGN("[%s] Template flow removed (%s, %u, %u, \"%s\", \"%s\"), tags:%s",
                                tflow->token, tflow->bridge, tflow->table_id,
                                tflow->priority, tflow->rule, tflow->action,
                                tbuf);

    om_template_tflow_update(DELETE, tflow);
    om_tflow_free(tflow);
    return true;
}

// Fill in schema structure from template flow with expanded rule
bool
om_tflow_to_schema(om_tflow_t *tflow, char *erule,
                                    struct schema_Openflow_Config *sflow)
{
    // Zero it out first
    memset(sflow, 0, sizeof(*sflow));

    // Table ID
    sflow->table = tflow->table_id;

    // Priority
    sflow->priority = tflow->priority;

    // Token
    STRSCPY(sflow->token, tflow->token);

    // Bridge
    STRSCPY(sflow->bridge, tflow->bridge);

    // Action
    STRSCPY(sflow->action, tflow->action);

    // Rule (from Expanded rule passed in)
    STRSCPY(sflow->rule, erule);
    sflow->rule_exists = true;

    return true;
}
