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
#include <linux/version.h>


#include "realtek-smi-rtl8373.h"
#include "rtl8373_core.h"
#include "rtl8373_lut.h"
#include "rtl8373_interrupt.h"
#include "rtl8373_vlan.h"


int rtl8373_getAsicRegBit(struct regmap *map, u32 reg, u32 bit, u32 *value)
{
    u32 reg_val;
    int ret = regmap_read(map, reg, &reg_val);

	if (ret)
		return ret;

    *value = (reg_val >> bit) & 1;

    return 0;
}

EXPORT_SYMBOL_NS_GPL(rtl8373_getAsicRegBit, REALTEK_DSA);

int rtl8373_getAsicRegBits(struct regmap *map, u32 reg, u32 mask, u32 *value)
{
	u32 reg_val;
	u32 shift;
	int ret;

	if (!mask || !value)
		return -EINVAL;

	shift = __ffs(mask);

	ret = regmap_read(map, reg, &reg_val);
	if (ret)
		return ret;

	*value = (reg_val & mask) >> shift;
	return 0;
}


EXPORT_SYMBOL_NS_GPL(rtl8373_getAsicRegBits, REALTEK_DSA);

int rtl8373_getAsicReg(struct regmap *map, u32 reg, u32 *value)
{
     int ret = regmap_read(map, reg, value);

	if (ret)
		return ret;

    return 0;
}

EXPORT_SYMBOL_NS_GPL(rtl8373_getAsicReg, REALTEK_DSA);

int rtl8373_setAsicReg(struct regmap *map, u32 reg, u32 value)
{
    int ret;
    ret = regmap_write(map, reg, value);
	if (ret)
		return ret;

    return 0;
}

EXPORT_SYMBOL_NS_GPL(rtl8373_setAsicReg, REALTEK_DSA);

int rtl8373_setAsicRegBit(struct regmap *map,u32 reg, u32 bit, u32 value)
{
    u32 mask = 1 << bit;
    return regmap_update_bits(map, reg, mask, value ? mask : 0);
}

EXPORT_SYMBOL_NS_GPL(rtl8373_setAsicRegBit, REALTEK_DSA);

int rtl8373_setAsicRegBits(struct regmap *map, u32 reg, u32 mask, u32 value)
{
    int shift;

    if (!mask)
        return -EINVAL;

    shift = __ffs(mask);
    return regmap_update_bits(map, reg, mask, (value << shift) & mask);
}



EXPORT_SYMBOL_NS_GPL(rtl8373_setAsicRegBits, REALTEK_DSA);


static int _rtl8373_phymask_write(struct realtek_smi *priv, u32 phy_mask,
				     u32 dev_addr, u32 reg_addr, u32 indata);
static int _rtl8373_phy_read(struct realtek_smi *priv, u32 phyid,
				     u32 dev_addr, u32 reg_addr, u32 *indata);

static int _rtl8373_phy_regbits_write(struct realtek_smi *priv, u32 phy_mask,
				       u32 dev_addr, u32 reg_addr, u32 mask, u32 indata);

int rtl8373_phymask_write(struct realtek_smi *priv, u32 phy_mask,
				       u32 dev_addr, u32 reg_addr, u32 indata)
{
    return _rtl8373_phymask_write(priv, phy_mask, dev_addr, reg_addr, indata);
}
EXPORT_SYMBOL_NS_GPL(rtl8373_phymask_write, REALTEK_DSA);

int rtl8373_phy_read(struct realtek_smi *priv, u32 phyid,
				     u32 dev_addr, u32 reg_addr, u32 *indata)
{
    //rtl83xx_lock(priv);
    return _rtl8373_phy_read(priv, phyid,dev_addr, reg_addr, indata);
    //rtl83xx_unlock(priv);
}
EXPORT_SYMBOL_NS_GPL(rtl8373_phy_read, REALTEK_DSA);

static inline int mask_lowest_bit_pos(u32 mask)
{
    int pos = 0;
    if (!mask) return 0;
    while (!(mask & (1U << pos)) && pos < RTL8373_PHYBITLENGTH)
        pos++;
    return pos;
}

int rtl8373_phy_regbits_read(struct realtek_smi *priv, u32 phyid,
                         u32 dev_addr, u32 reg_addr, u32 mask, u32 *pdata)
{
    rtk_api_ret_t retVal;
    u32 regdata;
    u32 bitsShift;

    if( !mask )
        return RT_ERR_INPUT;

    bitsShift = 0;
    while(!(mask & (1 << bitsShift)))
    {
        bitsShift++;
        if(bitsShift >= RTL8373_REGBITLENGTH)
            return RT_ERR_INPUT;
    }

    if ((retVal = _rtl8373_phy_read(priv, phyid,dev_addr,reg_addr, &regdata)) != RT_ERR_OK)
    {
        return retVal;
    }
        
    *pdata = (regdata & mask) >> bitsShift;

     return RT_ERR_OK;
}
EXPORT_SYMBOL_NS_GPL(rtl8373_phy_regbits_read, REALTEK_DSA);

int rtl8373_phy_write(struct realtek_smi *priv, u32 phy_mask,
				     u32 dev_addr, u32 reg_addr, u32 indata)
{
    //rtl83xx_lock(priv);
    // [CBT] The reg input already is bitmask.
    //return _rtl8373_phymask_write(priv, 1<<phyid, dev_addr, reg_addr, indata);
    return _rtl8373_phymask_write(priv, phy_mask, dev_addr, reg_addr, indata);
    //rtl83xx_unlock(priv);
}
EXPORT_SYMBOL_NS_GPL(rtl8373_phy_write, REALTEK_DSA);

int rtl8373_phy_regbits_write(struct realtek_smi *priv, u32 phy_mask,
				       u32 dev_addr, u32 reg_addr, u32 mask, u32 indata)
{
    //rtl83xx_lock(priv);
    return _rtl8373_phy_regbits_write(priv, phy_mask, dev_addr, reg_addr, mask, indata);
    //rtl83xx_unlock(priv);
}

EXPORT_SYMBOL_NS_GPL(rtl8373_phy_regbits_write, REALTEK_DSA);

// [CBT] remove 'static' for fix build error.
//static int rtl8373_phy_poll_busy(struct realtek_smi *priv) {
int rtl8373_phy_poll_busy(struct realtek_smi *priv) {
    int ret;
    unsigned int reg_val;

    ret = regmap_read_poll_timeout(priv->map,
                                   RTL8373_SMI_ACCESS_PHY_CTRL_1_ADDR,
                                   reg_val,
                                   (FIELD_GET(RTL8373_SMI_ACCESS_PHY_CTRL_1_CMD_MASK, reg_val) == 0) &&
                                   (FIELD_GET(RTL8373_SMI_ACCESS_PHY_CTRL_1_FAIL_MASK, reg_val) == 0),
                                   10, 100);

    return ret;
}
EXPORT_SYMBOL_NS_GPL(rtl8373_phy_poll_busy, REALTEK_DSA);
void delay_0_1ms(u32 units)
{
    // 0.1 ms = 100 microseconds
    udelay(units * 100);
}
EXPORT_SYMBOL_NS_GPL(delay_0_1ms, REALTEK_DSA);
#if 0
static int rtl8373_phymask_write_prepare(struct realtek_smi *priv, u32 phy_mask,
				     u32 dev_addr, u32 reg_addr, u16 indata)
{
	u32 val;
	int ret;

	ret = regmap_write(priv->map,
			   RTL8373_SMI_ACCESS_PHY_CTRL_0_ADDR, phy_mask);
	if (ret)
    {
        return ret;
    }

	/* Set OCP prefix */
    ret = regmap_update_bits(priv->map, RTL8373_SMI_ACCESS_PHY_CTRL_3_ADDR,
                             RTL8373_SMI_ACCESS_PHY_CTRL_3_INDATA_15_0_MASK,
                             FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_3_INDATA_15_0_MASK, indata));
	if (ret)
    {
        return ret;
    }

	val = 0;
    val = FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_MMD_DEVAD_4_0_MASK, dev_addr);
	val |= FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_MMD_REG_15_0_MASK, reg_addr);
    ret = regmap_write(priv->map, RTL8373_SMI_ACCESS_PHY_CTRL_1_ADDR, val);
    if (ret)
    {
        return ret;
    }

	return 0;
}
EXPORT_SYMBOL_NS_GPL(rtl8373_phymask_write_prepare, REALTEK_DSA);

static int rtl8373_phyid_read_prepare(struct realtek_smi *priv, u32 phy_id,
				     u32 dev_addr, u32 reg_addr)
{
	u32 val;
	int ret;

	/* Set OCP prefix */
    ret = regmap_update_bits(priv->map, RTL8373_SMI_ACCESS_PHY_CTRL_3_ADDR,
                             RTL8373_SMI_ACCESS_PHY_CTRL_3_INDATA_15_0_MASK,
                             FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_3_INDATA_15_0_MASK, phy_id));
    if (ret)
    {
        return ret;
    }
	val = 0;
    val = FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_MMD_DEVAD_4_0_MASK, dev_addr);
	val |= FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_MMD_REG_15_0_MASK, reg_addr);
    ret = regmap_write(priv->map, RTL8373_SMI_ACCESS_PHY_CTRL_1_ADDR, val);
    if (ret)
    {
        return ret;
    }


	return 0;
}
static int _rtl8373_phymask_write(struct realtek_smi *priv, u32 phy_mask,
				     u32 dev_addr, u32 reg_addr, u16 indata)
{
	u32 val;
	int ret;

	ret = rtl8373_phy_poll_busy(priv);
	if (ret)
		return ret;

	ret = rtl8373_phymask_write_prepare(priv, phy_mask, dev_addr,reg_addr,indata);
	if (ret)
		return ret;

	/* Set PHY register data */
	ret = regmap_write(priv->map,
			   RTL8373_SMI_ACCESS_PHY_CTRL_1_ADDR, indata);
	if (ret)
		return ret;

	/* Execute write operation */
	val = FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_RWOP_MASK,
			 RTL8373_INDIRECT_ACCESS_CTRL_RW_WRITE) |
		  FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_TYPE_MASK,
			 RTL8373_INDIRECT_ACCESS_CTRL_CMD_VALUE) |
	      FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_CMD_MASK,
			 RTL8373_INDIRECT_ACCESS_CTRL_CMD_VALUE);
	ret = regmap_write(priv->map, RTL8373_SMI_ACCESS_PHY_CTRL_1_ADDR,
			   val);

	ret = rtl8373_phy_poll_busy(priv);
	if (ret)
		return ret;

	return 0;
}
#else
static int _rtl8373_phymask_write(struct realtek_smi *priv, u32 phy_mask, u32 dev_addr, u32 reg_addr, u32 indata)
{
    u32 value;
    int ret;

    ret = regmap_write(priv->map, RTL8373_SMI_ACCESS_PHY_CTRL_0_ADDR, phy_mask);
    if (ret) {
        return ret;
    }

    ret = rtl8373_setAsicRegBits(priv->map, RTL8373_SMI_ACCESS_PHY_CTRL_3_ADDR, RTL8373_SMI_ACCESS_PHY_CTRL_3_INDATA_15_0_MASK, indata);
    if (ret) {
        return ret;
    }

    value = (dev_addr << 19) | (reg_addr << 3) | 0x7;

    ret = regmap_write(priv->map, RTL8373_SMI_ACCESS_PHY_CTRL_1_ADDR, value);
    if (ret) {
        return ret;
    }

    ret = regmap_read_poll_timeout(priv->map, RTL8373_SMI_ACCESS_PHY_CTRL_1_ADDR, value, ((value & RTL8373_SMI_ACCESS_PHY_CTRL_1_CMD_MASK) == 0 && (value & RTL8373_SMI_ACCESS_PHY_CTRL_1_FAIL_MASK) == 0), 100, 1000);
    if (ret) {
        return -ETIMEDOUT;
    }
    return 0;
}
#endif


int _rtl8373_phy_regbits_write(struct realtek_smi *priv, u32 phy_mask,
                        u32 dev_addr, u32 reg_addr, u32 bitmask, u32 indata)
{
    rtk_api_ret_t ret;
    u32 regdata;
    u32 port;
    u32 bitsShift;
    u32 valueShifted;


    if( !bitmask )
        return RT_ERR_INPUT;

    bitsShift = 0;
    while(!(bitmask & (1 << bitsShift)))
    {
        bitsShift++;
        if(bitsShift >= RTL8373_REGBITLENGTH)
            return RT_ERR_INPUT;
    }

    valueShifted = indata << bitsShift;
    if(valueShifted > RTL8373_REGDATAMAX)
        return RT_ERR_INPUT;

    for (port = 0; port < 9; port++) {
        if ((1U << port) & phy_mask) {
			// [CBT] read function reg parameter is integer
            //ret = _rtl8373_phy_read(priv, 1 << port, dev_addr, reg_addr, &regdata);
            ret = _rtl8373_phy_read(priv, port, dev_addr, reg_addr, &regdata);
            if (ret)
                return ret;
            regdata = regdata & (~bitmask);
            regdata = regdata | (valueShifted & bitmask);
            ret = _rtl8373_phymask_write(priv, 1 << port, dev_addr, reg_addr, regdata);
            if (ret)
                return ret;
        }
    }

    return 0;
}


#if 0

static int _rtl8373_phy_read(struct realtek_smi *priv, u32 phyid,
				     u32 dev_addr, u32 reg_addr, u16 *indata)
{
	u32 val;
	int ret;

	ret = rtl8373_phy_poll_busy(priv);
	if (ret)
		return ret;

	ret = rtl8373_phyid_read_prepare(priv, phyid, dev_addr,reg_addr);
	if (ret)
		return ret;

	/* Execute write operation */
	val = FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_RWOP_MASK,
			 RTL8373_INDIRECT_ACCESS_CTRL_RW_READ) |
		  FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_TYPE_MASK,
			 RTL8373_INDIRECT_ACCESS_CTRL_CMD_VALUE) |
	      FIELD_PREP(RTL8373_SMI_ACCESS_PHY_CTRL_1_CMD_MASK,
			 RTL8373_INDIRECT_ACCESS_CTRL_CMD_VALUE);
	ret = regmap_write(priv->map, RTL8373_SMI_ACCESS_PHY_CTRL_1_ADDR,
			   val);

	ret = rtl8373_phy_poll_busy(priv);
	if (ret)
		return ret;

	/* Get PHY register data */
	ret = regmap_read(priv->map,
			  RTL8373_SMI_ACCESS_PHY_CTRL_2_ADDR, &val);
	if (ret)
		return ret;

	*indata = val & 0xFFFF;


	return 0;
}
#else
static int _rtl8373_phy_read(struct realtek_smi *priv, u32 phyid, u32 dev_addr, u32 reg_addr, u32 *indata)
{
    u32 value;
    int ret;

    ret = rtl8373_setAsicRegBits(priv->map, RTL8373_SMI_ACCESS_PHY_CTRL_3_ADDR, RTL8373_SMI_ACCESS_PHY_CTRL_3_INDATA_15_0_MASK, phyid);
    if (ret) {
        return ret;
    }

    value = (dev_addr << 19) | (reg_addr << 3) | (0 << 2) | 0x3;

    ret = regmap_write(priv->map, RTL8373_SMI_ACCESS_PHY_CTRL_1_ADDR, value);
    if (ret) {
        return ret;
    }

    ret = regmap_read_poll_timeout(priv->map, RTL8373_SMI_ACCESS_PHY_CTRL_1_ADDR, value, ((value & RTL8373_SMI_ACCESS_PHY_CTRL_1_CMD_MASK) == 0 && (value & RTL8373_SMI_ACCESS_PHY_CTRL_1_FAIL_MASK) == 0), 100, 1000);
    if (ret) {
        return -ETIMEDOUT;
    }

    ret = regmap_read(priv->map, RTL8373_SMI_ACCESS_PHY_CTRL_2_ADDR, &value);
    if (ret) {
        return ret;
    }

    *indata = (value & 0xFFFF);
    return 0;
}
#endif



int dal_rtl8373_sds_reg_write(struct realtek_smi *priv, u32 sds_index, u32 sds_page, u32 sds_reg, u32 regdata)
{
    int ret;

    ret = regmap_read_poll_timeout(priv->map,
                                   RTL8373_SDS_INDACS_CMD_ADDR,
                                   ret,
                                   (ret & RTL8373_SDS_INDACS_CMD_SDS_CMD_MASK) == 0,
                                   10,
                                   100);
    if (ret)
    {
        return ret;
    }

    ret = rtl8373_setAsicReg(priv->map, RTL8373_SDS_INDACS_WD_ADDR, regdata);
    if (ret)
    {
        return ret;
    }

    ret = rtl8373_setAsicRegBit(priv->map, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_INDEX_OFFSET, sds_index);
    if (ret)
    {
        return ret;
    }

    ret = rtl8373_setAsicRegBits(priv->map, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_PAGE_MASK, sds_page);
    if (ret)
    {
        return ret;
    }

    ret = rtl8373_setAsicRegBits(priv->map, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_REGAD_MASK, sds_reg);
    if (ret)
    {
        return ret;
    }

    ret = rtl8373_setAsicRegBit(priv->map, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_RWOP_OFFSET, 1);
    if (ret)
    {
        return ret;
    }

    ret = rtl8373_setAsicRegBit(priv->map, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_CMD_OFFSET, 1);
    if (ret)
    {
        return ret;
    }

    ret = regmap_read_poll_timeout(priv->map,
                                   RTL8373_SDS_INDACS_CMD_ADDR,
                                   ret,
                                   (ret & RTL8373_SDS_INDACS_CMD_SDS_CMD_MASK) == 0,
                                   10,
                                   1000);

    return ret;

}
EXPORT_SYMBOL_NS_GPL(dal_rtl8373_sds_reg_write, REALTEK_DSA);

int dal_rtl8373_sds_reg_read(struct realtek_smi *priv, u32 sds_index, u32 sds_page, u32 sds_reg, u32 *pdata)
{
    int ret;

    ret = regmap_read_poll_timeout(priv->map,
                                   RTL8373_SDS_INDACS_CMD_ADDR,
                                   ret,
                                   (ret & RTL8373_SDS_INDACS_CMD_SDS_CMD_MASK) == 0,
                                   10,
                                   1000);
    if (ret)
    {
        return ret;
    }

    ret = rtl8373_setAsicRegBit(priv->map, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_INDEX_OFFSET, sds_index);
    if (ret)
    {
        return ret;
    }

    ret = rtl8373_setAsicRegBits(priv->map, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_PAGE_MASK, sds_page);
    if (ret)
    {
        return ret;
    }

    ret = rtl8373_setAsicRegBits(priv->map, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_REGAD_MASK, sds_reg);
    if (ret)
    {
        return ret;
    }

    ret = rtl8373_setAsicRegBit(priv->map, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_RWOP_OFFSET, 0);
    if (ret)
    {
        return ret;
    }

    ret = rtl8373_setAsicRegBit(priv->map, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_CMD_OFFSET, 1);
    if (ret)
    {
        return ret;
    }


    ret = regmap_read_poll_timeout(priv->map,
                                   RTL8373_SDS_INDACS_CMD_ADDR,
                                   ret,
                                   (ret & RTL8373_SDS_INDACS_CMD_SDS_CMD_MASK) == 0,
                                   10,
                                   1000);
    if (ret)
    {
        return ret;
    }

    ret = rtl8373_getAsicReg(priv->map, RTL8373_SDS_INDACS_RD_ADDR, pdata);
    if (ret)
    {
        return ret;
    }

    return 0;
}
EXPORT_SYMBOL_NS_GPL(dal_rtl8373_sds_reg_read, REALTEK_DSA);

rtk_api_ret_t dal_rtl8373_sds_regbits_read(
    struct realtek_smi *priv,
    u32 sds_index, u32 sds_page, u32 sds_reg,
    u32 mask, u32 *pdata)
{
    rtk_api_ret_t retVal;
    u32 regdata;
    u32 bitsShift;

    if( !mask )
        return RT_ERR_INPUT;

    bitsShift = 0;
    while(!(mask & (1 << bitsShift)))
    {
        bitsShift++;
        if(bitsShift >= RTL8373_REGBITLENGTH)
            return RT_ERR_INPUT;
    }

    if ((retVal = dal_rtl8373_sds_reg_read(priv, sds_index,sds_page,sds_reg, &regdata)) != RT_ERR_OK)
        return retVal;

    *pdata = (regdata & mask) >> bitsShift;


     return RT_ERR_OK;
}

EXPORT_SYMBOL_NS_GPL(dal_rtl8373_sds_regbits_read, REALTEK_DSA);

int dal_rtl8373_sds_regbits_write(
    struct realtek_smi *priv,
    u32 sds_index, u32 sds_page, u32 sds_reg,
    u32 bit_mask, u32 bit_value)
{
    int ret;
    u32 regdata;
    int bitsShift;
    u32 valueShifted;

    if (!bit_mask)
        return -EINVAL;

    bitsShift = 0;
    while(!(bit_mask & (1 << bitsShift)))
    {
        bitsShift++;
        if(bitsShift >= RTL8373_REGBITLENGTH)
            return RT_ERR_INPUT;
    }

    valueShifted = bit_value << bitsShift;
    if(valueShifted > RTL8373_REGDATAMAX)
        return RT_ERR_INPUT;

    ret = dal_rtl8373_sds_reg_read(priv, sds_index, sds_page, sds_reg, &regdata);
    if (ret)
        return ret;
    regdata = regdata & (~bit_mask);
    regdata = regdata | (valueShifted & bit_mask);

    ret = dal_rtl8373_sds_reg_write(priv, sds_index, sds_page, sds_reg, regdata);
    if (ret)
        return ret;

    return 0;
}


EXPORT_SYMBOL_NS_GPL(dal_rtl8373_sds_regbits_write, REALTEK_DSA);



enum dsa_tag_protocol
rtl8373_get_tag_protocol(struct dsa_switch *ds, int port,
			   enum dsa_tag_protocol mp)
{
	struct realtek_smi *smi;
	smi = ds->priv;

	if (smi->svlan_disable == true)
    #if LINUX_VERSION_CODE < KERNEL_VERSION(5,15,0)
        return DSA_TAG_PROTO_NONE;	
    #else
    {
        smi->proto = DSA_TAG_PROTO_RTL8_4;
        return DSA_TAG_PROTO_RTL8_4; 
    }
    #endif
	else
	{
		smi->proto = DSA_TAG_PROTO_QCA_8021Q;
		return DSA_TAG_PROTO_QCA_8021Q;//DSA_TAG_PROTO_RTK_DSA_DRV
	}
}
EXPORT_SYMBOL_NS_GPL(rtl8373_get_tag_protocol, REALTEK_DSA);


int _rtl8373_portMaxLen_set(struct dsa_switch *ds, int port, int mtu)
{
	struct realtek_smi *priv = ds->priv;
    int frame_size;
    int ret;

	if (!dsa_is_cpu_port(ds, port))
		return 0;

    port = priv->ops->port_l2p_get(priv, port);
	frame_size = mtu + VLAN_ETH_HLEN + ETH_FCS_LEN;

	ret = rtl8373_setAsicRegBits(priv->map, RTL8373_MAC_L2_PORT_MAX_LEN_CTRL_ADDR(port), RTL8373_MAC_L2_PORT_MAX_LEN_CTRL_MAX_LEN_100M_10M_SEL_MASK, frame_size);
	if(ret != RT_ERR_OK)
		return ret;

	ret = rtl8373_setAsicRegBits(priv->map, RTL8373_MAC_L2_PORT_MAX_LEN_CTRL_ADDR(port), RTL8373_MAC_L2_PORT_MAX_LEN_CTRL_MAX_LEN_1G_2P5G_5G_10G_SEL_MASK, frame_size);
	if(ret != RT_ERR_OK)
		return ret;

    return RT_ERR_OK;
}
int rtl8373_portMaxLen_set(struct dsa_switch *ds, int port, int mtu)
{
	struct realtek_smi *smi = ds->priv;
	int ret;
	mutex_lock(&smi->map_lock);
	ret = _rtl8373_portMaxLen_set(ds, port, mtu);
	mutex_unlock(&smi->map_lock);
    return ret;
}
EXPORT_SYMBOL_NS_GPL(rtl8373_portMaxLen_set, REALTEK_DSA);

int rtl8373_portMaxLen_get(struct dsa_switch *ds, int port)
{
	return 11996;
}

EXPORT_SYMBOL_NS_GPL(rtl8373_portMaxLen_get, REALTEK_DSA);



 /* Function Name:
 *      dal_rtl8224_top_reg_write
 * Description:
 *      Set RTL8224 top register value.
 * Input:
 *      top_reg_addr   - top register address
 *      value               - register value
 * Output:
*      pdata       - None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_QUEUE_NUM    - Invalid queue number.
 *      RT_ERR_INPUT        - Invalid input parameters.
 * Note:
 *      This API will set phy register data.
 */

int dal_rtl8224_top_reg_write(struct realtek_smi *priv, u32 top_reg_addr,  u32 value)
{
    u32 phymask;
    int ret;
    u32 lowdata, highdata;

    phymask=1;

    lowdata = value & 0xffff;
    highdata = (value >> 16) & 0xffff;
    if ((ret = rtl8373_phy_write(priv, phymask,30,top_reg_addr,lowdata)) != RT_ERR_OK)
        return ret;

    if ((ret = rtl8373_phy_write(priv, phymask,30,top_reg_addr+1,highdata)) != RT_ERR_OK)
        return ret;

    return RT_ERR_OK;
}
EXPORT_SYMBOL_NS_GPL(dal_rtl8224_top_reg_write, REALTEK_DSA);

/* Function Name:
 *      dal_rtl8224_top_reg_read
 * Description:
 *      Get RTL8224 top register value.
 * Input:
 *      top_reg_addr   - top register address
 *      pvalue               - register value
 * Output:
*      pdata       - None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_QUEUE_NUM    - Invalid queue number.
 *      RT_ERR_INPUT        - Invalid input parameters.
 * Note:
 *      This API will get phy register data.
 */

int dal_rtl8224_top_reg_read(struct realtek_smi *priv, u32 top_reg_addr,  u32* pvalue)
{
    u32 phyid;
    u32 lowdata, highdata;
    int ret;

    phyid=0;
    if ((ret = rtl8373_phy_read(priv, phyid,30,top_reg_addr,&lowdata)) != RT_ERR_OK)
        return ret;

    if ((ret = rtl8373_phy_read(priv, phyid,30,top_reg_addr+1,&highdata)) != RT_ERR_OK)
        return ret;

    *pvalue = (lowdata & 0xffff) | ((u32)(highdata & 0xffff) << 16);

    return RT_ERR_OK;
}
EXPORT_SYMBOL_NS_GPL(dal_rtl8224_top_reg_read, REALTEK_DSA);

/* Function Name:
 *      dal_rtl8224_top_regbit_write
 * Description:
 *      Set RTL8224 top register bit value.
 * Input:
 *      top_reg_addr   - top register address
 *      offet                - bit offset
 *      value               - register bit value
 * Output:
*      pdata       - None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_QUEUE_NUM    - Invalid queue number.
 *      RT_ERR_INPUT        - Invalid input parameters.
 * Note:
 *      This API will set phy register data.
 */

int  dal_rtl8224_top_regbit_write(struct realtek_smi *priv, u32 top_reg_addr,  u32 offset, u32 value)
{
    u32 phyid;
    u32 phymask;
    u32 regdata;
    int ret;

    phyid=0;

    if(offset < 16)
    {
        if ((ret = rtl8373_phy_read(priv, phyid,30,top_reg_addr, &regdata)) != RT_ERR_OK)
            return ret;
        if(1 == value)
            regdata |= (1 << offset);
        else
            regdata &= ~(1 << offset);

        phymask = 1;
        return rtl8373_phy_write(priv, phymask, 30, top_reg_addr, regdata);

    }
    else
    {
        offset = offset - 16;

        if ((ret = rtl8373_phy_read(priv, phyid,30,top_reg_addr+1, &regdata)) != RT_ERR_OK)
            return ret;

        if(1 == value)
            regdata |= (1 << offset);
        else
            regdata &= ~(1 << offset);

        phymask = 1;
        return rtl8373_phy_write(priv, phymask, 30, top_reg_addr+1, regdata);
    }
}
EXPORT_SYMBOL_NS_GPL(dal_rtl8224_top_regbit_write, REALTEK_DSA);

/* Function Name:
 *      dal_rtl8224_top_regbit_read
 * Description:
 *      Get RTL8224 top register bit value.
 * Input:
 *      top_reg_addr   - top register address
 *      offet                - bit offset
 *      value               - register bit value
 * Output:
*      pdata       - None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_QUEUE_NUM    - Invalid queue number.
 *      RT_ERR_INPUT        - Invalid input parameters.
 * Note:
 *      This API will get phy register data.
 */

int dal_rtl8224_top_regbit_read(struct realtek_smi *priv, u32 top_reg_addr,  u32 offset, u32* pvalue)
{
    u32 phyid;
    u32 regdata;
    int ret;

    phyid=0;

    if(offset < 16)
    {
        if ((ret = rtl8373_phy_read(priv, phyid,30,top_reg_addr, &regdata)) != RT_ERR_OK)
            return ret;

        *pvalue = (regdata >> offset) & 1;
    }
    else
    {
        offset =  offset - 16;
        if ((ret = rtl8373_phy_read(priv, phyid,30,top_reg_addr+1, &regdata)) != RT_ERR_OK)
            return ret;

        *pvalue = (regdata >> offset) & 1;
    }

    return RT_ERR_OK;
}
EXPORT_SYMBOL_NS_GPL(dal_rtl8224_top_regbit_read, REALTEK_DSA);

/* Function Name:
 *      dal_rtl8224_top_regbits_write
 * Description:
 *      Set RTL8224 top register bits value.
 * Input:
 *      top_reg_addr   - top register address
 *      bitmask          - bit mask
 *      value               - register bits value
 * Output:
*      pdata       - None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_QUEUE_NUM    - Invalid queue number.
 *      RT_ERR_INPUT        - Invalid input parameters.
 * Note:
 *      This API will set phy register data.
 */

int dal_rtl8224_top_regbits_write(struct realtek_smi *priv, u32 top_reg_addr, u32 bitmask, u32 value)
{
    u32 phymask;
    u32 phyid;
    u32 regdata;
    u32 regdatau16;
    int ret;
    u32 bitsShift;
    u32 valueShifted;

    if( !bitmask )
        return RT_ERR_INPUT;

    bitsShift = 0;
    while(!(bitmask & (1 << bitsShift)))
    {
        bitsShift++;
        if(bitsShift >= RTL8373_REGBITLENGTH)
            return RT_ERR_INPUT;
    }

    valueShifted = value << bitsShift;
    if(valueShifted > RTL8373_REGDATAMAX)
        return RT_ERR_INPUT;

    if(bitsShift < 16)
    {
        phyid=0;
        if ((ret = rtl8373_phy_read(priv, phyid,30,top_reg_addr, &regdatau16)) != RT_ERR_OK)
            return ret;

        regdata = regdatau16 & (~bitmask);
        regdata = regdata | (valueShifted & bitmask);

        phymask = 1;
        return rtl8373_phy_write(priv, phymask, 30, top_reg_addr, regdata);
    }
    else
    {
        phyid=0;
        if ((ret = rtl8373_phy_read(priv, phyid,30,top_reg_addr+1, &regdatau16)) != RT_ERR_OK)
            return ret;

        bitmask = bitmask >> 16;
        valueShifted = valueShifted >> 16;
        regdata = regdatau16 & (~bitmask);
        regdata = regdata | (valueShifted & bitmask);

        phymask = 1;
        return rtl8373_phy_write(priv, phymask, 30, top_reg_addr+1, regdata);
    }

}
EXPORT_SYMBOL_NS_GPL(dal_rtl8224_top_regbits_write, REALTEK_DSA);


/* Function Name:
 *      dal_rtl8224_top_regbits_read
 * Description:
 *      Get RTL8224 top register bits value.
 * Input:
 *      top_reg_addr   - top register address
 *      bitmask          - bit mask
 *      value               - register bits value
 * Output:
*      pdata       - None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_QUEUE_NUM    - Invalid queue number.
 *      RT_ERR_INPUT        - Invalid input parameters.
 * Note:
 *      This API will get phy register data.
 */

int dal_rtl8224_top_regbits_read(struct realtek_smi *priv, u32 top_reg_addr,  u32 bitmask, u32* pvalue)
{
    u32 phyid;
    u32 regdata;
    int ret;
    u32 bitsShift;

    if( !bitmask )
        return RT_ERR_INPUT;

    bitsShift = 0;
    while(!(bitmask & (1 << bitsShift)))
    {
        bitsShift++;
        if(bitsShift >= RTL8373_REGBITLENGTH)
            return RT_ERR_INPUT;
    }

    phyid=0;

    if(bitsShift < 16)
    {
        if ((ret = rtl8373_phy_read(priv, phyid,30,top_reg_addr, &regdata)) != RT_ERR_OK)
            return ret;

        *pvalue = (regdata & bitmask) >> bitsShift;
    }
    else
    {
        if ((ret = rtl8373_phy_read(priv, phyid,30,top_reg_addr+1, &regdata)) != RT_ERR_OK)
            return ret;

        bitsShift = bitsShift - 16;
        bitmask = bitmask >> 16;
        *pvalue = (regdata & bitmask) >> bitsShift;
    }

    return RT_ERR_OK;
}
EXPORT_SYMBOL_NS_GPL(dal_rtl8224_top_regbits_read, REALTEK_DSA);
/* Function Name:
 *      dal_rtl8224_sdsreg_write
 * Description:
 *      Configure phy register data.
 * Input:
 *      sds_index  - sds index 0 ~ 1
 *      sds_page   - page
 *      sds_geg   - register address
 *      regdata       - input data
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_QUEUE_NUM    - Invalid queue number.
 *      RT_ERR_INPUT        - Invalid input parameters.
 * Note:
 *      This API will configure phy register data.
 */

int dal_rtl8224_sds_reg_write(struct realtek_smi *priv, u32 sds_index, u32 sds_page, u32 sds_reg, u32 regdata)
{
    int ret;
    u32 tmp_cmd;
    u32 pollcnt = 0;

    for(pollcnt = 0; pollcnt < RTL8373_MAX_POLLCNT; pollcnt++)
    {
           if ((ret = dal_rtl8224_top_regbit_read(priv, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_CMD_OFFSET, &tmp_cmd)) != RT_ERR_OK)
                       return ret;

           if(tmp_cmd == 0)
                  break;
    }

    if(pollcnt == RTL8373_MAX_POLLCNT)
            return RT_ERR_BUSYWAIT_TIMEOUT;

    if ((ret = dal_rtl8224_top_reg_write(priv, RTL8373_SDS_INDACS_WD_ADDR, regdata)) != RT_ERR_OK)
                return ret;


    if ((ret = dal_rtl8224_top_regbit_write(priv, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_INDEX_OFFSET, sds_index)) != RT_ERR_OK)
                return ret;


    if ((ret = dal_rtl8224_top_regbits_write(priv, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_PAGE_MASK, sds_page)) != RT_ERR_OK)
                return ret;


    if ((ret = dal_rtl8224_top_regbits_write(priv, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_REGAD_MASK, sds_reg)) != RT_ERR_OK)
                return ret;


    if ((ret = dal_rtl8224_top_regbit_write(priv, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_RWOP_OFFSET, 1)) != RT_ERR_OK)
                return ret;


    if ((ret = dal_rtl8224_top_regbit_write(priv, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_CMD_OFFSET, 1)) != RT_ERR_OK)
                    return ret;


    for(pollcnt = 0; pollcnt < RTL8373_MAX_POLLCNT; pollcnt++)
    {
           if ((ret = dal_rtl8224_top_regbit_read(priv, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_CMD_OFFSET, &tmp_cmd)) != RT_ERR_OK)
                       return ret;

           if(tmp_cmd == 0)
                  break;
    }

    if(pollcnt == RTL8373_MAX_POLLCNT)
            return RT_ERR_BUSYWAIT_TIMEOUT;

     return RT_ERR_OK;
}

EXPORT_SYMBOL_NS_GPL(dal_rtl8224_sds_reg_write, REALTEK_DSA);
/* Function Name:
 *      dal_rtl8224_sdsreg_read
 * Description:
 *      Configure phy register data.
 * Input:
 *      sds_index  - sds index 0 ~ 1
 *      sds_page   - page
 *      sds_geg   - register address
 *      pdata       - output data
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_QUEUE_NUM    - Invalid queue number.
 *      RT_ERR_INPUT        - Invalid input parameters.
 * Note:
 *      This API will configure phy register data.
 */

int dal_rtl8224_sds_reg_read(struct realtek_smi *priv, u32 sds_index, u32 sds_page, u32 sds_reg, u32 * pdata)
{
    int ret;
    u32 tmp_cmd;
    u32 pollcnt = 0;

    for(pollcnt = 0; pollcnt < RTL8373_MAX_POLLCNT; pollcnt++)
    {
           if ((ret = dal_rtl8224_top_regbit_read(priv, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_CMD_OFFSET, &tmp_cmd)) != RT_ERR_OK)
                       return ret;

           if(tmp_cmd == 0)
                  break;
    }

    if(pollcnt == RTL8373_MAX_POLLCNT)
            return RT_ERR_BUSYWAIT_TIMEOUT;



    if ((ret = dal_rtl8224_top_regbit_write(priv, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_INDEX_OFFSET, sds_index)) != RT_ERR_OK)
                return ret;


    if ((ret = dal_rtl8224_top_regbits_write(priv, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_PAGE_MASK, sds_page)) != RT_ERR_OK)
                return ret;


    if ((ret = dal_rtl8224_top_regbits_write(priv, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_REGAD_MASK, sds_reg)) != RT_ERR_OK)
                return ret;


    if ((ret = dal_rtl8224_top_regbit_write(priv, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_RWOP_OFFSET, 0)) != RT_ERR_OK)
                return ret;


    if ((ret = dal_rtl8224_top_regbit_write(priv, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_CMD_OFFSET, 1)) != RT_ERR_OK)
                    return ret;


    for(pollcnt = 0; pollcnt < RTL8373_MAX_POLLCNT; pollcnt++)
    {
           if ((ret = dal_rtl8224_top_regbit_read(priv, RTL8373_SDS_INDACS_CMD_ADDR, RTL8373_SDS_INDACS_CMD_SDS_CMD_OFFSET, &tmp_cmd)) != RT_ERR_OK)
                       return ret;

           if(tmp_cmd == 0)
                  break;
    }

    if ((ret = dal_rtl8224_top_reg_read(priv, RTL8373_SDS_INDACS_RD_ADDR, pdata)) != RT_ERR_OK)
                return ret;


    if(pollcnt == RTL8373_MAX_POLLCNT)
            return RT_ERR_BUSYWAIT_TIMEOUT;

     return RT_ERR_OK;
}

EXPORT_SYMBOL_NS_GPL(dal_rtl8224_sds_reg_read, REALTEK_DSA);
/* Function Name:
 *      dal_rtl8224_sds_regbits_write
 * Description:
 *      Configure phy register data.
 * Input:
 *      sds_index  - sds index 0 ~ 1
 *      sds_page   - page
 *      sds_geg   - register address
 *      bitmask   - bits mask
 *      value       - input data
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_QUEUE_NUM    - Invalid queue number.
 *      RT_ERR_INPUT        - Invalid input parameters.
 * Note:
 *      This API will configure phy register data.
 */

int dal_rtl8224_sds_regbits_write(struct realtek_smi *priv, u32 sds_index, u32 sds_page, u32 sds_reg, u32 bitmask, u32 value)
{
    int ret;
    u32 regdata;
    u32 bitsShift;
    u32 valueShifted;

    if( !bitmask )
        return RT_ERR_INPUT;

    bitsShift = 0;
    while(!(bitmask & (1 << bitsShift)))
    {
        bitsShift++;
        if(bitsShift >= RTL8373_REGBITLENGTH)
            return RT_ERR_INPUT;
    }

    valueShifted = value << bitsShift;
    if(valueShifted > RTL8373_REGDATAMAX)
        return RT_ERR_INPUT;

    if ((ret = dal_rtl8224_sds_reg_read(priv, sds_index,sds_page,sds_reg,&regdata)) != RT_ERR_OK)
            return ret;

    regdata = regdata & (~bitmask);
    regdata = regdata | (valueShifted & bitmask);

    if ((ret = dal_rtl8224_sds_reg_write(priv, sds_index,sds_page,sds_reg,regdata)) != RT_ERR_OK)
            return ret;

    return RT_ERR_OK;
}
EXPORT_SYMBOL_NS_GPL(dal_rtl8224_sds_regbits_write, REALTEK_DSA);

/* Function Name:
 *      dal_rtl8224_sds_regbis_read
 * Description:
 *      Configure phy register data.
 * Input:
 *      sds_index  - sds index 0 ~ 1
 *      sds_page   - page
 *      sds_geg   - register address
 *      pvalue       - output data
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 *      RT_ERR_QUEUE_NUM    - Invalid queue number.
 *      RT_ERR_INPUT        - Invalid input parameters.
 * Note:
 *      This API will configure phy register data.
 */

int dal_rtl8224_sds_regbits_read(struct realtek_smi *priv, u32 sds_index, u32 sds_page, u32 sds_reg, u32 bitmask, u32 * pvalue)
{
    int ret;
    u32 regdata;
    u32 bitsShift;

    if( !bitmask )
        return RT_ERR_INPUT;

    bitsShift = 0;
    while(!(bitmask & (1 << bitsShift)))
    {
        bitsShift++;
        if(bitsShift >= RTL8373_REGBITLENGTH)
            return RT_ERR_INPUT;
    }

    if ((ret = dal_rtl8224_sds_reg_read(priv, sds_index,sds_page,sds_reg, &regdata)) != RT_ERR_OK)
        return ret;

    *pvalue = (regdata & bitmask) >> bitsShift;

     return RT_ERR_OK;
}

EXPORT_SYMBOL_NS_GPL(dal_rtl8224_sds_regbits_read, REALTEK_DSA);

MODULE_IMPORT_NS(REALTEK_DSA);
