// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include "wmi.h"
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
#include "debugfs.h"
#include "debug.h"
#include "mac.h"
#include "hw.h"
#include "peer.h"
#include "p2p.h"
#include "testmode.h"
#include "dp_mon.h"
#include "vendor.h"
#include "cfr.h"
#include "ini.h"
#include "erp.h"

struct ath12k_wmi_svc_ready_parse {
	bool wmi_svc_bitmap_done;
};

struct wmi_tlv_fw_stats_parse {
	const struct wmi_stats_event *ev;
	const struct wmi_per_chain_rssi_stat_params *rssi;
	int rssi_num;
	bool chain_rssi_done;
	struct ath12k_fw_stats *stats;
};

struct ath12k_wmi_dma_ring_caps_parse {
	struct ath12k_wmi_dma_ring_caps_params *dma_ring_caps;
	u32 n_dma_ring_caps;
};

struct ath12k_wmi_service_ext_arg {
	u32 default_conc_scan_config_bits;
	u32 default_fw_config_bits;
	struct ath12k_wmi_ppe_threshold_arg ppet;
	u32 he_cap_info;
	u32 mpdu_density;
	u32 max_bssid_rx_filters;
	u32 num_hw_modes;
	u32 num_phy;
	u32 num_chainmask_tables;
	struct ath12k_chainmask_table
		chainmask_table[ATH12K_MAX_CHAINMASK_TABLES];
};

struct ath12k_wmi_svc_rdy_ext_parse {
	struct ath12k_wmi_service_ext_arg arg;
	const struct ath12k_wmi_soc_mac_phy_hw_mode_caps_params *hw_caps;
	const struct ath12k_wmi_hw_mode_cap_params *hw_mode_caps;
	u32 n_hw_mode_caps;
	u32 tot_phy_id;
	struct ath12k_wmi_hw_mode_cap_params pref_hw_mode_caps;
	struct ath12k_wmi_mac_phy_caps_params *mac_phy_caps;
	u32 n_mac_phy_caps;
	const struct ath12k_wmi_soc_hal_reg_caps_params *soc_hal_reg_caps;
	const struct ath12k_wmi_hal_reg_caps_ext_params *ext_hal_reg_caps;
	u32 n_ext_hal_reg_caps;
	struct ath12k_wmi_dma_ring_caps_parse dma_caps_parse;
	bool hw_mode_done;
	bool mac_phy_done;
	bool ext_hal_reg_done;
	u32 n_mac_phy_chainmask_combo;
	bool mac_phy_chainmask_combo_done;
	bool mac_phy_chainmask_cap_done;
	bool oem_dma_ring_cap_done;
	bool dma_ring_cap_done;
};

/**
 * ath12k_wmi_svc_rdy_ext2_arg - WMI service ready extended 2
 * @afc_deployment_type: AFC deployment type (indoor, outdoor)
 */
struct ath12k_wmi_svc_rdy_ext2_arg {
	u32 reg_db_version;
	u32 hw_min_max_tx_power_2ghz;
	u32 hw_min_max_tx_power_5ghz;
	u32 chwidth_num_peer_caps;
	u32 preamble_puncture_bw;
	u32 max_user_per_ppdu_ofdma;
	u32 max_user_per_ppdu_mumimo;
	u32 target_cap_flags;
	u32 eht_cap_mac_info[WMI_MAX_EHTCAP_MAC_SIZE];
	u32 max_num_linkview_peers;
	u32 max_tid_msduq;
	u32 def_tid_msduq;
	u32 afc_deployment_type;
};

struct ath12k_wmi_svc_rdy_ext2_parse {
	struct ath12k_wmi_svc_rdy_ext2_arg arg;
	struct ath12k_wmi_dma_ring_caps_parse dma_caps_parse;
	bool dma_ring_cap_done;
	bool spectral_bin_scaling_done;
	bool mac_phy_caps_ext_done;
};

struct ath12k_wmi_rdy_parse {
	u32 num_extra_mac_addr;
};

struct ath12k_wmi_dma_buf_release_arg {
	struct ath12k_wmi_dma_buf_release_fixed_params fixed;
	const struct ath12k_wmi_dma_buf_release_entry_params *buf_entry;
	const struct ath12k_wmi_dma_buf_release_meta_data_params *meta_data;
	u32 num_buf_entry;
	u32 num_meta;
	bool buf_entry_done;
	bool meta_data_done;
};

struct wmi_pdev_sscan_fw_param_parse {
	struct ath12k_wmi_pdev_sscan_fw_cmd_fixed_param fixed;
	struct ath12k_wmi_pdev_sscan_fft_bin_index *bin;
	struct ath12k_wmi_pdev_sscan_chan_info ch_info;
	struct ath12k_wmi_pdev_sscan_per_detector_info *det_info;
	bool bin_entry_done;
	bool det_info_entry_done;

};

struct wmi_spectral_capabilities_parse {
	struct ath12k_wmi_spectral_scan_bw_capabilities *sscan_bw_caps;
	struct ath12k_wmi_spectral_fft_size_capabilities *fft_size_caps;
	bool sscan_bw_caps_entry_done;
	bool fft_size_caps_entry_done;
	u32 num_bw_caps_entry;
	u32 num_fft_size_caps_entry;
};

struct ath12k_wmi_tlv_policy {
	size_t min_len;
};

struct wmi_tlv_mgmt_rx_parse {
	const struct ath12k_wmi_mgmt_rx_params *fixed;
	const u8 *frame_buf;
	bool frame_buf_done;
	struct ath12k_mgmt_rx_cu_arg cu_params;
	struct ath12k_wmi_mgmt_rx_mlo_link_removal_info
		*link_removal_info[TARGET_NUM_VDEVS * ATH12K_WMI_MLO_MAX_LINKS];
	u32 num_link_removal_info_count;
	bool mgmt_ml_info_done;
	bool bpcc_buf_done;
	bool parse_link_removal_info_done;
	struct ath12k_wmi_mgmt_rx_mlo_bcast_ttlm_info
		*bcast_ttlm_info[TARGET_NUM_VDEVS - 1];
	u32 num_bcast_ttlm_info_count;
	bool parse_bcast_ttlm_info_done;
};

static const struct ath12k_wmi_tlv_policy ath12k_wmi_tlv_policies[] = {
	[WMI_TAG_ARRAY_BYTE] = { .min_len = 0 },
	[WMI_TAG_ARRAY_UINT32] = { .min_len = 0 },
	[WMI_TAG_SERVICE_READY_EVENT] = {
		.min_len = sizeof(struct wmi_service_ready_event) },
	[WMI_TAG_SERVICE_READY_EXT_EVENT] = {
		.min_len = sizeof(struct wmi_service_ready_ext_event) },
	[WMI_TAG_SOC_MAC_PHY_HW_MODE_CAPS] = {
		.min_len = sizeof(struct ath12k_wmi_soc_mac_phy_hw_mode_caps_params) },
	[WMI_TAG_SOC_HAL_REG_CAPABILITIES] = {
		.min_len = sizeof(struct ath12k_wmi_soc_hal_reg_caps_params) },
	[WMI_TAG_VDEV_START_RESPONSE_EVENT] = {
		.min_len = sizeof(struct wmi_vdev_start_resp_event) },
	[WMI_TAG_PEER_DELETE_RESP_EVENT] = {
		.min_len = sizeof(struct wmi_peer_delete_resp_event) },
	[WMI_TAG_OFFLOAD_BCN_TX_STATUS_EVENT] = {
		.min_len = sizeof(struct wmi_bcn_tx_status_event) },
	[WMI_TAG_VDEV_STOPPED_EVENT] = {
		.min_len = sizeof(struct wmi_vdev_stopped_event) },
	[WMI_TAG_REG_CHAN_LIST_CC_EXT_EVENT] = {
		.min_len = sizeof(struct wmi_reg_chan_list_cc_ext_event) },
	[WMI_TAG_MGMT_RX_HDR] = {
		.min_len = sizeof(struct ath12k_wmi_mgmt_rx_params) },
	[WMI_TAG_MGMT_TX_COMPL_EVENT] = {
		.min_len = sizeof(struct wmi_mgmt_tx_compl_event) },
	[WMI_TAG_SCAN_EVENT] = {
		.min_len = sizeof(struct wmi_scan_event) },
	[WMI_TAG_PEER_STA_KICKOUT_EVENT] = {
		.min_len = sizeof(struct wmi_peer_sta_kickout_event) },
	[WMI_TAG_ROAM_EVENT] = {
		.min_len = sizeof(struct wmi_roam_event) },
	[WMI_TAG_CHAN_INFO_EVENT] = {
		.min_len = sizeof(struct wmi_chan_info_event) },
	[WMI_TAG_PDEV_BSS_CHAN_INFO_EVENT] = {
		.min_len = sizeof(struct wmi_pdev_bss_chan_info_event) },
	[WMI_TAG_VDEV_INSTALL_KEY_COMPLETE_EVENT] = {
		.min_len = sizeof(struct wmi_vdev_install_key_compl_event) },
	[WMI_TAG_READY_EVENT] = {
		.min_len = sizeof(struct ath12k_wmi_ready_event_min_params) },
	[WMI_TAG_SERVICE_AVAILABLE_EVENT] = {
		.min_len = sizeof(struct wmi_service_available_event) },
	[WMI_TAG_PEER_ASSOC_CONF_EVENT] = {
		.min_len = sizeof(struct wmi_peer_assoc_conf_event) },
	[WMI_TAG_RFKILL_EVENT] = {
		.min_len = sizeof(struct wmi_rfkill_state_change_event) },
	[WMI_TAG_PDEV_CTL_FAILSAFE_CHECK_EVENT] = {
		.min_len = sizeof(struct wmi_pdev_ctl_failsafe_chk_event) },
	[WMI_TAG_HOST_SWFDA_EVENT] = {
		.min_len = sizeof(struct wmi_fils_discovery_event) },
	[WMI_TAG_OFFLOAD_PRB_RSP_TX_STATUS_EVENT] = {
		.min_len = sizeof(struct wmi_probe_resp_tx_status_event) },
	[WMI_TAG_VDEV_DELETE_RESP_EVENT] = {
		.min_len = sizeof(struct wmi_vdev_delete_resp_event) },
	[WMI_TAG_TWT_ENABLE_COMPLETE_EVENT] = {
		.min_len = sizeof(struct wmi_twt_enable_event) },
	[WMI_TAG_TWT_DISABLE_COMPLETE_EVENT] = {
		.min_len = sizeof(struct wmi_twt_disable_event) },
	[WMI_TAG_P2P_NOA_INFO] = {
		.min_len = sizeof(struct ath12k_wmi_p2p_noa_info) },
	[WMI_TAG_P2P_NOA_EVENT] = {
		.min_len = sizeof(struct wmi_p2p_noa_event) },
	[WMI_TAG_PER_CHAIN_RSSI_STATS] = {
		.min_len = sizeof(struct wmi_per_chain_rssi_stat_params) },
	[WMI_TAG_11D_NEW_COUNTRY_EVENT] = {
		.min_len = sizeof(struct wmi_11d_new_cc_event) },
	[WMI_TAG_PEER_CREATE_RESP_EVENT] = {
		.min_len = sizeof(struct ath12k_wmi_peer_create_conf_ev) },
	[WMI_TAG_MUEDCA_PARAMS_CONFIG_EVENT] = {
		.min_len = sizeof(struct wmi_pdev_update_muedca_event) },
	[WMI_TAG_TWT_ADD_DIALOG_COMPLETE_EVENT] = {
		.min_len = sizeof(struct wmi_twt_add_dialog_event) },
	[WMI_TAG_OBSS_COLOR_COLLISION_EVT]
		= { .min_len = sizeof(struct wmi_obss_color_collision_event) },
	[WMI_TAG_TWT_BTWT_INVITE_STA_COMPLETE_EVENT] = {
		.min_len = sizeof(struct wmi_twt_btwt_invite_sta_event) },
	[WMI_TAG_TWT_BTWT_REMOVE_STA_COMPLETE_EVENT] = {
		.min_len = sizeof(struct wmi_twt_btwt_invite_sta_event) },
	[WMI_TAG_OFFCHAN_DATA_TX_COMPL_EVENT] = {
		.min_len = sizeof(struct wmi_offchan_data_tx_compl_event) },
};

__le32 ath12k_wmi_tlv_hdr(u32 cmd, u32 len)
{
	return le32_encode_bits(cmd, WMI_TLV_TAG) |
		le32_encode_bits(len, WMI_TLV_LEN);
}

__le32 ath12k_wmi_tlv_cmd_hdr(u32 cmd, u32 len)
{
	return ath12k_wmi_tlv_hdr(cmd, len - TLV_HDR_SIZE);
}

#define PRIMAP(_hw_mode_) \
	[_hw_mode_] = _hw_mode_##_PRI

static const int ath12k_hw_mode_pri_map[] = {
	PRIMAP(WMI_HOST_HW_MODE_SINGLE),
	PRIMAP(WMI_HOST_HW_MODE_DBS),
	PRIMAP(WMI_HOST_HW_MODE_SBS_PASSIVE),
	PRIMAP(WMI_HOST_HW_MODE_SBS),
	PRIMAP(WMI_HOST_HW_MODE_DBS_SBS),
	PRIMAP(WMI_HOST_HW_MODE_DBS_OR_SBS),
	/* keep last */
	PRIMAP(WMI_HOST_HW_MODE_MAX),
};

enum ath12k_type_req_ctrl_path_stats_id {
       TYPE_REQ_CTRL_PATH_PDEV_TX_STAT = 0,
       TYPE_REQ_CTRL_PATH_VDEV_EXTD_STAT,
       TYPE_REQ_CTRL_PATH_MEM_STAT,
       TYPE_REQ_CTRL_PATH_TWT_STAT,
       TYPE_REQ_CTRL_PATH_BMISS_STAT,
       TYPE_REQ_CTRL_PATH_PMLO_STAT,
       TYPE_REQ_CTRL_PATH_RRM_STA_STAT,
};

const char *mgmt_frame_name[] = { "AssocReq",
			    "AssocResp",
			    "ReAssocReq",
			    "ReAssocResp",
			    "ProbeReq",
			    "ProbeResp",
			     0, 0,
			    "Beacon",
			     0,
			    "DisAssoc",
			    "Auth",
			    "Deauth",
			    "Action"};

int ath12k_wmi_pdev_enable_telemetry_stats(struct ath12k_base *ab,
	struct ath12k *ar,
	struct wmi_request_ctrl_path_stats_cmd_fixed_param *param)
{
       struct ath12k_wmi_pdev *wmi = ar->wmi;
       enum ath12k_type_req_ctrl_path_stats_id req_id = TYPE_REQ_CTRL_PATH_PMLO_STAT;
	struct wmi_request_ctrl_path_stats_cmd_fixed_param *cmd;
       u32 pdev_id_array;
       u32 num_pdev_ids = 1;
       int len, ret;
       struct wmi_tlv *tlv;
       struct sk_buff *skb;
       void *ptr;

	len = sizeof(struct wmi_request_ctrl_path_stats_cmd_fixed_param) +
		TLV_HDR_SIZE + (sizeof(u32) * (num_pdev_ids));

       skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
       if (!skb)
               return -ENOMEM;

       cmd = (void *)skb->data;
       cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_CTRL_PATH_STATS_CMD_FIXED_PARAM,
                                                sizeof(*cmd));

       cmd->stats_id_mask = (1 << WMI_REQ_CTRL_PATH_PMLO_STAT);
	if (!param) {
		cmd->request_id = req_id;
		cmd->action = WMI_REQUEST_CTRL_PATH_STAT_PERIODIC_PUBLISH;
		cmd->subid = ATH12K_WMI_SUBID_PERIODICITY_1_SEC;
	} else {
		cmd->request_id = param->request_id;
		cmd->action = param->action;
		cmd->subid = param->subid;
	}

       pdev_id_array = ar->pdev->pdev_id;

       ptr = skb->data + sizeof(*cmd);

       tlv = ptr;
       tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_UINT32) |
               FIELD_PREP(WMI_TLV_LEN, sizeof(u32) * num_pdev_ids);
       ptr += TLV_HDR_SIZE;
       memcpy(ptr, &pdev_id_array, sizeof(pdev_id_array));

       ret = ath12k_wmi_cmd_send(wmi, skb, WMI_REQUEST_CTRL_PATH_STATS_CMDID);
       if (ret) {
               dev_kfree_skb(skb);
               ath12k_warn(ab, "Failed to send WMI_REQUEST_CTRL_PATH_STATS_CMDID: %d", ret);
       }

       return ret;
}


enum wmi_host_channel_width
ath12k_wmi_get_host_chan_width(u32 width)
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
	case NL80211_CHAN_WIDTH_320:
		host_width = WMI_HOST_CHAN_WIDTH_320;
		break;
	default:
		host_width = WMI_HOST_CHAN_WIDTH_MAX;
		break;
	}

	return host_width;
}

static int
ath12k_wmi_tlv_iter(struct ath12k_base *ab, const void *ptr, size_t len,
		    int (*iter)(struct ath12k_base *ab, u16 tag, u16 len,
				const void *ptr, void *data),
		    void *data)
{
	const void *begin = ptr;
	const struct wmi_tlv *tlv;
	u16 tlv_tag, tlv_len;
	int ret;


	while (len > 0) {
		if (len < sizeof(*tlv)) {
			ath12k_err(ab, "wmi tlv parse failure at byte %zd (%zu bytes left, %zu expected)\n",
				   ptr - begin, len, sizeof(*tlv));
			return -EINVAL;
		}

		tlv = ptr;
		tlv_tag = le32_get_bits(tlv->header, WMI_TLV_TAG);
		tlv_len = le32_get_bits(tlv->header, WMI_TLV_LEN);
		ptr += sizeof(*tlv);
		len -= sizeof(*tlv);

		if (tlv_len > len) {
			ath12k_err(ab, "wmi tlv parse failure of tag %u at byte %zd (%zu bytes left, %u expected)\n",
				   tlv_tag, ptr - begin, len, tlv_len);
			return -EINVAL;
		}

		if (tlv_tag < ARRAY_SIZE(ath12k_wmi_tlv_policies) &&
		    ath12k_wmi_tlv_policies[tlv_tag].min_len &&
		    ath12k_wmi_tlv_policies[tlv_tag].min_len > tlv_len) {
			ath12k_err(ab, "wmi tlv parse failure of tag %u at byte %zd (%u bytes is less than min length %zu)\n",
				   tlv_tag, ptr - begin, tlv_len,
				   ath12k_wmi_tlv_policies[tlv_tag].min_len);
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

static int ath12k_wmi_tlv_iter_parse(struct ath12k_base *ab, u16 tag, u16 len,
				     const void *ptr, void *data)
{
	const void **tb = data;

	if (tag < WMI_TAG_MAX)
		tb[tag] = ptr;

	return 0;
}

static int ath12k_wmi_tlv_parse(struct ath12k_base *ar, const void **tb,
				const void *ptr, size_t len)
{
	return ath12k_wmi_tlv_iter(ar, ptr, len, ath12k_wmi_tlv_iter_parse,
				   (void *)tb);
}

static const void **
ath12k_wmi_tlv_parse_alloc(struct ath12k_base *ab,
			   struct sk_buff *skb, gfp_t gfp)
{
	const void **tb;
	int ret;

	tb = kcalloc(WMI_TAG_MAX, sizeof(*tb), gfp);
	if (!tb)
		return ERR_PTR(-ENOMEM);

	ret = ath12k_wmi_tlv_parse(ab, tb, skb->data, skb->len);
	if (ret) {
		kfree(tb);
		return ERR_PTR(ret);
	}

	return tb;
}

static int ath12k_wmi_cmd_send_nowait(struct ath12k_wmi_pdev *wmi, struct sk_buff *skb,
				      u32 cmd_id)
{
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_cmd_hdr *cmd_hdr;
	int ret;

	if (!skb_push(skb, sizeof(struct wmi_cmd_hdr)))
		return -ENOMEM;

	cmd_hdr = (struct wmi_cmd_hdr *)skb->data;
	cmd_hdr->cmd_id = le32_encode_bits(cmd_id, WMI_CMD_HDR_CMD_ID);
	WMI_COMMAND_RECORD(wmi, skb, cmd_id);

	memset(skb_cb, 0, sizeof(*skb_cb));
	ret = ath12k_htc_send(&ab->htc, wmi->eid, skb);

	if (ret)
		goto err_pull;

	return 0;

err_pull:
	skb_pull(skb, sizeof(struct wmi_cmd_hdr));
	return ret;
}

int ath12k_wmi_cmd_send(struct ath12k_wmi_pdev *wmi, struct sk_buff *skb,
			u32 cmd_id)
{
	struct ath12k_wmi_base *wmi_ab = wmi->wmi_ab;
	struct ath12k_base *ab = wmi_ab->ab;
	int ret = -EOPNOTSUPP;

	if (!(test_bit(ATH12K_FLAG_WMI_INIT_DONE, &wmi_ab->ab->dev_flags)) &&
		cmd_id != WMI_INIT_CMDID)
		return -ESHUTDOWN;

	might_sleep();

	if (ab->hw_params->credit_flow) {
		wait_event_timeout(wmi_ab->tx_credits_wq, ({
			ret = ath12k_wmi_cmd_send_nowait(wmi, skb, cmd_id);

			if (ret && test_bit(ATH12K_FLAG_CRASH_FLUSH,
					    &wmi_ab->ab->dev_flags))
				ret = -ESHUTDOWN;
			(ret != -EAGAIN);
			}), WMI_SEND_TIMEOUT_HZ);
	} else {
		wait_event_timeout(wmi->tx_ce_desc_wq, ({
			ret = ath12k_wmi_cmd_send_nowait(wmi, skb, cmd_id);
			if (ret && test_bit(ATH12K_FLAG_CRASH_FLUSH,
					    &wmi_ab->ab->dev_flags))
				ret = -ESHUTDOWN;

			(ret != -ENOBUFS);
			}), WMI_SEND_TIMEOUT_HZ);
	}

	if (ret == -EAGAIN)
		ath12k_warn(wmi_ab->ab, "wmi command %d timeout\n", cmd_id);

	if (ret == -ENOBUFS)
		ath12k_warn(wmi_ab->ab, "ce desc not available for wmi command %d\n",
			    cmd_id);

	return ret;
}

static int ath12k_pull_svc_ready_ext(struct ath12k_wmi_pdev *wmi_handle,
				     const void *ptr,
				     struct ath12k_wmi_service_ext_arg *arg)
{
	const struct wmi_service_ready_ext_event *ev = ptr;
	int i;

	if (!ev)
		return -EINVAL;

	/* Move this to host based bitmap */
	arg->default_conc_scan_config_bits =
		le32_to_cpu(ev->default_conc_scan_config_bits);
	arg->default_fw_config_bits = le32_to_cpu(ev->default_fw_config_bits);
	arg->he_cap_info = le32_to_cpu(ev->he_cap_info);
	arg->mpdu_density = le32_to_cpu(ev->mpdu_density);
	arg->max_bssid_rx_filters = le32_to_cpu(ev->max_bssid_rx_filters);
	arg->ppet.numss_m1 = le32_to_cpu(ev->ppet.numss_m1);
	arg->ppet.ru_bit_mask = le32_to_cpu(ev->ppet.ru_info);

	for (i = 0; i < WMI_MAX_NUM_SS; i++)
		arg->ppet.ppet16_ppet8_ru3_ru0[i] =
			le32_to_cpu(ev->ppet.ppet16_ppet8_ru3_ru0[i]);

	return 0;
}

static int
ath12k_pull_mac_phy_cap_svc_ready_ext(struct ath12k_wmi_pdev *wmi_handle,
				      struct ath12k_wmi_svc_rdy_ext_parse *svc,
				      u8 hw_mode_id, u8 phy_id,
				      struct ath12k_pdev *pdev)
{
	const struct ath12k_wmi_mac_phy_caps_params *mac_caps;
	const struct ath12k_wmi_soc_mac_phy_hw_mode_caps_params *hw_caps = svc->hw_caps;
	const struct ath12k_wmi_hw_mode_cap_params *wmi_hw_mode_caps = svc->hw_mode_caps;
	const struct ath12k_wmi_mac_phy_caps_params *wmi_mac_phy_caps = svc->mac_phy_caps;
	struct ath12k_base *ab = wmi_handle->wmi_ab->ab;
	struct ath12k_band_cap *cap_band;
	struct ath12k_pdev_cap *pdev_cap = &pdev->cap;
	struct ath12k_fw_pdev *fw_pdev;
	u32 phy_map;
	u32 hw_idx, phy_idx = 0;
	int i;

	if (!hw_caps || !wmi_hw_mode_caps || !svc->soc_hal_reg_caps)
		return -EINVAL;

	for (hw_idx = 0; hw_idx < le32_to_cpu(hw_caps->num_hw_modes); hw_idx++) {
		if (hw_mode_id == le32_to_cpu(wmi_hw_mode_caps[hw_idx].hw_mode_id))
			break;

		phy_map = le32_to_cpu(wmi_hw_mode_caps[hw_idx].phy_id_map);
		phy_idx = fls(phy_map);
	}

	if (hw_idx == le32_to_cpu(hw_caps->num_hw_modes))
		return -EINVAL;

	phy_idx += phy_id;
	if (phy_id >= le32_to_cpu(svc->soc_hal_reg_caps->num_phy))
		return -EINVAL;

	mac_caps = wmi_mac_phy_caps + phy_idx;

	pdev->pdev_id = ath12k_wmi_mac_phy_get_pdev_id(mac_caps);
	pdev->hw_link_id = ath12k_wmi_mac_phy_get_hw_link_id(mac_caps);
	pdev_cap->supported_bands |= le32_to_cpu(mac_caps->supported_bands);
	pdev_cap->ampdu_density = le32_to_cpu(mac_caps->ampdu_density);
	pdev_cap->chainmask_table_id = mac_caps->chainmask_table_id;

	fw_pdev = &ab->fw_pdev[ab->fw_pdev_count];
	fw_pdev->supported_bands = le32_to_cpu(mac_caps->supported_bands);
	fw_pdev->pdev_id = ath12k_wmi_mac_phy_get_pdev_id(mac_caps);
	fw_pdev->phy_id = le32_to_cpu(mac_caps->phy_id);
	ab->fw_pdev_count++;

	/* Take non-zero tx/rx chainmask. If tx/rx chainmask differs from
	 * band to band for a single radio, need to see how this should be
	 * handled.
	 */
	if (le32_to_cpu(mac_caps->supported_bands) & WMI_HOST_WLAN_2GHZ_CAP) {
		pdev_cap->vht_cap = le32_to_cpu(mac_caps->vht_cap_info_2g);
		pdev_cap->vht_mcs = le32_to_cpu(mac_caps->vht_supp_mcs_2g);
		pdev_cap->tx_chain_mask = le32_to_cpu(mac_caps->tx_chain_mask_2g);
		pdev_cap->rx_chain_mask = le32_to_cpu(mac_caps->rx_chain_mask_2g);
	} else if (le32_to_cpu(mac_caps->supported_bands) & WMI_HOST_WLAN_5GHZ_CAP) {
		pdev_cap->vht_cap = le32_to_cpu(mac_caps->vht_cap_info_5g);
		pdev_cap->vht_mcs = le32_to_cpu(mac_caps->vht_supp_mcs_5g);
		pdev_cap->he_mcs = le32_to_cpu(mac_caps->he_supp_mcs_5g);
		pdev_cap->tx_chain_mask = le32_to_cpu(mac_caps->tx_chain_mask_5g);
		pdev_cap->rx_chain_mask = le32_to_cpu(mac_caps->rx_chain_mask_5g);
		pdev_cap->nss_ratio_enabled =
			WMI_NSS_RATIO_EN_DIS_GET(mac_caps->nss_ratio);
		pdev_cap->nss_ratio_info =
			WMI_NSS_RATIO_INFO_GET(mac_caps->nss_ratio);
	} else {
		return -EINVAL;
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

	if (le32_to_cpu(mac_caps->supported_bands) & WMI_HOST_WLAN_2GHZ_CAP) {
		cap_band = &pdev_cap->band[NL80211_BAND_2GHZ];
		cap_band->phy_id = le32_to_cpu(mac_caps->phy_id);
		cap_band->max_bw_supported = le32_to_cpu(mac_caps->max_bw_supported_2g);
		cap_band->ht_cap_info = le32_to_cpu(mac_caps->ht_cap_info_2g);
		cap_band->he_cap_info[0] = le32_to_cpu(mac_caps->he_cap_info_2g);
		cap_band->he_cap_info[1] = le32_to_cpu(mac_caps->he_cap_info_2g_ext);
		cap_band->he_mcs = le32_to_cpu(mac_caps->he_supp_mcs_2g);
		for (i = 0; i < WMI_MAX_HECAP_PHY_SIZE; i++)
			cap_band->he_cap_phy_info[i] =
				le32_to_cpu(mac_caps->he_cap_phy_info_2g[i]);

		cap_band->he_ppet.numss_m1 = le32_to_cpu(mac_caps->he_ppet2g.numss_m1);
		cap_band->he_ppet.ru_bit_mask = le32_to_cpu(mac_caps->he_ppet2g.ru_info);

		for (i = 0; i < WMI_MAX_NUM_SS; i++)
			cap_band->he_ppet.ppet16_ppet8_ru3_ru0[i] =
				le32_to_cpu(mac_caps->he_ppet2g.ppet16_ppet8_ru3_ru0[i]);
	}

	if (le32_to_cpu(mac_caps->supported_bands) & WMI_HOST_WLAN_5GHZ_CAP) {
		cap_band = &pdev_cap->band[NL80211_BAND_5GHZ];
		cap_band->phy_id = le32_to_cpu(mac_caps->phy_id);
		cap_band->max_bw_supported =
			le32_to_cpu(mac_caps->max_bw_supported_5g);
		cap_band->ht_cap_info = le32_to_cpu(mac_caps->ht_cap_info_5g);
		cap_band->he_cap_info[0] = le32_to_cpu(mac_caps->he_cap_info_5g);
		cap_band->he_cap_info[1] = le32_to_cpu(mac_caps->he_cap_info_5g_ext);
		cap_band->he_mcs = le32_to_cpu(mac_caps->he_supp_mcs_5g);
		for (i = 0; i < WMI_MAX_HECAP_PHY_SIZE; i++)
			cap_band->he_cap_phy_info[i] =
				le32_to_cpu(mac_caps->he_cap_phy_info_5g[i]);

		cap_band->he_ppet.numss_m1 = le32_to_cpu(mac_caps->he_ppet5g.numss_m1);
		cap_band->he_ppet.ru_bit_mask = le32_to_cpu(mac_caps->he_ppet5g.ru_info);

		for (i = 0; i < WMI_MAX_NUM_SS; i++)
			cap_band->he_ppet.ppet16_ppet8_ru3_ru0[i] =
				le32_to_cpu(mac_caps->he_ppet5g.ppet16_ppet8_ru3_ru0[i]);

		cap_band = &pdev_cap->band[NL80211_BAND_6GHZ];
		cap_band->max_bw_supported =
			le32_to_cpu(mac_caps->max_bw_supported_5g);
		cap_band->ht_cap_info = le32_to_cpu(mac_caps->ht_cap_info_5g);
		cap_band->he_cap_info[0] = le32_to_cpu(mac_caps->he_cap_info_5g);
		cap_band->he_cap_info[1] = le32_to_cpu(mac_caps->he_cap_info_5g_ext);
		cap_band->he_mcs = le32_to_cpu(mac_caps->he_supp_mcs_5g);
		for (i = 0; i < WMI_MAX_HECAP_PHY_SIZE; i++)
			cap_band->he_cap_phy_info[i] =
				le32_to_cpu(mac_caps->he_cap_phy_info_5g[i]);

		cap_band->he_ppet.numss_m1 = le32_to_cpu(mac_caps->he_ppet5g.numss_m1);
		cap_band->he_ppet.ru_bit_mask = le32_to_cpu(mac_caps->he_ppet5g.ru_info);

		for (i = 0; i < WMI_MAX_NUM_SS; i++)
			cap_band->he_ppet.ppet16_ppet8_ru3_ru0[i] =
				le32_to_cpu(mac_caps->he_ppet5g.ppet16_ppet8_ru3_ru0[i]);
	}

	return 0;
}

static void ath12k_wmi_process_mvr_event(struct ath12k_base *ab, u32 *vdev_id_bm,
					 u32 num_vdev_bm)
{
	struct ath12k *ar = NULL;
	struct ath12k_link_vif *arvif = NULL;
	u32 vdev_bitmap, bit_pos;

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "wmi mvr resp num_vdev_bm %d vdev_id_bm[0]=0x%x vdev_id_bm[1]=0x%x\n",
		   num_vdev_bm, vdev_id_bm[0],
		   (num_vdev_bm == WMI_MVR_RESP_VDEV_BM_MAX_LEN ?
				   vdev_id_bm[1] : 0x00));

	/* 31-0 bits processing */
	vdev_bitmap = vdev_id_bm[0];

	for (bit_pos = 0; bit_pos < 32; bit_pos++) {

		if (!(vdev_bitmap & BIT(bit_pos)))
			continue;

		arvif = ath12k_mac_get_arvif_by_vdev_id(ab, bit_pos);
		if (!arvif) {
			ath12k_warn(ab, "wmi mvr resp for unknown vdev %d", bit_pos);
			continue;
		}

		arvif->mvr_processing = false;
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "wmi mvr vdev %d restarted\n", bit_pos);
	}

	/* TODO: 63-32 bits processing
	 * Add support to parse bitmap once support for
	 * TARGET_NUM_VDEVS > 32 is added
	 */

	if (arvif)
		ar = arvif->ar;

	if (ar)
		complete(&ar->mvr_complete);
}

static int ath12k_wmi_tlv_mvr_event_parse(struct ath12k_base *ab,
					  u16 tag, u16 len,
					  const void *ptr, void *data)
{
	struct wmi_pdev_mvr_resp_event_parse *parse = data;
	struct wmi_pdev_mvr_resp_event_fixed_param *fixed_param;

	switch(tag) {
	case WMI_TAG_MULTIPLE_VDEV_RESTART_RESPONSE_EVENT:
		fixed_param = (struct wmi_pdev_mvr_resp_event_fixed_param *)ptr;

		if (fixed_param->status) {
			ath12k_warn(ab, "wmi mvr resp event status %u\n",
				    fixed_param->status);
			return -EINVAL;
		}

		memcpy(&parse->fixed_param, fixed_param,
		       sizeof(struct wmi_pdev_mvr_resp_event_fixed_param));
		break;
	case WMI_TAG_ARRAY_UINT32:
		if ((len > WMI_MVR_RESP_VDEV_BM_MAX_LEN_BYTES) || (len == 0)) {
			ath12k_warn(ab, "wmi invalid vdev id len in mvr resp %u\n",
				    len);
			return -EINVAL;
		}

		parse->num_vdevs_bm = len / sizeof(u32);
		memcpy(parse->vdev_id_bm, ptr, len);
		break;
	default:
		break;
	}

	return 0;
}

static void ath12k_wmi_event_mvr_response(struct ath12k_base *ab,
					  struct sk_buff *skb)
{
	struct wmi_pdev_mvr_resp_event_parse parse = {};
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_mvr_event_parse,
				  &parse);
	if (ret) {
		ath12k_warn(ab, "wmi failed to parse mvr response tlv %d\n",
			    ret);
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI, "wmi mvr resp for pdev %d\n",
		   parse.fixed_param.pdev_id);

	ath12k_wmi_process_mvr_event(ab, parse.vdev_id_bm, parse.num_vdevs_bm);
}

static int
ath12k_pull_reg_cap_svc_rdy_ext(struct ath12k_wmi_pdev *wmi_handle,
				const struct ath12k_wmi_soc_hal_reg_caps_params *reg_caps,
				const struct ath12k_wmi_hal_reg_caps_ext_params *ext_caps,
				u8 phy_idx,
				struct ath12k_wmi_hal_reg_capabilities_ext_arg *param)
{
	const struct ath12k_wmi_hal_reg_caps_ext_params *ext_reg_cap;

	if (!reg_caps || !ext_caps)
		return -EINVAL;

	if (phy_idx >= le32_to_cpu(reg_caps->num_phy))
		return -EINVAL;

	ext_reg_cap = &ext_caps[phy_idx];

	param->phy_id = le32_to_cpu(ext_reg_cap->phy_id);
	param->eeprom_reg_domain = le32_to_cpu(ext_reg_cap->eeprom_reg_domain);
	param->eeprom_reg_domain_ext =
		le32_to_cpu(ext_reg_cap->eeprom_reg_domain_ext);
	param->regcap1 = le32_to_cpu(ext_reg_cap->regcap1);
	param->regcap2 = le32_to_cpu(ext_reg_cap->regcap2);
	/* check if param->wireless_mode is needed */
	param->low_2ghz_chan = le32_to_cpu(ext_reg_cap->low_2ghz_chan);
	param->high_2ghz_chan = le32_to_cpu(ext_reg_cap->high_2ghz_chan);
	param->low_5ghz_chan = le32_to_cpu(ext_reg_cap->low_5ghz_chan);
	param->high_5ghz_chan = le32_to_cpu(ext_reg_cap->high_5ghz_chan);

	return 0;
}

static int ath12k_pull_service_ready_tlv(struct ath12k_base *ab,
					 const void *evt_buf,
					 struct ath12k_wmi_target_cap_arg *cap)
{
	const struct wmi_service_ready_event *ev = evt_buf;

	if (!ev) {
		ath12k_err(ab, "%s: failed by NULL param\n",
			   __func__);
		return -EINVAL;
	}

	cap->phy_capability = le32_to_cpu(ev->phy_capability);
	cap->max_frag_entry = le32_to_cpu(ev->max_frag_entry);
	cap->num_rf_chains = le32_to_cpu(ev->num_rf_chains);
	cap->ht_cap_info = le32_to_cpu(ev->ht_cap_info);
	cap->vht_cap_info = le32_to_cpu(ev->vht_cap_info);
	cap->vht_supp_mcs = le32_to_cpu(ev->vht_supp_mcs);
	cap->hw_min_tx_power = le32_to_cpu(ev->hw_min_tx_power);
	cap->hw_max_tx_power = le32_to_cpu(ev->hw_max_tx_power);
	cap->sys_cap_info = le32_to_cpu(ev->sys_cap_info);
	cap->min_pkt_size_enable = le32_to_cpu(ev->min_pkt_size_enable);
	cap->max_bcn_ie_size = le32_to_cpu(ev->max_bcn_ie_size);
	cap->max_num_scan_channels = le32_to_cpu(ev->max_num_scan_channels);
	cap->max_supported_macs = le32_to_cpu(ev->max_supported_macs);
	cap->wmi_fw_sub_feat_caps = le32_to_cpu(ev->wmi_fw_sub_feat_caps);
	cap->txrx_chainmask = le32_to_cpu(ev->txrx_chainmask);
	cap->default_dbs_hw_mode_index = le32_to_cpu(ev->default_dbs_hw_mode_index);
	cap->num_msdu_desc = le32_to_cpu(ev->num_msdu_desc);

	return 0;
}

/* Save the wmi_service_bitmap into a linear bitmap. The wmi_services in
 * wmi_service ready event are advertised in b0-b3 (LSB 4-bits) of each
 * 4-byte word.
 */
static void ath12k_wmi_service_bitmap_copy(struct ath12k_wmi_pdev *wmi,
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

static int ath12k_wmi_svc_rdy_parse(struct ath12k_base *ab, u16 tag, u16 len,
				    const void *ptr, void *data)
{
	struct ath12k_wmi_svc_ready_parse *svc_ready = data;
	struct ath12k_wmi_pdev *wmi_handle = &ab->wmi_ab.wmi[0];
	u16 expect_len;

	switch (tag) {
	case WMI_TAG_SERVICE_READY_EVENT:
		if (ath12k_pull_service_ready_tlv(ab, ptr, &ab->target_caps))
			return -EINVAL;
		break;

	case WMI_TAG_ARRAY_UINT32:
		if (!svc_ready->wmi_svc_bitmap_done) {
			expect_len = WMI_SERVICE_BM_SIZE * sizeof(u32);
			if (len < expect_len) {
				ath12k_warn(ab, "invalid len %d for the tag 0x%x\n",
					    len, tag);
				return -EINVAL;
			}

			ath12k_wmi_service_bitmap_copy(wmi_handle, ptr);

			svc_ready->wmi_svc_bitmap_done = true;
		}
		break;
	default:
		break;
	}

	return 0;
}

static int ath12k_service_ready_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k_wmi_svc_ready_parse svc_ready = { };
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_svc_rdy_parse,
				  &svc_ready);
	if (ret) {
		ath12k_warn(ab, "failed to parse tlv %d\n", ret);
		return ret;
	}

	return 0;
}

static u32 ath12k_wmi_mgmt_get_freq(struct ath12k *ar,
				    struct ieee80211_tx_info *info)
{
	struct ath12k_base *ab = ar->ab;
	u32 freq = 0;

	if (ab->hw_params->single_pdev_only &&
	    ar->scan.is_roc &&
	    (info->flags & IEEE80211_TX_CTL_TX_OFFCHAN))
		freq = ar->scan.roc_freq;

	return freq;
}

struct sk_buff *ath12k_wmi_alloc_skb(struct ath12k_wmi_base *wmi_ab, u32 len)
{
	struct sk_buff *skb;
	struct ath12k_base *ab = wmi_ab->ab;
	u32 round_len = roundup(len, 4);

	skb = ath12k_htc_alloc_skb(ab, WMI_SKB_HEADROOM + round_len);
	if (!skb)
		return NULL;

	skb_reserve(skb, WMI_SKB_HEADROOM);
	if (!IS_ALIGNED((unsigned long)skb->data, 4))
		ath12k_warn(ab, "unaligned WMI skb data\n");

	skb_put(skb, round_len);
	memset(skb->data, 0, round_len);

	return skb;
}

int ath12k_wmi_mgmt_send(struct ath12k *ar, u32 vdev_id, u32 buf_id,
			 struct sk_buff *frame, bool link_agnostic,
			 bool tx_params_valid)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_mgmt_send_cmd *cmd;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(frame);
	struct wmi_mlo_mgmt_send_params *ml_params;
	struct wmi_mgmt_send_params *params;
	struct wmi_tlv *frame_tlv;
	struct sk_buff *skb;
	u32 buf_len;
	int ret, len;
	void *ptr;
	struct wmi_tlv *tlv;

	buf_len = min_t(int, frame->len, WMI_MGMT_SEND_DOWNLD_LEN);

	len = sizeof(*cmd) + sizeof(*frame_tlv) + roundup(buf_len, sizeof(u32));

	if (link_agnostic)
		len += sizeof(struct wmi_mgmt_send_params) +
				TLV_HDR_SIZE + sizeof(*ml_params);

	if (tx_params_valid)
		len += sizeof(*params);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_mgmt_send_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MGMT_TX_SEND_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->desc_id = cpu_to_le32(buf_id);
	cmd->chanfreq = cpu_to_le32(ath12k_wmi_mgmt_get_freq(ar, info));
	cmd->paddr_lo = cpu_to_le32(lower_32_bits(ATH12K_SKB_CB(frame)->paddr));
	cmd->paddr_hi = cpu_to_le32(upper_32_bits(ATH12K_SKB_CB(frame)->paddr));
	cmd->frame_len = cpu_to_le32(frame->len);
	cmd->buf_len = cpu_to_le32(buf_len);
	cmd->tx_params_valid = tx_params_valid;

	frame_tlv = (struct wmi_tlv *)(skb->data + sizeof(*cmd));
	frame_tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, roundup(buf_len, sizeof(u32)));

	memcpy(frame_tlv->value, frame->data, buf_len);

	if (!link_agnostic)
		goto send;

	ptr = skb->data + sizeof(*cmd) + sizeof(*frame_tlv) + roundup(buf_len, sizeof(u32));

	tlv = ptr;

	/* Tx params not used currently */
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_TX_SEND_PARAMS) |
		      FIELD_PREP(WMI_TLV_LEN, sizeof(struct wmi_mgmt_send_params) - TLV_HDR_SIZE);
	ptr += sizeof(struct wmi_mgmt_send_params);

	tlv = ptr;
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
		      FIELD_PREP(WMI_TLV_LEN, sizeof(*ml_params));
	ptr += TLV_HDR_SIZE;

	ml_params = ptr;
	ml_params->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_MLO_TX_SEND_PARAMS) |
				FIELD_PREP(WMI_TLV_LEN, sizeof(*ml_params) - TLV_HDR_SIZE);

	if (ath12k_hw_group_recovery_in_progress(ar->ab->ag) &&
	    ar->ab->ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE2) {
		ml_params->hw_link_id = ar->pdev->hw_link_id;
	} else {
		ml_params->hw_link_id = WMI_MLO_MGMT_TID;
	}

	if (tx_params_valid) {
		params = (struct wmi_mgmt_send_params *)(skb->data + (len - sizeof(*params)));
		params->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_TX_SEND_PARAMS) |
				     FIELD_PREP(WMI_TLV_LEN, sizeof(*params) - TLV_HDR_SIZE);
		params->tx_param_dword1 |= WMI_TX_PARAMS_DWORD1_CFR_CAPTURE;
	}

send:
	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_MGMT_TX_SEND_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_MGMT_TX_SEND_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_stats_request_cmd(struct ath12k *ar, u32 stats_id,
				      u32 vdev_id, u32 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_request_stats_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_request_stats_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_REQUEST_STATS_CMD,
						 sizeof(*cmd));

	cmd->stats_id = cpu_to_le32(stats_id);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->pdev_id = cpu_to_le32(pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_REQUEST_STATS_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_REQUEST_STATS cmd\n");
		dev_kfree_skb(skb);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI request stats 0x%x vdev id %d pdev id %d\n",
		   stats_id, vdev_id, pdev_id);

	return ret;
}

/* For Big Endian Host, Copy Engine byte_swap is enabled
 * When Copy Engine does byte_swap, need to byte swap again for the
 * Host to get/put buffer content in the correct byte order
 */
void ath12k_ce_byte_swap(void *mem, u32 len)
{
	int i;

	if (IS_ENABLED(CONFIG_CPU_BIG_ENDIAN)) {
		if (!mem)
			return;
		for (i = 0; i < (len / 4); i++) {
			*(u32 *)mem = swab32(*(u32 *)mem);
			mem += 4;
		}
	}
}

/* Send off-channel managemnt frame to firmware. when driver receive a
 * packet with off channel tx flag enabled. This API will send the
 * packet to firmware with WMI command WMI_TAG_OFFCHAN_DATA_TX_SEND_CMD
 * for off-chan tx.
 */
int ath12k_wmi_offchan_mgmt_send(struct ath12k *ar, u32 vdev_id, u32 buf_id,
				 struct sk_buff *frame)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(frame);
	struct wmi_mgmt_send_cmd *cmd;
	struct wmi_tlv *frame_tlv;
	struct sk_buff *skb;
	u32 buf_len, buf_len_padded;
	int ret, len;
	void *ptr;
	struct wmi_tlv *tlv;

	buf_len = min(frame->len, WMI_MGMT_SEND_DOWNLD_LEN);
	buf_len_padded = roundup(buf_len, sizeof(u32));

	len = sizeof(*cmd) + sizeof(*frame_tlv) + buf_len_padded +
	      sizeof(struct wmi_mgmt_send_params);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;
	cmd = ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_OFFCHAN_DATA_TX_SEND_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->desc_id = cpu_to_le32(buf_id);
	cmd->chanfreq = cpu_to_le32(ath12k_wmi_mgmt_get_freq(ar, info));
	cmd->paddr_lo = cpu_to_le32(lower_32_bits(ATH12K_SKB_CB(frame)->paddr));
	cmd->paddr_hi = cpu_to_le32(upper_32_bits(ATH12K_SKB_CB(frame)->paddr));
	cmd->frame_len = cpu_to_le32(frame->len);
	cmd->buf_len = cpu_to_le32(buf_len);
	cmd->tx_params_valid = 1;
	ptr += sizeof(*cmd);

	frame_tlv = ptr;
	frame_tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, buf_len_padded);
	ptr += sizeof(*frame_tlv);

	memcpy(ptr, frame->data, buf_len);
	ath12k_ce_byte_swap(ptr, buf_len);
	ptr += buf_len_padded;

	tlv = ptr;
	/* Tx params not used currently */
	tlv->header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_TX_SEND_PARAMS,
					     sizeof(struct wmi_mgmt_send_params));

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_OFFCHAN_DATA_TX_SEND_CMDID);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_OFFCHAN_DATA_TX_SEND_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_create(struct ath12k *ar, u8 *macaddr,
			   struct ath12k_wmi_vdev_create_arg *args)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_create_cmd *cmd;
	struct sk_buff *skb;
	struct ath12k_wmi_vdev_txrx_streams_params *txrx_streams;
	bool is_ml_vdev = is_valid_ether_addr(args->mld_addr);
	struct wmi_vdev_create_mlo_params *ml_params;
	struct wmi_tlv *tlv;
	int ret, len;
	void *ptr;

	/* It can be optimized my sending tx/rx chain configuration
	 * only for supported bands instead of always sending it for
	 * both the bands.
	 */
	len = sizeof(*cmd) + TLV_HDR_SIZE +
		(WMI_NUM_SUPPORTED_BAND_MAX * sizeof(*txrx_streams)) +
		(is_ml_vdev ? TLV_HDR_SIZE + sizeof(*ml_params) : 0);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_create_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_CREATE_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(args->if_id);
	cmd->vdev_type = cpu_to_le32(args->type);
	cmd->vdev_subtype = cpu_to_le32(args->subtype);
	cmd->num_cfg_txrx_streams = cpu_to_le32(WMI_NUM_SUPPORTED_BAND_MAX);
	cmd->pdev_id = cpu_to_le32(args->pdev_id);
	cmd->mbssid_flags = cpu_to_le32(args->mbssid_flags);
	cmd->mbssid_tx_vdev_id = cpu_to_le32(args->mbssid_tx_vdev_id);
	cmd->vdev_stats_id = cpu_to_le32(args->if_stats_id);
	ether_addr_copy(cmd->vdev_macaddr.addr, macaddr);

	if (args->if_stats_id != ATH12K_INVAL_VDEV_STATS_ID)
		cmd->vdev_stats_id_valid = cpu_to_le32(BIT(0));

	ptr = skb->data + sizeof(*cmd);
	len = WMI_NUM_SUPPORTED_BAND_MAX * sizeof(*txrx_streams);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);

	ptr += TLV_HDR_SIZE;
	txrx_streams = ptr;
	len = sizeof(*txrx_streams);
	txrx_streams->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_TXRX_STREAMS,
							  len);
	txrx_streams->band = cpu_to_le32(WMI_TPC_CHAINMASK_CONFIG_BAND_2G);
	txrx_streams->supported_tx_streams =
				cpu_to_le32(args->chains[NL80211_BAND_2GHZ].tx);
	txrx_streams->supported_rx_streams =
				cpu_to_le32(args->chains[NL80211_BAND_2GHZ].rx);

	txrx_streams++;
	txrx_streams->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_TXRX_STREAMS,
							  len);
	txrx_streams->band = cpu_to_le32(WMI_TPC_CHAINMASK_CONFIG_BAND_5G);
	txrx_streams->supported_tx_streams =
				cpu_to_le32(args->chains[NL80211_BAND_5GHZ].tx);
	txrx_streams->supported_rx_streams =
				cpu_to_le32(args->chains[NL80211_BAND_5GHZ].rx);

	ptr += WMI_NUM_SUPPORTED_BAND_MAX * sizeof(*txrx_streams);

	if (is_ml_vdev) {
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 sizeof(*ml_params));
		ptr += TLV_HDR_SIZE;
		ml_params = ptr;

		ml_params->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_VDEV_CREATE_PARAMS,
					       sizeof(*ml_params));
		ether_addr_copy(ml_params->mld_macaddr.addr, args->mld_addr);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev create: id %d type %d subtype %d macaddr %pM pdevid %d vdev bitmap (allocate:0x%llx free:0x%llx)\n",
		   args->if_id, args->type, args->subtype, macaddr, args->pdev_id,
		   ar->allocated_vdev_map, ar->ab->free_vdev_map);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_CREATE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_VDEV_CREATE_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_delete(struct ath12k *ar, u8 vdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_delete_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_delete_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_DELETE_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev delete id %d num_peers : %d vdev bitmap (allocate:0x%llx free:0x%llx)\n",
		   vdev_id, ar->num_peers, ar->allocated_vdev_map,
		   ar->ab->free_vdev_map);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_DELETE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit WMI_VDEV_DELETE_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_stop(struct ath12k *ar, u8 vdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_stop_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_stop_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_STOP_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "WMI vdev stop id 0x%x\n", vdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_STOP_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit WMI_VDEV_STOP cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_down(struct ath12k *ar, u8 vdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_down_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_down_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_DOWN_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "WMI vdev down id 0x%x\n", vdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_DOWN_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit WMI_VDEV_DOWN cmd vdev id : %d\n",
			    vdev_id);
		dev_kfree_skb(skb);
	}

	return ret;
}

static inline bool
ath12k_wmi_check_device_present(u32 width_device,
				u32 center_freq_device,
				u32 center_freq_oper)
{
	return (center_freq_device && width_device &&
		center_freq_device != center_freq_oper);
}

static void ath12k_wmi_set_wmi_channel_device(struct ath12k_wmi_channel_params *chan_device,
					      struct wmi_vdev_start_req_arg *channel,
					      u32 cf_device, u32 width_device)
{
	enum wmi_phy_mode mode_device;

	memset(chan_device, 0, sizeof(*chan_device));

	chan_device->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_CHANNEL,
							 sizeof(*chan_device));
	chan_device->mhz = cpu_to_le32(channel->freq);
	chan_device->band_center_freq1 = cpu_to_le32(cf_device);

	if (width_device == NL80211_CHAN_WIDTH_320) {
		mode_device = MODE_11BE_EHT320;
		if (channel->freq > chan_device->band_center_freq1)
			chan_device->band_center_freq1 = cf_device + 80;
		else
			chan_device->band_center_freq1 = cf_device - 80;
		chan_device->band_center_freq2 = cf_device;
	} else if (width_device == NL80211_CHAN_WIDTH_160) {
		mode_device = MODE_11BE_EHT160;
		if (channel->freq > chan_device->band_center_freq1)
			chan_device->band_center_freq1 = cf_device + 40;
		else
			chan_device->band_center_freq1 = cf_device - 40;
		chan_device->band_center_freq2 = cf_device;
	} else if (width_device == NL80211_CHAN_WIDTH_80) {
		mode_device = MODE_11BE_EHT80;
	} else if (width_device == NL80211_CHAN_WIDTH_40) {
		mode_device = MODE_11BE_EHT40;
	} else {
		mode_device = MODE_UNKNOWN;
	}

	chan_device->info |= le32_encode_bits(mode_device, WMI_CHAN_INFO_MODE);
	if (channel->passive)
		chan_device->info |= cpu_to_le32(WMI_CHAN_INFO_PASSIVE);
	if (channel->allow_ibss)
		chan_device->info |= cpu_to_le32(WMI_CHAN_INFO_ADHOC_ALLOWED);
	if (channel->allow_ht)
		chan_device->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_HT);
	if (channel->allow_vht)
		chan_device->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_VHT);
	if (channel->allow_he)
		chan_device->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_HE);
	if (channel->ht40plus)
		chan_device->info |= cpu_to_le32(WMI_CHAN_INFO_HT40_PLUS);
	if (channel->chan_radar)
		chan_device->info |= cpu_to_le32(WMI_CHAN_INFO_DFS);
	if (channel->freq2_radar)
		chan_device->info |= cpu_to_le32(WMI_CHAN_INFO_DFS_FREQ2);

	chan_device->reg_info_1 = le32_encode_bits(channel->max_power,
						   WMI_CHAN_REG_INFO1_MAX_PWR) |
				  le32_encode_bits(channel->max_reg_power,
						   WMI_CHAN_REG_INFO1_MAX_REG_PWR);

	chan_device->reg_info_2 = le32_encode_bits(channel->max_antenna_gain,
						   WMI_CHAN_REG_INFO2_ANT_MAX) |
				  le32_encode_bits(channel->max_power,
						   WMI_CHAN_REG_INFO2_MAX_TX_PWR);
}

static void ath12k_wmi_put_wmi_channel(struct ath12k_wmi_channel_params *chan,
				       struct wmi_vdev_start_req_arg *arg)
{
	u32 center_freq1 = arg->band_center_freq1;

	memset(chan, 0, sizeof(*chan));

	chan->mhz = cpu_to_le32(arg->freq);
	chan->band_center_freq1 = cpu_to_le32(arg->band_center_freq1);
	if (arg->mode == MODE_11BE_EHT320) {
		if (arg->freq > center_freq1)
			chan->band_center_freq1 =
					cpu_to_le32(center_freq1 + 80);
		else
			chan->band_center_freq1 =
					cpu_to_le32(center_freq1 - 80);

		chan->band_center_freq2 = cpu_to_le32(arg->band_center_freq1);
	} else if (arg->mode == MODE_11BE_EHT160) {
		if (arg->freq > center_freq1)
			chan->band_center_freq1 =
					cpu_to_le32(center_freq1 + 40);
		else
			chan->band_center_freq1 =
					cpu_to_le32(center_freq1 - 40);

		chan->band_center_freq2 = cpu_to_le32(arg->band_center_freq1);
	} else {
		chan->band_center_freq2 = 0;
	}

	chan->info |= le32_encode_bits(arg->mode, WMI_CHAN_INFO_MODE);
	if (arg->passive)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_PASSIVE);
	if (arg->allow_ibss)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_ADHOC_ALLOWED);
	if (arg->allow_ht)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_HT);
	if (arg->allow_vht)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_VHT);
	if (arg->allow_he)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_HE);
	if (arg->ht40plus)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_HT40_PLUS);
	if (arg->chan_radar)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_DFS);
	if (arg->freq2_radar)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_DFS_FREQ2);

	chan->reg_info_1 = le32_encode_bits(arg->max_power,
					    WMI_CHAN_REG_INFO1_MAX_PWR) |
		le32_encode_bits(arg->max_reg_power,
				 WMI_CHAN_REG_INFO1_MAX_REG_PWR);

	chan->reg_info_2 = le32_encode_bits(arg->max_antenna_gain,
					    WMI_CHAN_REG_INFO2_ANT_MAX) |
		le32_encode_bits(arg->max_power, WMI_CHAN_REG_INFO2_MAX_TX_PWR);
}

int ath12k_wmi_vdev_start(struct ath12k *ar, struct wmi_vdev_start_req_arg *arg,
			  bool restart)
{
	struct ath12k_wmi_channel_params *chan_device;
	struct wmi_vdev_start_mlo_params *ml_params;
	struct wmi_partner_link_info *partner_info;
	struct ath12k_hw_group *ag = ar->ab->ag;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_start_request_cmd *cmd;
	bool device_params_present = false;
	struct sk_buff *skb;
	struct ath12k_wmi_channel_params *chan;
	struct wmi_tlv *tlv;
	void *ptr;
	int ret, len, i, ml_arg_size = 0;

	if (WARN_ON(arg->ssid_len > sizeof(cmd->ssid.ssid)))
		return -EINVAL;

	len = sizeof(*cmd) + sizeof(*chan) + TLV_HDR_SIZE;

	if (!restart && arg->ml.enabled) {
		ml_arg_size = TLV_HDR_SIZE + sizeof(*ml_params) +
			      TLV_HDR_SIZE + (arg->ml.num_partner_links *
					      sizeof(*partner_info));
		len += ml_arg_size;
	}
	device_params_present = ath12k_wmi_check_device_present(arg->width_device,
								arg->center_freq_device,
								arg->band_center_freq1);
	if (device_params_present)
		len += TLV_HDR_SIZE + sizeof(*chan_device);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_start_request_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_START_REQUEST_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->beacon_interval = cpu_to_le32(arg->bcn_intval);
	cmd->bcn_tx_rate = cpu_to_le32(arg->bcn_tx_rate);
	cmd->dtim_period = cpu_to_le32(arg->dtim_period);
	cmd->num_noa_descriptors = cpu_to_le32(arg->num_noa_descriptors);
	cmd->preferred_rx_streams = cpu_to_le32(arg->pref_rx_streams);
	cmd->preferred_tx_streams = cpu_to_le32(arg->pref_tx_streams);
	cmd->cac_duration_ms = cpu_to_le32(arg->cac_duration_ms);
	cmd->regdomain = cpu_to_le32(arg->regdomain);
	cmd->he_ops = cpu_to_le32(arg->he_ops);
	cmd->punct_bitmap = cpu_to_le32(arg->punct_bitmap);
	cmd->mbssid_flags = cpu_to_le32(arg->mbssid_flags);
	cmd->mbssid_tx_vdev_id = cpu_to_le32(arg->mbssid_tx_vdev_id);

	if (!restart) {
		if (arg->ssid) {
			cmd->ssid.ssid_len = cpu_to_le32(arg->ssid_len);
			memcpy(cmd->ssid.ssid, arg->ssid, arg->ssid_len);
		}
		if (arg->hidden_ssid)
			cmd->flags |= cpu_to_le32(WMI_VDEV_START_HIDDEN_SSID);
		if (arg->pmf_enabled)
			cmd->flags |= cpu_to_le32(WMI_VDEV_START_PMF_ENABLED);
	}

	cmd->flags |= cpu_to_le32(WMI_VDEV_START_LDPC_RX_ENABLED);
	if (test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ag->flags))
		cmd->flags |= cpu_to_le32(WMI_VDEV_START_HW_ENCRYPTION_DISABLED);

	ptr = skb->data + sizeof(*cmd);
	chan = ptr;

	ath12k_wmi_put_wmi_channel(chan, arg);

	chan->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_CHANNEL,
						  sizeof(*chan));
	ptr += sizeof(*chan);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);

	/* Note: This is a nested TLV containing:
	 * [wmi_tlv][ath12k_wmi_p2p_noa_descriptor][wmi_tlv]..
	 */

	ptr += sizeof(*tlv);

	if (ml_arg_size) {
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 sizeof(*ml_params));
		ptr += TLV_HDR_SIZE;

		ml_params = ptr;

		ml_params->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_VDEV_START_PARAMS,
					       sizeof(*ml_params));

		ml_params->flags = le32_encode_bits(arg->ml.enabled,
						    ATH12K_WMI_FLAG_MLO_ENABLED) |
				   le32_encode_bits(arg->ml.assoc_link,
						    ATH12K_WMI_FLAG_MLO_ASSOC_LINK) |
				   le32_encode_bits(arg->ml.mcast_link,
						    ATH12K_WMI_FLAG_MLO_MCAST_VDEV) |
				   le32_encode_bits(arg->ml.link_add,
						    ATH12K_WMI_FLAG_MLO_LINK_ADD) |
				   le32_encode_bits(1,
						    ATH12K_WMI_FLAG_MLO_IEEE_LINK_IDX_VALID) |
				   le32_encode_bits(arg->ml.mlo_bridge_link,
						    ATH12K_WMI_FLAG_MLO_BRIDGE_LINK);
		ml_params->ieee_link_id = arg->ml.ieee_link_id;

		ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "vdev %d start ml flags 0x%x ieee_link_id=%d\n",
			   arg->vdev_id, ml_params->flags, ml_params->ieee_link_id);

		ptr += sizeof(*ml_params);

		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 arg->ml.num_partner_links *
						 sizeof(*partner_info));
		ptr += TLV_HDR_SIZE;

		partner_info = ptr;

		for (i = 0; i < arg->ml.num_partner_links; i++) {
			partner_info->tlv_header =
				ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_PARTNER_LINK_PARAMS,
						       sizeof(*partner_info));
			partner_info->vdev_id =
				cpu_to_le32(arg->ml.partner_info[i].vdev_id);
			partner_info->hw_link_id =
				cpu_to_le32(arg->ml.partner_info[i].hw_link_id);
			ether_addr_copy(partner_info->vdev_addr.addr,
					arg->ml.partner_info[i].addr);
			partner_info->ieee_link_id = arg->ml.partner_info[i].logical_link_idx;
			partner_info->flags = le32_encode_bits(1,
							       ATH12K_WMI_FLAG_MLO_IEEE_LINK_IDX_VALID_PARTNER) |
					      le32_encode_bits(arg->ml.partner_info[i].mlo_bridge_link,
							       ATH12K_WMI_FLAG_MLO_BRIDGE_LINK);

			ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "partner vdev %d hw_link_id %d macaddr%pM flags:0x%x\n",
				   partner_info->vdev_id, partner_info->hw_link_id,
				   partner_info->vdev_addr.addr, partner_info->flags);

			partner_info++;
		}

		ptr = partner_info;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "vdev %s id 0x%x freq 0x%x mode 0x%x\n",
		   restart ? "restart" : "start", arg->vdev_id,
		   arg->freq, arg->mode);

	if (test_bit(WMI_TLV_SERVICE_SW_PROG_DFS_SUPPORT, ar->ab->wmi_ab.svc_map) &&
	    device_params_present) {
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 sizeof(*chan_device));
		ptr += TLV_HDR_SIZE;

		chan_device = ptr;
		ath12k_wmi_set_wmi_channel_device(chan_device, arg,
						  arg->center_freq_device,
						  arg->width_device);
		ptr += sizeof(*chan_device);
	}

	if (restart)
		ret = ath12k_wmi_cmd_send(wmi, skb,
					  WMI_VDEV_RESTART_REQUEST_CMDID);
	else
		ret = ath12k_wmi_cmd_send(wmi, skb,
					  WMI_VDEV_START_REQUEST_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit vdev_%s cmd\n",
			    restart ? "restart" : "start");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_up(struct ath12k *ar, struct ath12k_wmi_vdev_up_params *params)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_up_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_up_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_UP_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(params->vdev_id);
	cmd->vdev_assoc_id = cpu_to_le32(params->aid);

	ether_addr_copy(cmd->vdev_bssid.addr, params->bssid);

	if (params->tx_bssid) {
		ether_addr_copy(cmd->tx_vdev_bssid.addr, params->tx_bssid);
		cmd->nontx_profile_idx = cpu_to_le32(params->nontx_profile_idx);
		cmd->nontx_profile_cnt = cpu_to_le32(params->nontx_profile_cnt);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI mgmt vdev up id 0x%x assoc id %d bssid %pM\n",
		   params->vdev_id, params->aid, params->bssid);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_UP_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit WMI_VDEV_UP cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_peer_create_cmd(struct ath12k *ar,
				    struct ath12k_wmi_peer_create_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_create_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;
	struct wmi_peer_create_mlo_params *ml_param;
	void *ptr;
	struct wmi_tlv *tlv;

	len = sizeof(*cmd) + TLV_HDR_SIZE + sizeof(*ml_param);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_create_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_CREATE_CMD,
						 sizeof(*cmd));

	ether_addr_copy(cmd->peer_macaddr.addr, arg->peer_addr);
	cmd->peer_type = cpu_to_le32(arg->peer_type);
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);

	ptr = skb->data + sizeof(*cmd);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 sizeof(*ml_param));
	ptr += TLV_HDR_SIZE;
	ml_param = ptr;
	ml_param->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_PEER_CREATE_PARAMS,
					       sizeof(*ml_param));
	ml_param->flags = le32_encode_bits(arg->ml_enabled,
					   ATH12K_WMI_FLAG_MLO_ENABLED) |
			  le32_encode_bits(arg->mlo_bridge_peer,
					   ATH12K_WMI_FLAG_MLO_BRIDGE_PEER);

	ptr += sizeof(*ml_param);

	ath12k_dbg(ar->ab, ATH12K_DBG_PEER | ATH12K_DBG_MLME,
		   "WMI peer create vdev_id %d peer_addr %pM ml_flags 0x%x num_peer:%d bridge peer %d\n",
		   arg->vdev_id, arg->peer_addr, ml_param->flags, ar->num_peers, arg->mlo_bridge_peer);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_CREATE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit WMI_PEER_CREATE cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_peer_delete_cmd(struct ath12k *ar,
				    const u8 *peer_addr, u8 vdev_id,
				    struct ath12k_sta *ahsta)
{
	struct ath12k_hw_group *ag = ar->ab->ag;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_delete_cmd *cmd;
	struct ath12k_wmi_peer_delete_mlo_params *mlo_params;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;
	u32 mlo_hw_link_id_bitmap = 0;
	int ret, len;

	if (ahsta)
		mlo_hw_link_id_bitmap = ahsta->mlo_hw_link_id_bitmap;

	len = sizeof(*cmd) + sizeof(*mlo_params) + TLV_HDR_SIZE;
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = (struct wmi_peer_delete_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_DELETE_CMD,
						 sizeof(*cmd));

	ether_addr_copy(cmd->peer_macaddr.addr, peer_addr);
	cmd->vdev_id = cpu_to_le32(vdev_id);

	ptr += sizeof(*cmd);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, sizeof(*mlo_params));

	ptr += TLV_HDR_SIZE;
	mlo_params = ptr;
	mlo_params->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_PARAMS_PEER_DELETE,
							sizeof(*mlo_params));

	if (ag && (ath12k_hw_group_recovery_in_progress(ag) ||
	    (ahsta && ahsta->peer_delete_send_mlo_hw_bitmap)) &&
	    mlo_hw_link_id_bitmap)
		mlo_hw_link_id_bitmap &= ~BIT(ar->pdev->hw_link_id);
	else
		mlo_hw_link_id_bitmap = 0;

	mlo_params->mlo_hw_link_id_bitmap = cpu_to_le32(mlo_hw_link_id_bitmap);

	ath12k_dbg(ar->ab, ATH12K_DBG_PEER | ATH12K_DBG_MLME,
		   "WMI peer delete vdev_id %d peer_addr %pM num_peer : %d hw_link_id_bitmap 0x%x\n",
		   vdev_id,  peer_addr, ar->num_peers, mlo_hw_link_id_bitmap);

	if (ahsta && ahsta->peer_delete_send_mlo_hw_bitmap) {
		ath12k_dbg(ar->ab, ATH12K_DBG_PEER,
			   "WMI peer delete peer_delete_send_mlo_hw_bitmap: 0x%x\n",
			   ahsta->peer_delete_send_mlo_hw_bitmap);
	}

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_DELETE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PEER_DELETE cmd"
			   " peer_addr %pM num_peer : %d\n",
			    peer_addr, ar->num_peers);
		dev_kfree_skb(skb);
	}

	return ret;
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
int ath12k_wmi_config_peer_ppeds_routing(struct ath12k *ar,
					 const u8 *peer_addr, u8 vdev_id,
					 u32 service_code, u32 priority_valid,
					 u32 src_info, bool ppe_routing_enable,
					 bool use_ppe)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	struct wmi_peer_config_ppeds_cmd *cmd;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_config_ppeds_cmd *)skb->data;
	cmd->tlv_header  = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_CONFIG_PPEDS_ROUTING,
						  sizeof(*cmd));

	ether_addr_copy(cmd->peer_macaddr.addr, peer_addr);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	if (ppe_routing_enable) {
		if (use_ppe)
			cmd->ppe_routing_enable = WMI_AST_USE_PPE_ENABLED;
		else
			cmd->ppe_routing_enable = WMI_AST_USE_PPE_DISABLED;
	} else {
		cmd->ppe_routing_enable = WMI_PPE_ROUTING_DISABLED;
	}
	cmd->service_code = cpu_to_le32(service_code);
	cmd->priority_valid = cpu_to_le32(priority_valid);
	cmd->src_info = cpu_to_le32(src_info);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_CONFIG_PPE_DS_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PEER_CONFIG_PPE_DS cmd" \
			   "peer_addr %pM num_peer : %d\n",
			    peer_addr, ar->num_peers);
		dev_kfree_skb(skb);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "ppe ds routing cmd peer_addr %pM vdev_id %d service_code %d " \
		   "priority_valid %d src_info %d ppe_routing_enable %d\n",
		   peer_addr, vdev_id, service_code, priority_valid,
		   src_info, ppe_routing_enable);

	return ret;
}
#endif

int ath12k_wmi_send_pdev_pkt_route(struct ath12k *ar,
				   struct ath12k_wmi_pkt_route_param *param)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_pkt_route_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
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

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "CCE PPE WMI pdev pkt route opcode %d route_bmap %d dst_ring %d meta_data %d" \
		   "dst_ring_handler %d\n", param->opcode, param->route_type_bmap,
		   param->dst_ring, param->meta_data, param->dst_ring_handler);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_UPDATE_PKT_ROUTING_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PDEV_UPDATE_PKT_ROUTING cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_pdev_set_regdomain(struct ath12k *ar,
				       struct ath12k_wmi_pdev_set_regdomain_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_set_regdomain_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_regdomain_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_REGDOMAIN_CMD,
						 sizeof(*cmd));

	cmd->reg_domain = cpu_to_le32(arg->current_rd_in_use);
	cmd->reg_domain_2g = cpu_to_le32(arg->current_rd_2g);
	cmd->reg_domain_5g = cpu_to_le32(arg->current_rd_5g);
	cmd->conformance_test_limit_2g = cpu_to_le32(arg->ctl_2g);
	cmd->conformance_test_limit_5g = cpu_to_le32(arg->ctl_5g);
	cmd->dfs_domain = cpu_to_le32(arg->dfs_domain);
	cmd->pdev_id = cpu_to_le32(arg->pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI pdev regd rd %d rd2g %d rd5g %d domain %d pdev id %d\n",
		   arg->current_rd_in_use, arg->current_rd_2g,
		   arg->current_rd_5g, arg->dfs_domain, arg->pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_SET_REGDOMAIN_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PDEV_SET_REGDOMAIN cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_set_peer_param(struct ath12k *ar, const u8 *peer_addr,
			      u32 vdev_id, u32 param_id, u32 param_val)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_set_param_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_set_param_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_SET_PARAM_CMD,
						 sizeof(*cmd));
	ether_addr_copy(cmd->peer_macaddr.addr, peer_addr);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->param_id = cpu_to_le32(param_id);
	cmd->param_value = cpu_to_le32(param_val);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev %d peer 0x%pM set param %d value %d\n",
		   vdev_id, peer_addr, param_id, param_val);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_SET_PARAM_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PEER_SET_PARAM cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_peer_flush_tids_cmd(struct ath12k *ar,
					u8 peer_addr[ETH_ALEN],
					u32 peer_tid_bitmap,
					u8 vdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_flush_tids_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_flush_tids_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_FLUSH_TIDS_CMD,
						 sizeof(*cmd));

	ether_addr_copy(cmd->peer_macaddr.addr, peer_addr);
	cmd->peer_tid_bitmap = cpu_to_le32(peer_tid_bitmap);
	cmd->vdev_id = cpu_to_le32(vdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI peer flush vdev_id %d peer_addr %pM tids %08x\n",
		   vdev_id, peer_addr, peer_tid_bitmap);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_FLUSH_TIDS_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PEER_FLUSH_TIDS cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_peer_rx_reorder_queue_setup(struct ath12k *ar,
					   int vdev_id, const u8 *addr,
					   dma_addr_t paddr, u8 tid,
					   u8 ba_window_size_valid,
					   u32 ba_window_size)
{
	struct wmi_peer_reorder_queue_setup_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_reorder_queue_setup_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_REORDER_QUEUE_SETUP_CMD,
						 sizeof(*cmd));

	ether_addr_copy(cmd->peer_macaddr.addr, addr);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->tid = cpu_to_le32(tid);
	cmd->queue_ptr_lo = cpu_to_le32(lower_32_bits(paddr));
	cmd->queue_ptr_hi = cpu_to_le32(upper_32_bits(paddr));
	cmd->queue_no = cpu_to_le32(tid);
	cmd->ba_window_size_valid = cpu_to_le32(ba_window_size_valid);
	cmd->ba_window_size = cpu_to_le32(ba_window_size);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi rx reorder queue setup addr %pM vdev_id %d tid %d\n",
		   addr, vdev_id, tid);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb,
				  WMI_PEER_REORDER_QUEUE_SETUP_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PEER_REORDER_QUEUE_SETUP\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath12k_wmi_rx_reord_queue_remove(struct ath12k *ar,
				 struct ath12k_wmi_rx_reorder_queue_remove_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_reorder_queue_remove_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_reorder_queue_remove_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_REORDER_QUEUE_REMOVE_CMD,
						 sizeof(*cmd));

	ether_addr_copy(cmd->peer_macaddr.addr, arg->peer_macaddr);
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->tid_mask = cpu_to_le32(arg->peer_tid_bitmap);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "%s: peer_macaddr %pM vdev_id %d, tid_map %d", __func__,
		   arg->peer_macaddr, arg->vdev_id, arg->peer_tid_bitmap);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PEER_REORDER_QUEUE_REMOVE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PEER_REORDER_QUEUE_REMOVE_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_pdev_set_param(struct ath12k *ar, u32 param_id,
			      u32 param_value, u8 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_set_param_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	if (ar->ab->is_bypassed) {
		ath12k_warn(ar->ab, "Chip is bypassed, skip Pdev set cmd");
		return 0;
	}

	cmd = (struct wmi_pdev_set_param_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_PARAM_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(pdev_id);
	cmd->param_id = cpu_to_le32(param_id);
	cmd->param_value = cpu_to_le32(param_value);

	ath12k_dbg_level(ar->ab, ATH12K_DBG_WMI, ATH12K_DBG_L2,
			 "WMI pdev set param %d pdev id %d value %d\n",
			 param_id, pdev_id, param_value);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_SET_PARAM_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PDEV_SET_PARAM cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_pdev_set_ps_mode(struct ath12k *ar, int vdev_id, u32 enable)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_set_ps_mode_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_ps_mode_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_STA_POWERSAVE_MODE_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->sta_ps_mode = cpu_to_le32(enable);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev set psmode %d vdev id %d\n",
		   enable, vdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_STA_POWERSAVE_MODE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PDEV_SET_PARAM cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_pdev_set_timer_for_mec(struct ath12k *ar, int vdev_id, u32 mec_timer)
{
	struct wmi_pdev_set_mec_timer_cmd *cmd;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_mec_timer_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_MEC_AGEING_TIMER_PARAMS,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->mec_aging_timer_threshold = cpu_to_le32(mec_timer);
	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_MEC_AGING_TIMER_CONFIG_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set mec timer\n");
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_pdev_suspend(struct ath12k *ar, u32 suspend_opt,
			    u32 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_suspend_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_suspend_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SUSPEND_CMD,
						 sizeof(*cmd));

	cmd->suspend_opt = cpu_to_le32(suspend_opt);
	cmd->pdev_id = cpu_to_le32(pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI pdev suspend pdev_id %d\n", pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_SUSPEND_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PDEV_SUSPEND cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_pdev_resume(struct ath12k *ar, u32 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_resume_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_resume_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_RESUME_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI pdev resume pdev id %d\n", pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_RESUME_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PDEV_RESUME cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

/* TODO FW Support for the cmd is not available yet.
 * Can be tested once the command and corresponding
 * event is implemented in FW
 */
int ath12k_wmi_pdev_bss_chan_info_request(struct ath12k *ar,
					  enum wmi_bss_chan_info_req_type type)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_bss_chan_info_req_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_bss_chan_info_req_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_BSS_CHAN_INFO_REQUEST,
						 sizeof(*cmd));
	cmd->req_type = cpu_to_le32(type);
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI bss chan info req type %d\n", type);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_BSS_CHAN_INFO_REQUEST_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PDEV_BSS_CHAN_INFO_REQUEST cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_set_ap_ps_param_cmd(struct ath12k *ar, u8 *peer_addr,
					struct ath12k_wmi_ap_ps_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_ap_ps_peer_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_ap_ps_peer_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_AP_PS_PEER_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, peer_addr);
	cmd->param = cpu_to_le32(arg->param);
	cmd->value = cpu_to_le32(arg->value);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI set ap ps vdev id %d peer %pM param %d value %d\n",
		   arg->vdev_id, peer_addr, arg->param, arg->value);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_AP_PS_PEER_PARAM_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_AP_PS_PEER_PARAM_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_set_sta_ps_param(struct ath12k *ar, u32 vdev_id,
				u32 param, u32 param_value)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_sta_powersave_param_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_sta_powersave_param_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_STA_POWERSAVE_PARAM_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->param = cpu_to_le32(param);
	cmd->value = cpu_to_le32(param_value);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI set sta ps vdev_id %d param %d value %d\n",
		   vdev_id, param, param_value);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_STA_POWERSAVE_PARAM_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_STA_POWERSAVE_PARAM_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_force_fw_hang_cmd(struct ath12k *ar, u32 type, u32 delay_time_ms, bool nowait)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_force_fw_hang_cmd *cmd;
	struct sk_buff *skb;
	int ret = 0, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_force_fw_hang_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_FORCE_FW_HANG_CMD,
						 len);

	cmd->type = cpu_to_le32(type);
	cmd->delay_time_ms = cpu_to_le32(delay_time_ms);
	if (nowait)
		ath12k_wmi_cmd_send_nowait(wmi, skb, WMI_FORCE_FW_HANG_CMDID);
	else
		ret = ath12k_wmi_cmd_send(wmi, skb, WMI_FORCE_FW_HANG_CMDID);

	if (ret) {
		ath12k_warn(ar->ab, "Failed to send WMI_FORCE_FW_HANG_CMDID");
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_vdev_set_param_cmd(struct ath12k *ar, u32 vdev_id,
				  u32 param_id, u32 param_value)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_set_param_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_set_param_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_SET_PARAM_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->param_id = cpu_to_le32(param_id);
	cmd->param_value = cpu_to_le32(param_value);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev id 0x%x set param %d value %d\n",
		   vdev_id, param_id, param_value);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_SET_PARAM_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_VDEV_SET_PARAM_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static void ath12k_wmi_copy_coex_config(struct ath12k *ar, struct wmi_coex_config_cmd *cmd,
                                       struct coex_config_arg *coex_config)
{
        switch (coex_config->config_type) {
        case WMI_COEX_CONFIG_BTC_ENABLE:
                cmd->coex_enable = coex_config->coex_enable;
                ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
                           "WMI coex config type %u vdev id %d"
                           " coex_enable %u\n",
                           coex_config->config_type,
                           coex_config->vdev_id,
                           coex_config->coex_enable);
                break;
        case WMI_COEX_CONFIG_WLAN_PKT_PRIORITY:
                cmd->wlan_pkt_type = coex_config->wlan_pkt_type;
                cmd->wlan_pkt_weight = coex_config->wlan_pkt_weight;
                cmd->bt_pkt_weight = coex_config->bt_pkt_weight;
                ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
                           "WMI coex config type %u vdev id %d"
                           " wlan pkt type 0x%x wlan pkt weight %u"
                           " bt pkt weight %u\n",
                           coex_config->config_type,
                           coex_config->vdev_id,
                           coex_config->wlan_pkt_type,
                           coex_config->wlan_pkt_weight,
                           coex_config->bt_pkt_weight);
                break;
        case WMI_COEX_CONFIG_PTA_INTERFACE:
                cmd->pta_num = coex_config->pta_num;
                cmd->coex_mode = coex_config->coex_mode;
                cmd->bt_txrx_time = coex_config->bt_txrx_time;
                cmd->bt_priority_time = coex_config->bt_priority_time;
                cmd->pta_algorithm = coex_config->pta_algorithm;
                cmd->pta_priority = coex_config->pta_priority;
                ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
                           "WMI coex config type %u vdev id %d"
                           " pta num %u coex mode 0x%x"
                           " bt_txrx_time 0x%x"
                           " bt_priority_time 0x%x pta alogrithm 0x%x"
                           " pta priority 0x%x\n",
                           coex_config->config_type,
                           coex_config->vdev_id,
                           coex_config->pta_num,
                           coex_config->coex_mode,
                           coex_config->bt_txrx_time,
                           coex_config->bt_priority_time,
                           coex_config->pta_algorithm,
                           coex_config->pta_priority);
                break;
        case WMI_COEX_CONFIG_AP_TDM:
                cmd->duty_cycle = coex_config->duty_cycle;
                cmd->wlan_duration = coex_config->wlan_duration;
                ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
                           "WMI coex config type %u vdev id %d"
                           " duty_cycle %u wlan_duration %u\n",
                           coex_config->config_type,
                           coex_config->vdev_id,
                           coex_config->duty_cycle,
                           coex_config->wlan_duration);
                break;
        case WMI_COEX_CONFIG_FORCED_ALGO:
                cmd->coex_algo = coex_config->coex_algo;
                ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
                           "WMI coex config type %u vdev id %d"
                           " coex_algorithm %u\n",
                           coex_config->config_type,
                           coex_config->vdev_id,
                           coex_config->coex_algo);
		break;
        default:
                break;
        }
}

int ath12k_send_coex_config_cmd(struct ath12k *ar,
                                struct coex_config_arg *coex_config)
{
        struct ath12k_wmi_pdev *wmi = ar->wmi;
        struct wmi_coex_config_cmd *cmd;
        struct sk_buff *skb;
        int ret;

        skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
        if (!skb)
                return -ENOMEM;

        cmd = (struct wmi_coex_config_cmd *)skb->data;
        cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_COEX_CONFIG_CMD) |
                          FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

        cmd->vdev_id = coex_config->vdev_id;
        cmd->config_type = coex_config->config_type;
        ath12k_wmi_copy_coex_config(ar, cmd, coex_config);

        ret = ath12k_wmi_cmd_send(wmi, skb, WMI_COEX_CONFIG_CMDID);
        if (ret) {
                ath12k_warn(ar->ab, "failed to send WMI_COEX_CONFIG_CMD cmd\n");
                dev_kfree_skb(skb);
        }

        return ret;
}

int ath12k_wmi_send_pdev_temperature_cmd(struct ath12k *ar)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_get_pdev_temperature_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_get_pdev_temperature_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_GET_TEMPERATURE_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI pdev get temperature for pdev_id %d\n", ar->pdev->pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_GET_TEMPERATURE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PDEV_GET_TEMPERATURE cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_bcn_offload_control_cmd(struct ath12k *ar,
					    u32 vdev_id, u32 bcn_ctrl_op)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_bcn_offload_ctrl_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_bcn_offload_ctrl_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_BCN_OFFLOAD_CTRL_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->bcn_ctrl_op = cpu_to_le32(bcn_ctrl_op);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI bcn ctrl offload vdev id %d ctrl_op %d\n",
		   vdev_id, bcn_ctrl_op);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_BCN_OFFLOAD_CTRL_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_BCN_OFFLOAD_CTRL_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_p2p_go_bcn_ie(struct ath12k *ar, u32 vdev_id,
			     const u8 *p2p_ie)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_p2p_go_set_beacon_ie_cmd *cmd;
	size_t p2p_ie_len, aligned_len;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int ret, len;

	p2p_ie_len = p2p_ie[1] + 2;
	aligned_len = roundup(p2p_ie_len, sizeof(u32));

	len = sizeof(*cmd) + TLV_HDR_SIZE + aligned_len;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;
	cmd = ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_P2P_GO_SET_BEACON_IE,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->ie_buf_len = cpu_to_le32(p2p_ie_len);

	ptr += sizeof(*cmd);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ARRAY_BYTE,
					     aligned_len);
	memcpy(tlv->value, p2p_ie, p2p_ie_len);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_P2P_GO_SET_BEACON_IE);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_P2P_GO_SET_BEACON_IE\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static void ath12k_wmi_bcn_fill_ml_info(struct ath12k_link_vif *arvif,
					struct wmi_bcn_tmpl_ml_info *ml_info)
{
	struct ieee80211_bss_conf *link_conf, *tx_link_conf;
	struct ath12k_base *ab = arvif->ar->ab;
	struct ath12k_link_vif *arvif_iter;
	u32 vdev_id = arvif->vdev_id;
	DECLARE_BITMAP(vdev_map_cat1, 64);
	DECLARE_BITMAP(vdev_map_cat2, 64);

	bitmap_zero(vdev_map_cat1, 64);
	bitmap_zero(vdev_map_cat2, 64);
	rcu_read_lock();

	tx_link_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!tx_link_conf) {
		rcu_read_unlock();
		goto err_fill_ml_info;
	}

	/* Fill CU flags for non-tx vdevs while setting tx vdev beacon.
	 */
	list_for_each_entry(arvif_iter, &arvif->ar->arvifs, list) {
		if (arvif_iter != arvif && arvif_iter->tx_vdev_id == arvif->vdev_id &&
		    ath12k_mac_is_ml_arvif(arvif_iter)) {
			link_conf = ath12k_mac_get_link_bss_conf(arvif_iter);
			if (!link_conf) {
				rcu_read_unlock();
				goto err_fill_ml_info;
			}
			/* If this is cu cat 1 for tx vdev, then it applies
			 * to non-tx vdev as well.
			 */
			if (link_conf->elemid_added || tx_link_conf->elemid_added)
				set_bit(arvif_iter->vdev_id, vdev_map_cat1);
			/* If arvif is not up, current set beacon will be bringing it up
			 * So for link addition, set critical update even if arvif is
			 * not up.
			 */
			if (link_conf->elemid_modified || !arvif_iter->is_up)
				set_bit(arvif_iter->vdev_id, vdev_map_cat2);
		}
	}

	rcu_read_unlock();

	ml_info->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_BCN_TMPL_ML_INFO_CMD,
						     sizeof(*ml_info));
	ml_info->hw_link_id = cpu_to_le32(arvif->ar->pdev->hw_link_id);

	if (tx_link_conf->elemid_added)
		set_bit(vdev_id, vdev_map_cat1);

	if (tx_link_conf->elemid_modified)
		set_bit(vdev_id, vdev_map_cat2);

err_fill_ml_info:
#if BITS_PER_LONG == 32
	ml_info->cu_vdev_map_cat1_lo = cpu_to_le32(vdev_map_cat1[0]);
	ml_info->cu_vdev_map_cat1_hi = cpu_to_le32(vdev_map_cat1[1]);
	ml_info->cu_vdev_map_cat2_lo = cpu_to_le32(vdev_map_cat2[0]);
	ml_info->cu_vdev_map_cat2_hi = cpu_to_le32(vdev_map_cat2[1]);
#else
	ml_info->cu_vdev_map_cat1_lo =
			   cpu_to_le32(ATH12K_GET_LOWER_32_BITS(vdev_map_cat1[0]));
	ml_info->cu_vdev_map_cat1_hi =
			   cpu_to_le32(ATH12K_GET_UPPER_32_BITS(vdev_map_cat1[0]));
	ml_info->cu_vdev_map_cat2_lo =
			   cpu_to_le32(ATH12K_GET_LOWER_32_BITS(vdev_map_cat2[0]));
	ml_info->cu_vdev_map_cat2_hi =
			   cpu_to_le32(ATH12K_GET_UPPER_32_BITS(vdev_map_cat2[0]));
#endif

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "wmi CU filled ml info cat1_lo=0x%x cat1_hi=0x%x cat2_lo=0x%x cat2_hi=0x%x\n",
		   ml_info->cu_vdev_map_cat1_lo, ml_info->cu_vdev_map_cat1_hi,
		   ml_info->cu_vdev_map_cat2_lo, ml_info->cu_vdev_map_cat2_hi);
}

static void ath12k_wmi_fill_cu_arg(struct ath12k_link_vif *arvif,
				   struct wmi_critical_update_arg *cu_arg)
{
	struct ath12k_prb_resp_tmpl_ml_info *ar_ml_info;
	struct ath12k_base *ab = arvif->ar->ab;
	struct wmi_bcn_tmpl_ml_info *ml_info;
	struct ath12k *ar = arvif->ar;
	int i;

	if (!ath12k_mac_is_ml_arvif(arvif))
		return;

	/* Fill ML params
	 * ML params should be filled for all partner links
	 */
	cu_arg->num_ml_params = 0;
	/* TODO: Fill ML params. Will work without this info too */

	/* Fill ML info
	 * ML info should be filled for impacted link only
	 */
	cu_arg->num_ml_info = 1;
	cu_arg->ml_info = (struct wmi_bcn_tmpl_ml_info *)
			  kzalloc((cu_arg->num_ml_info * sizeof(*ml_info)),
				  GFP_KERNEL);

	if (!cu_arg->ml_info) {
		ath12k_warn(ab, "wmi failed to get memory for ml info");
		cu_arg->num_ml_info = 0;
	} else {
		for (i = 0; i < cu_arg->num_ml_info; i++) {
			ml_info = &cu_arg->ml_info[i];
			ath12k_wmi_bcn_fill_ml_info(arvif, ml_info);
			/* Retain copy of CU vdev bitmap. Which are used to
			 * update cu_vdev_map in 20TU probe response template.
			 */
			if (ar->supports_6ghz) {
				ar_ml_info = &arvif->ml_info;
				ar_ml_info->hw_link_id = ml_info->hw_link_id;
				ar_ml_info->cu_vdev_map_cat1_lo = ml_info->cu_vdev_map_cat1_lo;
				ar_ml_info->cu_vdev_map_cat1_hi = ml_info->cu_vdev_map_cat1_hi;
				ar_ml_info->cu_vdev_map_cat2_lo = ml_info->cu_vdev_map_cat2_lo;
				ar_ml_info->cu_vdev_map_cat2_hi = ml_info->cu_vdev_map_cat2_hi;
			}
		}
	}
}

static void *
ath12k_wmi_append_critical_update_params(struct ath12k *ar, u32 vdev_id,
					 void *ptr,
					 struct wmi_critical_update_arg *cu_arg)
{
	struct wmi_bcn_tmpl_ml_params *ml_params;
	struct wmi_bcn_tmpl_ml_info *ml_info;
	void *start = ptr;
	struct wmi_tlv *tlv;
	size_t ml_params_len = cu_arg->num_ml_params * sizeof(*ml_params);
	size_t ml_info_len = cu_arg->num_ml_info * sizeof(*ml_info);
	int i;

	/* Add ML params */
	tlv = (struct wmi_tlv *)ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, ml_params_len);
	ml_params = (struct wmi_bcn_tmpl_ml_params *)tlv->value;

	for (i = 0; i < cu_arg->num_ml_params; i++)
		memcpy(&ml_params[i], &cu_arg->ml_params[i],
		       sizeof(*ml_params));

	if (cu_arg->num_ml_params)
		kfree(cu_arg->ml_params);

	ptr += TLV_HDR_SIZE + ml_params_len;

	/* Add ML info */
	tlv = (struct wmi_tlv *)ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, ml_info_len);
	ml_info = (struct wmi_bcn_tmpl_ml_info *)tlv->value;

	for (i = 0; i < cu_arg->num_ml_info; i++)
		memcpy(&ml_info[i], &cu_arg->ml_info[i],
		       sizeof(*ml_info));

	if (cu_arg->num_ml_info)
		kfree(cu_arg->ml_info);

	ptr += TLV_HDR_SIZE + ml_info_len;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi %ld bytes of additional data filled for CU\n",
		    (unsigned long)(ptr - start));
	return ptr;
}

int ath12k_wmi_bcn_tmpl(struct ath12k_link_vif *arvif,
			struct ieee80211_mutable_offsets *offs,
			struct sk_buff *bcn,
			struct ath12k_wmi_bcn_tmpl_ema_arg *ema_args)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = ar->ab;
	struct wmi_bcn_tmpl_cmd *cmd;
	struct ath12k_wmi_bcn_prb_info_params *bcn_prb_info;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ieee80211_bss_conf *conf;
	u32 vdev_id = arvif->vdev_id;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	u32 ema_params = 0;
	void *ptr;
	int ret, len;
	size_t aligned_len = roundup(bcn->len, 4);
	struct wmi_critical_update_arg cu_arg = {
						 .num_ml_params = 0,
						 .ml_params = NULL,
						 .num_ml_info = 0,
						 .ml_info = NULL,
						};

	conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!conf) {
		ath12k_warn(ab,
			    "unable to access bss link conf in beacon template command for vif %pM link %u\n",
			    ahvif->vif->addr, arvif->link_id);
		return -EINVAL;
	}

	ath12k_wmi_fill_cu_arg(arvif, &cu_arg);

	len = sizeof(*cmd) + sizeof(*bcn_prb_info) + TLV_HDR_SIZE + aligned_len +
	      TLV_HDR_SIZE + (sizeof(struct wmi_bcn_tmpl_ml_params) * cu_arg.num_ml_params) +
	      TLV_HDR_SIZE + (sizeof(struct wmi_bcn_tmpl_ml_info) * cu_arg.num_ml_info);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb) {
		if (cu_arg.num_ml_params)
			kfree(cu_arg.ml_params);
		if (cu_arg.num_ml_info)
			kfree(cu_arg.ml_info);
		return -ENOMEM;
	}

	cmd = (struct wmi_bcn_tmpl_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_BCN_TMPL_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->tim_ie_offset = cpu_to_le32(offs->tim_offset);

	if (conf->csa_active) {
		cmd->csa_switch_count_offset =
				cpu_to_le32(offs->cntdwn_counter_offs[0]);
		cmd->ext_csa_switch_count_offset =
				cpu_to_le32(offs->cntdwn_counter_offs[1]);
		cmd->csa_event_bitmap = cpu_to_le32(0xFFFFFFFF);
		arvif->current_cntdown_counter = bcn->data[offs->cntdwn_counter_offs[0]];
	}

	cmd->buf_len = cpu_to_le32(bcn->len);
	cmd->mbssid_ie_offset = cpu_to_le32(offs->mbssid_off);
	if (ema_args) {
		u32p_replace_bits(&ema_params, ema_args->bcn_cnt, WMI_EMA_BEACON_CNT);
		u32p_replace_bits(&ema_params, ema_args->bcn_index, WMI_EMA_BEACON_IDX);
		if (ema_args->bcn_index == 0)
			u32p_replace_bits(&ema_params, 1, WMI_EMA_BEACON_FIRST);
		if (ema_args->bcn_index + 1 == ema_args->bcn_cnt)
			u32p_replace_bits(&ema_params, 1, WMI_EMA_BEACON_LAST);
		cmd->ema_params = cpu_to_le32(ema_params);
	}
	cmd->feature_enable_bitmap = cpu_to_le32(u32_encode_bits(arvif->beacon_prot,
						 WMI_BEACON_PROTECTION_EN_BIT));

	ptr = skb->data + sizeof(*cmd);

	bcn_prb_info = ptr;
	len = sizeof(*bcn_prb_info);
	bcn_prb_info->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_BCN_PRB_INFO,
							  len);
	bcn_prb_info->caps = 0;
	bcn_prb_info->erp = 0;

	ptr += sizeof(*bcn_prb_info);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, aligned_len);
	memcpy(tlv->value, bcn->data, bcn->len);

	ptr += (TLV_HDR_SIZE + aligned_len);

	ptr = ath12k_wmi_append_critical_update_params(ar, vdev_id, ptr,
						       &cu_arg);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_BCN_TMPL_CMDID);
	if (ret) {
		ath12k_warn(ab, "failed to send WMI_BCN_TMPL_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_install_key(struct ath12k *ar,
				struct wmi_vdev_install_key_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_install_key_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	int ret, len, key_len_aligned;

	/* WMI_TAG_ARRAY_BYTE needs to be aligned with 4, the actual key
	 * length is specified in cmd->key_len.
	 */
	key_len_aligned = roundup(arg->key_len, 4);

	len = sizeof(*cmd) + TLV_HDR_SIZE + key_len_aligned;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_install_key_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_INSTALL_KEY_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, arg->macaddr);
	cmd->key_idx = cpu_to_le32(arg->key_idx);
	cmd->key_flags = cpu_to_le32(arg->key_flags);
	cmd->key_cipher = cpu_to_le32(arg->key_cipher);
	cmd->key_len = cpu_to_le32(arg->key_len);
	cmd->key_txmic_len = cpu_to_le32(arg->key_txmic_len);
	cmd->key_rxmic_len = cpu_to_le32(arg->key_rxmic_len);

	if (arg->key_rsc_counter)
		cmd->key_rsc_counter = cpu_to_le64(arg->key_rsc_counter);

	tlv = (struct wmi_tlv *)(skb->data + sizeof(*cmd));
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, key_len_aligned);
	memcpy(tlv->value, arg->key_data, arg->key_len);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI | ATH12K_DBG_EAPOL,
		   "WMI vdev install key idx %d cipher %d len %d\n",
		   arg->key_idx, arg->key_cipher, arg->key_len);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_INSTALL_KEY_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_VDEV_INSTALL_KEY cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static void ath12k_wmi_copy_peer_flags(struct wmi_peer_assoc_complete_cmd *cmd,
				       struct ath12k_wmi_peer_assoc_arg *arg,
				       bool hw_crypto_disabled)
{
	cmd->peer_flags = 0;
	cmd->peer_flags_ext = 0;

	if (arg->is_wme_set) {
		if (arg->qos_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_QOS);
		if (arg->apsd_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_APSD);
		if (arg->ht_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_HT);
		if (arg->bw_40)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_40MHZ);
		if (arg->bw_80)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_80MHZ);
		if (arg->bw_160)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_160MHZ);
		if (arg->bw_320)
			cmd->peer_flags_ext |= cpu_to_le32(WMI_PEER_EXT_320MHZ);

		/* Typically if STBC is enabled for VHT it should be enabled
		 * for HT as well
		 **/
		if (arg->stbc_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_STBC);

		/* Typically if LDPC is enabled for VHT it should be enabled
		 * for HT as well
		 **/
		if (arg->ldpc_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_LDPC);

		if (arg->static_mimops_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_STATIC_MIMOPS);
		if (arg->dynamic_mimops_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_DYN_MIMOPS);
		if (arg->spatial_mux_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_SPATIAL_MUX);
		if (arg->vht_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_VHT);
		if (arg->he_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_HE);
		if (arg->twt_requester)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_TWT_REQ);
		if (arg->twt_responder)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_TWT_RESP);
		if (arg->eht_flag)
			cmd->peer_flags_ext |= cpu_to_le32(WMI_PEER_EXT_EHT);
	}

	/* Suppress authorization for all AUTH modes that need 4-way handshake
	 * (during re-association).
	 * Authorization will be done for these modes on key installation.
	 */
	if (arg->auth_flag  && !arg->ml.ml_reconfig)
		cmd->peer_flags |= cpu_to_le32(WMI_PEER_AUTH);
	if (arg->need_ptk_4_way) {
		cmd->peer_flags |= cpu_to_le32(WMI_PEER_NEED_PTK_4_WAY);
		if (!hw_crypto_disabled && arg->is_assoc)
			cmd->peer_flags &= cpu_to_le32(~WMI_PEER_AUTH);
	}
	if (arg->need_gtk_2_way)
		cmd->peer_flags |= cpu_to_le32(WMI_PEER_NEED_GTK_2_WAY);
	/* safe mode bypass the 4-way handshake */
	if (arg->safe_mode_enabled)
		cmd->peer_flags &= cpu_to_le32(~(WMI_PEER_NEED_PTK_4_WAY |
						 WMI_PEER_NEED_GTK_2_WAY));

	if (arg->is_pmf_enabled)
		cmd->peer_flags |= cpu_to_le32(WMI_PEER_PMF);

	/* Disable AMSDU for station transmit, if user configures it */
	/* Disable AMSDU for AP transmit to 11n Stations, if user configures
	 * it
	 * if (arg->amsdu_disable) Add after FW support
	 **/

	/* Target asserts if node is marked HT and all MCS is set to 0.
	 * Mark the node as non-HT if all the mcs rates are disabled through
	 * iwpriv
	 **/
	if (arg->peer_ht_rates.num_rates == 0)
		cmd->peer_flags &= cpu_to_le32(~WMI_PEER_HT);
}

#define EMLSR_PAD_DELAY_MAX    5
#define EMLSR_TRANS_DELAY_MAX  6
#define EML_TRANS_TIMEOUT_MAX  11
#define TU_TO_USEC(t)          ((t) << 10)  /* (t) x 1024 */

static u32 ath12k_wmi_get_emlsr_pad_delay_us(u16 eml_cap)
{
	/* IEEE Std 802.11be-2024 Table 9-417i—Encoding of the EMLSR
	 * Padding Delay subfield.
	*/
	u32 pad_delay = u16_get_bits(eml_cap, IEEE80211_EML_CAP_EMLSR_PADDING_DELAY);
	static const u32 pad_delay_us[EMLSR_PAD_DELAY_MAX] = {0, 32, 64, 128, 256};

	if (pad_delay >= EMLSR_PAD_DELAY_MAX)
		return 0;

	return pad_delay_us[pad_delay];
}

static u32 ath12k_wmi_get_emlsr_trans_delay_us(u16 eml_cap)
{
	/* IEEE Std 802.11be-2024 Table 9-417j—Encoding of the EMLSR
	 * Transition Delay subfield.
	 */
	u32 trans_delay = u16_get_bits(eml_cap,
				       IEEE80211_EML_CAP_EMLSR_TRANSITION_DELAY);
	static const u32 trans_delay_us[EMLSR_TRANS_DELAY_MAX] = {
		0, 16, 32, 64, 128, 256
	};

	if (trans_delay >= EMLSR_TRANS_DELAY_MAX)
		return 0;

	return trans_delay_us[trans_delay];
}

static u32 ath12k_wmi_get_emlsr_trans_timeout_us(u16 eml_cap)
{
	/* IEEE Std 802.11be-2024 Table 9-417m—Encoding of the
	 * Transition Timeout subfield.
	 */
	u8 timeout = u16_get_bits(eml_cap, IEEE80211_EML_CAP_TRANSITION_TIMEOUT);
	static const u32 trans_timeout_us[EML_TRANS_TIMEOUT_MAX] = {
		0, 128, 256, 512,
		TU_TO_USEC(1),
		TU_TO_USEC((1U << 1)),
		TU_TO_USEC((1U << 2)),
		TU_TO_USEC((1U << 3)),
		TU_TO_USEC((1U << 4)),
		TU_TO_USEC((1U << 5)),
		TU_TO_USEC((1U << 6)),
	};

	if (timeout >= EML_TRANS_TIMEOUT_MAX)
		return 0;

	return trans_timeout_us[timeout];
}

int ath12k_wmi_send_peer_assoc_cmd(struct ath12k *ar,
				   struct ath12k_wmi_peer_assoc_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_assoc_complete_cmd *cmd;
	struct ath12k_wmi_vht_rate_set_params *mcs;
	struct ath12k_wmi_he_rate_set_params *he_mcs;
	struct ath12k_wmi_eht_rate_set_params *eht_mcs;
	struct ath12k_wmi_ttlm_peer_params *ttlm_params;
	struct wmi_peer_assoc_mlo_params *ml_params;
	struct wmi_peer_assoc_mlo_partner_info_params *partner_info;
	struct wmi_peer_assoc_tid_to_link_map *ttlm;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;
	u32 peer_legacy_rates_align, eml_delay, eml_trans_timeout;
	u32 peer_ht_rates_align;
	int i, ret, len, dir, tid_num;
	u16 eml_cap;
	__le32 v;

	peer_legacy_rates_align = roundup(arg->peer_legacy_rates.num_rates,
					  sizeof(u32));
	peer_ht_rates_align = roundup(arg->peer_ht_rates.num_rates,
				      sizeof(u32));

	len = sizeof(*cmd) +
	      TLV_HDR_SIZE + (peer_legacy_rates_align * sizeof(u8)) +
	      TLV_HDR_SIZE + (peer_ht_rates_align * sizeof(u8)) +
	      sizeof(*mcs) + TLV_HDR_SIZE +
	      (sizeof(*he_mcs) * arg->peer_he_mcs_count) +
	      TLV_HDR_SIZE + (sizeof(*eht_mcs) * arg->peer_eht_mcs_count);

	if (arg->ml.enabled)
		len += TLV_HDR_SIZE + sizeof(*ml_params) +
		       TLV_HDR_SIZE + (arg->ml.num_partner_links * sizeof(*partner_info));
	else
		len += (2 * TLV_HDR_SIZE);

	len += TLV_HDR_SIZE + (arg->ttlm_params.num_dir * TTLM_MAX_NUM_TIDS *
			       sizeof(struct wmi_peer_assoc_tid_to_link_map));

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_ASSOC_COMPLETE_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(arg->vdev_id);

	cmd->peer_new_assoc = cpu_to_le32(arg->peer_new_assoc);
	cmd->peer_associd = cpu_to_le32(arg->peer_associd);
	cmd->punct_bitmap = cpu_to_le32(arg->punct_bitmap);

	ath12k_wmi_copy_peer_flags(cmd, arg,
				   test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED,
					    &ar->ab->ag->flags));

	ether_addr_copy(cmd->peer_macaddr.addr, arg->peer_mac);

	cmd->peer_rate_caps = cpu_to_le32(arg->peer_rate_caps);
	cmd->peer_caps = cpu_to_le32(arg->peer_caps);
	cmd->peer_listen_intval = cpu_to_le32(arg->peer_listen_intval);
	cmd->peer_ht_caps = cpu_to_le32(arg->peer_ht_caps);
	cmd->peer_max_mpdu = cpu_to_le32(arg->peer_max_mpdu);
	cmd->peer_mpdu_density = cpu_to_le32(arg->peer_mpdu_density);
	cmd->peer_vht_caps = cpu_to_le32(arg->peer_vht_caps);
	cmd->peer_phymode = cpu_to_le32(arg->peer_phymode);

	/* Update 11ax capabilities */
	cmd->peer_he_cap_info = cpu_to_le32(arg->peer_he_cap_macinfo[0]);
	cmd->peer_he_cap_info_ext = cpu_to_le32(arg->peer_he_cap_macinfo[1]);
	cmd->peer_he_cap_info_internal = cpu_to_le32(arg->peer_he_cap_macinfo_internal);
	cmd->peer_he_caps_6ghz = cpu_to_le32(arg->peer_he_caps_6ghz);
	cmd->peer_he_ops = cpu_to_le32(arg->peer_he_ops);
	for (i = 0; i < WMI_MAX_HECAP_PHY_SIZE; i++)
		cmd->peer_he_cap_phy[i] =
			cpu_to_le32(arg->peer_he_cap_phyinfo[i]);
	cmd->peer_ppet.numss_m1 = cpu_to_le32(arg->peer_ppet.numss_m1);
	cmd->peer_ppet.ru_info = cpu_to_le32(arg->peer_ppet.ru_bit_mask);
	for (i = 0; i < WMI_MAX_NUM_SS; i++)
		cmd->peer_ppet.ppet16_ppet8_ru3_ru0[i] =
			cpu_to_le32(arg->peer_ppet.ppet16_ppet8_ru3_ru0[i]);

	/* Update 11be capabilities */
	memcpy_and_pad(cmd->peer_eht_cap_mac, sizeof(cmd->peer_eht_cap_mac),
		       arg->peer_eht_cap_mac, sizeof(arg->peer_eht_cap_mac),
		       0);
	memcpy_and_pad(cmd->peer_eht_cap_phy, sizeof(cmd->peer_eht_cap_phy),
		       arg->peer_eht_cap_phy, sizeof(arg->peer_eht_cap_phy),
		       0);
	memcpy_and_pad(&cmd->peer_eht_ppet, sizeof(cmd->peer_eht_ppet),
		       &arg->peer_eht_ppet, sizeof(arg->peer_eht_ppet), 0);

	/* Update peer legacy rate information */
	ptr += sizeof(*cmd);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, peer_legacy_rates_align);

	ptr += TLV_HDR_SIZE;

	cmd->num_peer_legacy_rates = cpu_to_le32(arg->peer_legacy_rates.num_rates);
	memcpy(ptr, arg->peer_legacy_rates.rates,
	       arg->peer_legacy_rates.num_rates);

	/* Update peer HT rate information */
	ptr += peer_legacy_rates_align;

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, peer_ht_rates_align);
	ptr += TLV_HDR_SIZE;
	cmd->num_peer_ht_rates = cpu_to_le32(arg->peer_ht_rates.num_rates);
	memcpy(ptr, arg->peer_ht_rates.rates,
	       arg->peer_ht_rates.num_rates);

	/* VHT Rates */
	ptr += peer_ht_rates_align;

	mcs = ptr;

	mcs->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VHT_RATE_SET,
						 sizeof(*mcs));

	cmd->peer_nss = cpu_to_le32(arg->peer_nss);

	/* Update bandwidth-NSS mapping */
	cmd->peer_bw_rxnss_override = 0;
	cmd->peer_bw_rxnss_override |= cpu_to_le32(arg->peer_bw_rxnss_override);

	if (arg->vht_capable) {
		mcs->rx_max_rate = cpu_to_le32(arg->rx_max_rate);
		mcs->rx_mcs_set = cpu_to_le32(arg->rx_mcs_set);
		mcs->tx_max_rate = cpu_to_le32(arg->tx_max_rate);
		mcs->tx_mcs_set = cpu_to_le32(arg->tx_mcs_set);
	}

	/* HE Rates */
	cmd->peer_he_mcs = cpu_to_le32(arg->peer_he_mcs_count);
	cmd->min_data_rate = cpu_to_le32(arg->min_data_rate);

	ptr += sizeof(*mcs);

	len = arg->peer_he_mcs_count * sizeof(*he_mcs);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE;

	/* Loop through the HE rate set */
	for (i = 0; i < arg->peer_he_mcs_count; i++) {
		he_mcs = ptr;
		he_mcs->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_HE_RATE_SET,
							    sizeof(*he_mcs));

		he_mcs->rx_mcs_set = cpu_to_le32(arg->peer_he_tx_mcs_set[i]);
		he_mcs->tx_mcs_set = cpu_to_le32(arg->peer_he_rx_mcs_set[i]);
		ptr += sizeof(*he_mcs);
	}

	tlv = ptr;
	len = arg->ml.enabled ? sizeof(*ml_params) : 0;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE;
	if (!len)
		goto skip_ml_params;

	ml_params = ptr;
	ml_params->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_PEER_ASSOC_PARAMS,
						       len);
	ml_params->flags = cpu_to_le32(ATH12K_WMI_FLAG_MLO_ENABLED);

	if (arg->ml.assoc_link)
		ml_params->flags |= cpu_to_le32(ATH12K_WMI_FLAG_MLO_ASSOC_LINK);

	if (arg->ml.primary_umac)
		ml_params->flags |= cpu_to_le32(ATH12K_WMI_FLAG_MLO_PRIMARY_UMAC);

	if (arg->ml.logical_link_idx_valid)
		ml_params->flags |=
			cpu_to_le32(ATH12K_WMI_FLAG_MLO_LOGICAL_LINK_IDX_VALID);

	if (arg->ml.peer_id_valid)
		ml_params->flags |= cpu_to_le32(ATH12K_WMI_FLAG_MLO_PEER_ID_VALID);

	if (arg->ml.bridge_peer)
		ml_params->flags |= cpu_to_le32(ATH12K_WMI_FLAG_MLO_BRIDGE_PEER);

	if (arg->ml.mlo_link_add)
		ml_params->flags |= cpu_to_le32(ATH12K_WMI_FLAG_MLO_LINK_ADD);

	if (arg->ml.mlo_link_del)
		ml_params->flags |= cpu_to_le32(ATH12K_WMI_FLAG_MLO_LINK_DEL);

	ether_addr_copy(ml_params->mld_addr.addr, arg->ml.mld_addr);
	ml_params->logical_link_idx = cpu_to_le32(arg->ml.logical_link_idx);
	ml_params->ml_peer_id = cpu_to_le32(arg->ml.ml_peer_id);
	ml_params->ieee_link_id = cpu_to_le32(arg->ml.ieee_link_id);

	if (arg->ml.ml_reconfig)
		ml_params->ml_reconfig = 1;

	eml_cap = arg->ml.eml_cap;
	if (u16_get_bits(eml_cap, IEEE80211_EML_CAP_EMLSR_SUPP)) {
		/* Padding delay */
		eml_delay = ath12k_wmi_get_emlsr_pad_delay_us(eml_cap);
		ml_params->emlsr_padding_delay_us = cpu_to_le32(eml_delay);
		/* Transition delay */
		eml_delay = ath12k_wmi_get_emlsr_trans_delay_us(eml_cap);
		ml_params->emlsr_trans_delay_us = cpu_to_le32(eml_delay);
		/* Transition timeout */
		eml_trans_timeout = ath12k_wmi_get_emlsr_trans_timeout_us(eml_cap);
		ml_params->emlsr_trans_timeout_us = cpu_to_le32(eml_trans_timeout);
		ml_params->flags |= cpu_to_le32(ATH12K_WMI_FLAG_MLO_EMLSR_SUPPORT);
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi peer (%pM) emlsr padding delay %u, trans delay %u trans timeout %u",
			   arg->peer_mac, ml_params->emlsr_padding_delay_us,
			   ml_params->emlsr_trans_delay_us,
			   ml_params->emlsr_trans_timeout_us);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_PEER, "peer (%pM) ml flags %x mld_addr %pM logical_link_idx %u ml peer id %d ieee_link_id %u num_partner_links %d is_bridge_peer %d\n",
		   arg->peer_mac, ml_params->flags, ml_params->mld_addr.addr,
		   ml_params->logical_link_idx, ml_params->ml_peer_id,
		   ml_params->ieee_link_id,
		   arg->ml.num_partner_links, arg->ml.bridge_peer);

	ptr += sizeof(*ml_params);

skip_ml_params:
	/* Loop through the EHT rate set */
	len = arg->peer_eht_mcs_count * sizeof(*eht_mcs);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE;

	for (i = 0; i < arg->peer_eht_mcs_count; i++) {
		eht_mcs = ptr;
		eht_mcs->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_EHT_RATE_SET,
							     sizeof(*eht_mcs));

		eht_mcs->rx_mcs_set = cpu_to_le32(arg->peer_eht_rx_mcs_set[i]);
		eht_mcs->tx_mcs_set = cpu_to_le32(arg->peer_eht_tx_mcs_set[i]);
		ptr += sizeof(*eht_mcs);
	}

	/* Update MCS15 capability */
	if (!arg->enable_mcs15)
		cmd->peer_eht_ops |= cpu_to_le32(IEEE80211_EHT_OPER_MCS15_DISABLE);

	tlv = ptr;
	len = arg->ml.enabled ? arg->ml.num_partner_links * sizeof(*partner_info) : 0;
	/* fill ML Partner links */
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE;

	if (len == 0)
		goto ttlm;

	for (i = 0; i < arg->ml.num_partner_links; i++) {
		u32 cmd = WMI_TAG_MLO_PARTNER_LINK_PARAMS_PEER_ASSOC;

		partner_info = ptr;
		partner_info->tlv_header = ath12k_wmi_tlv_cmd_hdr(cmd,
								  sizeof(*partner_info));
		partner_info->vdev_id = cpu_to_le32(arg->ml.partner_info[i].vdev_id);
		partner_info->hw_link_id =
			cpu_to_le32(arg->ml.partner_info[i].hw_link_id);
		partner_info->flags = cpu_to_le32(ATH12K_WMI_FLAG_MLO_ENABLED);

		if (arg->ml.partner_info[i].assoc_link)
			partner_info->flags |=
				cpu_to_le32(ATH12K_WMI_FLAG_MLO_ASSOC_LINK);

		if (arg->ml.partner_info[i].primary_umac)
			partner_info->flags |=
				cpu_to_le32(ATH12K_WMI_FLAG_MLO_PRIMARY_UMAC);

		if (arg->ml.partner_info[i].logical_link_idx_valid) {
			v = cpu_to_le32(ATH12K_WMI_FLAG_MLO_LINK_ID_VALID);
			partner_info->flags |= v;
		}

		if (arg->ml.partner_info[i].bridge_peer)
			partner_info->flags |=
				cpu_to_le32(ATH12K_WMI_FLAG_MLO_BRIDGE_PEER);

		if (arg->ml.partner_info[i].mlo_link_add)
			partner_info->flags |= cpu_to_le32(ATH12K_WMI_FLAG_MLO_LINK_ADD);

		if (arg->ml.partner_info[i].mlo_link_del)
			partner_info->flags |= cpu_to_le32(ATH12K_WMI_FLAG_MLO_LINK_DEL);

		partner_info->logical_link_idx =
			cpu_to_le32(arg->ml.partner_info[i].logical_link_idx);
		ptr += sizeof(*partner_info);
	}

ttlm:
	/* add ttlm params */
	ttlm_params = &arg->ttlm_params;
	tlv = ptr;
	len = ttlm_params->num_dir * TTLM_MAX_NUM_TIDS * sizeof(*ttlm);
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE;

	if (!len)
		goto send;

	for (dir = 0; dir < ttlm_params->num_dir; dir++) {
		struct ath12k_wmi_host_ttlm_of_tids *ttlm_of_tids = &ttlm_params->ttlm_info[dir];

		for (tid_num = 0; tid_num < TTLM_MAX_NUM_TIDS; tid_num++) {
			ttlm = (struct wmi_peer_assoc_tid_to_link_map *)ptr;
			ttlm->tlv_header =
				ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_ASSOC_TID_TO_LINK_MAP,
						       sizeof(*ttlm) - TLV_HDR_SIZE);
			ttlm->tid_to_link_map_info = 0;
			/* populate tid number */
			ttlm->tid_to_link_map_info |= le32_encode_bits(tid_num, WMI_TTLM_TID_MASK);
			/* populate the direction */
			ttlm->tid_to_link_map_info |= le32_encode_bits(ttlm_of_tids->direction,
								       WMI_TTLM_DIR_MASK);
			/* populate default link mapping value */
			ttlm->tid_to_link_map_info |=
				le32_encode_bits(ttlm_of_tids->default_link_mapping,
						 WMI_TTLM_DEFAULT_MAPPING_MASK);
			/* populate ttlm provisioned links for the corressponding tid number */
			ttlm->tid_to_link_map_info |=
				le32_encode_bits(ttlm_of_tids->ttlm_provisioned_links[tid_num],
						 WMI_TTLM_LINK_MAPPING_MASK);
			ptr += sizeof(*ttlm);
		}
	}

send:
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI | ATH12K_DBG_MLME,
		   "wmi peer assoc vdev id %d assoc id %d peer mac %pM peer_flags %x rate_caps %x peer_caps %x listen_intval %d ht_caps %x max_mpdu %d nss %d phymode %d peer_mpdu_density %d vht_caps %x he cap_info %x he ops %x he cap_info_ext %x he phy %x %x %x peer_bw_rxnss_override %x peer_flags_ext %x eht mac_cap %x %x eht phy_cap %x %x %x peer_eht_ops %x\n",
		   cmd->vdev_id, cmd->peer_associd, arg->peer_mac,
		   cmd->peer_flags, cmd->peer_rate_caps, cmd->peer_caps,
		   cmd->peer_listen_intval, cmd->peer_ht_caps,
		   cmd->peer_max_mpdu, cmd->peer_nss, cmd->peer_phymode,
		   cmd->peer_mpdu_density,
		   cmd->peer_vht_caps, cmd->peer_he_cap_info,
		   cmd->peer_he_ops, cmd->peer_he_cap_info_ext,
		   cmd->peer_he_cap_phy[0], cmd->peer_he_cap_phy[1],
		   cmd->peer_he_cap_phy[2],
		   cmd->peer_bw_rxnss_override, cmd->peer_flags_ext,
		   cmd->peer_eht_cap_mac[0], cmd->peer_eht_cap_mac[1],
		   cmd->peer_eht_cap_phy[0], cmd->peer_eht_cap_phy[1],
		   cmd->peer_eht_cap_phy[2], cmd->peer_eht_ops);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_ASSOC_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PEER_ASSOC_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_update_scan_chan_list(struct ath12k *ar,
				     struct ath12k_wmi_scan_req_arg *req_arg)
{
	struct ieee80211_supported_band **bands;
	struct ath12k_wmi_scan_chan_list_arg *arg;
	struct cfg80211_chan_def *chandef;
	struct ieee80211_channel *channel, *req_channel;
	struct ieee80211_hw *hw = ar->ah->hw;
	struct ath12k_wmi_channel_arg *ch;
	enum nl80211_band band;
	int num_channels = 0;
	int i, ret;
	bool found = false;

	bands = hw->wiphy->bands;
	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		if (!(ar->mac.sbands[band].channels && bands[band]))
			continue;

		for (i = 0; i < bands[band]->n_channels; i++) {
			if (bands[band]->channels[i].flags &
			    IEEE80211_CHAN_DISABLED)
				continue;

			if (band == NL80211_BAND_5GHZ || band == NL80211_BAND_6GHZ)
				if (bands[band]->channels[i].center_freq <
				    ar->chan_info.low_freq ||
				    bands[band]->channels[i].center_freq >
				    ar->chan_info.high_freq)
					continue;

			num_channels++;
		}
	}

	if (!num_channels) {
		ath12k_warn(ar->ab, "pdev is not supported for this country\n");
		return -EOPNOTSUPP;
	}

	arg = kzalloc(struct_size(arg, channel, num_channels), GFP_KERNEL);

	if (!arg)
		return -ENOMEM;

	arg->pdev_id = ar->pdev->pdev_id;
	arg->nallchans = num_channels;

	ch = arg->channel;
	chandef = req_arg ? req_arg->chandef : NULL;
	req_channel = chandef ? chandef->chan : NULL;

	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		if (!(ar->mac.sbands[band].channels && bands[band]))
			continue;

		for (i = 0; i < bands[band]->n_channels; i++) {
			channel = &bands[band]->channels[i];

			if (channel->flags & IEEE80211_CHAN_DISABLED)
				continue;

			if (band == NL80211_BAND_5GHZ || band == NL80211_BAND_6GHZ) {
				if (bands[band]->channels[i].center_freq <
				    ar->chan_info.low_freq ||
				    bands[band]->channels[i].center_freq >
				    ar->chan_info.high_freq) {
					ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
						   "skip freq %d outside range %d-%d MHz\n",
						   bands[band]->channels[i].center_freq,
						   ar->chan_info.low_freq,
						   ar->chan_info.high_freq);
					continue;
				}
			}

                       if (req_channel && !found &&
                           req_channel->center_freq == channel->center_freq) {
                               ch->mhz = req_arg->chan_list.chan[0].freq;
                               ch->cfreq1 = chandef->center_freq1;
                               ch->cfreq2 = chandef->center_freq2;

                               ch->phy_mode = req_arg->chan_list.chan[0].phymode;
                               channel = req_channel;
			       arg->append_chan_list = true;
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

			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "mac channel [%d/%d] freq %d maxpower %d "
				   "regpower %d antenna %d cfreq1 %d mode %d\n",
				   i, arg->nallchans,
				   ch->mhz, ch->maxpower, ch->maxregpower,
				   ch->antennamax, ch->cfreq1, ch->phy_mode);

			ch++;
			/* TODO: use quarrter/half rate, cfreq12, dfs_cfreq2
			 * set_agile, reg_class_idx
			 */
		}
	}

	ret = ath12k_wmi_send_scan_chan_list_cmd(ar, arg);
	kfree(arg);

	return ret;
}

void ath12k_wmi_start_scan_init(struct ath12k *ar,
				struct ath12k_wmi_scan_req_arg *arg,
				enum nl80211_iftype vif_type)
{
	/* setup commonly used values */
	arg->scan_req_id = 1;
	arg->scan_priority = WMI_SCAN_PRIORITY_MEDIUM;
	arg->dwell_time_active = 50;
	arg->dwell_time_active_2g = 0;
	arg->dwell_time_passive = 150;
	arg->dwell_time_active_6g = 70;
	arg->dwell_time_passive_6g = 70;
	arg->min_rest_time = 50;
	arg->max_rest_time = 500;

	if (vif_type == NL80211_IFTYPE_STATION) {
		arg->scan_priority = WMI_SCAN_PRIORITY_HIGH;
		arg->dwell_time_active = 70;
		arg->dwell_time_active_2g = 70;
		arg->dwell_time_active_6g = 80;
		arg->dwell_time_passive_6g = 80;
	}

	if (ar->scan_min_rest_time)
		arg->min_rest_time = ar->scan_min_rest_time;
	if (ar->scan_max_rest_time)
		arg->max_rest_time = ar->scan_max_rest_time;

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
	arg->num_bssid = 1;

	/* fill bssid_list[0] with 0xff, otherwise bssid and RA will be
	 * ZEROs in probe request
	 */
	eth_broadcast_addr(arg->bssid_list[0].addr);
}

static void ath12k_wmi_copy_scan_event_cntrl_flags(struct wmi_start_scan_cmd *cmd,
						   struct ath12k_wmi_scan_req_arg *arg)
{
	/* Scan events subscription */
	if (arg->scan_ev_started)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_STARTED);
	if (arg->scan_ev_completed)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_COMPLETED);
	if (arg->scan_ev_bss_chan)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_BSS_CHANNEL);
	if (arg->scan_ev_foreign_chan)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_FOREIGN_CHAN);
	if (arg->scan_ev_dequeued)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_DEQUEUED);
	if (arg->scan_ev_preempted)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_PREEMPTED);
	if (arg->scan_ev_start_failed)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_START_FAILED);
	if (arg->scan_ev_restarted)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_RESTARTED);
	if (arg->scan_ev_foreign_chn_exit)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_FOREIGN_CHAN_EXIT);
	if (arg->scan_ev_suspended)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_SUSPENDED);
	if (arg->scan_ev_resumed)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_RESUMED);

	/** Set scan control flags */
	cmd->scan_ctrl_flags = 0;
	if (arg->scan_f_passive)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FLAG_PASSIVE);
	if (arg->scan_f_strict_passive_pch)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FLAG_STRICT_PASSIVE_ON_PCHN);
	if (arg->scan_f_promisc_mode)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FILTER_PROMISCUOS);
	if (arg->scan_f_capture_phy_err)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_CAPTURE_PHY_ERROR);
	if (arg->scan_f_half_rate)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FLAG_HALF_RATE_SUPPORT);
	if (arg->scan_f_quarter_rate)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FLAG_QUARTER_RATE_SUPPORT);
	if (arg->scan_f_cck_rates)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_ADD_CCK_RATES);
	if (arg->scan_f_ofdm_rates)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_ADD_OFDM_RATES);
	if (arg->scan_f_chan_stat_evnt)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_CHAN_STAT_EVENT);
	if (arg->scan_f_filter_prb_req)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FILTER_PROBE_REQ);
	if (arg->scan_f_bcast_probe)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_ADD_BCAST_PROBE_REQ);
	if (arg->scan_f_offchan_mgmt_tx)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_OFFCHAN_MGMT_TX);
	if (arg->scan_f_offchan_data_tx)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_OFFCHAN_DATA_TX);
	if (arg->scan_f_force_active_dfs_chn)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FLAG_FORCE_ACTIVE_ON_DFS);
	if (arg->scan_f_add_tpc_ie_in_probe)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_ADD_TPC_IE_IN_PROBE_REQ);
	if (arg->scan_f_add_ds_ie_in_probe)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_ADD_DS_IE_IN_PROBE_REQ);
	if (arg->scan_f_add_spoofed_mac_in_probe)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_ADD_SPOOF_MAC_IN_PROBE_REQ);
	if (arg->scan_f_add_rand_seq_in_probe)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_RANDOM_SEQ_NO_IN_PROBE_REQ);
	if (arg->scan_f_en_ie_whitelist_in_probe)
		cmd->scan_ctrl_flags |=
			cpu_to_le32(WMI_SCAN_ENABLE_IE_WHTELIST_IN_PROBE_REQ);
	if (arg->scan_f_higher_mcs_nac_scan)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FLAG_HIGHER_MCS_NAC_SCAN);

	cmd->scan_ctrl_flags |= le32_encode_bits(arg->adaptive_dwell_time_mode,
						 WMI_SCAN_DWELL_MODE_MASK);
}

int ath12k_wmi_send_scan_start_cmd(struct ath12k *ar,
				   struct ath12k_wmi_scan_req_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct wmi_start_scan_cmd *cmd;
	struct ath12k_wmi_ssid_params *ssid = NULL;
	struct ath12k_wmi_mac_addr_params *bssid;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;
	int i, ret, len;
	u32 *tmp_ptr, extraie_len_with_pad = 0;
	u8 *phy_ptr;
	struct ath12k_wmi_hint_short_ssid_arg *s_ssid = NULL;
	struct ath12k_wmi_hint_bssid_arg *hint_bssid = NULL;
	u8 phymode_roundup = 0;

	len = sizeof(*cmd);

	len += TLV_HDR_SIZE;
	if (arg->chan_list.num_chan)
		len += arg->chan_list.num_chan * sizeof(u32);

	len += TLV_HDR_SIZE;
	if (arg->num_ssids)
		len += arg->num_ssids * sizeof(*ssid);

	len += TLV_HDR_SIZE;
	if (arg->num_bssid)
		len += sizeof(*bssid) * arg->num_bssid;

	if (arg->num_hint_bssid)
		len += TLV_HDR_SIZE +
		       arg->num_hint_bssid * sizeof(*hint_bssid);

	if (arg->num_hint_s_ssid)
		len += TLV_HDR_SIZE +
		       arg->num_hint_s_ssid * sizeof(*s_ssid);

	len += TLV_HDR_SIZE;
	if (arg->extraie.len && arg->extraie.len <= 0xFFFF)
		extraie_len_with_pad =
			roundup(arg->extraie.len, sizeof(u32));
	if (extraie_len_with_pad <= (wmi->wmi_ab->max_msg_len[ar->pdev_idx] - len)) {
		len += extraie_len_with_pad;
	} else {
		ath12k_warn(ar->ab, "discard large size %d bytes extraie for scan start\n",
			    arg->extraie.len);
		extraie_len_with_pad = 0;
	}

	len += TLV_HDR_SIZE;
	if (arg->scan_f_en_ie_whitelist_in_probe)
		len += arg->ie_whitelist.num_vendor_oui *
				sizeof(struct wmi_vendor_oui);

	len += TLV_HDR_SIZE;
	if (arg->scan_f_wide_band)
		phymode_roundup =
			roundup(arg->chan_list.num_chan * sizeof(u8),
				sizeof(u32));
	len += phymode_roundup;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_START_SCAN_CMD,
						 sizeof(*cmd));

	cmd->scan_id = cpu_to_le32(arg->scan_id);
	cmd->scan_req_id = cpu_to_le32(arg->scan_req_id);
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	if (ar->state_11d == ATH12K_11D_PREPARING)
		arg->scan_priority = WMI_SCAN_PRIORITY_MEDIUM;
	else
		arg->scan_priority = WMI_SCAN_PRIORITY_LOW;
	cmd->notify_scan_events = cpu_to_le32(arg->notify_scan_events);

	/* Set NAC scan flag if any NRP is set already */
	spin_lock_bh(&dp->dp_lock);
	if (!list_empty(&dp->neighbor_peers))
		arg->scan_f_higher_mcs_nac_scan = true;
	spin_unlock_bh(&dp->dp_lock);

	ath12k_wmi_copy_scan_event_cntrl_flags(cmd, arg);

	cmd->dwell_time_active = cpu_to_le32(arg->dwell_time_active);
	cmd->dwell_time_active_2g = cpu_to_le32(arg->dwell_time_active_2g);
	cmd->dwell_time_passive = cpu_to_le32(arg->dwell_time_passive);
	cmd->dwell_time_active_6g = cpu_to_le32(arg->dwell_time_active_6g);
	cmd->dwell_time_passive_6g = cpu_to_le32(arg->dwell_time_passive_6g);
	cmd->min_rest_time = cpu_to_le32(arg->min_rest_time);
	cmd->max_rest_time = cpu_to_le32(arg->max_rest_time);
	cmd->repeat_probe_time = cpu_to_le32(arg->repeat_probe_time);
	cmd->probe_spacing_time = cpu_to_le32(arg->probe_spacing_time);
	cmd->idle_time = cpu_to_le32(arg->idle_time);
	cmd->max_scan_time = cpu_to_le32(arg->max_scan_time);
	cmd->probe_delay = cpu_to_le32(arg->probe_delay);
	cmd->burst_duration = cpu_to_le32(arg->burst_duration);
	cmd->num_chan = cpu_to_le32(arg->chan_list.num_chan);
	cmd->num_bssid = cpu_to_le32(arg->num_bssid);
	cmd->num_ssids = cpu_to_le32(arg->num_ssids);
	cmd->ie_len = cpu_to_le32(arg->extraie.len);
	cmd->n_probes = cpu_to_le32(arg->n_probes);

	ptr += sizeof(*cmd);

	len = arg->chan_list.num_chan * sizeof(u32);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, len);
	ptr += TLV_HDR_SIZE;
	tmp_ptr = (u32 *)ptr;

	for (i = 0; i < arg->chan_list.num_chan; ++i)
		tmp_ptr[i] = arg->chan_list.chan[i].freq;

	ptr += len;

	len = arg->num_ssids * sizeof(*ssid);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_FIXED_STRUCT, len);

	ptr += TLV_HDR_SIZE;

	if (arg->num_ssids) {
		ssid = ptr;
		for (i = 0; i < arg->num_ssids; ++i) {
			ssid->ssid_len = cpu_to_le32(arg->ssid[i].ssid_len);
			memcpy(ssid->ssid, arg->ssid[i].ssid,
			       arg->ssid[i].ssid_len);
			ssid++;
		}
	}

	ptr += (arg->num_ssids * sizeof(*ssid));
	len = arg->num_bssid * sizeof(*bssid);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_FIXED_STRUCT, len);

	ptr += TLV_HDR_SIZE;
	bssid = ptr;

	if (arg->num_bssid) {
		for (i = 0; i < arg->num_bssid; ++i) {
			ether_addr_copy(bssid->addr,
					arg->bssid_list[i].addr);
			bssid++;
		}
	}

	ptr += arg->num_bssid * sizeof(*bssid);

	len = extraie_len_with_pad;
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, len);
	ptr += TLV_HDR_SIZE;

	if (extraie_len_with_pad)
		memcpy(ptr, arg->extraie.ptr,
		       arg->extraie.len);

	ptr += extraie_len_with_pad;

       len = arg->ie_whitelist.num_vendor_oui * sizeof(struct wmi_vendor_oui);
       tlv = ptr;
       tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
                     FIELD_PREP(WMI_TLV_LEN, len);
       ptr += TLV_HDR_SIZE;

       if (arg->scan_f_en_ie_whitelist_in_probe) {
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
       if (arg->scan_f_wide_band) {
               phy_ptr = ptr;
               /* Add PHY mode TLV for wide band scan with phymode + 1 value
                * so that phymode '0' is ignored by FW as default value.
                */
               for (i = 0; i < arg->chan_list.num_chan; ++i)
                       phy_ptr[i] = arg->chan_list.chan[i].phymode + 1;
       }
       ptr += phymode_roundup;

	if (arg->num_hint_s_ssid) {
		len = arg->num_hint_s_ssid * sizeof(*s_ssid);
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_FIXED_STRUCT, len);
		ptr += TLV_HDR_SIZE;
		s_ssid = ptr;
		for (i = 0; i < arg->num_hint_s_ssid; ++i) {
			s_ssid->freq_flags = arg->hint_s_ssid[i].freq_flags;
			s_ssid->short_ssid = arg->hint_s_ssid[i].short_ssid;
			s_ssid++;
		}
		ptr += len;
	}

	if (arg->num_hint_bssid) {
		len = arg->num_hint_bssid * sizeof(struct ath12k_wmi_hint_bssid_arg);
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_FIXED_STRUCT, len);
		ptr += TLV_HDR_SIZE;
		hint_bssid = ptr;
		for (i = 0; i < arg->num_hint_bssid; ++i) {
			hint_bssid->freq_flags =
				arg->hint_bssid[i].freq_flags;
			ether_addr_copy(&arg->hint_bssid[i].bssid.addr[0],
					&hint_bssid->bssid.addr[0]);
			hint_bssid++;
		}
	}

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_START_SCAN_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_START_SCAN_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

/* HOST_TO_FW_DBM_MULTIPLIER - Scaling factor for dBm (in 0.25dBm units) values
 * sent to firmware
 */
#define HOST_TO_FW_DBM_MULTIPLIER 4
/**
 * ath12k_wmi_fill_tpc_power_cmd_header - Populate the fixed header fields
 *                                        for the TPC power WMI command
 * @cmd: Pointer to the WMI command structure to be filled
 * @vdev_id: Virtual device identifier
 * @param: Pointer to the TPC power configuration parameters
 *
 * This helper function initializes the fixed portion of the
 * WMI_VDEV_SET_TPC_POWER_CMD structure. It sets the TLV header,
 * virtual device ID, PSD power flag, EIRP power (converted to firmware
 * units), and 6 GHz power type.
 */
static void
ath12k_wmi_fill_tpc_power_cmd_header(struct wmi_vdev_set_tpc_power_cmd *cmd,
				     u32 vdev_id,
				     struct ath12k_reg_tpc_power_info *param)
{
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_SET_TPC_POWER_CMD, sizeof(*cmd));
	cmd->vdev_id = vdev_id;
	cmd->psd_power = param->is_psd_power;
	cmd->eirp_power = param->eirp_power * HOST_TO_FW_DBM_MULTIPLIER;
	cmd->power_type_6ghz = param->power_type_6g;
}

/**
 * ath12k_wmi_fill_empty_common_power_tlv - Fill an empty TLV for common power
 * info.
 * @ptr: Pointer to the current position in the WMI buffer; will be updated
 *
 * This helper function inserts a placeholder TLV for the common power info
 * array in the WMI command buffer. The TLV is tagged as an array of structures
 * but has a length of zero, indicating that no common power entries are present.
 *
 * This is required to maintain the expected TLV structure layout in the
 * firmware interface, even when no common power data is provided.
 */
static void
ath12k_wmi_fill_empty_common_power_tlv(u8 **ptr)
{
	struct wmi_tlv *tlv = (struct wmi_tlv *)(*ptr);

	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 0 * sizeof(struct wmi_vdev_ch_power_info));
	*ptr += TLV_HDR_SIZE;
}

/**
 * ath12k_wmi_fill_psd_power_array - Populate the PSD power TLV array in the WMI
 * buffer.
 * @ar: Pointer to the ath12k device context
 * @ptr: Pointer to the current position in the WMI buffer; will be updated
 * @param: Pointer to the TPC power configuration parameters
 *
 * This helper function fills the WMI buffer with an array of TLVs representing
 * per-channel PSD (Power Spectral Density) power levels. Each entry includes
 * the channel center frequency and the corresponding PSD power value, scaled
 * for firmware consumption.
 *
 * The function also logs each entry for debugging purposes and advances the
 * buffer pointer accordingly.
 */
static void
ath12k_wmi_fill_psd_power_array(struct ath12k *ar, u8 **ptr,
				struct ath12k_reg_tpc_power_info *param)
{
	struct wmi_vdev_ch_power_psd_info *ch_power_psd_info;
	struct wmi_tlv *tlv = (struct wmi_tlv *)(*ptr);
	u32 psd_info_tlv_header;
	int i;

	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 param->num_psd_pwr_levels * sizeof(*ch_power_psd_info));
	*ptr += TLV_HDR_SIZE;

	psd_info_tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_CH_PSD_POWER_INFO,
						     sizeof(*ch_power_psd_info));
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "PSD Array:\n");
	for (i = 0; i < param->num_psd_pwr_levels; ++i) {
		ch_power_psd_info = (struct wmi_vdev_ch_power_psd_info *)(*ptr);
		ch_power_psd_info->tlv_header = psd_info_tlv_header;
		ch_power_psd_info->chan_cfreq =
			param->chan_psd_power_info[i].chan_cfreq;
		ch_power_psd_info->psd_power =
			param->chan_psd_power_info[i].tx_power *
			HOST_TO_FW_DBM_MULTIPLIER;

		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "chan_cfreq = %u, psd_power = %d\n",
			   ch_power_psd_info->chan_cfreq,
			   ch_power_psd_info->psd_power);

		*ptr += sizeof(*ch_power_psd_info);
	}
}

/**
 * ath12k_wmi_fill_eirp_power_array - Populate the EIRP power TLV array in the
 * WMI buffer
 * @ar: Pointer to the ath12k device context
 * @ptr: Pointer to the current position in the WMI buffer; will be updated
 * @param: Pointer to the TPC power configuration parameters
 *
 * This helper function fills the WMI buffer with an array of TLVs representing
 * per-channel EIRP (Equivalent Isotropically Radiated Power) power levels.
 * Each entry includes the channel center frequency and the corresponding
 * EIRP power value, scaled for firmware consumption.
 *
 * The function logs each entry for debugging and advances the buffer pointer
 * accordingly to maintain correct TLV structure alignment.
 */
static void
ath12k_wmi_fill_eirp_power_array(struct ath12k *ar, u8 **ptr,
				 struct ath12k_reg_tpc_power_info *param)
{
	struct wmi_vdev_ch_power_eirp_info *ch_power_eirp_info;
	struct wmi_tlv *tlv = (struct wmi_tlv *)(*ptr);
	u32 eirp_info_tlv_header;
	int i;

	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 param->num_eirp_pwr_levels * sizeof(*ch_power_eirp_info));
	*ptr += TLV_HDR_SIZE;

	eirp_info_tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_CH_EIRP_POWER_INFO,
						      sizeof(*ch_power_eirp_info));
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "EIRP Array:\n");
	for (i = 0; i < param->num_eirp_pwr_levels; ++i) {
		ch_power_eirp_info =
			(struct wmi_vdev_ch_power_eirp_info *)(*ptr);
		ch_power_eirp_info->tlv_header = eirp_info_tlv_header;
		ch_power_eirp_info->chan_cfreq =
				param->chan_eirp_power_info[i].chan_cfreq;
		ch_power_eirp_info->eirp_power =
		    param->chan_eirp_power_info[i].tx_power *
		    HOST_TO_FW_DBM_MULTIPLIER;

		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "chan_cfreq = %u, eirp_power = %d\n",
			   ch_power_eirp_info->chan_cfreq,
			   ch_power_eirp_info->eirp_power);

		*ptr += sizeof(*ch_power_eirp_info);
	}
}

/**
 * ath12_wmi_send_vdev_set_both_psd_and_eirp_in_tpc_for_sp - Send WMI command
 * with both PSD and EIRP power levels
 * @ar: Pointer to ath12k device context
 * @vdev_id: Virtual device ID
 * @param: Pointer to TPC power info structure containing PSD and EIRP data
 *
 * Constructs and sends a WMI_VDEV_SET_TPC_POWER_CMD to firmware with both PSD
 * and EIRP values for 6 GHz Standard Power (SP) AP mode. The function allocates
 * a WMI buffer, populates the command and TLV structures for PSD and EIRP power
 * levels, and dispatches the command to firmware. It also includes debug
 * logging and error handling for traceability and robustness.
 *
 * Return: 0 on success, negative error code on failure.
 */
static int
ath12_wmi_send_vdev_set_both_psd_and_eirp_in_tpc_for_sp(struct ath12k *ar,
							u32 vdev_id,
							struct ath12k_reg_tpc_power_info *param)
{
	struct wmi_vdev_set_tpc_power_cmd *cmd;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	int len, ret;
	u8 *ptr;

	len = sizeof(*cmd) + TLV_HDR_SIZE;
	len += TLV_HDR_SIZE + (sizeof(struct wmi_vdev_ch_power_psd_info) *
			       param->num_psd_pwr_levels);
	len += TLV_HDR_SIZE + (sizeof(struct wmi_vdev_ch_power_eirp_info) *
			       param->num_eirp_pwr_levels);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;
	cmd = (struct wmi_vdev_set_tpc_power_cmd *)ptr;

	ath12k_wmi_fill_tpc_power_cmd_header(cmd, vdev_id, param);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi TPC vdev_id: %d is_psd_power: %d eirp_power: %d power_type_6g: %d\n",
		   vdev_id, param->is_psd_power, param->eirp_power,
		   param->power_type_6g);

	ptr += sizeof(*cmd);

	ath12k_wmi_fill_empty_common_power_tlv(&ptr);
	ath12k_wmi_fill_psd_power_array(ar, &ptr, param);
	ath12k_wmi_fill_eirp_power_array(ar, &ptr, param);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_SET_TPC_POWER_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_VDEV_SET_TPC_POWER_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_vdev_set_tpc_power(struct ath12k *ar,
                                       u32 vdev_id,
                                       struct ath12k_reg_tpc_power_info *param)
{
        struct ath12k_wmi_pdev *wmi = ar->wmi;
        struct wmi_vdev_set_tpc_power_cmd *cmd;
        struct wmi_vdev_ch_power_info *ch;
        struct sk_buff *skb;
        struct wmi_tlv *tlv;
        u8 *ptr;
        int i, ret, len;

	if (test_bit(WMI_TLV_SERVICE_BOTH_PSD_EIRP_FOR_AP_SP_CLIENT_SP_SUPPORT,
		     ar->ab->wmi_ab.svc_map) &&
	    (param->power_type_6g == WMI_REG_STD_POWER_AP ||
		 param->power_type_6g == REG_SP_CLIENT_TYPE))
		return ath12_wmi_send_vdev_set_both_psd_and_eirp_in_tpc_for_sp(ar,
							vdev_id, param);

        len = sizeof(*cmd) + TLV_HDR_SIZE;
        len += (sizeof(struct wmi_vdev_ch_power_info) * param->num_pwr_levels);

        skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
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
        ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
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

                ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
                           "wmi TPC chan_cfreq: %d , tx_power: %d\n",
                           ch->chan_cfreq, ch->tx_power);
        }

        ret = ath12k_wmi_cmd_send(wmi, skb,
                                  WMI_VDEV_SET_TPC_POWER_CMDID);
        if (ret) {
                ath12k_warn(ar->ab, "failed to send WMI_VDEV_SET_TPC_POWER_CMDID\n");
                dev_kfree_skb(skb);
        }
        return ret;
}

int ath12k_wmi_send_scan_stop_cmd(struct ath12k *ar,
				  struct ath12k_wmi_scan_cancel_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_stop_scan_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_stop_scan_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_STOP_SCAN_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->requestor = cpu_to_le32(arg->requester);
	cmd->scan_id = cpu_to_le32(arg->scan_id);
	cmd->pdev_id = cpu_to_le32(arg->pdev_id);
	/* stop the scan with the corresponding scan_id */
	if (arg->req_type == WLAN_SCAN_CANCEL_PDEV_ALL) {
		/* Cancelling all scans */
		cmd->req_type = cpu_to_le32(WMI_SCAN_STOP_ALL);
	} else if (arg->req_type == WLAN_SCAN_CANCEL_VDEV_ALL) {
		/* Cancelling VAP scans */
		cmd->req_type = cpu_to_le32(WMI_SCAN_STOP_VAP_ALL);
	} else if (arg->req_type == WLAN_SCAN_CANCEL_SINGLE) {
		/* Cancelling specific scan */
		cmd->req_type = WMI_SCAN_STOP_ONE;
	} else {
		ath12k_warn(ar->ab, "invalid scan cancel req_type %d",
			    arg->req_type);
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_STOP_SCAN_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_STOP_SCAN_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_scan_chan_list_cmd(struct ath12k *ar,
				       struct ath12k_wmi_scan_chan_list_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_scan_chan_list_cmd *cmd;
	struct sk_buff *skb;
	struct ath12k_wmi_channel_params *chan_info;
	struct ath12k_wmi_channel_arg *channel_arg;
	struct wmi_tlv *tlv;
	void *ptr;
	int i, ret, len;
	u16 num_send_chans, num_sends = 0, max_chan_limit = 0;
	__le32 *reg1, *reg2;

	channel_arg = &arg->channel[0];
	while (arg->nallchans) {
		len = sizeof(*cmd) + TLV_HDR_SIZE;
		max_chan_limit = (wmi->wmi_ab->max_msg_len[ar->pdev_idx] - len) /
			sizeof(*chan_info);

		num_send_chans = min(arg->nallchans, max_chan_limit);

		arg->nallchans -= num_send_chans;
		len += sizeof(*chan_info) * num_send_chans;

		skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
		if (!skb)
			return -ENOMEM;

		cmd = (struct wmi_scan_chan_list_cmd *)skb->data;
		cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SCAN_CHAN_LIST_CMD,
							 sizeof(*cmd));
		cmd->pdev_id = cpu_to_le32(arg->pdev_id);
		cmd->num_scan_chans = cpu_to_le32(num_send_chans);
		if (num_sends || arg->append_chan_list)
			cmd->flags |= cpu_to_le32(WMI_APPEND_TO_EXISTING_CHAN_LIST_FLAG);

		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "WMI no.of chan = %d len = %d pdev_id = %d num_sends = %d append_chan_list %d\n",
			   num_send_chans, len, cmd->pdev_id, num_sends, arg->append_chan_list);

		ptr = skb->data + sizeof(*cmd);

		len = sizeof(*chan_info) * num_send_chans;
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ARRAY_STRUCT,
						     len);
		ptr += TLV_HDR_SIZE;

		for (i = 0; i < num_send_chans; ++i) {
			chan_info = ptr;
			memset(chan_info, 0, sizeof(*chan_info));
			len = sizeof(*chan_info);
			chan_info->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_CHANNEL,
								       len);

			reg1 = &chan_info->reg_info_1;
			reg2 = &chan_info->reg_info_2;
			chan_info->mhz = cpu_to_le32(channel_arg->mhz);
			chan_info->band_center_freq1 = cpu_to_le32(channel_arg->cfreq1);
			chan_info->band_center_freq2 = cpu_to_le32(channel_arg->cfreq2);

			if (channel_arg->is_chan_passive)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_PASSIVE);
			if (channel_arg->allow_he)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_HE);
			else if (channel_arg->allow_vht)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_VHT);
			else if (channel_arg->allow_ht)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_HT);
			if (channel_arg->half_rate)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_HALF_RATE);
			if (channel_arg->quarter_rate)
				chan_info->info |=
					cpu_to_le32(WMI_CHAN_INFO_QUARTER_RATE);

			if (channel_arg->psc_channel)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_PSC);

			if (channel_arg->dfs_set)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_DFS);

			chan_info->info |= le32_encode_bits(channel_arg->phy_mode,
							    WMI_CHAN_INFO_MODE);
			*reg1 |= le32_encode_bits(channel_arg->minpower,
						  WMI_CHAN_REG_INFO1_MIN_PWR);
			*reg1 |= le32_encode_bits(channel_arg->maxpower,
						  WMI_CHAN_REG_INFO1_MAX_PWR);
			*reg1 |= le32_encode_bits(channel_arg->maxregpower,
						  WMI_CHAN_REG_INFO1_MAX_REG_PWR);
			*reg1 |= le32_encode_bits(channel_arg->reg_class_id,
						  WMI_CHAN_REG_INFO1_REG_CLS);
			*reg2 |= le32_encode_bits(channel_arg->antennamax,
						  WMI_CHAN_REG_INFO2_ANT_MAX);
			*reg2 |= le32_encode_bits(channel_arg->maxregpower,
						  WMI_CHAN_REG_INFO2_MAX_TX_PWR);

			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "WMI chan scan list chan[%d] = %u, chan_info->info %8x\n",
				   i, chan_info->mhz, chan_info->info);

			ptr += sizeof(*chan_info);

			channel_arg++;
		}

		ret = ath12k_wmi_cmd_send(wmi, skb, WMI_SCAN_CHAN_LIST_CMDID);
		if (ret) {
			ath12k_warn(ar->ab, "failed to send WMI_SCAN_CHAN_LIST cmd\n");
			dev_kfree_skb(skb);
			return ret;
		}

		num_sends++;
	}

	return 0;
}

int ath12k_wmi_send_wmm_update_cmd(struct ath12k *ar, u32 vdev_id,
				   struct wmi_wmm_params_all_arg *param)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_set_wmm_params_cmd *cmd;
	struct wmi_wmm_params *wmm_param;
	struct wmi_wmm_params_arg *wmi_wmm_arg;
	struct sk_buff *skb;
	int ret, ac;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_set_wmm_params_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_SET_WMM_PARAMS_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->wmm_param_type = 0;

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
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_SET_WMM_PARAMS_CMD,
					       sizeof(*wmm_param));

		wmm_param->aifs = cpu_to_le32(wmi_wmm_arg->aifs);
		wmm_param->cwmin = cpu_to_le32(wmi_wmm_arg->cwmin);
		wmm_param->cwmax = cpu_to_le32(wmi_wmm_arg->cwmax);
		wmm_param->txoplimit = cpu_to_le32(wmi_wmm_arg->txop);
		wmm_param->acm = cpu_to_le32(wmi_wmm_arg->acm);
		wmm_param->no_ack = cpu_to_le32(wmi_wmm_arg->no_ack);

		ath12k_dbg_level(ar->ab, ATH12K_DBG_WMI, ATH12K_DBG_L3,
				 "wmi wmm set ac %d aifs %d cwmin %d cwmax %d txop %d acm %d no_ack %d\n",
				 ac, wmm_param->aifs, wmm_param->cwmin,
				 wmm_param->cwmax, wmm_param->txoplimit,
				 wmm_param->acm, wmm_param->no_ack);
	}
	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_VDEV_SET_WMM_PARAMS_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_VDEV_SET_WMM_PARAMS_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_dfs_phyerr_offload_enable_cmd(struct ath12k *ar,
						  u32 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_dfs_phyerr_offload_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_dfs_phyerr_offload_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_DFS_PHYERR_OFFLOAD_ENABLE_CMD,
				       sizeof(*cmd));

	cmd->pdev_id = cpu_to_le32(pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI dfs phy err offload enable pdev id %d\n", pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_DFS_PHYERR_OFFLOAD_ENABLE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PDEV_DFS_PHYERR_OFFLOAD_ENABLE cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_set_bios_cmd(struct ath12k_base *ab, u32 param_id,
			    const u8 *buf, size_t buf_len)
{
	struct ath12k_wmi_base *wmi_ab = &ab->wmi_ab;
	struct wmi_pdev_set_bios_interface_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	u8 *ptr;
	u32 len, len_aligned;
	int ret;

	len_aligned = roundup(buf_len, sizeof(u32));
	len = sizeof(*cmd) + TLV_HDR_SIZE + len_aligned;

	skb = ath12k_wmi_alloc_skb(wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_bios_interface_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_BIOS_INTERFACE_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(WMI_PDEV_ID_SOC);
	cmd->param_type_id = cpu_to_le32(param_id);
	cmd->length = cpu_to_le32(buf_len);

	ptr = skb->data + sizeof(*cmd);
	tlv = (struct wmi_tlv *)ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, len_aligned);
	ptr += TLV_HDR_SIZE;
	memcpy(ptr, buf, buf_len);

	ret = ath12k_wmi_cmd_send(&wmi_ab->wmi[0],
				  skb,
				  WMI_PDEV_SET_BIOS_INTERFACE_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "failed to send WMI_PDEV_SET_BIOS_INTERFACE_CMDID parameter id %d: %d\n",
			    param_id, ret);
		dev_kfree_skb(skb);
	}

	return 0;
}

int ath12k_wmi_set_bios_sar_cmd(struct ath12k_base *ab, const u8 *psar_table)
{
	struct ath12k_wmi_base *wmi_ab = &ab->wmi_ab;
	struct wmi_pdev_set_bios_sar_table_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	int ret;
	u8 *buf_ptr;
	u32 len, sar_table_len_aligned, sar_dbs_backoff_len_aligned;
	const u8 *psar_value = psar_table + ATH12K_ACPI_POWER_LIMIT_DATA_OFFSET;
	const u8 *pdbs_value = psar_table + ATH12K_ACPI_DBS_BACKOFF_DATA_OFFSET;

	sar_table_len_aligned = roundup(ATH12K_ACPI_BIOS_SAR_TABLE_LEN, sizeof(u32));
	sar_dbs_backoff_len_aligned = roundup(ATH12K_ACPI_BIOS_SAR_DBS_BACKOFF_LEN,
					      sizeof(u32));
	len = sizeof(*cmd) + TLV_HDR_SIZE + sar_table_len_aligned +
		TLV_HDR_SIZE + sar_dbs_backoff_len_aligned;

	skb = ath12k_wmi_alloc_skb(wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_bios_sar_table_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_BIOS_SAR_TABLE_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(WMI_PDEV_ID_SOC);
	cmd->sar_len = cpu_to_le32(ATH12K_ACPI_BIOS_SAR_TABLE_LEN);
	cmd->dbs_backoff_len = cpu_to_le32(ATH12K_ACPI_BIOS_SAR_DBS_BACKOFF_LEN);

	buf_ptr = skb->data + sizeof(*cmd);
	tlv = (struct wmi_tlv *)buf_ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE,
					 sar_table_len_aligned);
	buf_ptr += TLV_HDR_SIZE;
	memcpy(buf_ptr, psar_value, ATH12K_ACPI_BIOS_SAR_TABLE_LEN);

	buf_ptr += sar_table_len_aligned;
	tlv = (struct wmi_tlv *)buf_ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE,
					 sar_dbs_backoff_len_aligned);
	buf_ptr += TLV_HDR_SIZE;
	memcpy(buf_ptr, pdbs_value, ATH12K_ACPI_BIOS_SAR_DBS_BACKOFF_LEN);

	ret = ath12k_wmi_cmd_send(&wmi_ab->wmi[0],
				  skb,
				  WMI_PDEV_SET_BIOS_SAR_TABLE_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "failed to send WMI_PDEV_SET_BIOS_INTERFACE_CMDID %d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_set_bios_geo_cmd(struct ath12k_base *ab, const u8 *pgeo_table)
{
	struct ath12k_wmi_base *wmi_ab = &ab->wmi_ab;
	struct wmi_pdev_set_bios_geo_table_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	int ret;
	u8 *buf_ptr;
	u32 len, sar_geo_len_aligned;
	const u8 *pgeo_value = pgeo_table + ATH12K_ACPI_GEO_OFFSET_DATA_OFFSET;

	sar_geo_len_aligned = roundup(ATH12K_ACPI_BIOS_SAR_GEO_OFFSET_LEN, sizeof(u32));
	len = sizeof(*cmd) + TLV_HDR_SIZE + sar_geo_len_aligned;

	skb = ath12k_wmi_alloc_skb(wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_bios_geo_table_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_BIOS_GEO_TABLE_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(WMI_PDEV_ID_SOC);
	cmd->geo_len = cpu_to_le32(ATH12K_ACPI_BIOS_SAR_GEO_OFFSET_LEN);

	buf_ptr = skb->data + sizeof(*cmd);
	tlv = (struct wmi_tlv *)buf_ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, sar_geo_len_aligned);
	buf_ptr += TLV_HDR_SIZE;
	memcpy(buf_ptr, pgeo_value, ATH12K_ACPI_BIOS_SAR_GEO_OFFSET_LEN);

	ret = ath12k_wmi_cmd_send(&wmi_ab->wmi[0],
				  skb,
				  WMI_PDEV_SET_BIOS_GEO_TABLE_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "failed to send WMI_PDEV_SET_BIOS_GEO_TABLE_CMDID %d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_delba_send(struct ath12k *ar, u32 vdev_id, const u8 *mac,
			  u32 tid, u32 initiator, u32 reason)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_delba_send_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_delba_send_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_DELBA_SEND_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, mac);
	cmd->tid = cpu_to_le32(tid);
	cmd->initiator = cpu_to_le32(initiator);
	cmd->reasoncode = cpu_to_le32(reason);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi delba send vdev_id 0x%X mac_addr %pM tid %u initiator %u reason %u\n",
		   vdev_id, mac, tid, initiator, reason);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_DELBA_SEND_CMDID);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_DELBA_SEND_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_addba_set_resp(struct ath12k *ar, u32 vdev_id, const u8 *mac,
			      u32 tid, u32 status)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_addba_setresponse_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_addba_setresponse_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ADDBA_SETRESPONSE_CMD,
				       sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, mac);
	cmd->tid = cpu_to_le32(tid);
	cmd->statuscode = cpu_to_le32(status);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi addba set resp vdev_id 0x%X mac_addr %pM tid %u status %u\n",
		   vdev_id, mac, tid, status);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_ADDBA_SET_RESP_CMDID);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_ADDBA_SET_RESP_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_addba_send(struct ath12k *ar, u32 vdev_id, const u8 *mac,
			  u32 tid, u32 buf_size)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_addba_send_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_addba_send_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ADDBA_SEND_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, mac);
	cmd->tid = cpu_to_le32(tid);
	cmd->buffersize = cpu_to_le32(buf_size);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi addba send vdev_id 0x%X mac_addr %pM tid %u bufsize %u\n",
		   vdev_id, mac, tid, buf_size);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_ADDBA_SEND_CMDID);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_ADDBA_SEND_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_addba_clear_resp(struct ath12k *ar, u32 vdev_id, const u8 *mac)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_addba_clear_resp_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_addba_clear_resp_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ADDBA_CLEAR_RESP_CMD,
				       sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, mac);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi addba clear resp vdev_id 0x%X mac_addr %pM\n",
		   vdev_id, mac);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_ADDBA_CLEAR_RESP_CMDID);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_ADDBA_CLEAR_RESP_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_pdev_peer_pktlog_filter(struct ath12k *ar, u8 *addr, u8 enable)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_pktlog_filter_cmd *cmd;
	struct wmi_pdev_pktlog_filter_info *info;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;
	int ret, len;

	len = sizeof(*cmd) + sizeof(*info) + TLV_HDR_SIZE;
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_pktlog_filter_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_PEER_PKTLOG_FILTER_CMD,
						 sizeof(*cmd));

	cmd->pdev_id = cpu_to_le32(DP_HW2SW_MACID(ar->pdev->pdev_id));
	cmd->num_mac = cpu_to_le32(1);
	cmd->enable = cpu_to_le32(enable);

	ptr = skb->data + sizeof(*cmd);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, sizeof(*info));

	ptr += TLV_HDR_SIZE;
	info = ptr;

	ether_addr_copy(info->peer_macaddr.addr, addr);
	info->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_PEER_PKTLOG_FILTER_INFO,
						  sizeof(*info));

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_PKTLOG_FILTER_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PDEV_PKTLOG_ENABLE_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_init_country_cmd(struct ath12k *ar,
				     struct ath12k_wmi_init_country_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_init_country_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_init_country_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SET_INIT_COUNTRY_CMD,
				       sizeof(*cmd));

	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);

	switch (arg->flags) {
	case ALPHA_IS_SET:
		cmd->init_cc_type = WMI_COUNTRY_INFO_TYPE_ALPHA;
		memcpy(&cmd->cc_info.alpha2, arg->cc_info.alpha2, 3);
		break;
	case CC_IS_SET:
		cmd->init_cc_type = cpu_to_le32(WMI_COUNTRY_INFO_TYPE_COUNTRY_CODE);
		cmd->cc_info.country_code =
			cpu_to_le32(arg->cc_info.country_code);
		break;
	case REGDMN_IS_SET:
		cmd->init_cc_type = cpu_to_le32(WMI_COUNTRY_INFO_TYPE_REGDOMAIN);
		cmd->cc_info.regdom_id = cpu_to_le32(arg->cc_info.regdom_id);
		break;
	default:
		ret = -EINVAL;
		goto out;
	}

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_SET_INIT_COUNTRY_CMDID);

out:
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_SET_INIT_COUNTRY CMD :%d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_set_current_country_cmd(struct ath12k *ar,
					    struct wmi_set_current_country_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_set_current_country_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_set_current_country_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SET_CURRENT_COUNTRY_CMD,
				       sizeof(*cmd));

	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	memcpy(&cmd->new_alpha2, &arg->alpha2, sizeof(arg->alpha2));
	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_SET_CURRENT_COUNTRY_CMDID);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "set current country pdev id %d alpha2 %c%c\n",
		   ar->pdev->pdev_id,
		   arg->alpha2[0],
		   arg->alpha2[1]);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_SET_CURRENT_COUNTRY_CMDID: %d\n", ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath12k_wmi_send_thermal_mitigation_cmd(struct ath12k *ar,
				       struct ath12k_wmi_thermal_mitigation_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_therm_throt_config_request_cmd *cmd;
	struct wmi_therm_throt_level_config_info *lvl_conf;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	int i, ret, len;

	if (test_bit(WMI_SERVICE_THERM_THROT_5_LEVELS, ar->ab->wmi_ab.svc_map))
		len = sizeof(*cmd) + TLV_HDR_SIZE + (ENHANCED_THERMAL_LEVELS * sizeof(*lvl_conf));
	else
		len = sizeof(*cmd) + TLV_HDR_SIZE + (THERMAL_LEVELS * sizeof(*lvl_conf));

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_therm_throt_config_request_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_THERM_THROT_CONFIG_REQUEST,
						 sizeof(*cmd));

	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	cmd->enable = cpu_to_le32(arg->enable);
	cmd->dc = cpu_to_le32(arg->dc);
	cmd->dc_per_event = cpu_to_le32(arg->dc_per_event);
	if (test_bit(WMI_SERVICE_THERM_THROT_5_LEVELS, ar->ab->wmi_ab.svc_map))
		cmd->therm_throt_levels = cpu_to_le32(ENHANCED_THERMAL_LEVELS);
	else
		cmd->therm_throt_levels = cpu_to_le32(THERMAL_LEVELS);

	tlv = (struct wmi_tlv *)(skb->data + sizeof(*cmd));
	if (test_bit(WMI_SERVICE_THERM_THROT_5_LEVELS, ar->ab->wmi_ab.svc_map))
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 ENHANCED_THERMAL_LEVELS * sizeof(*lvl_conf));
	else
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 THERMAL_LEVELS * sizeof(*lvl_conf));

	lvl_conf = (struct wmi_therm_throt_level_config_info *)(skb->data +
								sizeof(*cmd) +
								TLV_HDR_SIZE);

	if (test_bit(WMI_SERVICE_THERM_THROT_5_LEVELS, ar->ab->wmi_ab.svc_map)) {
		for (i = 0; i < ENHANCED_THERMAL_LEVELS; i++) {
			lvl_conf->tlv_header =
				ath12k_wmi_tlv_cmd_hdr(WMI_TAG_THERM_THROT_LEVEL_CONFIG_INFO,
						       sizeof(*lvl_conf));

			lvl_conf->temp_lwm = arg->levelconf[i].tmplwm;
			lvl_conf->temp_hwm = arg->levelconf[i].tmphwm;
			lvl_conf->dc_off_percent = arg->levelconf[i].dcoffpercent;
			lvl_conf->prio = arg->levelconf[i].priority;

			if (test_bit(WMI_TLV_SERVICE_THERM_THROT_POUT_REDUCTION,
				     ar->ab->wmi_ab.svc_map))
				lvl_conf->pout_reduction_25db =
					arg->levelconf[i].pout_reduction_db;

			if (test_bit(WMI_SERVICE_THERM_THROT_TX_CHAIN_MASK,
				     ar->ab->wmi_ab.svc_map))
				lvl_conf->tx_chain_mask = arg->levelconf[i].tx_chain_mask;
			lvl_conf->duty_cycle = arg->levelconf[i].duty_cycle;
			lvl_conf++;
		}
	} else {
		for (i = 0; i < THERMAL_LEVELS; i++) {
			lvl_conf->tlv_header =
				ath12k_wmi_tlv_cmd_hdr(WMI_TAG_THERM_THROT_LEVEL_CONFIG_INFO,
						       sizeof(*lvl_conf));

			lvl_conf->temp_lwm = arg->levelconf[i].tmplwm;
			lvl_conf->temp_hwm = arg->levelconf[i].tmphwm;
			lvl_conf->dc_off_percent = arg->levelconf[i].dcoffpercent;
			lvl_conf->prio = arg->levelconf[i].priority;

			if (test_bit(WMI_TLV_SERVICE_THERM_THROT_POUT_REDUCTION,
				     ar->ab->wmi_ab.svc_map))
				lvl_conf->pout_reduction_25db =
					arg->levelconf[i].pout_reduction_db;

			if (test_bit(WMI_SERVICE_THERM_THROT_TX_CHAIN_MASK,
				     ar->ab->wmi_ab.svc_map))
				lvl_conf->tx_chain_mask = arg->levelconf[i].tx_chain_mask;
			lvl_conf->duty_cycle = arg->levelconf[i].duty_cycle;
			lvl_conf++;
		}
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev set thermal throt pdev_id %d enable %d dc %d dc_per_event %x levels %d\n",
		   ar->pdev->pdev_id, arg->enable, arg->dc,
		   arg->dc_per_event,
		   (test_bit(WMI_TLV_SERVICE_THERM_THROT_POUT_REDUCTION, ar->ab->wmi_ab.svc_map) ?
		   ENHANCED_THERMAL_LEVELS : THERMAL_LEVELS));

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_THERM_THROT_SET_CONF_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send THERM_THROT_SET_CONF cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_11d_scan_start_cmd(struct ath12k *ar,
				       struct wmi_11d_scan_start_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_11d_scan_start_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_11d_scan_start_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_11D_SCAN_START_CMD,
				       sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->scan_period_msec = cpu_to_le32(arg->scan_period_msec);
	cmd->start_interval_msec = cpu_to_le32(arg->start_interval_msec);
	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_11D_SCAN_START_CMDID);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "send 11d scan start vdev id %d period %d ms internal %d ms\n",
		   arg->vdev_id, arg->scan_period_msec,
		   arg->start_interval_msec);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_11D_SCAN_START_CMDID: %d\n", ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_11d_scan_stop_cmd(struct ath12k *ar, u32 vdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_11d_scan_stop_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_11d_scan_stop_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_11D_SCAN_STOP_CMD,
				       sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(vdev_id);
	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_11D_SCAN_STOP_CMDID);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "send 11d scan stop vdev id %d\n",
		   cmd->vdev_id);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_11D_SCAN_STOP_CMDID: %d\n", ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath12k_wmi_send_twt_enable_cmd(struct ath12k *ar, u32 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_enable_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_enable_params_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_TWT_ENABLE_CMD,
						 len);
	cmd->pdev_id = cpu_to_le32(pdev_id);
	cmd->sta_cong_timer_ms = cpu_to_le32(ATH12K_TWT_DEF_STA_CONG_TIMER_MS);
	cmd->default_slot_size = cpu_to_le32(ATH12K_TWT_DEF_DEFAULT_SLOT_SIZE);
	cmd->congestion_thresh_setup =
		cpu_to_le32(ATH12K_TWT_DEF_CONGESTION_THRESH_SETUP);
	cmd->congestion_thresh_teardown =
		cpu_to_le32(ATH12K_TWT_DEF_CONGESTION_THRESH_TEARDOWN);
	cmd->congestion_thresh_critical =
		cpu_to_le32(ATH12K_TWT_DEF_CONGESTION_THRESH_CRITICAL);
	cmd->interference_thresh_teardown =
		cpu_to_le32(ATH12K_TWT_DEF_INTERFERENCE_THRESH_TEARDOWN);
	cmd->interference_thresh_setup =
		cpu_to_le32(ATH12K_TWT_DEF_INTERFERENCE_THRESH_SETUP);
	cmd->min_no_sta_setup = cpu_to_le32(ATH12K_TWT_DEF_MIN_NO_STA_SETUP);
	cmd->min_no_sta_teardown = cpu_to_le32(ATH12K_TWT_DEF_MIN_NO_STA_TEARDOWN);
	cmd->no_of_bcast_mcast_slots =
		cpu_to_le32(ATH12K_TWT_DEF_NO_OF_BCAST_MCAST_SLOTS);
	cmd->min_no_twt_slots = cpu_to_le32(ATH12K_TWT_DEF_MIN_NO_TWT_SLOTS);
	cmd->max_no_sta_twt = cpu_to_le32(ATH12K_TWT_DEF_MAX_NO_STA_TWT);
	cmd->mode_check_interval = cpu_to_le32(ATH12K_TWT_DEF_MODE_CHECK_INTERVAL);
	cmd->add_sta_slot_interval = cpu_to_le32(ATH12K_TWT_DEF_ADD_STA_SLOT_INTERVAL);
	cmd->remove_sta_slot_interval =
		cpu_to_le32(ATH12K_TWT_DEF_REMOVE_STA_SLOT_INTERVAL);
	/* TODO add MBSSID support */
	cmd->mbss_support = 0;

	cmd->flags = cpu_to_le32(ATH12K_WMI_TWT_ENABLE_FLAG_BTWT |
				 ATH12K_WMI_TWT_ENABLE_FLAG_LEGACY_BSSID |
				 ATH12K_WMI_TWT_ENABLE_FLAG_SPLIT_CONFIG |
				 ATH12K_WMI_TWT_ENABLE_FLAG_RESPONDER |
				 ATH12K_WMI_TWT_ENABLE_FLAG_ITWT_BTWT |
				 ATH12K_WMI_TWT_ENABLE_FLAG_BTWT_RTWT);

	if (cmd->mbss_support)
		cmd->flags |= cpu_to_le32(ATH12K_WMI_TWT_ENABLE_FLAG_11AX_BSSID);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_TWT_ENABLE_CMDID);
	if (ret) {
		ath12k_warn(ab, "Failed to send WMI_TWT_ENABLE_CMDID");
		dev_kfree_skb(skb);
	} else {
		ar->twt_enabled = 1;
	}
	return ret;
}

int
ath12k_wmi_send_twt_disable_cmd(struct ath12k *ar, u32 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_disable_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_disable_params_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_TWT_DISABLE_CMD,
						 len);
	cmd->pdev_id = cpu_to_le32(pdev_id);
	cmd->flags = cpu_to_le32(ATH12K_WMI_TWT_ENABLE_FLAG_SPLIT_CONFIG |
				 ATH12K_WMI_TWT_ENABLE_FLAG_RESPONDER |
				 ATH12K_WMI_TWT_ENABLE_FLAG_ITWT_BTWT |
				 ATH12K_WMI_TWT_ENABLE_FLAG_BTWT_RTWT);
	cmd->reason_code = 0;

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_TWT_DISABLE_CMDID);
	if (ret) {
		ath12k_warn(ab, "Failed to send WMI_TWT_DISABLE_CMDID");
		dev_kfree_skb(skb);
	} else {
		ar->twt_enabled = 0;
	}
	return ret;
}

int ath12k_wmi_send_twt_add_dialog_cmd(struct ath12k *ar,
				       struct wmi_twt_add_dialog_params *params)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_add_dialog_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
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
	cmd->b_twt_persistence = params->b_twt_persistence;
	cmd->b_twt_recommendation = params->b_twt_recommendation;
	cmd->min_wake_intvl_us = params->min_wake_intvl_us;
	cmd->max_wake_intvl_us = params->max_wake_intvl_us;
	cmd->min_wake_dura_us = params->min_wake_dura_us;
	cmd->max_wake_dura_us = params->max_wake_dura_us;
	cmd->sp_start_tsf_lo = (uint32_t)(params->wake_time_tsf & 0xFFFFFFFF);
	cmd->sp_start_tsf_hi = (uint32_t)(params->wake_time_tsf >> 32);
	cmd->announce_timeout_us = params->announce_timeout_us;
	cmd->link_id_bitmap = params->link_id_bitmap;
	cmd->r_twt_dl_tid_bitmap = params->r_twt_dl_tid_bitmap;
	cmd->r_twt_ul_tid_bitmap = params->r_twt_ul_tid_bitmap;

	cmd->flags = params->twt_cmd;
	if (params->flag_bcast)
		cmd->flags |= WMI_TWT_ADD_DIALOG_FLAG_BCAST;
	if (params->flag_trigger)
		cmd->flags |= WMI_TWT_ADD_DIALOG_FLAG_TRIGGER;
	if (params->flag_flow_type)
		cmd->flags |= WMI_TWT_ADD_DIALOG_FLAG_FLOW_TYPE;
	if (params->flag_protection)
		cmd->flags |= WMI_TWT_ADD_DIALOG_FLAG_PROTECTION;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi add twt dialog vdev %u dialog id %u wake interval %u mantissa %u wake duration %u service period offset %u flags 0x%x\n",
		   cmd->vdev_id, cmd->dialog_id, cmd->wake_intvl_us,
		   cmd->wake_intvl_mantis, cmd->wake_dura_us, cmd->sp_offset_us,
		   cmd->flags);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "B-TWT persistence %u B-TWT recommendation %u\n",
		   cmd->b_twt_persistence, cmd->b_twt_recommendation);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "Wake intveral: Max %u Min %u Wake duration: Max %u Min %u\n",
		   cmd->min_wake_intvl_us, cmd->max_wake_intvl_us,
		   cmd->min_wake_dura_us, cmd->min_wake_dura_us);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "SP start tsf lo %u hi %u announcement us %u\n",
		   cmd->sp_start_tsf_lo, cmd->sp_start_tsf_hi,
		   cmd->announce_timeout_us);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "RTWT link id bitmap %u dl %u ul %u\n",
		   cmd->link_id_bitmap, cmd->r_twt_dl_tid_bitmap,
		   cmd->r_twt_ul_tid_bitmap);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_TWT_ADD_DIALOG_CMDID);

	if (ret) {
		ath12k_warn(ab,
			    "failed to send wmi command to add twt dialog: %d",
			    ret);
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_send_twt_del_dialog_cmd(struct ath12k *ar,
				       struct wmi_twt_del_dialog_params *params)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_del_dialog_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_del_dialog_params_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_TWT_DEL_DIALOG_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);

	cmd->vdev_id = params->vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, params->peer_macaddr);
	cmd->dialog_id = params->dialog_id;
	cmd->b_twt_persistence = params->b_twt_persistence;
	cmd->is_bcast_twt = params->is_bcast_twt;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi delete twt dialog vdev %u dialog id %u\n",
		   cmd->vdev_id, cmd->dialog_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_TWT_DEL_DIALOG_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "failed to send wmi command to delete twt dialog: %d",
			    ret);
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_send_twt_pause_dialog_cmd(struct ath12k *ar,
					 struct wmi_twt_pause_dialog_params *params)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_pause_dialog_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_pause_dialog_params_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_TWT_PAUSE_DIALOG_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);

	cmd->vdev_id = params->vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, params->peer_macaddr);
	cmd->dialog_id = params->dialog_id;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi pause twt dialog vdev %u dialog id %u\n",
		   cmd->vdev_id, cmd->dialog_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_TWT_PAUSE_DIALOG_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "failed to send wmi command to pause twt dialog: %d",
			    ret);
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_send_twt_resume_dialog_cmd(struct ath12k *ar,
					  struct wmi_twt_resume_dialog_params *params)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_resume_dialog_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
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

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi resume twt dialog vdev %u dialog id %u service period offset %u next twt subfield size %u\n",
		   cmd->vdev_id, cmd->dialog_id, cmd->sp_offset_us,
		   cmd->next_twt_size);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_TWT_RESUME_DIALOG_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "failed to send wmi command to resume twt dialog: %d",
			    ret);
		dev_kfree_skb(skb);
	}
	return ret;
}

int
ath12k_wmi_send_twt_vdev_cfg_cmd(struct ath12k *ar, u32 vdev_id,
				 u32 val)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_vdev_config_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_vdev_config_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_TWT_VDEV_CONFIG_CMD,
						 len);

	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->twt_support = cpu_to_le32(val);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "send twt vdev config %d %d %x\n",
		   cmd->pdev_id, cmd->vdev_id, cmd->twt_support);
	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_TWT_VDEV_CONFIG_CMDID);
	if (ret) {
		ath12k_warn(ab, "Failed to send WMI_TWT_VDEV_CONFIG_CMDID");
		dev_kfree_skb(skb);
	}
	return ret;
}

int
ath12k_wmi_send_obss_spr_cmd(struct ath12k *ar, u32 vdev_id,
			     struct ieee80211_he_obss_pd *he_obss_pd)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_obss_spatial_reuse_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_obss_spatial_reuse_params_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_OBSS_SPATIAL_REUSE_SET_CMD,
						 len);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->enable = cpu_to_le32(he_obss_pd->enable);
	cmd->obss_min = a_cpu_to_sle32(he_obss_pd->min_offset);
	cmd->obss_max = a_cpu_to_sle32(he_obss_pd->max_offset);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_OBSS_PD_SPATIAL_REUSE_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "Failed to send WMI_PDEV_OBSS_PD_SPATIAL_REUSE_CMDID");
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_obss_color_cfg_cmd(struct ath12k *ar, u32 vdev_id,
				  u8 bss_color, u32 period,
				  bool enable)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_obss_color_collision_cfg_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_obss_color_collision_cfg_params_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_OBSS_COLOR_COLLISION_DET_CONFIG,
						 len);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->evt_type = enable ? cpu_to_le32(ATH12K_OBSS_COLOR_COLLISION_DETECTION) :
		cpu_to_le32(ATH12K_OBSS_COLOR_COLLISION_DETECTION_DISABLE);
	cmd->current_bss_color = cpu_to_le32(bss_color);
	cmd->detection_period_ms = cpu_to_le32(period);
	cmd->scan_period_ms = cpu_to_le32(ATH12K_BSS_COLOR_COLLISION_SCAN_PERIOD_MS);
	cmd->free_slot_expiry_time_ms = 0;
	cmd->flags = 0;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi_send_obss_color_collision_cfg id %d type %d bss_color %d detect_period %d scan_period %d\n",
		   cmd->vdev_id, cmd->evt_type, cmd->current_bss_color,
		   cmd->detection_period_ms, cmd->scan_period_ms);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_OBSS_COLOR_COLLISION_DET_CONFIG_CMDID);
	if (ret) {
		ath12k_warn(ab, "Failed to send WMI_OBSS_COLOR_COLLISION_DET_CONFIG_CMDID");
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_pdev_set_srg_bss_color_bitmap(struct ath12k *ar, u32 *bitmap)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_pdev_obss_pd_bitmap_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_obss_pd_bitmap_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PDEV_SRG_BSS_COLOR_BITMAP_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	memcpy(cmd->bitmap, bitmap, sizeof(cmd->bitmap));

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "obss pd pdev_id %d bss color bitmap %08x %08x\n",
		   cmd->pdev_id, cmd->bitmap[0], cmd->bitmap[1]);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SET_SRG_BSS_COLOR_BITMAP_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "Failed to send WMI_PDEV_SET_SRG_BSS_COLOR_BITMAP_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath12k_wmi_pdev_set_srg_patial_bssid_bitmap(struct ath12k *ar, u32 *bitmap)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_pdev_obss_pd_bitmap_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_obss_pd_bitmap_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_PDEV_SRG_PARTIAL_BSSID_BITMAP_CMD) |
		FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	memcpy(cmd->bitmap, bitmap, sizeof(cmd->bitmap));

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "obss pd pdev_id %d partial bssid bitmap %08x %08x\n",
		   cmd->pdev_id, cmd->bitmap[0], cmd->bitmap[1]);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SET_SRG_PARTIAL_BSSID_BITMAP_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "Failed to send WMI_PDEV_SET_SRG_PARTIAL_BSSID_BITMAP_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath12k_wmi_pdev_srg_obss_color_enable_bitmap(struct ath12k *ar, u32 *bitmap)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_pdev_obss_pd_bitmap_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_obss_pd_bitmap_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_PDEV_SRG_OBSS_COLOR_ENABLE_BITMAP_CMD) |
		FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	memcpy(cmd->bitmap, bitmap, sizeof(cmd->bitmap));

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "obss pd srg pdev_id %d bss color enable bitmap %08x %08x\n",
		   cmd->pdev_id, cmd->bitmap[0], cmd->bitmap[1]);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SET_SRG_OBSS_COLOR_ENABLE_BITMAP_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "Failed to send WMI_PDEV_SET_SRG_OBSS_COLOR_ENABLE_BITMAP_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath12k_wmi_pdev_srg_obss_bssid_enable_bitmap(struct ath12k *ar, u32 *bitmap)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_pdev_obss_pd_bitmap_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_obss_pd_bitmap_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_PDEV_SRG_OBSS_BSSID_ENABLE_BITMAP_CMD) |
		FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	memcpy(cmd->bitmap, bitmap, sizeof(cmd->bitmap));

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "obss pd srg pdev_id %d bssid enable bitmap %08x %08x\n",
		   cmd->pdev_id, cmd->bitmap[0], cmd->bitmap[1]);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SET_SRG_OBSS_BSSID_ENABLE_BITMAP_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "Failed to send WMI_PDEV_SET_SRG_OBSS_BSSID_ENABLE_BITMAP_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath12k_wmi_pdev_non_srg_obss_color_enable_bitmap(struct ath12k *ar, u32 *bitmap)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_pdev_obss_pd_bitmap_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_obss_pd_bitmap_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_PDEV_NON_SRG_OBSS_COLOR_ENABLE_BITMAP_CMD) |
		FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	memcpy(cmd->bitmap, bitmap, sizeof(cmd->bitmap));

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "obss pd non_srg pdev_id %d bss color enable bitmap %08x %08x\n",
		   cmd->pdev_id, cmd->bitmap[0], cmd->bitmap[1]);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SET_NON_SRG_OBSS_COLOR_ENABLE_BITMAP_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "Failed to send WMI_PDEV_SET_NON_SRG_OBSS_COLOR_ENABLE_BITMAP_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath12k_wmi_pdev_non_srg_obss_bssid_enable_bitmap(struct ath12k *ar, u32 *bitmap)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_pdev_obss_pd_bitmap_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_obss_pd_bitmap_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_PDEV_NON_SRG_OBSS_BSSID_ENABLE_BITMAP_CMD) |
		FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	memcpy(cmd->bitmap, bitmap, sizeof(cmd->bitmap));

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "obss pd non_srg pdev_id %d bssid enable bitmap %08x %08x\n",
		   cmd->pdev_id, cmd->bitmap[0], cmd->bitmap[1]);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SET_NON_SRG_OBSS_BSSID_ENABLE_BITMAP_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "Failed to send WMI_PDEV_SET_NON_SRG_OBSS_BSSID_ENABLE_BITMAP_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_bss_color_change_enable_cmd(struct ath12k *ar, u32 vdev_id,
						bool enable)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_bss_color_change_enable_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_bss_color_change_enable_params_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_BSS_COLOR_CHANGE_ENABLE,
						 len);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->enable = enable ? cpu_to_le32(1) : 0;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi_send_bss_color_change_enable id %d enable %d\n",
		   cmd->vdev_id, cmd->enable);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_BSS_COLOR_CHANGE_ENABLE_CMDID);
	if (ret) {
		ath12k_warn(ab, "Failed to send WMI_BSS_COLOR_CHANGE_ENABLE_CMDID");
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_fils_discovery_tmpl(struct ath12k *ar, u32 vdev_id,
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

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev %i set FILS discovery template\n", vdev_id);

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_fils_discovery_tmpl_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_FILS_DISCOVERY_TMPL_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->buf_len = cpu_to_le32(tmpl->len);
	ptr = skb->data + sizeof(*cmd);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, aligned_len);
	memcpy(tlv->value, tmpl->data, tmpl->len);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_FILS_DISCOVERY_TMPL_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "WMI vdev %i failed to send FILS discovery template command\n",
			    vdev_id);
		dev_kfree_skb(skb);
	}
	return ret;
}

static void *
ath12k_wmi_append_prb_resp_cu_params(struct ath12k *ar, u32 vdev_id, void *ptr)
{
	struct wmi_prb_resp_tmpl_ml_info_params *ml_info;
	struct ath12k_prb_resp_tmpl_ml_info *ar_ml_info;
	void *start = ptr;
	struct wmi_tlv *tlv;
	struct ath12k_link_vif *arvif = ath12k_mac_get_arvif(ar, vdev_id);
	size_t ml_info_len = sizeof(*ml_info);

	if (!arvif)
		return ptr;

	/* Add ML info */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, ml_info_len);
	ml_info = (struct wmi_prb_resp_tmpl_ml_info_params *)tlv->value;

	ar_ml_info = &arvif->ml_info;
	ml_info->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PRB_RESP_TMPL_ML_INFO_CMD,
						     sizeof(*ml_info));
	ml_info->hw_link_id = cpu_to_le32(ar_ml_info->hw_link_id);
	ml_info->cu_vdev_map_cat1_lo = cpu_to_le32(ar_ml_info->cu_vdev_map_cat1_lo);
	ml_info->cu_vdev_map_cat1_hi = cpu_to_le32(ar_ml_info->cu_vdev_map_cat1_hi);
	ml_info->cu_vdev_map_cat2_lo = cpu_to_le32(ar_ml_info->cu_vdev_map_cat2_lo);
	ml_info->cu_vdev_map_cat2_hi = cpu_to_le32(ar_ml_info->cu_vdev_map_cat2_hi);

	ptr += TLV_HDR_SIZE + sizeof(*ml_info);
	/* Reset CU bitmap and bpcc values*/
	memset(&arvif->ml_info, 0, sizeof(arvif->ml_info));
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi %ld bytes of additional data filled for prb resp CU\n",
		   (unsigned long)(ptr - start));
	return ptr;
}

int ath12k_wmi_peer_set_cfr_capture_conf(struct ath12k *ar,
					 u32 vdev_id, const u8 *mac_addr,
					 struct wmi_peer_cfr_capture_conf_arg *arg)
{
	struct wmi_peer_cfr_capture_cmd_fixed_param *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
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

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_PEER_CFR_CAPTURE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "WMI vdev %d failed to send peer cfr capture cmd\n",
			    vdev_id);
		dev_kfree_skb(skb);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev %i set cfr capture conf cmd->request=%d cmd->vdev_id=%d cmd->periodicity=%d cmd->bandwidth=%d cmd->capture_method=%d",
		   vdev_id, cmd->request, cmd->vdev_id, cmd->periodicity,
	           cmd->bandwidth, cmd->capture_method);
	return ret;
}

int ath12k_wmi_probe_resp_tmpl(struct ath12k *ar,
			       struct ath12k_link_vif *arvif,
			       struct sk_buff *tmpl)
{
	struct wmi_probe_tmpl_cmd *cmd;
	struct ath12k_wmi_bcn_prb_info_params *probe_info;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int ret, len, mlinfo_tlv_len = 0;
	size_t aligned_len = roundup(tmpl->len, 4);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev %i set probe response template\n", arvif->vdev_id);

	if (ath12k_mac_is_ml_arvif(arvif))
		mlinfo_tlv_len = TLV_HDR_SIZE + sizeof(struct wmi_prb_resp_tmpl_ml_info_params);

	len = sizeof(*cmd) + sizeof(*probe_info) + TLV_HDR_SIZE + aligned_len + mlinfo_tlv_len;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb) {
		memset(&arvif->ml_info, 0, sizeof(arvif->ml_info));
		return -ENOMEM;
	}
	cmd = (struct wmi_probe_tmpl_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PRB_TMPL_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arvif->vdev_id);
	cmd->buf_len = cpu_to_le32(tmpl->len);

	ptr = skb->data + sizeof(*cmd);

	probe_info = ptr;
	len = sizeof(*probe_info);
	probe_info->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_BCN_PRB_INFO,
							len);
	probe_info->caps = 0;
	probe_info->erp = 0;

	ptr += sizeof(*probe_info);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, aligned_len);
	memcpy(tlv->value, tmpl->data, tmpl->len);
	ptr += (TLV_HDR_SIZE + aligned_len);

	if (ath12k_mac_is_ml_arvif(arvif))
		ptr = ath12k_wmi_append_prb_resp_cu_params(ar, arvif->vdev_id, ptr);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_PRB_TMPL_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "WMI vdev %i failed to send probe response template command\n",
			    arvif->vdev_id);
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_fils_discovery(struct ath12k *ar, u32 vdev_id, u32 interval,
			      bool unsol_bcast_probe_resp_enabled)
{
	struct sk_buff *skb;
	int ret, len;
	struct wmi_fils_discovery_cmd *cmd;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev %i set %s interval to %u TU\n",
		   vdev_id, unsol_bcast_probe_resp_enabled ?
		   "unsolicited broadcast probe response" : "FILS discovery",
		   interval);

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_fils_discovery_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ENABLE_FILS_CMD,
						 len);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->interval = cpu_to_le32(interval);
	cmd->config = cpu_to_le32(unsol_bcast_probe_resp_enabled);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_ENABLE_FILS_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "WMI vdev %i failed to send FILS discovery enable/disable command\n",
			    vdev_id);
		dev_kfree_skb(skb);
	}
	return ret;
}

static void
ath12k_fill_band_to_mac_param(struct ath12k_base  *soc,
			      struct ath12k_wmi_pdev_band_arg *arg)
{
	u8 i;
	struct ath12k_wmi_hal_reg_capabilities_ext_arg *hal_reg_cap;
	struct ath12k_pdev *pdev;

	for (i = 0; i < soc->num_radios; i++) {
		pdev = &soc->pdevs[i];
		hal_reg_cap = &soc->hal_reg_cap[i];
		arg[i].pdev_id = pdev->pdev_id;

		switch (pdev->cap.supported_bands) {
		case WMI_HOST_WLAN_2GHZ_5GHZ_CAP:
			arg[i].start_freq = hal_reg_cap->low_2ghz_chan;
			arg[i].end_freq = hal_reg_cap->high_5ghz_chan;
			break;
		case WMI_HOST_WLAN_2GHZ_CAP:
			arg[i].start_freq = hal_reg_cap->low_2ghz_chan;
			arg[i].end_freq = hal_reg_cap->high_2ghz_chan;
			pdev->phy_name = ATH12K_PHY_2GHZ;
			break;
		case WMI_HOST_WLAN_5GHZ_CAP:
			arg[i].start_freq = hal_reg_cap->low_5ghz_chan;
			arg[i].end_freq = hal_reg_cap->high_5ghz_chan;
			if (hal_reg_cap->high_5ghz_chan <= ATH12K_MIN_5G_HIGH_BAND_FREQ)
				pdev->phy_name = ATH12K_PHY_5GHZ_LOW;
			else if (hal_reg_cap->low_5ghz_chan >= ATH12K_MAX_5G_LOW_BAND_FREQ &&
				 hal_reg_cap->high_5ghz_chan <= ATH12K_MIN_6GHZ_FREQ)
				pdev->phy_name = ATH12K_PHY_5GHZ_HIGH;
			else if (hal_reg_cap->low_5ghz_chan >= ATH12K_MIN_6GHZ_FREQ &&
				 hal_reg_cap->high_5ghz_chan <= ATH12K_MAX_6GHZ_FREQ)
				pdev->phy_name = ATH12K_PHY_6GHZ;
			else
				pdev->phy_name = ATH12K_PHY_5GHZ;
			break;
		default:
			break;
		}
	}
}

static void
ath12k_wmi_copy_resource_config(struct ath12k_base *ab,
				struct ath12k_wmi_resource_config_params *wmi_cfg,
				struct ath12k_wmi_resource_config_arg *tg_cfg)
{
	wmi_cfg->num_vdevs = cpu_to_le32(tg_cfg->num_vdevs);
	wmi_cfg->num_peers = cpu_to_le32(tg_cfg->num_peers);
	wmi_cfg->num_offload_peers = cpu_to_le32(tg_cfg->num_offload_peers);
	wmi_cfg->num_offload_reorder_buffs =
		cpu_to_le32(tg_cfg->num_offload_reorder_buffs);
	wmi_cfg->num_peer_keys = cpu_to_le32(tg_cfg->num_peer_keys);
	wmi_cfg->num_tids = cpu_to_le32(tg_cfg->num_tids);
	wmi_cfg->ast_skid_limit = cpu_to_le32(tg_cfg->ast_skid_limit);
	wmi_cfg->tx_chain_mask = cpu_to_le32(tg_cfg->tx_chain_mask);
	wmi_cfg->rx_chain_mask = cpu_to_le32(tg_cfg->rx_chain_mask);
	wmi_cfg->rx_timeout_pri[0] = cpu_to_le32(tg_cfg->rx_timeout_pri[0]);
	wmi_cfg->rx_timeout_pri[1] = cpu_to_le32(tg_cfg->rx_timeout_pri[1]);
	wmi_cfg->rx_timeout_pri[2] = cpu_to_le32(tg_cfg->rx_timeout_pri[2]);
	wmi_cfg->rx_timeout_pri[3] = cpu_to_le32(tg_cfg->rx_timeout_pri[3]);
	wmi_cfg->rx_decap_mode = cpu_to_le32(tg_cfg->rx_decap_mode);
	wmi_cfg->scan_max_pending_req = cpu_to_le32(tg_cfg->scan_max_pending_req);
	wmi_cfg->bmiss_offload_max_vdev = cpu_to_le32(tg_cfg->bmiss_offload_max_vdev);
	wmi_cfg->roam_offload_max_vdev = cpu_to_le32(tg_cfg->roam_offload_max_vdev);
	wmi_cfg->roam_offload_max_ap_profiles =
		cpu_to_le32(tg_cfg->roam_offload_max_ap_profiles);
	wmi_cfg->num_mcast_groups = cpu_to_le32(tg_cfg->num_mcast_groups);
	wmi_cfg->num_mcast_table_elems = cpu_to_le32(tg_cfg->num_mcast_table_elems);
	wmi_cfg->mcast2ucast_mode = cpu_to_le32(tg_cfg->mcast2ucast_mode);
	wmi_cfg->tx_dbg_log_size = cpu_to_le32(tg_cfg->tx_dbg_log_size);
	wmi_cfg->num_wds_entries = cpu_to_le32(tg_cfg->num_wds_entries);
	wmi_cfg->dma_burst_size = cpu_to_le32(tg_cfg->dma_burst_size);
	wmi_cfg->mac_aggr_delim = cpu_to_le32(tg_cfg->mac_aggr_delim);
	wmi_cfg->rx_skip_defrag_timeout_dup_detection_check =
		cpu_to_le32(tg_cfg->rx_skip_defrag_timeout_dup_detection_check);
	wmi_cfg->vow_config = cpu_to_le32(tg_cfg->vow_config);
	wmi_cfg->gtk_offload_max_vdev = cpu_to_le32(tg_cfg->gtk_offload_max_vdev);
	wmi_cfg->num_msdu_desc = cpu_to_le32(tg_cfg->num_msdu_desc);
	wmi_cfg->max_frag_entries = cpu_to_le32(tg_cfg->max_frag_entries);
	wmi_cfg->num_tdls_vdevs = cpu_to_le32(tg_cfg->num_tdls_vdevs);
	wmi_cfg->num_tdls_conn_table_entries =
		cpu_to_le32(tg_cfg->num_tdls_conn_table_entries);
	wmi_cfg->beacon_tx_offload_max_vdev =
		cpu_to_le32(tg_cfg->beacon_tx_offload_max_vdev);
	wmi_cfg->num_multicast_filter_entries =
		cpu_to_le32(tg_cfg->num_multicast_filter_entries);
	wmi_cfg->num_wow_filters = cpu_to_le32(tg_cfg->num_wow_filters);
	wmi_cfg->num_keep_alive_pattern = cpu_to_le32(tg_cfg->num_keep_alive_pattern);
	wmi_cfg->keep_alive_pattern_size = cpu_to_le32(tg_cfg->keep_alive_pattern_size);
	wmi_cfg->max_tdls_concurrent_sleep_sta =
		cpu_to_le32(tg_cfg->max_tdls_concurrent_sleep_sta);
	wmi_cfg->max_tdls_concurrent_buffer_sta =
		cpu_to_le32(tg_cfg->max_tdls_concurrent_buffer_sta);
	wmi_cfg->wmi_send_separate = cpu_to_le32(tg_cfg->wmi_send_separate);
	wmi_cfg->num_ocb_vdevs = cpu_to_le32(tg_cfg->num_ocb_vdevs);
	wmi_cfg->num_ocb_channels = cpu_to_le32(tg_cfg->num_ocb_channels);
	wmi_cfg->num_ocb_schedules = cpu_to_le32(tg_cfg->num_ocb_schedules);
	wmi_cfg->bpf_instruction_size = cpu_to_le32(tg_cfg->bpf_instruction_size);
	wmi_cfg->max_bssid_rx_filters = cpu_to_le32(tg_cfg->max_bssid_rx_filters);
	wmi_cfg->use_pdev_id = cpu_to_le32(tg_cfg->use_pdev_id);
	wmi_cfg->flag1 = cpu_to_le32(tg_cfg->atf_config |
				     WMI_RSRC_CFG_FLAG1_BSS_CHANNEL_INFO_64);
	if (tg_cfg->carrier_config)
		wmi_cfg->carrier_config = cpu_to_le32(tg_cfg->carrier_config);

	ath12k_dbg(ab, ATH12K_DBG_WMI, "ATF: carrier_config %d",
		   tg_cfg->carrier_config);

	if (tg_cfg->carrier_vow_optimization)
		wmi_cfg->flag1 |= WMI_RSRC_CFG_FLAG1_VIDEO_OVER_WIFI_ENABLE;
	wmi_cfg->peer_map_unmap_version = cpu_to_le32(tg_cfg->peer_map_unmap_version);
	wmi_cfg->sched_params = cpu_to_le32(tg_cfg->sched_params);
	wmi_cfg->twt_ap_pdev_count = cpu_to_le32(tg_cfg->twt_ap_pdev_count);
	wmi_cfg->twt_ap_sta_count = cpu_to_le32(tg_cfg->twt_ap_sta_count);
	wmi_cfg->flags2 = le32_encode_bits(tg_cfg->peer_metadata_ver,
					   WMI_RSRC_CFG_FLAGS2_RX_PEER_METADATA_VERSION);

	wmi_cfg->host_service_flags = cpu_to_le32(tg_cfg->afc_support <<
						   WMI_RSRC_CFG_HOST_SUPPORT_LP_SP_MODE_BIT);
	wmi_cfg->host_service_flags |= cpu_to_le32(tg_cfg->afc_disable_timer_check <<
						   WMI_RSRC_CFG_HOST_AFC_DIS_TIMER_CHECK_BIT);
	wmi_cfg->host_service_flags |= cpu_to_le32(tg_cfg->afc_disable_req_id_check <<
						   WMI_RSRC_CFG_HOST_AFC_DIS_REQ_ID_CHECK_BIT);
	wmi_cfg->host_service_flags |= cpu_to_le32(tg_cfg->afc_indoor_support <<
						   WMI_RSRC_CFG_HOST_AFC_INDOOR_SUPPORT);
	wmi_cfg->host_service_flags |= cpu_to_le32(tg_cfg->afc_outdoor_support <<
						   WMI_RSRC_CFG_HOST_AFC_OUTDOOR_SUPPORT);

	ath12k_dbg(NULL, ATH12K_DBG_WMI, "afc_support: %u "
		   "dis_timer_check: %u, dis_reqid_check: %u, indoor: %u, outdoor: %u\n",
		   tg_cfg->afc_support, tg_cfg->afc_disable_timer_check,
		   tg_cfg->afc_disable_req_id_check, tg_cfg->afc_indoor_support,
		   tg_cfg->afc_outdoor_support);

	wmi_cfg->host_service_flags |= cpu_to_le32(tg_cfg->is_reg_cc_ext_event_supported <<
				WMI_RSRC_CFG_HOST_SVC_FLAG_REG_CC_EXT_SUPPORT_BIT);
	if (ab->hw_params->reoq_lut_support)
		wmi_cfg->host_service_flags |=
			cpu_to_le32(1 << WMI_RSRC_CFG_HOST_SVC_FLAG_REO_QREF_SUPPORT_BIT);
	wmi_cfg->host_service_flags |=
			cpu_to_le32(1 << WMI_RSRC_CFG_HOST_SVC_FLAG_FULL_BW_NOL_SUPPORT_BIT);
	if (!ath12k_afc_reg_no_action) {
		wmi_cfg->host_service_flags |=
			cpu_to_le32(1 << WMI_RSRC_CFG_HOST_AFC_TRIGGER_ON_DEFAULT_CC_EVENT_BIT);
	}
	if (tg_cfg->def_flow_override)
		wmi_cfg->host_service_flags |=
			cpu_to_le32(1 << WMI_RSRC_CFG_HOST_SVC_FLAG_DEF_FLOW_OVERRIDE_SET_BIT);
	wmi_cfg->ema_max_vap_cnt = cpu_to_le32(tg_cfg->ema_max_vap_cnt);
	wmi_cfg->ema_max_profile_period = cpu_to_le32(tg_cfg->ema_max_profile_period);
	wmi_cfg->flags2 |= cpu_to_le32(WMI_RSRC_CFG_FLAGS2_CALC_NEXT_DTIM_COUNT_SET);
	if (ath12k_frame_mode != ATH12K_HW_TXRX_ETHERNET)
		wmi_cfg->flags2 |= WMI_RSRC_CFG_FLAGS2_INTRABSS_MEC_WDS_LEARNING_DISABLE;
	else
		wmi_cfg->flags2 |= WMI_RSRC_CFG_FLAGS2_FW_AST_INDICATION_DISABLE;

	if (tg_cfg->is_wds_null_frame_supported)
		wmi_cfg->flags2 |= WMI_RSRC_CFG_FLAGS2_WDS_NULL_FRAME_SUPPORT;

	if (tg_cfg->rep_ul_resp)
		wmi_cfg->flags2 |= cpu_to_le32(tg_cfg->rep_ul_resp);

	wmi_cfg->ema_init_config =
		cpu_to_le32(u32_encode_bits(tg_cfg->max_beacon_size,
					    WMI_RSRC_CFG_EMA_INIT_CONFIG_BEACON_SIZE));
	wmi_cfg->flags2 |= (tg_cfg->qos) ?
			   (WMI_RSRC_CFG_FLAGS2_SAWF_CONFIG_ENABLE_SET) : (0);

}

/**
 * ath12k_set_afc_config() - Set AFC config parameters
 * @config: WMI resource config
 * @ab: ath12k base
 *
 * Return: None
 */
static void ath12k_set_afc_config(struct ath12k_wmi_resource_config_arg *config,
				  struct ath12k_base *ab)
{
	config->afc_indoor_support = false;
	config->afc_outdoor_support = false;

	if (ab->afc_dev_deployment == ATH12K_AFC_DEPLOYMENT_INDOOR)
		config->afc_indoor_support = true;
	else if (ab->afc_dev_deployment == ATH12K_AFC_DEPLOYMENT_OUTDOOR)
		config->afc_outdoor_support = true;
	else if (ab->afc_dev_deployment == ATH12K_AFC_DEPLOYMENT_UNKNOWN)
		config->afc_indoor_support = true;

	config->afc_support = ath12k_6ghz_sp_pwrmode_supp_enabled;
	config->afc_disable_timer_check = ath12k_afc_disable_timer_check;
	config->afc_disable_req_id_check = ath12k_afc_disable_req_id_check;
}

static int ath12k_init_cmd_send(struct ath12k_wmi_pdev *wmi,
				struct ath12k_wmi_init_cmd_arg *arg)
{
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct sk_buff *skb;
	struct wmi_init_cmd *cmd;
	struct ath12k_wmi_resource_config_params *cfg;
	struct ath12k_wmi_pdev_set_hw_mode_cmd *hw_mode;
	struct ath12k_wmi_pdev_band_to_mac_params *band_to_mac;
	struct ath12k_wmi_host_mem_chunk_params *host_mem_chunks;
	struct wmi_tlv *tlv;
	struct device *dev = ab->dev;
	bool three_way_coex_enabled = false;
	size_t ret, len;
	void *ptr;
	u32 hw_mode_len = 0;
	u16 idx;

	if (arg->hw_mode_id != WMI_HOST_HW_MODE_MAX)
		hw_mode_len = sizeof(*hw_mode) + TLV_HDR_SIZE +
			      (arg->num_band_to_mac * sizeof(*band_to_mac));

	len = sizeof(*cmd) + TLV_HDR_SIZE + sizeof(*cfg) + hw_mode_len +
	      (arg->num_mem_chunks ? (sizeof(*host_mem_chunks) * WMI_MAX_MEM_REQS) : 0);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_init_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_INIT_CMD,
						 sizeof(*cmd));

	ptr = skb->data + sizeof(*cmd);
	cfg = ptr;

	three_way_coex_enabled = of_property_read_bool(dev->of_node, "qcom,btcoex");
	if (three_way_coex_enabled)
		cfg->flag1 |= WMI_RSRC_CFG_FLAG1_THREE_WAY_COEX_CONFIG_OVERRIDE_SUPPORT;

	ath12k_wmi_copy_resource_config(ab, cfg, &arg->res_cfg);

	cfg->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_RESOURCE_CONFIG,
						 sizeof(*cfg));

	ptr += sizeof(*cfg);
	host_mem_chunks = ptr + TLV_HDR_SIZE;
	len = sizeof(struct ath12k_wmi_host_mem_chunk_params);

	for (idx = 0; idx < arg->num_mem_chunks; ++idx) {
		host_mem_chunks[idx].tlv_header =
			ath12k_wmi_tlv_hdr(WMI_TAG_WLAN_HOST_MEMORY_CHUNK,
					   len);

		host_mem_chunks[idx].ptr = cpu_to_le32(arg->mem_chunks[idx].paddr);
		host_mem_chunks[idx].size = cpu_to_le32(arg->mem_chunks[idx].len);
		host_mem_chunks[idx].req_id = cpu_to_le32(arg->mem_chunks[idx].req_id);

		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "WMI host mem chunk req_id %d paddr 0x%llx len %d\n",
			   arg->mem_chunks[idx].req_id,
			   (u64)arg->mem_chunks[idx].paddr,
			   arg->mem_chunks[idx].len);
	}
	cmd->num_host_mem_chunks = cpu_to_le32(arg->num_mem_chunks);
	len = sizeof(struct ath12k_wmi_host_mem_chunk_params) * arg->num_mem_chunks;

	/* num_mem_chunks is zero */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE + len;

	if (arg->hw_mode_id != WMI_HOST_HW_MODE_MAX) {
		hw_mode = (struct ath12k_wmi_pdev_set_hw_mode_cmd *)ptr;
		hw_mode->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_HW_MODE_CMD,
							     sizeof(*hw_mode));

		hw_mode->hw_mode_index = cpu_to_le32(arg->hw_mode_id);
		hw_mode->num_band_to_mac = cpu_to_le32(arg->num_band_to_mac);

		ptr += sizeof(*hw_mode);

		len = arg->num_band_to_mac * sizeof(*band_to_mac);
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);

		ptr += TLV_HDR_SIZE;
		len = sizeof(*band_to_mac);

		for (idx = 0; idx < arg->num_band_to_mac; idx++) {
			band_to_mac = (void *)ptr;

			band_to_mac->tlv_header =
				ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_BAND_TO_MAC,
						       len);
			band_to_mac->pdev_id = cpu_to_le32(arg->band_to_mac[idx].pdev_id);
			band_to_mac->start_freq =
				cpu_to_le32(arg->band_to_mac[idx].start_freq);
			band_to_mac->end_freq =
				cpu_to_le32(arg->band_to_mac[idx].end_freq);
			ptr += sizeof(*band_to_mac);
		}
	}

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_INIT_CMDID);
	if (ret) {
		ath12k_warn(ab, "failed to send WMI_INIT_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_pdev_lro_cfg(struct ath12k *ar,
			    int pdev_id)
{
	struct ath12k_wmi_pdev_lro_config_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct ath12k_wmi_pdev_lro_config_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_LRO_INFO_CMD,
						 sizeof(*cmd));

	get_random_bytes(cmd->th_4, sizeof(cmd->th_4));
	get_random_bytes(cmd->th_6, sizeof(cmd->th_6));

	cmd->pdev_id = cpu_to_le32(pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI lro cfg cmd pdev_id 0x%x\n", pdev_id);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_LRO_CONFIG_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send lro cfg req wmi cmd\n");
		goto err;
	}

	return 0;
err:
	dev_kfree_skb(skb);
	return ret;
}

int ath12k_wmi_wait_for_service_ready(struct ath12k_base *ab)
{
	unsigned long time_left;

	time_left = wait_for_completion_timeout(&ab->wmi_ab.service_ready,
						WMI_SERVICE_READY_TIMEOUT_HZ);
	if (!time_left)
		return -ETIMEDOUT;

	return 0;
}

int ath12k_wmi_wait_for_unified_ready(struct ath12k_base *ab)
{
	unsigned long time_left;

	time_left = wait_for_completion_timeout(&ab->wmi_ab.unified_ready,
						WMI_SERVICE_READY_TIMEOUT_HZ);
	if (!time_left)
		return -ETIMEDOUT;

	return 0;
}

int ath12k_wmi_set_hw_mode(struct ath12k_base *ab,
			   enum wmi_host_hw_mode_config_type mode)
{
	struct ath12k_wmi_pdev_set_hw_mode_cmd *cmd;
	struct sk_buff *skb;
	struct ath12k_wmi_base *wmi_ab = &ab->wmi_ab;
	int len;
	int ret;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct ath12k_wmi_pdev_set_hw_mode_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_HW_MODE_CMD,
						 sizeof(*cmd));

	cmd->pdev_id = WMI_PDEV_ID_SOC;
	cmd->hw_mode_index = cpu_to_le32(mode);

	ret = ath12k_wmi_cmd_send(&wmi_ab->wmi[0], skb, WMI_PDEV_SET_HW_MODE_CMDID);
	if (ret) {
		ath12k_warn(ab, "failed to send WMI_PDEV_SET_HW_MODE_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_cmd_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_wmi_base *wmi_ab = &ab->wmi_ab;
	struct ath12k_wmi_init_cmd_arg arg = {};

	if (test_bit(WMI_TLV_SERVICE_REG_CC_EXT_EVENT_SUPPORT,
		     ab->wmi_ab.svc_map))
		arg.res_cfg.is_reg_cc_ext_event_supported = true;
	if (test_bit(WMI_TLV_SERVICE_RADAR_FLAGS_SUPPORT, ab->wmi_ab.svc_map))
		arg.res_cfg.is_full_bw_nol_feature_supported = true;

	if (test_bit(WMI_SERVICE_WDS_NULL_FRAME_SUPPORT, ab->wmi_ab.svc_map))
		arg.res_cfg.is_wds_null_frame_supported = true;

	if (test_bit(WMI_TLV_SERVICE_AFC_SUPPORT, ab->wmi_ab.svc_map))
		ath12k_set_afc_config(&arg.res_cfg, ab);

	ab->hw_params->wmi_init(ab, &arg.res_cfg);
	ab->wow.wmi_conf_rx_decap_mode = arg.res_cfg.rx_decap_mode;

	arg.num_mem_chunks = wmi_ab->num_mem_chunks;
	arg.hw_mode_id = wmi_ab->preferred_hw_mode;
	arg.mem_chunks = wmi_ab->mem_chunks;

	if (ab->hw_params->single_pdev_only)
		arg.hw_mode_id = WMI_HOST_HW_MODE_MAX;

	arg.num_band_to_mac = ab->num_radios;
	ath12k_fill_band_to_mac_param(ab, arg.band_to_mac);
	ath12k_cfg_parse_pdev_section(ab);

	dp->peer_metadata_ver = arg.res_cfg.peer_metadata_ver;

	return ath12k_init_cmd_send(&wmi_ab->wmi[0], &arg);
}

int ath12k_wmi_vdev_spectral_conf(struct ath12k *ar,
				  struct ath12k_wmi_vdev_spectral_conf_arg *arg)
{
	struct ath12k_wmi_vdev_spectral_conf_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct ath12k_wmi_vdev_spectral_conf_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_SPECTRAL_CONFIGURE_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->scan_count = cpu_to_le32(arg->scan_count);
	cmd->scan_period = cpu_to_le32(arg->scan_period);
	cmd->scan_priority = cpu_to_le32(arg->scan_priority);
	cmd->scan_fft_size = cpu_to_le32(arg->scan_fft_size);
	cmd->scan_gc_ena = cpu_to_le32(arg->scan_gc_ena);
	cmd->scan_restart_ena = cpu_to_le32(arg->scan_restart_ena);
	cmd->scan_noise_floor_ref = cpu_to_le32(arg->scan_noise_floor_ref);
	cmd->scan_init_delay = cpu_to_le32(arg->scan_init_delay);
	cmd->scan_nb_tone_thr = cpu_to_le32(arg->scan_nb_tone_thr);
	cmd->scan_str_bin_thr = cpu_to_le32(arg->scan_str_bin_thr);
	cmd->scan_wb_rpt_mode = cpu_to_le32(arg->scan_wb_rpt_mode);
	cmd->scan_rssi_rpt_mode = cpu_to_le32(arg->scan_rssi_rpt_mode);
	cmd->scan_rssi_thr = cpu_to_le32(arg->scan_rssi_thr);
	cmd->scan_pwr_format = cpu_to_le32(arg->scan_pwr_format);
	cmd->scan_rpt_mode = cpu_to_le32(arg->scan_rpt_mode);
	cmd->scan_bin_scale = cpu_to_le32(arg->scan_bin_scale);
	cmd->scan_dbm_adj = cpu_to_le32(arg->scan_dbm_adj);
	cmd->scan_chn_mask = cpu_to_le32(arg->scan_chn_mask);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI spectral scan config cmd vdev_id 0x%x\n",
		   arg->vdev_id);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb,
				  WMI_VDEV_SPECTRAL_SCAN_CONFIGURE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send spectral scan config wmi cmd\n");
		goto err;
	}

	return 0;
err:
	dev_kfree_skb(skb);
	return ret;
}

int ath12k_wmi_vdev_spectral_enable(struct ath12k *ar, u32 vdev_id,
				    u32 trigger, u32 enable)
{
	struct ath12k_wmi_vdev_spectral_enable_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct ath12k_wmi_vdev_spectral_enable_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_SPECTRAL_ENABLE_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->trigger_cmd = cpu_to_le32(trigger);
	cmd->enable_cmd = cpu_to_le32(enable);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI spectral enable cmd vdev id 0x%x\n",
		   vdev_id);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb,
				  WMI_VDEV_SPECTRAL_SCAN_ENABLE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send spectral enable wmi cmd\n");
		goto err;
	}

	return 0;
err:
	dev_kfree_skb(skb);
	return ret;
}

int ath12k_wmi_pdev_dma_ring_cfg(struct ath12k *ar,
				 struct ath12k_wmi_pdev_dma_ring_cfg_arg *arg)
{
	struct ath12k_wmi_pdev_dma_ring_cfg_req_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct ath12k_wmi_pdev_dma_ring_cfg_req_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_DMA_RING_CFG_REQ,
						 sizeof(*cmd));

	cmd->pdev_id = cpu_to_le32(arg->pdev_id);
	cmd->module_id = cpu_to_le32(arg->module_id);
	cmd->base_paddr_lo = cpu_to_le32(arg->base_paddr_lo);
	cmd->base_paddr_hi = cpu_to_le32(arg->base_paddr_hi);
	cmd->head_idx_paddr_lo = cpu_to_le32(arg->head_idx_paddr_lo);
	cmd->head_idx_paddr_hi = cpu_to_le32(arg->head_idx_paddr_hi);
	cmd->tail_idx_paddr_lo = cpu_to_le32(arg->tail_idx_paddr_lo);
	cmd->tail_idx_paddr_hi = cpu_to_le32(arg->tail_idx_paddr_hi);
	cmd->num_elems = cpu_to_le32(arg->num_elems);
	cmd->buf_size = cpu_to_le32(arg->buf_size);
	cmd->num_resp_per_event = cpu_to_le32(arg->num_resp_per_event);
	cmd->event_timeout_ms = cpu_to_le32(arg->event_timeout_ms);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI DMA ring cfg req cmd pdev_id 0x%x\n",
		   arg->pdev_id);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb,
				  WMI_PDEV_DMA_RING_CFG_REQ_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send dma ring cfg req wmi cmd\n");
		goto err;
	}

	return 0;
err:
	dev_kfree_skb(skb);
	return ret;
}

static int ath12k_wmi_dma_buf_entry_parse(struct ath12k_base *soc,
					  u16 tag, u16 len,
					  const void *ptr, void *data)
{
	struct ath12k_wmi_dma_buf_release_arg *arg = data;

	if (tag != WMI_TAG_DMA_BUF_RELEASE_ENTRY)
		return -EPROTO;

	if (arg->num_buf_entry >= le32_to_cpu(arg->fixed.num_buf_release_entry))
		return -ENOBUFS;

	arg->num_buf_entry++;
	return 0;
}

static int ath12k_wmi_dma_buf_meta_parse(struct ath12k_base *soc,
					 u16 tag, u16 len,
					 const void *ptr, void *data)
{
	struct ath12k_wmi_dma_buf_release_arg *arg = data;

	if (tag != WMI_TAG_DMA_BUF_RELEASE_SPECTRAL_META_DATA)
		return -EPROTO;

	if (arg->num_meta >= le32_to_cpu(arg->fixed.num_meta_data_entry))
		return -ENOBUFS;

	arg->num_meta++;

	return 0;
}

static int ath12k_wmi_dma_buf_parse(struct ath12k_base *ab,
				    u16 tag, u16 len,
				    const void *ptr, void *data)
{
	struct ath12k_wmi_dma_buf_release_arg *arg = data;
	const struct ath12k_wmi_dma_buf_release_fixed_params *fixed;
	u32 pdev_id;
	int ret;

	switch (tag) {
	case WMI_TAG_DMA_BUF_RELEASE:
		fixed = ptr;
		arg->fixed = *fixed;
		pdev_id = DP_HW2SW_MACID(le32_to_cpu(fixed->pdev_id));
		arg->fixed.pdev_id = cpu_to_le32(pdev_id);
		break;
	case WMI_TAG_ARRAY_STRUCT:
		if (!arg->buf_entry_done) {
			arg->num_buf_entry = 0;
			arg->buf_entry = ptr;

			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_dma_buf_entry_parse,
						  arg);
			if (ret) {
				ath12k_warn(ab, "failed to parse dma buf entry tlv %d\n",
					    ret);
				return ret;
			}

			arg->buf_entry_done = true;
		} else if (!arg->meta_data_done) {
			arg->num_meta = 0;
			arg->meta_data = ptr;

			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_dma_buf_meta_parse,
						  arg);
			if (ret) {
				ath12k_warn(ab, "failed to parse dma buf meta tlv %d\n",
					    ret);
				return ret;
			}

			arg->meta_data_done = true;
		}
		break;
	default:
		break;
	}
	return 0;
}

static void ath12k_wmi_pdev_dma_ring_buf_release_event(struct ath12k_base *ab,
						       struct sk_buff *skb)
{
	struct ath12k_wmi_dma_buf_release_arg arg = {};
	struct ath12k_dbring_buf_release_event param;
	int ret;


	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_dma_buf_parse,
				  &arg);
	if (ret) {
		ath12k_warn(ab, "failed to parse dma buf release tlv %d\n", ret);
		return;
	}

	param.fixed = arg.fixed;
	param.buf_entry = arg.buf_entry;
	param.num_buf_entry = arg.num_buf_entry;
	param.meta_data = arg.meta_data;
	param.num_meta = arg.num_meta;

	ret = ath12k_dbring_buffer_release_event(ab, &param);
	if (ret) {
		ath12k_warn(ab, "failed to handle dma buf release event %d\n", ret);
		return;
	}
}

#define make_min_max(max,min) (u32_encode_bits(max, 0xf0) | u32_encode_bits(min, 0xf))

static void
ath12k_wmi_pdev_update_muedca_params_status_event(struct ath12k_base *ab,
						  struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_pdev_update_muedca_event *ev;
	struct ieee80211_mu_edca_param_set *params;
	struct ath12k *ar;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_MUEDCA_PARAMS_CONFIG_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch pdev update muedca params ev");
		goto mem_free;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, ev->pdev_id);
	if (!ar) {
		ath12k_warn(ab, "MU-EDCA parameter change in invalid pdev %d\n",
			    ev->pdev_id);
		goto unlock;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "update MU-EDCA parameters for radio index %u\n",
		   ar->radio_idx);

	params = kzalloc(sizeof(*params), GFP_ATOMIC);
	if (!params) {
		ath12k_warn(ab,
			    "Failed to allocate memory for updated MU-EDCA Parameters");
		goto unlock;
	}

	params->ac_be.aifsn = ev->aifsn[WMI_AC_BE];
	params->ac_be.ecw_min_max = make_min_max(ev->ecwmax[WMI_AC_BE],
						 ev->ecwmin[WMI_AC_BE]);
	params->ac_be.mu_edca_timer = ev->muedca_expiration_time[WMI_AC_BE];

	params->ac_bk.aifsn = ev->aifsn[WMI_AC_BK];
	params->ac_bk.ecw_min_max = make_min_max(ev->ecwmax[WMI_AC_BK],
						 ev->ecwmin[WMI_AC_BK]);
	params->ac_bk.mu_edca_timer = ev->muedca_expiration_time[WMI_AC_BK];

	params->ac_vi.aifsn = ev->aifsn[WMI_AC_VI];
	params->ac_vi.ecw_min_max = make_min_max(ev->ecwmax[WMI_AC_VI],
						 ev->ecwmin[WMI_AC_VI]);
	params->ac_vi.mu_edca_timer = ev->muedca_expiration_time[WMI_AC_VI];

	params->ac_vo.aifsn = ev->aifsn[WMI_AC_VO];
	params->ac_vo.ecw_min_max = make_min_max(ev->ecwmax[WMI_AC_VO],
						 ev->ecwmin[WMI_AC_VO]);
	params->ac_vo.mu_edca_timer = ev->muedca_expiration_time[WMI_AC_VO];

	cfg80211_update_muedca_params_event(ar->ah->hw->wiphy, ar->radio_idx,
					    params, GFP_ATOMIC);

	kfree(params);
unlock:
	rcu_read_unlock();
mem_free:
	kfree(tb);
}


static int ath12k_wmi_tlv_mac_phy_chainmask_caps(struct ath12k_base *soc,
						 u16 len, const void *ptr, void *data)
{
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;
	struct wmi_mac_phy_chainmask_caps *cmask_caps = (struct wmi_mac_phy_chainmask_caps *)ptr;
	struct ath12k_chainmask_table *cmask_table;
	struct ath12k_pdev_cap *pdev_cap;
	u32 tag;
	int i, j;

	if (!svc_rdy_ext->hw_mode_caps)
		return -EINVAL;

	if ((!svc_rdy_ext->arg.num_chainmask_tables) ||
	    (svc_rdy_ext->arg.num_chainmask_tables > ATH12K_MAX_CHAINMASK_TABLES))
		return -EINVAL;

	for (i = 0; i < svc_rdy_ext->arg.num_chainmask_tables; i++) {
		cmask_table = &svc_rdy_ext->arg.chainmask_table[i];

		for (j = 0; j < cmask_table->num_valid_chainmasks; j++) {
			tag = FIELD_GET(WMI_TLV_TAG, cmask_caps->tlv_header);

			if (tag != WMI_TAG_MAC_PHY_CHAINMASK_CAPABILITY)
				return -EPROTO;

			cmask_table->cap_list[j].chainmask = cmask_caps->chainmask;
			cmask_table->cap_list[j].supported_caps = cmask_caps->supported_flags;
			cmask_caps++;
			ath12k_dbg(soc, ATH12K_DBG_WMI,"[id %d] chainmask %x supported_caps %x",
				   cmask_table->table_id, cmask_table->cap_list[j].chainmask,
				   cmask_table->cap_list[j].supported_caps);
		}
	}

	for (i = 0; i < soc->num_radios; i++) {
		pdev_cap = &soc->pdevs[i].cap;
		for (j = 0; j < svc_rdy_ext->n_mac_phy_chainmask_combo; j++) {
			cmask_table = &svc_rdy_ext->arg.chainmask_table[j];
			if (cmask_table->table_id == pdev_cap->chainmask_table_id)
				break;
		}
		for (j = 0; j < cmask_table->num_valid_chainmasks; j++) {
			if (cmask_table->cap_list[j].supported_caps & WMI_SUPPORT_CHAIN_MASK_ADFS)
				pdev_cap->adfs_chain_mask |= (1 << cmask_table->cap_list[j].chainmask);
		}
		ath12k_dbg(soc, ATH12K_DBG_WMI, "updated adfs chain mask %lx for pdev %d",
			   pdev_cap->adfs_chain_mask, i);
	}
	return 0;
}

static void ath12k_wmi_free_chainmask_caps(struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext)
{
	int i;

	if (!svc_rdy_ext->arg.num_chainmask_tables)
		return;

	for (i = 0; i < svc_rdy_ext->arg.num_chainmask_tables; i++) {
		if (!svc_rdy_ext->arg.chainmask_table[i].cap_list)
			continue;
		kfree(svc_rdy_ext->arg.chainmask_table[i].cap_list);
		svc_rdy_ext->arg.chainmask_table[i].cap_list = NULL;
	}
}

static int ath12k_wmi_tlv_mac_phy_chainmask_combo_parse(struct ath12k_base *soc,
							u16 tag, u16 len,
							const void *ptr, void *data)
{
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;
	struct wmi_mac_phy_chainmask_combo *cmask_combo = (struct wmi_mac_phy_chainmask_combo *) ptr;
	u32 i = svc_rdy_ext->n_mac_phy_chainmask_combo;
	struct ath12k_chainmask_table *cmask_table;

	if (tag != WMI_TAG_MAC_PHY_CHAINMASK_COMBO)
		return -EPROTO;

	if (svc_rdy_ext->n_mac_phy_chainmask_combo >= svc_rdy_ext->arg.num_chainmask_tables)
		return -ENOBUFS;

	cmask_table = &svc_rdy_ext->arg.chainmask_table[i];
	cmask_table->table_id = cmask_combo->chainmask_table_id;
	cmask_table->num_valid_chainmasks = cmask_combo->num_valid_chainmask;
	cmask_table->cap_list = kcalloc(cmask_combo->num_valid_chainmask,
					sizeof(struct ath12k_chainmask_caps),
					GFP_ATOMIC);
	if (!svc_rdy_ext->arg.chainmask_table[i].cap_list)
		return -ENOMEM;

	svc_rdy_ext->n_mac_phy_chainmask_combo++;
	return 0;
}

static int ath12k_wmi_hw_mode_caps_parse(struct ath12k_base *soc,
					 u16 tag, u16 len,
					 const void *ptr, void *data)
{
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;
	struct ath12k_wmi_hw_mode_cap_params *hw_mode_cap;
	u32 phy_map = 0;

	if (tag != WMI_TAG_HW_MODE_CAPABILITIES)
		return -EPROTO;

	if (svc_rdy_ext->n_hw_mode_caps >= svc_rdy_ext->arg.num_hw_modes)
		return -ENOBUFS;

	hw_mode_cap = container_of(ptr, struct ath12k_wmi_hw_mode_cap_params,
				   hw_mode_id);
	svc_rdy_ext->n_hw_mode_caps++;

	phy_map = le32_to_cpu(hw_mode_cap->phy_id_map);
	svc_rdy_ext->tot_phy_id += fls(phy_map);

	return 0;
}

static int ath12k_wmi_hw_mode_caps(struct ath12k_base *soc,
				   u16 len, const void *ptr, void *data)
{
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;
	const struct ath12k_wmi_hw_mode_cap_params *hw_mode_caps;
	enum wmi_host_hw_mode_config_type mode, pref;
	u32 i;
	int ret;

	svc_rdy_ext->n_hw_mode_caps = 0;
	svc_rdy_ext->hw_mode_caps = ptr;

	ret = ath12k_wmi_tlv_iter(soc, ptr, len,
				  ath12k_wmi_hw_mode_caps_parse,
				  svc_rdy_ext);
	if (ret) {
		ath12k_warn(soc, "failed to parse tlv %d\n", ret);
		return ret;
	}

	for (i = 0 ; i < svc_rdy_ext->n_hw_mode_caps; i++) {
		hw_mode_caps = &svc_rdy_ext->hw_mode_caps[i];
		mode = le32_to_cpu(hw_mode_caps->hw_mode_id);

		if (mode >= WMI_HOST_HW_MODE_MAX)
			continue;

		pref = soc->wmi_ab.preferred_hw_mode;

		if (ath12k_hw_mode_pri_map[mode] < ath12k_hw_mode_pri_map[pref]) {
			svc_rdy_ext->pref_hw_mode_caps = *hw_mode_caps;
			soc->wmi_ab.preferred_hw_mode = mode;
		}
	}

	ath12k_dbg(soc, ATH12K_DBG_WMI, "preferred_hw_mode:%d\n",
		   soc->wmi_ab.preferred_hw_mode);
	if (soc->wmi_ab.preferred_hw_mode == WMI_HOST_HW_MODE_MAX)
		return -EINVAL;

	return 0;
}

static int ath12k_wmi_mac_phy_caps_parse(struct ath12k_base *soc,
					 u16 tag, u16 len,
					 const void *ptr, void *data)
{
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;

	if (tag != WMI_TAG_MAC_PHY_CAPABILITIES)
		return -EPROTO;

	if (svc_rdy_ext->n_mac_phy_caps >= svc_rdy_ext->tot_phy_id)
		return -ENOBUFS;

	len = min_t(u16, len, sizeof(struct ath12k_wmi_mac_phy_caps_params));
	if (!svc_rdy_ext->n_mac_phy_caps) {
		svc_rdy_ext->mac_phy_caps = kzalloc((svc_rdy_ext->tot_phy_id) * len,
						    GFP_ATOMIC);
		if (!svc_rdy_ext->mac_phy_caps)
			return -ENOMEM;
	}

	memcpy(svc_rdy_ext->mac_phy_caps + svc_rdy_ext->n_mac_phy_caps, ptr, len);
	svc_rdy_ext->n_mac_phy_caps++;
	return 0;
}

static int ath12k_wmi_ext_hal_reg_caps_parse(struct ath12k_base *soc,
					     u16 tag, u16 len,
					     const void *ptr, void *data)
{
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;

	if (tag != WMI_TAG_HAL_REG_CAPABILITIES_EXT)
		return -EPROTO;

	if (svc_rdy_ext->n_ext_hal_reg_caps >= svc_rdy_ext->arg.num_phy)
		return -ENOBUFS;

	svc_rdy_ext->n_ext_hal_reg_caps++;
	return 0;
}

static int ath12k_wmi_ext_hal_reg_caps(struct ath12k_base *soc,
				       u16 len, const void *ptr, void *data)
{
	struct ath12k_wmi_pdev *wmi_handle = &soc->wmi_ab.wmi[0];
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;
	struct ath12k_wmi_hal_reg_capabilities_ext_arg reg_cap;
	int ret;
	u32 i;

	svc_rdy_ext->n_ext_hal_reg_caps = 0;
	svc_rdy_ext->ext_hal_reg_caps = ptr;
	ret = ath12k_wmi_tlv_iter(soc, ptr, len,
				  ath12k_wmi_ext_hal_reg_caps_parse,
				  svc_rdy_ext);
	if (ret) {
		ath12k_warn(soc, "failed to parse tlv %d\n", ret);
		return ret;
	}

	for (i = 0; i < svc_rdy_ext->arg.num_phy; i++) {
		ret = ath12k_pull_reg_cap_svc_rdy_ext(wmi_handle,
						      svc_rdy_ext->soc_hal_reg_caps,
						      svc_rdy_ext->ext_hal_reg_caps, i,
						      &reg_cap);
		if (ret) {
			ath12k_warn(soc, "failed to extract reg cap %d\n", i);
			return ret;
		}

		if (reg_cap.phy_id >= MAX_RADIOS) {
			ath12k_warn(soc, "unexpected phy id %u\n", reg_cap.phy_id);
			return -EINVAL;
		}

		soc->hal_reg_cap[reg_cap.phy_id] = reg_cap;
	}
	return 0;
}

static int ath12k_wmi_ext_soc_hal_reg_caps_parse(struct ath12k_base *soc,
						 u16 len, const void *ptr,
						 void *data)
{
	struct ath12k_wmi_pdev *wmi_handle = &soc->wmi_ab.wmi[0];
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;
	u8 hw_mode_id = le32_to_cpu(svc_rdy_ext->pref_hw_mode_caps.hw_mode_id);
	u32 phy_id_map;
	int pdev_index = 0;
	int ret;

	svc_rdy_ext->soc_hal_reg_caps = ptr;
	svc_rdy_ext->arg.num_phy = le32_to_cpu(svc_rdy_ext->soc_hal_reg_caps->num_phy);

	soc->num_radios = 0;
	phy_id_map = le32_to_cpu(svc_rdy_ext->pref_hw_mode_caps.phy_id_map);
	soc->fw_pdev_count = 0;

	while (phy_id_map && soc->num_radios < MAX_RADIOS) {
		ret = ath12k_pull_mac_phy_cap_svc_ready_ext(wmi_handle,
							    svc_rdy_ext,
							    hw_mode_id, soc->num_radios,
							    &soc->pdevs[pdev_index]);
		if (ret) {
			ath12k_warn(soc, "failed to extract mac caps, idx :%d\n",
				    soc->num_radios);
			return ret;
		}

		soc->num_radios++;

		/* For single_pdev_only targets,
		 * save mac_phy capability in the same pdev
		 */
		if (soc->hw_params->single_pdev_only)
			pdev_index = 0;
		else
			pdev_index = soc->num_radios;

		/* TODO: mac_phy_cap prints */
		phy_id_map >>= 1;
	}

	if (soc->hw_params->single_pdev_only) {
		soc->num_radios = 1;
		soc->pdevs[0].pdev_id = 0;
	}

	return 0;
}

static int ath12k_wmi_dma_ring_caps_parse(struct ath12k_base *soc,
					  u16 tag, u16 len,
					  const void *ptr, void *data)
{
	struct ath12k_wmi_dma_ring_caps_parse *parse = data;

	if (tag != WMI_TAG_DMA_RING_CAPABILITIES)
		return -EPROTO;

	parse->n_dma_ring_caps++;
	return 0;
}

static int ath12k_wmi_alloc_dbring_caps(struct ath12k_base *ab,
					u32 num_cap)
{
	size_t sz;
	void *ptr;

	sz = num_cap * sizeof(struct ath12k_dbring_cap);
	ptr = kzalloc(sz, GFP_ATOMIC);
	if (!ptr)
		return -ENOMEM;

	ab->db_caps = ptr;
	ab->num_db_cap = num_cap;

	return 0;
}

static void ath12k_wmi_free_dbring_caps(struct ath12k_base *ab)
{
	kfree(ab->db_caps);
	ab->db_caps = NULL;
	ab->num_db_cap = 0;
}

static int ath12k_wmi_dma_ring_caps(struct ath12k_base *ab,
				    u16 len, const void *ptr, void *data)
{
	struct ath12k_wmi_dma_ring_caps_parse *dma_caps_parse = data;
	struct ath12k_wmi_dma_ring_caps_params *dma_caps;
	struct ath12k_dbring_cap *dir_buff_caps;
	int ret;
	u32 i;

	dma_caps_parse->n_dma_ring_caps = 0;
	dma_caps = (struct ath12k_wmi_dma_ring_caps_params *)ptr;
	ret = ath12k_wmi_tlv_iter(ab, ptr, len,
				  ath12k_wmi_dma_ring_caps_parse,
				  dma_caps_parse);
	if (ret) {
		ath12k_warn(ab, "failed to parse dma ring caps tlv %d\n", ret);
		return ret;
	}

	if (!dma_caps_parse->n_dma_ring_caps)
		return 0;

	if (ab->num_db_cap) {
		ath12k_warn(ab, "Already processed, so ignoring dma ring caps\n");
		return 0;
	}

	ret = ath12k_wmi_alloc_dbring_caps(ab, dma_caps_parse->n_dma_ring_caps);
	if (ret)
		return ret;

	dir_buff_caps = ab->db_caps;
	for (i = 0; i < dma_caps_parse->n_dma_ring_caps; i++) {
		if (le32_to_cpu(dma_caps[i].module_id) >= WMI_DIRECT_BUF_MAX) {
			ath12k_warn(ab, "Invalid module id %d\n",
				    le32_to_cpu(dma_caps[i].module_id));
			ret = -EINVAL;
			goto free_dir_buff;
		}

		dir_buff_caps[i].id = le32_to_cpu(dma_caps[i].module_id);
		dir_buff_caps[i].pdev_id =
			DP_HW2SW_MACID(le32_to_cpu(dma_caps[i].pdev_id));
		dir_buff_caps[i].min_elem = le32_to_cpu(dma_caps[i].min_elem);
		dir_buff_caps[i].min_buf_sz = le32_to_cpu(dma_caps[i].min_buf_sz);
		dir_buff_caps[i].min_buf_align = le32_to_cpu(dma_caps[i].min_buf_align);
	}

	return 0;

free_dir_buff:
	ath12k_wmi_free_dbring_caps(ab);
	return ret;
}

static int ath12k_wmi_svc_rdy_ext_parse(struct ath12k_base *ab,
					u16 tag, u16 len,
					const void *ptr, void *data)
{
	struct ath12k_wmi_pdev *wmi_handle = &ab->wmi_ab.wmi[0];
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;
	int ret;

	switch (tag) {
	case WMI_TAG_SERVICE_READY_EXT_EVENT:
		ret = ath12k_pull_svc_ready_ext(wmi_handle, ptr,
						&svc_rdy_ext->arg);
		if (ret) {
			ath12k_warn(ab, "unable to extract ext params\n");
			return ret;
		}
		break;

	case WMI_TAG_SOC_MAC_PHY_HW_MODE_CAPS:
		svc_rdy_ext->hw_caps = ptr;
		svc_rdy_ext->arg.num_hw_modes =
			le32_to_cpu(svc_rdy_ext->hw_caps->num_hw_modes);
		svc_rdy_ext->arg.num_chainmask_tables = le32_to_cpu(svc_rdy_ext->hw_caps->num_chainmask_tables);
		break;

	case WMI_TAG_SOC_HAL_REG_CAPABILITIES:
		ret = ath12k_wmi_ext_soc_hal_reg_caps_parse(ab, len, ptr,
							    svc_rdy_ext);
		if (ret)
			return ret;
		break;

	case WMI_TAG_ARRAY_STRUCT:
		if (!svc_rdy_ext->hw_mode_done) {
			ret = ath12k_wmi_hw_mode_caps(ab, len, ptr, svc_rdy_ext);
			if (ret)
				return ret;

			svc_rdy_ext->hw_mode_done = true;
		} else if (!svc_rdy_ext->mac_phy_done) {
			svc_rdy_ext->n_mac_phy_caps = 0;
			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_mac_phy_caps_parse,
						  svc_rdy_ext);
			if (ret) {
				ath12k_warn(ab, "failed to parse tlv %d\n", ret);
				return ret;
			}

			svc_rdy_ext->mac_phy_done = true;
		} else if (!svc_rdy_ext->ext_hal_reg_done) {
			ret = ath12k_wmi_ext_hal_reg_caps(ab, len, ptr, svc_rdy_ext);
			if (ret)
				return ret;

			svc_rdy_ext->ext_hal_reg_done = true;
		} else if (!svc_rdy_ext->mac_phy_chainmask_combo_done) {
			svc_rdy_ext->n_mac_phy_chainmask_combo = 0;
			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_tlv_mac_phy_chainmask_combo_parse,
						  svc_rdy_ext);
			if (ret) {
				ath12k_warn(ab, "failed to parse chainmask combo tlv %d\n", ret);
				return ret;
			}
			svc_rdy_ext->mac_phy_chainmask_combo_done = true;
		} else if (!svc_rdy_ext->mac_phy_chainmask_cap_done) {
			ret = ath12k_wmi_tlv_mac_phy_chainmask_caps(ab, len, ptr, svc_rdy_ext);
			if (ret) {
				ath12k_warn(ab, "failed to parse chainmask caps tlv %d\n", ret);
				return ret;
			}
			svc_rdy_ext->mac_phy_chainmask_cap_done = true;
		} else if (!svc_rdy_ext->oem_dma_ring_cap_done) {
			svc_rdy_ext->oem_dma_ring_cap_done = true;
		} else if (!svc_rdy_ext->dma_ring_cap_done) {
			ret = ath12k_wmi_dma_ring_caps(ab, len, ptr,
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

static int ath12k_service_ready_ext_event(struct ath12k_base *ab,
					  struct sk_buff *skb)
{
	struct ath12k_wmi_svc_rdy_ext_parse svc_rdy_ext = { };
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_svc_rdy_ext_parse,
				  &svc_rdy_ext);
	if (ret) {
		ath12k_warn(ab, "failed to parse tlv %d\n", ret);
		goto err;
	}

	if (!test_bit(WMI_TLV_SERVICE_EXT2_MSG, ab->wmi_ab.svc_map))
		complete(&ab->wmi_ab.service_ready);

	kfree(svc_rdy_ext.mac_phy_caps);
	ath12k_wmi_free_chainmask_caps(&svc_rdy_ext);
	return 0;

err:
	kfree(svc_rdy_ext.mac_phy_caps);
	ath12k_wmi_free_chainmask_caps(&svc_rdy_ext);
	ath12k_wmi_free_dbring_caps(ab);
	return ret;
}

static int ath12k_pull_svc_ready_ext2(struct ath12k_wmi_pdev *wmi_handle,
				      const void *ptr,
				      struct ath12k_wmi_svc_rdy_ext2_arg *arg)
{
	const struct wmi_service_ready_ext2_event *ev = ptr;

	if (!ev)
		return -EINVAL;

	arg->reg_db_version = le32_to_cpu(ev->reg_db_version);
	arg->hw_min_max_tx_power_2ghz = le32_to_cpu(ev->hw_min_max_tx_power_2ghz);
	arg->hw_min_max_tx_power_5ghz = le32_to_cpu(ev->hw_min_max_tx_power_5ghz);
	arg->chwidth_num_peer_caps = le32_to_cpu(ev->chwidth_num_peer_caps);
	arg->preamble_puncture_bw = le32_to_cpu(ev->preamble_puncture_bw);
	arg->max_user_per_ppdu_ofdma = le32_to_cpu(ev->max_user_per_ppdu_ofdma);
	arg->max_user_per_ppdu_mumimo = le32_to_cpu(ev->max_user_per_ppdu_mumimo);
	arg->target_cap_flags = le32_to_cpu(ev->target_cap_flags);
	arg->max_tid_msduq = le32_to_cpu(ev->max_num_msduq_supported_per_tid);
	arg->def_tid_msduq = le32_to_cpu(ev->default_num_msduq_supported_per_tid);
	arg->afc_deployment_type = le32_to_cpu(ev->afc_deployment_type);
	return 0;
}

static void ath12k_wmi_eht_caps_parse(struct ath12k_pdev *pdev, u32 band,
				      const __le32 cap_mac_info[],
				      const __le32 cap_phy_info[],
				      const __le32 supp_mcs[],
				      const struct ath12k_wmi_ppe_threshold_params *ppet,
				       __le32 cap_info_internal)
{
	struct ath12k_band_cap *cap_band = &pdev->cap.band[band];
	u32 support_320mhz;
	u8 i;

	if (band == NL80211_BAND_6GHZ)
		support_320mhz = cap_band->eht_cap_phy_info[0] &
					IEEE80211_EHT_PHY_CAP0_320MHZ_IN_6GHZ;

	for (i = 0; i < WMI_MAX_EHTCAP_MAC_SIZE; i++)
		cap_band->eht_cap_mac_info[i] = le32_to_cpu(cap_mac_info[i]);

	for (i = 0; i < WMI_MAX_EHTCAP_PHY_SIZE; i++)
		cap_band->eht_cap_phy_info[i] = le32_to_cpu(cap_phy_info[i]);

	if (band == NL80211_BAND_6GHZ)
		cap_band->eht_cap_phy_info[0] |= support_320mhz;

	cap_band->eht_mcs_20_only = le32_to_cpu(supp_mcs[0]);
	cap_band->eht_mcs_80 = le32_to_cpu(supp_mcs[1]);
	if (band != NL80211_BAND_2GHZ) {
		cap_band->eht_mcs_160 = le32_to_cpu(supp_mcs[2]);
		cap_band->eht_mcs_320 = le32_to_cpu(supp_mcs[3]);
	}

	cap_band->eht_ppet.numss_m1 = le32_to_cpu(ppet->numss_m1);
	cap_band->eht_ppet.ru_bit_mask = le32_to_cpu(ppet->ru_info);
	for (i = 0; i < WMI_MAX_NUM_SS; i++)
		cap_band->eht_ppet.ppet16_ppet8_ru3_ru0[i] =
			le32_to_cpu(ppet->ppet16_ppet8_ru3_ru0[i]);

	cap_band->eht_cap_info_internal = le32_to_cpu(cap_info_internal);
}

static int
ath12k_wmi_tlv_mac_phy_caps_ext_parse(struct ath12k_base *ab,
				      const struct ath12k_wmi_caps_ext_params *caps,
				      struct ath12k_pdev *pdev)
{
	struct ath12k_band_cap *cap_band;
	u32 bands, support_320mhz;
	int i;

	if (ab->hw_params->single_pdev_only) {
		if (caps->hw_mode_id == WMI_HOST_HW_MODE_SINGLE) {
			support_320mhz = le32_to_cpu(caps->eht_cap_phy_info_5ghz[0]) &
				IEEE80211_EHT_PHY_CAP0_320MHZ_IN_6GHZ;
			cap_band = &pdev->cap.band[NL80211_BAND_6GHZ];
			cap_band->eht_cap_phy_info[0] |= support_320mhz;
			return 0;
		}

		for (i = 0; i < ab->fw_pdev_count; i++) {
			struct ath12k_fw_pdev *fw_pdev = &ab->fw_pdev[i];

			if (fw_pdev->pdev_id == ath12k_wmi_caps_ext_get_pdev_id(caps) &&
			    fw_pdev->phy_id == le32_to_cpu(caps->phy_id)) {
				bands = fw_pdev->supported_bands;
				break;
			}
		}

		if (i == ab->fw_pdev_count)
			return -EINVAL;
	} else {
		bands = pdev->cap.supported_bands;
	}

	if (bands & WMI_HOST_WLAN_2GHZ_CAP) {
		ath12k_wmi_eht_caps_parse(pdev, NL80211_BAND_2GHZ,
					  caps->eht_cap_mac_info_2ghz,
					  caps->eht_cap_phy_info_2ghz,
					  caps->eht_supp_mcs_ext_2ghz,
					  &caps->eht_ppet_2ghz,
					  caps->eht_cap_info_internal);
	}

	if (bands & WMI_HOST_WLAN_5GHZ_CAP) {
		ath12k_wmi_eht_caps_parse(pdev, NL80211_BAND_5GHZ,
					  caps->eht_cap_mac_info_5ghz,
					  caps->eht_cap_phy_info_5ghz,
					  caps->eht_supp_mcs_ext_5ghz,
					  &caps->eht_ppet_5ghz,
					  caps->eht_cap_info_internal);

		ath12k_wmi_eht_caps_parse(pdev, NL80211_BAND_6GHZ,
					  caps->eht_cap_mac_info_5ghz,
					  caps->eht_cap_phy_info_5ghz,
					  caps->eht_supp_mcs_ext_5ghz,
					  &caps->eht_ppet_5ghz,
					  caps->eht_cap_info_internal);
	}

	pdev->cap.eml_cap = le32_to_cpu(caps->eml_capability);
	pdev->cap.mld_cap = le32_to_cpu(caps->mld_capability);

	return 0;
}

static int ath12k_wmi_tlv_mac_phy_caps_ext(struct ath12k_base *ab, u16 tag,
					   u16 len, const void *ptr,
					   void *data)
{
	const struct ath12k_wmi_caps_ext_params *caps = ptr;
	int i = 0, ret;

	if (tag != WMI_TAG_MAC_PHY_CAPABILITIES_EXT)
		return -EPROTO;

	if (ab->hw_params->single_pdev_only) {
		if (ab->wmi_ab.preferred_hw_mode != le32_to_cpu(caps->hw_mode_id) &&
		    caps->hw_mode_id != WMI_HOST_HW_MODE_SINGLE)
			return 0;
	} else {
		for (i = 0; i < ab->num_radios; i++) {
			if (ab->pdevs[i].pdev_id ==
			    ath12k_wmi_caps_ext_get_pdev_id(caps))
				break;
		}

		if (i == ab->num_radios)
			return -EINVAL;
	}

	ret = ath12k_wmi_tlv_mac_phy_caps_ext_parse(ab, caps, &ab->pdevs[i]);
	if (ret) {
		ath12k_warn(ab,
			    "failed to parse extended MAC PHY capabilities for pdev %d: %d\n",
			    ret, ab->pdevs[i].pdev_id);
		return ret;
	}

	return 0;
}

static int ath12k_wmi_svc_rdy_ext2_parse(struct ath12k_base *ab,
					 u16 tag, u16 len,
					 const void *ptr, void *data)
{
	struct ath12k_wmi_pdev *wmi_handle = &ab->wmi_ab.wmi[0];
	struct ath12k_wmi_svc_rdy_ext2_parse *parse = data;
	int ret;

	switch (tag) {
	case WMI_TAG_SERVICE_READY_EXT2_EVENT:
		ret = ath12k_pull_svc_ready_ext2(wmi_handle, ptr,
						 &parse->arg);
		if (ret) {
			ath12k_warn(ab,
				    "failed to extract wmi service ready ext2 parameters: %d\n",
				    ret);
			return ret;
		}
		ab->chwidth_num_peer_caps = parse->arg.chwidth_num_peer_caps;
		ab->max_tid_msduq = parse->arg.max_tid_msduq;
		ab->def_tid_msduq = parse->arg.def_tid_msduq;
		ab->afc_dev_deployment = parse->arg.afc_deployment_type;
		break;

	case WMI_TAG_ARRAY_STRUCT:
		if (!parse->dma_ring_cap_done) {
			ret = ath12k_wmi_dma_ring_caps(ab, len, ptr,
						       &parse->dma_caps_parse);
			if (ret)
				return ret;

			parse->dma_ring_cap_done = true;
		} else if (!parse->spectral_bin_scaling_done) {
			/* TODO: This is a place-holder as WMI tag for
			 * spectral scaling is before
			 * WMI_TAG_MAC_PHY_CAPABILITIES_EXT
			 */
			parse->spectral_bin_scaling_done = true;
		} else if (!parse->mac_phy_caps_ext_done) {
			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_tlv_mac_phy_caps_ext,
						  parse);
			if (ret) {
				ath12k_warn(ab, "failed to parse extended MAC PHY capabilities WMI TLV: %d\n",
					    ret);
				return ret;
			}

			parse->mac_phy_caps_ext_done = true;
		}
		break;
	default:
		break;
	}

	return 0;
}

static int ath12k_service_ready_ext2_event(struct ath12k_base *ab,
					   struct sk_buff *skb)
{
	struct ath12k_wmi_svc_rdy_ext2_parse svc_rdy_ext2 = { };
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_svc_rdy_ext2_parse,
				  &svc_rdy_ext2);
	if (ret) {
		ath12k_warn(ab, "failed to parse ext2 event tlv %d\n", ret);
		goto err;
	}

	complete(&ab->wmi_ab.service_ready);

	return 0;

err:
	ath12k_wmi_free_dbring_caps(ab);
	return ret;
}

static int ath12k_pull_vdev_start_resp_tlv(struct ath12k_base *ab, struct sk_buff *skb,
					   struct wmi_vdev_start_resp_event *vdev_rsp)
{
	const void **tb;
	const struct wmi_vdev_start_resp_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_VDEV_START_RESPONSE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch vdev start resp ev");
		kfree(tb);
		return -EPROTO;
	}

	*vdev_rsp = *ev;

	kfree(tb);
	return 0;
}

static int ath12k_copy_afc_power_event_fixed_info(struct ath12k_base *ab,
						  struct ath12k_afc_info *afc,
						  const void *ptr,
						  u16 len)
{
	struct wmi_afc_power_event_param *afc_pwr_param;
	struct ath12k_afc_sp_reg_info *afc_reg_info;

	afc_pwr_param = (struct wmi_afc_power_event_param *)ptr;
	afc_reg_info = kzalloc(sizeof(*afc_reg_info), GFP_ATOMIC);

	if (!afc_reg_info)
		return -ENOMEM;

	afc_reg_info->fw_status_code =
				le32_to_cpu(afc_pwr_param->fw_status_code);
	afc_reg_info->resp_id = le32_to_cpu(afc_pwr_param->resp_id);
	afc_reg_info->serv_resp_code =
				le32_to_cpu(afc_pwr_param->afc_serv_resp_code);
	afc_reg_info->afc_wfa_version =
				le32_to_cpu(afc_pwr_param->afc_wfa_version);
	afc_reg_info->avail_exp_time_d =
				le32_to_cpu(afc_pwr_param->avail_exp_time_d);
	afc_reg_info->avail_exp_time_t =
				le32_to_cpu(afc_pwr_param->avail_exp_time_t);
	afc->afc_reg_info = afc_reg_info;

	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "pwr event-fw status %d req id %d server resp code %d wfa version %d expiry date %d time %d\n",
		   afc_reg_info->fw_status_code, afc_reg_info->resp_id,
		   afc_reg_info->serv_resp_code, afc_reg_info->afc_wfa_version,
		   afc_reg_info->avail_exp_time_d, afc_reg_info->avail_exp_time_t);
	return 0;
}

static int ath12k_wmi_afc_fill_freq_obj(struct ath12k_base *ab,
					const void *ptr, u16 len,
					struct ath12k_afc_info *afc)
{
	struct ath12k_afc_sp_reg_info *afc_reg_info = afc->afc_reg_info;
	struct wmi_6ghz_afc_frequency_info *freq_buf = NULL;
	struct ath12k_afc_freq_obj *freq_obj = NULL;
	int i;
	/* AFC payload with 0 freq objects is still considered a valid payload. Hence,
	 * do not return an error.
	 */
	if (!afc_reg_info->num_freq_objs) {
		ath12k_dbg(ab, ATH12K_DBG_AFC, "No freq objects in afc power event\n");
		return 0;
	}

	ath12k_dbg(ab, ATH12K_DBG_AFC, "Num afc freq obj received %d\n",
		   afc_reg_info->num_freq_objs);
	freq_obj = kzalloc(afc_reg_info->num_freq_objs * sizeof(*freq_obj),
			   GFP_ATOMIC);
	if (!freq_obj)
		return -ENOMEM;

	freq_buf = (struct wmi_6ghz_afc_frequency_info *)ptr;
	for (i = 0; i < afc_reg_info->num_freq_objs; i++) {
		freq_obj[i].low_freq =
				le32_to_cpu(u32_get_bits(freq_buf[i].freq_info,
							 WMI_AFC_LOW_FREQUENCY));
		freq_obj[i].high_freq =
				le32_to_cpu(u32_get_bits(freq_buf[i].freq_info,
							 WMI_AFC_HIGH_FREQUENCY));
		freq_obj[i].max_psd = le32_to_cpu(freq_buf[i].psd_power_info);
		ath12k_dbg(ab, ATH12K_DBG_AFC,
			   "Freq obj: low: %d high: %d psd: %d\n",
			   freq_obj[i].low_freq, freq_obj[i].high_freq,
			   freq_obj[i].max_psd);
	}

	afc_reg_info->afc_freq_info = freq_obj;

	return 0;
}

static int ath12k_wmi_afc_fill_chan_obj(struct ath12k_base *ab,
					const void *ptr, u16 len,
					struct ath12k_afc_info *afc)
{
	struct ath12k_afc_sp_reg_info *afc_reg_info = afc->afc_reg_info;
	struct ath12k_afc_chan_obj *chan_obj = NULL;
	struct ath12k_chan_eirp_obj *eirp_info = NULL;
	struct wmi_6ghz_afc_channel_info *chan_buf = NULL;
	int i;

	/* AFC payload with 0 channel objects is still considered a valid payload. Hence,
	 * do not return an error.
	 */
	if (!afc_reg_info->num_chan_objs) {
		ath12k_dbg(ab, ATH12K_DBG_AFC, "No channel objects in afc power event\n");
		return 0;
	}

	ath12k_dbg(ab, ATH12K_DBG_AFC, "Num chan objects received %d\n",
		   afc_reg_info->num_chan_objs);
	chan_obj = kzalloc(afc_reg_info->num_chan_objs * sizeof(*chan_obj),
			   GFP_ATOMIC);
	if (!chan_obj)
		return -ENOMEM;

	chan_buf = (struct wmi_6ghz_afc_channel_info *)ptr;
	for (i = 0; i < afc_reg_info->num_chan_objs; i++) {
		chan_obj[i].global_opclass =
			le32_to_cpu(chan_buf[i].global_operating_class);
		chan_obj[i].num_chans =
				le32_to_cpu(chan_buf[i].num_channels);
		ath12k_dbg(ab, ATH12K_DBG_AFC,
			   "Chan obj %d  global_opclass : %d num_chans %d\n", i,
			   chan_obj[i].global_opclass, chan_obj[i].num_chans);
		eirp_info = kzalloc(chan_obj[i].num_chans * sizeof(*eirp_info),
				    GFP_ATOMIC);
		if (!eirp_info)
			return -ENOMEM;

		chan_obj[i].chan_eirp_info = eirp_info;
	}

	afc_reg_info->afc_chan_info = chan_obj;

	return 0;
}

static int ath12k_wmi_afc_fill_chan_eirp_obj(struct ath12k_base *ab,
					     const void *ptr, u16 len,
					     struct ath12k_afc_info *afc,
					     u32 total_eirp_info)
{
	struct ath12k_afc_sp_reg_info *afc_reg_info = afc->afc_reg_info;
	struct wmi_afc_chan_eirp_power_info *eirp_buf = NULL;
	struct ath12k_afc_chan_obj *chan_info = NULL;
	struct ath12k_chan_eirp_obj *eirp_obj;
	int eirp_count, idx1, idx2, count = 0;

	eirp_buf = (struct wmi_afc_chan_eirp_power_info *)ptr;
	chan_info = afc_reg_info->afc_chan_info;
	for (idx1 = 0; idx1 < afc_reg_info->num_chan_objs; ++idx1) {
		eirp_obj = chan_info[idx1].chan_eirp_info;
		eirp_count = le32_to_cpu(chan_info[idx1].num_chans);
		ath12k_dbg(ab, ATH12K_DBG_AFC, "Chan obj %d Chan eirp count %d\n",
			   idx1, eirp_count);
		for (idx2 = 0; idx2 < eirp_count; ++idx2) {
			eirp_obj[idx2].cfi =
				le32_to_cpu(eirp_buf[count].channel_cfi);
			eirp_obj[idx2].eirp_power =
					le32_to_cpu(eirp_buf[count].eirp_pwr);
		ath12k_dbg(ab, ATH12K_DBG_AFC, "Chan eirp obj %d CFI %d EIRP %d\n",
			   idx2, eirp_obj[idx2].cfi, eirp_obj[idx2].eirp_power);
			++count;
		}
	}

	return 0;
}

/**
 * ath12k_copy_afc_expiry_event - Copy AFC expiry event from WMI params
 * @ab: Pointer to ath12k_base structure
 * @afc: Pointer to AFC information structure
 * @ptr: Pointer to event data
 * @len: Length of event data
 *
 * Return: 0 on success, negative error code on failure
 */
static int ath12k_copy_afc_expiry_event(struct ath12k_base *ab,
					struct ath12k_afc_info *afc, const void *ptr,
					u16 len)
{
	struct wmi_afc_expiry_event_param *param = (struct wmi_afc_expiry_event_param *)ptr;

	afc->request_id = param->request_id;
	afc->event_subtype = param->event_subtype;
	ath12k_dbg(ab, ATH12K_DBG_WMI, "Received AFC expiry request id %u subtye %d\n",
		   afc->request_id, afc->event_subtype);
	return 0;
}

static int ath12k_wmi_afc_event_parser(struct ath12k_base *ab,
				       u16 tag, u16 len,
				       const void *ptr, void *data)
{
	struct ath12k_afc_info *afc = (struct ath12k_afc_info *)data;
	int total_eirp_obj, sub_tlv_size, ret = 0;
	struct wmi_tlv *tlv;
	u16 tlv_tag;

	ath12k_dbg(ab, ATH12K_DBG_AFC, "AFC event tag 0x%x of len %d type %d\n",
		   tag, len, afc->event_type);

	switch (tag) {
	case WMI_TAG_AFC_EVENT_FIXED_PARAM:
		/* Fixed param is already processed */
		break;
	case WMI_TAG_AFC_EXPIRY_EVENT_PARAM:
		if (len == 0) {
			ath12k_warn(ab, "len should not be zero\n");
			return 0;
		}

		if (afc->event_type != ATH12K_AFC_EVENT_TIMER_EXPIRY) {
			ath12k_warn(ab, "Invalid event_type %d received\n", afc->event_type);
			return 0;
		}

		ret = ath12k_copy_afc_expiry_event(ab, afc, ptr, len);
		if (ret) {
			ath12k_warn(ab, "Failed to copy expiry event\n");
			return ret;
		}
		break;
	case WMI_TAG_AFC_POWER_EVENT_PARAM:
		if (len == 0)
			return 0;

		if (afc->event_type != ATH12K_AFC_EVENT_POWER_INFO)
			return 0;

		ret = ath12k_copy_afc_power_event_fixed_info(ab, afc, ptr, len);
		if (ret) {
			ath12k_warn(ab, "Failed to copy power event fixed info\n");
			return ret;
		}
		break;
	case WMI_TAG_ARRAY_STRUCT:
		struct ath12k_afc_sp_reg_info *afc_reg_info = afc->afc_reg_info;

		if (len == 0)
			return 0;

		tlv = (struct wmi_tlv *)ptr;
		tlv_tag = u32_get_bits(tlv->header, WMI_TLV_TAG);

		if (tlv_tag == WMI_TAG_AFC_6GHZ_FREQUENCY_INFO) {
			sub_tlv_size = sizeof(struct wmi_6ghz_afc_frequency_info);
			afc_reg_info->num_freq_objs = len / sub_tlv_size;
			ret = ath12k_wmi_afc_fill_freq_obj(ab, ptr, len, afc);
		} else if (tlv_tag == WMI_TAG_AFC_6GHZ_CHANNEL_INFO) {
			sub_tlv_size = sizeof(struct wmi_6ghz_afc_channel_info);
			afc_reg_info->num_chan_objs = len / sub_tlv_size;
			ret = ath12k_wmi_afc_fill_chan_obj(ab, ptr, len, afc);
		} else if (tlv_tag == WMI_TAG_AFC_CHAN_EIRP_POWER_INFO) {
			sub_tlv_size = sizeof(struct wmi_afc_chan_eirp_power_info);
			total_eirp_obj = len / sub_tlv_size;
			ret =  ath12k_wmi_afc_fill_chan_eirp_obj(ab, ptr, len, afc,
								 total_eirp_obj);
		}
		break;
	default:
		ath12k_warn(ab, "Unknown afc event tag %d\n", tag);
		return -EINVAL;
	}

	return ret;
}

static int
ath12k_wmi_afc_process_fixed_param(struct ath12k_base *ab,
				   void *ptr, size_t len, struct ath12k_afc_info *afc,
				   u8 *pdev_id, struct ath12k **ar)
{
	struct wmi_afc_event_fixed_param *fixed_param;
	const struct wmi_tlv *tlv;
	u16 tlv_tag;

	if (!ptr) {
		ath12k_warn(ab, "No data present in afc event\n");
		return -1;
	}

	if (len < (sizeof(*fixed_param) + TLV_HDR_SIZE)) {
		ath12k_warn(ab, "afc event size invalid\n");
		return -1;
	}

	tlv = (struct wmi_tlv *)ptr;
	tlv_tag = u32_get_bits(tlv->header, WMI_TLV_TAG);
	ptr += sizeof(*tlv);

	if (tlv_tag == WMI_TAG_AFC_EVENT_FIXED_PARAM) {
		fixed_param = (struct wmi_afc_event_fixed_param *)ptr;
		*pdev_id = le32_to_cpu(fixed_param->pdev_id);
		*ar = ab->pdevs[*pdev_id].ar;
		if (!*ar)
			ath12k_warn(ab, "Cannot get ar for afc fixed param\n");
	} else {
		ath12k_warn(ab, "Wrong tag %d in afc fixed param\n", tlv_tag);
		return -1;
	}

	afc->event_type = le32_to_cpu(fixed_param->event_type);

	return 0;
}

static void ath12k_wmi_afc_event(struct ath12k_base *ab,
				 struct sk_buff *skb)
{
	struct ath12k_afc_info afc = {};
	struct ath12k_afc_info *afc_info = &afc;
	struct ath12k *ar;
	int ret;
	u8 pdev_id;

	ret = ath12k_wmi_afc_process_fixed_param(ab, skb->data, skb->len,
						 afc_info, &pdev_id, &ar);
	if (ret) {
		ath12k_warn(ab, "Failed to process afc fixed param ret %d\n", ret);
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_AFC, "Received AFC event of type %d for pdev: %d\n",
		   afc_info->event_type, pdev_id);
	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_afc_event_parser, afc_info);
	if (ret) {
		ath12k_free_afc_power_event_info(afc_info);
		ath12k_warn(ab, "Failed to parse afc event type %d ret = %d\n",
			    afc_info->event_type, ret);
		return;
	}

	if (!ar && afc_info->event_type != ATH12K_AFC_EVENT_TIMER_EXPIRY) {
		ath12k_warn(ab, "Failed to get ar for afc processing\n");
		ath12k_free_afc_power_event_info(afc_info);
		return;
	}

	switch (afc_info->event_type) {
	case ATH12K_AFC_EVENT_POWER_INFO:
		ret = ath12k_reg_process_afc_power_event(ar, afc_info);
		if (ret)
			ath12k_warn(ab, "AFC reg rule update failed ret : %d\n",
				    ret);

		/* Update AFC application with power event update complete */
		ath12k_vendor_send_power_update_complete(ar, afc_info);
		if (afc_info->afc_reg_info->fw_status_code !=
		    REG_FW_AFC_POWER_EVENT_SUCCESS) {
			ath12k_free_afc_power_event_info(afc_info);
		}

		break;
	case ATH12K_AFC_EVENT_TIMER_EXPIRY:
		if (ar) {
			ar->afc.event_type = afc_info->event_type;
			ar->afc.event_subtype = afc_info->event_subtype;
			ar->afc.request_id = afc_info->request_id;
			ret = ath12k_process_expiry_event(ar);
			if (ret)
				ath12k_warn(ab, "Failed to process expiry event\n");
		} else {
			ab->afc_exp_info[pdev_id].is_afc_exp_valid = true;
			ab->afc_exp_info[pdev_id].event_subtype = afc_info->event_subtype;
			ab->afc_exp_info[pdev_id].req_id = afc_info->request_id;
			ath12k_dbg(ab, ATH12K_DBG_AFC,
				   "AFC expiry event received without ar\n");
		}
		break;
	default:
		ath12k_warn(ab, "AFC reg rule update failed ret : %d\n", ret);
		break;
	};
}

int ath12k_wmi_send_afc_cmd_tlv(struct ath12k *ar, int data_type,
				enum wmi_afc_cmd_type afc_cmd)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	int ret = 0;
	struct sk_buff *skb;
	struct wmi_afc_cmd_fixed_param *cmd = NULL;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_afc_cmd_fixed_param *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_AFC_CMD_FIXED_PARAM) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	cmd->cmd_type = cpu_to_le32(afc_cmd);
	cmd->serv_resp_format = cpu_to_le32(data_type);

	switch (afc_cmd) {
	case WMI_AFC_CMD_SERV_RESP_READY:
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "Sending afc indication for pdev id %d resp format %d\n",
			   cmd->pdev_id, cmd->serv_resp_format);
		break;
	case WMI_AFC_CMD_CLEAR_AFC_PAYLOAD:
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "Sending afc payload clear cmd for pdev id %d\n",
			   cmd->pdev_id);
		break;
	case WMI_AFC_CMD_RESET_AFC:
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "Sending afc reset cmd for pdev id %d\n",
			   cmd->pdev_id);
		break;
	default:
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "Unknown AFC command\n");
		return -EINVAL;
	}

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_AFC_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "Failed to send WMI_AFC_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static struct ath12k_reg_rule
*create_ext_reg_rules_from_wmi(u32 num_reg_rules,
			       struct ath12k_wmi_reg_rule_ext_params *wmi_reg_rule)
{
	struct ath12k_reg_rule *reg_rule_ptr;
	u32 count;

	reg_rule_ptr = kzalloc((num_reg_rules * sizeof(*reg_rule_ptr)),
			       GFP_ATOMIC);

	if (!reg_rule_ptr)
		return NULL;

	for (count = 0; count < num_reg_rules; count++) {
		reg_rule_ptr[count].start_freq =
			le32_get_bits(wmi_reg_rule[count].freq_info,
				      REG_RULE_START_FREQ);
		reg_rule_ptr[count].end_freq =
			le32_get_bits(wmi_reg_rule[count].freq_info,
				      REG_RULE_END_FREQ);
		reg_rule_ptr[count].max_bw =
			le32_get_bits(wmi_reg_rule[count].bw_pwr_info,
				      REG_RULE_MAX_BW);
		reg_rule_ptr[count].reg_power =
			le32_get_bits(wmi_reg_rule[count].bw_pwr_info,
				      REG_RULE_REG_PWR);
		reg_rule_ptr[count].ant_gain =
			le32_get_bits(wmi_reg_rule[count].bw_pwr_info,
				      REG_RULE_ANT_GAIN);
		reg_rule_ptr[count].flags =
			le32_get_bits(wmi_reg_rule[count].flag_info,
				      REG_RULE_FLAGS);
		reg_rule_ptr[count].psd_flag =
			le32_get_bits(wmi_reg_rule[count].psd_power_info,
				      REG_RULE_PSD_INFO);
		reg_rule_ptr[count].psd_eirp =
			le32_get_bits(wmi_reg_rule[count].psd_power_info,
				      REG_RULE_PSD_EIRP);
	}

	return reg_rule_ptr;
}

static u8 ath12k_wmi_ignore_num_extra_rules(struct ath12k_wmi_reg_rule_ext_params *rule,
					    u32 num_reg_rules)
{
	u8 num_invalid_5ghz_rules = 0;
	u32 count, start_freq;

	for (count = 0; count < num_reg_rules; count++) {
		start_freq = le32_get_bits(rule[count].freq_info, REG_RULE_START_FREQ);

		if (start_freq >= ATH12K_MIN_6GHZ_FREQ)
			num_invalid_5ghz_rules++;
	}

	return num_invalid_5ghz_rules;
}

static const char *ath12k_cc_status_to_str(enum ath12k_reg_cc_code code)
{
       switch (code) {
       case REG_SET_CC_STATUS_PASS:
               return "REG_SET_CC_STATUS_PASS";
       case REG_CURRENT_ALPHA2_NOT_FOUND:
               return "REG_CURRENT_ALPHA2_NOT_FOUND";
       case REG_INIT_ALPHA2_NOT_FOUND:
               return "REG_INIT_ALPHA2_NOT_FOUND";
       case REG_SET_CC_CHANGE_NOT_ALLOWED:
               return "REG_SET_CC_CHANGE_NOT_ALLOWED";
       case REG_SET_CC_STATUS_NO_MEMORY:
               return "REG_SET_CC_STATUS_NO_MEMORY";
       case REG_SET_CC_STATUS_FAIL:
               return "REG_SET_CC_STATUS_FAIL";
       default:
               return "unknown cc status";
       }
}

static const char *ath12k_super_reg_6g_to_str(enum reg_super_domain_6g domain_id)
{
        switch (domain_id) {
        case FCC1_6G:
                return "FCC1_6G";
        case ETSI1_6G:
                return "ETSI1_6G";
        case ETSI2_6G:
                return "ETSI2_6G";
        case APL1_6G:
                return "APL1_6G";
        case FCC1_6G_CL:
                return "FCC1_6G_CL";
        default:
                return "unknown domain id";
        }
}

static const char *ath12k_6g_client_type_to_str(enum wmi_reg_6g_client_type type)
{
        switch (type) {
        case WMI_REG_DEFAULT_CLIENT:
                return "DEFAULT CLIENT";
        case WMI_REG_SUBORDINATE_CLIENT:
                return "SUBORDINATE CLIENT";
        default:
                return "unknown client type";
        }
}

static const char *ath12k_6g_ap_type_to_str(enum wmi_reg_6g_ap_type type)
{
        switch (type) {
        case WMI_REG_INDOOR_AP:
                return "INDOOR AP";
        case WMI_REG_STD_POWER_AP:
                return "STANDARD POWER AP";
        case WMI_REG_VLP_AP:
                return "VERY LOW POWER AP";
        default:
                return "unknown AP type";
       }
}

static const char *ath12k_sub_reg_6g_to_str(enum reg_subdomains_6g sub_id)
{
        switch (sub_id) {
        case FCC1_CLIENT_LPI_REGULAR_6G:
                return "FCC1_CLIENT_LPI_REGULAR_6G";
        case FCC1_CLIENT_SP_6G:
                return "FCC1_CLIENT_SP_6G";
        case FCC1_AP_LPI_6G:
                return "FCC1_AP_LPI_6G/FCC1_CLIENT_LPI_SUBORDINATE";
        case FCC1_AP_SP_6G:
                return "FCC1_AP_SP_6G";
        case ETSI1_LPI_6G:
                return "ETSI1_LPI_6G";
        case ETSI1_VLP_6G:
                return "ETSI1_VLP_6G";
        case ETSI2_LPI_6G:
                return "ETSI2_LPI_6G";
        case ETSI2_VLP_6G:
                return "ETSI2_VLP_6G";
        case APL1_LPI_6G:
                return "APL1_LPI_6G";
        case APL1_VLP_6G:
                return "APL1_VLP_6G";
        case EMPTY_6G:
                return "N/A";
        default:
                return "unknown sub reg id";
        }
}

static void ath12k_print_reg_rule(struct ath12k_base *ab, const char *prev,
				  u32 num_reg_rules,
				  const struct ath12k_reg_rule *reg_rule_ptr)
{
       const struct ath12k_reg_rule *reg_rule = reg_rule_ptr;
       u32 count;

       ath12k_dbg(ab, ATH12K_DBG_WMI, "%s reg rules number %d\n", prev, num_reg_rules);

       for (count = 0; count < num_reg_rules; count++) {
	       ath12k_dbg(ab, ATH12K_DBG_WMI,
			  "reg rule %d: (%d - %d @ %d) (%d, %d) (FLAGS %d) (psd flag %d EIRP %d dB/MHz)\n",
			  count + 1, reg_rule->start_freq, reg_rule->end_freq,
			  reg_rule->max_bw, reg_rule->ant_gain, reg_rule->reg_power,
			  reg_rule->flags, reg_rule->psd_flag, reg_rule->psd_eirp);
	       reg_rule++;
       }
}

static u8
ath12k_invalid_5g_reg_ext_rules_from_wmi(u32 num_reg_rules,
                                         struct ath12k_wmi_reg_rule_ext_params *wmi_reg_rule)
{
        u8 num_invalid_5g_rules = 0;
        u32 count, start_freq, end_freq;

        for (count = 0; count < num_reg_rules; count++) {
                start_freq = FIELD_GET(REG_RULE_START_FREQ,
                                       wmi_reg_rule[count].freq_info);
                end_freq = FIELD_GET(REG_RULE_END_FREQ,
                                     wmi_reg_rule[count].freq_info);

                if (start_freq >= ATH12K_MIN_6GHZ_FREQ &&
                    end_freq <= ATH12K_MAX_6GHZ_FREQ)
                        num_invalid_5g_rules++;
        }

        return num_invalid_5g_rules;
}

static int ath12k_pull_reg_chan_list_ext_update_ev(struct ath12k_base *ab,
						   struct sk_buff *skb,
						   struct ath12k_reg_info *reg_info)
{
	const void **tb;
	const struct wmi_reg_chan_list_cc_ext_event *ev;
	struct ath12k_wmi_reg_rule_ext_params *ext_wmi_reg_rule;
	u32 num_2g_reg_rules, num_5g_reg_rules;
	u32 num_6g_reg_rules_ap[WMI_REG_CURRENT_MAX_AP_TYPE];
	u32 num_6g_reg_rules_cl[WMI_REG_CURRENT_MAX_AP_TYPE][WMI_REG_MAX_CLIENT_TYPE];
	u8 num_invalid_5ghz_ext_rules;
	u32 total_reg_rules = 0;
	int ret, i, j, skip_6g_rules_in_5g_rules = 0;

	ath12k_dbg(ab, ATH12K_DBG_WMI, "processing regulatory ext channel list\n");

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_REG_CHAN_LIST_CC_EXT_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch reg chan list ext update ev\n");
		kfree(tb);
		return -EPROTO;
	}

	reg_info->num_2g_reg_rules = le32_to_cpu(ev->num_2g_reg_rules);
	reg_info->num_5g_reg_rules = le32_to_cpu(ev->num_5g_reg_rules);
	reg_info->num_6g_reg_rules_ap[WMI_REG_INDOOR_AP] =
		le32_to_cpu(ev->num_6g_reg_rules_ap_lpi);
	reg_info->num_6g_reg_rules_ap[WMI_REG_STD_POWER_AP] =
		le32_to_cpu(ev->num_6g_reg_rules_ap_sp);
	reg_info->num_6g_reg_rules_ap[WMI_REG_VLP_AP] =
		le32_to_cpu(ev->num_6g_reg_rules_ap_vlp);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
                  "6g reg info client type %s rnr_tpe_usable %d unspecified_ap_usable %d AP sub domain: lpi %s , sp %s , vlp %s\n",
                  ath12k_6g_client_type_to_str(reg_info->client_type),
                  reg_info->rnr_tpe_usable,
                  reg_info->unspecified_ap_usable,
                  ath12k_sub_reg_6g_to_str
                  (ev->domain_code_6g_ap_lpi),
                  ath12k_sub_reg_6g_to_str
                  (ev->domain_code_6g_ap_sp),
                  ath12k_sub_reg_6g_to_str
                  (ev->domain_code_6g_ap_vlp));

	for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
		reg_info->num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][i] =
			le32_to_cpu(ev->num_6g_reg_rules_cl_lpi[i]);
		reg_info->num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][i] =
			le32_to_cpu(ev->num_6g_reg_rules_cl_sp[i]);
		reg_info->num_6g_reg_rules_cl[WMI_REG_VLP_AP][i] =
			le32_to_cpu(ev->num_6g_reg_rules_cl_vlp[i]);
		ath12k_dbg(ab, ATH12K_DBG_WMI,
                  "6g AP BW: lpi %d - %d sp %d - %d vlp %d - %d\n",
                  ev->min_bw_6g_ap_lpi,
                  ev->max_bw_6g_ap_lpi,
                  ev->min_bw_6g_ap_sp,
                  ev->max_bw_6g_ap_sp,
                  ev->min_bw_6g_ap_vlp,
                  ev->max_bw_6g_ap_vlp);
	}

	num_2g_reg_rules = reg_info->num_2g_reg_rules;
	total_reg_rules += num_2g_reg_rules;
	num_5g_reg_rules = reg_info->num_5g_reg_rules;
	total_reg_rules += num_5g_reg_rules;

	if (num_2g_reg_rules > MAX_REG_RULES || num_5g_reg_rules > MAX_REG_RULES) {
		ath12k_warn(ab, "Num reg rules for 2G/5G exceeds max limit (num_2g_reg_rules: %d num_5g_reg_rules: %d max_rules: %d)\n",
			    num_2g_reg_rules, num_5g_reg_rules, MAX_REG_RULES);
		kfree(tb);
		return -EINVAL;
	}

	for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++) {
		num_6g_reg_rules_ap[i] = reg_info->num_6g_reg_rules_ap[i];

		if (num_6g_reg_rules_ap[i] > MAX_6GHZ_REG_RULES) {
			ath12k_warn(ab, "Num 6G reg rules for AP mode(%d) exceeds max limit (num_6g_reg_rules_ap: %d, max_rules: %d)\n",
				    i, num_6g_reg_rules_ap[i], MAX_6GHZ_REG_RULES);
			kfree(tb);
			return -EINVAL;
		}

		total_reg_rules += num_6g_reg_rules_ap[i];
	}

	for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
		num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][i] =
				reg_info->num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][i];
		total_reg_rules += num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][i];

		num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][i] =
				reg_info->num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][i];
		total_reg_rules += num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][i];

		num_6g_reg_rules_cl[WMI_REG_VLP_AP][i] =
				reg_info->num_6g_reg_rules_cl[WMI_REG_VLP_AP][i];
		total_reg_rules += num_6g_reg_rules_cl[WMI_REG_VLP_AP][i];

		if (num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][i] > MAX_6GHZ_REG_RULES ||
		    num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][i] > MAX_6GHZ_REG_RULES ||
		    num_6g_reg_rules_cl[WMI_REG_VLP_AP][i] >  MAX_6GHZ_REG_RULES) {
			ath12k_warn(ab, "Num 6g client reg rules exceeds max limit, for client(type: %d)\n",
				    i);
			kfree(tb);
			return -EINVAL;
		}
	}

	switch (le32_to_cpu(ev->status_code)) {
	case WMI_REG_SET_CC_STATUS_PASS:
		reg_info->status_code = REG_SET_CC_STATUS_PASS;
		break;
	case WMI_REG_CURRENT_ALPHA2_NOT_FOUND:
		reg_info->status_code = REG_CURRENT_ALPHA2_NOT_FOUND;
		break;
	case WMI_REG_INIT_ALPHA2_NOT_FOUND:
		reg_info->status_code = REG_INIT_ALPHA2_NOT_FOUND;
		break;
	case WMI_REG_SET_CC_CHANGE_NOT_ALLOWED:
		reg_info->status_code = REG_SET_CC_CHANGE_NOT_ALLOWED;
		break;
	case WMI_REG_SET_CC_STATUS_NO_MEMORY:
		reg_info->status_code = REG_SET_CC_STATUS_NO_MEMORY;
		break;
	case WMI_REG_SET_CC_STATUS_FAIL:
		reg_info->status_code = REG_SET_CC_STATUS_FAIL;
		break;
	}

	if (reg_info->status_code != REG_SET_CC_STATUS_PASS)
		ath12k_warn(ab, "reg chan list ext event failed status %d: %s\n",
			    reg_info->status_code,
			    ath12k_cc_status_to_str(reg_info->status_code));

	if (!total_reg_rules) {
		u32 phy_id = le32_to_cpu(ev->phy_id);
		struct ath12k *ar;

		ath12k_warn(ab, "No reg rules available, dfs %d, ctry %d domain %d\n",
			    ev->dfs_region, ev->country_id, ev->domain_code);
		if (phy_id >= ab->num_radios) {
			ath12k_warn(ab, "Invalid phy_id %d\n", phy_id);
			kfree(tb);
			return -EINVAL;
		}

		ar = ab->pdevs[phy_id].ar;
		if (!ar) {
			ath12k_warn(ab, "ar is NULL for phy_id %d\n", phy_id);
			kfree(tb);
			return -EINVAL;
		}
		/* Reset to the previous country */
		if (ab->workqueue &&
		    test_bit(ATH12K_FLAG_REGISTERED, &ab->dev_flags))
			queue_work(ab->workqueue, &ar->reg_set_previous_country);
		else
			ath12k_warn(ab, "Cannot queue work, workqueue unavailable or device not registered\n");

		kfree(tb);
		return -EINVAL;
	}

	memcpy(reg_info->alpha2, &ev->alpha2, REG_ALPHA2_LEN);

	reg_info->dfs_region = le32_to_cpu(ev->dfs_region);
	reg_info->phybitmap = le32_to_cpu(ev->phybitmap);
	reg_info->num_phy = le32_to_cpu(ev->num_phy);
	reg_info->phy_id = le32_to_cpu(ev->phy_id);
	reg_info->ctry_code = le32_to_cpu(ev->country_id);
	reg_info->reg_dmn_pair = le32_to_cpu(ev->domain_code);

	reg_info->is_ext_reg_event = true;

	reg_info->min_bw_2g = le32_to_cpu(ev->min_bw_2g);
	reg_info->max_bw_2g = le32_to_cpu(ev->max_bw_2g);
	reg_info->min_bw_5g = le32_to_cpu(ev->min_bw_5g);
	reg_info->max_bw_5g = le32_to_cpu(ev->max_bw_5g);
	reg_info->min_bw_6g_ap[WMI_REG_INDOOR_AP] = le32_to_cpu(ev->min_bw_6g_ap_lpi);
	reg_info->max_bw_6g_ap[WMI_REG_INDOOR_AP] = le32_to_cpu(ev->max_bw_6g_ap_lpi);
	reg_info->min_bw_6g_ap[WMI_REG_STD_POWER_AP] = le32_to_cpu(ev->min_bw_6g_ap_sp);
	reg_info->max_bw_6g_ap[WMI_REG_STD_POWER_AP] = le32_to_cpu(ev->max_bw_6g_ap_sp);
	reg_info->min_bw_6g_ap[WMI_REG_VLP_AP] = le32_to_cpu(ev->min_bw_6g_ap_vlp);
	reg_info->max_bw_6g_ap[WMI_REG_VLP_AP] = le32_to_cpu(ev->max_bw_6g_ap_vlp);

	for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
		reg_info->min_bw_6g_client[WMI_REG_INDOOR_AP][i] =
			le32_to_cpu(ev->min_bw_6g_client_lpi[i]);
		reg_info->max_bw_6g_client[WMI_REG_INDOOR_AP][i] =
			le32_to_cpu(ev->max_bw_6g_client_lpi[i]);
		reg_info->min_bw_6g_client[WMI_REG_STD_POWER_AP][i] =
			le32_to_cpu(ev->min_bw_6g_client_sp[i]);
		reg_info->max_bw_6g_client[WMI_REG_STD_POWER_AP][i] =
			le32_to_cpu(ev->max_bw_6g_client_sp[i]);
		reg_info->min_bw_6g_client[WMI_REG_VLP_AP][i] =
			le32_to_cpu(ev->min_bw_6g_client_vlp[i]);
		reg_info->max_bw_6g_client[WMI_REG_VLP_AP][i] =
			le32_to_cpu(ev->max_bw_6g_client_vlp[i]);
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "%s:cc_ext %s dfs %d BW: min_2g %d max_2g %d min_5g %d max_5g %d phy_bitmap 0x%x",
		   __func__, reg_info->alpha2, reg_info->dfs_region,
		   reg_info->min_bw_2g, reg_info->max_bw_2g,
		   reg_info->min_bw_5g, reg_info->max_bw_5g,
		   reg_info->phybitmap);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "num_2g_reg_rules %d num_5g_reg_rules %d",
		   num_2g_reg_rules, num_5g_reg_rules);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "num_6g_reg_rules_ap_lpi: %d num_6g_reg_rules_ap_sp: %d num_6g_reg_rules_ap_vlp: %d",
		   num_6g_reg_rules_ap[WMI_REG_INDOOR_AP],
		   num_6g_reg_rules_ap[WMI_REG_STD_POWER_AP],
		   num_6g_reg_rules_ap[WMI_REG_VLP_AP]);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "6g Regular client: num_6g_reg_rules_lpi: %d num_6g_reg_rules_sp: %d num_6g_reg_rules_vlp: %d",
		   num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][WMI_REG_DEFAULT_CLIENT],
		   num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][WMI_REG_DEFAULT_CLIENT],
		   num_6g_reg_rules_cl[WMI_REG_VLP_AP][WMI_REG_DEFAULT_CLIENT]);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "6g Subordinate client: num_6g_reg_rules_lpi: %d num_6g_reg_rules_sp: %d num_6g_reg_rules_vlp: %d",
		   num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][WMI_REG_SUBORDINATE_CLIENT],
		   num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][WMI_REG_SUBORDINATE_CLIENT],
		   num_6g_reg_rules_cl[WMI_REG_VLP_AP][WMI_REG_SUBORDINATE_CLIENT]);

	ext_wmi_reg_rule =
		(struct ath12k_wmi_reg_rule_ext_params *)((u8 *)ev
			+ sizeof(*ev)
			+ sizeof(struct wmi_tlv));

	if (num_2g_reg_rules) {
		reg_info->reg_rules_2g_ptr =
			create_ext_reg_rules_from_wmi(num_2g_reg_rules,
						      ext_wmi_reg_rule);

		if (!reg_info->reg_rules_2g_ptr) {
			kfree(tb);
			ath12k_warn(ab, "Unable to Allocate memory for 2g rules\n");
			return -ENOMEM;
		}
	}

	ext_wmi_reg_rule += num_2g_reg_rules;

	/* Firmware might include 6 GHz reg rule in 5 GHz rule list
	 * for few countries along with separate 6 GHz rule.
	 * Having same 6 GHz reg rule in 5 GHz and 6 GHz rules list
	 * causes intersect check to be true, and same rules will be
	 * shown multiple times in iw cmd.
	 * Hence, avoid parsing 6 GHz rule from 5 GHz reg rule list
	 */
	num_invalid_5ghz_ext_rules = ath12k_wmi_ignore_num_extra_rules(ext_wmi_reg_rule,
								       num_5g_reg_rules);

	if (num_invalid_5ghz_ext_rules) {
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "CC: %s 5 GHz reg rules number %d from fw, %d number of invalid 5 GHz rules",
			   reg_info->alpha2, reg_info->num_5g_reg_rules,
			   num_invalid_5ghz_ext_rules);

		num_5g_reg_rules = num_5g_reg_rules - num_invalid_5ghz_ext_rules;
		reg_info->num_5g_reg_rules = num_5g_reg_rules;
	}

        skip_6g_rules_in_5g_rules = ath12k_invalid_5g_reg_ext_rules_from_wmi(num_5g_reg_rules,
                                                                             ext_wmi_reg_rule);

        if(skip_6g_rules_in_5g_rules) {
                ath12k_dbg(ab, ATH12K_DBG_WMI,
                           "CC: %s 5g reg rules number %d from fw, %d number of invalid 5g rules",
                           reg_info->alpha2, reg_info->num_5g_reg_rules,
                           skip_6g_rules_in_5g_rules);

                num_5g_reg_rules = num_5g_reg_rules - skip_6g_rules_in_5g_rules;
                reg_info->num_5g_reg_rules = num_5g_reg_rules;
        }

	if (num_5g_reg_rules) {
		reg_info->reg_rules_5g_ptr =
			create_ext_reg_rules_from_wmi(num_5g_reg_rules,
						      ext_wmi_reg_rule);

		if (!reg_info->reg_rules_5g_ptr) {
			kfree(tb);
			ath12k_warn(ab, "Unable to Allocate memory for 5g rules\n");
			return -ENOMEM;
		}
	}


	/* We have adjusted the number of 5 GHz reg rules above. But still those
	 * many rules needs to be adjusted in ext_wmi_reg_rule.
	 *
	 * NOTE: num_invalid_5ghz_ext_rules will be 0 for rest other cases.
	 */

	ext_wmi_reg_rule += (num_5g_reg_rules + num_invalid_5ghz_ext_rules);

	for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++) {
		reg_info->reg_rules_6g_ap_ptr[i] =
			create_ext_reg_rules_from_wmi(num_6g_reg_rules_ap[i],
						      ext_wmi_reg_rule);

		if (!reg_info->reg_rules_6g_ap_ptr[i]) {
			kfree(tb);
			ath12k_warn(ab, "Unable to Allocate memory for 6g ap rules\n");
			return -ENOMEM;
		}

		ath12k_print_reg_rule(ab, ath12k_6g_ap_type_to_str(i),
                                     num_6g_reg_rules_ap[i],
                                     reg_info->reg_rules_6g_ap_ptr[i]);

		ext_wmi_reg_rule += num_6g_reg_rules_ap[i];
	}

	for (j = 0; j < WMI_REG_CURRENT_MAX_AP_TYPE; j++) {
		ath12k_dbg(ab, ATH12K_DBG_WMI,
                          "AP type %s", ath12k_6g_ap_type_to_str(j));

		for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
			reg_info->reg_rules_6g_client_ptr[j][i] =
				create_ext_reg_rules_from_wmi(num_6g_reg_rules_cl[j][i],
							      ext_wmi_reg_rule);

			if (!reg_info->reg_rules_6g_client_ptr[j][i]) {
				kfree(tb);
				ath12k_warn(ab, "Unable to Allocate memory for 6g client rules\n");
				return -ENOMEM;
			}

			ath12k_print_reg_rule(ab, ath12k_6g_client_type_to_str(i),
                                             num_6g_reg_rules_cl[j][i],
                                             reg_info->reg_rules_6g_client_ptr[j][i]);

			ext_wmi_reg_rule += num_6g_reg_rules_cl[j][i];
		}
	}

	reg_info->client_type = le32_to_cpu(ev->client_type);
	reg_info->rnr_tpe_usable = ev->rnr_tpe_usable;
	reg_info->unspecified_ap_usable = ev->unspecified_ap_usable;
	reg_info->domain_code_6g_ap[WMI_REG_INDOOR_AP] =
		le32_to_cpu(ev->domain_code_6g_ap_lpi);
	reg_info->domain_code_6g_ap[WMI_REG_STD_POWER_AP] =
		le32_to_cpu(ev->domain_code_6g_ap_sp);
	reg_info->domain_code_6g_ap[WMI_REG_VLP_AP] =
		le32_to_cpu(ev->domain_code_6g_ap_vlp);

	for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
		reg_info->domain_code_6g_client[WMI_REG_INDOOR_AP][i] =
			le32_to_cpu(ev->domain_code_6g_client_lpi[i]);
		reg_info->domain_code_6g_client[WMI_REG_STD_POWER_AP][i] =
			le32_to_cpu(ev->domain_code_6g_client_sp[i]);
		reg_info->domain_code_6g_client[WMI_REG_VLP_AP][i] =
			le32_to_cpu(ev->domain_code_6g_client_vlp[i]);
	}

	reg_info->domain_code_6g_super_id = le32_to_cpu(ev->domain_code_6g_super_id);

	ath12k_dbg(ab, ATH12K_DBG_WMI, "6g client type %s 6g super domain %s",
                  ath12k_6g_client_type_to_str(reg_info->client_type),
                  ath12k_super_reg_6g_to_str(reg_info->domain_code_6g_super_id));

	ath12k_dbg(ab, ATH12K_DBG_WMI, "processed regulatory ext channel list\n");

	kfree(tb);
	return 0;
}

static int ath12k_pull_peer_del_resp_ev(struct ath12k_base *ab, struct sk_buff *skb,
					struct wmi_peer_delete_resp_event *peer_del_resp)
{
	const void **tb;
	const struct wmi_peer_delete_resp_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PEER_DELETE_RESP_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch peer delete resp ev");
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

static int ath12k_pull_vdev_del_resp_ev(struct ath12k_base *ab,
					struct sk_buff *skb,
					u32 *vdev_id)
{
	const void **tb;
	const struct wmi_vdev_delete_resp_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_VDEV_DELETE_RESP_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch vdev delete resp ev");
		kfree(tb);
		return -EPROTO;
	}

	*vdev_id = le32_to_cpu(ev->vdev_id);

	kfree(tb);
	return 0;
}

static int ath12k_pull_bcn_tx_status_ev(struct ath12k_base *ab,
					struct sk_buff *skb,
					u32 *vdev_id, u32 *tx_status)
{
	const void **tb;
	const struct wmi_bcn_tx_status_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_OFFLOAD_BCN_TX_STATUS_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch bcn tx status ev");
		kfree(tb);
		return -EPROTO;
	}

	*vdev_id = le32_to_cpu(ev->vdev_id);
	*tx_status = le32_to_cpu(ev->tx_status);

	kfree(tb);
	return 0;
}

static int ath12k_pull_vdev_stopped_param_tlv(struct ath12k_base *ab, struct sk_buff *skb,
					      u32 *vdev_id)
{
	const void **tb;
	const struct wmi_vdev_stopped_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_VDEV_STOPPED_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch vdev stop ev");
		kfree(tb);
		return -EPROTO;
	}

	*vdev_id = le32_to_cpu(ev->vdev_id);

	kfree(tb);
	return 0;
}

static int ath12k_wmi_mgmt_rx_sub_tlv_parse(struct ath12k_base *ab,
					    u16 tag, u16 len,
					    const void *ptr, void *data)
{
	struct wmi_tlv_mgmt_rx_parse *parse = data;
	struct ath12k_mgmt_rx_cu_arg *rx_cu_params;
	struct ath12k_wmi_mgmt_rx_cu_params *rx_cu_params_tlv;

	switch (tag) {
	case WMI_TAG_MLO_MGMT_RX_CU_PARAMS:
		rx_cu_params = &parse->cu_params;
		rx_cu_params_tlv = (struct ath12k_wmi_mgmt_rx_cu_params *)ptr;
		rx_cu_params->cu_vdev_map[0] =
			le32_get_bits(rx_cu_params_tlv->cu_vdev_map_1, CU_VDEV_MAP_LB);
		rx_cu_params->cu_vdev_map[1] =
			le32_get_bits(rx_cu_params_tlv->cu_vdev_map_1, CU_VDEV_MAP_HB);
		rx_cu_params->cu_vdev_map[2] =
			le32_get_bits(rx_cu_params_tlv->cu_vdev_map_2, CU_VDEV_MAP_LB);
		rx_cu_params->cu_vdev_map[3] =
			le32_get_bits(rx_cu_params_tlv->cu_vdev_map_2, CU_VDEV_MAP_HB);
		rx_cu_params->cu_vdev_map[4] =
			le32_get_bits(rx_cu_params_tlv->cu_vdev_map_3, CU_VDEV_MAP_LB);
		rx_cu_params->cu_vdev_map[5] =
			le32_get_bits(rx_cu_params_tlv->cu_vdev_map_3, CU_VDEV_MAP_HB);
		rx_cu_params->cu_vdev_map[6] =
			le32_get_bits(rx_cu_params_tlv->cu_vdev_map_4, CU_VDEV_MAP_LB);
		rx_cu_params->cu_vdev_map[7] =
			le32_get_bits(rx_cu_params_tlv->cu_vdev_map_4, CU_VDEV_MAP_HB);
		parse->mgmt_ml_info_done = true;
		break;
	case WMI_TAG_MLO_LINK_REMOVAL_TBTT_COUNT:
		parse->link_removal_info[parse->num_link_removal_info_count] =
			(struct ath12k_wmi_mgmt_rx_mlo_link_removal_info *)ptr;
		parse->num_link_removal_info_count++;
		parse->parse_link_removal_info_done = true;
		break;
	case WMI_TAG_MLO_TID_TO_LINK_MAPPING_BCAST_T2LM_INFO:
		if (parse->num_bcast_ttlm_info_count < TARGET_NUM_VDEVS - 1) {
			parse->bcast_ttlm_info[parse->num_bcast_ttlm_info_count] =
				(struct ath12k_wmi_mgmt_rx_mlo_bcast_ttlm_info *)ptr;
			parse->num_bcast_ttlm_info_count++;
			parse->parse_bcast_ttlm_info_done = true;
		}
		break;
	}
	return 0;
}

static int ath12k_wmi_tlv_mgmt_rx_parse(struct ath12k_base *ab,
					u16 tag, u16 len,
					const void *ptr, void *data)
{
	struct wmi_tlv_mgmt_rx_parse *parse = data;
	int ret;

	switch (tag) {
	case WMI_TAG_MGMT_RX_HDR:
		parse->fixed = ptr;
		break;
	case WMI_TAG_ARRAY_BYTE:
		if (!parse->frame_buf_done) {
			parse->frame_buf = ptr;
			parse->frame_buf_done = true;
		} else if (!parse->bpcc_buf_done) {
			if (len == 0)
				break;
			parse->cu_params.bpcc_bufp = (u8*)ptr;
			parse->bpcc_buf_done = true;
		}
		break;
	case WMI_TAG_ARRAY_STRUCT:
		ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					  ath12k_wmi_mgmt_rx_sub_tlv_parse, parse);
		if (ret) {
			ath12k_warn(ab, "failed to parse mgmt rx sub tlv %d\n", ret);
			return ret;
		}
		break;
	}
	return 0;
}

static bool ath12k_get_ar_next_vdev_pos(struct ath12k *ar, u32 *pos)
{
	bool bit;
	u32 i = 0;

	for (i = *pos; i < ar->ab->num_max_vdev_supported; i++) {
		bit = ar->allocated_vdev_map & (1LL << i);
		if (bit) {
			*pos = i;
			return true;
		}
	}
	return false;
}

static void ath12k_update_cu_params(struct ath12k_base *ab,
				    struct ath12k_mgmt_rx_cu_arg *cu_params)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_link_vif *arvif, *tmp;
	struct ieee80211_vif *vif;
	u8 *bpcc_ptr, *bpcc_bufp;
	struct ath12k_hw *ah;
	u32 vdev_id, pos = 0;
	bool critical_flag;
	struct ath12k *ar;
	int num_hw, i, j;
	u16 vdev_map;

	if (!cu_params->bpcc_bufp)
		return;

	/* Iterate over all the radios */
	for (num_hw = 0; num_hw < ag->num_hw; num_hw++) {
		ah = ag->ah[num_hw];
		if (!ah)
			continue;
		for_each_ar(ah, ar, j) {
			ar = &ah->radio[j];

			if (test_bit(ATH12K_FLAG_RECOVERY, &ar->ab->dev_flags))
				continue;

			pos = 0;
			for (i = 0; i < ar->num_created_vdevs; i++) {
				if (!ath12k_get_ar_next_vdev_pos(ar, &pos)) {
					pos++;
					continue;
				}
				vdev_id = pos;
				pos++;
				spin_lock_bh(&ar->data_lock);
				list_for_each_entry_safe(arvif, tmp, &ar->arvifs, list) {
					if (arvif->vdev_id != vdev_id ||
					    ath12k_mac_is_bridge_vdev(arvif))
						continue;
					vif = arvif->ahvif->vif;
					if (arvif->is_up && vif->valid_links) {
						vdev_map = cu_params->cu_vdev_map[num_hw];
						critical_flag = vdev_map & (1 << i);
						bpcc_bufp = cu_params->bpcc_bufp;
						bpcc_ptr = bpcc_bufp +
						    ((num_hw * MAX_AP_MLDS_PER_LINK) + i);
						ieee80211_critical_update(vif,
									  arvif->link_id,
									  critical_flag,
									  *bpcc_ptr);
						break;
					}
				}
				spin_unlock_bh(&ar->data_lock);
			}
		}
	}
}

static void
ath12k_update_link_removal_params(struct ath12k_base *ab,
				  const struct ath12k_mgmt_rx_mlo_link_removal_info *params,
				  u32 num_link_removal_params)
{
	struct ath12k_hw_group *ag = ath12k_ab_to_ag(ab);
	struct ath12k_hw *ah;
	struct ath12k *ar, *target_ar = NULL;
	struct ath12k_link_vif *arvif;
	const struct ath12k_mgmt_rx_mlo_link_removal_info *info;
	u32 i, j, num_hw;

	/**
	 * If a broadcast probe request is received on a given pdev, FW sends MLO link
	 * removal information for all AP MLDs which satisfy both conditions below
	 *      1) The AP MLD has one APs being removed.
	 *      2) The AP MLD has a BSS on the pdev on which the broadcast probe request is received
	 */
	for (i = 0; i < num_link_removal_params; i++) {
		info = &params[i];

		if (info->hw_link_id > ATH12K_GROUP_MAX_RADIO) {
			ath12k_warn(ab, "Wrong hw_link_id received:%d\n",
				    info->hw_link_id);
			continue;
		}

		for (num_hw = 0; num_hw < ag->num_hw; num_hw++) {
			ah = ath12k_ag_to_ah(ag, num_hw);
			if (!ah)
				continue;
			for_each_ar(ah, ar, j) {
				if (ar->hw_link_id == info->hw_link_id)
					target_ar = ar;
			}
		}
		if (!target_ar) {
			ath12k_warn(ab, "Couldn't fetch hw links for hw_link_id:%d\n",
				    info->hw_link_id);
			continue;
		}

		arvif = ath12k_mac_get_arvif(target_ar, info->vdev_id);
		if (!arvif) {
			ath12k_err(ab, "Error in getting arvif from vdev id:%d info link:%d\n",
				   info->vdev_id, info->hw_link_id);
			continue;
		}

		/* update mac80211 only if tbtt_count is greater than 0 */
		if (arvif->is_up && arvif->ahvif->vif->valid_links && info->tbtt_count)
			ieee80211_link_removal_count_update(arvif->ahvif->vif,
							    arvif->link_id,
							    info->tbtt_count);
	}
}

static void ath12k_wmi_update_ml_link_removal_info_count(struct ath12k_base *ab,
							 struct ath12k_wmi_mgmt_rx_arg *hdr,
							 struct wmi_tlv_mgmt_rx_parse *parse)
{
	u32 tbtt_val;
	int idx;

	hdr->num_link_removal_info = parse->num_link_removal_info_count;

	for (idx = 0; idx < hdr->num_link_removal_info; idx++) {
		tbtt_val = le32_to_cpu(parse->link_removal_info[idx]->tbtt_info);

		hdr->link_removal_info[idx].vdev_id =
			le16_get_bits(tbtt_val,
				      WMI_MGMT_RX_MLO_LINK_REMOVAL_INFO_VDEV_ID_GET);
		hdr->link_removal_info[idx].hw_link_id =
			le16_get_bits(tbtt_val,
				      WMI_MGMT_RX_MLO_LINK_REMOVAL_INFO_HW_LINK_ID_GET);
		hdr->link_removal_info[idx].tbtt_count =
			le32_get_bits(tbtt_val,
				      WMI_MGMT_RX_MLO_LINK_REMOVAL_INFO_TBTT_COUNT_GET);
	}
}

static void ath12k_wmi_update_ml_bcast_ttlm_info_params(struct ath12k_base *ab,
							struct ath12k_wmi_mgmt_rx_arg *hdr,
							struct wmi_tlv_mgmt_rx_parse *parse)
{
	struct ath12k_wmi_mgmt_rx_mlo_bcast_ttlm_info *ttlm;
	int idx;

	hdr->num_bcast_ttlm_info = parse->num_bcast_ttlm_info_count;

	for (idx = 0; idx < hdr->num_bcast_ttlm_info; idx++) {
		ttlm = parse->bcast_ttlm_info[idx];

		hdr->bcast_ttlm_info[idx].vdev_id =
			le32_get_bits(ttlm->ttlm_info,
				      WMI_MGMT_RX_MLO_BCAST_TTLM_INFO_VDEV_ID_GET);
		hdr->bcast_ttlm_info[idx].expec_dur =
			le32_get_bits(ttlm->ttlm_info,
				      WMI_MGMT_RX_MLO_BCAST_TTLM_INFO_EXPEC_DUR_GET);
	}
}

static int ath12k_pull_mgmt_rx_params_tlv(struct ath12k_base *ab,
					  struct sk_buff *skb,
					  struct ath12k_wmi_mgmt_rx_arg *hdr)
{
	struct wmi_tlv_mgmt_rx_parse parse = { };
	const struct ath12k_wmi_mgmt_rx_params *ev;
	const u8 *frame;
	int i, ret;

	memset(&parse, 0, sizeof(parse));
	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_mgmt_rx_parse,
				  &parse);
	if (ret) {
		ath12k_warn(ab, "failed to parse mgmt rx tlv %d\n", ret);
		return ret;
	}

	ev = parse.fixed;
	frame = parse.frame_buf;

	if (!ev || !frame) {
		ath12k_warn(ab, "failed to fetch mgmt rx hdr");
		return -EPROTO;
	}

	hdr->pdev_id = le32_to_cpu(ev->pdev_id);
	hdr->chan_freq = le32_to_cpu(ev->chan_freq);
	hdr->channel = le32_to_cpu(ev->channel);
	hdr->snr = le32_to_cpu(ev->snr);
	hdr->rate = le32_to_cpu(ev->rate);
	hdr->phy_mode = le32_to_cpu(ev->phy_mode);
	hdr->buf_len = le32_to_cpu(ev->buf_len);
	hdr->status = le32_to_cpu(ev->status);
	hdr->flags = le32_to_cpu(ev->flags);
	hdr->rssi = a_sle32_to_cpu(ev->rssi);
	hdr->tsf_delta = le32_to_cpu(ev->tsf_delta);

	for (i = 0; i < ATH_MAX_ANTENNA; i++)
		hdr->rssi_ctl[i] = le32_to_cpu(ev->rssi_ctl[i]);

	if (parse.mgmt_ml_info_done) {
		rcu_read_lock();
		ath12k_update_cu_params(ab, &parse.cu_params);
		rcu_read_unlock();
	}

	if (skb->len < (frame - skb->data) + hdr->buf_len) {
		ath12k_warn(ab, "invalid length in mgmt rx hdr ev");
		return -EPROTO;
	}

	/* ML link removal info TLV */
	if (parse.num_link_removal_info_count)
		ath12k_wmi_update_ml_link_removal_info_count(ab, hdr, &parse);

	/* Bcast TTLM info TLV */
	if (parse.num_bcast_ttlm_info_count)
		ath12k_wmi_update_ml_bcast_ttlm_info_params(ab, hdr, &parse);

	/* shift the sk_buff to point to `frame` */
	skb_trim(skb, 0);
	skb_put(skb, frame - skb->data);
	skb_pull(skb, frame - skb->data);
	skb_put(skb, hdr->buf_len);

	return 0;
}

static int wmi_process_mgmt_tx_comp(struct ath12k *ar, u32 desc_id,
				    u32 status,
				    u32 ack_rssi)
{
	struct sk_buff *msdu;
	struct ieee80211_tx_info *info;
	struct ath12k_skb_cb *skb_cb;
	struct ieee80211_hdr *hdr;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	struct ath12k_mgmt_frame_stats *mgmt_stats;
	u16 frm_stype;
	int num_mgmt;

	spin_lock_bh(&ar->data_lock);
	spin_lock_bh(&ar->txmgmt_idr_lock);
	msdu = idr_find(&ar->txmgmt_idr, desc_id);

	if (!msdu) {
		ath12k_warn(ar->ab, "received mgmt tx compl for invalid msdu_id: %d\n",
			    desc_id);
		spin_unlock_bh(&ar->txmgmt_idr_lock);
		spin_unlock_bh(&ar->data_lock);
		return -ENOENT;
	}

	idr_remove(&ar->txmgmt_idr, desc_id);
	spin_unlock_bh(&ar->txmgmt_idr_lock);

	skb_cb = ATH12K_SKB_CB(msdu);
	ath12k_core_dma_unmap_single(ar->ab->dev, skb_cb->paddr, msdu->len, DMA_TO_DEVICE);

	hdr = (struct ieee80211_hdr *)msdu->data;

	if (ieee80211_is_mgmt(hdr->frame_control)) {
		frm_stype = FIELD_GET(IEEE80211_FCTL_STYPE, hdr->frame_control);
		vif = skb_cb->vif;
		if (ATH12K_MGMT_MLME_FRAME(hdr->frame_control))
			ath12k_dbg(ar->ab, ATH12K_DBG_MLME, "Tx completion for %s frame to STA %pM status %u\n",
				   mgmt_frame_name[frm_stype], hdr->addr1, status);

		if (!vif) {
			ath12k_warn(ar->ab, "failed to find vif to update txcompl mgmt stats\n");
			goto skip_mgmt_stats;
		}

	        ahvif = ath12k_vif_to_ahvif(vif);
		mgmt_stats = &ahvif->mgmt_stats;

		if (!status)
			mgmt_stats->tx_compl_succ[frm_stype]++;
		else
			mgmt_stats->tx_compl_fail[frm_stype]++;
	}

skip_mgmt_stats:
	spin_unlock_bh(&ar->data_lock);

	info = IEEE80211_SKB_CB(msdu);
	memset(&info->status, 0, sizeof(info->status));

	info->status.rates[0].idx = -1;

	if ((!(info->flags & IEEE80211_TX_CTL_NO_ACK)) && !status) {
		info->flags |= IEEE80211_TX_STAT_ACK;
		info->status.ack_signal = ack_rssi;
		info->status.flags |= IEEE80211_TX_STATUS_ACK_SIGNAL_VALID;
	}

	if ((info->flags & IEEE80211_TX_CTL_NO_ACK) && !status)
		info->flags |= IEEE80211_TX_STAT_NOACK_TRANSMITTED;

	ieee80211_tx_status_irqsafe(ath12k_ar_to_hw(ar), msdu);

	num_mgmt = atomic_dec_if_positive(&ar->num_pending_mgmt_tx);

	/* WARN when we received this event without doing any mgmt tx */
	if (num_mgmt < 0)
		WARN_ON_ONCE(1);

	if (!num_mgmt)
		wake_up(&ar->txmgmt_empty_waitq);

	return 0;
}

static int ath12k_pull_mgmt_tx_compl_param_tlv(struct ath12k_base *ab,
					       struct sk_buff *skb,
					       struct wmi_mgmt_tx_compl_event *param)
{
	const void **tb;
	const struct wmi_mgmt_tx_compl_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_MGMT_TX_COMPL_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch mgmt tx compl ev");
		kfree(tb);
		return -EPROTO;
	}

	param->pdev_id = ev->pdev_id;
	param->desc_id = ev->desc_id;
	param->status = ev->status;
	param->ppdu_id = ev->ppdu_id;
	param->ack_rssi = ev->ack_rssi;

	kfree(tb);
	return 0;
}

static void wmi_process_offchan_tx_comp(struct ath12k *ar, u32 desc_id,
					u32 status)
{
	struct sk_buff *msdu;
	struct ath12k_skb_cb *skb_cb;
	struct ieee80211_tx_info *info;

	spin_lock_bh(&ar->data_lock);
	spin_lock_bh(&ar->txmgmt_idr_lock);
	msdu = idr_find(&ar->txmgmt_idr, desc_id);

	if (!msdu) {
		spin_unlock_bh(&ar->txmgmt_idr_lock);
		spin_unlock_bh(&ar->data_lock);
		ath12k_warn(ar->ab, "received offchan tx compl for invalid msdu_id: %d\n",
			    desc_id);
		return;
	}

	idr_remove(&ar->txmgmt_idr, desc_id);
	spin_unlock_bh(&ar->txmgmt_idr_lock);

	skb_cb = ATH12K_SKB_CB(msdu);
	dma_unmap_single(ar->ab->dev, skb_cb->paddr, msdu->len, DMA_TO_DEVICE);

	spin_unlock_bh(&ar->data_lock);

	info = IEEE80211_SKB_CB(msdu);
	if (!(info->flags & IEEE80211_TX_CTL_NO_ACK) && !status)
		info->flags |= IEEE80211_TX_STAT_ACK;

	ieee80211_tx_status_irqsafe(ar->ah->hw, msdu);
}

static int ath12k_pull_offchan_tx_compl_param_tlv(struct ath12k_base *ab,
						  struct sk_buff *skb,
						  struct wmi_offchan_data_tx_compl_event
						  *params)
{
	const void **tb;
	const struct wmi_offchan_data_tx_compl_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_OFFCHAN_DATA_TX_COMPL_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch offchan tx compl ev\n");
		kfree(tb);
		return -EPROTO;
	}

	*params = *ev;

	kfree(tb);
	return 0;
}

static void ath12k_wmi_event_scan_started(struct ath12k *ar)
{
	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		ath12k_warn(ar->ab, "received scan started event in an invalid scan state: %s (%d)\n",
			    ath12k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH12K_SCAN_STARTING:
		ar->scan.state = ATH12K_SCAN_RUNNING;

		if (ar->scan.is_roc)
			ieee80211_ready_on_channel(ath12k_ar_to_hw(ar));

		complete(&ar->scan.started);
		break;
	}
}

static void ath12k_wmi_event_scan_start_failed(struct ath12k *ar)
{
	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		ath12k_warn(ar->ab, "received scan start failed event in an invalid scan state: %s (%d)\n",
			    ath12k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH12K_SCAN_STARTING:
		complete(&ar->scan.started);
		__ath12k_mac_scan_finish(ar);
		break;
	}
}

static void ath12k_wmi_event_scan_completed(struct ath12k *ar)
{
	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
	case ATH12K_SCAN_STARTING:
		/* One suspected reason scan can be completed while starting is
		 * if firmware fails to deliver all scan events to the host,
		 * e.g. when transport pipe is full. This has been observed
		 * with spectral scan phyerr events starving wmi transport
		 * pipe. In such case the "scan completed" event should be (and
		 * is) ignored by the host as it may be just firmware's scan
		 * state machine recovering.
		 */
		ath12k_warn(ar->ab, "received scan completed event in an invalid scan state: %s (%d)\n",
			    ath12k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		__ath12k_mac_scan_finish(ar);
		break;
	}
}

static void ath12k_wmi_event_scan_bss_chan(struct ath12k *ar, u32 scan_id, u32 freq)
{
	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
	case ATH12K_SCAN_STARTING:
		ath12k_warn(ar->ab, "received scan bss chan event in an invalid scan state: %s (%d)\n",
			    ath12k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		/*In order to support off channel tx/rx along with off channel scan,
		 * scan vdev is created and started to do the tx. once we start the scan
		 * driver will send the ROC scan request to FW. Due to this, the first
		 * scan request is not treated as off channel scan in FW. So it
		 * sends bss scan event instead of foreign channel scan
		 */
		if (scan_id ==  ATH12K_ROC_SCAN_ID) {
			ar->scan_channel = ieee80211_get_channel(ar->ah->hw->wiphy,
								 freq);
			if (ar->scan.is_roc && ar->scan.roc_freq == freq)
				complete(&ar->scan.on_channel);
		} else {
			ar->scan_channel = NULL;
		}
		break;
	}
}

static void ath12k_wmi_event_scan_foreign_chan(struct ath12k *ar, u32 freq)
{
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);

	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
	case ATH12K_SCAN_STARTING:
		ath12k_warn(ar->ab, "received scan foreign chan event in an invalid scan state: %s (%d)\n",
			    ath12k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		ar->scan_channel = ieee80211_get_channel(hw->wiphy, freq);

		if (ar->scan.is_roc && ar->scan.roc_freq == freq)
			complete(&ar->scan.on_channel);

		break;
	}
}

static const char *
ath12k_wmi_event_scan_type_str(enum wmi_scan_event_type type,
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

static int ath12k_pull_scan_ev(struct ath12k_base *ab, struct sk_buff *skb,
			       struct wmi_scan_event *scan_evt_param)
{
	const void **tb;
	const struct wmi_scan_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_SCAN_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch scan ev");
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

static int ath12k_pull_peer_sta_kickout_ev(struct ath12k_base *ab, struct sk_buff *skb,
					   struct wmi_peer_sta_kickout_arg *arg)
{
	const void **tb;
	const struct wmi_peer_sta_kickout_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PEER_STA_KICKOUT_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch peer sta kickout ev");
		kfree(tb);
		return -EPROTO;
	}

	arg->mac_addr = ev->peer_macaddr.addr;
	arg->reason = ev->reason;
	arg->rssi = ev->rssi;

	kfree(tb);
	return 0;
}

static int ath12k_pull_roam_ev(struct ath12k_base *ab, struct sk_buff *skb,
			       struct wmi_roam_event *roam_ev)
{
	const void **tb;
	const struct wmi_roam_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_ROAM_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch roam ev");
		kfree(tb);
		return -EPROTO;
	}

	roam_ev->vdev_id = ev->vdev_id;
	roam_ev->reason = ev->reason;
	roam_ev->rssi = ev->rssi;

	kfree(tb);
	return 0;
}

static int freq_to_idx(struct ath12k *ar, int freq)
{
	struct ieee80211_supported_band *sband;
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);
	int band, ch, idx = 0;

	for (band = NL80211_BAND_2GHZ; band < NUM_NL80211_BANDS; band++) {
		if (!ar->mac.sbands[band].channels)
			continue;

		sband = hw->wiphy->bands[band];
		if (!sband)
			continue;

		for (ch = 0; ch < sband->n_channels; ch++, idx++)
			if (sband->channels[ch].center_freq == freq)
				goto exit;
	}

exit:
	return idx;
}

static int ath12k_pull_chan_info_ev(struct ath12k_base *ab, struct sk_buff *skb,
				    struct wmi_chan_info_event *ch_info_ev)
{
	const void **tb;
	const struct wmi_chan_info_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_CHAN_INFO_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch chan info ev");
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
ath12k_pull_pdev_bss_chan_info_ev(struct ath12k_base *ab, struct sk_buff *skb,
				  struct wmi_pdev_bss_chan_info_event *bss_ch_info_ev)
{
	const void **tb;
	const struct wmi_pdev_bss_chan_info_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PDEV_BSS_CHAN_INFO_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch pdev bss chan info ev");
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
ath12k_pull_vdev_install_key_compl_ev(struct ath12k_base *ab, struct sk_buff *skb,
				      struct wmi_vdev_install_key_complete_arg *arg)
{
	const void **tb;
	const struct wmi_vdev_install_key_compl_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_VDEV_INSTALL_KEY_COMPLETE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch vdev install key compl ev");
		kfree(tb);
		return -EPROTO;
	}

	arg->vdev_id = le32_to_cpu(ev->vdev_id);
	arg->macaddr = ev->peer_macaddr.addr;
	arg->key_idx = le32_to_cpu(ev->key_idx);
	arg->key_flags = le32_to_cpu(ev->key_flags);
	arg->status = le32_to_cpu(ev->status);

	kfree(tb);
	return 0;
}

static int ath12k_pull_peer_assoc_conf_ev(struct ath12k_base *ab, struct sk_buff *skb,
					  struct wmi_peer_assoc_conf_arg *peer_assoc_conf)
{
	const void **tb;
	const struct wmi_peer_assoc_conf_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PEER_ASSOC_CONF_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch peer assoc conf ev");
		kfree(tb);
		return -EPROTO;
	}

	peer_assoc_conf->vdev_id = le32_to_cpu(ev->vdev_id);
	peer_assoc_conf->macaddr = ev->peer_macaddr.addr;
	peer_assoc_conf->status = le32_to_cpu(ev->status);

	kfree(tb);
	return 0;
}

static void ath12k_wmi_op_ep_tx_credits(struct ath12k_base *ab)
{
	/* try to send pending beacons first. they take priority */
	wake_up(&ab->wmi_ab.tx_credits_wq);
}

static int ath12k_reg_11d_new_cc_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	const struct wmi_11d_new_cc_event *ev;
	struct ath12k *ar;
	struct ath12k_pdev *pdev;
	const void **tb;
	int ret, i;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_11D_NEW_COUNTRY_EVENT];
	if (!ev) {
		kfree(tb);
		ath12k_warn(ab, "failed to fetch 11d new cc ev");
		return -EPROTO;
	}

	spin_lock_bh(&ab->base_lock);
	memcpy(&ab->new_alpha2, &ev->new_alpha2, REG_ALPHA2_LEN);
	spin_unlock_bh(&ab->base_lock);

	ath12k_dbg(ab, ATH12K_DBG_WMI, "wmi 11d new cc %c%c\n",
		   ab->new_alpha2[0],
		   ab->new_alpha2[1]);

	kfree(tb);

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		ar = pdev->ar;
		ar->state_11d = ATH12K_11D_IDLE;
		complete(&ar->completed_11d_scan);
	}

	queue_work(ab->workqueue, &ab->update_11d_work);

	return 0;
}

static void ath12k_wmi_htc_tx_complete(struct ath12k_base *ab,
				       struct sk_buff *skb)
{
	struct ath12k_wmi_pdev *wmi = NULL;
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct wmi_cmd_hdr *cmd_hdr;
	enum wmi_tlv_cmd_id id;
	u32 i;
	u8 wmi_ep_count;
	u8 eid;

	eid = skb_cb->u.eid;
	skb_pull(skb, sizeof(struct ath12k_htc_hdr));
	cmd_hdr = (struct wmi_cmd_hdr *)skb->data;
	id = le32_get_bits(cmd_hdr->cmd_id, WMI_CMD_HDR_CMD_ID);
	dev_kfree_skb(skb);

	if (eid >= ATH12K_HTC_EP_COUNT)
		return;

	wmi_ep_count = ab->htc.wmi_ep_count;
	if (wmi_ep_count > ab->hw_params->max_radios)
		return;

	for (i = 0; i < ab->htc.wmi_ep_count; i++) {
		if (ab->wmi_ab.wmi[i].eid == eid) {
			wmi = &ab->wmi_ab.wmi[i];
			break;
		}
	}

	if (wmi) {
		WMI_COMMAND_TX_CMP_RECORD(wmi, id);
		wake_up(&wmi->tx_ce_desc_wq);
	}
}

static inline bool
ath12k_is_regd_alpha2_global_mode(const struct ieee80211_regdomain *regd)
{
	if (!regd)
		return false;

	return !strncmp(regd->alpha2, "00", 2);
}

static int ath12k_reg_handle_chan_list(struct ath12k_base *ab,
				       struct ath12k_reg_info *reg_info,
				       enum ieee80211_ap_reg_power power_type)
{
	const struct ieee80211_regdomain *wiphy_regd = NULL;
	struct ieee80211_regdomain *regd;
	int pdev_idx;
	struct ath12k *ar;

	if (reg_info->status_code != REG_SET_CC_STATUS_PASS) {
		/* In case of failure to set the requested ctry,
		 * fw retains the current regd. We print a failure info
		 * and return from here.
		 */
		ath12k_warn(ab, "Failed to set the requested Country regulatory setting\n");
		return -EINVAL;
	}

	pdev_idx = reg_info->phy_id;

	if (pdev_idx >= ab->num_radios) {
		/* Process the event for phy0 only if single_pdev_only
		 * is true. If pdev_idx is valid but not 0, discard the
		 * event. Otherwise, it goes to fallback.
		 */
		if (ab->hw_params->single_pdev_only &&
		    pdev_idx < ab->hw_params->num_rxdma_per_pdev)
			goto retfail;
		else
			goto fallback;
	}

	ar = ab->pdevs[pdev_idx].ar;
	rcu_read_lock();
	if (ar)
		wiphy_regd = rcu_dereference(ar->ah->hw->wiphy->regd);

	/* Avoid multiple overwrites to default regd, during core
	 * stop-start after mac registration. Also, add an exception when
	 * the current regd in wiphy is 00.
	 */
	if (!ath12k_is_regd_alpha2_global_mode(wiphy_regd) &&
	    ab->default_regd[pdev_idx] && !ab->new_regd[pdev_idx] &&
	    !memcmp(ab->default_regd[pdev_idx]->alpha2,
		    reg_info->alpha2, 2)) {
		rcu_read_unlock();
		goto retfail;
	}

	rcu_read_unlock();

	regd = ath12k_reg_build_regd(ab, reg_info);
	if (!regd) {
		ath12k_warn(ab, "failed to build regd from reg_info\n");
		goto fallback;
	}

	if (ar && ar->supports_6ghz) {
		ath12k_dbg(ab, ATH12K_DBG_REG,
			   "Freeing AFC info for pdev %d\n", pdev_idx);
		spin_lock_bh(&ar->data_lock);
		ath12k_free_afc_power_event_info(&ar->afc);
		spin_unlock_bh(&ar->data_lock);
	}

	spin_lock_bh(&ab->base_lock);
	if (test_bit(ATH12K_FLAG_REGISTERED, &ab->dev_flags)) {
		/* Once mac is registered, ar is valid and all CC events from
		 * fw is considered to be received due to user requests
		 * currently.
		 * Free previously built regd before assigning the newly
		 * generated regd to ar. NULL pointer handling will be
		 * taken care by kfree itself.
		 */
		kfree(ab->new_regd[pdev_idx]);
		ab->new_regd[pdev_idx] = regd;
		queue_work(ab->workqueue, &ar->regd_update_work);
	} else {
		/* Multiple events for the same *ar is not expected. But we
		 * can still clear any previously stored default_regd if we
		 * are receiving this event for the same radio by mistake.
		 * NULL pointer handling will be taken care by kfree itself.
		 */
		kfree(ab->default_regd[pdev_idx]);
		/* This regd would be applied during mac registration */
		ab->default_regd[pdev_idx] = regd;
	}

	ab->regd_freed = false;
	ab->dfs_region = reg_info->dfs_region;
	spin_unlock_bh(&ab->base_lock);

	return 0;

fallback:
	/* Fallback to older reg (by sending previous country setting
	 * again if fw has succeeded and we failed to process here.
	 * The Regdomain should be uniform across driver and fw. Since the
	 * FW has processed the command and sent a success status, we expect
	 * this function to succeed as well. If it doesn't, CTRY needs to be
	 * reverted at the fw and the old SCAN_CHAN_LIST cmd needs to be sent.
	 */
	/* TODO: This is rare, but still should also be handled */
	WARN_ON(1);
retfail:
        return -EINVAL;
}

static int ath12k_reg_chan_list_event(struct ath12k_base *ab, struct sk_buff *skb)
{
       struct ath12k_reg_info *reg_info;
       int ret, i, j;

       reg_info = kzalloc(sizeof(*reg_info), GFP_ATOMIC);
       if (!reg_info)
               return -ENOMEM;

       ret = ath12k_pull_reg_chan_list_ext_update_ev(ab, skb, reg_info);
       if (ret) {
               ath12k_warn(ab, "failed to extract regulatory info from received event\n");
               goto mem_free;
       }

       ret = ath12k_reg_handle_chan_list(ab, reg_info, IEEE80211_REG_UNSET_AP);
       if (ret) {
               ath12k_warn(ab, "failed to process regulatory info from received event\n");
               goto mem_free;
       }
mem_free:
	if (reg_info) {
		kfree(reg_info->reg_rules_2g_ptr);
		kfree(reg_info->reg_rules_5g_ptr);
		if (reg_info->is_ext_reg_event) {
			for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++) {
				kfree(reg_info->reg_rules_6g_ap_ptr[i]);
				for (j = 0; j < WMI_REG_MAX_CLIENT_TYPE; j++)
                                	kfree(reg_info->reg_rules_6g_client_ptr[i][j]);
                       }
		}
		kfree(reg_info);
	}
	return ret;
}

static int ath12k_wmi_rdy_parse(struct ath12k_base *ab, u16 tag, u16 len,
				const void *ptr, void *data)
{
	struct ath12k_wmi_rdy_parse *rdy_parse = data;
	struct wmi_ready_event fixed_param;
	struct ath12k_wmi_mac_addr_params *addr_list;
	struct ath12k_pdev *pdev;
	u32 num_mac_addr;
	int i;

	switch (tag) {
	case WMI_TAG_READY_EVENT:
		memset(&fixed_param, 0, sizeof(fixed_param));
		memcpy(&fixed_param, (struct wmi_ready_event *)ptr,
		       min_t(u16, sizeof(fixed_param), len));
		ab->wlan_init_status = le32_to_cpu(fixed_param.ready_event_min.status);
		rdy_parse->num_extra_mac_addr =
			le32_to_cpu(fixed_param.ready_event_min.num_extra_mac_addr);

		ether_addr_copy(ab->mac_addr,
				fixed_param.ready_event_min.mac_addr.addr);
		ab->pktlog_defs_checksum = le32_to_cpu(fixed_param.pktlog_defs_checksum);
		ab->wmi_ready = true;
		ab->max_ml_peer_supported = fixed_param.max_num_ml_peers;
		break;
	case WMI_TAG_ARRAY_FIXED_STRUCT:
		addr_list = (struct ath12k_wmi_mac_addr_params *)ptr;
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

static int ath12k_ready_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k_wmi_rdy_parse rdy_parse = { };
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_rdy_parse, &rdy_parse);
	if (ret) {
		ath12k_warn(ab, "failed to parse tlv %d\n", ret);
		return ret;
	}

	complete(&ab->wmi_ab.unified_ready);
	return 0;
}

static void ath12k_peer_delete_resp_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_peer_delete_resp_event peer_del_resp;
	struct ath12k *ar;

	if (ath12k_pull_peer_del_resp_ev(ab, skb, &peer_del_resp) != 0) {
		ath12k_warn(ab, "failed to extract peer delete resp");
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, le32_to_cpu(peer_del_resp.vdev_id));
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in peer delete resp ev %d",
			    peer_del_resp.vdev_id);
		rcu_read_unlock();
		return;
	}

	complete(&ar->peer_delete_done);
	rcu_read_unlock();
	ath12k_dbg(ab, ATH12K_DBG_PEER | ATH12K_DBG_MLME,
		   "peer delete resp for vdev id %d addr %pM\n",
		   peer_del_resp.vdev_id, peer_del_resp.peer_macaddr.addr);
}

static void ath12k_vdev_delete_resp_event(struct ath12k_base *ab,
					  struct sk_buff *skb)
{
	struct ath12k *ar;
	u32 vdev_id = 0;

	if (ath12k_pull_vdev_del_resp_ev(ab, skb, &vdev_id) != 0) {
		ath12k_warn(ab, "failed to extract vdev delete resp");
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in vdev delete resp ev %d",
			    vdev_id);
		rcu_read_unlock();
		return;
	}

	complete(&ar->vdev_delete_done);

	rcu_read_unlock();

	ath12k_dbg(ab, ATH12K_DBG_WMI, "vdev delete resp for vdev id %d\n",
		   vdev_id);
}

static const char *ath12k_wmi_vdev_resp_print(u32 vdev_resp_status)
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

static void ath12k_vdev_start_resp_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_vdev_start_resp_event vdev_start_resp;
	struct ath12k *ar;
	u32 status;

	if (ath12k_pull_vdev_start_resp_tlv(ab, skb, &vdev_start_resp) != 0) {
		ath12k_warn(ab, "failed to extract vdev start resp");
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, le32_to_cpu(vdev_start_resp.vdev_id));
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in vdev start resp ev %d",
			    vdev_start_resp.vdev_id);
		rcu_read_unlock();
		return;
	}

	ar->last_wmi_vdev_start_status = 0;
	ar->max_allowed_tx_power = vdev_start_resp.max_allowed_tx_power;

	status = le32_to_cpu(vdev_start_resp.status);

	if (WARN_ON_ONCE(status)) {
		ath12k_warn(ab, "vdev start resp error status %d (%s)\n",
			    status, ath12k_wmi_vdev_resp_print(status));
		ar->last_wmi_vdev_start_status = status;
	}

	complete(&ar->vdev_setup_done);

	rcu_read_unlock();

	ath12k_dbg(ab, ATH12K_DBG_WMI, "vdev start resp for vdev id %d",
		   vdev_start_resp.vdev_id);
}

static void ath12k_bcn_tx_status_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k *ar;
	struct ath12k_link_vif *arvif;
	u32 vdev_id, tx_status, found = 0;

	if (ath12k_pull_bcn_tx_status_ev(ab, skb, &vdev_id, &tx_status) != 0) {
		ath12k_warn(ab, "failed to extract bcn tx status");
		return;
	}

	ar = ath12k_mac_get_ar_by_vdev_id(ab, vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in bcn tx status event %d",
			    vdev_id);
		return;
	}

	spin_lock_bh(&ar->data_lock);
	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (ath12k_mac_is_bridge_vdev(arvif))
			continue;
		if (vdev_id == arvif->vdev_id) {
			found = 1;
			break;
		}
	}
	spin_unlock_bh(&ar->data_lock);

	if (!found) {
		ath12k_warn(ab, "vdev not found in arlist");
		return;
	}

	if (arvif->is_up && arvif->ar)
		wiphy_work_queue(ath12k_ar_to_hw(arvif->ar)->wiphy,
				 &arvif->update_bcn_tx_status_work);
}

static void ath12k_vdev_stopped_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k *ar;
	u32 vdev_id = 0;

	if (ath12k_pull_vdev_stopped_param_tlv(ab, skb, &vdev_id) != 0) {
		ath12k_warn(ab, "failed to extract vdev stopped event");
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in vdev stopped ev %d",
			    vdev_id);
		rcu_read_unlock();
		return;
	}

	complete(&ar->vdev_setup_done);

	rcu_read_unlock();

	ath12k_dbg(ab, ATH12K_DBG_WMI, "vdev stopped for vdev id %d", vdev_id);
}

static void
ath12k_update_bcast_ttlm_params(struct ath12k_base *ab,
				const struct ath12k_mgmt_rx_mlo_bcast_ttlm_info *params,
				u32 num_bcast_ttlm_params)
{
	struct ath12k_link_vif *arvif;
	const struct ath12k_mgmt_rx_mlo_bcast_ttlm_info *info;
	u32 i;

	for (i = 0; i < num_bcast_ttlm_params; i++) {
		u32 vdev_id, expec_dur;

		info = &params[i];
		vdev_id = le32_to_cpu(info->vdev_id);
		expec_dur = le32_to_cpu(info->expec_dur);
		arvif = ath12k_mac_get_arvif_by_vdev_id(ab, vdev_id);
		if (!arvif) {
			ath12k_err(ab, "Error in getting arvif from vdev id:%d\n",
				   info->vdev_id);
			continue;
		}

		/* update mac80211 only if expec_dur is greater than 0 */
		if (arvif->is_up && arvif->ahvif->vif->valid_links && expec_dur)
			ieee80211_ttlm_info_expec_dur_update(arvif->ahvif->vif,
							     arvif->link_id,
							     expec_dur);
	}
}

static void ath12k_mgmt_rx_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k_wmi_mgmt_rx_arg rx_ev = {0};
	struct ath12k *ar;
	struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(skb);
	struct ieee80211_sta *pubsta = NULL;
	struct ath12k_link_sta *arsta;
	bool is_4addr_null_pkt = false;
	struct ieee80211_hdr *hdr;
	u16 fc;
	struct ieee80211_supported_band *sband;
	struct ath12k_dp_link_peer *peer;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	struct ath12k_mgmt_frame_stats *mgmt_stats;
	u16 frm_stype;
	struct ath12k_dp *dp;

	rx_ev.num_link_removal_info = 0;
	if (ath12k_pull_mgmt_rx_params_tlv(ab, skb, &rx_ev) != 0) {
		ath12k_warn(ab, "failed to extract mgmt rx event");
		dev_kfree_skb(skb);
		return;
	}

	memset(status, 0, sizeof(*status));

	ath12k_dbg(ab, ATH12K_DBG_MGMT, "mgmt rx event status %08x\n",
		   rx_ev.status);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, rx_ev.pdev_id);

	if (!ar) {
		ath12k_warn(ab, "invalid pdev_id %d in mgmt_rx_event\n",
			    rx_ev.pdev_id);
		dev_kfree_skb(skb);
		goto exit;
	}

	if ((test_bit(ATH12K_FLAG_CAC_RUNNING, &ar->dev_flags)) ||
	    (rx_ev.status & (WMI_RX_STATUS_ERR_DECRYPT |
			     WMI_RX_STATUS_ERR_KEY_CACHE_MISS |
			     WMI_RX_STATUS_ERR_CRC))) {
		dev_kfree_skb(skb);
		goto exit;
	}

	if (rx_ev.status & WMI_RX_STATUS_ERR_MIC)
		status->flag |= RX_FLAG_MMIC_ERROR;

	if (rx_ev.chan_freq >= ATH12K_MIN_6GHZ_FREQ &&
	    rx_ev.chan_freq <= ATH12K_MAX_6GHZ_FREQ) {
		status->band = NL80211_BAND_6GHZ;
		status->freq = rx_ev.chan_freq;
	} else if (rx_ev.channel >= 1 && rx_ev.channel <= 14) {
		status->band = NL80211_BAND_2GHZ;
	} else if (rx_ev.channel >= 36 && rx_ev.channel <= ATH12K_MAX_5GHZ_CHAN) {
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
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "wmi mgmt rx 11b (CCK) on 5/6GHz, band = %d\n", status->band);

	sband = &ar->mac.sbands[status->band];

	if (status->band != NL80211_BAND_6GHZ)
		status->freq = ieee80211_channel_to_frequency(rx_ev.channel,
							      status->band);

	status->signal = rx_ev.snr + ar->rssi_offsets.rssi_offset;
	status->rate_idx = ath12k_mac_bitrate_to_idx(sband, rx_ev.rate / 100);

	hdr = (struct ieee80211_hdr *)skb->data;
	fc = le16_to_cpu(hdr->frame_control);
	frm_stype = FIELD_GET(IEEE80211_FCTL_STYPE, fc);

	if (ATH12K_MGMT_MLME_FRAME(hdr->frame_control))
		ath12k_dbg(ar->ab, ATH12K_DBG_MLME, "Received %s frame from STA %pM\n",
			   mgmt_frame_name[frm_stype], hdr->addr2);

	dp = ath12k_ab_to_dp(ab);
	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_addr(dp, hdr->addr1);
	if(!peer)
		peer = ath12k_dp_link_peer_find_by_addr(dp, hdr->addr3);
	if (!peer) {
		spin_unlock_bh(&dp->dp_lock);
		goto skip_mgmt_stats;
	}

	vif = peer->vif;

	spin_unlock_bh(&dp->dp_lock);

	if (!vif)
		goto skip_mgmt_stats;

	spin_lock_bh(&ar->data_lock);

	ahvif = ath12k_vif_to_ahvif(vif);
	mgmt_stats = &ahvif->mgmt_stats;
	mgmt_stats->rx_cnt[frm_stype]++;
	mgmt_stats->aggr_rx_mgmt++;

	spin_unlock_bh(&ar->data_lock);

skip_mgmt_stats:

	is_4addr_null_pkt = ieee80211_is_nullfunc(hdr->frame_control) &&
			    ieee80211_has_a4(hdr->frame_control);

	if (ieee80211_is_data(hdr->frame_control) && !is_4addr_null_pkt) {
		ab->dp->device_stats.rx_pkt_null_frame_dropped++;
		dev_kfree_skb(skb);
		goto exit;
	}

	if (is_4addr_null_pkt) {
		spin_lock_bh(&ab->base_lock);
		arsta = ath12k_link_sta_find_by_addr(ab, hdr->addr2);
		if (!arsta || arsta->is_bridge_peer) {
			spin_unlock_bh(&ab->base_lock);
			ath12k_warn(ab, "arsta not found %pM\n",
				    hdr->addr2);
			dev_kfree_skb(skb);
			goto exit;
		}
		pubsta = ieee80211_find_sta_by_ifaddr(ath12k_ar_to_hw(ar),
						      hdr->addr2, NULL);
		if (pubsta && pubsta->valid_links) {
			status->link_valid = 1;
			status->link_id = arsta->link_id;
		}
		spin_unlock_bh(&ab->base_lock);
		ab->dp->device_stats.rx_pkt_null_frame_handled++;
		ieee80211_rx_napi(ar->ah->hw, pubsta, skb, NULL);
		goto exit;
	}

	/* Firmware is guaranteed to report all essential management frames via
	 * WMI while it can deliver some extra via HTT. Since there can be
	 * duplicates split the reporting wrt monitor/sniffing.
	 */
	status->flag |= RX_FLAG_SKIP_MONITOR;

	/* In case of PMF, FW delivers decrypted frames with Protected Bit set
	 * including group privacy action frames.
	 */
	if (ieee80211_has_protected(hdr->frame_control)) {
		status->flag |= RX_FLAG_DECRYPTED;

		if (!ieee80211_is_robust_mgmt_frame(skb)) {
			status->flag |= RX_FLAG_IV_STRIPPED |
					RX_FLAG_MMIC_STRIPPED;
			hdr->frame_control = __cpu_to_le16(fc &
					     ~IEEE80211_FCTL_PROTECTED);
		}
	}

	if (ieee80211_is_beacon(hdr->frame_control))
		ath12k_mac_handle_beacon(ar, skb);

	/**
	 * RX MLO Link removal info TLV. Parse this TLV only when
	 * num_link_removal_info is present, otherwise ignore it.
	 */
	if (rx_ev.num_link_removal_info)
		ath12k_update_link_removal_params(ab, rx_ev.link_removal_info,
						  rx_ev.num_link_removal_info);

	if (ieee80211_is_probe_req(hdr->frame_control) &&
	    rx_ev.num_bcast_ttlm_info)
		ath12k_update_bcast_ttlm_params(ab, rx_ev.bcast_ttlm_info,
						rx_ev.num_bcast_ttlm_info);

	ath12k_dbg(ab, ATH12K_DBG_MGMT,
		   "event mgmt rx skb %p len %d ftype %02x stype %02x\n",
		   skb, skb->len,
		   fc & IEEE80211_FCTL_FTYPE, fc & IEEE80211_FCTL_STYPE);

	ath12k_dbg(ab, ATH12K_DBG_MGMT,
		   "event mgmt rx freq %d band %d snr %d, rate_idx %d\n",
		   status->freq, status->band, status->signal,
		   status->rate_idx);

	ieee80211_rx_ni(ath12k_ar_to_hw(ar), skb);

exit:
	rcu_read_unlock();
}

static void ath12k_mgmt_tx_compl_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_mgmt_tx_compl_event tx_compl_param = {0};
	struct ath12k *ar;

	if (ath12k_pull_mgmt_tx_compl_param_tlv(ab, skb, &tx_compl_param) != 0) {
		ath12k_warn(ab, "failed to extract mgmt tx compl event");
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, le32_to_cpu(tx_compl_param.pdev_id));
	if (!ar) {
		ath12k_warn(ab, "invalid pdev id %d in mgmt_tx_compl_event\n",
			    tx_compl_param.pdev_id);
		goto exit;
	}

	wmi_process_mgmt_tx_comp(ar, le32_to_cpu(tx_compl_param.desc_id),
				 le32_to_cpu(tx_compl_param.status),
				 le32_to_cpu(tx_compl_param.ack_rssi));

	ath12k_dbg(ab, ATH12K_DBG_MGMT,
		   "mgmt tx compl ev pdev_id %d, desc_id %d, status %d",
		   tx_compl_param.pdev_id, tx_compl_param.desc_id,
		   tx_compl_param.status);

exit:
	rcu_read_unlock();
}

static void ath12k_offchan_tx_completion_event(struct ath12k_base *ab,
					       struct sk_buff *skb)
{
	struct wmi_offchan_data_tx_compl_event offchan_tx_cmpl_params = {0};
	u32 desc_id;
	u32 pdev_id;
	u32 status;
	struct ath12k *ar;

	if (ath12k_pull_offchan_tx_compl_param_tlv(ab, skb, &offchan_tx_cmpl_params)
	    != 0) {
		ath12k_warn(ab, "failed to extract mgmt tx compl event");
		return;
	}
	status  = __le32_to_cpu(offchan_tx_cmpl_params.status);
	pdev_id = __le32_to_cpu(offchan_tx_cmpl_params.pdev_id);
	desc_id = __le32_to_cpu(offchan_tx_cmpl_params.desc_id);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid pdev id %d in offchan_tx_compl_event\n",
			    pdev_id);
		goto exit;
	}

	wmi_process_offchan_tx_comp(ar, desc_id, status);

	ath12k_dbg(ab, ATH12K_DBG_MGMT,
		   "off chan tx compl ev pdev_id %d, desc_id %d, status %d",
		   pdev_id, desc_id, status);
exit:
	rcu_read_unlock();
}

static struct ath12k *ath12k_get_ar_on_scan_state(struct ath12k_base *ab,
						  u32 vdev_id,
						  enum ath12k_scan_state state)
{
	int i;
	struct ath12k_pdev *pdev;
	struct ath12k *ar;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = rcu_dereference(ab->pdevs_active[i]);
		if (pdev && pdev->ar) {
			ar = pdev->ar;

			spin_lock_bh(&ar->data_lock);
			if (ar->scan.state == state &&
			    ar->scan.arvif &&
			    ar->scan.arvif->vdev_id == vdev_id) {
				spin_unlock_bh(&ar->data_lock);
				return ar;
			}
			spin_unlock_bh(&ar->data_lock);
		}
	}
	return NULL;
}

static void ath12k_scan_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k *ar;
	struct wmi_scan_event scan_ev = {0};

	if (ath12k_pull_scan_ev(ab, skb, &scan_ev) != 0) {
		ath12k_warn(ab, "failed to extract scan event");
		return;
	}

	rcu_read_lock();

	/* In case the scan was cancelled, ex. during interface teardown,
	 * the interface will not be found in active interfaces.
	 * Rather, in such scenarios, iterate over the active pdev's to
	 * search 'ar' if the corresponding 'ar' scan is ABORTING and the
	 * aborting scan's vdev id matches this event info.
	 */
	if (le32_to_cpu(scan_ev.event_type) == WMI_SCAN_EVENT_COMPLETED &&
	    le32_to_cpu(scan_ev.reason) == WMI_SCAN_REASON_CANCELLED) {
		ar = ath12k_get_ar_on_scan_state(ab, le32_to_cpu(scan_ev.vdev_id),
						 ATH12K_SCAN_ABORTING);
		if (!ar)
			ar = ath12k_get_ar_on_scan_state(ab, le32_to_cpu(scan_ev.vdev_id),
							 ATH12K_SCAN_RUNNING);
	} else {
		ar = ath12k_mac_get_ar_by_vdev_id(ab, le32_to_cpu(scan_ev.vdev_id));
	}

	if (!ar) {
		ath12k_warn(ab, "Received scan event for unknown vdev");
		rcu_read_unlock();
		return;
	}

	spin_lock_bh(&ar->data_lock);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "scan event %s type %d reason %d freq %d req_id %d scan_id %d vdev_id %d state %s (%d)\n",
		   ath12k_wmi_event_scan_type_str(le32_to_cpu(scan_ev.event_type),
						  le32_to_cpu(scan_ev.reason)),
		   le32_to_cpu(scan_ev.event_type),
		   le32_to_cpu(scan_ev.reason),
		   le32_to_cpu(scan_ev.channel_freq),
		   le32_to_cpu(scan_ev.scan_req_id),
		   le32_to_cpu(scan_ev.scan_id),
		   le32_to_cpu(scan_ev.vdev_id),
		   ath12k_scan_state_str(ar->scan.state), ar->scan.state);

	switch (le32_to_cpu(scan_ev.event_type)) {
	case WMI_SCAN_EVENT_STARTED:
		ath12k_wmi_event_scan_started(ar);
		break;
	case WMI_SCAN_EVENT_COMPLETED:
		ath12k_wmi_event_scan_completed(ar);
		break;
	case WMI_SCAN_EVENT_BSS_CHANNEL:
		ath12k_wmi_event_scan_bss_chan(ar, le32_to_cpu(scan_ev.scan_id),
					       le32_to_cpu(scan_ev.channel_freq));
		break;
	case WMI_SCAN_EVENT_FOREIGN_CHAN:
		ath12k_wmi_event_scan_foreign_chan(ar, le32_to_cpu(scan_ev.channel_freq));
		break;
	case WMI_SCAN_EVENT_START_FAILED:
		ath12k_warn(ab, "received scan start failure event\n");
		ath12k_wmi_event_scan_start_failed(ar);
		break;
	case WMI_SCAN_EVENT_DEQUEUED:
		__ath12k_mac_scan_finish(ar);
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

static void ath12k_peer_sta_kickout_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_peer_sta_kickout_arg arg = {};
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ieee80211_sta *sta;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_vif *ahvif;
	unsigned int link_id;
	struct ath12k *ar;

	if (ath12k_pull_peer_sta_kickout_ev(ab, skb, &arg) != 0) {
		ath12k_warn(ab, "failed to extract peer sta kickout event");
		return;
	}

	rcu_read_lock();

	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_addr(dp, arg.mac_addr);

	if (!peer) {
		ath12k_warn(ab, "peer not found %pM\n",
			    arg.mac_addr);
		goto exit;
	}

	ar = ath12k_mac_get_ar_by_vdev_id(ab, peer->vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid ar in peer sta kickout ev");
		goto exit;
	}

	ahvif = ath12k_vif_to_ahvif(peer->vif);

	if (peer->mlo)
		sta = ieee80211_find_sta_by_link_addrs(ar->ah->hw, arg.mac_addr,
						       NULL, &link_id);
	else
		sta = ieee80211_find_sta_by_ifaddr(ath12k_ar_to_hw(ar),
					   arg.mac_addr, NULL);
	if (!sta) {
		ath12k_warn(ab, "Spurious quick kickout for %sSTA %pM\n",
			    peer->mlo ? "MLO " : "", arg.mac_addr);
		goto exit;
	}

	if (peer->mlo && peer->link_id != link_id) {
		ath12k_warn(ab,
			    "Spurious quick kickout for MLO STA %pM with invalid link_id, peer: %d, sta: %d\n",
			    arg.mac_addr, peer->link_id, link_id);
		goto exit;
	}

	if (ar->ab->hw_params->handle_beacon_miss &&
	    ahvif->vif->type == NL80211_IFTYPE_STATION &&
	    arg.reason == __cpu_to_le32(WMI_PEER_STA_KICKOUT_REASON_INACTIVITY))
		ath12k_mac_handle_beacon_miss(ar, peer->vdev_id);
	else
		ieee80211_report_low_ack(sta, 10);

	ath12k_dbg(ab, ATH12K_DBG_PEER, "peer sta kickout event %pM",
		   arg.mac_addr);

exit:
	spin_unlock_bh(&dp->dp_lock);
	rcu_read_unlock();
}

static void ath12k_roam_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_roam_event roam_ev = {};
	struct ath12k *ar;
	u32 vdev_id;
	u8 roam_reason;

	if (ath12k_pull_roam_ev(ab, skb, &roam_ev) != 0) {
		ath12k_warn(ab, "failed to extract roam event");
		return;
	}

	vdev_id = le32_to_cpu(roam_ev.vdev_id);
	roam_reason = u32_get_bits(le32_to_cpu(roam_ev.reason),
				   WMI_ROAM_REASON_MASK);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "wmi roam event vdev %u reason %d rssi %d\n",
		   vdev_id, roam_reason, roam_ev.rssi);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in roam ev %d", vdev_id);
		rcu_read_unlock();
		return;
	}

	if (roam_reason >= WMI_ROAM_REASON_MAX)
		ath12k_warn(ab, "ignoring unknown roam event reason %d on vdev %i\n",
			    roam_reason, vdev_id);

	switch (roam_reason) {
	case WMI_ROAM_REASON_BEACON_MISS:
		ath12k_mac_handle_beacon_miss(ar, vdev_id);
		break;
	case WMI_ROAM_REASON_BETTER_AP:
	case WMI_ROAM_REASON_LOW_RSSI:
	case WMI_ROAM_REASON_SUITABLE_AP_FOUND:
	case WMI_ROAM_REASON_HO_FAILED:
		ath12k_warn(ab, "ignoring not implemented roam event reason %d on vdev %i\n",
			    roam_reason, vdev_id);
		break;
	}

	rcu_read_unlock();
}

static void ath12k_chan_info_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_chan_info_event ch_info_ev = {0};
	struct ath12k *ar;
	struct survey_info *survey;
	int idx;
	/* HW channel counters frequency value in hertz */
	u32 cc_freq_hz = ab->cc_freq_hz;

	if (ath12k_pull_chan_info_ev(ab, skb, &ch_info_ev) != 0) {
		ath12k_warn(ab, "failed to extract chan info event");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "chan info vdev_id %d err_code %d freq %d cmd_flags %d noise_floor %d rx_clear_count %d cycle_count %d mac_clk_mhz %d\n",
		   ch_info_ev.vdev_id, ch_info_ev.err_code, ch_info_ev.freq,
		   ch_info_ev.cmd_flags, ch_info_ev.noise_floor,
		   ch_info_ev.rx_clear_count, ch_info_ev.cycle_count,
		   ch_info_ev.mac_clk_mhz);

	if (le32_to_cpu(ch_info_ev.cmd_flags) == WMI_CHAN_INFO_END_RESP) {
		ath12k_dbg(ab, ATH12K_DBG_WMI, "chan info report completed\n");
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, le32_to_cpu(ch_info_ev.vdev_id));
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in chan info ev %d",
			    ch_info_ev.vdev_id);
		rcu_read_unlock();
		return;
	}
	spin_lock_bh(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
	case ATH12K_SCAN_STARTING:
		ath12k_warn(ab, "received chan info event without a scan request, ignoring\n");
		goto exit;
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		break;
	}

	idx = freq_to_idx(ar, le32_to_cpu(ch_info_ev.freq));
	if (idx >= ARRAY_SIZE(ar->survey)) {
		ath12k_warn(ab, "chan info: invalid frequency %d (idx %d out of bounds)\n",
			    ch_info_ev.freq, idx);
		goto exit;
	}

	/* If FW provides MAC clock frequency in Mhz, overriding the initialized
	 * HW channel counters frequency value
	 */
	if (ch_info_ev.mac_clk_mhz)
		cc_freq_hz = (le32_to_cpu(ch_info_ev.mac_clk_mhz) * 1000);

	if (ch_info_ev.cmd_flags == WMI_CHAN_INFO_START_RESP) {
		survey = &ar->survey[idx];
		memset(survey, 0, sizeof(*survey));
		survey->noise = le32_to_cpu(ch_info_ev.noise_floor);
		survey->filled = SURVEY_INFO_NOISE_DBM | SURVEY_INFO_TIME |
				 SURVEY_INFO_TIME_BUSY;
		survey->time = div_u64(le32_to_cpu(ch_info_ev.cycle_count), cc_freq_hz);
		survey->time_busy = div_u64(le32_to_cpu(ch_info_ev.rx_clear_count),
					    cc_freq_hz);
	}
exit:
	spin_unlock_bh(&ar->data_lock);
	rcu_read_unlock();
}

static void
ath12k_pdev_bss_chan_info_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_pdev_bss_chan_info_event bss_ch_info_ev = {};
	struct survey_info *survey;
	struct ath12k *ar;
	u32 cc_freq_hz = ab->cc_freq_hz;
	u64 busy, total, tx, rx, rx_bss;
	int idx;

	if (ath12k_pull_pdev_bss_chan_info_ev(ab, skb, &bss_ch_info_ev) != 0) {
		ath12k_warn(ab, "failed to extract pdev bss chan info event");
		return;
	}

	busy = (u64)(le32_to_cpu(bss_ch_info_ev.rx_clear_count_high)) << 32 |
		le32_to_cpu(bss_ch_info_ev.rx_clear_count_low);

	total = (u64)(le32_to_cpu(bss_ch_info_ev.cycle_count_high)) << 32 |
		le32_to_cpu(bss_ch_info_ev.cycle_count_low);

	tx = (u64)(le32_to_cpu(bss_ch_info_ev.tx_cycle_count_high)) << 32 |
		le32_to_cpu(bss_ch_info_ev.tx_cycle_count_low);

	rx = (u64)(le32_to_cpu(bss_ch_info_ev.rx_cycle_count_high)) << 32 |
		le32_to_cpu(bss_ch_info_ev.rx_cycle_count_low);

	rx_bss = (u64)(le32_to_cpu(bss_ch_info_ev.rx_bss_cycle_count_high)) << 32 |
		le32_to_cpu(bss_ch_info_ev.rx_bss_cycle_count_low);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pdev bss chan info:\n pdev_id: %d freq: %d noise: %d cycle: busy %llu total %llu tx %llu rx %llu rx_bss %llu\n",
		   bss_ch_info_ev.pdev_id, bss_ch_info_ev.freq,
		   bss_ch_info_ev.noise_floor, busy, total,
		   tx, rx, rx_bss);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, le32_to_cpu(bss_ch_info_ev.pdev_id));

	if (!ar) {
		ath12k_warn(ab, "invalid pdev id %d in bss_chan_info event\n",
			    bss_ch_info_ev.pdev_id);
		rcu_read_unlock();
		return;
	}

	spin_lock_bh(&ar->data_lock);
	idx = freq_to_idx(ar, le32_to_cpu(bss_ch_info_ev.freq));
	if (idx >= ARRAY_SIZE(ar->survey)) {
		ath12k_warn(ab, "bss chan info: invalid frequency %d (idx %d out of bounds)\n",
			    bss_ch_info_ev.freq, idx);
		goto exit;
	}

	survey = &ar->survey[idx];

	survey->noise     = le32_to_cpu(bss_ch_info_ev.noise_floor);
	survey->time      = div_u64(total, cc_freq_hz);
	survey->time_busy = div_u64(busy, cc_freq_hz);
	survey->time_rx   = div_u64(rx, cc_freq_hz);
	survey->time_bss_rx = div_u64(rx_bss, cc_freq_hz);
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

static void ath12k_vdev_install_key_compl_event(struct ath12k_base *ab,
						struct sk_buff *skb)
{
	struct wmi_vdev_install_key_complete_arg install_key_compl = {0};
	struct ath12k *ar;

	if (ath12k_pull_vdev_install_key_compl_ev(ab, skb, &install_key_compl) != 0) {
		ath12k_warn(ab, "failed to extract install key compl event");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI | ATH12K_DBG_EAPOL,
		   "vdev install key ev idx %d flags %08x macaddr %pM status %d\n",
		   install_key_compl.key_idx, install_key_compl.key_flags,
		   install_key_compl.macaddr, install_key_compl.status);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, install_key_compl.vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in install key compl ev %d",
			    install_key_compl.vdev_id);
		rcu_read_unlock();
		return;
	}

	ar->install_key_status = 0;

	if (install_key_compl.status != WMI_VDEV_INSTALL_KEY_COMPL_STATUS_SUCCESS) {
		ath12k_warn(ab, "install key failed for %pM status %d\n",
			    install_key_compl.macaddr, install_key_compl.status);
		ar->install_key_status = install_key_compl.status;
	}

	complete(&ar->install_key_done);
	rcu_read_unlock();
}

static int ath12k_wmi_tlv_services_parser(struct ath12k_base *ab,
					  u16 tag, u16 tag_len,
					  const void *ptr,
					  void *data)
{
	const struct wmi_service_available_event *ev;
	u32 *wmi_ext2_service_bitmap;
	int i, j;
	u16 expected_len;
	u16 wmi_max_ext2_service_words;

	expected_len = WMI_SERVICE_SEGMENT_BM_SIZE32 * sizeof(u32);
	if (tag_len < expected_len) {
		ath12k_warn(ab, "invalid length %d for the WMI services available tag 0x%x\n",
			    tag_len, tag);
		return -EINVAL;
	}

	switch (tag) {
	case WMI_TAG_SERVICE_AVAILABLE_EVENT:
		ev = (struct wmi_service_available_event *)ptr;
		for (i = 0, j = WMI_MAX_SERVICE;
		     i < WMI_SERVICE_SEGMENT_BM_SIZE32 && j < WMI_MAX_EXT_SERVICE;
		     i++) {
			do {
				if (le32_to_cpu(ev->wmi_service_segment_bitmap[i]) &
				    BIT(j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32))
					set_bit(j, ab->wmi_ab.svc_map);
			} while (++j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32);
		}

		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "wmi_ext_service_bitmap 0x%x 0x%x 0x%x 0x%x",
			   ev->wmi_service_segment_bitmap[0],
			   ev->wmi_service_segment_bitmap[1],
			   ev->wmi_service_segment_bitmap[2],
			   ev->wmi_service_segment_bitmap[3]);
		break;
	case WMI_TAG_ARRAY_UINT32:
		wmi_ext2_service_bitmap = (u32 *)ptr;
		wmi_max_ext2_service_words = tag_len / sizeof(u32);
		for (i = 0, j = WMI_MAX_EXT_SERVICE;
		     i < wmi_max_ext2_service_words && j < WMI_MAX_EXT2_SERVICE;
		     i++) {
			do {
				if (wmi_ext2_service_bitmap[i] &
				    BIT(j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32))
					set_bit(j, ab->wmi_ab.svc_map);
			} while (++j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32);
		}

		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "wmi_ext2_service_bitmap 0x%04x 0x%04x 0x%04x 0x%04x",
			   wmi_ext2_service_bitmap[0], wmi_ext2_service_bitmap[1],
			   wmi_ext2_service_bitmap[2], wmi_ext2_service_bitmap[3]);
		break;
	}
	return 0;
}

static int ath12k_service_available_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_services_parser,
				  NULL);
	return ret;
}

static void ath12k_peer_assoc_conf_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_peer_assoc_conf_arg peer_assoc_conf = {0};
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_link_peer *peer;
	struct ath12k *ar;

	if (ath12k_pull_peer_assoc_conf_ev(ab, skb, &peer_assoc_conf) != 0) {
		ath12k_warn(ab, "failed to extract peer assoc conf event");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI | ATH12K_DBG_MLME,
		   "peer assoc conf ev vdev id %d macaddr %pM status:%d\n",
		   peer_assoc_conf.vdev_id, peer_assoc_conf.macaddr,
		   peer_assoc_conf.status);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, peer_assoc_conf.vdev_id);

	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in peer assoc conf ev %d",
			    peer_assoc_conf.vdev_id);
		rcu_read_unlock();
		return;
	}

	spin_lock_bh(&dp->dp_lock);
	peer =  ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, peer_assoc_conf.vdev_id,
							     peer_assoc_conf.macaddr);
	if (peer && !peer_assoc_conf.status)
		peer->assoc_success = true;
	spin_unlock_bh(&dp->dp_lock);

	complete(&ar->peer_assoc_done);
	rcu_read_unlock();
}

static void
ath12k_wmi_fw_vdev_stats_dump(struct ath12k *ar,
			      struct ath12k_fw_stats *fw_stats,
			      char *buf, u32 *length)
{
	const struct ath12k_fw_stats_vdev *vdev;
	u32 buf_len = ATH12K_FW_STATS_BUF_SIZE;
	struct ath12k_link_vif *arvif;
	u32 len = *length;
	u8 *vif_macaddr;
	int i;

	len += scnprintf(buf + len, buf_len - len, "\n");
	len += scnprintf(buf + len, buf_len - len, "%30s\n",
			 "ath12k VDEV stats");
	len += scnprintf(buf + len, buf_len - len, "%30s\n\n",
			 "=================");

	list_for_each_entry(vdev, &fw_stats->vdevs, list) {
		arvif = ath12k_mac_get_arvif(ar, vdev->vdev_id);
		if (!arvif)
			continue;
		vif_macaddr = arvif->ahvif->vif->addr;

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

		for (i = 0 ; i < WLAN_MAX_AC; i++)
			len += scnprintf(buf + len, buf_len - len,
					"%25s [%02d] %u\n",
					"num tx frames", i,
					vdev->num_tx_frames[i]);

		for (i = 0 ; i < WLAN_MAX_AC; i++)
			len += scnprintf(buf + len, buf_len - len,
					"%25s [%02d] %u\n",
					"num tx frames retries", i,
					vdev->num_tx_frames_retries[i]);

		for (i = 0 ; i < WLAN_MAX_AC; i++)
			len += scnprintf(buf + len, buf_len - len,
					"%25s [%02d] %u\n",
					"num tx frames failures", i,
					vdev->num_tx_frames_failures[i]);

		for (i = 0 ; i < MAX_TX_RATE_VALUES; i++)
			len += scnprintf(buf + len, buf_len - len,
					"%25s [%02d] 0x%08x\n",
					"tx rate history", i,
					vdev->tx_rate_history[i]);
		for (i = 0 ; i < MAX_TX_RATE_VALUES; i++)
			len += scnprintf(buf + len, buf_len - len,
					"%25s [%02d] %u\n",
					"beacon rssi history", i,
					vdev->beacon_rssi_history[i]);

		len += scnprintf(buf + len, buf_len - len, "\n");
		*length = len;
	}
}

static void
ath12k_wmi_fw_bcn_stats_dump(struct ath12k *ar,
			     struct ath12k_fw_stats *fw_stats,
			     char *buf, u32 *length)
{
	const struct ath12k_fw_stats_bcn *bcn;
	u32 buf_len = ATH12K_FW_STATS_BUF_SIZE;
	struct ath12k_link_vif *arvif;
	u32 len = *length;
	size_t num_bcn;

	num_bcn = list_count_nodes(&fw_stats->bcn);

	len += scnprintf(buf + len, buf_len - len, "\n");
	len += scnprintf(buf + len, buf_len - len, "%30s (%zu)\n",
			 "ath12k Beacon stats", num_bcn);
	len += scnprintf(buf + len, buf_len - len, "%30s\n\n",
			 "===================");

	list_for_each_entry(bcn, &fw_stats->bcn, list) {
		arvif = ath12k_mac_get_arvif(ar, bcn->vdev_id);
		if (!arvif)
			continue;
		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "VDEV ID", bcn->vdev_id);
		len += scnprintf(buf + len, buf_len - len, "%30s %pM\n",
				 "VDEV MAC address", arvif->ahvif->vif->addr);
		len += scnprintf(buf + len, buf_len - len, "%30s\n\n",
				 "================");
		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "Num of beacon tx success", bcn->tx_bcn_succ_cnt);
		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "Num of beacon tx failures", bcn->tx_bcn_outage_cnt);

		len += scnprintf(buf + len, buf_len - len, "\n");
		*length = len;
	}
}

static void
ath12k_wmi_fw_pdev_base_stats_dump(const struct ath12k_fw_stats_pdev *pdev,
				   char *buf, u32 *length, u64 fw_soc_drop_cnt)
{
	u32 len = *length;
	u32 buf_len = ATH12K_FW_STATS_BUF_SIZE;

	len = scnprintf(buf + len, buf_len - len, "\n");
	len += scnprintf(buf + len, buf_len - len, "%30s\n",
			"ath12k PDEV stats");
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
	len += scnprintf(buf + len, buf_len - len, "%30s %10llu\n",
			"soc drop count", fw_soc_drop_cnt);

	*length = len;
}

static void
ath12k_wmi_fw_pdev_tx_stats_dump(const struct ath12k_fw_stats_pdev *pdev,
				 char *buf, u32 *length)
{
	u32 len = *length;
	u32 buf_len = ATH12K_FW_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "\n%30s\n",
			 "ath12k PDEV TX stats");
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
			 "PPDUs cleaned", pdev->tx_abort);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "MPDUs requeued", pdev->mpdus_requed);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Excessive retries", pdev->tx_ko);
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
	*length = len;
}

static void
ath12k_wmi_fw_pdev_rx_stats_dump(const struct ath12k_fw_stats_pdev *pdev,
				 char *buf, u32 *length)
{
	u32 len = *length;
	u32 buf_len = ATH12K_FW_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "\n%30s\n",
			 "ath12k PDEV RX stats");
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
	*length = len;
}

static void
ath12k_wmi_fw_pdev_stats_dump(struct ath12k *ar,
			      struct ath12k_fw_stats *fw_stats,
			      char *buf, u32 *length)
{
	const struct ath12k_fw_stats_pdev *pdev;
	u32 len = *length;

	pdev = list_first_entry_or_null(&fw_stats->pdevs,
					struct ath12k_fw_stats_pdev, list);
	if (!pdev) {
		ath12k_warn(ar->ab, "failed to get pdev stats\n");
		return;
	}

	ath12k_wmi_fw_pdev_base_stats_dump(pdev, buf, &len,
					   ar->ab->fw_soc_drop_count);
	ath12k_wmi_fw_pdev_tx_stats_dump(pdev, buf, &len);
	ath12k_wmi_fw_pdev_rx_stats_dump(pdev, buf, &len);

	*length = len;
}

void ath12k_wmi_fw_stats_dump(struct ath12k *ar,
			      struct ath12k_fw_stats *fw_stats,
			      u32 stats_id, char *buf)
{
	u32 len = 0;
	u32 buf_len = ATH12K_FW_STATS_BUF_SIZE;

	spin_lock_bh(&ar->data_lock);

	switch (stats_id) {
	case WMI_REQUEST_VDEV_STAT:
		ath12k_wmi_fw_vdev_stats_dump(ar, fw_stats, buf, &len);
		break;
	case WMI_REQUEST_BCN_STAT:
		ath12k_wmi_fw_bcn_stats_dump(ar, fw_stats, buf, &len);
		break;
	case WMI_REQUEST_PDEV_STAT:
		ath12k_wmi_fw_pdev_stats_dump(ar, fw_stats, buf, &len);
		break;
	default:
		break;
	}

	spin_unlock_bh(&ar->data_lock);

	if (len >= buf_len)
		buf[len - 1] = 0;
	else
		buf[len] = 0;

	ath12k_fw_stats_reset(ar);
}

static void
ath12k_wmi_pull_vdev_stats(const struct wmi_vdev_stats_params *src,
			   struct ath12k_fw_stats_vdev *dst)
{
	int i;

	dst->vdev_id = le32_to_cpu(src->vdev_id);
	dst->beacon_snr = le32_to_cpu(src->beacon_snr);
	dst->data_snr = le32_to_cpu(src->data_snr);
	dst->num_rx_frames = le32_to_cpu(src->num_rx_frames);
	dst->num_rts_fail = le32_to_cpu(src->num_rts_fail);
	dst->num_rts_success = le32_to_cpu(src->num_rts_success);
	dst->num_rx_err = le32_to_cpu(src->num_rx_err);
	dst->num_rx_discard = le32_to_cpu(src->num_rx_discard);
	dst->num_tx_not_acked = le32_to_cpu(src->num_tx_not_acked);

	for (i = 0; i < WLAN_MAX_AC; i++)
		dst->num_tx_frames[i] =
			le32_to_cpu(src->num_tx_frames[i]);

	for (i = 0; i < WLAN_MAX_AC; i++)
		dst->num_tx_frames_retries[i] =
			le32_to_cpu(src->num_tx_frames_retries[i]);

	for (i = 0; i < WLAN_MAX_AC; i++)
		dst->num_tx_frames_failures[i] =
			le32_to_cpu(src->num_tx_frames_failures[i]);

	for (i = 0; i < MAX_TX_RATE_VALUES; i++)
		dst->tx_rate_history[i] =
			le32_to_cpu(src->tx_rate_history[i]);

	for (i = 0; i < MAX_TX_RATE_VALUES; i++)
		dst->beacon_rssi_history[i] =
			le32_to_cpu(src->beacon_rssi_history[i]);
}

static void
ath12k_wmi_pull_bcn_stats(const struct ath12k_wmi_bcn_stats_params *src,
			  struct ath12k_fw_stats_bcn *dst)
{
	dst->vdev_id = le32_to_cpu(src->vdev_id);
	dst->tx_bcn_succ_cnt = le32_to_cpu(src->tx_bcn_succ_cnt);
	dst->tx_bcn_outage_cnt = le32_to_cpu(src->tx_bcn_outage_cnt);
}

static void
ath12k_wmi_pull_pdev_stats_base(const struct ath12k_wmi_pdev_base_stats_params *src,
				struct ath12k_fw_stats_pdev *dst)
{
	dst->ch_noise_floor = a_sle32_to_cpu(src->chan_nf);
	dst->tx_frame_count = __le32_to_cpu(src->tx_frame_count);
	dst->rx_frame_count = __le32_to_cpu(src->rx_frame_count);
	dst->rx_clear_count = __le32_to_cpu(src->rx_clear_count);
	dst->cycle_count = __le32_to_cpu(src->cycle_count);
	dst->phy_err_count = __le32_to_cpu(src->phy_err_count);
	dst->chan_tx_power = __le32_to_cpu(src->chan_tx_pwr);
}

static void
ath12k_wmi_pull_pdev_stats_tx(const struct ath12k_wmi_pdev_tx_stats_params *src,
			      struct ath12k_fw_stats_pdev *dst)
{
	dst->comp_queued = a_sle32_to_cpu(src->comp_queued);
	dst->comp_delivered = a_sle32_to_cpu(src->comp_delivered);
	dst->msdu_enqued = a_sle32_to_cpu(src->msdu_enqued);
	dst->mpdu_enqued = a_sle32_to_cpu(src->mpdu_enqued);
	dst->wmm_drop = a_sle32_to_cpu(src->wmm_drop);
	dst->local_enqued = a_sle32_to_cpu(src->local_enqued);
	dst->local_freed = a_sle32_to_cpu(src->local_freed);
	dst->hw_queued = a_sle32_to_cpu(src->hw_queued);
	dst->hw_reaped = a_sle32_to_cpu(src->hw_reaped);
	dst->underrun = a_sle32_to_cpu(src->underrun);
	dst->tx_abort = a_sle32_to_cpu(src->tx_abort);
	dst->mpdus_requed = a_sle32_to_cpu(src->mpdus_requed);
	dst->tx_ko = __le32_to_cpu(src->tx_ko);
	dst->data_rc = __le32_to_cpu(src->data_rc);
	dst->self_triggers = __le32_to_cpu(src->self_triggers);
	dst->sw_retry_failure = __le32_to_cpu(src->sw_retry_failure);
	dst->illgl_rate_phy_err = __le32_to_cpu(src->illgl_rate_phy_err);
	dst->pdev_cont_xretry = __le32_to_cpu(src->pdev_cont_xretry);
	dst->pdev_tx_timeout = __le32_to_cpu(src->pdev_tx_timeout);
	dst->pdev_resets = __le32_to_cpu(src->pdev_resets);
	dst->stateless_tid_alloc_failure =
		__le32_to_cpu(src->stateless_tid_alloc_failure);
	dst->phy_underrun = __le32_to_cpu(src->phy_underrun);
	dst->txop_ovf = __le32_to_cpu(src->txop_ovf);
}

static void
ath12k_wmi_pull_pdev_stats_rx(const struct ath12k_wmi_pdev_rx_stats_params *src,
			      struct ath12k_fw_stats_pdev *dst)
{
	dst->mid_ppdu_route_change =
		a_sle32_to_cpu(src->mid_ppdu_route_change);
	dst->status_rcvd = a_sle32_to_cpu(src->status_rcvd);
	dst->r0_frags = a_sle32_to_cpu(src->r0_frags);
	dst->r1_frags = a_sle32_to_cpu(src->r1_frags);
	dst->r2_frags = a_sle32_to_cpu(src->r2_frags);
	dst->r3_frags = a_sle32_to_cpu(src->r3_frags);
	dst->htt_msdus = a_sle32_to_cpu(src->htt_msdus);
	dst->htt_mpdus = a_sle32_to_cpu(src->htt_mpdus);
	dst->loc_msdus = a_sle32_to_cpu(src->loc_msdus);
	dst->loc_mpdus = a_sle32_to_cpu(src->loc_mpdus);
	dst->oversize_amsdu = a_sle32_to_cpu(src->oversize_amsdu);
	dst->phy_errs = a_sle32_to_cpu(src->phy_errs);
	dst->phy_err_drop = a_sle32_to_cpu(src->phy_err_drop);
	dst->mpdu_errs = a_sle32_to_cpu(src->mpdu_errs);
}

static int ath12k_wmi_tlv_fw_stats_data_parse(struct ath12k_base *ab,
					      struct wmi_tlv_fw_stats_parse *parse,
					      const void *ptr,
					      u16 len)
{
	const struct wmi_stats_event *ev = parse->ev;
	struct ath12k_fw_stats *stats = parse->stats;
	struct ath12k *ar;
	struct ath12k_link_vif *arvif;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	struct ath12k_link_sta *arsta;
	int i, ret = 0;
	const void *data = ptr;

	if (!ev) {
		ath12k_warn(ab, "failed to fetch update stats ev");
		return -EPROTO;
	}

	if (!stats)
		return -EINVAL;

	rcu_read_lock();

	stats->pdev_id = le32_to_cpu(ev->pdev_id);
	ar = ath12k_mac_get_ar_by_pdev_id(ab, stats->pdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid pdev id %d in update stats event\n",
			    le32_to_cpu(ev->pdev_id));
		ret = -EPROTO;
		goto exit;
	}

	for (i = 0; i < le32_to_cpu(ev->num_vdev_stats); i++) {
		const struct wmi_vdev_stats_params *src;
		struct ath12k_fw_stats_vdev *dst;

		src = data;
		if (len < sizeof(*src)) {
			ret = -EPROTO;
			goto exit;
		}

		arvif = ath12k_mac_get_arvif(ar, le32_to_cpu(src->vdev_id));
		if (arvif) {
			sta = ieee80211_find_sta_by_ifaddr(ath12k_ar_to_hw(ar),
							   arvif->bssid,
							   NULL);
			if (sta) {
				ahsta = ath12k_sta_to_ahsta(sta);
				arsta = &ahsta->deflink;
				arsta->rssi_beacon = le32_to_cpu(src->beacon_snr);
				ath12k_dbg(ab, ATH12K_DBG_WMI,
					   "wmi stats vdev id %d snr %d\n",
					   src->vdev_id, src->beacon_snr);
			} else {
				ath12k_dbg(ab, ATH12K_DBG_WMI,
					   "not found station bssid %pM for vdev stat\n",
					   arvif->bssid);
			}
		}

		data += sizeof(*src);
		len -= sizeof(*src);
		dst = kzalloc(sizeof(*dst), GFP_ATOMIC);
		if (!dst)
			continue;
		ath12k_wmi_pull_vdev_stats(src, dst);
		stats->stats_id = WMI_REQUEST_VDEV_STAT;
		list_add_tail(&dst->list, &stats->vdevs);
	}
	for (i = 0; i < le32_to_cpu(ev->num_bcn_stats); i++) {
		const struct ath12k_wmi_bcn_stats_params *src;
		struct ath12k_fw_stats_bcn *dst;

		src = data;
		if (len < sizeof(*src)) {
			ret = -EPROTO;
			goto exit;
		}

		data += sizeof(*src);
		len -= sizeof(*src);
		dst = kzalloc(sizeof(*dst), GFP_ATOMIC);
		if (!dst)
			continue;
		ath12k_wmi_pull_bcn_stats(src, dst);
		stats->stats_id = WMI_REQUEST_BCN_STAT;
		list_add_tail(&dst->list, &stats->bcn);
	}
	for (i = 0; i < le32_to_cpu(ev->num_pdev_stats); i++) {
		const struct ath12k_wmi_pdev_stats_params *src;
		struct ath12k_fw_stats_pdev *dst;

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

		ath12k_wmi_pull_pdev_stats_base(&src->base, dst);
		ath12k_wmi_pull_pdev_stats_tx(&src->tx, dst);
		ath12k_wmi_pull_pdev_stats_rx(&src->rx, dst);
		list_add_tail(&dst->list, &stats->pdevs);
	}

exit:
	rcu_read_unlock();
	return ret;
}

static int ath12k_wmi_tlv_rssi_chain_parse(struct ath12k_base *ab,
					   u16 tag, u16 len,
					   const void *ptr, void *data)
{
	const struct wmi_rssi_stat_params *stats_rssi = ptr;
	struct wmi_tlv_fw_stats_parse *parse = data;
	const struct wmi_stats_event *ev = parse->ev;
	struct ath12k_link_vif *arvif;
	struct ath12k_link_sta *arsta;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	struct ath12k *ar;
	int pdev_id;
	int vdev_id;
	int j;

	if (tag != WMI_TAG_RSSI_STATS)
		return -EPROTO;

	pdev_id = le32_to_cpu(ev->pdev_id);
	vdev_id = le32_to_cpu(stats_rssi->vdev_id);
	guard(rcu)();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid pdev id %d in rssi chain parse\n",
			    pdev_id);
		return -EPROTO;
	}

	arvif = ath12k_mac_get_arvif(ar, vdev_id);
	if (!arvif) {
		ath12k_warn(ab, "not found vif for vdev id %d\n", vdev_id);
		return -EPROTO;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "stats bssid %pM vif %p\n",
		   arvif->bssid, arvif->ahvif->vif);

	sta = ieee80211_find_sta_by_ifaddr(ath12k_ar_to_hw(ar),
					   arvif->bssid,
					   NULL);
	if (!sta) {
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "not found station of bssid %pM for rssi chain\n",
			   arvif->bssid);
		return -EPROTO;
	}

	ahsta = ath12k_sta_to_ahsta(sta);
	arsta = &ahsta->deflink;

	BUILD_BUG_ON(ARRAY_SIZE(arsta->chain_signal) >
		     ARRAY_SIZE(stats_rssi->rssi_avg_beacon));

	for (j = 0; j < ARRAY_SIZE(arsta->chain_signal); j++)
		arsta->chain_signal[j] = le32_to_cpu(stats_rssi->rssi_avg_beacon[j]);

	return 0;
}

static int ath12k_wmi_tlv_fw_stats_parse(struct ath12k_base *ab,
					 u16 tag, u16 len,
					 const void *ptr, void *data)
{
	struct wmi_tlv_fw_stats_parse *parse = data;
	int ret = 0;

	switch (tag) {
	case WMI_TAG_STATS_EVENT:
		parse->ev = ptr;
		break;
	case WMI_TAG_ARRAY_BYTE:
		ret = ath12k_wmi_tlv_fw_stats_data_parse(ab, parse, ptr, len);
		break;
	case WMI_TAG_PER_CHAIN_RSSI_STATS:
		parse->rssi = ptr;
		if (le32_to_cpu(parse->ev->stats_id) & WMI_REQUEST_RSSI_PER_CHAIN_STAT)
			parse->rssi_num = le32_to_cpu(parse->rssi->num_per_chain_rssi);
		break;
	case WMI_TAG_ARRAY_STRUCT:
		if (parse->rssi_num && !parse->chain_rssi_done) {
			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_tlv_rssi_chain_parse,
						  parse);
			if (ret)
				return ret;

			parse->chain_rssi_done = true;
		}
		break;
	default:
		break;
	}
	return ret;
}

static int ath12k_wmi_pull_fw_stats(struct ath12k_base *ab, struct sk_buff *skb,
				    struct ath12k_fw_stats *stats)
{
	struct wmi_tlv_fw_stats_parse parse = {};

	stats->stats_id = 0;
	parse.stats = stats;

	return ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				   ath12k_wmi_tlv_fw_stats_parse,
				   &parse);
}

static void ath12k_wmi_fw_stats_process(struct ath12k *ar,
					struct ath12k_fw_stats *stats)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_pdev *pdev;
	bool is_end = true;
	size_t total_vdevs_started = 0;
	int i;

	if (stats->stats_id == WMI_REQUEST_VDEV_STAT) {
		if (list_empty(&stats->vdevs)) {
			ath12k_warn(ab, "empty vdev stats");
			return;
		}
		/* FW sends all the active VDEV stats irrespective of PDEV,
		 * hence limit until the count of all VDEVs started
		 */
		rcu_read_lock();
		for (i = 0; i < ab->num_radios; i++) {
			pdev = rcu_dereference(ab->pdevs_active[i]);
			if (pdev && pdev->ar)
				total_vdevs_started += pdev->ar->num_started_vdevs;
		}
		rcu_read_unlock();

		if (total_vdevs_started)
			is_end = ((++ar->fw_stats.num_vdev_recvd) ==
				  total_vdevs_started);

		list_splice_tail_init(&stats->vdevs,
				      &ar->fw_stats.vdevs);

		if (is_end)
			complete(&ar->fw_stats_done);

		return;
	}

	if (stats->stats_id == WMI_REQUEST_BCN_STAT) {
		if (list_empty(&stats->bcn)) {
			ath12k_warn(ab, "empty beacon stats");
			return;
		}
		/* Mark end until we reached the count of all started VDEVs
		 * within the PDEV
		 */
		if (ar->num_started_vdevs)
			is_end = ((++ar->fw_stats.num_bcn_recvd) ==
				  ar->num_started_vdevs);

		list_splice_tail_init(&stats->bcn,
				      &ar->fw_stats.bcn);

		if (is_end)
			complete(&ar->fw_stats_done);
	}
}

static void ath12k_update_stats_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k_fw_stats stats = {};
	struct ath12k *ar;
	int ret;

	INIT_LIST_HEAD(&stats.pdevs);
	INIT_LIST_HEAD(&stats.vdevs);
	INIT_LIST_HEAD(&stats.bcn);

	ret = ath12k_wmi_pull_fw_stats(ab, skb, &stats);
	if (ret) {
		ath12k_warn(ab, "failed to pull fw stats: %d\n", ret);
		goto free;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI, "event update stats");

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, stats.pdev_id);
	if (!ar) {
		rcu_read_unlock();
		ath12k_warn(ab, "failed to get ar for pdev_id %d: %d\n",
			    stats.pdev_id, ret);
		goto free;
	}

	spin_lock_bh(&ar->data_lock);

	/* Handle WMI_REQUEST_PDEV_STAT status update */
	if (stats.stats_id == WMI_REQUEST_PDEV_STAT) {
		list_splice_tail_init(&stats.pdevs, &ar->fw_stats.pdevs);
		complete(&ar->fw_stats_done);
		goto complete;
	}

	/* Handle WMI_REQUEST_VDEV_STAT and WMI_REQUEST_BCN_STAT updates. */
	ath12k_wmi_fw_stats_process(ar, &stats);

complete:
	complete(&ar->fw_stats_complete);
	spin_unlock_bh(&ar->data_lock);
	rcu_read_unlock();

	/* Since the stats's pdev, vdev and beacon list are spliced and reinitialised
	 * at this point, no need to free the individual list.
	 */
	return;

free:
	ath12k_fw_stats_free(&stats);
}

/* PDEV_CTL_FAILSAFE_CHECK_EVENT is received from FW when the frequency scanned
 * is not part of BDF CTL(Conformance test limits) table entries.
 */
static void ath12k_pdev_ctl_failsafe_check_event(struct ath12k_base *ab,
						 struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_pdev_ctl_failsafe_chk_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_PDEV_CTL_FAILSAFE_CHECK_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch pdev ctl failsafe check ev");
		kfree(tb);
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pdev ctl failsafe check ev status %d\n",
		   ev->ctl_failsafe_status);

	/* If ctl_failsafe_status is set to 1 FW will max out the Transmit power
	 * to 10 dBm else the CTL power entry in the BDF would be picked up.
	 */
	if (ev->ctl_failsafe_status != 0)
		ath12k_warn(ab, "pdev ctl failsafe failure status %d",
			    ev->ctl_failsafe_status);

	kfree(tb);
}

void ath12k_debug_print_dcs_wlan_intf_stats(struct ath12k_base *ab,
					    struct wmi_dcs_wlan_interference_stats *info)
{
	ath12k_dbg(ab, ATH12K_DBG_WMI, "wlan_intf: reg_tsf32=%u", info->reg_tsf32);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "wlan_intf: last_ack_rssi=%u",
		   info->last_ack_rssi);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "wlan_intf: tx_waste_time=%u",
		   info->tx_waste_time);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "wlan_intf: rx_time=%u", info->rx_time);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "wlan_intf: phyerr_cnt=%u", info->phyerr_cnt);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "wlan_intf: listen_time=%u", info->listen_time);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "wlan_intf: reg_tx_frame_cnt=%u",
		   info->reg_tx_frame_cnt);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "wlan_intf: reg_rx_frame_cnt=%u",
		   info->reg_rx_frame_cnt);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "wlan_intf: reg_rxclr_cnt=%u",
		   info->reg_rxclr_cnt);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "wlan_intf: reg_cycle_cnt=%u",
		   info->reg_cycle_cnt);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "wlan_intf: reg_rxclr_ext_cnt=%u",
		   info->reg_rxclr_ext_cnt);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "wlan_intf: reg_ofdm_phyerr_cnt=%u",
		   info->reg_ofdm_phyerr_cnt);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "wlan_intf: reg_cck_phyerr_cnt=%u",
		   info->reg_cck_phyerr_cnt);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "wlan_intf: chan_nf=%d", info->chan_nf);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "wlan_intf: my_bss_rx_cycle_count=%u",
		   info->my_bss_rx_cycle_count);
}

static int ath12k_wmi_dcs_intf_subtlv_parser(struct ath12k_base *ab,
					     u16 tag, u16 len,
					     const void *ptr, void *data)
{
	int ret = 0;
	struct wmi_dcs_awgn_info *awgn_info;
	struct wmi_dcs_cw_info *cw_info;
	struct wmi_dcs_wlan_interference_stats_ev *wlan_info;
	struct wmi_dcs_wlan_interference_stats *tmp;

	switch (tag) {
	case WMI_TAG_DCS_AWGN_INT_TYPE:
		awgn_info = (struct wmi_dcs_awgn_info *)ptr;

		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "AWGN Info: channel width: %d, chan freq: %d, center_freq0: %d, center_freq1: %d, bw_intf_bitmap: %d\n",
			   awgn_info->channel_width, awgn_info->chan_freq, awgn_info->center_freq0, awgn_info->center_freq1,
			   awgn_info->chan_bw_interference_bitmap);
		memcpy(data, awgn_info, sizeof(*awgn_info));
		break;
	case WMI_TAG_WLAN_DCS_CW_INT:
		cw_info = (struct wmi_dcs_cw_info *)ptr;
		ath12k_dbg(ab, ATH12K_DBG_WMI, "CW Info: channel=%d", cw_info->channel);
		memcpy(data, cw_info, sizeof(*cw_info));
		break;
	case WMI_TAG_ATH_DCS_WLAN_INT_STAT:
		wlan_info = (struct wmi_dcs_wlan_interference_stats_ev *)ptr;
		tmp = (struct wmi_dcs_wlan_interference_stats *)data;
		tmp->reg_tsf32 = le32_to_cpu(wlan_info->reg_tsf32);
		tmp->last_ack_rssi = le32_to_cpu(wlan_info->last_ack_rssi);
		tmp->tx_waste_time = le32_to_cpu(wlan_info->tx_waste_time);
		tmp->rx_time = le32_to_cpu(wlan_info->rx_time);
		tmp->phyerr_cnt = le32_to_cpu(wlan_info->phyerr_cnt);
		tmp->listen_time = le32_to_cpu(wlan_info->listen_time);
		tmp->reg_tx_frame_cnt = le32_to_cpu(wlan_info->reg_tx_frame_cnt);
		tmp->reg_rx_frame_cnt = le32_to_cpu(wlan_info->reg_rx_frame_cnt);
		tmp->reg_rxclr_cnt = le32_to_cpu(wlan_info->reg_rxclr_cnt);
		tmp->reg_cycle_cnt = le32_to_cpu(wlan_info->reg_cycle_cnt);
		tmp->reg_rxclr_ext_cnt = le32_to_cpu(wlan_info->reg_rxclr_ext_cnt);
		tmp->reg_ofdm_phyerr_cnt = le32_to_cpu(wlan_info->reg_ofdm_phyerr_cnt);
		tmp->reg_cck_phyerr_cnt = le32_to_cpu(wlan_info->reg_cck_phyerr_cnt);
		tmp->chan_nf = le32_to_cpu(wlan_info->chan_nf);
		tmp->my_bss_rx_cycle_count =
			le32_to_cpu(wlan_info->my_bss_rx_cycle_count);
		break;
	default:
		ath12k_warn(ab,
			    "Received invalid tag for wmi dcs interference in subtlvs\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ath12k_wmi_dcs_event_parser(struct ath12k_base *ab,
				       u16 tag, u16 len,
				       const void *ptr, void *data)
{
	int ret = 0;

	ath12k_dbg(ab, ATH12K_DBG_WMI, "wmi dcs awgn event tag 0x%x of len %d rcvd\n",
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
		ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					  ath12k_wmi_dcs_intf_subtlv_parser,
					  data);
		break;
	default:
		ath12k_warn(ab, "Received invalid tag for wmi dcs interference event\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

bool ath12k_wmi_validate_dcs_awgn_info(struct ath12k *ar, struct wmi_dcs_awgn_info *awgn_info)
{
	spin_lock_bh(&ar->data_lock);

	if (!ar->rx_channel) {
		spin_unlock_bh(&ar->data_lock);
		return false;
	}

	if (awgn_info->chan_freq != ar->rx_channel->center_freq) {
		spin_unlock_bh(&ar->data_lock);
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "dcs interference event received with wrong channel %d",awgn_info->chan_freq);
		return false;
	}
	spin_unlock_bh(&ar->data_lock);

	switch (awgn_info->channel_width) {
	case WMI_HOST_CHAN_WIDTH_20:
		if (awgn_info->chan_bw_interference_bitmap > WMI_DCS_SEG_PRI20) {
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "dcs interference event received with wrong chan width bmap %d for 20MHz",
				   awgn_info->chan_bw_interference_bitmap);
			return false;
		}
		break;
	case WMI_HOST_CHAN_WIDTH_40:
		if (awgn_info->chan_bw_interference_bitmap > (WMI_DCS_SEG_PRI20 |
							      WMI_DCS_SEG_SEC20)) {
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "dcs interference event received with wrong chan width bmap %d for 40MHz",
				   awgn_info->chan_bw_interference_bitmap);
			return false;
		}
		break;
	case WMI_HOST_CHAN_WIDTH_80:
		if (awgn_info->chan_bw_interference_bitmap > (WMI_DCS_SEG_PRI20 |
							      WMI_DCS_SEG_SEC20 |
							      WMI_DCS_SEG_SEC40)) {
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "dcs interference event received with wrong chan width bmap %d for 80MHz",
				   awgn_info->chan_bw_interference_bitmap);
			return false;
		}
		break;
	case WMI_HOST_CHAN_WIDTH_160:
	case WMI_HOST_CHAN_WIDTH_80P80:
		if (awgn_info->chan_bw_interference_bitmap > (WMI_DCS_SEG_PRI20 |
							      WMI_DCS_SEG_SEC20 |
							      WMI_DCS_SEG_SEC40 |
							      WMI_DCS_SEG_SEC80)) {
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "dcs interference event received with wrong chan width bmap %d for 80P80/160MHz",
				   awgn_info->chan_bw_interference_bitmap);
			return false;
		}
		break;
	case WMI_HOST_CHAN_WIDTH_320:
		if (awgn_info->chan_bw_interference_bitmap > (WMI_DCS_SEG_PRI20 |
							      WMI_DCS_SEG_SEC20 |
							      WMI_DCS_SEG_SEC40 |
							      WMI_DCS_SEG_SEC80 |
							      WMI_DCS_SEG_SEC160)) {
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "dcs interference event received with wrong chan width bmap %d for 320MHz",
				   awgn_info->chan_bw_interference_bitmap);
			return false;
		}
        break;
	default:
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "dcs interference event received with unknown channel width %d",
			   awgn_info->channel_width);
		return false;
	}
	return true;
}

static void
ath12k_wmi_dcs_cw_interference_event(struct ath12k_base *ab,
				     struct sk_buff *skb,
				     u32 pdev_id)
{
	struct ath12k *ar;
	struct wmi_dcs_cw_info cw_info = {};
	struct ieee80211_chanctx_conf *chanctx_conf;
	struct ath12k_mac_get_any_chanctx_conf_arg arg;
	struct ath12k_hw *ah;
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_dcs_event_parser,
				  &cw_info);
	if (ret) {
		ath12k_warn(ab, "failed to parse cw tlv %d\n", ret);
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		ath12k_warn(ab, "CW detected in invalid pdev id(%d)\n",
			    pdev_id);
		goto exit;
	}

	spin_lock_bh(&ar->data_lock);
	if (!(ar->dcs_enable_bitmap & WMI_DCS_CW_INTF)) {
		/* TODO - incase Fw missed the pdev set param
		 * to disable CW Interference
		 */
		spin_unlock_bh(&ar->data_lock);
		goto exit;
	}
	spin_unlock_bh(&ar->data_lock);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "CW Interference detected for pdev=%d\n",
		   pdev_id);

	ah = ar->ah;

	arg.ar = ar;
	arg.chanctx_conf = NULL;
	ieee80211_iter_chan_contexts_atomic(ah->hw, ath12k_mac_get_any_chanctx_conf_iter,
					    &arg);
	chanctx_conf = arg.chanctx_conf;
	if (!chanctx_conf) {
		ath12k_warn(ab, "chanctx_conf is not available\n");
		goto exit;
	}
	ieee80211_cw_detected(ah->hw, chanctx_conf->def.chan);
exit:
	rcu_read_unlock();
}

static void
ath12k_wmi_dcs_wlan_interference_event(struct ath12k_base *ab,
				       struct sk_buff *skb,
				       u32 pdev_id)
{
	struct wmi_dcs_wlan_interference_stats wlan_info = {};
	struct ath12k *ar;
	int ret;
	struct ath12k_dcs_wlan_interference *dcs_wlan_intf;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_dcs_event_parser,
				  &wlan_info);
	if (ret)
		return;

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar)
		goto exit;

	spin_lock_bh(&ar->data_lock);
	if (!(ar->dcs_enable_bitmap & WMI_DCS_WLAN_INTF)) {
		spin_unlock_bh(&ar->data_lock);
		goto exit;
	}
	spin_unlock_bh(&ar->data_lock);

	dcs_wlan_intf = kzalloc(sizeof(*dcs_wlan_intf), GFP_ATOMIC);
	if (!dcs_wlan_intf)
		goto exit;

	INIT_LIST_HEAD(&dcs_wlan_intf->list);
	memcpy(&dcs_wlan_intf->info, &wlan_info, sizeof(wlan_info));
	spin_lock_bh(&ar->data_lock);
	list_add_tail(&dcs_wlan_intf->list, &ar->wlan_intf_list);
	spin_unlock_bh(&ar->data_lock);
	schedule_work(&ar->wlan_intf_work);
exit:
	rcu_read_unlock();
}

static void
ath12k_wmi_dcs_awgn_interference_event(struct ath12k_base *ab,
				       struct sk_buff *skb,
				       u32 pdev_id)
{
	struct ath12k_mac_get_any_chanctx_conf_arg arg;
	struct wmi_dcs_awgn_info awgn_info = {};
	struct cfg80211_chan_def *chandef;
	struct ath12k *ar;
	int ret;

	if (!test_bit(WMI_TLV_SERVICE_DCS_AWGN_INT_SUPPORT, ab->wmi_ab.svc_map)) {
		ath12k_warn(ab, "firmware doesn't support awgn interference, so dropping dcs interference ev\n");
		return;
	}

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_dcs_event_parser,
				  &awgn_info);
	if (ret) {
		ath12k_warn(ab, "failed to parse awgn tlv %d\n", ret);
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		ath12k_warn(ab, "awgn detected in invalid pdev id(%d)\n",
			    pdev_id);
		goto exit;
	}
	if (!ar->supports_6ghz) {
		ath12k_warn(ab, "pdev does not supports 6G, so dropping dcs interference event\n");
		goto exit;
	}

	spin_lock_bh(&ar->data_lock);
	if (ar->awgn_intf_handling_in_prog) {
		spin_unlock_bh(&ar->data_lock);
		rcu_read_unlock();
		return;
	}
	spin_unlock_bh(&ar->data_lock);

	if (!ath12k_wmi_validate_dcs_awgn_info(ar, &awgn_info)) {
		ath12k_warn(ab, "Invalid DCS AWGN TLV - Skipping event");
		goto exit;
	}

	ath12k_info(ab, "Interface(pdev %d) : AWGN interference detected\n",
		    pdev_id);

	arg.ar = ar;
	arg.chanctx_conf = NULL;
	ieee80211_iter_chan_contexts_atomic(ath12k_ar_to_hw(ar),
					    ath12k_mac_get_any_chanctx_conf_iter, &arg);
	if (!arg.chanctx_conf) {
		ath12k_warn(ab, "failed to find valid chanctx_conf in AWGN event\n");
		goto exit;
	}
	chandef = &arg.chanctx_conf->def;
	if (!chandef) {
		ath12k_warn(ab, "chandef is not available\n");
		goto exit;
	}
	ar->awgn_chandef = *chandef;

	ieee80211_awgn_detected(ath12k_ar_to_hw(ar), awgn_info.chan_bw_interference_bitmap,
				chandef->chan);

	spin_lock_bh(&ar->data_lock);
	ar->awgn_intf_handling_in_prog = true;
	ar->chan_bw_interference_bitmap = awgn_info.chan_bw_interference_bitmap;
	spin_unlock_bh(&ar->data_lock);

	ath12k_dbg(ab, ATH12K_DBG_WMI, "AWGN : Interference handling started\n");
exit:
	rcu_read_unlock();
}

static void
ath12k_wmi_process_csa_switch_count_event(struct ath12k_base *ab,
					  const struct ath12k_wmi_pdev_csa_event *ev,
					  const u32 *vdev_ids)
{
	u32 current_switch_count = le32_to_cpu(ev->current_switch_count);
	u32 num_vdevs = le32_to_cpu(ev->num_vdevs);
	struct ieee80211_bss_conf *conf;
	struct ath12k_link_vif *arvif;
	struct ath12k_vif *ahvif;
	int i;

	rcu_read_lock();
	for (i = 0; i < num_vdevs; i++) {
		arvif = ath12k_mac_get_arvif_by_vdev_id(ab, vdev_ids[i]);

		if (!arvif) {
			ath12k_warn(ab, "Recvd csa status for unknown vdev %d",
				    vdev_ids[i]);
			continue;
		}
		ahvif = arvif->ahvif;

		if (ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR)
			continue;

		if (arvif->link_id >= IEEE80211_MLD_MAX_NUM_LINKS) {
			ath12k_warn(ab, "Invalid CSA switch count even link id: %d\n",
				    arvif->link_id);
			continue;
		}

		conf = rcu_dereference(ahvif->vif->link_conf[arvif->link_id]);
		if (!conf) {
			ath12k_warn(ab, "unable to access bss link conf in process csa for vif %pM link %u\n",
				    ahvif->vif->addr, arvif->link_id);
			continue;
		}

		if (!arvif->is_up || !conf->csa_active)
			continue;

		/* Finish CSA when counter reaches zero */
		if (!current_switch_count) {
			ieee80211_csa_finish(ahvif->vif, arvif->link_id);
			arvif->current_cntdown_counter = 0;
		} else if (current_switch_count > 1) {
			/* If the count in event is not what we expect, don't update the
			 * mac80211 count. Since during beacon Tx failure, count in the
			 * firmware will not decrement and this event will come with the
			 * previous count value again
			 */
			if (current_switch_count != arvif->current_cntdown_counter)
				continue;

			arvif->current_cntdown_counter =
				ieee80211_beacon_update_cntdwn(ahvif->vif,
							       arvif->link_id);
		}
	}
	rcu_read_unlock();
}

static void
ath12k_wmi_pdev_csa_switch_count_status_event(struct ath12k_base *ab,
					      struct sk_buff *skb)
{
	const void **tb;
	const struct ath12k_wmi_pdev_csa_event *ev;
	const u32 *vdev_ids;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_PDEV_CSA_SWITCH_COUNT_STATUS_EVENT];
	vdev_ids = tb[WMI_TAG_ARRAY_UINT32];

	if (!ev || !vdev_ids) {
		ath12k_warn(ab, "failed to fetch pdev csa switch count ev");
		kfree(tb);
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pdev csa switch count %d for pdev %d, num_vdevs %d",
		   ev->current_switch_count, ev->pdev_id,
		   ev->num_vdevs);

	ath12k_wmi_process_csa_switch_count_event(ab, ev, vdev_ids);

	kfree(tb);
}

static void
ath12k_dfs_calculate_subchannels(struct ath12k_base *ab,
				 const struct ath12k_wmi_pdev_radar_event *radar,
				 bool do_full_bw_nol)
{
	u32 radar_found_freq, sub_channel_cfreq, radar_found_freq_low, radar_found_freq_high;
	struct ath12k_mac_get_any_chanctx_conf_arg arg;
	u16 radar_bitmap = 0, subchannel_count;
	bool chandef_device_present = false;
	struct cfg80211_chan_def *chandef;
	enum nl80211_chan_width width;
	struct ath12k *ar;
	u32 center_freq;
	int i;

	ar = ath12k_mac_get_ar_by_pdev_id(ab, radar->pdev_id);
	if (!ar) {
		ath12k_warn(ab, "Failed to fetch ar for pdev %d\n",
			    radar->pdev_id);
		return;
	}
	arg.ar = ar;
	arg.chanctx_conf = NULL;
	if (!radar->detector_id) {
		ieee80211_iter_chan_contexts_atomic(ath12k_ar_to_hw(ar),
						    ath12k_mac_get_any_chanctx_conf_iter, &arg);
		if (!arg.chanctx_conf) {
			ath12k_warn(ab, "failed to find valid chanctx_conf in radar detected event\n");
			return;
		}
		chandef = &arg.chanctx_conf->def;
		chandef_device_present = cfg80211_chandef_device_present(chandef);
	} else {
		chandef = &ar->agile_chandef;
	}

	if (!chandef) {
		ath12k_warn(ab, "chandef information is not available\n");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI, " Operating freq:%u center_freq1:%u, center_freq2:%u",
		   chandef->chan->center_freq, chandef->center_freq1,chandef->center_freq2);

	if (chandef_device_present)
		width = chandef->width_device;
	else
		width = chandef->width;

	subchannel_count = ath12k_calculate_subchannel_count(width);
	if (!subchannel_count) {
		ath12k_warn(ab, "invalid subchannel count for bandwidth=%d\n", width);
		goto mark_radar;
	}

	if (do_full_bw_nol) {
		ath12k_dbg(ab, ATH12K_DBG_WMI, "put all channels in NOL\n");
		goto mark_radar;
	}
	ath12k_dbg(ab, ATH12K_DBG_WMI, "perform channel submarking\n");

	if (chandef_device_present)
		center_freq = chandef->center_freq_device;
	else
		center_freq = chandef->center_freq1;

	radar_found_freq = center_freq + radar->freq_offset;

	radar_found_freq_high = radar_found_freq_low = radar_found_freq;

	if (radar->is_chirp) {
		radar_found_freq_high = radar_found_freq_high + 10;
		radar_found_freq_low  = radar_found_freq_low  - 10;
	}

	sub_channel_cfreq = center_freq - ((subchannel_count-1) * 10);

	for(i=0; i < subchannel_count; i++) {
		if (sub_channel_cfreq >= 5260 &&
		    ((radar_found_freq_low >= sub_channel_cfreq-10 &&
		    radar_found_freq_low <= sub_channel_cfreq+10) ||
		    (radar_found_freq_high >= sub_channel_cfreq-10 &&
		    radar_found_freq_high <= sub_channel_cfreq+10)))
			radar_bitmap |= 1 << i;

		sub_channel_cfreq += 20;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI, "radar_bitmap:%0x and subchannel_count:%d",
		   radar_bitmap,subchannel_count);

mark_radar:
	if (!radar->detector_id) {
		ieee80211_radar_detected_bitmap(ar->ah->hw, radar_bitmap, chandef->chan);
	} else {
		ar->agile_chandef.radar_bitmap = radar_bitmap;
		ath12k_mac_background_dfs_event(ar, ATH12K_BGDFS_RADAR);
	}
}

static void
ath12k_wmi_pdev_dfs_radar_detected_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	const struct wmi_pdev_radar_flags_param *rf_ev;
	const struct ath12k_wmi_pdev_radar_event *ev;
	struct ath12k *ar;
	bool do_full_bw_nol = false;
	bool is_full_bw_nol_feature_supported = false;
	const struct wmi_tlv *tlv;
	u16 tlv_tag;
	u32 len = 0;
	void *ptr;

	ptr = skb->data;

	len += sizeof(*ev) + TLV_HDR_SIZE;
	if (skb->len < len) {
		ath12k_warn(ab, "Radar event is of incorrect length\n");
		return;
	}

	tlv = ptr;
	tlv_tag = le32_get_bits(tlv->header, WMI_TLV_TAG);

	ptr += sizeof(*tlv);
	if (tlv_tag != WMI_TAG_PDEV_DFS_RADAR_DETECTION_EVENT) {
		ath12k_warn(ab, "pdev dfs event received with wrong tag %x\n", tlv_tag);
		return;
	}
	ev = ptr;
	ptr += sizeof(*ev);

	is_full_bw_nol_feature_supported = test_bit(WMI_TLV_SERVICE_RADAR_FLAGS_SUPPORT,
						    ab->wmi_ab.svc_map);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "pdev dfs radar event found\n");
	ath12k_dbg(ab, ATH12K_DBG_WMI, "pdev dfs radar flags host support %x\n",
		   is_full_bw_nol_feature_supported);
	if (is_full_bw_nol_feature_supported) {
		/* Expect an array TLV containing a single radar flags param TLV */
		len += sizeof(*tlv) + sizeof(*tlv) + sizeof(*rf_ev);
		if (skb->len < len) {
			ath12k_warn(ab, "pdev dfs radar flag event size invalid\n");
			return;
		}

		/* Skip Array TLV Tag */
		ptr += sizeof(*tlv);

		tlv = ptr;
		tlv_tag = le32_get_bits(tlv->header, WMI_TLV_TAG);
		ptr += sizeof(*tlv);
		if (tlv_tag != WMI_TAG_PDEV_DFS_RADAR_FLAGS) {
			ath12k_warn(ab, "pdev dfs radar flag event received with wrong tag\n");
			return;
		}

		rf_ev = ptr;
		do_full_bw_nol = le32_to_cpu(rf_ev->radar_flags) &
				 (1 << WMI_PDEV_RADAR_FLAGS_FULL_BW_NOL_MARK_BIT);
		ath12k_dbg(ab, ATH12K_DBG_WMI, "pdev dfs radar flag event found, radar_flag_bit %d\n",
			   do_full_bw_nol);
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pdev dfs radar detected on pdev %d, detection mode %d, chan freq %d, chan_width %d, detector id %d, seg id %d, timestamp %d, chirp %d, freq offset %d, sidx %d",
		   ev->pdev_id, ev->detection_mode, ev->chan_freq, ev->chan_width,
		   ev->detector_id, ev->segment_id, ev->timestamp, ev->is_chirp,
		   ev->freq_offset, ev->sidx);

	rcu_read_lock();

	ar = ath12k_mac_get_ar_by_pdev_id(ab, le32_to_cpu(ev->pdev_id));
	if (!ar) {
		ath12k_warn(ab, "radar detected in invalid pdev %d\n",
			    ev->pdev_id);
		rcu_read_unlock();
		return;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_REG, "DFS Radar Detected in pdev %d\n",
		   ev->pdev_id);

	if (ar->dfs_block_radar_events)
		ath12k_info(ab, "DFS Radar detected, but ignored as requested\n");
	else
		ath12k_dfs_calculate_subchannels(ab, ev, do_full_bw_nol);

	rcu_read_unlock();
}

static void
ath12k_wmi_dcs_interference_event(struct ath12k_base *ab,
				  struct sk_buff *skb)
{
	const struct wmi_dcs_interference_ev *dcs_intf_ev;
	const struct wmi_tlv *tlv;
	u32 pdev_id, interference_type;
	u16 tlv_tag;
	u8 *ptr;

	ptr = skb->data;

	if (skb->len < (sizeof(*dcs_intf_ev) + TLV_HDR_SIZE)) {
		ath12k_warn(ab, "dcs interference event size invalid\n");
		return;
	}

	tlv = (struct wmi_tlv *)ptr;
	tlv_tag = u32_get_bits(tlv->header, WMI_TLV_TAG);
	ptr += sizeof(*tlv);

	if (tlv_tag == WMI_TAG_DCS_INTERFERENCE_EVENT) {
		dcs_intf_ev = (struct wmi_dcs_interference_ev *)ptr;
		pdev_id = le32_to_cpu(dcs_intf_ev->pdev_id);
		interference_type = le32_to_cpu(dcs_intf_ev->interference_type);
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "Interference detected on pdev %d, interference type %d\n",
			   pdev_id, interference_type);
	} else {
		ath12k_warn(ab, "dcs interference event received with wrong tag\n");
		return;
	}

	switch (interference_type) {
	case WMI_DCS_CW_INTF:
		ath12k_wmi_dcs_cw_interference_event(ab, skb, pdev_id);
		break;
	case WMI_DCS_WLAN_INTF:
		ath12k_wmi_dcs_wlan_interference_event(ab, skb, pdev_id);
		break;
	case WMI_DCS_AWGN_INTF:
		ath12k_wmi_dcs_awgn_interference_event(ab, skb, pdev_id);
		break;
	default:
		ath12k_warn(ab,
			    "For pdev=%d, Invalid Interference type=%d received from FW",
			    pdev_id, interference_type);
		break;
	}
}

static void ath12k_tm_wmi_event_segmented(struct ath12k_base *ab, u32 cmd_id,
					  struct sk_buff *skb)
{
	const void **tb;
	int ret;
	u16 length;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);

	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse ftm event tlv: %d\n", ret);
		return;
	}

	length = skb->len - TLV_HDR_SIZE;
	ath12k_tm_process_event(ab, cmd_id, tb, length);
	kfree(tb);
	tb = NULL;
}

static void
ath12k_wmi_pdev_temperature_event(struct ath12k_base *ab,
				  struct sk_buff *skb)
{
	const struct wmi_pdev_temperature_event *ev;
	struct ath12k *ar;
	const void **tb;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
	       ret = PTR_ERR(tb);
	   ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
	   return;
	}

	ev = tb[WMI_TAG_PDEV_TEMPERATURE_EVENT];
	if (!ev) {
	    ath12k_warn(ab, "failed to fetch pdev temp ev");
	    kfree(tb);
	    return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
			"pdev temperature ev temp %d pdev_id %d\n", ev->temp,
			ev->pdev_id);

	rcu_read_lock();

	ar = ath12k_mac_get_ar_by_pdev_id(ab, le32_to_cpu(ev->pdev_id));
	if (!ar) {
		ath12k_warn(ab, "invalid pdev id in pdev temperature ev %d", ev->pdev_id);
		goto exit;
	}

	ath12k_thermal_event_temperature(ar, ev->temp);
exit:
	kfree(tb);
	rcu_read_unlock();
}

static int ath12k_wmi_stats_parser(struct ath12k_base *ab,
				   u16 tag, u16 tag_len,
				   const void *ptr,
				   void *data)
{
	int ret = 0;
	u16 tlv_tag, tlv_len, len = 0;
	const struct wmi_tlv *tlv;
	struct wmi_therm_throt_level_stats_info *tt_stats = data;

	switch (tag) {
	case WMI_TAG_THERM_THROT_STATS_EVENT:
		break;
	case WMI_TAG_ARRAY_STRUCT:
		len = tag_len;
		tlv = (struct wmi_tlv *)ptr;
		tlv_tag = u32_get_bits(tlv->header, WMI_TLV_TAG);

		while (len > 0) {
			len -= sizeof(*tlv);
			tlv_len = le32_get_bits(tlv->header, WMI_TLV_LEN);
			ptr += sizeof(*tlv);
			struct wmi_therm_throt_level_stats_info *stats;

			stats = (struct wmi_therm_throt_level_stats_info *)ptr;

			memcpy(tt_stats, stats,
			       sizeof(struct wmi_therm_throt_level_stats_info));
			ptr += tlv_len;
			tt_stats++;
			len -= tlv_len;
		}
		break;
	default:
		ath12k_warn(ab, "Invalid tag received tag %d len %d\n",
			    tag, len);
		return -EINVAL;
	}
	return ret;
}

static void ath12k_wmi_thermal_throt_stats_event(struct ath12k_base *ab,
						 struct sk_buff *skb)
{
	struct ath12k *ar;
	const void **tb;
	int ret;
	const struct wmi_therm_throt_stats_event *ev;
	struct wmi_therm_throt_level_stats_info *stats;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_err(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_THERM_THROT_STATS_EVENT];
	if (!ev) {
		ath12k_err(ab, "failed to fetch thermal throt stats ev");
		goto err;
	}

	/* Print debug only if DUT temperature is not in optimal range as this
	 * event is received once on every 2 DC
	 */
	if (ev->level > 0)
		ath12k_dbg(ab, ATH12K_DBG_WMI, "thermal stats ev level %d pdev_id %d\n",
			   ev->level, ev->pdev_id);

	ar = ath12k_mac_get_ar_by_pdev_id(ab, ev->pdev_id);
	if (!ar)
		goto err;

	stats = ar->tt_level_stats;
	memcpy(&ar->tt_current_state, ev, sizeof(struct wmi_therm_throt_stats_event));
	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_stats_parser,
				  stats);

	ath12k_thermal_event_throt_level(ar, ev->level);

err:
	kfree(tb);
}

static void ath12k_fils_discovery_event(struct ath12k_base *ab,
					struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_fils_discovery_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab,
			    "failed to parse FILS discovery event tlv %d\n",
			    ret);
		return;
	}

	ev = tb[WMI_TAG_HOST_SWFDA_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch FILS discovery event\n");
		kfree(tb);
		return;
	}

	ath12k_warn(ab,
		    "FILS discovery frame expected from host for vdev_id: %u, transmission scheduled at %u, next TBTT: %u\n",
		    ev->vdev_id, ev->fils_tt, ev->tbtt);

	kfree(tb);
}

static void ath12k_probe_resp_tx_status_event(struct ath12k_base *ab,
					      struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_probe_resp_tx_status_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab,
			    "failed to parse probe response transmission status event tlv: %d\n",
			    ret);
		return;
	}

	ev = tb[WMI_TAG_OFFLOAD_PRB_RSP_TX_STATUS_EVENT];
	if (!ev) {
		ath12k_warn(ab,
			    "failed to fetch probe response transmission status event");
		kfree(tb);
		return;
	}

	if (ev->tx_status)
		ath12k_warn(ab,
			    "Probe response transmission failed for vdev_id %u, status %u\n",
			    ev->vdev_id, ev->tx_status);

	kfree(tb);
}

static int ath12k_wmi_p2p_noa_event(struct ath12k_base *ab,
				    struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_p2p_noa_event *ev;
	const struct ath12k_wmi_p2p_noa_info *noa;
	struct ath12k *ar;
	int ret, vdev_id;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse P2P NoA TLV: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_P2P_NOA_EVENT];
	noa = tb[WMI_TAG_P2P_NOA_INFO];

	if (!ev || !noa) {
		ret = -EPROTO;
		goto out;
	}

	vdev_id = __le32_to_cpu(ev->vdev_id);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "wmi tlv p2p noa vdev_id %i descriptors %u\n",
		   vdev_id, le32_get_bits(noa->noa_attr, WMI_P2P_NOA_INFO_DESC_NUM));

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id %d in P2P NoA event\n",
			    vdev_id);
		ret = -EINVAL;
		goto unlock;
	}

	ath12k_p2p_noa_update_by_vdev_id(ar, vdev_id, noa);

	ret = 0;

unlock:
	rcu_read_unlock();
out:
	kfree(tb);
	return ret;
}

static void ath12k_rfkill_state_change_event(struct ath12k_base *ab,
					     struct sk_buff *skb)
{
	const struct wmi_rfkill_state_change_event *ev;
	const void **tb;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_RFKILL_EVENT];
	if (!ev) {
		kfree(tb);
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_MAC,
		   "wmi tlv rfkill state change gpio %d type %d radio_state %d\n",
		   le32_to_cpu(ev->gpio_pin_num),
		   le32_to_cpu(ev->int_type),
		   le32_to_cpu(ev->radio_state));

	spin_lock_bh(&ab->base_lock);
	ab->rfkill_radio_on = (ev->radio_state == cpu_to_le32(WMI_RFKILL_RADIO_STATE_ON));
	spin_unlock_bh(&ab->base_lock);

	queue_work(ab->workqueue, &ab->rfkill_work);
	kfree(tb);
}

static void
ath12k_wmi_diag_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	const struct wmi_tlv *tlv;
	u16 tlv_tag, tlv_len;
	u32 *dev_id;
	u8 *data;

	tlv = (struct wmi_tlv *)skb->data;
	tlv_tag = FIELD_GET(WMI_TLV_TAG, tlv->header);
	tlv_len = FIELD_GET(WMI_TLV_LEN, tlv->header);

	if (tlv_tag == WMI_TAG_ARRAY_BYTE) {
		data = skb->data + sizeof(struct wmi_tlv);
		dev_id = (uint32_t *)data;
		*dev_id = ab->hw_params->hw_rev + ATH12K_DIAG_HW_ID_OFFSET;
	} else {
		ath12k_warn(ab, "WMI Diag Event missing required tlv\n");
		return;
	}

	trace_ath12k_wmi_diag(ab, skb->data, skb->len);

	ath12k_fwlog_write(ab, data, tlv_len);
}

static void ath12k_wmi_twt_enable_event(struct ath12k_base *ab,
					struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_twt_enable_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse wmi twt enable status event tlv: %d\n",
			    ret);
		return;
	}

	ev = tb[WMI_TAG_TWT_ENABLE_COMPLETE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch twt enable wmi event\n");
		goto exit;
	}

	ath12k_dbg(ab, ATH12K_DBG_MAC, "wmi twt enable event pdev id %u status %u\n",
		   le32_to_cpu(ev->pdev_id),
		   le32_to_cpu(ev->status));

exit:
	kfree(tb);
}

static void ath12k_wmi_twt_disable_event(struct ath12k_base *ab,
					 struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_twt_disable_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse wmi twt disable status event tlv: %d\n",
			    ret);
		return;
	}

	ev = tb[WMI_TAG_TWT_DISABLE_COMPLETE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch twt disable wmi event\n");
		goto exit;
	}

	ath12k_dbg(ab, ATH12K_DBG_MAC, "wmi twt disable event pdev id %d status %u\n",
		   le32_to_cpu(ev->pdev_id),
		   le32_to_cpu(ev->status));

exit:
	kfree(tb);
}

static void ath12k_wmi_twt_btwt_invite_sta_compl_event(struct ath12k_base *ab,
						       struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_twt_btwt_invite_sta_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse wmi btwt invite sta event tlv: %d\n",
			    ret);
		return;
	}

	ev = tb[WMI_TAG_TWT_BTWT_INVITE_STA_COMPLETE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch btwt invite sta event\n");
		goto exit;
	}

	ath12k_info(ab, "wmi btwt invite sta event vdev id %u peer %pM dialog %d status %u\n",
		   le32_to_cpu(ev->vdev_id), ev->peer_macaddr.addr,
		   le32_to_cpu(ev->dialog_id),
		   le32_to_cpu(ev->status));

exit:
	kfree(tb);
}

static void ath12k_wmi_twt_btwt_remove_sta_compl_event(struct ath12k_base *ab,
						       struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_twt_btwt_remove_sta_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse wmi btwt remove sta event tlv: %d\n",
			    ret);
		return;
	}

	ev = tb[WMI_TAG_TWT_BTWT_REMOVE_STA_COMPLETE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch btwt remove sta event\n");
		goto exit;
	}

	ath12k_info(ab, "wmi btwt remove sta event vdev id %u peer %pM dialog %d status %u\n",
		   le32_to_cpu(ev->vdev_id), ev->peer_macaddr.addr,
		   le32_to_cpu(ev->dialog_id),
		   le32_to_cpu(ev->status));

exit:
	kfree(tb);
}

static int ath12k_wmi_wow_wakeup_host_parse(struct ath12k_base *ab,
					    u16 tag, u16 len,
					    const void *ptr, void *data)
{
	const struct wmi_wow_ev_pg_fault_param *pf_param;
	const struct wmi_wow_ev_param *param;
	struct wmi_wow_ev_arg *arg = data;
	int pf_len;

	switch (tag) {
	case WMI_TAG_WOW_EVENT_INFO:
		param = ptr;
		arg->wake_reason = le32_to_cpu(param->wake_reason);
		ath12k_dbg(ab, ATH12K_DBG_WMI, "wow wakeup host reason %d %s\n",
			   arg->wake_reason, wow_reason(arg->wake_reason));
		break;

	case WMI_TAG_ARRAY_BYTE:
		if (arg && arg->wake_reason == WOW_REASON_PAGE_FAULT) {
			pf_param = ptr;
			pf_len = le32_to_cpu(pf_param->len);
			if (pf_len > len - sizeof(pf_len) ||
			    pf_len < 0) {
				ath12k_warn(ab, "invalid wo reason page fault buffer len %d\n",
					    pf_len);
				return -EINVAL;
			}
			ath12k_dbg(ab, ATH12K_DBG_WMI, "wow_reason_page_fault len %d\n",
				   pf_len);
			ath12k_dbg_dump(ab, ATH12K_DBG_WMI,
					"wow_reason_page_fault packet present",
					"wow_pg_fault ",
					pf_param->data,
					pf_len);
		}
		break;
	default:
		break;
	}

	return 0;
}

static void ath12k_wmi_event_wow_wakeup_host(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_wow_ev_arg arg = { };
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_wow_wakeup_host_parse,
				  &arg);
	if (ret) {
		ath12k_warn(ab, "failed to parse wmi wow wakeup host event tlv: %d\n",
			    ret);
		return;
	}

	complete(&ab->wow.wakeup_completed);
}

static void ath12k_wmi_gtk_offload_status_event(struct ath12k_base *ab,
						struct sk_buff *skb)
{
	const struct wmi_gtk_offload_status_event *ev;
	struct ath12k_link_vif *arvif;
	__be64 replay_ctr_be;
	u64 replay_ctr;
	const void **tb;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_GTK_OFFLOAD_STATUS_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch gtk offload status ev");
		kfree(tb);
		return;
	}

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif_by_vdev_id(ab, le32_to_cpu(ev->vdev_id));
	if (!arvif) {
		rcu_read_unlock();
		ath12k_warn(ab, "failed to get arvif for vdev_id:%d\n",
			    le32_to_cpu(ev->vdev_id));
		kfree(tb);
		return;
	}

	replay_ctr = le64_to_cpu(ev->replay_ctr);
	arvif->rekey_data.replay_ctr = replay_ctr;
	ath12k_dbg(ab, ATH12K_DBG_WMI, "wmi gtk offload event refresh_cnt %d replay_ctr %llu\n",
		   le32_to_cpu(ev->refresh_cnt), replay_ctr);

	/* supplicant expects big-endian replay counter */
	replay_ctr_be = cpu_to_be64(replay_ctr);

	ieee80211_gtk_rekey_notify(arvif->ahvif->vif, arvif->bssid,
				   (void *)&replay_ctr_be, GFP_ATOMIC);

	rcu_read_unlock();

	kfree(tb);
}

static void ath12k_wmi_event_mlo_setup_complete(struct ath12k_base *ab,
						struct sk_buff *skb)
{
	const struct wmi_mlo_setup_complete_event *ev;
	struct ath12k *ar = NULL;
	struct ath12k_pdev *pdev;
	u32 max_ml_peer_ids;
	const void **tb;
	int ret, i;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse mlo setup complete event tlv: %d\n",
			    ret);
		return;
	}

	ev = tb[WMI_TAG_MLO_SETUP_COMPLETE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch mlo setup complete event\n");
		kfree(tb);
		return;
	}

	if (le32_to_cpu(ev->pdev_id) > ab->num_radios)
		goto skip_lookup;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		if (pdev && pdev->pdev_id == le32_to_cpu(ev->pdev_id)) {
			ar = pdev->ar;
			break;
		}
	}

skip_lookup:
	if (!ar) {
		ath12k_warn(ab, "invalid pdev_id %d status %u in setup complete event\n",
			    ev->pdev_id, ev->status);
		goto out;
	}

	ar->mlo_setup_status = le32_to_cpu(ev->status);
	max_ml_peer_ids = le32_to_cpu(ev->max_ml_peer_ids);
	if (!max_ml_peer_ids ||
	    max_ml_peer_ids > ATH12K_MAX_MLO_PEERS) {
		ath12k_warn(ab, "Invalid ML peer ids from firmware %d", max_ml_peer_ids);
		max_ml_peer_ids = ATH12K_MAX_MLO_PEERS;
	}
	ar->ah->max_ml_peer_ids = max_ml_peer_ids;
	if (ab->hw_params->hal_ops->hal_get_tqm_scratch_reg)
		ab->hw_params->hal_ops->hal_get_tqm_scratch_reg(ab, &ar->delta_tqm);
	complete(&ar->mlo_setup_done);

out:
	kfree(tb);
}

static void ath12k_wmi_event_teardown_complete(struct ath12k_base *ab,
					       struct sk_buff *skb)
{
	const struct wmi_mlo_teardown_complete_event *ev;
	struct ath12k_hw_group *ag = ab->ag;
	bool complete_flag = true;
	struct ath12k_pdev *pdev;
	struct ath12k_hw *ah;
	struct ath12k *ar = NULL;
	const void **tb;
	int i, j, ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse teardown complete event tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_MLO_TEARDOWN_COMPLETE];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch teardown complete event\n");
		kfree(tb);
		return;
	}

	kfree(tb);

	if (ev->pdev_id > ab->num_radios)
		return;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];

		if (pdev && pdev->pdev_id == ev->pdev_id)
			ar = pdev->ar;
	}

	if (!ar) {
		ath12k_warn(ab, "invalid pdev id in teardown complete ev %d",
			    ev->pdev_id);
		return;
	}

	ar->teardown_complete_event = true;
	for (i = 0; i < ag->num_hw; i++) {
		ah = ag->ah[i];
		if (!ah)
			continue;

		for_each_ar(ah, ar, j) {
			ar = &ah->radio[j];

			if (!ar->teardown_complete_event)
				complete_flag = false;
		}
	}
	if (complete_flag && ag->trigger_umac_reset) {
                complete(&ag->umac_reset_complete);
		ag->trigger_umac_reset = false;
	}
}

#ifdef CPTCFG_ATH12K_DEBUGFS

void ath12k_wmi_crl_path_stats_list_free(struct ath12k *ar, struct list_head *head)
{
	struct wmi_ctrl_path_stats_list *stats, *tmp;

	lockdep_assert_held(&ar->wmi_ctrl_path_stats_lock);
	list_for_each_entry_safe(stats, tmp, head, list) {
		kfree(stats->stats_ptr);
		list_del(&stats->list);
		kfree(stats);
	}
}

int wmi_print_ctrl_path_pdev_tx_stats_tlv(struct ath12k_base *ab, u16 len, const void *ptr, void *data)
{
	struct wmi_ctrl_path_stats_ev_parse_param *stats_buff = (struct wmi_ctrl_path_stats_ev_parse_param *)data;
	struct wmi_ctrl_path_pdev_stats_params *pdev_stats_skb = (struct wmi_ctrl_path_pdev_stats_params *)ptr;
	struct wmi_ctrl_path_pdev_stats_params *pdev_stats = NULL;
	struct wmi_ctrl_path_stats_list *stats = kzalloc(sizeof(struct wmi_ctrl_path_stats_list), GFP_ATOMIC);
	struct ath12k *ar = NULL;

	if (!stats)
		return -ENOMEM;

	pdev_stats = kzalloc(sizeof(*pdev_stats), GFP_ATOMIC);
	if (!pdev_stats) {
		kfree(stats);
		return -ENOMEM;
	}

	memcpy(pdev_stats, pdev_stats_skb, sizeof(struct wmi_ctrl_path_pdev_stats_params));
	stats->stats_ptr = pdev_stats;
	stats->tagid = WMI_TAG_CTRL_PATH_PDEV_STATS;
	list_add_tail(&stats->list, &stats_buff->list);

	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_stats_skb->pdev_id + 1);
	if (!ar) {
		ath12k_warn(ab, "Failed to get ar for wmi ctrl stats\n");
		kfree(pdev_stats);
		list_del(&stats->list);
		kfree(stats);
		return -EINVAL;
	}

	spin_lock_bh(&ar->wmi_ctrl_path_stats_lock);
	ath12k_wmi_crl_path_stats_list_free(ar, &ar->debug.wmi_ctrl_path_stats.pdev_stats);
	spin_unlock_bh(&ar->wmi_ctrl_path_stats_lock);
	ar->debug.wmi_ctrl_path_stats_tagid = WMI_TAG_CTRL_PATH_PDEV_STATS;
	stats_buff->ar = ar;
	return 0;
}

int wmi_print_ctrl_path_cal_stats_tlv(struct ath12k_base *ab, u16 len,
				      const void *ptr, void *data)
{
	struct wmi_ctrl_path_stats_ev_parse_param *stats_buff = (struct wmi_ctrl_path_stats_ev_parse_param *)data;
	struct wmi_ctrl_path_cal_stats *cal_stats_skb = (struct wmi_ctrl_path_cal_stats *)ptr;
	struct wmi_ctrl_path_cal_stats *cal_stats = NULL;
	struct wmi_ctrl_path_stats_list *stats = kzalloc(sizeof(struct wmi_ctrl_path_stats_list), GFP_ATOMIC);
	struct ath12k *ar = NULL;

	if (!stats)
		return -ENOMEM;

	cal_stats = kzalloc(sizeof(*cal_stats), GFP_ATOMIC);
	if (!cal_stats) {
		kfree(stats);
		return -ENOMEM;
	}

	memcpy(cal_stats, cal_stats_skb, sizeof(struct wmi_ctrl_path_cal_stats));
	stats->stats_ptr = cal_stats;
	stats->tagid = WMI_CTRL_PATH_CAL_STATS;
	list_add_tail(&stats->list, &stats_buff->list);

	ar = ath12k_mac_get_ar_by_pdev_id(ab, cal_stats_skb->pdev_id + 1);
	if (!ar) {
		ath12k_warn(ab, "Failed to get ar for wmi ctrl cal stats\n");
		kfree(cal_stats);
		list_del(&stats->list);
		kfree(stats);
		return -EINVAL;
	}

	spin_lock_bh(&ar->wmi_ctrl_path_stats_lock);
	ath12k_wmi_crl_path_stats_list_free(ar, &ar->debug.wmi_ctrl_path_stats.pdev_stats);
	spin_unlock_bh(&ar->wmi_ctrl_path_stats_lock);
	ar->debug.wmi_ctrl_path_stats_tagid = WMI_CTRL_PATH_CAL_STATS;
	stats_buff->ar = ar;
	return 0;
}

int wmi_print_ctrl_path_btcoex_stats_tlv(struct ath12k_base *ab, u16 len,
					 const void *ptr, void *data)
{
	struct wmi_ctrl_path_stats_ev_parse_param *stats_buff =
				(struct wmi_ctrl_path_stats_ev_parse_param *)data;
	struct wmi_ctrl_path_btcoex_stats *btcoex_stats_skb =
				(struct wmi_ctrl_path_btcoex_stats *)ptr;
	struct wmi_ctrl_path_btcoex_stats *btcoex_stats = NULL;
	struct wmi_ctrl_path_stats_list *stats;
	struct ath12k *ar = NULL;

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
	stats->tagid = WMI_CTRL_PATH_BTCOEX_STATS;
	list_add_tail(&stats->list, &stats_buff->list);

	ar = ath12k_mac_get_ar_by_pdev_id(ab, btcoex_stats_skb->pdev_id + 1);
	if (!ar) {
		ath12k_warn(ab, "Failed to get ar for wmi ctrl cal stats\n");
		kfree(btcoex_stats);
		list_del(&stats->list);
		kfree(stats);
		return -EINVAL;
	}

	spin_lock_bh(&ar->wmi_ctrl_path_stats_lock);
	ath12k_wmi_crl_path_stats_list_free(ar, &ar->debug.wmi_ctrl_path_stats.pdev_stats);
	spin_unlock_bh(&ar->wmi_ctrl_path_stats_lock);
	ar->debug.wmi_ctrl_path_stats_tagid = WMI_CTRL_PATH_BTCOEX_STATS;
	stats_buff->ar = ar;
	return 0;
}

/*static void
ath12k_wmi_ctrl_path_pdev_stats_list_free(struct list_head *head)
{
	struct wmi_ctrl_path_pdev_stats *stats_list, *tmp;

	list_for_each_entry_safe(stats_list, tmp, head, list) {
		list_del(&stats_list->list);
		kfree(stats_list);
	}
}

static void
ath12k_wmi_ctrl_path_stats_list_free(struct ath12k_wmi_ctrl_path_stats_list *param)
{
	ath12k_wmi_ctrl_path_pdev_stats_list_free(&param->pdev_stats);
}
*/


int wmi_print_ctrl_path_awgn_stats_tlv(struct ath12k_base *ab, u16 len,
				       const void *ptr, void *data)
{
	struct wmi_ctrl_path_stats_ev_parse_param *stats_buff =
			    (struct wmi_ctrl_path_stats_ev_parse_param *)data;
	struct wmi_ctrl_path_awgn_stats *awgn_stats_skb, *awgn_stats = NULL;
	struct wmi_ctrl_path_stats_list *stats;
	struct ath12k *ar = NULL;
	int i;

	awgn_stats_skb = (struct wmi_ctrl_path_awgn_stats *)ptr;

	for (i = 0; i < ATH12K_GROUP_MAX_RADIO; i++) {
		ar = ath12k_mac_get_ar_by_pdev_id(ab,
						  ab->ag->dp_hw_grp->hw_links[i].pdev_idx);
		if (!ar) {
			ath12k_warn(ab, "Failed to get ar for wmi ctrl awgn stats\n");
			return -EINVAL;
		}

		if (ar->supports_6ghz)
			break;
	}

	stats = kzalloc(sizeof(*stats), GFP_ATOMIC);
	if (!stats)
		return -ENOMEM;

	awgn_stats = kzalloc(sizeof(*awgn_stats), GFP_ATOMIC);

	if (!awgn_stats) {
		kfree(stats);
		return -ENOMEM;
	}

	memcpy(awgn_stats, awgn_stats_skb, sizeof(*awgn_stats));
	stats->stats_ptr = awgn_stats;
	list_add_tail(&stats->list, &stats_buff->list);

	spin_lock_bh(&ar->wmi_ctrl_path_stats_lock);
	ath12k_wmi_crl_path_stats_list_free(ar, &ar->debug.wmi_ctrl_path_stats.pdev_stats);
	spin_unlock_bh(&ar->wmi_ctrl_path_stats_lock);
	ar->debug.wmi_ctrl_path_stats_tagid = WMI_CTRL_PATH_AWGN_STATS;
	stats_buff->ar = ar;

	return 0;
}

int wmi_print_ctrl_path_mem_stats_tlv(struct ath12k_base *ab, u16 len,
				      const void *ptr, void *data)
{
	struct wmi_ctrl_path_stats_ev_parse_param *stats_buff =
		(struct wmi_ctrl_path_stats_ev_parse_param *)data;
	struct wmi_ctrl_path_mem_stats_params *mem_stats_skb =
		(struct wmi_ctrl_path_mem_stats_params *)ptr;
	struct wmi_ctrl_path_mem_stats_params *mem_stats = NULL;
	struct wmi_ctrl_path_stats_list *stats;
	struct ath12k *ar = NULL;
	int i;

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		if (!ar || !ar->ctrl_mem_stats)
			continue;
		stats = kzalloc(sizeof(*stats), GFP_ATOMIC);
		if (!stats)
			return -ENOMEM;

		mem_stats = kzalloc(sizeof(*mem_stats), GFP_ATOMIC);
		if (!mem_stats) {
			kfree(stats);
			return -ENOMEM;
		}

		memcpy(mem_stats, mem_stats_skb, sizeof(*mem_stats));
		stats->stats_ptr = mem_stats;
		stats->tagid = WMI_CTRL_PATH_MEM_STATS;
		list_add_tail(&stats->list, &stats_buff->list);

		spin_lock_bh(&ar->wmi_ctrl_path_stats_lock);
		ath12k_wmi_crl_path_stats_list_free(ar, &ar->debug.wmi_ctrl_path_stats.pdev_stats);
		spin_unlock_bh(&ar->wmi_ctrl_path_stats_lock);
		ar->debug.wmi_ctrl_path_stats_tagid = WMI_CTRL_PATH_MEM_STATS;
		stats_buff->ar = ar;
	}

	return 0;
}

int wmi_print_ctrl_path_afc_stats_tlv(struct ath12k_base *ab, u16 len,
				      const void *ptr, void *data)
{
	struct wmi_ctrl_path_stats_ev_parse_param *stats_buff = data;
	struct wmi_ctrl_path_afc_stats *afc_stats_skb, *afc_stats = NULL;
	struct wmi_ctrl_path_stats_list *stats;
	struct ath12k *ar = NULL;
	int i;
	struct ath12k_base *partner_ab;;
	u8 device_id, pdev_idx;

	afc_stats_skb = (struct wmi_ctrl_path_afc_stats *)ptr;

	for (i = 0; i < ATH12K_GROUP_MAX_RADIO; i++) {
		device_id = ab->ag->dp_hw_grp->hw_links[i].device_id;
		pdev_idx = ab->ag->dp_hw_grp->hw_links[i].pdev_idx;
		partner_ab = ath12k_ag_to_ab(ab->ag, device_id);
		ar = partner_ab->pdevs[pdev_idx].ar;
		if (!ar) {
			ath12k_warn(ab, "Failed to get ar for wmi ctrl afc stats\n");
			return -EINVAL;
		}

		if (ar->supports_6ghz)
			break;
	}

	if (!ar->supports_6ghz) {
		ath12k_warn(ab, "AFC stats is not supported on a non 6 GHz radio\n");
		return -EINVAL;
	}

	stats = kzalloc(sizeof(*stats), GFP_ATOMIC);
	if (!stats)
		return -ENOMEM;

	afc_stats = kzalloc(sizeof(*afc_stats), GFP_ATOMIC);

	if (!afc_stats) {
		kfree(stats);
		return -ENOMEM;
	}

	memcpy(afc_stats, afc_stats_skb, sizeof(*afc_stats));
	stats->stats_ptr = afc_stats;
	stats->tagid = WMI_CTRL_PATH_AFC_STATS;
	list_add_tail(&stats->list, &stats_buff->list);

	spin_lock_bh(&ar->wmi_ctrl_path_stats_lock);
	ath12k_wmi_crl_path_stats_list_free(ar, &ar->debug.wmi_ctrl_path_stats.pdev_stats);
	spin_unlock_bh(&ar->wmi_ctrl_path_stats_lock);
	ar->debug.wmi_ctrl_path_stats_tagid = WMI_CTRL_PATH_AFC_STATS;
	stats_buff->ar = ar;

	return 0;
}

int wmi_print_ctrl_path_pmlo_stats_tlv(struct ath12k_base *ab, u16 len, const void *ptr, void *data)
{
       struct wmi_ctrl_path_stats_ev_parse_param *stats_buff = (struct wmi_ctrl_path_stats_ev_parse_param *)data;
       struct wmi_ctrl_path_pmlo_telemetry_stats *pmlo_stats_skb = (struct wmi_ctrl_path_pmlo_telemetry_stats *)ptr;
       struct wmi_ctrl_path_pmlo_telemetry_stats *pmlo_stats = NULL;
       struct wmi_ctrl_path_stats_list *stats = kzalloc(sizeof(struct wmi_ctrl_path_stats_list), GFP_ATOMIC);
       struct ath12k *ar = NULL;
       u32 value;

       if (!stats)
               return -ENOMEM;

	pmlo_stats = kzalloc(sizeof(*pmlo_stats), GFP_ATOMIC);
       if (!pmlo_stats) {
               kfree(stats);
               return -ENOMEM;
       }

       memcpy(pmlo_stats, pmlo_stats_skb, sizeof(struct wmi_ctrl_path_pmlo_telemetry_stats));
       stats->stats_ptr = pmlo_stats;
	stats->tagid = WMI_CTRL_PATH_PMLO_STATS;
       list_add_tail(&stats->list, &stats_buff->list);

       ar = ath12k_mac_get_ar_by_pdev_id(ab, pmlo_stats_skb->pdev_id);
       if (!ar) {
               ath12k_warn(ab, "Failed to get ar for wmi ctrl stats\n");
		kfree(pmlo_stats);
               list_del(&stats->list);
               kfree(stats);
               return -EINVAL;
       }

       spin_lock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
       value = le32_to_cpu(pmlo_stats->estimated_air_time_per_ac);
       ar->stats.telemetry_stats.estimated_air_time_ac_be =
               u32_get_bits(value, GENMASK(7, 0));
       ar->stats.telemetry_stats.estimated_air_time_ac_bk =
               u32_get_bits(value, GENMASK(15, 8));
       ar->stats.telemetry_stats.estimated_air_time_ac_vi =
               u32_get_bits(value, GENMASK(23, 16));
       ar->stats.telemetry_stats.estimated_air_time_ac_vo =
               u32_get_bits(value, GENMASK(31, 24));

	ath12k_wmi_crl_path_stats_list_free(ar, &ar->debug.period_wmi_list);
       spin_unlock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	ar->debug.wmi_ctrl_path_stats_tagid = WMI_CTRL_PATH_PMLO_STATS;
       stats_buff->ar = ar;
       return 0;
}

static int ath12k_wmi_ctrl_stats_subtlv_parser(struct ath12k_base *ab,
					       u16 tag, u16 len,
					       const void *ptr, void *data)
{
	int ret;

	switch (tag) {
	case WMI_TAG_CTRL_PATH_STATS_EV_FIXED_PARAM:
		break;
	case WMI_TAG_CTRL_PATH_PDEV_STATS:
		ret = wmi_print_ctrl_path_pdev_tx_stats_tlv(ab, len, ptr, data);
		break;
	case WMI_CTRL_PATH_CAL_STATS:
		ret = wmi_print_ctrl_path_cal_stats_tlv(ab, len, ptr, data);
		break;
	case WMI_CTRL_PATH_BTCOEX_STATS:
		ret = wmi_print_ctrl_path_btcoex_stats_tlv(ab, len, ptr, data);
		break;
	case WMI_CTRL_PATH_AWGN_STATS:
		ret = wmi_print_ctrl_path_awgn_stats_tlv(ab, len, ptr, data);
		break;
	case WMI_CTRL_PATH_MEM_STATS:
		ret = wmi_print_ctrl_path_mem_stats_tlv(ab, len, ptr, data);
		break;
	case WMI_CTRL_PATH_AFC_STATS:
		ret = wmi_print_ctrl_path_afc_stats_tlv(ab, len, ptr, data);
		break;
	case WMI_CTRL_PATH_PMLO_STATS:
	        ret = wmi_print_ctrl_path_pmlo_stats_tlv(ab, len, ptr, data);
		break;
		/* Add case for newly wmi ctrl path added stats here */
	default:
		ath12k_warn(ab,
			    "Received invalid tag for wmi ctrl path stats in subtlvs, tag : 0x%x\n",
			    tag);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ath12k_wmi_ctrl_stats_event_parser(struct ath12k_base *ab,
					      u16 tag, u16 len,
					      const void *ptr, void *data)
{
	int ret;

	ath12k_dbg_level(ab, ATH12K_DBG_WMI, ATH12K_DBG_L3,
			 "wmi ctrl path stats tag 0x%x of len %d rcvd\n",
			 tag, len);

	switch (tag) {
	case WMI_TAG_CTRL_PATH_STATS_EV_FIXED_PARAM:
		/* Fixed param is already processed*/
		ret = 0;
		break;
	case WMI_TAG_ARRAY_STRUCT:
		/* len 0 is expected for array of struct when there
		 * is no content of that type to pack inside that tlv
		 */
		if (len == 0)
		return 0;

		ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					  ath12k_wmi_ctrl_stats_subtlv_parser,
					  data);
		break;
	default:
		ath12k_warn(ab, "Received invalid tag for wmi ctrl path stats\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void ath12k_wmi_ctrl_path_stats_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_ctrl_path_stats_event *fixed_param;
	struct wmi_ctrl_path_stats_ev_parse_param param = {0};
	struct ath12k_wmi_ctrl_path_stats_list *stats;
	const struct wmi_tlv *tlv;
	struct list_head *src, *dst;
	struct ath12k *ar;
	void *ptr = skb->data;
	u16 tlv_tag, tag_id;
	u32 more;
	int ret;

	if (!skb->data) {
		ath12k_warn(ab, "No data present in wmi ctrl stats event\n");
		return;
	}

	if (skb->len < (sizeof(*fixed_param) + TLV_HDR_SIZE)) {
		ath12k_warn(ab, "wmi ctrl stats event size invalid\n");
		return;
	}

	param.ar = NULL;

	tlv = ptr;
	tlv_tag = le32_get_bits(tlv->header, WMI_TLV_TAG);
	ptr += sizeof(*tlv);

	if (tlv_tag != WMI_TAG_CTRL_PATH_STATS_EV_FIXED_PARAM) {
		ath12k_warn(ab, "wmi ctrl stats without fixed param tlv at start\n");
		return;
	}

	INIT_LIST_HEAD(&param.list);

	fixed_param = ptr;
	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_ctrl_stats_event_parser,
				  &param);
	if (ret) {
		ath12k_warn(ab, "failed to parse wmi_ctrl_path_stats tlv: %d\n", ret);
		if (!param.ar)
			return;
		goto free;
	}

	ar = param.ar;
	if (!ar)
		return;

	tag_id = ar->debug.wmi_ctrl_path_stats_tagid;
	stats = &ar->debug.wmi_ctrl_path_stats;
	more = __le32_to_cpu(fixed_param->more);

	src = &param.list;
	dst = &stats->pdev_stats;

	switch (tag_id) {
	case WMI_TAG_CTRL_PATH_PDEV_STATS:
		break;
	case WMI_CTRL_PATH_PMLO_STATS:
		ath12k_dp_mon_pdev_update_telemetry_stats(ar->ab, ar->pdev_idx);
		break;
	case WMI_TAG_CTRL_PATH_STATS_EV_FIXED_PARAM:
	case WMI_CTRL_PATH_CAL_STATS:
	case WMI_CTRL_PATH_BTCOEX_STATS:
	case WMI_CTRL_PATH_AWGN_STATS:
	case WMI_CTRL_PATH_MEM_STATS:
	case WMI_CTRL_PATH_AFC_STATS:
		break;
	/* Add case for newly wmi ctrl path added stats here */
	default:
		ath12k_warn(ab, "Received tag for wmi ctrl path stats %d\n", tag_id);
		goto free;
	}

	spin_lock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	if (!more) {
		if (tag_id == WMI_CTRL_PATH_PMLO_STATS) {
			list_splice_tail_init(&param.list, &ar->debug.period_wmi_list);
		} else {
			if (!ar->debug.wmi_ctrl_path_stats_more_enabled)
				ath12k_wmi_crl_path_stats_list_free(ar,
								    &stats->pdev_stats);
			else
				ar->debug.wmi_ctrl_path_stats_more_enabled = false;
			list_splice_tail_init(src, dst);
		}
		complete(&ar->debug.wmi_ctrl_path_stats_rcvd);
	} else {
		if (tag_id == WMI_CTRL_PATH_PMLO_STATS) {
			list_splice_tail_init(&param.list, &ar->debug.period_wmi_list);
		} else {
			if (!ar->debug.wmi_ctrl_path_stats_more_enabled) {
				ath12k_wmi_crl_path_stats_list_free(ar,
								    &stats->pdev_stats);
				ar->debug.wmi_ctrl_path_stats_more_enabled = true;
			}
			list_splice_tail_init(src, dst);
		}
	}
	spin_unlock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	return;
free:
	spin_lock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	ath12k_wmi_crl_path_stats_list_free(ar, &param.list);
	spin_unlock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
}
#else
static void ath12k_wmi_ctrl_path_stats_event(struct ath12k_base *ab,
struct sk_buff *skb)
{
	ath12k_update_stats_event(ab, skb);
}
#endif /* CPTCFG_ATH12K_DEBUGFS */

#ifdef CPTCFG_ATH12K_DEBUGFS
static int ath12k_wmi_tpc_stats_copy_buffer(struct ath12k_base *ab,
					    const void *ptr, u16 tag, u16 len,
					    struct wmi_tpc_stats_arg *tpc_stats)
{
	u32 len1, len2, len3, len4;
	s16 *dst_ptr;
	s8 *dst_ptr_ctl;

	len1 = le32_to_cpu(tpc_stats->max_reg_allowed_power.tpc_reg_pwr.reg_array_len);
	len2 = le32_to_cpu(tpc_stats->rates_array1.tpc_rates_array.rate_array_len);
	len3 = le32_to_cpu(tpc_stats->rates_array2.tpc_rates_array.rate_array_len);
	len4 = le32_to_cpu(tpc_stats->ctl_array.tpc_ctl_pwr.ctl_array_len);

	switch (tpc_stats->event_count) {
	case ATH12K_TPC_STATS_CONFIG_REG_PWR_EVENT:
		if (len1 > len)
			return -ENOBUFS;

		if (tpc_stats->tlvs_rcvd & WMI_TPC_REG_PWR_ALLOWED) {
			dst_ptr = tpc_stats->max_reg_allowed_power.reg_pwr_array;
			memcpy(dst_ptr, ptr, len1);
		}
		break;
	case ATH12K_TPC_STATS_RATES_EVENT1:
		if (len2 > len)
			return -ENOBUFS;

		if (tpc_stats->tlvs_rcvd & WMI_TPC_RATES_ARRAY1) {
			dst_ptr = tpc_stats->rates_array1.rate_array;
			memcpy(dst_ptr, ptr, len2);
		}
		break;
	case ATH12K_TPC_STATS_RATES_EVENT2:
		if (len3 > len)
			return -ENOBUFS;

		if (tpc_stats->tlvs_rcvd & WMI_TPC_RATES_ARRAY2) {
			dst_ptr = tpc_stats->rates_array2.rate_array;
			memcpy(dst_ptr, ptr, len3);
		}
		break;
	case ATH12K_TPC_STATS_CTL_TABLE_EVENT:
		if (len4 > len)
			return -ENOBUFS;

		if (tpc_stats->tlvs_rcvd & WMI_TPC_CTL_PWR_ARRAY) {
			dst_ptr_ctl = tpc_stats->ctl_array.ctl_pwr_table;
			memcpy(dst_ptr_ctl, ptr, len4);
		}
		break;
	}
	return 0;
}

static int ath12k_tpc_get_reg_pwr(struct ath12k_base *ab,
				  struct wmi_tpc_stats_arg *tpc_stats,
				  struct wmi_max_reg_power_fixed_params *ev)
{
	struct wmi_max_reg_power_allowed_arg *reg_pwr;
	u32 total_size;

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "Received reg power array type %d length %d for tpc stats\n",
		   ev->reg_power_type, ev->reg_array_len);

	switch (le32_to_cpu(ev->reg_power_type)) {
	case TPC_STATS_REG_PWR_ALLOWED_TYPE:
		reg_pwr = &tpc_stats->max_reg_allowed_power;
		break;
	default:
		return -EINVAL;
	}

	/* Each entry is 2 byte hence multiplying the indices with 2 */
	total_size = le32_to_cpu(ev->d1) * le32_to_cpu(ev->d2) *
		     le32_to_cpu(ev->d3) * le32_to_cpu(ev->d4) * 2;
	if (le32_to_cpu(ev->reg_array_len) != total_size) {
		ath12k_warn(ab,
			    "Total size and reg_array_len doesn't match for tpc stats\n");
		return -EINVAL;
	}

	memcpy(&reg_pwr->tpc_reg_pwr, ev, sizeof(struct wmi_max_reg_power_fixed_params));

	reg_pwr->reg_pwr_array = kzalloc(le32_to_cpu(reg_pwr->tpc_reg_pwr.reg_array_len),
					 GFP_ATOMIC);
	if (!reg_pwr->reg_pwr_array)
		return -ENOMEM;

	tpc_stats->tlvs_rcvd |= WMI_TPC_REG_PWR_ALLOWED;

	return 0;
}

static int ath12k_tpc_get_rate_array(struct ath12k_base *ab,
				     struct wmi_tpc_stats_arg *tpc_stats,
				     struct wmi_tpc_rates_array_fixed_params *ev)
{
	struct wmi_tpc_rates_array_arg *rates_array;
	u32 flag = 0, rate_array_len;

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "Received rates array type %d length %d for tpc stats\n",
		   ev->rate_array_type, ev->rate_array_len);

	switch (le32_to_cpu(ev->rate_array_type)) {
	case ATH12K_TPC_STATS_RATES_ARRAY1:
		rates_array = &tpc_stats->rates_array1;
		flag = WMI_TPC_RATES_ARRAY1;
		break;
	case ATH12K_TPC_STATS_RATES_ARRAY2:
		rates_array = &tpc_stats->rates_array2;
		flag = WMI_TPC_RATES_ARRAY2;
		break;
	default:
		ath12k_warn(ab,
			    "Received invalid type of rates array for tpc stats\n");
		return -EINVAL;
	}
	memcpy(&rates_array->tpc_rates_array, ev,
	       sizeof(struct wmi_tpc_rates_array_fixed_params));
	rate_array_len = le32_to_cpu(rates_array->tpc_rates_array.rate_array_len);
	rates_array->rate_array = kzalloc(rate_array_len, GFP_ATOMIC);
	if (!rates_array->rate_array)
		return -ENOMEM;

	tpc_stats->tlvs_rcvd |= flag;
	return 0;
}

static int ath12k_tpc_get_ctl_pwr_tbl(struct ath12k_base *ab,
				      struct wmi_tpc_stats_arg *tpc_stats,
				      struct wmi_tpc_ctl_pwr_fixed_params *ev)
{
	struct wmi_tpc_ctl_pwr_table_arg *ctl_array;
	u32 total_size, ctl_array_len, flag = 0;

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "Received ctl array type %d length %d for tpc stats\n",
		   ev->ctl_array_type, ev->ctl_array_len);

	switch (le32_to_cpu(ev->ctl_array_type)) {
	case ATH12K_TPC_STATS_CTL_ARRAY:
		ctl_array = &tpc_stats->ctl_array;
		flag = WMI_TPC_CTL_PWR_ARRAY;
		break;
	default:
		ath12k_warn(ab,
			    "Received invalid type of ctl pwr table for tpc stats\n");
		return -EINVAL;
	}

	total_size = le32_to_cpu(ev->d1) * le32_to_cpu(ev->d2) *
		     le32_to_cpu(ev->d3) * le32_to_cpu(ev->d4);
	if (le32_to_cpu(ev->ctl_array_len) != total_size) {
		ath12k_warn(ab,
			    "Total size and ctl_array_len doesn't match for tpc stats\n");
		return -EINVAL;
	}

	memcpy(&ctl_array->tpc_ctl_pwr, ev, sizeof(struct wmi_tpc_ctl_pwr_fixed_params));
	ctl_array_len = le32_to_cpu(ctl_array->tpc_ctl_pwr.ctl_array_len);
	ctl_array->ctl_pwr_table = kzalloc(ctl_array_len, GFP_ATOMIC);
	if (!ctl_array->ctl_pwr_table)
		return -ENOMEM;

	tpc_stats->tlvs_rcvd |= flag;
	return 0;
}

static int ath12k_wmi_tpc_stats_subtlv_parser(struct ath12k_base *ab,
					      u16 tag, u16 len,
					      const void *ptr, void *data)
{
	struct wmi_tpc_rates_array_fixed_params *tpc_rates_array;
	struct wmi_max_reg_power_fixed_params *tpc_reg_pwr;
	struct wmi_tpc_ctl_pwr_fixed_params *tpc_ctl_pwr;
	struct wmi_tpc_stats_arg *tpc_stats = data;
	struct wmi_tpc_config_params *tpc_config;
	int ret = 0;

	if (!tpc_stats) {
		ath12k_warn(ab, "tpc stats memory unavailable\n");
		return -EINVAL;
	}

	switch (tag) {
	case WMI_TAG_TPC_STATS_CONFIG_EVENT:
		tpc_config = (struct wmi_tpc_config_params *)ptr;
		memcpy(&tpc_stats->tpc_config, tpc_config,
		       sizeof(struct wmi_tpc_config_params));
		break;
	case WMI_TAG_TPC_STATS_REG_PWR_ALLOWED:
		tpc_reg_pwr = (struct wmi_max_reg_power_fixed_params *)ptr;
		ret = ath12k_tpc_get_reg_pwr(ab, tpc_stats, tpc_reg_pwr);
		break;
	case WMI_TAG_TPC_STATS_RATES_ARRAY:
		tpc_rates_array = (struct wmi_tpc_rates_array_fixed_params *)ptr;
		ret = ath12k_tpc_get_rate_array(ab, tpc_stats, tpc_rates_array);
		break;
	case WMI_TAG_TPC_STATS_CTL_PWR_TABLE_EVENT:
		tpc_ctl_pwr = (struct wmi_tpc_ctl_pwr_fixed_params *)ptr;
		ret = ath12k_tpc_get_ctl_pwr_tbl(ab, tpc_stats, tpc_ctl_pwr);
		break;
	default:
		ath12k_warn(ab,
			    "Received invalid tag for tpc stats in subtlvs\n");
		return -EINVAL;
	}
	return ret;
}

static int ath12k_wmi_tpc_stats_event_parser(struct ath12k_base *ab,
					     u16 tag, u16 len,
					     const void *ptr, void *data)
{
	struct wmi_tpc_stats_arg *tpc_stats = (struct wmi_tpc_stats_arg *)data;
	int ret;

	switch (tag) {
	case WMI_TAG_HALPHY_CTRL_PATH_EVENT_FIXED_PARAM:
		ret = 0;
		/* Fixed param is already processed*/
		break;
	case WMI_TAG_ARRAY_STRUCT:
		/* len 0 is expected for array of struct when there
		 * is no content of that type to pack inside that tlv
		 */
		if (len == 0)
			return 0;
		ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					  ath12k_wmi_tpc_stats_subtlv_parser,
					  tpc_stats);
		break;
	case WMI_TAG_ARRAY_INT16:
		if (len == 0)
			return 0;
		ret = ath12k_wmi_tpc_stats_copy_buffer(ab, ptr,
						       WMI_TAG_ARRAY_INT16,
						       len, tpc_stats);
		break;
	case WMI_TAG_ARRAY_BYTE:
		if (len == 0)
			return 0;
		ret = ath12k_wmi_tpc_stats_copy_buffer(ab, ptr,
						       WMI_TAG_ARRAY_BYTE,
						       len, tpc_stats);
		break;
	default:
		ath12k_warn(ab, "Received invalid tag for tpc stats\n");
		ret = -EINVAL;
		break;
	}
	return ret;
}

void ath12k_wmi_free_tpc_stats_mem(struct ath12k *ar)
{
	struct wmi_tpc_stats_arg *tpc_stats = ar->debug.tpc_stats;

	lockdep_assert_held(&ar->data_lock);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "tpc stats mem free\n");
	if (tpc_stats) {
		kfree(tpc_stats->max_reg_allowed_power.reg_pwr_array);
		kfree(tpc_stats->rates_array1.rate_array);
		kfree(tpc_stats->rates_array2.rate_array);
		kfree(tpc_stats->ctl_array.ctl_pwr_table);
		kfree(tpc_stats);
		ar->debug.tpc_stats = NULL;
	}
}

static void ath12k_wmi_process_tpc_stats(struct ath12k_base *ab,
					 struct sk_buff *skb)
{
	struct ath12k_wmi_pdev_tpc_stats_event_fixed_params *fixed_param;
	struct wmi_tpc_stats_arg *tpc_stats;
	const struct wmi_tlv *tlv;
	void *ptr = skb->data;
	struct ath12k *ar;
	u16 tlv_tag;
	u32 event_count;
	int ret;

	if (!skb->data) {
		ath12k_warn(ab, "No data present in tpc stats event\n");
		return;
	}

	if (skb->len < (sizeof(*fixed_param) + TLV_HDR_SIZE)) {
		ath12k_warn(ab, "TPC stats event size invalid\n");
		return;
	}

	tlv = (struct wmi_tlv *)ptr;
	tlv_tag = le32_get_bits(tlv->header, WMI_TLV_TAG);
	ptr += sizeof(*tlv);

	if (tlv_tag != WMI_TAG_HALPHY_CTRL_PATH_EVENT_FIXED_PARAM) {
		ath12k_warn(ab, "TPC stats without fixed param tlv at start\n");
		return;
	}

	fixed_param = (struct ath12k_wmi_pdev_tpc_stats_event_fixed_params *)ptr;
	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, le32_to_cpu(fixed_param->pdev_id) + 1);
	if (!ar) {
		ath12k_warn(ab, "Failed to get ar for tpc stats\n");
		rcu_read_unlock();
		return;
	}
	spin_lock_bh(&ar->data_lock);
	if (!ar->debug.tpc_request) {
		/* Event is received either without request or the
		 * timeout, if memory is already allocated free it
		 */
		if (ar->debug.tpc_stats) {
			ath12k_warn(ab, "Freeing memory for tpc_stats\n");
			ath12k_wmi_free_tpc_stats_mem(ar);
		}
		goto unlock;
	}

	event_count = le32_to_cpu(fixed_param->event_count);
	if (event_count == 0) {
		if (ar->debug.tpc_stats) {
			ath12k_warn(ab,
				    "Invalid tpc memory present\n");
			goto unlock;
		}
		ar->debug.tpc_stats =
			kzalloc(sizeof(struct wmi_tpc_stats_arg),
				GFP_ATOMIC);
		if (!ar->debug.tpc_stats) {
			ath12k_warn(ab,
				    "Failed to allocate memory for tpc stats\n");
			goto unlock;
		}
	}

	tpc_stats = ar->debug.tpc_stats;
	if (!tpc_stats) {
		ath12k_warn(ab, "tpc stats memory unavailable\n");
		goto unlock;
	}

	if (!(event_count == 0)) {
		if (event_count != tpc_stats->event_count + 1) {
			ath12k_warn(ab,
				    "Invalid tpc event received\n");
			goto unlock;
		}
	}
	tpc_stats->pdev_id = le32_to_cpu(fixed_param->pdev_id);
	tpc_stats->end_of_event = le32_to_cpu(fixed_param->end_of_event);
	tpc_stats->event_count = le32_to_cpu(fixed_param->event_count);
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "tpc stats event_count %d\n",
		   tpc_stats->event_count);
	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tpc_stats_event_parser,
				  tpc_stats);
	if (ret) {
		ath12k_wmi_free_tpc_stats_mem(ar);
		ath12k_warn(ab, "failed to parse tpc_stats tlv: %d\n", ret);
		goto unlock;
	}

	if (tpc_stats->end_of_event)
		complete(&ar->debug.tpc_complete);

unlock:
	spin_unlock_bh(&ar->data_lock);
	rcu_read_unlock();
}
#else
static void ath12k_wmi_process_tpc_stats(struct ath12k_base *ab,
					 struct sk_buff *skb)
{
}
#endif

static long int
ath12k_pull_peer_create_conf_ev(struct ath12k_base *ab,
				struct sk_buff *skb,
				struct ath12k_wmi_peer_create_conf_arg *arg)
{
	const void **tb;
	const struct ath12k_wmi_peer_create_conf_ev *ev;
	long int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %ld\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PEER_CREATE_RESP_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch peer create response ev");
		kfree(tb);
		return -EPROTO;
	}

	arg->vdev_id = __le32_to_cpu(ev->vdev_id);
	ether_addr_copy(arg->mac_addr, ev->peer_macaddr.addr);
	arg->status = __le32_to_cpu(ev->status);

	kfree(tb);
	return 0;
}

static void ath12k_wmi_peer_create_conf_event(struct ath12k_base *ab,
					      struct sk_buff *skb)
{
	struct ath12k_wmi_peer_create_conf_arg arg = {};

	if (ath12k_pull_peer_create_conf_ev(ab, skb, &arg)) {
		ath12k_warn(ab, "failed to extract peer create conf event");
		return;
	}

	if (arg.status != ATH12K_WMI_PEER_CREATE_SUCCESS) {
		ath12k_warn(ab, "Peer %pM creation failed due to %d",
			    arg.mac_addr, arg.status);
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI | ATH12K_DBG_MLME,
		   "Peer create conf event for %pM status %d",
		   arg.mac_addr, arg.status);
}

static int ath12k_wmi_pdev_sscan_fft_bin_index_parse(struct ath12k_base *soc,
						     u16 tag, u16 len,
						     const void *ptr, void *data)
{
	if (tag != WMI_TAG_PDEV_SSCAN_FFT_BIN_INDEX)
		return -EPROTO;
	return 0;
}

static int ath12k_wmi_pdev_sscan_per_detector_info_parse(struct ath12k_base *soc,
							 u16 tag, u16 len,
							 const void *ptr, void *data)
{
	if (tag != WMI_TAG_PDEV_SSCAN_PER_DETECTOR_INFO)
		return -EPROTO;

	return 0;
}

static int ath12k_wmi_tlv_sscan_fw_parse(struct ath12k_base *ab,
					 u16 tag, u16 len,
					 const void *ptr, void *data)
{
	struct wmi_pdev_sscan_fw_param_parse *parse = data;
	int ret;

	switch (tag) {

	case WMI_TAG_PDEV_SSCAN_FW_CMD_FIXED_PARAM:
		memcpy(&parse->fixed, ptr,
		       sizeof(struct ath12k_wmi_pdev_sscan_fw_cmd_fixed_param));
		parse->fixed.pdev_id = DP_HW2SW_MACID(parse->fixed.pdev_id);
		break;
	case WMI_TAG_ARRAY_STRUCT:
	       if (!parse->bin_entry_done) {
		       parse->bin = (struct ath12k_wmi_pdev_sscan_fft_bin_index *)ptr;

		       ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						 ath12k_wmi_pdev_sscan_fft_bin_index_parse,
						 parse);

		       if (ret) {
			       ath12k_warn(ab, "failed to parse fft bin index %d\n",
					   ret);
			       return ret;
		       }

		       parse->bin_entry_done = true;
	       } else if (!parse->det_info_entry_done) {
		       parse->det_info = (struct ath12k_wmi_pdev_sscan_per_detector_info *)ptr;

		       ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						 ath12k_wmi_pdev_sscan_per_detector_info_parse,
						 parse);

		       if (ret) {
			       ath12k_warn(ab, "failed to parse detector info %d\n",
					   ret);
			       return ret;
		       }
		       parse->det_info_entry_done = true;
	       }
	       break;
	case WMI_TAG_PDEV_SSCAN_CHAN_INFO:
	       memcpy(&parse->ch_info, ptr,
		      sizeof(struct ath12k_wmi_pdev_sscan_chan_info));
	       parse->bin_entry_done = true;
	       break;
	default:
	       break;
	}
	return 0;

}

static void
ath12k_wmi_pdev_sscan_fw_param_event(struct ath12k_base *ab,
				     struct sk_buff *skb)
{
	struct ath12k *ar;
	struct wmi_pdev_sscan_fw_param_parse parse = { };
	struct wmi_pdev_sscan_fw_param_event param;
	int ret;
	u8 pdev_idx;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_sscan_fw_parse,
				  &parse);

	if (ret) {
		ath12k_warn(ab, "failed to parse padev sscan fw tlv %d\n", ret);
		return;
	}

	param.fixed             = parse.fixed;
	param.bin		= parse.bin;
	param.ch_info		= parse.ch_info;
	param.det_info		= parse.det_info;

	pdev_idx = param.fixed.pdev_id;
	ar = ab->pdevs[pdev_idx].ar;

#ifdef CPTCFG_ATH12K_SPECTRAL
	ar->spectral.ch_width = param.ch_info.operating_bw;
#endif

}

static int
ath12k_wmi_spectral_scan_bw_cap_parse(struct ath12k_base *soc,
				     u16 tag, u16 len,
				     const void *ptr, void *data)
{
	struct wmi_spectral_capabilities_parse *parse = data;
	if (tag != WMI_TAG_SPECTRAL_SCAN_BW_CAPABILITIES)
		return -EPROTO;
	parse->num_bw_caps_entry++;
	return 0;
}

static int
ath12k_wmi_spectral_fft_size_cap_parse(struct ath12k_base *soc,
				       u16 tag, u16 len,
				       const void *ptr, void *data)
{
	struct wmi_spectral_capabilities_parse *parse = data;
	if (tag != WMI_TAG_SPECTRAL_FFT_SIZE_CAPABILITIES)
		return -EPROTO;

	parse->num_fft_size_caps_entry++;
	return 0;
}

static int
ath12k_wmi_tlv_spectral_cap_parse(struct ath12k_base *ab,
				  u16 tag, u16 len,
				  const void *ptr, void *data)
{
	struct wmi_spectral_capabilities_parse *parse = data;
	int ret;

	if (tag == WMI_TAG_ARRAY_STRUCT) {
		if (!parse->sscan_bw_caps_entry_done) {
			parse->num_bw_caps_entry = 0;
			parse->sscan_bw_caps = (struct ath12k_wmi_spectral_scan_bw_capabilities *)ptr;
			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					ath12k_wmi_spectral_scan_bw_cap_parse,
					parse);
			if (ret) {
				ath12k_warn(ab, "failed to parse scan bw cap %d\n",
					    ret);
				return ret;
			}
			parse->sscan_bw_caps_entry_done = true;
		} else if (!parse->fft_size_caps_entry_done) {
			parse->num_fft_size_caps_entry = 0;
			parse->fft_size_caps = (struct ath12k_wmi_spectral_fft_size_capabilities *)ptr;
			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					ath12k_wmi_spectral_fft_size_cap_parse,
					parse);
			if (ret) {
				ath12k_warn(ab, "failed to parse fft size cap %d\n",
					    ret);
				return ret;
			}
			parse->fft_size_caps_entry_done = true;
		}
	}
	return 0;
}

static void
ath12k_wmi_spectral_capabilities_event(struct ath12k_base *ab,
				       struct sk_buff *skb)
{
	struct wmi_spectral_capabilities_parse parse = { };
	struct wmi_spectral_capabilities_event param;
	struct ath12k *ar = NULL;
	int ret, size;
	u8 pdev_id, i;
	struct ath12k_pdev *pdev;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_spectral_cap_parse,
				  &parse);
	if (ret) {
		ath12k_warn(ab, "failed to parse spectral capabilities tlv %d\n", ret);
		return;
	}

	param.sscan_bw_caps = parse.sscan_bw_caps;
	param.fft_size_caps = parse.fft_size_caps;
	param.num_bw_caps_entry = parse.num_bw_caps_entry;
	param.num_fft_size_caps_entry = parse.num_fft_size_caps_entry;

	pdev_id = param.sscan_bw_caps->pdev_id;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		if (pdev && pdev->pdev_id == pdev_id) {
			ar = pdev->ar;
			break;
		}
	}

	if (!ar) {
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "ar is NULL for pdev_id %d use default spectral fft size 7",
			   pdev_id);
		return;
	}
	size = sizeof(struct ath12k_wmi_spectral_fft_size_capabilities)*
					param.num_fft_size_caps_entry;

#ifdef CPTCFG_ATH12K_SPECTRAL
	ar->spectral.spectral_cap.fft_size_caps = kzalloc(size, GFP_ATOMIC);
	if (!ar->spectral.spectral_cap.fft_size_caps) {
		ath12k_warn(ab, "Failed to allocate memory");
		return;
	}
	memcpy(ar->spectral.spectral_cap.fft_size_caps,
		param.fft_size_caps, size);
	ar->spectral.spectral_cap.num_bw_caps_entry = param.num_bw_caps_entry;
	ar->spectral.spectral_cap.num_fft_size_caps_entry = param.num_fft_size_caps_entry;
#endif

}

static const char *ath12k_wmi_twt_add_dialog_event_status(u32 status)
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
	case WMI_ADD_TWT_STATUS_AP_PARAMS_NOT_IN_RANGE:
		return "peer AP wake interval, duration not in range";
	case WMI_ADD_TWT_STATUS_AP_IE_VALIDATION_FAILED:
		return "peer AP IE Validation Failed";
	case WMI_ADD_TWT_STATUS_ROAM_IN_PROGRESS:
		return "Roaming in progress";
	case WMI_ADD_TWT_STATUS_CHAN_SW_IN_PROGRESS:
		return "Channel switch in progress";
	case WMI_ADD_TWT_STATUS_SCAN_IN_PROGRESS:
		return "Scan in progress";
	case WMI_ADD_TWT_STATUS_DIALOG_ID_BUSY:
		return "FW is in the process of handling this dialog";
	case WMI_ADD_TWT_STATUS_BTWT_NOT_ENBABLED:
		return "Broadcast TWT is not enabled";
	case WMI_ADD_TWT_STATUS_RTWT_NOT_ENBABLED:
		return "Restricted TWT is not enabled";
	case WMI_ADD_TWT_STATUS_LINK_SWITCH_IN_PROGRESS:
		return "Link switch is ongoing";
	case WMI_ADD_TWT_STATUS_UNSUPPORTED_MODE_MLMR:
		return "Unsupported in MLMR mode";
	case WMI_ADD_TWT_STATUS_UNKNOWN_ERROR:
		fallthrough;
	default:
		return "unknown error";
	}
}

static void ath12k_wmi_twt_add_dialog_event(struct ath12k_base *ab,
					    struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_twt_add_dialog_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab,
			    "failed to parse wmi twt add dialog status event tlv: %d\n",
			    ret);
		return;
	}

	ev = tb[WMI_TAG_TWT_ADD_DIALOG_COMPLETE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch twt add dialog wmi event\n");
		goto exit;
	}

	if (ev->status)
		ath12k_warn(ab,
			    "wmi add twt dialog event vdev %d dialog id %d status %s\n",
			    ev->vdev_id, ev->dialog_id,
			    ath12k_wmi_twt_add_dialog_event_status(ev->status));

exit:
	kfree(tb);
}

static void ath12k_wmi_suspend_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k *ar = NULL;
	const struct wmi_suspend_resp_event *ev;
	struct ath12k_pdev *pdev;
	const void **tb;
	u32 pdev_id, i;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ath12k_warn(ab, "failed to parse tlv: %ld\n", PTR_ERR(tb));
		return;
	}

	ev = tb[WMI_PDEV_SUSPEND_EVENT_FIXED_PARAM];

	if (!ev) {
		ath12k_warn(ab, "failed to fetch pdev suspend resp ev");
		kfree(tb);
		return;
	}

	pdev_id = le32_to_cpu(ev->pdev_id);
	kfree(tb);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "WMI suspend event received for pdev_id %d\n", pdev_id);

	if (ev->pdev_id > ab->num_radios)
		return;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];

		if (pdev && pdev->pdev_id == ev->pdev_id) {
			ar = pdev->ar;
			break;
		}
	}

	if (!ar) {
		ath12k_warn(ab, "invalid pdev_id received for WMI pdev suspend event\n");
		return;
	}

	ar->pdev_suspend = true;
	complete(&ar->suspend);
}

static void ath12k_wmi_pdev_resume_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	const struct wmi_pdev_resume_resp_event *ev;
	struct ath12k *ar = NULL;
	struct ath12k_pdev *pdev;
	const void **tb;
	u32 pdev_id, i;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ath12k_warn(ab, "failed to parse tlv: %ld\n", PTR_ERR(tb));
		return;
	}

	ev = tb[WMI_TAG_PDEV_RESUME_EVENT];

	if (!ev) {
		ath12k_warn(ab, "failed to fetch pdev resume resp ev");
		kfree(tb);
		return;
	}

	pdev_id = le32_to_cpu(ev->pdev_id);
	kfree(tb);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "WMI resume event received for pdev_id %d\n", pdev_id);

	if (ev->pdev_id > ab->num_radios)
		return;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];

		if (pdev && pdev->pdev_id == ev->pdev_id) {
			ar = pdev->ar;
			break;
		}
	}

	if (!ar) {
		ath12k_warn(ab, "invalid pdev_id received for WMI pdev resume event\n");
		return;
	}

	ar->pdev_suspend = false;
	complete(&ar->pdev_resume);
}

static void
ath12k_wmi_obss_color_collision_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	const struct wmi_obss_color_collision_event *ev;
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;
	const void **tb;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_OBSS_COLOR_COLLISION_EVT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch obss color collision ev");
		goto exit;
	}

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif_by_vdev_id(ab, ev->vdev_id);
	if (!arvif) {
		ath12k_warn(ab, "failed to find arvif with vedv id %d in obss_color_collision_event\n",
				ev->vdev_id);
		goto unlock;
	}

	if (ath12k_mac_is_bridge_vdev(arvif))
		goto unlock;

	switch (ev->evt_type) {
	case WMI_BSS_COLOR_COLLISION_DETECTION:
		ar = arvif->ar;
		arvif->obss_color_bitmap = ev->obss_color_bitmap;
		rcu_read_unlock();
		wiphy_work_queue(ath12k_ar_to_hw(ar)->wiphy, &arvif->update_obss_color_notify_work);

		ath12k_dbg(ab, ATH12K_DBG_WMI,
				"OBSS color collision detected vdev:%d, event:%d, bitmap:%08llx\n",
				ev->vdev_id, ev->evt_type, ev->obss_color_bitmap);
		goto exit;
	case WMI_BSS_COLOR_COLLISION_DISABLE:
	case WMI_BSS_COLOR_FREE_SLOT_TIMER_EXPIRY:
	case WMI_BSS_COLOR_FREE_SLOT_AVAILABLE:
		goto unlock;
	default:
		ath12k_warn(ab, "received unknown obss color collision detetction event\n");
	}

unlock:
	rcu_read_unlock();
exit:
	kfree(tb);
}

static int
ath12k_wmi_rssi_dbm_conv_subtlv_parser(struct ath12k_base *ab,
				       u16 tag, u16 len,
				       const void *ptr, void *data)
{
	struct wmi_rssi_dbm_conv_offsets *rssi_offsets =
		(struct wmi_rssi_dbm_conv_offsets *) data;
	struct wmi_rssi_dbm_conv_param_info *param_info;
	struct wmi_rssi_dbm_conv_temp_offset *temp_offset_info;
	int i, ret = 0, num_rx_ant, avg_nf = 0;

	switch (tag) {
	case WMI_TAG_RSSI_DBM_CONVERSION_PARAMS_INFO:
		if (len != sizeof(*param_info)) {
			ath12k_warn(ab, "wmi rssi dbm conv subtlv 0x%x invalid len rcvd",
				    tag);
			return -EINVAL;
		}
		param_info = (struct wmi_rssi_dbm_conv_param_info *)ptr;

		num_rx_ant = hweight32(param_info->curr_rx_chainmask);
		if (num_rx_ant < 1 || num_rx_ant > MAX_NUM_ANTENNA) {
			ath12k_warn(ab,
				    "wmi rssi dbm conv subtlv 0x%x recv invalid rx_chain_mask: %d",
				    tag, param_info->curr_rx_chainmask);
			return -EINVAL;
		}
		/* Using minimum pri20 Noise Floor across active chains instead
		 * of all sub-bands*/
		for (i = 0; i < num_rx_ant; i++) {
			if (param_info->curr_rx_chainmask & (0x01 << i))
				avg_nf += param_info->nf_hw_dbm[i][0];
		}
		if (avg_nf)
			avg_nf = avg_nf / num_rx_ant;
		rssi_offsets->avg_nf_dbm = (s8)avg_nf;
		rssi_offsets->xlna_bypass_offset = param_info->xlna_bypass_offset;
		rssi_offsets->xlna_bypass_threshold = param_info->xlna_bypass_threshold;
		break;
	case WMI_TAG_RSSI_DBM_CONVERSION_TEMP_OFFSET_INFO:
		if (len != sizeof(*temp_offset_info)) {
			ath12k_warn(ab, "wmi rssi dbm conv subtlv 0x%x invalid len rcvd",
				    tag);
			return -EINVAL;
		}
		temp_offset_info = (struct wmi_rssi_dbm_conv_temp_offset *)ptr;
		rssi_offsets->rssi_temp_offset = temp_offset_info->rssi_temp_offset;
		break;
	default:
		ath12k_warn(ab, "Received invalid sub-tag for wmi rssi dbm conversion\n");
		ret = -EINVAL;
	}
	return ret;
}

static int
ath12k_wmi_rssi_dbm_conv_event_parser(struct ath12k_base *ab,
				      u16 tag, u16 len,
				      const void *ptr, void *data)
{
	int ret = 0;

	ath12k_dbg(ab, ATH12K_DBG_WMI, "wmi rssi dbm conv tag 0x%x of len %d rcvd",
		   tag, len);
	switch (tag) {
	case WMI_TAG_RSSI_DBM_CONVERSION_PARAMS_INFO_FIXED_PARAM:
		/* Fixed param is already processed*/
		break;
	case WMI_TAG_ARRAY_STRUCT:
		/* len 0 is expected for array of struct when there
		 * is no content of that type inside that tlv
		 */
		if (len == 0)
			return ret;
		ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					  ath12k_wmi_rssi_dbm_conv_subtlv_parser,
					  data);
		break;
	default:
		ath12k_warn(ab, "Received invalid tag for wmi rssi dbm conv interference event\n");
		ret = -EINVAL;
		break;

	}

	return ret;
}

static struct
ath12k *ath12k_wmi_rssi_dbm_process_fixed_param(struct ath12k_base *ab,
						u8 *ptr, size_t len)
{
	struct ath12k *ar;
	const struct wmi_tlv *tlv;
	struct wmi_rssi_dbm_conv_event_fixed_param *fixed_param;
	u16 tlv_tag;

	if(!ptr) {
		ath12k_warn(ab, "No data present in rssi dbm conv event\n");
		return NULL;
	}

	if (len < (sizeof(*fixed_param) + TLV_HDR_SIZE)) {
		ath12k_warn(ab, "rssi dbm conv event size invalid\n");
		return NULL;
	}

	tlv = (struct wmi_tlv *)ptr;
	tlv_tag = FIELD_GET(WMI_TLV_TAG, tlv->header);
	ptr += sizeof(*tlv);

	if (tlv_tag == WMI_TAG_RSSI_DBM_CONVERSION_PARAMS_INFO_FIXED_PARAM) {
		fixed_param = (struct wmi_rssi_dbm_conv_event_fixed_param *)ptr;

		ar = ath12k_mac_get_ar_by_pdev_id(ab, fixed_param->pdev_id);
		if (!ar) {
			ath12k_warn(ab, "Failed to get ar for rssi dbm conv event\n");
			return NULL;
		}
	} else {
		ath12k_warn(ab, "rssi dbm conv event received without fixed param tlv at start\n");
		return NULL;
	}

	return ar;
}

static void ath12k_wmi_rssi_dbm_conversion_param_info(struct ath12k_base *ab,
						      struct sk_buff *skb)
{
	struct ath12k *ar;
	struct wmi_rssi_dbm_conv_offsets *rssi_offsets;
	int ret, i;

	/* if pdevs are not active ignore the event */
	for (i = 0; i < ab->num_radios; i++) {
		if (!ab->pdevs_active[i])
			return;
	}

	ar = ath12k_wmi_rssi_dbm_process_fixed_param(ab, skb->data,
						     skb->len);
	if(!ar) {
		ath12k_warn(ab, "failed to get ar from rssi dbm conversion event\n");
		return;
	}

	rssi_offsets = &ar->rssi_offsets;
	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_rssi_dbm_conv_event_parser,
				  rssi_offsets);
	if (ret) {
		ath12k_warn(ab, "Unable to parse rssi dbm conversion event\n");
		return;
	}

	rssi_offsets->rssi_offset = rssi_offsets->avg_nf_dbm +
				    rssi_offsets->rssi_temp_offset;

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "RSSI offset updated, current offset is %d\n",
		   rssi_offsets->rssi_offset);
}

static int ath12k_wmi_tbtt_offset_subtlv_parser(struct ath12k_base *ab, u16 tag,
						u16 len, const void *ptr,
						void *data)
{
	int ret = 0;
	struct ath12k *ar;
	u64 tx_delay = 0;
	struct wmi_tbtt_offset_info *tbtt_offset_info;
	struct ieee80211_chanctx_conf *conf;
	struct ath12k_link_vif *arvif;
	struct ieee80211_bss_conf *link_conf;
	struct ieee80211_vif *vif;

	tbtt_offset_info = (struct wmi_tbtt_offset_info *)ptr;

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, tbtt_offset_info->vdev_id);
	if (!ar) {
		ath12k_warn(ab, "ar not found, vdev_id %d\n", tbtt_offset_info->vdev_id);
		ret = -EINVAL;
		goto exit;
	}

	arvif = ath12k_mac_get_arvif(ar, tbtt_offset_info->vdev_id);
	if (!arvif) {
		ath12k_warn(ab, "arvif not found, vdev_id %d\n",
			    tbtt_offset_info->vdev_id);
		ret = -EINVAL;
		goto exit;
	}

	vif = arvif->ahvif->vif;
	if (!arvif->is_up || arvif->ahvif->vdev_type != WMI_VDEV_TYPE_AP ||
	    ath12k_mac_is_bridge_vdev(arvif)) {
		ret = 0;
		goto exit;
	}

	arvif->tbtt_offset = tbtt_offset_info->tbtt_offset;

	if (!vif->link_conf[arvif->link_id]) {
		ret = -ENOENT;
		goto exit;
	}

	link_conf = wiphy_dereference(ath12k_ar_to_hw(ar)->wiphy,
				      vif->link_conf[arvif->link_id]);

	if (!link_conf) {
		ret = -ENOENT;
		goto exit;
	}

	if (link_conf->csa_active) {
		ath12k_warn(ab, "Skip TBTT event since CSA is active\n");
		goto exit;
	}

	conf = rcu_dereference(link_conf->chanctx_conf);
	if (!conf) {
		ret = -ENOENT;
		goto exit;
	}

	if (conf->def.chan->band == NL80211_BAND_2GHZ) {
		/* 1Mbps Beacon: */
		/* 144 us ( LPREAMBLE) + 48 (PLCP Header)
		 * + 192 (1Mbps, 24 ytes)
		 * = 384 us + 2us(MAC/BB DELAY
		 */
		tx_delay = 386;
	} else if (conf->def.chan->band == NL80211_BAND_5GHZ ||
		   conf->def.chan->band == NL80211_BAND_6GHZ) {
		/* 6Mbps Beacon: */
		/* 20(lsig)+2(service)+32(6mbps, 24 bytes)
		 * = 54us + 2us(MAC/BB DELAY)
		 */
		tx_delay = 56;
	}
	arvif->tbtt_offset -= tx_delay;
	wiphy_work_queue(ath12k_ar_to_hw(ar)->wiphy, &arvif->update_bcn_template_work);
exit:
	rcu_read_unlock();
	return ret;
}

static int ath12k_wmi_tbtt_offset_event_parser(struct ath12k_base *ab,
					       u16 tag, u16 len,
					       const void *ptr, void *data)
{
	int ret = 0;

	ath12k_dbg(ab, ATH12K_DBG_WMI, "wmi tbtt offset event tag 0x%x of len %d rcvd\n",
		   tag, len);

	switch (tag) {
	case WMI_TAG_TBTT_OFFSET_EXT_EVENT:
		break;
	case WMI_TAG_ARRAY_STRUCT:
		ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					  ath12k_wmi_tbtt_offset_subtlv_parser,
					  data);
		break;
	default:
		ath12k_warn(ab, "Received invalid tag 0x%x for wmi tbtt offset event\n", tag);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ath12k_wmi_pull_tbtt_offset(struct ath12k_base *ab, struct sk_buff *skb,
				       struct wmi_tbtt_offset_ev_arg *arg)
{
	struct wmi_tbtt_offset_info tbtt_offset_info = {0};
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tbtt_offset_event_parser,
				  &tbtt_offset_info);
	if (ret) {
		ath12k_warn(ab, "failed to parse tbtt tlv %d\n", ret);
		return -EINVAL;
	}
	return 0;
}

void ath12k_wmi_event_tbttoffset_update(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_tbtt_offset_ev_arg arg = {};
	int ret;

	ret = ath12k_wmi_pull_tbtt_offset(ab, skb, &arg);
	if (ret)
		ath12k_warn(ab, "failed to parse tbtt offset event: %d\n", ret);
}

static int ath12k_wmi_tlv_mlo_reconfig_link_removal_parse(struct ath12k_base *ab,
							  u16 tag, u16 len,
							  const void *ptr, void *data)
{
	struct ath12k_wmi_mlo_link_removal_event_params *param = data, *tmp;
	struct ath12k_wmi_mlo_link_removal_tbtt_update *tbtt;
	int ret = 0;

	switch (tag) {
	case WMI_TAG_MLO_LINK_REMOVAL_EVENT_FIXED_PARAM:
		tmp = (struct ath12k_wmi_mlo_link_removal_event_params *)ptr;
		param->vdev_id = tmp->vdev_id;
		break;
	case WMI_TAG_MLO_LINK_REMOVAL_TBTT_UPDATE:
		tbtt = (struct ath12k_wmi_mlo_link_removal_tbtt_update *)ptr;
		param->tbtt_info.tbtt_count = le32_to_cpu(tbtt->tbtt_count);
		param->tbtt_info.tsf = (u64)(le32_to_cpu(tbtt->tsf_high)) << 32 |
					     le32_to_cpu(tbtt->tsf_low);
		param->tbtt_info.qtimer_reading = (u64)(le32_to_cpu(tbtt->qtimer_reading_high)) << 32 |
							le32_to_cpu(tbtt->qtimer_reading_low);
		break;
	default:
		ath12k_warn(ab, "Received invalid tag:%u\n", tag);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int
ath12k_wmi_update_link_reconfig_remove_update(struct ath12k_link_vif *arvif,
					      struct ath12k_wmi_mlo_link_removal_event_params *ev)
{
	return ieee80211_update_link_reconfig_remove_update(arvif->ahvif->vif, arvif->link_id,
							    ev->tbtt_info.tbtt_count,
							    ev->tbtt_info.tsf,
							    ev->tbtt_info.tbtt_count ?
							    NL80211_CMD_LINK_REMOVAL_STARTED :
							    NL80211_CMD_LINK_REMOVAL_COMPLETED);
}

static void ath12k_wmi_event_mlo_reconfig_link_removal(struct ath12k_base *ab,
						       struct sk_buff *skb)
{
	struct ath12k_link_vif *arvif;
	struct ath12k_wmi_mlo_link_removal_event_params ev = { };
	struct ath12k *ar;
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_mlo_reconfig_link_removal_parse,
				  &ev);

	if (ret) {
		ath12k_warn(ab, "failed to parse TLV for event:%x ret:%d\n",
			    WMI_MLO_LINK_REMOVAL_EVENTID, ret);
		return;
	}

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif_by_vdev_id(ab, le32_to_cpu(ev.vdev_id));
	if (!arvif) {
		rcu_read_unlock();
		ath12k_warn(ab, "Link removal event received in invalid BSS %d\n",
			    le32_to_cpu(ev.vdev_id));
		return;
	}

	ar = arvif->ar;
	if (!ar) {
		rcu_read_unlock();
		ath12k_warn(ab, "Link removal event received on invalid vdev %d\n",
			    le32_to_cpu(ev.vdev_id));
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI, "Link removal event received in vdev :%d\n",
		   ev.vdev_id);

	if (ev.tbtt_info.tbtt_count && !arvif->is_link_removal_in_progress) {
		arvif->is_link_removal_update_pending = false;
		arvif->is_link_removal_in_progress = true;
		memset(&arvif->link_removal_data, 0,
		       sizeof(struct ath12k_wmi_mlo_link_removal_event_params));
	}

	if (!ev.tbtt_info.tbtt_count) {
		/* If UMAC migration is in progress then return now. The migration event
		 * will notify the upper layer about link removal event
		 */
		if (arvif->is_umac_migration_in_progress) {
			ath12k_dbg(ab, ATH12K_DBG_WMI,
				   "UMAC Migration is in progress. Defer sending link removal event to upper layer\n");
			arvif->is_link_removal_update_pending = true;
			memcpy(&arvif->link_removal_data, &ev,
			       sizeof(struct ath12k_wmi_mlo_link_removal_event_params));

			rcu_read_unlock();
			return;
		}

		arvif->is_link_removal_in_progress = false;
	}

	ret = ath12k_wmi_update_link_reconfig_remove_update(arvif, &ev);
	if (ret)
		ath12k_warn(arvif->ar->ab, "sending link removal event FAILED:%d link_id:%d\n",
			    ret, arvif->link_id);

	rcu_read_unlock();
}

static int ath12k_wmi_tlv_peer_migrate_sub_tlv_parse(struct ath12k_base *ab,
						     u16 tag, u16 len,
						     const void *ptr, void *data)
{
	struct wmi_mlo_pri_link_peer_migr_compl_event *parse = data;

	switch (tag) {
	case WMI_TAG_MLO_PRIMARY_LINK_PEER_MIGRATION_STATUS:
		parse->peer_info[parse->num_pri_link_peer_mig_status++] =
			(struct wmi_mlo_primary_link_peer_migration_status *)ptr;
		break;
	}

	return 0;
}

static int ath12k_wmi_tlv_peer_ptqm_migrate_parse(struct ath12k_base *ab,
						  u16 tag, u16 len,
						  const void *ptr, void *data)
{
	struct wmi_mlo_pri_link_peer_migr_compl_event *parse = data;
	struct wmi_mlo_pri_link_peer_mig_compl_fixed_param *fixed_param;
	int ret = 0;

	switch (tag) {
	case WMI_TAG_MLO_PRIMARY_LINK_PEER_MIGRATION_COMPL_FIXED_PARAM:
		fixed_param = (struct wmi_mlo_pri_link_peer_mig_compl_fixed_param *)ptr;
		parse->fixed_param.vdev_id = fixed_param->vdev_id;
		break;
	case WMI_TAG_ARRAY_STRUCT:
		ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					  ath12k_wmi_tlv_peer_migrate_sub_tlv_parse,
					  parse);
		if (ret) {
			ath12k_warn(ab, "failed to parse PTQM event rx sub tlv %d\n", ret);
			return ret;
		}
		break;
	}

	return 0;
}

static void ath12k_wmi_peer_migration_event(struct ath12k_base *ab,
					      struct sk_buff *skb)
{
	struct wmi_mlo_pri_link_peer_migr_compl_event parse = {0};
	struct ath12k_mac_pri_link_migr_peer_node *peer_node, *tmp_peer;
	struct ath12k *ar;
	struct ath12k_link_vif *arvif;
	struct ath12k_dp_link_peer *peer;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	int vdev_id, num_peers, ret, i;
	u16 ml_peer_id;
	u8 status;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_peer_ptqm_migrate_parse,
				  &parse);
	if (ret) {
		ath12k_warn(ab, "failed to parse mgmt rx tlv %d\n", ret);
		return;
	}

	vdev_id = le32_to_cpu(parse.fixed_param.vdev_id);
	num_peers = le32_to_cpu(parse.num_pri_link_peer_mig_status);

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif_by_vdev_id(ab, vdev_id);
	if (!arvif) {
		rcu_read_unlock();
		ath12k_warn(ab, "MLO Peer Migration event for unknow vdev %d\n",
			    vdev_id);
		return;
	}

	ar = arvif->ar;

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "MLO Peer Migration event received for vdev %d, num_peers %d\n",
		   vdev_id, num_peers);

	spin_lock_bh(&ab->dp->dp_lock);

	for (i = 0; i < num_peers; i++) {
		ml_peer_id = le32_get_bits(parse.peer_info[i]->status_info,
					   WMI_MLO_PRIMARY_LINK_PEER_MIGRATION_STATUS_ML_PEER_ID);
		status = le32_get_bits(parse.peer_info[i]->status_info,
				       WMI_MLO_PRIMARY_LINK_PEER_MIGRATION_STATUS_STATUS);

		list_for_each_entry_safe(peer_node, tmp_peer, &arvif->peer_migrate_list,
					 list) {
			if (peer_node->ml_peer_id == ml_peer_id) {
				list_del(&peer_node->list);
				kfree(peer_node);
			}
		}

		ath12k_wmi_peer_migration_event_extn(arvif->ahvif);

		ml_peer_id |= ATH12K_PEER_ML_ID_VALID;
		peer = ath12k_dp_link_peer_find_by_ml_peer_vdev_id(ab->dp,
								   ml_peer_id,
								   vdev_id);
		if (!peer) {
			ath12k_err(ab, "failed to find ML peer with id %d\n", ml_peer_id);
			goto exit_pri_link_mig_event;
		}

		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "peer migration status ML peer id %d status %u\n",
			   ml_peer_id, status);

		sta = peer->sta;
		if (!sta)
			continue;

		ahsta = (struct ath12k_sta *)sta->drv_priv;

		ahsta->is_migration_in_progress = false;

		/* Only disassoc when ML link removal in not in progress since
		 * if migration failed, ML link removal handling will take
		 * care to disconnect this peer
		 */
		if (!arvif->is_link_removal_in_progress &&
		    status != WMI_PRIMARY_LINK_PEER_MIGRATION_SUCCESS) {
			ath12k_mac_peer_disassoc(ab, sta, ahsta,
						 ATH12K_DBG_WMI);
			continue;
		}

		/* if peer is actually migrated, this condition should fail. If not, then
		 * possibly in HTT part some error occured
		 */
		if (!arvif->is_link_removal_in_progress &&
		    ahsta->primary_link_id == peer->link_id) {
			ath12k_err(ab,
				   "unknown error occured during peer migration, disconnecting\n");
			ath12k_mac_peer_disassoc(ab, sta, ahsta,
						 ATH12K_DBG_WMI);
		}
	}

exit_pri_link_mig_event:
	spin_unlock_bh(&ab->dp->dp_lock);
	rcu_read_unlock();

	/* Event is received for all queued ML peers in this arvif */
	if (list_empty(&arvif->peer_migrate_list)) {
		complete(&arvif->wmi_migration_event_resp);
		arvif->is_umac_migration_in_progress = false;

		/* Migration happened because of ML removal */
		if (arvif->is_link_removal_in_progress &&
		    arvif->is_link_removal_update_pending) {
			ath12k_dbg(ab, ATH12K_DBG_WMI,
				   "ML Link removal update was pending. Send it now\n");

			arvif->is_link_removal_in_progress = false;
			arvif->is_link_removal_update_pending = false;

			ret = ath12k_wmi_update_link_reconfig_remove_update(arvif,
									    &arvif->link_removal_data);
			if (ret)
				ath12k_warn(arvif->ar->ab, "sending link removal event FAILED:%d link_id:%d\n",
					    ret, arvif->link_id);
		}
	}

	return;
}


static void ath12k_wmi_tlv_cfr_cpature_event_fixed_param(const void *ptr,
							 void *data)
{
	struct ath12k_cfr_peer_tx_param *tx_params =
			(struct ath12k_cfr_peer_tx_param *)data;
	struct ath12k_wmi_cfr_peer_tx_event_param *params =
			(struct ath12k_wmi_cfr_peer_tx_event_param *)ptr;

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

static void ath12k_wmi_tlv_cfr_cpature_phase_fixed_param(const void *ptr,
							 void *data)
{
	struct ath12k_cfr_peer_tx_param *tx_params =
			(struct ath12k_cfr_peer_tx_param *)data;
	struct ath12k_wmi_cfr_peer_tx_event_phase_param *params =
			(struct ath12k_wmi_cfr_peer_tx_event_phase_param *)ptr;
	int i;

	for (i = 0; i < WMI_MAX_CHAINS; i++) {
		tx_params->chain_phase[i] = params->chain_phase[i];
		tx_params->agc_gain[i] = params->agc_gain[i];
	}
}

static int ath12k_wmi_tlv_cfr_capture_evt_parse(struct ath12k_base *ab,
						u16 tag, u16 len,
						const void *ptr, void *data)
{
	switch (tag) {
	case WMI_TAG_PEER_CFR_CAPTURE_EVENT:
		ath12k_wmi_tlv_cfr_cpature_event_fixed_param(ptr, data);
		break;
	case WMI_TAG_CFR_CAPTURE_PHASE_PARAM:
		ath12k_wmi_tlv_cfr_cpature_phase_fixed_param(ptr, data);
		break;
	default:
		ath12k_warn(ab, "Invalid tag received tag %d len %d\n",
		tag, len);
		return -EINVAL;
	}

	return 0;
}

static void ath12k_wmi_parse_cfr_capture_event(struct ath12k_base *ab,
					       struct sk_buff *skb)
{
	struct ath12k_cfr_peer_tx_param params = {};
	int ret;

	ath12k_dbg_dump(ab, ATH12K_DBG_CFR_DUMP, "cfr_dump:", "",
			skb->data, skb->len);

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_cfr_capture_evt_parse,
				  &params);
	if (ret) {
		ath12k_warn(ab, "failed to parse cfr capture event tlv %d\n",
			    ret);
		return;
	}

	ret = ath12k_process_cfr_capture_event(ab, &params);
	if (ret)
		ath12k_warn(ab, "failed to process cfr cpature ret = %d\n",
			    ret);
}

static void ath12k_process_ocac_complete_event(struct ath12k_base *ab,
					       struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_vdev_adfs_ocac_complete_event_fixed_param *ev;
	struct ath12k *ar;
	u32 vdev_id, status;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_VDEV_ADFS_OCAC_COMPLETE_EVENT];

	if (!ev) {
		ath12k_warn(ab, "failed to fetch ocac completed ev");
		kfree(tb);
		return;
	}

	vdev_id = __le32_to_cpu(ev->vdev_id);
	status = __le32_to_cpu(ev->status);
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pdev dfs ocac complete event on pdev %d, chan freq %d,"
		   "chan_width %d, status %d  freq %d, freq1  %d, freq2 %d",
		   ev->vdev_id, __le32_to_cpu(ev->chan_freq),
		   __le32_to_cpu(ev->chan_width), status,
		   __le32_to_cpu(ev->center_freq), __le32_to_cpu(ev->center_freq1),
		   __le32_to_cpu(ev->center_freq2));

	ar = ath12k_mac_get_ar_by_vdev_id(ab, vdev_id);

	if (!ar) {
		ath12k_warn(ab, "OCAC complete event in invalid vdev %d\n",
			    ev->vdev_id);
		goto exit;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,"aDFS ocac complete event in vdev %d\n",
		   __le32_to_cpu(ev->vdev_id));

	if (status) {
	    ath12k_mac_background_dfs_event(ar, ATH12K_BGDFS_ABORT);
	} else {
		memset(&ar->agile_chandef, 0, sizeof(struct cfg80211_chan_def));
		ar->agile_chandef.chan = NULL;
	}
exit:
	kfree(tb);
}

static void ath12k_wmi_tid_to_link_map_event(struct ath12k_base *ab,
					     struct sk_buff *skb)
{
	const struct wmi_tid_to_link_mapping_event *ev;
	struct ath12k_link_vif *arvif;
	u16 mapping_switch_tsf;
	u32 status_type;
	const void **tb;
	u32 vdev_id;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_MLO_TID_TO_LINK_MAPPING_EVENT_FIXED_PARAM];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch TID to link mapping ev");
		kfree(tb);
		return;
	}

	vdev_id = le32_to_cpu(ev->vdev_id);
	rcu_read_lock();
	arvif = ath12k_mac_get_arvif_by_vdev_id(ab, vdev_id);
	if (!arvif) {
		ath12k_warn(ab, "failed to find arvif with vedv id %d in tid_to_link_map_event_event\n",
			    vdev_id);
		goto unlock;
	}

	mapping_switch_tsf = le32_get_bits(ev->mapping_switch_tsf,
					   WMI_TTLM_MAPPING_SWITCH_TSF_BITS);
	status_type = le32_to_cpu(ev->status_type);
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "TID to link mapping event received.  vdev:%d, status:%d, mapping_switch_tsf: %u\n",
		   vdev_id, status_type, mapping_switch_tsf);
	ath12k_tid_to_link_mapping_evt_notify(arvif, mapping_switch_tsf,
					      status_type);

unlock:
	rcu_read_unlock();
	kfree(tb);
}

static int ath12k_wmi_tlv_mlo_3_link_tlt_evt_parse(struct ath12k_base *ab,
                                                   u16 tag, u16  len,
                                                   const void *ptr, void *data)
{
        struct mlo_tlt_selection_evt_params *tlt_sel_params;
        struct wmi_mlo_tlt_selection_for_tid_spray_event *ev;
        int i;

        switch (tag) {
        case WMI_TAG_MLO_TLT_SELECTION_FOR_TID_SPRAY_EVENT_FIXED_PARAM:
                tlt_sel_params = (struct mlo_tlt_selection_evt_params *)data;
                ev = (struct wmi_mlo_tlt_selection_for_tid_spray_event *)ptr;

                /* copy mld mac address */
                ether_addr_copy(tlt_sel_params->mld_addr, ev->mld_mac.addr);

                for (i = 0; i < WMI_TLT_MAX_LINKS; i++) {
                        /* fill link bit map values */
                        if (i < WMI_TLT_NUM_TID_PER_AC)
                                tlt_sel_params->link_bmap[i] =
                                        __le32_to_cpu(ev->link_bmap[i]);

                        /* fill link priority */
                        tlt_sel_params->link_priority[i] =
                                __le32_to_cpu(ev->hwlink_priority[i]);
                }
                break;

        default:
                ath12k_warn(ab, "Invalid tag received tag %d len %d\n",
                            tag, len);
                break;
        }
        return 0;
}

static u32 ath12k_mlo_get_link_maxphyrate(struct ath12k_base *ab,
                                          struct ath12k_dp_link_peer *peer,
                                          u16 hw_link_id)
{
       u32 maxphyrate;

       if (hw_link_id == INVALID_HW_LINK_ID) {
               ath12k_err(ab, "invalid hw link id is passed: %d",
                          hw_link_id);
               return 0;
       }

       if (hw_link_id != peer->hw_link_id)
               return 0;

       maxphyrate = cfg80211_calculate_bitrate(&peer->txrate);

       return maxphyrate;
}

static void
ath12k_update_peer_tlt_selection(struct ath12k_base *ab,
                                struct ath12k_dp_link_peer *peer,
                       struct mlo_tlt_selection_evt_params *evt_params)
{
	u8 tid_weight[ATH12K_DATA_TID_MAX] = {0};
	u8 primary_tid_weight = 0;
	u8 secondary_tid_weight = 0;
	u32 primary_tid_capacity = 0;
	u32 secondary_tid_capacity = 0;
	u64 total_capacity = 0;
	u8 i = 0;
	u8 tid_bitmap = 0;
	u8 common_link_tid_bitmap = 0;
	u32 common_link_capacity = 0;

	/* check whether link bitmap is valid or not */
	if ((evt_params->link_bmap[0] == 0) ||
	    (evt_params->link_bmap[1] == 0)) {
		ath12k_err(ab, "invalid link bitmap received "
			   "primary tid bitmap = %d secondary tid bitmap = %d",
			   evt_params->link_bmap[0], evt_params->link_bmap[1]);
		return;
	}

	if (!peer->sta || !peer->sta->valid_links)
		return;

	/* non 3 link association */
	if (hweight16(peer->sta->valid_links) !=
	    ATH12K_3LINK_MLO_MAX_STA_LINKS)
		return;

	/* find the common link */
	common_link_tid_bitmap =
		evt_params->link_bmap[0] & evt_params->link_bmap[1];
	if (common_link_tid_bitmap) {
		common_link_capacity =
			ath12k_mlo_get_link_maxphyrate(ab, peer,
						       GET_3_LINK_TX_HW_LINK_ID(common_link_tid_bitmap));
		/* distribute the common link capacity */
		if (common_link_capacity) {
			primary_tid_capacity = common_link_capacity / 2;
			secondary_tid_capacity = common_link_capacity / 2;
		}
	}

	/* calulate primary tid capacity */
	for (i = 0; i < ATH12K_DATA_TID_MAX; i++) {
		/* skip the common link capacity addition */
		tid_bitmap = ((evt_params->link_bmap[0])  & (1 << i));
		if (tid_bitmap && (tid_bitmap != common_link_tid_bitmap)) {
			primary_tid_capacity +=
				ath12k_mlo_get_link_maxphyrate(ab, peer,
							       GET_3_LINK_TX_HW_LINK_ID(tid_bitmap));
		}
	}

	/* calulate secondary tid capacity */
	for (i = 0; i < ATH12K_DATA_TID_MAX; i++) {
		/* skip the common link capacity addition */
		tid_bitmap = ((evt_params->link_bmap[1])  & (1 << i));
		if (tid_bitmap && (tid_bitmap != common_link_tid_bitmap)) {
			secondary_tid_capacity +=
				ath12k_mlo_get_link_maxphyrate(ab, peer,
							       GET_3_LINK_TX_HW_LINK_ID(tid_bitmap));
		}
	}

	if (!primary_tid_capacity || !secondary_tid_capacity) {
		ath12k_err(ab, "Invalid link phy rate peer mld mac address: %pM"
			   " link_priority = %d:%d:%d primary tid bitmap = %d"
			   " secondary tid bitmap= %d common link bit map = %d",
			   evt_params->mld_addr, evt_params->link_priority[0],
			   evt_params->link_priority[1],
			   evt_params->link_priority[2],
			   evt_params->link_bmap[0], evt_params->link_bmap[1],
			   common_link_tid_bitmap);
		return;
	}

	total_capacity = primary_tid_capacity + secondary_tid_capacity;

	/* percentage calculation */
	primary_tid_weight = div64_u64((u64)(primary_tid_capacity * 100),
				       total_capacity);
	secondary_tid_weight = div64_u64((u64)(secondary_tid_capacity * 100),
					 total_capacity);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "peer mld mac address: %pM, link_priority = %d:%d:%d"
		   " primary_link_capacity = %d secondary_link_capacity = %d"
		   " primary_tid_weight = %d secondary_tid_weight = %d"
		   " primary tid bitmap = %d secondary tid bitmap = %d"
		   " common link capacity = %d common link bit map = %d",
		   evt_params->mld_addr, evt_params->link_priority[0],
		   evt_params->link_priority[1], evt_params->link_priority[2],
		   primary_tid_capacity, secondary_tid_capacity,
		   primary_tid_weight, secondary_tid_weight,
		   evt_params->link_bmap[0], evt_params->link_bmap[1],
		   common_link_capacity, common_link_tid_bitmap);

	/* best effort */
	tid_weight[0] = primary_tid_weight;
	tid_weight[3] = secondary_tid_weight;

	/* background */
	tid_weight[1] = primary_tid_weight;
	tid_weight[2] = secondary_tid_weight;

	/* video */
	tid_weight[4] = primary_tid_weight;
	tid_weight[5] = secondary_tid_weight;

	/* voice */
	tid_weight[6] = primary_tid_weight;
	tid_weight[7] = secondary_tid_weight;

	for (i = 0; i < ATH12K_DATA_TID_MAX; i++) {
		if (peer->tid_weight[i] != tid_weight[i])
			peer->tid_weight[i] = tid_weight[i];
	}

	return;
}

static void ath12k_wmi_mlo_3_link_tlt_selection(struct ath12k_base *ab,
                                                struct sk_buff *skb)
{
        struct mlo_tlt_selection_evt_params tlt_sel_params = {0};
        struct ath12k_dp_link_peer *peer;
	struct ath12k_dp *dp;
        int ret, i;

        ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
                                  ath12k_wmi_tlv_mlo_3_link_tlt_evt_parse,
                                  &tlt_sel_params);
        if (ret) {
                ath12k_warn(ab, "failed to fetch tlt selection tlv %d", ret);
                return;
        }

	dp = ath12k_ab_to_dp(ab);
	spin_lock_bh(&dp->dp_lock);

        for (i = 0; i < ab->num_radios; i++) {
               if (!is_zero_ether_addr(tlt_sel_params.mld_addr)) {
                       peer = ath12k_dp_link_peer_find_by_addr(dp, tlt_sel_params.mld_addr);
                       if (!peer) {
                               continue;
		       }
		       break;
               }
       }

        if (!peer) {
                ath12k_warn(ab, "peer not found %pM",
                            tlt_sel_params.mld_addr);
                goto exit;
        }

        ath12k_update_peer_tlt_selection(ab, peer, &tlt_sel_params);

exit:
	spin_unlock_bh(&dp->dp_lock);
        return;
}

static void
ath12k_wmi_pktlog_decode_info(struct ath12k_base *ab,
                                  struct sk_buff *skb)
{
	struct ath12k *ar;
	const void **tb;
	int ret;
	u32 pdev_id;
	struct ath12k_pktlog *pktlog;
	const struct ath12k_pl_fw_info *pktlog_info;

	if (!test_bit(WMI_TLV_SERVICE_PKTLOG_DECODE_INFO_SUPPORT, ab->wmi_ab.svc_map)) {
		ath12k_warn(ab, "firmware doesn't support pktlog decode info support\n");
		return;
	}
	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}
	pktlog_info = tb[WMI_TAG_PDEV_PKTLOG_DECODE_INFO];
	if (!pktlog_info) {
		ath12k_warn(ab, "failed to fetch pktlog debug info");
		kfree(tb);
		return;
	}

	pdev_id = DP_SW2HW_MACID(pktlog_info->pdev_id);
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pktlog pktlog_defs_json_version: %d", pktlog_info->pktlog_defs_json_version);
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pktlog software_image: %s", pktlog_info->software_image);
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pktlog chip_info: %s", pktlog_info->chip_info);
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pktlog pdev_id: %d", pdev_id);

	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid pdev id in pktlog decode info %d", pdev_id);
		kfree(tb);
		return;
	}
	pktlog = &ar->debug.pktlog;
	pktlog->fw_version_record = 1;
	if (pktlog->buf == NULL) {
		ath12k_warn(ab, "failed to initialize, start pktlog\n");
		kfree(tb);
		return;
	}
	pktlog->buf->bufhdr.magic_num = PKTLOG_MAGIC_NUM_FW_VERSION_SUPPORT;
	memcpy(pktlog->buf->bufhdr.software_image, pktlog_info->software_image, sizeof(pktlog_info->software_image));
	memcpy(pktlog->buf->bufhdr.chip_info, pktlog_info->chip_info, sizeof(pktlog_info->chip_info));
	pktlog->buf->bufhdr.pktlog_defs_json_version = pktlog_info->pktlog_defs_json_version;
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pktlog new magic_num: 0x%x\n", pktlog->buf->bufhdr.magic_num);
	kfree(tb);
}

static void ath12k_wmi_op_rx(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct wmi_cmd_hdr *cmd_hdr;
	enum wmi_tlv_event_id id;
	struct ath12k_wmi_pdev *wmi = NULL;
	u32 i;
	u8 eid, wmi_ep_count;

	eid = skb_cb->u.eid;
	cmd_hdr = (struct wmi_cmd_hdr *)skb->data;
	id = le32_get_bits(cmd_hdr->cmd_id, WMI_CMD_HDR_CMD_ID);

	wmi_ep_count = ab->htc.wmi_ep_count;

	if (!skb_pull(skb, sizeof(struct wmi_cmd_hdr)))
		goto out;

	if (wmi_ep_count <= ab->hw_params->max_radios) {
		for (i = 0; i < ab->htc.wmi_ep_count; i++) {
			if (ab->wmi_ab.wmi[i].eid == eid) {
				wmi = &ab->wmi_ab.wmi[i];
				break;
			}
		}
	}

	if (wmi) {
		if (id != WMI_DIAG_EVENTID && id != WMI_MGMT_RX_EVENTID)
			WMI_EVENT_RX_RECORD(wmi, skb, id);
	}

	switch (id) {
		/* Process all the WMI events here */
	case WMI_SERVICE_READY_EVENTID:
		ath12k_service_ready_event(ab, skb);
		break;
	case WMI_SERVICE_READY_EXT_EVENTID:
		ath12k_service_ready_ext_event(ab, skb);
		break;
	case WMI_SERVICE_READY_EXT2_EVENTID:
		ath12k_service_ready_ext2_event(ab, skb);
		break;
	case WMI_REG_CHAN_LIST_CC_EXT_EVENTID:
		ath12k_reg_chan_list_event(ab, skb);
		break;
	case WMI_READY_EVENTID:
		ath12k_ready_event(ab, skb);
		break;
	case WMI_PEER_DELETE_RESP_EVENTID:
		ath12k_peer_delete_resp_event(ab, skb);
		break;
	case WMI_VDEV_START_RESP_EVENTID:
		ath12k_vdev_start_resp_event(ab, skb);
		break;
	case WMI_OFFLOAD_BCN_TX_STATUS_EVENTID:
		ath12k_bcn_tx_status_event(ab, skb);
		break;
	case WMI_VDEV_STOPPED_EVENTID:
		ath12k_vdev_stopped_event(ab, skb);
		break;
	case WMI_MGMT_RX_EVENTID:
		ath12k_mgmt_rx_event(ab, skb);
		/* mgmt_rx_event() owns the skb now! */
		return;
	case WMI_MGMT_TX_COMPLETION_EVENTID:
		ath12k_mgmt_tx_compl_event(ab, skb);
		break;
	case WMI_SCAN_EVENTID:
		ath12k_scan_event(ab, skb);
		break;
	case WMI_PEER_STA_KICKOUT_EVENTID:
		ath12k_peer_sta_kickout_event(ab, skb);
		break;
	case WMI_ROAM_EVENTID:
		ath12k_roam_event(ab, skb);
		break;
	case WMI_CHAN_INFO_EVENTID:
		ath12k_chan_info_event(ab, skb);
		break;
	case WMI_PDEV_BSS_CHAN_INFO_EVENTID:
		ath12k_pdev_bss_chan_info_event(ab, skb);
		break;
	case WMI_VDEV_INSTALL_KEY_COMPLETE_EVENTID:
		ath12k_vdev_install_key_compl_event(ab, skb);
		break;
	case WMI_SERVICE_AVAILABLE_EVENTID:
		ath12k_service_available_event(ab, skb);
		break;
	case WMI_PEER_ASSOC_CONF_EVENTID:
		ath12k_peer_assoc_conf_event(ab, skb);
		break;
	case WMI_UPDATE_STATS_EVENTID:
		ath12k_update_stats_event(ab, skb);
		break;
	case WMI_PDEV_CTL_FAILSAFE_CHECK_EVENTID:
		ath12k_pdev_ctl_failsafe_check_event(ab, skb);
		break;
	case WMI_PDEV_CSA_SWITCH_COUNT_STATUS_EVENTID:
		ath12k_wmi_pdev_csa_switch_count_status_event(ab, skb);
		break;
	case WMI_PDEV_TEMPERATURE_EVENTID:
		ath12k_wmi_pdev_temperature_event(ab, skb);
		break;
	case WMI_THERM_THROT_STATS_EVENTID:
		ath12k_wmi_thermal_throt_stats_event(ab, skb);
		break;
	case WMI_PDEV_DMA_RING_BUF_RELEASE_EVENTID:
		ath12k_wmi_pdev_dma_ring_buf_release_event(ab, skb);
		break;
	case WMI_HOST_FILS_DISCOVERY_EVENTID:
		ath12k_fils_discovery_event(ab, skb);
		break;
	case WMI_OFFLOAD_PROB_RESP_TX_STATUS_EVENTID:
		ath12k_probe_resp_tx_status_event(ab, skb);
		break;
	case WMI_RFKILL_STATE_CHANGE_EVENTID:
		ath12k_rfkill_state_change_event(ab, skb);
		break;
	case WMI_TWT_ENABLE_EVENTID:
		ath12k_wmi_twt_enable_event(ab, skb);
		break;
	case WMI_TWT_DISABLE_EVENTID:
		ath12k_wmi_twt_disable_event(ab, skb);
		break;
	case WMI_P2P_NOA_EVENTID:
		ath12k_wmi_p2p_noa_event(ab, skb);
		break;
	case WMI_PDEV_DFS_RADAR_DETECTION_EVENTID:
		ath12k_wmi_pdev_dfs_radar_detected_event(ab, skb);
		break;
	case WMI_VDEV_DELETE_RESP_EVENTID:
		ath12k_vdev_delete_resp_event(ab, skb);
		break;
	case WMI_DIAG_EVENTID:
		ath12k_wmi_diag_event(ab, skb);
		break;
	case WMI_WOW_WAKEUP_HOST_EVENTID:
		ath12k_wmi_event_wow_wakeup_host(ab, skb);
		break;
	case WMI_GTK_OFFLOAD_STATUS_EVENTID:
		ath12k_wmi_gtk_offload_status_event(ab, skb);
		break;
	case WMI_MLO_SETUP_COMPLETE_EVENTID:
		ath12k_wmi_event_mlo_setup_complete(ab, skb);
		break;
	case WMI_MLO_TEARDOWN_COMPLETE_EVENTID:
		ath12k_wmi_event_teardown_complete(ab, skb);
		break;
	case WMI_HALPHY_STATS_CTRL_PATH_EVENTID:
		ath12k_wmi_process_tpc_stats(ab, skb);
		break;
	case WMI_OFFCHAN_DATA_TX_COMPLETION_EVENTID:
		ath12k_offchan_tx_completion_event(ab, skb);
		break;
	case WMI_CTRL_PATH_STATS_EVENTID:
		ath12k_wmi_ctrl_path_stats_event(ab, skb);
		break;
	case WMI_11D_NEW_COUNTRY_EVENTID:
		ath12k_reg_11d_new_cc_event(ab, skb);
		break;
	case WMI_PEER_CREATE_CONF_EVENTID:
		ath12k_wmi_peer_create_conf_event(ab, skb);
		break;
	case WMI_TBTTOFFSET_EXT_UPDATE_EVENTID:
		ath12k_wmi_event_tbttoffset_update(ab, skb);
		break;
	/* add Unsupported events (rare) here */
	case WMI_PEER_OPER_MODE_CHANGE_EVENTID:
	case WMI_PDEV_DMA_RING_CFG_RSP_EVENTID:
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "ignoring unsupported event 0x%x\n", id);
		break;
	/* add Unsupported events (frequent) here */
	case WMI_PDEV_GET_HALPHY_CAL_STATUS_EVENTID:
	case WMI_MGMT_RX_FW_CONSUMED_EVENTID:
	case WMI_TWT_DEL_DIALOG_EVENTID:
	case WMI_TWT_PAUSE_DIALOG_EVENTID:
	case WMI_TWT_RESUME_DIALOG_EVENTID:
		/* debug might flood hence silently ignore (no-op) */
		break;
	case WMI_PDEV_SSCAN_FW_PARAM_EVENTID:
		ath12k_wmi_pdev_sscan_fw_param_event(ab, skb);
		break;
	case WMI_SPECTRAL_CAPABILITIES_EVENTID:
		ath12k_wmi_spectral_capabilities_event(ab, skb);
		break;
	case WMI_PDEV_RESUME_EVENTID:
		ath12k_wmi_pdev_resume_event(ab, skb);
		break;
	case WMI_PDEV_SUSPEND_EVENTID:
		ath12k_wmi_suspend_event(ab, skb);
		break;
	case WMI_PDEV_UTF_EVENTID:
		if (test_bit(ATH12K_FLAG_FTM_SEGMENTED, &ab->dev_flags))
			ath12k_tm_wmi_event_segmented(ab, id, skb);
		else
			ath12k_tm_wmi_event_unsegmented(ab, id, skb);
		break;
	case WMI_DCS_INTERFERENCE_EVENTID:
		ath12k_wmi_dcs_interference_event(ab, skb);
                break;
	case WMI_MUEDCA_PARAMS_CONFIG_EVENTID:
		ath12k_wmi_pdev_update_muedca_params_status_event(ab, skb);
		break;
	case WMI_PDEV_MULTIPLE_VDEV_RESTART_RESP_EVENTID:
		ath12k_wmi_event_mvr_response(ab, skb);
		break;
	case WMI_TWT_ADD_DIALOG_EVENTID:
		ath12k_wmi_twt_add_dialog_event(ab, skb);
		break;
	case WMI_OBSS_COLOR_COLLISION_DETECTION_EVENTID:
		ath12k_wmi_obss_color_collision_event(ab, skb);
		break;
	case WMI_PEER_CFR_CAPTURE_EVENTID:
		ath12k_wmi_parse_cfr_capture_event(ab, skb);
		break;
	case WMI_PDEV_RSSI_DBM_CONVERSION_PARAMS_INFO_EVENTID:
		ath12k_wmi_rssi_dbm_conversion_param_info(ab, skb);
		break;
	case WMI_MLO_LINK_REMOVAL_EVENTID:
		ath12k_wmi_event_mlo_reconfig_link_removal(ab, skb);
		break;
	case WMI_MLO_PRIMARY_LINK_PEER_MIGRATION_EVENT_ID:
		ath12k_wmi_peer_migration_event(ab, skb);
		break;
	case WMI_AFC_EVENTID:
		ath12k_wmi_afc_event(ab, skb);
		break;
	case WMI_VDEV_ADFS_OCAC_COMPLETE_EVENTID:
		ath12k_process_ocac_complete_event(ab, skb);
		break;
	case WMI_MLO_TID_TO_LINK_MAP_EVENT_ID:
		ath12k_wmi_tid_to_link_map_event(ab, skb);
		break;
	case WMI_PDEV_PKTLOG_DECODE_INFO_EVENTID:
		ath12k_wmi_pktlog_decode_info(ab, skb);
		break;
	case WMI_TWT_BTWT_INVITE_STA_COMPLETE_EVENTID:
		ath12k_wmi_twt_btwt_invite_sta_compl_event(ab, skb);
		break;
	case WMI_TWT_BTWT_REMOVE_STA_COMPLETE_EVENTID:
		ath12k_wmi_twt_btwt_remove_sta_compl_event(ab, skb);
		break;
	case WMI_MLO_TLT_SELECTION_FOR_TID_SPRAY_EVENTID:
		ath12k_wmi_mlo_3_link_tlt_selection(ab, skb);
		break;

	default:
		ath12k_dbg_level(ab, ATH12K_DBG_WMI, ATH12K_DBG_L1,
				 "Unknown eventid: 0x%x\n", id);
		break;
	}

out:
	dev_kfree_skb(skb);
}

static int ath12k_connect_pdev_htc_service(struct ath12k_base *ab,
					   u32 pdev_idx)
{
	int status;
	static const u32 svc_id[] = {
		ATH12K_HTC_SVC_ID_WMI_CONTROL,
		ATH12K_HTC_SVC_ID_WMI_CONTROL_MAC1,
		ATH12K_HTC_SVC_ID_WMI_CONTROL_MAC2
	};
	struct ath12k_htc_svc_conn_req conn_req = {};
	struct ath12k_htc_svc_conn_resp conn_resp = {};

	/* these fields are the same for all service endpoints */
	conn_req.ep_ops.ep_tx_complete = ath12k_wmi_htc_tx_complete;
	conn_req.ep_ops.ep_rx_complete = ath12k_wmi_op_rx;
	conn_req.ep_ops.ep_tx_credits = ath12k_wmi_op_ep_tx_credits;

	/* connect to control service */
	conn_req.service_id = svc_id[pdev_idx];

	status = ath12k_htc_connect_service(&ab->htc, &conn_req, &conn_resp);
	if (status) {
		ath12k_warn(ab, "failed to connect to WMI CONTROL service status: %d\n",
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
ath12k_wmi_send_unit_test_cmd(struct ath12k *ar,
			      struct wmi_unit_test_cmd ut_cmd,
			      u32 *test_args)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_unit_test_cmd *cmd;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;
	u32 *ut_cmd_args;
	int buf_len, arg_len;
	int ret;
	int i;

	arg_len = sizeof(u32) * le32_to_cpu(ut_cmd.num_args);
	buf_len = sizeof(ut_cmd) + arg_len + TLV_HDR_SIZE;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, buf_len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_unit_test_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_UNIT_TEST_CMD,
						 sizeof(ut_cmd));

	cmd->vdev_id = ut_cmd.vdev_id;
	cmd->module_id = ut_cmd.module_id;
	cmd->num_args = ut_cmd.num_args;
	cmd->diag_token = ut_cmd.diag_token;

	ptr = skb->data + sizeof(ut_cmd);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, arg_len);

	ptr += TLV_HDR_SIZE;

	ut_cmd_args = ptr;
	for (i = 0; i < le32_to_cpu(ut_cmd.num_args); i++)
		ut_cmd_args[i] = test_args[i];

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI unit test : module %d vdev %d n_args %d token %d\n",
		   cmd->module_id, cmd->vdev_id, cmd->num_args,
		   cmd->diag_token);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_UNIT_TEST_CMDID);

	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_UNIT_TEST CMD :%d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_set_afc_grace_timer(struct ath12k *ar, u32 afc_grace_timer_value)
{
	struct ath12k_link_vif *arvif;
	u32 afc_args[AFC_MAX_TEST_ARGS];
	struct wmi_unit_test_cmd wmi_ut;
	bool arvif_found = false;

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (arvif->is_started) {
			arvif_found = true;
			break;
		}
	}

	if (!arvif_found) {
		ath12k_warn(ar->ab, "No valid vdev found to set AFC grace timer on ar:%d\n",
			    ar->pdev_idx);
		return -EINVAL;
	}

	afc_args[AFC_GRACE_TIMER_SUBCMDID] = AFC_UNIT_TEST_GRACE_TIMER_SUBCMDID;
	afc_args[AFC_GRACE_TIMER_PDEV_ID] = ar->pdev->pdev_id;
	afc_args[AFC_GRACE_TIMER_VALUE] = afc_grace_timer_value;

	wmi_ut.vdev_id = cpu_to_le32(arvif->vdev_id);
	wmi_ut.module_id = cpu_to_le32(AFC_UNIT_TEST_MODULE_ID);
	wmi_ut.num_args = cpu_to_le32(AFC_MAX_TEST_ARGS);
	wmi_ut.diag_token = cpu_to_le32(AFC_UNIT_TEST_TOKEN);

	ath12k_dbg(ar->ab, ATH12K_DBG_REG, "Sending AFC grace timer value to FW\n");

	return ath12k_wmi_send_unit_test_cmd(ar, wmi_ut, afc_args);
}

int ath12k_wmi_simulate_radar(struct ath12k *ar, u32 radar_params)
{
	struct ath12k_link_vif *arvif;
	u32 dfs_args[DFS_MAX_TEST_ARGS];
	struct wmi_unit_test_cmd wmi_ut;
	bool arvif_found = false;

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (arvif->is_started && arvif->ahvif->vdev_type == WMI_VDEV_TYPE_AP) {
			arvif_found = true;
			break;
		}
	}

	if (!arvif_found)
		return -EINVAL;

	dfs_args[DFS_TEST_CMDID] = 0;
	dfs_args[DFS_TEST_PDEV_ID] = ar->pdev->pdev_id;
	/* Currently we pass segment_id(b0 - b1), chirp(b2)
	 * freq offset (b3 - b10), detector_id(b11 - b12) to unit test.
	 */
	dfs_args[DFS_TEST_RADAR_PARAM] = radar_params;

	wmi_ut.vdev_id = cpu_to_le32(arvif->vdev_id);
	wmi_ut.module_id = cpu_to_le32(DFS_UNIT_TEST_MODULE);
	wmi_ut.num_args = cpu_to_le32(DFS_MAX_TEST_ARGS);
	wmi_ut.diag_token = cpu_to_le32(DFS_UNIT_TEST_TOKEN);

	ath12k_dbg(ar->ab, ATH12K_DBG_REG, "Triggering Radar Simulation\n");

	return ath12k_wmi_send_unit_test_cmd(ar, wmi_ut, dfs_args);
}

int ath12k_wmi_dbglog_cfg(struct ath12k *ar, u32 param, u64 value)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_dbglog_config_cmd_fixed_param *cmd;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	u32 module_id_bitmap;
	int ret, len;

	len = sizeof(*cmd) + TLV_HDR_SIZE + sizeof(module_id_bitmap);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;
	cmd = (struct wmi_dbglog_config_cmd_fixed_param *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_DEBUG_LOG_CONFIG_CMD,
						 sizeof(*cmd));
	cmd->dbg_log_param = param;

	tlv = (struct wmi_tlv *)((u8 *)cmd + sizeof(*cmd));
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_UINT32) |
		      FIELD_PREP(WMI_TLV_LEN, sizeof(u32));

	switch (param) {
	case WMI_DEBUG_LOG_PARAM_LOG_LEVEL:
	case WMI_DEBUG_LOG_PARAM_VDEV_ENABLE:
	case WMI_DEBUG_LOG_PARAM_VDEV_DISABLE:
	case WMI_DEBUG_LOG_PARAM_VDEV_ENABLE_BITMAP:
		cmd->value = value;
		break;
	case WMI_DEBUG_LOG_PARAM_MOD_ENABLE_BITMAP:
	case WMI_DEBUG_LOG_PARAM_WOW_MOD_ENABLE_BITMAP:
		cmd->value = value;
		module_id_bitmap = value >> 32;
		memcpy(tlv->value, &module_id_bitmap, sizeof(module_id_bitmap));
		break;
	default:
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_DBGLOG_CFG_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_DBGLOG_CFG_CMDID\n");
		dev_kfree_skb(skb);
	}
	return ret;
}

#ifdef CPTCFG_ATH12K_DEBUGFS

int ath12k_wmi_send_twt_btwt_invite_sta_cmd(struct ath12k *ar,
					    struct wmi_twt_btwt_invite_sta_params *params)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_btwt_invite_sta_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_btwt_invite_sta_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_TWT_BTWT_INVITE_STA_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = params->vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, params->peer_macaddr);
	cmd->dialog_id = params->dialog_id;
	cmd->r_twt_dl_tid_bitmap = params->r_twt_dl_tid_bitmap;
	cmd->r_twt_dl_tid_bitmap = params->r_twt_dl_tid_bitmap;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi BTWT invite sta vdev %u dialog id %u\n",
		   cmd->vdev_id, cmd->dialog_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_TWT_BTWT_INVITE_STA_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "failed to send wmi command to B-TWT invite sta: %d",
			    ret);
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_send_twt_btwt_remove_sta_cmd(struct ath12k *ar,
					    struct wmi_twt_btwt_remove_sta_params *params)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_btwt_remove_sta_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_btwt_remove_sta_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_TWT_BTWT_REMOVE_STA_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = params->vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, params->peer_macaddr);
	cmd->dialog_id = params->dialog_id;
	cmd->r_twt_dl_tid_bitmap = params->r_twt_dl_tid_bitmap;
	cmd->r_twt_dl_tid_bitmap = params->r_twt_dl_tid_bitmap;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi BTWT remove sta vdev %u dialog id %u\n",
		   cmd->vdev_id, cmd->dialog_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_TWT_BTWT_REMOVE_STA_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "failed to send wmi command to B-TWT remove sta: %d",
			    ret);
		dev_kfree_skb(skb);
	}
	return ret;
}

int
ath12k_wmi_send_wmi_ctrl_stats_cmd(struct ath12k *ar,
				   struct wmi_ctrl_path_stats_arg *arg)
{
	struct wmi_request_ctrl_path_stats_cmd_fixed_param fixed_cmd;
	struct wmi_ctrl_path_stats_cmd *cmd;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct ath12k_debug *debug = &ar->debug;
	__le32 pdev_id;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	int len, ret;
	void *ptr;
	u32 stats_id;

	switch (arg->stats_id) {
	case WMI_REQ_CTRL_PATH_PDEV_TX_STAT:
	case WMI_REQ_CTRL_PATH_CAL_STAT:
	case WMI_REQ_CTRL_PATH_BTCOEX_STAT:
		stats_id = (1 << arg->stats_id);
		break;
	case WMI_REQ_CTRL_PATH_AWGN_STAT:
	case WMI_REQ_CTRL_PATH_AFC_STAT:
		if (ar->supports_6ghz) {
			stats_id = (1 << arg->stats_id);
		} else {
			ath12k_warn(ab,
			  "Stats id %d %s stats are only supported for 6GHz",
			  arg->stats_id,
			  (arg->stats_id ==
			   WMI_REQ_CTRL_PATH_AWGN_STAT) ? "AWGN" : "AFC");
			return -EIO;
		}
		break;
	case WMI_REQ_CTRL_PATH_MEM_STAT:
		ar->ctrl_mem_stats = true;
		stats_id = (1 << arg->stats_id);
		break;
		/* Add case for newly wmi ctrl path stats here */
	case WMI_REQ_CTRL_PATH_PMLO_STAT:
		fixed_cmd.request_id = arg->req_id;
		fixed_cmd.action = arg->action;
		fixed_cmd.subid = 0;
		return ath12k_wmi_pdev_enable_telemetry_stats(ab, ar,
							      &fixed_cmd);
	default:
		ath12k_warn(ab, "Unsupported stats id %d", arg->stats_id);
		return -EIO;
		break;
	}

	pdev_id = cpu_to_le32(ath12k_mac_get_target_pdev_id(ar));

	len = sizeof(*cmd) +
		TLV_HDR_SIZE + sizeof(u32) +
		TLV_HDR_SIZE + TLV_HDR_SIZE;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (void *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_CTRL_PATH_STATS_CMD_FIXED_PARAM,
						 sizeof(*cmd));
	cmd->stats_id = cpu_to_le32(stats_id);
	cmd->req_id = cpu_to_le32(arg->req_id);
	cmd->action = cpu_to_le32(arg->action);

	ptr = skb->data + sizeof(*cmd);

	/* The below TLV arrays optionally follow this fixed param TLV structure
	 * 1. ARRAY_UINT32 pdev_ids[]
	 *      If this array is present and non-zero length, stats should only
	 *      be provided from the pdevs identified in the array.
	 * 2. ARRAY_UNIT32 vdev_ids[]
	 *      If this array is present and non-zero length, stats should only
	 *      be provided from the vdevs identified in the array.
	 * 3. ath12k_wmi_mac_addr_params peer_macaddr[];
	 *      If this array is present and non-zero length, stats should only
	 *      be provided from the peers with the MAC addresses specified
	 *      in the array
	 */

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, sizeof(u32));
	ptr += TLV_HDR_SIZE;
	memcpy(ptr, &pdev_id, sizeof(u32));
	ptr += sizeof(u32);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, 0);
	ptr += TLV_HDR_SIZE;

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_FIXED_STRUCT, 0);
	ptr += TLV_HDR_SIZE;

	if (arg->action == WMI_REQUEST_CTRL_PATH_STAT_GET)
		reinit_completion(&ar->debug.wmi_ctrl_path_stats_rcvd);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_REQUEST_CTRL_PATH_STATS_CMDID);
	if (ret) {
		dev_kfree_skb(skb);
		ath12k_warn(ab, "Failed to send WMI_REQUEST_CTRL_PATH_STATS_CMDID: %d",
			    ret);
	} else {
		if (arg->action == WMI_REQUEST_CTRL_PATH_STAT_GET) {
			if (!wait_for_completion_timeout(&debug->wmi_ctrl_path_stats_rcvd,
							 WMI_CTRL_STATS_READY_TIMEOUT)) {
				ath12k_warn(ab, "wmi ctrl path stats timed out\n");
				ret = -ETIMEDOUT;
			}
		}
	}
	return ret;
}
#endif

int ath12k_wmi_send_tpc_stats_request(struct ath12k *ar,
				      enum wmi_halphy_ctrl_path_stats_id tpc_stats_type)
{
	struct wmi_request_halphy_ctrl_path_stats_cmd_fixed_params *cmd;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	__le32 *pdev_id;
	u32 buf_len;
	void *ptr;
	int ret;

	buf_len = sizeof(*cmd) + TLV_HDR_SIZE + sizeof(u32) + TLV_HDR_SIZE + TLV_HDR_SIZE;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, buf_len);
	if (!skb)
		return -ENOMEM;
	cmd = (struct wmi_request_halphy_ctrl_path_stats_cmd_fixed_params *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_HALPHY_CTRL_PATH_CMD_FIXED_PARAM,
						 sizeof(*cmd));

	cmd->stats_id_mask = cpu_to_le32(WMI_REQ_CTRL_PATH_PDEV_TX_STAT);
	cmd->action = cpu_to_le32(WMI_REQUEST_CTRL_PATH_STAT_GET);
	cmd->subid = cpu_to_le32(tpc_stats_type);

	ptr = skb->data + sizeof(*cmd);

	/* The below TLV arrays optionally follow this fixed param TLV structure
	 * 1. ARRAY_UINT32 pdev_ids[]
	 *      If this array is present and non-zero length, stats should only
	 *      be provided from the pdevs identified in the array.
	 * 2. ARRAY_UNIT32 vdev_ids[]
	 *      If this array is present and non-zero length, stats should only
	 *      be provided from the vdevs identified in the array.
	 * 3. ath12k_wmi_mac_addr_params peer_macaddr[];
	 *      If this array is present and non-zero length, stats should only
	 *      be provided from the peers with the MAC addresses specified
	 *      in the array
	 */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, sizeof(u32));
	ptr += TLV_HDR_SIZE;

	pdev_id = ptr;
	*pdev_id = cpu_to_le32(ath12k_mac_get_target_pdev_id(ar));
	ptr += sizeof(*pdev_id);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, 0);
	ptr += TLV_HDR_SIZE;

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_FIXED_STRUCT, 0);
	ptr += TLV_HDR_SIZE;

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_REQUEST_HALPHY_CTRL_PATH_STATS_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_REQUEST_STATS_CTRL_PATH_CMDID\n");
		dev_kfree_skb(skb);
		return ret;
	}
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "WMI get TPC STATS sent on pdev %d\n",
		   ar->pdev->pdev_id);

	return ret;
}

int ath12k_wmi_pdev_m3_dump_enable(struct ath12k *ar, u32 enable)
{
        struct ath12k_link_vif *arvif;
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

        ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "%s M3 SSR dump\n",
                   enable ? "Enabling" : "Disabling");

        return ath12k_wmi_send_unit_test_cmd(ar, wmi_ut, m3_args);
}

int ath12k_wmi_simulate_awgn(struct ath12k *ar, u32 chan_bw_interference_bitmap)
{
	u32 awgn_args[WMI_AWGN_MAX_TEST_ARGS];
	struct wmi_unit_test_cmd wmi_ut;
        struct ath12k_link_vif *arvif;
	struct ath12k_vif *ahvif;
        bool arvif_found = false;

        if (!test_bit(WMI_TLV_SERVICE_DCS_AWGN_INT_SUPPORT, ar->ab->wmi_ab.svc_map)) {
                ath12k_warn(ar->ab, "firmware doesn't support awgn interference, so can't simulate it\n");
                return -EOPNOTSUPP;
        }

        list_for_each_entry(arvif, &ar->arvifs, list) {
		ahvif = arvif->ahvif;
                if (arvif->is_started && ahvif->vdev_type == WMI_VDEV_TYPE_AP) {
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

        ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
                   "Triggering AWGN Simulation, interference bitmap : 0x%x\n",
                   chan_bw_interference_bitmap);

        return ath12k_wmi_send_unit_test_cmd(ar, wmi_ut, awgn_args);
}

int ath12k_wmi_connect(struct ath12k_base *ab)
{
	u32 i;
	u8 wmi_ep_count;

	wmi_ep_count = ab->htc.wmi_ep_count;
	if (wmi_ep_count > ab->hw_params->max_radios)
		return -1;

	for (i = 0; i < wmi_ep_count; i++)
		ath12k_connect_pdev_htc_service(ab, i);

	return 0;
}

static void ath12k_wmi_pdev_detach(struct ath12k_base *ab, u8 pdev_id)
{
	if (WARN_ON(pdev_id >= MAX_RADIOS))
		return;

	/* TODO: Deinit any pdev specific wmi resource */
}

int ath12k_wmi_pdev_attach(struct ath12k_base *ab,
			   u8 pdev_id)
{
	struct ath12k_wmi_pdev *wmi_handle;

	if (pdev_id >= ab->hw_params->max_radios)
		return -EINVAL;

	wmi_handle = &ab->wmi_ab.wmi[pdev_id];

	wmi_handle->wmi_ab = &ab->wmi_ab;

	ab->wmi_ab.ab = ab;
	/* TODO: Init remaining resource specific to pdev */

	return 0;
}

int ath12k_wmi_attach(struct ath12k_base *ab)
{
	int ret;

	ret = ath12k_wmi_pdev_attach(ab, 0);
	if (ret)
		return ret;

	ab->wmi_ab.ab = ab;
	ab->wmi_ab.preferred_hw_mode = WMI_HOST_HW_MODE_MAX;

	/* It's overwritten when service_ext_ready is handled */
	if (ab->hw_params->single_pdev_only)
		ab->wmi_ab.preferred_hw_mode = WMI_HOST_HW_MODE_SINGLE;

	/* TODO: Init remaining wmi soc resources required */
	init_completion(&ab->wmi_ab.service_ready);
	init_completion(&ab->wmi_ab.unified_ready);

	return 0;
}

void ath12k_wmi_detach(struct ath12k_base *ab)
{
	int i;

	/* TODO: Deinit wmi resource specific to SOC as required */

	for (i = 0; i < ab->htc.wmi_ep_count; i++)
		ath12k_wmi_pdev_detach(ab, i);

	clear_bit(ATH12K_FLAG_WMI_INIT_DONE, &ab->dev_flags);
	ath12k_wmi_free_dbring_caps(ab);
}

int ath12k_wmi_hw_data_filter_cmd(struct ath12k *ar, struct wmi_hw_data_filter_arg *arg)
{
	struct wmi_hw_data_filter_cmd *cmd;
	struct sk_buff *skb;
	int len;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);

	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_hw_data_filter_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_HW_DATA_FILTER_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->enable = cpu_to_le32(arg->enable ? 1 : 0);

	/* Set all modes in case of disable */
	if (arg->enable)
		cmd->hw_filter_bitmap = cpu_to_le32(arg->hw_filter_bitmap);
	else
		cmd->hw_filter_bitmap = cpu_to_le32((u32)~0U);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi hw data filter enable %d filter_bitmap 0x%x\n",
		   arg->enable, arg->hw_filter_bitmap);

	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_HW_DATA_FILTER_CMDID);
}

int ath12k_wmi_wow_host_wakeup_ind(struct ath12k *ar)
{
	struct wmi_wow_host_wakeup_cmd *cmd;
	struct sk_buff *skb;
	size_t len;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_wow_host_wakeup_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_WOW_HOSTWAKEUP_FROM_SLEEP_CMD,
						 sizeof(*cmd));

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi tlv wow host wakeup ind\n");

	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_WOW_HOSTWAKEUP_FROM_SLEEP_CMDID);
}

int ath12k_wmi_wow_enable(struct ath12k *ar)
{
	struct wmi_wow_enable_cmd *cmd;
	struct sk_buff *skb;
	int len;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_wow_enable_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_WOW_ENABLE_CMD,
						 sizeof(*cmd));

	cmd->enable = cpu_to_le32(1);
	cmd->pause_iface_config = cpu_to_le32(WOW_IFACE_PAUSE_ENABLED);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi tlv wow enable\n");

	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_WOW_ENABLE_CMDID);
}

int ath12k_wmi_wow_add_wakeup_event(struct ath12k *ar, u32 vdev_id,
				    enum wmi_wow_wakeup_event event,
				    u32 enable)
{
	struct wmi_wow_add_del_event_cmd *cmd;
	struct sk_buff *skb;
	size_t len;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_wow_add_del_event_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_WOW_ADD_DEL_EVT_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->is_add = cpu_to_le32(enable);
	cmd->event_bitmap = cpu_to_le32((1 << event));

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi tlv wow add wakeup event %s enable %d vdev_id %d\n",
		   wow_wakeup_event(event), enable, vdev_id);

	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_WOW_ENABLE_DISABLE_WAKE_EVENT_CMDID);
}

int ath12k_wmi_wow_add_pattern(struct ath12k *ar, u32 vdev_id, u32 pattern_id,
			       const u8 *pattern, const u8 *mask,
			       int pattern_len, int pattern_offset)
{
	struct wmi_wow_add_pattern_cmd *cmd;
	struct wmi_wow_bitmap_pattern_params *bitmap;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	size_t len;

	len = sizeof(*cmd) +
	      sizeof(*tlv) +			/* array struct */
	      sizeof(*bitmap) +			/* bitmap */
	      sizeof(*tlv) +			/* empty ipv4 sync */
	      sizeof(*tlv) +			/* empty ipv6 sync */
	      sizeof(*tlv) +			/* empty magic */
	      sizeof(*tlv) +			/* empty info timeout */
	      sizeof(*tlv) + sizeof(u32);	/* ratelimit interval */

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	/* cmd */
	ptr = skb->data;
	cmd = ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_WOW_ADD_PATTERN_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->pattern_id = cpu_to_le32(pattern_id);
	cmd->pattern_type = cpu_to_le32(WOW_BITMAP_PATTERN);

	ptr += sizeof(*cmd);

	/* bitmap */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, sizeof(*bitmap));

	ptr += sizeof(*tlv);

	bitmap = ptr;
	bitmap->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_WOW_BITMAP_PATTERN_T,
						    sizeof(*bitmap));
	memcpy(bitmap->patternbuf, pattern, pattern_len);
	memcpy(bitmap->bitmaskbuf, mask, pattern_len);
	bitmap->pattern_offset = cpu_to_le32(pattern_offset);
	bitmap->pattern_len = cpu_to_le32(pattern_len);
	bitmap->bitmask_len = cpu_to_le32(pattern_len);
	bitmap->pattern_id = cpu_to_le32(pattern_id);

	ptr += sizeof(*bitmap);

	/* ipv4 sync */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);

	ptr += sizeof(*tlv);

	/* ipv6 sync */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);

	ptr += sizeof(*tlv);

	/* magic */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);

	ptr += sizeof(*tlv);

	/* pattern info timeout */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, 0);

	ptr += sizeof(*tlv);

	/* ratelimit interval */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, sizeof(u32));

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi tlv wow add pattern vdev_id %d pattern_id %d pattern_offset %d pattern_len %d\n",
		   vdev_id, pattern_id, pattern_offset, pattern_len);

	ath12k_dbg_dump(ar->ab, ATH12K_DBG_WMI, NULL, "wow pattern: ",
			bitmap->patternbuf, pattern_len);
	ath12k_dbg_dump(ar->ab, ATH12K_DBG_WMI, NULL, "wow bitmask: ",
			bitmap->bitmaskbuf, pattern_len);

	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_WOW_ADD_WAKE_PATTERN_CMDID);
}

int ath12k_wmi_wow_del_pattern(struct ath12k *ar, u32 vdev_id, u32 pattern_id)
{
	struct wmi_wow_del_pattern_cmd *cmd;
	struct sk_buff *skb;
	size_t len;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_wow_del_pattern_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_WOW_DEL_PATTERN_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->pattern_id = cpu_to_le32(pattern_id);
	cmd->pattern_type = cpu_to_le32(WOW_BITMAP_PATTERN);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi tlv wow del pattern vdev_id %d pattern_id %d\n",
		   vdev_id, pattern_id);

	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_WOW_DEL_WAKE_PATTERN_CMDID);
}

static struct sk_buff *
ath12k_wmi_op_gen_config_pno_start(struct ath12k *ar, u32 vdev_id,
				   struct wmi_pno_scan_req_arg *pno)
{
	struct nlo_configured_params *nlo_list;
	size_t len, nlo_list_len, channel_list_len;
	struct wmi_wow_nlo_config_cmd *cmd;
	__le32 *channel_list;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	u32 i;

	len = sizeof(*cmd) +
	      sizeof(*tlv) +
	      /* TLV place holder for array of structures
	       * nlo_configured_params(nlo_list)
	       */
	      sizeof(*tlv);
	      /* TLV place holder for array of uint32 channel_list */

	channel_list_len = sizeof(u32) * pno->a_networks[0].channel_count;
	len += channel_list_len;

	nlo_list_len = sizeof(*nlo_list) * pno->uc_networks_count;
	len += nlo_list_len;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return ERR_PTR(-ENOMEM);

	ptr = skb->data;
	cmd = ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_NLO_CONFIG_CMD, sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(pno->vdev_id);
	cmd->flags = cpu_to_le32(WMI_NLO_CONFIG_START | WMI_NLO_CONFIG_SSID_HIDE_EN);

	/* current FW does not support min-max range for dwell time */
	cmd->active_dwell_time = cpu_to_le32(pno->active_max_time);
	cmd->passive_dwell_time = cpu_to_le32(pno->passive_max_time);

	if (pno->do_passive_scan)
		cmd->flags |= cpu_to_le32(WMI_NLO_CONFIG_SCAN_PASSIVE);

	cmd->fast_scan_period = cpu_to_le32(pno->fast_scan_period);
	cmd->slow_scan_period = cpu_to_le32(pno->slow_scan_period);
	cmd->fast_scan_max_cycles = cpu_to_le32(pno->fast_scan_max_cycles);
	cmd->delay_start_time = cpu_to_le32(pno->delay_start_time);

	if (pno->enable_pno_scan_randomization) {
		cmd->flags |= cpu_to_le32(WMI_NLO_CONFIG_SPOOFED_MAC_IN_PROBE_REQ |
					  WMI_NLO_CONFIG_RANDOM_SEQ_NO_IN_PROBE_REQ);
		ether_addr_copy(cmd->mac_addr.addr, pno->mac_addr);
		ether_addr_copy(cmd->mac_mask.addr, pno->mac_addr_mask);
	}

	ptr += sizeof(*cmd);

	/* nlo_configured_params(nlo_list) */
	cmd->no_of_ssids = cpu_to_le32(pno->uc_networks_count);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, nlo_list_len);

	ptr += sizeof(*tlv);
	nlo_list = ptr;
	for (i = 0; i < pno->uc_networks_count; i++) {
		tlv = (struct wmi_tlv *)(&nlo_list[i].tlv_header);
		tlv->header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ARRAY_BYTE,
						     sizeof(*nlo_list));

		nlo_list[i].ssid.valid = cpu_to_le32(1);
		nlo_list[i].ssid.ssid.ssid_len =
			cpu_to_le32(pno->a_networks[i].ssid.ssid_len);
		memcpy(nlo_list[i].ssid.ssid.ssid,
		       pno->a_networks[i].ssid.ssid,
		       le32_to_cpu(nlo_list[i].ssid.ssid.ssid_len));

		if (pno->a_networks[i].rssi_threshold &&
		    pno->a_networks[i].rssi_threshold > -300) {
			nlo_list[i].rssi_cond.valid = cpu_to_le32(1);
			nlo_list[i].rssi_cond.rssi =
					cpu_to_le32(pno->a_networks[i].rssi_threshold);
		}

		nlo_list[i].bcast_nw_type.valid = cpu_to_le32(1);
		nlo_list[i].bcast_nw_type.bcast_nw_type =
					cpu_to_le32(pno->a_networks[i].bcast_nw_type);
	}

	ptr += nlo_list_len;
	cmd->num_of_channels = cpu_to_le32(pno->a_networks[0].channel_count);
	tlv = ptr;
	tlv->header =  ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, channel_list_len);
	ptr += sizeof(*tlv);
	channel_list = ptr;

	for (i = 0; i < pno->a_networks[0].channel_count; i++)
		channel_list[i] = cpu_to_le32(pno->a_networks[0].channels[i]);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi tlv start pno config vdev_id %d\n",
		   vdev_id);

	return skb;
}

static struct sk_buff *ath12k_wmi_op_gen_config_pno_stop(struct ath12k *ar,
							 u32 vdev_id)
{
	struct wmi_wow_nlo_config_cmd *cmd;
	struct sk_buff *skb;
	size_t len;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return ERR_PTR(-ENOMEM);

	cmd = (struct wmi_wow_nlo_config_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_NLO_CONFIG_CMD, len);

	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->flags = cpu_to_le32(WMI_NLO_CONFIG_STOP);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi tlv stop pno config vdev_id %d\n", vdev_id);
	return skb;
}

int ath12k_wmi_wow_config_pno(struct ath12k *ar, u32 vdev_id,
			      struct wmi_pno_scan_req_arg  *pno_scan)
{
	struct sk_buff *skb;

	if (pno_scan->enable)
		skb = ath12k_wmi_op_gen_config_pno_start(ar, vdev_id, pno_scan);
	else
		skb = ath12k_wmi_op_gen_config_pno_stop(ar, vdev_id);

	if (IS_ERR_OR_NULL(skb))
		return -ENOMEM;

	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_NETWORK_LIST_OFFLOAD_CONFIG_CMDID);
}

static void ath12k_wmi_fill_ns_offload(struct ath12k *ar,
				       struct wmi_arp_ns_offload_arg *offload,
				       void **ptr,
				       bool enable,
				       bool ext)
{
	struct wmi_ns_offload_params *ns;
	struct wmi_tlv *tlv;
	void *buf_ptr = *ptr;
	u32 ns_cnt, ns_ext_tuples;
	int i, max_offloads;

	ns_cnt = offload->ipv6_count;

	tlv  = buf_ptr;

	if (ext) {
		ns_ext_tuples = offload->ipv6_count - WMI_MAX_NS_OFFLOADS;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 ns_ext_tuples * sizeof(*ns));
		i = WMI_MAX_NS_OFFLOADS;
		max_offloads = offload->ipv6_count;
	} else {
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 WMI_MAX_NS_OFFLOADS * sizeof(*ns));
		i = 0;
		max_offloads = WMI_MAX_NS_OFFLOADS;
	}

	buf_ptr += sizeof(*tlv);

	for (; i < max_offloads; i++) {
		ns = buf_ptr;
		ns->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_NS_OFFLOAD_TUPLE,
							sizeof(*ns));

		if (enable) {
			if (i < ns_cnt)
				ns->flags |= cpu_to_le32(WMI_NSOL_FLAGS_VALID);

			memcpy(ns->target_ipaddr[0], offload->ipv6_addr[i], 16);
			memcpy(ns->solicitation_ipaddr, offload->self_ipv6_addr[i], 16);

			if (offload->ipv6_type[i])
				ns->flags |= cpu_to_le32(WMI_NSOL_FLAGS_IS_IPV6_ANYCAST);

			memcpy(ns->target_mac.addr, offload->mac_addr, ETH_ALEN);

			if (!is_zero_ether_addr(ns->target_mac.addr))
				ns->flags |= cpu_to_le32(WMI_NSOL_FLAGS_MAC_VALID);

			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "wmi index %d ns_solicited %pI6 target %pI6",
				   i, ns->solicitation_ipaddr,
				   ns->target_ipaddr[0]);
		}

		buf_ptr += sizeof(*ns);
	}

	*ptr = buf_ptr;
}

static void ath12k_wmi_fill_arp_offload(struct ath12k *ar,
					struct wmi_arp_ns_offload_arg *offload,
					void **ptr,
					bool enable)
{
	struct wmi_arp_offload_params *arp;
	struct wmi_tlv *tlv;
	void *buf_ptr = *ptr;
	int i;

	/* fill arp tuple */
	tlv = buf_ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 WMI_MAX_ARP_OFFLOADS * sizeof(*arp));
	buf_ptr += sizeof(*tlv);

	for (i = 0; i < WMI_MAX_ARP_OFFLOADS; i++) {
		arp = buf_ptr;
		arp->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ARP_OFFLOAD_TUPLE,
							 sizeof(*arp));

		if (enable && i < offload->ipv4_count) {
			/* Copy the target ip addr and flags */
			arp->flags = cpu_to_le32(WMI_ARPOL_FLAGS_VALID);
			memcpy(arp->target_ipaddr, offload->ipv4_addr[i], 4);

			ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi arp offload address %pI4",
				   arp->target_ipaddr);
		}

		buf_ptr += sizeof(*arp);
	}

	*ptr = buf_ptr;
}

int ath12k_wmi_arp_ns_offload(struct ath12k *ar,
			      struct ath12k_link_vif *arvif,
			      struct wmi_arp_ns_offload_arg *offload,
			      bool enable)
{
	struct wmi_set_arp_ns_offload_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *buf_ptr;
	size_t len;
	u8 ns_cnt, ns_ext_tuples = 0;

	ns_cnt = offload->ipv6_count;

	len = sizeof(*cmd) +
	      sizeof(*tlv) +
	      WMI_MAX_NS_OFFLOADS * sizeof(struct wmi_ns_offload_params) +
	      sizeof(*tlv) +
	      WMI_MAX_ARP_OFFLOADS * sizeof(struct wmi_arp_offload_params);

	if (ns_cnt > WMI_MAX_NS_OFFLOADS) {
		ns_ext_tuples = ns_cnt - WMI_MAX_NS_OFFLOADS;
		len += sizeof(*tlv) +
		       ns_ext_tuples * sizeof(struct wmi_ns_offload_params);
	}

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	buf_ptr = skb->data;
	cmd = buf_ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SET_ARP_NS_OFFLOAD_CMD,
						 sizeof(*cmd));
	cmd->flags = cpu_to_le32(0);
	cmd->vdev_id = cpu_to_le32(arvif->vdev_id);
	cmd->num_ns_ext_tuples = cpu_to_le32(ns_ext_tuples);

	buf_ptr += sizeof(*cmd);

	ath12k_wmi_fill_ns_offload(ar, offload, &buf_ptr, enable, 0);
	ath12k_wmi_fill_arp_offload(ar, offload, &buf_ptr, enable);

	if (ns_ext_tuples)
		ath12k_wmi_fill_ns_offload(ar, offload, &buf_ptr, enable, 1);

	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_SET_ARP_NS_OFFLOAD_CMDID);
}

int ath12k_wmi_gtk_rekey_offload(struct ath12k *ar,
				 struct ath12k_link_vif *arvif, bool enable)
{
	struct ath12k_rekey_data *rekey_data = &arvif->rekey_data;
	struct wmi_gtk_rekey_offload_cmd *cmd;
	struct sk_buff *skb;
	__le64 replay_ctr;
	int len;

	len = sizeof(*cmd);
	skb =  ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_gtk_rekey_offload_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_GTK_OFFLOAD_CMD, sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arvif->vdev_id);

	if (enable) {
		cmd->flags = cpu_to_le32(GTK_OFFLOAD_ENABLE_OPCODE);

		/* the length in rekey_data and cmd is equal */
		memcpy(cmd->kck, rekey_data->kck, sizeof(cmd->kck));
		memcpy(cmd->kek, rekey_data->kek, sizeof(cmd->kek));

		replay_ctr = cpu_to_le64(rekey_data->replay_ctr);
		memcpy(cmd->replay_ctr, &replay_ctr,
		       sizeof(replay_ctr));
	} else {
		cmd->flags = cpu_to_le32(GTK_OFFLOAD_DISABLE_OPCODE);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "offload gtk rekey vdev: %d %d\n",
		   arvif->vdev_id, enable);
	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_GTK_OFFLOAD_CMDID);
}

int ath12k_wmi_gtk_rekey_getinfo(struct ath12k *ar,
				 struct ath12k_link_vif *arvif)
{
	struct wmi_gtk_rekey_offload_cmd *cmd;
	struct sk_buff *skb;
	int len;

	len = sizeof(*cmd);
	skb =  ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_gtk_rekey_offload_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_GTK_OFFLOAD_CMD, sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arvif->vdev_id);
	cmd->flags = cpu_to_le32(GTK_OFFLOAD_REQUEST_STATUS_OPCODE);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "get gtk rekey vdev_id: %d\n",
		   arvif->vdev_id);
	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_GTK_OFFLOAD_CMDID);
}

int ath12k_wmi_sta_keepalive(struct ath12k *ar,
			     const struct wmi_sta_keepalive_arg *arg)
{
	struct wmi_sta_keepalive_arp_resp_params *arp;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_sta_keepalive_cmd *cmd;
	struct sk_buff *skb;
	size_t len;

	len = sizeof(*cmd) + sizeof(*arp);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_sta_keepalive_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_STA_KEEPALIVE_CMD, sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->enabled = cpu_to_le32(arg->enabled);
	cmd->interval = cpu_to_le32(arg->interval);
	cmd->method = cpu_to_le32(arg->method);

	arp = (struct wmi_sta_keepalive_arp_resp_params *)(cmd + 1);
	arp->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_STA_KEEPALVE_ARP_RESPONSE,
						 sizeof(*arp));
	if (arg->method == WMI_STA_KEEPALIVE_METHOD_UNSOLICITED_ARP_RESPONSE ||
	    arg->method == WMI_STA_KEEPALIVE_METHOD_GRATUITOUS_ARP_REQUEST) {
		arp->src_ip4_addr = cpu_to_le32(arg->src_ip4_addr);
		arp->dest_ip4_addr = cpu_to_le32(arg->dest_ip4_addr);
		ether_addr_copy(arp->dest_mac_addr.addr, arg->dest_mac_addr);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi sta keepalive vdev %d enabled %d method %d interval %d\n",
		   arg->vdev_id, arg->enabled, arg->method, arg->interval);

	return ath12k_wmi_cmd_send(wmi, skb, WMI_STA_KEEPALIVE_CMDID);
}

int ath12k_wmi_mlo_setup(struct ath12k *ar, struct wmi_mlo_setup_arg *mlo_params)
{
	struct wmi_mlo_setup_cmd *cmd;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	u32 *partner_links, num_links;
	int i, ret, buf_len, arg_len;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;

	num_links = mlo_params->num_partner_links;
	arg_len = num_links * sizeof(u32);
	buf_len = sizeof(*cmd) + TLV_HDR_SIZE + arg_len;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, buf_len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_mlo_setup_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_SETUP_CMD,
						 sizeof(*cmd));
	cmd->mld_group_id = mlo_params->group_id;
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	cmd->max_num_ml_peers = cpu_to_le32(mlo_params->max_ml_peer_supported);
	ptr = skb->data + sizeof(*cmd);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, arg_len);
	ptr += TLV_HDR_SIZE;

	partner_links = ptr;
	for (i = 0; i < num_links; i++)
		partner_links[i] = mlo_params->partner_link_id[i];

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_MLO_SETUP_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit WMI_MLO_SETUP_CMDID command: %d\n",
			    ret);
		dev_kfree_skb(skb);
		return ret;
	}

	return 0;
}

int ath12k_wmi_mlo_ready(struct ath12k *ar)
{
	struct wmi_mlo_ready_cmd *cmd;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_mlo_ready_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_READY_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_MLO_READY_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit WMI_MLO_READY_CMDID command: %d\n",
			    ret);
		dev_kfree_skb(skb);
		return ret;
	}

	return 0;
}

int ath12k_wmi_mlo_teardown(struct ath12k *ar, bool umac_reset,
			    u32 reason_code, bool erp_standby_mode)
{
	struct wmi_mlo_teardown_cmd *cmd;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_mlo_teardown_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_TEARDOWN_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	cmd->umac_reset = cpu_to_le32(umac_reset);
	cmd->reason_code = cpu_to_le32(reason_code);
	cmd->erp_standby_mode = cpu_to_le32(erp_standby_mode);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_MLO_TEARDOWN_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit WMI MLO teardown command: %d\n",
			    ret);
		dev_kfree_skb(skb);
		return ret;
	}

	return 0;
}

int ath12k_wmi_mlo_reconfig_link_removal(struct ath12k *ar, u32 vdev_id,
					 const u8 *reconfig_ml_ie,
					 size_t reconfig_ml_ie_len)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_mlo_link_removal_cmd_fixed_param *cmd;
	struct wmi_tlv *reconfig_ie_tlv;
	struct sk_buff *skb;
	int ret, len;
	u32 reconfig_ie_len_aligned = roundup(reconfig_ml_ie_len,
					      sizeof(u32));
	void *ptr;

	len = TLV_HDR_SIZE + sizeof(*cmd) + reconfig_ie_len_aligned;
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_mlo_link_removal_cmd_fixed_param *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_LINK_REMOVAL_CMD_FIXED_PARAM,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->reconfig_ml_ie_num_bytes_valid = cpu_to_le32(reconfig_ml_ie_len);

	ptr = skb->data + sizeof(*cmd);

	reconfig_ie_tlv = ptr;
	reconfig_ie_tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE,
						     reconfig_ie_len_aligned);
	memcpy(reconfig_ie_tlv->value, reconfig_ml_ie, reconfig_ml_ie_len);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_MLO_LINK_REMOVAL_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_MLO_LINK_REMOVAL_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

bool ath12k_wmi_is_umac_migration_supported(struct ath12k_base *ab)
{
	struct ath12k_wmi_base *wmi_ab = &ab->wmi_ab;

	return test_bit(WMI_SERVICE_UMAC_MIGRATION_SUPPORT, wmi_ab->svc_map);
}

int ath12k_wmi_mlo_send_ptqm_migrate_cmd(struct ath12k_link_vif *arvif,
				         struct list_head *peer_migr_list,
				         u16 num_peers)
{
	struct ath12k_mac_pri_link_migr_peer_node *peer_node, *tmp_peer;
	struct wmi_mlo_pri_link_peer_mig_fixed_param *cmd;
	struct wmi_mlo_new_pri_link_peer_info *peer_info;
	u16 max_entry_per_cmd = 0, max_entry_cnt = 0;
	struct ath12k *ar = arvif->ar;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int ret = -EINVAL, len, i;

	max_entry_per_cmd = (wmi->wmi_ab->max_msg_len[ar->pdev_idx] -
			     sizeof(*cmd) - TLV_HDR_SIZE) /
			    sizeof(*peer_info);

	reinit_completion(&arvif->wmi_migration_event_resp);

	while (num_peers > 0) {
		len = sizeof(*cmd) + TLV_HDR_SIZE;
		max_entry_cnt = min(max_entry_per_cmd, num_peers);

		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "Setting max entry limit for migration as %u\n",
			   max_entry_cnt);

		num_peers -= max_entry_cnt;
		len += sizeof(*peer_info) * max_entry_cnt;

		skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
		if (!skb)
			return -ENOMEM;

		cmd = (struct wmi_mlo_pri_link_peer_mig_fixed_param *)skb->data;
		cmd->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_PRIMARY_LINK_PEER_MIGRATION_FIXED_PARAM,
					       (sizeof(*cmd)));
		cmd->vdev_id = cpu_to_le32(arvif->vdev_id);

		ptr = skb->data + sizeof(*cmd);

		len = sizeof(*peer_info) * max_entry_cnt;
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
		ptr += TLV_HDR_SIZE;

		i = 0;
		list_for_each_entry(peer_node, peer_migr_list, list) {
			peer_info = ptr;
			len = sizeof(*peer_info);
			peer_info->tlv_header =
				ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_NEW_PRIMARY_LINK_PEER_INFO,
						       len);
			peer_info->new_link_info =
				le32_encode_bits(peer_node->ml_peer_id,
						 WMI_MLO_PRIMARY_LINK_PEER_MIGRATION_ML_PEER_ID) |
				le32_encode_bits(peer_node->hw_link_id,
						 WMI_MLO_PRIMARY_LINK_PEER_MIGRATION_HW_LINK_ID);

			ath12k_dbg(ab, ATH12K_DBG_WMI,
				   "ml_peer_id %u new hw link id %d\n",
				   peer_node->ml_peer_id, peer_node->hw_link_id);

			ptr += len;

			if (++i == max_entry_cnt)
				break;
		}

		ret = ath12k_wmi_cmd_send(wmi, skb, WMI_MLO_PRIMARY_LINK_PEER_MIGRATION_CMDID);
		if (ret) {
			ath12k_warn(ar->ab, "failed to send WMI_MLO_PRIMARY_LINK_PEER_MIGRATION cmd\n");
			dev_kfree_skb(skb);
			return ret;
		}

		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "WMI PTQM command sent. vdev_id %d num_peers %d\n",
			   arvif->vdev_id, max_entry_cnt);

		/* now that command is sent, remove this from the list so
		 * that next iteration does not consider it again. Removing
		 * from list also indicates that command was sent successfully
		 * to firmware.
		 */
		i = 0;
		list_for_each_entry_safe(peer_node, tmp_peer, peer_migr_list, list) {
			list_move_tail(&peer_node->list, &arvif->peer_migrate_list);
			if (++i == max_entry_cnt)
				break;
		}

		/* cancel the worker if already there is a worker scheduled and
		 * schedule a new one. This basically resets the timer
		 */
		cancel_work_sync(&arvif->wmi_migration_cmd_work);
		queue_work(ar->ab->workqueue, &arvif->wmi_migration_cmd_work);
		arvif->is_umac_migration_in_progress = true;
	}

	return ret;
}

int ath12k_wmi_pdev_ap_ps_cmd_send(struct ath12k *ar, u8 pdev_id,
				   u32 param_value)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_ap_ps_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_ap_ps_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PDEV_GREEN_AP_PS_ENABLE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->pdev_id = pdev_id;
	cmd->param_value = param_value;

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_GREEN_AP_PS_ENABLE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send ap ps enable/disable cmd %d\n",ret);
		dev_kfree_skb(skb);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi pdev ap ps set pdev id %d value %d\n",
		   pdev_id, param_value);

	return ret;
}

bool ath12k_wmi_is_mvr_supported(struct ath12k_base *ab)
{
	struct ath12k_wmi_base *wmi_ab = &ab->wmi_ab;

	return test_bit(WMI_TLV_SERVICE_MULTIPLE_VDEV_RESTART,
			 wmi_ab->svc_map) &&
		test_bit(WMI_TLV_SERVICE_MULTIPLE_VDEV_RESTART_RESPONSE_SUPPORT,
			 wmi_ab->svc_map);
}

int ath12k_wmi_pdev_multiple_vdev_restart(struct ath12k *ar,
					  struct wmi_pdev_multiple_vdev_restart_req_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = ar->ab;
	struct wmi_pdev_multiple_vdev_restart_request_cmd *cmd;
	struct ath12k_wmi_channel_params *chan;
	struct wmi_tlv *tlv;
	struct ath12k_wmi_channel_params *chan_device;
	u32 num_vdev_ids;
	__le32 *vdev_ids;
	size_t vdev_ids_len;
	struct sk_buff *skb;
	void *ptr;
	int ret, len, i;
	bool device_params_present = false;

	if (ab->ag && ab->ag->num_devices >= ATH12K_MIN_NUM_DEVICES_NLINK) {
		if (WARN_ON(arg->vdev_ids.id_len > TARGET_NUM_VDEVS + TARGET_NUM_BRIDGE_VDEVS))
			return -EINVAL;
	} else {
		if (WARN_ON(arg->vdev_ids.id_len > TARGET_NUM_VDEVS))
			return -EINVAL;
	}

	num_vdev_ids = arg->vdev_ids.id_len;
	vdev_ids_len = num_vdev_ids * sizeof(__le32);

	len = sizeof(*cmd) + TLV_HDR_SIZE + vdev_ids_len +
	      sizeof(*chan) + TLV_HDR_SIZE + TLV_HDR_SIZE +
	      TLV_HDR_SIZE;

	device_params_present = ath12k_wmi_check_device_present(arg->width_device,
								arg->center_freq_device,
								arg->vdev_start_arg.band_center_freq1);

	if (device_params_present)
		len += TLV_HDR_SIZE + sizeof(*chan_device);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_multiple_vdev_restart_request_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_MULTIPLE_VDEV_RESTART_REQUEST_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	cmd->num_vdevs = cpu_to_le32(arg->vdev_ids.id_len);

	cmd->puncture_20mhz_bitmap = cpu_to_le32(arg->ru_punct_bitmap);

	cmd->flags = cpu_to_le32(WMI_MVR_RESPONSE_SUPPORT_EXPECTED);

	ptr = skb->data + sizeof(*cmd);
	tlv = (struct wmi_tlv *)ptr;

	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, vdev_ids_len);
	vdev_ids = (__le32 *)tlv->value;

	for (i = 0; i < num_vdev_ids; i++)
		vdev_ids[i] = cpu_to_le32(arg->vdev_ids.id[i]);

	ptr += TLV_HDR_SIZE + vdev_ids_len;
	chan = (struct ath12k_wmi_channel_params *)ptr;

	ath12k_wmi_put_wmi_channel(chan, &arg->vdev_start_arg);

	chan->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_CHANNEL, sizeof(*chan));
	ptr += sizeof(*chan);

	/* Zero length TLVs for phymode_list, preferred_tx_stream_list
	 * and preferred_rx_stream_list which are mandatory if any of
	 * the following TLVs are to be sent to target.
	 */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, 0);
	ptr += sizeof(*tlv);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, 0);
	ptr += sizeof(*tlv);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, 0);
	ptr += sizeof(*tlv);

	if (test_bit(WMI_TLV_SERVICE_SW_PROG_DFS_SUPPORT, ar->ab->wmi_ab.svc_map) &&
	    device_params_present) {
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 sizeof(*chan_device));
		ptr += TLV_HDR_SIZE;
		chan_device = ptr;
		ath12k_wmi_set_wmi_channel_device(chan_device, &arg->vdev_start_arg,
						  arg->center_freq_device,
						  arg->width_device);
		ptr += sizeof(*chan_device);
	}

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_MULTIPLE_VDEV_RESTART_REQUEST_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "wmi failed to send mvr command (%d)\n",
			    ret);
		dev_kfree_skb(skb);
		return ret;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi mvr cmd sent num_vdevs %d freq %d\n",
		   num_vdev_ids, arg->vdev_start_arg.freq);

	return ret;
}

static void ath12k_wmi_put_peer_list(struct ath12k_base *ab,
				     struct wmi_chan_width_peer_list *peer_list,
				     struct wmi_chan_width_peer_arg *peer_arg,
				     u32 num_peers, int start_idx)
{
	struct wmi_chan_width_peer_list *itr;
	struct wmi_chan_width_peer_arg *arg_itr;
	int i;
	u32 host_chan_width;

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "wmi peer channel width switch command peer list\n");

	for (i = 0; i < num_peers; i++) {
		itr = &peer_list[i];
		arg_itr = &peer_arg[start_idx + i];

		itr->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_CHAN_WIDTH_PEER_LIST,
							 sizeof(*itr));
		ether_addr_copy(itr->mac_addr.addr, arg_itr->mac_addr.addr);

		/* Convert ieee80211_sta_rx_bw to wmi_host_chan_width */
		switch (arg_itr->chan_width) {
		case IEEE80211_STA_RX_BW_20:
			host_chan_width = WMI_HOST_CHAN_WIDTH_20;
		break;
		case IEEE80211_STA_RX_BW_40:
			host_chan_width = WMI_HOST_CHAN_WIDTH_40;
			break;
		case IEEE80211_STA_RX_BW_80:
			host_chan_width = WMI_HOST_CHAN_WIDTH_80;
		break;
		case IEEE80211_STA_RX_BW_160:
			host_chan_width = WMI_HOST_CHAN_WIDTH_160;
		break;
		case IEEE80211_STA_RX_BW_320:
			host_chan_width = WMI_HOST_CHAN_WIDTH_320;
		break;
		default:
			ath12k_warn(ab, "invalid bw %d switching back to 20 MHz\n",
				    arg_itr->chan_width);
			host_chan_width = WMI_HOST_CHAN_WIDTH_20;
			break;
		}

		itr->chan_width = cpu_to_le32(host_chan_width);
		itr->puncture_20mhz_bitmap = cpu_to_le32(arg_itr->puncture_20mhz_bitmap);

		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "   (%u) width %u addr %pM punct_bitmap 0x%x host chan_width: %d\n",
			   i + 1, arg_itr->chan_width, arg_itr->mac_addr.addr,
			   arg_itr->puncture_20mhz_bitmap, host_chan_width);
	}
}

static int ath12k_wmi_peer_chan_width_switch(struct ath12k *ar,
					     struct wmi_peer_chan_width_switch_arg *arg)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_chan_width_switch_req_cmd *cmd;
	struct wmi_chan_width_peer_list *peer_list;
	struct wmi_tlv *tlv;
	u32 num_peers;
	size_t peer_list_len;
	struct sk_buff *skb;
	void *ptr;
	int ret, len;

	num_peers = arg->num_peers;

	if (WARN_ON(num_peers > ab->chwidth_num_peer_caps))
		return -EINVAL;

	peer_list_len = num_peers * sizeof(*peer_list);

	len = sizeof(*cmd) + TLV_HDR_SIZE + peer_list_len;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_chan_width_switch_req_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_CHAN_WIDTH_SWITCH_CMD,
						 sizeof(*cmd));
	cmd->num_peers = cpu_to_le32(num_peers);
	cmd->vdev_var = cpu_to_le32(arg->vdev_var);

	ptr = skb->data + sizeof(*cmd);
	tlv = (struct wmi_tlv *)ptr;

	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, peer_list_len);
	peer_list = (struct wmi_chan_width_peer_list *)tlv->value;

	ath12k_wmi_put_peer_list(ab, peer_list, arg->peer_arg, num_peers,
				 arg->start_idx);

	ptr += peer_list_len;

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_CHAN_WIDTH_SWITCH_CMDID);
	if (ret) {
		ath12k_warn(ab, "wmi failed to send peer chan width switch command (%d)\n",
			    ret);
		dev_kfree_skb(skb);
		return ret;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "wmi peer chan width switch cmd sent num_peers %d \n",
		   num_peers);

	return ret;
}

void ath12k_wmi_set_peers_chan_width(struct ath12k_link_vif *arvif,
				     struct wmi_chan_width_peer_arg *peer_arg,
				     int num, u8 start_idx)
{
	struct ath12k *ar = arvif->ar;
	struct wmi_chan_width_peer_arg *arg;
	int i, err;

	for (i = 0; i < num; i++) {
		arg = &peer_arg[start_idx + i];

		err = ath12k_wmi_set_peer_param(ar, arg->mac_addr.addr,
						arvif->vdev_id, WMI_PEER_CHWIDTH,
						arg->chan_width);
		if (err) {
			ath12k_warn(ar->ab, "failed to update STA %pM peer bw %d: %d\n",
				    arg->mac_addr.addr, arg->chan_width, err);
			continue;
		}
	}
}

void ath12k_wmi_peer_chan_width_switch_work(struct wiphy *wiphy, struct wiphy_work *work)
{
	struct ath12k_link_vif *arvif = container_of(work, struct ath12k_link_vif,
						     peer_ch_width_switch_work);
	struct ath12k *ar = arvif->ar;
	struct ath12k_peer_ch_width_switch_data *data;
	struct wmi_peer_chan_width_switch_arg arg;
	unsigned long time_left = 0;
	int count_left, curr_count, max_count_per_cmd = ar->ab->chwidth_num_peer_caps;
	int cmd_num = 0, ret;

	/* possible that the worker got scheduled after complete was triggered. In
	 * this case we don't wait for timeout
	 */
	if (arvif->peer_ch_width_switch_data->count == arvif->num_stations)
		goto send_cmd;

	time_left = wait_for_completion_timeout(&arvif->peer_ch_width_switch_send,
						ATH12K_PEER_CH_WIDTH_SWITCH_TIMEOUT_HZ);
	if (time_left == 0) {
		/* Even though timeout occured, we would send the command for the peers
		 * for which we received sta rc update event, hence not returning
		 */
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "timed out waiting for all peers in peer channel width switch\n");
	}

send_cmd:

	data = arvif->peer_ch_width_switch_data;

	spin_lock_bh(&ar->data_lock);
	arg.vdev_var = arvif->vdev_id;
	spin_unlock_bh(&ar->data_lock);

	arg.vdev_var |= ATH12K_PEER_VALID_VDEV_ID | ATH12K_PEER_PUNCT_BITMAP_VALID;
	arg.peer_arg = data->peer_arg;

	count_left = data->count;

	while (count_left > 0) {
		if (count_left <= max_count_per_cmd)
			curr_count = count_left;
		else
			curr_count = max_count_per_cmd;

		count_left -= curr_count;

		cmd_num++;

		arg.num_peers = curr_count;
		arg.start_idx = (cmd_num - 1) * max_count_per_cmd;

		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "wmi peer channel width switch command num %u\n",
			   cmd_num);

		ret = ath12k_wmi_peer_chan_width_switch(ar, &arg);
		if (ret) {
			/* fallback */
			ath12k_wmi_set_peers_chan_width(arvif, arg.peer_arg,
							arg.num_peers,
							arg.start_idx);
		}
	}

	kfree(arvif->peer_ch_width_switch_data);
	arvif->peer_ch_width_switch_data = NULL;
}

int ath12k_wmi_send_wsi_stats_info(struct ath12k *ar,
				   struct ath12k_wmi_wsi_stats_info_param *param)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_wsi_stats_info_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_wsi_stats_info_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_WSI_STATS_INFO_CMD,
						 (sizeof(*cmd)));
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	cmd->wsi_ingress_load_info = cpu_to_le32(param->wsi_ingress_load_info);
	cmd->wsi_egress_load_info = cpu_to_le32(param->wsi_egress_load_info);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI pdev wsi stats info pdev_id %d ingress_load_info %d egress_load_info %d\n",
		   cmd->pdev_id, cmd->wsi_ingress_load_info, cmd->wsi_egress_load_info);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_WSI_STATS_INFO_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PDEV_WSI_STATS_INFO_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_set_neighbor_rx_cmd(struct ath12k *ar,
					struct ath12k_set_neighbor_rx_params *param)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_set_neighbor_rx_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_set_neighbor_rx_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_FILTER_NRP_CONFIG_CMD,
						(sizeof(*cmd)));
	cmd->vdev_id = cpu_to_le32(param->vdev_id);
	cmd->action = cpu_to_le32(param->action);
	cmd->type = WMI_FILTER_NRP_TYPE_STA_MACADDR;
	cmd->bssid_idx = cpu_to_le32(1);
	ether_addr_copy(cmd->macaddr.addr, param->nrp_addr);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI set nrp config vdev_id %d action %d nrp mac %pM\n",
		   cmd->vdev_id, cmd->action, param->nrp_addr);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_FILTER_NEIGHBOR_RX_PACKETS_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send VDEV_FILTER_NEIGHBOR_RX_PACKETS_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static void *
ath12k_populate_link_control_tlv(void *buf_ptr,
				 struct ath12k_wmi_ttlm_peer_params *params)
{
	struct wmi_mlo_peer_link_control_param *link_control;
	u8 pref_link = 0;
	u8 latency = 0;
	u8 links = 0;
	struct wmi_tlv *tlv = buf_ptr;

	/* The Link Preference TLV is planned to be deprecated,
	 * so the TLV is going to be exlcuded by default.
	 */
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
	buf_ptr += TLV_HDR_SIZE;

	tlv = buf_ptr;
	if (params->preferred_links.num_pref_links) {
		tlv->header =
			ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					   sizeof(struct wmi_mlo_peer_link_control_param));

		buf_ptr += TLV_HDR_SIZE;

		link_control = (struct wmi_mlo_peer_link_control_param *)buf_ptr;
		link_control->tlv_header =
			ath12k_wmi_tlv_hdr(WMI_TAG_MLO_PEER_LINK_CONTROL_PARAM,
					   sizeof(*link_control) - TLV_HDR_SIZE);
		link_control->num_links =
			cpu_to_le32(params->preferred_links.num_pref_links);
		links = cpu_to_le32(params->preferred_links.num_pref_links);

		for (pref_link = 0; pref_link < links; pref_link++) {
			link_control->link_priority_order[pref_link] =
			cpu_to_le32(params->preferred_links.preferred_link_order[pref_link]);
		}
		link_control->flags =
			cpu_to_le32(params->preferred_links.link_control_flags);
		link_control->tx_link_tuple_bitmap =
			cpu_to_le32(params->preferred_links.tlt_characterization_params);

		for (latency = 0; latency < WLAN_MAX_AC; latency++) {
			link_control->max_timeout_ms[latency] =
				cpu_to_le32(params->preferred_links.timeout[latency]);
		}
		buf_ptr += sizeof(struct wmi_mlo_peer_link_control_param);
	} else {
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
		buf_ptr += TLV_HDR_SIZE;
	}
	return buf_ptr;
}

static void*
ath12k_wmi_fill_tid_to_link_map_info(struct ath12k_wmi_ttlm_peer_params *params,
				     struct wmi_tid_to_link_map *ttlm,
				     void *ptr)
{
	struct ath12k_wmi_host_ttlm_of_tids *ttlm_of_tids;
	int dir, tid;

	for (dir = 0; dir < params->num_dir; dir++) {
		ttlm_of_tids = &params->ttlm_info[dir];
		for (tid = 0; tid < TTLM_MAX_NUM_TIDS; tid++) {
			ttlm = (struct wmi_tid_to_link_map *)ptr;
			ttlm->tlv_header =
				ath12k_wmi_tlv_hdr(WMI_TAG_TID_TO_LINK_MAP,
						   sizeof(*ttlm) - TLV_HDR_SIZE);
			/* populate tid number */
			ttlm->tid_to_link_map_info |=
				le32_encode_bits(tid,
						 WMI_TTLM_TID_MASK);

			/* populate direction */
			ttlm->tid_to_link_map_info |=
				le32_encode_bits(ttlm_of_tids->direction,
						 WMI_TTLM_DIR_MASK);

			/* populate default link mapping value */
			ttlm->tid_to_link_map_info |=
				le32_encode_bits(ttlm_of_tids->default_link_mapping,
						 WMI_TTLM_DEFAULT_MAPPING_MASK);

			/* populate ttlm provisioned links for the
			 * corressponding tid number
			 */
			ttlm->tid_to_link_map_info |=
				le32_encode_bits(ttlm_of_tids->ttlm_provisioned_links[tid],
						 WMI_TTLM_LINK_MAPPING_MASK);

			ptr += sizeof(*ttlm);
		}
	}
	return ptr;
}

int
ath12k_wmi_send_mlo_peer_tid_to_link_map_cmd(struct ath12k *ar,
					     struct ath12k_wmi_ttlm_peer_params *params,
					     bool ttlm_info)
{
	struct wmi_peer_tid_to_link_map_fixed_param *cmd;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_tid_to_link_map *ttlm;
	int ret, buf_len;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;

	/* find the buf len pref link */
	buf_len = sizeof(*cmd);

	buf_len += TLV_HDR_SIZE;
	if (ttlm_info)
		buf_len += params->num_dir * TTLM_MAX_NUM_TIDS * sizeof(*ttlm);

	/* Update the length for preferred link tlv.
	 * The link preference tlv is planned to be deprecated, so the tlv
	 * is going to be excluded by default
	 */
	buf_len += TLV_HDR_SIZE;

	/* update the length for link control tlv */
	buf_len += TLV_HDR_SIZE;
	if (params->preferred_links.num_pref_links)
		buf_len += sizeof(struct wmi_mlo_peer_link_control_param);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, buf_len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_tid_to_link_map_fixed_param *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_TID_TO_LINK_MAP_FIXED_PARAM,
				       sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(params->pdev_id);
	ether_addr_copy(cmd->link_macaddr.addr, params->peer_macaddr);

	ptr = skb->data + sizeof(*cmd);
	tlv = ptr;

	if (ttlm_info) {
		buf_len = params->num_dir * TTLM_MAX_NUM_TIDS * sizeof(*ttlm);
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 buf_len);
		ptr += TLV_HDR_SIZE;

		ptr = ath12k_wmi_fill_tid_to_link_map_info(params, ttlm, ptr);
	} else {
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
		ptr += TLV_HDR_SIZE;
	}

	ptr = ath12k_populate_link_control_tlv(ptr, params);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_MLO_PEER_TID_TO_LINK_MAP_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_MLO_PEER_TID_TO_LINK_MAP_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static void
ath12k_wmi_ttlm_update_ie_info(struct ath12k *ar,
			       struct wmi_mlo_ap_vdev_tid_to_link_map_ie_info *ie_info,
			       struct ath12k_wmi_mlo_ttlm_ie *params)
{
	ie_info->tid_to_link_map_ctrl |=
		le32_encode_bits(params->ttlm.direction,
				 WMI_ADV_TTLM_CTRL_DIRECTION_MASK);
	ie_info->tid_to_link_map_ctrl |=
		le32_encode_bits(params->ttlm.default_link_mapping,
				 WMI_ADV_TTLM_CTRL_DEFAULT_LINK_MAPPING_MASK);
	ie_info->tid_to_link_map_ctrl |=
		le32_encode_bits(params->ttlm.mapping_switch_time_present,
				 WMI_ADV_TTLM_CTRL_MST_PRESENT_MASK);
	ie_info->tid_to_link_map_ctrl |=
		le32_encode_bits(params->ttlm.expected_duration_present,
				 WMI_ADV_TTLM_CTRL_ED_PRESENT_MASK);
	ie_info->tid_to_link_map_ctrl |=
		le32_encode_bits(params->ttlm.link_mapping_size,
				 WMI_ADV_TTLM_CTRL_LINK_MAP_SIZE_MASK);

	ie_info->map_switch_time = cpu_to_le32(params->ttlm.mapping_switch_time);
	ie_info->expected_duration = cpu_to_le32(params->ttlm.expected_duration);
	ie_info->disabled_link_bitmap = cpu_to_le32(params->disabled_link_bitmap);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "dir:%d def_map:%d mst_persent:%d ed_present:%d "
		   "link_map_size:%d mst:%d ed:%d disabled_link_map 0x%x",
		   le32_get_bits(ie_info->tid_to_link_map_ctrl,
				 WMI_ADV_TTLM_CTRL_DIRECTION_MASK),
		   le32_get_bits(ie_info->tid_to_link_map_ctrl,
				 WMI_ADV_TTLM_CTRL_DEFAULT_LINK_MAPPING_MASK),
		   le32_get_bits(ie_info->tid_to_link_map_ctrl,
				 WMI_ADV_TTLM_CTRL_MST_PRESENT_MASK),
		   le32_get_bits(ie_info->tid_to_link_map_ctrl,
				 WMI_ADV_TTLM_CTRL_ED_PRESENT_MASK),
		   le32_get_bits(ie_info->tid_to_link_map_ctrl,
				 WMI_ADV_TTLM_CTRL_LINK_MAP_SIZE_MASK),
		   ie_info->map_switch_time,
		   ie_info->expected_duration,
		   ie_info->disabled_link_bitmap);

	/* Do not fill link mapping values when default mapping is set to 1 */
	if (params->ttlm.default_link_mapping)
		return;

	ie_info->tid_to_link_map_ctrl |=
		le32_encode_bits(0xff,
				 WMI_ADV_TTLM_CTRL_MAP_PRESENCE_INDICATOR_MASK);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "link_map_present_indicator: 0x%x",
		   le32_get_bits(ie_info->tid_to_link_map_ctrl,
				 WMI_ADV_TTLM_CTRL_MAP_PRESENCE_INDICATOR_MASK));

	ie_info->ieee_tid_0_1_link_map |=
		le32_encode_bits(params->ttlm.ieee_link_map_tid[0],
				 WMI_ADV_TTLM_TID0_LINK_MAP_MASK);
	ie_info->ieee_tid_0_1_link_map |=
		le32_encode_bits(params->ttlm.ieee_link_map_tid[1],
				 WMI_ADV_TTLM_TID1_LINK_MAP_MASK);
	ie_info->ieee_tid_2_3_link_map |=
		le32_encode_bits(params->ttlm.ieee_link_map_tid[2],
				 WMI_ADV_TTLM_TID2_LINK_MAP_MASK);
	ie_info->ieee_tid_2_3_link_map |=
		le32_encode_bits(params->ttlm.ieee_link_map_tid[3],
				 WMI_ADV_TTLM_TID3_LINK_MAP_MASK);
	ie_info->ieee_tid_4_5_link_map |=
		le32_encode_bits(params->ttlm.ieee_link_map_tid[4],
				 WMI_ADV_TTLM_TID4_LINK_MAP_MASK);
	ie_info->ieee_tid_4_5_link_map |=
		le32_encode_bits(params->ttlm.ieee_link_map_tid[5],
				 WMI_ADV_TTLM_TID5_LINK_MAP_MASK);
	ie_info->ieee_tid_6_7_link_map |=
		le32_encode_bits(params->ttlm.ieee_link_map_tid[6],
				 WMI_ADV_TTLM_TID6_LINK_MAP_MASK);
	ie_info->ieee_tid_6_7_link_map |=
		le32_encode_bits(params->ttlm.ieee_link_map_tid[7],
				 WMI_ADV_TTLM_TID7_LINK_MAP_MASK);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "ieee link map of tid 0:0x%x 1:0x%x 2:0x%x 3:0x%x 4:0x%x 5:0x%x 6:0x%x 7:0x%x\n",
		   le32_get_bits(ie_info->ieee_tid_0_1_link_map, WMI_ADV_TTLM_TID0_LINK_MAP_MASK),
		   le32_get_bits(ie_info->ieee_tid_0_1_link_map, WMI_ADV_TTLM_TID1_LINK_MAP_MASK),
		   le32_get_bits(ie_info->ieee_tid_2_3_link_map, WMI_ADV_TTLM_TID2_LINK_MAP_MASK),
		   le32_get_bits(ie_info->ieee_tid_2_3_link_map, WMI_ADV_TTLM_TID3_LINK_MAP_MASK),
		   le32_get_bits(ie_info->ieee_tid_4_5_link_map, WMI_ADV_TTLM_TID4_LINK_MAP_MASK),
		   le32_get_bits(ie_info->ieee_tid_4_5_link_map, WMI_ADV_TTLM_TID5_LINK_MAP_MASK),
		   le32_get_bits(ie_info->ieee_tid_6_7_link_map, WMI_ADV_TTLM_TID6_LINK_MAP_MASK),
		   le32_get_bits(ie_info->ieee_tid_6_7_link_map, WMI_ADV_TTLM_TID7_LINK_MAP_MASK));

	ie_info->hw_tid_0_1_link_map |=
		le32_encode_bits(params->ttlm.hw_link_map_tid[0],
				 WMI_ADV_TTLM_TID0_LINK_MAP_MASK);
	ie_info->hw_tid_0_1_link_map |=
		le32_encode_bits(params->ttlm.hw_link_map_tid[1],
				 WMI_ADV_TTLM_TID1_LINK_MAP_MASK);
	ie_info->hw_tid_2_3_link_map |=
		le32_encode_bits(params->ttlm.hw_link_map_tid[2],
				 WMI_ADV_TTLM_TID2_LINK_MAP_MASK);
	ie_info->hw_tid_2_3_link_map |=
		le32_encode_bits(params->ttlm.hw_link_map_tid[3],
				 WMI_ADV_TTLM_TID3_LINK_MAP_MASK);
	ie_info->hw_tid_4_5_link_map |=
		le32_encode_bits(params->ttlm.hw_link_map_tid[4],
				 WMI_ADV_TTLM_TID4_LINK_MAP_MASK);
	ie_info->hw_tid_4_5_link_map |=
		le32_encode_bits(params->ttlm.hw_link_map_tid[5],
				 WMI_ADV_TTLM_TID5_LINK_MAP_MASK);
	ie_info->hw_tid_6_7_link_map |=
		le32_encode_bits(params->ttlm.hw_link_map_tid[6],
				 WMI_ADV_TTLM_TID6_LINK_MAP_MASK);
	ie_info->hw_tid_6_7_link_map |=
		le32_encode_bits(params->ttlm.hw_link_map_tid[7],
				 WMI_ADV_TTLM_TID7_LINK_MAP_MASK);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "hw link map of tid 0:0x%x 1:0x%x 2:0x%x 3:0x%x 4:0x%x 5:0x%x 6:0x%x 7:0x%x\n",
		   le32_get_bits(ie_info->hw_tid_0_1_link_map, WMI_ADV_TTLM_TID0_LINK_MAP_MASK),
		   le32_get_bits(ie_info->hw_tid_0_1_link_map, WMI_ADV_TTLM_TID1_LINK_MAP_MASK),
		   le32_get_bits(ie_info->hw_tid_2_3_link_map, WMI_ADV_TTLM_TID2_LINK_MAP_MASK),
		   le32_get_bits(ie_info->hw_tid_2_3_link_map, WMI_ADV_TTLM_TID3_LINK_MAP_MASK),
		   le32_get_bits(ie_info->hw_tid_4_5_link_map, WMI_ADV_TTLM_TID4_LINK_MAP_MASK),
		   le32_get_bits(ie_info->hw_tid_4_5_link_map, WMI_ADV_TTLM_TID5_LINK_MAP_MASK),
		   le32_get_bits(ie_info->hw_tid_6_7_link_map, WMI_ADV_TTLM_TID6_LINK_MAP_MASK),
		   le32_get_bits(ie_info->hw_tid_6_7_link_map, WMI_ADV_TTLM_TID7_LINK_MAP_MASK));
}

int
ath12k_wmi_ap_tid_to_link_map_config(struct ath12k *ar,
				     struct ath12k_wmi_tid_to_link_map_ap_params *params)
{
	struct wmi_mlo_ap_vdev_tid_to_link_map_cmd_fixed_param *cmd;
	struct wmi_mlo_ap_vdev_tid_to_link_map_ie_info *ie_info;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;
	int i, ret, buf_len;

	if (params->num_ttlm_info > WLAN_MAX_TTLM_IE) {
		ath12k_warn(ar->ab,
			    "Failed to send TTLM command to FW for vdev id %d as ttlm info %d is greater than max %d",
			    params->vdev_id,
			    params->num_ttlm_info,
			    WLAN_MAX_TTLM_IE);
		return -EINVAL;
	}
	buf_len = sizeof(*cmd) + TLV_HDR_SIZE + (params->num_ttlm_info *
						 sizeof(*ie_info));

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, buf_len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_mlo_ap_vdev_tid_to_link_map_cmd_fixed_param *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_TID_TO_LINK_MAPPING_CMD_FIXED_PARAM,
				       sizeof(*cmd));
	cmd->pdev_id = params->pdev_id;
	cmd->vdev_id = params->vdev_id;
	ptr = skb->data + sizeof(*cmd);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 (params->num_ttlm_info *
					  sizeof(*ie_info)));
	ptr += TLV_HDR_SIZE;

	for (i = 0; i < params->num_ttlm_info; i++) {
		ie_info = (struct wmi_mlo_ap_vdev_tid_to_link_map_ie_info *)ptr;
		ie_info->tlv_header =
			ath12k_wmi_tlv_hdr(WMI_TAG_MLO_TID_TO_LINK_MAPPING_IE_INFO,
					   sizeof(*ie_info) - TLV_HDR_SIZE);
		/* update the ie info params */
		ath12k_wmi_ttlm_update_ie_info(ar, ie_info, &params->ie[i]);
		ptr += sizeof(*ie_info);
	}

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_MLO_AP_VDEV_TID_TO_LINK_MAP_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_MLO_AP_VDEV_TID_TO_LINK_MAP_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_dl_qos_profile_create(struct ath12k_base *ab,
				     struct ath12k_qos_params *param,
				     u8 qos_profile_id)
{
	struct ath12k *ar;
	struct ath12k_wmi_pdev *wmi;
	struct wmi_sawf_svc_cfg_cmd_fixed_param *cmd;
	struct sk_buff *skb;
	int len, ret;

	ar = ab->pdevs[0].ar;
	if (!ar)
		return -EINVAL;

	wmi = ar->wmi;
	if (!wmi)
		return -EINVAL;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_sawf_svc_cfg_cmd_fixed_param *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SAWF_SERVICE_CLASS_CFG_CMD_FIXED_PARAM,
						 sizeof(*cmd));

	cmd->svc_class_id = cpu_to_le32(qos_profile_id);
	cmd->min_thruput_kbps = cpu_to_le32(param->min_data_rate);
	cmd->max_thruput_kbps = cpu_to_le32(param->mean_data_rate);
	cmd->burst_size_bytes = cpu_to_le32(param->burst_size);
	cmd->svc_interval_ms = cpu_to_le32(param->min_service_interval);
	cmd->delay_bound_ms = cpu_to_le32(param->delay_bound);
	cmd->time_to_live_ms = cpu_to_le32(param->msdu_life_time);
	cmd->priority = cpu_to_le32(param->priority);
	cmd->tid = cpu_to_le32(param->tid);
	cmd->msdu_loss_rate_ppm = cpu_to_le32(param->msdu_delivery_info);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "QoS Profile Configure: Profile ID: %u, min_throughput: %u,"
		   "max_throughput: %u, burst_size: %u, svc_interval: %u,"
		   "delay_bound: %u, TTL: %u, priority: %u,"
		   "tid: %u, msdu_loss_rate: %u",
		   cmd->svc_class_id, cmd->min_thruput_kbps,
		   cmd->max_thruput_kbps,
		   cmd->burst_size_bytes, cmd->svc_interval_ms,
		   cmd->delay_bound_ms,
		   cmd->time_to_live_ms, cmd->priority, cmd->tid,
		   cmd->msdu_loss_rate_ppm);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_SAWF_SERVICE_CLASS_CFG_CMDID);
	if (ret) {
		ath12k_err(ab,
			   "failed to config/reconfig QoS params");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_dl_qos_profile_delete(struct ath12k_base *ab, u8 qos_profile_id)
{
	struct ath12k *ar;
	struct ath12k_wmi_pdev *wmi;
	struct wmi_sawf_svc_disable_cmd_fixed_param *cmd;
	struct sk_buff *skb;
	int len, ret;

	ar = ab->pdevs[0].ar;
	if (!ar)
		return -ENOMEM;

	wmi = ar->wmi;
	if (!wmi)
		return -ENOMEM;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_sawf_svc_disable_cmd_fixed_param *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SAWF_SERVICE_CLASS_DISABLE_CMD_FIXED_PARAM,
						 sizeof(*cmd));

	cmd->svc_class_id = cpu_to_le32(qos_profile_id);

	ath12k_dbg_level(ar->ab, ATH12K_DBG_WMI, ATH12K_DBG_L2,
			 "QoS profile WMI disable: qos id: %u",
			 cmd->svc_class_id);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_SAWF_SERVICE_CLASS_DISABLE_CMDID);
	if (ret) {
		ath12k_err(ar->ab,
			   "failed to disable qos id: %u\n",
			   qos_profile_id);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_ul_qos_profile_config(struct ath12k *ar,
				     struct ath12k_wmi_ul_qos_params *params)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_tid_latency_info *latency_info;
	struct wmi_peer_tid_latency_config_fixed_param *cmd;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	u32 len;
	u32 num_peer = 1;
	int ret;

	if (!params)
		return -EINVAL;

	len = sizeof(*cmd) + TLV_HDR_SIZE + (num_peer * sizeof(*latency_info));
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb) {
		ath12k_warn(ar->ab, "wmi cfg peer latency fail-Outof Memory\n");
		return -ENOMEM;
	}
	cmd = (struct wmi_peer_tid_latency_config_fixed_param *)skb->data;
	cmd->tlv_header =
		le32_encode_bits(WMI_TAG_PEER_TID_LATENCY_CONFIG_FIXED_PARAM,
				 WMI_TLV_TAG) |
		le32_encode_bits((sizeof(*cmd) - TLV_HDR_SIZE),
				 WMI_TLV_LEN);

	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);

	tlv = (struct wmi_tlv *)(skb->data + sizeof(*cmd));
	len = sizeof(*latency_info) * num_peer;

	tlv->header = le32_encode_bits(WMI_TAG_ARRAY_STRUCT, WMI_TLV_TAG) |
			le32_encode_bits(len, WMI_TLV_LEN);

	latency_info =
		(struct wmi_tid_latency_info *)(skb->data + sizeof(*cmd) + TLV_HDR_SIZE);

	latency_info->tlv_header =
		le32_encode_bits(WMI_TAG_TID_LATENCY_INFO,
				 WMI_TLV_TAG) |
		le32_encode_bits((sizeof(*latency_info) - TLV_HDR_SIZE),
				 WMI_TLV_LEN);

	latency_info->service_interval = cpu_to_le32(params->service_interval);
	latency_info->burst_size_diff = cpu_to_le32(params->burst_size);
	latency_info->max_latency = cpu_to_le32(params->max_latency);
	latency_info->min_tput = cpu_to_le32(params->min_throughput);

	ether_addr_copy(latency_info->destmac.addr, params->peer_mac);

	latency_info->latency_tid_info =
		le32_encode_bits(params->latency_tid, SDWF_UL_TID_NUM) |
		le32_encode_bits(params->ac, SDWF_UL_AC) |
		le32_encode_bits(params->dl_enable, SDWF_UL_DL_EN) |
		le32_encode_bits(params->ul_enable, SDWF_UL_UL_EN) |
		le32_encode_bits(params->add_or_sub, SDWF_UL_BURST_SZ_SUM) |
		le32_encode_bits(params->sawf_ul_param, SDWF_UL_PARAM) |
		le32_encode_bits(params->ofdma_disable,
				 SDWF_UL_UL_OFDMA_DISABLE) |
		le32_encode_bits(params->mu_mimo_disable,
				 SDWF_UL_UL_MU_MIMO_DISABLE);
	ret = ath12k_wmi_cmd_send_nowait(wmi, skb,
					 WMI_PEER_TID_LATENCY_CONFIG_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_PEER_TID_LATENCY_CONFIG_CMDID cmd %d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_pdev_pktlog_enable(struct ath12k *ar, u32 pktlog_filter)
{
        struct ath12k_wmi_pdev *wmi = ar->wmi;
        struct wmi_pktlog_enable_cmd *cmd;
        struct sk_buff *skb;
        int ret;

        skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
        if (!skb)
                return -ENOMEM;

        cmd = (struct wmi_pktlog_enable_cmd *)skb->data;

        cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_PKTLOG_ENABLE_CMD,
                                                 sizeof(*cmd));

        cmd->pdev_id = cpu_to_le32(DP_HW2SW_MACID(ar->pdev->pdev_id));
        cmd->evlist = cpu_to_le32(pktlog_filter);
        cmd->enable = cpu_to_le32(ATH12K_WMI_PKTLOG_ENABLE_FORCE);

        ret = ath12k_wmi_cmd_send(wmi, skb,
                                  WMI_PDEV_PKTLOG_ENABLE_CMDID);
        if (ret) {
                ath12k_err(ar->ab,
			   "failed to send WMI_PDEV_PKTLOG_ENABLE_CMDID\n");
                dev_kfree_skb(skb);
        }

        return ret;
}

int ath12k_wmi_pdev_pktlog_disable(struct ath12k *ar)
{
        struct ath12k_wmi_pdev *wmi = ar->wmi;
        struct wmi_pktlog_disable_cmd *cmd;
        struct sk_buff *skb;
        int ret;

        skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
        if (!skb)
                return -ENOMEM;

        cmd = (struct wmi_pktlog_disable_cmd *)skb->data;

        cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_PKTLOG_DISABLE_CMD,
                                                 sizeof(*cmd));

        cmd->pdev_id = cpu_to_le32(DP_HW2SW_MACID(ar->pdev->pdev_id));

        ret = ath12k_wmi_cmd_send(wmi, skb,
                                  WMI_PDEV_PKTLOG_DISABLE_CMDID);
        if (ret) {
                ath12k_err(ar->ab,
			   "failed to send WMI_PDEV_PKTLOG_DISABLE_CMDID\n");
                dev_kfree_skb(skb);
        }

        return ret;
}

int ath12k_wmi_vdev_adfs_ch_cfg_cmd_send(struct ath12k *ar,
					 u32 vdev_id,
					 struct cfg80211_chan_def *def)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_adfs_ch_cfg_cmd *cmd;
	u32 min_duration_ms;
	struct sk_buff *skb;
	int ret = 0;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_adfs_ch_cfg_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_ADFS_CH_CFG_CMD,
						 sizeof(struct wmi_vdev_adfs_ch_cfg_cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);

	if (ar->ab->dfs_region == ATH12K_DFS_REG_ETSI) {
		cmd->ocac_mode = WMI_ADFS_MODE_QUICK_OCAC;
		min_duration_ms = cfg80211_chandef_dfs_cac_time(ar->ah->hw->wiphy,
								def, true, false);
		cmd->min_duration_ms = cpu_to_le32(min_duration_ms);

		if (min_duration_ms == MIN_WEATHER_RADAR_CHAN_PRECAC_TIMEOUT)
			cmd->max_duration_ms = cpu_to_le32(MAX_WEATHER_RADAR_CHAN_PRECAC_TIMEOUT);
		else
			cmd->max_duration_ms = cpu_to_le32(MAX_PRECAC_TIMEOUT);
	} else if (ar->ab->dfs_region == ATH12K_DFS_REG_FCC) {
		cmd->ocac_mode = cpu_to_le32(WMI_ADFS_MODE_QUICK_RCAC);
		cmd->min_duration_ms = cpu_to_le32(MIN_RCAC_TIMEOUT);
		cmd->max_duration_ms = cpu_to_le32(MAX_RCAC_TIMEOUT);
	}

	cmd->chan_freq = cpu_to_le32(def->chan->center_freq);
	cmd->chan_width = cpu_to_le32(ath12k_wmi_get_host_chan_width(def->width));
	cmd->center_freq1 = cpu_to_le32(def->center_freq1);
	cmd->center_freq2 = cpu_to_le32(def->center_freq2);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "Send adfs channel cfg command for vdev id %d "
		   "mode as %d min duration %d chan_freq %d chan_width %d\n"
		   "center_freq1 %d center_freq2 %d", cmd->vdev_id,
		   cmd->ocac_mode, cmd->min_duration_ms, cmd->chan_freq,
		   cmd->chan_width, cmd->center_freq1, cmd->center_freq2);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_ADFS_CH_CFG_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_VDEV_ADFS_CH_CFG_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_adfs_ocac_abort_cmd_send(struct ath12k *ar, u32 vdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_adfs_ocac_abort_cmd *cmd;
	struct sk_buff *skb;
	int ret = 0;

	if (!ar->agile_chandef.chan) {
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "Currently, agile CAC is not active on any channel."
			   "Ignore abort");
		return ret;
	}

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_adfs_ocac_abort_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_ADFS_OCAC_ABORT_CMD,
						 sizeof(struct wmi_vdev_adfs_ocac_abort_cmd));

	cmd->vdev_id = cpu_to_le32(vdev_id);
	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_ADFS_OCAC_ABORT_CMDID);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_VDEV_ADFS_ABORT_CMD\n");
		dev_kfree_skb(skb);
		return ret;
	}
	return ret;
}

int ath12k_wmi_atf_send_group_config(struct ath12k *ar)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_atf_group_info *group;
	struct wmi_atf_group_info *group_info;
	struct wmi_atf_ssid_grp_request_fixed_param *cmd;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;
	int ret, len, i;
	u32 pdev_id = ar->pdev->pdev_id;

	len = sizeof(*cmd) + TLV_HDR_SIZE + TLV_HDR_SIZE;
	len += ar->atf_table.total_groups * sizeof(*group_info);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = (struct wmi_atf_ssid_grp_request_fixed_param *)ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ATF_SSID_GRP_REQUEST_FIXED_PARAM,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(pdev_id);
	ptr += sizeof(*cmd);

	/* Adding two tag array structs because the WMI command structure expects two array
	 * arguments. Since the first one is deprecated, it will be sent with a size of 0.
	 */
	tlv = (struct wmi_tlv *)ptr;
	tlv->header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ARRAY_STRUCT, TLV_HDR_SIZE);
	ptr += TLV_HDR_SIZE;

	tlv = (struct wmi_tlv *)ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 ar->atf_table.total_groups *
					 sizeof(*group_info));
	ptr += TLV_HDR_SIZE;

	group_info = (struct wmi_atf_group_info *)ptr;
	for (i = 0; i < ar->atf_table.total_groups; i++) {
		group = &ar->atf_table.group_info[i];
		group_info->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ATF_SSID_GROUPING_REQUEST_EVENT_V2,
					       sizeof(*group_info));
		group_info->atf_group_id = cpu_to_le32(group->group_id);
		group_info->atf_group_units = cpu_to_le32(group->group_airtime);
		group_info->atf_group_flags =
			le32_encode_bits(group->group_policy, WMI_ATF_GROUP_SCHED_POLICY);
		group_info->atf_total_num_peers =
			le32_encode_bits(group->unconfigured_peers,
					 WMI_ATF_GROUP_NUM_IMPLICIT_PEERS) |
			le32_encode_bits(group->configured_peers,
					 WMI_ATF_GROUP_NUM_EXPLICIT_PEERS);
		group_info->atf_total_implicit_peer_units =
			cpu_to_le32(group->unconfigured_peers_airtime);
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "id: %d  airtime %d flags %d num: %d",
			   group_info->atf_group_id, group_info->atf_group_units,
			   group_info->atf_group_flags, group_info->atf_total_num_peers);

		group_info++;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI ATF SSID group config for pdev id %u total groups %u\n",
		   ar->pdev->pdev_id, ar->atf_table.total_groups);
	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_ATF_SSID_GROUPING_REQUEST_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_ATF_SSID_GROUPING_REQUEST_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_atf_send_peer_config(struct ath12k *ar,
				    struct ath12k_atf_peer_params *peer_param)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_atf_request_fixed_param *cmd;
	struct ath12k_atf_peer_info *param_peer_info;
	struct wmi_atf_peer_info *peer_info;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;
	int ret, len, i;

	len = sizeof(*cmd) + TLV_HDR_SIZE + TLV_HDR_SIZE;
	len += peer_param->num_peers * sizeof(*peer_info);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb) {
		ath12k_err(ar->ab, "failed to allocate skb");
		return -ENOMEM;
	}

	ptr = skb->data;

	cmd = (struct wmi_peer_atf_request_fixed_param *)ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_ATF_REQUEST,
						 sizeof(*cmd));
	cmd->num_peers = cpu_to_le32(peer_param->num_peers);
	cmd->pdev_id = cpu_to_le32(peer_param->pdev_id);
	cmd->atf_flags = cpu_to_le32(peer_param->atf_flags);
	ptr += sizeof(*cmd);

	/* Adding two tag array structs because the WMI command structure expects two array
	 * arguments. Since the first one is deprecated, it will be sent with a size of 0.
	 */
	tlv = (struct wmi_tlv *)ptr;
	tlv->header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ARRAY_STRUCT, TLV_HDR_SIZE);
	ptr += TLV_HDR_SIZE;

	tlv = (struct wmi_tlv *)ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 peer_param->num_peers *
					 sizeof(*peer_info));
	ptr += TLV_HDR_SIZE;

	param_peer_info = peer_param->peer_info;

	peer_info = (struct wmi_atf_peer_info *)ptr;
	for (i = 0; i < peer_param->num_peers; i++) {
		peer_info->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ATF_PEER_REQUEST_EVENT_V2,
							       sizeof(*peer_info));
		memcpy(peer_info->peer_macaddr, param_peer_info->peer_macaddr, ETH_ALEN);
		peer_info->atf_peer_info =
			le32_encode_bits(param_peer_info->percentage_peer, WMI_ATF_PEER_AIRTIME) |
			le32_encode_bits(param_peer_info->group_index, WMI_ATF_PEER_GROUP_ID) |
			le32_encode_bits(param_peer_info->explicit_peer_flag,
					 WMI_ATF_PEER_CONFIGURED);
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "ATF: peer mac %pM percentage %u group_id %u explicit_peer_flag %u\n",
			   param_peer_info->peer_macaddr, param_peer_info->percentage_peer,
			   param_peer_info->group_index, param_peer_info->explicit_peer_flag);
		peer_info++;
		param_peer_info++;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "ATF: WMI ATF peer config for num_peers %u pdev id %u atf_flags %u peers %u\n",
		   peer_param->num_peers, peer_param->pdev_id,
		   peer_param->atf_flags, peer_param->num_peers);
	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_ATF_REQUEST_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_PEER_ATF_REQUEST_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

