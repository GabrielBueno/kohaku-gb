#ifndef MEM_H
#define MEM_H

#include <stdint.h>

struct mem_write_dev {
	void (*write)(void *target, uint16_t addr, uint8_t value);
	void *target;
};

struct mem_read_dev {
	uint8_t (*read)(void *target, uint16_t addr);
	void* target;
};

struct mem {
	struct mem_read_dev *read_dev[256];
	struct mem_write_dev *write_dev[256];
	uint8_t *read_direct[256];
	uint8_t *write_direct[256];
};

extern struct mem_read_dev  MEM_UNMAPPED_READ_DEV;
extern struct mem_write_dev MEM_UNMAPPED_WRITE_DEV;

void mem_init(struct mem *mem);
uint8_t mem_read8(struct mem *mem, uint16_t addr);
uint16_t mem_read16(struct mem *mem, uint16_t addr);
void mem_write8(struct mem *mem, uint16_t addr, uint8_t value);

#endif
