#ifndef MEM_H
#define MEM_H

#include <stdint.h>

struct write_device {
	void (*write)(void* target, uint16_t addr, uint8_t value);
	void* target;
};

struct memmap {
	uint8_t* read_pages[256];
	struct write_device* write_pages[256];
};

void memmap_init(struct memmap* memmap);
uint8_t read8(struct memmap *memmap, uint16_t addr);
void write8(struct memmap *memmap, uint16_t addr, uint8_t value);
void set8(struct memmap* memmap, uint16_t addr, uint8_t value);

#endif
