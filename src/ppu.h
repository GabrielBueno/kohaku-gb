#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include "memory.h"
#include "interrupt.h"
#include "constants.h"

enum ppu_result {
    PPU_OK  = 0,
    PPU_ERR = 1,
};

struct ppu {
    struct {
        uint8_t bg[256 * 256 * 3];
        uint8_t screen[GAMEBOY_LCD_HEIGHT * GAMEBOY_LCD_WIDTH * 3];
    } textures;

    uint8_t vram[8 * 1024];
    uint8_t oam[256];
    struct mem *mem;
    struct interrupt *interrupt;
    int frame_dots;
    int line_dots;
    uint8_t lcd_ctrl;
    uint8_t lcd_stat;
    uint8_t scy;
    uint8_t scx;
    uint8_t ly;
    uint8_t lyc;
    uint8_t bgp;
    uint8_t obp0;
    uint8_t obp1;
    uint8_t wy;
    uint8_t wx;
    uint8_t mode;
    uint8_t enabled;
};

enum ppu_result ppu_init(struct ppu *ppu, struct mem *mem, struct interrupt *interrupt);
enum ppu_result ppu_close(struct ppu *ppu);

void ppu_tick(struct ppu *ppu, int cycles);
void ppu_render(struct ppu *ppu);
void ppu_dma(struct ppu *ppu, uint8_t value);

#endif