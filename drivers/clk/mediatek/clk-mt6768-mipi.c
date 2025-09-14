// SPDX-License-Identifier: GPL-2.0

#include <linux/clk-provider.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mediatek,mt6768-clk.h>

static const struct mtk_gate_regs mipi0a_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x80,
	.sta_ofs = 0x80,
};

#define GATE_MIPI0A(_id, _name, _parent, _shift) \
	GATE_MTK(_id, _name, _parent, &mipi0a_cg_regs, _shift, &mtk_clk_gate_ops_no_setclr_inv)

static const struct mtk_gate mipi0a_clks[] = {
	GATE_MIPI0A(CLK_MIPI0A_CSR_CSI_EN_0A, "mipi0a_csr_0a", "f_fseninf_ck", 1),
};

static const struct mtk_gate_regs mipi0b_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x80,
	.sta_ofs = 0x80,
};

#define GATE_MIPI0B(_id, _name, _parent, _shift) \
	GATE_MTK(_id, _name, _parent, &mipi0b_cg_regs, _shift, &mtk_clk_gate_ops_no_setclr_inv)

static const struct mtk_gate mipi0b_clks[] = {
	GATE_MIPI0B(CLK_MIPI0B_CSR_CSI_EN_0B, "mipi0b_csr_0b", "f_fseninf_ck", 1),
};

static const struct mtk_gate_regs mipi1a_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x80,
	.sta_ofs = 0x80,
};

#define GATE_MIPI1A(_id, _name, _parent, _shift) \
	GATE_MTK(_id, _name, _parent, &mipi1a_cg_regs, _shift, &mtk_clk_gate_ops_no_setclr_inv)

static const struct mtk_gate mipi1a_clks[] = {
	GATE_MIPI1A(CLK_MIPI1A_CSR_CSI_EN_1A, "mipi1a_csr_1a", "f_fseninf_ck", 1),
};

static const struct mtk_gate_regs mipi1b_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x80,
	.sta_ofs = 0x80,
};

#define GATE_MIPI1B(_id, _name, _parent, _shift) \
	GATE_MTK(_id, _name, _parent, &mipi1b_cg_regs, _shift, &mtk_clk_gate_ops_no_setclr_inv)

static const struct mtk_gate mipi1b_clks[] = {
	GATE_MIPI1B(CLK_MIPI1B_CSR_CSI_EN_1B, "mipi1b_csr_1b", "f_fseninf_ck", 1),
};

static const struct mtk_gate_regs mipi2a_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x80,
	.sta_ofs = 0x80,
};

#define GATE_MIPI2A(_id, _name, _parent, _shift) \
	GATE_MTK(_id, _name, _parent, &mipi2a_cg_regs, _shift, &mtk_clk_gate_ops_no_setclr_inv)

static const struct mtk_gate mipi2a_clks[] = {
	GATE_MIPI2A(CLK_MIPI2A_CSR_CSI_EN_2A, "mipi2a_csr_2a", "f_fseninf_ck", 1),
};

static const struct mtk_gate_regs mipi2b_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x80,
	.sta_ofs = 0x80,
};

#define GATE_MIPI2B(_id, _name, _parent, _shift) \
	GATE_MTK(_id, _name, _parent, &mipi2b_cg_regs, _shift, &mtk_clk_gate_ops_no_setclr_inv)

static const struct mtk_gate mipi2b_clks[] = {
	GATE_MIPI2B(CLK_MIPI2B_CSR_CSI_EN_2B, "mipi2b_csr_2b", "f_fseninf_ck", 1),
};

static const struct mtk_clk_desc mipi0a_desc = {
	.clks = mipi0a_clks,
	.num_clks = ARRAY_SIZE(mipi0a_clks),
};

static const struct mtk_clk_desc mipi0b_desc = {
	.clks = mipi0b_clks,
	.num_clks = ARRAY_SIZE(mipi0b_clks),
};

static const struct mtk_clk_desc mipi1a_desc = {
	.clks = mipi1a_clks,
	.num_clks = ARRAY_SIZE(mipi1a_clks),
};

static const struct mtk_clk_desc mipi1b_desc = {
	.clks = mipi1b_clks,
	.num_clks = ARRAY_SIZE(mipi1b_clks),
};

static const struct mtk_clk_desc mipi2a_desc = {
	.clks = mipi2a_clks,
	.num_clks = ARRAY_SIZE(mipi2a_clks),
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

static struct platform_driver clk_mt6768_mipi = {
	.probe = mtk_clk_simple_probe,
	.remove = mtk_clk_simple_remove,
	.driver = {
		.name = "clk-mt6768-mipi",
		.of_match_table = of_match_clk_mt6768_mipi,
	},
};
module_platform_driver(clk_mt6768_mipi);

MODULE_DESCRIPTION("MediaTek MT6768 Camera clocks driver");
MODULE_LICENSE("GPL");

