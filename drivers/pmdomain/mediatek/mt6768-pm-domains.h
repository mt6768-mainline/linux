/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __SOC_MEDIATEK_MT6768_PM_DOMAINS_H
#define __SOC_MEDIATEK_MT6768_PM_DOMAINS_H

#include <linux/soc/mediatek/infracfg.h>
#include "mtk-pm-domains.h"
#include <dt-bindings/power/mediatek,mt6768-power-controller.h>

/*
 * MT6768 power domain support
 */

#define BUS_PROT_INFRA_NOACK(mask)                               \
	BUS_PROT_WR_IGN(INFRA, mask, MT8183_TOP_AXI_PROT_EN_SET, \
			MT8183_TOP_AXI_PROT_EN_CLR,              \
			MT8183_TOP_AXI_PROT_EN_STA1)

#define BUS_PROT_INFRA_NOACK_1(mask)                               \
	BUS_PROT_WR_IGN(INFRA, mask, MT8183_TOP_AXI_PROT_EN_1_SET, \
			MT8183_TOP_AXI_PROT_EN_1_CLR,              \
			MT8183_TOP_AXI_PROT_EN_STA1_1)

#define BUS_PROT_SMI(mask)                           \
	BUS_PROT_WR(SMI, mask, \
		    MT8183_SMI_COMMON_CLAMP_EN_SET,        \
		    MT8183_SMI_COMMON_CLAMP_EN_CLR,        \
		    MT8183_SMI_COMMON_CLAMP_EN)

static enum scpsys_bus_prot_block scpsys_bus_prot_blocks_mt6768[] = {
	BUS_PROT_BLOCK_INFRA, BUS_PROT_BLOCK_SMI
};

static const struct scpsys_domain_data scpsys_domain_data_mt6768[] = {
	[MT6768_POWER_DOMAIN_MD1] = {
		.name = "md1",
		.sta_mask = BIT(0),
		.ctl_offs = 0x0320,
		.ext_buck_iso_offs = 0x398,
		.ext_buck_iso_mask = 0x1,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = 0,
		.sram_pdn_ack_bits = 0,
		.bp_cfg = {
			BUS_PROT_INFRA_NOACK(MT6768_TOP_AXI_PROT_EN_MD1_STEP1),
			BUS_PROT_INFRA_NOACK(MT6768_TOP_AXI_PROT_EN_MD1_STEP2),
			BUS_PROT_INFRA_NOACK_1(MT6768_TOP_AXI_PROT_EN_1_MD1_STEP2),
		},
		.caps = MTK_SCPD_KEEP_DEFAULT_OFF,
	},
	[MT6768_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.sta_mask = PWR_STATUS_CONN,
		.ctl_offs = 0x032c,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = 0,
		.sram_pdn_ack_bits = 0,
		.bp_cfg = {
			BUS_PROT_INFRA_NOACK(MT6768_TOP_AXI_PROT_EN_CONN_STEP1),
			BUS_PROT_INFRA_NOACK_1(MT6768_TOP_AXI_PROT_EN_1_CONN_STEP1),
			BUS_PROT_INFRA_NOACK(MT6768_TOP_AXI_PROT_EN_CONN_STEP2),
		},
		.caps = MTK_SCPD_KEEP_DEFAULT_OFF,
	},
	[MT6768_POWER_DOMAIN_MM] = {
		.name = "mm",
		.sta_mask = BIT(5),
		.ctl_offs = 0x030c,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_cfg = {
			BUS_PROT_INFRA_NOACK_1(MT6768_TOP_AXI_PROT_EN_1_MM_STEP1),
			BUS_PROT_INFRA_NOACK_1(MT6768_TOP_AXI_PROT_EN_1_MM_STEP2),
			BUS_PROT_INFRA_NOACK(MT6768_TOP_AXI_PROT_EN_MM_STEP3),
			BUS_PROT_INFRA_NOACK(MT6768_TOP_AXI_PROT_EN_MM_STEP4),
		},
	},
	[MT6768_POWER_DOMAIN_MFG] = {
		.name = "mfg",
		.sta_mask = BIT(11),
		.ctl_offs = 0x338,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_cfg = {
			BUS_PROT_INFRA_NOACK(MT6768_TOP_AXI_PROT_EN_MFG_STEP1),
			BUS_PROT_INFRA_NOACK(MT6768_TOP_AXI_PROT_EN_MFG_STEP2),
		},
		.caps = MTK_SCPD_DOMAIN_SUPPLY | MTK_SCPD_KEEP_DEFAULT_OFF,
	},
	[MT6768_POWER_DOMAIN_ISP] = {
		.name = "isp",
		.sta_mask = BIT(6),
		.ctl_offs = 0x0308,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_cfg = {
			BUS_PROT_INFRA_NOACK_1(MT6768_TOP_AXI_PROT_EN_1_ISP_STEP1),
			BUS_PROT_SMI(MT6768_SMI_COMMON_CLAMP_ISP),
		},
		.caps = MTK_SCPD_KEEP_DEFAULT_OFF,
	},
	[MT6768_POWER_DOMAIN_MFG_CORE0] = {
		.name = "mfg_core0",
		.sta_mask = BIT(12),
		.ctl_offs = 0x034c,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_KEEP_DEFAULT_OFF,
	},
	[MT6768_POWER_DOMAIN_MFG_CORE1] = {
		.name = "mfg_core1",
		.sta_mask = BIT(13),
		.ctl_offs = 0x0310,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_KEEP_DEFAULT_OFF,
	},
	[MT6768_POWER_DOMAIN_MFG_ASYNC] = {
		.name = "mfg_async",
		.sta_mask = BIT(14),
		.ctl_offs = 0x0334,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = 0,
		.sram_pdn_ack_bits = 0,
		.caps = MTK_SCPD_DOMAIN_SUPPLY | MTK_SCPD_KEEP_DEFAULT_OFF,
	},
	[MT6768_POWER_DOMAIN_CAM] = {
		.name = "cam",
		.sta_mask = BIT(7),
		.ctl_offs = 0x344,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(9, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
		.bp_cfg = {
			BUS_PROT_INFRA_NOACK_1(MT6768_TOP_AXI_PROT_EN_1_CAM_STEP1),
			BUS_PROT_INFRA_NOACK(MT6768_TOP_AXI_PROT_EN_CAM_STEP2),
			BUS_PROT_SMI(MT6768_SMI_COMMON_CLAMP_CAM),
		},
		.caps = MTK_SCPD_KEEP_DEFAULT_OFF,
	},
	[MT6768_POWER_DOMAIN_VENC] = {
		.name = "venc",
		.sta_mask = BIT(9),
		.ctl_offs = 0x304,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_cfg = {
			BUS_PROT_INFRA_NOACK_1(MT6768_TOP_AXI_PROT_EN_1_VENC_STEP1),
			BUS_PROT_SMI(MT6768_SMI_COMMON_CLAMP_VENC),
		},
		.caps = MTK_SCPD_KEEP_DEFAULT_OFF,
	},
	[MT6768_POWER_DOMAIN_VDEC] = {
		.name = "vdec",
		.sta_mask = BIT(8),
		.ctl_offs = 0x370,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_cfg = {
			BUS_PROT_INFRA_NOACK_1(MT6768_TOP_AXI_PROT_EN_1_VDEC_STEP1),
			BUS_PROT_SMI(MT6768_SMI_COMMON_CLAMP_VDEC),
		},
		.caps = MTK_SCPD_KEEP_DEFAULT_OFF,
	},
};

static const struct scpsys_soc_data mt6768_scpsys_data = {
	.domains_data = scpsys_domain_data_mt6768,
	.num_domains = ARRAY_SIZE(scpsys_domain_data_mt6768),
	.bus_prot_blocks = scpsys_bus_prot_blocks_mt6768,
	.num_bus_prot_blocks = ARRAY_SIZE(scpsys_bus_prot_blocks_mt6768),
};

#endif /* __SOC_MEDIATEK_MT6768_PM_DOMAINS_H */
