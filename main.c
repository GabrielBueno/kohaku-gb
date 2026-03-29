#include <stdio.h>
#include <stdint.h>

#include "io.h"

#define ROM "./Roms/tetris.gb"

int main()
{
	fprintf(stderr, "Kohaku\n");

	struct Buffer rom_buffer;

	ReadFile(ROM, &rom_buffer);

	

	return 0;
}