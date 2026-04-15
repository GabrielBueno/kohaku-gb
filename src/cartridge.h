#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include "memory.h"
#include "file.h"

#include <stdint.h>
#include <stdlib.h>

enum cart_result
{
	CART_OK    = 0,
	CART_ERROR = 1,
};

enum cart_type {
	CART_TYPE_ROM_ONLY                   = 0x00,
	CART_TYPE_MBC1                       = 0x01,
	CART_TYPE_MBC1_WITH_RAM              = 0x02,
	CART_TYPE_MBC1_WITH_RAM_WITH_BAT     = 0x03,
	CART_TYPE_MBC2                       = 0x04,
	CART_TYPE_MBC2_WITH_BAT              = 0x05,
	CART_TYPE_MBC3                       = 0x11,
	CART_TYPE_MBC3_WITH_RAM              = 0x12,
	CART_TYPE_MBC3_WITH_RAM_WITH_BATTERY = 0x13,
};

struct cartridge
{
	struct mem_write_dev mem_write_dev;

	struct {
		uint8_t* data;
		size_t length;
	} rom;

	struct mem* mem;
	int rom_banks;
	int ram_banks;
	int current_rom_bank;
	int current_ram_bank;
	uint8_t ram_enable;
	uint8_t cart_type;
	uint8_t ram[128 * 1024];
};

struct cartridge_info {
	char title[17];
	enum cart_type type;
};

enum cart_result cart_init(struct cartridge *cart, struct file file, struct mem *mem);
enum cart_result cart_info(struct cartridge *cart, struct cartridge_info *info);

#endif
