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
 * openflow Manager - Tag Group Handling
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

#include "target.h"
#include "policy_tags.h"

/*****************************************************************************/
#define MODULE_ID LOG_MODULE_ID_MAIN


/******************************************************************************
 * Local Variables
 *****************************************************************************/
static ds_tree_t om_tag_groups = DS_TREE_INIT((ds_key_cmp_t *)strcmp,
                                               om_tag_group_t, dst_node);


/******************************************************************************
 * Local Functions
 *****************************************************************************/

// Free a tag group
static void
om_tag_group_free(om_tag_group_t *group)
{
    om_tag_list_entry_t *ep;
    ds_tree_iter_t      iter;

    if (group) {
        // Tags list
        ep = ds_tree_ifirst(&iter, &group->tags);
        while(ep) {
            ds_tree_iremove(&iter);
            om_tag_list_entry_free(ep);
            ep = ds_tree_inext(&iter);
        }

        // Name
        free(group->name);

        // Group itself
        free(group);
    }

    return;
}

// Fill in a tag list from schema
static bool
om_tag_group_create_list_from_schema(ds_tree_t *list,
                                    struct schema_Openflow_Tag_Group *sgroup)
{
    uint8_t             flags;
    char                *name;
    int                 i;

    om_tag_list_init(list);

    // Tag names
    for(i = 0;i < sgroup->tags_len;i++) {
        name = sgroup->tags[i];

        flags = 0;
        if (*name == TEMPLATE_DEVICE_CHAR) {
            name++;
            flags |= OM_TLE_FLAG_DEVICE;
        }
        else if (*name == TEMPLATE_CLOUD_CHAR) {
            name++;
            flags |= OM_TLE_FLAG_CLOUD;
        }
        if (!om_tag_list_entry_add(list, name, flags)) {
            goto alloc_err;
        }
    }

    return true;

alloc_err:
    om_tag_list_free(list);
    return false;
}

// Allocate a new tag group, using data from schema row
static om_tag_group_t *
om_tag_group_alloc_from_schema(struct schema_Openflow_Tag_Group *sgroup)
{
    om_tag_group_t      *group;

    // For memory optimization, all string variables within the tag
    // groups are dynamically allocated.

    // Group itself
    if (!(group = calloc(1, sizeof(*group)))) {
        goto alloc_err;
    }

    // Cloud and Device Value Arrays
    if (!om_tag_group_create_list_from_schema(&group->tags, sgroup)) {
        goto alloc_err;
    }

    // Group name
    if (!(group->name = strdup(sgroup->name))) {
        goto alloc_err;
    }

    return group;

alloc_err:
    LOGEM("Failed to allocate memory for tag group '%s'", sgroup->name);

    if (group) {
        om_tag_group_free(group);
    }

    return NULL;
}

// Append all tag member's values
static bool
om_tag_group_append_members(om_tag_group_t *group, ds_tree_t *dest)
{
    om_tag_list_entry_t *tle;
    om_tag_t            *tag;

    ds_tree_foreach(&group->tags, tle) {
        if ((tag = om_tag_find_by_name(tle->value, false))) {
            if (!om_tag_list_append_list(dest, &tag->values, tle->flags)) {
                LOGE("[%s] Tag group failed to allocate memory "
                     "building value list",
                     group->name);
                return false;
            }
        }
    }

    return true;
}

// Update a group's tag
static bool
om_tag_group_update(om_tag_group_t *group)
{
    ds_tree_t           new_values;
    om_tag_t            *gtag;

    if ((gtag = om_tag_find_by_name(group->name, true))) {
        // Create new members values list
        om_tag_list_init(&new_values);
        if (om_tag_group_append_members(group, &new_values)) {
            // Update the tag
            om_tag_update(gtag, &new_values);
        }
        om_tag_list_free(&new_values);
    }

    return true;
}


/******************************************************************************
 * Public Functions
 *****************************************************************************/

// Find a tag group by it's name
om_tag_group_t *
om_tag_group_find_by_name(char *name)
{
    return (om_tag_group_t *)ds_tree_find(&om_tag_groups, name);
}

// Add a new tag group from schema row
bool
om_tag_group_add_from_schema(struct schema_Openflow_Tag_Group *sgroup)
{
    om_tag_group_t      *group;
    om_tag_t            *gtag;
    char                dbuf[2048];

    if (om_tag_group_find_by_name(sgroup->name)) {
        // Duplicate
        return false;
    }

    if (!(group = om_tag_group_alloc_from_schema(sgroup))) {
        // Allocation failure -- reported already
        return false;
    }
    ds_tree_insert(&om_tag_groups, group, group->name);

    om_tag_list_to_buf(&group->tags, 0, dbuf, sizeof(dbuf)-1);
    LOGN("[%s] Tag group inserted, tags:%s", group->name, dbuf);

    // Create tag for this group
    if (!(gtag = om_tag_alloc(group->name, true))) {
        // It reports error
        return false;
    }

    // Create list based on tag members
    if (!om_tag_group_append_members(group, &gtag->values)) {
        om_tag_free(gtag);
        return false;
    }

    if (!om_tag_add(gtag)) {
        om_tag_free(gtag);
        return false;
    }

    return true;
}

// Remove a tag group from schema row
bool
om_tag_group_remove_from_schema(struct schema_Openflow_Tag_Group *sgroup)
{
    om_tag_group_t      *group;
    om_tag_t            *gtag;
    char                tbuf[2048];

    if (!(group = om_tag_group_find_by_name(sgroup->name))) {
        // Not found
        return false;
    }
    ds_tree_remove(&om_tag_groups, group);

    om_tag_list_to_buf(&group->tags, 0, tbuf, sizeof(tbuf)-1);
    LOGN("[%s] Tag group removed, tags:%s", group->name, tbuf);

    // Remove tag for this group
    if ((gtag = om_tag_find_by_name(group->name, true))) {
        om_tag_remove(gtag);
    }

    om_tag_group_free(group);
    return true;
}

// Update a tag group from schema row
bool
om_tag_group_update_from_schema(struct schema_Openflow_Tag_Group *sgroup)
{
    om_tag_list_diff_t  diff;
    om_tag_group_t      *group;
    ds_tree_t           new_values;
    char                rbuf[2048];
    char                abuf[2048];
    bool                ret;

    if (!(group = om_tag_group_find_by_name(sgroup->name))) {
        // Not found
        return false;
    }

    // Build new tag list.  Then diff / report / apply for debug purposes
    if (!om_tag_group_create_list_from_schema(&new_values, sgroup)) {
        LOGE("[%s] Failed to allocate memory to handle group update!",
             group->name);
        return false;
    }

    ret = om_tag_list_diff(&group->tags, &new_values, &diff);
    om_tag_list_free(&new_values);
    if (!ret) {
        LOGE("[%s] Failed to allocate memory for diff to handle group update!",
             group->name);
        return false;
    }

    om_tag_list_to_buf(&diff.removed, 0, rbuf, sizeof(rbuf)-1);
    om_tag_list_to_buf(&diff.added,   0, abuf, sizeof(abuf)-1);
    LOGN("[%s] Tag group updated, removed:%s, added:%s",
         group->name, rbuf, abuf);

    ret = om_tag_list_apply_diff(&group->tags, &diff);
    om_tag_list_diff_free(&diff);
    if (ret == false) {
        LOGE("[%s] Failed to allocate memory to apply diff for group update",
             group->name);
        return false;
    }

    return om_tag_group_update(group);
}

// Update a group based on tag being updated
bool
om_tag_group_update_by_tag(char *tag_name)
{
    om_tag_group_t      *group;

    ds_tree_foreach(&om_tag_groups, group) {
        if (om_tag_list_entry_find_by_value(&group->tags, tag_name)) {
            LOGN("[%s] Tag group updating due to tag '%s' changing",
                 group->name, tag_name);
            om_tag_group_update(group);
        }
    }

    return true;
}
