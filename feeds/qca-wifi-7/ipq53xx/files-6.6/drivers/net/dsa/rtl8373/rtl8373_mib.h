// SPDX-License-Identifier: GPL-2.0
#ifndef __RTL8373_MIB_H__
#define __RTL8373_MIB_H__


typedef enum rtl8373_mib_counter_index {

    RTL8373_MIB_ifInOctets = 0,
    RTL8373_MIB_ifOutOctets,
    RTL8373_MIB_ifInUcastPkts,
    RTL8373_MIB_ifInMulticastPkts,
    RTL8373_MIB_ifInBroadcastPkts,
    RTL8373_MIB_ifOutUcastPkts,
    RTL8373_MIB_ifOutMulticastPkts,
    RTL8373_MIB_ifOutBroadcastPkts,

    RTL8373_MIB_ifOutDiscards,
    RTL8373_MIB_dot1dTpPortInDiscards,

    RTL8373_MIB_dot3StatsSingleColFrames,
    RTL8373_MIB_dot3StatsMultiColFrames,
    RTL8373_MIB_dot3sDeferredTransmissions,
    RTL8373_MIB_dot3StatsLateCollisions,
    RTL8373_MIB_dot3StatsExcessCollisions,
    RTL8373_MIB_dot3StatsSymbolErrors,
    RTL8373_MIB_dot3CtrlInUnknownOpcodes,
    RTL8373_MIB_dot3InPauseFrames,
    RTL8373_MIB_dot3OutPauseFrames,

    RTL8373_MIB_etherStatsDropEvents,

    RTL8373_MIB_tx_ethBroadcastPkts,
    RTL8373_MIB_tx_ethMulticastPkts,
    RTL8373_MIB_tx_ethCRCAlignErrors,
    RTL8373_MIB_rx_ethCRCAlignErrors,
    RTL8373_MIB_tx_ethUndersizePkts,
    RTL8373_MIB_rx_ethUndersizePkts,
    RTL8373_MIB_tx_ethOversizePkts,
    RTL8373_MIB_rx_ethOversizePkts,
    RTL8373_MIB_tx_ethFragments,
    RTL8373_MIB_rx_ethFragments,
    RTL8373_MIB_tx_ethJabbers,
    RTL8373_MIB_rx_ethJabbers,
    RTL8373_MIB_tx_ethCollisions,

    RTL8373_MIB_tx_ethPkts64Octets,
    RTL8373_MIB_rx_ethPkts64Octets,
    RTL8373_MIB_tx_ethPkts65to127Octets,
    RTL8373_MIB_rx_ethPkts65to127Octets,
    RTL8373_MIB_tx_ethPkts128to255Octets,
    RTL8373_MIB_rx_ethPkts128to255Octets,
    RTL8373_MIB_tx_ethPkts256to511Octets,
    RTL8373_MIB_rx_ethPkts256to511Octets,
    RTL8373_MIB_tx_ethPkts512to1023Octets,
    RTL8373_MIB_rx_ethPkts512to1023Octets,
    RTL8373_MIB_tx_Pkts1024to1518Oct,
    RTL8373_MIB_rx_Pkts1024to1518Oct,

    RTL8373_MIB_rx_ethUndersizedropPkts,
    RTL8373_MIB_tx_Pkts1519toMaxOct,
    RTL8373_MIB_rx_Pkts1519toMaxOct,
    RTL8373_MIB_tx_ethPktsOverMaxOctets,
    RTL8373_MIB_rx_ethPktsOverMaxOctets,

    RTL8373_MIB_tx_FlexPktsSET1,
    RTL8373_MIB_rx_FlexPktsSET1,
    RTL8373_MIB_tx_FlexPktsCRCSET1,
    RTL8373_MIB_rx_FlexPktsCRCSET1,
    RTL8373_MIB_tx_FlexPktsSET0,
    RTL8373_MIB_rx_FlexPktsSET0,
    RTL8373_MIB_tx_FlexPktsCRSET0C,
    RTL8373_MIB_rx_FlexPktsCRSET0C,

    RTL8373_MIB_lengthFieldError,
    RTL8373_MIB_falseCarrieimes,
    RTL8373_MIB_underSizeOctets,
    RTL8373_MIB_framingErrors,

    RTL8373_MIB_rxMacDiscards,
    RTL8373_MIB_rxMacIPGShortDropRT,

    RTL8373_MIB_dot1dTpLearnEntryDisc,

    RTL8373_MIB_egrQueue7DropPktRT,
    RTL8373_MIB_egrQueue6DropPktRT,
    RTL8373_MIB_egrQueue5DropPktRT,
    RTL8373_MIB_egrQueue4DropPktRT,
    RTL8373_MIB_egrQueue3DropPktRT,
    RTL8373_MIB_egrQueue2DropPktRT,
    RTL8373_MIB_egrQueue1DropPktRT,
    RTL8373_MIB_egrQueue0DropPktRT,

    RTL8373_MIB_egrQueue7OutPktRT,
    RTL8373_MIB_egrQueue6OutPktRT,
    RTL8373_MIB_egrQueue5OutPktRT,
    RTL8373_MIB_egrQueue4OutPktRT,
    RTL8373_MIB_egrQueue3OutPktRT,
    RTL8373_MIB_egrQueue2OutPktRT,
    RTL8373_MIB_egrQueue1OutPktRT,
    RTL8373_MIB_egrQueue0OutPktRT,

    RTL8373_MIB_TxGoodCnt,
    RTL8373_MIB_RxGoodCnt,
    RTL8373_MIB_RxErrorCnt,
    RTL8373_MIB_TxErrorCnt,

    RTL8373_MIB_TxGoodCnt_phy,
    RTL8373_MIB_RxGoodCnt_phy,
    RTL8373_MIB_RxErrorCnt_phy,
    RTL8373_MIB_TxErrorCnt_phy,

    RTL8373_MIB_END,

} rtk_stat_port_type_t;

struct rtl8373_mib_counter {
	u32 offset;
	u32 length;
	const char *name;
};

#define RTL8373_MAKE_MIB_COUNTER(_offset, _length, _name) \
		[RTL8373_MIB_ ## _name] = { _offset, _length, #_name }

int rtl8373_mib_counter_read(struct realtek_smi *priv, int port,
                                     u32 offset, u32 length, u64 *mibvalue);
void rtl8373_get_ethtool_stats(struct dsa_switch *ds, int port, u64 *data);

void rtl8373_get_strings(struct dsa_switch *ds, int port, u32 stringset,
			 uint8_t *data);

int rtl8373_get_sset_count(struct dsa_switch *ds, int port, int sset);
#endif
