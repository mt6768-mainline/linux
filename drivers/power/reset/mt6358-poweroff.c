// SPDX-License-Identifier: GPL-2.0
/*
 * Power off through MediaTek PMIC
 *
 * Copyright (C) 2018 MediaTek Inc.
 *
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6397/rtc.h>
#include <linux/mfd/mt6358/registers.h>
#include <linux/reboot.h>

struct mt6358_pwrc {
	struct device *dev;
	struct regmap *regmap;
	u32 base;
};

#define pwrap_write(adr, wdata) writel(((1 << 31) | (((adr) >> 1) << 16) | (wdata)), base + 0xC20);

static int mt6358_do_pwroff(struct sys_off_data *data)
{
	struct mt6358_pwrc *pwrc = data->cb_data;
	unsigned int val;
	int ret;
	void* base = ioremap(0x1000d000, 0xc30);

#define PMIC_BBPU_2SEC_EN_MASK                               0x1
#define PMIC_BBPU_2SEC_EN_SHIFT                              8
#define RTC_BBPU_2SEC_EN				(PMIC_BBPU_2SEC_EN_MASK << PMIC_BBPU_2SEC_EN_SHIFT)

#define PMIC_BBPU_AUTO_PDN_SEL_MASK                          0x1
#define PMIC_BBPU_AUTO_PDN_SEL_SHIFT                         6
#define RTC_BBPU_AUTO_PDN_SEL			(PMIC_BBPU_AUTO_PDN_SEL_MASK << PMIC_BBPU_AUTO_PDN_SEL_SHIFT)
	//regmap_read(pwrc->regmap, pwrc->base + 0x18, &val); // + AL_SEC
	//pwrap_write(MT6358_RTC_AL_SEC, (val & ~RTC_BBPU_2SEC_EN) & ~RTC_BBPU_AUTO_PDN_SEL);

	//pwrap_write(MT6358_RTC_WRTGR, 1);
	//mdelay(10);

#define RTC_BBPU_PWREN				(PMIC_PWREN_MASK << PMIC_PWREN_SHIFT)
#define PMIC_PWREN_MASK                                      0x1
#define PMIC_PWREN_SHIFT                                     0

#define PMIC_BBPU_CLR_MASK                                   0x1
#define PMIC_BBPU_CLR_SHIFT                                  1
#define RTC_BBPU_CLR				(PMIC_BBPU_CLR_MASK << PMIC_BBPU_CLR_SHIFT)
	pwrap_write(MT6358_RTC_BBPU, RTC_BBPU_KEY | RTC_BBPU_PWREN | RTC_BBPU_CLR);

#define PMIC_DOW_MSK_MASK                                    0x1
#define PMIC_DOW_MSK_SHIFT                                   4
#define RTC_AL_MASK_DOW				(PMIC_DOW_MSK_MASK << PMIC_DOW_MSK_SHIFT)
	pwrap_write(MT6358_RTC_AL_MASK, RTC_AL_MASK_DOW);
	mdelay(10);
	pwrap_write(MT6358_RTC_WRTGR, 1);
	mdelay(10);

#define RTC_K_EOSC_RSV_0				(1U << 8)
#define RTC_K_EOSC_RSV_2				(1U << 10)
	pwrap_write(MT6358_RTC_AL_YEA, RTC_K_EOSC_RSV_0 | RTC_K_EOSC_RSV_2);
	pwrap_write(MT6358_RTC_WRTGR, 1);
	mdelay(10);

	//pwrap_write(MT6358_RTC_BBPU, RTC_BBPU_KEY | RTC_BBPU_RELOAD);
	//pwrap_write(MT6358_RTC_WRTGR, 1);
	//mdelay(10);

#define PMIC_RELOAD_MASK                                     0x1
#define PMIC_RELOAD_SHIFT                                    5
#define RTC_BBPU_RELOAD				(PMIC_RELOAD_MASK << PMIC_RELOAD_SHIFT)
	pwrap_write(MT6358_RTC_BBPU, RTC_BBPU_KEY | RTC_BBPU_RELOAD);
	pwrap_write(MT6358_RTC_WRTGR, 1);
	mdelay(10);
	pwrap_write(MT6358_RG_PPCCTL0, 0);

	/* Wait some time until system down, otherwise, notice with a warn */
	mdelay(1000);

	WARN_ONCE(1, "Unable to power off system\n");

	return NOTIFY_DONE;
}

static int mt6358_pwrc_probe(struct platform_device *pdev)
{
	struct mt6397_chip *mt6397_chip = dev_get_drvdata(pdev->dev.parent);
	struct mt6358_pwrc *pwrc;
	struct resource *res;
	int ret;

	pwrc = devm_kzalloc(&pdev->dev, sizeof(*pwrc), GFP_KERNEL);
	if (!pwrc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;

	pwrc->base = res->start;
	pwrc->regmap = mt6397_chip->regmap;
	pwrc->dev = &pdev->dev;

	ret = devm_register_sys_off_handler(pwrc->dev,
					    SYS_OFF_MODE_POWER_OFF,
					    SYS_OFF_PRIO_DEFAULT,
					    mt6358_do_pwroff,
					    pwrc);
	if (ret)
		return dev_err_probe(pwrc->dev, ret, "failed to register power-off handler\n");

	return 0;
}

static const struct of_device_id mt6358_pwrc_dt_match[] = {
	{ .compatible = "mediatek,mt6358-pwrc" },
	{},
};
MODULE_DEVICE_TABLE(of, mt6358_pwrc_dt_match);

static struct platform_driver mt6358_pwrc_driver = {
	.probe          = mt6358_pwrc_probe,
	.driver         = {
		.name   = "mt6358-pwrc",
		.of_match_table = mt6358_pwrc_dt_match,
	},
};

module_platform_driver(mt6358_pwrc_driver);

MODULE_DESCRIPTION("Poweroff driver for MT6358 PMIC");
MODULE_AUTHOR("Me, myself and I");

