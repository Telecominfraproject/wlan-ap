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

#include "json_util.h"
#include "evsched.h"
#include "schema.h"
#include "log.h"
#include "nm2.h"
#include "ovsdb.h"
#include "ovsdb_sync.h"

#include "target.h"

// Defines
#define MODULE_ID LOG_MODULE_ID_MAIN

// OVSDB constants
#define OVSDB_MAC_TABLE            "OVS_MAC_Learning"

static bool                         g_mac_learning_init = false;

/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/
static bool
nm2_mac_learning_update(struct schema_OVS_MAC_Learning *omac, bool oper_status)
{
    pjs_errmsg_t        perr;
    json_t             *where, *row;
    bool                ret;

    /* Skip deleting the empty entries at startup */
    if(!g_mac_learning_init && (strlen(omac->hwaddr) == 0))
    {
        return true;
    }

    // force lower case mac
    str_tolower(omac->hwaddr);
    LOGT("Updating MAC learning '%s' %d", omac->hwaddr, oper_status);

    if (oper_status == false)
    {
        where = ovsdb_tran_cond(OCLM_STR, "hwaddr", OFUNC_EQ, omac->hwaddr);
        ret = ovsdb_sync_delete_where(OVSDB_MAC_TABLE, where);
        if (!ret)
        {
            LOGE("Updating MAC learning %s (Failed to remove entry)",
                        omac->hwaddr);
            return false;
        }
        LOGD("Removed MAC learning '%s' with '%s' '%s'",
                        omac->hwaddr, omac->brname, omac->ifname);

        // Update the eth_devices tag in OpenFlow_Tag
        if (lan_clients_oftag_remove_mac(omac->hwaddr) == -1)
            LOGE("Updating OpenFlow_Tag %s (Failed to remove entry)",
                   omac->hwaddr);
    }
    else
    {
        where = ovsdb_tran_cond(OCLM_STR, "hwaddr", OFUNC_EQ, omac->hwaddr);
        row   = schema_OVS_MAC_Learning_to_json(omac, perr);
        ret = ovsdb_sync_upsert_where(OVSDB_MAC_TABLE, where, row, NULL);
        if (!ret)
        {
            LOGE("Updating MAC learning %s (Failed to insert entry)",
                        omac->hwaddr);
            return false;
        }
        LOGD("Updated MAC learning '%s' with '%s' '%s'",
                        omac->hwaddr, omac->brname, omac->ifname);

        // Update the eth_devices tag in OpenFlow_Tag
        if (lan_clients_oftag_add_mac(omac->hwaddr) == -1)
            LOGE("Updating OpenFlow_Tag %s (Failed to insert entry)",
                   omac->hwaddr);
    }

    return true;
}


/******************************************************************************
 *  PUBLIC definitions
 *****************************************************************************/
bool
nm2_mac_learning_init(void)
{
    bool         ret;

    /* Register to MAC learning changed ... */
    ret = target_mac_learning_register(nm2_mac_learning_update);
    if (false == ret)
    {
        return false;
    }

    g_mac_learning_init = true;

    return true;
}
