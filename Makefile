all:
	gcc \
		-g -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-parameter -Wno-unused-argument \
		./src/tests.c ./src/file.c ./src/gb.c ./src/joypad.c ./src/ioregs.c ./src/cartridge.c ./src/memory.c ./src/ram.c ./src/ppu.c ./src/cpu.c ./src/main.c -o \
		kohaku