// SPDX-License-Identifier: GPL-2.0
//
// mt6768-afe-clk.c  --  Mediatek 6768 afe clock ctrl
//
// Copyright (c) 2018 MediaTek Inc.
// Author: Michael Hsiao <michael.hsiao@mediatek.com>

#include <linux/clk.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include "mt6768-afe-common.h"
#include "mt6768-afe-clk.h"
#include "mt6768-reg.h"

#define AP_PLL_CON3		0x000c
#define CLK_AUDDIV_0		0x0320

enum {
	CLK_AFE = 0,
	CLK_TML,
	CLK_APLL22M,
	CLK_APLL24M,
	CLK_APLL1_TUNER,
	CLK_INFRA_AXI,
	CLK_INFRA_26M,
	CLK_MUX_AUDIO,
	CLK_MUX_AUDIOINTBUS,
	CLK_TOP_SYSPLL1_D4,
	CLK_TOP_MUX_AUD_1,
	CLK_TOP_APLL1_CK,
	CLK_TOP_MUX_AUD_ENG1,
	CLK_TOP_APLL1_D8,
	CLK_TOP_I2S0_M_SEL,
	CLK_TOP_I2S1_M_SEL,
	CLK_TOP_I2S2_M_SEL,
	CLK_TOP_I2S3_M_SEL,
	CLK_TOP_APLL12_DIV0,
	CLK_TOP_APLL12_DIV1,
	CLK_TOP_APLL12_DIV2,
	CLK_TOP_APLL12_DIV3,
	CLOCK_APMIXED_APLL1,
	CLK_CLK26M,
	CLK_NUM
};

static const char *aud_clks[CLK_NUM] = {
	[CLK_AFE] = "aud_afe_clk",
	[CLK_TML] = "aud_tml_clk",
	[CLK_APLL22M] = "aud_apll22m_clk",
	[CLK_APLL24M] = "aud_apll24m_clk",
	[CLK_APLL1_TUNER] = "aud_apll1_tuner_clk",
	[CLK_INFRA_AXI] = "aud_infra_axi_clk",
	[CLK_INFRA_26M] = "aud_infra_26m_clk",
	[CLK_MUX_AUDIO] = "top_mux_audio",
	[CLK_MUX_AUDIOINTBUS] = "top_mux_audio_int",
	[CLK_TOP_SYSPLL1_D4] = "top_sys_pll1_d4",
	[CLK_TOP_MUX_AUD_1] = "top_mux_aud_1",
	[CLK_TOP_APLL1_CK] = "top_apll1_ck",
	[CLK_TOP_MUX_AUD_ENG1] = "top_mux_aud_eng1",
	[CLK_TOP_APLL1_D8] = "top_apll1_d8",
	[CLK_TOP_I2S0_M_SEL] = "top_i2s0_m_sel",
	[CLK_TOP_I2S1_M_SEL] = "top_i2s1_m_sel",
	[CLK_TOP_I2S2_M_SEL] = "top_i2s2_m_sel",
	[CLK_TOP_I2S3_M_SEL] = "top_i2s3_m_sel",
	[CLK_TOP_APLL12_DIV0] = "top_apll12_div0",
	[CLK_TOP_APLL12_DIV1] = "top_apll12_div1",
	[CLK_TOP_APLL12_DIV2] = "top_apll12_div2",
	[CLK_TOP_APLL12_DIV3] = "top_apll12_div3",
	[CLOCK_APMIXED_APLL1] = "apmixed_apll1",
	[CLK_CLK26M] = "top_clk26m_clk",
};

static int mt6768_set_audio_int_bus_parent(struct mtk_base_afe *afe,
					   int clk_id)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int ret;

	ret = clk_set_parent(afe_priv->clk[CLK_MUX_AUDIOINTBUS],
			     afe_priv->clk[clk_id]);
	if (ret)
		dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
			__func__, aud_clks[CLK_MUX_AUDIOINTBUS],
			aud_clks[clk_id], ret);

	return ret;
}

static int apll1_mux_setting(struct mtk_base_afe *afe, bool enable)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int ret;

	if (enable) {
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_1]);
		if (ret) {
			dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_1], ret);
			return ret;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_1],
				     afe_priv->clk[CLK_TOP_APLL1_CK]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_1],
				aud_clks[CLK_TOP_APLL1_CK], ret);
			return ret;
		}

		/* 180.6336 / 8 = 22.5792MHz */
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1]);
		if (ret) {
			dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG1], ret);
			return ret;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1],
				     afe_priv->clk[CLK_TOP_APLL1_D8]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG1],
				aud_clks[CLK_TOP_APLL1_D8], ret);
			return ret;
		}
	} else {
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret)
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG1],
				aud_clks[CLK_CLK26M], ret);
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1]);

		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_1],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret)
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_1],
				aud_clks[CLK_CLK26M], ret);
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_1]);
	}

	return 0;
}

static int apll2_mux_setting(struct mtk_base_afe *afe, bool enable)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int ret;

	if (enable) {
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_1]);
		if (ret) {
			dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_1], ret);
			return ret;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_1],
				     afe_priv->clk[CLK_TOP_APLL1_CK]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_1],
				aud_clks[CLK_TOP_APLL1_CK], ret);
			return ret;
		}

		/* 196.608 / 8 = 24.576MHz */
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1]);
		if (ret) {
			dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG1], ret);
			return ret;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1],
				     afe_priv->clk[CLK_TOP_APLL1_D8]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG1],
				aud_clks[CLK_TOP_APLL1_D8], ret);
			return ret;
		}
	} else {
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret)
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG1],
				aud_clks[CLK_CLK26M], ret);
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1]);

		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_1],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret)
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_1],
				aud_clks[CLK_CLK26M], ret);
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_1]);
	}

	return 0;
}

int mt6768_afe_enable_clock(struct mtk_base_afe *afe)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int ret;

	ret = clk_prepare_enable(afe_priv->clk[CLK_INFRA_AXI]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_INFRA_AXI], ret);
		return ret;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_INFRA_26M]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_INFRA_26M], ret);
		goto CLK_INFRA_26M_ERR;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_MUX_AUDIO]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_MUX_AUDIO], ret);
		goto CLK_MUX_AUDIO_ERR;
	}
	ret = clk_set_parent(afe_priv->clk[CLK_MUX_AUDIO],
			     afe_priv->clk[CLK_CLK26M]);
	if (ret) {
		dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
			__func__, aud_clks[CLK_MUX_AUDIO],
			aud_clks[CLK_CLK26M], ret);
		goto CLK_MUX_AUDIO_ERR;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_MUX_AUDIOINTBUS], ret);
		goto CLK_MUX_AUDIO_INTBUS_ERR;
	}
	ret = mt6768_set_audio_int_bus_parent(afe, CLK_TOP_SYSPLL1_D4);
	if (ret)
		goto CLK_MUX_AUDIO_INTBUS_PARENT_ERR;

	ret = clk_prepare_enable(afe_priv->clk[CLK_AFE]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_AFE], ret);
		goto CLK_AFE_ERR;
	}

	return 0;

CLK_AFE_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_AFE]);
CLK_MUX_AUDIO_INTBUS_PARENT_ERR:
	mt6768_set_audio_int_bus_parent(afe, CLK_CLK26M);
CLK_MUX_AUDIO_INTBUS_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
CLK_MUX_AUDIO_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIO]);
CLK_INFRA_26M_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_INFRA_26M]);
	clk_disable_unprepare(afe_priv->clk[CLK_INFRA_AXI]);
	return ret;
}

void mt6768_afe_disable_clock(struct mtk_base_afe *afe)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	clk_disable_unprepare(afe_priv->clk[CLK_AFE]);
	mt6768_set_audio_int_bus_parent(afe, CLK_CLK26M);
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIO]);
	clk_disable_unprepare(afe_priv->clk[CLK_INFRA_26M]);
	clk_disable_unprepare(afe_priv->clk[CLK_INFRA_AXI]);
}

int mt6768_apll_180m_enable(struct mtk_base_afe *afe)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int ret;

	/* 180.6336 / 8 = 22.5792MHz, 8: default value */
	if (afe_priv->topckgen)
		regmap_update_bits(afe_priv->topckgen,
				   CLK_AUDDIV_0, 0xf << 24, 7 << 24);

	clk_set_rate(afe_priv->clk[CLOCK_APMIXED_APLL1], 180633600);
	apll1_mux_setting(afe, true);

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL22M]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_APLL22M], ret);
		return ret;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL1_TUNER]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_APLL1_TUNER], ret);
		goto ERR_CLK_APLL1_TUNER;
	}

	if (afe_priv->apmixed)
		regmap_update_bits(afe_priv->apmixed, AP_PLL_CON3, 0x1, 0x1);

	regmap_update_bits(afe->regmap, AFE_APLL_TUNER_CFG,
			   0x0000FFF7, 0x00000432);
	regmap_update_bits(afe->regmap, AFE_APLL_TUNER_CFG, 0x1, 0x1);

	regmap_update_bits(afe->regmap, AFE_HD_ENGEN_ENABLE,
			   AFE_22M_ON_MASK_SFT,
			   0x1 << AFE_22M_ON_SFT);

	return 0;

ERR_CLK_APLL1_TUNER:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL1_TUNER]);
	clk_disable_unprepare(afe_priv->clk[CLK_APLL22M]);
	return ret;
}

void mt6768_apll_180m_disable(struct mtk_base_afe *afe)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	regmap_update_bits(afe->regmap, AFE_HD_ENGEN_ENABLE,
			   AFE_22M_ON_MASK_SFT,
			   0x0 << AFE_22M_ON_SFT);

	regmap_update_bits(afe->regmap, AFE_APLL_TUNER_CFG, 0x1, 0x0);

	if (afe_priv->apmixed)
		regmap_update_bits(afe_priv->apmixed, AP_PLL_CON3, 0x1, 0x0);

	clk_disable_unprepare(afe_priv->clk[CLK_APLL1_TUNER]);
	clk_disable_unprepare(afe_priv->clk[CLK_APLL22M]);

	apll1_mux_setting(afe, false);
}

int mt6768_apll_196m_enable(struct mtk_base_afe *afe)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int ret;

	/* 196.608 / 8 = 24.576MHz, 8: default value */
	if (afe_priv->topckgen)
		regmap_update_bits(afe_priv->topckgen,
				   CLK_AUDDIV_0, 0xf << 24, 7 << 24);

	clk_set_rate(afe_priv->clk[CLOCK_APMIXED_APLL1], 196608000);
	apll2_mux_setting(afe, true);

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL24M]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_APLL24M], ret);
		return ret;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL1_TUNER]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_APLL1_TUNER], ret);
		goto ERR_CLK_APLL2_TUNER;
	}

	if (afe_priv->apmixed)
		regmap_update_bits(afe_priv->apmixed, AP_PLL_CON3, 0x1, 0x1);

	regmap_update_bits(afe->regmap, AFE_APLL_TUNER_CFG,
			   0x0000FFF7, 0x00000432);
	regmap_update_bits(afe->regmap, AFE_APLL_TUNER_CFG, 0x1, 0x1);

	regmap_update_bits(afe->regmap, AFE_HD_ENGEN_ENABLE,
			   AFE_24M_ON_MASK_SFT,
			   0x1 << AFE_24M_ON_SFT);

	return 0;

ERR_CLK_APLL2_TUNER:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL1_TUNER]);
	clk_disable_unprepare(afe_priv->clk[CLK_APLL24M]);
	return ret;
}

void mt6768_apll_196m_disable(struct mtk_base_afe *afe)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	regmap_update_bits(afe->regmap, AFE_HD_ENGEN_ENABLE,
			   AFE_24M_ON_MASK_SFT,
			   0x0 << AFE_24M_ON_SFT);

	regmap_update_bits(afe->regmap, AFE_APLL_TUNER_CFG, 0x1, 0x0);

	if (afe_priv->apmixed)
		regmap_update_bits(afe_priv->apmixed, AP_PLL_CON3, 0x1, 0x0);

	clk_disable_unprepare(afe_priv->clk[CLK_APLL1_TUNER]);
	clk_disable_unprepare(afe_priv->clk[CLK_APLL24M]);

	apll2_mux_setting(afe, false);
}

int mt6768_get_apll_rate(struct mtk_base_afe *afe, int apll)
{
	return apll == MT6768_APLL_180M ? 180633600 : 196608000;
}

int mt6768_get_apll_by_rate(struct mtk_base_afe *afe, int rate)
{
	return (rate % 8000 == 0 || rate % 12000 == 0) ?
		MT6768_APLL_196M : MT6768_APLL_180M;
}

int mt6768_get_apll_by_name(struct mtk_base_afe *afe, const char *name)
{
	if (strcmp(name, APLL_180MHZ_W_NAME) == 0)
		return MT6768_APLL_180M;
	else
		return MT6768_APLL_196M;
}

/* mck */
struct mt6768_mck_div {
	int m_sel_id;
	int div_clk_id;
};

static const struct mt6768_mck_div mck_div[MT6768_MCK_NUM] = {
	[MT6768_I2S0_MCK] = {
		.m_sel_id = CLK_TOP_I2S0_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV0,
	},
	[MT6768_I2S1_MCK] = {
		.m_sel_id = CLK_TOP_I2S1_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV1,
	},
	[MT6768_I2S2_MCK] = {
		.m_sel_id = CLK_TOP_I2S2_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV2,
	},
	[MT6768_I2S3_MCK] = {
		.m_sel_id = CLK_TOP_I2S3_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV3,
	},
};

int mt6768_mck_enable(struct mtk_base_afe *afe, int mck_id, int rate)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int apll_clk_id = CLK_TOP_MUX_AUD_1;
	int m_sel_id = mck_div[mck_id].m_sel_id;
	int div_clk_id = mck_div[mck_id].div_clk_id;
	int ret;

	if (m_sel_id >= 0) {
		ret = clk_prepare_enable(afe_priv->clk[m_sel_id]);
		if (ret) {
			dev_err(afe->dev, "%s(), clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[m_sel_id], ret);
			return ret;
		}
		ret = clk_set_parent(afe_priv->clk[m_sel_id],
				     afe_priv->clk[apll_clk_id]);
		if (ret) {
			dev_err(afe->dev, "%s(), clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[m_sel_id],
				aud_clks[apll_clk_id], ret);
			return ret;
		}
	}

	ret = clk_prepare_enable(afe_priv->clk[div_clk_id]);
	if (ret) {
		dev_err(afe->dev, "%s(), clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[div_clk_id], ret);
		return ret;
	}
	ret = clk_set_rate(afe_priv->clk[div_clk_id], rate);
	if (ret) {
		dev_err(afe->dev, "%s(), clk_set_rate %s, rate %d, fail %d\n",
			__func__, aud_clks[div_clk_id], rate, ret);
		return ret;
	}

	return 0;
}

void mt6768_mck_disable(struct mtk_base_afe *afe, int mck_id)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int m_sel_id = mck_div[mck_id].m_sel_id;
	int div_clk_id = mck_div[mck_id].div_clk_id;

	clk_disable_unprepare(afe_priv->clk[div_clk_id]);
	if (m_sel_id >= 0)
		clk_disable_unprepare(afe_priv->clk[m_sel_id]);
}

int mt6768_init_clock(struct mtk_base_afe *afe)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int i;

	afe_priv->clk = devm_kcalloc(afe->dev, CLK_NUM, sizeof(*afe_priv->clk),
				     GFP_KERNEL);
	if (!afe_priv->clk)
		return -ENOMEM;

	for (i = 0; i < CLK_NUM; i++) {
		afe_priv->clk[i] = devm_clk_get(afe->dev, aud_clks[i]);
		if (IS_ERR(afe_priv->clk[i])) {
			dev_err(afe->dev, "%s devm_clk_get %s fail\n",
				__func__, aud_clks[i]);
			return PTR_ERR(afe_priv->clk[i]);
		}
	}

	afe_priv->topckgen = syscon_regmap_lookup_by_phandle(afe->dev->of_node,
							     "mediatek,topckgen");
	if (IS_ERR(afe_priv->topckgen))
		return PTR_ERR(afe_priv->topckgen);

	afe_priv->apmixed = syscon_regmap_lookup_by_phandle(afe->dev->of_node,
							    "mediatek,apmixed");
	if (IS_ERR(afe_priv->apmixed))
		return PTR_ERR(afe_priv->apmixed);

	return 0;
}
