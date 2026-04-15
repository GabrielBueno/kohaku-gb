#ifndef REGISTERS_H
#define REGISTERS_H

#include "memory.h"

#define REG_ADDR_JOYP       0xff00
#define REG_ADDR_HRAM_START 0xff80
#define REG_ADDR_HRAM_END   0xfffe

struct ioregs {
    uint8_t high_ram[127];
    struct mem_write_dev write_dev;
    struct mem_read_dev read_dev;
};

#endif