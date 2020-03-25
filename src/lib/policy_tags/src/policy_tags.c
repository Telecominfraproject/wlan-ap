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
 * Openflow Manager - Template Tag Handling
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

// #include "target.h"
#include "ovsdb.h"
#include "ovsdb_update.h"
#include "schema.h"
#include "policy_tags.h"

/*****************************************************************************/

#define MODULE_ID LOG_MODULE_ID_MAIN


/******************************************************************************
 * Local Variables
 *****************************************************************************/
static ds_tree_t            om_tags = DS_TREE_INIT((ds_key_cmp_t *)strcmp,
                                                                   om_tag_t, dst_node);


static struct tag_mgr my_mgr_s = { 0 };
static struct tag_mgr *my_mgr = &my_mgr_s;

/******************************************************************************
 * Local Functions
 *****************************************************************************/

// Fill in a tag list from schema
static bool
om_tag_add_list_from_schema(ds_tree_t *list, struct schema_Openflow_Tag *stag)
{
    om_tag_list_entry_t *tle;
    int                 i;

    // Device Value Array
    for(i = 0;i < stag->device_value_len;i++) {
        if (!om_tag_list_entry_add(list, stag->device_value[i], OM_TLE_FLAG_DEVICE)) {
            goto err;
        }
    }

    // Cloud Value Array
    for(i = 0;i < stag->cloud_value_len;i++) {
        if ((tle = om_tag_list_entry_find_by_value(list, stag->cloud_value[i]))) {
            tle->flags |= OM_TLE_FLAG_CLOUD;
        }
        else {
            if (!om_tag_list_entry_add(list, stag->cloud_value[i], OM_TLE_FLAG_CLOUD)) {
                goto err;
            }
        }
    }

    return true;

err:
    return false;
}


// Allocate a new tag, using data from schema row
static om_tag_t *
om_tag_alloc_from_schema(struct schema_Openflow_Tag *stag)
{
    om_tag_t            *tag;

    if (!(tag = om_tag_alloc(stag->name, false))) {
        goto alloc_err;
    }

    // Cloud and Device Value Arrays
    if (!om_tag_add_list_from_schema(&tag->values, stag)) {
        LOGEM("Failed to add list for tag '%s'", stag->name);
        goto alloc_err;
    }

    return tag;

alloc_err:
    if (tag) {
        om_tag_free(tag);
    }

    return NULL;
}


/******************************************************************************
 * Public Functions
 *****************************************************************************/

// Free a tag entry
void
om_tag_free(om_tag_t *tag)
{
    om_tag_list_entry_t *vp;
    ds_tree_iter_t      iter;

    if (tag) {
        // Values list
        vp = ds_tree_ifirst(&iter, &tag->values);
        while(vp) {
            ds_tree_iremove(&iter);
            om_tag_list_entry_free(vp);
            vp = ds_tree_inext(&iter);
        }

        // Name
        free(tag->name);

        // Tag itself
        free(tag);
    }

    return;
}

// Allocate a new tag
om_tag_t *
om_tag_alloc(const char *name, bool group)
{
    om_tag_t            *tag;

    // For memory optimization, all string variables within the tags
    // are dynamically allocated.

    // Tag itself
    if (!(tag = calloc(1, sizeof(*tag)))) {
        goto alloc_err;
    }

    // Initialize Values list
    om_tag_list_init(&tag->values);

    // Tag name
    if (!(tag->name = strdup(name))) {
        goto alloc_err;
    }

    // Group?
    tag->group = group;

    return tag;

alloc_err:
    LOGEM("Failed to allocate memory for %stag '%s'", group ? "group " : "", name);

    if (tag) {
        om_tag_free(tag);
    }

    return NULL;
}

// Find a tag by it's name
om_tag_t *
om_tag_find_by_name(const char *name, bool group)
{
    om_tag_t            *tag;

    ds_tree_foreach(&om_tags, tag) {
        if (!strcmp(tag->name, name) && tag->group == group) {
            return tag;
        }
    }

    return NULL;
}

// Add a tag to the global tree
bool
om_tag_add(om_tag_t *tag)
{
    char                dbuf[2048];

    if (om_tag_find_by_name(tag->name, tag->group)) {
        LOGE("[%s] Failed to add tag, it's a duplicate!", tag->name);
        // Duplicate
        return false;
    }

    ds_tree_insert(&om_tags, tag, tag->name);

    om_tag_list_to_buf(&tag->values, 0, dbuf, sizeof(dbuf)-1);
    LOGN("[%s] %sTag added, values:%s",
         tag->name, tag->group ? "Group " : "", dbuf);

    if (my_mgr->service_tag_update != NULL) {
        my_mgr->service_tag_update(tag, NULL, &tag->values, NULL);
    }

    if (!tag->group) {
        om_tag_group_update_by_tag(tag->name);
    }
    return true;
}

// Remove a tag from global tree
bool
om_tag_remove(om_tag_t *tag)
{
    char                dbuf[2048];

    ds_tree_remove(&om_tags, tag);

    om_tag_list_to_buf(&tag->values, 0, dbuf, sizeof(dbuf)-1);
    LOGN("[%s] %sTag removed, values:%s",
                                tag->name, tag->group ? "Group " : "", dbuf);

    if (my_mgr->service_tag_update != NULL) {
        my_mgr->service_tag_update(tag, &tag->values, NULL, NULL);
    }

    if (!tag->group) {
        om_tag_group_update_by_tag(tag->name);
    }

    om_tag_free(tag);
    return true;
}

// Update a tag to new values
bool
om_tag_update(om_tag_t *tag, ds_tree_t *new_values)
{
    om_tag_list_diff_t  diff;
    char                rbuf[2048];
    char                abuf[2048];
    char                ubuf[2048];
    bool                ret = true;

    // Create Diff from old to new lists
    ret = om_tag_list_diff(&tag->values, new_values, &diff);
    if (!ret) {
        LOGE("[%s] Failed to allocate memory for diff to handle update!", tag->name);
        return false;
    }

    om_tag_list_to_buf(&diff.removed, 0, rbuf, sizeof(rbuf)-1);
    om_tag_list_to_buf(&diff.added,   0, abuf, sizeof(abuf)-1);
    om_tag_list_to_buf(&diff.updated, 0, ubuf, sizeof(ubuf)-1);
    LOGN("[%s] %sTag updated, removed:%s, added:%s, updated:%s",
                                tag->name, tag->group ? "Group " : "",
                                rbuf, abuf, ubuf);

    if (my_mgr->service_tag_update != NULL) {
        my_mgr->service_tag_update(tag, &diff.removed, &diff.added, &diff.updated);
    }

    // Apply the diff to our tag's list
    if (!om_tag_list_apply_diff(&tag->values, &diff)) {
        LOGE("[%s] Failed to allocate memory to apply diff for update", tag->name);
        ret = false;
    }

    om_tag_list_diff_free(&diff);

    if (!tag->group) {
        om_tag_group_update_by_tag(tag->name);
    }
    return ret;
}

// Add a new tag from schema row
bool
om_tag_add_from_schema(struct schema_Openflow_Tag *stag)
{
    om_tag_t            *tag;

    if (!(tag = om_tag_alloc_from_schema(stag))) {
        // Allocation failure -- reported already
        return false;
    }

    if (!om_tag_add(tag)) {
        om_tag_free(tag);
        return false;
    }

    return true;
}

// Remove a tag from schema flow
bool
om_tag_remove_from_schema(struct schema_Openflow_Tag *stag)
{
    om_tag_t            *tag;

    if (!(tag = om_tag_find_by_name(stag->name, false))) {
        // Not found
        return false;
    }

    return om_tag_remove(tag);
}

// Update a tag from schema flow
bool
om_tag_update_from_schema(struct schema_Openflow_Tag *stag)
{
    ds_tree_t           new_values;
    om_tag_t            *tag;
    bool                ret;

    if (!(tag = om_tag_find_by_name(stag->name, false))) {
        // Not found
        return false;
    }

    // Build new list
    om_tag_list_init(&new_values);
    if (!om_tag_add_list_from_schema(&new_values, stag)) {
        LOGE("[%s] Failed to allocate memory to handle update!", tag->name);
        return false;
    }

    ret = om_tag_update(tag, &new_values);
    om_tag_list_free(&new_values);

    return ret;
}

void
om_tag_init(struct tag_mgr *mgr) {
    memcpy(&my_mgr_s, mgr, sizeof(my_mgr_s));
}
