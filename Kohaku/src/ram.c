#include "ram.h"

#include <stddef.h>
#include <assert.h>

enum ram_result ram_init(struct ram *ram, struct mem *mem) {
    assert(ram != NULL);
    assert(mem != NULL);

    for (uint8_t page = 0xc0; page <= 0xdf; page++) {
        uint8_t *offset_pointer = &ram->ram[(page - 0xc0) << 8];

        mem->read_dev[page]    = &MEM_UNMAPPED_READ_DEV;
        mem->read_direct[page] = offset_pointer;

        mem->write_dev[page]    = &MEM_UNMAPPED_WRITE_DEV;
        mem->write_direct[page] = offset_pointer;

        if (page <= 0xdd) {
            uint8_t echo_page = 0xe0 + (page - 0xc0);

            mem->read_dev[echo_page]    = &MEM_UNMAPPED_READ_DEV;
            mem->read_direct[echo_page] = offset_pointer;

            mem->write_dev[echo_page]    = &MEM_UNMAPPED_WRITE_DEV;
            mem->write_direct[echo_page] = offset_pointer;
        }
    }

    return RAM_OK;
}

enum ram_result ram_close(struct ram *ram) {
    return RAM_OK;
}