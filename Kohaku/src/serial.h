#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

struct serial {
    uint8_t trans_ctrl;
    uint8_t trans_data;
};

void serial_init(struct serial *serial);
void serial_reg_trans_data_write(struct serial *serial, uint8_t value);
void serial_reg_trans_ctrl_write(struct serial *serial, uint8_t value);

#endif