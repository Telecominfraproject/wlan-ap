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
 * Schema tags library include file
 */

#ifndef __SCHEMA_TAGS_H__
#define __SCHEMA_TAGS_H__

#include "os.h"
#include "ovsdb.h"
#include "ovsdb_update.h"
#include "schema.h"
#include "log.h"
#include "ds_tree.h"



/******************************************************************************
 * Misc Definitions
 *****************************************************************************/
#define TEMPLATE_VAR_CHAR       '$'

#define TEMPLATE_TAG_BEGIN      '{'
#define TEMPLATE_TAG_END        '}'

#define TEMPLATE_GROUP_BEGIN    '['
#define TEMPLATE_GROUP_END      ']'

#define TEMPLATE_DEVICE_CHAR    '@'
#define TEMPLATE_CLOUD_CHAR     '#'

typedef enum {
    ADD         = 0,
    DELETE,
    UPDATE
} om_action_t;

/******************************************************************************
 * Tag List Definitions
 *****************************************************************************/

typedef struct {
    uint8_t         flags;
    char            *value;

    ds_tree_node_t  dst_node;
} om_tag_list_entry_t;

typedef struct {
    ds_tree_t       removed;
    ds_tree_t       added;
    ds_tree_t       updated;
} om_tag_list_diff_t;

extern bool     om_tag_list_entry_add(ds_tree_t *list,
                                      char *value, uint8_t flags);
extern bool     om_tag_list_append_list(ds_tree_t *dest,
                                        ds_tree_t *src, uint8_t filter_flags);
extern bool     om_tag_list_diff(ds_tree_t *old_list, ds_tree_t *new_list,
                                 om_tag_list_diff_t *diff);
extern bool     om_tag_list_apply_diff(ds_tree_t *list,
                                       om_tag_list_diff_t *diff);
extern void     om_tag_list_entry_free(om_tag_list_entry_t *tle);
extern void     om_tag_list_free(ds_tree_t *list);
extern void     om_tag_list_init(ds_tree_t *list);
extern void     om_tag_list_to_buf(ds_tree_t *list, uint8_t flags,
                                   char *buf, int buf_len);
extern void     om_tag_list_diff_free(om_tag_list_diff_t *diff);

extern om_tag_list_entry_t *
                om_tag_list_entry_find_by_value(ds_tree_t *list, char *value);
extern om_tag_list_entry_t *
                om_tag_list_entry_find_by_val_flags(ds_tree_t *list,
                                                  char *value, uint8_t flags);


/******************************************************************************
 * Tag Definitions
 *****************************************************************************/

#define OM_TLE_FLAG_DEVICE      (1 << 0)
#define OM_TLE_FLAG_CLOUD       (1 << 1)
#define OM_TLE_FLAG_GROUP       (1 << 2)

#define OM_TLE_VAR_FLAGS(x)     (x & (OM_TLE_FLAG_DEVICE | OM_TLE_FLAG_CLOUD))

typedef struct {
    char            *name;
    bool            group;

    ds_tree_t       values; // Tree of om_tag_list_entry_t

    ds_tree_node_t  dst_node;
} om_tag_t;

extern void     om_tag_free(om_tag_t *tag);
extern bool     om_tag_add(om_tag_t *tag);
extern bool     om_tag_remove(om_tag_t *tag);
extern bool     om_tag_update(om_tag_t *tag, ds_tree_t *new_values);
extern bool     om_tag_add_from_schema(struct schema_Openflow_Tag *stag);
extern bool     om_tag_remove_from_schema(struct schema_Openflow_Tag *stag);
extern bool     om_tag_update_from_schema(struct schema_Openflow_Tag *stag);
extern om_tag_t *
                om_tag_alloc(const char *name, bool group);
extern om_tag_t *
                om_tag_find_by_name(const char *name, bool group);


struct tag_mgr {
    bool (*service_tag_update)(om_tag_t *tag,
                               ds_tree_t *removed,
                               ds_tree_t *added,
                               ds_tree_t *updated);
};

extern void om_tag_init(struct tag_mgr *mgr);
/******************************************************************************
 * Tag Group Definitions
 *****************************************************************************/

typedef struct {
    char            *name;

    ds_tree_t       tags;   // Tree of om_tag_list_entry_t

    ds_tree_node_t  dst_node;
} om_tag_group_t;

extern bool     om_tag_group_update_by_tag(char *tag_name);
extern bool     om_tag_group_add_from_schema(
    struct schema_Openflow_Tag_Group *sgroup);
extern bool     om_tag_group_remove_from_schema(
    struct schema_Openflow_Tag_Group *sgroup);
extern bool     om_tag_group_update_from_schema(
    struct schema_Openflow_Tag_Group *sgroup);
extern om_tag_group_t *
                om_tag_group_find_by_name(char *name);


#endif /* __SCHEMA_TAGS_H__ */
