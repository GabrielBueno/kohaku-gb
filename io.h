#ifndef IO_H
#define IO_H

#include <stdint.h>

struct Buffer
{
	uint8_t* data;
	size_t size;
};

void ReadFile(const char* path, struct Buffer* buffer);

#endif
