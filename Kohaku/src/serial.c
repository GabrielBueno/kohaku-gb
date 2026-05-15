#include "serial.h"

#include <stdio.h>

void serial_init(struct serial *serial) {

}

void serial_reg_trans_data_write(struct serial *serial, uint8_t value) {
    serial->trans_data = value;
}

void serial_reg_trans_ctrl_write(struct serial *serial, uint8_t value) {
    serial->trans_ctrl = value;
    fprintf(stdout, "%c", serial->trans_data);
}