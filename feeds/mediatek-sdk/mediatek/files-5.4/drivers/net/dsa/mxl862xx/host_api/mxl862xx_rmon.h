// SPDX-License-Identifier: GPL-2.0
/*
 * drivers/net/dsa/host_api/mxl862xx_rmon.h - dsa driver for Maxlinear mxl862xx switch chips family
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

#ifndef _MXL862XX_RMON_H_
#define _MXL862XX_RMON_H_

#include "mxl862xx_types.h"

#pragma pack(push, 1)
#pragma scalar_storage_order little-endian

/** \brief Port Type - GSWIP-3.1 only.
   Used by \ref MXL862XX_port_cfg_t. */
typedef enum {
	/** Logical Port */
	MXL862XX_LOGICAL_PORT = 0,
	/** Physical Port
	    Applicable only for GSWIP-3.1/3.2 */
	MXL862XX_PHYSICAL_PORT = 1,
	/** Connectivity Termination Port (CTP)
	    Applicable only for GSWIP-3.1/3.2 */
	MXL862XX_CTP_PORT = 2,
	/** Bridge Port
	    Applicable only for GSWIP-3.1/3.2 */
	MXL862XX_BRIDGE_PORT = 3
} mxl862xx_port_type_t;

/**Defined as per RMON counter table structure
  Applicable only for GSWIP 3.1*/
typedef enum {
	MXL862XX_RMON_CTP_PORT_RX = 0,
	MXL862XX_RMON_CTP_PORT_TX = 1,
	MXL862XX_RMON_BRIDGE_PORT_RX = 2,
	MXL862XX_RMON_BRIDGE_PORT_TX = 3,
	MXL862XX_RMON_CTP_PORT_PCE_BYPASS = 4,
	MXL862XX_RMON_TFLOW_RX = 5,
	MXL862XX_RMON_TFLOW_TX = 6,
	MXL862XX_RMON_QMAP = 0x0E,
	MXL862XX_RMON_METER = 0x19,
	MXL862XX_RMON_PMAC = 0x1C,
} mxl862xx_rmon_port_type_t;

/** Defined as per RMON counter table structure
 * Applicable only for GSWIP 3.0
 */
typedef enum {
	MXL862XX_RMON_REDIRECTION = 0X18,
	MXL862XX_RMON_IF = 0x1A,
	MXL862XX_RMON_ROUTE = 0x1B,
	MXL862XX_RMON_PMACIG = 0x1C,
} mxl862xx_rmon_port_t;

typedef enum {
	/** Parser microcode table */
	PMAC_BPMAP_INDEX = 0x00,
	PMAC_IGCFG_INDEX = 0x01,
	PMAC_EGCFG_INDEX = 0x02,
} pm_tbl_cmds_t;
/** \brief RMON Counters Type enumeration.
    Used by \ref MXL862XX_RMON_clear_t and \ref MXL862XX_RMON_mode_t. */
typedef enum {
	/** All RMON Types Counters */
	MXL862XX_RMON_ALL_TYPE = 0,
	/** All PMAC RMON Counters */
	MXL862XX_RMON_PMAC_TYPE = 1,
	/** Port based RMON Counters */
	MXL862XX_RMON_PORT_TYPE = 2,
	/** Meter based RMON Counters */
	MXL862XX_RMON_METER_TYPE = 3,
	/** Interface based RMON Counters */
	MXL862XX_RMON_IF_TYPE = 4,
	/** Route based RMON Counters */
	MXL862XX_RMON_ROUTE_TYPE = 5,
	/** Redirected Traffic based RMON Counters */
	MXL862XX_RMON_REDIRECT_TYPE = 6,
	/** Bridge Port based RMON Counters */
	MXL862XX_RMON_BRIDGE_TYPE = 7,
	/** CTP Port based RMON Counters */
	MXL862XX_RMON_CTP_TYPE = 8,
} mxl862xx_rmon_type_t;

/** \brief RMON Counters Mode Enumeration.
    This enumeration defines Counters mode - Packets based or Bytes based counting.
    Metering and Routing Sessions RMON counting support either Byte based or packets based only. */
typedef enum {
	/** Packet based RMON Counters */
	MXL862XX_RMON_COUNT_PKTS = 0,
	/** Bytes based RMON Counters */
	MXL862XX_RMON_COUNT_BYTES = 1,
	/**  number of dropped frames, supported only for interface cunters */
	MXL862XX_RMON_DROP_COUNT = 2,
} mxl862xx_rmon_count_mode_t;

/** \brief Used for getting metering RMON counters.
    Used by \ref MXL862XX_RMON_METER_GET */
typedef enum {
	/* Resereved */
	MXL862XX_RMON_METER_COLOR_RES = 0,
	/* Green color */
	MXL862XX_RMON_METER_COLOR_GREEN = 1,
	/* Yellow color */
	MXL862XX_RMON_METER_COLOR_YELLOW = 2,
	/* Red color */
	MXL862XX_RMON_METER_COLOR_RED = 3,
} mxl862xx_rmon_meter_color_t;

/* TFLOW counter type */
typedef enum {
	/* Set all Rx/Tx/PCE-Bp-Tx registers to same value */
	MXL862XX_TFLOW_COUNTER_ALL = 0, //Default for 'set' function.
	/* SEt PCE Rx register config only */
	MXL862XX_TFLOW_COUNTER_PCE_Rx = 1, //Default for 'get' function.
	/* SEt PCE Tx register config only */
	MXL862XX_TFLOW_COUNTER_PCE_Tx = 2,
	/* SEt PCE-Bypass Tx register config only */
	MXL862XX_TFLOW_COUNTER_PCE_BP_Tx = 3,
} mxl862xx_tflow_count_conf_type_t;

/* TFLOW counter mode type */
typedef enum {
	/* Global mode */
	MXL862XX_TFLOW_CMODE_GLOBAL = 0,
	/* Logical mode */
	MXL862XX_TFLOW_CMODE_LOGICAL = 1,
	/* CTP port mode */
	MXL862XX_TFLOW_CMODE_CTP = 2,
	/* Bridge port mode */
	MXL862XX_TFLOW_CMODE_BRIDGE = 3,
} mxl862xx_tflow_cmode_type_t;

/* TFLOW CTP counter LSB bits */
typedef enum {
	/* Num of valid bits  */
	MXL862XX_TCM_CTP_VAL_BITS_0 = 0,
	MXL862XX_TCM_CTP_VAL_BITS_1 = 1,
	MXL862XX_TCM_CTP_VAL_BITS_2 = 2,
	MXL862XX_TCM_CTP_VAL_BITS_3 = 3,
	MXL862XX_TCM_CTP_VAL_BITS_4 = 4,
	MXL862XX_TCM_CTP_VAL_BITS_5 = 5,
	MXL862XX_TCM_CTP_VAL_BITS_6 = 6,
} mxl862xx_tflow_ctp_val_bits_t;

/* TFLOW bridge port counter LSB bits */
typedef enum {
	/* Num of valid bits  */
	MXL862XX_TCM_BRP_VAL_BITS_2 = 2,
	MXL862XX_TCM_BRP_VAL_BITS_3 = 3,
	MXL862XX_TCM_BRP_VAL_BITS_4 = 4,
	MXL862XX_TCM_BRP_VAL_BITS_5 = 5,
	MXL862XX_TCM_BRP_VAL_BITS_6 = 6,
} mxl862xx_tflow_brp_val_bits_t;

/**
 \brief RMON Counters for individual Port.
 This structure contains the RMON counters of an Ethernet Switch Port.
    Used by \ref MXL862XX_RMON_PORT_GET. */
typedef struct {
	/** Port Type. This gives information which type of port to get RMON.
	    port_id should be based on this field.
	    This is new in GSWIP-3.1. For GSWIP-2.1/2.2/3.0, this field is always
	    ZERO (MXL862XX_LOGICAL_PORT). */
	mxl862xx_port_type_t port_type;
	/** Ethernet Port number (zero-based counting). The valid range is hardware
	    dependent. An error code is delivered if the selected port is not
	    available. This parameter specifies for which MAC port the RMON
	    counter is read. It has to be set by the application before
	    calling \ref MXL862XX_RMON_PORT_GET. */
	u16 port_id;
	/** Sub interface ID group. The valid range is hardware/protocol dependent.

	    \remarks
	    This field is valid when \ref MXL862XX_RMON_Port_cnt_t::e_port_type is
	    \ref MXL862XX_port_type_t::MXL862XX_CTP_PORT.
	    Sub interface ID group is defined for each of \ref MXL862XX_Logical_port_mode_t.
	    For both \ref MXL862XX_LOGICAL_PORT_8BIT_WLAN and
	    \ref MXL862XX_LOGICAL_PORT_9BIT_WLAN, this field is VAP.
	    For \ref MXL862XX_LOGICAL_PORT_GPON, this field is GEM index.
	    For \ref MXL862XX_LOGICAL_PORT_EPON, this field is stream index.
	    For \ref MXL862XX_LOGICAL_PORT_GINT, this field is LLID.
	    For others, this field is 0. */
	u16 sub_if_id_group;
	/** Separate set of CTP Tx counters when PCE is bypassed. GSWIP-3.1 only.*/
	bool pce_bypass;
	/*Applicable only for GSWIP 3.1*/
	/** Discarded at Extended VLAN Operation Packet Count. GSWIP-3.1 only. */
	u32 rx_extended_vlan_discard_pkts;
	/** Discarded MTU Exceeded Packet Count. GSWIP-3.1 only. */
	u32 mtu_exceed_discard_pkts;
	/** Tx Undersize (<64) Packet Count. GSWIP-3.1 only. */
	u32 tx_under_size_good_pkts;
	/** Tx Oversize (>1518) Packet Count. GSWIP-3.1 only. */
	u32 tx_oversize_good_pkts;
	/** Receive Packet Count (only packets that are accepted and not discarded). */
	u32 rx_good_pkts;
	/** Receive Unicast Packet Count. */
	u32 rx_unicast_pkts;
	/** Receive Broadcast Packet Count. */
	u32 rx_broadcast_pkts;
	/** Receive Multicast Packet Count. */
	u32 rx_multicast_pkts;
	/** Receive FCS Error Packet Count. */
	u32 rx_fCSError_pkts;
	/** Receive Undersize Good Packet Count. */
	u32 rx_under_size_good_pkts;
	/** Receive Oversize Good Packet Count. */
	u32 rx_oversize_good_pkts;
	/** Receive Undersize Error Packet Count. */
	u32 rx_under_size_error_pkts;
	/** Receive Good Pause Packet Count. */
	u32 rx_good_pause_pkts;
	/** Receive Oversize Error Packet Count. */
	u32 rx_oversize_error_pkts;
	/** Receive Align Error Packet Count. */
	u32 rx_align_error_pkts;
	/** Filtered Packet Count. */
	u32 rx_filtered_pkts;
	/** Receive Size 64 Bytes Packet Count. */
	u32 rx64Byte_pkts;
	/** Receive Size 65-127 Bytes Packet Count. */
	u32 rx127Byte_pkts;
	/** Receive Size 128-255 Bytes Packet Count. */
	u32 rx255Byte_pkts;
	/** Receive Size 256-511 Bytes Packet Count. */
	u32 rx511Byte_pkts;
	/** Receive Size 512-1023 Bytes Packet Count. */
	u32 rx1023Byte_pkts;
	/** Receive Size 1024-1522 Bytes (or more, if configured) Packet Count. */
	u32 rx_max_byte_pkts;
	/** Overall Transmit Good Packets Count. */
	u32 tx_good_pkts;
	/** Transmit Unicast Packet Count. */
	u32 tx_unicast_pkts;
	/** Transmit Broadcast Packet Count. */
	u32 tx_broadcast_pkts;
	/** Transmit Multicast Packet Count. */
	u32 tx_multicast_pkts;
	/** Transmit Single Collision Count. */
	u32 tx_single_coll_count;
	/** Transmit Multiple Collision Count. */
	u32 tx_mult_coll_count;
	/** Transmit Late Collision Count. */
	u32 tx_late_coll_count;
	/** Transmit Excessive Collision Count. */
	u32 tx_excess_coll_count;
	/** Transmit Collision Count. */
	u32 tx_coll_count;
	/** Transmit Pause Packet Count. */
	u32 tx_pause_count;
	/** Transmit Size 64 Bytes Packet Count. */
	u32 tx64Byte_pkts;
	/** Transmit Size 65-127 Bytes Packet Count. */
	u32 tx127Byte_pkts;
	/** Transmit Size 128-255 Bytes Packet Count. */
	u32 tx255Byte_pkts;
	/** Transmit Size 256-511 Bytes Packet Count. */
	u32 tx511Byte_pkts;
	/** Transmit Size 512-1023 Bytes Packet Count. */
	u32 tx1023Byte_pkts;
	/** Transmit Size 1024-1522 Bytes (or more, if configured) Packet Count. */
	u32 tx_max_byte_pkts;
	/** Transmit Drop Packet Count. */
	u32 tx_dropped_pkts;
	/** Transmit Dropped Packet Count, based on Congestion Management. */
	u32 tx_acm_dropped_pkts;
	/** Receive Dropped Packet Count. */
	u32 rx_dropped_pkts;
	/** Receive Good Byte Count (64 bit). */
	u64 rx_good_bytes;
	/** Receive Bad Byte Count (64 bit). */
	u64 rx_bad_bytes;
	/** Transmit Good Byte Count (64 bit). */
	u64 tx_good_bytes;
} mxl862xx_rmon_port_cnt_t;

/** \brief RMON Counters Mode for different Elements.
    This structure takes RMON Counter Element Name and mode config */
typedef struct {
	/** RMON Counters Type */
	mxl862xx_rmon_type_t rmon_type;
	/** RMON Counters Mode */
	mxl862xx_rmon_count_mode_t count_mode;
} mxl862xx_rmon_mode_t;

/**
 \brief RMON Counters for Meter - Type (GSWIP-3.0 only).
 This structure contains the RMON counters of one Meter Instance.
    Used by \ref MXL862XX_RMON_METER_GET. */
typedef struct {
	/** Meter Instance number (zero-based counting). The valid range is hardware
	    dependent. An error code is delivered if the selected meter is not
	    available. This parameter specifies for which Meter Id the RMON-1
	    counter is read. It has to be set by the application before
	    calling \ref MXL862XX_RMON_METER_GET. */
	u8 meter_id;
	/** Metered Green colored packets or bytes (depending upon mode) count. */
	u32 green_count;
	/** Metered Yellow colored packets or bytes (depending upon mode) count. */
	u32 yellow_count;
	/** Metered Red colored packets or bytes (depending upon mode) count. */
	u32 red_count;
	/** Metered Reserved (Future Use) packets or bytes (depending upon mode) count. */
	u32 res_count;
} mxl862xx_rmon_meter_cnt_t;

/** \brief RMON Counters Data Structure for clearance of values.
    Used by \ref MXL862XX_RMON_CLEAR. */
typedef struct {
	/** RMON Counters Type */
	mxl862xx_rmon_type_t rmon_type;
	/** RMON Counters Identifier - Meter, Port, If, Route, etc. */
	u8 rmon_id;
} mxl862xx_rmon_clear_t;

/**
   \brief Hardware platform TFLOW counter mode.
   Supported modes include, Global (default), Logical, CTP, Bridge port mode.
   The number of counters that can be assigned varies based these mode type.
    Used by \ref MXL862XX_TFLOW_COUNT_MODE_SET and MXL862XX_TFLOW_COUNT_MODE_GET. */
typedef struct {
	//Counter type. PCE Rx/Tx/Bp-Tx.
	mxl862xx_tflow_count_conf_type_t count_type;
	//Counter mode. Global/Logical/CTP/br_p.
	mxl862xx_tflow_cmode_type_t count_mode;
	//The below params are valid only for CTP/br_p types.
	//A group of ports matching MS (9-n) bits. n is n_ctp_lsb or n_brp_lsb.
	u16 port_msb;
	//Number of valid bits in CTP port counter mode.
	mxl862xx_tflow_ctp_val_bits_t ctp_lsb;
	//Number of valid bits in bridge port counter mode.
	mxl862xx_tflow_brp_val_bits_t brp_lsb;
} mxl862xx_tflow_cmode_conf_t;

/**
   \brief Hardware platform extended RMON Counters. GSWIP-3.1 only.
   This structure contains additional RMON counters. These counters can be
   used by the packet classification engine and can be freely assigned to
   dedicated packet rules and flows.
    Used by \ref MXL862XX_RMON_FLOW_GET. */
typedef struct {
	/** If TRUE, use \ref MXL862XX_RMON_flow_get_t::n_index to access the Flow Counter,
	    otherwise, use \ref MXL862XX_TFLOW_COUNT_MODE_GET to determine mode and use
	    \ref MXL862XX_RMON_flow_get_t::port_id and \ref MXL862XX_RMON_flow_get_t::flow_id
	    to calculate index of the Flow Counter. */
	bool use_index;
	/** Absolute index of Flow Counter. */
	u16 index;
	/** Port ID. This could be Logical Port, CTP or Bridge Port. It depends
	    on the mode set by \ref MXL862XX_TFLOW_COUNT_MODE_SET. */
	u16 port_id;
	/** \ref MXL862XX_PCE_action_t::flow_id. The range depends on the mode set
	    by \ref MXL862XX_TFLOW_COUNT_MODE_SET. */
	u16 flow_id;

	/** Rx Packet Counter */
	u32 rx_pkts;
	/** Tx Packet Counter (non-PCE-Bypass) */
	u32 tx_pkts;
	/** Tx Packet Counter (PCE-Bypass) */
	u32 tx_pce_bypass_pkts;
} mxl862xx_rmon_flow_get_t;

/** \brief For debugging Purpose only.
    Used for GSWIP 3.1. */

typedef struct {
	/** Ethernet Port number (zero-based counting). The valid range is hardware
	    dependent. An error code is delivered if the selected port is not
	    available. This parameter specifies for which MAC port the RMON
	    counter is read. It has to be set by the application before
	    calling \ref MXL862XX_RMON_PORT_GET. */
	u16 port_id;
	/**Table address selection based on port type
	  Applicable only for GSWIP 3.1
	 \ref MXL862XX_RMON_port_type_t**/
	mxl862xx_rmon_port_type_t port_type;
	/*Applicable only for GSWIP 3.1*/
	u32 rx_extended_vlan_discard_pkts;
	/*Applicable only for GSWIP 3.1*/
	u32 mtu_exceed_discard_pkts;
	/*Applicable only for GSWIP 3.1*/
	u32 tx_under_size_good_pkts;
	/*Applicable only for GSWIP 3.1*/
	u32 tx_oversize_good_pkts;
	/** Receive Packet Count (only packets that are accepted and not discarded). */
	u32 rx_good_pkts;
	/** Receive Unicast Packet Count. */
	u32 rx_unicast_pkts;
	/** Receive Broadcast Packet Count. */
	u32 rx_broadcast_pkts;
	/** Receive Multicast Packet Count. */
	u32 rx_multicast_pkts;
	/** Receive FCS Error Packet Count. */
	u32 rx_fcserror_pkts;
	/** Receive Undersize Good Packet Count. */
	u32 rx_under_size_good_pkts;
	/** Receive Oversize Good Packet Count. */
	u32 rx_oversize_good_pkts;
	/** Receive Undersize Error Packet Count. */
	u32 rx_under_size_error_pkts;
	/** Receive Good Pause Packet Count. */
	u32 rx_good_pause_pkts;
	/** Receive Oversize Error Packet Count. */
	u32 rx_oversize_error_pkts;
	/** Receive Align Error Packet Count. */
	u32 rx_align_error_pkts;
	/** Filtered Packet Count. */
	u32 rx_filtered_pkts;
	/** Receive Size 64 Bytes Packet Count. */
	u32 rx64byte_pkts;
	/** Receive Size 65-127 Bytes Packet Count. */
	u32 rx127byte_pkts;
	/** Receive Size 128-255 Bytes Packet Count. */
	u32 rx255byte_pkts;
	/** Receive Size 256-511 Bytes Packet Count. */
	u32 rx511byte_pkts;
	/** Receive Size 512-1023 Bytes Packet Count. */
	u32 rx1023byte_pkts;
	/** Receive Size 1024-1522 Bytes (or more, if configured) Packet Count. */
	u32 rx_max_byte_pkts;
	/** Overall Transmit Good Packets Count. */
	u32 tx_good_pkts;
	/** Transmit Unicast Packet Count. */
	u32 tx_unicast_pkts;
	/** Transmit Broadcast Packet Count. */
	u32 tx_broadcast_pkts;
	/** Transmit Multicast Packet Count. */
	u32 tx_multicast_pkts;
	/** Transmit Single Collision Count. */
	u32 tx_single_coll_count;
	/** Transmit Multiple Collision Count. */
	u32 tx_mult_coll_count;
	/** Transmit Late Collision Count. */
	u32 tx_late_coll_count;
	/** Transmit Excessive Collision Count. */
	u32 tx_excess_coll_count;
	/** Transmit Collision Count. */
	u32 tx_coll_count;
	/** Transmit Pause Packet Count. */
	u32 tx_pause_count;
	/** Transmit Size 64 Bytes Packet Count. */
	u32 tx64byte_pkts;
	/** Transmit Size 65-127 Bytes Packet Count. */
	u32 tx127byte_pkts;
	/** Transmit Size 128-255 Bytes Packet Count. */
	u32 tx255byte_pkts;
	/** Transmit Size 256-511 Bytes Packet Count. */
	u32 tx511byte_pkts;
	/** Transmit Size 512-1023 Bytes Packet Count. */
	u32 tx1023byte_pkts;
	/** Transmit Size 1024-1522 Bytes (or more, if configured) Packet Count. */
	u32 tx_max_byte_pkts;
	/** Transmit Drop Packet Count. */
	u32 tx_dropped_pkts;
	/** Transmit Dropped Packet Count, based on Congestion Management. */
	u32 tx_acm_dropped_pkts;
	/** Receive Dropped Packet Count. */
	u32 rx_dropped_pkts;
	/** Receive Good Byte Count (64 bit). */
	u64 rx_good_bytes;
	/** Receive Bad Byte Count (64 bit). */
	u64 rx_bad_bytes;
	/** Transmit Good Byte Count (64 bit). */
	u64 tx_good_bytes;
	/**For GSWIP V32 only **/
	/** Receive Unicast Packet Count for Yellow & Red packet. */
	u32 rx_unicast_pkts_yellow_red;
	/** Receive Broadcast Packet Count for Yellow & Red packet. */
	u32 rx_broadcast_pkts_yellow_red;
	/** Receive Multicast Packet Count for Yellow & Red packet. */
	u32 rx_multicast_pkts_yellow_red;
	/** Receive Good Byte Count (64 bit) for Yellow & Red packet. */
	u64 rx_good_bytes_yellow_red;
	/** Receive Packet Count for Yellow & Red packet.  */
	u32 rx_good_pkts_yellow_red;
	/** Transmit Unicast Packet Count for Yellow & Red packet. */
	u32 tx_unicast_pkts_yellow_red;
	/** Transmit Broadcast Packet Count for Yellow & Red packet. */
	u32 tx_broadcast_pkts_yellow_red;
	/** Transmit Multicast Packet Count for Yellow & Red packet. */
	u32 tx_multicast_pkts_yellow_red;
	/** Transmit Good Byte Count (64 bit) for Yellow & Red packet. */
	u64 tx_good_bytes_yellow_red;
	/** Transmit Packet Count for Yellow & Red packet.  */
	u32 tx_good_pkts_yellow_red;
} mxl862xx_debug_rmon_port_cnt_t;

#pragma scalar_storage_order default
#pragma pack(pop)

#endif /* _MXL862XX_RMON_H_ */
