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
#define OVSDB_MAC_TABLE            "Client_Nickname_Config"

ovsdb_table_t                       table_Client_Nickname_Config;

static bool                         g_client_nickname_init = false;

/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/
static bool
pm_client_nickname_update(struct schema_Client_Nickname_Config *cncfg, bool status)
{
    pjs_errmsg_t        perr;
    json_t             *where, *row;
    bool                ret;

    /* Skip deleting the empty entries at startup */
    if(!g_client_nickname_init && (strlen(cncfg->mac) == 0))
    {
        return true;
    }

    LOGI("Updating client '%s' nickname", cncfg->mac);

    if (status == false)  {
        where = ovsdb_tran_cond(OCLM_STR, "mac", OFUNC_EQ, str_tolower(cncfg->mac));
        ret = ovsdb_sync_delete_where(OVSDB_MAC_TABLE, where);
        if (!ret) {
            LOGE("Updating client %s nickname (Failed to remove entry)",
                cncfg->mac);
            return false;
        }
        LOGN("Removed client '%s' nickname", cncfg->mac);

    }
    else {
        where = ovsdb_tran_cond(OCLM_STR, "mac", OFUNC_EQ, str_tolower(cncfg->mac));
        row   = schema_Client_Nickname_Config_to_json(cncfg, perr);
        ret = ovsdb_sync_upsert_where(OVSDB_MAC_TABLE, where, row, NULL);
        if (!ret) {
            LOGE("Updating client nickname %s (Failed to insert entry)",
                cncfg->mac);
            return false;
        }
        LOGN("Updated client '%s' nickname '%s'",
             cncfg->mac, cncfg->nickname);

    }

    return true;
}

void
callback_Client_Nickname_Config(
        ovsdb_update_monitor_t *mon,
        struct schema_Client_Nickname_Config *old_rec,
        struct schema_Client_Nickname_Config *cncfg,
        ovsdb_cache_row_t *row)
{
    (void)old_rec;
    (void)row;
    (void)mon;

    if (mon->mon_type == OVSDB_UPDATE_ERROR)
    {
        LOGE("Can't update OVSDB client nickname");
    }
    else if (mon->mon_type == OVSDB_UPDATE_DEL) {
        MEMZERO(cncfg->nickname);
        target_client_nickname_set(cncfg);
    }
    else {
        target_client_nickname_set(cncfg);
    }

}

/******************************************************************************
 *  PUBLIC definitions
 *****************************************************************************/
bool
pm_client_nickname_init(void)
{
    bool         ret;

    OVSDB_TABLE_INIT(Client_Nickname_Config, mac);

    /* Register to client nickname changed ... */
    ret = target_client_nickname_register(pm_client_nickname_update);
    if (false == ret) {
        return false;
    }
    // Initialize OVSDB monitor callback
    OVSDB_CACHE_MONITOR(Client_Nickname_Config, false);

    g_client_nickname_init = true;

    return true;
}
