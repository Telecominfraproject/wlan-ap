// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/of.h>

#include <linux/vmalloc.h>

#include "debugfs.h"

#include "core.h"
#include "debug.h"
#include "wmi.h"
#include "hal_rx.h"
#include "dp_tx.h"
#include "debugfs_htt_stats.h"
#include "peer.h"
#include "hif.h"
#include "pktlog.h"
#include "qmi.h"

struct dentry *debugfs_ath11k;
struct dentry *debugfs_debug_infra;

static const char *htt_bp_umac_ring[HTT_SW_UMAC_RING_IDX_MAX] = {
	"REO2SW1_RING",
	"REO2SW2_RING",
	"REO2SW3_RING",
	"REO2SW4_RING",
	"WBM2REO_LINK_RING",
	"REO2TCL_RING",
	"REO2FW_RING",
	"RELEASE_RING",
	"PPE_RELEASE_RING",
	"TCL2TQM_RING",
	"TQM_RELEASE_RING",
	"REO_RELEASE_RING",
	"WBM2SW0_RELEASE_RING",
	"WBM2SW1_RELEASE_RING",
	"WBM2SW2_RELEASE_RING",
	"WBM2SW3_RELEASE_RING",
	"REO_CMD_RING",
	"REO_STATUS_RING",
};

static const char *htt_bp_lmac_ring[HTT_SW_LMAC_RING_IDX_MAX] = {
	"FW2RXDMA_BUF_RING",
	"FW2RXDMA_STATUS_RING",
	"FW2RXDMA_LINK_RING",
	"SW2RXDMA_BUF_RING",
	"WBM2RXDMA_LINK_RING",
	"RXDMA2FW_RING",
	"RXDMA2SW_RING",
	"RXDMA2RELEASE_RING",
	"RXDMA2REO_RING",
	"MONITOR_STATUS_RING",
	"MONITOR_BUF_RING",
	"MONITOR_DESC_RING",
	"MONITOR_DEST_RING",
};

void ath11k_debugfs_add_dbring_entry(struct ath11k *ar,
				     enum wmi_direct_buffer_module id,
				     enum ath11k_dbg_dbr_event event,
				     struct hal_srng *srng)
{
	struct ath11k_debug_dbr *dbr_debug;
	struct ath11k_dbg_dbr_data *dbr_data;
	struct ath11k_dbg_dbr_entry *entry;

	if (id >= WMI_DIRECT_BUF_MAX || event >= ATH11K_DBG_DBR_EVENT_MAX)
		return;

	dbr_debug = ar->debug.dbr_debug[id];
	if (!dbr_debug)
		return;

	if (!dbr_debug->dbr_debug_enabled)
		return;

	dbr_data = &dbr_debug->dbr_dbg_data;

	spin_lock_bh(&dbr_data->lock);

	if (dbr_data->entries) {
		entry = &dbr_data->entries[dbr_data->dbr_debug_idx];
		entry->hp = srng->u.src_ring.hp;
		entry->tp = *srng->u.src_ring.tp_addr;
		entry->timestamp = jiffies;
		entry->event = event;

		dbr_data->dbr_debug_idx++;
		if (dbr_data->dbr_debug_idx ==
		    dbr_data->num_ring_debug_entries)
			dbr_data->dbr_debug_idx = 0;
	}

	spin_unlock_bh(&dbr_data->lock);
}

static void ath11k_debugfs_fw_stats_reset(struct ath11k *ar)
{
	spin_lock_bh(&ar->data_lock);
	ar->fw_stats_done = false;
	ath11k_fw_stats_pdevs_free(&ar->fw_stats.pdevs);
	ath11k_fw_stats_vdevs_free(&ar->fw_stats.vdevs);
	spin_unlock_bh(&ar->data_lock);
}

void ath11k_debugfs_fw_stats_process(struct ath11k *ar, struct ath11k_fw_stats *stats)
{
	struct ath11k_base *ab = ar->ab;
	struct ath11k_pdev *pdev;
	bool is_end;
	static unsigned int num_vdev, num_bcn;
	size_t total_vdevs_started = 0;
	int i;

	/* WMI_REQUEST_PDEV_STAT request has been already processed */

	if (stats->stats_id == WMI_REQUEST_RSSI_PER_CHAIN_STAT) {
		ar->fw_stats_done = true;
		return;
	}

	if (stats->stats_id == WMI_REQUEST_VDEV_STAT) {
		if (list_empty(&stats->vdevs)) {
			ath11k_warn(ab, "empty vdev stats");
			return;
		}
		/* FW sends all the active VDEV stats irrespective of PDEV,
		 * hence limit until the count of all VDEVs started
		 */
		for (i = 0; i < ab->num_radios; i++) {
			pdev = rcu_dereference(ab->pdevs_active[i]);
			if (pdev && pdev->ar)
				total_vdevs_started += ar->num_started_vdevs;
		}

		is_end = ((++num_vdev) == total_vdevs_started);

		list_splice_tail_init(&stats->vdevs,
				      &ar->fw_stats.vdevs);

		if (is_end) {
			ar->fw_stats_done = true;
			num_vdev = 0;
		}
		return;
	}

	if (stats->stats_id == WMI_REQUEST_BCN_STAT) {
		if (list_empty(&stats->bcn)) {
			ath11k_warn(ab, "empty bcn stats");
			return;
		}
		/* Mark end until we reached the count of all started VDEVs
		 * within the PDEV
		 */
		is_end = ((++num_bcn) == ar->num_started_vdevs);

		list_splice_tail_init(&stats->bcn,
				      &ar->fw_stats.bcn);

		if (is_end) {
			ar->fw_stats_done = true;
			num_bcn = 0;
		}
	}
}

static int ath11k_debugfs_fw_stats_request(struct ath11k *ar,
					   struct stats_request_params *req_param)
{
	struct ath11k_base *ab = ar->ab;
	unsigned long timeout, time_left;
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	/* FW stats can get split when exceeding the stats data buffer limit.
	 * In that case, since there is no end marking for the back-to-back
	 * received 'update stats' event, we keep a 3 seconds timeout in case,
	 * fw_stats_done is not marked yet
	 */
#if LINUX_VERSION_IS_GEQ(6,13,0)
	timeout = jiffies + secs_to_jiffies(3);
#else
	timeout = jiffies + msecs_to_jiffies(3 * 1000);
#endif

	ath11k_debugfs_fw_stats_reset(ar);

	reinit_completion(&ar->fw_stats_complete);

	ret = ath11k_wmi_send_stats_request_cmd(ar, req_param);

	if (ret) {
		ath11k_warn(ab, "could not request fw stats (%d)\n",
			    ret);
		return ret;
	}

	time_left = wait_for_completion_timeout(&ar->fw_stats_complete, 1 * HZ);

	if (!time_left)
		return -ETIMEDOUT;

	for (;;) {
		if (time_after(jiffies, timeout))
			break;

		spin_lock_bh(&ar->data_lock);
		if (ar->fw_stats_done) {
			spin_unlock_bh(&ar->data_lock);
			break;
		}
		spin_unlock_bh(&ar->data_lock);
	}
	return 0;
}

int ath11k_debugfs_get_fw_stats(struct ath11k *ar, u32 pdev_id,
				u32 vdev_id, u32 stats_id)
{
	struct ath11k_base *ab = ar->ab;
	struct stats_request_params req_param;
	int ret;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ret = -ENETDOWN;
		goto err_unlock;
	}

	req_param.pdev_id = pdev_id;
	req_param.vdev_id = vdev_id;
	req_param.stats_id = stats_id;

	ret = ath11k_debugfs_fw_stats_request(ar, &req_param);
	if (ret)
		ath11k_warn(ab, "failed to request fw stats: %d\n", ret);

	ath11k_dbg(ab, ATH11K_DBG_WMI,
		   "debug get fw stat pdev id %d vdev id %d stats id 0x%x\n",
		   pdev_id, vdev_id, stats_id);

err_unlock:
	mutex_unlock(&ar->conf_mutex);

	return ret;
}

static int ath11k_open_pdev_stats(struct inode *inode, struct file *file)
{
	struct ath11k *ar = inode->i_private;
	struct ath11k_base *ab = ar->ab;
	struct stats_request_params req_param;
	void *buf = NULL;
	int ret;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ret = -ENETDOWN;
		goto err_unlock;
	}

	buf = vmalloc(ATH11K_FW_STATS_BUF_SIZE);
	if (!buf) {
		ret = -ENOMEM;
		goto err_unlock;
	}

	req_param.pdev_id = ar->pdev->pdev_id;
	req_param.vdev_id = 0;
	req_param.stats_id = WMI_REQUEST_PDEV_STAT;

	ret = ath11k_debugfs_fw_stats_request(ar, &req_param);
	if (ret) {
		ath11k_warn(ab, "failed to request fw pdev stats: %d\n", ret);
		goto err_free;
	}

	ath11k_wmi_fw_stats_fill(ar, &ar->fw_stats, req_param.stats_id, buf);

	file->private_data = buf;

	mutex_unlock(&ar->conf_mutex);
	return 0;

err_free:
	vfree(buf);

err_unlock:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static int ath11k_release_pdev_stats(struct inode *inode, struct file *file)
{
	vfree(file->private_data);

	return 0;
}

static ssize_t ath11k_read_pdev_stats(struct file *file,
				      char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	const char *buf = file->private_data;
	size_t len = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_pdev_stats = {
	.open = ath11k_open_pdev_stats,
	.release = ath11k_release_pdev_stats,
	.read = ath11k_read_pdev_stats,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_read_wmm_stats(struct file *file,
				     char __user *ubuf,
				     size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	int len = 0;
	int size = 2048;
	char *buf;
	ssize_t retval;
	u64 total_wmm_sent_pkts = 0;
	u64 total_wmm_received_pkts = 0;
	u64 total_wmm_fail_sent = 0;
	u64 total_wmm_fail_received = 0;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	mutex_lock(&ar->conf_mutex);
	for (count = 0; count < WME_NUM_AC; count++) {
		total_wmm_sent_pkts += ar->wmm_stats.total_wmm_tx_pkts[count];
		total_wmm_received_pkts += ar->wmm_stats.total_wmm_rx_pkts[count];
		total_wmm_fail_sent += ar->wmm_stats.total_wmm_tx_drop[count];
		total_wmm_fail_received += ar->wmm_stats.total_wmm_rx_drop[count];
	}

	len += scnprintf(buf + len, size - len, "total number of wmm_sent: %llu\n",
			 total_wmm_sent_pkts);
	len += scnprintf(buf + len, size - len, "total number of wmm_received: %llu\n",
			 total_wmm_received_pkts);
	len += scnprintf(buf + len, size - len, "total number of wmm_fail_sent: %llu\n",
			 total_wmm_fail_sent);
	len += scnprintf(buf + len, size - len, "total number of wmm_fail_received: %llu\n",
			 total_wmm_fail_received);
	len += scnprintf(buf + len, size - len, "num of be wmm_sent: %llu\n",
			 ar->wmm_stats.total_wmm_tx_pkts[WME_AC_BE]);
	len += scnprintf(buf + len, size - len, "num of bk wmm_sent: %llu\n",
			 ar->wmm_stats.total_wmm_tx_pkts[WME_AC_BK]);
	len += scnprintf(buf + len, size - len, "num of vi wmm_sent: %llu\n",
			 ar->wmm_stats.total_wmm_tx_pkts[WME_AC_VI]);
	len += scnprintf(buf + len, size - len, "num of vo wmm_sent: %llu\n",
			 ar->wmm_stats.total_wmm_tx_pkts[WME_AC_VO]);
	len += scnprintf(buf + len, size - len, "num of be wmm_received: %llu\n",
			 ar->wmm_stats.total_wmm_rx_pkts[WME_AC_BE]);
	len += scnprintf(buf + len, size - len, "num of bk wmm_received: %llu\n",
			 ar->wmm_stats.total_wmm_rx_pkts[WME_AC_BK]);
	len += scnprintf(buf + len, size - len, "num of vi wmm_received: %llu\n",
			 ar->wmm_stats.total_wmm_rx_pkts[WME_AC_VI]);
	len += scnprintf(buf + len, size - len, "num of vo wmm_received: %llu\n",
			 ar->wmm_stats.total_wmm_rx_pkts[WME_AC_VO]);
	len += scnprintf(buf + len, size - len, "num of be wmm_tx_dropped: %llu\n",
			 ar->wmm_stats.total_wmm_tx_drop[WME_AC_BE]);
	len += scnprintf(buf + len, size - len, "num of bk wmm_tx_dropped: %llu\n",
			 ar->wmm_stats.total_wmm_tx_drop[WME_AC_BK]);
	len += scnprintf(buf + len, size - len, "num of vi wmm_tx_dropped: %llu\n",
			 ar->wmm_stats.total_wmm_tx_drop[WME_AC_VI]);
	len += scnprintf(buf + len, size - len, "num of vo wmm_tx_dropped: %llu\n",
			 ar->wmm_stats.total_wmm_tx_drop[WME_AC_VO]);
	len += scnprintf(buf + len, size - len, "num of be wmm_rx_dropped: %llu\n",
			 ar->wmm_stats.total_wmm_rx_drop[WME_AC_BE]);
	len += scnprintf(buf + len, size - len, "num of bk wmm_rx_dropped: %llu\n",
			 ar->wmm_stats.total_wmm_rx_drop[WME_AC_BK]);
	len += scnprintf(buf + len, size - len, "num of vi wmm_rx_dropped: %llu\n",
			 ar->wmm_stats.total_wmm_rx_drop[WME_AC_VI]);
	len += scnprintf(buf + len, size - len, "num of vo wmm_rx_dropped: %llu\n",
			 ar->wmm_stats.total_wmm_rx_drop[WME_AC_VO]);

	mutex_unlock(&ar->conf_mutex);

	if (len > size)
		len = size;
	retval = simple_read_from_buffer(ubuf, count, ppos, buf, len);
	kfree(buf);

	return retval;
}

static const struct file_operations fops_wmm_stats = {
	.read = ath11k_read_wmm_stats,
	.open = simple_open,
};

static int ath11k_open_vdev_stats(struct inode *inode, struct file *file)
{
	struct ath11k *ar = inode->i_private;
	struct stats_request_params req_param;
	void *buf = NULL;
	int ret;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ret = -ENETDOWN;
		goto err_unlock;
	}

	buf = vmalloc(ATH11K_FW_STATS_BUF_SIZE);
	if (!buf) {
		ret = -ENOMEM;
		goto err_unlock;
	}

	req_param.pdev_id = ar->pdev->pdev_id;
	/* VDEV stats is always sent for all active VDEVs from FW */
	req_param.vdev_id = 0;
	req_param.stats_id = WMI_REQUEST_VDEV_STAT;

	ret = ath11k_debugfs_fw_stats_request(ar, &req_param);
	if (ret) {
		ath11k_warn(ar->ab, "failed to request fw vdev stats: %d\n", ret);
		goto err_free;
	}

	ath11k_wmi_fw_stats_fill(ar, &ar->fw_stats, req_param.stats_id, buf);

	file->private_data = buf;

	mutex_unlock(&ar->conf_mutex);
	return 0;

err_free:
	vfree(buf);

err_unlock:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static int ath11k_release_vdev_stats(struct inode *inode, struct file *file)
{
	vfree(file->private_data);

	return 0;
}

static ssize_t ath11k_read_vdev_stats(struct file *file,
				      char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	const char *buf = file->private_data;
	size_t len = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_vdev_stats = {
	.open = ath11k_open_vdev_stats,
	.release = ath11k_release_vdev_stats,
	.read = ath11k_read_vdev_stats,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static int ath11k_open_bcn_stats(struct inode *inode, struct file *file)
{
	struct ath11k *ar = inode->i_private;
	struct ath11k_vif *arvif;
	struct stats_request_params req_param;
	void *buf = NULL;
	int ret;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ret = -ENETDOWN;
		goto err_unlock;
	}

	buf = vmalloc(ATH11K_FW_STATS_BUF_SIZE);
	if (!buf) {
		ret = -ENOMEM;
		goto err_unlock;
	}

	req_param.stats_id = WMI_REQUEST_BCN_STAT;
	req_param.pdev_id = ar->pdev->pdev_id;

	/* loop all active VDEVs for bcn stats */
	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (!arvif->is_up)
			continue;

		req_param.vdev_id = arvif->vdev_id;
		ret = ath11k_debugfs_fw_stats_request(ar, &req_param);
		if (ret) {
			ath11k_warn(ar->ab, "failed to request fw bcn stats: %d\n", ret);
			goto err_free;
		}
	}

	ath11k_wmi_fw_stats_fill(ar, &ar->fw_stats, req_param.stats_id, buf);

	/* since beacon stats request is looped for all active VDEVs, saved fw
	 * stats is not freed for each request until done for all active VDEVs
	 */
	spin_lock_bh(&ar->data_lock);
	ath11k_fw_stats_bcn_free(&ar->fw_stats.bcn);
	spin_unlock_bh(&ar->data_lock);

	file->private_data = buf;

	mutex_unlock(&ar->conf_mutex);
	return 0;

err_free:
	vfree(buf);

err_unlock:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static int ath11k_release_bcn_stats(struct inode *inode, struct file *file)
{
	vfree(file->private_data);

	return 0;
}

static ssize_t ath11k_read_bcn_stats(struct file *file,
				     char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	const char *buf = file->private_data;
	size_t len = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_bcn_stats = {
	.open = ath11k_open_bcn_stats,
	.release = ath11k_release_bcn_stats,
	.read = ath11k_read_bcn_stats,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_read_simulate_fw_crash(struct file *file,
					     char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	const char buf[] =
		"To simulate firmware crash write one of the keywords to this file:\n"
		"`assert` - this will send WMI_FORCE_FW_HANG_CMDID to firmware to cause assert.\n"
		"`hw-restart` - this will simply queue hw restart without fw/hw actually crashing.\n";

	return simple_read_from_buffer(user_buf, count, ppos, buf, strlen(buf));
}

/* Simulate firmware crash:
 * 'soft': Call wmi command causing firmware hang. This firmware hang is
 * recoverable by warm firmware reset.
 * 'hard': Force firmware crash by setting any vdev parameter for not allowed
 * vdev id. This is hard firmware crash because it is recoverable only by cold
 * firmware reset.
 */
static ssize_t ath11k_write_simulate_fw_crash(struct file *file,
					      const char __user *user_buf,
					      size_t count, loff_t *ppos)
{
	struct ath11k_base *ab = file->private_data;
	struct ath11k_pdev *pdev;
	struct ath11k *ar = ab->pdevs[0].ar;
	char buf[32] = {0};
	ssize_t rc;
	int i, ret, radioup = 0;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		ar = pdev->ar;
		if (ar && ar->state == ATH11K_STATE_ON) {
			radioup = 1;
			break;
		}
	}
	/* filter partial writes and invalid commands */
	if (*ppos != 0 || count >= sizeof(buf) || count == 0)
		return -EINVAL;

	rc = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (rc < 0)
		return rc;

	/* drop the possible '\n' from the end */
	if (buf[*ppos - 1] == '\n')
		buf[*ppos - 1] = '\0';

	if (radioup == 0) {
		ret = -ENETDOWN;
		goto exit;
	}

	if (!strcmp(buf, "assert")) {
		ath11k_info(ab, "simulating firmware assert crash\n");
		ret = ath11k_wmi_force_fw_hang_cmd(ar,
						   ATH11K_WMI_FW_HANG_ASSERT_TYPE,
						   ATH11K_WMI_FW_HANG_DELAY);
	} else if (!strcmp(buf, "hw-restart")) {
		ath11k_info(ab, "user requested hw restart\n");
		queue_work(ab->workqueue_aux, &ab->reset_work);
		ret = 0;
	} else {
		ret = -EINVAL;
		goto exit;
	}

	if (ret) {
		ath11k_warn(ab, "failed to simulate firmware crash: %d\n", ret);
		goto exit;
	}

	ret = count;

exit:
	return ret;
}

static const struct file_operations fops_simulate_fw_crash = {
	.read = ath11k_read_simulate_fw_crash,
	.write = ath11k_write_simulate_fw_crash,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_read_trace_qdss(struct file *file,
				      char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	const char buf[] =
	"'1` - this will start qdss trace collection\n"
	"`0` - this will stop and save the qdss trace collection\n";

	return simple_read_from_buffer(user_buf, count, ppos, buf, strlen(buf));
}

static ssize_t
ath11k_write_trace_qdss(struct file *file,
			const char __user *user_buf,
			size_t count, loff_t *ppos)
{
	struct ath11k_base *ab = file->private_data;
	struct ath11k_pdev *pdev;
	struct ath11k *ar;
	int i, ret, radioup = 0;
	bool qdss_enable;

	if (kstrtobool_from_user(user_buf, count, &qdss_enable))
		return -EINVAL;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		ar = pdev->ar;
		if (ar && ar->state == ATH11K_STATE_ON) {
			radioup = 1;
			break;
		}
	}
	if (radioup == 0) {
		ath11k_err(ab, "radio is not up\n");
		ret = -ENETDOWN;
		goto exit;
	}

	if (qdss_enable) {
		if (ab->is_qdss_tracing) {
			ret = count;
			goto exit;
		}
		ath11k_config_qdss(ab);
	} else {
		if (!ab->is_qdss_tracing) {
			ret = count;
			goto exit;
		}
		if (!ab->hw_params.fixed_bdf_addr) {
			ret = ath11k_send_qdss_trace_mode_req(ab,
							      QMI_WLANFW_QDSS_TRACE_OFF_V01);
			if (ret < 0)
				ath11k_warn(ab,
					    "Failed to trace QDSS: %d\n", ret);
		}
	}

	ret = count;

exit:
	return ret;
}

static const struct file_operations fops_trace_qdss = {
	.read = ath11k_read_trace_qdss,
	.write = ath11k_write_trace_qdss,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_write_enable_extd_tx_stats(struct file *file,
						 const char __user *ubuf,
						 size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	u32 filter;
	int ret;

	if (kstrtouint_from_user(ubuf, count, 0, &filter))
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ret = -ENETDOWN;
		goto out;
	}

	if (filter == ar->debug.extd_tx_stats) {
		ret = count;
		goto out;
	}

	ar->debug.extd_tx_stats = filter;
	ret = count;

out:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static ssize_t ath11k_read_enable_extd_tx_stats(struct file *file,
						char __user *ubuf,
						size_t count, loff_t *ppos)

{
	char buf[32] = {0};
	struct ath11k *ar = file->private_data;
	int len = 0;

	mutex_lock(&ar->conf_mutex);
	len = scnprintf(buf, sizeof(buf) - len, "%08x\n",
			ar->debug.extd_tx_stats);
	mutex_unlock(&ar->conf_mutex);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_extd_tx_stats = {
	.read = ath11k_read_enable_extd_tx_stats,
	.write = ath11k_write_enable_extd_tx_stats,
	.open = simple_open
};

static ssize_t ath11k_write_extd_rx_stats(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	struct ath11k_base *ab = ar->ab;
	struct htt_rx_ring_tlv_filter tlv_filter = {0};
	u32 enable, rx_filter = 0, ring_id;
	int i;
	int ret;

	if (kstrtouint_from_user(ubuf, count, 0, &enable))
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ret = -ENETDOWN;
		goto exit;
	}

	if (enable > 1) {
		ret = -EINVAL;
		goto exit;
	}

	if (enable == ar->debug.extd_rx_stats) {
		ret = count;
		goto exit;
	}

	if (test_bit(ATH11K_FLAG_MONITOR_STARTED, &ar->monitor_flags)) {
		ar->debug.extd_rx_stats = enable;
		ret = count;
		goto exit;
	}

	if (test_bit(ATH11K_FLAG_MONITOR_STARTED, &ar->monitor_flags)) {
		ar->debug.extd_rx_stats = enable;
		ret = count;
		goto exit;
	}

	if (enable) {
		rx_filter =  HTT_RX_FILTER_TLV_FLAGS_MPDU_START;
		rx_filter |= HTT_RX_FILTER_TLV_FLAGS_PPDU_START;
		rx_filter |= HTT_RX_FILTER_TLV_FLAGS_PPDU_END;
		rx_filter |= HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS;
		rx_filter |= HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS_EXT;
		rx_filter |= HTT_RX_FILTER_TLV_FLAGS_PPDU_END_STATUS_DONE;

		tlv_filter.rx_filter = rx_filter;
		tlv_filter.pkt_filter_flags0 = HTT_RX_FP_MGMT_FILTER_FLAGS0;
		tlv_filter.pkt_filter_flags1 = HTT_RX_FP_MGMT_FILTER_FLAGS1;
		tlv_filter.pkt_filter_flags2 = HTT_RX_FP_CTRL_FILTER_FLASG2;
		tlv_filter.pkt_filter_flags3 = HTT_RX_FP_CTRL_FILTER_FLASG3 |
			HTT_RX_FP_DATA_FILTER_FLASG3;
	} else {
		tlv_filter = ath11k_mac_mon_status_filter_default;
		ath11k_nss_ext_rx_stats(ar->ab, &tlv_filter);
	}

	ar->debug.rx_filter = tlv_filter.rx_filter;
	tlv_filter.offset_valid = false;

	for (i = 0; i < ab->hw_params.num_rxdma_per_pdev; i++) {
		ring_id = ar->dp.rx_mon_status_refill_ring[i].refill_buf_ring.ring_id;
		ret = ath11k_dp_tx_htt_rx_filter_setup(ar->ab, ring_id, ar->dp.mac_id,
						       HAL_RXDMA_MONITOR_STATUS,
						       DP_RX_BUFFER_SIZE, &tlv_filter);

		if (ret) {
			ath11k_warn(ar->ab, "failed to set rx filter for monitor status ring\n");
			goto exit;
		}
	}

	ar->debug.extd_rx_stats = enable;
	ret = count;
exit:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static ssize_t ath11k_read_extd_rx_stats(struct file *file,
					 char __user *ubuf,
					 size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	char buf[32];
	int len = 0;

	mutex_lock(&ar->conf_mutex);
	len = scnprintf(buf, sizeof(buf) - len, "%d\n",
			ar->debug.extd_rx_stats);
	mutex_unlock(&ar->conf_mutex);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_extd_rx_stats = {
	.read = ath11k_read_extd_rx_stats,
	.write = ath11k_write_extd_rx_stats,
	.open = simple_open,
};

static int ath11k_reset_nrp_filter(struct ath11k *ar,
				   bool reset)
{
	int i = 0;
	int ret = 0;
	u32 ring_id = 0;
	struct htt_rx_ring_tlv_filter tlv_filter = {0};
	struct ath11k_pdev_dp *dp = &ar->dp;
	struct ath11k_base *ab = ar->ab;

	if (ab->hw_params.full_monitor_mode) {
		ret = ath11k_dp_tx_htt_rx_full_mon_setup(ab,
							 dp->mac_id, !reset);
		if (ret < 0) {
			ath11k_err(ab, "failed to setup full monitor %d\n", ret);
			return ret;
		}
	}

	ring_id = dp->rxdma_mon_buf_ring.refill_buf_ring.ring_id;
	tlv_filter.offset_valid = false;
	if (!reset) {
		tlv_filter.pkt_filter_flags0 =
			HTT_RX_MON_MO_MGMT_FILTER_FLAGS0;
		tlv_filter.pkt_filter_flags1 =
			HTT_RX_MON_MO_MGMT_FILTER_FLAGS1;
		tlv_filter.pkt_filter_flags2 =
			HTT_RX_MON_MO_CTRL_FILTER_FLASG2;
		tlv_filter.pkt_filter_flags3 =
			HTT_RX_MON_MO_CTRL_FILTER_FLASG3 |
			HTT_RX_MON_MO_DATA_FILTER_FLASG3;
	}

	if (ar->ab->hw_params.rxdma1_enable) {
		ret = ath11k_dp_tx_htt_rx_filter_setup(ar->ab, ring_id, ar->dp.mac_id,
						       HAL_RXDMA_MONITOR_BUF,
						       DP_RXDMA_REFILL_RING_SIZE,
						       &tlv_filter);
	} else if (!reset) {
		for (i = 0; i < ar->ab->hw_params.num_rxdma_per_pdev; i++) {
			ring_id = ar->dp.rx_mac_buf_ring[i].ring_id;
			ret = ath11k_dp_tx_htt_rx_filter_setup(ar->ab, ring_id,
							       ar->dp.mac_id + i,
							       HAL_RXDMA_BUF,
							       1024,
							       &tlv_filter);
		}
	}

	if (ret)
		return ret;

	for (i = 0; i < ar->ab->hw_params.num_rxdma_per_pdev; i++) {
		ring_id = ar->dp.rx_mon_status_refill_ring[i].refill_buf_ring.ring_id;
		if (!reset) {
			tlv_filter.rx_filter =
				HTT_RX_MON_FILTER_TLV_FLAGS_MON_STATUS_RING;
		} else {
			tlv_filter = ath11k_mac_mon_status_filter_default;

			if (ath11k_debugfs_is_extd_rx_stats_enabled(ar))
				tlv_filter.rx_filter = ath11k_debugfs_rx_filter(ar);
		}

		ret = ath11k_dp_tx_htt_rx_filter_setup(ar->ab, ring_id,
						       ar->dp.mac_id + i,
						       HAL_RXDMA_MONITOR_STATUS,
						       DP_RXDMA_REFILL_RING_SIZE,
						       &tlv_filter);
	}

	if (!ar->ab->hw_params.rxdma1_enable)
		mod_timer(&ar->ab->mon_reap_timer, jiffies +
			  msecs_to_jiffies(ATH11K_MON_TIMER_INTERVAL));

	return ret;
}

void ath11k_debugfs_nrp_cleanup_all(struct ath11k *ar)
{
	struct ath11k_base *ab = ar->ab;
	struct ath11k_neighbor_peer *nrp, *tmp;

	spin_lock_bh(&ab->base_lock);
	list_for_each_entry_safe(nrp, tmp, &ab->neighbor_peers, list) {
		if (nrp->is_filter_on)
			complete(&nrp->filter_done);
		list_del(&nrp->list);
		kfree(nrp);
	}

	ab->num_nrps = 0;
	spin_unlock_bh(&ab->base_lock);

	debugfs_remove_recursive(ar->debug.debugfs_nrp);
}

void ath11k_debugfs_nrp_clean(struct ath11k *ar, const u8 *addr)
{
	int i, j;
	char fname[MAC_UNIT_LEN * ETH_ALEN] = {0};

	for (i = 0, j = 0; i < (MAC_UNIT_LEN * ETH_ALEN); i += MAC_UNIT_LEN, j++) {
		if (j == ETH_ALEN - 1) {
			snprintf(fname + i, sizeof(fname) - i, "%02x", *(addr + j));
			break;
		}
		snprintf(fname + i, sizeof(fname) - i, "%02x:", *(addr + j));
	}

	spin_lock_bh(&ar->ab->base_lock);
	ar->ab->num_nrps--;
	spin_unlock_bh(&ar->ab->base_lock);

	debugfs_lookup_and_remove(fname, ar->debug.debugfs_nrp);
	if (!ar->ab->num_nrps) {
		debugfs_remove_recursive(ar->debug.debugfs_nrp);
		ath11k_reset_nrp_filter(ar, true);
	}
}

static ssize_t ath11k_read_nrp_rssi(struct file *file,
				    char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	struct ath11k_base *ab = ar->ab;
	struct ath11k_vif *arvif = NULL;
	struct ath11k_neighbor_peer *nrp = NULL, *tmp;
	struct peer_create_params peer_param = {0};
	u8 macaddr[ETH_ALEN] = {0};
	loff_t file_pos = *ppos;
	struct path *fpath = &file->f_path;
	char *fname = fpath->dentry->d_iname;
	char buf[128] = {0};
	int i = 0;
	int j = 0;
	int len = 0;
	int vdev_id = -1;
	bool nrp_found = false;
	int ret;

	mutex_lock(&ar->conf_mutex);
	if (ar->state != ATH11K_STATE_ON) {
		mutex_unlock(&ar->conf_mutex);
		return -ENETDOWN;
	}
	mutex_unlock(&ar->conf_mutex);

	if (file_pos > 0)
		return 0;

	mutex_lock(&ar->conf_mutex);
	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (arvif->vdev_type == WMI_VDEV_TYPE_AP) {
			vdev_id = arvif->vdev_id;
			break;
		}
	}
	mutex_unlock(&ar->conf_mutex);

	if (vdev_id < 0) {
		ath11k_warn(ab, "unable to get vdev for AP interface\n");
		return 0;
	}

	for (i = 0, j = 0;  i < MAC_UNIT_LEN * ETH_ALEN; i += MAC_UNIT_LEN, j++) {
		if (sscanf(fname + i, "%hhX", &macaddr[j]) <= 0)
			return -EINVAL;
	}

	spin_lock_bh(&ab->base_lock);
	list_for_each_entry(nrp, &ab->neighbor_peers, list) {
		if (ether_addr_equal(macaddr, nrp->addr)) {
			reinit_completion(&nrp->filter_done);
			nrp->vdev_id = vdev_id;
			nrp->is_filter_on = false;
			break;
		}
	}
	spin_unlock_bh(&ab->base_lock);

	peer_param.vdev_id = nrp->vdev_id;
	peer_param.peer_addr = nrp->addr;
	peer_param.peer_type = WMI_PEER_TYPE_DEFAULT;

	ret = ath11k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SET_PROMISC_MODE_CMDID,
					1, ar->pdev->pdev_id);
	if (ret)
		ath11k_err(ar->ab, "failed to enable promisc mode: %d\n", ret);

	if (!ath11k_peer_create(ar, arvif, NULL, &peer_param)) {
		spin_lock_bh(&ab->base_lock);
		list_for_each_entry_safe(nrp, tmp, &ab->neighbor_peers, list) {
			if (ether_addr_equal(nrp->addr, peer_param.peer_addr)) {
				nrp_found = true;
				break;
			}
		}
		spin_unlock_bh(&ab->base_lock);

		if (nrp_found) {
			spin_lock_bh(&ab->base_lock);
			nrp->is_filter_on = true;
			spin_unlock_bh(&ab->base_lock);

			wait_for_completion_interruptible_timeout(&nrp->filter_done, 15 * HZ);

			spin_lock_bh(&ab->base_lock);
			nrp->is_filter_on = false;
			spin_unlock_bh(&ab->base_lock);

			len = scnprintf(buf, sizeof(buf),
					"Neighbor Peer MAC\t\tRSSI\t\tTime\n");
			len += scnprintf(buf + len, sizeof(buf) - len, "%pM\t\t%u\t\t%lld\n",
					 nrp->addr, nrp->rssi, nrp->timestamp);
		} else {
			ath11k_peer_delete(ar, vdev_id, macaddr);
			ath11k_warn(ab, "%pM not found in nrp list\n", macaddr);
			return -EINVAL;
		}

		ret = ath11k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_SET_PROMISC_MODE_CMDID,
						0, ar->pdev->pdev_id);
		if (ret)
			ath11k_err(ar->ab, "failed to disable promisc mode: %d\n", ret);

		ath11k_peer_delete(ar, vdev_id, macaddr);
	} else {
		ath11k_warn(ab, "unable to create peer for nrp[%pM]\n", macaddr);
		return -EINVAL;
	}

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_read_nrp_rssi = {
	.read = ath11k_read_nrp_rssi,
	.open = simple_open,
};

static ssize_t ath11k_write_nrp_mac(struct file *file,
				    const char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	struct ath11k_base *ab = ar->ab;
	struct ath11k_peer *peer = NULL;
	struct ath11k_neighbor_peer *nrp = NULL, *tmp = NULL;
	u8 mac[ETH_ALEN] = {0};
	char fname[MAC_UNIT_LEN * ETH_ALEN] = {0};
	char *str = NULL;
	char *buf = vmalloc(count);
	char *ptr = buf;
	int i = 0;
	int j = 0;
	int ret = count;
	int action = 0;
	ssize_t rc = 0;
	bool del_nrp = false;

	mutex_lock(&ar->conf_mutex);

	rc = simple_write_to_buffer(buf, count, ppos, ubuf, count);
	if (rc <= 0)
		goto exit;

	buf[count - 1] = '\0';

	if (ar->state != ATH11K_STATE_ON) {
		ret = -ENETDOWN;
		goto exit;
	}

	str = strsep(&buf, ",");
	if (!strcmp(str, "add"))
		action = NRP_ACTION_ADD;
	else if (!strcmp(str, "del"))
		action = NRP_ACTION_DEL;
	else {
		ath11k_err(ab, "error: invalid argument\n");
		goto exit;
	}

	memset(mac, 0, sizeof(mac));
	while ((str = strsep(&buf, ":")) != NULL) {
		if (i >= ETH_ALEN || kstrtou8(str, 16, mac + i)) {
			ath11k_warn(ab, "error: invalid mac address\n");
			goto exit;
		}
		i++;
	}

	if (i != ETH_ALEN) {
		ath11k_warn(ab, "error: invalid mac address\n");
		goto exit;
	}

	if (!is_valid_ether_addr(mac)) {
		ath11k_err(ab, "error: invalid mac address\n");
		goto exit;
	}

	for (i = 0, j = 0; i < (MAC_UNIT_LEN * ETH_ALEN); i += MAC_UNIT_LEN, j++) {
		if (j == ETH_ALEN - 1) {
			snprintf(fname + i, sizeof(fname) - i, "%02x", mac[j]);
			break;
		}
		snprintf(fname + i, sizeof(fname) - i, "%02x:", mac[j]);
	}

	switch (action) {
	case NRP_ACTION_ADD:
		if (ab->num_nrps == (ATH11K_MAX_NRPS - 1)) {
			ath11k_warn(ab, "max nrp reached, cannot create more\n");
			goto exit;
		}

		spin_lock_bh(&ab->base_lock);
		list_for_each_entry(nrp, &ab->neighbor_peers, list) {
			if (ether_addr_equal(nrp->addr, mac)) {
				ath11k_warn(ab, "cannot add existing neighbor peer\n");
				goto exit;
			}
		}
		spin_unlock_bh(&ab->base_lock);

		spin_lock_bh(&ab->base_lock);
		peer = ath11k_peer_find_by_addr(ab, mac);
		if (peer) {
			ath11k_warn(ab, "cannot add exisitng peer [%pM] as nrp\n", mac);
			spin_unlock_bh(&ab->base_lock);
			goto exit;
		}
		spin_unlock_bh(&ab->base_lock);

		nrp = kzalloc(sizeof(*nrp), GFP_KERNEL);
		if (!nrp)
			goto exit;

		init_completion(&nrp->filter_done);
		ether_addr_copy(nrp->addr, mac);

		spin_lock_bh(&ab->base_lock);
		list_add_tail(&nrp->list, &ab->neighbor_peers);
		spin_unlock_bh(&ab->base_lock);

		if (!ab->num_nrps) {
			ar->debug.debugfs_nrp = debugfs_create_dir("nrp_rssi",
								   ar->debug.debugfs_pdev);
			ath11k_reset_nrp_filter(ar, false);
		}
		spin_lock_bh(&ab->base_lock);
		ab->num_nrps++;
		spin_unlock_bh(&ab->base_lock);

		debugfs_create_file(fname, 0644,
				    ar->debug.debugfs_nrp, ar,
				    &fops_read_nrp_rssi);
		break;
	case NRP_ACTION_DEL:
		if (!ar->ab->num_nrps) {
			ath11k_err(ab, "error: no nac added\n");
			goto exit;
		}

		spin_lock_bh(&ab->base_lock);
		list_for_each_entry_safe(nrp, tmp, &ab->neighbor_peers, list) {
			if (ether_addr_equal(nrp->addr, mac)) {
				list_del(&nrp->list);
				kfree(nrp);
				del_nrp = true;
				break;
			}

		}
		spin_unlock_bh(&ab->base_lock);

		if (!del_nrp)
			ath11k_warn(ab, "cannot delete %pM not added to list\n", mac);
		else
			ath11k_debugfs_nrp_clean(ar, mac);
		break;
	default:
		break;
	}
 exit:
	mutex_unlock(&ar->conf_mutex);

	vfree(ptr);
	return ret;
}

static const struct file_operations fops_write_nrp_mac = {
	.write = ath11k_write_nrp_mac,
	.open = simple_open,
};

static int ath11k_fill_bp_stats(struct ath11k_base *ab,
				struct ath11k_bp_stats *bp_stats,
				char *buf, int len, int size)
{
	lockdep_assert_held(&ab->base_lock);

	len += scnprintf(buf + len, size - len, "count: %u\n",
			 bp_stats->count);
	len += scnprintf(buf + len, size - len, "hp: %u\n",
			 bp_stats->hp);
	len += scnprintf(buf + len, size - len, "tp: %u\n",
			 bp_stats->tp);
	len += scnprintf(buf + len, size - len, "seen before: %ums\n\n",
			 jiffies_to_msecs(jiffies - bp_stats->jiffies));
	return len;
}

ssize_t ath11k_debugfs_dump_soc_ring_bp_stats(struct ath11k_base *ab,
					      char *buf, int size)
{
	struct ath11k_bp_stats *bp_stats;
	bool stats_rxd = false;
	u8 i, pdev_idx;
	int len = 0;

	len += scnprintf(buf + len, size - len, "\nBackpressure Stats\n");
	len += scnprintf(buf + len, size - len, "==================\n");

	spin_lock_bh(&ab->base_lock);
	for (i = 0; i < HTT_SW_UMAC_RING_IDX_MAX; i++) {
		bp_stats = &ab->soc_stats.bp_stats.umac_ring_bp_stats[i];

		if (!bp_stats->count)
			continue;

		len += scnprintf(buf + len, size - len, "Ring: %s\n",
				 htt_bp_umac_ring[i]);
		len = ath11k_fill_bp_stats(ab, bp_stats, buf, len, size);
		stats_rxd = true;
	}

	for (i = 0; i < HTT_SW_LMAC_RING_IDX_MAX; i++) {
		for (pdev_idx = 0; pdev_idx < MAX_RADIOS; pdev_idx++) {
			bp_stats =
				&ab->soc_stats.bp_stats.lmac_ring_bp_stats[i][pdev_idx];

			if (!bp_stats->count)
				continue;

			len += scnprintf(buf + len, size - len, "Ring: %s\n",
					 htt_bp_lmac_ring[i]);
			len += scnprintf(buf + len, size - len, "pdev: %d\n",
					 pdev_idx);
			len = ath11k_fill_bp_stats(ab, bp_stats, buf, len, size);
			stats_rxd = true;
		}
	}
	spin_unlock_bh(&ab->base_lock);

	if (!stats_rxd)
		len += scnprintf(buf + len, size - len,
				 "No Ring Backpressure stats received\n\n");

	return len;
}

static ssize_t ath11k_debugfs_dump_soc_dp_stats(struct file *file,
						char __user *user_buf,
						size_t count, loff_t *ppos)
{
	struct ath11k_base *ab = file->private_data;
	struct ath11k_soc_dp_stats *soc_stats = &ab->soc_stats;
	int len = 0, i, retval;
	const int size = 4096;
	static const char *rxdma_err[HAL_REO_ENTR_RING_RXDMA_ECODE_MAX] = {
			"Overflow", "MPDU len", "FCS", "Decrypt", "TKIP MIC",
			"Unencrypt", "MSDU len", "MSDU limit", "WiFi parse",
			"AMSDU parse", "SA timeout", "DA timeout",
			"Flow timeout", "Flush req"};
	static const char *reo_err[HAL_REO_DEST_RING_ERROR_CODE_MAX] = {
			"Desc addr zero", "Desc inval", "AMPDU in non BA",
			"Non BA dup", "BA dup", "Frame 2k jump", "BAR 2k jump",
			"Frame OOR", "BAR OOR", "No BA session",
			"Frame SN equal SSN", "PN check fail", "2k err",
			"PN err", "Desc blocked"};

	char *buf;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ATH11K_MEMORY_STATS_INC(ab, malloc_size, size);

	len += scnprintf(buf + len, size - len, "SOC RX STATS:\n\n");
	len += scnprintf(buf + len, size - len, "err ring pkts: %u\n",
			 soc_stats->err_ring_pkts);
	len += scnprintf(buf + len, size - len, "Invalid RBM: %u\n\n",
			 soc_stats->invalid_rbm);
	len += scnprintf(buf + len, size - len, "RXDMA errors:\n");
	for (i = 0; i < HAL_REO_ENTR_RING_RXDMA_ECODE_MAX; i++)
		len += scnprintf(buf + len, size - len, "%s: handled %u dropped %u\n",
				 rxdma_err[i], soc_stats->rxdma_error[i],
				 soc_stats->rxdma_error_drop[i]);

	len += scnprintf(buf + len, size - len, "\nREO errors:\n");
	for (i = 0; i < HAL_REO_DEST_RING_ERROR_CODE_MAX; i++)
		len += scnprintf(buf + len, size - len, "%s: handled %u dropped %u\n",
				 reo_err[i], soc_stats->reo_error[i],
				 soc_stats->reo_error_drop[i]);

	len += scnprintf(buf + len, size - len, "\nHAL REO errors:\n");
	len += scnprintf(buf + len, size - len,
			 "ring0: %u\nring1: %u\nring2: %u\nring3: %u\n",
			 soc_stats->hal_reo_error[0],
			 soc_stats->hal_reo_error[1],
			 soc_stats->hal_reo_error[2],
			 soc_stats->hal_reo_error[3]);

	len += scnprintf(buf + len, size - len, "\nSOC TX STATS:\n");
	len += scnprintf(buf + len, size - len, "\nTCL Ring Full Failures:\n");

	for (i = 0; i < ab->hw_params.max_tx_ring; i++)
		len += scnprintf(buf + len, size - len, "ring%d: %u\n",
				 i, soc_stats->tx_err.desc_na[i]);

	len += scnprintf(buf + len, size - len, "\nTCL Ring idr Failures:\n");
	for (i = 0; i < DP_TCL_NUM_RING_MAX; i++)
		len += scnprintf(buf + len, size - len, "ring%d: %u\n",
				 i, soc_stats->tx_err.idr_na[i]);

	len += scnprintf(buf + len, size - len, "\nMax Transmit Failures: %d\n",
			 atomic_read(&soc_stats->tx_err.max_fail));

	len += scnprintf(buf + len, size - len,
			 "\nMisc Transmit Failures: %d\n",
			 atomic_read(&soc_stats->tx_err.misc_fail));

	len += scnprintf(buf + len, size - len,
			 "\nNSS Transmit Failures: %d\n",
			 atomic_read(&soc_stats->tx_err.nss_tx_fail));

	len += scnprintf(buf + len, size - len,
			 "\nHAL_REO_CMD_DRAIN Counter: %u\n",
			 soc_stats->hal_reo_cmd_drain);

	len += scnprintf(buf + len, size - len,
			 "\nREO_CMD_CACHE_FLUSH Failure: %u\n",
			 soc_stats->reo_cmd_cache_error);

	len += scnprintf(buf + len, size - len,
			 "\nREO_CMD_UPDATE_RX_QUEUE Failure: %u\n",
			 soc_stats->reo_cmd_update_rx_queue_error);

	len += ath11k_debugfs_dump_soc_ring_bp_stats(ab, buf + len, size - len);

	if (len > size)
		len = size;
	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	ATH11K_MEMORY_STATS_DEC(ab, malloc_size, size);

	return retval;
}

static const struct file_operations fops_soc_dp_stats = {
	.read = ath11k_debugfs_dump_soc_dp_stats,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_write_fw_dbglog(struct file *file,
				      const char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	char buf[128] = {0};
	struct ath11k_fw_dbglog dbglog;
	unsigned int param, mod_id_index, is_end;
	u64 value;
	int ret, num;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos,
				     user_buf, count);
	if (ret <= 0)
		return ret;

	num = sscanf(buf, "%u %llx %u %u", &param, &value, &mod_id_index, &is_end);

	if (num < 2)
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);
	if (param == WMI_DEBUG_LOG_PARAM_MOD_ENABLE_BITMAP ||
	    param == WMI_DEBUG_LOG_PARAM_WOW_MOD_ENABLE_BITMAP) {
		if (num != 4 || mod_id_index > (MAX_MODULE_ID_BITMAP_WORDS - 1)) {
			ret = -EINVAL;
			goto out;
		}
		ar->debug.module_id_bitmap[mod_id_index] = upper_32_bits(value);
		if (!is_end) {
			ret = count;
			goto out;
		}
	} else {
		if (num != 2) {
			ret = -EINVAL;
			goto out;
		}
	}

	dbglog.param = param;
	dbglog.value = lower_32_bits(value);
	ret = ath11k_wmi_fw_dbglog_cfg(ar, ar->debug.module_id_bitmap, &dbglog);
	if (ret) {
		ath11k_warn(ar->ab, "fw dbglog config failed from debugfs: %d\n",
			    ret);
		goto out;
	}

	ret = count;

out:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static const struct file_operations fops_fw_dbglog = {
	.write = ath11k_write_fw_dbglog,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static int ath11k_open_sram_dump(struct inode *inode, struct file *file)
{
	struct ath11k_base *ab = inode->i_private;
	u8 *buf;
	u32 start, end;
	int ret;

	start = ab->hw_params.sram_dump.start;
	end = ab->hw_params.sram_dump.end;

	buf = vmalloc(end - start + 1);
	if (!buf)
		return -ENOMEM;

	ret = ath11k_hif_read(ab, buf, start, end);
	if (ret) {
		ath11k_warn(ab, "failed to dump sram: %d\n", ret);
		vfree(buf);
		return ret;
	}

	file->private_data = buf;
	return 0;
}

static ssize_t ath11k_read_sram_dump(struct file *file,
				     char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct ath11k_base *ab = file->f_inode->i_private;
	const char *buf = file->private_data;
	int len;
	u32 start, end;

	start = ab->hw_params.sram_dump.start;
	end = ab->hw_params.sram_dump.end;
	len = end - start + 1;

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static int ath11k_release_sram_dump(struct inode *inode, struct file *file)
{
	vfree(file->private_data);
	file->private_data = NULL;

	return 0;
}

static const struct file_operations fops_sram_dump = {
	.open = ath11k_open_sram_dump,
	.read = ath11k_read_sram_dump,
	.release = ath11k_release_sram_dump,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_write_rx_hash(struct file *file,
				    const char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	struct ath11k_base *ab = file->private_data;
	struct ath11k_pdev *pdev;
	u32 rx_hash;
	u8 buf[128] = {0};
	int ret, i, radioup = 0;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		if (pdev && pdev->ar) {
			radioup = 1;
			break;
		}
	}

	if (radioup == 0) {
		ath11k_err(ab, "radio is not up\n");
		ret = -ENETDOWN;
		goto exit;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		goto exit;

	buf[ret] = '\0';
	ret = sscanf(buf, "%x", &rx_hash);
	if (!ret) {
		ret = -EINVAL;
		goto exit;
	}

	if (rx_hash != ab->rx_hash) {
		ab->rx_hash = rx_hash;
		if (rx_hash)
			ath11k_hal_reo_hash_setup(ab, rx_hash);
	}
	ret = count;
exit:
	return ret;
}
static const struct file_operations fops_soc_rx_hash = {
	.open = simple_open,
	.write = ath11k_write_rx_hash,
};

static void ath11k_debug_config_mon_status(struct ath11k *ar, bool enable)
{
	struct htt_rx_ring_tlv_filter tlv_filter = {0};
	struct ath11k_base *ab = ar->ab;
	int i;
	u32 ring_id;

	if (enable)
		tlv_filter = ath11k_mac_mon_status_filter_default;

	for (i = 0; i < ab->hw_params.num_rxdma_per_pdev; i++) {
		ring_id = ar->dp.rx_mon_status_refill_ring[i].refill_buf_ring.ring_id;
		ath11k_dp_tx_htt_rx_filter_setup(ar->ab, ring_id,
						 ar->dp.mac_id + i,
						 HAL_RXDMA_MONITOR_STATUS,
						 DP_RX_BUFFER_SIZE,
						 &tlv_filter);
	}
}

static ssize_t ath11k_write_stats_disable(struct file *file,
					  const char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath11k_base *ab = file->private_data;
	struct ath11k_pdev *pdev;
	bool disable;
	int ret, i, radioup = 0;
	u32 mask = 0;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		if (pdev && pdev->ar) {
			radioup = 1;
			break;
		}
	}

	if (radioup == 0) {
		ath11k_err(ab, "radio is not up\n");
		ret = -ENETDOWN;
		goto exit;
	}

	if (kstrtobool_from_user(user_buf, count, &disable))
		 return -EINVAL;

	 if (disable != ab->stats_disable) {
		ab->stats_disable = disable;
		for (i = 0; i < ab->num_radios; i++) {
			pdev = &ab->pdevs[i];
			if (pdev && pdev->ar) {
				ath11k_debug_config_mon_status(pdev->ar, !disable);

				if (!disable)
					mask = HTT_PPDU_STATS_TAG_DEFAULT;

				ath11k_dp_tx_htt_h2t_ppdu_stats_req(pdev->ar, mask);
			}
		}
	 }

	ret = count;

exit:
	return ret;
}

static const struct file_operations fops_soc_stats_disable = {
	.open = simple_open,
	.write = ath11k_write_stats_disable,
};

static ssize_t ath11k_debug_write_fw_recovery(struct file *file,
                                              const char __user *user_buf,
                                              size_t count, loff_t *ppos)
{
       struct ath11k_base *ab = file->private_data;
       struct ath11k *ar;
       struct ath11k_pdev *pdev;
       struct device *dev = ab->dev;
       bool multi_pd_arch = false;
       unsigned int value;
       int ret, i;

       if (kstrtouint_from_user(user_buf, count, 0, &value))
                return -EINVAL;

       if (value < ATH11K_FW_RECOVERY_DISABLE ||
	   value > ATH11K_FW_RECOVERY_ENABLE_SSR_ONLY) {
		ath11k_warn(ab, "Please enter: 0 = Disable, 1 = Enable (auto recover),"
			    "2 = Enable SSR only");
		ret = -EINVAL;
		goto exit;
       }

       for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		ar = pdev->ar;
		if (ar && ar->state == ATH11K_STATE_ON)
			break;
       }

       multi_pd_arch = of_property_read_bool(dev->of_node, "qcom,multipd_arch");
       if (multi_pd_arch) {
	       if (value == ATH11K_FW_RECOVERY_DISABLE ||
		   value == ATH11K_FW_RECOVERY_ENABLE_SSR_ONLY) {
		       ath11k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_MPD_USERPD_SSR,
						 0, ar->pdev->pdev_id);
	       } else if (value == ATH11K_FW_RECOVERY_ENABLE_AUTO)
		       ath11k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_MPD_USERPD_SSR,
						 1, ar->pdev->pdev_id);
       }
       ab->fw_recovery_support = value ? true : false;

       ret = count;

exit:
       return ret;
}

static ssize_t ath11k_debug_read_fw_recovery(struct file *file,
                                          char __user *user_buf,
                                          size_t count, loff_t *ppos)
{
       struct ath11k_base *ab = file->private_data;
       char buf[32];
       size_t len;

       len = scnprintf(buf, sizeof(buf), "%u\n", ab->fw_recovery_support);

       return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_fw_recovery = {
       .read = ath11k_debug_read_fw_recovery,
       .write = ath11k_debug_write_fw_recovery,
       .open = simple_open,
};

static ssize_t
ath11k_debug_read_enable_memory_stats(struct file *file,
				      char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	struct ath11k_base *ab = file->private_data;
	char buf[10];
	size_t len;

	len = scnprintf(buf, sizeof(buf), "%d\n", ab->enable_memory_stats);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t
ath11k_debug_write_enable_memory_stats(struct file *file,
				       const char __user *ubuf,
				       size_t count, loff_t *ppos)
{
	struct ath11k_base *ab = file->private_data;
	bool enable;
	int ret;

	if (kstrtobool_from_user(ubuf, count, &enable))
		return -EINVAL;

	if (enable == ab->enable_memory_stats) {
		ret = count;
		goto exit;
	}

	ab->enable_memory_stats = enable;
	ret = count;
exit:
	return ret;
}

static const struct file_operations fops_enable_memory_stats = {
	.read = ath11k_debug_read_enable_memory_stats,
	.write = ath11k_debug_write_enable_memory_stats,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
	.open = simple_open,
};

static ssize_t ath11k_debug_dump_memory_stats(struct file *file,
					      char __user *user_buf,
					      size_t count, loff_t *ppos)
{
	struct ath11k_base *ab = file->private_data;
	struct ath11k_memory_stats *memory_stats = &ab->memory_stats;
	int len = 0, retval;
	const int size = 4096;

	char *buf;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, size - len, "MEMORY STATS IN BYTES:\n");
	len += scnprintf(buf + len, size - len, "malloc size : %u\n",
			 atomic_read(&memory_stats->malloc_size));
	len += scnprintf(buf + len, size - len, "ce_ring_alloc size: %u\n",
			 atomic_read(&memory_stats->ce_ring_alloc));
	len += scnprintf(buf + len, size - len, "dma_alloc size:: %u\n",
			 atomic_read(&memory_stats->dma_alloc));
	len += scnprintf(buf + len, size - len, "htc_skb_alloc size: %u\n",
			 atomic_read(&memory_stats->htc_skb_alloc));
	len += scnprintf(buf + len, size - len, "wmi tx skb alloc size: %u\n",
			 atomic_read(&memory_stats->wmi_tx_skb_alloc));
	len += scnprintf(buf + len, size - len, "per peer object: %u\n",
			 atomic_read(&memory_stats->per_peer_object));
	len += scnprintf(buf + len, size - len, "rx_post_buf size: %u\n",
			 atomic_read(&memory_stats->ce_rx_pipe));
	len += scnprintf(buf + len, size - len, "Total size: %u\n\n",
			 (atomic_read(&memory_stats->malloc_size) +
			 atomic_read(&memory_stats->ce_ring_alloc) +
			 atomic_read(&memory_stats->dma_alloc) +
			 atomic_read(&memory_stats->htc_skb_alloc) +
			 atomic_read(&memory_stats->wmi_tx_skb_alloc) +
			 atomic_read(&memory_stats->per_peer_object) +
			 atomic_read(&memory_stats->ce_rx_pipe)));

	if (len > size)
		len = size;

	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return retval;
}

static const struct file_operations fops_memory_stats = {
	.read = ath11k_debug_dump_memory_stats,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_write_ce_latency_stats(struct file *file,
				       const char __user *user_buf,
				       size_t count, loff_t *ppos)
{
       struct ath11k_base *ab = file->private_data;
       bool enable;
       int ret;

       if (kstrtobool_from_user(user_buf, count, &enable))
                return -EINVAL;

       if (enable == ab->ce_latency_stats_enable) {
                ret = count;
                goto exit;
       }

       ab->ce_latency_stats_enable = enable;
       ret = count;

exit:
	return ret;
}

static ssize_t ath11k_read_ce_latency_stats(struct file *file,
					    char __user *user_buf,
					    size_t count, loff_t *ppos)
{
	struct ath11k_base *ab = file->private_data;
	int len = 0, retval;
	const int size = 12288;
	char *buf;
	struct ath11k_ce_pipe *ce_pipe;
	int i, j;
	unsigned int last_sched, last_exec;
	char *ce_time_dur[CE_TIME_DURATION_MAX] = {
		"ce_time_dur_100US", "ce_time_dur_200US", "ce_time_dur_300US",
		"ce_time_dur_400US", "ce_time_dur_500US"};

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, size - len, "CE_LATENCY_STATS:\n");
	for (i = 0; i < ab->hw_params.ce_count; i++) {
		ce_pipe = &ab->ce.ce_pipe[i];

		len += scnprintf(buf + len, size - len, "CE_id  %u ", i);
		len += scnprintf(buf + len, size - len, "pipe_num  %d ",
				 ce_pipe->pipe_num);
		len += scnprintf(buf + len, size - len, "%ums before, ",
				 jiffies_to_msecs(jiffies - ce_pipe->timestamp));
		len += scnprintf(buf + len, size - len, "sched_delay_gt_500US %u, ",
				 ce_pipe->sched_delay_gt_500US);
		len += scnprintf(buf + len, size - len, "exec_delay_gt_500US %u,\n",
				 ce_pipe->sched_delay_gt_500US);

		for (j = 0; j < CE_TIME_DURATION_MAX; j++) {
			last_sched = jiffies_to_msecs(jiffies -
						      ce_pipe->tracker[j].sched_last_update);
			last_exec = jiffies_to_msecs(jiffies -
						     ce_pipe->tracker[j].exec_last_update);

			len += scnprintf(buf + len, size - len, "%-17s,\t ", ce_time_dur[j]);
			len += scnprintf(buf + len, size - len, "last_sched_before %10ums,\t ",
					 ((ce_pipe->tracker[j].sched_last_update > 0) ? last_sched : 0));
			len += scnprintf(buf + len, size - len, "tot_sched_cnt %20llu,\t ",
					 ce_pipe->tracker[j].sched_count);
			len += scnprintf(buf + len, size - len, "last_exec_before %10ums,\t ",
					 ((ce_pipe->tracker[j].exec_last_update > 0) ? last_exec : 0));
			len += scnprintf(buf + len, size - len, "tot_exec_cnt %20llu\n",
					 ce_pipe->tracker[j].exec_count);
		}
	}
	if (len > size)
		len = size;
	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return retval;
}

static const struct file_operations fops_ce_latency_stats = {
	.write = ath11k_write_ce_latency_stats,
	.open = simple_open,
	.read = ath11k_read_ce_latency_stats,
};

static ssize_t ath11k_debugfs_hal_dump_srng_stats_read(struct file *file,
						char __user *user_buf,
						size_t count, loff_t *ppos)
{
	struct ath11k_base *ab = file->private_data;
	int len = 0, retval;
	const int size = 4096 * 6;
	char *buf;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len = ath11k_debugfs_hal_dump_srng_stats(ab, buf + len, size - len);
	if (len > size)
		len = size;
	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return retval;
}

static const struct file_operations fops_dump_hal_stats = {
	.read = ath11k_debugfs_hal_dump_srng_stats_read,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

int ath11k_debugfs_pdev_create(struct ath11k_base *ab)
{
	if (test_bit(ATH11K_FLAG_REGISTERED, &ab->dev_flags))
		return 0;

	debugfs_create_file("simulate_fw_crash", 0600, ab->debugfs_soc, ab,
			    &fops_simulate_fw_crash);

	if (ab->hw_params.is_qdss_support)
		debugfs_create_file("trace_qdss", 0600, ab->debugfs_soc, ab,
			 	    &fops_trace_qdss);

	debugfs_create_file("soc_dp_stats", 0400, ab->debugfs_soc, ab,
			    &fops_soc_dp_stats);

	if (ab->hw_params.sram_dump.start != 0)
		debugfs_create_file("sram", 0400, ab->debugfs_soc, ab,
				    &fops_sram_dump);

	debugfs_create_file("set_fw_recovery", 0600, ab->debugfs_soc, ab,
			    &fops_fw_recovery);

	debugfs_create_file("enable_memory_stats", 0600, ab->debugfs_soc,
			    ab, &fops_enable_memory_stats);

	debugfs_create_file("memory_stats", 0600, ab->debugfs_soc, ab,
			    &fops_memory_stats);

	debugfs_create_file("ce_latency_stats", 0600, ab->debugfs_soc, ab,
			    &fops_ce_latency_stats);

	debugfs_create_file("rx_hash", 0600, ab->debugfs_soc, ab,
			    &fops_soc_rx_hash);

	debugfs_create_file("dump_srng_stats", 0600, ab->debugfs_soc, ab,
			    &fops_dump_hal_stats);

	return 0;
}

void ath11k_debugfs_pdev_destroy(struct ath11k_base *ab)
{
}

int ath11k_debugfs_soc_create(struct ath11k_base *ab)
{
	struct dentry *root;
	bool dput_needed;
	char name[64];
	int ret;

	root = debugfs_lookup("ath11k", NULL);
	if (!root) {
		root = debugfs_create_dir("ath11k", NULL);
		if (IS_ERR_OR_NULL(root))
			return PTR_ERR(root);

		dput_needed = false;
	} else {
		/* a dentry from lookup() needs dput() after we don't use it */
		dput_needed = true;
	}

	scnprintf(name, sizeof(name), "%s-%s", ath11k_bus_str(ab->hif.bus),
		  dev_name(ab->dev));

	ab->debugfs_soc = debugfs_create_dir(name, root);
	if (IS_ERR_OR_NULL(ab->debugfs_soc)) {
		ret = PTR_ERR(ab->debugfs_soc);
		goto out;
	}
	debugfs_create_file("stats_disable", 0600, ab->debugfs_soc, ab,
			    &fops_soc_stats_disable);

	ret = 0;

out:
	if (dput_needed)
		dput(root);

	return ret;
}

void ath11k_debugfs_soc_destroy(struct ath11k_base *ab)
{
	debugfs_remove_recursive(ab->debugfs_soc);
	ab->debugfs_soc = NULL;

	/* We are not removing ath11k directory on purpose, even if it
	 * would be empty. This simplifies the directory handling and it's
	 * a minor cosmetic issue to leave an empty ath11k directory to
	 * debugfs.
	 */
}
EXPORT_SYMBOL(ath11k_debugfs_soc_destroy);

int ath11k_debugfs_create()
{
	debugfs_ath11k = debugfs_create_dir("ath11k", NULL);
	if (IS_ERR_OR_NULL(debugfs_ath11k)) {
		if (IS_ERR(debugfs_ath11k))
			return PTR_ERR(debugfs_ath11k);
		return -ENOMEM;
	}

	return 0;
}

void ath11k_debugfs_destroy()
{
	debugfs_remove_recursive(debugfs_ath11k);
	debugfs_ath11k = NULL;
	debugfs_debug_infra = NULL;
}

void ath11k_debugfs_fw_stats_init(struct ath11k *ar)
{
	struct dentry *fwstats_dir = debugfs_create_dir("fw_stats",
							ar->debug.debugfs_pdev);

	ar->fw_stats.debugfs_fwstats = fwstats_dir;

	/* all stats debugfs files created are under "fw_stats" directory
	 * created per PDEV
	 */
	debugfs_create_file("pdev_stats", 0600, fwstats_dir, ar,
			    &fops_pdev_stats);
	debugfs_create_file("vdev_stats", 0600, fwstats_dir, ar,
			    &fops_vdev_stats);
	debugfs_create_file("beacon_stats", 0600, fwstats_dir, ar,
			    &fops_bcn_stats);
}

/* TX delay stats class names ordered by TID. */
static const char ath11k_tx_delay_stats_names[][7] = {
	"AC_BE",
	"AC_BK",
	"AC_BK+",
	"AC_BE+",
	"AC_VI",
	"AC_VI+",
	"AC_VO",
	"AC_VO+",
	"TID8",
	"TID9",
	"TID10",
	"TID11",
	"TID12",
	"TID13",
	"TID14",
	"TID15",
};

#define ATH11K_TX_DELAY_STATS_NAMES_SIZE ARRAY_SIZE(ath11k_tx_delay_stats_names)

/* Returns start time of the transmit delay histogram stats bin. */
static inline int ath11k_tx_delay_bin_to_ms(int bin)
{
	int bin_ms;

	/* The first two bins span 1ms (i.e. [0, 1), and [1, 2)) are returned
	 * directly. All other power-of-two bucket ranges are subdivided into
	 * two bins, with the even numbered bin covering the first half of the
	 * range and the odd numbered bin offset by 1/2 of the range.
	 */
	if (bin < 2)
		return bin;
	bin_ms = 1 << (bin / 2);
	if (bin % 2)
		bin_ms += bin_ms >> 1;
	return bin_ms;
}

static ssize_t ath11k_tx_delay_histo_dump(struct file *file,
					  char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath11k_tx_delay_stats *stats = file->private_data;
	struct ath11k_tx_delay_stats stats_local;
	char *buf;
	unsigned int len = 0, buf_len = 4096, i;
	ssize_t ret_cnt;

	memcpy(&stats_local, stats, sizeof(struct ath11k_tx_delay_stats));
	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return 0;

	len += scnprintf(buf + len, buf_len - len, "TX delay histogram(ms)\n");
	for (i = 0; i < ATH11K_DELAY_STATS_SCALED_BINS; i++) {
		len += scnprintf(buf + len, buf_len - len,
				 "[%4u - %4u):%8u ",
				 ath11k_tx_delay_bin_to_ms(i),
				 ath11k_tx_delay_bin_to_ms(i + 1),
				 stats_local.counts[i]);

		if (i % 5 == 4)
			len += scnprintf(buf + len, buf_len - len, "\n");
	}
	len += scnprintf(buf + len, buf_len - len, "[%4d -  inf):%8u ",
			 ath11k_tx_delay_bin_to_ms(i), stats_local.counts[i]);

	len += scnprintf(buf + len, buf_len - len, "\n");

	ret_cnt = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret_cnt;
}

static ssize_t ath11k_tx_delay_histo_reset(struct file *file,
					   const char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	struct ath11k_tx_delay_stats *stats = file->private_data;
	int val, ret;

	ret = kstrtoint_from_user(user_buf, count, 0, &val);
	if (ret)
		return ret;
	if (val != 0)
		return -EINVAL;
	memset(stats, 0, sizeof(struct ath11k_tx_delay_stats));
	return count;
}

static const struct file_operations fops_tx_delay_histo = {
	.read = ath11k_tx_delay_histo_dump,
	.write = ath11k_tx_delay_histo_reset,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

void ath11k_init_tx_latency_stats(struct ath11k *ar)
{
	size_t tx_delay_stats_size;
	struct ath11k_tx_delay_stats *pbuf, *buf;
	struct dentry *tx_delay_histo_dir;
	int i;

	tx_delay_stats_size = sizeof(struct ath11k_tx_delay_stats) *
			      ARRAY_SIZE(ar->debug.tx_delay_stats);

	pbuf = kzalloc(tx_delay_stats_size, GFP_KERNEL);
	if (!pbuf) {
		ath11k_err(ar->ab, "Unable to allocate memory for latency stats\n");
		return;
	}

	buf = pbuf;

	for (i = 0; i < ARRAY_SIZE(ar->debug.tx_delay_stats); i++) {
		ar->debug.tx_delay_stats[i] = buf;
		buf++;
	}

	tx_delay_histo_dir = debugfs_create_dir("tx_delay_histogram",
						ar->debug.debugfs_pdev);
	if (IS_ERR_OR_NULL(tx_delay_histo_dir)) {
		ath11k_err(ar->ab, "Failed to create debugfs dir tx_delay_stats\n");
		kfree(pbuf);
		return;
	}
	for (i = 0; i < ATH11K_TX_DELAY_STATS_NAMES_SIZE; i++) {
		debugfs_create_file(ath11k_tx_delay_stats_names[i], 0644,
				    tx_delay_histo_dir,
				    ar->debug.tx_delay_stats[i],
				    &fops_tx_delay_histo);
	}
}

static ssize_t ath11k_write_pktlog_filter(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	struct ath11k_base *ab = ar->ab;
	struct htt_rx_ring_tlv_filter tlv_filter = {0}, cbf_tlv_filter = {0};
	u32 rx_filter = 0, ring_id, filter, mode;
	u8 buf[128] = {0};
	int i, ret, rx_buf_sz = 0;
	ssize_t rc;
	char *pktlog_mode[ATH11K_PKTLOG_MODE_CBF_FULL] = {"lite", "full",
							  "cbf lite",
							  "cbf full"};

	mutex_lock(&ar->conf_mutex);
	if (ar->state != ATH11K_STATE_ON) {
		ret = -ENETDOWN;
		goto out;
	}

	rc = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (rc < 0) {
		ret = rc;
		goto out;
	}
	buf[rc] = '\0';

	ret = sscanf(buf, "0x%x %u", &filter, &mode);
	if (ret != 2 || !mode || mode >= ATH11K_PKTLOG_MODE_MAX) {
		ret = -EINVAL;
		goto out;
	}

	if (filter) {
		ret = ath11k_wmi_pdev_pktlog_enable(ar, filter);
		if (ret) {
			ath11k_warn(ar->ab,
				    "failed to enable pktlog filter %x: %d\n",
				    ar->debug.pktlog_filter, ret);
			goto out;
		}
	} else {
		ret = ath11k_wmi_pdev_pktlog_disable(ar);
		if (ret) {
			ath11k_warn(ar->ab, "failed to disable pktlog: %d\n", ret);
			goto out;
		}
	}

	/* Clear rx filter set for monitor mode and rx status */
	tlv_filter.offset_valid = false;
	for (i = 0; i < ab->hw_params.num_rxdma_per_pdev; i++) {
		ring_id = ar->dp.rx_mon_status_refill_ring[i].refill_buf_ring.ring_id;
		if (mode == ATH11K_PKTLOG_MODE_CBF_LITE ||
		    mode == ATH11K_PKTLOG_MODE_CBF_FULL) {
			ret = ath11k_dp_tx_htt_rx_filter_setup(ab, ring_id, ar->dp.mac_id,
							       HAL_RXDMA_MONITOR_BUF,
							       rx_buf_sz, &cbf_tlv_filter);
			if (ret) {
				ath11k_warn(ab, "failed to set rx filter for monitor buffer ring\n");
				goto out;
			}
		}
		ret = ath11k_dp_tx_htt_rx_filter_setup(ar->ab, ring_id, ar->dp.mac_id,
						       HAL_RXDMA_MONITOR_STATUS,
						       rx_buf_sz, &tlv_filter);
		if (ret) {
			ath11k_warn(ar->ab, "failed to set rx filter for monitor status ring\n");
			goto out;
		}
	}
	if (mode == ATH11K_PKTLOG_MODE_CBF_LITE ||
	    mode == ATH11K_PKTLOG_MODE_CBF_FULL) {
		cbf_tlv_filter.pkt_filter_flags0 = 0;
		cbf_tlv_filter.pkt_filter_flags1 = HTT_RX_FP_MGMT_PKT_FILTER_TLV_FLAGS1_ACTION_NOACK;
		cbf_tlv_filter.pkt_filter_flags2 = 0;
		cbf_tlv_filter.pkt_filter_flags3 = 0;
		cbf_tlv_filter.rx_filter = HTT_RX_RXDMA_FILTER_TLV_FLAGS_BUF_RING;
	}

#define HTT_RX_FILTER_TLV_LITE_MODE \
			(HTT_RX_FILTER_TLV_FLAGS_PPDU_START | \
			HTT_RX_FILTER_TLV_FLAGS_PPDU_END | \
			HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS | \
			HTT_RX_FILTER_TLV_FLAGS_PPDU_END_USER_STATS_EXT | \
			HTT_RX_FILTER_TLV_FLAGS_PPDU_END_STATUS_DONE | \
			HTT_RX_FILTER_TLV_FLAGS_MPDU_START)

	if (mode == ATH11K_PKTLOG_MODE_FULL ||
	    mode == ATH11K_PKTLOG_MODE_CBF_FULL) {
		rx_filter = HTT_RX_FILTER_TLV_LITE_MODE |
			    HTT_RX_FILTER_TLV_FLAGS_MSDU_START |
			    HTT_RX_FILTER_TLV_FLAGS_MSDU_END |
			    HTT_RX_FILTER_TLV_FLAGS_MPDU_END |
			    HTT_RX_FILTER_TLV_FLAGS_PACKET_HEADER |
			    HTT_RX_FILTER_TLV_FLAGS_ATTENTION;
		rx_buf_sz = DP_RX_BUFFER_SIZE;
	} else if (mode == ATH11K_PKTLOG_MODE_LITE ||
		   mode == ATH11K_PKTLOG_MODE_CBF_LITE) {
		ret = ath11k_dp_tx_htt_h2t_ppdu_stats_req(ar,
							  HTT_PPDU_STATS_TAG_PKTLOG);
		if (ret) {
			ath11k_err(ar->ab, "failed to enable pktlog lite: %d\n", ret);
			goto out;
		}

		rx_filter = HTT_RX_FILTER_TLV_LITE_MODE;
		rx_buf_sz = DP_RX_BUFFER_SIZE_LITE;
	} else {
		rx_buf_sz = DP_RX_BUFFER_SIZE;
		tlv_filter = ath11k_mac_mon_status_filter_default;
		rx_filter = tlv_filter.rx_filter;

		ret = ath11k_dp_tx_htt_h2t_ppdu_stats_req(ar,
							  HTT_PPDU_STATS_TAG_DEFAULT);
		if (ret) {
			ath11k_err(ar->ab, "failed to send htt ppdu stats req: %d\n",
				   ret);
			goto out;
		}
	}

	tlv_filter.rx_filter = rx_filter;
	if (rx_filter) {
		tlv_filter.pkt_filter_flags0 = HTT_RX_FP_MGMT_FILTER_FLAGS0;
		tlv_filter.pkt_filter_flags1 = HTT_RX_FP_MGMT_FILTER_FLAGS1;
		tlv_filter.pkt_filter_flags2 = HTT_RX_FP_CTRL_FILTER_FLASG2;
		tlv_filter.pkt_filter_flags3 = HTT_RX_FP_CTRL_FILTER_FLASG3 |
					       HTT_RX_FP_DATA_FILTER_FLASG3;
	}

	for (i = 0; i < ab->hw_params.num_rxdma_per_pdev; i++) {
		ring_id = ar->dp.rx_mon_status_refill_ring[i].refill_buf_ring.ring_id;
		if (mode == ATH11K_PKTLOG_MODE_CBF_LITE ||
		    mode == ATH11K_PKTLOG_MODE_CBF_FULL) {
			ret = ath11k_dp_tx_htt_rx_filter_setup(ab, ring_id,
							       ar->dp.mac_id + i,
							       HAL_RXDMA_MONITOR_BUF,
							       rx_buf_sz, &cbf_tlv_filter);
			if (ret) {
				ath11k_warn(ab, "failed to set rx filter for monitor buffer ring\n");
				goto out;
			}
		}

		ret = ath11k_dp_tx_htt_rx_filter_setup(ab, ring_id,
						       ar->dp.mac_id + i,
						       HAL_RXDMA_MONITOR_STATUS,
						       rx_buf_sz, &tlv_filter);

		if (ret) {
			ath11k_warn(ab, "failed to set rx filter for monitor status ring\n");
			goto out;
		}
	}

	ath11k_info(ar->ab, "pktlog mode %s\n", pktlog_mode[mode - 1]);

	ar->debug.pktlog_filter = filter;
	ar->debug.pktlog_mode = mode;
	ret = count;

out:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static ssize_t ath11k_read_pktlog_filter(struct file *file,
					 char __user *ubuf,
					 size_t count, loff_t *ppos)

{
	char buf[32] = {0};
	struct ath11k *ar = file->private_data;
	int len = 0;

	mutex_lock(&ar->conf_mutex);
	len = scnprintf(buf, sizeof(buf) - len, "%08x %08x\n",
			ar->debug.pktlog_filter,
			ar->debug.pktlog_mode);
	mutex_unlock(&ar->conf_mutex);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_pktlog_filter = {
	.read = ath11k_read_pktlog_filter,
	.write = ath11k_write_pktlog_filter,
	.open = simple_open
};

#define	SEGMENT_ID	GENMASK(1,0)
#define CHRIP_ID	BIT(2)
#define OFFSET		GENMASK(10,3)
#define DETECTOR_ID	GENMASK(12,11)
static ssize_t ath11k_write_simulate_radar(struct file *file,
					   const char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	int ret;
	u32 radar_params;
	u8 agile = 0, segment = 0, chrip = 0;
	int offset = 0, len;
	char buf[64], *token, *sptr;


	if (ar->state != ATH11K_STATE_ON) {
		ath11k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
		mutex_unlock(&ar->conf_mutex);
		return -ENETDOWN;
	}

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	/* For backward compatibility */
	if (len <= 2)
		goto send_cmd;

	buf[len] = '\0';
	sptr = buf;
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;

	if (kstrtou8(token, 16, &segment))
		return -EINVAL;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;

	if (kstrtou8(token, 16, &chrip))
		return -EINVAL;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;

	if (kstrtoint(token, 16, &offset))
		return -EINVAL;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;

	if (kstrtou8(token, 16, &agile))
		return -EINVAL;

	if ((segment > 1) || (chrip > 1) || (agile > 2))
		return -EINVAL;

send_cmd:
	radar_params = FIELD_PREP(SEGMENT_ID, segment) |
		       FIELD_PREP(CHRIP_ID, chrip) |
		       FIELD_PREP(OFFSET, offset) |
		       FIELD_PREP(DETECTOR_ID, agile);

	ret = ath11k_wmi_simulate_radar(ar, radar_params);
	if (ret)
		return ret;

	return count;
}

static const struct file_operations fops_simulate_radar = {
	.write = ath11k_write_simulate_radar,
	.open = simple_open
};

static ssize_t ath11k_debug_dump_dbr_entries(struct file *file,
					     char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	struct ath11k_dbg_dbr_data *dbr_dbg_data = file->private_data;
	static const char * const event_id_to_string[] = {"empty", "Rx", "Replenish"};
	int size = ATH11K_DEBUG_DBR_ENTRIES_MAX * 100;
	char *buf;
	int i, ret;
	int len = 0;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, size - len,
			 "-----------------------------------------\n");
	len += scnprintf(buf + len, size - len,
			 "| idx |  hp  |  tp  | timestamp |  event |\n");
	len += scnprintf(buf + len, size - len,
			 "-----------------------------------------\n");

	spin_lock_bh(&dbr_dbg_data->lock);

	for (i = 0; i < dbr_dbg_data->num_ring_debug_entries; i++) {
		len += scnprintf(buf + len, size - len,
				 "|%4u|%8u|%8u|%11llu|%8s|\n", i,
				 dbr_dbg_data->entries[i].hp,
				 dbr_dbg_data->entries[i].tp,
				 dbr_dbg_data->entries[i].timestamp,
				 event_id_to_string[dbr_dbg_data->entries[i].event]);
	}

	spin_unlock_bh(&dbr_dbg_data->lock);

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return ret;
}

static const struct file_operations fops_debug_dump_dbr_entries = {
	.read = ath11k_debug_dump_dbr_entries,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static void ath11k_debugfs_dbr_dbg_destroy(struct ath11k *ar, int dbr_id)
{
	struct ath11k_debug_dbr *dbr_debug;
	struct ath11k_dbg_dbr_data *dbr_dbg_data;

	if (!ar->debug.dbr_debug[dbr_id])
		return;

	dbr_debug = ar->debug.dbr_debug[dbr_id];
	dbr_dbg_data = &dbr_debug->dbr_dbg_data;

	debugfs_remove_recursive(dbr_debug->dbr_debugfs);
	kfree(dbr_dbg_data->entries);
	kfree(dbr_debug);
	ar->debug.dbr_debug[dbr_id] = NULL;
}

static int ath11k_debugfs_dbr_dbg_init(struct ath11k *ar, int dbr_id)
{
	struct ath11k_debug_dbr *dbr_debug;
	struct ath11k_dbg_dbr_data *dbr_dbg_data;
	static const char * const dbr_id_to_str[] = {"spectral", "CFR"};

	if (ar->debug.dbr_debug[dbr_id])
		return 0;

	ar->debug.dbr_debug[dbr_id] = kzalloc(sizeof(*dbr_debug),
					      GFP_KERNEL);

	if (!ar->debug.dbr_debug[dbr_id])
		return -ENOMEM;

	dbr_debug = ar->debug.dbr_debug[dbr_id];
	dbr_dbg_data = &dbr_debug->dbr_dbg_data;

	if (dbr_debug->dbr_debugfs)
		return 0;

	dbr_debug->dbr_debugfs = debugfs_create_dir(dbr_id_to_str[dbr_id],
						    ar->debug.debugfs_pdev);
	if (IS_ERR_OR_NULL(dbr_debug->dbr_debugfs)) {
		if (IS_ERR(dbr_debug->dbr_debugfs))
			return PTR_ERR(dbr_debug->dbr_debugfs);
		return -ENOMEM;
	}

	dbr_debug->dbr_debug_enabled = true;
	dbr_dbg_data->num_ring_debug_entries = ATH11K_DEBUG_DBR_ENTRIES_MAX;
	dbr_dbg_data->dbr_debug_idx = 0;
	dbr_dbg_data->entries = kcalloc(ATH11K_DEBUG_DBR_ENTRIES_MAX,
					sizeof(struct ath11k_dbg_dbr_entry),
					GFP_KERNEL);
	if (!dbr_dbg_data->entries)
		return -ENOMEM;

	spin_lock_init(&dbr_dbg_data->lock);

	debugfs_create_file("dump_dbr_debug", 0444, dbr_debug->dbr_debugfs,
			    dbr_dbg_data, &fops_debug_dump_dbr_entries);

	return 0;
}

static ssize_t ath11k_debugfs_write_enable_dbr_dbg(struct file *file,
						   const char __user *ubuf,
						   size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	char buf[32] = {0};
	u32 dbr_id, enable;
	int ret;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ret = -ENETDOWN;
		goto out;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		goto out;

	buf[ret] = '\0';
	ret = sscanf(buf, "%u %u", &dbr_id, &enable);
	if (ret != 2 || dbr_id > 1 || enable > 1) {
		ret = -EINVAL;
		ath11k_warn(ar->ab, "usage: echo <dbr_id> <val> dbr_id:0-Spectral 1-CFR val:0-disable 1-enable\n");
		goto out;
	}

	if (enable) {
		ret = ath11k_debugfs_dbr_dbg_init(ar, dbr_id);
		if (ret) {
			ath11k_warn(ar->ab, "db ring module debugfs init failed: %d\n",
				    ret);
			goto out;
		}
	} else {
		ath11k_debugfs_dbr_dbg_destroy(ar, dbr_id);
	}

	ret = count;
out:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static const struct file_operations fops_dbr_debug = {
	.write = ath11k_debugfs_write_enable_dbr_dbg,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_write_ps_timekeeper_enable(struct file *file,
						 const char __user *user_buf,
						 size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	ssize_t ret;
	u8 ps_timekeeper_enable;

	if (kstrtou8_from_user(user_buf, count, 0, &ps_timekeeper_enable))
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ret = -ENETDOWN;
		goto exit;
	}

	if (!ar->ps_state_enable) {
		ret = -EINVAL;
		goto exit;
	}

	ar->ps_timekeeper_enable = !!ps_timekeeper_enable;
	ret = count;
exit:
	mutex_unlock(&ar->conf_mutex);

	return ret;
}

static ssize_t ath11k_read_ps_timekeeper_enable(struct file *file,
						char __user *user_buf,
						size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	char buf[32];
	int len;

	mutex_lock(&ar->conf_mutex);
	len = scnprintf(buf, sizeof(buf), "%d\n", ar->ps_timekeeper_enable);
	mutex_unlock(&ar->conf_mutex);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_ps_timekeeper_enable = {
	.read = ath11k_read_ps_timekeeper_enable,
	.write = ath11k_write_ps_timekeeper_enable,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static void ath11k_reset_peer_ps_duration(void *data,
					  struct ieee80211_sta *sta)
{
	struct ath11k *ar = data;
	struct ath11k_sta *arsta = ath11k_sta_to_arsta(sta);

	spin_lock_bh(&ar->data_lock);
	arsta->ps_total_duration = 0;
	spin_unlock_bh(&ar->data_lock);
}

static ssize_t ath11k_write_reset_ps_duration(struct file *file,
					      const  char __user *user_buf,
					      size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	int ret;
	u8 reset_ps_duration;

	if (kstrtou8_from_user(user_buf, count, 0, &reset_ps_duration))
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ret = -ENETDOWN;
		goto exit;
	}

	if (!ar->ps_state_enable) {
		ret = -EINVAL;
		goto exit;
	}

	ieee80211_iterate_stations_atomic(ar->hw,
					  ath11k_reset_peer_ps_duration,
					  ar);

	ret = count;
exit:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static const struct file_operations fops_reset_ps_duration = {
	.write = ath11k_write_reset_ps_duration,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static void ath11k_peer_ps_state_disable(void *data,
					 struct ieee80211_sta *sta)
{
	struct ath11k *ar = data;
	struct ath11k_sta *arsta = ath11k_sta_to_arsta(sta);

	spin_lock_bh(&ar->data_lock);
	arsta->peer_ps_state = WMI_PEER_PS_STATE_DISABLED;
	arsta->ps_start_time = 0;
	arsta->ps_total_duration = 0;
	spin_unlock_bh(&ar->data_lock);
}

static ssize_t ath11k_write_ps_state_enable(struct file *file,
					    const char __user *user_buf,
					    size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	struct ath11k_pdev *pdev = ar->pdev;
	int ret;
	u32 param;
	u8 ps_state_enable;

	if (kstrtou8_from_user(user_buf, count, 0, &ps_state_enable))
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ath11k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
		ret = -ENETDOWN;
		goto exit;
	}

	ps_state_enable = !!ps_state_enable;

	if (ar->ps_state_enable == ps_state_enable) {
		ret = count;
		goto exit;
	}

	param = WMI_PDEV_PEER_STA_PS_STATECHG_ENABLE;
	ret = ath11k_wmi_pdev_set_param(ar, param, ps_state_enable, pdev->pdev_id);
	if (ret) {
		ath11k_warn(ar->ab, "failed to enable ps_state_enable: %d\n",
			    ret);
		goto exit;
	}
	ar->ps_state_enable = ps_state_enable;

	if (!ar->ps_state_enable) {
		ar->ps_timekeeper_enable = false;
		ieee80211_iterate_stations_atomic(ar->hw,
						  ath11k_peer_ps_state_disable,
						  ar);
	}

	ret = count;

exit:
	mutex_unlock(&ar->conf_mutex);

	return ret;
}

static ssize_t ath11k_read_ps_state_enable(struct file *file,
					   char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	char buf[32];
	int len;

	mutex_lock(&ar->conf_mutex);
	len = scnprintf(buf, sizeof(buf), "%d\n", ar->ps_state_enable);
	mutex_unlock(&ar->conf_mutex);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_ps_state_enable = {
	.read = ath11k_read_ps_state_enable,
	.write = ath11k_write_ps_state_enable,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_write_simulate_awgn(struct file *file,
					  const char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	int ret;
	u32 chan_bw_interference_bitmap;

	mutex_lock(&ar->conf_mutex);
	if (ar->state != ATH11K_STATE_ON) {
		ret = -ENETDOWN;
		goto exit;
	}

	if (kstrtou32_from_user(user_buf, count, 0, &chan_bw_interference_bitmap))
		return -EINVAL;

	ret = ath11k_wmi_simulate_awgn(ar, chan_bw_interference_bitmap);
	if (ret)
		goto exit;

	ret = count;

exit:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static const struct file_operations fops_simulate_awgn = {
	.write = ath11k_write_simulate_awgn,
	.open = simple_open
};

static ssize_t ath11k_write_btcoex(struct file *file,
				   const char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif;
	struct ath11k *ar = file->private_data;
	char buf[256];
	size_t buf_size;
	int ret,coex = -1;
	enum qca_wlan_priority_type wlan_prio_mask = 0;
	int wlan_weight = 0;

	if (!ar)
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);
	if (ar->state != ATH11K_STATE_ON) {
		ath11k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
		mutex_unlock(&ar->conf_mutex);
		return -ENETDOWN;
	}
	mutex_unlock(&ar->conf_mutex);

	buf_size = min(count, (sizeof(buf) - 1));
	if (copy_from_user(buf, ubuf, buf_size))
		return -EFAULT;

	buf[buf_size] = '\0';
	ret = sscanf(buf, "%d %u %d" , &coex, &wlan_prio_mask, &wlan_weight);
	if (!ret)
		return -EINVAL;

	if (wlan_weight == -1)
		wlan_weight = 0;

	if(wlan_prio_mask == -1)
		wlan_prio_mask =0;

	if(wlan_weight < 0 || wlan_prio_mask < 0)
		return -EINVAL;

	if(coex != 1 &&  coex != -1 && coex)
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);
	arvif = list_first_entry(&ar->arvifs, typeof(*arvif), list);
	if (!arvif->is_started) {
		ret = -EINVAL;
		goto exit;
	}

	if(coex == 1 && !test_bit(ATH11K_FLAG_BTCOEX, &ar->dev_flags))
		set_bit(ATH11K_FLAG_BTCOEX, &ar->dev_flags);

	if(coex == -1 && !test_bit(ATH11K_FLAG_BTCOEX, &ar->dev_flags)){
		ret = -EINVAL;
		goto exit;
	}

	if (!coex)
		clear_bit(ATH11K_FLAG_BTCOEX, &ar->dev_flags);

	ret = ath11k_mac_coex_config(ar, arvif, coex, wlan_prio_mask, wlan_weight);
	if (ret)
		goto exit;

	ar->coex.wlan_prio_mask = wlan_prio_mask;
	ar->coex.wlan_weight = wlan_weight;
	ret = count;
exit:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static ssize_t ath11k_read_btcoex(struct file *file, char __user *ubuf,
				  size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	char buf[256];
	int  len=0;

	if (!ar)
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);
	len = scnprintf(buf, sizeof(buf) - len, "%d %d %d\n",
			test_bit(ATH11K_FLAG_BTCOEX, &ar->dev_flags),
			ar->coex.wlan_prio_mask,
			ar->coex.wlan_weight);
	mutex_unlock(&ar->conf_mutex);
	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops__btcoex = {
	.read = ath11k_read_btcoex,
	.write = ath11k_write_btcoex,
	.open = simple_open
};


static ssize_t ath11k_write_btcoex_duty_cycle(struct file *file,
					      const char __user *ubuf,
					      size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif;
	struct ath11k *ar = file->private_data;
	struct coex_config_arg coex_config;
	char buf[256];
	size_t buf_size;
	u32 duty_cycle,wlan_duration;
	int ret;

	if (!ar)
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);
	if (ar->state != ATH11K_STATE_ON) {
		ath11k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
		mutex_unlock(&ar->conf_mutex);
		return -ENETDOWN;
	}
	mutex_unlock(&ar->conf_mutex);

	if (!test_bit(ATH11K_FLAG_BTCOEX, &ar->dev_flags))
		return -EINVAL;

	if (ar->coex.coex_algo_type != COEX_ALGO_OCS) {
		ath11k_err(ar->ab,"duty cycle algo is not enabled");
		return -EINVAL;
	}

	buf_size = min(count, (sizeof(buf) - 1));
	if (copy_from_user(buf, ubuf, buf_size))
		return -EFAULT;

	buf[buf_size] = '\0';
	ret = sscanf(buf, "%d %d" , &duty_cycle, &wlan_duration);

	if (!ret)
		return -EINVAL;

	/*Maximum duty_cycle period allowed is 100 Miliseconds*/
	if (duty_cycle < wlan_duration || !duty_cycle || !wlan_duration || duty_cycle > 100000)
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);
	arvif = list_first_entry(&ar->arvifs, typeof(*arvif), list);
	if (!arvif->is_started) {
		ret = -EINVAL;
		goto exit;
	}

	coex_config.vdev_id = arvif->vdev_id;
	coex_config.config_type = WMI_COEX_CONFIG_AP_TDM;
	coex_config.duty_cycle = duty_cycle;
	coex_config.wlan_duration = wlan_duration;

	ret = ath11k_send_coex_config_cmd(ar, &coex_config);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to set coex config vdev_id %d ret %d\n",
			    coex_config.vdev_id, ret);
		goto exit;
	}

	ar->coex.duty_cycle = duty_cycle;
	ar->coex.wlan_duration = wlan_duration;
	ret = count;
exit:
	mutex_unlock(&ar->conf_mutex);
	return ret;

}

static ssize_t ath11k_read_btcoex_duty_cycle(struct file *file, char __user *ubuf,
					     size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	char buf[256];
	int len =0;

	if (!ar)
		return -EINVAL;

	len = scnprintf(buf, sizeof(buf) - len, "%d %d\n",
			ar->coex.duty_cycle,ar->coex.wlan_duration);
	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}


static const struct file_operations fops__btcoex_duty_cycle = {
	.read = ath11k_read_btcoex_duty_cycle,
	.write = ath11k_write_btcoex_duty_cycle,
	.open = simple_open
};

static ssize_t ath11k_write_btcoex_algo(struct file *file,
					const char __user *ubuf,
					size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif;
	struct ath11k *ar = file->private_data;
	unsigned int coex_algo;
	struct coex_config_arg coex_config;
	int ret;

	if (kstrtouint_from_user(ubuf, count, 0, &coex_algo))
		return -EINVAL;

	if (coex_algo >= COEX_ALGO_MAX_SUPPORTED)
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ath11k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
		mutex_unlock(&ar->conf_mutex);
		return -ENETDOWN;
	}

	arvif = list_first_entry(&ar->arvifs, typeof(*arvif), list);
	if (!arvif->is_started) {
		ret = -EINVAL;
		goto exit;
	}

	ar->coex.coex_algo_type = coex_algo;
	coex_config.vdev_id = arvif->vdev_id;
	coex_config.config_type = WMI_COEX_CONFIG_FORCED_ALGO;
	coex_config.coex_algo = coex_algo;

	ret = ath11k_send_coex_config_cmd(ar, &coex_config);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to set coex algorithm vdev_id %d ret %d\n",
			    coex_config.vdev_id, ret);
		goto exit;
	}

	ret = count;
exit:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static ssize_t ath11k_read_btcoex_algo(struct file *file, char __user *ubuf,
				       size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	char buf[32];
	int len = 0;

	len = scnprintf(buf, sizeof(buf) - len, "%d\n",
			ar->coex.coex_algo_type);
	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_btcoex_algo = {
	.read = ath11k_read_btcoex_algo,
	.write = ath11k_write_btcoex_algo,
	.open = simple_open
};

static ssize_t ath11k_dump_mgmt_stats(struct file *file, char __user *ubuf,
				      size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
#ifndef CPTCFG_ATH11K_MEM_PROFILE_512M
	struct ath11k_base *ab = ar->ab;
#endif
	struct ath11k_vif *arvif = NULL;
	struct ath11k_mgmt_frame_stats *mgmt_stats;
	int len = 0, ret, i;
	int size = (TARGET_NUM_VDEVS(ab) - 1) * 1500;
	char *buf;
	const char *mgmt_frm_type[ATH11K_STATS_MGMT_FRM_TYPE_MAX-1] = {"assoc_req", "assoc_resp",
								       "reassoc_req", "reassoc_resp",
								       "probe_req", "probe_resp",
								       "timing_advertisement", "reserved",
								       "beacon", "atim", "disassoc",
								       "auth", "deauth", "action", "action_no_ack"};

	if (ar->state != ATH11K_STATE_ON)
		return -ENETDOWN;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ATH11K_MEMORY_STATS_INC(ar->ab, malloc_size, size);

	mutex_lock(&ar->conf_mutex);
	spin_lock_bh(&ar->data_lock);

	list_for_each_entry (arvif, &ar->arvifs, list) {
		if (!arvif)
			break;

		if (arvif->vdev_type == WMI_VDEV_TYPE_MONITOR)
			continue;

		mgmt_stats = &arvif->mgmt_stats;
		len += scnprintf(buf + len, size - len, "MGMT frame stats for vdev %u :\n",
				 arvif->vdev_id);
		len += scnprintf(buf + len, size - len, "  TX stats :\n ");
		len += scnprintf(buf + len, size - len, "  Success frames:\n");
		for (i = 0; i < ATH11K_STATS_MGMT_FRM_TYPE_MAX-1; i++)
			len += scnprintf(buf + len, size - len, "	%s: %d\n", mgmt_frm_type[i],
					 mgmt_stats->tx_succ_cnt[i]);

		len += scnprintf(buf + len, size - len, "  Failed frames:\n");

		for (i = 0; i < ATH11K_STATS_MGMT_FRM_TYPE_MAX-1; i++)
			len += scnprintf(buf + len, size - len, "	%s: %d\n", mgmt_frm_type[i],
					 mgmt_stats->tx_fail_cnt[i]);

		len += scnprintf(buf + len, size - len, "  RX stats :\n");
		len += scnprintf(buf + len, size - len, "  Success frames:\n");
		for (i = 0; i < ATH11K_STATS_MGMT_FRM_TYPE_MAX-1; i++)
			len += scnprintf(buf + len, size - len, "	%s: %d\n", mgmt_frm_type[i],
					 mgmt_stats->rx_cnt[i]);

		len += scnprintf(buf + len, size - len, " Tx completion stats :\n");
		len += scnprintf(buf + len, size - len, " success completions:\n");
		for (i = 0; i < ATH11K_STATS_MGMT_FRM_TYPE_MAX-1; i++)
			len += scnprintf(buf + len, size - len, "       %s: %d\n", mgmt_frm_type[i],
					 mgmt_stats->tx_compl_succ[i]);
		len += scnprintf(buf + len, size - len, " failure completions:\n");
		for (i = 0; i < ATH11K_STATS_MGMT_FRM_TYPE_MAX-1; i++)
			len += scnprintf(buf + len, size - len, "       %s: %d\n", mgmt_frm_type[i],
					 mgmt_stats->tx_compl_fail[i]);
	}

	spin_unlock_bh(&ar->data_lock);

	if (len > size)
		len = size;

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, len);
	mutex_unlock(&ar->conf_mutex);
	kfree(buf);

	ATH11K_MEMORY_STATS_DEC(ar->ab, malloc_size, size);

	return ret;
}

static const struct file_operations fops_dump_mgmt_stats = {
	.read = ath11k_dump_mgmt_stats,
	.open = simple_open
};

static ssize_t ath11k_write_enable_m3_dump(struct file *file,
					   const char __user *ubuf,
					   size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	bool enable;
	int ret;

	if (kstrtobool_from_user(ubuf, count, &enable))
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ret = -ENETDOWN;
		goto exit;
	}

	if (enable == ar->debug.enable_m3_dump) {
		ret = count;
		goto exit;
	}

	ret = ath11k_wmi_pdev_m3_dump_enable(ar, enable);
	if (ret) {
		ath11k_warn(ar->ab,
			    "failed to enable m3 ssr dump %d\n",
			    ret);
		goto exit;
	}

	ar->debug.enable_m3_dump = enable;
	ret = count;

exit:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static ssize_t ath11k_read_enable_m3_dump(struct file *file,
					  char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	char buf[32];
	size_t len = 0;

	mutex_lock(&ar->conf_mutex);
	len = scnprintf(buf, sizeof(buf) - len, "%d\n",
			ar->debug.enable_m3_dump);
	mutex_unlock(&ar->conf_mutex);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);

}

static const struct file_operations fops_enable_m3_dump = {
	.read = ath11k_read_enable_m3_dump,
	.write = ath11k_write_enable_m3_dump,
	.open = simple_open
};

static ssize_t ath11k_write_nss_stats(struct file *file,
				      const char __user *ubuf,
				      size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	struct ath11k_base *ab = ar->ab;
	u32 nss_stats;
	int ret;

	if (!ab->nss.enabled) {
		ath11k_warn(ab, "nss offload not enabled\n");
		return -EINVAL;
	}

	if (kstrtouint_from_user(ubuf, count, 0, &nss_stats))
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ret = -ENETDOWN;
		goto out;
	}

	if (nss_stats == ab->nss.stats_enabled) {
		ret = count;
		goto out;
	}

	if (nss_stats > 0) {
		ab->nss.stats_enabled = 1;
		ath11k_nss_peer_stats_enable(ar);
	} else {
		ab->nss.stats_enabled = 0;
		ath11k_nss_peer_stats_disable(ar);
	}

	ret = count;
out:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static ssize_t ath11k_read_nss_stats(struct file *file,
				     char __user *ubuf,
				     size_t count, loff_t *ppos)

{
	char buf[32] = {0};
	struct ath11k *ar = file->private_data;
	struct ath11k_base *ab = ar->ab;
	int len = 0;

	mutex_lock(&ar->conf_mutex);
	len = scnprintf(buf, sizeof(buf) - len, "%08x\n",
			ab->nss.stats_enabled);
	mutex_unlock(&ar->conf_mutex);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_nss_stats = {
	.read = ath11k_read_nss_stats,
	.write = ath11k_write_nss_stats,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_athdiag_read(struct file *file,
				   char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	u8 *buf;
	int ret;

	if (*ppos <= 0)
		return -EINVAL;

	if (!count)
		return 0;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ath11k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
		mutex_unlock(&ar->conf_mutex);
		return -ENETDOWN;
	}

	buf = vmalloc(count);
	if (!buf) {
		ret = -ENOMEM;
		 goto exit;
	}

	ret = ath11k_qmi_mem_read(ar->ab, *ppos, buf, count);
	if (ret < 0) {
		ath11k_warn(ar->ab, "failed to read address 0x%08x via diagnose window from debugfs: %d\n",
			    (u32)(*ppos), ret);
		 goto exit;
	}

	ret = copy_to_user(user_buf, buf, count);
	if (ret) {
		ret = -EFAULT;
		goto exit;
	}

	count -= ret;
	*ppos += count;
	ret = count;
exit:
	vfree(buf);
	mutex_unlock(&ar->conf_mutex);

	return ret;
}

static ssize_t ath11k_athdiag_write(struct file *file,
				    const char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	u8 *buf;
	int ret;

	if (*ppos <= 0)
		return -EINVAL;

	if (!count)
		return 0;

	mutex_lock(&ar->conf_mutex);

	buf = vmalloc(count);
	if (!buf) {
		ret = -ENOMEM;
		goto exit;
	}

	ret = copy_from_user(buf, user_buf, count);
	if (ret) {
		ret = -EFAULT;
		goto exit;
	}

	ret = ath11k_qmi_mem_write(ar->ab, *ppos, buf, count);
	if (ret < 0) {
		ath11k_warn(ar->ab, "failed to write address 0x%08x via diagnose window from debugfs: %d\n",
			    (u32)(*ppos), ret);
		goto exit;
	}

	*ppos += count;
	ret = count;

exit:
	vfree(buf);
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static const struct file_operations fops_athdiag = {
	.read = ath11k_athdiag_read,
	.write = ath11k_athdiag_write,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static int ath11k_get_tpc_ctl_mode(struct wmi_tpc_stats_event *tpc_stats,
				   u32 pream_idx, int *mode)
{
	switch (pream_idx) {
	case WMI_TPC_PREAM_CCK:
		*mode = ATH11K_TPC_STATS_CTL_MODE_CCK;
		break;
	case WMI_TPC_PREAM_OFDM:
		*mode = ATH11K_TPC_STATS_CTL_MODE_OFDM;
		break;
	case WMI_TPC_PREAM_HT20:
	case WMI_TPC_PREAM_VHT20:
	case WMI_TPC_PREAM_HE20:
		*mode = ATH11K_TPC_STATS_CTL_MODE_BW_20;
		break;
	case WMI_TPC_PREAM_HT40:
	case WMI_TPC_PREAM_VHT40:
	case WMI_TPC_PREAM_HE40:
		*mode = ATH11K_TPC_STATS_CTL_MODE_BW_40;
		break;
	case WMI_TPC_PREAM_VHT80:
	case WMI_TPC_PREAM_HE80:
		*mode = ATH11K_TPC_STATS_CTL_MODE_BW_80;
		break;
	case WMI_TPC_PREAM_VHT160:
	case WMI_TPC_PREAM_HE160:
		*mode = ATH11K_TPC_STATS_CTL_MODE_BW_160;
		break;
	default:
		return -EINVAL;
	}

	if (tpc_stats->tpc_config.chan_freq >= 5180) {
		/* Index of 5G is one less than 2.4G due to absence of CCK */
		*mode -= 1;
	}

	return 0;
}

static s16 ath11k_tpc_get_rate(struct ath11k *ar,
			       struct wmi_tpc_stats_event *tpc_stats,
			       u32 rate_idx, u32 num_chains, u32 rate_code,
			       u32 pream_idx, u8 type)
{
	s8 rates_ctl_min, tpc_ctl, tpc_ctl_pri, tpc_ctl_sec;
	u8 chain_idx, stm_idx, num_streams;
	s16 rates, tpc, reg_pwr;
	u32 tot_nss, tot_modes, txbf_on_off, chan_pri_sec;
	u32 index_offset1, index_offset2, index_offset3;
	int mode, ret, txbf_enabled;
	bool is_mu;

	num_streams = 1 + ATH11K_HW_NSS(rate_code);
	chain_idx = num_chains - 1;
	stm_idx = num_streams - 1;
	mode = -1;

	ret = ath11k_get_tpc_ctl_mode(tpc_stats, pream_idx, &mode);
	if (ret) {
		ath11k_warn(ar->ab, "Invalid mode index received\n");
		tpc = TPC_INVAL;
		goto out;
	}

	if (num_chains < num_streams) {
		tpc = TPC_INVAL;
		goto out;
	}

	if (__le32_to_cpu(tpc_stats->tpc_config.num_tx_chain) <= 1) {
		tpc = TPC_INVAL;
		goto out;
	}

	if (type == ATH11K_DBG_TPC_STATS_MU_WITH_TXBF ||
	    type == ATH11K_DBG_TPC_STATS_SU_WITH_TXBF)
		txbf_enabled = 1;
	else
		txbf_enabled = 0;

	if (type == ATH11K_DBG_TPC_STATS_MU_WITH_TXBF ||
	    type == ATH11K_DBG_TPC_STATS_MU) {
		is_mu = true;
	} else {
		is_mu = false;
	}

	/* Below is the min calculation of ctl array, rates array and
	 * regulator power table. tpc is minimum of all 3
	 */
	if (chain_idx < 4) {
		if (is_mu) {
			rates = FIELD_GET(ATH11K_TPC_RATE_ARRAY_MU,
					  tpc_stats->rates_array1.rate_array[rate_idx]);
		} else {
			rates = FIELD_GET(ATH11K_TPC_RATE_ARRAY_SU,
					  tpc_stats->rates_array1.rate_array[rate_idx]);
		}
	} else {
		if (is_mu) {
			rates = FIELD_GET(ATH11K_TPC_RATE_ARRAY_MU,
					  tpc_stats->rates_array2.rate_array[rate_idx]);
		} else {
			rates = FIELD_GET(ATH11K_TPC_RATE_ARRAY_SU,
					  tpc_stats->rates_array2.rate_array[rate_idx]);
		}
	}

	/* ctl_160 array is accessed for BW 160. Mode is subtracted by -1 as
	 * 5G index is one less than 2G due to absence of CCK
	 * ctl array are 4 dimension array which is packed linearly, hence
	 * needs to be stitched back based on the dimension values.
	 * formula : when buf[i][j][k][l] values can be taken as
	 * buf[i*d3*d2*d1 + j*d2*d1 + k*d1 + l]
	 */
	if (tpc_stats->tlvs_rcvd & WMI_TPC_CTL_PWR_160ARRAY &&
	    mode == ATH11K_TPC_STATS_CTL_MODE_BW_160 - 1) {
		tot_nss = tpc_stats->ctl_160array.d1;
		txbf_on_off = tpc_stats->ctl_160array.d2;
		chan_pri_sec = tpc_stats->ctl_160array.d3;
		index_offset1 = chan_pri_sec * txbf_on_off * tot_nss;
		index_offset2 = txbf_on_off * tot_nss;
		index_offset3 = tot_nss;

		if (num_streams < 2)
			num_streams = 2;

		tpc_ctl_pri = *(tpc_stats->ctl_160array.ctl_pwr_table +
				((num_chains / 2) - 1) * index_offset1 +
				0 + txbf_enabled * index_offset3 +
				(((num_streams) / 2) - 1));

		tpc_ctl_sec = *(tpc_stats->ctl_160array.ctl_pwr_table +
				((num_chains / 2) - 1) * index_offset1 +
				1 * index_offset2 + txbf_enabled * index_offset3 +
				(((num_streams) / 2) - 1));
		/* Taking min of pri and sec channel for 160 Mhz */
		tpc_ctl = min_t(s8, tpc_ctl_pri, tpc_ctl_sec);
	} else if (tpc_stats->tlvs_rcvd & WMI_TPC_CTL_PWR_ARRAY) {
		tot_nss = tpc_stats->ctl_array.d1;
		tot_modes = tpc_stats->ctl_array.d2;
		txbf_on_off = tpc_stats->ctl_array.d3;
		index_offset1 = txbf_on_off * tot_modes * tot_nss;
		index_offset2 = tot_modes * tot_nss;
		index_offset3 = tot_nss;

		tpc_ctl = *(tpc_stats->ctl_array.ctl_pwr_table +
			    chain_idx * index_offset1 + txbf_enabled * index_offset2
			    + mode * index_offset3 + stm_idx);
	} else {
		tpc_ctl = TPC_MAX;
		ath11k_info(ar->ab,
			    "ctl array for tpc stats not received from fw\n");
	}

	rates_ctl_min = min_t(s16, rates, tpc_ctl);

	reg_pwr = tpc_stats->max_reg_allowed_power.reg_pwr_array[chain_idx];
	tpc = min_t(s16, rates_ctl_min, reg_pwr);

	/* MODULATION_LIMIT is the maximum power limit,tpc should not exceed
	 * modulation limt even if min tpc of all three array is greater
	 * modulation limit
	 */
	tpc = min_t(s16, tpc, MODULATION_LIMIT);

out:
	return tpc;
}

static bool ath11k_he_supports_extra_mcs(struct ath11k *ar, int freq)
{
	struct ath11k_pdev_cap *cap = &ar->pdev->cap;
	struct ath11k_band_cap *cap_band;
	bool extra_mcs_supported;

	if (freq <= ATH11K_2G_MAX_FREQUENCY)
		cap_band = &cap->band[NL80211_BAND_2GHZ];
	else
		cap_band = &cap->band[NL80211_BAND_5GHZ];

	extra_mcs_supported = FIELD_GET(HE_EXTRA_MCS_SUPPORT, cap_band->he_cap_info[1]);
	return extra_mcs_supported;
}

static int ath11k_tpc_fill_pream(struct ath11k *ar, char *buf, int buf_len, int len,
				 int pream_idx, int max_nss, int max_rates,
				 int pream_type, int tpc_type, int rate_idx)
{
	int nss, rates, chains;
	u8 active_tx_chains;
	u16 rate_code, tpc;
	struct wmi_tpc_stats_event *tpc_stats = ar->tpc_stats;

	static const char pream_str[WMI_TPC_PREAM_MAX][MAX_TPC_PREAM_STR_LEN] = {
				     "CCK",
				     "OFDM",
				     "HT20",
				     "HT40",
				     "VHT20",
				     "VHT40",
				     "VHT80",
				     "VHT160",
				     "HE20",
				     "HE40",
				     "HE80",
				     "HE160"};

	active_tx_chains = ar->num_tx_chains;

	for (nss = 0; nss < max_nss; nss++) {
		for (rates = 0; rates < max_rates; rates++, rate_idx++) {
			/* FW send extra MCS(10&11) for VHT and HE rates,
			 *  this is not used. Hence skipping it here
			 */
			if (pream_type == WMI_RATE_PREAMBLE_VHT &&
			    rates > ATH11K_VHT_MCS_MAX)
				continue;

			if (pream_type == WMI_RATE_PREAMBLE_HE &&
			    rates > ATH11K_HE_MCS_MAX)
				continue;

			rate_code = ATH11K_HW_RATE_CODE(rates, nss, pream_type);
			len += scnprintf(buf + len, buf_len - len,
				 "%d\t %s\t 0x%03x\t", rate_idx,
				 pream_str[pream_idx], rate_code);

			for (chains = 0; chains < active_tx_chains; chains++) {
				/* check for 160Mhz where two chains requires to
				 * support one spatial stream.
				 */
				if ((pream_idx == WMI_TPC_PREAM_VHT160 ||
				     pream_idx == WMI_TPC_PREAM_HE160) &&
				    (((chains + 1) / 2) < nss + 1)) {
					len += scnprintf(buf + len,
							 buf_len - len,
							 "\t%s", "NA");
				} else if (nss > chains) {
					len += scnprintf(buf + len,
							 buf_len - len,
							 "\t%s", "NA");
				} else {
					tpc = ath11k_tpc_get_rate(ar, tpc_stats, rate_idx,
								  chains + 1, rate_code,
								  pream_idx, tpc_type);

					if (tpc == TPC_INVAL) {
						len += scnprintf(buf + len,
						       buf_len - len, "\tNA");
					} else {
						len += scnprintf(buf + len,
						       buf_len - len, "\t%d",
						       tpc);
					}
				}
			}
			len += scnprintf(buf + len, buf_len - len, "\n");
		}
	}
	return len;
}

static int ath11k_tpc_stats_print(struct ath11k *ar,
				  struct wmi_tpc_stats_event *tpc_stats,
				  char *buf, size_t len, u8 type)
{
	u32 i, pream_idx = 0, rate_pream_idx = 0, total_rates = 0;
	u8 nss, active_tx_chains;
	size_t buf_len = ATH11K_TPC_STATS_BUF_SIZE;
	bool he_ext_mcs;
	static const char type_str[ATH11K_DBG_TPC_MAX_STATS][13] = {"SU",
					   "SU WITH TXBF",
					   "MU",
					   "MU WITH TXBF"};

	u8 max_rates[WMI_TPC_PREAM_MAX] = {ATH11K_CCK_RATES,
					   ATH11K_OFDM_RATES,
					   AT11K_HT_RATES,
					   AT11K_HT_RATES,
					   ATH11K_VHT_RATES,
					   ATH11K_VHT_RATES,
					   ATH11K_VHT_RATES,
					   ATH11K_VHT_RATES,
					   ATH11K_HE_RATES,
					   ATH11K_HE_RATES,
					   ATH11K_HE_RATES,
					   ATH11K_HE_RATES};

	u8 max_nss[WMI_TPC_PREAM_MAX] = {ATH11K_NSS_1, ATH11K_NSS_1,
					 ATH11K_NSS_4, ATH11K_NSS_4,
					 ATH11K_NSS_8, ATH11K_NSS_8,
					 ATH11K_NSS_8, ATH11K_NSS_4,
					 ATH11K_NSS_8, ATH11K_NSS_8,
					 ATH11K_NSS_8, ATH11K_NSS_4};

	u16 rate_idx[WMI_TPC_PREAM_MAX] = {0};

	u8 pream_type[WMI_TPC_PREAM_MAX] = {WMI_RATE_PREAMBLE_CCK,
					    WMI_RATE_PREAMBLE_OFDM,
					    WMI_RATE_PREAMBLE_HT,
					    WMI_RATE_PREAMBLE_HT,
					    WMI_RATE_PREAMBLE_VHT,
					    WMI_RATE_PREAMBLE_VHT,
					    WMI_RATE_PREAMBLE_VHT,
					    WMI_RATE_PREAMBLE_VHT,
					    WMI_RATE_PREAMBLE_HE,
					    WMI_RATE_PREAMBLE_HE,
					    WMI_RATE_PREAMBLE_HE,
					    WMI_RATE_PREAMBLE_HE};

	active_tx_chains = ar->num_tx_chains;
	he_ext_mcs = ath11k_he_supports_extra_mcs(ar, tpc_stats->tpc_config.chan_freq);

	/* mcs 12&13 is sent by FW for qcn9000 in rate array, skipping it as
	 * it is not supported
	 */
	if (he_ext_mcs) {
		for (i = WMI_TPC_PREAM_HE20; i <= WMI_TPC_PREAM_HE160;  ++i)
			max_rates[i] = ATH11K_HE_RATES_WITH_EXTRA_MCS;
	}

	if (type == ATH11K_DBG_TPC_STATS_MU ||
	    type == ATH11K_DBG_TPC_STATS_MU_WITH_TXBF)
		pream_idx = WMI_TPC_PREAM_VHT20;

	/* Enumerate all the rate indices */
	for (i = rate_pream_idx + 1 ; i < WMI_TPC_PREAM_MAX; i++) {
		nss = (max_nss[i - 1] < tpc_stats->tpc_config.num_tx_chain ?
		       max_nss[i - 1] : tpc_stats->tpc_config.num_tx_chain);

		if ((i == WMI_TPC_PREAM_VHT160 || i == WMI_TPC_PREAM_HE160) &&
		    (!(tpc_stats->tlvs_rcvd & WMI_TPC_CTL_PWR_160ARRAY))) {
			/* ratesarray doesnot hold any 160Mhz power info,
			 * so skipping here
			 */
			rate_idx[i] = rate_idx[i - 1] + max_rates[i - 1] * nss;
			max_rates[i] = 0;
			max_nss[i] = 0;
			continue;
		}

		rate_idx[i] = rate_idx[i - 1] + max_rates[i - 1] * nss;
	}

	for (i = 0 ; i < WMI_TPC_PREAM_MAX; i++) {
		nss = (max_nss[i] < tpc_stats->tpc_config.num_tx_chain ?
		       max_nss[i] : tpc_stats->tpc_config.num_tx_chain);
		total_rates += max_rates[i] * nss;
	}

	len += scnprintf(buf + len, buf_len - len,
			 "No.of rates-%d\n", total_rates);

	len += scnprintf(buf + len, buf_len - len,
			 "**************** %s ****************\n",
			 type_str[type]);
	len += scnprintf(buf + len, buf_len - len,
			 "\t\t\t\tTPC values for Active chains\n");
	len += scnprintf(buf + len, buf_len - len,
			 "Rate idx Preamble Rate code");

	for (i = 1; i <= active_tx_chains; ++i) {
		len += scnprintf(buf + len, buf_len - len,
				 "\t%d-Chain", i);
	}
	len += scnprintf(buf + len, buf_len - len, "\n");

	for (i = pream_idx; i < WMI_TPC_PREAM_MAX; i++) {
		if (tpc_stats->tpc_config.chan_freq <= 2483) {
			if (i == WMI_TPC_PREAM_VHT80 ||
			    i == WMI_TPC_PREAM_VHT160 ||
			    i == WMI_TPC_PREAM_HE80 ||
			    i == WMI_TPC_PREAM_HE160) {
				continue;
			}
		} else {
			if (i == WMI_TPC_PREAM_CCK)
				continue;
		}

		nss = (max_nss[i] < ar->num_tx_chains ? max_nss[i] : ar->num_tx_chains);

		if (i == WMI_TPC_PREAM_VHT160 || i == WMI_TPC_PREAM_HE160) {
			/* Skip 160MHz in power table if not supported */
			if (!(tpc_stats->tlvs_rcvd & WMI_TPC_CTL_PWR_160ARRAY))
				continue;
		}

		len = ath11k_tpc_fill_pream(ar, buf, buf_len, len, i, nss,
					    max_rates[i], pream_type[i],
					    type, rate_idx[i]);
	}
	return len;
}

static void ath11k_tpc_stats_fill(struct ath11k *ar,
				  struct wmi_tpc_stats_event *tpc_stats,
				  char *buf)
{
	struct wmi_tpc_configs *tpc;
	size_t len = 0;
	size_t buf_len = ATH11K_TPC_STATS_BUF_SIZE;

	spin_lock_bh(&ar->data_lock);
	if (!tpc_stats) {
		ath11k_warn(ar->ab, "failed to find tpc stats\n");
		goto unlock;
	}

	tpc = &tpc_stats->tpc_config;
	len += scnprintf(buf + len, buf_len - len, "\n");
	len += scnprintf(buf + len, buf_len - len,
			 "*************** TPC config **************\n");
	len += scnprintf(buf + len, buf_len - len,
			 "* powers are in 0.25 dBm steps\n");
	len += scnprintf(buf + len, buf_len - len,
			 "reg domain-%d\t\tchan freq-%d\n",
			 tpc->reg_domain, tpc->chan_freq);
	len += scnprintf(buf + len, buf_len - len,
			 "power limit-%d\t\tmax reg-domain Power-%d\n",
			 (tpc->twice_max_reg_power) / 2, tpc->power_limit);
	len += scnprintf(buf + len, buf_len - len,
			 "No.of tx chain-%d\t",
			 ar->num_tx_chains);

	ath11k_tpc_stats_print(ar, tpc_stats, buf, len,
			       ar->tpc_stats_type);

unlock:
	spin_unlock_bh(&ar->data_lock);
}

static int ath11k_debug_tpc_stats_request(struct ath11k *ar)
{
	int ret;
	unsigned long time_left;
	struct ath11k_base *ab = ar->ab;

	lockdep_assert_held(&ar->conf_mutex);

	reinit_completion(&ar->tpc_complete);

	ret = ath11k_wmi_pdev_get_tpc_table_cmdid(ar);
	if (ret) {
		ath11k_warn(ab, "failed to request tpc table cmdid: %d\n", ret);
		return ret;
	}

	spin_lock_bh(&ar->data_lock);
	ar->tpc_request = true;
	spin_unlock_bh(&ar->data_lock);

	time_left = wait_for_completion_timeout(&ar->tpc_complete,
						TPC_STATS_WAIT_TIME);
	spin_lock_bh(&ar->data_lock);
	ar->tpc_request = false;
	spin_unlock_bh(&ar->data_lock);

	if (time_left == 0)
		return -ETIMEDOUT;

	return 0;
}

static int ath11k_tpc_stats_open(struct inode *inode, struct file *file)
{
	struct ath11k *ar = inode->i_private;
	void *buf;
	int ret;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ath11k_warn(ar->ab, "Interface not up\n");
		ret = -ENETDOWN;
		goto err_unlock;
	}

	buf = vmalloc(ATH11K_TPC_STATS_BUF_SIZE);
	if (!buf) {
		ret = -ENOMEM;
		goto err_unlock;
	}

	ret = ath11k_debug_tpc_stats_request(ar);
	if (ret) {
		ath11k_warn(ar->ab, "failed to request tpc stats: %d\n",
			    ret);
		spin_lock_bh(&ar->data_lock);
		ath11k_wmi_free_tpc_stats_mem(ar);
		spin_unlock_bh(&ar->data_lock);
		goto err_free;
	}

	ath11k_tpc_stats_fill(ar, ar->tpc_stats, buf);
	file->private_data = buf;

	spin_lock_bh(&ar->data_lock);
	ath11k_wmi_free_tpc_stats_mem(ar);
	spin_unlock_bh(&ar->data_lock);
	mutex_unlock(&ar->conf_mutex);

	return 0;

err_free:
	vfree(buf);

err_unlock:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static int ath11k_tpc_stats_release(struct inode *inode,
				    struct file *file)
{
	vfree(file->private_data);
	return 0;
}

static ssize_t ath11k_tpc_stats_read(struct file *file,
				     char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	const char *buf = file->private_data;
	unsigned int len = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath11k_read_tpc_stats_type(struct file *file,
					  char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	char buf[32];
	size_t len;

	len = scnprintf(buf, sizeof(buf), "%u\n", ar->tpc_stats_type);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath11k_write_tpc_stats_type(struct file *file,
					   const char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	u8 type;
	int ret;

	ret = kstrtou8_from_user(user_buf, count, 0, &type);
	if (ret)
		return ret;

	if (type >= ATH11K_DBG_TPC_MAX_STATS)
		return -E2BIG;

	ar->tpc_stats_type = type;

	ret = count;

	return ret;
}

static const struct file_operations fops_tpc_stats_type = {
	.read = ath11k_read_tpc_stats_type,
	.write = ath11k_write_tpc_stats_type,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static const struct file_operations fops_tpc_stats = {
	.open = ath11k_tpc_stats_open,
	.release = ath11k_tpc_stats_release,
	.read = ath11k_tpc_stats_read,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_write_disable_dynamic_bw(struct file *file,
					       const char __user *ubuf,
					       size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	u32 filter;
	int ret;

	if (kstrtouint_from_user(ubuf, count, 0, &filter))
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ret = -ENETDOWN;
		goto out;
	}

	if (filter == ar->debug.disable_dynamic_bw) {
		ret = count;
		goto out;
	}

	ret = ath11k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_DYNAMIC_BW, !filter,
					ar->pdev->pdev_id);
	if (ret) {
		ath11k_err(ar->ab, "failed to %s dynamic bw: %d\n",
			   filter ? "disable" : "enable", ret);
		goto out;
	}

	ar->debug.disable_dynamic_bw = filter;
	ret = count;

out:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static ssize_t ath11k_read_disable_dynamic_bw(struct file *file,
					      char __user *ubuf,
					      size_t count, loff_t *ppos)

{
	char buf[32] = {0};
	struct ath11k *ar = file->private_data;
	int len = 0;

	mutex_lock(&ar->conf_mutex);
	len = scnprintf(buf, sizeof(buf) - len, "%08x\n",
			ar->debug.disable_dynamic_bw);
	mutex_unlock(&ar->conf_mutex);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_disable_dyn_bw = {
	.read = ath11k_read_disable_dynamic_bw,
	.write = ath11k_write_disable_dynamic_bw,
	.open = simple_open
};

static ssize_t ath11k_read_ani_enable(struct file *file, char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	int len = 0;
	char buf[32];

	len = scnprintf(buf, sizeof(buf) - len, "%d\n",ar->ani_enabled);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath11k_write_ani_enable(struct file *file,
				       const char __user *user_buf,
				       size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	int ret;
	u8 enable;

	if (kstrtou8_from_user(user_buf, count, 0, &enable))
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ath11k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
		mutex_unlock(&ar->conf_mutex);
		return -ENETDOWN;
	}

	if (ar->ani_enabled == enable) {
		ret = count;
		goto exit;
	}

	ret = ath11k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ANI_ENABLE,
					enable, ar->pdev->pdev_id);
	if (ret) {
		ath11k_warn(ar->ab, "ani_enable failed from debugfs: %d\n", ret);
		goto exit;
	}
	ar->ani_enabled = enable;
	ret = count;

exit:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static const struct file_operations fops_ani_enable = {
	.read = ath11k_read_ani_enable,
	.write = ath11k_write_ani_enable,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_read_ani_poll_period(struct file *file,
					   char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	int len = 0;
	char buf[32];

	len = scnprintf(buf, sizeof(buf) - len, "%u\n", ar->ab->ani_poll_period);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath11k_write_ani_poll_period(struct file *file,
					    const char __user *user_buf,
					    size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	int ret;
	u32 ani_poll_period;

	if (kstrtou32_from_user(user_buf, count, 0, &ani_poll_period))
		return -EINVAL;

	if(ani_poll_period > ATH11K_ANI_POLL_PERIOD_MAX)
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ath11k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
		mutex_unlock(&ar->conf_mutex);
		return -ENETDOWN;
	}

	ret = ath11k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ANI_POLL_PERIOD,
			ani_poll_period, ar->pdev->pdev_id);
	if (ret) {
		ath11k_warn(ar->ab, "ani poll period write failed in debugfs: %d\n", ret);
		goto exit;
	}
	ar->ab->ani_poll_period = ani_poll_period;
	ret = count;

exit:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static const struct file_operations fops_ani_poll_period = {
	.read = ath11k_read_ani_poll_period,
	.write = ath11k_write_ani_poll_period,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_read_ani_listen_period(struct file *file,
					     char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	int len = 0;
	char buf[32];

	len = scnprintf(buf, sizeof(buf) - len, "%u\n", ar->ab->ani_listen_period);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath11k_write_ani_listen_period(struct file *file,
					      const char __user *user_buf,
					      size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	int ret;
	u32 ani_listen_period = 0;

	if (kstrtou32_from_user(user_buf, count, 0, &ani_listen_period))
		return -EINVAL;

	if(ani_listen_period > ATH11K_ANI_LISTEN_PERIOD_MAX)
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON) {
		ath11k_warn(ar->ab, "pdev %d not in ON state\n", ar->pdev->pdev_id);
		mutex_unlock(&ar->conf_mutex);
		return -ENETDOWN;
	}

	ret = ath11k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ANI_LISTEN_PERIOD,
					ani_listen_period, ar->pdev->pdev_id);
	if (ret) {
		ath11k_warn(ar->ab, "ani listen period write failed in debugfs: %d\n", ret);
		goto exit;
	}
	ar->ab->ani_listen_period = ani_listen_period;
	ret = count;

exit:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static const struct file_operations fops_ani_listen_period = {
	.read = ath11k_read_ani_listen_period,
	.write = ath11k_write_ani_listen_period,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static int ath11k_debug_get_ani_level(struct ath11k *ar)
{
	unsigned long time_left;
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	reinit_completion(&(ar->ab->ani_ofdm_event));

	ret = ath11k_wmi_pdev_get_ani_level(ar, WMI_PDEV_GET_ANI_OFDM_CONFIG_CMDID,
					    ar->pdev->pdev_id);
	if (ret) {
		ath11k_warn(ar->ab, "failed to request ofdm ani level: %d\n", ret);
		return ret;
	}
	time_left = wait_for_completion_timeout(&ar->ab->ani_ofdm_event, 1 * HZ);
	if (time_left == 0)
		return -ETIMEDOUT;

	if (ar->ab->target_caps.phy_capability & WHAL_WLAN_11G_CAPABILITY) {
		reinit_completion(&(ar->ab->ani_cck_event));
		ret = ath11k_wmi_pdev_get_ani_level(ar, WMI_PDEV_GET_ANI_CCK_CONFIG_CMDID,
						    ar->pdev->pdev_id);
		if (ret) {
			ath11k_warn(ar->ab, "failed to request cck ani level: %d\n", ret);
			return ret;
		}
		time_left = wait_for_completion_timeout(&ar->ab->ani_cck_event, 1 * HZ);
		if (time_left == 0)
			return -ETIMEDOUT;
	}

	return 0;
}

static ssize_t ath11k_read_ani_level(struct file *file, char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	char buf[128];
	int ret, len = 0;

	mutex_lock(&ar->conf_mutex);
	if (ar->state != ATH11K_STATE_ON) {
		ret = -ENETDOWN;
		goto unlock;
	}

	if(!ar->ani_enabled) {
		len += scnprintf(buf, sizeof(buf), "ANI is disabled\n");
	} else {
		ret = ath11k_debug_get_ani_level(ar);
		if (ret) {
			ath11k_warn(ar->ab, "failed to request ani level: %d\n", ret);
			goto unlock;
		}
		len += scnprintf(buf, sizeof(buf), "ofdm level %d cck level %d\n",
				ar->ab->ani_ofdm_level, ar->ab->ani_cck_level);
	}
	mutex_unlock(&ar->conf_mutex);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);

unlock:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static ssize_t ath11k_write_ani_level(struct file *file,
				      const char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	char buf[32] = {0};
	ssize_t rc;
	u32 ofdm_param = 0, cck_param = 0;
	int ofdm_level, cck_level;
	int ret;

	rc = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (rc < 0)
		return rc;

	buf[*ppos - 1] = '\0';

	ret = sscanf(buf, "%d %d", &ofdm_level, &cck_level);

	if (ret != 2)
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON && ar->state != ATH11K_STATE_RESTARTED) {
		ret = -ENETDOWN;
		goto exit;
	}

	if ((ofdm_level >= ATH11K_ANI_LEVEL_MIN && ofdm_level <= ATH11K_ANI_LEVEL_MAX) ||
	   (ofdm_level == ATH11K_ANI_LEVEL_AUTO)) {
		ofdm_param = WMI_PDEV_PARAM_ANI_OFDM_LEVEL;
	} else {
		ret = -EINVAL;
		goto exit;
	}

	if((ar->ab->target_caps.phy_capability & WHAL_WLAN_11G_CAPABILITY)) {
		if ((cck_level >= ATH11K_ANI_LEVEL_MIN &&
		   cck_level <= ATH11K_ANI_LEVEL_MAX) ||
		   (cck_level == ATH11K_ANI_LEVEL_AUTO)) {
			cck_param = WMI_PDEV_PARAM_ANI_CCK_LEVEL;
		} else {
			ret = -EINVAL;
			goto exit;
		}
	}

	ret = ath11k_wmi_pdev_set_param(ar, ofdm_param, ofdm_level, ar->pdev->pdev_id);
	if (ret) {
		ath11k_warn(ar->ab, "failed to set ANI ofdm level :%d\n", ret);
		goto exit;
	}

	if (cck_param) {
		ret = ath11k_wmi_pdev_set_param(ar, cck_param, cck_level,
						ar->pdev->pdev_id);
		if (ret) {
			ath11k_warn(ar->ab, "failed to set ANI cck level :%d\n", ret);
			goto exit;
		}
	}

	ret = count;
exit:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static const struct file_operations fops_ani_level = {
	.write = ath11k_write_ani_level,
	.read = ath11k_read_ani_level,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_medium_busy_read(struct file *file,
				       char __user *user_buf,
				       size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	u8 buf[50];
	size_t len = 0;

	mutex_lock(&ar->conf_mutex);
	len += scnprintf(buf + len, sizeof(buf) - len,
			 "Medium Busy in percentage %u\n",
			 ar->hw->medium_busy);
	mutex_unlock(&ar->conf_mutex);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_medium_busy = {
	.read = ath11k_medium_busy_read,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_coex_priority_read(struct file *file,
					 char __user *user_buf,
					 size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	u8 buf[128];
	size_t len = 0;
	int i;

	mutex_lock(&ar->conf_mutex);
	for (i = 0; i < ATH11K_MAX_COEX_PRIORITY_LEVEL; i++) {
		len += scnprintf(buf + len, sizeof(buf) - len,
				 "priority[%d] :  %u\n", i,
				 ar->debug.coex_priority_level[0]);
	}
	mutex_unlock(&ar->conf_mutex);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath11k_coex_priority_write(struct file *file,
					  const char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	struct ath11k_vif *arvif = NULL;
	struct coex_config_arg coex_config;
	char buf[128] = {0};
	int ret;
	u32 temp_priority[ATH11K_MAX_COEX_PRIORITY_LEVEL] = {0};
	u32 config_type = 0xFF;

	mutex_lock(&ar->conf_mutex);

	if (ar->state != ATH11K_STATE_ON &&
	    ar->state != ATH11K_STATE_RESTARTED) {
		ret = -ENETDOWN;
		goto exit;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret < 0)
		goto exit;

	ret = sscanf(buf, "%x %x %x %x", &config_type, &temp_priority[0],
		     &temp_priority[1], &temp_priority[2]);
	if ((config_type == 1 && ret != 4) || (config_type > 1)) {
		ret = -EINVAL;
		goto exit;
	}

	list_for_each_entry(arvif, &ar->arvifs, list) {
		coex_config.vdev_id = arvif->vdev_id;
		if (config_type == 1) {
			coex_config.config_type = WMI_COEX_CONFIG_THREE_WAY_COEX_START;
			coex_config.priority0 = temp_priority[0];
			coex_config.priority1 = temp_priority[1];
			coex_config.priority2 = temp_priority[2];
		} else {
			coex_config.config_type = WMI_COEX_CONFIG_THREE_WAY_COEX_RESET;
		}
		ret = ath11k_send_coex_config_cmd(ar, &coex_config);
		if (ret) {
			ath11k_warn(ar->ab,
				    "failed to set coex config vdev_id %d ret %d\n",
				    coex_config.vdev_id, ret);
			goto exit;
		}
	}

	memcpy(ar->debug.coex_priority_level, temp_priority,
	       sizeof(ar->debug.coex_priority_level));

	ret = count;
exit:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static const struct file_operations fops_coex_priority = {
	.read = ath11k_coex_priority_read,
	.write = ath11k_coex_priority_write,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_bss_survey_mode_read(struct file *file,
					   char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	char buf[32] = {0};
	size_t len = 0;

	mutex_lock(&ar->conf_mutex);
	len += scnprintf(buf + len, sizeof(buf) - len,
			 "%u\n",
			 ar->debug.bss_survey_mode);
	mutex_unlock(&ar->conf_mutex);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath11k_bss_survey_mode_write(struct file *file,
					    const char __user *user_buf,
					    size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	u32 survey_mode;

	if (kstrtouint_from_user(user_buf, count, 0, &survey_mode))
		return -EINVAL;

	if ((survey_mode != WMI_BSS_SURVEY_REQ_TYPE_READ) &&
	    (survey_mode != WMI_BSS_SURVEY_REQ_TYPE_READ_CLEAR))
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);

	ar->debug.bss_survey_mode = survey_mode;

	mutex_unlock(&ar->conf_mutex);
	return count;
}

static const struct file_operations fops_bss_survey_mode = {
	.read = ath11k_bss_survey_mode_read,
	.write = ath11k_bss_survey_mode_write,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

int ath11k_debugfs_register(struct ath11k *ar)
{
	struct ath11k_base *ab = ar->ab;
	char pdev_name[10];
	char buf[100] = {0};

	snprintf(pdev_name, sizeof(pdev_name), "%s%u", "mac", ar->pdev_idx);

	ar->debug.debugfs_pdev = debugfs_create_dir(pdev_name, ab->debugfs_soc);
	if (IS_ERR(ar->debug.debugfs_pdev))
		return PTR_ERR(ar->debug.debugfs_pdev);

	/* Create a symlink under ieee80211/phy* */
	snprintf(buf, 100, "../../ath11k/%pd2", ar->debug.debugfs_pdev);
	debugfs_create_symlink("ath11k", ar->hw->wiphy->debugfsdir, buf);

	ath11k_debugfs_htt_stats_init(ar);

	ath11k_debugfs_fw_stats_init(ar);
	ath11k_init_tx_latency_stats(ar);
	ath11k_init_pktlog(ar);
	ath11k_smart_ant_debugfs_init(ar);
	init_completion(&ar->tpc_complete);
        init_completion(&ab->ani_ofdm_event);
        init_completion(&ab->ani_cck_event);

	memset(&ar->wmm_stats, 0, sizeof(struct ath11k_wmm_stats));

	debugfs_create_file("wmm_stats", 0644,
			    ar->debug.debugfs_pdev, ar,
			    &fops_wmm_stats);

	debugfs_create_file("neighbor_peer", 0644,
			    ar->debug.debugfs_pdev, ar,
			    &fops_write_nrp_mac);
	debugfs_create_file("ext_tx_stats", 0644,
			    ar->debug.debugfs_pdev, ar,
			    &fops_extd_tx_stats);
	debugfs_create_file("ext_rx_stats", 0644,
			    ar->debug.debugfs_pdev, ar,
			    &fops_extd_rx_stats);
	debugfs_create_file("pktlog_filter", 0644,
			    ar->debug.debugfs_pdev, ar,
			    &fops_pktlog_filter);
	debugfs_create_file("fw_dbglog_config", 0600,
			    ar->debug.debugfs_pdev, ar,
			    &fops_fw_dbglog);
	debugfs_create_file("btcoex", 0644,
			    ar->debug.debugfs_pdev, ar,
			    &fops__btcoex);
	debugfs_create_file("btcoex_duty_cycle", 0644,
			    ar->debug.debugfs_pdev, ar,
			    &fops__btcoex_duty_cycle);
	debugfs_create_file("btcoex_algorithm", 0644,
			    ar->debug.debugfs_pdev, ar,
			    &fops_btcoex_algo);
	debugfs_create_file("dump_mgmt_stats", 0644,
			    ar->debug.debugfs_pdev, ar,
			    &fops_dump_mgmt_stats);
	debugfs_create_file("disable_dynamic_bw", 0644,
			    ar->debug.debugfs_pdev, ar,
			    &fops_disable_dyn_bw);

	if (ar->hw->wiphy->bands[NL80211_BAND_5GHZ]) {
		debugfs_create_file("dfs_simulate_radar", 0200,
				    ar->debug.debugfs_pdev, ar,
				    &fops_simulate_radar);
		debugfs_create_bool("dfs_block_radar_events", 0200,
				    ar->debug.debugfs_pdev,
				    &ar->dfs_block_radar_events);
	}

	if (ab->hw_params.dbr_debug_support)
		debugfs_create_file("enable_dbr_debug", 0200, ar->debug.debugfs_pdev,
				    ar, &fops_dbr_debug);

	debugfs_create_file("ps_state_enable", 0600, ar->debug.debugfs_pdev, ar,
			    &fops_ps_state_enable);

	if (test_bit(WMI_TLV_SERVICE_PEER_POWER_SAVE_DURATION_SUPPORT,
		     ar->ab->wmi_ab.svc_map)) {
		debugfs_create_file("ps_timekeeper_enable", 0600,
				    ar->debug.debugfs_pdev, ar,
				    &fops_ps_timekeeper_enable);

		debugfs_create_file("reset_ps_duration", 0200,
				    ar->debug.debugfs_pdev, ar,
				    &fops_reset_ps_duration);
	}

	if (ar->hw->wiphy->bands[NL80211_BAND_6GHZ]) {
		debugfs_create_file("simulate_awgn", 0200,
				    ar->debug.debugfs_pdev, ar,
				    &fops_simulate_awgn);
	}

	debugfs_create_file("enable_m3_dump", 0644,
			    ar->debug.debugfs_pdev, ar,
			    &fops_enable_m3_dump);
	debugfs_create_file("tpc_stats", 0400, ar->debug.debugfs_pdev,
			    ar, &fops_tpc_stats);
	debugfs_create_file("tpc_stats_type", 0600, ar->debug.debugfs_pdev,
			    ar, &fops_tpc_stats_type);

	if (ab->nss.enabled)
		debugfs_create_file("nss_peer_stats_config", 0644,
				    ar->debug.debugfs_pdev, ar, &fops_nss_stats);

	debugfs_create_file("athdiag", S_IRUSR | S_IWUSR,
			    ar->debug.debugfs_pdev, ar,
			    &fops_athdiag);

	debugfs_create_file("ani_enable", S_IRUSR | S_IWUSR,
			    ar->debug.debugfs_pdev, ar, &fops_ani_enable);
	debugfs_create_file("ani_level", S_IRUSR | S_IWUSR,
			    ar->debug.debugfs_pdev, ar, &fops_ani_level);
	debugfs_create_file("ani_poll_period", S_IRUSR | S_IWUSR,
			    ar->debug.debugfs_pdev, ar, &fops_ani_poll_period);
	debugfs_create_file("ani_listen_period", S_IRUSR | S_IWUSR,
			    ar->debug.debugfs_pdev, ar, &fops_ani_listen_period);
	debugfs_create_file("medium_busy", S_IRUSR, ar->debug.debugfs_pdev, ar,
			    &fops_medium_busy);
	debugfs_create_file("coex_priority", 0600,
			    ar->debug.debugfs_pdev, ar, &fops_coex_priority);
	debugfs_create_file("bss_survey_mode", 0644,
			    ar->debug.debugfs_pdev, ar,
			    &fops_bss_survey_mode);
	return 0;
}

void ath11k_debugfs_unregister(struct ath11k *ar)
{
	struct ath11k_debug_dbr *dbr_debug;
	struct ath11k_dbg_dbr_data *dbr_dbg_data;
	int i;

	for (i = 0; i < WMI_DIRECT_BUF_MAX; i++) {
		dbr_debug = ar->debug.dbr_debug[i];
		if (!dbr_debug)
			continue;

		dbr_dbg_data = &dbr_debug->dbr_dbg_data;
		kfree(dbr_dbg_data->entries);
		debugfs_remove_recursive(dbr_debug->dbr_debugfs);
		kfree(dbr_debug);
		ar->debug.dbr_debug[i] = NULL;
	}

	ath11k_deinit_pktlog(ar);
	kfree(ar->debug.tx_delay_stats[0]);
	debugfs_remove_recursive(ar->debug.debugfs_pdev);
	ar->debug.debugfs_pdev = NULL;
}

static ssize_t ath11k_write_twt_add_dialog(struct file *file,
					   const char __user *ubuf,
					   size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	struct wmi_twt_add_dialog_params params = { 0 };
	struct wmi_twt_enable_params twt_params = {0};
	struct ath11k *ar = arvif->ar;
	u8 buf[128] = {0};
	int ret;

	if (ar->twt_enabled == 0) {
		ath11k_err(ar->ab, "twt support is not enabled\n");
		return -EOPNOTSUPP;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf,
		     "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %u %u %u %u %u %hhu %hhu %hhu %hhu %hhu",
		     &params.peer_macaddr[0],
		     &params.peer_macaddr[1],
		     &params.peer_macaddr[2],
		     &params.peer_macaddr[3],
		     &params.peer_macaddr[4],
		     &params.peer_macaddr[5],
		     &params.dialog_id,
		     &params.wake_intvl_us,
		     &params.wake_intvl_mantis,
		     &params.wake_dura_us,
		     &params.sp_offset_us,
		     &params.twt_cmd,
		     &params.flag_bcast,
		     &params.flag_trigger,
		     &params.flag_flow_type,
		     &params.flag_protection);
	if (ret != 16)
		return -EINVAL;

	/* In the case of station vif, TWT is entirely handled by
	 * the firmware based on the input parameters in the TWT enable
	 * WMI command that is sent to the target during assoc.
	 * For manually testing the TWT feature, we need to first disable
	 * TWT and send enable command again with TWT input parameter
	 * sta_cong_timer_ms set to 0.
	 */
	if (arvif->vif->type == NL80211_IFTYPE_STATION) {
		ath11k_wmi_send_twt_disable_cmd(ar, ar->pdev->pdev_id);

		ath11k_wmi_fill_default_twt_params(&twt_params);
		twt_params.sta_cong_timer_ms = 0;

		ath11k_wmi_send_twt_enable_cmd(ar, ar->pdev->pdev_id, &twt_params);
	}

	params.vdev_id = arvif->vdev_id;

	ret = ath11k_wmi_send_twt_add_dialog_cmd(arvif->ar, &params);
	if (ret)
		goto err_twt_add_dialog;

	return count;

err_twt_add_dialog:
	if (arvif->vif->type == NL80211_IFTYPE_STATION) {
		ath11k_wmi_send_twt_disable_cmd(ar, ar->pdev->pdev_id);
		ath11k_wmi_fill_default_twt_params(&twt_params);
		ath11k_wmi_send_twt_enable_cmd(ar, ar->pdev->pdev_id, &twt_params);
	}

	return ret;
}

static ssize_t ath11k_write_twt_del_dialog(struct file *file,
					   const char __user *ubuf,
					   size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	struct wmi_twt_del_dialog_params params = { 0 };
	struct wmi_twt_enable_params twt_params = {0};
	struct ath11k *ar = arvif->ar;
	u8 buf[64] = {0};
	int ret;

	if (ar->twt_enabled == 0) {
		ath11k_err(ar->ab, "twt support is not enabled\n");
		return -EOPNOTSUPP;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %u",
		     &params.peer_macaddr[0],
		     &params.peer_macaddr[1],
		     &params.peer_macaddr[2],
		     &params.peer_macaddr[3],
		     &params.peer_macaddr[4],
		     &params.peer_macaddr[5],
		     &params.dialog_id);
	if (ret != 7)
		return -EINVAL;

	params.vdev_id = arvif->vdev_id;

	ret = ath11k_wmi_send_twt_del_dialog_cmd(arvif->ar, &params);
	if (ret)
		return ret;

	if (arvif->vif->type == NL80211_IFTYPE_STATION) {
		ath11k_wmi_send_twt_disable_cmd(ar, ar->pdev->pdev_id);
		ath11k_wmi_fill_default_twt_params(&twt_params);
		ath11k_wmi_send_twt_enable_cmd(ar, ar->pdev->pdev_id, &twt_params);
	}

	return count;
}

static ssize_t ath11k_write_twt_pause_dialog(struct file *file,
					     const char __user *ubuf,
					     size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	struct wmi_twt_pause_dialog_params params = { 0 };
	u8 buf[64] = {0};
	int ret;

	if (arvif->ar->twt_enabled == 0) {
		ath11k_err(arvif->ar->ab, "twt support is not enabled\n");
		return -EOPNOTSUPP;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %u",
		     &params.peer_macaddr[0],
		     &params.peer_macaddr[1],
		     &params.peer_macaddr[2],
		     &params.peer_macaddr[3],
		     &params.peer_macaddr[4],
		     &params.peer_macaddr[5],
		     &params.dialog_id);
	if (ret != 7)
		return -EINVAL;

	params.vdev_id = arvif->vdev_id;

	ret = ath11k_wmi_send_twt_pause_dialog_cmd(arvif->ar, &params);
	if (ret)
		return ret;

	return count;
}

static ssize_t ath11k_write_twt_resume_dialog(struct file *file,
					      const char __user *ubuf,
					      size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	struct wmi_twt_resume_dialog_params params = { 0 };
	u8 buf[64] = {0};
	int ret;

	if (arvif->ar->twt_enabled == 0) {
		ath11k_err(arvif->ar->ab, "twt support is not enabled\n");
		return -EOPNOTSUPP;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %u %u %u",
		     &params.peer_macaddr[0],
		     &params.peer_macaddr[1],
		     &params.peer_macaddr[2],
		     &params.peer_macaddr[3],
		     &params.peer_macaddr[4],
		     &params.peer_macaddr[5],
		     &params.dialog_id,
		     &params.sp_offset_us,
		     &params.next_twt_size);
	if (ret != 9)
		return -EINVAL;

	params.vdev_id = arvif->vdev_id;

	ret = ath11k_wmi_send_twt_resume_dialog_cmd(arvif->ar, &params);
	if (ret)
		return ret;

	return count;
}

static const struct file_operations ath11k_fops_twt_add_dialog = {
	.write = ath11k_write_twt_add_dialog,
	.open = simple_open
};

static const struct file_operations ath11k_fops_twt_del_dialog = {
	.write = ath11k_write_twt_del_dialog,
	.open = simple_open
};

static const struct file_operations ath11k_fops_twt_pause_dialog = {
	.write = ath11k_write_twt_pause_dialog,
	.open = simple_open
};

static const struct file_operations ath11k_fops_twt_resume_dialog = {
	.write = ath11k_write_twt_resume_dialog,
	.open = simple_open
};

static ssize_t ath11k_write_wmi_ctrl_path_stats(struct file *file,
		const char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	struct wmi_ctrl_path_stats_cmd_param param = {0};
	u8 buf[128] = {0};
	int ret;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0) {
		return ret;
	}

	buf[ret] = '\0';

	ret = sscanf(buf, "%u %u", &param.stats_id, &param.action);
	if (ret != 2)
		return -EINVAL;

	if (!param.action || param.action > WMI_REQ_CTRL_PATH_STAT_RESET)
		return -EINVAL;

	ret = ath11k_wmi_send_wmi_ctrl_stats_cmd(arvif->ar, &param);
	return ret ? ret : count;
}

int wmi_ctrl_path_pdev_stat(struct ath11k_vif *arvif, char __user *ubuf,
			    size_t count, loff_t *ppos)
{
	const int size = 2048;
	char *buf;
	u8 i;
	int len = 0, ret_val;
	u16 index_tx = 0;
	u16 index_rx = 0;
	char fw_tx_mgmt_subtype[WMI_MAX_STRING_LEN] = {0};
	char fw_rx_mgmt_subtype[WMI_MAX_STRING_LEN] = {0};
	struct wmi_ctrl_path_stats_list *stats;
	struct wmi_ctrl_path_pdev_stats *pdev_stats;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	list_for_each_entry (stats, &arvif->ar->debug.wmi_list, list) {
		if (!stats) {
			break;
		}

		pdev_stats = stats->stats_ptr;

		for (i = 0; i < WMI_MGMT_FRAME_SUBTYPE_MAX; i++) {
			index_tx += snprintf(&fw_tx_mgmt_subtype[index_tx],
			        WMI_MAX_STRING_LEN - index_tx,
			        " %u:%u,", i,
			        pdev_stats->tx_mgmt_subtype[i]);
			index_rx += snprintf(&fw_rx_mgmt_subtype[index_rx],
			         WMI_MAX_STRING_LEN - index_rx,
			         " %u:%u,", i,
			         pdev_stats->rx_mgmt_subtype[i]);
		}

		len += scnprintf(buf + len, size - len,
				 "WMI_CTRL_PATH_PDEV_TX_STATS: \n");
		len += scnprintf(buf + len, size - len,
				 "fw_tx_mgmt_subtype = %s \n",
				 fw_tx_mgmt_subtype);
		len += scnprintf(buf + len, size - len,
				 "fw_rx_mgmt_subtype = %s \n",
				 fw_rx_mgmt_subtype);
		len += scnprintf(buf + len, size - len,
				 "scan_fail_dfs_violation_time_ms = %u \n",
				 pdev_stats->scan_fail_dfs_violation_time_ms);
		len += scnprintf(buf + len, size - len,
				 "nol_chk_fail_last_chan_freq = %u \n",
				 pdev_stats->nol_chk_fail_last_chan_freq);
		len += scnprintf(buf + len, size - len,
				 "nol_chk_fail_time_stamp_ms = %u \n",
				 pdev_stats->nol_chk_fail_time_stamp_ms);
		len += scnprintf(buf + len, size - len,
				 "tot_peer_create_cnt = %u \n",
				 pdev_stats->tot_peer_create_cnt);
		len += scnprintf(buf + len, size - len,
				 "tot_peer_del_cnt = %u \n",
				 pdev_stats->tot_peer_del_cnt);
		len += scnprintf(buf + len, size - len,
				 "tot_peer_del_resp_cnt = %u \n",
				 pdev_stats->tot_peer_del_resp_cnt);
		len += scnprintf(buf + len, size - len,
				 "vdev_pause_fail_rt_to_sched_algo_fifo_full_cnt = %u \n",
				 pdev_stats->vdev_pause_fail_rt_to_sched_algo_fifo_full_cnt);

	}

	ret_val =  simple_read_from_buffer(ubuf, count, ppos, buf, len);
	ath11k_wmi_crl_path_stats_list_free(&arvif->ar->debug.wmi_list);
	kfree(buf);
	return ret_val;
}

int wmi_ctrl_path_cal_stat(struct ath11k_vif *arvif, char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	const int size = 4096;
	char *buf;
	u8 cal_type_mask, cal_prof_mask, is_periodic_cal;
	int len = 0, ret_val;
	struct wmi_ctrl_path_stats_list *stats;
	struct wmi_ctrl_path_cal_stats *cal_stats;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, size - len,
			 "WMI_CTRL_PATH_CAL_STATS\n");
	len += scnprintf(buf + len, size - len,
			 "%-25s %-25s %-17s %-16s %-16s %-16s\n",
			 "cal_profile", "cal_type",
			 "cal_triggered_cnt", "cal_fail_cnt",
			 "cal_fcs_cnt", "cal_fcs_fail_cnt");

	list_for_each_entry (stats, &arvif->ar->debug.wmi_list, list) {
		if (!stats)
			break;

		cal_stats = stats->stats_ptr;

		cal_prof_mask = FIELD_GET(WMI_CTRL_PATH_CAL_PROF_MASK,
					  cal_stats->cal_info);
		if (cal_prof_mask == WMI_CTRL_PATH_STATS_CAL_PROFILE_INVALID)
			continue;

		cal_type_mask = FIELD_GET(WMI_CTRL_PATH_CAL_TYPE_MASK,
					  cal_stats->cal_info);
		is_periodic_cal = FIELD_GET(WMI_CTRL_PATH_IS_PERIODIC_CAL,
					    cal_stats->cal_info);


		if (!is_periodic_cal) {
			len +=
		scnprintf(buf + len, size - len,
			  "%-25s %-25s %-17d %-16d %-16d %-16d\n",
			  wmi_ctrl_path_cal_prof_id_to_name(cal_prof_mask),
			  wmi_ctrl_path_cal_type_id_to_name(cal_type_mask),
			  cal_stats->cal_triggered_cnt,
			  cal_stats->cal_fail_cnt,
			  cal_stats->cal_fcs_cnt,
			  cal_stats->cal_fcs_fail_cnt);
		} else {
			len +=
		scnprintf(buf + len, size - len,
			  "%-25s %-25s %-17d %-16d %-16d %-16d\n",
			  "PERIODIC_CAL",
			  wmi_ctrl_path_periodic_cal_type_id_to_name(cal_type_mask),
			  cal_stats->cal_triggered_cnt,
			  cal_stats->cal_fail_cnt,
			  cal_stats->cal_fcs_cnt,
			  cal_stats->cal_fcs_fail_cnt);
		}

	}

	ret_val =  simple_read_from_buffer(ubuf, count, ppos, buf, len);
	ath11k_wmi_crl_path_stats_list_free(&arvif->ar->debug.wmi_list);
	kfree(buf);
	return ret_val;
}

int wmi_ctrl_path_btcoex_stat(struct ath11k_vif *arvif, char __user *ubuf,
			      size_t count, loff_t *ppos)
{
	struct wmi_ctrl_path_stats_list *stats;
	struct wmi_ctrl_path_btcoex_stats *btcoex_stats;
	const int size = 2048;
	int len = 0, ret_val;
	char *buf;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	list_for_each_entry(stats, &arvif->ar->debug.wmi_list, list) {
		if (!stats)
			break;

		btcoex_stats = stats->stats_ptr;

		len += scnprintf(buf + len, size - len,
				 "WMI_CTRL_PATH_BTCOEX_STATS:\n");
		len += scnprintf(buf + len, size - len,
				 "pdev_id = %u\n",
				 btcoex_stats->pdev_id);
		len += scnprintf(buf + len, size - len,
				 "bt_tx_req_cntr = %u\n",
				 btcoex_stats->bt_tx_req_cntr);
		len += scnprintf(buf + len, size - len,
				 "bt_rx_req_cntr = %u\n",
				 btcoex_stats->bt_rx_req_cntr);
		len += scnprintf(buf + len, size - len,
				 "bt_req_nack_cntr = %u\n",
				 btcoex_stats->bt_req_nack_cntr);
		len += scnprintf(buf + len, size - len,
				 "wl_tx_req_nack_schd_bt_reason_cntr = %u\n",
				 btcoex_stats->wl_tx_req_nack_schd_bt_reason_cntr);
		len += scnprintf(buf + len, size - len,
				 "wl_tx_req_nack_current_bt_reason_cntr = %u\n",
				 btcoex_stats->wl_tx_req_nack_current_bt_reason_cntr);
		len += scnprintf(buf + len, size - len,
				 "wl_tx_req_nack_other_wlan_tx_reason_cntr = %u\n",
				 btcoex_stats->wl_tx_req_nack_other_wlan_tx_reason_cntr);
		len += scnprintf(buf + len, size - len,
				 "wl_in_tx_abort_cntr = %u\n",
				 btcoex_stats->wl_in_tx_abort_cntr);
		len += scnprintf(buf + len, size - len,
				 "wl_tx_auto_resp_req_cntr = %u\n",
				 btcoex_stats->wl_tx_auto_resp_req_cntr);
		len += scnprintf(buf + len, size - len,
				 "wl_tx_req_ack_cntr = %u\n",
				 btcoex_stats->wl_tx_req_ack_cntr);
		len += scnprintf(buf + len, size - len,
				 "wl_tx_req_cntr = %u\n",
				 btcoex_stats->wl_tx_req_cntr);
	}

	ret_val =  simple_read_from_buffer(ubuf, count, ppos, buf, len);
	ath11k_wmi_crl_path_stats_list_free(&arvif->ar->debug.wmi_list);
	kfree(buf);
	return ret_val;
}

static ssize_t ath11k_read_wmi_ctrl_path_stats(struct file *file,
		char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	int ret_val = 0;
	u32 tagid = 0;

	tagid = arvif->ar->debug.wmi_ctrl_path_stats_tagid;

	switch (tagid) {
	case WMI_CTRL_PATH_PDEV_STATS:
		ret_val = wmi_ctrl_path_pdev_stat(arvif, ubuf, count, ppos);
		break;
	case WMI_CTRL_PATH_CAL_STATS:
		ret_val = wmi_ctrl_path_cal_stat(arvif, ubuf, count, ppos);
		break;
	case WMI_CTRL_PATH_BTCOEX_STATS:
		ret_val = wmi_ctrl_path_btcoex_stat(arvif, ubuf, count, ppos);
		break;
	/* Add case for newly wmi ctrl path added stats here */
	default :
		/* Unsupported tag */
		break;
	}

	return ret_val;
}

static const struct file_operations ath11k_fops_wmi_ctrl_stats = {
	.write = ath11k_write_wmi_ctrl_path_stats,
	.open = simple_open,
	.read = ath11k_read_wmi_ctrl_path_stats,
};

static ssize_t ath11k_write_mac_filter(struct file *file,
				       const char __user *ubuf,
				       size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	struct ath11k_mac_filter *mac_filter;
	struct ath11k_mac_filter *i;
	struct ath11k *ar = arvif->ar;
	struct ath11k_peer *peer;
	bool found = false;
	u8 buf[64] = {0};
	int ret;
	unsigned int val;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (ret < 0)
		return ret;

	mac_filter = kzalloc(sizeof(*mac_filter), GFP_ATOMIC);
	if (!mac_filter)
		return -ENOMEM;

	buf[ret] = '\0';
	ret = sscanf(buf, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %u",
		     &mac_filter->peer_mac[0], &mac_filter->peer_mac[1],
		     &mac_filter->peer_mac[2], &mac_filter->peer_mac[3],
		     &mac_filter->peer_mac[4], &mac_filter->peer_mac[5],
		     &val);

	mutex_lock(&ar->conf_mutex);

	if (!list_empty(&arvif->mac_filters)) {
		list_for_each_entry(i, &arvif->mac_filters, list) {
			if (ether_addr_equal(i->peer_mac, mac_filter->peer_mac)) {
				found = true;
				break;
			}
		}
	}

	spin_lock_bh(&ar->ab->base_lock);
	peer = ath11k_peer_find_by_addr(ar->ab, mac_filter->peer_mac);
	if (!found && val) {
		list_add(&mac_filter->list, &arvif->mac_filters);
		arvif->mac_filter_count++;
		if (peer)
			peer->peer_logging_enabled = true;
	} else if (found && !val) {
		list_del(&i->list);
		kfree(i);
		arvif->mac_filter_count--;
		if (peer)
			peer->peer_logging_enabled = false;
	}

	spin_unlock_bh(&ar->ab->base_lock);
	mutex_unlock(&ar->conf_mutex);
	return count;
}

static ssize_t ath11k_read_mac_filter(struct file *file, char __user *ubuf,
				      size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	struct ath11k *ar = arvif->ar;
	struct ath11k_mac_filter *i;
	int len = 0, ret;
	char *buf;
	int size;

	size = (arvif->mac_filter_count * 20) + 25;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	mutex_lock(&ar->conf_mutex);

	if (list_empty(&arvif->mac_filters)) {
		len += scnprintf(buf + len, size - len, "List is Empty\n");
		goto exit;
	}

	len += scnprintf(buf + len, size - len, "Mac address entries :\n");
	list_for_each_entry(i, &arvif->mac_filters, list) {
		len += scnprintf(buf + len, size - len, "%pM\n", i->peer_mac);
	}

exit:
	mutex_unlock(&ar->conf_mutex);
	ret = simple_read_from_buffer(ubuf, count, ppos, buf, len);
	kfree(buf);
	return ret;

}

static const struct file_operations fops_mac_filter = {
	.read = ath11k_read_mac_filter,
	.write = ath11k_write_mac_filter,
	.open = simple_open
};

void ath11k_debugfs_dbg_mac_filter(struct ath11k_vif *arvif)
{
	arvif->mac_filter = debugfs_create_file("mac_filter",
						0644,
						arvif->vif->debugfs_dir, arvif,
						&fops_mac_filter);
	INIT_LIST_HEAD(&arvif->mac_filters);
}

void ath11k_debugfs_wmi_ctrl_stats(struct ath11k_vif *arvif)
{
	arvif->wmi_ctrl_stat = debugfs_create_file("wmi_ctrl_stats", 0644,
						   arvif->vif->debugfs_dir,
						   arvif,
						   &ath11k_fops_wmi_ctrl_stats);
	INIT_LIST_HEAD(&arvif->ar->debug.wmi_list);
	init_completion(&arvif->ar->debug.wmi_ctrl_path_stats_rcvd);
}

static ssize_t ath11k_wbm_tx_comp_stats_read(struct file *file,
					       char __user *user_buf,
					       size_t count,
					       loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	struct ath11k *ar = arvif->ar;
	char buf[256] = {0};
	int len = 0;
	char *fields[] = {[HAL_WBM_REL_HTT_TX_COMP_STATUS_OK] = "Acked pkt count",
			  [HAL_WBM_REL_HTT_TX_COMP_STATUS_TTL] = "Status ttl pkt count",
			  [HAL_WBM_REL_HTT_TX_COMP_STATUS_DROP] = "Dropped pkt count",
			  [HAL_WBM_REL_HTT_TX_COMP_STATUS_REINJ] = "Reinj pkt count",
			  [HAL_WBM_REL_HTT_TX_COMP_STATUS_INSPECT] = "Inspect pkt count",
			  [HAL_WBM_REL_HTT_TX_COMP_STATUS_MEC_NOTIFY] = "MEC notify pkt count"};
	int idx;

	mutex_lock(&ar->conf_mutex);

	if (!arvif->is_started) {
		len += scnprintf(buf + len, sizeof(buf) - len, "vif not started\n");
		goto out;
	}

	len += scnprintf(buf + len, sizeof(buf) - len, "WBM tx completion stats of data pkts :\n");
	for(idx = 0; idx <= HAL_WBM_REL_HTT_TX_COMP_STATUS_MEC_NOTIFY; idx++) {
		len += scnprintf(buf + len, sizeof(buf) - len,
				 "%-23s :  %llu\n",
				 fields[idx],
				 arvif->wbm_tx_comp_stats[idx]);
	}

out:
	mutex_unlock(&ar->conf_mutex);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath11k_wbm_tx_comp_stats_write(struct file *file,
						const char __user *user_buf,
						size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	struct ath11k *ar = arvif->ar;
	u8 reset;
	int ret;

	ret = kstrtou8_from_user(user_buf, count, 0, &reset);

	if (ret || reset != 1)
		return -EINVAL;

	mutex_lock(&ar->conf_mutex);
	if (arvif->is_started)
		memset(arvif->wbm_tx_comp_stats, 0, sizeof(arvif->wbm_tx_comp_stats));
	mutex_unlock(&ar->conf_mutex);

	return count;
}

static const struct file_operations fops_wbm_tx_comp_stats = {
	.write = ath11k_wbm_tx_comp_stats_write,
	.read = ath11k_wbm_tx_comp_stats_read,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

void ath11k_debugfs_wbm_tx_comp_stats(struct ath11k_vif *arvif)
{
	arvif->wbm_tx_completion_stats =
		debugfs_create_file("wbm_tx_completion_stats",
				    S_IRUSR | S_IWUSR, arvif->vif->debugfs_dir,
				    arvif, &fops_wbm_tx_comp_stats);
}

static ssize_t ath11k_write_ampdu_aggr_size(struct file *file,
					    const char __user *ubuf,
					    size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	struct ath11k_base *ab = arvif->ar->ab;
	unsigned int tx_aggr_size = 0;
	int ret;
	struct set_custom_aggr_size_params params = {0};

	if (kstrtouint_from_user(ubuf, count, 0, &tx_aggr_size))
		return -EINVAL;

	if (tx_aggr_size > ATH11K_CONFIG_AGGR_MAX_AMPDU_SIZE) {
		ath11k_warn(ab, "Valid AMPDU Aggregation Size is in the range 0-255");
		return -EINVAL;
	}

	params.aggr_type = WMI_VDEV_CUSTOM_AGGR_TYPE_AMPDU;
	params.tx_aggr_size = tx_aggr_size;
	params.rx_aggr_size_disable = true;
	params.vdev_id = arvif->vdev_id;

	ret = ath11k_wmi_send_aggr_size_cmd(arvif->ar, &params);
	if (ret)
		ath11k_warn(ab, "Failed to set ampdu config vdev_id %d"
			   "ret %d \n",params.vdev_id, ret);

	return ret ? ret : count;
}

static const struct file_operations fops_ampdu_aggr_size = {
        .write = ath11k_write_ampdu_aggr_size,
        .open = simple_open,
	.owner = THIS_MODULE,
        .llseek = default_llseek,
};

static ssize_t ath11k_write_amsdu_aggr_size(struct file *file,
					    const char __user *ubuf,
					    size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	struct ath11k_base *ab = arvif->ar->ab;
	unsigned int tx_aggr_size = 0;
	int ret;
	struct set_custom_aggr_size_params params = {0};

	if (kstrtouint_from_user(ubuf, count, 0, &tx_aggr_size))
		return -EINVAL;

	if (tx_aggr_size > ATH11K_CONFIG_AGGR_MAX_AMSDU_SIZE) {
		ath11k_warn(ab, "Valid AMSDU Aggregation size is in the range 0-7");
		return -EINVAL;
	}

	params.aggr_type = WMI_VDEV_CUSTOM_AGGR_TYPE_AMSDU;
	params.tx_aggr_size = tx_aggr_size;
	params.rx_aggr_size_disable = true;
	params.vdev_id = arvif->vdev_id;

	ret = ath11k_wmi_send_aggr_size_cmd(arvif->ar, &params);
	if (ret)
		ath11k_warn(ab, "Failed to set amsdu config vdev_id %d"
			   "ret %d \n",params.vdev_id, ret);

	return ret ? ret : count;
}

static const struct file_operations fops_amsdu_aggr_size = {
	.write = ath11k_write_amsdu_aggr_size,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

void ath11k_debug_aggr_size_config_init(struct ath11k_vif *arvif)
{
	arvif->ampdu_aggr_size = debugfs_create_file("ampdu_aggr_size", 0644,
						     arvif->vif->debugfs_dir,
						     arvif,
						     &fops_ampdu_aggr_size);
	arvif->amsdu_aggr_size = debugfs_create_file("amsdu_aggr_size", 0644,
						     arvif->vif->debugfs_dir,
						     arvif,
						     &fops_amsdu_aggr_size);
}

void ath11k_debugfs_op_vif_add(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif)
{
	struct ath11k_vif *arvif = ath11k_vif_to_arvif(vif);
	struct ath11k_base *ab = arvif->ar->ab;
	struct dentry *debugfs_twt;

	if (arvif->vif->type != NL80211_IFTYPE_AP &&
	    !(arvif->vif->type == NL80211_IFTYPE_STATION &&
	      test_bit(WMI_TLV_SERVICE_STA_TWT, ab->wmi_ab.svc_map)))
		return;

	debugfs_twt = debugfs_create_dir("twt",
					 arvif->vif->debugfs_dir);
	debugfs_create_file("add_dialog", 0200, debugfs_twt,
			    arvif, &ath11k_fops_twt_add_dialog);

	debugfs_create_file("del_dialog", 0200, debugfs_twt,
			    arvif, &ath11k_fops_twt_del_dialog);

	debugfs_create_file("pause_dialog", 0200, debugfs_twt,
			    arvif, &ath11k_fops_twt_pause_dialog);

	debugfs_create_file("resume_dialog", 0200, debugfs_twt,
			    arvif, &ath11k_fops_twt_resume_dialog);
}

