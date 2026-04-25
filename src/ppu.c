#include "ppu.h"

#include <stddef.h>
#include <assert.h>
#include "log.h"



#define STAT_FLAG_LYC_INT   0x40
#define STAT_FLAG_MODE2_INT 0x20
#define STAT_FLAG_MODE1_INT 0x10
#define STAT_FLAG_MODE0_INT 0x08
#define STAT_FLAG_LYC_EQ_LY 0x04
#define STAT_FLAG_MODE      0x03

#define CTRL_FLAG_PPU_ENABLE                   0x80
#define CTRL_FLAG_WIN_TILE_MAP_AREA            0x40
#define CTRL_FLAG_WIN_ENABLE                   0x20
#define CTRL_FLAG_BG_AND_WINDOW_ADDRESING_MODE 0x10
#define CTRL_FLAG_BG_TILE_MAP_AREA             0x08
#define CTRL_FLAG_OBJ_SIZE                     0x04
#define CTRL_FLAG_OBJ_ENABLE                   0x02
#define CTRL_FLAG_BG_WIN_ENABLE                0x01

#define ADDRESSING_MODE_8000 1
#define ADDRESSING_MODE_8800 0

static uint8_t pallete[][3] = {
    { 0xf2, 0xe5, 0xd5, },
    { 0xbf, 0x99, 0x95, },
    { 0x8c, 0x5a, 0x5a, },
    { 0x40, 0x01, 0x01, },
};


static enum ppu_result map_addresses(struct ppu *ppu, struct mem *mem);
static uint16_t get_tile_address(uint8_t index, uint8_t method);

static void render_scanline(struct ppu *ppu);
static void render_scanline_bg(struct ppu *ppu, int ly);
static void render_scanline_win(struct ppu *ppu, int ly);
static void render_scanline_obj(struct ppu *ppu, int ly);

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

    ppu->int_stat_requested     = 0;
    ppu->int_vblank_requested   = 0;
    ppu->vblank_ready_to_render = 0;

    enum ppu_result result;

    if ((result = map_addresses(ppu, mem)) != PPU_OK)
        return result;

    return PPU_OK;
}

enum ppu_result ppu_close(struct ppu *ppu) {
    return PPU_OK;
}

void ppu_tick(struct ppu *ppu, int cycles) {
    uint8_t lcdc = ppu->lcd_ctrl;

    while (cycles > 0) {
        int remaining = 0;

        switch (ppu->mode) {
        case 2: remaining = 80     - ppu->line_dots; break;
        case 3: remaining = 80+172 - ppu->line_dots; break;
        case 0: remaining = 456    - ppu->line_dots; break;
        case 1: remaining = 456    - ppu->line_dots; break;
        }

        int step = (cycles < remaining) ? cycles : remaining;

        cycles          -= step;
        ppu->line_dots  += step;
        ppu->frame_dots += step;

        if (ppu->line_dots >= 456) {
            ppu->line_dots -= 456;
            ppu->ly++;

            if (ppu->ly == 144) {
                ppu->mode                   = 1;
                ppu->vblank_ready_to_render = 1;

                interrupt_request(ppu->interrupt, INTERRUPT_VBLANK, INTERRUPT_REQUESTED);

                if (ppu->lcd_stat & STAT_FLAG_MODE1_INT)
                    interrupt_request(ppu->interrupt, INTERRUPT_LCD, INTERRUPT_REQUESTED);
            } else if (ppu->ly > 153) {
                ppu->mode = 2;
                ppu->ly   = 0;
            }

            if (ppu->lyc == ppu->ly) {
                ppu->lcd_stat |= STAT_FLAG_LYC_EQ_LY;

                if (ppu->lcd_stat & STAT_FLAG_LYC_INT)
                    interrupt_request(ppu->interrupt, INTERRUPT_LCD, INTERRUPT_REQUESTED);
            } else {
                ppu->lcd_stat &= ~STAT_FLAG_LYC_EQ_LY;
            }
        }

        if (ppu->mode == 1)
            continue;

        if (ppu->mode != 2 && ppu->line_dots <= 80) {
            ppu->mode = 2;

            if (ppu->lcd_stat & STAT_FLAG_MODE2_INT)
                interrupt_request(ppu->interrupt, INTERRUPT_LCD, INTERRUPT_REQUESTED);
        } else if (ppu->mode != 3 && ppu->line_dots <= 80+172) {
            ppu->mode = 3;
        } else if (ppu->mode != 0 && ppu->line_dots <= 456) {
            ppu->mode = 0;

            if (ppu->lcd_stat & STAT_FLAG_MODE0_INT)
                interrupt_request(ppu->interrupt, INTERRUPT_LCD, INTERRUPT_REQUESTED);

            render_scanline(ppu);
        }
    }

    ppu->lcd_stat = (ppu->lcd_stat & 0xfc) | ppu->mode;
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

static void render_scanline(struct ppu *ppu) {
    int ly = ppu->ly;

    if (ly < 144) {
        render_scanline_bg(ppu, ly);
        render_scanline_win(ppu, ly);
        render_scanline_obj(ppu, ly);
    }
}

static void render_scanline_bg(struct ppu *ppu, int ly) {
    uint8_t lcdc        = ppu->lcd_ctrl;
    uint8_t bgenable    = lcdc & CTRL_FLAG_BG_WIN_ENABLE;
    uint8_t scy         = ppu->scy;
    uint8_t scx         = ppu->scx;
    uint8_t addressing  = (lcdc & CTRL_FLAG_BG_AND_WINDOW_ADDRESING_MODE) ? ADDRESSING_MODE_8000 : ADDRESSING_MODE_8800;
    uint16_t from       = (lcdc & CTRL_FLAG_BG_TILE_MAP_AREA) ? 0x9c00 : 0x9800;

    uint8_t *vram        = ppu->vram;
    uint8_t *framebuffer = ppu->textures.screen;

    if (!bgenable) {
        for (int x = 0; x < 160; x++) {
            int framebuffer_idx = (ly * 160 * 3) + (x * 3);

            framebuffer[framebuffer_idx]   = 0xff;
            framebuffer[framebuffer_idx+1] = 0xff;
            framebuffer[framebuffer_idx+2] = 0xff;
        }

        return;
    }

    for (int x = 0; x < 160; x++) {
        int bg_x = (scx +  x) & 0xff;
        int bg_y = (scy + ly) & 0xff;

        int tile_col = bg_x / 8;
        int tile_row = bg_y / 8;

        uint16_t map_addr = from + (tile_row * 32) + tile_col;
        uint8_t  tile_idx = vram[map_addr - 0x8000];

        int tile_x = bg_x % 8;
        int tile_y = bg_y % 8;

        uint16_t tile_addr = get_tile_address(tile_idx, addressing);
        uint8_t  lsb       = vram[(tile_addr + (tile_y * 2)) - 0x8000];
        uint8_t  msb       = vram[(tile_addr + (tile_y * 2) + 1) - 0x8000];

        int bit = 7 - tile_x;

        uint8_t color_idx = ((((msb >> bit) & 1) << 1)) | ((lsb >> bit) & 1);
        uint8_t *color = pallete[color_idx];

        int framebuffer_idx = (ly * 160 * 3) + (x * 3);

        framebuffer[framebuffer_idx]   = color[0];
        framebuffer[framebuffer_idx+1] = color[1];
        framebuffer[framebuffer_idx+2] = color[2];
    }
}

static void render_scanline_win(struct ppu *ppu, int ly) {
    uint8_t lcdc        = ppu->lcd_ctrl;
    uint8_t bgenable    = lcdc & CTRL_FLAG_BG_WIN_ENABLE;
    uint8_t winenable   = lcdc & CTRL_FLAG_WIN_ENABLE;
    uint8_t wy          = ppu->wy;
    uint8_t wx          = ppu->wx;
    uint8_t addressing  = (lcdc & CTRL_FLAG_BG_AND_WINDOW_ADDRESING_MODE) ? ADDRESSING_MODE_8000 : ADDRESSING_MODE_8800;
    uint16_t from       = (lcdc & CTRL_FLAG_WIN_TILE_MAP_AREA) ? 0x9c00 : 0x9800;

    uint8_t *vram        = ppu->vram;
    uint8_t *framebuffer = ppu->textures.screen;

    if (!bgenable || !winenable)
        return;

    if (ly < wy)
        return;

    int win_row  = ly - wy;
    int screen_x = wx - 7;

    for (int x = screen_x; x < 160; x++) {
        int win_col = x - screen_x;

        int tile_col = win_col / 8;
        int tile_row = win_row / 8;
        int tile_x   = win_col % 8;
        int tile_y   = win_row % 8;

        uint16_t map_addr = from + (tile_row * 32) + tile_col;
        uint8_t  tile_idx = vram[map_addr - 0x8000];

        uint16_t tile_addr = get_tile_address(tile_idx, addressing);
        uint8_t  lsb       = vram[(tile_addr + (tile_y * 2)) - 0x8000];
        uint8_t  msb       = vram[(tile_addr + (tile_y * 2) + 1) - 0x8000];

        int bit = 7 - tile_x;

        uint8_t color_idx = ((((msb >> bit) & 1) << 1)) | ((lsb >> bit) & 1);
        uint8_t *color = pallete[color_idx];

        int framebuffer_idx = (ly * 160 * 3) + (x * 3);
        
        framebuffer[framebuffer_idx]   = color[0];
        framebuffer[framebuffer_idx+1] = color[1];
        framebuffer[framebuffer_idx+2] = color[2];
    }
}

static void render_scanline_obj(struct ppu *ppu, int ly) {
    uint8_t  lcdc       = ppu->lcd_ctrl;
    uint8_t  obj_enable = lcdc & CTRL_FLAG_OBJ_ENABLE;
    uint8_t  obj_size   = lcdc & CTRL_FLAG_OBJ_SIZE;

    uint8_t *vram        = ppu->vram;
    uint8_t *oam         = ppu->oam;
    uint8_t *framebuffer = ppu->textures.screen;

    for (uint16_t addr = 0xfe00; addr <= 0xfe9f; addr += 4) {
        int obj_y = oam[addr - 0xfe00] - 16;
        
        if (ly < obj_y || ly >= obj_y+8)
            continue;

        int      obj_x     = oam[(addr+1) - 0xfe00] - 8;
        uint8_t  tile_idx  = oam[(addr+2) - 0xfe00];
        uint8_t  attr      = oam[(addr+3) - 0xfe00];
        uint16_t tile_addr = get_tile_address(tile_idx, ADDRESSING_MODE_8000);

        int tile_row = ly - obj_y;

        uint8_t lsb = vram[(tile_addr + (tile_row * 2)) - 0x8000];
        uint8_t msb = vram[(tile_addr + (tile_row * 2) + 1) - 0x8000];

        for (int x = obj_x; x < obj_x+8; x++) {
            if (x < 0)
                continue;

            if (x >= 160)
                break;

            int bit = 7 - (x - obj_x);

            uint8_t color_idx = ((((msb >> bit) & 1) << 1)) | ((lsb >> bit) & 1);
            
            if (color_idx == 0)
                continue;

            uint8_t *color = pallete[color_idx];

            int framebuffer_idx = (ly * 160 * 3) + (x * 3);
            
            framebuffer[framebuffer_idx]   = color[0];
            framebuffer[framebuffer_idx+1] = color[1];
            framebuffer[framebuffer_idx+2] = color[2];
        }
    }
}

static uint16_t get_tile_address(uint8_t index, uint8_t addressing) {
    uint16_t base_addr = 0x8000;
    int      offset    = 16*index;

    if (addressing == ADDRESSING_MODE_8800) {
        base_addr = 0x9000;
        offset    = 16 * ((int8_t)index);
    }

    return base_addr + offset;
}