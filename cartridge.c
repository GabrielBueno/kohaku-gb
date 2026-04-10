#include "cartridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "log.h"

static enum cart_result init(struct cartridge* cart, struct file file, struct memmap* memmap);
static enum cart_result map_addresses(struct cartridge *cart);
static void map_first_bank(struct cartridge *cart);
static void map_write_pages(struct cartridge* cart);
static void ch_rom_bank(struct cartridge *cart, int rom_bank);
static void ch_ram_bank(struct cartridge *cart, int ram_bank);

enum cart_result cart_init(struct cartridge *cart, struct file file, struct memmap* memmap) {
	enum cart_result result;

	if ((result = init(cart, file, memmap)) != CART_OK)
		return result;

	if ((result = map_addresses(cart)) != CART_OK)
		return result;

	return CART_OK;
}

void cart_write(void* target, uint16_t addr, uint8_t value) {
	struct cartridge* cart = (struct cartridge*)target;

	DEBUG("wrote to cartridge: %02x -> %02x", addr, value);
}

static enum cart_result init(struct cartridge* cart, struct file file, struct memmap *memmap) {
	assert(cart != NULL);
	assert(file.data != NULL);
	assert(file.length != 0);
	assert(memmap != NULL);

	uint8_t* rom_data = file.data;
	size_t rom_size   = file.length;

	if (rom_size < (size_t)0x8000) {
		ERROR("the file is too small to be a valid ROM.");
		return CART_ERROR;
	}

	uint8_t cart_type = rom_data[0x147];
	int rom_banks = 2;
	int ram_banks = 1;

	switch (rom_data[0x148]) {
	case 0x00:
		rom_banks = 2;
		break;
	case 0x01:
		rom_banks = 4;
		break;
	case 0x02:
		rom_banks = 8;
		break;
	case 0x03:
		rom_banks = 16;
		break;
	case 0x04:
		rom_banks = 32;
		break;
	case 0x05:
		rom_banks = 64;
		break;
	case 0x06:
		rom_banks = 128;
		break;
	case 0x07:
		rom_banks = 256;
		break;
	case 0x08:
		rom_banks = 512;
		break;
	case 0x52:
		rom_banks = 72;
		break;
	case 0x53:
		rom_banks = 80;
		break;
	case 0x54:
		rom_banks = 96;
		break;
	default:
		ERROR("invalid rom size flag on cartridge header (on ROM address 0x148): %02x.", rom_data[0x148]);
		return CART_ERROR;
	}

	switch (rom_data[0x149]) {
	case 0x00:
	case 0x01:
		ram_banks = 0;
		break;

	case 0x02:
		ram_banks = 1;
		break;

	case 0x03:
		ram_banks = 4;
		break;

	case 0x04:
		ram_banks = 16;
		break;

	case 0x05:
		ram_banks = 8;
		break;

	default:
		ERROR("invalid ram size flag on cartridge header (on ROM address 0x149): %02x.", rom_data[0x149]);
		return CART_ERROR;
	}

	cart->memmap           = memmap;
	cart->cart_type        = cart_type;
	cart->rom_banks        = rom_banks;
	cart->ram_banks        = ram_banks;
	cart->current_ram_bank = 0;
	cart->current_rom_bank = 1;
	cart->rom.data         = file.data;
	cart->rom.length       = file.length;

	return CART_OK;
}

static enum cart_result map_addresses(struct cartridge *cart) {
	assert(cart != NULL);

	map_first_bank(cart);
	map_write_pages(cart);
	ch_rom_bank(cart, cart->current_rom_bank);
	ch_ram_bank(cart, cart->current_ram_bank);

	return CART_OK;
}

static void map_first_bank(struct cartridge *cart) {
	assert(cart != NULL);

	for (int page = 0x00; page < 0x40; page++)
		cart->memmap->read_pages[page] = &cart->rom.data[page << 8];
}

static void map_write_pages(struct cartridge* cart) {
	assert(cart != NULL);

	cart->write_device.target = cart;
	cart->write_device.write  = cart_write;

	for (int page = 0x00; page < 0x80; page++)
		cart->memmap->write_pages[page] = &cart->write_device;
}

static void ch_rom_bank(struct cartridge *cart, int rom_bank) {
	assert(cart != NULL);
	assert(rom_bank >= 1);
	assert(rom_bank < cart->rom_banks);

	int base_address = 0x4000 * rom_bank;

	for (int page = 0x40; page < 0x80; page++)
		cart->memmap->read_pages[page] = &cart->rom.data[base_address + ((page - 0x40) << 8)];
}

static void ch_ram_bank(struct cartridge *cart, int ram_bank) {
	
}

