#include "joypad.h"

#include <stddef.h>

#include "registers.h"

void joypad_init(struct joypad* joypad, struct memmap* memmap, struct registers* registers) {
    assert(memmap    != NULL);
    assert(joypad    != NULL);
    assert(registers != NULL);

    joypad->memmap    = memmap;
    joypad->registers = registers;
}

void joypad_set(struct joypad* joypad, enum joypad_button btn, enum joypad_state state) {
    assert(joypad != NULL);

    joypad->button_mask = (joypad->button_mask & ~btn) | (btn * (state == JOYPAD_PRESS));

    uint8_t joyp = read8(joypad->memmap, REG_JOYP_ADDR);

    write8(joypad->memmap, REG_JOYP_ADDR, joyp & 0x30);
}