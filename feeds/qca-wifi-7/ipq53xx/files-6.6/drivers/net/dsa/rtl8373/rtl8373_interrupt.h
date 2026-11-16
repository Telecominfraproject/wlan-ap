// SPDX-License-Identifier: GPL-2.0
#ifndef __RTL8373_INTERRUPT_H__
#define __RTL8373_INTERRUPT_H__


#define   RTL8373_INTR_ALL_MASK 0x7ff
#define   RTL8373_INTR_POLARITY_HIGH	0
#define   RTL8373_INTR_POLARITY_LOW	1
typedef enum rtk_int_polarity_e
{
    INT_POLAR_HIGH = 0,
    INT_POLAR_LOW,
    INT_POSITIVE_PULSE,
    INT_NEGATIVE_PULSE,
    INT_POLAR_END
} rtk_int_polarity_t;

typedef enum interrupt_misc_e
{
    TM_HIGH=0,
    TM_LOW,
    SMI_CHECK_REG_0,
    SMI_CHECK_REG_1,
    SMI_CHECK_REG_2,
    SMI_CHECK_REG_3,
    SMI_CHECK_REG_4,
    SDS_RX_SYM_ERR_0,
    SDS_RX_SYM_ERR_1,
    SAMOVE = 10,
    AUTO_REC,
    ACL = 13,
    INCPU,
    LOOP_DETEC,
    METER_EXCEED,
    ROUT_PBUF,
    PTP1588,
    INTERRUPT_MISC_END
}interrupt_misc_t;

typedef enum interrupt_glb_e
{
    THERMAL_DET=0,
    SMI_CHK=2,
    SDS_RX_ERR=4,
    GPIO,
    GLB_SAMOVE=7,
    GLB_AUTO_REC,
    GLBACL=10,
    GLB_LOOP_DETEC=12,
    GLB_METER_EXCEED,
    PTP,
    GLB_ROUT_PBUF=24,
    SDS_UPD_PHY,
    SDS_LNK_FLT,
    WOL,
    RLFD,
    GPHY,
    LRN_OVER,
    LINK_CHG,
    INTERRUPT_GLB_END
}interrupt_glb_t;


int rtl8373_irq_disable(struct realtek_smi *priv);
int rtl8373_irq_enable(struct realtek_smi *priv);
irqreturn_t rtl8373_irq(int irq, void *data);
#endif