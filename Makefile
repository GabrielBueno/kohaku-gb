all:
	gcc \
		-g -O3 -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-parameter -Wno-unused-argument -Wno-unused-function \
		-lSDL3 -lSDL3_ttf \
		./src/tests.c ./src/file.c ./src/gb.c ./src/joypad.c ./src/ioregs.c ./src/cartridge.c ./src/memory.c ./src/ram.c ./src/ppu.c ./src/cpu.c ./src/interrupt.c ./src/timer.c ./src/serial.c ./src/window.c ./src/main.c -o \
		kohaku