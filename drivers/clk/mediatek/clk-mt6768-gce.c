// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Copyright (c) 2026 Roman Vivchar <rva333@protonmail.com>
 */

#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include "clk-mtk.h"
#include "clk-gate.h"
#include <dt-bindings/clock/mediatek,mt6768-clock.h>

static const struct mtk_gate_regs gce_cg_regs = {
	.set_ofs = 0xf0,
	.clr_ofs = 0xf0,
	.sta_ofs = 0xf0,
};

#define GATE_GCE(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &gce_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}


static const struct mtk_gate gce_gates[] = {
	GATE_GCE(CLK_GCE_GCE, "gce", "axi_sel", 16),
};

static const struct mtk_clk_desc gce_desc = {
	.clks = gce_gates,
	.num_clks = ARRAY_SIZE(gce_gates),
};

static const struct platform_device_id clk_mt6768_gce_id_table[] = {
	{ .name = "clk-mt6768-gce", .driver_data = (kernel_ulong_t)&gce_desc },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(platform, clk_mt6768_gce_id_table);

static struct platform_driver clk_mt6768_gce_drv = {
	.probe = mtk_clk_pdev_probe,
	.remove = mtk_clk_pdev_remove,
	.driver = {
		.name = "clk-mt6768-gce",
	},
	.id_table = clk_mt6768_gce_id_table,
};
module_platform_driver(clk_mt6768_gce_drv);

MODULE_DESCRIPTION("MediaTek MT6768 GCE clocks driver");
MODULE_LICENSE("GPL");
