// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025-2026 Roman Vivchar <rva333@protonmail.com>
 *
 * Backlight driver for the Kinetic KTD3137
 */

#include <linux/backlight.h>
#include <linux/bitfield.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/regmap.h>

#define KTD3137_REG_DEV_ID		0x00
#define KTD3137_REG_SW_RESET		0x01
#define KTD3137_REG_MODE		0x02
#define KTD3137_REG_CONTROL		0x03
#define KTD3137_REG_RATIO_LSB		0x04
#define KTD3137_REG_RATIO_MSB		0x05
#define KTD3137_REG_PWM			0x06
#define KTD3137_REG_RAMP_ON		0x07
#define KTD3137_REG_TRANS_RAMP		0x08
#define KTD3137_REG_FLASH_SETTING	0x09
#define KTD3137_REG_STATUS		0x0a
#define KTD3137_REG_MAX			0x0a

/* KTD3137_REG_DEV_ID */
#define KTD3137_DEV_ID_A		0x12
#define KTD3137_DEV_ID_B		0x18

/* KTD3137_REG_MODE */
#define KTD3137_MODE_BL_EN		BIT(0)
#define KTD3137_MODE_OVP_32V		BIT(5)
#define KTD3137_MODE_FREQ_1000KHZ	BIT(6)
#define KTD3137_MODE_CURRENT_MASK	GENMASK(7, 3)

/* KTD3137_REG_CONTROL */
#define KTD3137_CONTROL_LINEAR		(BIT(1) | BIT(2))
#define KTD3137_CONTROL_IND_2P6A	BIT(3)
#define KTD3137_CONTROL_OVP_32V		BIT(5)
#define KTD3137_CONTROL_FREQ_1000KHZ	BIT(6)
#define KTD3137_CONTROL_DEFAULT		(KTD3137_CONTROL_IND_2P6A | \
					 KTD3137_CONTROL_OVP_32V | \
					 KTD3137_CONTROL_FREQ_1000KHZ)

/* KTD3137_REG_RATIO_LSB */
#define KTD3137_RATIO_LSB_MASK		GENMASK(2, 0)

/* KTD3137_REG_PWM */
#define KTD3137_PWM_CH1_EN		BIT(0)
#define KTD3137_PWM_CH2_EN		BIT(1)
#define KTD3137_PWM_FILTER_6LSB		(BIT(3) | BIT(4))
#define KTD3137_PWM_DISABLE		BIT(7)
#define KTD3137_PWM_DEFAULT		(KTD3137_PWM_CH1_EN | \
					 KTD3137_PWM_CH2_EN | \
					 KTD3137_PWM_FILTER_6LSB | \
					 KTD3137_PWM_DISABLE)

/* KTD3137_REG_RAMP_ON */
#define KTD3137_RAMP_ON_MASK		GENMASK(7, 4)
#define KTD3137_RAMP_OFF_MASK		GENMASK(3, 0)
/* Ramp ON: 2ms, OFF: 8ms */
#define KTD3137_RAMP_DEFAULT		(FIELD_PREP(KTD3137_RAMP_ON_MASK, 4) | \
					 FIELD_PREP(KTD3137_RAMP_OFF_MASK, 4))

#define KTD3137_DEFAULT_BRIGHTNESS	2047
#define KTD3137_MAX_BRIGHTNESS		2047

#define KTD3137_I_MIN_UA		5000
#define KTD3137_I_MAX_UA		29800
#define KTD3137_I_STEP_UA		800
#define KTD3137_I_DEFAULT_UA		20200

struct ktd3137 {
	struct device *dev;
	struct regmap *regmap;
	struct gpio_desc *enable_gpio;
	bool led_on;
	u32 led_current;
};

static const struct regmap_config ktd3137_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = KTD3137_REG_MAX,
};

static int ktd3137_init(struct ktd3137 *ktd)
{
	unsigned int val = KTD3137_CONTROL_DEFAULT | KTD3137_CONTROL_LINEAR;
	int ret;

	ret = regmap_write(ktd->regmap, KTD3137_REG_CONTROL, val);
	if (ret)
		return ret;

	ret = regmap_write(ktd->regmap, KTD3137_REG_PWM, KTD3137_PWM_DEFAULT);
	if (ret)
		return ret;

	ret = regmap_write(ktd->regmap, KTD3137_REG_RAMP_ON, KTD3137_RAMP_DEFAULT);
	if (ret)
		return ret;

	ret = regmap_write(ktd->regmap, KTD3137_REG_TRANS_RAMP, 0x00);
	if (ret)
		return ret;

	return regmap_update_bits(ktd->regmap, KTD3137_REG_MODE,
				  KTD3137_MODE_CURRENT_MASK, ktd->led_current);
}

static int ktd3137_check_status(struct ktd3137 *ktd)
{
	unsigned int val;
	int ret;

	ret = regmap_read(ktd->regmap, KTD3137_REG_STATUS, &val);
	if (ret)
		return ret;

	if (!val)
		return 0;

	dev_warn(ktd->dev, "status fault 0x%02x detected, reinitializing\n", val);
	return ktd3137_init(ktd);
}

static int ktd3137_backlight_update_status(struct backlight_device *backlight_dev)
{
	struct ktd3137 *ktd = bl_get_data(backlight_dev);
	unsigned int brightness = backlight_get_brightness(backlight_dev);
	int ret;

	if (!ktd->led_on && brightness > 0) {
		ret = regmap_set_bits(ktd->regmap, KTD3137_REG_MODE,
				      KTD3137_MODE_BL_EN);
		if (ret)
			return ret;
		ktd->led_on = true;
	} else if (brightness == 0) {
		ret = regmap_clear_bits(ktd->regmap, KTD3137_REG_MODE,
					KTD3137_MODE_BL_EN);
		if (ret)
			return ret;
		ktd->led_on = false;
	}

	ret = regmap_update_bits(ktd->regmap, KTD3137_REG_RATIO_LSB,
				 KTD3137_RATIO_LSB_MASK, brightness);
	if (ret)
		return ret;

	ret = regmap_write(ktd->regmap, KTD3137_REG_RATIO_MSB, brightness >> 3);
	if (ret)
		return ret;

	return ktd3137_check_status(ktd);
}

static const struct backlight_ops ktd3137_backlight_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = ktd3137_backlight_update_status,
};

static int ktd3137_parse_dt(struct ktd3137 *ktd)
{
	u32 max_ua = KTD3137_I_DEFAULT_UA;
	u32 code;

	device_property_read_u32(ktd->dev, "led-max-microamp", &max_ua);

	if (max_ua < KTD3137_I_MIN_UA || max_ua > KTD3137_I_MAX_UA)
		return dev_err_probe(ktd->dev, -EINVAL,
				     "led-max-microamp (%u) out of range\n", max_ua);

	code = (max_ua - KTD3137_I_MIN_UA) / KTD3137_I_STEP_UA;
	ktd->led_current = FIELD_PREP(KTD3137_MODE_CURRENT_MASK, code);

	return 0;
}

static int ktd3137_probe(struct i2c_client *client)
{
	struct backlight_device *backlight_dev;
	struct backlight_properties props = { };
	struct ktd3137 *ktd;
	unsigned int val;
	int ret;

	ktd = devm_kzalloc(&client->dev, sizeof(*ktd), GFP_KERNEL);
	if (!ktd)
		return -ENOMEM;

	ktd->dev = &client->dev;

	ktd->enable_gpio = devm_gpiod_get_optional(ktd->dev, "enable",
						   GPIOD_OUT_HIGH);
	if (IS_ERR(ktd->enable_gpio))
		return dev_err_probe(ktd->dev, PTR_ERR(ktd->enable_gpio),
				     "failed to get enable gpio\n");

	ktd->regmap = devm_regmap_init_i2c(client, &ktd3137_regmap_config);
	if (IS_ERR(ktd->regmap))
		return dev_err_probe(ktd->dev, PTR_ERR(ktd->regmap),
				     "failed to init regmap\n");

	ret = regmap_read(ktd->regmap, KTD3137_REG_DEV_ID, &val);
	if (ret)
		return dev_err_probe(ktd->dev, ret, "failed to read chip id\n");

	if (val != KTD3137_DEV_ID_A && val != KTD3137_DEV_ID_B)
		return dev_err_probe(ktd->dev, -ENODEV,
				     "unsupported chip id: 0x%02x\n", val);

	ret = ktd3137_parse_dt(ktd);
	if (ret)
		return ret;

	ret = ktd3137_init(ktd);
	if (ret)
		return dev_err_probe(ktd->dev, ret, "failed to init chip\n");

	ret = ktd3137_check_status(ktd);
	if (ret)
		return ret;

	props.type = BACKLIGHT_RAW;
	props.max_brightness = KTD3137_MAX_BRIGHTNESS;
	props.brightness = KTD3137_DEFAULT_BRIGHTNESS;
	props.scale = BACKLIGHT_SCALE_LINEAR;

	backlight_dev = devm_backlight_device_register(ktd->dev,
						       "ktd3137-backlight",
						       ktd->dev, ktd,
						       &ktd3137_backlight_ops,
						       &props);
	if (IS_ERR(backlight_dev))
		return dev_err_probe(ktd->dev, PTR_ERR(backlight_dev),
				     "failed to register backlight device\n");

	i2c_set_clientdata(client, backlight_dev);
	backlight_update_status(backlight_dev);

	return 0;
}

static void ktd3137_remove(struct i2c_client *client)
{
	struct backlight_device *backlight_dev = i2c_get_clientdata(client);
	struct ktd3137 *ktd = bl_get_data(backlight_dev);

	backlight_dev->props.brightness = 0;
	backlight_update_status(backlight_dev);

	gpiod_set_value_cansleep(ktd->enable_gpio, 0);
}

static const struct i2c_device_id ktd3137_ids[] = {
	{ "ktd3137" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ktd3137_ids);

static const struct of_device_id ktd3137_match_table[] = {
	{ .compatible = "kinetic,ktd3137" },
	{ }
};
MODULE_DEVICE_TABLE(of, ktd3137_match_table);

static struct i2c_driver ktd3137_driver = {
	.driver = {
		.name = "ktd3137",
		.of_match_table = ktd3137_match_table,
	},
	.probe = ktd3137_probe,
	.remove = ktd3137_remove,
	.id_table = ktd3137_ids,
};
module_i2c_driver(ktd3137_driver);

MODULE_DESCRIPTION("Kinetic KTD3137 Backlight Driver");
MODULE_LICENSE("GPL");
