// SPDX-License-Identifier: GPL-2.0-only

#include <dt-bindings/clock/mediatek,mt6768-clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include "clk-gate.h"
#include "clk-mtk.h"

#define GATE_PERI(_id, _name, _parent, _shift)			\
		GATE_MTK(_id, _name, _parent, &peri_cg_regs,	\
			 _shift, &mtk_clk_gate_ops_no_setclr_inv)


static const struct mtk_gate_regs peri_cg_regs = {
	.set_ofs = 0x20C,
	.clr_ofs = 0x20C,
	.sta_ofs = 0x20C,
};

static const struct mtk_gate peri_clks[] = {
	GATE_PERI(CLK_PERIAXI_DISABLE, "periaxi_disable", "axi_ck", 31),
};

static const struct mtk_clk_desc peri_desc = {
	.clks = peri_clks,
	.num_clks = ARRAY_SIZE(peri_clks),
};


static const struct of_device_id of_match_mt6768_pericfg[] = {
	{ .compatible = "mediatek,mt6768-pericfg", .data = &peri_desc },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, of_match_clk_mt6768_pericfg);

static struct platform_driver clk_mt6768_pericfg = {
	.probe = mtk_clk_simple_probe,
	.remove = mtk_clk_simple_remove,
	.driver = {
		.name = "clk-mt6768-pericfg",
		.of_match_table = of_match_mt6768_pericfg,
	},
};
module_platform_driver(clk_mt6768_pericfg);

MODULE_DESCRIPTION("MediaTek MT6768 pericfg clock driver");
MODULE_LICENSE("GPL");

