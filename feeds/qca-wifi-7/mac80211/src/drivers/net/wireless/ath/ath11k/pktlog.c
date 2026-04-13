
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/vmalloc.h>
#include "core.h"
#include "wmi.h"
#include "debug.h"

static struct page *pktlog_virt_to_logical(void *addr)
{
	struct page *page;
	unsigned long vpage = 0UL;

	page = vmalloc_to_page(addr);
	if (page) {
		vpage = (unsigned long)page_address(page);
		vpage |= ((unsigned long)addr & (PAGE_SIZE - 1));
	}
	return virt_to_page((void *)vpage);
}

static void ath_pktlog_release(struct ath_pktlog *pktlog)
{
	unsigned long page_cnt, vaddr;
	struct page *page;

	page_cnt =
		((sizeof(*pktlog->buf) +
		pktlog->buf_size) / PAGE_SIZE) + 1;

	for (vaddr = (unsigned long)(pktlog->buf); vaddr <
			(unsigned long)(pktlog->buf) +
			(page_cnt * PAGE_SIZE);
			vaddr += PAGE_SIZE) {
		page = pktlog_virt_to_logical((void *)vaddr);
		clear_bit(PG_reserved, &page->flags);
	}

	vfree(pktlog->buf);
	pktlog->buf = NULL;
}

static int ath_alloc_pktlog_buf(struct ath11k *ar)
{
	u32 page_cnt;
	unsigned long vaddr;
	struct page *page;
	struct ath_pktlog *pktlog = &ar->debug.pktlog;

	if (pktlog->buf_size == 0)
		return -EINVAL;

	page_cnt = (sizeof(*pktlog->buf) +
		    pktlog->buf_size) / PAGE_SIZE;

	pktlog->buf =  vmalloc((page_cnt + 2) * PAGE_SIZE);
	if (!pktlog->buf)
		return -ENOMEM;

	pktlog->buf = (struct ath_pktlog_buf *)
				     (((unsigned long)
				      (pktlog->buf)
				     + PAGE_SIZE - 1) & PAGE_MASK);

	for (vaddr = (unsigned long)(pktlog->buf);
		      vaddr < ((unsigned long)(pktlog->buf)
		      + (page_cnt * PAGE_SIZE)); vaddr += PAGE_SIZE) {
		page = pktlog_virt_to_logical((void *)vaddr);
		set_bit(PG_reserved, &page->flags);
	}

	return 0;
}

static void ath_init_pktlog_buf(struct ath11k *ar, struct ath_pktlog *pktlog)
{
	if (!ar->ab->pktlog_defs_checksum) {
		pktlog->buf->bufhdr.magic_num = PKTLOG_MAGIC_NUM;
		pktlog->buf->bufhdr.version = CUR_PKTLOG_VER;
	} else {
		pktlog->buf->bufhdr.magic_num = PKTLOG_NEW_MAGIC_NUM;
		pktlog->buf->bufhdr.version = ar->ab->pktlog_defs_checksum;
	}
	pktlog->buf->rd_offset = -1;
	pktlog->buf->wr_offset = 0;
}

static inline void ath_pktlog_mov_rd_idx(struct ath_pktlog *pl_info,
					 int32_t *rd_offset)
{
	int32_t boundary;
	struct ath_pktlog_buf *log_buf = pl_info->buf;
	struct ath_pktlog_hdr *log_hdr;

	log_hdr = (struct ath_pktlog_hdr *)(log_buf->log_data + *rd_offset);
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

static char *ath_pktlog_getbuf(struct ath_pktlog *pl_info,
			       struct ath_pktlog_hdr_arg *hdr_arg)
{
	struct ath_pktlog_buf *log_buf;
	int32_t cur_wr_offset, buf_size;
	char *log_ptr;

	log_buf = pl_info->buf;
	buf_size = pl_info->buf_size;

	spin_lock_bh(&pl_info->lock);
	cur_wr_offset = log_buf->wr_offset;
	/* Move read offset to the next entry if there is a buffer overlap */
	if (log_buf->rd_offset >= 0) {
		if ((cur_wr_offset <= log_buf->rd_offset) &&
		    (cur_wr_offset + pl_info->hdr_size) >
		     log_buf->rd_offset)
			ath_pktlog_mov_rd_idx(pl_info, &log_buf->rd_offset);
	} else {
		log_buf->rd_offset = cur_wr_offset;
	}

	memcpy(&log_buf->log_data[cur_wr_offset],
	       hdr_arg->pktlog_hdr, pl_info->hdr_size);

	cur_wr_offset += pl_info->hdr_size;

	if ((buf_size - cur_wr_offset) < hdr_arg->payload_size) {
		while ((cur_wr_offset <= log_buf->rd_offset) &&
		       (log_buf->rd_offset < buf_size))
			  ath_pktlog_mov_rd_idx(pl_info, &log_buf->rd_offset);
		cur_wr_offset = 0;
	}

	while ((cur_wr_offset <= log_buf->rd_offset) &&
	       ((cur_wr_offset + hdr_arg->payload_size) > log_buf->rd_offset))
			  ath_pktlog_mov_rd_idx(pl_info, &log_buf->rd_offset);

	log_ptr = &log_buf->log_data[cur_wr_offset];
	cur_wr_offset += hdr_arg->payload_size;

	log_buf->wr_offset =
		((buf_size - cur_wr_offset) >=
		 pl_info->hdr_size) ? cur_wr_offset : 0;
	spin_unlock_bh(&pl_info->lock);

	return log_ptr;
}

static vm_fault_t pktlog_pgfault(struct vm_fault *vmf)
{
#if LINUX_VERSION_IS_LESS(5,4,0)
	unsigned long address = (unsigned long)vmf->virtual_address;
#elif LINUX_VERSION_IS_GEQ(5,4,0)
	unsigned long address = vmf->address;
#endif
	struct vm_area_struct *vma = vmf->vma;

	if (address == 0UL)
		return VM_FAULT_NOPAGE;

	if (vmf->pgoff > vma->vm_end)
		return VM_FAULT_SIGBUS;

	get_page(virt_to_page((void *)address));
	vmf->page = virt_to_page((void *)address);
#if LINUX_VERSION_IS_LESS(5,4,0)
	return VM_FAULT_MINOR;
#elif LINUX_VERSION_IS_GEQ(5,4,0)
	return 0;
#endif
}

static struct vm_operations_struct pktlog_vmops = {
	.fault = pktlog_pgfault
};

static int ath_pktlog_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct ath11k *ar = file->private_data;

	/* entire buffer should be mapped */
	if (vma->vm_pgoff != 0)
		return -EINVAL;

	if (!ar->debug.pktlog.buf) {
		pr_err("Can't allocate pktlog buf\n");
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

static ssize_t ath_pktlog_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	size_t bufhdr_size;
	size_t nbytes = 0, ret_val = 0;
	int rem_len;
	int start_offset, end_offset;
	int fold_offset, ppos_data, cur_rd_offset;
	struct ath11k *ar = file->private_data;
	struct ath_pktlog *info = &ar->debug.pktlog;
	struct ath_pktlog_buf *log_buf = info->buf;

	if (log_buf == NULL)
		return 0;

	bufhdr_size = sizeof(log_buf->bufhdr);

	/* copy valid log entries from circular buffer into user space */
	rem_len = count;

	nbytes = 0;

	if (*ppos < bufhdr_size) {
		nbytes = min((int)(bufhdr_size -  *ppos), rem_len);
		if (copy_to_user(userbuf,
				 ((char *)&log_buf->bufhdr) + *ppos, nbytes))
			return -EFAULT;
		rem_len -= nbytes;
		ret_val += nbytes;
	}

	spin_lock_bh(&info->lock);

	start_offset = log_buf->rd_offset;

	if ((rem_len == 0) || (start_offset < 0))
		goto read_done;

	fold_offset = -1;
	cur_rd_offset = start_offset;

	/* Find the last offset and fold-offset if the buffer is folded */
	do {
		int log_data_offset;
		struct ath_pktlog_hdr *log_hdr;

		log_hdr = (struct ath_pktlog_hdr *)(log_buf->log_data + cur_rd_offset);
		log_data_offset = cur_rd_offset + info->hdr_size;

		if ((fold_offset == -1) &&
		    ((info->buf_size - log_data_offset) <= log_hdr->size))
			fold_offset = log_data_offset - 1;

		ath_pktlog_mov_rd_idx(info, &cur_rd_offset);

		if ((fold_offset == -1) && (cur_rd_offset == 0) &&
		    (cur_rd_offset != log_buf->wr_offset))
			fold_offset = log_data_offset + log_hdr->size - 1;

		end_offset = log_data_offset + log_hdr->size - 1;

	} while (cur_rd_offset != log_buf->wr_offset);

	ppos_data = *ppos + ret_val - bufhdr_size + start_offset;

	if (fold_offset == -1) {
		if (ppos_data > end_offset)
			goto read_done;

		nbytes = min(rem_len, end_offset - ppos_data + 1);
		if (copy_to_user(userbuf + ret_val,
				 log_buf->log_data + ppos_data, nbytes)) {
			ret_val = -EFAULT;
			goto out;
		}
		ret_val += nbytes;
		rem_len -= nbytes;
	} else {
		if (ppos_data <= fold_offset) {
			nbytes = min(rem_len, fold_offset - ppos_data + 1);
			if (copy_to_user(userbuf + ret_val,
					 log_buf->log_data + ppos_data,	nbytes)) {
				ret_val = -EFAULT;
				goto out;
			}
			ret_val += nbytes;
			rem_len -= nbytes;
		}

		if (rem_len == 0)
			goto read_done;

		ppos_data =
			*ppos + ret_val - (bufhdr_size +
					(fold_offset - start_offset + 1));

		if (ppos_data <= end_offset) {
			nbytes = min(rem_len, end_offset - ppos_data + 1);
			if (copy_to_user(userbuf + ret_val, log_buf->log_data
					 + ppos_data,
					 nbytes)) {
				ret_val = -EFAULT;
				goto out;
			}
			ret_val += nbytes;
			rem_len -= nbytes;
		}
	}

read_done:
	*ppos += ret_val;
out:
	spin_unlock_bh(&info->lock);
	return ret_val;
}

static const struct file_operations fops_pktlog_dump = {
	.read = ath_pktlog_read,
	.mmap = ath_pktlog_mmap,
	.open = simple_open
};

static ssize_t write_pktlog_start(struct file *file, const char __user *ubuf,
				  size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	struct ath_pktlog *pktlog = &ar->debug.pktlog;
	u32 start_pktlog;
	int err;

	err = kstrtou32_from_user(ubuf, count, 0, &start_pktlog);
	if (err)
		return err;

	if (ar->debug.is_pkt_logging && start_pktlog) {
		pr_err("packet logging is inprogress\n");
		return -EINVAL;
	}
	if (start_pktlog) {
		if (pktlog->buf != NULL)
			ath_pktlog_release(pktlog);

		err = ath_alloc_pktlog_buf(ar);
		if (err != 0)
			return err;

		ath_init_pktlog_buf(ar, pktlog);
		ar->debug.is_pkt_logging = 1;
	} else {
		ar->debug.is_pkt_logging = 0;
	}

	return count;
}

static ssize_t read_pktlog_start(struct file *file, char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	char buf[32];
	struct ath11k *ar = file->private_data;
	int len = 0;

	len = scnprintf(buf, sizeof(buf) - len, "%d\n",
			ar->debug.is_pkt_logging);
	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_pktlog_start = {
	.read = read_pktlog_start,
	.write = write_pktlog_start,
	.open = simple_open
};

static ssize_t pktlog_size_write(struct file *file, const char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	struct ath11k *ar = file->private_data;
	u32 pktlog_size;

	if (kstrtou32_from_user(ubuf, count, 0, &pktlog_size))
		return -EINVAL;

	if (pktlog_size == ar->debug.pktlog.buf_size)
		return count;

	if (ar->debug.is_pkt_logging) {
		pr_debug("Stop packet logging before changing the size\n");
		return -EINVAL;
	}

	ar->debug.pktlog.buf_size = pktlog_size;

	return count;
}

static ssize_t pktlog_size_read(struct file *file, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	char buf[32];
	struct ath11k *ar = file->private_data;
	int len = 0;

	len = scnprintf(buf, sizeof(buf) - len, "%uL\n",
			ar->debug.pktlog.buf_size);
	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_pktlog_size = {
	.read = pktlog_size_read,
	.write = pktlog_size_write,
	.open = simple_open
};

static void ath_pktlog_init(struct ath11k *ar)
{
	struct ath_pktlog *pktlog = &ar->debug.pktlog;

	spin_lock_init(&pktlog->lock);
	pktlog->buf_size = ATH_DEBUGFS_PKTLOG_SIZE_DEFAULT;
	pktlog->buf = NULL;

	pktlog->hdr_size = sizeof(struct ath_pktlog_hdr);
	pktlog->hdr_size_field_offset =
		   offsetof(struct ath_pktlog_hdr, size);
}

void ath11k_init_pktlog(struct ath11k *ar)
{
	ar->debug.debugfs_pktlog = debugfs_create_dir("pktlog",
						      ar->debug.debugfs_pdev);
	debugfs_create_file("start", S_IRUGO | S_IWUSR,
			    ar->debug.debugfs_pktlog, ar, &fops_pktlog_start);
	debugfs_create_file("size", S_IRUGO | S_IWUSR,
			    ar->debug.debugfs_pktlog, ar, &fops_pktlog_size);
	debugfs_create_file("dump", S_IRUGO | S_IWUSR,
			    ar->debug.debugfs_pktlog, ar, &fops_pktlog_dump);
	ath_pktlog_init(ar);
}

void ath11k_deinit_pktlog(struct ath11k *ar)
{
	struct ath_pktlog *pktlog = &ar->debug.pktlog;

	if (pktlog->buf != NULL)
		ath_pktlog_release(pktlog);
}

static int ath_pktlog_pull_hdr(struct ath_pktlog_hdr_arg *arg,
			       u8 *data)
{
	struct ath_pktlog_hdr *hdr = (struct ath_pktlog_hdr *)data;

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

	return 0;
}

static void ath_pktlog_write_buf(struct ath_pktlog *pl_info,
				 struct ath_pktlog_hdr_arg *hdr_arg)
{
	char *log_data;

	log_data = ath_pktlog_getbuf(pl_info, hdr_arg);
	if (!log_data)
		return;

	memcpy(log_data, hdr_arg->payload, hdr_arg->payload_size);
}

void ath11k_htt_pktlog_process(struct ath11k *ar, u8 *data)
{
	struct ath_pktlog *pl_info = &ar->debug.pktlog;
	struct ath_pktlog_hdr_arg hdr_arg;
	int ret;

	if  (!ar->debug.is_pkt_logging)
		return;

	ret = ath_pktlog_pull_hdr(&hdr_arg, data);
	if (ret)
		return;

	ath_pktlog_write_buf(pl_info, &hdr_arg);
}

void ath11k_htt_ppdu_pktlog_process(struct ath11k *ar, u8 *data, u32 len)
{
	struct ath_pktlog *pl_info = &ar->debug.pktlog;
	struct ath_pktlog_hdr hdr;
	struct ath_pktlog_hdr_arg hdr_arg;

	if  (!ar->debug.is_pkt_logging)
		return;

	hdr.flags = (1 << PKTLOG_FLG_FRM_TYPE_REMOTE_S);
	hdr.missed_cnt = 0;
	hdr.log_type = ATH11K_PKTLOG_TYPE_PPDU_STATS;
	hdr.timestamp = 0;
	hdr.size = len;
	hdr.type_specific_data = 0;

	hdr_arg.log_type = hdr.log_type;
	hdr_arg.payload_size = hdr.size;
	hdr_arg.payload = (u8 *)data;
	hdr_arg.pktlog_hdr = (u8 *)&hdr;

	ath_pktlog_write_buf(pl_info, &hdr_arg);
}

void ath11k_rx_stats_buf_pktlog_process(struct ath11k *ar, u8 *data, u16 log_type, u32 len)
{
	struct ath_pktlog *pl_info = &ar->debug.pktlog;
	struct ath_pktlog_hdr hdr;
	struct ath_pktlog_hdr_arg hdr_arg;

	if  (!ar->debug.is_pkt_logging)
		return;

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

	ath_pktlog_write_buf(pl_info, &hdr_arg);
}

void ath11k_cbf_pktlog_process(struct ath11k *ar, u8 *data, u32 len,
			       struct htt_t2h_ppdu_stats_ind_hdr *ind_hdr,
			       struct htt_ppdu_stats_rx_mgmtctrl_payload_tlv *tlv_hdr)
{
	struct ath_pktlog *pl_info = &ar->debug.pktlog;
	struct ath_pktlog_hdr hdr;
	struct ath_pktlog_hdr_arg hdr_arg;
	uint32_t hdr_size;
	char *log_data;

	if  (!ar->debug.is_pkt_logging)
	        return;

	hdr_size = sizeof(struct htt_t2h_ppdu_stats_ind_hdr) +
			  sizeof(struct htt_ppdu_stats_rx_mgmtctrl_payload_tlv);
	hdr.flags = (1 << PKTLOG_FLG_FRM_TYPE_REMOTE_S);
	hdr.missed_cnt = 0;
	hdr.log_type = ATH11K_PKTLOG_TYPE_PPDU_STATS;
	hdr.timestamp = 0;
	hdr.size = ALIGN(len + hdr_size, PKTLOG_ALIGN);

	hdr_arg.log_type = hdr.log_type;
	hdr_arg.payload_size = hdr.size;
	hdr_arg.payload = data;
	hdr_arg.pktlog_hdr = (u8 *)&hdr;

	log_data = ath_pktlog_getbuf(pl_info, &hdr_arg);
	if (!log_data)
	        return;

	memcpy(log_data, ind_hdr, sizeof(struct htt_t2h_ppdu_stats_ind_hdr));

	log_data += sizeof(struct htt_t2h_ppdu_stats_ind_hdr);
	memcpy(log_data, tlv_hdr,
	       sizeof(struct htt_ppdu_stats_rx_mgmtctrl_payload_tlv));

	log_data += sizeof(struct htt_ppdu_stats_rx_mgmtctrl_payload_tlv);
	memcpy(log_data, data, len);
}
