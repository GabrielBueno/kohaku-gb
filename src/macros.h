#ifndef MACROS_H
#define MACROS_H

#define BYTE_16BIT(MSB, LSB) (((uint16_t)(MSB) << 8) | (uint16_t)(LSB & 0xff))
#define BYTE_MSB(U16) ((uint8_t)((U16) >> 8))
#define BYTE_LSB(U16) ((uint8_t)((U16)))

#endif