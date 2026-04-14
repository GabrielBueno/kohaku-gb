#ifndef JOYPAD_H
#define JOYPAD_H

#include "memory.h";

enum joypad_button {
    JOYPAD_BTN_A      = 1 << 0,
    JOYPAD_BTN_B      = 1 << 1,
    JOYPAD_BTN_SELECT = 1 << 2,
    JOYPAD_BTN_START  = 1 << 3,

    JOYPAD_DPAD_RIGHT  = 1 << 4,
    JOYPAD_DPAD_LEFT   = 1 << 5,
    JOYPAD_DPAD_UP     = 1 << 6,
    JOYPAD_DPAD_DOWN   = 1 << 7,
};

enum joypad_state {
    JOYPAD_PRESS   = 1,
    JOYPAD_RELEASE = 2,
};

struct joypad {
    struct write_device write_device;
    struct memmap* memmap;
    struct registers* registers;
    uint8_t button_mask;
};

void joypad_init(struct joypad* joypad, struct memmap* memmap, struct registers* registers);
void joypad_set(struct joypad* joypad, enum joypad_button btn, enum joypad_state state);

#endif