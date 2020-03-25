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

#include <stdbool.h>
#include <ev.h>
#include <assert.h>

#include "ds.h"
#include "ds_dlist.h"
#include "os.h"
#include "log.h"
#include "target.h"
#include "target_impl.h"
#include "target_common.h"
#include "schema.h"

#define MODULE_ID LOG_MODULE_ID_TARGET

/******************************************************************************
 * Init
 *****************************************************************************/

#ifndef IMPL_target_ready
bool target_ready(struct ev_loop *loop)
{
    return true;
}
#endif

#ifndef IMPL_target_init
bool target_init(target_init_opt_t opt, struct ev_loop *loop)
{
    return true;
}
#endif

#ifndef IMPL_target_close
bool target_close(target_init_opt_t opt, struct ev_loop *loop)
{
    return true;
}
#endif

/******************************************************************************
 * Identity
 *****************************************************************************/

#ifndef IMPL_target_serial_get
bool target_serial_get(void *buff, size_t buffsz)
{
    strscpy(((char*)buff), "STUB_SERIAL", buffsz);
    return true;
}
#endif

#ifndef IMPL_target_id_get
bool target_id_get(void *buff, size_t buffsz)
{
    return target_serial_get(buff, buffsz);
}
#endif

#ifndef IMPL_target_sku_get
bool target_sku_get(void *buff, size_t buffsz)
{
    return false;
}
#endif

#ifndef IMPL_target_model_get
bool target_model_get(void *buff, size_t buffsz)
{
    return false;
}
#endif

#ifndef IMPL_target_sw_version_get
bool target_sw_version_get(void *buff, size_t buffsz)
{
    snprintf(buff, buffsz, "%s", app_build_ver_get());
    return true;
}
#endif

#ifndef IMPL_target_hw_revision_get
bool target_hw_revision_get(void *buff, size_t buffsz)
{
    return false;
}
#endif

#ifndef IMPL_target_platform_version_get
bool target_platform_version_get(void *buff, size_t buffsz)
{
    return false;
}
#endif

#ifndef IMPL_target_log_open
bool target_log_open(char *name, int flags)
{
    return log_open(name, flags);
}
#endif

#ifndef IMPL_target_log_pull
bool target_log_pull(const char *upload_location, const char *upload_token)
{
    return true;
}
#endif

#ifndef IMPL_target_log_state_file
const char *target_log_state_file(void)
{
    // Notes:
    // * Call to target_init() is not required.
    // * Path should be pointing to RW part of the file systems, since
    //   ev_stat (inotify) is used to watch for file changes.
    return NULL;
}
#endif

#ifndef IMPL_target_log_trigger_dir
const char *target_log_trigger_dir(void)
{
    // Notes:
    // * Call to target_init() is not required.
    return NULL;
}
#endif

#ifndef IMPL_target_tls_cacert_filename
const char *target_tls_cacert_filename(void)
{
    // Return path/filename to CA Certificate used to validate cloud
    return TARGET_CERT_PATH "/ca.pem";
}
#endif

#ifndef IMPL_target_tls_mycert_filename
const char *target_tls_mycert_filename(void)
{
    // Return path/filename to MY Certificate used to authenticate with cloud
    return TARGET_CERT_PATH "/client.pem";
}
#endif

#ifndef IMPL_target_tls_privkey_filename
const char *target_tls_privkey_filename(void)
{
    // Return path/filename to MY Private Key used to authenticate with cloud
    return TARGET_CERT_PATH "/client_dec.key";
}
#endif


/******************************************************************************
 * LED
 *****************************************************************************/

#ifndef IMPL_target_led_device_dir
const char *target_led_device_dir(void)
{
    // Notes:
    // * Call to target_init() is not required.
    return NULL;
}
#endif

#ifndef IMPL_target_led_names
int target_led_names(const char **leds[])
{
    /* Return number of different LEDs and their names for current target */
    return 0;
}
#endif

/******************************************************************************
 * BLE
 *****************************************************************************/

#ifndef IMPL_target_ble_preinit
bool target_ble_preinit(struct ev_loop *loop)
{
    return true;
}
#endif

#ifndef IMPL_target_ble_prerun
bool target_ble_prerun(struct ev_loop *loop)
{
    return true;
}
#endif

#ifndef IMPL_target_ble_broadcast_start
bool target_ble_broadcast_start(struct schema_AW_Bluetooth_Config *config)
{
    return false;
}
#endif

#ifndef IMPL_target_ble_broadcast_stop
bool target_ble_broadcast_stop(void)
{
    return false;
}
#endif

/******************************************************************************
 * OM
 *****************************************************************************/

#ifndef IMPL_target_om_hook
bool target_om_hook( target_om_hook_t hook, const char *openflow_rule )
{
    return true;
}
#endif

/******************************************************************************
 * RADIO
 *****************************************************************************/

#ifndef IMPL_target_radio_init
bool target_radio_init(const struct target_radio_ops *ops)
{
    return false;
}
#endif

#ifndef IMPL_target_radio_config_init
bool target_radio_config_init(ds_dlist_t *init_cfg)
{
    memset(init_cfg, 0, sizeof(*init_cfg));
    return false;
}
#else
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma message("target_radio_config_init is deprecated")
#endif
#endif

#ifndef IMPL_target_radio_config_init2
bool target_radio_config_init2(void)
{
    return false;
}
#endif

#ifndef IMPL_target_radio_config_need_reset
bool target_radio_config_need_reset(void)
{
    return false;
}
#endif

#ifndef IMPL_target_radio_config_set
bool target_radio_config_set(char *ifname, struct schema_Wifi_Radio_Config *rconf)
{
  return true;
}
#else
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma message("target_radio_config_set is deprecated")
#endif
#endif

#ifndef IMPL_target_radio_config_set2
bool target_radio_config_set2(const struct schema_Wifi_Radio_Config *rconf,
                              const struct schema_Wifi_Radio_Config_flags *changed)
{
  return true;
}
#endif

#ifndef IMPL_target_radio_config_get
bool target_radio_config_get(char *ifname, struct schema_Wifi_Radio_Config *rconf)
{
  return true;
}
#else
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma message("target_radio_config_get is deprecated")
#endif
#endif

#ifndef IMPL_target_radio_state_get
bool target_radio_state_get(char *ifname, struct schema_Wifi_Radio_State *rstate)
{
  return true;
}
#else
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma message("target_radio_state_get is deprecated")
#endif
#endif

#ifndef IMPL_target_radio_state_register
bool target_radio_state_register(char  *ifname, target_radio_state_cb_t *rstate_cb)
{
  return true;
}
#else
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma message("target_radio_state_register is deprecated")
#endif
#endif

#ifndef IMPL_target_radio_config_register
bool target_radio_config_register(char  *ifname, target_radio_config_cb_t *rconf_cb)
{
  return true;
}
#else
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma message("target_radio_config_register is deprecated")
#endif
#endif

/******************************************************************************
 *  INTERFACE
 *****************************************************************************/

#ifndef IMPL_target_wan_interface_name
const char *target_wan_interface_name()
{
    const char *iface_name = "eth0";
    return iface_name;
}
#endif

/******************************************************************************
 * VIF
 *****************************************************************************/

#ifndef IMPL_target_vif_config_set
bool target_vif_config_set (char *ifname, struct schema_Wifi_VIF_Config *vconf)
{
    return true;
}
#else
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma message("target_vif_config_set is deprecated")
#endif
#endif

#ifndef IMPL_target_vif_config_set2
bool target_vif_config_set2(const struct schema_Wifi_VIF_Config *vconf,
                            const struct schema_Wifi_Radio_Config *rconf,
                            const struct schema_Wifi_Credential_Config *cconfs,
                            const struct schema_Wifi_VIF_Config_flags *changed,
                            int num_cconfs)
{
    return true;
}
#endif

#ifndef IMPL_target_vif_get_sta_ifname
const char * target_vif_get_sta_ifname(const char *phy)
{
    return NULL;
}
#endif

#ifndef IMPL_target_vif_config_get
bool target_vif_config_get(char *ifname, struct schema_Wifi_VIF_Config *vconf)
{
    return true;
}
#else
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma message("target_vif_config_get is deprecated")
#endif
#endif

#ifndef IMPL_target_vif_state_get
bool target_vif_state_get(char  *ifname, struct schema_Wifi_VIF_State *vstate)
{
    return true;
}
#else
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma message("target_vif_state_get is deprecated")
#endif
#endif

#ifndef IMPL_target_vif_state_register
bool target_vif_state_register(char *ifname, target_vif_state_cb_t *vstate_cb)
{
    return true;
}
#else
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma message("target_vif_state_register is deprecated")
#endif
#endif

#ifndef IMPL_target_vif_config_register
bool target_vif_config_register(char  *ifname, target_vif_config_cb_t *rconf_cb)
{
    return true;
}
#else
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma message("target_vif_config_register is deprecated")
#endif
#endif

/******************************************************************************
 *  CLIENTS definitions
 *****************************************************************************/

#ifndef IMPL_target_clients_register
bool target_clients_register(char *ifname, target_clients_cb_t *clients_update_cb)
{
    return true;
}
#else
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma message("target_clients_register is deprecated")
#endif
#endif

/******************************************************************************
 *  DHCP/HOSTS definitions
 *****************************************************************************/

#ifndef IMPL_target_dhcp_leased_ip_register
bool target_dhcp_leased_ip_register(target_dhcp_leased_ip_cb_t *dlip_cb)
{
    return true;
}
#endif

/******************************************************************************
 * INET definitions
 *****************************************************************************/
#ifndef IMPL_target_route_state_init
bool target_route_state_init(ds_dlist_t *inets)
{
        ds_dlist_init(inets, target_route_state_init_t, dsl_node);
            return true;
}
#endif

/******************************************************************************
 * Ethernet clients
 *****************************************************************************/

#ifndef IMPL_target_ethclient_brlist_get
const char **target_ethclient_brlist_get()
{
    static const char *brlist[] = { "br-home", NULL };
    return brlist;
}
#endif

#ifndef IMPL_target_ethclient_iflist_get
const char **target_ethclient_iflist_get()
{
    static const char *iflist[] = { "eth0", NULL };
    return iflist;
}
#endif

#ifndef IMPL_target_route_state_register
bool target_route_state_register(target_route_state_cb_t *rts_cb)
{
    return true;
}
#endif

/******************************************************************************
 * SERVICE
 *****************************************************************************/

#ifndef IMPL_target_device_config_register
bool target_device_config_register(void *awlan_cb)
{
    return true;
}
#endif

#ifndef IMPL_target_device_config_set
bool target_device_config_set(struct schema_AWLAN_Node *awlan)
{
    return true;
}
#endif

#ifndef IMPL_target_device_execute
bool target_device_execute(const char *cmd)
{
    return true;
}
#endif
#ifndef IMPL_target_device_capabilities_get
int target_device_capabilities_get()
{
    return 0;
}
#endif
#ifndef IMPL_target_device_connectivity_check
bool target_device_connectivity_check(const char *ifname,
                                      target_connectivity_check_t *cstate,
                                      target_connectivity_check_option_t opts)
{
    return true;
}
#endif

#ifndef IMPL_target_device_restart_managers
bool target_device_restart_managers()
{
    return true;
}
#endif

#ifndef IMPL_target_device_wdt_ping
bool target_device_wdt_ping()
{
    return true;
}
#endif

/******************************************************************************
 * DM
 *****************************************************************************/

#ifndef IMPL_target_managers_config
target_managers_config_t target_managers_config[] = { };
int target_managers_num = (sizeof(target_managers_config) / sizeof(target_managers_config[0]));
#endif

/*
 * Give up on everything and just call the restart.sh script. This should reset the system
 * to a clean slate, restart OVSDB and kick off a new instance of DM.
 */
#ifndef IMPL_target_managers_restart
void target_managers_restart(void)
{
    int fd;
    char cmd[TARGET_BUFF_SZ];

    const char *scripts_dir = target_scripts_dir();
    int max_fd = sysconf(_SC_OPEN_MAX);

    LOG(EMERG, "=======  GENERAL RESTART  ========");

    sprintf(cmd, "%s/restart.sh", scripts_dir);

    LOG(EMERG, "Plan B is executing restart script: %s", cmd);

    /* Close file descriptors from 3 and above */
    for(fd = 3; fd < max_fd; fd++) close(fd);

    /* When the parent process exits, the child will get disowned */
    if (fork() != 0)
    {
        exit(1);
    }

    setsid();

    /* Execute the restart script */
    if (scripts_dir == NULL)
    {
        LOG(ERR, "Error, script dir not defined");
    }
    else
    {
        struct stat sb;
        if ( !(0 == stat(scripts_dir, &sb) && S_ISDIR(sb.st_mode)) )
        {
            LOG(ERR, "Error, scripts dir does not exist");
        }
    }
    execl(cmd, cmd, NULL);

    LOG(EMERG, "Failed to execute plan B!");
}
#endif

/******************************************************************************
 * NM
 *****************************************************************************/
#ifndef IMPL_target_mac_learning_register
bool target_mac_learning_register(target_mac_learning_cb_t *omac_cb)
{
    return false;
}
#endif

/******************************************************************************
 * PATHS
 *****************************************************************************/
#ifndef IMPL_target_tools_dir
const char* target_tools_dir(void)
{
    assert(!"tools_dir not defined for current platform.");
    return NULL;
}
#endif

#ifndef IMPL_target_bin_dir
const char* target_bin_dir(void)
{
    assert(!"bin_dir not defined for current platform.");
    return NULL;
}
#endif

#ifndef IMPL_target_scripts_dir
const char* target_scripts_dir(void)
{
    // For backwards compatibility, return bin dir
    return target_bin_dir();
}
#endif

#ifndef IMPL_target_persistent_storage_dir
const char *target_persistent_storage_dir(void)
{
    assert(!"persistent_storage_dir not defined for current platform.");
    return NULL;
}
#endif

/******************************************************************************
 * UPGRADE
 *****************************************************************************/

#ifndef IMPL_target_upg_download_required
bool target_upg_download_required(char *url)
{
    (void)url;
    return true;
}
#endif

#ifndef IMPL_target_upg_command
char *target_upg_command()
{
    return NULL;
}
#endif

#ifndef IMPL_target_upg_command_full
char *target_upg_command_full()
{
    return NULL;
}
#endif

#ifndef IMPL_target_upg_command_args
char **target_upg_command_args(char *password)
{
    (void)password;
    static char *upg_command_args[] = { NULL };
    return upg_command_args;
}
#endif

#ifndef IMPL_target_upg_free_space_err
double target_upg_free_space_err()
{
    return 10000.0; /* MB */
}
#endif

#ifndef IMPL_target_upg_free_space_warn
double target_upg_free_space_warn()
{
    return 10000.0; /* MB */
}
#endif

/******************************************************************************
 * STATS
 *****************************************************************************/
#ifndef IMPL_target_stats_device_get
bool target_stats_device_get(
        dpp_device_record_t        *device_entry)
{
    static bool printed = false;
    if (!printed) {
        LOG(DEBUG, "Sending device report: stats not supported");
        printed = true;
    }
    return false;
}
#endif

#ifndef IMPL_target_stats_device_temp_get
bool target_stats_device_temp_get(
        radio_entry_t              *radio_cfg,
        dpp_device_temp_t          *temp_entry)
{
    static bool printed = false;
    if (!printed) {
        LOG(DEBUG, "Sending device report: temperature not supported");
        printed = true;
    }
    return false;
}
#endif

#ifndef IMPL_target_stats_device_txchainmask_get
bool target_stats_device_txchainmask_get(
        radio_entry_t              *radio_cfg,
        dpp_device_txchainmask_t   *txchainmask_entry)
{
    static bool printed = false;
    if (!printed) {
        LOG(DEBUG, "Sending device report: txchainmask not supported");
        printed = true;
    }
    return false;
}
#endif

#ifndef IMPL_target_stats_device_fanrpm_get
bool target_stats_device_fanrpm_get(uint32_t        *fan_rpm)
{
    static bool printed = false;
    if (!printed) {
        LOG(DEBUG, "Sending device report: FAN rpm not supported");
        printed = true;
    }
    return false;
}
#endif

#ifndef IMPL_target_get_btrace_type
btrace_type target_get_btrace_type()
{
    return BTRACE_FILE_LOG;
}
#endif

/******************************************************************************
 * MAP
 *****************************************************************************/
#ifndef IMPL_target_map_ifname_to_bandstr
const char *
target_map_ifname_to_bandstr(const char *ifname)
{
    (void)ifname;
    return NULL;
}
#endif

/******************************************************************************
 * BM and BSAL
 *****************************************************************************/
#ifndef IMPL_target_bsal_init
int target_bsal_init(bsal_event_cb_t event_cb, struct ev_loop* loop)
{
    (void)event_cb;
    (void)loop;
    return -1;
}
#endif

#ifndef IMPL_target_bsal_cleanup
int target_bsal_cleanup( void )
{
    return -1;
}
#endif

#ifndef IMPL_target_bsal_iface_add
int target_bsal_iface_add(const bsal_ifconfig_t *ifcfg)
{
    (void)ifcfg;
    return -1;
}
#endif

#ifndef IMPL_target_bsal_iface_update
int target_bsal_iface_update(const bsal_ifconfig_t *ifcfg)
{
    (void)ifcfg;
    return -1;
}
#endif

#ifndef IMPL_target_bsal_iface_remove
int target_bsal_iface_remove(const bsal_ifconfig_t *ifcfg)
{
    (void)ifcfg;
    return -1;
}
#endif

#ifndef IMPL_target_bsal_client_add
int target_bsal_client_add(const char *ifname, const uint8_t *mac_addr,
                           const bsal_client_config_t *conf)
{
    (void)ifname;
    (void)mac_addr;
    (void)conf;
    return -1;
}
#endif

#ifndef IMPL_target_bsal_client_update
int target_bsal_client_update(const char *ifname, const uint8_t *mac_addr,
                              const bsal_client_config_t *conf)
{
    (void)ifname;
    (void)mac_addr;
    (void)conf;
    return -1;
}
#endif

#ifndef IMPL_target_bsal_client_remove
int target_bsal_client_remove(const char *ifname, const uint8_t *mac_addr)
{
    (void)ifname;
    (void)mac_addr;
    return -1;
}
#endif

#ifndef IMPL_target_bsal_client_measure
int target_bsal_client_measure(const char *ifname, const uint8_t *mac_addr,
                               int num_samples)
{
    (void)ifname;
    (void)mac_addr;
    (void)num_samples;
    return -1;
}
#endif

#ifndef IMPL_target_bsal_client_disconnect
int target_bsal_client_disconnect(const char *ifname, const uint8_t *mac_addr,
                                  bsal_disc_type_t type, uint8_t reason)
{
    (void)ifname;
    (void)mac_addr;
    (void)type;
    (void)reason;
    return -1;
}
#endif

#ifndef IMPL_target_bsal_client_info
int target_bsal_client_info(const char *ifname, const uint8_t *mac_addr, bsal_client_info_t *info)
{
    (void)ifname;
    (void)mac_addr;
    (void)info;
    return -1;
}
#endif

#ifndef IMPL_target_bsal_bss_tm_request
int target_bsal_bss_tm_request(const char *ifname, const uint8_t *mac_addr,
                               const bsal_btm_params_t *btm_params)
{
    (void)ifname;
    (void)mac_addr;
    (void)btm_params;
    return -1;
}
#endif

#ifndef IMPL_target_bsal_rrm_beacon_report_request
int target_bsal_rrm_beacon_report_request(const char *ifname,
                        const uint8_t *mac_addr, const bsal_rrm_params_t *rrm_params)
{
    (void)ifname;
    (void)mac_addr;
    (void)rrm_params;
    return -1;
}
#endif

#ifndef IMPL_target_bsal_rrm_set_neighbor
int target_bsal_rrm_set_neighbor(const char *ifname, const bsal_neigh_info_t *nr)
{
    (void)ifname;
    (void)nr;
    return -1;
}
#endif

#ifndef IMPL_target_bsal_rrm_remove_neighbor
int target_bsal_rrm_remove_neighbor(const char *ifname, const bsal_neigh_info_t *nr)
{
    (void)ifname;
    (void)nr;
    return -1;
}
#endif
