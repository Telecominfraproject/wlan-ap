// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Airoha Inc.
 * Author: Min Yao <min.yao@airoha.com>
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/of_platform.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/of_address.h>

#include "an8855.h"
#include "an8855_regs.h"

/* AN8855 registers */
#define SCU_BASE					0x10000000
#define RG_RGMII_TXCK_C				(SCU_BASE + 0x1d0)

#define HSGMII_AN_CSR_BASE			0x10220000
#define SGMII_REG_AN0				(HSGMII_AN_CSR_BASE + 0x000)
#define SGMII_REG_AN_13				(HSGMII_AN_CSR_BASE + 0x034)
#define SGMII_REG_AN_FORCE_CL37		(HSGMII_AN_CSR_BASE + 0x060)

#define HSGMII_CSR_PCS_BASE			0x10220000
#define RG_HSGMII_PCS_CTROL_1		(HSGMII_CSR_PCS_BASE + 0xa00)
#define RG_AN_SGMII_MODE_FORCE		(HSGMII_CSR_PCS_BASE + 0xa24)

#define MULTI_SGMII_CSR_BASE		0x10224000
#define SGMII_STS_CTRL_0			(MULTI_SGMII_CSR_BASE + 0x018)
#define MSG_RX_CTRL_0				(MULTI_SGMII_CSR_BASE + 0x100)
#define MSG_RX_LIK_STS_0			(MULTI_SGMII_CSR_BASE + 0x514)
#define MSG_RX_LIK_STS_2			(MULTI_SGMII_CSR_BASE + 0x51c)
#define PHY_RX_FORCE_CTRL_0			(MULTI_SGMII_CSR_BASE + 0x520)

#define XFI_CSR_PCS_BASE			0x10225000
#define RG_USXGMII_AN_CONTROL_0		(XFI_CSR_PCS_BASE + 0xbf8)

#define MULTI_PHY_RA_CSR_BASE		0x10226000
#define RG_RATE_ADAPT_CTRL_0		(MULTI_PHY_RA_CSR_BASE + 0x000)
#define RATE_ADP_P0_CTRL_0			(MULTI_PHY_RA_CSR_BASE + 0x100)
#define MII_RA_AN_ENABLE			(MULTI_PHY_RA_CSR_BASE + 0x300)

#define QP_DIG_CSR_BASE				0x1022a000
#define QP_CK_RST_CTRL_4			(QP_DIG_CSR_BASE + 0x310)
#define QP_DIG_MODE_CTRL_0			(QP_DIG_CSR_BASE + 0x324)
#define QP_DIG_MODE_CTRL_1			(QP_DIG_CSR_BASE + 0x330)

#define SERDES_WRAPPER_BASE			0x1022c000
#define USGMII_CTRL_0				(SERDES_WRAPPER_BASE + 0x000)

#define QP_PMA_TOP_BASE				0x1022e000
#define PON_RXFEDIG_CTRL_0			(QP_PMA_TOP_BASE + 0x100)
#define PON_RXFEDIG_CTRL_9			(QP_PMA_TOP_BASE + 0x124)

#define SS_LCPLL_PWCTL_SETTING_2	(QP_PMA_TOP_BASE + 0x208)
#define SS_LCPLL_TDC_FLT_2			(QP_PMA_TOP_BASE + 0x230)
#define SS_LCPLL_TDC_FLT_5			(QP_PMA_TOP_BASE + 0x23c)
#define SS_LCPLL_TDC_PCW_1			(QP_PMA_TOP_BASE + 0x248)
#define INTF_CTRL_8		(QP_PMA_TOP_BASE + 0x320)
#define INTF_CTRL_9		(QP_PMA_TOP_BASE + 0x324)
#define PLL_CTRL_0		(QP_PMA_TOP_BASE + 0x400)
#define PLL_CTRL_2		(QP_PMA_TOP_BASE + 0x408)
#define PLL_CTRL_3		(QP_PMA_TOP_BASE + 0x40c)
#define PLL_CTRL_4		(QP_PMA_TOP_BASE + 0x410)
#define PLL_CK_CTRL_0	(QP_PMA_TOP_BASE + 0x414)
#define RX_DLY_0		(QP_PMA_TOP_BASE + 0x614)
#define RX_CTRL_2		(QP_PMA_TOP_BASE + 0x630)
#define RX_CTRL_5		(QP_PMA_TOP_BASE + 0x63c)
#define RX_CTRL_6		(QP_PMA_TOP_BASE + 0x640)
#define RX_CTRL_7		(QP_PMA_TOP_BASE + 0x644)
#define RX_CTRL_8		(QP_PMA_TOP_BASE + 0x648)
#define RX_CTRL_26		(QP_PMA_TOP_BASE + 0x690)
#define RX_CTRL_42		(QP_PMA_TOP_BASE + 0x6d0)

#define QP_ANA_CSR_BASE				0x1022f000
#define RG_QP_RX_DAC_EN				(QP_ANA_CSR_BASE + 0x00)
#define RG_QP_RXAFE_RESERVE			(QP_ANA_CSR_BASE + 0x04)
#define RG_QP_CDR_LPF_MJV_LIM		(QP_ANA_CSR_BASE + 0x0c)
#define RG_QP_CDR_LPF_SETVALUE		(QP_ANA_CSR_BASE + 0x14)
#define RG_QP_CDR_PR_CKREF_DIV1		(QP_ANA_CSR_BASE + 0x18)
#define RG_QP_CDR_PR_KBAND_DIV_PCIE	(QP_ANA_CSR_BASE + 0x1c)
#define RG_QP_CDR_FORCE_IBANDLPF_R_OFF	(QP_ANA_CSR_BASE + 0x20)
#define RG_QP_TX_MODE_16B_EN		(QP_ANA_CSR_BASE + 0x28)
#define RG_QP_PLL_IPLL_DIG_PWR_SEL	(QP_ANA_CSR_BASE + 0x3c)
#define RG_QP_PLL_SDM_ORD			(QP_ANA_CSR_BASE + 0x40)

#define ETHER_SYS_BASE				0x1028c800
#define RG_P5MUX_MODE				(ETHER_SYS_BASE + 0x00)
#define RG_FORCE_CKDIR_SEL			(ETHER_SYS_BASE + 0x04)
#define RG_SWITCH_MODE				(ETHER_SYS_BASE + 0x08)
#define RG_FORCE_MAC5_SB			(ETHER_SYS_BASE + 0x2c)
#define RG_GPHY_AFE_PWD				(ETHER_SYS_BASE + 0x40)
#define RG_GPHY_SMI_ADDR			(ETHER_SYS_BASE + 0x48)
#define CSR_RMII					(ETHER_SYS_BASE + 0x70)

/* PHY EEE Register bitmap of define */
#define PHY_DEV07				0x07
#define PHY_DEV07_REG_03C		0x3c

/* PHY Extend Register 0x14 bitmap of define */
#define PHY_EXT_REG_14			0x14

/* Fields of PHY_EXT_REG_14 */
#define PHY_EN_DOWN_SHFIT		BIT(4)

/* Unique fields of PMCR for AN8855 */
#define FORCE_TX_FC		BIT(4)
#define FORCE_RX_FC		BIT(5)
#define FORCE_DPX		BIT(25)
#define FORCE_SPD		BITS(28, 30)
#define FORCE_LNK		BIT(24)
#define FORCE_MODE		BIT(31)

#define CHIP_ID			0x10005000
#define CHIP_REV		0x10005004

static int an8855_set_hsgmii_mode(struct gsw_an8855 *gsw)
{
	u32 val = 0;

	/* PLL */
	val = an8855_reg_read(gsw, QP_DIG_MODE_CTRL_1);
	val &= ~(0x3 << 2);
	val |= (0x1 << 2);
	an8855_reg_write(gsw, QP_DIG_MODE_CTRL_1, val);

	/* PLL - LPF */
	val = an8855_reg_read(gsw, PLL_CTRL_2);
	val &= ~(0x3 << 0);
	val |= (0x1 << 0);
	val &= ~(0x7 << 2);
	val |= (0x5 << 2);
	val &= ~BITS(6, 7);
	val &= ~(0x7 << 8);
	val |= (0x3 << 8);
	val |= BIT(29);
	val &= ~BITS(12, 13);
	an8855_reg_write(gsw, PLL_CTRL_2, val);

	/* PLL - ICO */
	val = an8855_reg_read(gsw, PLL_CTRL_4);
	val |= BIT(2);
	an8855_reg_write(gsw, PLL_CTRL_4, val);

	val = an8855_reg_read(gsw, PLL_CTRL_2);
	val &= ~BIT(14);
	an8855_reg_write(gsw, PLL_CTRL_2, val);

	/* PLL - CHP */
	val = an8855_reg_read(gsw, PLL_CTRL_2);
	val &= ~(0xf << 16);
	val |= (0x6 << 16);
	an8855_reg_write(gsw, PLL_CTRL_2, val);

	/* PLL - PFD */
	val = an8855_reg_read(gsw, PLL_CTRL_2);
	val &= ~(0x3 << 20);
	val |= (0x1 << 20);
	val &= ~(0x3 << 24);
	val |= (0x1 << 24);
	val &= ~BIT(26);
	an8855_reg_write(gsw, PLL_CTRL_2, val);

	/* PLL - POSTDIV */
	val = an8855_reg_read(gsw, PLL_CTRL_2);
	val |= BIT(22);
	val &= ~BIT(27);
	val &= ~BIT(28);
	an8855_reg_write(gsw, PLL_CTRL_2, val);

	/* PLL - SDM */
	val = an8855_reg_read(gsw, PLL_CTRL_4);
	val &= ~BITS(3, 4);
	an8855_reg_write(gsw, PLL_CTRL_4, val);

	val = an8855_reg_read(gsw, PLL_CTRL_2);
	val &= ~BIT(30);
	an8855_reg_write(gsw, PLL_CTRL_2, val);

	val = an8855_reg_read(gsw, SS_LCPLL_PWCTL_SETTING_2);
	val &= ~(0x3 << 16);
	val |= (0x1 << 16);
	an8855_reg_write(gsw, SS_LCPLL_PWCTL_SETTING_2, val);

	an8855_reg_write(gsw, SS_LCPLL_TDC_FLT_2, 0x7a000000);
	an8855_reg_write(gsw, SS_LCPLL_TDC_PCW_1, 0x7a000000);

	val = an8855_reg_read(gsw, SS_LCPLL_TDC_FLT_5);
	val &= ~BIT(24);
	an8855_reg_write(gsw, SS_LCPLL_TDC_FLT_5, val);

	val = an8855_reg_read(gsw, PLL_CK_CTRL_0);
	val &= ~BIT(8);
	an8855_reg_write(gsw, PLL_CK_CTRL_0, val);

	/* PLL - SS */
	val = an8855_reg_read(gsw, PLL_CTRL_3);
	val &= ~BITS(0, 15);
	an8855_reg_write(gsw, PLL_CTRL_3, val);

	val = an8855_reg_read(gsw, PLL_CTRL_4);
	val &= ~BITS(0, 1);
	an8855_reg_write(gsw, PLL_CTRL_4, val);

	val = an8855_reg_read(gsw, PLL_CTRL_3);
	val &= ~BITS(16, 31);
	an8855_reg_write(gsw, PLL_CTRL_3, val);

	/* PLL - TDC */
	val = an8855_reg_read(gsw, PLL_CK_CTRL_0);
	val &= ~BIT(9);
	an8855_reg_write(gsw, PLL_CK_CTRL_0, val);

	val = an8855_reg_read(gsw, RG_QP_PLL_SDM_ORD);
	val |= BIT(3);
	val |= BIT(4);
	an8855_reg_write(gsw, RG_QP_PLL_SDM_ORD, val);

	val = an8855_reg_read(gsw, RG_QP_RX_DAC_EN);
	val &= ~(0x3 << 16);
	val |= (0x2 << 16);
	an8855_reg_write(gsw, RG_QP_RX_DAC_EN, val);

	/* TCL Disable (only for Co-SIM) */
	val = an8855_reg_read(gsw, PON_RXFEDIG_CTRL_0);
	val &= ~BIT(12);
	an8855_reg_write(gsw, PON_RXFEDIG_CTRL_0, val);

	/* TX Init */
	val = an8855_reg_read(gsw, RG_QP_TX_MODE_16B_EN);
	val &= ~BIT(0);
	val &= ~(0xffff << 16);
	val |= (0x4 << 16);
	an8855_reg_write(gsw, RG_QP_TX_MODE_16B_EN, val);

	/* RX Control */
	val = an8855_reg_read(gsw, RG_QP_RXAFE_RESERVE);
	val |= BIT(11);
	an8855_reg_write(gsw, RG_QP_RXAFE_RESERVE, val);

	val = an8855_reg_read(gsw, RG_QP_CDR_LPF_MJV_LIM);
	val &= ~(0x3 << 4);
	val |= (0x1 << 4);
	an8855_reg_write(gsw, RG_QP_CDR_LPF_MJV_LIM, val);

	val = an8855_reg_read(gsw, RG_QP_CDR_LPF_SETVALUE);
	val &= ~(0xf << 25);
	val |= (0x1 << 25);
	val &= ~(0x7 << 29);
	val |= (0x3 << 29);
	an8855_reg_write(gsw, RG_QP_CDR_LPF_SETVALUE, val);

	val = an8855_reg_read(gsw, RG_QP_CDR_PR_CKREF_DIV1);
	val &= ~(0x1f << 8);
	val |= (0xf << 8);
	an8855_reg_write(gsw, RG_QP_CDR_PR_CKREF_DIV1, val);

	val = an8855_reg_read(gsw, RG_QP_CDR_PR_KBAND_DIV_PCIE);
	val &= ~(0x3f << 0);
	val |= (0x19 << 0);
	val &= ~BIT(6);
	an8855_reg_write(gsw, RG_QP_CDR_PR_KBAND_DIV_PCIE, val);

	val = an8855_reg_read(gsw, RG_QP_CDR_FORCE_IBANDLPF_R_OFF);
	val &= ~(0x7f << 6);
	val |= (0x21 << 6);
	val &= ~(0x3 << 16);
	val |= (0x2 << 16);
	val &= ~BIT(13);
	an8855_reg_write(gsw, RG_QP_CDR_FORCE_IBANDLPF_R_OFF, val);

	val = an8855_reg_read(gsw, RG_QP_CDR_PR_KBAND_DIV_PCIE);
	val &= ~BIT(30);
	an8855_reg_write(gsw, RG_QP_CDR_PR_KBAND_DIV_PCIE, val);

	val = an8855_reg_read(gsw, RG_QP_CDR_PR_CKREF_DIV1);
	val &= ~(0x7 << 24);
	val |= (0x4 << 24);
	an8855_reg_write(gsw, RG_QP_CDR_PR_CKREF_DIV1, val);

	val = an8855_reg_read(gsw, PLL_CTRL_0);
	val |= BIT(0);
	an8855_reg_write(gsw, PLL_CTRL_0, val);

	val = an8855_reg_read(gsw, RX_CTRL_26);
	val &= ~BIT(23);
	val |= BIT(26);
	an8855_reg_write(gsw, RX_CTRL_26, val);

	val = an8855_reg_read(gsw, RX_DLY_0);
	val &= ~(0xff << 0);
	val |= (0x6f << 0);
	val |= BITS(8, 13);
	an8855_reg_write(gsw, RX_DLY_0, val);

	val = an8855_reg_read(gsw, RX_CTRL_42);
	val &= ~(0x1fff << 0);
	val |= (0x150 << 0);
	an8855_reg_write(gsw, RX_CTRL_42, val);

	val = an8855_reg_read(gsw, RX_CTRL_2);
	val &= ~(0x1fff << 16);
	val |= (0x150 << 16);
	an8855_reg_write(gsw, RX_CTRL_2, val);

	val = an8855_reg_read(gsw, PON_RXFEDIG_CTRL_9);
	val &= ~(0x7 << 0);
	val |= (0x1 << 0);
	an8855_reg_write(gsw, PON_RXFEDIG_CTRL_9, val);

	val = an8855_reg_read(gsw, RX_CTRL_8);
	val &= ~(0xfff << 16);
	val |= (0x200 << 16);
	val &= ~(0x7fff << 14);
	val |= (0xfff << 14);
	an8855_reg_write(gsw, RX_CTRL_8, val);

	/* Frequency memter */
	val = an8855_reg_read(gsw, RX_CTRL_5);
	val &= ~(0xfffff << 10);
	val |= (0x10 << 10);
	an8855_reg_write(gsw, RX_CTRL_5, val);

	val = an8855_reg_read(gsw, RX_CTRL_6);
	val &= ~(0xfffff << 0);
	val |= (0x64 << 0);
	an8855_reg_write(gsw, RX_CTRL_6, val);

	val = an8855_reg_read(gsw, RX_CTRL_7);
	val &= ~(0xfffff << 0);
	val |= (0x2710 << 0);
	an8855_reg_write(gsw, RX_CTRL_7, val);

	/* PCS Init */
	val = an8855_reg_read(gsw, RG_HSGMII_PCS_CTROL_1);
	val &= ~BIT(30);
	an8855_reg_write(gsw, RG_HSGMII_PCS_CTROL_1, val);

	/* Rate Adaption */
	val = an8855_reg_read(gsw, RATE_ADP_P0_CTRL_0);
	val &= ~BIT(31);
	an8855_reg_write(gsw, RATE_ADP_P0_CTRL_0, val);

	val = an8855_reg_read(gsw, RG_RATE_ADAPT_CTRL_0);
	val |= BIT(0);
	val |= BIT(4);
	val |= BITS(26, 27);
	an8855_reg_write(gsw, RG_RATE_ADAPT_CTRL_0, val);

	/* Disable AN */
	val = an8855_reg_read(gsw, SGMII_REG_AN0);
	val &= ~BIT(12);
	an8855_reg_write(gsw, SGMII_REG_AN0, val);

	/* Force Speed */
	val = an8855_reg_read(gsw, SGMII_STS_CTRL_0);
	val |= BIT(2);
	val |= BITS(4, 5);
	an8855_reg_write(gsw, SGMII_STS_CTRL_0, val);

	/* bypass flow control to MAC */
	an8855_reg_write(gsw, MSG_RX_LIK_STS_0, 0x01010107);
	an8855_reg_write(gsw, MSG_RX_LIK_STS_2, 0x00000EEF);

	return 0;
}

static int an8855_sgmii_setup(struct gsw_an8855 *gsw, int mode)
{
	u32 val = 0;

	/* PMA Init */
	/* PLL */
	val = an8855_reg_read(gsw, QP_DIG_MODE_CTRL_1);
	val &= ~BITS(2, 3);
	an8855_reg_write(gsw, QP_DIG_MODE_CTRL_1, val);

	/* PLL - LPF */
	val = an8855_reg_read(gsw, PLL_CTRL_2);
	val &= ~(0x3 << 0);
	val |= (0x1 << 0);
	val &= ~(0x7 << 2);
	val |= (0x5 << 2);
	val &= ~BITS(6, 7);
	val &= ~(0x7 << 8);
	val |= (0x3 << 8);
	val |= BIT(29);
	val &= ~BITS(12, 13);
	an8855_reg_write(gsw, PLL_CTRL_2, val);

	/* PLL - ICO */
	val = an8855_reg_read(gsw, PLL_CTRL_4);
	val |= BIT(2);
	an8855_reg_write(gsw, PLL_CTRL_4, val);

	val = an8855_reg_read(gsw, PLL_CTRL_2);
	val &= ~BIT(14);
	an8855_reg_write(gsw, PLL_CTRL_2, val);

	/* PLL - CHP */
	val = an8855_reg_read(gsw, PLL_CTRL_2);
	val &= ~(0xf << 16);
	val |= (0x4 << 16);
	an8855_reg_write(gsw, PLL_CTRL_2, val);

	/* PLL - PFD */
	val = an8855_reg_read(gsw, PLL_CTRL_2);
	val &= ~(0x3 << 20);
	val |= (0x1 << 20);
	val &= ~(0x3 << 24);
	val |= (0x1 << 24);
	val &= ~BIT(26);
	an8855_reg_write(gsw, PLL_CTRL_2, val);

	/* PLL - POSTDIV */
	val = an8855_reg_read(gsw, PLL_CTRL_2);
	val |= BIT(22);
	val &= ~BIT(27);
	val &= ~BIT(28);
	an8855_reg_write(gsw, PLL_CTRL_2, val);

	/* PLL - SDM */
	val = an8855_reg_read(gsw, PLL_CTRL_4);
	val &= ~BITS(3, 4);
	an8855_reg_write(gsw, PLL_CTRL_4, val);

	val = an8855_reg_read(gsw, PLL_CTRL_2);
	val &= ~BIT(30);
	an8855_reg_write(gsw, PLL_CTRL_2, val);

	val = an8855_reg_read(gsw, SS_LCPLL_PWCTL_SETTING_2);
	val &= ~(0x3 << 16);
	val |= (0x1 << 16);
	an8855_reg_write(gsw, SS_LCPLL_PWCTL_SETTING_2, val);

	an8855_reg_write(gsw, SS_LCPLL_TDC_FLT_2, 0x48000000);
	an8855_reg_write(gsw, SS_LCPLL_TDC_PCW_1, 0x48000000);

	val = an8855_reg_read(gsw, SS_LCPLL_TDC_FLT_5);
	val &= ~BIT(24);
	an8855_reg_write(gsw, SS_LCPLL_TDC_FLT_5, val);

	val = an8855_reg_read(gsw, PLL_CK_CTRL_0);
	val &= ~BIT(8);
	an8855_reg_write(gsw, PLL_CK_CTRL_0, val);

	/* PLL - SS */
	val = an8855_reg_read(gsw, PLL_CTRL_3);
	val &= ~BITS(0, 15);
	an8855_reg_write(gsw, PLL_CTRL_3, val);

	val = an8855_reg_read(gsw, PLL_CTRL_4);
	val &= ~BITS(0, 1);
	an8855_reg_write(gsw, PLL_CTRL_4, val);

	val = an8855_reg_read(gsw, PLL_CTRL_3);
	val &= ~BITS(16, 31);
	an8855_reg_write(gsw, PLL_CTRL_3, val);

	/* PLL - TDC */
	val = an8855_reg_read(gsw, PLL_CK_CTRL_0);
	val &= ~BIT(9);
	an8855_reg_write(gsw, PLL_CK_CTRL_0, val);

	val = an8855_reg_read(gsw, RG_QP_PLL_SDM_ORD);
	val |= BIT(3);
	val |= BIT(4);
	an8855_reg_write(gsw, RG_QP_PLL_SDM_ORD, val);

	val = an8855_reg_read(gsw, RG_QP_RX_DAC_EN);
	val &= ~(0x3 << 16);
	val |= (0x2 << 16);
	an8855_reg_write(gsw, RG_QP_RX_DAC_EN, val);

	/* PLL - TCL Disable (only for Co-SIM) */
	val = an8855_reg_read(gsw, PON_RXFEDIG_CTRL_0);
	val &= ~BIT(12);
	an8855_reg_write(gsw, PON_RXFEDIG_CTRL_0, val);

	/* TX Init */
	val = an8855_reg_read(gsw, RG_QP_TX_MODE_16B_EN);
	val &= ~BIT(0);
	val &= ~BITS(16, 31);
	an8855_reg_write(gsw, RG_QP_TX_MODE_16B_EN, val);

	/* RX Init */
	val = an8855_reg_read(gsw, RG_QP_RXAFE_RESERVE);
	val |= BIT(11);
	an8855_reg_write(gsw, RG_QP_RXAFE_RESERVE, val);

	val = an8855_reg_read(gsw, RG_QP_CDR_LPF_MJV_LIM);
	val &= ~(0x3 << 4);
	val |= (0x2 << 4);
	an8855_reg_write(gsw, RG_QP_CDR_LPF_MJV_LIM, val);

	val = an8855_reg_read(gsw, RG_QP_CDR_LPF_SETVALUE);
	val &= ~(0xf << 25);
	val |= (0x1 << 25);
	val &= ~(0x7 << 29);
	val |= (0x6 << 29);
	an8855_reg_write(gsw, RG_QP_CDR_LPF_SETVALUE, val);

	val = an8855_reg_read(gsw, RG_QP_CDR_PR_CKREF_DIV1);
	val &= ~(0x1f << 8);
	val |= (0xc << 8);
	an8855_reg_write(gsw, RG_QP_CDR_PR_CKREF_DIV1, val);

	val = an8855_reg_read(gsw, RG_QP_CDR_PR_KBAND_DIV_PCIE);
	val &= ~(0x3f << 0);
	val |= (0x19 << 0);
	val &= ~BIT(6);
	an8855_reg_write(gsw, RG_QP_CDR_PR_KBAND_DIV_PCIE, val);

	val = an8855_reg_read(gsw, RG_QP_CDR_FORCE_IBANDLPF_R_OFF);
	val &= ~(0x7f << 6);
	val |= (0x21 << 6);
	val &= ~(0x3 << 16);
	val |= (0x2 << 16);
	val &= ~BIT(13);
	an8855_reg_write(gsw, RG_QP_CDR_FORCE_IBANDLPF_R_OFF, val);

	val = an8855_reg_read(gsw, RG_QP_CDR_PR_KBAND_DIV_PCIE);
	val &= ~BIT(30);
	an8855_reg_write(gsw, RG_QP_CDR_PR_KBAND_DIV_PCIE, val);

	val = an8855_reg_read(gsw, RG_QP_CDR_PR_CKREF_DIV1);
	val &= ~(0x7 << 24);
	val |= (0x4 << 24);
	an8855_reg_write(gsw, RG_QP_CDR_PR_CKREF_DIV1, val);

	val = an8855_reg_read(gsw, PLL_CTRL_0);
	val |= BIT(0);
	an8855_reg_write(gsw, PLL_CTRL_0, val);

	val = an8855_reg_read(gsw, RX_CTRL_26);
	val &= ~BIT(23);
	if (mode == SGMII_MODE_AN)
		val |= BIT(26);

	an8855_reg_write(gsw, RX_CTRL_26, val);

	val = an8855_reg_read(gsw, RX_DLY_0);
	val &= ~(0xff << 0);
	val |= (0x6f << 0);
	val |= BITS(8, 13);
	an8855_reg_write(gsw, RX_DLY_0, val);

	val = an8855_reg_read(gsw, RX_CTRL_42);
	val &= ~(0x1fff << 0);
	val |= (0x150 << 0);
	an8855_reg_write(gsw, RX_CTRL_42, val);

	val = an8855_reg_read(gsw, RX_CTRL_2);
	val &= ~(0x1fff << 16);
	val |= (0x150 << 16);
	an8855_reg_write(gsw, RX_CTRL_2, val);

	val = an8855_reg_read(gsw, PON_RXFEDIG_CTRL_9);
	val &= ~(0x7 << 0);
	val |= (0x1 << 0);
	an8855_reg_write(gsw, PON_RXFEDIG_CTRL_9, val);

	val = an8855_reg_read(gsw, RX_CTRL_8);
	val &= ~(0xfff << 16);
	val |= (0x200 << 16);
	val &= ~(0x7fff << 0);
	val |= (0xfff << 0);
	an8855_reg_write(gsw, RX_CTRL_8, val);

	/* Frequency memter */
	val = an8855_reg_read(gsw, RX_CTRL_5);
	val &= ~(0xfffff << 10);
	val |= (0x28 << 10);
	an8855_reg_write(gsw, RX_CTRL_5, val);

	val = an8855_reg_read(gsw, RX_CTRL_6);
	val &= ~(0xfffff << 0);
	val |= (0x64 << 0);
	an8855_reg_write(gsw, RX_CTRL_6, val);

	val = an8855_reg_read(gsw, RX_CTRL_7);
	val &= ~(0xfffff << 0);
	val |= (0x2710 << 0);
	an8855_reg_write(gsw, RX_CTRL_7, val);

	if (mode == SGMII_MODE_FORCE) {
		/* PCS Init */
		val = an8855_reg_read(gsw, QP_DIG_MODE_CTRL_0);
		val &= ~BIT(0);
		val &= ~BITS(4, 5);
		an8855_reg_write(gsw, QP_DIG_MODE_CTRL_0, val);

		val = an8855_reg_read(gsw, RG_HSGMII_PCS_CTROL_1);
		val &= ~BIT(30);
		an8855_reg_write(gsw, RG_HSGMII_PCS_CTROL_1, val);

		/* Rate Adaption - GMII path config. */
		val = an8855_reg_read(gsw, RG_AN_SGMII_MODE_FORCE);
		val |= BIT(0);
		val &= ~BITS(4, 5);
		an8855_reg_write(gsw, RG_AN_SGMII_MODE_FORCE, val);

		val = an8855_reg_read(gsw, SGMII_STS_CTRL_0);
		val |= BIT(2);
		val &= ~(0x3 << 4);
		val |= (0x2 << 4);
		an8855_reg_write(gsw, SGMII_STS_CTRL_0, val);

		val = an8855_reg_read(gsw, SGMII_REG_AN0);
		val &= ~BIT(12);
		an8855_reg_write(gsw, SGMII_REG_AN0, val);

		val = an8855_reg_read(gsw, PHY_RX_FORCE_CTRL_0);
		val |= BIT(4);
		an8855_reg_write(gsw, PHY_RX_FORCE_CTRL_0, val);

		val = an8855_reg_read(gsw, RATE_ADP_P0_CTRL_0);
		val &= ~BITS(0, 3);
		val |= BIT(28);
		an8855_reg_write(gsw, RATE_ADP_P0_CTRL_0, val);

		val = an8855_reg_read(gsw, RG_RATE_ADAPT_CTRL_0);
		val |= BIT(0);
		val |= BIT(4);
		val |= BITS(26, 27);
		an8855_reg_write(gsw, RG_RATE_ADAPT_CTRL_0, val);
	} else {
		/* PCS Init */
		val = an8855_reg_read(gsw, RG_HSGMII_PCS_CTROL_1);
		val &= ~BIT(30);
		an8855_reg_write(gsw, RG_HSGMII_PCS_CTROL_1, val);

		/* Set AN Ability - Interrupt */
		val = an8855_reg_read(gsw, SGMII_REG_AN_FORCE_CL37);
		val |= BIT(0);
		an8855_reg_write(gsw, SGMII_REG_AN_FORCE_CL37, val);

		val = an8855_reg_read(gsw, SGMII_REG_AN_13);
		val &= ~(0x3f << 0);
		val |= (0xb << 0);
		val |= BIT(8);
		an8855_reg_write(gsw, SGMII_REG_AN_13, val);

		/* Rate Adaption - GMII path config. */
		val = an8855_reg_read(gsw, SGMII_REG_AN0);
		val |= BIT(12);
		an8855_reg_write(gsw, SGMII_REG_AN0, val);

		val = an8855_reg_read(gsw, MII_RA_AN_ENABLE);
		val |= BIT(0);
		an8855_reg_write(gsw, MII_RA_AN_ENABLE, val);

		val = an8855_reg_read(gsw, RATE_ADP_P0_CTRL_0);
		val |= BIT(28);
		an8855_reg_write(gsw, RATE_ADP_P0_CTRL_0, val);

		val = an8855_reg_read(gsw, RG_RATE_ADAPT_CTRL_0);
		val |= BIT(0);
		val |= BIT(4);
		val |= BITS(26, 27);
		an8855_reg_write(gsw, RG_RATE_ADAPT_CTRL_0, val);

		/* Only for Co-SIM */

		/* AN Speed up (Only for Co-SIM) */

		/* Restart AN */
		val = an8855_reg_read(gsw, SGMII_REG_AN0);
		val |= BIT(9);
		val |= BIT(15);
		an8855_reg_write(gsw, SGMII_REG_AN0, val);
	}

	/* bypass flow control to MAC */
	an8855_reg_write(gsw, MSG_RX_LIK_STS_0, 0x01010107);
	an8855_reg_write(gsw, MSG_RX_LIK_STS_2, 0x00000EEF);

	return 0;
}

static int an8855_set_port_rmii(struct gsw_an8855 *gsw)
{
	an8855_reg_write(gsw, RG_P5MUX_MODE, 0x301);
	an8855_reg_write(gsw, RG_FORCE_CKDIR_SEL, 0x101);
	an8855_reg_write(gsw, RG_SWITCH_MODE, 0x101);
	an8855_reg_write(gsw, RG_FORCE_MAC5_SB, 0x1010101);
	an8855_reg_write(gsw, CSR_RMII, 0x420102);
	an8855_reg_write(gsw, RG_RGMII_TXCK_C, 0x1100910);
	return 0;
}

static int an8855_set_port_rgmii(struct gsw_an8855 *gsw)
{
	an8855_reg_write(gsw, RG_FORCE_MAC5_SB, 0x20101);
	return 0;
}

static int an8855_mac_port_setup(struct gsw_an8855 *gsw, u32 port,
				 struct an8855_port_cfg *port_cfg)
{
	u32 pmcr;

	if (port != 5) {
		dev_info(gsw->dev, "port %d is not a MAC port\n", port);
		return -EINVAL;
	}

	if (port_cfg->enabled) {
		pmcr = an8855_reg_read(gsw, PMCR(5));

		switch (port_cfg->phy_mode) {
		case PHY_INTERFACE_MODE_RMII:
			pmcr &= ~FORCE_SPD;
			pmcr |= FORCE_MODE | (MAC_SPD_100 << 28) | FORCE_DPX
				| FORCE_LNK | FORCE_TX_FC | FORCE_RX_FC;
			an8855_set_port_rmii(gsw);
			break;
		case PHY_INTERFACE_MODE_RGMII:
			pmcr &= ~FORCE_SPD;
			pmcr |= FORCE_MODE | (MAC_SPD_1000 << 28) | FORCE_DPX
				| FORCE_LNK | FORCE_TX_FC | FORCE_RX_FC;
			an8855_set_port_rgmii(gsw);
			break;
		case PHY_INTERFACE_MODE_SGMII:
			if (port_cfg->force_link) {
				pmcr &= ~FORCE_SPD;
				pmcr |= FORCE_MODE | (MAC_SPD_1000 << 28)
					 | FORCE_DPX | FORCE_LNK | FORCE_TX_FC
					 | FORCE_RX_FC;
				an8855_sgmii_setup(gsw, SGMII_MODE_FORCE);
			} else
				an8855_sgmii_setup(gsw, SGMII_MODE_AN);
			break;
		case PHY_INTERFACE_MODE_2500BASEX:
			pmcr &= ~FORCE_SPD;
			pmcr |= FORCE_MODE | (MAC_SPD_2500 << 28) | FORCE_DPX
				| FORCE_LNK | FORCE_TX_FC | FORCE_RX_FC;
			an8855_set_hsgmii_mode(gsw);
			break;
		default:
			dev_info(gsw->dev, "%s is not supported by port %d\n",
				 phy_modes(port_cfg->phy_mode), port);
		}

		if (port_cfg->force_link)
			an8855_reg_write(gsw, PMCR(port), pmcr);
	}

	return 0;
}

static int an8855_sw_detect(struct gsw_an8855 *gsw, struct chip_rev *crev)
{
	u32 id, rev;

	id = an8855_reg_read(gsw, CHIP_ID);
	rev = an8855_reg_read(gsw, CHIP_REV);
	if (id == AN8855) {
		if (crev) {
			crev->rev = rev;
			crev->name = "AN8855";
		}

		return 0;
	}

	return -ENODEV;
}

static void an8855_phy_setting(struct gsw_an8855 *gsw)
{
	int i;
	u32 val;

	/* Release power down */
	an8855_reg_write(gsw, RG_GPHY_AFE_PWD, 0x0);

	for (i = 0; i < AN8855_NUM_PHYS; i++) {
		/* Enable HW auto downshift */
		gsw->mii_write(gsw, i, 0x1f, 0x1);
		val = gsw->mii_read(gsw, i, PHY_EXT_REG_14);
		val |= PHY_EN_DOWN_SHFIT;
		gsw->mii_write(gsw, i, PHY_EXT_REG_14, val);
		gsw->mii_write(gsw, i, 0x1f, 0x0);

		/* Enable Asymmetric Pause Capability */
		val = gsw->mii_read(gsw, i, MII_ADVERTISE);
		val |= ADVERTISE_PAUSE_ASYM;
		gsw->mii_write(gsw, i, MII_ADVERTISE, val);
	}
}

static void an8855_low_power_setting(struct gsw_an8855 *gsw)
{
	int port, addr;

	for (port = 0; port < AN8855_NUM_PHYS; port++) {
		gsw->mmd_write(gsw, port, 0x1e, 0x11, 0x0f00);
		gsw->mmd_write(gsw, port, 0x1e, 0x3c, 0x0000);
		gsw->mmd_write(gsw, port, 0x1e, 0x3d, 0x0000);
		gsw->mmd_write(gsw, port, 0x1e, 0x3e, 0x0000);
		gsw->mmd_write(gsw, port, 0x1e, 0xc6, 0x53aa);
	}

	gsw->mmd_write(gsw, 0, 0x1f, 0x268, 0x07f1);
	gsw->mmd_write(gsw, 0, 0x1f, 0x269, 0x2111);
	gsw->mmd_write(gsw, 0, 0x1f, 0x26a, 0x0000);
	gsw->mmd_write(gsw, 0, 0x1f, 0x26b, 0x0074);
	gsw->mmd_write(gsw, 0, 0x1f, 0x26e, 0x00f6);
	gsw->mmd_write(gsw, 0, 0x1f, 0x26f, 0x6666);
	gsw->mmd_write(gsw, 0, 0x1f, 0x271, 0x2c02);
	gsw->mmd_write(gsw, 0, 0x1f, 0x272, 0x0c22);
	gsw->mmd_write(gsw, 0, 0x1f, 0x700, 0x0001);
	gsw->mmd_write(gsw, 0, 0x1f, 0x701, 0x0803);
	gsw->mmd_write(gsw, 0, 0x1f, 0x702, 0x01b6);
	gsw->mmd_write(gsw, 0, 0x1f, 0x703, 0x2111);

	gsw->mmd_write(gsw, 1, 0x1f, 0x700, 0x0001);

	for (addr = 0x200; addr <= 0x230; addr += 2)
		gsw->mmd_write(gsw, 0, 0x1f, addr, 0x2020);

	for (addr = 0x201; addr <= 0x231; addr += 2)
		gsw->mmd_write(gsw, 0, 0x1f, addr, 0x0020);
}

static void an8855_eee_setting(struct gsw_an8855 *gsw, u32 port)
{
	/* Disable EEE */
	gsw->mmd_write(gsw, port, PHY_DEV07, PHY_DEV07_REG_03C, 0);
}

static int an8855_sw_init(struct gsw_an8855 *gsw)
{
	int i;
	u32 val;

	gsw->phy_base = gsw->smi_addr & AN8855_SMI_ADDR_MASK;

	gsw->mii_read = an8855_mii_read;
	gsw->mii_write = an8855_mii_write;
	gsw->mmd_read = an8855_mmd_read;
	gsw->mmd_write = an8855_mmd_write;

	/* Force MAC link down before reset */
	an8855_reg_write(gsw, PMCR(5), FORCE_MODE);

	/* Switch soft reset */
	an8855_reg_write(gsw, SYS_CTRL, SW_SYS_RST);
	usleep_range(100000, 110000);

	/* change gphy smi address */
	if (gsw->new_smi_addr != gsw->smi_addr) {
		an8855_reg_write(gsw, RG_GPHY_SMI_ADDR, gsw->new_smi_addr);
		gsw->smi_addr = gsw->new_smi_addr;
		gsw->phy_base = gsw->new_smi_addr;
	}

	for (i = 0; i < AN8855_NUM_PHYS; i++) {
		val = gsw->mii_read(gsw, i, MII_BMCR);
		val |= BMCR_ISOLATE;
		gsw->mii_write(gsw, i, MII_BMCR, val);
	}

	an8855_mac_port_setup(gsw, 5, &gsw->port5_cfg);

	/* Global mac control settings */
	val = an8855_reg_read(gsw, GMACCR);
	val |= (15 << MAX_RX_JUMBO_S) | RX_PKT_LEN_MAX_JUMBO;
	an8855_reg_write(gsw, GMACCR, val);

	val = an8855_reg_read(gsw, CKGCR);
	val &= ~(CKG_LNKDN_GLB_STOP | CKG_LNKDN_PORT_STOP);
	an8855_reg_write(gsw, CKGCR, val);
	return 0;
}

static int an8855_sw_post_init(struct gsw_an8855 *gsw)
{
	int i;
	u32 val;

	for (i = 0; i < AN8855_NUM_PHYS; i++) {
		val = gsw->mii_read(gsw, i, MII_BMCR);
		val &= ~BMCR_ISOLATE;
		gsw->mii_write(gsw, i, MII_BMCR, val);
	}

	an8855_phy_setting(gsw);

	for (i = 0; i < AN8855_NUM_PHYS; i++)
		an8855_eee_setting(gsw, i);

	/* PHY restart AN*/
	for (i = 0; i < AN8855_NUM_PHYS; i++)
		gsw->mii_write(gsw, i, MII_BMCR, 0x1240);

	return 0;
}

struct an8855_sw_id an8855_id = {
	.model = AN8855,
	.detect = an8855_sw_detect,
	.init = an8855_sw_init,
	.post_init = an8855_sw_post_init
};

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Min Yao <min.yao@airoha.com>");
MODULE_DESCRIPTION("Driver for Airoha AN8855 Gigabit Switch");
