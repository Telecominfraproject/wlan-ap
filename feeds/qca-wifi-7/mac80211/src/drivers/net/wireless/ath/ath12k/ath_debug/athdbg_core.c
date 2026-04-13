// SPDX-License-Identifier: BSD-3-Clause-Clear
/*Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/devcoredump.h>
#include <linux/uaccess.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include "athdbg_minidump.h"
#include "athdbg_core.h"
#include "../debug.h"

MODULE_SOFTDEP("post: ath12k ath12k_wifi7");

struct ath_debug_base *athdbg_base;

static bool athdbg_drv_ready(struct ath12k_base *ab)
{
	if (!ab)
		return FALSE;

	if ((athdbg_base->dbg_to_ath_ops != NULL &&
		(athdbg_base->dbg_to_ath_ops->dev_running_status(ab) != TRUE))) {
		pr_err("athdbg_core: dev[%s] not running", dev_name(ab->dev));
		return FALSE;
	}

	return TRUE;
}

unsigned int athdbg_conv_str_to_dbgmask(char *dbgmask)
{
	if (!strcmp(dbgmask, "ahb"))
		return ATH12K_DBG_AHB;
	else if (!strcmp(dbgmask, "wmi"))
		return ATH12K_DBG_WMI;
	else if (!strcmp(dbgmask, "htc"))
		return ATH12K_DBG_HTC;
	else if (!strcmp(dbgmask, "htt"))
		return ATH12K_DBG_DP_HTT;
	else if (!strcmp(dbgmask, "mac"))
		return ATH12K_DBG_MAC;
	else if (!strcmp(dbgmask, "boot"))
		return ATH12K_DBG_BOOT;
	else if (!strcmp(dbgmask, "qmi"))
		return ATH12K_DBG_QMI;
	else if (!strcmp(dbgmask, "data"))
		return ATH12K_DBG_DATA;
	else if (!strcmp(dbgmask, "mgmt"))
		return ATH12K_DBG_MGMT;
	else if (!strcmp(dbgmask, "hal"))
		return ATH12K_DBG_HAL;
	else if (!strcmp(dbgmask, "pci"))
		return ATH12K_DBG_PCI;
	else if (!strcmp(dbgmask, "cp_tx"))
		return ATH12K_DBG_DP_TX;
	else if (!strcmp(dbgmask, "dp_rx"))
		return ATH12K_DBG_DP_RX;
	else if (!strcmp(dbgmask, "wow"))
		return ATH12K_DBG_WOW;
	else if (!strcmp(dbgmask, "fst"))
		return ATH12K_DBG_DP_FST;
	else if (!strcmp(dbgmask, "peer"))
		return ATH12K_DBG_PEER;
	else
		return 0;
}

static void athdbg_process_request(struct work_struct *work)
{
	struct ath_debug_base *athdbg_base = container_of(work, struct ath_debug_base, dbg_wk);
	struct athdbg_request *dbg_req;
	int ret;

	mutex_lock(&athdbg_base->req_lock);

	while (!list_empty(&athdbg_base->req_list)) {
		dbg_req = list_first_entry(&athdbg_base->req_list,
					 struct athdbg_request, req_list);
		list_del(&dbg_req->req_list);
		mutex_unlock(&athdbg_base->req_lock);

		switch (dbg_req->req_type) {
		case ATH_DBG_REQ_SETMASK:
			if (athdbg_drv_ready(dbg_req->ab) != TRUE) {
				pr_err("athdbg_core: Setmask Failure - device not running");
			}
			athdbg_base->dbg_to_ath_ops->set_dbg_mask(dbg_req->data);
			break;
		case ATH_DBG_REQ_COLLECT_MINI_DUMP:
			athdbg_process_minidump_request(dbg_req->ab, dbg_req);
			break;
		case ATH_DBG_REQ_ENABLE_QDSS:
			athdbg_config_qdss(dbg_req->ab);
			break;

		case ATH_DBG_REQ_DUMP_QDSS:
		{
			ret = athdbg_send_qdss_trace_mode_req(dbg_req->ab,
							QMI_WLANFW_QDSS_TRACE_OFF_V01,
							dbg_req->data);
			if (ret < 0)
				pr_warn("athdbg_core: Failed to stop QDSS: %d\n", ret);
		}
			break;
		case ATH_DBG_REQ_UNKNOWN:
			pr_err("athdbg_core: Unknown Request");
			break;
		}

		kfree(dbg_req);
		mutex_lock(&athdbg_base->req_lock);
	}
	mutex_unlock(&athdbg_base->req_lock);
}

static int __init athdbg_driver_init(void)
{
	struct ath_debug_base *athdbg;

	athdbg = kzalloc(sizeof(struct ath_debug_base), GFP_KERNEL);

	if (!athdbg) {
		pr_err("athdbg_core: alloc failure");
		return -ENOMEM;
	}

	athdbg_base = athdbg;

	athdbg_base->dbg_wq = create_singlethread_workqueue("athdbg_wq");

	if (!athdbg_base->dbg_wq) {
		kfree(athdbg_base);
		athdbg_base = NULL;
		return -ENOMEM;
	}

	athdbg_create_minidump_struct_list();

	INIT_WORK(&athdbg_base->dbg_wk, athdbg_process_request);
	INIT_LIST_HEAD(&athdbg_base->req_list);
	mutex_init(&athdbg_base->req_lock);

	return 0;
}

EXPORT_SYMBOL(athdbg_base);

static void __exit athdbg_driver_exit(void)
{
	athdbg_clear_minidump_struct_list();
	cancel_work_sync(&athdbg_base->dbg_wk);
	destroy_workqueue(athdbg_base->dbg_wq);
	kfree(athdbg_base);
}

module_init(athdbg_driver_init);
module_exit(athdbg_driver_exit);

MODULE_DESCRIPTION("Driver support debug options for Qualcomm Technologies WLAN devices");
MODULE_LICENSE("Dual BSD/GPL");
