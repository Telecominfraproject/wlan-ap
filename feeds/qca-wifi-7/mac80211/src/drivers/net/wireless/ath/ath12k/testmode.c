// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "testmode.h"
#include <net/netlink.h>
#include "debug.h"
#include "wmi.h"
#include "hw.h"
#include "core.h"
#include "hif.h"
#include "../testmode_i.h"

#define ATH12K_FTM_SEGHDR_CURRENT_SEQ		GENMASK(3, 0)
#define ATH12K_FTM_SEGHDR_TOTAL_SEGMENTS	GENMASK(7, 4)

static const struct nla_policy ath12k_tm_policy[ATH_TM_ATTR_MAX + 1] = {
	[ATH_TM_ATTR_CMD]		= { .type = NLA_U32 },
	[ATH_TM_ATTR_DATA]		= { .type = NLA_BINARY,
					    .len = ATH_TM_DATA_MAX_LEN },
	[ATH_TM_ATTR_WMI_CMDID]		= { .type = NLA_U32 },
	[ATH_TM_ATTR_VERSION_MAJOR]	= { .type = NLA_U32 },
	[ATH_TM_ATTR_VERSION_MINOR]	= { .type = NLA_U32 },
	[ATH_TM_ATTR_FWLOG]		= { .type = NLA_BINARY,
					    .len = 2048 },
	[ATH_TM_ATTR_LINK_IDX]		= { .type = NLA_U8 },
	[ATH_TM_ATTR_DUAL_MAC]		= { .type = NLA_U8 },
};

void ath12k_fwlog_write(struct ath12k_base *ab, u8 *data, int len)
{
	struct sk_buff *nl_skb;
	int ret, i;
	struct ath12k *ar = NULL;
	struct ath12k_pdev *pdev;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		if (pdev && pdev->ar)  {
			ar = pdev->ar;
			break;
		}
	}

	if (!ar)
		return;

	nl_skb = cfg80211_testmode_alloc_event_skb(ar->ah->hw->wiphy, len, GFP_ATOMIC);

	if (!nl_skb) {
		ath12k_warn(ab,  "failed to allocate skb for fwlog event\n");
		return;
	}

	ret = nla_put(nl_skb, ATH_TM_ATTR_FWLOG, len, data);
	if (ret) {
		ath12k_warn(ab, "failed to put fwlog wmi event to nl: %d\n", ret);
		kfree_skb(nl_skb);
		return;
	}

	if (ab->ag->mlo_capable) {
		ret = nla_put_u8(nl_skb, ATH_TM_ATTR_LINK_IDX, ar->hw_link_id);
		if (ret) {
			ath12k_warn(ab, "failed to put link idx wmi event to nl: %d\n", ret);
			kfree_skb(nl_skb);
			return;
		}
	}

	if (ab->num_radios == 2)
		ret = nla_put_u8(nl_skb, ATH_TM_ATTR_DUAL_MAC, ab->num_radios);

	if (ret) {
		ath12k_warn(ab, "failed to put dual mac wmi event to nl: %d\n", ret);
		kfree_skb(nl_skb);
		return;
	}

	cfg80211_testmode_event(nl_skb, GFP_ATOMIC);
}

static struct ath12k *ath12k_tm_get_ar(struct ath12k_base *ab)
{
	struct ath12k_pdev *pdev;
	struct ath12k *ar;
	int i;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		ar = pdev->ar;

		if (ar && ar->ah->state == ATH12K_HW_STATE_TM)
			return ar;
	}

	return NULL;
}

void ath12k_tm_wmi_event_unsegmented(struct ath12k_base *ab, u32 cmd_id,
				     struct sk_buff *skb)
{
	struct sk_buff *nl_skb;
	struct ath12k *ar;

	ath12k_dbg(ab, ATH12K_DBG_TESTMODE,
		   "testmode event wmi cmd_id %d skb length %d\n",
		   cmd_id, skb->len);

	ath12k_dbg_dump(ab, ATH12K_DBG_TESTMODE, NULL, "", skb->data, skb->len);

	ar = ath12k_tm_get_ar(ab);
	if (!ar) {
		ath12k_warn(ab, "testmode event not handled due to invalid pdev\n");
		return;
	}

	spin_lock_bh(&ar->data_lock);

	nl_skb = cfg80211_testmode_alloc_event_skb(ar->ah->hw->wiphy,
						   2 * nla_total_size(sizeof(u32)) +
						   nla_total_size(skb->len),
						   GFP_ATOMIC);
	spin_unlock_bh(&ar->data_lock);

	if (!nl_skb) {
		ath12k_warn(ab,
			    "failed to allocate skb for unsegmented testmode wmi event\n");
		return;
	}

	if (nla_put_u32(nl_skb, ATH_TM_ATTR_CMD, ATH_TM_CMD_WMI) ||
	    nla_put_u32(nl_skb, ATH_TM_ATTR_WMI_CMDID, cmd_id) ||
	    nla_put(nl_skb, ATH_TM_ATTR_DATA, skb->len, skb->data)) {
		ath12k_warn(ab, "failed to populate testmode unsegmented event\n");
		kfree_skb(nl_skb);
		return;
	}

	cfg80211_testmode_event(nl_skb, GFP_ATOMIC);
}

void ath12k_tm_process_event(struct ath12k_base *ab, u32 cmd_id,
			     const void **tb,
			     u16 length)
{
	const struct wmi_pdev_utf_event_param *param = NULL;
	const struct ath12k_wmi_ftm_event *ftm_msg;
	u8 total_segments, current_seq;
	struct sk_buff *nl_skb;
	struct ath12k *ar;
	u32 data_pos, pdev_id;
	u16 datalen;
	u8 const *buf_pos;

	ftm_msg = tb[WMI_TAG_ARRAY_BYTE];
	if (!ftm_msg) {
		ath12k_warn(ab, "failed to fetch ftm msg\n");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_TESTMODE,
		   "testmode event wmi cmd_id %d ftm event msg %pK datalen %d\n",
		   cmd_id, ftm_msg, length);
	ath12k_dbg_dump(ab, ATH12K_DBG_TESTMODE, NULL, "", ftm_msg, length);

	if (test_bit(WMI_TLV_SERVICE_PDEV_PARAM_IN_UTF_WMI, ab->wmi_ab.svc_map)) {
		param = tb[WMI_TAG_PDEV_UTF_EVENT_FIXED_PARAM];
		if (!param) {
			ath12k_warn(ab, "failed to fetch utf msg\n");
			return;
		}
		length -= (sizeof(*param) + TLV_HDR_SIZE);
		pdev_id = le32_to_cpu(DP_HW2SW_MACID(param->pdev_id));
		ath12k_dbg_dump(ab, ATH12K_DBG_TESTMODE, NULL, "", param, sizeof(*param));
	} else {
		pdev_id = DP_HW2SW_MACID(le32_to_cpu(ftm_msg->seg_hdr.pdev_id));
	}

	if (pdev_id >= ab->num_radios) {
		ath12k_warn(ab, "testmode event not handled due to invalid pdev id\n");
		return;
	}

	ar = ab->pdevs[pdev_id].ar;

	if (!ar) {
		ath12k_warn(ab, "testmode event not handled due to absence of pdev\n");
		return;
	}

	current_seq = le32_get_bits(ftm_msg->seg_hdr.segmentinfo,
				    ATH12K_FTM_SEGHDR_CURRENT_SEQ);
	total_segments = le32_get_bits(ftm_msg->seg_hdr.segmentinfo,
				       ATH12K_FTM_SEGHDR_TOTAL_SEGMENTS);
	datalen = length - (sizeof(struct ath12k_wmi_ftm_seg_hdr_params));
	buf_pos = ftm_msg->data;

	if (current_seq == 0) {
		ab->ftm_event_obj.expected_seq = 0;
		ab->ftm_event_obj.data_pos = 0;
	}

	data_pos = ab->ftm_event_obj.data_pos;

	if ((data_pos + datalen) > ATH_FTM_EVENT_MAX_BUF_LENGTH) {
		ath12k_warn(ab,
			    "Invalid event length date_pos[%d] datalen[%d]\n",
			    data_pos, datalen);
		return;
	}

	memcpy(&ab->ftm_event_obj.eventdata[data_pos], buf_pos, datalen);
	data_pos += datalen;

	if (++ab->ftm_event_obj.expected_seq != total_segments) {
		ab->ftm_event_obj.data_pos = data_pos;
		ath12k_dbg(ab, ATH12K_DBG_TESTMODE,
			   "partial data received current_seq[%d], total_seg[%d]\n",
			    current_seq, total_segments);
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_TESTMODE,
		   "total data length[%d] = [%d]\n",
		   data_pos, ftm_msg->seg_hdr.len);

	spin_lock_bh(&ar->data_lock);
	nl_skb = cfg80211_testmode_alloc_event_skb(ar->ah->hw->wiphy,
						   2 * nla_total_size(sizeof(u32)) +
						   nla_total_size(data_pos),
						   GFP_ATOMIC);
	spin_unlock_bh(&ar->data_lock);

	if (!nl_skb) {
		ath12k_warn(ab,
			    "failed to allocate skb for testmode wmi event\n");
		return;
	}

	if (nla_put_u32(nl_skb, ATH_TM_ATTR_CMD,
			ATH_TM_CMD_WMI_FTM) ||
	    nla_put_u32(nl_skb, ATH_TM_ATTR_WMI_CMDID, cmd_id) ||
	    nla_put(nl_skb, ATH_TM_ATTR_DATA, data_pos,
		    &ab->ftm_event_obj.eventdata[0])) {
		ath12k_warn(ab, "failed to populate testmode event");
		kfree_skb(nl_skb);
		return;
	}

	cfg80211_testmode_event(nl_skb, GFP_ATOMIC);
}

static int ath12k_tm_cmd_get_version(struct ath12k *ar, struct nlattr *tb[])
{
	struct sk_buff *skb;

	ath12k_dbg(ar->ab, ATH12K_DBG_TESTMODE,
		   "testmode cmd get version_major %d version_minor %d\n",
		   ATH_TESTMODE_VERSION_MAJOR,
		   ATH_TESTMODE_VERSION_MINOR);

	spin_lock_bh(&ar->data_lock);
	skb = cfg80211_testmode_alloc_reply_skb(ar->ah->hw->wiphy,
						2 * nla_total_size(sizeof(u32)));
	spin_unlock_bh(&ar->data_lock);

	if (!skb)
		return -ENOMEM;

	if (nla_put_u32(skb, ATH_TM_ATTR_VERSION_MAJOR,
			ATH_TESTMODE_VERSION_MAJOR) ||
	    nla_put_u32(skb, ATH_TM_ATTR_VERSION_MINOR,
			ATH_TESTMODE_VERSION_MINOR)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_testmode_reply(skb);
}

static int ath12k_tm_cmd_process_ftm(struct ath12k *ar, struct nlattr *tb[])
{
	struct wmi_pdev_utf_cmd_fixed_param *utf_cmd;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	struct ath12k_wmi_ftm_cmd *ftm_cmd;
	uint16_t len;
	int ret = 0;
	void *buf;
	size_t aligned_len;
	u32 cmd_id, buf_len;
	u16 chunk_len, total_bytes, num_segments;
	u8 segnumber = 0, *bufpos;

	ath12k_dbg(ar->ab, ATH12K_DBG_TESTMODE, "ah->state  %d\n", ar->ah->state);
	if (ar->ah->state != ATH12K_HW_STATE_TM)
		return -ENETDOWN;

	if (!tb[ATH_TM_ATTR_DATA])
		return -EINVAL;

	buf = nla_data(tb[ATH_TM_ATTR_DATA]);
	buf_len = nla_len(tb[ATH_TM_ATTR_DATA]);
	cmd_id = WMI_PDEV_UTF_CMDID;
	ath12k_dbg(ar->ab, ATH12K_DBG_TESTMODE,
		   "testmode cmd wmi cmd_id %d buf %pK buf_len %d\n",
		   cmd_id, buf, buf_len);
	ath12k_dbg_dump(ar->ab, ATH12K_DBG_TESTMODE, NULL, "", buf, buf_len);
	bufpos = buf;
	total_bytes = buf_len;
	num_segments = total_bytes / MAX_WMI_UTF_LEN;

	if (buf_len - (num_segments * MAX_WMI_UTF_LEN))
		num_segments++;

	while (buf_len) {
		if (buf_len > MAX_WMI_UTF_LEN)
			chunk_len = MAX_WMI_UTF_LEN;    /* MAX message */
		else
			chunk_len = buf_len;

		if (test_bit(WMI_TLV_SERVICE_PDEV_PARAM_IN_UTF_WMI, ar->ab->wmi_ab.svc_map))
			len = chunk_len + sizeof(*ftm_cmd) + sizeof(*utf_cmd);
		else
			len = chunk_len + sizeof(*ftm_cmd);

		skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);

		if (!skb)
			return -ENOMEM;

		ftm_cmd = (struct ath12k_wmi_ftm_cmd *)skb->data;
		aligned_len  = chunk_len + sizeof(struct ath12k_wmi_ftm_seg_hdr_params);
		ftm_cmd->tlv_header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, aligned_len);
		ftm_cmd->seg_hdr.len = cpu_to_le32(total_bytes);
		ftm_cmd->seg_hdr.msgref = cpu_to_le32(ar->ftm_msgref);
		ftm_cmd->seg_hdr.segmentinfo =
			le32_encode_bits(num_segments,
					 ATH12K_FTM_SEGHDR_TOTAL_SEGMENTS) |
			le32_encode_bits(segnumber,
					 ATH12K_FTM_SEGHDR_CURRENT_SEQ);
		ftm_cmd->seg_hdr.pdev_id = cpu_to_le32(ar->pdev->pdev_id);
		segnumber++;
		memcpy(&ftm_cmd->data, bufpos, chunk_len);
		if (test_bit(WMI_TLV_SERVICE_PDEV_PARAM_IN_UTF_WMI, ar->ab->wmi_ab.svc_map)) {
			utf_cmd = (struct wmi_pdev_utf_cmd_fixed_param *)(skb->data + chunk_len + sizeof(*ftm_cmd));
			utf_cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_PDEV_UTF_CMD_FIXED_PARAM) |
				FIELD_PREP(WMI_TLV_LEN, sizeof(*utf_cmd) - TLV_HDR_SIZE);
			utf_cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
		}

		ret = ath12k_wmi_cmd_send(wmi, skb, cmd_id);

		if (ret) {
			ath12k_warn(ar->ab, "ftm wmi command fail: %d\n", ret);
			kfree_skb(skb);
			return ret;
		}

		buf_len -= chunk_len;
		bufpos += chunk_len;
	}

	++ar->ftm_msgref;
	return ret;
}

static int ath12k_tm_cmd_testmode_start(struct ath12k *ar, struct nlattr *tb[])
{
	if (ar->ah->state == ATH12K_HW_STATE_TM)
		return -EALREADY;

	if (ar->ah->state != ATH12K_HW_STATE_OFF)
		return -EBUSY;

	ar->ab->ftm_event_obj.eventdata = kzalloc(ATH_FTM_EVENT_MAX_BUF_LENGTH,
						  GFP_KERNEL);

	if (!ar->ab->ftm_event_obj.eventdata)
		return -ENOMEM;

	ar->ah->state = ATH12K_HW_STATE_TM;
	ar->ftm_msgref = 0;
	return 0;
}

static int ath12k_tm_cmd_wmi(struct ath12k *ar, struct nlattr *tb[],
			     struct ieee80211_vif *vif, u8 link_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_link_vif *arvif;
	struct ath12k_hw *ah = ar->ah;
	struct ath12k_vif *ahvif;
	struct sk_buff *skb;
	int ret = 0, tag;
	void *buf;
	u32 cmd_id, buf_len;
	u32 *cmd;

	if (!tb[ATH_TM_ATTR_DATA])
		return -EINVAL;

	if (!tb[ATH_TM_ATTR_WMI_CMDID])
		return -EINVAL;

	buf = nla_data(tb[ATH_TM_ATTR_DATA]);
	buf_len = nla_len(tb[ATH_TM_ATTR_DATA]);

	if (!buf_len) {
		ath12k_warn(ar->ab, "No data present in testmode command\n");
		return -EINVAL;
	}

	cmd_id = nla_get_u32(tb[ATH_TM_ATTR_WMI_CMDID]);

	cmd = (u32 *)buf;
	tag = FIELD_GET(WMI_TLV_TAG, *cmd);
	cmd++;

	if (tag == WMI_TAG_PDEV_SET_PARAM_CMD)
		*cmd = cpu_to_le32(ar->pdev->pdev_id);

	if (ar->ab->fw_mode != ATH12K_FIRMWARE_MODE_FTM &&
	    (tag == WMI_TAG_VDEV_SET_PARAM_CMD || tag == WMI_TAG_UNIT_TEST_CMD)) {
		if (vif) {
			ahvif = (struct ath12k_vif *)vif->drv_priv;
			arvif = wiphy_dereference(ah->hw->wiphy, ahvif->link[link_id]);
			if (!arvif) {
				ath12k_warn(ar->ab, "failed to find link interface\n");
				return -EINVAL;
			}
			*cmd = cpu_to_le32(arvif->vdev_id);
		}
		else {
			ath12k_warn(ar->ab, "vdev is not up for given vdev id, so failed to send wmi command (testmode): %d\n",
				    ret);
			return -EINVAL;
		}
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_TESTMODE,
		   "testmode cmd wmi cmd_id %d  buf length %d\n",
		   cmd_id, buf_len);

	ath12k_dbg_dump(ar->ab, ATH12K_DBG_TESTMODE, NULL, "", buf, buf_len);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, buf_len);

	if (!skb)
		return -ENOMEM;

	memcpy(skb->data, buf, buf_len);

	ret = ath12k_wmi_cmd_send(wmi, skb, cmd_id);
	if (ret) {
		dev_kfree_skb(skb);
		ath12k_warn(ar->ab, "failed to transmit wmi command (testmode): %d\n",
			    ret);
	}

	return ret;
}

int ath12k_tm_cmd(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		  u8 link_id, void *data, int len)
{
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar = NULL;
	struct nlattr *tb[ATH_TM_ATTR_MAX + 1];
	struct ath12k_base *ab;
	struct wiphy *wiphy = hw->wiphy;
	enum ath_tm_cmd cmd_type;
	int ret;

	lockdep_assert_held(&wiphy->mtx);

	ret = nla_parse(tb, ATH_TM_ATTR_MAX, data, len, ath12k_tm_policy,
			NULL);
	if (ret)
		return ret;

	if (!tb[ATH_TM_ATTR_CMD])
		return -EINVAL;

	cmd_type = nla_get_u32(tb[ATH_TM_ATTR_CMD]);

	if (vif == NULL && (cmd_type == ATH_TM_CMD_WMI_FTM ||
	    cmd_type == ATH_TM_CMD_TESTMODE_START ||
	    cmd_type == ATH_TM_CMD_WMI)) {
		if (ah->num_radio)
			ar = ah->radio;
	} else {
		ar = ath12k_get_ar_by_vif(hw, vif, link_id);
	}

	if (!ar) {
		ath12k_err(NULL,
			   "unable to determine device\n");
		return -EINVAL;
	}

	if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS) {
		ath12k_warn(ar->ab, "invalid link id specified\n");
		return -EINVAL;
	}

	ab = ar->ab;
	switch (cmd_type) {
	case ATH_TM_CMD_WMI:
		return ath12k_tm_cmd_wmi(ar, tb, vif, link_id);
	case ATH_TM_CMD_TESTMODE_START:
		return ath12k_tm_cmd_testmode_start(ar, tb);
	case ATH_TM_CMD_GET_VERSION:
		return ath12k_tm_cmd_get_version(ar, tb);
	case ATH_TM_CMD_WMI_FTM:
		set_bit(ATH12K_FLAG_FTM_SEGMENTED, &ab->dev_flags);
		return ath12k_tm_cmd_process_ftm(ar, tb);
	default:
		return -EOPNOTSUPP;
	}
}
EXPORT_SYMBOL(ath12k_tm_cmd);
