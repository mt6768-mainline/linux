// SPDX-License-Identifier: GPL-2.0

#include <linux/bits.h>
#include <linux/device.h>
#include <linux/hw_random.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/arm-smccc.h>

#define MT67XX_RNG_MAGIC	0x74726e67

#define MTK_SIP_KERNEL_GET_RND \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32, ARM_SMCCC_OWNER_SIP, 0x26A)

static int mtk_trng_read(struct hwrng *rng, void *data, size_t max, bool wait)
{
	struct arm_smccc_res res;
	unsigned int buf[4] = { 0 };
	unsigned int copied = 0;

	while (copied < max) {
	  arm_smccc_smc(MTK_SIP_KERNEL_GET_RND, MT67XX_RNG_MAGIC, 0, 0, 0, 0, 0, 0, &res);
	  if (!res.a0 && !res.a1 && !res.a2 && !res.a3)
	  	goto out;

	  buf[0] = res.a0;
	  buf[1] = res.a1;
	  buf[2] = res.a2;
	  buf[3] = res.a3;

	  memcpy(data, buf, min(sizeof(buf), max - copied));
	  data += sizeof(buf);
	  copied += sizeof(buf);
	}

out:
	return copied;
}

static struct hwrng mtk_trng = {
	.name = "mtk_trng",
	.read = mtk_trng_read,
	.quality = 900,
};

static int __init mtk_trng_init(void)
{
	return hwrng_register(&mtk_trng);
}

static void __exit mtk_trng_exit(void)
{
	hwrng_unregister(&mtk_trng);
}

module_init(mtk_trng_init);
module_exit(mtk_trng_exit);

MODULE_ALIAS("platform:mtk_trng");
MODULE_DESCRIPTION("MediaTek TRNG Driver");
MODULE_LICENSE("GPL");

