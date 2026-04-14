#include "registers.h"

#include <stddef.h>

static void write_reg(void* ctx, uint16_t addr, uint8_t value);
static void write_joyp(struct registers* registers, uint8_t value);

void reg_init(struct registers* registers, struct memmap* memmap, struct joypad* joypad) {
    assert(registers != NULL);
    assert(memmap != NULL);
    assert(joypad != NULL);

    registers->memmap = memmap;
    registers->joypad = joypad;
    registers->write_device.target = registers;
    registers->write_device.write  = write_reg;
}

static void write_reg(void* ctx, uint16_t addr, uint8_t value) {
    struct registers* registers = (struct registers*) ctx;

    if (addr == REG_JOYP_ADDR)
        write_joyp(registers, value);
}

static void write_joyp(struct registers* registers, uint8_t value) {
    assert(registers != NULL);

    uint8_t buttons       = registers->joypad->button_mask;
    uint8_t masked_value  = value & 0x30;
    uint8_t masked_button = 0xf;

    switch (value & 0x30) {
    case REG_JOYP_MASK_SELECT_BUTTONS:
        masked_button = ~(buttons & 0x0f);
        break;

    case REG_JOYP_MASK_SELECT_DPAD:
        masked_button = ~(buttons >> 4);
        break;
    }

    set8(registers->memmap, REG_JOYP_ADDR, masked_value & masked_button);
}