/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp. All rights reserved.
 */

#include "rtk_phylib_rtl826xb.h"

/* Indirect Register Access APIs */
int rtk_phylib_826xb_sds_read(rtk_phydev *phydev, uint32 page, uint32 reg, uint8 msb, uint8 lsb, uint32 *pData)
{
    int32  ret = 0;
    uint32 rData = 0;
    uint32 op = (page & 0x3f) | ((reg & 0x1f) << 6) | (0x8000);
    uint32 i = 0;
    uint32 mask = 0;
    mask = UINT32_BITS_MASK(msb,lsb);

    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 323, 15, 0, op));

    for (i = 0; i < 10; i++)
    {
        RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 30, 323, 15, 15, &rData));
        if (rData == 0)
        {
            break;
        }
        rtk_phylib_udelay(10);
    }
    if (i == 10)
    {
        return -1;
    }

    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 30, 322, 15, 0, &rData));
    *pData = REG32_FIELD_GET(rData, lsb, mask);

    return ret;
}

int rtk_phylib_826xb_sds_write(rtk_phydev *phydev, uint32 page, uint32 reg, uint8 msb, uint8 lsb, uint32 data)
{
    int32  ret = 0;
    uint32 wData = 0, rData = 0;
    uint32 op = (page & 0x3f) | ((reg & 0x1f) << 6) | (0x8800);
    uint32 mask = 0;
    mask = UINT32_BITS_MASK(msb,lsb);

    RTK_PHYLIB_ERR_CHK(rtk_phylib_826xb_sds_read(phydev, page, reg, 15, 0, &rData));

    wData = REG32_FIELD_SET(rData, data, lsb, mask);

    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 321, 15, 0, wData));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 323, 15, 0, op));

    return ret;
}

int rtk_phylib_826xb_indirect_read(rtk_phydev *phydev,  uint32 indr_addr, uint32 *pData)
{
    int32   ret = 0;
    uint32  rData;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xA436, 15, 0, indr_addr));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 31, 0xA438, 15, 0, &rData));
    *pData = rData;

    return ret;
}

int rtk_phylib_826xb_sram_read(rtk_phydev *phydev,  uint32 indr_addr, uint8 msb, uint8 lsb, uint32 *pData)
{
    int32  ret = 0;
    uint32 mask = 0, rData = 0;

    mask = UINT32_BITS_MASK(msb,lsb);
    RTK_PHYLIB_ERR_CHK(rtk_phylib_826xb_indirect_read(phydev, indr_addr, &rData));
    *pData = REG32_FIELD_GET(rData, lsb, mask);
    return ret;
}

/* MACsec */
int rtk_phylib_826xb_macsec_read(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 reg, uint8 msb, uint8 lsb, uint32 *pData)
{
    int32  ret = 0;
    uint32 data_h = 0, data_l = 0;
    uint32 rData = 0;
    uint32 mask = 0;
    uint32 data_e = 0;
    WAIT_COMPLETE_VAR();

    mask = UINT32_BITS_MASK(msb,lsb);

    switch(dir)
    {
        case RTK_MACSEC_DIR_EGRESS:
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x02FB, 15, 0, reg));
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x02FC, 15, 0, 0x10));
            WAIT_COMPLETE(10000000)
            {
                RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 30, 0x02FC, 15, 0, &data_e));
                if ((data_e & 0x10) == 0x0)
                {
                    #ifdef MACSEC_DBG_PRINT
                    if (_t_wait != 0)
                        PR_DBG("[%s-%u] _t_wait: %u\n", __FUNCTION__, dir, _t_wait);
                    #endif
                    break;
                }
            }
            if (WAIT_COMPLETE_IS_TIMEOUT())
            {
                PR_ERR("[%s-%u] timeout!\n", __FUNCTION__, dir);
                return RTK_PHYLIB_ERR_TIMEOUT;
            }
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 30, 0x02F8, 15, 0, &data_h));
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 30, 0x02F9, 15, 0, &data_l));
            break;
        case RTK_MACSEC_DIR_INGRESS:
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xA6EA, 1, 1, 0x1));
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x0300, 15, 0, reg));
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x0301, 15, 0, 0x10));
            WAIT_COMPLETE(10000000)
            {
                RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 30, 0x0301, 15, 0, &data_e));
                if ((data_e & 0x10) == 0x0)
                {
                    #ifdef MACSEC_DBG_PRINT
                    if (_t_wait != 0)
                        PR_DBG("[%s-%u] _t_wait: %u\n", __FUNCTION__, dir, _t_wait);
                    #endif
                    break;
                }
            }
            if (WAIT_COMPLETE_IS_TIMEOUT())
            {
                PR_ERR("[%s-%u] timeout!\n", __FUNCTION__, dir);
                RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xA6EA, 1, 1, 0x0));
                return RTK_PHYLIB_ERR_TIMEOUT;
            }
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 30, 0x02FD, 15, 0, &data_h));
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 30, 0x02FE, 15, 0, &data_l));
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xA6EA, 1, 1, 0x0));
            break;
        default:
            return -1;
    }

    rData = (data_h << 16) + data_l;
    *pData = REG32_FIELD_GET(rData, lsb, mask);
    return ret;
}

int rtk_phylib_826xb_macsec_write(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 reg, uint8 msb, uint8 lsb, uint32 data)
{
    int32 ret = 0;
    uint32 data_l = data & 0xFFFF;
    uint32 data_h = (data >> 16) & 0xFFFF;
    uint32 data_e = 0;
    WAIT_COMPLETE_VAR();

    switch(dir)
    {
        case RTK_MACSEC_DIR_EGRESS:
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x02F8 , 15, 0, data_h));
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x02F9 , 15, 0, data_l));
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x02FB , 15, 0, reg));
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x02FC, 15, 0, 0x1));
            WAIT_COMPLETE(10000000)
            {
                RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 30, 0x02FC, 15, 0, &data_e));
                if ((data_e & 0x1) == 0x0)
                {
                    #ifdef MACSEC_DBG_PRINT
                    if (_t_wait != 0)
                        PR_DBG("[%s-%u] _t_wait: %u\n", __FUNCTION__, dir, _t_wait);
                    #endif
                    break;
                }
            }
            if (WAIT_COMPLETE_IS_TIMEOUT())
            {
                PR_ERR("[%s-%u] timeout!\n", __FUNCTION__, dir);
                return RTK_PHYLIB_ERR_TIMEOUT;
            }
            break;

        case RTK_MACSEC_DIR_INGRESS:
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xA6EA, 1, 1, 0x1));
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x02FD, 15, 0, data_h));
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x02FE, 15, 0, data_l));
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x0300, 15, 0, reg));
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x0301, 15, 0, 0x1));
            WAIT_COMPLETE(10000000)
            {
                RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 30, 0x0301, 15, 0, &data_e));
                if ((data_e & 0x1) == 0x0)
                {
                    #ifdef MACSEC_DBG_PRINT
                    if (_t_wait != 0)
                        PR_DBG("[%s-%u] _t_wait: %u\n", __FUNCTION__, dir, _t_wait);
                    #endif
                    break;
                }
            }
            if (WAIT_COMPLETE_IS_TIMEOUT())
            {
                PR_ERR("[%s-%u] timeout!\n", __FUNCTION__, dir);
                RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xA6EA, 1, 1, 0x0));
                return RTK_PHYLIB_ERR_TIMEOUT;
            }
            RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xA6EA, 1, 1, 0x0));
            break;

        default:
            return RTK_PHYLIB_ERR_INPUT;
    }
    return ret;
}

int rtk_phylib_826xb_macsec_init(rtk_phydev *phydev)
{
    int32 ret = 0;
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x2e0, 1, 0, 0b11));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x2d8, 15, 0, 0x5313));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x2da, 15, 0, 0x0101));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x2dc, 15, 0, 0x0101));

    //MACSEC_RXSYS_CFG4
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x3c6, 7, 0, 0xa));
    //MACSEC_TXLINE_CFG4
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x37b, 7, 0, 0x6));
    //loopback fifo_setting
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x2f7, 15, 0, 0x486c));
    //RA_setting
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x3f1, 15, 0, 0x72));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x3f0, 15, 0, 0x0b0b));
    //RA ifg
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x3ee, 15, 13, 0x2));
    return ret;
}

int rtk_phylib_826xb_macsec_bypass_set(rtk_phydev *phydev, uint32 bypass)
{
    int32 ret = 0;

    if (bypass != 0)
    {
        RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x2d8, 15, 0, 0x5313));
        RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x3f1, 15, 0, 0x72));
        RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x3f0, 15, 0, 0x0b0b));
        RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x398, 2, 0, 0x7));
    }
    else
    {
        RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x2d8, 15, 0, 0x5111));
        RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x3f1, 15, 0, 0xe871));
        RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x3f0, 15, 0, 0x0c0c));
        RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x398, 2, 0, 0x5));
    }
    return ret;
}

int rtk_phylib_826xb_macsec_bypass_get(rtk_phydev *phydev, uint32 *pBypass)
{
    int32 ret = 0;
    uint32 bypass_rx = 0;
    uint32 bypass_tx = 0;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 30, 0x2d8, 9, 9, &bypass_rx));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 30, 0x2d8, 1, 1, &bypass_tx));

    *pBypass = (bypass_rx == 0 && bypass_tx == 0) ? 0 : 1;

    return ret;
}

/* RTCT */
int rtk_phylib_826xb_cable_test_start(rtk_phydev *phydev)
{
    int32  ret = 0;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xa4a0, 10, 10, 1));
    rtk_phylib_mdelay(1000);

    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xa422, 15, 0, 0xF2));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xa422, 0, 0, 1));

    return 0;
}

int rtk_phylib_826xb_cable_test_finished_get(rtk_phydev *phydev, uint32 *finished)
{
    int32  ret = 0;
    uint32 rData = 0;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 31, 0xA422, 15, 15, &rData));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xa4a0, 10, 10, 0));
    *finished = rData;

    return 0;
}

int rtk_phylib_826xb_cable_test_result_get(rtk_phydev *phydev, uint32 pair, rtk_rtct_channel_result_t *result)
{
    int32  ret = 0;
    uint32 cable_factor = 7820;
    uint32 indr_add_ss = 0x8027 + (pair * 0x4);
    uint32 indr_add_lh = 0x8028 + (pair * 0x4);
    uint32 indr_add_ll = 0x8029 + (pair * 0x4);
    uint32 rtct_status = 0;
    uint32 rtct_len_h = 0;
    uint32 rtct_len_l = 0;
    int32 len_cnt = 0;

    if (pair > 3)
        return RTK_PHYLIB_ERR_INPUT;

    rtk_phylib_memset(result, 0x0, sizeof(rtk_rtct_channel_result_t));

    RTK_PHYLIB_ERR_CHK(rtk_phylib_826xb_sram_read(phydev, indr_add_ss, 15, 8, &rtct_status));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_826xb_sram_read(phydev, indr_add_lh, 15, 8, &rtct_len_h));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_826xb_sram_read(phydev, indr_add_ll, 15, 8, &rtct_len_l));

    result->cable_status = RTK_PHYLIB_CABLE_STATUS_NORMAL;
    switch (rtct_status)
    {
        case 0x60: /* normal */
            result->cable_status = RTK_PHYLIB_CABLE_STATUS_NORMAL;
            break;
        case 0x48: /* open */
            result->cable_status |= RTK_PHYLIB_CABLE_STATUS_OPEN;
            break;
        case 0x50: /* short */
            result->cable_status |= RTK_PHYLIB_CABLE_STATUS_SHORT;
            break;
        case 0xC0: /* inter pair short */
            result->cable_status |= RTK_PHYLIB_CABLE_STATUS_INTER_PAIR_SHORT;
            break;
        case 0x42: /* mismatch-open */
            result->cable_status |= RTK_PHYLIB_CABLE_STATUS_MISMATCH;
            result->cable_status |= RTK_PHYLIB_CABLE_STATUS_OPEN;
            break;
        case 0x44: /* mismatch-short */
            result->cable_status |= RTK_PHYLIB_CABLE_STATUS_MISMATCH;
            result->cable_status |= RTK_PHYLIB_CABLE_STATUS_SHORT;
            break;
        default:
            result->cable_status |= RTK_PHYLIB_CABLE_STATUS_INTER_PAIR_SHORT;
            break;
    }

    len_cnt = ((int32)rtct_len_h << 8) + (int32)rtct_len_l - 255;
    if (len_cnt < 0)
        result->length_cm = 0;
    else
        result->length_cm = ((uint32)len_cnt * 10000)/cable_factor;

    return 0;
}

/* Interrupt */
int rtk_phylib_826xb_intr_enable(rtk_phydev *phydev, uint32 en)
{
    int32  ret = 0;
    /* enable normal interrupt IMR_INT_PHY0 */
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0xE1, 0, 0, (en == 0) ? 0x0 : 0x1));

    return ret;
}

int rtk_phylib_826xb_intr_read_clear(rtk_phydev *phydev, uint32 *status)
{
    int32  ret = 0;
    uint32 rData = 0;
    uint32 rStatus = 0;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 31, 0xA43A, 15, 0, &rData));
    if(rData & BIT_1)
        rStatus |= RTK_PHY_INTR_RLFD;
    if(rData & BIT_2)
        rStatus |= RTK_PHY_INTR_NEXT_PAGE_RECV;
    if(rData & BIT_3)
        rStatus |= RTK_PHY_INTR_AN_COMPLETE;
    if(rData & BIT_4)
        rStatus |= RTK_PHY_INTR_LINK_CHANGE;
    if(rData & BIT_9)
        rStatus |= RTK_PHY_INTR_ALDPS_STATE_CHANGE;
    if(rData & BIT_11)
        rStatus |= RTK_PHY_INTR_FATAL_ERROR;
    if(rData & BIT_7)
        rStatus |= RTK_PHY_INTR_WOL;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 30, 0xE2, 15, 0, &rData));
    if(rData & BIT_3)
        rStatus |= RTK_PHY_INTR_TM_LOW;
    if(rData & BIT_4)
        rStatus |= RTK_PHY_INTR_TM_HIGH;
    if(rData & BIT_6)
        rStatus |= RTK_PHY_INTR_MACSEC;
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0xE2, 15, 0, 0xFF));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x2DC, 15, 0, 0xFF));

    *status = rStatus;
    return ret;
}

int rtk_phylib_826xb_intr_init(rtk_phydev *phydev)
{
    int32  ret = 0;
    uint32 status = 0;

    /* Disable all IMR*/
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0xE1, 15, 0, 0));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0xE3, 15, 0, 0));

    /* source */
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0xE4, 15, 0, 0x1));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0xE0, 15, 0, 0x2F));

    /* init common link change */
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xA424,  15,  0, 0x10));
    /* init rlfd */
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xA442, 15, 15, 0x1));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xA448, 7, 7, 0x1));
    /* init tm */
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x1A0, 11, 11, 0x1));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x19D, 11, 11, 0x1));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x1A1, 11, 11, 0x1));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 30, 0x19F, 11, 11, 0x1));
    /* init WOL */
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xA424,  7,  7, 0x1));

    /* clear status */
    RTK_PHYLIB_ERR_CHK(rtk_phylib_826xb_intr_read_clear(phydev, &status));

    return ret;
}

int rtk_phylib_826xb_link_down_power_saving_set(rtk_phydev *phydev, uint32 ena)
{
    int32  ret = 0;
    uint32 data =  (ena > 0) ? 0x1 : 0x0;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xA430, 2, 2, data));
    return ret;
}

int rtk_phylib_826xb_link_down_power_saving_get(rtk_phydev *phydev, uint32 *pEna)
{
    int32  ret = 0;
    uint32 data = 0;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 31, 0xA430, 2, 2, &data));
    *pEna = data;
    return ret;
}

int rtk_phylib_826xb_wol_reset(rtk_phydev *phydev)
{
    int32  ret = 0;
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xD8A2, 15, 15, 0));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xD8A2, 15, 15, 1));
    return ret;
}

int rtk_phylib_826xb_wol_set(rtk_phydev *phydev, uint32 wol_opts)
{
    int32 ret = 0;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xD8A0, 13, 13, (wol_opts & RTK_WOL_OPT_LINK) ? 1 : 0));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xD8A0, 12, 12, (wol_opts & RTK_WOL_OPT_MAGIC) ? 1 : 0));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xD8A0, 10, 10, (wol_opts & RTK_WOL_OPT_UCAST) ? 1 : 0));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xD8A0, 9, 9, (wol_opts & RTK_WOL_OPT_MCAST) ? 1 : 0));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xD8A0, 8, 8, (wol_opts & RTK_WOL_OPT_BCAST) ? 1 : 0));

    return  ret;

}

int rtk_phylib_826xb_wol_get(rtk_phydev *phydev, uint32 *pWol_opts)
{
    int32 ret = 0;
    uint32 data = 0;
    uint32 wol_opts = 0;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 31, 0xD8A0, 13, 13, &data));
    wol_opts |= ((data) ? RTK_WOL_OPT_LINK : 0);
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 31, 0xD8A0, 12, 12, &data));
    wol_opts |= ((data) ? RTK_WOL_OPT_MAGIC : 0);
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 31, 0xD8A0, 10, 10, &data));
    wol_opts |= ((data) ? RTK_WOL_OPT_UCAST : 0);
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 31, 0xD8A0, 9, 9, &data));
    wol_opts |= ((data) ? RTK_WOL_OPT_MCAST : 0);
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 31, 0xD8A0, 8, 8, &data));
    wol_opts |= ((data) ? RTK_WOL_OPT_BCAST : 0);

    *pWol_opts = wol_opts;
    return  ret;
}

int rtk_phylib_826xb_wol_unicast_addr_set(rtk_phydev *phydev, uint8 *mac_addr)
{
    int32 ret = 0;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xD8C0, 15, 0, (mac_addr[1] << 8 | mac_addr[0])));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xD8C2, 15, 0, (mac_addr[3] << 8 | mac_addr[2])));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, 0xD8C4, 15, 0, (mac_addr[5] << 8 | mac_addr[4])));
    return ret;
}

uint32 rtk_phylib_826xb_wol_multicast_mac2offset(uint8 *mac_addr)
{
    uint32 crc = 0xFFFFFFFF;
    uint32 i = 0, j = 0;
    uint32 b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0;

    for (i = 0; i < 6; i++) {
        crc ^= mac_addr[i];
        for (j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    crc = ~crc;

    b5 = ((crc & 0b000001) << 5 );
    b4 = ((crc & 0b000010) << 3 );
    b3 = ((crc & 0b000100) << 1 );
    b2 = (((crc & 0b001000) ? 0 : 1) << 2 );
    b1 = (((crc & 0b010000) ? 0 : 1) << 1 );
    b0 = (((crc & 0b100000) ? 0 : 1) << 0 );

    return (b5 | b4 | b3 | b2 | b1 | b0);
}

int rtk_phylib_826xb_wol_multicast_mask_add(rtk_phydev *phydev, uint32 offset)
{
    const uint32 cfg_reg[4] = {0xD8C6, 0xD8C8, 0xD8CA, 0xD8CC};
    int32 ret = 0;
    uint32 idx = offset/16;
    uint32 multicast_cfg = 0;


    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_read(phydev, 31, cfg_reg[idx], 15, 0, &multicast_cfg));

    multicast_cfg = (multicast_cfg | (0b1 << (offset % 16)));

    RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, cfg_reg[idx], 15, 0, multicast_cfg));
    return ret;
}

int rtk_phylib_826xb_wol_multicast_mask_reset(rtk_phydev *phydev)
{
    const uint32 cfg_reg[4] = {0xD8C6, 0xD8C8, 0xD8CA, 0xD8CC};
    int32 ret = 0;
    uint32 idx = 0;

    for (idx = 0; idx < 4; idx++)
    {
        RTK_PHYLIB_ERR_CHK(rtk_phylib_mmd_write(phydev, 31, cfg_reg[idx], 15, 0, 0));
    }

    return ret;
}
