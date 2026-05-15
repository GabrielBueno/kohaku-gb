#include "memory.h"

#include <stddef.h>
#include "log.h"
#include "macros.h"

static void unmapped_write(void *target, uint16_t addr, uint8_t value) {
    FATAL("writing to an unmapped address (addr=%02x, value=%02x)", addr, value);
}

static uint8_t unmapped_read(void *target, uint16_t addr) {
    FATAL("reading from an unmapped address (addr=%02x)", addr);
    return 0;
}

struct mem_write_dev MEM_UNMAPPED_WRITE_DEV = { .target = NULL, .write = unmapped_write };
struct mem_read_dev  MEM_UNMAPPED_READ_DEV  = { .target = NULL, .read  = unmapped_read };

void mem_init(struct mem *mem) {
    for (uint16_t page = 0x00; page <= 0xff; page++) {
        mem->write_dev[page]    = &MEM_UNMAPPED_WRITE_DEV;
        mem->read_dev[page]     = &MEM_UNMAPPED_READ_DEV;
        mem->write_direct[page] = NULL;
        mem->read_direct[page]  = NULL;
    }
}

uint8_t mem_read8(struct mem *mem, uint16_t addr) {
    uint8_t page   = addr >> 8;
    uint8_t offset = addr & 0xff;

    uint8_t* direct = mem->read_direct[page];

    if (direct != NULL)
        return direct[offset];

    struct mem_read_dev *dev = mem->read_dev[page];

    return dev->read(dev->target, addr);
}

uint16_t mem_read16(struct mem *mem, uint16_t addr) {
    uint8_t lsb = mem_read8(mem, addr);
    uint8_t msb = mem_read8(mem, addr+1);

    return BYTE_16BIT(msb, lsb);
}

void mem_write8(struct mem *mem, uint16_t addr, uint8_t value) {
    uint8_t page   = addr >> 8;
    uint8_t offset = addr & 0xff;

    uint8_t *direct = mem->write_direct[page];

    if (direct != NULL) {
        direct[offset] = value;
        return;
    }

    struct mem_write_dev *dev = mem->write_dev[page];

    dev->write(dev->target, addr, value);
}