#include "memory.h"

uint8_t Read8(struct MemoryMap* memmap, uint16_t addr)
{
	memmap->read_pages[addr >> 8][addr];
}

void Write8(struct MemoryMap* memmap, uint16_t addr, uint8_t value)
{
	memmap->write_pages[addr >> 8][addr].write(addr, value);
}