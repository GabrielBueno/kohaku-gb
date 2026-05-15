#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <stdint.h>

enum interrupt_result {
    INTERRUPT_OK    = 0,
    INTERRUPT_ERROR = 1,
};

enum interrupt_flag {
    INTERRUPT_VBLANK = 1 << 0,
    INTERRUPT_LCD    = 1 << 1,
    INTERRUPT_TIMER  = 1 << 2,
    INTERRUPT_SERIAL = 1 << 3,
    INTERRUPT_JOYPAD = 1 << 4,
};

enum interrupt_request {
    INTERRUPT_NOT_REQUESTED = 0,
    INTERRUPT_REQUESTED     = 1,
};

enum interrupt_enable_state {
    INTERRUPT_DISABLED = 0,
    INTERRUPT_ENABLED  = 1,
};

struct interrupt {
    uint8_t master_enable;
    uint8_t enabled;
    uint8_t requested;
};

enum interrupt_result interrupt_init(struct interrupt *interrupt);

void interrupt_enable(struct interrupt *interrupt, enum interrupt_flag flag, enum interrupt_enable_state state);
void interrupt_request(struct interrupt *interrupt, enum interrupt_flag flag, enum interrupt_request req);

void interrupt_reg_ie_write(struct interrupt *interrupt, uint8_t value);
void interrupt_reg_if_write(struct interrupt *interrupt, uint8_t value);

uint8_t interrupt_reg_ie_read(struct interrupt *interrupt);
uint8_t interrupt_reg_if_read(struct interrupt *interrupt);

#endif