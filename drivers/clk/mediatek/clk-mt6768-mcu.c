// SPDX-License-Identifier: GPL-2.0

#include <dt-bindings/clock/mediatek,mt6768-clk.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"

static const char *const mcu_armpll_ll_parents[] = {
	"clk26m",
	"armpll",
	"arm_div_pll1_en",
	"arm_div_pll2_en"
};

static const char *const mcu_armpll_bl_parents[] = {
	"clk26m",
	"armpll_l",
	"arm_div_pll1_en",
	"arm_div_pll2_en"
};

static const char *const mcu_armpll_bus_parents[] = {
	"clk26m",
	"ccipll",
	"arm_div_pll1_en",
	"arm_div_pll2_en"
};

static struct mtk_composite mcu_muxes[] = {
	MUX(CLK_MCU_PLL_LL_SEL, "mcu_armpll_ll", mcu_armpll_ll_parents, 0x2a0, 9, 2),
	MUX(CLK_MCU_PLL_L_SEL, "mcu_armpll_bl", mcu_armpll_bl_parents, 0x2a4, 9, 2),
	MUX(CLK_MCU_PLL_BUS_SEL, "mcu_armpll_bus", mcu_armpll_bus_parents, 0x2e0, 9, 2),
};

static const struct mtk_clk_desc mcu_desc = {
	.composite_clks = mcu_muxes,
	.num_composite_clks = ARRAY_SIZE(mcu_muxes),
};

static const struct of_device_id of_match_clk_mt6768_mcu[] = {
	{ .compatible = "mediatek,mt6768-mcucfg", .data = &mcu_desc },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, of_match_clk_mt6768_mcu);

static struct platform_driver clk_mt6768_mcu_drv = {
	.probe = mtk_clk_simple_probe,
	.remove = mtk_clk_simple_remove,
	.driver = {
		.name = "clk-mt6768-mcu",
		.of_match_table = of_match_clk_mt6768_mcu,
	},
};
module_platform_driver(clk_mt6768_mcu_drv);

MODULE_DESCRIPTION("MediaTek MT6768 MicroController Unit clocks driver");
MODULE_LICENSE("GPL");

