// SPDX-License-Identifier: GPL-2.0
/*
 * mt7986-si3218x.c  --  MT7986-SI3218X ALSA SoC machine driver
 *
 * Copyright (c) 2023 MediaTek Inc.
 * Authors: Vic Wu <vic.wu@mediatek.com>
 *          Maso Huang <maso.huang@mediatek.com>
 */

#include <linux/bitfield.h>
#include <linux/module.h>
#include <sound/soc.h>

#include "mt7986-afe-common.h"
#include "mt7986-reg.h"
#include "../common/mtk-afe-platform-driver.h"

enum {
	HOPPING_CLK = 0,
	APLL_CLK = 1,
};

enum {
	I2S = 0,
	PCMA = 4,
	PCMB,
};

enum {
	ETDM_IN5 = 2,
	ETDM_OUT5 = 10,
};

enum {
	AFE_FS_8K = 0,
	AFE_FS_11K = 1,
	AFE_FS_12K = 2,
	AFE_FS_16K = 4,
	AFE_FS_22K = 5,
	AFE_FS_24K = 6,
	AFE_FS_32K = 8,
	AFE_FS_44K = 9,
	AFE_FS_48K = 10,
	AFE_FS_88K = 13,
	AFE_FS_96K = 14,
	AFE_FS_176K = 17,
	AFE_FS_192K = 18,
};

enum {
	ETDM_FS_8K = 0,
	ETDM_FS_12K = 1,
	ETDM_FS_16K = 2,
	ETDM_FS_24K = 3,
	ETDM_FS_32K = 4,
	ETDM_FS_48K = 5,
	ETDM_FS_96K = 7,
	ETDM_FS_192K = 9,
	ETDM_FS_11K = 16,
	ETDM_FS_22K = 17,
	ETDM_FS_44K = 18,
	ETDM_FS_88K = 19,
	ETDM_FS_176K = 20,
};

static int mt7986_si3218x_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt7986_afe_private *afe_priv = afe->platform_priv;
	int ret;

	/* enable clk */
	ret = clk_bulk_prepare_enable(afe_priv->num_clks, afe_priv->clks);
	if (ret) {
		dev_err(afe->dev, "Failed to enable clocks\n");
		return ret;
	}

	regmap_update_bits(afe->regmap, AUDIO_TOP_CON2, CLK_OUT5_PDN_MASK, 0);
	regmap_update_bits(afe->regmap, AUDIO_TOP_CON2, CLK_IN5_PDN_MASK, 0);
	regmap_update_bits(afe->regmap, AUDIO_TOP_CON4, 0x3fff, 0);
	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0, AUD_APLL2_EN_MASK,
			   AUD_APLL2_EN);
	regmap_update_bits(afe->regmap, AUDIO_ENGEN_CON0, AUD_26M_EN_MASK,
			   AUD_26M_EN);

	/* set ETDM_IN5_CON0 */
	regmap_update_bits(afe->regmap, ETDM_IN5_CON0, ETDM_SYNC_MASK,
			   ETDM_SYNC);
	regmap_update_bits(afe->regmap, ETDM_IN5_CON0, ETDM_FMT_MASK,
			   FIELD_PREP(ETDM_FMT_MASK, PCMA));
	regmap_update_bits(afe->regmap, ETDM_IN5_CON0, ETDM_BIT_LEN_MASK,
			   FIELD_PREP(ETDM_BIT_LEN_MASK, 16 - 1));
	regmap_update_bits(afe->regmap, ETDM_IN5_CON0, ETDM_WRD_LEN_MASK,
			   FIELD_PREP(ETDM_WRD_LEN_MASK, 16 - 1));
	regmap_update_bits(afe->regmap, ETDM_IN5_CON0, ETDM_CH_NUM_MASK,
			   FIELD_PREP(ETDM_CH_NUM_MASK, 4 - 1));
	regmap_update_bits(afe->regmap, ETDM_IN5_CON0, RELATCH_SRC_MASK,
			   FIELD_PREP(RELATCH_SRC_MASK, APLL_CLK));

	/* set ETDM_IN5_CON2 */
	regmap_update_bits(afe->regmap, ETDM_IN5_CON2, IN_CLK_SRC_MASK,
			   IN_CLK_SRC(APLL_CLK));

	/* set ETDM_IN5_CON3 */
	regmap_update_bits(afe->regmap, ETDM_IN5_CON3, IN_SEL_FS_MASK,
			   IN_SEL_FS(ETDM_FS_16K));

	/* set ETDM_IN5_CON4 */
	regmap_update_bits(afe->regmap, ETDM_IN5_CON4, IN_CLK_INV_MASK,
			   IN_CLK_INV);
	regmap_update_bits(afe->regmap, ETDM_IN5_CON4, IN_RELATCH_MASK,
			   IN_RELATCH(AFE_FS_16K));

	/* set ETDM_OUT5_CON0 */
	regmap_update_bits(afe->regmap, ETDM_OUT5_CON0, ETDM_FMT_MASK,
			   FIELD_PREP(ETDM_FMT_MASK, PCMA));
	regmap_update_bits(afe->regmap, ETDM_OUT5_CON0, ETDM_BIT_LEN_MASK,
			   FIELD_PREP(ETDM_BIT_LEN_MASK, 16 - 1));
	regmap_update_bits(afe->regmap, ETDM_OUT5_CON0, ETDM_WRD_LEN_MASK,
			   FIELD_PREP(ETDM_WRD_LEN_MASK, 16 - 1));
	regmap_update_bits(afe->regmap, ETDM_OUT5_CON0, ETDM_CH_NUM_MASK,
			   FIELD_PREP(ETDM_CH_NUM_MASK, 4 - 1));
	regmap_update_bits(afe->regmap, ETDM_OUT5_CON0, RELATCH_SRC_MASK,
			   FIELD_PREP(RELATCH_SRC_MASK, APLL_CLK));

	/* set ETDM_OUT5_CON4 */
	regmap_update_bits(afe->regmap, ETDM_OUT5_CON4, OUT_SEL_FS_MASK,
			   OUT_SEL_FS(ETDM_FS_16K));
	regmap_update_bits(afe->regmap, ETDM_OUT5_CON4, OUT_CLK_SRC_MASK,
			   OUT_CLK_SRC(APLL_CLK));
	regmap_update_bits(afe->regmap, ETDM_OUT5_CON4, OUT_RELATCH_MASK,
			   OUT_RELATCH(AFE_FS_16K));

	/* set ETDM_OUT5_CON5 */
	regmap_update_bits(afe->regmap, ETDM_OUT5_CON5, OUT_CLK_INV_MASK,
			   OUT_CLK_INV);
	regmap_update_bits(afe->regmap, ETDM_OUT5_CON5, ETDM_CLK_DIV_MASK,
			   ETDM_CLK_DIV);

	/* set external loopback */
	regmap_update_bits(afe->regmap, ETDM_4_7_COWORK_CON0, OUT_SEL_MASK,
			   OUT_SEL(ETDM_IN5));

	/* enable ETDM */
	regmap_update_bits(afe->regmap, ETDM_IN5_CON0, ETDM_EN_MASK,
			   ETDM_EN);
	regmap_update_bits(afe->regmap, ETDM_OUT5_CON0, ETDM_EN_MASK,
			   ETDM_EN);

	return 0;
}

SND_SOC_DAILINK_DEFS(playback,
	DAILINK_COMP_ARRAY(COMP_CPU("DL1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(capture,
	DAILINK_COMP_ARRAY(COMP_CPU("UL1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(codec,
	DAILINK_COMP_ARRAY(COMP_CPU("ETDM")),
	DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "proslic_spi-aif")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link mt7986_si3218x_dai_links[] = {
	/* FE */
	{
		.name = "si3218x-playback",
		.stream_name = "si3218x-playback",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback),
	},
	{
		.name = "si3218x-capture",
		.stream_name = "si3218x-capture",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture),
	},
	/* BE */
	{
		.name = "si3218x-codec",
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_DSP_A |
			SND_SOC_DAIFMT_IB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.init = mt7986_si3218x_init,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(codec),
	},
};

static struct snd_soc_card mt7986_si3218x_card = {
	.name = "mt7986-si3218x",
	.owner = THIS_MODULE,
	.dai_link = mt7986_si3218x_dai_links,
	.num_links = ARRAY_SIZE(mt7986_si3218x_dai_links),
};

static int mt7986_si3218x_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt7986_si3218x_card;
	struct snd_soc_dai_link *dai_link;
	struct device_node *platform, *codec;
	struct device_node *platform_dai_node, *codec_dai_node;
	int ret, i;

	card->dev = &pdev->dev;

	platform = of_get_child_by_name(pdev->dev.of_node, "platform");

	if (platform) {
		platform_dai_node = of_parse_phandle(platform, "sound-dai", 0);
		of_node_put(platform);

		if (!platform_dai_node) {
			dev_err(&pdev->dev, "Failed to parse platform/sound-dai property\n");
			return -EINVAL;
		}
	} else {
		dev_err(&pdev->dev, "Property 'platform' missing or invalid\n");
		return -EINVAL;
	}

	for_each_card_prelinks(card, i, dai_link) {
		if (dai_link->platforms->name)
			continue;
		dai_link->platforms->of_node = platform_dai_node;
	}

	codec = of_get_child_by_name(pdev->dev.of_node, "codec");

	if (codec) {
		codec_dai_node = of_parse_phandle(codec, "sound-dai", 0);
		of_node_put(codec);

		if (!codec_dai_node) {
			of_node_put(platform_dai_node);
			dev_err(&pdev->dev, "Failed to parse codec/sound-dai property\n");
			return -EINVAL;
		}
	} else {
		of_node_put(platform_dai_node);
		dev_err(&pdev->dev, "Property 'codec' missing or invalid\n");
		return -EINVAL;
	}

	for_each_card_prelinks(card, i, dai_link) {
		if (dai_link->codecs->name)
			continue;
		dai_link->codecs->of_node = codec_dai_node;
	}

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret) {
		dev_err(&pdev->dev, "%s snd_soc_register_card fail: %d\n", __func__, ret);
		goto err_of_node_put;
	}

err_of_node_put:
	of_node_put(platform_dai_node);
	of_node_put(codec_dai_node);
	return ret;
}

static const struct of_device_id mt7986_si3218x_machine_dt_match[] = {
	{.compatible = "mediatek,mt7986-si3218x-sound"},
	{ /* sentinel */ }
};

static struct platform_driver mt7986_si3218x_machine = {
	.driver = {
		.name = "mt7986-si3218x",
		.of_match_table = mt7986_si3218x_machine_dt_match,
	},
	.probe = mt7986_si3218x_machine_probe,
};

module_platform_driver(mt7986_si3218x_machine);

/* Module information */
MODULE_DESCRIPTION("MT7986 SI3218X ALSA SoC machine driver");
MODULE_AUTHOR("Vic Wu <vic.wu@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("mt7986 si3218x soc card");
