#ifndef MEM_H
#define MEM_H

#include <stdint.h>

struct WriteDevice {
	void (*write)(uint16_t addr, uint8_t value);
};

struct MemoryMap {
	uint8_t* read_pages[256];
	struct WriteDevice* write_pages[256];
};

uint8_t Read8(struct MemoryMap* memmap, uint16_t addr);
void Write8(struct MemoryMap* memmap, uint16_t addr, uint8_t value);

#endif
