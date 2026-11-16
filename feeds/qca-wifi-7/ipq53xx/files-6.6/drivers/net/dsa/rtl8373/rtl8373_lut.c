// SPDX-License-Identifier: GPL-2.0
#include <linux/mutex.h>
#include <linux/if_bridge.h>
#include <linux/if_vlan.h>
#include <linux/delay.h>
#include <net/dsa.h>
#include <linux/bitops.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>


#include "realtek-smi-rtl8373.h"
#include "rtl8373_core.h"
#include "rtl8373_lut.h"
#include "rtl8373_interrupt.h"
#include "rtl8373_switch.h"

static void _rtl8373_fdbStUser2Smi( rtl8373_luttb *pLutSt, u32 *pFdbSmi)
{
    /* L3 lookup */
    if(pLutSt->l3lookup)
    {
        pFdbSmi[0] = pLutSt->sip;

        pFdbSmi[1] = pLutSt->dip & 0xFFFFFFF;
        pFdbSmi[1] |= (pLutSt->l3lookup & 0x0001) << 28;
        pFdbSmi[1] |= (pLutSt->mbr & 0x0003) << 30;

        pFdbSmi[2] = (pLutSt->mbr >> 2) & 0xFF;
        pFdbSmi[2] |= (pLutSt->igmp_idx & 0xFF) << 8;
        pFdbSmi[2] |= (pLutSt->igmp_asic & 1) << 16;

    }
    else if(pLutSt->mac.octet[0] & 0x01) /*Multicast L2 Lookup*/
    {
        pFdbSmi[0] = pLutSt->mac.octet[5]|(pLutSt->mac.octet[4] << 8)|(pLutSt->mac.octet[3] << 16)|(pLutSt->mac.octet[2] << 24);

        pFdbSmi[1] = pLutSt->mac.octet[1]| (pLutSt->mac.octet[0] << 8);
        pFdbSmi[1] |= (pLutSt->cvid_fid & 0xFFF) << 16;
        pFdbSmi[1] |= (pLutSt->l3lookup & 1) << 28;
        pFdbSmi[1] |= (pLutSt->ivl_svl & 1) << 29;
        pFdbSmi[1] |= (pLutSt->mbr & 0x3) << 30;

        pFdbSmi[2] = (pLutSt->mbr >> 2) & 0xFF;
        pFdbSmi[2] |= (pLutSt->igmp_idx & 0xFF) << 8;
        pFdbSmi[2] |= (pLutSt->igmp_asic & 1) << 16;
    }
    else /*Asic auto-learning*/
    {
        pFdbSmi[0] = pLutSt->mac.octet[5]|(pLutSt->mac.octet[4] << 8)|(pLutSt->mac.octet[3] << 16)|(pLutSt->mac.octet[2] << 24);

        pFdbSmi[1] = pLutSt->mac.octet[1]| (pLutSt->mac.octet[0] << 8);
        pFdbSmi[1] |= (pLutSt->cvid_fid & 0xFFF) << 16;
        pFdbSmi[1] |= (pLutSt->l3lookup & 1) << 28;
        pFdbSmi[1] |= (pLutSt->ivl_svl & 1) << 29;
        pFdbSmi[1] |= (pLutSt->spa & 0x3) << 30;

        pFdbSmi[2] = pLutSt->spa >> 2;
        pFdbSmi[2] |= (pLutSt->age & 7) << 2;
        pFdbSmi[2] |= (pLutSt->auth & 1) << 5;
        pFdbSmi[2] |= (pLutSt->nosalearn & 1) << 16;
        
    }
}


static void _rtl8373_fdbStSmi2User( rtl8373_luttb *pLutSt, u32 *pFdbSmi)
{
    /*L3 lookup*/
    if(pFdbSmi[1] & 0x10000000)
    {
        pLutSt->sip             = pFdbSmi[0];
        pLutSt->dip             = pFdbSmi[1] & 0xFFFFFFF;
        pLutSt->mbr             = ((pFdbSmi[2] & 0x00FF) << 2) | ((pFdbSmi[1] >> 30) & 3);
        pLutSt->l3lookup        = (pFdbSmi[1] >> 28) & 1;
        pLutSt->igmp_asic        = (pFdbSmi[2] >> 16) & 1;
        pLutSt->igmp_idx        = (pFdbSmi[2] >> 8) & 0xff;
    }
    else if((pFdbSmi[1] >> 8) & 0x01) /*Multicast L2 Lookup*/
    {
        pLutSt->mac.octet[5]    = pFdbSmi[0] & 0xFF;
        pLutSt->mac.octet[4]    = (pFdbSmi[0] & 0xFF00) >> 8;
        pLutSt->mac.octet[3]    = (pFdbSmi[0] & 0xFF0000) >> 16;
        pLutSt->mac.octet[2]    = (pFdbSmi[0] & 0xFF000000) >> 24;
        pLutSt->mac.octet[1]    = (pFdbSmi[1] & 0xFF);
        pLutSt->mac.octet[0]    = (pFdbSmi[1] & 0xFF00) >> 8;

        pLutSt->cvid_fid        = (pFdbSmi[1] >> 16) & 0x0FFF;
        pLutSt->l3lookup        = (pFdbSmi[1] >> 28) & 1;
        pLutSt->ivl_svl            = (pFdbSmi[1] >> 29) & 1;
        pLutSt->mbr             = ((pFdbSmi[2] & 0x00FF) << 2) | ((pFdbSmi[1] >> 30) & 3);
        pLutSt->igmp_asic        = (pFdbSmi[2] >> 16) & 1;
        pLutSt->igmp_idx        = (pFdbSmi[2] >> 8) & 0xff;

    }
    else /*Asic auto-learning*/
    {
        pLutSt->mac.octet[5]    = pFdbSmi[0] & 0xFF;
        pLutSt->mac.octet[4]    = (pFdbSmi[0] & 0xFF00) >> 8;
        pLutSt->mac.octet[3]    = (pFdbSmi[0] & 0xFF0000) >> 16;
        pLutSt->mac.octet[2]    = (pFdbSmi[0] & 0xFF000000) >> 24;
        pLutSt->mac.octet[1]    = (pFdbSmi[1] & 0xFF);
        pLutSt->mac.octet[0]    = (pFdbSmi[1] & 0xFF00) >> 8;

        pLutSt->cvid_fid        = (pFdbSmi[1] >> 16) & 0x0FFF;
        pLutSt->l3lookup        = (pFdbSmi[1] >> 28) & 1;
        pLutSt->ivl_svl            = (pFdbSmi[1] >> 29) & 1;
        pLutSt->spa                = ((pFdbSmi[2] & 0x3) << 2) | ((pFdbSmi[1] >> 30) & 3);
        pLutSt->age                = ((pFdbSmi[2] >> 2) & 0x7);
        pLutSt->auth            = ((pFdbSmi[2] >> 5) & 0x1);

        pLutSt->nosalearn       = (pFdbSmi[2] >> 16) & 1;
    }
}
static int _rtl8373_getL2LookupTb(struct realtek_smi *priv, u32 method, rtl8373_luttb *pL2Table)
{
    int ret;
    u32 regData;
    u32* accessPtr;
    u32 i;
    u32 smil2Table[RTL8373_LUT_TABLE_SIZE];
    u32 busyCounter;
    u32 tblCmd;

    if(pL2Table->wait_time == 0)
        busyCounter = RTL8373_LUT_BUSY_CHECK_NO;
    else
        busyCounter = pL2Table->wait_time;

    while(busyCounter)
    {
        ret = rtl8373_getAsicRegBit(priv->map, RTL8373_ITA_CTRL0_ADDR, RTL8373_ITA_CTRL0_TLB_EXECUTE_OFFSET,&regData);
        if(ret)
            return ret;

        pL2Table->lookup_busy = regData;
        if(!pL2Table->lookup_busy)
            break;

        busyCounter --;
        if(busyCounter == 0)
            return RT_ERR_BUSYWAIT_TIMEOUT;
    }

    ret = rtl8373_setAsicRegBits(priv->map,RTL8373_ITA_L2_CTRL_ADDR, RTL8373_ITA_L2_CTRL_READ_MTHD_MASK, method);
    if(ret)
        return ret;

    switch(method)
    {
        case RTL8373_LUTREADMETHOD_ADDRESS:
        case RTL8373_LUTREADMETHOD_NEXT_ADDRESS:
        case RTL8373_LUTREADMETHOD_NEXT_L2UC:
        case RTL8373_LUTREADMETHOD_NEXT_L2MC:
        case RTL8373_LUTREADMETHOD_NEXT_L3MC:
        case RTL8373_LUTREADMETHOD_NEXT_L2L3MC:

            tblCmd = (1|(0 << 1)|(4 << 8) | (pL2Table->address << RTL8373_ITA_CTRL0_TBL_ADDR_OFFSET));
            ret = rtl8373_setAsicReg(priv->map,RTL8373_ITA_CTRL0_ADDR, tblCmd);
            if(ret)
                return ret;
            break;
        case RTL8373_LUTREADMETHOD_MAC:
            memset(smil2Table, 0x00, sizeof(u32) * RTL8373_LUT_TABLE_SIZE);
            _rtl8373_fdbStUser2Smi(pL2Table, smil2Table);

            accessPtr = smil2Table;
            regData = *accessPtr;

            for(i=0; i<RTL8373_LUT_TABLE_SIZE; i++)
            {
                ret = rtl8373_setAsicReg(priv->map,RTL8373_ITA_WRITE_DATA0_ADDR(i), regData);

                if(ret)
                    return ret;

                accessPtr ++;
                regData = *accessPtr;

            }

            tblCmd = (1|(0 << 1)|(4 << 8));
            ret = rtl8373_setAsicReg(priv->map,RTL8373_ITA_CTRL0_ADDR, tblCmd);
            
            break;
        case RTL8373_LUTREADMETHOD_NEXT_L2UCSPA:
            ret = rtl8373_setAsicRegBits(priv->map,RTL8373_ITA_CTRL0_ADDR, RTL8373_ITA_CTRL0_TBL_ADDR_MASK, pL2Table->address);
            if(ret)
                return ret;

            ret = rtl8373_setAsicRegBits(priv->map,RTL8373_ITA_L2_CTRL_ADDR, RTL8373_ITA_L2_CTRL_PORT_NUM_MASK, pL2Table->spa);
            if(ret)
                return ret;

            tblCmd = (1|(0 << 1)|(4 << 8) | (pL2Table->address << RTL8373_ITA_CTRL0_TBL_ADDR_OFFSET));
            ret = rtl8373_setAsicReg(priv->map,RTL8373_ITA_CTRL0_ADDR, tblCmd);
            if(ret)
                return ret;
            break;
        default:
            return RT_ERR_INPUT;
    }

    if(pL2Table->wait_time == 0)
        busyCounter = RTL8373_LUT_BUSY_CHECK_NO;
    else
        busyCounter = pL2Table->wait_time;

    while(busyCounter)
    {
        ret = rtl8373_getAsicRegBit(priv->map,RTL8373_ITA_CTRL0_ADDR, RTL8373_ITA_CTRL0_TLB_EXECUTE_OFFSET,&regData);
        if(ret)
            return ret;

        pL2Table->lookup_busy = regData;
        if(!pL2Table->lookup_busy)
            break;

        busyCounter --;
        if(busyCounter == 0)
            return RT_ERR_BUSYWAIT_TIMEOUT;
    }

    ret = rtl8373_getAsicRegBit(priv->map,RTL8373_ITA_L2_CTRL_ADDR, RTL8373_ITA_L2_CTRL_ACT_STS_OFFSET,&regData);
    if(ret)
            return ret;
    pL2Table->lookup_hit = regData;
    if(!pL2Table->lookup_hit)
        return RT_ERR_L2_ENTRY_NOTFOUND;

    /*Read access address*/
    ret = rtl8373_getAsicRegBits(priv->map,RTL8373_ITA_L2_CTRL_ADDR, RTL8373_ITA_L2_CTRL_TBL_ADDR_MASK, &regData);
    if(ret)
        return ret;

    pL2Table->address = (regData & 0xffff);

    ret = rtl8373_getAsicReg(priv->map,RTL8373_ITA_L2_CTRL_ADDR, &regData);

    /*read L2 entry */
    memset(smil2Table, 0x00, sizeof(u32) * RTL8373_LUT_TABLE_SIZE);

    accessPtr = smil2Table;

    for(i = 0; i < RTL8373_LUT_TABLE_SIZE; i++)
    {
        ret = rtl8373_getAsicReg(priv->map,RTL8373_ITA_READ_DATA0_ADDR(i), &regData);
        if(ret)
            return ret;

        *accessPtr = regData;

        accessPtr ++;
    }

    _rtl8373_fdbStSmi2User(pL2Table, smil2Table);

    return 0;
}


static int _rtl8373_setL2LookupTb(struct realtek_smi *priv, rtl8373_luttb *pL2Table)
{
    int ret;
    u32 regData;
    u32 *accessPtr;
    u32 i;
    u32 smil2Table[RTL8373_LUT_TABLE_SIZE];
    u32 tblCmd;
    u32 busyCounter;
    memset(smil2Table, 0x00, sizeof(u16) * RTL8373_LUT_TABLE_SIZE);
    _rtl8373_fdbStUser2Smi(pL2Table, smil2Table);

    if(pL2Table->wait_time == 0)
        busyCounter = RTL8373_LUT_BUSY_CHECK_NO;
    else
        busyCounter = pL2Table->wait_time;

    while(busyCounter)
    {
        ret = rtl8373_getAsicRegBit(priv->map,RTL8373_ITA_CTRL0_ADDR, RTL8373_ITA_CTRL0_TLB_EXECUTE_OFFSET,&regData);
        if(ret)
            return ret;

        pL2Table->lookup_busy = regData;
        if(!regData)
            break;

        busyCounter --;
        if(busyCounter == 0)
            return RT_ERR_BUSYWAIT_TIMEOUT;
    }

    accessPtr = smil2Table;

    for(i = 0; i < RTL8373_LUT_TABLE_SIZE; i++)
    {
        regData = *(accessPtr + i);
        ret = rtl8373_setAsicReg(priv->map,RTL8373_ITA_WRITE_DATA0_ADDR(i), regData);

        if(ret)
            return ret;
    }

    tblCmd = (1<<1) | (4 << 8)  | 1;
    /* Write Command */

    ret = rtl8373_setAsicReg(priv->map,RTL8373_ITA_CTRL0_ADDR, tblCmd);
    if(ret)
        return ret;

    if(pL2Table->wait_time == 0)
        busyCounter = RTL8373_LUT_BUSY_CHECK_NO;
    else
        busyCounter = pL2Table->wait_time;

    while(busyCounter)
    {
        ret = rtl8373_getAsicRegBit(priv->map,RTL8373_ITA_CTRL0_ADDR, RTL8373_ITA_CTRL0_TLB_EXECUTE_OFFSET,&regData);
        if(ret)
            return ret;

        pL2Table->lookup_busy = regData;
        if(!regData)
            break;

        busyCounter --;
        if(busyCounter == 0)
            return RT_ERR_BUSYWAIT_TIMEOUT;
    }

    /*Read access status*/
    ret = rtl8373_getAsicRegBit(priv->map,RTL8373_ITA_L2_CTRL_ADDR, RTL8373_ITA_L2_CTRL_ACT_STS_OFFSET, &regData);
    if(ret)
        return ret;

    pL2Table->lookup_hit = regData;
    if(!pL2Table->lookup_hit)
        return RT_ERR_FAILED;

    /*Read access address*/
    ret = rtl8373_getAsicRegBits(priv->map,RTL8373_ITA_L2_CTRL_ADDR, RTL8373_ITA_L2_CTRL_TBL_ADDR_MASK, &regData);
    if(ret)
        return ret;

    pL2Table->address = (regData & 0xffff);
    pL2Table->lookup_busy = 0;

    return 0;
}


static int _rtl8373_clearL2LookupTb(struct realtek_smi *priv, u32 index)
{

    int ret;
    u32 regData;
    u32 tblCmd;
    u32 busyCounter;
    //memset(smil2Table, 0x00, sizeof(u16) * RTL8373_LUT_TABLE_SIZE);

    busyCounter = RTL8373_LUT_BUSY_CHECK_NO;

    while(busyCounter)
    {
        ret = rtl8373_getAsicRegBit(priv->map,RTL8373_ITA_CTRL0_ADDR, RTL8373_ITA_CTRL0_TLB_EXECUTE_OFFSET,&regData);
        if(ret)
            return ret;

        if(!regData)
            break;

        busyCounter --;
        if(busyCounter == 0)
            return RT_ERR_BUSYWAIT_TIMEOUT;
    }
    
    ret = rtl8373_setAsicRegBit(priv->map,RTL8373_ITA_L2_CTRL_ADDR, RTL8373_ITA_L2_CTRL_ENTRY_CLR_OFFSET, 1);
    if(ret)
        return ret;


    tblCmd = (index << 16)|(1<<1) | (4 << 8)  | 1;
    /* Write Command */
    ret = rtl8373_setAsicReg(priv->map, RTL8373_ITA_CTRL0_ADDR, tblCmd);
    if(ret)
        return ret;

    busyCounter = RTL8373_LUT_BUSY_CHECK_NO;

    while(busyCounter)
    {
        ret = rtl8373_getAsicRegBit(priv->map, RTL8373_ITA_CTRL0_ADDR, RTL8373_ITA_CTRL0_TLB_EXECUTE_OFFSET,&regData);
        if(ret)
            return ret;

        if(!regData)
            break;

        busyCounter --;
        if(busyCounter == 0)
            return RT_ERR_BUSYWAIT_TIMEOUT;
    }

#if 1
    ret = rtl8373_setAsicRegBit(priv->map, RTL8373_ITA_L2_CTRL_ADDR, RTL8373_ITA_L2_CTRL_ENTRY_CLR_OFFSET, 0);
    if(ret)
        return ret;
#endif 
    /*Read access status*/
    ret = rtl8373_getAsicRegBit(priv->map, RTL8373_ITA_L2_CTRL_ADDR, RTL8373_ITA_L2_CTRL_ACT_STS_OFFSET, &regData);
    if(ret)
        return ret;


    return 0;
}

int
rtl8373_port_fdb_add(struct dsa_switch *ds, int port,
		       const unsigned char *addr, u16 vid,
		       struct dsa_db db)
{
	struct realtek_smi *priv = ds->priv;
    rtl8373_luttb l2Table;
	int ret = RT_ERR_OK;

    mutex_lock(&priv->map_lock);
 /* fill key (MAC,FID) to get L2 entry */
    memcpy(l2Table.mac.octet, addr, ETHER_ADDR_LEN);
    l2Table.ivl_svl     = 1;
    l2Table.cvid_fid    = vid;
    ret = _rtl8373_getL2LookupTb(priv, RTL8373_LUTREADMETHOD_MAC, &l2Table);
    if (RT_ERR_OK == ret )
    {
        memcpy(l2Table.mac.octet, addr, ETHER_ADDR_LEN);
        l2Table.ivl_svl     = 1;
        l2Table.cvid_fid    = vid;
        l2Table.spa         = priv->ops->port_l2p_get(priv, port);
        l2Table.nosalearn   = 1;
        l2Table.l3lookup    = 0;
        l2Table.age         = 6;
        l2Table.auth        = 1;
        if((ret = _rtl8373_setL2LookupTb(priv, &l2Table)) != RT_ERR_OK)
            goto out;

        ret = RT_ERR_OK;
        goto out;
    }
    else if (RT_ERR_L2_ENTRY_NOTFOUND == ret )
    {
        memset(&l2Table, 0, sizeof(rtl8373_luttb));
        memcpy(l2Table.mac.octet, addr, ETHER_ADDR_LEN);
        l2Table.ivl_svl     = 1;
        l2Table.cvid_fid    = vid;
        l2Table.spa         = priv->ops->port_l2p_get(priv, port);
        l2Table.nosalearn   = 1;
        l2Table.l3lookup    = 0;
        l2Table.age         = 6;
        l2Table.auth        = 1;

        if ((ret = _rtl8373_setL2LookupTb(priv, &l2Table)) != RT_ERR_OK)
            return ret;

        ret = _rtl8373_getL2LookupTb(priv, RTL8373_LUTREADMETHOD_MAC, &l2Table);
            goto out;
    }
    else
    {
        goto out;
    }
out:
    mutex_unlock(&priv->map_lock);
    return ret;
    
}

EXPORT_SYMBOL_NS_GPL(rtl8373_port_fdb_add, REALTEK_DSA);

int
rtl8373_port_fdb_del(struct dsa_switch *ds, int port,
		       const unsigned char *addr, u16 vid,
		       struct dsa_db db)
{
	struct realtek_smi *priv = ds->priv;
    rtl8373_luttb l2Table;
	int ret = RT_ERR_OK;
    mutex_lock(&priv->map_lock);
 /* fill key (MAC,FID) to get L2 entry */
    memcpy(l2Table.mac.octet, addr, ETHER_ADDR_LEN);
    l2Table.ivl_svl     = 1;
    l2Table.cvid_fid    = vid;
    ret = _rtl8373_getL2LookupTb(priv, RTL8373_LUTREADMETHOD_MAC, &l2Table);
    if (RT_ERR_OK == ret )
    {
        memcpy(l2Table.mac.octet, addr, ETHER_ADDR_LEN);
        l2Table.ivl_svl     = 1;
        l2Table.cvid_fid    = vid;
        l2Table.spa         = 0;
        l2Table.nosalearn   = 0;
        l2Table.l3lookup    = 0;
        l2Table.age         = 0;
        l2Table.auth        = 0;
        if((ret = _rtl8373_setL2LookupTb(priv, &l2Table)) != RT_ERR_OK)
             goto out;

        ret = RT_ERR_OK;
         goto out;
    }  
    else
        goto out;

    out:
    mutex_unlock(&priv->map_lock);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(rtl8373_port_fdb_del, REALTEK_DSA);

int rtl8373_port_fdb_dump(struct dsa_switch *ds, int port,
                          dsa_fdb_dump_cb_t *cb, void *data)
{
    struct realtek_smi *priv = ds->priv;
    rtl8373_luttb l2Table;
    rtk_mac_cnt_t mac_cnt;
    rtk_api_ret_t ret_api;
    u16 cnt = 4096;          // Maximum LUT entries
    u16 pAddress = 0;        // Start LUT address
    u16 last_address = 0xffff; // Invalid address for progress check
    int ret = 0;

    mutex_lock(&priv->map_lock);
    // Step 1: Check port's learned MAC count before dumping
    ret_api = dal_rtl8373_l2_learningCnt_get(priv, priv->ops->port_l2p_get(priv, port), &mac_cnt);
    if ((ret_api != RT_ERR_OK) || (mac_cnt == 0)) {
        // No learned MAC for this port, no need to dump FDB
        mutex_unlock(&priv->map_lock);
        return 0;
    }

    // Step 2: Iteratively dump each FDB entry belonging to this port
    while (cnt--) {
        // Prepare lookup table for next hardware read
        memset(&l2Table, 0, sizeof(rtl8373_luttb));
        l2Table.spa = priv->ops->port_l2p_get(priv, port); // Translate logical port to physical
        l2Table.address = pAddress;

        ret_api = _rtl8373_getL2LookupTb(priv, RTL8373_LUTREADMETHOD_NEXT_L2UCSPA, &l2Table);
        if (ret_api != RT_ERR_OK)
            break;

        // Exit condition if hardware didn't advance the address
        if (l2Table.address == last_address)
            break;
        last_address = l2Table.address;
       
        // Pass valid entry to callback function
        ret = cb(l2Table.mac.octet, l2Table.cvid_fid, l2Table.nosalearn, data);
        if (ret < 0)
            break;
        
        // Prepare LUT address for next round
        pAddress = l2Table.address + 1;
    }

    mutex_unlock(&priv->map_lock);
    return ret;
}

EXPORT_SYMBOL_NS_GPL(rtl8373_port_fdb_dump, REALTEK_DSA);


int rtl8373_port_mdb_add(struct dsa_switch *ds, int port,
		       const struct switchdev_obj_port_mdb *mdb,
		       struct dsa_db db)
{
    struct realtek_smi *priv = ds->priv;
    rtl8373_luttb  l2Table;
    int ret = RT_ERR_OK;

    mutex_lock(&priv->map_lock);
    memset(&l2Table, 0, sizeof(rtl8373_luttb));
    /* fill key (MAC,FID) to get L2 entry */
    memcpy(l2Table.mac.octet, mdb->addr, ETHER_ADDR_LEN);
    l2Table.ivl_svl     = 1;
    l2Table.cvid_fid    = mdb->vid;

    ret = _rtl8373_getL2LookupTb(priv, RTL8373_LUTREADMETHOD_MAC, &l2Table);
    if (RT_ERR_OK == ret)
    {
        memcpy(l2Table.mac.octet, mdb->addr, ETHER_ADDR_LEN);
        l2Table.ivl_svl     = 1;
        l2Table.cvid_fid    = mdb->vid;
        l2Table.mbr         |= 1 << priv->ops->port_l2p_get(priv, port);
         ret = _rtl8373_setL2LookupTb(priv, &l2Table);

    }
    else if (RT_ERR_L2_ENTRY_NOTFOUND == ret)
    {
        memset(&l2Table, 0, sizeof(rtl8373_luttb));
        memcpy(l2Table.mac.octet, mdb->addr, ETHER_ADDR_LEN);
        l2Table.ivl_svl     = 1;
        l2Table.cvid_fid    = mdb->vid;
        l2Table.mbr         = 1 << priv->ops->port_l2p_get(priv, port);;
         ret = _rtl8373_setL2LookupTb(priv, &l2Table);
    }
    mutex_unlock(&priv->map_lock);
    return ret;
}
EXPORT_SYMBOL_NS_GPL(rtl8373_port_mdb_add, REALTEK_DSA);

int rtl8373_port_mdb_del(struct dsa_switch *ds, int port,
		       const struct switchdev_obj_port_mdb *mdb,
		       struct dsa_db db)
{
    struct realtek_smi *priv = ds->priv;
    rtl8373_luttb  l2Table;
    int ret;
    mutex_lock(&priv->map_lock);
    memset(&l2Table, 0, sizeof(rtl8373_luttb));

    /* fill key (MAC,FID) to get L2 entry */
    memcpy(l2Table.mac.octet, mdb->addr, ETHER_ADDR_LEN);
        l2Table.ivl_svl     = 1;
        l2Table.cvid_fid    = mdb->vid;


    ret = _rtl8373_getL2LookupTb(priv, RTL8373_LUTREADMETHOD_MAC, &l2Table);
    if (RT_ERR_OK == ret)
    {
        ret = _rtl8373_clearL2LookupTb(priv, l2Table.address);
    }

    mutex_unlock(&priv->map_lock);
    return ret;
}
EXPORT_SYMBOL_NS_GPL(rtl8373_port_mdb_del, REALTEK_DSA);

/* Function Name:
 *      dal_rtl8373_l2_limitLearningCnt_set
 * Description:
 *      Set per-Port auto learning limit number
 * Input:
 *      port    - Port id.
 *      mac_cnt - Auto learning entries limit number
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK                   - OK
 *      RT_ERR_FAILED               - Failed
 *      RT_ERR_SMI                  - SMI access error
 *      RT_ERR_PORT_ID              - Invalid port number.
 *      RT_ERR_LIMITED_L2ENTRY_NUM  - Invalid auto learning limit number
 * Note:
 *      The API can set per-port ASIC auto learning limit number from 0(disable learning)
 *      to 2112.
 */
rtk_api_ret_t dal_rtl8373_l2_limitLearningCnt_set(struct realtek_smi *smi, rtk_port_t port, rtk_mac_cnt_t mac_cnt)
{
    rtk_api_ret_t retVal;

    if (mac_cnt > 4096)
        return RT_ERR_LIMITED_L2ENTRY_NUM;
    port = smi->ops->port_l2p_get(smi, port);
    if ((retVal = rtl8373_setAsicReg(smi->map, RTL8373_L2_LRN_PORT_CONSTRT_CTRL_ADDR(port), mac_cnt)) != RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_rtl8373_l2_limitLearningCnt_get
 * Description:
 *      Get per-Port auto learning limit number
 * Input:
 *      port - Port id.
 * Output:
 *      pMac_cnt - Auto learning entries limit number
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_PORT_ID      - Invalid port number.
 * Note:
 *      The API can get per-port ASIC auto learning limit number.
 */
rtk_api_ret_t dal_rtl8373_l2_limitLearningCnt_get(struct realtek_smi *smi, rtk_port_t port, rtk_mac_cnt_t *pMac_cnt)
{
    rtk_api_ret_t retVal;

    if(NULL == pMac_cnt)
        return RT_ERR_NULL_POINTER;

    if ((retVal = rtl8373_getAsicReg(smi->map, RTL8373_L2_LRN_PORT_CONSTRT_CTRL_ADDR(port), pMac_cnt)) != RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_rtl8373_l2_limitSystemLearningCnt_set
 * Description:
 *      Set System auto learning limit number
 * Input:
 *      mac_cnt - Auto learning entries limit number
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK                   - OK
 *      RT_ERR_FAILED               - Failed
 *      RT_ERR_SMI                  - SMI access error
 *      RT_ERR_LIMITED_L2ENTRY_NUM  - Invalid auto learning limit number
 * Note:
 *      The API can set system ASIC auto learning limit number from 0(disable learning)
 *      to 2112.
 */
rtk_api_ret_t dal_rtl8373_l2_limitSystemLearningCnt_set(struct realtek_smi *smi, rtk_mac_cnt_t mac_cnt)
{
    rtk_api_ret_t retVal;    
    
    if (mac_cnt > (4096 + 64))
        return RT_ERR_LIMITED_L2ENTRY_NUM;

    if ((retVal = rtl8373_setAsicRegBits(smi->map, RTL8373_L2_LRN_CONSTRT_CTRL_ADDR, RTL8373_L2_LRN_CONSTRT_CTRL_CONSTRT_NUM_MASK, mac_cnt)) != RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_rtl8373_l2_limitSystemLearningCnt_get
 * Description:
 *      Get System auto learning limit number
 * Input:
 *      None
 * Output:
 *      pMac_cnt - Auto learning entries limit number
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_PORT_ID      - Invalid port number.
 * Note:
 *      The API can get system ASIC auto learning limit number.
 */
rtk_api_ret_t dal_rtl8373_l2_limitSystemLearningCnt_get(struct realtek_smi *smi, rtk_mac_cnt_t *pMac_cnt)
{
    rtk_api_ret_t retVal;    
    if(NULL == pMac_cnt)
        return RT_ERR_NULL_POINTER;

    if ((retVal = rtl8373_getAsicRegBits(smi->map, RTL8373_L2_LRN_CONSTRT_CTRL_ADDR, RTL8373_L2_LRN_CONSTRT_CTRL_CONSTRT_NUM_MASK, pMac_cnt)) != RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_rtl8373_l2_limitLearningCntAction_set
 * Description:
 *      Configure auto learn over limit number action.
 * Input:
 *      port - Port id.
 *      action - Auto learning entries limit number
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_PORT_ID      - Invalid port number.
 *      RT_ERR_NOT_ALLOWED  - Invalid learn over action
 * Note:
 *      The API can set SA unknown packet action while auto learn limit number is over
 *      The action symbol as following:
 *      - LIMIT_LEARN_CNT_ACTION_DROP,
 *      - LIMIT_LEARN_CNT_ACTION_FORWARD,
 *      - LIMIT_LEARN_CNT_ACTION_TO_CPU,
 */
rtk_api_ret_t dal_rtl8373_l2_limitLearningCntAction_set( struct realtek_smi *smi, rtk_l2_limitLearnCntAction_t action)
{
    rtk_api_ret_t retVal;
    u32 data;    
    if ( LIMIT_LEARN_CNT_ACTION_DROP == action )
        data = 1;
    else if ( LIMIT_LEARN_CNT_ACTION_FORWARD == action )
        data = 0;
    else if ( LIMIT_LEARN_CNT_ACTION_TO_CPU == action )
        data = 2;
    else
        return RT_ERR_NOT_ALLOWED;

    if ((retVal = rtl8373_setAsicRegBits(smi->map, RTL8373_L2_LRN_PORT_CONSTRT_ACT_ADDR, RTL8373_L2_LRN_PORT_CONSTRT_ACT_LRN_ACT_MASK, data)) != RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_rtl8373_l2_limitLearningCntAction_get
 * Description:
 *      Get auto learn over limit number action.
 * Input:
 *      port - Port id.
 * Output:
 *      pAction - Learn over action
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_PORT_ID      - Invalid port number.
 * Note:
 *      The API can get SA unknown packet action while auto learn limit number is over
 *      The action symbol as following:
 *      - LIMIT_LEARN_CNT_ACTION_DROP,
 *      - LIMIT_LEARN_CNT_ACTION_FORWARD,
 *      - LIMIT_LEARN_CNT_ACTION_TO_CPU,
 */
rtk_api_ret_t dal_rtl8373_l2_limitLearningCntAction_get(struct realtek_smi *smi, rtk_l2_limitLearnCntAction_t *pAction)
{
    rtk_api_ret_t retVal;
    u32 action;    
    if(NULL == pAction)
        return RT_ERR_NULL_POINTER;

    if ((retVal = rtl8373_getAsicRegBits(smi->map, RTL8373_L2_LRN_PORT_CONSTRT_ACT_ADDR, RTL8373_L2_LRN_PORT_CONSTRT_ACT_LRN_ACT_MASK, &action)) != RT_ERR_OK)
        return retVal;

    if ( 1 == action )
        *pAction = LIMIT_LEARN_CNT_ACTION_DROP;
    else if ( 0 == action )
        *pAction = LIMIT_LEARN_CNT_ACTION_FORWARD;
    else if ( 2 == action )
        *pAction = LIMIT_LEARN_CNT_ACTION_TO_CPU;
    else
    *pAction = action;

    return RT_ERR_OK;
}/* Function Name:
 *      dal_rtl8373_l2_limitSystemLearningCntAction_set
 * Description:
 *      Configure system auto learn over limit number action.
 * Input:
 *      port - Port id.
 *      action - Auto learning entries limit number
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_PORT_ID      - Invalid port number.
 *      RT_ERR_NOT_ALLOWED  - Invalid learn over action
 * Note:
 *      The API can set SA unknown packet action while auto learn limit number is over
 *      The action symbol as following:
 *      - LIMIT_LEARN_CNT_ACTION_DROP,
 *      - LIMIT_LEARN_CNT_ACTION_FORWARD,
 *      - LIMIT_LEARN_CNT_ACTION_TO_CPU,
 */
rtk_api_ret_t dal_rtl8373_l2_limitSystemLearningCntAction_set(struct realtek_smi *smi, rtk_l2_limitLearnCntAction_t action)
{
    rtk_api_ret_t retVal;
    u32 data;    
    if ( LIMIT_LEARN_CNT_ACTION_DROP == action )
        data = 1;
    else if ( LIMIT_LEARN_CNT_ACTION_FORWARD == action )
        data = 0;
    else if ( LIMIT_LEARN_CNT_ACTION_TO_CPU == action )
        data = 2;
    else
        return RT_ERR_NOT_ALLOWED;

    if ((retVal = rtl8373_setAsicRegBits(smi->map, RTL8373_L2_LRN_CONSTRT_ACT_ADDR, RTL8373_L2_LRN_CONSTRT_ACT_LRN_ACT_MASK, data)) != RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_rtl8373_l2_limitSystemLearningCntAction_get
 * Description:
 *      Get system auto learn over limit number action.
 * Input:
 *      None.
 * Output:
 *      pAction - Learn over action
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_PORT_ID      - Invalid port number.
 * Note:
 *      The API can get SA unknown packet action while auto learn limit number is over
 *      The action symbol as following:
 *      - LIMIT_LEARN_CNT_ACTION_DROP,
 *      - LIMIT_LEARN_CNT_ACTION_FORWARD,
 *      - LIMIT_LEARN_CNT_ACTION_TO_CPU,
 */
rtk_api_ret_t dal_rtl8373_l2_limitSystemLearningCntAction_get(struct realtek_smi *smi, rtk_l2_limitLearnCntAction_t *pAction)
{
    rtk_api_ret_t retVal;
    u32 action;    
    if(NULL == pAction)
        return RT_ERR_NULL_POINTER;

    if ((retVal = rtl8373_getAsicRegBits(smi->map, RTL8373_L2_LRN_CONSTRT_ACT_ADDR, RTL8373_L2_LRN_CONSTRT_ACT_LRN_ACT_MASK, &action)) != RT_ERR_OK)
        return retVal;

    if ( 1 == action )
        *pAction = LIMIT_LEARN_CNT_ACTION_DROP;
    else if ( 0 == action )
        *pAction = LIMIT_LEARN_CNT_ACTION_FORWARD;
    else if ( 2 == action )
        *pAction = LIMIT_LEARN_CNT_ACTION_TO_CPU;
    else
    *pAction = action;

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_rtl8373_l2_limitSystemLearningCntPortMask_set
 * Description:
 *      Configure system auto learn portmask
 * Input:
 *      pPortmask - Port Mask
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_PORT_MASK    - Invalid port mask.
 * Note:
 *
 */
rtk_api_ret_t dal_rtl8373_l2_limitSystemLearningCntPortMask_set(struct realtek_smi *smi, rtk_portmask_t *pPortmask)
{
    rtk_api_ret_t retVal;
    u32 pmask;    
    if(NULL == pPortmask)
        return RT_ERR_NULL_POINTER;

    if ((retVal = rtk_switch_portmask_L2P_get(smi, pPortmask, &pmask)) != RT_ERR_OK)
        return retVal;

    if ((retVal = rtl8373_setAsicRegBits(smi->map, RTL8373_L2_LRN_CONSTRT_CTRL_ADDR, RTL8373_L2_LRN_CONSTRT_CTRL_PORT_MASK_MASK, pmask & 0x3ff)) != RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_rtl8373_l2_limitSystemLearningCntPortMask_get
 * Description:
 *      get system auto learn portmask
 * Input:
 *      None
 * Output:
 *      pPortmask - Port Mask
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_NULL_POINTER - Null pointer.
 * Note:
 *
 */
rtk_api_ret_t dal_rtl8373_l2_limitSystemLearningCntPortMask_get(struct realtek_smi *smi, rtk_portmask_t *pPortmask)
{
    rtk_api_ret_t retVal;
    u32 pmask;    
    if(NULL == pPortmask)
        return RT_ERR_NULL_POINTER;

    if ((retVal = rtl8373_getAsicRegBits(smi->map, RTL8373_L2_LRN_CONSTRT_CTRL_ADDR, RTL8373_L2_LRN_CONSTRT_CTRL_PORT_MASK_MASK, &pmask)) != RT_ERR_OK)
        return retVal;

    if ((retVal = rtk_switch_portmask_P2L_get(smi, pmask, pPortmask)) != RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}


/* Function Name:
 *      dal_rtl8367d_l2_learningCnt_get
 * Description:
 *      Get per-Port current auto learning number
 * Input:
 *      port - Port id.
 * Output:
 *      pMac_cnt - ASIC auto learning entries number
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_PORT_ID      - Invalid port number.
 * Note:
 *      The API can get per-port ASIC auto learning number
 */
rtk_api_ret_t dal_rtl8373_l2_learningCnt_get(struct realtek_smi *smi, rtk_port_t port, rtk_mac_cnt_t *pMac_cnt)
{
    rtk_api_ret_t retVal;

    if(NULL == pMac_cnt)
        return RT_ERR_NULL_POINTER;

    if ((retVal = rtl8373_getAsicReg(smi->map, RTL8373_L2_LRN_PORT_CONSTRT_CNT_ADDR(port), pMac_cnt)) != RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}
MODULE_IMPORT_NS(REALTEK_DSA);