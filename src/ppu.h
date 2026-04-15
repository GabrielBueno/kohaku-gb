#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include "memory.h"

enum ppu_result {
    PPU_OK  = 0,
    PPU_ERR = 1,
};

struct ppu {
    uint8_t vram[8 * 1024];
    uint8_t oam[256];
    struct mem *mem;
};

enum ppu_result ppu_init(struct ppu *ppu, struct mem *mem);
enum ppu_result ppu_close(struct ppu *ppu);

#endif