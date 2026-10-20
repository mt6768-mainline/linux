// SPDX-License-Identifier: GPL-2.0
//
// Mediatek ALSA SoC AFE platform driver for 6768
//
// Copyright (c) 2018 MediaTek Inc.
// Author: Michael Hsiao <michael.hsiao@mediatek.com>

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>

#include "mt6768-afe-common.h"
#include "mt6768-afe-clk.h"
#include "mt6768-afe-gpio.h"
#include "mt6768-interconnection.h"
#include "mt6768-reg.h"
#include "../common/mtk-afe-platform-driver.h"
#include "../common/mtk-afe-fe-dai.h"

enum {
	MTK_AFE_RATE_8K = 0,
	MTK_AFE_RATE_11K = 1,
	MTK_AFE_RATE_12K = 2,
	MTK_AFE_RATE_384K = 3,
	MTK_AFE_RATE_16K = 4,
	MTK_AFE_RATE_22K = 5,
	MTK_AFE_RATE_24K = 6,
	MTK_AFE_RATE_130K = 7,
	MTK_AFE_RATE_32K = 8,
	MTK_AFE_RATE_44K = 9,
	MTK_AFE_RATE_48K = 10,
	MTK_AFE_RATE_88K = 11,
	MTK_AFE_RATE_96K = 12,
	MTK_AFE_RATE_176K = 13,
	MTK_AFE_RATE_192K = 14,
	MTK_AFE_RATE_260K = 15,
};

enum {
	MTK_AFE_DAI_MEMIF_RATE_8K = 0,
	MTK_AFE_DAI_MEMIF_RATE_16K = 1,
	MTK_AFE_DAI_MEMIF_RATE_32K = 2,
	MTK_AFE_DAI_MEMIF_RATE_48K = 3,
};

unsigned int mt6768_general_rate_transform(struct device *dev,
					   unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MTK_AFE_RATE_8K;
	case 11025:
		return MTK_AFE_RATE_11K;
	case 12000:
		return MTK_AFE_RATE_12K;
	case 16000:
		return MTK_AFE_RATE_16K;
	case 22050:
		return MTK_AFE_RATE_22K;
	case 24000:
		return MTK_AFE_RATE_24K;
	case 32000:
		return MTK_AFE_RATE_32K;
	case 44100:
		return MTK_AFE_RATE_44K;
	case 48000:
		return MTK_AFE_RATE_48K;
	case 88200:
		return MTK_AFE_RATE_88K;
	case 96000:
		return MTK_AFE_RATE_96K;
	case 130000:
		return MTK_AFE_RATE_130K;
	case 176400:
		return MTK_AFE_RATE_176K;
	case 192000:
		return MTK_AFE_RATE_192K;
	case 260000:
		return MTK_AFE_RATE_260K;
	default:
		dev_warn(dev, "%s(), rate %u invalid, use %d\n",
			 __func__, rate, MTK_AFE_RATE_48K);
		return MTK_AFE_RATE_48K;
	}
}

static unsigned int dai_memif_rate_transform(struct device *dev,
					     unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MTK_AFE_DAI_MEMIF_RATE_8K;
	case 16000:
		return MTK_AFE_DAI_MEMIF_RATE_16K;
	case 32000:
		return MTK_AFE_DAI_MEMIF_RATE_32K;
	case 48000:
		return MTK_AFE_DAI_MEMIF_RATE_48K;
	default:
		dev_warn(dev, "%s(), rate %u invalid, use %d\n",
			 __func__, rate, MTK_AFE_DAI_MEMIF_RATE_16K);
		return MTK_AFE_DAI_MEMIF_RATE_16K;
	}
}

unsigned int mt6768_rate_transform(struct device *dev,
				   unsigned int rate, int aud_blk)
{
	switch (aud_blk) {
	case MT6768_MEMIF_MOD_DAI:
		return dai_memif_rate_transform(dev, rate);
	default:
		return mt6768_general_rate_transform(dev, rate);
	}
}

static const struct snd_pcm_hardware mt6768_afe_hardware = {
	.info = SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_MMAP_VALID,
	.formats = SNDRV_PCM_FMTBIT_S16_LE |
		   SNDRV_PCM_FMTBIT_S24_LE |
		   SNDRV_PCM_FMTBIT_S32_LE,
	.period_bytes_min = 256,
	.period_bytes_max = 4 * 48 * 1024,
	.periods_min = 2,
	.periods_max = 256,
	.buffer_bytes_max = 8 * 48 * 1024,
	.fifo_size = 0,
};

static int mt6768_memif_fs(struct snd_pcm_substream *substream,
			   unsigned int rate)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	int id = snd_soc_rtd_to_cpu(rtd, 0)->id;

	return mt6768_rate_transform(afe->dev, rate, id);
}

static int mt6768_irq_fs(struct snd_pcm_substream *substream, unsigned int rate)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);

	return mt6768_general_rate_transform(afe->dev, rate);
}

#define MTK_PCM_RATES (SNDRV_PCM_RATE_8000_48000 |\
		       SNDRV_PCM_RATE_88200 |\
		       SNDRV_PCM_RATE_96000 |\
		       SNDRV_PCM_RATE_176400 |\
		       SNDRV_PCM_RATE_192000)

#define MTK_PCM_DAI_RATES (SNDRV_PCM_RATE_8000 |\
			   SNDRV_PCM_RATE_16000 |\
			   SNDRV_PCM_RATE_32000 |\
			   SNDRV_PCM_RATE_48000)

#define MTK_PCM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S24_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mt6768_memif_dai_driver[] = {
	/* FE DAIs: memory interfaces to CPU */
	{
		.name = "DL1",
		.id = MT6768_MEMIF_DL1,
		.playback = {
			.stream_name = "DL1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mtk_afe_fe_ops,
	},
	{
		.name = "DL12",
		.id = MT6768_MEMIF_DL12,
		.playback = {
			.stream_name = "DL12",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mtk_afe_fe_ops,
	},
	{
		.name = "DL2",
		.id = MT6768_MEMIF_DL2,
		.playback = {
			.stream_name = "DL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mtk_afe_fe_ops,
	},
	{
		.name = "DL3",
		.id = MT6768_MEMIF_DL3,
		.playback = {
			.stream_name = "DL3",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mtk_afe_fe_ops,
	},
	{
		.name = "UL1",
		.id = MT6768_MEMIF_VUL12,
		.capture = {
			.stream_name = "UL1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mtk_afe_fe_ops,
	},
	{
		.name = "UL2",
		.id = MT6768_MEMIF_AWB,
		.capture = {
			.stream_name = "UL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mtk_afe_fe_ops,
	},
	{
		.name = "UL3",
		.id = MT6768_MEMIF_VUL2,
		.capture = {
			.stream_name = "UL3",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mtk_afe_fe_ops,
	},
	{
		.name = "UL4",
		.id = MT6768_MEMIF_AWB2,
		.capture = {
			.stream_name = "UL4",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mtk_afe_fe_ops,
	},
	{
		.name = "UL_MONO_1",
		.id = MT6768_MEMIF_MOD_DAI,
		.capture = {
			.stream_name = "UL_MONO_1",
			.channels_min = 1,
			.channels_max = 1,
			.rates = MTK_PCM_DAI_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mtk_afe_fe_ops,
	},
};

/* dma widget & routes */
static const struct snd_kcontrol_new memif_ul1_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1 Switch", AFE_CONN21,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH1 Switch", AFE_CONN21,
				    I_I2S0_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul1_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1 Switch", AFE_CONN22,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2 Switch", AFE_CONN22,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH2 Switch", AFE_CONN22,
				    I_I2S0_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul2_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1 Switch", AFE_CONN5,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1 Switch", AFE_CONN5,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1 Switch", AFE_CONN5,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1 Switch", AFE_CONN5,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH1 Switch", AFE_CONN5,
				    I_I2S0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH1 Switch", AFE_CONN5,
				    I_I2S2_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul2_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2 Switch", AFE_CONN6,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2 Switch", AFE_CONN6,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2 Switch", AFE_CONN6,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2 Switch", AFE_CONN6,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH2 Switch", AFE_CONN6,
				    I_I2S0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH2 Switch", AFE_CONN6,
				    I_I2S2_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul3_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1 Switch", AFE_CONN32,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH1 Switch", AFE_CONN32,
				    I_I2S2_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul3_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2 Switch", AFE_CONN33,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH2 Switch", AFE_CONN33,
				    I_I2S2_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul4_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1 Switch", AFE_CONN38,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH1 Switch", AFE_CONN38,
				    I_I2S0_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul4_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2 Switch", AFE_CONN39,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH2 Switch", AFE_CONN39,
				    I_I2S0_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul_mono_1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_2_CAP_CH1 Switch", AFE_CONN12,
				    I_PCM_2_CAP_CH1, 1, 0),
};

static const struct snd_soc_dapm_widget mt6768_memif_widgets[] = {
	SND_SOC_DAPM_MIXER("UL1_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul1_ch1_mix, ARRAY_SIZE(memif_ul1_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL1_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul1_ch2_mix, ARRAY_SIZE(memif_ul1_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL2_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul2_ch1_mix, ARRAY_SIZE(memif_ul2_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL2_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul2_ch2_mix, ARRAY_SIZE(memif_ul2_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL3_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul3_ch1_mix, ARRAY_SIZE(memif_ul3_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL3_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul3_ch2_mix, ARRAY_SIZE(memif_ul3_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL4_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul4_ch1_mix, ARRAY_SIZE(memif_ul4_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL4_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul4_ch2_mix, ARRAY_SIZE(memif_ul4_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL_MONO_1_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul_mono_1_mix,
			   ARRAY_SIZE(memif_ul_mono_1_mix)),
};

static const struct snd_soc_dapm_route mt6768_memif_routes[] = {
	/* capture */
	{"UL1", NULL, "UL1_CH1"},
	{"UL1", NULL, "UL1_CH2"},
	{"UL1_CH1", "ADDA_UL_CH1 Switch", "ADDA Capture"},
	{"UL1_CH2", "ADDA_UL_CH1 Switch", "ADDA Capture"},
	{"UL1_CH2", "ADDA_UL_CH2 Switch", "ADDA Capture"},
	{"UL1_CH1", "I2S0_CH1 Switch", "I2S0"},
	{"UL1_CH2", "I2S0_CH2 Switch", "I2S0"},

	{"UL2", NULL, "UL2_CH1"},
	{"UL2", NULL, "UL2_CH2"},
	{"UL2_CH1", "ADDA_UL_CH1 Switch", "ADDA Capture"},
	{"UL2_CH2", "ADDA_UL_CH2 Switch", "ADDA Capture"},
	{"UL2_CH1", "I2S0_CH1 Switch", "I2S0"},
	{"UL2_CH2", "I2S0_CH2 Switch", "I2S0"},
	{"UL2_CH1", "I2S2_CH1 Switch", "I2S2"},
	{"UL2_CH2", "I2S2_CH2 Switch", "I2S2"},

	{"UL3", NULL, "UL3_CH1"},
	{"UL3", NULL, "UL3_CH2"},
	{"UL3_CH1", "ADDA_UL_CH1 Switch", "ADDA Capture"},
	{"UL3_CH2", "ADDA_UL_CH2 Switch", "ADDA Capture"},
	{"UL3_CH1", "I2S2_CH1 Switch", "I2S2"},
	{"UL3_CH2", "I2S2_CH2 Switch", "I2S2"},

	{"UL4", NULL, "UL4_CH1"},
	{"UL4", NULL, "UL4_CH2"},
	{"UL4_CH1", "ADDA_UL_CH1 Switch", "ADDA Capture"},
	{"UL4_CH2", "ADDA_UL_CH2 Switch", "ADDA Capture"},
	{"UL4_CH1", "I2S0_CH1 Switch", "I2S0"},
	{"UL4_CH2", "I2S0_CH2 Switch", "I2S0"},

	{"UL_MONO_1", NULL, "UL_MONO_1_CH1"},
	{"UL_MONO_1_CH1", "PCM_2_CAP_CH1 Switch", "PCM 2 Capture"},
};

static const struct snd_soc_component_driver mt6768_afe_pcm_dai_component = {
	.name = "mt6768-afe-pcm-dai",
};

static const struct mtk_base_memif_data memif_data[MT6768_MEMIF_NUM] = {
	[MT6768_MEMIF_DL1] = {
		.name = "DL1",
		.id = MT6768_MEMIF_DL1,
		.reg_ofs_base = AFE_DL1_BASE,
		.reg_ofs_cur = AFE_DL1_CUR,
		.reg_ofs_end = AFE_DL1_END,
		.reg_ofs_base_msb = AFE_DL1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL1_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL1_END_MSB,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = DL1_MODE_SFT,
		.fs_maskbit = DL1_MODE_MASK,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = DL1_DATA_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = DL1_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = DL1_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_MEMIF_PBUF_SIZE,
		.pbuf_mask = DL1_PBUF_SIZE_MASK,
		.pbuf_shift = DL1_PBUF_SIZE_SFT,
		.minlen_reg = AFE_MEMIF_MINLEN,
		.minlen_mask = DL1_MINLEN_MASK,
		.minlen_shift = DL1_MINLEN_SFT,
	},
	[MT6768_MEMIF_DL12] = {
		.name = "DL12",
		.id = MT6768_MEMIF_DL12,
		.reg_ofs_base = AFE_DL1_D2_BASE,
		.reg_ofs_cur = AFE_DL1_D2_CUR,
		.reg_ofs_end = AFE_DL1_D2_END,
		.reg_ofs_base_msb = AFE_DL1_D2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL1_D2_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL1_D2_END_MSB,
		.fs_reg = AFE_DAC_CON0,
		.fs_shift = DL1_DATA2_MODE_SFT,
		.fs_maskbit = DL1_DATA2_MODE_MASK,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = DL1_DATA2_DATA_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = DL12_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = DL12_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_MEMIF_PBUF_SIZE,
		.pbuf_mask = DL1_DATA2_PBUF_SIZE_MASK,
		.pbuf_shift = DL1_DATA2_PBUF_SIZE_SFT,
		.minlen_reg = AFE_MEMIF_MINLEN,
		.minlen_mask = DL1_DATA2_MINLEN_MASK,
		.minlen_shift = DL1_DATA2_MINLEN_SFT,
	},
	[MT6768_MEMIF_DL2] = {
		.name = "DL2",
		.id = MT6768_MEMIF_DL2,
		.reg_ofs_base = AFE_DL2_BASE,
		.reg_ofs_cur = AFE_DL2_CUR,
		.reg_ofs_end = AFE_DL2_END,
		.reg_ofs_base_msb = AFE_DL2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL2_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL2_END_MSB,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = DL2_MODE_SFT,
		.fs_maskbit = DL2_MODE_MASK,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = DL2_DATA_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = DL2_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = DL2_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_MEMIF_PBUF_SIZE,
		.pbuf_mask = DL2_PBUF_SIZE_MASK,
		.pbuf_shift = DL2_PBUF_SIZE_SFT,
		.minlen_reg = AFE_MEMIF_MINLEN,
		.minlen_mask = DL2_MINLEN_MASK,
		.minlen_shift = DL2_MINLEN_SFT,
	},
	[MT6768_MEMIF_DL3] = {
		.name = "DL3",
		.id = MT6768_MEMIF_DL3,
		.reg_ofs_base = AFE_DL3_BASE,
		.reg_ofs_cur = AFE_DL3_CUR,
		.reg_ofs_end = AFE_DL3_END,
		.reg_ofs_base_msb = AFE_DL3_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL3_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL3_END_MSB,
		.fs_reg = AFE_DAC_CON2,
		.fs_shift = DL3_MODE_SFT,
		.fs_maskbit = DL3_MODE_MASK,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = DL3_DATA_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = DL3_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = DL3_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_MEMIF_PBUF_SIZE,
		.pbuf_mask = DL3_PBUF_SIZE_MASK,
		.pbuf_shift = DL3_PBUF_SIZE_SFT,
		.minlen_reg = AFE_MEMIF_MINLEN,
		.minlen_mask = DL3_MINLEN_MASK,
		.minlen_shift = DL3_MINLEN_SFT,
	},
	[MT6768_MEMIF_MOD_DAI] = {
		.name = "MOD_DAI",
		.id = MT6768_MEMIF_MOD_DAI,
		.reg_ofs_base = AFE_MOD_DAI_BASE,
		.reg_ofs_cur = AFE_MOD_DAI_CUR,
		.reg_ofs_end = AFE_MOD_DAI_END,
		.reg_ofs_base_msb = AFE_MOD_DAI_BASE_MSB,
		.reg_ofs_cur_msb = AFE_MOD_DAI_CUR_MSB,
		.reg_ofs_end_msb = AFE_MOD_DAI_END_MSB,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = MOD_DAI_MODE_SFT,
		.fs_maskbit = MOD_DAI_MODE_MASK,
		.mono_reg = AFE_DAC_CON0,
		.mono_shift = MOD_DAI_DUP_WR_SFT,
		.mono_invert = 1,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = MOD_DAI_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = MOD_DAI_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6768_MEMIF_VUL] = {
		.name = "VUL",
		.id = MT6768_MEMIF_VUL,
		.reg_ofs_base = AFE_VUL_BASE,
		.reg_ofs_cur = AFE_VUL_CUR,
		.reg_ofs_end = AFE_VUL_END,
		.reg_ofs_base_msb = AFE_VUL_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL_END_MSB,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = VUL_MODE_SFT,
		.fs_maskbit = VUL_MODE_MASK,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = VUL_DATA_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = VUL_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = VUL_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6768_MEMIF_VUL12] = {
		.name = "VUL12",
		.id = MT6768_MEMIF_VUL12,
		.reg_ofs_base = AFE_VUL_D2_BASE,
		.reg_ofs_cur = AFE_VUL_D2_CUR,
		.reg_ofs_end = AFE_VUL_D2_END,
		.reg_ofs_base_msb = AFE_VUL_D2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL_D2_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL_D2_END_MSB,
		.fs_reg = AFE_DAC_CON0,
		.fs_shift = VUL_DATA2_MODE_SFT,
		.fs_maskbit = VUL_DATA2_MODE_MASK,
		.mono_reg = AFE_DAC_CON0,
		.mono_shift = VUL_DATA2_DATA_SFT,
		.quad_ch_reg = AFE_MEMIF_PBUF_SIZE,
		.quad_ch_mask = VUL12_4CH_EN_MASK,
		.quad_ch_shift = VUL12_4CH_EN_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = VUL_DATA2_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = VUL12_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6768_MEMIF_VUL2] = {
		.name = "VUL2",
		.id = MT6768_MEMIF_VUL2,
		.reg_ofs_base = AFE_VUL2_BASE,
		.reg_ofs_cur = AFE_VUL2_CUR,
		.reg_ofs_end = AFE_VUL2_END,
		.reg_ofs_base_msb = AFE_VUL2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL2_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL2_END_MSB,
		.fs_reg = AFE_DAC_CON2,
		.fs_shift = VUL2_MODE_SFT,
		.fs_maskbit = VUL2_MODE_MASK,
		.mono_reg = AFE_DAC_CON2,
		.mono_shift = VUL2_DATA_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = VUL2_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = VUL2_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6768_MEMIF_AWB] = {
		.name = "AWB",
		.id = MT6768_MEMIF_AWB,
		.reg_ofs_base = AFE_AWB_BASE,
		.reg_ofs_cur = AFE_AWB_CUR,
		.reg_ofs_end = AFE_AWB_END,
		.reg_ofs_base_msb = AFE_AWB_BASE_MSB,
		.reg_ofs_cur_msb = AFE_AWB_CUR_MSB,
		.reg_ofs_end_msb = AFE_AWB_END_MSB,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = AWB_MODE_SFT,
		.fs_maskbit = AWB_MODE_MASK,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = AWB_DATA_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = AWB_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = AWB_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6768_MEMIF_AWB2] = {
		.name = "AWB2",
		.id = MT6768_MEMIF_AWB2,
		.reg_ofs_base = AFE_AWB2_BASE,
		.reg_ofs_cur = AFE_AWB2_CUR,
		.reg_ofs_end = AFE_AWB2_END,
		.reg_ofs_base_msb = AFE_AWB2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_AWB2_CUR_MSB,
		.reg_ofs_end_msb = AFE_AWB2_END_MSB,
		.fs_reg = AFE_DAC_CON2,
		.fs_shift = AWB2_MODE_SFT,
		.fs_maskbit = AWB2_MODE_MASK,
		.mono_reg = AFE_DAC_CON2,
		.mono_shift = AWB2_DATA_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = AWB2_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = AWB2_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
};

#define MT6768_AFE_IRQ_BASE(_id, _fs_reg, _fs_shift)	\
	[MT6768_IRQ_##_id] = {	\
		.id = MT6768_IRQ_##_id,			\
		.irq_cnt_reg = AFE_IRQ_MCU_CNT##_id,	\
		.irq_cnt_shift = 0,			\
		.irq_cnt_maskbit = 0x3ffff,		\
		.irq_fs_reg = (_fs_reg),		\
		.irq_fs_shift = (_fs_shift),		\
		.irq_fs_maskbit = 0xf,			\
		.irq_en_reg = AFE_IRQ_MCU_CON0,		\
		.irq_en_shift = IRQ##_id##_MCU_ON_SFT,	\
		.irq_clr_reg = AFE_IRQ_MCU_CLR,		\
		.irq_clr_shift = IRQ##_id##_MCU_CLR_SFT,\
	}

static const struct mtk_base_irq_data irq_data[MT6768_IRQ_NUM] = {
	MT6768_AFE_IRQ_BASE(0, AFE_IRQ_MCU_CON1, 0),
	MT6768_AFE_IRQ_BASE(1, AFE_IRQ_MCU_CON1, 4),
	MT6768_AFE_IRQ_BASE(2, AFE_IRQ_MCU_CON1, 8),
	MT6768_AFE_IRQ_BASE(3, AFE_IRQ_MCU_CON1, 12),
	MT6768_AFE_IRQ_BASE(4, AFE_IRQ_MCU_CON1, 16),
	MT6768_AFE_IRQ_BASE(5, AFE_IRQ_MCU_CON1, 20),
	MT6768_AFE_IRQ_BASE(6, AFE_IRQ_MCU_CON1, 24),
	MT6768_AFE_IRQ_BASE(7, AFE_IRQ_MCU_CON1, 28),
	MT6768_AFE_IRQ_BASE(11, AFE_IRQ_MCU_CON2, 0),
	MT6768_AFE_IRQ_BASE(12, AFE_IRQ_MCU_CON2, 4),
};

static bool mt6768_is_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case AUDIO_TOP_CON0:
	case AUDIO_TOP_CON1:
	case AUDIO_TOP_CON3:
	case AFE_DAC_CON0:
	case AFE_IRQ_MCU_CON0:
	case AFE_IRQ_MCU_EN1:
	case AFE_IRQ_MCU_EN:
	case AFE_DL1_CUR_MSB:
	case AFE_DL1_CUR:
	case AFE_DL1_END:
	case AFE_DL2_CUR_MSB:
	case AFE_DL2_CUR:
	case AFE_DL2_END:
	case AFE_DL3_CUR_MSB:
	case AFE_DL3_CUR:
	case AFE_DL3_END:
	case AFE_DL1_D2_CUR_MSB:
	case AFE_DL1_D2_CUR:
	case AFE_DL1_D2_END:
	case AFE_ADDA_SRC_DEBUG_MON0:
	case AFE_ADDA_SRC_DEBUG_MON1:
	case AFE_ADDA_UL_SRC_MON0:
	case AFE_ADDA_UL_SRC_MON1:
	case AFE_VUL_CUR_MSB:
	case AFE_VUL_CUR:
	case AFE_VUL_END:
	case AFE_SIDETONE_MON:
	case AFE_SIDETONE_CON0:
	case AFE_SIDETONE_COEFF:
	case AFE_VUL2_CUR_MSB:
	case AFE_VUL2_CUR:
	case AFE_VUL2_END:
	case AFE_BUS_MON0:
	case AFE_MRGIF_MON0:
	case AFE_MRGIF_MON1:
	case AFE_MRGIF_MON2:
	case AFE_DAC_MON:
	case AFE_IRQ0_MCU_CNT_MON:
	case AFE_IRQ6_MCU_CNT_MON:
	case AFE_VUL_D2_CUR_MSB:
	case AFE_VUL_D2_CUR:
	case AFE_VUL_D2_END:
	case AFE_IRQ3_MCU_CNT_MON:
	case AFE_IRQ4_MCU_CNT_MON:
	case AFE_IRQ_MCU_STATUS:
	case AFE_IRQ_MCU_CLR:
	case AFE_IRQ_MCU_MON2:
	case AFE_IRQ1_MCU_CNT_MON:
	case AFE_IRQ2_MCU_CNT_MON:
	case AFE_IRQ1_MCU_EN_CNT_MON:
	case AFE_IRQ5_MCU_CNT_MON:
	case AFE_IRQ7_MCU_CNT_MON:
	case AFE_GAIN1_CUR:
	case AFE_GAIN2_CUR:
	case AFE_SRAM_DELSEL_CON0:
	case AFE_SRAM_DELSEL_CON2:
	case AFE_SRAM_DELSEL_CON3:
	case AFE_ASRC_2CH_CON12:
	case AFE_ASRC_2CH_CON13:
	case FPGA_CFG0:
	case FPGA_CFG1:
	case FPGA_CFG2:
	case FPGA_CFG3:
	case AUDIO_TOP_DBG_MON0:
	case AUDIO_TOP_DBG_MON1:
	case AFE_IRQ8_MCU_CNT_MON:
	case AFE_IRQ11_MCU_CNT_MON:
	case AFE_IRQ12_MCU_CNT_MON:
	case AFE_CBIP_MON0:
	case AFE_CBIP_SLV_MUX_MON0:
	case AFE_CBIP_SLV_DECODER_MON0:
	case AFE_AWB_CUR_MSB:
	case AFE_AWB_CUR:
	case AFE_AWB_END:
	case AFE_AWB2_CUR_MSB:
	case AFE_AWB2_CUR:
	case AFE_AWB2_END:
	case AFE_MOD_DAI_CUR_MSB:
	case AFE_MOD_DAI_CUR:
	case AFE_MOD_DAI_END:
	case AFE_ADDA_DL_SDM_FIFO_MON:
	case AFE_ADDA_DL_SRC_LCH_MON:
	case AFE_ADDA_DL_SRC_RCH_MON:
	case AFE_ADDA_DL_SDM_OUT_MON:
	case AFE_CONNSYS_I2S_MON:
	case AFE_ASRC_2CH_CON0:
	case AFE_ASRC_2CH_CON2:
	case AFE_ASRC_2CH_CON3:
	case AFE_ASRC_2CH_CON4:
	case AFE_ASRC_2CH_CON5:
	case AFE_ASRC_2CH_CON7:
	case AFE_ASRC_2CH_CON8:
	case AFE_ADDA_MTKAIF_MON0:
	case AFE_ADDA_MTKAIF_MON1:
	case AFE_AUD_PAD_TOP:
	case AFE_APLL_TUNER_CFG:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config mt6768_afe_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.volatile_reg = mt6768_is_volatile_reg,
	.max_register = AFE_MAX_REGISTER,
	.num_reg_defaults_raw = AFE_MAX_REGISTER,
	.cache_type = REGCACHE_FLAT,
};

static irqreturn_t mt6768_afe_irq_handler(int irq_id, void *dev)
{
	struct mtk_base_afe *afe = dev;
	struct mtk_base_afe_memif *memif;
	struct mtk_base_afe_irq *irq;
	unsigned int status, status_mcu, mcu_en;
	int ret, i;
	irqreturn_t iret = IRQ_NONE;

	ret = regmap_read(afe->regmap, AFE_IRQ_MCU_STATUS, &status);
	if (ret || !status)
		return IRQ_NONE;

	ret = regmap_read(afe->regmap, AFE_IRQ_MCU_EN, &mcu_en);
	if (ret)
		mcu_en = 0xffff;

	status_mcu = status & mcu_en & AFE_IRQ_STATUS_BITS;

	for (i = 0; i < MT6768_MEMIF_NUM; i++) {
		memif = &afe->memif[i];
		if (!memif->substream)
			continue;

		if (memif->irq_usage < 0)
			continue;

		irq = &afe->irqs[memif->irq_usage];

		if (status_mcu & (1 << irq->irq_data->irq_en_shift)) {
			snd_pcm_period_elapsed(memif->substream);
			iret = IRQ_HANDLED;
		}
	}

	/* clear irq */
	regmap_write(afe->regmap, AFE_IRQ_MCU_CLR, status_mcu);

	return iret;
}

static int mt6768_afe_runtime_suspend(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);

	if (!afe->regmap)
		goto skip_regmap;

	/* disable AFE */
	regmap_update_bits(afe->regmap, AFE_DAC_CON0, 0x1, 0x0);

	/* clear all irq status */
	regmap_update_bits(afe->regmap, AFE_IRQ_MCU_CLR, 0xffff, 0xffff);

	regcache_cache_only(afe->regmap, true);
	regcache_mark_dirty(afe->regmap);

skip_regmap:
	mt6768_afe_disable_clock(afe);
	return 0;
}

static int mt6768_afe_runtime_resume(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	int ret;

	ret = mt6768_afe_enable_clock(afe);
	if (ret)
		return ret;

	if (!afe->regmap)
		return 0;

	regcache_cache_only(afe->regmap, false);
	regcache_sync(afe->regmap);

	/* enable audio sys DCM for power saving */
	regmap_update_bits(afe->regmap, AUDIO_TOP_CON0, 0x1 << 29, 0x1 << 29);

	/* force cpu use 8_24 format when writing 32bit data */
	regmap_update_bits(afe->regmap, AFE_MEMIF_MSB, 1 << 28, 0 << 28);

	/* set all output port to 24bit */
	regmap_write(afe->regmap, AFE_CONN_24BIT, 0xffffffff);
	regmap_write(afe->regmap, AFE_CONN_24BIT_1, 0xffffffff);

	/* unmask MCU interrupts */
	regmap_update_bits(afe->regmap, AFE_IRQ_MCU_EN, 0xffff, 0xffff);

	/* enable AFE */
	regmap_update_bits(afe->regmap, AFE_DAC_CON0, 0x1, 0x1);

	return 0;
}

static int mt6768_dai_memif_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mt6768_memif_dai_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mt6768_memif_dai_driver);

	dai->dapm_widgets = mt6768_memif_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mt6768_memif_widgets);
	dai->dapm_routes = mt6768_memif_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mt6768_memif_routes);

	return 0;
}

typedef int (*dai_register_cb)(struct mtk_base_afe *);
static const dai_register_cb dai_register_cbs[] = {
	mt6768_dai_adda_register,
	mt6768_dai_i2s_register,
	mt6768_dai_pcm_register,
	mt6768_dai_hostless_register,
	mt6768_dai_memif_register,
};

static int mt6768_afe_pcm_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_base_afe *afe;
	struct mt6768_afe_private *afe_priv;
	struct reset_control *rstc;
	int irq_id, ret, i;

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));
	if (ret)
		return ret;

	afe = devm_kzalloc(dev, sizeof(*afe), GFP_KERNEL);
	if (!afe)
		return -ENOMEM;

	platform_set_drvdata(pdev, afe);
	afe->platform_priv = devm_kzalloc(dev, sizeof(*afe_priv), GFP_KERNEL);
	if (!afe->platform_priv)
		return -ENOMEM;

	afe_priv = afe->platform_priv;
	afe->dev = dev;

	ret = mt6768_init_clock(afe);
	if (ret)
		return ret;

	pm_runtime_enable(dev);

	/* regmap init */
	afe->regmap = syscon_node_to_regmap(dev->parent->of_node);
	if (IS_ERR(afe->regmap)) {
		dev_err(dev, "could not get regmap from parent\n");
		ret = PTR_ERR(afe->regmap);
		goto err_pm_disable;
	}

	ret = regmap_attach_dev(dev, afe->regmap, &mt6768_afe_regmap_config);
	if (ret) {
		dev_warn(dev, "regmap_attach_dev fail, ret %d\n", ret);
		goto err_pm_disable;
	}

	rstc = devm_reset_control_get_optional_exclusive(dev, "audiosys");
	if (IS_ERR(rstc)) {
		ret = PTR_ERR(rstc);
		dev_err(dev, "could not get audiosys reset: %d\n", ret);
		goto err_pm_disable;
	}

	if (rstc) {
		ret = reset_control_reset(rstc);
		if (ret) {
			dev_err(dev, "failed to trigger audio reset: %d\n", ret);
			goto err_pm_disable;
		}
	}

	afe_priv->pm_runtime_bypass_reg_ctl = true;
	ret = pm_runtime_resume_and_get(dev);
	if (ret) {
		afe_priv->pm_runtime_bypass_reg_ctl = false;
		goto err_pm_disable;
	}

	ret = regmap_reinit_cache(afe->regmap, &mt6768_afe_regmap_config);
	pm_runtime_put_sync(dev);
	afe_priv->pm_runtime_bypass_reg_ctl = false;

	if (ret) {
		dev_err(dev, "regmap_reinit_cache fail, ret %d\n", ret);
		goto err_pm_disable;
	}

	regcache_cache_only(afe->regmap, true);
	regcache_mark_dirty(afe->regmap);

	/* pinctrl init */
	mt6768_afe_gpio_init(afe);

	/* init memif */
	afe->memif_size = MT6768_MEMIF_NUM;
	afe->memif = devm_kcalloc(dev, afe->memif_size, sizeof(*afe->memif),
				  GFP_KERNEL);
	if (!afe->memif) {
		ret = -ENOMEM;
		goto err_pm_disable;
	}

	for (i = 0; i < afe->memif_size; i++) {
		afe->memif[i].data = &memif_data[i];
		afe->memif[i].irq_usage = -1;
	}

	mutex_init(&afe->irq_alloc_lock);

	/* init irqs */
	afe->irqs_size = MT6768_IRQ_NUM;
	afe->irqs = devm_kcalloc(dev, afe->irqs_size, sizeof(*afe->irqs),
				 GFP_KERNEL);
	if (!afe->irqs) {
		ret = -ENOMEM;
		goto err_pm_disable;
	}

	for (i = 0; i < afe->irqs_size; i++)
		afe->irqs[i].irq_data = &irq_data[i];

	irq_id = platform_get_irq(pdev, 0);
	if (irq_id < 0) {
		ret = irq_id;
		goto err_pm_disable;
	}

	ret = devm_request_irq(dev, irq_id, mt6768_afe_irq_handler,
			       IRQF_TRIGGER_NONE, "asys-isr", (void *)afe);
	if (ret)
		goto err_pm_disable;

	/* init sub_dais */
	INIT_LIST_HEAD(&afe->sub_dais);

	for (i = 0; i < ARRAY_SIZE(dai_register_cbs); i++) {
		ret = dai_register_cbs[i](afe);
		if (ret) {
			dev_warn(dev, "dai register i %d fail, ret %d\n",
				 i, ret);
			goto err_pm_disable;
		}
	}

	ret = mtk_afe_combine_sub_dai(afe);
	if (ret) {
		dev_warn(dev, "mtk_afe_combine_sub_dai fail, ret %d\n", ret);
		goto err_pm_disable;
	}

	afe->mtk_afe_hardware = &mt6768_afe_hardware;
	afe->memif_fs = mt6768_memif_fs;
	afe->irq_fs = mt6768_irq_fs;

	afe->runtime_resume = mt6768_afe_runtime_resume;
	afe->runtime_suspend = mt6768_afe_runtime_suspend;

	ret = devm_snd_soc_register_component(dev, &mtk_afe_pcm_platform,
					      NULL, 0);
	if (ret) {
		dev_warn(dev, "err_platform\n");
		goto err_pm_disable;
	}

	ret = devm_snd_soc_register_component(dev, &mt6768_afe_pcm_dai_component,
					      afe->dai_drivers,
					      afe->num_dai_drivers);
	if (ret) {
		dev_warn(dev, "err_dai_component\n");
		goto err_pm_disable;
	}

	return 0;

err_pm_disable:
	pm_runtime_disable(dev);
	return ret;
}

static void mt6768_afe_pcm_dev_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pm_runtime_disable(dev);
	if (!pm_runtime_status_suspended(dev))
		mt6768_afe_runtime_suspend(dev);
}

static const struct of_device_id mt6768_afe_pcm_dt_match[] = {
	{ .compatible = "mediatek,mt6768-audio", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6768_afe_pcm_dt_match);

static const struct dev_pm_ops mt6768_afe_pm_ops = {
	RUNTIME_PM_OPS(mt6768_afe_runtime_suspend,
		       mt6768_afe_runtime_resume, NULL)
};

static struct platform_driver mt6768_afe_pcm_driver = {
	.driver = {
		.name = "mt6768-audio",
		.of_match_table = mt6768_afe_pcm_dt_match,
		.pm = pm_ptr(&mt6768_afe_pm_ops),
	},
	.probe = mt6768_afe_pcm_dev_probe,
	.remove = mt6768_afe_pcm_dev_remove,
};

module_platform_driver(mt6768_afe_pcm_driver);

MODULE_DESCRIPTION("Mediatek ALSA SoC AFE platform driver for 6768");
MODULE_AUTHOR("Michael Hsiao <michael.hsiao@mediatek.com>");
MODULE_LICENSE("GPL");
