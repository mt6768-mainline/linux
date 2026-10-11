// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Texas Instruments LM3643(A) Synchronous Boost Dual LED Flash Driver
 *
 * Copyright 2026 Rillian Grant <rillian.grant@gmail.com>
 */

#include <linux/acpi.h>
#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/cleanup.h>
#include <linux/dmi.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/led-class-flash.h>
#include <linux/leds.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <media/v4l2-flash-led-class.h>

#define LM3643_NUM_CHANNELS 2
#define LM3643_CHAN_JOINT LM3643_NUM_CHANNELS

#define LM3643_REG_ENABLE 0x01
#define LM3643_REG_FLASH_BR_LED1 0x03
#define LM3643_REG_FLASH_BR_LED2 0x04
#define LM3643_REG_TORCH_BR_LED1 0x05
#define LM3643_REG_TORCH_BR_LED2 0x06
#define LM3643_REG_CONFIG 0x08
#define LM3643_REG_FLAGS1 0x0A
#define LM3643_REG_FLAGS2 0x0B
#define LM3643_REG_DEV_ID 0x0C

#define LM3643_ENABLE_LED_MASK GENMASK(1, 0)
#define LM3643_ENABLE_LED1 BIT(0)
#define LM3643_ENABLE_LED2 BIT(1)

#define LM3643_MODE_MASK GENMASK(3, 2)
#define LM3643_MODE_STANDBY 0x00
#define LM3643_MODE_TORCH FIELD_PREP(LM3643_MODE_MASK, 0x2)
#define LM3643_MODE_FLASH FIELD_PREP(LM3643_MODE_MASK, 0x3)

#define LM3643_TORCH_BR_MASK GENMASK(6, 0)
#define LM3643_TORCH_BR_LED2_OVERRIDE BIT(7)
#define LM3643_TORCH_BR_CODE_RESET 0x3F
#define LM3643_TORCH_BR_CODE_MAX 0x7F

/* Torch: microamps = (code x 1.4) + 0.977 */
#define LM3643_TORCH_BR_UA_OFFSET 977
#define LM3643_TORCH_BR_UA_STEP 1400
#define LM3643_TORCH_BR_UA_TO_CODE(ua) \
	(((ua) - LM3643_TORCH_BR_UA_OFFSET) / LM3643_TORCH_BR_UA_STEP)
#define LM3643_TORCH_BR_CODE_TO_UA(code) \
	(((code) * LM3643_TORCH_BR_UA_STEP) + LM3643_TORCH_BR_UA_OFFSET)
#define LM3643_TORCH_BR_UA_MIN LM3643_TORCH_BR_CODE_TO_UA(0)

#define LM3643_FLASH_BR_MASK GENMASK(6, 0)
#define LM3643_FLASH_BR_LED2_OVERRIDE BIT(7)
#define LM3643_FLASH_BR_CODE_RESET 0x3F
#define LM3643_FLASH_BR_CODE_MAX 0x7F

/* Flash: microamps = (code x 11.725) + 10.9 */
#define LM3643_FLASH_BR_UA_OFFSET 10900
#define LM3643_FLASH_BR_UA_STEP 11725
#define LM3643_FLASH_BR_UA_TO_CODE(ua) \
	(((ua) - LM3643_FLASH_BR_UA_OFFSET) / LM3643_FLASH_BR_UA_STEP)
#define LM3643_FLASH_BR_CODE_TO_UA(code) \
	(((code) * LM3643_FLASH_BR_UA_STEP) + LM3643_FLASH_BR_UA_OFFSET)
#define LM3643_FLASH_BR_UA_MIN LM3643_FLASH_BR_CODE_TO_UA(0)
/* Maximum current the chip can produce across both sources */
#define LM3643_FLASH_BR_UA_TOTAL_MAX 1500000

/* CDEV brightness values are 1-indexed for use by led_classdev */
#define LM3643_TORCH_BR_CODE_TO_CDEV(code) ((code) + 1)
#define LM3643_TORCH_BR_CDEV_TO_CODE(brightness) ((brightness) - 1)
#define LM3643_TORCH_BR_CDEV_MAX LM3643_TORCH_BR_CODE_TO_CDEV(LM3643_TORCH_BR_CODE_MAX)
#define LM3643_TORCH_BR_UA_TO_CDEV(ua) \
	  LM3643_TORCH_BR_CODE_TO_CDEV(LM3643_TORCH_BR_UA_TO_CODE(ua))
#define LM3643_TORCH_BR_CDEV_TO_UA(brightness) \
	  LM3643_TORCH_BR_CODE_TO_UA(LM3643_TORCH_BR_CDEV_TO_CODE(brightness))

#define LM3643_CONFIG_FLASH_TIMEOUT_MASK GENMASK(3, 0)
#define LM3643_CONFIG_FLASH_TIMEOUT_RESET 0xA

#define LM3643_TIMEOUT_US_MIN 10000
#define LM3643_TIMEOUT_US_STEP 10000
#define LM3643_TIMEOUT_US_MAX 400000

#define LM3643_FLAGS1_MASK GENMASK(6, 0)
#define LM3643_FLAGS1_FAULT_TIMEOUT BIT(0)
#define LM3643_FLAGS1_FAULT_UVLO BIT(1)
#define LM3643_FLAGS1_FAULT_TSD BIT(2)
#define LM3643_FLAGS1_FAULT_CURRENT_LIMIT BIT(3)
#define LM3643_FLAGS1_FAULT_VLED2_SHORT BIT(4)
#define LM3643_FLAGS1_FAULT_VLED1_SHORT BIT(5)
#define LM3643_FLAGS1_FAULT_VOUT_SHORT BIT(6)
#define LM3643_FLAGS1_LED1_FAULTS (LM3643_FLAGS1_MASK & ~LM3643_FLAGS1_FAULT_VLED2_SHORT)
#define LM3643_FLAGS1_LED2_FAULTS (LM3643_FLAGS1_MASK & ~LM3643_FLAGS1_FAULT_VLED1_SHORT)

#define LM3643_FLAGS2_MASK GENMASK(2, 0)
#define LM3643_FLAGS2_FAULT_TEMP BIT(0)
#define LM3643_FLAGS2_FAULT_OVP BIT(1)
#define LM3643_FLAGS2_FAULT_IVFM_TRIP BIT(2)

#define LM3643_FAULTS_ALL ( \
	LED_FAULT_TIMEOUT \
	| LED_FAULT_UNDER_VOLTAGE \
	| LED_FAULT_OVER_TEMPERATURE \
	| LED_FAULT_OVER_CURRENT \
	| LED_FAULT_SHORT_CIRCUIT \
	| LED_FAULT_LED_OVER_TEMPERATURE \
	| LED_FAULT_OVER_VOLTAGE \
	| LED_FAULT_INPUT_VOLTAGE \
)

#define LM3643_DEV_ID_MASK GENMASK(5, 3)
#define LM3643_DEV_ID 0x00

struct lm3643_chan {
	u8 enable_bit;
	u8 torch_br_reg;
	u8 flash_br_reg;
	u8 flags1_faults;
	u8 num_sources;
};

static const struct lm3643_chan lm3643_chans[LM3643_NUM_CHANNELS + 1] = {
	{
		.enable_bit = LM3643_ENABLE_LED1,
		.torch_br_reg = LM3643_REG_TORCH_BR_LED1,
		.flash_br_reg = LM3643_REG_FLASH_BR_LED1,
		.flags1_faults = LM3643_FLAGS1_LED1_FAULTS,
		.num_sources = 1,
	},
	{
		.enable_bit = LM3643_ENABLE_LED2,
		.torch_br_reg = LM3643_REG_TORCH_BR_LED2,
		.flash_br_reg = LM3643_REG_FLASH_BR_LED2,
		.flags1_faults = LM3643_FLAGS1_LED2_FAULTS,
		.num_sources = 1,
	},
	[LM3643_CHAN_JOINT] = {
		.enable_bit = LM3643_ENABLE_LED_MASK,
		.torch_br_reg = LM3643_REG_TORCH_BR_LED1,
		.flash_br_reg = LM3643_REG_FLASH_BR_LED1,
		.flags1_faults = LM3643_FLAGS1_MASK,
		.num_sources = LM3643_NUM_CHANNELS,
	},
};

static const struct led_flash_setting lm3643_flash_br_setting = {
	.max = LM3643_FLASH_BR_CODE_TO_UA(LM3643_FLASH_BR_CODE_MAX),
	.min = LM3643_FLASH_BR_UA_MIN,
	.step = LM3643_FLASH_BR_UA_STEP,
	.val = LM3643_FLASH_BR_CODE_TO_UA(LM3643_FLASH_BR_CODE_RESET),
};

static const u32 lm3643_timeout_us[] = {
	LM3643_TIMEOUT_US_MIN,
	20000,
	30000,
	40000,
	50000,
	60000,
	70000,
	80000,
	90000,
	100000,
	150000,
	200000,
	250000,
	300000,
	350000,
	LM3643_TIMEOUT_US_MAX,
};

static_assert(ARRAY_SIZE(lm3643_timeout_us) == LM3643_CONFIG_FLASH_TIMEOUT_MASK + 1);

static unsigned int lm3643_timeout_to_code(u32 timeout)
{
	unsigned int i;

	for (i = ARRAY_SIZE(lm3643_timeout_us) - 1; i > 0; i--)
		if (timeout >= lm3643_timeout_us[i])
			break;

	return i;
}

/* The chip's timeouts step by 10 ms up to 100 ms and then by 50 ms. */
static const struct led_flash_setting lm3643_flash_time_setting = {
	.max = LM3643_TIMEOUT_US_MAX,
	.min = LM3643_TIMEOUT_US_MIN,
	.step = LM3643_TIMEOUT_US_STEP,
	.val = lm3643_timeout_us[LM3643_CONFIG_FLASH_TIMEOUT_RESET],
};

struct lm3643_led {
	struct lm3643 *chip;
	struct regmap *regmap;
	struct led_classdev_flash flash_cdev;
	struct v4l2_flash *v4l2_flash;

	const struct lm3643_chan *chan;

	u8 flags1, flags2;
};

struct lm3643 {
	struct regmap *regmap;
	/* Synchronizes access to enable and flag registers */
	struct mutex lock;
	struct lm3643_led leds[LM3643_NUM_CHANNELS];
	unsigned int leds_active;
};

static enum led_brightness lm3643_torch_get_brightness(struct led_classdev *led_cdev)
{
	struct lm3643_led *led = container_of(lcdev_to_flcdev(led_cdev),
					      struct lm3643_led,
					      flash_cdev);
	const struct lm3643_chan *chan = led->chan;
	unsigned int brightness;
	unsigned int enable;
	int ret;

	guard(mutex)(&led->chip->lock);

	ret = regmap_read(led->regmap, LM3643_REG_ENABLE, &enable);
	if (ret) {
		dev_err(led_cdev->dev, "failed to get enable register\n");
		return LED_OFF;
	}
	enable &= LM3643_MODE_MASK | chan->enable_bit;
	if (enable != (LM3643_MODE_TORCH | chan->enable_bit))
		return LED_OFF;

	ret = regmap_read(led->regmap, chan->torch_br_reg, &brightness);
	if (ret) {
		dev_err(led_cdev->dev,
			"failed to get LED brightness register 0x%02x\n",
			chan->torch_br_reg);
		return LED_OFF;
	}
	brightness &= LM3643_TORCH_BR_MASK;

	return LM3643_TORCH_BR_CODE_TO_CDEV(brightness);
}

static int lm3643_torch_set_brightness(struct led_classdev *led_cdev,
				       enum led_brightness brightness)
{
	struct lm3643_led *led = container_of(lcdev_to_flcdev(led_cdev),
					      struct lm3643_led, flash_cdev);
	const struct lm3643_chan *chan = led->chan;
	struct lm3643 *chip = led->chip;
	unsigned int sibling_bit, keep, enable, mode;
	int ret;

	guard(mutex)(&chip->lock);

	ret = regmap_read(led->regmap, LM3643_REG_ENABLE, &enable);
	if (ret)
		return ret;
	sibling_bit = LM3643_ENABLE_LED_MASK & ~chan->enable_bit;

	if (brightness == 0) {
		unsigned int mask = chan->enable_bit;

		/* Last active output: leave torch mode, back to standby */
		if (!(enable & sibling_bit))
			mask |= LM3643_MODE_MASK;

		return regmap_update_bits(led->regmap, LM3643_REG_ENABLE, mask, 0);
	}

	mode = enable & LM3643_MODE_MASK;
	if ((enable & sibling_bit) &&
	    mode != LM3643_MODE_STANDBY &&
	    mode != LM3643_MODE_TORCH)
		return -EBUSY;

	ret = regmap_update_bits(led->regmap, chan->torch_br_reg, LM3643_TORCH_BR_MASK,
				 LM3643_TORCH_BR_CDEV_TO_CODE(brightness));
	if (ret)
		return ret;

	keep = (enable & LM3643_MODE_MASK) == LM3643_MODE_TORCH
		? enable & sibling_bit
		: 0;

	return regmap_update_bits(led->regmap,
				  LM3643_REG_ENABLE,
				  LM3643_MODE_MASK | LM3643_ENABLE_LED_MASK,
				  LM3643_MODE_TORCH | chan->enable_bit | keep);
}

static int lm3643_flash_brightness_get(struct led_classdev_flash *fled_cdev, u32 *brightness)
{
	struct lm3643_led *led = container_of(fled_cdev,
					      struct lm3643_led,
					      flash_cdev);
	const struct lm3643_chan *chan = led->chan;
	int ret;

	ret = regmap_read(led->regmap, chan->flash_br_reg, brightness);
	if (ret)
		return ret;

	*brightness &= LM3643_FLASH_BR_MASK;
	*brightness = LM3643_FLASH_BR_CODE_TO_UA(*brightness);
	*brightness *= chan->num_sources;

	return 0;
}

static int lm3643_flash_brightness_set(struct led_classdev_flash *fled_cdev, u32 brightness)
{
	struct lm3643_led *led = container_of(fled_cdev, struct lm3643_led, flash_cdev);
	const struct lm3643_chan *chan = led->chan;

	brightness /= chan->num_sources;
	brightness = LM3643_FLASH_BR_UA_TO_CODE(brightness);

	return regmap_update_bits(led->regmap,
				  chan->flash_br_reg,
				  LM3643_FLASH_BR_MASK,
				  brightness);
}

static int lm3643_flash_timeout_set(struct led_classdev_flash *fled_cdev, u32 timeout)
{
	unsigned int code = lm3643_timeout_to_code(timeout);

	fled_cdev->timeout.val = lm3643_timeout_us[code];

	return 0;
}

static int lm3643_flash_strobe_get(struct led_classdev_flash *fled_cdev, bool *state)
{
	struct lm3643_led *led = container_of(fled_cdev, struct lm3643_led, flash_cdev);
	const struct lm3643_chan *chan = led->chan;
	unsigned int mode;
	int ret;

	ret = regmap_read(led->regmap, LM3643_REG_ENABLE, &mode);
	if (ret)
		return ret;

	*state = (mode & (LM3643_MODE_MASK | chan->enable_bit)) ==
		 (LM3643_MODE_FLASH | chan->enable_bit);

	return 0;
}

static int lm3643_flash_strobe_set(struct led_classdev_flash *fled_cdev, bool state)
{
	struct lm3643_led *led = container_of(fled_cdev, struct lm3643_led, flash_cdev);
	const struct lm3643_chan *chan = led->chan;
	unsigned int enable_reg, sibling_bit, mode;
	int ret;

	guard(mutex)(&led->chip->lock);

	ret = regmap_read(led->regmap, LM3643_REG_ENABLE, &enable_reg);
	if (ret)
		return ret;
	sibling_bit = LM3643_ENABLE_LED_MASK & ~led->chan->enable_bit;
	mode = enable_reg & LM3643_MODE_MASK;

	if (!state) {
		unsigned int mask = chan->enable_bit;

		if (mode != LM3643_MODE_FLASH)
			return 0;

		/* Leave flash mode once no other output is strobing */
		if (!(enable_reg & sibling_bit))
			mask |= LM3643_MODE_MASK;

		return regmap_update_bits(led->regmap, LM3643_REG_ENABLE, mask, 0);
	}
	if ((enable_reg & sibling_bit) && mode != LM3643_MODE_STANDBY)
		return -EBUSY;

	ret = regmap_update_bits(led->regmap,
				 LM3643_REG_CONFIG,
				 LM3643_CONFIG_FLASH_TIMEOUT_MASK,
				 lm3643_timeout_to_code(fled_cdev->timeout.val));
	if (ret)
		return ret;

	return regmap_update_bits(led->regmap,
				  LM3643_REG_ENABLE,
				  LM3643_MODE_MASK | LM3643_ENABLE_LED_MASK,
				  LM3643_MODE_FLASH | chan->enable_bit);
}

static unsigned int lm3643_decode_faults(u8 flags1, u8 flags2)
{
	unsigned int faults = 0;

	if (flags1 & LM3643_FLAGS1_FAULT_TIMEOUT)
		faults |= LED_FAULT_TIMEOUT;
	if (flags1 & LM3643_FLAGS1_FAULT_UVLO)
		faults |= LED_FAULT_UNDER_VOLTAGE;
	if (flags1 & LM3643_FLAGS1_FAULT_TSD)
		faults |= LED_FAULT_OVER_TEMPERATURE;
	if (flags1 & LM3643_FLAGS1_FAULT_CURRENT_LIMIT)
		faults |= LED_FAULT_OVER_CURRENT;

	/*
	 * The caller has already masked off the sibling channel's short flag,
	 * so all three short conditions collapse to the one generic fault.
	 */
	if (flags1 & (LM3643_FLAGS1_FAULT_VLED1_SHORT |
			  LM3643_FLAGS1_FAULT_VLED2_SHORT |
			  LM3643_FLAGS1_FAULT_VOUT_SHORT))
		faults |= LED_FAULT_SHORT_CIRCUIT;

	/* TSD is the die tripping at 150C; TEMP is the external NTC at the LED. */
	if (flags2 & LM3643_FLAGS2_FAULT_TEMP)
		faults |= LED_FAULT_LED_OVER_TEMPERATURE;
	if (flags2 & LM3643_FLAGS2_FAULT_OVP)
		faults |= LED_FAULT_OVER_VOLTAGE;
	if (flags2 & LM3643_FLAGS2_FAULT_IVFM_TRIP)
		faults |= LED_FAULT_INPUT_VOLTAGE;

	return faults;
}

static int lm3643_fault_get(struct led_classdev_flash *fled_cdev, u32 *fault)
{
	struct lm3643_led *led = container_of(fled_cdev, struct lm3643_led, flash_cdev);
	struct lm3643 *chip = led->chip;
	u32 flags1, flags2;
	int ret;

	guard(mutex)(&chip->lock);

	ret = regmap_read(chip->regmap, LM3643_REG_FLAGS1, &flags1);
	if (ret)
		return ret;

	ret = regmap_read(chip->regmap, LM3643_REG_FLAGS2, &flags2);
	if (ret)
		return ret;

	flags1 &= LM3643_FLAGS1_MASK;
	flags2 &= LM3643_FLAGS2_MASK;

	for (int i = 0; i < LM3643_NUM_CHANNELS; i++) {
		struct lm3643_led *sibling = &chip->leds[i];

		if (!sibling->chan)
			continue;

		sibling->flags1 |= flags1 & sibling->chan->flags1_faults;
		sibling->flags2 |= flags2;
	}

	*fault = lm3643_decode_faults(led->flags1, led->flags2);
	led->flags1 = 0;
	led->flags2 = 0;

	return 0;
}

static const struct led_flash_ops lm3643_flash_ops = {
	.strobe_get = lm3643_flash_strobe_get,
	.strobe_set = lm3643_flash_strobe_set,
	.flash_brightness_get = lm3643_flash_brightness_get,
	.flash_brightness_set = lm3643_flash_brightness_set,
	.timeout_set = lm3643_flash_timeout_set,
	.fault_get = lm3643_fault_get,
};

static const struct regmap_config lm3643_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x0D,
	.cache_type = REGCACHE_NONE,
};

static void lm3643_v4l2_release(void *v4l2_flash)
{
	v4l2_flash_release(v4l2_flash);
}

static void lm3643_standby(void *regmap)
{
	regmap_write(regmap, LM3643_REG_ENABLE, LM3643_MODE_STANDBY);
}

static void lm3643_scale_flash_setting_microamps(struct led_flash_setting *setting, u8 num_sources)
{
	setting->max *= num_sources;
	setting->min *= num_sources;
	setting->step *= num_sources;
	setting->val *= num_sources;
}

static struct lm3643_led *lm3643_claim_channels(struct device *dev, struct lm3643 *chip,
						struct fwnode_handle *fwnode)
{
	u32 sources[LM3643_NUM_CHANNELS];
	struct lm3643_led *led;
	int num_sources;
	int ret;

	guard(mutex)(&chip->lock);

	num_sources = fwnode_property_count_u32(fwnode, "led-sources");
	if (num_sources < 0)
		return ERR_PTR(dev_err_probe(dev, num_sources,
			"failed to read led-sources property\n"));
	if (num_sources < 1 || num_sources > LM3643_NUM_CHANNELS)
		return ERR_PTR(dev_err_probe(dev, -EINVAL,
			"led-sources has %d entries, expected 1 to %d\n",
			num_sources, LM3643_NUM_CHANNELS));

	ret = fwnode_property_read_u32_array(fwnode, "led-sources", sources, num_sources);
	if (ret)
		return ERR_PTR(dev_err_probe(dev, ret,
			"failed to read led-sources property\n"));

	for (int i = 0; i < num_sources; i++) {
		if (sources[i] >= LM3643_NUM_CHANNELS)
			return ERR_PTR(dev_err_probe(dev, -EINVAL,
				"led-sources entry %u exceeds the %d current outputs\n",
				sources[i], LM3643_NUM_CHANNELS));
		if (chip->leds_active & BIT(sources[i]))
			return ERR_PTR(dev_err_probe(dev, -EINVAL,
				"current output %u claimed more than once\n", sources[i]));

		chip->leds_active |= BIT(sources[i]);
	}

	if (num_sources == LM3643_NUM_CHANNELS) {
		led = &chip->leds[0];
		led->chan = &lm3643_chans[LM3643_CHAN_JOINT];
	} else {
		led = &chip->leds[sources[0]];
		led->chan = &lm3643_chans[sources[0]];
	}

	led->chip = chip;
	led->regmap = chip->regmap;

	return led;
}

static int lm3643_apply_fw_limits(struct device *dev, struct lm3643_led *led,
				  struct fwnode_handle *fwnode)
{
	struct led_classdev_flash *flash_cdev = &led->flash_cdev;
	struct led_classdev *led_cdev = &flash_cdev->led_cdev;
	u8 num_sources = led->chan->num_sources;
	u32 max_us, max_torch_ua, max_flash_ua, max_br;

	if (!fwnode_property_read_u32(fwnode, "flash-max-microamp", &max_flash_ua)) {
		if (max_flash_ua < LM3643_FLASH_BR_UA_MIN * num_sources)
			return dev_err_probe(dev, -EINVAL,
				"flash-max-microamp %u is below the %u uA minimum\n",
				max_flash_ua, LM3643_FLASH_BR_UA_MIN * num_sources);

		/* Rounded down to a supported value. */
		flash_cdev->brightness.max = min(flash_cdev->brightness.max, max_flash_ua);
	}

	if (!fwnode_property_read_u32(fwnode, "led-max-microamp", &max_torch_ua)) {
		if (max_torch_ua < LM3643_TORCH_BR_UA_MIN * num_sources)
			return dev_err_probe(dev, -EINVAL,
				"led-max-microamp %u is below the %u uA minimum\n",
				max_torch_ua, LM3643_TORCH_BR_UA_MIN * num_sources);

		led_cdev->max_brightness = min(led_cdev->max_brightness,
					       LM3643_TORCH_BR_UA_TO_CDEV(max_torch_ua /
									  num_sources));
	}

	if (!fwnode_property_read_u32(fwnode, "flash-max-timeout-us", &max_us)) {
		if (max_us < LM3643_TIMEOUT_US_MIN)
			return dev_err_probe(dev, -EINVAL,
				"flash-max-timeout-us %u below the %u us minimum\n",
				max_us, LM3643_TIMEOUT_US_MIN);

		flash_cdev->timeout.max = lm3643_timeout_us[lm3643_timeout_to_code(max_us)];
	}

	/* LED core applies this value */
	if (!fwnode_property_read_u32(fwnode, "max-brightness", &max_br) &&
	    (!max_br || max_br > led_cdev->max_brightness))
		return dev_err_probe(dev, -EINVAL,
				     "max-brightness %u is outside 1 to %u\n",
				     max_br, led_cdev->max_brightness);

	return 0;
}

static int lm3643_register_v4l2(struct device *dev, struct lm3643_led *led,
				struct fwnode_handle *fwnode)
{
	struct led_classdev *led_cdev = &led->flash_cdev.led_cdev;
	struct v4l2_flash_config v4l2_flash_config = {};

	strscpy(v4l2_flash_config.dev_name, dev_name(led_cdev->dev),
		sizeof(v4l2_flash_config.dev_name));
	v4l2_flash_config.flash_faults = LM3643_FAULTS_ALL;
	v4l2_flash_config.has_external_strobe = false;
	v4l2_flash_config.intensity.min = LM3643_TORCH_BR_UA_MIN;
	v4l2_flash_config.intensity.step = LM3643_TORCH_BR_UA_STEP;
	v4l2_flash_config.intensity.max = LM3643_TORCH_BR_CDEV_TO_UA(led_cdev->max_brightness);
	v4l2_flash_config.intensity.val =
		min_t(u32, LM3643_TORCH_BR_CODE_TO_UA(LM3643_TORCH_BR_CODE_RESET),
		      v4l2_flash_config.intensity.max);

	lm3643_scale_flash_setting_microamps(&v4l2_flash_config.intensity, led->chan->num_sources);

	led->v4l2_flash = v4l2_flash_init(dev, fwnode, &led->flash_cdev, NULL,
					  &v4l2_flash_config);
	if (IS_ERR(led->v4l2_flash))
		return dev_err_probe(dev,
				     PTR_ERR(led->v4l2_flash),
				     "failed to register v4l2 flash\n");

	return devm_add_action_or_reset(dev, lm3643_v4l2_release, led->v4l2_flash);
}

static int lm3643_register_led(struct device *dev, struct lm3643 *chip,
			       struct fwnode_handle *fwnode)
{
	struct led_init_data init_data = { .fwnode = fwnode };
	struct led_classdev_flash *flash_cdev;
	struct led_classdev *led_cdev;
	struct lm3643_led *led;
	int ret;

	led = lm3643_claim_channels(dev, chip, fwnode);
	if (IS_ERR(led))
		return PTR_ERR(led);

	flash_cdev = &led->flash_cdev;
	led_cdev = &flash_cdev->led_cdev;

	if (led->chan->num_sources == LM3643_NUM_CHANNELS) {
		/* Bit 7 of the LED1 brightness registers override LED2 with the same values. */
		ret = regmap_update_bits(chip->regmap, LM3643_REG_TORCH_BR_LED1,
					 LM3643_TORCH_BR_LED2_OVERRIDE,
					 LM3643_TORCH_BR_LED2_OVERRIDE);
		if (ret)
			return ret;

		ret = regmap_update_bits(chip->regmap, LM3643_REG_FLASH_BR_LED1,
					 LM3643_FLASH_BR_LED2_OVERRIDE,
					 LM3643_FLASH_BR_LED2_OVERRIDE);
		if (ret)
			return ret;
	}

	flash_cdev->brightness = lm3643_flash_br_setting;
	lm3643_scale_flash_setting_microamps(&flash_cdev->brightness, led->chan->num_sources);
	flash_cdev->timeout = lm3643_flash_time_setting;
	flash_cdev->ops = &lm3643_flash_ops;

	led_cdev->flags |= LED_DEV_CAP_FLASH;
	led_cdev->max_brightness = LM3643_TORCH_BR_CDEV_MAX;
	led_cdev->brightness_set_blocking = lm3643_torch_set_brightness;
	led_cdev->brightness_get = lm3643_torch_get_brightness;

	ret = lm3643_apply_fw_limits(dev, led, fwnode);
	if (ret)
		return ret;

	flash_cdev->brightness.max = min(flash_cdev->brightness.max, LM3643_FLASH_BR_UA_TOTAL_MAX);
	flash_cdev->brightness.val = min(flash_cdev->brightness.val, flash_cdev->brightness.max);
	flash_cdev->timeout.val = min(flash_cdev->timeout.val, flash_cdev->timeout.max);

	ret = lm3643_flash_brightness_set(flash_cdev, flash_cdev->brightness.val);
	if (ret)
		return ret;

	ret = devm_led_classdev_flash_register_ext(dev, flash_cdev, &init_data);
	if (ret)
		return ret;

	return lm3643_register_v4l2(dev, led, fwnode);
}

static const u32 lm3643_ir_flood_sources[] = { 0, 1 };

static const struct software_node lm3643_ir_flood_root = {
	.name = "lm3643",
};

/*
 * The Lenovo ThinkPad X1 Carbon Gen 14 enumerates the chip through ACPI
 * (HID TXNW3643) without a _DSD, so the LED is not described by firmware.
 * On the X1 Gen 14 it drives the Windows Hello IR flood illuminator, with
 * both current sources feeding a single LED. Describe that with a software
 * node, with torch and flash limits (100 mA and 300 mA per output) well below
 * the chip's maximums.
 */
static const struct property_entry lm3643_x1c14_props[] = {
	PROPERTY_ENTRY_U32("reg", 0),
	PROPERTY_ENTRY_U32_ARRAY("led-sources", lm3643_ir_flood_sources),
	PROPERTY_ENTRY_STRING("function", LED_FUNCTION_FLASH),
	PROPERTY_ENTRY_U32("color", LED_COLOR_ID_IR),
	PROPERTY_ENTRY_U32("led-max-microamp", 200000),
	PROPERTY_ENTRY_U32("flash-max-microamp", 600000),
	PROPERTY_ENTRY_U32("flash-max-timeout-us", 100000),
	{ }
};

static const struct software_node lm3643_x1c14_led = {
	.name = "led-0",
	.parent = &lm3643_ir_flood_root,
	.properties = lm3643_x1c14_props,
};

static const struct software_node *lm3643_x1c14_nodes[] = {
	&lm3643_ir_flood_root,
	&lm3643_x1c14_led,
	NULL
};

/*
 * The Lenovo ThinkPad X9-15 Gen 1 enumerates the chip through ACPI
 * (HID TXNW3643) without a _DSD. On the X9-15 it drives the Windows Hello IR
 * flood illuminator, with both current sources feeding a single LED. Flash
 * maximums were obtained from an SPB class extension ETW trace of the
 * vendor-supplied Windows driver. The torch limit is a conservative value well
 * below the chip's maximum.
 */
static const struct property_entry lm3643_x9_15_props[] = {
	PROPERTY_ENTRY_U32("reg", 0),
	PROPERTY_ENTRY_U32_ARRAY("led-sources", lm3643_ir_flood_sources),
	PROPERTY_ENTRY_STRING("function", LED_FUNCTION_FLASH),
	PROPERTY_ENTRY_U32("color", LED_COLOR_ID_IR),
	PROPERTY_ENTRY_U32("led-max-microamp", 200000),
	PROPERTY_ENTRY_U32("flash-max-microamp", 608050),
	PROPERTY_ENTRY_U32("flash-max-timeout-us", 400000),
	{ }
};

static const struct software_node lm3643_x9_15_led = {
	.name = "led-0",
	.parent = &lm3643_ir_flood_root,
	.properties = lm3643_x9_15_props,
};

static const struct software_node *lm3643_x9_15_nodes[] = {
	&lm3643_ir_flood_root,
	&lm3643_x9_15_led,
	NULL
};

static const struct dmi_system_id lm3643_dmi_leds[] = {
	{
		/* Lenovo ThinkPad X1 Carbon Gen 14 */
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "LENOVO"),
			DMI_EXACT_MATCH(DMI_PRODUCT_VERSION, "ThinkPad X1 Carbon Gen 14"),
		},
		.driver_data = (void *)lm3643_x1c14_nodes,
	},
	{
		/* Lenovo ThinkPad X9-15 Gen 1 */
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "LENOVO"),
			DMI_EXACT_MATCH(DMI_PRODUCT_VERSION, "ThinkPad X9-15 Gen 1"),
		},
		.driver_data = (void *)lm3643_x9_15_nodes,
	},
	{ }
};

static void lm3643_remove_dmi_leds(void *data)
{
	struct device *dev = data;
	const struct dmi_system_id *id = dmi_first_match(lm3643_dmi_leds);

	set_secondary_fwnode(dev, NULL);
	if (id)
		software_node_unregister_node_group(id->driver_data);
}

static int lm3643_add_dmi_leds(struct device *dev)
{
	const struct dmi_system_id *id;
	const struct software_node **nodes;
	int ret;

	if (!has_acpi_companion(dev))
		return 0;

	id = dmi_first_match(lm3643_dmi_leds);
	if (!id)
		return 0;

	nodes = id->driver_data;
	/* The nodes are static: a second chip on the same board cannot reuse them */
	if (software_node_fwnode(nodes[0]))
		return dev_err_probe(dev, -EBUSY, "LED description already in use\n");

	ret = software_node_register_node_group(nodes);
	if (ret)
		return dev_err_probe(dev, ret, "failed to register the LED description\n");

	set_secondary_fwnode(dev, software_node_fwnode(nodes[0]));
	dev_dbg(dev, "no LED nodes in firmware, using the DMI description\n");

	return devm_add_action_or_reset(dev, lm3643_remove_dmi_leds, dev);
}

static int lm3643_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct lm3643 *chip;
	struct gpio_desc *hwen;
	unsigned int dev_id;
	unsigned int count;
	int ret;

	count = device_get_child_node_count(dev);
	if (!count) {
		ret = lm3643_add_dmi_leds(dev);
		if (ret)
			return ret;
		count = device_get_child_node_count(dev);
	}
	if (!count || count > LM3643_NUM_CHANNELS)
		return dev_err_probe(dev, -EINVAL, "%u LED nodes found, expected 1 to %d\n",
					 count, LM3643_NUM_CHANNELS);

	ret = devm_regulator_get_enable_optional(dev, "vin");
	if (ret && ret != -ENODEV)
		return dev_err_probe(dev, ret, "failed to enable vin-supply\n");

	hwen = devm_gpiod_get_optional(dev, "enable", GPIOD_OUT_HIGH);
	if (IS_ERR(hwen))
		return dev_err_probe(dev, PTR_ERR(hwen), "failed to get enable GPIO\n");

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	i2c_set_clientdata(client, chip);

	chip->regmap = devm_regmap_init_i2c(client, &lm3643_regmap_config);
	if (IS_ERR(chip->regmap))
		return dev_err_probe(dev,
				     PTR_ERR(chip->regmap),
				     "failed to allocate register map\n");

	ret = regmap_read(chip->regmap, LM3643_REG_DEV_ID, &dev_id);
	if (ret)
		return dev_err_probe(dev, ret, "failed to read device ID\n");

	dev_id &= LM3643_DEV_ID_MASK;
	if (dev_id != LM3643_DEV_ID)
		return dev_err_probe(dev, -ENODEV, "wrong chip id 0x%02x (expected 0x%02x)\n",
					 dev_id, LM3643_DEV_ID);

	ret = devm_mutex_init(dev, &chip->lock);
	if (ret)
		return dev_err_probe(dev, ret, "failed to create mutex\n");

	ret = regmap_write(chip->regmap, LM3643_REG_ENABLE, LM3643_MODE_STANDBY);
	if (ret)
		return dev_err_probe(dev, ret, "failed to put device in standby mode\n");
	ret = devm_add_action_or_reset(dev, lm3643_standby, chip->regmap);
	if (ret)
		return ret;

	ret = regmap_update_bits(chip->regmap, LM3643_REG_TORCH_BR_LED1,
				 LM3643_TORCH_BR_LED2_OVERRIDE,
				 0);
	if (ret)
		return dev_err_probe(dev,
				     ret,
				     "failed to clear LED2 torch current override register\n");

	ret = regmap_update_bits(chip->regmap, LM3643_REG_FLASH_BR_LED1,
				 LM3643_FLASH_BR_LED2_OVERRIDE,
				 0);
	if (ret)
		return dev_err_probe(dev,
				     ret,
				     "failed to clear LED2 flash current override register\n");

	device_for_each_child_node_scoped(dev, child) {
		ret = lm3643_register_led(dev, chip, child);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct acpi_device_id lm3643_acpi_leds_match[] = {
	{ "TXNW3643" },
	{ },
};
MODULE_DEVICE_TABLE(acpi, lm3643_acpi_leds_match);

static const struct of_device_id lm3643_of_leds_match[] = {
	{ .compatible = "ti,lm3643" },
	{ },
};
MODULE_DEVICE_TABLE(of, lm3643_of_leds_match);

static struct i2c_driver lm3643_i2c_driver = {
	.driver = {
		.name = "lm3643",
		.acpi_match_table = lm3643_acpi_leds_match,
		.of_match_table = lm3643_of_leds_match,
	},
	.probe = lm3643_probe,
};
module_i2c_driver(lm3643_i2c_driver);

MODULE_DESCRIPTION("Texas Instruments LM3643 LED Flash Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rillian Grant <rillian.grant@gmail.com>");
