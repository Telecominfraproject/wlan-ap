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
 * Openflow Manager - Tag List Handling
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


/******************************************************************************
 * Local Functions
 *****************************************************************************/

// Alloc a list entry
static om_tag_list_entry_t *
om_tag_list_entry_alloc(char *value, uint8_t flags) {
    om_tag_list_entry_t       *tle;

    // For memory optimization, all string variables within the tag list entries
    // are dynamically allocated.

    // Tag List Entry itself
    if (!(tle = calloc(1, sizeof(*tle)))) {
        goto alloc_err;
    }

    // Tag List Entry Value
    if (!(tle->value = strdup(value))) {
        goto alloc_err;
    }
    tle->flags = flags;

    return tle;

alloc_err:
    if (tle) {
        om_tag_list_entry_free(tle);
    }

    return NULL;
}



/******************************************************************************
 * Public Functions
 *****************************************************************************/

// Free a list entry
void
om_tag_list_entry_free(om_tag_list_entry_t *tle) {
    if (tle) {
        // Value
        if (tle->value) {
            free(tle->value);
        }

        // List entry itself
        free(tle);
    }

    return;
}

// Free a list diff
void
om_tag_list_diff_free(om_tag_list_diff_t *diff)
{
    om_tag_list_free(&diff->removed);
    om_tag_list_free(&diff->added);
    om_tag_list_free(&diff->updated);
    return;
}

// Compare two lists, and provide separated removed/added lists
bool
om_tag_list_diff(ds_tree_t *old_list, ds_tree_t *new_list, om_tag_list_diff_t *diff)
{
    om_tag_list_entry_t *ntle;
    om_tag_list_entry_t *otle;

    om_tag_list_init(&diff->removed);
    om_tag_list_init(&diff->added);
    om_tag_list_init(&diff->updated);

    if (old_list) {
        // Find removed entries
        ds_tree_foreach(old_list, otle) {
            if (!new_list || !om_tag_list_entry_find_by_value(new_list, otle->value)) {
                if (!om_tag_list_entry_add(&diff->removed, otle->value, otle->flags)) {
                    goto alloc_err;
                }
            }
        }
    }

    if (new_list) {
        // Find added entries
        ds_tree_foreach(new_list, ntle) {
            if (!old_list || !om_tag_list_entry_find_by_value(old_list, ntle->value)) {
                if (!om_tag_list_entry_add(&diff->added, ntle->value, ntle->flags)) {
                    goto alloc_err;
                }
            }
        }
    }

    if (old_list && new_list) {
        // Find Updated Entries
        ds_tree_foreach(old_list, otle) {
            if ((ntle = om_tag_list_entry_find_by_value(new_list, otle->value))) {
                if (otle->flags != ntle->flags) {
                    if (!om_tag_list_entry_add(&diff->updated, ntle->value, ntle->flags)) {
                        goto alloc_err;
                    }
                }
            }
        }
    }

    return true;

alloc_err:
    om_tag_list_diff_free(diff);
    return false;
}

// Apply a diff to a list
bool
om_tag_list_apply_diff(ds_tree_t *list, om_tag_list_diff_t *diff)
{
    om_tag_list_entry_t *dtle, *tle;

    // Remove entries
    ds_tree_foreach(&diff->removed, dtle) {
        if ((tle = om_tag_list_entry_find_by_value(list, dtle->value))) {
            ds_tree_remove(list, tle);
            om_tag_list_entry_free(tle);
        }
    }

    // Add entries
    ds_tree_foreach(&diff->added, dtle) {
        if (!om_tag_list_entry_find_by_value(list, dtle->value)) {
            if (!om_tag_list_entry_add(list, dtle->value, dtle->flags)) {
                return false;
            }
        }
    }

    // Update entires
    ds_tree_foreach(&diff->updated, dtle) {
        if ((tle = om_tag_list_entry_find_by_value(list, dtle->value))) {
            tle->flags = dtle->flags;
        }
    }

    return true;
}

// Find a tag list entry by it's value
om_tag_list_entry_t *
om_tag_list_entry_find_by_value(ds_tree_t *list, char *value)
{
    return (om_tag_list_entry_t *)ds_tree_find(list, value);
}

// Find a tag list entry by it's value and matching flags
om_tag_list_entry_t *
om_tag_list_entry_find_by_val_flags(ds_tree_t *list, char *value, uint8_t flags)
{
    om_tag_list_entry_t     *tle;

    ds_tree_foreach(list, tle) {
        if (flags && !(tle->flags & flags)) {
            continue;
        }

        if (!strcmp(tle->value, value)) {
            return tle;
        }
    }

    return NULL;
}

// Add a tag list entry to a list
bool
om_tag_list_entry_add(ds_tree_t *list, char *value, uint8_t flags)
{
    om_tag_list_entry_t *tle;

    if (!(tle = om_tag_list_entry_alloc(value, flags))) {
        return false;
    }
    ds_tree_insert(list, tle, tle->value);

    return true;
}

// Add a tag list to another list
bool
om_tag_list_append_list(ds_tree_t *dest, ds_tree_t *src, uint8_t filter_flags)
{
    om_tag_list_entry_t *ntle;
    om_tag_list_entry_t *tle;

    ds_tree_foreach(src, tle) {
        if (filter_flags && !(tle->flags & filter_flags)) {
            continue;
        }

        if ((ntle = om_tag_list_entry_find_by_value(dest, tle->value))) {
            // Combine flags, otherwise it's already in the list
            ntle->flags |= tle->flags;
        }
        else {
            if (!om_tag_list_entry_add(dest, tle->value, tle->flags)) {
                return false;
            }
        }
    }

    return true;
}

// Free a list
void
om_tag_list_free(ds_tree_t *list)
{
    om_tag_list_entry_t *tle;
    ds_tree_iter_t      iter;

    tle = ds_tree_ifirst(&iter, list);
    while(tle) {
        ds_tree_iremove(&iter);
        om_tag_list_entry_free(tle);
        tle = ds_tree_inext(&iter);
    }

    return;
}

// Initialize a tag list
void
om_tag_list_init(ds_tree_t *list)
{
    ds_tree_init(list, (ds_key_cmp_t *)strcmp, om_tag_list_entry_t, dst_node);
    return;
}

// Put tag list into buffer
void
om_tag_list_to_buf(ds_tree_t *list, uint8_t flags, char *buf, int buf_len)
{
    om_tag_list_entry_t *tle;
    char                fstr[9];
    char                *p;
    int                 len;

    p = buf;
    *p = '\0';

    ds_tree_foreach(list, tle) {
        if (buf_len <= 0) {
            break;
        }

        *fstr = '\0';
        if (flags) {
            if (!(tle->flags & flags)) {
                continue;
            }
        }
        else {
            if (tle->flags) {
                strcat(fstr, "(");
                if (tle->flags & OM_TLE_FLAG_DEVICE) {
                    strcat(fstr, "D");
                }
                if (tle->flags & OM_TLE_FLAG_CLOUD) {
                    strcat(fstr, "C");
                }
                if (tle->flags & OM_TLE_FLAG_GROUP) {
                    strcat(fstr, "G");
                }
                strcat(fstr, ")");
            }
        }

        len = snprintf(p, buf_len, " %s%s", tle->value, fstr);

        buf_len -= len;
        p       += len;
    }

    return;
}
