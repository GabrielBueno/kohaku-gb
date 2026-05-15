#include "cartridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "log.h"

static uint8_t ram_disabled_page[256];

static enum cart_result init(struct cartridge *cart, struct file file, struct mem *mem);
static enum cart_result map_addresses(struct cartridge *cart);

static enum cart_result map_write_pages(struct cartridge *cart);
static void map_first_bank(struct cartridge *cart);
static void ch_rom_bank(struct cartridge *cart, int rom_bank);
static void ch_ram_bank(struct cartridge *cart, int ram_bank);
static void map_init_ram(struct cartridge *cart);
static void disable_ram(struct cartridge *cart);
static void enable_ram(struct cartridge *cart);

static uint8_t ram_read(void *target, uint16_t addr);
static void ram_write(void *target, uint16_t addr, uint8_t value);

static void rom_only_write(void *target, uint16_t addr, uint8_t value);
static void mbc1_write(void *target, uint16_t addr, uint8_t value);
static void mbc2_write(void *target, uint16_t addr, uint8_t value);
static void mbc3_write(void *target, uint16_t addr, uint8_t value);

enum cart_result cart_init(struct cartridge *cart, struct file file, struct mem *mem) {
	enum cart_result result;

	for (int i = 0; i < 256; i++)
		ram_disabled_page[i] = 0xff;

	if ((result = init(cart, file, mem)) != CART_OK)
		return result;

	if ((result = map_addresses(cart)) != CART_OK)
		return result;

	return CART_OK;
}

enum cart_result cart_info(struct cartridge *cart, struct cartridge_info *info) {
	assert(cart != NULL);
	assert(info != NULL);

	memcpy(info->title, &cart->rom.data[0x134], sizeof(info->title));

	info->title[16] = '\0';
	info->type = cart->cart_type;

	return CART_OK;
}

void cart_info_print(struct cartridge *cart, struct cartridge_info *info) {
	fprintf(stderr, "%s\nCART TYPE: #%02x\nROM BANKS: %d\nRAM BANKS: %d\n", info->title, info->type, cart->rom_banks, cart->ram_banks);
}

static enum cart_result init(struct cartridge *cart, struct file file, struct mem *mem) {
	assert(cart != NULL);
	assert(file.data != NULL);
	assert(file.length != 0);
	assert(mem != NULL);

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

	// case 0x06:
	// 	rom_banks = 128;
	// 	break;
	// case 0x07:
	// 	rom_banks = 256;
	// 	break;
	// case 0x08:
	// 	rom_banks = 512;
	// 	break;
	// case 0x52:
	// 	rom_banks = 72;
	// 	break;
	// case 0x53:
	// 	rom_banks = 80;
	// 	break;
	// case 0x54:
	// 	rom_banks = 96;
	// 	break;

	case 0x06:
	case 0x07:
	case 0x08:
	case 0x52:
	case 0x53:
	case 0x54:
		ERROR("cartridges with this number of banks are not supported (%02x).", rom_data[0x148]);
		return CART_ERROR;

	default:
		ERROR("invalid rom size flag on cartridge header (on ROM address 0x148): %02x.", rom_data[0x148]);
		return CART_ERROR;
	}

	switch (rom_data[0x149]) {
	case 0x00:
	case 0x01:
		ram_banks = 1;
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

	cart->mem              = mem;
	cart->cart_type        = cart_type;
	cart->rom_banks        = rom_banks;
	cart->ram_banks        = ram_banks;
	cart->current_ram_bank = 0;
	cart->current_rom_bank = 1;
	cart->rom.data         = file.data;
	cart->rom.length       = file.length;

	cart->ram_enable = 0;

	return CART_OK;
}

static enum cart_result map_addresses(struct cartridge *cart) {
	assert(cart != NULL);

	enum cart_result result = CART_OK;

	if ((result = map_write_pages(cart)) != CART_OK)
		return result;

	map_first_bank(cart);
	map_init_ram(cart);
	
	ch_rom_bank(cart, cart->current_rom_bank);
	ch_ram_bank(cart, cart->current_ram_bank);

	return CART_OK;
}

static void map_first_bank(struct cartridge *cart) {
	assert(cart != NULL);

	for (int page = 0x00; page < 0x40; page++)
		cart->mem->read_direct[page] = &cart->rom.data[page << 8];
}

static enum cart_result map_write_pages(struct cartridge* cart) {
	assert(cart != NULL);
	
	switch (cart->cart_type) {
	case CART_TYPE_ROM_ONLY:
		cart->mem_write_dev.write = rom_only_write;
		break;

	case CART_TYPE_MBC1:
	case CART_TYPE_MBC1_WITH_RAM:
	case CART_TYPE_MBC1_WITH_RAM_WITH_BAT:
		cart->mem_write_dev.write = mbc1_write;
		break;

	case CART_TYPE_MBC2:
	case CART_TYPE_MBC2_WITH_BAT:
		cart->mem_write_dev.write = mbc2_write;
		break;

	case CART_TYPE_MBC3:
	case CART_TYPE_MBC3_WITH_RAM:
	case CART_TYPE_MBC3_WITH_RAM_WITH_BATTERY:
		cart->mem_write_dev.write = mbc3_write;
		break;

	default:
		ERROR("cartridge type (%02x) is not supported.", cart->cart_type);
		return CART_ERROR;
	}

	cart->mem_write_dev.target = cart;

	for (int page = 0x00; page < 0x80; page++)
		cart->mem->write_dev[page] = &cart->mem_write_dev;

	return CART_OK;
}

static void ch_rom_bank(struct cartridge *cart, int rom_bank) {
	assert(cart != NULL);
	assert(rom_bank < cart->rom_banks);

	int base_address = 0x4000 * rom_bank;

	for (int page = 0x40; page < 0x80; page++) {
		cart->mem->read_direct[page] = &cart->rom.data[base_address + ((page - 0x40) << 8)];
	}

	cart->current_rom_bank = rom_bank;
}

static void map_init_ram(struct cartridge *cart) {
	cart->ram_write_dev.target = cart;
	cart->ram_write_dev.write  = ram_write;
	cart->ram_read_dev.target  = cart;
	cart->ram_read_dev.read    = ram_read;

	struct mem_read_dev  *read_dev  = &cart->ram_read_dev;
	struct mem_write_dev *write_dev = &cart->ram_write_dev;

	for (uint8_t page = 0xa0; page <= 0xbf; page++) {
		cart->mem->read_dev[page] = read_dev;
		cart->mem->read_direct[page] = NULL;
		cart->mem->write_dev[page] = write_dev;
		cart->mem->write_direct[page] = NULL;
	}
}

static void ch_ram_bank(struct cartridge *cart, int ram_bank) {
	if (cart->ram_banks == 0)
		return;
		
	assert(cart != NULL);
	assert(ram_bank >= 0);
	assert(ram_bank < cart->ram_banks);

	cart->current_ram_bank = ram_bank;
}

static void disable_ram(struct cartridge *cart) {
	assert(cart != NULL);

	cart->ram_enable = 0;
}

static void enable_ram(struct cartridge *cart) {
	DEBUG("enabling ram");
	assert(cart != NULL);

	cart->ram_enable = 1;
	ch_ram_bank(cart, cart->current_ram_bank);
}

static uint8_t ram_read(void *target, uint16_t addr) {
	struct cartridge *cart = (struct cartridge*)target;

	if (cart->ram_enable)
		return cart->ram[(8*1024*cart->current_ram_bank) + addr]; 

	return 0xff;
}

static void ram_write(void *target, uint16_t addr, uint8_t value) {
	//DEBUG("writing to cart ram");
	struct cartridge *cart = (struct cartridge*)target;

	if (cart->ram_enable)
		cart->ram[(8*1024*cart->current_ram_bank) + addr] = value;
}

static void rom_only_write(void *target, uint16_t addr, uint8_t value) {
	struct cartridge *cart = (struct cartridge*)target;

	DEBUG("writing to ROM_ONLY cartridge (addr: %02x, val: %02x)", addr, value);
}

static void mbc1_write(void *target, uint16_t addr, uint8_t value) {
	struct cartridge *cart = (struct cartridge*)target;

	if (addr <= 0x1fff) {
		if (value == 0x0a) {
			enable_ram(cart);
		} else {
			disable_ram(cart);
		}
	} else if (addr <= 0x3fff) {
		uint8_t reg  = value & 0b11111;
		uint8_t mask = cart->rom_banks - 1;

		mask |= mask >> 1;
		mask |= mask >> 2;
		mask |= mask >> 4;

		if (reg == 0x00)
			ch_rom_bank(cart, 1);
		else
			ch_rom_bank(cart, reg & mask);
	}
}

static void mbc2_write(void *target, uint16_t addr, uint8_t value) {
	struct cartridge *cart = (struct cartridge*)target;

	DEBUG("wrote to MBC2 cartridge: %02x -> %02x", addr, value);
}

static void mbc3_write(void *target, uint16_t addr, uint8_t value) {
	struct cartridge *cart = (struct cartridge*)target;

	if (addr <= 0x1fff) {
		if (value == 0x0a)
			enable_ram(cart);
		else
			disable_ram(cart);
	}
	
	if (addr >= 0x2000 && addr <= 0x3fff) {
		uint8_t reg  = value & 0x7f;
		uint8_t mask = cart->rom_banks - 1;

		mask |= mask >> 1;
		mask |= mask >> 2;
		mask |= mask >> 4;

		if (reg == 0x00)
			ch_rom_bank(cart, 1);
		else
			ch_rom_bank(cart, reg & mask);
	}

	if (addr >= 0x4000 && addr <= 0x5fff) {
		if (value <= 0x07) {
			uint8_t reg  = value & 0x07;
			uint8_t mask = cart->ram_banks - 1;

			mask |= mask >> 1;
			mask |= mask >> 2;
			mask |= mask >> 4;

			ch_ram_bank(cart, reg & mask);
		} else {
			WARN("writing to RTC register 0x4000 is yet to be implemented.");
		}
	}

	if (addr >= 0xa000 && addr <= 0xbfff) {
		WARN("writing to RTC register 0xa000 is yet to be implemented.");
	}
}