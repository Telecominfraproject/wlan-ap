// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2015, 2021 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "core.h"
#include "wmi.h"
#include "debug.h"
#include "debugfs.h"
#include "smart_ant.h"

static ssize_t ath11k_read_sa_enable_ops(struct file *file,
					 char __user *ubuf,
					 size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	char buf[4];
	int len = 0;

	if (!ath11k_smart_ant_enabled(ar))
		return -ENOTSUPP;

	len = scnprintf(buf, sizeof(buf) - len, "%d\n",
			ar->smart_ant_info.enabled);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static ssize_t ath11k_write_sa_enable_ops(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	int ret;
	bool enable;

	if (!ath11k_smart_ant_enabled(ar))
		return -ENOTSUPP;

	if (kstrtobool_from_user(ubuf, count, &enable))
		return -EINVAL;

	if (ar->smart_ant_info.enabled == enable)
		return count;

	mutex_lock(&ar->conf_mutex);
	if (ar->state != ATH11K_STATE_ON) {
		ath11k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
		mutex_unlock(&ar->conf_mutex);
		return -ENETDOWN;
	}

	ar->smart_ant_info.enabled = enable;
	if (enable) {
		ret = ath11k_wmi_pdev_enable_smart_ant(ar,
						       &ar->smart_ant_info);
		if (ret)
			goto exit;
	} else {
		ret = ath11k_wmi_pdev_disable_smart_ant(ar,
							&ar->smart_ant_info);
		if (ret)
			goto exit;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_SMART_ANT, "Smart antenna %s\n",
		   enable ? "enabled" : "disabled");
exit:
	mutex_unlock(&ar->conf_mutex);
	if (ret)
		return ret;
	return count;
}

static const struct file_operations fops_sa_enable_ops = {
	.read = ath11k_read_sa_enable_ops,
	.write = ath11k_write_sa_enable_ops,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

#define ATH11K_SA_TX_ANT_MIN_LEN 20

static ssize_t ath11k_write_sa_tx_ant(struct file *file,
				      const char __user *ubuf,
				      size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	u32 ants[WMI_SMART_MAX_RATE_SERIES], txant;
	u8 mac_addr[ETH_ALEN];
	struct ieee80211_sta *sta;
	struct ath11k_sta *arsta;
	int ret, i, vdev_id, len;
	char *token, *sptr;
	char buf[64];

	if (!ath11k_smart_ant_enabled(ar))
		return -ENOTSUPP;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;

	if (len < ATH11K_SA_TX_ANT_MIN_LEN)
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);
	if (ar->state != ATH11K_STATE_ON) {
		ath11k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
		mutex_unlock(&ar->conf_mutex);
		return -ENETDOWN;
	}
	mutex_unlock(&ar->conf_mutex);

	buf[len] = '\0';
	sptr = buf;
	for (i = 0; i < ETH_ALEN - 1; i++) {
		token = strsep(&sptr, ":");
		if (!token)
			return -EINVAL;
		if (kstrtou8(token, 16, &mac_addr[i]))
			return -EINVAL;
	}

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;

	if (kstrtou8(token, 16, &mac_addr[i]))
		return -EINVAL;

	if (kstrtou32(sptr, 16, &txant))
		return -EINVAL;

	if (txant > ((1 << ar->ab->target_caps.num_rf_chains) - 1)) {
		ath11k_err(ar->ab, "Invalid tx antenna config\n");
		return -EINVAL;
	}

	rcu_read_lock();

	sta = ieee80211_find_sta_by_ifaddr(ar->hw, mac_addr, NULL);
	if (!sta) {
		ath11k_err(ar->ab, "Sta entry not found\n");
		rcu_read_unlock();
		return -EINVAL;
	}

	arsta = (struct ath11k_sta *)sta->drv_priv;
	vdev_id = arsta->arvif->vdev_id;

	rcu_read_unlock();

	for (i = 0; i < WMI_SMART_MAX_RATE_SERIES; i++)
		ants[i] = txant;

	ath11k_dbg(ar->ab, ATH11K_DBG_SMART_ANT, "Smart antenna set tx antenna to %d\n",
		   txant);
	mutex_lock(&ar->conf_mutex);
	ret = ath11k_wmi_peer_set_smart_tx_ant(ar, vdev_id, mac_addr,
					       ants);
	mutex_unlock(&ar->conf_mutex);

	if (!ret)
		ret = count;

	return ret;
}

static const struct file_operations fops_sa_tx_ant = {
	.write = ath11k_write_sa_tx_ant,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_write_sa_rx_ant(struct file *file,
				      const char __user *ubuf,
				      size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	u8 rxant;
	int ret;

	if (!ath11k_smart_ant_enabled(ar))
		return -ENOTSUPP;

	if (kstrtou8_from_user(ubuf, count, 0, &rxant))
		return -EINVAL;

	if (rxant > ((1 << ar->ab->target_caps.num_rf_chains) - 1)) {
		ath11k_err(ar->ab, "Invalid Rx antenna config\n");
		return -EINVAL;
	}

	ath11k_dbg(ar->ab, ATH11K_DBG_SMART_ANT,
		   "Setting Rx antenna to %d\n", rxant);

	mutex_lock(&ar->conf_mutex);
	if (ar->state != ATH11K_STATE_ON) {
		ath11k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
		mutex_unlock(&ar->conf_mutex);
		return -ENETDOWN;
	}
	ret = ath11k_wmi_pdev_set_rx_ant(ar, rxant);
	mutex_unlock(&ar->conf_mutex);

	if (!ret)
		ret = count;

	return ret;
}

static ssize_t ath11k_read_sa_rx_ant(struct file *file,
				     char __user *ubuf,
				     size_t count, loff_t *ppos)
{
	char buf[4];
	struct ath11k *ar = file->private_data;
	int len = 0;

	len = scnprintf(buf, sizeof(buf) - len, "%d\n",
			ar->rx_antenna);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_sa_rx_ant = {
	.read = ath11k_read_sa_rx_ant,
	.write = ath11k_write_sa_rx_ant,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

#define ATH11K_SA_TRAIN_INFO_MIN_LEN 24

static ssize_t ath11k_write_sa_train_info(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	u8 mac_addr[ETH_ALEN];
	struct ieee80211_sta *sta;
	struct ath11k_sta *arsta;
	struct ath11k_smart_ant_train_info params;
	int ret, i, vdev_id, len;
	u32 rate_mask = 0;
	char *token, *sptr;
	char buf[128];

	mutex_lock(&ar->conf_mutex);
	if (ar->state != ATH11K_STATE_ON) {
		ath11k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
		mutex_unlock(&ar->conf_mutex);
		return -ENETDOWN;
	}
	mutex_unlock(&ar->conf_mutex);

	if (!ath11k_smart_ant_enabled(ar))
		return -ENOTSUPP;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;

	if (len < ATH11K_SA_TRAIN_INFO_MIN_LEN)
		return -EINVAL;

	buf[len] = '\0';
	sptr = buf;
	for (i = 0; i < ETH_ALEN - 1; i++) {
		token = strsep(&sptr, ":");
		if (!token)
			return -EINVAL;

		if (kstrtou8(token, 16, &mac_addr[i]))
			return -EINVAL;
	}

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;

	if (kstrtou8(token, 16, &mac_addr[i]))
		return -EINVAL;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &params.rate_array[0]))
		return -EINVAL;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;

	if (kstrtou32(token, 16, &params.antenna_array[0]))
		return -EINVAL;

	if (kstrtou32(sptr, 0, &params.numpkts))
		return -EINVAL;

	for (i = 0; i < WMI_SMART_MAX_RATE_SERIES; i++) {
		params.rate_array[i] = params.rate_array[0];
		params.antenna_array[i] = params.antenna_array[0];
	}

	if (params.antenna_array[0] >
	    ((1 << ar->ab->target_caps.num_rf_chains) - 1)) {
		ath11k_err(ar->ab, "Invalid tx ant for trianing\n");
		return -EINVAL;
	}

	rcu_read_lock();
	sta = ieee80211_find_sta_by_ifaddr(ar->hw, mac_addr, NULL);
	if (!sta) {
		ath11k_err(ar->ab, "Sta entry not found\n");
		rcu_read_unlock();
		return -EINVAL;
	}

	for (i = 0; i <= sta->deflink.bandwidth; i++)
		rate_mask |= (0xff << (8 * i));

	if ((params.rate_array[0] & rate_mask) != params.rate_array[0]) {
		ath11k_err(ar->ab, "Invalid rates for training\n");
		rcu_read_unlock();
		return -EINVAL;
	}

	arsta = (struct ath11k_sta *)sta->drv_priv;
	vdev_id = arsta->arvif->vdev_id;

	rcu_read_unlock();

	ath11k_dbg(ar->ab, ATH11K_DBG_SMART_ANT, "Training for peer %pM num pkts:%d\n",
		   mac_addr, params.numpkts);

	for (i = 0; i < WMI_SMART_MAX_RATE_SERIES; i++) {
		ath11k_dbg(ar->ab, ATH11K_DBG_SMART_ANT, "rate[%d] 0x%x antenna[%d] %d\n",
			   i, params.rate_array[i], i, params.antenna_array[i]);
	}

	mutex_lock(&ar->conf_mutex);
	ret = ath11k_wmi_peer_set_smart_ant_train_info(ar, vdev_id, mac_addr,
						       &params);
	mutex_unlock(&ar->conf_mutex);
	if (!ret)
		ret = count;

	return ret;
}

static const struct file_operations fops_sa_train_info_ops = {
	.write = ath11k_write_sa_train_info,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

void ath11k_smart_ant_debugfs_init(struct ath11k *ar)
{
	ar->debug.debugfs_smartant = debugfs_create_dir("smart_antenna",
							ar->debug.debugfs_pdev);

	debugfs_create_file("smart_ant_enable", S_IRUSR | S_IWUSR,
			    ar->debug.debugfs_smartant, ar, &fops_sa_enable_ops);

	debugfs_create_file("smart_ant_tx_ant", S_IWUSR,
			    ar->debug.debugfs_smartant, ar, &fops_sa_tx_ant);

	debugfs_create_file("smart_ant_rx_ant", S_IRUSR | S_IWUSR,
			    ar->debug.debugfs_smartant, ar, &fops_sa_rx_ant);

	debugfs_create_file("smart_ant_train_info", S_IWUSR,
			    ar->debug.debugfs_smartant, ar, &fops_sa_train_info_ops);

}
