#include "cartridge.h"

#include <stdio.h>

enum CartResult init(struct Cartridge* cart, struct Buffer* rom_buffer, struct MemoryMap* memmap);
enum CartResult map_addresses(struct Cartridge* cart);

void map_first_bank(struct Cartridge* cart);
void ch_rom_bank(struct Cartridge* cart, int rom_bank);
void ch_ram_bank(struct Cartridge* cart, int ram_bank);

enum CartResult CartInit(struct Cartridge* cart, struct Buffer* rom_buffer, struct MemoryMap* memmap)
{
	enum CartResult result;

	if ((result = init(cart, rom_buffer, memmap)) != 0)
	{
		return result;
	}

	if ((result = map_addresses(cart)) != 0)
	{
		return result;
	}

	return CART_OK;
}

enum CartResult init(struct Cartridge* cart, struct Buffer* rom_buffer, struct MemoryMap* memmap)
{
	if (rom_buffer == NULL)
	{
		fprintf(stderr, "[InitCartridge()] rom_buffer parameter was NULL.\n");
		return CART_READ_ERROR;
	}

	if (memmap == NULL)
	{
		fprintf(stderr, "[InitCartridge()] memmap parameter was NULL.\n");
		return CART_READ_ERROR;
	}

	uint8_t* rom_data = rom_buffer->data;
	size_t rom_size = rom_buffer->size;

	if (rom_size < (size_t)0x8000)
	{
		fprintf(stderr, "[InitCartridge()] input buffer is too small to be a valid ROM\n");
		return CART_READ_ERROR;
	}

	uint8_t cart_type = rom_data[0x147];
	int rom_banks = 2;
	int ram_banks = 1;

	switch (rom_data[0x148])
	{
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
		fprintf(stderr, "[InitCartridge()] invalid rom size flag on cartridge header (on ROM address 0x148): %02x\n", rom_data[0x148]);
		return CART_READ_ERROR;
	}

	switch (rom_data[0x149])
	{
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
		fprintf(stderr, "[InitCartridge()] invalid ram size flag on cartridge header (on ROM address 0x149): %02x\n", rom_data[0x149]);
		return CART_READ_ERROR;
	}

	cart->rom              = rom_buffer;
	cart->memmap           = memmap;
	cart->cart_type        = cart_type;
	cart->rom_banks        = rom_banks;
	cart->ram_banks        = ram_banks;
	cart->current_ram_bank = 0;
	cart->current_rom_bank = 1;

	return CART_OK;
}

enum CartResult map_addresses(struct Cartridge* cart)
{
	map_first_bank(cart);
	ch_rom_bank(cart, cart->current_rom_bank);
	ch_ram_bank(cart, cart->current_ram_bank);
}

void map_first_bank(struct Cartridge* cart)
{
	for (int page = 0x00; page < 0x40; page++)
	{
		cart->memmap->read_pages[page] = &cart->rom->data[page << 8];
	}
}

void ch_rom_bank(struct Cartridge* cart, int rom_bank)
{
	int base_address = 0x4000 * rom_bank;

	for (int page = 0x40; page < 0x80; page++)
	{
		cart->memmap->read_pages[page] = &cart->rom->data[base_address + ((page - 0x40) << 8)] - base_address;
	}
}

void ch_ram_bank(struct Cartridge* cart, int ram_bank)
{

}