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


//#include "realtek-mdio.h"
#include "realtek-smi-rtl8373.h"
#include "rtl8373_core.h"
#include "rtl8373_lut.h"
#include "rtl8373_interrupt.h"


static int rtl8373_get_and_clear_status_reg(struct realtek_smi *priv, u32 reg,
					      u32 *val)
{
	int ret;

	ret = regmap_read(priv->map, reg, val);
	if (ret)
		return ret;

	return regmap_write(priv->map, reg, *val);
}

irqreturn_t rtl8373_irq(int irq, void *data)
{
	struct realtek_smi *priv = data;
	unsigned long line_changes = 0;
	u32 stat;
	int line;
	int ret;

	ret = rtl8373_get_and_clear_status_reg(priv, RTL8373_ISR_EXT_GLB_ADDR,
						 &stat); 
	if (ret)
		goto out_error;

	if (stat & RTL8373_ISR_EXT_GLB_ISR_EXT_GLB_LINK_CHG_MASK) {

		u32 val;

		ret = rtl8373_get_and_clear_status_reg(
			priv, RTL8373_ISR_EXT_PORT_LINK_STS_CHG_ADDR, &val);
		if (ret)
			goto out_error;

		line_changes = FIELD_GET(RTL8373_ISR_EXT_PORT_LINK_STS_CHG_ISR_EXT_PORT_LINK_STS_CHG_9_0_MASK, val);

	}

	if (!line_changes)
		goto out_none;

	for_each_set_bit(line, &line_changes, priv->end_pports) {
		int child_irq = irq_find_mapping(priv->irqdomain, line);

		handle_nested_irq(child_irq);
	}

	return IRQ_HANDLED;

out_error:
	dev_err(priv->dev, "failed to read interrupt status: %d\n", ret);

out_none:
	return IRQ_NONE;
}

EXPORT_SYMBOL_NS_GPL(rtl8373_irq, REALTEK_DSA);
/* Function Name:
 *      dal_rtl8373_IE_set
 * Description:
 *      Set switch interrupt enable
 * Input:
 *     enable: 0:disable,  1: enable
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK           - OK
 *      RT_ERR_FAILED       - Failed
 *      RT_ERR_SMI          - SMI access error
 * Note:
  *      The API can set wol enable 
 */

int dal_rtl8373_IE_set(struct realtek_smi *priv, bool enable)
{
    int ret;

    if ((ret = rtl8373_setAsicRegBit(priv->map, RTL8373_ISR_SW_INT_MODE_ADDR, RTL8373_ISR_SW_INT_MODE_SWITCH_IE_OFFSET, enable)) != RT_ERR_OK)
        return ret;

    return RT_ERR_OK;
}

static int rtl8373_set_irq_enable(struct realtek_smi *priv, bool enable)
{
    int ret;
	ret = dal_rtl8373_IE_set(priv, enable);
    if(ret)
        return ret;

    return rtl8373_setAsicRegBits(priv->map, RTL8373_IMR_EXT_PORT_LINK_STS_CHG_ADDR,
                RTL8373_IMR_EXT_PORT_LINK_STS_CHG_IMR_EXT_PORT_LINK_STS_CHG_10_0_MASK,enable ? 0x7ff : 0);
}

int rtl8373_irq_enable(struct realtek_smi *priv)
{
	return rtl8373_set_irq_enable(priv, true);
}
EXPORT_SYMBOL_NS_GPL(rtl8373_irq_enable, REALTEK_DSA);

int rtl8373_irq_disable(struct realtek_smi *priv)
{
	return rtl8373_set_irq_enable(priv, false);
}

EXPORT_SYMBOL_NS_GPL(rtl8373_irq_disable, REALTEK_DSA);

MODULE_IMPORT_NS(REALTEK_DSA);


