#include "timer.h"

#include <stddef.h>
#include <assert.h>

void timer_init(struct timer *timer) {
    assert(timer != NULL);

    timer->div  = 0;
    timer->tac  = 0;
    timer->tima = 0;
    timer->tma  = 0;
}

void timer_reg_div_write(struct timer *timer, uint8_t value) {
    timer->div = 0;
}

uint8_t timer_reg_div_read(struct timer *timer) {
    return timer->div;
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