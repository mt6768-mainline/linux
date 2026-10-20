// SPDX-License-Identifier: GPL-2.0
//
// mt6768-afe-gpio.c  --  Mediatek 6768 afe gpio ctrl
//
// Copyright (c) 2018 MediaTek Inc.
// Author: Michael Hsiao <michael.hsiao@mediatek.com>

#include <linux/pinctrl/consumer.h>

#include "mt6768-afe-common.h"
#include "mt6768-afe-gpio.h"

static struct pinctrl *aud_pinctrl;

enum mt6768_afe_gpio {
	MT6768_AFE_GPIO_CLK_MOSI_OFF,
	MT6768_AFE_GPIO_CLK_MOSI_ON,
	MT6768_AFE_GPIO_CLK_MISO_OFF,
	MT6768_AFE_GPIO_CLK_MISO_ON,
	MT6768_AFE_GPIO_DAT_MISO_OFF,
	MT6768_AFE_GPIO_DAT_MISO_ON,
	MT6768_AFE_GPIO_DAT_MOSI_OFF,
	MT6768_AFE_GPIO_DAT_MOSI_ON,
	MT6768_AFE_GPIO_I2S0_OFF,
	MT6768_AFE_GPIO_I2S0_ON,
	MT6768_AFE_GPIO_I2S1_OFF,
	MT6768_AFE_GPIO_I2S1_ON,
	MT6768_AFE_GPIO_I2S2_OFF,
	MT6768_AFE_GPIO_I2S2_ON,
	MT6768_AFE_GPIO_I2S3_OFF,
	MT6768_AFE_GPIO_I2S3_ON,
	MT6768_AFE_GPIO_GPIO_NUM
};

struct audio_gpio_attr {
	const char *name;
	bool gpio_prepare;
	struct pinctrl_state *gpioctrl;
};

static struct audio_gpio_attr aud_gpios[MT6768_AFE_GPIO_GPIO_NUM] = {
	[MT6768_AFE_GPIO_CLK_MOSI_OFF] = {"aud_clk_mosi_off", false, NULL},
	[MT6768_AFE_GPIO_CLK_MOSI_ON] = {"aud_clk_mosi_on", false, NULL},
	[MT6768_AFE_GPIO_CLK_MISO_OFF] = {"aud_clk_miso_off", false, NULL},
	[MT6768_AFE_GPIO_CLK_MISO_ON] = {"aud_clk_miso_on", false, NULL},
	[MT6768_AFE_GPIO_DAT_MISO_OFF] = {"aud_dat_miso_off", false, NULL},
	[MT6768_AFE_GPIO_DAT_MISO_ON] = {"aud_dat_miso_on", false, NULL},
	[MT6768_AFE_GPIO_DAT_MOSI_OFF] = {"aud_dat_mosi_off", false, NULL},
	[MT6768_AFE_GPIO_DAT_MOSI_ON] = {"aud_dat_mosi_on", false, NULL},
	[MT6768_AFE_GPIO_I2S0_OFF] = {"aud_gpio_i2s0_off", false, NULL},
	[MT6768_AFE_GPIO_I2S0_ON] = {"aud_gpio_i2s0_on", false, NULL},
	[MT6768_AFE_GPIO_I2S1_OFF] = {"aud_gpio_i2s1_off", false, NULL},
	[MT6768_AFE_GPIO_I2S1_ON] = {"aud_gpio_i2s1_on", false, NULL},
	[MT6768_AFE_GPIO_I2S2_OFF] = {"aud_gpio_i2s2_off", false, NULL},
	[MT6768_AFE_GPIO_I2S2_ON] = {"aud_gpio_i2s2_on", false, NULL},
	[MT6768_AFE_GPIO_I2S3_OFF] = {"aud_gpio_i2s3_off", false, NULL},
	[MT6768_AFE_GPIO_I2S3_ON] = {"aud_gpio_i2s3_on", false, NULL},
};

static DEFINE_MUTEX(gpio_request_mutex);

int mt6768_afe_gpio_init(struct mtk_base_afe *afe)
{
	int i;

	aud_pinctrl = devm_pinctrl_get(afe->dev);
	if (IS_ERR(aud_pinctrl)) {
		dev_warn(afe->dev, "%s(), cannot get aud_pinctrl, using defaults\n",
			__func__);
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(aud_gpios); i++) {
		aud_gpios[i].gpioctrl = pinctrl_lookup_state(aud_pinctrl,
							     aud_gpios[i].name);
		if (IS_ERR(aud_gpios[i].gpioctrl)) {
			aud_gpios[i].gpioctrl = NULL;
		} else {
			aud_gpios[i].gpio_prepare = true;
		}
	}

	return 0;
}

static int mt6768_afe_gpio_select(struct mtk_base_afe *afe,
				  enum mt6768_afe_gpio type)
{
	if (!aud_pinctrl || type < 0 || type >= MT6768_AFE_GPIO_GPIO_NUM)
		return 0;

	if (!aud_gpios[type].gpio_prepare || !aud_gpios[type].gpioctrl)
		return 0;

	return pinctrl_select_state(aud_pinctrl, aud_gpios[type].gpioctrl);
}

static int mt6768_afe_gpio_adda_dl(struct mtk_base_afe *afe, bool enable)
{
	if (enable) {
		mt6768_afe_gpio_select(afe, MT6768_AFE_GPIO_CLK_MOSI_ON);
		return mt6768_afe_gpio_select(afe, MT6768_AFE_GPIO_DAT_MOSI_ON);
	} else {
		mt6768_afe_gpio_select(afe, MT6768_AFE_GPIO_DAT_MOSI_OFF);
		return mt6768_afe_gpio_select(afe, MT6768_AFE_GPIO_CLK_MOSI_OFF);
	}
}

static int mt6768_afe_gpio_adda_ul(struct mtk_base_afe *afe, bool enable)
{
	if (enable) {
		mt6768_afe_gpio_select(afe, MT6768_AFE_GPIO_CLK_MISO_ON);
		return mt6768_afe_gpio_select(afe, MT6768_AFE_GPIO_DAT_MISO_ON);
	} else {
		mt6768_afe_gpio_select(afe, MT6768_AFE_GPIO_DAT_MISO_OFF);
		return mt6768_afe_gpio_select(afe, MT6768_AFE_GPIO_CLK_MISO_OFF);
	}
}

int mt6768_afe_gpio_request(struct mtk_base_afe *afe, bool enable,
			    int dai, int uplink)
{
	int ret = 0;

	mutex_lock(&gpio_request_mutex);
	switch (dai) {
	case MT6768_DAI_ADDA:
		if (uplink)
			ret = mt6768_afe_gpio_adda_ul(afe, enable);
		else
			ret = mt6768_afe_gpio_adda_dl(afe, enable);
		break;
	case MT6768_DAI_I2S_0:
		ret = mt6768_afe_gpio_select(afe, enable ?
					      MT6768_AFE_GPIO_I2S0_ON :
					      MT6768_AFE_GPIO_I2S0_OFF);
		break;
	case MT6768_DAI_I2S_1:
		ret = mt6768_afe_gpio_select(afe, enable ?
					      MT6768_AFE_GPIO_I2S1_ON :
					      MT6768_AFE_GPIO_I2S1_OFF);
		break;
	case MT6768_DAI_I2S_2:
		ret = mt6768_afe_gpio_select(afe, enable ?
					      MT6768_AFE_GPIO_I2S2_ON :
					      MT6768_AFE_GPIO_I2S2_OFF);
		break;
	case MT6768_DAI_I2S_3:
		ret = mt6768_afe_gpio_select(afe, enable ?
					      MT6768_AFE_GPIO_I2S3_ON :
					      MT6768_AFE_GPIO_I2S3_OFF);
		break;
	default:
		break;
	}
	mutex_unlock(&gpio_request_mutex);
	return ret;
}
