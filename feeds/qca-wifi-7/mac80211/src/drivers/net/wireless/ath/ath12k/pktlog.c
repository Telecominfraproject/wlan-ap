// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/vmalloc.h>
#include "core.h"
#include "wmi.h"
#include "debug.h"
#include "dp_mon.h"

/* Convert a kernel virtual address to a kernel logical address */
static void ath12k_pktlog_release(struct ath12k_pktlog *pktlog)
{
	uintptr_t vaddr, vaddr_start, vaddr_end;
	unsigned long page_cnt;
	struct page *page;

	vaddr_start = (uintptr_t)pktlog->buf;
	page_cnt = DIV_ROUND_UP(sizeof(*pktlog->buf) + pktlog->buf_size,
				PAGE_SIZE);
	vaddr_end = vaddr_start + (page_cnt * PAGE_SIZE);

	for (vaddr = vaddr_start; vaddr < vaddr_end; vaddr += PAGE_SIZE) {
		page = vmalloc_to_page((const void *)vaddr);
		if (page)
			clear_bit(PG_reserved, &page->flags);
	}

	vfree(pktlog->buf);
	pktlog->buf = NULL;
}

static int ath12k_alloc_pktlog_buf(struct ath12k *ar)
{
	u32 page_cnt;
	uintptr_t vaddr, vaddr_start, vaddr_end;
	struct page *page;
	void *buf;
	struct ath12k_pktlog *pktlog = &ar->debug.pktlog;

	if (pktlog->buf_size == 0)
		return -EINVAL;

	page_cnt = DIV_ROUND_UP(sizeof(*pktlog->buf) + pktlog->buf_size,
				PAGE_SIZE);
	buf =  vmalloc((page_cnt + PKTLOG_EXTRA_PAGES) * PAGE_SIZE);
	if (!buf)
		return -ENOMEM;

	pktlog->buf = (struct ath12k_pktlog_buf *)ALIGN((uintptr_t)(buf),
							PAGE_SIZE);
	vaddr_start = (uintptr_t)pktlog->buf;
	vaddr_end = vaddr_start + (page_cnt * PAGE_SIZE);

	for (vaddr = vaddr_start; vaddr < vaddr_end; vaddr += PAGE_SIZE) {
		page = vmalloc_to_page((const void *)vaddr);
		if (page)
			set_bit(PG_reserved, &page->flags);
	}

	return 0;
}

static void ath12k_init_pktlog_buf(struct ath12k *ar, struct ath12k_pktlog
                                   *pktlog)
{
	if (!ar->ab->pktlog_defs_checksum) {
		pktlog->buf->bufhdr.magic_num = PKTLOG_MAGIC_NUM;
		pktlog->buf->bufhdr.version = CUR_PKTLOG_VER;
	} else {
		pktlog->buf->bufhdr.magic_num = PKTLOG_NEW_MAGIC_NUM;
		pktlog->buf->bufhdr.version = ar->ab->pktlog_defs_checksum;
	}

	if (!test_bit(WMI_TLV_SERVICE_PKTLOG_DECODE_INFO_SUPPORT, ar->ab->wmi_ab.svc_map)) {
                ath12k_warn(ar->ab, "firmware doesn't support pktlog decode info support\n");
                pktlog->invalid_decode_info = 1;
        }

	pktlog->buf->rd_offset = -1;
	pktlog->buf->wr_offset = 0;
}

static inline void ath12k_pktlog_mov_rd_idx(struct ath12k_pktlog *pl_info,
                                            int32_t *rd_offset)
{
	int32_t boundary;
	struct ath12k_pktlog_buf *log_buf = pl_info->buf;
	struct ath12k_pktlog_hdr *log_hdr;

	log_hdr = (struct ath12k_pktlog_hdr *)(log_buf->log_data + *rd_offset);
	boundary = *rd_offset;
	boundary += pl_info->hdr_size;
	boundary += log_hdr->size;

	if (boundary <= pl_info->buf_size)
		*rd_offset = boundary;
	else
		*rd_offset = log_hdr->size;

	if ((pl_info->buf_size - *rd_offset) < pl_info->hdr_size)
		*rd_offset = 0;
}

static char *ath12k_pktlog_getbuf(struct ath12k_pktlog *pl_info,
                                  struct ath12k_pktlog_hdr_arg *hdr_arg)
{
	struct ath12k_pktlog_buf *log_buf;
	int32_t cur_wr_offset, buf_size;
	char *log_ptr;

	spin_lock_bh(&pl_info->lock);
	log_buf = pl_info->buf;
	buf_size = pl_info->buf_size;

	cur_wr_offset = log_buf->wr_offset;
	/* Move read offset to the next entry if there is a buffer overlap */
	if (log_buf->rd_offset >= 0) {
		if ((cur_wr_offset <= log_buf->rd_offset) &&
		    (cur_wr_offset + pl_info->hdr_size) >
		     log_buf->rd_offset)
			ath12k_pktlog_mov_rd_idx(pl_info, &log_buf->rd_offset);
	} else {
		log_buf->rd_offset = cur_wr_offset;
	}

	memcpy(&log_buf->log_data[cur_wr_offset],
	       hdr_arg->pktlog_hdr, pl_info->hdr_size);

	cur_wr_offset += pl_info->hdr_size;

	if ((buf_size - cur_wr_offset) < hdr_arg->payload_size) {
		while ((cur_wr_offset <= log_buf->rd_offset) &&
		       (log_buf->rd_offset < buf_size))
			  ath12k_pktlog_mov_rd_idx(pl_info, &log_buf->rd_offset);
		cur_wr_offset = 0;
	}

	while ((cur_wr_offset <= log_buf->rd_offset) &&
	       ((cur_wr_offset + hdr_arg->payload_size) > log_buf->rd_offset))
			  ath12k_pktlog_mov_rd_idx(pl_info, &log_buf->rd_offset);

	log_ptr = &log_buf->log_data[cur_wr_offset];
	cur_wr_offset += hdr_arg->payload_size;

	log_buf->wr_offset =
		((buf_size - cur_wr_offset) >=
		 pl_info->hdr_size) ? cur_wr_offset : 0;
	spin_unlock_bh(&pl_info->lock);

	return log_ptr;
}

static  vm_fault_t pktlog_pgfault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	unsigned long address = vmf->address;

	if (address == 0UL)
		return VM_FAULT_NOPAGE;

	if (vmf->pgoff > ((vma->vm_end - vma->vm_start) >> PAGE_SHIFT))
		return VM_FAULT_SIGBUS;

	get_page(virt_to_page((void *)address));
	vmf->page = virt_to_page((void *)address);

	return 0;
}

static const struct vm_operations_struct pktlog_vmops = {
	.fault = pktlog_pgfault
};

static int ath12k_pktlog_mmap(struct file *file, struct vm_area_struct
                              *vma)
{
	struct ath12k *ar = file->private_data;

	/* entire buffer should be mapped */
	if (vma->vm_pgoff != 0)
		return -EINVAL;

	if (!ar->debug.pktlog.buf) {
		ath12k_err(ar->ab, "Can't allocate pktlog buf\n");
		return -ENOMEM;
	}
#if LINUX_VERSION_IS_LESS(6, 6, 3)
	vma->vm_flags |= VM_LOCKED;
#else
	vm_flags_set(vma, VM_LOCKED);
#endif
	vma->vm_ops = &pktlog_vmops;

	return 0;
}

static ssize_t ath12k_pktlog_read(struct file *file, char __user *userbuf,
                                  size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_pktlog *info = &ar->debug.pktlog;
	struct ath12k_pktlog_buf *log_buf;
	size_t bufhdr_size, rem_len, nbytes = 0, ret_val = 0;
	size_t start_offset, end_offset = 0;
	size_t fold_offset = INVALID_OFFSET, ppos_data;
	int cur_rd_offset;
	char *buf;
	ssize_t final_ret = 0;

	if (count == 0 || !userbuf)
		return 0;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	if (!ath12k_ftm_mode && ar->ah->state != ATH12K_HW_STATE_ON) {
		final_ret = -ENETDOWN;
		goto unlock;
	}

	buf = vmalloc(count);
	if (!buf) {
		final_ret = -ENOMEM;
		goto unlock;
	}

	spin_lock_bh(&info->lock);
	log_buf = info->buf;
	if (!log_buf) {
		spin_unlock_bh(&info->lock);
		final_ret = 0;
		goto free;
	}

	bufhdr_size = sizeof(log_buf->bufhdr);
	if (!info->fw_version_record || info->invalid_decode_info)
		bufhdr_size -= sizeof(struct ath12k_pktlog_decode_info);

	/* copy valid log entries from circular buffer into user space */
	rem_len = count;

	if (*ppos < bufhdr_size) {
		nbytes = min_t(size_t, bufhdr_size - (size_t)*ppos, rem_len);
		if (*ppos + nbytes > sizeof(log_buf->bufhdr)) {
			final_ret = -EFAULT;
			spin_unlock_bh(&info->lock);
			goto free;
		}

		memcpy(buf, ((char *)&log_buf->bufhdr) + *ppos, nbytes);
		rem_len -= nbytes;
		ret_val += nbytes;
	}

	start_offset = log_buf->rd_offset;
	if (rem_len == 0 || start_offset == INVALID_OFFSET) {
		spin_unlock_bh(&info->lock);
		goto copy_to_user;
	}

	cur_rd_offset = start_offset;

	/* Find the last offset and fold-offset if the buffer is folded */
	do {
		int log_data_offset;
		struct ath12k_pktlog_hdr *log_hdr;

		log_hdr = (struct ath12k_pktlog_hdr *)(log_buf->log_data +
						       cur_rd_offset);
		log_data_offset = cur_rd_offset + info->hdr_size;

		if (fold_offset == INVALID_OFFSET &&
		    ((info->buf_size - log_data_offset) <= log_hdr->size))
			fold_offset = log_data_offset - 1;

		ath12k_pktlog_mov_rd_idx(info, &cur_rd_offset);

		if (fold_offset == INVALID_OFFSET &&
		    cur_rd_offset == 0 &&
		    cur_rd_offset != log_buf->wr_offset)
			fold_offset = log_data_offset + log_hdr->size - 1;

		end_offset = log_data_offset + log_hdr->size - 1;

	} while (cur_rd_offset != log_buf->wr_offset);

	spin_unlock_bh(&info->lock);

	ppos_data = *ppos + ret_val - bufhdr_size + start_offset;

	if (fold_offset == INVALID_OFFSET) {
		if (ppos_data > end_offset)
			goto copy_to_user;

		nbytes = min(rem_len, end_offset - ppos_data + 1);
		if (ppos_data < 0 || ppos_data + nbytes > info->buf_size) {
			final_ret = -EFAULT;
			goto free;
		}

		memcpy(buf + ret_val, log_buf->log_data + ppos_data, nbytes);
		ret_val += nbytes;
	} else {
		if (ppos_data <= fold_offset) {
			nbytes = min(rem_len, fold_offset - ppos_data + 1);
			if (ppos_data < 0 || ppos_data + nbytes >
			    info->buf_size) {
				final_ret = -EFAULT;
				goto free;
			}

			memcpy(buf + ret_val, log_buf->log_data + ppos_data,
			       nbytes);
			ret_val += nbytes;
			rem_len -= nbytes;
		}

		if (rem_len > 0) {
			ppos_data =
				*ppos + ret_val - (bufhdr_size +
						   (fold_offset - start_offset + 1));

			if (ppos_data <= end_offset) {
				nbytes = min(rem_len, end_offset - ppos_data + 1);
				if (ppos_data < 0 || ppos_data + nbytes >
				    info->buf_size) {
					final_ret = -EFAULT;
					goto free;
				}

				memcpy(buf + ret_val, log_buf->log_data + ppos_data,
				       nbytes);
				ret_val += nbytes;
			}
		}
	}

	if (ret_val == 0) {
		final_ret = 0;
		goto free;
	}

copy_to_user:
	if (ret_val < 0 || ret_val > count || *ppos + ret_val < *ppos) {
		final_ret = -EFAULT;
		goto free;
	}

	if (copy_to_user(userbuf, buf, ret_val)) {
		final_ret = -EFAULT;
		goto free;
	}

	*ppos += ret_val;
	final_ret = ret_val;

free:
	vfree(buf);
unlock:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return final_ret;
}

static const struct file_operations fops_pktlog_dump = {
	.read = ath12k_pktlog_read,
	.mmap = ath12k_pktlog_mmap,
	.open = simple_open
};

/**
 * ath12k_write_pktlog_start() : This function is responsible for starting and
 * stopping pktlog data collection.
 *
 * Below command invokes this function:
 * 1. To start the dump collection:
 *   echo 1 > /sys/kernel/debug/ath12k/<HW>/mac0/pktlog/start
 *
 * 2. To stop the dump collection:
 *   echo 0 > /sys/kernel/debug/ath12k/<HW>/mac0/pktlog/start
 *
 */
static ssize_t ath12k_write_pktlog_start(struct file *file, const char __user *ubuf,
                                         size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_pktlog *pktlog = &ar->debug.pktlog;
	u32 start_pktlog;
	int err;

	err = kstrtou32_from_user(ubuf, count, 0, &start_pktlog);
	if (err)
		return err;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	if (!ath12k_ftm_mode && ar->ah->state != ATH12K_HW_STATE_ON) {
		err = -ENETDOWN;
		goto exit;
	}

	if (ar->debug.is_pkt_logging && start_pktlog) {
		ath12k_err(ar->ab, "packet logging is inprogress\n");
		err = -EINVAL;
		goto exit;
	}

	if (start_pktlog) {
		if (pktlog->buf)
			ath12k_pktlog_release(pktlog);

		err = ath12k_alloc_pktlog_buf(ar);
		if (err)
			goto exit;

		err = ath12k_wmi_pdev_pktlog_enable(ar,
						    ar->debug.pktlog_filter);
		if (err) {
			ath12k_err(ar->ab,
				   "failed to enable pktlog filter 0%x: %d\n",
				   ar->debug.pktlog_filter, err);
			ath12k_pktlog_release(pktlog);
			goto exit;
		}
		ath12k_init_pktlog_buf(ar, pktlog);
		ar->debug.is_pkt_logging = true;
	} else {
		ar->debug.is_pkt_logging = false;
		ath12k_dp_mon_pktlog_config(ar, false,
					    ar->debug.pktlog_mode,
					    ar->debug.pktlog_filter);
		err = ath12k_dp_mon_rx_update_filter(ar);
		if (err)
			ath12k_err(ar->ab,
				   "Failed to configure pktlog filters\n");

		err = ath12k_wmi_pdev_pktlog_disable(ar);
		if (err) {
			ath12k_err(ar->ab,
				   "failed to disable pktlog : %d\n", err);
			goto exit;
		}
	}

	err = count;

exit:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return err;
}

static ssize_t ath12k_read_pktlog_start(struct file *file, char __user *ubuf,
                                        size_t count, loff_t *ppos)
{
	char buf[32];
	struct ath12k *ar = file->private_data;
	int len = 0;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	len = scnprintf(buf, sizeof(buf), "%d\n",
					ar->debug.is_pkt_logging);
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_pktlog_start = {
	.read = ath12k_read_pktlog_start,
	.write = ath12k_write_pktlog_start,
	.open = simple_open
};

static ssize_t ath12k_pktlog_size_write(struct file *file, const char __user *ubuf,
                                        size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	u32 pktlog_size;
	int ret;

	if (kstrtou32_from_user(ubuf, count, 0, &pktlog_size))
		return -EINVAL;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	/* Validate size is within reasonable limits */
	if (pktlog_size < ATH12K_PKTLOG_SIZE_MIN ||
	    pktlog_size > ATH12K_PKTLOG_SIZE_MAX) {
		ath12k_err(ar->ab,
			   "Invalid size. Must be between 16KB and 50MB\n");
		ret = -EINVAL;
		goto exit;
	}

	if (pktlog_size == ar->debug.pktlog.buf_size) {
		ret = count;
		goto exit;
	}

	if (ar->debug.is_pkt_logging) {
		ath12k_err(ar->ab,
			   "Stop packet logging before changing the size\n");
		ret = -EINVAL;
		goto exit;
	}

	ar->debug.pktlog.buf_size = pktlog_size;
	ret = count;

exit:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return ret;
}

static ssize_t ath12k_pktlog_size_read(struct file *file, char __user *ubuf,
                                       size_t count, loff_t *ppos)
{
	char buf[32];
	struct ath12k *ar = file->private_data;
	int len = 0;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	len = scnprintf(buf, sizeof(buf), "%uL\n",
			ar->debug.pktlog.buf_size);
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_pktlog_size = {
	.read = ath12k_pktlog_size_read,
	.write = ath12k_pktlog_size_write,
	.open = simple_open
};

static void ath12k_pktlog_init(struct ath12k *ar)
{
	struct ath12k_pktlog *pktlog = &ar->debug.pktlog;

	spin_lock_init(&pktlog->lock);
	pktlog->buf_size = ATH12K_DEBUGFS_PKTLOG_SIZE_DEFAULT;
	pktlog->buf = NULL;

	pktlog->hdr_size = sizeof(struct ath12k_pktlog_hdr);
	pktlog->hdr_size_field_offset =
		   offsetof(struct ath12k_pktlog_hdr, size);
}

void ath12k_init_pktlog(struct ath12k *ar)
{
	ar->debug.debugfs_pktlog = debugfs_create_dir("pktlog",
						      ar->debug.debugfs_pdev);
	debugfs_create_file("start", S_IRUGO | S_IWUSR,
			    ar->debug.debugfs_pktlog, ar, &fops_pktlog_start);
	debugfs_create_file("size", S_IRUGO | S_IWUSR,
			    ar->debug.debugfs_pktlog, ar, &fops_pktlog_size);
	debugfs_create_file("dump", S_IRUGO,
			    ar->debug.debugfs_pktlog, ar, &fops_pktlog_dump);

	ath12k_pktlog_init(ar);
}

void ath12k_deinit_pktlog(struct ath12k *ar)
{
	struct ath12k_pktlog *pktlog = &ar->debug.pktlog;

	if (pktlog->buf)
		ath12k_pktlog_release(pktlog);
}

static void ath12k_pktlog_pull_hdr(struct ath12k_pktlog_hdr_arg *arg,
				   struct ath12k *ar, u8 *data)
{
	struct ath12k_pktlog_hdr *hdr;

	if (!arg || !data) {
		ath12k_warn(ar->ab, "Invalid arguments to pktlog_pull_hdr\n");
		return;
	}

	hdr = (struct ath12k_pktlog_hdr *)data;

	hdr->flags = __le16_to_cpu(hdr->flags);
	hdr->missed_cnt = __le16_to_cpu(hdr->missed_cnt);
	hdr->log_type = __le16_to_cpu(hdr->log_type);
	hdr->size = __le16_to_cpu(hdr->size);
	hdr->timestamp = __le32_to_cpu(hdr->timestamp);
	hdr->type_specific_data = __le32_to_cpu(hdr->type_specific_data);

	arg->log_type = hdr->log_type;
	arg->payload = hdr->payload;
	arg->payload_size = hdr->size;
	arg->pktlog_hdr = data;
}

static void ath12k_pktlog_write_buf(struct ath12k *ar,
				    struct ath12k_pktlog *pl_info,
				    struct ath12k_pktlog_hdr_arg *hdr_arg)
{
	char *log_data;

	if (!pl_info || !pl_info->buf || pl_info->buf_size <= 0) {
		ath12k_warn(ar->ab, "Invalid pl_info or buffer\n");
		return;
	}

	if (!hdr_arg || !hdr_arg->payload || hdr_arg->payload_size <= 0) {
		ath12k_warn(ar->ab, "Invalid hdr_arg or payload\n");
		return;
	}

	log_data = ath12k_pktlog_getbuf(pl_info, hdr_arg);
	if (!log_data) {
		ath12k_warn(ar->ab, "pktlog data is NULL\n");
		return;
	}

	if (hdr_arg->payload_size > pl_info->buf_size) {
		ath12k_warn(ar->ab,
			    "Payload size is too large : %d > buf_size: %d\n",
			    hdr_arg->payload_size, pl_info->buf_size);
		return;
	}

	if (log_data < pl_info->buf->log_data ||
	    (log_data + hdr_arg->payload_size) >
	    (pl_info->buf->log_data + pl_info->buf_size)) {
		ath12k_warn(ar->ab,
			    "memcpy out of bounds : log_data: %p size: %d\n",
			    log_data, hdr_arg->payload_size);
		return;
	}

	memcpy(log_data, hdr_arg->payload, hdr_arg->payload_size);
}

void ath12k_htt_pktlog_process(struct ath12k *ar, u8 *data)
{
	struct ath12k_pktlog *pl_info;
	struct ath12k_pktlog_hdr_arg hdr_arg;

	if (!ar)
		return;

	pl_info = &ar->debug.pktlog;
	ath12k_pktlog_pull_hdr(&hdr_arg, ar, data);
	ath12k_pktlog_write_buf(ar, pl_info, &hdr_arg);
}

void ath12k_htt_ppdu_pktlog_process(struct ath12k *ar, u8 *data,
                                    u32 len)
{
	struct ath12k_pktlog *pl_info;
	struct ath12k_pktlog_hdr hdr;
	struct ath12k_pktlog_hdr_arg hdr_arg;

	if (!ar)
		return;

	pl_info = &ar->debug.pktlog;
	hdr.flags = (1 << PKTLOG_FLG_FRM_TYPE_REMOTE_S);
	hdr.missed_cnt = 0;
	hdr.log_type = ATH12K_PKTLOG_TYPE_PPDU_STATS;
	hdr.timestamp = 0;
	hdr.size = len;
	hdr.type_specific_data = 0;

	hdr_arg.log_type = hdr.log_type;
	hdr_arg.payload_size = hdr.size;
	hdr_arg.payload = (u8 *)data;
	hdr_arg.pktlog_hdr = (u8 *)&hdr;

	ath12k_pktlog_write_buf(ar, pl_info, &hdr_arg);
}

void ath12k_dp_rx_stats_buf_pktlog_process(struct ath12k *ar, u8 *data,
                                          u16 log_type, u32 len)
{
	struct ath12k_pktlog *pl_info;
	struct ath12k_pktlog_hdr hdr;
	struct ath12k_pktlog_hdr_arg hdr_arg;

	if (!ar)
		return;

	pl_info = &ar->debug.pktlog;

	hdr.flags = (1 << PKTLOG_FLG_FRM_TYPE_REMOTE_S);
	hdr.missed_cnt = 0;
	hdr.log_type = log_type;
	hdr.timestamp = 0;
	hdr.size = len;
	hdr.type_specific_data = 0;

	hdr_arg.log_type = log_type;
	hdr_arg.payload_size = len;
	hdr_arg.payload = data;
	hdr_arg.pktlog_hdr = (u8 *)&hdr;

	ath12k_pktlog_write_buf(ar, pl_info, &hdr_arg);
}
EXPORT_SYMBOL(ath12k_dp_rx_stats_buf_pktlog_process);
