// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 */

#ifdef CPTCFG_ATH11K_NSS_SUPPORT

#include <net/mac80211.h>
#include "dp_rx.h"
#include "nss.h"
#include "debug.h"
#include "debug_nss.h"

extern struct dentry *debugfs_debug_infra;

static unsigned int
debug_nss_fill_mpp_dump(struct ath11k_vif *arvif, char *buf, ssize_t size)
{
	struct arvif_nss *nss = &arvif->nss;
	struct ath11k *ar = arvif->ar;
	struct ath11k_nss_mpp_entry *entry, *tmp;
	LIST_HEAD(local_entry);
	unsigned int len = 0;
	int i;

	len += scnprintf(buf + len, size - len, "\nProxy path table\n");
	len += scnprintf(buf + len, size - len, "dest_mac_addr\t\tmesh_dest_mac\t\tflags\n");
	spin_lock_bh(&ar->nss.dump_lock);
	list_splice_tail_init(&nss->mpp_dump, &local_entry);
	spin_unlock_bh(&ar->nss.dump_lock);

	list_for_each_entry_safe(entry, tmp, &local_entry, list) {
		for (i = 0; i < entry->num_entries; i++)
			len += scnprintf(buf + len, size - len, "%pM\t%pM\t0x%x\n",
					 entry->mpp[i].dest_mac_addr,
					 entry->mpp[i].mesh_dest_mac, entry->mpp[i].flags);
		list_del(&entry->list);
		kfree(entry);
	}

	return len;
}

static int ath11k_nss_dump_mpp_open(struct inode *inode, struct file *file)
{
	struct ath11k_vif *arvif = inode->i_private;
	struct ath11k *ar = arvif->ar;
	unsigned long time_left;
	struct ath11k_nss_dbg_priv_data *priv_data;
	int ret;
	ssize_t size = 100;
	char *buf;

	mutex_lock(&ar->conf_mutex);

	reinit_completion(&arvif->nss.dump_mpp_complete);

	priv_data = kzalloc(sizeof(*priv_data), GFP_KERNEL);
	if (!priv_data) {
		mutex_unlock(&ar->conf_mutex);
		return -ENOMEM;
	}

	priv_data->arvif = arvif;
	ret = ath11k_nss_dump_mpp_request(arvif);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send dump mpp command %d\n", ret);
		goto err_unlock;
	}

	time_left = wait_for_completion_timeout(&arvif->nss.dump_mpp_complete,
						ATH11K_NSS_MPATH_DUMP_TIMEOUT);
	if (time_left == 0) {
		ret = -ETIMEDOUT;
		goto err_unlock;
	}

	mutex_unlock(&ar->conf_mutex);

	size += (arvif->nss.mpp_dump_num_entries * 200 + 10 * 100);
	buf = kmalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	priv_data->buf = buf;

	priv_data->len= debug_nss_fill_mpp_dump(arvif, buf, size);

	file->private_data = priv_data;

	return 0;

err_unlock:
	kfree(priv_data);
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static int ath11k_nss_dump_mpp_release(struct inode *inode, struct file *file)
{
	struct ath11k_nss_dbg_priv_data *priv_data = file->private_data;

	kfree(priv_data->buf);
	kfree(priv_data);
	return 0;
}

static ssize_t ath11k_nss_dump_mpp_read(struct file *file,
					char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct ath11k_nss_dbg_priv_data *priv_data = file->private_data;
	struct ath11k_vif *arvif = priv_data->arvif;
	char *buf = priv_data->buf;
	struct ath11k *ar = arvif->ar;
	int ret;

	mutex_lock(&ar->conf_mutex);

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, priv_data->len);

	mutex_unlock(&ar->conf_mutex);

	return ret;
}

static const struct file_operations fops_nss_dump_mpp_table = {
	.open = ath11k_nss_dump_mpp_open,
	.read = ath11k_nss_dump_mpp_read,
	.release = ath11k_nss_dump_mpp_release,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static unsigned int
debug_nss_fill_mpath_dump(struct ath11k_vif *arvif, char *buf, ssize_t size)
{
	struct arvif_nss *nss = &arvif->nss;
	struct ath11k *ar = arvif->ar;
	struct ath11k_nss_mpath_entry *entry, *tmp;
	LIST_HEAD(local_entry);
	unsigned int len = 0;
	u64 expiry_time;
	int i;

	len += scnprintf(buf + len, size - len, "\nmpath table\n");
	len += scnprintf(buf + len, size - len, "dest_mac_addr\t\tnext_hop_mac\t\tmetric\t"
			"expiry_time\thop_count\tflags\tlink_vap_id\n");

	spin_lock_bh(&ar->nss.dump_lock);
	list_splice_tail_init(&nss->mpath_dump, &local_entry);
	spin_unlock_bh(&ar->nss.dump_lock);

	list_for_each_entry_safe(entry, tmp, &local_entry, list) {
		for (i = 0; i < entry->num_entries; i++) {
			memcpy(&expiry_time, entry->mpath[i].expiry_time, sizeof(u64));
			len += scnprintf(buf + len, size - len, "%pM\t%pM\t%u\t%llu\t\t\t%d\t0x%x\t%d\n",
					 entry->mpath[i].dest_mac_addr,
					 entry->mpath[i].next_hop_mac_addr, entry->mpath[i].metric,
					 expiry_time, entry->mpath[i].hop_count,
					 entry->mpath[i].flags, entry->mpath[i].link_vap_id);
		}
		kfree(entry);
	}

	return len;
}

static int ath11k_nss_dump_mpath_open(struct inode *inode, struct file *file)
{
	struct ath11k_vif *arvif = inode->i_private;
	struct ath11k *ar = arvif->ar;
	unsigned long time_left;
	struct ath11k_nss_dbg_priv_data *priv_data;
	ssize_t size = 200;
	char *buf;
	int ret;

	reinit_completion(&arvif->nss.dump_mpath_complete);

	priv_data = kzalloc(sizeof(*priv_data), GFP_KERNEL);
	if (!priv_data)
		return -ENOMEM;

	mutex_lock(&ar->conf_mutex);

	priv_data->arvif = arvif;
	ret = ath11k_nss_dump_mpath_request(arvif);
	if (ret) {
		ath11k_warn(ar->ab, "failed to send dump mpath command %d\n", ret);
		goto err_unlock;
	}

	time_left = wait_for_completion_timeout(&arvif->nss.dump_mpath_complete,
						ATH11K_NSS_MPATH_DUMP_TIMEOUT);
	if (time_left == 0) {
		ret = -ETIMEDOUT;
		goto err_unlock;
	}

	mutex_unlock(&ar->conf_mutex);

	size += (arvif->nss.mpath_dump_num_entries * 200 + 10 * 100);
	buf = kmalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	priv_data->buf = buf;

	priv_data->len = debug_nss_fill_mpath_dump(arvif, buf, size);

	file->private_data = priv_data;

	return 0;

err_unlock:
	mutex_unlock(&ar->conf_mutex);
	return ret;
}

static int ath11k_nss_dump_mpath_release(struct inode *inode, struct file *file)
{
	struct ath11k_nss_dbg_priv_data *priv_data = file->private_data;

	kfree(priv_data->buf);
	kfree(priv_data);
	return 0;

}

static ssize_t ath11k_nss_dump_mpath_read(struct file *file,
					  char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ath11k_nss_dbg_priv_data *priv_data = file->private_data;
	struct ath11k_vif *arvif = priv_data->arvif;
	char *buf = priv_data->buf;
	struct ath11k *ar = arvif->ar;
	int ret;

	mutex_lock(&ar->conf_mutex);

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, priv_data->len);

	mutex_unlock(&ar->conf_mutex);

	return ret;
}

static const struct file_operations fops_nss_dump_mpath_table = {
	.open = ath11k_nss_dump_mpath_open,
	.read = ath11k_nss_dump_mpath_read,
	.release = ath11k_nss_dump_mpath_release,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_nss_mpath_add(struct file *file,
				     char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	struct ieee80211_mesh_path_offld path = {0};
	u8 buf[128] = {0};
	int ret;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%u %hhu %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %hhu %hhu",
			&path.metric,
			&path.hop_count,
			&path.mesh_da[0],
			&path.mesh_da[1],
			&path.mesh_da[2],
			&path.mesh_da[3],
			&path.mesh_da[4],
			&path.mesh_da[5],
			&path.next_hop[0],
			&path.next_hop[1],
			&path.next_hop[2],
			&path.next_hop[3],
			&path.next_hop[4],
			&path.next_hop[5],
			&path.block_mesh_fwd,
			&path.metadata_type);


	path.flags |= IEEE80211_MESH_PATH_ACTIVE | IEEE80211_MESH_PATH_RESOLVED;

	if (ret != 16)
		return -EINVAL;

	/* Configure the mpath */
	ret = ath11k_nss_mesh_config_path(arvif->ar, arvif,
			IEEE80211_MESH_PATH_OFFLD_CMD_ADD_MPATH,
			&path);
	if(ret) {
		ath11k_warn(arvif->ar->ab, "failed to configure mpath ret %d\n", ret);
		return -EINVAL;
	}

	return ret ? ret : count;

}

static const struct file_operations fops_nss_mpath_add = {
	.open = simple_open,
	.write = ath11k_nss_mpath_add,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_nss_mpp_add(struct file *file,
				   char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	struct ieee80211_mesh_path_offld path = {0};
	u8 buf[128] = {0};
	int ret;

	if (!arvif->ar->ab->nss.debug_mode) {
		ret = -EPERM;
		return ret;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
			&path.da[0],
			&path.da[1],
			&path.da[2],
			&path.da[3],
			&path.da[4],
			&path.da[5],
			&path.mesh_da[0],
			&path.mesh_da[1],
			&path.mesh_da[2],
			&path.mesh_da[3],
			&path.mesh_da[4],
			&path.mesh_da[5]);

	path.flags |= IEEE80211_MESH_PATH_ACTIVE | IEEE80211_MESH_PATH_RESOLVED;

	if (ret != 12)
		return -EINVAL;

	/* Configure the mpp */
	ret = ath11k_nss_mesh_config_path(arvif->ar, arvif,
			IEEE80211_MESH_PATH_OFFLD_CMD_ADD_MPP,
			&path);
	if(ret) {
		ath11k_warn(arvif->ar->ab, "failed to configure mpp ret %d\n", ret);
		return -EINVAL;
	}

	return ret ? ret : count;

}

static const struct file_operations fops_nss_mpp_add = {
	.open = simple_open,
	.write = ath11k_nss_mpp_add,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_nss_mpath_update(struct file *file,
					char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	struct ieee80211_mesh_path_offld path = {0};
	u8 buf[128] = {0};
	int ret;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%u %hhu %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %hhu %lu %hhu %hhu",
			&path.metric,
			&path.hop_count,
			&path.mesh_da[0],
			&path.mesh_da[1],
			&path.mesh_da[2],
			&path.mesh_da[3],
			&path.mesh_da[4],
			&path.mesh_da[5],
			&path.next_hop[0],
			&path.next_hop[1],
			&path.next_hop[2],
			&path.next_hop[3],
			&path.next_hop[4],
			&path.next_hop[5],
			&path.old_next_hop[0],
			&path.old_next_hop[1],
			&path.old_next_hop[2],
			&path.old_next_hop[3],
			&path.old_next_hop[4],
			&path.old_next_hop[5],
			&path.mesh_gate,
			&path.exp_time,
			&path.block_mesh_fwd,
			&path.metadata_type);


	path.flags |= IEEE80211_MESH_PATH_ACTIVE | IEEE80211_MESH_PATH_RESOLVED;

	if (ret != 24)
		return -EINVAL;

	/* Configure the mpath */
	ret = ath11k_nss_mesh_config_path(arvif->ar, arvif,
			IEEE80211_MESH_PATH_OFFLD_CMD_UPDATE_MPATH,
			&path);
	if(ret) {
		ath11k_warn(arvif->ar->ab, "failed to configure mpath ret %d\n", ret);
		return -EINVAL;
	}

	return ret ? ret : count;

}

static const struct file_operations fops_nss_mpath_update = {
	.open = simple_open,
	.write = ath11k_nss_mpath_update,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_nss_mpp_update(struct file *file,
					char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	struct ieee80211_mesh_path_offld path = {0};
	u8 buf[128] = {0};
	int ret;

	if (!arvif->ar->ab->nss.debug_mode) {
		ret = -EPERM;
		return ret;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
			&path.da[0],
			&path.da[1],
			&path.da[2],
			&path.da[3],
			&path.da[4],
			&path.da[5],
			&path.mesh_da[0],
			&path.mesh_da[1],
			&path.mesh_da[2],
			&path.mesh_da[3],
			&path.mesh_da[4],
			&path.mesh_da[5]);

	path.flags |= IEEE80211_MESH_PATH_ACTIVE | IEEE80211_MESH_PATH_RESOLVED;

	if (ret != 12)
		return -EINVAL;

	/* Configure the mpp */
	ret = ath11k_nss_mesh_config_path(arvif->ar, arvif,
			IEEE80211_MESH_PATH_OFFLD_CMD_UPDATE_MPP,
			&path);
	if(ret) {
		ath11k_warn(arvif->ar->ab, "failed to configure mpp ret %d\n", ret);
		return -EINVAL;
	}

	return ret ? ret : count;

}

static const struct file_operations fops_nss_mpp_update = {
	.open = simple_open,
	.write = ath11k_nss_mpp_update,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};


static ssize_t ath11k_nss_mpath_delete(struct file *file,
					char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	struct ieee80211_mesh_path_offld path = {0};
	u8 buf[128] = {0};
	int ret;

	if (!arvif->ar->ab->nss.debug_mode) {
		ret = -EPERM;
		return ret;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
			&path.mesh_da[0],
			&path.mesh_da[1],
			&path.mesh_da[2],
			&path.mesh_da[3],
			&path.mesh_da[4],
			&path.mesh_da[5],
			&path.next_hop[0],
			&path.next_hop[1],
			&path.next_hop[2],
			&path.next_hop[3],
			&path.next_hop[4],
			&path.next_hop[5]);

	path.flags |= IEEE80211_MESH_PATH_DELETED;

	if (ret != 12)
		return -EINVAL;

	/* Configure the mpath */
	ret = ath11k_nss_mesh_config_path(arvif->ar, arvif,
			IEEE80211_MESH_PATH_OFFLD_CMD_DELETE_MPATH,
			&path);
	if(ret) {
		ath11k_warn(arvif->ar->ab, "failed to configure mpath ret %d\n", ret);
		return -EINVAL;
	}

	return ret ? ret : count;

}

static const struct file_operations fops_nss_mpath_del = {
	.open = simple_open,
	.write = ath11k_nss_mpath_delete,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_nss_mpp_delete(struct file *file,
					char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	struct ieee80211_mesh_path_offld path = {0};
	u8 buf[128] = {0};
	int ret;

	if (!arvif->ar->ab->nss.debug_mode) {
		ret = -EPERM;
		return ret;
	}

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
			&path.da[0],
			&path.da[1],
			&path.da[2],
			&path.da[3],
			&path.da[4],
			&path.da[5],
			&path.mesh_da[0],
			&path.mesh_da[1],
			&path.mesh_da[2],
			&path.mesh_da[3],
			&path.mesh_da[4],
			&path.mesh_da[5]);

	path.flags |= IEEE80211_MESH_PATH_DELETED;

	if (ret != 12)
		return -EINVAL;

	/* Configure the mpp */
	ret = ath11k_nss_mesh_config_path(arvif->ar, arvif,
			IEEE80211_MESH_PATH_OFFLD_CMD_DELETE_MPP,
			&path);
	if(ret) {
		ath11k_warn(arvif->ar->ab, "failed to configure mpp ret %d\n", ret);
		return -EINVAL;
	}

	return ret ? ret : count;

}

static const struct file_operations fops_nss_mpp_del = {
	.open = simple_open,
	.write = ath11k_nss_mpp_delete,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};


static ssize_t ath11k_nss_assoc_link(struct file *file,
				     char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	u8 buf[128] = {0};
	int ret;
	u32 assoc_link = 0;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%u", &assoc_link);

	if (ret != 1)
		return -EINVAL;

	arvif->ar->ab->nss.debug_mode = true;
	arvif->vif->driver_flags |= IEEE80211_VIF_NSS_OFFLOAD_DEBUG_MODE;

	ret = ath11k_nss_assoc_link_arvif_to_ifnum(arvif, assoc_link);

	return ret ? ret : count;

}

static const struct file_operations fops_nss_assoc_link = {
	.open = simple_open,
	.write = ath11k_nss_assoc_link,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_nss_links(struct file *file,
				  char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	char buf[512] = {0};
	struct arvif_nss *nss;
	int len = 0;

	list_for_each_entry(nss, &mesh_vaps, list)
		len += scnprintf(buf + len, sizeof(buf) - len, "link id %d\n",
				nss->if_num);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_nss_links = {
	.open = simple_open,
	.read = ath11k_nss_links,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_nss_vap_link_id(struct file *file,
				  char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	struct arvif_nss *nss = &arvif->nss;
	char buf[512] = {0};
	int len = 0;

	len = scnprintf(buf, sizeof(buf) - len, "link id %d\n",
			nss->if_num);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_nss_vap_link_id = {
	.open = simple_open,
	.read = ath11k_nss_vap_link_id,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_nss_read_mpp_mode(struct file *file,
				  char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	char buf[512] = {0};
	int len = 0;

	len = scnprintf(buf, sizeof(buf) - len, "%s\n",mpp_mode ?
			"Host Assisted Learning" : "NSS Independent Learning");

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath11k_nss_write_mpp_mode(struct file *file,
				  char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	u8 buf[128] = {0};
	int ret;
	u32 mppath_mode = 0;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';
	ret = sscanf(buf, "%u", &mppath_mode);

	if (ret != 1)
		return -EINVAL;

	mpp_mode = mppath_mode;

	ret = 0;

	return ret ? ret : count;
}

static const struct file_operations fops_nss_mpp_mode = {
	.open = simple_open,
	.write = ath11k_nss_write_mpp_mode,
	.read = ath11k_nss_read_mpp_mode,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_nss_write_excep_flags(struct file *file,
				  char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	u8 buf[128] = {0};
	int ret;
	struct nss_wifi_mesh_exception_flag_msg msg;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';

	ret = sscanf(buf, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %hhu",
			&msg.dest_mac_addr[0],
			&msg.dest_mac_addr[1],
			&msg.dest_mac_addr[2],
			&msg.dest_mac_addr[3],
			&msg.dest_mac_addr[4],
			&msg.dest_mac_addr[5],
			&msg.exception);

	if (ret != 7)
		return -EINVAL;

	ret = ath11k_nss_mesh_exception_flags(arvif, &msg);

	return ret ? ret : count;
}

static const struct file_operations fops_nss_excep_flags = {
	.open = simple_open,
	.write = ath11k_nss_write_excep_flags,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_nss_write_metadata_type(struct file *file,
				  char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	struct ath11k *ar = arvif->ar;
	u8 buf[128] = {0};
	int ret;
	u8 pkt_type;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';

	ret = sscanf(buf, "%hhu", &pkt_type);
	mutex_lock(&ar->conf_mutex);
	arvif->nss.metadata_type = pkt_type ? NSS_WIFI_MESH_PRE_HEADER_80211 : NSS_WIFI_MESH_PRE_HEADER_NONE;
	mutex_unlock(&ar->conf_mutex);

	if (ret != 1)
		return -EINVAL;

	ret = 0;

	return ret ? ret : count;
}

static const struct file_operations fops_nss_metadata_type = {
	.open = simple_open,
	.write = ath11k_nss_write_metadata_type,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath11k_nss_write_exc_rate_limit(struct file *file,
				  char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	struct ath11k_vif *arvif = file->private_data;
	u8 buf[128] = {0};
	int ret;
	struct nss_wifi_mesh_rate_limit_config nss_exc_cfg;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret < 0)
		return ret;

	buf[ret] = '\0';

	ret = sscanf(buf, "%u %u %u",
			&nss_exc_cfg.exception_num,
			&nss_exc_cfg.enable,
			&nss_exc_cfg.rate_limit);

	if (ret != 3)
		return -EINVAL;

	ret = ath11k_nss_exc_rate_config(arvif, &nss_exc_cfg);

	return ret ? ret : count;
}

static const struct file_operations fops_nss_exc_rate_limit = {
	.open = simple_open,
	.write = ath11k_nss_write_exc_rate_limit,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

void ath11k_debugfs_nss_mesh_vap_create(struct ath11k_vif *arvif)
{
	struct dentry *debugfs_nss_mesh_dir, *debugfs_dbg_infra;

	debugfs_nss_mesh_dir = debugfs_create_dir("nss_mesh", arvif->vif->debugfs_dir);
	debugfs_dbg_infra = debugfs_create_dir("dbg_infra", debugfs_nss_mesh_dir);

	debugfs_create_file("dump_nss_mpath_table", 0600,
			debugfs_nss_mesh_dir, arvif,
			&fops_nss_dump_mpath_table);

	debugfs_create_file("dump_nss_mpp_table", 0600,
			debugfs_nss_mesh_dir, arvif,
			&fops_nss_dump_mpp_table);

	debugfs_create_file("mpath_add", 0200,
			debugfs_dbg_infra, arvif,
			&fops_nss_mpath_add);

	debugfs_create_file("mpath_update", 0200,
			debugfs_dbg_infra, arvif,
			&fops_nss_mpath_update);

	debugfs_create_file("mpath_del", 0200,
			debugfs_dbg_infra, arvif,
			&fops_nss_mpath_del);

	debugfs_create_file("mpp_add", 0200,
			debugfs_dbg_infra, arvif,
			&fops_nss_mpp_add);

	debugfs_create_file("mpp_update", 0200,
			debugfs_dbg_infra, arvif,
			&fops_nss_mpp_update);

	debugfs_create_file("mpp_del", 0200,
			debugfs_dbg_infra, arvif,
			&fops_nss_mpp_del);

	debugfs_create_file("assoc_link", 0200,
			debugfs_dbg_infra, arvif,
			&fops_nss_assoc_link);

	debugfs_create_file("vap_linkid", 0200,
			debugfs_dbg_infra, arvif,
			&fops_nss_vap_link_id);

	debugfs_create_file("excep_flags", 0200,
			debugfs_dbg_infra, arvif,
			&fops_nss_excep_flags);

	debugfs_create_file("metadata_type", 0200,
			debugfs_dbg_infra, arvif,
			&fops_nss_metadata_type);

	debugfs_create_file("exc_rate_limit", 0200,
			debugfs_dbg_infra, arvif,
			&fops_nss_exc_rate_limit);
}

void ath11k_debugfs_nss_soc_create(struct ath11k_base *ab)
{
	if (debugfs_debug_infra)
		return;

	debugfs_debug_infra = debugfs_create_dir("dbg_infra", debugfs_ath11k);

	debugfs_create_file("links", 0200,
			debugfs_debug_infra, ab,
			&fops_nss_links);

	debugfs_create_file("mpp_mode", 0600,
			debugfs_debug_infra, ab,
			&fops_nss_mpp_mode);
}

#endif
