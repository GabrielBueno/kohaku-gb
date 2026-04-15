#include "ppu.h"

#include <stddef.h>
#include <assert.h>

enum ppu_result map_addresses(struct ppu *ppu, struct mem *mem);

enum ppu_result ppu_init(struct ppu *ppu, struct mem *mem) {
    assert(ppu != NULL);
    assert(mem != NULL);

    enum ppu_result result;

    if ((result = map_addresses(ppu, mem)) != PPU_OK)
        return result;

    return PPU_OK;
}

enum ppu_result ppu_close(struct ppu *ppu) {
    return PPU_OK;
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