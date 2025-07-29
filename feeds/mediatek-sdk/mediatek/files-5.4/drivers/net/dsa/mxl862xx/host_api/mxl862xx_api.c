// SPDX-License-Identifier: GPL-2.0
/*
 * drivers/net/dsa/host_api/mxl862xx_api.c - dsa driver for Maxlinear mxl862xx switch chips family
 *
 * Copyright (C) 2024 MaxLinear Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include "mxl862xx_host_api_impl.h"
#include "mxl862xx_api.h"
#include "mxl862xx_mmd_apis.h"

int sys_misc_fw_version(const mxl862xx_device_t *dev,
			struct sys_fw_image_version *sys_img_ver)
{
	return mxl862xx_api_wrap(dev, SYS_MISC_FW_VERSION, sys_img_ver,
			    sizeof(*sys_img_ver), 0, sizeof(*sys_img_ver));
}

int mxl862xx_register_mod(const mxl862xx_device_t *dev, mxl862xx_register_mod_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_COMMON_REGISTERMOD, parm, sizeof(*parm), 0,
			    0);
}

int mxl862xx_port_link_cfg_set(const mxl862xx_device_t *dev, mxl862xx_port_link_cfg_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_COMMON_PORTLINKCFGSET, parm, sizeof(*parm),
			    MXL862XX_COMMON_PORTLINKCFGGET, 0);
}

int mxl862xx_port_cfg_get(const mxl862xx_device_t *dev, mxl862xx_port_cfg_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_COMMON_PORTCFGGET, parm, sizeof(*parm), 0,
			    sizeof(*parm));
}

int mxl862xx_port_cfg_set(const mxl862xx_device_t *dev, mxl862xx_port_cfg_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_COMMON_PORTCFGSET, parm, sizeof(*parm),
			    MXL862XX_COMMON_PORTCFGGET, 0);
}

int mxl862xx_bridge_alloc(const mxl862xx_device_t *dev, mxl862xx_bridge_alloc_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_BRIDGE_ALLOC, parm, sizeof(*parm), 0,
			    sizeof(*parm));
}
int mxl862xx_bridge_free(const mxl862xx_device_t *dev, mxl862xx_bridge_alloc_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_BRIDGE_FREE, parm, sizeof(*parm), 0, 0);
}

int mxl862xx_bridge_config_set(const mxl862xx_device_t *dev, mxl862xx_bridge_config_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_BRIDGE_CONFIGSET, parm, sizeof(*parm),
			    MXL862XX_BRIDGE_CONFIGGET, 0);
}

int mxl862xx_bridge_config_get(const mxl862xx_device_t *dev, mxl862xx_bridge_config_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_BRIDGE_CONFIGGET, parm, sizeof(*parm), 0,
			    sizeof(*parm));
}

int mxl862xx_bridge_port_alloc(const mxl862xx_device_t *dev, mxl862xx_bridge_port_alloc_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_BRIDGEPORT_ALLOC, parm, sizeof(*parm), 0,
			    sizeof(*parm));
}

int mxl862xx_bridge_port_free(const mxl862xx_device_t *dev, mxl862xx_bridge_port_alloc_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_BRIDGEPORT_FREE, parm, sizeof(*parm), 0, 0);
}

int mxl862xx_bridge_port_config_set(const mxl862xx_device_t *dev,
				    mxl862xx_bridge_port_config_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_BRIDGEPORT_CONFIGSET, parm, sizeof(*parm),
			    MXL862XX_BRIDGEPORT_CONFIGGET, 0);
}

int mxl862xx_bridge_port_config_get(const mxl862xx_device_t *dev,
				    mxl862xx_bridge_port_config_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_BRIDGEPORT_CONFIGGET, parm, sizeof(*parm),
			    0, sizeof(*parm));
}

int mxl862xx_debug_rmon_port_get(const mxl862xx_device_t *dev,
				 mxl862xx_debug_rmon_port_cnt_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_DEBUG_RMON_PORT_GET, parm, sizeof(*parm),
			    0, sizeof(*parm));
}

int mxl862xx_mac_table_clear(const mxl862xx_device_t *dev)
{
	return mxl862xx_api_wrap(dev, MXL862XX_MAC_TABLECLEAR, NULL, 0, 0, 0);
}

int mxl862xx_mac_table_clear_cond(const mxl862xx_device_t *dev,
				  mxl862xx_mac_table_clear_cond_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_MAC_TABLECLEARCOND, parm, sizeof(*parm), 0,
			    0);
}

int mxl862xx_mac_table_entry_read(const mxl862xx_device_t *dev,
				  mxl862xx_mac_table_read_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_MAC_TABLEENTRYREAD, parm, sizeof(*parm), 0,
			    sizeof(*parm));
}

int mxl862xx_mac_table_entry_add(const mxl862xx_device_t *dev,
				 mxl862xx_mac_table_add_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_MAC_TABLEENTRYADD, parm, sizeof(*parm), 0,
			    0);
}

int mxl862xx_mac_table_entry_remove(const mxl862xx_device_t *dev,
				    mxl862xx_mac_table_remove_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_MAC_TABLEENTRYREMOVE, parm, sizeof(*parm),
			    0, 0);
}

int mxl862xx_stp_port_cfg_set(const mxl862xx_device_t *dev, mxl862xx_stp_port_cfg_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_STP_PORTCFGSET, parm, sizeof(*parm),
			    MXL862XX_STP_PORTCFGGET, 0);
}

int mxl862xx_ss_sp_tag_get(const mxl862xx_device_t *dev, mxl862xx_ss_sp_tag_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_SS_SPTAG_GET, parm, sizeof(*parm), 0,
			    sizeof(*parm));
}

int mxl862xx_ss_sp_tag_set(const mxl862xx_device_t *dev, mxl862xx_ss_sp_tag_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_SS_SPTAG_SET, parm, sizeof(*parm),
			    MXL862XX_SS_SPTAG_GET, sizeof(*parm));
}

int mxl862xx_ctp_port_config_get(const mxl862xx_device_t *dev,
				 mxl862xx_ctp_port_config_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_CTP_PORTCONFIGGET, parm, sizeof(*parm), 0,
			    sizeof(*parm));
}

int mxl862xx_ctp_port_config_set(const mxl862xx_device_t *dev,
				 mxl862xx_ctp_port_config_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_CTP_PORTCONFIGSET, parm, sizeof(*parm),
			    MXL862XX_CTP_PORTCONFIGGET, 0);
}

int mxl862xx_ctp_port_assignment_set(const mxl862xx_device_t *dev,
				     mxl862xx_ctp_port_assignment_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_CTP_PORTASSIGNMENTSET, parm, sizeof(*parm),
			    MXL862XX_CTP_PORTASSIGNMENTGET, 0);
}

int mxl862xx_ctp_port_assignment_get(const mxl862xx_device_t *dev,
				     mxl862xx_ctp_port_assignment_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_CTP_PORTASSIGNMENTGET, parm, sizeof(*parm),
			    0, sizeof(*parm));
}

int mxl862xx_monitor_port_cfg_get(const mxl862xx_device_t *dev,
				  mxl862xx_monitor_port_cfg_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_COMMON_MONITORPORTCFGGET, parm,
			    sizeof(*parm), 0, sizeof(*parm));
}

int mxl862xx_monitor_port_cfg_set(const mxl862xx_device_t *dev,
				  mxl862xx_monitor_port_cfg_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_COMMON_MONITORPORTCFGSET, parm,
			    sizeof(*parm), MXL862XX_COMMON_MONITORPORTCFGGET, 0);
}

int mxl862xx_extended_vlan_alloc(const mxl862xx_device_t *dev,
				 mxl862xx_extendedvlan_alloc_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_EXTENDEDVLAN_ALLOC, parm, sizeof(*parm), 0,
			    sizeof(*parm));
}

int mxl862xx_extended_vlan_set(const mxl862xx_device_t *dev,
			       mxl862xx_extendedvlan_config_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_EXTENDEDVLAN_SET, parm, sizeof(*parm),
			    MXL862XX_EXTENDEDVLAN_GET, 0);
}

int mxl862xx_extended_vlan_get(const mxl862xx_device_t *dev,
			       mxl862xx_extendedvlan_config_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_EXTENDEDVLAN_GET, parm, sizeof(*parm), 0,
			    sizeof(*parm));
}

int mxl862xx_extended_vlan_free(const mxl862xx_device_t *dev,
				mxl862xx_extendedvlan_alloc_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_EXTENDEDVLAN_FREE, parm, sizeof(*parm), 0,
			    0);
}

int mxl862xx_vlan_filter_alloc(const mxl862xx_device_t *dev,
			       mxl862xx_vlanfilter_alloc_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_VLANFILTER_ALLOC, parm, sizeof(*parm), 0,
			    sizeof(*parm));
}

int mxl862xx_vlan_filter_set(const mxl862xx_device_t *dev,
			     mxl862xx_vlanfilter_config_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_VLANFILTER_SET, parm, sizeof(*parm),
			    MXL862XX_VLANFILTER_GET, 0);
}

int mxl862xx_vlan_filter_get(const mxl862xx_device_t *dev,
			     mxl862xx_vlanfilter_config_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_VLANFILTER_GET, parm, sizeof(*parm), 0,
			    sizeof(*parm));
}

int mxl862xx_vlan_filter_free(const mxl862xx_device_t *dev,
			      mxl862xx_vlanfilter_alloc_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_VLANFILTER_FREE, parm, sizeof(*parm), 0,
			    0);
}

int mxl862xx_cfg_get(const mxl862xx_device_t *dev, mxl862xx_cfg_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_COMMON_CFGGET, parm, sizeof(*parm), 0,
			    sizeof(*parm));
}

int mxl862xx_cfg_set(const mxl862xx_device_t *dev, mxl862xx_cfg_t *parm)
{
	return mxl862xx_api_wrap(dev, MXL862XX_COMMON_CFGSET, parm, sizeof(*parm),
				MXL862XX_COMMON_CFGGET, 0);
}

