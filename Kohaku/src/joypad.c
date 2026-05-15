#include "joypad.h"

#include <stddef.h>
#include <assert.h>
#include "ioregs.h"

void joypad_init(struct joypad* joypad) {
    assert(joypad != NULL);

    joypad->button_mask = 0xff;
    joypad->selection   = JOYPAD_SEL_BTN;
}

void joypad_set(struct joypad* joypad, enum joypad_button btn, enum joypad_state state) {
    assert(joypad != NULL);

    uint8_t mask = ~joypad->button_mask;

    mask = (mask & ~btn) | (btn * (state == JOYPAD_PRESS));

    joypad->button_mask = ~mask;
}

void joypad_ioreg_joyp_write(struct joypad *joypad, uint8_t value) {
    joypad->selection = value & 0x30;
}

uint8_t joypad_ioreg_joyp_read(struct joypad *joypad) {
    if (joypad->selection == JOYPAD_SEL_BTN)
        return joypad->button_mask & 0x0f;

    if (joypad->selection == JOYPAD_SEL_DPAD)
        return joypad->button_mask >> 4;

    return 0x0f;
}