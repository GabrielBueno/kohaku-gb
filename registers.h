#ifndef REGISTERS_H
#define REGISTERS_H

#include "memory.h"
#include "joypad.h"

#define REG_JOYP_ADDR                0xff00
#define REG_JOYP_MASK_SELECT_BUTTONS 0x20
#define REG_JOYP_MASK_SELECT_DPAD    0x10

struct registers {
    struct joypad* joypad;
    struct write_device write_device;
    struct memmap* memmap;
};

void reg_init(struct registers* registers, struct memmap* memmap, struct joypad* joypad);

#endif