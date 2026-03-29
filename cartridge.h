#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include "io.h"
#include "memory.h"

#include <stdint.h>

enum CartResult
{
	CART_OK = 0,
	CART_READ_ERROR = 1,
};

struct Cartridge
{
	struct Buffer* rom;
	struct MemoryMap* memmap;
	struct WriteDevice* write_device;
	uint8_t cart_type;
	int rom_banks;
	int ram_banks;
	int current_rom_bank;
	int current_ram_bank;
	uint8_t ram[128 * 1024];
};

enum CartResult CartInit(struct Cartridge* cart, struct Buffer* rom_buffer, struct MemoryMap* memmap);
void CartWrite(struct Cartridge* cart, uint16_t addr, uint8_t value);

#endif
