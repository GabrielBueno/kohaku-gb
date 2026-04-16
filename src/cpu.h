#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include "memory.h"
#include "interrupt.h"

enum cpu_result {
    CPU_OK  = 0,
    CPU_ERR = 1,
};

struct cpu {
    struct mem *mem;
    struct interrupt *interrupt;

    uint16_t SP;
    uint16_t PC;

    uint8_t A;
    uint8_t F;
    uint8_t B;
    uint8_t C;
    uint8_t D;
    uint8_t E;
    uint8_t H;
    uint8_t L;

    uint8_t halt;
    uint8_t stop;

    uint8_t opcode;
};

enum cpu_result cpu_init(struct cpu *cpu, struct mem *mem, struct interrupt *interrupt);
enum cpu_result cpu_close(struct cpu *cpu);

int cpu_tick(struct cpu *cpu, int cycles);

#endif