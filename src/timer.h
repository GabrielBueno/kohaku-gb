#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

struct timer {
    uint8_t div;
    uint8_t tima;
    uint8_t tma;
    uint8_t tac;
};

void timer_init(struct timer *timer);

void timer_reg_div_write(struct timer *timer, uint8_t value);
uint8_t timer_reg_div_read(struct timer *timer);

void timer_reg_tima_write(struct timer *timer, uint8_t value);
uint8_t timer_reg_tima_read(struct timer *timer);

void timer_reg_tma_write(struct timer *timer, uint8_t value);
uint8_t timer_reg_tma_read(struct timer *timer);

void timer_reg_tac_write(struct timer *timer, uint8_t value);
uint8_t timer_reg_tac_read(struct timer *timer);

#endif