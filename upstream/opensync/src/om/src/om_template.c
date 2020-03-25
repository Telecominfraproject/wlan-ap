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
 * Openflow Manager - Dynamic Template Processing
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

#include "policy_tags.h"
#include "om.h"

/*****************************************************************************/

#define MODULE_ID           LOG_MODULE_ID_MAIN
#define MAX_TAGS_PER_RULE   5

typedef enum {
    TAG_FILTER_NORMAL       = 0,
    TAG_FILTER_MATCH,
    TAG_FILTER_MISMATCH
} tag_filter_t;

typedef struct {
    char            *name;
    bool            group;
    char            *value;
} om_tdata_tv_t;

typedef struct {
    om_tdata_tv_t   tv[MAX_TAGS_PER_RULE];
    tag_filter_t    filter;
    ds_tree_t       *tag_override_values;
    char            *tag_override_name;
    bool            ignore_err;
    int             tv_cnt;
} om_tdata_t;

/******************************************************************************
 * Local Variables
 *****************************************************************************/


/******************************************************************************
 * Local Functions
 *****************************************************************************/
static size_t
om_template_rule_len(om_tflow_t *tflow, om_tdata_t *tdata)
{
    int         len;
    int         i;

    len = strlen(tflow->rule);
    for(i = 0;i < tdata->tv_cnt;i++) {
        len -= (strlen(tdata->tv[i].name) + 3 /* ${} */);
        len += strlen(tdata->tv[i].value);
    }

    // Add room for NULL termination
    return len + 1;
}

static char *
om_template_tdata_get_value(om_tdata_t *tdata, char *tag_name, bool group)
{
    int         i;

    for(i = 0;i < tdata->tv_cnt;i++) {
        if (!strcmp(tdata->tv[i].name, tag_name) && tdata->tv[i].group == group) {
            return tdata->tv[i].value;
        }
    }

    return NULL;
}

static char *
om_template_rule_expand(om_tflow_t *tflow, om_tdata_t *tdata)
{
    char        *mrule = NULL;
    char        *erule = NULL;
    char        *nval;
    char        *p;
    char        *s;
    char        *e;
    char        end;
    bool        group;
    int         nlen;

    // Duplicate rule we can modify
    if (!(mrule = strdup(tflow->rule))) {
        LOGE("[%s] Error expanding tags, memory alloc failed", tflow->token);
        goto err;
    }

    // Determine new length, and allocate memory for expanded rule
    nlen = om_template_rule_len(tflow, tdata);
    if (!(erule = calloc(1, nlen))) {
        LOGE("[%s] Error expanding tags, memory alloc failed", tflow->token);
        goto err;
    }

    // Copy rule, replacing tags
    p = mrule;
    s = p;
    while((s = strchr(s, TEMPLATE_VAR_CHAR))) {
        if (*(s+1) == TEMPLATE_TAG_BEGIN) {
           end = TEMPLATE_TAG_END;
           group = false;
        }
        else if (*(s+1) == TEMPLATE_GROUP_BEGIN) {
           end = TEMPLATE_GROUP_END;
           group = true;
        }
        else {
            s++;
            continue;
        }
        *s = '\0';
        strcat(erule, p);

        s += 2;
        if (*s == TEMPLATE_DEVICE_CHAR || *s == TEMPLATE_CLOUD_CHAR) {
            s++;
        }
        if (!(e = strchr(s, end))) {
            LOGE("[%s] Error expanding tags!", tflow->token);
            goto err;
        }
        *e++ = '\0';
        p = e;

        if (!(nval = om_template_tdata_get_value(tdata, s, group))) {
            LOGE("[%s] Error expanding %stag '%s', not found", tflow->token, group ? "group " : "", s);
            goto err;
        }
        strcat(erule, nval);

        s = p;
    }
    if (*p != '\0') {
        strcat(erule, p);
    }

    free(mrule);
    return erule;

err:
    if (mrule) {
        free(mrule);
    }
    if (erule) {
        free(erule);
    }

    return NULL;
}

static bool
om_template_apply(om_action_t type, om_tflow_t *tflow, om_tdata_t *tdata)
{
    struct schema_Openflow_Config   sflow;
    char                            *erule;
    bool                            ret = false;

    if (!(erule = om_template_rule_expand(tflow, tdata))) {
        // It reports error
        return false;
    }

    LOGD("[%s] %s expanded rule '%s'",
                tflow->token,
                (type == ADD) ? "Adding" : "Removing",
                erule);

    switch(type) {

    case ADD:
        if (om_tflow_to_schema(tflow, erule, &sflow)) {
            ret = om_add_flow(sflow.token, &sflow);
        }
        break;

    case DELETE:
        if (om_tflow_to_schema(tflow, erule, &sflow)) {
            ret = om_del_flow(sflow.token, &sflow);
        }
        break;

    default:
        break;

    }

    free(erule);
    return ret;
}

static bool
om_template_apply_tag(om_action_t type, om_tflow_t *tflow,
                    om_tag_list_entry_t *ttle, ds_tree_iter_t *iter, om_tdata_t *tdata, size_t tdn)
{
    om_tag_list_entry_t *tle;
    om_tag_list_entry_t *ntle;
    om_tag_list_entry_t *ftle;
    ds_tree_iter_t      *niter;
    tag_filter_t        filter = TAG_FILTER_NORMAL;
    ds_tree_t           *tlist;
    om_tag_t            *tag;
    uint8_t             filter_flags;
    bool                ret = true;

    if (tdn == 0) {
        LOGI("[%s] %s system flows from template flow %s",
             tflow->token, (type == ADD) ? "Adding" : "Removing",
             ttle->value);
    }

    if (tdata->tag_override_name && !strcmp(ttle->value, tdata->tag_override_name)) {
        tlist  = tdata->tag_override_values;
        filter = tdata->filter;
    }
    else {
        if (!(tag = om_tag_find_by_name(ttle->value, (ttle->flags & OM_TLE_FLAG_GROUP) ? true : false))) {
            LOGW("[%s] Template flow not applied, %stag '%s' not found",
                                                  tflow->token,
                                                  (ttle->flags & OM_TLE_FLAG_GROUP) ? "group " : "",
                                                  ttle->value);
            return false;
        }
        tlist = &tag->values;
    }

    if (!(ftle = om_tag_list_entry_find_by_val_flags(&tflow->tags, ttle->value, ttle->flags))) {
        LOGW("[%s] Template flow does not contain %stag '%s'",
                                                  tflow->token,
                                                  (ttle->flags & OM_TLE_FLAG_GROUP) ? "group " : "",
                                                  ttle->value);
        return false;
    }
    filter_flags = OM_TLE_VAR_FLAGS(ftle->flags);

    ntle = ds_tree_inext(iter);

    tdata->tv[tdn].name  = ttle->value;
    tdata->tv[tdn].group = (ttle->flags & OM_TLE_FLAG_GROUP) ? true : false;
    ds_tree_foreach(tlist, tle) {
        // Perform filter
        switch(filter) {

        default:
        case TAG_FILTER_NORMAL:
            if (filter_flags != 0 && (tle->flags & filter_flags) == 0) {
                continue;
            }
            break;

        case TAG_FILTER_MATCH:
            if (filter_flags == 0 || (tle->flags & filter_flags) == 0) {
                continue;
            }
            break;

        case TAG_FILTER_MISMATCH:
            if (filter_flags == 0 || (tle->flags & filter_flags) != 0) {
                continue;
            }
            break;

        }

        tdata->tv[tdn].value = tle->value;
        if (ntle) {
            if ((tdn+1) >= (sizeof(tdata->tv)/sizeof(om_tdata_tv_t))) {
                LOGE("[%s] Template flow not applied, too many tags", tflow->token);
                return false;
            }
            if (!(niter = malloc(sizeof(*niter)))) {
                LOGE("[%s] Template flow not applied, memory alloc failed", tflow->token);
                return false;
            }
            memcpy(niter, iter, sizeof(*niter));

            ret = om_template_apply_tag(type, tflow, ntle, niter, tdata, tdn+1);
            free(niter);
            if (!ret) {
                break;
            }
        }
        else {
            tdata->tv_cnt = tdn+1;
            ret = om_template_apply(type, tflow, tdata);
            if (!ret) {
                if (!tdata->ignore_err) {
                    break;
                }
                ret = true;
            }
        }
    }

    return ret;
}


/******************************************************************************
 * Public Functions
 *****************************************************************************/

// Update system flows based on add/remove of template rule
bool
om_template_tflow_update(om_action_t type, om_tflow_t *tflow)
{
    om_tag_list_entry_t *tle;
    ds_tree_iter_t      iter;
    om_tdata_t          tdata;

    if (ds_tree_head(&tflow->tags)) {
        memset(&tdata, 0, sizeof(tdata));
        tdata.filter     = TAG_FILTER_NORMAL;
        tdata.ignore_err = false;
        tle = ds_tree_ifirst(&iter, &tflow->tags);
        if (!om_template_apply_tag(type, tflow, tle, &iter, &tdata, 0)) {
            return false;
        }
    }

    return true;
}

// Update system flows based on tag update
bool
om_template_tag_update(om_tag_t *tag, ds_tree_t *removed,
                       ds_tree_t *added, ds_tree_t *updated)
{
    om_tag_list_entry_t *tle;
    ds_tree_iter_t      iter;
    om_tflow_t          *tflow;
    om_tdata_t          tdata;
    ds_tree_t           *tflows;
    bool                ret = true;

    // Fetch flow tree
    tflows = om_tflow_get_tree();

    // Walk template flows and find ones which reference this tag
    ds_tree_foreach(tflows, tflow) {
        if (!om_tag_list_entry_find_by_value(&tflow->tags, tag->name)) {
            continue;
        }

        // Do removals first
        if (removed && ds_tree_head(removed)) {
            memset(&tdata, 0, sizeof(tdata));
            tdata.tag_override_name   = tag->name;
            tdata.tag_override_values = removed;
            tdata.filter              = TAG_FILTER_NORMAL;
            tdata.ignore_err          = false;
            tle = ds_tree_ifirst(&iter, &tflow->tags);
            if (!om_template_apply_tag(DELETE, tflow, tle, &iter, &tdata, 0)) {
                ret = false; // still continue
            }
        }

        // Now do additions
        if (added && ds_tree_head(added)) {
            memset(&tdata, 0, sizeof(tdata));
            tdata.tag_override_name   = tag->name;
            tdata.tag_override_values = added;
            tdata.filter              = TAG_FILTER_NORMAL;
            tdata.ignore_err          = false;
            tle = ds_tree_ifirst(&iter, &tflow->tags);
            if (!om_template_apply_tag(ADD, tflow, tle, &iter, &tdata, 0)) {
                ret = false; // still continue
            }
        }

        // Now do updates
        if (updated && ds_tree_head(updated)) {
            // Remove unmatching first
            memset(&tdata, 0, sizeof(tdata));
            tdata.tag_override_name   = tag->name;
            tdata.tag_override_values = updated;
            tdata.filter              = TAG_FILTER_MISMATCH;
            tdata.ignore_err          = true; // Some flows may already not exist
            tle = ds_tree_ifirst(&iter, &tflow->tags);
            if (!om_template_apply_tag(DELETE, tflow, tle, &iter, &tdata, 0)) {
                ret = false; // still continue
            }

            // Add matching now
            memset(&tdata, 0, sizeof(tdata));
            tdata.tag_override_name   = tag->name;
            tdata.tag_override_values = updated;
            tdata.filter              = TAG_FILTER_MATCH;
            tdata.ignore_err          = false;
            tle = ds_tree_ifirst(&iter, &tflow->tags);
            if (!om_template_apply_tag(ADD, tflow, tle, &iter, &tdata, 0)) {
                ret = false; // still continue
            }
        }
    }

    return ret;
}
