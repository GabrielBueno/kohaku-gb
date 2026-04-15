#ifndef GAMEBOY_H
#define GAMEBOY_H

#include "cartridge.h"
#include "memory.h"
#include "joypad.h"
#include "ioregs.h"

enum gb_result {
    GB_OK  = 0,
    GB_ERR = 1,
};

struct gb {
    struct cartridge cart;
    struct cartridge_info cart_info;
    struct mem mem;
    struct joypad joypad;
    struct ioregs ioregs;
};

struct gb_options {
    struct file rom_file;
};

enum gb_result gb_init(struct gb *gb, struct gb_options *options);
enum gb_result gb_close(struct gb *gb);

#endif