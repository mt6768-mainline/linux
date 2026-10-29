// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Copyright (c) 2025-2026 Roman Vivchar <rva333@protonmail.com>
 */

#include <linux/device.h>
#include <linux/interconnect.h>
#include <linux/interconnect-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <dt-bindings/interconnect/mediatek,mt6768.h>

#include "icc-emi.h"

static struct mtk_icc_node ddr_emi = {
	.name = "ddr-emi",
	.id = SLAVE_DDR_EMI,
	.ep = 1,
};

static struct mtk_icc_node mcusys = {
	.name = "mcusys",
	.id = MASTER_MCUSYS,
	.ep = 0,
	.num_links = 1,
	.links = { SLAVE_DDR_EMI }
};

static struct mtk_icc_node mcu_port0 = {
	.name = "mcu-port0",
	.id = MASTER_MCU_0,
	.ep = 0,
	.num_links = 1,
	.links = { SLAVE_DDR_EMI }
};

static struct mtk_icc_node mcu_port1 = {
	.name = "mcu-port1",
	.id = MASTER_MCU_1,
	.ep = 0,
	.num_links = 1,
	.links = { SLAVE_DDR_EMI }
};

static struct mtk_icc_node gpu = {
	.name = "gpu",
	.id = MASTER_GPUSYS,
	.ep = 0,
	.num_links = 1,
	.links = { SLAVE_DDR_EMI }
};

static struct mtk_icc_node mmsys = {
	.name = "mmsys",
	.id = MASTER_MMSYS,
	.ep = 0,
	.num_links = 1,
	.links = { SLAVE_DDR_EMI }
};

static struct mtk_icc_node mm_disp = {
	.name = "mm-disp",
	.id = MASTER_MM_DISP,
	.ep = 0,
	.num_links = 1,
	.links = { MASTER_MMSYS }
};

static struct mtk_icc_node mm_vdec = {
	.name = "mm-vdec",
	.id = MASTER_MM_VDEC,
	.ep = 0,
	.num_links = 1,
	.links = { MASTER_MMSYS }
};

static struct mtk_icc_node mm_venc = {
	.name = "mm-venc",
	.id = MASTER_MM_VENC,
	.ep = 0,
	.num_links = 1,
	.links = { MASTER_MMSYS }
};

static struct mtk_icc_node mm_cam = {
	.name = "mm-cam",
	.id = MASTER_MM_CAM,
	.ep = 0,
	.num_links = 1,
	.links = { MASTER_MMSYS }
};

static struct mtk_icc_node mm_img = {
	.name = "mm-img",
	.id = MASTER_MM_IMG,
	.ep = 0,
	.num_links = 1,
	.links = { MASTER_MMSYS }
};

static struct mtk_icc_node mm_mdp = {
	.name = "mm-mdp",
	.id = MASTER_MM_MDP,
	.ep = 0,
	.num_links = 1,
	.links = { MASTER_MMSYS }
};

static struct mtk_icc_node usb = {
	.name = "usb",
	.id = MASTER_USB,
	.ep = 0,
	.num_links = 1,
	.links = { SLAVE_DDR_EMI }
};

static struct mtk_icc_node dbgif = {
	.name = "dbgif",
	.id = MASTER_DBGIF,
	.ep = 0,
	.num_links = 1,
	.links = { SLAVE_DDR_EMI }
};

static struct mtk_icc_node wifi = {
	.name = "wifi",
	.id = MASTER_WIFI,
	.ep = 0,
	.num_links = 1,
	.links = { SLAVE_DDR_EMI }
};

static struct mtk_icc_node bt = {
	.name = "bt",
	.id = MASTER_BT,
	.ep = 0,
	.num_links = 1,
	.links = { SLAVE_DDR_EMI }
};

static struct mtk_icc_node netsys = {
	.name = "netsys",
	.id = MASTER_NETSYS,
	.ep = 0,
	.num_links = 1,
	.links = { SLAVE_DDR_EMI }
};

static struct mtk_icc_node *mt6768_emi_icc_nodes[] = {
	[SLAVE_DDR_EMI] = &ddr_emi,
	[MASTER_MCUSYS] = &mcusys,
	[MASTER_MCU_0] = &mcu_port0,
	[MASTER_MCU_1] = &mcu_port1,
	[MASTER_GPUSYS] = &gpu,
	[MASTER_MMSYS] = &mmsys,
	[MASTER_MM_DISP] = &mm_disp,
	[MASTER_MM_VDEC] = &mm_vdec,
	[MASTER_MM_VENC] = &mm_venc,
	[MASTER_MM_CAM] = &mm_cam,
	[MASTER_MM_IMG] = &mm_img,
	[MASTER_MM_MDP] = &mm_mdp,
	[MASTER_USB] = &usb,
	[MASTER_DBGIF] = &dbgif,
	[MASTER_WIFI] = &wifi,
	[MASTER_BT] = &bt,
	[MASTER_NETSYS] = &netsys,
};

static const struct mtk_icc_desc mt6768_emi_icc = {
	.nodes = mt6768_emi_icc_nodes,
	.num_nodes = ARRAY_SIZE(mt6768_emi_icc_nodes),
};

static const struct of_device_id mtk_mt6768_emi_icc_of_match[] = {
	{ .compatible = "mediatek,mt6768-emi", .data = &mt6768_emi_icc },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mtk_mt6768_emi_icc_of_match);

static struct platform_driver mtk_emi_icc_mt6768_driver = {
	.driver = {
		.name = "emi-icc-mt6768",
		.of_match_table = mtk_mt6768_emi_icc_of_match,
		.sync_state = icc_sync_state,
	},
	.probe = mtk_emi_icc_probe,
	.remove = mtk_emi_icc_remove,
};
module_platform_driver(mtk_emi_icc_mt6768_driver);

MODULE_DESCRIPTION("MediaTek MT6768 EMI ICC driver");
MODULE_LICENSE("GPL");
