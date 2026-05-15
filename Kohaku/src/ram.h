#ifndef RAM_H
#define RAM_H

#include "memory.h"

enum ram_result {
    RAM_OK  = 0,
    RAM_ERR = 1,
};

struct ram {
    uint8_t ram[8 * 1024];
    struct mem *mem;
};

enum ram_result ram_init(struct ram *ram, struct mem *mem);
enum ram_result ram_close(struct ram *ram);

#endif