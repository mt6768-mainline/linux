// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Copyright (c) 2025-2026 Roman Vivchar <rva333@protonmail.com>
 */
#include <dt-bindings/clock/mediatek,mt6768-clock.h>

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>

#include "clk-mtk.h"
#include "clk-gate.h"
#include "clk-pll.h"

#define MT6768_PLL_FMAX		(3800UL * MHZ)
#define MT6768_PLL_FMIN		(1500UL * MHZ)

#define AP_PLL_CON3		0x0c
#define PLLON_CON0		0x44
#define PLLON_CON1		0x48

#define PLL_RAW(_id, _name, _reg, _pwr_reg, _en_mask, _flags, _pcwbits,	\
		_pcwibits, _pd_reg, _pd_shift, _tuner_reg, _tuner_en_reg,\
		_tuner_en_bit, _pcw_reg, _pcw_shift, _fmin, _fmax,	\
		_div_table) {						\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.pwr_reg = _pwr_reg,					\
		.en_mask = _en_mask,					\
		.flags = _flags,					\
		.rst_bar_mask = BIT(23),				\
		.fmax = _fmax,						\
		.fmin = _fmin,						\
		.pcwbits = _pcwbits,					\
		.pcwibits = _pcwibits,					\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,				\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.div_table = _div_table,				\
	}

#define PLL(_id, _name, _reg, _pwr_reg, _en_mask, _flags, _pcwbits,	\
			_pcwibits, _pd_reg, _pd_shift, _tuner_reg,	\
			_tuner_en_reg, _tuner_en_bit, _pcw_reg,		\
			_pcw_shift, _fmin, _fmax, _div_table)		\
		PLL_RAW(_id, _name, _reg, _pwr_reg, _en_mask, _flags,	\
			_pcwbits, _pcwibits, _pd_reg, _pd_shift,	\
			_tuner_reg, _tuner_en_reg, _tuner_en_bit,	\
			_pcw_reg, _pcw_shift, _fmin, _fmax, _div_table)	\

/* Educated guess */
static const struct mtk_pll_div_table armpll_div_table[] = {
	{ .div = 0, .freq = MT6768_PLL_FMAX },
	{ .div = 1, .freq = 1700 * MHZ },
	{ .div = 2, .freq = 850 * MHZ },
	{ .div = 3, .freq = 375 * MHZ },
	{ .div = 4, .freq = 187 * MHZ },
	{ /* sentinel */ }
};

/* Educated guess */
static const struct mtk_pll_div_table ccipll_div_table[] = {
	{ .div = 0, .freq = MT6768_PLL_FMAX },
	{ .div = 1, .freq = 1400 * MHZ },
	{ .div = 2, .freq = 750 * MHZ },
	{ .div = 3, .freq = 375 * MHZ },
	{ .div = 4, .freq = 187 * MHZ },
	{ /* sentinel */ }
};

static const struct mtk_pll_div_table mfgpll_div_table[] = {
	{ .div = 0, .freq = MT6768_PLL_FMAX },
	{ .div = 1, .freq = 1900 * MHZ },
	{ .div = 2, .freq = 950 * MHZ },
	{ .div = 3, .freq = 475 * MHZ },
	{ .div = 4, .freq = 237 * MHZ },
	{ /* sentinel */ }
};

static const struct mtk_pll_data plls[] = {
	PLL(CLK_APMIXED_ARMPLL, "armpll", 0x0208, 0x0214, BIT(0),
	    PLL_AO, 22, 8, 0x020C, 24, 0, 0, 0, 0x020C, 0, MT6768_PLL_FMIN,
	    MT6768_PLL_FMAX, armpll_div_table),
	PLL(CLK_APMIXED_ARMPLL_L, "armpll_l", 0x0218, 0x0224, BIT(0),
	    PLL_AO, 22, 8, 0x021C, 24, 0, 0, 0, 0x021C, 0, MT6768_PLL_FMIN,
	    MT6768_PLL_FMAX, armpll_div_table),
	PLL(CLK_APMIXED_CCIPLL, "ccipll", 0x0228, 0x0234, BIT(0),
	    PLL_AO, 22, 8, 0x022C, 24, 0, 0, 0, 0x022C, 0, MT6768_PLL_FMIN,
	    MT6768_PLL_FMAX, ccipll_div_table),
	PLL(CLK_APMIXED_UNIV2PLL, "univ2pll", 0x0238, 0x0244, BIT(0),
	    HAVE_RST_BAR, 22, 8, 0x023C, 24, 0, 0, 0, 0x023C, 0,
	    2496UL * MHZ, 2496UL * MHZ, NULL),
	PLL(CLK_APMIXED_MFGPLL, "mfgpll", 0x0248, 0x0254, BIT(0),
	    0, 22, 8, 0x024C, 24, 0, 0, 0, 0x024C, 0, MT6768_PLL_FMIN,
	    MT6768_PLL_FMAX, mfgpll_div_table),
	PLL(CLK_APMIXED_MAINPLL, "mainpll", 0x0258, 0x0264, BIT(0),
	    HAVE_RST_BAR | PLL_AO, 22, 8, 0x025C, 24, 0, 0, 0, 0x025C,
	    0, 1092 * MHZ, 1092 * MHZ, NULL),
	PLL(CLK_APMIXED_APLL1, "apll1", 0x0308, 0x0318, BIT(0),
	    0, 32, 8, 0x030C, 24, 0x0040, 0x000C, 0, 0x0310, 0,
	    196608000, 196608000, NULL),
	PLL(CLK_APMIXED_MMPLL, "mmpll", 0x031C, 0x0328, BIT(0),
	    0, 22, 8, 0x0320, 24, 0, 0, 0, 0x0320, 0, MT6768_PLL_FMIN,
	    MT6768_PLL_FMAX, NULL),
	PLL(CLK_APMIXED_MPLL, "mpll", 0x032C, 0x0338, BIT(0),
	    PLL_AO, 22, 8, 0x0330, 24, 0, 0, 0, 0x0330, 0,
	    208 * MHZ, 208 * MHZ, NULL),
	PLL(CLK_APMIXED_MSDCPLL, "msdcpll", 0x033C, 0x0348, BIT(0),
	    0, 22, 8, 0x0340, 24, 0, 0, 0, 0x0340, 0, MT6768_PLL_FMIN,
	    MT6768_PLL_FMAX, NULL),
};

static const struct mtk_gate_regs apmixed_cg_regs = {
	.set_ofs = 0x14,
	.clr_ofs = 0x14,
	.sta_ofs = 0x14,
};

#define GATE_APMIXED(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &apmixed_cg_regs, _shift, &mtk_clk_gate_ops_no_setclr_inv)

static const struct mtk_gate apmixed_gates[] = {
	GATE_APMIXED(CLK_APMIXED_SSUSB26M, "apmixed_ssusb26m", "clk26m", 4),
	GATE_APMIXED(CLK_APMIXED_APPLL26M, "apmixed_appll26m", "clk26m", 5),
	GATE_APMIXED(CLK_APMIXED_MIPIC0_26M, "apmixed_mipic026m", "clk26m", 6),
	GATE_APMIXED(CLK_APMIXED_MDPLLGP26M, "apmixed_mdpll26m", "clk26m", 7),
	GATE_APMIXED(CLK_APMIXED_MMSYS_F26M, "apmixed_mmsys26m", "clk26m", 8),
	GATE_APMIXED(CLK_APMIXED_UFS26M, "apmixed_ufs26m", "clk26m", 9),
	GATE_APMIXED(CLK_APMIXED_MIPIC1_26M, "apmixed_mipic126m", "clk26m", 11),
	GATE_APMIXED(CLK_APMIXED_MEMPLL26M, "apmixed_mempll26m", "clk26m", 13),
	GATE_APMIXED(CLK_APMIXED_CLKSQ_LVPLL_26M, "apmixed_lvpll26m", "clk26m", 14),
	GATE_APMIXED(CLK_APMIXED_MIPID0_26M, "apmixed_mipid026m", "clk26m", 16),
};

static int clk_mt6768_apmixed_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base)
		return dev_err_probe(&pdev->dev, -ENOENT,
				     "failed to get base address\n");

	clk_data = mtk_alloc_clk_data(ARRAY_SIZE(plls) + ARRAY_SIZE(apmixed_gates));
	if (!clk_data)
		return -ENOMEM;

	r = mtk_clk_register_plls(&pdev->dev, plls, ARRAY_SIZE(plls), clk_data);
	if (r)
		goto free_apmixed_data;

	r = mtk_clk_register_gates(&pdev->dev, node, apmixed_gates,
				   ARRAY_SIZE(apmixed_gates), clk_data);
	if (r)
		goto unregister_plls;

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);
	if (r)
		goto unregister_gates;

	platform_set_drvdata(pdev, clk_data);

	/* MPLL, ARM/CCIPLLs, MAINPLL set to HW mode, TDCLKSQ, CLKSQ1 */
	writel(readl(base + AP_PLL_CON3) & 0xFFFFFFE1, base + AP_PLL_CON3);
	writel(readl(base + PLLON_CON0) & 0x01041041, base + PLLON_CON0);
	writel(readl(base + PLLON_CON1) & 0x01041041, base + PLLON_CON1);

	return 0;

unregister_gates:
	mtk_clk_unregister_gates(apmixed_gates, ARRAY_SIZE(apmixed_gates), clk_data);
unregister_plls:
	mtk_clk_unregister_plls(plls, ARRAY_SIZE(plls), clk_data);
free_apmixed_data:
	mtk_free_clk_data(clk_data);
	return r;
}

static void clk_mt6768_apmixed_remove(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data = platform_get_drvdata(pdev);
	struct device_node *node = pdev->dev.of_node;

	of_clk_del_provider(node);
	mtk_clk_unregister_gates(apmixed_gates, ARRAY_SIZE(apmixed_gates), clk_data);
	mtk_clk_unregister_plls(plls, ARRAY_SIZE(plls), clk_data);
	mtk_free_clk_data(clk_data);
}

static const struct of_device_id of_match_clk_mt6768_apmixed[] = {
	{ .compatible = "mediatek,mt6768-apmixedsys" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, of_match_clk_mt6768_apmixed);

static struct platform_driver clk_mt6768_apmixed_drv = {
	.probe = clk_mt6768_apmixed_probe,
	.remove = clk_mt6768_apmixed_remove,
	.driver = {
		.name = "clk-mt6768-apmixed",
		.of_match_table = of_match_clk_mt6768_apmixed,
	},
};
module_platform_driver(clk_mt6768_apmixed_drv);

MODULE_DESCRIPTION("MediaTek MT6768 apmixedsys clocks driver");
MODULE_LICENSE("GPL");

