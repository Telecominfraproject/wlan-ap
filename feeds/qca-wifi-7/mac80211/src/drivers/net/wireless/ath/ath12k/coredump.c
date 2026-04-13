// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/devcoredump.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/dma-direction.h>
#include <linux/mm.h>
#include <linux/uuid.h>
#include <linux/time.h>
#include <linux/elf.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/soc/qcom/mdt_loader.h>

#include "core.h"
#include "pci.h"
#include "hif.h"
#include "coredump.h"
#include "debug.h"
#include "mhi.h"
#include "ahb.h"

struct ath12k_coredump_segment_info ath12k_coredump_seg_info;
EXPORT_SYMBOL(ath12k_coredump_seg_info);
struct ath12k_coredump_info ath12k_coredump_ram_info;
EXPORT_SYMBOL(ath12k_coredump_ram_info);

static ssize_t ath12k_coredump_pci_read(char *buffer, loff_t offset,
					size_t count, void *data,
					size_t header_size)
{
	struct ath12k_pci_elf_coredump_state *st = data;
	size_t bytes_left = count;
	struct ath12k_dump_segment *seg = NULL;
	size_t copied = 0;
	int i;
	unsigned long data_left;
	size_t seg_copy_sz, copy_size, hdr_copy;
	void *addr;
	loff_t cur;

	/* Copy ELF header first */
	if (offset < header_size) {
		hdr_copy = min_t(size_t, bytes_left, header_size - offset);
		memcpy(buffer, (u8 *)st->elf_hdr + offset, hdr_copy);
		return hdr_copy;
	}

	offset -= header_size;

	while (bytes_left) {
		cur = 0;
		seg = NULL;

		for (i = 0; i < st->num_chunks; i++) {
			if (offset < cur + st->chunks[i].len) {
				seg = &st->chunks[i];
				break;
			}
			cur += st->chunks[i].len;
		}
		if (!seg)
			break;

		data_left = offset - cur;
		seg_copy_sz = seg->len - data_left;
		copy_size = min_t(size_t, bytes_left, seg_copy_sz);
		addr = (u8 *)seg->vaddr + data_left;

		if (data_left + copy_size <= seg->len) {
			memcpy_fromio(buffer, addr, copy_size);
		} else {
			ath12k_warn(st->ab, "Buffer copy would exceed segment bounds\n");
			break;
		}

		offset     += copy_size;
		buffer     += copy_size;
		bytes_left -= copy_size;
		copied     += copy_size;
	}

	return copied;
}

static void ath12k_coredump_pci_free(void *data)
{
	struct ath12k_pci_elf_coredump_state *st = data;

	complete(&st->dump_done);
}

static int ath12k_coredump_build_dump_info(struct ath12k_base *ab,
					struct ath12k_dump_segment *segments,
					u32 num_seg,
					struct ath12k_dump_segment **chunks_out,
					u32 *num_chunks)
{
	struct ath12k_dump_entry tmp[ATH12K_MAX_DUMP_ENTRIES] = {0};
	int i, type, num_dump_types = 0;
	struct ath12k_dump_segment *chunks;
	struct ath12k_pci_dump_file_data *df;
	struct ath12k_dump_entry *entry;
	void *data;
	size_t data_len;

	for (i = 0; i < num_seg; i++) {
		type = segments[i].type;
		if (type < 0 || type >= ATH12K_MAX_DUMP_ENTRIES)
			continue;

		if (tmp[type].entry_num == 0)
			tmp[type].entry_start = i + 1;
		tmp[type].type = type;
		tmp[type].entry_num++;
	}

	/* to find the number of dump types */
	for (i = 0; i < ATH12K_MAX_DUMP_ENTRIES; i++)
		if (tmp[i].entry_num)
			num_dump_types++;

	data_len = ALIGN(sizeof(struct ath12k_pci_dump_file_data) +
			num_dump_types * sizeof(struct ath12k_dump_entry), 4);
	data = kzalloc(data_len, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	df = data;
	df->magic = ATH12K_RAMDUMP_MAGIC;
	df->version = ATH12K_FW_CRASH_DUMP_V2;
	df->chipset = ((struct ath12k_pci *)ab->drv_priv)->dev_id;
	df->total_entries = num_dump_types;

	entry = df->entry;
	for (i = 0; i < ATH12K_MAX_DUMP_ENTRIES; i++) {
		if (tmp[i].entry_num) {
			*entry = tmp[i];
			entry++;
		}
	}

	chunks = kcalloc(1 + num_seg, sizeof(*chunks), GFP_KERNEL);
	if (!chunks) {
		kfree(data);
		return -ENOMEM;
	}

	chunks[0].addr  = 0;
	chunks[0].vaddr = data;
	chunks[0].len   = data_len;
	chunks[0].type  = 0xFFFFFFFF;

	for (i = 0; i < num_seg; i++)
		chunks[1 + i] = segments[i];

	*chunks_out     = chunks;
	*num_chunks = 1 + num_seg;
	return 0;
}

static int ath12k_coredump_build_elf32(struct ath12k_base *ab,
					struct ath12k_dump_segment *segments,
					u32 num_seg,
					struct ath12k_pci_elf_coredump_state **out_state)
{
	struct ath12k_pci_elf_coredump_state *st;
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;
	void *elf_hdr;
	struct ath12k_dump_segment *chunks;
	u32 num_chunks;
	u16 phnum;
	u32 elf_hdr_sz, cur_off, i, paddr32;
	int ret;

	ret = ath12k_coredump_build_dump_info(ab, segments,
						  num_seg, &chunks, &num_chunks);
	if (ret)
		return ret;

	phnum = num_chunks;
	elf_hdr_sz = sizeof(Elf32_Ehdr) + phnum * sizeof(Elf32_Phdr);

	elf_hdr = vzalloc(elf_hdr_sz);
	if (!elf_hdr) {
		kfree(chunks[0].vaddr);
		kfree(chunks);
		return -ENOMEM;
	}

	/* ELF header */
	ehdr = (Elf32_Ehdr *)elf_hdr;
	memcpy(ehdr->e_ident, ELFMAG, SELFMAG);
	ehdr->e_ident[EI_CLASS] = ELFCLASS32;
	ehdr->e_ident[EI_DATA] = ELFDATA2LSB;
	ehdr->e_ident[EI_VERSION] = EV_CURRENT;
	ehdr->e_ident[EI_OSABI] = ELFOSABI_NONE;
	ehdr->e_type = cpu_to_le16(ET_CORE);
	ehdr->e_machine = cpu_to_le16(EM_NONE);
	ehdr->e_version = cpu_to_le32(EV_CURRENT);
	ehdr->e_entry = cpu_to_le32(0);
	ehdr->e_phoff = cpu_to_le32(sizeof(Elf32_Ehdr));
	ehdr->e_shoff = cpu_to_le32(0);
	ehdr->e_flags = cpu_to_le32(0);
	ehdr->e_ehsize = cpu_to_le16(sizeof(Elf32_Ehdr));
	ehdr->e_phentsize = cpu_to_le16(sizeof(Elf32_Phdr));
	ehdr->e_phnum = cpu_to_le16(phnum);
	ehdr->e_shentsize = cpu_to_le16(0);
	ehdr->e_shnum = cpu_to_le16(0);
	ehdr->e_shstrndx = cpu_to_le16(0);

	/* Program headers */
	phdr = (Elf32_Phdr *)((u8 *)elf_hdr + sizeof(Elf32_Ehdr));
	cur_off = elf_hdr_sz;

	for (i = 0; i < phnum; i++) {
		paddr32 = (i == 0) ? 0 : (u32)chunks[i].addr;
		phdr[i].p_type   = cpu_to_le32(PT_LOAD);
		phdr[i].p_offset = cpu_to_le32(cur_off);
		phdr[i].p_vaddr  = cpu_to_le32(paddr32);
		phdr[i].p_paddr  = cpu_to_le32(paddr32);
		phdr[i].p_filesz = cpu_to_le32(chunks[i].len);
		phdr[i].p_memsz  = cpu_to_le32(chunks[i].len);
		phdr[i].p_flags  = cpu_to_le32(PF_R | PF_W | PF_X);
		phdr[i].p_align  = cpu_to_le32(0);
		cur_off += chunks[i].len;
	}

	st = kzalloc(sizeof(*st), GFP_KERNEL);
	if (!st) {
		vfree(elf_hdr);
		kfree(chunks[0].vaddr);
		kfree(chunks);
		return -ENOMEM;
	}

	st->ab         = ab;
	st->elf_hdr    = elf_hdr;
	st->elf_hdr_sz = elf_hdr_sz;
	st->chunks     = chunks;
	st->num_chunks = phnum;

	init_completion(&st->dump_done);
	*out_state = st;
	return 0;
}

static void *ath12k_coredump_find_segment(loff_t user_offset,
                                         struct ath12k_dump_segment *segment,
                                         int num_seg, size_t *data_left)
{
       int i;

       for (i = 0; i < num_seg; i++, segment++) {
               if (user_offset < segment->len) {
                       *data_left = user_offset;
                       return segment;
               }
               user_offset -= segment->len;
       }

       *data_left = 0;
       return NULL;
}

static ssize_t ath12k_coredump_read_q6dump(char *buffer, loff_t offset, size_t count,
                                          void *data, size_t header_size)
{
       struct ath12k_coredump_state *dump_state = data;
       struct ath12k_dump_segment *segments = dump_state->segments;
       struct ath12k_dump_segment *seg;
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
               seg = ath12k_coredump_find_segment(offset - header_size, segments,
                                                  dump_state->num_seg, &data_left);
               /* End of segments check */
               if (!seg) {
                       pr_info("Ramdump complete %lld bytes read\n", offset);
                       return 0;
               }

               if (data_left)
                       copy_size = min_t(size_t, bytes_left, data_left);
               else
                       copy_size = bytes_left;

               addr = (void __iomem *)seg->vaddr;
               addr += data_left;
               memcpy_fromio(buffer, addr, copy_size);

               offset += copy_size;
               buffer += copy_size;
               bytes_left -= copy_size;
       }

       return count - bytes_left;
}

static void ath12k_coredump_free_q6dump(void *data)
{
       struct ath12k_coredump_state *dump_state = data;

       complete(&dump_state->dump_done);
}

static ssize_t ath12k_coredump_read(char *buffer, loff_t offset, size_t count,
                       void *data, size_t datalen)
{
    struct ath12k_dump_segment *segments = data;

    int ret = 0;

    ret = memory_read_from_buffer(buffer, count, &offset,
                      segments->vaddr, datalen);
    if (!ret)
        ath12k_info(NULL, "Ramdump complete, %lld  bytes read\n", offset);

    return ret;
}

static void ath12k_coredump_free(void *data)
{
    struct ath12k_dump_segment *segments = data;

    complete(&segments->dump_done);
}

void ath12k_coredump_dump_segment(struct ath12k_base *ab,
                  struct ath12k_dump_segment *segments, size_t seg_len)
{
    struct device *dev;

    dev = ab->dev;
    init_completion(&segments->dump_done);

    dev_coredumpm(dev, THIS_MODULE,segments, seg_len, GFP_KERNEL,
              ath12k_coredump_read, ath12k_coredump_free);

    wait_for_completion(&segments->dump_done);
}

void ath12k_coredump_build_inline(struct ath12k_base *ab,
                                 struct ath12k_dump_segment *segments, int num_seg)
{
       struct ath12k_coredump_state dump_state;
       struct timespec64 timestamp;
       struct ath12k_dump_file_data *file_data;
       size_t header_size;
       struct ath12k_pci *ar_pci = (struct ath12k_pci *)ab->drv_priv;
       struct device *dev;
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);
       u8 *buf;

       header_size = sizeof(*file_data);
       header_size += num_seg * sizeof(*segments);
       header_size = PAGE_ALIGN(header_size);
       buf = kzalloc(header_size, GFP_KERNEL);
       if (!buf) {
		ath12k_warn(ab, "Failed to allocate memory for coredump\n");
               return;
	}

       file_data = (struct ath12k_dump_file_data *)buf;
       strscpy(file_data->df_magic, "ATH12K-FW-DUMP",
               sizeof(file_data->df_magic));
       file_data->len = cpu_to_le32(header_size);
       file_data->version = cpu_to_le32(ATH12K_FW_CRASH_DUMP_V2);
	if (ab->hif.bus == ATH12K_BUS_AHB || ab->hif.bus == ATH12K_BUS_HYBRID) {
		file_data->chip_id = ab->qmi.target.chip_id;
		file_data->qrtr_id = ab->qmi.service_ins_id;
		file_data->bus_id = ab_ahb->userpd_id;
	} else {
	       file_data->chip_id = cpu_to_le32(ar_pci->dev_id);
	       file_data->qrtr_id = cpu_to_le32(ar_pci->ab->qmi.service_ins_id);
	       file_data->bus_id = pci_domain_nr(ar_pci->pdev->bus);
	}
       dev = ab->dev;;

       guid_gen(&file_data->guid);
       ktime_get_real_ts64(&timestamp);
       file_data->tv_sec = cpu_to_le64(timestamp.tv_sec);
       file_data->tv_nsec = cpu_to_le64(timestamp.tv_nsec);
       file_data->num_seg = cpu_to_le32(num_seg);
       file_data->seg_size = cpu_to_le32(sizeof(*segments));

       /* copy segment details to file */
       buf += offsetof(struct ath12k_dump_file_data, seg);
       file_data->seg = (struct ath12k_dump_segment *)buf;
       memcpy(file_data->seg, segments, num_seg * sizeof(*segments));

       dump_state.header = file_data;
       dump_state.num_seg = num_seg;
       dump_state.segments = segments;
       init_completion(&dump_state.dump_done);

       dev_coredumpm(dev, THIS_MODULE, &dump_state, header_size, GFP_KERNEL,
                     ath12k_coredump_read_q6dump, ath12k_coredump_free_q6dump);

       /* Wait until the dump is read and free is called */
       wait_for_completion(&dump_state.dump_done);
       kfree(file_data);
}

enum
ath12k_fw_crash_dump_type ath12k_coredump_get_dump_type(enum ath12k_qmi_target_mem type)
{
	enum ath12k_fw_crash_dump_type dump_type;

	switch (type) {
	case HOST_DDR_REGION_TYPE:
		dump_type = FW_CRASH_DUMP_REMOTE_MEM_DATA;
		break;
	case M3_DUMP_REGION_TYPE:
		dump_type = FW_CRASH_DUMP_M3_DUMP;
		break;
	case PAGEABLE_MEM_REGION_TYPE:
		dump_type = FW_CRASH_DUMP_PAGEABLE_DATA;
		break;
	case CALDB_MEM_REGION_TYPE:
		dump_type = FW_CRASH_DUMP_CALDB_DATA;
		break;
	case MLO_GLOBAL_MEM_REGION_TYPE:
		dump_type = FW_CRASH_DUMP_MLO_GLOBAL_DATA;
		break;
	case AFC_REGION_TYPE:
		dump_type = FW_CRASH_DUMP_AFC_DATA;
		break;
	default:
		dump_type = FW_CRASH_DUMP_TYPE_MAX;
		break;
	}

	return dump_type;
}

#ifdef CPTCFG_ATH12K_COREDUMP
void ath12k_coredump_upload(struct work_struct *work)
{
	struct ath12k_base *ab = container_of(work, struct ath12k_base, dump_work);

	ath12k_info(ab, "Uploading coredump\n");
	// dev_coredumpv() takes ownership of the buffer
	dev_coredumpv(ab->dev, ab->dump_data, ab->ath12k_coredump_len, GFP_KERNEL);
	ab->dump_data = NULL;
}
#endif

static void ath12k_coredump_q6crash_reason(struct ath12k_base *ab)
{
        int i = 0;
        uint64_t coredump_offset = 0;
        struct ath12k_pci *ar_pci = (struct ath12k_pci *)ab->drv_priv;
        struct mhi_controller *mhi_ctrl = ar_pci->mhi_ctrl;
        struct mhi_buf *mhi_buf;
        struct image_info *rddm_image;
        struct ath12k_coredump_q6ramdump_header *ramdump_header;
        struct ath12k_coredump_q6ramdump_entry *ramdump_table;
        char *msg = NULL;
        struct pci_dev *pci_dev = ar_pci->pdev;

        rddm_image = mhi_ctrl->rddm_image;
        mhi_buf = rddm_image->mhi_buf;

        ath12k_info(ab, "CRASHED - [DID:DOMAIN:BUS:SLOT] - %x:%04u:%02u:%02u\n",
                    pci_dev->device, pci_dev->bus->domain_nr,
                    pci_dev->bus->number, PCI_SLOT(pci_dev->devfn));

        /* Get RDDM header size */
        ramdump_header = (struct ath12k_coredump_q6ramdump_header *)mhi_buf[0].buf;
        ramdump_table = ramdump_header->ramdump_table;
        coredump_offset = le32_to_cpu(ramdump_header->header_size);

        /* Traverse ramdump table to get coredump offset */
        while (i < MAX_RAMDUMP_TABLE_SIZE) {
                if (!strncmp(ramdump_table->description, COREDUMP_DESC,
                             sizeof(COREDUMP_DESC)) ||
                    !strncmp(ramdump_table->description, Q6_SFR_DESC,
                             sizeof(Q6_SFR_DESC))) {
                        break;
                }
                coredump_offset += le64_to_cpu(ramdump_table->size);
                ramdump_table++;
                i++;
        }

        if (i == MAX_RAMDUMP_TABLE_SIZE) {
                ath12k_warn(ab, "Cannot find '%s' entry in ramdump\n",
                            COREDUMP_DESC);
                return;
        }

        /* Locate coredump data from the ramdump segments */
        for (i = 0; i < rddm_image->entries; i++) {
                if (coredump_offset < mhi_buf[i].len) {
                        msg = mhi_buf[i].buf + coredump_offset;
                        break;
                }

                coredump_offset -= mhi_buf[i].len;
        }

        if (msg && msg[0])
                ath12k_err(ab, "Fatal error received from wcss!\n%s\n",
                            msg);
}

void ath12k_coredump_download_rddm(struct ath12k_base *ab)
{
       struct ath12k_pci *ar_pci = (struct ath12k_pci *)ab->drv_priv;
       struct mhi_controller *mhi_ctrl = ar_pci->mhi_ctrl;
       struct image_info *rddm_img, *fw_img;
       struct ath12k_dump_segment *segment, *seg_info;
       int i, rem_seg_cnt = 0, len, num_seg, seg_sz, qdss_seg_cnt = 1;

	int skip_count = 0;
	enum ath12k_fw_crash_dump_type mem_type;
	struct ath12k_coredump_segment_info *chip_seg;
	int dump_count;
	struct ath12k_hw_group *ag = ab->ag;
	bool state = false;

	if (ath12k_check_erp_power_down(ag) && ab->pm_suspend)
		return;

	if (ab->in_panic)
		state = true;


       ath12k_mhi_coredump(mhi_ctrl, state);
        ath12k_coredump_q6crash_reason(ab);

       rddm_img = mhi_ctrl->rddm_image;
       fw_img = mhi_ctrl->fbc_image;

       for (i = 0; i < ab->qmi.mem_seg_count; i++) {
               if (ab->qmi.target_mem[i].type == HOST_DDR_REGION_TYPE ||
                   (ab->qmi.target_mem[i].type == CALDB_MEM_REGION_TYPE &&
		   ath12k_cold_boot_cal && ab->hw_params->cold_boot_calib) ||
                   ab->qmi.target_mem[i].type == M3_DUMP_REGION_TYPE ||
			ab->qmi.target_mem[i].type == PAGEABLE_MEM_REGION_TYPE ||
			ab->qmi.target_mem[i].type == MLO_GLOBAL_MEM_REGION_TYPE ||
			ab->qmi.target_mem[i].type == AFC_REGION_TYPE)

                       rem_seg_cnt++;
       }

       num_seg = fw_img->entries + rddm_img->entries + rem_seg_cnt;

#ifdef CPTCFG_ATHDEBUG
	if (ab->is_qdss_tracing)
		num_seg += qdss_seg_cnt;
#endif

       len = num_seg * sizeof(*segment);

       segment = kzalloc(len, GFP_NOWAIT);
       if (!segment) {
		ath12k_err(ab, " Failed to allocate memory for segment for rddm download\n");
               return;
	}

       seg_info = segment;
       for (i = 0; i < fw_img->entries ; i++) {

		if (!fw_img->mhi_buf[i].buf) {
			skip_count++;
			continue;
		}
               seg_sz = fw_img->mhi_buf[i].len;
               seg_info->len = PAGE_ALIGN(seg_sz);
               seg_info->addr = fw_img->mhi_buf[i].dma_addr;
               seg_info->vaddr = fw_img->mhi_buf[i].buf;
               seg_info->type = FW_CRASH_DUMP_PAGING_DATA;
               seg_info++;
       }

       for (i = 0; i < rddm_img->entries; i++) {

		if (!rddm_img->mhi_buf[i].buf) {
			skip_count++;
			continue;
		}

               seg_sz = rddm_img->mhi_buf[i].len;
               seg_info->len = PAGE_ALIGN(seg_sz);
               seg_info->addr = rddm_img->mhi_buf[i].dma_addr;
               seg_info->vaddr = rddm_img->mhi_buf[i].buf;
               seg_info->type = FW_CRASH_DUMP_RDDM_DATA;
               seg_info++;
       }

       for (i = 0; i < ab->qmi.mem_seg_count; i++) {
		mem_type = ath12k_coredump_get_dump_type(ab->qmi.target_mem[i].type);
		if(mem_type == FW_CRASH_DUMP_TYPE_MAX) {
			ath12k_info(ab, "target mem region type %d not supported", ab->qmi.target_mem[i].type);
			continue;
		}

		if (mem_type == FW_CRASH_DUMP_CALDB_DATA &&
		    !(ath12k_cold_boot_cal && ab->hw_params->cold_boot_calib))
			continue;

		if (!ab->qmi.target_mem[i].paddr) {
			skip_count++;
			ath12k_info(ab, "Skipping mem region type %d", ab->qmi.target_mem[i].type);
			continue;
		}
		seg_info->len = ab->qmi.target_mem[i].size;
		seg_info->addr = ab->qmi.target_mem[i].paddr;
		seg_info->vaddr = ab->qmi.target_mem[i].v.ioaddr;
		seg_info->type = mem_type;
		ath12k_info(ab,
			    "seg vaddr is %px len is 0x%x type %d\n",
			    seg_info->vaddr,
			    seg_info->len,
			    seg_info->type);
		seg_info++;

       }

#ifdef CPTCFG_ATHDEBUG
	if (ab->is_qdss_tracing) {
		seg_info->len = ab->dbg_qmi.qdss_mem[0].size;
		seg_info->addr = ab->dbg_qmi.qdss_mem[0].paddr;
		seg_info->vaddr = ab->dbg_qmi.qdss_mem[0].v.ioaddr;
		seg_info->type = FW_CRASH_DUMP_QDSS_DATA;
		seg_info++;
	}
#endif

	num_seg = num_seg - skip_count;

	if (!ab->fw_recovery_support || ab->in_panic ||
	    test_bit(ATH12K_GROUP_FLAG_UNREGISTER, &ag->flags)) {
		if (ag->mlo_capable) {
			dump_count = atomic_read(&ath12k_coredump_ram_info.num_chip);
			if (dump_count >= ATH12K_MAX_SOCS) {
				ath12k_err(ab, "invalid chip number %d\n",
					   dump_count);
				return;
			} else {
				chip_seg = &ath12k_coredump_ram_info.chip_seg_info[dump_count];
				chip_seg->chip_id = ar_pci->dev_id;
				chip_seg->qrtr_id = ar_pci->ab->qmi.service_ins_id;
				chip_seg->bus_id = pci_domain_nr(ar_pci->pdev->bus);
				chip_seg->num_seg = num_seg;
				chip_seg->seg = segment;
				atomic_inc(&ath12k_coredump_ram_info.num_chip);
			}
		} else {
			/* This part of code for 12.2 without mlo_capable=1 */
			dump_count = atomic_read(&ath12k_coredump_ram_info.num_chip);
			chip_seg = &ath12k_coredump_ram_info.chip_seg_info[dump_count];
			chip_seg->chip_id = ar_pci->dev_id;
			chip_seg->qrtr_id = ar_pci->ab->qmi.service_ins_id;
			chip_seg->bus_id = pci_domain_nr(ar_pci->pdev->bus);
			chip_seg->num_seg = num_seg;
			chip_seg->seg = segment;
			atomic_inc(&ath12k_coredump_ram_info.num_chip);
		}

		chip_seg = &ath12k_coredump_seg_info;
		chip_seg->chip_id = ar_pci->dev_id;
		chip_seg->qrtr_id = ar_pci->ab->qmi.service_ins_id;
		chip_seg->bus_id = pci_domain_nr(ar_pci->pdev->bus);
		chip_seg->num_seg = num_seg;
		chip_seg->seg = segment;

		ath12k_core_issue_bug_on(ab);

	} else if (!ab->in_panic) {
		struct ath12k_pci_elf_coredump_state *st = NULL;
		int ret;

		ath12k_info(ab, "WLAN target is restarting\n");

		ret = ath12k_coredump_build_elf32(ab, segment, num_seg, &st);

		if (ret) {
			ath12k_err(ab, "ELF32 build failed: %d\n", ret);
			kfree(segment);
			return;
		}
		dev_coredumpm(ab->dev, THIS_MODULE, st, st->elf_hdr_sz, GFP_KERNEL,
				ath12k_coredump_pci_read, ath12k_coredump_pci_free);
		wait_for_completion(&st->dump_done);
		vfree(st->elf_hdr);
		kfree(st->chunks[0].vaddr);
		kfree(st->chunks);
		kfree(st);
		kfree(segment);
	}
}

#ifdef CPTCFG_ATH12K_COREDUMP
void ath12k_coredump_collect(struct ath12k_base *ab)
{
	ath12k_coredump_q6crash_reason(ab);
	ath12k_hif_coredump_download(ab);
}
#endif

#define ELFCLASS32     1
#define EM_NONE                0
#define ET_CORE        4
#define EV_CURRENT     1

static void *ath12k_coredump_find_ahb_segment(loff_t user_offset,
                                              struct ath12k_ahb_dump_segment *segment,
                                              int num_seg, unsigned long *data_left)
{
        int i;

        for (i = 0; i < num_seg; i++, segment++) {
                if (user_offset < segment->len) {
                        *data_left = user_offset;
                        return segment;
                }
                user_offset -= segment->len;
        }

        *data_left = 0;
        return NULL;
}

static ssize_t ath12k_userpd_coredump_read(char *buffer, loff_t offset, size_t count,
                                           void *data, size_t header_size)
{
        struct ath12k_elf_coredump_state *dump_state = data;
        struct ath12k_ahb_dump_segment *segments = dump_state->segments;
        struct ath12k_ahb_dump_segment *seg;
        void *elfcore = dump_state->header;
        size_t copy_size, bytes_left = count;
        void *addr;
        unsigned long data_left, seg_copy_sz;

       /* Copy the header first */
        if (offset < header_size) {
                memcpy(buffer, elfcore, header_size);
                offset += header_size;
                bytes_left -= header_size;
                buffer += header_size;
                return header_size;
        }

        while (bytes_left) {
                seg = ath12k_coredump_find_ahb_segment(offset - header_size, segments,
                                                       dump_state->num_seg, &data_left);
                if (!seg) {
                        pr_info("Ramdump complete %lld bytes read\n", offset);
                        return 0;
                }

                seg_copy_sz = seg->len - data_left;
                if (seg_copy_sz)
                        copy_size = min_t(size_t, bytes_left, seg_copy_sz);
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

static void ath12k_userpd_coredump_free(void *data)
{
        struct ath12k_elf_coredump_state *elf_dump_state = data;

        complete(&elf_dump_state->dump_done);
}

static void ath12k_coredump_free_seg_info(struct ath12k_base *ab, void *segment,
                     int num_phdr)
{
        struct ath12k_ahb_dump_segment *seg_info = segment;
        int index;

        if (!seg_info)
                return;

        for (index = 0; index < num_phdr - 1; index++) {
                if (seg_info && seg_info->vaddr)
                        devm_iounmap(ab->dev, seg_info->vaddr);
                seg_info++;
        }
}

static int ath12k_coredump_build_seg_info(struct ath12k_base *ab, void *segment,
                                          int num_phdr, int *bootaddr)
{
        struct ath12k_ahb_dump_segment *seg_info = segment;
        struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);
        int index, ret = 0;
        ssize_t offset;
        struct reserved_mem *rmem;
        struct device_node *mem_regions;
        struct elf32_phdr *phdrs;
        struct elf32_phdr *phdr;
        struct elf32_hdr *ehdr;
        const struct firmware *fw;
        char fw_name[ATH12K_USERPD_FW_NAME_LEN];

        /* First dump various region from reserved memory */
        for (index = 0; index < num_phdr - 1; index++) {
                mem_regions = of_parse_phandle(ab->dev->of_node, "dump-region", index);
                if (!mem_regions) {
                        ath12k_warn(ab, "Memory region not defined to collect dump\n");
                        return -EINVAL;
                }

                rmem = of_reserved_mem_lookup(mem_regions);
                if (!rmem) {
                        of_node_put(mem_regions);
                        return -EINVAL;
                }

                seg_info->len = rmem->size;
                seg_info->addr = rmem->base;
                seg_info->hdr_vaddr = (void *)rmem->base;
                seg_info->vaddr = devm_ioremap_wc(ab->dev, rmem->base, rmem->size);
                seg_info++;
                of_node_put(mem_regions);
        }

        snprintf(fw_name, sizeof(fw_name), "%s/%s/%s%d%s", ATH12K_FW_DIR,
                 ab->hw_params->fw.dir, ATH12K_AHB_FW_PREFIX, ab_ahb->userpd_id,
                 ATH12K_AHB_FW_SUFFIX);

        ret = request_firmware(&fw, fw_name, ab->dev);
        if (ret < 0) {
                ath12k_err(ab, "request_firmware failed\n");
                return ret;
        }

        ehdr = (struct elf32_hdr *)fw->data;
        *bootaddr = ehdr->e_entry;
        phdrs = (struct elf32_phdr *)(ehdr + 1);

        for (index = 0; index < ehdr->e_phnum; index++) {
                phdr = &phdrs[index];

                if (!mdt_phdr_valid(phdr))
                        continue;

                offset = phdr->p_paddr - ab_ahb->mem_phys;
                if (offset < 0 || offset + phdr->p_memsz > ab_ahb->mem_size) {
                        ath12k_err(ab, "segment outside memory range\n");
                        ret = -EINVAL;
                        goto end;
                }

                seg_info->addr = ab_ahb->mem_phys + offset;
                seg_info->hdr_vaddr = (void *)(uintptr_t)phdr->p_vaddr;
                seg_info->vaddr = ab_ahb->mem_region + offset;
                seg_info->len = phdr->p_memsz;
        }
end:
        /* request_firmware is used to read the headers which has
         * the start address this is used form the dump headers.
         * however these address are not used for reading the
         * coredump contents */

        release_firmware(fw);
        return ret;
}

void ath12k_coredump_ahb_collect(struct ath12k_base *ab)
{
        struct ath12k_elf_coredump_state elf_dump_state;
        Elf32_Ehdr *ehdr;
        Elf32_Phdr *phdr;
        u8 class = ELFCLASS32;
        struct ath12k_ahb_dump_segment *segment, *seg_info;
        size_t data_size, offset, seg_info_len;
        void *data;
        struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);
        int num_segs = 0, ret, index, phnum = 0;
        u32 bootaddr;

        num_segs = of_count_phandle_with_args(ab->dev->of_node, "dump-region", NULL);
        if (num_segs <= 0) {
                ath12k_warn(ab, "UserPD %d dump regions not defined\n", ab_ahb->userpd_id);
                return;
        }

        phnum = num_segs;
        /* Add one more segment to dump UPD FW info */
        phnum++;

        seg_info_len = phnum * sizeof(*segment);
        segment = kzalloc(seg_info_len, GFP_NOWAIT);
        if (!segment) {
                ath12k_warn(ab, "Memory unavailable\n");
                return;
        }

        ret = ath12k_coredump_build_seg_info(ab, segment, phnum, &bootaddr);
        if (ret) {
                ath12k_err(ab, "Failed to build segment info - %d\n", ret);
                goto end;
        }

        data_size = sizeof(*ehdr);
        data_size += phnum * sizeof(*phdr);
        data = vzalloc(data_size);
        if (!data)
                goto end;

        ehdr = (Elf32_Ehdr *)data;
        memcpy(ehdr->e_ident, ELFMAG, SELFMAG);
        ehdr->e_ident[EI_CLASS] = class;
        ehdr->e_ident[EI_DATA] = ELFDATA2LSB;
        ehdr->e_ident[EI_VERSION] = EV_CURRENT;
        ehdr->e_ident[EI_OSABI] = ELFOSABI_NONE;
        ehdr->e_type = ET_CORE;
        ehdr->e_machine = EM_NONE;
        ehdr->e_version = EV_CURRENT;
        ehdr->e_entry = bootaddr;
        ehdr->e_phoff = sizeof(*ehdr);
        ehdr->e_ehsize = sizeof(*ehdr);
        ehdr->e_phentsize = sizeof(*phdr);
        ehdr->e_phnum = phnum;

        phdr = (Elf32_Phdr *)(data + sizeof(*ehdr));
        offset = sizeof(*ehdr);
        offset += sizeof(*phdr) * phnum;
        seg_info = segment;
        for (index = 0; index < phnum; index++, seg_info++) {
                phdr->p_type = PT_LOAD;
                phdr->p_offset = offset;
                phdr->p_vaddr = (intptr_t)seg_info->hdr_vaddr;
                phdr->p_paddr = seg_info->addr;
                phdr->p_filesz = seg_info->len;
                phdr->p_memsz = seg_info->len;
                phdr->p_flags = PF_R | PF_W | PF_X;
                phdr->p_align = 0;
                offset += phdr->p_filesz;
                phdr++;
        }

        elf_dump_state.header = data;
        elf_dump_state.num_seg = phnum;
        elf_dump_state.ab =  ab;
        elf_dump_state.segments = segment;
        init_completion(&elf_dump_state.dump_done);

        dev_coredumpm(ab->dev, THIS_MODULE, &elf_dump_state, data_size, GFP_KERNEL,
                      ath12k_userpd_coredump_read, ath12k_userpd_coredump_free);

        wait_for_completion(&elf_dump_state.dump_done);
        vfree(elf_dump_state.header);
end:
        ath12k_coredump_free_seg_info(ab, segment, phnum);
        kfree(segment);
}

void ath12k_coredump_m3_dump(struct ath12k_base *ab,
			     struct ath12k_qmi_m3_dump_upload_req_data *event_data)
{
	struct target_mem_chunk *target_mem = ab->qmi.target_mem;
	struct ath12k_qmi_m3_dump_data m3_dump_data;
	void *dump;
	int i, ret = 0;

	dump = vzalloc(event_data->size);
	if (!dump) {
		return;
	}

	for (i = 0; i < ab->qmi.mem_seg_count; i++) {
		if (target_mem[i].paddr == event_data->addr &&
		    event_data->size <= target_mem[i].size)
			break;
	}

	if (i == ab->qmi.mem_seg_count) {
		ath12k_warn(ab, "qmi invalid paddr from firmware for M3 dump\n");
		ret = -EINVAL;
		vfree(dump);
		goto send_resp;
	}

	m3_dump_data.addr = target_mem[i].v.ioaddr;
	m3_dump_data.size = event_data->size;
	m3_dump_data.pdev_id = event_data->pdev_id;
	m3_dump_data.timestamp = ktime_to_ms(ktime_get());

	memcpy(dump, m3_dump_data.addr, m3_dump_data.size);

	dev_coredumpv(ab->dev, dump, m3_dump_data.size,
		      GFP_KERNEL);

send_resp:
       ret = ath12k_qmi_m3_dump_upload_done_ind_send(ab, event_data->pdev_id, ret);
       if (ret < 0)
               ath12k_warn(ab, "qmi M3 dump upload done failed\n");
}
