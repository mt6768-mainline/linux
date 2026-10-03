// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Copyright (c) 2025-2026 Roman Vivchar <rva333@protonmail.com>
 */

#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include "clk-mtk.h"
#include "clk-gate.h"
#include "reset.h"
#include <dt-bindings/clock/mediatek,mt6768-clock.h>
#include <dt-bindings/reset/mediatek,mt6768-resets.h>

#define PERI_BUS_DCM_CTRL	0x074
#define INFRA_CG1		BIT(2)

#define INFRA_TOPAXI_SI0_CTL	0x200
#define INFRA_CG0		BIT(31)

static const struct mtk_gate_regs infra2_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x84,
	.sta_ofs = 0x90,
};

static const struct mtk_gate_regs infra3_cg_regs = {
	.set_ofs = 0x88,
	.clr_ofs = 0x8c,
	.sta_ofs = 0x94,
};

static const struct mtk_gate_regs infra4_cg_regs = {
	.set_ofs = 0xa4,
	.clr_ofs = 0xa8,
	.sta_ofs = 0xac,
};

static const struct mtk_gate_regs infra5_cg_regs = {
	.set_ofs = 0xc0,
	.clr_ofs = 0xc4,
	.sta_ofs = 0xc8,
};

#define GATE_INFRA2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra2_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_INFRA3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra3_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}


#define GATE_INFRA4(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra4_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}


#define GATE_INFRA5(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra5_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate infra_gates[] = {
	GATE_INFRA2(CLK_INFRA_PMIC_TMR, "infra_pmic_tmr", "clk26m", 0),
	GATE_INFRA2(CLK_INFRA_PMIC_AP, "infra_pmic_ap", "clk26m", 1),
	GATE_INFRA2(CLK_INFRA_PMIC_MD, "infra_pmic_md", "clk26m", 2),
	GATE_INFRA2(CLK_INFRA_PMIC_CONN, "infra_pmic_conn", "clk26m", 3),
	GATE_INFRA2(CLK_INFRA_SCP_CORE, "infra_scp_core", "scp_sel", 4),
	GATE_INFRA2(CLK_INFRA_SEJ, "infra_sej", "axi_sel", 5),
	GATE_INFRA2(CLK_INFRA_APXGPT, "infra_apxgpt", "axi_sel", 6),
	GATE_INFRA2(CLK_INFRA_ICUSB, "infra_icusb", "axi_sel", 8),
	GATE_INFRA2(CLK_INFRA_GCE, "infra_gce", "axi_sel", 9),
	GATE_INFRA2(CLK_INFRA_THERM, "infra_therm", "axi_sel", 10),
	GATE_INFRA2(CLK_INFRA_I2C_AP, "infra_i2c_ap", "i2c_sel", 11),
	GATE_INFRA2(CLK_INFRA_I2C_CCU, "infra_i2c_ccu", "i2c_sel", 12),
	GATE_INFRA2(CLK_INFRA_I2C_SSPM, "infra_i2c_sspm", "i2c_sel", 13),
	GATE_INFRA2(CLK_INFRA_I2C_RSV, "infra_i2c_rsv", "i2c_sel", 14),
	GATE_INFRA2(CLK_INFRA_PWM_HCLK, "infra_pwm_hclk", "axi_sel", 15),
	GATE_INFRA2(CLK_INFRA_PWM1, "infra_pwm1", "pwm_sel", 16),
	GATE_INFRA2(CLK_INFRA_PWM2, "infra_pwm2", "pwm_sel", 17),
	GATE_INFRA2(CLK_INFRA_PWM3, "infra_pwm3", "pwm_sel", 18),
	GATE_INFRA2(CLK_INFRA_PWM4, "infra_pwm4", "pwm_sel", 19),
	GATE_INFRA2(CLK_INFRA_PWM5, "infra_pwm5", "pwm_sel", 20),
	GATE_INFRA2(CLK_INFRA_PWM, "infra_pwm", "pwm_sel", 21),
	GATE_INFRA2(CLK_INFRA_UART0, "infra_uart0", "uart_sel", 22),
	GATE_INFRA2(CLK_INFRA_UART1, "infra_uart1", "uart_sel", 23),
	GATE_INFRA2(CLK_INFRA_GCE_26M, "infra_gce_26m", "clk26m", 27),
	GATE_INFRA2(CLK_INFRA_CQ_DMA_FPC, "infra_dma", "axi_sel", 28),
	GATE_INFRA2(CLK_INFRA_BTIF, "infra_btif", "axi_sel", 31),

	/* INFRA mode 1 */
	GATE_INFRA3(CLK_INFRA_SPI0, "infra_spi0", "spi_sel", 1),
	GATE_INFRA3(CLK_INFRA_MSDC0, "infra_msdc0", "msdc5hclk_sel", 2),
	GATE_INFRA3(CLK_INFRA_MSDC1, "infra_msdc1", "axi_sel", 4),
	GATE_INFRA3(CLK_INFRA_DVFSRC, "infra_dvfsrc", "clk26m", 7),
	GATE_INFRA3(CLK_INFRA_GCPU, "infra_gcpu", "axi_sel", 8),
	GATE_INFRA3(CLK_INFRA_TRNG, "infra_trng", "axi_sel", 9),
	GATE_INFRA3(CLK_INFRA_AUXADC, "infra_auxadc", "clk26m", 10),
	GATE_INFRA3(CLK_INFRA_CPUM, "infra_cpum", "axi_sel", 11),
	GATE_INFRA3(CLK_INFRA_CCIF1_AP, "infra_ccif1_ap", "axi_sel", 12),
	GATE_INFRA3(CLK_INFRA_CCIF1_MD, "infra_ccif1_md", "axi_sel", 13),
	GATE_INFRA3(CLK_INFRA_AUXADC_MD, "infra_auxadc_md", "clk26m", 14),
	GATE_INFRA3(CLK_INFRA_AP_DMA, "infra_ap_dma", "axi_sel", 18),
	GATE_INFRA3(CLK_INFRA_XIU, "infra_xiu", "axi_sel", 19),
	GATE_INFRA3(CLK_INFRA_DEVICE_APC, "infra_dapc", "axi_sel", 20),
	GATE_INFRA3(CLK_INFRA_CCIF_AP, "infra_ccif_ap", "axi_sel", 23),
	GATE_INFRA3(CLK_INFRA_DEBUGTOP, "infra_debugtop", "axi_sel", 24),
	GATE_INFRA3(CLK_INFRA_AUDIO, "infra_audio", "axi_sel", 25),
	GATE_INFRA3(CLK_INFRA_CCIF_MD, "infra_ccif_md", "axi_sel", 26),
	GATE_INFRA3(CLK_INFRA_DXCC_SEC_CORE, "infra_secore", "dxcc_sel", 27),
	GATE_INFRA3(CLK_INFRA_DXCC_AO, "infra_dxcc_ao", "dxcc_sel", 28),
	GATE_INFRA3(CLK_INFRA_DRAMC_F26M, "infra_dramc26", "clk26m", 31),

	GATE_INFRA4(CLK_INFRA_RG_PWM_FBCLK6, "infra_pwmfb", "clk26m", 0),
	GATE_INFRA4(CLK_INFRA_DISP_PWM, "infra_disp_pwm", "disp_pwm_sel", 2),
	GATE_INFRA4(CLK_INFRA_CLDMA_BCLK, "infra_cldmabclk", "axi_sel", 3),
	GATE_INFRA4(CLK_INFRA_AUDIO_26M_BCLK, "infra_audio26m", "clk26m", 4),
	GATE_INFRA4(CLK_INFRA_SPI1, "infra_spi1", "spi_sel", 6),
	GATE_INFRA4(CLK_INFRA_I2C4, "infra_i2c4", "i2c_sel", 7),
	GATE_INFRA4(CLK_INFRA_MODEM_TEMP_SHARE, "infra_mdtemp", "clk26m", 8),
	GATE_INFRA4(CLK_INFRA_SPI2, "infra_spi2", "spi_sel", 9),
	GATE_INFRA4(CLK_INFRA_SPI3, "infra_spi3", "spi_sel", 10),
	GATE_INFRA4(CLK_INFRA_SSPM, "infra_hf_fsspm", "sspm_sel", 15),
	GATE_INFRA4(CLK_INFRA_I2C5, "infra_i2c5", "i2c_sel", 18),
	GATE_INFRA4(CLK_INFRA_I2C5_ARBITER, "infra_i2c5a", "i2c_sel", 19),
	GATE_INFRA4(CLK_INFRA_I2C5_IMM, "infra_i2c5_imm", "i2c_sel", 20),
	GATE_INFRA4(CLK_INFRA_I2C1_ARBITER, "infra_i2c1a", "i2c_sel", 21),
	GATE_INFRA4(CLK_INFRA_I2C1_IMM, "infra_i2c1_imm", "i2c_sel", 22),
	GATE_INFRA4(CLK_INFRA_I2C2_ARBITER, "infra_i2c2a", "i2c_sel", 23),
	GATE_INFRA4(CLK_INFRA_I2C2_IMM, "infra_i2c2_imm", "i2c_sel", 24),
	GATE_INFRA4(CLK_INFRA_SPI4, "infra_spi4", "spi_sel", 25),
	GATE_INFRA4(CLK_INFRA_SPI5, "infra_spi5", "spi_sel", 26),
	GATE_INFRA4(CLK_INFRA_CQ_DMA, "infra_cq_dma", "axi_sel", 27),
	GATE_INFRA4(CLK_INFRA_FAES_FDE, "infra_faes_fde", "aes_fde_sel", 29),

	GATE_INFRA5(CLK_INFRA_MSDC0_SELF, "infra_msdc0sf", "msdc50_0_sel", 0),
	GATE_INFRA5(CLK_INFRA_MSDC1_SELF, "infra_msdc1sf", "msdc50_0_sel", 1),
	GATE_INFRA5(CLK_INFRA_SSPM_26M_SELF, "infra_sspm_26m", "clk26m", 3),
	GATE_INFRA5(CLK_INFRA_SSPM_32K_SELF, "infra_sspm_32k", "clk32k", 4),
	GATE_INFRA5(CLK_INFRA_I2C6, "infra_i2c6", "i2c_sel", 6),
	GATE_INFRA5(CLK_INFRA_AP_MSDC0, "infra_ap_msdc0", "msdc50_0_sel", 7),
	GATE_INFRA5(CLK_INFRA_MD_MSDC0, "infra_md_msdc0", "msdc50_0_sel", 8),
	GATE_INFRA5(CLK_INFRA_MSDC0_SRC, "infra_msdc0_clk", "msdc50_0_sel", 9),
	GATE_INFRA5(CLK_INFRA_MSDC1_SRC, "infra_msdc1_clk", "msdc30_1_sel", 10),
	GATE_INFRA5(CLK_INFRA_SEJ_F13M, "infra_sej_f13m", "clk26m", 15),
	GATE_INFRA5(CLK_INFRA_AES_TOP0_BCLK, "infra_aes", "axi_sel", 16),
	GATE_INFRA5(CLK_INFRA_MCU_PM_BCLK, "infra_mcu_pm_bclk", "axi_sel", 17),
	GATE_INFRA5(CLK_INFRA_CCIF2_AP, "infra_ccif2_ap", "axi_sel", 18),
	GATE_INFRA5(CLK_INFRA_CCIF2_MD, "infra_ccif2_md", "axi_sel", 19),
	GATE_INFRA5(CLK_INFRA_CCIF3_AP, "infra_ccif3_ap", "axi_sel", 20),
	GATE_INFRA5(CLK_INFRA_CCIF3_MD, "infra_ccif3_md", "axi_sel", 21),
};

static u16 infra_ao_rst_ofs[] = {
	INFRA_RST0_SET_OFFSET,
	INFRA_RST1_SET_OFFSET,
	INFRA_RST2_SET_OFFSET,
	INFRA_RST3_SET_OFFSET,
};

static u16 infra_ao_idx_map[] = {
	[MT6768_INFRA_RST0_THERM_CTRL_SWRST]	= 0 * RST_NR_PER_BANK + 0,
	[MT6768_INFRA_RST0_USB_TOP_SWRST]	= 0 * RST_NR_PER_BANK + 1,
	[MT6768_INFRA_RST0_MM_IOMMU_SWRST]	= 0 * RST_NR_PER_BANK + 3,
	[MT6768_INFRA_RST0_MSDC3_SWRST]		= 0 * RST_NR_PER_BANK + 4,
	[MT6768_INFRA_RST0_MSDC2_SWRST]		= 0 * RST_NR_PER_BANK + 5,
	[MT6768_INFRA_RST0_MSDC1_SWRST]		= 0 * RST_NR_PER_BANK + 6,
	[MT6768_INFRA_RST0_MSDC0_SWRST]		= 0 * RST_NR_PER_BANK + 7,
	[MT6768_INFRA_RST0_APDMA_SWRST]		= 0 * RST_NR_PER_BANK + 9,
	[MT6768_INFRA_RST0_MIMP_D_SWRST]	= 0 * RST_NR_PER_BANK + 10,
	[MT6768_INFRA_RST0_BTIF_SWRST]		= 0 * RST_NR_PER_BANK + 12,
	[MT6768_INFRA_RST0_DISP_PWM_SWRST]	= 0 * RST_NR_PER_BANK + 14,
	[MT6768_INFRA_RST0_AUXADC_SWRST]	= 0 * RST_NR_PER_BANK + 15,

	[MT6768_INFRA_RST1_IRTX_SWRST]		= 1 * RST_NR_PER_BANK + 0,
	[MT6768_INFRA_RST1_SPI0_SWRST]		= 1 * RST_NR_PER_BANK + 1,
	[MT6768_INFRA_RST1_I2C0_SWRST]		= 1 * RST_NR_PER_BANK + 2,
	[MT6768_INFRA_RST1_I2C1_SWRST]		= 1 * RST_NR_PER_BANK + 3,
	[MT6768_INFRA_RST1_I2C2_SWRST]		= 1 * RST_NR_PER_BANK + 4,
	[MT6768_INFRA_RST1_I2C3_SWRST]		= 1 * RST_NR_PER_BANK + 5,
	[MT6768_INFRA_RST1_UART0_SWRST]		= 1 * RST_NR_PER_BANK + 6,
	[MT6768_INFRA_RST1_UART1_SWRST]		= 1 * RST_NR_PER_BANK + 7,
	[MT6768_INFRA_RST1_UART2_SWRST]		= 1 * RST_NR_PER_BANK + 8,
	[MT6768_INFRA_RST1_PWM_SWRST]		= 1 * RST_NR_PER_BANK + 9,
	[MT6768_INFRA_RST1_SPI1_SWRST]		= 1 * RST_NR_PER_BANK + 10,
	[MT6768_INFRA_RST1_I2C4_SWRST]		= 1 * RST_NR_PER_BANK + 11,
	[MT6768_INFRA_RST1_DVFSP_SWRST]		= 1 * RST_NR_PER_BANK + 12,
	[MT6768_INFRA_RST1_SPI2_SWRST]		= 1 * RST_NR_PER_BANK + 13,
	[MT6768_INFRA_RST1_SPI3_SWRST]		= 1 * RST_NR_PER_BANK + 14,
	[MT6768_INFRA_RST1_UFSHCI_SWRST]	= 1 * RST_NR_PER_BANK + 15,

	[MT6768_INFRA_RST2_PMIC_WRAP_SWRST]	= 2 * RST_NR_PER_BANK + 0,
	[MT6768_INFRA_RST2_SPM_SWRST]		= 2 * RST_NR_PER_BANK + 1,
	[MT6768_INFRA_RST2_USBSIF_SWRST]	= 2 * RST_NR_PER_BANK + 2,
	[MT6768_INFRA_RST2_KP_SWRST]		= 2 * RST_NR_PER_BANK + 4,
	[MT6768_INFRA_RST2_APXGPT_SWRST]	= 2 * RST_NR_PER_BANK + 5,
	[MT6768_INFRA_RST2_CLDMA_AO_SWRST]	= 2 * RST_NR_PER_BANK + 6,
	[MT6768_INFRA_RST2_UNIPRO_UFS_SWRST]	= 2 * RST_NR_PER_BANK + 7,
	[MT6768_INFRA_RST2_DX_CC_SWRST]		= 2 * RST_NR_PER_BANK + 8,
	[MT6768_INFRA_RST2_UFSPHY_SWRST]	= 2 * RST_NR_PER_BANK + 9,

	[MT6768_INFRA_RST3_DX_CC_SEC_SWRST]	= 3 * RST_NR_PER_BANK + 0,
	[MT6768_INFRA_RST3_GCE_SWRST]		= 3 * RST_NR_PER_BANK + 1,
	[MT6768_INFRA_RST3_CLDMA_PD_SWRST]	= 3 * RST_NR_PER_BANK + 2,
	[MT6768_INFRA_RST3_TRNG_SWRST]		= 3 * RST_NR_PER_BANK + 3,
	[MT6768_INFRA_RST3_AP_MD_CCIF_1_SWRST]	= 3 * RST_NR_PER_BANK + 7,
	[MT6768_INFRA_RST3_AP_MD_CCIF_SWRST]	= 3 * RST_NR_PER_BANK + 8,
	[MT6768_INFRA_RST3_I2C1_IMM_SWRST]	= 3 * RST_NR_PER_BANK + 9,
	[MT6768_INFRA_RST3_I2C1_ARB_SWRST]	= 3 * RST_NR_PER_BANK + 10,
	[MT6768_INFRA_RST3_I2C2_IMM_SWRST]	= 3 * RST_NR_PER_BANK + 11,
	[MT6768_INFRA_RST3_I2C2_ARB_SWRST]	= 3 * RST_NR_PER_BANK + 12,
	[MT6768_INFRA_RST3_I2C5_SWRST]		= 3 * RST_NR_PER_BANK + 13,
	[MT6768_INFRA_RST3_I2C5_IMM_SWRST]	= 3 * RST_NR_PER_BANK + 14,
	[MT6768_INFRA_RST3_I2C5_ARB_SWRST]	= 3 * RST_NR_PER_BANK + 15,
	[MT6768_INFRA_RST3_SPI4_SWRST]		= 3 * RST_NR_PER_BANK + 16,
	[MT6768_INFRA_RST3_SPI5_SWRST]		= 3 * RST_NR_PER_BANK + 17,
	[MT6768_INFRA_RST3_I2C6_SWRST]		= 3 * RST_NR_PER_BANK + 24,
};

static const struct mtk_clk_rst_desc infra_ao_rst_desc = {
	.version = MTK_RST_SET_CLR,
	.rst_bank_ofs = infra_ao_rst_ofs,
	.rst_bank_nr = ARRAY_SIZE(infra_ao_rst_ofs),
	.rst_idx_map = infra_ao_idx_map,
	.rst_idx_map_nr = ARRAY_SIZE(infra_ao_idx_map),
};

static int clk_mt6768_infracfg_ao_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_hw_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base)
		return dev_err_probe(&pdev->dev, -ENOENT,
				     "failed to get base address\n");

	clk_data = mtk_alloc_clk_data(ARRAY_SIZE(infra_gates));
	if (!clk_data)
		return -ENOMEM;

	r = mtk_clk_register_gates(&pdev->dev, node, infra_gates,
				   ARRAY_SIZE(infra_gates), clk_data);
	if (r)
		goto free_infra_data;

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);
	if (r)
		goto unregister_gates;

	r = mtk_register_reset_controller_with_dev(&pdev->dev, &infra_ao_rst_desc);
	if (r)
		goto unregister_clk_provider;

	platform_set_drvdata(pdev, clk_data);

	/* Disable TOPAXI protection */
	writel(readl(base + INFRA_TOPAXI_SI0_CTL) | INFRA_CG0,
	       base + INFRA_TOPAXI_SI0_CTL);
	/* Disable DCM */
	writel(readl(base + PERI_BUS_DCM_CTRL) & ~INFRA_CG1,
	       base + PERI_BUS_DCM_CTRL);

	return 0;

unregister_clk_provider:
	of_clk_del_provider(node);
unregister_gates:
	mtk_clk_unregister_gates(infra_gates, ARRAY_SIZE(infra_gates), clk_data);
free_infra_data:
	mtk_free_clk_data(clk_data);
	return r;
}

static void clk_mt6768_infracfg_ao_remove(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data = platform_get_drvdata(pdev);
	struct device_node *node = pdev->dev.of_node;

	of_clk_del_provider(node);
	mtk_clk_unregister_gates(infra_gates, ARRAY_SIZE(infra_gates), clk_data);
	mtk_free_clk_data(clk_data);
}

static const struct of_device_id of_match_clk_mt6768_infracfg_ao[] = {
	{ .compatible = "mediatek,mt6768-infracfg-ao" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, of_match_clk_mt6768_infracfg_ao);

static struct platform_driver clk_mt6768_infracfg_ao_drv = {
	.driver = {
		.name = "clk-mt6768-infracfg-ao",
		.of_match_table = of_match_clk_mt6768_infracfg_ao,
	},
	.probe = clk_mt6768_infracfg_ao_probe,
	.remove = clk_mt6768_infracfg_ao_remove,
};
module_platform_driver(clk_mt6768_infracfg_ao_drv);

MODULE_DESCRIPTION("MediaTek MT6768 infracfg always-on clocks driver");
MODULE_LICENSE("GPL");

