// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek MT6358 PMIC Fuel Gauge Driver
 *
 * Copyright (c) 2021 MediaTek Inc.
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 * Copyright (c) 2026 Roman Vivchar <rva333@protonmail.com>
 */

#include <linux/array_size.h>
#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/cleanup.h>
#include <linux/delay.h>
#include <linux/devm-helpers.h>
#include <linux/iio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/mfd/mt6358/registers.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/nvmem-consumer.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/time.h>
#include <linux/units.h>
#include <linux/wordpart.h>
#include <linux/workqueue.h>

/* MT6358_BM_TOP_CKPDN_CON0 */
#define MT6358_RG_FGADC_CK_PDN			BIT(3)
#define MT6358_RG_FGADC_ANA_CK_PDN		BIT(4)

/* MT6358_FGADC_ANA_TEST_CON0 */
#define MT6358_FG_DWA_RST_SW			BIT(13)
#define MT6358_FG_DWA_RST_MODE			BIT(12)
#define MT6358_FG_SPARE				BIT(5)

/* MT6358_FGADC_CON0 */
#define MT6358_FG_SOFF_SLP_EN			BIT(9)
#define MT6358_FG_SON_SLP_EN			BIT(8)
#define MT6358_FG_ON				BIT(0)

/* MT6358_FGADC_CON1 */
#define MT6358_FG_LATCHDATA_ST			BIT(15)
#define MT6358_FG_SW_RESET_MASK			GENMASK(12, 8)
#define MT6358_FG_SW_CLEAR			BIT(3)
#define MT6358_FG_SW_READ_PRE			BIT(0)

/* MT6358_FGADC_RST_CON0 */
#define MT6358_FG_RSTB_STATUS			BIT(0)

/* MT6358_FGADC_CUR_CON0 */
#define MT6358_FG_CUR_RAW_MASK			GENMASK(15, 0)

/* MT6358_FGADC_IAVG_CON1 */
#define MT6358_FG_IAVG_VLD			BIT(15)
#define MT6358_FG_IAVG_27_16_MASK		GENMASK(11, 0)

/* MT6358_FGADC_CAR_CON0 */
#define MT6358_FG_CAR_04_00_MASK		GENMASK(15, 11)

/* MT6358_FGADC_CAR_CON1 */
#define MT6358_FG_CAR_SIGN			BIT(15)
#define MT6358_FG_CAR_19_05_MASK		GENMASK(14, 0)

/* MT6358_FGADC_CARTH_CON4 */
#define MT6358_FG_BAT1_LTH_15_00_MASK		GENMASK(15, 0)
/* MT6358_FGADC_CARTH_CON5 */
#define MT6358_FG_BAT1_LTH_31_16_MASK		GENMASK(15, 0)
/* MT6358_FGADC_CARTH_CON6 */
#define MT6358_FG_BAT1_HTH_15_00_MASK		GENMASK(15, 0)
/* MT6358_FGADC_CARTH_CON7 */
#define MT6358_FG_BAT1_HTH_31_16_MASK		GENMASK(15, 0)

/* MT6358_BATON_ANA_MON0 */
#define MT6358_AD_BATON_UNDET			BIT(1)

/* MT6358_BATON_CON0 */
#define MT6358_BATON_DEBOUNCE_THD_MASK		GENMASK(3, 2)
#define MT6358_BATON_DEBOUNCE_WND_MASK		GENMASK(1, 0)

/* Unplug events should be caught without delay */
#define MT6358_BATON_DEBOUNCE_THD_FAST		0
/* 16ms debounce */
#define MT6358_BATON_DEBOUNCE_WND_16MS		2

/* MT6358_AUXADC_ADC_OUT_PWRON_PCHR */
#define MT6358_AUXADC_ADC_RDY_PWRON_PCHR	BIT(15)
#define MT6358_AUXADC_ADC_OUT_PWRON_PCHR_MASK	GENMASK(14, 0)

/* MT6358_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR */
#define MT6358_AUXADC_ADC_RDY_BAT_PLUGIN_PCHR	BIT(15)
#define MT6358_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR_MASK GENMASK(14, 0)


/* MT6358 AUXADC 15-bit raw battery voltage mask */
#define MT6358_AUXADC_BATADC_RAW_MASK		GENMASK(14, 0)

/* 1 LSB = 381.47 uA. For 47.3mA (sleep-off mode) we use 124 LSBs */
#define MT6358_FG_SOFF_SLP_CUR_TH_VAL		124

/* Sampling interval in seconds when sleeping */
#define MT6358_AUXADC_NAG_PRD_VAL		10

/* 1 LSB */
#define MT6358_UNIT_FGCURRENT_NUA		381470LL
/* Average current */
#define MT6358_UNIT_FG_IAVG_NUA			190735LL
/* Coulomb counter, 2^11 LSB */
#define MT6358_UNIT_FGCAR_NUAH			108507LL
/* 8,874 counts per 1 mAh (3600 * 10^6 uAs / 190735 uAs) */
#define MT6358_CAR_TO_REG_FACTOR		18874LL

/* Nominal shunt for silicon unit conversions */
#define MT6358_R_FG_NOMINAL_UOHM		10000LL
/* Default Coulomb counter tuning factor (1000 = 1.0) */
#define MT6358_CAR_TUNE_NOMINAL			1000LL
/* For negative charge wrap-around */
#define MT6358_CAR_MODULUS			BIT_ULL(20)

/*
 * Input voltage is 5.4 V (5,400,000 uV) across 32,768 (2^15) steps.
 * Reducing 5400000 / 32768 yields the 675000 / 4096.
 */
#define MT6358_BAT_VOLT_SCALE_NUM		675000ULL
#define MT6358_BAT_VOLT_SCALE_DEN		4096ULL

/* Minimal and maximum valid voltage */
#define MT6358_VOLTAGE_MIN_VALID_MV		2000
#define MT6358_VOLTAGE_MAX_VALID_MV		5000

/* Fallback OCV if read fails during the boot */
#define MT6358_DEFAULT_OCV_UV			3800000

/* OCV temperature */
#define MT6358_OCV_TEMP_NOMINAL_C		25

/* Threshold to determine charging state */
#define MT6358_FG_CURRENT_STATE_THRESHOLD_UA	50000

/* Monitoring interval */
#define MT6358_FG_MONITOR_DELAY_MS		(10 * MSEC_PER_SEC)

#define MT6358_FG_SOC_VALID			BIT(7)
#define MT6358_FG_SOC_MASK			GENMASK(6, 0)

struct mt6358_gauge {
	struct device *dev;
	struct regmap *regmap;
	struct power_supply *psy;
	struct power_supply_desc psy_desc;
	struct power_supply_battery_info *bat_info;

	struct mutex lock; /* protects hardware FG latching and registers */
	struct delayed_work monitor_work;

	struct iio_channel *chan_bat_voltage;

	struct nvmem_cell *cell_init;
	struct nvmem_cell *cell_soc;

	int irq_coulomb_h;
	int irq_coulomb_l;
	int irq_bat_out;

	u32 r_fg_uohm;
	u32 car_tune_value;

	bool is_warm_boot;
	bool present;
	int boot_ocv_uv;
	int soc;
	s64 initial_charge_uah;
	s64 total_cap_uah;
};

static void mt6358_fg_unlatch(struct mt6358_gauge *gauge)
{
	unsigned int val;

	regmap_update_bits(gauge->regmap, MT6358_FGADC_CON1,
			   MT6358_FG_SW_CLEAR | MT6358_FG_SW_READ_PRE, MT6358_FG_SW_CLEAR);

	regmap_read_poll_timeout(gauge->regmap, MT6358_FGADC_CON1, val,
				 !(val & MT6358_FG_LATCHDATA_ST),
				 50, 10 * USEC_PER_MSEC);

	regmap_update_bits(gauge->regmap, MT6358_FGADC_CON1, MT6358_FG_SW_CLEAR, 0);
}

static int mt6358_fg_latch(struct mt6358_gauge *gauge)
{
	unsigned int val;
	int ret;

	ret = regmap_write(gauge->regmap, MT6358_FGADC_CON1, MT6358_FG_SW_READ_PRE);
	if (ret)
		return ret;

	ret = regmap_read_poll_timeout(gauge->regmap, MT6358_FGADC_CON1, val,
				       (val & MT6358_FG_LATCHDATA_ST),
				       50, 20 * USEC_PER_MSEC);
	if (ret) {
		dev_err_ratelimited(gauge->dev, "FG latch timeout: 0x%x\n", val);
		mt6358_fg_unlatch(gauge);
		regmap_set_bits(gauge->regmap, MT6358_FGADC_CON0, MT6358_FG_ON);
		return ret;
	}

	return 0;
}

static int mt6358_fg_read_current(struct mt6358_gauge *gauge, int *curr_ua)
{
	unsigned int reg_val;
	s16 raw;
	s64 cur;
	int ret;

	guard(mutex)(&gauge->lock);

	ret = mt6358_fg_latch(gauge);
	if (ret)
		return ret;

	ret = regmap_read(gauge->regmap, MT6358_FGADC_CUR_CON0, &reg_val);
	mt6358_fg_unlatch(gauge);
	if (ret)
		return ret;

	raw = (s16)FIELD_GET(MT6358_FG_CUR_RAW_MASK, reg_val);
	cur = (s64)raw * MT6358_UNIT_FGCURRENT_NUA;
	cur = div_s64(cur * MT6358_R_FG_NOMINAL_UOHM, gauge->r_fg_uohm);
	cur = div_s64(cur * (s64)gauge->car_tune_value, MT6358_CAR_TUNE_NOMINAL);

	*curr_ua = (int)div_s64(cur, MILLI);
	return 0;
}

static int mt6358_fg_read_current_avg(struct mt6358_gauge *gauge, int *curr_ua)
{
	unsigned int con0, con1;
	u32 raw28;
	s32 sraw;
	s64 cur;
	int ret;

	scoped_guard(mutex, &gauge->lock) {
		ret = mt6358_fg_latch(gauge);
		if (ret)
			return ret;

		ret = regmap_read(gauge->regmap, MT6358_FGADC_IAVG_CON1, &con1);
		if (ret == 0)
			ret = regmap_read(gauge->regmap, MT6358_FGADC_IAVG_CON0, &con0);

		mt6358_fg_unlatch(gauge);
		if (ret)
			return ret;
	}

	if (!(con1 & MT6358_FG_IAVG_VLD))
		return mt6358_fg_read_current(gauge, curr_ua);

	/* Convert s28 to s32 */
	raw28 = ((u32)FIELD_GET(MT6358_FG_IAVG_27_16_MASK, con1) << 16) |
		lower_16_bits(con0);
	sraw = sign_extend32(raw28, 27);

	cur = (s64)sraw * MT6358_UNIT_FG_IAVG_NUA;
	cur = div_s64(cur * MT6358_R_FG_NOMINAL_UOHM, gauge->r_fg_uohm);
	cur = div_s64(cur * (s64)gauge->car_tune_value, MT6358_CAR_TUNE_NOMINAL);

	*curr_ua = (int)div_s64(cur, MILLI);
	return 0;
}

static int mt6358_fg_read_coulomb_raw(struct mt6358_gauge *gauge, u32 *raw_val, bool *sign_neg)
{
	unsigned int car0, car1;
	int ret;

	scoped_guard(mutex, &gauge->lock) {
		ret = mt6358_fg_latch(gauge);
		if (ret)
			return ret;

		ret = regmap_read(gauge->regmap, MT6358_FGADC_CAR_CON0, &car0);
		if (ret == 0)
			ret = regmap_read(gauge->regmap, MT6358_FGADC_CAR_CON1, &car1);

		mt6358_fg_unlatch(gauge);
		if (ret)
			return ret;
	}

	*raw_val = FIELD_GET(MT6358_FG_CAR_04_00_MASK, car0) |
		   (FIELD_GET(MT6358_FG_CAR_19_05_MASK, car1) << 5);
	*sign_neg = (car1 & MT6358_FG_CAR_SIGN) != 0;

	return 0;
}

static int mt6358_fg_read_charge_counter(struct mt6358_gauge *gauge, s64 *charge_uah)
{
	u32 ucar;
	bool neg;
	s64 car;
	int ret;

	ret = mt6358_fg_read_coulomb_raw(gauge, &ucar, &neg);
	if (ret)
		return ret;

	if (ucar == 0 || ucar == 0xfffff) {
		*charge_uah = 0;
		return 0;
	}

	if (neg)
		car = -(s64)(MT6358_CAR_MODULUS - ucar);
	else
		car = (s64)ucar;

	car = car * MT6358_UNIT_FGCAR_NUAH;
	car = div_s64(car * MT6358_R_FG_NOMINAL_UOHM, gauge->r_fg_uohm);
	car = div_s64(car * (s64)gauge->car_tune_value, MT6358_CAR_TUNE_NOMINAL);

	*charge_uah = div_s64(car, MILLI);
	return 0;
}

static bool mt6358_fg_is_present(struct mt6358_gauge *gauge)
{
	unsigned int val;
	int ret;

	ret = regmap_read(gauge->regmap, MT6358_BATON_ANA_MON0, &val);
	if (ret)
		return false;

	return !(val & MT6358_AD_BATON_UNDET);
}

static int mt6358_fg_read_voltage(struct mt6358_gauge *gauge, int *val_uv)
{
	int val_mv, ret;
	// u32 raw;

	ret = iio_read_channel_processed(gauge->chan_bat_voltage, &val_mv);
	if (ret)
		return ret;

	if (val_mv < MT6358_VOLTAGE_MIN_VALID_MV || val_mv > MT6358_VOLTAGE_MAX_VALID_MV)
		return -EINVAL;

	*val_uv = val_mv * (int)MILLI;
	return 0;

	// ret = iio_read_channel_raw(gauge->chan_bat_voltage, &val_mv);
	// if (ret < 0)
	// 	return ret;

	// raw = (u32)val_mv & MT6358_AUXADC_BATADC_RAW_MASK;
	// *val_uv = (int)div_u64((u64)raw * MT6358_BAT_VOLT_SCALE_NUM,
	// 		       MT6358_BAT_VOLT_SCALE_DEN);
}

static int mt6358_fg_read_boot_ocv(struct mt6358_gauge *gauge, int *val_uv)
{
	unsigned int val;
	int ret;

	ret = regmap_read(gauge->regmap, MT6358_AUXADC_ADC_OUT_PWRON_PCHR, &val);
	if (ret)
		return ret;

	if (val & MT6358_AUXADC_ADC_RDY_PWRON_PCHR) {
		u32 adc_out = FIELD_GET(MT6358_AUXADC_ADC_OUT_PWRON_PCHR_MASK, val);

		*val_uv = (int)div_u64((u64)adc_out * MT6358_BAT_VOLT_SCALE_NUM,
				       MT6358_BAT_VOLT_SCALE_DEN);
		return 0;
	}

	ret = regmap_read(gauge->regmap, MT6358_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR, &val);
	if (ret)
		return ret;

	if (val & MT6358_AUXADC_ADC_RDY_BAT_PLUGIN_PCHR) {
		u32 adc_out = FIELD_GET(MT6358_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR_MASK, val);

		*val_uv = (int)div_u64((u64)adc_out * MT6358_BAT_VOLT_SCALE_NUM,
				       MT6358_BAT_VOLT_SCALE_DEN);
		return 0;
	}

	return mt6358_fg_read_voltage(gauge, val_uv);
}

static void mt6358_fg_update_soc(struct mt6358_gauge *gauge)
{
	s64 delta_uah, current_charge_uah;
	int soc;
	u8 val;

	if (mt6358_fg_read_charge_counter(gauge, &delta_uah)) {
		dev_warn_ratelimited(gauge->dev, "failed to read charge counter\n");
		return;
	}

	current_charge_uah = gauge->initial_charge_uah + delta_uah;
	if (current_charge_uah > gauge->total_cap_uah)
		current_charge_uah = gauge->total_cap_uah;
	if (current_charge_uah < 0)
		current_charge_uah = 0;

	soc = (int)div_s64(current_charge_uah * PERCENT, gauge->total_cap_uah);
	gauge->soc = clamp(soc, 0, PERCENT);

	if (IS_ERR_OR_NULL(gauge->cell_soc))
		/* optional */
		return;

	val = FIELD_PREP(MT6358_FG_SOC_MASK, gauge->soc) | MT6358_FG_SOC_VALID;
	nvmem_cell_write(gauge->cell_soc, &val, sizeof(val));
}

static int mt6358_fg_set_coulomb_thresholds(struct mt6358_gauge *gauge)
{
	unsigned int car0, car1;
	u32 val32_car;
	s64 step_uah, counts;
	u32 high_bound, low_bound;
	int ret;

	/* Step size corresponding to 1% change in battery capacity */
	step_uah = div_s64(gauge->total_cap_uah, PERCENT);
	counts = div_s64(step_uah * MT6358_CAR_TO_REG_FACTOR, (s64)MICRO);
	counts = div_s64(counts * gauge->r_fg_uohm, MT6358_R_FG_NOMINAL_UOHM);
	counts = div_s64(counts * MT6358_CAR_TUNE_NOMINAL, gauge->car_tune_value);

	/* Prevent interrupt storm if the count is too high */
	if (counts <= 0)
		counts = 100;

	guard(mutex)(&gauge->lock);

	ret = mt6358_fg_latch(gauge);
	if (ret)
		return ret;

	ret = regmap_read(gauge->regmap, MT6358_FGADC_CAR_CON0, &car0);
	if (ret == 0)
		ret = regmap_read(gauge->regmap, MT6358_FGADC_CAR_CON1, &car1);

	mt6358_fg_unlatch(gauge);
	if (ret)
		return ret;

	val32_car = lower_16_bits(car0) | ((u32)lower_16_bits(car1) << 16);
	high_bound = val32_car + (u32)counts;
	low_bound = val32_car - (u32)counts;

	regmap_update_bits(gauge->regmap, MT6358_FGADC_CARTH_CON6,
			   MT6358_FG_BAT1_HTH_15_00_MASK,
			   FIELD_PREP(MT6358_FG_BAT1_HTH_15_00_MASK, lower_16_bits(high_bound)));
	regmap_update_bits(gauge->regmap, MT6358_FGADC_CARTH_CON7,
			   MT6358_FG_BAT1_HTH_31_16_MASK,
			   FIELD_PREP(MT6358_FG_BAT1_HTH_31_16_MASK, upper_16_bits(high_bound)));

	regmap_update_bits(gauge->regmap, MT6358_FGADC_CARTH_CON4,
			   MT6358_FG_BAT1_LTH_15_00_MASK,
			   FIELD_PREP(MT6358_FG_BAT1_LTH_15_00_MASK, lower_16_bits(low_bound)));
	regmap_update_bits(gauge->regmap, MT6358_FGADC_CARTH_CON5,
			   MT6358_FG_BAT1_LTH_31_16_MASK,
			   FIELD_PREP(MT6358_FG_BAT1_LTH_31_16_MASK, upper_16_bits(low_bound)));

	return 0;
}

static irqreturn_t mt6358_fg_coulomb_irq(int irq, void *data)
{
	struct mt6358_gauge *gauge = data;

	mt6358_fg_update_soc(gauge);
	mt6358_fg_set_coulomb_thresholds(gauge);
	power_supply_changed(gauge->psy);

	return IRQ_HANDLED;
}

static irqreturn_t mt6358_fg_bat_out_irq(int irq, void *data)
{
	struct mt6358_gauge *gauge = data;

	gauge->present = mt6358_fg_is_present(gauge);
	power_supply_changed(gauge->psy);

	return IRQ_HANDLED;
}

static void mt6358_fg_monitor_work(struct work_struct *work)
{
	struct mt6358_gauge *gauge =
		container_of(work, struct mt6358_gauge, monitor_work.work);

	gauge->present = mt6358_fg_is_present(gauge);
	mt6358_fg_update_soc(gauge);
	power_supply_changed(gauge->psy);

	schedule_delayed_work(&gauge->monitor_work,
			      msecs_to_jiffies(MT6358_FG_MONITOR_DELAY_MS));
}

static int mt6358_gauge_get_property(struct power_supply *psy,
				     enum power_supply_property psp,
				     union power_supply_propval *val)
{
	struct mt6358_gauge *gauge = power_supply_get_drvdata(psy);
	int ret, cur_ua;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (!gauge->present) {
			val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
			return 0;
		}

		ret = mt6358_fg_read_current(gauge, &cur_ua);
		if (ret)
			return ret;

		if (gauge->soc >= PERCENT)
			val->intval = POWER_SUPPLY_STATUS_FULL;
		else if (cur_ua > MT6358_FG_CURRENT_STATE_THRESHOLD_UA)
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else if (cur_ua < -MT6358_FG_CURRENT_STATE_THRESHOLD_UA)
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		else
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		return 0;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = gauge->present ? 1 : 0;
		return 0;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		return mt6358_fg_read_voltage(gauge, &val->intval);

	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		val->intval = gauge->boot_ocv_uv;
		return 0;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		return mt6358_fg_read_current(gauge, &val->intval);

	case POWER_SUPPLY_PROP_CURRENT_AVG:
		return mt6358_fg_read_current_avg(gauge, &val->intval);

	case POWER_SUPPLY_PROP_CHARGE_COUNTER: {
		s64 charge_uah;

		ret = mt6358_fg_read_charge_counter(gauge, &charge_uah);
		if (ret)
			return ret;

		val->intval = (int)charge_uah;
		return 0;
	}

	case POWER_SUPPLY_PROP_CHARGE_NOW: {
		s64 delta_uah, charge_now;

		ret = mt6358_fg_read_charge_counter(gauge, &delta_uah);
		if (ret)
			return ret;

		charge_now = gauge->initial_charge_uah + delta_uah;
		if (charge_now < 0)
			charge_now = 0;
		if (charge_now > gauge->total_cap_uah)
			charge_now = gauge->total_cap_uah;
		val->intval = (int)charge_now;

		return 0;
	}

	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = (int)gauge->total_cap_uah;
		return 0;

	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		if (gauge->bat_info->voltage_min_design_uv < 0)
			return -ENODATA;

		val->intval = gauge->bat_info->voltage_min_design_uv;
		return 0;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		if (gauge->bat_info->voltage_max_design_uv < 0)
			return -ENODATA;

		val->intval = gauge->bat_info->voltage_max_design_uv;
		return 0;

	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = gauge->soc;
		return 0;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		return 0;

	default:
		return -EINVAL;
	}
}

static enum power_supply_property mt6358_gauge_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

static int mt6358_fg_hw_init(struct mt6358_gauge *gauge)
{
	unsigned int val;
	int ret;

	/* Ungate FGADC clocks */
	ret = regmap_write(gauge->regmap, MT6358_BM_TOP_CKPDN_CON0_CLR,
			   MT6358_RG_FGADC_CK_PDN | MT6358_RG_FGADC_ANA_CK_PDN);
	if (ret)
		return ret;

	/* Enable FGADC converter and sleep mode */
	ret = regmap_set_bits(gauge->regmap, MT6358_FGADC_CON0,
			      MT6358_FG_ON | MT6358_FG_SON_SLP_EN | MT6358_FG_SOFF_SLP_EN);
	if (ret)
		return ret;

	/* Reset any stale FSM */
	mt6358_fg_unlatch(gauge);

	ret = regmap_update_bits(gauge->regmap, MT6358_BATON_CON0,
				 MT6358_BATON_DEBOUNCE_THD_MASK |
				 MT6358_BATON_DEBOUNCE_WND_MASK,
				 FIELD_PREP(MT6358_BATON_DEBOUNCE_THD_MASK,
					    MT6358_BATON_DEBOUNCE_THD_FAST) |
				 FIELD_PREP(MT6358_BATON_DEBOUNCE_WND_MASK,
					    MT6358_BATON_DEBOUNCE_WND_16MS));
	if (ret)
		return ret;

	ret = regmap_write(gauge->regmap, MT6358_AUXADC_NAG_0,
			   MT6358_AUXADC_NAG_PRD_VAL);
	if (ret)
		return ret;

	ret = regmap_write(gauge->regmap, MT6358_FGADC_SOFF_CON0,
			   MT6358_FG_SOFF_SLP_CUR_TH_VAL);
	if (ret)
		return ret;

	ret = regmap_update_bits(gauge->regmap, MT6358_FGADC_ANA_TEST_CON0,
				 MT6358_FG_DWA_RST_SW | MT6358_FG_DWA_RST_MODE, 0);
	if (ret)
		return ret;

	ret = regmap_update_bits(gauge->regmap, MT6358_FGADC_ANA_CON0,
				 MT6358_FG_SPARE, 0);
	if (ret)
		return ret;

	ret = regmap_read(gauge->regmap, MT6358_FGADC_RST_CON0, &val);
	if (ret)
		return ret;

	gauge->is_warm_boot = (val & MT6358_FG_RSTB_STATUS) != 0;

	if (gauge->is_warm_boot)
		return 0;

	/* Reset FG */
	ret = regmap_update_bits(gauge->regmap, MT6358_FGADC_CON1,
				 MT6358_FG_SW_RESET_MASK,
				 MT6358_FG_SW_RESET_MASK);
	if (ret)
		return ret;

	fsleep(USEC_PER_MSEC);
	ret = regmap_update_bits(gauge->regmap, MT6358_FGADC_CON1,
				 MT6358_FG_SW_RESET_MASK, 0);
	if (ret)
		return ret;

	return regmap_update_bits(gauge->regmap, MT6358_FGADC_RST_CON0,
				  MT6358_FG_RSTB_STATUS, MT6358_FG_RSTB_STATUS);
}

static int mt6358_gauge_probe(struct platform_device *pdev)
{
	struct mt6397_chip *mt6397 = dev_get_drvdata(pdev->dev.parent);
	struct power_supply_config psy_cfg = {};
	struct device *dev = &pdev->dev;
	struct mt6358_gauge *gauge;
	int saved_soc = -1;
	u32 val;
	int ret;

	gauge = devm_kzalloc(dev, sizeof(*gauge), GFP_KERNEL);
	if (!gauge)
		return -ENOMEM;

	gauge->dev = dev;
	gauge->regmap = mt6397->regmap;
	mutex_init(&gauge->lock);
	platform_set_drvdata(pdev, gauge);

	if (device_property_read_u32(dev, "shunt-resistor-micro-ohms", &gauge->r_fg_uohm))
		gauge->r_fg_uohm = (u32)MT6358_R_FG_NOMINAL_UOHM;

	/* Calibration multiplier */
	if (device_property_read_u32(dev, "mediatek,car-tune-value", &gauge->car_tune_value))
		gauge->car_tune_value = (u32)MT6358_CAR_TUNE_NOMINAL;

	gauge->chan_bat_voltage = devm_iio_channel_get(dev, NULL);
	if (IS_ERR(gauge->chan_bat_voltage))
		return dev_err_probe(dev, PTR_ERR(gauge->chan_bat_voltage),
				     "failed to get battery voltage IIO channel\n");

	gauge->cell_init = devm_nvmem_cell_get(dev, "initialization");
	if (IS_ERR(gauge->cell_init)) {
		ret = PTR_ERR(gauge->cell_init);
		if (ret == -ENOENT)
			gauge->cell_init = NULL;

		return ret;
	}

	gauge->cell_soc = devm_nvmem_cell_get(dev, "state-of-charge");
	if (IS_ERR(gauge->cell_soc)) {
		ret = PTR_ERR(gauge->cell_soc);
		if (ret == -ENOENT)
			gauge->cell_soc = NULL;

		return ret;
	}

	ret = mt6358_fg_hw_init(gauge);
	if (ret)
		return dev_err_probe(dev, ret, "failed to initialize fg\n");

	gauge->present = mt6358_fg_is_present(gauge);

	gauge->psy_desc.name = "mt6358-battery";
	gauge->psy_desc.type = POWER_SUPPLY_TYPE_BATTERY;
	gauge->psy_desc.properties = mt6358_gauge_props;
	gauge->psy_desc.num_properties = ARRAY_SIZE(mt6358_gauge_props);
	gauge->psy_desc.get_property = mt6358_gauge_get_property;

	psy_cfg.fwnode = dev_fwnode(dev);
	psy_cfg.drv_data = gauge;

	gauge->psy = devm_power_supply_register(dev, &gauge->psy_desc, &psy_cfg);
	if (IS_ERR(gauge->psy))
		return dev_err_probe(dev, PTR_ERR(gauge->psy),
				     "failed to register power supply\n");

	ret = power_supply_get_battery_info(gauge->psy, &gauge->bat_info);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get battery info\n");

	if (gauge->bat_info->charge_full_design_uah <= 0)
		return dev_err_probe(dev, -EINVAL,
				     "invalid battery full charge design capacity\n");

	gauge->total_cap_uah = gauge->bat_info->charge_full_design_uah;

	/* Read saved State Of Charge from nvmem if warm boot */
	if (gauge->cell_soc) {
		size_t len = 0;
		u8 *buf = nvmem_cell_read(gauge->cell_soc, &len);

		if (!IS_ERR(buf) && len >= 1) {
			if (buf[0] & MT6358_FG_SOC_VALID)
				saved_soc = FIELD_GET(MT6358_FG_SOC_MASK, buf[0]);
			kfree(buf);
		}
	}

	ret = mt6358_fg_read_boot_ocv(gauge, &gauge->boot_ocv_uv);
	if (ret)
		gauge->boot_ocv_uv = MT6358_DEFAULT_OCV_UV;

	if (saved_soc >= 0 && saved_soc <= PERCENT && gauge->is_warm_boot) {
		gauge->soc = saved_soc;
	} else if (gauge->bat_info->ocv_table[0]) {
		gauge->soc = power_supply_batinfo_ocv2cap(gauge->bat_info,
							  gauge->boot_ocv_uv,
							  MT6358_OCV_TEMP_NOMINAL_C);
	} else if (gauge->bat_info->voltage_max_design_uv > gauge->bat_info->voltage_min_design_uv &&
		   gauge->bat_info->voltage_min_design_uv > 0) {
		int min_uv = gauge->bat_info->voltage_min_design_uv;
		int max_uv = gauge->bat_info->voltage_max_design_uv;

		val = (u32)clamp((int)div_s64((s64)(gauge->boot_ocv_uv - min_uv) * PERCENT,
					      max_uv - min_uv), 0, PERCENT);
		gauge->soc = val;
	} else {
		return dev_err_probe(dev, -EINVAL,
				     "no battery OCV table or min/max design voltages\n");
	}

	gauge->initial_charge_uah = div_s64(gauge->total_cap_uah * gauge->soc, PERCENT);

	mt6358_fg_set_coulomb_thresholds(gauge);

	gauge->irq_coulomb_h = platform_get_irq_byname_optional(pdev, "COULOMB_H");
	gauge->irq_coulomb_l = platform_get_irq_byname_optional(pdev, "COULOMB_L");
	gauge->irq_bat_out = platform_get_irq_byname_optional(pdev, "BAT_OUT");

	if (gauge->irq_coulomb_h > 0) {
		ret = devm_request_threaded_irq(dev, gauge->irq_coulomb_h,
						NULL, mt6358_fg_coulomb_irq,
						IRQF_ONESHOT, "mt6358-coulomb-h", gauge);
		if (ret)
			dev_warn(dev, "failed to request COULOMB_H IRQ: %d\n", ret);
	}

	if (gauge->irq_coulomb_l > 0) {
		ret = devm_request_threaded_irq(dev, gauge->irq_coulomb_l,
						NULL, mt6358_fg_coulomb_irq,
						IRQF_ONESHOT, "mt6358-coulomb-l", gauge);
		if (ret)
			dev_warn(dev, "failed to request COULOMB_L IRQ: %d\n", ret);
	}

	if (gauge->irq_bat_out > 0) {
		ret = devm_request_threaded_irq(dev, gauge->irq_bat_out,
						NULL, mt6358_fg_bat_out_irq,
						IRQF_ONESHOT, "mt6358-bat-out", gauge);
		if (ret)
			dev_warn(dev, "failed to request BAT_OUT IRQ: %d\n", ret);
	}

	INIT_DELAYED_WORK(&gauge->monitor_work, mt6358_fg_monitor_work);
	schedule_delayed_work(&gauge->monitor_work,
			      msecs_to_jiffies(MT6358_FG_MONITOR_DELAY_MS));

	return 0;
}

static void mt6358_gauge_remove(struct platform_device *pdev)
{
	struct mt6358_gauge *gauge = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&gauge->monitor_work);
}

static int mt6358_gauge_suspend(struct device *dev)
{
	struct mt6358_gauge *gauge = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&gauge->monitor_work);
	return 0;
}

static int mt6358_gauge_resume(struct device *dev)
{
	struct mt6358_gauge *gauge = dev_get_drvdata(dev);

	mt6358_fg_update_soc(gauge);
	power_supply_changed(gauge->psy);
	schedule_delayed_work(&gauge->monitor_work,
			      msecs_to_jiffies(MT6358_FG_MONITOR_DELAY_MS));

	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(mt6358_gauge_pm_ops,
				mt6358_gauge_suspend, mt6358_gauge_resume);

static const struct of_device_id mt6358_gauge_of_match[] = {
	{ .compatible = "mediatek,mt6358-gauge" },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6358_gauge_of_match);

static struct platform_driver mt6358_gauge_driver = {
	.probe = mt6358_gauge_probe,
	.remove = mt6358_gauge_remove,
	.driver = {
		.name = "mt6358-gauge",
		.of_match_table = mt6358_gauge_of_match,
		.pm = pm_sleep_ptr(&mt6358_gauge_pm_ops),
	},
};
module_platform_driver(mt6358_gauge_driver);

MODULE_AUTHOR("Roman Vivchar <rva333@protonmail.com>");
MODULE_DESCRIPTION("MediaTek MT6358 Fuel Gauge driver");
MODULE_LICENSE("GPL");
