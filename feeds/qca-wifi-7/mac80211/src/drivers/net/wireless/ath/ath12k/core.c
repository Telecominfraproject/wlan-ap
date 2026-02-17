// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/remoteproc.h>
#include <linux/panic_notifier.h>
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/pci.h>
#ifdef CONFIG_IO_COHERENCY
#include <linux/tmelcom_ipc.h>
#endif

#include "ahb.h"
#include "core.h"
#include "coredump.h"
#include "dp_tx.h"
#include "dp_rx.h"
#include "debug.h"
#include "debugfs.h"
#include "fw.h"
#include "hif.h"
#include "pci.h"
#include "pcic.h"
#include "wow.h"
#include "dp_cmn.h"
#include "dp.h"
#include "fse.h"
#include "accel_cfg.h"
#include "peer.h"
#include "qos.h"
#include "vendor.h"
#include "telemetry.h"
#include "ppe.h"
#include "cfr.h"
#include "ini.h"
#include "erp.h"
#include "sdwf.h"
#include "telemetry_agent_if.h"

#ifdef CPTCFG_ATHDEBUG
#include "athdbg_if.h"
#endif

#define ATH12K_NUM_POOL_PPEDS_TX_DESC_DEFAULT 0x8000
#define ATH12K_PPEDS_HOTLIST_LEN_MAX_DEFAULT 1024

struct ath12k_ppeds_desc_params ath12k_ppeds_desc_params = {
	.num_ppeds_desc = ATH12K_NUM_POOL_PPEDS_TX_DESC_DEFAULT,
	.ppeds_hotlist_len = ATH12K_PPEDS_HOTLIST_LEN_MAX_DEFAULT,
};

module_param_named(num_ppeds_tx_desc, ath12k_ppeds_desc_params.num_ppeds_desc, uint, 0644);
MODULE_PARM_DESC(num_ppeds_tx_desc, "Number of PPEDS descriptors");

module_param_named(ppeds_hotlist_len, ath12k_ppeds_desc_params.ppeds_hotlist_len, uint, 0644);
MODULE_PARM_DESC(ppeds_hotlist_len, "PPEDS hotlist length");

unsigned int ath12k_ppe_ds_enabled = true;
module_param_named(ppe_ds_enable, ath12k_ppe_ds_enabled, uint, 0644);
MODULE_PARM_DESC(ppe_ds_enable, "ppe_ds_enable: 0-disable, 1-enable");

#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
extern struct ath12k_ps_context ath12k_global_ps_ctx;
#endif

unsigned int ath12k_debug_mask;
module_param_named(debug_mask, ath12k_debug_mask, uint, 0644);
MODULE_PARM_DESC(debug_mask, "Debugging mask");
EXPORT_SYMBOL(ath12k_debug_mask);

unsigned int ath12k_debug_mask_level;
module_param_named(debug_level, ath12k_debug_mask_level, uint, 0644);
MODULE_PARM_DESC(debug_mask, "Debugging level");

unsigned int ath12k_mlo_capable = true;
module_param_named(mlo_capable, ath12k_mlo_capable, uint, 0644);
MODULE_PARM_DESC(mlo_capable, "mlo capable: 0-disable, 1-enable");

bool ath12k_ftm_mode;
module_param_named(ftm_mode, ath12k_ftm_mode, bool, 0444);
MODULE_PARM_DESC(ftm_mode, "Boots up in factory test mode");
EXPORT_SYMBOL(ath12k_ftm_mode);

unsigned int ath12k_frame_mode = ATH12K_HW_TXRX_ETHERNET;
module_param_named(frame_mode, ath12k_frame_mode, uint, 0644);
MODULE_PARM_DESC(frame_mode,
		 "Datapath frame mode (0: raw, 1: native wifi (default), 2: ethernet)");

bool ath12k_fse_3_tuple_enabled = true;
module_param_named(fse_3_tuple_enabled, ath12k_fse_3_tuple_enabled, bool, 0644);
MODULE_PARM_DESC(fse_3_tuple_enabled, "fse_3_tuple_enabled: 0-disable, 1-enable");

#ifdef CONFIG_IO_COHERENCY
bool ath12k_io_coherency_enabled = true;
module_param_named(io_coherency, ath12k_io_coherency_enabled, bool, 0644);
MODULE_PARM_DESC(io_coherency, "Enable io_coherency (0 - disable, 1 - enable)");
#endif

#ifndef CONFIG_ATH12K_MEM_PROFILE_512M
unsigned int ath12k_max_clients = 256;
module_param_named(max_clients, ath12k_max_clients, uint, 0644);
MODULE_PARM_DESC(max_clients, "Max clients support");
#endif

static unsigned int ath12k_en_fwlog = true;
module_param_named(en_fwlog, ath12k_en_fwlog, uint, 0644);
MODULE_PARM_DESC(en_fwlog, "fwlog: 0-disable, 1-enable");

unsigned int ath12k_ssr_failsafe_mode = true;
module_param_named(ssr_failsafe_mode, ath12k_ssr_failsafe_mode, uint, 0644);
MODULE_PARM_DESC(ssr_failsafe_mode, "ssr failsafe mode: 0-disable, 1-enable");

bool ath12k_rx_nwifi_err_dump = false;
module_param_named(rx_nwifi_err_dump, ath12k_rx_nwifi_err_dump, bool, 0644);
MODULE_PARM_DESC(rx_nwifi_err_dump, "rx nwifi err dump: 0-disable, 1-enable");
EXPORT_SYMBOL(ath12k_rx_nwifi_err_dump);

bool ath12k_ppe_rfs_support = true;
module_param_named(ppe_rfs_support, ath12k_ppe_rfs_support, bool, 0644);
MODULE_PARM_DESC(ppe_rfs_support, "Enable PPE RFS support for DL (0 - disable, 1 - enable)");

unsigned int ath12k_fw_mem_seg;
module_param_named(fw_mem_seg, ath12k_fw_mem_seg, uint, 0644);
MODULE_PARM_DESC(fw_mem_seg, "Enable/Disable FW segmented memory");

unsigned int ath12k_rfs_core_mask[4] = {ATH12K_MAX_CORE_MASK, ATH12K_MAX_CORE_MASK,
					ATH12K_MAX_CORE_MASK, ATH12K_MAX_CORE_MASK};
module_param_array_named(rfs_core_mask, ath12k_rfs_core_mask, int, NULL, 0644);
MODULE_PARM_DESC(rfs_core_mask, "Default RFS core mask, mask for 2G, mask for 5G,\n"
		 "mask for 6G. One bit for one CPU core\n");

bool ath12k_debug_critical = false;
module_param_named(debug_critical, ath12k_debug_critical, bool, 0644);
MODULE_PARM_DESC(debug_critical, "Debug critical issue (0 - disable, 1 - enable)");
EXPORT_SYMBOL(ath12k_debug_critical);

unsigned int ath12k_cfr_enable_bmap = 0;
module_param_named(cfr_enable_bmap, ath12k_cfr_enable_bmap, uint, 0644);
MODULE_PARM_DESC(cfr_enable_bmap, "cfr_enable_bmap: 0-disable, enable-(0x7 for 3 chipsets)");

bool ath12k_mlo_3_link_tx;
module_param_named(mlo_3_link_tx, ath12k_mlo_3_link_tx, bool, 0644);
MODULE_PARM_DESC(mlo_3_link_tx, "3 link MLO active TX support (0 - disable, 1 - enable)");

unsigned int ath12k_wsi_bypass_bmap;
module_param_named(wsi_bypass_bmap, ath12k_wsi_bypass_bmap, uint, 0644);
MODULE_PARM_DESC(wsi_bypass_bmap,
		 "Bitmap for the chip to be bypassed for WSI interface");

bool ath12k_carrier_vow_optimization;
module_param_named(carrier_vow_optimization, ath12k_carrier_vow_optimization, bool, 0644);
MODULE_PARM_DESC(carrier_vow_optimization, "Enable/Disable VoW optimization for carier usecases");
EXPORT_SYMBOL(ath12k_carrier_vow_optimization);

unsigned int ath12k_reorder_VI_timeout;
module_param_named(reorder_VI_timeout, ath12k_reorder_VI_timeout, uint, 0644);
MODULE_PARM_DESC(reorder_VI_timeout, "Reorder VI timeout (ms)");
EXPORT_SYMBOL(ath12k_reorder_VI_timeout);

/* protected with ath12k_hw_group_mutex */
static struct list_head ath12k_hw_group_list = LIST_HEAD_INIT(ath12k_hw_group_list);

static DEFINE_MUTEX(ath12k_hw_group_mutex);

extern struct ath12k_coredump_info ath12k_coredump_ram_info;

#ifdef CONFIG_IO_COHERENCY
int ath12k_core_config_iocoherency(struct ath12k_base *ab, bool enable)
{
	int ret, num_elem, idx = 0;
	struct tmel_secure_io secure_reg;

	if (!ath12k_io_coherency_enabled) {
		ath12k_err(ab, "io-coherency Disabled\n");
		return 0;
	}

	if (ab->in_coldboot_fwreset)
		return 0;

	num_elem = of_property_count_elems_of_size(ab->dev->of_node, "secure-reg",
						   sizeof(u32));
	if (num_elem < 0) {
		ath12k_err(ab, "secure-reg not configured for io-coherency\n");
		return 0;
	}

	while (idx < num_elem) {
		ret = of_property_read_u32_index(ab->dev->of_node, "secure-reg", idx++,
						 &secure_reg.reg_addr);
		if (ret) {
			ath12k_err(ab, "failed to get the secure reg addr %d\n", (idx - 1));
			goto err;
		}

		if (enable) {
			ret = of_property_read_u32_index(ab->dev->of_node, "secure-reg", idx++,
							 &secure_reg.reg_val);

			if (ret) {
				ath12k_err(ab, "failed to get the secure reg val %d\n", (idx - 1));
				goto err;
			}
		} else {
			secure_reg.reg_val = 0;
			idx++;
		}

		ath12k_info(ab, "Configuring secure reg: 0x%x val: 0x%x\n",
			    secure_reg.reg_addr, secure_reg.reg_val);

		ret = tmelcom_secure_io_write(&secure_reg, sizeof(struct tmel_secure_io));

		if (ret) {
			ath12k_err(ab, "Failed to update secure_reg settings, ret = %d reg: 0x%x val: 0x%x\n",
				   ret, secure_reg.reg_addr, secure_reg.reg_val);
			goto err;
		}
	}

err:
	return ret;
}
#endif

static int ath12k_core_rfkill_config(struct ath12k_base *ab)
{
	struct ath12k *ar;
	int ret = 0, i;

	if (!(ab->target_caps.sys_cap_info & WMI_SYS_CAP_INFO_RFKILL))
		return 0;

	if (ath12k_acpi_get_disable_rfkill(ab))
		return 0;

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;

		ret = ath12k_mac_rfkill_config(ar);
		if (ret && ret != -EOPNOTSUPP) {
			ath12k_warn(ab, "failed to configure rfkill: %d", ret);
			return ret;
		}
	}

	return ret;
}

/* Check if we need to continue with suspend/resume operation.
 * Return:
 *	a negative value: error happens and don't continue.
 *	0:  no error but don't continue.
 *	positive value: no error and do continue.
 */
static int ath12k_core_continue_suspend_resume(struct ath12k_base *ab)
{
	struct ath12k *ar;

	if (!ab->hw_params->supports_suspend)
		return -EOPNOTSUPP;

	/* so far single_pdev_only chips have supports_suspend as true
	 * so pass 0 as a dummy pdev_id here.
	 */
	ar = ab->pdevs[0].ar;
	if (!ar || !ar->ah || ar->ah->state != ATH12K_HW_STATE_OFF)
		return 0;

	return 1;
}

static unsigned int ath12k_crypto_mode;
module_param_named(crypto_mode, ath12k_crypto_mode, uint, 0644);
MODULE_PARM_DESC(crypto_mode, "crypto mode: 0-hardware, 1-software");

int ath12k_core_suspend(struct ath12k_base *ab)
{
	struct ath12k *ar;
	int ret, i;

	ret = ath12k_core_continue_suspend_resume(ab);
	if (ret <= 0)
		return ret;

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		if (!ar)
			continue;

		wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);

		ret = ath12k_mac_wait_tx_complete(ar);
		if (ret) {
			wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
			ath12k_warn(ab, "failed to wait tx complete: %d\n", ret);
			return ret;
		}

		wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	}

	/* PM framework skips suspend_late/resume_early callbacks
	 * if other devices report errors in their suspend callbacks.
	 * However ath12k_core_resume() would still be called because
	 * here we return success thus kernel put us on dpm_suspended_list.
	 * Since we won't go through a power down/up cycle, there is
	 * no chance to call complete(&ab->restart_completed) in
	 * ath12k_core_restart(), making ath12k_core_resume() timeout.
	 * So call it here to avoid this issue. This also works in case
	 * no error happens thus suspend_late/resume_early get called,
	 * because it will be reinitialized in ath12k_core_resume_early().
	 */
	complete(&ab->restart_completed);

	return 0;
}
EXPORT_SYMBOL(ath12k_core_suspend);

int ath12k_core_suspend_late(struct ath12k_base *ab)
{
	int ret;

	ret = ath12k_core_continue_suspend_resume(ab);
	if (ret <= 0)
		return ret;

	ath12k_acpi_stop(ab);

	ath12k_hif_irq_disable(ab);
	ath12k_hif_ce_irq_disable(ab);

	if (!ab->pm_suspend)
		ath12k_hif_power_down(ab, true);

	return 0;
}
EXPORT_SYMBOL(ath12k_core_suspend_late);

int ath12k_core_resume_early(struct ath12k_base *ab)
{
	int ret;

	ret = ath12k_core_continue_suspend_resume(ab);
	if (ret <= 0)
		return ret;

	reinit_completion(&ab->restart_completed);
	ret = ath12k_hif_power_up(ab);
	if (ret)
		ath12k_warn(ab, "failed to power up hif during resume: %d\n", ret);

	return ret;
}
EXPORT_SYMBOL(ath12k_core_resume_early);

int ath12k_core_resume(struct ath12k_base *ab)
{
	long time_left;
	int ret;

	ret = ath12k_core_continue_suspend_resume(ab);
	if (ret <= 0)
		return ret;

	time_left = wait_for_completion_timeout(&ab->restart_completed,
						ATH12K_RESET_TIMEOUT_HZ);
	if (time_left == 0) {
		ath12k_warn(ab, "timeout while waiting for restart complete");
		return -ETIMEDOUT;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_core_resume);

static int __ath12k_core_create_board_name(struct ath12k_base *ab, char *name,
					   size_t name_len, bool with_variant,
					   bool bus_type_mode, bool with_default)
{
	/* strlen(',variant=') + strlen(ab->qmi.target.bdf_ext) */
	char variant[9 + ATH12K_QMI_BDF_EXT_STR_LENGTH] = { 0 };

	if (with_variant && ab->qmi.target.bdf_ext[0] != '\0')
		scnprintf(variant, sizeof(variant), ",variant=%s",
			  ab->qmi.target.bdf_ext);

	switch (ab->id.bdf_search) {
	case ATH12K_BDF_SEARCH_BUS_AND_BOARD:
		if (bus_type_mode)
			scnprintf(name, name_len,
				  "bus=%s",
				  ath12k_bus_str(ab->hif.bus));
		else
			scnprintf(name, name_len,
				  "bus=%s,vendor=%04x,device=%04x,subsystem-vendor=%04x,subsystem-device=%04x,qmi-chip-id=%d,qmi-board-id=%d%s",
				  ath12k_bus_str(ab->hif.bus),
				  ab->id.vendor, ab->id.device,
				  ab->id.subsystem_vendor,
				  ab->id.subsystem_device,
				  ab->qmi.target.chip_id,
				  ab->qmi.target.board_id,
				  variant);
		break;
	default:
		scnprintf(name, name_len,
			  "bus=%s,qmi-chip-id=%d,qmi-board-id=%d%s",
			  ath12k_bus_str(ab->hif.bus),
			  ab->qmi.target.chip_id,
			  with_default ?
			  ATH12K_BOARD_ID_DEFAULT : ab->qmi.target.board_id,
			  variant);
		break;
	}

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "boot using board name '%s'\n", name);

	return 0;
}

static int ath12k_core_create_board_name(struct ath12k_base *ab, char *name,
					 size_t name_len)
{
	return __ath12k_core_create_board_name(ab, name, name_len, true, false, false);
}

static int ath12k_core_create_fallback_board_name(struct ath12k_base *ab, char *name,
						  size_t name_len)
{
	return __ath12k_core_create_board_name(ab, name, name_len, false, false, true);
}

static int ath12k_core_create_bus_type_board_name(struct ath12k_base *ab, char *name,
						  size_t name_len)
{
	return __ath12k_core_create_board_name(ab, name, name_len, false, true, true);
}

const struct firmware *ath12k_core_firmware_request(struct ath12k_base *ab,
						    const char *file)
{
	const struct firmware *fw;
	char path[100];
	int ret;

	if (!file)
		return ERR_PTR(-ENOENT);

	ath12k_core_create_firmware_path(ab, file, path, sizeof(path));

	ret = firmware_request_nowarn(&fw, path, ab->dev);
	if (ret)
		return ERR_PTR(ret);

	if (!fw || !fw->size)
		return ERR_PTR(-ENOENT);

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "boot firmware request %s size %zu\n",
		   path, fw->size);

	return fw;
}

void ath12k_core_free_bdf(struct ath12k_base *ab, struct ath12k_board_data *bd)
{
	if (!IS_ERR(bd->fw))
		release_firmware(bd->fw);

	memset(bd, 0, sizeof(*bd));
}

static int ath12k_core_parse_bd_ie_board(struct ath12k_base *ab,
					 struct ath12k_board_data *bd,
					 const void *buf, size_t buf_len,
					 const char *boardname,
					 int ie_id,
					 int name_id,
					 int data_id)
{
	const struct ath12k_fw_ie *hdr;
	bool name_match_found;
	int ret, board_ie_id;
	size_t board_ie_len;
	const void *board_ie_data;

	name_match_found = false;

	/* go through ATH12K_BD_IE_BOARD_/ATH12K_BD_IE_REGDB_ elements */
	while (buf_len > sizeof(struct ath12k_fw_ie)) {
		hdr = buf;
		board_ie_id = le32_to_cpu(hdr->id);
		board_ie_len = le32_to_cpu(hdr->len);
		board_ie_data = hdr->data;

		buf_len -= sizeof(*hdr);
		buf += sizeof(*hdr);

		if (buf_len < ALIGN(board_ie_len, 4)) {
			ath12k_err(ab, "invalid %s length: %zu < %zu\n",
				   ath12k_bd_ie_type_str(ie_id),
				   buf_len, ALIGN(board_ie_len, 4));
			ret = -EINVAL;
			goto out;
		}

		if (board_ie_id == name_id) {
			ath12k_dbg_dump(ab, ATH12K_DBG_BOOT, "board name", "",
					board_ie_data, board_ie_len);

			if (board_ie_len != strlen(boardname))
				goto next;

			ret = memcmp(board_ie_data, boardname, strlen(boardname));
			if (ret)
				goto next;

			name_match_found = true;
			ath12k_dbg(ab, ATH12K_DBG_BOOT,
				   "boot found match %s for name '%s'",
				   ath12k_bd_ie_type_str(ie_id),
				   boardname);
		} else if (board_ie_id == data_id) {
			if (!name_match_found)
				/* no match found */
				goto next;

			ath12k_dbg(ab, ATH12K_DBG_BOOT,
				   "boot found %s for '%s'",
				   ath12k_bd_ie_type_str(ie_id),
				   boardname);

			bd->data = board_ie_data;
			bd->len = board_ie_len;

			ret = 0;
			goto out;
		} else {
			ath12k_warn(ab, "unknown %s id found: %d\n",
				    ath12k_bd_ie_type_str(ie_id),
				    board_ie_id);
		}
next:
		/* jump over the padding */
		board_ie_len = ALIGN(board_ie_len, 4);

		buf_len -= board_ie_len;
		buf += board_ie_len;
	}

	/* no match found */
	ret = -ENOENT;

out:
	return ret;
}

static int ath12k_core_fetch_board_data_api_n(struct ath12k_base *ab,
					      struct ath12k_board_data *bd,
					      const char *boardname,
					      int ie_id_match,
					      int name_id,
					      int data_id)
{
	size_t len, magic_len;
	const u8 *data;
	char *filename, filepath[100];
	size_t ie_len;
	struct ath12k_fw_ie *hdr;
	int ret, ie_id;

	filename = ATH12K_BOARD_API2_FILE;

	if (!bd->fw)
		bd->fw = ath12k_core_firmware_request(ab, filename);

	if (IS_ERR(bd->fw))
		return PTR_ERR(bd->fw);

	data = bd->fw->data;
	len = bd->fw->size;

	ath12k_core_create_firmware_path(ab, filename,
					 filepath, sizeof(filepath));

	/* magic has extra null byte padded */
	magic_len = strlen(ATH12K_BOARD_MAGIC) + 1;
	if (len < magic_len) {
		ath12k_err(ab, "failed to find magic value in %s, file too short: %zu\n",
			   filepath, len);
		ret = -EINVAL;
		goto err;
	}

	if (memcmp(data, ATH12K_BOARD_MAGIC, magic_len)) {
		ath12k_err(ab, "found invalid board magic\n");
		ret = -EINVAL;
		goto err;
	}

	/* magic is padded to 4 bytes */
	magic_len = ALIGN(magic_len, 4);
	if (len < magic_len) {
		ath12k_err(ab, "failed: %s too small to contain board data, len: %zu\n",
			   filepath, len);
		ret = -EINVAL;
		goto err;
	}

	data += magic_len;
	len -= magic_len;

	while (len > sizeof(struct ath12k_fw_ie)) {
		hdr = (struct ath12k_fw_ie *)data;
		ie_id = le32_to_cpu(hdr->id);
		ie_len = le32_to_cpu(hdr->len);

		len -= sizeof(*hdr);
		data = hdr->data;

		if (len < ALIGN(ie_len, 4)) {
			ath12k_err(ab, "invalid length for board ie_id %d ie_len %zu len %zu\n",
				   ie_id, ie_len, len);
			ret = -EINVAL;
			goto err;
		}

		if (ie_id == ie_id_match) {
			ret = ath12k_core_parse_bd_ie_board(ab, bd, data,
							    ie_len,
							    boardname,
							    ie_id_match,
							    name_id,
							    data_id);
			if (ret == -ENOENT)
				/* no match found, continue */
				goto next;
			else if (ret)
				/* there was an error, bail out */
				goto err;
			/* either found or error, so stop searching */
			goto out;
		}
next:
		/* jump over the padding */
		ie_len = ALIGN(ie_len, 4);

		len -= ie_len;
		data += ie_len;
	}

out:
	if (!bd->data || !bd->len) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "failed to fetch %s for %s from %s\n",
			   ath12k_bd_ie_type_str(ie_id_match),
			   boardname, filepath);
		ret = -ENODATA;
		goto err;
	}

	return 0;

err:
	ath12k_core_free_bdf(ab, bd);
	return ret;
}

int ath12k_core_fetch_board_data_api_1(struct ath12k_base *ab,
				       struct ath12k_board_data *bd,
				       char *filename)
{
	bd->fw = ath12k_core_firmware_request(ab, filename);
	if (IS_ERR(bd->fw))
		return PTR_ERR(bd->fw);

	bd->data = bd->fw->data;
	bd->len = bd->fw->size;

	return 0;
}

#define BOARD_NAME_SIZE 200
int ath12k_core_fetch_bdf(struct ath12k_base *ab, struct ath12k_board_data *bd)
{
	char boardname[BOARD_NAME_SIZE], fallback_boardname[BOARD_NAME_SIZE];
	char *filename, filepath[100];
	int bd_api;
	int ret;

	filename = ATH12K_BOARD_API2_FILE;

	ret = ath12k_core_create_board_name(ab, boardname, sizeof(boardname));
	if (ret) {
		ath12k_err(ab, "failed to create board name: %d", ret);
		return ret;
	}

	bd_api = 2;
	ret = ath12k_core_fetch_board_data_api_n(ab, bd, boardname,
						 ATH12K_BD_IE_BOARD,
						 ATH12K_BD_IE_BOARD_NAME,
						 ATH12K_BD_IE_BOARD_DATA);
	if (!ret)
		goto success;

	ret = ath12k_core_create_fallback_board_name(ab, fallback_boardname,
						     sizeof(fallback_boardname));
	if (ret) {
		ath12k_err(ab, "failed to create fallback board name: %d", ret);
		return ret;
	}

	ret = ath12k_core_fetch_board_data_api_n(ab, bd, fallback_boardname,
						 ATH12K_BD_IE_BOARD,
						 ATH12K_BD_IE_BOARD_NAME,
						 ATH12K_BD_IE_BOARD_DATA);
	if (!ret)
		goto success;

	bd_api = 1;
	ret = ath12k_core_fetch_board_data_api_1(ab, bd, ATH12K_DEFAULT_BOARD_FILE);
	if (ret) {
		ath12k_core_create_firmware_path(ab, filename,
						 filepath, sizeof(filepath));
		ath12k_err(ab, "failed to fetch board data for %s from %s\n",
			   boardname, filepath);
		if (memcmp(boardname, fallback_boardname, strlen(boardname)))
			ath12k_err(ab, "failed to fetch board data for %s from %s\n",
				   fallback_boardname, filepath);

		ath12k_err(ab, "failed to fetch board.bin from %s\n",
			   ab->hw_params->fw.dir);
		return ret;
	}

success:
	ath12k_dbg(ab, ATH12K_DBG_BOOT, "using board api %d\n", bd_api);
	return 0;
}

int ath12k_core_fetch_fw_cfg(struct ath12k_base *ab,
			     struct ath12k_board_data *bd)
{
	int ret;

	ret = ath12k_core_fetch_board_data_api_1(ab, bd, ATH12K_FW_CFG_FILE);
	if (ret) {
		ath12k_dbg(ab, ATH12K_DBG_QMI, "failed to fetch %s from %s\n",
			   ATH12K_FW_CFG_FILE, ab->hw_params->fw.dir);
		return -ENOENT;
	}

	ath12k_info(ab, "fetching %s from %s\n", ATH12K_FW_CFG_FILE,
		    ab->hw_params->fw.dir);

	return 0;
}

int ath12k_core_fetch_rxgainlut(struct ath12k_base *ab, struct ath12k_board_data *bd)
{
	char rxgainlutname[BOARD_NAME_SIZE] = {};
	char rxgainlutdefaultname[BOARD_NAME_SIZE] = {};
	int ret;

	ret = ath12k_core_create_board_name(ab, rxgainlutname,
					    BOARD_NAME_SIZE);
	if (ret) {
		ath12k_err(ab, "failed to create rxgainlut name: %d", ret);
	return ret;
	}

	ret = ath12k_core_fetch_board_data_api_n(ab, bd, rxgainlutname,
						 ATH12K_BD_IE_RXGAINLUT,
						 ATH12K_BD_IE_RXGAINLUT_NAME,
						 ATH12K_BD_IE_RXGAINLUT_DATA);
	if (!ret)
		goto exit;
	
	ret = ath12k_core_create_fallback_board_name(ab, rxgainlutdefaultname,
					    	     BOARD_NAME_SIZE);
	if (ret) {
		ath12k_err(ab, "failed to create rxgainlut name: %d", ret);
	return ret;
	}

	ret = ath12k_core_fetch_board_data_api_n(ab, bd, rxgainlutdefaultname,
						 ATH12K_BD_IE_RXGAINLUT,
						 ATH12K_BD_IE_RXGAINLUT_NAME,
						 ATH12K_BD_IE_RXGAINLUT_DATA);
	if (!ret)
		goto exit;

	snprintf(rxgainlutname, sizeof(rxgainlutname), "%s%04x",
		 ATH12K_RXGAINLUT_FILE_PREFIX, ab->qmi.target.board_id);

	ret = ath12k_core_fetch_board_data_api_1(ab, bd, rxgainlutname);
	if (ret) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "failed to fetch %s from %s\n",
			   rxgainlutname, ab->hw_params->fw.dir);

		ret = ath12k_core_fetch_board_data_api_1(ab, bd,
							 ATH12K_RXGAINLUT_FILE);
		if (ret) {
			ath12k_warn(ab, "failed to fetch default %s from %s\n",
				    ATH12K_RXGAINLUT_FILE, ab->hw_params->fw.dir);
			return -ENOENT;
		}
	}

exit:
	ath12k_dbg(ab, ATH12K_DBG_BOOT, "fetche rxgainlut");
	return 0;
}

int ath12k_core_fetch_regdb(struct ath12k_base *ab, struct ath12k_board_data *bd)
{
	char boardname[BOARD_NAME_SIZE], default_boardname[BOARD_NAME_SIZE];
	int ret;

	ret = ath12k_core_create_board_name(ab, boardname, BOARD_NAME_SIZE);
	if (ret) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "failed to create board name for regdb: %d", ret);
		goto exit;
	}

	ret = ath12k_core_fetch_board_data_api_n(ab, bd, boardname,
						 ATH12K_BD_IE_REGDB,
						 ATH12K_BD_IE_REGDB_NAME,
						 ATH12K_BD_IE_REGDB_DATA);
	if (!ret)
		goto exit;

	ret = ath12k_core_create_bus_type_board_name(ab, default_boardname,
						     BOARD_NAME_SIZE);
	if (ret) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "failed to create default board name for regdb: %d", ret);
		goto exit;
	}

	ret = ath12k_core_fetch_board_data_api_n(ab, bd, default_boardname,
						 ATH12K_BD_IE_REGDB,
						 ATH12K_BD_IE_REGDB_NAME,
						 ATH12K_BD_IE_REGDB_DATA);
	if (!ret)
		goto exit;

	ret = ath12k_core_fetch_board_data_api_1(ab, bd, ATH12K_REGDB_FILE_NAME);
	if (ret)
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "failed to fetch %s from %s\n",
			   ATH12K_REGDB_FILE_NAME, ab->hw_params->fw.dir);

exit:
	if (!ret)
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "fetched regdb\n");

	return ret;
}

u32 ath12k_core_get_max_station_per_radio(struct ath12k_base *ab)
{
	if (ab->num_radios == 2)
		return TARGET_NUM_STATIONS_DBS;
	else if (ab->num_radios == 3)
		return TARGET_NUM_PEERS_PDEV_DBS_SBS;
	return TARGET_NUM_STATIONS_SINGLE;
}

u32 ath12k_core_get_max_peers_per_radio(struct ath12k_base *ab)
{
	if (ab->num_radios == 2)
		return TARGET_NUM_PEERS_PDEV_DBS;
	else if (ab->num_radios == 3)
		return TARGET_NUM_PEERS_PDEV_DBS_SBS;
	return TARGET_NUM_PEERS_PDEV_SINGLE;
}
EXPORT_SYMBOL(ath12k_core_get_max_peers_per_radio);

u32 ath12k_core_get_max_num_tids(struct ath12k_base *ab)
{
	if (ab->num_radios == 2)
		return TARGET_NUM_TIDS(DBS);
	else if (ab->num_radios == 3)
		return TARGET_NUM_TIDS(DBS_SBS);
	return TARGET_NUM_TIDS(SINGLE);
}
EXPORT_SYMBOL(ath12k_core_get_max_num_tids);

struct reserved_mem *ath12k_core_get_reserved_mem_by_name(struct ath12k_base *ab,
							 const char *name)
{
	struct device *dev = ab->dev;
	struct reserved_mem *rmem;
	struct device_node *node;
	int index;

	index = of_property_match_string(dev->of_node, "memory-region-names", name);
	if (index < 0) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "memory region %s not found\n", name);
		return NULL;
	}

	node = of_parse_phandle(dev->of_node, "memory-region", index);
	if (!node) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "failed to parse memory region %s\n", name);
		return NULL;
	}

	rmem = of_reserved_mem_lookup(node);
	of_node_put(node);
	if (!rmem) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "unable to get memory-region for index %d\n", index);
		return NULL;
	}

	return rmem;
}

static inline
void ath12k_core_to_group_ref_get(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;

	lockdep_assert_held(&ag->mutex);

	if (ab->hw_group_ref) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "core already attached to group %d\n",
			   ag->id);
		return;
	}

	ab->hw_group_ref = true;
	ag->num_started++;

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "core attached to group %d, num_started %d\n",
		   ag->id, ag->num_started);
}

static inline
void ath12k_core_to_group_ref_put(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;

	lockdep_assert_held(&ag->mutex);

	if (!ab->hw_group_ref) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "core already de-attached from group %d\n",
			   ag->id);
		return;
	}

	ab->hw_group_ref = false;
	ag->num_started--;

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "core de-attached from group %d, num_started %d\n",
		   ag->id, ag->num_started);
}

int ath12k_core_power_up(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab;
	unsigned long time_left;
	struct ath12k *ar;
	bool radio_suspended = false;
	int i, j;

	for (i = 0; i < ag->num_probed; i++) {
		ab =  ag->ab[i];
		if (ab->pm_suspend) {
			ath12k_hif_power_up(ab);
			ab->pm_suspend = false;
			ab->powerup_triggered = true;
			ath12k_info(ab, "Q6 power up is started\n");
			reinit_completion(&ab->power_up);
		}
	}

	for (i = 0; i < ag->num_probed; i++) {
		ab =  ag->ab[i];

		if (!ab->powerup_triggered && ab->num_radios > 1) {
			for (j = 0; j < ab->num_radios; j++) {
				ar = ab->pdevs[j].ar;
				if (ar && ar->pdev_suspend) {
					radio_suspended = true;
					break;
				}
			}
		}

		if (ab->powerup_triggered || radio_suspended) {
			time_left = wait_for_completion_timeout(&ab->power_up,
								ATH12K_Q6_POWER_UP_TIMEOUT);
			if (!time_left) {
				ath12k_err(ab, "Q6 power up wait timed out\n");
				return -ETIMEDOUT;
			}
		}
	}

	return 0;
}

void ath12k_core_cleanup_power_down_q6(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab;
	struct ath12k_hw *ah;
	struct ath12k *ar;
	unsigned long time_left;
	int i, j, ret;
	bool skip_power_down;

	reinit_completion(&ag->umac_reset_complete);
	for (i = 0; i < ag->num_hw; i++) {
		ah = ag->ah[i];
		if (!ah)
			continue;

		ret = ath12k_mac_mlo_standby_teardown(ah);
		if (ret) {
			ath12k_err(NULL, "mlo teardown is failed for ERP\n");
			return;
		}
	}

	time_left = wait_for_completion_timeout(&ag->umac_reset_complete,
				msecs_to_jiffies(ATH12K_UMAC_RESET_TIMEOUT_IN_MS));
	if (!time_left) {
		ath12k_err(NULL, "UMAC reset didn't get completed within %d ms\n",
			    ATH12K_UMAC_RESET_TIMEOUT_IN_MS);
		ag->trigger_umac_reset = false;
		return;
	}

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		skip_power_down = false;

		for (j = 0; j < ab->num_radios; j++) {
			ar = ab->pdevs[j].ar;

			if (ar) {
				if (ar->allocated_vdev_map)
					skip_power_down = true;
				else
					ath12k_mac_stop(ar);
			}
		}

		if (!skip_power_down && !ab->pm_suspend) {
			ab->qmi.num_radios = U8_MAX;
			ath12k_hif_irq_disable(ab);
			ath12k_hif_ce_irq_disable(ab);
			ath12k_dp_ppeds_interrupt_stop(ab);
			ath12k_qmi_firmware_stop(ab);
			ath12k_hif_power_down(ab, false);
			ath12k_core_to_group_ref_put(ab);
			ath12k_qmi_free_resource(ab);
			ab->pm_suspend = true;
			ath12k_info(ab, "Q6 power down\n");
		}
	}

	if (!test_bit(ATH12K_GROUP_FLAG_HIF_POWER_DOWN, &ag->flags))
		set_bit(ATH12K_GROUP_FLAG_HIF_POWER_DOWN, &ag->flags);
}

static void ath12k_core_stop(struct ath12k_base *ab)
{
	ath12k_core_to_group_ref_put(ab);
	ath12k_acpi_stop(ab);
	ath12k_hif_stop(ab);
	ath12k_wmi_detach(ab);
	ath12k_dp_cmn_device_deinit(ab->dp);
	ath12k_cfg_deinit(ab);

	/* De-Init of components as needed */
}

static void ath12k_core_check_cc_code_bdfext(const struct dmi_header *hdr, void *data)
{
	struct ath12k_base *ab = data;
	const char *magic = ATH12K_SMBIOS_BDF_EXT_MAGIC;
	struct ath12k_smbios_bdf *smbios = (struct ath12k_smbios_bdf *)hdr;
	ssize_t copied;
	size_t len;
	int i;

	if (ab->qmi.target.bdf_ext[0] != '\0')
		return;

	if (hdr->type != ATH12K_SMBIOS_BDF_EXT_TYPE)
		return;

	if (hdr->length != ATH12K_SMBIOS_BDF_EXT_LENGTH) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "wrong smbios bdf ext type length (%d).\n",
			   hdr->length);
		return;
	}

	spin_lock_bh(&ab->base_lock);

	switch (smbios->country_code_flag) {
	case ATH12K_SMBIOS_CC_ISO:
		ab->new_alpha2[0] = u16_get_bits(smbios->cc_code, 0xff);
		ab->new_alpha2[1] = u16_get_bits(smbios->cc_code, 0xff);
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "boot smbios cc_code %c%c\n",
			   ab->new_alpha2[0], ab->new_alpha2[1]);
		break;
	case ATH12K_SMBIOS_CC_WW:
		ab->new_alpha2[0] = '0';
		ab->new_alpha2[1] = '0';
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "boot smbios worldwide regdomain\n");
		break;
	default:
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "boot ignore smbios country code setting %d\n",
			   smbios->country_code_flag);
		break;
	}

	spin_unlock_bh(&ab->base_lock);

	if (!smbios->bdf_enabled) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "bdf variant name not found.\n");
		return;
	}

	/* Only one string exists (per spec) */
	if (memcmp(smbios->bdf_ext, magic, strlen(magic)) != 0) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "bdf variant magic does not match.\n");
		return;
	}

	len = min_t(size_t,
		    strlen(smbios->bdf_ext), sizeof(ab->qmi.target.bdf_ext));
	for (i = 0; i < len; i++) {
		if (!isascii(smbios->bdf_ext[i]) || !isprint(smbios->bdf_ext[i])) {
			ath12k_dbg(ab, ATH12K_DBG_BOOT,
				   "bdf variant name contains non ascii chars.\n");
			return;
		}
	}

	/* Copy extension name without magic prefix */
	copied = strscpy(ab->qmi.target.bdf_ext, smbios->bdf_ext + strlen(magic),
			 sizeof(ab->qmi.target.bdf_ext));
	if (copied < 0) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "bdf variant string is longer than the buffer can accommodate\n");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_BOOT,
		   "found and validated bdf variant smbios_type 0x%x bdf %s\n",
		   ATH12K_SMBIOS_BDF_EXT_TYPE, ab->qmi.target.bdf_ext);
}

int ath12k_core_check_smbios(struct ath12k_base *ab)
{
	ab->qmi.target.bdf_ext[0] = '\0';
	dmi_walk(ath12k_core_check_cc_code_bdfext, ab);

	if (ab->qmi.target.bdf_ext[0] == '\0')
		return -ENODATA;

	return 0;
}

static int ath12k_core_soc_create(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	int ret;

	if (ath12k_ftm_mode) {
		ab->fw_mode = ATH12K_FIRMWARE_MODE_FTM;
		ath12k_info(ab, "Booting in ftm mode\n");
	}

	ret = ath12k_qmi_init_service(ab);
	if (ret) {
		ath12k_err(ab, "failed to initialize qmi :%d\n", ret);
		return ret;
	}

	ath12k_debugfs_soc_create(ab);

	ret = ath12k_hif_power_up(ab);
	if (ret) {
		ath12k_err(ab, "failed to power up :%d\n", ret);
		goto err_qmi_deinit;
	}

	return 0;

err_qmi_deinit:
	ath12k_debugfs_soc_destroy(ab);
	mutex_unlock(&ag->mutex);
	ath12k_qmi_deinit_service(ab);
	mutex_lock(&ag->mutex);
	return ret;
}

static void ath12k_core_soc_destroy(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;

	if (!test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags) && !ab->is_bypassed)
		ath12k_qmi_firmware_stop(ab);

	if (ab->ce_pipe_init_done && !ab->is_bypassed)
		ath12k_ce_cleanup_pipes(ab);

	if (!ab->pm_suspend)
		ath12k_hif_power_down(ab, false);

	ath12k_reg_free(ab);
	ath12k_debugfs_soc_destroy(ab);
	mutex_unlock(&ag->mutex);
	ath12k_qmi_deinit_service(ab);
	mutex_lock(&ag->mutex);
}

static int ath12k_core_mlo_shmem_per_device_crash_info_addresses(
		struct ath12k_base *ab,
		struct ath12k_host_mlo_glb_device_crash_info *global_device_crash_info)
{
	int i;
	struct ath12k_host_mlo_glb_per_device_crash_info *per_device_crash_info = NULL;

	for (i = 0; i < global_device_crash_info->no_of_devices; i++)
	{
		per_device_crash_info = &global_device_crash_info->per_device_crash_info[i];

		if (!per_device_crash_info)
			return -EINVAL;

		if (ab->device_id == per_device_crash_info->device_id)
			break;
	}

	if (i >= global_device_crash_info->no_of_devices) {
		ath12k_err(ab, "error in chip id:%d\n", ab->device_id);
		return 0;
	}

	if (!per_device_crash_info ||
	    !per_device_crash_info->crash_reason ||
	    !per_device_crash_info->recovery_mode) {
		ath12k_err(ab, "crash_reason address is null\n");
		return 0;
	}

	ab->crash_info_address = per_device_crash_info->crash_reason;
	ab->recovery_mode_address = per_device_crash_info->recovery_mode;

	return 0;
}

static int ath12k_core_mlo_shmem_crash_info_init(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_host_mlo_mem_arena *mlomem_arena_ctx;
	struct ath12k_host_mlo_glb_device_crash_info *global_device_crash_info;

	mlomem_arena_ctx = &ab->ag->mlomem_arena;

	if (!(ag->mlo_mem.is_mlo_mem_avail))
		return 0;

	global_device_crash_info = &mlomem_arena_ctx->global_device_crash_info;

	if (ath12k_core_mlo_shmem_per_device_crash_info_addresses(ab,
				global_device_crash_info) < 0) {
		ath12k_warn(ab, "per_device_crash_info is not set\n");
		return -EINVAL;
	}

	return 0;
}

static int ath12k_core_pdev_init(struct ath12k_base *ab)
{
	ath12k_fse_init(ab);
	ath12k_telemetry_init(ab);
	ath12k_dp_accel_cfg_init(ab);
	ath12k_thermal_register(ab);
	ath12k_spectral_init(ab);
	/* Check if cfr_enable_bmap is set for the corresponding HW */
	if (ath12k_cfr_enable_bmap & (1 << ab->device_id)) {
		ath12k_info(ab, "Enabling CFR for chip id:%d\n", ab->device_id);
		ath12k_cfr_init(ab);
	}

	return 0;
}

static void ath12k_core_pdev_deinit(struct ath12k_base *ab)
{
	ath12k_dp_accel_cfg_deinit(ab);
	ath12k_fse_deinit(ab);
	ath12k_telemetry_deinit(ab);
	ath12k_thermal_unregister(ab);
	ath12k_spectral_deinit(ab);
	if (ath12k_cfr_enable_bmap & (1 << ab->device_id))
		ath12k_cfr_deinit(ab);
}

static int ath12k_core_pdev_create(struct ath12k_base *ab)
{
	int ret;

	ret = ath12k_dp_arch_pdev_alloc(ab->dp);
	if (ret) {
		ath12k_err(ab, "failed to attach DP pdev: %d\n", ret);
		goto err_pdev_debug;
	}

	return 0;

err_pdev_debug:
	ath12k_debugfs_pdev_destroy(ab);
	return ret;
}

static void ath12k_core_pdev_destroy(struct ath12k_base *ab)
{
	ath12k_dp_arch_pdev_free(ab->dp);
	ath12k_debugfs_pdev_destroy(ab);
}

static int ath12k_core_start(struct ath12k_base *ab)
{
	int ret;

	lockdep_assert_held(&ab->core_lock);

	ret = ath12k_wmi_attach(ab);
	if (ret) {
		ath12k_err(ab, "failed to attach wmi: %d\n", ret);
		return ret;
	}

	ret = ath12k_htc_init(ab);
	if (ret) {
		ath12k_err(ab, "failed to init htc: %d\n", ret);
		goto err_wmi_detach;
	}

	ret = ath12k_hif_start(ab);
	if (ret) {
		ath12k_err(ab, "failed to start HIF: %d\n", ret);
		goto err_wmi_detach;
	}

	ret = ath12k_htc_wait_target(&ab->htc);
	if (ret) {
		ath12k_err(ab, "failed to connect to HTC: %d\n", ret);
		goto err_hif_stop;
	}

	ret = ath12k_dp_htt_connect(ath12k_ab_to_dp(ab));
	if (ret) {
		ath12k_err(ab, "failed to connect to HTT: %d\n", ret);
		goto err_hif_stop;
	}
	ret = ath12k_wmi_connect(ab);
	if (ret) {
		ath12k_err(ab, "failed to connect wmi: %d\n", ret);
		goto err_hif_stop;
	}
	ret = ath12k_htc_start(&ab->htc);
	if (ret) {
		ath12k_err(ab, "failed to start HTC: %d\n", ret);
		goto err_hif_stop;
	}

	ret = ath12k_wmi_wait_for_service_ready(ab);
	if (ret) {
		ath12k_err(ab, "failed to receive wmi service ready event: %d\n",
			   ret);
		goto err_hif_stop;
	}

	ath12k_hal_cc_config(ab);

	ret = ath12k_wmi_cmd_init(ab);
	if (ret) {
		ath12k_err(ab, "failed to send wmi init cmd: %d\n", ret);
		goto err_hif_stop;
	}

	ret = ath12k_wmi_wait_for_unified_ready(ab);
	if (ret) {
		ath12k_err(ab, "failed to receive wmi unified ready event: %d\n",
			   ret);
		goto err_hif_stop;
	}

	WARN_ON(test_bit(ATH12K_FLAG_WMI_INIT_DONE, &ab->dev_flags));
	set_bit(ATH12K_FLAG_WMI_INIT_DONE, &ab->dev_flags);

	/* put hardware to DBS mode */
	if (ab->hw_params->single_pdev_only) {
		ret = ath12k_wmi_set_hw_mode(ab, WMI_HOST_HW_MODE_DBS);
		if (ret) {
			ath12k_err(ab, "failed to send dbs mode: %d\n", ret);
			goto err_hif_stop;
		}
	}

	ret = ath12k_dp_tx_htt_h2t_ver_req_msg(ab);
	if (ret) {
		ath12k_err(ab, "failed to send htt version request message: %d\n",
			   ret);
		goto err_hif_stop;
	}

	ath12k_acpi_set_dsm_func(ab);

	/* Indicate the core start in the appropriate group */
	ath12k_core_to_group_ref_get(ab);

	ath12k_dp_rx_fst_init(ab);
	ath12k_wsi_load_info_wsiorder_update(ab);

	return 0;

err_hif_stop:
	ath12k_hif_stop(ab);
err_wmi_detach:
	ath12k_wmi_detach(ab);
	return ret;
}

static void ath12k_core_device_cleanup(struct ath12k_base *ab)
{
	mutex_lock(&ab->core_lock);

	ath12k_hif_irq_disable(ab);
	ath12k_core_pdev_destroy(ab);
	ath12k_dp_umac_reset_deinit(ab);
	mutex_unlock(&ab->core_lock);
}

static void ath12k_stats_event_work_handler(struct wiphy *wiphy,
					    struct wiphy_work *work)
{
	struct ath12k_stats_work_context *stats_ctx;
	struct ath12k_stats_list_entry *list_entry;
	struct list_head temp_list;

	stats_ctx = container_of(work, struct ath12k_stats_work_context,
				 stats_nb_work);

	INIT_LIST_HEAD(&temp_list);

	spin_lock(&stats_ctx->list_lock);
	list_splice_tail_init(&stats_ctx->work_list, &temp_list);
	spin_unlock(&stats_ctx->list_lock);

	while (!list_empty(&temp_list)) {
		list_entry = list_first_entry(&temp_list,
					      struct ath12k_stats_list_entry,
					      node);

		ath12k_wifi_stats_reply_setup(&list_entry->usr_command);

		list_del(&list_entry->node);
		kfree(list_entry);
	}
}

static void ath12k_stats_event_work_free(struct ath12k_stats_work_context *stats_ctx)
{
	struct ath12k_stats_list_entry *list_entry;

	spin_lock(&stats_ctx->list_lock);
	while (!list_empty(&stats_ctx->work_list)) {
		list_entry = list_first_entry(&stats_ctx->work_list,
					      struct ath12k_stats_list_entry,
					      node);

		list_del(&list_entry->node);
		kfree(list_entry);
	}
	spin_unlock(&stats_ctx->list_lock);
}

static void ath12k_core_hw_group_stop(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab;
	struct ath12k_hw *ah = ag->ah[0];
	int i;

	lockdep_assert_held(&ag->mutex);

	clear_bit(ATH12K_GROUP_FLAG_REGISTERED, &ag->flags);
	cancel_work_sync(&ag->reset_group_work);

	for (i = ag->num_devices - 1; i >= 0; i--) {
		ab = ag->ab[i];
		if (!ab || ab->is_bypassed)
			continue;
		mutex_lock(&ab->core_lock);
		ath12k_core_pdev_deinit(ab);
		mutex_unlock(&ab->core_lock);
	}

	wiphy_work_cancel(ah->hw->wiphy, &ag->stats_work.stats_nb_work);
	ath12k_stats_event_work_free(&ag->stats_work);

	ath12k_mac_unregister(ag);

	ath12k_mac_mlo_teardown(ag);

	for (i = ag->num_devices - 1; i >= 0; i--) {
		ab = ag->ab[i];
		if (!ab || ab->is_bypassed)
			continue;

		clear_bit(ATH12K_FLAG_REGISTERED, &ab->dev_flags);

		ath12k_core_device_cleanup(ab);
		
		if (ab->hw_params->reoq_lut_support &&
		    !ab->pm_suspend) {
			mutex_lock(&ab->core_lock);
			ath12k_dp_reoq_lut_addr_reset(ath12k_ab_to_dp(ab));
			mutex_unlock(&ab->core_lock);
		}
	}

	ath12k_mac_destroy(ag);
}

u8 ath12k_get_num_partner_link(struct ath12k *ar)
{
	struct ath12k_base *partner_ab, *ab = ar->ab;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_pdev *pdev;
	u8 num_link = 0;
	int i, j;

	lockdep_assert_held(&ag->mutex);

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];

		for (j = 0; j < partner_ab->num_radios; j++) {
			pdev = &partner_ab->pdevs[j];

			/* Avoid the self link */
			if (ar == pdev->ar)
				continue;

			num_link++;
		}
	}

	return num_link;
}

static int __ath12k_mac_mlo_ready(struct ath12k *ar)
{
	u8 num_link = ath12k_get_num_partner_link(ar);
	int ret;

	if (num_link == 0)
		return 0;

	ret = ath12k_wmi_mlo_ready(ar);
	if (ret) {
		ath12k_err(ar->ab, "MLO ready failed for pdev %d: %d\n",
			   ar->pdev_idx, ret);
		return ret;
	}

	ar->teardown_complete_event = false;
	ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "mlo ready done for pdev %d\n",
		   ar->pdev_idx);

	return 0;
}

int ath12k_mac_mlo_ready(struct ath12k_hw_group *ag)
{
	struct ath12k_hw *ah;
	struct ath12k *ar;
	int ret;
	int i, j;

	for (i = 0; i < ag->num_hw; i++) {
		ah = ag->ah[i];
		if (!ah)
			continue;

		for_each_ar(ah, ar, j) {
			ar = &ah->radio[j];
			if (!ar || ar->ab->is_bypassed)
				continue;
			ret = __ath12k_mac_mlo_ready(ar);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int ath12k_core_mlo_setup(struct ath12k_hw_group *ag)
{
	int ret, i;

	if (!ag->mlo_capable)
		return 0;

	ret = ath12k_mac_mlo_setup(ag);
	if (ret)
		return ret;

	for (i = 0; i < ag->num_devices; i++)
		if (!ag->ab[i]->is_bypassed)
			ath12k_dp_partner_cc_init(ag->ab[i]);

	ret = ath12k_mac_mlo_ready(ag);
	if (ret)
		goto err_mlo_teardown;

	return 0;

err_mlo_teardown:
	ath12k_mac_mlo_teardown(ag);

	return ret;
}

int ath12k_core_pdev_enable_telemetry_stats(struct ath12k_base *ab)
{
	struct ath12k_pdev *pdev;
	struct ath12k *ar;
	int ret, i;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		if (pdev) {
			ar = pdev->ar;
			ret = ath12k_wmi_pdev_enable_telemetry_stats(ab, ar, NULL);
			if (ret) {
				ath12k_err(ab,
					   "Failed to enable pdev telemetry stats for pdev id:%d\n",
					   pdev->pdev_id);
			} else {
				ath12k_info(ab,
					    "Enable pdev telemetry stats for pdev id: %d\n",
					    pdev->pdev_id);
			}
		}
	}

	return 0;
}

static int ath12k_core_hw_group_start(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab;
	int ret, i;

	lockdep_assert_held(&ag->mutex);

	if (test_bit(ATH12K_GROUP_FLAG_REGISTERED, &ag->flags)) {
		ret = ath12k_core_mlo_setup(ag);
		if (WARN_ON(ret))
			goto err_mac_destroy;
		goto core_pdev_create;
	}

	switch (ath12k_crypto_mode) {
	case ATH12K_CRYPT_MODE_SW:
		set_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ag->flags);
		set_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ag->flags);
		break;
	case ATH12K_CRYPT_MODE_HW:
		clear_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ag->flags);
		clear_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ag->flags);
		break;
	default:
		ath12k_info(NULL, "invalid crypto_mode: %d\n", ath12k_crypto_mode);
		return -EINVAL;
	}

	if (ath12k_frame_mode == ATH12K_HW_TXRX_RAW)
		set_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ag->flags);

	/* Skip mac allocate and register during WSI remap,
	 * as the devices will be already registered to mac
	 * and only driver add/delete will be done.
	 */

	if (!ag->wsi_remap_in_progress) {
		ret = ath12k_mac_allocate(ag);
		if (WARN_ON(ret))
			return ret;
	}

	ret = ath12k_core_mlo_setup(ag);
	if (WARN_ON(ret))
		goto err_mac_destroy;

	if (!ag->wsi_remap_in_progress) {
		ret = ath12k_mac_register(ag);
		if (WARN_ON(ret))
			goto err_mlo_teardown;
	}

	set_bit(ATH12K_GROUP_FLAG_REGISTERED, &ag->flags);

	spin_lock_init(&ag->qos.profile_lock);

core_pdev_create:
	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab || ab->is_bypassed)
			continue;

		/* Skip pdev creation if WSI remap in progress and chip is not
		 * in bypass Add state, as the pdev for other chips will be
		 * already present.
		 */
		if (ag->wsi_remap_in_progress &&
		    ab->wsi_remap_state != ATH12K_WSI_BYPASS_ADD_DEVICE)
			continue;

		if (ath12k_check_erp_power_down(ag) && !ab->powerup_triggered)
			continue;

		mutex_lock(&ab->core_lock);

		if (ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE0 && !ab->recovery_start) {
			mutex_unlock(&ab->core_lock);
			continue;
		}

		set_bit(ATH12K_FLAG_REGISTERED, &ab->dev_flags);

		ret = ath12k_core_pdev_create(ab);
		if (ret) {
			ath12k_err(ab, "failed to create pdev core %d\n", ret);
			mutex_unlock(&ab->core_lock);
			goto err;
		}

		ret = ath12k_core_pdev_init(ab);
		if (ret) {
			ath12k_err(ab, "failed to init pdev core %d\n", ret);
			mutex_unlock(&ab->core_lock);
			goto err;
		}

		ath12k_debugfs_pdev_create(ab);

		ath12k_hif_irq_enable(ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		ath12k_hif_ppeds_irq_enable(ab, PPEDS_IRQ_REO2PPE);
		ath12k_hif_ppeds_irq_enable(ab, PPEDS_IRQ_PPE_WBM2SW_REL);
#endif

#ifdef CPTCFG_ATHDEBUG
		if (ab->hw_params->en_qdsslog) {
			ath12k_info(ab, "QDSS trace enabled\n");
			athdbg_if_get_service(ab, ATHDBG_SRV_CONFIG_QDSS);
		}
#endif
		ret = ath12k_core_rfkill_config(ab);
		if (ret && ret != -EOPNOTSUPP) {
			mutex_unlock(&ab->core_lock);
			goto err;
		}

                ret = ath12k_core_mlo_shmem_crash_info_init(ab);
                if (ret) {
                        ath12k_err(ab, "failed to parse crash info %d\n", ret);
                }

		if (ath12k_en_fwlog == true) {
			if (ath12k_enable_fwlog(ab))
				ath12k_err(ab, "failed to enable fwlog: %d\n", ret);
		}

		ret = ath12k_dp_umac_reset_init(ab);
		if (ret) {
			mutex_unlock(&ab->core_lock);
			ath12k_warn(ab, "Failed to initialize UMAC RESET: %d\n", ret);
			goto err;
		}

		ath12k_core_pdev_enable_telemetry_stats(ab);

		mutex_unlock(&ab->core_lock);
	}

	return 0;

err:
	ath12k_core_hw_group_stop(ag);
	return ret;

err_mlo_teardown:
	ath12k_mac_mlo_teardown(ag);

err_mac_destroy:
	ath12k_mac_destroy(ag);

	return ret;
}

static int ath12k_core_start_firmware(struct ath12k_base *ab,
				      enum ath12k_firmware_mode mode)
{
	int ret;

	ath12k_ce_get_shadow_config(ab, &ab->qmi.ce_cfg.shadow_reg_v3,
				    &ab->qmi.ce_cfg.shadow_reg_v3_len);

	ret = ath12k_qmi_firmware_start(ab, mode);
	if (ret) {
		ath12k_err(ab, "failed to send firmware start: %d\n", ret);
		return ret;
	}
#ifdef CONFIG_IO_COHERENCY
	ret = ath12k_core_config_iocoherency(ab, true);
	if (ret)
		ath12k_err(ab, "failed to configure IOCoherency: %d\n", ret);
#endif
	return ret;
}

static inline
bool ath12k_core_hw_group_start_ready(struct ath12k_hw_group *ag)
{
	lockdep_assert_held(&ag->mutex);

	/* If some device is in bypass state, consider num_bypassed counter
	 * to check the hw group ready.
	 */
	return (ag->num_started == (ag->num_devices - ag->num_bypassed));
}

static void ath12k_fw_stats_pdevs_free(struct list_head *head)
{
	struct ath12k_fw_stats_pdev *i, *tmp;

	list_for_each_entry_safe(i, tmp, head, list) {
		list_del(&i->list);
		kfree(i);
	}
}

void ath12k_fw_stats_bcn_free(struct list_head *head)
{
	struct ath12k_fw_stats_bcn *i, *tmp;

	list_for_each_entry_safe(i, tmp, head, list) {
		list_del(&i->list);
		kfree(i);
	}
}

static void ath12k_fw_stats_vdevs_free(struct list_head *head)
{
	struct ath12k_fw_stats_vdev *i, *tmp;

	list_for_each_entry_safe(i, tmp, head, list) {
		list_del(&i->list);
		kfree(i);
	}
}

void ath12k_fw_stats_init(struct ath12k *ar)
{
	INIT_LIST_HEAD(&ar->fw_stats.vdevs);
	INIT_LIST_HEAD(&ar->fw_stats.pdevs);
	INIT_LIST_HEAD(&ar->fw_stats.bcn);
	init_completion(&ar->fw_stats_complete);
	init_completion(&ar->fw_stats_done);
}

void ath12k_fw_stats_free(struct ath12k_fw_stats *stats)
{
	ath12k_fw_stats_pdevs_free(&stats->pdevs);
	ath12k_fw_stats_vdevs_free(&stats->vdevs);
	ath12k_fw_stats_bcn_free(&stats->bcn);
}
EXPORT_SYMBOL(ath12k_fw_stats_free);

void ath12k_fw_stats_reset(struct ath12k *ar)
{
	spin_lock_bh(&ar->data_lock);
	ath12k_fw_stats_free(&ar->fw_stats);
	ar->fw_stats.num_vdev_recvd = 0;
	ar->fw_stats.num_bcn_recvd = 0;
	spin_unlock_bh(&ar->data_lock);
}

static void ath12k_core_wsi_remap_mlo_reconfig(struct ath12k_hw_group *ag)
{
	int i;
	struct ath12k_base *partner_ab;

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		if (!partner_ab->is_bypassed) {
			ath12k_dbg(partner_ab, ATH12K_DBG_WSI_BYPASS, "WSI Bypass: Trigger MLO reconfig");
			ath12k_qmi_trigger_mlo_reconfig(partner_ab);
		}
	}
}

int ath12k_core_qmi_firmware_ready(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ath12k_ab_to_ag(ab);
	int ret, i;
	struct ath12k *ar;
	struct ath12k_bridge_iter bridge_iter = {};
	u8 active_num_devices;
	struct ath12k_base *partner_ab;

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	/* TODO: DS: revisit this for new DS design in WDS mode */
	if (ath12k_ppe_ds_enabled) {
		if (ath12k_frame_mode != ATH12K_HW_TXRX_ETHERNET) {
			ath12k_warn(ab,
				    "Force enabling Ethernet frame mode in PPE DS for" \
				    " AP and STA modes.\n");
			/* MESH and WDS VAPs will still use NATIVE_WIFI mode
			 * @ath12k_mac_update_vif_offload()
			 * TODO: add device capability check
			 */
			ath12k_ppe_ds_enabled = 0;
		} else if (ab->hw_params->ds_support) {
			set_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags);
		}
	}
#endif
	ret = ath12k_core_start_firmware(ab, ab->fw_mode);
	if (ret) {
		ath12k_err(ab, "failed to start firmware: %d\n", ret);
		return ret;
	}

	ret = ath12k_ce_init_pipes(ab);
	if (ret) {
		ath12k_err(ab, "failed to initialize CE: %d\n", ret);
		goto err_firmware_stop;
	}

	ret = ath12k_dp_cmn_device_init(ab->dp);
	if (ret) {
		ath12k_err(ab, "failed to init DP: %d\n", ret);
		goto err_firmware_stop;
	}


	mutex_lock(&ag->mutex);
	mutex_lock(&ab->core_lock);

	if (ath12k_cfg_init(ab))
		ath12k_err(ab, "Failed to initialize per radio INI data in driver\n");
	else
		ath12k_info(ab, "Initialized per radio INI data in driver\n");

	ret = ath12k_core_start(ab);
	if (ret) {
		ath12k_err(ab, "failed to start core: %d\n", ret);
		goto err_dp_free;
	}

	mutex_unlock(&ab->core_lock);

	if (ath12k_core_hw_group_start_ready(ag)) {
		if (!ag->wsi_remap_in_progress) {
			ret = ath12k_qmi_mlo_global_snapshot_mem_init(ab);
			if (ret) {
				ath12k_warn(ab, "failure in global mem init\n");
				goto err_core_stop;
			}
		}

		/* When the WSI remap is in progress or when one of the device
		 * is in bypassed state, other device which is restarting
		 * should update the remapped MLO config during bootup.
		 */
		if (ag->wsi_remap_in_progress ||
		    (ag->num_bypassed > 0 &&
		     test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags))) {
			ath12k_core_wsi_remap_mlo_reconfig(ag);
		}

		ret = ath12k_core_hw_group_start(ag);
		if (ret) {
			ath12k_warn(ab, "unable to start hw group\n");
			goto err_core_stop;
		}
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "group %d started\n", ag->id);
		if (ag->wsi_remap_in_progress) {
			/* During bypass, device will restart from start.
			 * But the ath12k reference will be already present.
			 * Hence reset the flags here.
			 */
			for (i = 0; i < ab->num_radios; i++) {
				ar = ab->pdevs[i].ar;
				ar->pdev_suspend = false;
			}

			if (!ar)
				ath12k_err(ab, "ar is NULL\n");

			active_num_devices = ag->num_devices - ag->num_bypassed;
			if (ab->wsi_remap_state == ATH12K_WSI_BYPASS_ADD_DEVICE &&
			    active_num_devices == ATH12K_MIN_NUM_DEVICES_NLINK && ar) {
				bridge_iter.ah = ar->ah;
				bridge_iter.active_num_devices = active_num_devices;
				ieee80211_iterate_interfaces(ar->ah->hw, IEEE80211_IFACE_ITER_NORMAL,
							     ath12k_mac_add_bridge_vdevs_iter,
							     &bridge_iter);
			}
			/* Reset the WSI flags */
			ag->wsi_remap_in_progress = false;
			ab->wsi_remap_state = 0;
			ath12k_info(ab, "WSI remap: Device re-addition completed\n");
		}
	}

	if (ath12k_core_hw_group_start_ready(ag)) {
		mutex_unlock(&ag->mutex);
		for (i = 0; i < ag->num_devices; i++) {
			partner_ab = ag->ab[i];
			if (partner_ab->is_static_bypassed) {
				ret = ath12k_wsi_bypass_precheck(partner_ab, 1);
				if (ret) {
					ath12k_err(ab, "WSI Bypass precheck failed");
					goto out;
				}
				partner_ab->wsi_remap_state = 1;
				ath12k_mac_dynamic_wsi_remap(partner_ab);
			}
		}
		goto out;
	}

	mutex_unlock(&ag->mutex);

out:
	return 0;

err_core_stop:
	for (i = ag->num_devices - 1; i >= 0; i--) {
		ab = ag->ab[i];
		if (!ab || ab->is_bypassed)
			continue;

		mutex_lock(&ab->core_lock);
		ath12k_core_stop(ab);
		mutex_unlock(&ab->core_lock);
	}
	mutex_unlock(&ag->mutex);
	goto exit;

err_dp_free:
	ath12k_dp_cmn_device_deinit(ab->dp);
	mutex_unlock(&ab->core_lock);
	mutex_unlock(&ag->mutex);

err_firmware_stop:
	ath12k_qmi_firmware_stop(ab);

exit:
	return ret;
}

u8 ath12k_core_get_total_num_vdevs(struct ath12k_base *ab)
{
	if (ab->ag && ab->ag->num_devices >= ATH12K_MIN_NUM_DEVICES_NLINK)
		return TARGET_NUM_VDEVS + TARGET_NUM_BRIDGE_VDEVS;

	return TARGET_NUM_VDEVS;
}
EXPORT_SYMBOL(ath12k_core_get_total_num_vdevs);

bool ath12k_core_is_vdev_limit_reached(struct ath12k *ar,
				       bool is_bridge_vdev)
{
	struct ath12k_base *ab;
	u32 num_created_vdevs;
	u8 total_num_vdevs, num_created_bridge_vdevs;
	bool ret = false;

	ab = ar->ab;
	total_num_vdevs = ath12k_core_get_total_num_vdevs(ab);
	num_created_vdevs = ar->num_created_vdevs;
	num_created_bridge_vdevs = ar->num_created_bridge_vdevs;

	if (total_num_vdevs == ATH12K_MAX_NUM_VDEVS_NLINK) {
		if ((num_created_vdevs + num_created_bridge_vdevs) >
		    (ATH12K_MAX_NUM_VDEVS_NLINK - 1)) {
			ath12k_err(ab, "failed to create vdev, reached total max vdev limit %d[%d]\n",
				   num_created_vdevs + num_created_bridge_vdevs,
				   ATH12K_MAX_NUM_VDEVS_NLINK);
			ret = true;
			goto exit;
		}

		if (!is_bridge_vdev &&
		    num_created_vdevs > (TARGET_NUM_VDEVS - 1)) {
			ath12k_err(ab, "failed to create vdev, reached max vdev limit %d[%d]\n",
				   num_created_vdevs,
				   TARGET_NUM_VDEVS);
			ret = true;
			goto exit;
		}

		if (is_bridge_vdev &&
		    num_created_bridge_vdevs > (TARGET_NUM_BRIDGE_VDEVS - 1)) {
			ath12k_warn(ab, "failed to create bridge vdev, reached max bridge vdev limit: %d[%d]\n",
				    num_created_bridge_vdevs, TARGET_NUM_BRIDGE_VDEVS);
			ret = true;
			goto exit;
		}
		goto exit;
	}

	if (num_created_vdevs > (TARGET_NUM_VDEVS - 1)) {
		ath12k_err(ab, "failed to create vdev, reached max vdev limit %d [%d]\n",
			   num_created_vdevs, TARGET_NUM_VDEVS);
		ret = true;
	}

exit:
	return ret;
}

static int ath12k_core_reconfigure_on_crash(struct ath12k_base *ab)
{
	int ret;
	u8 total_vdevs;

	mutex_lock(&ab->core_lock);
	ath12k_core_pdev_deinit(ab);
	ath12k_dp_arch_pdev_free(ab->dp);
	ath12k_ce_cleanup_pipes(ab);
	ath12k_wmi_detach(ab);
	mutex_unlock(&ab->core_lock);

	ath12k_dp_cmn_device_deinit(ab->dp);
	ath12k_hal_srng_deinit(ab);
	ath12k_dp_umac_reset_deinit(ab);
	ath12k_umac_reset_completion(ab);

	total_vdevs = ath12k_core_get_total_num_vdevs(ab);
	ab->free_vdev_map = (1LL << (ab->num_radios * total_vdevs)) - 1;
	ab->free_vdev_stats_id_map = 0;

	ret = ath12k_hal_srng_init(ab);
	if (ret)
		return ret;

	clear_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags);

	ret = ath12k_core_qmi_firmware_ready(ab);
	if (ret)
		goto err_hal_srng_deinit;

	/* ATH12K_FLAG_QMI_FW_READY_COMPLETE flag set during ERP exit*/
	if (ath12k_check_erp_power_down(ab->ag)) {
		if (!test_bit(ATH12K_FLAG_QMI_FW_READY_COMPLETE, &ab->dev_flags))
			set_bit(ATH12K_FLAG_QMI_FW_READY_COMPLETE, &ab->dev_flags);
	}

	return 0;

err_hal_srng_deinit:
	ath12k_hal_srng_deinit(ab);
	return ret;
}

static void ath12k_rfkill_work(struct work_struct *work)
{
	struct ath12k_base *ab = container_of(work, struct ath12k_base, rfkill_work);
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k *ar;
	struct ath12k_hw *ah;
	struct ieee80211_hw *hw;
	bool rfkill_radio_on;
	int i, j;

	spin_lock_bh(&ab->base_lock);
	rfkill_radio_on = ab->rfkill_radio_on;
	spin_unlock_bh(&ab->base_lock);

	for (i = 0; i < ag->num_hw; i++) {
		ah = ath12k_ag_to_ah(ag, i);
		if (!ah)
			continue;

		for (j = 0; j < ah->num_radio; j++) {
			ar = &ah->radio[j];
			if (!ar)
				continue;

			ath12k_mac_rfkill_enable_radio(ar, rfkill_radio_on);
		}

		hw = ah->hw;
		wiphy_rfkill_set_hw_state(hw->wiphy, !rfkill_radio_on);
	}
}

static void ath12k_mac_peer_ab_disassoc(struct ath12k_base *ab)
{
	struct ath12k_dp_link_peer *peer, *tmp;
	struct ath12k_sta *ahsta;
	struct ieee80211_sta *sta;

	spin_lock_bh(&ab->dp->dp_lock);
	list_for_each_entry_safe(peer, tmp, &ab->dp->peers, list) {

		if (!peer->vif)
			continue;

		/* In case of STA Vif type,
		 * report disconnect will be sent during sta_restart work.
		 */
		if (peer->vif->type == NL80211_IFTYPE_STATION)
			continue;

		sta = peer->sta;
		if (!sta)
			continue;

		ahsta = (struct ath12k_sta *)sta->drv_priv;
		/* Sending low ack event to hostapd to remove (free) the
		 * existing STAs since FW is crashed and recovering at the momemt.
		 * After recovery, FW comes up with no information about peers.
		 * To stop any operation related to peers coming from upper
		 * layers.
		 * Here, 0xFFFF is used to differentiate between low ack event
		 * sent during recovery versus normal low ack event. In normal,
		 * low ack event, num_packets is not expected to be 0xFFFF.
		 */
		ath12k_mac_peer_disassoc(ab, sta, ahsta, ATH12K_DBG_MAC);
	}
	spin_unlock_bh(&ab->dp->dp_lock);
}

void ath12k_dcs_wlan_intf_cleanup(struct ath12k *ar)
{
	struct ath12k_dcs_wlan_interference *dcs_wlan_intf, *temp;

	spin_lock_bh(&ar->data_lock);
	list_for_each_entry_safe(dcs_wlan_intf, temp, &ar->wlan_intf_list, list) {
		list_del(&dcs_wlan_intf->list);
		kfree(dcs_wlan_intf);
	}
	spin_unlock_bh(&ar->data_lock);
}

void ath12k_core_halt(struct ath12k *ar)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_hw_group *ag = ab->ag;

	if (ab->is_bypassed)
		return;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	/* Send low ack disassoc to hostapd to free the peers from host
	 * to associate fresh after recovery. It is expected that, this
	 * low ack event to hostapd must free the stas from hostapd and
	 * kernel before new association process starts.
	 *
	 * And also during ieee80211_reconfig, it should not add back the
	 * existing stas as it is already freed from host due to low ack
	 * event.
	 */

	if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE0)
		ath12k_mac_peer_ab_disassoc(ab);

	ath12k_telemetry_ab_peer_agent_destroy(ab);

	ath12k_mac_peer_cleanup_all(ar);
	ath12k_dcs_wlan_intf_cleanup(ar);
	cancel_work_sync(&ar->regd_update_work);
	cancel_work_sync(&ar->reg_set_previous_country);
	cancel_work_sync(&ar->wlan_intf_work);
	cancel_work_sync(&ab->rfkill_work);
	cancel_work_sync(&ab->update_11d_work);
	wiphy_work_cancel(ath12k_ar_to_hw(ar)->wiphy, &ar->agile_cac_abort_wq);

	rcu_assign_pointer(ab->pdevs_active[ar->pdev_idx], NULL);
	synchronize_rcu();

	if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE0)
		INIT_LIST_HEAD(&ar->arvifs);
	idr_init(&ar->txmgmt_idr);

	cancel_work_sync(&ar->erp_handle_trigger_work);
	ar->erp_trigger_set = false;
	cancel_work_sync(&ar->ssr_erp_exit);
}

static void ath12k_core_mlo_hw_queues_stop(struct ath12k_hw_group *ag)
{
	struct ath12k_hw *ah;
	int i;

	lockdep_assert_held(&ag->mutex);

	for (i = 0; i < ag->num_hw; i++) {
		ah = ath12k_ag_to_ah(ag,i);
		if (!ah)
			continue;

		wiphy_lock(ah->hw->wiphy);
		/* If queue 0 is stopped, it is safe to assume that all
		 * other queues are stopped by driver via
		 * ieee80211_stop_queues() below. This means, there is
		 * no need to stop it again and hence continue
		 */

		if (ieee80211_queue_stopped(ah->hw, 0)) {
			wiphy_unlock(ah->hw->wiphy);
			return;
		}

                ieee80211_stop_queues(ah->hw);
		wiphy_unlock(ah->hw->wiphy);
	}
}

void ath12k_core_radio_cleanup(struct ath12k *ar)
{
	ar->free_map_id = ATH12K_FREE_MAP_ID_MASK;
	ath12k_mac_drain_tx(ar);
	ar->state_11d = ATH12K_11D_IDLE;
	complete(&ar->completed_11d_scan);
	complete(&ar->scan.started);
	complete_all(&ar->scan.completed);
	complete(&ar->scan.on_channel);
	complete(&ar->peer_assoc_done);
	complete(&ar->peer_delete_done);
	ath12k_debugfs_nrp_cleanup_all(ar);
	complete(&ar->install_key_done);
	complete(&ar->vdev_setup_done);
	complete(&ar->vdev_delete_done);
	complete(&ar->bss_survey_done);
	complete(&ar->thermal.wmi_sync);
	complete(&ar->scan.on_channel);

	wake_up(&ar->dp.tx_empty_waitq);
	idr_for_each(&ar->txmgmt_idr,
		     ath12k_mac_tx_mgmt_pending_free, ar);
	idr_destroy(&ar->txmgmt_idr);
	wake_up(&ar->txmgmt_empty_waitq);

	ar->monitor_vdev_id = -1;
	ar->monitor_started = false;
	ar->monitor_vdev_created = false;
}

static void ath12k_core_pre_reconfigure_recovery(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k *ar;
	struct ath12k_hw *ah;
	struct ath12k_link_vif *arvif;
	int i, j;

	spin_lock_bh(&ab->base_lock);
	ab->stats.fw_crash_counter++;
	spin_unlock_bh(&ab->base_lock);

	if (ab->is_reset)
		set_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags);

	for (i = 0; i < ag->num_hw; i++) {
		ah = ath12k_ag_to_ah(ag, i);
		if (!ah || ah->state == ATH12K_HW_STATE_OFF ||
		    ah->state == ATH12K_HW_STATE_TM ||
		    ah->state == ATH12K_HW_STATE_RESTARTING)
			continue;

		for (j = 0; j < ah->num_radio; j++) {
			ar = &ah->radio[j];
			if (ar->ab->is_bypassed)
				continue;

			if (ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE0 &&
			    !ar->ab->is_reset)
				continue;

			list_for_each_entry(arvif, &ar->arvifs, list) {
				if (arvif->is_started)
					ath12k_debugfs_remove_interface(arvif);
				arvif->is_started = false;
				arvif->is_created = false;
				arvif->is_up = false;
			}
			ath12k_core_radio_cleanup(ar);
		}

		wiphy_unlock(ah->hw->wiphy);
	}

	wake_up(&ab->wmi_ab.tx_credits_wq);
	wake_up(&ab->peer_mapping_wq);
}

static void ath12k_update_11d(struct work_struct *work)
{
	struct ath12k_base *ab = container_of(work, struct ath12k_base, update_11d_work);
	struct ath12k *ar;
	struct ath12k_pdev *pdev;
	struct wmi_set_current_country_arg arg = {};
	int ret, i;

	spin_lock_bh(&ab->base_lock);
	memcpy(&arg.alpha2, &ab->new_alpha2, 2);
	spin_unlock_bh(&ab->base_lock);

	ath12k_dbg(ab, ATH12K_DBG_WMI, "update 11d new cc %c%c\n",
	arg.alpha2[0], arg.alpha2[1]);

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		ar = pdev->ar;

		memcpy(&ar->alpha2, &arg.alpha2, 2);
		ret = ath12k_wmi_send_set_current_country_cmd(ar, &arg);
		if (ret)
			ath12k_warn(ar->ab,
				    "pdev id %d failed set current country code: %d\n",
				    i, ret);
	}
}

static void ath12k_core_post_reconfigure_recovery(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_hw *ah;
	struct ath12k *ar;
	int i, j;

	for (i = 0; i < ag->num_hw; i++) {
		ah = ath12k_ag_to_ah(ag, i);
		if (!ah || ah->state == ATH12K_HW_STATE_OFF)
			continue;

		wiphy_lock(ah->hw->wiphy);
		mutex_lock(&ah->hw_mutex);

		switch (ah->state) {
		case ATH12K_HW_STATE_ON:
			ah->state = ATH12K_HW_STATE_RESTARTING;

			for (j = 0; j < ah->num_radio; j++) {
				ar = &ah->radio[j];
				if (ar->ab->is_bypassed)
					continue;

				if (ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE0 &&
				    !ar->ab->is_reset)
					continue;

				if (ar->scan.state == ATH12K_SCAN_RUNNING ||
						ar->scan.state == ATH12K_SCAN_STARTING)
					ar->scan.state = ATH12K_SCAN_ABORTING;
				ath12k_mac_scan_finish(ar);
				mutex_unlock(&ah->hw_mutex);
				wiphy_unlock(ah->hw->wiphy);
				cancel_delayed_work_sync(&ar->scan.timeout);
				cancel_delayed_work_sync(&ar->scan.roc_done);
				wiphy_lock(ah->hw->wiphy);
				mutex_lock(&ah->hw_mutex);
				ath12k_core_halt(ar);
			}
			/* At this point link peers will be deleted
			 * for all the radios through
			 * ath12k_mac_peer_cleannup_all()
			 */
			if (ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE2)
				ath12k_mac_dp_peer_cleanup(ah, ag->recovery_mode);

			break;
		case ATH12K_HW_STATE_OFF:
			ath12k_warn(ab,
				    "cannot restart hw %d that hasn't been started\n",
				    i);
			break;
		case ATH12K_HW_STATE_RESTARTING:
			break;
		case ATH12K_HW_STATE_RESTARTED:
			ah->state = ATH12K_HW_STATE_WEDGED;
			fallthrough;
		case ATH12K_HW_STATE_WEDGED:
			ath12k_warn(ab,
				    "device is wedged, will not restart hw %d\n", i);
			break;
		case ATH12K_HW_STATE_TM:
			ath12k_warn(ab, "fw mode reset done radio %d\n", i);
			break;
		}

		mutex_unlock(&ah->hw_mutex);
		wiphy_unlock(ah->hw->wiphy);
	}

	complete(&ab->driver_recovery);
}

static void ath12k_core_radio_start(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k *ar;
	struct ath12k_hw *ah = ath12k_ag_to_ah(ag, 0);
	int i, j;

	mutex_lock(&ag->mutex);
	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];

		for (j = 0; j < ab->num_radios; j++) {
			ar = ab->pdevs[j].ar;

			if (!ath12k_ftm_mode && ar) {
				if (ar->allocated_vdev_map) {
					continue;
				} else {
					if (ath12k_mac_start(ar)) {
						ath12k_err(ar->ab, "mac radio start failed\n");
						mutex_unlock(&ag->mutex);
						return;
					}

					ar->pdev_suspend = false;
				}
			}
		}

		ab->powerup_triggered = false;
		complete(&ab->power_up);
	}

	ah->state = ATH12K_HW_STATE_ON;
	mutex_unlock(&ag->mutex);

	if (test_bit(ATH12K_GROUP_FLAG_HIF_POWER_DOWN, &ag->flags))
		clear_bit(ATH12K_GROUP_FLAG_HIF_POWER_DOWN, &ag->flags);
}

static void ath12k_core_restart(struct work_struct *work)
{
	struct ath12k_base *ab = container_of(work, struct ath12k_base, restart_work);
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_hw *ah;
	int ret, i;

	ret = ath12k_core_reconfigure_on_crash(ab);
	if (ret) {
		ath12k_err(ab, "failed to reconfigure driver on crash recovery\n");
		/*
		 * If for any reason, reconfiguration fails, issue bug on for
		 * Mode 0
		 */
		if (ath12k_ssr_failsafe_mode && ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE0)
			BUG_ON(1);
		return;
	}

	if (ath12k_core_hw_group_start_ready(ag) &&
	    ath12k_check_erp_power_down(ag) &&
	    !ath12k_hw_group_recovery_in_progress(ag))
		ath12k_core_radio_start(ab);

	if (ab->is_reset ||
	    (ath12k_check_erp_power_down(ag) &&
	     ath12k_hw_group_recovery_in_progress(ag))) {
		if (!test_bit(ATH12K_FLAG_REGISTERED, &ab->dev_flags)) {
			atomic_dec(&ab->reset_count);
			complete(&ab->reset_complete);
			ab->is_reset = false;
			atomic_set(&ab->fail_cont_count, 0);
			ath12k_dbg(ab, ATH12K_DBG_BOOT, "reset success\n");
		}

		mutex_lock(&ag->mutex);

		if (!ath12k_core_hw_group_start_ready(ag)) {
			mutex_unlock(&ag->mutex);
			goto exit_restart;
		}

		if (ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE0) {
			queue_work(ab->workqueue_aux, &ab->recovery_work);
			mutex_unlock(&ag->mutex);
			goto exit_restart;
		}

		for (i = 0; i < ag->num_hw; i++) {
			ah = ath12k_ag_to_ah(ag, i);
			ieee80211_restart_hw(ah->hw);
		}

		mutex_unlock(&ag->mutex);
	}

exit_restart:
	complete(&ab->restart_completed);
}

static void ath12k_core_mode1_recovery_sta_list(void *data, struct ieee80211_sta *sta)
{
	struct ath12k_link_sta *arsta, *p_arsta;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_link_vif *arvif = (struct ath12k_link_vif *)data, *p_arvif;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ieee80211_vif *vif = ahvif->vif;
	struct ath12k *ar = arvif->ar;
	struct ath12k_base *ab = arvif->ar->ab;
	struct ath12k_key_conf *key_conf = NULL;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ar->ab);
	struct ieee80211_key_conf *key;
	int ret = -1, key_idx;
	u8 link_id = arvif->link_id;
	enum ieee80211_sta_state state, prev_state;
	bool sta_added = false;
	unsigned long links;

	if (ahsta->ahvif != arvif->ahvif)
		return;

	/* Check if there is a link sta in the vif link */
	if (!(BIT(link_id) & ahsta->links_map))
		return;

	/* From iterator, rcu_read_lock is acquired. Will be revisited
	 * later to use local list
	 */
	arsta = rcu_dereference(ahsta->link[link_id]);
	if (!arsta)
		return;

	key_conf = container_of((void *)sta, struct ath12k_key_conf, sta);

	if (vif->type != NL80211_IFTYPE_AP &&
	    vif->type != NL80211_IFTYPE_AP_VLAN &&
	    vif->type != NL80211_IFTYPE_STATION &&
	    vif->type != NL80211_IFTYPE_MESH_POINT)
		return;

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, arvif->vdev_id, arsta->addr);
	if (peer) {
		sta_added = true;
		spin_unlock_bh(&dp->dp_lock);
		goto key_add;
	}
	spin_unlock_bh(&dp->dp_lock);

	prev_state = arsta->ahsta->state;
	for (state = IEEE80211_STA_NOTEXIST;
			state < prev_state; state++) {
		/* all station set case */
		/* TODO: Iterator API is called with rcu lock
		 * hence need for this unlock/lock statement.
		 * Need to revisit in next version
		 */
		rcu_read_unlock();
		ath12k_mac_op_sta_state(ar->ah->hw, arvif->ahvif->vif, sta,
				state, (state + 1));
		rcu_read_lock();
		sta_added = true;
	}

	if (!sta_added)
		goto skip_key_add;

key_add:
	for (key_idx = 0; key_idx < WMI_MAX_KEY_INDEX; key_idx++) {
		key = arsta->keys[key_idx];

		if (key) {
			/* BIP needs to be done in software */
			if (key->cipher == WLAN_CIPHER_SUITE_AES_CMAC ||
			    key->cipher == WLAN_CIPHER_SUITE_BIP_GMAC_128 ||
			    key->cipher == WLAN_CIPHER_SUITE_BIP_GMAC_256 ||
			    key->cipher == WLAN_CIPHER_SUITE_BIP_CMAC_256) {
				return;
			}

			if (test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED,
				     &ab->ag->flags))
				return;

			if (!arvif->is_created) {
				key_conf = kzalloc(sizeof(*key_conf), GFP_ATOMIC);

				if (!key_conf)
					return;

				key_conf->cmd = SET_KEY;
				key_conf->sta = sta;
				key_conf->key = key;

				list_add_tail(&key_conf->list,
					      &ahvif->cache[link_id]->key_conf.list);

				ath12k_info(ab, "set key param cached since vif not assign to radio\n");
				return;
			}

			if (sta->mlo) {
				links = ahsta->links_map;
				for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
					p_arvif = ath12k_get_arvif_from_link_id(ahvif,
										link_id);
					p_arsta = rcu_dereference(ahsta->link[link_id]);
					if (WARN_ON(!p_arvif || !p_arsta))
						continue;

					/* TODO: Iterator API is called with rcu lock
					 * hence need for this unlock/lock statement.
					 * Need to revisit in next version
					 */
					rcu_read_unlock();
					ret = ath12k_mac_set_key(p_arvif->ar, SET_KEY,
								 p_arvif,  p_arsta, key);
					rcu_read_lock();
					if (ret)
						break;
				}
			} else {
				p_arsta = &ahsta->deflink;
				p_arvif = p_arsta->arvif;
				if (WARN_ON(!p_arvif))
					return;

				/* TODO: Iterator API is called with rcu lock
				 * hence need for this unlock/lock statement.
				 * Need to revisit in next version
				 */
				rcu_read_unlock();
				ret = ath12k_mac_set_key(p_arvif->ar, SET_KEY, p_arvif,
							 p_arsta, key);
				rcu_read_lock();
			}
		}
	}

skip_key_add:
	ath12k_dbg(ab, ATH12K_DBG_MODE1_RECOVERY,
			"Recovered sta:%pM link_id:%d, num_sta:%d\n",
			arsta->addr, arsta->link_id, arvif->ar->num_stations);
	return;
}

static void ath12k_core_iterate_sta_list(struct ath12k *ar,
                                         struct ath12k_link_vif *arvif)
{
	ieee80211_iterate_stations_atomic(ar->ah->hw,
					  ath12k_core_mode1_recovery_sta_list,
					  arvif);
}

static void ath12k_core_ml_sta_add(struct ath12k *ar)
{
	struct ath12k_link_vif *arvif, *tmp;
	struct ieee80211_bss_conf *info;
	struct ath12k_vif *ahvif;
	struct ieee80211_vif *vif;

	lockdep_assert_wiphy(ar->ah->hw->wiphy);

	list_for_each_entry_safe_reverse(arvif, tmp, &ar->arvifs, list) {
		ahvif = arvif->ahvif;

		if (!ahvif)
			continue;

		vif = ahvif->vif;
		if (ahvif->vdev_type != WMI_VDEV_TYPE_STA)
			continue;

		if (!vif->valid_links)
			continue;

		ath12k_core_iterate_sta_list(ar, arvif);

		if (ath12k_mac_is_bridge_vdev(arvif))
			info = NULL;
		else
			info = vif->link_conf[arvif->link_id];

		/* Set is_up to false as we will do
		 * recovery for that vif in the
		 * upcoming executions
		 */
		arvif->is_up = false;
		if (vif->cfg.assoc)
			ath12k_bss_assoc(ar, arvif, info);
		else
			ath12k_bss_disassoc(ar, arvif);
		ath12k_dbg(ar->ab, ATH12K_DBG_MODE1_RECOVERY,
			   "station vif:%pM recovered\n",
			   arvif->bssid);
	}
}

/* API to recovery station VIF enabled in non-asserted links */
static void ath12k_core_mlo_recover_station(struct ath12k_hw_group *ag,
					    struct ath12k_base *assert_ab)
{
	struct ath12k_base *ab;
	struct ath12k_pdev *pdev;
	struct ath12k *ar;
	int i, j;

	if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE2)
		return;

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];

		if (ab->is_bypassed || ab == assert_ab)
			continue;

		for (j = 0; j < ab->num_radios; j++) {
			pdev = &ab->pdevs[j];
			ar = pdev->ar;

			if (!ar)
				continue;

			if (list_empty(&ar->arvifs))
				continue;

			/* Re-add all MLD station VIF which are
			 * in non-asserted link
			 */
			ath12k_core_ml_sta_add(ar);
		}
	}
}

static int ath12k_mlo_recovery_link_vif_reconfig(struct ath12k *ar,
						struct ath12k_vif *ahvif,
						struct ath12k_link_vif *arvif,
						struct ieee80211_vif *vif,
						struct ieee80211_bss_conf *link_conf)
{
	int i;
	int link_id = arvif->link_id;
	struct ath12k_hw *ah = ar->ah;
	struct ieee80211_tx_queue_params params;
	struct wmi_wmm_params_arg *p = NULL;
	struct ieee80211_bss_conf *info;
	u64 changed = 0;
	bool bridge_vdev;

	lockdep_assert_wiphy(ah->hw->wiphy);

	switch (vif->type) {
	case NL80211_IFTYPE_AP_VLAN:
	case NL80211_IFTYPE_MONITOR:
		break;
	case NL80211_IFTYPE_ADHOC:
		fallthrough;
	default:
		ieee80211_iterate_stations_atomic(ar->ah->hw,
				ath12k_core_mode1_recovery_sta_list,
				arvif);
		fallthrough;
	case NL80211_IFTYPE_AP: /* AP stations are handled later */
		for (i = 0; i < IEEE80211_NUM_ACS; i++) {

			if ((vif->active_links &&
			    !(vif->active_links & BIT(link_id))) ||
			    link_id >= IEEE80211_MLD_MAX_NUM_LINKS)
				break;

			switch (i) {
			case IEEE80211_AC_VO:
				p = &arvif->wmm_params.ac_vo;
				break;
			case IEEE80211_AC_VI:
				p = &arvif->wmm_params.ac_vi;
				break;
			case IEEE80211_AC_BE:
				p = &arvif->wmm_params.ac_be;
				break;
			case IEEE80211_AC_BK:
				p = &arvif->wmm_params.ac_bk;
				break;
			}

			params.cw_min = p->cwmin;
			params.cw_max = p->cwmax;
			params.aifs = p->aifs;
			params.txop = p->txop;

			ath12k_mac_conf_tx(arvif, i, &params);
		}
		break;
	}

	/* common change flags for all interface types */
	changed = BSS_CHANGED_ERP_CTS_PROT |
		BSS_CHANGED_ERP_PREAMBLE |
		BSS_CHANGED_ERP_SLOT |
		BSS_CHANGED_HT |
		BSS_CHANGED_BASIC_RATES |
		BSS_CHANGED_BEACON_INT |
		BSS_CHANGED_BSSID |
		BSS_CHANGED_CQM |
		BSS_CHANGED_QOS |
		BSS_CHANGED_TXPOWER |
		BSS_CHANGED_MCAST_RATE;

	bridge_vdev = ath12k_mac_is_bridge_vdev(arvif);

	if (!bridge_vdev && link_conf->mu_mimo_owner)
		changed |= BSS_CHANGED_MU_GROUPS;

	switch (vif->type) {
	case NL80211_IFTYPE_STATION:
		if (!vif->valid_links) {
			/* Set this only for legacy stations */
			changed |= BSS_CHANGED_ASSOC |
				BSS_CHANGED_ARP_FILTER |
				BSS_CHANGED_PS;

			/* Assume re-send beacon info report to the driver */
			changed |= BSS_CHANGED_BEACON_INFO;

			if (link_conf->max_idle_period ||
					link_conf->protected_keep_alive)
				changed |= BSS_CHANGED_KEEP_ALIVE;

			if (!arvif->is_created) {
				ath12k_info(NULL,
					    "bss info parameter changes %llx cached to apply after vdev create on channel assign\n",
					    changed);
				ahvif->cache[link_id]->bss_conf_changed |= changed;

				return 0;
			}
		}

		/* Set is_up to false as we will do
		 * recovery for that vif in the
		 * upcoming executions
		 */
		arvif->is_up = false;
		ath12k_mac_bss_info_changed(ar, arvif, link_conf, changed);
		if (vif->valid_links) {
			if (bridge_vdev)
				info = NULL;
			else
				info = vif->link_conf[link_id];

			if (vif->cfg.assoc)
				ath12k_bss_assoc(ar, arvif, info);
			else
				ath12k_bss_disassoc(ar, arvif);
		}
		break;
	case NL80211_IFTYPE_OCB:
		changed |= BSS_CHANGED_OCB;

		ath12k_mac_bss_info_changed(ar, arvif, link_conf, changed);
		break;
	case NL80211_IFTYPE_ADHOC:
		changed |= BSS_CHANGED_IBSS;
		fallthrough;
	case NL80211_IFTYPE_AP:
		changed |= BSS_CHANGED_P2P_PS;

		if (vif->type == NL80211_IFTYPE_AP) {
			changed |= BSS_CHANGED_AP_PROBE_RESP;
			ahvif->u.ap.ssid_len = vif->cfg.ssid_len;
			if (vif->cfg.ssid_len)
				memcpy(ahvif->u.ap.ssid, vif->cfg.ssid, vif->cfg.ssid_len);
		}
		fallthrough;
	case NL80211_IFTYPE_MESH_POINT:
		if (link_conf->enable_beacon) {
			changed |= BSS_CHANGED_BEACON |
				BSS_CHANGED_BEACON_ENABLED;

			ath12k_mac_bss_info_changed(ar, arvif, link_conf,
					changed & ~BSS_CHANGED_IDLE);

		}
		break;
	case NL80211_IFTYPE_NAN:
	case NL80211_IFTYPE_AP_VLAN:
	case NL80211_IFTYPE_MONITOR:
	case NL80211_IFTYPE_P2P_DEVICE:
		/* nothing to do */
		break;
	case NL80211_IFTYPE_UNSPECIFIED:
	case NUM_NL80211_IFTYPES:
	case NL80211_IFTYPE_P2P_CLIENT:
	case NL80211_IFTYPE_P2P_GO:
	case NL80211_IFTYPE_WDS:
		WARN_ON(1);
		break;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MODE1_RECOVERY,
		   "Reconfig link vif done:type:%d\n", vif->type);

	return 0;
}

static int ath12k_mlo_core_recovery_reconfig_link_bss(struct ath12k *ar,
						      struct ieee80211_bss_conf *link_conf,
						      struct ath12k_vif *ahvif,
						      struct ath12k_link_vif *arvif)
{
	struct ieee80211_vif *vif = arvif->ahvif->vif;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_hw *ah = ar->ah;
	enum ieee80211_ap_reg_power power_type;
	struct ath12k_wmi_peer_create_arg param;
	struct ieee80211_chanctx_conf *ctx = &arvif->chanctx;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int ret = -1;
	u8 link_id;
	bool is_bridge_vdev;
	struct ath12k_dp_peer_create_params dp_params = {};

	lockdep_assert_wiphy(ah->hw->wiphy);

	is_bridge_vdev = ath12k_mac_is_bridge_vdev(arvif);
	link_id = is_bridge_vdev ? arvif->link_id : link_conf->link_id;

	ath12k_dbg(ab, ATH12K_DBG_MODE1_RECOVERY,
		   "Recovering: link_id:%d addr %pM type:%d subtype:%d\n",
		   link_id, arvif->bssid, vif->type, arvif->vdev_subtype);

	if (vif->type == NL80211_IFTYPE_AP &&
		ar->num_peers > (ar->max_num_peers - 1)) {
		ath12k_err(ab, "Error in peers:%d\n",
			   ar->num_peers);
		goto exit;
	}

	if (ath12k_core_is_vdev_limit_reached(ar, is_bridge_vdev))
		goto exit;

	ret = ath12k_mac_vdev_create(ar, arvif, is_bridge_vdev);

	if (ret) {
		ath12k_warn(ab, "failed to create vdev %pM\n", arvif->bssid);
		goto exit;
	}

	if (!is_bridge_vdev) {
		if (!ctx->def.chan) {
			ath12k_dbg(ab, ATH12K_DBG_MODE1_RECOVERY,
				   "Skipping vdev start for MLD %pM as chanctx is not assigned\n",
				   arvif->bssid);
			goto exit;
		}
		ath12k_mac_vif_cache_flush(ar, arvif);

		if (ar->supports_6ghz && ctx->def.chan->band == NL80211_BAND_6GHZ &&
		    (ahvif->vdev_type == WMI_VDEV_TYPE_STA ||
		    ahvif->vdev_type == WMI_VDEV_TYPE_AP)) {
			power_type = link_conf->power_type;
                        ath12k_dbg(ab, ATH12K_DBG_MAC, "mac chanctx power type %d\n",
				   power_type);
			if (power_type == IEEE80211_REG_UNSET_AP)
				power_type = IEEE80211_REG_LPI_AP;

			/* TODO: Transmit Power Envelope specification for 320 is not
			 * available yet. Need to add TPE 320 support when spec is ready
			 */
			if (ahvif->vdev_type == WMI_VDEV_TYPE_STA &&
			    ctx->def.width != NL80211_CHAN_WIDTH_320) {
				ath12k_mac_parse_tx_pwr_env(ar, arvif);
			}
		}
	}

	spin_lock_bh(&dp->dp_lock);
        /* for some targets bss peer must be created before vdev_start */
	if (ab->hw_params->vdev_start_delay &&
	    ahvif->vdev_type != WMI_VDEV_TYPE_AP &&
	    ahvif->vdev_type != WMI_VDEV_TYPE_MONITOR &&
	    !ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, arvif->vdev_id, arvif->bssid)) {
		ret = 0;
		spin_unlock_bh(&dp->dp_lock);
		goto exit;
	}
	spin_unlock_bh(&dp->dp_lock);

	if (ab->hw_params->vdev_start_delay &&
	    (ahvif->vdev_type == WMI_VDEV_TYPE_AP ||
	    ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR)) {
		dp_params.is_vdev_peer = true;
		dp_params.hw_link_id = ar->hw_link_id;

		ret = ath12k_dp_peer_create(&ah->dp_hw, arvif->bssid, &dp_params, vif);
		if (ret) {
			ath12k_warn(ab, "failed to create dp_peer for vdev AP %d: %d\n",
				    arvif->vdev_id, ret);
			goto exit;
		}

		param.vdev_id = arvif->vdev_id;
		param.peer_type = WMI_PEER_TYPE_DEFAULT;
		param.peer_addr = ar->mac_addr;

		ret = ath12k_peer_create(ar, arvif, NULL, &param);
		if (ret) {
			ath12k_warn(ab, "failed to create peer after vdev start delay: %d",
					ret);
			goto exit;
                }
        }

	if (ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR) {
		ret = ath12k_mac_monitor_start(ar);
		if (ret)
			goto exit;
		arvif->is_started = true;
		goto exit;
	}

	if (ctx->def.chan)
		ret = ath12k_mac_vdev_start(arvif, ctx);
	else if (is_bridge_vdev)
		ret = ath12k_mac_vdev_start(arvif, NULL);

	if (ret) {
		ath12k_err(ab, "vdev start failed during recovery\n");
		goto exit;
	}

	arvif->is_started = true;
	ret = 0;
exit:
	ath12k_dbg(ab, ATH12K_DBG_MODE1_RECOVERY,
		   "ret:%d No. of vdev created:%d, links_map:0x%x, flag:%d\n",
		   ret,
		   hweight16(ahvif->links_map),
		   ahvif->links_map,
		   arvif->is_created);

	return ret;
}

static void ath12k_core_peer_disassoc(struct ath12k_hw_group *ag,
				      struct ath12k_base *assert_ab)
{
	struct ath12k_base *ab;
	struct ath12k_dp_link_peer *peer, *tmp;
	struct ath12k_sta *ahsta;
	struct ieee80211_sta *sta;
	int i;

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];

		if (ab->is_bypassed)
			continue;

		if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE2) {
			if (ab != assert_ab)
				continue;
		}

		spin_lock_bh(&ab->dp->dp_lock);
		list_for_each_entry_safe(peer, tmp, &ab->dp->peers, list) {
			if (!peer->sta || !peer->vif)
				continue;

			/* Allow sending disassoc to legacy peer
			 * only for asserted radio
			 */
			if (!peer->mlo && ab != assert_ab)
				continue;

			sta = peer->sta;
			ahsta = (struct ath12k_sta *)sta->drv_priv;

			/* Send low ack to disassoc the MLD station
			 * Need to check on the sequence as FW has
			 * discarded the management packet at this
			 * sequence.
			 */
			ath12k_mac_peer_disassoc(ab, sta, ahsta,
						 ATH12K_DBG_MODE1_RECOVERY);
		}
		spin_unlock_bh(&ab->dp->dp_lock);
	}
}

/* Wrapper function for recovery after crash
 * This recovery function will be called for
 * both Mode 1 and Mode 2. Because both Mode
 * will recover only the crashed radio
 * without affecting the other active radio
 */
int ath12k_recovery_reconfig(struct ath12k_base *ab)
{
	struct ath12k *ar = NULL;
	struct ath12k_pdev *pdev;
	struct ath12k_link_vif *arvif, *tmp;
	struct ath12k_vif *ahvif ;
	struct ieee80211_bss_conf *link;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_base *partner_ab;
	struct ath12k_hw *ah = ath12k_ag_to_ah(ag, 0);
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp *dp;
	struct ieee80211_key_conf *key;
	struct cfg80211_chan_def def;
	int i, j, key_idx;
	int ret = -EINVAL;
	bool is_bridge_vdev;

	wiphy_lock(ah->hw->wiphy);

	if (!ath12k_ftm_mode) {
		ret = ath12k_mac_op_start(ah->hw);
		if (ret) {
			ath12k_err(ab, "mac radio start failed\n");
			wiphy_unlock(ah->hw->wiphy);
			return ret;
		}
	}

	/* add chanctx/hw_config/filter part */
	for (j = 0; j < ab->num_radios; j++) {
		pdev = &ab->pdevs[j];
		ar = pdev->ar;

		if (!ar || ar->ab->is_bypassed)
			continue;

		list_for_each_entry_safe_reverse(arvif, tmp, &ar->arvifs, list) {
			ahvif = arvif->ahvif;

			is_bridge_vdev = ath12k_mac_is_bridge_vdev(arvif);

			if (!ahvif)
				continue;

			if (!is_bridge_vdev && !arvif->chanctx.def.chan)
				continue;

			arvif->is_started = false;
			arvif->is_created = false;

			if (is_bridge_vdev ||
			    ath12k_mac_vif_link_chan(ahvif->vif, arvif->link_id, &def))
				continue;

			spin_lock_bh(&ar->data_lock);
			ar->rx_channel = def.chan;
			spin_unlock_bh(&ar->data_lock);

                        /* configure filter - we can use the same flag*/
		}
	}

	/* assign chanctx part */
	for (j = 0; j < ab->num_radios; j++) {
		pdev = &ab->pdevs[j];
		ar = pdev->ar;

		if (!ar || ar->ab->is_bypassed)
			continue;

		list_for_each_entry_safe_reverse(arvif, tmp, &ar->arvifs, list) {
			ahvif = arvif->ahvif;
			is_bridge_vdev = ath12k_mac_is_bridge_vdev(arvif);

			if (!ahvif)
				continue;

			if (is_bridge_vdev) {
				link = NULL;
			} else {
				rcu_read_lock();
				link = rcu_dereference(ahvif->vif->link_conf[arvif->link_id]);

				/* Not expected */
				if (WARN_ON(!link)) {
					rcu_read_unlock();
					continue;
				}
				rcu_read_unlock();
			}
			ret = ath12k_mlo_core_recovery_reconfig_link_bss(ar, link, ahvif, arvif);

			if (ret) {
				ath12k_err(ab, "ERROR in reconfig link:%d\n", ret);
				wiphy_unlock(ah->hw->wiphy);
                                return ret;
			}
			ath12k_dbg(ab, ATH12K_DBG_MODE1_RECOVERY,
				   "vdev_created getting incremented:%d\n",
				   hweight16(ahvif->links_map));
		}
		ath12k_dbg(ab, ATH12K_DBG_MODE1_RECOVERY, "assign chanctx is completed\n");
	}

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		if (partner_ab->is_bypassed)
			continue;
		clear_bit(ATH12K_FLAG_UMAC_RECOVERY_START, &partner_ab->dev_flags);
	}

	/* reconfig_link_bss */
	for (j = 0; j < ab->num_radios; j++) {
		pdev = &ab->pdevs[j];
		ar = pdev->ar;

		if (!ar || ar->ab->is_bypassed)
			continue;

		list_for_each_entry_safe_reverse(arvif, tmp, &ar->arvifs, list) {
			ahvif = arvif->ahvif;
			is_bridge_vdev = ath12k_mac_is_bridge_vdev(arvif);

			if (!ahvif)
				continue;

			if (!is_bridge_vdev && !arvif->chanctx.def.chan)
				continue;

			if (is_bridge_vdev) {
				switch (ahvif->vdev_type) {
				case WMI_VDEV_TYPE_AP:
					ath12k_mac_bridge_vdev_up(arvif);
					break;
				case WMI_VDEV_TYPE_STA:
					link = NULL;
					goto skip_link_info;
				default:
					break;
				}
				continue;
			}

			rcu_read_lock();
			link = rcu_dereference(ahvif->vif->link_conf[arvif->link_id]);

			/* Not expected */
			if (WARN_ON(!link)) {
				rcu_read_unlock();
				continue;
			}
			rcu_read_unlock();

skip_link_info:
			ret = ath12k_mlo_recovery_link_vif_reconfig(ar, ahvif,
								    arvif,
								    arvif->ahvif->vif,
								    link);
			if (ret) {
				ath12k_err(ab, "Failed to update reconfig_bss\n");
				wiphy_unlock(ah->hw->wiphy);
				return ret;
			}
		}
	}

	/* TODO: Need to check for STA BVAP case */
	/* recover station VIF enabled in non-asserted links */
	ath12k_core_mlo_recover_station(ag, ab);

	/* sta state part */
	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		if (partner_ab->is_bypassed)
			continue;

		if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE2)
			continue;

		for (j = 0; j < partner_ab->num_radios; j++) {
			pdev = &partner_ab->pdevs[j];
			ar = pdev->ar;

			if (!ar)
				continue;

			if (list_empty(&ar->arvifs))
				continue;

			list_for_each_entry_safe_reverse(arvif, tmp, &ar->arvifs, list) {
				ahvif = arvif->ahvif;

				if (!ahvif)
					continue;

				if (ahvif->vdev_type != WMI_VDEV_TYPE_STA)
					ath12k_core_iterate_sta_list(ar, arvif);

				if (ath12k_mac_is_bridge_vdev(arvif))
					continue;

				dp = ath12k_ab_to_dp(partner_ab);
				spin_lock_bh(&dp->dp_lock);
				peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, arvif->vdev_id, arvif->bssid);
				if (!peer) {
					ath12k_info(ab, "Failed to fetch the peer during reconfig\n");
					spin_unlock_bh(&dp->dp_lock);
					continue;
				}
				spin_unlock_bh(&dp->dp_lock);

				for (key_idx = 0; key_idx < WMI_MAX_KEY_INDEX; key_idx++) {
					key = arvif->keys[key_idx];
					if (key) {
						ath12k_dbg(ab, ATH12K_DBG_MODE1_RECOVERY,
								"key:%p cipher:%d idx:%d flags:%d\n",
								key, key->cipher, key->keyidx, key->flags);
						ret = ath12k_mac_set_key(arvif->ar, SET_KEY, arvif, NULL, key);
					}
				}
			}
		}
	}

	ath12k_mac_reconfig_complete(ah->hw, IEEE80211_RECONFIG_TYPE_RESTART);

	/* Send disassoc to MLD STA */
	ath12k_core_peer_disassoc(ag, ab);
	ab->recovery_start = false;
	ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE0;
	ath12k_info(ab, "Mode1 recovery completed\n");
	wiphy_unlock(ah->hw->wiphy);
	return ret;
}

/* this recovery work is called for both
 * mode 1 and mode 2 during the recovery.
 */
static void ath12k_core_recovery_work(struct work_struct *work)
{
	struct ath12k_base *ab = container_of(work, struct ath12k_base, recovery_work);
	if (ab->is_bypassed)
		return;

	ath12k_info(ab, "queued recovery work\n");
	ath12k_recovery_reconfig(ab);
}

void ath12k_core_trigger_bug_on(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	int dump_count;

	/* Crash the system once all the stats are dumped */
	if (ab->in_panic)
		return;

	if (ag->mlo_capable) {
		dump_count = atomic_read(&ath12k_coredump_ram_info.num_chip);
		if (dump_count >= ATH12K_MAX_SOCS) {
			ath12k_err(ab, "invalid chip number %d\n",
					dump_count);
			return;
		}
	}

	atomic_inc(&ath12k_coredump_ram_info.num_chip);
	ath12k_core_issue_bug_on(ab);
}

static void ath12k_core_update_userpd_state(struct work_struct *work)
{
	struct ath12k_hw_group *ag = container_of(work, struct ath12k_hw_group, reset_group_work);
	struct ath12k_base *ab;
	struct ath12k_ahb *ab_ahb = NULL;
	int i;

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];

		if (ab->is_bypassed)
			continue;

		if (ab->hif.bus == ATH12K_BUS_AHB || ab->hif.bus == ATH12K_BUS_HYBRID) {
			ath12k_hal_dump_srng_stats(ab);
			ab_ahb = ath12k_ab_to_ahb(ab);

			ab_ahb->crash_type = ATH12K_RPROC_ROOTPD_CRASH;
			set_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags);

			if (!ab->is_reset)
				ath12k_hif_irq_disable(ab);

			if (ab->fw_recovery_support &&
			    !test_bit(ATH12K_GROUP_FLAG_UNREGISTER, &ag->flags))
				set_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags);
			else
				ath12k_core_trigger_bug_on(ab);
		}
	}
}

/* Asserted target's reboot handling for crash type ATH12K_RPROC_USERPD_CRASH */
static void ath12k_core_upd_power_down(struct ath12k_base *ab)
{
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);

	/*
	 * Stop user pd
	 * Collect coredump using user pd
	 */
	if (ab_ahb->crash_type == ATH12K_RPROC_USERPD_CRASH) {
		ath12k_hif_power_down(ab, false);
		ath12k_coredump_ahb_collect(ab);
	}

	ab_ahb->crash_type = ATH12K_NO_CRASH;
}

/*
 * Trigger umac_reset with umac_reset flag set. This is a
 * waiting function which will return only after UMAC reset
 * is complete on non-asserted chip set. UMAC reset completion
 * is identified by waiting for MLO Teardown complete for all
 * chipsets
 */

static int ath12k_core_trigger_umac_reset(struct ath12k_base *ab,
					  enum wmi_mlo_tear_down_reason_code_type reason_code)
{
	struct ath12k_hw_group *ag = ab->ag;
	long time_left;
	int ret = 0;

	reinit_completion(&ag->umac_reset_complete);

	ath12k_mac_mlo_teardown_with_umac_reset(ab, reason_code);

	time_left = wait_for_completion_timeout(&ag->umac_reset_complete,
			msecs_to_jiffies(ATH12K_UMAC_RESET_TIMEOUT_IN_MS));

	if (!time_left) {
		ath12k_warn(ab, "UMAC reset didn't get completed within %d ms\n", ATH12K_UMAC_RESET_TIMEOUT_IN_MS);
		ret = -ETIMEDOUT;
	}

	ag->trigger_umac_reset = false;
	return ret;
}

void ath12k_core_trigger_partner_device_crash(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_ahb *ab_ahb = NULL;
	struct ath12k_base *partner_ab;
	int i;

	lockdep_assert_held(&ag->mutex);

	if (ab->hif.bus == ATH12K_BUS_AHB || ab->hif.bus == ATH12K_BUS_HYBRID)
		ab_ahb = ath12k_ab_to_ahb(ab);

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		if (partner_ab->is_bypassed || ab == partner_ab)
			continue;

		/* If the partner chip is either AHB/Hybrid
		 * and if it is a rootpd crash then the userpd
		 * won't be responding to the FW Hang command
		 * So skip to send it for AHB or Hybrid SOC's
		 * in case of a rootpd crash as queueing this
		 * reset_work will be taken care of AHB/Hybrid
		 * inside ath12k_ahb_queue_all_userpd_reset().
		 */
		if (partner_ab->hif.bus != ATH12K_BUS_PCI && ab_ahb
				&& ab_ahb->crash_type == ATH12K_RPROC_ROOTPD_CRASH)
			continue;

		/* issue FW Hang command on partner chips for Mode0. This is a fool proof
		 * method to ensure recovery of all partner chips in MODE0 instead of
		 * relying on firmware to crash partner chips
		 */
		if (!test_bit(ATH12K_FLAG_RECOVERY, &partner_ab->dev_flags)) {
			ath12k_info(ab, "sending fw_hang cmd to partner chipset(s)\n");
			set_bit(ATH12K_FLAG_RECOVERY, &partner_ab->dev_flags);
			clear_bit(ATH12K_FLAG_UMAC_RECOVERY_START, &partner_ab->dev_flags);
			partner_ab->qmi.num_radios = U8_MAX;
			ath12k_wmi_force_fw_hang_cmd(partner_ab->pdevs[0].ar,
					ATH12K_WMI_FW_HANG_ASSERT_TYPE,
					ATH12K_WMI_FW_HANG_DELAY, true);
		}
	}
}

static void ath12k_partner_chip_power_state_info(struct ath12k_hw_group *ag,
						 u8 power_state)
{
	struct ath12k_base *ab;
	int i, ret;

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];

		if (ab->is_reset)
			continue;

		ret = ath12k_qmi_partner_chip_power_info_send(ab, power_state);

		if (ret < 0)
			ath12k_err(ab, "Failed to send the power state for the chip\n");
	}
}

static void ath12k_core_disable_ext_irq_during_recovery(struct ath12k_base *ab)
{
	struct ath12k_ahb *ab_ahb;

	if (!ab->is_reset) {
		/* Disable IRQs only for PCI bus and AHB bus in case of userPD crash
		 * IRQs will be disabled for AHB rootPD crash from rootPD crash notifier
		 */
		switch (ab->hif.bus) {
		case ATH12K_BUS_AHB:
		case ATH12K_BUS_HYBRID:
			ab_ahb = ath12k_ab_to_ahb(ab);

			if (ab_ahb->crash_type == ATH12K_RPROC_ROOTPD_CRASH)
				break;
			fallthrough;
		case ATH12K_BUS_PCI:
			ath12k_hif_irq_disable(ab);
			break;
		}
	}
}

static void ath12k_core_reset(struct work_struct *work)
{
	struct ath12k_base *partner_ab, *ab = container_of(work, struct ath12k_base, reset_work);
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_hw *ah;
	int reset_count, fail_cont_count, i;
	long time_left;

	if (!(test_bit(ATH12K_FLAG_QMI_FW_READY_COMPLETE, &ab->dev_flags))) {
		ath12k_warn(ab, "ignore reset dev flags 0x%lx\n", ab->dev_flags);
		return;
	}

	ab->recovery_start = false;

	if (ab->recovery_mode_address) {
		switch (*ab->recovery_mode_address) {
		case ATH12K_MLO_RECOVERY_MODE2:
			ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE2;
			break;
		case ATH12K_MLO_RECOVERY_MODE1:
			ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE1;
			break;
		case ATH12K_MLO_RECOVERY_MODE0:
			fallthrough;
		default:
			ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE0;
		}

		ath12k_dbg(ab, ATH12K_DBG_MODE1_RECOVERY,"mode:%d\n", ag->recovery_mode);
	} else {
		ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE0;
	}

	if (ath12k_check_erp_power_down(ag))
		ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE0;

	if (ab->fw_recovery_support)
		ath12k_info(ab, "Recovery is initiated with Mode%d\n",
				ag->recovery_mode - 1);

	/* Sometimes the recovery will fail and then the next all recovery fail,
	 * this is to avoid infinite recovery since it can not recovery success
	 */
	fail_cont_count = atomic_read(&ab->fail_cont_count);

	if (fail_cont_count >= ATH12K_RESET_MAX_FAIL_COUNT_FINAL) {
		ath12k_warn(ab, "Recovery Failed, Fail count:%d MAX_FAIL_COUNT final:%d\n",
				fail_cont_count,
				ATH12K_RESET_MAX_FAIL_COUNT_FINAL);
		return;
	}

	if (fail_cont_count >= ATH12K_RESET_MAX_FAIL_COUNT_FIRST &&
	    time_before(jiffies, ab->reset_fail_timeout)) {
		ath12k_warn(ab, "Recovery Failed, Fail count:%d MAX_FAIL_COUNT first:%d\n",
				fail_cont_count,
				ATH12K_RESET_MAX_FAIL_COUNT_FIRST);
		return;
	}

	reset_count = atomic_inc_return(&ab->reset_count);

#ifdef CPTCFG_ATHDEBUG
	if (ab) {
		ath12k_info(ab, "%s : collect minidump\n", __func__);
		athdbg_if_get_service(ab, ATHDBG_SRV_COLLECT_MINIDUMP_REFERENCES);
		if (ab->fw_recovery_support)
			athdbg_if_get_service(ab, ATHDBG_SRV_DO_MINIDUMP);
	}
#endif

	if (reset_count > 1) {
		/* Sometimes it happened another reset worker before the previous one
		 * completed, then the second reset worker will destroy the previous one,
		 * thus below is to avoid that.
		 */
		ath12k_warn(ab, "already resetting count %d\n", reset_count);

		/* Need to collect the UserPD dumps during the subsequent FW crash since
		 * crashed UserPD will get cleared during BUG_ON
		 */
		if (ab->hif.bus != ATH12K_BUS_PCI) {
			ath12k_info(ab, "Collecting the userpd dumps before full crash\n");
			if (!ab->pm_suspend)
				ath12k_core_upd_power_down(ab);
		}

		if (ath12k_ssr_failsafe_mode && ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE0)
			BUG_ON(1);

		reinit_completion(&ab->reset_complete);
		time_left = wait_for_completion_timeout(&ab->reset_complete,
							ATH12K_RESET_TIMEOUT_HZ);
		if (time_left) {
			ath12k_dbg(ab, ATH12K_DBG_BOOT, "to skip reset\n");
			atomic_dec(&ab->reset_count);
			return;
		}

		ab->reset_fail_timeout = jiffies + ATH12K_RESET_FAIL_TIMEOUT_HZ;
		/* Record the continuous recovery fail count when recovery failed*/
		fail_cont_count = atomic_inc_return(&ab->fail_cont_count);
	}

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "reset starting\n");

	mutex_lock(&ag->mutex);

	ath12k_core_disable_ext_irq_during_recovery(ab);
	ath12k_hif_ce_irq_disable(ab);
	ab->is_reset = true;

	if (ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE0)
		ath12k_core_mlo_hw_queues_stop(ab->ag);

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		if (partner_ab->is_bypassed || ab == partner_ab)
			continue;

		/* Need to check partner_ab flag to select recovery mode
		 * as Mode0, if continuous reset has happened
		 */

		if (ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE0) {
			if (test_bit(ATH12K_FLAG_UMAC_RECOVERY_START, &partner_ab->dev_flags) ||
			    test_bit(ATH12K_FLAG_RECOVERY, &partner_ab->dev_flags)) {
				/* On receiving MHI Interrupt for pdev which is
				 * already in UMAC Recovery, then fallback to
				 * MODE0
				 */
				ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE0;
                        	ath12k_info(ab, "Recovery is falling back to Mode0 as one of the partner chip is already in recovery\n");
				break;
			} else {
				/* Set dev flags to UMAC recovery START
				 * and set flag to send teardown later
				 */
				ath12k_info(ab, "setting teardown to true\n");
				set_bit(ATH12K_FLAG_UMAC_RECOVERY_START, &partner_ab->dev_flags);
                        }
                }
        }

	/* UMAC RESET relies on ag->num_started as barrier to make sure
	 * umac related interrupts are received from all non-asserted chips
	 * before writing to the shared memory. So need to decrement it
	 * when recovery is enabled before triggering umac reset.
	 */
	if (ab->fw_recovery_support)
		ath12k_core_to_group_ref_put(ab);

	if (ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE0) {
		if (ath12k_core_trigger_umac_reset(ab, WMI_MLO_TEARDOWN_SSR_REASON) ||
		    ath12k_mac_partner_peer_cleanup(ab)) {
			/* Fallback to Mode0 if umac reset/peer_cleanup is
			 * failed
			 */
			ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE0;
			/* TODO: DS: Handle any clean up necessary for Mode1 SSR */
			ath12k_info(ab, "Recovery is falling back to Mode0\n");
		} else {
			/* wake queues here as ping should continue for
			 * legacy clients in non-asserted chipsets
			 */
			ath12k_core_peer_disassoc(ag, ab);
			for (i = 0; i < ag->num_hw; i++) {
				ah = ag->ah[i];
				if (!ah)
					continue;

				ieee80211_wake_queues(ah->hw);
			}
			ath12k_dbg(ab, ATH12K_DBG_MODE1_RECOVERY,
					"Queues are started as umac reset is completed for partner chipset\n");
		}
	}

	if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE0 &&
	    !ath12k_check_erp_power_down(ag))
		ath12k_core_trigger_partner_device_crash(ab);

	/* prepare coredump */
	if (ab->hif.bus == ATH12K_BUS_PCI) {
		ath12k_coredump_download_rddm(ab);
	} else if ((ab->hif.bus == ATH12K_BUS_AHB || ab->hif.bus == ATH12K_BUS_HYBRID) &&
		   !ab->fw_recovery_support) {
		ath12k_core_trigger_bug_on(ab);
	}

	atomic_set(&ab->recovery_count, 0);

	ath12k_coredump_collect(ab);

	ath12k_core_pre_reconfigure_recovery(ab);

	ath12k_core_post_reconfigure_recovery(ab);

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "waiting recovery start...\n");

	if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE2)
		ath12k_partner_chip_power_state_info(ag, FW_ASSERTED_CHIP_PWR_DOWN);

	if (ab->fw_recovery_support) {

		if (ab->hif.bus == ATH12K_BUS_PCI) {
			ath12k_hif_power_down(ab, false);
		} else {
			if (!ab->pm_suspend)
				ath12k_core_upd_power_down(ab);
		}
	}

	/* prepare for power up */
	ab->qmi.num_radios = U8_MAX;
	//ab->single_chip_mlo_supp = false; TODO need to revisit

	if (ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE0)
		ab->recovery_start = true;

	ab->recovery_mode_address = NULL;
	ab->crash_info_address = NULL;

	if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE0 && ag->num_started > 0) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "waiting for %d partner device(s) to reset\n",
			   ag->num_started);
		mutex_unlock(&ag->mutex);
		return;
	}

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if ((!ab->is_reset &&
		    !ath12k_check_erp_power_down(ag)) ||
		    ab->is_bypassed)
			continue;

		ath12k_qmi_free_resource(ab);
		ath12k_hif_power_up(ab);

		if (ath12k_check_erp_power_down(ag)) {
			ab->pm_suspend = false;
			ab->powerup_triggered = true;
		}

		if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE2)
			ath12k_partner_chip_power_state_info(ag,
							     FW_ASSERTED_CHIP_PWR_UP);

		ath12k_dbg(ab, ATH12K_DBG_BOOT, "reset started\n");
	}

	mutex_unlock(&ag->mutex);
}

static int ath12k_core_panic_handler(struct notifier_block *nb,
				     unsigned long action, void *data)
{
	struct ath12k_base *ab = container_of(nb, struct ath12k_base,
					      panic_nb);

	if (!ab)
		return 0;
#ifdef CPTCFG_ATHDEBUG
	athdbg_if_get_service(ab, ATHDBG_SRV_COLLECT_MINIDUMP_REFERENCES);
#endif
	if (ab->in_panic)
		goto panic_handler;
	
	ab->in_panic = true;

	if (ab->hif.bus == ATH12K_BUS_PCI)
		ath12k_coredump_download_rddm(ab);
	else
		atomic_inc(&ath12k_coredump_ram_info.num_chip);

panic_handler:
	return ath12k_hif_panic_handler(ab);
}

static int ath12k_core_panic_notifier_register(struct ath12k_base *ab)
{
	ab->panic_nb.notifier_call = ath12k_core_panic_handler;

	return atomic_notifier_chain_register(&panic_notifier_list,
					      &ab->panic_nb);
}

void ath12k_core_panic_notifier_unregister(struct ath12k_base *ab)
{
	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &ab->panic_nb);
}

static inline
bool ath12k_core_hw_group_create_ready(struct ath12k_hw_group *ag)
{
	lockdep_assert_held(&ag->mutex);

	return (ag->num_probed == ag->num_devices);
}

static struct ath12k_hw_group *ath12k_core_hw_group_alloc(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag;
	int count = 0;

	lockdep_assert_held(&ath12k_hw_group_mutex);

	list_for_each_entry(ag, &ath12k_hw_group_list, list)
		count++;

	ag = kzalloc(sizeof(*ag), GFP_KERNEL);
	if (!ag)
		return NULL;

	ag->dp_hw_grp = ath12k_core_dp_hw_group_alloc(ab->dp);
	if (!ag->dp_hw_grp) {
		kfree(ag);
		return NULL;
	}

	ag->id = count;
#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
	ag->dbs_power_reduction = ATH12K_DEFAULT_POWER_REDUCTION;
	ag->eth_power_reduction = ATH12K_DEFAULT_POWER_REDUCTION;
#endif
	list_add(&ag->list, &ath12k_hw_group_list);
	INIT_WORK(&ag->reset_group_work, ath12k_core_update_userpd_state);
	mutex_init(&ag->mutex);
	init_completion(&ag->umac_reset_complete);
	init_completion(&ag->peer_cleanup_complete);
	ag->mlo_capable = false;
	ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE0;
	ag->wsi_load_info = NULL;
	ag->wsi_peer_clean_timeout = ATH12K_MAC_PEER_CLEANUP_TIMEOUT_MSECS;

#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
	ath12k_global_ps_ctx.ag = ag;
#endif
	return ag;
}

static void ath12k_core_hw_group_free(struct ath12k_hw_group *ag)
{
	mutex_lock(&ath12k_hw_group_mutex);

	list_del(&ag->list);
	kfree(ag->dp_hw_grp);
	kfree(ag);

	mutex_unlock(&ath12k_hw_group_mutex);
}

static struct ath12k_hw_group *ath12k_core_hw_group_find_by_dt(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag;
	int i;

	if (!ab->dev->of_node)
		return NULL;

	list_for_each_entry(ag, &ath12k_hw_group_list, list)
		for (i = 0; i < ag->num_devices; i++)
			if (ag->wsi_node[i] == ab->dev->of_node)
				return ag;

	return NULL;
}

static bool
ath12k_core_check_is_bypassed(struct ath12k_hw_group *ag, struct device_node *connected_dev) {
	int i;
	struct ath12k_base *ab;

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];

		if (ab->dev->of_node == connected_dev && ab->is_bypassed)
			return true;
	}

	return false;
}

/*
@reg - use reg as 0 to get the right side neighbour (Connected to TX port),
and 1 to get left side neighbour (Connected to RX port).
*/

static struct device_node *
ath12k_get_connected_dev(struct ath12k_base *ab, int reg,
			 bool include_bypassed_device) {

	struct device_node *endpoint, *next_endpoint, *connected_dev;
	struct device_node *ab_dev = ab->dev->of_node;

	connected_dev = ab_dev;

	do {
		endpoint = of_graph_get_endpoint_by_regs(connected_dev, reg, -1);

		if (!endpoint)
			return NULL;

		next_endpoint = of_graph_get_remote_endpoint(endpoint);
		if (!next_endpoint) {
			of_node_put(next_endpoint);
			return NULL;
		}

		of_node_put(endpoint);

		connected_dev = of_graph_get_port_parent(next_endpoint);

		if (!connected_dev) {
			of_node_put(next_endpoint);
			return NULL;
		}

		of_node_put(next_endpoint);
		of_node_put(connected_dev);

		if (!include_bypassed_device ||
		    !ath12k_core_check_is_bypassed(ab->ag, connected_dev))
			return connected_dev;

	} while (connected_dev != ab_dev);

	return NULL;
}

static int ath12k_core_get_wsi_info(struct ath12k_hw_group *ag,
				    struct ath12k_base *ab)
{
	struct device_node *wsi_dev = ab->dev->of_node, *next_wsi_dev;
	struct device_node *tx_endpoint, *next_rx_endpoint;
	int device_count = 0;

	next_wsi_dev = wsi_dev;

	if (!next_wsi_dev)
		return -ENODEV;

	do {
		if (device_count >= ATH12K_MAX_SOCS) {
			ath12k_warn(ab, "device count in DT %d has reached or exceeded the limit %d\n",
				    device_count, ATH12K_MAX_SOCS);
			of_node_put(next_wsi_dev);
			return -EINVAL;
		}
		ag->wsi_node[device_count] = next_wsi_dev;

		tx_endpoint = of_graph_get_endpoint_by_regs(next_wsi_dev, 0, -1);

		if (!tx_endpoint) {
			of_node_put(next_wsi_dev);
			return -ENODEV;
		}

		next_rx_endpoint = of_graph_get_remote_endpoint(tx_endpoint);
		if (!next_rx_endpoint) {
 			of_node_put(next_wsi_dev);
			of_node_put(tx_endpoint);
			return -ENODEV;
		}

		next_wsi_dev = of_graph_get_port_parent(next_rx_endpoint);
		if (!next_wsi_dev) {
			of_node_put(next_wsi_dev);
			return -ENODEV;
		}

		of_node_put(tx_endpoint);
		of_node_put(next_wsi_dev);

		device_count++;
	} while (wsi_dev != next_wsi_dev);

	of_node_put(next_wsi_dev);
	ag->num_devices = device_count;

	return 0;
}

/* If one of the device is bypassed, use bypass_wsi_info */
struct ath12k_wsi_info
*ath12k_core_get_current_wsi_info(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;

	if (ag->num_bypassed > 0)
		return &ab->bypass_wsi_info;
	else
		return &ab->wsi_info;
}

static void ath12k_core_fill_adj_info(struct ath12k_base *ab)
{
	struct device_node* ab_dev = ab->dev->of_node;
	struct device_node *tx_neighbour, *rx_neighbour;
	struct ath12k_hw_group *ag = ab->ag;
	int num_adj_chips = 0, i = 0;
	u8 adj_device_idx_bmp = 0;
	struct ath12k_wsi_info *wsi_info, *adj_wsi_info;
	struct ath12k_base *partner_ab;
	bool is_bypassed = ag->num_bypassed ? true : false;

	wsi_info = ath12k_core_get_current_wsi_info(ab);

	tx_neighbour = ath12k_get_connected_dev(ab, 0, is_bypassed);

	if (!tx_neighbour) {
		of_node_put(ab_dev);
		return;
	}

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		adj_wsi_info = ath12k_core_get_current_wsi_info(partner_ab);
		if (tx_neighbour == partner_ab->dev->of_node) {
			wsi_info->adj_chip_idxs[num_adj_chips] = adj_wsi_info->index;
			adj_device_idx_bmp |= BIT(wsi_info->adj_chip_idxs[num_adj_chips]);
			break;
		}
	}

	num_adj_chips++;

	rx_neighbour = ath12k_get_connected_dev(ab, 1, is_bypassed);
	if (!rx_neighbour) {
		of_node_put(ab_dev);
		return;
	}

	if (tx_neighbour != rx_neighbour) {

		for (i = 0; i < ag->num_devices; i++) {
			partner_ab = ag->ab[i];
			adj_wsi_info = ath12k_core_get_current_wsi_info(partner_ab);
			if (rx_neighbour == partner_ab->dev->of_node) {
				wsi_info->adj_chip_idxs[num_adj_chips] = adj_wsi_info->index;
				adj_device_idx_bmp |= BIT(wsi_info->adj_chip_idxs[num_adj_chips]);
				break;
			}
		}
		num_adj_chips++;
	}

	ath12k_dbg(ab, ATH12K_DBG_QMI, "Adj chip info:\n");
	ath12k_dbg(ab, ATH12K_DBG_QMI, "num_adj_chips: %d\n", num_adj_chips);
	for (i = 0; i < num_adj_chips; i++)
		ath12k_dbg(ab, ATH12K_DBG_QMI, "[%d] : %d", i, ab->wsi_info.adj_chip_idxs[i]);

	of_node_put(tx_neighbour);
	of_node_put(rx_neighbour);
	of_node_put(ab_dev);

	wsi_info->num_adj_chips = num_adj_chips;
	for (i = 0; i < ag->num_devices && adj_device_idx_bmp; i++) {
		if ((adj_device_idx_bmp & BIT(i)) ||
		    (i == wsi_info->index))
			continue;
		wsi_info->diag_device_idx_bmap |= BIT(i);
 	}
}

static int ath12k_core_get_wsi_index(struct ath12k_hw_group *ag,
				     struct ath12k_base *ab)
{
	int i, wsi_controller_index = -1, node_index = -1;
	bool control;

	for (i = 0; i < ag->num_devices; i++) {
		control = of_property_read_bool(ag->wsi_node[i], "qcom,wsi-controller");
		if (control)
			wsi_controller_index = i;

		if (ag->wsi_node[i] == ab->dev->of_node)
			node_index = i;
	}

	if (wsi_controller_index == -1) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "wsi controller is not defined in dt");
		return -EINVAL;
	}

	if (node_index == -1) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "unable to get WSI node index");
		return -EINVAL;
	}

	ab->wsi_info.index = (ag->num_devices + node_index - wsi_controller_index) %
		ag->num_devices;
	ab->bypass_wsi_info.index = (ag->num_devices + node_index - wsi_controller_index) %
		ag->num_devices;

	return 0;
}

static struct ath12k_hw_group *ath12k_core_hw_group_assign(struct ath12k_base *ab)
{
	struct ath12k_wsi_info *wsi = &ab->wsi_info;
	struct ath12k_hw_group *ag;

	lockdep_assert_held(&ath12k_hw_group_mutex);

	if (ath12k_ftm_mode || !ath12k_mlo_capable)
		goto invalid_group;

	/* The grouping of multiple devices will be done based on device tree file.
	 * The platforms that do not have any valid group information would have
	 * each device to be part of its own invalid group.
	 *
	 * We use group id ATH12K_INVALID_GROUP_ID for single device group
	 * which didn't have dt entry or wrong dt entry, there could be many
	 * groups with same group id, i.e ATH12K_INVALID_GROUP_ID. So
	 * default group id of ATH12K_INVALID_GROUP_ID combined with
	 * num devices in ath12k_hw_group determines if the group is
	 * multi device or single device group
	 */

	ag = ath12k_core_hw_group_find_by_dt(ab);
	if (!ag) {
		ag = ath12k_core_hw_group_alloc(ab);
		if (!ag) {
			ath12k_warn(ab, "unable to create new hw group\n");
			return NULL;
		}

		if (ath12k_core_get_wsi_info(ag, ab) ||
		    ath12k_core_get_wsi_index(ag, ab)) {
			ath12k_dbg(ab, ATH12K_DBG_BOOT,
				   "unable to get wsi info from dt, grouping single device");
			ag->id = ATH12K_INVALID_GROUP_ID;
			ag->num_devices = 1;
			memset(ag->wsi_node, 0, sizeof(ag->wsi_node));
			wsi->index = 0;
		}

		goto exit;
	} else if (test_bit(ATH12K_GROUP_FLAG_UNREGISTER, &ag->flags)) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "group id %d in unregister state\n",
			   ag->id);
		goto invalid_group;
	} else {
		if (ath12k_core_get_wsi_index(ag, ab))
			goto invalid_group;
		goto exit;
	}

invalid_group:
	ag = ath12k_core_hw_group_alloc(ab);
	if (!ag) {
		ath12k_warn(ab, "unable to create new hw group\n");
		return NULL;
	}

	ag->id = ATH12K_INVALID_GROUP_ID;
	ag->num_devices = 1;
	wsi->index = 0;

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "single device added to hardware group\n");

exit:
	if (ag->num_probed >= ag->num_devices) {
		ath12k_warn(ab, "unable to add new device to group, max limit reached\n");
		goto invalid_group;
	}

	ab->device_id = ag->num_probed++;

	if (ag->id != ATH12K_INVALID_GROUP_ID)
		ab->is_static_bypassed = (ath12k_wsi_bypass_bmap & (1 << wsi->index));
	else if (ath12k_wsi_bypass_bmap)
		ath12k_warn(ab, "single device does not support static WSI bypass\n");

	ag->ab[ab->device_id] = ab;
	ab->ag = ag;

	ath12k_dp_cmn_hw_group_assign(ath12k_ab_to_dp(ab), ag);

	if (ath12k_wsi_load_info_init(ab))
		ath12k_err(ab, "failed to initialize wsi load info\n");

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "wsi group-id %d num-devices %d index %d",
		   ag->id, ag->num_devices, wsi->index);

	/* stats context Initialization */
	wiphy_work_init(&ag->stats_work.stats_nb_work,
			ath12k_stats_event_work_handler);
	spin_lock_init(&ag->stats_work.list_lock);
	INIT_LIST_HEAD(&ag->stats_work.work_list);

	return ag;
}

static void ath12k_update_mlo_adj_chip(struct ath12k_hw_group *ag)
{
	int i;
	struct ath12k_base *partner_ab;

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		if (!partner_ab || partner_ab->is_bypassed)
			continue;

		mutex_lock(&partner_ab->core_lock);
		ath12k_core_fill_adj_info(partner_ab);

		mutex_unlock(&partner_ab->core_lock);
	}
}

void ath12k_core_pci_link_speed(struct ath12k_base *ab, u16 link_speed,
				u16 link_width)
{
	struct ath12k_pci *ab_pci;
	struct pci_dev *root_port;

	if (ab->hif.bus != ATH12K_BUS_PCI)
		return;

	ab_pci = ath12k_pci_priv(ab);
	if (!ab_pci) {
		ath12k_err(ab, "Error in getting the pci_priv\n");
		return;
	}

	if (link_speed > ab_pci->def_link_speed)
		link_speed = ab_pci->def_link_speed;

	if (link_width > ab_pci->def_link_width)
		link_width = ab_pci->def_link_width;

	root_port = pcie_find_root_port(ab_pci->pdev);

	if (!root_port) {
		ath12k_err(ab, "Error in getting the root port\n");
		return;
	}

	ath12k_info(ab, "Link speed is %d and width is %d\n", link_speed, link_width);

	if (pcie_set_link_speed(root_port, link_speed))
		ath12k_err(ab, "Failed to set the link speed\n");

	if (pcie_set_link_width(root_port, link_width))
		ath12k_err(ab, "Failed to set the link width\n");
}

static int ath12k_core_wsi_remap_pdev_suspend(struct ath12k_base *ab)
{
	int i, ret = 0;
	struct ath12k *ar;

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		if (!ar) {
			ath12k_err(ab, "Invalid Radio\n");
			return -EINVAL;
		}
		ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS, "WSI Bypass: PDEV suspend");
		ret = ath12k_mac_pdev_suspend(ar);
		if (ret) {
			ath12k_err(ab, "PDEV suspend failed for wsi remap: %d\n", ret);
			goto fail;
		}
	}

fail:
	return ret;
}

static void ath12k_core_wsi_remap_reset(struct ath12k_base *ab)
{
	mutex_lock(&ab->core_lock);
	ath12k_core_pdev_deinit(ab);
	ath12k_dp_arch_pdev_free(ab->dp);
	ath12k_ce_cleanup_pipes(ab);
	ath12k_wmi_detach(ab);
	mutex_unlock(&ab->core_lock);

	ath12k_dp_cmn_device_deinit(ab->dp);
	ath12k_hal_srng_deinit(ab);
	ath12k_dp_umac_reset_deinit(ab);
	ath12k_umac_reset_completion(ab);

}

int ath12k_core_dynamic_wsi_remap(struct ath12k_base *ab)
{
	int ret = 0, i;
	struct ath12k_hw *ah;
	struct ath12k_hw_group *ag;

	ag = ab->ag;

	ag->wsi_remap_in_progress = true;

	ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS, "WSI Bypass: MLO teardown with Umac reset");
	ath12k_core_mlo_hw_queues_stop(ab->ag);

	ret = ath12k_core_trigger_umac_reset(ab,
					     WMI_MLO_TEARDOWN_REASON_DYNAMIC_WSI_REMAP);
	if (ret) {
		/* We should not come here. Something wrong with MLO teardown.
		 * Crash the system to recover.
		 */
		ath12k_err(ab, "WSI Bypass: MLO teardown/UMAC reset failed with error %d",
			   ret);
		ath12k_err(ab, "WSI Bypass: Abort Bypass procedure! Reset the system...");
		BUG_ON(1);
	} else {
		for (i = 0; i < ag->num_hw; i++) {
			ah = ag->ah[i];
			if (!ah)
				continue;

			ieee80211_wake_queues(ah->hw);
		}
	}

	if (ab->wsi_remap_state == ATH12K_WSI_BYPASS_REMOVE_DEVICE) {
		/* Send WMI_PDEV_SUSPEND for the chip which is bypassed */
		ret = ath12k_core_wsi_remap_pdev_suspend(ab);
		if (ret) {
			ath12k_err(ab, "pdev suspend failed with error %d", ret);
			goto fail;
		}

		ab->is_bypassed = true;

		ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS, "WSI Bypass: Send Mode OFF for Q6");
		ath12k_qmi_firmware_stop(ab);
		ath12k_qmi_free_resource(ab);

		/* Clean up the states for bypassed chip */
		ath12k_core_wsi_remap_reset(ab);

		/* Update the counters in the hw group */
		ag->num_bypassed++;
		ath12k_core_to_group_ref_put(ab);
		ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS,
			   "WSI Bypass: num_bypassed: %d num_started: %d",
			   ag->num_bypassed, ag->num_started);

		/* Update the new adj chip info */
		ath12k_update_mlo_adj_chip(ag);

		mutex_lock(&ag->mutex);
		/* Trigger MLO reconfig QMI message to All active chips */
		ath12k_core_wsi_remap_mlo_reconfig(ag);

		/* Put PCIe link to low speed mode */
		ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS, "WSI Bypass: Configure PCIe link to low speed");
		ath12k_core_pci_link_speed(ab, 1, 1);

		ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS, "WSI Bypass: Initiate MLO setup after bypass");
		ath12k_core_mlo_setup(ag);
		mutex_unlock(&ag->mutex);
		/* Reset the flags */
		ag->wsi_remap_in_progress = false;
		ab->wsi_remap_state = 0;
		ath12k_info(ab, "WSI remap: Device bypass completed\n");
	} else if (ab->wsi_remap_state == ATH12K_WSI_BYPASS_ADD_DEVICE) {
		ab->is_bypassed = false;
		ag->num_bypassed--;

		ath12k_hif_irq_disable(ab);
		ath12k_hif_ce_irq_disable(ab);
		ath12k_dp_ppeds_interrupt_stop(ab);

		ath12k_hif_power_down(ab, false);
		/* Reset the PCIe link speed */
		ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS, "WSI Bypass: Reset PCIe link speed");
		ath12k_core_pci_link_speed(ab, 3, 2);

		/* Update the new adj chip info */
		ath12k_update_mlo_adj_chip(ag);

		/* Power on the Q6. After FW image load, FW will initiate
		 * QMI sequence. After the normal bootup sequence, the
		 * counter will be reset after WMI ready from the FW
		 */
		ret = ath12k_hal_srng_init(ab);
		if (ret) {
			ath12k_err(ab, "srng init failed %d\n", ret);
			return ret;
		}

		ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS, "WSI Bypass: Power on Q6");
		ret = ath12k_hif_power_up(ab);
		if (ret) {
			ath12k_err(ab, "failed to power up :%d\n", ret);
			goto fail;
		}

		ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS,
			   "WSI Bypass: num_bypassed %d num_started %d",
			   ag->num_bypassed, ag->num_started);
	}

fail:
	return ret;
}

void ath12k_core_hw_group_unassign(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ath12k_ab_to_ag(ab);
	u8 device_id = ab->device_id;
	int num_probed;

	if (!ag)
		return;

	if (ag->wsi_load_info) {
		ath12k_wsi_load_info_deinit(ab, ag->wsi_load_info);
		ag->wsi_load_info = NULL;
	}

	mutex_lock(&ag->mutex);

	if (WARN_ON(device_id >= ag->num_devices)) {
		mutex_unlock(&ag->mutex);
		return;
	}

	if (WARN_ON(ag->ab[device_id] != ab)) {
		mutex_unlock(&ag->mutex);
		return;
	}

	ath12k_dp_cmn_hw_group_unassign(ath12k_ab_to_dp(ab), ag);

	ag->ab[device_id] = NULL;
	ab->ag = NULL;
	ab->device_id = ATH12K_INVALID_DEVICE_ID;

	if (ag->num_probed)
		ag->num_probed--;

	num_probed = ag->num_probed;

	mutex_unlock(&ag->mutex);

	if (!num_probed)
		ath12k_core_hw_group_free(ag);
}

static void ath12k_core_hw_group_destroy(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab;
	int i;

	if (WARN_ON(!ag))
		return;

	mutex_lock(&ag->mutex);
	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab)
			continue;

		mutex_lock(&ab->core_lock);
		ath12k_core_soc_destroy(ab);
		mutex_unlock(&ab->core_lock);
		ath12k_core_panic_notifier_unregister(ab);
	}
	mutex_unlock(&ag->mutex);
}

static void ath12k_core_hw_group_cleanup(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab;
	int i;

	if (!ag)
		return;

	mutex_lock(&ag->mutex);

	if (!test_bit(ATH12K_GROUP_FLAG_REGISTERED, &ag->flags)) {
		mutex_unlock(&ag->mutex);
		return;
	}

	if (test_bit(ATH12K_GROUP_FLAG_UNREGISTER, &ag->flags)) {
		mutex_unlock(&ag->mutex);
		return;
	}

	ath12k_send_fw_hang_cmd(ag->ab[0], ATH12K_FW_RECOVERY_DISABLE);

	set_bit(ATH12K_GROUP_FLAG_UNREGISTER, &ag->flags);

	ath12k_core_hw_group_stop(ag);

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab)
			continue;

		if (!test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags) &&
		     test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags)) {
			if (ab->hif.bus == ATH12K_BUS_PCI)
				ath12k_coredump_download_rddm(ab);
		}

	}

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab || ab->is_bypassed)
			continue;

		mutex_lock(&ab->core_lock);
		ath12k_core_stop(ab);
		mutex_unlock(&ab->core_lock);
	}

	mutex_unlock(&ag->mutex);
}

static int ath12k_core_hw_group_create(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab;
	int i;

	lockdep_assert_held(&ag->mutex);

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab || ab->is_bypassed)
			continue;

		mutex_lock(&ab->core_lock);

		if (test_bit(ATH12K_FLAG_SOC_CREATE_FAIL, &ab->dev_flags)) {
			mutex_unlock(&ab->core_lock);
			return -ENODEV;
		}
		ath12k_core_fill_adj_info(ab);

		mutex_unlock(&ab->core_lock);

	}
	return 0;
}

void ath12k_core_hw_group_set_mlo_capable(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab;
	int i;

	if (ath12k_ftm_mode || !ath12k_mlo_capable)
		return;

	lockdep_assert_held(&ag->mutex);

	/* If more than one devices are grouped, then inter MLO
	 * functionality can work still independent of whether internally
	 * each device supports single_chip_mlo or not.
	 * Only when there is one device, then disable for WCN chipsets
	 * till the required driver implementation is in place.
	 */
	if (ag->num_devices == 1) {
		ab = ag->ab[0];

		/* WCN chipsets does not advertise in firmware features
		 * hence skip checking
		 */
		if (ab->hw_params->def_num_link)
			return;
	}

	ag->mlo_capable = true;

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab || ab->is_bypassed)
			continue;

		/* even if 1 device's firmware feature indicates MLO
		 * unsupported, make MLO unsupported for the whole group
		 */

		 /* TODO: As AHB, HYBRID chipsets dosen't support firmware2.bin
		  * bypassing this check for the both. Need to make this generic
		  */

		if (ab->fw.api_version == ATH12K_FW_API_V2 && !test_bit(ATH12K_FW_FEATURE_MLO, ab->fw.fw_features)) {
			ag->mlo_capable = false;
			return;
		}
	}
}

int ath12k_wsi_load_info_init(struct ath12k_base *ab)
{
	struct ath12k_mlo_wsi_load_info *wsi_load_info = NULL;
	struct ath12k_mlo_wsi_device_group *mlo_grp_info;
	struct ath12k_hw_group *ag = ab->ag;
	int ret = 0;
	u8 i;

	if (!ag)
		return ret;

	if (ag->num_devices < ATH12K_MIN_NUM_DEVICES_NLINK)
		return ret;

	if (!ag->wsi_load_info) {
		wsi_load_info = kzalloc(sizeof(*wsi_load_info), GFP_KERNEL);
		if (!wsi_load_info)
			return -ENOMEM;
		ag->wsi_load_info = wsi_load_info;
		mlo_grp_info = &wsi_load_info->mlo_device_grp;
		mlo_grp_info->num_devices = 0;
		for (i = 0; i < ATH12K_MAX_SOCS; i++)
			mlo_grp_info->wsi_order[i] = WSI_INVALID_ORDER;
		ath12k_dbg(ab, ATH12K_DBG_MAC, "successfully initialized wsi load info\n");
	}
	return ret;
}

void ath12k_wsi_load_info_deinit(struct ath12k_base *ab,
				 struct ath12k_mlo_wsi_load_info *wsi_load_info)
{
	ath12k_dbg(ab, ATH12K_DBG_MAC, "successfully deinitialized wsi load info\n");
	kfree(wsi_load_info);
}

void ath12k_wsi_load_info_wsiorder_update(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_wsi_load_info *wsi_load_info;
	struct ath12k_mlo_wsi_device_group *mlo_grp_info;
	u8 i;

	if (!ag || !ag->wsi_load_info)
		return;

	if (ag->num_devices < ATH12K_MIN_NUM_DEVICES_NLINK)
		return;

	wsi_load_info = ag->wsi_load_info;
	mlo_grp_info = &wsi_load_info->mlo_device_grp;

	for (i = 0; i < ATH12K_MAX_SOCS; i++) {
		if (mlo_grp_info->wsi_order[i] == WSI_INVALID_ORDER) {
			mlo_grp_info->wsi_order[i] = ab->wsi_info.index;
			mlo_grp_info->num_devices++;
			ath12k_dbg(ab, ATH12K_DBG_MAC,
				   "wsi load info update for wsi order %d\n",
				   ab->wsi_info.index);
			break;
		}
	}
}

int ath12k_core_add_dl_qos(struct ath12k_base *ab,
			   struct ath12k_qos_params *params, u8 id)
{
	struct ath12k_hw_group *ag = ab->ag;
	int ret = 0;
	u8 ab_id;

	for (ab_id = ag->num_probed; ab_id > 0; ab_id--) {
		ab = ag->ab[ab_id - 1];
		if (!test_bit(WMI_TLV_SERVICE_SDWF_LEVEL0,
			      ab->wmi_ab.svc_map)) {
			/* QoS not supported by the FW */
			ath12k_info(ab, "QoS is not supported");
			continue;
		}
		ret = ath12k_wmi_dl_qos_profile_create(ab, params, id);
		ath12k_dbg(ab, ATH12K_DBG_QOS,
			   "QoS DL profile %d created in FW", id);
	}
	return ret;
}
EXPORT_SYMBOL(ath12k_core_add_dl_qos);

int ath12k_core_del_dl_qos(struct ath12k_base *ab, u8 id)
{
	struct ath12k_hw_group *ag = ab->ag;
	int ret = 0;
	u8 ab_id;

	for (ab_id = ag->num_probed; ab_id > 0; ab_id--) {
		ab = ag->ab[ab_id - 1];
		if (!test_bit(WMI_TLV_SERVICE_SDWF_LEVEL0,
			      ab->wmi_ab.svc_map)) {
			/* QoS not supported by the FW */
			ath12k_info(ab, "QoS is not supported");
			continue;
		}
		ret = ath12k_wmi_dl_qos_profile_delete(ab, id);
		ath12k_dbg(ab, ATH12K_DBG_QOS,
			   "QoS DL profile %d deleted in FW", id);
	}
	return ret;
}
EXPORT_SYMBOL(ath12k_core_del_dl_qos);

int ath12k_core_config_ul_qos(struct ath12k *ar,
			      struct ath12k_qos_params *params,
			      u16 id, u8 *mac_addr, bool add_or_sub)
{
	struct  ath12k_wmi_ul_qos_params ul_params = {0};
	int ret;

	if (!ar || !mac_addr)
		return -ENOMEM;

	ul_params.qos_id = id;
	ul_params.ul_enable = 1;
	ul_params.sawf_ul_param = 1;
	ul_params.ac = ath12k_tid_to_ac(params->tid);
	ul_params.latency_tid = params->tid;
	ul_params.service_interval = params->min_service_interval;
	ul_params.burst_size = params->burst_size;
	ul_params.min_throughput = params->min_data_rate;
	ul_params.max_latency = params->delay_bound;
	ul_params.ofdma_disable = params->ul_ofdma_disable ? 1 : 0;
	ul_params.mu_mimo_disable = params->ul_mu_mimo_disable ? 1 : 0;

	if (add_or_sub)
		ul_params.add_or_sub = SDWF_UL_BURST_SZ_SUM_ADD;
	else
		ul_params.add_or_sub = SDWF_UL_BURST_SZ_SUM_DEL;

	ether_addr_copy(ul_params.peer_mac, mac_addr);

	ret = ath12k_wmi_ul_qos_profile_config(ar, &ul_params);
	ath12k_dbg(ar->ab, ATH12K_DBG_QOS,
		   "QoS UL profile %d config(add/del: %d) in FW",
		   id, add_or_sub);
	return ret;
}
EXPORT_SYMBOL(ath12k_core_config_ul_qos);

struct ath12k_base *ath12k_core_get_ab_by_wiphy(const struct wiphy *wiphy,
						bool no_arvifs)
{
	struct ath12k_hw_group *ag;
	struct ath12k_base *ab;
	struct ath12k *ar;
	int soc, i;

	mutex_lock(&ath12k_hw_group_mutex);
	list_for_each_entry(ag, &ath12k_hw_group_list, list) {
		if (!ag) {
			ath12k_err(NULL, "unable to fetch hw group\n");
			mutex_unlock(&ath12k_hw_group_mutex);
			return NULL;
		}

		for (soc = ag->num_probed; soc > 0; soc--) {
			ab = ag->ab[soc - 1];
			for (i = 0; i < ab->num_radios; i++) {
				ar = ab->pdevs[i].ar;
				if (!ar || ar->ah->hw->wiphy != wiphy)
					continue;

				if (no_arvifs && !list_empty(&ar->arvifs))
					continue;

				mutex_unlock(&ath12k_hw_group_mutex);
				return ab;
			}
		}
	}

	mutex_unlock(&ath12k_hw_group_mutex);
	return NULL;
}

u8 ath12k_core_get_ab_list_by_wiphy(const struct wiphy *wiphy,
				    struct ath12k_base **ab_list,
				    u8 ab_list_size)
{
	struct ath12k_hw_group *ag;
	struct ath12k_base *ab;
	struct ath12k *ar;
	int soc, i, index = 0;

	mutex_lock(&ath12k_hw_group_mutex);

	list_for_each_entry(ag, &ath12k_hw_group_list, list) {
		if (!ag)
			continue;

		for (soc = ag->num_probed; soc > 0; soc--) {
			ab = ag->ab[soc - 1];

			for (i = 0; i < ab->num_radios; i++) {
				ar = ab->pdevs[i].ar;

				if (index >= ab_list_size) {
					ath12k_err(NULL,
						   "insufficient length for ab_list\n");
					goto error;
				}

				if (!ar || ath12k_ar_to_hw(ar)->wiphy != wiphy)
					continue;

				ab_list[index] = ab;
				index++;
				break;
			}
		}
	}

error:
	mutex_unlock(&ath12k_hw_group_mutex);
	return index;
}

struct ath12k_hw_group *ath12k_core_get_ag(void)
{
	struct ath12k_hw_group *ag;

	mutex_lock(&ath12k_hw_group_mutex);
	ag = list_first_entry_or_null(&ath12k_hw_group_list,
				      struct ath12k_hw_group, list);
	mutex_unlock(&ath12k_hw_group_mutex);
	return ag;
}

int ath12k_core_init(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag;
	bool is_ready;
	int ret;

	ret = ath12k_core_panic_notifier_register(ab);
	if (ret)
		ath12k_warn(ab, "failed to register panic handler: %d\n", ret);

#ifdef CPTCFG_ATHDEBUG
	athdbg_ops_register(ab);
#endif
	mutex_lock(&ath12k_hw_group_mutex);

	ag = ath12k_core_hw_group_assign(ab);
	if (!ag) {
		mutex_unlock(&ath12k_hw_group_mutex);
		ath12k_warn(ab, "unable to get hw group\n");
		return -ENODEV;
	}

	mutex_unlock(&ath12k_hw_group_mutex);

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "num devices %d num probed %d\n",
		   ag->num_devices, ag->num_probed);

	mutex_lock(&ag->mutex);
	mutex_lock(&ab->core_lock);
	ret = ath12k_core_soc_create(ab);
	if (ret) {
		set_bit(ATH12K_FLAG_SOC_CREATE_FAIL, &ab->dev_flags);
		mutex_unlock(&ab->core_lock);
		mutex_unlock(&ag->mutex);
		ath12k_err(ab, "failed to create soc core: %d\n", ret);
		return ret;
	}
	mutex_unlock(&ab->core_lock);
	is_ready = ath12k_core_hw_group_create_ready(ag);

	if (is_ready) {
		ret = ath12k_core_hw_group_create(ag);
		if (ret) {
			ath12k_warn(ab, "unable to create hw group\n");
			mutex_unlock(&ag->mutex);
			goto err;
		}
	}
	mutex_unlock(&ag->mutex);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	/* Used for tracking the order of per ab's DS node in bringup sequence
	 * for the purposes of affinity settings
	 */
	ab->dp->ppe.ppeds_soc_idx = -1;
#endif
	ret = ath12k_telemetry_ab_agent_create_handler(ab);
	if (ret)
		ath12k_err(ab, "Unable to create telemetry psoc agent object: %d\n", ret);

#ifdef CPTCFG_ATHDEBUG
	if (is_ready)
		athdbg_if_register(ab);
#endif

	return 0;

err:
	ath12k_core_hw_group_destroy(ab->ag);
	ath12k_core_hw_group_unassign(ab);
	return ret;
}

void ath12k_core_deinit(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;

	if (ath12k_telemetry_ab_agent_delete_handler(ab))
		ath12k_err(ab, "failed to destroy soc agent\n");
	ath12k_core_hw_group_cleanup(ab->ag);
	ath12k_core_hw_group_destroy(ab->ag);
#ifdef CPTCFG_ATHDEBUG
	/* Unregister Minidump after all radios are brought down.
	 * Num started count will become zero here after all radios
	 * are brought down
	 */
	if (!ag->num_started)
		athdbg_if_unregister(ab);
#endif
	ath12k_core_hw_group_unassign(ab);
}

void ath12k_core_free(struct ath12k_base *ab)
{
	destroy_workqueue(ab->workqueue_aux);
	destroy_workqueue(ab->workqueue);
	kfree(ab);
}

struct ath12k_base *ath12k_core_alloc(struct device *dev, size_t priv_size,
				      enum ath12k_bus bus)
{
	u32 wide_band = ATH12K_WIDE_BAND_NONE;
	struct ath12k_base *ab;
	u32 addr;

	ab = kzalloc(sizeof(*ab) + priv_size, GFP_KERNEL);
	if (!ab)
		return NULL;

	init_completion(&ab->driver_recovery);

	ab->workqueue = create_singlethread_workqueue("ath12k_wq");
	if (!ab->workqueue)
		goto err_sc_free;

	ab->workqueue_aux = create_singlethread_workqueue("ath12k_aux_wq");
	if (!ab->workqueue_aux)
		goto err_free_wq;

	mutex_init(&ab->core_lock);
	mutex_init(&ab->tbl_mtx_lock);
	spin_lock_init(&ab->base_lock);
	init_completion(&ab->reset_complete);

	init_waitqueue_head(&ab->peer_mapping_wq);
	init_waitqueue_head(&ab->wmi_ab.tx_credits_wq);
	init_waitqueue_head(&ab->qmi.cold_boot_waitq);
	INIT_WORK(&ab->restart_work, ath12k_core_restart);
	INIT_WORK(&ab->reset_work, ath12k_core_reset);
	INIT_WORK(&ab->recovery_work, ath12k_core_recovery_work);
	INIT_WORK(&ab->rfkill_work, ath12k_rfkill_work);
	INIT_WORK(&ab->dump_work, ath12k_coredump_upload);
	INIT_WORK(&ab->update_11d_work, ath12k_update_11d);

	timer_setup(&ab->rx_replenish_retry, ath12k_ce_rx_replenish_retry, 0);
	init_completion(&ab->htc_suspend);
	init_completion(&ab->restart_completed);
	init_completion(&ab->wow.wakeup_completed);
	init_completion(&ab->rddm_reset_done);
	init_completion(&ab->power_up);

	ab->dev = dev;
	ab->hif.bus = bus;
	ab->qmi.num_radios = U8_MAX;

	if (!of_property_read_u32(ab->dev->of_node, "memory-region", &addr))
		set_bit(ATH12K_FLAG_FIXED_MEM_REGION, &ab->dev_flags);

	/* This is HACK to bring up the qcn9224 with segmented memory */
	if (ab->hif.bus == ATH12K_BUS_PCI && ath12k_fw_mem_seg)
		clear_bit(ATH12K_FLAG_FIXED_MEM_REGION, &ab->dev_flags);

	if (of_property_read_u32(ab->dev->of_node, "qcom,wide_band", &wide_band))
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "Wide band property not present");

	ab->wide_band = wide_band;

	/* Device index used to identify the devices in a group.
	 *
	 * In Intra-device MLO, only one device present in a group,
	 * so it is always zero.
	 *
	 * In Inter-device MLO, Multiple device present in a group,
	 * expect non-zero value.
	 */
	ab->device_id = 0;

	return ab;

err_free_wq:
	destroy_workqueue(ab->workqueue);
err_sc_free:
	kfree(ab);
	return NULL;
}

void ath12k_core_issue_bug_on(struct ath12k_base *ab)
{
        struct ath12k_hw_group *ag = ab->ag;

        if (ab->in_panic)
                goto out;

        /* set in_panic to true to avoid multiple rddm download during
         * firmware crash
         */
        ab->in_panic = true;

        if (!ag->mlo_capable)
                BUG_ON(1);

	if (atomic_read(&ath12k_coredump_ram_info.num_chip) >=
			(ab->ag->num_started - ab->ag->num_bypassed))
                BUG_ON(1);
        else
                goto out;

out:
        ath12k_info(ab,
                    "%d chip dump collected and waiting for partner chips\n",
                    atomic_read(&ath12k_coredump_ram_info.num_chip));
}

void ath12k_telemetry_notify_breach(u8 *mac_addr, u8 svc_id, u8 param,
				    bool set_clear, u8 tid)
{
	struct ath12k_hw_group *ag = NULL;
	struct ieee80211_vif *vif = NULL;
	struct ath12k_base *ab = NULL;
	struct ath12k_dp_link_peer *peer = NULL;
	int soc;
	u8 *mld_addr = NULL;

	if (!mac_addr)
		return;

	mutex_lock(&ath12k_hw_group_mutex);
	list_for_each_entry(ag, &ath12k_hw_group_list, list) {
		if (!ag) {
			ath12k_err(NULL, "unable to fetch hw group\n");
			continue;
		}

		for (soc = ag->num_probed; soc > 0; soc--) {
			ab = ag->ab[soc - 1];
			if (!ab) {
				/* Control should not reach here */
				ath12k_info(NULL, "SOC not initialized\n");
				continue;
			}

			spin_lock_bh(&ab->dp->dp_lock);
			peer = ath12k_dp_link_peer_find_by_addr(ab->dp, mac_addr);
			if (peer) {
				vif = peer->vif;
				if (peer->mlo)
					mld_addr = peer->ml_addr;
				ath12k_dbg(ab, ATH12K_DBG_QOS, "Breach detected: Peer %pM\n",
					   mac_addr);
				spin_unlock_bh(&ab->dp->dp_lock);
				mutex_unlock(&ath12k_hw_group_mutex);
				ath12k_vendor_telemetry_notify_breach(vif,
								      mac_addr,
								      svc_id,
								      param,
								      set_clear,
								      tid,
								      mld_addr);
				return;
			}
			spin_unlock_bh(&ab->dp->dp_lock);
		}
	}
	mutex_unlock(&ath12k_hw_group_mutex);

	ath12k_dbg(NULL, ATH12K_DBG_QOS, "Peer(%pM) not found for notifying breach",
		   mac_addr);
}

MODULE_DESCRIPTION("Driver support for Qualcomm Technologies WLAN devices");
MODULE_LICENSE("Dual BSD/GPL");
