// SPDX-License-Identifier: GPL-2.0
//
// mt6768-mt6358.c  --  MT6768-MT6358 ALSA SoC machine driver
//
// Copyright (c) 2022 MediaTek Inc.
// Author: Gang Feng <gang.feng@mediatek.com>

#include <linux/module.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <sound/jack.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <sound/cs35l41.h>
#include "mt6768-afe-common.h"
#include "../common/mtk-afe-platform-driver.h"
#include "../../codecs/mt6358.h"

struct mt6768_mt6358_priv {
	struct snd_soc_jack headset_jack;
};

static int mt6768_mt6358_codec_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *cmpnt_codec =
		snd_soc_rtd_to_codec(rtd, 0)->component;
	struct snd_soc_component *cmpnt_afe =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt_afe);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	/* set mtkaif protocol */
	mt6358_set_mtkaif_protocol(cmpnt_codec, MT6358_MTKAIF_PROTOCOL_1);
	afe_priv->mtkaif_protocol = MT6768_MTKAIF_PROTOCOL_1;

	return 0;
}

static int mt6768_mt6358_i2s_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	unsigned int rate = params_rate(params);
	unsigned int mclk_fs_ratio = 128;
	unsigned int mclk_fs = rate * mclk_fs_ratio;
	unsigned int bclk = rate * 64;
	struct snd_soc_dai *codec_dai;
	unsigned int rx_slot = 0;
	int i, ret;

	for_each_rtd_codec_dais(rtd, i, codec_dai) {
		if (codec_dai->driver->name &&
		    strcmp(codec_dai->driver->name, "cs35l41-pcm") == 0) {
			ret = snd_soc_component_set_sysclk(codec_dai->component,
							   CS35L41_CLKID_SCLK, 0,
							   bclk, SND_SOC_CLOCK_IN);
			if (ret < 0)
				dev_err(rtd->dev, "failed to set cs35l41 component sysclk: %d\n", ret);

			ret = snd_soc_dai_set_sysclk(codec_dai, CS35L41_CLKID_SCLK,
						     bclk, SND_SOC_CLOCK_IN);
			if (ret < 0)
				dev_err(rtd->dev, "failed to set cs35l41 dai sysclk: %d\n", ret);

			ret = snd_soc_dai_set_channel_map(codec_dai, 0, NULL,
							  1, &rx_slot);
			if (ret < 0)
				dev_err(rtd->dev, "failed to set cs35l41 channel map: %d\n", ret);
		}
	}

	return snd_soc_dai_set_sysclk(snd_soc_rtd_to_cpu(rtd, 0),
				      0, mclk_fs, SND_SOC_CLOCK_OUT);
}

static const struct snd_soc_ops mt6768_mt6358_i2s_ops = {
	.hw_params = mt6768_mt6358_i2s_hw_params,
};

static int mt6768_i2s_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				      struct snd_pcm_hw_params *params)
{
	snd_mask_reset_range(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT),
			     0, SNDRV_PCM_FORMAT_LAST);
	params_set_format(params, SNDRV_PCM_FORMAT_S24_LE);
	return 0;
}

/* FE */
SND_SOC_DAILINK_DEFS(playback1,
	DAILINK_COMP_ARRAY(COMP_CPU("DL1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(playback2,
	DAILINK_COMP_ARRAY(COMP_CPU("DL2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(playback3,
	DAILINK_COMP_ARRAY(COMP_CPU("DL3")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(capture1,
	DAILINK_COMP_ARRAY(COMP_CPU("UL1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(capture2,
	DAILINK_COMP_ARRAY(COMP_CPU("UL2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(capture3,
	DAILINK_COMP_ARRAY(COMP_CPU("UL3")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(capture4,
	DAILINK_COMP_ARRAY(COMP_CPU("UL4")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

/* BE */
SND_SOC_DAILINK_DEFS(primary_codec,
	DAILINK_COMP_ARRAY(COMP_CPU("ADDA")),
	DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "mt6358-snd-codec-aif1")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(i2s0,
	DAILINK_COMP_ARRAY(COMP_CPU("I2S0")),
	DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "cs35l41-pcm")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(i2s1,
	DAILINK_COMP_ARRAY(COMP_CPU("I2S1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(i2s2,
	DAILINK_COMP_ARRAY(COMP_CPU("I2S2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(i2s3,
	DAILINK_COMP_ARRAY(COMP_CPU("I2S3")),
	DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "cs35l41-pcm")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(pcm2,
	DAILINK_COMP_ARRAY(COMP_CPU("PCM 2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link mt6768_mt6358_dai_links[] = {
	/* Front End DAIs */
	{
		.name = "Playback_1",
		.stream_name = "Playback_1",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.playback_only = 1,
		SND_SOC_DAILINK_REG(playback1),
	},
	{
		.name = "Playback_2",
		.stream_name = "Playback_2",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.playback_only = 1,
		SND_SOC_DAILINK_REG(playback2),
	},
	{
		.name = "Playback_3",
		.stream_name = "Playback_3",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.playback_only = 1,
		SND_SOC_DAILINK_REG(playback3),
	},
	{
		.name = "Capture_1",
		.stream_name = "Capture_1",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.capture_only = 1,
		SND_SOC_DAILINK_REG(capture1),
	},
	{
		.name = "Capture_2",
		.stream_name = "Capture_2",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.capture_only = 1,
		SND_SOC_DAILINK_REG(capture2),
	},
	{
		.name = "Capture_3",
		.stream_name = "Capture_3",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.capture_only = 1,
		SND_SOC_DAILINK_REG(capture3),
	},
	{
		.name = "Capture_4",
		.stream_name = "Capture_4",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.capture_only = 1,
		SND_SOC_DAILINK_REG(capture4),
	},

	/* Back End DAIs */
	{
		.name = "Primary Codec",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.init = mt6768_mt6358_codec_init,
		SND_SOC_DAILINK_REG(primary_codec),
	},
	{
		.name = "I2S0",
		.no_pcm = 1,
		.capture_only = 1,
		.ignore_suspend = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			   SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBC_CFC,
		.be_hw_params_fixup = mt6768_i2s_hw_params_fixup,
		.ops = &mt6768_mt6358_i2s_ops,
		SND_SOC_DAILINK_REG(i2s0),
	},
	{
		.name = "I2S1",
		.no_pcm = 1,
		.playback_only = 1,
		.ignore_suspend = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			   SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBC_CFC,
		.be_hw_params_fixup = mt6768_i2s_hw_params_fixup,
		.ops = &mt6768_mt6358_i2s_ops,
		SND_SOC_DAILINK_REG(i2s1),
	},
	{
		.name = "I2S2",
		.no_pcm = 1,
		.capture_only = 1,
		.ignore_suspend = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			   SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBC_CFC,
		.be_hw_params_fixup = mt6768_i2s_hw_params_fixup,
		.ops = &mt6768_mt6358_i2s_ops,
		SND_SOC_DAILINK_REG(i2s2),
	},
	{
		.name = "I2S3",
		.no_pcm = 1,
		.playback_only = 1,
		.ignore_suspend = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			   SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBC_CFC,
		.be_hw_params_fixup = mt6768_i2s_hw_params_fixup,
		.ops = &mt6768_mt6358_i2s_ops,
		SND_SOC_DAILINK_REG(i2s3),
	},
	{
		.name = "PCM 2",
		.no_pcm = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(pcm2),
	},
};

static const struct snd_kcontrol_new mt6768_mt6358_snd_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphone"),
	SOC_DAPM_PIN_SWITCH("Earpiece"),
	SOC_DAPM_PIN_SWITCH("Speaker"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
	SOC_DAPM_PIN_SWITCH("Internal Mic"),
};

static const struct snd_soc_dapm_widget mt6768_mt6358_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_SPK("Earpiece", NULL),
	SND_SOC_DAPM_SPK("Speaker", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Internal Mic", NULL),
};

static const struct snd_soc_dapm_route mt6768_mt6358_dapm_routes[] = {
	{"Headphone", NULL, "Headphone L"},
	{"Headphone", NULL, "Headphone R"},
	{"Earpiece", NULL, "Receiver"},
	{"AIN0", NULL, "Internal Mic"},
	{"AIN1", NULL, "Headset Mic"},
	{"AIN2", NULL, "Internal Mic"},
	{"Speaker", NULL, "SPK"},
};

static struct snd_soc_card mt6768_mt6358_card = {
	.name = "mt6768-sound",
	.owner = THIS_MODULE,
	.dai_link = mt6768_mt6358_dai_links,
	.num_links = ARRAY_SIZE(mt6768_mt6358_dai_links),
	.controls = mt6768_mt6358_snd_controls,
	.num_controls = ARRAY_SIZE(mt6768_mt6358_snd_controls),
	.dapm_widgets = mt6768_mt6358_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt6768_mt6358_dapm_widgets),
	.dapm_routes = mt6768_mt6358_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(mt6768_mt6358_dapm_routes),
};

static int mt6768_mt6358_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt6768_mt6358_card;
	struct device_node *platform_node, *codec_node, *speaker_node;
	struct mt6768_mt6358_priv *priv;
	struct snd_soc_dai_link *dai_link;
	int ret, i;

	card->dev = &pdev->dev;

	platform_node = of_parse_phandle(pdev->dev.of_node,
					 "mediatek,platform", 0);
	if (!platform_node) {
		dev_err(&pdev->dev, "Property 'mediatek,platform' missing or invalid\n");
		return -EINVAL;
	}

	codec_node = of_parse_phandle(pdev->dev.of_node,
				      "mediatek,audio-codec", 0);
	if (!codec_node) {
		dev_err(&pdev->dev, "Property 'mediatek,audio-codec' missing or invalid\n");
		ret = -EINVAL;
		goto out_platform;
	}

	speaker_node = of_parse_phandle(pdev->dev.of_node,
					"mediatek,speaker-codec", 0);
	if (!speaker_node) {
		struct device_node *child = of_get_child_by_name(pdev->dev.of_node,
								 "mediatek,speaker-codec");
		if (child) {
			speaker_node = of_parse_phandle(child, "sound-dai", 0);
			of_node_put(child);
		}
	}

	for_each_card_prelinks(card, i, dai_link) {
		if (!dai_link->platforms->name)
			dai_link->platforms->of_node = platform_node;

		if (strcmp(dai_link->name, "Primary Codec") == 0)
			dai_link->codecs->of_node = codec_node;

		if (strcmp(dai_link->name, "I2S3") == 0) {
			if (speaker_node) {
				dai_link->codecs->of_node = speaker_node;
			} else {
				dai_link->codecs = &snd_soc_dummy_dlc;
				dai_link->num_codecs = 1;
			}
		}

		if (strcmp(dai_link->name, "I2S0") == 0) {
			if (speaker_node) {
				dai_link->codecs->of_node = speaker_node;
			} else {
				dai_link->codecs = &snd_soc_dummy_dlc;
				dai_link->num_codecs = 1;
			}
		}
	}

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto out_codec;
	}

	snd_soc_card_set_drvdata(card, priv);

	ret = devm_snd_soc_register_card(&pdev->dev, card);

out_codec:
	of_node_put(speaker_node);
	of_node_put(codec_node);
out_platform:
	of_node_put(platform_node);
	return ret;
}

static const struct of_device_id mt6768_mt6358_dt_match[] = {
	{ .compatible = "mediatek,mt6768-sound", },
	{ .compatible = "mediatek,mt6768-mt6358-sound", },
	{}
};
MODULE_DEVICE_TABLE(of, mt6768_mt6358_dt_match);

static struct platform_driver mt6768_mt6358_driver = {
	.driver = {
		.name = "mt6768-mt6358",
		.of_match_table = mt6768_mt6358_dt_match,
		.pm = &snd_soc_pm_ops,
	},
	.probe = mt6768_mt6358_dev_probe,
};

module_platform_driver(mt6768_mt6358_driver);

MODULE_DESCRIPTION("MT6768-MT6358 ALSA SoC machine driver");
MODULE_AUTHOR("Gang Feng <gang.feng@mediatek.com>");
MODULE_LICENSE("GPL");
