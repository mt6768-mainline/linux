// SPDX-License-Identifier: GPL-2.0
//
// MediaTek ALSA SoC Audio DAI PCM Control
//
// Copyright (c) 2018 MediaTek Inc.
// Author: Michael Hsiao <michael.hsiao@mediatek.com>

#include <linux/regmap.h>
#include <sound/pcm_params.h>
#include "mt6768-afe-common.h"
#include "mt6768-interconnection.h"

enum AUD_TX_LCH_RPT {
	AUD_TX_LCH_RPT_NO_REPEAT = 0,
	AUD_TX_LCH_RPT_REPEAT = 1
};

enum AUD_VBT_16K_MODE {
	AUD_VBT_16K_MODE_DISABLE = 0,
	AUD_VBT_16K_MODE_ENABLE = 1
};

enum AUD_BT_MODE {
	AUD_BT_MODE_DUAL_MIC_ON_TX = 0,
	AUD_BT_MODE_SINGLE_MIC_ON_TX = 1
};

enum AUD_PCM_AFIFO_SRC {
	AUD_PCM_AFIFO_ASRC = 0,
	AUD_PCM_AFIFO_AFIFO = 1
};

enum AUD_PCM_WLEN {
	AUD_PCM_WLEN_PCM_32_BCK_CYCLES = 0,
	AUD_PCM_WLEN_PCM_64_BCK_CYCLES = 1
};

enum AUD_PCM_FMT {
	AUD_PCM_FMT_I2S = 0,
	AUD_PCM_FMT_EIAJ = 1,
	AUD_PCM_FMT_PCM_MODE_A = 2,
	AUD_PCM_FMT_PCM_MODE_B = 3
};

/* dai component */
static const struct snd_kcontrol_new mtk_pcm_2_playback_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1 Switch", AFE_CONN17,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1 Switch", AFE_CONN17,
				    I_DL2_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_pcm_2_playback_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2 Switch", AFE_CONN18,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2 Switch", AFE_CONN18,
				    I_DL2_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_pcm_2_playback_ch4_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH1 Switch", AFE_CONN24,
				    I_I2S0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH2 Switch", AFE_CONN24,
				    I_I2S0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1 Switch", AFE_CONN24,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH1 Switch", AFE_CONN24,
				    I_I2S2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH2 Switch", AFE_CONN24,
				    I_I2S2_CH2, 1, 0),
};

static const struct snd_soc_dapm_widget mtk_dai_pcm_widgets[] = {
	/* inter-connections */
	SND_SOC_DAPM_MIXER("PCM_2_PB_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_pcm_2_playback_ch1_mix,
			   ARRAY_SIZE(mtk_pcm_2_playback_ch1_mix)),
	SND_SOC_DAPM_MIXER("PCM_2_PB_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_pcm_2_playback_ch2_mix,
			   ARRAY_SIZE(mtk_pcm_2_playback_ch2_mix)),
	SND_SOC_DAPM_MIXER("PCM_2_PB_CH4", SND_SOC_NOPM, 0, 0,
			   mtk_pcm_2_playback_ch4_mix,
			   ARRAY_SIZE(mtk_pcm_2_playback_ch4_mix)),

	SND_SOC_DAPM_SUPPLY("PCM_2_EN",
			    PCM2_INTF_CON, PCM_EN_SFT, 0,
			    NULL, 0),
};

static const struct snd_soc_dapm_route mtk_dai_pcm_routes[] = {
	{"PCM 2 Playback", NULL, "PCM_2_PB_CH1"},
	{"PCM 2 Playback", NULL, "PCM_2_PB_CH2"},
	{"PCM 2 Playback", NULL, "PCM_2_PB_CH4"},

	{"PCM 2 Playback", NULL, "PCM_2_EN"},
	{"PCM 2 Capture", NULL, "PCM_2_EN"},

	{"PCM_2_PB_CH1", "DL2_CH1 Switch", "DL2"},
	{"PCM_2_PB_CH2", "DL2_CH2 Switch", "DL2"},
	{"PCM_2_PB_CH4", "DL1_CH1 Switch", "DL1"},
};

/* dai ops */
static int mtk_dai_pcm_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	unsigned int rate = params_rate(params);
	unsigned int rate_reg = mt6768_rate_transform(afe->dev, rate, dai->id);
	unsigned int pcm_con = 0;

	struct snd_soc_dapm_widget *p = snd_soc_dai_get_widget_playback(dai);
	struct snd_soc_dapm_widget *c = snd_soc_dai_get_widget_capture(dai);

	if ((p && p->active) || (c && c->active))
		return 0;

	switch (dai->id) {
	case MT6768_DAI_PCM_2:
		pcm_con |= AUD_TX_LCH_RPT_NO_REPEAT << TX_LCH_RPT_SFT;
		pcm_con |= AUD_VBT_16K_MODE_DISABLE << VBT_16K_MODE_SFT;
		pcm_con |= AUD_BT_MODE_DUAL_MIC_ON_TX << BT_MODE_SFT;
		pcm_con |= AUD_PCM_AFIFO_AFIFO << PCM_AFIFO_SFT;
		pcm_con |= AUD_PCM_WLEN_PCM_32_BCK_CYCLES << PCM_WLEN_SFT;
		pcm_con |= rate_reg << PCM_MODE_SFT;
		pcm_con |= AUD_PCM_FMT_PCM_MODE_B << PCM_FMT_SFT;

		regmap_update_bits(afe->regmap, PCM2_INTF_CON,
				   0xfffffffe, pcm_con);
		break;
	default:
		dev_warn(afe->dev, "%s(), id %d not supported\n",
			 __func__, dai->id);
		return -EINVAL;
	}

	return 0;
}

static const struct snd_soc_dai_ops mtk_dai_pcm_ops = {
	.hw_params = mtk_dai_pcm_hw_params,
};

#define MTK_PCM_RATES (SNDRV_PCM_RATE_8000 |\
		       SNDRV_PCM_RATE_16000 |\
		       SNDRV_PCM_RATE_32000 |\
		       SNDRV_PCM_RATE_48000)

#define MTK_PCM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S24_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mtk_dai_pcm_driver[] = {
	{
		.name = "PCM 2",
		.id = MT6768_DAI_PCM_2,
		.playback = {
			.stream_name = "PCM 2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.capture = {
			.stream_name = "PCM 2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mtk_dai_pcm_ops,
		.symmetric_rate = 1,
		.symmetric_sample_bits = 1,
	},
};

int mt6768_dai_pcm_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mtk_dai_pcm_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mtk_dai_pcm_driver);

	dai->dapm_widgets = mtk_dai_pcm_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mtk_dai_pcm_widgets);
	dai->dapm_routes = mtk_dai_pcm_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mtk_dai_pcm_routes);
	return 0;
}
