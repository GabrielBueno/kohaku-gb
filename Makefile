all:
	gcc \
		-g -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-parameter -Wno-unused-argument \
		tests.c main.c cartridge.c memory.c -o \
		kohaku