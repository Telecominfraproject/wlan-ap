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
 * @file connector.h
 * @brief Connector API between OVSDB and external vendor database
 */

#ifndef CONNECTOR_H_INCLUDED
#define CONNECTOR_H_INCLUDED

#include <ev.h>
#include "const.h"
#include "schema.h"

/*****************************************************************************/

#define CONNECTOR_DEVICE_INFO_LEN      64

/*****************************************************************************/

typedef enum
{
    MONITOR_MODE = 0,
    CLOUD_MODE,
    BATTERY_MODE
} connector_device_mode_e;

typedef struct
{
    char serial_number[CONNECTOR_DEVICE_INFO_LEN];      /* SerialNumber */
    char platform_version[CONNECTOR_DEVICE_INFO_LEN];   /* SoftwareVersion */
    char model[CONNECTOR_DEVICE_INFO_LEN];              /* ModelName */
    char revision[CONNECTOR_DEVICE_INFO_LEN];           /* HardwareVersion */
} connector_device_info_t;

/**
 * @brief List of OVSDB APIs
 *
 * @description APIs for populating OVSDB
 */
struct connector_ovsdb_api
{
    bool (*connector_device_info_cb)(const connector_device_info_t *info);
    bool (*connector_device_mode_cb)(const connector_device_mode_e mode);
    bool (*connector_cloud_address_cb)(const char *cloud_address);
    bool (*connector_radio_update_cb)(const struct schema_Wifi_Radio_Config *rconf);
    bool (*connector_vif_update_cb)(const struct schema_Wifi_VIF_Config *vconf, const char *radio_ifname);
    bool (*connector_inet_update_cb)(const struct schema_Wifi_Inet_Config *iconf);
};

/**
 * @brief Initial database synchronization
 *
 * @description On startup, databases must be synchronized.
 * With the following function vendor must call the following API's (order is important):
 * 1. connector_device_info_cb      - Device info
 * 2. connector_device_mode_cb      - Operation mode (monitor / cloud)
 * 3. connector_cloud_address_cb    - Cloud address to connect
 * 4. connector_radio_update_cb     - Settings for each radio
 * 5. connector_vif_update_cb       - Settings for each AP/STA interface
 * 6. connector_inet_update_cb      - Settings for each eth/bridge with dhcp/ip
 *
 * To update OVSDB in runtime, use the same callbacks with proper schema flags:
 *  _partial_update: true  - update just subset within a row in the table
 *                   false - replace the whole row in the table
 *
 * Good practice is to populate an OVSDB row in one go and NOT parameter by parameter.
 * If there is no other way, then we suggest that you implement debounce logic.
 *
 * @param[in] loop - Event loop handler (optional)
 * @param[in] api - Structure of all API's
 *
 * @return success of synchronization
 */
bool connector_init(struct ev_loop *loop, const struct connector_ovsdb_api *api);

/**
 * @brief Close access to external database
 * @param[in] loop - Event loop handler (optional)
 * @return success of closing
 */
bool connector_close(struct ev_loop *loop);

/**
 * @brief Update device mode in external DB
 *
 * @description Device mode in OVSDB has changed,
 *              update info in external database
 * @param[in] mode - Device operation mode
 * @return success of synchronization
 */
bool connector_sync_mode(const connector_device_mode_e mode);

/**
 * @brief Update radio in external DB
 *
 * @description Table Wifi_Radio_Config in OVSDB has changed,
 *              update info in external database
 * @param[in] rconf - Radio configuration row
 * @return success of synchronization
 */
bool connector_sync_radio(const struct schema_Wifi_Radio_Config *rconf);

/**
 * @brief Update VIF in external DB
 *
 * @description Table schema_Wifi_VIF_Config in OVSDB has changed,
 *              update info in external database
 * @param[in] vconf - VIF configuration row
 * @return success of synchronization
 */
bool connector_sync_vif(const struct schema_Wifi_VIF_Config *vconf);

/**
 * @brief Update inet in external DB
 *
 * @description Table schema_Wifi_Inet_Config in OVSDB has changed,
 *              update info in external database
 * @param[in] iconf - Inet configuration row
 * @return success of synchronization
 */
bool connector_sync_inet(const struct schema_Wifi_Inet_Config *iconf);

#endif /* CONNECTOR_H_INCLUDED */
