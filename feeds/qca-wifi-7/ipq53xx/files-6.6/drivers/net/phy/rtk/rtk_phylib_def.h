/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp. All rights reserved.
 */

#ifndef __RTK_PHYLIB_DEF_H
#define __RTK_PHYLIB_DEF_H

#include "type.h"

#define PHY_C22_MMD_PAGE            0x0A41
#define PHY_C22_MMD_DEV_REG         13
#define PHY_C22_MMD_ADD_REG         14

/* MDIO Manageable Device(MDD) address*/
#define PHY_MMD_PMAPMD              1
#define PHY_MMD_PCS                 3
#define PHY_MMD_AN                  7
#define PHY_MMD_VEND1               30   /* Vendor specific 1 */
#define PHY_MMD_VEND2               31   /* Vendor specific 2 */

#define BIT_0        0x00000001U
#define BIT_1        0x00000002U
#define BIT_2        0x00000004U
#define BIT_3        0x00000008U
#define BIT_4        0x00000010U
#define BIT_5        0x00000020U
#define BIT_6        0x00000040U
#define BIT_7        0x00000080U
#define BIT_8        0x00000100U
#define BIT_9        0x00000200U
#define BIT_10       0x00000400U
#define BIT_11       0x00000800U
#define BIT_12       0x00001000U
#define BIT_13       0x00002000U
#define BIT_14       0x00004000U
#define BIT_15       0x00008000U
#define BIT_16       0x00010000U
#define BIT_17       0x00020000U
#define BIT_18       0x00040000U
#define BIT_19       0x00080000U
#define BIT_20       0x00100000U
#define BIT_21       0x00200000U
#define BIT_22       0x00400000U
#define BIT_23       0x00800000U
#define BIT_24       0x01000000U
#define BIT_25       0x02000000U
#define BIT_26       0x04000000U
#define BIT_27       0x08000000U
#define BIT_28       0x10000000U
#define BIT_29       0x20000000U
#define BIT_30       0x40000000U
#define BIT_31       0x80000000U

#define MASK_1_BITS     (BIT_1 - 1)
#define MASK_2_BITS     (BIT_2 - 1)
#define MASK_3_BITS     (BIT_3 - 1)
#define MASK_4_BITS     (BIT_4 - 1)
#define MASK_5_BITS     (BIT_5 - 1)
#define MASK_6_BITS     (BIT_6 - 1)
#define MASK_7_BITS     (BIT_7 - 1)
#define MASK_8_BITS     (BIT_8 - 1)
#define MASK_9_BITS     (BIT_9 - 1)
#define MASK_10_BITS    (BIT_10 - 1)
#define MASK_11_BITS    (BIT_11 - 1)
#define MASK_12_BITS    (BIT_12 - 1)
#define MASK_13_BITS    (BIT_13 - 1)
#define MASK_14_BITS    (BIT_14 - 1)
#define MASK_15_BITS    (BIT_15 - 1)
#define MASK_16_BITS    (BIT_16 - 1)
#define MASK_17_BITS    (BIT_17 - 1)
#define MASK_18_BITS    (BIT_18 - 1)
#define MASK_19_BITS    (BIT_19 - 1)
#define MASK_20_BITS    (BIT_20 - 1)
#define MASK_21_BITS    (BIT_21 - 1)
#define MASK_22_BITS    (BIT_22 - 1)
#define MASK_23_BITS    (BIT_23 - 1)
#define MASK_24_BITS    (BIT_24 - 1)
#define MASK_25_BITS    (BIT_25 - 1)
#define MASK_26_BITS    (BIT_26 - 1)
#define MASK_27_BITS    (BIT_27 - 1)
#define MASK_28_BITS    (BIT_28 - 1)
#define MASK_29_BITS    (BIT_29 - 1)
#define MASK_30_BITS    (BIT_30 - 1)
#define MASK_31_BITS    (BIT_31 - 1)

#define REG32_FIELD_SET(_data, _val, _fOffset, _fMask)      ((_data & ~(_fMask)) | ((_val << (_fOffset)) & (_fMask)))
#define REG32_FIELD_GET(_data, _fOffset, _fMask)            ((_data & (_fMask)) >> (_fOffset))
#define UINT32_BITS_MASK(_mBit, _lBit)                      ((0xFFFFFFFF >> (31 - _mBit)) ^ ((1 << _lBit) - 1))

typedef struct phy_device *  rtk_port_t;

#if 1 /* ss\sdk\include\hal\phy\phydef.h */
/* unified patch format */
typedef enum rtk_phypatch_type_e
{
    PHY_PATCH_TYPE_NONE = 0,
    PHY_PATCH_TYPE_TOP = 1,
    PHY_PATCH_TYPE_SDS,
    PHY_PATCH_TYPE_AFE,
    PHY_PATCH_TYPE_UC,
    PHY_PATCH_TYPE_UC2,
    PHY_PATCH_TYPE_NCTL0,
    PHY_PATCH_TYPE_NCTL1,
    PHY_PATCH_TYPE_NCTL2,
    PHY_PATCH_TYPE_ALGXG,
    PHY_PATCH_TYPE_ALG1G,
    PHY_PATCH_TYPE_NORMAL,
    PHY_PATCH_TYPE_DATARAM,
    PHY_PATCH_TYPE_RTCT,
    PHY_PATCH_TYPE_END
} rtk_phypatch_type_t;

#define RTK_PATCH_TYPE_FLOW(_id)    (PHY_PATCH_TYPE_END + _id)
#define RTK_PATCH_TYPE_FLOWID_MAX   PHY_PATCH_TYPE_END
#define RTK_PATCH_SEQ_MAX     ( PHY_PATCH_TYPE_END + RTK_PATCH_TYPE_FLOWID_MAX -1)

/* Interrupt */
/* PHY Interrupt Status */
#define RTK_PHY_INTR_NEXT_PAGE_RECV       (BIT_0)
#define RTK_PHY_INTR_AN_COMPLETE          (BIT_1)
#define RTK_PHY_INTR_LINK_CHANGE          (BIT_2)
#define RTK_PHY_INTR_ALDPS_STATE_CHANGE   (BIT_3)
#define RTK_PHY_INTR_RLFD                 (BIT_4)
#define RTK_PHY_INTR_TM_LOW               (BIT_5)
#define RTK_PHY_INTR_TM_HIGH              (BIT_6)
#define RTK_PHY_INTR_FATAL_ERROR          (BIT_7)
#define RTK_PHY_INTR_MACSEC               (BIT_8)
#define RTK_PHY_INTR_PTP1588              (BIT_9)
#define RTK_PHY_INTR_WOL                  (BIT_10)

typedef struct rtk_hwpatch_s
{
    uint8    patch_op;
    uint8    portmask;
    uint16   pagemmd;
    uint16   addr;
    uint8    msb;
    uint8    lsb;
    uint16   data;
    uint8    compare_op;
    uint16   sram_p;
    uint16   sram_rr;
    uint16   sram_rw;
    uint16   sram_a;
} rtk_hwpatch_t;

typedef struct rtk_hwpatch_data_s
{
    rtk_hwpatch_t *conf;
    uint32        size;
} rtk_hwpatch_data_t;

typedef struct rtk_hwpatch_seq_s
{
    uint8 patch_type;
    union
    {
        rtk_hwpatch_data_t data;
        uint8 flow_id;
    } patch;
} rtk_hwpatch_seq_t;

typedef struct rt_phy_patch_db_s
{
    /* patch operation */
    int32   (*fPatch_op)(uint32 unit, rtk_port_t port, uint8 portOffset, rtk_hwpatch_t *pPatch_data, uint8 patch_mode);
    int32   (*fPatch_flow)(uint32 unit, rtk_port_t port, uint8 portOffset, uint8 patch_flow, uint8 patch_mode);

    /* patch data */
    rtk_hwpatch_seq_t seq_table[RTK_PATCH_SEQ_MAX];
    rtk_hwpatch_seq_t cmp_table[RTK_PATCH_SEQ_MAX];

} rt_phy_patch_db_t;
#endif

/* cable type for cable test */
typedef enum {
	RTK_RTCT_CABLE_COMMON,
	RTK_RTCT_CABLE_CAT5E,
	RTK_RTCT_CABLE_CAT6A,
} rtk_rtct_cable_type_t;

/* MACSec */
#ifndef RTK_MAX_MACSEC_SA_PER_PORT
  #define RTK_MAX_MACSEC_SA_PER_PORT                  64                              /* max number of Secure Association by a port*/
  #define RTK_MAX_MACSEC_SC_PER_PORT                  RTK_MAX_MACSEC_SA_PER_PORT/4    /* max number of Secure Channel by a port (4 AN per SC) */
#endif

#define RTK_MACSEC_MAX_KEY_LEN        32

typedef enum rtk_macsec_reg_e
{
    RTK_MACSEC_DIR_EGRESS = 0,
    RTK_MACSEC_DIR_INGRESS,
    RTK_MACSEC_DIR_END,
} rtk_macsec_dir_t;

typedef enum rtk_macsec_an_e
{
    RTK_MACSEC_AN0 = 0,
    RTK_MACSEC_AN1,
    RTK_MACSEC_AN2,
    RTK_MACSEC_AN3,
    RTK_MACSEC_AN_MAX,
} rtk_macsec_an_t ;

typedef enum rtk_macsec_flow_e
{
    RTK_MACSEC_FLOW_BYPASS = 0,
    RTK_MACSEC_FLOW_DROP,
    RTK_MACSEC_FLOW_INGRESS,
    RTK_MACSEC_FLOW_EGRESS,
} rtk_macsec_flow_type_t;

typedef enum rtk_macsec_validate_e
{
    RTK_MACSEC_VALIDATE_STRICT = 0,
    RTK_MACSEC_VALIDATE_CHECK,
    RTK_MACSEC_VALIDATE_DISABLE,
} rtk_macsec_validate_t;

typedef enum rtk_macsec_cipher_e
{
    RTK_MACSEC_CIPHER_GCM_ASE_128 = 0,
    RTK_MACSEC_CIPHER_GCM_ASE_256,
    RTK_MACSEC_CIPHER_GCM_ASE_XPN_128,
    RTK_MACSEC_CIPHER_GCM_ASE_XPN_256,
    RTK_MACSEC_CIPHER_MAX,
} rtk_macsec_cipher_t;

typedef enum rtk_macsec_stat_e
{
    RTK_MACSEC_STAT_InPktsUntagged = 0,
    RTK_MACSEC_STAT_InPktsNoTag,
    RTK_MACSEC_STAT_InPktsBadTag,
    RTK_MACSEC_STAT_InPktsUnknownSCI,
    RTK_MACSEC_STAT_InPktsNoSCI,
    RTK_MACSEC_STAT_OutPktsUntagged,
    RTK_MACSEC_STAT_MAX,
} rtk_macsec_stat_t;

typedef enum rtk_macsec_txsa_stat_e
{
    RTK_MACSEC_TXSA_STAT_OutPktsTooLong = 0,
    RTK_MACSEC_TXSA_STAT_OutOctetsProtectedEncrypted,
    RTK_MACSEC_TXSA_STAT_OutPktsProtectedEncrypted,
    RTK_MACSEC_TXSA_STAT_MAX,
} rtk_macsec_txsa_stat_t;

typedef enum rtk_macsec_rxsa_stat_e
{
    RTK_MACSEC_RXSA_STAT_InPktsUnusedSA = 0,
    RTK_MACSEC_RXSA_STAT_InPktsNotUsingSA,
    RTK_MACSEC_RXSA_STAT_InPktsUnchecked,
    RTK_MACSEC_RXSA_STAT_InPktsDelayed,
    RTK_MACSEC_RXSA_STAT_InPktsLate,
    RTK_MACSEC_RXSA_STAT_InPktsOK,
    RTK_MACSEC_RXSA_STAT_InPktsInvalid,
    RTK_MACSEC_RXSA_STAT_InPktsNotValid,
    RTK_MACSEC_RXSA_STAT_InOctetsDecryptedValidated,
    RTK_MACSEC_RXSA_STAT_MAX,
} rtk_macsec_rxsa_stat_t;


typedef enum rtk_macsec_match_tx_e
{
    RTK_MACSEC_MATCH_NON_CTRL = 0, /* match all non-control and untagged packets */
    RTK_MACSEC_MATCH_MAC_DA, /* match all non-control and untagged packets with specific MAC DA */
} rtk_macsec_match_tx_t;

typedef struct rtk_macsec_txsc_s
{
    /* 8-byte SCI([0:5] = MAC address, [6:7] = port index) for this secure channel  */
    uint8      sci[8];

    /* cipher suite for this SC */
    rtk_macsec_cipher_t    cipher_suite;

    /* packet flow type to match this SC */
    rtk_macsec_match_tx_t  flow_match;
    rtk_mac_t  mac_da; /* the target address for RTK_MACSEC_MATCH_MAC_DA */

    uint8 protect_frame;  /* 1 = enable frame protection */
    uint8 include_sci;    /* 1 = include explicit SCI in packet */
    uint8 use_es;         /* 1 = set ES (End Station) bit in TCI field */
    uint8 use_scb;        /* 1 = set SCB (Single Copy Broadcast) bit in TCI field */
    uint8 conf_protect;   /* 1 = enable confidentiality protection, */
}rtk_macsec_txsc_t;

typedef enum rtk_macsec_match_rx_e
{
    RTK_MACSEC_MATCH_SCI = 0,
    RTK_MACSEC_MATCH_MAC_SA,  //for pkt without SCI field/TCI.SC=0,
} rtk_macsec_match_rx_t;

typedef struct rtk_macsec_rxsc_s
{
    /* 8-byte SCI([0:5] = MAC address, [6:7] = port index) for this secure channel  */
    uint8      sci[8];

    /* cipher suite for this SC */
    rtk_macsec_cipher_t   cipher_suite;

    /* packet flow type to match this SC */
    rtk_macsec_match_rx_t flow_match;
    rtk_mac_t  mac_sa; /* the target address for RTK_MACSEC_MATCH_MAC_SA */

    /* frame validation level */
    rtk_macsec_validate_t validate_frames;

    /* replay protection */
    uint8  replay_protect;  /* 1 = enable replay protection */
    uint32 replay_window;   /* the window size for replay protection, range for PN: 0 ~ 2^32 - 1, for XPN: 0 ~ 2^30 */

}rtk_macsec_rxsc_t;

typedef union rtk_macsec_sc_u
{
    rtk_macsec_txsc_t tx;
    rtk_macsec_rxsc_t rx;
}
rtk_macsec_sc_t;

typedef struct rtk_macsec_txsc_status_s
{
    uint32 hw_flow_index;
    uint32 hw_sa_index;
    uint8  sa_inUse;
    uint32 hw_flow_data;
    uint8  hw_sc_flow_status;
    rtk_macsec_an_t running_an;
}
rtk_macsec_txsc_status_t;

typedef struct rtk_macsec_rxsc_status_s
{
    uint32 hw_flow_base;
    uint32 hw_sa_index[RTK_MACSEC_AN_MAX];
    uint8  sa_inUse[RTK_MACSEC_AN_MAX];
    uint32 hw_flow_data[RTK_MACSEC_AN_MAX];
    uint8  hw_sc_flow_status[RTK_MACSEC_AN_MAX];
}
rtk_macsec_rxsc_status_t;

typedef union rtk_macsec_sc_status_u
{
    rtk_macsec_txsc_status_t tx;
    rtk_macsec_rxsc_status_t rx;
}
rtk_macsec_sc_status_t;

typedef struct rtk_macsec_sa_s
{
    uint8 key[RTK_MACSEC_MAX_KEY_LEN];  // MACsec Key.
    uint32 key_bytes; // Size of the MACsec key in bytes (16 for AES128, 32 for AES256).

    uint32 pn;      // PN (32-bit) or lower 32-bit of XPN (64-bit)
    uint32 pn_h;    // higher 32-bit of XPN (64-bit)
    uint8 salt[12]; // 12-byte salt (for XPN).
    uint8 ssci[4];  // 4-byte SSCI value (for XPN).
} rtk_macsec_sa_t;

#define RTK_MACSEC_INTR_EGRESS_PN_THRESHOLD          0x00000001
#define RTK_MACSEC_INTR_EGRESS_PN_ROLLOVER           0x00000002

typedef struct rtk_macsec_intr_status_s
{
    /* a bitmap of RTK_MACSEC_INTR_*  to present occured event */
    uint32 status;

    /* When read 1b, the corresponding MACsec egress SA is about to expire due to
       the packet number crossing the rtk_macsec_port_cfg_t.pn_intr_threshold or xpn_intr_threshold*/
    uint8  egress_pn_thr_an_bmap[RTK_MAX_MACSEC_SC_PER_PORT]; //bitmap of AN3~0.

    /* When read 1b, the corresponding MACsec egress SA has expired due to
       the packet number reaching the maximum allowed value. */
    uint8  egress_pn_exp_an_bmap[RTK_MAX_MACSEC_SC_PER_PORT]; //bitmap of AN3~0.
}
rtk_macsec_intr_status_t;


typedef enum rtk_wol_opt_e
{
    RTK_WOL_OPT_LINK   = (0x1U << 0),
    RTK_WOL_OPT_MAGIC  = (0x1U << 1),
    RTK_WOL_OPT_UCAST  = (0x1U << 2),
    RTK_WOL_OPT_MCAST  = (0x1U << 3),
    RTK_WOL_OPT_BCAST  = (0x1U << 4),
} rtk_wol_opt_t;


#endif /* __RTK_PHYLIB_DEF_H */
