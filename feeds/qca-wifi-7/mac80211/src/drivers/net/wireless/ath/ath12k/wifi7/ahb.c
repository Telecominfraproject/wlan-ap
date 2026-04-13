// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/mdt_loader.h>
#include "../ahb.h"
#include "ahb.h"
#include "../debug.h"
#include "../hif.h"
#include "hw.h"
#include "dp.h"

static const struct of_device_id ath12k_wifi7_ahb_of_match[] = {
	{ .compatible = "qcom,ipq5332-wifi",
	  .data = (void *)ATH12K_HW_IPQ5332_HW10,
	},
	{ .compatible = "qcom,ipq5424-wifi",
	  .data = (void *)ATH12K_HW_IPQ5424_HW10,
	},
	{ .compatible = "qcom,qcn6432-wifi",
	  .data = (void *)ATH12K_HW_QCN6432_HW10,
	},
	{ }
};

MODULE_DEVICE_TABLE(of, ath12k_wifi7_ahb_of_match);

static const struct ath12k_ahb_ops ahb_ops_ipq5332 = {
	.mdt_load = qcom_mdt_load,
};

static const struct ath12k_ahb_ops ahb_ops_ipq5424 = {
	.mdt_load = qcom_mdt_load_no_init,
};

static int ath12k_wifi7_ahb_probe(struct platform_device *pdev)
{
	struct ath12k_ahb *ab_ahb;
	struct ath12k_base *ab;
	int ret;

	ab = platform_get_drvdata(pdev);
	ab_ahb = ath12k_ab_to_ahb(ab);

	switch (ab->hw_rev) {
	case ATH12K_HW_IPQ5332_HW10:
		ab_ahb->userpd_id = ATH12K_IPQ5332_USERPD_ID;
		fallthrough;
	case ATH12K_HW_QCN6432_HW10:
		ab_ahb->scm_auth_enabled = true;
		ab_ahb->ahb_ops = &ahb_ops_ipq5332;
		break;
	case ATH12K_HW_IPQ5424_HW10:
		ab_ahb->userpd_id = ATH12K_IPQ5332_USERPD_ID;
		ab_ahb->scm_auth_enabled = false;
		ab_ahb->ahb_ops = &ahb_ops_ipq5424;
		break;
	default:
		return -EOPNOTSUPP;
	}

	ret = ath12k_wifi7_hw_init(ab);
	if (ret) {
		ath12k_err(ab, "hw_init failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static struct ath12k_ahb_driver ath12k_wifi7_ahb_driver = {
	.name = "ath12k_wifi7_ahb",
	.id_table = ath12k_wifi7_ahb_of_match,
	.ops.probe = ath12k_wifi7_ahb_probe,
	.ops.dp_init = ath12k_wifi7_dp_init,
	.ops.dp_deinit = ath12k_wifi7_dp_deinit,
};

int ath12k_wifi7_ahb_init(void)
{
	return ath12k_ahb_register_driver(ATH12K_DEVICE_FAMILY_WIFI7,
					  &ath12k_wifi7_ahb_driver);
}

void ath12k_wifi7_ahb_exit(void)
{
	ath12k_ahb_unregister_driver(ATH12K_DEVICE_FAMILY_WIFI7);
}
