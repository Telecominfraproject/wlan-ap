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
#include "rtl8373_mib.h"
#include "rtl8373_interrupt.h"

#if 1
struct rtl8373_mib_counter rtl8373_mib_counters[] = {
    RTL8373_MAKE_MIB_COUNTER(0, 2, ifInOctets),
    RTL8373_MAKE_MIB_COUNTER(2, 2, ifOutOctets),
    RTL8373_MAKE_MIB_COUNTER(4, 2, ifInUcastPkts),
    RTL8373_MAKE_MIB_COUNTER(6, 2, ifInMulticastPkts),
    RTL8373_MAKE_MIB_COUNTER(8, 2, ifInBroadcastPkts),
    RTL8373_MAKE_MIB_COUNTER(10, 2, ifOutUcastPkts),
    RTL8373_MAKE_MIB_COUNTER(12, 2, ifOutMulticastPkts),
    RTL8373_MAKE_MIB_COUNTER(14, 2, ifOutBroadcastPkts),
    RTL8373_MAKE_MIB_COUNTER(16, 1, ifOutDiscards),
    RTL8373_MAKE_MIB_COUNTER(17, 1, dot1dTpPortInDiscards),
    RTL8373_MAKE_MIB_COUNTER(18, 1, dot3StatsSingleColFrames),
    RTL8373_MAKE_MIB_COUNTER(19, 1, dot3StatsMultiColFrames),
    RTL8373_MAKE_MIB_COUNTER(20, 1, dot3sDeferredTransmissions),
    RTL8373_MAKE_MIB_COUNTER(21, 1, dot3StatsLateCollisions),
    RTL8373_MAKE_MIB_COUNTER(22, 1, dot3StatsExcessCollisions),
    RTL8373_MAKE_MIB_COUNTER(23, 1, dot3StatsSymbolErrors),
    RTL8373_MAKE_MIB_COUNTER(24, 1, dot3CtrlInUnknownOpcodes),
    RTL8373_MAKE_MIB_COUNTER(25, 1, dot3InPauseFrames),
    RTL8373_MAKE_MIB_COUNTER(26, 1, dot3OutPauseFrames),
    RTL8373_MAKE_MIB_COUNTER(27, 1, etherStatsDropEvents),
    RTL8373_MAKE_MIB_COUNTER(28, 1, tx_ethBroadcastPkts),
    RTL8373_MAKE_MIB_COUNTER(29, 1, tx_ethMulticastPkts),
    RTL8373_MAKE_MIB_COUNTER(30, 1, tx_ethCRCAlignErrors),
    RTL8373_MAKE_MIB_COUNTER(31, 1, rx_ethCRCAlignErrors),
    RTL8373_MAKE_MIB_COUNTER(32, 1, tx_ethUndersizePkts),
    RTL8373_MAKE_MIB_COUNTER(33, 1, rx_ethUndersizePkts),
    RTL8373_MAKE_MIB_COUNTER(34, 1, tx_ethOversizePkts),
    RTL8373_MAKE_MIB_COUNTER(35, 1, rx_ethOversizePkts),
    RTL8373_MAKE_MIB_COUNTER(36, 1, tx_ethFragments),
    RTL8373_MAKE_MIB_COUNTER(37, 1, rx_ethFragments),
    RTL8373_MAKE_MIB_COUNTER(38, 1, tx_ethJabbers),
    RTL8373_MAKE_MIB_COUNTER(39, 1, rx_ethJabbers),
    RTL8373_MAKE_MIB_COUNTER(40, 1, tx_ethCollisions),
    RTL8373_MAKE_MIB_COUNTER(41, 1, tx_ethPkts64Octets),
    RTL8373_MAKE_MIB_COUNTER(42, 1, rx_ethPkts64Octets),
    RTL8373_MAKE_MIB_COUNTER(43, 1, tx_ethPkts65to127Octets),
    RTL8373_MAKE_MIB_COUNTER(44, 1, rx_ethPkts65to127Octets),
    RTL8373_MAKE_MIB_COUNTER(45, 1, tx_ethPkts128to255Octets),
    RTL8373_MAKE_MIB_COUNTER(46, 1, rx_ethPkts128to255Octets),
    RTL8373_MAKE_MIB_COUNTER(47, 1, tx_ethPkts256to511Octets),
    RTL8373_MAKE_MIB_COUNTER(48, 1, rx_ethPkts256to511Octets),
    RTL8373_MAKE_MIB_COUNTER(49, 1, tx_ethPkts512to1023Octets),
    RTL8373_MAKE_MIB_COUNTER(50, 1, rx_ethPkts512to1023Octets),
    RTL8373_MAKE_MIB_COUNTER(51, 1, tx_Pkts1024to1518Oct),
    RTL8373_MAKE_MIB_COUNTER(52, 1, rx_Pkts1024to1518Oct),
    RTL8373_MAKE_MIB_COUNTER(54, 1, rx_ethUndersizedropPkts),
    RTL8373_MAKE_MIB_COUNTER(55, 1, tx_Pkts1519toMaxOct),
    RTL8373_MAKE_MIB_COUNTER(56, 1, rx_Pkts1519toMaxOct),
    RTL8373_MAKE_MIB_COUNTER(57, 1, tx_ethPktsOverMaxOctets),
    RTL8373_MAKE_MIB_COUNTER(58, 1, rx_ethPktsOverMaxOctets),
    RTL8373_MAKE_MIB_COUNTER(59, 1, tx_FlexPktsSET1),
    RTL8373_MAKE_MIB_COUNTER(60, 1, rx_FlexPktsSET1),
    RTL8373_MAKE_MIB_COUNTER(61, 1, tx_FlexPktsCRCSET1),
    RTL8373_MAKE_MIB_COUNTER(62, 1, rx_FlexPktsCRCSET1),
    RTL8373_MAKE_MIB_COUNTER(63, 1, tx_FlexPktsSET0),
    RTL8373_MAKE_MIB_COUNTER(64, 1, rx_FlexPktsSET0),
    RTL8373_MAKE_MIB_COUNTER(65, 1, tx_FlexPktsCRSET0C),
    RTL8373_MAKE_MIB_COUNTER(66, 1, rx_FlexPktsCRSET0C),
    RTL8373_MAKE_MIB_COUNTER(67, 1, lengthFieldError),
    RTL8373_MAKE_MIB_COUNTER(68, 1, falseCarrieimes),
    RTL8373_MAKE_MIB_COUNTER(69, 1, underSizeOctets),
    RTL8373_MAKE_MIB_COUNTER(70, 1, framingErrors),
    RTL8373_MAKE_MIB_COUNTER(72, 1, rxMacDiscards),
    RTL8373_MAKE_MIB_COUNTER(73, 1, rxMacIPGShortDropRT),
    RTL8373_MAKE_MIB_COUNTER(75, 1, dot1dTpLearnEntryDisc),
    RTL8373_MAKE_MIB_COUNTER(76, 1, egrQueue7DropPktRT),
    RTL8373_MAKE_MIB_COUNTER(77, 1, egrQueue6DropPktRT),
    RTL8373_MAKE_MIB_COUNTER(78, 1, egrQueue5DropPktRT),
    RTL8373_MAKE_MIB_COUNTER(79, 1, egrQueue4DropPktRT),
    RTL8373_MAKE_MIB_COUNTER(80, 1, egrQueue3DropPktRT),
    RTL8373_MAKE_MIB_COUNTER(81, 1, egrQueue2DropPktRT),
    RTL8373_MAKE_MIB_COUNTER(82, 1, egrQueue1DropPktRT),
    RTL8373_MAKE_MIB_COUNTER(83, 1, egrQueue0DropPktRT),
    RTL8373_MAKE_MIB_COUNTER(84, 1, egrQueue7OutPktRT),
    RTL8373_MAKE_MIB_COUNTER(85, 1, egrQueue6OutPktRT),
    RTL8373_MAKE_MIB_COUNTER(86, 1, egrQueue5OutPktRT),
    RTL8373_MAKE_MIB_COUNTER(87, 1, egrQueue4OutPktRT),
    RTL8373_MAKE_MIB_COUNTER(88, 1, egrQueue3OutPktRT),
    RTL8373_MAKE_MIB_COUNTER(89, 1, egrQueue2OutPktRT),
    RTL8373_MAKE_MIB_COUNTER(90, 1, egrQueue1OutPktRT),
    RTL8373_MAKE_MIB_COUNTER(91, 1, egrQueue0OutPktRT),
    RTL8373_MAKE_MIB_COUNTER(92, 2, TxGoodCnt),
    RTL8373_MAKE_MIB_COUNTER(94, 2, RxGoodCnt),
    RTL8373_MAKE_MIB_COUNTER(96, 1, RxErrorCnt),
    RTL8373_MAKE_MIB_COUNTER(97, 1, TxErrorCnt),
    RTL8373_MAKE_MIB_COUNTER(98, 2, TxGoodCnt_phy),
    RTL8373_MAKE_MIB_COUNTER(100, 2, RxGoodCnt_phy),
    RTL8373_MAKE_MIB_COUNTER(102, 1, RxErrorCnt_phy),
    RTL8373_MAKE_MIB_COUNTER(103, 1, TxErrorCnt_phy),
};
#endif

int rtl8373_mib_size(void)
{
    return ARRAY_SIZE(rtl8373_mib_counters);
}

#define RTL8373_MIB_ADDRESS_SP(_p, _x) \
        (1 | (((_p) & 0xf) << 1) | ((((_x) / 2) & 0x3f) << 5))

int rtl8373_mib_counter_read(struct realtek_smi *priv, int port,
                                     u32 offset, u32 length, u64 *mibvalue)
{
    u32 dataH, dataL;
    int ret = 0;

      // Set the MIB counter address
    ret = rtl8373_setAsicReg(priv->map,
                             RTL8373_INDIRECT_ACCESS_CTRL_ADDR,
                             RTL8373_MIB_ADDRESS_SP(priv->ops->port_l2p_get(priv, port), offset));

    if (ret)
        goto out;

    // Wait for the read to complete
    ret = regmap_read_poll_timeout(priv->map,
                                   RTL8373_INDIRECT_ACCESS_CTRL_ADDR,
                                   dataL,  // This parameter checks the condition
                                   !(dataL & (1 << RTL8373_INDIRECT_ACCESS_CTRL_ACC_CMD_OFFSET)),
                                   10,
                                   100);
    if (ret)
        goto out;

    // Read the lower 32-bit register value
    ret = rtl8373_getAsicReg(priv->map,
                             RTL8373_INDIRECT_ACCESS_CNT_L_ADDR,
                             &dataL);
    if (ret)
        goto out;

    // Read the upper 32-bit register value
    ret = rtl8373_getAsicReg(priv->map,
                             RTL8373_INDIRECT_ACCESS_CNT_H_ADDR,
                             &dataH);
    if (ret)
        goto out;

    // Set the MIB value based on the length and offset
    if (length == 2) {
        *mibvalue = ((u64)dataL << 32) | dataH;
    } else if (offset % 2) {
        *mibvalue = dataH;
    } else {
        *mibvalue = dataL;
    }
out:

    return ret;
}


EXPORT_SYMBOL_NS_GPL(rtl8373_mib_counter_read, REALTEK_DSA);

void rtl8373_get_ethtool_stats(struct dsa_switch *ds, int port, u64 *data)
{
    struct realtek_smi *priv = ds->priv;
    int i, j = 0;
    int ret;

    mutex_lock(&priv->map_lock);

    for (i = 0; i < priv->num_mib_counters; i++) {

        struct rtl8373_mib_counter *mib = &priv->mib_counters[i];

        if (!mib->name)
            continue;

        ret = rtl8373_mib_counter_read(priv, port,
                                       mib->offset,
                                       mib->length,
                                       &data[j]);
        if (ret) {
            dev_err(priv->dev,
                    "failed to read counter %s (port %d): %d\n",
                    mib->name, port, ret);
			data[j] = 0;
			j++;
            continue;
        }

		//printk(" %s data = %lld\r\n",mib->name ,data[j]);
        j++;
    }

    mutex_unlock(&priv->map_lock);
}
EXPORT_SYMBOL_NS_GPL(rtl8373_get_ethtool_stats, REALTEK_DSA);

int rtl8373_get_sset_count(struct dsa_switch *ds, int port, int sset)
{
	struct realtek_smi *priv = ds->priv;
	int i, cnt = 0;

	if (sset != ETH_SS_STATS)
		return 0;

    for (i = 0; i < priv->num_mib_counters; i++) {

		struct rtl8373_mib_counter *mib = &priv->mib_counters[i];

        if (mib->name)
            cnt++;
    }

    return cnt;
}
EXPORT_SYMBOL_NS_GPL(rtl8373_get_sset_count, REALTEK_DSA);

static void _rtl8373_get_strings(struct dsa_switch *ds, int port, u32 stringset,
			 uint8_t *data)
{
	struct realtek_smi *priv = ds->priv;
	int i, j = 0;

	if (stringset != ETH_SS_STATS)
		return;

	if (port >= priv->num_ports)
		return;

	for (i = 0; i < priv->num_mib_counters; i++) {

		struct rtl8373_mib_counter *mib = &priv->mib_counters[i];

		if (!mib->name)
			continue;

		strncpy(data + j * ETH_GSTRING_LEN, mib->name, ETH_GSTRING_LEN);
		data[(j * ETH_GSTRING_LEN) + ETH_GSTRING_LEN - 1] = '\0';

		j++;
	}
}

void rtl8373_get_strings(struct dsa_switch *ds, int port, u32 stringset,
			 uint8_t *data)
{
	struct realtek_smi *priv = ds->priv;
    mutex_lock(&priv->map_lock);
    _rtl8373_get_strings(ds, port, stringset, data);
    mutex_unlock(&priv->map_lock);
}
EXPORT_SYMBOL_NS_GPL(rtl8373_get_strings, REALTEK_DSA);

MODULE_IMPORT_NS(REALTEK_DSA);
