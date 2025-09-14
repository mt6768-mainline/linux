// SPDX-License-Identifier: GPL-2.0

#include <linux/clk-provider.h>
#include <linux/platform_device.h>

#include "clk-gate.h"
#include "clk-mtk.h"
#include "clk-pll.h"

#include <dt-bindings/clock/mediatek,mt6768-clk.h>

static const struct mtk_gate_regs apmixed_cg_regs = {
	.set_ofs = 0x14,
	.clr_ofs = 0x14,
	.sta_ofs = 0x14,
};

#define GATE_APMIXED(_id, _name, _parent, _shift) \
	GATE_MTK(_id, _name, _parent, &apmixed_cg_regs, _shift, &mtk_clk_gate_ops_no_setclr_inv)

static const struct mtk_gate apmixed_clks[] = {
	GATE_APMIXED(CLK_APMIXED_SSUSB26M, "apmixed_ssusb26m", "f_f26m_ck", 4),
	GATE_APMIXED(CLK_APMIXED_APPLL26M, "apmixed_appll26m", "f_f26m_ck", 5),
	GATE_APMIXED(CLK_APMIXED_MIPIC0_26M, "apmixed_mipic026m", "f_f26m_ck", 6),
	GATE_APMIXED(CLK_APMIXED_MDPLLGP26M, "apmixed_mdpll26m", "f_f26m_ck", 7),
	GATE_APMIXED(CLK_APMIXED_MMSYS_F26M, "apmixed_mmsys26m", "f_f26m_ck", 8),
	GATE_APMIXED(CLK_APMIXED_UFS26M, "apmixed_ufs26m", "f_f26m_ck", 9),
	GATE_APMIXED(CLK_APMIXED_MIPIC1_26M, "apmixed_mipic126m", "f_f26m_ck", 11),
	GATE_APMIXED(CLK_APMIXED_MEMPLL26M, "apmixed_mempll26m", "f_f26m_ck", 13),
	GATE_APMIXED(CLK_APMIXED_CLKSQ_LVPLL_26M, "apmixed_lvpll26m", "f_f26m_ck", 14),
	GATE_APMIXED(CLK_APMIXED_MIPID0_26M, "apmixed_mipid026m", "f_f26m_ck", 16),
};

#define MT6768_PLL_FMAX		(3800UL * MHZ)
#define MT6768_PLL_FMIN		(1500UL * MHZ)
#define MT6768_INTEGER_BITS	8

#define CON0_MT6768_RST_BAR	BIT(23)
#define CON0_MT6768_EN_MASK	BIT(0)

#define PLL(_id, _name, _reg, _pwr_reg, _flags, _pcwbits, _pd_reg, _pd_shift, _tuner_reg,	\
	    _tuner_en_reg, _tuner_en_bit, _pcw_reg, _pcw_shift) {				\
		.id = _id,									\
		.name = _name,									\
		.reg = _reg,									\
		.pwr_reg = _pwr_reg,								\
		.en_mask = CON0_MT6768_EN_MASK,							\
		.flags = _flags,								\
		.rst_bar_mask = CON0_MT6768_RST_BAR,						\
		.fmax = MT6768_PLL_FMAX,							\
		.fmin = MT6768_PLL_FMIN,							\
		.pcwbits = _pcwbits,								\
		.pcwibits = MT6768_INTEGER_BITS,						\
		.pd_reg = _pd_reg,								\
		.pd_shift = _pd_shift,								\
		.tuner_reg = _tuner_reg,							\
		.tuner_en_reg = _tuner_en_reg,							\
		.tuner_en_bit = _tuner_en_bit,							\
		.pcw_reg = _pcw_reg,								\
		.pcw_shift = _pcw_shift,							\
	}

static const struct mtk_pll_data plls[] = {
	PLL(CLK_APMIXED_ARMPLL, "armpll", 0x0208, 0x0214,
	    PLL_AO, 22, 0x020C, 24, 0, 0, 0, 0x020C, 0),
	PLL(CLK_APMIXED_ARMPLL_L, "armpll_l", 0x0218, 0x0224,
	    PLL_AO, 22, 0x021C, 24, 0, 0, 0, 0x021C, 0),
	PLL(CLK_APMIXED_CCIPLL, "ccipll", 0x0228, 0x0234,
	    PLL_AO, 22, 0x022C, 24, 0, 0, 0, 0x022C, 0),
	PLL(CLK_APMIXED_UNIV2PLL, "univ2pll", 0x0238, 0x0244,
	    HAVE_RST_BAR, 22, 0x023C, 24, 0, 0, 0, 0x023C, 0),
	PLL(CLK_APMIXED_MFGPLL, "mfgpll", 0x0248, 0x0254,
	    HAVE_RST_BAR, 22, 0x024C, 24, 0, 0, 0, 0x024C, 0),

	PLL(CLK_APMIXED_MAINPLL, "mainpll", 0x0258, 0x0264,
	    PLL_AO, 22, 0x025C, 24, 0, 0, 0, 0x025C, 0),
	/* APLL pcw is at 0x310, postdiv at 0x30c. Also, it has turner regs.*/
	PLL(CLK_APMIXED_APLL1, "apll1", 0x0308, 0x0318,
	    HAVE_RST_BAR, 32, 0x030C, 24, 0x0040, 0x000C, 0, 0x0310, 0),
	PLL(CLK_APMIXED_MMPLL, "mmpll", 0x031C, 0x0328,
	    HAVE_RST_BAR, 22, 0x0320, 24, 0, 0, 0, 0x0320, 0),
	PLL(CLK_APMIXED_MPLL, "mpll", 0x032C, 0x0338,
	    PLL_AO, 22, 0x0330, 24, 0, 0, 0, 0x0330, 0),
	PLL(CLK_APMIXED_MSDCPLL, "msdcpll", 0x033C, 0x0348,
	    HAVE_RST_BAR, 22, 0x0320, 24, 0, 0, 0, 0x0340, 0),
};

static int clk_mt6768_apmixed_probe(struct platform_device *pdev)
{
  void __iomem *base;
	struct clk_hw_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	int r;

	clk_data = mtk_alloc_clk_data(CLK_APMIXED_NR_CLK);
	if (!clk_data)
		return -ENOMEM;
	platform_set_drvdata(pdev, clk_data);

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	r = mtk_clk_register_plls(node, plls, ARRAY_SIZE(plls), clk_data);
	if (r)
		goto free_apmixed_data;

	r = mtk_clk_register_gates(&pdev->dev, node, apmixed_clks, ARRAY_SIZE(apmixed_clks), clk_data);
	if (r)
		goto unregister_plls;

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);
	if (r)
		goto unregister_gates;

  /* MPLL, ARM/CCIPLLs, MAINPLL set to HW mode, TDCLKSQ, CLKSQ1 */
  #define AP_PLL_CON3 (base + 0x0c)
  #define PLLON_CON0  (base + 0x44)
  #define PLLON_CON1  (base + 0x48)

  writel(readl(AP_PLL_CON3) & 0xFFFFFFE1, AP_PLL_CON3);
	writel(readl(PLLON_CON0) & 0x01041041, PLLON_CON0);
	writel(readl(PLLON_CON1) & 0x01041041, PLLON_CON1);

	return 0;

unregister_gates:
	mtk_clk_unregister_gates(apmixed_clks, ARRAY_SIZE(apmixed_clks), clk_data);
unregister_plls:
	mtk_clk_unregister_plls(plls, ARRAY_SIZE(plls), clk_data);
free_apmixed_data:
	mtk_free_clk_data(clk_data);
	return r;
}

static void clk_mt6768_apmixed_remove(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data = platform_get_drvdata(pdev);

	mtk_clk_unregister_plls(plls, ARRAY_SIZE(plls), clk_data);
	mtk_clk_unregister_gates(apmixed_clks, ARRAY_SIZE(apmixed_clks), clk_data);
}

static const struct of_device_id of_match_mt6768_apmixedsys[] = {
	{ .compatible = "mediatek,mt6768-apmixedsys" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, of_match_mt6768_apmixedsys);

static struct platform_driver clk_mt6768_apmixedsys = {
	.probe = clk_mt6768_apmixed_probe,
	.remove = clk_mt6768_apmixed_remove,
	.driver = {
		.name = "clk-mt6768-apmixedsys",
		.of_match_table = of_match_mt6768_apmixedsys,
	},
};
module_platform_driver(clk_mt6768_apmixedsys);

MODULE_DESCRIPTION("MediaTek MT6768 apmixedsys clock driver");
MODULE_LICENSE("GPL");

