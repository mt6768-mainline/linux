/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt6768-afe-clk.h  --  Mediatek 6768 afe clock ctrl definition
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

#ifndef _MT6768_AFE_CLK_H_
#define _MT6768_AFE_CLK_H_

/* APLL */
#define APLL_180MHZ_W_NAME "APLL_180MHZ"
#define APLL_196MHZ_W_NAME "APLL_196MHZ"
enum {
	MT6768_APLL_180M = 0,
	MT6768_APLL_196M,
};

struct mtk_base_afe;

int mt6768_init_clock(struct mtk_base_afe *afe);
int mt6768_afe_enable_clock(struct mtk_base_afe *afe);
void mt6768_afe_disable_clock(struct mtk_base_afe *afe);

int mt6768_apll_180m_enable(struct mtk_base_afe *afe);
void mt6768_apll_180m_disable(struct mtk_base_afe *afe);

int mt6768_apll_196m_enable(struct mtk_base_afe *afe);
void mt6768_apll_196m_disable(struct mtk_base_afe *afe);

int mt6768_get_apll_rate(struct mtk_base_afe *afe, int apll);
int mt6768_get_apll_by_rate(struct mtk_base_afe *afe, int rate);
int mt6768_get_apll_by_name(struct mtk_base_afe *afe, const char *name);

int mt6768_mck_enable(struct mtk_base_afe *afe, int mck_id, int rate);
void mt6768_mck_disable(struct mtk_base_afe *afe, int mck_id);

#endif
