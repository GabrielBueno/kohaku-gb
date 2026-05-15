#include "timer.h"

#include <stddef.h>
#include <assert.h>
#include "log.h"

void timer_init(struct timer *timer, struct interrupt *interrupt) {
    assert(timer != NULL);
    assert(interrupt != NULL);

    timer->interrupt = interrupt;
    timer->counter   = 0;
    timer->tac       = 0;
    timer->tima      = 0;
    timer->tma       = 0;
    timer->tima_overflow = 0;
}

void timer_tick(struct timer *timer, int cycles) {
    uint8_t tac = timer->tac;

    if (!(tac & 0b100)) {
        timer->counter += cycles;
        return;
    }

    if (timer->tima_overflow) {
        timer->tima_overflow = 0;
        timer->tima = timer->tma;
        interrupt_request(timer->interrupt, INTERRUPT_TIMER, INTERRUPT_REQUESTED);
    }

    static uint16_t counter_tima_masks[] = { 1<<9, 1<<3, 1<<5, 1<<7 };

    uint8_t tima       = timer->tima;
    uint8_t tma        = timer->tma;
    uint16_t counter   = timer->counter;
    uint16_t tima_mask = counter_tima_masks[tac & 0b11];

    for (int t = 0; t < cycles; t++) {
        uint16_t counter_prev = counter;
        counter++;

        uint8_t was_high = counter_prev & tima_mask;
        uint8_t is_high  = counter      & tima_mask;

        // DEBUG("counting %d...", timer->counter);

        if (was_high && !is_high) {
            if (tima == 0xff) {
                timer->tima_overflow = 1;
                timer->tima = 0;
                tima = 0;
            } else {
                tima++;
            }
        }
    }

    timer->counter = counter;
    timer->tima    = tima;
}

void timer_reg_div_write(struct timer *timer, uint8_t value) {
    timer->counter = 0;
}

uint8_t timer_reg_div_read(struct timer *timer) {
    return (timer->counter & 0xff00) >> 8;
}

void timer_reg_tima_write(struct timer *timer, uint8_t value) {
    timer->tima = value;
}

uint8_t timer_reg_tima_read(struct timer *timer) {
    return timer->tima;
}

void timer_reg_tma_write(struct timer *timer, uint8_t value) {
    timer->tma = value;
}

uint8_t timer_reg_tma_read(struct timer *timer) {
    return timer->tma;
}

void timer_reg_tac_write(struct timer *timer, uint8_t value) {
    timer->tac = value & 0x07;
}

uint8_t timer_reg_tac_read(struct timer *timer) {
    return timer->tac;
}