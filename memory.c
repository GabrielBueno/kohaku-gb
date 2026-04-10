#include "memory.h"

#include <stdlib.h>
#include "log.h"

static void unmapped_write(void* target, uint16_t addr, uint8_t value) {
    FATAL("writing to an unmapped address (addr=%02x, value=%02x)", addr, value);
}

struct write_device unmapped_write_device = {
    .target = NULL,
    .write  = unmapped_write,
};

void memmap_init(struct memmap* memmap) {
    for (uint16_t page = 0x00; page <= 0xff; page++) {
        memmap->write_pages[page] = &unmapped_write_device;
        memmap->read_pages[page]  = NULL;
    }
}

uint8_t read8(struct memmap *memmap, uint16_t addr) {
    uint8_t* page = memmap->read_pages[addr >> 8];
       
#ifndef MEMMAP_SKIP_NULL_CHECK
    if (page == NULL) {
        FATAL("reading from an unmapped address (addr=%02x)", addr);
    }
#endif

    return page[addr & 0xff];
}

void write8(struct memmap *memmap, uint16_t addr, uint8_t value) {
    struct write_device* dev = memmap->write_pages[addr >> 8];
    
#ifndef MEMMAP_SKIP_NULL_CHECK
    if (dev == NULL) {
        FATAL("writing to a device that is set to NULL (addr=%02x)", addr);
    }
#endif

    dev->write(dev->target, addr, value);
}