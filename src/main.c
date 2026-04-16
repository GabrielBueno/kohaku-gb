#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <stdlib.h>

#include "file.h"
#include "gb.h"
#include "log.h"

//
// POKEMON - "./roms/pokemon_red.gb"
// BLARGGS CPU_INSTRS - "./roms/tests/blargg/cpu_instrs/cpu_instrs.gb"
//

#define ROM "./roms/tests/blargg/cpu_instrs/cpu_instrs.gb"
// #define TEST

#ifdef TEST
#include "tests.h"
#endif

struct gb         gb;
struct gb_options gb_opt;

void sighandler(int signal) {
	fprintf(stderr, "SIGINT\n");
	gb_stop(&gb);
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

	if (gb_run(&gb) != GB_OK)
		FATAL("fatal error during execution");

	gb_close(&gb);

	return 0;
}