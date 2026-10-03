// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Copyright (c) 2025-2026 Roman Vivchar <rva333@protonmail.com>
 */

#include <linux/clk-provider.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mediatek,mt6768-clock.h>

static const struct mtk_gate_regs vdec0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vdec1_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xc,
	.sta_ofs = 0x8,
};

#define GATE_VDEC0(_id, _name, _parent, _shift)               \
	GATE_MTK(_id, _name, _parent, &vdec0_cg_regs, _shift, \
		 &mtk_clk_gate_ops_setclr_inv)

#define GATE_VDEC1(_id, _name, _parent, _shift)               \
	GATE_MTK(_id, _name, _parent, &vdec1_cg_regs, _shift, \
		 &mtk_clk_gate_ops_setclr_inv)

static const struct mtk_gate vdec_clks[] = {
	GATE_VDEC0(CLK_VDEC_VDEC, "vdec_vdec", "mm_sel", 0),
	GATE_VDEC0(CLK_VDEC_ACTIVE, "vdec_active", "mm_sel", 4),
	GATE_VDEC0(CLK_VDEC_EN_ENG, "vdec_en_eng", "mm_sel", 8),
	GATE_VDEC1(CLK_VDEC_LARB1, "vdec_larb1", "mm_sel", 0),
};

static const struct mtk_clk_desc vdec_desc = {
	.clks = vdec_clks,
	.num_clks = ARRAY_SIZE(vdec_clks),
};

static const struct of_device_id of_match_clk_mt6768_vdec[] = {
	{ .compatible = "mediatek,mt6768-vdecsys", .data = &vdec_desc },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, of_match_clk_mt6768_vdec);

static struct platform_driver clk_mt6768_vdec_drv = {
	.probe = mtk_clk_simple_probe,
	.remove = mtk_clk_simple_remove,
	.driver = {
		.name = "clk-mt6768-vdec",
		.of_match_table = of_match_clk_mt6768_vdec,
	},
};
module_platform_driver(clk_mt6768_vdec_drv);

MODULE_DESCRIPTION("MediaTek MT6768 Video Decoders clocks driver");
MODULE_LICENSE("GPL");
