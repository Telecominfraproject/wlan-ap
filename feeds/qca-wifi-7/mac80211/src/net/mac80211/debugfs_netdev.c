// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2006	Jiri Benc <jbenc@suse.cz>
 * Copyright 2007	Johannes Berg <johannes@sipsolutions.net>
 * Copyright (C) 2020-2023 Intel Corporation
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <net/mac80211.h>
#include <net/cfg80211.h>
#include "ieee80211_i.h"
#include "rate.h"
#include "debugfs.h"
#include "debugfs_netdev.h"
#include "driver-ops.h"

const char *rx_drop_reason_strings[RX_DROP_REASON_MAX] = {
		[RX_DROP_MIC_FAIL] = "mic_fail",
		[RX_DROP_REPLAY] = "replay",
		[RX_DROP_BAD_MMIE] = "bad_mmie",
		[RX_DROP_DUP] = "dup",
		[RX_DROP_SPURIOUS] = "spurious",
		[RX_DROP_DECRYPT_FAIL] = "decrypt_fail",
		[RX_DROP_NO_KEY_ID] = "no_keyid",
		[RX_DROP_BAD_CIPHER] = "bad_cipher",
		[RX_DROP_OOM] = "OOM",
		[RX_DROP_NONSEQ_PN] = "nonseq_pn",
		[RX_DROP_BAD_KEY_COLOR] = "bad_key_color",
		[RX_DROP_BAD_4ADDR] = "bad_4addr",
		[RX_DROP_BAD_AMSDU] = "bad_amsdu",
		[RX_DROP_BAD_AMSDU_CIPHER] = "Bad amsdu cipher",
		[RX_DROP_INVALID_8023] = "Invalid 8023",
		[RX_DROP_RUNT_ACTION] = "Runt action",
		[RX_DROP_UNPROT_ACTION] = "Unprot action",
		[RX_DROP_ACTION_UNKNOWN_SRC] = "Unknown src",
		[RX_DROP_REJECTED_ACTION_RESPONSE] = "Rej action resp",
		[RX_DROP_EXPECT_DEFRAG_PROT] = "defrag port",
		[RX_DROP_WEP_DEC_FAIL] = "wep dec fail",
		[RX_DROP_NO_IV] = "No IV",
		[RX_DROP_NO_ICV] = "No ICV",
		[RX_DROP_AP_RX_GROUPCAST] = "RX groupcast",
		[RX_DROP_SHORT_MMIC] = "short mmic",
		[RX_DROP_MMIC_FAIL] = "mmic fail",
		[RX_DROP_SHORT_TKIP] = "short tkip",
		[RX_DROP_TKIP_FAIL] = "tkip fail",
		[RX_DROP_SHORT_CCMP] = "short ccmp",
		[RX_DROP_SHORT_CCMP_MIC] = "short ccmp mic",
		[RX_DROP_SHORT_GCMP] = "short gcmp",
		[RX_DROP_SHORT_GCMP_MIC] = "short gcmp mic",
		[RX_DROP_SHORT_CMAC] = "short cmac",
		[RX_DROP_SHORT_CMAC256] = "cmac256",
		[RX_DROP_SHORT_GMAC] = "short gmac",
		[RX_DROP_UNEXPECTED_4ADDR_FRAME] = "unexpected 4addr",
		[RX_DROP_BAD_BCN_KEYIDX] = "bcn keyidx",
		[RX_DROP_BAD_MGMT_KEYIDX] = "mgmt keyidx",
		[RX_DROP_INVALID_SKB] = "invalid skb",
		[RX_DROP_SHORT_HDR] = "short hdr",
		[RX_DROP_NO_PUBSTA] = "no pubsta",
		[RX_DROP_NO_SET_STA] = "no set sta",
		[RX_DROP_NO_FAST_RX] = "no fast rx",
		[RX_DROP_NO_LINK_STA] = "no link sta",
		[RX_DROP_FRAME_ERR] = "frame_err",
};

const char *tx_drop_reason_strings[TX_DROP_REASON_MAX] = {
		[TX_DROP_SDATA_STATE] = "sdata_state",
		[TX_DROP_SKB_SANITY_CHECK_FAIL] = "skb_sanity_check_fail",
		[TX_DROP_SKB_RESIZE_FAIL] = "skb_resize_fail",
		[TX_DROP_SKB_QUEUE_FULL] = "skb_queue full",
		[TX_DROP_RA_STA_FAIL] = "ra_sta_failure",
		[TX_DROP_HW_CHECK_FAIL] = "hw_check_failure",
		[TX_DROP_TX_SKB_FIXUP_FAIL] = "skb_fixup_failure",
		[TX_DROP_BAD_LINK_ID] = "bad_link_id",
		[TX_DROP_BAD_CHANCTX_CONF] = "bad_chanctx_conf",
		[TX_DROP_BAD_LINK_CONF] = "bad_link_conf",
		[TX_DROP_UNKNOWN_VIF_TYPE] = "unknown_vif_type",
		[TX_DROP_UNAUTHORIZED_STA] = "unauthorized_sta",
		[TX_DROP_QUEUE_PURGE] = "queue_purge",
		[TX_DROP_MISC] = "misc"
};

struct ieee80211_if_read_sdata_data {
	ssize_t (*format)(const struct ieee80211_sub_if_data *, char *, int);
	struct ieee80211_sub_if_data *sdata;
};

static ssize_t ieee80211_if_read_sdata_handler(struct wiphy *wiphy,
					       struct file *file,
					       char *buf,
					       size_t bufsize,
					       void *data)
{
	struct ieee80211_if_read_sdata_data *d = data;

	return d->format(d->sdata, buf, bufsize);
}

static ssize_t ieee80211_if_read_sdata(
	struct file *file,
	char __user *userbuf,
	size_t count, loff_t *ppos,
	ssize_t (*format)(const struct ieee80211_sub_if_data *sdata, char *, int))
{
	struct ieee80211_sub_if_data *sdata = file->private_data;
	struct ieee80211_if_read_sdata_data data = {
		.format = format,
		.sdata = sdata,
	};
	char buf[200];

	return wiphy_locked_debugfs_read(sdata->local->hw.wiphy,
					 file, buf, sizeof(buf),
					 userbuf, count, ppos,
					 ieee80211_if_read_sdata_handler,
					 &data);
}

struct ieee80211_if_write_sdata_data {
	ssize_t (*write)(struct ieee80211_sub_if_data *, const char *, int);
	struct ieee80211_sub_if_data *sdata;
};

static ssize_t ieee80211_if_write_sdata_handler(struct wiphy *wiphy,
						struct file *file,
						char *buf,
						size_t count,
						void *data)
{
	struct ieee80211_if_write_sdata_data *d = data;

	return d->write(d->sdata, buf, count);
}

static ssize_t ieee80211_if_write_sdata(
	struct file *file,
	const char __user *userbuf,
	size_t count, loff_t *ppos,
	ssize_t (*write)(struct ieee80211_sub_if_data *sdata, const char *, int))
{
	struct ieee80211_sub_if_data *sdata = file->private_data;
	struct ieee80211_if_write_sdata_data data = {
		.write = write,
		.sdata = sdata,
	};
	char buf[64];

	return wiphy_locked_debugfs_write(sdata->local->hw.wiphy,
					  file, buf, sizeof(buf),
					  userbuf, count,
					  ieee80211_if_write_sdata_handler,
					  &data);
}

struct ieee80211_if_read_link_data {
	ssize_t (*format)(const struct ieee80211_link_data *, char *, int);
	struct ieee80211_link_data *link;
};

static ssize_t ieee80211_if_read_link_handler(struct wiphy *wiphy,
					      struct file *file,
					      char *buf,
					      size_t bufsize,
					      void *data)
{
	struct ieee80211_if_read_link_data *d = data;

	return d->format(d->link, buf, bufsize);
}

static ssize_t ieee80211_if_read_link(
	struct file *file,
	char __user *userbuf,
	size_t count, loff_t *ppos,
	ssize_t (*format)(const struct ieee80211_link_data *link, char *, int))
{
	struct ieee80211_link_data *link = file->private_data;
	struct ieee80211_if_read_link_data data = {
		.format = format,
		.link = link,
	};
	char buf[200];

	return wiphy_locked_debugfs_read(link->sdata->local->hw.wiphy,
					 file, buf, sizeof(buf),
					 userbuf, count, ppos,
					 ieee80211_if_read_link_handler,
					 &data);
}

struct ieee80211_if_write_link_data {
	ssize_t (*write)(struct ieee80211_link_data *, const char *, int);
	struct ieee80211_link_data *link;
};

static ssize_t ieee80211_if_write_link_handler(struct wiphy *wiphy,
					       struct file *file,
					       char *buf,
					       size_t count,
					       void *data)
{
	struct ieee80211_if_write_sdata_data *d = data;

	return d->write(d->sdata, buf, count);
}

static ssize_t ieee80211_if_write_link(
	struct file *file,
	const char __user *userbuf,
	size_t count, loff_t *ppos,
	ssize_t (*write)(struct ieee80211_link_data *link, const char *, int))
{
	struct ieee80211_link_data *link = file->private_data;
	struct ieee80211_if_write_link_data data = {
		.write = write,
		.link = link,
	};
	char buf[64];

	return wiphy_locked_debugfs_write(link->sdata->local->hw.wiphy,
					  file, buf, sizeof(buf),
					  userbuf, count,
					  ieee80211_if_write_link_handler,
					  &data);
}

#define IEEE80211_IF_FMT(name, type, field, format_string)		\
static ssize_t ieee80211_if_fmt_##name(					\
	const type *data, char *buf,					\
	int buflen)							\
{									\
	return scnprintf(buf, buflen, format_string, data->field);	\
}
#define IEEE80211_IF_FMT_DEC(name, type, field)				\
		IEEE80211_IF_FMT(name, type, field, "%d\n")
#define IEEE80211_IF_FMT_HEX(name, type, field)				\
		IEEE80211_IF_FMT(name, type, field, "%#x\n")
#define IEEE80211_IF_FMT_LHEX(name, type, field)			\
		IEEE80211_IF_FMT(name, type, field, "%#lx\n")

#define IEEE80211_IF_FMT_HEXARRAY(name, type, field)			\
static ssize_t ieee80211_if_fmt_##name(					\
	const type *data,						\
	char *buf, int buflen)						\
{									\
	char *p = buf;							\
	int i;								\
	for (i = 0; i < sizeof(data->field); i++) {			\
		p += scnprintf(p, buflen + buf - p, "%.2x ",		\
				 data->field[i]);			\
	}								\
	p += scnprintf(p, buflen + buf - p, "\n");			\
	return p - buf;							\
}

#define IEEE80211_IF_FMT_ATOMIC(name, type, field)			\
static ssize_t ieee80211_if_fmt_##name(					\
	const type *data,						\
	char *buf, int buflen)						\
{									\
	return scnprintf(buf, buflen, "%d\n", atomic_read(&data->field));\
}

#define IEEE80211_IF_FMT_MAC(name, type, field)				\
static ssize_t ieee80211_if_fmt_##name(					\
	const type *data, char *buf,					\
	int buflen)							\
{									\
	return scnprintf(buf, buflen, "%pM\n", data->field);		\
}

#define IEEE80211_IF_FMT_JIFFIES_TO_MS(name, type, field)		\
static ssize_t ieee80211_if_fmt_##name(					\
	const type *data,						\
	char *buf, int buflen)						\
{									\
	return scnprintf(buf, buflen, "%d\n",				\
			 jiffies_to_msecs(data->field));		\
}

#if LINUX_VERSION_IS_GEQ(6,13,0)
#define _IEEE80211_IF_FILE_OPS(name, _read, _write)			\
static const struct debugfs_short_fops name##_ops = {				\
	.read = (_read),						\
	.write = (_write),						\
	.llseek = generic_file_llseek,					\
}
#else
#define _IEEE80211_IF_FILE_OPS(name, _read, _write)                     \
static const struct file_operations name##_ops = {                           \
        .read = (_read),                                                \
        .write = (_write),                                              \
	.open = simple_open,						\
        .llseek = generic_file_llseek,                                  \
}
#endif

#define _IEEE80211_IF_FILE_R_FN(name)					\
static ssize_t ieee80211_if_read_##name(struct file *file,		\
					char __user *userbuf,		\
					size_t count, loff_t *ppos)	\
{									\
	return ieee80211_if_read_sdata(file,				\
				       userbuf, count, ppos,		\
				       ieee80211_if_fmt_##name);	\
}

#define _IEEE80211_IF_FILE_W_FN(name)					\
static ssize_t ieee80211_if_write_##name(struct file *file,		\
					 const char __user *userbuf,	\
					 size_t count, loff_t *ppos)	\
{									\
	return ieee80211_if_write_sdata(file, userbuf,			\
					count, ppos,			\
					ieee80211_if_parse_##name);	\
}

#define IEEE80211_IF_FILE_R(name)					\
	_IEEE80211_IF_FILE_R_FN(name)					\
	_IEEE80211_IF_FILE_OPS(name, ieee80211_if_read_##name, NULL)

#define IEEE80211_IF_FILE_W(name)					\
	_IEEE80211_IF_FILE_W_FN(name)					\
	_IEEE80211_IF_FILE_OPS(name, NULL, ieee80211_if_write_##name)

#define IEEE80211_IF_FILE_RW(name)					\
	_IEEE80211_IF_FILE_R_FN(name)					\
	_IEEE80211_IF_FILE_W_FN(name)					\
	_IEEE80211_IF_FILE_OPS(name, ieee80211_if_read_##name,		\
			       ieee80211_if_write_##name)

#define IEEE80211_IF_FILE(name, field, format)				\
	IEEE80211_IF_FMT_##format(name, struct ieee80211_sub_if_data, field) \
	IEEE80211_IF_FILE_R(name)

#define _IEEE80211_IF_LINK_R_FN(name)					\
static ssize_t ieee80211_if_read_##name(struct file *file,		\
					char __user *userbuf,		\
					size_t count, loff_t *ppos)	\
{									\
	return ieee80211_if_read_link(file,				\
				      userbuf, count, ppos,		\
				      ieee80211_if_fmt_##name);	\
}

#define _IEEE80211_IF_LINK_W_FN(name)					\
static ssize_t ieee80211_if_write_##name(struct file *file,		\
					 const char __user *userbuf,	\
					 size_t count, loff_t *ppos)	\
{									\
	return ieee80211_if_write_link(file, userbuf,			\
				       count, ppos,			\
				       ieee80211_if_parse_##name);	\
}

#define IEEE80211_IF_LINK_FILE_R(name)					\
	_IEEE80211_IF_LINK_R_FN(name)					\
	_IEEE80211_IF_FILE_OPS(link_##name, ieee80211_if_read_##name, NULL)

#define IEEE80211_IF_LINK_FILE_W(name)					\
	_IEEE80211_IF_LINK_W_FN(name)					\
	_IEEE80211_IF_FILE_OPS(link_##name, NULL, ieee80211_if_write_##name)

#define IEEE80211_IF_LINK_FILE_RW(name)					\
	_IEEE80211_IF_LINK_R_FN(name)					\
	_IEEE80211_IF_LINK_W_FN(name)					\
	_IEEE80211_IF_FILE_OPS(link_##name, ieee80211_if_read_##name,	\
			       ieee80211_if_write_##name)

#define IEEE80211_IF_LINK_FILE(name, field, format)				\
	IEEE80211_IF_FMT_##format(name, struct ieee80211_link_data, field) \
	IEEE80211_IF_LINK_FILE_R(name)

/* common attributes */
IEEE80211_IF_FILE(rc_rateidx_mask_2ghz, rc_rateidx_mask[NL80211_BAND_2GHZ],
		  HEX);
IEEE80211_IF_FILE(rc_rateidx_mask_5ghz, rc_rateidx_mask[NL80211_BAND_5GHZ],
		  HEX);
IEEE80211_IF_FILE(rc_rateidx_mcs_mask_2ghz,
		  rc_rateidx_mcs_mask[NL80211_BAND_2GHZ], HEXARRAY);
IEEE80211_IF_FILE(rc_rateidx_mcs_mask_5ghz,
		  rc_rateidx_mcs_mask[NL80211_BAND_5GHZ], HEXARRAY);

static ssize_t ieee80211_if_fmt_rc_rateidx_vht_mcs_mask_2ghz(
				const struct ieee80211_sub_if_data *sdata,
				char *buf, int buflen)
{
	int i, len = 0;
	const u16 *mask = sdata->rc_rateidx_vht_mcs_mask[NL80211_BAND_2GHZ];

	for (i = 0; i < NL80211_VHT_NSS_MAX; i++)
		len += scnprintf(buf + len, buflen - len, "%04x ", mask[i]);
	len += scnprintf(buf + len, buflen - len, "\n");

	return len;
}

IEEE80211_IF_FILE_R(rc_rateidx_vht_mcs_mask_2ghz);

static ssize_t ieee80211_if_fmt_rc_rateidx_vht_mcs_mask_5ghz(
				const struct ieee80211_sub_if_data *sdata,
				char *buf, int buflen)
{
	int i, len = 0;
	const u16 *mask = sdata->rc_rateidx_vht_mcs_mask[NL80211_BAND_5GHZ];

	for (i = 0; i < NL80211_VHT_NSS_MAX; i++)
		len += scnprintf(buf + len, buflen - len, "%04x ", mask[i]);
	len += scnprintf(buf + len, buflen - len, "\n");

	return len;
}

IEEE80211_IF_FILE_R(rc_rateidx_vht_mcs_mask_5ghz);

IEEE80211_IF_FILE(flags, flags, HEX);
IEEE80211_IF_FILE(state, state, LHEX);
IEEE80211_IF_LINK_FILE(txpower, conf->txpower, DEC);
IEEE80211_IF_LINK_FILE(ap_power_level, ap_power_level, DEC);
IEEE80211_IF_LINK_FILE(user_power_level, user_power_level, DEC);

static ssize_t
ieee80211_if_fmt_hw_queues(const struct ieee80211_sub_if_data *sdata,
			   char *buf, int buflen)
{
	int len;

	len = scnprintf(buf, buflen, "AC queues: VO:%d VI:%d BE:%d BK:%d\n",
			sdata->vif.hw_queue[IEEE80211_AC_VO],
			sdata->vif.hw_queue[IEEE80211_AC_VI],
			sdata->vif.hw_queue[IEEE80211_AC_BE],
			sdata->vif.hw_queue[IEEE80211_AC_BK]);

	if (sdata->vif.type == NL80211_IFTYPE_AP)
		len += scnprintf(buf + len, buflen - len, "cab queue: %d\n",
				 sdata->vif.cab_queue);

	return len;
}
IEEE80211_IF_FILE_R(hw_queues);

static ssize_t ieee80211_if_fmt_mac_tid_stats_read(struct file *file,
						   char __user *userbuf,
						   size_t count,
						   loff_t *ppos)
{
	struct ieee80211_sub_if_data *sdata = file->private_data;
	struct txrx_stats *total_stats;
	u8 tid, reason;
	int cpu;
	const int size = 15000;
	int len = 0, retval;
	char *buf;

	buf = vmalloc(size);
	if (!buf)
		return -ENOMEM;

	total_stats = kzalloc(sizeof(*total_stats), GFP_KERNEL);
	if (!total_stats) {
		vfree(buf);
		return -ENOMEM;
	}

	len = scnprintf(buf + len, size - len, "\n\t\tMAC80211 RX STATS\t\t\n");

	len += scnprintf(buf + len, size - len,
			 "TID \t packets bytes netif_pkts forwarded_pkts fast_path_pkts multicast_pkts local_stack_pkts queue_pkts monitor_pkts\n");

	for_each_possible_cpu(cpu) {
		struct pcpu_txrx_stats *txrx_stats = per_cpu_ptr(sdata->txrx_stats, cpu);

		u64_stats_update_begin(&txrx_stats->syncp);

		for (u8 tid = 0; tid < IEEE80211_NUM_TIDS; tid++) {
			total_stats->tid_stats[tid].rx_packets +=
				txrx_stats->tid_stats[tid].rx_packets;
			total_stats->tid_stats[tid].rx_bytes +=
				txrx_stats->tid_stats[tid].rx_bytes;
			total_stats->tid_stats[tid].rx_netif_pkts +=
				txrx_stats->tid_stats[tid].rx_netif_pkts;
			total_stats->tid_stats[tid].rx_forwarded_pkts +=
				txrx_stats->tid_stats[tid].rx_forwarded_pkts;
			total_stats->tid_stats[tid].rx_fast_path_pkts +=
				txrx_stats->tid_stats[tid].rx_fast_path_pkts;
			total_stats->tid_stats[tid].rx_multicast_pkts +=
				txrx_stats->tid_stats[tid].rx_multicast_pkts;
			total_stats->tid_stats[tid].rx_local_stack_pkts +=
				txrx_stats->tid_stats[tid].rx_local_stack_pkts;
			total_stats->tid_stats[tid].rx_queue_pkts +=
				txrx_stats->tid_stats[tid].rx_queue_pkts;
			total_stats->tid_stats[tid].rx_monitor_pkts +=
				txrx_stats->tid_stats[tid].rx_monitor_pkts;
			total_stats->tid_stats[tid].tx_eth_pkts +=
				txrx_stats->tid_stats[tid].tx_eth_pkts;
			total_stats->tid_stats[tid].tx_eth_pkts_bytes +=
				txrx_stats->tid_stats[tid].tx_eth_pkts_bytes;
			total_stats->tid_stats[tid].fast_tx_pkts +=
				txrx_stats->tid_stats[tid].fast_tx_pkts;
			total_stats->tid_stats[tid].fast_tx_pkts_bytes +=
				txrx_stats->tid_stats[tid].fast_tx_pkts_bytes;
			total_stats->tid_stats[tid].tx_nwifi_pkts +=
				txrx_stats->tid_stats[tid].tx_nwifi_pkts;
			total_stats->tid_stats[tid].tx_nwifi_pkts_bytes +=
				txrx_stats->tid_stats[tid].tx_nwifi_pkts_bytes;
			total_stats->tid_stats[tid].tx_monitor_pkts +=
				txrx_stats->tid_stats[tid].tx_monitor_pkts;
			total_stats->tid_stats[tid].tx_monitor_pkts_bytes +=
				txrx_stats->tid_stats[tid].tx_monitor_pkts_bytes;
			total_stats->tid_stats[tid].tx_multicast_pkts +=
				txrx_stats->tid_stats[tid].tx_multicast_pkts;
			total_stats->tid_stats[tid].tx_multicast_pkts_bytes +=
				txrx_stats->tid_stats[tid].tx_multicast_pkts_bytes;

			for (u8 reason = 0; reason < RX_DROP_REASON_MAX; reason++) {
				total_stats->tid_stats[tid].rx_drop_stats[reason] +=
					txrx_stats->tid_stats[tid].rx_drop_stats[reason];
			}

			for (u8 reason = 0; reason < TX_DROP_REASON_MAX; reason++) {
				total_stats->tid_stats[tid].tx_drop_stats[reason] +=
					txrx_stats->tid_stats[tid].tx_drop_stats[reason];
			}
		}
		u64_stats_update_end(&txrx_stats->syncp);
	}

	for (tid = 0; tid < IEEE80211_NUM_TIDS; tid++) {
		len += scnprintf(buf + len, size - len, "%-8d %-8llu %-8llu %-12llu %-14llu %-14llu %-14llu %-16llu %-10llu %-12llu\n", tid,
				 total_stats->tid_stats[tid].rx_packets,
				 total_stats->tid_stats[tid].rx_bytes,
				 total_stats->tid_stats[tid].rx_netif_pkts,
				 total_stats->tid_stats[tid].rx_forwarded_pkts,
				 total_stats->tid_stats[tid].rx_fast_path_pkts,
				 total_stats->tid_stats[tid].rx_multicast_pkts,
				 total_stats->tid_stats[tid].rx_local_stack_pkts,
				 total_stats->tid_stats[tid].rx_queue_pkts,
				 total_stats->tid_stats[tid].rx_monitor_pkts);
	}

	len += scnprintf(buf + len, size - len, "\n\t\tMAC80211 RX DROP STATS\t\t\n");

	for (tid = 0; tid < IEEE80211_NUM_TIDS; tid++) {
		len += scnprintf(buf + len, size - len, "TID %d\t", tid);
		for (reason = 0; reason < RX_DROP_REASON_MAX; reason++) {
			if (total_stats->tid_stats[tid].rx_drop_stats[reason])
				len += scnprintf(buf + len, size - len, "%d: %llu\t,",
						reason,
						total_stats->tid_stats[tid].rx_drop_stats[reason]);
		}
		len += scnprintf(buf + len, size - len, "\n");
	}

	len += scnprintf(buf + len, size - len, "\n\t\tMAC80211 TX STATS\t\t\n");

#ifdef CPTCFG_MAC80211_DEBUG_COUNTERS
	len += scnprintf(buf + len, size - len,
			"TransmittedFragmentCount: %u\n"
			"MulticastTransmittedFrameCount: %u\n"
			"FailedCount: %u\n"
			"RetryCount: %u\n"
			"MultipleRetryCount: %u\n"
			"FrameDuplicateCount: %u\n"
			"TransmittedFrameCount: %u\n",
			sdata->local->dot11TransmittedFragmentCount,
			sdata->local->dot11MulticastTransmittedFrameCount,
			sdata->local->dot11FailedCount,
			sdata->local->dot11RetryCount,
			sdata->local->dot11MultipleRetryCount,
			sdata->local->dot11FrameDuplicateCount,
			sdata->local->dot11TransmittedFrameCount);
#endif
	len += scnprintf(buf + len, size - len,
			 "TID\t packets  bytes  eth_pkts  eth_bytes  fast_tx_pkts  fast_tx_bytes  nwifi_pkts  nwifi_bytes mon_pkts  mon_bytes  mc_pkts  mc_bytes\n");

	for (tid = 0; tid < IEEE80211_NUM_TIDS; tid++) {
		len += scnprintf(buf + len, size - len,
				 "%-8d %-8llu %-8llu %-10llu %-10llu %-12llu %-12llu %-10llu %-10llu %-10llu %-10llu %-10llu %-10llu\n",
				 tid,
				 total_stats->tid_stats[tid].tx_eth_pkts +
				 total_stats->tid_stats[tid].fast_tx_pkts +
				 total_stats->tid_stats[tid].tx_nwifi_pkts +
				 total_stats->tid_stats[tid].tx_monitor_pkts,
				 total_stats->tid_stats[tid].tx_eth_pkts_bytes +
				 total_stats->tid_stats[tid].fast_tx_pkts_bytes +
				 total_stats->tid_stats[tid].tx_nwifi_pkts_bytes +
				 total_stats->tid_stats[tid].tx_monitor_pkts_bytes,
				 total_stats->tid_stats[tid].tx_eth_pkts,
				 total_stats->tid_stats[tid].tx_eth_pkts_bytes,
				 total_stats->tid_stats[tid].fast_tx_pkts,
				 total_stats->tid_stats[tid].fast_tx_pkts_bytes,
				 total_stats->tid_stats[tid].tx_nwifi_pkts,
				 total_stats->tid_stats[tid].tx_nwifi_pkts_bytes,
				 total_stats->tid_stats[tid].tx_monitor_pkts,
				 total_stats->tid_stats[tid].tx_monitor_pkts_bytes,
				 total_stats->tid_stats[tid].tx_multicast_pkts,
				 total_stats->tid_stats[tid].tx_multicast_pkts_bytes);
		}

	len += scnprintf(buf + len, size - len, "\n\t\tMAC80211 TX TID DROP STATS\t\t\n");

	for (tid = 0; tid < IEEE80211_NUM_TIDS; tid++) {
		len += scnprintf(buf + len, size - len, "TID %d\t", tid);
		for (reason = 0; reason < TX_DROP_REASON_MAX; reason++) {
			if (total_stats->tid_stats[tid].tx_drop_stats[reason])
				len += scnprintf(buf + len, size - len, "%d: %llu\t,",
						 reason,
						 total_stats->tid_stats[tid].tx_drop_stats[reason]);
		}
		len += scnprintf(buf + len, size - len, "\n");
	}

	if (len > size)
		len = size;

	retval = simple_read_from_buffer(userbuf, count, ppos, buf, len);
	vfree(buf);
	kfree(total_stats);

	return retval;
}

static const struct file_operations mac_tid_stats_ops = {
	.read = ieee80211_if_fmt_mac_tid_stats_read,
	.open = simple_open,
	.llseek = generic_file_llseek,
};

static ssize_t ieee80211_if_parse_reset_mac_tid_stats(struct ieee80211_sub_if_data *sdata,
						      const char *buf, int buflen)
{
	int cpu;
	u8 tid;

	for_each_possible_cpu(cpu) {
		struct pcpu_txrx_stats *txrx_stats = per_cpu_ptr(sdata->txrx_stats, cpu);

		u64_stats_update_begin(&txrx_stats->syncp);
		for (tid = 0; tid < IEEE80211_NUM_TIDS; tid++)
			memset(&txrx_stats->tid_stats[tid],
			       0, sizeof(struct txrx_tid_stats));
		u64_stats_update_end(&txrx_stats->syncp);
	}
	return buflen;
}

IEEE80211_IF_FILE_W(reset_mac_tid_stats);

static ssize_t ieee80211_if_parse_enable_mac_tid_stats(struct ieee80211_sub_if_data *sdata,
						       const char *buf, int buflen)
{
	struct ieee80211_local *local = sdata->local;
	struct ieee80211_hw *hw = &local->hw;
	int ret;
	u8 val;

	ret = kstrtou8(buf, 0, &val);
	if (ret)
		return ret;

	hw->tid_stats_disable = !val;

	return buflen;
}

IEEE80211_IF_FILE_W(enable_mac_tid_stats);

/* STA attributes */
IEEE80211_IF_FILE(bssid, deflink.u.mgd.bssid, MAC);
IEEE80211_IF_FILE(aid, vif.cfg.aid, DEC);
IEEE80211_IF_FILE(beacon_timeout, u.mgd.beacon_timeout, JIFFIES_TO_MS);

static int ieee80211_set_smps(struct ieee80211_link_data *link,
			      enum ieee80211_smps_mode smps_mode)
{
	struct ieee80211_sub_if_data *sdata = link->sdata;
	struct ieee80211_local *local = sdata->local;

	/* The driver indicated that EML is enabled for the interface, thus do
	 * not allow to override the SMPS state.
	 */
	if (sdata->vif.driver_flags & IEEE80211_VIF_EML_ACTIVE)
		return -EOPNOTSUPP;

	if (!(local->hw.wiphy->features & NL80211_FEATURE_STATIC_SMPS) &&
	    smps_mode == IEEE80211_SMPS_STATIC)
		return -EINVAL;

	/* auto should be dynamic if in PS mode */
	if (!(local->hw.wiphy->features & NL80211_FEATURE_DYNAMIC_SMPS) &&
	    (smps_mode == IEEE80211_SMPS_DYNAMIC ||
	     smps_mode == IEEE80211_SMPS_AUTOMATIC))
		return -EINVAL;

	if (sdata->vif.type != NL80211_IFTYPE_STATION)
		return -EOPNOTSUPP;

	return __ieee80211_request_smps_mgd(link->sdata, link, smps_mode);
}

static const char *smps_modes[IEEE80211_SMPS_NUM_MODES] = {
	[IEEE80211_SMPS_AUTOMATIC] = "auto",
	[IEEE80211_SMPS_OFF] = "off",
	[IEEE80211_SMPS_STATIC] = "static",
	[IEEE80211_SMPS_DYNAMIC] = "dynamic",
};

static ssize_t ieee80211_if_fmt_smps(const struct ieee80211_link_data *link,
				     char *buf, int buflen)
{
	if (link->sdata->vif.type == NL80211_IFTYPE_STATION)
		return snprintf(buf, buflen, "request: %s\nused: %s\n",
				smps_modes[link->u.mgd.req_smps],
				smps_modes[link->smps_mode]);
	return -EINVAL;
}

static ssize_t ieee80211_if_parse_smps(struct ieee80211_link_data *link,
				       const char *buf, int buflen)
{
	enum ieee80211_smps_mode mode;

	for (mode = 0; mode < IEEE80211_SMPS_NUM_MODES; mode++) {
		if (strncmp(buf, smps_modes[mode], buflen) == 0) {
			int err = ieee80211_set_smps(link, mode);
			if (!err)
				return buflen;
			return err;
		}
	}

	return -EINVAL;
}
IEEE80211_IF_LINK_FILE_RW(smps);

ssize_t ieee80211_if_fmt_bmiss_threshold(const struct ieee80211_sub_if_data *sdata,
				     char *buf, int buflen)
{
	return snprintf(buf, buflen, "%u\n", sdata->vif.bss_conf.bmiss_threshold);
}

static ssize_t ieee80211_if_parse_bmiss_threshold(struct ieee80211_sub_if_data *sdata,
						  const char *buf, int buflen)
{
	int ret;
	u8 val;

	ret = kstrtou8(buf, 0, &val);
	if (ret)
		return ret;

	if (!val)
		return -EINVAL;

	sdata->vif.bss_conf.bmiss_threshold = val;

	return buflen;
}

IEEE80211_IF_FILE_RW(bmiss_threshold);

static ssize_t ieee80211_if_fmt_noqueue_enable(const struct ieee80211_sub_if_data *sdata,
					       char *buf, int buflen)
{
	return snprintf(buf, buflen, "%u\n", sdata->vif.noqueue_enable);
}

static ssize_t ieee80211_if_parse_noqueue_enable(struct ieee80211_sub_if_data *sdata,
						 const char *buf, int buflen)
{
	int ret;
	u8 val;

	ret = kstrtou8(buf, 0, &val);
	if (ret)
		return ret;

	if (val > 1)
		return -EINVAL;

	sdata->vif.noqueue_enable = val;

	return buflen;
}

IEEE80211_IF_FILE_RW(noqueue_enable);

static ssize_t ieee80211_if_parse_tkip_mic_test(
	struct ieee80211_sub_if_data *sdata, const char *buf, int buflen)
{
	struct ieee80211_local *local = sdata->local;
	u8 addr[ETH_ALEN];
	struct sk_buff *skb;
	struct ieee80211_hdr *hdr;
	__le16 fc;

	if (!mac_pton(buf, addr))
		return -EINVAL;

	if (!ieee80211_sdata_running(sdata))
		return -ENOTCONN;

	skb = dev_alloc_skb(local->hw.extra_tx_headroom + 24 + 100);
	if (!skb)
		return -ENOMEM;
	skb_reserve(skb, local->hw.extra_tx_headroom);

	hdr = skb_put_zero(skb, 24);
	fc = cpu_to_le16(IEEE80211_FTYPE_DATA | IEEE80211_STYPE_DATA);

	switch (sdata->vif.type) {
	case NL80211_IFTYPE_AP:
		fc |= cpu_to_le16(IEEE80211_FCTL_FROMDS);
		/* DA BSSID SA */
		memcpy(hdr->addr1, addr, ETH_ALEN);
		memcpy(hdr->addr2, sdata->vif.addr, ETH_ALEN);
		memcpy(hdr->addr3, sdata->vif.addr, ETH_ALEN);
		break;
	case NL80211_IFTYPE_STATION:
		fc |= cpu_to_le16(IEEE80211_FCTL_TODS);
		/* BSSID SA DA */
		if (!sdata->u.mgd.associated) {
			dev_kfree_skb(skb);
			return -ENOTCONN;
		}
		memcpy(hdr->addr1, sdata->deflink.u.mgd.bssid, ETH_ALEN);
		memcpy(hdr->addr2, sdata->vif.addr, ETH_ALEN);
		memcpy(hdr->addr3, addr, ETH_ALEN);
		break;
	default:
		dev_kfree_skb(skb);
		return -EOPNOTSUPP;
	}
	hdr->frame_control = fc;

	/*
	 * Add some length to the test frame to make it look bit more valid.
	 * The exact contents does not matter since the recipient is required
	 * to drop this because of the Michael MIC failure.
	 */
	skb_put_zero(skb, 50);

	IEEE80211_SKB_CB(skb)->flags |= IEEE80211_TX_INTFL_TKIP_MIC_FAILURE;

	ieee80211_tx_skb(sdata, skb);

	return buflen;
}
IEEE80211_IF_FILE_W(tkip_mic_test);

static ssize_t ieee80211_if_parse_beacon_loss(
	struct ieee80211_sub_if_data *sdata, const char *buf, int buflen)
{
	if (!ieee80211_sdata_running(sdata) || !sdata->vif.cfg.assoc)
		return -ENOTCONN;

	ieee80211_beacon_loss(&sdata->vif);

	return buflen;
}
IEEE80211_IF_FILE_W(beacon_loss);

static ssize_t ieee80211_if_fmt_uapsd_queues(
	const struct ieee80211_sub_if_data *sdata, char *buf, int buflen)
{
	const struct ieee80211_if_managed *ifmgd = &sdata->u.mgd;

	return snprintf(buf, buflen, "0x%x\n", ifmgd->uapsd_queues);
}

static ssize_t ieee80211_if_parse_uapsd_queues(
	struct ieee80211_sub_if_data *sdata, const char *buf, int buflen)
{
	struct ieee80211_if_managed *ifmgd = &sdata->u.mgd;
	u8 val;
	int ret;

	ret = kstrtou8(buf, 0, &val);
	if (ret)
		return ret;

	if (val & ~IEEE80211_WMM_IE_STA_QOSINFO_AC_MASK)
		return -ERANGE;

	ifmgd->uapsd_queues = val;

	return buflen;
}
IEEE80211_IF_FILE_RW(uapsd_queues);

static ssize_t ieee80211_if_fmt_uapsd_max_sp_len(
	const struct ieee80211_sub_if_data *sdata, char *buf, int buflen)
{
	const struct ieee80211_if_managed *ifmgd = &sdata->u.mgd;

	return snprintf(buf, buflen, "0x%x\n", ifmgd->uapsd_max_sp_len);
}

static ssize_t ieee80211_if_parse_uapsd_max_sp_len(
	struct ieee80211_sub_if_data *sdata, const char *buf, int buflen)
{
	struct ieee80211_if_managed *ifmgd = &sdata->u.mgd;
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	if (val & ~IEEE80211_WMM_IE_STA_QOSINFO_SP_MASK)
		return -ERANGE;

	ifmgd->uapsd_max_sp_len = val;

	return buflen;
}
IEEE80211_IF_FILE_RW(uapsd_max_sp_len);

static ssize_t ieee80211_if_fmt_tdls_wider_bw(
	const struct ieee80211_sub_if_data *sdata, char *buf, int buflen)
{
	const struct ieee80211_if_managed *ifmgd = &sdata->u.mgd;
	bool tdls_wider_bw;

	tdls_wider_bw = ieee80211_hw_check(&sdata->local->hw, TDLS_WIDER_BW) &&
			!ifmgd->tdls_wider_bw_prohibited;

	return snprintf(buf, buflen, "%d\n", tdls_wider_bw);
}

static ssize_t ieee80211_if_parse_tdls_wider_bw(
	struct ieee80211_sub_if_data *sdata, const char *buf, int buflen)
{
	struct ieee80211_if_managed *ifmgd = &sdata->u.mgd;
	u8 val;
	int ret;

	ret = kstrtou8(buf, 0, &val);
	if (ret)
		return ret;

	ifmgd->tdls_wider_bw_prohibited = !val;
	return buflen;
}
IEEE80211_IF_FILE_RW(tdls_wider_bw);

/* AP attributes */
IEEE80211_IF_FILE(num_mcast_sta, u.ap.num_mcast_sta, ATOMIC);
IEEE80211_IF_FILE(num_sta_ps, u.ap.ps.num_sta_ps, ATOMIC);
IEEE80211_IF_FILE(dtim_count, u.ap.ps.dtim_count, DEC);
IEEE80211_IF_FILE(num_mcast_sta_vlan, u.vlan.num_mcast_sta, ATOMIC);

static ssize_t ieee80211_if_fmt_num_buffered_multicast(
	const struct ieee80211_sub_if_data *sdata, char *buf, int buflen)
{
	return scnprintf(buf, buflen, "%u\n",
			 skb_queue_len(&sdata->u.ap.ps.bc_buf));
}
IEEE80211_IF_FILE_R(num_buffered_multicast);

static ssize_t ieee80211_if_fmt_aqm(
	const struct ieee80211_sub_if_data *sdata, char *buf, int buflen)
{
	struct ieee80211_local *local = sdata->local;
	struct txq_info *txqi;
	int len;

	if (!sdata->vif.txq)
		return 0;

	txqi = to_txq_info(sdata->vif.txq);

	spin_lock_bh(&local->fq.lock);
	rcu_read_lock();

	len = scnprintf(buf,
			buflen,
			"ac backlog-bytes backlog-packets new-flows drops marks overlimit collisions tx-bytes tx-packets\n"
			"%u %u %u %u %u %u %u %u %u %u\n",
			txqi->txq.ac,
			txqi->tin.backlog_bytes,
			txqi->tin.backlog_packets,
			txqi->tin.flows,
			txqi->cstats.drop_count,
			txqi->cstats.ecn_mark,
			txqi->tin.overlimit,
			txqi->tin.collisions,
			txqi->tin.tx_bytes,
			txqi->tin.tx_packets);

	rcu_read_unlock();
	spin_unlock_bh(&local->fq.lock);

	return len;
}
IEEE80211_IF_FILE_R(aqm);

IEEE80211_IF_FILE(multicast_to_unicast, u.ap.multicast_to_unicast, HEX);

/* IBSS attributes */
static ssize_t ieee80211_if_fmt_tsf(
	const struct ieee80211_sub_if_data *sdata, char *buf, int buflen)
{
	struct ieee80211_local *local = sdata->local;
	u64 tsf;

	tsf = drv_get_tsf(local, (struct ieee80211_sub_if_data *)sdata);

	return scnprintf(buf, buflen, "0x%016llx\n", (unsigned long long) tsf);
}

static ssize_t ieee80211_if_parse_tsf(
	struct ieee80211_sub_if_data *sdata, const char *buf, int buflen)
{
	struct ieee80211_local *local = sdata->local;
	unsigned long long tsf;
	int ret;
	int tsf_is_delta = 0;

	if (strncmp(buf, "reset", 5) == 0) {
		if (local->ops->reset_tsf) {
			drv_reset_tsf(local, sdata);
			wiphy_info(local->hw.wiphy, "debugfs reset TSF\n");
		}
	} else {
		if (buflen > 10 && buf[1] == '=') {
			if (buf[0] == '+')
				tsf_is_delta = 1;
			else if (buf[0] == '-')
				tsf_is_delta = -1;
			else
				return -EINVAL;
			buf += 2;
		}
		ret = kstrtoull(buf, 10, &tsf);
		if (ret < 0)
			return ret;
		if (tsf_is_delta && local->ops->offset_tsf) {
			drv_offset_tsf(local, sdata, tsf_is_delta * tsf);
			wiphy_info(local->hw.wiphy,
				   "debugfs offset TSF by %018lld\n",
				   tsf_is_delta * tsf);
		} else if (local->ops->set_tsf) {
			if (tsf_is_delta)
				tsf = drv_get_tsf(local, sdata) +
				      tsf_is_delta * tsf;
			drv_set_tsf(local, sdata, tsf);
			wiphy_info(local->hw.wiphy,
				   "debugfs set TSF to %#018llx\n", tsf);
		}
	}

	ieee80211_recalc_dtim(local, sdata);
	return buflen;
}
IEEE80211_IF_FILE_RW(tsf);

static ssize_t ieee80211_if_fmt_valid_links(const struct ieee80211_sub_if_data *sdata,
					    char *buf, int buflen)
{
	return snprintf(buf, buflen, "0x%x\n", sdata->vif.valid_links);
}
IEEE80211_IF_FILE_R(valid_links);

static ssize_t ieee80211_if_fmt_active_links(const struct ieee80211_sub_if_data *sdata,
					     char *buf, int buflen)
{
	return snprintf(buf, buflen, "0x%x\n", sdata->vif.active_links);
}

static ssize_t ieee80211_if_parse_active_links(struct ieee80211_sub_if_data *sdata,
					       const char *buf, int buflen)
{
	u16 active_links;

	if (kstrtou16(buf, 0, &active_links) || !active_links)
		return -EINVAL;

	return ieee80211_set_active_links(&sdata->vif, active_links) ?: buflen;
}
IEEE80211_IF_FILE_RW(active_links);

IEEE80211_IF_LINK_FILE(addr, conf->addr, MAC);

#ifdef CPTCFG_MAC80211_MESH
IEEE80211_IF_FILE(estab_plinks, u.mesh.estab_plinks, ATOMIC);

/* Mesh stats attributes */
IEEE80211_IF_FILE(fwded_mcast, u.mesh.mshstats.fwded_mcast, DEC);
IEEE80211_IF_FILE(fwded_unicast, u.mesh.mshstats.fwded_unicast, DEC);
IEEE80211_IF_FILE(fwded_frames, u.mesh.mshstats.fwded_frames, DEC);
IEEE80211_IF_FILE(dropped_frames_ttl, u.mesh.mshstats.dropped_frames_ttl, DEC);
IEEE80211_IF_FILE(dropped_frames_no_route,
		  u.mesh.mshstats.dropped_frames_no_route, DEC);

/* Mesh parameters */
IEEE80211_IF_FILE(dot11MeshMaxRetries,
		  u.mesh.mshcfg.dot11MeshMaxRetries, DEC);
IEEE80211_IF_FILE(dot11MeshRetryTimeout,
		  u.mesh.mshcfg.dot11MeshRetryTimeout, DEC);
IEEE80211_IF_FILE(dot11MeshConfirmTimeout,
		  u.mesh.mshcfg.dot11MeshConfirmTimeout, DEC);
IEEE80211_IF_FILE(dot11MeshHoldingTimeout,
		  u.mesh.mshcfg.dot11MeshHoldingTimeout, DEC);
IEEE80211_IF_FILE(dot11MeshTTL, u.mesh.mshcfg.dot11MeshTTL, DEC);
IEEE80211_IF_FILE(element_ttl, u.mesh.mshcfg.element_ttl, DEC);
IEEE80211_IF_FILE(auto_open_plinks, u.mesh.mshcfg.auto_open_plinks, DEC);
IEEE80211_IF_FILE(dot11MeshMaxPeerLinks,
		  u.mesh.mshcfg.dot11MeshMaxPeerLinks, DEC);
IEEE80211_IF_FILE(dot11MeshHWMPactivePathTimeout,
		  u.mesh.mshcfg.dot11MeshHWMPactivePathTimeout, DEC);
IEEE80211_IF_FILE(dot11MeshHWMPpreqMinInterval,
		  u.mesh.mshcfg.dot11MeshHWMPpreqMinInterval, DEC);
IEEE80211_IF_FILE(dot11MeshHWMPperrMinInterval,
		  u.mesh.mshcfg.dot11MeshHWMPperrMinInterval, DEC);
IEEE80211_IF_FILE(dot11MeshHWMPnetDiameterTraversalTime,
		  u.mesh.mshcfg.dot11MeshHWMPnetDiameterTraversalTime, DEC);
IEEE80211_IF_FILE(dot11MeshHWMPmaxPREQretries,
		  u.mesh.mshcfg.dot11MeshHWMPmaxPREQretries, DEC);
IEEE80211_IF_FILE(path_refresh_time,
		  u.mesh.mshcfg.path_refresh_time, DEC);
IEEE80211_IF_FILE(min_discovery_timeout,
		  u.mesh.mshcfg.min_discovery_timeout, DEC);
IEEE80211_IF_FILE(dot11MeshHWMPRootMode,
		  u.mesh.mshcfg.dot11MeshHWMPRootMode, DEC);
IEEE80211_IF_FILE(dot11MeshGateAnnouncementProtocol,
		  u.mesh.mshcfg.dot11MeshGateAnnouncementProtocol, DEC);
IEEE80211_IF_FILE(dot11MeshHWMPRannInterval,
		  u.mesh.mshcfg.dot11MeshHWMPRannInterval, DEC);
IEEE80211_IF_FILE(dot11MeshForwarding, u.mesh.mshcfg.dot11MeshForwarding, DEC);
IEEE80211_IF_FILE(rssi_threshold, u.mesh.mshcfg.rssi_threshold, DEC);
IEEE80211_IF_FILE(ht_opmode, u.mesh.mshcfg.ht_opmode, DEC);
IEEE80211_IF_FILE(dot11MeshHWMPactivePathToRootTimeout,
		  u.mesh.mshcfg.dot11MeshHWMPactivePathToRootTimeout, DEC);
IEEE80211_IF_FILE(dot11MeshHWMProotInterval,
		  u.mesh.mshcfg.dot11MeshHWMProotInterval, DEC);
IEEE80211_IF_FILE(dot11MeshHWMPconfirmationInterval,
		  u.mesh.mshcfg.dot11MeshHWMPconfirmationInterval, DEC);
IEEE80211_IF_FILE(power_mode, u.mesh.mshcfg.power_mode, DEC);
IEEE80211_IF_FILE(dot11MeshAwakeWindowDuration,
		  u.mesh.mshcfg.dot11MeshAwakeWindowDuration, DEC);
IEEE80211_IF_FILE(dot11MeshConnectedToMeshGate,
		  u.mesh.mshcfg.dot11MeshConnectedToMeshGate, DEC);
IEEE80211_IF_FILE(dot11MeshNolearn, u.mesh.mshcfg.dot11MeshNolearn, DEC);
IEEE80211_IF_FILE(dot11MeshConnectedToAuthServer,
		  u.mesh.mshcfg.dot11MeshConnectedToAuthServer, DEC);
#endif

#define DEBUGFS_ADD_MODE(name, mode) \
	debugfs_create_file(#name, mode, sdata->vif.debugfs_dir, \
			    sdata, &name##_ops)

#define DEBUGFS_ADD_X(_bits, _name, _mode) \
	debugfs_create_x##_bits(#_name, _mode, sdata->vif.debugfs_dir, \
				&sdata->vif._name)

#define DEBUGFS_ADD_X8(_name, _mode) \
	DEBUGFS_ADD_X(8, _name, _mode)

#define DEBUGFS_ADD_X16(_name, _mode) \
	DEBUGFS_ADD_X(16, _name, _mode)

#define DEBUGFS_ADD_X32(_name, _mode) \
	DEBUGFS_ADD_X(32, _name, _mode)

#define DEBUGFS_ADD(name) DEBUGFS_ADD_MODE(name, 0400)

static void add_common_files(struct ieee80211_sub_if_data *sdata)
{
	DEBUGFS_ADD(rc_rateidx_mask_2ghz);
	DEBUGFS_ADD(rc_rateidx_mask_5ghz);
	DEBUGFS_ADD(rc_rateidx_mcs_mask_2ghz);
	DEBUGFS_ADD(rc_rateidx_mcs_mask_5ghz);
	DEBUGFS_ADD(rc_rateidx_vht_mcs_mask_2ghz);
	DEBUGFS_ADD(rc_rateidx_vht_mcs_mask_5ghz);
	DEBUGFS_ADD(mac_tid_stats);
	DEBUGFS_ADD(reset_mac_tid_stats);
	DEBUGFS_ADD(enable_mac_tid_stats);
	DEBUGFS_ADD(hw_queues);

	if (sdata->vif.type != NL80211_IFTYPE_P2P_DEVICE &&
	    sdata->vif.type != NL80211_IFTYPE_NAN)
		DEBUGFS_ADD(aqm);
}

static void add_sta_files(struct ieee80211_sub_if_data *sdata)
{
	DEBUGFS_ADD(bssid);
	DEBUGFS_ADD(aid);
	DEBUGFS_ADD(beacon_timeout);
	DEBUGFS_ADD_MODE(tkip_mic_test, 0200);
	DEBUGFS_ADD_MODE(beacon_loss, 0200);
	DEBUGFS_ADD_MODE(uapsd_queues, 0600);
	DEBUGFS_ADD_MODE(uapsd_max_sp_len, 0600);
	DEBUGFS_ADD_MODE(tdls_wider_bw, 0600);
	DEBUGFS_ADD_MODE(valid_links, 0400);
	DEBUGFS_ADD_MODE(active_links, 0600);
	DEBUGFS_ADD_X16(dormant_links, 0400);
}

static void add_ap_files(struct ieee80211_sub_if_data *sdata)
{
	DEBUGFS_ADD(num_mcast_sta);
	DEBUGFS_ADD(num_sta_ps);
	DEBUGFS_ADD(dtim_count);
	DEBUGFS_ADD(num_buffered_multicast);
	DEBUGFS_ADD_MODE(tkip_mic_test, 0200);
	DEBUGFS_ADD_MODE(multicast_to_unicast, 0600);
	DEBUGFS_ADD_MODE(bmiss_threshold, 0600);
	DEBUGFS_ADD_MODE(noqueue_enable, 0600);
}

static void add_vlan_files(struct ieee80211_sub_if_data *sdata)
{
	/* add num_mcast_sta_vlan using name num_mcast_sta */
	debugfs_create_file("num_mcast_sta", 0400, sdata->vif.debugfs_dir,
			    sdata, &num_mcast_sta_vlan_ops);
}

static void add_ibss_files(struct ieee80211_sub_if_data *sdata)
{
	DEBUGFS_ADD_MODE(tsf, 0600);
}

#ifdef CPTCFG_MAC80211_MESH

static void add_mesh_files(struct ieee80211_sub_if_data *sdata)
{
	DEBUGFS_ADD_MODE(tsf, 0600);
	DEBUGFS_ADD_MODE(estab_plinks, 0400);
	DEBUGFS_ADD_MODE(bmiss_threshold, 0600);
	DEBUGFS_ADD_MODE(noqueue_enable, 0600);
}

static void add_mesh_stats(struct ieee80211_sub_if_data *sdata)
{
	struct dentry *dir = debugfs_create_dir("mesh_stats",
						sdata->vif.debugfs_dir);
#define MESHSTATS_ADD(name)\
	debugfs_create_file(#name, 0400, dir, sdata, &name##_ops)

	MESHSTATS_ADD(fwded_mcast);
	MESHSTATS_ADD(fwded_unicast);
	MESHSTATS_ADD(fwded_frames);
	MESHSTATS_ADD(dropped_frames_ttl);
	MESHSTATS_ADD(dropped_frames_no_route);
#undef MESHSTATS_ADD
}

static void add_mesh_config(struct ieee80211_sub_if_data *sdata)
{
	struct dentry *dir = debugfs_create_dir("mesh_config",
						sdata->vif.debugfs_dir);

#define MESHPARAMS_ADD(name) \
	debugfs_create_file(#name, 0600, dir, sdata, &name##_ops)

	MESHPARAMS_ADD(dot11MeshMaxRetries);
	MESHPARAMS_ADD(dot11MeshRetryTimeout);
	MESHPARAMS_ADD(dot11MeshConfirmTimeout);
	MESHPARAMS_ADD(dot11MeshHoldingTimeout);
	MESHPARAMS_ADD(dot11MeshTTL);
	MESHPARAMS_ADD(element_ttl);
	MESHPARAMS_ADD(auto_open_plinks);
	MESHPARAMS_ADD(dot11MeshMaxPeerLinks);
	MESHPARAMS_ADD(dot11MeshHWMPactivePathTimeout);
	MESHPARAMS_ADD(dot11MeshHWMPpreqMinInterval);
	MESHPARAMS_ADD(dot11MeshHWMPperrMinInterval);
	MESHPARAMS_ADD(dot11MeshHWMPnetDiameterTraversalTime);
	MESHPARAMS_ADD(dot11MeshHWMPmaxPREQretries);
	MESHPARAMS_ADD(path_refresh_time);
	MESHPARAMS_ADD(min_discovery_timeout);
	MESHPARAMS_ADD(dot11MeshHWMPRootMode);
	MESHPARAMS_ADD(dot11MeshHWMPRannInterval);
	MESHPARAMS_ADD(dot11MeshForwarding);
	MESHPARAMS_ADD(dot11MeshGateAnnouncementProtocol);
	MESHPARAMS_ADD(rssi_threshold);
	MESHPARAMS_ADD(ht_opmode);
	MESHPARAMS_ADD(dot11MeshHWMPactivePathToRootTimeout);
	MESHPARAMS_ADD(dot11MeshHWMProotInterval);
	MESHPARAMS_ADD(dot11MeshHWMPconfirmationInterval);
	MESHPARAMS_ADD(power_mode);
	MESHPARAMS_ADD(dot11MeshAwakeWindowDuration);
	MESHPARAMS_ADD(dot11MeshConnectedToMeshGate);
	MESHPARAMS_ADD(dot11MeshNolearn);
	MESHPARAMS_ADD(dot11MeshConnectedToAuthServer);
#undef MESHPARAMS_ADD
}
#endif

static void add_files(struct ieee80211_sub_if_data *sdata)
{
	if (!sdata->vif.debugfs_dir)
		return;

	DEBUGFS_ADD(flags);
	DEBUGFS_ADD(state);

	if (sdata->vif.type != NL80211_IFTYPE_MONITOR)
		add_common_files(sdata);

	switch (sdata->vif.type) {
	case NL80211_IFTYPE_MESH_POINT:
#ifdef CPTCFG_MAC80211_MESH
		add_mesh_files(sdata);
		add_mesh_stats(sdata);
		add_mesh_config(sdata);
#endif
		break;
	case NL80211_IFTYPE_STATION:
		add_sta_files(sdata);
		break;
	case NL80211_IFTYPE_ADHOC:
		add_ibss_files(sdata);
		break;
	case NL80211_IFTYPE_AP:
		add_ap_files(sdata);
		break;
	case NL80211_IFTYPE_AP_VLAN:
		add_vlan_files(sdata);
		break;
	default:
		break;
	}
}

#undef DEBUGFS_ADD_MODE
#undef DEBUGFS_ADD

#define DEBUGFS_ADD_MODE(dentry, name, mode) \
	debugfs_create_file(#name, mode, dentry, \
			    link, &link_##name##_ops)

#define DEBUGFS_ADD(dentry, name) DEBUGFS_ADD_MODE(dentry, name, 0400)

static void add_link_files(struct ieee80211_link_data *link,
			   struct dentry *dentry)
{
	DEBUGFS_ADD(dentry, txpower);
	DEBUGFS_ADD(dentry, user_power_level);
	DEBUGFS_ADD(dentry, ap_power_level);

	switch (link->sdata->vif.type) {
	case NL80211_IFTYPE_STATION:
		DEBUGFS_ADD_MODE(dentry, smps, 0600);
		break;
	default:
		break;
	}
}

void ieee80211_debugfs_add_link(struct ieee80211_sub_if_data *sdata,
				unsigned long add)
{
	char buf[IFNAMSIZ];
	u8 id;

	if (!sdata->vif.valid_links)
		return;

	for_each_set_bit(id, &add, IEEE80211_MLD_MAX_NUM_LINKS) {
		if (sdata->vif.link_debugfs[id])
			continue;

		snprintf(buf, IFNAMSIZ, "link%d", id);
		sdata->vif.link_debugfs[id] = debugfs_create_dir(buf,
								 sdata->vif.debugfs_dir);
	}
}

void ieee80211_debugfs_remove_link(struct ieee80211_sub_if_data *sdata, unsigned long rem)
{
	u8 link_id;

	if (!sdata->vif.valid_links)
		return;

	for_each_set_bit(link_id, &rem, IEEE80211_MLD_MAX_NUM_LINKS) {
		if (!sdata->vif.link_debugfs[link_id])
			continue;

		debugfs_remove_recursive(sdata->vif.link_debugfs[link_id]);
		sdata->vif.link_debugfs[link_id] = NULL;
	}
}

static void ieee80211_debugfs_add_netdev(struct ieee80211_sub_if_data *sdata,
					 bool mld_vif)
{
	char buf[10 + IFNAMSIZ];
	int i = 0;

	snprintf(buf, 10 + IFNAMSIZ, "netdev:%s", sdata->name);
	sdata->vif.debugfs_dir = debugfs_create_dir(buf,
		sdata->local->hw.wiphy->debugfsdir);
	/* deflink also has this */
	sdata->deflink.debugfs_dir = sdata->vif.debugfs_dir;
	sdata->debugfs.subdir_stations = debugfs_create_dir("stations",
							sdata->vif.debugfs_dir);
	for (i = 0; i < IEEE80211_MLD_MAX_NUM_LINKS; i++)
		sdata->vif.link_debugfs[i] = NULL;

	add_files(sdata);
	if (!mld_vif)
		add_link_files(&sdata->deflink, sdata->vif.debugfs_dir);

	/* create default link if it does not exist */
	if (sdata->vif.link_debugfs[0])
		return;

	/*TODO : Need to revamp the contents of link0 directory to link-0
	 * directory
	 */
	memset(buf, 0, 10 + IFNAMSIZ);
	snprintf(buf, 10 + IFNAMSIZ, "link0");
	sdata->vif.link_debugfs[0] = debugfs_create_dir(buf,
							sdata->vif.debugfs_dir);
}

void ieee80211_debugfs_remove_netdev(struct ieee80211_sub_if_data *sdata)
{
	int i = 0;
	if (!sdata->vif.debugfs_dir)
		return;

	if (!sdata->vif.valid_links &&
		sdata->vif.link_debugfs[0]) {
		debugfs_remove_recursive(sdata->vif.link_debugfs[0]);
		sdata->vif.link_debugfs[0] = NULL;
	}

	debugfs_remove_recursive(sdata->vif.debugfs_dir);
	sdata->vif.debugfs_dir = NULL;
	sdata->debugfs.subdir_stations = NULL;

	for (i = 0; i < IEEE80211_MLD_MAX_NUM_LINKS; i++)
		sdata->vif.link_debugfs[i] = NULL;
}

void ieee80211_debugfs_rename_netdev(struct ieee80211_sub_if_data *sdata)
{
#if LINUX_VERSION_IS_GEQ(6,14,0)
	debugfs_change_name(sdata->vif.debugfs_dir, "netdev:%s", sdata->name);
#else
	struct dentry *dir;
	char buf[10 + IFNAMSIZ];

	dir = sdata->vif.debugfs_dir;

	if (IS_ERR_OR_NULL(dir))
		return;

	snprintf(buf, sizeof(buf), "netdev:%s", sdata->name);
	debugfs_rename(dir->d_parent, dir, dir->d_parent, buf);
#endif
}

void ieee80211_debugfs_recreate_netdev(struct ieee80211_sub_if_data *sdata,
				       bool mld_vif)
{
	ieee80211_debugfs_remove_netdev(sdata);
	ieee80211_debugfs_add_netdev(sdata, mld_vif);

	if (sdata->flags & IEEE80211_SDATA_IN_DRIVER) {
		drv_vif_add_debugfs(sdata->local, sdata);
		if (!mld_vif)
			ieee80211_link_debugfs_drv_add(&sdata->deflink);
	}
}

#ifdef CPTCFG_MAC80211_DEBUGFS
void ieee80211_link_debugfs_add(struct ieee80211_link_data *link)
{
	char link_dir_name[10];

	if (WARN_ON(!link->sdata->vif.debugfs_dir || link->debugfs_dir))
		return;

	/* For now, this should not be called for non-MLO capable drivers */
	if (WARN_ON(!(link->sdata->local->hw.wiphy->flags & WIPHY_FLAG_SUPPORTS_MLO)))
		return;

	if (link->debugfs_dir)
		return;

	snprintf(link_dir_name, sizeof(link_dir_name),
		 "link-%d", link->link_id);

	link->debugfs_dir =
		debugfs_create_dir(link_dir_name,
				   link->sdata->vif.debugfs_dir);

	DEBUGFS_ADD(link->debugfs_dir, addr);
	add_link_files(link, link->debugfs_dir);
}

void ieee80211_link_debugfs_remove(struct ieee80211_link_data *link)
{
	if (!link->sdata->vif.debugfs_dir || !link->debugfs_dir) {
		link->debugfs_dir = NULL;
		return;
	}

	if (link->debugfs_dir == link->sdata->vif.debugfs_dir) {
		WARN_ON(link != &link->sdata->deflink);
		link->debugfs_dir = NULL;
		return;
	}

	debugfs_remove_recursive(link->debugfs_dir);
	link->debugfs_dir = NULL;
}

void ieee80211_link_debugfs_drv_add(struct ieee80211_link_data *link)
{
	if (link->sdata->vif.type == NL80211_IFTYPE_MONITOR ||
	    WARN_ON(!link->debugfs_dir))
		return;

	drv_link_add_debugfs(link->sdata->local, link->sdata,
			     link->conf, link->debugfs_dir);
}

void ieee80211_link_debugfs_drv_remove(struct ieee80211_link_data *link)
{
	if (!link || !link->debugfs_dir)
		return;

	if (WARN_ON(link->debugfs_dir == link->sdata->vif.debugfs_dir))
		return;

	/* Recreate the directory excluding the driver data */
	debugfs_remove_recursive(link->debugfs_dir);
	link->debugfs_dir = NULL;

	ieee80211_link_debugfs_add(link);
}
#endif
