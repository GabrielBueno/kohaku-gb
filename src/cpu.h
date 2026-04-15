#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include "memory.h"

enum cpu_result {
    CPU_OK  = 0,
    CPU_ERR = 1,
};

struct cpu {
    struct mem *mem;
    uint8_t interrupt_enable;
};

enum cpu_result cpu_init(struct cpu *cpu, struct mem *mem);
enum cpu_result cpu_close(struct cpu *cpu);

void cpu_ioreg_interrupt_enable_write(struct cpu *cpu, uint8_t value);
uint8_t cpu_ioreg_interrupt_enable_read(struct cpu *cpu);

#endif