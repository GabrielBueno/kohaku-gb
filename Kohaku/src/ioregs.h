#ifndef REGISTERS_H
#define REGISTERS_H

#include "memory.h"

#define REG_ADDR_JOYP        0xff00
#define REG_ADDR_HRAM_START  0xff80
#define REG_ADDR_HRAM_END    0xfffe

#define REG_ADDR_SERIAL_TRANSFER_DATA    0xff01
#define REG_ADDR_SERIAL_TRANSFER_CONTROL 0xff02

#define REG_ADDR_TIMER_DIV  0xff04
#define REG_ADDR_TIMER_TIMA 0xff05
#define REG_ADDR_TIMER_TMA  0xff06
#define REG_ADDR_TIMER_TAC  0xff07

#define REG_ADDR_PPU_LCD_CTRL 0xff40
#define REG_ADDR_PPU_LCD_STAT 0xff41
#define REG_ADDR_PPU_SCY      0xff42
#define REG_ADDR_PPU_SCX      0xff43
#define REG_ADDR_PPU_LY       0xff44
#define REG_ADDR_PPU_LYC      0xff45
#define REG_ADDR_PPU_DMA      0xff46
#define REG_ADDR_PPU_BGP      0xff47
#define REG_ADDR_PPU_OBP0     0xff48
#define REG_ADDR_PPU_OBP1     0xff49
#define REG_ADDR_PPU_WY       0xff4a
#define REG_ADDR_PPU_WX       0xff4b


#define REG_ADDR_INTERRUPT_FLAG   0xff0f
#define REG_ADDR_INTERRUPT_ENABLE 0xffff

struct ioregs {
    uint8_t high_ram[127];
    struct mem_write_dev write_dev;
    struct mem_read_dev read_dev;
};

#endif