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

struct cartridge
{
	struct write_device write_device;

	struct {
		uint8_t* data;
		size_t length;
	} rom;

	struct memmap* memmap;
	int rom_banks;
	int ram_banks;
	int current_rom_bank;
	int current_ram_bank;
	uint8_t ram_enable;
	uint8_t cart_type;
	uint8_t ram[128 * 1024];
};

enum cart_result cart_init(struct cartridge *cart, struct file file, struct memmap *memmap);

#endif
