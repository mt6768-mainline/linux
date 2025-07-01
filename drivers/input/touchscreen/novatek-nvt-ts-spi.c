// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Driver for Novatek touchscreen controller.
 *
 * Copyright (c) 2025 Dinolek <git@dinolek.me>
 */

#include <linux/spi/spi.h>

#include "novatek-nvt-ts.h"

#define INFO_PARTITIONS_OFFSET	0x30

#define NVT_TS_SPI_TRANSFER_LEN	(63 * SZ_1K)
#define NVT_TS_SPI_FORCE_PAGE	BIT(24)

struct bootloader_header {
	u32 bin_addr;
	u32 sram_addr;
	u32 size;
};

struct partition_header {
	u32 sram_addr;
	u32 size;
	u32 bin_addr;
	u32 crc;
};

struct firmware_header {
	struct {
		struct bootloader_header ilm;
		struct bootloader_header dlm;
	} partition;
	struct {
		u32 ilm;
		u32 dlm;
	} crc;
	u8 reserved[8];
	struct {
		u8 count : 4;
		bool enabled : 1;
	} overlay;
};

static int nvt_ts_spi_bootloader_reset(struct regmap *regmap,
				       const struct nvt_ts_memory_map *mem_map)
{
	int ret;

	ret = regmap_write(regmap, mem_map->software_reset_n8, 0x69);
	if (ret)
		return ret;

	msleep(20);

	return regmap_write(regmap, mem_map->spi_rd_fast, 0x00);
}

static int nvt_ts_spi_upload_partition(const struct device *dev,
				       struct regmap *regmap,
				       const struct firmware *fw,
				       const struct partition_header *part_hdr)
{
	size_t size = part_hdr->size + 1;
	size_t off = 0;

	if (part_hdr->bin_addr + size > fw->size) {
		dev_err(dev, "partition exceeds firmware size (%zu > %zu)\n",
			part_hdr->bin_addr + size, fw->size);
		return -EINVAL;
	}

	if (size == 1) {
		dev_dbg(dev, "skipping reserved partition\n");
		return 0;
	}

	dev_dbg(dev,
		"uploading partition at address 0x%08x, size %zu bytes from address 0x%08x\n",
		part_hdr->sram_addr, size, part_hdr->bin_addr);

	while (off < size) {
		size_t chunk = min(NVT_TS_SPI_TRANSFER_LEN, size - off);

		int ret = regmap_raw_write(
			regmap,
			(part_hdr->sram_addr + off) | NVT_TS_SPI_FORCE_PAGE,
			&fw->data[part_hdr->bin_addr + off], chunk);
		if (ret) {
			dev_err(dev,
				"failed to write partition at 0x%08lx: %d\n",
				part_hdr->sram_addr + off, ret);
			return ret;
		}

		off += chunk;
	}

	return 0;
}

static int nvt_ts_spi_upload_bootloader(const struct device *dev,
					struct regmap *regmap,
					const struct firmware *fw,
					const struct firmware_header *hdr)
{
	const struct bootloader_header *ilm_hdr = &hdr->partition.ilm;
	const struct bootloader_header *dlm_hdr = &hdr->partition.dlm;
	struct partition_header part_hdr;
	int ret;

	part_hdr.sram_addr = ilm_hdr->sram_addr;
	part_hdr.size = ilm_hdr->size;
	part_hdr.bin_addr = ilm_hdr->bin_addr;
	ret = nvt_ts_spi_upload_partition(dev, regmap, fw, &part_hdr);
	if (ret)
		return ret;

	part_hdr.sram_addr = dlm_hdr->sram_addr;
	part_hdr.size = dlm_hdr->size;
	part_hdr.bin_addr = dlm_hdr->bin_addr;
	return nvt_ts_spi_upload_partition(dev, regmap, fw, &part_hdr);
}

static int nvt_ts_spi_upload_partitions(const struct device *dev,
					struct regmap *regmap,
					const struct firmware *fw,
					size_t hdr_offset, size_t count)
{
	const struct partition_header *part_hdr = (void *)&fw->data[hdr_offset];

	if (hdr_offset + count * sizeof(*part_hdr) > fw->size) {
		dev_err(dev, "invalid partition header offset or count\n");
		return -EINVAL;
	}

	for (int i = 0; i < count; i++) {
		int ret = nvt_ts_spi_upload_partition(dev, regmap, fw,
						      &part_hdr[i]);
		if (ret)
			return ret;
	}

	return 0;
}

static int nvt_ts_spi_write_bootloader_crc(struct regmap *regmap, u32 dest_addr,
					   u32 dest, u32 length_addr,
					   u32 length, u32 crc_addr, u32 crc)
{
	int ret;

	ret = regmap_raw_write(regmap, dest_addr, &dest, 3);
	if (ret)
		return ret;

	ret = regmap_raw_write(regmap, length_addr, &length, 3);
	if (ret)
		return ret;

	return regmap_raw_write(regmap, crc_addr, &crc, 4);
}

static int nvt_ts_spi_enable_crc(struct regmap *regmap,
				 const struct nvt_ts_memory_map *mem_map)
{
	int ret;

	/* enable hw bootloader crc */
	ret = regmap_set_bits(regmap, mem_map->bootloader_crc_enable, BIT(7));
	if (ret)
		return ret;

	/* clear fw reset status */
	ret = regmap_write(regmap,
			   mem_map->event_buffer | NVT_TS_RESET_STATUS_START,
			   0x00);
	if (ret)
		return ret;

	/* enable fw crc */
	return regmap_write(regmap,
			    mem_map->event_buffer | NVT_TS_HOST_COMMAND_START,
			    0xae);
}

static int nvt_ts_spi_upload_firmware(const struct nvt_ts_data *data,
				      const struct firmware *fw)
{
	const struct nvt_ts_memory_map *mem_map = data->memory_map;
	const struct firmware_header *hdr;
	struct device *dev = data->dev;
	size_t info_count;
	u32 status;
	int ret;

	if (!mem_map) {
		dev_err(dev, "no memory map defined\n");
		return -EINVAL;
	}

	if (!fw || fw->size < sizeof(*hdr)) {
		dev_err(dev, "invalid firmware data\n");
		return -EINVAL;
	}

	hdr = (const struct firmware_header *)fw->data;

	ret = nvt_ts_spi_bootloader_reset(data->regmap, mem_map);
	if (ret) {
		dev_err(dev, "failed to reset bootloader: %d\n", ret);
		return ret;
	}

	ret = nvt_ts_spi_upload_bootloader(dev, data->regmap, fw, hdr);
	if (ret) {
		dev_err(dev, "failed to upload bootloader: %d\n", ret);
		return ret;
	}

	info_count = DIV_ROUND_UP(hdr->partition.ilm.bin_addr - INFO_PARTITIONS_OFFSET,
				  sizeof(struct partition_header));
	ret = nvt_ts_spi_upload_partitions(dev, data->regmap, fw,
					   INFO_PARTITIONS_OFFSET, info_count);
	if (ret) {
		dev_err(dev, "failed to upload info partitions: %d\n", ret);
		return ret;
	}

	if (hdr->overlay.enabled) {
		ret = nvt_ts_spi_upload_partitions(dev, data->regmap, fw,
						   hdr->partition.dlm.bin_addr,
						   hdr->overlay.count);
		if (ret) {
			dev_err(dev,
				"failed to upload overlay partitions: %d\n",
				ret);
			return ret;
		}
	}

	ret = nvt_ts_spi_write_bootloader_crc(
		data->regmap, mem_map->destination.ilm,
		hdr->partition.ilm.sram_addr, mem_map->length.ilm,
		hdr->partition.ilm.size, mem_map->golden_checksum.ilm,
		hdr->crc.ilm);
	if (ret) {
		dev_err(dev, "failed to write ILM bootloader CRC: %d\n", ret);
		return ret;
	}

	ret = nvt_ts_spi_write_bootloader_crc(
		data->regmap, mem_map->destination.dlm,
		hdr->partition.dlm.sram_addr, mem_map->length.dlm,
		hdr->partition.dlm.size, mem_map->golden_checksum.dlm,
		hdr->crc.dlm);
	if (ret) {
		dev_err(dev, "failed to write DLM bootloader CRC: %d\n", ret);
		return ret;
	}

	ret = nvt_ts_spi_enable_crc(data->regmap, mem_map);
	if (ret) {
		dev_err(dev, "failed to enable bootloader CRC: %d\n", ret);
		return ret;
	}

	ret = regmap_write(data->regmap, mem_map->boot_ready, 0x01);
	if (ret) {
		dev_err(dev, "failed to set boot ready: %d\n", ret);
		return ret;
	}

	ret = regmap_read_poll_timeout(
		data->regmap, mem_map->event_buffer | NVT_TS_RESET_STATUS_START,
		status, status == 0xa0, 10000, 100000);
	if (ret) {
		dev_err(dev, "firmware failed to boot, status: 0x%02x\n",
			status);
		return ret;
	}

	return 0;
}

static int nvt_ts_spi_xfer(struct spi_device *spi, u32 page_addr, u8 cmd,
			   struct spi_transfer *xfers, size_t num_xfers)
{
	struct spi_transfer cmd_xfer = { .tx_buf = &cmd, .len = 1 };
	struct spi_message msg;

	u8 page_buf[] = { 0xff, page_addr >> 8, page_addr };
	struct spi_transfer page_xfer = {
		.tx_buf = page_buf,
		.len = ARRAY_SIZE(page_buf),
		.cs_change = 1,
	};

	spi_message_init(&msg);

	if (page_addr)
		spi_message_add_tail(&page_xfer, &msg);

	spi_message_add_tail(&cmd_xfer, &msg);

	for (int i = 0; i < num_xfers; i++)
		spi_message_add_tail(&xfers[i], &msg);

	return spi_sync(spi, &msg);
}

static int nvt_ts_spi_reg_xfer(struct spi_device *spi, const void *reg_buf,
			       bool write, struct spi_transfer *xfers,
			       size_t num_xfers)
{
	u32 reg = *(u32 *)reg_buf;
	u32 page_addr = reg >> 7;
	u8 cmd = reg & GENMASK(6, 0);

	if (write)
		cmd |= BIT(7);

	return nvt_ts_spi_xfer(spi, page_addr, cmd, xfers, num_xfers);
}

static int nvt_ts_spi_read(void *context, const void *reg_buf, size_t reg_size,
			   void *val_buf, size_t val_size)
{
	struct spi_device *spi = to_spi_device(context);
	u8 status;
	int ret;

	struct spi_transfer xfers[] = {
		{ .rx_buf = &status, .len = 1 },
		{ .rx_buf = val_buf, .len = val_size },
	};

	ret = nvt_ts_spi_reg_xfer(spi, reg_buf, false, xfers,
				  ARRAY_SIZE(xfers));
	if (ret)
		return ret;

	if (status != 0xff) {
		dev_err(&spi->dev, "read failed, status: 0x%02x\n", status);
		return -EIO;
	}

	return 0;
}

static int nvt_ts_spi_write(void *context, const void *data, size_t count)
{
	struct spi_device *spi = to_spi_device(context);

	struct spi_transfer xfer = {
		.tx_buf = data + sizeof(u32),
		.len = count - sizeof(u32),
	};

	return nvt_ts_spi_reg_xfer(spi, data, true, &xfer, 1);
}

static const struct regmap_config nvt_ts_spi_regmap_conf = {
	.reg_bits = 32,
	.val_bits = 8,
	.read = nvt_ts_spi_read,
	.write = nvt_ts_spi_write,
};

static const struct input_id nvt_ts_spi_input_id = {
	.bustype = BUS_SPI,
};

static int nvt_ts_spi_probe(struct spi_device *spi)
{
	struct regmap *regmap;

	regmap =
		devm_regmap_init(&spi->dev, NULL, spi, &nvt_ts_spi_regmap_conf);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	return nvt_ts_probe(&spi->dev, spi->irq, regmap, &nvt_ts_spi_input_id,
			    nvt_ts_spi_upload_firmware);
}

static const struct nvt_ts_memory_map nvt_nt36672a_memory_map = {
	.event_buffer = 0x21c00,
	.software_reset_n8 = 0x3f0fe,
	.spi_rd_fast = 0x3f310,
	.bootloader_crc_enable = 0x3f30e,
	.boot_ready = 0x3f10d,
	.length = {
		.ilm = 0x3f118,
		.dlm = 0x3f130,
	},
	.destination = {
		.ilm = 0x3f128,
		.dlm = 0x3f12c,
	},
	.golden_checksum = {
		.ilm = 0x3f100,
		.dlm = 0x3f104,
	}
};

static const struct nvt_ts_chip_data nvt_nt36672a_ts_data = {
	.memory_map = &nvt_nt36672a_memory_map,
	.wake_type = 0x01,
	.chip_id = 0x00,
};

static const struct of_device_id nvt_ts_spi_of_match[] = {
	{ .compatible = "novatek,nt36672a-ts", .data = &nvt_nt36672a_ts_data },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, nvt_ts_spi_of_match);

static const struct spi_device_id nvt_ts_spi_id[] = {
	{ "nt36672a-ts", (kernel_ulong_t)&nvt_nt36672a_ts_data },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(spi, nvt_ts_spi_id);

static struct spi_driver nvt_ts_spi_driver = {
	.driver = {
		.name	= "novatek-nvt-ts-spi",
		.pm	= pm_sleep_ptr(&nvt_ts_pm_ops),
		.of_match_table = nvt_ts_spi_of_match,
	},
	.probe = nvt_ts_spi_probe,
	.id_table = nvt_ts_spi_id,
};
module_spi_driver(nvt_ts_spi_driver);

MODULE_DESCRIPTION("Novatek spi touchscreen driver");
MODULE_AUTHOR("Hans de Goede <hdegoede@redhat.com>");
MODULE_LICENSE("GPL");
