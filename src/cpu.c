#include "cpu.h"

#include <stddef.h>
#include <assert.h>

enum cpu_result cpu_init(struct cpu *cpu, struct mem *mem) {
    assert(cpu != NULL);
    assert(mem != NULL);

    cpu->mem              = mem;
    cpu->interrupt_enable = 0;

    return CPU_OK;
}

enum cpu_result cpu_close(struct cpu *cpu) {
    return CPU_OK;
}

void cpu_ioreg_interrupt_enable_write(struct cpu *cpu, uint8_t value) {
    cpu->interrupt_enable = value;
}

uint8_t cpu_ioreg_interrupt_enable_read(struct cpu *cpu) {
    return cpu->interrupt_enable;
}