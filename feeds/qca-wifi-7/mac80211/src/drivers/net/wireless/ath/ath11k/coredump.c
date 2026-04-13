// SPDX-License-Identifier: BSD-3-Clause-Clear
/**
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 */

#include <linux/devcoredump.h>
#include <linux/platform_device.h>
#include <linux/dma-direction.h>
#include <linux/mhi.h>
#include <linux/pci.h>
#include <linux/mm.h>
#include <linux/uuid.h>
#include <linux/time.h>
#include "core.h"
#include "coredump.h"
#include "pci.h"
#include "debug.h"

struct ath11k_coredump_segment_info ath11k_coredump_seg_info;
EXPORT_SYMBOL(ath11k_coredump_seg_info);

static void *ath11k_coredump_find_segment(loff_t user_offset,
					  struct ath11k_dump_segment *segment,
					  int phnum, size_t *data_left)
{
	int i;

	for (i = 0; i < phnum; i++, segment++) {
		if (user_offset < segment->len) {
			*data_left = user_offset;
			return segment;
		}
		user_offset -= segment->len;
	}

	*data_left = 0;
	return NULL;
}

static ssize_t ath11k_coredump_read_q6dump(char *buffer, loff_t offset, size_t count,
				void *data, size_t header_size)
{
	struct ath11k_coredump_state *dump_state = data;
	struct ath11k_dump_segment *segments = dump_state->segments;
	struct ath11k_dump_segment *seg;
	void *elfcore = dump_state->header;
	size_t data_left, copy_size, bytes_left = count;
	void __iomem *addr;

	/* Copy the header first */
	if (offset < header_size) {
		copy_size = header_size - offset;
		copy_size = min(copy_size, bytes_left);

		memcpy(buffer, elfcore + offset, copy_size);
		offset += copy_size;
		bytes_left -= copy_size;
		buffer += copy_size;

		return copy_size;
	}

	while (bytes_left) {
		seg = ath11k_coredump_find_segment(offset - header_size,
				    segments, dump_state->num_seg,
				    &data_left);
		/* End of segments check */
		if (!seg) {
			pr_info("Ramdump complete %lld bytes read\n", offset);
			return 0;
		}

		if (data_left)
			copy_size = min_t(size_t, bytes_left, data_left);
		else
			copy_size = bytes_left;

		addr = seg->vaddr;
		addr += data_left;
		memcpy_fromio(buffer, addr, copy_size);

		offset += copy_size;
		buffer += copy_size;
		bytes_left -= copy_size;
	}

	return count - bytes_left;
}

static void ath11k_coredump_free_q6dump(void *data)
{
	struct ath11k_coredump_state *dump_state = data;

	complete(&dump_state->dump_done);
}

void ath11k_coredump_build_inline(struct ath11k_base *ab,
				  struct ath11k_dump_segment *segments, int num_seg)
{
	struct ath11k_coredump_state *dump_state;
	struct timespec64 timestamp;
	struct ath11k_dump_file_data *file_data;
	size_t header_size;
	struct ath11k_pci *ar_pci = (struct ath11k_pci *)ab->drv_priv;
	struct device dev;
	u8 *buf;

	header_size = sizeof(struct ath11k_dump_file_data);
	header_size += num_seg * sizeof(*segments);
	header_size = PAGE_ALIGN(header_size);
	buf = vzalloc(header_size);
	if (!buf)
		return;

	ATH11K_MEMORY_STATS_INC(ab, malloc_size, header_size);

	file_data = (struct ath11k_dump_file_data *)buf;
	strscpy(file_data->df_magic, "ATH11K-FW-DUMP",
	        sizeof(file_data->df_magic));
	file_data->len = cpu_to_le32(header_size);
	file_data->version = cpu_to_le32(ATH11K_FW_CRASH_DUMP_VERSION);
	if (ab->hw_rev == ATH11K_HW_QCN6122) {
		file_data->chip_id = ab->qmi.target.chip_id;
		file_data->qrtr_id = ab->qmi.service_ins_id;
		file_data->bus_id = ab->userpd_id;
		dev = ab->pdev->dev;
	} else {
		file_data->chip_id = ar_pci->dev_id;
		file_data->qrtr_id = ar_pci->ab->qmi.service_ins_id;
		file_data->bus_id = pci_domain_nr(ar_pci->pdev->bus);
		dev = ar_pci->pdev->dev;
	}
	if (file_data->bus_id > ATH11K_MAX_PCI_DOMAINS)
		file_data->bus_id = ATH11K_MAX_PCI_DOMAINS;
	guid_gen(&file_data->guid);
	ktime_get_real_ts64(&timestamp);
	file_data->tv_sec = cpu_to_le64(timestamp.tv_sec);
	file_data->tv_nsec = cpu_to_le64(timestamp.tv_nsec);
	file_data->num_seg = num_seg;
	file_data->seg_size = sizeof(*segments);

	/* copy segment details to file */
	buf += offsetof(struct ath11k_dump_file_data, seg);
	file_data->seg =(struct ath11k_dump_segment *)buf;
	memcpy(file_data->seg, segments, num_seg * sizeof(*segments));

	dump_state = vzalloc(sizeof(*dump_state));
	if(!dump_state) {
		ATH11K_MEMORY_STATS_DEC(ab, malloc_size, header_size);
		return;
	}

	dump_state->header = file_data;
	dump_state->num_seg = num_seg;
	dump_state->segments = segments;
	init_completion(&dump_state->dump_done);

	dev_coredumpm(&dev, NULL, dump_state, header_size, GFP_KERNEL,
		      ath11k_coredump_read_q6dump, ath11k_coredump_free_q6dump);

	/* Wait until the dump is read and free is called */
	wait_for_completion(&dump_state->dump_done);
	ATH11K_MEMORY_STATS_DEC(ab, malloc_size, sizeof(*dump_state));
	ATH11K_MEMORY_STATS_DEC(ab, malloc_size, header_size);
	vfree(dump_state);
	vfree(file_data);
}

void ath11k_coredump_download_rddm(struct ath11k_base *ab)
{
	struct ath11k_pci *ar_pci = (struct ath11k_pci *)ab->drv_priv;
	struct mhi_controller *mhi_ctrl = ar_pci->mhi_ctrl;
	struct image_info *rddm_img, *fw_img;
	struct ath11k_dump_segment *segment, *seg_info;
	int i, rem_seg_cnt = 0, len, num_seg, seg_sz, qdss_seg_cnt = 1;

#if LINUX_VERSION_IS_LESS(5,4,0)
	mhi_download_rddm_img(mhi_ctrl, false);
#elif LINUX_VERSION_IS_GEQ(5,4,0)
	mhi_download_rddm_image(mhi_ctrl, false);
#endif

	rddm_img = mhi_ctrl->rddm_image;
	fw_img = mhi_ctrl->fbc_image;

	for (i = 0; i < ab->qmi.mem_seg_count; i++) {
		if (ab->qmi.target_mem[i].type == HOST_DDR_REGION_TYPE ||
		    (ab->qmi.target_mem[i].type == CALDB_MEM_REGION_TYPE && ab->enable_cold_boot_cal && ab->hw_params.coldboot_cal_mm) ||
			ab->qmi.target_mem[i].type == M3_DUMP_REGION_TYPE ||
			ab->qmi.target_mem[i].type == PAGEABLE_MEM_REGION_TYPE)
			rem_seg_cnt++;
	}

	num_seg = fw_img->entries + rddm_img->entries + rem_seg_cnt;
	if (ab->is_qdss_tracing)
		num_seg += qdss_seg_cnt;

	len = num_seg * sizeof(*segment);

	seg_info = segment = (struct ath11k_dump_segment *)vzalloc(len);
	if (!seg_info)
		ath11k_warn(ab, "fail to alloc memory for rddm\n");

	for (i = 0; i < fw_img->entries ; i++) {
		if (!fw_img->mhi_buf[i].buf)
			continue;
		seg_sz = fw_img->mhi_buf[i].len;
		seg_info->len = PAGE_ALIGN(seg_sz);
		seg_info->addr = fw_img->mhi_buf[i].dma_addr;
		seg_info->vaddr = fw_img->mhi_buf[i].buf;
		seg_info->type = ATH11K_FW_CRASH_PAGING_DATA;
		ath11k_info(ab, "seg vaddr is %px len is 0x%x type %d\n",
			    seg_info->vaddr, seg_info->len, seg_info->type);

		seg_info++;
	}

	for (i = 0; i < rddm_img->entries; i++) {
		if (!rddm_img->mhi_buf[i].buf)
			continue;
		seg_sz = rddm_img->mhi_buf[i].len;
		seg_info->len = PAGE_ALIGN(seg_sz);
		seg_info->addr = rddm_img->mhi_buf[i].dma_addr;
		seg_info->vaddr = rddm_img->mhi_buf[i].buf;
		seg_info->type = ATH11K_FW_CRASH_RDDM_DATA;
		ath11k_info(ab, "seg vaddr is %px len is 0x%x type %d\n",
			    seg_info->vaddr, seg_info->len, seg_info->type);
		seg_info++;
	}

	for (i = 0; i < ab->qmi.mem_seg_count; i++) {
		if (ab->qmi.target_mem[i].type == HOST_DDR_REGION_TYPE ||
		    ab->qmi.target_mem[i].type == M3_DUMP_REGION_TYPE) {
			seg_info->len = ab->qmi.target_mem[i].size;
			seg_info->addr = ab->qmi.target_mem[i].paddr;
			seg_info->vaddr = ab->qmi.target_mem[i].vaddr;
			seg_info->type = ATH11K_FW_REMOTE_MEM_DATA;
			ath11k_info(ab, "seg vaddr is %px len is 0x%x type %d\n",
				    seg_info->vaddr, seg_info->len, seg_info->type);
			seg_info++;
		}
	}

	if (ab->is_qdss_tracing) {
		seg_info->len = ab->qmi.qdss_mem[0].size;
		seg_info->addr = ab->qmi.qdss_mem[0].paddr;
		seg_info->vaddr = ab->qmi.qdss_mem[0].vaddr;
		seg_info->type = ATH11K_FW_REMOTE_MEM_DATA;
		ath11k_info(ab, "seg vaddr is %px len is 0x%x type %d\n",
			    seg_info->vaddr, seg_info->len, seg_info->type);
		seg_info++;
	}

	for (i = 0; i < ab->qmi.mem_seg_count; i++) {
		if ((ab->qmi.target_mem[i].type == CALDB_MEM_REGION_TYPE &&
		     ab->enable_cold_boot_cal && ab->hw_params.coldboot_cal_mm)) {
			seg_info->len = ab->qmi.target_mem[i].size;
			seg_info->addr = ab->qmi.target_mem[i].paddr;
			seg_info->vaddr = ab->qmi.target_mem[i].vaddr;
			seg_info->type = ATH11K_FW_REMOTE_MEM_DATA;
			ath11k_info(ab, "seg vaddr is %px len is 0x%x type %d\n",
				    seg_info->vaddr, seg_info->len, seg_info->type);
			seg_info++;
		}
	}

	/* Crash the system once all the stats are dumped */
	if(!ab->fw_recovery_support) {
		ath11k_core_dump_bp_stats(ab);
		ath11k_hal_dump_srng_stats(ab);

		ath11k_coredump_seg_info.chip_id = ar_pci->dev_id;
		ath11k_coredump_seg_info.qrtr_id = ar_pci->ab->qmi.service_ins_id;
		ath11k_coredump_seg_info.bus_id = pci_domain_nr(ar_pci->pdev->bus);
		ath11k_coredump_seg_info.num_seg = num_seg;
		ath11k_coredump_seg_info.seg = segment;

		BUG_ON(1);
	} else {
		ath11k_coredump_build_inline(ab, segment, num_seg);
	}

	vfree(segment);
}

void ath11k_coredump_qdss_dump(struct ath11k_base *ab,
			       struct ath11k_qmi_event_qdss_trace_save_data *event_data)
{
	struct ath11k_dump_segment *segment;
	int len, num_seg;
	void *dump;

	num_seg = event_data->mem_seg_len;
	len = sizeof(*segment);
	segment = (struct ath11k_dump_segment *)vzalloc(len);
	if (!segment) {
		ath11k_warn(ab, "fail to alloc memory for qdss\n");
		return;
	}

	ATH11K_MEMORY_STATS_INC(ab, malloc_size, len);

	if (event_data->total_size &&
	    event_data->total_size <= ab->qmi.qdss_mem[0].size)
		dump = vzalloc(event_data->total_size);
	if (!dump) {
		ATH11K_MEMORY_STATS_DEC(ab, malloc_size, len);
		vfree(segment);
		return;
	}

	 ATH11K_MEMORY_STATS_INC(ab, malloc_size, event_data->total_size);

	if (num_seg == 1) {
		segment->len = event_data->mem_seg[0].size;
		segment->vaddr = ab->qmi.qdss_mem[0].vaddr;
		ath11k_dbg(ab, ATH11K_DBG_QMI, "seg vaddr is 0x%p len is 0x%x\n",
			   segment->vaddr, segment->len);
		segment->type = ATH11K_FW_QDSS_DATA;
	} else if (num_seg == 2) {
		/* FW sends the 2 segments in below format, we need to get
		* segment 0 first then segment 1
		*
		*  QDSS ETR Memory - 1MB
		* +---------------------+
		* |   segment 1 start   |
		* |                     |
		* |                     |
		* |                     |
		* |   segment 1 end     |
		* +---------------------+
		* |   segment 0 start   |
		* |                     |
		* |                     |
		* |   segment 0 end     |
		* +---------------------+
		*/

		if (event_data->mem_seg[1].addr != ab->qmi.qdss_mem[0].paddr) {
			ath11k_warn(ab, "Invalid seg 0 addr 0x%llx\n",
				    event_data->mem_seg[1].addr);
			goto out;
		}
		if (event_data->mem_seg[0].size + event_data->mem_seg[1].size !=
		    ab->qmi.qdss_mem[0].size) {
			ath11k_warn(ab, "Invalid total size 0x%x 0x%x\n",
				    event_data->mem_seg[0].size,
				    event_data->mem_seg[1].size);
			goto out;
		}

		ath11k_dbg(ab, ATH11K_DBG_QMI, "qdss mem seg0 addr 0x%llx size 0x%x\n",
			   event_data->mem_seg[0].addr, event_data->mem_seg[0].size);
		ath11k_dbg(ab, ATH11K_DBG_QMI, "qdss mem seg1 addr 0x%llx size 0x%x\n",
			   event_data->mem_seg[1].addr, event_data->mem_seg[1].size);

		memcpy(dump,
		       ab->qmi.qdss_mem[0].vaddr + event_data->mem_seg[1].size,
		       event_data->mem_seg[0].size);
		memcpy(dump + event_data->mem_seg[0].size,
		       ab->qmi.qdss_mem[0].vaddr, event_data->mem_seg[1].size);

		segment->len = event_data->mem_seg[0].size + event_data->mem_seg[1].size;
		segment->vaddr = dump;
		ath11k_dbg(ab, ATH11K_DBG_QMI, "seg vaddr is 0x%p and len is 0x%x\n",
			   segment->vaddr, segment->len);
		segment->type = ATH11K_FW_QDSS_DATA;
	}
	ath11k_coredump_build_inline(ab, segment, 1);
out:
	ATH11K_MEMORY_STATS_DEC(ab, malloc_size, event_data->total_size);
	ATH11K_MEMORY_STATS_DEC(ab, malloc_size, len);
	vfree(segment);
	vfree(dump);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
int ath11k_coredump_mhi_update_bhie_table(struct ath11k_base *ab, void *va,
					  phys_addr_t pa, size_t size)
{

	struct ath11k_pci *ar_pci = (struct ath11k_pci *)ab->drv_priv;
	struct mhi_controller *mhi_ctrl = ar_pci->mhi_ctrl;
	int ret;

	/* Attach Pageable region to MHI buffer so that it is
	 * included as part of pageable region in dumps
	 */
	ret = mhi_update_bhie_table_for_dyn_paging(mhi_ctrl, va, pa, size);
	if (ret)
		ath11k_dbg(ab, ATH11K_DBG_QMI,
				"failed to add Dynamic Paging region to MHI Buffer table %d\n", ret);

	return ret;
}
#endif
