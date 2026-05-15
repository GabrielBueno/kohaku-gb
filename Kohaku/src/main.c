#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <stdlib.h>

#include "log.h"
#include "file.h"
#include "cartridge.h"
#include "gb.h"
#include "window.h"

//
// POKEMON - "./roms/pokemon_red.gb"
// BLARGGS CPU_INSTRS - "./roms/tests/blargg/cpu_instrs/cpu_instrs.gb"
//

// #define ROM "./roms/tests/blargg/cpu_instrs/cpu_instrs.gb"
#define ROM "./roms/pokemon_red.gb"
// #define TEST

#ifdef TEST
#include "tests.h"
#endif

struct gb         gb;
struct window     window;
struct gb_options gb_opt;

void sighandler(int signal) {
	fprintf(stderr, "SIGINT\n");
	window.should_quit = 1;
}

int main() {
	srand(time(NULL));
    signal(SIGINT, sighandler);

	#ifdef TEST
	tests_run();
	return 0;
	#endif

	fprintf(stderr, "Kohaku\n");

	if (file_read(&gb_opt.rom_file, ROM) != FILE_OK)
		FATAL("failed to read file %s", ROM);

	if (gb_init(&gb, &gb_opt) != GB_OK)
		FATAL("failed to initialize gb");

	cart_info_print(&gb.cart, &gb.cart_info);

	if (window_init(&window, &gb, WINDOW_DEFAULT) != WINDOW_OK)
		FATAL("failed to initialize window");
	
	while (!window.should_quit) {
		int cycles = 0;

		while (cycles < 70224)
			cycles += gb_tick(&gb);

		window_poll_events(&window);

		if (gb.ppu.vblank_ready_to_render) {
			window_render(&window);
			gb.ppu.vblank_ready_to_render = 0;
		}
	}

	gb_close(&gb);
	window_close(&window);

	return 0;
}