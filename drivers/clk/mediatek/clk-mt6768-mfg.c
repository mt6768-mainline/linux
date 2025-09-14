// SPDX-License-Identifier: GPL-2.0-only

#include <dt-bindings/clock/mediatek,mt6768-clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include "clk-gate.h"
#include "clk-mtk.h"

static const struct mtk_gate_regs mfgcfg_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_MFGCFG(_id, _name, _parent, _shift) \
	GATE_MTK(_id, _name, _parent, &mfgcfg_cg_regs, _shift, &mtk_clk_gate_ops_setclr)

static const struct mtk_gate mfgcfg_clks[] = {
	GATE_MFGCFG(CLK_MFGCFG_BG3D, "mfgcfg_bg3d", "mfg_ck", 0),
};

static const struct mtk_clk_desc mfg_desc = {
	.clks = mfgcfg_clks,
	.num_clks = ARRAY_SIZE(mfgcfg_clks),
};

static const struct of_device_id of_match_clk_mt6768_mfg[] = {
	{ .compatible = "mediatek,mt6768-mfgcfg", .data = &mfg_desc },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, of_match_clk_mt6768_mfg);

static struct platform_driver clk_mt6768_mfg = {
	.driver = {
		.name = "clk-mt6768-mfg",
		.of_match_table = of_match_clk_mt6768_mfg,
	},
	.probe = mtk_clk_simple_probe,
	.remove = mtk_clk_simple_remove,
};
module_platform_driver(clk_mt6768_mfg);

MODULE_DESCRIPTION("MediaTek MT6768 mfg clocks driver");
MODULE_LICENSE("GPL");

