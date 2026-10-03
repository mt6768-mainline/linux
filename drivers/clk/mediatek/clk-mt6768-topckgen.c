// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Copyright (c) 2025-2026 Roman Vivchar <rva333@protonmail.com>
 */
#include <dt-bindings/clock/mediatek,mt6768-clock.h>

#include <linux/clk.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-mux.h"
#include "clk-gate.h"

#define CLK_CFG_UPDATE	0x04
#define CLK_CFG_0	0x40
#define CLK_CFG_0_SET	0x44
#define CLK_CFG_0_CLR	0x48
#define CLK_CFG_1	0x50
#define CLK_CFG_1_SET	0x54
#define CLK_CFG_1_CLR	0x58
#define CLK_CFG_2	0x60
#define CLK_CFG_2_SET	0x64
#define CLK_CFG_2_CLR	0x68
#define CLK_CFG_3	0x70
#define CLK_CFG_3_SET	0x74
#define CLK_CFG_3_CLR	0x78
#define CLK_CFG_4	0x80
#define CLK_CFG_4_SET	0x84
#define CLK_CFG_4_CLR	0x88
#define CLK_CFG_5	0x90
#define CLK_CFG_5_SET	0x94
#define CLK_CFG_5_CLR	0x98
#define CLK_CFG_6	0xa0
#define CLK_CFG_6_SET	0xa4
#define CLK_CFG_6_CLR	0xa8
#define CLK_CFG_7	0xb0
#define CLK_CFG_7_SET	0xb4
#define CLK_CFG_7_CLR	0xb8
#define CLK_CFG_8	0xc0
#define CLK_CFG_8_SET	0xc4
#define CLK_CFG_8_CLR	0xc8
#define CLK_CFG_9	0xd0
#define CLK_CFG_9_SET	0xd4
#define CLK_CFG_9_CLR	0xd8
#define CLK_CFG_10	0xe0
#define CLK_CFG_10_SET	0xe4
#define CLK_CFG_10_CLR	0xe8

static const struct mtk_fixed_clk fixed_clks[] = {
	/*
	 * This clock is managed by SCP firmware and can act like a primary source.
	 * However, getting its rate is quite difficult and not any useful since
	 * SCP can change its own clock. Leave it at 250 MHz as minimal safe frequency.
	 */
	FIXED_CLK(CLK_TOP_ULPOSC1, "ulposc1", NULL, 250000000),
	/*
	* DMPLL (or MEMPLL) is the DDRPHY clock. It heavily depends on the board.
	* It's managed by SSPM and separate driver for it would be read-only. This
	* doesn't look any useful, so keep it as fixed clock with lowest LPDDR3-933
	* setting available on this SoC for now.
	*/
	FIXED_CLK(CLK_TOP_DMPLL, "dmpll", NULL, 466000000),
};

static const struct mtk_fixed_factor top_divs[] = {
	FACTOR(CLK_TOP_SYSPLL_D2, "syspll_d2", "mainpll", 1, 2),
	FACTOR(CLK_TOP_SYSPLL1_D2, "syspll1_d2", "syspll_d2", 1, 2),
	FACTOR(CLK_TOP_SYSPLL1_D4, "syspll1_d4", "syspll_d2", 1, 4),
	FACTOR(CLK_TOP_SYSPLL1_D8, "syspll1_d8", "syspll_d2", 1, 8),
	FACTOR(CLK_TOP_SYSPLL1_D16, "syspll1_d16", "syspll_d2", 1, 16),
	FACTOR(CLK_TOP_SYSPLL_D3, "syspll_d3", "mainpll", 1, 3),
	FACTOR(CLK_TOP_SYSPLL2_D2, "syspll2_d2", "syspll_d3", 1, 2),
	FACTOR(CLK_TOP_SYSPLL2_D4, "syspll2_d4", "syspll_d3", 1, 4),
	FACTOR(CLK_TOP_SYSPLL2_D8, "syspll2_d8", "syspll_d3", 1, 8),
	FACTOR(CLK_TOP_SYSPLL_D5, "syspll_d5", "mainpll", 1, 5),
	FACTOR(CLK_TOP_SYSPLL3_D2, "syspll3_d2", "syspll_d5", 1, 2),
	FACTOR(CLK_TOP_SYSPLL3_D4, "syspll3_d4", "syspll_d5", 1, 4),
	FACTOR(CLK_TOP_SYSPLL_D7, "syspll_d7", "mainpll", 1, 7),
	FACTOR(CLK_TOP_SYSPLL4_D2, "syspll4_d2", "syspll_d7", 1, 2),
	FACTOR(CLK_TOP_SYSPLL4_D4, "syspll4_d4", "syspll_d7", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL_192M, "univpll_192m", "univ2pll", 1, 13),
	FACTOR(CLK_TOP_UNIVPLL_192M_D4, "univpll_192m_d4", "univpll_192m", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL_192M_D8, "univpll_192m_d8", "univpll_192m", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL_192M_D16, "univpll_192m_d16", "univpll_192m", 1, 16),
	FACTOR(CLK_TOP_UNIVPLL_192M_D32, "univpll_192m_d32", "univpll_192m", 1, 32),
	FACTOR(CLK_TOP_UNIVPLL, "univpll", "univ2pll", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL_D2, "univpll_d2", "univpll", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL1_D2, "univpll1_d2", "univpll_d2", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL1_D4, "univpll1_d4", "univpll_d2", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL_D3, "univpll_d3", "univpll", 1, 3),
	FACTOR(CLK_TOP_UNIVPLL2_D2, "univpll2_d2", "univpll_d3", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL2_D4, "univpll2_d4", "univpll_d3", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL2_D8, "univpll2_d8", "univpll_d3", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL2_D32, "univpll2_d32", "univpll_d3", 1, 32),
	FACTOR(CLK_TOP_UNIVPLL_D5, "univpll_d5", "univpll", 1, 5),
	FACTOR(CLK_TOP_UNIVPLL3_D2, "univpll3_d2", "univpll_d5", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL3_D4, "univpll3_d4", "univpll_d5", 1, 4),
	FACTOR(CLK_TOP_MMPLL_D2, "mmpll_d2", "mmpll", 1, 2),
	FACTOR(CLK_TOP_MPLL_D2, "mpll_d2", "mpll", 1, 2),
	FACTOR(CLK_TOP_MPLL_D4, "mpll_d4", "mpll", 1, 4),
	FACTOR(CLK_TOP_MSDCPLL_D2, "msdcpll_d2", "msdcpll", 1, 2),
	FACTOR(CLK_TOP_APLL1_D2, "apll1_d2", "apll1", 1, 2),
	FACTOR(CLK_TOP_APLL1_D4, "apll1_d4", "apll1", 1, 4),
	FACTOR(CLK_TOP_APLL1_D8, "apll1_d8", "apll1", 1, 8),
	FACTOR(CLK_TOP_ULPOSC1_D2, "ulposc1_d2", "ulposc1", 1, 2),
	FACTOR(CLK_TOP_ULPOSC1_D4, "ulposc1_d4", "ulposc1", 1, 4),
	FACTOR(CLK_TOP_ULPOSC1_D8, "ulposc1_d8", "ulposc1", 1, 8),
	FACTOR(CLK_TOP_ULPOSC1_D16, "ulposc1_d16", "ulposc1", 1, 16),
	FACTOR(CLK_TOP_ULPOSC1_D32, "ulposc1_d32", "ulposc1", 1, 32),
};

static const char * const axi_parents[] = {
	"clk26m",
	"syspll_d7",
	"syspll1_d4",
	"syspll3_d2"
};

static const char * const mem_parents[] = {
	"clk26m",
	"dmpll",
	"apll1"
};

static const char * const mm_parents[] = {
	"clk26m",
	"mmpll",
	"syspll1_d2",
	"syspll_d5",
	"syspll1_d4",
	"univpll_d5",
	"univpll1_d2",
	"mmpll_d2"
};

static const char * const scp_parents[] = {
	"clk26m",
	"syspll4_d2",
	"univpll2_d2",
	"syspll1_d2",
	"univpll1_d2",
	"syspll_d3",
	"univpll_d3"
};

static const char * const mfg_parents[] = {
	"clk26m",
	"mfgpll",
	"syspll_d3",
	"univpll_d3"
};

static const char * const atb_parents[] = {
	"clk26m",
	"syspll1_d4",
	"syspll1_d2"
};

static const char * const camtg_parents[] = {
	"clk26m",
	"univpll_192m_d8",
	"univpll2_d8",
	"univpll_192m_d4",
	"univpll2_d32",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const uart_parents[] = {
	"clk26m",
	"univpll2_d8"
};

static const char * const spi_parents[] = {
	"clk26m",
	"syspll3_d2",
	"syspll4_d2",
	"syspll2_d4"
};

static const char * const msdc5hclk_parents[] = {
	"clk26m",
	"syspll1_d2",
	"univpll1_d4",
	"syspll2_d2"
};

static const char * const msdc50_0_parents[] = {
	"clk26m",
	"msdcpll",
	"syspll2_d2",
	"syspll4_d2",
	"univpll1_d2",
	"syspll1_d2",
	"univpll_d5",
	"univpll1_d4"
};

static const char * const msdc30_1_parents[] = {
	"clk26m",
	"msdcpll_d2",
	"univpll2_d2",
	"syspll2_d2",
	"syspll1_d4",
	"univpll1_d4",
	"univpll_192m_d4",
	"syspll2_d4"
};

static const char * const audio_parents[] = {
	"clk26m",
	"syspll3_d4",
	"syspll4_d4",
	"syspll1_d16"
};

static const char * const aud_intbus_parents[] = {
	"clk26m",
	"syspll1_d4",
	"syspll4_d2"
};

static const char * const aud_1_parents[] = {
	"clk26m",
	"apll1"
};

static const char * const aud_engen1_parents[] = {
	"clk26m",
	"apll1_d2",
	"apll1_d4",
	"apll1_d8"
};

static const char * const disp_pwm_parents[] = {
	"clk26m",
	"univpll2_d4",
	"ulposc1_d2",
	"ulposc1_d8"
};

static const char * const sspm_parents[] = {
	"clk26m",
	"syspll1_d2",
	"syspll_d3"
};

static const char * const dxcc_parents[] = {
	"clk26m",
	"syspll1_d2",
	"syspll1_d4",
	"syspll1_d8"
};

static const char * const usb_top_parents[] = {
	"clk26m",
	"univpll3_d4"
};

static const char * const spm_parents[] = {
	"clk26m",
	"syspll1_d8"
};

static const char * const i2c_parents[] = {
	"clk26m",
	"univpll3_d4",
	"univpll3_d2",
	"syspll1_d8",
	"syspll2_d8"
};

static const char * const pwm_parents[] = {
	"clk26m",
	"univpll3_d4",
	"syspll1_d8"
};

static const char * const seninf_parents[] = {
	"clk26m",
	"univpll1_d4",
	"univpll1_d2",
	"univpll2_d2"
};

static const char * const aes_fde_parents[] = {
	"clk26m",
	"msdcpll",
	"univpll_d3",
	"univpll2_d2",
	"univpll1_d2",
	"syspll1_d2"
};

static const char * const ulposc_parents[] = {
	"clk26m",
	"ulposc1_d4",
	"ulposc1_d8",
	"ulposc1_d16",
	"ulposc1_d32"
};

static const char * const camtm_parents[] = {
	"clk26m",
	"univpll1_d4",
	"univpll1_d2",
	"univpll2_d2"
};

static const char * const venc_parents[] = {
	"clk26m",
	"mmpll",
	"syspll1_d2",
	"syspll_d5",
	"syspll1_d4",
	"syspll_d3",
	"univpll_d3",
	"univpll1_d2"
};

static const char * const cam_parents[] = {
	"clk26m",
	"syspll_d2",
	"syspll1_d2",
	"syspll_d5",
	"mmpll",
	"univpll_d5",
	"univpll1_d2",
	"mmpll_d2"
};

static const struct mtk_mux top_muxes[] = {
	/* CLK_CFG_0 */
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_AXI_SEL, "axi_sel", axi_parents,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR, 0, 2, 7,
		CLK_CFG_UPDATE, 0, CLK_IS_CRITICAL),
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_MEM_SEL, "mem_sel", mem_parents,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR, 8, 2, 15,
		CLK_CFG_UPDATE, 1, CLK_IS_CRITICAL),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MM_SEL, "mm_sel", mm_parents, CLK_CFG_0,
		CLK_CFG_0_SET, CLK_CFG_0_CLR, 16, 3, 23, CLK_CFG_UPDATE, 2),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SCP_SEL, "scp_sel", scp_parents, CLK_CFG_0,
		CLK_CFG_0_SET, CLK_CFG_0_CLR, 24, 3, 31, CLK_CFG_UPDATE, 3),
	/* CLK_CFG_1 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MFG_SEL, "mfg_sel", mfg_parents, CLK_CFG_1,
		CLK_CFG_1_SET, CLK_CFG_1_CLR, 0, 2, 7, CLK_CFG_UPDATE, 4),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ATB_SEL, "atb_sel", atb_parents, CLK_CFG_1,
		CLK_CFG_1_SET, CLK_CFG_1_CLR, 8, 2, 15, CLK_CFG_UPDATE, 5),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG_SEL, "camtg_sel",
		camtg_parents, CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR,
		16, 3, 23, CLK_CFG_UPDATE, 6),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG1_SEL, "camtg1_sel",
		camtg_parents, CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR,
		24, 3, 31, CLK_CFG_UPDATE, 7),
	/* CLK_CFG_2 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG2_SEL, "camtg2_sel",
		camtg_parents, CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR,
		0, 3, 7, CLK_CFG_UPDATE, 8),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG3_SEL, "camtg3_sel",
		camtg_parents, CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR,
		8, 3, 15, CLK_CFG_UPDATE, 9),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UART_SEL, "uart_sel", uart_parents, CLK_CFG_2,
		CLK_CFG_2_SET, CLK_CFG_2_CLR, 16, 1, 23, CLK_CFG_UPDATE, 10),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI_SEL, "spi_sel", spi_parents, CLK_CFG_2,
		CLK_CFG_2_SET, CLK_CFG_2_CLR, 24, 2, 31, CLK_CFG_UPDATE, 11),
	/* CLK_CFG_3 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_0_HCLK_SEL, "msdc5hclk_sel",
		msdc5hclk_parents, CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR, 0,
		2, 7, CLK_CFG_UPDATE, 12),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_0_SEL, "msdc50_0_sel",
		msdc50_0_parents, CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR,
		8, 3, 15, CLK_CFG_UPDATE, 13),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL, "msdc30_1_sel",
		msdc30_1_parents, CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR,
		16, 3, 23, CLK_CFG_UPDATE, 14),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUDIO_SEL, "audio_sel",
		audio_parents, CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR,
		24, 2, 31, CLK_CFG_UPDATE, 15),
	/* CLK_CFG_4 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL, "aud_intbus_sel",
		aud_intbus_parents, CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
		0, 2, 7, CLK_CFG_UPDATE, 16),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_1_SEL, "aud_1_sel",
		aud_1_parents, CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
		8, 1, 15, CLK_CFG_UPDATE, 17),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_ENGEN1_SEL, "aud_engen1_sel",
		aud_engen1_parents, CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
		16, 2, 23, CLK_CFG_UPDATE, 18),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DISP_PWM_SEL, "disp_pwm_sel",
		disp_pwm_parents, CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
		24, 2, 31, CLK_CFG_UPDATE, 19),
	/* CLK_CFG_5 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SSPM_SEL, "sspm_sel", sspm_parents, CLK_CFG_5,
		CLK_CFG_5_SET, CLK_CFG_5_CLR, 0, 2, 7, CLK_CFG_UPDATE, 20),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DXCC_SEL, "dxcc_sel", dxcc_parents, CLK_CFG_5,
		CLK_CFG_5_SET, CLK_CFG_5_CLR, 8, 2, 15, CLK_CFG_UPDATE, 21),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_TOP_SEL, "usb_top_sel",
		usb_top_parents, CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR,
		16, 1, 23, CLK_CFG_UPDATE, 22),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPM_SEL, "spm_sel", spm_parents, CLK_CFG_5,
		CLK_CFG_5_SET, CLK_CFG_5_CLR, 24, 1, 31, CLK_CFG_UPDATE, 23),
	/* CLK_CFG_6 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_I2C_SEL, "i2c_sel", i2c_parents, CLK_CFG_6,
		CLK_CFG_6_SET, CLK_CFG_6_CLR, 0, 3, 7, CLK_CFG_UPDATE, 24),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PWM_SEL, "pwm_sel", pwm_parents, CLK_CFG_6,
		CLK_CFG_6_SET, CLK_CFG_6_CLR, 8, 2, 15, CLK_CFG_UPDATE, 25),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF_SEL, "seninf_sel",
		seninf_parents, CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR,
		16, 2, 23, CLK_CFG_UPDATE, 26),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AES_FDE_SEL, "aes_fde_sel",
		aes_fde_parents, CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR,
		24, 3, 31, CLK_CFG_UPDATE, 27),
	/* CLK_CFG_7 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PWRAP_ULPOSC_SEL, "ulposc_sel",
		ulposc_parents, CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR,
		0, 3, 7, CLK_CFG_UPDATE, 28),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTM_SEL, "camtm_sel",
		camtm_parents, CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR,
		8, 2, 15, CLK_CFG_UPDATE, 29),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_VENC_SEL, "venc_sel",
		venc_parents, CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR,
		16, 3, 23, CLK_CFG_UPDATE, 30),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAM_SEL, "cam_sel",
		cam_parents, CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR,
		24, 3, 31, CLK_CFG_UPDATE, 31),
};

static const struct mtk_gate_regs top0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs top1_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x104,
	.sta_ofs = 0x104,
};

#define GATE_TOP0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_TOP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

static const struct mtk_gate top_gates[] = {
	GATE_TOP0(CLK_TOP_MD_32K, "md_32k", "clk32k", 8),
	GATE_TOP0(CLK_TOP_MD_26M, "md_26m", "clk26m", 9),
	GATE_TOP0(CLK_TOP_MD2_32K, "md2_32k", "clk32k", 10),
	GATE_TOP0(CLK_TOP_MD2_26M, "md2_26m", "clk26m", 11),
	GATE_TOP1(CLK_TOP_ARMPLL_DIVIDER_PLL0, "arm_div_pll0", "syspll_d2", 3),
	GATE_TOP1(CLK_TOP_ARMPLL_DIVIDER_PLL1, "arm_div_pll1", "mainpll", 4),
	GATE_TOP1(CLK_TOP_ARMPLL_DIVIDER_PLL2, "arm_div_pll2", "univpll_d2", 5),
	GATE_TOP1(CLK_TOP_FMEM_OCC_DRC, "drc", "univpll2_d2", 6),
	GATE_TOP1(CLK_TOP_USB20_48M, "usb20_48m", "univpll_192m_d4", 8),
	GATE_TOP1(CLK_TOP_UNIVPLL_48M, "univpll_48m", "univpll_192m_d4", 9),
	GATE_TOP1(CLK_TOP_MPLL_104M, "mpll_104m", "mpll_d2", 10),
	GATE_TOP1(CLK_TOP_MPLL_52M, "mpll_52m", "mpll_d4", 11),
	GATE_TOP1(CLK_TOP_F_UFS_MP_SAP_CFG, "ufs_sap", "clk26m", 12),
	GATE_TOP1(CLK_TOP_F_BIST2FPC, "bist2fpc", "univpll2_d2", 16),
};

static const char * const i2s_parents[] = {
	"aud_1_sel",
};

static const struct mtk_composite top_aud_clocks[] = {
	MUX(CLK_TOP_I2S0_M_SEL, "i2s0_sel", i2s_parents, 0x320, 8, 1),
	MUX(CLK_TOP_I2S1_M_SEL, "i2s1_sel", i2s_parents, 0x320, 9, 1),
	MUX(CLK_TOP_I2S2_M_SEL, "i2s2_sel", i2s_parents, 0x320, 10, 1),
	MUX(CLK_TOP_I2S3_M_SEL, "i2s3_sel", i2s_parents, 0x320, 11, 1),

	DIV_GATE(CLK_TOP_APLL12_DIV0, "apll12_div0", "i2s0_sel", 0x320, 2, 0x324, 8, 0),
	DIV_GATE(CLK_TOP_APLL12_DIV1, "apll12_div1", "i2s1_sel", 0x320, 3, 0x324, 8, 8),
	DIV_GATE(CLK_TOP_APLL12_DIV2, "apll12_div2", "i2s2_sel", 0x320, 4, 0x324, 8, 16),
	DIV_GATE(CLK_TOP_APLL12_DIV3, "apll12_div3", "i2s3_sel", 0x320, 5, 0x324, 8, 24),
};

static int clk_mt6768_reg_mfg_mux_notifier(struct device *dev, struct clk *clk)
{
	struct mtk_mux_nb *mfg_mux_nb;
	int i;

	mfg_mux_nb = devm_kzalloc(dev, sizeof(*mfg_mux_nb), GFP_KERNEL);
	if (!mfg_mux_nb)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(top_muxes); i++)
		if (top_muxes[i].id == CLK_TOP_MFG_SEL)
			break;
	if (i == ARRAY_SIZE(top_muxes))
		return -EINVAL;

	mfg_mux_nb->ops = top_muxes[i].ops;
	mfg_mux_nb->bypass_index = 0; /* Bypass to 26M crystal */

	return devm_mtk_clk_mux_notifier_register(dev, clk, mfg_mux_nb);
}

static const struct mtk_clk_desc topckgen_desc = {
	.fixed_clks = fixed_clks,
	.num_fixed_clks = ARRAY_SIZE(fixed_clks),
	.factor_clks = top_divs,
	.num_factor_clks = ARRAY_SIZE(top_divs),
	.mux_clks = top_muxes,
	.num_mux_clks = ARRAY_SIZE(top_muxes),
	.composite_clks = top_aud_clocks,
	.num_composite_clks = ARRAY_SIZE(top_aud_clocks),
	.clks = top_gates,
	.num_clks = ARRAY_SIZE(top_gates),
	.clk_notifier_func = clk_mt6768_reg_mfg_mux_notifier,
	.mfg_clk_idx = CLK_TOP_MFG_SEL,
};

static const struct of_device_id of_match_topckgen_mt6768[] = {
	{ .compatible = "mediatek,mt6768-topckgen", .data = &topckgen_desc },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, of_match_topckgen_mt6768);

static struct platform_driver clk_mt6768_topckgen_drv = {
	.probe = mtk_clk_simple_probe,
	.remove = mtk_clk_simple_remove,
	.driver = {
		.name = "clk-mt6768-topck",
		.of_match_table = of_match_topckgen_mt6768,
	},
};

MODULE_DESCRIPTION("MediaTek MT6768 top clock generators driver");
module_platform_driver(clk_mt6768_topckgen_drv);
MODULE_LICENSE("GPL");
