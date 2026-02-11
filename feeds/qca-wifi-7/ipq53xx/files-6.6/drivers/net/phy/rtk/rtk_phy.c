/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp. All rights reserved.
 */

#include <linux/module.h>
#include <linux/phy.h>
#include <linux/ethtool.h>
#include <linux/netdevice.h>

#include "phy_rtl826xb_patch.h"

#include "rtk_phylib_rtl826xb.h"
#include "rtk_phylib_macsec.h"
#include "rtk_phylib.h"

#include "rtk_phy.h"
/*
static int rtk_phy_cable_test_report_trans(rtk_rtct_channel_result_t *result)
{
    if(result->cable_status == 0)
        return ETHTOOL_A_CABLE_RESULT_CODE_OK;

    if(result->cable_status & RTK_PHYLIB_CABLE_STATUS_INTER_PAIR_SHORT)
        return ETHTOOL_A_CABLE_RESULT_CODE_SAME_SHORT;
    if(result->cable_status & RTK_PHYLIB_CABLE_STATUS_SHORT)
        return ETHTOOL_A_CABLE_RESULT_CODE_CROSS_SHORT;
    if(result->cable_status & RTK_PHYLIB_CABLE_STATUS_OPEN)
        return ETHTOOL_A_CABLE_RESULT_CODE_OPEN;
    if(result->cable_status & RTK_PHYLIB_CABLE_STATUS_CROSS)
        return ETHTOOL_A_CABLE_RESULT_CODE_CROSS_SHORT;

    return ETHTOOL_A_CABLE_RESULT_CODE_UNSPEC;
}*/

static int rtl826xb_get_features(struct phy_device *phydev)
{
    int ret;
    struct rtk_phy_priv *priv = phydev->priv;

    ret = genphy_c45_pma_read_abilities(phydev);
    if (ret)
        return ret;

    linkmode_or(phydev->supported, phydev->supported, PHY_BASIC_FEATURES);

    linkmode_set_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
                       phydev->supported);
    linkmode_set_bit(ETHTOOL_LINK_MODE_5000baseT_Full_BIT,
                       phydev->supported);

    /* not support 10M modes */
    linkmode_clear_bit(ETHTOOL_LINK_MODE_10baseT_Half_BIT,
                       phydev->supported);
    linkmode_clear_bit(ETHTOOL_LINK_MODE_10baseT_Full_BIT,
                       phydev->supported);

    switch (priv->phytype)
    {
        case RTK_PHYLIB_RTL8251L:
        case RTK_PHYLIB_RTL8254B:
            linkmode_clear_bit(ETHTOOL_LINK_MODE_10000baseT_Full_BIT,
                       phydev->supported);
            break;

        default:
	    linkmode_clear_bit(ETHTOOL_LINK_MODE_10000baseT_Full_BIT,
                       phydev->supported);
            break;
    }

    return 0;
}

static int rtl826xb_probe(struct phy_device *phydev)
{
    struct rtk_phy_priv *priv = NULL;
    int data = 0;

    priv = devm_kzalloc(&phydev->mdio.dev, sizeof(struct rtk_phy_priv), GFP_KERNEL);
    if (!priv)
    {
        return -ENOMEM;
    }
    memset(priv, 0, sizeof(struct rtk_phy_priv));

    if (phy_rtl826xb_patch_db_init(0, phydev, &(priv->patch)) != RT_ERR_OK)
        return -ENOMEM;

    if (phydev->drv->phy_id == REALTEK_PHY_ID_RTL8261N)
    {
        data =  phy_read_mmd(phydev, 30, 0x103);
        if (data < 0)
            return data;

        if (data == 0x8251)
        {
            priv->phytype = RTK_PHYLIB_RTL8251L;
        }
        else
        {
            priv->phytype = RTK_PHYLIB_RTL8261N;
        }
    }
    else if (phydev->drv->phy_id == REALTEK_PHY_ID_RTL8264B)
    {
        data =  phy_read_mmd(phydev, 30, 0x103);
        if (data < 0)
            return data;

        if (data == 0x8254)
        {
            priv->phytype = RTK_PHYLIB_RTL8254B;
        }
        else
        {
            priv->phytype = RTK_PHYLIB_RTL8264B;
        }
    }
    priv->isBasePort = (phydev->drv->phy_id == REALTEK_PHY_ID_RTL8261N) ? (1) : (((phydev->mdio.addr % 4) == 0) ? (1) : (0));
    phydev->priv = priv;

    return 0;
}

static int rtkphy_config_init(struct phy_device *phydev)
{
    int ret = 0;
    switch (phydev->drv->phy_id)
    {
        case REALTEK_PHY_ID_RTL8261N:
        case REALTEK_PHY_ID_RTL8264B:
            phydev_info(phydev, "%s:%u [RTL8261N/RTL826XB] phy_id: 0x%X PHYAD:%d\n", __FUNCTION__, __LINE__, phydev->drv->phy_id, phydev->mdio.addr);

          #if 1 /* toggle reset */
            phy_write_mmd(phydev, 30, 0x145, 0x0001);
            phy_write_mmd(phydev, 30, 0x145, 0x0000);
            mdelay(30);
          #endif

            ret = phy_patch(0, phydev, 0, PHY_PATCH_MODE_NORMAL);
            if (ret)
            {
                phydev_err(phydev, "%s:%u [RTL8261N/RTL826XB] patch failed!! 0x%X\n", __FUNCTION__, __LINE__, ret);
                return ret;
            }

            ret = rtk_phylib_826xb_intr_init(phydev);
            if (ret)
            {
                phydev_err(phydev, "%s:%u [RTL8261N/RTL826XB] rtk_phylib_826xb_intr_init failed!! 0x%X\n", __FUNCTION__, __LINE__, ret);
                return ret;
            }

            ret = rtk_macsec_init(phydev);
            if (ret)
            {
                phydev_err(phydev, "%s:%u [RTL8261N/RTL826XB] rtk_macsec_init failed!! 0x%X\n", __FUNCTION__, __LINE__, ret);
                return ret;
            }
            ret = rtk_phylib_826xb_sds_write(phydev, 6, 3, 15, 0, 0x88C6);
            if (ret)
            {
                phydev_err(phydev, "%s:%u [RTL8261N/RTL826XB] SerDes init failed!! 0x%X\n", __FUNCTION__, __LINE__, ret);
                return ret;
            }

          #if 0 /* Debug: patch check */
            ret = phy_patch(0, phydev, 0, PHY_PATCH_MODE_CMP);
            if (ret)
            {
                phydev_err(phydev, "%s:%u [RTL8261N/RTL826XB] phy_patch failed!! 0x%X\n", __FUNCTION__, __LINE__, ret);
                return ret;
            }
            printk("[%s,%u] patch chk %s\n", __FUNCTION__, __LINE__, (ret == 0) ? "PASS" : "FAIL");
          #endif
          #if 0 /* Debug: USXGMII*/
            {
                uint32 data = 0;
                rtk_phylib_826xb_sds_read(phydev, 0x07, 0x10, 15, 0, &data);
                printk("[%s,%u] SDS 0x07, 0x10 : 0x%X\n", __FUNCTION__, __LINE__, data);
                rtk_phylib_826xb_sds_read(phydev, 0x06, 0x12, 15, 0, &data);
                printk("[%s,%u] SDS 0x06, 0x12 : 0x%X\n", __FUNCTION__, __LINE__, data);
            }
            {
                u16 sdspage = 0x5, sdsreg = 0x0;
                u16 regData = (sdspage & 0x3f) | ((sdsreg & 0x1f) << 6) | BIT(15);
                u16 readData = 0;
                phy_write_mmd(phydev, 30, 323, regData);
                do
                {
                    udelay(10);
                    readData = phy_read_mmd(phydev, 30, 323);
                } while ((readData & BIT(15)) != 0);
                readData = phy_read_mmd(phydev, 30, 322);
                printk("[%s,%d] sds link [%s] (0x%X)\n", __FUNCTION__, __LINE__, (readData & BIT(12)) ? "UP" : "DOWN", readData);
            }
          #endif

            break;
        default:
            phydev_err(phydev, "%s:%u Unknow phy_id: 0x%X\n", __FUNCTION__, __LINE__, phydev->drv->phy_id);
            return -EPERM;
    }

    return ret;
}

static int rtkphy_c45_suspend(struct phy_device *phydev)
{
    int ret = 0;

    ret = rtk_phylib_c45_power_low(phydev);

    phydev->speed = SPEED_UNKNOWN;
    phydev->duplex = DUPLEX_UNKNOWN;
    phydev->pause = 0;
    phydev->asym_pause = 0;

    return ret;
}

static int rtkphy_c45_resume(struct phy_device *phydev)
{
    return rtk_phylib_c45_power_normal(phydev);
}

static int rtkphy_c45_config_aneg(struct phy_device *phydev)
{
    bool changed = false;
    u16 reg = 0;
    int ret = 0;

    phydev->mdix_ctrl = ETH_TP_MDI_AUTO;
    if (phydev->autoneg == AUTONEG_DISABLE)
    {
        if (phydev->speed != SPEED_100)
        {
            return -EPERM;
        }
        return genphy_c45_pma_setup_forced(phydev);
    }

    ret = genphy_c45_an_config_aneg(phydev);
    if (ret < 0)
        return ret;
    if (ret > 0)
        changed = true;

    reg = 0;
    if (linkmode_test_bit(ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
                  phydev->advertising))
        reg |= BIT(9);

    if (linkmode_test_bit(ETHTOOL_LINK_MODE_1000baseT_Half_BIT,
                  phydev->advertising))
        reg |= BIT(8);

    ret = phy_modify_mmd_changed(phydev, MDIO_MMD_VEND2, 0xA412,
                     BIT(9) | BIT(8) , reg);
    if (ret < 0)
        return ret;
    if (ret > 0)
        changed = true;

    return genphy_c45_check_and_restart_aneg(phydev, changed);
}

static int rtkphy_c45_aneg_done(struct phy_device *phydev)
{
    return genphy_c45_aneg_done(phydev);
}

static int rtkphy_c45_read_status(struct phy_device *phydev)
{
    int ret = 0, status = 0;
    phydev->speed = SPEED_UNKNOWN;
    phydev->duplex = DUPLEX_UNKNOWN;
    phydev->pause = 0;
    phydev->asym_pause = 0;

    ret = genphy_c45_read_link(phydev);
    if (ret)
        return ret;

    if (phydev->autoneg == AUTONEG_ENABLE)
    {
        linkmode_clear_bit(ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
           phydev->lp_advertising);

        ret = genphy_c45_read_lpa(phydev);
        if (ret)
            return ret;

        status =  phy_read_mmd(phydev, 31, 0xA414);
        if (status < 0)
            return status;
        linkmode_mod_bit(ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
            phydev->lp_advertising, status & BIT(11));

        phy_resolve_aneg_linkmode(phydev);
    }
    else
    {
        ret = genphy_c45_read_pma(phydev);
    }

    /* mdix*/
    status = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_10GBT_SWAPPOL);
    if (status < 0)
        return status;

    switch (status & 0x3)
    {
        case MDIO_PMA_10GBT_SWAPPOL_ABNX | MDIO_PMA_10GBT_SWAPPOL_CDNX:
            phydev->mdix = ETH_TP_MDI;
            break;

        case 0:
            phydev->mdix = ETH_TP_MDI_X;
            break;

        default:
            phydev->mdix = ETH_TP_MDI_INVALID;
            break;
    }

    return ret;
}

static int rtkphy_c45_pcs_loopback(struct phy_device *phydev, bool enable)
{
    return rtk_phylib_c45_pcs_loopback(phydev, (enable == true) ? 1 : 0);
}
/*
static int rtl826xb_cable_test_start(struct phy_device *phydev)
{
    return rtk_phylib_826xb_cable_test_start(phydev);
}
*/
/*
static int rtl826xb_cable_test_get_status(struct phy_device *phydev, bool *finished)
{
    uint32 finish_read = 0;
    int32 ret = 0;
    uint32 pair = 0;
    rtk_rtct_channel_result_t reslut;

    *finished = false;
    RTK_PHYLIB_ERR_CHK(rtk_phylib_826xb_cable_test_finished_get(phydev, &finish_read));
    *finished = (finish_read == 1) ? true : false;

    if (finish_read == 1)
    {
        for (pair = 0; pair < 4; pair++)
        {
            RTK_PHYLIB_ERR_CHK(rtk_phylib_826xb_cable_test_result_get(phydev, pair, &reslut));
            ethnl_cable_test_result(phydev, pair, rtk_phy_cable_test_report_trans(&reslut));

            if(reslut.cable_status != RTK_PHYLIB_CABLE_STATUS_NORMAL)
                ethnl_cable_test_fault_length(phydev, pair, reslut.length_cm);
        }
    }
    return ret;
}
*/
static int rtl826xb_config_intr(struct phy_device *phydev)
{
    int32 ret = 0;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_826xb_intr_enable(phydev, (phydev->interrupts == PHY_INTERRUPT_ENABLED)? 1 : 0));
    return ret;
}
/*
static int rtl826xb_ack_intr(struct phy_device *phydev)
{
    int32 ret = 0;
    uint32 status = 0;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_826xb_intr_read_clear(phydev, &status));
    if (status & RTK_PHY_INTR_WOL)
    {
        rtk_phylib_826xb_wol_reset(phydev);
    }

    return ret;
}
*/
static irqreturn_t rtl826xb_handle_intr(struct phy_device *phydev)
{
    irqreturn_t ret;
    uint32 status = 0;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_826xb_intr_read_clear(phydev, &status));
    if (status & RTK_PHY_INTR_LINK_CHANGE)
    {
        pr_debug("[%s,%d] RTK_PHY_INTR_LINK_CHANGE\n", __FUNCTION__, __LINE__);
        phy_mac_interrupt(phydev);
    }

    if (status & RTK_PHY_INTR_WOL)
    {
        pr_debug("[%s,%d] RTK_PHY_INTR_WOL\n", __FUNCTION__, __LINE__);
        rtk_phylib_826xb_wol_reset(phydev);
    }

    return IRQ_HANDLED;
}

static int rtl826xb_get_tunable(struct phy_device *phydev, struct ethtool_tunable *tuna, void *data)
{
    int32 ret = 0;
    uint32 val = 0;

    switch (tuna->id)
    {
        case ETHTOOL_PHY_EDPD:
            RTK_PHYLIB_ERR_CHK(rtk_phylib_826xb_link_down_power_saving_get(phydev, &val));
            *(u16 *)data = (val == 0) ? ETHTOOL_PHY_EDPD_DISABLE : ETHTOOL_PHY_EDPD_DFLT_TX_MSECS;
            return 0;

        default:
            return -EOPNOTSUPP;
    }
}

static int rtl826xb_set_tunable(struct phy_device *phydev, struct ethtool_tunable *tuna, const void *data)
{
    int32 ret = 0;
    uint32 val = 0;

    switch (tuna->id)
    {
        case ETHTOOL_PHY_EDPD:
            switch (*(const u16 *)data)
            {
                case ETHTOOL_PHY_EDPD_DFLT_TX_MSECS:
                    val = 1;
                    break;
                case ETHTOOL_PHY_EDPD_DISABLE:
                    val = 0;
                    break;
                default:
                    return -EINVAL;
            }
            RTK_PHYLIB_ERR_CHK(rtk_phylib_826xb_link_down_power_saving_set(phydev, val));
            return 0;

        default:
            return -EOPNOTSUPP;
    }
}

static int rtl826xb_set_wol(struct phy_device *phydev,
              struct ethtool_wolinfo *wol)
{
    int32 ret = 0;
    uint8 *mac_addr = NULL;
    uint32 rtk_wol_opts = 0;

    struct net_device *ndev = phydev->attached_dev;
    if (!ndev)
        return RTK_PHYLIB_ERR_FAILED;

    if (wol->wolopts & ~( WAKE_PHY | WAKE_MAGIC | WAKE_UCAST | WAKE_BCAST | WAKE_MCAST))
        return -EOPNOTSUPP;

    if (wol->wolopts & (WAKE_MAGIC | WAKE_UCAST))
    {
        mac_addr = (uint8 *) ndev->dev_addr;
        RTK_PHYLIB_ERR_CHK(rtk_phylib_826xb_wol_unicast_addr_set(phydev, mac_addr));
    }

    if (wol->wolopts & WAKE_MCAST)
    {
        RTK_PHYLIB_ERR_CHK(rtk_phylib_826xb_wol_multicast_mask_reset(phydev));

        if (!netdev_mc_empty(ndev))
        {
            struct netdev_hw_addr *ha;
            netdev_for_each_mc_addr(ha, ndev)
            {
                pr_info("[%s,%d] mac: %pM \n", __FUNCTION__, __LINE__, ha->addr);
                RTK_PHYLIB_ERR_CHK(rtk_phylib_826xb_wol_multicast_mask_add(phydev, rtk_phylib_826xb_wol_multicast_mac2offset(ha->addr)));
            }
        }
    }

    if (wol->wolopts & WAKE_PHY)
        rtk_wol_opts |= RTK_WOL_OPT_LINK;
    if (wol->wolopts & WAKE_MAGIC)
        rtk_wol_opts |= RTK_WOL_OPT_MAGIC;
    if (wol->wolopts & WAKE_UCAST)
        rtk_wol_opts |= RTK_WOL_OPT_UCAST;
    if (wol->wolopts & WAKE_BCAST)
        rtk_wol_opts |= RTK_WOL_OPT_BCAST;
    if (wol->wolopts & WAKE_MCAST)
        rtk_wol_opts |= RTK_WOL_OPT_MCAST;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_826xb_wol_set(phydev, rtk_wol_opts));

    return 0;
}


static void rtl826xb_get_wol(struct phy_device *phydev,
               struct ethtool_wolinfo *wol)
{
    int32 ret = 0;
    uint32 rtk_wol_opts = 0;

    wol->supported = WAKE_PHY | WAKE_MAGIC | WAKE_UCAST | WAKE_BCAST | WAKE_MCAST;
    wol->wolopts = 0;

    ret = rtk_phylib_826xb_wol_get(phydev, &rtk_wol_opts);
    if (ret != 0)
        return;

    if (rtk_wol_opts & RTK_WOL_OPT_LINK)
        wol->wolopts |= WAKE_PHY;
    if (rtk_wol_opts & RTK_WOL_OPT_MAGIC)
        wol->wolopts |= WAKE_MAGIC;
    if (rtk_wol_opts & RTK_WOL_OPT_UCAST)
        wol->wolopts |= WAKE_UCAST;
    if (rtk_wol_opts & RTK_WOL_OPT_MCAST)
        wol->wolopts |= WAKE_MCAST;
    if (rtk_wol_opts & RTK_WOL_OPT_BCAST)
        wol->wolopts |= WAKE_BCAST;
}

static struct phy_driver rtk_phy_drivers[] = {
    {
        PHY_ID_MATCH_EXACT(REALTEK_PHY_ID_RTL8261N),
        .name               = "Realtek RTL8261N/8261BE/8251L",
        .get_features       = rtl826xb_get_features,
        .config_init        = rtkphy_config_init,
        .probe              = rtl826xb_probe,
        .suspend            = rtkphy_c45_suspend,
        .resume             = rtkphy_c45_resume,
        .config_aneg        = rtkphy_c45_config_aneg,
        .aneg_done          = rtkphy_c45_aneg_done,
        .read_status        = rtkphy_c45_read_status,
        .set_loopback       = rtkphy_c45_pcs_loopback,
//        .cable_test_start      = rtl826xb_cable_test_start,
//        .cable_test_get_status = rtl826xb_cable_test_get_status,
        .config_intr        = rtl826xb_config_intr,
//        .ack_interrupt      = rtl826xb_ack_intr,
        .handle_interrupt   = rtl826xb_handle_intr,
        .get_tunable        = rtl826xb_get_tunable,
        .set_tunable        = rtl826xb_set_tunable,
        .set_wol            = rtl826xb_set_wol,
        .get_wol            = rtl826xb_get_wol,
    },
    {
        PHY_ID_MATCH_EXACT(REALTEK_PHY_ID_RTL8264B),
        .name               = "Realtek RTL8264B/8254B",
        .get_features       = rtl826xb_get_features,
        .config_init        = rtkphy_config_init,
        .probe              = rtl826xb_probe,
        .suspend            = rtkphy_c45_suspend,
        .resume             = rtkphy_c45_resume,
        .config_aneg        = rtkphy_c45_config_aneg,
        .aneg_done          = rtkphy_c45_aneg_done,
        .read_status        = rtkphy_c45_read_status,
        .set_loopback       = rtkphy_c45_pcs_loopback,
//        .cable_test_start      = rtl826xb_cable_test_start,
//        .cable_test_get_status = rtl826xb_cable_test_get_status,
        .config_intr        = rtl826xb_config_intr,
//        .ack_interrupt      = rtl826xb_ack_intr,
        .handle_interrupt   = rtl826xb_handle_intr,
        .get_tunable        = rtl826xb_get_tunable,
        .set_tunable        = rtl826xb_set_tunable,
        .set_wol            = rtl826xb_set_wol,
        .get_wol            = rtl826xb_get_wol,
    },
};

module_phy_driver(rtk_phy_drivers);


static struct mdio_device_id __maybe_unused rtk_phy_tbl[] = {
    { PHY_ID_MATCH_EXACT(REALTEK_PHY_ID_RTL8261N) },
    { PHY_ID_MATCH_EXACT(REALTEK_PHY_ID_RTL8264B) },
    { },
};

MODULE_DEVICE_TABLE(mdio, rtk_phy_tbl);

MODULE_AUTHOR("Realtek");
MODULE_DESCRIPTION("Realtek PHY drivers");
MODULE_LICENSE("GPL");
