#include <stdio.h>
#include <stdint.h>

#define ROM "./Roms/tetris.gb"
#define TEST
#define MEMMAP_CHECK_NULL

#ifdef TEST
#include "tests.h"
#endif

int main() {
	#ifdef TEST
	tests_run();
	return 0;
	#endif

	fprintf(stderr, "Kohaku\n");

	return 0;
}