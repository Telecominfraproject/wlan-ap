/* SPDX-License-Identifier: BSD-3-Clause-Clear*/
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#ifndef ATH_DBG_IF_H
#define ATH_DBG_IF_H
#include "core.h"
#include "mac.h"
#include <linux/module.h>
#include <linux/kernel.h>

enum athdbg_service {
	ATHDBG_SRV_CONFIG_QDSS,
	ATHDBG_SRV_DO_MINIDUMP,
	ATHDBG_SRV_COLLECT_MINIDUMP_REFERENCES,
	ATHDBG_SRV_QMI_DEINIT,
	ATHDBG_SRV_QDSS_MEM_FREE,
	ATHDBG_SRV_MHI_Q6_DUMP_BL_SRAM,
	ATHDBG_SRV_MHI_Q6_BOOT_DEBUG_TIMEOUT,
};

struct athdbg_to_ath12k_ops {
	struct reserved_mem *(*get_reserved_mem_by_name)(
			struct ath12k_base *ab, const char *name);
	void (*coredump_qdss_dump)(struct ath12k_base *ab,
			struct ath12k_qmi_event_qdss_trace_save_data *event_data);
	void (*coredump_dump_segment)(struct ath12k_base *ab,
			struct ath12k_dump_segment *segments, size_t seg_len);
	bool (*dev_running_status)(struct ath12k_base *drv_ab);
	void (*set_dbg_mask)(unsigned int debug_mask);
	struct ath12k_link_vif *(*get_link_vif_from_vdev_id)(
			struct ath12k_base *ab, u32 vdev_id);
	u32 (*pci_read32)(struct ath12k_base *ab, u32 offset);
	void *(*pci_get_priv)(struct ath12k_base *ab);
};

bool athdbg_if_check_dev_running(struct ath12k_base *drv_ab);
void athdbg_if_setmask(unsigned int debug_mask);
void athdbg_ops_register(struct ath12k_base *drv_ab);
void athdbg_if_register(struct ath12k_base *drv_ab);
void athdbg_if_unregister(struct ath12k_base *ab);
int athdbg_if_get_service(struct ath12k_base *ab, enum athdbg_service srv);
int athdbg_config_qdss(struct ath12k_base *ab);
void athdbg_qmi_deinit(struct ath12k_base *ab);
int athdbg_qmi_worker_init(void *qmi_ab);

extern unsigned int ath12k_debug_mask;
extern const struct file_operations debugfs_mask_fops;
extern const struct file_operations debugfs_qdss_enable_fops;
extern const struct file_operations debugfs_qdss_collect_fops;
#endif
