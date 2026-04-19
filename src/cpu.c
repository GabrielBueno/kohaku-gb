#include "cpu.h"

#include <stddef.h>
#include <assert.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "macros.h"
#include "log.h"

#define BIT_7(BYTE) ((BYTE)&0x80)
#define BIT_0(BYTE) ((BYTE)&0x01)
#define BIT_N(BYTE, N) ((BYTE)&(0x01 << (N)))

#define BYTE_FLAG_SET(FLAG, MASK, COND) COND ? (FLAG |= MASK) : (FLAG &= ~MASK)
#define BYTE_SET_BIT(BYTE, N) BYTE = (((BYTE) | (0x01 << N)))
#define BYTE_CLEAR_BIT(BYTE, N) BYTE = (((BYTE) & (~(0x01 << N))))

#define BYTE_SWAP(BYTE) BYTE = ((BYTE&0x0f) << 4) | ((BYTE&0xf0) >> 4);

#define BYTE_ROTATE_LEFT(BYTE)  BYTE = (((BYTE) << 1) | (((BYTE)&0x80) >> 7))
#define BYTE_ROTATE_RIGHT(BYTE) BYTE = (((BYTE) >> 1) | (((BYTE)&0x01) << 7))

#define BYTE_ROTATE_LEFT_CARRY(BYTE, CARRY)  BYTE = (((BYTE) << 1) | (!!(CARRY)))
#define BYTE_ROTATE_RIGHT_CARRY(BYTE, CARRY) BYTE = (((BYTE) >> 1) | ((!!(CARRY)) << 7))

#define BYTE_SHIFT_LEFT(BYTE)  BYTE = (BYTE << 1)
#define BYTE_SHIFT_RIGHT(BYTE) BYTE = (BYTE >> 1)
#define BYTE_SHIFT_LEFT_KEEP_BIT(BYTE)  BYTE = ((BYTE&0x01) | (BYTE << 1))
#define BYTE_SHIFT_RIGHT_KEEP_BIT(BYTE) BYTE = ((BYTE&0x80) | (BYTE >> 1))

#define CPU_AF(CPU) (BYTE_16BIT(CPU->A, CPU->F))
#define CPU_BC(CPU) (BYTE_16BIT(CPU->B, CPU->C))
#define CPU_DE(CPU) (BYTE_16BIT(CPU->D, CPU->E))
#define CPU_HL(CPU) (BYTE_16BIT(CPU->H, CPU->L))

#define CPU_SET_AF(CPU, WORD) CPU->A = ((WORD) >> 8); CPU->F = ((WORD) & 0xff)
#define CPU_SET_BC(CPU, WORD) CPU->B = ((WORD) >> 8); CPU->C = ((WORD) & 0xff)
#define CPU_SET_DE(CPU, WORD) CPU->D = ((WORD) >> 8); CPU->E = ((WORD) & 0xff)
#define CPU_SET_HL(CPU, WORD) CPU->H = ((WORD) >> 8); CPU->L = ((WORD) & 0xff)

#define CPU_ZERO(CPU)    ((CPU->F & CPU_FLAG_Z) > 0)
#define CPU_CARRY(CPU)   ((CPU->F & CPU_FLAG_C) > 0)
#define CPU_H_CARRY(CPU) ((CPU->F & CPU_FLAG_H) > 0)
#define CPU_N(CPU)       ((CPU->F & CPU_FLAG_N) > 0)

#define CPU_FLAG_Z 0x80
#define CPU_FLAG_N 0x40
#define CPU_FLAG_H 0x20
#define CPU_FLAG_C 0x10

#define CPU_FLAG_SET(FLAG, MASK, COND) ((COND) ? (FLAG |= MASK) : (FLAG &= ~MASK))
#define CPU_SET_Z(CPU, COND) CPU_FLAG_SET(CPU->F, CPU_FLAG_Z, COND)
#define CPU_SET_N(CPU, COND) CPU_FLAG_SET(CPU->F, CPU_FLAG_N, COND)
#define CPU_SET_H(CPU, COND) CPU_FLAG_SET(CPU->F, CPU_FLAG_H, COND)
#define CPU_SET_C(CPU, COND) CPU_FLAG_SET(CPU->F, CPU_FLAG_C, COND)

#define CPU_SET_H_SUM(CPU, LEFT, RIGHT) CPU_SET_H(CPU, (((0x0f&LEFT) + (0x0f&RIGHT)) & 0xf0))
#define CPU_SET_H_SUB(CPU, LEFT, RIGHT) CPU_SET_H(CPU, ((0x0f&LEFT) - (0x0f&RIGHT)) & 0xf0)
#define CPU_SET_C_SUM(CPU, LEFT, RIGHT) CPU_SET_C(CPU, ((((uint16_t)LEFT) + ((uint16_t)RIGHT)) & 0xff00))
#define CPU_SET_C_SUB(CPU, LEFT, RIGHT) CPU_SET_C(CPU, ((uint16_t)LEFT - (uint16_t)RIGHT) & 0xff00)

#define CPU_SET_H_SUMC(CPU, LEFT, RIGHT, C) CPU_SET_H(CPU, (((0x0f&LEFT) + (0x0f&RIGHT) + (0x0f&C)) & 0xf0))
#define CPU_SET_H_SUBC(CPU, LEFT, RIGHT, C) CPU_SET_H(CPU, (((0x0f&LEFT)-(0x0f&RIGHT)-(0x0f&C)) & 0xf0))
#define CPU_SET_C_SUMC(CPU, LEFT, RIGHT, C) CPU_SET_C(CPU, ((((uint16_t)LEFT) + ((uint16_t)RIGHT) + ((uint16_t)C)) & 0xff00))
#define CPU_SET_C_SUBC(CPU, LEFT, RIGHT, C) CPU_SET_C(CPU, (((uint16_t)LEFT - (uint16_t)RIGHT - (uint16_t)C) & 0xff00))

#define CPU_SET_H_REG16_SUM(CPU, LEFT, RIGHT) CPU_SET_H(CPU, (((0xfff&(uint16_t)LEFT) + (0xfff&(uint16_t)RIGHT)) & 0xf000))
#define CPU_SET_C_REG16_SUM(CPU, LEFT, RIGHT) CPU_SET_C(CPU, ((((uint32_t)LEFT) + ((uint32_t)RIGHT)) & 0xff0000))

#define CPU_SET_H_REG16_SUM_SIGNED(CPU, LEFT, RIGHT) CPU_SET_H(CPU, (((0xff&(int16_t)LEFT) + (0xff&(int16_t)RIGHT)) & 0xff00))
#define CPU_SET_C_REG16_SUM_SIGNED(CPU, LEFT, RIGHT) CPU_SET_C(CPU, ((((int32_t)LEFT) + ((int32_t)RIGHT)) & 0xff0000))

static int check_interrupt(struct cpu *cpu);
static int exec_next_instr(struct cpu *cpu);

enum cpu_result cpu_init(struct cpu *cpu, struct mem *mem, struct interrupt *interrupt) {
    assert(cpu != NULL);
    assert(mem != NULL);
    assert(interrupt != NULL);

    cpu->mem       = mem;
    cpu->interrupt = interrupt;

    cpu->A   = 0x01;
    cpu->F   = 0xb0;
    cpu->B   = 0x00;
    cpu->C   = 0x13;
    cpu->D   = 0x00;
    cpu->E   = 0xd8;
    cpu->H   = 0x01;
    cpu->L   = 0x4d;

    cpu->PC  = 0x0100;
    cpu->SP  = 0xfffe;

    return CPU_OK;
}

enum cpu_result cpu_close(struct cpu *cpu) {
    return CPU_OK;
}

int cpu_tick(struct cpu *cpu) {
    int taken = 0;
    
    if ((taken = check_interrupt(cpu)) > 0)
        return taken;

    return exec_next_instr(cpu);
}

static int check_interrupt(struct cpu *cpu) {
    uint8_t requested = 0x1f & cpu->interrupt->requested;
    uint8_t enabled   = 0x1f & cpu->interrupt->enabled;
    uint8_t to_exec   = requested & enabled;

    if (cpu->halt && to_exec)
        cpu->halt = 0;

    if (cpu->interrupt->master_enable == 0)
        return 0;

    if (to_exec == 0)
        return 0;

    // gets the least significant bit set on `to_exec`.
    to_exec &= -to_exec;

    static const uint16_t call_addrs[] = { 0x40, 0x48, 0x50, 0x58, 0x60 };
    int idx;

    #if defined(_MSC_VER)
        unsigned long _idx;
        _BitScanForward(&_idx, to_exec);

        idx = (int)_idx;
    #else
        idx = __builtin_ctz(to_exec);
    #endif

    assert(idx >= 0 && idx < 5);

    uint16_t call_addr = call_addrs[idx];

    cpu->SP -= 2;
    mem_write8(cpu->mem, cpu->SP + 1, (cpu->PC>>8) & 0xff);
    mem_write8(cpu->mem, cpu->SP, cpu->PC&0xff);

    cpu->PC                       = call_addr;
    cpu->interrupt->master_enable = 0;
    cpu->interrupt->requested    &= ~to_exec;

    return 20;
}

//
// INSTRUCTIONS
//

static int exec_next_instr(struct cpu *cpu) {
    // struct cpu prev_state = cpu;

    // uint16_t pc     = cpu->PC;
    // uint8_t  opcode = mem_read8(cpu->mem, pc);
    // int      cycles = 0;

    // // emu->last_PC = pc;
    // emu->opcode_history[emu->cur_opcode_hist].opcode = opcode;
    // emu->opcode_history[emu->cur_opcode_hist].PC     = pc;
    // emu->cur_opcode_hist = (emu->cur_opcode_hist + 1);

    // if (emu->cur_opcode_hist >= OPCODE_HISTORY) {
    //     emu->cur_opcode_hist = 0;
    // }

    if (cpu->halt)
        return 4;
    
    uint16_t pc     = cpu->PC;
    uint8_t  opcode = mem_read8(cpu->mem, pc);

    switch (opcode) {
    case 0x00: { //NOP {
        cpu->PC += 1;
        return 4;
    }

    case 0x01: { //LD BC, {16bit}
        cpu->B = mem_read8(cpu->mem, pc+2);
        cpu->C = mem_read8(cpu->mem, pc+1);
        cpu->PC += 3;
        return 12;
    }

    case 0x02: { //LD (BC), A
        mem_write8(cpu->mem, CPU_BC(cpu), cpu->A);
        cpu->PC += 1;
        return 8;
    }

    case 0x03: { //INC BC
        CPU_SET_BC(cpu, CPU_BC(cpu)+1);
        cpu->PC += 1;
        return 8;
    }

    case 0x04: { //INC B
        CPU_SET_N(cpu, 0);
        CPU_SET_H_SUM(cpu, cpu->B, 1);

        cpu->B  += 1;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->B == 0);
        return 4;
    }

    case 0x05: { //DEC B
        CPU_SET_N(cpu, 1);
        CPU_SET_H_SUB(cpu, cpu->B, 1);

        cpu->B  -= 1;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->B == 0);
        return 4;
    }

    case 0x06: { //LD B, {8bit}
        cpu->B   = mem_read8(cpu->mem, pc+1);
        cpu->PC += 2;
        return 8;
    }

    case 0x07: { //RLCA
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_Z(cpu, 0);
        CPU_SET_C(cpu, BIT_7(cpu->A));
        BYTE_ROTATE_LEFT(cpu->A);
        cpu->PC += 1;
        return 4;
    }

    case 0x08: { //LD ({16bit}), SP
        uint16_t addr = mem_read16(cpu->mem, pc+1);

        mem_write8(cpu->mem, addr,   cpu->SP&0xff);
        mem_write8(cpu->mem, addr+1, cpu->SP>>8);

        cpu->PC += 3;
        return 20;
    }

    case 0x09: { //ADD HL, BC
        uint16_t hl    = CPU_HL(cpu);
        uint16_t reg16 = CPU_BC(cpu);

        CPU_SET_N(cpu, 0);
        CPU_SET_H_REG16_SUM(cpu, hl, reg16);
        CPU_SET_C_REG16_SUM(cpu, hl, reg16);
        CPU_SET_HL(cpu, hl+reg16);

        cpu->PC += 1;
        return 8;
    }

    case 0x0a: { //LD A, (BC)
        cpu->A   = mem_read8(cpu->mem, CPU_BC(cpu));
        cpu->PC += 1;
        return 8;
    }

    case 0x0b: { //DEC BC
        CPU_SET_BC(cpu, CPU_BC(cpu)-1);
        cpu->PC += 1;
        return 8;
    }

    case 0x0c: { //INC C
        CPU_SET_N(cpu, 0);
        CPU_SET_H_SUM(cpu, cpu->C, 1);

        cpu->C  += 1;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->C == 0);
        return 4;
    }

    case 0x0d: { //DEC C
        CPU_SET_N(cpu, 1);
        CPU_SET_H_SUB(cpu, cpu->C, 1);

        cpu->C  -= 1;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->C == 0);
        return 4;
    }
    
    case 0x0e: { //LD C, {8bit}
        cpu->C   = mem_read8(cpu->mem, pc+1);
        cpu->PC += 2;
        return 8;
    }

    case 0x0f: { //RRCA
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_Z(cpu, 0);
        CPU_SET_C(cpu, cpu->A & 0x01);
        BYTE_ROTATE_RIGHT(cpu->A);
        cpu->PC += 1;
        return 4;
    }

    case 0x10: { //STOP
        uint8_t mem = mem_read8(cpu->mem, pc+1);

        if (mem == 0x00) {
            cpu->stop = 1;
        }

        cpu->PC += 2;
        return 4;
    }

    case 0x11: { //LD DE, {16bit}
        cpu->D = mem_read8(cpu->mem, pc+2);
        cpu->E = mem_read8(cpu->mem, pc+1);
        cpu->PC += 3;
        return 12;
    }

    case 0x12: { //LD (DE), A
        mem_write8(cpu->mem, CPU_DE(cpu), cpu->A);
        cpu->PC += 1;
        return 8;
    }

    case 0x13: { //INC DE
        CPU_SET_DE(cpu, CPU_DE(cpu)+1);
        cpu->PC += 1;
        return 8;
    }

    case 0x14: { //INC D
        CPU_SET_N(cpu, 0);
        CPU_SET_H_SUM(cpu, cpu->D, 1);

        cpu->D  += 1;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->D == 0);
        return 4;
    }

    case 0x15: { //DEC D
        CPU_SET_N(cpu, 1);
        CPU_SET_H_SUB(cpu, cpu->D, 1);

        cpu->D  -= 1;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->D == 0);
        return 4;
    }

    case 0x16: { //LD D, {8bit}
        cpu->D   = mem_read8(cpu->mem, pc+1);
        cpu->PC += 2;
        return 8;
    }

    case 0x17: { //RLA
        uint8_t carry = CPU_CARRY(cpu);

        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_Z(cpu, 0);
        CPU_SET_C(cpu, cpu->A & 0x80);
        BYTE_ROTATE_LEFT_CARRY(cpu->A, carry);
        cpu->PC += 1;

        return 4;
    }

    case 0x18: { //JR {8bit}
        cpu->PC = (uint16_t)(cpu->PC + 2 + (int16_t)((int8_t)mem_read8(cpu->mem, pc+1)));
        return 12;
    }

    case 0x19: { //ADD HL, DE
        uint16_t hl    = CPU_HL(cpu);
        uint16_t reg16 = CPU_DE(cpu);

        CPU_SET_N(cpu, 0);
        CPU_SET_H_REG16_SUM(cpu, hl, reg16);
        CPU_SET_C_REG16_SUM(cpu, hl, reg16);
        CPU_SET_HL(cpu, hl+reg16);

        cpu->PC += 1;
        return 8;
    }

    case 0x1a: { //LD A, (DE)
        cpu->A   = mem_read8(cpu->mem, CPU_DE(cpu));
        cpu->PC += 1;
        return 8;
    }

    case 0x1b: { //DEC DE
        CPU_SET_DE(cpu, CPU_DE(cpu)-1);
        cpu->PC += 1;
        return 8;
    }

    case 0x1c: { //INC E
        CPU_SET_N(cpu, 0);
        CPU_SET_H_SUM(cpu, cpu->E, 1);

        cpu->E  += 1;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->E == 0);
        return 4;
    }

    case 0x1d: { //DEC E
        CPU_SET_N(cpu, 1);
        CPU_SET_H_SUB(cpu, cpu->E, 1);

        cpu->E  -= 1;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->E == 0);
        return 4;
    }

    case 0x1e: { //LD E, {8bit}
        cpu->E   = mem_read8(cpu->mem, pc+1);
        cpu->PC += 2;
        return 8;
    }

    case 0x1f: { //RRA
        uint8_t carry = CPU_CARRY(cpu);

        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_Z(cpu, 0);
        CPU_SET_C(cpu, cpu->A & 0x01);
        BYTE_ROTATE_RIGHT_CARRY(cpu->A, carry);
        cpu->PC += 1;
        return 4;
    }

    case 0x20: { //JR NZ, {8bit}
        if (!CPU_ZERO(cpu)) {
            cpu->PC = (uint16_t)(cpu->PC + 2 + (int16_t)((int8_t)mem_read8(cpu->mem, pc+1)));
            return 12;
        } else {
            cpu->PC += 2;
            return 8;
        }
    }

    case 0x21: { //LD HL, {16bit}
        cpu->H = mem_read8(cpu->mem, pc+2);
        cpu->L = mem_read8(cpu->mem, pc+1);
        cpu->PC += 3;
        return 12;
    }

    case 0x22: { //LDI (HL), A
        uint16_t hl  = CPU_HL(cpu);
        uint16_t hld = hl+1;

        mem_write8(cpu->mem, hl, cpu->A);
        cpu->H = (hld & 0xff00) >> 8;
        cpu->L = hld & 0x00ff;

        cpu->PC += 1;
        return 8;
    }

    case 0x23: { //INC HL
        CPU_SET_HL(cpu, CPU_HL(cpu)+1);
        cpu->PC += 1;
        return 8;
    }

    case 0x24: { //INC H
        CPU_SET_N(cpu, 0);
        CPU_SET_H_SUM(cpu, cpu->H, 1);

        cpu->H  += 1;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->H == 0);
        return 4;
    }
    
    case 0x25: { //DEC H
        CPU_SET_N(cpu, 1);
        CPU_SET_H_SUB(cpu, cpu->H, 1);

        cpu->H  -= 1;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->H == 0);
        return 4;
    }

    case 0x26: { //LD H, {8bit}
        cpu->H   = mem_read8(cpu->mem, pc+1);
        cpu->PC += 2;
        return 8;
    }

    case 0x27: { //DAA
        if (CPU_N(cpu)) {
            if (CPU_CARRY(cpu)) {
                cpu->A -= 0x60;
            }

            if (CPU_H_CARRY(cpu)) {
                cpu->A -= 0x06;
            }
        } else {
            if (CPU_CARRY(cpu) || cpu->A > 0x99) {
                cpu->A += 0x60;
                CPU_SET_C(cpu, 1);
            }

            if (CPU_H_CARRY(cpu) || (cpu->A & 0x0f) > 0x09) {
                cpu->A += 0x06;
            }
        }

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_H(cpu, 0);
        cpu->PC += 1;
        
        return 4;
    }

    case 0x28: { //JR Z, {8bit}
        if (CPU_ZERO(cpu)) {
            cpu->PC = (uint16_t)(cpu->PC + 2 + (int16_t)((int8_t)mem_read8(cpu->mem, pc+1)));
            return 12;
        } else {
            cpu->PC += 2;
            return 8;
        }
    }

    case 0x29: { //ADD HL, HL
        uint16_t hl    = CPU_HL(cpu);
        uint16_t reg16 = CPU_HL(cpu);

        CPU_SET_N(cpu, 0);
        CPU_SET_H_REG16_SUM(cpu, hl, reg16);
        CPU_SET_C_REG16_SUM(cpu, hl, reg16);
        CPU_SET_HL(cpu, hl+reg16);

        cpu->PC += 1;
        return 8;
    }

    case 0x2a: { //LDI A, (HL)
        uint16_t hl  = CPU_HL(cpu);
        uint16_t hld = hl+1;

        cpu->A = mem_read8(cpu->mem, hl);
        cpu->H = (hld & 0xff00) >> 8;
        cpu->L = hld & 0x00ff;

        cpu->PC += 1;
        return 8;
    }

    case 0x2b: { //DEC HL
        CPU_SET_HL(cpu, CPU_HL(cpu)-1);
        cpu->PC += 1;
        return 8;
    }

    case 0x2c: { //INC L
        CPU_SET_N(cpu, 0);
        CPU_SET_H_SUM(cpu, cpu->L, 1);

        cpu->L  += 1;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->L == 0);
        return 4;
    }

    case 0x2d: { //DEC L
        CPU_SET_N(cpu, 1);
        CPU_SET_H_SUB(cpu, cpu->L, 1);

        cpu->L  -= 1;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->L == 0);
        return 4;
    }

    case 0x2e: { //LD L, {8bit}
        cpu->L   = mem_read8(cpu->mem, pc+1);
        cpu->PC += 2;
        return 8;
    }

    case 0x2f: { //CPL A
        CPU_SET_N(cpu, 1);
        CPU_SET_H(cpu, 1);

        cpu->A   = ~cpu->A;
        cpu->PC += 1;
        return 4;
    }

    case 0x30: { //JR NC, {8bit}
        if (!CPU_CARRY(cpu)) {
            cpu->PC = (uint16_t)(cpu->PC + 2 + (int16_t)((int8_t)mem_read8(cpu->mem, pc+1)));
            return 12;
        } else {
            cpu->PC += 2;
            return 8;
        }
    }

    case 0x31: { //LD SP, {16bit}
        uint8_t s = mem_read8(cpu->mem, pc+2);
        uint8_t p = mem_read8(cpu->mem, pc+1);

        cpu->SP  = BYTE_16BIT(s, p);
        cpu->PC += 3;
        return 12;
    }
    
    case 0x32: { //LDD (HL), A
        uint16_t hl  = CPU_HL(cpu);
        uint16_t hld = hl-1;

        mem_write8(cpu->mem, hl, cpu->A);
        cpu->H = (hld & 0xff00) >> 8;
        cpu->L = hld & 0x00ff;

        cpu->PC += 1;
        return 8;
    }

    case 0x33: { //INC SP
        cpu->SP += 1;
        cpu->PC += 1;
        return 8;
    }

    case 0x34: { //INC (HL)
        uint16_t hl  = CPU_HL(cpu);
        uint8_t  mem = mem_read8(cpu->mem, hl);

        CPU_SET_N(cpu, 0);
        CPU_SET_H_SUM(cpu, mem, 1);

        mem += 1;
        cpu->PC += 1;

        mem_write8(cpu->mem, hl, mem);
        CPU_SET_Z(cpu, mem == 0);
        return 12;
    }

    case 0x35: { //DEC (HL)
        uint16_t hl  = CPU_HL(cpu);
        uint8_t  mem = mem_read8(cpu->mem, hl);

        CPU_SET_N(cpu, 1);
        CPU_SET_H_SUB(cpu, mem, 1);

        mem -= 1;
        cpu->PC += 1;

        mem_write8(cpu->mem, hl, mem);
        CPU_SET_Z(cpu, mem == 0);
        return 12;
    }

    case 0x36: { //LD (HL), {8bit}
        mem_write8(cpu->mem, CPU_HL(cpu), mem_read8(cpu->mem, pc+1));
        cpu->PC += 2;
        return 12;
    }

    case 0x37: { //SCF
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, 1);
        cpu->PC += 1;
        return 4;
    }

    case 0x38: { //JR C, {8bit}
        if (CPU_CARRY(cpu)) {
            cpu->PC = (uint16_t)(cpu->PC + 2 + (int16_t)((int8_t)mem_read8(cpu->mem, pc+1)));
            return 12;
        } else {
            cpu->PC += 2;
            return 8;
        }
    }

    case 0x39: { //ADD HL, SP
        uint16_t hl    = CPU_HL(cpu);
        uint16_t reg16 = cpu->SP;

        CPU_SET_N(cpu, 0);
        CPU_SET_H_REG16_SUM(cpu, hl, reg16);
        CPU_SET_C_REG16_SUM(cpu, hl, reg16);
        CPU_SET_HL(cpu, hl+reg16);

        cpu->PC += 1;
        return 8;
    }

    case 0x3a: { //LDD A, (HL)
        uint16_t hl  = CPU_HL(cpu);
        uint16_t hld = hl-1;

        cpu->A = mem_read8(cpu->mem, hl);
        cpu->H = (hld & 0xff00) >> 8;
        cpu->L = hld & 0x00ff;

        cpu->PC += 1;
        return 8;
    }

    case 0x3b: { //DEC SP
        cpu->SP -= 1;
        cpu->PC += 1;
        return 8;
    }

    case 0x3c: { //INC A
        CPU_SET_N(cpu, 0);
        CPU_SET_H_SUM(cpu, cpu->A, 1);

        cpu->A  += 1;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x3d: { //DEC A
        CPU_SET_N(cpu, 1);
        CPU_SET_H_SUB(cpu, cpu->A, 1);

        cpu->A  -= 1;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x3e: { //LD A, {8bit}
        cpu->A   = mem_read8(cpu->mem, pc+1);
        cpu->PC += 2;
        return 8;
    }

    case 0x3f: { //CCF
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, !CPU_CARRY(cpu));
        cpu->PC += 1;
        return 4;
    }

    case 0x40: { //LD B, B
        cpu->PC += 1;
        return 4;
    }

    case 0x41: { //LD B, C
        cpu->B = cpu->C;
        cpu->PC += 1;
        return 4;
    }

    case 0x42: { //LD B, D
        cpu->B = cpu->D;
        cpu->PC += 1;
        return 4;
    }

    case 0x43: { //LD B, E
        cpu->B = cpu->E;
        cpu->PC += 1;
        return 4;
    }

    case 0x44: { //LD B, H
        cpu->B = cpu->H;
        cpu->PC += 1;
        return 4;
    }

    case 0x45: { //LD B, L
        cpu->B = cpu->L;
        cpu->PC += 1;
        return 4;
    }

    case 0x46: { //LD B, (HL)
        cpu->B = mem_read8(cpu->mem, CPU_HL(cpu));
        cpu->PC += 1;
        return 8;
    }

    case 0x47: { //LD B, A
        cpu->B = cpu->A;
        cpu->PC += 1;
        return 4;
    }

    case 0x48: { //LD C, B
        cpu->C = cpu->B;
        cpu->PC += 1;
        return 4;
    }

    case 0x49: { //LD C, C
        cpu->C = cpu->C;
        cpu->PC += 1;
        return 4;
    }

    case 0x4a: { //LD C, D
        cpu->C = cpu->D;
        cpu->PC += 1;
        return 4;
    }

    case 0x4b: { //LD C, E
        cpu->C = cpu->E;
        cpu->PC += 1;
        return 4;
    }

    case 0x4c: { //LD C, H
        cpu->C = cpu->H;
        cpu->PC += 1;
        return 4;
    }

    case 0x4d: { //LD C, L
        cpu->C = cpu->L;
        cpu->PC += 1;
        return 4;
    }

    case 0x4e: { //LD C, (HL)
        cpu->C = mem_read8(cpu->mem, CPU_HL(cpu));
        cpu->PC += 1;
        return 8;
    }

    case 0x4f: { //LD C, A
        cpu->C = cpu->A;
        cpu->PC += 1;
        return 4;
    }

    case 0x50: { //LD D, B
        cpu->D = cpu->B;
        cpu->PC += 1;
        return 4;
    }

    case 0x51: { //LD D, C
        cpu->D = cpu->C;
        cpu->PC += 1;
        return 4;
    }

    case 0x52: { //LD D, D
        cpu->D = cpu->D;
        cpu->PC += 1;
        return 4;
    }

    case 0x53: { //LD D, E
        cpu->D = cpu->E;
        cpu->PC += 1;
        return 4;
    }

    case 0x54: { //LD D, H
        cpu->D = cpu->H;
        cpu->PC += 1;
        return 4;
    }

    case 0x55: { //LD D, L
        cpu->D = cpu->L;
        cpu->PC += 1;
        return 4;
    }

    case 0x56: { //LD D, (HL)
        cpu->D = mem_read8(cpu->mem, CPU_HL(cpu));
        cpu->PC += 1;
        return 8;
    }

    case 0x57: { //LD D, A
        cpu->D = cpu->A;
        cpu->PC += 1;
        return 4;
    }

    case 0x58: { //LD E, B
        cpu->E = cpu->B;
        cpu->PC += 1;
        return 4;
    }

    case 0x59: { //LD E, C
        cpu->E = cpu->C;
        cpu->PC += 1;
        return 4;
    }

    case 0x5a: { //LD E, D
        cpu->E = cpu->D;
        cpu->PC += 1;
        return 4;
    }

    case 0x5b: { //LD E, E
        cpu->E = cpu->E;
        cpu->PC += 1;
        return 4;
    }

    case 0x5c: { //LD E, H
        cpu->E = cpu->H;
        cpu->PC += 1;
        return 4;
    }

    case 0x5d: { //LD E, L
        cpu->E = cpu->L;
        cpu->PC += 1;
        return 4;
    }

    case 0x5e: { //LD E, (HL)
        cpu->E = mem_read8(cpu->mem, CPU_HL(cpu));
        cpu->PC += 1;
        return 8;
    }

    case 0x5f: { //LD E, A
        cpu->E = cpu->A;
        cpu->PC += 1;
        return 4;
    }

    case 0x60: { //LD H, B
        cpu->H = cpu->B;
        cpu->PC += 1;
        return 4;
    }

    case 0x61: { //LD H, C
        cpu->H = cpu->C;
        cpu->PC += 1;
        return 4;
    }

    case 0x62: { //LD H, D
        cpu->H = cpu->D;
        cpu->PC += 1;
        return 4;
    }

    case 0x63: { //LD H, E
        cpu->H = cpu->E;
        cpu->PC += 1;
        return 4;
    }

    case 0x64: { //LD H, H
        cpu->H = cpu->H;
        cpu->PC += 1;
        return 4;
    }

    case 0x65: { //LD H, L
        cpu->H = cpu->L;
        cpu->PC += 1;
        return 4;
    }

    case 0x66: { //LD H, (HL)
        cpu->H = mem_read8(cpu->mem, CPU_HL(cpu));
        cpu->PC += 1;
        return 8;
    }

    case 0x67: { //LD H, A
        cpu->H = cpu->A;
        cpu->PC += 1;
        return 4;
    }

    case 0x68: { //LD L, B
        cpu->L = cpu->B;
        cpu->PC += 1;
        return 4;
    }

    case 0x69: { //LD L, C
        cpu->L = cpu->C;
        cpu->PC += 1;
        return 4;
    }

    case 0x6a: { //LD L, D
        cpu->L = cpu->D;
        cpu->PC += 1;
        return 4;
    }

    case 0x6b: { //LD L, E
        cpu->L = cpu->E;
        cpu->PC += 1;
        return 4;
    }

    case 0x6c: { //LD L, H
        cpu->L = cpu->H;
        cpu->PC += 1;
        return 4;
    }

    case 0x6d: { //LD L, L
        cpu->L = cpu->L;
        cpu->PC += 1;
        return 4;
    }

    case 0x6e: { //LD L, (HL)
        cpu->L = mem_read8(cpu->mem, CPU_HL(cpu));
        cpu->PC += 1;
        return 8;
    }

    case 0x6f: { //LD L, A
        cpu->L = cpu->A;
        cpu->PC += 1;
        return 4;
    }

    case 0x70: { //LD (HL), B
        mem_write8(cpu->mem, CPU_HL(cpu), cpu->B);
        cpu->PC += 1;
        return 8;
    }

    case 0x71: { //LD (HL), C
        mem_write8(cpu->mem, CPU_HL(cpu), cpu->C);
        cpu->PC += 1;
        return 8;
    }

    case 0x72: { //LD (HL), D
        mem_write8(cpu->mem, CPU_HL(cpu), cpu->D);
        cpu->PC += 1;
        return 8;
    }

    case 0x73: { //LD (HL), E
        mem_write8(cpu->mem, CPU_HL(cpu), cpu->E);
        cpu->PC += 1;
        return 8;
    }

    case 0x74: { //LD (HL), H
        mem_write8(cpu->mem, CPU_HL(cpu), cpu->H);
        cpu->PC += 1;
        return 8;
    }

    case 0x75: { //LD (HL), L
        mem_write8(cpu->mem, CPU_HL(cpu), cpu->L);
        cpu->PC += 1;
        return 8;
    }

    case 0x76: { //HALT
        cpu->halt = 1;
        cpu->PC  += 1;
        return 4;
    }

    case 0x77: { //LD (HL), A
        mem_write8(cpu->mem, CPU_HL(cpu), cpu->A);
        cpu->PC += 1;
        return 8;
    }

    case 0x78: { //LD A, B
        cpu->A = cpu->B;
        cpu->PC += 1;
        return 4;
    }

    case 0x79: { //LD A, C
        cpu->A = cpu->C;
        cpu->PC += 1;
        return 4;
    }

    case 0x7a: { //LD A, D
        cpu->A = cpu->D;
        cpu->PC += 1;
        return 4;
    }

    case 0x7b: { //LD A, E
        cpu->A = cpu->E;
        cpu->PC += 1;
        return 4;
    }

    case 0x7c: { //LD A, H
        cpu->A = cpu->H;
        cpu->PC += 1;
        return 4;
    }

    case 0x7d: { //LD A, L
        cpu->A = cpu->L;
        cpu->PC += 1;
        return 4;
    }

    case 0x7e: { //LD A, (HL)
        cpu->A = mem_read8(cpu->mem, CPU_HL(cpu));
        cpu->PC += 1;
        return 8;
    }

    case 0x7f: { //LD A, A
        cpu->PC += 1;
        return 4;
    }

    case 0x80: { //ADD A, B
        CPU_SET_N(cpu, 0);
        CPU_SET_C_SUM(cpu, cpu->A, cpu->B);
        CPU_SET_H_SUM(cpu, cpu->A, cpu->B);

        cpu->A  += cpu->B;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x81: { //ADD A, C
        CPU_SET_N(cpu, 0);
        CPU_SET_C_SUM(cpu, cpu->A, cpu->C);
        CPU_SET_H_SUM(cpu, cpu->A, cpu->C);

        cpu->A  += cpu->C;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x82: { //ADD A, D
        CPU_SET_N(cpu, 0);
        CPU_SET_C_SUM(cpu, cpu->A, cpu->D);
        CPU_SET_H_SUM(cpu, cpu->A, cpu->D);

        cpu->A  += cpu->D;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x83: { //ADD A, E
        CPU_SET_N(cpu, 0);
        CPU_SET_C_SUM(cpu, cpu->A, cpu->E);
        CPU_SET_H_SUM(cpu, cpu->A, cpu->E);

        cpu->A  += cpu->E;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x84: { //ADD A, H
        CPU_SET_N(cpu, 0);
        CPU_SET_C_SUM(cpu, cpu->A, cpu->H);
        CPU_SET_H_SUM(cpu, cpu->A, cpu->H);

        cpu->A  += cpu->H;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x85: { //ADD A, L
        CPU_SET_N(cpu, 0);
        CPU_SET_C_SUM(cpu, cpu->A, cpu->L);
        CPU_SET_H_SUM(cpu, cpu->A, cpu->L);

        cpu->A  += cpu->L;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x86: { //ADD A, (HL)
        uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));

        CPU_SET_N(cpu, 0);
        CPU_SET_C_SUM(cpu, cpu->A, mem);
        CPU_SET_H_SUM(cpu, cpu->A, mem);

        cpu->A  += mem;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 8;
    }

    case 0x87: { //ADD A, A
        CPU_SET_N(cpu, 0);
        CPU_SET_C_SUM(cpu, cpu->A, cpu->A);
        CPU_SET_H_SUM(cpu, cpu->A, cpu->A);

        cpu->A  += cpu->A;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x88: { //ADC A, B
        uint8_t carry = CPU_CARRY(cpu);

        CPU_SET_N(cpu, 0);
        CPU_SET_C_SUMC(cpu, cpu->A, cpu->B, carry);
        CPU_SET_H_SUMC(cpu, cpu->A, cpu->B, carry);

        cpu->A  += (cpu->B + carry);
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x89: { //ADC A, C
        uint8_t carry = CPU_CARRY(cpu);

        CPU_SET_N(cpu, 0);
        CPU_SET_H_SUMC(cpu, cpu->A, cpu->C, carry);
        CPU_SET_C_SUMC(cpu, cpu->A, cpu->C, carry);

        cpu->A  += (cpu->C + carry);
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x8a: { //ADC A, D
        uint8_t carry = CPU_CARRY(cpu);

        CPU_SET_N(cpu, 0);
        CPU_SET_C_SUMC(cpu, cpu->A, cpu->D, carry);
        CPU_SET_H_SUMC(cpu, cpu->A, cpu->D, carry);

        cpu->A  += (cpu->D + carry);
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x8b: { //ADC A, E
        uint8_t carry = CPU_CARRY(cpu);

        CPU_SET_N(cpu, 0);
        CPU_SET_C_SUMC(cpu, cpu->A, cpu->E, carry);
        CPU_SET_H_SUMC(cpu, cpu->A, cpu->E, carry);

        cpu->A  += (cpu->E + carry);
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x8c: { //ADC A, H
        uint8_t carry = CPU_CARRY(cpu);

        CPU_SET_N(cpu, 0);
        CPU_SET_C_SUMC(cpu, cpu->A, cpu->H, carry);
        CPU_SET_H_SUMC(cpu, cpu->A, cpu->H, carry);

        cpu->A  += (cpu->H + carry);
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x8d: { //ADC A, L
        uint8_t carry = CPU_CARRY(cpu);

        CPU_SET_N(cpu, 0);
        CPU_SET_C_SUMC(cpu, cpu->A, cpu->L, carry);
        CPU_SET_H_SUMC(cpu, cpu->A, cpu->L, carry);

        cpu->A  += (cpu->L + carry);
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x8e: { //ADC A, (HL)
        uint8_t carry = CPU_CARRY(cpu);
        uint8_t mem   = mem_read8(cpu->mem, CPU_HL(cpu));

        CPU_SET_N(cpu, 0);
        CPU_SET_C_SUMC(cpu, cpu->A, mem, carry);
        CPU_SET_H_SUMC(cpu, cpu->A, mem, carry);

        cpu->A  += (mem + carry);
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 8;
    }

    case 0x8f: { //ADC A, A
        uint8_t carry = CPU_CARRY(cpu);

        CPU_SET_N(cpu, 0);
        CPU_SET_C_SUMC(cpu, cpu->A, cpu->A, carry);
        CPU_SET_H_SUMC(cpu, cpu->A, cpu->A, carry);

        cpu->A  += (cpu->A + carry);
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x90: { //SUB A, B
        CPU_SET_N(cpu, 1);
        CPU_SET_C_SUB(cpu, cpu->A, cpu->B);
        CPU_SET_H_SUB(cpu, cpu->A, cpu->B);

        cpu->A  -= cpu->B;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x91: { //SUB A, C
        CPU_SET_N(cpu, 1);
        CPU_SET_C_SUB(cpu, cpu->A, cpu->C);
        CPU_SET_H_SUB(cpu, cpu->A, cpu->C);

        cpu->A  -= cpu->C;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x92: { //SUB A, D
        CPU_SET_N(cpu, 1);
        CPU_SET_C_SUB(cpu, cpu->A, cpu->D);
        CPU_SET_H_SUB(cpu, cpu->A, cpu->D);

        cpu->A  -= cpu->D;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x93: { //SUB A, E
        CPU_SET_N(cpu, 1);
        CPU_SET_C_SUB(cpu, cpu->A, cpu->E);
        CPU_SET_H_SUB(cpu, cpu->A, cpu->E);

        cpu->A  -= cpu->E;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x94: { //SUB A, H
        CPU_SET_N(cpu, 1);
        CPU_SET_C_SUB(cpu, cpu->A, cpu->H);
        CPU_SET_H_SUB(cpu, cpu->A, cpu->H);

        cpu->A  -= cpu->H;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x95: { //SUB A, L
        CPU_SET_N(cpu, 1);
        CPU_SET_C_SUB(cpu, cpu->A, cpu->L);
        CPU_SET_H_SUB(cpu, cpu->A, cpu->L);

        cpu->A  -= cpu->L;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x96: { //SUB A, (HL)
        uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));

        CPU_SET_N(cpu, 1);
        CPU_SET_C_SUB(cpu, cpu->A, mem);
        CPU_SET_H_SUB(cpu, cpu->A, mem);

        cpu->A  -= mem;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 8;
    }

    case 0x97: { //SUB A, A
        CPU_SET_N(cpu, 1);
        CPU_SET_C_SUB(cpu, cpu->A, cpu->A);
        CPU_SET_H_SUB(cpu, cpu->A, cpu->A);

        cpu->A  -= cpu->A;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x98: { //SBC A, B
        uint8_t carry = CPU_CARRY(cpu);

        CPU_SET_N(cpu, 1);
        CPU_SET_C_SUBC(cpu, cpu->A, cpu->B, carry);
        CPU_SET_H_SUBC(cpu, cpu->A, cpu->B, carry);

        cpu->A  -= (cpu->B + carry);
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x99: { //SBC A, C
        uint8_t carry = CPU_CARRY(cpu);

        CPU_SET_N(cpu, 1);
        CPU_SET_C_SUBC(cpu, cpu->A, cpu->C, carry);
        CPU_SET_H_SUBC(cpu, cpu->A, cpu->C, carry);

        cpu->A  -= (cpu->C + carry);
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x9a: { //SBC A, D
        uint8_t carry = CPU_CARRY(cpu);

        CPU_SET_N(cpu, 1);
        CPU_SET_C_SUBC(cpu, cpu->A, cpu->D, carry);
        CPU_SET_H_SUBC(cpu, cpu->A, cpu->D, carry);

        cpu->A  -= (cpu->D + carry);
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x9b: { //SBC A, E
        uint8_t carry = CPU_CARRY(cpu);

        CPU_SET_N(cpu, 1);
        CPU_SET_C_SUBC(cpu, cpu->A, cpu->E, carry);
        CPU_SET_H_SUBC(cpu, cpu->A, cpu->E, carry);

        cpu->A  -= (cpu->E + carry);
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x9c: { //SBC A, H
        uint8_t carry = CPU_CARRY(cpu);

        CPU_SET_N(cpu, 1);
        CPU_SET_C_SUBC(cpu, cpu->A, cpu->H, carry);
        CPU_SET_H_SUBC(cpu, cpu->A, cpu->H, carry);

        cpu->A  -= (cpu->H + carry);
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x9d: { //SBC A, L
        uint8_t carry = CPU_CARRY(cpu);

        CPU_SET_N(cpu, 1);
        CPU_SET_C_SUBC(cpu, cpu->A, cpu->L, carry);
        CPU_SET_H_SUBC(cpu, cpu->A, cpu->L, carry);

        cpu->A  -= (cpu->L + carry);
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0x9e: { //SBC A, (HL)
        uint8_t carry = CPU_CARRY(cpu);
        uint8_t mem   = mem_read8(cpu->mem, CPU_HL(cpu));

        CPU_SET_N(cpu, 1);
        CPU_SET_C_SUBC(cpu, cpu->A, mem, carry);
        CPU_SET_H_SUBC(cpu, cpu->A, mem, carry);

        cpu->A  -= (mem + carry);
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 8;
    }

    case 0x9f: { //SBC A, A
        uint8_t carry = CPU_CARRY(cpu);

        CPU_SET_N(cpu, 1);
        CPU_SET_C_SUBC(cpu, cpu->A, cpu->A, carry);
        CPU_SET_H_SUBC(cpu, cpu->A, cpu->A, carry);

        cpu->A  -= (cpu->A + carry);
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 4;
    }

    case 0xa0: { //AND A, B
        cpu->A  &= cpu->B;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 1);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xa1: { //AND A, C
        cpu->A  &= cpu->C;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 1);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xa2: { //AND A, D
        cpu->A  &= cpu->D;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 1);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xa3: { //AND A, E
        cpu->A  &= cpu->E;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 1);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xa4: { //AND A, H
        cpu->A  &= cpu->H;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 1);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xa5: { //AND A, L
        cpu->A  &= cpu->L;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 1);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xa6: { //AND A, (HL)
        uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));

        cpu->A  &= mem;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 1);
        CPU_SET_C(cpu, 0);
        return 8;
    }

    case 0xa7: { //AND A, A
        cpu->A  &= cpu->A;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 1);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xa8: { //XOR A, B
        cpu->A  ^= cpu->B;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xa9: { //XOR A, C
        cpu->A  ^= cpu->C;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xaa: { //XOR A, D
        cpu->A  ^= cpu->D;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xab: { //XOR A, E
        cpu->A  ^= cpu->E;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xac: { //XOR A, H
        cpu->A  ^= cpu->H;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xad: { //XOR A, L
        cpu->A  ^= cpu->L;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xae: { //XOR A, (HL)
        uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));

        cpu->A  ^= mem;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, 0);
        return 8;
    }

    case 0xaf: { //XOR A, A
        cpu->A  ^= cpu->A;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xb0: { //OR A, B
        cpu->A  |= cpu->B;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xb1: { //OR A, C
        cpu->A  |= cpu->C;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xb2: { //OR A, D
        cpu->A  |= cpu->D;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xb3: { //OR A, E
        cpu->A  |= cpu->E;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xb4: { //OR A, H
        cpu->A  |= cpu->H;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xb5: { //OR A, L
        cpu->A  |= cpu->L;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xb6: { //OR A, (HL)
        uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));

        cpu->A  |= mem;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, 0);
        return 8;
    }

    case 0xb7: { //OR A, A
        cpu->A  |= cpu->A;
        cpu->PC += 1;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, 0);
        return 4;
    }

    case 0xb8: { //CP A, B
        CPU_SET_N(cpu, 1);
        CPU_SET_Z(cpu,     cpu->A == cpu->B);
        CPU_SET_C_SUB(cpu, cpu->A, cpu->B);
        CPU_SET_H_SUB(cpu, cpu->A, cpu->B);

        cpu->PC += 1;
        return 4;
    }

    case 0xb9: { //CP A, C
        CPU_SET_N(cpu, 1);
        CPU_SET_Z(cpu,     cpu->A == cpu->C);
        CPU_SET_C_SUB(cpu, cpu->A, cpu->C);
        CPU_SET_H_SUB(cpu, cpu->A, cpu->C);

        cpu->PC += 1;
        return 4;
    }

    case 0xba: { //CP A, D
        CPU_SET_N(cpu, 1);
        CPU_SET_Z(cpu,     cpu->A == cpu->D);
        CPU_SET_C_SUB(cpu, cpu->A, cpu->D);
        CPU_SET_H_SUB(cpu, cpu->A, cpu->D);

        cpu->PC += 1;
        return 4;
    }

    case 0xbb: { //CP A, E
        CPU_SET_N(cpu, 1);
        CPU_SET_Z(cpu,     cpu->A == cpu->E);
        CPU_SET_C_SUB(cpu, cpu->A, cpu->E);
        CPU_SET_H_SUB(cpu, cpu->A, cpu->E);

        cpu->PC += 1;
        return 4;
    }

    case 0xbc: { //CP A, H
        CPU_SET_N(cpu, 1);
        CPU_SET_Z(cpu,     cpu->A == cpu->H);
        CPU_SET_C_SUB(cpu, cpu->A, cpu->H);
        CPU_SET_H_SUB(cpu, cpu->A, cpu->H);

        cpu->PC += 1;
        return 4;
    }

    case 0xbd: { //CP A, L
        CPU_SET_N(cpu, 1);
        CPU_SET_Z(cpu,     cpu->A == cpu->L);
        CPU_SET_C_SUB(cpu, cpu->A, cpu->L);
        CPU_SET_H_SUB(cpu, cpu->A, cpu->L);

        cpu->PC += 1;
        return 4;
    }

    case 0xbe: { //CP A, (HL)
        uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));

        CPU_SET_N(cpu, 1);
        CPU_SET_Z(cpu,     cpu->A == mem);
        CPU_SET_C_SUB(cpu, cpu->A, mem);
        CPU_SET_H_SUB(cpu, cpu->A, mem);

        cpu->PC += 1;
        return 8;
    }

    case 0xbf: { //CP A, A
        CPU_SET_N(cpu, 1);
        CPU_SET_Z(cpu,     cpu->A == cpu->A);
        CPU_SET_C_SUB(cpu, cpu->A, cpu->A);
        CPU_SET_H_SUB(cpu, cpu->A, cpu->A);

        cpu->PC += 1;
        return 4;
    }

    case 0xc0: { //RET NZ
        if (!CPU_ZERO(cpu)) {
            uint8_t lsb = mem_read8(cpu->mem, cpu->SP);
            cpu->SP   += 1;
            uint8_t msb = mem_read8(cpu->mem, cpu->SP);
            cpu->SP   += 1;

            cpu->PC = BYTE_16BIT(msb, lsb);
            return 20;
        } else {
            cpu->PC += 1;
            return 8;
        }
    }

    case 0xc1: { //POP BC
        cpu->C   = mem_read8(cpu->mem, cpu->SP);
        cpu->SP += 1;
        cpu->B   = mem_read8(cpu->mem, cpu->SP);
        cpu->SP += 1;
        cpu->PC += 1;
        return 12;
    }

    case 0xc2: { //JP NZ, {16bit}
        if (!CPU_ZERO(cpu)) {
            cpu->PC = mem_read16(cpu->mem, pc+1);
            return 16;
        } else {
            cpu->PC += 3;
            return 12;
        }
    }

    case 0xc3: { //JP {16bit}
        cpu->PC = mem_read16(cpu->mem, pc+1);
        // printf("JP to %04x\n", cpu->PC);
        return 16;
    }

    case 0xc4: { //CALL NZ, {16bit}
        uint16_t addr = pc+3;

        if (!CPU_ZERO(cpu)) {
            cpu->SP -= 1;
            mem_write8(cpu->mem, cpu->SP, (addr>>8) & 0xff);
            cpu->SP -= 1;
            mem_write8(cpu->mem, cpu->SP, addr&0xff);

            cpu->PC = mem_read16(cpu->mem, pc+1);
            return 24;
        } else {
            cpu->PC = addr;
            return 12;
        }
    }

    case 0xc5: { //PUSH BC
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, cpu->B);
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, cpu->C);
        cpu->PC += 1;
        return 16;
    }

    case 0xc6: { //ADD A, {8bit}
        uint8_t mem = mem_read8(cpu->mem, pc+1);

        CPU_SET_N(cpu, 0);
        CPU_SET_C_SUM(cpu, cpu->A, mem);
        CPU_SET_H_SUM(cpu, cpu->A, mem);

        cpu->A  += mem;
        cpu->PC += 2;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 8;
    }

    case 0xc7: { //RST 00H
        uint16_t addr = cpu->PC + 1;
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, (addr>>8) & 0xff);
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, addr&0xff);
        cpu->PC = 0x00;
        return 16;
    }

    case 0xc8: { //RET Z
        if (CPU_ZERO(cpu)) {
            uint8_t lsb = mem_read8(cpu->mem, cpu->SP);
            cpu->SP   += 1;
            uint8_t msb = mem_read8(cpu->mem, cpu->SP);
            cpu->SP   += 1;
            cpu->PC    = BYTE_16BIT(msb, lsb);
            return 20;
        } else {
            cpu->PC += 1;
            return 8;
        }
    }

    case 0xc9: { //RET
        uint8_t lsb = mem_read8(cpu->mem, cpu->SP);
        cpu->SP   += 1;
        uint8_t msb = mem_read8(cpu->mem, cpu->SP);
        cpu->SP   += 1;
        cpu->PC    = BYTE_16BIT(msb, lsb);
        return 16;
    }

    case 0xca: { //JP Z, {16bit}
        if (CPU_ZERO(cpu)) {
            cpu->PC = mem_read16(cpu->mem, pc+1);
            return 16;
        } else {
            cpu->PC += 3;
            return 12;
        }
    }

    case 0xcb: { //CB PREFIXED INSTR SET
        uint8_t postfix = mem_read8(cpu->mem, pc+1);
        cpu->PC += 2;

        switch (postfix) {
        case 0x00: { // RLC B
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->B));
            BYTE_ROTATE_LEFT(cpu->B);
            CPU_SET_Z(cpu, cpu->B == 0);
            return 8;
        }

        case 0x01: { // RLC C
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->C));
            BYTE_ROTATE_LEFT(cpu->C);
            CPU_SET_Z(cpu, cpu->C == 0);
            return 8;
        }

        case 0x02: { // RLC D
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->D));
            BYTE_ROTATE_LEFT(cpu->D);
            CPU_SET_Z(cpu, cpu->D == 0);
            return 8;
        }

        case 0x03: { // RLC E
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->E));
            BYTE_ROTATE_LEFT(cpu->E);
            CPU_SET_Z(cpu, cpu->E == 0);
            return 8;
        }

        case 0x04: { // RLC H
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->H));
            BYTE_ROTATE_LEFT(cpu->H);
            CPU_SET_Z(cpu, cpu->H == 0);
            return 8;
        }

        case 0x05: { // RLC L
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->L));
            BYTE_ROTATE_LEFT(cpu->L);
            CPU_SET_Z(cpu, cpu->L == 0);
            return 8;
        }

        case 0x06: { // RLC (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(mem));
            BYTE_ROTATE_LEFT(mem);
            CPU_SET_Z(cpu, mem == 0);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0x07: { // RLC A
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->A));
            BYTE_ROTATE_LEFT(cpu->A);
            CPU_SET_Z(cpu, cpu->A == 0);
            return 8;
        }

        case 0x08: { // RRC B
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->B));
            BYTE_ROTATE_RIGHT(cpu->B);
            CPU_SET_Z(cpu, cpu->B == 0);
            return 8;
        }

        case 0x09: { // RRC C
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->C));
            BYTE_ROTATE_RIGHT(cpu->C);
            CPU_SET_Z(cpu, cpu->C == 0);
            return 8;
        }

        case 0x0a: { // RRC D
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->D));
            BYTE_ROTATE_RIGHT(cpu->D);
            CPU_SET_Z(cpu, cpu->D == 0);
            return 8;
        }

        case 0x0b: { // RRC E
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->E));
            BYTE_ROTATE_RIGHT(cpu->E);
            CPU_SET_Z(cpu, cpu->E == 0);
            return 8;
        }

        case 0x0c: { // RRC H
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->H));
            BYTE_ROTATE_RIGHT(cpu->H);
            CPU_SET_Z(cpu, cpu->H == 0);
            return 8;
        }

        case 0x0d: { // RRC L
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->L));
            BYTE_ROTATE_RIGHT(cpu->L);
            CPU_SET_Z(cpu, cpu->L == 0);
            return 8;
        }

        case 0x0e: { // RRC (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(mem));
            BYTE_ROTATE_RIGHT(mem);
            CPU_SET_Z(cpu, mem == 0);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0x0f: { // RRC A
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->A));
            BYTE_ROTATE_RIGHT(cpu->A);
            CPU_SET_Z(cpu, cpu->A == 0);
            return 8;
        }

        case 0x10: { // RL B
            uint8_t carry = CPU_CARRY(cpu);

            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->B));
            BYTE_ROTATE_LEFT_CARRY(cpu->B, carry);
            CPU_SET_Z(cpu, cpu->B == 0);
            return 8;
        }

        case 0x11: { // RL C
            uint8_t carry = CPU_CARRY(cpu);

            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->C));
            BYTE_ROTATE_LEFT_CARRY(cpu->C, carry);
            CPU_SET_Z(cpu, cpu->C == 0);
            return 8;
        }

        case 0x12: { // RL D
            uint8_t carry = CPU_CARRY(cpu);

            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->D));
            BYTE_ROTATE_LEFT_CARRY(cpu->D, carry);
            CPU_SET_Z(cpu, cpu->D == 0);
            return 8;
        }

        case 0x13: { // RL E
            uint8_t carry = CPU_CARRY(cpu);
            
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->E));
            BYTE_ROTATE_LEFT_CARRY(cpu->E, carry);
            CPU_SET_Z(cpu, cpu->E == 0);
            return 8;
        }

        case 0x14: { // RL H
            uint8_t carry = CPU_CARRY(cpu);

            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->H));
            BYTE_ROTATE_LEFT_CARRY(cpu->H, carry);
            CPU_SET_Z(cpu, cpu->H == 0);
            return 8;
        }

        case 0x15: { // RL L
            uint8_t carry = CPU_CARRY(cpu);

            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->L));
            BYTE_ROTATE_LEFT_CARRY(cpu->L, carry);
            CPU_SET_Z(cpu, cpu->L == 0);
            return 8;
        }

        case 0x16: { // RL (HL)
            uint8_t carry = CPU_CARRY(cpu);
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));

            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(mem));
            BYTE_ROTATE_LEFT_CARRY(mem, carry);
            CPU_SET_Z(cpu, mem == 0);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0x17: { // RL A
            uint8_t carry = CPU_CARRY(cpu);

            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->A));
            BYTE_ROTATE_LEFT_CARRY(cpu->A, carry);
            CPU_SET_Z(cpu, cpu->A == 0);
            return 8;
        }

        case 0x18: { // RR B
            uint8_t carry = CPU_CARRY(cpu);

            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->B));
            BYTE_ROTATE_RIGHT_CARRY(cpu->B, carry);
            CPU_SET_Z(cpu, cpu->B == 0);
            return 8;
        }

        case 0x19: { // RR C
            uint8_t carry = CPU_CARRY(cpu);

            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->C));
            BYTE_ROTATE_RIGHT_CARRY(cpu->C, carry);
            CPU_SET_Z(cpu, cpu->C == 0);
            return 8;
        }

        case 0x1a: { // RR D
            uint8_t carry = CPU_CARRY(cpu);

            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->D));
            BYTE_ROTATE_RIGHT_CARRY(cpu->D, carry);
            CPU_SET_Z(cpu, cpu->D == 0);
            return 8;
        }

        case 0x1b: { // RR E
            uint8_t carry = CPU_CARRY(cpu);

            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->E));
            BYTE_ROTATE_RIGHT_CARRY(cpu->E, carry);
            CPU_SET_Z(cpu, cpu->E == 0);
            return 8;
        }

        case 0x1c: { // RR H
            uint8_t carry = CPU_CARRY(cpu);

            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->H));
            BYTE_ROTATE_RIGHT_CARRY(cpu->H, carry);
            CPU_SET_Z(cpu, cpu->H == 0);
            return 8;
        }

        case 0x1d: { // RR L
            uint8_t carry = CPU_CARRY(cpu);

            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->L));
            BYTE_ROTATE_RIGHT_CARRY(cpu->L, carry);
            CPU_SET_Z(cpu, cpu->L == 0);
            return 8;
        }

        case 0x1e: { // RR (HL)
            uint8_t carry = CPU_CARRY(cpu);
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));

            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(mem));
            BYTE_ROTATE_RIGHT_CARRY(mem, carry);
            CPU_SET_Z(cpu, mem == 0);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0x1f: { // RR A
            uint8_t carry = CPU_CARRY(cpu);

            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->A));
            BYTE_ROTATE_RIGHT_CARRY(cpu->A, carry);
            CPU_SET_Z(cpu, cpu->A == 0);
            return 8;
        }

        case 0x20: { // SLA B
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->B));
            BYTE_SHIFT_LEFT(cpu->B);
            CPU_SET_Z(cpu, cpu->B == 0);
            return 8;
        }

        case 0x21: { // SLA C
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->C));
            BYTE_SHIFT_LEFT(cpu->C);
            CPU_SET_Z(cpu, cpu->C == 0);
            return 8;
        }

        case 0x22: { // SLA D
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->D));
            BYTE_SHIFT_LEFT(cpu->D);
            CPU_SET_Z(cpu, cpu->D == 0);
            return 8;
        }

        case 0x23: { // SLA E
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->E));
            BYTE_SHIFT_LEFT(cpu->E);
            CPU_SET_Z(cpu, cpu->E == 0);
            return 8;
        }

        case 0x24: { // SLA H
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->H));
            BYTE_SHIFT_LEFT(cpu->H);
            CPU_SET_Z(cpu, cpu->H == 0);
            return 8;
        }

        case 0x25: { // SLA L
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->L));
            BYTE_SHIFT_LEFT(cpu->L);
            CPU_SET_Z(cpu, cpu->L == 0);
            return 8;
        }

        case 0x26: { // SLA (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(mem));
            BYTE_SHIFT_LEFT(mem);
            CPU_SET_Z(cpu, mem == 0);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0x27: { // SLA A
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_7(cpu->A));
            BYTE_SHIFT_LEFT(cpu->A);
            CPU_SET_Z(cpu, cpu->A == 0);
            return 8;
        }
        
        case 0x28: { // SRA B
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->B));
            BYTE_SHIFT_RIGHT_KEEP_BIT(cpu->B);
            CPU_SET_Z(cpu, cpu->B == 0);
            return 8;
        }

        case 0x29: { // SRA C
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->C));
            BYTE_SHIFT_RIGHT_KEEP_BIT(cpu->C);
            CPU_SET_Z(cpu, cpu->C == 0);
            return 8;
        }

        case 0x2a: { // SRA D
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->D));
            BYTE_SHIFT_RIGHT_KEEP_BIT(cpu->D);
            CPU_SET_Z(cpu, cpu->D == 0);
            return 8;
        }

        case 0x2b: { // SRA E
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->E));
            BYTE_SHIFT_RIGHT_KEEP_BIT(cpu->E);
            CPU_SET_Z(cpu, cpu->E == 0);
            return 8;
        }

        case 0x2c: { // SRA H
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->H));
            BYTE_SHIFT_RIGHT_KEEP_BIT(cpu->H);
            CPU_SET_Z(cpu, cpu->H == 0);
            return 8;
        }

        case 0x2d: { // SRA L
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->L));
            BYTE_SHIFT_RIGHT_KEEP_BIT(cpu->L);
            CPU_SET_Z(cpu, cpu->L == 0);
            return 8;
        }

        case 0x2e: { // SRA (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(mem));
            BYTE_SHIFT_RIGHT_KEEP_BIT(mem);
            CPU_SET_Z(cpu, mem == 0);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0x2f: { // SRA A
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->A));
            BYTE_SHIFT_RIGHT_KEEP_BIT(cpu->A);
            CPU_SET_Z(cpu, cpu->A == 0);
            return 8;
        }

        case 0x30: { //SWAP B
            BYTE_SWAP(cpu->B);
            CPU_SET_Z(cpu, cpu->B == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, 0);
            return 8;
        }

        case 0x31: { //SWAP C
            BYTE_SWAP(cpu->C);
            CPU_SET_Z(cpu, cpu->C == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, 0);
            return 8;
        }

        case 0x32: { //SWAP D
            BYTE_SWAP(cpu->D);
            CPU_SET_Z(cpu, cpu->D == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, 0);
            return 8;
        }

        case 0x33: { //SWAP E
            BYTE_SWAP(cpu->E);
            CPU_SET_Z(cpu, cpu->E == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, 0);
            return 8;
        }

        case 0x34: { //SWAP H
            BYTE_SWAP(cpu->H);
            CPU_SET_Z(cpu, cpu->H == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, 0);
            return 8;
        }

        case 0x35: { //SWAP L
            BYTE_SWAP(cpu->L);
            CPU_SET_Z(cpu, cpu->L == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, 0);
            return 8;
        }

        case 0x36: { //SWAP (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            BYTE_SWAP(mem);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            CPU_SET_Z(cpu, mem == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, 0);
            return 16;
        }

        case 0x37: { //SWAP A
            BYTE_SWAP(cpu->A);
            CPU_SET_Z(cpu, cpu->A == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, 0);
            return 8;
        }

        case 0x38: { // SRL B
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->B));
            BYTE_SHIFT_RIGHT(cpu->B);
            CPU_SET_Z(cpu, cpu->B == 0);
            return 8;
        }

        case 0x39: { // SRL C
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->C));
            BYTE_SHIFT_RIGHT(cpu->C);
            CPU_SET_Z(cpu, cpu->C == 0);
            return 8;
        }

        case 0x3a: { // SRL D
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->D));
            BYTE_SHIFT_RIGHT(cpu->D);
            CPU_SET_Z(cpu, cpu->D == 0);
            return 8;
        }

        case 0x3b: { // SRL E
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->E));
            BYTE_SHIFT_RIGHT(cpu->E);
            CPU_SET_Z(cpu, cpu->E == 0);
            return 8;
        }

        case 0x3c: { // SRL H
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->H));
            BYTE_SHIFT_RIGHT(cpu->H);
            CPU_SET_Z(cpu, cpu->H == 0);
            return 8;
        }

        case 0x3d: { // SRL L
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->L));
            BYTE_SHIFT_RIGHT(cpu->L);
            CPU_SET_Z(cpu, cpu->L == 0);
            return 8;
        }

        case 0x3e: { // SRL (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(mem));
            BYTE_SHIFT_RIGHT(mem);
            CPU_SET_Z(cpu, mem == 0);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0x3f: { // SRL A
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 0);
            CPU_SET_C(cpu, BIT_0(cpu->A));
            BYTE_SHIFT_RIGHT(cpu->A);
            CPU_SET_Z(cpu, cpu->A == 0);
            return 8;
        }

        case 0x40: { // BIT 0, B
            CPU_SET_Z(cpu, BIT_N(cpu->B, 0) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x41: { // BIT 0, C
            CPU_SET_Z(cpu, BIT_N(cpu->C, 0) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x42: { // BIT 0, D
            CPU_SET_Z(cpu, BIT_N(cpu->D, 0) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x43: { // BIT 0, E
            CPU_SET_Z(cpu, BIT_N(cpu->E, 0) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x44: { // BIT 0, H
            CPU_SET_Z(cpu, BIT_N(cpu->H, 0) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x45: { // BIT 0, L
            CPU_SET_Z(cpu, BIT_N(cpu->L, 0) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x46: { // BIT 0, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            CPU_SET_Z(cpu, BIT_N(mem, 0) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 12;
        }

        case 0x47: { // BIT 0, A
            CPU_SET_Z(cpu, BIT_N(cpu->A, 0) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x48: { // BIT 1, B
            CPU_SET_Z(cpu, BIT_N(cpu->B, 1) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x49: { // BIT 1, C
            CPU_SET_Z(cpu, BIT_N(cpu->C, 1) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x4a: { // BIT 1, D
            CPU_SET_Z(cpu, BIT_N(cpu->D, 1) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x4b: { // BIT 1, E
            CPU_SET_Z(cpu, BIT_N(cpu->E, 1) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x4c: { // BIT 1, H
            CPU_SET_Z(cpu, BIT_N(cpu->H, 1) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x4d: { // BIT 1, L
            CPU_SET_Z(cpu, BIT_N(cpu->L, 1) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x4e: { // BIT 1, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            CPU_SET_Z(cpu, BIT_N(mem, 1) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 12;
        }

        case 0x4f: { // BIT 1, A
            CPU_SET_Z(cpu, BIT_N(cpu->A, 1) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x50: { // BIT 2, B
            CPU_SET_Z(cpu, BIT_N(cpu->B, 2) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x51: { // BIT 2, C
            CPU_SET_Z(cpu, BIT_N(cpu->C, 2) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x52: { // BIT 2, D
            CPU_SET_Z(cpu, BIT_N(cpu->D, 2) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x53: { // BIT 2, E
            CPU_SET_Z(cpu, BIT_N(cpu->E, 2) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x54: { // BIT 2, H
            CPU_SET_Z(cpu, BIT_N(cpu->H, 2) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x55: { // BIT 2, L
            CPU_SET_Z(cpu, BIT_N(cpu->L, 2) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x56: { // BIT 2, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            CPU_SET_Z(cpu, BIT_N(mem, 2) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 12;
        }

        case 0x57: { // BIT 2, A
            CPU_SET_Z(cpu, BIT_N(cpu->A, 2) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x58: { // BIT 3, B
            CPU_SET_Z(cpu, BIT_N(cpu->B, 3) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x59: { // BIT 3, C
            CPU_SET_Z(cpu, BIT_N(cpu->C, 3) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x5a: { // BIT 3, D
            CPU_SET_Z(cpu, BIT_N(cpu->D, 3) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x5b: { // BIT 3, E
            CPU_SET_Z(cpu, BIT_N(cpu->E, 3) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x5c: { // BIT 3, H
            CPU_SET_Z(cpu, BIT_N(cpu->H, 3) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x5d: { // BIT 3, L
            CPU_SET_Z(cpu, BIT_N(cpu->L, 3) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x5e: { // BIT 3, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            CPU_SET_Z(cpu, BIT_N(mem, 3) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 12;
        }

        case 0x5f: { // BIT 3, A
            CPU_SET_Z(cpu, BIT_N(cpu->A, 3) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }
        
        case 0x60: { // BIT 4, B
            CPU_SET_Z(cpu, BIT_N(cpu->B, 4) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x61: { // BIT 4, C
            CPU_SET_Z(cpu, BIT_N(cpu->C, 4) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x62: { // BIT 4, D
            CPU_SET_Z(cpu, BIT_N(cpu->D, 4) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x63: { // BIT 4, E
            CPU_SET_Z(cpu, BIT_N(cpu->E, 4) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x64: { // BIT 4, H
            CPU_SET_Z(cpu, BIT_N(cpu->H, 4) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x65: { // BIT 4, L
            CPU_SET_Z(cpu, BIT_N(cpu->L, 4) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x66: { // BIT 4, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            CPU_SET_Z(cpu, BIT_N(mem, 4) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 12;
        }

        case 0x67: { // BIT 4, A
            CPU_SET_Z(cpu, BIT_N(cpu->A, 4) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x68: { // BIT 5, B
            CPU_SET_Z(cpu, BIT_N(cpu->B, 5) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x69: { // BIT 5, C
            CPU_SET_Z(cpu, BIT_N(cpu->C, 5) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x6a: { // BIT 5, D
            CPU_SET_Z(cpu, BIT_N(cpu->D, 5) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x6b: { // BIT 5, E
            CPU_SET_Z(cpu, BIT_N(cpu->E, 5) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x6c: { // BIT 5, H
            CPU_SET_Z(cpu, BIT_N(cpu->H, 5) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x6d: { // BIT 5, L
            CPU_SET_Z(cpu, BIT_N(cpu->L, 5) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x6e: { // BIT 5, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            CPU_SET_Z(cpu, BIT_N(mem, 5) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 12;
        }

        case 0x6f: { // BIT 5, A
            CPU_SET_Z(cpu, BIT_N(cpu->A, 5) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x70: { // BIT 6, B
            CPU_SET_Z(cpu, BIT_N(cpu->B, 6) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x71: { // BIT 6, C
            CPU_SET_Z(cpu, BIT_N(cpu->C, 6) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x72: { // BIT 6, D
            CPU_SET_Z(cpu, BIT_N(cpu->D, 6) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x73: { // BIT 6, E
            CPU_SET_Z(cpu, BIT_N(cpu->E, 6) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x74: { // BIT 6, H
            CPU_SET_Z(cpu, BIT_N(cpu->H, 6) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x75: { // BIT 6, L
            CPU_SET_Z(cpu, BIT_N(cpu->L, 6) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x76: { // BIT 6, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            CPU_SET_Z(cpu, BIT_N(mem, 6) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 12;
        }

        case 0x77: { // BIT 6, A
            CPU_SET_Z(cpu, BIT_N(cpu->A, 6) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x78: { // BIT 7, B
            CPU_SET_Z(cpu, BIT_N(cpu->B, 7) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x79: { // BIT 7, C
            CPU_SET_Z(cpu, BIT_N(cpu->C, 7) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x7a: { // BIT 7, D
            CPU_SET_Z(cpu, BIT_N(cpu->D, 7) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x7b: { // BIT 7, E
            CPU_SET_Z(cpu, BIT_N(cpu->E, 7) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x7c: { // BIT 7, H
            CPU_SET_Z(cpu, BIT_N(cpu->H, 7) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x7d: { // BIT 7, L
            CPU_SET_Z(cpu, BIT_N(cpu->L, 7) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x7e: { // BIT 7, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            CPU_SET_Z(cpu, BIT_N(mem, 7) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 12;
        }

        case 0x7f: { // BIT 7, A
            CPU_SET_Z(cpu, BIT_N(cpu->A, 7) == 0);
            CPU_SET_N(cpu, 0);
            CPU_SET_H(cpu, 1);
            return 8;
        }

        case 0x80: { // RES 0, B
            BYTE_CLEAR_BIT(cpu->B, 0);
            return 8;
        }

        case 0x81: { // RES 0, C
            BYTE_CLEAR_BIT(cpu->C, 0);
            return 8;
        }

        case 0x82: { // RES 0, D
            BYTE_CLEAR_BIT(cpu->D, 0);
            return 8;
        }

        case 0x83: { // RES 0, E
            BYTE_CLEAR_BIT(cpu->E, 0);
            return 8;
        }

        case 0x84: { // RES 0, H
            BYTE_CLEAR_BIT(cpu->H, 0);
            return 8;
        }

        case 0x85: { // RES 0, L
            BYTE_CLEAR_BIT(cpu->L, 0);
            return 8;
        }

        case 0x86: { // RES 0, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            BYTE_CLEAR_BIT(mem, 0);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0x87: { // RES 0, A
            BYTE_CLEAR_BIT(cpu->A, 0);
            return 8;
        }

        case 0x88: { // RES 1, B
            BYTE_CLEAR_BIT(cpu->B, 1);
            return 8;
        }

        case 0x89: { // RES 1, C
            BYTE_CLEAR_BIT(cpu->C, 1);
            return 8;
        }

        case 0x8a: { // RES 1, D
            BYTE_CLEAR_BIT(cpu->D, 1);
            return 8;
        }

        case 0x8b: { // RES 1, E
            BYTE_CLEAR_BIT(cpu->E, 1);
            return 8;
        }

        case 0x8c: { // RES 1, H
            BYTE_CLEAR_BIT(cpu->H, 1);
            return 8;
        }

        case 0x8d: { // RES 1, L
            BYTE_CLEAR_BIT(cpu->L, 1);
            return 8;
        }

        case 0x8e: { // RES 1, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            BYTE_CLEAR_BIT(mem, 1);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0x8f: { // RES 1, A
            BYTE_CLEAR_BIT(cpu->A, 1);
            return 8;
        }

        case 0x90: { // RES 2, B
            BYTE_CLEAR_BIT(cpu->B, 2);
            return 8;
        }

        case 0x91: { // RES 2, C
            BYTE_CLEAR_BIT(cpu->C, 2);
            return 8;
        }

        case 0x92: { // RES 2, D
            BYTE_CLEAR_BIT(cpu->D, 2);
            return 8;
        }

        case 0x93: { // RES 2, E
            BYTE_CLEAR_BIT(cpu->E, 2);
            return 8;
        }

        case 0x94: { // RES 2, H
            BYTE_CLEAR_BIT(cpu->H, 2);
            return 8;
        }

        case 0x95: { // RES 2, L
            BYTE_CLEAR_BIT(cpu->L, 2);
            return 8;
        }

        case 0x96: { // RES 2, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            BYTE_CLEAR_BIT(mem, 2);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0x97: { // RES 2, A
            BYTE_CLEAR_BIT(cpu->A, 2);
            return 8;
        }

        case 0x98: { // RES 3, B
            BYTE_CLEAR_BIT(cpu->B, 3);
            return 8;
        }

        case 0x99: { // RES 3, C
            BYTE_CLEAR_BIT(cpu->C, 3);
            return 8;
        }

        case 0x9a: { // RES 3, D
            BYTE_CLEAR_BIT(cpu->D, 3);
            return 8;
        }

        case 0x9b: { // RES 3, E
            BYTE_CLEAR_BIT(cpu->E, 3);
            return 8;
        }

        case 0x9c: { // RES 3, H
            BYTE_CLEAR_BIT(cpu->H, 3);
            return 8;
        }

        case 0x9d: { // RES 3, L
            BYTE_CLEAR_BIT(cpu->L, 3);
            return 8;
        }

        case 0x9e: { // RES 3, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            BYTE_CLEAR_BIT(mem, 3);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0x9f: { // RES 3, A
            BYTE_CLEAR_BIT(cpu->A, 3);
            return 8;
        }

        case 0xa0: { // RES 4, B
            BYTE_CLEAR_BIT(cpu->B, 4);
            return 8;
        }
        
        case 0xa1: { // RES 4, C
            BYTE_CLEAR_BIT(cpu->C, 4);
            return 8;
        }

        case 0xa2: { // RES 4, D
            BYTE_CLEAR_BIT(cpu->D, 4);
            return 8;
        }

        case 0xa3: { // RES 4, E
            BYTE_CLEAR_BIT(cpu->E, 4);
            return 8;
        }

        case 0xa4: { // RES 4, H
            BYTE_CLEAR_BIT(cpu->H, 4);
            return 8;
        }

        case 0xa5: { // RES 4, L
            BYTE_CLEAR_BIT(cpu->L, 4);
            return 8;
        }

        case 0xa6: { // RES 4, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            BYTE_CLEAR_BIT(mem, 4);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0xa7: { // RES 4, A
            BYTE_CLEAR_BIT(cpu->A, 4);
            return 8;
        }

        case 0xa8: { // RES 5, B
            BYTE_CLEAR_BIT(cpu->B, 5);
            return 8;
        }

        case 0xa9: { // RES 5, C
            BYTE_CLEAR_BIT(cpu->C, 5);
            return 8;
        }

        case 0xaa: { // RES 5, D
            BYTE_CLEAR_BIT(cpu->D, 5);
            return 8;
        }

        case 0xab: { // RES 5, E
            BYTE_CLEAR_BIT(cpu->E, 5);
            return 8;
        }

        case 0xac: { // RES 5, H
            BYTE_CLEAR_BIT(cpu->H, 5);
            return 8;
        }

        case 0xad: { // RES 5, L
            BYTE_CLEAR_BIT(cpu->L, 5);
            return 8;
        }

        case 0xae: { // RES 5, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            BYTE_CLEAR_BIT(mem, 5);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0xaf: { // RES 5, A
            BYTE_CLEAR_BIT(cpu->A, 5);
            return 8;
        }

        case 0xb0: { // RES 6, B
            BYTE_CLEAR_BIT(cpu->B, 6);
            return 8;
        }

        case 0xb1: { // RES 6, C
            BYTE_CLEAR_BIT(cpu->C, 6);
            return 8;
        }

        case 0xb2: { // RES 6, D
            BYTE_CLEAR_BIT(cpu->D, 6);
            return 8;
        }

        case 0xb3: { // RES 6, E
            BYTE_CLEAR_BIT(cpu->E, 6);
            return 8;
        }

        case 0xb4: { // RES 6, H
            BYTE_CLEAR_BIT(cpu->H, 6);
            return 8;
        }

        case 0xb5: { // RES 6, L
            BYTE_CLEAR_BIT(cpu->L, 6);
            return 8;
        }

        case 0xb6: { // RES 6, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            BYTE_CLEAR_BIT(mem, 6);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0xb7: { // RES 6, A
            BYTE_CLEAR_BIT(cpu->A, 6);
            return 8;
        }

        case 0xb8: { // RES 7, B
            BYTE_CLEAR_BIT(cpu->B, 7);
            return 8;
        }

        case 0xb9: { // RES 7, C
            BYTE_CLEAR_BIT(cpu->C, 7);
            return 8;
        }

        case 0xba: { // RES 7, D
            BYTE_CLEAR_BIT(cpu->D, 7);
            return 8;
        }

        case 0xbb: { // RES 7, E
            BYTE_CLEAR_BIT(cpu->E, 7);
            return 8;
        }

        case 0xbc: { // RES 7, H
            BYTE_CLEAR_BIT(cpu->H, 7);
            return 8;
        }

        case 0xbd: { // RES 7, L
            BYTE_CLEAR_BIT(cpu->L, 7);
            return 8;
        }

        case 0xbe: { // RES 7, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            BYTE_CLEAR_BIT(mem, 7);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0xbf: { // RES 7, A
            BYTE_CLEAR_BIT(cpu->A, 7);
            return 8;
        }

        case 0xc0: { // SET 0, B
            BYTE_SET_BIT(cpu->B, 0);
            return 8;
        }
        
        case 0xc1: { // SET 0, C
            BYTE_SET_BIT(cpu->C, 0);
            return 8;
        }

        case 0xc2: { // SET 0, D
            BYTE_SET_BIT(cpu->D, 0);
            return 8;
        }

        case 0xc3: { // SET 0, E
            BYTE_SET_BIT(cpu->E, 0);
            return 8;
        }

        case 0xc4: { // SET 0, H
            BYTE_SET_BIT(cpu->H, 0);
            return 8;
        }

        case 0xc5: { // SET 0, L
            BYTE_SET_BIT(cpu->L, 0);
            return 8;
        }

        case 0xc6: { // SET 0, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            BYTE_SET_BIT(mem, 0);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0xc7: { // SET 0, A
            BYTE_SET_BIT(cpu->A, 0);
            return 8;
        }

        case 0xc8: { // SET 1, B
            BYTE_SET_BIT(cpu->B, 1);
            return 8;
        }
        
        case 0xc9: { // SET 1, C
            BYTE_SET_BIT(cpu->C, 1);
            return 8;
        }

        case 0xca: { // SET 1, D
            BYTE_SET_BIT(cpu->D, 1);
            return 8;
        }

        case 0xcb: { // SET 1, E
            BYTE_SET_BIT(cpu->E, 1);
            return 8;
        }

        case 0xcc: { // SET 1, H
            BYTE_SET_BIT(cpu->H, 1);
            return 8;
        }

        case 0xcd: { // SET 1, L
            BYTE_SET_BIT(cpu->L, 1);
            return 8;
        }

        case 0xce: { // SET 1, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            BYTE_SET_BIT(mem, 1);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0xcf: { // SET 1, A
            BYTE_SET_BIT(cpu->A, 1);
            return 8;
        }

        case 0xd0: { // SET 2, B
            BYTE_SET_BIT(cpu->B, 2);
            return 8;
        }
        
        case 0xd1: { // SET 2, C
            BYTE_SET_BIT(cpu->C, 2);
            return 8;
        }

        case 0xd2: { // SET 2, D
            BYTE_SET_BIT(cpu->D, 2);
            return 8;
        }

        case 0xd3: { // SET 2, E
            BYTE_SET_BIT(cpu->E, 2);
            return 8;
        }

        case 0xd4: { // SET 2, H
            BYTE_SET_BIT(cpu->H, 2);
            return 8;
        }

        case 0xd5: { // SET 2, L
            BYTE_SET_BIT(cpu->L, 2);
            return 8;
        }

        case 0xd6: { // SET 2, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            BYTE_SET_BIT(mem, 2);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0xd7: { // SET 2, A
            BYTE_SET_BIT(cpu->A, 2);
            return 8;
        }

        case 0xd8: { // SET 3, B
            BYTE_SET_BIT(cpu->B, 3);
            return 8;
        }
        
        case 0xd9: { // SET 3, C
            BYTE_SET_BIT(cpu->C, 3);
            return 8;
        }

        case 0xda: { // SET 3, D
            BYTE_SET_BIT(cpu->D, 3);
            return 8;
        }

        case 0xdb: { // SET 3, E
            BYTE_SET_BIT(cpu->E, 3);
            return 8;
        }

        case 0xdc: { // SET 3, H
            BYTE_SET_BIT(cpu->H, 3);
            return 8;
        }

        case 0xdd: { // SET 3, L
            BYTE_SET_BIT(cpu->L, 3);
            return 8;
        }

        case 0xde: { // SET 3, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            BYTE_SET_BIT(mem, 3);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0xdf: { // SET 3, A
            BYTE_SET_BIT(cpu->A, 3);
            return 8;
        }

        case 0xe0: { // SET 4, B
            BYTE_SET_BIT(cpu->B, 4);
            return 8;
        }
        
        case 0xe1: { // SET 4, C
            BYTE_SET_BIT(cpu->C, 4);
            return 8;
        }

        case 0xe2: { // SET 4, D
            BYTE_SET_BIT(cpu->D, 4);
            return 8;
        }

        case 0xe3: { // SET 4, E
            BYTE_SET_BIT(cpu->E, 4);
            return 8;
        }

        case 0xe4: { // SET 4, H
            BYTE_SET_BIT(cpu->H, 4);
            return 8;
        }

        case 0xe5: { // SET 4, L
            BYTE_SET_BIT(cpu->L, 4);
            return 8;
        }

        case 0xe6: { // SET 4, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            BYTE_SET_BIT(mem, 4);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0xe7: { // SET 4, A
            BYTE_SET_BIT(cpu->A, 4);
            return 8;
        }

        case 0xe8: { // SET 5, B
            BYTE_SET_BIT(cpu->B, 5);
            return 8;
        }
        
        case 0xe9: { // SET 5, C
            BYTE_SET_BIT(cpu->C, 5);
            return 8;
        }

        case 0xea: { // SET 5, D
            BYTE_SET_BIT(cpu->D, 5);
            return 8;
        }

        case 0xeb: { // SET 5, E
            BYTE_SET_BIT(cpu->E, 5);
            return 8;
        }

        case 0xec: { // SET 5, H
            BYTE_SET_BIT(cpu->H, 5);
            return 8;
        }

        case 0xed: { // SET 5, L
            BYTE_SET_BIT(cpu->L, 5);
            return 8;
        }

        case 0xee: { // SET 5, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            BYTE_SET_BIT(mem, 5);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0xef: { // SET 5, A
            BYTE_SET_BIT(cpu->A, 5);
            return 8;
        }

        case 0xf0: { // SET 6, B
            BYTE_SET_BIT(cpu->B, 6);
            return 8;
        }
        
        case 0xf1: { // SET 6, C
            BYTE_SET_BIT(cpu->C, 6);
            return 8;
        }

        case 0xf2: { // SET 6, D
            BYTE_SET_BIT(cpu->D, 6);
            return 8;
        }

        case 0xf3: { // SET 6, E
            BYTE_SET_BIT(cpu->E, 6);
            return 8;
        }

        case 0xf4: { // SET 6, H
            BYTE_SET_BIT(cpu->H, 6);
            return 8;
        }

        case 0xf5: { // SET 6, L
            BYTE_SET_BIT(cpu->L, 6);
            return 8;
        }

        case 0xf6: { // SET 6, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            BYTE_SET_BIT(mem, 6);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0xf7: { // SET 6, A
            BYTE_SET_BIT(cpu->A, 6);
            return 8;
        }

        case 0xf8: { // SET 7, B
            BYTE_SET_BIT(cpu->B, 7);
            return 8;
        }
        
        case 0xf9: { // SET 7, C
            BYTE_SET_BIT(cpu->C, 7);
            return 8;
        }

        case 0xfa: { // SET 7, D
            BYTE_SET_BIT(cpu->D, 7);
            return 8;
        }

        case 0xfb: { // SET 7, E
            BYTE_SET_BIT(cpu->E, 7);
            return 8;
        }

        case 0xfc: { // SET 7, H
            BYTE_SET_BIT(cpu->H, 7);
            return 8;
        }

        case 0xfd: { // SET 7, L
            BYTE_SET_BIT(cpu->L, 7);
            return 8;
        }

        case 0xfe: { // SET 7, (HL)
            uint8_t mem = mem_read8(cpu->mem, CPU_HL(cpu));
            BYTE_SET_BIT(mem, 7);
            mem_write8(cpu->mem, CPU_HL(cpu), mem);
            return 16;
        }

        case 0xff: { // SET 7, A
            BYTE_SET_BIT(cpu->A, 7);
            return 8;
        }
        default: {
            return 1;
        }
        }
    }

    case 0xcc: { //CALL Z, {16bit}
        uint16_t addr = pc+3;

        if (CPU_ZERO(cpu)) {
            cpu->SP -= 1;
            mem_write8(cpu->mem, cpu->SP, (addr>>8) & 0xff);
            cpu->SP -= 1;
            mem_write8(cpu->mem, cpu->SP, addr&0xff);

            cpu->PC = mem_read16(cpu->mem, pc+1);
            return 24;
        } else {
            cpu->PC = addr;
            return 12;
        }
    }

    case 0xcd: { //CALL {16bit}
        uint16_t addr = pc+3;

        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, (addr>>8) & 0xff);
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, addr&0xff);

        cpu->PC = mem_read16(cpu->mem, pc+1);
        return 24;
    }

    case 0xce: { //ADC A, {8bit}
        uint8_t carry = CPU_CARRY(cpu);
        uint8_t mem   = mem_read8(cpu->mem, pc+1);

        CPU_SET_N(cpu, 0);
        CPU_SET_C_SUMC(cpu, cpu->A, mem, carry);
        CPU_SET_H_SUMC(cpu, cpu->A, mem, carry);

        cpu->A  += (mem + carry);
        cpu->PC += 2;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 8;
    }

    case 0xcf: { //RST 08H
        uint16_t addr = cpu->PC + 1;

        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, (addr>>8) & 0xff);
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, addr&0xff);
        cpu->PC = 0x08;
        return 16;
    }

    case 0xd0: { //RET NC
        if (!CPU_CARRY(cpu)) {
            uint8_t lsb = mem_read8(cpu->mem, cpu->SP);
            cpu->SP   += 1;
            uint8_t msb = mem_read8(cpu->mem, cpu->SP);
            cpu->SP   += 1;
            cpu->PC    = BYTE_16BIT(msb, lsb);
            return 20;
        } else {
            cpu->PC += 1;
            return 8;
        }
    }

    case 0xd1: { //POP DE
        cpu->E   = mem_read8(cpu->mem, cpu->SP);
        cpu->SP += 1;
        cpu->D   = mem_read8(cpu->mem, cpu->SP);
        cpu->SP += 1;
        cpu->PC += 1;
        return 12;
    }

    case 0xd2: { //JP NC, {16bit}
        if (!CPU_CARRY(cpu)) {
            cpu->PC = mem_read16(cpu->mem, pc+1);
            return 16;
        } else {
            cpu->PC += 3;
            return 12;
        }
    }

    case 0xd4: { //CALL NC, {16bit}
        uint16_t addr = pc+3;

        if (!CPU_CARRY(cpu)) {
            cpu->SP -= 1;
            mem_write8(cpu->mem, cpu->SP, (addr>>8) & 0xff);
            cpu->SP -= 1;
            mem_write8(cpu->mem, cpu->SP, addr&0xff);

            cpu->PC = mem_read16(cpu->mem, pc+1);
            return 24;
        } else {
            cpu->PC = addr;
            return 12;
        }
    }

    case 0xd5: { //PUSH DE
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, cpu->D);
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, cpu->E);
        cpu->PC += 1;
        return 16;
    }

    case 0xd6: { //SUB A, {8bit}
        uint8_t mem = mem_read8(cpu->mem, pc+1);

        CPU_SET_N(cpu, 1);
        CPU_SET_C_SUB(cpu, cpu->A, mem);
        CPU_SET_H_SUB(cpu, cpu->A, mem);

        cpu->A  -= mem;
        cpu->PC += 2;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 8;
    }

    case 0xd7: { //RST 10H
        uint16_t addr = cpu->PC + 1;

        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, (addr>>8) & 0xff);
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, addr&0xff);
        cpu->PC = 0x10;
        return 16;
    }

    case 0xd8: { //RET C
        if (CPU_CARRY(cpu)) {
            uint8_t lsb = mem_read8(cpu->mem, cpu->SP);
            cpu->SP   += 1;
            uint8_t msb = mem_read8(cpu->mem, cpu->SP);
            cpu->SP   += 1;
            cpu->PC    = BYTE_16BIT(msb, lsb);
            return 20;
        } else {
            cpu->PC += 1;
            return 8;
        }
    }

    case 0xd9: { //RETI
        uint8_t lsb = mem_read8(cpu->mem, cpu->SP);
        cpu->SP += 1;
        uint8_t msb = mem_read8(cpu->mem, cpu->SP);
        cpu->SP += 1;
        cpu->PC = BYTE_16BIT(msb, lsb);
        cpu->interrupt->master_enable = 1;
        return 16;
    }

    case 0xda: { //JP C, {16bit}
        if (CPU_CARRY(cpu)) {
            cpu->PC = mem_read16(cpu->mem, pc+1);
            return 16;
        } else {
            cpu->PC += 3;
            return 12;
        }
    }

    case 0xdc: { //CALL C, {16bit}
        uint16_t addr = pc+3;

        if (CPU_CARRY(cpu)) {
            cpu->SP -= 1;
            mem_write8(cpu->mem, cpu->SP, (addr>>8) & 0xff);
            cpu->SP -= 1;
            mem_write8(cpu->mem, cpu->SP, addr&0xff);

            cpu->PC = mem_read16(cpu->mem, pc+1);
            return 24;
        } else {
            cpu->PC = addr;
            return 12;
        }
    }

    case 0xde: { //SBC A, {8bit}
        uint8_t mem   = mem_read8(cpu->mem, pc+1);
        uint8_t carry = CPU_CARRY(cpu);

        CPU_SET_N(cpu, 1);
        CPU_SET_C_SUBC(cpu, cpu->A, mem, carry);
        CPU_SET_H_SUBC(cpu, cpu->A, mem, carry);

        cpu->A  -= (mem + carry);
        cpu->PC += 2;

        CPU_SET_Z(cpu, cpu->A == 0);
        return 8;
    }

    case 0xdf: { //RST 18H
        uint16_t addr = cpu->PC + 1;
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, (addr>>8) & 0xff);
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, addr&0xff);
        cpu->PC = 0x18;
        return 16;
    }

    case 0xe0: { //LDH ({8bit}), A
        mem_write8(cpu->mem, 0xff00+mem_read8(cpu->mem, pc+1), cpu->A);
        cpu->PC += 2;
        return 12;
    }

    case 0xe1: { //POP HL
        cpu->L   = mem_read8(cpu->mem, cpu->SP);
        cpu->SP += 1;
        cpu->H   = mem_read8(cpu->mem, cpu->SP);
        cpu->SP += 1;
        cpu->PC += 1;
        return 12;
    }

    case 0xe2: { //LD (C), A
        mem_write8(cpu->mem, 0xff00+cpu->C, cpu->A);
        cpu->PC += 1;
        return 8;
    }

    case 0xe5: { //PUSH HL
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, cpu->H);
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, cpu->L);
        cpu->PC += 1;
        return 16;
    }

    case 0xe6: { //AND A, {8bit}
        uint8_t mem = mem_read8(cpu->mem, pc+1);

        cpu->A  &= mem;
        cpu->PC += 2;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 1);
        CPU_SET_C(cpu, 0);
        return 8;
    }

    case 0xe7: { //RST 20H
        uint16_t addr = cpu->PC + 1;
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, (addr>>8) & 0xff);
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, addr&0xff);
        cpu->PC = 0x20;
        return 16;
    }

    case 0xe8: { //ADD SP, {8bit}
        int16_t signed_16 = (int8_t)mem_read8(cpu->mem, pc+1);

        CPU_SET_Z(cpu, 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H_SUM(cpu, (uint8_t)cpu->SP, (uint8_t)signed_16);
        CPU_SET_C_SUM(cpu, (uint8_t)cpu->SP, (uint8_t)signed_16);

        // H and C flags are set by the unsigned sum of the 8 bits from memory and
        // the lower 8 bits from SP.

        // CPU_SET_H_REG16_SUM(cpu, cpu->SP, (uint8_t)signed_16);
        // CPU_SET_C_REG16_SUM(cpu, cpu->SP, (uint8_t)signed_16);
        // CPU_SET_H_REG16_SUM_SIGNED(cpu, cpu->SP, signed_16);
        // CPU_SET_C_REG16_SUM_SIGNED(cpu, cpu->SP, signed_16);

        cpu->SP  = cpu->SP + signed_16;
        cpu->PC += 2;
        return 16;
    }

    case 0xe9: { //JP HL
        cpu->PC = CPU_HL(cpu);
        return 4;
    }

    case 0xea: { //LD ({16bit}), A
        mem_write8(cpu->mem, mem_read16(cpu->mem, pc+1), cpu->A);
        cpu->PC += 3;
        return 16;
    }

    case 0xee: { //XOR A, {8bit}
        uint8_t mem = mem_read8(cpu->mem, pc+1);

        cpu->A  ^= mem;
        cpu->PC += 2;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, 0);
        return 8;
    }

    case 0xef: { //RST 28H
        uint16_t addr = cpu->PC + 1;
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, (addr>>8) & 0xff);
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, addr&0xff);
        cpu->PC = 0x28;
        return 16;
    }

    case 0xf0: { //LDH A, ({8bit})
        cpu->A   = mem_read8(cpu->mem, 0xff00+mem_read8(cpu->mem, pc+1));
        cpu->PC += 2;
        return 12;
    }

    case 0xf1: { //POP AF
        cpu->F     = mem_read8(cpu->mem, cpu->SP) & 0xf0;
        cpu->SP   += 1;
        cpu->A     = mem_read8(cpu->mem, cpu->SP);
        cpu->SP   += 1;
        cpu->PC   += 1;
        return 12;
    }

    case 0xf2: { //LD A, (C)
        cpu->A   = mem_read8(cpu->mem, 0xff00+cpu->C);
        cpu->PC += 1;
        return 8;
    }

    case 0xf3: { //DI
        cpu->interrupt->master_enable = 0;
        cpu->PC += 1;
        return 4;
    }

    case 0xf5: { //PUSH AF
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, cpu->A);
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, cpu->F);
        cpu->PC += 1;
        return 16;
    }

    case 0xf6: { //OR A, {8bit}
        uint8_t mem = mem_read8(cpu->mem, pc+1);

        cpu->A  |= mem;
        cpu->PC += 2;

        CPU_SET_Z(cpu, cpu->A == 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H(cpu, 0);
        CPU_SET_C(cpu, 0);
        return 8;
    }

    case 0xf7: { //RST 30H
        uint16_t addr = cpu->PC + 1;
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, (addr>>8) & 0xff);
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, addr&0xff);
        cpu->PC = 0x30;
        return 16;
    }

    case 0xf8: { //LD HL, SP+{s8bit}
        int16_t signed_16 = (int8_t)mem_read8(cpu->mem, pc+1);

        CPU_SET_Z(cpu, 0);
        CPU_SET_N(cpu, 0);
        CPU_SET_H_SUM(cpu, (uint8_t)cpu->SP, (uint8_t)signed_16);
        CPU_SET_C_SUM(cpu, (uint8_t)cpu->SP, (uint8_t)signed_16);
        
        signed_16 = cpu->SP + signed_16;

        cpu->H   = signed_16 >> 8;
        cpu->L   = signed_16 & 0xff;
        cpu->PC += 2;
        return 12;
    }

    case 0xf9: { //LD SP, HL
        cpu->SP  = CPU_HL(cpu);
        cpu->PC += 1;
        return 8;
    }

    case 0xfa: { //LD A, ({16bit})
        cpu->A   = mem_read8(cpu->mem, mem_read16(cpu->mem, pc+1));
        cpu->PC += 3;
        return 16;
    }

    case 0xfb: { //EI
        cpu->interrupt->master_enable = 1;
        cpu->PC += 1;
        return 4;
    }

    case 0xfe: { //CP A, {8bit}
        uint8_t mem = mem_read8(cpu->mem, pc+1);

        CPU_SET_N(cpu, 1);
        CPU_SET_Z(cpu,     cpu->A == mem);
        CPU_SET_C_SUB(cpu, cpu->A, mem);
        CPU_SET_H_SUB(cpu, cpu->A, mem);

        cpu->PC += 2;
        return 8;
    }

    case 0xff: { //RST 38H
        uint16_t addr = cpu->PC + 1;
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, (addr>>8) & 0xff);
        cpu->SP -= 1;
        mem_write8(cpu->mem, cpu->SP, addr&0xff);
        cpu->PC = 0x38;
        return 16;
    }

    default: {
        WARN("unknown opcode %02x (last PC was %04x (%02x))\n", opcode, pc, mem_read8(cpu->mem, pc));
        // emu_print_opcode_history(stderr, emu);
        // cpu->PC += 1;
        exit(-1);
        return 1;
    }
    }
}