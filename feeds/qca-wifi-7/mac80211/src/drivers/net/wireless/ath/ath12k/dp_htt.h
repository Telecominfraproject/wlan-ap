// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_DP_HTT_H
#define ATH12K_DP_HTT_H

struct ath12k_dp;

/* HTT definitions */
#define HTT_TAG_TCL_METADATA_VERSION		5

#define HTT_TCL_META_DATA_TYPE			GENMASK(1, 0)
#define HTT_TCL_META_DATA_VALID_HTT		BIT(2)

/* vdev meta data */
#define HTT_TCL_META_DATA_VDEV_ID		 GENMASK(10, 3)
#define HTT_TCL_META_DATA_PDEV_ID		 GENMASK(12, 11)
#define HTT_TCL_META_DATA_HOST_INSPECTED_MISSION BIT(13)

/* peer meta data */
#define HTT_TCL_META_DATA_PEER_ID		GENMASK(15, 3)

/* Global sequence number */
#define HTT_TCL_META_DATA_TYPE_GLOBAL_SEQ_NUM		3
#define HTT_TCL_META_DATA_GLOBAL_SEQ_HOST_INSPECTED	BIT(2)
#define HTT_TCL_META_DATA_GLOBAL_SEQ_NUM		GENMASK(14, 3)
#define HTT_TX_MLO_MCAST_HOST_REINJECT_BASE_VDEV_ID	128

/* HTT tx completion is overlaid in wbm_release_ring */
#define HTT_TX_WBM_COMP_INFO0_STATUS		GENMASK(16, 13)
#define HTT_TX_WBM_COMP_INFO1_REINJECT_REASON	GENMASK(3, 0)
#define HTT_TX_WBM_COMP_INFO1_EXCEPTION_FRAME	BIT(4)

#define HTT_TX_WBM_COMP_INFO2_PPDU_ID		GENMASK(23, 0)
#define HTT_TX_WBM_COMP_INFO2_ACK_RSSI		GENMASK(31, 24)

#define HTT_TX_WBM_COMP_INFO3_SW_PEER_ID	GENMASK(15, 0)
#define HTT_TX_WBM_COMP_INFO3_TID		GENMASK(20, 16)
#define HTT_TX_WBM_COMP_INFO3_VALID		BIT(21)

#define ATH12K_DSCP_PRIORITY 7

#define SDWF_PEER_ID_SHIFT 0x6
#define SDWF_PEER_ID_MASK 0x3ff
#define SDWF_MSDUQ_MASK 0x3f

#define HTT_TX_WBM_COMPLETION_V3_REINJECT_REASON_M		0x0000000F

#define HAL_TX_COMP_TQM_RELEASE_REASON_MASK             0x0001e000

#define HTT_TX_WBM_REINJECT_SW_PEER_ID_M    0x0000ffff
#define HTT_TX_WBM_REINJECT_DATA_LEN_M      0xffff0000
#define HTT_TX_WBM_REINJECT_TID_M           0x0000001f
#define HTT_TX_WBM_REINJECT_MSDUQ_ID_M      0x000001e0

enum htt_tx_fw2wbm_reinject_reason {
	HTT_TX_FW2WBM_REINJECT_REASON_RAW_ENCAP_EXP,
	HTT_TX_FW2WBM_REINJECT_REASON_INJECT_VIA_EXP,
	HTT_TX_FW2WBM_REINJECT_REASON_MCAST,
	HTT_TX_FW2WBM_REINJECT_REASON_ARP,
	HTT_TX_FW2WBM_REINJECT_REASON_DHCP,
	HTT_TX_FW2WBM_REINJECT_REASON_FLOW_CONTROL,
	HTT_TX_FW2WBM_REINJECT_REASON_MLO_MCAST,
	HTT_TX_FW2WBM_REINJECT_REASON_SDWF_SVC_CLASS_ID_ABSENT,
	HTT_TX_FW2WBM_REINJECT_REASON_OPT_DP_CTRL, /* tx qdata packet */

	HTT_TX_FW2WBM_REINJECT_REASON_MAX,
};

struct htt_tx_wbm_completion {
	__le32 rsvd0[2];
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le32 info3;
	__le32 info4;
	__le32 rsvd1;

} __packed;

enum htt_h2t_msg_type {
	HTT_H2T_MSG_TYPE_VERSION_REQ			= 0,
	HTT_H2T_MSG_TYPE_SRING_SETUP			= 0xb,
	HTT_H2T_MSG_TYPE_RX_RING_SELECTION_CFG		= 0xc,
	HTT_H2T_MSG_TYPE_EXT_STATS_CFG			= 0x10,
	HTT_H2T_MSG_TYPE_PPDU_STATS_CFG			= 0x11,
	HTT_H2T_MSG_TYPE_RX_FSE_SETUP_CFG       	= 0x12,
	HTT_H2T_MSG_TYPE_RX_FSE_OPERATION_CFG   	= 0x13,
	HTT_H2T_MSG_TYPE_RX_FSE_3_TUPLE_HASH_CFG	= 0x16,
	HTT_H2T_MSG_TYPE_RXDMA_RXOLE_PPE_CFG		= 0x19,
	HTT_H2T_MSG_TYPE_VDEV_TXRX_STATS_CFG		= 0x1a,
	HTT_H2T_MSG_TYPE_TX_MONITOR_CFG			= 0x1b,
	HTT_H2T_MSG_TYPE_STREAMING_STATS_REQ		= 0x20,
	HTT_H2T_MSG_TYPE_UMAC_RESET_PREREQUISITE_SETUP  = 0x21,
	HTT_H2T_MSG_TYPE_UMAC_RESET_START_PRE_RESET     = 0x22,
	HTT_H2T_MSG_TYPE_PRIMARY_LINK_PEER_MIGRATE_RESP	= 0x24,
};

#define HTT_ATH12K_UMAC_RESET_T2H_DO_PRE_RESET  BIT(0)
#define HTT_ATH12K_UMAC_RESET_T2H_DO_POST_RESET_START   BIT(1)
#define HTT_ATH12K_UMAC_RESET_T2H_DO_POST_RESET_COMPLETE        BIT(2)
#define HTT_ATH12K_UMAC_RESET_T2H_INIT_UMAC_RECOVERY    BIT(3)
#define HTT_ATH12K_UMAC_RESET_T2H_INIT_TARGET_RECOVERY_SYNC_USING_UMAC  BIT(4)

#define ATH12K_HTT_UMAC_RESET_MSG_SHMEM_PRE_RESET_DONE_SET      BIT(0)
#define ATH12K_HTT_UMAC_RESET_MSG_SHMEM_POST_RESET_START_DONE_SET BIT(1)
#define ATH12K_HTT_UMAC_RESET_MSG_SHMEM_POST_RESET_COMPLETE_DONE BIT(2)

struct ath12k_dp_htt_umac_reset_recovery_msg_shmem_t {
        u32 magic_num;
        union {
                /*
                 * BIT [0]        :- T2H msg to do pre-reset
                 * BIT [1]        :- T2H msg to do post-reset start
                 * BIT [2]        :- T2H msg to do post-reset complete
                 * BIT [3]        :- T2H msg to indicate to Host that
                 *                   a trigger request for MLO UMAC Recovery
                 *                   is received for UMAC hang.
                 * BIT [4]        :- T2H msg to indicate to Host that
                 *                   a trigger request for MLO UMAC Recovery
                 *                   is received for Mode-1 Target Recovery.
                 * BIT [31 : 5]   :- reserved
                 */
                u32 t2h_msg;
                u32 recovery_action;
        };
        union {
                /*
                 * BIT [0]        :- H2T msg to send pre-reset done
                 * BIT [1]        :- H2T msg to send post-reset start done
                 * BIT [2]        :- H2T msg to send post-reset complete done
                 * BIT [3]        :- H2T msg to start pre-reset. This is deprecated.
                 * BIT [31 : 4]   :- reserved
                 */
                u32 h2t_msg;
                u32 recovery_action_done;
        };
};

struct ath12k_htt_umac_reset_setup_cmd_params {
        uint32_t msi_data;
        uint32_t addr_lo;
        uint32_t addr_hi;
};

struct htt_h2t_paddr_size {
        u32 size;
        u32 addr_lo;
        u32 addr_hi;
};

#define HTT_H2T_MSG_TYPE_SET    GENMASK(7, 0)
#define HTT_H2T_MSG_METHOD      GENMASK(11, 8)
#define HTT_T2H_MSG_METHOD      GENMASK(15, 12)

#define ATH12K_DP_UMAC_RESET_SHMEM_MAGIC_NUM    0xDEADBEEF
#define ATH12K_DP_UMAC_RESET_SHMEM_ALIGN        8

/**
 * @brief HTT_H2T_MSG_TYPE_UMAC_RESET_PREREQUISITE_SETUP message
 *
 * @details
 *  The HTT_H2T_MSG_TYPE_UMAC_HANG_RECOVERY_PREREQUISITE_SETUP message is sent
 *  by the host to provide prerequisite info to target for the UMAC hang
 *  recovery feature.
 *  The info sent in this H2T message are T2H message method, H2T message
 *  method, T2H MSI interrupt number and physical start address, size of
 *  the shared memory (refers to the shared memory dedicated for messaging
 *  between host and target when the DUT is in UMAC hang recovery mode).
 *  This H2T message is expected to be only sent if the WMI service bit
 *  WMI_SERVICE_UMAC_HANG_RECOVERY_SUPPORT was firstly indicated by the target.
 *
 * |31                           16|15          12|11           8|7          0|
 * |-------------------------------+--------------+--------------+------------|
 * |            reserved           |h2t msg method|t2h msg method|  msg_type  |
 * |--------------------------------------------------------------------------|
 * |                           t2h msi interrupt number                       |
 * |--------------------------------------------------------------------------|
 * |                           shared memory area size                        |
 * |--------------------------------------------------------------------------|
 * |                     shared memory area physical address low              |
 * |--------------------------------------------------------------------------|
 * |                     shared memory area physical address high             |
 * |--------------------------------------------------------------------------|
  * The message is interpreted as follows:
 * dword0 - b'0:7   - msg_type
 *                    (HTT_H2T_MSG_TYPE_UMAC_RESET_PREREQUISITE_SETUP)
 *          b'8:11  - t2h_msg_method: indicates method to be used for
 *                    T2H communication in UMAC hang recovery mode.
 *                    Value zero indicates MSI interrupt (default method).
 *                    Refer to htt_umac_hang_recovery_msg_method enum.
 *          b'12:15 - h2t_msg_method: indicates method to be used for
 *                    H2T communication in UMAC hang recovery mode.
 *                    Value zero indicates polling by target for this h2t msg
 *                    during UMAC hang recovery mode.
 *                    Refer to htt_umac_hang_recovery_msg_method enum.
 *          b'16:31 - reserved.
 * dword1 - b'0:31  - t2h_msi_data: MSI data to be used for
 *                    T2H communication in UMAC hang recovery mode.
 * dword2 - b'0:31  - size: size of shared memory dedicated for messaging
 *                    only when in UMAC hang recovery mode.
 *                    This refers to size in bytes.
 * dword3 - b'0:31  - physical_address_lo: lower 32 bit physical address
 *                    of the shared memory dedicated for messaging only when
 *                    in UMAC hang recovery mode.
 * dword4 - b'0:31  - physical_address_hi: higher 32 bit physical address
 *                    of the shared memory dedicated for messaging only when
 *                    in UMAC hang recovery mode.
 */

struct htt_dp_umac_reset_setup_req_cmd {
        u32 msg_info;
        u32 msi_data;
        struct htt_h2t_paddr_size msg_shared_mem;
}__packed;

/**
 * @brief HTT_H2T_MSG_TYPE_UMAC_RESET_START_PRE_RESET message
 *
 * @details
 *  The HTT_H2T_MSG_TYPE_UMAC_HANG_RECOVERY_SOC_START_PRE_RESET is a SOC level
 *  HTT message sent by the host to indicate that the target needs to start the
 *  UMAC hang recovery feature from the point of pre-reset routine.
 *  The purpose of this H2T message is to have host synchronize and trigger
 *  UMAC recovery across all targets.
 *  The info sent in this H2T message is the flag to indicate whether the
 *  target needs to execute UMAC-recovery in context of the Initiator or
 *  Non-Initiator.
 *  This H2T message is expected to be sent as response to the
 *  initiate_umac_recovery indication from the Initiator target attached to
 *  this same host.
 *  This H2T message is expected to be only sent if the WMI service bit
 *  WMI_SERVICE_UMAC_HANG_RECOVERY_SUPPORT was firstly indicated by the target
 *  and HTT_H2T_MSG_TYPE_UMAC_HANG_RECOVERY_PREREQUISITE_SETUP was sent
 *  beforehand.
 *
 * |31                                    10|9|8|7            0|
 * |-----------------------------------------------------------|
 * |                 reserved               |U|I|   msg_type   |
 * |-----------------------------------------------------------|
 * Where:
 *     I = is_initiator
 *     U = is_umac_hang
 *
 * The message is interpreted as follows:
 * dword0 - b'0:7   - msg_type
 *                    (HTT_H2T_MSG_TYPE_UMAC_RESET_START_PRE_RESET)
 *          b'8     - is_initiator: indicates whether the target needs to
 *                    execute the UMAC-recovery in context of the Initiator or
 *                    Non-Initiator.
 *                    The value zero indicates this target is Non-Initiator.
 *          b'9     - is_umac_hang: indicates whether MLO UMAC recovery
 *                    executed in context of UMAC hang or Target recovery.
 *          b'10:31 - reserved.
 */
struct h2t_umac_hang_recovery_start_pre_reset {
       u32 hdr;
} __packed;

#define HTT_H2T_UMAC_RESET_MSG_TYPE     GENMASK(7, 0)
#define HTT_H2T_UMAC_RESET_IS_INITIATOR_SET     BIT(8)
#define HTT_H2T_UMAC_RESET_IS_TARGET_RECOVERY_SET       BIT(9)

#define HTT_H2T_RXOLE_PPE_CFG_MSG_TYPE			GENMASK(7, 0)
#define HTT_H2T_RXOLE_PPE_CFG_OVERRIDE			BIT(8)
#define HTT_H2T_RXOLE_PPE_CFG_REO_DST_IND		GENMASK(13, 9)
#define HTT_H2T_RXOLE_PPE_CFG_MULTI_BUF_MSDU_OVRD_EN	BIT(14)
#define HTT_H2T_RXOLE_PPE_CFG_INTRA_BUS_OVRD		BIT(15)
#define HTT_H2T_RXOLE_PPE_CFG_DECAP_RAW_OVRD		BIT(16)
#define HTT_H2T_RXOLE_PPE_CFG_NWIFI_OVRD		BIT(17)
#define HTT_H2T_RXOLE_PPE_CFG_IP_FRAG_OVRD		BIT(18)

struct htt_h2t_msg_type_rxdma_rxole_ppe_cfg {
	u32 info0;
};

#define HTT_VER_REQ_INFO_MSG_ID		GENMASK(7, 0)
#define HTT_OPTION_TCL_METADATA_VER_V2	2
#define HTT_OPTION_TAG			GENMASK(7, 0)
#define HTT_OPTION_LEN			GENMASK(15, 8)
#define HTT_OPTION_VALUE		GENMASK(31, 16)
#define HTT_TCL_METADATA_VER_SZ		4

struct htt_ver_req_cmd {
	__le32 ver_reg_info;
	__le32 tcl_metadata_version;
} __packed;

enum htt_srng_ring_type {
	HTT_HW_TO_SW_RING,
	HTT_SW_TO_HW_RING,
	HTT_SW_TO_SW_RING,
};

enum htt_srng_ring_id {
	HTT_RXDMA_HOST_BUF_RING,
	HTT_RXDMA_MONITOR_STATUS_RING,
	HTT_RXDMA_MONITOR_BUF_RING,
	HTT_RXDMA_MONITOR_DESC_RING,
	HTT_RXDMA_MONITOR_DEST_RING,
	HTT_HOST1_TO_FW_RXBUF_RING,
	HTT_HOST2_TO_FW_RXBUF_RING,
	HTT_RXDMA_NON_MONITOR_DEST_RING,
	HTT_RXDMA_HOST_BUF_RING2,
	HTT_TX_MON_HOST2MON_BUF_RING,
	HTT_TX_MON_MON2HOST_DEST_RING,
	HTT_RX_MON_HOST2MON_BUF_RING,
	HTT_RX_MON_MON2HOST_DEST_RING,
};

/* host -> target  HTT_SRING_SETUP message
 *
 * After target is booted up, Host can send SRING setup message for
 * each host facing LMAC SRING. Target setups up HW registers based
 * on setup message and confirms back to Host if response_required is set.
 * Host should wait for confirmation message before sending new SRING
 * setup message
 *
 * The message would appear as follows:
 *
 * |31            24|23    20|19|18 16|15|14          8|7                0|
 * |--------------- +-----------------+----------------+------------------|
 * |    ring_type   |      ring_id    |    pdev_id     |     msg_type     |
 * |----------------------------------------------------------------------|
 * |                          ring_base_addr_lo                           |
 * |----------------------------------------------------------------------|
 * |                         ring_base_addr_hi                            |
 * |----------------------------------------------------------------------|
 * |ring_misc_cfg_flag|ring_entry_size|            ring_size              |
 * |----------------------------------------------------------------------|
 * |                         ring_head_offset32_remote_addr_lo            |
 * |----------------------------------------------------------------------|
 * |                         ring_head_offset32_remote_addr_hi            |
 * |----------------------------------------------------------------------|
 * |                         ring_tail_offset32_remote_addr_lo            |
 * |----------------------------------------------------------------------|
 * |                         ring_tail_offset32_remote_addr_hi            |
 * |----------------------------------------------------------------------|
 * |                          ring_msi_addr_lo                            |
 * |----------------------------------------------------------------------|
 * |                          ring_msi_addr_hi                            |
 * |----------------------------------------------------------------------|
 * |                          ring_msi_data                               |
 * |----------------------------------------------------------------------|
 * |         intr_timer_th            |IM|      intr_batch_counter_th     |
 * |----------------------------------------------------------------------|
 * |          reserved        |RR|PTCF|        intr_low_threshold         |
 * |----------------------------------------------------------------------|
 * Where
 *     IM = sw_intr_mode
 *     RR = response_required
 *     PTCF = prefetch_timer_cfg
 *
 * The message is interpreted as follows:
 * dword0  - b'0:7   - msg_type: This will be set to
 *                     HTT_H2T_MSG_TYPE_SRING_SETUP
 *           b'8:15  - pdev_id:
 *                     0 (for rings at SOC/UMAC level),
 *                     1/2/3 mac id (for rings at LMAC level)
 *           b'16:23 - ring_id: identify which ring is to setup,
 *                     more details can be got from enum htt_srng_ring_id
 *           b'24:31 - ring_type: identify type of host rings,
 *                     more details can be got from enum htt_srng_ring_type
 * dword1  - b'0:31  - ring_base_addr_lo: Lower 32bits of ring base address
 * dword2  - b'0:31  - ring_base_addr_hi: Upper 32bits of ring base address
 * dword3  - b'0:15  - ring_size: size of the ring in unit of 4-bytes words
 *           b'16:23 - ring_entry_size: Size of each entry in 4-byte word units
 *           b'24:31 - ring_misc_cfg_flag: Valid only for HW_TO_SW_RING and
 *                     SW_TO_HW_RING.
 *                     Refer to HTT_SRING_SETUP_RING_MISC_CFG_RING defs.
 * dword4  - b'0:31  - ring_head_off32_remote_addr_lo:
 *                     Lower 32 bits of memory address of the remote variable
 *                     storing the 4-byte word offset that identifies the head
 *                     element within the ring.
 *                     (The head offset variable has type u32.)
 *                     Valid for HW_TO_SW and SW_TO_SW rings.
 * dword5  - b'0:31  - ring_head_off32_remote_addr_hi:
 *                     Upper 32 bits of memory address of the remote variable
 *                     storing the 4-byte word offset that identifies the head
 *                     element within the ring.
 *                     (The head offset variable has type u32.)
 *                     Valid for HW_TO_SW and SW_TO_SW rings.
 * dword6  - b'0:31  - ring_tail_off32_remote_addr_lo:
 *                     Lower 32 bits of memory address of the remote variable
 *                     storing the 4-byte word offset that identifies the tail
 *                     element within the ring.
 *                     (The tail offset variable has type u32.)
 *                     Valid for HW_TO_SW and SW_TO_SW rings.
 * dword7  - b'0:31  - ring_tail_off32_remote_addr_hi:
 *                     Upper 32 bits of memory address of the remote variable
 *                     storing the 4-byte word offset that identifies the tail
 *                     element within the ring.
 *                     (The tail offset variable has type u32.)
 *                     Valid for HW_TO_SW and SW_TO_SW rings.
 * dword8  - b'0:31  - ring_msi_addr_lo: Lower 32bits of MSI cfg address
 *                     valid only for HW_TO_SW_RING and SW_TO_HW_RING
 * dword9  - b'0:31  - ring_msi_addr_hi: Upper 32bits of MSI cfg address
 *                     valid only for HW_TO_SW_RING and SW_TO_HW_RING
 * dword10 - b'0:31  - ring_msi_data: MSI data
 *                     Refer to HTT_SRING_SETUP_RING_MSC_CFG_xxx defs
 *                     valid only for HW_TO_SW_RING and SW_TO_HW_RING
 * dword11 - b'0:14  - intr_batch_counter_th:
 *                     batch counter threshold is in units of 4-byte words.
 *                     HW internally maintains and increments batch count.
 *                     (see SRING spec for detail description).
 *                     When batch count reaches threshold value, an interrupt
 *                     is generated by HW.
 *           b'15    - sw_intr_mode:
 *                     This configuration shall be static.
 *                     Only programmed at power up.
 *                     0: generate pulse style sw interrupts
 *                     1: generate level style sw interrupts
 *           b'16:31 - intr_timer_th:
 *                     The timer init value when timer is idle or is
 *                     initialized to start downcounting.
 *                     In 8us units (to cover a range of 0 to 524 ms)
 * dword12 - b'0:15  - intr_low_threshold:
 *                     Used only by Consumer ring to generate ring_sw_int_p.
 *                     Ring entries low threshold water mark, that is used
 *                     in combination with the interrupt timer as well as
 *                     the clearing of the level interrupt.
 *           b'16:18 - prefetch_timer_cfg:
 *                     Used only by Consumer ring to set timer mode to
 *                     support Application prefetch handling.
 *                     The external tail offset/pointer will be updated
 *                     at following intervals:
 *                     3'b000: (Prefetch feature disabled; used only for debug)
 *                     3'b001: 1 usec
 *                     3'b010: 4 usec
 *                     3'b011: 8 usec (default)
 *                     3'b100: 16 usec
 *                     Others: Reserved
 *           b'19    - response_required:
 *                     Host needs HTT_T2H_MSG_TYPE_SRING_SETUP_DONE as response
 *           b'20:31 - reserved:  reserved for future use
 */

#define HTT_SRNG_SETUP_CMD_INFO0_MSG_TYPE	GENMASK(7, 0)
#define HTT_SRNG_SETUP_CMD_INFO0_PDEV_ID	GENMASK(15, 8)
#define HTT_SRNG_SETUP_CMD_INFO0_RING_ID	GENMASK(23, 16)
#define HTT_SRNG_SETUP_CMD_INFO0_RING_TYPE	GENMASK(31, 24)

#define HTT_SRNG_SETUP_CMD_INFO1_RING_SIZE			GENMASK(15, 0)
#define HTT_SRNG_SETUP_CMD_INFO1_RING_ENTRY_SIZE		GENMASK(23, 16)
#define HTT_SRNG_SETUP_CMD_INFO1_RING_LOOP_CNT_DIS		BIT(25)
#define HTT_SRNG_SETUP_CMD_INFO1_RING_FLAGS_MSI_SWAP		BIT(27)
#define HTT_SRNG_SETUP_CMD_INFO1_RING_FLAGS_HOST_FW_SWAP	BIT(28)
#define HTT_SRNG_SETUP_CMD_INFO1_RING_FLAGS_TLV_SWAP		BIT(29)

#define HTT_SRNG_SETUP_CMD_INTR_INFO_BATCH_COUNTER_THRESH	GENMASK(14, 0)
#define HTT_SRNG_SETUP_CMD_INTR_INFO_SW_INTR_MODE		BIT(15)
#define HTT_SRNG_SETUP_CMD_INTR_INFO_INTR_TIMER_THRESH		GENMASK(31, 16)

#define HTT_SRNG_SETUP_CMD_INFO2_INTR_LOW_THRESH	GENMASK(15, 0)
#define HTT_SRNG_SETUP_CMD_INFO2_PRE_FETCH_TIMER_CFG	GENMASK(18, 16)
#define HTT_SRNG_SETUP_CMD_INFO2_RESPONSE_REQUIRED	BIT(19)

struct htt_srng_setup_cmd {
	__le32 info0;
	__le32 ring_base_addr_lo;
	__le32 ring_base_addr_hi;
	__le32 info1;
	__le32 ring_head_off32_remote_addr_lo;
	__le32 ring_head_off32_remote_addr_hi;
	__le32 ring_tail_off32_remote_addr_lo;
	__le32 ring_tail_off32_remote_addr_hi;
	__le32 ring_msi_addr_lo;
	__le32 ring_msi_addr_hi;
	__le32 msi_data;
	__le32 intr_info;
	__le32 info2;
} __packed;

/* host -> target FW  PPDU_STATS config message
 *
 * @details
 * The following field definitions describe the format of the HTT host
 * to target FW for PPDU_STATS_CFG msg.
 * The message allows the host to configure the PPDU_STATS_IND messages
 * produced by the target.
 *
 * |31          24|23          16|15           8|7            0|
 * |-----------------------------------------------------------|
 * |    REQ bit mask             |   pdev_mask  |   msg type   |
 * |-----------------------------------------------------------|
 * Header fields:
 *  - MSG_TYPE
 *    Bits 7:0
 *    Purpose: identifies this is a req to configure ppdu_stats_ind from target
 *    Value: 0x11
 *  - PDEV_MASK
 *    Bits 8:15
 *    Purpose: identifies which pdevs this PPDU stats configuration applies to
 *    Value: This is a overloaded field, refer to usage and interpretation of
 *           PDEV in interface document.
 *           Bit   8    :  Reserved for SOC stats
 *           Bit 9 - 15 :  Indicates PDEV_MASK in DBDC
 *                         Indicates MACID_MASK in DBS
 *  - REQ_TLV_BIT_MASK
 *    Bits 16:31
 *    Purpose: each set bit indicates the corresponding PPDU stats TLV type
 *        needs to be included in the target's PPDU_STATS_IND messages.
 *    Value: refer htt_ppdu_stats_tlv_tag_t <<<???
 *
 */

struct htt_ppdu_stats_cfg_cmd {
	__le32 msg;
} __packed;

#define HTT_PPDU_STATS_CFG_MSG_TYPE		GENMASK(7, 0)
#define HTT_PPDU_STATS_CFG_SOC_STATS		BIT(8)
#define HTT_PPDU_STATS_CFG_PDEV_ID		GENMASK(15, 9)
#define HTT_PPDU_STATS_CFG_TLV_TYPE_BITMASK	GENMASK(31, 16)

enum htt_ppdu_stats_tag_type {
	HTT_PPDU_STATS_TAG_COMMON,
	HTT_PPDU_STATS_TAG_USR_COMMON,
	HTT_PPDU_STATS_TAG_USR_RATE,
	HTT_PPDU_STATS_TAG_USR_MPDU_ENQ_BITMAP_64,
	HTT_PPDU_STATS_TAG_USR_MPDU_ENQ_BITMAP_256,
	HTT_PPDU_STATS_TAG_SCH_CMD_STATUS,
	HTT_PPDU_STATS_TAG_USR_COMPLTN_COMMON,
	HTT_PPDU_STATS_TAG_USR_COMPLTN_BA_BITMAP_64,
	HTT_PPDU_STATS_TAG_USR_COMPLTN_BA_BITMAP_256,
	HTT_PPDU_STATS_TAG_USR_COMPLTN_ACK_BA_STATUS,
	HTT_PPDU_STATS_TAG_USR_COMPLTN_FLUSH,
	HTT_PPDU_STATS_TAG_USR_COMMON_ARRAY,
	HTT_PPDU_STATS_TAG_INFO,
	HTT_PPDU_STATS_TAG_TX_MGMTCTRL_PAYLOAD,
	HTT_PPDU_STATS_USERS_INFO,
	HTT_PPDU_STATS_USR_MPDU_ENQ_BITMAP_1024_TLV,
	HTT_PPDU_STATS_USR_COMPLTN_BA_BITMAP_1024_TLV,
	HTT_PPDU_STATS_RX_MGMTCTRL_PAYLOAD_TLV,
	HTT_PPDU_STATS_FOR_SMU_TLV,
	HTT_PPDU_STATS_MLO_TX_RESP_TLV,
	HTT_PPDU_STATS_MLO_TX_NOTIFICATION_TLV,

	/* New TLV's are added above to this line */
	HTT_PPDU_STATS_TAG_MAX,
};

#define HTT_STATS_FRAMECTRL_TYPE_MASK GENMASK(3,2)
#define HTT_STATS_GET_FRAME_CTRL_TYPE(_val)	\
		u32_get_bits(_val, HTT_STATS_FRAMECTRL_TYPE_MASK)

#define HTT_PPDU_STATS_TAG_DEFAULT (BIT(HTT_PPDU_STATS_TAG_COMMON) \
				   | BIT(HTT_PPDU_STATS_TAG_USR_COMMON) \
				   | BIT(HTT_PPDU_STATS_TAG_USR_RATE) \
				   | BIT(HTT_PPDU_STATS_TAG_SCH_CMD_STATUS) \
				   | BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_COMMON) \
				   | BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_ACK_BA_STATUS) \
				   | BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_FLUSH) \
				   | BIT(HTT_PPDU_STATS_TAG_USR_COMMON_ARRAY))

#define HTT_PPDU_STATS_ENHANCED_TX_COMPLN (BIT(HTT_PPDU_STATS_TAG_COMMON) \
                                   | BIT(HTT_PPDU_STATS_TAG_USR_COMMON) \
                                   | BIT(HTT_PPDU_STATS_TAG_USR_RATE) \
                                   | BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_COMMON) \
                                   | BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_FLUSH))

#define HTT_PPDU_STATS_TAG_PKTLOG  (BIT(HTT_PPDU_STATS_TAG_USR_MPDU_ENQ_BITMAP_64) | \
				    BIT(HTT_PPDU_STATS_TAG_USR_MPDU_ENQ_BITMAP_256) | \
				    BIT(HTT_PPDU_STATS_USR_MPDU_ENQ_BITMAP_1024_TLV) | \
				    BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_BA_BITMAP_64) | \
				    BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_BA_BITMAP_256) | \
				    BIT(HTT_PPDU_STATS_USR_COMPLTN_BA_BITMAP_1024_TLV) | \
				    BIT(HTT_PPDU_STATS_TAG_INFO) | \
				    BIT(HTT_PPDU_STATS_TAG_TX_MGMTCTRL_PAYLOAD) | \
				    BIT(HTT_PPDU_STATS_USERS_INFO) | \
				    HTT_PPDU_STATS_TAG_DEFAULT)

enum htt_stats_internal_ppdu_frametype {
	HTT_STATS_PPDU_FTYPE_CTRL,
	HTT_STATS_PPDU_FTYPE_DATA,
	HTT_STATS_PPDU_FTYPE_BAR,
	HTT_STATS_PPDU_FTYPE_MAX
};

enum htt_ppdu_stats_ru_size {
       HTT_PPDU_STATS_RU_26,
       HTT_PPDU_STATS_RU_52,
       HTT_PPDU_STATS_RU_52_26,
       HTT_PPDU_STATS_RU_106,
       HTT_PPDU_STATS_RU_106_26,
       HTT_PPDU_STATS_RU_242,
       HTT_PPDU_STATS_RU_484,
       HTT_PPDU_STATS_RU_484_242,
       HTT_PPDU_STATS_RU_996,
       HTT_PPDU_STATS_RU_996_484,
       HTT_PPDU_STATS_RU_996_484_242,
       HTT_PPDU_STATS_RU_996x2,
       HTT_PPDU_STATS_RU_996x2_484,
       HTT_PPDU_STATS_RU_996x3,
       HTT_PPDU_STATS_RU_996x3_484,
       HTT_PPDU_STATS_RU_996x4,
};

enum htt_stats_frametype {
	HTT_STATS_FTYPE_TIDQ_DATA_SU = 15,
	HTT_STATS_FTYPE_TIDQ_DATA_MU,
};

/* HTT_H2T_MSG_TYPE_RX_RING_SELECTION_CFG Message
 *
 * details:
 *    HTT_H2T_MSG_TYPE_RX_RING_SELECTION_CFG message is sent by host to
 *    configure RXDMA rings.
 *    The configuration is per ring based and includes both packet subtypes
 *    and PPDU/MPDU TLVs.
 *
 *    The message would appear as follows:
 *
 *    |31   29|28|27|26|25|24|23       16|15             8|7             0|
 *    |-------+--+--+--+--+--+-----------+----------------+---------------|
 *    | rsvd1 |ED|DT|OV|PS|SS|  ring_id  |     pdev_id    |    msg_type   |
 *    |-------------------------------------------------------------------|
 *    |              rsvd2               |           ring_buffer_size     |
 *    |-------------------------------------------------------------------|
 *    |                        packet_type_enable_flags_0                 |
 *    |-------------------------------------------------------------------|
 *    |                        packet_type_enable_flags_1                 |
 *    |-------------------------------------------------------------------|
 *    |                        packet_type_enable_flags_2                 |
 *    |-------------------------------------------------------------------|
 *    |                        packet_type_enable_flags_3                 |
 *    |-------------------------------------------------------------------|
 *    |                         tlv_filter_in_flags                       |
 *    |-------------------------------------------------------------------|
 * Where:
 *     PS = pkt_swap
 *     SS = status_swap
 * The message is interpreted as follows:
 * dword0 - b'0:7   - msg_type: This will be set to
 *                    HTT_H2T_MSG_TYPE_RX_RING_SELECTION_CFG
 *          b'8:15  - pdev_id:
 *                    0 (for rings at SOC/UMAC level),
 *                    1/2/3 mac id (for rings at LMAC level)
 *          b'16:23 - ring_id : Identify the ring to configure.
 *                    More details can be got from enum htt_srng_ring_id
 *          b'24    - status_swap: 1 is to swap status TLV
 *          b'25    - pkt_swap:  1 is to swap packet TLV
 *          b'26    - rx_offset_valid (OV): flag to indicate rx offsets
 *		      configuration fields are valid
 *          b'27    - drop_thresh_valid (DT): flag to indicate if the
 *		      rx_drop_threshold field is valid
 *          b'28    - rx_mon_global_en: Enable/Disable global register
 *		      configuration in Rx monitor module.
 *          b'29:31 - rsvd1:  reserved for future use
 * dword1 - b'0:16  - ring_buffer_size: size of buffers referenced by rx ring,
 *                    in byte units.
 *                    Valid only for HW_TO_SW_RING and SW_TO_HW_RING
 *        - b'16:31 - rsvd2: Reserved for future use
 * dword2 - b'0:31  - packet_type_enable_flags_0:
 *                    Enable MGMT packet from 0b0000 to 0b1001
 *                    bits from low to high: FP, MD, MO - 3 bits
 *                        FP: Filter_Pass
 *                        MD: Monitor_Direct
 *                        MO: Monitor_Other
 *                    10 mgmt subtypes * 3 bits -> 30 bits
 *                    Refer to PKT_TYPE_ENABLE_FLAG0_xxx_MGMT_xxx defs
 * dword3 - b'0:31  - packet_type_enable_flags_1:
 *                    Enable MGMT packet from 0b1010 to 0b1111
 *                    bits from low to high: FP, MD, MO - 3 bits
 *                    Refer to PKT_TYPE_ENABLE_FLAG1_xxx_MGMT_xxx defs
 * dword4 - b'0:31 -  packet_type_enable_flags_2:
 *                    Enable CTRL packet from 0b0000 to 0b1001
 *                    bits from low to high: FP, MD, MO - 3 bits
 *                    Refer to PKT_TYPE_ENABLE_FLAG2_xxx_CTRL_xxx defs
 * dword5 - b'0:31  - packet_type_enable_flags_3:
 *                    Enable CTRL packet from 0b1010 to 0b1111,
 *                    MCAST_DATA, UCAST_DATA, NULL_DATA
 *                    bits from low to high: FP, MD, MO - 3 bits
 *                    Refer to PKT_TYPE_ENABLE_FLAG3_xxx_CTRL_xxx defs
 * dword6 - b'0:31 -  tlv_filter_in_flags:
 *                    Filter in Attention/MPDU/PPDU/Header/User tlvs
 *                    Refer to CFG_TLV_FILTER_IN_FLAG defs
 */

#define HTT_RX_RING_SELECTION_CFG_CMD_INFO0_MSG_TYPE	GENMASK(7, 0)
#define HTT_RX_RING_SELECTION_CFG_CMD_INFO0_PDEV_ID	GENMASK(15, 8)
#define HTT_RX_RING_SELECTION_CFG_CMD_INFO0_RING_ID	GENMASK(23, 16)
#define HTT_RX_RING_SELECTION_CFG_CMD_INFO0_SS		BIT(24)
#define HTT_RX_RING_SELECTION_CFG_CMD_INFO0_PS		BIT(25)
#define HTT_RX_RING_SELECTION_CFG_CMD_INFO0_OFFSET_VALID	BIT(26)
#define HTT_RX_RING_SELECTION_CFG_CMD_INFO0_DROP_THRES_VAL	BIT(27)
#define HTT_RX_RING_SELECTION_CFG_CMD_INFO0_EN_RXMON		BIT(28)
#define HTT_RX_RING_SELECTION_CFG_CMD_INFO0_PKT_TYPE_EN_DATA	BIT(29)

#define HTT_RX_RING_SELECTION_CFG_CMD_INFO1_BUF_SIZE		GENMASK(15, 0)
#define HTT_RX_RING_SELECTION_CFG_CMD_INFO1_CONF_LEN_MGMT	GENMASK(18, 16)
#define HTT_RX_RING_SELECTION_CFG_CMD_INFO1_CONF_LEN_CTRL	GENMASK(21, 19)
#define HTT_RX_RING_SELECTION_CFG_CMD_INFO1_CONF_LEN_DATA	GENMASK(24, 22)
#define HTT_RX_RING_SEL_CFG_CMD_INFO1_CONF_HDR_LEN		GENMASK(26, 25)

#define HTT_RX_RING_SELECTION_CFG_CMD_INFO2_DROP_THRESHOLD	GENMASK(9, 0)
#define HTT_RX_RING_SELECTION_CFG_CMD_INFO2_EN_LOG_MGMT_TYPE	BIT(17)
#define HTT_RX_RING_SELECTION_CFG_CMD_INFO2_EN_CTRL_TYPE	BIT(18)
#define HTT_RX_RING_SELECTION_CFG_CMD_INFO2_EN_LOG_DATA_TYPE	BIT(19)

#define HTT_RX_RING_SELECTION_CFG_CMD_INFO3_EN_TLV_PKT_OFFSET	BIT(0)
#define HTT_RX_RING_SELECTION_CFG_CMD_INFO3_PKT_TLV_OFFSET	GENMASK(14, 1)

#define HTT_RX_RING_SELECTION_CFG_RX_PACKET_OFFSET      GENMASK(15, 0)
#define HTT_RX_RING_SELECTION_CFG_RX_HEADER_OFFSET      GENMASK(31, 16)
#define HTT_RX_RING_SELECTION_CFG_RX_MPDU_END_OFFSET    GENMASK(15, 0)
#define HTT_RX_RING_SELECTION_CFG_RX_MPDU_START_OFFSET  GENMASK(31, 16)
#define HTT_RX_RING_SELECTION_CFG_RX_MSDU_END_OFFSET    GENMASK(15, 0)
#define HTT_RX_RING_SELECTION_CFG_RX_MSDU_START_OFFSET  GENMASK(31, 16)
#define HTT_RX_RING_SELECTION_CFG_RX_ATTENTION_OFFSET   GENMASK(15, 0)

#define HTT_RX_RING_SELECTION_CFG_WORD_MASK_COMPACT_SET	BIT(23)
#define HTT_RX_RING_SELECTION_CFG_RX_MPDU_START_MASK	GENMASK(15, 0)
#define HTT_RX_RING_SELECTION_CFG_RX_MPDU_END_MASK	GENMASK(18, 16)
#define HTT_RX_RING_SELECTION_CFG_RX_MSDU_END_MASK	GENMASK(16, 0)

#define HTT_RX_RING_SELECTION_CFG_RX_MON_MPDU_START_MASK		GENMASK(19, 0)
#define HTT_RX_RING_SELECTION_CFG_RX_MON_MPDU_END_MASK			GENMASK(27, 20)
#define HTT_RX_RING_SELECTION_CFG_RX_MON_MSDU_END_MASK			GENMASK(19, 0)
#define HTT_RX_RING_SELECTION_CFG_RX_MON_PPDU_END_USR_STATS_MASK	GENMASK(19, 0)

#define HTT_RX_PKT_ENABLE_SUBTYPE_SET( \
	 word, mode, type, flag, sub_type, val) \
	do { \
		if (val) { \
			(word) |= (HTT_RX_##mode##_##type##_PKT_FILTER_TLV_ \
					##flag##_##sub_type); \
		} \
	} while (0)

enum htt_rx_filter_tlv_flags {
	HTT_RX_FILTER_TLV_FLAGS_MPDU_START		= BIT(0),
	HTT_RX_FILTER_TLV_FLAGS_MSDU_START		= BIT(1),
	HTT_RX_FILTER_TLV_FLAGS_RX_PACKET		= BIT(2),
	HTT_RX_FILTER_TLV_FLAGS_MSDU_END		= BIT(3),
	HTT_RX_FILTER_TLV_FLAGS_MPDU_END		= BIT(4),
	HTT_RX_FILTER_TLV_FLAGS_PACKET_HEADER		= BIT(5),
	HTT_RX_FILTER_TLV_FLAGS_PER_MSDU_HEADER		= BIT(6),
	HTT_RX_FILTER_TLV_FLAGS_ATTENTION		= BIT(7),
	HTT_RX_FILTER_TLV_FLAGS_PPDU_START		= BIT(8),
	HTT_RX_FILTER_TLV_FLAGS_PPDU_END		= BIT(9),
	HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS	= BIT(10),
	HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS_EXT	= BIT(11),
	HTT_RX_FILTER_TLV_FLAGS_PPDU_END_STATUS_DONE	= BIT(12),
	HTT_RX_FILTER_TLV_FLAGS_PPDU_START_USER_INFO	= BIT(13),
};

enum htt_rx_mgmt_pkt_filter_tlv_flags0 {
	HTT_RX_FP_MGMT_PKT_FILTER_TLV_FLAGS0_ASSOC_REQ		= BIT(0),
	HTT_RX_MD_MGMT_PKT_FILTER_TLV_FLAGS0_ASSOC_REQ		= BIT(1),
	HTT_RX_MO_MGMT_PKT_FILTER_TLV_FLAGS0_ASSOC_REQ		= BIT(2),
	HTT_RX_FP_MGMT_PKT_FILTER_TLV_FLAGS0_ASSOC_RESP		= BIT(3),
	HTT_RX_MD_MGMT_PKT_FILTER_TLV_FLAGS0_ASSOC_RESP		= BIT(4),
	HTT_RX_MO_MGMT_PKT_FILTER_TLV_FLAGS0_ASSOC_RESP		= BIT(5),
	HTT_RX_FP_MGMT_PKT_FILTER_TLV_FLAGS0_REASSOC_REQ	= BIT(6),
	HTT_RX_MD_MGMT_PKT_FILTER_TLV_FLAGS0_REASSOC_REQ	= BIT(7),
	HTT_RX_MO_MGMT_PKT_FILTER_TLV_FLAGS0_REASSOC_REQ	= BIT(8),
	HTT_RX_FP_MGMT_PKT_FILTER_TLV_FLAGS0_REASSOC_RESP	= BIT(9),
	HTT_RX_MD_MGMT_PKT_FILTER_TLV_FLAGS0_REASSOC_RESP	= BIT(10),
	HTT_RX_MO_MGMT_PKT_FILTER_TLV_FLAGS0_REASSOC_RESP	= BIT(11),
	HTT_RX_FP_MGMT_PKT_FILTER_TLV_FLAGS0_PROBE_REQ		= BIT(12),
	HTT_RX_MD_MGMT_PKT_FILTER_TLV_FLAGS0_PROBE_REQ		= BIT(13),
	HTT_RX_MO_MGMT_PKT_FILTER_TLV_FLAGS0_PROBE_REQ		= BIT(14),
	HTT_RX_FP_MGMT_PKT_FILTER_TLV_FLAGS0_PROBE_RESP		= BIT(15),
	HTT_RX_MD_MGMT_PKT_FILTER_TLV_FLAGS0_PROBE_RESP		= BIT(16),
	HTT_RX_MO_MGMT_PKT_FILTER_TLV_FLAGS0_PROBE_RESP		= BIT(17),
	HTT_RX_FP_MGMT_PKT_FILTER_TLV_FLAGS0_PROBE_TIMING_ADV	= BIT(18),
	HTT_RX_MD_MGMT_PKT_FILTER_TLV_FLAGS0_PROBE_TIMING_ADV	= BIT(19),
	HTT_RX_MO_MGMT_PKT_FILTER_TLV_FLAGS0_PROBE_TIMING_ADV	= BIT(20),
	HTT_RX_FP_MGMT_PKT_FILTER_TLV_FLAGS0_RESERVED_7		= BIT(21),
	HTT_RX_MD_MGMT_PKT_FILTER_TLV_FLAGS0_RESERVED_7		= BIT(22),
	HTT_RX_MO_MGMT_PKT_FILTER_TLV_FLAGS0_RESERVED_7		= BIT(23),
	HTT_RX_FP_MGMT_PKT_FILTER_TLV_FLAGS0_BEACON		= BIT(24),
	HTT_RX_MD_MGMT_PKT_FILTER_TLV_FLAGS0_BEACON		= BIT(25),
	HTT_RX_MO_MGMT_PKT_FILTER_TLV_FLAGS0_BEACON		= BIT(26),
	HTT_RX_FP_MGMT_PKT_FILTER_TLV_FLAGS0_ATIM		= BIT(27),
	HTT_RX_MD_MGMT_PKT_FILTER_TLV_FLAGS0_ATIM		= BIT(28),
	HTT_RX_MO_MGMT_PKT_FILTER_TLV_FLAGS0_ATIM		= BIT(29),
};

enum htt_rx_mgmt_pkt_filter_tlv_flags1 {
	HTT_RX_FP_MGMT_PKT_FILTER_TLV_FLAGS1_DISASSOC		= BIT(0),
	HTT_RX_MD_MGMT_PKT_FILTER_TLV_FLAGS1_DISASSOC		= BIT(1),
	HTT_RX_MO_MGMT_PKT_FILTER_TLV_FLAGS1_DISASSOC		= BIT(2),
	HTT_RX_FP_MGMT_PKT_FILTER_TLV_FLAGS1_AUTH		= BIT(3),
	HTT_RX_MD_MGMT_PKT_FILTER_TLV_FLAGS1_AUTH		= BIT(4),
	HTT_RX_MO_MGMT_PKT_FILTER_TLV_FLAGS1_AUTH		= BIT(5),
	HTT_RX_FP_MGMT_PKT_FILTER_TLV_FLAGS1_DEAUTH		= BIT(6),
	HTT_RX_MD_MGMT_PKT_FILTER_TLV_FLAGS1_DEAUTH		= BIT(7),
	HTT_RX_MO_MGMT_PKT_FILTER_TLV_FLAGS1_DEAUTH		= BIT(8),
	HTT_RX_FP_MGMT_PKT_FILTER_TLV_FLAGS1_ACTION		= BIT(9),
	HTT_RX_MD_MGMT_PKT_FILTER_TLV_FLAGS1_ACTION		= BIT(10),
	HTT_RX_MO_MGMT_PKT_FILTER_TLV_FLAGS1_ACTION		= BIT(11),
	HTT_RX_FP_MGMT_PKT_FILTER_TLV_FLAGS1_ACTION_NOACK	= BIT(12),
	HTT_RX_MD_MGMT_PKT_FILTER_TLV_FLAGS1_ACTION_NOACK	= BIT(13),
	HTT_RX_MO_MGMT_PKT_FILTER_TLV_FLAGS1_ACTION_NOACK	= BIT(14),
	HTT_RX_FP_MGMT_PKT_FILTER_TLV_FLAGS1_RESERVED_15	= BIT(15),
	HTT_RX_MD_MGMT_PKT_FILTER_TLV_FLAGS1_RESERVED_15	= BIT(16),
	HTT_RX_MO_MGMT_PKT_FILTER_TLV_FLAGS1_RESERVED_15	= BIT(17),
};

enum htt_rx_ctrl_pkt_filter_tlv_flags2 {
	HTT_RX_FP_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_RESERVED_1	= BIT(0),
	HTT_RX_MD_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_RESERVED_1	= BIT(1),
	HTT_RX_MO_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_RESERVED_1	= BIT(2),
	HTT_RX_FP_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_RESERVED_2	= BIT(3),
	HTT_RX_MD_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_RESERVED_2	= BIT(4),
	HTT_RX_MO_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_RESERVED_2	= BIT(5),
	HTT_RX_FP_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_TRIGGER	= BIT(6),
	HTT_RX_MD_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_TRIGGER	= BIT(7),
	HTT_RX_MO_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_TRIGGER	= BIT(8),
	HTT_RX_FP_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_RESERVED_4	= BIT(9),
	HTT_RX_MD_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_RESERVED_4	= BIT(10),
	HTT_RX_MO_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_RESERVED_4	= BIT(11),
	HTT_RX_FP_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_BF_REP_POLL	= BIT(12),
	HTT_RX_MD_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_BF_REP_POLL	= BIT(13),
	HTT_RX_MO_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_BF_REP_POLL	= BIT(14),
	HTT_RX_FP_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_VHT_NDP	= BIT(15),
	HTT_RX_MD_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_VHT_NDP	= BIT(16),
	HTT_RX_MO_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_VHT_NDP	= BIT(17),
	HTT_RX_FP_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_FRAME_EXT	= BIT(18),
	HTT_RX_MD_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_FRAME_EXT	= BIT(19),
	HTT_RX_MO_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_FRAME_EXT	= BIT(20),
	HTT_RX_FP_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_WRAPPER	= BIT(21),
	HTT_RX_MD_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_WRAPPER	= BIT(22),
	HTT_RX_MO_CTRL_PKT_FILTER_TLV_FLAGS2_CTRL_WRAPPER	= BIT(23),
	HTT_RX_FP_CTRL_PKT_FILTER_TLV_FLAGS2_BAR		= BIT(24),
	HTT_RX_MD_CTRL_PKT_FILTER_TLV_FLAGS2_BAR		= BIT(25),
	HTT_RX_MO_CTRL_PKT_FILTER_TLV_FLAGS2_BAR		= BIT(26),
	HTT_RX_FP_CTRL_PKT_FILTER_TLV_FLAGS2_BA			= BIT(27),
	HTT_RX_MD_CTRL_PKT_FILTER_TLV_FLAGS2_BA			= BIT(28),
	HTT_RX_MO_CTRL_PKT_FILTER_TLV_FLAGS2_BA			= BIT(29),
};

enum htt_rx_ctrl_pkt_filter_tlv_flags3 {
	HTT_RX_FP_CTRL_PKT_FILTER_TLV_FLAGS3_PSPOLL		= BIT(0),
	HTT_RX_MD_CTRL_PKT_FILTER_TLV_FLAGS3_PSPOLL		= BIT(1),
	HTT_RX_MO_CTRL_PKT_FILTER_TLV_FLAGS3_PSPOLL		= BIT(2),
	HTT_RX_FP_CTRL_PKT_FILTER_TLV_FLAGS3_RTS		= BIT(3),
	HTT_RX_MD_CTRL_PKT_FILTER_TLV_FLAGS3_RTS		= BIT(4),
	HTT_RX_MO_CTRL_PKT_FILTER_TLV_FLAGS3_RTS		= BIT(5),
	HTT_RX_FP_CTRL_PKT_FILTER_TLV_FLAGS3_CTS		= BIT(6),
	HTT_RX_MD_CTRL_PKT_FILTER_TLV_FLAGS3_CTS		= BIT(7),
	HTT_RX_MO_CTRL_PKT_FILTER_TLV_FLAGS3_CTS		= BIT(8),
	HTT_RX_FP_CTRL_PKT_FILTER_TLV_FLAGS3_ACK		= BIT(9),
	HTT_RX_MD_CTRL_PKT_FILTER_TLV_FLAGS3_ACK		= BIT(10),
	HTT_RX_MO_CTRL_PKT_FILTER_TLV_FLAGS3_ACK		= BIT(11),
	HTT_RX_FP_CTRL_PKT_FILTER_TLV_FLAGS3_CFEND		= BIT(12),
	HTT_RX_MD_CTRL_PKT_FILTER_TLV_FLAGS3_CFEND		= BIT(13),
	HTT_RX_MO_CTRL_PKT_FILTER_TLV_FLAGS3_CFEND		= BIT(14),
	HTT_RX_FP_CTRL_PKT_FILTER_TLV_FLAGS3_CFEND_ACK		= BIT(15),
	HTT_RX_MD_CTRL_PKT_FILTER_TLV_FLAGS3_CFEND_ACK		= BIT(16),
	HTT_RX_MO_CTRL_PKT_FILTER_TLV_FLAGS3_CFEND_ACK		= BIT(17),
};

enum htt_rx_data_pkt_filter_tlv_flasg3 {
	HTT_RX_FP_DATA_PKT_FILTER_TLV_FLASG3_MCAST	= BIT(18),
	HTT_RX_MD_DATA_PKT_FILTER_TLV_FLASG3_MCAST	= BIT(19),
	HTT_RX_MO_DATA_PKT_FILTER_TLV_FLASG3_MCAST	= BIT(20),
	HTT_RX_FP_DATA_PKT_FILTER_TLV_FLASG3_UCAST	= BIT(21),
	HTT_RX_MD_DATA_PKT_FILTER_TLV_FLASG3_UCAST	= BIT(22),
	HTT_RX_MO_DATA_PKT_FILTER_TLV_FLASG3_UCAST	= BIT(23),
	HTT_RX_FP_DATA_PKT_FILTER_TLV_FLASG3_NULL_DATA	= BIT(24),
	HTT_RX_MD_DATA_PKT_FILTER_TLV_FLASG3_NULL_DATA	= BIT(25),
	HTT_RX_MO_DATA_PKT_FILTER_TLV_FLASG3_NULL_DATA	= BIT(26),
};

enum htt_rx_hdr_len_type {
	HTT_RX_HDR_LEN_64_BYTES = 1,
	HTT_RX_HDR_LEN_128_BYTES,
	HTT_RX_HDR_LEN_256_BYTES,
};

#define HTT_RX_MON_FILTER_TLV_FLAGS \
		(HTT_RX_FILTER_TLV_FLAGS_MPDU_START | \
		HTT_RX_FILTER_TLV_FLAGS_PPDU_START | \
		HTT_RX_FILTER_TLV_FLAGS_PPDU_END | \
		HTT_RX_FILTER_TLV_FLAGS_PPDU_END_STATUS_DONE)

#define HTT_RX_MON_FILTER_TLV_EXTD_FLAGS \
		(HTT_RX_MON_FILTER_TLV_FLAGS | \
		HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS | \
		HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS_EXT | \
		HTT_RX_FILTER_TLV_FLAGS_PPDU_START_USER_INFO)

#define HTT_RX_MON_FILTER_TLV_FLAGS_MON_STATUS_RING \
		(HTT_RX_FILTER_TLV_FLAGS_MPDU_START | \
		HTT_RX_FILTER_TLV_FLAGS_PPDU_START | \
		HTT_RX_FILTER_TLV_FLAGS_PPDU_END | \
		HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS | \
		HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS_EXT | \
		HTT_RX_FILTER_TLV_FLAGS_PPDU_END_STATUS_DONE)

#define HTT_RX_MON_FILTER_TLV_FLAGS_MON_BUF_RING \
		(HTT_RX_FILTER_TLV_FLAGS_MPDU_START | \
		HTT_RX_FILTER_TLV_FLAGS_MSDU_START | \
		HTT_RX_FILTER_TLV_FLAGS_RX_PACKET | \
		HTT_RX_FILTER_TLV_FLAGS_MSDU_END | \
		HTT_RX_FILTER_TLV_FLAGS_MPDU_END | \
		HTT_RX_FILTER_TLV_FLAGS_PACKET_HEADER | \
		HTT_RX_FILTER_TLV_FLAGS_PER_MSDU_HEADER | \
		HTT_RX_FILTER_TLV_FLAGS_ATTENTION)

#define HTT_RX_MON_FILTER_TLV_FLAGS_MON_DEST_RING \
	(HTT_RX_FILTER_TLV_FLAGS_MPDU_START | \
	HTT_RX_FILTER_TLV_FLAGS_MSDU_START | \
	HTT_RX_FILTER_TLV_FLAGS_RX_PACKET | \
	HTT_RX_FILTER_TLV_FLAGS_MSDU_END | \
	HTT_RX_FILTER_TLV_FLAGS_MPDU_END | \
	HTT_RX_FILTER_TLV_FLAGS_PACKET_HEADER | \
	HTT_RX_FILTER_TLV_FLAGS_PER_MSDU_HEADER | \
	HTT_RX_FILTER_TLV_FLAGS_PPDU_START | \
	HTT_RX_FILTER_TLV_FLAGS_PPDU_END | \
	HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS | \
	HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS_EXT | \
	HTT_RX_FILTER_TLV_FLAGS_PPDU_END_STATUS_DONE | \
	HTT_RX_FILTER_TLV_FLAGS_PPDU_START_USER_INFO)

/* msdu start. mpdu end, attention, rx hdr tlv's are not subscribed */
#define HTT_RX_TLV_FLAGS_RXDMA_RING \
		(HTT_RX_FILTER_TLV_FLAGS_MPDU_START | \
		HTT_RX_FILTER_TLV_FLAGS_RX_PACKET | \
		HTT_RX_FILTER_TLV_FLAGS_MSDU_END)

#define HTT_TX_RING_SELECTION_CFG_CMD_INFO0_MSG_TYPE	GENMASK(7, 0)
#define HTT_TX_RING_SELECTION_CFG_CMD_INFO0_PDEV_ID	GENMASK(15, 8)

#define HTT_RX_FILTER_TLV_PKTLOG_LITE \
        (HTT_RX_FILTER_TLV_FLAGS_MPDU_START | \
         HTT_RX_FILTER_TLV_FLAGS_PPDU_START | \
         HTT_RX_FILTER_TLV_FLAGS_PPDU_END | \
         HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS | \
         HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS_EXT | \
         HTT_RX_FILTER_TLV_FLAGS_PPDU_END_STATUS_DONE | \
         HTT_RX_FILTER_TLV_FLAGS_PPDU_START_USER_INFO)

#define HTT_RX_FILTER_TLV_PKTLOG_FULL \
        (HTT_RX_FILTER_TLV_PKTLOG_LITE | \
         HTT_RX_FILTER_TLV_FLAGS_MSDU_START | \
         HTT_RX_FILTER_TLV_FLAGS_MSDU_END | \
         HTT_RX_FILTER_TLV_FLAGS_MPDU_END | \
         HTT_RX_FILTER_TLV_FLAGS_PACKET_HEADER)

struct htt_rx_ring_selection_cfg_cmd {
	__le32 info0;
	__le32 info1;
	__le32 pkt_type_en_flags0;
	__le32 pkt_type_en_flags1;
	__le32 pkt_type_en_flags2;
	__le32 pkt_type_en_flags3;
	__le32 rx_filter_tlv;
	__le32 rx_packet_offset;
	__le32 rx_mpdu_offset;
	__le32 rx_msdu_offset;
	__le32 rx_attn_offset;
	__le32 info2;
	__le32 reserved[2];
	__le32 rx_mpdu_start_end_mask;
	__le32 rx_msdu_end_word_mask;
	__le32 info3;
	__le32 rx_mon_mpdu_start_end_mask;
	__le32 rx_mon_msdu_end_word_mask;
	__le32 rx_mon_ppdu_end_usr_stats_wmask;
	__le32 reserved1[2];
	__le32 pkt_type_en_data_flag0;
	__le32 pkt_type_en_data_flag1;
	__le32 pkt_type_en_data_flag2;
	__le32 pkt_type_en_data_flag3;
} __packed;

#define HTT_RX_RING_TLV_DROP_THRESHOLD_VALUE	32
#define HTT_RX_RING_DEFAULT_DMA_LENGTH		0x7
#define HTT_RX_RING_PKT_TLV_OFFSET		0x1

struct htt_rx_ring_tlv_filter {
	u32 rx_filter; /* see htt_rx_filter_tlv_flags */
	bool offset_valid;
	bool enable_fp;
	bool enable_mo;
	bool enable_md;
	bool enable_fp_packet;
	bool enable_mo_packet;
	bool enable_md_packet;
	u16 fp_mgmt_filter;
	u16 fp_ctrl_filter;
	u16 fp_data_filter;
	u16 mo_mgmt_filter;
	u16 mo_ctrl_filter;
	u16 mo_data_filter;
	u16 md_mgmt_filter;
	u16 md_ctrl_filter;
	u16 md_data_filter;
	u16 fp_packet_mgmt_filter;
	u16 fp_packet_ctrl_filter;
	u16 fp_packet_data_filter;
	u16 mo_packet_mgmt_filter;
	u16 mo_packet_ctrl_filter;
	u16 mo_packet_data_filter;
	u16 md_packet_mgmt_filter;
	u16 md_packet_ctrl_filter;
	u16 md_packet_data_filter;
	u16 rx_packet_offset;
	u16 rx_header_offset;
	u16 rx_mpdu_end_offset;
	u16 rx_mpdu_start_offset;
	u16 rx_msdu_end_offset;
	u16 rx_msdu_start_offset;
	u16 rx_attn_offset;
	u16 rx_mpdu_start_wmask;
	u16 rx_mpdu_end_wmask;
	u32 rx_msdu_end_wmask;
	u32 rx_mon_mpdu_start_wmask;
	u16 rx_mon_mpdu_end_wmask;
	u32 rx_mon_msdu_end_wmask;
	u32 rx_mon_ppdu_end_usr_stats_wmask;
	u32 conf_len_ctrl;
	u32 conf_len_mgmt;
	u32 conf_len_data;
	u16 rx_drop_threshold;
	bool enable_log_mgmt_type;
	bool enable_log_ctrl_type;
	bool enable_log_data_type;
	bool enable_rx_tlv_offset;
	u16 rx_tlv_offset;
	bool drop_threshold_valid;
	bool rxmon_disable;
	u8 rx_hdr_len;
};

#define HTT_STATS_FRAME_CTRL_TYPE_MGMT  0x0
#define HTT_STATS_FRAME_CTRL_TYPE_CTRL  0x1
#define HTT_STATS_FRAME_CTRL_TYPE_DATA  0x2
#define HTT_STATS_FRAME_CTRL_TYPE_RESV  0x3

#define HTT_TX_RING_SELECTION_CFG_CMD_INFO0_MSG_TYPE	GENMASK(7, 0)
#define HTT_TX_RING_SELECTION_CFG_CMD_INFO0_PDEV_ID	GENMASK(15, 8)
#define HTT_TX_RING_SELECTION_CFG_CMD_INFO0_RING_ID	GENMASK(23, 16)
#define HTT_TX_RING_SELECTION_CFG_CMD_INFO0_SS		BIT(24)
#define HTT_TX_RING_SELECTION_CFG_CMD_INFO0_PS		BIT(25)

#define HTT_TX_RING_SELECTION_CFG_CMD_INFO1_RING_BUFF_SIZE	GENMASK(15, 0)
#define HTT_TX_RING_SELECTION_CFG_CMD_INFO1_PKT_TYPE		GENMASK(18, 16)
#define HTT_TX_RING_SELECTION_CFG_CMD_INFO1_CONF_LEN_MGMT	GENMASK(21, 19)
#define HTT_TX_RING_SELECTION_CFG_CMD_INFO1_CONF_LEN_CTRL	GENMASK(24, 22)
#define HTT_TX_RING_SELECTION_CFG_CMD_INFO1_CONF_LEN_DATA	GENMASK(27, 25)

#define HTT_TX_RING_SELECTION_CFG_CMD_INFO2_PKT_TYPE_EN_FLAG	GENMASK(2, 0)

struct htt_tx_ring_selection_cfg_cmd {
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le32 tlv_filter_mask_in0;
	__le32 tlv_filter_mask_in1;
	__le32 tlv_filter_mask_in2;
	__le32 tlv_filter_mask_in3;
	__le32 reserved[3];
} __packed;

#define HTT_TX_RING_TLV_FILTER_MGMT_DMA_LEN	GENMASK(3, 0)
#define HTT_TX_RING_TLV_FILTER_CTRL_DMA_LEN	GENMASK(7, 4)
#define HTT_TX_RING_TLV_FILTER_DATA_DMA_LEN	GENMASK(11, 8)

#define HTT_TX_MON_FILTER_HYBRID_MODE \
		(HTT_TX_FILTER_TLV_FLAGS0_RESPONSE_START_STATUS | \
		HTT_TX_FILTER_TLV_FLAGS0_RESPONSE_END_STATUS | \
		HTT_TX_FILTER_TLV_FLAGS0_TX_FES_STATUS_START | \
		HTT_TX_FILTER_TLV_FLAGS0_TX_FES_STATUS_END | \
		HTT_TX_FILTER_TLV_FLAGS0_TX_FES_STATUS_START_PPDU | \
		HTT_TX_FILTER_TLV_FLAGS0_TX_FES_STATUS_USER_PPDU | \
		HTT_TX_FILTER_TLV_FLAGS0_TX_FES_STATUS_ACK_OR_BA | \
		HTT_TX_FILTER_TLV_FLAGS0_TX_FES_STATUS_1K_BA | \
		HTT_TX_FILTER_TLV_FLAGS0_TX_FES_STATUS_START_PROT | \
		HTT_TX_FILTER_TLV_FLAGS0_TX_FES_STATUS_PROT | \
		HTT_TX_FILTER_TLV_FLAGS0_TX_FES_STATUS_USER_RESPONSE | \
		HTT_TX_FILTER_TLV_FLAGS0_RECEIVED_RESPONSE_INFO | \
		HTT_TX_FILTER_TLV_FLAGS0_RECEIVED_RESPONSE_INFO_PART2)

struct htt_tx_ring_tlv_filter {
	u32 tx_mon_downstream_tlv_flags;
	u32 tx_mon_upstream_tlv_flags0;
	u32 tx_mon_upstream_tlv_flags1;
	u32 tx_mon_upstream_tlv_flags2;
	bool tx_mon_mgmt_filter;
	bool tx_mon_data_filter;
	bool tx_mon_ctrl_filter;
	u16 tx_mon_pkt_dma_len;
} __packed;

enum htt_tx_mon_upstream_tlv_flags0 {
	HTT_TX_FILTER_TLV_FLAGS0_RESPONSE_START_STATUS		= BIT(1),
	HTT_TX_FILTER_TLV_FLAGS0_RESPONSE_END_STATUS		= BIT(2),
	HTT_TX_FILTER_TLV_FLAGS0_TX_FES_STATUS_START		= BIT(3),
	HTT_TX_FILTER_TLV_FLAGS0_TX_FES_STATUS_END		= BIT(4),
	HTT_TX_FILTER_TLV_FLAGS0_TX_FES_STATUS_START_PPDU	= BIT(5),
	HTT_TX_FILTER_TLV_FLAGS0_TX_FES_STATUS_USER_PPDU	= BIT(6),
	HTT_TX_FILTER_TLV_FLAGS0_TX_FES_STATUS_ACK_OR_BA	= BIT(7),
	HTT_TX_FILTER_TLV_FLAGS0_TX_FES_STATUS_1K_BA		= BIT(8),
	HTT_TX_FILTER_TLV_FLAGS0_TX_FES_STATUS_START_PROT	= BIT(9),
	HTT_TX_FILTER_TLV_FLAGS0_TX_FES_STATUS_PROT		= BIT(10),
	HTT_TX_FILTER_TLV_FLAGS0_TX_FES_STATUS_USER_RESPONSE	= BIT(11),
	HTT_TX_FILTER_TLV_FLAGS0_RX_FRAME_BITMAP_ACK		= BIT(12),
	HTT_TX_FILTER_TLV_FLAGS0_RX_FRAME_1K_BITMAP_ACK		= BIT(13),
	HTT_TX_FILTER_TLV_FLAGS0_COEX_TX_STATUS			= BIT(14),
	HTT_TX_FILTER_TLV_FLAGS0_RECEIVED_RESPONSE_INFO		= BIT(15),
	HTT_TX_FILTER_TLV_FLAGS0_RECEIVED_RESPONSE_INFO_PART2	= BIT(16),
};

#define HTT_TX_FILTER_TLV_FLAGS2_TXPCU_PHYTX_OTHER_TRANSMIT_INFO32	BIT(11)

/* HTT message target->host */

enum htt_t2h_msg_type {
	HTT_T2H_MSG_TYPE_VERSION_CONF,
	HTT_T2H_MSG_TYPE_PEER_MAP	= 0x3,
	HTT_T2H_MSG_TYPE_PEER_UNMAP	= 0x4,
	HTT_T2H_MSG_TYPE_RX_ADDBA	= 0x5,
	HTT_T2H_MSG_TYPE_PKTLOG		= 0x8,
	HTT_T2H_MSG_TYPE_SEC_IND	= 0xb,
	HTT_T2H_MSG_TYPE_PEER_MAP2	= 0x1e,
	HTT_T2H_MSG_TYPE_PEER_UNMAP2	= 0x1f,
	HTT_T2H_MSG_TYPE_PPDU_STATS_IND = 0x1d,
	HTT_T2H_MSG_TYPE_EXT_STATS_CONF = 0x1c,
	HTT_T2H_MSG_TYPE_BKPRESSURE_EVENT_IND = 0x24,
	HTT_T2H_MSG_TYPE_MLO_TIMESTAMP_OFFSET_IND = 0x28,
	HTT_T2H_MSG_TYPE_MLO_RX_PEER_MAP = 0x29,
	HTT_T2H_MSG_TYPE_MLO_RX_PEER_UNMAP = 0x2a,
	HTT_T2H_MSG_TYPE_PEER_MAP3	= 0x2b,
	HTT_T2H_MSG_TYPE_VDEV_TXRX_STATS_PERIODIC_IND = 0x2c,
	HTT_T2H_MSG_TYPE_QOS_MSDUQ_INFO_IND = 0x2e,
	HTT_T2H_MSG_TYPE_STREAMING_STATS_IND = 0x2f,
	HTT_T2H_MSG_TYPE_PPDU_ID_FMT_IND = 0x30,
	HTT_T2H_MSG_TYPE_PRIMARY_LINK_PEER_MIGRATE_IND = 0x37,
};

#define HTT_TARGET_VERSION_MAJOR 3

#define HTT_T2H_MSG_TYPE		GENMASK(7, 0)
#define HTT_T2H_VERSION_CONF_MINOR	GENMASK(15, 8)
#define HTT_T2H_VERSION_CONF_MAJOR	GENMASK(23, 16)

struct htt_t2h_version_conf_msg {
	__le32 version;
} __packed;

#define HTT_T2H_PEER_MAP_INFO_VDEV_ID	GENMASK(15, 8)
#define HTT_T2H_PEER_MAP_INFO_PEER_ID	GENMASK(31, 16)
#define HTT_T2H_PEER_MAP_INFO1_MAC_ADDR_H16	GENMASK(15, 0)
#define HTT_T2H_PEER_MAP_INFO1_HW_PEER_ID	GENMASK(31, 16)
#define HTT_T2H_PEER_MAP_INFO2_AST_HASH_VAL	GENMASK(15, 0)
#define HTT_T2H_PEER_MAP3_INFO2_HW_PEER_ID     GENMASK(15, 0)
#define HTT_T2H_PEER_MAP3_INFO2_AST_HASH_VAL   GENMASK(31, 16)
#define HTT_T2H_PEER_MAP_INFO2_NEXT_HOP_M	BIT(16)
#define HTT_T2H_PEER_MAP_INFO2_NEXT_HOP_S	16

struct htt_t2h_peer_map_event {
	__le32 info;
	__le32 mac_addr_l32;
	__le32 info1;
	__le32 info2;
} __packed;

#define HTT_T2H_PEER_UNMAP_INFO_VDEV_ID	HTT_T2H_PEER_MAP_INFO_VDEV_ID
#define HTT_T2H_PEER_UNMAP_INFO_PEER_ID	HTT_T2H_PEER_MAP_INFO_PEER_ID
#define HTT_T2H_PEER_UNMAP_INFO1_MAC_ADDR_H16 \
					HTT_T2H_PEER_MAP_INFO1_MAC_ADDR_H16
#define HTT_T2H_PEER_MAP_INFO1_NEXT_HOP_M HTT_T2H_PEER_MAP_INFO2_NEXT_HOP_M
#define HTT_T2H_PEER_MAP_INFO1_NEXT_HOP_S HTT_T2H_PEER_MAP_INFO2_NEXT_HOP_S

struct htt_t2h_peer_unmap_event {
	__le32 info;
	__le32 mac_addr_l32;
	__le32 info1;
} __packed;

struct htt_resp_msg {
	union {
		struct htt_t2h_version_conf_msg version_msg;
		struct htt_t2h_peer_map_event peer_map_ev;
		struct htt_t2h_peer_unmap_event peer_unmap_ev;
	};
} __packed;

struct htt_h2t_msg_type_vdev_txrx_stats_req {
        u32 hdr;
        u32 vdev_id_lo_bitmask;
        u32 vdev_id_hi_bitmask;
};

#define HTT_VDEV_GET_STATS_U64(msg_l32, msg_u32)\
	(((u64)__le32_to_cpu(msg_u32) << 32) | (__le32_to_cpu(msg_l32)))
#define HTT_T2H_VDEV_STATS_PERIODIC_MSG_TYPE		GENMASK(7, 0)
#define HTT_T2H_VDEV_STATS_PERIODIC_PDEV_ID		GENMASK(15, 8)
#define HTT_T2H_VDEV_STATS_PERIODIC_PAYLOAD_BYTES	GENMASK(23, 16)
#define HTT_VDEV_TXRX_STATS_COMMON_TLV		0
#define HTT_VDEV_TXRX_STATS_HW_STATS_TLV	1

#define HTT_H2T_VDEV_TXRX_HDR_MSG_TYPE	      	GENMASK(7, 0)
#define HTT_H2T_VDEV_TXRX_HDR_PDEV_ID	      	GENMASK(15, 8)
#define HTT_H2T_VDEV_TXRX_HDR_ENABLE		BIT(16)
#define HTT_H2T_VDEV_TXRX_HDR_INTERVAL		GENMASK(24, 17)
#define HTT_H2T_VDEV_TXRX_HDR_RESET_STATS	GENMASK(26, 25)
#define HTT_H2T_VDEV_TXRX_LO_BITMASK		GENMASK(31, 0)
#define HTT_H2T_VDEV_TXRX_HI_BITMASK		GENMASK_ULL(63, 32)
#define ATH12K_STATS_TIMER_DUR_1SEC		1000

struct htt_t2h_vdev_txrx_stats_ind {
	__le32 vdev_id;
	__le32 rx_msdu_byte_cnt_lo;
	__le32 rx_msdu_byte_cnt_hi;
	__le32 rx_msdu_cnt_lo;
	__le32 rx_msdu_cnt_hi;
	__le32 tx_msdu_byte_cnt_lo;
	__le32 tx_msdu_byte_cnt_hi;
	__le32 tx_msdu_cnt_lo;
	__le32 tx_msdu_cnt_hi;
	__le32 tx_retry_cnt_lo;
	__le32 tx_retry_cnt_hi;
	__le32 tx_retry_byte_cnt_lo;
	__le32 tx_retry_byte_cnt_hi;
	__le32 tx_drop_cnt_lo;
	__le32 tx_drop_cnt_hi;
	__le32 tx_drop_byte_cnt_lo;
	__le32 tx_drop_byte_cnt_hi;
	__le32 msdu_ttl_cnt_lo;
	__le32 msdu_ttl_cnt_hi;
	__le32 msdu_ttl_byte_cnt_lo;
	__le32 msdu_ttl_byte_cnt_hi;
} __packed;

struct htt_t2h_vdev_common_stats_tlv {
	__le32 soc_drop_count_lo;
	__le32 soc_drop_count_hi;
} __packed;

#define HTT_BACKPRESSURE_EVENT_PDEV_ID_M GENMASK(15, 8)
#define HTT_BACKPRESSURE_EVENT_RING_TYPE_M GENMASK(23, 16)
#define HTT_BACKPRESSURE_EVENT_RING_ID_M GENMASK(31, 24)

#define HTT_BACKPRESSURE_EVENT_HP_M GENMASK(15, 0)
#define HTT_BACKPRESSURE_EVENT_TP_M GENMASK(31, 16)

#define HTT_BACKPRESSURE_UMAC_RING_TYPE	0
#define HTT_BACKPRESSURE_LMAC_RING_TYPE	1

enum htt_backpressure_umac_ringid {
	HTT_SW_RING_IDX_REO_REO2SW1_RING,
	HTT_SW_RING_IDX_REO_REO2SW2_RING,
	HTT_SW_RING_IDX_REO_REO2SW3_RING,
	HTT_SW_RING_IDX_REO_REO2SW4_RING,
	HTT_SW_RING_IDX_REO_WBM2REO_LINK_RING,
	HTT_SW_RING_IDX_REO_REO2TCL_RING,
	HTT_SW_RING_IDX_REO_REO2FW_RING,
	HTT_SW_RING_IDX_REO_REO_RELEASE_RING,
	HTT_SW_RING_IDX_WBM_PPE_RELEASE_RING,
	HTT_SW_RING_IDX_TCL_TCL2TQM_RING,
	HTT_SW_RING_IDX_WBM_TQM_RELEASE_RING,
	HTT_SW_RING_IDX_WBM_REO_RELEASE_RING,
	HTT_SW_RING_IDX_WBM_WBM2SW0_RELEASE_RING,
	HTT_SW_RING_IDX_WBM_WBM2SW1_RELEASE_RING,
	HTT_SW_RING_IDX_WBM_WBM2SW2_RELEASE_RING,
	HTT_SW_RING_IDX_WBM_WBM2SW3_RELEASE_RING,
	HTT_SW_RING_IDX_REO_REO_CMD_RING,
	HTT_SW_RING_IDX_REO_REO_STATUS_RING,
	HTT_SW_UMAC_RING_IDX_MAX,
};

enum htt_backpressure_lmac_ringid {
	HTT_SW_RING_IDX_FW2RXDMA_BUF_RING,
	HTT_SW_RING_IDX_FW2RXDMA_STATUS_RING,
	HTT_SW_RING_IDX_FW2RXDMA_LINK_RING,
	HTT_SW_RING_IDX_SW2RXDMA_BUF_RING,
	HTT_SW_RING_IDX_WBM2RXDMA_LINK_RING,
	HTT_SW_RING_IDX_RXDMA2FW_RING,
	HTT_SW_RING_IDX_RXDMA2SW_RING,
	HTT_SW_RING_IDX_RXDMA2RELEASE_RING,
	HTT_SW_RING_IDX_RXDMA2REO_RING,
	HTT_SW_RING_IDX_MONITOR_STATUS_RING,
	HTT_SW_RING_IDX_MONITOR_BUF_RING,
	HTT_SW_RING_IDX_MONITOR_DESC_RING,
	HTT_SW_RING_IDX_MONITOR_DEST_RING,
	HTT_SW_LMAC_RING_IDX_MAX,
};

/* ppdu stats
 *
 * @details
 * The following field definitions describe the format of the HTT target
 * to host ppdu stats indication message.
 *
 *
 * |31                         16|15   12|11   10|9      8|7            0 |
 * |----------------------------------------------------------------------|
 * |    payload_size             | rsvd  |pdev_id|mac_id  |    msg type   |
 * |----------------------------------------------------------------------|
 * |                          ppdu_id                                     |
 * |----------------------------------------------------------------------|
 * |                        Timestamp in us                               |
 * |----------------------------------------------------------------------|
 * |                          reserved                                    |
 * |----------------------------------------------------------------------|
 * |                    type-specific stats info                          |
 * |                     (see htt_ppdu_stats.h)                           |
 * |----------------------------------------------------------------------|
 * Header fields:
 *  - MSG_TYPE
 *    Bits 7:0
 *    Purpose: Identifies this is a PPDU STATS indication
 *             message.
 *    Value: 0x1d
 *  - mac_id
 *    Bits 9:8
 *    Purpose: mac_id of this ppdu_id
 *    Value: 0-3
 *  - pdev_id
 *    Bits 11:10
 *    Purpose: pdev_id of this ppdu_id
 *    Value: 0-3
 *     0 (for rings at SOC level),
 *     1/2/3 PDEV -> 0/1/2
 *  - payload_size
 *    Bits 31:16
 *    Purpose: total tlv size
 *    Value: payload_size in bytes
 */

#define HTT_T2H_PPDU_STATS_INFO_PDEV_ID GENMASK(11, 10)
#define HTT_T2H_PPDU_STATS_INFO_PAYLOAD_SIZE GENMASK(31, 16)

/* @brief target -> host packet log message
 *
 * @details
 * The following field definitions describe the format of the packet log
 * message sent from the target to the host.
 * The message consists of a 4-octet header,followed by a variable number
 * of 32-bit character values.
 *
 * |31                         16|15  12|11   10|9    8|7            0|
 * |------------------------------------------------------------------|
 * |        payload_size         | rsvd |pdev_id|mac_id|   msg type   |
 * |------------------------------------------------------------------|
 * |                              payload                             |
 * |------------------------------------------------------------------|
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as a pktlog message
 *     Value: HTT_T2H_MSG_TYPE_PKTLOG
 *   - mac_id
 *     Bits 9:8
 *     Purpose: identifies which MAC/PHY instance generated this pktlog info
 *     Value: 0-3
 *   - pdev_id
 *     Bits 11:10
 *     Purpose: pdev_id
 *     Value: 0-3
 *     0 (for rings at SOC level),
 *     1/2/3 PDEV -> 0/1/2
 *   - payload_size
 *     Bits 31:16
 *     Purpose: explicitly specify the payload size
 *     Value: payload size in bytes (payload size is a multiple of 4 bytes)
 */
#define HTT_T2H_PKTLOG_PDEV_ID		GENMASK(11, 10)
#define HTT_T2H_PKTLOG_PAYLOAD_SIZE	GENMASK(31, 16)

struct ath12k_htt_pktlog_msg {
	__le32 header;
	u8 payload[];
} __packed;

struct ath12k_htt_ppdu_stats_msg {
	__le32 info;
	__le32 ppdu_id;
	__le32 timestamp;
	__le32 rsvd;
	u8 data[];
} __packed;

struct htt_tlv {
	__le32 header;
	u8 value[];
} __packed;

#define HTT_TLV_TAG			GENMASK(11, 0)
#define HTT_TLV_LEN			GENMASK(23, 12)

enum HTT_PPDU_STATS_BW {
	HTT_PPDU_STATS_BANDWIDTH_5MHZ   = 0,
	HTT_PPDU_STATS_BANDWIDTH_10MHZ  = 1,
	HTT_PPDU_STATS_BANDWIDTH_20MHZ  = 2,
	HTT_PPDU_STATS_BANDWIDTH_40MHZ  = 3,
	HTT_PPDU_STATS_BANDWIDTH_80MHZ  = 4,
	HTT_PPDU_STATS_BANDWIDTH_160MHZ = 5, /* includes 80+80 */
	HTT_PPDU_STATS_BANDWIDTH_DYN    = 6,
	HTT_PPDU_STATS_BANDWIDTH_DYN_PATTERNS = 7,
	HTT_PPDU_STATS_BANDWIDTH_320MHZ = 8,

};

#define HTT_PPDU_STATS_CMN_FLAGS_FRAME_TYPE_M	GENMASK(7, 0)
#define HTT_PPDU_STATS_CMN_FLAGS_QUEUE_TYPE_M	GENMASK(15, 8)
/* bw - HTT_PPDU_STATS_BW */
#define HTT_PPDU_STATS_CMN_FLAGS_BW_M		GENMASK(19, 16)

struct htt_ppdu_stats_common {
	__le32 ppdu_id;
	__le16 sched_cmdid;
	u8 ring_id;
	u8 num_users;
	__le32 flags; /* %HTT_PPDU_STATS_COMMON_FLAGS_*/
	__le32 chain_mask;
	__le32 fes_duration_us; /* frame exchange sequence */
	__le32 ppdu_sch_eval_start_tstmp_us;
	__le32 ppdu_sch_end_tstmp_us;
	__le32 ppdu_start_tstmp_us;
	/* BIT [15 :  0] - phy mode (WLAN_PHY_MODE) with which ppdu was transmitted
	 * BIT [31 : 16] - bandwidth (in MHz) with which ppdu was transmitted
	 */
	__le16 phy_mode;
	__le16 bw_mhz;
	__le32 cca_delta_time_us;
	__le32 rxfrm_delta_time_us;
	__le32 txfrm_delta_time_us;
	/*  The phy_ppdu_tx_time_us reports the time it took to transmit
	 * a PPDU by itself
	 * BIT [15 :  0] - phy_ppdu_tx_time_us reports the time it took to
	 *                 transmit by itself (not including response time)
	 * BIT [23 : 16] - num_ul_expected_users reports the number of users
	 *                 that are expected to respond to this transmission
	 * BIT [24 : 24] - beam_change reports the beam forming pattern
	 *                 between non-HE and HE portion.
	 *                 If we apply TxBF starting from legacy preamble,
	 *                 then beam_change = 0.
	 *                 If we apply TxBF only starting from HE portion,
	 *                 then beam_change = 1.
	 * BIT [25 : 25] - doppler_indication
	 * BIT [29 : 26] - spatial_reuse for HE_SU,HE_MU and HE_EXT_SU format PPDU
	 *                 HTT_PPDU_STATS_SPATIAL_REUSE
	 * BIT [31 : 30] - reserved
	 */
	__le16 phy_ppdu_tx_time_us;
	u8 num_ul_expected_users;
	u8 reserved;
} __packed;

enum htt_ppdu_stats_gi {
	HTT_PPDU_STATS_SGI_0_8_US,
	HTT_PPDU_STATS_SGI_0_4_US,
	HTT_PPDU_STATS_SGI_1_6_US,
	HTT_PPDU_STATS_SGI_3_2_US,
};

#define HTT_PPDU_STATS_USER_RATE_TLV_TID_NUM_M     GENMASK(7, 0)

#define HTT_PPDU_STATS_USER_RATE_INFO0_USER_POS_M	GENMASK(3, 0)
#define HTT_PPDU_STATS_USER_RATE_INFO0_MU_GROUP_ID_M	GENMASK(11, 4)
#define HTT_PPDU_STATS_USER_RATE_INFO0_RU_SIZE         GENMASK(15, 12)

enum HTT_PPDU_STATS_PPDU_TYPE {
	HTT_PPDU_STATS_PPDU_TYPE_SU,
	HTT_PPDU_STATS_PPDU_TYPE_MU_MIMO,
	HTT_PPDU_STATS_PPDU_TYPE_MU_OFDMA,
	HTT_PPDU_STATS_PPDU_TYPE_MU_MIMO_OFDMA,
	HTT_PPDU_STATS_PPDU_TYPE_UL_TRIG,
	HTT_PPDU_STATS_PPDU_TYPE_BURST_BCN,
	HTT_PPDU_STATS_PPDU_TYPE_UL_BSR_RESP,
	HTT_PPDU_STATS_PPDU_TYPE_UL_BSR_TRIG,
	HTT_PPDU_STATS_PPDU_TYPE_UL_RESP,
	HTT_PPDU_STATS_PPDU_TYPE_MAX
};

enum HTT_PPDU_STATS_RESP_PPDU_TYPE {
       HTT_PPDU_STATS_RESP_PPDU_TYPE_MU_MIMO_UL,
       HTT_PPDU_STATS_RESP_PPDU_TYPE_MU_OFDMA_UL,
};

#define HTT_PPDU_STATS_USER_RATE_INFO1_RESP_TYPE_VALID BIT(0)
#define HTT_PPDU_STATS_USER_RATE_INFO1_PPDU_TYPE_M	GENMASK(5, 1)

#define HTT_PPDU_STATS_USER_RATE_FLAGS_LTF_SIZE_M	GENMASK(1, 0)
#define HTT_PPDU_STATS_USER_RATE_FLAGS_STBC_M		BIT(2)
#define HTT_PPDU_STATS_USER_RATE_FLAGS_HE_RE_M		BIT(3)
#define HTT_PPDU_STATS_USER_RATE_FLAGS_TXBF_M		GENMASK(7, 4)
#define HTT_PPDU_STATS_USER_RATE_FLAGS_BW_M		GENMASK(11, 8)
#define HTT_PPDU_STATS_USER_RATE_FLAGS_NSS_M		GENMASK(15, 12)
#define HTT_PPDU_STATS_USER_RATE_FLAGS_MCS_M		GENMASK(19, 16)
#define HTT_PPDU_STATS_USER_RATE_FLAGS_PREAMBLE_M	GENMASK(23, 20)
#define HTT_PPDU_STATS_USER_RATE_FLAGS_GI_M		GENMASK(27, 24)
#define HTT_PPDU_STATS_USER_RATE_FLAGS_DCM_M		BIT(28)
#define HTT_PPDU_STATS_USER_RATE_FLAGS_LDPC_M		BIT(29)

#define HTT_USR_RATE_PPDU_TYPE(_val) \
		le32_get_bits(_val, HTT_PPDU_STATS_USER_RATE_INFO1_PPDU_TYPE_M)
#define HTT_USR_RATE_PREAMBLE(_val) \
		le32_get_bits(_val, HTT_PPDU_STATS_USER_RATE_FLAGS_PREAMBLE_M)
#define HTT_USR_RATE_BW(_val) \
		le32_get_bits(_val, HTT_PPDU_STATS_USER_RATE_FLAGS_BW_M)
#define HTT_USR_RATE_NSS(_val) \
		le32_get_bits(_val, HTT_PPDU_STATS_USER_RATE_FLAGS_NSS_M)
#define HTT_USR_RATE_MCS(_val) \
		le32_get_bits(_val, HTT_PPDU_STATS_USER_RATE_FLAGS_MCS_M)
#define HTT_USR_RATE_GI(_val) \
		le32_get_bits(_val, HTT_PPDU_STATS_USER_RATE_FLAGS_GI_M)
#define HTT_USR_RATE_DCM(_val) \
		le32_get_bits(_val, HTT_PPDU_STATS_USER_RATE_FLAGS_DCM_M)

#define HTT_PPDU_STATS_USER_RATE_RESP_FLAGS_LTF_SIZE_M		GENMASK(1, 0)
#define HTT_PPDU_STATS_USER_RATE_RESP_FLAGS_STBC_M		BIT(2)
#define HTT_PPDU_STATS_USER_RATE_RESP_FLAGS_HE_RE_M		BIT(3)
#define HTT_PPDU_STATS_USER_RATE_RESP_FLAGS_TXBF_M		GENMASK(7, 4)
#define HTT_PPDU_STATS_USER_RATE_RESP_FLAGS_BW_M		GENMASK(11, 8)
#define HTT_PPDU_STATS_USER_RATE_RESP_FLAGS_NSS_M		GENMASK(15, 12)
#define HTT_PPDU_STATS_USER_RATE_RESP_FLAGS_MCS_M		GENMASK(19, 16)
#define HTT_PPDU_STATS_USER_RATE_RESP_FLAGS_PREAMBLE_M		GENMASK(23, 20)
#define HTT_PPDU_STATS_USER_RATE_RESP_FLAGS_GI_M		GENMASK(27, 24)
#define HTT_PPDU_STATS_USER_RATE_RESP_FLAGS_DCM_M		BIT(28)
#define HTT_PPDU_STATS_USER_RATE_RESP_FLAGS_LDPC_M		BIT(29)
#define HTT_PPDU_STATS_USER_RATE_RESP_FLAGS_PPDU_TYPE          GENMASK(31, 30)

#define HTT_USR_RESP_RATE_PPDU_TYPE(_val) \
       u32_get_bits(_val, HTT_PPDU_STATS_USER_RATE_RESP_FLAGS_PPDU_TYPE)

struct htt_ppdu_stats_user_rate {
	u8 tid_num;
	u8 reserved0;
	__le16 sw_peer_id;
	__le32 info0; /* %HTT_PPDU_STATS_USER_RATE_INFO0_*/
	__le16 ru_end;
	__le16 ru_start;
	__le16 resp_ru_end;
	__le16 resp_ru_start;
	__le32 info1; /* %HTT_PPDU_STATS_USER_RATE_INFO1_ */
	__le32 rate_flags; /* %HTT_PPDU_STATS_USER_RATE_FLAGS_ */
	/* Note: resp_rate_info is only valid for if resp_type is UL */
	__le32 resp_rate_flags; /* %HTT_PPDU_STATS_USER_RATE_RESP_FLAGS_ */
	__le16 punctured;
	__le16 reserved1;
} __packed;

#define HTT_PPDU_STATS_TX_INFO_FLAGS_RATECODE_M		GENMASK(7, 0)
#define HTT_PPDU_STATS_TX_INFO_FLAGS_IS_AMPDU_M		BIT(8)
#define HTT_PPDU_STATS_TX_INFO_FLAGS_BA_ACK_FAILED_M	GENMASK(10, 9)
#define HTT_PPDU_STATS_TX_INFO_FLAGS_BW_M		GENMASK(13, 11)
#define HTT_PPDU_STATS_TX_INFO_FLAGS_SGI_M		BIT(14)
#define HTT_PPDU_STATS_TX_INFO_FLAGS_PEERID_M		GENMASK(31, 16)

#define HTT_TX_INFO_IS_AMSDU(_flags) \
			u32_get_bits(_flags, HTT_PPDU_STATS_TX_INFO_FLAGS_IS_AMPDU_M)
#define HTT_TX_INFO_BA_ACK_FAILED(_flags) \
			u32_get_bits(_flags, HTT_PPDU_STATS_TX_INFO_FLAGS_BA_ACK_FAILED_M)
#define HTT_TX_INFO_RATECODE(_flags) \
			u32_get_bits(_flags, HTT_PPDU_STATS_TX_INFO_FLAGS_RATECODE_M)
#define HTT_TX_INFO_PEERID(_flags) \
			u32_get_bits(_flags, HTT_PPDU_STATS_TX_INFO_FLAGS_PEERID_M)

enum  htt_ppdu_stats_usr_compln_status {
	HTT_PPDU_STATS_USER_STATUS_OK,
	HTT_PPDU_STATS_USER_STATUS_FILTERED,
	HTT_PPDU_STATS_USER_STATUS_RESP_TIMEOUT,
	HTT_PPDU_STATS_USER_STATUS_RESP_MISMATCH,
	HTT_PPDU_STATS_USER_STATUS_ABORT,
};

#define HTT_STATS_MAX_CHAINS 8
#define HTT_PPDU_STATS_USR_CMN_FLAG_DELAYBA    BIT(14)
#define HTT_PPDU_STATS_USR_CMN_HDR_SW_PEERID    GENMASK(31, 16)
#define HTT_PPDU_STATS_USR_CMN_CTL_FRM_CTRL    GENMASK(15, 0)
#define HTT_PPDU_STATS_USER_CMN_TLV_TX_PWR_CHAINS_PER_U32 4
#define HTT_PPDU_STATS_USER_CMN_TX_PWR_ARR_SIZE HTT_STATS_MAX_CHAINS / \
						HTT_PPDU_STATS_USER_CMN_TLV_TX_PWR_CHAINS_PER_U32

/* Common stats for both control and data packets */
struct  htt_ppdu_stats_user_common {
	u8 tid_num;
	u8 vdev_id;
	__le16 sw_peer_id;
	__le32 info;
	__le32 ctrl;
	__le32 buffer_paddr_31_0;
	__le32 buffer_paddr_39_32;
	__le32 host_opaque_cookie;
	__le32 qdepth_bytes;
	__le32 full_aid;
	__le32 data_frm_ppdu_id;
	__le32 sw_rts_prot_dur_us;
	u8 tx_pwr_multiplier;
	u8 chain_enable_bits;
	__le16 reserved;
	/*
	*tx_pwr is applicable for each radio chain
	*tx_pwr for each radio chain is a 8 bit value
	*/
	__le32 tx_pwr[HTT_PPDU_STATS_USER_CMN_TX_PWR_ARR_SIZE];
} __packed;

#define HTT_PPDU_STATS_USR_CMPLTN_CMN_FLAGS_LONG_RETRY_M	GENMASK(3, 0)
#define HTT_PPDU_STATS_USR_CMPLTN_CMN_FLAGS_SHORT_RETRY_M	GENMASK(7, 4)
#define HTT_PPDU_STATS_USR_CMPLTN_CMN_FLAGS_IS_AMPDU_M		BIT(8)
#define HTT_PPDU_STATS_USR_CMPLTN_CMN_FLAGS_RESP_TYPE_M		GENMASK(12, 9)

#define HTT_USR_CMPLTN_IS_AMPDU(_val) \
	    le32_get_bits(_val, HTT_PPDU_STATS_USR_CMPLTN_CMN_FLAGS_IS_AMPDU_M)
#define HTT_USR_CMPLTN_LONG_RETRY(_val) \
	    le32_get_bits(_val, HTT_PPDU_STATS_USR_CMPLTN_CMN_FLAGS_LONG_RETRY_M)
#define HTT_USR_CMPLTN_SHORT_RETRY(_val) \
	    le32_get_bits(_val, HTT_PPDU_STATS_USR_CMPLTN_CMN_FLAGS_SHORT_RETRY_M)

struct htt_ppdu_stats_usr_cmpltn_cmn {
	u8 status;
	u8 tid_num;
	__le16 sw_peer_id;
	/* RSSI value of last ack packet (units = dB above noise floor) */
	__le32 ack_rssi;
	__le16 mpdu_tried;
	__le16 mpdu_success;
	__le32 flags; /* %HTT_PPDU_STATS_USR_CMPLTN_CMN_FLAGS_LONG_RETRIES*/
} __packed;

#define HTT_PPDU_STATS_ACK_BA_INFO_NUM_MPDU_M	GENMASK(8, 0)
#define HTT_PPDU_STATS_ACK_BA_INFO_NUM_MSDU_M	GENMASK(24, 9)
#define HTT_PPDU_STATS_ACK_BA_INFO_TID_NUM	GENMASK(31, 25)

#define HTT_PPDU_STATS_NON_QOS_TID	16
#define HTT_PPDU_STATS_PPDU_ID         GENMASK(24, 0)
#define HTT_PPDU_STATS_CMPLTN_FLUSH_INFO_NUM_MPDU GENMASK(16, 8)

struct htt_ppdu_stats_usr_cmpltn_ack_ba_status {
	__le32 ppdu_id;
	__le16 sw_peer_id;
	__le16 reserved0;
	__le32 info; /* %HTT_PPDU_STATS_USR_CMPLTN_CMN_INFO_ */
	__le16 current_seq;
	__le16 start_seq;
	__le32 success_bytes;
} __packed;

/* Flush stats for failed tx completions */
struct htt_ppdu_stats_cmpltn_flush {
	__le32 drop_reason;
	__le32 info;
	u8 tid_num;
	u8 queue_type;
	__le16 sw_peer_id;
} __packed;

struct htt_ppdu_user_stats {
	u16 peer_id;
	u16 delay_ba;
	u32 tlv_flags;
	bool is_valid_peer_id;
	u8 ru_tones;
	u8 nss;
	struct htt_ppdu_stats_user_rate rate;
	struct htt_ppdu_stats_usr_cmpltn_cmn cmpltn_cmn;
	struct htt_ppdu_stats_usr_cmpltn_ack_ba_status ack_ba;
	struct htt_ppdu_stats_user_common common;
	struct htt_ppdu_stats_cmpltn_flush cmpltn_flush;
};

#define HTT_PPDU_STATS_MAX_USERS	37
#define HTT_PPDU_DESC_MAX_DEPTH	16

struct htt_ppdu_stats {
	struct htt_ppdu_stats_common common;
	struct htt_ppdu_user_stats user_stats[HTT_PPDU_STATS_MAX_USERS];
};

struct htt_ppdu_stats_info {
	u32 tlv_bitmap;
	u32 ppdu_id;
	u32 frame_type;
	u32 frame_ctrl;
	u32 delay_ba;
	u32 bar_num_users;
	u32 max_users;
	struct htt_ppdu_stats ppdu_stats;
	u32 usr_ru_tones_sum;
	u16 htt_frame_type;
	u8 usr_nss_sum;
	struct list_head list;
	u8 pdev_id;
};

/* @brief target -> host MLO offset indiciation message
 *
 * @details
 * The following field definitions describe the format of the HTT target
 * to host mlo offset indication message.
 *
 *
 * |31        29|28    |26|25  22|21 16|15  13|12     10 |9     8|7     0|
 * |---------------------------------------------------------------------|
 * |   rsvd1    | mac_freq                    |chip_id   |pdev_id|msgtype|
 * |---------------------------------------------------------------------|
 * |                           sync_timestamp_lo_us                      |
 * |---------------------------------------------------------------------|
 * |                           sync_timestamp_hi_us                      |
 * |---------------------------------------------------------------------|
 * |                           mlo_offset_lo                             |
 * |---------------------------------------------------------------------|
 * |                           mlo_offset_hi                             |
 * |---------------------------------------------------------------------|
 * |                           mlo_offset_clcks                          |
 * |---------------------------------------------------------------------|
 * |   rsvd2           | mlo_comp_clks |mlo_comp_us                      |
 * |---------------------------------------------------------------------|
 * |   rsvd3                   |mlo_comp_timer                           |
 * |---------------------------------------------------------------------|
 * Header fields
 *  - MSG_TYPE
 *    Bits 7:0
 *    Purpose: Identifies this is a MLO offset indication msg
 *  - PDEV_ID
 *    Bits 9:8
 *    Purpose: Pdev of this MLO offset
 *  - CHIP_ID
 *    Bits 12:10
 *    Purpose: chip_id of this MLO offset
 *  - MAC_FREQ
 *    Bits 28:13
 *  - SYNC_TIMESTAMP_LO_US
 *    Purpose: clock frequency of the mac HW block in MHz
 *    Bits: 31:0
 *    Purpose: lower 32 bits of the WLAN global time stamp at which
 *             last sync interrupt was received
 *  - SYNC_TIMESTAMP_HI_US
 *    Bits: 31:0
 *    Purpose: upper 32 bits of WLAN global time stamp at which
 *             last sync interrupt was received
 *  - MLO_OFFSET_LO
 *    Bits: 31:0
 *    Purpose: lower 32 bits of the MLO offset in us
 *  - MLO_OFFSET_HI
 *    Bits: 31:0
 *    Purpose: upper 32 bits of the MLO offset in us
 *  - MLO_COMP_US
 *    Bits: 15:0
 *    Purpose: MLO time stamp compensation applied in us
 *  - MLO_COMP_CLCKS
 *    Bits: 25:16
 *    Purpose: MLO time stamp compensation applied in clock ticks
 *  - MLO_COMP_TIMER
 *    Bits: 21:0
 *    Purpose: Periodic timer at which compensation is applied
 */

#define HTT_T2H_MLO_OFFSET_INFO_MSG_TYPE        GENMASK(7, 0)
#define HTT_T2H_MLO_OFFSET_INFO_PDEV_ID         GENMASK(9, 8)

struct ath12k_htt_mlo_offset_msg {
	__le32 info;
	__le32 sync_timestamp_lo_us;
	__le32 sync_timestamp_hi_us;
	__le32 mlo_offset_hi;
	__le32 mlo_offset_lo;
	__le32 mlo_offset_clks;
	__le32 mlo_comp_clks;
	__le32 mlo_comp_timer;
} __packed;

/* @brief host -> target FW extended statistics retrieve
 *
 * @details
 * The following field definitions describe the format of the HTT host
 * to target FW extended stats retrieve message.
 * The message specifies the type of stats the host wants to retrieve.
 *
 * |31          24|23          16|15           8|7            0|
 * |-----------------------------------------------------------|
 * |   reserved   | stats type   |   pdev_mask  |   msg type   |
 * |-----------------------------------------------------------|
 * |                   config param [0]                        |
 * |-----------------------------------------------------------|
 * |                   config param [1]                        |
 * |-----------------------------------------------------------|
 * |                   config param [2]                        |
 * |-----------------------------------------------------------|
 * |                   config param [3]                        |
 * |-----------------------------------------------------------|
 * |                         reserved                          |
 * |-----------------------------------------------------------|
 * |                        cookie LSBs                        |
 * |-----------------------------------------------------------|
 * |                        cookie MSBs                        |
 * |-----------------------------------------------------------|
 * Header fields:
 *  - MSG_TYPE
 *    Bits 7:0
 *    Purpose: identifies this is a extended stats upload request message
 *    Value: 0x10
 *  - PDEV_MASK
 *    Bits 8:15
 *    Purpose: identifies the mask of PDEVs to retrieve stats from
 *    Value: This is a overloaded field, refer to usage and interpretation of
 *           PDEV in interface document.
 *           Bit   8    :  Reserved for SOC stats
 *           Bit 9 - 15 :  Indicates PDEV_MASK in DBDC
 *                         Indicates MACID_MASK in DBS
 *  - STATS_TYPE
 *    Bits 23:16
 *    Purpose: identifies which FW statistics to upload
 *    Value: Defined by htt_dbg_ext_stats_type (see htt_stats.h)
 *  - Reserved
 *    Bits 31:24
 *  - CONFIG_PARAM [0]
 *    Bits 31:0
 *    Purpose: give an opaque configuration value to the specified stats type
 *    Value: stats-type specific configuration value
 *           Refer to htt_stats.h for interpretation for each stats sub_type
 *  - CONFIG_PARAM [1]
 *    Bits 31:0
 *    Purpose: give an opaque configuration value to the specified stats type
 *    Value: stats-type specific configuration value
 *           Refer to htt_stats.h for interpretation for each stats sub_type
 *  - CONFIG_PARAM [2]
 *    Bits 31:0
 *    Purpose: give an opaque configuration value to the specified stats type
 *    Value: stats-type specific configuration value
 *           Refer to htt_stats.h for interpretation for each stats sub_type
 *  - CONFIG_PARAM [3]
 *    Bits 31:0
 *    Purpose: give an opaque configuration value to the specified stats type
 *    Value: stats-type specific configuration value
 *           Refer to htt_stats.h for interpretation for each stats sub_type
 *  - Reserved [31:0] for future use.
 *  - COOKIE_LSBS
 *    Bits 31:0
 *    Purpose: Provide a mechanism to match a target->host stats confirmation
 *        message with its preceding host->target stats request message.
 *    Value: LSBs of the opaque cookie specified by the host-side requestor
 *  - COOKIE_MSBS
 *    Bits 31:0
 *    Purpose: Provide a mechanism to match a target->host stats confirmation
 *        message with its preceding host->target stats request message.
 *    Value: MSBs of the opaque cookie specified by the host-side requestor
 */

struct htt_ext_stats_cfg_hdr {
	u8 msg_type;
	u8 pdev_mask;
	u8 stats_type;
	u8 reserved;
} __packed;

struct htt_ext_stats_cfg_cmd {
	struct htt_ext_stats_cfg_hdr hdr;
	__le32 cfg_param0;
	__le32 cfg_param1;
	__le32 cfg_param2;
	__le32 cfg_param3;
	__le32 reserved;
	__le32 cookie_lsb;
	__le32 cookie_msb;
} __packed;

/* htt stats config default params */
#define HTT_STAT_DEFAULT_RESET_START_OFFSET 0
#define HTT_STAT_DEFAULT_CFG0_ALL_HWQS 0xffffffff
#define HTT_STAT_DEFAULT_CFG0_ALL_TXQS 0xffffffff
#define HTT_STAT_DEFAULT_CFG0_ALL_CMDQS 0xffff
#define HTT_STAT_DEFAULT_CFG0_ALL_RINGS 0xffff
#define HTT_STAT_DEFAULT_CFG0_ACTIVE_PEERS 0xff
#define HTT_STAT_DEFAULT_CFG0_CCA_CUMULATIVE 0x00
#define HTT_STAT_DEFAULT_CFG0_ACTIVE_VDEVS 0x00
#define HTT_STAT_DEFAULT_CFG0_MASK 0x07

/* HTT_DBG_EXT_STATS_PEER_INFO
 * PARAMS:
 * @config_param0:
 *  [Bit0] - [0] for sw_peer_id, [1] for mac_addr based request
 *  [Bit15 : Bit 1] htt_peer_stats_req_mode_t
 *  [Bit31 : Bit16] sw_peer_id
 * @config_param1:
 *  peer_stats_req_type_mask:32 (enum htt_peer_stats_tlv_enum)
 *   0 bit htt_peer_stats_cmn_tlv
 *   1 bit htt_peer_details_tlv
 *   2 bit htt_tx_peer_rate_stats_tlv
 *   3 bit htt_rx_peer_rate_stats_tlv
 *   4 bit htt_tx_tid_stats_tlv/htt_tx_tid_stats_v1_tlv
 *   5 bit htt_rx_tid_stats_tlv
 *   6 bit htt_msdu_flow_stats_tlv
 * @config_param2: [Bit31 : Bit0] mac_addr31to0
 * @config_param3: [Bit15 : Bit0] mac_addr47to32
 *                [Bit31 : Bit16] reserved
 */
#define HTT_STAT_PEER_INFO_MAC_ADDR BIT(0)
#define HTT_STAT_DEFAULT_PEER_REQ_TYPE 0x7f

/* Used to set different configs to the specified stats type.*/
struct htt_ext_stats_cfg_params {
	u32 cfg0;
	u32 cfg1;
	u32 cfg2;
	u32 cfg3;
};

#define ATH12K_HTT_MAC_ADDR_L32_0	GENMASK(7, 0)
#define ATH12K_HTT_MAC_ADDR_L32_1	GENMASK(15, 8)
#define ATH12K_HTT_MAC_ADDR_L32_2	GENMASK(23, 16)
#define ATH12K_HTT_MAC_ADDR_L32_3	GENMASK(31, 24)
#define ATH12K_HTT_MAC_ADDR_H16_0	GENMASK(7, 0)
#define ATH12K_HTT_MAC_ADDR_H16_1	GENMASK(15, 8)

struct htt_mac_addr {
	__le32 mac_addr_l32;
	__le32 mac_addr_h16;
} __packed;

/* info0 */
#define HTT_DP_RX_FLOW_FST_SETUP_MSG_TYPE                  GENMASK(7, 0)
#define HTT_DP_RX_FLOW_FST_SETUP_PDEV_ID                   GENMASK(15, 8)
#define HTT_DP_RX_FLOW_FST_SETUP_RESERVED0                 GENMASK(31, 16)

/* info1 */
#define HTT_DP_RX_FLOW_FST_SETUP_NUM_RECORDS               GENMASK(19, 0)
#define HTT_DP_RX_FLOW_FST_SETUP_MAX_SEARCH                GENMASK(27, 20)
#define HTT_DP_RX_FLOW_FST_SETUP_IP_DA_SA                  GENMASK(29, 28)
#define HTT_DP_RX_FLOW_FST_SETUP_RESERVED1                 GENMASK(31, 30)

/* info2 */
#define HTT_DP_RX_FLOW_FST_SETUP_TOEPLITZ                  GENMASK(26, 0)
#define HTT_DP_RX_FLOW_FST_SETUP_RESERVED2                 GENMASK(31, 27)

struct htt_rx_flow_fst_setup_cmd {
	__le32 info0;
	__le32 info1;
	__le32 base_addr_lo;
	__le32 base_addr_hi;
	__le32 toeplitz31_0;
	__le32 toeplitz63_32;
	__le32 toeplitz95_64;
	__le32 toeplitz127_96;
	__le32 toeplitz159_128;
	__le32 toeplitz191_160;
	__le32 toeplitz223_192;
	__le32 toeplitz255_224;
	__le32 toeplitz287_256;
	__le32 info2;
} __packed;

/* info0 */
#define HTT_H2T_MSG_RX_FSE_MSG_TYPE                  GENMASK(7, 0)
#define HTT_H2T_MSG_RX_FSE_PDEV_ID                   GENMASK(15, 8)
#define HTT_H2T_MSG_RX_FSE_RESERVED0                 GENMASK(31, 16)

/* info1 */
#define HTT_H2T_MSG_RX_FSE_IPSEC_VALID               GENMASK(0, 0)
#define HTT_H2T_MSG_RX_FSE_OPERATION                 GENMASK(7, 1)
#define HTT_H2T_MSG_RX_FSE_RESERVED1                 GENMASK(31, 8)

/* info2 */
#define HTT_H2T_MSG_RX_FSE_SRC_PORT                  GENMASK(15, 0)
#define HTT_H2T_MSG_RX_FSE_DEST_PORT                 GENMASK(31, 16)

/* info3 */
#define HTT_H2T_MSG_RX_FSE_L4_PROTO                  GENMASK(7, 0)
#define HTT_H2T_MSG_RX_FSE_RESERVED2                 GENMASK(31, 8)

struct htt_rx_msg_fse_operation {
	__le32 info0;
	__le32 info1;
	__le32 ip_src_addr_31_0;
	__le32 ip_src_addr_63_32;
	__le32 ip_src_addr_95_64;
	__le32 ip_src_addr_127_96;
	__le32 ip_dest_addr_31_0;
	__le32 ip_dest_addr_63_32;
	__le32 ip_dest_addr_95_64;
	__le32 ip_dest_addr_127_96;
	__le32 info2;
	__le32 info3;
} __packed;

struct htt_rx_flow_fst_setup {
	u32 max_entries;
	u32 max_search;
	u32 base_addr_lo;
	u32 base_addr_hi;
	u32 ip_da_sa_prefix;
	u32 hash_key_len;
	u8 *hash_key;
};

enum dp_htt_flow_fst_operation {
	DP_HTT_FST_CACHE_OP_NONE,
	DP_HTT_FST_CACHE_INVALIDATE_ENTRY,
	DP_HTT_FST_CACHE_INVALIDATE_FULL,
	DP_HTT_FST_ENABLE,
	DP_HTT_FST_DISABLE
};

enum htt_rx_fse_operation {
	HTT_RX_FSE_CACHE_INVALIDATE_NONE,
	HTT_RX_FSE_CACHE_INVALIDATE_ENTRY,
	HTT_RX_FSE_CACHE_INVALIDATE_FULL,
	HTT_RX_FSE_DISABLE,
	HTT_RX_FSE_ENABLE,
};

/**
 * @brief host --> target Receive to configure the RxOLE 3-tuple Hash
 *
 * MSG_TYPE => HTT_H2T_MSG_TYPE_3_TUPLE_HASH_CFG
 *
 *     |31            24|23              |15             8|7        3|2|1|0|
 *     |----------------+----------------+----------------+----------------|
 *     |              reserved           |    pdev_id     |    msg_type    |
 *     |---------------------------------+----------------+----------------|
 *     |                        reserved                             |G|E|F|
 *     |---------------------------------+----------------+----------------|
 *     Where E = Configure the target to provide the 3-tuple hash value in
 *               toeplitz_hash_2_or_4 field of rx_msdu_start tlv
 *           F = Configure the target to provide the 3-tuple hash value in
 *               flow_id_toeplitz field of rx_msdu_start tlv
 *           G = Configure the target to provide the 3-tuple based flow
 *               classification search
 *
 * The following field definitions describe the format of the 3 tuple hash value
 * message sent from the host to target as part of initialization sequence.
 *
 * Header fields:
 *  dword0 - b'7:0   - msg_type: This will be set to
 *                     0x16 (HTT_H2T_MSG_TYPE_3_TUPLE_HASH_CFG)
 *           b'15:8  - pdev_id:  0 indicates msg is for all LMAC rings, i.e. soc
 *                     1, 2, 3 indicates pdev_id 0,1,2 and the msg is for the
 *                     specified pdev's LMAC ring.
 *           b'31:16 - reserved : Reserved for future use
 *  dword1 - b'0     - flow_id_toeplitz_field_enable
 *           b'1     - toeplitz_hash_2_or_4_field_enable
 *           b'2     - flow_classification_3_tuple_field_enable
 *           b'31:3  - reserved : Reserved for future use
 * ---------+------+----------------------------------------------------------
 *     bit1 | bit0 |   Functionality
 * ---------+------+----------------------------------------------------------
 *       0  |   1  |   Configure the target to provide the 3 tuple hash value
 *          |      |   in flow_id_toeplitz field
 * ---------+------+----------------------------------------------------------
 *       1  |   0  |   Configure the target to provide the 3 tuple hash value
 *          |      |   in toeplitz_hash_2_or_4 field
 * ---------+------+----------------------------------------------------------
 *       1  |   1  |   Configure the target to provide the 3 tuple hash value
 *          |      |   in both flow_id_toeplitz & toeplitz_hash_2_or_4 field
 * ---------+------+----------------------------------------------------------
 *       0  |   0  |   Configure the target to provide the 5 tuple hash value
 *          |      |   in flow_id_toeplitz field 2 or 4 tuple has value in
 *          |      |   toeplitz_hash_2_or_4 field
 *----------------------------------------------------------------------------
 */
struct htt_h2t_msg_rx_3_tuple_hash_cfg {
	/*
	 * BIT [7:0]      :- H2T msg_type
	 * BIT [15:8]     :- H2T pdev_id
	 * BIT [32:16]    :- Reserved
	 */
	__le32 info0;

	/*
	 * BIT [0]        :- flow_id_toeplitz_field_enable
	 * BIT [1]        :- toeplitz_hash_2_or_4_field_enable
	 * BIT [2]        :- flow_classification_3_tuple_field_enable
	 * BIT [32:3]     :- Reserved
	 */
	__le32 info1;
} __packed;

/* DWORD0 : pdev_id configuration Macros */
#define HTT_H2T_MSG_RX_FSE_3_TUPLE_MSG_TYPE	GENMASK(7, 0)
#define HTT_H2T_MSG_RX_FSE_3_TUPLE_PDEV_ID	GENMASK(15, 8)

/* DWORD1: rx 3 tuple hash value reception field configuration Macros */
#define HTT_H2T_FLOW_CLASSIFY_3_TUPLE_FIELD_ENABLE   1
#define HTT_H2T_FLOW_ID_TOEPLITZ_FIELD_CONFIG	BIT(0)
#define HTT_H2T_TOEPLITZ_2_OR_4_FIELD_CONFIG	BIT(1)
#define HTT_H2T_FLOW_CLASSIFY_3_TUPLE_FIELD_CONFIG   BIT(2)

#define HTT_3_TUPLE_HASH_CFG_REQ_BYTES     8

struct ath12k_htt_mlo_link_peer_info {
	struct htt_tlv tlv_hdr;
	u16 sw_peer_id;
	u8 vdev_id;
	u8 chip_id;
} __packed;

#define ATH12K_HTT_MLO_PEER_MAP_INFO0_PEER_ID		GENMASK(23, 8)
#define ATH12K_HTT_MLO_PEER_MAP_MAC_ADDR_H16		GENMASK(15, 0)
#define ATH12K_HTT_MLO_PEER_MAP_AST_IDX			GENMASK(15, 0)
#define ATH12K_HTT_MLO_PEER_MAP_CACHE_SET_NUM		GENMASK(31, 28)
#define ATH12K_HTT_MAX_MLO_LINKS			3

struct ath12k_htt_mlo_peer_map_msg {
	u32 info0;
	struct htt_mac_addr mac_addr;
	u32 info1;
	u32 info2;
	u32 info3;
	u32 rsvd0;
	u32 rsvd1;
	struct ath12k_htt_mlo_link_peer_info link_peer[ATH12K_HTT_MAX_MLO_LINKS];
} __packed;

#define ATH12K_HTT_MLO_PEER_UNMAP_PEER_ID               GENMASK(23, 8)
struct ath12k_htt_mlo_peer_unmap_msg {
	u32 info0;
} __packed;

#define ATH12K_HTT_PRI_LINK_MIGR_MSG_TYPE		GENMASK(7, 0)
#define ATH12K_HTT_PRI_LINK_MIGR_CHIP_ID		GENMASK(11, 8)
#define ATH12K_HTT_PRI_LINK_MIGR_PDEV_ID		GENMASK(15, 12)
#define ATH12K_HTT_PRI_LINK_MIGR_VDEV_ID		GENMASK(31, 16)
#define ATH12K_HTT_PRI_LINK_MIGR_PEER_ID		GENMASK(15, 0)
#define ATH12K_HTT_PRI_LINK_MIGR_ML_PEER_ID		GENMASK(31, 16)
#define ATH12K_HTT_PRI_LINK_MIGR_STATUS 		GENMASK(7, 0)
#define ATH12K_HTT_PRI_LINK_MIGR_SRC_INFO		GENMASK(23, 8)
#define ATH12K_HTT_PRI_LINK_MIGR_SRC_INFO_VALID 	GENMASK(24, 24)

struct ath12k_htt_pri_link_migr_ind_msg {
	__le32 info0;
	__le32 info1;
} __packed;

struct ath12k_htt_pri_link_migr_h2t_msg {
	/* Bits 7:0    msg_type
	 * Bits 11:8   chip_id
	 * Bits 15:12  pdev_id
	 * Bits 31:16  vdev_id
	 */
	__le32 info0;

	/* Bits 15:0   peer_id
	 * Bits 31:16  ml_peer_id
	 */
	__le32 info1;

	/* Bits 7:0    status
	 * Bits 23:8   src_info
	 * Bit  24     src_info_valid
	 * Bits 31:25  reserved
	 */
	__le32 info2;
} __packed;

/* MSG_TYPE => HTT_T2H_QOS_MSDUQ_INFO_IND
 *
 * @details
 * When QOS is enabled and a flow is mapped to a policy during the traffic
 * flow if the flow is seen the associated service class is conveyed to the
 * target via TCL Data Command. Target on the other hand internally creates the
 * MSDUQ. Once the target creates the MSDUQ the target sends the information
 * of the newly created MSDUQ and some other identifiers to uniquely identity
 * the newly created MSDUQ
 *
 * |31    27|          24|23    16|15|14          11|10|9 8|7     4|3    0|
 * |------------------------------+------------------------+--------------|
 * |             peer ID          |         HTT qtype      |   msg type   |
 * |---------------------------------+--------------+--+---+-------+------|
 * |            reserved             |AST list index|FO|WC | HLOS  | remap|
 * |                                 |              |  |   | TID   | TID  |
 * |---------------------+------------------------------------------------|
 * |    reserved1        |               tgt_opaque_id                    |
 * |---------------------+------------------------------------------------|
 *
 * Header fields:
 *
 * info0 - b'7:0       - msg_type: This will be set to
 *                        0x2e (HTT_T2H_QOS_MSDUQ_INFO_IND)
 *          b'15:8      - HTT qtype
 *          b'31:16     - peer ID
 *
 * info1 - b'3:0       - remap TID, as assigned in firmware
 *          b'7:4       - HLOS TID, as sent by host in TCL Data Command
 *                        hlos_tid : Common to Lithium and Beryllium
 *          b'9:8       - who_classify_info_sel (WC), as sent by host in
 *                        TCL Data Command : Beryllium
 *          b10         - flow_override (FO), as sent by host in
 *                        TCL Data Command: Beryllium
 *          b11:14      - ast_list_idx
 *                        Array index into the list of extension AST entries
 *                        (not the actual AST 16-bit index).
 *                        The ast_list_idx is one-based, with the following
 *                        range of values:
 *                          - legacy targets supporting 16 user-defined
 *                            MSDU queues: 1-2
 *                          - legacy targets supporting 48 user-defined
 *                            MSDU queues: 1-6
 *                          - new targets: 0 (peer_id is used instead)
 *                        Note that since ast_list_idx is one-based,
 *                        the host will need to subtract 1 to use it as an
 *                        index into a list of extension AST entries.
 *          b15:31      - reserved
 *
 * info2 - b'23:0      - tgt_opaque_id Opaque Tx flow number which is a
 *                        unique MSDUQ id in firmware
 *          b'24:31     - reserved1
 */

#define HTT_T2H_QOS_MSDUQ_INFO_0_IND_HTT_QTYPE_ID             GENMASK(15, 8)
#define HTT_T2H_QOS_MSDUQ_INFO_0_IND_PEER_ID                  GENMASK(31, 16)
#define HTT_T2H_QOS_MSDUQ_INFO_1_IND_REMAP_TID_ID             GENMASK(3, 0)
#define HTT_T2H_QOS_MSDUQ_INFO_1_IND_HLOS_TID_ID              GENMASK(7, 4)
#define HTT_T2H_QOS_MSDUQ_INFO_1_IND_WHO_CLSFY_INFO_SEL_ID    GENMASK(9, 8)
#define HTT_T2H_QOS_MSDUQ_INFO_1_IND_FLOW_OVERRIDE_ID         BIT(10)
#define HTT_T2H_QOS_MSDUQ_INFO_1_IND_AST_INDEX_ID             GENMASK(14, 11)
#define HTT_T2H_QOS_MSDUQ_INFO_2_IND_TGT_OPAQUE_ID            GENMASK(23, 0)

struct htt_t2h_qos_info_ind {
	__le32 info0;
	__le32 info1;
	__le32 info2;
} __packed;

struct ath12k_htt_ppdu_id_fmt_info {
	__le32 info;
	__le32 info1;

	/* Bits 15:0   reserved
	 * Bit  16     link_id valid
	 * Bits 21:17  link_id bits
	 * Bits 26:22  link_id offset
	 * Bits 31:27  reserved
	 */
	__le32 link_id;
	__le32 info2;
	__le32 info3;
} __packed;

#define HTT_PPDU_ID_FMT_VALID_BITS	BIT(16)
#define HTT_PPDU_ID_FMT_GET_BITS	GENMASK(21, 17)
#define HTT_PPDU_ID_FMT_GET_OFFSET	GENMASK(26, 22)

int ath12k_dp_htt_connect(struct ath12k_dp *dp);

void ath12k_dp_htt_htc_t2h_msg_handler(struct ath12k_base *ab,
				       struct sk_buff *skb);

int ath12k_dp_htt_tlv_iter(struct ath12k_base *ab, const void *ptr, size_t len,
			   int (*iter)(struct ath12k_base *ar, u16 tag, u16 len,
				       const void *ptr, void *data),
			   void *data);

int ath12k_dp_tx_htt_h2t_ver_req_msg(struct ath12k_base *ab);
int ath12k_dp_tx_htt_h2t_ppdu_stats_req(struct ath12k *ar, u32 mask);
int
ath12k_dp_tx_htt_h2t_ext_stats_req(struct ath12k *ar, u8 type,
				   struct htt_ext_stats_cfg_params *cfg_params,
				   u64 cookie);
int ath12k_dp_tx_htt_rx_filter_setup(struct ath12k_base *ab, u32 ring_id,
				     int mac_id, enum hal_ring_type ring_type,
				     int rx_buf_size,
				     struct htt_rx_ring_tlv_filter *tlv_filter);
int ath12k_dp_tx_htt_tx_filter_setup(struct ath12k_base *ab, u32 ring_id,
				     int mac_id, enum hal_ring_type ring_type,
				     int tx_buf_size,
				     struct htt_tx_ring_tlv_filter *htt_tlv_filter);
int ath12k_dp_htt_rx_flow_fst_setup(struct ath12k_base *ab, struct htt_rx_flow_fst_setup *setup_info);
int ath12k_dp_htt_rx_flow_fse_operation(struct ath12k_base *ab,
					enum dp_htt_flow_fst_operation op_code,
					struct hal_flow_tuple_info *tuple_info);
int ath12k_dp_htt_rx_fse_3_tuple_config_send(struct ath12k_base *ab,
					     u32 tuple_mask, u8 pdev_id);
int ath12k_dp_tx_htt_pri_link_migr_msg(struct ath12k_base *ab, u16 vdev_id,
				       u16 peer_id, u16 ml_peer_id, u8 pdev_id,
				       u8 chip_id, u16 src_info, bool status);
#endif
