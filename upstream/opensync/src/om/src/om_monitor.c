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
 * Openflow Manager - OVSDB monitoring
 */

#define  _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <stdarg.h>
#include <linux/types.h>

#include "om.h"
#include "util.h"
#include "ovsdb_sync.h"


/*****************************************************************************/
#define MODULE_ID LOG_MODULE_ID_MAIN

/*****************************************************************************/
static ovsdb_update_monitor_t   om_monitor_config;
static ovsdb_update_monitor_t   om_monitor_tags;
static ovsdb_update_monitor_t   om_monitor_tag_groups;

/******************************************************************************
 * Updates Openflow_State table to reflect the exit code of ovs-ofctl command
 *****************************************************************************/
static void
om_monitor_update_openflow_state( struct schema_Openflow_Config *ofconf,
                                  om_action_t type, bool ret )
{
    struct  schema_Openflow_State    ofstate;
    json_t                           *jrc;
    json_t                           *js_trans = NULL;
    json_t                           *js_where = NULL;
    json_t                           *js_row   = NULL;

    // memset ofstate to avoid conversion to schema failures
    memset( &ofstate, 0, sizeof( ofstate ) );

    STRSCPY(ofstate.bridge, ofconf->bridge);
    STRSCPY(ofstate.token, ofconf->token);

    // Reflect the result of ovs-ofctl command
    ofstate.success = ret;
    ofstate.success_exists = true;

    if( type == ADD ) {
        // A new flow was added successfully. Add a new row into Openflow_State so that
        // the cloud knows ovs-ofctl succeeded.
        if( !( js_row = schema_Openflow_State_to_json( &ofstate, NULL ))) {
            LOGE( "Failed to convert to schema" );
            goto err_exit;
        }

        if( !(js_trans = ovsdb_tran_multi( NULL, NULL,
                        SCHEMA_TABLE( Openflow_State ),
                        OTR_INSERT,
                        NULL,
                        js_row ))) {
            LOGE( "Failed to create TRANS object" );
            goto err_exit;
        }

        jrc = ovsdb_method_send_s(MT_TRANS, js_trans);
        if (jrc == NULL)
        {
            LOGE( "Openflow_State insert failed to send" );
            goto err_exit;
        }
        json_decref(jrc);

    } else if( type == DELETE ) {
        if( ret ) {
            // A flow was deleted successfully. Delete the corresponding
            // row from Openflow_State so that cloud knows ovs-ofctl succeeded.
            if( !( js_where = ovsdb_tran_cond( OCLM_STR, "token", OFUNC_EQ,
                            ofstate.token ))) {
                LOGE( "Failed to create DEL WHERE object" );
                goto err_exit;
            }

            if( !(js_trans = ovsdb_tran_multi( NULL, NULL,
                            SCHEMA_TABLE( Openflow_State ),
                            OTR_DELETE,
                            js_where,
                            NULL ))) {
                LOGE( "Failed to create DEL TRANS object" );
                goto err_exit;
            }

            jrc = ovsdb_method_send_s(MT_TRANS, js_trans );
            if (jrc == NULL)
            {
                LOGE( "Openstate delete failed to send" );
            }
            json_decref(jrc);

            return;
        } else {
            // ovs-ofctl failed to delete the flow. Don't delete the
            // row from Openflow_State
        }
    }

err_exit:
    if (js_trans) {
        json_decref(js_trans);
    } else {
        if (js_where) {
            json_decref(js_where);
        }
        if (js_row) {
            json_decref(js_row);
        }
    }

    return;
}

/******************************************************************************
 * Adds/deletes flows based on additions/deletions to Openflow_Config table
 *****************************************************************************/
static void
om_monitor_update_flows(om_action_t type, json_t *js)
{
    struct  schema_Openflow_Config   ofconf;
    pjs_errmsg_t                     perr;
    bool                             is_template;
    bool                             ret = false;

    memset( &ofconf, 0, sizeof( ofconf ) );

    if( !schema_Openflow_Config_from_json( &ofconf, js, false, perr )) {
        LOGE( "Failed to parse new Openflow_Config row: %s", perr );
        return;
    }

    is_template = om_tflow_rule_is_template(ofconf.rule);

    switch( type ) {
        case ADD:
            if (is_template) {
                ret = om_tflow_add_from_schema(&ofconf);
            }
            else {

                ret = om_add_flow( ofconf.token, &ofconf );
                LOGN("[%s] Static flow insertion %s (%s, %u, %u, \"%s\", \"%s\")",
                     ofconf.token,
                     (ret == true) ? "succeeded" : "failed",
                     ofconf.bridge, ofconf.table,
                     ofconf.priority, ofconf.rule,
                     ofconf.action);
            }
            break;

        case UPDATE:
            LOGE("Cloud attempted to update a flow (token '%s') -- this is not supported!", ofconf.token);
            return;

        case DELETE:
            if (is_template) {
                ret = om_tflow_remove_from_schema(&ofconf);
            }
            else {
                ret = om_del_flow( ofconf.token, &ofconf );
                LOGN("[%s] Static flow deletion %s (%s, %u, %u, \"%s\", \"%s\")",
                     ofconf.token,
                     (ret == true) ? "succeeded" : "failed",
                     ofconf.bridge, ofconf.table,
                     ofconf.priority, ofconf.rule,
                     ofconf.action);
            }
            break;
    }

    // Update the result in Openflow_State table so the cloud knows
    om_monitor_update_openflow_state( &ofconf, type, ret );

    return;
}

/******************************************************************************
 * Takes appropriate actions based on cloud updates to Openflow_Config table
 *****************************************************************************/
static void
om_monitor_config_cb(ovsdb_update_monitor_t *self)
{
    switch(self->mon_type) {

    case OVSDB_UPDATE_NEW:
        om_monitor_update_flows(ADD, self->mon_json_new);
        break;

    case OVSDB_UPDATE_MODIFY:
        om_monitor_update_flows(UPDATE, self->mon_json_new);
        break;

    case OVSDB_UPDATE_DEL:
        om_monitor_update_flows(DELETE, self->mon_json_old);
        break;

    default:
        break;

    }

    return;
}

/******************************************************************************
 * Adds/deletes/updates tags based on Openflow_Tag table
 *****************************************************************************/
static void
om_monitor_update_tags(om_action_t type, json_t *js)
{
    struct  schema_Openflow_Tag      stag;
    pjs_errmsg_t                     perr;

    memset(&stag, 0, sizeof(stag));

    if(!schema_Openflow_Tag_from_json(&stag, js, false, perr)) {
        LOGE("Failed to parse Openflow_Tag row: %s", perr);
        return;
    }

    switch(type) {

    case ADD:
        om_tag_add_from_schema(&stag);
        break;

    case DELETE:
        om_tag_remove_from_schema(&stag);
        break;

    case UPDATE:
        om_tag_update_from_schema(&stag);
        break;

    default:
        return;

    }

    return;
}

/******************************************************************************
 * Handle Openflow_Tag callbacks
 ******************************************************************************/
static void
om_monitor_tags_cb(ovsdb_update_monitor_t *self)
{
    switch(self->mon_type) {

    case OVSDB_UPDATE_NEW:
        om_monitor_update_tags(ADD, self->mon_json_new);
        break;

    case OVSDB_UPDATE_MODIFY:
        om_monitor_update_tags(UPDATE, self->mon_json_new);
        break;

    case OVSDB_UPDATE_DEL:
        om_monitor_update_tags(DELETE, self->mon_json_old);
        break;

    default:
        break;

    }

    return;
}

/******************************************************************************
 * Adds/deletes/updates tag groups based on Openflow_Tag_Group table
 *****************************************************************************/
static void
om_monitor_update_tag_groups(om_action_t type, json_t *js)
{
    struct schema_Openflow_Tag_Group    sgroup;
    pjs_errmsg_t                        perr;

    memset(&sgroup, 0, sizeof(sgroup));

    if(!schema_Openflow_Tag_Group_from_json(&sgroup, js, false, perr)) {
        LOGE("Failed to parse Openflow_Tag_Group row: %s", perr);
        return;
    }

    switch(type) {

    case ADD:
        om_tag_group_add_from_schema(&sgroup);
        break;

    case DELETE:
        om_tag_group_remove_from_schema(&sgroup);
        break;

    case UPDATE:
        om_tag_group_update_from_schema(&sgroup);
        break;

    default:
        return;

    }

    return;
}

/******************************************************************************
 * Handle Openflow_Tag_Group callbacks
 *****************************************************************************/
static void
om_monitor_tag_groups_cb(ovsdb_update_monitor_t *self)
{
    switch(self->mon_type) {

    case OVSDB_UPDATE_NEW:
        om_monitor_update_tag_groups(ADD, self->mon_json_new);
        break;

    case OVSDB_UPDATE_MODIFY:
        om_monitor_update_tag_groups(UPDATE, self->mon_json_new);
        break;

    case OVSDB_UPDATE_DEL:
        om_monitor_update_tag_groups(DELETE, self->mon_json_old);
        break;

    default:
        break;

    }

    return;
}

/******************************************************************************
 * Sets up Openflow_Config table monitoring
 *****************************************************************************/
bool
om_monitor_init(void)
{
    LOGI( "Openflow Monitoring initialization" );

    // Start OVSDB monitoring
    if(!ovsdb_update_monitor(&om_monitor_config,
                             om_monitor_config_cb,
                             SCHEMA_TABLE(Openflow_Config),
                             OMT_ALL)) {
        LOGE("Failed to monitor OVSDB table '%s'", SCHEMA_TABLE( Openflow_Config ) );
        return false;
    }

    if(!ovsdb_update_monitor(&om_monitor_tags,
                             om_monitor_tags_cb,
                             SCHEMA_TABLE(Openflow_Tag),
                             OMT_ALL)) {
        LOGE("Failed to monitor OVSDB table '%s'", SCHEMA_TABLE(Openflow_Tag));
        return false;
    }

    if(!ovsdb_update_monitor(&om_monitor_tag_groups,
                             om_monitor_tag_groups_cb,
                             SCHEMA_TABLE(Openflow_Tag_Group),
                             OMT_ALL)) {
        LOGE("Failed to monitor OVSDB table '%s'", SCHEMA_TABLE(Openflow_Tag_Group));
        return false;
    }

    return true;
}
