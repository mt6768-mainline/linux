// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 * Copyright (c) 2024 Collabora Ltd.
 *                    AngeloGioacchino Del Regno <angelogioacchino.delregno@collabora.com>
 */

#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/arm-smccc.h>
#include <linux/bitfield.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/dvfsrc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

/* DVFSRC_LEVEL */
#define DVFSRC_V1_LEVEL_TARGET_LEVEL	GENMASK(15, 0)
#define DVFSRC_TGT_LEVEL_IDLE		0x00
#define DVFSRC_V1_LEVEL_CURRENT_LEVEL	GENMASK(31, 16)

/* DVFSRC_SW_REQ, DVFSRC_SW_REQ2 */
#define DVFSRC_V1_SW_REQ2_DRAM_LEVEL	GENMASK(1, 0)
#define DVFSRC_V1_SW_REQ2_VCORE_LEVEL	GENMASK(3, 2)

#define DVFSRC_V2_SW_REQ_DRAM_LEVEL	GENMASK(3, 0)
#define DVFSRC_V2_SW_REQ_VCORE_LEVEL	GENMASK(6, 4)

/* DVFSRC_VCORE */
#define DVFSRC_V1_VCORE_REQ_VSCP_LEVEL	GENMASK(30, 3)
#define DVFSRC_V2_VCORE_REQ_VSCP_LEVEL	GENMASK(14, 12)

#define DVFSRC_POLL_TIMEOUT_US		1000
#define STARTUP_TIME_US			1

#define MTK_SIP_DVFSRC_INIT		0x0
#define MTK_SIP_DVFSRC_START		0x1

struct dvfsrc_bw_constraints {
	u16 max_dram_nom_bw;
	u16 max_dram_peak_bw;
	u16 max_dram_hrt_bw;
};

struct dvfsrc_opp {
	u32 vcore_opp;
	u32 dram_opp;
};

struct dvfsrc_opp_desc {
	const struct dvfsrc_opp *opps;
	u32 num_opp;
};

struct dvfsrc_soc_data;
struct mtk_dvfsrc {
	struct device *dev;
	struct platform_device *icc;
	struct platform_device *regulator;
	const struct dvfsrc_soc_data *dvd;
	const struct dvfsrc_opp_desc *curr_opps;
	void __iomem *regs;
	int dram_type;
};

struct dvfsrc_soc_data {
	const int *regs;
	const struct dvfsrc_opp_desc *opps_desc;
	u32 (*get_target_level)(struct mtk_dvfsrc *dvfsrc);
	u32 (*get_current_level)(struct mtk_dvfsrc *dvfsrc);
	u32 (*get_vcore_level)(struct mtk_dvfsrc *dvfsrc);
	u32 (*get_vscp_level)(struct mtk_dvfsrc *dvfsrc);
	void (*set_dram_bw)(struct mtk_dvfsrc *dvfsrc, u64 bw);
	void (*set_dram_peak_bw)(struct mtk_dvfsrc *dvfsrc, u64 bw);
	void (*set_dram_hrt_bw)(struct mtk_dvfsrc *dvfsrc, u64 bw);
	void (*set_opp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	void (*set_vcore_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	void (*set_vscp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	int (*wait_for_opp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	int (*wait_for_vcore_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	const struct dvfsrc_bw_constraints *bw_constraints;
};

static u32 dvfsrc_readl(struct mtk_dvfsrc *dvfs, u32 offset)
{
	return readl(dvfs->regs + dvfs->dvd->regs[offset]);
}

static void dvfsrc_writel(struct mtk_dvfsrc *dvfs, u32 offset, u32 val)
{
	writel(val, dvfs->regs + dvfs->dvd->regs[offset]);
}

enum dvfsrc_regs {
	DVFSRC_SW_REQ,
	DVFSRC_SW_REQ2,
	DVFSRC_LEVEL,
	DVFSRC_TARGET_LEVEL,
	DVFSRC_SW_BW,
	DVFSRC_SW_PEAK_BW,
	DVFSRC_SW_HRT_BW,
	DVFSRC_VCORE,
	DVFSRC_REGS_MAX,
};

static const int dvfsrc_mt6768_regs[] = {
	[DVFSRC_SW_REQ] =		0x4,
	[DVFSRC_LEVEL] =		0xDC,
	[DVFSRC_SW_BW] =		0x16C,
	[DVFSRC_SW_PEAK_BW] =		0x160,
	[DVFSRC_VCORE] =	0x48,
};

static const int dvfsrc_mt8183_regs[] = {
	[DVFSRC_SW_REQ] = 0x4,
	[DVFSRC_SW_REQ2] = 0x8,
	[DVFSRC_LEVEL] = 0xDC,
	[DVFSRC_SW_BW] = 0x160,
};

static const int dvfsrc_mt8195_regs[] = {
	[DVFSRC_SW_REQ] = 0xc,
	[DVFSRC_VCORE] = 0x6c,
	[DVFSRC_SW_PEAK_BW] = 0x278,
	[DVFSRC_SW_BW] = 0x26c,
	[DVFSRC_SW_HRT_BW] = 0x290,
	[DVFSRC_LEVEL] = 0xd44,
	[DVFSRC_TARGET_LEVEL] = 0xd48,
};

static const struct dvfsrc_opp *dvfsrc_get_current_opp(struct mtk_dvfsrc *dvfsrc)
{
	u32 level = dvfsrc->dvd->get_current_level(dvfsrc);

	return &dvfsrc->curr_opps->opps[level];
}

static bool dvfsrc_is_idle(struct mtk_dvfsrc *dvfsrc)
{
	if (!dvfsrc->dvd->get_target_level)
		return true;

	return dvfsrc->dvd->get_target_level(dvfsrc) == DVFSRC_TGT_LEVEL_IDLE;
}

static int dvfsrc_wait_for_vcore_level_v1(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	const struct dvfsrc_opp *curr;

	return readx_poll_timeout_atomic(dvfsrc_get_current_opp, dvfsrc, curr,
					 curr->vcore_opp >= level, STARTUP_TIME_US,
					 DVFSRC_POLL_TIMEOUT_US);
}

static int dvfsrc_wait_for_opp_level_v1(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	const struct dvfsrc_opp *target, *curr;
	int ret;

	target = &dvfsrc->curr_opps->opps[level];
	ret = readx_poll_timeout_atomic(dvfsrc_get_current_opp, dvfsrc, curr,
					curr->dram_opp >= target->dram_opp &&
					curr->vcore_opp >= target->vcore_opp,
					STARTUP_TIME_US, DVFSRC_POLL_TIMEOUT_US);
	if (ret < 0) {
		dev_warn(dvfsrc->dev,
			 "timeout! target OPP: %u, dram: %d, vcore: %d\n", level,
			 curr->dram_opp, curr->vcore_opp);
		return ret;
	}

	return 0;
}

static int dvfsrc_wait_for_opp_level_v2(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	const struct dvfsrc_opp *target, *curr;
	int ret;

	target = &dvfsrc->curr_opps->opps[level];
	ret = readx_poll_timeout_atomic(dvfsrc_get_current_opp, dvfsrc, curr,
					curr->dram_opp >= target->dram_opp &&
					curr->vcore_opp >= target->vcore_opp,
					STARTUP_TIME_US, DVFSRC_POLL_TIMEOUT_US);
	if (ret < 0) {
		dev_warn(dvfsrc->dev,
			 "timeout! target OPP: %u, dram: %d\n", level, curr->dram_opp);
		return ret;
	}

	return 0;
}

static u32 dvfsrc_get_target_level_v1(struct mtk_dvfsrc *dvfsrc)
{
	u32 val = dvfsrc_readl(dvfsrc, DVFSRC_LEVEL);

	return FIELD_GET(DVFSRC_V1_LEVEL_TARGET_LEVEL, val);
}

static u32 dvfsrc_get_current_level_v1(struct mtk_dvfsrc *dvfsrc)
{
	u32 val = dvfsrc_readl(dvfsrc, DVFSRC_LEVEL);
	u32 current_level = FIELD_GET(DVFSRC_V1_LEVEL_CURRENT_LEVEL, val);

	return ffs(current_level) - 1;
}

static u32 dvfsrc_get_target_level_v2(struct mtk_dvfsrc *dvfsrc)
{
	return dvfsrc_readl(dvfsrc, DVFSRC_TARGET_LEVEL);
}

static u32 dvfsrc_get_current_level_v2(struct mtk_dvfsrc *dvfsrc)
{
	u32 val = dvfsrc_readl(dvfsrc, DVFSRC_LEVEL);
	u32 level = ffs(val);

	/* Valid levels */
	if (level < dvfsrc->curr_opps->num_opp)
		return dvfsrc->curr_opps->num_opp - level;

	/* Zero for level 0 or invalid level */
	return 0;
}

static u32 dvfsrc_get_vcore_level_v1(struct mtk_dvfsrc *dvfsrc)
{
	u32 val = dvfsrc_readl(dvfsrc, DVFSRC_SW_REQ2);

	return FIELD_GET(DVFSRC_V1_SW_REQ2_VCORE_LEVEL, val);
}

static void dvfsrc_set_vcore_level_v1(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	u32 val = dvfsrc_readl(dvfsrc, DVFSRC_SW_REQ2);

	val &= ~DVFSRC_V1_SW_REQ2_VCORE_LEVEL;
	val |= FIELD_PREP(DVFSRC_V1_SW_REQ2_VCORE_LEVEL, level);

	dvfsrc_writel(dvfsrc, DVFSRC_SW_REQ2, val);
}

static u32 dvfsrc_get_vcore_level_v2(struct mtk_dvfsrc *dvfsrc)
{
	u32 val = dvfsrc_readl(dvfsrc, DVFSRC_SW_REQ);

	return FIELD_GET(DVFSRC_V2_SW_REQ_VCORE_LEVEL, val);
}

static void dvfsrc_set_vcore_level_v2(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	u32 val = dvfsrc_readl(dvfsrc, DVFSRC_SW_REQ);

	val &= ~DVFSRC_V2_SW_REQ_VCORE_LEVEL;
	val |= FIELD_PREP(DVFSRC_V2_SW_REQ_VCORE_LEVEL, level);

	dvfsrc_writel(dvfsrc, DVFSRC_SW_REQ, val);
}

static u32 dvfsrc_get_vscp_level_v1(struct mtk_dvfsrc *dvfsrc)
{
	u32 val = dvfsrc_readl(dvfsrc, DVFSRC_VCORE);

	return FIELD_GET(DVFSRC_V1_VCORE_REQ_VSCP_LEVEL, val);
}

static void dvfsrc_set_vscp_level_v1(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	u32 val = dvfsrc_readl(dvfsrc, DVFSRC_VCORE);

	val &= ~DVFSRC_V1_VCORE_REQ_VSCP_LEVEL;
	val |= FIELD_PREP(DVFSRC_V1_VCORE_REQ_VSCP_LEVEL, level);

	dvfsrc_writel(dvfsrc, DVFSRC_VCORE, val);
}

static u32 dvfsrc_get_vscp_level_v2(struct mtk_dvfsrc *dvfsrc)
{
	u32 val = dvfsrc_readl(dvfsrc, DVFSRC_VCORE);

	return FIELD_GET(DVFSRC_V2_VCORE_REQ_VSCP_LEVEL, val);
}

static void dvfsrc_set_vscp_level_v2(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	u32 val = dvfsrc_readl(dvfsrc, DVFSRC_VCORE);

	val &= ~DVFSRC_V2_VCORE_REQ_VSCP_LEVEL;
	val |= FIELD_PREP(DVFSRC_V2_VCORE_REQ_VSCP_LEVEL, level);

	dvfsrc_writel(dvfsrc, DVFSRC_VCORE, val);
}

static void __dvfsrc_set_dram_bw_v1(struct mtk_dvfsrc *dvfsrc, u32 reg,
				    u16 max_bw, u16 min_bw, u64 bw)
{
	u32 new_bw = (u32)div_u64(bw, 100 * 1000);

	/* If bw constraints (in mbps) are defined make sure to respect them */
	if (max_bw)
		new_bw = min(new_bw, max_bw);
	if (min_bw && new_bw > 0)
		new_bw = max(new_bw, min_bw);

	dvfsrc_writel(dvfsrc, reg, new_bw);
}

static void dvfsrc_set_dram_bw_v1(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	u64 max_bw = dvfsrc->dvd->bw_constraints->max_dram_nom_bw;

	__dvfsrc_set_dram_bw_v1(dvfsrc, DVFSRC_SW_BW, max_bw, 0, bw);
};

static void dvfsrc_set_dram_peak_bw_v1(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	u64 max_bw = dvfsrc->dvd->bw_constraints->max_dram_peak_bw;

	__dvfsrc_set_dram_bw_v1(dvfsrc, DVFSRC_SW_PEAK_BW, max_bw, 0, bw);
}

static void dvfsrc_set_dram_hrt_bw_v1(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	u64 max_bw = dvfsrc->dvd->bw_constraints->max_dram_hrt_bw;

	__dvfsrc_set_dram_bw_v1(dvfsrc, DVFSRC_SW_HRT_BW, max_bw, 0, bw);
}

static void dvfsrc_set_opp_level_v1(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	const struct dvfsrc_opp *opp = &dvfsrc->curr_opps->opps[level];
	u32 val;

	/* Translate Pstate to DVFSRC level and set it to DVFSRC HW */
	val = FIELD_PREP(DVFSRC_V1_SW_REQ2_DRAM_LEVEL, opp->dram_opp);
	val |= FIELD_PREP(DVFSRC_V1_SW_REQ2_VCORE_LEVEL, opp->vcore_opp);

	dev_dbg(dvfsrc->dev, "vcore_opp: %d, dram_opp: %d\n", opp->vcore_opp, opp->dram_opp);
	dvfsrc_writel(dvfsrc, DVFSRC_SW_REQ, val);
}

int mtk_dvfsrc_send_request(const struct device *dev, u32 cmd, u64 data)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);
	bool state;
	int ret;

	dev_dbg(dvfsrc->dev, "cmd: %d, data: %llu\n", cmd, data);

	switch (cmd) {
	case MTK_DVFSRC_CMD_BW:
		dvfsrc->dvd->set_dram_bw(dvfsrc, data);
		return 0;
	case MTK_DVFSRC_CMD_HRT_BW:
		if (dvfsrc->dvd->set_dram_hrt_bw)
			dvfsrc->dvd->set_dram_hrt_bw(dvfsrc, data);
		return 0;
	case MTK_DVFSRC_CMD_PEAK_BW:
		if (dvfsrc->dvd->set_dram_peak_bw)
			dvfsrc->dvd->set_dram_peak_bw(dvfsrc, data);
		return 0;
	case MTK_DVFSRC_CMD_OPP:
		if (!dvfsrc->dvd->set_opp_level)
			return 0;

		dvfsrc->dvd->set_opp_level(dvfsrc, data);
		break;
	case MTK_DVFSRC_CMD_VCORE_LEVEL:
		dvfsrc->dvd->set_vcore_level(dvfsrc, data);
		break;
	case MTK_DVFSRC_CMD_VSCP_LEVEL:
		if (!dvfsrc->dvd->set_vscp_level)
			return 0;

		dvfsrc->dvd->set_vscp_level(dvfsrc, data);
		break;
	default:
		dev_err(dvfsrc->dev, "unknown command: %d\n", cmd);
		return -EOPNOTSUPP;
	}

	/* DVFSRC needs at least 2T(~196ns) to handle a request */
	udelay(STARTUP_TIME_US);

	ret = readx_poll_timeout_atomic(dvfsrc_is_idle, dvfsrc, state, state,
					STARTUP_TIME_US, DVFSRC_POLL_TIMEOUT_US);
	if (ret < 0) {
		dev_warn(dvfsrc->dev,
			 "%d: idle timeout, data: %llu, last: %d -> %d\n", cmd, data,
			 dvfsrc->dvd->get_current_level(dvfsrc),
			 dvfsrc->dvd->get_target_level(dvfsrc));
		return ret;
	}

	if (cmd == MTK_DVFSRC_CMD_OPP)
		ret = dvfsrc->dvd->wait_for_opp_level(dvfsrc, data);
	else
		ret = dvfsrc->dvd->wait_for_vcore_level(dvfsrc, data);

	if (ret < 0) {
		dev_warn(dvfsrc->dev,
			 "%d: wait timeout, data: %llu, last: %d -> %d\n",
			 cmd, data,
			 dvfsrc->dvd->get_current_level(dvfsrc),
			 dvfsrc->dvd->get_target_level(dvfsrc));
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_dvfsrc_send_request);

int mtk_dvfsrc_query_info(const struct device *dev, u32 cmd, int *data)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	switch (cmd) {
	case MTK_DVFSRC_CMD_VCORE_LEVEL:
		*data = dvfsrc->dvd->get_vcore_level(dvfsrc);
		break;
	case MTK_DVFSRC_CMD_VSCP_LEVEL:
		*data = dvfsrc->dvd->get_vscp_level(dvfsrc);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_dvfsrc_query_info);

#define DVFSRC_BASIC_CONTROL       (0x0)
#define DVFSRC_SW_REQ              (0x4)
#define DVFSRC_SW_REQ2             (0x8)
#define DVFSRC_EMI_REQUEST         (0xC)
#define DVFSRC_EMI_REQUEST2        (0x10)
#define DVFSRC_EMI_REQUEST3        (0x14)
#define DVFSRC_EMI_HRT             (0x18)
#define DVFSRC_EMI_HRT2            (0x1C)
#define DVFSRC_EMI_HRT3            (0x20)
#define DVFSRC_EMI_QOS0            (0x24)
#define DVFSRC_EMI_QOS1            (0x28)
#define DVFSRC_EMI_QOS2            (0x2C)
#define DVFSRC_EMI_MD2SPM0         (0x30)
#define DVFSRC_EMI_MD2SPM1         (0x34)
#define DVFSRC_EMI_MD2SPM2         (0x38)
#define DVFSRC_EMI_MD2SPM0_T       (0x3C)
#define DVFSRC_EMI_MD2SPM1_T       (0x40)
#define DVFSRC_EMI_MD2SPM2_T       (0x44)
#define DVFSRC_VCORE_REQUEST       (0x48)
#define DVFSRC_VCORE_REQUEST2      (0x4C)
#define DVFSRC_VCORE_HRT           (0x50)
#define DVFSRC_VCORE_HRT2          (0x54)
#define DVFSRC_VCORE_HRT3          (0x58)
#define DVFSRC_VCORE_QOS0          (0x5C)
#define DVFSRC_VCORE_QOS1          (0x60)
#define DVFSRC_VCORE_QOS2          (0x64)
#define DVFSRC_VCORE_MD2SPM0       (0x68)
#define DVFSRC_VCORE_MD2SPM1       (0x6C)
#define DVFSRC_VCORE_MD2SPM2       (0x70)
#define DVFSRC_VCORE_MD2SPM0_T     (0x74)
#define DVFSRC_VCORE_MD2SPM1_T     (0x78)
#define DVFSRC_VCORE_MD2SPM2_T     (0x7C)
#define DVFSRC_MD_REQUEST          (0x80)
#define DVFSRC_MD_SW_CONTROL	   (0x84)
#define DVFSRC_MD_VMODEM_REMAP     (0x88)
#define DVFSRC_MD_VMD_REMAP        (0x8C)
#define DVFSRC_MD_VSRAM_REMAP      (0x90)
#define DVFSRC_HALT_SW_CONTROL	   (0x94)
#define DVFSRC_INT                 (0x98)
#define DVFSRC_INT_EN              (0x9C)
#define DVFSRC_INT_CLR             (0xA0)
#define DVFSRC_BW_MON_WINDOW       (0xA4)
#define DVFSRC_BW_MON_THRES_1      (0xA8)
#define DVFSRC_BW_MON_THRES_2      (0xAC)
#define DVFSRC_MD_TURBO            (0xB0)
#define DVFSRC_DEBOUNCE_FOUR       (0xD0)
#define DVFSRC_DEBOUNCE_RISE_FALL  (0xD4)
#define DVFSRC_TIMEOUT_NEXTREQ     (0xD8)
#define DVFSRC_LEVEL               (0xDC)
#define DVFSRC_LEVEL_LABEL_0_1     (0xE0)
#define DVFSRC_LEVEL_LABEL_2_3     (0xE4)
#define DVFSRC_LEVEL_LABEL_4_5     (0xE8)
#define DVFSRC_LEVEL_LABEL_6_7     (0xEC)
#define DVFSRC_LEVEL_LABEL_8_9     (0xF0)
#define DVFSRC_LEVEL_LABEL_10_11   (0xF4)
#define DVFSRC_LEVEL_LABEL_12_13   (0xF8)
#define DVFSRC_LEVEL_LABEL_14_15   (0xFC)
#define DVFSRC_MM_BW_0             (0x100)
#define DVFSRC_MM_BW_1             (0x104)
#define DVFSRC_MM_BW_2             (0x108)
#define DVFSRC_MM_BW_3             (0x10C)
#define DVFSRC_MM_BW_4             (0x110)
#define DVFSRC_MM_BW_5             (0x114)
#define DVFSRC_MM_BW_6             (0x118)
#define DVFSRC_MM_BW_7             (0x11C)
#define DVFSRC_MM_BW_8             (0x120)
#define DVFSRC_MM_BW_9             (0x124)
#define DVFSRC_MM_BW_10            (0x128)
#define DVFSRC_MM_BW_11            (0x12C)
#define DVFSRC_MM_BW_12            (0x130)
#define DVFSRC_MM_BW_13            (0x134)
#define DVFSRC_MM_BW_14            (0x138)
#define DVFSRC_MM_BW_15            (0x13C)
#define DVFSRC_MD_BW_0             (0x140)
#define DVFSRC_MD_BW_1             (0x144)
#define DVFSRC_MD_BW_2             (0x148)
#define DVFSRC_MD_BW_3             (0x14C)
#define DVFSRC_MD_BW_4             (0x150)
#define DVFSRC_MD_BW_5             (0x154)
#define DVFSRC_MD_BW_6             (0x158)
#define DVFSRC_MD_BW_7             (0x15C)
#define DVFSRC_SW_BW_0             (0x160)
#define DVFSRC_SW_BW_1             (0x164)
#define DVFSRC_SW_BW_2             (0x168)
#define DVFSRC_SW_BW_3             (0x16C)
#define DVFSRC_SW_BW_4             (0x170)
#define DVFSRC_QOS_EN              (0x180)
#define DVFSRC_ISP_HRT             (0x190)
#define DVFSRC_FORCE               (0x300)
#define DVFSRC_SEC_SW_REQ          (0x304)
#define DVFSRC_LAST                (0x308)
#define DVFSRC_LAST_L              (0x30C)
#define DVFSRC_MD_SCENARIO         (0X310)
#define DVFSRC_RECORD_0_0          (0x400)
#define DVFSRC_RECORD_0_1          (0x404)
#define DVFSRC_RECORD_0_2          (0x408)
#define DVFSRC_RECORD_1_0          (0x40C)
#define DVFSRC_RECORD_1_1          (0x410)
#define DVFSRC_RECORD_1_2          (0x414)
#define DVFSRC_RECORD_2_0          (0x418)
#define DVFSRC_RECORD_2_1          (0x41C)
#define DVFSRC_RECORD_2_2          (0x420)
#define DVFSRC_RECORD_3_0          (0x424)
#define DVFSRC_RECORD_3_1          (0x428)
#define DVFSRC_RECORD_3_2          (0x42C)
#define DVFSRC_RECORD_4_0          (0x430)
#define DVFSRC_RECORD_4_1          (0x434)
#define DVFSRC_RECORD_4_2          (0x438)
#define DVFSRC_RECORD_5_0          (0x43C)
#define DVFSRC_RECORD_5_1          (0x440)
#define DVFSRC_RECORD_5_2          (0x444)
#define DVFSRC_RECORD_6_0          (0x448)
#define DVFSRC_RECORD_6_1          (0x44C)
#define DVFSRC_RECORD_6_2          (0x450)
#define DVFSRC_RECORD_7_0          (0x454)
#define DVFSRC_RECORD_7_1          (0x458)
#define DVFSRC_RECORD_7_2          (0x45C)
#define DVFSRC_RECORD_0_L_0        (0x460)
#define DVFSRC_RECORD_0_L_1        (0x464)
#define DVFSRC_RECORD_0_L_2        (0x468)
#define DVFSRC_RECORD_1_L_0        (0x46C)
#define DVFSRC_RECORD_1_L_1        (0x470)
#define DVFSRC_RECORD_1_L_2        (0x474)
#define DVFSRC_RECORD_2_L_0        (0x478)
#define DVFSRC_RECORD_2_L_1        (0x47C)
#define DVFSRC_RECORD_2_L_2        (0x480)
#define DVFSRC_RECORD_3_L_0        (0x484)
#define DVFSRC_RECORD_3_L_1        (0x488)
#define DVFSRC_RECORD_3_L_2        (0x48C)
#define DVFSRC_RECORD_4_L_0        (0x490)
#define DVFSRC_RECORD_4_L_1        (0x494)
#define DVFSRC_RECORD_4_L_2        (0x498)
#define DVFSRC_RECORD_5_L_0        (0x49C)
#define DVFSRC_RECORD_5_L_1        (0x4A0)
#define DVFSRC_RECORD_5_L_2        (0x4A4)
#define DVFSRC_RECORD_6_L_0        (0x4A8)
#define DVFSRC_RECORD_6_L_1        (0x4AC)
#define DVFSRC_RECORD_6_L_2        (0x4B0)
#define DVFSRC_RECORD_7_L_0        (0x4B4)
#define DVFSRC_RECORD_7_L_1        (0x4B8)
#define DVFSRC_RECORD_7_L_2        (0x4BC)
#define DVFSRC_RECORD_MD_0         (0x4C0)
#define DVFSRC_RECORD_MD_1         (0x4C4)
#define DVFSRC_RECORD_MD_2         (0x4C8)
#define DVFSRC_RECORD_MD_3         (0x4CC)
#define DVFSRC_RECORD_MD_4         (0x4D0)
#define DVFSRC_RECORD_MD_5         (0x4D4)
#define DVFSRC_RECORD_MD_6         (0x4D8)
#define DVFSRC_RECORD_MD_7         (0x4DC)
#define DVFSRC_RECORD_COUNT        (0x4F0)
#define DVFSRC_RSRV_0              (0x600)
#define DVFSRC_RSRV_1              (0x604)
#define DVFSRC_RSRV_2              (0x608)
#define DVFSRC_RSRV_3              (0x60C)
#define DVFSRC_RSRV_4              (0x610)
#define DVFSRC_RSRV_5              (0x614)

static int dvfsrc_lp4x_2ch_3600[][128] = {
		{ DVFSRC_EMI_REQUEST,		0x00240009 },
		{ DVFSRC_EMI_REQUEST3,		0x09000000 },
		{ DVFSRC_EMI_HRT,		0x003E362C },
		{ DVFSRC_EMI_QOS0,		0x00000033 },
		{ DVFSRC_EMI_QOS1,		0x0000004C },
		{ DVFSRC_EMI_MD2SPM0,		0x0000003F },
		{ DVFSRC_EMI_MD2SPM1,		0x00000000 },
		{ DVFSRC_EMI_MD2SPM2,		0x000080C0 },
		{ DVFSRC_EMI_MD2SPM0_T,		0x00000007 },
		{ DVFSRC_EMI_MD2SPM1_T,		0x00000038 },
		{ DVFSRC_EMI_MD2SPM2_T,		0x000080C0 },

		{ DVFSRC_VCORE_HRT,		0x00000036 },

		{ DVFSRC_MD_SW_CONTROL,		0x20000000 },

		{ DVFSRC_TIMEOUT_NEXTREQ,	0x00000014 },
		{ DVFSRC_INT_EN,		0x00000003 },

		{ DVFSRC_LEVEL_LABEL_0_1,	0x00010000 },
		{ DVFSRC_LEVEL_LABEL_2_3,	0x00020101 },
		{ DVFSRC_LEVEL_LABEL_4_5,	0x01020012 },
		{ DVFSRC_LEVEL_LABEL_6_7,	0x02120112 },
		{ DVFSRC_LEVEL_LABEL_8_9,	0x00230013 },
		{ DVFSRC_LEVEL_LABEL_10_11,	0x01230113 },
		{ DVFSRC_LEVEL_LABEL_12_13,	0x02230213 },
		{ DVFSRC_LEVEL_LABEL_14_15,	0x03230323 },

		{ DVFSRC_FORCE,			0x40000000 },
		{ DVFSRC_RSRV_1,		0x0000000C },

		{ DVFSRC_QOS_EN,		0x0000407F },

		{ DVFSRC_BASIC_CONTROL,		0x0000407B },
		{ DVFSRC_BASIC_CONTROL,		0x0000017B },
		{ DVFSRC_FORCE,			0x00000000 },

		{ -1, 0 },
};

static int mtk_dvfsrc_probe(struct platform_device *pdev)
{
	struct arm_smccc_res ares;
	struct mtk_dvfsrc *dvfsrc;
	struct device_node *np;
	struct regmap *regmap;
	int ret, val;

	dvfsrc = devm_kzalloc(&pdev->dev, sizeof(*dvfsrc), GFP_KERNEL);
	if (!dvfsrc)
		return -ENOMEM;

	dvfsrc->dvd = of_device_get_match_data(&pdev->dev);
	dvfsrc->dev = &pdev->dev;

	dvfsrc->regs = devm_platform_get_and_ioremap_resource(pdev, 0, NULL);
	if (IS_ERR(dvfsrc->regs))
		return PTR_ERR(dvfsrc->regs);

	np = of_parse_phandle(pdev->dev.of_node, "mediatek,rgu", 0);

	if (!np) {
		arm_smccc_smc(MTK_SIP_DVFSRC_VCOREFS_CONTROL, MTK_SIP_DVFSRC_INIT,
		      0, 0, 0, 0, 0, 0, &ares);
		if (ares.a0)
			return dev_err_probe(&pdev->dev, -EINVAL, "DVFSRC init failed: %lu\n", ares.a0);

		dvfsrc->dram_type = ares.a1;
	} else {
		regmap = syscon_node_to_regmap(np);

		/* mtk_rgu_cfg_dvfsrc(1) */
		ret = regmap_read(regmap, 0xA0, &val);
		if (ret)
			return dev_err_probe(&pdev->dev, ret, "Failed to read MTK_WDT_DEBUG_CTL2\n");

		ret = regmap_write(regmap, 0xA0, val | BIT(9) | 0x55000000);
		if (ret)
			return dev_err_probe(&pdev->dev, ret, "Failed to write to MTK_WDT_DEBUG_CTL2\n");

		ret = regmap_read(regmap, 0x44, &val);
		if (ret)
			return dev_err_probe(&pdev->dev, ret, "Failed to read MTK_WDT_LATCH_CTL\n");

		ret = regmap_write(regmap, 0x44, val | BIT(13) | 0x95000000);
		if (ret)
			return dev_err_probe(&pdev->dev, ret, "Failed to write to MTK_WDT_LATCH_CTL\n");

		/* helio_dvfsrc_sram_reg_init() */
		for (val = 0; val < 0x80; val += 4)
			writel_relaxed(0, dvfsrc->regs + val);

		/* helio_dvfsrc_reg_config() */
		val = 0;
		while (dvfsrc_lp4x_2ch_3600[val][0] != 0) {
			writel(dvfsrc_lp4x_2ch_3600[val][1], dvfsrc->regs + dvfsrc_lp4x_2ch_3600[val][0]);
			val++;
		}

		dvfsrc->dram_type = 0;

		dev_info(&pdev->dev, "DVFSRC should be ready now...\n");
	}

	dev_dbg(&pdev->dev, "DRAM Type: %d\n", dvfsrc->dram_type);

	dvfsrc->curr_opps = &dvfsrc->dvd->opps_desc[dvfsrc->dram_type];
	platform_set_drvdata(pdev, dvfsrc);

	ret = devm_of_platform_populate(&pdev->dev);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "Failed to populate child devices\n");

	/* Everything is set up - make it run! */
	if (!np)
		arm_smccc_smc(MTK_SIP_DVFSRC_VCOREFS_CONTROL, MTK_SIP_DVFSRC_START,
		      0, 0, 0, 0, 0, 0, &ares);
	else
		/* helio_dvfsrc_enable(1) */
		arm_smccc_smc(0x82000226 | 0x40000000, BIT(3) | BIT(10), 0, 0, 0, 0, 0, 0, &ares); // MTK_SIP_KERNEL_SPM_VCOREFS_ARGS, SPM_FLAG_DIS_VCORE_DVS | SPM_FLAG_RUN_COMMON_SCENARIO
	if (ares.a0)
		return dev_err_probe(&pdev->dev, -EINVAL, "Cannot start DVFSRC: %lu\n", ares.a0);

	return 0;
}

static const struct dvfsrc_bw_constraints dvfsrc_bw_constr_v1 = { 0, 0, 0 };
static const struct dvfsrc_bw_constraints dvfsrc_bw_constr_v2 = {
	.max_dram_nom_bw = 255,
	.max_dram_peak_bw = 255,
	.max_dram_hrt_bw = 1023,
};

static const struct dvfsrc_opp dvfsrc_opp_mt6768[] = {
	{0, 0}, {1, 0}, {1, 0}, {2, 0},
	{2, 1}, {2, 0}, {2, 1}, {2, 1},
	{3, 1}, {3, 2}, {3, 1}, {3, 2},
	{3, 1}, {3, 2}, {3, 2}, {3, 2},
};

static const struct dvfsrc_opp_desc dvfsrc_opp_mt6768_desc[] = {
	[0] = {
		.opps = dvfsrc_opp_mt6768,
		.num_opp = ARRAY_SIZE(dvfsrc_opp_mt6768),
	}
};

static const struct dvfsrc_soc_data mt6768_data = {
	.opps_desc = dvfsrc_opp_mt6768_desc,
	.regs = dvfsrc_mt6768_regs,
	.get_target_level = dvfsrc_get_target_level_v2,
	.get_current_level = dvfsrc_get_current_level_v2,
	.get_vcore_level = dvfsrc_get_vcore_level_v1,
	.get_vscp_level = dvfsrc_get_vscp_level_v1,
	.set_dram_bw = dvfsrc_set_dram_bw_v1,
	.set_dram_peak_bw = dvfsrc_set_dram_peak_bw_v1,
	.set_dram_hrt_bw = dvfsrc_set_dram_hrt_bw_v1,
	.set_vcore_level = dvfsrc_set_vcore_level_v1,
	.set_vscp_level = dvfsrc_set_vscp_level_v1,
	.wait_for_opp_level = dvfsrc_wait_for_opp_level_v2,
	.wait_for_vcore_level = dvfsrc_wait_for_vcore_level_v1,
	.bw_constraints = &dvfsrc_bw_constr_v1,
};


static const struct dvfsrc_opp dvfsrc_opp_mt6893_lp4[] = {
	{ 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 0 },
	{ 0, 1 }, { 1, 1 }, { 2, 1 }, { 3, 1 },
	{ 0, 2 }, { 1, 2 }, { 2, 2 }, { 3, 2 },
	{ 0, 3 }, { 1, 3 }, { 2, 3 }, { 3, 3 },
	{ 1, 4 }, { 2, 4 }, { 3, 4 }, { 2, 5 },
	{ 3, 5 }, { 3, 6 }, { 4, 6 }, { 4, 7 },
};

static const struct dvfsrc_opp_desc dvfsrc_opp_mt6893_desc[] = {
	[0] = {
		.opps = dvfsrc_opp_mt6893_lp4,
		.num_opp = ARRAY_SIZE(dvfsrc_opp_mt6893_lp4),
	}
};

static const struct dvfsrc_soc_data mt6893_data = {
	.opps_desc = dvfsrc_opp_mt6893_desc,
	.regs = dvfsrc_mt8195_regs,
	.get_target_level = dvfsrc_get_target_level_v2,
	.get_current_level = dvfsrc_get_current_level_v2,
	.get_vcore_level = dvfsrc_get_vcore_level_v2,
	.get_vscp_level = dvfsrc_get_vscp_level_v2,
	.set_dram_bw = dvfsrc_set_dram_bw_v1,
	.set_dram_peak_bw = dvfsrc_set_dram_peak_bw_v1,
	.set_dram_hrt_bw = dvfsrc_set_dram_hrt_bw_v1,
	.set_vcore_level = dvfsrc_set_vcore_level_v2,
	.set_vscp_level = dvfsrc_set_vscp_level_v2,
	.wait_for_opp_level = dvfsrc_wait_for_opp_level_v2,
	.wait_for_vcore_level = dvfsrc_wait_for_vcore_level_v1,
	.bw_constraints = &dvfsrc_bw_constr_v2,
};

static const struct dvfsrc_opp dvfsrc_opp_mt8183_lp4[] = {
	{ 0, 0 }, { 0, 1 }, { 0, 2 }, { 1, 2 },
};

static const struct dvfsrc_opp dvfsrc_opp_mt8183_lp3[] = {
	{ 0, 0 }, { 0, 1 }, { 1, 1 }, { 1, 2 },
};

static const struct dvfsrc_opp_desc dvfsrc_opp_mt8183_desc[] = {
	[0] = {
		.opps = dvfsrc_opp_mt8183_lp4,
		.num_opp = ARRAY_SIZE(dvfsrc_opp_mt8183_lp4),
	},
	[1] = {
		.opps = dvfsrc_opp_mt8183_lp3,
		.num_opp = ARRAY_SIZE(dvfsrc_opp_mt8183_lp3),
	},
	[2] = {
		.opps = dvfsrc_opp_mt8183_lp3,
		.num_opp = ARRAY_SIZE(dvfsrc_opp_mt8183_lp3),
	}
};

static const struct dvfsrc_soc_data mt8183_data = {
	.opps_desc = dvfsrc_opp_mt8183_desc,
	.regs = dvfsrc_mt8183_regs,
	.get_target_level = dvfsrc_get_target_level_v1,
	.get_current_level = dvfsrc_get_current_level_v1,
	.get_vcore_level = dvfsrc_get_vcore_level_v1,
	.set_dram_bw = dvfsrc_set_dram_bw_v1,
	.set_opp_level = dvfsrc_set_opp_level_v1,
	.set_vcore_level = dvfsrc_set_vcore_level_v1,
	.wait_for_opp_level = dvfsrc_wait_for_opp_level_v1,
	.wait_for_vcore_level = dvfsrc_wait_for_vcore_level_v1,
	.bw_constraints = &dvfsrc_bw_constr_v1,
};

static const struct dvfsrc_opp dvfsrc_opp_mt8195_lp4[] = {
	{ 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 0 },
	{ 0, 1 }, { 1, 1 }, { 2, 1 }, { 3, 1 },
	{ 0, 2 }, { 1, 2 }, { 2, 2 }, { 3, 2 },
	{ 1, 3 }, { 2, 3 }, { 3, 3 }, { 1, 4 },
	{ 2, 4 }, { 3, 4 }, { 2, 5 }, { 3, 5 },
	{ 3, 6 },
};

static const struct dvfsrc_opp_desc dvfsrc_opp_mt8195_desc[] = {
	[0] = {
		.opps = dvfsrc_opp_mt8195_lp4,
		.num_opp = ARRAY_SIZE(dvfsrc_opp_mt8195_lp4),
	}
};

static const struct dvfsrc_soc_data mt8195_data = {
	.opps_desc = dvfsrc_opp_mt8195_desc,
	.regs = dvfsrc_mt8195_regs,
	.get_target_level = dvfsrc_get_target_level_v2,
	.get_current_level = dvfsrc_get_current_level_v2,
	.get_vcore_level = dvfsrc_get_vcore_level_v2,
	.get_vscp_level = dvfsrc_get_vscp_level_v2,
	.set_dram_bw = dvfsrc_set_dram_bw_v1,
	.set_dram_peak_bw = dvfsrc_set_dram_peak_bw_v1,
	.set_dram_hrt_bw = dvfsrc_set_dram_hrt_bw_v1,
	.set_vcore_level = dvfsrc_set_vcore_level_v2,
	.set_vscp_level = dvfsrc_set_vscp_level_v2,
	.wait_for_opp_level = dvfsrc_wait_for_opp_level_v2,
	.wait_for_vcore_level = dvfsrc_wait_for_vcore_level_v1,
	.bw_constraints = &dvfsrc_bw_constr_v2,
};

static const struct of_device_id mtk_dvfsrc_of_match[] = {
	{ .compatible = "mediatek,mt6768-dvfsrc", .data = &mt6768_data },
	{ .compatible = "mediatek,mt6893-dvfsrc", .data = &mt6893_data },
	{ .compatible = "mediatek,mt8183-dvfsrc", .data = &mt8183_data },
	{ .compatible = "mediatek,mt8195-dvfsrc", .data = &mt8195_data },
	{ /* sentinel */ }
};

static struct platform_driver mtk_dvfsrc_driver = {
	.probe	= mtk_dvfsrc_probe,
	.driver = {
		.name = "mtk-dvfsrc",
		.of_match_table = mtk_dvfsrc_of_match,
	},
};
module_platform_driver(mtk_dvfsrc_driver);

MODULE_AUTHOR("AngeloGioacchino Del Regno <angelogioacchino.delregno@collabora.com>");
MODULE_AUTHOR("Dawei Chien <dawei.chien@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek DVFSRC driver");
