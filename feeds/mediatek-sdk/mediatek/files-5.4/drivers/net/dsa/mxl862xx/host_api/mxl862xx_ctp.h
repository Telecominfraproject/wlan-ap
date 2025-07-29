/* SPDX-License-Identifier: GPL-2.0 */
/*
 * drivers/net/dsa/host_api/mxl862xx_ctp.h - Header file for DSA Driver for MaxLinear Mxl862xx switch chips family
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
#ifndef _MXL862XX_CTP_H_
#define _MXL862XX_CTP_H_

#include "mxl862xx_types.h"

#pragma pack(push, 1)
#pragma scalar_storage_order little-endian

/** \brief Logical port mode.
    Used by \ref MXL862XX_CTP_port_assignment_t. */
typedef enum {
	/** WLAN with 8-bit station ID */
	MXL862XX_LOGICAL_PORT_8BIT_WLAN = 0,
	/** WLAN with 9-bit station ID */
	MXL862XX_LOGICAL_PORT_9BIT_WLAN = 1,
	/** GPON OMCI context */
	MXL862XX_LOGICAL_PORT_GPON = 2,
	/** EPON context */
	MXL862XX_LOGICAL_PORT_EPON = 3,
	/** G.INT context */
	MXL862XX_LOGICAL_PORT_GINT = 4,
	/** Others (sub interface ID is 0 by default) */
	MXL862XX_LOGICAL_PORT_OTHER = 0xFF,
} mxl862xx_logical_port_mode_t;

/** \brief CTP Port configuration mask.
    Used by \ref MXL862XX_CTP_port_config_t. */
typedef enum {
	/** Mask for \ref MXL862XX_CTP_port_config_t::bridge_port_id */
	MXL862XX_CTP_PORT_CONFIG_MASK_BRIDGE_PORT_ID = 0x00000001,
	/** Mask for \ref MXL862XX_CTP_port_config_t::b_forced_traffic_class and
	    \ref MXL862XX_CTP_port_config_t::n_default_traffic_class */
	MXL862XX_CTP_PORT_CONFIG_MASK_FORCE_TRAFFIC_CLASS = 0x00000002,
	/** Mask for \ref MXL862XX_CTP_port_config_t::b_ingress_extended_vlan_enable and
	    \ref MXL862XX_CTP_port_config_t::n_ingress_extended_vlan_block_id */
	MXL862XX_CTP_PORT_CONFIG_MASK_INGRESS_VLAN = 0x00000004,
	/** Mask for \ref MXL862XX_CTP_port_config_t::b_ingress_extended_vlan_igmp_enable and
	    \ref MXL862XX_CTP_port_config_t::n_ingress_extended_vlan_block_id_igmp */
	MXL862XX_CTP_PORT_CONFIG_MASK_INGRESS_VLAN_IGMP = 0x00000008,
	/** Mask for \ref MXL862XX_CTP_port_config_t::b_egress_extended_vlan_enable and
	    \ref MXL862XX_CTP_port_config_t::n_egress_extended_vlan_block_id */
	MXL862XX_CTP_PORT_CONFIG_MASK_EGRESS_VLAN = 0x00000010,
	/** Mask for \ref MXL862XX_CTP_port_config_t::b_egress_extended_vlan_igmp_enable and
	    \ref MXL862XX_CTP_port_config_t::n_egress_extended_vlan_block_id_igmp */
	MXL862XX_CTP_PORT_CONFIG_MASK_EGRESS_VLAN_IGMP = 0x00000020,
	/** Mask for \ref MXL862XX_CTP_port_config_t::b_ingress_nto1Vlan_enable */
	MXL862XX_CTP_PORT_CONFIG_MASK_INRESS_NTO1_VLAN = 0x00000040,
	/** Mask for \ref MXL862XX_CTP_port_config_t::b_egress_nto1Vlan_enable */
	MXL862XX_CTP_PORT_CONFIG_MASK_EGRESS_NTO1_VLAN = 0x00000080,
	/** Mask for \ref MXL862XX_CTP_port_config_t::e_ingress_marking_mode */
	MXL862XX_CTP_PORT_CONFIG_INGRESS_MARKING = 0x00000100,
	/** Mask for \ref MXL862XX_CTP_port_config_t::e_egress_marking_mode */
	MXL862XX_CTP_PORT_CONFIG_EGRESS_MARKING = 0x00000200,
	/** Mask for \ref MXL862XX_CTP_port_config_t::b_egress_marking_override_enable and
	    \ref MXL862XX_CTP_port_config_t::e_egress_marking_mode_override */
	MXL862XX_CTP_PORT_CONFIG_EGRESS_MARKING_OVERRIDE = 0x00000400,
	/** Mask for \ref MXL862XX_CTP_port_config_t::e_egress_remarking_mode */
	MXL862XX_CTP_PORT_CONFIG_EGRESS_REMARKING = 0x00000800,
	/** Mask for \ref MXL862XX_CTP_port_config_t::b_ingress_metering_enable and
	    \ref MXL862XX_CTP_port_config_t::n_ingress_traffic_meter_id */
	MXL862XX_CTP_PORT_CONFIG_INGRESS_METER = 0x00001000,
	/** Mask for \ref MXL862XX_CTP_port_config_t::b_egress_metering_enable and
	    \ref MXL862XX_CTP_port_config_t::n_egress_traffic_meter_id */
	MXL862XX_CTP_PORT_CONFIG_EGRESS_METER = 0x00002000,
	/** Mask for \ref MXL862XX_CTP_port_config_t::b_bridging_bypass,
	    \ref MXL862XX_CTP_port_config_t::n_dest_logical_port_id,
	    \ref MXL862XX_CTP_port_config_t::b_pmapper_enable,
	    \ref MXL862XX_CTP_port_config_t::n_dest_sub_if_id_group,
	    \ref MXL862XX_CTP_port_config_t::e_pmapper_mapping_mode
	    \ref mxl862xx_bridge_port_config_t::b_pmapper_id_valid and
	    \ref MXL862XX_CTP_port_config_t::s_pmapper */
	MXL862XX_CTP_PORT_CONFIG_BRIDGING_BYPASS = 0x00004000,
	/** Mask for \ref MXL862XX_CTP_port_config_t::n_first_flow_entry_index and
	    \ref MXL862XX_CTP_port_config_t::n_number_of_flow_entries */
	MXL862XX_CTP_PORT_CONFIG_FLOW_ENTRY = 0x00008000,
	/** Mask for \ref MXL862XX_CTP_port_config_t::b_ingress_loopback_enable,
	    \ref MXL862XX_CTP_port_config_t::b_ingress_da_sa_swap_enable,
	    \ref MXL862XX_CTP_port_config_t::b_egress_loopback_enable,
	    \ref MXL862XX_CTP_port_config_t::b_egress_da_sa_swap_enable,
	    \ref MXL862XX_CTP_port_config_t::b_ingress_mirror_enable and
	    \ref MXL862XX_CTP_port_config_t::b_egress_mirror_enable */
	MXL862XX_CTP_PORT_CONFIG_LOOPBACK_AND_MIRROR = 0x00010000,

	/** Enable all fields */
	MXL862XX_CTP_PORT_CONFIG_MASK_ALL = 0x7FFFFFFF,
	/** Bypass any check for debug purpose */
	MXL862XX_CTP_PORT_CONFIG_MASK_FORCE = 0x80000000
} mxl862xx_ctp_port_config_mask_t;

/** \brief CTP Port Assignment/association with logical port.
    Used by \ref MXL862XX_CTP_PORT_ASSIGNMENT_ALLOC, \ref MXL862XX_CTP_PORT_ASSIGNMENT_SET
    and \ref MXL862XX_CTP_PORT_ASSIGNMENT_GET. */
typedef struct {
	/** Logical Port Id. The valid range is hardware dependent. */
	u8 logical_port_id;

	/** First CTP Port ID mapped to above logical port ID.

	    \remarks
	    For \ref MXL862XX_CTP_PORT_ASSIGNMENT_ALLOC, this is output when CTP
	    allocation is successful. For other APIs, this is input. */
	u16 first_ctp_port_id;
	/** Total number of CTP Ports mapped above logical port ID. */
	u16 number_of_ctp_port;

	/** Logical port mode to define sub interface ID format. */
	mxl862xx_logical_port_mode_t mode;

	/** Bridge ID (FID)

	    \remarks
	    For \ref MXL862XX_CTP_PORT_ASSIGNMENT_ALLOC, this is input. Each CTP allocated
	    is mapped to Bridge Port given by this field. The Bridge Port will be
	    configured to use first CTP
	    (\ref MXL862XX_CTP_port_assignment_t::n_first_ctp_port_id) as egress CTP.
	    For other APIs, this is ignored. */
	u16 bridge_port_id;
} mxl862xx_ctp_port_assignment_t;

/** \brief CTP Port Configuration.
    Used by \ref MXL862XX_CTP_PORT_CONFIG_SET and \ref MXL862XX_CTP_PORT_CONFIG_GET. */
typedef struct {
	/** Logical Port Id. The valid range is hardware dependent.
	    If \ref MXL862XX_CTP_port_config_t::e_mask has
	    \ref MXL862XX_Ctp_port_config_mask_t::MXL862XX_CTP_PORT_CONFIG_MASK_FORCE, this field
	    is ignored. */
	u8 logical_port_id;

	/** Sub interface ID group. The valid range is hardware/protocol dependent.

	    \remarks
	    Sub interface ID group is defined for each of \ref MXL862XX_Logical_port_mode_t.
	    For both \ref MXL862XX_LOGICAL_PORT_8BIT_WLAN and
	    \ref MXL862XX_LOGICAL_PORT_9BIT_WLAN, this field is VAP.
	    For \ref MXL862XX_LOGICAL_PORT_GPON, this field is GEM index.
	    For \ref MXL862XX_LOGICAL_PORT_EPON, this field is stream index.
	    For \ref MXL862XX_LOGICAL_PORT_GINT, this field is LLID.
	    For others, this field is 0.
	    If \ref MXL862XX_CTP_port_config_t::e_mask has
	    \ref MXL862XX_Ctp_port_config_mask_t::MXL862XX_CTP_PORT_CONFIG_MASK_FORCE, this field
	    is absolute index of CTP in hardware for debug purpose, bypassing
	    any check. */
	u16 n_sub_if_id_group;

	/** Mask for updating/retrieving fields. */
	mxl862xx_ctp_port_config_mask_t mask;

	/** Ingress Bridge Port ID to which this CTP port is associated for ingress
	    traffic. */
	u16 bridge_port_id;

	/** Default traffic class can not be overridden by other rules (except
	    traffic flow table and special tag) in processing stages. */
	bool forced_traffic_class;
	/** Default traffic class associated with all ingress traffic from this CTP
	    Port. */
	u8 default_traffic_class;

	/** Enable Extended VLAN processing for ingress non-IGMP traffic. */
	bool ingress_extended_vlan_enable;
	/** Extended VLAN block allocated for ingress non-IGMP traffic. It defines
	    extended VLAN process for ingress non-IGMP traffic. Valid when
	    b_ingress_extended_vlan_enable is TRUE. */
	u16 ingress_extended_vlan_block_id;
	/** Extended VLAN block size for ingress non-IGMP traffic. This is optional.
	    If it is 0, the block size of n_ingress_extended_vlan_block_id will be used.
	    Otherwise, this field will be used. */
	u16 ingress_extended_vlan_block_size;
	/** Enable extended VLAN processing for ingress IGMP traffic. */
	bool ingress_extended_vlan_igmp_enable;
	/** Extended VLAN block allocated for ingress IGMP traffic. It defines
	    extended VLAN process for ingress IGMP traffic. Valid when
	    b_ingress_extended_vlan_igmp_enable is TRUE. */
	u16 ingress_extended_vlan_block_id_igmp;
	/** Extended VLAN block size for ingress IGMP traffic. This is optional.
	    If it is 0, the block size of n_ingress_extended_vlan_block_id_igmp will be
	    used. Otherwise, this field will be used. */
	u16 ingress_extended_vlan_block_size_igmp;

	/** Enable extended VLAN processing for egress non-IGMP traffic. */
	bool egress_extended_vlan_enable;
	/** Extended VLAN block allocated for egress non-IGMP traffic. It defines
	    extended VLAN process for egress non-IGMP traffic. Valid when
	    b_egress_extended_vlan_enable is TRUE. */
	u16 egress_extended_vlan_block_id;
	/** Extended VLAN block size for egress non-IGMP traffic. This is optional.
	    If it is 0, the block size of n_egress_extended_vlan_block_id will be used.
	    Otherwise, this field will be used. */
	u16 egress_extended_vlan_block_size;
	/** Enable extended VLAN processing for egress IGMP traffic. */
	bool egress_extended_vlan_igmp_enable;
	/** Extended VLAN block allocated for egress IGMP traffic. It defines
	    extended VLAN process for egress IGMP traffic. Valid when
	    b_egress_extended_vlan_igmp_enable is TRUE. */
	u16 egress_extended_vlan_block_id_igmp;
	/** Extended VLAN block size for egress IGMP traffic. This is optional.
	    If it is 0, the block size of n_egress_extended_vlan_block_id_igmp will be
	    used. Otherwise, this field will be used. */
	u16 egress_extended_vlan_block_size_igmp;

	/** For WLAN type logical port, this should be FALSE. For other types, if
	     enabled and ingress packet is VLAN tagged, outer VLAN ID is used for
	    "n_sub_if_id" field in MAC table, otherwise, 0 is used for "n_sub_if_id". */
	bool ingress_nto1vlan_enable;
	/** For WLAN type logical port, this should be FALSE. For other types, if
	     enabled and egress packet is known unicast, outer VLAN ID is from
	     "n_sub_if_id" field in MAC table. */
	bool egress_nto1vlan_enable;

	/** Ingress color marking mode for ingress traffic. */
	mxl862xx_color_marking_mode_t ingress_marking_mode;
	/** Egress color marking mode for ingress traffic at egress priority queue
	    color marking stage */
	mxl862xx_color_marking_mode_t egress_marking_mode;
	/** Egress color marking mode override color marking mode from last stage. */
	bool egress_marking_override_enable;
	/** Egress color marking mode for egress traffic. Valid only when
	    b_egress_marking_override is TRUE. */
	mxl862xx_color_marking_mode_t egress_marking_mode_override;

	/** Color remarking for egress traffic. */
	mxl862xx_color_remarking_mode_t egress_remarking_mode;

	/** Traffic metering on ingress traffic applies. */
	bool ingress_metering_enable;
	/** Meter for ingress CTP process.

	    \remarks
	    Meter should be allocated with \ref MXL862XX_QOS_METER_ALLOC before CTP
	    port configuration. If this CTP port is re-set, the last used meter
	    should be released. */
	u16 ingress_traffic_meter_id;
	/** Traffic metering on egress traffic applies. */
	bool egress_metering_enable;
	/** Meter for egress CTP process.

	    \remarks
	    Meter should be allocated with \ref MXL862XX_QOS_METER_ALLOC before CTP
	    port configuration. If this CTP port is re-set, the last used meter
	    should be released. */
	u16 egress_traffic_meter_id;

	/** Ingress traffic bypass bridging/multicast processing. Following
	    parameters are used to determine destination. Traffic flow table is not
	    bypassed. */
	bool bridging_bypass;
	/** When b_bridging_bypass is TRUE, this field defines destination logical
	    port. */
	u8 dest_logical_port_id;
	/** When b_bridging_bypass is TRUE, this field indicates whether to use
	    \ref MXL862XX_CTP_port_config_t::n_dest_sub_if_id_group or
	    \ref MXL862XX_CTP_port_config_t::e_pmapper_mapping_mode/
	    \ref MXL862XX_CTP_port_config_t::s_pmapper. */
	bool pmapper_enable;
	/** When b_bridging_bypass is TRUE and b_pmapper_enable is FALSE, this field
	    defines destination sub interface ID group. */
	u16 dest_sub_if_id_group;
	/** When b_bridging_bypass is TRUE and b_pmapper_enable is TRUE, this field
	    selects either DSCP or PCP to derive sub interface ID. */
	mxl862xx_pmapper_mapping_mode_t pmapper_mapping_mode;
	/** When b_pmapper_enable is TRUE, P-mapper is used. This field determines
	    whether s_pmapper.n_pmapper_id is valid. If this field is TRUE, the
	    P-mapper is re-used and no allocation of new P-mapper or value
	    change in the P-mapper. If this field is FALSE, allocation is
	    taken care in the API implementation. */
	bool pmapper_id_valid;
	/** When b_bridging_bypass is TRUE and b_pmapper_enable is TRUE, P-mapper is
	    used. If b_pmapper_id_valid is FALSE, API implementation need take care
	    of P-mapper allocation, and maintain the reference counter of
	    P-mapper used multiple times. If b_pmapper_id_valid is TRUE, only
	    s_pmapper.n_pmapper_id is used to associate the P-mapper, and there is
	    no allocation of new P-mapper or value change in the P-mapper. */
	mxl862xx_pmapper_t pmapper;

	/** First traffic flow table entry is associated to this CTP port. Ingress
	    traffic from this CTP port will go through traffic flow table search
	    starting from n_first_flow_entry_index. Should be times of 4. */
	u16 first_flow_entry_index;
	/** Number of traffic flow table entries are associated to this CTP port.
	    Ingress traffic from this CTP port will go through PCE rules search
	    ending at (n_first_flow_entry_index+n_number_of_flow_entries)-1. Should
	    be times of 4. */
	u16 number_of_flow_entries;

	/** Ingress traffic from this CTP port will be redirected to ingress
	    logical port of this CTP port with source sub interface ID used as
	    destination sub interface ID. Following processing except traffic
	    flow table search is bypassed if loopback enabled. */
	bool ingress_loopback_enable;
	/** Destination/Source MAC address of ingress traffic is swapped before
	    transmitted (not swapped during PCE processing stages). If destination
	    is multicast, there is no swap, but source MAC address is replaced
	    with global configurable value. */
	bool ingress_da_sa_swap_enable;
	/** Egress traffic to this CTP port will be redirected to ingress logical
	    port with same sub interface ID as ingress. */
	bool egress_loopback_enable;
	/** Destination/Source MAC address of egress traffic is swapped before
	    transmitted. */
	bool egress_da_sa_swap_enable;

	/** If enabled, ingress traffic is mirrored to the monitoring port.
	    \remarks
	    This should be used exclusive with b_ingress_loopback_enable. */
	bool ingress_mirror_enable;
	/** If enabled, egress traffic is mirrored to the monitoring port.
	    \remarks
	    This should be used exclusive with b_egress_loopback_enable. */
	bool egress_mirror_enable;
} mxl862xx_ctp_port_config_t;

#pragma scalar_storage_order default
#pragma pack(pop)

#endif /*_MXL862XX_CTP_H */
