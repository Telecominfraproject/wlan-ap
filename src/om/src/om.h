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
 * openflow Manager - Main Include file
 */

#ifndef __OM_H__
#define __OM_H__

#include "os.h"
#include "evsched.h"
#include "ovsdb.h"
#include "ovsdb_update.h"
#include "log.h"
#include "ds_tree.h"
#include "policy_tags.h"


/******************************************************************************
 * Template Flow Definitions
 *****************************************************************************/

typedef struct {
    char            *token;
    char            *bridge;
    char            *rule;
    char            *action;
    uint8_t         table_id;
    uint8_t         priority;

    ds_tree_t       tags;   // Tree of om_tag_list_entry_t

    ds_tree_node_t  dst_node;
} om_tflow_t;

extern ds_tree_t *
                om_tflow_get_tree(void);
extern bool     om_tflow_rule_is_template(const char *rule);
extern bool     om_tflow_add_from_schema(struct schema_Openflow_Config *sflow);
extern bool     om_tflow_remove_from_schema(struct schema_Openflow_Config *sflow);
extern bool     om_tflow_to_schema(om_tflow_t *tflow, char *erule,
                                            struct schema_Openflow_Config *sflow);



/******************************************************************************
 * Template Action Definitions
 *****************************************************************************/
extern bool     om_template_tflow_update(om_action_t type, om_tflow_t *tflow);
extern bool     om_template_tag_update(om_tag_t *tag, ds_tree_t *removed,
                                        ds_tree_t *added, ds_tree_t *updated);


/******************************************************************************
 * Openflow rules add/delete Definitions
 *****************************************************************************/
extern bool     om_add_flow(const char *token, const struct schema_Openflow_Config *ofconf);
extern bool     om_del_flow(const char *token, const struct schema_Openflow_Config *ofconf);

/******************************************************************************
 * Misc External Function Definitions
 *****************************************************************************/
extern bool     om_monitor_init(void);

#endif /* __OM_H__ */
