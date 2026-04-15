#include <stdio.h>
#include <stdint.h>

#include "log.h"
#include "file.h"
#include "memory.h"
#include "cartridge.h"

#define ROM "./Roms/pokemon_red.gb"
// #define TEST

#ifdef TEST
#include "tests.h"
#endif

int main() {
	#ifdef TEST
	tests_run();
	return 0;
	#endif

	fprintf(stderr, "Kohaku\n");

	struct file           rom_file;
	struct mem            mem;
	struct cartridge      cart;
	struct cartridge_info cartridge_info;

	file_read(&rom_file, ROM);
	mem_init(&mem);
	cart_init(&cart, rom_file, &mem);
	cart_info(&cart, &cartridge_info);

	DEBUG("CART: %s (%02x)", cartridge_info.title, cartridge_info.type);

	return 0;
}