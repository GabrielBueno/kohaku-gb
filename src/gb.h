#ifndef GAMEBOY_H
#define GAMEBOY_H

#include "memory.h"
#include "cpu.h"
#include "ppu.h"
#include "ram.h"
#include "cartridge.h"
#include "joypad.h"
#include "ioregs.h"
#include "interrupt.h"
#include "timer.h"

enum gb_result {
    GB_OK  = 0,
    GB_ERR = 1,
};

struct gb {
    struct cartridge cart;
    struct cartridge_info cart_info;
    struct cpu cpu;
    struct ram ram;
    struct ppu ppu;
    struct mem mem;
    struct joypad joypad;
    struct ioregs ioregs;
    struct interrupt interrupt;
    struct timer timer;
};

struct gb_options {
    struct file rom_file;
};

enum gb_result gb_init(struct gb *gb, struct gb_options *options);
enum gb_result gb_close(struct gb *gb);

#endif