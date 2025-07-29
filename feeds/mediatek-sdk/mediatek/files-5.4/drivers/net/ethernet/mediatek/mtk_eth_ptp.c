// SPDX-License-Identifier: GPL-2.0-only
/*
 * IEEE1588v2 PTP support for MediaTek ETH device.
 *
 * Copyright (c) 2024 MediaTek Inc.
 * Authors: Bo-Cun Chen <bc-bocun.chen@mediatek.com>
 */

#include <linux/if_vlan.h>
#include <linux/jiffies.h>
#include <linux/net_tstamp.h>
#include <linux/ptp_classify.h>

#include "mtk_eth_soc.h"

/* Values for the messageType field */
#define SYNC			0x0
#define DELAY_REQ		0x1
#define PDELAY_REQ		0x2
#define PDELAY_RESP		0x3
#define FOLLOW_UP		0x8
#define DELAY_RESP		0x9
#define PDELAY_RESP_FOLLOW_UP	0xA
#define ANNOUNCE		0xB

static int mtk_ptp_hwtstamp_enable(struct net_device *dev, int hwtstamp)
{
	struct mtk_mac *mac = netdev_priv(dev);
	struct mtk_eth *eth = mac->hw;

	mtk_m32(eth, CSR_HW_TS_EN(mac->id),
		hwtstamp ? CSR_HW_TS_EN(mac->id) : 0, MAC_TS_MAC_CFG);

	return 0;
}

static int mtk_ptp_hwtstamp_get_t1(struct mtk_mac *mac, u16 seqid, struct timespec64 *ts)
{
	struct mtk_eth *eth = mac->hw;
	u32 sid[2], dw[4];
	int i;

	sid[0] = mtk_r32(eth, MAC_TS_T1_SID1(mac->id));
	for (i = 0; i < 4; i++)
		dw[i] = mtk_r32(eth, MAC_TS_T1_DW(mac->id) + i * 4);
	sid[1] = mtk_r32(eth, MAC_TS_T1_SID2(mac->id));

	if (seqid != sid[0] || sid[0] != sid[1]) {
		dev_warn(eth->dev, "invalid t1 hwtstamp(%d, %d, %d)!\n",
			 seqid, sid[0], sid[1]);
		return -EINVAL;
	}

	ts->tv_sec = dw[2] | ((u64)dw[3] << 32);
	ts->tv_nsec = dw[1];

	return 0;
}

static int mtk_ptp_hwtstamp_get_t2(struct mtk_mac *mac, u16 seqid, struct timespec64 *ts)
{
	struct mtk_eth *eth = mac->hw;
	u32 sid[2], dw[4];
	int i;

	sid[0] = mtk_r32(eth, MAC_TS_T2_SID1(mac->id));
	for (i = 0; i < 4; i++)
		dw[i] = mtk_r32(eth, MAC_TS_T2_DW(mac->id) + i * 4);
	sid[1] = mtk_r32(eth, MAC_TS_T2_SID2(mac->id));

	if (seqid != sid[0] || sid[0] != sid[1]) {
		dev_warn(eth->dev, "invalid t2 hwtstamp(%d, %d, %d)!\n",
			 seqid, sid[0], sid[1]);
		return -EINVAL;
	}

	ts->tv_sec = dw[2] | ((u64)dw[3] << 32);
	ts->tv_nsec = dw[1];

	return 0;
}

static int mtk_ptp_hwtstamp_get_t3(struct mtk_mac *mac, u16 seqid, struct timespec64 *ts)
{
	struct mtk_eth *eth = mac->hw;
	u32 sid[2], dw[4];
	int i;

	sid[0] = mtk_r32(eth, MAC_TS_T3_SID1(mac->id));
	for (i = 0; i < 4; i++)
		dw[i] = mtk_r32(eth, MAC_TS_T3_DW(mac->id) + i * 4);
	sid[1] = mtk_r32(eth, MAC_TS_T3_SID2(mac->id));

	if (seqid != sid[0] || sid[0] != sid[1]) {
		dev_warn(eth->dev, "invalid t3 hwtstamp(%d, %d, %d)!\n",
			 seqid, sid[0], sid[1]);
		return -EINVAL;
	}

	ts->tv_sec = dw[2] | ((u64)dw[3] << 32);
	ts->tv_nsec = dw[1];

	return 0;
}

static int mtk_ptp_hwtstamp_get_t4(struct mtk_mac *mac, u16 seqid, struct timespec64 *ts)
{
	struct mtk_eth *eth = mac->hw;
	u32 sid[2], dw[4];
	int i;

	sid[0] = mtk_r32(eth, MAC_TS_T4_SID1(mac->id));
	for (i = 0; i < 4; i++)
		dw[i] = mtk_r32(eth, MAC_TS_T4_DW(mac->id) + i * 4);
	sid[1] = mtk_r32(eth, MAC_TS_T4_SID2(mac->id));

	if (seqid != sid[0] || sid[0] != sid[1]) {
		dev_warn(eth->dev, "invalid t4 hwtstamp(%d, %d, %d)!\n",
			 seqid, sid[0], sid[1]);
		return -EINVAL;
	}

	ts->tv_sec = dw[2] | ((u64)dw[3] << 32);
	ts->tv_nsec = dw[1];

	return 0;
}

static void mtk_ptp_hwtstamp_tx_work(struct work_struct *work)
{
	struct mtk_mac *mac = container_of(work, struct mtk_mac, ptp_tx_work);
	struct mtk_eth *eth = mac->hw;
	struct sk_buff *skb = mac->ptp_tx_skb;
	struct skb_shared_hwtstamps shhwtstamps;
	struct timespec64 ts;
	unsigned int ptp_class, offset = 0;
	u8 *data, msgtype;
	u16 seqid;
	int ret;

	if (!skb)
		return;

	ptp_class = mac->ptp_tx_class;
	if (ptp_class & PTP_CLASS_VLAN)
		offset += VLAN_HLEN;

	if ((ptp_class & PTP_CLASS_PMASK) == PTP_CLASS_L2)
		offset += ETH_HLEN;

	data = skb_mac_header(skb);
	seqid = get_unaligned_be16(data + offset + OFF_PTP_SEQUENCE_ID);
	msgtype = data[offset] & 0x0f;
	switch (msgtype) {
	case SYNC:
	case PDELAY_REQ:
		ret = mtk_ptp_hwtstamp_get_t1(mac, seqid, &ts);
		break;
	case DELAY_REQ:
	case PDELAY_RESP:
		ret = mtk_ptp_hwtstamp_get_t3(mac, seqid, &ts);
		break;
	default:
		dev_warn(eth->dev, "unrecognized hwtstamp msgtype (%d)!", msgtype);
		goto out;
	}

	if (ret) {
		if (time_is_before_jiffies(mac->ptp_tx_start +
					   msecs_to_jiffies(500))) {
			dev_warn(eth->dev, "detect %s hwtstamp timeout!",
				 (msgtype == SYNC || msgtype == PDELAY_REQ) ? "t1" : "t3");
			goto out;
		} else {
			schedule_work(&mac->ptp_tx_work);
			return;
		}
	}

	skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;

	memset(&shhwtstamps, 0, sizeof(shhwtstamps));
	shhwtstamps.hwtstamp = ktime_set(ts.tv_sec, ts.tv_nsec);
	skb_tstamp_tx(skb, &shhwtstamps);

out:
	dev_kfree_skb_any(skb);
	mac->ptp_tx_skb = NULL;
	mac->ptp_tx_class = 0;
	mac->ptp_tx_start = 0;
}

int mtk_ptp_hwtstamp_process_tx(struct net_device *dev, struct sk_buff *skb)
{
	struct mtk_mac *mac = netdev_priv(dev);
	unsigned int ptp_class, offset = 0;

	ptp_class = ptp_classify_raw(skb);
	if (ptp_class == PTP_CLASS_NONE)
		return 0;

	if (ptp_class & PTP_CLASS_VLAN)
		offset += VLAN_HLEN;

	if ((ptp_class & PTP_CLASS_PMASK) == PTP_CLASS_L2)
		offset += ETH_HLEN;
	else
		return 0;

	mac->ptp_tx_skb = skb_get(skb);
	mac->ptp_tx_class = ptp_class;
	mac->ptp_tx_start = jiffies;
	schedule_work(&mac->ptp_tx_work);

	return 0;
}

int mtk_ptp_hwtstamp_process_rx(struct net_device *dev, struct sk_buff *skb)
{
	struct mtk_mac *mac = netdev_priv(dev);
	struct mtk_eth *eth = mac->hw;
	struct skb_shared_hwtstamps *shhwtstamps = skb_hwtstamps(skb);
	struct timespec64 ts;
	unsigned int ptp_class, offset = 0;
	int ret;
	u8 *data, msgtype;
	u16 seqid;

	ptp_class = ptp_classify_raw(skb);
	if (ptp_class == PTP_CLASS_NONE)
		return 0;

	if (ptp_class & PTP_CLASS_VLAN)
		offset += VLAN_HLEN;

	if ((ptp_class & PTP_CLASS_PMASK) == PTP_CLASS_L2)
		offset += ETH_HLEN;
	else
		return 0;

	skb_reset_mac_header(skb);
	data = skb_mac_header(skb);
	seqid = get_unaligned_be16(data + offset + OFF_PTP_SEQUENCE_ID);
	msgtype = data[offset] & 0x0f;
	switch (msgtype) {
	case SYNC:
	case PDELAY_REQ:
		ret = mtk_ptp_hwtstamp_get_t2(mac, seqid, &ts);
		break;
	case DELAY_REQ:
	case PDELAY_RESP:
		ret = mtk_ptp_hwtstamp_get_t4(mac, seqid, &ts);
		break;
	default:
		dev_warn(eth->dev, "unrecognized hwtstamp msgtype (%d)!", msgtype);
		return 0;
	}

	if (ret)
		return ret;

	memset(shhwtstamps, 0, sizeof(*shhwtstamps));
	shhwtstamps->hwtstamp = ktime_set(ts.tv_sec, ts.tv_nsec);

	return 0;
}

int mtk_ptp_hwtstamp_set_config(struct net_device *dev, struct ifreq *ifr)
{
	struct mtk_mac *mac = netdev_priv(dev);
	struct mtk_eth *eth = mac->hw;
	struct hwtstamp_config cfg;

	if (!MTK_HAS_CAPS(eth->soc->caps, MTK_HWTSTAMP))
		return -EOPNOTSUPP;

	if (copy_from_user(&cfg, ifr->ifr_data, sizeof(cfg)))
		return -EFAULT;

	/* reserved for future extensions */
	if (cfg.flags)
		return -EINVAL;

	if (cfg.tx_type != HWTSTAMP_TX_OFF && cfg.tx_type != HWTSTAMP_TX_ON &&
	    cfg.tx_type != HWTSTAMP_TX_ONESTEP_SYNC)
		return -ERANGE;

	switch (cfg.rx_filter) {
	case HWTSTAMP_FILTER_NONE:
		eth->rx_ts_enabled = 0;
		break;
	case HWTSTAMP_FILTER_PTP_V2_L2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ:
		eth->rx_ts_enabled = HWTSTAMP_FILTER_PTP_V2_EVENT;
		cfg.rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;
		break;
	default:
		return -ERANGE;
	}

	eth->tx_ts_enabled = cfg.tx_type;

	mtk_ptp_hwtstamp_enable(dev, eth->tx_ts_enabled);

	return copy_to_user(ifr->ifr_data, &cfg, sizeof(cfg)) ? -EFAULT : 0;
}

int mtk_ptp_hwtstamp_get_config(struct net_device *dev, struct ifreq *ifr)
{
	struct mtk_mac *mac = netdev_priv(dev);
	struct mtk_eth *eth = mac->hw;
	struct hwtstamp_config cfg;

	if (!MTK_HAS_CAPS(eth->soc->caps, MTK_HWTSTAMP))
		return -EOPNOTSUPP;

	cfg.flags = 0;
	cfg.tx_type = eth->tx_ts_enabled;
	cfg.rx_filter = eth->rx_ts_enabled;

	return copy_to_user(ifr->ifr_data, &cfg, sizeof(cfg)) ? -EFAULT : 0;
}

static int mtk_ptp_adjfine(struct ptp_clock_info *ptp, long scaled_ppm)
{
	struct mtk_eth *eth = container_of(ptp, struct mtk_eth, ptp_info);
	u64 base, adj;
	u16 data16;
	bool negative;

	if (scaled_ppm) {
		base = 0x4 << 16;
		negative = diff_by_scaled_ppm(base, scaled_ppm, &adj);
		data16 = (u16)adj;

		if (negative)
			mtk_w32(eth,
				FIELD_PREP(CSR_TICK_NANOSECOND, 0x3) |
				FIELD_PREP(CSR_TICK_SUB_NANOSECOND, 0xFFFF - data16),
				MAC_TS_TICK_SUBSECOND);
		else
			mtk_w32(eth,
				FIELD_PREP(CSR_TICK_NANOSECOND, 0x4) |
				FIELD_PREP(CSR_TICK_SUB_NANOSECOND, data16),
				MAC_TS_TICK_SUBSECOND);

		// update tick configuration
		mtk_m32(eth, CSR_TICK_UPDATE, CSR_TICK_UPDATE, MAC_TS_TICK_CTRL);
		mtk_m32(eth, CSR_TICK_UPDATE, 0, MAC_TS_TICK_CTRL);
	}

	return 0;
}

static int mtk_ptp_adjtime(struct ptp_clock_info *ptp, s64 delta)
{
	struct mtk_eth *eth = container_of(ptp, struct mtk_eth, ptp_info);
	struct timespec64 ts = ns_to_timespec64(delta);

	mtk_w32(eth, (ts.tv_nsec >> 0) & 0xFFFFFFFF, MAC_TS_SUBSECOND_FIELD1);
	mtk_w32(eth, (ts.tv_sec >>  0) & 0xFFFFFFFF, MAC_TS_SECOND_FIELD0);
	mtk_w32(eth, (ts.tv_sec >> 32) & 0x0000FFFF, MAC_TS_SECOND_FIELD1);

	// adjust timestamp
	mtk_m32(eth, CSR_TS_ADJUST, CSR_TS_ADJUST, MAC_TS_TIMESTAMP_CTRL);
	mtk_m32(eth, CSR_TS_ADJUST, 0, MAC_TS_TIMESTAMP_CTRL);

	return 0;
}

static int mtk_ptp_gettime64(struct ptp_clock_info *ptp,
			     struct timespec64 *ts)
{
	struct mtk_eth *eth = container_of(ptp, struct mtk_eth, ptp_info);
	unsigned long t_start = jiffies;
	u32 val[4];
	int i;

	mtk_w32(eth, CPU_TRIG, MAC_TS_CPU_TRIG);

	while (1) {
		if (!(mtk_r32(eth, MAC_TS_CPU_TRIG) & CPU_TS_VALID))
			break;
		if (time_after(jiffies, t_start + jiffies_to_msecs(1000))) {
			pr_warn("cpu trigger timeout!");
			return -ETIMEDOUT;
		}
		cond_resched();
	}

	for (i = 0; i < 4; i++)
		val[i] = mtk_r32(eth, MAC_TS_CPU_TS_DW(i));

	ts->tv_sec = val[2] | ((u64)val[3] << 32);
	ts->tv_nsec = val[1];

	return 0;
}

static int mtk_ptp_settime64(struct ptp_clock_info *ptp,
			     const struct timespec64 *ts)
{
	struct mtk_eth *eth = container_of(ptp, struct mtk_eth, ptp_info);

	mtk_w32(eth, (ts->tv_nsec >> 0) & 0xFFFFFFFF, MAC_TS_SUBSECOND_FIELD1);
	mtk_w32(eth, (ts->tv_sec >>  0) & 0xFFFFFFFF, MAC_TS_SECOND_FIELD0);
	mtk_w32(eth, (ts->tv_sec >> 32) & 0x0000FFFF, MAC_TS_SECOND_FIELD1);

	// update timestamp
	mtk_m32(eth, CSR_TS_UPDATE, CSR_TS_UPDATE, MAC_TS_TIMESTAMP_CTRL);
	mtk_m32(eth, CSR_TS_UPDATE, 0, MAC_TS_TIMESTAMP_CTRL);

	return 0;
}

static int mtk_ptp_enable(struct ptp_clock_info *ptp,
			  struct ptp_clock_request *request, int on)
{
	struct mtk_eth *eth = container_of(ptp, struct mtk_eth, ptp_info);

	// enable rx T1/T3 timestamp mask
	mtk_w32(eth, 0x00000077, MAC_TS_RSV);
	// update tick configuration
	mtk_m32(eth, CSR_TICK_UPDATE, CSR_TICK_UPDATE, MAC_TS_TICK_CTRL);
	mtk_m32(eth, CSR_TICK_UPDATE, 0, MAC_TS_TICK_CTRL);
	// enable tick
	mtk_m32(eth, CSR_TICK_RUN, on, MAC_TS_TICK_CTRL);

	return 0;
}

static const struct ptp_clock_info mtk_ptp_caps = {
	.owner		= THIS_MODULE,
	.name		= "mtk_ptp",
	.max_adj	= 24999999,
	.n_alarm	= 0,
	.n_ext_ts	= 0,
	.n_per_out	= 1,
	.n_pins		= 0,
	.pps		= 0,
	.adjfine	= mtk_ptp_adjfine,
	.adjtime	= mtk_ptp_adjtime,
	.gettime64	= mtk_ptp_gettime64,
	.settime64	= mtk_ptp_settime64,
	.enable		= mtk_ptp_enable,
};

int mtk_ptp_clock_init(struct mtk_eth *eth)
{
	struct mtk_mac *mac;
	int i;

	eth->ptp_info = mtk_ptp_caps;
	eth->ptp_clock = ptp_clock_register(&eth->ptp_info,
					    eth->dev);
	if (IS_ERR(eth->ptp_clock)) {
		eth->ptp_clock = NULL;
		return -EINVAL;
	}

	for (i = 0; i < MTK_MAX_DEVS; i++) {
		mac = eth->mac[i];
		if (!mac)
			continue;

		INIT_WORK(&mac->ptp_tx_work, mtk_ptp_hwtstamp_tx_work);
	}

	mtk_ptp_enable(&eth->ptp_info, NULL, 1);

	return 0;
}

int mtk_ptp_clock_deinit(struct mtk_eth *eth)
{
	struct mtk_mac *mac;
	int i;

	for (i = 0; i < MTK_MAX_DEVS; i++) {
		mac = eth->mac[i];
		if (!mac)
			continue;

		cancel_work_sync(&mac->ptp_tx_work);
		dev_kfree_skb_any(mac->ptp_tx_skb);
		mac->ptp_tx_skb = NULL;
		mac->ptp_tx_class = 0;
		mac->ptp_tx_start = 0;
	}

	mtk_ptp_enable(&eth->ptp_info, NULL, 0);

	ptp_clock_unregister(eth->ptp_clock);

	return 0;
}
