#include "ppu.h"

#include <stddef.h>
#include <assert.h>

#define DOTS_PER_SCANLINE 460
#define DOTS_PER_FRAME    70224
#define LINES_PER_FRAME   153

#define STAT_FLAG_LYC_INT   0x40
#define STAT_FLAG_MODE2_INT 0x20
#define STAT_FLAG_MODE1_INT 0x10
#define STAT_FLAG_MODE0_INT 0x08
#define STAT_FLAG_LYC_EQ_LY 0x04
#define STAT_FLAG_MODE      0x03

#define CTRL_FLAG_BG_AND_WINDOW_ADDRESING_MODE 0x10
#define CTRL_FLAG_BG_TILE_MAP_AREA             0x08
#define CTRL_FLAG_WIN_TILE_MAP_AREA            0x40
#define CTRL_FLAG_BG_WIN_ENABLE                0x01

#define ADDRESSING_MODE_8000 1
#define ADDRESSING_MODE_8800 0

static uint8_t pallete[][3] = {
    { 0x33, 0x33, 0x33, },
    { 0x99, 0x99, 0x99, },
    { 0xee, 0xee, 0xee, },
    { 0xff, 0xff, 0xff, },
};

static enum ppu_result map_addresses(struct ppu *ppu, struct mem *mem);

static void get_tile(struct ppu *ppu, uint8_t index, uint8_t method, uint8_t *tile_data);
static void get_obj_tile(struct ppu *ppu, uint8_t index, uint8_t *tile_data);
static void get_bgwin_tile(struct ppu *ppu, uint8_t index, uint8_t *tile_data);

static void render_bg(struct ppu *ppu);
static void render_win(struct ppu *ppu);
static void render_obj(struct ppu *ppu);

enum ppu_result ppu_init(struct ppu *ppu, struct mem *mem, struct interrupt *interrupt) {
    assert(ppu       != NULL);
    assert(mem       != NULL);
    assert(interrupt != NULL);

    ppu->mem       = mem;
    ppu->interrupt = interrupt;

    ppu->lcd_ctrl   = 0x91;
    ppu->lcd_stat   = 0x00;
    ppu->scy        = 0x00;
    ppu->scx        = 0x00;
    ppu->lyc        = 0x00;
    ppu->ly         = 0x00;
    ppu->bgp        = 0x00;
    ppu->obp0       = 0x00;
    ppu->obp1       = 0xff;
    ppu->wy         = 0x00;
    ppu->wx         = 0x00;
    ppu->frame_dots = 0;
    ppu->line_dots  = 0;
    ppu->mode       = 2;

    enum ppu_result result;

    if ((result = map_addresses(ppu, mem)) != PPU_OK)
        return result;

    return PPU_OK;
}

enum ppu_result ppu_close(struct ppu *ppu) {
    return PPU_OK;
}

void ppu_tick(struct ppu *ppu, int cycles) {
    int line_dots   = ppu->line_dots;
    int frame_dots  = ppu->frame_dots;
    uint8_t stat    = ppu->lcd_stat;
    uint8_t ly      = ppu->ly;
    uint8_t lyc     = ppu->lyc;
    uint8_t mode    = stat & STAT_FLAG_MODE;
    uint8_t int_req = 0;

    for (int t = 0; t < cycles; t++) {
        line_dots  = (line_dots  + 1) % DOTS_PER_SCANLINE;
        frame_dots = (frame_dots + 1) % DOTS_PER_FRAME;

        if (frame_dots == 0)
            ly = (ly + 1) % LINES_PER_FRAME;
        
        if (ly >= 144) {
            mode = 1;

            if ((stat & STAT_FLAG_MODE1_INT) && !int_req) {
                interrupt_request(ppu->interrupt, INTERRUPT_LCD, INTERRUPT_REQUESTED);
                int_req = 1;
            }
        } else if (line_dots <= 80) {
            mode = 2;

            if ((stat & STAT_FLAG_MODE2_INT) && !int_req) {
                interrupt_request(ppu->interrupt, INTERRUPT_LCD, INTERRUPT_REQUESTED);
                int_req = 1;
            }
        } else if (line_dots <= 172) {
            mode = 3;
        } else {
            mode = 0;

            if ((stat & STAT_FLAG_MODE0_INT) && !int_req) {
                interrupt_request(ppu->interrupt, INTERRUPT_LCD, INTERRUPT_REQUESTED);
                int_req = 1;
            }
        }

        if (ly == lyc && !(stat & STAT_FLAG_LYC_EQ_LY)) {
            stat = stat | STAT_FLAG_LYC_EQ_LY;

            if ((stat & STAT_FLAG_LYC_INT) && !int_req) {
                interrupt_request(ppu->interrupt, INTERRUPT_LCD, INTERRUPT_REQUESTED);
                int_req = 1;
            }
        }
    }

    ppu->line_dots  = line_dots;
    ppu->frame_dots = frame_dots;
    ppu->mode       = mode;
    ppu->ly         = ly;
    ppu->lcd_stat   = (stat & 0xf4) | mode;
}

void ppu_render(struct ppu *ppu) {

}

void ppu_dma(struct ppu *ppu, uint8_t value) {
    uint16_t hi = ((uint16_t)value) << 8;

    for (uint16_t lo = 0x00; lo <= 0x9f; lo++) {
        uint16_t from = hi+lo;
        uint16_t to   = 0xfe00+lo;

        mem_write8(ppu->mem, to, mem_read8(ppu->mem, from));
    }
}

enum ppu_result map_addresses(struct ppu *ppu, struct mem *mem) {
    for (uint8_t page = 0x80; page <= 0x9f; page++) {
        uint8_t *offset_ptr = &ppu->vram[(page - 0x80) << 8];

        mem->write_dev[page] = &MEM_UNMAPPED_WRITE_DEV;
        mem->read_dev[page]  = &MEM_UNMAPPED_READ_DEV;

        mem->write_direct[page] = offset_ptr;
        mem->read_direct[page]  = offset_ptr;
    }

    mem->write_dev[0xfe] = &MEM_UNMAPPED_WRITE_DEV;
    mem->read_dev[0xfe]  = &MEM_UNMAPPED_READ_DEV;

    mem->write_direct[0xfe] = ppu->oam;
    mem->read_direct[0xfe]  = ppu->oam;

    return PPU_OK;
}

static void render_bg(struct ppu *ppu) {
    uint8_t  lcdc       = ppu->lcd_ctrl;
    uint8_t  bgenable   = (lcdc & CTRL_FLAG_BG_WIN_ENABLE);
    uint8_t  addressing = (lcdc & CTRL_FLAG_BG_AND_WINDOW_ADDRESING_MODE) ? ADDRESSING_MODE_8000 : ADDRESSING_MODE_8800;
    uint16_t from       = (lcdc & CTRL_FLAG_BG_TILE_MAP_AREA) ? 0x9800 : 0x9c00;
    uint16_t to         = from + 1024;
    uint8_t *map        = ppu->textures.bg;

    if (!bgenable) {
        for (int i = 0; i < 256*256*3; i++)
            ppu->textures.bg[i] = 0xff;

        return;
    }

    uint8_t tile_data[16];

    for (uint16_t addr = from; addr < to; addr++) {
        uint8_t index = mem_read8(ppu->mem, addr);

        int texture_x = 0;
        int texture_y = 0;

        get_tile(ppu, index, addressing, tile_data);

        for (int tile_idx = 0; tile_idx < 16; tile_idx += 2) {
            uint8_t lsb = tile_data[tile_idx];
            uint8_t msb = tile_data[tile_idx+1];

            for (int bit = 7; bit >= 1; bit--) {
                uint8_t *color = pallete[(((msb >> bit) << 1) | (lsb >> bit)) & 0x3];

                int x = (texture_x + bit) * 3;
                int y = (texture_y + (tile_idx / 2)) * 3;
                int i = (y * 256) + x;

                map[i]   = color[0];
                map[i+1] = color[1];
                map[i+2] = color[2];
            }
        }
    }
}

static void render_win(struct ppu *ppu) {

}

static void render_obj(struct ppu *ppu) {

}

static void get_tile(struct ppu *ppu, uint8_t index, uint8_t addressing, uint8_t *tile_data) {
    uint16_t base_addr = 0x8000;
    int      offset    = 16*index;

    if (addressing == ADDRESSING_MODE_8800) {
        base_addr = 0x9000;
        offset    = 16 * ((int8_t)index);
    }

    uint16_t addr = base_addr + offset;

    for (int i = 0; i < 16; i += 2) {
        uint8_t lsb = mem_read8(ppu->mem, addr+i);
        uint8_t msb = mem_read8(ppu->mem, addr+i+1);

        tile_data[i]   = lsb;
        tile_data[i+1] = msb;
    }
}

static void get_obj_tile(struct ppu *ppu, uint8_t index, uint8_t *tile_data) {
    get_tile(ppu, index, ADDRESSING_MODE_8000, tile_data);
}

static void get_bgwin_tile(struct ppu *ppu, uint8_t index, uint8_t *tile_data) {
    uint8_t ctrl       = ppu->lcd_ctrl;
    uint8_t addressing = (ctrl & CTRL_FLAG_BG_AND_WINDOW_ADDRESING_MODE) ? ADDRESSING_MODE_8000 : ADDRESSING_MODE_8800;

    get_tile(ppu, index, addressing, tile_data);
}