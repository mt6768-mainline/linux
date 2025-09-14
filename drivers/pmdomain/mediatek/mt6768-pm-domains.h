/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __SOC_MEDIATEK_MT6768_PM_DOMAINS_H
#define __SOC_MEDIATEK_MT6768_PM_DOMAINS_H

#include "linux/soc/mediatek/infracfg.h"
#include "mtk-pm-domains.h"
#include <dt-bindings/power/mt6768-power.h>

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
			BUS_PROT_INFRA_NOACK(BIT(7)),
			BUS_PROT_INFRA_NOACK(BIT(3) | BIT(4)),
			BUS_PROT_INFRA_NOACK_1(BIT(6)),
		},
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
			BUS_PROT_INFRA_NOACK(BIT(13)),
			BUS_PROT_INFRA_NOACK_1(BIT(18)),
			BUS_PROT_INFRA_NOACK(BIT(14) | BIT(16)),
		},
		.caps = MTK_SCPD_KEEP_DEFAULT_OFF,
	},
	[MT6768_POWER_DOMAIN_DPY] = {
		.name = "dpy",
		.sta_mask = BIT(2),
		.ctl_offs = 0x031c,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.bp_cfg = {
			BUS_PROT_INFRA_NOACK(BIT(0) | BIT(5) | BIT(23) | BIT(26)),
			BUS_PROT_INFRA_NOACK_1(BIT(10) | BIT(11) | BIT(12) | BIT(13) | BIT(14) | BIT(15) | BIT(16) | BIT(17)),
			BUS_PROT_INFRA_NOACK(BIT(1) | BIT(2) | BIT(3) | BIT(4) | BIT(10) | BIT(11) | BIT(21) | BIT(22)),
		},

		.caps = MTK_SCPD_ALWAYS_ON,
	},
	[MT6768_POWER_DOMAIN_DISP] = {
		.name = "disp",
		.sta_mask = PWR_STATUS_ISP,
		.ctl_offs = 0x030c,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_cfg = {
			BUS_PROT_INFRA_NOACK_1(BIT(19) | BIT(20) | BIT(30) | BIT(31)),
			BUS_PROT_INFRA_NOACK_1(BIT(16) | BIT(17)),
			BUS_PROT_INFRA_NOACK(BIT(10) | BIT(11)),
			BUS_PROT_INFRA_NOACK(BIT(1) | BIT(2)),
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
			BUS_PROT_INFRA_NOACK(BIT(25)),
			BUS_PROT_INFRA_NOACK(BIT(21) | BIT(22)),
		}
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
			BUS_PROT_INFRA_NOACK_1(BIT(20)),
			BUS_PROT_SMI(BIT(2)),
		},
	},

	[MT6768_POWER_DOMAIN_IFR] = {
		.name = "ifr",
		.sta_mask = BIT(3),
		.ctl_offs = 0x318,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),

		.caps = MTK_SCPD_ALWAYS_ON,
	},
	[MT6768_POWER_DOMAIN_MFG_CORE0] = {
		.name = "mfg_core0",
		.sta_mask = BIT(12),
		.ctl_offs = 0x034c,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT6768_POWER_DOMAIN_MFG_CORE1] = {
		.name = "mfg_core1",
		.sta_mask = BIT(13),
		.ctl_offs = 0x0310,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT6768_POWER_DOMAIN_MFG_ASYNC] = {
		.name = "mfg_async",
		.sta_mask = BIT(14),
		.ctl_offs = 0x0334,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = 0,
		.sram_pdn_ack_bits = 0,
		//.caps = MTK_SCPD_DOMAIN_SUPPLY,
	},
	[MT6768_POWER_DOMAIN_CAM] = {
		.name = "cam",
		.sta_mask = BIT(7),
		.ctl_offs = 0x344,
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_cfg = {
			BUS_PROT_INFRA_NOACK_1(BIT(19) | BIT(21)),
			BUS_PROT_SMI(BIT(3)),
			BUS_PROT_INFRA_NOACK(BIT(20)),
		},
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
			BUS_PROT_INFRA_NOACK_1(BIT(31)),
			BUS_PROT_SMI(BIT(4)),
		},
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
			BUS_PROT_INFRA_NOACK_1(BIT(30)),
			BUS_PROT_SMI(BIT(1)),
		},
	},
};

static const struct scpsys_soc_data mt6768_scpsys_data = {
	.domains_data = scpsys_domain_data_mt6768,
	.num_domains = ARRAY_SIZE(scpsys_domain_data_mt6768),
};

#endif /* __SOC_MEDIATEK_MT8183_PM_DOMAINS_H */
