// SPDX-License-Identifier: GPL-2.0-only
/*
 * Backlight driver for the Kinetic KTD3137
 */

#include <linux/leds.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>

#define DEFAULT_BRIGHTNESS	2047
#define MAX_BRIGHTNESS		2047
#define REG_MAX			0x0A

#define REG_DEV_ID		0x00
#define REG_SW_RESET		0x01
#define REG_MODE		0x02
#define REG_CONTROL		0x03
#define REG_RATIO_LSB		0x04
#define REG_RATIO_MSB		0x05
#define REG_PWM			0x06
#define REG_RAMP_ON		0x07
#define REG_TRANS_RAMP		0x08
#define REG_FLASH_SETTING	0x09
#define REG_STATUS		0x0A

#define LINEAR_BITS		BIT(1) | BIT(2)
#define CONTROL_DEFAULT		BIT(3) | /* Inductor limit: 2.6A */ \
				BIT(5) | /* OVP control: 32V */ \
				BIT(6)   /* Switching frequency: 1000KHz */

#define PWM_DEFAULT		BIT(0) | BIT(1) | /* Channel 1 & 2 */ \
				BIT(3) | BIT(4) | /* 6 LSBs */ \
				BIT(7)		  /* Disable PWM */

#define BACKLIGHT_MODE_MASK	BIT(0)
#define LED_MODE_MASK		GENMASK(7, 3)
#define LSB_MASK		GENMASK(3, 0)
#define RAMP_ON_TIMING_MASK	GENMASK(7, 4)
#define RAMP_OFF_TIMING_MASK	GENMASK(3, 0)

struct ktd3137 {
	struct i2c_client *client;
	struct regmap *regmap;
	bool led_on;

	bool is_linear;
	u32 led_current;
};

static const struct regmap_config ktd3137_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = REG_MAX,
};

static int ktd3137_read(struct ktd3137 *ktd, unsigned int reg,
			 unsigned int *val)
{
	return regmap_read(ktd->regmap, reg, val);
}

static int ktd3137_write(struct ktd3137 *ktd, unsigned int reg,
			 unsigned int val)
{
	return regmap_write(ktd->regmap, reg, val);
}

static int ktd3137_update_bits(struct ktd3137 *ktd, unsigned int reg,
			       unsigned int mask, unsigned int val)
{
	return regmap_update_bits(ktd->regmap, reg, mask, val);
}

static int ktd3137_parse_dt(struct ktd3137 *ktd) {
	if (of_property_read_u32(ktd->client->dev.of_node, "kinetic,led-current", &ktd->led_current))
		dev_err_probe(&ktd->client->dev, -EINVAL, "kinetic,led-current is missing\n");

	ktd->is_linear = of_property_read_bool(ktd->client->dev.of_node, "kinetic,linear");

	return 0;
}

static int ktd3137_init(struct ktd3137 *ktd)
{
	unsigned int val = 0;
	int ret;

	if (ktd->is_linear)
		val = CONTROL_DEFAULT | LINEAR_BITS;
	else
		val = CONTROL_DEFAULT;
	ret = ktd3137_write(ktd, REG_CONTROL, val);
	if (ret) {
		dev_err(&ktd->client->dev, "failed to setup control reg\n");
		return ret;
	}

	ktd3137_write(ktd, REG_PWM, PWM_DEFAULT);
	if (ret) {
		dev_err(&ktd->client->dev, "failed to setup pwm mode\n");
		return ret;
	}

	/* 2ms */
	ret = ktd3137_update_bits(ktd, REG_RAMP_ON, RAMP_ON_TIMING_MASK, 0x40);
	if (ret) {
		dev_err(&ktd->client->dev, "failed to setup ramp_on reg\n");
		return ret;
	}

	/* 8ms */
	ret = ktd3137_update_bits(ktd, REG_RAMP_ON, RAMP_OFF_TIMING_MASK, 0x04);
	if (ret) {
		dev_err(&ktd->client->dev, "failed to setup ramp reg\n");
		return ret;
	}

	ret = ktd3137_write(ktd, REG_TRANS_RAMP, 0x00);
	if (ret) {
		dev_err(&ktd->client->dev, "failed to setup ramp reg\n");
		return ret;
	}

	ret = ktd3137_update_bits(ktd, REG_MODE, LED_MODE_MASK, ktd->led_current);
	if (ret) {
		dev_err(&ktd->client->dev, "failed to setup mode reg\n");
		return ret;
	}

	return ret;
}

static int ktd3137_check_status(struct ktd3137 *ktd) {
	u32 val;

	ktd3137_read(ktd, REG_STATUS, &val);
	if (!val)
		return 0;

	dev_warn(&ktd->client->dev, "chip was reset\n");
	val = ktd3137_init(ktd);
	if (val) {
		dev_err(&ktd->client->dev, "failed to reinit the chip\n");
		return val;
	}

	return 0;
}

static int ktd3137_backlight_update_status(struct backlight_device *backlight_dev)
{
	struct ktd3137 *ktd = bl_get_data(backlight_dev);
	unsigned int brightness = backlight_get_brightness(backlight_dev);
	int ret = 0;

	if (!ktd->led_on && brightness > 0) {
		ret = ktd3137_update_bits(ktd, REG_MODE, BACKLIGHT_MODE_MASK, 0x01);
		ktd->led_on = true;
	} else if (brightness == 0) {
		ret = ktd3137_update_bits(ktd, REG_MODE, BACKLIGHT_MODE_MASK, 0x00);
		ktd->led_on = false;
	}

	if (ret) {
		dev_err(&ktd->client->dev, "failed to change backlight mode\n");
		return ret;
	}

	ret = ktd3137_update_bits(ktd, REG_RATIO_LSB, LSB_MASK, brightness);
	if (ret) {
		dev_err(&ktd->client->dev, "failed to set lsb backlight value\n");
		return ret;
	}
	
  ret = ktd3137_write(ktd, REG_RATIO_MSB, brightness >> 3);
	if (ret) {
		dev_err(&ktd->client->dev, "failed to set msb backlight value\n");
		return ret;
	}

	return ktd3137_check_status(ktd);
}

static const struct backlight_ops ktd3137_backlight_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = ktd3137_backlight_update_status,
};

static int ktd3137_probe(struct i2c_client *client)
{
	struct backlight_device *backlight_dev;
	struct backlight_properties props;
	struct ktd3137 *ktd;
	int ret = 0, val = 0;

	ktd = devm_kzalloc(&client->dev, sizeof(*ktd), GFP_KERNEL);
	if (!ktd)
		return -ENOMEM;

	ktd->client = client;
	ktd->regmap = devm_regmap_init_i2c(client, &ktd3137_regmap_config);
	if (IS_ERR(ktd->regmap))
		return dev_err_probe(&client->dev, PTR_ERR(ktd->regmap), "failed to init regmap\n");

	ret = ktd3137_read(ktd, REG_DEV_ID, &val);
	if (ret)
	  return dev_err_probe(&client->dev, ret, "failed to get chip id\n");
	else if (val != 18 && val != 24)
		return dev_err_probe(&client->dev, -ENOTSUPP, "unsupported chip variant\n");

	ret = ktd3137_parse_dt(ktd);
	if (ret)
		return ret;

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = MAX_BRIGHTNESS;
	props.brightness = DEFAULT_BRIGHTNESS;

	if (ktd->is_linear)
		props.scale = BACKLIGHT_SCALE_LINEAR;
	else
		props.scale = BACKLIGHT_SCALE_NON_LINEAR;

	backlight_dev = devm_backlight_device_register(&client->dev, "ktd3137-backlight",
					&client->dev, ktd, &ktd3137_backlight_ops, &props);
	if (IS_ERR(backlight_dev))
		return dev_err_probe(&client->dev, PTR_ERR(backlight_dev),
				"failed to register backlight device\n");

	ret = ktd3137_init(ktd);
	if (ret)
		return ret;

	ret = ktd3137_check_status(ktd);
	if (ret)
		return ret;

	i2c_set_clientdata(client, backlight_dev);
	backlight_update_status(backlight_dev);

	return 0;
}

static void ktd3137_remove(struct i2c_client *client)
{
	struct backlight_device *backlight_dev = i2c_get_clientdata(client);
	backlight_dev->props.brightness = 0;
	backlight_update_status(backlight_dev);
}

static const struct i2c_device_id ktd3137_ids[] = {
	{ "ktd3137" },
	{}
};
MODULE_DEVICE_TABLE(i2c, ktd3137_ids);

static const struct of_device_id ktd3137_match_table[] = {
	{ .compatible = "kinetic,ktd3137" },
	{},
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

MODULE_DESCRIPTION("Kinetic ktd3137 Backlight Driver");
MODULE_AUTHOR("Me, myself, and I");
MODULE_LICENSE("GPL");
