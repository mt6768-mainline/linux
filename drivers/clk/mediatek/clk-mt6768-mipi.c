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

static const struct mtk_gate_regs mipi_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x80,
	.sta_ofs = 0x80,
};

#define GATE_MIPI(_id, _name, _parent, _shift)               \
	GATE_MTK(_id, _name, _parent, &mipi_cg_regs, _shift, \
		 &mtk_clk_gate_ops_no_setclr_inv)

static const struct mtk_gate mipi0a_clks[] = {
	GATE_MIPI(CLK_MIPI0A_CSR_CSI_EN_0A, "mipi0a_csr_0a", "seninf_sel", 1),
};

static const struct mtk_clk_desc mipi0a_desc = {
	.clks = mipi0a_clks,
	.num_clks = ARRAY_SIZE(mipi0a_clks),
};

static const struct mtk_gate mipi0b_clks[] = {
	GATE_MIPI(CLK_MIPI0B_CSR_CSI_EN_0B, "mipi0b_csr_0b", "seninf_sel", 1),
};

static const struct mtk_clk_desc mipi0b_desc = {
	.clks = mipi0b_clks,
	.num_clks = ARRAY_SIZE(mipi0b_clks),
};

static const struct mtk_gate mipi1a_clks[] = {
	GATE_MIPI(CLK_MIPI1A_CSR_CSI_EN_1A, "mipi1a_csr_1a", "seninf_sel", 1),
};

static const struct mtk_clk_desc mipi1a_desc = {
	.clks = mipi1a_clks,
	.num_clks = ARRAY_SIZE(mipi1a_clks),
};

static const struct mtk_gate mipi1b_clks[] = {
	GATE_MIPI(CLK_MIPI1B_CSR_CSI_EN_1B, "mipi1b_csr_1b", "seninf_sel", 1),
};

static const struct mtk_clk_desc mipi1b_desc = {
	.clks = mipi1b_clks,
	.num_clks = ARRAY_SIZE(mipi1b_clks),
};

static const struct mtk_gate mipi2a_clks[] = {
	GATE_MIPI(CLK_MIPI2A_CSR_CSI_EN_2A, "mipi2a_csr_2a", "seninf_sel", 1),
};

static const struct mtk_clk_desc mipi2a_desc = {
	.clks = mipi2a_clks,
	.num_clks = ARRAY_SIZE(mipi2a_clks),
};

static const struct mtk_gate mipi2b_clks[] = {
	GATE_MIPI(CLK_MIPI2B_CSR_CSI_EN_2B, "mipi2b_csr_2b", "seninf_sel", 1),
};

static const struct mtk_clk_desc mipi2b_desc = {
	.clks = mipi2b_clks,
	.num_clks = ARRAY_SIZE(mipi2b_clks),
};

static const struct of_device_id of_match_clk_mt6768_mipi[] = {
	{ .compatible = "mediatek,mt6768-mipi0a", .data = &mipi0a_desc },
	{ .compatible = "mediatek,mt6768-mipi0b", .data = &mipi0b_desc },
	{ .compatible = "mediatek,mt6768-mipi1a", .data = &mipi1a_desc },
	{ .compatible = "mediatek,mt6768-mipi1b", .data = &mipi1b_desc },
	{ .compatible = "mediatek,mt6768-mipi2a", .data = &mipi2a_desc },
	{ .compatible = "mediatek,mt6768-mipi2b", .data = &mipi2b_desc },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, of_match_clk_mt6768_mipi);

static struct platform_driver clk_mt6768_mipi0a_drv = {
	.probe = mtk_clk_simple_probe,
	.remove = mtk_clk_simple_remove,
	.driver = {
		.name = "clk-mt6768-mipi",
		.of_match_table = of_match_clk_mt6768_mipi,
	},
};
module_platform_driver(clk_mt6768_mipi0a_drv);

MODULE_DESCRIPTION("MediaTek MT6768 MIPI RX clocks driver");
MODULE_LICENSE("GPL");

