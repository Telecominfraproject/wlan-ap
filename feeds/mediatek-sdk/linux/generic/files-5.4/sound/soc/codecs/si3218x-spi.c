// SPDX-License-Identifier: GPL-2.0
//
// Copyright (C) 2023 MediaTek Inc.

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/spi/spi.h>

/* alsa sound header */
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "si3218x/si3218x.h"

enum si3218x_spi_type {
	MTK_EXT_PROSLIC = 0,
	MTK_EXT_TYPE_NUM
};

struct mtk_ext_spi_ctrl {
	int (*spi_probe)(struct spi_device *spi, struct spi_driver *spi_drv);
	int (*spi_remove)(struct spi_device *spi);
	const char *stream_name;
	const char *codec_dai_name;
	const char *codec_name;
};

static struct mtk_ext_spi_ctrl mtk_ext_list[MTK_EXT_TYPE_NUM] = {
	[MTK_EXT_PROSLIC] = {
		.spi_probe = si3218x_spi_probe,
		.spi_remove = si3218x_spi_remove,
	},
};

static unsigned int mtk_ext_type;

static int mtk_ext_spi_probe(struct spi_device *spi);
static int mtk_ext_spi_remove(struct spi_device *spi)
{
	dev_info(&spi->dev, "%s()\n", __func__);

	if (mtk_ext_list[mtk_ext_type].spi_remove)
		mtk_ext_list[mtk_ext_type].spi_remove(spi);

	return 0;
}

static const struct of_device_id mtk_ext_match_table[] = {
	{.compatible = "silabs,proslic_spi" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mtk_ext_match_table);

static struct spi_driver mtk_ext_spi_driver = {
	.driver = {
		.name = "proslic_spi",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mtk_ext_match_table),
	},
	.probe = mtk_ext_spi_probe,
	.remove = mtk_ext_spi_remove,
};

module_spi_driver(mtk_ext_spi_driver);

static int mtk_ext_spi_probe(struct spi_device *spi)
{
	int i, ret = 0;

	dev_err(&spi->dev, "%s()\n", __func__);

	mtk_ext_type = MTK_EXT_PROSLIC;
	for (i = 0; i < MTK_EXT_TYPE_NUM; i++) {
		if (!mtk_ext_list[i].spi_probe)
			continue;

		ret = mtk_ext_list[i].spi_probe(spi, &mtk_ext_spi_driver);
		if (ret)
			continue;

		mtk_ext_type = i;
		break;
	}

	return ret;
}
