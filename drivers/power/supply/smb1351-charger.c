// SPDX-License-Identifier: GPL-2.0-only
/*
 * Qualcomm / Summit Microelectronics SMB1351 Battery Charger Driver
 *
 * Copyright (C) 2016-2018 The Linux Foundation. All rights reserved.
 * Copyright (C) 2018-2021 XiaoMi, Inc.
 * Copyright (C) 2026 Roman Vivchar <rva333@protonmail.com>
 */

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>

/* Configuration registers */
#define SMB1351_CHG_CURRENT_CTRL_REG		0x00
#define SMB1351_FAST_CHG_CURRENT_MASK		GENMASK(7, 4)
#define SMB1351_AC_INPUT_CURRENT_LIMIT_MASK	GENMASK(3, 0)

#define SMB1351_CHG_OTH_CURRENT_CTRL_REG	0x01
#define SMB1351_PRECHG_CURRENT_MASK		GENMASK(7, 5)
#define SMB1351_ITERM_MASK			GENMASK(4, 2)
#define SMB1351_USB_2_3_MODE_SEL_BIT		BIT(1)
#define SMB1351_USB_2_3_MODE_BY_I2C		0
#define SMB1351_USB_5_1_CMD_POLARITY_BIT	BIT(0)
#define SMB1351_USB_CMD_POLARITY_500_1_100_0	0

#define SMB1351_VARIOUS_FUNC_REG		0x02
#define SMB1351_SUSPEND_MODE_CTRL_BIT		BIT(7)
#define SMB1351_SUSPEND_MODE_CTRL_BY_PIN	0
#define SMB1351_SUSPEND_MODE_CTRL_BY_I2C	BIT(7)
#define SMB1351_BATT_TO_SYS_POWER_CTRL_BIT	BIT(6)
#define SMB1351_MAX_SYS_VOLTAGE			BIT(5)
#define SMB1351_AICL_EN_BIT			BIT(4)
#define SMB1351_AICL_DET_TH_BIT			BIT(3)
#define SMB1351_APSD_EN_BIT			BIT(2)
#define SMB1351_BATT_OV_BIT			BIT(1)
#define SMB1351_VCHG_FUNC_BIT			BIT(0)

#define SMB1351_VFLOAT_REG			0x03
#define SMB1351_VFLOAT_MASK			GENMASK(5, 0)

#define SMB1351_CHG_CTRL_REG			0x04
#define SMB1351_AUTO_RECHG_BIT			BIT(7)
#define SMB1351_ITERM_EN_BIT			BIT(6)
#define SMB1351_AUTO_RECHG_TH_BIT		BIT(3)

#define SMB1351_CHG_STAT_TIMERS_CTRL_REG	0x05
#define SMB1351_STAT_OUTPUT_POLARITY_BIT	BIT(7)
#define SMB1351_STAT_OUTPUT_MODE_BIT		BIT(6)
#define SMB1351_STAT_OUTPUT_CTRL_BIT		BIT(5)

#define SMB1351_CHG_PIN_EN_CTRL_REG		0x06
#define SMB1351_LED_BLINK_FUNC_BIT		BIT(7)
#define SMB1351_EN_PIN_CTRL_MASK		GENMASK(6, 5)
#define SMB1351_USBCS_CTRL_BIT			BIT(4)
#define SMB1351_CHG_ERR_BIT			BIT(2)
#define SMB1351_APSD_DONE_BIT			BIT(1)

#define SMB1351_THERM_A_CTRL_REG		0x07
#define SMB1351_MIN_SYS_VOLTAGE_MASK		GENMASK(7, 6)
#define SMB1351_THERM_MONITOR_BIT		BIT(4)

#define SMB1351_WDOG_SAFETY_TIMER_CTRL_REG	0x08
#define SMB1351_WDOG_TIMER_EN_BIT		BIT(0)

#define SMB1351_OTG_USBIN_AICL_CTRL_REG		0x09
#define SMB1351_OTG_ID_PIN_CTRL_MASK		GENMASK(7, 6)

#define SMB1351_FAULT_INT_REG			0x0C
#define SMB1351_STATUS_INT_REG			0x0D

#define SMB1351_VARIOUS_FUNC_2_REG		0x0E
#define SMB1351_PRECHG_TO_FASTCHG_BIT		BIT(1)
#define SMB1351_STAT_PIN_CONFIG_BIT		BIT(0)

#define SMB1351_FLEXCHARGER_REG			0x10
#define SMB1351_VARIOUS_FUNC_3_REG		0x11
#define SMB1351_SDP_SUSPEND_BIT			BIT(3)

#define SMB1351_HVDCP_BATT_MISSING_CTRL_REG	0x12
#define SMB1351_BATT_MISSING_THERM_PIN_SOURCE_BIT BIT(0)

#define SMB1351_PON_OPTIONS_REG			0x13
#define SMB1351_OTG_MODE_POWER_OPTIONS_REG	0x14
#define SMB1351_ADAPTER_CONFIG_MASK		GENMASK(7, 6)

#define SMB1351_CHARGER_I2C_CTRL_REG		0x15
#define SMB1351_LAST_CNFG_REG			0x16

/* Version */
#define SMB1351_VERSION_REG			0x2E

/* Command registers */
#define SMB1351_CMD_I2C_REG			0x30
#define SMB1351_CMD_BQ_CFG_ACCESS_BIT		BIT(6)

#define SMB1351_CMD_INPUT_LIMIT_REG		0x31
#define SMB1351_CMD_OVERRIDE_BIT		BIT(7)
#define SMB1351_CMD_SUSPEND_MODE_BIT		BIT(6)
#define SMB1351_CMD_INPUT_CURRENT_MODE_BIT	BIT(3)
#define SMB1351_CMD_USB_2_3_SEL_BIT		BIT(2)
#define SMB1351_CMD_USB_1_5_AC_CTRL_MASK	GENMASK(1, 0)
#define SMB1351_CMD_USB_100_MODE		0
#define SMB1351_CMD_USB_AC_MODE			1
#define SMB1351_CMD_USB_500_MODE		2

#define SMB1351_CMD_CHG_REG			0x32
#define SMB1351_CMD_DISABLE_THERM_MONITOR_BIT	BIT(4)
#define SMB1351_CMD_TURN_OFF_STAT_PIN_BIT	BIT(3)
#define SMB1351_CMD_PRE_TO_FAST_EN_BIT		BIT(2)
#define SMB1351_CMD_CHG_EN_BIT			BIT(1)
#define SMB1351_CMD_OTG_EN_BIT			BIT(0)

/* Status registers */
#define SMB1351_STATUS_0_REG			0x36
#define SMB1351_STATUS_AICL_BIT			BIT(7)

#define SMB1351_STATUS_1_REG			0x37
#define SMB1351_STATUS_INPUT_USB_BIT		BIT(0)

#define SMB1351_STATUS_4_REG			0x3A
#define SMB1351_STATUS_OTG_BIT			BIT(7)
#define SMB1351_STATUS_AFVC_BIT			BIT(6)
#define SMB1351_STATUS_DONE_BIT			BIT(5)
#define SMB1351_STATUS_BATT_LESS_THAN_2V_BIT	BIT(4)
#define SMB1351_STATUS_HOLD_OFF_BIT		BIT(3)
#define SMB1351_STATUS_CHG_MASK			GENMASK(2, 1)
#define SMB1351_STATUS_NO_CHARGING		(0 << 1)
#define SMB1351_STATUS_FAST_CHARGING		(2 << 1)
#define SMB1351_STATUS_PRE_CHARGING		(1 << 1)
#define SMB1351_STATUS_TAPER_CHARGING		(3 << 1)
#define SMB1351_STATUS_CHG_EN_STATUS_BIT	BIT(0)

#define SMB1351_STATUS_5_REG			0x3B
#define SMB1351_STATUS_PORT_CDP			0x80
#define SMB1351_STATUS_PORT_DCP			0x40
#define SMB1351_STATUS_PORT_OTHER		0x20
#define SMB1351_STATUS_PORT_SDP			0x10

#define SMB1351_CHG_REVISION_REG		0x3F

/* IRQ registers */
#define SMB1351_IRQ_A_REG			0x40
#define SMB1351_IRQ_NUM_REGS			8

/* Limits */
#define SMB1351_MIN_FLOAT_UV			3500000
#define SMB1351_MAX_FLOAT_UV			4500000
#define SMB1351_FLOAT_STEP_UV			20000

static const unsigned int fast_chg_current_table[] = {
	1000000, 1200000, 1400000, 1600000, 1800000, 2000000, 2200000,
	2400000, 2600000, 2800000, 3000000, 3400000, 3600000, 3800000,
	4000000, 4640000,
};

static const unsigned int input_current_table[] = {
	500000, 685000, 1000000, 1100000, 1200000, 1300000, 1500000, 1600000,
	1700000, 1800000, 2000000, 2200000, 2500000, 3000000,
};

static const unsigned int pre_chg_current_table[] = {
	200000, 300000, 400000, 500000, 600000, 700000,
};

static const unsigned int term_current_table[] = {
	200000, 300000, 400000, 500000, 600000, 700000,
};


struct smb1351_charger {
	struct device *dev;
	struct regmap *regmap;
	struct power_supply *usb_psy;
	struct regulator_dev *otg_rdev;
	struct mutex lock;

	unsigned int fast_charge_current_ua;
	unsigned int float_voltage_uv;
	unsigned int precharge_current_ua;
	unsigned int charge_term_current_ua;
	unsigned int input_current_limit_ua;
	struct gpio_desc *enable_gpio;
};

static int smb1351_masked_write(struct smb1351_charger *chip, unsigned int reg,
				unsigned int mask, unsigned int val)
{
	int ret;

	/* Configuration registers need write access enabled */
	if (reg <= SMB1351_LAST_CNFG_REG) {
		ret = regmap_update_bits(chip->regmap, SMB1351_CMD_I2C_REG,
					 SMB1351_CMD_BQ_CFG_ACCESS_BIT,
					 SMB1351_CMD_BQ_CFG_ACCESS_BIT);
		if (ret)
			return ret;
	}

	return regmap_update_bits(chip->regmap, reg, mask, val);
}

static int smb1351_write(struct smb1351_charger *chip, unsigned int reg,
			 unsigned int val)
{
	int ret;

	/* Configuration registers need write access enabled */
	if (reg <= SMB1351_LAST_CNFG_REG) {
		ret = regmap_update_bits(chip->regmap, SMB1351_CMD_I2C_REG,
					 SMB1351_CMD_BQ_CFG_ACCESS_BIT,
					 SMB1351_CMD_BQ_CFG_ACCESS_BIT);
		if (ret)
			return ret;
	}

	return regmap_write(chip->regmap, reg, val);
}

static int smb1351_set_fast_charge_current(struct smb1351_charger *chip,
					   unsigned int current_ua)
{
	int i;

	for (i = ARRAY_SIZE(fast_chg_current_table) - 1; i >= 0; i--) {
		if (fast_chg_current_table[i] <= current_ua)
			break;
	}
	/* Safe value */
	if (i < 0)
		i = 0;

	chip->fast_charge_current_ua = fast_chg_current_table[i];

	return smb1351_masked_write(chip, SMB1351_CHG_CURRENT_CTRL_REG,
				   SMB1351_FAST_CHG_CURRENT_MASK,
				   FIELD_PREP(SMB1351_FAST_CHG_CURRENT_MASK, i));
}

static int smb1351_set_input_current_limit(struct smb1351_charger *chip,
					   unsigned int current_ua)
{
	int i, ret;
	u8 cmd_mode;

	for (i = ARRAY_SIZE(input_current_table) - 1; i >= 0; i--) {
		if (input_current_table[i] <= current_ua)
			break;
	}
	/* Safe value */
	if (i < 0)
		i = 0;

	chip->input_current_limit_ua = input_current_table[i];

	ret = smb1351_masked_write(chip, SMB1351_CHG_CURRENT_CTRL_REG,
				   SMB1351_AC_INPUT_CURRENT_LIMIT_MASK,
				   FIELD_PREP(SMB1351_AC_INPUT_CURRENT_LIMIT_MASK, i));
	if (ret)
		return ret;

	if (current_ua > 500000)
		cmd_mode = SMB1351_CMD_USB_AC_MODE;
	else if (current_ua > 100000)
		cmd_mode = SMB1351_CMD_USB_500_MODE;
	else
		cmd_mode = SMB1351_CMD_USB_100_MODE;

	return smb1351_masked_write(chip, SMB1351_CMD_INPUT_LIMIT_REG,
				   SMB1351_CMD_SUSPEND_MODE_BIT |
				   SMB1351_CMD_INPUT_CURRENT_MODE_BIT |
				   SMB1351_CMD_USB_1_5_AC_CTRL_MASK,
				   SMB1351_CMD_INPUT_CURRENT_MODE_BIT | cmd_mode);
}

static int smb1351_set_float_voltage(struct smb1351_charger *chip,
				     unsigned int voltage_uv)
{
	unsigned int val;
	u8 reg_val;

	val = clamp_val(voltage_uv, SMB1351_MIN_FLOAT_UV, SMB1351_MAX_FLOAT_UV);
	reg_val = (val - SMB1351_MIN_FLOAT_UV) / SMB1351_FLOAT_STEP_UV;
	chip->float_voltage_uv = SMB1351_MIN_FLOAT_UV +
				 (reg_val * SMB1351_FLOAT_STEP_UV);

	return smb1351_masked_write(chip, SMB1351_VFLOAT_REG,
				   SMB1351_VFLOAT_MASK, reg_val);
}

static int smb1351_set_precharge_current(struct smb1351_charger *chip,
					 unsigned int current_ua)
{
	int i;

	for (i = ARRAY_SIZE(pre_chg_current_table) - 1; i >= 0; i--) {
		if (pre_chg_current_table[i] <= current_ua)
			break;
	}
	/* Safe value */
	if (i < 0)
		i = 0;

	chip->precharge_current_ua = pre_chg_current_table[i];

	return smb1351_masked_write(chip, SMB1351_CHG_OTH_CURRENT_CTRL_REG,
				   SMB1351_PRECHG_CURRENT_MASK,
				   FIELD_PREP(SMB1351_PRECHG_CURRENT_MASK, i));
}

static int smb1351_set_term_current(struct smb1351_charger *chip,
				    unsigned int current_ua)
{
	int i, ret;

	for (i = ARRAY_SIZE(term_current_table) - 1; i >= 0; i--) {
		if (term_current_table[i] <= current_ua)
			break;
	}
	/* Safe value */
	if (i < 0)
		i = 0;

	chip->charge_term_current_ua = term_current_table[i];

	ret = smb1351_masked_write(chip, SMB1351_CHG_OTH_CURRENT_CTRL_REG,
				   SMB1351_ITERM_MASK,
				   FIELD_PREP(SMB1351_ITERM_MASK, i));
	if (ret)
		return ret;

	/* Enable termination (0 in ITERM_EN_BIT enables termination) */
	return smb1351_masked_write(chip, SMB1351_CHG_CTRL_REG,
				   SMB1351_ITERM_EN_BIT, 0);
}

static int smb1351_hw_init(struct smb1351_charger *chip)
{
	int ret;

	/* Disable watchdog timer */
	ret = smb1351_masked_write(chip, SMB1351_WDOG_SAFETY_TIMER_CTRL_REG,
				   SMB1351_WDOG_TIMER_EN_BIT, 0);
	if (ret)
		return ret;

	/* Set minimum system voltage to 3.15V */
	ret = smb1351_masked_write(chip, SMB1351_THERM_A_CTRL_REG,
				   SMB1351_MIN_SYS_VOLTAGE_MASK, 0);
	if (ret)
		return ret;

	/* Set battery missing source to THERM pin */
	ret = smb1351_masked_write(chip, SMB1351_HVDCP_BATT_MISSING_CTRL_REG,
				   SMB1351_BATT_MISSING_THERM_PIN_SOURCE_BIT,
				   SMB1351_BATT_MISSING_THERM_PIN_SOURCE_BIT);
	if (ret)
		return ret;

	/* Setup CHG_PIN_EN_CTRL_REG: I2C control of charger enable and USBCS */
	ret = smb1351_masked_write(chip, SMB1351_CHG_PIN_EN_CTRL_REG,
				   SMB1351_EN_PIN_CTRL_MASK | SMB1351_USBCS_CTRL_BIT |
				   SMB1351_CHG_ERR_BIT | SMB1351_APSD_DONE_BIT |
				   SMB1351_LED_BLINK_FUNC_BIT,
				   SMB1351_CHG_ERR_BIT | SMB1351_APSD_DONE_BIT |
				   SMB1351_LED_BLINK_FUNC_BIT);
	if (ret)
		return ret;

	/*
	 * Select USB 2.0/3.0 mode by I2C command and configure standard
	 * command polarity (1 = 500mA, 0 = 100mA).
	 */
	ret = smb1351_masked_write(chip, SMB1351_CHG_OTH_CURRENT_CTRL_REG,
				   SMB1351_USB_2_3_MODE_SEL_BIT |
				   SMB1351_USB_5_1_CMD_POLARITY_BIT,
				   SMB1351_USB_2_3_MODE_BY_I2C |
				   SMB1351_USB_CMD_POLARITY_500_1_100_0);
	if (ret)
		return ret;

	/* Setup USB suspend by I2C, AICL, and APSD in VARIOUS_FUNC_REG */
	ret = smb1351_masked_write(chip, SMB1351_VARIOUS_FUNC_REG,
				   SMB1351_SUSPEND_MODE_CTRL_BIT |
				   SMB1351_AICL_EN_BIT |
				   SMB1351_APSD_EN_BIT,
				   SMB1351_SUSPEND_MODE_CTRL_BY_I2C |
				   SMB1351_AICL_EN_BIT |
				   SMB1351_APSD_EN_BIT);
	if (ret)
		return ret;

	/* Configure STAT pin as static IRQ output in VARIOUS_FUNC_2_REG */
	ret = smb1351_masked_write(chip, SMB1351_VARIOUS_FUNC_2_REG,
				   SMB1351_STAT_PIN_CONFIG_BIT,
				   SMB1351_STAT_PIN_CONFIG_BIT);
	if (ret)
		return ret;

	/* Do not suspend charging when connected to an SDP port */
	ret = smb1351_masked_write(chip, SMB1351_VARIOUS_FUNC_3_REG,
				   SMB1351_SDP_SUSPEND_BIT, 0);
	if (ret)
		return ret;

	/* Configure adapter options in OTG_MODE_POWER_OPTIONS_REG */
	ret = smb1351_masked_write(chip, SMB1351_OTG_MODE_POWER_OPTIONS_REG,
				   SMB1351_ADAPTER_CONFIG_MASK, SMB1351_ADAPTER_CONFIG_MASK);
	if (ret)
		return ret;

	/* Setup fast charge current */
	ret = smb1351_set_fast_charge_current(chip, chip->fast_charge_current_ua);
	if (ret)
		return ret;

	/* Setup float voltage */
	ret = smb1351_set_float_voltage(chip, chip->float_voltage_uv);
	if (ret)
		return ret;

	/* Setup pre-charge current */
	if (chip->precharge_current_ua > 0) {
		ret = smb1351_set_precharge_current(chip, chip->precharge_current_ua);
		if (ret)
			return ret;
	}

	/* Setup termination current (iterm) */
	if (chip->charge_term_current_ua > 0) {
		ret = smb1351_set_term_current(chip, chip->charge_term_current_ua);
		if (ret)
			return ret;
	}

	/* Setup default input current limit */
	ret = smb1351_set_input_current_limit(chip, chip->input_current_limit_ua);
	if (ret)
		return ret;

	/* Enable battery charging */
	ret = smb1351_masked_write(chip, SMB1351_CMD_CHG_REG,
				   SMB1351_CMD_CHG_EN_BIT,
				   SMB1351_CMD_CHG_EN_BIT);
	if (ret)
		return ret;

	/* Enable IRQ masks: fault & status */
	smb1351_write(chip, SMB1351_FAULT_INT_REG, 0xFF);
	smb1351_write(chip, SMB1351_STATUS_INT_REG, 0xFF);

	return 0;
}

static int smb1351_usb_get_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	struct smb1351_charger *chip = power_supply_get_drvdata(psy);
	unsigned int stat1, stat4, stat5;
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = regmap_read(chip->regmap, SMB1351_STATUS_1_REG, &stat1);
		if (ret)
			return ret;
		val->intval = !!(stat1 & SMB1351_STATUS_INPUT_USB_BIT);
		return 0;

	case POWER_SUPPLY_PROP_STATUS:
		ret = regmap_read(chip->regmap, SMB1351_STATUS_1_REG, &stat1);
		if (ret)
			return ret;
		ret = regmap_read(chip->regmap, SMB1351_STATUS_4_REG, &stat4);
		if (ret)
			return ret;

		if (stat4 & SMB1351_STATUS_DONE_BIT) {
			val->intval = POWER_SUPPLY_STATUS_FULL;
		} else {
			unsigned int chg_stat = stat4 & SMB1351_STATUS_CHG_MASK;

			if (chg_stat == SMB1351_STATUS_FAST_CHARGING ||
			    chg_stat == SMB1351_STATUS_PRE_CHARGING ||
			    chg_stat == SMB1351_STATUS_TAPER_CHARGING)
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			else if (stat1 & SMB1351_STATUS_INPUT_USB_BIT)
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			else
				val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		}
		return 0;

	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		ret = regmap_read(chip->regmap, SMB1351_STATUS_4_REG, &stat4);
		if (ret)
			return ret;

		switch (stat4 & SMB1351_STATUS_CHG_MASK) {
		case SMB1351_STATUS_FAST_CHARGING:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
			break;
		case SMB1351_STATUS_PRE_CHARGING:
		case SMB1351_STATUS_TAPER_CHARGING:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		default:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			break;
		}
		return 0;

	case POWER_SUPPLY_PROP_USB_TYPE:
		ret = regmap_read(chip->regmap, SMB1351_STATUS_5_REG, &stat5);
		if (ret)
			return ret;

		if (stat5 & SMB1351_STATUS_PORT_DCP)
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
		else if (stat5 & SMB1351_STATUS_PORT_CDP)
			val->intval = POWER_SUPPLY_USB_TYPE_CDP;
		else if (stat5 & SMB1351_STATUS_PORT_SDP)
			val->intval = POWER_SUPPLY_USB_TYPE_SDP;
		else
			val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		return 0;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = chip->input_current_limit_ua;
		return 0;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = chip->fast_charge_current_ua;
		return 0;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = fast_chg_current_table[ARRAY_SIZE(fast_chg_current_table) - 1];
		return 0;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		val->intval = chip->float_voltage_uv;
		return 0;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		val->intval = SMB1351_MAX_FLOAT_UV;
		return 0;

	default:
		return -EINVAL;
	}
}

static int smb1351_usb_set_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    const union power_supply_propval *val)
{
	struct smb1351_charger *chip = power_supply_get_drvdata(psy);
	int ret;

	guard(mutex)(&chip->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		ret = smb1351_set_input_current_limit(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = smb1351_set_fast_charge_current(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = smb1351_set_float_voltage(chip, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int smb1351_usb_property_is_writeable(struct power_supply *psy,
					     enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		return 1;
	default:
		return 0;
	}
}

static enum power_supply_property smb1351_usb_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
};

static const struct power_supply_desc smb1351_usb_desc = {
	.name			= "smb1351-usb",
	.type			= POWER_SUPPLY_TYPE_USB,
	.usb_types		= BIT(POWER_SUPPLY_USB_TYPE_UNKNOWN) |
				  BIT(POWER_SUPPLY_USB_TYPE_SDP) |
				  BIT(POWER_SUPPLY_USB_TYPE_DCP) |
				  BIT(POWER_SUPPLY_USB_TYPE_CDP),
	.properties		= smb1351_usb_properties,
	.num_properties		= ARRAY_SIZE(smb1351_usb_properties),
	.get_property		= smb1351_usb_get_property,
	.set_property		= smb1351_usb_set_property,
	.property_is_writeable	= smb1351_usb_property_is_writeable,
};


/* OTG 5V Boost Regulator */
static int smb1351_otg_enable(struct regulator_dev *rdev)
{
	struct smb1351_charger *chip = rdev_get_drvdata(rdev);

	return regmap_update_bits(chip->regmap, SMB1351_CMD_CHG_REG,
				  SMB1351_CMD_OTG_EN_BIT,
				  SMB1351_CMD_OTG_EN_BIT);
}

static int smb1351_otg_disable(struct regulator_dev *rdev)
{
	struct smb1351_charger *chip = rdev_get_drvdata(rdev);

	return regmap_update_bits(chip->regmap, SMB1351_CMD_CHG_REG,
				  SMB1351_CMD_OTG_EN_BIT, 0);
}

static int smb1351_otg_is_enabled(struct regulator_dev *rdev)
{
	struct smb1351_charger *chip = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	ret = regmap_read(chip->regmap, SMB1351_CMD_CHG_REG, &val);
	if (ret)
		return ret;

	return !!(val & SMB1351_CMD_OTG_EN_BIT);
}

static const struct regulator_ops smb1351_otg_ops = {
	.enable		= smb1351_otg_enable,
	.disable	= smb1351_otg_disable,
	.is_enabled	= smb1351_otg_is_enabled,
};


static const struct regulator_desc smb1351_otg_rdesc = {
	.name		= "smb1351-usb-otg-vbus",
	.of_match	= "usb-vbus",
	.ops		= &smb1351_otg_ops,
	.owner		= THIS_MODULE,
	.type		= REGULATOR_VOLTAGE,
	.fixed_uV	= 5000000,
	.n_voltages	= 1,
};

static irqreturn_t smb1351_irq_handler(int irq, void *data)
{
	struct smb1351_charger *chip = data;
	u8 irq_stat[SMB1351_IRQ_NUM_REGS];
	unsigned int stat1;
	int ret;

	/* Reading IRQ registers clears them */
	ret = regmap_bulk_read(chip->regmap, SMB1351_IRQ_A_REG, irq_stat,
			       SMB1351_IRQ_NUM_REGS);
	if (ret)
		dev_err(chip->dev, "Failed to read IRQ status: %d\n", ret);

	regmap_read(chip->regmap, SMB1351_STATUS_1_REG, &stat1);

	if (stat1 & SMB1351_STATUS_INPUT_USB_BIT) {
		/* Re-apply configured input limit and ensure charging is enabled */
		smb1351_set_input_current_limit(chip, chip->input_current_limit_ua);
		smb1351_masked_write(chip, SMB1351_CMD_CHG_REG,
				     SMB1351_CMD_CHG_EN_BIT,
				     SMB1351_CMD_CHG_EN_BIT);
	}

	power_supply_changed(chip->usb_psy);

	return IRQ_HANDLED;
}

static const struct regmap_config smb1351_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.max_register	= 0x4F,
	.cache_type	= REGCACHE_NONE,
};

static int smb1351_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct power_supply_config psy_cfg = {};
	struct power_supply_battery_info *info;
	struct regulator_config rcfg = {};
	struct smb1351_charger *chip;
	unsigned int rev;
	int ret;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = dev;
	mutex_init(&chip->lock);

	chip->regmap = devm_regmap_init_i2c(client, &smb1351_regmap_config);
	if (IS_ERR(chip->regmap))
		return PTR_ERR(chip->regmap);

	i2c_set_clientdata(client, chip);

	ret = regmap_read(chip->regmap, SMB1351_CHG_REVISION_REG, &rev);
	if (ret)
		return dev_err_probe(dev, ret, "failed to detect SMB1351\n");

	dev_info(dev, "detected SMB1351 charger (rev 0x%02x)\n", rev);

	psy_cfg.drv_data = chip;
	psy_cfg.fwnode = dev_fwnode(dev);
	chip->usb_psy = devm_power_supply_register(dev, &smb1351_usb_desc, &psy_cfg);
	if (IS_ERR(chip->usb_psy))
		return dev_err_probe(dev, PTR_ERR(chip->usb_psy),
				     "failed to register power supply\n");

	ret = power_supply_get_battery_info(chip->usb_psy, &info);
	if (ret)
		return dev_err_probe(dev, ret,
				     "failed to get battery info from DT\n");

	if (info->constant_charge_current_max_ua <= 0 ||
	    info->constant_charge_voltage_max_uv <= 0) {
		power_supply_put_battery_info(chip->usb_psy, info);
		return dev_err_probe(dev, -EINVAL,
				     "missing charge current or voltage in battery info\n");
	}

	chip->fast_charge_current_ua = info->constant_charge_current_max_ua;
	chip->float_voltage_uv = info->constant_charge_voltage_max_uv;
	chip->precharge_current_ua = info->precharge_current_ua;
	chip->charge_term_current_ua = info->charge_term_current_ua;
	power_supply_put_battery_info(chip->usb_psy, info);

	ret = device_property_read_u32(dev, "input-current-limit-microamp",
				       &chip->input_current_limit_ua);
	if (ret)
		/* 500mA safe limit */
		chip->input_current_limit_ua = 500000;

	chip->enable_gpio = devm_gpiod_get_optional(dev, "enable", GPIOD_OUT_HIGH);
	if (IS_ERR(chip->enable_gpio))
		return dev_err_probe(dev, PTR_ERR(chip->enable_gpio),
				     "failed to get enable gpio\n");

	ret = smb1351_hw_init(chip);
	if (ret)
		return dev_err_probe(dev, ret, "failed to initialize smb1351 chip\n");

	rcfg.dev = dev;
	rcfg.regmap = chip->regmap;
	rcfg.driver_data = chip;
	chip->otg_rdev = devm_regulator_register(dev, &smb1351_otg_rdesc,
						&rcfg);
	if (IS_ERR(chip->otg_rdev))
		return dev_err_probe(dev, PTR_ERR(chip->otg_rdev),
				     "failed to register regulator\n");

	if (client->irq) {
		ret = devm_request_threaded_irq(dev, client->irq, NULL,
						smb1351_irq_handler,
						IRQF_ONESHOT,
						"smb1351-stat", chip);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct of_device_id smb1351_of_match[] = {
	{ .compatible = "qcom,smb1351-charger" },
	{ }
};
MODULE_DEVICE_TABLE(of, smb1351_of_match);

static const struct i2c_device_id smb1351_i2c_ids[] = {
	{ "smb1351-charger" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, smb1351_i2c_ids);

static struct i2c_driver smb1351_charger_driver = {
	.probe = smb1351_probe,
	.driver = {
		.name = "smb1351-charger",
		.of_match_table = smb1351_of_match,
	},
	.id_table = smb1351_i2c_ids,
};
module_i2c_driver(smb1351_charger_driver);

MODULE_AUTHOR("Roman Vivchar <rva333@protonmail.com>");
MODULE_DESCRIPTION("Qualcomm SMB1351 Battery Charger Driver");
MODULE_LICENSE("GPL");
