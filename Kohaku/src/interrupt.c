#include "interrupt.h"

#include <assert.h>
#include <stddef.h>
#include "log.h"

enum interrupt_result interrupt_init(struct interrupt *interrupt) {
    assert(interrupt != NULL);

    interrupt->master_enable = 0;
    interrupt->enabled       = 0;
    interrupt->requested     = 0;

    return INTERRUPT_OK;
}

void interrupt_enable(struct interrupt *interrupt, enum interrupt_flag flag, enum interrupt_enable_state state) {
    assert(interrupt != NULL);

    interrupt->enabled = (interrupt->enabled & (~flag)) | ((state == INTERRUPT_ENABLED) * flag);
}

void interrupt_request(struct interrupt *interrupt, enum interrupt_flag flag, enum interrupt_request req) {
    assert(interrupt != NULL);

    interrupt->requested = (interrupt->requested & (~flag)) | ((req == INTERRUPT_REQUESTED) * flag);
}

void interrupt_reg_ie_write(struct interrupt *interrupt, uint8_t value) {
    interrupt->enabled = value & 0x1f;
}

void interrupt_reg_if_write(struct interrupt *interrupt, uint8_t value) {
    interrupt->requested = value & 0x1f;
}

uint8_t interrupt_reg_ie_read(struct interrupt *interrupt) {
    return interrupt->enabled;
}

uint8_t interrupt_reg_if_read(struct interrupt *interrupt) {
    return interrupt->requested;
}