// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/skbuff.h>
#include <linux/ctype.h>
#include <net/mac80211.h>
#include <net/cfg80211.h>
#include <linux/completion.h>
#include <linux/if_ether.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/uuid.h>
#include <linux/time.h>
#include <linux/of.h>
#include "core.h"
#include "debug.h"
#include "mac.h"
#include "hw.h"
#include "peer.h"
#include "testmode.h"
#include "p2p.h"

struct wmi_tlv_policy {
	size_t min_len;
};

struct wmi_tlv_svc_ready_parse {
	bool wmi_svc_bitmap_done;
};

struct wmi_tlv_dma_ring_caps_parse {
	struct wmi_dma_ring_capabilities *dma_ring_caps;
	u32 n_dma_ring_caps;
};

struct wmi_tlv_svc_rdy_ext_parse {
	struct ath11k_service_ext_param param;
	struct wmi_soc_mac_phy_hw_mode_caps *hw_caps;
	struct wmi_hw_mode_capabilities *hw_mode_caps;
	u32 n_hw_mode_caps;
	u32 tot_phy_id;
	struct wmi_hw_mode_capabilities pref_hw_mode_caps;
	struct wmi_mac_phy_capabilities *mac_phy_caps;
	u32 n_mac_phy_caps;
	struct wmi_soc_hal_reg_capabilities *soc_hal_reg_caps;
	struct wmi_hal_reg_capabilities_ext *ext_hal_reg_caps;
	u32 n_ext_hal_reg_caps;
	struct wmi_tlv_dma_ring_caps_parse dma_caps_parse;
	bool hw_mode_done;
	bool mac_phy_done;
	bool ext_hal_reg_done;
	u32 n_mac_phy_chainmask_combo;
	bool mac_phy_chainmask_combo_done;
	bool mac_phy_chainmask_cap_done;
	bool oem_dma_ring_cap_done;
	bool dma_ring_cap_done;
};

struct wmi_tlv_svc_rdy_ext2_parse {
	struct wmi_tlv_dma_ring_caps_parse dma_caps_parse;
	bool dma_ring_cap_done;
};

struct wmi_tlv_rdy_parse {
	u32 num_extra_mac_addr;
};

struct wmi_tlv_dma_buf_release_parse {
	struct ath11k_wmi_dma_buf_release_fixed_param fixed;
	struct wmi_dma_buf_release_entry *buf_entry;
	struct wmi_dma_buf_release_meta_data *meta_data;
	u32 num_buf_entry;
	u32 num_meta;
	bool buf_entry_done;
	bool meta_data_done;
};

struct wmi_tlv_fw_stats_parse {
	const struct wmi_stats_event *ev;
	const struct wmi_per_chain_rssi_stats *rssi;
	struct ath11k_fw_stats *stats;
	int rssi_num;
	bool chain_rssi_done;
};

struct wmi_tlv_mgmt_rx_parse {
	const struct wmi_mgmt_rx_hdr *fixed;
	const u8 *frame_buf;
	bool frame_buf_done;
};

static const struct wmi_tlv_policy wmi_tlv_policies[] = {
	[WMI_TAG_ARRAY_BYTE]
		= { .min_len = 0 },
	[WMI_TAG_ARRAY_UINT32]
		= { .min_len = 0 },
	[WMI_TAG_ARRAY_STRUCT]
		= { .min_len = 0 },
	[WMI_TAG_ARRAY_INT16]
		= { .min_len = 0 },
	[WMI_TAG_SERVICE_READY_EVENT]
		= { .min_len = sizeof(struct wmi_service_ready_event) },
	[WMI_TAG_SERVICE_READY_EXT_EVENT]
		= { .min_len =  sizeof(struct wmi_service_ready_ext_event) },
	[WMI_TAG_SOC_MAC_PHY_HW_MODE_CAPS]
		= { .min_len = sizeof(struct wmi_soc_mac_phy_hw_mode_caps) },
	[WMI_TAG_SOC_HAL_REG_CAPABILITIES]
		= { .min_len = sizeof(struct wmi_soc_hal_reg_capabilities) },
	[WMI_TAG_VDEV_START_RESPONSE_EVENT]
		= { .min_len = sizeof(struct wmi_vdev_start_resp_event) },
	[WMI_TAG_PEER_DELETE_RESP_EVENT]
		= { .min_len = sizeof(struct wmi_peer_delete_resp_event) },
	[WMI_TAG_OFFLOAD_BCN_TX_STATUS_EVENT]
		= { .min_len = sizeof(struct wmi_bcn_tx_status_event) },
	[WMI_TAG_VDEV_STOPPED_EVENT]
		= { .min_len = sizeof(struct wmi_vdev_stopped_event) },
	[WMI_TAG_REG_CHAN_LIST_CC_EVENT]
		= { .min_len = sizeof(struct wmi_reg_chan_list_cc_event) },
	[WMI_TAG_REG_CHAN_LIST_CC_EXT_EVENT]
		= { .min_len = sizeof(struct wmi_reg_chan_list_cc_ext_event) },
	[WMI_TAG_MGMT_RX_HDR]
		= { .min_len = sizeof(struct wmi_mgmt_rx_hdr) },
	[WMI_TAG_MGMT_TX_COMPL_EVENT]
		= { .min_len = sizeof(struct wmi_tx_compl_event) },
	[WMI_TAG_SCAN_EVENT]
		= { .min_len = sizeof(struct wmi_scan_event) },
	[WMI_TAG_PEER_STA_KICKOUT_EVENT]
		= { .min_len = sizeof(struct wmi_peer_sta_kickout_event) },
	[WMI_TAG_ROAM_EVENT]
		= { .min_len = sizeof(struct wmi_roam_event) },
	[WMI_TAG_CHAN_INFO_EVENT]
		= { .min_len = sizeof(struct wmi_chan_info_event) },
	[WMI_TAG_PDEV_BSS_CHAN_INFO_EVENT]
		= { .min_len = sizeof(struct wmi_pdev_bss_chan_info_event) },
	[WMI_TAG_VDEV_INSTALL_KEY_COMPLETE_EVENT]
		= { .min_len = sizeof(struct wmi_vdev_install_key_compl_event) },
	[WMI_TAG_READY_EVENT] = {
		.min_len = sizeof(struct wmi_ready_event_min) },
	[WMI_TAG_SERVICE_AVAILABLE_EVENT]
		= {.min_len = sizeof(struct wmi_service_available_event) },
	[WMI_TAG_PEER_ASSOC_CONF_EVENT]
		= { .min_len = sizeof(struct wmi_peer_assoc_conf_event) },
	[WMI_TAG_STATS_EVENT]
		= { .min_len = sizeof(struct wmi_stats_event) },
	[WMI_TAG_PDEV_CTL_FAILSAFE_CHECK_EVENT]
		= { .min_len = sizeof(struct wmi_pdev_ctl_failsafe_chk_event) },
	[WMI_TAG_HOST_SWFDA_EVENT] = {
		.min_len = sizeof(struct wmi_fils_discovery_event) },
	[WMI_TAG_OFFLOAD_PRB_RSP_TX_STATUS_EVENT] = {
		.min_len = sizeof(struct wmi_probe_resp_tx_status_event) },
	[WMI_TAG_VDEV_DELETE_RESP_EVENT] = {
		.min_len = sizeof(struct wmi_vdev_delete_resp_event) },
	[WMI_TAG_OBSS_COLOR_COLLISION_EVT] = {
		.min_len = sizeof(struct wmi_obss_color_collision_event) },
	[WMI_TAG_11D_NEW_COUNTRY_EVENT] = {
		.min_len = sizeof(struct wmi_11d_new_cc_ev) },
	[WMI_TAG_PER_CHAIN_RSSI_STATS] = {
		.min_len = sizeof(struct wmi_per_chain_rssi_stats) },
	[WMI_TAG_TWT_ADD_DIALOG_COMPLETE_EVENT] = {
		.min_len = sizeof(struct wmi_twt_add_dialog_event) },
	[WMI_TAG_TPC_STATS_EVENT_FIXED_PARAM]
		= { .min_len = sizeof(struct wmi_tpc_stats_event_fixed_param) },
	[WMI_TAG_P2P_NOA_INFO] = {
		.min_len = sizeof(struct ath11k_wmi_p2p_noa_info) },
	[WMI_TAG_P2P_NOA_EVENT] = {
		.min_len = sizeof(struct wmi_p2p_noa_event) },
	[WMI_TAG_WDS_ADDR_EVENT]
		= { .min_len = sizeof(struct wmi_wds_addr_event) },
	[WMI_TAG_PEER_RATECODE_LIST_EVENT]
		= { .min_len = sizeof(struct wmi_peer_ratecode_list_fixed_param) },
};

#define PRIMAP(_hw_mode_) \
	[_hw_mode_] = _hw_mode_##_PRI

static const int ath11k_hw_mode_pri_map[] = {
	PRIMAP(WMI_HOST_HW_MODE_SINGLE),
	PRIMAP(WMI_HOST_HW_MODE_DBS),
	PRIMAP(WMI_HOST_HW_MODE_SBS_PASSIVE),
	PRIMAP(WMI_HOST_HW_MODE_SBS),
	PRIMAP(WMI_HOST_HW_MODE_DBS_SBS),
	PRIMAP(WMI_HOST_HW_MODE_DBS_OR_SBS),
	/* keep last */
	PRIMAP(WMI_HOST_HW_MODE_MAX),
};

enum wmi_host_channel_width
ath11k_wmi_get_host_chan_width(u32 width)
{
	enum wmi_host_channel_width host_width;

	switch (width) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
		host_width = WMI_HOST_CHAN_WIDTH_20;
		break;
	case NL80211_CHAN_WIDTH_40:
		host_width = WMI_HOST_CHAN_WIDTH_40;
		break;
	case NL80211_CHAN_WIDTH_80:
		host_width = WMI_HOST_CHAN_WIDTH_80;
		break;
	case NL80211_CHAN_WIDTH_160:
		host_width = WMI_HOST_CHAN_WIDTH_160;
		break;
	case NL80211_CHAN_WIDTH_80P80:
		host_width = WMI_HOST_CHAN_WIDTH_80P80;
		break;
	default:
		host_width = WMI_HOST_CHAN_WIDTH_MAX;
		break;
	}

	return host_width;
}

static int
ath11k_wmi_tlv_iter(struct ath11k_base *ab, const void *ptr, size_t len,
		    int (*iter)(struct ath11k_base *ab, u16 tag, u16 len,
				const void *ptr, void *data),
		    void *data)
{
	const void *begin = ptr;
	const struct wmi_tlv *tlv;
	u16 tlv_tag, tlv_len;
	int ret;

	while (len > 0) {
		if (len < sizeof(*tlv)) {
			ath11k_err(ab, "wmi tlv parse failure at byte %zd (%zu bytes left, %zu expected)\n",
				   ptr - begin, len, sizeof(*tlv));
			return -EINVAL;
		}

		tlv = ptr;
		tlv_tag = FIELD_GET(WMI_TLV_TAG, tlv->header);
		tlv_len = FIELD_GET(WMI_TLV_LEN, tlv->header);
		ptr += sizeof(*tlv);
		len -= sizeof(*tlv);

		if (tlv_len > len) {
			ath11k_err(ab, "wmi tlv parse failure of tag %u at byte %zd (%zu bytes left, %u expected)\n",
				   tlv_tag, ptr - begin, len, tlv_len);
			return -EINVAL;
		}

		if (tlv_tag < ARRAY_SIZE(wmi_tlv_policies) &&
		    wmi_tlv_policies[tlv_tag].min_len &&
		    wmi_tlv_policies[tlv_tag].min_len > tlv_len) {
			ath11k_err(ab, "wmi tlv parse failure of tag %u at byte %zd (%u bytes is less than min length %zu)\n",
				   tlv_tag, ptr - begin, tlv_len,
				   wmi_tlv_policies[tlv_tag].min_len);
			return -EINVAL;
		}

		ret = iter(ab, tlv_tag, tlv_len, ptr, data);
		if (ret)
			return ret;

		ptr += tlv_len;
		len -= tlv_len;
	}

	return 0;
}

static int ath11k_wmi_tlv_iter_parse(struct ath11k_base *ab, u16 tag, u16 len,
				     const void *ptr, void *data)
{
	const void **tb = data;

	if (tag < WMI_TAG_MAX)
		tb[tag] = ptr;

	return 0;
}

static int ath11k_wmi_tlv_parse(struct ath11k_base *ar, const void **tb,
				const void *ptr, size_t len)
{
	return ath11k_wmi_tlv_iter(ar, ptr, len, ath11k_wmi_tlv_iter_parse,
				   (void *)tb);
}

const void **ath11k_wmi_tlv_parse_alloc(struct ath11k_base *ab, const void *ptr,
                                        size_t len, gfp_t gfp)
{
        const void **tb;
        int ret;

        tb = kcalloc(WMI_TAG_MAX, sizeof(*tb), gfp);
        if (!tb)
                return ERR_PTR(-ENOMEM);

        ret = ath11k_wmi_tlv_parse(ab, tb, ptr, len);
        if (ret) {
                kfree(tb);
                return ERR_PTR(ret);
        }

        return tb;
}

static int ath11k_wmi_cmd_send_nowait(struct ath11k_pdev_wmi *wmi, struct sk_buff *skb,
				      u32 cmd_id)
{
	struct ath11k_skb_cb *skb_cb = ATH11K_SKB_CB(skb);
	struct ath11k_base *ab = wmi->wmi_ab->ab;
	struct wmi_cmd_hdr *cmd_hdr;
	int ret;
	u32 cmd = 0;

	if (skb_push(skb, sizeof(struct wmi_cmd_hdr)) == NULL)
		return -ENOMEM;

	cmd |= FIELD_PREP(WMI_CMD_HDR_CMD_ID, cmd_id);

	cmd_hdr = (struct wmi_cmd_hdr *)skb->data;
	cmd_hdr->cmd_id = cmd;

	trace_ath11k_wmi_cmd(ab, cmd_id, skb->data, skb->len);

	memset(skb_cb, 0, sizeof(*skb_cb));
	ret = ath11k_htc_send(&ab->htc, wmi->eid, skb);

	if (ret)
		goto err_pull;

	return 0;

err_pull:
	skb_pull(skb, sizeof(struct wmi_cmd_hdr));
	return ret;
}

int ath11k_wmi_cmd_send(struct ath11k_pdev_wmi *wmi, struct sk_buff *skb,
			u32 cmd_id)
{
	struct ath11k_wmi_base *wmi_ab = wmi->wmi_ab;
	int ret = -EOPNOTSUPP;
	struct ath11k_base *ab = wmi_ab->ab;

	might_sleep();

	if (ab->hw_params.credit_flow) {
		wait_event_timeout(wmi_ab->tx_credits_wq, ({
			ret = ath11k_wmi_cmd_send_nowait(wmi, skb, cmd_id);

			if (ret && test_bit(ATH11K_FLAG_CRASH_FLUSH,
					    &wmi_ab->ab->dev_flags))
				ret = -ESHUTDOWN;

			(ret != -EAGAIN);
			}), WMI_SEND_TIMEOUT_HZ);
	} else {
		wait_event_timeout(wmi->tx_ce_desc_wq, ({
			ret = ath11k_wmi_cmd_send_nowait(wmi, skb, cmd_id);

			if (ret && test_bit(ATH11K_FLAG_CRASH_FLUSH,
					    &wmi_ab->ab->dev_flags))
				ret = -ESHUTDOWN;

			(ret != -ENOBUFS);
			}), WMI_SEND_TIMEOUT_HZ);
	}

	if (ret == -EAGAIN)
		ath11k_warn(wmi_ab->ab, "wmi command %d timeout\n", cmd_id);

	if (ret == -ENOBUFS)
		ath11k_warn(wmi_ab->ab, "ce desc not available for wmi command %d\n",
			    cmd_id);

	return ret;
}

static int ath11k_pull_svc_ready_ext(struct ath11k_pdev_wmi *wmi_handle,
				     const void *ptr,
				     struct ath11k_service_ext_param *param)
{
	const struct wmi_service_ready_ext_event *ev = ptr;

	if (!ev)
		return -EINVAL;

	/* Move this to host based bitmap */
	param->default_conc_scan_config_bits = ev->default_conc_scan_config_bits;
	param->default_fw_config_bits =	ev->default_fw_config_bits;
	param->he_cap_info = ev->he_cap_info;
	param->mpdu_density = ev->mpdu_density;
	param->max_bssid_rx_filters = ev->max_bssid_rx_filters;
	memcpy(&param->ppet, &ev->ppet, sizeof(param->ppet));

	return 0;
}

static int
ath11k_pull_mac_phy_cap_svc_ready_ext(struct ath11k_pdev_wmi *wmi_handle,
				      struct wmi_soc_mac_phy_hw_mode_caps *hw_caps,
				      struct wmi_hw_mode_capabilities *wmi_hw_mode_caps,
				      struct wmi_soc_hal_reg_capabilities *hal_reg_caps,
				      struct wmi_mac_phy_capabilities *wmi_mac_phy_caps,
				      u8 hw_mode_id, u8 phy_id,
				      struct ath11k_pdev *pdev)
{
	struct wmi_mac_phy_capabilities *mac_phy_caps;
	struct ath11k_base *ab = wmi_handle->wmi_ab->ab;
	struct ath11k_band_cap *cap_band;
	struct ath11k_pdev_cap *pdev_cap = &pdev->cap;
	u32 phy_map;
	u32 hw_idx, phy_idx = 0;

	if (!hw_caps || !wmi_hw_mode_caps || !hal_reg_caps)
		return -EINVAL;

	for (hw_idx = 0; hw_idx < hw_caps->num_hw_modes; hw_idx++) {
		if (hw_mode_id == wmi_hw_mode_caps[hw_idx].hw_mode_id)
			break;

		phy_map = wmi_hw_mode_caps[hw_idx].phy_id_map;
		while (phy_map) {
			phy_map >>= 1;
			phy_idx++;
		}
	}

	if (hw_idx == hw_caps->num_hw_modes)
		return -EINVAL;

	phy_idx += phy_id;
	if (phy_id >= hal_reg_caps->num_phy)
		return -EINVAL;

	mac_phy_caps = wmi_mac_phy_caps + phy_idx;

	pdev->pdev_id = mac_phy_caps->pdev_id;
	pdev_cap->supported_bands |= mac_phy_caps->supported_bands;
	pdev_cap->ampdu_density = mac_phy_caps->ampdu_density;
	ab->target_pdev_ids[ab->target_pdev_count].supported_bands =
		mac_phy_caps->supported_bands;
	ab->target_pdev_ids[ab->target_pdev_count].pdev_id = mac_phy_caps->pdev_id;
	ab->target_pdev_count++;

	if (!(mac_phy_caps->supported_bands & WMI_HOST_WLAN_2G_CAP) &&
	    !(mac_phy_caps->supported_bands & WMI_HOST_WLAN_5G_CAP))
		return -EINVAL;

	/* Take non-zero tx/rx chainmask. If tx/rx chainmask differs from
	 * band to band for a single radio, need to see how this should be
	 * handled.
	 */
	if (mac_phy_caps->supported_bands & WMI_HOST_WLAN_2G_CAP) {
		pdev_cap->vht_cap = mac_phy_caps->vht_cap_info_2g;
		pdev_cap->vht_mcs = mac_phy_caps->vht_supp_mcs_2g;
		pdev_cap->tx_chain_mask = mac_phy_caps->tx_chain_mask_2g;
		pdev_cap->rx_chain_mask = mac_phy_caps->rx_chain_mask_2g;
	}

	if (mac_phy_caps->supported_bands & WMI_HOST_WLAN_5G_CAP) {
		pdev_cap->vht_cap = mac_phy_caps->vht_cap_info_5g;
		pdev_cap->vht_mcs = mac_phy_caps->vht_supp_mcs_5g;
		pdev_cap->he_mcs = mac_phy_caps->he_supp_mcs_5g;
		pdev_cap->tx_chain_mask = mac_phy_caps->tx_chain_mask_5g;
		pdev_cap->rx_chain_mask = mac_phy_caps->rx_chain_mask_5g;
		pdev_cap->nss_ratio_enabled =
			WMI_NSS_RATIO_ENABLE_DISABLE_GET(mac_phy_caps->nss_ratio);
		pdev_cap->nss_ratio_info =
			WMI_NSS_RATIO_INFO_GET(mac_phy_caps->nss_ratio);
	}

	/* tx/rx chainmask reported from fw depends on the actual hw chains used,
	 * For example, for 4x4 capable macphys, first 4 chains can be used for first
	 * mac and the remaining 4 chains can be used for the second mac or vice-versa.
	 * In this case, tx/rx chainmask 0xf will be advertised for first mac and 0xf0
	 * will be advertised for second mac or vice-versa. Compute the shift value
	 * for tx/rx chainmask which will be used to advertise supported ht/vht rates to
	 * mac80211.
	 */
	pdev_cap->tx_chain_mask_shift =
			find_first_bit((unsigned long *)&pdev_cap->tx_chain_mask, 32);
	pdev_cap->rx_chain_mask_shift =
			find_first_bit((unsigned long *)&pdev_cap->rx_chain_mask, 32);

	if (mac_phy_caps->supported_bands & WMI_HOST_WLAN_2G_CAP) {
		cap_band = &pdev_cap->band[NL80211_BAND_2GHZ];
		cap_band->phy_id = mac_phy_caps->phy_id;
		cap_band->max_bw_supported = mac_phy_caps->max_bw_supported_2g;
		cap_band->ht_cap_info = mac_phy_caps->ht_cap_info_2g;
		cap_band->he_cap_info[0] = mac_phy_caps->he_cap_info_2g;
		cap_band->he_cap_info[1] = mac_phy_caps->he_cap_info_2g_ext;
		cap_band->he_mcs = mac_phy_caps->he_supp_mcs_2g;
		memcpy(cap_band->he_cap_phy_info, &mac_phy_caps->he_cap_phy_info_2g,
		       sizeof(u32) * PSOC_HOST_MAX_PHY_SIZE);
		memcpy(&cap_band->he_ppet, &mac_phy_caps->he_ppet2g,
		       sizeof(struct ath11k_ppe_threshold));
	}

	if (mac_phy_caps->supported_bands & WMI_HOST_WLAN_5G_CAP) {
		cap_band = &pdev_cap->band[NL80211_BAND_5GHZ];
		cap_band->phy_id = mac_phy_caps->phy_id;
		cap_band->max_bw_supported = mac_phy_caps->max_bw_supported_5g;
		cap_band->ht_cap_info = mac_phy_caps->ht_cap_info_5g;
		cap_band->he_cap_info[0] = mac_phy_caps->he_cap_info_5g;
		cap_band->he_cap_info[1] = mac_phy_caps->he_cap_info_5g_ext;
		cap_band->he_mcs = mac_phy_caps->he_supp_mcs_5g;
		memcpy(cap_band->he_cap_phy_info, &mac_phy_caps->he_cap_phy_info_5g,
		       sizeof(u32) * PSOC_HOST_MAX_PHY_SIZE);
		memcpy(&cap_band->he_ppet, &mac_phy_caps->he_ppet5g,
		       sizeof(struct ath11k_ppe_threshold));

		cap_band = &pdev_cap->band[NL80211_BAND_6GHZ];
		cap_band->max_bw_supported = mac_phy_caps->max_bw_supported_5g;
		cap_band->ht_cap_info = mac_phy_caps->ht_cap_info_5g;
		cap_band->he_cap_info[0] = mac_phy_caps->he_cap_info_5g;
		cap_band->he_cap_info[1] = mac_phy_caps->he_cap_info_5g_ext;
		cap_band->he_mcs = mac_phy_caps->he_supp_mcs_5g;
		memcpy(cap_band->he_cap_phy_info, &mac_phy_caps->he_cap_phy_info_5g,
		       sizeof(u32) * PSOC_HOST_MAX_PHY_SIZE);
		memcpy(&cap_band->he_ppet, &mac_phy_caps->he_ppet5g,
		       sizeof(struct ath11k_ppe_threshold));
	}

	return 0;
}

static int
ath11k_pull_reg_cap_svc_rdy_ext(struct ath11k_pdev_wmi *wmi_handle,
				struct wmi_soc_hal_reg_capabilities *reg_caps,
				struct wmi_hal_reg_capabilities_ext *wmi_ext_reg_cap,
				u8 phy_idx,
				struct ath11k_hal_reg_capabilities_ext *param)
{
	struct wmi_hal_reg_capabilities_ext *ext_reg_cap;

	if (!reg_caps || !wmi_ext_reg_cap)
		return -EINVAL;

	if (phy_idx >= reg_caps->num_phy)
		return -EINVAL;

	ext_reg_cap = &wmi_ext_reg_cap[phy_idx];

	param->phy_id = ext_reg_cap->phy_id;
	param->eeprom_reg_domain = ext_reg_cap->eeprom_reg_domain;
	param->eeprom_reg_domain_ext =
			      ext_reg_cap->eeprom_reg_domain_ext;
	param->regcap1 = ext_reg_cap->regcap1;
	param->regcap2 = ext_reg_cap->regcap2;
	/* check if param->wireless_mode is needed */
	param->low_2ghz_chan = ext_reg_cap->low_2ghz_chan;
	param->high_2ghz_chan = ext_reg_cap->high_2ghz_chan;
	param->low_5ghz_chan = ext_reg_cap->low_5ghz_chan;
	param->high_5ghz_chan = ext_reg_cap->high_5ghz_chan;

	return 0;
}

static int ath11k_pull_service_ready_tlv(struct ath11k_base *ab,
					 const void *evt_buf,
					 struct ath11k_targ_cap *cap)
{
	const struct wmi_service_ready_event *ev = evt_buf;

	if (!ev) {
		ath11k_err(ab, "%s: failed by NULL param\n",
			   __func__);
		return -EINVAL;
	}

	cap->phy_capability = ev->phy_capability;
	cap->max_frag_entry = ev->max_frag_entry;
	cap->num_rf_chains = ev->num_rf_chains;
	cap->ht_cap_info = ev->ht_cap_info;
	cap->vht_cap_info = ev->vht_cap_info;
	cap->vht_supp_mcs = ev->vht_supp_mcs;
	cap->hw_min_tx_power = ev->hw_min_tx_power;
	cap->hw_max_tx_power = ev->hw_max_tx_power;
	cap->sys_cap_info = ev->sys_cap_info;
	cap->min_pkt_size_enable = ev->min_pkt_size_enable;
	cap->max_bcn_ie_size = ev->max_bcn_ie_size;
	cap->max_num_scan_channels = ev->max_num_scan_channels;
	cap->max_supported_macs = ev->max_supported_macs;
	cap->wmi_fw_sub_feat_caps = ev->wmi_fw_sub_feat_caps;
	cap->txrx_chainmask = ev->txrx_chainmask;
	cap->default_dbs_hw_mode_index = ev->default_dbs_hw_mode_index;
	cap->num_msdu_desc = ev->num_msdu_desc;

	return 0;
}

/* Save the wmi_service_bitmap into a linear bitmap. The wmi_services in
 * wmi_service ready event are advertised in b0-b3 (LSB 4-bits) of each
 * 4-byte word.
 */
static void ath11k_wmi_service_bitmap_copy(struct ath11k_pdev_wmi *wmi,
					   const u32 *wmi_svc_bm)
{
	int i, j;

	for (i = 0, j = 0; i < WMI_SERVICE_BM_SIZE && j < WMI_MAX_SERVICE; i++) {
		do {
			if (wmi_svc_bm[i] & BIT(j % WMI_SERVICE_BITS_IN_SIZE32))
				set_bit(j, wmi->wmi_ab->svc_map);
		} while (++j % WMI_SERVICE_BITS_IN_SIZE32);
	}
}

static int ath11k_wmi_tlv_svc_rdy_parse(struct ath11k_base *ab, u16 tag, u16 len,
					const void *ptr, void *data)
{
	struct wmi_tlv_svc_ready_parse *svc_ready = data;
	struct ath11k_pdev_wmi *wmi_handle = &ab->wmi_ab.wmi[0];
	u16 expect_len;

	switch (tag) {
	case WMI_TAG_SERVICE_READY_EVENT:
		if (ath11k_pull_service_ready_tlv(ab, ptr, &ab->target_caps))
			return -EINVAL;
		break;

	case WMI_TAG_ARRAY_UINT32:
		if (!svc_ready->wmi_svc_bitmap_done) {
			expect_len = WMI_SERVICE_BM_SIZE * sizeof(u32);
			if (len < expect_len) {
				ath11k_warn(ab, "invalid len %d for the tag 0x%x\n",
					    len, tag);
				return -EINVAL;
			}

			ath11k_wmi_service_bitmap_copy(wmi_handle, ptr);

			svc_ready->wmi_svc_bitmap_done = true;
		}
		break;
	default:
		break;
	}

	return 0;
}

static int ath11k_service_ready_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct wmi_tlv_svc_ready_parse svc_ready = { };
	int ret;

	ret = ath11k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath11k_wmi_tlv_svc_rdy_parse,
				  &svc_ready);
	if (ret) {
		ath11k_warn(ab, "failed to parse tlv %d\n", ret);
		return ret;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event service ready");

	return 0;
}

struct sk_buff *ath11k_wmi_alloc_skb(struct ath11k_wmi_base *wmi_ab, u32 len)
{
	struct sk_buff *skb;
	struct ath11k_base *ab = wmi_ab->ab;
	u32 round_len = roundup(len, 4);

	skb = ath11k_htc_alloc_skb(ab, WMI_SKB_HEADROOM + round_len);
	if (!skb)
		return NULL;

	ATH11K_MEMORY_STATS_INC(ab, wmi_tx_skb_alloc, skb->truesize);

	skb_reserve(skb, WMI_SKB_HEADROOM);
	if (!IS_ALIGNED((unsigned long)skb->data, 4))
		ath11k_warn(ab, "unaligned WMI skb data\n");

	skb_put(skb, round_len);
	memset(skb->data, 0, round_len);

	return skb;
}

static u32 ath11k_wmi_mgmt_get_freq(struct ath11k *ar,
				    struct ieee80211_tx_info *info)
{
	struct ath11k_base *ab = ar->ab;
	u32 freq = 0;

	if (ab->hw_params.support_off_channel_tx &&
	    ar->scan.is_roc &&
	    (info->flags & IEEE80211_TX_CTL_TX_OFFCHAN))
		freq = ar->scan.roc_freq;

	return freq;
}

int ath11k_wmi_mgmt_send(struct ath11k *ar, u32 vdev_id, u32 buf_id,
			 struct sk_buff *frame, bool tx_params_valid)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(frame);
	struct wmi_mgmt_send_cmd *cmd;
	struct wmi_tx_send_params *params;
	struct wmi_tlv *frame_tlv;
	struct sk_buff *skb;
	u32 buf_len;
	int ret, len;

	buf_len = frame->len < WMI_MGMT_SEND_DOWNLD_LEN ?
		  frame->len : WMI_MGMT_SEND_DOWNLD_LEN;

	len = sizeof(*cmd) + sizeof(*frame_tlv) + roundup(buf_len, 4);

	if (tx_params_valid)
		len += sizeof(*params);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_mgmt_send_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_MGMT_TX_SEND_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	cmd->desc_id = buf_id;
	cmd->chanfreq = ath11k_wmi_mgmt_get_freq(ar, info);
	cmd->paddr_lo = lower_32_bits(ATH11K_SKB_CB(frame)->paddr);
	cmd->paddr_hi = upper_32_bits(ATH11K_SKB_CB(frame)->paddr);
	cmd->frame_len = frame->len;
	cmd->buf_len = buf_len;
	cmd->tx_params_valid = tx_params_valid;

	frame_tlv = (struct wmi_tlv *)(skb->data + sizeof(*cmd));
	frame_tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_BYTE) |
			    FIELD_PREP(WMI_TLV_LEN, buf_len);

	memcpy(frame_tlv->value, frame->data, buf_len);

	ath11k_ce_byte_swap(frame_tlv->value, buf_len);

	if (tx_params_valid) {
		params = (struct wmi_tx_send_params *)(skb->data + (len - sizeof(*params)));
		params->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_TX_SEND_PARAMS) |
				     FIELD_PREP(WMI_TLV_LEN, sizeof(*params) - TLV_HDR_SIZE);
		params->tx_params_dword1 |= WMI_TX_PARAMS_DWORD1_CFR_CAPTURE;
	}

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_MGMT_TX_SEND_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to submit WMI_MGMT_TX_SEND_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "cmd mgmt tx send");

	return ret;
}

int ath11k_wmi_qos_null_send(struct ath11k *ar, u32 vdev_id, u32 buf_id,
			     struct sk_buff *frame)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_qos_null_tx_cmd *cmd;
	struct wmi_tlv *frame_tlv;
	struct sk_buff *skb;
	u32 buf_len;
	int len, ret = 0;

	buf_len = frame->len < WMI_QOS_NULL_SEND_BUF_LEN ?
		  frame->len : WMI_QOS_NULL_SEND_BUF_LEN;

	len = sizeof(*cmd) + sizeof(*frame_tlv) + roundup(buf_len, 4);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_qos_null_tx_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_QOS_NULL_FRAME_TX_SEND) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	cmd->desc_id = buf_id;
	cmd->paddr_lo = lower_32_bits(ATH11K_SKB_CB(frame)->paddr);
	cmd->paddr_hi = upper_32_bits(ATH11K_SKB_CB(frame)->paddr);
	cmd->frame_len = frame->len;
	cmd->buf_len = buf_len;

	frame_tlv = (struct wmi_tlv *)(skb->data + sizeof(*cmd));
	frame_tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_BYTE) |
			    FIELD_PREP(WMI_TLV_LEN, buf_len);

	memcpy(frame_tlv->value, frame->data, buf_len);

	ath11k_ce_byte_swap(frame_tlv->value, buf_len);

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_QOS_NULL_FRAME_TX_SEND_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to submit WMI_QOS_NULL_FRAME_TX_SEND_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "wmi QOS null tx send cmd sent successfully\n");
	return ret;
}

int ath11k_wmi_set_per_peer_per_tid_cfg(struct ath11k *ar,
					const struct wmi_per_peer_per_tid_cfg_arg *arg)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_peer_per_tid_cfg_cmd *cmd;
	struct sk_buff *skb = NULL;
	int ret = 0;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	memset(skb->data, 0, sizeof(*cmd));

	cmd = (struct wmi_peer_per_tid_cfg_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PEER_TID_CONFIGURATIONS_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = arg->vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, arg->peer_macaddr.addr);
	cmd->tid = arg->tid;
	cmd->ack_policy = arg->ack_policy;
	cmd->aggr_control = arg->aggr_control;
	cmd->rate_control = arg->rate_ctrl;
	cmd->retry_count = arg->retry_count;
	cmd->rcode_flags = arg->rcode_flags;
	cmd->ext_tid_cfg_bitmap = arg->ext_tid_cfg_bitmap;
	cmd->rtscts_ctrl = arg->rtscts_ctrl;
	cmd->max_num_mpdu_in_ppdu = arg->max_num_mpdu_in_ppdu;
	cmd->max_num_msdu_in_mpdu = arg->max_num_msdu_in_mpdu;

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "wmi noack tid %d vdev id %d ack_policy %d aggr %u rate_ctrl %u rcflag 0x%x retry_count %d\n rtscts %d ext_tid_cfg_bitmap %d mac_addr %pM ampdu_count %d amsdu count %d\n",
		   arg->tid, arg->vdev_id, arg->ack_policy, arg->aggr_control,
		   arg->rate_ctrl, arg->rcode_flags, arg->retry_count, arg->rtscts_ctrl,
		   arg->ext_tid_cfg_bitmap, arg->peer_macaddr.addr,
		   arg->max_num_mpdu_in_ppdu, arg->max_num_msdu_in_mpdu);

	ret = ath11k_wmi_cmd_send(ar->wmi, skb, WMI_PEER_TID_CONFIGURATIONS_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to submit WMI_PEER_TID_CONFIGURATIONS_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "wmi peer tid configuration cmd sent successfully\n");
	return ret;
}

int ath11k_wmi_vdev_create(struct ath11k *ar, u8 *macaddr,
			   struct vdev_create_params *param)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_vdev_create_cmd *cmd;
	struct sk_buff *skb;
	struct wmi_vdev_txrx_streams *txrx_streams;
	struct wmi_tlv *tlv;
	int ret, len;
	void *ptr;

	/* It can be optimized my sending tx/rx chain configuration
	 * only for supported bands instead of always sending it for
	 * both the bands.
	 */
	len = sizeof(*cmd) + TLV_HDR_SIZE +
		(WMI_NUM_SUPPORTED_BAND_MAX * sizeof(*txrx_streams));

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_create_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_VDEV_CREATE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = param->if_id;
	cmd->vdev_type = param->type;
	cmd->vdev_subtype = param->subtype;
	cmd->num_cfg_txrx_streams = WMI_NUM_SUPPORTED_BAND_MAX;
	cmd->pdev_id = param->pdev_id;
	cmd->mbssid_flags = param->mbssid_flags;
	cmd->mbssid_tx_vdev_id = param->mbssid_tx_vdev_id;

	ether_addr_copy(cmd->vdev_macaddr.addr, macaddr);

	ptr = skb->data + sizeof(*cmd);
	len = WMI_NUM_SUPPORTED_BAND_MAX * sizeof(*txrx_streams);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, len);

	ptr += TLV_HDR_SIZE;
	txrx_streams = ptr;
	len = sizeof(*txrx_streams);
	txrx_streams->tlv_header =
		FIELD_PREP(WMI_TLV_TAG, WMI_TAG_VDEV_TXRX_STREAMS) |
		FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	txrx_streams->band = WMI_TPC_CHAINMASK_CONFIG_BAND_2G;
	txrx_streams->supported_tx_streams =
				 param->chains[NL80211_BAND_2GHZ].tx;
	txrx_streams->supported_rx_streams =
				 param->chains[NL80211_BAND_2GHZ].rx;

	txrx_streams++;
	txrx_streams->tlv_header =
		FIELD_PREP(WMI_TLV_TAG, WMI_TAG_VDEV_TXRX_STREAMS) |
		FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	txrx_streams->band = WMI_TPC_CHAINMASK_CONFIG_BAND_5G;
	txrx_streams->supported_tx_streams =
				 param->chains[NL80211_BAND_5GHZ].tx;
	txrx_streams->supported_rx_streams =
				 param->chains[NL80211_BAND_5GHZ].rx;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_VDEV_CREATE_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to submit WMI_VDEV_CREATE_CMDID\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd vdev create id %d type %d subtype %d macaddr %pM pdevid %d\n",
		   param->if_id, param->type, param->subtype,
		   macaddr, param->pdev_id);

	return ret;
}

int ath11k_wmi_vdev_delete(struct ath11k *ar, u8 vdev_id)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_vdev_delete_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_delete_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_VDEV_DELETE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_VDEV_DELETE_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to submit WMI_VDEV_DELETE_CMDID\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "cmd vdev delete id %d\n", vdev_id);

	return ret;
}

int ath11k_wmi_vdev_stop(struct ath11k *ar, u8 vdev_id)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_vdev_stop_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_stop_cmd *)skb->data;

	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_VDEV_STOP_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_VDEV_STOP_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to submit WMI_VDEV_STOP cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "cmd vdev stop id 0x%x\n", vdev_id);

	return ret;
}

int ath11k_wmi_vdev_down(struct ath11k *ar, u8 vdev_id)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_vdev_down_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_down_cmd *)skb->data;

	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_VDEV_DOWN_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_VDEV_DOWN_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to submit WMI_VDEV_DOWN cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "cmd vdev down id 0x%x\n", vdev_id);

	return ret;
}

static void ath11k_wmi_put_wmi_channel(struct wmi_channel *chan,
				       struct wmi_vdev_start_req_arg *arg)
{
	u32 center_freq1 = arg->channel.band_center_freq1;

	memset(chan, 0, sizeof(*chan));

	chan->mhz = arg->channel.freq;
	chan->band_center_freq1 = arg->channel.band_center_freq1;

	if (arg->channel.mode == MODE_11AX_HE160) {
		if (arg->channel.freq > arg->channel.band_center_freq1)
			chan->band_center_freq1 = center_freq1 + 40;
		else
			chan->band_center_freq1 = center_freq1 - 40;

		chan->band_center_freq2 = arg->channel.band_center_freq1;

	} else if ((arg->channel.mode == MODE_11AC_VHT80_80) ||
		   (arg->channel.mode == MODE_11AX_HE80_80)) {
		chan->band_center_freq2 = arg->channel.band_center_freq2;
	} else {
		chan->band_center_freq2 = 0;
	}

	chan->info |= FIELD_PREP(WMI_CHAN_INFO_MODE, arg->channel.mode);
	if (arg->channel.passive)
		chan->info |= WMI_CHAN_INFO_PASSIVE;
	if (arg->channel.allow_ibss)
		chan->info |= WMI_CHAN_INFO_ADHOC_ALLOWED;
	if (arg->channel.allow_ht)
		chan->info |= WMI_CHAN_INFO_ALLOW_HT;
	if (arg->channel.allow_vht)
		chan->info |= WMI_CHAN_INFO_ALLOW_VHT;
	if (arg->channel.allow_he)
		chan->info |= WMI_CHAN_INFO_ALLOW_HE;
	if (arg->channel.ht40plus)
		chan->info |= WMI_CHAN_INFO_HT40_PLUS;
	if (arg->channel.chan_radar)
		chan->info |= WMI_CHAN_INFO_DFS;
	if (arg->channel.freq2_radar)
		chan->info |= WMI_CHAN_INFO_DFS_FREQ2;

	chan->reg_info_1 = FIELD_PREP(WMI_CHAN_REG_INFO1_MAX_PWR,
				      arg->channel.max_power) |
		FIELD_PREP(WMI_CHAN_REG_INFO1_MAX_REG_PWR,
			   arg->channel.max_reg_power);

	chan->reg_info_2 = FIELD_PREP(WMI_CHAN_REG_INFO2_ANT_MAX,
				      arg->channel.max_antenna_gain) |
		FIELD_PREP(WMI_CHAN_REG_INFO2_MAX_TX_PWR,
			   arg->channel.max_power);
}

int ath11k_wmi_vdev_start(struct ath11k *ar, struct wmi_vdev_start_req_arg *arg,
			  bool restart)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_vdev_start_request_cmd *cmd;
	struct sk_buff *skb;
	struct wmi_channel *chan;
	struct wmi_tlv *tlv;
	void *ptr;
	int ret, len;

	if (WARN_ON(arg->ssid_len > sizeof(cmd->ssid.ssid)))
		return -EINVAL;

	len = sizeof(*cmd) + sizeof(*chan) + TLV_HDR_SIZE;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_start_request_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_VDEV_START_REQUEST_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = arg->vdev_id;
	cmd->beacon_interval = arg->bcn_intval;
	cmd->bcn_tx_rate = arg->bcn_tx_rate;
	cmd->dtim_period = arg->dtim_period;
	cmd->num_noa_descriptors = arg->num_noa_descriptors;
	cmd->preferred_rx_streams = arg->pref_rx_streams;
	cmd->preferred_tx_streams = arg->pref_tx_streams;
	cmd->cac_duration_ms = arg->cac_duration_ms;
	cmd->regdomain = arg->regdomain;
	cmd->he_ops = arg->he_ops;
	cmd->mbssid_flags = arg->mbssid_flags;
	cmd->mbssid_tx_vdev_id = arg->mbssid_tx_vdev_id;

	if (!restart) {
		if (arg->ssid) {
			cmd->ssid.ssid_len = arg->ssid_len;
			memcpy(cmd->ssid.ssid, arg->ssid, arg->ssid_len);
		}
		if (arg->hidden_ssid)
			cmd->flags |= WMI_VDEV_START_HIDDEN_SSID;
		if (arg->pmf_enabled)
			cmd->flags |= WMI_VDEV_START_PMF_ENABLED;
	}

	cmd->flags |= WMI_VDEV_START_LDPC_RX_ENABLED;
	if (test_bit(ATH11K_FLAG_HW_CRYPTO_DISABLED, &ar->ab->dev_flags))
		cmd->flags |= WMI_VDEV_START_HW_ENCRYPTION_DISABLED;

	ptr = skb->data + sizeof(*cmd);
	chan = ptr;

	ath11k_wmi_put_wmi_channel(chan, arg);

	chan->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_CHANNEL) |
			   FIELD_PREP(WMI_TLV_LEN,
				      sizeof(*chan) - TLV_HDR_SIZE);
	ptr += sizeof(*chan);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, 0);

	/* Note: This is a nested TLV containing:
	 * [wmi_tlv][ath11k_wmi_p2p_noa_descriptor][wmi_tlv]..
	 */

	ptr += sizeof(*tlv);

	if (restart)
		ret = ath11k_wmi_cmd_send(wmi, skb,
					  WMI_VDEV_RESTART_REQUEST_CMDID);
	else
		ret = ath11k_wmi_cmd_send(wmi, skb,
					  WMI_VDEV_START_REQUEST_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to submit vdev_%s cmd\n",
			    restart ? "restart" : "start");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "cmd vdev %s id 0x%x freq 0x%x mode 0x%x\n",
		   restart ? "restart" : "start", arg->vdev_id,
		   arg->channel.freq, arg->channel.mode);

	return ret;
}

int ath11k_wmi_vdev_up(struct ath11k *ar, struct vdev_up_params *params)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_vdev_up_cmd *cmd;
	struct ieee80211_bss_conf *bss_conf;
	struct ath11k_vif *arvif;
	struct sk_buff *skb;
	int ret;

	arvif = ath11k_mac_get_arvif(ar, params->vdev_id);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_up_cmd *)skb->data;

	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_VDEV_UP_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = params->vdev_id;
	cmd->vdev_assoc_id = params->aid;
	ether_addr_copy(cmd->vdev_bssid.addr, params->bssid);
	cmd->nontx_profile_idx = params->profile_idx;
	cmd->nontx_profile_cnt = params->profile_count;
	if (params->tx_bssid)
		ether_addr_copy(cmd->tx_vdev_bssid.addr, params->tx_bssid);

	if (params->tx_bssid)
		ether_addr_copy(cmd->tx_vdev_bssid.addr, params->tx_bssid);

	if (arvif && arvif->vif->type == NL80211_IFTYPE_STATION) {
		bss_conf = &arvif->vif->bss_conf;

		if (bss_conf->nontransmitted) {
			ether_addr_copy(cmd->tx_vdev_bssid.addr,
					bss_conf->transmitter_bssid);
			cmd->nontx_profile_idx = bss_conf->bssid_index;
			cmd->nontx_profile_cnt = bss_conf->bssid_indicator;
		}
	}

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_VDEV_UP_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to submit WMI_VDEV_UP cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd vdev up id 0x%x assoc id %d bssid %pM\n",
		   params->vdev_id, params->aid, params->bssid);

	return ret;
}

int ath11k_wmi_send_peer_create_cmd(struct ath11k *ar,
				    struct peer_create_params *param)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_peer_create_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_create_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PEER_CREATE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	ether_addr_copy(cmd->peer_macaddr.addr, param->peer_addr);
	cmd->peer_type = param->peer_type;
	cmd->vdev_id = param->vdev_id;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PEER_CREATE_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to submit WMI_PEER_CREATE cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd peer create vdev_id %d peer_addr %pM\n",
		   param->vdev_id, param->peer_addr);

	return ret;
}

int ath11k_wmi_send_peer_delete_cmd(struct ath11k *ar,
				    const u8 *peer_addr, u8 vdev_id)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_peer_delete_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_delete_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PEER_DELETE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	ether_addr_copy(cmd->peer_macaddr.addr, peer_addr);
	cmd->vdev_id = vdev_id;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PEER_DELETE_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_PEER_DELETE cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd peer delete vdev_id %d peer_addr %pM\n",
		   vdev_id,  peer_addr);

	return ret;
}

int ath11k_wmi_send_add_update_wds_entry_cmd(struct ath11k *ar,
					     const u8 *peer_addr,
					     const u8 *wds_addr, u8 vdev_id,
					     bool add_wds)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_add_wds_entry_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_add_wds_entry_cmd *)skb->data;
	if (add_wds)
		cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PEER_ADD_WDS_ENTRY_CMD) |
				  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	else
		cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PEER_UPDATE_WDS_ENTRY_CMD) |
				  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	ether_addr_copy(cmd->peer_macaddr.addr, peer_addr);
	ether_addr_copy(cmd->wds_macaddr.addr, wds_addr);
	cmd->vdev_id = vdev_id;
	cmd->flags = WMI_WDS_FLAG_STATIC;

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "WMI add WDS entry vdev_id %d peer_addr %pM, wds_addr %pM flags %x\n",
		   vdev_id, peer_addr, wds_addr, cmd->flags);

	if (add_wds)
		ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PEER_ADD_WDS_ENTRY_CMDID);
	else
		ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PEER_UPDATE_WDS_ENTRY_CMDID);

	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_PEER_%s_WDS_ENTRY cmd\n",
			    add_wds ? "ADD" : "UPDATE");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath11k_wmi_send_pdev_set_regdomain(struct ath11k *ar,
				       struct pdev_set_regdomain_params *param)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_pdev_set_regdomain_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_regdomain_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PDEV_SET_REGDOMAIN_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->reg_domain = param->current_rd_in_use;
	cmd->reg_domain_2g = param->current_rd_2g;
	cmd->reg_domain_5g = param->current_rd_5g;
	cmd->conformance_test_limit_2g = param->ctl_2g;
	cmd->conformance_test_limit_5g = param->ctl_5g;
	cmd->dfs_domain = param->dfs_domain;
	cmd->pdev_id = param->pdev_id;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PDEV_SET_REGDOMAIN_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_PDEV_SET_REGDOMAIN cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd pdev regd rd %d rd2g %d rd5g %d domain %d pdev id %d\n",
		   param->current_rd_in_use, param->current_rd_2g,
		   param->current_rd_5g, param->dfs_domain, param->pdev_id);

	return ret;
}

int ath11k_wmi_send_pdev_pkt_route(struct ath11k *ar, struct ath11k_wmi_pkt_route_param *param)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_pdev_pkt_route_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_pkt_route_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PDEV_UPDATE_PKT_ROUTING_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->pdev_id = ar->pdev->pdev_id;
	cmd->opcode = param->opcode;
	cmd->route_type_bmap = param->route_type_bmap;
	cmd->dst_ring = param->dst_ring;
	cmd->meta_data = param->meta_data;
	cmd->dst_ring_handler = param->dst_ring_handler;

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "WMI pdev pkt route opcode %d route_bmap %d dst_ring %d meta_datan %d dst_ringg_handler %d\n",
		   param->opcode, param->route_type_bmap,
		   param->dst_ring, param->meta_data, param->dst_ring_handler);

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PDEV_UPDATE_PKT_ROUTING_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_PDEV_UPDATE_PKT_ROUTING cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath11k_wmi_set_peer_param(struct ath11k *ar, const u8 *peer_addr,
			      u32 vdev_id, u32 param_id, u32 param_val)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_peer_set_param_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_set_param_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PEER_SET_PARAM_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	ether_addr_copy(cmd->peer_macaddr.addr, peer_addr);
	cmd->vdev_id = vdev_id;
	cmd->param_id = param_id;
	cmd->param_value = param_val;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PEER_SET_PARAM_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_PEER_SET_PARAM cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd peer set param vdev %d peer 0x%pM set param %d value %d\n",
		   vdev_id, peer_addr, param_id, param_val);

	return ret;
}

int ath11k_wmi_send_peer_flush_tids_cmd(struct ath11k *ar,
					u8 peer_addr[ETH_ALEN],
					struct peer_flush_params *param)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_peer_flush_tids_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_flush_tids_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PEER_FLUSH_TIDS_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	ether_addr_copy(cmd->peer_macaddr.addr, peer_addr);
	cmd->peer_tid_bitmap = param->peer_tid_bitmap;
	cmd->vdev_id = param->vdev_id;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PEER_FLUSH_TIDS_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_PEER_FLUSH_TIDS cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd peer flush tids vdev_id %d peer_addr %pM tids %08x\n",
		   param->vdev_id, peer_addr, param->peer_tid_bitmap);

	return ret;
}

int ath11k_wmi_peer_rx_reorder_queue_setup(struct ath11k *ar,
					   int vdev_id, const u8 *addr,
					   dma_addr_t paddr, u8 tid,
					   u8 ba_window_size_valid,
					   u32 ba_window_size)
{
	struct wmi_peer_reorder_queue_setup_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_reorder_queue_setup_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_REORDER_QUEUE_SETUP_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	ether_addr_copy(cmd->peer_macaddr.addr, addr);
	cmd->vdev_id = vdev_id;
	cmd->tid = tid;
	cmd->queue_ptr_lo = lower_32_bits(paddr);
	cmd->queue_ptr_hi = upper_32_bits(paddr);
	cmd->queue_no = tid;
	cmd->ba_window_size_valid = ba_window_size_valid;
	cmd->ba_window_size = ba_window_size;

	ret = ath11k_wmi_cmd_send(ar->wmi, skb,
				  WMI_PEER_REORDER_QUEUE_SETUP_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_PEER_REORDER_QUEUE_SETUP\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd peer reorder queue setup addr %pM vdev_id %d tid %d\n",
		   addr, vdev_id, tid);

	return ret;
}

int
ath11k_wmi_rx_reord_queue_remove(struct ath11k *ar,
				 struct rx_reorder_queue_remove_params *param)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_peer_reorder_queue_remove_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_reorder_queue_remove_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_REORDER_QUEUE_REMOVE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	ether_addr_copy(cmd->peer_macaddr.addr, param->peer_macaddr);
	cmd->vdev_id = param->vdev_id;
	cmd->tid_mask = param->peer_tid_bitmap;

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_PEER_REORDER_QUEUE_REMOVE_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_PEER_REORDER_QUEUE_REMOVE_CMDID");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd peer reorder queue remove peer_macaddr %pM vdev_id %d tid_map %d",
		   param->peer_macaddr, param->vdev_id, param->peer_tid_bitmap);

	return ret;
}

int ath11k_wmi_pdev_ap_ps_cmd_send(struct ath11k *ar, u8 pdev_id,
				   u32 param_value)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_pdev_ap_ps_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_ap_ps_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PDEV_GREEN_AP_PS_ENABLE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->pdev_id = pdev_id;
	cmd->param_value = param_value;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PDEV_GREEN_AP_PS_ENABLE_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send ap ps enable/disable cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "wmi pdev ap ps set pdev id %d value %d\n",
		   pdev_id, param_value);

	return ret;
}

int ath11k_wmi_pdev_set_param(struct ath11k *ar, u32 param_id,
			      u32 param_value, u8 pdev_id)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_pdev_set_param_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_param_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PDEV_SET_PARAM_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->pdev_id = pdev_id;
	cmd->param_id = param_id;
	cmd->param_value = param_value;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PDEV_SET_PARAM_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_PDEV_SET_PARAM cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd pdev set param %d pdev id %d value %d\n",
		   param_id, pdev_id, param_value);

	return ret;
}

int ath11k_wmi_pdev_set_ps_mode(struct ath11k *ar, int vdev_id,
				enum wmi_sta_ps_mode psmode)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_pdev_set_ps_mode_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_ps_mode_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_STA_POWERSAVE_MODE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	cmd->sta_ps_mode = psmode;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_STA_POWERSAVE_MODE_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_PDEV_SET_PARAM cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd sta powersave mode psmode %d vdev id %d\n",
		   psmode, vdev_id);

	return ret;
}

int ath11k_wmi_pdev_suspend(struct ath11k *ar, u32 suspend_opt,
			    u32 pdev_id)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_pdev_suspend_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_suspend_cmd *)skb->data;

	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PDEV_SUSPEND_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->suspend_opt = suspend_opt;
	cmd->pdev_id = pdev_id;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PDEV_SUSPEND_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_PDEV_SUSPEND cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd pdev suspend pdev_id %d\n", pdev_id);

	return ret;
}

int ath11k_wmi_pdev_resume(struct ath11k *ar, u32 pdev_id)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_pdev_resume_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_resume_cmd *)skb->data;

	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PDEV_RESUME_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->pdev_id = pdev_id;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PDEV_RESUME_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_PDEV_RESUME cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd pdev resume pdev id %d\n", pdev_id);

	return ret;
}

/* TODO FW Support for the cmd is not available yet.
 * Can be tested once the command and corresponding
 * event is implemented in FW
 */
int ath11k_wmi_pdev_bss_chan_info_request(struct ath11k *ar,
					  enum wmi_bss_chan_info_req_type type)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_pdev_bss_chan_info_req_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_bss_chan_info_req_cmd *)skb->data;

	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PDEV_BSS_CHAN_INFO_REQUEST) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->req_type = type;
	cmd->pdev_id = ar->pdev->pdev_id;

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_BSS_CHAN_INFO_REQUEST_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_PDEV_BSS_CHAN_INFO_REQUEST cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd pdev bss chan info request type %d\n", type);

	return ret;
}

int ath11k_wmi_send_set_ap_ps_param_cmd(struct ath11k *ar, u8 *peer_addr,
					struct ap_ps_params *param)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_ap_ps_peer_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_ap_ps_peer_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_AP_PS_PEER_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = param->vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, peer_addr);
	cmd->param = param->param;
	cmd->value = param->value;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_AP_PS_PEER_PARAM_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_AP_PS_PEER_PARAM_CMDID\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd ap ps peer param vdev id %d peer %pM param %d value %d\n",
		   param->vdev_id, peer_addr, param->param, param->value);

	return ret;
}

int ath11k_wmi_set_sta_ps_param(struct ath11k *ar, u32 vdev_id,
				u32 param, u32 param_value)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_sta_powersave_param_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_sta_powersave_param_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_STA_POWERSAVE_PARAM_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = vdev_id;
	cmd->param = param;
	cmd->value = param_value;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_STA_POWERSAVE_PARAM_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_STA_POWERSAVE_PARAM_CMDID");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd set powersave param vdev_id %d param %d value %d\n",
		   vdev_id, param, param_value);

	return ret;
}

int ath11k_wmi_force_fw_hang_cmd(struct ath11k *ar, u32 type, u32 delay_time_ms)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_force_fw_hang_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_force_fw_hang_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_FORCE_FW_HANG_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);

	cmd->type = type;
	cmd->delay_time_ms = delay_time_ms;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_FORCE_FW_HANG_CMDID);

	if (ret) {
		ath11k_warn(ar->ab, "Failed to send WMI_FORCE_FW_HANG_CMDID");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "cmd force fw hang");

	return ret;
}

int ath11k_wmi_vdev_set_param_cmd(struct ath11k *ar, u32 vdev_id,
				  u32 param_id, u32 param_value)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_vdev_set_param_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_set_param_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_VDEV_SET_PARAM_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = vdev_id;
	cmd->param_id = param_id;
	cmd->param_value = param_value;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_VDEV_SET_PARAM_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_VDEV_SET_PARAM_CMDID\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd vdev set param vdev 0x%x param %d value %d\n",
		   vdev_id, param_id, param_value);

	return ret;
}

static void ath11k_wmi_copy_coex_config(struct ath11k *ar, struct wmi_coex_config_cmd *cmd,
				       struct coex_config_arg *coex_config)
{
	if (coex_config->config_type == WMI_COEX_CONFIG_BTC_ENABLE) {
		cmd->coex_enable = coex_config->coex_enable;
		ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
			   "WMI coex config type %u vdev id %d  coex_enable %u\n",
			   coex_config->config_type, coex_config->vdev_id,
			   coex_config->coex_enable);
	}

	if (coex_config->config_type == WMI_COEX_CONFIG_WLAN_PKT_PRIORITY) {
		cmd->wlan_pkt_type = coex_config->wlan_pkt_type;
		cmd->wlan_pkt_weight = coex_config->wlan_pkt_weight;
		ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
			   "WMI coex config type %u vdev id %d  wlan pkt type 0x%x wlan pkt weight %u\n",
			   coex_config->config_type, coex_config->vdev_id,
			   coex_config->wlan_pkt_type, coex_config->wlan_pkt_weight);
	}

	if (coex_config->config_type == WMI_COEX_CONFIG_PTA_INTERFACE) {
		cmd->pta_num = coex_config->pta_num;
		cmd->coex_mode = coex_config->coex_mode;
		cmd->bt_txrx_time = coex_config->bt_txrx_time;
		cmd->bt_priority_time = coex_config->bt_priority_time;
		cmd->pta_algorithm = coex_config->pta_algorithm;
		cmd->pta_priority = coex_config->pta_priority;
		ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
			   "WMI coex config type %u vdev id %d  pta num %u coex mode %u bt_txrx_time %u bt_priority_time %u pta alogrithm %u pta priority %u\n",
			   coex_config->config_type, coex_config->vdev_id,
			   coex_config->pta_num, coex_config->coex_mode,
			   coex_config->bt_txrx_time, coex_config->bt_priority_time,
			   coex_config->pta_algorithm, coex_config->pta_priority);
	}

	if (coex_config->config_type == WMI_COEX_CONFIG_AP_TDM) {
		cmd->duty_cycle = coex_config->duty_cycle;
		cmd->wlan_duration = coex_config->wlan_duration;
		ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
			   "WMI coex config type %u vdev id %d  duty_cycle %u wlan_duration %u\n",
			    coex_config->config_type,
			    coex_config->vdev_id,
			    coex_config->duty_cycle,
			    coex_config->wlan_duration);
        }

	if (coex_config->config_type == WMI_COEX_CONFIG_FORCED_ALGO) {
		cmd->coex_algo = coex_config->coex_algo;
		ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
			   "WMI coex config type %u vdev id %d coex_algorithm %u\n",
			   coex_config->config_type,
			   coex_config->vdev_id,
			   coex_config->coex_algo);
	}

	if (coex_config->config_type == WMI_COEX_CONFIG_THREE_WAY_COEX_START ||
	    coex_config->config_type == WMI_COEX_CONFIG_THREE_WAY_COEX_RESET) {
		cmd->priority0 = coex_config->priority0;
		cmd->priority1 = coex_config->priority1;
		cmd->priority2 = coex_config->priority2;
		ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
			   "WMI coex config type %u vdev id %d priority0 : %u"
			   " priority1 : %u priority2 : %u", coex_config->config_type,
			   coex_config->vdev_id, coex_config->priority0,
			   coex_config->priority1, coex_config->priority2);
	}
}

int ath11k_send_coex_config_cmd(struct ath11k *ar,
				struct coex_config_arg *coex_config)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_coex_config_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_coex_config_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_COEX_CONFIG_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = coex_config->vdev_id;
	cmd->config_type = coex_config->config_type;
	ath11k_wmi_copy_coex_config(ar, cmd, coex_config);

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_COEX_CONFIG_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_COEX_CONFIG_CMD cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath11k_wmi_send_stats_request_cmd(struct ath11k *ar,
				      struct stats_request_params *param)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_request_stats_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_request_stats_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_REQUEST_STATS_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->stats_id = param->stats_id;
	cmd->vdev_id = param->vdev_id;
	cmd->pdev_id = param->pdev_id;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_REQUEST_STATS_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_REQUEST_STATS cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd request stats 0x%x vdev id %d pdev id %d\n",
		   param->stats_id, param->vdev_id, param->pdev_id);

	return ret;
}

static int ath11k_wmi_awgn_intf_subtlv_parser(struct ath11k_base *ab,
					      u16 tag, u16 len,
					      const void *ptr, void *data)
{
	int ret = 0;
	struct wmi_dcs_awgn_info *awgn_info;

	switch (tag) {
	case WMI_TAG_DCS_AWGN_INT_TYPE:
		awgn_info = (struct wmi_dcs_awgn_info *)ptr;

		ath11k_dbg(ab, ATH11K_DBG_WMI,
			   "AWGN Info: channel width: %d, chan freq: %d, center_freq0: %d, center_freq1: %d, bw_intf_bitmap: %d\n",
			   awgn_info->channel_width, awgn_info->chan_freq, awgn_info->center_freq0, awgn_info->center_freq1,
			   awgn_info->chan_bw_interference_bitmap);
		memcpy(data, awgn_info, sizeof(*awgn_info));
		break;
	default:
		ath11k_warn(ab,
			    "Received invalid tag for wmi dcs interference in subtlvs\n");
		return -EINVAL;
		break;
	}

	return ret;
}

static int ath11k_wmi_dcs_awgn_event_parser(struct ath11k_base *ab,
					    u16 tag, u16 len,
					    const void *ptr, void *data)
{
	int ret = 0;

	ath11k_dbg(ab, ATH11K_DBG_WMI, "wmi dcs awgn event tag 0x%x of len %d rcvd\n",
		   tag, len);

	switch (tag) {
	case WMI_TAG_DCS_INTERFERENCE_EVENT:
		/* Fixed param is already processed*/
		break;
	case WMI_TAG_ARRAY_STRUCT:
		/* len 0 is expected for array of struct when there
		 * is no content of that type to pack inside that tlv
		 */
		if (len == 0)
			return 0;
		ret = ath11k_wmi_tlv_iter(ab, ptr, len,
					  ath11k_wmi_awgn_intf_subtlv_parser,
					  data);
		break;
	default:
		ath11k_warn(ab, "Received invalid tag for wmi dcs interference event\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

bool ath11k_wmi_validate_dcs_awgn_info(struct ath11k *ar, struct wmi_dcs_awgn_info *awgn_info)
{
	spin_lock_bh(&ar->data_lock);

	if (!ar->rx_channel) {
		spin_unlock_bh(&ar->data_lock);
		return false;
	}

	if (awgn_info->chan_freq != ar->rx_channel->center_freq) {
		spin_unlock_bh(&ar->data_lock);
		ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
			   "dcs interference event received with wrong channel %d",awgn_info->chan_freq);
		return false;
	}
	spin_unlock_bh(&ar->data_lock);

	switch (awgn_info->channel_width) {
	case WMI_HOST_CHAN_WIDTH_20:
		if (awgn_info->chan_bw_interference_bitmap > WMI_DCS_SEG_PRI20) {
			ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
				   "dcs interference event received with wrong chan width bmap %d for 20MHz",
				   awgn_info->chan_bw_interference_bitmap);
			return false;
		}
		break;
	case WMI_HOST_CHAN_WIDTH_40:
		if (awgn_info->chan_bw_interference_bitmap > WMI_DCS_SEG_SEC20) {
			ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
				   "dcs interference event received with wrong chan width bmap %d for 40MHz",
				   awgn_info->chan_bw_interference_bitmap);
			return false;
		}
		break;
	case WMI_HOST_CHAN_WIDTH_80:
		if (awgn_info->chan_bw_interference_bitmap > WMI_DCS_SEG_SEC40) {
			ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
				   "dcs interference event received with wrong chan width bmap %d for 80MHz",
				   awgn_info->chan_bw_interference_bitmap);
			return false;
		}
		break;
	case WMI_HOST_CHAN_WIDTH_160:
	case WMI_HOST_CHAN_WIDTH_80P80:
		if (awgn_info->chan_bw_interference_bitmap > WMI_DCS_SEG_SEC80) {
			ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
				   "dcs interference event received with wrong chan width bmap %d for 80P80/160MHz",
				   awgn_info->chan_bw_interference_bitmap);
			return false;
		}
		break;
	default:
		ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
			   "dcs interference event received with unknown channel width %d",
			   awgn_info->channel_width);
		return false;
	}
	return true;
}

static void
ath11k_wmi_dcs_awgn_interference_event(struct ath11k_base *ab,
				       struct sk_buff *skb)
{
	const struct wmi_dcs_interference_ev *dcs_intf_ev;
	struct wmi_dcs_awgn_info awgn_info = {};
	struct cfg80211_chan_def *chandef = NULL;
	struct ath11k *ar;
	const struct wmi_tlv *tlv;
	u16 tlv_tag;
	u8 *ptr;
	int ret;

	if (!test_bit(WMI_TLV_SERVICE_DCS_AWGN_INT_SUPPORT, ab->wmi_ab.svc_map)) {
		ath11k_warn(ab, "firmware doesn't support awgn interference, so dropping dcs interference ev\n");
		return;
	}

	ptr = skb->data;

	if (skb->len < (sizeof(*dcs_intf_ev) + TLV_HDR_SIZE)) {
		ath11k_warn(ab, "dcs interference event size invalid\n");
		return;
	}

	tlv = (struct wmi_tlv *)ptr;
	tlv_tag = FIELD_GET(WMI_TLV_TAG, tlv->header);
	ptr += sizeof(*tlv);

	if (tlv_tag == WMI_TAG_DCS_INTERFERENCE_EVENT) {
		dcs_intf_ev = (struct wmi_dcs_interference_ev*)ptr;

		ath11k_dbg(ab, ATH11K_DBG_WMI,
			   "pdev awgn detected on pdev %d, interference type %d\n",
			   dcs_intf_ev->pdev_id, dcs_intf_ev->interference_type);

		if (dcs_intf_ev->interference_type != WMI_DCS_AWGN_INTF) {
			ath11k_warn(ab, "interference type is not awgn\n");
			return;
		}
	} else {
		ath11k_warn(ab, "dcs interference event received with wrong tag\n");
		return;
	}

	ret = ath11k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath11k_wmi_dcs_awgn_event_parser,
				  &awgn_info);
	if (ret) {
		ath11k_warn(ab, "failed to parse awgn tlv %d\n", ret);
		return;
	}

	rcu_read_lock();
	ar = ath11k_mac_get_ar_by_pdev_id(ab, dcs_intf_ev->pdev_id);
	if (!ar) {
		ath11k_warn(ab, "awgn detected in invalid pdev id(%d)\n",
			    dcs_intf_ev->pdev_id);
		goto exit;
	}

	if (!ar->supports_6ghz) {
		ath11k_warn(ab, "pdev does not supports 6G, so dropping dcs interference event\n");
		goto exit;
	}

	if (ar->awgn_intf_handling_in_prog)
		goto exit;

	if (!ath11k_wmi_validate_dcs_awgn_info(ar, &awgn_info)) {
		ath11k_warn(ab, "Invalid DCS AWGN TLV - Skipping event");
		goto exit;
	}

	ath11k_info(ab, "Interface(pdev %d) : AWGN interference detected\n",
		    dcs_intf_ev->pdev_id);

	ieee80211_iter_chan_contexts_atomic(ar->hw, ath11k_mac_get_any_chandef_iter,
					    &chandef);
	if (!chandef) {
		ath11k_warn(ab, "chandef is not available\n");
		goto exit;
	}
	ar->awgn_chandef = *chandef;

	ieee80211_awgn_detected(ar->hw, awgn_info.chan_bw_interference_bitmap, NULL);

	spin_lock_bh(&ar->data_lock);
	/* Incase of mesh intf presence, dont set in prog as there will be no
	   Channel/BW change happening. */
	ar->awgn_intf_handling_in_prog = (ar->num_mesh_vdevs ? false : true);
	ar->chan_bw_interference_bitmap = awgn_info.chan_bw_interference_bitmap;
	spin_unlock_bh(&ar->data_lock);

	ath11k_dbg(ab, ATH11K_DBG_WMI, "AWGN : Interference handling started\n");
exit:
	rcu_read_unlock();
}

int ath11k_wmi_send_pdev_temperature_cmd(struct ath11k *ar)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_get_pdev_temperature_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_get_pdev_temperature_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PDEV_GET_TEMPERATURE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PDEV_GET_TEMPERATURE_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_PDEV_GET_TEMPERATURE cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd pdev get temperature for pdev_id %d\n", ar->pdev->pdev_id);

	return ret;
}

int ath11k_wmi_send_bcn_offload_control_cmd(struct ath11k *ar,
					    u32 vdev_id, u32 bcn_ctrl_op)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_bcn_offload_ctrl_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_bcn_offload_ctrl_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_BCN_OFFLOAD_CTRL_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = vdev_id;
	cmd->bcn_ctrl_op = bcn_ctrl_op;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_BCN_OFFLOAD_CTRL_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_BCN_OFFLOAD_CTRL_CMDID\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd bcn offload ctrl vdev id %d ctrl_op %d\n",
		   vdev_id, bcn_ctrl_op);

	return ret;
}

int ath11k_wmi_p2p_go_bcn_ie(struct ath11k *ar, u32 vdev_id,
			     const u8 *p2p_ie)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_p2p_go_set_beacon_ie_cmd *cmd;
	size_t p2p_ie_len, aligned_len;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	int ret, len;

	p2p_ie_len = p2p_ie[1] + 2;
	aligned_len = roundup(p2p_ie_len, 4);

	len = sizeof(*cmd) + TLV_HDR_SIZE + aligned_len;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_p2p_go_set_beacon_ie_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_P2P_GO_SET_BEACON_IE) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	cmd->ie_buf_len = p2p_ie_len;

	tlv = (struct wmi_tlv *)cmd->tlv;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_BYTE) |
		      FIELD_PREP(WMI_TLV_LEN, aligned_len);
	memcpy(tlv->value, p2p_ie, p2p_ie_len);

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_P2P_GO_SET_BEACON_IE);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_P2P_GO_SET_BEACON_IE\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath11k_wmi_bcn_tmpl(struct ath11k *ar, u32 vdev_id,
			struct ieee80211_mutable_offsets *offs,
			struct sk_buff *bcn, u32 ema_params)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_bcn_tmpl_cmd *cmd;
	struct wmi_bcn_prb_info *bcn_prb_info;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int ret, len;
	size_t aligned_len = roundup(bcn->len, 4);
	struct ieee80211_vif *vif;
	struct ath11k_vif *arvif = ath11k_mac_get_arvif(ar, vdev_id);

	if (!arvif) {
		ath11k_warn(ar->ab, "failed to find arvif with vdev id %d\n", vdev_id);
		return -EINVAL;
	}

	vif = arvif->vif;

	len = sizeof(*cmd) + sizeof(*bcn_prb_info) + TLV_HDR_SIZE + aligned_len;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_bcn_tmpl_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_BCN_TMPL_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	cmd->tim_ie_offset = offs->tim_offset;

	if (vif->bss_conf.csa_active) {
		cmd->csa_switch_count_offset = offs->cntdwn_counter_offs[0];
		cmd->ext_csa_switch_count_offset = offs->cntdwn_counter_offs[1];
	}

	cmd->buf_len = bcn->len;
	cmd->mbssid_ie_offset = offs->mbssid_off;
	cmd->ema_params = ema_params;

	ptr = skb->data + sizeof(*cmd);

	bcn_prb_info = ptr;
	len = sizeof(*bcn_prb_info);
	bcn_prb_info->tlv_header = FIELD_PREP(WMI_TLV_TAG,
					      WMI_TAG_BCN_PRB_INFO) |
				   FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	bcn_prb_info->caps = 0;
	bcn_prb_info->erp = 0;

	ptr += sizeof(*bcn_prb_info);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_BYTE) |
		      FIELD_PREP(WMI_TLV_LEN, aligned_len);
	memcpy(tlv->value, bcn->data, bcn->len);

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_BCN_TMPL_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_BCN_TMPL_CMDID\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "cmd bcn tmpl");

	return ret;
}

int ath11k_wmi_vdev_install_key(struct ath11k *ar,
				struct wmi_vdev_install_key_arg *arg)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_vdev_install_key_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	int ret, len;
	int key_len_aligned = roundup(arg->key_len, sizeof(uint32_t));

	len = sizeof(*cmd) + TLV_HDR_SIZE + key_len_aligned;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_install_key_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_VDEV_INSTALL_KEY_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = arg->vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, arg->macaddr);
	cmd->key_idx = arg->key_idx;
	cmd->key_flags = arg->key_flags;
	cmd->key_cipher = arg->key_cipher;
	cmd->key_len = arg->key_len;
	cmd->key_txmic_len = arg->key_txmic_len;
	cmd->key_rxmic_len = arg->key_rxmic_len;
	cmd->group_key_id = arg->group_key_idx;

	if (arg->key_rsc_counter)
		memcpy(&cmd->key_rsc_counter, &arg->key_rsc_counter,
		       sizeof(struct wmi_key_seq_counter));

	tlv = (struct wmi_tlv *)(skb->data + sizeof(*cmd));
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_BYTE) |
		      FIELD_PREP(WMI_TLV_LEN, key_len_aligned);
	memcpy(tlv->value, (u8 *)arg->key_data, key_len_aligned);

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_VDEV_INSTALL_KEY_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_VDEV_INSTALL_KEY cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd vdev install key idx %d cipher %d len %d\n",
		   arg->key_idx, arg->key_cipher, arg->key_len);

	return ret;
}

static inline void
ath11k_wmi_copy_peer_flags(struct wmi_peer_assoc_complete_cmd *cmd,
			   struct peer_assoc_params *param,
			   bool hw_crypto_disabled)
{
	cmd->peer_flags = 0;

	if (param->is_wme_set) {
		if (param->qos_flag)
			cmd->peer_flags |= WMI_PEER_QOS;
		if (param->apsd_flag)
			cmd->peer_flags |= WMI_PEER_APSD;
		if (param->ht_flag)
			cmd->peer_flags |= WMI_PEER_HT;
		if (param->bw_40)
			cmd->peer_flags |= WMI_PEER_40MHZ;
		if (param->bw_80)
			cmd->peer_flags |= WMI_PEER_80MHZ;
		if (param->bw_160)
			cmd->peer_flags |= WMI_PEER_160MHZ;

		/* Typically if STBC is enabled for VHT it should be enabled
		 * for HT as well
		 **/
		if (param->stbc_flag)
			cmd->peer_flags |= WMI_PEER_STBC;

		/* Typically if LDPC is enabled for VHT it should be enabled
		 * for HT as well
		 **/
		if (param->ldpc_flag)
			cmd->peer_flags |= WMI_PEER_LDPC;

		if (param->static_mimops_flag)
			cmd->peer_flags |= WMI_PEER_STATIC_MIMOPS;
		if (param->dynamic_mimops_flag)
			cmd->peer_flags |= WMI_PEER_DYN_MIMOPS;
		if (param->spatial_mux_flag)
			cmd->peer_flags |= WMI_PEER_SPATIAL_MUX;
		if (param->vht_flag)
			cmd->peer_flags |= WMI_PEER_VHT;
		if (param->he_flag)
			cmd->peer_flags |= WMI_PEER_HE;
		if (param->twt_requester)
			cmd->peer_flags |= WMI_PEER_TWT_REQ;
		if (param->twt_responder)
			cmd->peer_flags |= WMI_PEER_TWT_RESP;
	}

	/* Suppress authorization for all AUTH modes that need 4-way handshake
	 * (during re-association).
	 * Authorization will be done for these modes on key installation.
	 */
	if (param->auth_flag)
		cmd->peer_flags |= WMI_PEER_AUTH;
	if (param->need_ptk_4_way) {
		cmd->peer_flags |= WMI_PEER_NEED_PTK_4_WAY;
		if (!hw_crypto_disabled && param->is_assoc)
			cmd->peer_flags &= ~WMI_PEER_AUTH;
	}
	if (param->need_gtk_2_way)
		cmd->peer_flags |= WMI_PEER_NEED_GTK_2_WAY;
	/* safe mode bypass the 4-way handshake */
	if (param->safe_mode_enabled)
		cmd->peer_flags &= ~(WMI_PEER_NEED_PTK_4_WAY |
				     WMI_PEER_NEED_GTK_2_WAY);

	if (param->is_pmf_enabled)
		cmd->peer_flags |= WMI_PEER_PMF;

	/* Disable AMSDU for station transmit, if user configures it */
	/* Disable AMSDU for AP transmit to 11n Stations, if user configures
	 * it
	 * if (param->amsdu_disable) Add after FW support
	 **/

	/* Target asserts if node is marked HT and all MCS is set to 0.
	 * Mark the node as non-HT if all the mcs rates are disabled through
	 * iwpriv
	 **/
	if (param->peer_ht_rates.num_rates == 0)
		cmd->peer_flags &= ~WMI_PEER_HT;
}

int ath11k_wmi_send_peer_assoc_cmd(struct ath11k *ar,
				   struct peer_assoc_params *param, bool debug)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_peer_assoc_complete_cmd *cmd;
	struct wmi_vht_rate_set *mcs;
	struct wmi_he_rate_set *he_mcs;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;
	u32 peer_legacy_rates_align;
	u32 peer_ht_rates_align;
	int i, ret, len;

	peer_legacy_rates_align = roundup(param->peer_legacy_rates.num_rates,
					  sizeof(u32));
	peer_ht_rates_align = roundup(param->peer_ht_rates.num_rates,
				      sizeof(u32));

	len = sizeof(*cmd) +
	      TLV_HDR_SIZE + (peer_legacy_rates_align * sizeof(u8)) +
	      TLV_HDR_SIZE + (peer_ht_rates_align * sizeof(u8)) +
	      sizeof(*mcs) + TLV_HDR_SIZE +
	      (sizeof(*he_mcs) * param->peer_he_mcs_count);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = ptr;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PEER_ASSOC_COMPLETE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = param->vdev_id;

	cmd->peer_new_assoc = param->peer_new_assoc;
	cmd->peer_associd = param->peer_associd;

	ath11k_wmi_copy_peer_flags(cmd, param,
				   test_bit(ATH11K_FLAG_HW_CRYPTO_DISABLED,
					    &ar->ab->dev_flags));

	ether_addr_copy(cmd->peer_macaddr.addr, param->peer_mac);

	cmd->peer_rate_caps = param->peer_rate_caps;
	cmd->peer_caps = param->peer_caps;
	cmd->peer_listen_intval = param->peer_listen_intval;
	cmd->peer_ht_caps = param->peer_ht_caps;
	cmd->peer_max_mpdu = param->peer_max_mpdu;
	cmd->peer_mpdu_density = param->peer_mpdu_density;
	cmd->peer_vht_caps = param->peer_vht_caps;
	cmd->peer_phymode = param->peer_phymode;

	/* Update 11ax capabilities */
	cmd->peer_he_cap_info = param->peer_he_cap_macinfo[0];
	cmd->peer_he_cap_info_ext = param->peer_he_cap_macinfo[1];
	cmd->peer_he_cap_info_internal = param->peer_he_cap_macinfo_internal;
	cmd->peer_he_caps_6ghz = param->peer_he_caps_6ghz;
	cmd->peer_he_ops = param->peer_he_ops;
	memcpy(&cmd->peer_he_cap_phy, &param->peer_he_cap_phyinfo,
	       sizeof(param->peer_he_cap_phyinfo));
	memcpy(&cmd->peer_ppet, &param->peer_ppet,
	       sizeof(param->peer_ppet));

	/* Update peer legacy rate information */
	ptr += sizeof(*cmd);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_BYTE) |
		      FIELD_PREP(WMI_TLV_LEN, peer_legacy_rates_align);

	ptr += TLV_HDR_SIZE;

	cmd->num_peer_legacy_rates = param->peer_legacy_rates.num_rates;
	memcpy(ptr, param->peer_legacy_rates.rates,
	       param->peer_legacy_rates.num_rates);

	/* Update peer HT rate information */
	ptr += peer_legacy_rates_align;

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_BYTE) |
		      FIELD_PREP(WMI_TLV_LEN, peer_ht_rates_align);
	ptr += TLV_HDR_SIZE;
	cmd->num_peer_ht_rates = param->peer_ht_rates.num_rates;
	memcpy(ptr, param->peer_ht_rates.rates,
	       param->peer_ht_rates.num_rates);

	/* VHT Rates */
	ptr += peer_ht_rates_align;

	mcs = ptr;

	mcs->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_VHT_RATE_SET) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*mcs) - TLV_HDR_SIZE);

	cmd->peer_nss = param->peer_nss;

	/* Update bandwidth-NSS mapping */
	cmd->peer_bw_rxnss_override = 0;
	cmd->peer_bw_rxnss_override |= param->peer_bw_rxnss_override;

	if (param->vht_capable) {
		mcs->rx_max_rate = param->rx_max_rate;
		mcs->rx_mcs_set = param->rx_mcs_set;
		mcs->tx_max_rate = param->tx_max_rate;
		mcs->tx_mcs_set = param->tx_mcs_set;
	}

	/* HE Rates */
	cmd->peer_he_mcs = param->peer_he_mcs_count;
	cmd->min_data_rate = param->min_data_rate;

	ptr += sizeof(*mcs);

	len = param->peer_he_mcs_count * sizeof(*he_mcs);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, len);
	ptr += TLV_HDR_SIZE;

	/* Loop through the HE rate set */
	for (i = 0; i < param->peer_he_mcs_count; i++) {
		he_mcs = ptr;
		he_mcs->tlv_header = FIELD_PREP(WMI_TLV_TAG,
						WMI_TAG_HE_RATE_SET) |
				     FIELD_PREP(WMI_TLV_LEN,
						sizeof(*he_mcs) - TLV_HDR_SIZE);

		he_mcs->rx_mcs_set = param->peer_he_tx_mcs_set[i];
		he_mcs->tx_mcs_set = param->peer_he_rx_mcs_set[i];
		ptr += sizeof(*he_mcs);
	}

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PEER_ASSOC_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_PEER_ASSOC_CMDID\n");
		dev_kfree_skb(skb);
	}

	if(debug || (ath11k_debug_mask & ATH11K_DBG_WMI)) {
		ath11k_warn(ar->ab,
			   "wmi peer assoc vdev id %d assoc id %d peer mac %pM peer_flags %x rate_caps %x\n peer_caps %x listen_intval %d ht_caps %x max_mpdu %d \nnss %d phymode %d peer_mpdu_density %d vht_caps %x he cap_info %x \nhe ops %x he cap_info_ext %x he phy %x %x %x \npeer_bw_rxnss_override %x\n",
			   cmd->vdev_id, cmd->peer_associd, param->peer_mac,
			   cmd->peer_flags, cmd->peer_rate_caps, cmd->peer_caps,
			   cmd->peer_listen_intval, cmd->peer_ht_caps,
			   cmd->peer_max_mpdu, cmd->peer_nss, cmd->peer_phymode,
			   cmd->peer_mpdu_density,
			   cmd->peer_vht_caps, cmd->peer_he_cap_info,
			   cmd->peer_he_ops, cmd->peer_he_cap_info_ext,
			   cmd->peer_he_cap_phy[0], cmd->peer_he_cap_phy[1],
			   cmd->peer_he_cap_phy[2],
			   cmd->peer_bw_rxnss_override);
	}
	return ret;
}

int ath11k_wmi_update_scan_chan_list(struct ath11k *ar,
				     struct scan_req_params *arg)
{
	struct ieee80211_supported_band **bands;
	struct scan_chan_list_params *params;
	struct channel_param *ch;
	struct cfg80211_chan_def *chandef;
	struct ieee80211_channel *channel, *req_channel;
	enum nl80211_band band;
	int num_channels = 0;
	int params_len, i, ret;
	bool found = false;

	bands = ar->hw->wiphy->bands;
	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		if (!bands[band])
			continue;
		for (i = 0; i < bands[band]->n_channels; i++) {
			if (bands[band]->channels[i].flags &
			    IEEE80211_CHAN_DISABLED)
				continue;

			num_channels++;
		}
	}

	if (WARN_ON(!num_channels))
		return -EINVAL;

	params_len = sizeof(struct scan_chan_list_params) +
			    num_channels * sizeof(struct channel_param);
	params = kzalloc(params_len, GFP_KERNEL);

	if (!params)
		return -ENOMEM;

	params->pdev_id = ar->pdev->pdev_id;
	params->nallchans = num_channels;

	ch = params->ch_param;
	chandef = arg ? arg->chandef : NULL;
	req_channel = chandef ? chandef->chan : NULL;

	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		if (!bands[band])
			continue;

		for (i = 0; i < bands[band]->n_channels; i++) {
			channel = &bands[band]->channels[i];

			if (channel->flags & IEEE80211_CHAN_DISABLED)
				continue;

			if (req_channel && !found &&
			    req_channel->center_freq == channel->center_freq) {
				ch->mhz = arg->channel_list.chan[0].freq;
				ch->cfreq1 = chandef->center_freq1;
				ch->cfreq2 = chandef->center_freq2;

				ch->phy_mode = arg->channel_list.chan[0].phymode;
				channel = req_channel;
				found = true;
			} else {
				ch->mhz = channel->center_freq;
				ch->cfreq1 = channel->center_freq;
				ch->phy_mode = (channel->band == NL80211_BAND_2GHZ) ?
						MODE_11G : MODE_11A;
			}

			/* TODO: Set to true/false based on some condition? */
			ch->allow_ht = true;
			ch->allow_vht = true;
			ch->allow_he = true;

			ch->dfs_set =
				!!(channel->flags & IEEE80211_CHAN_RADAR);
			ch->is_chan_passive = !!(channel->flags &
				IEEE80211_CHAN_NO_IR);
			ch->is_chan_passive |= ch->dfs_set;
			ch->minpower = 0;
			ch->maxpower = channel->max_power * 2;
			ch->maxregpower = channel->max_reg_power * 2;
			ch->antennamax = channel->max_antenna_gain * 2;

			if (channel->band == NL80211_BAND_6GHZ &&
			    cfg80211_channel_is_psc(channel))
				ch->psc_channel = true;

			ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
				   "mac channel [%d/%d] freq %d maxpower %d regpower %d antenna %d mode %d flag 0x%x chandef: %pk\n",
				   i, params->nallchans,
				   ch->mhz, ch->maxpower, ch->maxregpower,
				   ch->antennamax, ch->phy_mode, channel->flags,
				   chandef);
			ch++;
		}
	}

	ret = ath11k_wmi_send_scan_chan_list_cmd(ar, params);
	kfree(params);

	return ret;
}

void ath11k_wmi_start_scan_init(struct ath11k *ar,
				struct scan_req_params *arg)
{
	/* setup commonly used values */
	arg->scan_req_id = 1;
	if (ar->state_11d == ATH11K_11D_PREPARING)
		arg->scan_priority = WMI_SCAN_PRIORITY_MEDIUM;
	else
		arg->scan_priority = WMI_SCAN_PRIORITY_LOW;
	arg->dwell_time_active = 50;
	arg->dwell_time_active_2g = 0;
	arg->dwell_time_passive = 150;
	arg->dwell_time_active_6g = 40;
	arg->dwell_time_passive_6g = 30;
	arg->min_rest_time = 50;
	arg->max_rest_time = 500;
	arg->repeat_probe_time = 0;
	arg->probe_spacing_time = 0;
	arg->idle_time = 0;
	arg->max_scan_time = 20000;
	arg->probe_delay = 5;
	arg->notify_scan_events = WMI_SCAN_EVENT_STARTED |
				  WMI_SCAN_EVENT_COMPLETED |
				  WMI_SCAN_EVENT_BSS_CHANNEL |
				  WMI_SCAN_EVENT_FOREIGN_CHAN |
				  WMI_SCAN_EVENT_DEQUEUED;
	arg->scan_f_chan_stat_evnt = 1;

	if (test_bit(WMI_TLV_SERVICE_PASSIVE_SCAN_START_TIME_ENHANCE,
		     ar->ab->wmi_ab.svc_map))
		arg->scan_ctrl_flags_ext |=
			WMI_SCAN_FLAG_EXT_PASSIVE_SCAN_START_TIME_ENHANCE;

	arg->num_bssid = 1;

	/* fill bssid_list[0] with 0xff, otherwise bssid and RA will be
	 * ZEROs in probe request
	 */
	eth_broadcast_addr(arg->bssid_list[0].addr);
}

static inline void
ath11k_wmi_copy_scan_event_cntrl_flags(struct wmi_start_scan_cmd *cmd,
				       struct scan_req_params *param)
{
	/* Scan events subscription */
	if (param->scan_ev_started)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_STARTED;
	if (param->scan_ev_completed)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_COMPLETED;
	if (param->scan_ev_bss_chan)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_BSS_CHANNEL;
	if (param->scan_ev_foreign_chan)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_FOREIGN_CHAN;
	if (param->scan_ev_dequeued)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_DEQUEUED;
	if (param->scan_ev_preempted)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_PREEMPTED;
	if (param->scan_ev_start_failed)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_START_FAILED;
	if (param->scan_ev_restarted)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_RESTARTED;
	if (param->scan_ev_foreign_chn_exit)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_FOREIGN_CHAN_EXIT;
	if (param->scan_ev_suspended)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_SUSPENDED;
	if (param->scan_ev_resumed)
		cmd->notify_scan_events |=  WMI_SCAN_EVENT_RESUMED;

	/** Set scan control flags */
	cmd->scan_ctrl_flags = 0;
	if (param->scan_f_passive)
		cmd->scan_ctrl_flags |=  WMI_SCAN_FLAG_PASSIVE;
	if (param->scan_f_strict_passive_pch)
		cmd->scan_ctrl_flags |=  WMI_SCAN_FLAG_STRICT_PASSIVE_ON_PCHN;
	if (param->scan_f_promisc_mode)
		cmd->scan_ctrl_flags |=  WMI_SCAN_FILTER_PROMISCUOS;
	if (param->scan_f_capture_phy_err)
		cmd->scan_ctrl_flags |=  WMI_SCAN_CAPTURE_PHY_ERROR;
	if (param->scan_f_half_rate)
		cmd->scan_ctrl_flags |=  WMI_SCAN_FLAG_HALF_RATE_SUPPORT;
	if (param->scan_f_quarter_rate)
		cmd->scan_ctrl_flags |=  WMI_SCAN_FLAG_QUARTER_RATE_SUPPORT;
	if (param->scan_f_cck_rates)
		cmd->scan_ctrl_flags |=  WMI_SCAN_ADD_CCK_RATES;
	if (param->scan_f_ofdm_rates)
		cmd->scan_ctrl_flags |=  WMI_SCAN_ADD_OFDM_RATES;
	if (param->scan_f_chan_stat_evnt)
		cmd->scan_ctrl_flags |=  WMI_SCAN_CHAN_STAT_EVENT;
	if (param->scan_f_filter_prb_req)
		cmd->scan_ctrl_flags |=  WMI_SCAN_FILTER_PROBE_REQ;
	if (param->scan_f_bcast_probe)
		cmd->scan_ctrl_flags |=  WMI_SCAN_ADD_BCAST_PROBE_REQ;
	if (param->scan_f_offchan_mgmt_tx)
		cmd->scan_ctrl_flags |=  WMI_SCAN_OFFCHAN_MGMT_TX;
	if (param->scan_f_offchan_data_tx)
		cmd->scan_ctrl_flags |=  WMI_SCAN_OFFCHAN_DATA_TX;
	if (param->scan_f_force_active_dfs_chn)
		cmd->scan_ctrl_flags |=  WMI_SCAN_FLAG_FORCE_ACTIVE_ON_DFS;
	if (param->scan_f_add_tpc_ie_in_probe)
		cmd->scan_ctrl_flags |=  WMI_SCAN_ADD_TPC_IE_IN_PROBE_REQ;
	if (param->scan_f_add_ds_ie_in_probe)
		cmd->scan_ctrl_flags |=  WMI_SCAN_ADD_DS_IE_IN_PROBE_REQ;
	if (param->scan_f_add_spoofed_mac_in_probe)
		cmd->scan_ctrl_flags |=  WMI_SCAN_ADD_SPOOF_MAC_IN_PROBE_REQ;
	if (param->scan_f_add_rand_seq_in_probe)
		cmd->scan_ctrl_flags |=  WMI_SCAN_RANDOM_SEQ_NO_IN_PROBE_REQ;
	if (param->scan_f_en_ie_whitelist_in_probe)
		cmd->scan_ctrl_flags |=
			 WMI_SCAN_ENABLE_IE_WHTELIST_IN_PROBE_REQ;

	/* for adaptive scan mode using 3 bits (21 - 23 bits) */
	WMI_SCAN_SET_DWELL_MODE(cmd->scan_ctrl_flags,
				param->adaptive_dwell_time_mode);

	cmd->scan_ctrl_flags_ext = param->scan_ctrl_flags_ext;
}

int ath11k_wmi_send_scan_start_cmd(struct ath11k *ar,
				   struct scan_req_params *params)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_start_scan_cmd *cmd;
	struct wmi_ssid *ssid = NULL;
	struct wmi_mac_addr *bssid;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;
	int i, ret, len;
	u32 *tmp_ptr;
	u8 *phy_ptr;
	u16 extraie_len_with_pad = 0;
	struct hint_short_ssid *s_ssid = NULL;
	struct hint_bssid *hint_bssid = NULL;
	u8 phymode_roundup = 0;

	len = sizeof(*cmd);

	len += TLV_HDR_SIZE;
	if (params->channel_list.num_chan)
		len += params->channel_list.num_chan * sizeof(u32);

	len += TLV_HDR_SIZE;
	if (params->num_ssids)
		len += params->num_ssids * sizeof(*ssid);

	len += TLV_HDR_SIZE;
	if (params->num_bssid)
		len += sizeof(*bssid) * params->num_bssid;

	len += TLV_HDR_SIZE;
	if (params->extraie.len && params->extraie.len <= 0xFFFF)
		extraie_len_with_pad =
			roundup(params->extraie.len, sizeof(u32));
	len += extraie_len_with_pad;

	if (params->num_hint_bssid)
		len += TLV_HDR_SIZE +
		       params->num_hint_bssid * sizeof(struct hint_bssid);

	if (params->num_hint_s_ssid)
		len += TLV_HDR_SIZE +
		       params->num_hint_s_ssid * sizeof(struct hint_short_ssid);

	len += TLV_HDR_SIZE;
	if (params->scan_f_en_ie_whitelist_in_probe)
		len += params->ie_whitelist.num_vendor_oui *
				sizeof(struct wmi_vendor_oui);

	len += TLV_HDR_SIZE;
	if (params->scan_f_wide_band)
		phymode_roundup =
			roundup(params->channel_list.num_chan * sizeof(u8),
				sizeof(u32));

	len += phymode_roundup;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = ptr;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_START_SCAN_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->scan_id = params->scan_id;
	cmd->scan_req_id = params->scan_req_id;
	cmd->vdev_id = params->vdev_id;
	cmd->scan_priority = params->scan_priority;
	cmd->notify_scan_events = params->notify_scan_events;

	ath11k_wmi_copy_scan_event_cntrl_flags(cmd, params);

	cmd->dwell_time_active = params->dwell_time_active;
	cmd->dwell_time_active_2g = params->dwell_time_active_2g;
	cmd->dwell_time_passive = params->dwell_time_passive;
	cmd->dwell_time_active_6g = params->dwell_time_active_6g;
	cmd->dwell_time_passive_6g = params->dwell_time_passive_6g;
	cmd->min_rest_time = params->min_rest_time;
	cmd->max_rest_time = params->max_rest_time;
	cmd->repeat_probe_time = params->repeat_probe_time;
	cmd->probe_spacing_time = params->probe_spacing_time;
	cmd->idle_time = params->idle_time;
	cmd->max_scan_time = params->max_scan_time;
	cmd->probe_delay = params->probe_delay;
	cmd->burst_duration = params->burst_duration;
	cmd->num_chan = params->channel_list.num_chan;
	cmd->num_bssid = params->num_bssid;
	cmd->num_ssids = params->num_ssids;
	cmd->ie_len = params->extraie.len;
	cmd->n_probes = params->n_probes;
	ether_addr_copy(cmd->mac_addr.addr, params->mac_addr.addr);
	ether_addr_copy(cmd->mac_mask.addr, params->mac_mask.addr);

	ptr += sizeof(*cmd);

	len = params->channel_list.num_chan * sizeof(u32);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_UINT32) |
		      FIELD_PREP(WMI_TLV_LEN, len);
	ptr += TLV_HDR_SIZE;
	tmp_ptr = ptr;

	for (i = 0; i < params->channel_list.num_chan; ++i)
		tmp_ptr[i] = params->channel_list.chan[i].freq;

	ptr += len;

	len = params->num_ssids * sizeof(*ssid);
	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_FIXED_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, len);

	ptr += TLV_HDR_SIZE;

	if (params->num_ssids) {
		ssid = ptr;
		for (i = 0; i < params->num_ssids; ++i) {
			ssid->ssid_len = params->ssid[i].length;
			memcpy(ssid->ssid, params->ssid[i].ssid,
			       params->ssid[i].length);
			ssid++;
		}
	}

	ptr += (params->num_ssids * sizeof(*ssid));
	len = params->num_bssid * sizeof(*bssid);
	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_FIXED_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, len);

	ptr += TLV_HDR_SIZE;
	bssid = ptr;

	if (params->num_bssid) {
		for (i = 0; i < params->num_bssid; ++i) {
			ether_addr_copy(bssid->addr,
					params->bssid_list[i].addr);
			bssid++;
		}
	}

	ptr += params->num_bssid * sizeof(*bssid);

	len = extraie_len_with_pad;

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_BYTE) |
		      FIELD_PREP(WMI_TLV_LEN, len);
	ptr += TLV_HDR_SIZE;

	if (extraie_len_with_pad)
		memcpy(ptr, params->extraie.ptr,
		       params->extraie.len);

	ptr += extraie_len_with_pad;

	len = params->ie_whitelist.num_vendor_oui * sizeof(struct wmi_vendor_oui);
	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, len);
	ptr += TLV_HDR_SIZE;

	if (params->scan_f_en_ie_whitelist_in_probe) {
		/* TODO: fill vendor OUIs for probe req ie whitelisting */
		/* currently added for FW TLV validation */
	}

	ptr += cmd->num_vendor_oui * sizeof(struct wmi_vendor_oui);

	len = phymode_roundup;
	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_BYTE) |
		      FIELD_PREP(WMI_TLV_LEN, len);
	ptr += TLV_HDR_SIZE;

	/* Wide Band Scan */
	if (params->scan_f_wide_band) {
		phy_ptr = ptr;
		/* Add PHY mode TLV for wide band scan with phymode + 1 value
		 * so that phymode '0' is ignored by FW as default value.
		 */
		for (i = 0; i < params->channel_list.num_chan; ++i)
			phy_ptr[i] = params->channel_list.chan[i].phymode + 1;
	}
	ptr += phymode_roundup;

	if (params->num_hint_s_ssid) {
		len = params->num_hint_s_ssid * sizeof(struct hint_short_ssid);
		tlv = ptr;
		tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_FIXED_STRUCT) |
			      FIELD_PREP(WMI_TLV_LEN, len);
		ptr += TLV_HDR_SIZE;
		s_ssid = ptr;
		for (i = 0; i < params->num_hint_s_ssid; ++i) {
			s_ssid->freq_flags = params->hint_s_ssid[i].freq_flags;
			s_ssid->short_ssid = params->hint_s_ssid[i].short_ssid;
			s_ssid++;
		}
		ptr += len;
	}

	if (params->num_hint_bssid) {
		len = params->num_hint_bssid * sizeof(struct hint_bssid);
		tlv = ptr;
		tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_FIXED_STRUCT) |
			      FIELD_PREP(WMI_TLV_LEN, len);
		ptr += TLV_HDR_SIZE;
		hint_bssid = ptr;
		for (i = 0; i < params->num_hint_bssid; ++i) {
			hint_bssid->freq_flags =
				params->hint_bssid[i].freq_flags;
			ether_addr_copy(&params->hint_bssid[i].bssid.addr[0],
					&hint_bssid->bssid.addr[0]);
			hint_bssid++;
		}
	}

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_START_SCAN_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_START_SCAN_CMDID\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "cmd start scan");

	return ret;
}

int ath11k_wmi_send_vdev_set_tpc_power(struct ath11k *ar,
				       u32 vdev_id,
				       struct ath11k_reg_tpc_power_info *param)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_vdev_set_tpc_power_cmd *cmd;
	struct wmi_vdev_ch_power_info *ch;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	u8 *ptr;
	int i, ret, len;

	len = sizeof(*cmd) + TLV_HDR_SIZE;
	len += (sizeof(struct wmi_vdev_ch_power_info) * param->num_pwr_levels);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = (struct wmi_vdev_set_tpc_power_cmd *)ptr;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_VDEV_SET_TPC_POWER_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	cmd->psd_power = param->is_psd_power;
	cmd->eirp_power = param->eirp_power;
	cmd->power_type_6ghz = param->power_type_6g;
	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "wmi TPC vdev_id: %d is_psd_power: %d eirp_power: %d power_type_6g: %d\n",
		   vdev_id, param->is_psd_power, param->eirp_power, param->power_type_6g);

	ptr += sizeof(*cmd);
	tlv = (struct wmi_tlv *)ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, param->num_pwr_levels * sizeof(*ch));

	ptr += TLV_HDR_SIZE;
	ch = (struct wmi_vdev_ch_power_info *)ptr;

	for (i = 0; i < param->num_pwr_levels; i++, ch++) {
		ch->tlv_header = FIELD_PREP(WMI_TLV_TAG,
					    WMI_TAG_VDEV_CH_POWER_INFO) |
				FIELD_PREP(WMI_TLV_LEN,
					   sizeof(*ch) - TLV_HDR_SIZE);

		ch->chan_cfreq = param->chan_power_info[i].chan_cfreq;
		ch->tx_power = param->chan_power_info[i].tx_power;

		ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
			   "wmi TPC chan_cfreq: %d , tx_power: %d\n",
			   ch->chan_cfreq, ch->tx_power);
	}

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_VDEV_SET_TPC_POWER_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_VDEV_SET_TPC_POWER_CMDID\n");
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath11k_wmi_send_scan_stop_cmd(struct ath11k *ar,
				  struct scan_cancel_param *param)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_stop_scan_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_stop_scan_cmd *)skb->data;

	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_STOP_SCAN_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = param->vdev_id;
	cmd->requestor = param->requester;
	cmd->scan_id = param->scan_id;
	cmd->pdev_id = param->pdev_id;
	/* stop the scan with the corresponding scan_id */
	if (param->req_type == WLAN_SCAN_CANCEL_PDEV_ALL) {
		/* Cancelling all scans */
		cmd->req_type =  WMI_SCAN_STOP_ALL;
	} else if (param->req_type == WLAN_SCAN_CANCEL_VDEV_ALL) {
		/* Cancelling VAP scans */
		cmd->req_type =  WMI_SCN_STOP_VAP_ALL;
	} else if (param->req_type == WLAN_SCAN_CANCEL_SINGLE) {
		/* Cancelling specific scan */
		cmd->req_type =  WMI_SCAN_STOP_ONE;
	} else {
		ath11k_warn(ar->ab, "invalid scan cancel param %d",
			    param->req_type);
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_STOP_SCAN_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_STOP_SCAN_CMDID\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "cmd stop scan");

	return ret;
}

int ath11k_wmi_send_scan_chan_list_cmd(struct ath11k *ar,
				       struct scan_chan_list_params *chan_list)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_scan_chan_list_cmd *cmd;
	struct sk_buff *skb;
	struct wmi_channel *chan_info;
	struct channel_param *tchan_info;
	struct wmi_tlv *tlv;
	void *ptr;
	int i, ret, len;
	u16 num_send_chans, num_sends = 0, max_chan_limit = 0;
	u32 *reg1, *reg2;

	tchan_info = chan_list->ch_param;
	while (chan_list->nallchans) {
		len = sizeof(*cmd) + TLV_HDR_SIZE;
		max_chan_limit = (wmi->wmi_ab->max_msg_len[ar->pdev_idx] - len) /
			sizeof(*chan_info);

		if (chan_list->nallchans > max_chan_limit)
			num_send_chans = max_chan_limit;
		else
			num_send_chans = chan_list->nallchans;

		chan_list->nallchans -= num_send_chans;
		len += sizeof(*chan_info) * num_send_chans;

		skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
		if (!skb)
			return -ENOMEM;

		cmd = (struct wmi_scan_chan_list_cmd *)skb->data;
		cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_SCAN_CHAN_LIST_CMD) |
			FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
		cmd->pdev_id = chan_list->pdev_id;
		cmd->num_scan_chans = num_send_chans;
		if (num_sends || chan_list->append_chan_list)
			cmd->flags |= WMI_APPEND_TO_EXISTING_CHAN_LIST_FLAG;

		ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
			   "no.of chan = %d len = %d pdev_id = %d num_sends = %d\n",
			   num_send_chans, len, cmd->pdev_id, num_sends);

		ptr = skb->data + sizeof(*cmd);

		len = sizeof(*chan_info) * num_send_chans;
		tlv = ptr;
		tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
			      FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
		ptr += TLV_HDR_SIZE;

		for (i = 0; i < num_send_chans; ++i) {
			chan_info = ptr;
			memset(chan_info, 0, sizeof(*chan_info));
			len = sizeof(*chan_info);
			chan_info->tlv_header = FIELD_PREP(WMI_TLV_TAG,
							   WMI_TAG_CHANNEL) |
						FIELD_PREP(WMI_TLV_LEN,
							   len - TLV_HDR_SIZE);

			reg1 = &chan_info->reg_info_1;
			reg2 = &chan_info->reg_info_2;
			chan_info->mhz = tchan_info->mhz;
			chan_info->band_center_freq1 = tchan_info->cfreq1;
			chan_info->band_center_freq2 = tchan_info->cfreq2;

			if (tchan_info->is_chan_passive)
				chan_info->info |= WMI_CHAN_INFO_PASSIVE;
			if (tchan_info->allow_he)
				chan_info->info |= WMI_CHAN_INFO_ALLOW_HE;
			else if (tchan_info->allow_vht)
				chan_info->info |= WMI_CHAN_INFO_ALLOW_VHT;
			else if (tchan_info->allow_ht)
				chan_info->info |= WMI_CHAN_INFO_ALLOW_HT;
			if (tchan_info->half_rate)
				chan_info->info |= WMI_CHAN_INFO_HALF_RATE;
			if (tchan_info->quarter_rate)
				chan_info->info |= WMI_CHAN_INFO_QUARTER_RATE;
			if (tchan_info->psc_channel)
				chan_info->info |= WMI_CHAN_INFO_PSC;
			if (tchan_info->dfs_set)
				chan_info->info |= WMI_CHAN_INFO_DFS;

			chan_info->info |= FIELD_PREP(WMI_CHAN_INFO_MODE,
						      tchan_info->phy_mode);
			*reg1 |= FIELD_PREP(WMI_CHAN_REG_INFO1_MIN_PWR,
					    tchan_info->minpower);
			*reg1 |= FIELD_PREP(WMI_CHAN_REG_INFO1_MAX_PWR,
					    tchan_info->maxpower);
			*reg1 |= FIELD_PREP(WMI_CHAN_REG_INFO1_MAX_REG_PWR,
					    tchan_info->maxregpower);
			*reg1 |= FIELD_PREP(WMI_CHAN_REG_INFO1_REG_CLS,
					    tchan_info->reg_class_id);
			*reg2 |= FIELD_PREP(WMI_CHAN_REG_INFO2_ANT_MAX,
					    tchan_info->antennamax);
			*reg2 |= FIELD_PREP(WMI_CHAN_REG_INFO2_MAX_TX_PWR,
					    tchan_info->maxregpower);

			ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
				   "chan scan list chan[%d] = %u, chan_info->info %8x\n",
				   i, chan_info->mhz, chan_info->info);

			ptr += sizeof(*chan_info);

			tchan_info++;
		}

		ret = ath11k_wmi_cmd_send(wmi, skb, WMI_SCAN_CHAN_LIST_CMDID);
		if (ret) {
			ath11k_warn(ar->ab, "failed to send WMI_SCAN_CHAN_LIST cmd\n");
			dev_kfree_skb(skb);
			return ret;
		}

		ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "cmd scan chan list channels %d",
			   num_send_chans);

		num_sends++;
	}

	return 0;
}

int ath11k_wmi_send_wmm_update_cmd_tlv(struct ath11k *ar, u32 vdev_id,
				       struct wmi_wmm_params_all_arg *param,
				       enum wmi_wmm_params_type wmm_param_type)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_vdev_set_wmm_params_cmd *cmd;
	struct wmi_wmm_params *wmm_param;
	struct wmi_wmm_params_arg *wmi_wmm_arg;
	struct sk_buff *skb;
	int ret, ac;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_set_wmm_params_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_VDEV_SET_WMM_PARAMS_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = vdev_id;
	cmd->wmm_param_type = WMI_WMM_PARAM_TYPE_LEGACY;

	for (ac = 0; ac < WME_NUM_AC; ac++) {
		switch (ac) {
		case WME_AC_BE:
			wmi_wmm_arg = &param->ac_be;
			break;
		case WME_AC_BK:
			wmi_wmm_arg = &param->ac_bk;
			break;
		case WME_AC_VI:
			wmi_wmm_arg = &param->ac_vi;
			break;
		case WME_AC_VO:
			wmi_wmm_arg = &param->ac_vo;
			break;
		}

		wmm_param = (struct wmi_wmm_params *)&cmd->wmm_params[ac];
		wmm_param->tlv_header =
				FIELD_PREP(WMI_TLV_TAG,
					   WMI_TAG_VDEV_SET_WMM_PARAMS_CMD) |
				FIELD_PREP(WMI_TLV_LEN,
					   sizeof(*wmm_param) - TLV_HDR_SIZE);

		wmm_param->aifs = wmi_wmm_arg->aifs;
		wmm_param->cwmin = wmi_wmm_arg->cwmin;
		wmm_param->cwmax = wmi_wmm_arg->cwmax;
		wmm_param->txoplimit = wmi_wmm_arg->txop;
		wmm_param->acm = wmi_wmm_arg->acm;
		wmm_param->no_ack = wmi_wmm_arg->no_ack;

		ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
			   "wmm set type %d ac %d aifs %d cwmin %d cwmax %d txop %d acm %d no_ack %d\n",
			   wmm_param_type, ac, wmm_param->aifs, wmm_param->cwmin,
			   wmm_param->cwmax, wmm_param->txoplimit,
			   wmm_param->acm, wmm_param->no_ack);
	}
	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_VDEV_SET_WMM_PARAMS_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_VDEV_SET_WMM_PARAMS_CMDID");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "cmd vdev set wmm params");

	return ret;
}

int ath11k_wmi_send_dfs_phyerr_offload_enable_cmd(struct ath11k *ar,
						  u32 pdev_id)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_dfs_phyerr_offload_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_dfs_phyerr_offload_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_PDEV_DFS_PHYERR_OFFLOAD_ENABLE_CMD) |
		FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->pdev_id = pdev_id;

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_DFS_PHYERR_OFFLOAD_ENABLE_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_PDEV_DFS_PHYERR_OFFLOAD_ENABLE cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd pdev dfs phyerr offload enable pdev id %d\n", pdev_id);

	return ret;
}

int ath11k_wmi_delba_send(struct ath11k *ar, u32 vdev_id, const u8 *mac,
			  u32 tid, u32 initiator, u32 reason)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_delba_send_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_delba_send_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_DELBA_SEND_CMD) |
			FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, mac);
	cmd->tid = tid;
	cmd->initiator = initiator;
	cmd->reasoncode = reason;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_DELBA_SEND_CMDID);

	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_DELBA_SEND_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd delba send vdev_id 0x%X mac_addr %pM tid %u initiator %u reason %u\n",
		   vdev_id, mac, tid, initiator, reason);

	return ret;
}

int ath11k_wmi_addba_set_resp(struct ath11k *ar, u32 vdev_id, const u8 *mac,
			      u32 tid, u32 status)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_addba_setresponse_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_addba_setresponse_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ADDBA_SETRESPONSE_CMD) |
		FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, mac);
	cmd->tid = tid;
	cmd->statuscode = status;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_ADDBA_SET_RESP_CMDID);

	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_ADDBA_SET_RESP_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd addba set resp vdev_id 0x%X mac_addr %pM tid %u status %u\n",
		   vdev_id, mac, tid, status);

	return ret;
}

int ath11k_wmi_addba_send(struct ath11k *ar, u32 vdev_id, const u8 *mac,
			  u32 tid, u32 buf_size)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_addba_send_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_addba_send_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ADDBA_SEND_CMD) |
		FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, mac);
	cmd->tid = tid;
	cmd->buffersize = buf_size;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_ADDBA_SEND_CMDID);

	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_ADDBA_SEND_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd addba send vdev_id 0x%X mac_addr %pM tid %u bufsize %u\n",
		   vdev_id, mac, tid, buf_size);

	return ret;
}

int ath11k_wmi_addba_clear_resp(struct ath11k *ar, u32 vdev_id, const u8 *mac)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_addba_clear_resp_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_addba_clear_resp_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ADDBA_CLEAR_RESP_CMD) |
		FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, mac);

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_ADDBA_CLEAR_RESP_CMDID);

	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_ADDBA_CLEAR_RESP_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd addba clear resp vdev_id 0x%X mac_addr %pM\n",
		   vdev_id, mac);

	return ret;
}

int ath11k_wmi_pdev_peer_pktlog_filter(struct ath11k *ar, u8 *addr, u8 enable)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_pdev_pktlog_filter_cmd *cmd;
	struct wmi_pdev_pktlog_filter_info *info;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;
	int ret, len;

	len = sizeof(*cmd) + sizeof(*info) + TLV_HDR_SIZE;
	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_pktlog_filter_cmd *)skb->data;

	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PDEV_PEER_PKTLOG_FILTER_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->pdev_id = DP_HW2SW_MACID(ar->pdev->pdev_id);
	cmd->num_mac = 1;
	cmd->enable = enable;

	ptr = skb->data + sizeof(*cmd);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, sizeof(*info));

	ptr += TLV_HDR_SIZE;
	info = ptr;

	ether_addr_copy(info->peer_macaddr.addr, addr);
	info->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PDEV_PEER_PKTLOG_FILTER_INFO) |
			   FIELD_PREP(WMI_TLV_LEN,
				      sizeof(*info) - TLV_HDR_SIZE);

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_PKTLOG_FILTER_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_PDEV_PKTLOG_ENABLE_CMDID\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "cmd pdev pktlog filter");

	return ret;
}

int
ath11k_wmi_send_init_country_cmd(struct ath11k *ar,
				 struct wmi_init_country_params init_cc_params)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_init_country_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_init_country_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_SET_INIT_COUNTRY_CMD) |
		FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->pdev_id = ar->pdev->pdev_id;

	switch (init_cc_params.flags) {
	case ALPHA_IS_SET:
		cmd->init_cc_type = WMI_COUNTRY_INFO_TYPE_ALPHA;
		memcpy((u8 *)&cmd->cc_info.alpha2,
		       init_cc_params.cc_info.alpha2, 3);
		break;
	case CC_IS_SET:
		cmd->init_cc_type = WMI_COUNTRY_INFO_TYPE_COUNTRY_CODE;
		cmd->cc_info.country_code = init_cc_params.cc_info.country_code;
		break;
	case REGDMN_IS_SET:
		cmd->init_cc_type = WMI_COUNTRY_INFO_TYPE_REGDOMAIN;
		cmd->cc_info.regdom_id = init_cc_params.cc_info.regdom_id;
		break;
	default:
		ath11k_warn(ar->ab, "unknown cc params flags: 0x%x",
			    init_cc_params.flags);
		ret = -EINVAL;
		goto err;
	}

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_SET_INIT_COUNTRY_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_SET_INIT_COUNTRY CMD :%d\n",
			    ret);
		goto err;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "cmd set init country");

	return 0;

err:
	dev_kfree_skb(skb);
	return ret;
}

int ath11k_wmi_send_set_current_country_cmd(struct ath11k *ar,
					    struct wmi_set_current_country_params *param)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_set_current_country_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_set_current_country_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG, WMI_TAG_SET_CURRENT_COUNTRY_CMD) |
		FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->pdev_id = ar->pdev->pdev_id;
	memcpy(&cmd->new_alpha2, &param->alpha2, 3);

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_SET_CURRENT_COUNTRY_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_SET_CURRENT_COUNTRY_CMDID: %d\n", ret);
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd set current country pdev id %d alpha2 %c%c\n",
		   ar->pdev->pdev_id,
		   param->alpha2[0],
		   param->alpha2[1]);

	return ret;
}

int
ath11k_wmi_send_thermal_mitigation_param_cmd(struct ath11k *ar,
					     struct thermal_mitigation_params *param)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_therm_throt_config_request_cmd *cmd;
	struct wmi_therm_throt_level_config_info *lvl_conf;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	int i, ret, len;

	len = sizeof(*cmd) + TLV_HDR_SIZE +
	      THERMAL_LEVELS * sizeof(struct wmi_therm_throt_level_config_info);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_therm_throt_config_request_cmd *)skb->data;

	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_THERM_THROT_CONFIG_REQUEST) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->pdev_id = ar->pdev->pdev_id;
	cmd->enable = param->enable;
	cmd->dc = param->dc;
	cmd->dc_per_event = param->dc_per_event;
	cmd->therm_throt_levels = THERMAL_LEVELS;

	tlv = (struct wmi_tlv *)(skb->data + sizeof(*cmd));
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN,
				 (THERMAL_LEVELS *
				  sizeof(struct wmi_therm_throt_level_config_info)));

	lvl_conf = (struct wmi_therm_throt_level_config_info *)(skb->data +
								sizeof(*cmd) +
								TLV_HDR_SIZE);
	for (i = 0; i < THERMAL_LEVELS; i++) {
		lvl_conf->tlv_header =
			FIELD_PREP(WMI_TLV_TAG, WMI_TAG_THERM_THROT_LEVEL_CONFIG_INFO) |
			FIELD_PREP(WMI_TLV_LEN, sizeof(*lvl_conf) - TLV_HDR_SIZE);

		lvl_conf->temp_lwm = param->levelconf[i].tmplwm;
		lvl_conf->temp_hwm = param->levelconf[i].tmphwm;
		lvl_conf->dc_off_percent = param->levelconf[i].dcoffpercent;
		lvl_conf->prio = param->levelconf[i].priority;
		lvl_conf++;
	}

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_THERM_THROT_SET_CONF_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send THERM_THROT_SET_CONF cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd therm throt set conf pdev_id %d enable %d dc %d dc_per_event %x levels %d\n",
		   ar->pdev->pdev_id, param->enable, param->dc,
		   param->dc_per_event, THERMAL_LEVELS);

	return ret;
}

int ath11k_wmi_send_11d_scan_start_cmd(struct ath11k *ar,
				       struct wmi_11d_scan_start_params *param)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_11d_scan_start_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_11d_scan_start_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG, WMI_TAG_11D_SCAN_START_CMD) |
		FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = param->vdev_id;
	cmd->scan_period_msec = param->scan_period_msec;
	cmd->start_interval_msec = param->start_interval_msec;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_11D_SCAN_START_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_11D_SCAN_START_CMDID: %d\n", ret);
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd 11d scan start vdev id %d period %d ms internal %d ms\n",
		   cmd->vdev_id,
		   cmd->scan_period_msec,
		   cmd->start_interval_msec);

	return ret;
}

int ath11k_wmi_send_11d_scan_stop_cmd(struct ath11k *ar, u32 vdev_id)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_11d_scan_stop_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_11d_scan_stop_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG, WMI_TAG_11D_SCAN_STOP_CMD) |
		FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = vdev_id;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_11D_SCAN_STOP_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_11D_SCAN_STOP_CMDID: %d\n", ret);
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd 11d scan stop vdev id %d\n",
		   cmd->vdev_id);

	return ret;
}

int ath11k_wmi_pdev_pktlog_enable(struct ath11k *ar, u32 pktlog_filter)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_pktlog_enable_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pktlog_enable_cmd *)skb->data;

	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PDEV_PKTLOG_ENABLE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->pdev_id = DP_HW2SW_MACID(ar->pdev->pdev_id);
	cmd->evlist = pktlog_filter;
	cmd->enable = ATH11K_WMI_PKTLOG_ENABLE_FORCE;

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_PKTLOG_ENABLE_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_PDEV_PKTLOG_ENABLE_CMDID\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "cmd pdev pktlog enable");

	return ret;
}

int ath11k_wmi_pdev_pktlog_disable(struct ath11k *ar)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_pktlog_disable_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pktlog_disable_cmd *)skb->data;

	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PDEV_PKTLOG_DISABLE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->pdev_id = DP_HW2SW_MACID(ar->pdev->pdev_id);

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_PKTLOG_DISABLE_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_PDEV_PKTLOG_ENABLE_CMDID\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "cmd pdev pktlog disable");

	return ret;
}

void ath11k_wmi_fill_default_twt_params(struct wmi_twt_enable_params *twt_params)
{
	twt_params->sta_cong_timer_ms = ATH11K_TWT_DEF_STA_CONG_TIMER_MS;
	twt_params->default_slot_size = ATH11K_TWT_DEF_DEFAULT_SLOT_SIZE;
	twt_params->congestion_thresh_setup = ATH11K_TWT_DEF_CONGESTION_THRESH_SETUP;
	twt_params->congestion_thresh_teardown =
		ATH11K_TWT_DEF_CONGESTION_THRESH_TEARDOWN;
	twt_params->congestion_thresh_critical =
		ATH11K_TWT_DEF_CONGESTION_THRESH_CRITICAL;
	twt_params->interference_thresh_teardown =
		ATH11K_TWT_DEF_INTERFERENCE_THRESH_TEARDOWN;
	twt_params->interference_thresh_setup =
		ATH11K_TWT_DEF_INTERFERENCE_THRESH_SETUP;
	twt_params->min_no_sta_setup = ATH11K_TWT_DEF_MIN_NO_STA_SETUP;
	twt_params->min_no_sta_teardown = ATH11K_TWT_DEF_MIN_NO_STA_TEARDOWN;
	twt_params->no_of_bcast_mcast_slots = ATH11K_TWT_DEF_NO_OF_BCAST_MCAST_SLOTS;
	twt_params->min_no_twt_slots = ATH11K_TWT_DEF_MIN_NO_TWT_SLOTS;
	twt_params->max_no_sta_twt = ATH11K_TWT_DEF_MAX_NO_STA_TWT;
	twt_params->mode_check_interval = ATH11K_TWT_DEF_MODE_CHECK_INTERVAL;
	twt_params->add_sta_slot_interval = ATH11K_TWT_DEF_ADD_STA_SLOT_INTERVAL;
	twt_params->remove_sta_slot_interval =
		ATH11K_TWT_DEF_REMOVE_STA_SLOT_INTERVAL;
	/* TODO add MBSSID support */
	twt_params->mbss_support = 0;
}

int ath11k_wmi_send_twt_enable_cmd(struct ath11k *ar, u32 pdev_id,
				   struct wmi_twt_enable_params *params)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct ath11k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_enable_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_enable_params_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_TWT_ENABLE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = pdev_id;
	cmd->sta_cong_timer_ms = params->sta_cong_timer_ms;
	cmd->default_slot_size = params->default_slot_size;
	cmd->congestion_thresh_setup = params->congestion_thresh_setup;
	cmd->congestion_thresh_teardown = params->congestion_thresh_teardown;
	cmd->congestion_thresh_critical = params->congestion_thresh_critical;
	cmd->interference_thresh_teardown = params->interference_thresh_teardown;
	cmd->interference_thresh_setup = params->interference_thresh_setup;
	cmd->min_no_sta_setup = params->min_no_sta_setup;
	cmd->min_no_sta_teardown = params->min_no_sta_teardown;
	cmd->no_of_bcast_mcast_slots = params->no_of_bcast_mcast_slots;
	cmd->min_no_twt_slots = params->min_no_twt_slots;
	cmd->max_no_sta_twt = params->max_no_sta_twt;
	cmd->mode_check_interval = params->mode_check_interval;
	cmd->add_sta_slot_interval = params->add_sta_slot_interval;
	cmd->remove_sta_slot_interval = params->remove_sta_slot_interval;
	cmd->mbss_support = params->mbss_support;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_TWT_ENABLE_CMDID);
	if (ret) {
		ath11k_warn(ab, "Failed to send WMI_TWT_ENABLE_CMDID");
		dev_kfree_skb(skb);
		return ret;
	}

	ar->twt_enabled = 1;

	ath11k_dbg(ab, ATH11K_DBG_WMI, "cmd twt enable");

	return 0;
}

int
ath11k_wmi_send_twt_disable_cmd(struct ath11k *ar, u32 pdev_id)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct ath11k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_disable_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_disable_params_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_TWT_DISABLE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = pdev_id;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_TWT_DISABLE_CMDID);
	if (ret) {
		ath11k_warn(ab, "Failed to send WMI_TWT_DISABLE_CMDID");
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "cmd twt disable");

	ar->twt_enabled = 0;

	return 0;
}

int ath11k_wmi_send_twt_add_dialog_cmd(struct ath11k *ar,
				       struct wmi_twt_add_dialog_params *params)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct ath11k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_add_dialog_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_add_dialog_params_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_TWT_ADD_DIALOG_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);

	cmd->vdev_id = params->vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, params->peer_macaddr);
	cmd->dialog_id = params->dialog_id;
	cmd->wake_intvl_us = params->wake_intvl_us;
	cmd->wake_intvl_mantis = params->wake_intvl_mantis;
	cmd->wake_dura_us = params->wake_dura_us;
	cmd->sp_offset_us = params->sp_offset_us;
	cmd->flags = params->twt_cmd;
	if (params->flag_bcast)
		cmd->flags |= WMI_TWT_ADD_DIALOG_FLAG_BCAST;
	if (params->flag_trigger)
		cmd->flags |= WMI_TWT_ADD_DIALOG_FLAG_TRIGGER;
	if (params->flag_flow_type)
		cmd->flags |= WMI_TWT_ADD_DIALOG_FLAG_FLOW_TYPE;
	if (params->flag_protection)
		cmd->flags |= WMI_TWT_ADD_DIALOG_FLAG_PROTECTION;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_TWT_ADD_DIALOG_CMDID);
	if (ret) {
		ath11k_warn(ab,
			    "failed to send wmi command to add twt dialog: %d",
			    ret);
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd twt add dialog vdev %u dialog id %u wake interval %u mantissa %u wake duration %u service period offset %u flags 0x%x\n",
		   cmd->vdev_id, cmd->dialog_id, cmd->wake_intvl_us,
		   cmd->wake_intvl_mantis, cmd->wake_dura_us, cmd->sp_offset_us,
		   cmd->flags);

	return 0;
}

int ath11k_wmi_send_twt_del_dialog_cmd(struct ath11k *ar,
				       struct wmi_twt_del_dialog_params *params)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct ath11k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_del_dialog_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_del_dialog_params_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_TWT_DEL_DIALOG_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);

	cmd->vdev_id = params->vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, params->peer_macaddr);
	cmd->dialog_id = params->dialog_id;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_TWT_DEL_DIALOG_CMDID);
	if (ret) {
		ath11k_warn(ab,
			    "failed to send wmi command to delete twt dialog: %d",
			    ret);
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd twt del dialog vdev %u dialog id %u\n",
		   cmd->vdev_id, cmd->dialog_id);

	return 0;
}

int ath11k_wmi_send_twt_pause_dialog_cmd(struct ath11k *ar,
					 struct wmi_twt_pause_dialog_params *params)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct ath11k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_pause_dialog_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_pause_dialog_params_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_TWT_PAUSE_DIALOG_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);

	cmd->vdev_id = params->vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, params->peer_macaddr);
	cmd->dialog_id = params->dialog_id;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_TWT_PAUSE_DIALOG_CMDID);
	if (ret) {
		ath11k_warn(ab,
			    "failed to send wmi command to pause twt dialog: %d",
			    ret);
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd twt pause dialog vdev %u dialog id %u\n",
		   cmd->vdev_id, cmd->dialog_id);

	return 0;
}

int ath11k_wmi_send_twt_resume_dialog_cmd(struct ath11k *ar,
					  struct wmi_twt_resume_dialog_params *params)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct ath11k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_resume_dialog_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_resume_dialog_params_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_TWT_RESUME_DIALOG_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);

	cmd->vdev_id = params->vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, params->peer_macaddr);
	cmd->dialog_id = params->dialog_id;
	cmd->sp_offset_us = params->sp_offset_us;
	cmd->next_twt_size = params->next_twt_size;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_TWT_RESUME_DIALOG_CMDID);
	if (ret) {
		ath11k_warn(ab,
			    "failed to send wmi command to resume twt dialog: %d",
			    ret);
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd twt resume dialog vdev %u dialog id %u service period offset %u next twt subfield size %u\n",
		   cmd->vdev_id, cmd->dialog_id, cmd->sp_offset_us,
		   cmd->next_twt_size);

	return 0;
}

int
ath11k_wmi_send_wmi_ctrl_stats_cmd(struct ath11k *ar,
				  struct wmi_ctrl_path_stats_cmd_param *param)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct ath11k_base *ab = wmi->wmi_ab->ab;
	u32 num_pdev_idx = 0;
	u32 num_vdev_idx = 0;
	u32 num_mac_addr_list = 0;
	u32 pdev_id_array[2] = {0};
	int len, ret;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	struct wmi_ctrl_path_stats_cmd_param *cmd;
	void *ptr;
	u32 stats_id = 0;
	unsigned long time_left;

	switch (param->stats_id) {
	case WMI_REQ_CTRL_PATH_PDEV_TX_STAT:
		pdev_id_array[num_pdev_idx] = ar->pdev->pdev_id;
		stats_id = (1 << param->stats_id);
		num_pdev_idx++;
		break;
	case WMI_REQ_CTRL_PATH_CAL_STAT:
	case WMI_REQ_CTRL_PATH_BTCOEX_STAT:
		pdev_id_array[num_pdev_idx] = ar->pdev->pdev_id;
		stats_id = (1 << param->stats_id);
		num_pdev_idx++;
		break;
	/* Add case for newly wmi ctrl path stats here */
	default:
		ath11k_warn(ab, "Unsupported stats id %d", param->stats_id);
		return -EIO;
		break;
	}

	len = sizeof(*cmd) +
	      TLV_HDR_SIZE + (sizeof(u32) * num_pdev_idx) +
	      TLV_HDR_SIZE + (sizeof(u32) * num_vdev_idx) +
	      TLV_HDR_SIZE + (sizeof(struct wmi_mac_addr) * num_mac_addr_list);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (void *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_CTRL_PATH_STATS_CMD_FIXED_PARAM) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->stats_id = stats_id;
	cmd->req_id = param->req_id;
	cmd->action = param->action;

	ptr = skb->data + sizeof(*cmd);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_UINT32) |
		      FIELD_PREP(WMI_TLV_LEN, (sizeof(u32) * num_pdev_idx));
	ptr += TLV_HDR_SIZE;
	memcpy(ptr, &pdev_id_array[0], sizeof(pdev_id_array));
	ptr += sizeof(u32) * num_pdev_idx;

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_UINT32) |
		      FIELD_PREP(WMI_TLV_LEN, (sizeof(u32) * num_vdev_idx));
	ptr += TLV_HDR_SIZE;

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_FIXED_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, (sizeof(struct wmi_mac_addr) *
				 num_mac_addr_list));
	ptr += TLV_HDR_SIZE;

	if (param->action == WMI_REQ_CTRL_PATH_STAT_GET)
		reinit_completion(&ar->debug.wmi_ctrl_path_stats_rcvd);

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_REQUEST_CTRL_PATH_STATS_CMDID);
	if (ret) {
		dev_kfree_skb(skb);
		ath11k_warn(ab, "Failed to send WMI_REQUEST_CTRL_PATH_STATS_CMDID: %d",ret);
	} else {
		if (param->action == WMI_REQ_CTRL_PATH_STAT_GET) {
	                time_left = wait_for_completion_timeout(
						&ar->debug.wmi_ctrl_path_stats_rcvd,
						WMI_CTRL_STATS_READY_TIMEOUT_HZ * HZ);
			if (time_left == 0) {
				ath11k_warn(ab, "timeout in receiving wmi ctrl path stats\n");
				return -ETIMEDOUT;
			}
		}
	}

	return ret;
}

int
ath11k_wmi_send_obss_spr_cmd(struct ath11k *ar, u32 vdev_id,
			     struct ieee80211_he_obss_pd *he_obss_pd)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct ath11k_base *ab = wmi->wmi_ab->ab;
	struct wmi_obss_spatial_reuse_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_obss_spatial_reuse_params_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_OBSS_SPATIAL_REUSE_SET_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	cmd->enable = he_obss_pd->enable;
	cmd->obss_min = he_obss_pd->min_offset;
	cmd->obss_max = he_obss_pd->max_offset;

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_OBSS_PD_SPATIAL_REUSE_CMDID);
	if (ret) {
		ath11k_warn(ab,
			    "Failed to send WMI_PDEV_OBSS_PD_SPATIAL_REUSE_CMDID");
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "cmd pdev obss pd spatial reuse");

	return 0;
}

int
ath11k_wmi_pdev_set_srg_bss_color_bitmap(struct ath11k *ar, u32 *bitmap)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct ath11k_base *ab = wmi->wmi_ab->ab;
	struct wmi_pdev_obss_pd_bitmap_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_obss_pd_bitmap_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PDEV_SRG_BSS_COLOR_BITMAP_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	memcpy(cmd->bitmap, bitmap, sizeof(cmd->bitmap));

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SET_SRG_BSS_COLOR_BITMAP_CMDID);
	if (ret) {
		ath11k_warn(ab,
			    "failed to send WMI_PDEV_SET_SRG_BSS_COLOR_BITMAP_CMDID");
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd pdev set srg bss color bitmap pdev_id %d bss color bitmap %08x %08x\n",
		   cmd->pdev_id, cmd->bitmap[0], cmd->bitmap[1]);

	return 0;
}

int
ath11k_wmi_pdev_set_srg_patial_bssid_bitmap(struct ath11k *ar, u32 *bitmap)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct ath11k_base *ab = wmi->wmi_ab->ab;
	struct wmi_pdev_obss_pd_bitmap_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_obss_pd_bitmap_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_PDEV_SRG_PARTIAL_BSSID_BITMAP_CMD) |
		FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	memcpy(cmd->bitmap, bitmap, sizeof(cmd->bitmap));

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SET_SRG_PARTIAL_BSSID_BITMAP_CMDID);
	if (ret) {
		ath11k_warn(ab,
			    "failed to send WMI_PDEV_SET_SRG_PARTIAL_BSSID_BITMAP_CMDID");
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd pdev set srg partial bssid bitmap pdev_id %d partial bssid bitmap %08x %08x\n",
		   cmd->pdev_id, cmd->bitmap[0], cmd->bitmap[1]);

	return 0;
}

int
ath11k_wmi_pdev_srg_obss_color_enable_bitmap(struct ath11k *ar, u32 *bitmap)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct ath11k_base *ab = wmi->wmi_ab->ab;
	struct wmi_pdev_obss_pd_bitmap_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_obss_pd_bitmap_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_PDEV_SRG_OBSS_COLOR_ENABLE_BITMAP_CMD) |
		FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	memcpy(cmd->bitmap, bitmap, sizeof(cmd->bitmap));

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SET_SRG_OBSS_COLOR_ENABLE_BITMAP_CMDID);
	if (ret) {
		ath11k_warn(ab,
			    "failed to send WMI_PDEV_SET_SRG_OBSS_COLOR_ENABLE_BITMAP_CMDID");
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd pdev set srg obsscolor enable pdev_id %d bss color enable bitmap %08x %08x\n",
		   cmd->pdev_id, cmd->bitmap[0], cmd->bitmap[1]);

	return 0;
}

int
ath11k_wmi_pdev_srg_obss_bssid_enable_bitmap(struct ath11k *ar, u32 *bitmap)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct ath11k_base *ab = wmi->wmi_ab->ab;
	struct wmi_pdev_obss_pd_bitmap_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_obss_pd_bitmap_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_PDEV_SRG_OBSS_BSSID_ENABLE_BITMAP_CMD) |
		FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	memcpy(cmd->bitmap, bitmap, sizeof(cmd->bitmap));

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SET_SRG_OBSS_BSSID_ENABLE_BITMAP_CMDID);
	if (ret) {
		ath11k_warn(ab,
			    "failed to send WMI_PDEV_SET_SRG_OBSS_BSSID_ENABLE_BITMAP_CMDID");
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd pdev set srg obss bssid enable bitmap pdev_id %d bssid enable bitmap %08x %08x\n",
		   cmd->pdev_id, cmd->bitmap[0], cmd->bitmap[1]);

	return 0;
}

int
ath11k_wmi_pdev_non_srg_obss_color_enable_bitmap(struct ath11k *ar, u32 *bitmap)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct ath11k_base *ab = wmi->wmi_ab->ab;
	struct wmi_pdev_obss_pd_bitmap_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_obss_pd_bitmap_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_PDEV_NON_SRG_OBSS_COLOR_ENABLE_BITMAP_CMD) |
		FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	memcpy(cmd->bitmap, bitmap, sizeof(cmd->bitmap));

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SET_NON_SRG_OBSS_COLOR_ENABLE_BITMAP_CMDID);
	if (ret) {
		ath11k_warn(ab,
			    "failed to send WMI_PDEV_SET_NON_SRG_OBSS_COLOR_ENABLE_BITMAP_CMDID");
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd pdev set non srg obss color enable bitmap pdev_id %d bss color enable bitmap %08x %08x\n",
		   cmd->pdev_id, cmd->bitmap[0], cmd->bitmap[1]);

	return 0;
}

int
ath11k_wmi_pdev_non_srg_obss_bssid_enable_bitmap(struct ath11k *ar, u32 *bitmap)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct ath11k_base *ab = wmi->wmi_ab->ab;
	struct wmi_pdev_obss_pd_bitmap_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_obss_pd_bitmap_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_PDEV_NON_SRG_OBSS_BSSID_ENABLE_BITMAP_CMD) |
		FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	memcpy(cmd->bitmap, bitmap, sizeof(cmd->bitmap));

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SET_NON_SRG_OBSS_BSSID_ENABLE_BITMAP_CMDID);
	if (ret) {
		ath11k_warn(ab,
			    "failed to send WMI_PDEV_SET_NON_SRG_OBSS_BSSID_ENABLE_BITMAP_CMDID");
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd pdev set non srg obss bssid enable bitmap pdev_id %d bssid enable bitmap %08x %08x\n",
		   cmd->pdev_id, cmd->bitmap[0], cmd->bitmap[1]);

	return 0;
}

int
ath11k_wmi_send_obss_color_collision_cfg_cmd(struct ath11k *ar, u32 vdev_id,
					     u8 bss_color, u32 period,
					     bool enable)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct ath11k_base *ab = wmi->wmi_ab->ab;
	struct wmi_obss_color_collision_cfg_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_obss_color_collision_cfg_params_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_OBSS_COLOR_COLLISION_DET_CONFIG) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	cmd->evt_type = enable ? ATH11K_OBSS_COLOR_COLLISION_DETECTION :
				 ATH11K_OBSS_COLOR_COLLISION_DETECTION_DISABLE;
	cmd->current_bss_color = bss_color;
	cmd->detection_period_ms = period;
	cmd->scan_period_ms = ATH11K_BSS_COLOR_COLLISION_SCAN_PERIOD_MS;
	cmd->free_slot_expiry_time_ms = 0;
	cmd->flags = 0;

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_OBSS_COLOR_COLLISION_DET_CONFIG_CMDID);
	if (ret) {
		ath11k_warn(ab, "Failed to send WMI_OBSS_COLOR_COLLISION_DET_CONFIG_CMDID");
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd obss color collision det config id %d type %d bss_color %d detect_period %d scan_period %d\n",
		   cmd->vdev_id, cmd->evt_type, cmd->current_bss_color,
		   cmd->detection_period_ms, cmd->scan_period_ms);

	return 0;
}

int ath11k_wmi_send_bss_color_change_enable_cmd(struct ath11k *ar, u32 vdev_id,
						bool enable)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct ath11k_base *ab = wmi->wmi_ab->ab;
	struct wmi_bss_color_change_enable_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_bss_color_change_enable_params_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_BSS_COLOR_CHANGE_ENABLE) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	cmd->enable = enable ? 1 : 0;

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_BSS_COLOR_CHANGE_ENABLE_CMDID);
	if (ret) {
		ath11k_warn(ab, "Failed to send WMI_BSS_COLOR_CHANGE_ENABLE_CMDID");
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd bss color change enable id %d enable %d\n",
		   cmd->vdev_id, cmd->enable);

	return 0;
}

int ath11k_wmi_send_aggr_size_cmd(struct ath11k *ar,
				  struct set_custom_aggr_size_params *params)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct ath11k_base *ab = wmi->wmi_ab->ab;
	struct wmi_set_custom_aggr_size_params_cmd *cmd = NULL;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (void *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_VDEV_SET_CUSTOM_AGGR_SIZE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);

	cmd->vdev_id = params->vdev_id;
	cmd->tx_aggr_size = params->tx_aggr_size;
	cmd->rx_aggr_size = params->rx_aggr_size;
	cmd->enable_bitmap = FIELD_PREP(WMI_VDEV_AGGR_AC, params->ac) |
			     FIELD_PREP(WMI_VDEV_AGGR_TYPE, params->aggr_type) |
			     FIELD_PREP(WMI_VDEV_TX_AGGR_SZ_DISABLE,
					params->tx_aggr_size_disable) |
			     FIELD_PREP(WMI_VDEV_RX_AGGR_SZ_DISABLE,
					params->rx_aggr_size_disable) |
			     FIELD_PREP(WMI_VDEV_AGGR_AC_ENABLE,
					params->tx_ac_enable);

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "WMI set aggr size vdev_id %d tx aggr size %d rx_aggr_size %d "
		   "access category %d aggr type %d tx_aggr_size_disable %d "
		   "rx_aggr_size_disable %d tx_ac_enable %d ", params->vdev_id,
		   params->tx_aggr_size, params->rx_aggr_size, params->ac,
		   params->aggr_type, params->tx_aggr_size_disable,
		   params->rx_aggr_size_disable, params->tx_ac_enable);

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_VDEV_SET_CUSTOM_AGGR_SIZE_CMDID);

	if (ret) {
		ath11k_warn(ab,
			    "Failed to send WMI_VDEV_SET_CUSTOM_AGGR_SIZE_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath11k_wmi_fils_discovery_tmpl(struct ath11k *ar, u32 vdev_id,
				   struct sk_buff *tmpl)
{
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int ret, len;
	size_t aligned_len;
	struct wmi_fils_discovery_tmpl_cmd *cmd;

	aligned_len = roundup(tmpl->len, 4);
	len = sizeof(*cmd) + TLV_HDR_SIZE + aligned_len;

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "vdev %i set FILS discovery template\n", vdev_id);

	skb = ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_fils_discovery_tmpl_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_FILS_DISCOVERY_TMPL_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	cmd->buf_len = tmpl->len;
	ptr = skb->data + sizeof(*cmd);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_BYTE) |
		      FIELD_PREP(WMI_TLV_LEN, aligned_len);
	memcpy(tlv->value, tmpl->data, tmpl->len);

	ret = ath11k_wmi_cmd_send(ar->wmi, skb, WMI_FILS_DISCOVERY_TMPL_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "WMI vdev %i failed to send FILS discovery template command\n",
			    vdev_id);
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "cmd fils discovery tmpl");

	return 0;
}

int ath11k_wmi_peer_set_cfr_capture_conf(struct ath11k *ar,
					 u32 vdev_id, const u8 *mac_addr,
					 struct wmi_peer_cfr_capture_conf_arg *arg)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_peer_cfr_capture_cmd_fixed_param *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_cfr_capture_cmd_fixed_param *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PEER_CFR_CAPTURE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	ether_addr_copy(cmd->mac_addr.addr, mac_addr);
	cmd->request = arg->request;
	cmd->vdev_id = vdev_id;
	cmd->periodicity = arg->periodicity;
	cmd->bandwidth = arg->bandwidth;
	cmd->capture_method = arg->capture_method;

	ret = ath11k_wmi_cmd_send(ar->wmi, skb, WMI_PEER_CFR_CAPTURE_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "WMI vdev %d failed to send peer cfr capture cmd\n",
			    vdev_id);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath11k_wmi_probe_resp_tmpl(struct ath11k *ar, u32 vdev_id,
			       struct sk_buff *tmpl)
{
	struct wmi_probe_tmpl_cmd *cmd;
	struct wmi_bcn_prb_info *probe_info;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int ret, len;
	size_t aligned_len = roundup(tmpl->len, 4);

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "vdev %i set probe response template\n", vdev_id);

	len = sizeof(*cmd) + sizeof(*probe_info) + TLV_HDR_SIZE + aligned_len;

	skb = ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_probe_tmpl_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PRB_TMPL_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	cmd->buf_len = tmpl->len;

	ptr = skb->data + sizeof(*cmd);

	probe_info = ptr;
	len = sizeof(*probe_info);
	probe_info->tlv_header = FIELD_PREP(WMI_TLV_TAG,
					    WMI_TAG_BCN_PRB_INFO) |
				 FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	probe_info->caps = 0;
	probe_info->erp = 0;

	ptr += sizeof(*probe_info);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_BYTE) |
		      FIELD_PREP(WMI_TLV_LEN, aligned_len);
	memcpy(tlv->value, tmpl->data, tmpl->len);

	ret = ath11k_wmi_cmd_send(ar->wmi, skb, WMI_PRB_TMPL_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "WMI vdev %i failed to send probe response template command\n",
			    vdev_id);
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "cmd ");

	return 0;
}

int ath11k_wmi_fils_discovery(struct ath11k *ar, u32 vdev_id, u32 interval,
			      bool unsol_bcast_probe_resp_enabled)
{
	struct sk_buff *skb;
	int ret, len;
	struct wmi_fils_discovery_cmd *cmd;

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "vdev %i set %s interval to %u TU\n",
		   vdev_id, unsol_bcast_probe_resp_enabled ?
		   "unsolicited broadcast probe response" : "FILS discovery",
		   interval);

	len = sizeof(*cmd);
	skb = ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_fils_discovery_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ENABLE_FILS_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	cmd->interval = interval;
	cmd->config = unsol_bcast_probe_resp_enabled;

	ret = ath11k_wmi_cmd_send(ar->wmi, skb, WMI_ENABLE_FILS_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "WMI vdev %i failed to send FILS discovery enable/disable command\n",
			    vdev_id);
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "cmd enable fils");

	return 0;
}

static void
ath11k_wmi_obss_color_collision_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_obss_color_collision_event *ev;
	struct ath11k_vif *arvif;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event obss color collision");

	rcu_read_lock();

	ev = tb[WMI_TAG_OBSS_COLOR_COLLISION_EVT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch obss color collision ev");
		goto exit;
	}

	arvif = ath11k_mac_get_arvif_by_vdev_id(ab, ev->vdev_id);
	if (!arvif) {
		ath11k_warn(ab, "failed to find arvif with vedv id %d in obss_color_collision_event\n",
			    ev->vdev_id);
		goto exit;
	}

	switch (ev->evt_type) {
	case WMI_BSS_COLOR_COLLISION_DETECTION:
		ieee80211_obss_color_collision_notify(arvif->vif, ev->obss_color_bitmap,
						      GFP_KERNEL, 0);
		ath11k_dbg(ab, ATH11K_DBG_WMI,
			   "OBSS color collision detected vdev:%d, event:%d, bitmap:%08llx\n",
			   ev->vdev_id, ev->evt_type, ev->obss_color_bitmap);
		break;
	case WMI_BSS_COLOR_COLLISION_DISABLE:
	case WMI_BSS_COLOR_FREE_SLOT_TIMER_EXPIRY:
	case WMI_BSS_COLOR_FREE_SLOT_AVAILABLE:
		break;
	default:
		ath11k_warn(ab, "received unknown obss color collision detection event\n");
	}

exit:
	kfree(tb);
	rcu_read_unlock();
}

static void
ath11k_fill_band_to_mac_param(struct ath11k_base  *soc,
			      struct wmi_host_pdev_band_to_mac *band_to_mac)
{
	u8 i;
	struct ath11k_hal_reg_capabilities_ext *hal_reg_cap;
	struct ath11k_pdev *pdev;

	for (i = 0; i < soc->num_radios; i++) {
		pdev = &soc->pdevs[i];
		hal_reg_cap = &soc->hal_reg_cap[i];
		band_to_mac[i].pdev_id = pdev->pdev_id;

		switch (pdev->cap.supported_bands) {
		case WMI_HOST_WLAN_2G_5G_CAP:
			band_to_mac[i].start_freq = hal_reg_cap->low_2ghz_chan;
			band_to_mac[i].end_freq = hal_reg_cap->high_5ghz_chan;
			break;
		case WMI_HOST_WLAN_2G_CAP:
			band_to_mac[i].start_freq = hal_reg_cap->low_2ghz_chan;
			band_to_mac[i].end_freq = hal_reg_cap->high_2ghz_chan;
			pdev->phy_name = ATH11K_PHY_2GHZ;
			break;
		case WMI_HOST_WLAN_5G_CAP:
			band_to_mac[i].start_freq = hal_reg_cap->low_5ghz_chan;
			band_to_mac[i].end_freq = hal_reg_cap->high_5ghz_chan;
			if (hal_reg_cap->low_5ghz_chan >= ATH11K_MIN_6G_FREQ &&
			    hal_reg_cap->high_5ghz_chan <= ATH11K_MAX_6G_FREQ)
				pdev->phy_name = ATH11K_PHY_6GHZ;
			else
				pdev->phy_name = ATH11K_PHY_5GHZ;
			break;
		default:
			break;
		}
	}
}

static void
ath11k_wmi_copy_resource_config(struct wmi_resource_config *wmi_cfg,
				struct target_resource_config *tg_cfg)
{
	wmi_cfg->num_vdevs = tg_cfg->num_vdevs;
	wmi_cfg->num_peers = tg_cfg->num_peers;
	wmi_cfg->num_offload_peers = tg_cfg->num_offload_peers;
	wmi_cfg->num_offload_reorder_buffs = tg_cfg->num_offload_reorder_buffs;
	wmi_cfg->num_peer_keys = tg_cfg->num_peer_keys;
	wmi_cfg->num_tids = tg_cfg->num_tids;
	wmi_cfg->ast_skid_limit = tg_cfg->ast_skid_limit;
	wmi_cfg->tx_chain_mask = tg_cfg->tx_chain_mask;
	wmi_cfg->rx_chain_mask = tg_cfg->rx_chain_mask;
	wmi_cfg->rx_timeout_pri[0] = tg_cfg->rx_timeout_pri[0];
	wmi_cfg->rx_timeout_pri[1] = tg_cfg->rx_timeout_pri[1];
	wmi_cfg->rx_timeout_pri[2] = tg_cfg->rx_timeout_pri[2];
	wmi_cfg->rx_timeout_pri[3] = tg_cfg->rx_timeout_pri[3];
	wmi_cfg->rx_decap_mode = tg_cfg->rx_decap_mode;
	wmi_cfg->scan_max_pending_req = tg_cfg->scan_max_pending_req;
	wmi_cfg->bmiss_offload_max_vdev = tg_cfg->bmiss_offload_max_vdev;
	wmi_cfg->roam_offload_max_vdev = tg_cfg->roam_offload_max_vdev;
	wmi_cfg->roam_offload_max_ap_profiles =
		tg_cfg->roam_offload_max_ap_profiles;
	wmi_cfg->num_mcast_groups = tg_cfg->num_mcast_groups;
	wmi_cfg->num_mcast_table_elems = tg_cfg->num_mcast_table_elems;
	wmi_cfg->mcast2ucast_mode = tg_cfg->mcast2ucast_mode;
	wmi_cfg->tx_dbg_log_size = tg_cfg->tx_dbg_log_size;
	wmi_cfg->num_wds_entries = tg_cfg->num_wds_entries;
	wmi_cfg->dma_burst_size = tg_cfg->dma_burst_size;
	wmi_cfg->mac_aggr_delim = tg_cfg->mac_aggr_delim;
	wmi_cfg->rx_skip_defrag_timeout_dup_detection_check =
		tg_cfg->rx_skip_defrag_timeout_dup_detection_check;
	wmi_cfg->vow_config = tg_cfg->vow_config;
	wmi_cfg->gtk_offload_max_vdev = tg_cfg->gtk_offload_max_vdev;
	wmi_cfg->num_msdu_desc = tg_cfg->num_msdu_desc;
	wmi_cfg->max_frag_entries = tg_cfg->max_frag_entries;
	wmi_cfg->num_tdls_vdevs = tg_cfg->num_tdls_vdevs;
	wmi_cfg->num_tdls_conn_table_entries =
		tg_cfg->num_tdls_conn_table_entries;
	wmi_cfg->beacon_tx_offload_max_vdev =
		tg_cfg->beacon_tx_offload_max_vdev;
	wmi_cfg->num_multicast_filter_entries =
		tg_cfg->num_multicast_filter_entries;
	wmi_cfg->num_wow_filters = tg_cfg->num_wow_filters;
	wmi_cfg->num_keep_alive_pattern = tg_cfg->num_keep_alive_pattern;
	wmi_cfg->keep_alive_pattern_size = tg_cfg->keep_alive_pattern_size;
	wmi_cfg->max_tdls_concurrent_sleep_sta =
		tg_cfg->max_tdls_concurrent_sleep_sta;
	wmi_cfg->max_tdls_concurrent_buffer_sta =
		tg_cfg->max_tdls_concurrent_buffer_sta;
	wmi_cfg->wmi_send_separate = tg_cfg->wmi_send_separate;
	wmi_cfg->num_ocb_vdevs = tg_cfg->num_ocb_vdevs;
	wmi_cfg->num_ocb_channels = tg_cfg->num_ocb_channels;
	wmi_cfg->num_ocb_schedules = tg_cfg->num_ocb_schedules;
	wmi_cfg->bpf_instruction_size = tg_cfg->bpf_instruction_size;
	wmi_cfg->max_bssid_rx_filters = tg_cfg->max_bssid_rx_filters;
	wmi_cfg->use_pdev_id = tg_cfg->use_pdev_id;
	wmi_cfg->flag1 = tg_cfg->flag1;
	wmi_cfg->flag1 |= WMI_RSRC_CFG_FLAG1_ACK_RSSI;
	wmi_cfg->peer_map_unmap_v2_support = tg_cfg->peer_map_unmap_v2_support;
	wmi_cfg->sched_params = tg_cfg->sched_params;
	wmi_cfg->twt_ap_pdev_count = tg_cfg->twt_ap_pdev_count;
	wmi_cfg->twt_ap_sta_count = tg_cfg->twt_ap_sta_count;
	wmi_cfg->host_service_flags &=
		~(1 << WMI_CFG_HOST_SERVICE_FLAG_REG_CC_EXT);
	wmi_cfg->host_service_flags |= (tg_cfg->is_reg_cc_ext_event_supported <<
					WMI_CFG_HOST_SERVICE_FLAG_REG_CC_EXT);
	wmi_cfg->flags2 = WMI_RSRC_CFG_FLAG2_CALC_NEXT_DTIM_COUNT_SET;
	wmi_cfg->ema_max_vap_cnt = tg_cfg->ema_max_vap_cnt;
	wmi_cfg->ema_max_profile_period = tg_cfg->ema_max_profile_period;
	wmi_cfg->max_num_group_keys = tg_cfg->max_num_group_keys;
	wmi_cfg->smart_ant_cap = 1;
	wmi_cfg->flag1 |= tg_cfg->peer_tid_ext;
}

static int ath11k_init_cmd_send(struct ath11k_pdev_wmi *wmi,
				struct wmi_init_cmd_param *param)
{
	struct ath11k_base *ab = wmi->wmi_ab->ab;
	struct sk_buff *skb;
	struct wmi_init_cmd *cmd;
	struct wmi_resource_config *cfg;
	struct wmi_pdev_set_hw_mode_cmd_param *hw_mode;
	struct wmi_pdev_band_to_mac *band_to_mac;
	struct wlan_host_mem_chunk *host_mem_chunks;
	struct wmi_tlv *tlv;
	struct device *dev = ab->dev;
	size_t ret, len;
	bool three_way_coex_enabled = false;
	void *ptr;
	u32 hw_mode_len = 0;
	u16 idx;

	if (param->hw_mode_id != WMI_HOST_HW_MODE_MAX)
		hw_mode_len = sizeof(*hw_mode) + TLV_HDR_SIZE +
			      (param->num_band_to_mac * sizeof(*band_to_mac));

	len = sizeof(*cmd) + TLV_HDR_SIZE + sizeof(*cfg) + hw_mode_len +
	      (param->num_mem_chunks ? (sizeof(*host_mem_chunks) * WMI_MAX_MEM_REQS) : 0);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_init_cmd *)skb->data;

	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_INIT_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	ptr = skb->data + sizeof(*cmd);
	cfg = ptr;

	ath11k_wmi_copy_resource_config(cfg, param->res_cfg);

	three_way_coex_enabled = of_property_read_bool(dev->of_node, "qcom,btcoex");
	if (three_way_coex_enabled)
		cfg->flag1 |= WMI_RSRC_CFG_FLAG1_THREE_WAY_COEX_CONFIG_OVERRIDE_SUPPORT;
	cfg->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_RESOURCE_CONFIG) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cfg) - TLV_HDR_SIZE);

	ptr += sizeof(*cfg);
	host_mem_chunks = ptr + TLV_HDR_SIZE;
	len = sizeof(struct wlan_host_mem_chunk);

	for (idx = 0; idx < param->num_mem_chunks; ++idx) {
		host_mem_chunks[idx].tlv_header =
				FIELD_PREP(WMI_TLV_TAG,
					   WMI_TAG_WLAN_HOST_MEMORY_CHUNK) |
				FIELD_PREP(WMI_TLV_LEN, len);

		host_mem_chunks[idx].ptr = param->mem_chunks[idx].paddr;
		host_mem_chunks[idx].size = param->mem_chunks[idx].len;
		host_mem_chunks[idx].req_id = param->mem_chunks[idx].req_id;

		ath11k_dbg(ab, ATH11K_DBG_WMI,
			   "host mem chunk req_id %d paddr 0x%llx len %d\n",
			   param->mem_chunks[idx].req_id,
			   (u64)param->mem_chunks[idx].paddr,
			   param->mem_chunks[idx].len);
	}
	cmd->num_host_mem_chunks = param->num_mem_chunks;
	len = sizeof(struct wlan_host_mem_chunk) * param->num_mem_chunks;

	/* num_mem_chunks is zero */
	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, len);
	ptr += TLV_HDR_SIZE + len;

	if (param->hw_mode_id != WMI_HOST_HW_MODE_MAX) {
		hw_mode = ptr;
		hw_mode->tlv_header = FIELD_PREP(WMI_TLV_TAG,
						 WMI_TAG_PDEV_SET_HW_MODE_CMD) |
				      FIELD_PREP(WMI_TLV_LEN,
						 sizeof(*hw_mode) - TLV_HDR_SIZE);

		hw_mode->hw_mode_index = param->hw_mode_id;
		hw_mode->num_band_to_mac = param->num_band_to_mac;

		ptr += sizeof(*hw_mode);

		len = param->num_band_to_mac * sizeof(*band_to_mac);
		tlv = ptr;
		tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
			      FIELD_PREP(WMI_TLV_LEN, len);

		ptr += TLV_HDR_SIZE;
		len = sizeof(*band_to_mac);

		for (idx = 0; idx < param->num_band_to_mac; idx++) {
			band_to_mac = ptr;

			band_to_mac->tlv_header = FIELD_PREP(WMI_TLV_TAG,
							     WMI_TAG_PDEV_BAND_TO_MAC) |
						  FIELD_PREP(WMI_TLV_LEN,
							     len - TLV_HDR_SIZE);
			band_to_mac->pdev_id = param->band_to_mac[idx].pdev_id;
			band_to_mac->start_freq =
				param->band_to_mac[idx].start_freq;
			band_to_mac->end_freq =
				param->band_to_mac[idx].end_freq;
			ptr += sizeof(*band_to_mac);
		}
	}

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_INIT_CMDID);
	if (ret) {
		ath11k_warn(ab, "failed to send WMI_INIT_CMDID\n");
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "cmd wmi init");

	return 0;
}

int ath11k_wmi_pdev_lro_cfg(struct ath11k *ar,
			    int pdev_id)
{
	struct ath11k_wmi_pdev_lro_config_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct ath11k_wmi_pdev_lro_config_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_LRO_INFO_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	get_random_bytes(cmd->th_4, sizeof(uint32_t) * ATH11K_IPV4_TH_SEED_SIZE);
	get_random_bytes(cmd->th_6, sizeof(uint32_t) * ATH11K_IPV6_TH_SEED_SIZE);

	cmd->pdev_id = pdev_id;

	ret = ath11k_wmi_cmd_send(ar->wmi, skb, WMI_LRO_CONFIG_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send lro cfg req wmi cmd\n");
		goto err;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd lro config pdev_id 0x%x\n", pdev_id);
	return 0;
err:
	dev_kfree_skb(skb);
	return ret;
}

int ath11k_wmi_wait_for_service_ready(struct ath11k_base *ab)
{
	unsigned long time_left;

	time_left = wait_for_completion_timeout(&ab->wmi_ab.service_ready,
						WMI_SERVICE_READY_TIMEOUT_HZ);
	if (!time_left)
		return -ETIMEDOUT;

	return 0;
}

int ath11k_wmi_wait_for_unified_ready(struct ath11k_base *ab)
{
	unsigned long time_left;

	time_left = wait_for_completion_timeout(&ab->wmi_ab.unified_ready,
						WMI_SERVICE_READY_TIMEOUT_HZ);
	if (!time_left)
		return -ETIMEDOUT;

	return 0;
}

int ath11k_wmi_set_hw_mode(struct ath11k_base *ab,
			   enum wmi_host_hw_mode_config_type mode)
{
	struct wmi_pdev_set_hw_mode_cmd_param *cmd;
	struct sk_buff *skb;
	struct ath11k_wmi_base *wmi_ab = &ab->wmi_ab;
	int len;
	int ret;

	len = sizeof(*cmd);

	skb = ath11k_wmi_alloc_skb(wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_hw_mode_cmd_param *)skb->data;

	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PDEV_SET_HW_MODE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->pdev_id = WMI_PDEV_ID_SOC;
	cmd->hw_mode_index = mode;

	ret = ath11k_wmi_cmd_send(&wmi_ab->wmi[0], skb, WMI_PDEV_SET_HW_MODE_CMDID);
	if (ret) {
		ath11k_warn(ab, "failed to send WMI_PDEV_SET_HW_MODE_CMDID\n");
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "cmd pdev set hw mode %d", cmd->hw_mode_index);

	return 0;
}

int ath11k_wmi_cmd_init(struct ath11k_base *ab)
{
	struct ath11k_wmi_base *wmi_ab = &ab->wmi_ab;
	struct wmi_init_cmd_param init_param;
	struct target_resource_config  config;

	memset(&init_param, 0, sizeof(init_param));
	memset(&config, 0, sizeof(config));

	if (ab->nss.enabled)
		config.max_num_group_keys = ATH11K_GROUP_KEYS_NUM_MAX;

	ab->hw_params.hw_ops->wmi_init_config(ab, &config);

	if (test_bit(WMI_TLV_SERVICE_REG_CC_EXT_EVENT_SUPPORT,
		     ab->wmi_ab.svc_map))
		config.is_reg_cc_ext_event_supported = 1;

	memcpy(&wmi_ab->wlan_resource_config, &config, sizeof(config));

	init_param.res_cfg = &wmi_ab->wlan_resource_config;
	init_param.num_mem_chunks = wmi_ab->num_mem_chunks;
	init_param.hw_mode_id = wmi_ab->preferred_hw_mode;
	init_param.mem_chunks = wmi_ab->mem_chunks;

	if (ab->hw_params.single_pdev_only)
		init_param.hw_mode_id = WMI_HOST_HW_MODE_MAX;

	init_param.num_band_to_mac = ab->num_radios;
	ath11k_fill_band_to_mac_param(ab, init_param.band_to_mac);

	return ath11k_init_cmd_send(&wmi_ab->wmi[0], &init_param);
}

int ath11k_wmi_vdev_spectral_conf(struct ath11k *ar,
				  struct ath11k_wmi_vdev_spectral_conf_param *param)
{
	struct ath11k_wmi_vdev_spectral_conf_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct ath11k_wmi_vdev_spectral_conf_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_VDEV_SPECTRAL_CONFIGURE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	memcpy(&cmd->param, param, sizeof(*param));

	ret = ath11k_wmi_cmd_send(ar->wmi, skb,
				  WMI_VDEV_SPECTRAL_SCAN_CONFIGURE_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send spectral scan config wmi cmd\n");
		goto err;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd vdev spectral scan configure vdev_id 0x%x\n",
		   param->vdev_id);

	return 0;
err:
	dev_kfree_skb(skb);
	return ret;
}

int ath11k_wmi_vdev_spectral_enable(struct ath11k *ar, u32 vdev_id,
				    u32 trigger, u32 enable)
{
	struct ath11k_wmi_vdev_spectral_enable_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct ath11k_wmi_vdev_spectral_enable_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_VDEV_SPECTRAL_ENABLE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = vdev_id;
	cmd->trigger_cmd = trigger;
	cmd->enable_cmd = enable;

	ret = ath11k_wmi_cmd_send(ar->wmi, skb,
				  WMI_VDEV_SPECTRAL_SCAN_ENABLE_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send spectral enable wmi cmd\n");
		goto err;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd vdev spectral scan enable vdev id 0x%x\n",
		   vdev_id);

	return 0;
err:
	dev_kfree_skb(skb);
	return ret;
}

int ath11k_wmi_pdev_dma_ring_cfg(struct ath11k *ar,
				 struct ath11k_wmi_pdev_dma_ring_cfg_req_cmd *param)
{
	struct ath11k_wmi_pdev_dma_ring_cfg_req_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct ath11k_wmi_pdev_dma_ring_cfg_req_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_DMA_RING_CFG_REQ) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->pdev_id		= param->pdev_id;
	cmd->module_id		= param->module_id;
	cmd->base_paddr_lo	= param->base_paddr_lo;
	cmd->base_paddr_hi	= param->base_paddr_hi;
	cmd->head_idx_paddr_lo	= param->head_idx_paddr_lo;
	cmd->head_idx_paddr_hi	= param->head_idx_paddr_hi;
	cmd->tail_idx_paddr_lo	= param->tail_idx_paddr_lo;
	cmd->tail_idx_paddr_hi	= param->tail_idx_paddr_hi;
	cmd->num_elems		= param->num_elems;
	cmd->buf_size		= param->buf_size;
	cmd->num_resp_per_event	= param->num_resp_per_event;
	cmd->event_timeout_ms	= param->event_timeout_ms;

	ret = ath11k_wmi_cmd_send(ar->wmi, skb,
				  WMI_PDEV_DMA_RING_CFG_REQ_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send dma ring cfg req wmi cmd\n");
		goto err;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd pdev dma ring cfg req pdev_id 0x%x\n",
		   param->pdev_id);

	return 0;
err:
	dev_kfree_skb(skb);
	return ret;
}

static int ath11k_wmi_tlv_dma_buf_entry_parse(struct ath11k_base *soc,
					      u16 tag, u16 len,
					      const void *ptr, void *data)
{
	struct wmi_tlv_dma_buf_release_parse *parse = data;

	if (tag != WMI_TAG_DMA_BUF_RELEASE_ENTRY)
		return -EPROTO;

	if (parse->num_buf_entry >= parse->fixed.num_buf_release_entry)
		return -ENOBUFS;

	parse->num_buf_entry++;
	return 0;
}

static int ath11k_wmi_tlv_dma_buf_meta_parse(struct ath11k_base *soc,
					     u16 tag, u16 len,
					     const void *ptr, void *data)
{
	struct wmi_tlv_dma_buf_release_parse *parse = data;

	if (tag != WMI_TAG_DMA_BUF_RELEASE_SPECTRAL_META_DATA)
		return -EPROTO;

	if (parse->num_meta >= parse->fixed.num_meta_data_entry)
		return -ENOBUFS;

	parse->num_meta++;
	return 0;
}

static int ath11k_wmi_tlv_dma_buf_parse(struct ath11k_base *ab,
					u16 tag, u16 len,
					const void *ptr, void *data)
{
	struct wmi_tlv_dma_buf_release_parse *parse = data;
	int ret;

	switch (tag) {
	case WMI_TAG_DMA_BUF_RELEASE:
		memcpy(&parse->fixed, ptr,
		       sizeof(struct ath11k_wmi_dma_buf_release_fixed_param));
		parse->fixed.pdev_id = DP_HW2SW_MACID(parse->fixed.pdev_id);
		break;
	case WMI_TAG_ARRAY_STRUCT:
		if (!parse->buf_entry_done) {
			parse->num_buf_entry = 0;
			parse->buf_entry = (struct wmi_dma_buf_release_entry *)ptr;

			ret = ath11k_wmi_tlv_iter(ab, ptr, len,
						  ath11k_wmi_tlv_dma_buf_entry_parse,
						  parse);
			if (ret) {
				ath11k_warn(ab, "failed to parse dma buf entry tlv %d\n",
					    ret);
				return ret;
			}

			parse->buf_entry_done = true;
		} else if (!parse->meta_data_done) {
			parse->num_meta = 0;
			parse->meta_data = (struct wmi_dma_buf_release_meta_data *)ptr;

			ret = ath11k_wmi_tlv_iter(ab, ptr, len,
						  ath11k_wmi_tlv_dma_buf_meta_parse,
						  parse);
			if (ret) {
				ath11k_warn(ab, "failed to parse dma buf meta tlv %d\n",
					    ret);
				return ret;
			}

			parse->meta_data_done = true;
		}
		break;
	default:
		break;
	}
	return 0;
}

static void ath11k_wmi_pdev_dma_ring_buf_release_event(struct ath11k_base *ab,
						       struct sk_buff *skb)
{
	struct wmi_tlv_dma_buf_release_parse parse = { };
	struct ath11k_dbring_buf_release_event param;
	int ret;

	ret = ath11k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath11k_wmi_tlv_dma_buf_parse,
				  &parse);
	if (ret) {
		ath11k_warn(ab, "failed to parse dma buf release tlv %d\n", ret);
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event pdev dma ring buf release");

	param.fixed		= parse.fixed;
	param.buf_entry		= parse.buf_entry;
	param.num_buf_entry	= parse.num_buf_entry;
	param.meta_data		= parse.meta_data;
	param.num_meta		= parse.num_meta;

	ret = ath11k_dbring_buffer_release_event(ab, &param);
	if (ret) {
		ath11k_warn(ab, "failed to handle dma buf release event %d\n", ret);
		return;
	}
}

static int ath11k_wmi_tlv_hw_mode_caps_parse(struct ath11k_base *soc,
					     u16 tag, u16 len,
					     const void *ptr, void *data)
{
	struct wmi_tlv_svc_rdy_ext_parse *svc_rdy_ext = data;
	struct wmi_hw_mode_capabilities *hw_mode_cap;
	u32 phy_map = 0;

	if (tag != WMI_TAG_HW_MODE_CAPABILITIES)
		return -EPROTO;

	if (svc_rdy_ext->n_hw_mode_caps >= svc_rdy_ext->param.num_hw_modes)
		return -ENOBUFS;

	hw_mode_cap = container_of(ptr, struct wmi_hw_mode_capabilities,
				   hw_mode_id);
	svc_rdy_ext->n_hw_mode_caps++;

	phy_map = hw_mode_cap->phy_id_map;
	while (phy_map) {
		svc_rdy_ext->tot_phy_id++;
		phy_map = phy_map >> 1;
	}

	return 0;
}

static int ath11k_wmi_tlv_hw_mode_caps(struct ath11k_base *soc,
				       u16 len, const void *ptr, void *data)
{
	struct wmi_tlv_svc_rdy_ext_parse *svc_rdy_ext = data;
	struct wmi_hw_mode_capabilities *hw_mode_caps;
	enum wmi_host_hw_mode_config_type mode, pref;
	u32 i;
	int ret;

	svc_rdy_ext->n_hw_mode_caps = 0;
	svc_rdy_ext->hw_mode_caps = (struct wmi_hw_mode_capabilities *)ptr;

	ret = ath11k_wmi_tlv_iter(soc, ptr, len,
				  ath11k_wmi_tlv_hw_mode_caps_parse,
				  svc_rdy_ext);
	if (ret) {
		ath11k_warn(soc, "failed to parse tlv %d\n", ret);
		return ret;
	}

	i = 0;
	while (i < svc_rdy_ext->n_hw_mode_caps) {
		hw_mode_caps = &svc_rdy_ext->hw_mode_caps[i];
		mode = hw_mode_caps->hw_mode_id;
		pref = soc->wmi_ab.preferred_hw_mode;

		if (ath11k_hw_mode_pri_map[mode] < ath11k_hw_mode_pri_map[pref]) {
			svc_rdy_ext->pref_hw_mode_caps = *hw_mode_caps;
			soc->wmi_ab.preferred_hw_mode = mode;
		}
		i++;
	}

	ath11k_dbg(soc, ATH11K_DBG_WMI, "preferred_hw_mode:%d\n",
		   soc->wmi_ab.preferred_hw_mode);
	if (soc->wmi_ab.preferred_hw_mode == WMI_HOST_HW_MODE_MAX)
		return -EINVAL;

	return 0;
}

static int ath11k_wmi_tlv_mac_phy_caps_parse(struct ath11k_base *soc,
					     u16 tag, u16 len,
					     const void *ptr, void *data)
{
	struct wmi_tlv_svc_rdy_ext_parse *svc_rdy_ext = data;

	if (tag != WMI_TAG_MAC_PHY_CAPABILITIES)
		return -EPROTO;

	if (svc_rdy_ext->n_mac_phy_caps >= svc_rdy_ext->tot_phy_id)
		return -ENOBUFS;

	len = min_t(u16, len, sizeof(struct wmi_mac_phy_capabilities));
	if (!svc_rdy_ext->n_mac_phy_caps) {
		svc_rdy_ext->mac_phy_caps = kcalloc(svc_rdy_ext->tot_phy_id,
						    len, GFP_ATOMIC);
		if (!svc_rdy_ext->mac_phy_caps)
			return -ENOMEM;
	}

	memcpy(svc_rdy_ext->mac_phy_caps + svc_rdy_ext->n_mac_phy_caps, ptr, len);
	svc_rdy_ext->n_mac_phy_caps++;
	return 0;
}

static int ath11k_wmi_tlv_ext_hal_reg_caps_parse(struct ath11k_base *soc,
						 u16 tag, u16 len,
						 const void *ptr, void *data)
{
	struct wmi_tlv_svc_rdy_ext_parse *svc_rdy_ext = data;

	if (tag != WMI_TAG_HAL_REG_CAPABILITIES_EXT)
		return -EPROTO;

	if (svc_rdy_ext->n_ext_hal_reg_caps >= svc_rdy_ext->param.num_phy)
		return -ENOBUFS;

	svc_rdy_ext->n_ext_hal_reg_caps++;
	return 0;
}

static int ath11k_wmi_tlv_ext_hal_reg_caps(struct ath11k_base *soc,
					   u16 len, const void *ptr, void *data)
{
	struct ath11k_pdev_wmi *wmi_handle = &soc->wmi_ab.wmi[0];
	struct wmi_tlv_svc_rdy_ext_parse *svc_rdy_ext = data;
	struct ath11k_hal_reg_capabilities_ext reg_cap;
	int ret;
	u32 i;

	svc_rdy_ext->n_ext_hal_reg_caps = 0;
	svc_rdy_ext->ext_hal_reg_caps = (struct wmi_hal_reg_capabilities_ext *)ptr;
	ret = ath11k_wmi_tlv_iter(soc, ptr, len,
				  ath11k_wmi_tlv_ext_hal_reg_caps_parse,
				  svc_rdy_ext);
	if (ret) {
		ath11k_warn(soc, "failed to parse tlv %d\n", ret);
		return ret;
	}

	for (i = 0; i < svc_rdy_ext->param.num_phy; i++) {
		ret = ath11k_pull_reg_cap_svc_rdy_ext(wmi_handle,
						      svc_rdy_ext->soc_hal_reg_caps,
						      svc_rdy_ext->ext_hal_reg_caps, i,
						      &reg_cap);
		if (ret) {
			ath11k_warn(soc, "failed to extract reg cap %d\n", i);
			return ret;
		}

		memcpy(&soc->hal_reg_cap[reg_cap.phy_id],
		       &reg_cap, sizeof(reg_cap));
	}
	return 0;
}

static int ath11k_wmi_tlv_ext_soc_hal_reg_caps_parse(struct ath11k_base *soc,
						     u16 len, const void *ptr,
						     void *data)
{
	struct ath11k_pdev_wmi *wmi_handle = &soc->wmi_ab.wmi[0];
	struct wmi_tlv_svc_rdy_ext_parse *svc_rdy_ext = data;
	u8 hw_mode_id = svc_rdy_ext->pref_hw_mode_caps.hw_mode_id;
	u32 phy_id_map;
	int pdev_index = 0;
	int ret;

	svc_rdy_ext->soc_hal_reg_caps = (struct wmi_soc_hal_reg_capabilities *)ptr;
	svc_rdy_ext->param.num_phy = svc_rdy_ext->soc_hal_reg_caps->num_phy;

	soc->num_radios = 0;
	soc->target_pdev_count = 0;
	phy_id_map = svc_rdy_ext->pref_hw_mode_caps.phy_id_map;

	while (phy_id_map && soc->num_radios < MAX_RADIOS) {
		ret = ath11k_pull_mac_phy_cap_svc_ready_ext(wmi_handle,
							    svc_rdy_ext->hw_caps,
							    svc_rdy_ext->hw_mode_caps,
							    svc_rdy_ext->soc_hal_reg_caps,
							    svc_rdy_ext->mac_phy_caps,
							    hw_mode_id, soc->num_radios,
							    &soc->pdevs[pdev_index]);
		if (ret) {
			ath11k_warn(soc, "failed to extract mac caps, idx :%d\n",
				    soc->num_radios);
			return ret;
		}

		soc->num_radios++;

		/* For QCA6390, save mac_phy capability in the same pdev */
		if (soc->hw_params.single_pdev_only)
			pdev_index = 0;
		else
			pdev_index = soc->num_radios;

		/* TODO: mac_phy_cap prints */
		phy_id_map >>= 1;
	}

	/* For QCA6390, set num_radios to 1 because host manages
	 * both 2G and 5G radio in one pdev.
	 * Set pdev_id = 0 and 0 means soc level.
	 */
	if (soc->hw_params.single_pdev_only) {
		soc->num_radios = 1;
		soc->pdevs[0].pdev_id = 0;
	}

	if (!soc->reg_info_store) {
		soc->reg_info_store = kcalloc(soc->num_radios,
					      sizeof(*soc->reg_info_store),
					      GFP_ATOMIC);
		if (!soc->reg_info_store)
			return -ENOMEM;
	}

	return 0;
}

static int ath11k_wmi_tlv_dma_ring_caps_parse(struct ath11k_base *soc,
					      u16 tag, u16 len,
					      const void *ptr, void *data)
{
	struct wmi_tlv_dma_ring_caps_parse *parse = data;

	if (tag != WMI_TAG_DMA_RING_CAPABILITIES)
		return -EPROTO;

	parse->n_dma_ring_caps++;
	return 0;
}

static int ath11k_wmi_alloc_dbring_caps(struct ath11k_base *ab,
					u32 num_cap)
{
	size_t sz;
	void *ptr;

	sz = num_cap * sizeof(struct ath11k_dbring_cap);
	ptr = kzalloc(sz, GFP_ATOMIC);
	if (!ptr)
		return -ENOMEM;

	ab->db_caps = ptr;
	ab->num_db_cap = num_cap;

	return 0;
}

static void ath11k_wmi_free_dbring_caps(struct ath11k_base *ab)
{
	kfree(ab->db_caps);
	ab->db_caps = NULL;
	ab->num_db_cap = 0;
}

static int ath11k_wmi_tlv_dma_ring_caps(struct ath11k_base *ab,
					u16 len, const void *ptr, void *data)
{
	struct wmi_tlv_dma_ring_caps_parse *dma_caps_parse = data;
	struct wmi_dma_ring_capabilities *dma_caps;
	struct ath11k_dbring_cap *dir_buff_caps;
	int ret;
	u32 i;

	dma_caps_parse->n_dma_ring_caps = 0;
	dma_caps = (struct wmi_dma_ring_capabilities *)ptr;
	ret = ath11k_wmi_tlv_iter(ab, ptr, len,
				  ath11k_wmi_tlv_dma_ring_caps_parse,
				  dma_caps_parse);
	if (ret) {
		ath11k_warn(ab, "failed to parse dma ring caps tlv %d\n", ret);
		return ret;
	}

	if (!dma_caps_parse->n_dma_ring_caps)
		return 0;

	if (ab->num_db_cap) {
		ath11k_warn(ab, "Already processed, so ignoring dma ring caps\n");
		return 0;
	}

	ret = ath11k_wmi_alloc_dbring_caps(ab, dma_caps_parse->n_dma_ring_caps);
	if (ret)
		return ret;

	dir_buff_caps = ab->db_caps;
	for (i = 0; i < dma_caps_parse->n_dma_ring_caps; i++) {
		if (dma_caps[i].module_id >= WMI_DIRECT_BUF_MAX) {
			ath11k_warn(ab, "Invalid module id %d\n", dma_caps[i].module_id);
			ret = -EINVAL;
			goto free_dir_buff;
		}

		dir_buff_caps[i].id = dma_caps[i].module_id;
		dir_buff_caps[i].pdev_id = DP_HW2SW_MACID(dma_caps[i].pdev_id);
		dir_buff_caps[i].min_elem = dma_caps[i].min_elem;
		dir_buff_caps[i].min_buf_sz = dma_caps[i].min_buf_sz;
		dir_buff_caps[i].min_buf_align = dma_caps[i].min_buf_align;
	}

	return 0;

free_dir_buff:
	ath11k_wmi_free_dbring_caps(ab);
	return ret;
}

static int ath11k_wmi_tlv_mac_phy_chainmask_caps(struct ath11k_base *soc,
                                       u16 len, const void *ptr, void *data)
{
	struct wmi_tlv_svc_rdy_ext_parse *svc_rdy_ext = data;
	struct wmi_mac_phy_chainmask_caps *cmask_caps = (struct wmi_mac_phy_chainmask_caps *)ptr;
	struct ath11k_chainmask_table *cmask_table;
	struct ath11k_pdev_cap *pdev_cap;
	u32 tag;
	int i, j;

	if (!svc_rdy_ext->hw_mode_caps)
		return -EINVAL;

	if ((!svc_rdy_ext->param.num_chainmask_tables) ||
	    (svc_rdy_ext->param.num_chainmask_tables > ATH11K_MAX_CHAINMASK_TABLES))
		return -EINVAL;

	for (i = 0; i < svc_rdy_ext->param.num_chainmask_tables; i++) {
		cmask_table = &svc_rdy_ext->param.chainmask_table[i];

		for (j = 0; j < cmask_table->num_valid_chainmasks; j++) {
			tag = FIELD_GET(WMI_TLV_TAG, cmask_caps->tlv_header);

			if (tag != WMI_TAG_MAC_PHY_CHAINMASK_CAPABILITY)
				return -EPROTO;

			cmask_table->cap_list[j].chainmask = cmask_caps->chainmask;
			cmask_table->cap_list[j].supported_caps = cmask_caps->supported_flags;
			cmask_caps++;
		}
	}
	for (i = 0; i < svc_rdy_ext->tot_phy_id; i++) {
		pdev_cap = &soc->pdevs[i].cap;
		cmask_table = &svc_rdy_ext->param.chainmask_table[i];
		for (j = 0; j < cmask_table->num_valid_chainmasks; j++) {
			if (cmask_table->cap_list[j].supported_caps & WMI_SUPPORT_CHAIN_MASK_ADFS)
				pdev_cap->adfs_chain_mask |= (1 << cmask_table->cap_list[j].chainmask);
		}
	}
	return 0;
}

static void ath11k_wmi_free_chainmask_caps(struct wmi_tlv_svc_rdy_ext_parse *svc_rdy_ext)
{
	int i;

	if (!svc_rdy_ext->param.num_chainmask_tables)
		return;

	for (i = 0; i < svc_rdy_ext->param.num_chainmask_tables; i++) {
		if (!svc_rdy_ext->param.chainmask_table[i].cap_list)
			continue;
		kfree(svc_rdy_ext->param.chainmask_table[i].cap_list);
		svc_rdy_ext->param.chainmask_table[i].cap_list = NULL;
	}
}

static int ath11k_wmi_tlv_mac_phy_chainmask_combo_parse(struct ath11k_base *soc,
							u16 tag, u16 len,
							const void *ptr, void *data)
{
	struct wmi_tlv_svc_rdy_ext_parse *svc_rdy_ext = data;
	struct wmi_mac_phy_chainmask_combo *cmask_combo = (struct wmi_mac_phy_chainmask_combo *)ptr;
	u32 i = svc_rdy_ext->n_mac_phy_chainmask_combo;
	struct ath11k_chainmask_table *cmask_table;

	if (tag != WMI_TAG_MAC_PHY_CHAINMASK_COMBO)
		return -EPROTO;

	if (svc_rdy_ext->n_mac_phy_chainmask_combo >= svc_rdy_ext->param.num_chainmask_tables)
		return -ENOBUFS;

	cmask_table = &svc_rdy_ext->param.chainmask_table[i];
	cmask_table->table_id = cmask_combo->chainmask_table_id;
	cmask_table->num_valid_chainmasks = cmask_combo->num_valid_chainmask;
	cmask_table->cap_list = kcalloc(cmask_combo->num_valid_chainmask,
					sizeof(struct ath11k_chainmask_capabilities),
					GFP_ATOMIC);
	if (!cmask_table->cap_list)
		return -ENOMEM;

	svc_rdy_ext->n_mac_phy_chainmask_combo++;
	return 0;
}

static int ath11k_wmi_tlv_svc_rdy_ext_parse(struct ath11k_base *ab,
					    u16 tag, u16 len,
					    const void *ptr, void *data)
{
	struct ath11k_pdev_wmi *wmi_handle = &ab->wmi_ab.wmi[0];
	struct wmi_tlv_svc_rdy_ext_parse *svc_rdy_ext = data;
	int ret;

	switch (tag) {
	case WMI_TAG_SERVICE_READY_EXT_EVENT:
		ret = ath11k_pull_svc_ready_ext(wmi_handle, ptr,
						&svc_rdy_ext->param);
		if (ret) {
			ath11k_warn(ab, "unable to extract ext params\n");
			return ret;
		}
		break;

	case WMI_TAG_SOC_MAC_PHY_HW_MODE_CAPS:
		svc_rdy_ext->hw_caps = (struct wmi_soc_mac_phy_hw_mode_caps *)ptr;
		svc_rdy_ext->param.num_hw_modes = svc_rdy_ext->hw_caps->num_hw_modes;
		svc_rdy_ext->param.num_chainmask_tables = svc_rdy_ext->hw_caps->num_chainmask_tables;
		break;

	case WMI_TAG_SOC_HAL_REG_CAPABILITIES:
		ret = ath11k_wmi_tlv_ext_soc_hal_reg_caps_parse(ab, len, ptr,
								svc_rdy_ext);
		if (ret)
			return ret;
		break;

	case WMI_TAG_ARRAY_STRUCT:
		if (!svc_rdy_ext->hw_mode_done) {
			ret = ath11k_wmi_tlv_hw_mode_caps(ab, len, ptr,
							  svc_rdy_ext);
			if (ret)
				return ret;

			svc_rdy_ext->hw_mode_done = true;
		} else if (!svc_rdy_ext->mac_phy_done) {
			svc_rdy_ext->n_mac_phy_caps = 0;
			ret = ath11k_wmi_tlv_iter(ab, ptr, len,
						  ath11k_wmi_tlv_mac_phy_caps_parse,
						  svc_rdy_ext);
			if (ret) {
				ath11k_warn(ab, "failed to parse tlv %d\n", ret);
				return ret;
			}

			svc_rdy_ext->mac_phy_done = true;
		} else if (!svc_rdy_ext->ext_hal_reg_done) {
			ret = ath11k_wmi_tlv_ext_hal_reg_caps(ab, len, ptr,
							      svc_rdy_ext);
			if (ret)
				return ret;

			svc_rdy_ext->ext_hal_reg_done = true;
		} else if (!svc_rdy_ext->mac_phy_chainmask_combo_done) {
			svc_rdy_ext->n_mac_phy_chainmask_combo = 0;
			ret = ath11k_wmi_tlv_iter(ab, ptr, len,
						  ath11k_wmi_tlv_mac_phy_chainmask_combo_parse,
						  svc_rdy_ext);
			if (ret) {
				ath11k_warn(ab, "failed to parse chainmask combo tlv %d\n", ret);
				return ret;
			}
			svc_rdy_ext->mac_phy_chainmask_combo_done = true;
		} else if (!svc_rdy_ext->mac_phy_chainmask_cap_done) {
			ret = ath11k_wmi_tlv_mac_phy_chainmask_caps(ab, len, ptr, svc_rdy_ext);
			if (ret) {
				ath11k_warn(ab, "failed to parse chainmask caps tlv %d\n", ret);
				return ret;
			}
			svc_rdy_ext->mac_phy_chainmask_cap_done = true;
		} else if (!svc_rdy_ext->oem_dma_ring_cap_done) {
			svc_rdy_ext->oem_dma_ring_cap_done = true;
		} else if (!svc_rdy_ext->dma_ring_cap_done) {
			ret = ath11k_wmi_tlv_dma_ring_caps(ab, len, ptr,
							   &svc_rdy_ext->dma_caps_parse);
			if (ret)
				return ret;

			svc_rdy_ext->dma_ring_cap_done = true;
		}
		break;

	default:
		break;
	}
	return 0;
}

static int ath11k_service_ready_ext_event(struct ath11k_base *ab,
					  struct sk_buff *skb)
{
	struct wmi_tlv_svc_rdy_ext_parse svc_rdy_ext = { };
	int ret;

	ret = ath11k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath11k_wmi_tlv_svc_rdy_ext_parse,
				  &svc_rdy_ext);
	if (ret) {
		ath11k_warn(ab, "failed to parse tlv %d\n", ret);
		goto err;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event service ready ext");

	if (!test_bit(WMI_TLV_SERVICE_EXT2_MSG, ab->wmi_ab.svc_map))
		complete(&ab->wmi_ab.service_ready);

	kfree(svc_rdy_ext.mac_phy_caps);
	ath11k_wmi_free_chainmask_caps(&svc_rdy_ext);
	return 0;

err:
	ath11k_wmi_free_dbring_caps(ab);
	return ret;
}

static int ath11k_wmi_tlv_svc_rdy_ext2_parse(struct ath11k_base *ab,
					     u16 tag, u16 len,
					     const void *ptr, void *data)
{
	struct wmi_tlv_svc_rdy_ext2_parse *parse = data;
	int ret;

	switch (tag) {
	case WMI_TAG_ARRAY_STRUCT:
		if (!parse->dma_ring_cap_done) {
			ret = ath11k_wmi_tlv_dma_ring_caps(ab, len, ptr,
							   &parse->dma_caps_parse);
			if (ret)
				return ret;

			parse->dma_ring_cap_done = true;
		}
		break;
	default:
		break;
	}

	return 0;
}

static int ath11k_service_ready_ext2_event(struct ath11k_base *ab,
					   struct sk_buff *skb)
{
	struct wmi_tlv_svc_rdy_ext2_parse svc_rdy_ext2 = { };
	int ret;

	ret = ath11k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath11k_wmi_tlv_svc_rdy_ext2_parse,
				  &svc_rdy_ext2);
	if (ret) {
		ath11k_warn(ab, "failed to parse ext2 event tlv %d\n", ret);
		goto err;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event service ready ext2");

	complete(&ab->wmi_ab.service_ready);

	return 0;

err:
	ath11k_wmi_free_dbring_caps(ab);
	return ret;
}

static int ath11k_pull_vdev_start_resp_tlv(struct ath11k_base *ab, struct sk_buff *skb,
					   struct wmi_vdev_start_resp_event *vdev_rsp)
{
	const void **tb;
	const struct wmi_vdev_start_resp_event *ev;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_VDEV_START_RESPONSE_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch vdev start resp ev");
		kfree(tb);
		return -EPROTO;
	}

	memset(vdev_rsp, 0, sizeof(*vdev_rsp));

	vdev_rsp->vdev_id = ev->vdev_id;
	vdev_rsp->requestor_id = ev->requestor_id;
	vdev_rsp->resp_type = ev->resp_type;
	vdev_rsp->status = ev->status;
	vdev_rsp->chain_mask = ev->chain_mask;
	vdev_rsp->smps_mode = ev->smps_mode;
	vdev_rsp->mac_id = ev->mac_id;
	vdev_rsp->cfgd_tx_streams = ev->cfgd_tx_streams;
	vdev_rsp->cfgd_rx_streams = ev->cfgd_rx_streams;
	vdev_rsp->max_allowed_tx_power = ev->max_allowed_tx_power;

	kfree(tb);
	return 0;
}

static void ath11k_print_reg_rule(struct ath11k_base *ab, const char *band,
				  u32 num_reg_rules,
				  struct cur_reg_rule *reg_rule_ptr)
{
	struct cur_reg_rule *reg_rule = reg_rule_ptr;
	u32 count;

	ath11k_dbg(ab, ATH11K_DBG_WMI, "number of reg rules in %s band: %d\n",
		   band, num_reg_rules);

	for (count = 0; count < num_reg_rules; count++) {
		ath11k_dbg(ab, ATH11K_DBG_WMI,
			   "reg rule %d: (%d - %d @ %d) (%d, %d) (FLAGS %d)\n",
			   count + 1, reg_rule->start_freq, reg_rule->end_freq,
			   reg_rule->max_bw, reg_rule->ant_gain,
			   reg_rule->reg_power, reg_rule->flags);
		reg_rule++;
	}
}

static struct cur_reg_rule
*create_reg_rules_from_wmi(u32 num_reg_rules,
			   struct wmi_regulatory_rule_struct *wmi_reg_rule)
{
	struct cur_reg_rule *reg_rule_ptr;
	u32 count;

	reg_rule_ptr = kcalloc(num_reg_rules, sizeof(*reg_rule_ptr),
			       GFP_ATOMIC);

	if (!reg_rule_ptr)
		return NULL;

	for (count = 0; count < num_reg_rules; count++) {
		reg_rule_ptr[count].start_freq =
			FIELD_GET(REG_RULE_START_FREQ,
				  wmi_reg_rule[count].freq_info);
		reg_rule_ptr[count].end_freq =
			FIELD_GET(REG_RULE_END_FREQ,
				  wmi_reg_rule[count].freq_info);
		reg_rule_ptr[count].max_bw =
			FIELD_GET(REG_RULE_MAX_BW,
				  wmi_reg_rule[count].bw_pwr_info);
		reg_rule_ptr[count].reg_power =
			FIELD_GET(REG_RULE_REG_PWR,
				  wmi_reg_rule[count].bw_pwr_info);
		reg_rule_ptr[count].ant_gain =
			FIELD_GET(REG_RULE_ANT_GAIN,
				  wmi_reg_rule[count].bw_pwr_info);
		reg_rule_ptr[count].flags =
			FIELD_GET(REG_RULE_FLAGS,
				  wmi_reg_rule[count].flag_info);
	}

	return reg_rule_ptr;
}

static int ath11k_pull_reg_chan_list_update_ev(struct ath11k_base *ab,
					       struct sk_buff *skb,
					       struct cur_regulatory_info *reg_info)
{
	const void **tb;
	const struct wmi_reg_chan_list_cc_event *chan_list_event_hdr;
	struct wmi_regulatory_rule_struct *wmi_reg_rule;
	u32 num_2ghz_reg_rules, num_5ghz_reg_rules;
	int ret;

	ath11k_dbg(ab, ATH11K_DBG_WMI, "processing regulatory channel list\n");

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	chan_list_event_hdr = tb[WMI_TAG_REG_CHAN_LIST_CC_EVENT];
	if (!chan_list_event_hdr) {
		ath11k_warn(ab, "failed to fetch reg chan list update ev\n");
		kfree(tb);
		return -EPROTO;
	}

	reg_info->num_2ghz_reg_rules = chan_list_event_hdr->num_2ghz_reg_rules;
	reg_info->num_5ghz_reg_rules = chan_list_event_hdr->num_5ghz_reg_rules;

	if (!(reg_info->num_2ghz_reg_rules + reg_info->num_5ghz_reg_rules)) {
		ath11k_warn(ab, "No regulatory rules available in the event info\n");
		kfree(tb);
		return -EINVAL;
	}

	memcpy(reg_info->alpha2, &chan_list_event_hdr->alpha2,
	       REG_ALPHA2_LEN);
	reg_info->dfs_region = chan_list_event_hdr->dfs_region;
	reg_info->phybitmap = chan_list_event_hdr->phybitmap;
	reg_info->num_phy = chan_list_event_hdr->num_phy;
	reg_info->phy_id = chan_list_event_hdr->phy_id;
	reg_info->ctry_code = chan_list_event_hdr->country_id;
	reg_info->reg_dmn_pair = chan_list_event_hdr->domain_code;

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "status_code %s",
		   ath11k_cc_status_to_str(reg_info->status_code));

	reg_info->status_code =
		ath11k_wmi_cc_setting_code_to_reg(chan_list_event_hdr->status_code);

	reg_info->is_ext_reg_event = false;

	reg_info->min_bw_2ghz = chan_list_event_hdr->min_bw_2ghz;
	reg_info->max_bw_2ghz = chan_list_event_hdr->max_bw_2ghz;
	reg_info->min_bw_5ghz = chan_list_event_hdr->min_bw_5ghz;
	reg_info->max_bw_5ghz = chan_list_event_hdr->max_bw_5ghz;

	num_2ghz_reg_rules = reg_info->num_2ghz_reg_rules;
	num_5ghz_reg_rules = reg_info->num_5ghz_reg_rules;

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "cc %s dsf %d BW: min_2ghz %d max_2ghz %d min_5ghz %d max_5ghz %d",
		   reg_info->alpha2, reg_info->dfs_region,
		   reg_info->min_bw_2ghz, reg_info->max_bw_2ghz,
		   reg_info->min_bw_5ghz, reg_info->max_bw_5ghz);

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "num_2ghz_reg_rules %d num_5ghz_reg_rules %d",
		   num_2ghz_reg_rules, num_5ghz_reg_rules);

	wmi_reg_rule =
		(struct wmi_regulatory_rule_struct *)((u8 *)chan_list_event_hdr
						+ sizeof(*chan_list_event_hdr)
						+ sizeof(struct wmi_tlv));

	if (num_2ghz_reg_rules) {
		reg_info->reg_rules_2ghz_ptr =
				create_reg_rules_from_wmi(num_2ghz_reg_rules,
							  wmi_reg_rule);
		if (!reg_info->reg_rules_2ghz_ptr) {
			kfree(tb);
			ath11k_warn(ab, "Unable to Allocate memory for 2 GHz rules\n");
			return -ENOMEM;
		}

		ath11k_print_reg_rule(ab, "2 GHz",
				      num_2ghz_reg_rules,
				      reg_info->reg_rules_2ghz_ptr);
	}

	if (num_5ghz_reg_rules) {
		wmi_reg_rule += num_2ghz_reg_rules;
		reg_info->reg_rules_5ghz_ptr =
				create_reg_rules_from_wmi(num_5ghz_reg_rules,
							  wmi_reg_rule);
		if (!reg_info->reg_rules_5ghz_ptr) {
			kfree(tb);
			ath11k_warn(ab, "Unable to Allocate memory for 5 GHz rules\n");
			return -ENOMEM;
		}

		ath11k_print_reg_rule(ab, "5 GHz",
				      num_5ghz_reg_rules,
				      reg_info->reg_rules_5ghz_ptr);
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "processed regulatory channel list\n");

	kfree(tb);
	return 0;
}

static struct cur_reg_rule
*create_ext_reg_rules_from_wmi(u32 num_reg_rules,
			       struct wmi_regulatory_ext_rule *wmi_reg_rule)
{
	struct cur_reg_rule *reg_rule_ptr;
	u32 count;

	reg_rule_ptr =  kcalloc(num_reg_rules, sizeof(*reg_rule_ptr), GFP_ATOMIC);

	if (!reg_rule_ptr)
		return NULL;

	for (count = 0; count < num_reg_rules; count++) {
		reg_rule_ptr[count].start_freq =
			u32_get_bits(wmi_reg_rule[count].freq_info,
				     REG_RULE_START_FREQ);
		reg_rule_ptr[count].end_freq =
			u32_get_bits(wmi_reg_rule[count].freq_info,
				     REG_RULE_END_FREQ);
		reg_rule_ptr[count].max_bw =
			u32_get_bits(wmi_reg_rule[count].bw_pwr_info,
				     REG_RULE_MAX_BW);
		reg_rule_ptr[count].reg_power =
			u32_get_bits(wmi_reg_rule[count].bw_pwr_info,
				     REG_RULE_REG_PWR);
		reg_rule_ptr[count].ant_gain =
			u32_get_bits(wmi_reg_rule[count].bw_pwr_info,
				     REG_RULE_ANT_GAIN);
		reg_rule_ptr[count].flags =
			u32_get_bits(wmi_reg_rule[count].flag_info,
				     REG_RULE_FLAGS);
		reg_rule_ptr[count].psd_flag =
			u32_get_bits(wmi_reg_rule[count].psd_power_info,
				     REG_RULE_PSD_INFO);
		reg_rule_ptr[count].psd_eirp =
			u32_get_bits(wmi_reg_rule[count].psd_power_info,
				     REG_RULE_PSD_EIRP);
	}

	return reg_rule_ptr;
}

static u8
ath11k_invalid_5ghz_reg_ext_rules_from_wmi(u32 num_reg_rules,
					   const struct wmi_regulatory_ext_rule *rule)
{
	u8 num_invalid_5ghz_rules = 0;
	u32 count, start_freq;

	for (count = 0; count < num_reg_rules; count++) {
		start_freq = u32_get_bits(rule[count].freq_info,
					  REG_RULE_START_FREQ);

		if (start_freq >= ATH11K_MIN_6G_FREQ)
			num_invalid_5ghz_rules++;
	}

	return num_invalid_5ghz_rules;
}

static int ath11k_pull_reg_chan_list_ext_update_ev(struct ath11k_base *ab,
						   struct sk_buff *skb,
						   struct cur_regulatory_info *reg_info)
{
	const void **tb;
	const struct wmi_reg_chan_list_cc_ext_event *ev;
	struct wmi_regulatory_ext_rule *ext_wmi_reg_rule;
	u32 num_2ghz_reg_rules, num_5ghz_reg_rules;
	u32 num_6ghz_reg_rules_ap[WMI_REG_CURRENT_MAX_AP_TYPE];
	u32 num_6ghz_client[WMI_REG_CURRENT_MAX_AP_TYPE][WMI_REG_MAX_CLIENT_TYPE];
	u32 total_reg_rules = 0;
	int ret, i, j, num_invalid_5ghz_ext_rules = 0;

	ath11k_dbg(ab, ATH11K_DBG_WMI, "processing regulatory ext channel list\n");

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_REG_CHAN_LIST_CC_EXT_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch reg chan list ext update ev\n");
		kfree(tb);
		return -EPROTO;
	}

	reg_info->num_2ghz_reg_rules = ev->num_2ghz_reg_rules;
	reg_info->num_5ghz_reg_rules = ev->num_5ghz_reg_rules;
	reg_info->num_6ghz_rules_ap[WMI_REG_INDOOR_AP] =
			ev->num_6ghz_reg_rules_ap_lpi;
	reg_info->num_6ghz_rules_ap[WMI_REG_STANDARD_POWER_AP] =
			ev->num_6ghz_reg_rules_ap_sp;
	reg_info->num_6ghz_rules_ap[WMI_REG_VERY_LOW_POWER_AP] =
			ev->num_6ghz_reg_rules_ap_vlp;

	for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
		reg_info->num_6ghz_rules_client[WMI_REG_INDOOR_AP][i] =
			ev->num_6ghz_reg_rules_client_lpi[i];
		reg_info->num_6ghz_rules_client[WMI_REG_STANDARD_POWER_AP][i] =
			ev->num_6ghz_reg_rules_client_sp[i];
		reg_info->num_6ghz_rules_client[WMI_REG_VERY_LOW_POWER_AP][i] =
			ev->num_6ghz_reg_rules_client_vlp[i];
	}

	num_2ghz_reg_rules = reg_info->num_2ghz_reg_rules;
	num_5ghz_reg_rules = reg_info->num_5ghz_reg_rules;

	total_reg_rules += num_2ghz_reg_rules;
	total_reg_rules += num_5ghz_reg_rules;

	if ((num_2ghz_reg_rules > MAX_REG_RULES) ||
	    (num_5ghz_reg_rules > MAX_REG_RULES)) {
		ath11k_warn(ab, "Num reg rules for 2.4 GHz/5 GHz exceeds max limit (num_2ghz_reg_rules: %d num_5ghz_reg_rules: %d max_rules: %d)\n",
			    num_2ghz_reg_rules, num_5ghz_reg_rules, MAX_REG_RULES);
		kfree(tb);
		return -EINVAL;
	}

	for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++) {
		num_6ghz_reg_rules_ap[i] = reg_info->num_6ghz_rules_ap[i];

		if (num_6ghz_reg_rules_ap[i] > MAX_6GHZ_REG_RULES) {
			ath11k_warn(ab, "Num 6 GHz reg rules for AP mode(%d) exceeds max limit (num_6ghz_reg_rules_ap: %d, max_rules: %d)\n",
				    i, num_6ghz_reg_rules_ap[i], MAX_6GHZ_REG_RULES);
			kfree(tb);
			return -EINVAL;
		}

		total_reg_rules += num_6ghz_reg_rules_ap[i];
	}

	for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
		num_6ghz_client[WMI_REG_INDOOR_AP][i] =
			reg_info->num_6ghz_rules_client[WMI_REG_INDOOR_AP][i];
		total_reg_rules += num_6ghz_client[WMI_REG_INDOOR_AP][i];

		num_6ghz_client[WMI_REG_STANDARD_POWER_AP][i] =
			reg_info->num_6ghz_rules_client[WMI_REG_STANDARD_POWER_AP][i];
		total_reg_rules += num_6ghz_client[WMI_REG_STANDARD_POWER_AP][i];

		num_6ghz_client[WMI_REG_VERY_LOW_POWER_AP][i] =
			reg_info->num_6ghz_rules_client[WMI_REG_VERY_LOW_POWER_AP][i];
		total_reg_rules += num_6ghz_client[WMI_REG_VERY_LOW_POWER_AP][i];

		if ((num_6ghz_client[WMI_REG_INDOOR_AP][i] > MAX_6GHZ_REG_RULES) ||
		    (num_6ghz_client[WMI_REG_STANDARD_POWER_AP][i] >
							     MAX_6GHZ_REG_RULES) ||
		    (num_6ghz_client[WMI_REG_VERY_LOW_POWER_AP][i] >
							     MAX_6GHZ_REG_RULES)) {
			ath11k_warn(ab,
				    "Num 6 GHz client reg rules exceeds max limit, for client(type: %d)\n",
				    i);
			kfree(tb);
			return -EINVAL;
		}
	}

	if (!total_reg_rules) {
		ath11k_warn(ab, "No reg rules available\n");
		kfree(tb);
		return -EINVAL;
	}

	memcpy(reg_info->alpha2, &ev->alpha2, REG_ALPHA2_LEN);

	reg_info->dfs_region = ev->dfs_region;
	reg_info->phybitmap = ev->phybitmap;
	reg_info->num_phy = ev->num_phy;
	reg_info->phy_id = ev->phy_id;
	reg_info->ctry_code = ev->country_id;
	reg_info->reg_dmn_pair = ev->domain_code;

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "status_code %s",
		   ath11k_cc_status_to_str(reg_info->status_code));

	reg_info->status_code =
		ath11k_wmi_cc_setting_code_to_reg(ev->status_code);

	reg_info->is_ext_reg_event = true;

	reg_info->min_bw_2ghz = ev->min_bw_2ghz;
	reg_info->max_bw_2ghz = ev->max_bw_2ghz;
	reg_info->min_bw_5ghz = ev->min_bw_5ghz;
	reg_info->max_bw_5ghz = ev->max_bw_5ghz;

	reg_info->min_bw_6ghz_ap[WMI_REG_INDOOR_AP] =
			ev->min_bw_6ghz_ap_lpi;
	reg_info->max_bw_6ghz_ap[WMI_REG_INDOOR_AP] =
			ev->max_bw_6ghz_ap_lpi;
	reg_info->min_bw_6ghz_ap[WMI_REG_STANDARD_POWER_AP] =
			ev->min_bw_6ghz_ap_sp;
	reg_info->max_bw_6ghz_ap[WMI_REG_STANDARD_POWER_AP] =
			ev->max_bw_6ghz_ap_sp;
	reg_info->min_bw_6ghz_ap[WMI_REG_VERY_LOW_POWER_AP] =
			ev->min_bw_6ghz_ap_vlp;
	reg_info->max_bw_6ghz_ap[WMI_REG_VERY_LOW_POWER_AP] =
			ev->max_bw_6ghz_ap_vlp;

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "6 GHz AP BW: LPI (%d - %d), SP (%d - %d), VLP (%d - %d)\n",
		   reg_info->min_bw_6ghz_ap[WMI_REG_INDOOR_AP],
		   reg_info->max_bw_6ghz_ap[WMI_REG_INDOOR_AP],
		   reg_info->min_bw_6ghz_ap[WMI_REG_STANDARD_POWER_AP],
		   reg_info->max_bw_6ghz_ap[WMI_REG_STANDARD_POWER_AP],
		   reg_info->min_bw_6ghz_ap[WMI_REG_VERY_LOW_POWER_AP],
		   reg_info->max_bw_6ghz_ap[WMI_REG_VERY_LOW_POWER_AP]);

	for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
		reg_info->min_bw_6ghz_client[WMI_REG_INDOOR_AP][i] =
				ev->min_bw_6ghz_client_lpi[i];
		reg_info->max_bw_6ghz_client[WMI_REG_INDOOR_AP][i] =
				ev->max_bw_6ghz_client_lpi[i];
		reg_info->min_bw_6ghz_client[WMI_REG_STANDARD_POWER_AP][i] =
				ev->min_bw_6ghz_client_sp[i];
		reg_info->max_bw_6ghz_client[WMI_REG_STANDARD_POWER_AP][i] =
				ev->max_bw_6ghz_client_sp[i];
		reg_info->min_bw_6ghz_client[WMI_REG_VERY_LOW_POWER_AP][i] =
				ev->min_bw_6ghz_client_vlp[i];
		reg_info->max_bw_6ghz_client[WMI_REG_VERY_LOW_POWER_AP][i] =
				ev->max_bw_6ghz_client_vlp[i];

		ath11k_dbg(ab, ATH11K_DBG_WMI,
			   "6 GHz %s BW: LPI (%d - %d), SP (%d - %d), VLP (%d - %d)\n",
			   ath11k_6ghz_client_type_to_str(i),
			   reg_info->min_bw_6ghz_client[WMI_REG_INDOOR_AP][i],
			   reg_info->max_bw_6ghz_client[WMI_REG_INDOOR_AP][i],
			   reg_info->min_bw_6ghz_client[WMI_REG_STANDARD_POWER_AP][i],
			   reg_info->max_bw_6ghz_client[WMI_REG_STANDARD_POWER_AP][i],
			   reg_info->min_bw_6ghz_client[WMI_REG_VERY_LOW_POWER_AP][i],
			   reg_info->max_bw_6ghz_client[WMI_REG_VERY_LOW_POWER_AP][i]);
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "cc_ext %s dfs %d BW: min_2ghz %d max_2ghz %d min_5ghz %d max_5ghz %d phy_bitmap 0x%x",
		   reg_info->alpha2, reg_info->dfs_region,
		   reg_info->min_bw_2ghz, reg_info->max_bw_2ghz,
		   reg_info->min_bw_5ghz, reg_info->max_bw_5ghz,
		   reg_info->phybitmap);

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "num_2ghz_reg_rules %d num_5ghz_reg_rules %d",
		   num_2ghz_reg_rules, num_5ghz_reg_rules);

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "num_6ghz_reg_rules_ap_lpi: %d num_6ghz_reg_rules_ap_sp: %d num_6ghz_reg_rules_ap_vlp: %d",
		   num_6ghz_reg_rules_ap[WMI_REG_INDOOR_AP],
		   num_6ghz_reg_rules_ap[WMI_REG_STANDARD_POWER_AP],
		   num_6ghz_reg_rules_ap[WMI_REG_VERY_LOW_POWER_AP]);

	j = WMI_REG_DEFAULT_CLIENT;
	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "6 GHz Regular client: num_6ghz_reg_rules_lpi: %d num_6ghz_reg_rules_sp: %d num_6ghz_reg_rules_vlp: %d",
		   num_6ghz_client[WMI_REG_INDOOR_AP][j],
		   num_6ghz_client[WMI_REG_STANDARD_POWER_AP][j],
		   num_6ghz_client[WMI_REG_VERY_LOW_POWER_AP][j]);

	j = WMI_REG_SUBORDINATE_CLIENT;
	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "6 GHz Subordinate client: num_6ghz_reg_rules_lpi: %d num_6ghz_reg_rules_sp: %d num_6ghz_reg_rules_vlp: %d",
		   num_6ghz_client[WMI_REG_INDOOR_AP][j],
		   num_6ghz_client[WMI_REG_STANDARD_POWER_AP][j],
		   num_6ghz_client[WMI_REG_VERY_LOW_POWER_AP][j]);

	ext_wmi_reg_rule =
		(struct wmi_regulatory_ext_rule *)((u8 *)ev + sizeof(*ev) +
						   sizeof(struct wmi_tlv));
	if (num_2ghz_reg_rules) {
		reg_info->reg_rules_2ghz_ptr =
			create_ext_reg_rules_from_wmi(num_2ghz_reg_rules,
						      ext_wmi_reg_rule);

		if (!reg_info->reg_rules_2ghz_ptr) {
			kfree(tb);
			ath11k_warn(ab, "Unable to Allocate memory for 2 GHz rules\n");
			return -ENOMEM;
		}

		ath11k_print_reg_rule(ab, "2 GHz",
				      num_2ghz_reg_rules,
				      reg_info->reg_rules_2ghz_ptr);
	}

	ext_wmi_reg_rule += num_2ghz_reg_rules;

	/* Firmware might include 6 GHz reg rule in 5 GHz rule list
	 * for few countries along with separate 6 GHz rule.
	 * Having same 6 GHz reg rule in 5 GHz and 6 GHz rules list
	 * causes intersect check to be true, and same rules will be
	 * shown multiple times in iw cmd.
	 * Hence, avoid parsing 6 GHz rule from 5 GHz reg rule list
	 */
	num_invalid_5ghz_ext_rules =
		ath11k_invalid_5ghz_reg_ext_rules_from_wmi(num_5ghz_reg_rules,
							   ext_wmi_reg_rule);

	if (num_invalid_5ghz_ext_rules) {
		ath11k_dbg(ab, ATH11K_DBG_WMI,
			   "CC: %s 5 GHz reg rules number %d from fw, %d number of invalid 5 GHz rules",
			   reg_info->alpha2, reg_info->num_5ghz_reg_rules,
			   num_invalid_5ghz_ext_rules);

		num_5ghz_reg_rules = num_5ghz_reg_rules - num_invalid_5ghz_ext_rules;
		reg_info->num_5ghz_reg_rules = num_5ghz_reg_rules;
	}

	if (num_5ghz_reg_rules) {
		reg_info->reg_rules_5ghz_ptr =
			create_ext_reg_rules_from_wmi(num_5ghz_reg_rules,
						      ext_wmi_reg_rule);

		if (!reg_info->reg_rules_5ghz_ptr) {
			kfree(tb);
			ath11k_warn(ab, "Unable to Allocate memory for 5 GHz rules\n");
			return -ENOMEM;
		}

		ath11k_print_reg_rule(ab, "5 GHz",
				      num_5ghz_reg_rules,
				      reg_info->reg_rules_5ghz_ptr);
	}

	/* We have adjusted the number of 5 GHz reg rules above. But still those
	 * many rules needs to be adjusted in ext_wmi_reg_rule.
	 *
	 * NOTE: num_invalid_5ghz_ext_rules will be 0 for rest other cases.
	 */
	ext_wmi_reg_rule += (num_5ghz_reg_rules + num_invalid_5ghz_ext_rules);

	for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++) {
		reg_info->reg_rules_6ghz_ap_ptr[i] =
			create_ext_reg_rules_from_wmi(num_6ghz_reg_rules_ap[i],
						      ext_wmi_reg_rule);

		if (!reg_info->reg_rules_6ghz_ap_ptr[i]) {
			kfree(tb);
			ath11k_warn(ab, "Unable to Allocate memory for 6 GHz AP rules\n");
			return -ENOMEM;
		}

		ath11k_print_reg_rule(ab, ath11k_6ghz_ap_type_to_str(i),
				      num_6ghz_reg_rules_ap[i],
				      reg_info->reg_rules_6ghz_ap_ptr[i]);

		ext_wmi_reg_rule += num_6ghz_reg_rules_ap[i];
	}

	for (j = 0; j < WMI_REG_CURRENT_MAX_AP_TYPE; j++) {
		ath11k_dbg(ab, ATH11K_DBG_WMI,
			   "6 GHz AP type %s", ath11k_6ghz_ap_type_to_str(j));

		for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
			reg_info->reg_rules_6ghz_client_ptr[j][i] =
				create_ext_reg_rules_from_wmi(num_6ghz_client[j][i],
							      ext_wmi_reg_rule);

			if (!reg_info->reg_rules_6ghz_client_ptr[j][i]) {
				kfree(tb);
				ath11k_warn(ab, "Unable to Allocate memory for 6 GHz client rules\n");
				return -ENOMEM;
			}

			ath11k_print_reg_rule(ab,
					      ath11k_6ghz_client_type_to_str(i),
					      num_6ghz_client[j][i],
					      reg_info->reg_rules_6ghz_client_ptr[j][i]);

			ext_wmi_reg_rule += num_6ghz_client[j][i];
		}
	}

	reg_info->client_type = ev->client_type;
	reg_info->rnr_tpe_usable = ev->rnr_tpe_usable;
	reg_info->unspecified_ap_usable =
			ev->unspecified_ap_usable;
	reg_info->domain_code_6ghz_ap[WMI_REG_INDOOR_AP] =
			ev->domain_code_6ghz_ap_lpi;
	reg_info->domain_code_6ghz_ap[WMI_REG_STANDARD_POWER_AP] =
			ev->domain_code_6ghz_ap_sp;
	reg_info->domain_code_6ghz_ap[WMI_REG_VERY_LOW_POWER_AP] =
			ev->domain_code_6ghz_ap_vlp;

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "6 GHz reg info client type %s rnr_tpe_usable %d unspecified_ap_usable %d AP sub domain: lpi %s, sp %s, vlp %s\n",
		   ath11k_6ghz_client_type_to_str(reg_info->client_type),
		   reg_info->rnr_tpe_usable,
		   reg_info->unspecified_ap_usable,
		   ath11k_sub_reg_6ghz_to_str(ev->domain_code_6ghz_ap_lpi),
		   ath11k_sub_reg_6ghz_to_str(ev->domain_code_6ghz_ap_sp),
		   ath11k_sub_reg_6ghz_to_str(ev->domain_code_6ghz_ap_vlp));

	for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
		reg_info->domain_code_6ghz_client[WMI_REG_INDOOR_AP][i] =
				ev->domain_code_6ghz_client_lpi[i];
		reg_info->domain_code_6ghz_client[WMI_REG_STANDARD_POWER_AP][i] =
				ev->domain_code_6ghz_client_sp[i];
		reg_info->domain_code_6ghz_client[WMI_REG_VERY_LOW_POWER_AP][i] =
				ev->domain_code_6ghz_client_vlp[i];

		ath11k_dbg(ab, ATH11K_DBG_WMI,
			   "6 GHz client type %s client sub domain: lpi %s, sp %s, vlp %s\n",
			   ath11k_6ghz_client_type_to_str(i),
			   ath11k_sub_reg_6ghz_to_str(ev->domain_code_6ghz_client_lpi[i]),
			   ath11k_sub_reg_6ghz_to_str(ev->domain_code_6ghz_client_sp[i]),
			   ath11k_sub_reg_6ghz_to_str(ev->domain_code_6ghz_client_vlp[i])
			  );
	}

	reg_info->domain_code_6ghz_super_id = ev->domain_code_6ghz_super_id;

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "6 GHz client_type %s 6 GHz super domain %s",
		   ath11k_6ghz_client_type_to_str(reg_info->client_type),
		   ath11k_super_reg_6ghz_to_str(reg_info->domain_code_6ghz_super_id));

	ath11k_dbg(ab, ATH11K_DBG_WMI, "processed regulatory ext channel list\n");

	kfree(tb);
	return 0;
}

static int ath11k_pull_peer_del_resp_ev(struct ath11k_base *ab, struct sk_buff *skb,
					struct wmi_peer_delete_resp_event *peer_del_resp)
{
	const void **tb;
	const struct wmi_peer_delete_resp_event *ev;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PEER_DELETE_RESP_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch peer delete resp ev");
		kfree(tb);
		return -EPROTO;
	}

	memset(peer_del_resp, 0, sizeof(*peer_del_resp));

	peer_del_resp->vdev_id = ev->vdev_id;
	ether_addr_copy(peer_del_resp->peer_macaddr.addr,
			ev->peer_macaddr.addr);

	kfree(tb);
	return 0;
}

static int ath11k_pull_vdev_del_resp_ev(struct ath11k_base *ab,
					struct sk_buff *skb,
					u32 *vdev_id)
{
	const void **tb;
	const struct wmi_vdev_delete_resp_event *ev;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_VDEV_DELETE_RESP_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch vdev delete resp ev");
		kfree(tb);
		return -EPROTO;
	}

	*vdev_id = ev->vdev_id;

	kfree(tb);
	return 0;
}

static int ath11k_pull_bcn_tx_status_ev(struct ath11k_base *ab,
					struct sk_buff *skb,
					u32 *vdev_id, u32 *tx_status)
{
	const void **tb;
	const struct wmi_bcn_tx_status_event *ev;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_OFFLOAD_BCN_TX_STATUS_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch bcn tx status ev");
		kfree(tb);
		return -EPROTO;
	}

	*vdev_id   = ev->vdev_id;
	*tx_status = ev->tx_status;

	kfree(tb);
	return 0;
}

static int ath11k_pull_vdev_stopped_param_tlv(struct ath11k_base *ab, struct sk_buff *skb,
					      u32 *vdev_id)
{
	const void **tb;
	const struct wmi_vdev_stopped_event *ev;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_VDEV_STOPPED_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch vdev stop ev");
		kfree(tb);
		return -EPROTO;
	}

	*vdev_id =  ev->vdev_id;

	kfree(tb);
	return 0;
}

static int ath11k_wmi_tlv_mgmt_rx_parse(struct ath11k_base *ab,
					u16 tag, u16 len,
					const void *ptr, void *data)
{
	struct wmi_tlv_mgmt_rx_parse *parse = data;

	switch (tag) {
	case WMI_TAG_MGMT_RX_HDR:
		parse->fixed = ptr;
		break;
	case WMI_TAG_ARRAY_BYTE:
		if (!parse->frame_buf_done) {
			parse->frame_buf = ptr;
			parse->frame_buf_done = true;
		}
		break;
	}
	return 0;
}

static int ath11k_pull_mgmt_rx_params_tlv(struct ath11k_base *ab,
					  struct sk_buff *skb,
					  struct mgmt_rx_event_params *hdr)
{
	struct wmi_tlv_mgmt_rx_parse parse = { };
	const struct wmi_mgmt_rx_hdr *ev;
	const u8 *frame;
	int ret;

	ret = ath11k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath11k_wmi_tlv_mgmt_rx_parse,
				  &parse);
	if (ret) {
		ath11k_warn(ab, "failed to parse mgmt rx tlv %d\n",
			    ret);
		return ret;
	}

	ev = parse.fixed;
	frame = parse.frame_buf;

	if (!ev || !frame) {
		ath11k_warn(ab, "failed to fetch mgmt rx hdr");
		return -EPROTO;
	}

	hdr->pdev_id =  ev->pdev_id;
	hdr->chan_freq = ev->chan_freq;
	hdr->channel =  ev->channel;
	hdr->snr =  ev->snr;
	hdr->rate =  ev->rate;
	hdr->phy_mode =  ev->phy_mode;
	hdr->buf_len =  ev->buf_len;
	hdr->status =  ev->status;
	hdr->flags =  ev->flags;
	hdr->rssi =  ev->rssi;
	hdr->tsf_delta =  ev->tsf_delta;
	memcpy(hdr->rssi_ctl, ev->rssi_ctl, sizeof(hdr->rssi_ctl));

	if (skb->len < (frame - skb->data) + hdr->buf_len) {
		ath11k_warn(ab, "invalid length in mgmt rx hdr ev");
		return -EPROTO;
	}

	/* shift the sk_buff to point to `frame` */
	skb_trim(skb, 0);
	skb_put(skb, frame - skb->data);
	skb_pull(skb, frame - skb->data);
	skb_put(skb, hdr->buf_len);

	ath11k_ce_byte_swap(skb->data, hdr->buf_len);

	return 0;
}

static int wmi_process_tx_comp(struct ath11k *ar,
				    struct wmi_tx_compl_event *tx_compl_param)
{
	struct sk_buff *msdu;
	struct ieee80211_tx_info *info;
	struct ath11k_skb_cb *skb_cb;
	struct ieee80211_hdr *hdr;
	struct ieee80211_vif *vif;
	struct ath11k_vif *arvif;
	struct ath11k_mgmt_frame_stats *mgmt_stats;
	u16 frm_type;
	int num_mgmt;

	spin_lock_bh(&ar->data_lock);
	spin_lock_bh(&ar->txmgmt_idr_lock);
	msdu = idr_find(&ar->txmgmt_idr, tx_compl_param->desc_id);

	if (!msdu) {
		ath11k_warn(ar->ab, "received mgmt tx compl for invalid msdu_id: %d\n",
			    tx_compl_param->desc_id);
		spin_unlock_bh(&ar->txmgmt_idr_lock);
		spin_unlock_bh(&ar->data_lock);
		return -ENOENT;
	}

	idr_remove(&ar->txmgmt_idr, tx_compl_param->desc_id);
	spin_unlock_bh(&ar->txmgmt_idr_lock);

	skb_cb = ATH11K_SKB_CB(msdu);
	dma_unmap_single(ar->ab->dev, skb_cb->paddr, msdu->len, DMA_TO_DEVICE);
	hdr = (struct ieee80211_hdr *)msdu->data;

	if (ieee80211_is_mgmt(hdr->frame_control)) {
		frm_type = FIELD_GET(IEEE80211_FCTL_STYPE, hdr->frame_control);
		vif = skb_cb->vif;

		if (!vif) {
			ath11k_warn(ar->ab, "failed to find vif to update txcompl mgmt stats\n");
			goto skip_mgmt_stats;
		}

		arvif = ath11k_vif_to_arvif(vif);
		mgmt_stats = &arvif->mgmt_stats;

		if (!tx_compl_param->status)
			mgmt_stats->tx_compl_succ[frm_type]++;
		else
			mgmt_stats->tx_compl_fail[frm_type]++;
	}

skip_mgmt_stats:
	spin_unlock_bh(&ar->data_lock);

	info = IEEE80211_SKB_CB(msdu);
	if ((!(info->flags & IEEE80211_TX_CTL_NO_ACK)) &&
	    !tx_compl_param->status) {
		info->flags |= IEEE80211_TX_STAT_ACK;
		if (test_bit(WMI_TLV_SERVICE_TX_DATA_MGMT_ACK_RSSI,
			     ar->ab->wmi_ab.svc_map))
			info->status.ack_signal = tx_compl_param->ack_rssi;
	}

	/* dont update rates in this path, qos null data tx completions also can
	 * take this path in case of nss offload and can update invalid rates.
	 */
	info->status.rates[0].idx = -1;

	ieee80211_tx_status_irqsafe(ar->hw, msdu);

	num_mgmt = atomic_dec_if_positive(&ar->num_pending_mgmt_tx);

	/* WARN when we received this event without doing any mgmt tx */
	if (num_mgmt < 0)
		WARN_ON_ONCE(1);

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "mgmt tx comp pending %d desc id %d\n",
		   num_mgmt, tx_compl_param->desc_id);

	if (!num_mgmt)
		wake_up(&ar->txmgmt_empty_waitq);

	return 0;
}

static int ath11k_pull_tx_compl_param_tlv(struct ath11k_base *ab,
					  struct sk_buff *skb,
					  struct wmi_tx_compl_event *param,
					  int event_id)
{
	const void **tb;
	const struct wmi_tx_compl_event *ev;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[event_id];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch mgmt tx compl ev");
		kfree(tb);
		return -EPROTO;
	}

	param->pdev_id = ev->pdev_id;
	param->desc_id = ev->desc_id;
	param->status = ev->status;
	param->ack_rssi = ev->ack_rssi;

	kfree(tb);
	return 0;
}

static void ath11k_wmi_event_scan_started(struct ath11k *ar)
{
	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH11K_SCAN_IDLE:
	case ATH11K_SCAN_RUNNING:
	case ATH11K_SCAN_ABORTING:
		ath11k_warn(ar->ab, "received scan started event in an invalid scan state: %s (%d)\n",
			    ath11k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH11K_SCAN_STARTING:
		ar->scan.state = ATH11K_SCAN_RUNNING;
		if (ar->scan.is_roc)
			ieee80211_ready_on_channel(ar->hw);
		complete(&ar->scan.started);
		break;
	}
}

static void ath11k_wmi_event_scan_start_failed(struct ath11k *ar)
{
	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH11K_SCAN_IDLE:
	case ATH11K_SCAN_RUNNING:
	case ATH11K_SCAN_ABORTING:
		ath11k_warn(ar->ab, "received scan start failed event in an invalid scan state: %s (%d)\n",
			    ath11k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH11K_SCAN_STARTING:
		complete(&ar->scan.started);
		__ath11k_mac_scan_finish(ar);
		break;
	}
}

static void ath11k_wmi_event_scan_completed(struct ath11k *ar)
{
	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH11K_SCAN_IDLE:
	case ATH11K_SCAN_STARTING:
		/* One suspected reason scan can be completed while starting is
		 * if firmware fails to deliver all scan events to the host,
		 * e.g. when transport pipe is full. This has been observed
		 * with spectral scan phyerr events starving wmi transport
		 * pipe. In such case the "scan completed" event should be (and
		 * is) ignored by the host as it may be just firmware's scan
		 * state machine recovering.
		 */
		ath11k_warn(ar->ab, "received scan completed event in an invalid scan state: %s (%d)\n",
			    ath11k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH11K_SCAN_RUNNING:
	case ATH11K_SCAN_ABORTING:
		__ath11k_mac_scan_finish(ar);
		break;
	}
}

static void ath11k_wmi_event_scan_bss_chan(struct ath11k *ar)
{
	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH11K_SCAN_IDLE:
	case ATH11K_SCAN_STARTING:
		ath11k_warn(ar->ab, "received scan bss chan event in an invalid scan state: %s (%d)\n",
			    ath11k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH11K_SCAN_RUNNING:
	case ATH11K_SCAN_ABORTING:
		ar->scan_channel = NULL;
		break;
	}
}

static void ath11k_wmi_event_scan_foreign_chan(struct ath11k *ar, u32 freq)
{
	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH11K_SCAN_IDLE:
	case ATH11K_SCAN_STARTING:
		ath11k_warn(ar->ab, "received scan foreign chan event in an invalid scan state: %s (%d)\n",
			    ath11k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH11K_SCAN_RUNNING:
	case ATH11K_SCAN_ABORTING:
		ar->scan_channel = ieee80211_get_channel(ar->hw->wiphy, freq);
		if (ar->scan.is_roc && ar->scan.roc_freq == freq)
			complete(&ar->scan.on_channel);
		break;
	}
}

static const char *
ath11k_wmi_event_scan_type_str(enum wmi_scan_event_type type,
			       enum wmi_scan_completion_reason reason)
{
	switch (type) {
	case WMI_SCAN_EVENT_STARTED:
		return "started";
	case WMI_SCAN_EVENT_COMPLETED:
		switch (reason) {
		case WMI_SCAN_REASON_COMPLETED:
			return "completed";
		case WMI_SCAN_REASON_CANCELLED:
			return "completed [cancelled]";
		case WMI_SCAN_REASON_PREEMPTED:
			return "completed [preempted]";
		case WMI_SCAN_REASON_TIMEDOUT:
			return "completed [timedout]";
		case WMI_SCAN_REASON_INTERNAL_FAILURE:
			return "completed [internal err]";
		case WMI_SCAN_REASON_MAX:
			break;
		}
		return "completed [unknown]";
	case WMI_SCAN_EVENT_BSS_CHANNEL:
		return "bss channel";
	case WMI_SCAN_EVENT_FOREIGN_CHAN:
		return "foreign channel";
	case WMI_SCAN_EVENT_DEQUEUED:
		return "dequeued";
	case WMI_SCAN_EVENT_PREEMPTED:
		return "preempted";
	case WMI_SCAN_EVENT_START_FAILED:
		return "start failed";
	case WMI_SCAN_EVENT_RESTARTED:
		return "restarted";
	case WMI_SCAN_EVENT_FOREIGN_CHAN_EXIT:
		return "foreign channel exit";
	default:
		return "unknown";
	}
}

static int ath11k_pull_scan_ev(struct ath11k_base *ab, struct sk_buff *skb,
			       struct wmi_scan_event *scan_evt_param)
{
	const void **tb;
	const struct wmi_scan_event *ev;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_SCAN_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch scan ev");
		kfree(tb);
		return -EPROTO;
	}

	scan_evt_param->event_type = ev->event_type;
	scan_evt_param->reason = ev->reason;
	scan_evt_param->channel_freq = ev->channel_freq;
	scan_evt_param->scan_req_id = ev->scan_req_id;
	scan_evt_param->scan_id = ev->scan_id;
	scan_evt_param->vdev_id = ev->vdev_id;
	scan_evt_param->tsf_timestamp = ev->tsf_timestamp;

	kfree(tb);
	return 0;
}

static int ath11k_pull_peer_sta_kickout_ev(struct ath11k_base *ab, struct sk_buff *skb,
					   struct wmi_peer_sta_kickout_arg *arg)
{
	const void **tb;
	const struct wmi_peer_sta_kickout_event *ev;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PEER_STA_KICKOUT_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch peer sta kickout ev");
		kfree(tb);
		return -EPROTO;
	}

	arg->mac_addr = ev->peer_macaddr.addr;

	kfree(tb);
	return 0;
}

static int ath11k_pull_roam_ev(struct ath11k_base *ab, struct sk_buff *skb,
			       struct wmi_roam_event *roam_ev)
{
	const void **tb;
	const struct wmi_roam_event *ev;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_ROAM_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch roam ev");
		kfree(tb);
		return -EPROTO;
	}

	roam_ev->vdev_id = ev->vdev_id;
	roam_ev->reason = ev->reason;
	roam_ev->rssi = ev->rssi;

	kfree(tb);
	return 0;
}

static int freq_to_idx(struct ath11k *ar, int freq)
{
	struct ieee80211_supported_band *sband;
	int band, ch, idx = 0;

	for (band = NL80211_BAND_2GHZ; band < NUM_NL80211_BANDS; band++) {
		sband = ar->hw->wiphy->bands[band];
		if (!sband)
			continue;

		for (ch = 0; ch < sband->n_channels; ch++, idx++)
			if (sband->channels[ch].center_freq == freq)
				goto exit;
	}

exit:
	return idx;
}

static int ath11k_pull_chan_info_ev(struct ath11k_base *ab, struct sk_buff *skb,
				    struct wmi_chan_info_event *ch_info_ev)
{
	const void **tb;
	const struct wmi_chan_info_event *ev;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_CHAN_INFO_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch chan info ev");
		kfree(tb);
		return -EPROTO;
	}

	ch_info_ev->err_code = ev->err_code;
	ch_info_ev->freq = ev->freq;
	ch_info_ev->cmd_flags = ev->cmd_flags;
	ch_info_ev->noise_floor = ev->noise_floor;
	ch_info_ev->rx_clear_count = ev->rx_clear_count;
	ch_info_ev->cycle_count = ev->cycle_count;
	ch_info_ev->chan_tx_pwr_range = ev->chan_tx_pwr_range;
	ch_info_ev->chan_tx_pwr_tp = ev->chan_tx_pwr_tp;
	ch_info_ev->rx_frame_count = ev->rx_frame_count;
	ch_info_ev->tx_frame_cnt = ev->tx_frame_cnt;
	ch_info_ev->mac_clk_mhz = ev->mac_clk_mhz;
	ch_info_ev->vdev_id = ev->vdev_id;

	kfree(tb);
	return 0;
}

static int
ath11k_pull_pdev_bss_chan_info_ev(struct ath11k_base *ab, struct sk_buff *skb,
				  struct wmi_pdev_bss_chan_info_event *bss_ch_info_ev)
{
	const void **tb;
	const struct wmi_pdev_bss_chan_info_event *ev;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PDEV_BSS_CHAN_INFO_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch pdev bss chan info ev");
		kfree(tb);
		return -EPROTO;
	}

	bss_ch_info_ev->pdev_id = ev->pdev_id;
	bss_ch_info_ev->freq = ev->freq;
	bss_ch_info_ev->noise_floor = ev->noise_floor;
	bss_ch_info_ev->rx_clear_count_low = ev->rx_clear_count_low;
	bss_ch_info_ev->rx_clear_count_high = ev->rx_clear_count_high;
	bss_ch_info_ev->cycle_count_low = ev->cycle_count_low;
	bss_ch_info_ev->cycle_count_high = ev->cycle_count_high;
	bss_ch_info_ev->tx_cycle_count_low = ev->tx_cycle_count_low;
	bss_ch_info_ev->tx_cycle_count_high = ev->tx_cycle_count_high;
	bss_ch_info_ev->rx_cycle_count_low = ev->rx_cycle_count_low;
	bss_ch_info_ev->rx_cycle_count_high = ev->rx_cycle_count_high;
	bss_ch_info_ev->rx_bss_cycle_count_low = ev->rx_bss_cycle_count_low;
	bss_ch_info_ev->rx_bss_cycle_count_high = ev->rx_bss_cycle_count_high;

	kfree(tb);
	return 0;
}

static int
ath11k_pull_vdev_install_key_compl_ev(struct ath11k_base *ab, struct sk_buff *skb,
				      struct wmi_vdev_install_key_complete_arg *arg)
{
	const void **tb;
	const struct wmi_vdev_install_key_compl_event *ev;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_VDEV_INSTALL_KEY_COMPLETE_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch vdev install key compl ev");
		kfree(tb);
		return -EPROTO;
	}

	arg->vdev_id = ev->vdev_id;
	arg->macaddr = ev->peer_macaddr.addr;
	arg->key_idx = ev->key_idx;
	arg->key_flags = ev->key_flags;
	arg->status = ev->status;

	kfree(tb);
	return 0;
}

static int ath11k_pull_peer_assoc_conf_ev(struct ath11k_base *ab, struct sk_buff *skb,
					  struct wmi_peer_assoc_conf_arg *peer_assoc_conf)
{
	const void **tb;
	const struct wmi_peer_assoc_conf_event *ev;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PEER_ASSOC_CONF_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch peer assoc conf ev");
		kfree(tb);
		return -EPROTO;
	}

	peer_assoc_conf->vdev_id = ev->vdev_id;
	peer_assoc_conf->macaddr = ev->peer_macaddr.addr;

	kfree(tb);
	return 0;
}

static int ath11k_pull_wds_addr_ev(struct ath11k_base *ab, struct sk_buff *skb,
				   struct wmi_wds_addr_arg *wds_addr_arg)
{
	const void **tb;
	const struct wmi_wds_addr_event *ev;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_WDS_ADDR_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch wds peer ev");
		kfree(tb);
		return -EPROTO;
	}

	memcpy(wds_addr_arg->event_type, ev->event_type, WMI_NUM_WDS_EVENTS);
	wds_addr_arg->vdev_id = ev->vdev_id;
	wds_addr_arg->peer_macaddr = ev->peer_macaddr.addr;
	wds_addr_arg->dst_macaddr = ev->dst_macaddr.addr;

	kfree(tb);
	return 0;
}

static void ath11k_wmi_pull_pdev_stats_base(struct ath11k *ar,
					    const struct wmi_pdev_stats_base *src,
					    struct ath11k_fw_stats_pdev *dst)
{
	dst->ch_noise_floor = src->chan_nf;
	ar->chan_noise_floor = src->chan_nf;
	dst->tx_frame_count = src->tx_frame_count;
	dst->rx_frame_count = src->rx_frame_count;
	dst->rx_clear_count = src->rx_clear_count;
	dst->cycle_count = src->cycle_count;
	dst->phy_err_count = src->phy_err_count;
	dst->chan_tx_power = src->chan_tx_pwr;
}

static void
ath11k_wmi_pull_pdev_stats_tx(const struct wmi_pdev_stats_tx *src,
			      struct ath11k_fw_stats_pdev *dst)
{
	dst->comp_queued = src->comp_queued;
	dst->comp_delivered = src->comp_delivered;
	dst->msdu_enqued = src->msdu_enqued;
	dst->mpdu_enqued = src->mpdu_enqued;
	dst->wmm_drop = src->wmm_drop;
	dst->local_enqued = src->local_enqued;
	dst->local_freed = src->local_freed;
	dst->hw_queued = src->hw_queued;
	dst->hw_reaped = src->hw_reaped;
	dst->underrun = src->underrun;
	dst->hw_paused = src->hw_paused;
	dst->tx_abort = src->tx_abort;
	dst->mpdus_requeued = src->mpdus_requeued;
	dst->tx_ko = src->tx_ko;
	dst->tx_xretry = src->tx_xretry;
	dst->data_rc = src->data_rc;
	dst->self_triggers = src->self_triggers;
	dst->sw_retry_failure = src->sw_retry_failure;
	dst->illgl_rate_phy_err = src->illgl_rate_phy_err;
	dst->pdev_cont_xretry = src->pdev_cont_xretry;
	dst->pdev_tx_timeout = src->pdev_tx_timeout;
	dst->pdev_resets = src->pdev_resets;
	dst->stateless_tid_alloc_failure = src->stateless_tid_alloc_failure;
	dst->phy_underrun = src->phy_underrun;
	dst->txop_ovf = src->txop_ovf;
	dst->seq_posted = src->seq_posted;
	dst->seq_failed_queueing = src->seq_failed_queueing;
	dst->seq_completed = src->seq_completed;
	dst->seq_restarted = src->seq_restarted;
	dst->mu_seq_posted = src->mu_seq_posted;
	dst->mpdus_sw_flush = src->mpdus_sw_flush;
	dst->mpdus_hw_filter = src->mpdus_hw_filter;
	dst->mpdus_truncated = src->mpdus_truncated;
	dst->mpdus_ack_failed = src->mpdus_ack_failed;
	dst->mpdus_expired = src->mpdus_expired;
}

static void ath11k_wmi_pull_pdev_stats_rx(const struct wmi_pdev_stats_rx *src,
					  struct ath11k_fw_stats_pdev *dst)
{
	dst->mid_ppdu_route_change = src->mid_ppdu_route_change;
	dst->status_rcvd = src->status_rcvd;
	dst->r0_frags = src->r0_frags;
	dst->r1_frags = src->r1_frags;
	dst->r2_frags = src->r2_frags;
	dst->r3_frags = src->r3_frags;
	dst->htt_msdus = src->htt_msdus;
	dst->htt_mpdus = src->htt_mpdus;
	dst->loc_msdus = src->loc_msdus;
	dst->loc_mpdus = src->loc_mpdus;
	dst->oversize_amsdu = src->oversize_amsdu;
	dst->phy_errs = src->phy_errs;
	dst->phy_err_drop = src->phy_err_drop;
	dst->mpdu_errs = src->mpdu_errs;
	dst->rx_ovfl_errs = src->rx_ovfl_errs;
}

static void
ath11k_wmi_pull_vdev_stats(const struct wmi_vdev_stats *src,
			   struct ath11k_fw_stats_vdev *dst)
{
	int i;

	dst->vdev_id = src->vdev_id;
	dst->beacon_snr = src->beacon_snr;
	dst->data_snr = src->data_snr;
	dst->num_rx_frames = src->num_rx_frames;
	dst->num_rts_fail = src->num_rts_fail;
	dst->num_rts_success = src->num_rts_success;
	dst->num_rx_err = src->num_rx_err;
	dst->num_rx_discard = src->num_rx_discard;
	dst->num_tx_not_acked = src->num_tx_not_acked;

	for (i = 0; i < ARRAY_SIZE(src->num_tx_frames); i++)
		dst->num_tx_frames[i] = src->num_tx_frames[i];

	for (i = 0; i < ARRAY_SIZE(src->num_tx_frames_retries); i++)
		dst->num_tx_frames_retries[i] = src->num_tx_frames_retries[i];

	for (i = 0; i < ARRAY_SIZE(src->num_tx_frames_failures); i++)
		dst->num_tx_frames_failures[i] = src->num_tx_frames_failures[i];

	for (i = 0; i < ARRAY_SIZE(src->tx_rate_history); i++)
		dst->tx_rate_history[i] = src->tx_rate_history[i];

	for (i = 0; i < ARRAY_SIZE(src->beacon_rssi_history); i++)
		dst->beacon_rssi_history[i] = src->beacon_rssi_history[i];
}

static void
ath11k_wmi_pull_bcn_stats(const struct wmi_bcn_stats *src,
			  struct ath11k_fw_stats_bcn *dst)
{
	dst->vdev_id = src->vdev_id;
	dst->tx_bcn_succ_cnt = src->tx_bcn_succ_cnt;
	dst->tx_bcn_outage_cnt = src->tx_bcn_outage_cnt;
}

static int ath11k_wmi_tlv_rssi_chain_parse(struct ath11k_base *ab,
					   u16 tag, u16 len,
					   const void *ptr, void *data)
{
	struct wmi_tlv_fw_stats_parse *parse = data;
	const struct wmi_stats_event *ev = parse->ev;
	struct ath11k_fw_stats *stats = parse->stats;
	struct ath11k *ar;
	struct ath11k_vif *arvif;
	struct ieee80211_sta *sta;
	struct ath11k_sta *arsta;
	const struct wmi_rssi_stats *stats_rssi = (const struct wmi_rssi_stats *)ptr;
	int j, ret = 0;

	if (tag != WMI_TAG_RSSI_STATS)
		return -EPROTO;

	rcu_read_lock();

	ar = ath11k_mac_get_ar_by_pdev_id(ab, ev->pdev_id);
	stats->stats_id = WMI_REQUEST_RSSI_PER_CHAIN_STAT;

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "stats vdev id %d mac %pM\n",
		   stats_rssi->vdev_id, stats_rssi->peer_macaddr.addr);

	arvif = ath11k_mac_get_arvif(ar, stats_rssi->vdev_id);
	if (!arvif) {
		ath11k_warn(ab, "not found vif for vdev id %d\n",
			    stats_rssi->vdev_id);
		ret = -EPROTO;
		goto exit;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "stats bssid %pM vif %p\n",
		   arvif->bssid, arvif->vif);

	sta = ieee80211_find_sta_by_ifaddr(ar->hw,
					   arvif->bssid,
					   NULL);
	if (!sta) {
		ath11k_dbg(ab, ATH11K_DBG_WMI,
			   "not found station of bssid %pM for rssi chain\n",
			   arvif->bssid);
		goto exit;
	}

	arsta = ath11k_sta_to_arsta(sta);

	BUILD_BUG_ON(ARRAY_SIZE(arsta->chain_signal) >
		     ARRAY_SIZE(stats_rssi->rssi_avg_beacon));

	for (j = 0; j < ARRAY_SIZE(arsta->chain_signal); j++) {
		arsta->chain_signal[j] = stats_rssi->rssi_avg_beacon[j];
		ath11k_dbg(ab, ATH11K_DBG_WMI,
			   "stats beacon rssi[%d] %d data rssi[%d] %d\n",
			   j,
			   stats_rssi->rssi_avg_beacon[j],
			   j,
			   stats_rssi->rssi_avg_data[j]);
	}

exit:
	rcu_read_unlock();
	return ret;
}

static int ath11k_wmi_tlv_fw_stats_data_parse(struct ath11k_base *ab,
					      struct wmi_tlv_fw_stats_parse *parse,
					      const void *ptr,
					      u16 len)
{
	struct ath11k_fw_stats *stats = parse->stats;
	const struct wmi_stats_event *ev = parse->ev;
	struct ath11k *ar;
	struct ath11k_vif *arvif;
	struct ieee80211_sta *sta;
	struct ath11k_sta *arsta;
	int i, ret = 0;
	const void *data = ptr;
	u64 time, time_busy;

	if (!ev) {
		ath11k_warn(ab, "failed to fetch update stats ev");
		return -EPROTO;
	}

	stats->stats_id = 0;

	rcu_read_lock();

	ar = ath11k_mac_get_ar_by_pdev_id(ab, ev->pdev_id);

	for (i = 0; i < ev->num_pdev_stats; i++) {
		const struct wmi_pdev_stats *src;
		struct ath11k_fw_stats_pdev *dst;

		src = data;
		if (len < sizeof(*src)) {
			ret = -EPROTO;
			goto exit;
		}

		stats->stats_id = WMI_REQUEST_PDEV_STAT;

		data += sizeof(*src);
		len -= sizeof(*src);

		dst = kzalloc(sizeof(*dst), GFP_ATOMIC);
		if (!dst)
			continue;

		ath11k_wmi_pull_pdev_stats_base(ar, &src->base, dst);
		ath11k_wmi_pull_pdev_stats_tx(&src->tx, dst);
		ath11k_wmi_pull_pdev_stats_rx(&src->rx, dst);
		list_add_tail(&dst->list, &stats->pdevs);

		spin_lock_bh(&ar->data_lock);
		time = div_u64(dst->cycle_count, ab->cc_freq_hz);
		if(ar && time) {
			time_busy = div_u64(dst->rx_clear_count, ab->cc_freq_hz);
			ar->hw->medium_busy = div_u64((time_busy * 100), time);
		}
		spin_unlock_bh(&ar->data_lock);
	}

	for (i = 0; i < ev->num_vdev_stats; i++) {
		const struct wmi_vdev_stats *src;
		struct ath11k_fw_stats_vdev *dst;

		src = data;
		if (len < sizeof(*src)) {
			ret = -EPROTO;
			goto exit;
		}

		stats->stats_id = WMI_REQUEST_VDEV_STAT;

		arvif = ath11k_mac_get_arvif(ar, src->vdev_id);
		if (arvif) {
			sta = ieee80211_find_sta_by_ifaddr(ar->hw,
							   arvif->bssid,
							   NULL);
			if (sta) {
				arsta = ath11k_sta_to_arsta(sta);
				arsta->rssi_beacon = src->beacon_snr;
				ath11k_dbg(ab, ATH11K_DBG_WMI,
					   "stats vdev id %d snr %d\n",
					   src->vdev_id, src->beacon_snr);
			} else {
				ath11k_dbg(ab, ATH11K_DBG_WMI,
					   "not found station of bssid %pM for vdev stat\n",
					   arvif->bssid);
			}
		}

		data += sizeof(*src);
		len -= sizeof(*src);

		dst = kzalloc(sizeof(*dst), GFP_ATOMIC);
		if (!dst)
			continue;

		ath11k_wmi_pull_vdev_stats(src, dst);
		list_add_tail(&dst->list, &stats->vdevs);
	}

	for (i = 0; i < ev->num_bcn_stats; i++) {
		const struct wmi_bcn_stats *src;
		struct ath11k_fw_stats_bcn *dst;

		src = data;
		if (len < sizeof(*src)) {
			ret = -EPROTO;
			goto exit;
		}

		stats->stats_id = WMI_REQUEST_BCN_STAT;

		data += sizeof(*src);
		len -= sizeof(*src);

		dst = kzalloc(sizeof(*dst), GFP_ATOMIC);
		if (!dst)
			continue;

		ath11k_wmi_pull_bcn_stats(src, dst);
		list_add_tail(&dst->list, &stats->bcn);
	}

exit:
	rcu_read_unlock();
	return ret;
}

static int ath11k_wmi_tlv_fw_stats_parse(struct ath11k_base *ab,
					 u16 tag, u16 len,
					 const void *ptr, void *data)
{
	struct wmi_tlv_fw_stats_parse *parse = data;
	int ret = 0;

	switch (tag) {
	case WMI_TAG_STATS_EVENT:
		parse->ev = (struct wmi_stats_event *)ptr;
		parse->stats->pdev_id = parse->ev->pdev_id;
		break;
	case WMI_TAG_ARRAY_BYTE:
		ret = ath11k_wmi_tlv_fw_stats_data_parse(ab, parse, ptr, len);
		break;
	case WMI_TAG_PER_CHAIN_RSSI_STATS:
		parse->rssi = (struct wmi_per_chain_rssi_stats *)ptr;

		if (parse->ev->stats_id & WMI_REQUEST_RSSI_PER_CHAIN_STAT)
			parse->rssi_num = parse->rssi->num_per_chain_rssi_stats;

		ath11k_dbg(ab, ATH11K_DBG_WMI,
			   "stats id 0x%x num chain %d\n",
			   parse->ev->stats_id,
			   parse->rssi_num);
		break;
	case WMI_TAG_ARRAY_STRUCT:
		if (parse->rssi_num && !parse->chain_rssi_done) {
			ret = ath11k_wmi_tlv_iter(ab, ptr, len,
						  ath11k_wmi_tlv_rssi_chain_parse,
						  parse);
			if (ret) {
				ath11k_warn(ab, "failed to parse rssi chain %d\n",
					    ret);
				return ret;
			}
			parse->chain_rssi_done = true;
		}
		break;
	default:
		break;
	}
	return ret;
}

int ath11k_wmi_pull_fw_stats(struct ath11k_base *ab, struct sk_buff *skb,
			     struct ath11k_fw_stats *stats)
{
	struct wmi_tlv_fw_stats_parse parse = { };

	stats->stats_id = 0;
	parse.stats = stats;

	return ath11k_wmi_tlv_iter(ab, skb->data, skb->len,
				   ath11k_wmi_tlv_fw_stats_parse,
				   &parse);
}

static void
ath11k_wmi_fw_pdev_base_stats_fill(const struct ath11k_fw_stats_pdev *pdev,
				   char *buf, u32 *length)
{
	u32 len = *length;
	u32 buf_len = ATH11K_FW_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "\n");
	len += scnprintf(buf + len, buf_len - len, "%30s\n",
			"ath11k PDEV stats");
	len += scnprintf(buf + len, buf_len - len, "%30s\n\n",
			"=================");

	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			"Channel noise floor", pdev->ch_noise_floor);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			"Channel TX power", pdev->chan_tx_power);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			"TX frame count", pdev->tx_frame_count);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			"RX frame count", pdev->rx_frame_count);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			"RX clear count", pdev->rx_clear_count);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			"Cycle count", pdev->cycle_count);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			"PHY error count", pdev->phy_err_count);

	*length = len;
}

static void
ath11k_wmi_fw_pdev_tx_stats_fill(const struct ath11k_fw_stats_pdev *pdev,
				 char *buf, u32 *length)
{
	u32 len = *length;
	u32 buf_len = ATH11K_FW_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "\n%30s\n",
			 "ath11k PDEV TX stats");
	len += scnprintf(buf + len, buf_len - len, "%30s\n\n",
			 "====================");

	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "HTT cookies queued", pdev->comp_queued);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "HTT cookies disp.", pdev->comp_delivered);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "MSDU queued", pdev->msdu_enqued);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "MPDU queued", pdev->mpdu_enqued);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "MSDUs dropped", pdev->wmm_drop);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Local enqued", pdev->local_enqued);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Local freed", pdev->local_freed);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "HW queued", pdev->hw_queued);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "PPDUs reaped", pdev->hw_reaped);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Num underruns", pdev->underrun);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Num HW Paused", pdev->hw_paused);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "PPDUs cleaned", pdev->tx_abort);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "MPDUs requeued", pdev->mpdus_requeued);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "PPDU OK", pdev->tx_ko);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Excessive retries", pdev->tx_xretry);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "HW rate", pdev->data_rc);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Sched self triggers", pdev->self_triggers);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Dropped due to SW retries",
			 pdev->sw_retry_failure);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Illegal rate phy errors",
			 pdev->illgl_rate_phy_err);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "PDEV continuous xretry", pdev->pdev_cont_xretry);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "TX timeout", pdev->pdev_tx_timeout);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "PDEV resets", pdev->pdev_resets);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Stateless TIDs alloc failures",
			 pdev->stateless_tid_alloc_failure);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "PHY underrun", pdev->phy_underrun);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "MPDU is more than txop limit", pdev->txop_ovf);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Num sequences posted", pdev->seq_posted);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Num seq failed queueing ", pdev->seq_failed_queueing);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Num sequences completed ", pdev->seq_completed);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Num sequences restarted ", pdev->seq_restarted);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Num of MU sequences posted ", pdev->mu_seq_posted);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Num of MPDUS SW flushed ", pdev->mpdus_sw_flush);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Num of MPDUS HW filtered ", pdev->mpdus_hw_filter);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Num of MPDUS truncated ", pdev->mpdus_truncated);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Num of MPDUS ACK failed ", pdev->mpdus_ack_failed);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Num of MPDUS expired ", pdev->mpdus_expired);
	*length = len;
}

static void
ath11k_wmi_fw_pdev_rx_stats_fill(const struct ath11k_fw_stats_pdev *pdev,
				 char *buf, u32 *length)
{
	u32 len = *length;
	u32 buf_len = ATH11K_FW_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "\n%30s\n",
			 "ath11k PDEV RX stats");
	len += scnprintf(buf + len, buf_len - len, "%30s\n\n",
			 "====================");

	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Mid PPDU route change",
			 pdev->mid_ppdu_route_change);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Tot. number of statuses", pdev->status_rcvd);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Extra frags on rings 0", pdev->r0_frags);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Extra frags on rings 1", pdev->r1_frags);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Extra frags on rings 2", pdev->r2_frags);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Extra frags on rings 3", pdev->r3_frags);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "MSDUs delivered to HTT", pdev->htt_msdus);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "MPDUs delivered to HTT", pdev->htt_mpdus);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "MSDUs delivered to stack", pdev->loc_msdus);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "MPDUs delivered to stack", pdev->loc_mpdus);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Oversized AMSUs", pdev->oversize_amsdu);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "PHY errors", pdev->phy_errs);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "PHY errors drops", pdev->phy_err_drop);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "MPDU errors (FCS, MIC, ENC)", pdev->mpdu_errs);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Overflow errors", pdev->rx_ovfl_errs);
	*length = len;
}

static void
ath11k_wmi_fw_vdev_stats_fill(struct ath11k *ar,
			      const struct ath11k_fw_stats_vdev *vdev,
			      char *buf, u32 *length)
{
	u32 len = *length;
	u32 buf_len = ATH11K_FW_STATS_BUF_SIZE;
	struct ath11k_vif *arvif = ath11k_mac_get_arvif(ar, vdev->vdev_id);
	u8 *vif_macaddr;
	int i;

	/* VDEV stats has all the active VDEVs of other PDEVs as well,
	 * ignoring those not part of requested PDEV
	 */
	if (!arvif)
		return;

	vif_macaddr = arvif->vif->addr;

	len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
			 "VDEV ID", vdev->vdev_id);
	len += scnprintf(buf + len, buf_len - len, "%30s %pM\n",
			 "VDEV MAC address", vif_macaddr);
	len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
			 "beacon snr", vdev->beacon_snr);
	len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
			 "data snr", vdev->data_snr);
	len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
			 "num rx frames", vdev->num_rx_frames);
	len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
			 "num rts fail", vdev->num_rts_fail);
	len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
			 "num rts success", vdev->num_rts_success);
	len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
			 "num rx err", vdev->num_rx_err);
	len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
			 "num rx discard", vdev->num_rx_discard);
	len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
			 "num tx not acked", vdev->num_tx_not_acked);

	for (i = 0 ; i < ARRAY_SIZE(vdev->num_tx_frames); i++)
		len += scnprintf(buf + len, buf_len - len,
				"%25s [%02d] %u\n",
				"num tx frames", i,
				vdev->num_tx_frames[i]);

	for (i = 0 ; i < ARRAY_SIZE(vdev->num_tx_frames_retries); i++)
		len += scnprintf(buf + len, buf_len - len,
				"%25s [%02d] %u\n",
				"num tx frames retries", i,
				vdev->num_tx_frames_retries[i]);

	for (i = 0 ; i < ARRAY_SIZE(vdev->num_tx_frames_failures); i++)
		len += scnprintf(buf + len, buf_len - len,
				"%25s [%02d] %u\n",
				"num tx frames failures", i,
				vdev->num_tx_frames_failures[i]);

	for (i = 0 ; i < ARRAY_SIZE(vdev->tx_rate_history); i++)
		len += scnprintf(buf + len, buf_len - len,
				"%25s [%02d] 0x%08x\n",
				"tx rate history", i,
				vdev->tx_rate_history[i]);

	for (i = 0 ; i < ARRAY_SIZE(vdev->beacon_rssi_history); i++)
		len += scnprintf(buf + len, buf_len - len,
				"%25s [%02d] %u\n",
				"beacon rssi history", i,
				vdev->beacon_rssi_history[i]);

	len += scnprintf(buf + len, buf_len - len, "\n");
	*length = len;
}

static void
ath11k_wmi_fw_bcn_stats_fill(struct ath11k *ar,
			     const struct ath11k_fw_stats_bcn *bcn,
			     char *buf, u32 *length)
{
	u32 len = *length;
	u32 buf_len = ATH11K_FW_STATS_BUF_SIZE;
	struct ath11k_vif *arvif = ath11k_mac_get_arvif(ar, bcn->vdev_id);
	u8 *vdev_macaddr;

	if (!arvif) {
		ath11k_warn(ar->ab, "invalid vdev id %d in bcn stats",
			    bcn->vdev_id);
		return;
	}

	vdev_macaddr = arvif->vif->addr;

	len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
			 "VDEV ID", bcn->vdev_id);
	len += scnprintf(buf + len, buf_len - len, "%30s %pM\n",
			 "VDEV MAC address", vdev_macaddr);
	len += scnprintf(buf + len, buf_len - len, "%30s\n\n",
			 "================");
	len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
			 "Num of beacon tx success", bcn->tx_bcn_succ_cnt);
	len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
			 "Num of beacon tx failures", bcn->tx_bcn_outage_cnt);

	len += scnprintf(buf + len, buf_len - len, "\n");
	*length = len;
}

void ath11k_wmi_fw_stats_fill(struct ath11k *ar,
			      struct ath11k_fw_stats *fw_stats,
			      u32 stats_id, char *buf)
{
	u32 len = 0;
	u32 buf_len = ATH11K_FW_STATS_BUF_SIZE;
	const struct ath11k_fw_stats_pdev *pdev;
	const struct ath11k_fw_stats_vdev *vdev;
	const struct ath11k_fw_stats_bcn *bcn;
	size_t num_bcn;

	spin_lock_bh(&ar->data_lock);

	if (stats_id == WMI_REQUEST_PDEV_STAT) {
		pdev = list_first_entry_or_null(&fw_stats->pdevs,
						struct ath11k_fw_stats_pdev, list);
		if (!pdev) {
			ath11k_warn(ar->ab, "failed to get pdev stats\n");
			goto unlock;
		}

		ath11k_wmi_fw_pdev_base_stats_fill(pdev, buf, &len);
		ath11k_wmi_fw_pdev_tx_stats_fill(pdev, buf, &len);
		ath11k_wmi_fw_pdev_rx_stats_fill(pdev, buf, &len);
	}

	if (stats_id == WMI_REQUEST_VDEV_STAT) {
		len += scnprintf(buf + len, buf_len - len, "\n");
		len += scnprintf(buf + len, buf_len - len, "%30s\n",
				 "ath11k VDEV stats");
		len += scnprintf(buf + len, buf_len - len, "%30s\n\n",
				 "=================");

		list_for_each_entry(vdev, &fw_stats->vdevs, list)
			ath11k_wmi_fw_vdev_stats_fill(ar, vdev, buf, &len);
	}

	if (stats_id == WMI_REQUEST_BCN_STAT) {
		num_bcn = list_count_nodes(&fw_stats->bcn);

		len += scnprintf(buf + len, buf_len - len, "\n");
		len += scnprintf(buf + len, buf_len - len, "%30s (%zu)\n",
				 "ath11k Beacon stats", num_bcn);
		len += scnprintf(buf + len, buf_len - len, "%30s\n\n",
				 "===================");

		list_for_each_entry(bcn, &fw_stats->bcn, list)
			ath11k_wmi_fw_bcn_stats_fill(ar, bcn, buf, &len);
	}

unlock:
	spin_unlock_bh(&ar->data_lock);

	if (len >= buf_len)
		buf[len - 1] = 0;
	else
		buf[len] = 0;
}

static void ath11k_wmi_op_ep_tx_credits(struct ath11k_base *ab)
{
	/* try to send pending beacons first. they take priority */
	wake_up(&ab->wmi_ab.tx_credits_wq);
}

static int ath11k_reg_11d_new_cc_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	const struct wmi_11d_new_cc_ev *ev;
	struct ath11k *ar;
	struct ath11k_pdev *pdev;
	const void **tb;
	int ret, i;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_11D_NEW_COUNTRY_EVENT];
	if (!ev) {
		kfree(tb);
		ath11k_warn(ab, "failed to fetch 11d new cc ev");
		return -EPROTO;
	}

	spin_lock_bh(&ab->base_lock);
	memcpy(&ab->new_alpha2, &ev->new_alpha2, 2);
	spin_unlock_bh(&ab->base_lock);

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event 11d new cc %c%c\n",
		   ab->new_alpha2[0],
		   ab->new_alpha2[1]);

	kfree(tb);

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		ar = pdev->ar;
		ar->state_11d = ATH11K_11D_IDLE;
		complete(&ar->completed_11d_scan);
	}

	queue_work(ab->workqueue, &ab->update_11d_work);

	return 0;
}

static void ath11k_wmi_htc_tx_complete(struct ath11k_base *ab,
				       struct sk_buff *skb)
{
	struct ath11k_pdev_wmi *wmi = NULL;
	u32 i;
	u8 wmi_ep_count;
	u8 eid;

	eid = ATH11K_SKB_CB(skb)->eid;
	ATH11K_MEMORY_STATS_DEC(ab, wmi_tx_skb_alloc, skb->truesize);
	dev_kfree_skb(skb);

	if (eid >= ATH11K_HTC_EP_COUNT)
		return;

	wmi_ep_count = ab->htc.wmi_ep_count;
	if (wmi_ep_count > ab->hw_params.max_radios)
		return;

	for (i = 0; i < ab->htc.wmi_ep_count; i++) {
		if (ab->wmi_ab.wmi[i].eid == eid) {
			wmi = &ab->wmi_ab.wmi[i];
			break;
		}
	}

	if (wmi)
		wake_up(&wmi->tx_ce_desc_wq);
}

static int ath11k_reg_chan_list_event(struct ath11k_base *ab, struct sk_buff *skb,
				      enum wmi_reg_chan_list_cmd_type id)
{
	struct cur_regulatory_info *reg_info;
	int ret;

	reg_info = kzalloc(sizeof(*reg_info), GFP_ATOMIC);
	if (!reg_info)
		return -ENOMEM;

	if (id == WMI_REG_CHAN_LIST_CC_ID)
		ret = ath11k_pull_reg_chan_list_update_ev(ab, skb, reg_info);
	else
		ret = ath11k_pull_reg_chan_list_ext_update_ev(ab, skb, reg_info);

	if (ret) {
		ath11k_warn(ab, "failed to extract regulatory info from received event\n");
		goto mem_free;
	}

	ret = ath11k_reg_handle_chan_list(ab, reg_info, IEEE80211_REG_UNSET_AP);
	if (ret) {
		ath11k_warn(ab, "failed to process regulatory info from received event\n");
		goto mem_free;
	}

	return 0;

mem_free:
	if (ret != -EAGAIN)
		ath11k_reg_reset_info(reg_info);
	return ret;
}

static int ath11k_wmi_tlv_rdy_parse(struct ath11k_base *ab, u16 tag, u16 len,
				    const void *ptr, void *data)
{
	struct wmi_tlv_rdy_parse *rdy_parse = data;
	struct wmi_ready_event fixed_param;
	struct wmi_mac_addr *addr_list;
	struct ath11k_pdev *pdev;
	u32 num_mac_addr;
	int i;

	switch (tag) {
	case WMI_TAG_READY_EVENT:
		memset(&fixed_param, 0, sizeof(fixed_param));
		memcpy(&fixed_param, (struct wmi_ready_event *)ptr,
		       min_t(u16, sizeof(fixed_param), len));
		rdy_parse->num_extra_mac_addr =
			fixed_param.ready_event_min.num_extra_mac_addr;

		ether_addr_copy(ab->mac_addr,
				fixed_param.ready_event_min.mac_addr.addr);
		ab->max_ast_index = fixed_param.max_ast_index + 1;
		ab->pktlog_defs_checksum = fixed_param.pktlog_defs_checksum;
		break;
	case WMI_TAG_ARRAY_FIXED_STRUCT:
		addr_list = (struct wmi_mac_addr *)ptr;
		num_mac_addr = rdy_parse->num_extra_mac_addr;

		if (!(ab->num_radios > 1 && num_mac_addr >= ab->num_radios))
			break;

		for (i = 0; i < ab->num_radios; i++) {
			pdev = &ab->pdevs[i];
			ether_addr_copy(pdev->mac_addr, addr_list[i].addr);
		}
		ab->pdevs_macaddr_valid = true;
		break;
	default:
		break;
	}

	return 0;
}

static int ath11k_ready_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct wmi_tlv_rdy_parse rdy_parse = { };
	int ret;

	ret = ath11k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath11k_wmi_tlv_rdy_parse, &rdy_parse);
	if (ret) {
		ath11k_warn(ab, "failed to parse tlv %d\n", ret);
		return ret;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event ready");

	complete(&ab->wmi_ab.unified_ready);
	return 0;
}

static void ath11k_peer_delete_resp_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct wmi_peer_delete_resp_event peer_del_resp;
	struct ath11k *ar;

	if (ath11k_pull_peer_del_resp_ev(ab, skb, &peer_del_resp) != 0) {
		ath11k_warn(ab, "failed to extract peer delete resp");
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event peer delete resp");

	rcu_read_lock();
	ar = ath11k_mac_get_ar_by_vdev_id(ab, peer_del_resp.vdev_id);
	if (!ar) {
		ath11k_warn(ab, "invalid vdev id in peer delete resp ev %d",
			    peer_del_resp.vdev_id);
		rcu_read_unlock();
		return;
	}

	complete(&ar->peer_delete_done);
	rcu_read_unlock();
	ath11k_dbg(ab, ATH11K_DBG_WMI, "peer delete resp for vdev id %d addr %pM\n",
		   peer_del_resp.vdev_id, peer_del_resp.peer_macaddr.addr);
}

static void ath11k_vdev_delete_resp_event(struct ath11k_base *ab,
					  struct sk_buff *skb)
{
	struct ath11k *ar;
	u32 vdev_id = 0;

	if (ath11k_pull_vdev_del_resp_ev(ab, skb, &vdev_id) != 0) {
		ath11k_warn(ab, "failed to extract vdev delete resp");
		return;
	}

	rcu_read_lock();
	ar = ath11k_mac_get_ar_by_vdev_id(ab, vdev_id);
	if (!ar) {
		ath11k_warn(ab, "invalid vdev id in vdev delete resp ev %d",
			    vdev_id);
		rcu_read_unlock();
		return;
	}

	complete(&ar->vdev_delete_done);

	rcu_read_unlock();

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event vdev delete resp for vdev id %d\n",
		   vdev_id);
}

static inline const char *ath11k_wmi_vdev_resp_print(u32 vdev_resp_status)
{
	switch (vdev_resp_status) {
	case WMI_VDEV_START_RESPONSE_INVALID_VDEVID:
		return "invalid vdev id";
	case WMI_VDEV_START_RESPONSE_NOT_SUPPORTED:
		return "not supported";
	case WMI_VDEV_START_RESPONSE_DFS_VIOLATION:
		return "dfs violation";
	case WMI_VDEV_START_RESPONSE_INVALID_REGDOMAIN:
		return "invalid regdomain";
	default:
		return "unknown";
	}
}

static void ath11k_vdev_start_resp_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct wmi_vdev_start_resp_event vdev_start_resp;
	struct ath11k *ar;
	u32 status;

	if (ath11k_pull_vdev_start_resp_tlv(ab, skb, &vdev_start_resp) != 0) {
		ath11k_warn(ab, "failed to extract vdev start resp");
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event start resp event");

	rcu_read_lock();
	ar = ath11k_mac_get_ar_by_vdev_id(ab, vdev_start_resp.vdev_id);
	if (!ar) {
		ath11k_warn(ab, "invalid vdev id in vdev start resp ev %d",
			    vdev_start_resp.vdev_id);
		rcu_read_unlock();
		return;
	}

	ar->last_wmi_vdev_start_status = 0;
	ar->max_allowed_tx_power = vdev_start_resp.max_allowed_tx_power;
	status = vdev_start_resp.status;

	if (WARN_ON_ONCE(status)) {
		ath11k_warn(ab, "vdev start resp error status %d (%s)\n",
			    status, ath11k_wmi_vdev_resp_print(status));
		ar->last_wmi_vdev_start_status = status;
	}

	complete(&ar->vdev_setup_done);

	rcu_read_unlock();

	ath11k_dbg(ab, ATH11K_DBG_WMI, "vdev start resp for vdev id %d",
		   vdev_start_resp.vdev_id);
}

static void ath11k_bcn_tx_status_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct ath11k_vif *arvif;
	u32 vdev_id, tx_status;

	if (ath11k_pull_bcn_tx_status_ev(ab, skb, &vdev_id, &tx_status) != 0) {
		ath11k_warn(ab, "failed to extract bcn tx status");
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event offload bcn tx status");

	rcu_read_lock();
	arvif = ath11k_mac_get_arvif_by_vdev_id(ab, vdev_id);
	if (!arvif) {
		ath11k_warn(ab, "invalid vdev id %d in bcn_tx_status",
			    vdev_id);
		rcu_read_unlock();
		return;
	}

	queue_work(ab->workqueue, &arvif->bcn_tx_work);

	rcu_read_unlock();
}

static void ath11k_wmi_event_peer_sta_ps_state_chg(struct ath11k_base *ab,
						   struct sk_buff *skb)
{
	const struct wmi_peer_sta_ps_state_chg_event *ev;
	struct ieee80211_sta *sta;
	struct ath11k_peer *peer;
	struct ath11k *ar;
	struct ath11k_sta *arsta;
	const void **tb;
	enum ath11k_wmi_peer_ps_state peer_previous_ps_state;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_PEER_STA_PS_STATECHANGE_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch sta ps change ev");
		kfree(tb);
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "event peer sta ps change ev addr %pM state %u sup_bitmap %x ps_valid %u ts %u\n",
		   ev->peer_macaddr.addr, ev->peer_ps_state,
		   ev->ps_supported_bitmap, ev->peer_ps_valid,
		   ev->peer_ps_timestamp);

	rcu_read_lock();

	spin_lock_bh(&ab->base_lock);

	peer = ath11k_peer_find_by_addr(ab, ev->peer_macaddr.addr);

	if (!peer) {
		spin_unlock_bh(&ab->base_lock);
		ath11k_warn(ab, "peer not found %pM\n", ev->peer_macaddr.addr);
		goto exit;
	}

	ar = ath11k_mac_get_ar_by_vdev_id(ab, peer->vdev_id);

	if (!ar) {
		spin_unlock_bh(&ab->base_lock);
		ath11k_warn(ab, "invalid vdev id in peer sta ps state change ev %d",
			    peer->vdev_id);

		goto exit;
	}

	sta = peer->sta;

	spin_unlock_bh(&ab->base_lock);

	if (!sta) {
		ath11k_warn(ab, "failed to find station entry %pM\n",
			    ev->peer_macaddr.addr);
		goto exit;
	}

	arsta = ath11k_sta_to_arsta(sta);

	spin_lock_bh(&ar->data_lock);

	peer_previous_ps_state = arsta->peer_ps_state;
	arsta->peer_ps_state = ev->peer_ps_state;
	arsta->peer_current_ps_valid = !!ev->peer_ps_valid;

	if (test_bit(WMI_TLV_SERVICE_PEER_POWER_SAVE_DURATION_SUPPORT,
		     ar->ab->wmi_ab.svc_map)) {
		if (!(ev->ps_supported_bitmap & WMI_PEER_PS_VALID) ||
		    !(ev->ps_supported_bitmap & WMI_PEER_PS_STATE_TIMESTAMP) ||
		    !ev->peer_ps_valid)
			goto out;

		if (arsta->peer_ps_state == WMI_PEER_PS_STATE_ON) {
			arsta->ps_start_time = ev->peer_ps_timestamp;
			arsta->ps_start_jiffies = jiffies;
		} else if (arsta->peer_ps_state == WMI_PEER_PS_STATE_OFF &&
			   peer_previous_ps_state == WMI_PEER_PS_STATE_ON) {
			arsta->ps_total_duration = arsta->ps_total_duration +
					(ev->peer_ps_timestamp - arsta->ps_start_time);
		}

		if (ar->ps_timekeeper_enable)
			trace_ath11k_ps_timekeeper(ar, ev->peer_macaddr.addr,
						   ev->peer_ps_timestamp,
						   arsta->peer_ps_state);
	}

out:
	spin_unlock_bh(&ar->data_lock);
exit:
	rcu_read_unlock();
	kfree(tb);
}

static void ath11k_vdev_stopped_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct ath11k *ar;
	u32 vdev_id = 0;

	if (ath11k_pull_vdev_stopped_param_tlv(ab, skb, &vdev_id) != 0) {
		ath11k_warn(ab, "failed to extract vdev stopped event");
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event vdev stopped");

	rcu_read_lock();
	ar = ath11k_mac_get_ar_by_vdev_id(ab, vdev_id);
	if (!ar) {
		ath11k_warn(ab, "invalid vdev id in vdev stopped ev %d",
			    vdev_id);
		rcu_read_unlock();
		return;
	}

	complete(&ar->vdev_setup_done);

	rcu_read_unlock();

	ath11k_dbg(ab, ATH11K_DBG_WMI, "vdev stopped for vdev id %d", vdev_id);
}

static void ath11k_mgmt_rx_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct mgmt_rx_event_params rx_ev = {0};
	struct ath11k *ar;
	struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(skb);
	struct ieee80211_hdr *hdr;
	u16 fc;
	struct ieee80211_supported_band *sband;
	struct ath11k_peer *peer;
	struct ieee80211_vif *vif;
	struct ath11k_vif *arvif;
	struct ath11k_mgmt_frame_stats *mgmt_stats;
	u16 frm_type = 0;

	if (ath11k_pull_mgmt_rx_params_tlv(ab, skb, &rx_ev) != 0) {
		ath11k_warn(ab, "failed to extract mgmt rx event");
		dev_kfree_skb(skb);
		return;
	}

	memset(status, 0, sizeof(*status));

	ath11k_dbg(ab, ATH11K_DBG_MGMT, "event mgmt rx status %08x\n",
		   rx_ev.status);

	rcu_read_lock();
	ar = ath11k_mac_get_ar_by_pdev_id(ab, rx_ev.pdev_id);

	if (!ar) {
		ath11k_warn(ab, "invalid pdev_id %d in mgmt_rx_event\n",
			    rx_ev.pdev_id);
		dev_kfree_skb(skb);
		goto exit;
	}

	if ((test_bit(ATH11K_CAC_RUNNING, &ar->dev_flags)) ||
	    (rx_ev.status & (WMI_RX_STATUS_ERR_DECRYPT |
	    WMI_RX_STATUS_ERR_KEY_CACHE_MISS | WMI_RX_STATUS_ERR_CRC))) {
		dev_kfree_skb(skb);
		goto exit;
	}

	if (rx_ev.status & WMI_RX_STATUS_ERR_MIC)
		status->flag |= RX_FLAG_MMIC_ERROR;

	if (rx_ev.chan_freq >= ATH11K_MIN_6G_FREQ &&
	    rx_ev.chan_freq <= ATH11K_MAX_6G_FREQ) {
		status->band = NL80211_BAND_6GHZ;
		status->freq = rx_ev.chan_freq;
	} else if (rx_ev.channel >= 1 && rx_ev.channel <= 14) {
		status->band = NL80211_BAND_2GHZ;
	} else if (rx_ev.channel >= 36 && rx_ev.channel <= ATH11K_MAX_5G_CHAN) {
		status->band = NL80211_BAND_5GHZ;
	} else {
		/* Shouldn't happen unless list of advertised channels to
		 * mac80211 has been changed.
		 */
		WARN_ON_ONCE(1);
		dev_kfree_skb(skb);
		goto exit;
	}

	if (rx_ev.phy_mode == MODE_11B &&
	    (status->band == NL80211_BAND_5GHZ || status->band == NL80211_BAND_6GHZ))
		ath11k_dbg(ab, ATH11K_DBG_WMI,
			   "mgmt rx 11b (CCK) on 5/6GHz, band = %d\n", status->band);

	sband = &ar->mac.sbands[status->band];

	if (status->band != NL80211_BAND_6GHZ)
		status->freq = ieee80211_channel_to_frequency(rx_ev.channel,
							      status->band);

	status->signal = rx_ev.snr + ATH11K_DEFAULT_NOISE_FLOOR;
	status->rate_idx = ath11k_mac_bitrate_to_idx(sband, rx_ev.rate / 100);

	hdr = (struct ieee80211_hdr *)skb->data;
	fc = le16_to_cpu(hdr->frame_control);
	frm_type = FIELD_GET(IEEE80211_FCTL_STYPE, fc);

	spin_lock_bh(&ab->base_lock);

	peer = ath11k_peer_find_by_addr(ab, hdr->addr1);
	if(!peer)
		peer = ath11k_peer_find_by_addr(ab, hdr->addr3);
	if (!peer) {
		spin_unlock_bh(&ab->base_lock);
		goto skip_mgmt_stats;
	}

	vif = peer->vif;

	spin_unlock_bh(&ab->base_lock);

	if (!vif)
		goto skip_mgmt_stats;

	spin_lock_bh(&ar->data_lock);

	arvif = ath11k_vif_to_arvif(vif);
	mgmt_stats = &arvif->mgmt_stats;
	mgmt_stats->rx_cnt[frm_type]++;

	spin_unlock_bh(&ar->data_lock);

skip_mgmt_stats:
	/* Firmware is guaranteed to report all essential management frames via
	 * WMI while it can deliver some extra via HTT. Since there can be
	 * duplicates split the reporting wrt monitor/sniffing.
	 */
	status->flag |= RX_FLAG_SKIP_MONITOR;

	/* In case of PMF, FW delivers decrypted frames with Protected Bit set.
	 * Don't clear that. Also, FW delivers broadcast management frames
	 * (ex: group privacy action frames in mesh) as encrypted payload.
	 */
	if (ieee80211_has_protected(hdr->frame_control) &&
	    !is_multicast_ether_addr(ieee80211_get_DA(hdr))) {
		status->flag |= RX_FLAG_DECRYPTED;

		if (!ieee80211_is_robust_mgmt_frame(skb)) {
			status->flag |= RX_FLAG_IV_STRIPPED |
					RX_FLAG_MMIC_STRIPPED;
			hdr->frame_control = __cpu_to_le16(fc &
					     ~IEEE80211_FCTL_PROTECTED);
		}
	}

	if (ieee80211_is_beacon(hdr->frame_control))
		ath11k_mac_handle_beacon(ar, skb);

	ath11k_dbg(ab, ATH11K_DBG_MGMT,
		   "event mgmt rx skb %p len %d ftype %02x stype %02x\n",
		   skb, skb->len,
		   fc & IEEE80211_FCTL_FTYPE, fc & IEEE80211_FCTL_STYPE);

	ath11k_dbg(ab, ATH11K_DBG_MGMT,
		   "event mgmt rx freq %d band %d snr %d, rate_idx %d\n",
		   status->freq, status->band, status->signal,
		   status->rate_idx);

	ieee80211_rx_ni(ar->hw, skb);

exit:
	rcu_read_unlock();
}

static void ath11k_mgmt_tx_compl_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct wmi_tx_compl_event tx_compl_param = {0};
	struct ath11k *ar;

	if (ath11k_pull_tx_compl_param_tlv(ab, skb, &tx_compl_param,
					   WMI_TAG_MGMT_TX_COMPL_EVENT) != 0) {
		ath11k_warn(ab, "failed to extract mgmt tx compl event");
		return;
	}

	rcu_read_lock();
	ar = ath11k_mac_get_ar_by_pdev_id(ab, tx_compl_param.pdev_id);
	if (!ar) {
		ath11k_warn(ab, "invalid pdev id %d in mgmt_tx_compl_event\n",
			    tx_compl_param.pdev_id);
		goto exit;
	}

	wmi_process_tx_comp(ar, &tx_compl_param);

	ath11k_dbg(ab, ATH11K_DBG_MGMT,
		   "event mgmt tx compl ev pdev_id %d, desc_id %d, status %d ack_rssi %d",
		   tx_compl_param.pdev_id, tx_compl_param.desc_id,
		   tx_compl_param.status, tx_compl_param.ack_rssi);

exit:
	rcu_read_unlock();
}

void ath11k_wmi_crl_path_stats_list_free(struct list_head *head)
{
	struct wmi_ctrl_path_stats_list *stats, *tmp;
	list_for_each_entry_safe(stats, tmp, head, list) {
		if (stats->stats_ptr)
			kfree(stats->stats_ptr);
	        list_del(&stats->list);
	        kfree(stats);
	}
}

int wmi_print_ctrl_path_pdev_tx_stats_tlv(struct ath11k_base *ab, u16 len, const void *ptr, void *data)
{
	struct wmi_ctrl_path_stats_ev_parse_param *stats_buff = (struct wmi_ctrl_path_stats_ev_parse_param *)data;
	struct wmi_ctrl_path_pdev_stats *pdev_stats_skb = (struct wmi_ctrl_path_pdev_stats *)ptr;
	struct wmi_ctrl_path_pdev_stats *pdev_stats = NULL;
	struct wmi_ctrl_path_stats_list *stats = kzalloc(sizeof(struct wmi_ctrl_path_stats_list),GFP_ATOMIC);
	struct ath11k *ar = NULL;

	if (!stats)
		return -ENOMEM;

	pdev_stats = kzalloc(sizeof(*pdev_stats), GFP_ATOMIC);
	if (!pdev_stats)
		return -ENOMEM;

	memcpy(pdev_stats, pdev_stats_skb, sizeof(struct wmi_ctrl_path_pdev_stats));
	stats->stats_ptr = pdev_stats;
	list_add_tail(&stats->list, &stats_buff->list);

	ar = ath11k_mac_get_ar_by_pdev_id(ab, pdev_stats_skb->pdev_id + 1);
	if (!ar) {
		ath11k_warn(ab, "Failed to get ar for wmi ctrl stats\n");
		return -EINVAL;
	}

	ar->debug.wmi_ctrl_path_stats_tagid = WMI_CTRL_PATH_PDEV_STATS;
	stats_buff->ar = ar;
	return 0;
}

int wmi_print_ctrl_path_cal_stats_tlv(struct ath11k_base *ab, u16 len,
				      const void *ptr, void *data)
{
	struct wmi_ctrl_path_stats_ev_parse_param *stats_buff = (struct wmi_ctrl_path_stats_ev_parse_param *)data;
	struct wmi_ctrl_path_cal_stats *cal_stats_skb = (struct wmi_ctrl_path_cal_stats *)ptr;
	struct wmi_ctrl_path_cal_stats *cal_stats = NULL;
	struct wmi_ctrl_path_stats_list *stats = kzalloc(sizeof(struct wmi_ctrl_path_stats_list),GFP_ATOMIC);
	struct ath11k *ar = NULL;

	if (!stats)
		return -ENOMEM;

	cal_stats = kzalloc(sizeof(*cal_stats), GFP_ATOMIC);
	if (!cal_stats) {
		kfree(stats);
		return -ENOMEM;
	}

	memcpy(cal_stats, cal_stats_skb, sizeof(struct wmi_ctrl_path_cal_stats));
	stats->stats_ptr = cal_stats;
	list_add_tail(&stats->list, &stats_buff->list);

	ar = ath11k_mac_get_ar_by_pdev_id(ab, cal_stats_skb->pdev_id + 1);
	if (!ar) {
		ath11k_warn(ab, "Failed to get ar for wmi ctrl cal stats\n");
		return -EINVAL;
	}

	ar->debug.wmi_ctrl_path_stats_tagid = WMI_CTRL_PATH_CAL_STATS;
	stats_buff->ar = ar;
	return 0;
}

int wmi_print_ctrl_path_btcoex_stats_tlv(struct ath11k_base *ab, u16 len,
					 const void *ptr, void *data)
{
	struct wmi_ctrl_path_stats_ev_parse_param *stats_buff =
				(struct wmi_ctrl_path_stats_ev_parse_param *)data;
	struct wmi_ctrl_path_btcoex_stats *btcoex_stats_skb =
				(struct wmi_ctrl_path_btcoex_stats *)ptr;
	struct wmi_ctrl_path_btcoex_stats *btcoex_stats = NULL;
	struct wmi_ctrl_path_stats_list *stats;
	struct ath11k *ar = NULL;

	stats = kzalloc(sizeof(*stats), GFP_ATOMIC);
	if (!stats)
		return -ENOMEM;

	btcoex_stats = kzalloc(sizeof(*btcoex_stats), GFP_ATOMIC);
	if (!btcoex_stats) {
		kfree(stats);
		return -ENOMEM;
	}

	memcpy(btcoex_stats, btcoex_stats_skb, sizeof(*btcoex_stats));
	stats->stats_ptr = btcoex_stats;
	list_add_tail(&stats->list, &stats_buff->list);

	ar = ath11k_mac_get_ar_by_pdev_id(ab, btcoex_stats_skb->pdev_id + 1);
	if (!ar) {
		ath11k_warn(ab, "Failed to get ar for wmi ctrl cal stats\n");
		return -EINVAL;
	}

	ar->debug.wmi_ctrl_path_stats_tagid = WMI_CTRL_PATH_BTCOEX_STATS;
	stats_buff->ar = ar;
	return 0;
}

static int ath11k_wmi_ctrl_stats_subtlv_parser(struct ath11k_base *ab,
					      u16 tag, u16 len,
					      const void *ptr, void *data)
{
	int ret = 0;

	switch (tag) {
	case WMI_CTRL_PATH_STATS_EV_FIXED_PARAM:
		break;
	case WMI_CTRL_PATH_PDEV_STATS:
		ret = wmi_print_ctrl_path_pdev_tx_stats_tlv(ab, len, ptr, data);
		break;
	case WMI_CTRL_PATH_CAL_STATS:
		ret = wmi_print_ctrl_path_cal_stats_tlv(ab, len, ptr, data);
		break;
	case WMI_CTRL_PATH_BTCOEX_STATS:
		ret = wmi_print_ctrl_path_btcoex_stats_tlv(ab, len, ptr, data);
		break;
	/* Add case for newly wmi ctrl path added stats here */
	default:
		ath11k_warn(ab,
			    "Received invalid tag for wmi ctrl path stats in subtlvs, tag : 0x%x\n",
			    tag);
		return -EINVAL;
		break;
	}

	return ret;
}

static int ath11k_wmi_ctrl_stats_event_parser(struct ath11k_base *ab,
					     u16 tag, u16 len,
					     const void *ptr, void *data)
{
	int ret = 0;

	ath11k_dbg(ab, ATH11K_DBG_WMI, "wmi ctrl path stats tag 0x%x of len %d rcvd\n",
		   tag, len);

	switch (tag) {
	case WMI_CTRL_PATH_STATS_EV_FIXED_PARAM:
		/* Fixed param is already processed*/
		break;
	case WMI_TAG_ARRAY_STRUCT:
		/* len 0 is expected for array of struct when there
		 * is no content of that type to pack inside that tlv
		 */
		if (len == 0)
			return 0;
		ret = ath11k_wmi_tlv_iter(ab, ptr, len,
					  ath11k_wmi_ctrl_stats_subtlv_parser,
					  data);
		break;
	default:
		ath11k_warn(ab, "Received invalid tag for wmi ctrl path stats\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void ath11k_wmi_ctrl_path_stats_event(struct ath11k_base *ab, struct sk_buff *skb)
{
        int ret;
        const struct wmi_tlv *tlv;
	struct wmi_ctrl_path_stats_ev_param *fixed_param;
	u16 tlv_tag;
	u8 *ptr = skb->data;
	struct ath11k *ar = NULL;
	struct wmi_ctrl_path_stats_ev_parse_param param;
	INIT_LIST_HEAD(&param.list);

	if (!skb->data) {
		ath11k_warn(ab, "No data present in wmi ctrl stats event\n");
		return;
	}

	if (skb->len < (sizeof(*fixed_param) + TLV_HDR_SIZE)) {
		ath11k_warn(ab, "wmi ctrl stats event size invalid\n");
		return;
	}

	param.ar = NULL;

	tlv = (struct wmi_tlv *)ptr;
	tlv_tag = FIELD_GET(WMI_TLV_TAG, tlv->header);
	ptr += sizeof(*tlv);

	if (tlv_tag == WMI_CTRL_PATH_STATS_EV_FIXED_PARAM) {
		fixed_param = (struct wmi_ctrl_path_stats_ev_param *)ptr;
	} else {
		ath11k_warn(ab, "wmi ctrl Stats received without fixed param tlv at start\n");
	}

	ret = ath11k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath11k_wmi_ctrl_stats_event_parser,
				  &param);
	if (ret) {
		ath11k_warn(ab, "failed to parse wmi_ctrl_path_stats tlv: %d\n", ret);
		goto free;
	}

	ar = param.ar;
	if (!ar)
		return;

	list_splice_tail_init(&param.list, &ar->debug.wmi_list);

	if (!fixed_param->more) {
		complete(&ar->debug.wmi_ctrl_path_stats_rcvd);
		ath11k_dbg(ab, ATH11K_DBG_WMI,"wmi ctrl path stats completed");
	}

free:
	ath11k_wmi_crl_path_stats_list_free(&param.list);
}


static void ath11k_qos_null_compl_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct wmi_tx_compl_event tx_compl_param = {0};
	struct ath11k *ar;

	if (ath11k_pull_tx_compl_param_tlv(ab, skb, &tx_compl_param,
					   WMI_TAG_QOS_NULL_FRAME_TX_STATUS) != 0) {
		ath11k_warn(ab, "failed to extract qos null tx compl event");
		return;
	}

	rcu_read_lock();
	ar = ath11k_mac_get_ar_by_pdev_id(ab, tx_compl_param.pdev_id);
	if (!ar) {
		ath11k_warn(ab, "invalid pdev id %d in qos_null_tx_compl_event\n",
			    tx_compl_param.pdev_id);
		goto exit;
	}

	wmi_process_tx_comp(ar, &tx_compl_param);

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "QOS null tx compl ev pdev_id %d, desc_id %d, status %d",
		   tx_compl_param.pdev_id, tx_compl_param.desc_id,
		   tx_compl_param.status);

exit:
	rcu_read_unlock();
}

static struct ath11k *ath11k_get_ar_on_scan_state(struct ath11k_base *ab,
						  u32 vdev_id,
						  enum ath11k_scan_state state)
{
	int i;
	struct ath11k_pdev *pdev;
	struct ath11k *ar;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = rcu_dereference(ab->pdevs_active[i]);
		if (pdev && pdev->ar) {
			ar = pdev->ar;

			spin_lock_bh(&ar->data_lock);
			if (ar->scan.state == state &&
			    ar->scan.vdev_id == vdev_id) {
				spin_unlock_bh(&ar->data_lock);
				return ar;
			}
			spin_unlock_bh(&ar->data_lock);
		}
	}
	return NULL;
}

static void ath11k_scan_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct ath11k *ar;
	struct wmi_scan_event scan_ev = {0};

	if (ath11k_pull_scan_ev(ab, skb, &scan_ev) != 0) {
		ath11k_warn(ab, "failed to extract scan event");
		return;
	}

	rcu_read_lock();

	/* In case the scan was cancelled, ex. during interface teardown,
	 * the interface will not be found in active interfaces.
	 * Rather, in such scenarios, iterate over the active pdev's to
	 * search 'ar' if the corresponding 'ar' scan is ABORTING and the
	 * aborting scan's vdev id matches this event info.
	 */
	if (scan_ev.event_type == WMI_SCAN_EVENT_COMPLETED &&
	    scan_ev.reason == WMI_SCAN_REASON_CANCELLED) {
		ar = ath11k_get_ar_on_scan_state(ab, scan_ev.vdev_id,
						 ATH11K_SCAN_ABORTING);
		if (!ar)
			ar = ath11k_get_ar_on_scan_state(ab, scan_ev.vdev_id,
							 ATH11K_SCAN_RUNNING);
	} else {
		ar = ath11k_mac_get_ar_by_vdev_id(ab, scan_ev.vdev_id);
	}

	if (!ar) {
		ath11k_warn(ab, "Received scan event for unknown vdev");
		rcu_read_unlock();
		return;
	}

	spin_lock_bh(&ar->data_lock);

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "event scan %s type %d reason %d freq %d req_id %d scan_id %d vdev_id %d state %s (%d)\n",
		   ath11k_wmi_event_scan_type_str(scan_ev.event_type, scan_ev.reason),
		   scan_ev.event_type, scan_ev.reason, scan_ev.channel_freq,
		   scan_ev.scan_req_id, scan_ev.scan_id, scan_ev.vdev_id,
		   ath11k_scan_state_str(ar->scan.state), ar->scan.state);

	switch (scan_ev.event_type) {
	case WMI_SCAN_EVENT_STARTED:
		ath11k_wmi_event_scan_started(ar);
		break;
	case WMI_SCAN_EVENT_COMPLETED:
		ath11k_wmi_event_scan_completed(ar);
		break;
	case WMI_SCAN_EVENT_BSS_CHANNEL:
		ath11k_wmi_event_scan_bss_chan(ar);
		break;
	case WMI_SCAN_EVENT_FOREIGN_CHAN:
		ath11k_wmi_event_scan_foreign_chan(ar, scan_ev.channel_freq);
		break;
	case WMI_SCAN_EVENT_START_FAILED:
		ath11k_warn(ab, "received scan start failure event\n");
		ath11k_wmi_event_scan_start_failed(ar);
		break;
	case WMI_SCAN_EVENT_DEQUEUED:
		__ath11k_mac_scan_finish(ar);
		break;
	case WMI_SCAN_EVENT_PREEMPTED:
	case WMI_SCAN_EVENT_RESTARTED:
	case WMI_SCAN_EVENT_FOREIGN_CHAN_EXIT:
	default:
		break;
	}

	spin_unlock_bh(&ar->data_lock);

	rcu_read_unlock();
}

static void ath11k_peer_sta_kickout_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct wmi_peer_sta_kickout_arg arg = {};
	struct ieee80211_sta *sta;
	struct ath11k_peer *peer;
	struct ath11k *ar;
	u32 vdev_id;

	if (ath11k_pull_peer_sta_kickout_ev(ab, skb, &arg) != 0) {
		ath11k_warn(ab, "failed to extract peer sta kickout event");
		return;
	}

	rcu_read_lock();

	spin_lock_bh(&ab->base_lock);

	peer = ath11k_peer_find_by_addr(ab, arg.mac_addr);

	if (!peer) {
		ath11k_warn(ab, "peer not found %pM\n",
			    arg.mac_addr);
		spin_unlock_bh(&ab->base_lock);
		goto exit;
	}

	vdev_id = peer->vdev_id;

	spin_unlock_bh(&ab->base_lock);

	ar = ath11k_mac_get_ar_by_vdev_id(ab, vdev_id);
	if (!ar) {
		ath11k_warn(ab, "invalid vdev id in peer sta kickout ev %d",
			    peer->vdev_id);
		goto exit;
	}

	sta = ieee80211_find_sta_by_ifaddr(ar->hw,
					   arg.mac_addr, NULL);
	if (!sta) {
		ath11k_warn(ab, "Spurious quick kickout for STA %pM\n",
			    arg.mac_addr);
		goto exit;
	}

	if (peer->peer_logging_enabled)
		ath11k_dbg(ab, ATH11K_DBG_PEER, "wmi sta kickout event for %pM\n",
			   sta->addr);
	ieee80211_report_low_ack(sta, 10);

exit:
	rcu_read_unlock();
}

static void ath11k_roam_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct wmi_roam_event roam_ev = {};
	struct ath11k *ar;

	if (ath11k_pull_roam_ev(ab, skb, &roam_ev) != 0) {
		ath11k_warn(ab, "failed to extract roam event");
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "event roam vdev %u reason 0x%08x rssi %d\n",
		   roam_ev.vdev_id, roam_ev.reason, roam_ev.rssi);

	rcu_read_lock();
	ar = ath11k_mac_get_ar_by_vdev_id(ab, roam_ev.vdev_id);
	if (!ar) {
		ath11k_warn(ab, "invalid vdev id in roam ev %d",
			    roam_ev.vdev_id);
		rcu_read_unlock();
		return;
	}

	if (roam_ev.reason >= WMI_ROAM_REASON_MAX)
		ath11k_warn(ab, "ignoring unknown roam event reason %d on vdev %i\n",
			    roam_ev.reason, roam_ev.vdev_id);

	switch (roam_ev.reason) {
	case WMI_ROAM_REASON_BEACON_MISS:
		ath11k_mac_handle_beacon_miss(ar, roam_ev.vdev_id);
		break;
	case WMI_ROAM_REASON_BETTER_AP:
	case WMI_ROAM_REASON_LOW_RSSI:
	case WMI_ROAM_REASON_SUITABLE_AP_FOUND:
	case WMI_ROAM_REASON_HO_FAILED:
		ath11k_warn(ab, "ignoring not implemented roam event reason %d on vdev %i\n",
			    roam_ev.reason, roam_ev.vdev_id);
		break;
	}

	rcu_read_unlock();
}

static void ath11k_chan_info_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct wmi_chan_info_event ch_info_ev = {0};
	struct ath11k *ar;
	struct survey_info *survey;
	int idx;
	/* HW channel counters frequency value in hertz */
	u32 cc_freq_hz = ab->cc_freq_hz;

	if (ath11k_pull_chan_info_ev(ab, skb, &ch_info_ev) != 0) {
		ath11k_warn(ab, "failed to extract chan info event");
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "event chan info vdev_id %d err_code %d freq %d cmd_flags %d noise_floor %d rx_clear_count %d cycle_count %d mac_clk_mhz %d\n",
		   ch_info_ev.vdev_id, ch_info_ev.err_code, ch_info_ev.freq,
		   ch_info_ev.cmd_flags, ch_info_ev.noise_floor,
		   ch_info_ev.rx_clear_count, ch_info_ev.cycle_count,
		   ch_info_ev.mac_clk_mhz);

	if (ch_info_ev.cmd_flags == WMI_CHAN_INFO_END_RESP) {
		ath11k_dbg(ab, ATH11K_DBG_WMI, "chan info report completed\n");
		return;
	}

	rcu_read_lock();
	ar = ath11k_mac_get_ar_by_vdev_id(ab, ch_info_ev.vdev_id);
	if (!ar) {
		ath11k_warn(ab, "invalid vdev id in chan info ev %d",
			    ch_info_ev.vdev_id);
		rcu_read_unlock();
		return;
	}
	spin_lock_bh(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH11K_SCAN_IDLE:
	case ATH11K_SCAN_STARTING:
		ath11k_warn(ab, "received chan info event without a scan request, ignoring\n");
		goto exit;
	case ATH11K_SCAN_RUNNING:
	case ATH11K_SCAN_ABORTING:
		break;
	}

	idx = freq_to_idx(ar, ch_info_ev.freq);
	if (idx >= ARRAY_SIZE(ar->survey)) {
		ath11k_warn(ab, "chan info: invalid frequency %d (idx %d out of bounds)\n",
			    ch_info_ev.freq, idx);
		goto exit;
	}

	/* If FW provides MAC clock frequency in Mhz, overriding the initialized
	 * HW channel counters frequency value
	 */
	if (ch_info_ev.mac_clk_mhz)
		cc_freq_hz = (ch_info_ev.mac_clk_mhz * 1000);

	if (ch_info_ev.cmd_flags == WMI_CHAN_INFO_START_RESP) {
		survey = &ar->survey[idx];
		memset(survey, 0, sizeof(*survey));
		survey->noise = ch_info_ev.noise_floor;
		survey->filled = SURVEY_INFO_NOISE_DBM | SURVEY_INFO_TIME |
				 SURVEY_INFO_TIME_BUSY;
		survey->time = div_u64(ch_info_ev.cycle_count, cc_freq_hz);
		survey->time_busy = div_u64(ch_info_ev.rx_clear_count, cc_freq_hz);
		if (survey->time)
			ar->hw->medium_busy = div_u64((survey->time_busy * 100),
						      survey->time);
	}
exit:
	spin_unlock_bh(&ar->data_lock);
	rcu_read_unlock();
}

static void
ath11k_pdev_bss_chan_info_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct wmi_pdev_bss_chan_info_event bss_ch_info_ev = {};
	struct survey_info *survey;
	struct ath11k *ar;
	u32 cc_freq_hz = ab->cc_freq_hz;
	u64 busy, total, tx, rx, rx_bss;
	int idx;

	if (ath11k_pull_pdev_bss_chan_info_ev(ab, skb, &bss_ch_info_ev) != 0) {
		ath11k_warn(ab, "failed to extract pdev bss chan info event");
		return;
	}

	busy = (u64)(bss_ch_info_ev.rx_clear_count_high) << 32 |
			bss_ch_info_ev.rx_clear_count_low;

	total = (u64)(bss_ch_info_ev.cycle_count_high) << 32 |
			bss_ch_info_ev.cycle_count_low;

	tx = (u64)(bss_ch_info_ev.tx_cycle_count_high) << 32 |
			bss_ch_info_ev.tx_cycle_count_low;

	rx = (u64)(bss_ch_info_ev.rx_cycle_count_high) << 32 |
			bss_ch_info_ev.rx_cycle_count_low;

	rx_bss = (u64)(bss_ch_info_ev.rx_bss_cycle_count_high) << 32 |
			bss_ch_info_ev.rx_bss_cycle_count_low;

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "event pdev bss chan info:\n pdev_id: %d freq: %d noise: %d cycle: busy %llu total %llu tx %llu rx %llu rx_bss %llu\n",
		   bss_ch_info_ev.pdev_id, bss_ch_info_ev.freq,
		   bss_ch_info_ev.noise_floor, busy, total,
		   tx, rx, rx_bss);

	rcu_read_lock();
	ar = ath11k_mac_get_ar_by_pdev_id(ab, bss_ch_info_ev.pdev_id);

	if (!ar) {
		ath11k_warn(ab, "invalid pdev id %d in bss_chan_info event\n",
			    bss_ch_info_ev.pdev_id);
		rcu_read_unlock();
		return;
	}

	spin_lock_bh(&ar->data_lock);
	idx = freq_to_idx(ar, bss_ch_info_ev.freq);
	if (idx >= ARRAY_SIZE(ar->survey)) {
		ath11k_warn(ab, "bss chan info: invalid frequency %d (idx %d out of bounds)\n",
			    bss_ch_info_ev.freq, idx);
		goto exit;
	}

	survey = &ar->survey[idx];

	survey->noise     = bss_ch_info_ev.noise_floor;
	survey->time      = div_u64(total, cc_freq_hz);
	survey->time_busy = div_u64(busy, cc_freq_hz);
	survey->time_rx   = div_u64(rx_bss, cc_freq_hz);
	survey->time_tx   = div_u64(tx, cc_freq_hz);
	survey->filled   |= (SURVEY_INFO_NOISE_DBM |
			     SURVEY_INFO_TIME |
			     SURVEY_INFO_TIME_BUSY |
			     SURVEY_INFO_TIME_RX |
			     SURVEY_INFO_TIME_TX);
exit:
	spin_unlock_bh(&ar->data_lock);
	complete(&ar->bss_survey_done);

	rcu_read_unlock();
}

static void ath11k_vdev_install_key_compl_event(struct ath11k_base *ab,
						struct sk_buff *skb)
{
	struct wmi_vdev_install_key_complete_arg install_key_compl = {0};
	struct ath11k *ar;

	if (ath11k_pull_vdev_install_key_compl_ev(ab, skb, &install_key_compl) != 0) {
		ath11k_warn(ab, "failed to extract install key compl event");
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "event vdev install key ev idx %d flags %08x macaddr %pM status %d\n",
		   install_key_compl.key_idx, install_key_compl.key_flags,
		   install_key_compl.macaddr, install_key_compl.status);

	rcu_read_lock();
	ar = ath11k_mac_get_ar_by_vdev_id(ab, install_key_compl.vdev_id);
	if (!ar) {
		ath11k_warn(ab, "invalid vdev id in install key compl ev %d",
			    install_key_compl.vdev_id);
		rcu_read_unlock();
		return;
	}

	ar->install_key_status = 0;

	if (install_key_compl.status != WMI_VDEV_INSTALL_KEY_COMPL_STATUS_SUCCESS) {
		ath11k_warn(ab, "install key failed for %pM status %d\n",
			    install_key_compl.macaddr, install_key_compl.status);
		ar->install_key_status = install_key_compl.status;
	}

	complete(&ar->install_key_done);
	rcu_read_unlock();
}

static int  ath11k_wmi_tlv_services_parser(struct ath11k_base *ab,
					   u16 tag, u16 len,
					   const void *ptr, void *data)
{
	const struct wmi_service_available_event *ev;
	u32 *wmi_ext2_service_bitmap;
	int i, j;

	switch (tag) {
	case WMI_TAG_SERVICE_AVAILABLE_EVENT:
		ev = (struct wmi_service_available_event *)ptr;
		for (i = 0, j = WMI_MAX_SERVICE;
			i < WMI_SERVICE_SEGMENT_BM_SIZE32 && j < WMI_MAX_EXT_SERVICE;
			i++) {
			do {
				if (ev->wmi_service_segment_bitmap[i] &
				    BIT(j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32))
					set_bit(j, ab->wmi_ab.svc_map);
			} while (++j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32);
		}

		ath11k_dbg(ab, ATH11K_DBG_WMI,
			   "wmi_ext_service_bitmap 0:0x%04x, 1:0x%04x, 2:0x%04x, 3:0x%04x",
			   ev->wmi_service_segment_bitmap[0],
			   ev->wmi_service_segment_bitmap[1],
			   ev->wmi_service_segment_bitmap[2],
			   ev->wmi_service_segment_bitmap[3]);
		break;
	case WMI_TAG_ARRAY_UINT32:
		wmi_ext2_service_bitmap = (u32 *)ptr;
		for (i = 0, j = WMI_MAX_EXT_SERVICE;
			i < WMI_SERVICE_SEGMENT_BM_SIZE32 && j < WMI_MAX_EXT2_SERVICE;
			i++) {
			do {
				if (wmi_ext2_service_bitmap[i] &
				    BIT(j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32))
					set_bit(j, ab->wmi_ab.svc_map);
			} while (++j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32);
		}

		ath11k_dbg(ab, ATH11K_DBG_WMI,
			   "wmi_ext2_service__bitmap  0:0x%04x, 1:0x%04x, 2:0x%04x, 3:0x%04x",
			   wmi_ext2_service_bitmap[0], wmi_ext2_service_bitmap[1],
			   wmi_ext2_service_bitmap[2], wmi_ext2_service_bitmap[3]);
		break;
	}
	return 0;
}

static void ath11k_service_available_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	int ret;

	ret = ath11k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath11k_wmi_tlv_services_parser,
				  NULL);
	if (ret)
		ath11k_warn(ab, "failed to parse services available tlv %d\n", ret);

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event service available");
}

static void ath11k_peer_assoc_conf_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct wmi_peer_assoc_conf_arg peer_assoc_conf = {0};
	struct ath11k *ar;

	if (ath11k_pull_peer_assoc_conf_ev(ab, skb, &peer_assoc_conf) != 0) {
		ath11k_warn(ab, "failed to extract peer assoc conf event");
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "event peer assoc conf ev vdev id %d macaddr %pM\n",
		   peer_assoc_conf.vdev_id, peer_assoc_conf.macaddr);

	rcu_read_lock();
	ar = ath11k_mac_get_ar_by_vdev_id(ab, peer_assoc_conf.vdev_id);

	if (!ar) {
		ath11k_warn(ab, "invalid vdev id in peer assoc conf ev %d",
			    peer_assoc_conf.vdev_id);
		rcu_read_unlock();
		return;
	}

	complete(&ar->peer_assoc_done);
	rcu_read_unlock();
}

static void ath11k_update_stats_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct ath11k_fw_stats stats = {};
	struct ath11k *ar;
	int ret;

	INIT_LIST_HEAD(&stats.pdevs);
	INIT_LIST_HEAD(&stats.vdevs);
	INIT_LIST_HEAD(&stats.bcn);

	ret = ath11k_wmi_pull_fw_stats(ab, skb, &stats);
	if (ret) {
		ath11k_warn(ab, "failed to pull fw stats: %d\n", ret);
		goto free;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event update stats");

	rcu_read_lock();
	ar = ath11k_mac_get_ar_by_pdev_id(ab, stats.pdev_id);
	if (!ar) {
		rcu_read_unlock();
		ath11k_warn(ab, "failed to get ar for pdev_id %d: %d\n",
			    stats.pdev_id, ret);
		goto free;
	}

	spin_lock_bh(&ar->data_lock);

	/* WMI_REQUEST_PDEV_STAT can be requested via .get_txpower mac ops or via
	 * debugfs fw stats. Therefore, processing it separately.
	 */
	if (stats.stats_id == WMI_REQUEST_PDEV_STAT) {
		list_splice_tail_init(&stats.pdevs, &ar->fw_stats.pdevs);
		ar->fw_stats_done = true;
		goto complete;
	}

	/* WMI_REQUEST_VDEV_STAT, WMI_REQUEST_BCN_STAT and WMI_REQUEST_RSSI_PER_CHAIN_STAT
	 * are currently requested only via debugfs fw stats. Hence, processing these
	 * in debugfs context
	 */
	ath11k_debugfs_fw_stats_process(ar, &stats);

complete:
	complete(&ar->fw_stats_complete);
	rcu_read_unlock();
	spin_unlock_bh(&ar->data_lock);

	/* Since the stats's pdev, vdev and beacon list are spliced and reinitialised
	 * at this point, no need to free the individual list.
	 */
	return;

free:
	ath11k_fw_stats_free(&stats);
}

/* PDEV_CTL_FAILSAFE_CHECK_EVENT is received from FW when the frequency scanned
 * is not part of BDF CTL(Conformance test limits) table entries.
 */
static void ath11k_pdev_ctl_failsafe_check_event(struct ath11k_base *ab,
						 struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_pdev_ctl_failsafe_chk_event *ev;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_PDEV_CTL_FAILSAFE_CHECK_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch pdev ctl failsafe check ev");
		kfree(tb);
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "event pdev ctl failsafe check status %d\n",
		   ev->ctl_failsafe_status);

	/* If ctl_failsafe_status is set to 1 FW will max out the Transmit power
	 * to 10 dBm else the CTL power entry in the BDF would be picked up.
	 */
	if (ev->ctl_failsafe_status != 0)
		ath11k_warn(ab, "pdev ctl failsafe failure status %d",
			    ev->ctl_failsafe_status);

	kfree(tb);
}

static void
ath11k_wmi_process_csa_switch_count_event(struct ath11k_base *ab,
					  const struct wmi_pdev_csa_switch_ev *ev,
					  const u32 *vdev_ids)
{
	int i;
	struct ath11k_vif *arvif;

	/* Finish CSA once the switch count becomes NULL */
	if (ev->current_switch_count)
		return;

	rcu_read_lock();
	for (i = 0; i < ev->num_vdevs; i++) {
		arvif = ath11k_mac_get_arvif_by_vdev_id(ab, vdev_ids[i]);

		if (!arvif) {
			ath11k_warn(ab, "Recvd csa status for unknown vdev %d",
				    vdev_ids[i]);
			continue;
		}

		if (arvif->is_up && arvif->vif->bss_conf.csa_active)
			ieee80211_csa_finish(arvif->vif, 0);
	}
	rcu_read_unlock();
}

static void
ath11k_wmi_pdev_csa_switch_count_status_event(struct ath11k_base *ab,
					      struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_pdev_csa_switch_ev *ev;
	const u32 *vdev_ids;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_PDEV_CSA_SWITCH_COUNT_STATUS_EVENT];
	vdev_ids = tb[WMI_TAG_ARRAY_UINT32];

	if (!ev || !vdev_ids) {
		ath11k_warn(ab, "failed to fetch pdev csa switch count ev");
		kfree(tb);
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "event pdev csa switch count %d for pdev %d, num_vdevs %d",
		   ev->current_switch_count, ev->pdev_id,
		   ev->num_vdevs);

	ath11k_wmi_process_csa_switch_count_event(ab, ev, vdev_ids);

	kfree(tb);
}

int ath11k_wmi_pdev_get_ani_level(struct ath11k *ar, u32 cmd_id, u8 pdev_id)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_pdev_get_ani_level_cmd *cmd = NULL;
	struct sk_buff *skb;
	int ret;
	enum wmi_tlv_tag temp_tlv_tag;

	switch(cmd_id) {
	case WMI_PDEV_GET_ANI_OFDM_CONFIG_CMDID:
		temp_tlv_tag = WMI_TAG_PDEV_GET_ANI_OFDM_CONFIG_CMD;
		break;
	case WMI_PDEV_GET_ANI_CCK_CONFIG_CMDID:
		temp_tlv_tag = WMI_TAG_PDEV_GET_ANI_CCK_CONFIG_CMD;
		break;
	default:
		ath11k_warn(ar->ab, "Invalid cmd %d\n", cmd_id);
		return -EINVAL;
	}

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_get_ani_level_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, temp_tlv_tag) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->pdev_id = pdev_id;

	ret = ath11k_wmi_cmd_send(wmi, skb, cmd_id);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_PDEV_SET_PARAM cmd\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "WMI pdev pdev id %d\n", pdev_id);

	return ret;
}

static int ath11k_wmi_event_ani_ofdm_level(struct ath11k_base *ab,
					   struct sk_buff *skb)
{
	const struct wmi_pdev_ani_event *ev;

	ev = (struct wmi_pdev_ani_event *)skb->data;
	if (WARN_ON(skb->len < sizeof(*ev)))
		return -EPROTO;

	ab->ani_ofdm_level = ev->ani_level;
	complete(&ab->ani_ofdm_event);
	return 0;
}

static int ath11k_wmi_event_ani_cck_level(struct ath11k_base *ab,
					  struct sk_buff *skb)
{
	const struct wmi_pdev_ani_event *ev;

	ev = (struct wmi_pdev_ani_event *)skb->data;
	if (WARN_ON(skb->len < sizeof(*ev)))
		return -EPROTO;

	ab->ani_cck_level = ev->ani_level;
	complete(&(ab->ani_cck_event));
	return 0;
}

static void
ath11k_wmi_pdev_dfs_radar_detected_event(struct ath11k_base *ab, struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_pdev_radar_ev *ev;
	struct ath11k *ar;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_PDEV_DFS_RADAR_DETECTION_EVENT];

	if (!ev) {
		ath11k_warn(ab, "failed to fetch pdev dfs radar detected ev");
		kfree(tb);
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "event pdev dfs radar detected on pdev %d, detection mode %d, chan freq %d, chan_width %d, detector id %d, seg id %d, timestamp %d, chirp %d, freq offset %d, sidx %d",
		   ev->pdev_id, ev->detection_mode, ev->chan_freq, ev->chan_width,
		   ev->detector_id, ev->segment_id, ev->timestamp, ev->is_chirp,
		   ev->freq_offset, ev->sidx);

	rcu_read_lock();

	ar = ath11k_mac_get_ar_by_pdev_id(ab, ev->pdev_id);

	if (!ar) {
		ath11k_warn(ab, "radar detected in invalid pdev %d\n",
			    ev->pdev_id);
		goto exit;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_REG, "DFS Radar Detected in pdev %d\n",
		   ev->pdev_id);

	if (ar->dfs_block_radar_events)
		ath11k_info(ab, "DFS Radar detected, but ignored as requested\n");
	else if (ev->detector_id == 1)
		ath11k_mac_background_dfs_event(ar, ATH11K_BGDFS_RADAR);
	else
		ieee80211_radar_detected(ar->hw, NULL);

exit:
	rcu_read_unlock();

	kfree(tb);
}

static void
ath11k_wmi_pdev_temperature_event(struct ath11k_base *ab,
				  struct sk_buff *skb)
{
	struct ath11k *ar;
	const void **tb;
	const struct wmi_pdev_temperature_event *ev;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_PDEV_TEMPERATURE_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch pdev temp ev");
		kfree(tb);
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event pdev temperature ev temp %d pdev_id %d\n",
		   ev->temp, ev->pdev_id);

	rcu_read_lock();

	ar = ath11k_mac_get_ar_by_pdev_id(ab, ev->pdev_id);
	if (!ar) {
		ath11k_warn(ab, "invalid pdev id in pdev temperature ev %d", ev->pdev_id);
		goto exit;
	}

	ath11k_thermal_event_temperature(ar, ev->temp);

exit:
	rcu_read_unlock();

	kfree(tb);
}

static void ath11k_fils_discovery_event(struct ath11k_base *ab,
					struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_fils_discovery_event *ev;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab,
			    "failed to parse FILS discovery event tlv %d\n",
			    ret);
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event fils discovery");

	ev = tb[WMI_TAG_HOST_SWFDA_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch FILS discovery event\n");
		kfree(tb);
		return;
	}

	ath11k_warn(ab,
		    "FILS discovery frame expected from host for vdev_id: %u, transmission scheduled at %u, next TBTT: %u\n",
		    ev->vdev_id, ev->fils_tt, ev->tbtt);

	kfree(tb);
}

static void ath11k_probe_resp_tx_status_event(struct ath11k_base *ab,
					      struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_probe_resp_tx_status_event *ev;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab,
			    "failed to parse probe response transmission status event tlv: %d\n",
			    ret);
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event probe resp tx status");

	ev = tb[WMI_TAG_OFFLOAD_PRB_RSP_TX_STATUS_EVENT];
	if (!ev) {
		ath11k_warn(ab,
			    "failed to fetch probe response transmission status event");
		kfree(tb);
		return;
	}

	if (ev->tx_status)
		ath11k_warn(ab,
			    "Probe response transmission failed for vdev_id %u, status %u\n",
			    ev->vdev_id, ev->tx_status);

	kfree(tb);
}

static int ath11k_wmi_tlv_wow_wakeup_host_parse(struct ath11k_base *ab,
						u16 tag, u16 len,
						const void *ptr, void *data)
{
	struct wmi_wow_ev_arg *ev = data;
	const char *wow_pg_fault;
	int wow_pg_len;

	switch (tag) {
	case WMI_TAG_WOW_EVENT_INFO:
		memcpy(ev, ptr, sizeof(*ev));
		ath11k_dbg(ab, ATH11K_DBG_WMI, "wow wakeup host reason %d %s\n",
			   ev->wake_reason, wow_reason(ev->wake_reason));
		break;

	case WMI_TAG_ARRAY_BYTE:
		if (ev && ev->wake_reason == WOW_REASON_PAGE_FAULT) {
			wow_pg_fault = ptr;
			/* the first 4 bytes are length */
			wow_pg_len = *(int *)wow_pg_fault;
			wow_pg_fault += sizeof(int);
			ath11k_dbg(ab, ATH11K_DBG_WMI, "wow data_len = %d\n",
				   wow_pg_len);
			ath11k_dbg_dump(ab, ATH11K_DBG_WMI,
					"wow_event_info_type packet present",
					"wow_pg_fault ",
					wow_pg_fault,
					wow_pg_len);
		}
		break;
	default:
		break;
	}

	return 0;
}

static void ath11k_wmi_event_wow_wakeup_host(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct wmi_wow_ev_arg ev = { };
	int ret;

	ret = ath11k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath11k_wmi_tlv_wow_wakeup_host_parse,
				  &ev);
	if (ret) {
		ath11k_warn(ab, "failed to parse wmi wow tlv: %d\n", ret);
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event wow wakeup host");

	complete(&ab->wow.wakeup_completed);
}

static void
ath11k_wmi_diag_event(struct ath11k_base *ab,
		      struct sk_buff *skb)
{
	ath11k_dbg(ab, ATH11K_DBG_WMI, "event diag");

	trace_ath11k_wmi_diag(ab, skb->data, skb->len);
}

static const char *ath11k_wmi_twt_add_dialog_event_status(u32 status)
{
	switch (status) {
	case WMI_ADD_TWT_STATUS_OK:
		return "ok";
	case WMI_ADD_TWT_STATUS_TWT_NOT_ENABLED:
		return "twt disabled";
	case WMI_ADD_TWT_STATUS_USED_DIALOG_ID:
		return "dialog id in use";
	case WMI_ADD_TWT_STATUS_INVALID_PARAM:
		return "invalid parameters";
	case WMI_ADD_TWT_STATUS_NOT_READY:
		return "not ready";
	case WMI_ADD_TWT_STATUS_NO_RESOURCE:
		return "resource unavailable";
	case WMI_ADD_TWT_STATUS_NO_ACK:
		return "no ack";
	case WMI_ADD_TWT_STATUS_NO_RESPONSE:
		return "no response";
	case WMI_ADD_TWT_STATUS_DENIED:
		return "denied";
	case WMI_ADD_TWT_STATUS_UNKNOWN_ERROR:
		fallthrough;
	default:
		return "unknown error";
	}
}

static void ath11k_wmi_twt_add_dialog_event(struct ath11k_base *ab,
					    struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_twt_add_dialog_event *ev;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab,
			    "failed to parse wmi twt add dialog status event tlv: %d\n",
			    ret);
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event twt add dialog");

	ev = tb[WMI_TAG_TWT_ADD_DIALOG_COMPLETE_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch twt add dialog wmi event\n");
		goto exit;
	}

	if (ev->status)
		ath11k_warn(ab,
			    "wmi add twt dialog event vdev %d dialog id %d status %s\n",
			    ev->vdev_id, ev->dialog_id,
			    ath11k_wmi_twt_add_dialog_event_status(ev->status));

exit:
	kfree(tb);
}

static void ath11k_wmi_gtk_offload_status_event(struct ath11k_base *ab,
						struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_gtk_offload_status_event *ev;
	struct ath11k_vif *arvif;
	__be64 replay_ctr_be;
	u64    replay_ctr;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_GTK_OFFLOAD_STATUS_EVENT];
	if (!ev) {
		ath11k_warn(ab, "failed to fetch gtk offload status ev");
		kfree(tb);
		return;
	}

	rcu_read_lock();

	arvif = ath11k_mac_get_arvif_by_vdev_id(ab, ev->vdev_id);
	if (!arvif) {
		ath11k_warn(ab, "failed to get arvif for vdev_id:%d\n",
			    ev->vdev_id);
		goto exit;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI, "event gtk offload refresh_cnt %d\n",
		   ev->refresh_cnt);
	ath11k_dbg_dump(ab, ATH11K_DBG_WMI, "replay_cnt",
			NULL, ev->replay_ctr.counter, GTK_REPLAY_COUNTER_BYTES);

	replay_ctr =  ev->replay_ctr.word1;
	replay_ctr = (replay_ctr << 32) | ev->replay_ctr.word0;
	arvif->rekey_data.replay_ctr = replay_ctr;

	/* supplicant expects big-endian replay counter */
	replay_ctr_be = cpu_to_be64(replay_ctr);

	ieee80211_gtk_rekey_notify(arvif->vif, arvif->bssid,
				   (void *)&replay_ctr_be, GFP_ATOMIC);
exit:
	rcu_read_unlock();

	kfree(tb);
}

static void ath11k_wmi_p2p_noa_event(struct ath11k_base *ab,
				     struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_p2p_noa_event *ev;
	const struct ath11k_wmi_p2p_noa_info *noa;
	struct ath11k *ar;
	int vdev_id;
	u8 noa_descriptors;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ath11k_warn(ab, "failed to parse tlv: %ld\n", PTR_ERR(tb));
		return;
	}

	ev = tb[WMI_TAG_P2P_NOA_EVENT];
	noa = tb[WMI_TAG_P2P_NOA_INFO];

	if (!ev || !noa)
		goto out;

	vdev_id = ev->vdev_id;
	noa_descriptors = u32_get_bits(noa->noa_attr,
				       WMI_P2P_NOA_INFO_DESC_NUM);

	if (noa_descriptors > WMI_P2P_MAX_NOA_DESCRIPTORS) {
		ath11k_warn(ab, "invalid descriptor num %d in P2P NoA event\n",
			    noa_descriptors);
		goto out;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "wmi tlv p2p noa vdev_id %i descriptors %u\n",
		   vdev_id, noa_descriptors);

	rcu_read_lock();
	ar = ath11k_mac_get_ar_by_vdev_id(ab, vdev_id);
	if (!ar) {
		ath11k_warn(ab, "invalid vdev id %d in P2P NoA event\n",
			    vdev_id);
		goto unlock;
	}

	ath11k_p2p_noa_update_by_vdev_id(ar, vdev_id, noa);

unlock:
	rcu_read_unlock();
out:
	kfree(tb);
}

static int ath11k_tpc_get_reg_pwr(struct ath11k_base *ab,
				  struct wmi_tpc_stats_event *tpc_stats,
				  struct wmi_max_reg_power_allowed *ev)
{
	struct wmi_max_reg_power_allowed *reg_pwr = NULL;

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "Received reg power array type %d length %d for tpc stats\n",
		   ev->reg_power_type, ev->reg_array_len);

	switch (ev->reg_power_type) {
	case TPC_STATS_REG_PWR_ALLOWED_TYPE:
		reg_pwr = &tpc_stats->max_reg_allowed_power;
		break;
	default:
		goto out;
	}

	memcpy(reg_pwr, ev, sizeof(struct wmi_max_reg_power_allowed));

	reg_pwr->reg_pwr_array = kzalloc(reg_pwr->reg_array_len,
					 GFP_ATOMIC);
	if (!reg_pwr->reg_pwr_array)
		return -ENOMEM;

	tpc_stats->tlvs_rcvd |= WMI_TPC_REG_PWR_ALLOWED;
out:
	return 0;
}

static int ath11k_tpc_get_rate_array(struct ath11k_base *ab,
				     struct wmi_tpc_stats_event *tpc_stats,
				     struct wmi_tpc_rates_array *ev)
{
	u32 flag = 0;
	struct wmi_tpc_rates_array *rates_array = NULL;

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "Received rates array type %d length %d for tpc stats\n",
		   ev->rate_array_type, ev->rate_array_len);

	switch (ev->rate_array_type) {
	case ATH11K_TPC_STATS_RATES_ARRAY1:
		rates_array = &tpc_stats->rates_array1;
		flag = WMI_TPC_RATES_ARRAY1;
		break;
	case ATH11K_TPC_STATS_RATES_ARRAY2:
		rates_array = &tpc_stats->rates_array2;
		flag = WMI_TPC_RATES_ARRAY2;
		break;
	default:
		ath11k_warn(ab,
			    "Received invalid type of rates array for tpc stats\n");
		return -EINVAL;
	}
	memcpy(rates_array, ev, sizeof(struct wmi_tpc_rates_array));
	rates_array->rate_array = kzalloc(rates_array->rate_array_len,
					  GFP_ATOMIC);
	if (!rates_array->rate_array)
		return -ENOMEM;

	tpc_stats->tlvs_rcvd |= flag;
	return 0;
}

static int ath11k_tpc_get_ctl_pwr_tbl(struct ath11k_base *ab,
				      struct wmi_tpc_stats_event *tpc_stats,
				      struct wmi_tpc_ctl_pwr_table *ev)
{
	int total_size, ret = 0;
	u32 flag = 0;
	struct wmi_tpc_ctl_pwr_table *ctl_array = NULL;

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "Received ctl array type %d length %d for tpc stats\n",
		   ev->ctl_array_type, ev->ctl_array_len);

	switch (ev->ctl_array_type) {
	case ATH11K_TPC_STATS_CTL_ARRAY:
		ctl_array = &tpc_stats->ctl_array;
		flag = WMI_TPC_CTL_PWR_ARRAY;
		break;
	case ATH11K_TPC_STATS_CTL_160ARRAY:
		ctl_array = &tpc_stats->ctl_160array;
		flag = WMI_TPC_CTL_PWR_160ARRAY;
		break;
	default:
		ath11k_warn(ab,
			    "Received invalid type of ctl pwr table for tpc stats\n");
		return -EINVAL;
	}
	memcpy(ctl_array, ev, sizeof(struct wmi_tpc_ctl_pwr_table));
	total_size = (ctl_array->d1 * ctl_array->d2 *
		      ctl_array->d3 * ctl_array->d4);

	if (ctl_array->ctl_array_len != total_size) {
		ath11k_warn(ab,
			    "Total size and ctl_array_len doesn't match for tpc stats\n");
		return -EINVAL;
	}

	ctl_array->ctl_pwr_table = kzalloc(ctl_array->ctl_array_len,
					   GFP_ATOMIC);
	if (!ctl_array->ctl_pwr_table)
		return -ENOMEM;

	tpc_stats->tlvs_rcvd |= flag;
	return ret;
}

static int ath11k_wmi_tpc_stats_subtlv_parser(struct ath11k_base *ab,
					      u16 tag, u16 len,
					      const void *ptr, void *data)
{
	int ret = 0;
	struct wmi_tpc_stats_event *tpc_stats = (struct wmi_tpc_stats_event *)data;
	struct wmi_tpc_configs *tpc_config;
	struct wmi_max_reg_power_allowed *reg_pwr;
	struct wmi_tpc_rates_array *rates_array;
	struct wmi_tpc_ctl_pwr_table *ctl_tbl;

	if (!tpc_stats) {
		ath11k_warn(ab, "tpc stats memory unavailable\n");
		return -EINVAL;
	}

	switch (tag) {
	case WMI_TAG_TPC_STATS_CONFIG_EVENT:
		tpc_config = (struct wmi_tpc_configs *)ptr;
		memcpy(&tpc_stats->tpc_config, tpc_config,
		       sizeof(struct wmi_tpc_configs));
		break;

	case WMI_TAG_TPC_STATS_REG_PWR_ALLOWED:
		reg_pwr = (struct wmi_max_reg_power_allowed *)ptr;
		ret = ath11k_tpc_get_reg_pwr(ab, tpc_stats, reg_pwr);
		break;

	case WMI_TAG_TPC_STATS_RATES_ARRAY:
		rates_array = (struct wmi_tpc_rates_array *)ptr;
		ret = ath11k_tpc_get_rate_array(ab, tpc_stats, rates_array);
		break;

	case WMI_TAG_TPC_STATS_CTL_PWR_TABLE_EVENT:
		ctl_tbl = (struct wmi_tpc_ctl_pwr_table *)ptr;
		ret = ath11k_tpc_get_ctl_pwr_tbl(ab, tpc_stats, ctl_tbl);
		break;

	default:
		ath11k_warn(ab,
			    "Received invalid tag for tpc stats in subtlvs\n");
		return -EINVAL;
	}
	return ret;
}

static int ath11k_wmi_tpc_stats_copy_buffer(struct ath11k_base *ab,
					    const void *ptr, u16 tag, u16 len,
					    struct wmi_tpc_stats_event *tpc_stats)
{
	s16 *reg_rates_src;
	s8 *ctl_src;
	s16 *dst_ptr;
	s8 *dst_ptr_ctl;
	int ret = 0;

	if (tag == WMI_TAG_ARRAY_INT16)
		reg_rates_src = (s16 *)ptr;
	else
		ctl_src = (s8 *)ptr;

	switch (tpc_stats->event_count) {
	case ATH11K_TPC_STATS_CONFIG_REG_PWR_EVENT:
		if (tpc_stats->tlvs_rcvd & WMI_TPC_REG_PWR_ALLOWED) {
			dst_ptr = tpc_stats->max_reg_allowed_power.reg_pwr_array;
			memcpy(dst_ptr, reg_rates_src,
			       tpc_stats->max_reg_allowed_power.reg_array_len);
			reg_rates_src += tpc_stats->max_reg_allowed_power.reg_array_len;
		}
		break;
	case ATH11K_TPC_STATS_RATES_EVENT1:
		if (tpc_stats->tlvs_rcvd & WMI_TPC_RATES_ARRAY1) {
			dst_ptr = tpc_stats->rates_array1.rate_array;
			memcpy(dst_ptr, reg_rates_src,
			       tpc_stats->rates_array1.rate_array_len);
			reg_rates_src += tpc_stats->rates_array1.rate_array_len;
		}
		break;
	case ATH11K_TPC_STATS_RATES_EVENT2:
		if (tpc_stats->tlvs_rcvd & WMI_TPC_RATES_ARRAY2) {
			dst_ptr = tpc_stats->rates_array2.rate_array;
			memcpy(dst_ptr, reg_rates_src,
			       tpc_stats->rates_array2.rate_array_len);
			reg_rates_src += tpc_stats->rates_array2.rate_array_len;
		}
		break;
	case ATH11K_TPC_STATS_CTL_TABLE_EVENT:
		if (tpc_stats->tlvs_rcvd & WMI_TPC_CTL_PWR_ARRAY) {
			dst_ptr_ctl = tpc_stats->ctl_array.ctl_pwr_table;
			memcpy(dst_ptr_ctl, ctl_src,
			       tpc_stats->ctl_array.ctl_array_len);
			ctl_src += tpc_stats->ctl_array.ctl_array_len;
		}
		if (tpc_stats->tlvs_rcvd & WMI_TPC_CTL_PWR_160ARRAY) {
			dst_ptr_ctl = tpc_stats->ctl_160array.ctl_pwr_table;
			memcpy(dst_ptr_ctl, ctl_src,
			       tpc_stats->ctl_160array.ctl_array_len);
			ctl_src += tpc_stats->ctl_160array.ctl_array_len;
		}
		break;
	}
	return ret;
}

static int ath11k_wmi_tpc_stats_event_parser(struct ath11k_base *ab,
					     u16 tag, u16 len,
					     const void *ptr, void *data)
{
	struct wmi_tpc_stats_event *tpc_stats = (struct wmi_tpc_stats_event *)data;
	int ret = 0;

	ath11k_dbg(ab, ATH11K_DBG_WMI, "tpc stats tag 0x%x of len %d rcvd\n",
		   tag, len);
	switch (tag) {
	case WMI_TAG_TPC_STATS_EVENT_FIXED_PARAM:
		/* Fixed param is already processed*/
		break;

	case WMI_TAG_ARRAY_STRUCT:
		/* len 0 is expected for array of struct when there
		 * is no content of that type to pack inside that tlv
		 */
		if (len == 0)
			return 0;
		ret = ath11k_wmi_tlv_iter(ab, ptr, len,
					  ath11k_wmi_tpc_stats_subtlv_parser,
					  tpc_stats);
		break;

	case WMI_TAG_ARRAY_INT16:
		if (len == 0)
			return 0;
		ret = ath11k_wmi_tpc_stats_copy_buffer(ab, ptr,
						       WMI_TAG_ARRAY_INT16,
						       len, tpc_stats);
		break;

	case WMI_TAG_ARRAY_BYTE:
		if (len == 0)
			return 0;
		ret = ath11k_wmi_tpc_stats_copy_buffer(ab, ptr,
						       WMI_TAG_ARRAY_BYTE,
						       len, tpc_stats);
		break;

	default:
		ath11k_warn(ab, "Received invalid tag for tpc stats\n");
		ret = -EINVAL;
		break;
	}
	return ret;
}

void ath11k_wmi_free_tpc_stats_mem(struct ath11k *ar)
{
	struct wmi_tpc_stats_event *tpc_stats = ar->tpc_stats;

	lockdep_assert_held(&ar->data_lock);
	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "tpc stats mem free\n");
	if (tpc_stats) {
		kfree(tpc_stats->max_reg_allowed_power.reg_pwr_array);
		kfree(tpc_stats->rates_array1.rate_array);
		kfree(tpc_stats->rates_array2.rate_array);
		kfree(tpc_stats->ctl_array.ctl_pwr_table);
		kfree(tpc_stats->ctl_160array.ctl_pwr_table);
		kfree(tpc_stats);
		ar->tpc_stats = NULL;
	}
}

static struct ath11k *ath11k_wmi_tpc_process_fixed_param(struct ath11k_base *ab,
							 u8 *ptr, size_t len)
{
	struct ath11k *ar = NULL;
	const struct wmi_tlv *tlv;
	struct wmi_tpc_stats_event_fixed_param *fixed_param;
	struct wmi_tpc_stats_event *tpc_stats = NULL;
	u16 tlv_tag;

	if (!ptr) {
		ath11k_warn(ab, "No data present in tpc stats event\n");
		return NULL;
	}

	if (len < (sizeof(*fixed_param) + TLV_HDR_SIZE)) {
		ath11k_warn(ab, "tpc stats event size invalid\n");
		return NULL;
	}

	tlv = (struct wmi_tlv *)ptr;
	tlv_tag = FIELD_GET(WMI_TLV_TAG, tlv->header);
	ptr += sizeof(*tlv);

	if (tlv_tag == WMI_TAG_TPC_STATS_EVENT_FIXED_PARAM) {
		fixed_param = (struct wmi_tpc_stats_event_fixed_param *)ptr;
		ar = ath11k_mac_get_ar_by_pdev_id(ab, fixed_param->pdev_id);
		if (!ar) {
			ath11k_warn(ab, "Failed to get ar for tpc stats\n");
			return NULL;
		}
	} else {
		ath11k_warn(ab, "TPC Stats received without fixed param tlv at start\n");
		return NULL;
	}

	spin_lock_bh(&ar->data_lock);
	if (!ar->tpc_request) {
		/* Event is received either without request or the
		 * timeout, if memory is already allocated free it
		 */
		if (ar->tpc_stats) {
			ath11k_warn(ab, "Freeing memory for tpc_stats\n");
			ath11k_wmi_free_tpc_stats_mem(ar);
		}
		spin_unlock_bh(&ar->data_lock);
		return NULL;
	}

	if (fixed_param->event_count == 0) {
		if (ar->tpc_stats) {
			ath11k_warn(ab,
				    "Invalid tpc memory present\n");
			spin_unlock_bh(&ar->data_lock);
			return NULL;
		}
		ar->tpc_stats =
			    kzalloc(sizeof(struct wmi_tpc_stats_event),
				    GFP_ATOMIC);
	}

	if (!ar->tpc_stats) {
		ath11k_warn(ab,
			    "Failed to allocate memory for tpc stats\n");
		spin_unlock_bh(&ar->data_lock);
		return NULL;
	}

	tpc_stats = ar->tpc_stats;

	if (!(fixed_param->event_count == 0)) {
		if (fixed_param->event_count != tpc_stats->event_count + 1) {
			ath11k_warn(ab,
				    "Invalid tpc event received\n");
			spin_unlock_bh(&ar->data_lock);
			return NULL;
		}
	}
	tpc_stats->pdev_id = fixed_param->pdev_id;
	tpc_stats->event_count = fixed_param->event_count;
	tpc_stats->end_of_event = fixed_param->end_of_event;
	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "tpc stats event_count %d\n",
		   tpc_stats->event_count);

	spin_unlock_bh(&ar->data_lock);

	return ar;
}

static void ath11k_process_tpc_stats(struct ath11k_base *ab,
				     struct sk_buff *skb)
{
	int ret;
	struct ath11k *ar;
	struct wmi_tpc_stats_event *tpc_stats = NULL;

	rcu_read_lock();
	ar = ath11k_wmi_tpc_process_fixed_param(ab, skb->data, skb->len);
	if (!ar) {
		ath11k_warn(ab, "Failed to get ar for tpc event\n");
		goto exit;
	}

	spin_lock_bh(&ar->data_lock);
	tpc_stats = ar->tpc_stats;
	ret = ath11k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath11k_wmi_tpc_stats_event_parser,
				  tpc_stats);
	if (ret) {
		if (tpc_stats) {
			ath11k_warn(ab, "Freeing memory for tpc_stats\n");
			ath11k_wmi_free_tpc_stats_mem(ar);
		}
		spin_unlock_bh(&ar->data_lock);
		ath11k_warn(ab, "failed to parse tpc_stats tlv: %d\n", ret);
		goto exit;
	}

	if (tpc_stats && tpc_stats->end_of_event)
		complete(&ar->tpc_complete);

	spin_unlock_bh(&ar->data_lock);

exit:
	rcu_read_unlock();
}

int ath11k_wmi_pdev_get_tpc_table_cmdid(struct ath11k *ar)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_tpc_stats_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	spin_lock_bh(&ar->data_lock);
	ar->tpc_stats = NULL;
	spin_unlock_bh(&ar->data_lock);
	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;
	cmd = (struct wmi_tpc_stats_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_TPC_STATS_GET_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	cmd->param = WMI_TPC_CONFIG_PARAM;
	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PDEV_GET_TPC_STATS_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to submit WMI_PDEV_GET_TPC_STATS_CMDID\n");
		dev_kfree_skb(skb);
		return ret;
	}
	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "WMI get TPC STATS sent on pdev %d\n",
		   ar->pdev->pdev_id);

	return ret;
}

static void ath11k_wmi_wds_peer_event(struct ath11k_base *ab,
				      struct sk_buff *skb)
{
	struct wmi_wds_addr_arg wds_addr_arg = {0};

	if (ath11k_pull_wds_addr_ev(ab, skb, &wds_addr_arg) != 0) {
		ath11k_warn(ab, "failed to extract wds addr event");
		return;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "wds addr event vdev id %d peer macaddr %pM dst macaddr %pM\n",
		   wds_addr_arg.vdev_id, wds_addr_arg.peer_macaddr,
		   wds_addr_arg.dst_macaddr);
}

static void ath11k_wmi_tlv_cfr_cpature_event_fixed_param(const void *ptr,
							 void *data)
{
	struct ath11k_cfr_peer_tx_param *tx_params =
			(struct ath11k_cfr_peer_tx_param *)data;
	struct ath11k_wmi_cfr_peer_tx_event_param *params =
			(struct ath11k_wmi_cfr_peer_tx_event_param *)ptr;

	tx_params->capture_method = params->capture_method;
	tx_params->vdev_id = params->vdev_id;
	ether_addr_copy(tx_params->peer_mac_addr, params->mac_addr.addr);
	tx_params->primary_20mhz_chan = params->chan_mhz;
	tx_params->bandwidth = params->bandwidth;
	tx_params->phy_mode = params->phy_mode;
	tx_params->band_center_freq1 = params->band_center_freq1;
	tx_params->band_center_freq2 = params->band_center_freq2;
	tx_params->spatial_streams = params->sts_count;
	tx_params->correlation_info_1 = params->correlation_info_1;
	tx_params->correlation_info_2 = params->correlation_info_2;
	tx_params->status = params->status;
	tx_params->timestamp_us = params->timestamp_us;
	tx_params->counter = params->counter;
	memcpy(tx_params->chain_rssi, params->chain_rssi,
		 sizeof(tx_params->chain_rssi));

	if (WMI_CFR_CFO_MEASUREMENT_VALID & params->cfo_measurement)
		tx_params->cfo_measurement = FIELD_GET(WMI_CFR_CFO_MEASUREMENT_RAW_DATA,
						       params->cfo_measurement);
	else
		tx_params->cfo_measurement = 0;

	tx_params->rx_start_ts = params->rx_start_ts;
	tx_params->rx_ts_reset = params->rx_ts_reset;
}

static void ath11k_wmi_tlv_cfr_cpature_phase_fixed_param(const void *ptr,
							 void *data)
{
	struct ath11k_cfr_peer_tx_param *tx_params =
			(struct ath11k_cfr_peer_tx_param *)data;
	struct ath11k_wmi_cfr_peer_tx_event_phase_param *params =
			(struct ath11k_wmi_cfr_peer_tx_event_phase_param *)ptr;
	int i;

	for (i = 0; i < WMI_MAX_CHAINS; i++) {
		tx_params->chain_phase[i] = params->chain_phase[i];
		tx_params->agc_gain[i] = params->agc_gain[i];
	}
}

static int ath11k_wmi_tbtt_offset_subtlv_parser(struct ath11k_base *ab, u16 tag,
						u16 len, const void *ptr,
						void *data)
{
	int ret = 0;
	struct ath11k *ar;
	u64 tx_delay = 0;
	struct wmi_tbtt_offset_info *tbtt_offset_info;
	struct ieee80211_chanctx_conf *conf;
	struct ath11k_vif *arvif;

	tbtt_offset_info = (struct wmi_tbtt_offset_info *)ptr;

	rcu_read_lock();
	ar = ath11k_mac_get_ar_by_vdev_id(ab, tbtt_offset_info->vdev_id);
	if (!ar) {
		ath11k_warn(ab, "ar not found, vdev_id %d\n", tbtt_offset_info->vdev_id);
		ret = -EINVAL;
		goto exit;
	}

	arvif = ath11k_mac_get_arvif(ar, tbtt_offset_info->vdev_id);
	if (!arvif) {
		ath11k_warn(ab, "arvif not found, vdev_id %d\n",
			    tbtt_offset_info->vdev_id);
		ret = -EINVAL;
		goto exit;
	}

	if (!arvif->is_up || (arvif->vdev_type != WMI_VDEV_TYPE_AP)) {
		ret = 0;
		goto exit;
	}

	arvif->tbtt_offset = tbtt_offset_info->tbtt_offset;

	conf = rcu_dereference(arvif->vif->bss_conf.chanctx_conf);
	if (conf && conf->def.chan->band == NL80211_BAND_2GHZ) {
		/* 1Mbps Beacon: */
		/* 144 us ( LPREAMBLE) + 48 (PLCP Header)
		 * + 192 (1Mbps, 24 ytes)
		 * = 384 us + 2us(MAC/BB DELAY
		 */
		tx_delay = 386;
	} else if (conf && (conf->def.chan->band == NL80211_BAND_5GHZ ||
			    conf->def.chan->band == NL80211_BAND_6GHZ)) {
		/* 6Mbps Beacon: */
		/*20(lsig)+2(service)+32(6mbps, 24 bytes)
		 *= 54us + 2us(MAC/BB DELAY)
		 */
		tx_delay = 56;
	}
	arvif->tbtt_offset -= tx_delay;

	ieee80211_queue_work(ar->hw, &arvif->update_bcn_template_work);
exit:
	rcu_read_unlock();
	return ret;
}

static int ath11k_wmi_tbtt_offset_event_parser(struct ath11k_base *ab,
					       u16 tag, u16 len,
					       const void *ptr, void *data)
{
	int ret = 0;

	ath11k_dbg(ab, ATH11K_DBG_WMI, "wmi tbtt offset event tag 0x%x of len %d rcvd\n",
		   tag, len);

	if (tag == WMI_TAG_ARRAY_STRUCT) {
		ret = ath11k_wmi_tlv_iter(ab, ptr, len,
					  ath11k_wmi_tbtt_offset_subtlv_parser,
					  data);
	}

	return ret;
}

static int ath11k_wmi_pull_tbtt_offset(struct ath11k_base *ab, struct sk_buff *skb,
				       struct wmi_tbtt_offset_ev_arg *arg)
{
	struct wmi_tbtt_offset_event *ev = NULL;
	struct wmi_tbtt_offset_info tbtt_offset_info = {0};
	struct wmi_tlv *tlv;
	int ret;
	u8 *ptr;
	u16 tlv_tag;

	ptr = skb->data;

	if (skb->len < (sizeof(*ev) + TLV_HDR_SIZE)) {
		ath11k_warn(ab, "wmi_tbtt_offset event size invalid\n");
		return -EINVAL;
	}

	tlv = (struct wmi_tlv *)ptr;
	tlv_tag = FIELD_GET(WMI_TLV_TAG, tlv->header);
	ptr += sizeof(*tlv);

	if (tlv_tag == WMI_TAG_TBTT_OFFSET_EXT_EVENT) {
		ev = (struct wmi_tbtt_offset_event *)ptr;
	} else {
		ath11k_warn(ab, "tbtt event received with invalid tag\n");
		return -EINVAL;
	}

	ret = ath11k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath11k_wmi_tbtt_offset_event_parser,
				  &tbtt_offset_info);
	if (ret) {
		ath11k_warn(ab, "failed to parse tbtt tlv %d\n", ret);
		return -EINVAL;
	}
	return 0;
}

static int ath11k_wmi_tlv_cfr_capture_evt_parse(struct ath11k_base *ab,
						u16 tag, u16 len,
						const void *ptr, void *data)
{
	switch (tag) {
	case WMI_TAG_PEER_CFR_CAPTURE_EVENT:
		ath11k_wmi_tlv_cfr_cpature_event_fixed_param(ptr, data);
		break;
	case WMI_TAG_CFR_CAPTURE_PHASE_PARAM:
		ath11k_wmi_tlv_cfr_cpature_phase_fixed_param(ptr, data);
		break;
	default:
		ath11k_warn(ab, "Invalid tag received tag %d len %d\n",
		tag, len);
		return -EINVAL;
	}

	return 0;
}

static void ath11k_wmi_parse_cfr_capture_event(struct ath11k_base *ab,
					       struct sk_buff *skb)
{
	struct ath11k_cfr_peer_tx_param params = {};
	int ret;

	ath11k_dbg_dump(ab, ATH11K_DBG_CFR_DUMP, "cfr_dump:", "",
			skb->data, skb->len);

	ret = ath11k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath11k_wmi_tlv_cfr_capture_evt_parse,
				  &params);
	if (ret) {
		ath11k_warn(ab, "failed to parse cfr capture event tlv %d\n",
			    ret);
		return;
	}

	ret = ath11k_process_cfr_capture_event(ab, &params);
	if (ret)
		ath11k_dbg(ab, ATH11K_DBG_CFR,
			   "failed to process cfr cpature ret = %d\n", ret);
}

void ath11k_wmi_event_tbttoffset_update(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct wmi_tbtt_offset_ev_arg arg = {};
	int ret;

	ret = ath11k_wmi_pull_tbtt_offset(ab, skb, &arg);
	if (ret)
		ath11k_warn(ab, "failed to parse tbtt offset event: %d\n", ret);
}

static int ath11k_wmi_peer_ratecode_subtlv_parser(struct ath11k_base *ab,
						  u16 tag, u16 len,
						  const void *ptr, void *data)
{
	struct wmi_peer_cck_ofdm_rate_info *ofdm_rate;
	struct wmi_peer_mcs_rate_info *mcs_rate;
	struct ath11k_peer_rate_code_list_cap *rate_cap = data;
	int ret = 0;
	static int i = 0, j = 0;

	switch (tag) {
	case WMI_TAG_PEER_CCK_OFDM_RATE_INFO:
		ofdm_rate = (struct wmi_peer_cck_ofdm_rate_info *)ptr;
		if (i == ATH11K_SMART_ANT_LEGACY_RATE_WORDS)
			i = 0;
		rate_cap->rtcode_legacy[i] = ofdm_rate->ratecode_legacy;
		i++;
		break;
	case WMI_TAG_PEER_MCS_RATE_INFO:
		mcs_rate = (struct wmi_peer_mcs_rate_info *)ptr;
		if (j == ATH11K_SMART_ANT_MAX_HT_RATE_WORDS)
			j = 0;
		rate_cap->rtcode_20[j] = mcs_rate->rt_code_20;
		rate_cap->rtcode_40[j] = mcs_rate->rt_code_40;
		rate_cap->rtcode_80[j] = mcs_rate->rt_code_80;
		j++;
		break;
	default:
		ath11k_warn(ab,
			    "Received invalid tag for wmi peer ratecode in subtlvs\n");
		return -EINVAL;
		break;
	}

	return ret;
}

static int ath11k_wmi_peer_ratecode_event_parser(struct ath11k_base *ab,
						 u16 tag, u16 len,
						 const void *ptr, void *data)
{
	int ret = 0;

	ath11k_dbg(ab, ATH11K_DBG_WMI, "wmi peer ratecode event tag 0x%x of len %d rcvd\n",
		   tag, len);

	switch (tag) {
	case WMI_TAG_PEER_RATECODE_LIST_EVENT:
		/* Fixed param is already processed*/
		break;
	case WMI_TAG_ARRAY_STRUCT:
		 /* len 0 is expected for array of struct when there
		  * is no content of that type to pack inside that tlv
		  */
		if (len == 0)
			return 0;
		ret = ath11k_wmi_tlv_iter(ab, ptr, len,
					  ath11k_wmi_peer_ratecode_subtlv_parser,
					  data);
		break;
	default:
		ath11k_warn(ab, "Received invalid tag for wmi peer ratecode event\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void ath11k_wmi_event_peer_ratecode_list(struct ath11k_base *ab,
						struct sk_buff *skb)
{
	const struct wmi_peer_ratecode_list_fixed_param *fixed_param;
	const struct wmi_tlv *tlv;
	struct ath11k_peer *peer = NULL;
	struct ieee80211_sta *sta;
	struct ath11k_sta *arsta;
	u16 tlv_tag;
	u8 *ptr;
	int ret;

	ptr = skb->data;
	if (skb->len < (sizeof(*fixed_param) + TLV_HDR_SIZE)) {
		ath11k_warn(ab, "peer ratecode list event size invalid\n");
		return;
	}

	tlv = (struct wmi_tlv *)ptr;
	tlv_tag = FIELD_GET(WMI_TLV_TAG, tlv->header);
	ptr += sizeof(*tlv);

	if (tlv_tag == WMI_TAG_PEER_RATECODE_LIST_EVENT) {
		fixed_param = (struct wmi_peer_ratecode_list_fixed_param *)ptr;

		ath11k_dbg(ab, ATH11K_DBG_WMI,
			   "pdev peer ratecode list on pdev: %d of peer: %pM ratecount: %d\n",
			   fixed_param->pdev_id, fixed_param->macaddr.addr, fixed_param->ratecount);
	} else {
		ath11k_warn(ab, "peer ratecode list event received with wrong tag\n");
		return;
	}

	rcu_read_lock();
	spin_lock_bh(&ab->base_lock);

	peer = ath11k_peer_find_by_addr(ab, fixed_param->macaddr.addr);
	if (!peer) {
		ath11k_warn(ab, "peer not found %pM\n", fixed_param->macaddr.addr);
		spin_unlock_bh(&ab->base_lock);
		rcu_read_unlock();
		return;
	}

 	sta = peer->sta;

 	if (!sta) {
 		ath11k_warn(ab, "failed to find station entry %pM\n",
 			    fixed_param->macaddr.addr);
		spin_unlock_bh(&ab->base_lock);
		rcu_read_unlock();
 		return;
 	}

 	arsta = (struct ath11k_sta *)sta->drv_priv;

	spin_unlock_bh(&ab->base_lock);
	rcu_read_unlock();

	if (!arsta)
		return;

	if (!arsta->smart_ant_sta) {
		ath11k_dbg(ab, ATH11K_DBG_SMART_ANT,
			   "arsta->smart_ant_sta is null for the peer: %pM\n",
			   fixed_param->macaddr.addr);
 		return;
 	}

	ret = ath11k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath11k_wmi_peer_ratecode_event_parser,
				  &arsta->smart_ant_sta->rate_cap);
	if (ret) {
		ath11k_warn(ab, "failed to parse cck ofdm_rate tlv %d\n", ret);
		return;
	}

	ret = ath11k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath11k_wmi_peer_ratecode_event_parser,
				  &arsta->smart_ant_sta->rate_cap);
	if (ret) {
		ath11k_warn(ab, "failed to parse mcs_rate tlv %d\n", ret);
		return;
	}

	ether_addr_copy(arsta->smart_ant_sta->mac_addr, fixed_param->macaddr.addr);

	return;
}

static void ath11k_process_ocac_complete_event(struct ath11k_base *ab,
                struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_vdev_adfs_ocac_complete_event_fixed_param *ev;
	struct ath11k *ar;
	int ret;

	tb = ath11k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath11k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_VDEV_ADFS_OCAC_COMPLETE_EVENT];

	if (!ev) {
		ath11k_warn(ab, "failed to fetch ocac completed ev");
		goto exit;
	}

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "pdev dfs ocac complete event on vdev %d, "
		   "chan freq %d, chan_width %d, status %d "
		   "freq %d, freq1  %d, freq2 %d ",
		   ev->vdev_id, ev->chan_freq, ev->chan_width,
		   ev->status, ev->center_freq, ev->center_freq1,
		   ev->center_freq2);

	ar = ath11k_mac_get_ar_by_vdev_id(ab, ev->vdev_id);

	if (!ar) {
		ath11k_warn(ab, "OCAC complete event in invalid vdev %d\n",
			    ev->vdev_id);
		goto exit;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "aDFS ocac complete event in vdev %d\n",
		   ev->vdev_id);
	if (ev->status) {
		ath11k_mac_background_dfs_event(ar, ATH11K_BGDFS_ABORT);
	} else {
		memset(&ar->agile_chandef, 0, sizeof(struct cfg80211_chan_def));
		ar->agile_chandef.chan = NULL;
	}
exit:
	kfree(tb);
}

static void ath11k_wmi_tlv_op_rx(struct ath11k_base *ab, struct sk_buff *skb)
{
	struct wmi_cmd_hdr *cmd_hdr;
	enum wmi_tlv_event_id id;

	cmd_hdr = (struct wmi_cmd_hdr *)skb->data;
	id = FIELD_GET(WMI_CMD_HDR_CMD_ID, (cmd_hdr->cmd_id));

	trace_ath11k_wmi_event(ab, id, skb->data, skb->len);

	if (skb_pull(skb, sizeof(struct wmi_cmd_hdr)) == NULL)
		goto out;

	switch (id) {
		/* Process all the WMI events here */
	case WMI_SERVICE_READY_EVENTID:
		ath11k_service_ready_event(ab, skb);
		break;
	case WMI_SERVICE_READY_EXT_EVENTID:
		ath11k_service_ready_ext_event(ab, skb);
		break;
	case WMI_SERVICE_READY_EXT2_EVENTID:
		ath11k_service_ready_ext2_event(ab, skb);
		break;
	case WMI_REG_CHAN_LIST_CC_EVENTID:
		ath11k_reg_chan_list_event(ab, skb, WMI_REG_CHAN_LIST_CC_ID);
		break;
	case WMI_REG_CHAN_LIST_CC_EXT_EVENTID:
		ath11k_reg_chan_list_event(ab, skb, WMI_REG_CHAN_LIST_CC_EXT_ID);
		break;
	case WMI_READY_EVENTID:
		ath11k_ready_event(ab, skb);
		break;
	case WMI_PEER_DELETE_RESP_EVENTID:
		ath11k_peer_delete_resp_event(ab, skb);
		break;
	case WMI_VDEV_START_RESP_EVENTID:
		ath11k_vdev_start_resp_event(ab, skb);
		break;
	case WMI_OFFLOAD_BCN_TX_STATUS_EVENTID:
		ath11k_bcn_tx_status_event(ab, skb);
		break;
	case WMI_VDEV_STOPPED_EVENTID:
		ath11k_vdev_stopped_event(ab, skb);
		break;
	case WMI_MGMT_RX_EVENTID:
		ath11k_mgmt_rx_event(ab, skb);
		/* mgmt_rx_event() owns the skb now! */
		return;
	case WMI_MGMT_TX_COMPLETION_EVENTID:
		ath11k_mgmt_tx_compl_event(ab, skb);
		break;
	case WMI_SCAN_EVENTID:
		ath11k_scan_event(ab, skb);
		break;
	case WMI_PEER_STA_KICKOUT_EVENTID:
		ath11k_peer_sta_kickout_event(ab, skb);
		break;
	case WMI_ROAM_EVENTID:
		ath11k_roam_event(ab, skb);
		break;
	case WMI_CHAN_INFO_EVENTID:
		ath11k_chan_info_event(ab, skb);
		break;
	case WMI_PDEV_BSS_CHAN_INFO_EVENTID:
		ath11k_pdev_bss_chan_info_event(ab, skb);
		break;
	case WMI_VDEV_INSTALL_KEY_COMPLETE_EVENTID:
		ath11k_vdev_install_key_compl_event(ab, skb);
		break;
	case WMI_SERVICE_AVAILABLE_EVENTID:
		ath11k_service_available_event(ab, skb);
		break;
	case WMI_PEER_ASSOC_CONF_EVENTID:
		ath11k_peer_assoc_conf_event(ab, skb);
		break;
	case WMI_UPDATE_STATS_EVENTID:
		ath11k_update_stats_event(ab, skb);
		break;
	case WMI_PDEV_CTL_FAILSAFE_CHECK_EVENTID:
		ath11k_pdev_ctl_failsafe_check_event(ab, skb);
		break;
	case WMI_PDEV_CSA_SWITCH_COUNT_STATUS_EVENTID:
		ath11k_wmi_pdev_csa_switch_count_status_event(ab, skb);
		break;
	case WMI_PDEV_UTF_EVENTID:
		ath11k_tm_wmi_event(ab, id, skb);
		break;
	case WMI_PDEV_TEMPERATURE_EVENTID:
		ath11k_wmi_pdev_temperature_event(ab, skb);
		break;
	case WMI_PDEV_DMA_RING_BUF_RELEASE_EVENTID:
		ath11k_wmi_pdev_dma_ring_buf_release_event(ab, skb);
		break;
	case WMI_HOST_FILS_DISCOVERY_EVENTID:
		ath11k_fils_discovery_event(ab, skb);
		break;
	case WMI_OFFLOAD_PROB_RESP_TX_STATUS_EVENTID:
		ath11k_probe_resp_tx_status_event(ab, skb);
		break;
	case WMI_OBSS_COLOR_COLLISION_DETECTION_EVENTID:
		ath11k_wmi_obss_color_collision_event(ab, skb);
		break;
	case WMI_TWT_ADD_DIALOG_EVENTID:
		ath11k_wmi_twt_add_dialog_event(ab, skb);
		break;
	case WMI_PDEV_DFS_RADAR_DETECTION_EVENTID:
		ath11k_wmi_pdev_dfs_radar_detected_event(ab, skb);
		break;
	case WMI_VDEV_DELETE_RESP_EVENTID:
		ath11k_vdev_delete_resp_event(ab, skb);
		break;
	case WMI_WOW_WAKEUP_HOST_EVENTID:
		ath11k_wmi_event_wow_wakeup_host(ab, skb);
		break;
	case WMI_11D_NEW_COUNTRY_EVENTID:
		ath11k_reg_11d_new_cc_event(ab, skb);
		break;
	case WMI_DIAG_EVENTID:
		ath11k_wmi_diag_event(ab, skb);
		break;
	case WMI_PEER_STA_PS_STATECHG_EVENTID:
		ath11k_wmi_event_peer_sta_ps_state_chg(ab, skb);
		break;
	case WMI_GTK_OFFLOAD_STATUS_EVENTID:
		ath11k_wmi_gtk_offload_status_event(ab, skb);
		break;
	case WMI_P2P_NOA_EVENTID:
		ath11k_wmi_p2p_noa_event(ab, skb);
		break;
	case WMI_DCS_INTERFERENCE_EVENTID:
		ath11k_wmi_dcs_awgn_interference_event(ab, skb);
		break;
	case WMI_PDEV_GET_TPC_STATS_EVENTID:
		ath11k_process_tpc_stats(ab, skb);
		break;
	case WMI_WDS_PEER_EVENTID:
		ath11k_wmi_wds_peer_event(ab, skb);
		break;
	case WMI_QOS_NULL_FRAME_TX_COMPLETION_EVENTID:
		ath11k_qos_null_compl_event(ab, skb);
		break;
	case WMI_CTRL_PATH_STATS_EVENTID:
		ath11k_wmi_ctrl_path_stats_event(ab, skb);
		break;
	case WMI_PEER_CFR_CAPTURE_EVENTID:
		ath11k_wmi_parse_cfr_capture_event(ab, skb);
		break;
	case WMI_PDEV_ANI_CCK_LEVEL_EVENTID:
		ath11k_wmi_event_ani_cck_level(ab, skb);
		break;
	case WMI_PDEV_ANI_OFDM_LEVEL_EVENTID:
		ath11k_wmi_event_ani_ofdm_level(ab, skb);
		break;
	case WMI_PEER_RATECODE_LIST_EVENTID:
		ath11k_wmi_event_peer_ratecode_list(ab, skb);
		break;
	case WMI_TBTTOFFSET_EXT_UPDATE_EVENTID:
		ath11k_wmi_event_tbttoffset_update(ab, skb);
		break;
	case WMI_VDEV_ADFS_OCAC_COMPLETE_EVENTID:
		ath11k_process_ocac_complete_event(ab, skb);
		break;

	default:
		ath11k_dbg(ab, ATH11K_DBG_WMI, "unsupported event id 0x%x\n", id);
		break;
	}

out:
	ATH11K_MEMORY_STATS_DEC(ab, wmi_tx_skb_alloc, skb->truesize);
	dev_kfree_skb(skb);
}

static int ath11k_connect_pdev_htc_service(struct ath11k_base *ab,
					   u32 pdev_idx)
{
	int status;
	u32 svc_id[] = { ATH11K_HTC_SVC_ID_WMI_CONTROL,
			 ATH11K_HTC_SVC_ID_WMI_CONTROL_MAC1,
			 ATH11K_HTC_SVC_ID_WMI_CONTROL_MAC2 };

	struct ath11k_htc_svc_conn_req conn_req;
	struct ath11k_htc_svc_conn_resp conn_resp;

	memset(&conn_req, 0, sizeof(conn_req));
	memset(&conn_resp, 0, sizeof(conn_resp));

	/* these fields are the same for all service endpoints */
	conn_req.ep_ops.ep_tx_complete = ath11k_wmi_htc_tx_complete;
	conn_req.ep_ops.ep_rx_complete = ath11k_wmi_tlv_op_rx;
	conn_req.ep_ops.ep_tx_credits = ath11k_wmi_op_ep_tx_credits;

	/* connect to control service */
	conn_req.service_id = svc_id[pdev_idx];

	status = ath11k_htc_connect_service(&ab->htc, &conn_req, &conn_resp);
	if (status) {
		ath11k_warn(ab, "failed to connect to WMI CONTROL service status: %d\n",
			    status);
		return status;
	}

	ab->wmi_ab.wmi_endpoint_id[pdev_idx] = conn_resp.eid;
	ab->wmi_ab.wmi[pdev_idx].eid = conn_resp.eid;
	ab->wmi_ab.max_msg_len[pdev_idx] = conn_resp.max_msg_len;
	init_waitqueue_head(&ab->wmi_ab.wmi[pdev_idx].tx_ce_desc_wq);

	return 0;
}

static int
ath11k_wmi_send_unit_test_cmd(struct ath11k *ar,
			      struct wmi_unit_test_cmd ut_cmd,
			      u32 *test_args)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_unit_test_cmd *cmd;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;
	u32 *ut_cmd_args;
	int buf_len, arg_len;
	int ret;
	int i;

	arg_len = sizeof(u32) * ut_cmd.num_args;
	buf_len = sizeof(ut_cmd) + arg_len + TLV_HDR_SIZE;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, buf_len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_unit_test_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_UNIT_TEST_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(ut_cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = ut_cmd.vdev_id;
	cmd->module_id = ut_cmd.module_id;
	cmd->num_args = ut_cmd.num_args;
	cmd->diag_token = ut_cmd.diag_token;

	ptr = skb->data + sizeof(ut_cmd);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_UINT32) |
		      FIELD_PREP(WMI_TLV_LEN, arg_len);

	ptr += TLV_HDR_SIZE;

	ut_cmd_args = ptr;
	for (i = 0; i < ut_cmd.num_args; i++)
		ut_cmd_args[i] = test_args[i];

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_UNIT_TEST_CMDID);

	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_UNIT_TEST CMD :%d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "cmd unit test module %d vdev %d n_args %d token %d\n",
		   cmd->module_id, cmd->vdev_id, cmd->num_args,
		   cmd->diag_token);

	return ret;
}

int ath11k_wmi_simulate_radar(struct ath11k *ar, u32 radar_params)
{
	struct ath11k_vif *arvif;
	u32 dfs_args[DFS_MAX_TEST_ARGS];
	struct wmi_unit_test_cmd wmi_ut;
	bool arvif_found = false;

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (arvif->is_started && arvif->vdev_type == WMI_VDEV_TYPE_AP) {
			arvif_found = true;
			break;
		}
	}

	if (!arvif_found)
		return -EINVAL;

	dfs_args[DFS_TEST_CMDID] = 0;
	dfs_args[DFS_TEST_PDEV_ID] = ar->pdev->pdev_id;
	/* Currently we could pass segment_id(b0 - b1), chirp(b2)
	 * freq offset (b3 - b10) to unit test. For simulation
	 * purpose this can be set to 0 which is valid.
	 */
	dfs_args[DFS_TEST_RADAR_PARAM] = radar_params;

	wmi_ut.vdev_id = arvif->vdev_id;
	wmi_ut.module_id = DFS_UNIT_TEST_MODULE;
	wmi_ut.num_args = DFS_MAX_TEST_ARGS;
	wmi_ut.diag_token = DFS_UNIT_TEST_TOKEN;

	ath11k_dbg(ar->ab, ATH11K_DBG_REG,
		   "Triggering Radar Simulation with param %d\n", radar_params);

	return ath11k_wmi_send_unit_test_cmd(ar, wmi_ut, dfs_args);
}

int ath11k_wmi_fw_dbglog_cfg(struct ath11k *ar, u32 *module_id_bitmap,
			     struct ath11k_fw_dbglog *dbglog)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_debug_log_config_cmd_fixed_param *cmd;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	int ret, len;

	len = sizeof(*cmd) + TLV_HDR_SIZE + (MAX_MODULE_ID_BITMAP_WORDS * sizeof(u32));
	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_debug_log_config_cmd_fixed_param *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_DEBUG_LOG_CONFIG_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->dbg_log_param = dbglog->param;

	tlv = (struct wmi_tlv *)((u8 *)cmd + sizeof(*cmd));
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_UINT32) |
		      FIELD_PREP(WMI_TLV_LEN, MAX_MODULE_ID_BITMAP_WORDS * sizeof(u32));

	switch (dbglog->param) {
	case WMI_DEBUG_LOG_PARAM_LOG_LEVEL:
	case WMI_DEBUG_LOG_PARAM_VDEV_ENABLE:
	case WMI_DEBUG_LOG_PARAM_VDEV_DISABLE:
	case WMI_DEBUG_LOG_PARAM_VDEV_ENABLE_BITMAP:
		cmd->value = dbglog->value;
		break;
	case WMI_DEBUG_LOG_PARAM_MOD_ENABLE_BITMAP:
	case WMI_DEBUG_LOG_PARAM_WOW_MOD_ENABLE_BITMAP:
		cmd->value = dbglog->value;
		memcpy(tlv->value, module_id_bitmap,
		       MAX_MODULE_ID_BITMAP_WORDS * sizeof(u32));
		/* clear current config to be used for next user config */
		memset(module_id_bitmap, 0,
		       MAX_MODULE_ID_BITMAP_WORDS * sizeof(u32));
		break;
	default:
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_DBGLOG_CFG_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_DBGLOG_CFG_CMDID\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "cmd dbglog cfg");

	return ret;
}


int ath11k_wmi_simulate_awgn(struct ath11k *ar, u32 chan_bw_interference_bitmap)
{
	struct ath11k_vif *arvif;
	u32 awgn_args[WMI_AWGN_MAX_TEST_ARGS];
	struct wmi_unit_test_cmd wmi_ut;
	bool arvif_found = false;

	if (!test_bit(WMI_TLV_SERVICE_DCS_AWGN_INT_SUPPORT, ar->ab->wmi_ab.svc_map)) {
		ath11k_warn(ar->ab, "firmware doesn't support awgn interference, so can't simulate it\n");
		return -EOPNOTSUPP;
	}

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (arvif->is_started && arvif->vdev_type == WMI_VDEV_TYPE_AP) {
			arvif_found = true;
			break;
		}
	}

	if (!arvif_found)
		return -EINVAL;

	awgn_args[WMI_AWGN_TEST_AWGN_INT] = WMI_UNIT_TEST_AWGN_INTF_TYPE;
	awgn_args[WMI_AWGN_TEST_BITMAP] = chan_bw_interference_bitmap;

	wmi_ut.vdev_id = arvif->vdev_id;
	wmi_ut.module_id = WMI_AWGN_UNIT_TEST_MODULE;
	wmi_ut.num_args = WMI_AWGN_MAX_TEST_ARGS;
	wmi_ut.diag_token = WMI_AWGN_UNIT_TEST_TOKEN;

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "Triggering AWGN Simulation, interference bitmap : 0x%x\n",
		   chan_bw_interference_bitmap);

	return ath11k_wmi_send_unit_test_cmd(ar, wmi_ut, awgn_args);
}

int ath11k_wmi_pdev_m3_dump_enable(struct ath11k *ar, u32 enable) {
	struct ath11k_vif *arvif;
	u32 m3_args[WMI_M3_MAX_TEST_ARGS];
	struct wmi_unit_test_cmd wmi_ut;
	bool arvif_found = false;

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (arvif->is_started) {
			arvif_found = true;
			break;
		}
	}

	if (!arvif_found)
		return -EINVAL;

	m3_args[WMI_M3_TEST_CMDID] = WMI_DBG_ENABLE_M3_SSR;
	m3_args[WMI_M3_TEST_ENABLE] = enable;

	wmi_ut.vdev_id = arvif->vdev_id;
	wmi_ut.module_id = WMI_M3_UNIT_TEST_MODULE;
	wmi_ut.num_args = WMI_M3_MAX_TEST_ARGS;
	wmi_ut.diag_token = WMI_M3_UNIT_TEST_TOKEN;

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "%s M3 SSR dump\n",
		   enable ? "Enabling" : "Disabling");

	return ath11k_wmi_send_unit_test_cmd(ar, wmi_ut, m3_args);
}

int ath11k_wmi_connect(struct ath11k_base *ab)
{
	u32 i;
	u8 wmi_ep_count;

	wmi_ep_count = ab->htc.wmi_ep_count;
	if (wmi_ep_count > ab->hw_params.max_radios)
		return -1;

	for (i = 0; i < wmi_ep_count; i++)
		ath11k_connect_pdev_htc_service(ab, i);

	return 0;
}

static void ath11k_wmi_pdev_detach(struct ath11k_base *ab, u8 pdev_id)
{
	if (WARN_ON(pdev_id >= MAX_RADIOS))
		return;

	/* TODO: Deinit any pdev specific wmi resource */
}

int ath11k_wmi_pdev_attach(struct ath11k_base *ab,
			   u8 pdev_id)
{
	struct ath11k_pdev_wmi *wmi_handle;

	if (pdev_id >= ab->hw_params.max_radios)
		return -EINVAL;

	wmi_handle = &ab->wmi_ab.wmi[pdev_id];

	wmi_handle->wmi_ab = &ab->wmi_ab;

	ab->wmi_ab.ab = ab;
	/* TODO: Init remaining resource specific to pdev */

	return 0;
}

int ath11k_wmi_attach(struct ath11k_base *ab)
{
	int ret;

	ret = ath11k_wmi_pdev_attach(ab, 0);
	if (ret)
		return ret;

	ab->wmi_ab.ab = ab;
	ab->wmi_ab.preferred_hw_mode = WMI_HOST_HW_MODE_MAX;

	/* It's overwritten when service_ext_ready is handled */
	if (ab->hw_params.single_pdev_only && ab->hw_params.num_rxdma_per_pdev > 1)
		ab->wmi_ab.preferred_hw_mode = WMI_HOST_HW_MODE_SINGLE;

	/* TODO: Init remaining wmi soc resources required */
	init_completion(&ab->wmi_ab.service_ready);
	init_completion(&ab->wmi_ab.unified_ready);

	return 0;
}

void ath11k_wmi_detach(struct ath11k_base *ab)
{
	int i;

	/* TODO: Deinit wmi resource specific to SOC as required */

	for (i = 0; i < ab->htc.wmi_ep_count; i++)
		ath11k_wmi_pdev_detach(ab, i);

	ath11k_wmi_free_dbring_caps(ab);
}

int ath11k_wmi_hw_data_filter_cmd(struct ath11k *ar, u32 vdev_id,
				  u32 filter_bitmap, bool enable)
{
	struct wmi_hw_data_filter_cmd *cmd;
	struct sk_buff *skb;
	int len;

	len = sizeof(*cmd);
	skb = ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, len);

	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_hw_data_filter_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_HW_DATA_FILTER_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = vdev_id;
	cmd->enable = enable;

	/* Set all modes in case of disable */
	if (cmd->enable)
		cmd->hw_filter_bitmap = filter_bitmap;
	else
		cmd->hw_filter_bitmap = ((u32)~0U);

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "hw data filter enable %d filter_bitmap 0x%x\n",
		   enable, filter_bitmap);

	return ath11k_wmi_cmd_send(ar->wmi, skb, WMI_HW_DATA_FILTER_CMDID);
}

int ath11k_wmi_wow_host_wakeup_ind(struct ath11k *ar)
{
	struct wmi_wow_host_wakeup_ind *cmd;
	struct sk_buff *skb;
	size_t len;

	len = sizeof(*cmd);
	skb = ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_wow_host_wakeup_ind *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_WOW_HOSTWAKEUP_FROM_SLEEP_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "tlv wow host wakeup ind\n");

	return ath11k_wmi_cmd_send(ar->wmi, skb, WMI_WOW_HOSTWAKEUP_FROM_SLEEP_CMDID);
}

int ath11k_wmi_wow_enable(struct ath11k *ar)
{
	struct wmi_wow_enable_cmd *cmd;
	struct sk_buff *skb;
	int len;

	len = sizeof(*cmd);
	skb = ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_wow_enable_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_WOW_ENABLE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->enable = 1;
	cmd->pause_iface_config = WOW_IFACE_PAUSE_ENABLED;
	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "tlv wow enable\n");

	return ath11k_wmi_cmd_send(ar->wmi, skb, WMI_WOW_ENABLE_CMDID);
}

int ath11k_wmi_scan_prob_req_oui(struct ath11k *ar,
				 const u8 mac_addr[ETH_ALEN])
{
	struct sk_buff *skb;
	struct wmi_scan_prob_req_oui_cmd *cmd;
	u32 prob_req_oui;
	int len;

	prob_req_oui = (((u32)mac_addr[0]) << 16) |
		       (((u32)mac_addr[1]) << 8) | mac_addr[2];

	len = sizeof(*cmd);
	skb = ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_scan_prob_req_oui_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_SCAN_PROB_REQ_OUI_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->prob_req_oui = prob_req_oui;

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "scan prob req oui %d\n",
		   prob_req_oui);

	return ath11k_wmi_cmd_send(ar->wmi, skb, WMI_SCAN_PROB_REQ_OUI_CMDID);
}

int ath11k_wmi_wow_add_wakeup_event(struct ath11k *ar, u32 vdev_id,
				    enum wmi_wow_wakeup_event event,
				u32 enable)
{
	struct wmi_wow_add_del_event_cmd *cmd;
	struct sk_buff *skb;
	size_t len;

	len = sizeof(*cmd);
	skb = ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_wow_add_del_event_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_WOW_ADD_DEL_EVT_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = vdev_id;
	cmd->is_add = enable;
	cmd->event_bitmap = (1 << event);

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "tlv wow add wakeup event %s enable %d vdev_id %d\n",
		   wow_wakeup_event(event), enable, vdev_id);

	return ath11k_wmi_cmd_send(ar->wmi, skb, WMI_WOW_ENABLE_DISABLE_WAKE_EVENT_CMDID);
}

int ath11k_wmi_wow_add_pattern(struct ath11k *ar, u32 vdev_id, u32 pattern_id,
			       const u8 *pattern, const u8 *mask,
			   int pattern_len, int pattern_offset)
{
	struct wmi_wow_add_pattern_cmd *cmd;
	struct wmi_wow_bitmap_pattern *bitmap;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	u8 *ptr;
	size_t len;

	len = sizeof(*cmd) +
	      sizeof(*tlv) +			/* array struct */
	      sizeof(*bitmap) +			/* bitmap */
	      sizeof(*tlv) +			/* empty ipv4 sync */
	      sizeof(*tlv) +			/* empty ipv6 sync */
	      sizeof(*tlv) +			/* empty magic */
	      sizeof(*tlv) +			/* empty info timeout */
	      sizeof(*tlv) + sizeof(u32);	/* ratelimit interval */

	skb = ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	/* cmd */
	ptr = (u8 *)skb->data;
	cmd = (struct wmi_wow_add_pattern_cmd *)ptr;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_WOW_ADD_PATTERN_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = vdev_id;
	cmd->pattern_id = pattern_id;
	cmd->pattern_type = WOW_BITMAP_PATTERN;

	ptr += sizeof(*cmd);

	/* bitmap */
	tlv = (struct wmi_tlv *)ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG,
				 WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, sizeof(*bitmap));

	ptr += sizeof(*tlv);

	bitmap = (struct wmi_wow_bitmap_pattern *)ptr;
	bitmap->tlv_header = FIELD_PREP(WMI_TLV_TAG,
					WMI_TAG_WOW_BITMAP_PATTERN_T) |
			     FIELD_PREP(WMI_TLV_LEN, sizeof(*bitmap) - TLV_HDR_SIZE);

	memcpy(bitmap->patternbuf, pattern, pattern_len);
	ath11k_ce_byte_swap(bitmap->patternbuf, roundup(pattern_len, 4));
	memcpy(bitmap->bitmaskbuf, mask, pattern_len);
	ath11k_ce_byte_swap(bitmap->bitmaskbuf, roundup(pattern_len, 4));
	bitmap->pattern_offset = pattern_offset;
	bitmap->pattern_len = pattern_len;
	bitmap->bitmask_len = pattern_len;
	bitmap->pattern_id = pattern_id;

	ptr += sizeof(*bitmap);

	/* ipv4 sync */
	tlv = (struct wmi_tlv *)ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG,
				 WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, 0);

	ptr += sizeof(*tlv);

	/* ipv6 sync */
	tlv = (struct wmi_tlv *)ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG,
				 WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, 0);

	ptr += sizeof(*tlv);

	/* magic */
	tlv = (struct wmi_tlv *)ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG,
				 WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, 0);

	ptr += sizeof(*tlv);

	/* pattern info timeout */
	tlv = (struct wmi_tlv *)ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG,
				 WMI_TAG_ARRAY_UINT32) |
		      FIELD_PREP(WMI_TLV_LEN, 0);

	ptr += sizeof(*tlv);

	/* ratelimit interval */
	tlv = (struct wmi_tlv *)ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG,
				 WMI_TAG_ARRAY_UINT32) |
		      FIELD_PREP(WMI_TLV_LEN, sizeof(u32));

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "tlv wow add pattern vdev_id %d pattern_id %d pattern_offset %d\n",
		   vdev_id, pattern_id, pattern_offset);

	return ath11k_wmi_cmd_send(ar->wmi, skb, WMI_WOW_ADD_WAKE_PATTERN_CMDID);
}

int ath11k_wmi_wow_del_pattern(struct ath11k *ar, u32 vdev_id, u32 pattern_id)
{
	struct wmi_wow_del_pattern_cmd *cmd;
	struct sk_buff *skb;
	size_t len;

	len = sizeof(*cmd);
	skb = ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_wow_del_pattern_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_WOW_DEL_PATTERN_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = vdev_id;
	cmd->pattern_id = pattern_id;
	cmd->pattern_type = WOW_BITMAP_PATTERN;

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "tlv wow del pattern vdev_id %d pattern_id %d\n",
		   vdev_id, pattern_id);

	return ath11k_wmi_cmd_send(ar->wmi, skb, WMI_WOW_DEL_WAKE_PATTERN_CMDID);
}

static struct sk_buff *
ath11k_wmi_op_gen_config_pno_start(struct ath11k *ar,
				   u32 vdev_id,
				       struct wmi_pno_scan_req *pno)
{
	struct nlo_configured_parameters *nlo_list;
	struct wmi_wow_nlo_config_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	u32 *channel_list;
	size_t len, nlo_list_len, channel_list_len;
	u8 *ptr;
	u32 i;

	len = sizeof(*cmd) +
	      sizeof(*tlv) +
	      /* TLV place holder for array of structures
	       * nlo_configured_parameters(nlo_list)
	       */
	      sizeof(*tlv);
	      /* TLV place holder for array of uint32 channel_list */

	channel_list_len = sizeof(u32) * pno->a_networks[0].channel_count;
	len += channel_list_len;

	nlo_list_len = sizeof(*nlo_list) * pno->uc_networks_count;
	len += nlo_list_len;

	skb = ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return ERR_PTR(-ENOMEM);

	ptr = (u8 *)skb->data;
	cmd = (struct wmi_wow_nlo_config_cmd *)ptr;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_NLO_CONFIG_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = pno->vdev_id;
	cmd->flags = WMI_NLO_CONFIG_START | WMI_NLO_CONFIG_SSID_HIDE_EN;

	/* current FW does not support min-max range for dwell time */
	cmd->active_dwell_time = pno->active_max_time;
	cmd->passive_dwell_time = pno->passive_max_time;

	if (pno->do_passive_scan)
		cmd->flags |= WMI_NLO_CONFIG_SCAN_PASSIVE;

	cmd->fast_scan_period = pno->fast_scan_period;
	cmd->slow_scan_period = pno->slow_scan_period;
	cmd->fast_scan_max_cycles = pno->fast_scan_max_cycles;
	cmd->delay_start_time = pno->delay_start_time;

	if (pno->enable_pno_scan_randomization) {
		cmd->flags |= WMI_NLO_CONFIG_SPOOFED_MAC_IN_PROBE_REQ |
				WMI_NLO_CONFIG_RANDOM_SEQ_NO_IN_PROBE_REQ;
		ether_addr_copy(cmd->mac_addr.addr, pno->mac_addr);
		ether_addr_copy(cmd->mac_mask.addr, pno->mac_addr_mask);
		ath11k_ce_byte_swap(cmd->mac_addr.addr, 8);
		ath11k_ce_byte_swap(cmd->mac_mask.addr, 8);
	}

	ptr += sizeof(*cmd);

	/* nlo_configured_parameters(nlo_list) */
	cmd->no_of_ssids = pno->uc_networks_count;
	tlv = (struct wmi_tlv *)ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG,
				 WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, nlo_list_len);

	ptr += sizeof(*tlv);
	nlo_list = (struct nlo_configured_parameters *)ptr;
	for (i = 0; i < cmd->no_of_ssids; i++) {
		tlv = (struct wmi_tlv *)(&nlo_list[i].tlv_header);
		tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_BYTE) |
			      FIELD_PREP(WMI_TLV_LEN, sizeof(*nlo_list) - sizeof(*tlv));

		nlo_list[i].ssid.valid = true;
		nlo_list[i].ssid.ssid.ssid_len = pno->a_networks[i].ssid.ssid_len;
		memcpy(nlo_list[i].ssid.ssid.ssid,
		       pno->a_networks[i].ssid.ssid,
		       nlo_list[i].ssid.ssid.ssid_len);
		ath11k_ce_byte_swap(nlo_list[i].ssid.ssid.ssid,
				    roundup(nlo_list[i].ssid.ssid.ssid_len, 4));

		if (pno->a_networks[i].rssi_threshold &&
		    pno->a_networks[i].rssi_threshold > -300) {
			nlo_list[i].rssi_cond.valid = true;
			nlo_list[i].rssi_cond.rssi =
				pno->a_networks[i].rssi_threshold;
		}

		nlo_list[i].bcast_nw_type.valid = true;
		nlo_list[i].bcast_nw_type.bcast_nw_type =
			pno->a_networks[i].bcast_nw_type;
	}

	ptr += nlo_list_len;
	cmd->num_of_channels = pno->a_networks[0].channel_count;
	tlv = (struct wmi_tlv *)ptr;
	tlv->header =  FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_UINT32) |
		       FIELD_PREP(WMI_TLV_LEN, channel_list_len);
	ptr += sizeof(*tlv);
	channel_list = (u32 *)ptr;
	for (i = 0; i < cmd->num_of_channels; i++)
		channel_list[i] = pno->a_networks[0].channels[i];

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "tlv start pno config vdev_id %d\n",
		   vdev_id);

	return skb;
}

static struct sk_buff *ath11k_wmi_op_gen_config_pno_stop(struct ath11k *ar,
							 u32 vdev_id)
{
	struct wmi_wow_nlo_config_cmd *cmd;
	struct sk_buff *skb;
	size_t len;

	len = sizeof(*cmd);
	skb = ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return ERR_PTR(-ENOMEM);

	cmd = (struct wmi_wow_nlo_config_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_NLO_CONFIG_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);

	cmd->vdev_id = vdev_id;
	cmd->flags = WMI_NLO_CONFIG_STOP;

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "tlv stop pno config vdev_id %d\n", vdev_id);
	return skb;
}

int ath11k_wmi_wow_config_pno(struct ath11k *ar, u32 vdev_id,
			      struct wmi_pno_scan_req  *pno_scan)
{
	struct sk_buff *skb;

	if (pno_scan->enable)
		skb = ath11k_wmi_op_gen_config_pno_start(ar, vdev_id, pno_scan);
	else
		skb = ath11k_wmi_op_gen_config_pno_stop(ar, vdev_id);

	if (IS_ERR_OR_NULL(skb))
		return -ENOMEM;

	return ath11k_wmi_cmd_send(ar->wmi, skb, WMI_NETWORK_LIST_OFFLOAD_CONFIG_CMDID);
}

static void ath11k_wmi_fill_ns_offload(struct ath11k *ar,
				       struct ath11k_arp_ns_offload *offload,
				       u8 **ptr,
				       bool enable,
				       bool ext)
{
	struct wmi_ns_offload_tuple *ns;
	struct wmi_tlv *tlv;
	u8 *buf_ptr = *ptr;
	u32 ns_cnt, ns_ext_tuples;
	int i, max_offloads;

	ns_cnt = offload->ipv6_count;

	tlv  = (struct wmi_tlv *)buf_ptr;

	if (ext) {
		ns_ext_tuples = offload->ipv6_count - WMI_MAX_NS_OFFLOADS;
		tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
			      FIELD_PREP(WMI_TLV_LEN, ns_ext_tuples * sizeof(*ns));
		i = WMI_MAX_NS_OFFLOADS;
		max_offloads = offload->ipv6_count;
	} else {
		tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
			      FIELD_PREP(WMI_TLV_LEN, WMI_MAX_NS_OFFLOADS * sizeof(*ns));
		i = 0;
		max_offloads = WMI_MAX_NS_OFFLOADS;
	}

	buf_ptr += sizeof(*tlv);

	for (; i < max_offloads; i++) {
		ns = (struct wmi_ns_offload_tuple *)buf_ptr;
		ns->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_NS_OFFLOAD_TUPLE) |
				 FIELD_PREP(WMI_TLV_LEN, sizeof(*ns) - TLV_HDR_SIZE);

		if (enable) {
			if (i < ns_cnt)
				ns->flags |= WMI_NSOL_FLAGS_VALID;

			memcpy(ns->target_ipaddr[0], offload->ipv6_addr[i], 16);
			memcpy(ns->solicitation_ipaddr, offload->self_ipv6_addr[i], 16);
			ath11k_ce_byte_swap(ns->target_ipaddr[0], 16);
			ath11k_ce_byte_swap(ns->solicitation_ipaddr, 16);

			if (offload->ipv6_type[i])
				ns->flags |= WMI_NSOL_FLAGS_IS_IPV6_ANYCAST;

			memcpy(ns->target_mac.addr, offload->mac_addr, ETH_ALEN);
			ath11k_ce_byte_swap(ns->target_mac.addr, 8);

			if (ns->target_mac.word0 != 0 ||
			    ns->target_mac.word1 != 0) {
				ns->flags |= WMI_NSOL_FLAGS_MAC_VALID;
			}

			ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
				   "index %d ns_solicited %pI6 target %pI6",
				   i, ns->solicitation_ipaddr,
				   ns->target_ipaddr[0]);
		}

		buf_ptr += sizeof(*ns);
	}

	*ptr = buf_ptr;
}

static void ath11k_wmi_fill_arp_offload(struct ath11k *ar,
					struct ath11k_arp_ns_offload *offload,
					u8 **ptr,
					bool enable)
{
	struct wmi_arp_offload_tuple *arp;
	struct wmi_tlv *tlv;
	u8 *buf_ptr = *ptr;
	int i;

	/* fill arp tuple */
	tlv = (struct wmi_tlv *)buf_ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, WMI_MAX_ARP_OFFLOADS * sizeof(*arp));
	buf_ptr += sizeof(*tlv);

	for (i = 0; i < WMI_MAX_ARP_OFFLOADS; i++) {
		arp = (struct wmi_arp_offload_tuple *)buf_ptr;
		arp->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARP_OFFLOAD_TUPLE) |
				  FIELD_PREP(WMI_TLV_LEN, sizeof(*arp) - TLV_HDR_SIZE);

		if (enable && i < offload->ipv4_count) {
			/* Copy the target ip addr and flags */
			arp->flags = WMI_ARPOL_FLAGS_VALID;
			memcpy(arp->target_ipaddr, offload->ipv4_addr[i], 4);
			ath11k_ce_byte_swap(arp->target_ipaddr, 4);

			ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "arp offload address %pI4",
				   arp->target_ipaddr);
		}

		buf_ptr += sizeof(*arp);
	}

	*ptr = buf_ptr;
}

int ath11k_wmi_arp_ns_offload(struct ath11k *ar,
			      struct ath11k_vif *arvif, bool enable)
{
	struct ath11k_arp_ns_offload *offload;
	struct wmi_set_arp_ns_offload_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	u8 *buf_ptr;
	size_t len;
	u8 ns_cnt, ns_ext_tuples = 0;

	offload = &arvif->arp_ns_offload;
	ns_cnt = offload->ipv6_count;

	len = sizeof(*cmd) +
	      sizeof(*tlv) +
	      WMI_MAX_NS_OFFLOADS * sizeof(struct wmi_ns_offload_tuple) +
	      sizeof(*tlv) +
	      WMI_MAX_ARP_OFFLOADS * sizeof(struct wmi_arp_offload_tuple);

	if (ns_cnt > WMI_MAX_NS_OFFLOADS) {
		ns_ext_tuples = ns_cnt - WMI_MAX_NS_OFFLOADS;
		len += sizeof(*tlv) +
		       ns_ext_tuples * sizeof(struct wmi_ns_offload_tuple);
	}

	skb = ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	buf_ptr = skb->data;
	cmd = (struct wmi_set_arp_ns_offload_cmd *)buf_ptr;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_SET_ARP_NS_OFFLOAD_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->flags = 0;
	cmd->vdev_id = arvif->vdev_id;
	cmd->num_ns_ext_tuples = ns_ext_tuples;

	buf_ptr += sizeof(*cmd);

	ath11k_wmi_fill_ns_offload(ar, offload, &buf_ptr, enable, 0);
	ath11k_wmi_fill_arp_offload(ar, offload, &buf_ptr, enable);

	if (ns_ext_tuples)
		ath11k_wmi_fill_ns_offload(ar, offload, &buf_ptr, enable, 1);

	return ath11k_wmi_cmd_send(ar->wmi, skb, WMI_SET_ARP_NS_OFFLOAD_CMDID);
}

int ath11k_wmi_gtk_rekey_offload(struct ath11k *ar,
				 struct ath11k_vif *arvif, bool enable)
{
	struct wmi_gtk_rekey_offload_cmd *cmd;
	struct ath11k_rekey_data *rekey_data = &arvif->rekey_data;
	int len;
	struct sk_buff *skb;
	__le64 replay_ctr;

	len = sizeof(*cmd);
	skb =  ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_gtk_rekey_offload_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_GTK_OFFLOAD_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = arvif->vdev_id;

	if (enable) {
		cmd->flags = GTK_OFFLOAD_ENABLE_OPCODE;

		/* the length in rekey_data and cmd is equal */
		memcpy(cmd->kck, rekey_data->kck, sizeof(cmd->kck));
		ath11k_ce_byte_swap(cmd->kck, GTK_OFFLOAD_KEK_BYTES);
		memcpy(cmd->kek, rekey_data->kek, sizeof(cmd->kek));
		ath11k_ce_byte_swap(cmd->kek, GTK_OFFLOAD_KEK_BYTES);

		replay_ctr = cpu_to_le64(rekey_data->replay_ctr);
		memcpy(cmd->replay_ctr, &replay_ctr,
		       sizeof(replay_ctr));
		ath11k_ce_byte_swap(cmd->replay_ctr, GTK_REPLAY_COUNTER_BYTES);
	} else {
		cmd->flags = GTK_OFFLOAD_DISABLE_OPCODE;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "offload gtk rekey vdev: %d %d\n",
		   arvif->vdev_id, enable);
	return ath11k_wmi_cmd_send(ar->wmi, skb, WMI_GTK_OFFLOAD_CMDID);
}

int ath11k_wmi_gtk_rekey_getinfo(struct ath11k *ar,
				 struct ath11k_vif *arvif)
{
	struct wmi_gtk_rekey_offload_cmd *cmd;
	int len;
	struct sk_buff *skb;

	len = sizeof(*cmd);
	skb =  ath11k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_gtk_rekey_offload_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_GTK_OFFLOAD_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = arvif->vdev_id;
	cmd->flags = GTK_OFFLOAD_REQUEST_STATUS_OPCODE;

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "get gtk rekey vdev_id: %d\n",
		   arvif->vdev_id);
	return ath11k_wmi_cmd_send(ar->wmi, skb, WMI_GTK_OFFLOAD_CMDID);
}

int ath11k_wmi_pdev_set_bios_sar_table_param(struct ath11k *ar, const u8 *sar_val)
{	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_pdev_set_sar_table_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	u8 *buf_ptr;
	u32 len, sar_len_aligned, rsvd_len_aligned;

	sar_len_aligned = roundup(BIOS_SAR_TABLE_LEN, sizeof(u32));
	rsvd_len_aligned = roundup(BIOS_SAR_RSVD1_LEN, sizeof(u32));
	len = sizeof(*cmd) +
	      TLV_HDR_SIZE + sar_len_aligned +
	      TLV_HDR_SIZE + rsvd_len_aligned;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_sar_table_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PDEV_SET_BIOS_SAR_TABLE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	cmd->sar_len = BIOS_SAR_TABLE_LEN;
	cmd->rsvd_len = BIOS_SAR_RSVD1_LEN;

	buf_ptr = skb->data + sizeof(*cmd);
	tlv = (struct wmi_tlv *)buf_ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_BYTE) |
		      FIELD_PREP(WMI_TLV_LEN, sar_len_aligned);
	buf_ptr += TLV_HDR_SIZE;
	memcpy(buf_ptr, sar_val, BIOS_SAR_TABLE_LEN);

	buf_ptr += sar_len_aligned;
	tlv = (struct wmi_tlv *)buf_ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_BYTE) |
		      FIELD_PREP(WMI_TLV_LEN, rsvd_len_aligned);

	return ath11k_wmi_cmd_send(wmi, skb, WMI_PDEV_SET_BIOS_SAR_TABLE_CMDID);
}

int ath11k_wmi_pdev_set_bios_geo_table_param(struct ath11k *ar)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_pdev_set_geo_table_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	u8 *buf_ptr;
	u32 len, rsvd_len_aligned;

	rsvd_len_aligned = roundup(BIOS_SAR_RSVD2_LEN, sizeof(u32));
	len = sizeof(*cmd) + TLV_HDR_SIZE + rsvd_len_aligned;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_geo_table_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PDEV_SET_BIOS_GEO_TABLE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	cmd->rsvd_len = BIOS_SAR_RSVD2_LEN;

	buf_ptr = skb->data + sizeof(*cmd);
	tlv = (struct wmi_tlv *)buf_ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_BYTE) |
		      FIELD_PREP(WMI_TLV_LEN, rsvd_len_aligned);

	return ath11k_wmi_cmd_send(wmi, skb, WMI_PDEV_SET_BIOS_GEO_TABLE_CMDID);
}

int ath11k_wmi_sta_keepalive(struct ath11k *ar,
			     const struct wmi_sta_keepalive_arg *arg)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_sta_keepalive_cmd *cmd;
	struct wmi_sta_keepalive_arp_resp *arp;
	struct sk_buff *skb;
	size_t len;

	len = sizeof(*cmd) + sizeof(*arp);
	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_sta_keepalive_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_STA_KEEPALIVE_CMD) |
				     FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = arg->vdev_id;
	cmd->enabled = arg->enabled;
	cmd->interval = arg->interval;
	cmd->method = arg->method;

	arp = (struct wmi_sta_keepalive_arp_resp *)(cmd + 1);
	arp->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_STA_KEEPALIVE_ARP_RESPONSE) |
			 FIELD_PREP(WMI_TLV_LEN, sizeof(*arp) - TLV_HDR_SIZE);

	if (arg->method == WMI_STA_KEEPALIVE_METHOD_UNSOLICITED_ARP_RESPONSE ||
	    arg->method == WMI_STA_KEEPALIVE_METHOD_GRATUITOUS_ARP_REQUEST) {
		arp->src_ip4_addr = arg->src_ip4_addr;
		arp->dest_ip4_addr = arg->dest_ip4_addr;
		ether_addr_copy(arp->dest_mac_addr.addr, arg->dest_mac_addr);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "sta keepalive vdev %d enabled %d method %d interval %d\n",
		   arg->vdev_id, arg->enabled, arg->method, arg->interval);

	return ath11k_wmi_cmd_send(wmi, skb, WMI_STA_KEEPALIVE_CMDID);
}

bool ath11k_wmi_supports_6ghz_cc_ext(struct ath11k *ar)
{
	return test_bit(WMI_TLV_SERVICE_REG_CC_EXT_EVENT_SUPPORT,
			ar->ab->wmi_ab.svc_map) && ar->supports_6ghz;
}

void
ath11k_wmi_sa_set_gpio_param(struct ath11k *ar,
			     struct smart_ant_enable_params *params)
{
	params->gpio_pin[0] = ATH11K_SMART_ANT_PIN0;
	params->gpio_func[0] = ATH11K_SMART_ANT_FUNC0;
	params->gpio_pin[1] = ATH11K_SMART_ANT_PIN1;
	params->gpio_func[1] = ATH11K_SMART_ANT_FUNC1;
	params->gpio_pin[2] = 0;
	params->gpio_func[2] = 0;
	params->gpio_pin[3] = 0;
	params->gpio_func[3] = 0;
}

int ath11k_wmi_pdev_set_rx_ant(struct ath11k *ar, u32 rx_antenna)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_pdev_set_rx_antenna_cmd *cmd;
	struct sk_buff *skb;
	void *ptr;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = ptr;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PDEV_SMART_ANT_SET_RX_ANTENNA_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->rx_antenna = rx_antenna;
	cmd->pdev_id = ar->pdev->pdev_id;

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PDEV_SMART_ANT_SET_RX_ANTENNA_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to submit WMI_PDEV_SMART_ANT_SET_RX_ANTENNA_CMDID\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "WMI pdev rx set smart antenna pdev_id %d rx_antenna: %d\n",
		   ar->pdev->pdev_id, rx_antenna);

	return ret;
}

int ath11k_wmi_peer_set_smart_tx_ant(struct ath11k *ar,
				     u32 vdev_id, const u8 *macaddr,
				     const u32 *tx_antenna)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_pdev_set_tx_antenna_cmd *cmd;
	struct wmi_peer_set_smart_tx_ant_series_cmd *tx_ant_series;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int i, ret, len;

	len = sizeof(*cmd) + WMI_TLV_HDR_SIZE;
	len += WMI_SMART_MAX_RATE_SERIES *
	       sizeof(struct wmi_peer_set_smart_tx_ant_series_cmd);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = ptr;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PEER_SMART_ANT_SET_TX_ANTENNA_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	ether_addr_copy(cmd->macaddr.addr, macaddr);

	ptr = skb->data + sizeof(*cmd);

	len = WMI_SMART_MAX_RATE_SERIES *
	      sizeof(struct wmi_peer_set_smart_tx_ant_series_cmd);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, len);
	ptr += TLV_HDR_SIZE;

	tx_ant_series = ptr;

	for (i = 0; i < WMI_SMART_MAX_RATE_SERIES; i++) {
		tx_ant_series->tlv_header =
			FIELD_PREP(WMI_TLV_TAG,
				   WMI_TAG_PEER_SMART_ANT_SET_TX_ANTENNA_SERIES) |
			FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
		tx_ant_series->ant_series = tx_antenna[i];
		tx_ant_series++;
	}

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PEER_SMART_ANT_SET_TX_ANTENNA_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_PEER_SMART_ANT_SET_TX_ANTENNA_CMDID\n");
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI, "WMI peer set tx smart antenna peer %pM\n",
		   macaddr);

	return ret;
}

int
ath11k_wmi_pdev_disable_smart_ant(struct ath11k *ar,
				  struct ath11k_smart_ant_info *info)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_pdev_set_smart_ant_cmd *cmd;
	struct wmi_pdev_smart_ant_gpio_handle_cmd *gpio_param;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int i, ret, len;

	len = sizeof(*cmd) + TLV_HDR_SIZE +
	      WMI_SMART_ANTENNA_HAL_MAX * sizeof(*gpio_param);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = ptr;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PDEV_SMART_ANT_ENABLE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	cmd->mode = 0;
	cmd->enable = info->enabled;
	cmd->rx_antenna = 0;
	cmd->tx_default_antenna = 0;

	ptr = skb->data + sizeof(*cmd);

	len = WMI_SMART_ANTENNA_HAL_MAX * sizeof(*gpio_param);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, len);
	ptr += TLV_HDR_SIZE;
	gpio_param = ptr;

	for (i = 0; i < WMI_SMART_ANTENNA_HAL_MAX; i++) {
		gpio_param->tlv_header = FIELD_PREP(WMI_TLV_TAG,
						    WMI_TAG_PDEV_SMART_ANT_GPIO_HANDLE) |
					 FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);

		/* Set back the gpio pin and func values to 0 for ath11k chipsets. */

		gpio_param->gpio_pin = 0;
		gpio_param->gpio_func = 0;
		gpio_param->pdev_id = ar->pdev->pdev_id;
		gpio_param++;
	}

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PDEV_SMART_ANT_ENABLE_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_PDEV_SMART_ANT_ENABLE_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath11k_wmi_peer_set_smart_ant_node_config(struct ath11k *ar,
					  u8 mac_addr[ETH_ALEN],
					  struct ath11k_smart_ant_node_config_params *param)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_peer_set_smart_ant_node_config_ops_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int ret, len, args_tlv_len;
	u32 *node_config_args;

	args_tlv_len = TLV_HDR_SIZE + param->arg_count * sizeof(u32);
	len = sizeof(*cmd) + args_tlv_len;

	if (param->arg_count == 0) {
		ath11k_warn(ar->ab, "Argument count is 0\n");
		return -EINVAL;
	}

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_set_smart_ant_node_config_ops_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PEER_SMART_ANT_SET_NODE_CONFIG_OPS_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = param->vdev_id;
	cmd->args_count = param->arg_count;
	cmd->cmd_id = param->cmd_id;
	ether_addr_copy(cmd->mac_addr.addr, mac_addr);

	ptr = skb->data + sizeof(struct wmi_peer_set_smart_ant_node_config_ops_cmd);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_UINT32) |
		      FIELD_PREP(WMI_TLV_LEN,  param->arg_count * sizeof(u32));

	ptr += TLV_HDR_SIZE;

	node_config_args = (u32 *)ptr;

	node_config_args = param->arg_arr;

	ret = ath11k_wmi_cmd_send(wmi, skb,
				  WMI_PEER_SMART_ANT_SET_NODE_CONFIG_OPS_CMDID);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send WMI_PEER_SMART_ANT_SET_NODE_CONFIG_OPS CMD :%d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "WMI peer smart ant set node config ops: vdev_id: %d n_args: %d\n",
		   cmd->vdev_id, cmd->args_count);

	return ret;
}

int
ath11k_wmi_peer_set_smart_ant_train_info(struct ath11k *ar,
					 u32 vdev_id,
					 u8 mac_addr[ETH_ALEN],
					 struct ath11k_smart_ant_train_info *param)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_peer_set_smart_ant_train_ant_fixed_param_cmd *cmd;
	struct wmi_peer_set_smart_ant_train_ant_param *train_param;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int i, ret, len, itr = 0;

	len = sizeof(*cmd) + WMI_TLV_HDR_SIZE;
	len += WMI_SMART_MAX_RATE_SERIES *
	       sizeof(struct wmi_peer_set_smart_ant_train_ant_param);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = ptr;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PEER_SMART_ANT_SET_TRAIN_ANTENNA_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;
	ether_addr_copy(cmd->macaddr.addr, mac_addr);
	cmd->numpkts = param->numpkts;

	ptr = skb->data + sizeof(*cmd);

	len = WMI_SMART_MAX_RATE_SERIES *
	      sizeof(struct wmi_peer_set_smart_ant_train_ant_param);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, len);
	ptr += TLV_HDR_SIZE;

	train_param = ptr;

	for (i = 0; i < WMI_SMART_MAX_RATE_SERIES; i++) {
		train_param->tlv_header =
			FIELD_PREP(WMI_TLV_TAG,
				   WMI_TAG_PEER_SMART_ANT_SET_TRAIN_ANTENNA_PARAM) |
			FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
		train_param->train_rate_series_lo =
			((param->rate_array[itr] & ATH11K_SMART_ANT_MASK_RCODE) |
			 (param->rate_array[itr] & (ATH11K_SMART_ANT_MASK_RCODE << 16)));
		train_param->train_rate_series_hi =
			((param->rate_array[itr + 1] & ATH11K_SMART_ANT_MASK_RCODE) |
			 (param->rate_array[itr + 1] & (ATH11K_SMART_ANT_MASK_RCODE << 16)));
		itr += 2;
		train_param->train_antenna_series = param->antenna_array[i];
		train_param->rc_flags = 0;
		train_param++;
	}

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PEER_SMART_ANT_SET_TRAIN_INFO_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_PEER_SMART_ANT_SET_TRAIN_INFO_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath11k_wmi_pdev_enable_smart_ant(struct ath11k *ar,
				 struct ath11k_smart_ant_info *info)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_pdev_set_smart_ant_cmd *cmd;
	struct wmi_pdev_smart_ant_gpio_handle_cmd *gpio_param;
	struct wmi_tlv *tlv;
	struct smart_ant_enable_params *params = NULL;
	struct sk_buff *skb;
	void *ptr;
	int i;
	int ret, len;

	len = sizeof(*cmd) + TLV_HDR_SIZE +
	      WMI_SMART_ANTENNA_HAL_MAX * sizeof(*gpio_param);

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	params = kzalloc(sizeof(struct smart_ant_enable_params),
			 GFP_ATOMIC);
	if (!params)
		return -ENOMEM;

	ath11k_wmi_sa_set_gpio_param(ar, params);

	ptr = skb->data;

	cmd = ptr;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PDEV_SMART_ANT_ENABLE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	cmd->mode = WMI_SMART_ANT_MODE_PARALLEL;
	cmd->enable = info->enabled;
	cmd->rx_antenna = info->default_ant;
	cmd->tx_default_antenna = info->default_ant;

	ptr = skb->data + sizeof(*cmd);

	len = WMI_SMART_ANTENNA_HAL_MAX * sizeof(*gpio_param);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, len);
	ptr += TLV_HDR_SIZE;

	gpio_param = ptr;

	for (i = 0; i < WMI_SMART_ANTENNA_HAL_MAX; i++) {
		gpio_param->tlv_header = FIELD_PREP(WMI_TLV_TAG,
						    WMI_TAG_PDEV_SMART_ANT_GPIO_HANDLE) |
					 FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
		if (info->mode == WMI_SMART_ANT_MODE_SERIAL) {
			if (i < WMI_SMART_ANT_MAX_SERIAL_ANTENNA) {
				gpio_param->gpio_pin = params->gpio_pin[i];
				gpio_param->gpio_func = params->gpio_func[i];
			} else {
				gpio_param->gpio_pin = 0;
				gpio_param->gpio_func = 0;
			}
		} else if (info->mode == WMI_SMART_ANT_MODE_PARALLEL) {
			gpio_param->gpio_pin = params->gpio_pin[i];
			gpio_param->gpio_func = params->gpio_func[i];
		}
		gpio_param->pdev_id = ar->pdev->pdev_id;
		gpio_param++;
	}

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_PDEV_SMART_ANT_ENABLE_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_PDEV_SMART_ANT_ENABLE_CMDID\n");
		dev_kfree_skb(skb);
	}

	kfree(params);
	return ret;
}


int ath11k_wmi_vdev_adfs_ch_cfg_cmd_send(struct ath11k *ar, u32 vdev_id, struct cfg80211_chan_def *def)
{

	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_vdev_adfs_ch_cfg_cmd *cmd;
	struct sk_buff *skb;
	int ret = 0;

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_adfs_ch_cfg_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_VDEV_ADFS_CH_CFG_CMD) |
               		  FIELD_PREP(WMI_TLV_LEN,
				     sizeof(struct wmi_vdev_adfs_ch_cfg_cmd) - TLV_HDR_SIZE);
	cmd->vdev_id = vdev_id;

	if (ar->ab->dfs_region == ATH11K_DFS_REG_ETSI) {
		cmd->ocac_mode = WMI_ADFS_MODE_QUICK_OCAC;

		/* For ETSI, Agile CAC minimum time = 6 * CAC time */
		cmd->min_duration_ms = 6 * def->chan->dfs_cac_ms;
		if (def->chan->dfs_cac_ms != IEEE80211_DFS_MIN_CAC_TIME_MS)
			cmd->max_duration_ms = MAX_WEATHER_RADAR_CHAN_PRECAC_TIMEOUT;
		else
			cmd->max_duration_ms = MAX_PRECAC_TIMEOUT;

	} else if (ar->ab->dfs_region == ATH11K_DFS_REG_FCC) {
		cmd->ocac_mode = WMI_ADFS_MODE_QUICK_RCAC;
		cmd->min_duration_ms = MIN_RCAC_TIMEOUT;
		cmd->max_duration_ms = MAX_RCAC_TIMEOUT;
	}

	cmd->chan_freq = def->chan->center_freq;
	cmd->chan_width = ath11k_wmi_get_host_chan_width(def->width);
	cmd->center_freq1 = def->center_freq1;
	cmd->center_freq2 = def->center_freq2;

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "Send adfs channel cfg command for vdev id %d "
		   "mode as %d min duration %d chan_freq %d chan_width %d\n"
		   "center_freq1 %d center_freq2 %d", cmd->vdev_id,
		   cmd->ocac_mode, cmd->min_duration_ms, cmd->chan_freq,
		   cmd->chan_width, cmd->center_freq1, cmd->center_freq2);

	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_VDEV_ADFS_CH_CFG_CMDID);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to send WMI_VDEV_ADFS_CH_CFG_CMDID\n");
		dev_kfree_skb(skb);
       	}

	return ret;
}

int ath11k_wmi_vdev_adfs_ocac_abort_cmd_send(struct ath11k *ar, u32 vdev_id)
{
	struct ath11k_pdev_wmi *wmi = ar->wmi;
	struct wmi_vdev_adfs_ocac_abort_cmd *cmd;
	struct sk_buff *skb;
	int ret = 0;

	if (!ar->agile_chandef.chan) {
		ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
			   "Currently, agile CAC is not active on any channel."
			   "ignore abort");
		return ret;
	}

	skb = ath11k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_adfs_ocac_abort_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_VDEV_ADFS_OCAC_ABORT_CMD) |
			  FIELD_PREP(WMI_TLV_LEN,
				     sizeof(struct wmi_vdev_adfs_ocac_abort_cmd) - TLV_HDR_SIZE);

	cmd->vdev_id = vdev_id;
	ret = ath11k_wmi_cmd_send(wmi, skb, WMI_VDEV_ADFS_OCAC_ABORT_CMDID);

	if (ret) {
		ath11k_warn(ar->ab,
			"failed to send WMI_VDEV_ADFS_CH_CFG_CMDID\n");
		dev_kfree_skb(skb);
		return ret;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_WMI,
		   "Sent ADFS abort command to vdev %d", vdev_id);

	return ret;
}

