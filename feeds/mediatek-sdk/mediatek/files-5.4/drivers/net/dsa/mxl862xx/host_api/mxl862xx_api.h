// SPDX-License-Identifier: GPL-2.0
/*
 * drivers/net/dsa/host_api/mxl862xx_api.h - DSA Driver for MaxLinear Mxl862xx switch chips family
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

#ifndef _MXL862XX_API_H_
#define _MXL862XX_API_H_

#include "mxl862xx_types.h"
#include "mxl862xx_rmon.h"

#define MxL862XX_SDMA_PCTRLp(p) (0xBC0 + ((p) * 0x6))
#define MXL862XX_SDMA_PCTRL_EN BIT(0) /* SDMA Port Enable */

/* Ethernet Switch Fetch DMA Port Control Register */
#define MxL862XX_FDMA_PCTRLp(p) (0xA80 + ((p) * 0x6))
#define MXL862XX_FDMA_PCTRL_EN BIT(0) /* FDMA Port Enable */

#pragma pack(push, 1)
#pragma scalar_storage_order little-endian

//#define SCK_FW_1_0_41

/** \brief Spanning Tree Protocol port states.
    Used by \ref MXL862XX_STP_port_cfg_t. */
typedef enum {
	/** Forwarding state. The port is allowed to transmit and receive
	    all packets. Address Learning is allowed. */
	MXL862XX_STP_PORT_STATE_FORWARD = 0,
	/** Disabled/Discarding state. The port entity will not transmit
	    and receive any packets. Learning is disabled in this state. */
	MXL862XX_STP_PORT_STATE_DISABLE = 1,
	/** Learning state. The port entity will only transmit and receive
	    Spanning Tree Protocol packets (BPDU). All other packets are discarded.
	    MAC table address learning is enabled for all good frames. */
	MXL862XX_STP_PORT_STATE_LEARNING = 2,
	/** Blocking/Listening. Only the Spanning Tree Protocol packets will
	    be received and transmitted. All other packets are discarded by
	    the port entity. MAC table address learning is disabled in this
	    state. */
	MXL862XX_STP_PORT_STATE_BLOCKING = 3
} mxl862xx_stp_port_state_t;

/** \brief Bridge Port Allocation.
    Used by \ref MXL862XX_BRIDGE_PORT_ALLOC and \ref MXL862XX_BRIDGE_PORT_FREE. */
typedef struct {
	/** If \ref MXL862XX_BRIDGE_PORT_ALLOC is successful, a valid ID will be returned
	    in this field. Otherwise, \ref INVALID_HANDLE is returned in this field.
	    For \ref MXL862XX_BRIDGE_PORT_FREE, this field should be valid ID returned by
	    \ref MXL862XX_BRIDGE_PORT_ALLOC. ID 0 is special for CPU port in PRX300
	    by mapping to CTP 0 (Logical Port 0 with Sub-interface ID 0), and
	    pre-alloced during initialization. */
	u16 bridge_port_id;
} mxl862xx_bridge_port_alloc_t;

/** \brief Color Remarking Mode
    Used by \ref MXL862XX_CTP_port_config_t. */
typedef enum {
	/** values from last process stage */
	MXL862XX_REMARKING_NONE = 0,
	/** DEI mark mode */
	MXL862XX_REMARKING_DEI = 2,
	/** PCP 8P0D mark mode */
	MXL862XX_REMARKING_PCP_8P0D = 3,
	/** PCP 7P1D mark mode */
	MXL862XX_REMARKING_PCP_7P1D = 4,
	/** PCP 6P2D mark mode */
	MXL862XX_REMARKING_PCP_6P2D = 5,
	/** PCP 5P3D mark mode */
	MXL862XX_REMARKING_PCP_5P3D = 6,
	/** DSCP AF class */
	MXL862XX_REMARKING_DSCP_AF = 7
} mxl862xx_color_remarking_mode_t;

/** \brief Meters for various egress traffic type.
    Used by \ref MXL862XX_BRIDGE_port_config_t. */
typedef enum {
	/** Index of broadcast traffic meter */
	MXL862XX_BRIDGE_PORT_EGRESS_METER_BROADCAST = 0,
	/** Index of known multicast traffic meter */
	MXL862XX_BRIDGE_PORT_EGRESS_METER_MULTICAST = 1,
	/** Index of unknown multicast IP traffic meter */
	MXL862XX_BRIDGE_PORT_EGRESS_METER_UNKNOWN_MC_IP = 2,
	/** Index of unknown multicast non-IP traffic meter */
	MXL862XX_BRIDGE_PORT_EGRESS_METER_UNKNOWN_MC_NON_IP = 3,
	/** Index of unknown unicast traffic meter */
	MXL862XX_BRIDGE_PORT_EGRESS_METER_UNKNOWN_UC = 4,
	/** Index of traffic meter for other types */
	MXL862XX_BRIDGE_PORT_EGRESS_METER_OTHERS = 5,
	/** Number of index */
	MXL862XX_BRIDGE_PORT_EGRESS_METER_MAX = 6
} mxl862xx_bridge_port_egress_meter_t;

/** \brief P-mapper Mapping Mode
    Used by \ref MXL862XX_CTP_port_config_t. */
typedef enum {
	/** Use PCP for VLAN tagged packets to derive sub interface ID group.

	    \remarks
	    P-mapper table entry 1-8. */
	MXL862XX_PMAPPER_MAPPING_PCP = 0,
	/** Use LAG Index for Pmapper access (regardless of IP and VLAN packet)to
	    derive sub interface ID group.

	    \remarks
	    P-mapper table entry 9-72. */
	MXL862XX_PMAPPER_MAPPING_LAG = 1,
	/** Use DSCP for VLAN tagged IP packets to derive sub interface ID group.

	    \remarks
	    P-mapper table entry 9-72. */
	MXL862XX_PMAPPER_MAPPING_DSCP = 2,
} mxl862xx_pmapper_mapping_mode_t;

/** \brief P-mapper Configuration
    Used by \ref MXL862XX_CTP_port_config_t, MXL862XX_BRIDGE_port_config_t.
    In case of LAG, it is user's responsibility to provide the mapped entries
    in given P-mapper table. In other modes the entries are auto mapped from
    input packet. */
typedef struct {
	/** Index of P-mapper <0-31>. */
	u16 pmapper_id;

	/** Sub interface ID group.

	    \remarks
	    Entry 0 is for non-IP and non-VLAN tagged packets. Entries 1-8 are PCP
	    mapping entries for VLAN tagged packets with \ref MXL862XX_PMAPPER_MAPPING_PCP
	    selected. Entries 9-72 are DSCP or LAG mapping entries for IP packets without
	    VLAN tag or VLAN tagged packets with \ref MXL862XX_PMAPPER_MAPPING_DSCP or
	    MXL862XX_PMAPPER_MAPPING_LAG selected. When LAG is selected this 8bit field is
	    decoded as Destination sub-interface ID group field bits 3:0, Destination
	    logical port ID field bits 7:4 */
	u8 dest_sub_if_id_group[73];
} mxl862xx_pmapper_t;

/** \brief Bridge Port configuration mask.
    Used by \ref MXL862XX_BRIDGE_port_config_t. */
typedef enum {
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::n_bridge_id */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_BRIDGE_ID = 0x00000001,
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::b_ingress_extended_vlan_enable and
	    \ref MXL862XX_BRIDGE_port_config_t::n_ingress_extended_vlan_block_id */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_INGRESS_VLAN = 0x00000002,
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::b_egress_extended_vlan_enable and
	    \ref MXL862XX_BRIDGE_port_config_t::n_egress_extended_vlan_block_id */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_EGRESS_VLAN = 0x00000004,
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::e_ingress_marking_mode */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_INGRESS_MARKING = 0x00000008,
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::e_egress_remarking_mode */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_EGRESS_REMARKING = 0x00000010,
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::b_ingress_metering_enable and
	    \ref MXL862XX_BRIDGE_port_config_t::n_ingress_traffic_meter_id */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_INGRESS_METER = 0x00000020,
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::b_egress_sub_metering_enable and
	    \ref MXL862XX_BRIDGE_port_config_t::n_egress_traffic_sub_meter_id */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_EGRESS_SUB_METER = 0x00000040,
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::n_dest_logical_port_id,
	    \ref MXL862XX_BRIDGE_port_config_t::b_pmapper_enable,
	    \ref MXL862XX_BRIDGE_port_config_t::n_dest_sub_if_id_group,
	    \ref MXL862XX_BRIDGE_port_config_t::e_pmapper_mapping_mode,
	    \ref MXL862XX_BRIDGE_port_config_t::b_pmapper_id_valid and
	    \ref MXL862XX_BRIDGE_port_config_t::s_pmapper. */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_EGRESS_CTP_MAPPING = 0x00000080,
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::n_bridge_port_map */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_BRIDGE_PORT_MAP = 0x00000100,

	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::b_mc_dest_ip_lookup_enable. */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_MC_DEST_IP_LOOKUP = 0x00000200,
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::b_mc_src_ip_lookup_enable. */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_MC_SRC_IP_LOOKUP = 0x00000400,
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::b_dest_mac_lookup_enable. */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_MC_DEST_MAC_LOOKUP = 0x00000800,
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::b_src_mac_learning_enable. */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_MC_SRC_MAC_LEARNING = 0x00001000,
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::b_mac_spoofing_detect_enable. */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_MAC_SPOOFING = 0x00002000,
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::b_port_lock_enable. */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_PORT_LOCK = 0x00004000,

	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::b_mac_learning_limit_enable and
	    \ref MXL862XX_BRIDGE_port_config_t::n_mac_learning_limit. */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_MAC_LEARNING_LIMIT = 0x00008000,
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::n_mac_learning_count */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_MAC_LEARNED_COUNT = 0x00010000,

	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::b_ingress_vlan_filter_enable and
	    \ref MXL862XX_BRIDGE_port_config_t::n_ingress_vlan_filter_block_id. */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_INGRESS_VLAN_FILTER = 0x00020000,
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::b_bypass_egress_vlan_filter1,
	    \ref MXL862XX_BRIDGE_port_config_t::b_egress_vlan_filter1Enable
	    and \ref MXL862XX_BRIDGE_port_config_t::n_egress_vlan_filter1Block_id. */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_EGRESS_VLAN_FILTER1 = 0x00040000,
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::b_egress_vlan_filter2Enable and
	    \ref MXL862XX_BRIDGE_port_config_t::n_egress_vlan_filter2Block_id. */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_EGRESS_VLAN_FILTER2 = 0x00080000,
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::b_vlan_tag_selection,
	    \ref MXL862XX_BRIDGE_port_config_t::b_vlan_src_mac_priority_enable,
	    \ref MXL862XX_BRIDGE_port_config_t::b_vlan_src_mac_dEIEnable,
	    \ref MXL862XX_BRIDGE_port_config_t::b_vlan_src_mac_vid_enable,
	    \ref MXL862XX_BRIDGE_port_config_t::b_vlan_dst_mac_priority_enable,
	    \ref MXL862XX_BRIDGE_port_config_t::b_vlan_dst_mac_dEIEnable,
	    \ref MXL862XX_BRIDGE_port_config_t::b_vlan_dst_mac_vid_enable */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_VLAN_BASED_MAC_LEARNING = 0x00100000,
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::b_vlan_multicast_priority_enable,
	    \ref MXL862XX_BRIDGE_port_config_t::b_vlan_multicast_dEIEnable,
	    \ref MXL862XX_BRIDGE_port_config_t::b_vlan_multicast_vid_enable */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_VLAN_BASED_MULTICAST_LOOKUP = 0x00200000,
	/** Mask for \ref MXL862XX_BRIDGE_port_config_t::n_loop_violation_count */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_LOOP_VIOLATION_COUNTER = 0x00400000,
	/** Enable all */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_ALL = 0x7FFFFFFF,
	/** Bypass any check for debug purpose */
	MXL862XX_BRIDGE_PORT_CONFIG_MASK_FORCE = 0x80000000
} mxl862xx_bridge_port_config_mask_t;

/** \brief Color Marking Mode
    Used by \ref MXL862XX_CTP_port_config_t. */
typedef enum {
	/** mark packets (except critical) to green */
	MXL862XX_MARKING_ALL_GREEN = 0,
	/** do not change color and priority */
	MXL862XX_MARKING_INTERNAL_MARKING = 1,
	/** DEI mark mode */
	MXL862XX_MARKING_DEI = 2,
	/** PCP 8P0D mark mode */
	MXL862XX_MARKING_PCP_8P0D = 3,
	/** PCP 7P1D mark mode */
	MXL862XX_MARKING_PCP_7P1D = 4,
	/** PCP 6P2D mark mode */
	MXL862XX_MARKING_PCP_6P2D = 5,
	/** PCP 5P3D mark mode */
	MXL862XX_MARKING_PCP_5P3D = 6,
	/** DSCP AF class */
	MXL862XX_MARKING_DSCP_AF = 7
} mxl862xx_color_marking_mode_t;

/** \brief Bridge Port Configuration.
    Used by \ref MXL862XX_BRIDGE_PORT_CONFIG_SET and \ref MXL862XX_BRIDGE_PORT_CONFIG_GET. */
typedef struct {
	/** Bridge Port ID allocated by \ref MXL862XX_BRIDGE_PORT_ALLOC.

	    \remarks
	    If \ref MXL862XX_BRIDGE_port_config_t::e_mask has
	    \ref MXL862XX_Bridge_port_config_mask_t::MXL862XX_BRIDGE_PORT_CONFIG_MASK_FORCE, this
	    field is absolute index of Bridge Port in hardware for debug purpose,
	    bypassing any check. */
	u16 bridge_port_id;

	/** Mask for updating/retrieving fields. */
	mxl862xx_bridge_port_config_mask_t mask;

	/** Bridge ID (FID) to which this bridge port is associated. A default
	    bridge (ID 0) should be always available. */
	u16 bridge_id;

	/** Enable extended VLAN processing for ingress non-IGMP traffic. */
	bool ingress_extended_vlan_enable;
	/** Extended VLAN block allocated for ingress non-IGMP traffic. It defines
	    extended VLAN process for ingress non-IGMP traffic. Valid when
	    b_ingress_extended_vlan_enable is TRUE. */
	u16 ingress_extended_vlan_block_id;
	/** Extended VLAN block size for ingress non-IGMP traffic. This is optional.
	    If it is 0, the block size of n_ingress_extended_vlan_block_id will be used.
	    Otherwise, this field will be used. */
	u16 ingress_extended_vlan_block_size;

	/** Enable extended VLAN processing enabled for egress non-IGMP traffic. */
	bool egress_extended_vlan_enable;
	/** Extended VLAN block allocated for egress non-IGMP traffic. It defines
	    extended VLAN process for egress non-IGMP traffic. Valid when
	    b_egress_extended_vlan_enable is TRUE. */
	u16 egress_extended_vlan_block_id;
	/** Extended VLAN block size for egress non-IGMP traffic. This is optional.
	    If it is 0, the block size of n_egress_extended_vlan_block_id will be used.
	    Otherwise, this field will be used. */
	u16 egress_extended_vlan_block_size;

	/** Ingress color marking mode for ingress traffic. */
	mxl862xx_color_marking_mode_t ingress_marking_mode;

	/** Color remarking for egress traffic. */
	mxl862xx_color_remarking_mode_t egress_remarking_mode;

	/** Traffic metering on ingress traffic applies. */
	bool ingress_metering_enable;
	/** Meter for ingress Bridge Port process.

	    \remarks
	    Meter should be allocated with \ref MXL862XX_QOS_METER_ALLOC before Bridge
	    port configuration. If this Bridge port is re-set, the last used meter
	    should be released. */
	u16 ingress_traffic_meter_id;

	/** Traffic metering on various types of egress traffic (such as broadcast,
	    multicast, unknown unicast, etc) applies. */
	bool egress_sub_metering_enable[MXL862XX_BRIDGE_PORT_EGRESS_METER_MAX];
	/** Meter for egress Bridge Port process with specific type (such as
	    broadcast, multicast, unknown unicast, etc). Need pre-allocated for each
	    type. */
	u16 egress_traffic_sub_meter_id[MXL862XX_BRIDGE_PORT_EGRESS_METER_MAX];

	/** This field defines destination logical port. */
	u8 dest_logical_port_id;
	/** This field indicates whether to enable P-mapper. */
	bool pmapper_enable;
	/** When b_pmapper_enable is FALSE, this field defines destination sub
	    interface ID group. */
	u16 dest_sub_if_id_group;
	/** When b_pmapper_enable is TRUE, this field selects either DSCP or PCP to
	    derive sub interface ID. */
	mxl862xx_pmapper_mapping_mode_t pmapper_mapping_mode;
	/** When b_pmapper_enable is TRUE, P-mapper is used. This field determines
	    whether s_pmapper.n_pmapper_id is valid. If this field is TRUE, the
	    P-mapper is re-used and no allocation of new P-mapper or value
	    change in the P-mapper. If this field is FALSE, allocation is
	    taken care in the API implementation. */
	bool pmapper_id_valid;
	/** When b_pmapper_enable is TRUE, P-mapper is used. if b_pmapper_id_valid is
	    FALSE, API implementation need take care of P-mapper allocation,
	    and maintain the reference counter of P-mapper used multiple times.
	    If b_pmapper_id_valid is TRUE, only s_pmapper.n_pmapper_id is used to
	    associate the P-mapper, and there is no allocation of new P-mapper
	    or value change in the P-mapper. */
	mxl862xx_pmapper_t pmapper;

	/** Port map define broadcast domain.

	    \remarks
	    Each bit is one bridge port. Bridge port ID is index * 16 + bit offset.
	    For example, bit 1 of n_bridge_port_map[1] is bridge port ID 17. */
	u16 bridge_port_map[8]; /* max can be 16 */

	/** Multicast IP table is searched if this field is FALSE and traffic is IP
	    multicast. */
	bool mc_dest_ip_lookup_disable;
	/** Multicast IP table is searched if this field is TRUE and traffic is IP
	    multicast. */
	bool mc_src_ip_lookup_enable;

	/** Default is FALSE. Packet is treated as "unknown" if it's not
	    broadcast/multicast packet. */
	bool dest_mac_lookup_disable;

	/** Default is FALSE. Source MAC address is learned. */
	bool src_mac_learning_disable;

	/** If this field is TRUE and MAC address which is already learned in another
	    bridge port appears on this bridge port, port locking violation is
	    detected. */
	bool mac_spoofing_detect_enable;

	/** If this field is TRUE and MAC address which is already learned in this
	    bridge port appears on another bridge port, port locking violation is
	    detected. */
	bool port_lock_enable;

	/** Enable MAC learning limitation. */
	bool mac_learning_limit_enable;
	/** Max number of MAC can be learned from this bridge port. */
	u16 mac_learning_limit;

	/** Get number of Loop violation counter from this bridge port. */
	u16 loop_violation_count;

	/** Get number of MAC address learned from this bridge port. */
	u16 mac_learning_count;

	/** Enable ingress VLAN filter */
	bool ingress_vlan_filter_enable;
	/** VLAN filter block of ingress traffic if
	    \ref MXL862XX_BRIDGE_port_config_t::b_ingress_vlan_filter_enable is TRUE. */
	u16 ingress_vlan_filter_block_id;
	/** VLAN filter block size. This is optional.
	    If it is 0, the block size of n_ingress_vlan_filter_block_id will be used.
	    Otherwise, this field will be used. */
	u16 ingress_vlan_filter_block_size;
	/** For ingress traffic, bypass VLAN filter 1 at egress bridge port
	    processing. */
	bool bypass_egress_vlan_filter1;
	/** Enable egress VLAN filter 1 */
	bool egress_vlan_filter1enable;
	/** VLAN filter block 1 of egress traffic if
	    \ref MXL862XX_BRIDGE_port_config_t::b_egress_vlan_filter1Enable is TRUE. */
	u16 egress_vlan_filter1block_id;
	/** VLAN filter block 1 size. This is optional.
	    If it is 0, the block size of n_egress_vlan_filter1Block_id will be used.
	    Otherwise, this field will be used. */
	u16 egress_vlan_filter1block_size;
	/** Enable egress VLAN filter 2 */
	bool egress_vlan_filter2enable;
	/** VLAN filter block 2 of egress traffic if
	    \ref MXL862XX_BRIDGE_port_config_t::b_egress_vlan_filter2Enable is TRUE. */
	u16 egress_vlan_filter2block_id;
	/** VLAN filter block 2 size. This is optional.
	    If it is 0, the block size of n_egress_vlan_filter2Block_id will be used.
	    Otherwise, this field will be used. */
	u16 egress_vlan_filter2block_size;
#ifdef SCK_FW_1_0_41
	/** Enable Ingress VLAN Based Mac Learning */
	bool ingress_vlan_based_mac_learning_enable;
	/** 0 - Intermediate outer VLAN
	    tag is used for MAC address/multicast
	    learning, lookup and filtering.
	    1 - Original outer VLAN tag is used
	    for MAC address/multicast learning, lookup
	    and filtering. */
#endif
	bool vlan_tag_selection;
	/** 0 - Disable, VLAN Priority field is not used
	    and value 0 is used for source MAC address
	    learning and filtering.
	    1 - Enable, VLAN Priority field is used for
	    source MAC address learning and filtering. */
	bool vlan_src_mac_priority_enable;
	/** 0 - Disable, VLAN DEI/CFI field is not used
	    and value 0 is used for source MAC address
	    learning and filtering.
	    1 -	 Enable, VLAN DEI/CFI field is used for
	    source MAC address learning and filtering */
	bool vlan_src_mac_dei_enable;
	/** 0 - Disable, VLAN ID field is not used and
	    value 0 is used for source MAC address
	    learning and filtering
	    1 - Enable, VLAN ID field is used for source
	    MAC address learning and filtering. */
	bool vlan_src_mac_vid_enable;
	/** 0 - Disable, VLAN Priority field is not used
	    and value 0 is used for destination MAC
	    address look up and filtering.
	    1 - Enable, VLAN Priority field is used for
	    destination MAC address look up and
	    filtering */
	bool vlan_dst_mac_priority_enable;
	/** 0 - Disable, VLAN CFI/DEI field is not used
	    and value 0 is used for destination MAC
	    address lookup and filtering.
	    1 - Enable, VLAN CFI/DEI field is used for
	    destination MAC address look up and
	    filtering. */
	bool vlan_dst_mac_dei_enable;
	/** 0 - Disable, VLAN ID field is not used and
	    value 0 is used for destination MAC address
	    look up and filtering.
	    1 - Enable, VLAN ID field is destination for
	    destination MAC address look up and
	    filtering. */
	bool vlan_dst_mac_vid_enable;
#ifdef SCK_FW_1_0_41
	/** Enable, VLAN Based Multicast Lookup */
	bool vlan_based_multi_cast_lookup;
	/** 0 - Disable, VLAN Priority field is not used
	    and value 0 is used for IP multicast lookup.
	    1 - Enable, VLAN Priority field is used for IP
	    multicast lookup. */
#endif
	bool vlan_multicast_priority_enable;
	/** 0 - Disable, VLAN CFI/DEI field is not used
	    and value 0 is used for IP multicast lookup.
	    1 - Enable, VLAN CFI/DEI field is used for IP
	    multicast lookup. */
	bool vlan_multicast_dei_enable;
	/** 0 - Disable, VLAN ID field is not used and
	    value 0 is used for IP multicast lookup.
	    1 - Enable, VLAN ID field is destination for IP
	    multicast lookup. */
	bool vlan_multicast_vid_enable;
} mxl862xx_bridge_port_config_t;

/** \brief Bridge Allocation.
    Used by \ref MXL862XX_BRIDGE_ALLOC and \ref MXL862XX_BRIDGE_FREE. */
typedef struct {
	/** If \ref MXL862XX_BRIDGE_ALLOC is successful, a valid ID will be returned
	    in this field. Otherwise, \ref INVALID_HANDLE is returned in this field.
	    For \ref MXL862XX_BRIDGE_FREE, this field should be valid ID returned by
	    \ref MXL862XX_BRIDGE_ALLOC. ID 0 is special Bridge created during
	    initialization. */
	u16 bridge_id;
} mxl862xx_bridge_alloc_t;

/** \brief Ethernet port speed mode.
    For certain generations of GSWIP, a port might support only a subset of the possible settings.
    Used by \ref MXL862XX_port_link_cfg_t. */
typedef enum {
	/** 10 Mbit/s */
	MXL862XX_PORT_SPEED_10,
	/** 100 Mbit/s */
	MXL862XX_PORT_SPEED_100,
	/** 200 Mbit/s */
	MXL862XX_PORT_SPEED_200,
	/** 1000 Mbit/s */
	MXL862XX_PORT_SPEED_1000,
	/** 2.5 Gbit/s */
	MXL862XX_PORT_SPEED_2500,
	/** 5 Gbit/s */
	MXL862XX_PORT_SPEED_5000,
	/** 10 Gbit/s */
	MXL862XX_PORT_SPEED_10000,
	/** Auto speed for XGMAC */
	MXL862XX_PORT_SPEED_AUTO,
} mxl862xx_port_speed_t;

/** \brief Ethernet port duplex status.
    Used by \ref MXL862XX_port_link_cfg_t. */
typedef enum {
	/** Port operates in full-duplex mode */
	MXL862XX_DUPLEX_FULL = 0,
	/** Port operates in half-duplex mode */
	MXL862XX_DUPLEX_HALF = 1,
	/** Port operates in Auto mode */
	MXL862XX_DUPLEX_AUTO = 2,
} mxl862xx_port_duplex_t;

/** \brief Force the MAC and PHY link modus.
    Used by \ref MXL862XX_port_link_cfg_t. */
typedef enum {
	/** Link up. Any connected LED
	    still behaves based on the real PHY status. */
	MXL862XX_PORT_LINK_UP = 0,
	/** Link down. */
	MXL862XX_PORT_LINK_DOWN = 1,
	/** Link Auto. */
	MXL862XX_PORT_LINK_AUTO = 2,
} mxl862xx_port_link_t;

/** \brief Ethernet port interface mode.
    A port might support only a subset of the possible settings.
    Used by \ref MXL862XX_port_link_cfg_t. */
typedef enum {
	/** Normal PHY interface (twisted pair), use the internal MII Interface. */
	MXL862XX_PORT_HW_MII = 0,
	/** Reduced MII interface in normal mode. */
	MXL862XX_PORT_HW_RMII = 1,
	/** GMII or MII, depending upon the speed. */
	MXL862XX_PORT_HW_GMII = 2,
	/** RGMII mode. */
	MXL862XX_PORT_HW_RGMII = 3,
	/** XGMII mode. */
	MXL862XX_PORT_HW_XGMII = 4,
} mxl862xx_mii_mode_t;

/** \brief Ethernet port configuration for PHY or MAC mode.
    Used by \ref MXL862XX_port_link_cfg_t. */
typedef enum {
	/** MAC Mode. The Ethernet port is configured to work in MAC mode. */
	MXL862XX_PORT_MAC = 0,
	/** PHY Mode. The Ethernet port is configured to work in PHY mode. */
	MXL862XX_PORT_PHY = 1
} mxl862xx_mii_type_t;

/** \brief Ethernet port clock source configuration.
    Used by \ref MXL862XX_port_link_cfg_t. */
typedef enum {
	/** Clock Mode not applicable. */
	MXL862XX_PORT_CLK_NA = 0,
	/** Clock Master Mode. The port is configured to provide the clock as output signal. */
	MXL862XX_PORT_CLK_MASTER = 1,
	/** Clock Slave Mode. The port is configured to use the input clock signal. */
	MXL862XX_PORT_CLK_SLAVE = 2
} mxl862xx_clk_mode_t;

/** \brief Ethernet port link, speed status and flow control status.
    Used by \ref MXL862XX_PORT_LINK_CFG_GET and \ref MXL862XX_PORT_LINK_CFG_SET. */
typedef struct {
	/** Ethernet Port number (zero-based counting). The valid range is hardware
	    dependent. An error code is delivered if the selected port is not
	    available. */
	u16 port_id;
	/** Force Port Duplex Mode.

	    - 0: Negotiate Duplex Mode. Auto-negotiation mode. Negotiated
	      duplex mode given in 'e_duplex'
	      during MXL862XX_PORT_LINK_CFG_GET calls.
	    - 1: Force Duplex Mode. Force duplex mode in 'e_duplex'.
	*/
	bool duplex_force;
	/** Port Duplex Status. */
	mxl862xx_port_duplex_t duplex;
	/** Force Link Speed.

	    - 0: Negotiate Link Speed. Negotiated speed given in
	      'e_speed' during MXL862XX_PORT_LINK_CFG_GET calls.
	    - 1: Force Link Speed. Forced speed mode in 'e_speed'.
	*/
	bool speed_force;
	/** Ethernet port link up/down and speed status. */
	mxl862xx_port_speed_t speed;
	/** Force Link.

	     - 0: Auto-negotiate Link. Current link status is given in
	       'e_link' during MXL862XX_PORT_LINK_CFG_GET calls.
	     - 1: Force Duplex Mode. Force duplex mode in 'e_link'.
	 */
	bool link_force;
	/** Link Status. Read out the current link status.
	    Note that the link could be forced by setting 'b_link_force'. */
	mxl862xx_port_link_t link;
	/** Selected interface mode (MII/RMII/RGMII/GMII/XGMII). */
	mxl862xx_mii_mode_t mii_mode;
	/** Select MAC or PHY mode (PHY = Reverse x_mII). */
	mxl862xx_mii_type_t mii_type;
	/** Interface Clock mode (used for RMII mode). */
	mxl862xx_clk_mode_t clk_mode;
	/** 'Low Power Idle' Support for 'Energy Efficient Ethernet'.
	    Only enable this feature in case the attached PHY also supports it. */
	bool lpi;
} mxl862xx_port_link_cfg_t;

/** \brief Port Enable Type Selection.
    Used by \ref MXL862XX_port_cfg_t. */
typedef enum {
	/** The port is disabled in both directions. */
	MXL862XX_PORT_DISABLE = 0,
	/** The port is enabled in both directions (ingress and egress). */
	MXL862XX_PORT_ENABLE_RXTX = 1,
	/** The port is enabled in the receive (ingress) direction only. */
	MXL862XX_PORT_ENABLE_RX = 2,
	/** The port is enabled in the transmit (egress) direction only. */
	MXL862XX_PORT_ENABLE_TX = 3
} mxl862xx_port_enable_t;

/** \brief Ethernet flow control status.
    Used by \ref MXL862XX_port_cfg_t. */
typedef enum {
	/** Automatic flow control mode selection through auto-negotiation. */
	MXL862XX_FLOW_AUTO = 0,
	/** Receive flow control only */
	MXL862XX_FLOW_RX = 1,
	/** Transmit flow control only */
	MXL862XX_FLOW_TX = 2,
	/** Receive and Transmit flow control */
	MXL862XX_FLOW_RXTX = 3,
	/** No flow control */
	MXL862XX_FLOW_OFF = 4
} mxl862xx_port_flow_t;

/** \brief Port Mirror Options.
    Used by \ref MXL862XX_port_cfg_t. */
typedef enum {
	/** Mirror Feature is disabled. Normal port usage. */
	MXL862XX_PORT_MONITOR_NONE = 0,
	/** Port Ingress packets are mirrored to the monitor port. */
	MXL862XX_PORT_MONITOR_RX = 1,
	/** Port Egress packets are mirrored to the monitor port. */
	MXL862XX_PORT_MONITOR_TX = 2,
	/** Port Ingress and Egress packets are mirrored to the monitor port. */
	MXL862XX_PORT_MONITOR_RXTX = 3,
	/** Packet mirroring of 'unknown VLAN violation' frames. */
	MXL862XX_PORT_MONITOR_VLAN_UNKNOWN = 4,
	/** Packet mirroring of 'VLAN ingress or egress membership violation' frames. */
	MXL862XX_PORT_MONITOR_VLAN_MEMBERSHIP = 16,
	/** Packet mirroring of 'port state violation' frames. */
	MXL862XX_PORT_MONITOR_PORT_STATE = 32,
	/** Packet mirroring of 'MAC learning limit violation' frames. */
	MXL862XX_PORT_MONITOR_LEARNING_LIMIT = 64,
	/** Packet mirroring of 'port lock violation' frames. */
	MXL862XX_PORT_MONITOR_PORT_LOCK = 128
} mxl862xx_port_monitor_t;

/** \brief Interface RMON Counter Mode - (FID, SUBID or FLOWID) Config - GSWIP-3.0 only.
    Used by \ref MXL862XX_port_cfg_t. */
typedef enum {
	/** FID based Interface RMON counters Usage */
	MXL862XX_IF_RMON_FID = 0,
	/** Sub-Interface Id based Interface RMON counters Usage */
	MXL862XX_IF_RMON_SUBID = 1,
	/** Flow Id (LSB bits 3 to 0) based Interface RMON counters Usage */
	MXL862XX_IF_RMON_FLOWID_LSB = 2,
	/** Flow Id (MSB bits 7 to 4) based Interface RMON counters Usage */
	MXL862XX_IF_RMON_FLOWID_MSB = 3
} mxl862xx_if_rmon_mode_t;

/** \brief Port Configuration Parameters.
    Used by \ref MXL862XX_PORT_CFG_GET and \ref MXL862XX_PORT_CFG_SET. */
typedef struct {
	/** Port Type. This gives information which type of port is configured.
	    n_port_id should be based on this field. */
	mxl862xx_port_type_t port_type;

	/** Ethernet Port number (zero-based counting). The valid range is hardware
	    dependent. An error code is delivered if the selected port is not
	    available. */
	u16 port_id;
	/** Enable Port (ingress only, egress only, both directions, or disabled).
	    This parameter is used for Spanning Tree Protocol and 802.1X applications. */
	mxl862xx_port_enable_t enable;
	/** Drop unknown unicast packets.
	    Do not send out unknown unicast packets on this port,
	    if the boolean parameter is enabled. By default packets of this type
	    are forwarded to this port. */
	bool unicast_unknown_drop;
	/** Drop unknown multicast packets.
	    Do not send out unknown multicast packets on this port,
	    if boolean parameter is enabled. By default packets of this type
	    are forwarded to this port.
	    Some platforms also drop broadcast packets. */
	bool multicast_unknown_drop;
	/** Drop reserved packet types
	    (destination address from '01 80 C2 00 00 00' to
	    '01 80 C2 00 00 2F') received on this port. */
	bool reserved_packet_drop;
	/** Drop Broadcast packets received on this port. By default packets of this
	  type are forwarded to this port. */
	bool broadcast_drop;
	/** Enables MAC address table aging.
	    The MAC table entries learned on this port are removed after the
	    aging time has expired.
	    The aging time is a global parameter, common to all ports. */
	bool aging;
	/** MAC address table learning on the port specified by 'n_port_id'.
	    By default this parameter is always enabled. */
	bool learning;
	/** Automatic MAC address table learning locking on the port specified
	    by 'n_port_id'.
	    This parameter is only taken into account when 'b_learning' is enabled. */
	bool learning_mac_port_lock;
	/** Automatic MAC address table learning limitation on this port.
	    The learning functionality is disabled when the limit value is zero.
	    The value 0x_fFFF to allow unlimited learned address.
	    This parameter is only taken into account when 'b_learning' is enabled. */
	u16 learning_limit;
	/** MAC spoofing detection. Identifies ingress packets that carry
	    a MAC source address which was previously learned on a different ingress
	    port (learned by MAC bridging table). This also applies to static added
	    entries. Those violated packets could be accepted or discarded,
	    depending on the global switch configuration 'b_mAC_Spoofing_action'.
	    This parameter is only taken into account when 'b_learning' is enabled. */
	bool mac_spoofing_detection;
	/** Port Flow Control Status. Enables the flow control function. */
	mxl862xx_port_flow_t flow_ctrl;
	/** Port monitor feature. Allows forwarding of egress and/or ingress
	    packets to the monitor port. If enabled, the monitor port gets
	    a copy of the selected packet type. */
	mxl862xx_port_monitor_t port_monitor;
	/** Assign Interface RMON Counters for this Port - GSWIP-3.0 */
	bool if_counters;
	/** Interface RMON Counters Start Index - GSWIP-3.0.
	    Value of (-1) denotes unassigned Interface Counters.
	    Valid range : 0-255 available to be shared amongst ports in desired way*/
	int if_count_start_idx;
	/** Interface RMON Counters Mode - GSWIP-3.0 */
	mxl862xx_if_rmon_mode_t if_rmonmode;
} mxl862xx_port_cfg_t;

#define MXL862XX_ERROR_BASE 1024
/** \brief Enumeration for function status return. The upper four bits are reserved for error classification */
typedef enum {
	/** Correct or Expected Status */
	MXL862XX_STATUS_OK = 0,
	/** Generic or unknown error occurred */
	MXL862XX_STATUS_ERR = -1,
	/** Invalid function parameter */
	MXL862XX_STATUS_PARAM = -(MXL862XX_ERROR_BASE + 2),
	/** No space left in VLAN table */
	MXL862XX_STATUS_VLAN_SPACE = -(MXL862XX_ERROR_BASE + 3),
	/** Requested VLAN ID not found in table */
	MXL862XX_STATUS_VLAN_ID = -(MXL862XX_ERROR_BASE + 4),
	/** Invalid ioctl */
	MXL862XX_STATUS_INVAL_IOCTL = -(MXL862XX_ERROR_BASE + 5),
	/** Operation not supported by hardware */
	MXL862XX_STATUS_NO_SUPPORT = -(MXL862XX_ERROR_BASE + 6),
	/** Timeout */
	MXL862XX_STATUS_TIMEOUT = -(MXL862XX_ERROR_BASE + 7),
	/** At least one value is out of range */
	MXL862XX_STATUS_VALUE_RANGE = -(MXL862XX_ERROR_BASE + 8),
	/** The port_id/queue_id/meter_id/etc. is not available in this hardware or the
	    selected feature is not available on this port */
	MXL862XX_STATUS_PORT_INVALID = -(MXL862XX_ERROR_BASE + 9),
	/** The interrupt is not available in this hardware */
	MXL862XX_STATUS_IRQ_INVALID = -(MXL862XX_ERROR_BASE + 10),
	/** The MAC table is full, an entry could not be added */
	MXL862XX_STATUS_MAC_TABLE_FULL = -(MXL862XX_ERROR_BASE + 11),
	/** Locking failed - SWAPI is busy */
	MXL862XX_STATUS_LOCK_FAILED = -(MXL862XX_ERROR_BASE + 12),
	/** Multicast Forwarding table entry not found */
	MXL862XX_STATUS_ENTRY_NOT_FOUND = -(MXL862XX_ERROR_BASE + 13),
} mxl862xx_return_t;

/** \brief MAC Table Clear Type
    Used by \ref MXL862XX_MAC_table_clear_cond_t */
typedef enum {
	/** Clear dynamic entries based on Physical Port */
	MXL862XX_MAC_CLEAR_PHY_PORT = 0,
	/** Clear all dynamic entries */
	MXL862XX_MAC_CLEAR_DYNAMIC = 1,
} mxl862xx_mac_clear_type_t;

/** \brief MAC Table Clear based on given condition.
    Used by \ref MXL862XX_MAC_TABLE_CLEAR_COND. */
typedef struct {
	/** MAC table clear type \ref MXL862XX_Mac_clear_type_t */
	u8 type;
	union {
		/** Physical port id (0~16) if \ref e_type is
		    ref MXL862XX_MAC_CLEAR_PHY_PORT. */
		u8 port_id;
	};
} mxl862xx_mac_table_clear_cond_t;

/** \brief Configures the Spanning Tree Protocol state of an Ethernet port.
    Used by \ref MXL862XX_STP_PORT_CFG_SET
    and \ref MXL862XX_STP_PORT_CFG_GET. */
typedef struct {
	/** Ethernet Port number (zero-based counting) in GSWIP-2.1/2.2/3.0. From
	    GSWIP-3.1, this field is Bridge Port ID. The valid range is hardware
	    dependent. An error code is delivered if the selected port is not
	    available. */
	u16 port_id;
	/** Filtering Identifier (FID) (not supported by all switches).
	    The FID allows to keep multiple STP states per physical Ethernet port.
	    Multiple CTAG VLAN groups could be a assigned to one FID and therefore
	    share the same STP port state. Switch API ignores the FID value
	    in case the switch device does not support it or switch CTAG VLAN
	    awareness is disabled. */
	u16 fid;
	/** Spanning Tree Protocol state of the port. */
	mxl862xx_stp_port_state_t port_state;
} mxl862xx_stp_port_cfg_t;

/** \brief Special tag Ethertype mode */
typedef enum {
	/** The ether_type field of the Special Tag of egress packets is always set
	    to a prefined value. This same defined value applies for all
	    switch ports. */
	MXL862XX_CPU_ETHTYPE_PREDEFINED = 0,
	/** The Ethertype field of the Special Tag of egress packets is set to
	    the flow_iD parameter, which is a results of the switch flow
	    classification result. The switch flow table rule provides this
	    flow_iD as action parameter. */
	MXL862XX_CPU_ETHTYPE_FLOWID = 1
} mxl862xx_cpu_special_tag_eth_type_t;

/** \brief Parser Flags and Offsets Header settings on CPU Port for GSWIP-3.0.
    Used by \ref MXL862XX_CPU_Port_cfg_t. */
typedef enum {
	/** No Parsing Flags or Offsets accompanying to CPU Port for this combination */
	MXL862XX_CPU_PARSER_NIL = 0,
	/** 8-Bytes Parsing Flags (Bit 63:0) accompanying to CPU Port for this combination */
	MXL862XX_CPU_PARSER_FLAGS = 1,
	/** 40-Bytes Offsets (Offset-0 to -39) + 8-Bytes Parsing Flags (Bit 63:0) accompanying to CPU Port for this combination */
	MXL862XX_CPU_PARSER_OFFSETS_FLAGS = 2,
	/** Reserved - for future use */
	MXL862XX_CPU_PARSER_RESERVED = 3
} mxl862xx_cpu_parser_header_cfg_t;

/** \brief FCS and Pad Insertion operations for GSWIP 3.1
    Used by \ref MXL862XX_CPU_Port_cfg_set/Get. */
typedef enum {
	/** CRC Pad Insertion Enable */
	MXL862XX_CRC_PAD_INS_EN = 0,
	/** CRC Insertion Enable Pad Insertion Disable */
	MXL862XX_CRC_EN_PAD_DIS = 1,
	/** CRC Pad Insertion Disable */
	MXL862XX_CRC_PAD_INS_DIS = 2
} mxl862xx_fcs_tx_ops_t;

/** \brief Defines one port that is directly connected to the CPU and its applicable settings.
    Used by \ref MXL862XX_CPU_PORT_CFG_SET and \ref MXL862XX_CPU_PORT_CFG_GET. */
typedef struct {
	/** Ethernet Port number (zero-based counting) set to CPU Port. The valid number is hardware
	    dependent. (E.g. Port number 0 for GSWIP-3.0 or 6 for GSWIP-2.x). An error code is delivered if the selected port is not
	    available. */
	u16 port_id;
	/** CPU port validity.
	    Set command: set true to define a CPU port, set false to undo the setting.
	    Get command: true if defined as CPU, false if not defined as CPU port. */
	bool cPU_Port_valid;
	/** Special tag enable in ingress direction. */
	bool special_tag_ingress;
	/** Special tag enable in egress direction. */
	bool special_tag_egress;
	/** Enable FCS check

	    - false: No check, forward all frames
	    - 1: Check FCS, drop frames with errors
	*/
	bool fcs_check;
	/** Enable FCS generation

	    - false: Forward packets without FCS
	    - 1: Generate FCS for all frames
	*/
	bool fcs_generate;
	/** Special tag Ethertype mode. Not applicable to GSWIP-3.1. */
	mxl862xx_cpu_special_tag_eth_type_t special_tag_eth_type;
	/** GSWIP-3.0 specific Parser Header Config for no MPE flags (i.e. MPE1=0, MPE2=0). */
	mxl862xx_cpu_parser_header_cfg_t no_mpeparser_cfg;
	/** GSWIP-3.0 specific Parser Header Config for MPE-1 set flag (i.e. MPE1=1, MPE2=0). */
	mxl862xx_cpu_parser_header_cfg_t mpe1parser_cfg;
	/** GSWIP-3.0 specific Parser Header Config for MPE-2 set flag (i.e. MPE1=0, MPE2=1). */
	mxl862xx_cpu_parser_header_cfg_t mpe2parser_cfg;
	/** GSWIP-3.0 specific Parser Header Config for both MPE-1 and MPE-2 set flag (i.e. MPE1=1, MPE2=1). */
	mxl862xx_cpu_parser_header_cfg_t mpe1mpe2parser_cfg;
	/** GSWIP-3.1 FCS tx Operations. */
	mxl862xx_fcs_tx_ops_t fcs_tx_ops;
	/** GSWIP-3.2 Time Stamp Field Removal for PTP Packet
	    0 - DIS Removal is disabled
	    1 - EN Removal is enabled
	*/
	bool ts_ptp;
	/** GSWIP-3.2 Time Stamp Field Removal for Non-PTP Packet
	    0 - DIS Removal is disabled
	    1 - EN Removal is enabled
	*/
	bool ts_nonptp;
} mxl862xx_cpu_port_cfg_t;

typedef struct {
	/* port ID (1~16) */
	u8 pid;

	/* bit value 1 to indicate valid field
	 *   bit 0 - rx
	 *       1 - tx
	 *       2 - rx_pen
	 *       3 - tx_pen
	 */
	u8 mask;

	/* RX special tag mode
	 *  value 0 - packet does NOT have special tag and special tag is
	 *            NOT inserted
	 *        1 - packet does NOT have special tag and special tag is
	 *            inserted
	 *        2 - packet has special tag and special tag is NOT inserted
	 */
	u8 rx;

	/* TX special tag mode
	 *  value 0 - packet does NOT have special tag and special tag is
	 *            NOT removed
	 *        1 - packet has special tag and special tag is replaced
	 *        2 - packet has special tag and special tag is NOT removed
	 *        3 - packet has special tag and special tag is removed
	 */
	u8 tx;

	/* RX special tag info over preamble
	 *  value 0 - special tag info inserted from byte 2 to 7 are all 0
	 *        1 - special tag byte 5 is 16, other bytes from 2 to 7 are 0
	 *        2 - special tag byte 5 is from preamble field, others are 0
	 *        3 - special tag byte 2 to 7 are from preabmle field
	 */
	u8 rx_pen;

	/* TX special tag info over preamble
	 *  value 0 - disabled
	 *        1 - enabled
	 */
	u8 tx_pen;
} mxl862xx_ss_sp_tag_t;

/** \brief Port monitor configuration.
    Used by \ref MXL862XX_MONITOR_PORT_CFG_GET and \ref MXL862XX_MONITOR_PORT_CFG_SET. */
typedef struct {
	/** Ethernet Port number (zero-based counting). The valid range is hardware
	    dependent. An error code is delivered if the selected port is not
	    available. */
	u8 port_id;
	/* Monitoring Sub-IF id */
	u16 sub_if_id;
	/* Out of use. */
	bool monitor_port;
} mxl862xx_monitor_port_cfg_t;

/** \brief VLAN Filter TCI Mask.
    Used by \ref MXL862XX_VLANFILTER_config_t */
typedef enum {
	MXL862XX_VLAN_FILTER_TCI_MASK_VID = 0,
	MXL862XX_VLAN_FILTER_TCI_MASK_PCP = 1,
	MXL862XX_VLAN_FILTER_TCI_MASK_TCI = 2
} mxl862xx_vlan_filter_tci_mask_t;

typedef enum {
	MXL862XX_EXTENDEDVLAN_TPID_VTETYPE_1 = 0,
	MXL862XX_EXTENDEDVLAN_TPID_VTETYPE_2 = 1,
	MXL862XX_EXTENDEDVLAN_TPID_VTETYPE_3 = 2,
	MXL862XX_EXTENDEDVLAN_TPID_VTETYPE_4 = 3
} mxl862xx_extended_vlan_4_tpid_mode_t;

/** \brief Extended VLAN Filter TPID Field.
    Used by \ref MXL862XX_EXTENDEDVLAN_filter_vLAN_t. */
typedef enum {
	/** Do not filter. */
	MXL862XX_EXTENDEDVLAN_FILTER_TPID_NO_FILTER = 0,
	/** TPID is 0x8100. */
	MXL862XX_EXTENDEDVLAN_FILTER_TPID_8021Q = 1,
	/** TPID is global configured value. */
	MXL862XX_EXTENDEDVLAN_FILTER_TPID_VTETYPE = 2
} mxl862xx_extended_vlan_filter_tpid_t;

/** \brief Extended VLAN Treatment Set TPID.
   Used by \ref MXL862XX_EXTENDEDVLAN_treatment_vlan_t. */
typedef enum {
	/** TPID is copied from inner VLAN tag of received packet. */
	MXL862XX_EXTENDEDVLAN_TREATMENT_INNER_TPID = 0,
	/** TPID is copied from outer VLAN tag of received packet. */
	MXL862XX_EXTENDEDVLAN_TREATMENT_OUTER_TPID = 1,
	/** TPID is global configured value. */
	MXL862XX_EXTENDEDVLAN_TREATMENT_VTETYPE = 2,
	/** TPID is 0x8100. */
	MXL862XX_EXTENDEDVLAN_TREATMENT_8021Q = 3
} mxl862xx_extended_vlan_treatment_tpid_t;

/** \brief Extended VLAN Filter DEI Field.
    Used by \ref MXL862XX_EXTENDEDVLAN_filter_vLAN_t. */
typedef enum {
	/** Do not filter. */
	MXL862XX_EXTENDEDVLAN_FILTER_DEI_NO_FILTER = 0,
	/** DEI is 0. */
	MXL862XX_EXTENDEDVLAN_FILTER_DEI_0 = 1,
	/** DEI is 1. */
	MXL862XX_EXTENDEDVLAN_FILTER_DEI_1 = 2
} mxl862xx_extended_vlan_filter_dei_t;

/** \brief Extended VLAN Treatment Set DEI.
   Used by \ref MXL862XX_EXTENDEDVLAN_treatment_vlan_t. */
typedef enum {
	/** DEI (if applicable) is copied from inner VLAN tag of received packet. */
	MXL862XX_EXTENDEDVLAN_TREATMENT_INNER_DEI = 0,
	/** DEI (if applicable) is copied from outer VLAN tag of received packet. */
	MXL862XX_EXTENDEDVLAN_TREATMENT_OUTER_DEI = 1,
	/** DEI is 0. */
	MXL862XX_EXTENDEDVLAN_TREATMENT_DEI_0 = 2,
	/** DEI is 1. */
	MXL862XX_EXTENDEDVLAN_TREATMENT_DEI_1 = 3
} mxl862xx_extended_vlan_treatment_dei_t;

/** \brief Extended VLAN Filter Type.
    Used by \ref MXL862XX_EXTENDEDVLAN_filter_vLAN_t. */
typedef enum {
	/** There is tag and criteria applies. */
	MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NORMAL = 0,
	/** There is tag but no criteria. */
	MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_FILTER = 1,
	/** Default entry if no other rule applies. */
	MXL862XX_EXTENDEDVLAN_FILTER_TYPE_DEFAULT = 2,
	/** There is no tag. */
	MXL862XX_EXTENDEDVLAN_FILTER_TYPE_NO_TAG = 3,
	/** Block invalid*/
	MXL862XX_EXTENDEDVLAN_BLOCK_INVALID = 4
} mxl862xx_extended_vlan_filter_type_t;

/** \brief Extended VLAN Filter ether_type.
    Used by \ref MXL862XX_EXTENDEDVLAN_filter_vLAN_t. */
typedef enum {
	/** Do not filter. */
	MXL862XX_EXTENDEDVLAN_FILTER_ETHERTYPE_NO_FILTER = 0,
	/** IPo_e frame (Ethertyp is 0x0800). */
	MXL862XX_EXTENDEDVLAN_FILTER_ETHERTYPE_IPOE = 1,
	/** PPPo_e frame (Ethertyp is 0x8863 or 0x8864). */
	MXL862XX_EXTENDEDVLAN_FILTER_ETHERTYPE_PPPOE = 2,
	/** ARP frame (Ethertyp is 0x0806). */
	MXL862XX_EXTENDEDVLAN_FILTER_ETHERTYPE_ARP = 3,
	/** IPv6 IPo_e frame (Ethertyp is 0x86DD). */
	MXL862XX_EXTENDEDVLAN_FILTER_ETHERTYPE_IPV6IPOE = 4,
	/** EAPOL (Ethertyp is 0x888E). */
	MXL862XX_EXTENDEDVLAN_FILTER_ETHERTYPE_EAPOL = 5,
	/** DHCPV4 (UDP DESTINATION PORT 67&68). */
	MXL862XX_EXTENDEDVLAN_FILTER_ETHERTYPE_DHCPV4 = 6,
	/** DHCPV6 (UDP DESTINATION PORT 546&547). */
	MXL862XX_EXTENDEDVLAN_FILTER_ETHERTYPE_DHCPV6 = 7
} mxl862xx_extended_vlan_filter_ethertype_t;

/** \brief Extended VLAN Treatment Set Priority.
   Used by \ref MXL862XX_EXTENDEDVLAN_treatment_vlan_t. */
typedef enum {
	/** Set priority with given value. */
	MXL862XX_EXTENDEDVLAN_TREATMENT_PRIORITY_VAL = 0,
	/** Prority value is copied from inner VLAN tag of received packet. */
	MXL862XX_EXTENDEDVLAN_TREATMENT_INNER_PRORITY = 1,
	/** Prority value is copied from outer VLAN tag of received packet. */
	MXL862XX_EXTENDEDVLAN_TREATMENT_OUTER_PRORITY = 2,
	/** Prority value is derived from DSCP field of received packet. */
	MXL862XX_EXTENDEDVLAN_TREATMENT_DSCP = 3
} mxl862xx_extended_vlan_treatment_priority_t;

/** \brief Extended VLAN Treatment Set VID.
   Used by \ref MXL862XX_EXTENDEDVLAN_treatment_vlan_t. */
typedef enum {
	/** Set VID with given value. */
	MXL862XX_EXTENDEDVLAN_TREATMENT_VID_VAL = 0,
	/** VID is copied from inner VLAN tag of received packet. */
	MXL862XX_EXTENDEDVLAN_TREATMENT_INNER_VID = 1,
	/** VID is copied from outer VLAN tag of received packet. */
	MXL862XX_EXTENDEDVLAN_TREATMENT_OUTER_VID = 2,
} mxl862xx_extended_vlan_treatment_vid_t;

/** \brief Extended VLAN Treatment Remove Tag.
    Used by \ref MXL862XX_EXTENDEDVLAN_treatment_vlan_t. */
typedef enum {
	/** Do not remove VLAN tag. */
	MXL862XX_EXTENDEDVLAN_TREATMENT_NOT_REMOVE_TAG = 0,
	/** Remove 1 VLAN tag following DA/SA. */
	MXL862XX_EXTENDEDVLAN_TREATMENT_REMOVE_1_TAG = 1,
	/** Remove 2 VLAN tag following DA/SA. */
	MXL862XX_EXTENDEDVLAN_TREATMENT_REMOVE_2_TAG = 2,
	/** Discard upstream traffic. */
	MXL862XX_EXTENDEDVLAN_TREATMENT_DISCARD_UPSTREAM = 3,
} mxl862xx_extended_vlan_treatment_remove_tag_t;

/** \brief Extended VLAN Filter VLAN Tag.
    Used by \ref MXL862XX_EXTENDEDVLAN_filter_t. */
typedef struct {
	/** Filter Type: normal filter, default rule, or no tag */
	mxl862xx_extended_vlan_filter_type_t type;
	/** Enable priority field filtering. */
	bool priority_enable;
	/** Filter priority value if b_priority_enable is TRUE. */
	u32 priority_val;
	/** Enable VID filtering. */
	bool vid_enable;
	/** Filter VID if b_vid_enable is TRUE. */
	u32 vid_val;
	/** Mode to filter TPID of VLAN tag. */
	mxl862xx_extended_vlan_filter_tpid_t tpid;
	/** Mode to filter DEI of VLAN tag. */
	mxl862xx_extended_vlan_filter_dei_t dei;
} mxl862xx_extendedvlan_filter_vlan_t;

/** \brief Extended VLAN Treatment VLAN Tag.
    Used by \ref MXL862XX_EXTENDEDVLAN_treatment_t. */
typedef struct {
	/** Select source of priority field of VLAN tag. */
	mxl862xx_extended_vlan_treatment_priority_t priority_mode;
	/** If \ref MXL862XX_EXTENDEDVLAN_treatment_vlan_t::priority_mode is
	    \ref MXL862XX_EXTENDEDVLAN_TREATMENT_PRIORITY_VAL, use this value for
	    priority field of VLAN tag. */
	u32 priority_val;
	/** Select source of VID field of VLAN tag. */
	mxl862xx_extended_vlan_treatment_vid_t vid_mode;
	/** If \ref MXL862XX_EXTENDEDVLAN_treatment_vlan_t::e_vid_mode is
	    \ref MXL862XX_EXTENDEDVLAN_TREATMENT_VID_VAL, use this value for VID field
	    of VLAN tag. */
	u32 vid_val;
	/** Select source of TPID field of VLAN tag. */
	mxl862xx_extended_vlan_treatment_tpid_t tpid;
	/** Select source of DEI field of VLAN tag. */
	mxl862xx_extended_vlan_treatment_dei_t dei;
} mxl862xx_extendedvlan_treatment_vlan_t;

/** \brief Extended VLAN Filter.
    Used by \ref MXL862XX_EXTENDEDVLAN_config_t. */
typedef struct {
	/** Filter on Original Packet. */
	bool original_packet_filter_mode;
	mxl862xx_extended_vlan_4_tpid_mode_t filter_4_tpid_mode;
	/** Filter for outer VLAN tag. */
	mxl862xx_extendedvlan_filter_vlan_t outer_vlan;
	/** Filter for inner VLAN tag. */
	mxl862xx_extendedvlan_filter_vlan_t inner_vlan;
	/** Filter ether_type. */
	mxl862xx_extended_vlan_filter_ethertype_t ether_type;
} mxl862xx_extendedvlan_filter_t;

/** \brief Extended VLAN Allocation.
    Used by \ref MXL862XX_EXTENDEDVLAN_ALLOC and \ref MXL862XX_EXTENDEDVLAN_FREE. */
typedef struct {
	/** Total number of extended VLAN entries are requested. Proper value should
	    be given for \ref MXL862XX_EXTENDEDVLAN_ALLOC. This field is ignored for
	    \ref MXL862XX_EXTENDEDVLAN_FREE. */
	u16 number_of_entries;

	/** If \ref MXL862XX_EXTENDEDVLAN_ALLOC is successful, a valid ID will be returned
	    in this field. Otherwise, \ref INVALID_HANDLE is returned in this field.
	    For \ref MXL862XX_EXTENDEDVLAN_FREE, this field should be valid ID returned by
	    \ref MXL862XX_EXTENDEDVLAN_ALLOC. */
	u16 extended_vlan_block_id;
} mxl862xx_extendedvlan_alloc_t;

/** \brief Extended VLAN Treatment.
    Used by \ref MXL862XX_EXTENDEDVLAN_config_t. */
typedef struct {
	/** Number of VLAN tag(s) to remove. */
	mxl862xx_extended_vlan_treatment_remove_tag_t remove_tag;

	mxl862xx_extended_vlan_4_tpid_mode_t treatment_4_tpid_mode;

	/** Enable outer VLAN tag add/modification. */
	bool add_outer_vlan;
	/** If b_add_outer_vlan is TRUE, add or modify outer VLAN tag. */
	mxl862xx_extendedvlan_treatment_vlan_t outer_vlan;

	/** Enable inner VLAN tag add/modification. */
	bool add_inner_vlan;
	/** If b_add_inner_vlan is TRUE, add or modify inner VLAN tag. */
	mxl862xx_extendedvlan_treatment_vlan_t inner_vlan;

	/** Enable re-assignment of bridge port. */
	bool reassign_bridge_port;
	/** If b_reassign_bridge_port is TRUE, use this value for bridge port. */
	u16 new_bridge_port_id;

	/** Enable new DSCP. */
	bool new_dscp_enable;
	/** If b_new_dscp_enable is TRUE, use this value for DSCP. */
	u16 new_dscp;

	/** Enable new traffic class. */
	bool new_traffic_class_enable;
	/** If b_new_traffic_class_enable is TRUE, use this value for traffic class. */
	u8 new_traffic_class;

	/** Enable new meter. */
	bool new_meter_enable;
	/** New meter ID.

	    \remarks
	    Meter should be allocated with \ref MXL862XX_QOS_METER_ALLOC before extended
	    VLAN treatment is added. If this extended VLAN treatment is deleted,
	    this meter should be released with \ref MXL862XX_QOS_METER_FREE. */
	u16 s_new_traffic_meter_id;

	/** DSCP to PCP mapping, if
	    \ref MXL862XX_EXTENDEDVLAN_treatment_vlan_t::e_priority_mode in
	    \ref MXL862XX_EXTENDEDVLAN_treatment_t::s_outer_vlan.e_priority_mode or
	    \ref MXL862XX_EXTENDEDVLAN_treatment_t::s_inner_vlan.e_priority_mode is
	    \ref MXL862XX_EXTENDEDVLAN_TREATMENT_DSCP.

	    \remarks
	    The index of array stands for DSCP value. Each byte of the array is 3-bit
	    PCP value. For implementation, if DSCP2PCP is separate hardware table,
	    a resource management mechanism should be implemented. Allocation happens
	    when extended VLAN treatment added, and release happens when the
	    treatment is deleted. For debug, the DSCP2PCP table can be dumped with
	    \ref MXL862XX_DSCP2PCP_MAP_GET. */
	u8 dscp2pcp_map[64];

	/** Enable loopback. */
	bool loopback_enable;
	/** Enable destination/source MAC address swap. */
	bool da_sa_swap_enable;
	/** Enable traffic mirrored to the monitoring port. */
	bool mirror_enable;
} mxl862xx_extendedvlan_treatment_t;

/** \brief Extended VLAN Configuration.
    Used by \ref MXL862XX_EXTENDEDVLAN_SET and \ref MXL862XX_EXTENDEDVLAN_GET. */
typedef struct {
	/** This should be valid ID returned by \ref MXL862XX_EXTENDEDVLAN_ALLOC.
	    If it is \ref INVALID_HANDLE, \ref MXL862XX_EXTENDEDVLAN_config_t::n_entry_index
	    is absolute index of Extended VLAN entry in hardware for debug purpose,
	    bypassing any check. */
	u16 extended_vlan_block_id;

	/** Index of entry, ranges between 0 and
	    \ref MXL862XX_EXTENDEDVLAN_alloc_t::n_number_of_entries - 1, to
	    set (\ref MXL862XX_EXTENDEDVLAN_SET) or get (\ref MXL862XX_EXTENDEDVLAN_GET)
	    Extended VLAN Configuration entry. For debug purpose, this field could be
	    absolute index of Entended VLAN entry in hardware, when
	    \ref MXL862XX_EXTENDEDVLAN_config_t::n_extended_vlan_block_id is
	    \ref INVALID_HANDLE. */
	u16 entry_index;

	/** Extended VLAN Filter */
	mxl862xx_extendedvlan_filter_t filter;
	/** Extended VLAN Treatment */
	mxl862xx_extendedvlan_treatment_t treatment;
} mxl862xx_extendedvlan_config_t;

/** \brief VLAN Filter Allocation.
    Used by \ref MXL862XX_VLANFILTER_ALLOC and \ref MXL862XX_VLANFILTER_FREE. */
typedef struct {
	/** Total number of VLAN Filter entries are requested. Proper value should
	    be given for \ref MXL862XX_VLANFILTER_ALLOC. This field is ignored for
	    \ref MXL862XX_VLANFILTER_FREE. */
	u16 number_of_entries;

	/** If \ref MXL862XX_VLANFILTER_ALLOC is successful, a valid ID will be returned
	    in this field. Otherwise, \ref INVALID_HANDLE is returned in this field.
	    For \ref MXL862XX_EXTENDEDVLAN_FREE, this field should be valid ID returned by
	    \ref MXL862XX_VLANFILTER_ALLOC. */
	u16 vlan_filter_block_id;

	/** Discard packet without VLAN tag. */
	bool discard_untagged;
	/** Discard VLAN tagged packet not matching any filter entry. */
	bool discard_unmatched_tagged;
	/** Use default port VLAN ID for VLAN filtering

	    \remarks
	    This field is not available in PRX300. */
	bool use_default_port_vid;
} mxl862xx_vlanfilter_alloc_t;

/** \brief VLAN Filter.
    Used by \ref MXL862XX_VLANFILTER_SET and \ref MXL862XX_VLANFILTER_GET */
typedef struct {
	/** This should be valid ID return by \ref MXL862XX_VLANFILTER_ALLOC.
	    If it is \ref INVALID_HANDLE, \ref MXL862XX_VLANFILTER_config_t::n_entry_index
	    is absolute index of VLAN Filter entry in hardware for debug purpose,
	    bypassing any check. */
	u16 vlan_filter_block_id;

	/** Index of entry. ranges between 0 and
	    \ref MXL862XX_VLANFILTER_alloc_t::n_number_of_entries - 1, to
	    set (\ref MXL862XX_VLANFILTER_SET) or get (\ref MXL862XX_VLANFILTER_GET)
	    VLAN FIlter entry. For debug purpose, this field could be absolute index
	    of VLAN Filter entry in hardware, when
	    \ref MXL862XX_VLANFILTER_config_t::n_vlan_filter_block_id is
	    \ref INVALID_HANDLE. */
	u16 entry_index;

	/** VLAN TCI filter mask mode.

	    \remarks
	    In GSWIP-3.1, this field of first entry in the block will applies to rest
	    of entries in the same block. */
	mxl862xx_vlan_filter_tci_mask_t vlan_filter_mask;

	/** This is value for VLAN filtering. It depends on
	    \ref MXL862XX_VLANFILTER_config_t::e_vlan_filter_mask.
	    For MXL862XX_VLAN_FILTER_TCI_MASK_VID, this is 12-bit VLAN ID.
	    For MXL862XX_VLAN_FILTER_TCI_MASK_PCP, this is 3-bit PCP field of VLAN tag.
	    For MXL862XX_VLAN_FILTER_TCI_MASK_TCI, this is 16-bit TCI of VLAN tag. */
	u32 val;
	/** Discard packet if match. */
	bool discard_matched;
} mxl862xx_vlanfilter_config_t;

/* VLAN Rmon Counters */
typedef enum {
	MXL862XX_VLAN_RMON_RX = 0,
	MXL862XX_VLAN_RMON_TX = 1,
	MXL862XX_VLAN_RMON__PCE_BYPASS = 2,
} mxl862xx_vlan_rmon_type_t;

/**
 \brief RMON Counters structure for VLAN. */
typedef struct {
	u16 vlan_counter_index;
	mxl862xx_vlan_rmon_type_t vlan_rmon_type;
	u32 byte_count_high;
	u32 byte_count_low;
	u32 total_pkt_count;
	u32 multicast_pkt_count;
	u32 drop_pkt_count;
	u32 clear_all;
} mxl862xx_vlan_rmon_cnt_t;

/**
 \brief RMON Counters control structure for VLAN. */
typedef struct {
	bool vlan_rmon_enable;
	bool include_broad_cast_pkt_counting;
	u32 vlan_last_entry;
} mxl862xx_vlan_rmon_control_t;

/** \brief VLAN Counter Mapping. */
typedef enum {
	/** VLAN Mapping for Ingress */
	MXL862XX_VLAN_MAPPING_INGRESS = 0,
	/** VLAN Mapping for Egress */
	MXL862XX_VLAN_MAPPING_EGRESS = 1,
	/** VLAN Mapping for Ingress and Egress */
	MXL862XX_VLAN_MAPPING_INGRESS_AND_EGRESS = 2
} mxl862xx_vlan_counter_mapping_type_t;

/** \brief VLAN Counter Mapping Filter. */
typedef enum {
	/** There is tag and criteria applies. */
	MXL862XX_VLANCOUNTERMAP_FILTER_TYPE_NORMAL = 0,
	/** There is tag but no criteria. */
	MXL862XX_VLANCOUNTERMAP_FILTER_TYPE_NO_FILTER = 1,
	/** Default entry if no other rule applies. */
	MXL862XX_VLANCOUNTERMAP_FILTER_TYPE_DEFAULT = 2,
	/** There is no tag. */
	MXL862XX_VLANCOUNTERMAP_FILTER_TYPE_NO_TAG = 3,
	/** Filter invalid*/
	MXL862XX_VLANCOUNTERMAP_FILTER_INVALID = 4,
} mxl862xx_vlan_counter_map_filter_type_t;

/** \brief VLAN Counter Mapping Configuration. */
typedef struct {
	/** Counter Index */
	u8 counter_index;
	/** Ctp Port Id */
	u16 ctp_port_id;
	/** Priority Enable */
	bool priority_enable;
	/** Priority Val */
	u32 priority_val;
	/** VLAN Id Enable */
	bool vid_enable;
	/** VLAN Id Value */
	u32 vid_val;
	/** VLAN Tag Selection Value */
	bool vlan_tag_selection_enable;
	/** VLAN Counter Mapping Type */
	mxl862xx_vlan_counter_mapping_type_t vlan_counter_mapping_type;
	/** VLAN Counter Mapping Filter Type */
	mxl862xx_vlan_counter_map_filter_type_t vlan_counter_mapping_filter_type;
} mxl862xx_vlan_counter_mapping_config_t;

/** \brief MAC Table Entry to be added.
    Used by \ref MXL862XX_MAC_TABLE_ENTRY_ADD. */
typedef struct {
	/** Filtering Identifier (FID) (not supported by all switches) */
	u16 fid;
	/** Ethernet Port number (zero-based counting) in GSWIP-2.1/2.2/3.0. From
	    GSWIP-3.1, this field is Bridge Port ID. The valid range is hardware
	    dependent.

	    \remarks
	    In GSWIP-2.1/2.2/3.0, this field is used as portmap field, when the MSB
	    bit is set. In portmap mode, every value bit represents an Ethernet port.
	    LSB represents Port 0 with incrementing counting.
	    The (MSB - 1) bit represent the last port.
	    The macro \ref MXL862XX_PORTMAP_FLAG_SET allows to set the MSB bit,
	    marking it as portmap variable.
	    Checking the portmap flag can be done by
	    using the \ref MXL862XX_PORTMAP_FLAG_GET macro.
	    From GSWIP3.1, if MSB is set, other bits in this field are ignored.
	    array \ref MXL862XX_MAC_table_read_t::n_port_map is used for bit map. */
	u32 port_id;
	/** Bridge Port Map - to support GSWIP-3.1, following field is added
	    for port map in static entry. It's valid only when MSB of
	    \ref MXL862XX_MAC_table_read_t::n_port_id is set. Each bit stands for 1 bridge
	    port. */
	u16 port_map[8]; /* max can be 16 */
	/** Sub-Interface Identifier Destination (supported in GSWIP-3.0/3.1 only).

	    \remarks
	    In GSWIP-3.1, this field is sub interface ID for WLAN logical port. For
	    Other types, either outer VLAN ID if Nto1Vlan enabled or 0. */
	u16 sub_if_id;
	/** Aging Time, given in multiples of 1 second in a range
	    from 1 s to 1,000,000 s.
	    The configured value might be rounded that it fits to the given hardware platform. */
	int age_timer;
	/** STAG VLAN Id. Only applicable in case SVLAN support is enabled on the device. */
	u16 vlan_id;
	/** Static Entry (value will be aged out if the entry is not set to static). The
	    switch API implementation uses the maximum age timer in case the entry
	    is not static. */
	bool static_entry;
	/** Egress queue traffic class.
	    The queue index starts counting from zero.   */
	u8 traffic_class;
	/** MAC Address to add to the table. */
	u8 mac[ETH_ALEN];
	/** Source/Destination MAC address filtering flag (GSWIP-3.1 only)
	    Value 0 - not filter, 1 - source address filter,
	    2 - destination address filter, 3 - both source and destination filter.

	    \remarks
	    Please refer to "GSWIP Hardware Architecture Spec" chapter 3.4.4.6
	    "Source MAC Address Filtering and Destination MAC Address Filtering"
	    for more detail. */
	u8 filter_flag;
	/** Packet is marked as IGMP controlled if destination MAC address matches
	    MAC in this entry. (GSWIP-3.1 only) */
	bool igmp_controlled;

	/** Associated Mac address -(GSWIP-3.2)*/
	u8 associated_mac[ETH_ALEN];

	/** TCI for (GSWIP-3.2) B-Step
	    Bit [0:11] - VLAN ID
	    Bit [12] - VLAN CFI/DEI
	    Bit [13:15] - VLAN PRI */
	u16 tci;
} mxl862xx_mac_table_add_t;

/** \brief MAC Table Entry to be read.
    Used by \ref MXL862XX_MAC_TABLE_ENTRY_READ. */
typedef struct {
	/** Restart the get operation from the beginning of the table. Otherwise
	    return the next table entry (next to the entry that was returned
	    during the previous get operation). This boolean parameter is set by the
	    calling application. */
	bool initial;
	/** Indicates that the read operation got all last valid entries of the
	    table. This boolean parameter is set by the switch API
	    when the Switch API is called after the last valid one was returned already. */
	bool last;
	/** Get the MAC table entry belonging to the given Filtering Identifier
	    (not supported by all switches). */
	u16 fid;
	/** Ethernet Port number (zero-based counting) in GSWIP-2.1/2.2/3.0. From
	    GSWIP-3.1, this field is Bridge Port ID. The valid range is hardware
	    dependent.

	    \remarks
	    In GSWIP-2.1/2.2/3.0, this field is used as portmap field, when the MSB
	    bit is set. In portmap mode, every value bit represents an Ethernet port.
	    LSB represents Port 0 with incrementing counting.
	    The (MSB - 1) bit represent the last port.
	    The macro \ref MXL862XX_PORTMAP_FLAG_SET allows to set the MSB bit,
	    marking it as portmap variable.
	    Checking the portmap flag can be done by
	    using the \ref MXL862XX_PORTMAP_FLAG_GET macro.
	    From GSWIP3.1, if MSB is set, other bits in this field are ignored.
	    array \ref MXL862XX_MAC_table_read_t::n_port_map is used for bit map. */
	u32 port_id;
	/** Bridge Port Map - to support GSWIP-3.1, following field is added
	    for port map in static entry. It's valid only when MSB of
	    \ref MXL862XX_MAC_table_read_t::n_port_id is set. Each bit stands for 1 bridge
	    port. */
	u16 port_map[8]; /* max can be 16 */
	/** Aging Time, given in multiples of 1 second in a range from 1 s to 1,000,000 s.
	    The value read back in a GET command might differ slightly from the value
	    given in the SET command due to limited hardware timing resolution.
	    Filled out by the switch API implementation. */
	int age_timer;
	/** STAG VLAN Id. Only applicable in case SVLAN support is enabled on the device. */
	u16 vlan_id;
	/** Static Entry (value will be aged out after 'n_age_timer' if the entry
	    is not set to static). */
	bool static_entry;
	/** Sub-Interface Identifier Destination (supported in GSWIP-3.0/3.1 only). */
	u16 sub_if_id;
	/** MAC Address. Filled out by the switch API implementation. */
	u8 mac[ETH_ALEN];
	/** Source/Destination MAC address filtering flag (GSWIP-3.1 only)
	    Value 0 - not filter, 1 - source address filter,
	    2 - destination address filter, 3 - both source and destination filter.

	    \remarks
	    Please refer to "GSWIP Hardware Architecture Spec" chapter 3.4.4.6
	    "Source MAC Address Filtering and Destination MAC Address Filtering"
	    for more detail. */
	u8 filter_flag;
	/** Packet is marked as IGMP controlled if destination MAC address matches
	    MAC in this entry. (GSWIP-3.1 only) */
	bool igmp_controlled;

	/** Changed
	0: the entry is not changed
	1: the entry is changed and not accessed yet */

	bool entry_changed;

	/** Associated Mac address -(GSWIP-3.2)*/
	u8 associated_mac[ETH_ALEN];
	/* MAC Table Hit Status Update (Supported in GSWip-3.1/3.2) */
	bool hit_status;
	/** TCI for (GSWIP-3.2) B-Step
	    Bit [0:11] - VLAN ID
	    Bit [12] - VLAN CFI/DEI
	    Bit [13:15] - VLAN PRI */
	u16 tci;
	u16 first_bridge_port_id;
} mxl862xx_mac_table_read_t;

/** \brief MAC Table Entry to be removed.
    Used by \ref MXL862XX_MAC_TABLE_ENTRY_REMOVE. */
typedef struct {
	/** Filtering Identifier (FID) (not supported by all switches) */
	u16 fid;
	/** MAC Address to be removed from the table. */
	u8 mac[ETH_ALEN];
	/** Source/Destination MAC address filtering flag (GSWIP-3.1 only)
	    Value 0 - not filter, 1 - source address filter,
	    2 - destination address filter, 3 - both source and destination filter.

	    \remarks
	    Please refer to "GSWIP Hardware Architecture Spec" chapter 3.4.4.6
	    "Source MAC Address Filtering and Destination MAC Address Filtering"
	    for more detail. */
	u8 filter_flag;
	/** TCI for (GSWIP-3.2) B-Step
	    Bit [0:11] - VLAN ID
	    Bit [12] - VLAN CFI/DEI
	    Bit [13:15] - VLAN PRI */
	u16 tci;
} mxl862xx_mac_table_remove_t;

/** \brief Bridge configuration mask.
    Used by \ref MXL862XX_BRIDGE_config_t. */
typedef enum {
	/** Mask for \ref MXL862XX_BRIDGE_config_t::b_mac_learning_limit_enable
	    and \ref MXL862XX_BRIDGE_config_t::n_mac_learning_limit. */
	MXL862XX_BRIDGE_CONFIG_MASK_MAC_LEARNING_LIMIT = 0x00000001,
	/** Mask for \ref MXL862XX_BRIDGE_config_t::n_mac_learning_count */
	MXL862XX_BRIDGE_CONFIG_MASK_MAC_LEARNED_COUNT = 0x00000002,
	/** Mask for \ref MXL862XX_BRIDGE_config_t::n_learning_discard_event */
	MXL862XX_BRIDGE_CONFIG_MASK_MAC_DISCARD_COUNT = 0x00000004,
	/** Mask for \ref MXL862XX_BRIDGE_config_t::b_sub_metering_enable and
	    \ref MXL862XX_BRIDGE_config_t::n_traffic_sub_meter_id */
	MXL862XX_BRIDGE_CONFIG_MASK_SUB_METER = 0x00000008,
	/** Mask for \ref MXL862XX_BRIDGE_config_t::e_forward_broadcast,
	    \ref MXL862XX_BRIDGE_config_t::e_forward_unknown_multicast_ip,
	    \ref MXL862XX_BRIDGE_config_t::e_forward_unknown_multicast_non_ip,
	    and \ref MXL862XX_BRIDGE_config_t::e_forward_unknown_unicast. */
	MXL862XX_BRIDGE_CONFIG_MASK_FORWARDING_MODE = 0x00000010,

	/** Enable all */
	MXL862XX_BRIDGE_CONFIG_MASK_ALL = 0x7FFFFFFF,
	/** Bypass any check for debug purpose */
	MXL862XX_BRIDGE_CONFIG_MASK_FORCE = 0x80000000
} mxl862xx_bridge_config_mask_t;

/** \brief Bridge forwarding type of packet.
    Used by \ref MeXL862XX_BRIDGE_port_config_t. */
typedef enum {
	/** Packet is flooded to port members of ingress bridge port */
	MXL862XX_BRIDGE_FORWARD_FLOOD = 0,
	/** Packet is dscarded */
	MXL862XX_BRIDGE_FORWARD_DISCARD = 1,
	/** Packet is forwarded to logical port 0 CTP port 0 bridge port 0 */
	MXL862XX_BRIDGE_FORWARD_CPU = 2
} mxl862xx_bridge_forward_mode_t;

/** \brief Bridge Configuration.
    Used by \ref MXL862XX_BRIDGE_CONFIG_SET and \ref MXL862XX_BRIDGE_CONFIG_GET. */
typedef struct {
	/** Bridge ID (FID) allocated by \ref MXL862XX_BRIDGE_ALLOC.

	    \remarks
	    If \ref MXL862XX_BRIDGE_config_t::e_mask has
	    \ref MXL862XX_Bridge_config_mask_t::MXL862XX_BRIDGE_CONFIG_MASK_FORCE, this field is
	    absolute index of Bridge (FID) in hardware for debug purpose, bypassing
	    any check. */
	u16 bridge_id;

	/** Mask for updating/retrieving fields. */
	mxl862xx_bridge_config_mask_t mask;

	/** Enable MAC learning limitation. */
	bool mac_learning_limit_enable;
	/** Max number of MAC can be learned in this bridge (all bridge ports). */
	u16 mac_learning_limit;

	/** Get number of MAC address learned from this bridge port. */
	u16 mac_learning_count;

	/** Number of learning discard event due to hardware resource not available.

	    \remarks
	    This is discard event due to either MAC table full or Hash collision.
	    Discard due to n_mac_learning_count reached is not counted in this field. */
	u32 learning_discard_event;

	/** Traffic metering on type of traffic (such as broadcast, multicast,
	    unknown unicast, etc) applies. */
	bool sub_metering_enable[MXL862XX_BRIDGE_PORT_EGRESS_METER_MAX];
	/** Meter for bridge process with specific type (such as broadcast,
	    multicast, unknown unicast, etc). Need pre-allocated for each type. */
	u16 traffic_sub_meter_id[MXL862XX_BRIDGE_PORT_EGRESS_METER_MAX];

	/** Forwarding mode of broadcast traffic. */
	mxl862xx_bridge_forward_mode_t forward_broadcast;
	/** Forwarding mode of unknown multicast IP traffic. */
	mxl862xx_bridge_forward_mode_t forward_unknown_multicast_ip;
	/** Forwarding mode of unknown multicast non-IP traffic. */
	mxl862xx_bridge_forward_mode_t forward_unknown_multicast_non_ip;
	/** Forwarding mode of unknown unicast traffic. */
	mxl862xx_bridge_forward_mode_t forward_unknown_unicast;
} mxl862xx_bridge_config_t;

/** \brief Aging Timer Value.
    Used by \ref mxl862xx_cfg_t. */
typedef enum {
	/** 1 second aging time */
	MXL862XX_AGETIMER_1_SEC	= 1,
	/** 10 seconds aging time */
	MXL862XX_AGETIMER_10_SEC	= 2,
	/** 300 seconds aging time */
	MXL862XX_AGETIMER_300_SEC	= 3,
	/** 1 hour aging time */
	MXL862XX_AGETIMER_1_HOUR	= 4,
	/** 24 hours aging time */
	MXL862XX_AGETIMER_1_DAY	= 5,
	/** Custom aging time in seconds */
	MXL862XX_AGETIMER_CUSTOM  = 6
} mxl862xx_age_timer_t;

/** \brief Global Switch configuration Attributes.
    Used by \ref MXL862XX_CFG_SET and \ref MXL862XX_CFG_GET. */
typedef struct {
	/** MAC table aging timer. After this timer expires the MAC table
	    entry is aged out. */
	mxl862xx_age_timer_t mac_table_age_timer;
	/** If eMAC_TableAgeTimer = MXL862XX_AGETIMER_CUSTOM, this variable defines
	    MAC table aging timer in seconds. */
	u32 age_timer;
	/** Maximum Ethernet packet length. */
	u16 max_packet_len;
	/** Automatic MAC address table learning limitation consecutive action.
	    These frame addresses are not learned, but there exists control as to whether
	    the frame is still forwarded or dropped.

	    - False: Drop
	    - True: Forward
	*/
	bool learning_limit_action;
	/** Accept or discard MAC port locking violation packets.
	    MAC spoofing detection features identifies ingress packets that carry
	    a MAC source address which was previously learned on a different
	    ingress port (learned by MAC bridging table). This also applies to
	    static added entries. MAC address port locking is configured on
	    port level by 'bLearningMAC_PortLock'.

	    - False: Drop
		 - True: Forward
	*/
	bool mac_locking_action;
	/** Accept or discard MAC spoofing and port MAC locking violation packets.
	    MAC spoofing detection features identifies ingress packets that carry
	    a MAC source address which was previously learned on a different
	    ingress port (learned by MAC bridging table). This also applies to
	    static added entries. MAC spoofing detection is enabled on port
	    level by 'bMAC_SpoofingDetection'.

	    - False: Drop
	    - True: Forward
	*/
	bool mac_spoofing_action;
	/** Pause frame MAC source address mode. If enabled, use the alternative
	    address specified with 'nMAC'. */
	bool pause_mac_mode_src;
	/** Pause frame MAC source address. */
	u8	pause_mac_src[ETH_ALEN];
} mxl862xx_cfg_t;

/** \brief Sets the portmap flag of a port_iD variable.
    Some Switch API commands allow to use a port index as portmap variable.
    This requires that the MSB bit is set to indicate that this variable
    contains a portmap, instead of a port index.
    In portmap mode, every value bit represents an Ethernet port.
    LSB represents Port 0 with incrementing counting.
    The (MSB - 1) bit represent the last port. */
#define MXL862XX_PORTMAP_FLAG_SET(var_type) (1 << (sizeof(((var_type *)0)->port_id) * 8 - 1))

/** \brief Checks the portmap flag of a port_iD variable.
    Some Switch API commands allow to use a port index as portmap variable.
    This requires that the MSB bit is set to indicate that this variable
    contains a portmap, instead of a port index.
    In portmap mode, every value bit represents an Ethernet port.
    LSB represents Port 0 with incrementing counting.
    The (MSB - 1) bit represent the last port. */
#define MXL862XX_PORTMAP_FLAG_GET(var_type) (1 << (sizeof(((var_type *)0)->port_id) * 8 - 1))

#pragma scalar_storage_order default
#pragma pack(pop)

#include "mxl862xx_ctp.h"
#include "mxl862xx_flow.h"

struct sys_fw_image_version {
	uint8_t iv_major;
	uint8_t iv_minor;
	uint16_t iv_revision;
	uint32_t iv_build_num;
};

int sys_misc_fw_version(const mxl862xx_device_t *dummy,
			struct sys_fw_image_version *sys_img_ver);

int mxl862xx_register_mod(const mxl862xx_device_t *, mxl862xx_register_mod_t *);
int mxl862xx_port_link_cfg_set(const mxl862xx_device_t *, mxl862xx_port_link_cfg_t *);
int mxl862xx_port_link_cfg_get(const mxl862xx_device_t *, mxl862xx_port_link_cfg_t *);
int mxl862xx_port_cfg_set(const mxl862xx_device_t *, mxl862xx_port_cfg_t *);
int mxl862xx_port_cfg_get(const mxl862xx_device_t *, mxl862xx_port_cfg_t *);
/* Bridge */
int mxl862xx_bridge_alloc(const mxl862xx_device_t *, mxl862xx_bridge_alloc_t *);
int mxl862xx_bridge_free(const mxl862xx_device_t *, mxl862xx_bridge_alloc_t *);
int mxl862xx_bridge_config_set(const mxl862xx_device_t *dev, mxl862xx_bridge_config_t *);
int mxl862xx_bridge_config_get(const mxl862xx_device_t *dev, mxl862xx_bridge_config_t *);
/* Bridge Port */
int mxl862xx_bridge_port_alloc(const mxl862xx_device_t *,
					 mxl862xx_bridge_port_alloc_t *);
int mxl862xx_bridge_port_free(const mxl862xx_device_t *,
					 mxl862xx_bridge_port_alloc_t *);
int mxl862xx_bridge_port_config_set(const mxl862xx_device_t *,
				    mxl862xx_bridge_port_config_t *);
int mxl862xx_bridge_port_config_get(const mxl862xx_device_t *,
				    mxl862xx_bridge_port_config_t *);
/* Debug */
int mxl862xx_debug_rmon_port_get(const mxl862xx_device_t *,
				 mxl862xx_debug_rmon_port_cnt_t *);

int mxl862xx_mac_table_clear(const mxl862xx_device_t *);
int mxl862xx_mac_table_clear_cond(const mxl862xx_device_t *,
				  mxl862xx_mac_table_clear_cond_t *);
int mxl862xx_mac_table_entry_read(const mxl862xx_device_t *, mxl862xx_mac_table_read_t *);
int mxl862xx_mac_table_entry_add(const mxl862xx_device_t *, mxl862xx_mac_table_add_t *);
int mxl862xx_mac_table_entry_remove(const mxl862xx_device_t *,
              mxl862xx_mac_table_remove_t *);
int mxl862xx_stp_port_cfg_set(const mxl862xx_device_t *, mxl862xx_stp_port_cfg_t *parm);

int mxl862xx_ss_sp_tag_get(const mxl862xx_device_t *, mxl862xx_ss_sp_tag_t *);
int mxl862xx_ss_sp_tag_set(const mxl862xx_device_t *, mxl862xx_ss_sp_tag_t *);

int mxl862xx_ctp_port_config_get(const mxl862xx_device_t *, mxl862xx_ctp_port_config_t *);
int mxl862xx_ctp_port_config_set(const mxl862xx_device_t *, mxl862xx_ctp_port_config_t *);
int mxl862xx_ctp_port_assignment_set(const mxl862xx_device_t *,
				     mxl862xx_ctp_port_assignment_t *);
int mxl862xx_ctp_port_assignment_get(const mxl862xx_device_t *,
				     mxl862xx_ctp_port_assignment_t *);
int mxl862xx_monitor_port_cfg_get(const mxl862xx_device_t *, mxl862xx_monitor_port_cfg_t *);
int mxl862xx_monitor_port_cfg_set(const mxl862xx_device_t *, mxl862xx_monitor_port_cfg_t *);

int mxl862xx_extended_vlan_alloc(const mxl862xx_device_t *,
				 mxl862xx_extendedvlan_alloc_t *);
int mxl862xx_extended_vlan_set(const mxl862xx_device_t *,
			       mxl862xx_extendedvlan_config_t *);
int mxl862xx_extended_vlan_get(const mxl862xx_device_t *,
			       mxl862xx_extendedvlan_config_t *);
int mxl862xx_extended_vlan_free(const mxl862xx_device_t *,
				mxl862xx_extendedvlan_alloc_t *);
int mxl862xx_vlan_filter_alloc(const mxl862xx_device_t *, mxl862xx_vlanfilter_alloc_t *);
int mxl862xx_vlan_filter_set(const mxl862xx_device_t *, mxl862xx_vlanfilter_config_t *);
int mxl862xx_vlan_filter_get(const mxl862xx_device_t *, mxl862xx_vlanfilter_config_t *);
int mxl862xx_vlan_filter_free(const mxl862xx_device_t *, mxl862xx_vlanfilter_alloc_t *);
int mxl862xx_cfg_get(const mxl862xx_device_t *, mxl862xx_cfg_t *);
int mxl862xx_cfg_set(const mxl862xx_device_t *, mxl862xx_cfg_t *);
#endif /* _MXL862XX_API_H_ */
