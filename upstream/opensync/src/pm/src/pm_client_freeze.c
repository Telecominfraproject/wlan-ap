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
#include <sys/wait.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <inttypes.h>
#include <jansson.h>

#include "os.h"
#include "util.h"
#include "ovsdb.h"
#include "ovsdb_update.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"
#include "ovsdb_cache.h"
#include "schema.h"
#include "log.h"
#include "ds.h"
#include "json_util.h"
#include "pm.h"

#include "target.h"

// Defines
#define MODULE_ID LOG_MODULE_ID_MAIN

// OVSDB constants
#define OVSDB_MAC_TABLE            "Client_Freeze_Config"

ovsdb_table_t                       table_Client_Freeze_Config;

static bool                         g_client_freeze_init = false;

/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/
static bool
pm_client_freeze_update(struct schema_Client_Freeze_Config *cfcfg, bool status)
{
    pjs_errmsg_t        perr;
    json_t             *where, *row;
    bool                ret;

    /* Skip deleting the empty entries at startup */
    if(!g_client_freeze_init && (strlen(cfcfg->mac) == 0))
    {
        return true;
    }

    LOGI("Updating client '%s' freeze", cfcfg->mac);

    if (status == false)  {
        where = ovsdb_tran_cond(OCLM_STR, "mac", OFUNC_EQ, str_tolower(cfcfg->mac));
        ret = ovsdb_sync_delete_where(OVSDB_MAC_TABLE, where);
        if (!ret) {
            LOGE("Updating client %s freeze (Failed to remove entry)",
                cfcfg->mac);
            return false;
        }
        LOGN("Removed client '%s' freeze", cfcfg->mac);

    }
    else {
        where = ovsdb_tran_cond(OCLM_STR, "mac", OFUNC_EQ, str_tolower(cfcfg->mac));
        row   = schema_Client_Freeze_Config_to_json(cfcfg, perr);
        ret = ovsdb_sync_upsert_where(OVSDB_MAC_TABLE, where, row, NULL);
        if (!ret) {
            LOGE("Updating client freeze %s (Failed to insert entry)",
                cfcfg->mac);
            return false;
        }
        LOGN("Updated client '%s' freeze '%s'",
             cfcfg->mac, cfcfg->type);
    }

    return true;
}

void
callback_Client_Freeze_Config(
        ovsdb_update_monitor_t *mon,
        struct schema_Client_Freeze_Config *old_rec,
        struct schema_Client_Freeze_Config *cfcfg,
        ovsdb_cache_row_t *row)
{
    (void)old_rec;
    (void)row;
    (void)mon;

    if (mon->mon_type == OVSDB_UPDATE_ERROR) {
        LOGE("Can't update OVSDB client freeze");
    }
    else if (mon->mon_type == OVSDB_UPDATE_DEL){
        cfcfg->blocked = false;
        target_client_freeze_set(cfcfg);
    }
    else { /* MOFIFY AND NEW */
        target_client_freeze_set(cfcfg);
    }
}

/******************************************************************************
 *  PUBLIC definitions
 *****************************************************************************/
bool
pm_client_freeze_init(void)
{
    bool         ret;

    OVSDB_TABLE_INIT(Client_Freeze_Config, mac);

    /* Register to client freeze changed ... */
    ret = target_client_freeze_register(pm_client_freeze_update);
    if (false == ret) {
        return false;
    }
    // Initialize OVSDB monitor callback
    OVSDB_CACHE_MONITOR(Client_Freeze_Config, false);

    g_client_freeze_init = true;

    return true;
}
