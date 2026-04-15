#ifndef REGISTERS_H
#define REGISTERS_H

#include "memory.h"

#define REG_JOYP_ADDR                0xff00
#define REG_JOYP_MASK_SELECT_BUTTONS 0x20
#define REG_JOYP_MASK_SELECT_DPAD    0x10

struct ioregs {
    struct mem_write_dev write_dev;
    struct mem_read_dev  read_dev;
};

#endif