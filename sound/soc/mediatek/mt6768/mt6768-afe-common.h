/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt6768-afe-common.h  --  Mediatek 6768 audio driver definitions
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

#ifndef _MT_6768_AFE_COMMON_H_
#define _MT_6768_AFE_COMMON_H_

#include <sound/soc.h>
#include <linux/list.h>
#include <linux/regmap.h>
#include "mt6768-reg.h"
#include "../common/mtk-base-afe.h"

enum {
	MT6768_MEMIF_DL1,
	MT6768_MEMIF_DL12,
	MT6768_MEMIF_DL2,
	MT6768_MEMIF_DL3,
	MT6768_MEMIF_MOD_DAI,
	MT6768_MEMIF_VUL,
	MT6768_MEMIF_VUL12,
	MT6768_MEMIF_VUL2,
	MT6768_MEMIF_AWB,
	MT6768_MEMIF_AWB2,
	MT6768_MEMIF_NUM,
	MT6768_DAI_ADDA = MT6768_MEMIF_NUM,
	MT6768_DAI_PCM_2,
	MT6768_DAI_I2S_0,
	MT6768_DAI_I2S_1,
	MT6768_DAI_I2S_2,
	MT6768_DAI_I2S_3,
	MT6768_DAI_CONNSYS_I2S,
	MT6768_DAI_HOSTLESS_LPBK,
	MT6768_DAI_HOSTLESS_SPEECH,
	MT6768_DAI_NUM,
};

enum {
	MT6768_IRQ_0,
	MT6768_IRQ_1,
	MT6768_IRQ_2,
	MT6768_IRQ_3,
	MT6768_IRQ_4,
	MT6768_IRQ_5,
	MT6768_IRQ_6,
	MT6768_IRQ_7,
	MT6768_IRQ_11,
	MT6768_IRQ_12,
	MT6768_IRQ_NUM,
};

enum {
	MT6768_MTKAIF_PROTOCOL_1 = 0,
	MT6768_MTKAIF_PROTOCOL_2,
	MT6768_MTKAIF_PROTOCOL_2_CLK_P2,
};

/* MCLK */
enum {
	MT6768_I2S0_MCK = 0,
	MT6768_I2S1_MCK,
	MT6768_I2S2_MCK,
	MT6768_I2S3_MCK,
	MT6768_MCK_NUM,
};

struct clk;

struct mt6768_afe_private {
	struct clk **clk;
	struct regmap *topckgen;
	struct regmap *apmixed;

	int pm_runtime_bypass_reg_ctl;

	/* dai */
	void *dai_priv[MT6768_DAI_NUM];

	/* adda */
	int mtkaif_protocol;
	int mtkaif_calibration_ok;
	int mtkaif_chosen_phase[4];
	int mtkaif_phase_cycle[4];
	int mtkaif_calibration_num_phase;
	int mtkaif_dmic;

	/* mck */
	int mck_rate[MT6768_MCK_NUM];
};

unsigned int mt6768_general_rate_transform(struct device *dev,
					   unsigned int rate);
unsigned int mt6768_rate_transform(struct device *dev,
				   unsigned int rate, int aud_blk);

int mt6768_dai_i2s_set_share(struct mtk_base_afe *afe, const char *main_i2s_name,
			     const char *secondary_i2s_name);

int mt6768_dai_adda_register(struct mtk_base_afe *afe);
int mt6768_dai_pcm_register(struct mtk_base_afe *afe);
int mt6768_dai_i2s_register(struct mtk_base_afe *afe);
int mt6768_dai_hostless_register(struct mtk_base_afe *afe);

#endif
