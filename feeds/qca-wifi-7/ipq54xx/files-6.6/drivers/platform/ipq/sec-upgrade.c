/*
 * Copyright (c) 2014 - 2015, 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <asm/cacheflush.h>
#include <linux/dma-map-ops.h>
#include <linux/kernel_read_file.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/elf.h>
#include <linux/decompress/unlzma.h>
#include <linux/decompress/generic.h>
#include <linux/tmelcom_ipc.h>
#include <crypto/hash.h>
#include <crypto/sha2.h>

#define QFPROM_MAX_VERSION_EXCEEDED             0x10
#define QFPROM_IS_AUTHENTICATE_CMD_RSP_SIZE	0x2

#define SW_TYPE_SEC_DATA			0x2B

#ifdef CONFIG_QCOM_TMELCOM
enum sw_types {
	SW_TYPE_DEFAULT		=	0xFF,
	SW_TYPE_TZ		=	0x7,
	SW_TYPE_APPSBL		=	0x9,
	SW_TYPE_HLOS		=	0x71,
	SW_TYPE_DEVCFG		=	0x5,
	SW_TYPE_TME		=	0x73,
	SW_TYPE_XBL_SC		=	0x36,
	SW_TYPE_XBL_CFG		=	0x25,
	SW_TYPE_ATF		=	0x1E,
	SW_TYPR_ROOTFS		=	0x67,
	SW_TYPE_Q6_ROOTPD	=	0xD,
	SW_TYPE_USERPD_WDT	=	0x12,
	SW_TYPE_USERPD_OEM	=	0x63,
	SW_TYPE_IU_FW		=	0x2C,

};

u32 sw_id_list[] = {SW_TYPE_TME, SW_TYPE_XBL_SC, SW_TYPE_XBL_CFG,
		    SW_TYPE_DEVCFG, SW_TYPE_TZ, SW_TYPE_ATF, SW_TYPE_APPSBL,
		    SW_TYPE_HLOS, SW_TYPR_ROOTFS, SW_TYPE_Q6_ROOTPD,
		    SW_TYPE_USERPD_WDT, SW_TYPE_USERPD_OEM, SW_TYPE_IU_FW};
#else
enum sw_types {
	SW_TYPE_DEFAULT		=	0xFF,
	SW_TYPE_SBL		=	0x0,
	SW_TYPE_TZ		=	0x7,
	SW_TYPE_APPSBL		=	0x9,
	SW_TYPE_HLOS		=	0x17,
	SW_TYPE_RPM		=	0xA,
	SW_TYPE_DEVCFG		=	0x5,
	SW_TYPE_APDP		=	0x200,
};

u32 sw_id_list[] = {};
#endif

static int gl_version_enable;
static int version_commit_enable;
static int decompress_error;
static struct load_segs_info *ld_seg_buff;
static int rootfs_auth_enable;

enum qti_sec_img_auth_args {
	QTI_SEC_IMG_SW_TYPE,
	QTI_SEC_IMG_ADDR,
	QTI_SEC_HASH_ADDR,
	QTI_SEC_AUTH_ARG_MAX
};

struct qfprom_node_cfg {
	bool is_rlbk_support;
};

static ssize_t
qfprom_show_authenticate(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	int ret;

	if (qcom_qfprom_show_auth_available())
		ret = qcom_qfprom_show_authenticate();
	else
		ret = ipq54xx_qcom_qfprom_show_authenticate();

	if (ret < 0)
		return ret;

	/* show needs a string response */
	if (ret == 1)
		buf[0] = '1';
	else
		buf[0] = '0';

	buf[1] = '\0';

	return QFPROM_IS_AUTHENTICATE_CMD_RSP_SIZE;
}

int write_version(struct device *dev, uint32_t type, uint32_t version)
{
	int ret;
	uint32_t qfprom_ret_ptr;
	u32 *id_list;
	uint32_t *qfprom_api_status = kzalloc(sizeof(uint32_t), GFP_KERNEL);

	if (!qfprom_api_status)
		return -ENOMEM;

	qfprom_ret_ptr = dma_map_single(dev, qfprom_api_status,
			sizeof(*qfprom_api_status), DMA_FROM_DEVICE);

	ret = dma_mapping_error(dev, qfprom_ret_ptr);
	if (ret) {
		pr_err("DMA Mapping Error(api_status)\n");
		goto err_write;
	}

	if (qcom_qfrom_fuse_row_write_available()) {
		ret = qcom_qfprom_write_version(type, version, qfprom_ret_ptr);
	} else {
		id_list = kzalloc(sizeof(sw_id_list), GFP_KERNEL);
		if (!id_list)
		    return -ENOMEM;

		memcpy(id_list, sw_id_list, sizeof(sw_id_list));
		ret = tmelcomm_secboot_update_arb_version_list(id_list,
							       sizeof(sw_id_list));
	}

	dma_unmap_single(dev, qfprom_ret_ptr,
			sizeof(*qfprom_api_status), DMA_FROM_DEVICE);

	if(ret)
		pr_err("%s: Error in QFPROM write (%d, %d)\n",
					__func__, ret, *qfprom_api_status);
	if (*qfprom_api_status == QFPROM_MAX_VERSION_EXCEEDED)
		pr_err("Version %u exceeds maximum limit. All fuses blown.\n",
							    version);

err_write:
	kfree(qfprom_api_status);
	return ret;
}

int read_version(struct device *dev, int type, uint32_t **version_ptr)
{
	int ret, ret1, ret2;
	struct qfprom_read {
		uint32_t sw_type;
		uint32_t value;
		uint32_t qfprom_ret_ptr;
	} rdip;
	u32 *qfprom_api_status;
	u32 version;

	if (!qcom_qfrom_fuse_row_read_available()) {
		ret = tmelcomm_secboot_get_arb_version(type, &version);
		if (!ret)
			**version_ptr = version;
		return ret;
	}

	qfprom_api_status = kzalloc(sizeof(uint32_t), GFP_KERNEL);
	if (!qfprom_api_status)
		return -ENOMEM;

	rdip.sw_type = type;
	rdip.value = dma_map_single(dev, *version_ptr,
		sizeof(uint32_t), DMA_FROM_DEVICE);

	rdip.qfprom_ret_ptr = dma_map_single(dev, qfprom_api_status,
		sizeof(*qfprom_api_status), DMA_FROM_DEVICE);

	ret1 = dma_mapping_error(dev, rdip.value);
	ret2 = dma_mapping_error(dev, rdip.qfprom_ret_ptr);

	if (ret1 == 0 && ret2 == 0) {
		ret = qcom_qfprom_read_version(type, rdip.value,
			rdip.qfprom_ret_ptr);
	}
	if (ret1 == 0) {
		dma_unmap_single(dev, rdip.value,
			sizeof(uint32_t), DMA_FROM_DEVICE);
	}
	if (ret2 == 0) {
		dma_unmap_single(dev, rdip.qfprom_ret_ptr,
			sizeof(*qfprom_api_status), DMA_FROM_DEVICE);
	}
	if (ret1 || ret2) {
		pr_err("DMA Mapping Error version ret %d api_status ret %d\n",
							ret1, ret2);
		ret = ret1 ? ret1 : ret2;
		goto err_read;
	}

	if (ret || *qfprom_api_status) {
		pr_err("%s: Error in QFPROM read (%d, %d)\n",
			 __func__, ret, *qfprom_api_status);
	}
err_read:
	kfree(qfprom_api_status);
	return ret;
}

static ssize_t generic_version(struct device *dev, const char *buf,
		uint32_t sw_type, int op, size_t count)
{
	int ret = 0;
	uint32_t *version = kzalloc(sizeof(uint32_t), GFP_KERNEL);

	if (!version)
		return -ENOMEM;

	/*
	 * Operation Type: Read: 1 and Write: 2
	 */
	switch (op) {
	case 1:
		ret = read_version(dev, sw_type, &version);
		if (ret) {
			pr_err("Error in reading version: %d\n", ret);
			goto err_generic;
		}
		ret = snprintf((char *)buf, 10, "%d\n", *version);
		break;
	case 2:
		/* Input validation handled here */
		ret = kstrtouint(buf, 0, version);
		if (ret)
			goto err_generic;

		/* Commit version is true if user input is greater than 0 */
		if (*version <= 0) {
			ret = -EINVAL;
			goto err_generic;
		}
		ret = write_version(dev, sw_type, *version);
		if (ret) {
			pr_err("Error in writing version: %d\n", ret);
			goto err_generic;
		}
		ret = count;
		break;
	default:
		ret = -EINVAL;
	}

err_generic:
	kfree(version);
	return ret;
}

#ifndef CONFIG_QCOM_TMELCOM
static ssize_t
show_sbl_version(struct device *dev,
		 struct device_attribute *attr,
		 char *buf)
{
	return generic_version(dev, buf, SW_TYPE_SBL, 1, 0);
}

static ssize_t
store_sbl_version(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	return generic_version(dev, buf, SW_TYPE_SBL, 2, count);
}

static ssize_t
store_tz_version(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	return generic_version(dev, buf, SW_TYPE_TZ, 2, count);
}

static ssize_t
store_appsbl_version(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	return generic_version(dev, buf, SW_TYPE_APPSBL, 2, count);
}

static ssize_t
store_hlos_version(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	return generic_version(dev, buf, SW_TYPE_HLOS, 2, count);
}

static ssize_t
show_rpm_version(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return generic_version(dev, buf, SW_TYPE_RPM, 1, 0);
}

static ssize_t
store_rpm_version(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	return generic_version(dev, buf, SW_TYPE_RPM, 2, count);
}

static ssize_t
store_devcfg_version(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	return generic_version(dev, buf, SW_TYPE_DEVCFG, 2, count);
}

static ssize_t
show_apdp_version(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return generic_version(dev, buf, SW_TYPE_APDP, 1, 0);
}

static ssize_t
store_apdp_version(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	return generic_version(dev, buf, SW_TYPE_APDP, 2, count);
}
#else
static ssize_t
show_tmel_version(struct device *dev,
		  struct device_attribute *attr,
		  char *buf)
{
	return generic_version(dev, buf, SW_TYPE_TME, 1, 0);
}

static ssize_t
show_xbl_sc_version(struct device *dev,
		    struct device_attribute *attr,
		    char *buf)
{
	return generic_version(dev, buf, SW_TYPE_XBL_SC, 1, 0);
}

static ssize_t
show_xbl_cfg_version(struct device *dev,
		     struct device_attribute *attr,
		     char *buf)
{
	return generic_version(dev, buf, SW_TYPE_XBL_CFG, 1, 0);
}

static ssize_t
show_atf_version(struct device *dev,
		 struct device_attribute *attr,
		 char *buf)
{
	return generic_version(dev, buf, SW_TYPE_ATF, 1, 0);
}

static ssize_t
show_q6_rootpd_version(struct device *dev,
		       struct device_attribute *attr,
		       char *buf)
{
	return generic_version(dev, buf, SW_TYPE_Q6_ROOTPD, 1, 0);
}

static ssize_t
show_userpd_wdt_version(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return generic_version(dev, buf, SW_TYPE_USERPD_WDT, 1, 0);
}

static ssize_t
show_userpd_oem_version(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return generic_version(dev, buf, SW_TYPE_USERPD_OEM, 1, 0);
}

static ssize_t
show_iu_fw_version(struct device *dev,
		   struct device_attribute *attr,
		   char *buf)
{
	return generic_version(dev, buf, SW_TYPE_IU_FW, 1, 0);
}

static ssize_t
store_read_commit_version(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	int ret;
	u64 type;
	u32 version;

	if (kstrtoull(buf, 0, &type))
		return -EINVAL;

	ret = tmelcomm_secboot_get_arb_version(type, &version);
	if (ret)
		dev_err(dev, "Error in Version read for sw_type : 0x%llx\n",
			type);
	else
		dev_info(dev, "Read version for sw_type 0x%llx is : 0x%x",
			 type, version);
	return count;
}
#endif

static ssize_t
show_devcfg_version(struct device *dev,
		    struct device_attribute *attr,
		    char *buf)
{
	return generic_version(dev, buf, SW_TYPE_DEVCFG, 1, 0);
}

static ssize_t
show_tz_version(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return generic_version(dev, buf, SW_TYPE_TZ, 1, 0);
}

static ssize_t
show_appsbl_version(struct device *dev,
		    struct device_attribute *attr,
		    char *buf)
{
	return generic_version(dev, buf, SW_TYPE_APPSBL, 1, 0);
}

static ssize_t
show_hlos_version(struct device *dev,
		  struct device_attribute *attr,
		  char *buf)
{
	return generic_version(dev, buf, SW_TYPE_HLOS, 1, 0);
}

static ssize_t
store_version_commit(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	return generic_version(dev, buf, 0, 2, count);
}

struct elf_info {
	Elf32_Off offset;
	Elf32_Word filesz;
	Elf32_Word memsize;
};

#define PT_LZMA_FLAG		0x8000000
#define PT_COMPRESS_FLAG	(PT_LOOS + PT_LZMA_FLAG + PT_LOAD)
#define IS_ELF(ehdr) ((ehdr).e_ident[EI_MAG0] == ELFMAG0 && \
			(ehdr).e_ident[EI_MAG1] == ELFMAG1 && \
			(ehdr).e_ident[EI_MAG2] == ELFMAG2 && \
			(ehdr).e_ident[EI_MAG3] == ELFMAG3)

bool is_compressed(void *header, struct elf_info *info)
{
	Elf32_Ehdr *elf_hdr = (Elf32_Ehdr*) header;
	Elf32_Phdr *prg_hdr;
	bool com_flg = false;

	if(!IS_ELF(*((Elf32_Ehdr*) header))) {
		pr_info("Image is not an elf\n");
		return com_flg;
	}

	prg_hdr = (Elf32_Phdr *) ((unsigned long)elf_hdr + elf_hdr->e_phoff);
	for (int i = 0; i < elf_hdr->e_phnum; i++, prg_hdr++) {
		if (prg_hdr->p_type == PT_COMPRESS_FLAG) {
			com_flg = true;
			info->filesz = prg_hdr->p_filesz;
			info->offset = prg_hdr->p_offset;
			info->memsize= prg_hdr->p_memsz;
			break;
		}
	}

	return com_flg;
}

static void error(char *x)
{
        printk(KERN_ERR "%s\n", x);
	decompress_error = 1;
}


int img_decompress(void *file_buf, long size, void *out_buf, struct elf_info *info) {
	decompress_fn decompressor = NULL;
	const char *compress_name = NULL;
	int ret = -EINVAL;
	unsigned long my_inptr;
	const unsigned char *comp_data = (char *)file_buf;
	long comp_size = size;

	decompressor = decompress_method(comp_data, comp_size, &compress_name);

	if(!decompressor || !compress_name)
	{
		printk("[%s] decompress method is not configured\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	ret = decompressor((char*) comp_data, comp_size, NULL, NULL, (char*) out_buf, &my_inptr, error);
	if(decompress_error)
		ret = -EIO;
	else
		ret = 0;
exit:
	return ret;
}

static int elf64_parse_ld_segments(void *va_addr, u32 *md_size, unsigned int pa_addr)
{
	Elf64_Ehdr *ehdr = (Elf64_Ehdr *)(uintptr_t)va_addr;
	Elf64_Phdr *phdr = (Elf64_Phdr *)(uintptr_t)(va_addr + ehdr->e_phoff);
	u8 lds_cnt = 0; /* Loadable segment count */
	u32 ld_start_addr;

	if ((!IS_ELF(*ehdr)) || ehdr->e_phnum < 3)
		return -EINVAL;
	/*
	 * Considering that first two segments of program header are not loadable segments,
	 * ignoring it for memory allocation
	 */
	ld_seg_buff = kzalloc(sizeof(*ld_seg_buff) * (ehdr->e_phnum - 2), GFP_KERNEL);
	if (!ld_seg_buff)
		return -ENOMEM;

	for (int i = 0; i < ehdr->e_phnum; i++, phdr++) {
		/* Sum of the offset and filesize of the NULL segment before
		 * the first loadable segment */
		if (phdr->p_type == PT_LOAD) {
			if (!lds_cnt && *md_size == 0) {
				--phdr;
				*md_size = phdr->p_offset + phdr->p_filesz;
				++phdr;
			}

			if (!phdr->p_filesz && !phdr->p_memsz)
				continue;
			if (!phdr->p_filesz && phdr->p_memsz) {
				ld_seg_buff[lds_cnt].start_addr = 0;
				ld_seg_buff[lds_cnt++].end_addr = 0;
				continue;
			}
			/* Populating the start and end address of valid loadable
			 * segments by adding the offset with the physical elf address
			 */
			ld_start_addr = phdr->p_offset + pa_addr;
			ld_seg_buff[lds_cnt].start_addr = ld_start_addr;
			ld_seg_buff[lds_cnt++].end_addr = ld_start_addr + phdr->p_filesz;

			pr_debug("lds_cnt: %d, start_addr: %x, end_addr: %x\n", lds_cnt,
				 ld_seg_buff[lds_cnt - 1].start_addr,
				 ld_seg_buff[lds_cnt - 1].end_addr);
		}
	}
	return lds_cnt;
}

static int elf32_parse_ld_segments(void *va_addr, u32 *md_size, unsigned int pa_addr)
{
	Elf32_Ehdr *ehdr = (Elf32_Ehdr *)(uintptr_t)va_addr;
	Elf32_Phdr *phdr = (Elf32_Phdr *)(uintptr_t)(va_addr + ehdr->e_phoff);
	u8 lds_cnt = 0;
	u32 ld_start_addr;

	if ((!IS_ELF(*ehdr)) || ehdr->e_phnum < 3)
		return -EINVAL;
	/*
	 * Considering that first two segments of program header are not loadable segments,
	 * ignoring it for memory allocation
	 */
	ld_seg_buff = kzalloc(sizeof(*ld_seg_buff) * (ehdr->e_phnum - 2), GFP_KERNEL);
	if (!ld_seg_buff)
		return -ENOMEM;

	for (int i = 0; i < ehdr->e_phnum; i++, phdr++) {
		/* Sum of the offset and filesize of the NULL segment before
		 * the first loadable segment */
		if (phdr->p_type == PT_LOAD) {
			if (!lds_cnt && *md_size == 0) {
				--phdr;
				*md_size = phdr->p_offset + phdr->p_filesz;
				++phdr;
			}

			if (!phdr->p_filesz && !phdr->p_memsz)
				continue;
			if (!phdr->p_filesz && phdr->p_memsz) {
				ld_seg_buff[lds_cnt].start_addr = 0;
				ld_seg_buff[lds_cnt++].end_addr = 0;
				continue;
			}
			/* Populating the start and end address of valid loadable
			 * segments by adding the offset with the physical elf address
			 */
			ld_start_addr = phdr->p_offset + pa_addr;
			ld_seg_buff[lds_cnt].start_addr = ld_start_addr;
			ld_seg_buff[lds_cnt++].end_addr = ld_start_addr + phdr->p_filesz;

			pr_debug("lds_cnt: %d, start_addr: %x, end_addr: %x\n", lds_cnt,
				 ld_seg_buff[lds_cnt - 1].start_addr,
				 ld_seg_buff[lds_cnt - 1].end_addr);
		}
	}
	return lds_cnt;
}

static int parse_n_extract_ld_segment(void *va_addr, u32 *md_size, unsigned int pa_addr)
{
	Elf32_Ehdr *ehdr = (Elf32_Ehdr *)va_addr;

	if (ehdr->e_ident[EI_CLASS] == ELFCLASS64)
		return elf64_parse_ld_segments(va_addr, md_size, pa_addr);
	else if (ehdr->e_ident[EI_CLASS] == ELFCLASS32)
		return elf32_parse_ld_segments(va_addr, md_size, pa_addr);
	else
		return -EINVAL;
}

static int elf64_parse_hash_n_meta_data(void *va_addr, char *alg_name, size_t digest_size,
					unsigned int scm_cmd_id, unsigned int sw_type)
{
	Elf64_Ehdr *ehdr = (Elf64_Ehdr *)(uintptr_t)va_addr;
	Elf64_Phdr *phdr = (Elf64_Phdr *)(uintptr_t)(va_addr + ehdr->e_phoff);
	u8 *elf_seg_buf;
	struct crypto_shash *alg;
	struct shash_desc *desc;
	u8 *shabuf;
	u8 lds_cnt = 0;
	size_t seg_size;
	char *ld_seg_hash_buf;
	u32 md_size = 0;
	u32 hash_buf_size;
	int ret = 0;
	void *metadata = NULL;
	int i;

	if ((!IS_ELF(*ehdr)) || ehdr->e_phnum < 3)
		return -EINVAL;

	alg = crypto_alloc_shash(alg_name, 0, 0);
	if (IS_ERR(alg))
		return PTR_ERR(alg);

	shabuf = kzalloc(digest_size, GFP_KERNEL);
	ld_seg_hash_buf = kzalloc((digest_size * ehdr->e_phnum - 2), GFP_KERNEL);
	desc = kzalloc(sizeof(*desc) + crypto_shash_descsize(alg), GFP_KERNEL);

	if (!shabuf || !desc || !ld_seg_hash_buf)
		goto out_free;

	desc->tfm = alg;

	for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
		if (phdr->p_type == PT_LOAD) {
		/* NULL segment before the first loadable segment is considered
		 * for metadata size calculation.
		 */
			if (!lds_cnt && !md_size) {
				phdr--;
				md_size = phdr->p_offset + phdr->p_filesz;
				phdr++;
			}
			elf_seg_buf = (u8 *)va_addr + phdr->p_offset;
			seg_size = phdr->p_filesz;
			if (phdr->p_filesz && phdr->p_memsz) {
				if (crypto_shash_digest(desc, elf_seg_buf,
							seg_size, shabuf) < 0)
					goto out_free;
				memcpy(ld_seg_hash_buf + (lds_cnt * digest_size), shabuf,
				       digest_size);
				memset(shabuf, 0, digest_size);
				lds_cnt++;
			}
		}
	}

	hash_buf_size = digest_size * lds_cnt;

	metadata = kzalloc(md_size, GFP_KERNEL);
	if (!metadata)
		goto out_hash_free;

	memcpy(metadata, va_addr, md_size);

	ret = qcom_sec_upgrade_auth_hash_n_metadata(scm_cmd_id, sw_type, metadata,
						    md_size, NULL, 0, ld_seg_hash_buf,
						    hash_buf_size);
	kfree(metadata);
out_hash_free:
	kfree(ld_seg_hash_buf);
out_free:
	crypto_free_shash(alg);
	kfree(desc);
	kfree(shabuf);
	return ret;
}

static int elf32_parse_hash_n_meta_data(void *va_addr, char *alg_name, size_t digest_size,
					unsigned int scm_cmd_id, unsigned int sw_type)
{
	Elf32_Ehdr *ehdr = (Elf32_Ehdr *)(uintptr_t)va_addr;
	Elf32_Phdr *phdr = (Elf32_Phdr *)(uintptr_t)(va_addr + ehdr->e_phoff);
	u8 *elf_seg_buf;
	struct crypto_shash *alg;
	struct shash_desc *desc;
	u8 *shabuf;
	u8 lds_cnt = 0;
	size_t seg_size;
	char *ld_seg_hash_buf;
	u32 md_size = 0;
	u32 hash_buf_size;
	int ret = 0;
	void *metadata = NULL;
	int i;

	if ((!IS_ELF(*ehdr)) || ehdr->e_phnum < 3)
		return -EINVAL;

	alg = crypto_alloc_shash(alg_name, 0, 0);
	if (IS_ERR(alg))
		return PTR_ERR(alg);

	shabuf = kzalloc(digest_size, GFP_KERNEL);
	ld_seg_hash_buf = kzalloc((digest_size * ehdr->e_phnum - 2), GFP_KERNEL);
	desc = kzalloc(sizeof(*desc) + crypto_shash_descsize(alg), GFP_KERNEL);

	if (!shabuf || !desc || !ld_seg_hash_buf)
		goto out_free;

	desc->tfm = alg;

	for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
		if (phdr->p_type == PT_LOAD) {
		/* NULL segment before the first loadable segment is considered
		 * for metadata size calculation.
		 */
			if (!lds_cnt && !md_size) {
				phdr--;
				md_size = phdr->p_offset + phdr->p_filesz;
				phdr++;
			}
			elf_seg_buf = (u8 *)va_addr + phdr->p_offset;
			seg_size = phdr->p_filesz;
			if (phdr->p_filesz && phdr->p_memsz) {
				if (crypto_shash_digest(desc, elf_seg_buf,
							seg_size, shabuf) < 0)
					goto out_free;
				memcpy(ld_seg_hash_buf + (lds_cnt * digest_size), shabuf,
				       digest_size);
				memset(shabuf, 0, digest_size);
				lds_cnt++;
			}
		}
	}

	hash_buf_size = digest_size * lds_cnt;

	metadata = kzalloc(md_size, GFP_KERNEL);
	if (!metadata)
		goto out_hash_free;

	memcpy(metadata, va_addr, md_size);

	ret = qcom_sec_upgrade_auth_hash_n_metadata(scm_cmd_id, sw_type, metadata,
						    md_size, NULL, 0, ld_seg_hash_buf,
						    hash_buf_size);
	kfree(metadata);
out_hash_free:
	kfree(ld_seg_hash_buf);
out_free:
	crypto_free_shash(alg);
	kfree(desc);
	kfree(shabuf);
	return ret;
}

static int parse_n_calculate_hash_n_meta_data(void *va_addr, char *alg_name,
					      size_t digest_size, unsigned int scm_cmd_id,
					      unsigned int sw_type)
{
	Elf32_Ehdr *ehdr = (Elf32_Ehdr *)va_addr;

	if (ehdr->e_ident[EI_CLASS] == ELFCLASS64)
		return elf64_parse_hash_n_meta_data(va_addr, alg_name,
						    digest_size, scm_cmd_id, sw_type);
	else if (ehdr->e_ident[EI_CLASS] == ELFCLASS32)
		return elf32_parse_hash_n_meta_data(va_addr, alg_name,
						    digest_size, scm_cmd_id, sw_type);
	else
		return -EINVAL;
}

static ssize_t
rootfs_auth_flag(struct device *dev, struct device_attribute *sec_attr, char *buf)
{
	ssize_t ret = 0;

	if (rootfs_auth_enable)
		ret = snprintf(buf, sizeof("Enabled"), "%s\n", "Enabled");
	else
		ret = snprintf(buf, sizeof("Disabled"), "%s\n", "Disabled");
	return ret;
}

static bool elf_has_nand_preamble_magic(void *va_addr)
{
	struct nand_codeword *xbl_nand = (struct nand_codeword *)va_addr;

	if (xbl_nand && xbl_nand->magic_num1 == SBL_MAGIC_NUM_1 &&
	    xbl_nand->magic_num2 == SBL_MAGIC_NUM_2 &&
	    xbl_nand->magic_num3 == SBL_MAGIC_NUM_3) {
		return true;
	}
	return false;
}

/*
 * XBL Nand elf image
 *   ----------------------
 *   | (MAGIC)            |
 *   | PREAMBLE (10K)     |
 *   |--------------------|
 *   |SBL_CONTENT(118K)   |
 *   |--------------------|
 *   |SBL_MAGIC (12bytes) |
 *   |--------------------|
 *   |SBL_CONTENT(118K)   |
 *   |--------------------|
 *   |SBL_MAGIC (12bytes) |
 *   ----------------------
 * Every 128K segment is followed by MAGIC cookie
 * Preamble & MAGIC cookie has to be removed before calculating the hash.
 */
static void remove_nand_preamble_n_magic_bytes(void **data, long *img_size)
{
	unsigned char *xbl_nand_data = (unsigned char *)(*data);
	size_t remaining_bytes;
	int offset;
	int blk_size;

	blk_size = NAND_BLOCK_SIZE - SBL_MAGIC_NUM_OFFSET;
	for (offset = 0; offset < *img_size; offset += blk_size) {
	/* calculate the number of bytes left in the image after the current blk */
		remaining_bytes = *img_size - offset;
		if (remaining_bytes > NAND_BLOCK_SIZE + SBL_MAGIC_NUM_OFFSET) {
			memmove(xbl_nand_data + offset + NAND_BLOCK_SIZE,
				xbl_nand_data + offset + NAND_BLOCK_SIZE +
				SBL_MAGIC_NUM_OFFSET, remaining_bytes -
				NAND_BLOCK_SIZE - SBL_MAGIC_NUM_OFFSET);
			*img_size = *img_size - SBL_MAGIC_NUM_OFFSET;
		}
	}
	*data += NAND_PREAMBLE_SIZE;
	*img_size -= NAND_PREAMBLE_SIZE;
}

static ssize_t
store_sec_auth(struct device *dev,
			struct device_attribute *sec_attr,
			const char *buf, size_t count)
{
	int ret;
	long size, sw_type;
	unsigned int img_addr = 0, img_size = 0, hash_size = 0;
	char *file_name, *sec_auth_str;
	char *sec_auth_token[QTI_SEC_AUTH_ARG_MAX] = {NULL};
	static void __iomem *file_buf;
	struct device_node *np;
	int idx;
	size_t data_size = 0;
	u32 scm_cmd_id;
	void *hash_file_buf = NULL, *data = NULL, *hash_data = NULL,
	*out_data = NULL, *metadata = NULL;
	struct elf_info info = {0};
	int ld_seg_cnt = 0;
	u32 md_size = 0; /* ELF Metadata size */
	u64 status = 0;
	char *alg_name;
	size_t digest_size;
	bool xbl_nand = false;
	struct device_node *node;
	struct reserved_mem *rmem = NULL;

	file_name = kzalloc(count+1, GFP_KERNEL);
	if (file_name == NULL)
		return -ENOMEM;

	sec_auth_str = file_name;
	strlcpy(file_name, buf, count+1);

	for (idx = 0; (idx < QTI_SEC_AUTH_ARG_MAX && file_name != NULL); idx++) {
                sec_auth_token[idx] = strsep(&file_name, " ");
        }

	pr_info("Authenticating Image %s\n", sec_auth_token[QTI_SEC_IMG_ADDR]);

	ret = kstrtol(sec_auth_token[QTI_SEC_IMG_SW_TYPE], 0, &sw_type);

	if (ret) {
		pr_err("sw_type str to long conversion failed\n");
		goto free_mem;
	}
	ret = kernel_read_file_from_path(sec_auth_token[QTI_SEC_IMG_ADDR],
			0, &data, INT_MAX, &data_size, READING_POLICY);

	if (ret < 0) {
		pr_err("%s file open failed\n", sec_auth_token[QTI_SEC_IMG_ADDR]);
		goto free_mem;
	}
	size = data_size;
	data_size = 0;

	if (data != NULL) {
		if (elf_has_nand_preamble_magic(data)) {
			xbl_nand = true;
			remove_nand_preamble_n_magic_bytes(&data, &size);
		}
		if(is_compressed(data, &info)) {
			out_data = kzalloc(info.memsize, GFP_KERNEL);
			if(!out_data) {
				pr_err("%s: Memory allocation failed for out_data buffer\n", __func__);
				goto free_data;
			}
			if(!img_decompress(data + info.offset, info.filesz, out_data, &info)) {
				printk("Uncompressed!\n");
				if(!IS_ELF(*((Elf32_Ehdr*) out_data))) {
					printk("Invalid uncompressed Image\n");
					goto free_out_data;
				} else {
					printk("uncompressed MBN extracted!\n");
					size = info.memsize;
				}
			} else {
				printk("Failed uncompress!\n");
				goto free_out_data;
			}
		}
	} else {
		pr_err("%s data is null\n",sec_auth_token[QTI_SEC_IMG_ADDR]);
		goto free_mem;
	}

	if (sec_auth_token[QTI_SEC_HASH_ADDR]) {
		ret = kernel_read_file_from_path(sec_auth_token[QTI_SEC_HASH_ADDR], 0, &hash_data,
						 INT_MAX, &data_size, READING_POLICY);
		if (ret < 0) {
			pr_err("%s File open failed\n", sec_auth_token[QTI_SEC_HASH_ADDR]);
			ret = -EINVAL;
			hash_data = NULL;
			goto free_data;
		}
		hash_size = data_size;
		hash_file_buf = kzalloc(hash_size, GFP_KERNEL);

		if (!hash_file_buf) {
			ret = -ENOMEM;
			vfree(hash_data);
			goto free_data;
		}
		if (hash_data) {
			memcpy(hash_file_buf, hash_data, hash_size);
			vfree(hash_data);
			hash_data = NULL;
		} else {
			pr_err("%s data is null\n", sec_auth_token[QTI_SEC_HASH_ADDR]);
			goto hash_buf_alloc_err;
		}
	}

	np = of_find_node_by_name(NULL, "qfprom");
	if (!np) {
		pr_err("Unable to find qfprom node\n");
		if (xbl_nand)
			data -= NAND_PREAMBLE_SIZE;
		goto hash_buf_alloc_err;
	}

	ret = of_property_read_u32(np, "img-size", &img_size);

	ret = of_property_read_u32(np, "img-addr", &img_addr);

	node = of_parse_phandle(np, "memory-region", 0);

	if (node) {
		rmem = of_reserved_mem_lookup(node);
		of_node_put(node);

		if (!rmem) {
			pr_err("Unable to acquire memory-region\n");
			goto free_np;
		}
		img_addr = rmem->base;
		img_size = rmem->size;
	}

	ret = of_property_read_u32(np, "scm-cmd-id", &scm_cmd_id);
	if (ret)
		scm_cmd_id = QCOM_KERNEL_AUTH_CMD;

	if (!img_addr && !img_size) {
		if (of_device_is_compatible(np, "qcom,qfprom-ipq5332-sec")) {
			alg_name = "sha256";
			digest_size = SHA256_DIGEST_SIZE;
		} else {
			alg_name = "sha384";
			digest_size = SHA384_DIGEST_SIZE;
		}
		scm_cmd_id = QCOM_KERNEL_HASH_N_META_AUTH_CMD;
		if (hash_file_buf) {
			md_size = size;
			metadata = kzalloc(md_size, GFP_KERNEL);
			if (!metadata)
				goto free_np;
			memcpy(metadata, data, md_size);
			ret = qcom_sec_upgrade_auth_hash_n_metadata(scm_cmd_id, sw_type,
					metadata, md_size, NULL,
					0, hash_file_buf, hash_size);
			kfree(metadata);
			if (ret) {
				pr_err("Rootfs_auth_hash_n_metadata failed with %d\n", ret);
				goto free_np;
			}
		} else if (out_data) {
			ret = parse_n_calculate_hash_n_meta_data(out_data, alg_name, digest_size,
								 scm_cmd_id, sw_type);
		} else {
			ret = parse_n_calculate_hash_n_meta_data(data, alg_name, digest_size,
								 scm_cmd_id, sw_type);
		}
		if (ret) {
			pr_err("Hash and Metadata authentication failed with %d\n", ret);
			goto free_np;
		}
		ret = count;
		goto free_np;
	}

	if (size > img_size) {
		pr_err("File size exceeds allocated memory region\n");
		ret = -EINVAL;
		goto free_np;
	}

	file_buf = ioremap(img_addr, img_size);
	if (file_buf == NULL) {
		ret = -ENOMEM;
		goto free_np;
	}

	memset_io(file_buf, 0x0, img_size);

	if (out_data)
		memcpy_toio(file_buf, out_data, size);
	else
		memcpy_toio(file_buf, data, size);

	if (hash_file_buf) {
		/*
		 * Metadata and hash contents are passed to TZ for rootfs auth
		 * for all the targets. Both signature and Hash verification is done
		 * by TZ for other targets whereas only Hash verification of rootfs
		 * is performed by TZ incase of IPQ54xx and signature verification is
		 * done by TME.
		 */
		ret = qcom_sec_upgrade_auth_meta_data(QCOM_KERNEL_META_AUTH_CMD,
						      sw_type, size, img_addr,
						      hash_file_buf, hash_size);
		if (ret) {
			pr_err("sec_upgrade_auth_meta_data failed with return=%d\n", ret);
			goto un_map;
		}

		/*
		 * Metadata is passed for signature verification of rootfs in IPQ54xx.
		 * Passing the loadable_seg_info as NULL and loadable segment count
		 * as zero for rootfs image as only the elf header is being passed.
		 */
		if (of_device_is_compatible(np, "qcom,qfprom-ipq5424-sec")) {
			ret = qcom_sec_upgrade_auth_ld_segments(scm_cmd_id, sw_type,
								img_addr, size,
								NULL, 0, &status);
			if (ret) {
				pr_err("sec_upgrade_auth_ld_segments failed with return=%d\n", ret);
				goto un_map;
			}
		}
	}
	else {
		/* Pass the file_buf containing the elf for loadable segment extraction */
		if (of_device_is_compatible(np, "qcom,qfprom-ipq5424-sec")) {
			ld_seg_cnt = parse_n_extract_ld_segment(file_buf, &md_size, img_addr);
			if (ld_seg_cnt > 0) {
				ret = qcom_sec_upgrade_auth_ld_segments(scm_cmd_id, sw_type,
									img_addr, md_size,
									ld_seg_buff, ld_seg_cnt,
									&status);
			} else {
				pr_err("Unable to parse the loadable segments: ret: %d\n",
				       ld_seg_cnt);
				ret = ld_seg_cnt;
				goto free_ld_buff;
			}
		} else {
			ret = qcom_sec_upgrade_auth(scm_cmd_id, sw_type, size, img_addr);
		}
		if (ret || status) {
			pr_err("sec_upgrade_authentication failed return=%d status: %llx\n",
			       ret, status);
			goto un_map;
		}
	}
	ret = count;

free_ld_buff:
	kfree(ld_seg_buff);
	ld_seg_buff = NULL;
un_map:
	iounmap(file_buf);
free_np:
	of_node_put(np);
hash_buf_alloc_err:
	kfree(hash_file_buf);
free_out_data:
	if(out_data)
		kfree(out_data);
free_data:
	if (xbl_nand)
		data -= NAND_PREAMBLE_SIZE;
	vfree(data);
free_mem:
	kfree(sec_auth_str);
	return ret;
}

static struct device_attribute sec_attr =
	__ATTR(sec_auth, 0644, NULL, store_sec_auth);

static struct device_attribute rootfs_attr =
	__ATTR(rootfs_auth, 0644, rootfs_auth_flag, NULL);

struct kobject *sec_kobj;

static ssize_t
store_sec_dat(struct device *dev, struct device_attribute *attr,
	      const char *buf, size_t count)
{
	int ret = count;
	loff_t size;
	u64 fuse_status = 0;
	struct file *fptr = NULL;
	struct kstat st;
	void *ptr = NULL;
	struct fuse_blow fuse_blow = {0};
	dma_addr_t dma_req_addr = 0;
	size_t req_order = 0;
	struct page *req_page = NULL;
	u64 dma_size;
	struct device_node *np;
	u8 ld_seg_cnt = 0;
	u32 scm_cmd_id;
	u32 md_size = 0; /* ELF Metadata size */

	fptr = filp_open(buf, O_RDONLY, 0);
	if (IS_ERR(fptr)) {
		pr_err("%s File open failed\n", buf);
		ret = -EBADF;
		goto out;
	}

	np = of_find_node_by_name(NULL, "qfprom");
	if (!np) {
		pr_err("Unable to find qfprom node\n");
		goto out;
	}

	ret = vfs_getattr(&fptr->f_path, &st, STATX_SIZE, AT_STATX_SYNC_AS_STAT);
	if (ret) {
		pr_err("Getting file attributes failed\n");
		goto file_close;
	}
	size = st.size;

	/* determine the allocation order of a memory size */
	req_order = get_order(size);

	/* allocate pages from the kernel page pool */
	req_page = alloc_pages(GFP_KERNEL, req_order);
	if (!req_page) {
		ret = -ENOMEM;
		goto file_close;
	} else {
		/* get the mapped virtual address of the page */
		ptr = page_address(req_page);
	}
	memset(ptr, 0, size);
	ret = kernel_read(fptr, ptr, size, 0);
	if (ret != size) {
		pr_err("File read failed\n");
		ret = ret < 0 ? ret : -EIO;
		goto free_page;
	}

	arch_setup_dma_ops(dev, 0, 0, NULL, 0);

	dev->coherent_dma_mask = DMA_BIT_MASK(32);
	INIT_LIST_HEAD(&dev->dma_pools);
	dma_coerce_mask_and_coherent(dev, DMA_BIT_MASK(32));
	dma_size = dev->coherent_dma_mask + 1;

	/* map the memory region */
	dma_req_addr = dma_map_single(dev, ptr, size, DMA_TO_DEVICE);
	ret = dma_mapping_error(dev, dma_req_addr);
	if (ret) {
		pr_err("DMA Mapping Error\n");
		dma_unmap_single(dev, dma_req_addr, size, DMA_TO_DEVICE);
		free_pages((unsigned long)page_address(req_page), req_order);
		goto file_close;
	}
	fuse_blow.address = dma_req_addr;
	fuse_blow.status = &fuse_status;

	ret = of_property_read_u32(np, "scm-cmd-id", &scm_cmd_id);
	if (ret) {
		pr_err("Error in reading scm id\n");
		goto free_mem;
	}

	if (!IS_ELF(*((Elf32_Ehdr *)ptr)) &&
	    qcom_sec_dat_fuse_available(TZ_BLOW_FUSE_SECDAT)) {
		ret = qcom_fuseipq_scm_call(QCOM_SCM_SVC_FUSE,
					    TZ_BLOW_FUSE_SECDAT,
					    &fuse_blow,
					    sizeof(fuse_blow));
	} else if (IS_ELF(*((Elf32_Ehdr *)ptr)) &&
		   qcom_sec_dat_fuse_available(QCOM_AUTH_FUSE_UIE_KEY_CMD)) {
		fuse_blow.size = size;
		ret = qcom_fuseipq_scm_call(QCOM_SCM_SVC_FUSE,
					    QCOM_AUTH_FUSE_UIE_KEY_CMD,
					    &fuse_blow,
					    sizeof(fuse_blow));
	} else if (IS_ELF(*((Elf32_Ehdr *)ptr)) &&
		   qcom_sec_upgrade_auth_ld_segments_available(scm_cmd_id)) {
		/* Pass the ptr containing the elf for loadable segment
		 * extraction
		 */
		ld_seg_cnt = parse_n_extract_ld_segment(ptr, &md_size,
							dma_req_addr);
		ret = qcom_sec_upgrade_auth_ld_segments(scm_cmd_id,
							SW_TYPE_SEC_DATA,
							dma_req_addr,
							md_size,
							ld_seg_buff,
							ld_seg_cnt,
							&fuse_status);
	} else {
		ret = -EINVAL;
		goto free_mem;
	}

	if (ret) {
		pr_err("Error in QFPROM write (%d)\n", ret);
		ret = -EIO;
		goto free_ld_buff;
	}
	if (fuse_status == FUSEPROV_INVALID_HASH)
		pr_info("Invalid sec.dat\n");
	else if (fuse_status == IMAGE_AUTH_FAILURE)
		pr_info("Image authentication failed\n");
	else if (fuse_status == FUSEPROV_SUCCESS)
		pr_info("Fuse Blow Success\n");
	else
		pr_info("Fuse blow failed with err code : 0x%llx\n", fuse_status);

	ret = count;

free_ld_buff:
	kfree(ld_seg_buff);
	ld_seg_buff = NULL;
free_mem:
	dma_unmap_single(dev, dma_req_addr, size, DMA_TO_DEVICE);
free_page:
	free_pages((unsigned long)page_address(req_page), req_order);
file_close:
	filp_close(fptr, NULL);
out:
	return ret;
}

static struct device_attribute sec_dat_attr =
	__ATTR(sec_dat, 0200, NULL, store_sec_dat);

static const struct qfprom_node_cfg ipq9574_qfprom_node_cfg = {
	.is_rlbk_support	=	true,
};

static const struct qfprom_node_cfg ipq5332_qfprom_node_cfg = {
	.is_rlbk_support	=	true,
};

static const struct qfprom_node_cfg ipq5424_qfprom_node_cfg = {
	.is_rlbk_support	=	false,
};

/*
 * Do not change the order of attributes.
 * New types should be added at the end
 */
#ifdef CONFIG_QCOM_TMELCOM
static struct device_attribute qfprom_attrs[] = {
	__ATTR(authenticate, 0444, qfprom_show_authenticate, NULL),
	__ATTR(tz_version, 0444, show_tz_version, NULL),
	__ATTR(appsbl_version, 0444, show_appsbl_version, NULL),
	__ATTR(hlos_version, 0444, show_hlos_version, NULL),
	__ATTR(devcfg_version, 0444, show_devcfg_version, NULL),
	__ATTR(tmel_version, 0444, show_tmel_version, NULL),
	__ATTR(xbl_sc_version, 0444, show_xbl_sc_version, NULL),
	__ATTR(xbl_cfg_version, 0444, show_xbl_cfg_version, NULL),
	__ATTR(atf_version, 0444, show_atf_version, NULL),
	__ATTR(q6_rootpd_version, 0444, show_q6_rootpd_version, NULL),
	__ATTR(userpd_wdt_version, 0444, show_userpd_wdt_version, NULL),
	__ATTR(userpd_oem_version, 0444, show_userpd_oem_version, NULL),
	__ATTR(iu_fw_version, 0444, show_iu_fw_version, NULL),
	__ATTR(read_version, 0200, NULL, store_read_commit_version),
	__ATTR(version_commit, 0200, NULL, store_version_commit),
};
#else
static struct device_attribute qfprom_attrs[] = {
	__ATTR(authenticate, 0444, qfprom_show_authenticate,
					NULL),
	__ATTR(sbl_version, 0644, show_sbl_version,
					store_sbl_version),
	__ATTR(tz_version, 0644, show_tz_version,
					store_tz_version),
	__ATTR(appsbl_version, 0644, show_appsbl_version,
					store_appsbl_version),
	__ATTR(hlos_version, 0644, show_hlos_version,
					store_hlos_version),
	__ATTR(rpm_version, 0644, show_rpm_version,
					store_rpm_version),
	__ATTR(devcfg_version, 0644, show_devcfg_version,
					store_devcfg_version),
	__ATTR(apdp_version, 0644, show_apdp_version,
					store_apdp_version),
	__ATTR(version_commit, 0200, NULL,
					store_version_commit),

};
#endif

static struct bus_type qfprom_subsys = {
	.name = "qfprom",
	.dev_name = "qfprom",
};

static struct device device_qfprom = {
	.id = 0,
	.bus = &qfprom_subsys,
};

static int __init qfprom_create_files(int size, int16_t sw_bitmap,
				      const struct qfprom_node_cfg *cfg)
{
	int i;
	int err;
	int sw_bit;
	/* authenticate sysfs entry is mandatory */
	err = device_create_file(&device_qfprom, &qfprom_attrs[0]);
	if (err) {
		pr_err("%s: device_create_file(%s)=%d\n",
			__func__, qfprom_attrs[0].attr.name, err);
		return err;
	}

	if (cfg->is_rlbk_support && gl_version_enable != 1)
		return 0;

	for (i = 1; i < size; i++) {
		if(strncmp(qfprom_attrs[i].attr.name, "version_commit",
			   strlen(qfprom_attrs[i].attr.name))) {
			/*
			* Following is the BitMap adapted:
			* SBL:0 TZ:1 APPSBL:2 HLOS:3 RPM:4. New types should
			* be added at the end of "qfprom_attrs" variable.
			*/

			if (cfg->is_rlbk_support) {
				sw_bit = i - 1;
				if (!(sw_bitmap & (1 << sw_bit)))
					break;
			}
			err = device_create_file(&device_qfprom, &qfprom_attrs[i]);
			if (err) {
				pr_err("%s: device_create_file(%s)=%d\n",
					__func__, qfprom_attrs[i].attr.name, err);
				return err;
			}
		} else if (version_commit_enable) {
			err = device_create_file(&device_qfprom, &qfprom_attrs[i]);
			if (err) {
				pr_err("%s: device_create_file(%s)=%d\n",
					__func__, qfprom_attrs[i].attr.name, err);
				return err;
			}
		}
	}

	/* setup the DMA framework for the device 'qfprom' */
	device_qfprom.coherent_dma_mask = DMA_BIT_MASK(32);
	INIT_LIST_HEAD(&device_qfprom.dma_pools);
	dma_coerce_mask_and_coherent(&device_qfprom, DMA_BIT_MASK(32));

	arch_setup_dma_ops(&device_qfprom, 0, 0, NULL, 0);

	return 0;
}

int is_version_rlbk_enabled(struct device *dev, int16_t *sw_bitmap)
{
	int ret;
	uint32_t *version_enable = kzalloc(sizeof(uint32_t), GFP_KERNEL);
	if (!version_enable)
		return -ENOMEM;

	ret = read_version(dev, SW_TYPE_DEFAULT, &version_enable);
	if (ret) {
		pr_err("\n Version Read Failed with error %d", ret);
		goto err_ver;
	}

	*sw_bitmap = ((*version_enable & 0xFFFF0000) >> 16);

	ret = (*version_enable & 0x1);

err_ver:
	kfree(version_enable);
	return ret;
}

static int qfprom_probe(struct platform_device *pdev)
{
	int err, ret;
	int16_t sw_bitmap = 0;
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	const struct qfprom_node_cfg *cfg;
	u32 scm_cmd_id, rootfs_auth_flg;
	struct resource *res;
	void __iomem *secure_boot = NULL;
	u32 value = 0;

	if (!qcom_scm_is_available()) {
		pr_info("SCM call is not initialized, defering probe\n");
		return -EPROBE_DEFER;
	}

	cfg = of_device_get_match_data(dev);
	if (!cfg)
		return -EINVAL;

	if (cfg->is_rlbk_support) {
		gl_version_enable = is_version_rlbk_enabled(&pdev->dev, &sw_bitmap);
		if (gl_version_enable == 0)
			pr_info("\nVersion Rollback Feature Disabled\n");
	}
	/*
	 * Registering under "/sys/devices/system"
	 */
	err = subsys_system_register(&qfprom_subsys, NULL);
	if (err) {
		pr_err("%s: subsys_system_register fail (%d)\n",
			__func__, err);
		return err;
	}

	err = device_register(&device_qfprom);
	if (err) {
		pr_err("Could not register device %s, err=%d\n",
			dev_name(&device_qfprom), err);
		put_device(&device_qfprom);
		return err;
	}

	/*
	 * Registering sec_auth under "/sys/sec_authenticate"
	   only if board is secured
	 */
	if (qcom_qfprom_show_auth_available())
		ret = qcom_qfprom_show_authenticate();
	else
		ret = ipq54xx_qcom_qfprom_show_authenticate();

	if (ret < 0)
		return ret;

	if (ret == 1) {

		err = of_property_read_u32(np, "scm-cmd-id", &scm_cmd_id);
		if (err)
			scm_cmd_id = QCOM_KERNEL_AUTH_CMD;

		/*
		 * Checking if secure sysupgrade scm_call is supported
		 */
		if (!qcom_scm_sec_auth_available(scm_cmd_id)) {
			pr_info("qcom_scm_sec_auth_available is not supported\n");
		} else {
			sec_kobj = kobject_create_and_add("sec_upgrade", NULL);
			if (!sec_kobj) {
				pr_info("Failed to register sec_upgrade sysfs\n");
				return -ENOMEM;
			}

			err = sysfs_create_file(sec_kobj, &sec_attr.attr);
			if (err) {
				pr_info("Failed to register sec_auth sysfs\n");
				kobject_put(sec_kobj);
				sec_kobj = NULL;
			}
		}

		if (sec_kobj) {
			err = sysfs_create_file(sec_kobj, &rootfs_attr.attr);
			if (err) {
				pr_info("Failed to register rootfs_auth sysfs\n");
				kobject_put(sec_kobj);
				sec_kobj = NULL;
			}
		}

		if (!err) {
			err = of_property_read_u32(np, "rootfs_auth_enable", &rootfs_auth_flg);
			if (err)
				rootfs_auth_flg = 0;

			res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
			secure_boot = devm_ioremap_resource(&pdev->dev, res);
			if (IS_ERR(secure_boot)) {
				pr_info("ioremap failed for rootfs auth fuse");
			} else {
				value = readl(secure_boot);

				/*
				 * Bit5 of the fuse <SECURE_BOOTn> or dts property rootfs_auth_enable,
				 * indicates if rootfs auth is enabled or not
				 */
				if (value & BIT(5) || rootfs_auth_flg)
					rootfs_auth_enable = 1;
			}
		}
	}

	of_property_read_u32(np, "version-commit-enable", &version_commit_enable);
	if (version_commit_enable)
		pr_info("version commit support enabled\n");

	/* sysfs entry for fusing QFPROM */
	err = device_create_file(&device_qfprom, &sec_dat_attr);
	if (err) {
		pr_err("%s: device_create_file(%s)=%d\n",
			__func__, sec_dat_attr.attr.name, err);
	}

	/* Error values are printed in qfprom_create_files API. Skipping the
	   return value check to proceed with creating the next sysfs entry */
	err = qfprom_create_files(ARRAY_SIZE(qfprom_attrs), sw_bitmap, cfg);
	if (err) {
		pr_err("%s: device_create_file with error %d\n",
			__func__, err);
	}
	return err;
}

static const struct of_device_id qcom_qfprom_dt_match[] = {
	{ .compatible = "qcom,qfprom-sec",},
	{
		.compatible = "qcom,qfprom-ipq9574-sec",
		.data = (void *)&ipq9574_qfprom_node_cfg,
	}, {
		.compatible = "qcom,qfprom-ipq5332-sec",
		.data = (void *)&ipq5332_qfprom_node_cfg,
	}, {
		.compatible = "qcom,qfprom-ipq5424-sec",
		.data = (void *)&ipq5424_qfprom_node_cfg,
	},
	{}
};

static struct platform_driver qcom_qfprom_driver = {
	.driver = {
		.name	= "qcom_qfprom",
		.of_match_table = qcom_qfprom_dt_match,
	},
	.probe = qfprom_probe,
};

module_platform_driver(qcom_qfprom_driver);
