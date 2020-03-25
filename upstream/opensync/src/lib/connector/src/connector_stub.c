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

/**
 * @file connector_stub.c
 * @brief Connector between OVSDB and external vendor database (stub implementation)
 */

#include "connector_impl.h"
#include "connector.h"


/******************************************************************************
 *  Connector Implementation Stubs
 *****************************************************************************/

#ifndef IMPL_connector_init
bool connector_init(struct ev_loop *loop, const struct connector_ovsdb_api *api)
{
    /*
     * Check header description
     *
     * Use schema macros to update fields -> src/lib/schema/inc/schema.h
     * Example:
     *  SCHEMA_SET_STR(rconf.if_name, wifi0)
     *  SCHEMA_SET_INT(rconf.channel, 10)
     *  ...
     */
    return true;
}
#endif

#ifndef IMPL_connector_close
bool connector_close(struct ev_loop *loop)
{
    /* Close access to your DB */
    return true;
}
#endif

#ifndef IMPL_connector_sync_mode
bool connector_sync_mode(const connector_device_mode_e mode)
{
    /* Handle device mode */
    return true;
}
#endif

#ifndef IMPL_connector_sync_radio
bool connector_sync_radio(const struct schema_Wifi_Radio_Config *rconf)
{
    /*
     * External database may be updated with all radio settings,
     * or one can process just the entries with the '_changed' flag.
     * Example: if (rconf->channel_changed) set_new_channel(rconf->channel);
     */
    return true;
}
#endif

#ifndef IMPL_connector_sync_vif
bool connector_sync_vif(const struct schema_Wifi_VIF_Config *vconf)
{
    /*
     * External database may be updated with all VIF settings,
     * or one can process just the entries with the '_changed' flag.
     * Example: if (vconf->ssid_changed) set_new_ssid(vconf->ssid);
     */
    return true;
}
#endif

#ifndef IMPL_connector_sync_inet
bool connector_sync_inet(const struct schema_Wifi_Inet_Config *iconf)
{
    /*
     * External database may be updated with all Inet settings,
     * or one can process just the entries with the '_changed' flag.
     * Example: if (iconf->inet_addr_changed) set_new_ip(iconf->inet_addr);
     */
    return true;
}
#endif
