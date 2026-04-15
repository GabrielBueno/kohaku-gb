#include "gb.h"

#include <assert.h>
#include "log.h"

static enum gb_result init_mem(struct gb *gb, struct gb_options *options);
static enum gb_result init_cart(struct gb *gb, struct gb_options *options);
static enum gb_result init_joypad(struct gb *gb, struct gb_options *options);
static enum gb_result map_ioregs(struct gb *gb, struct gb_options *options);

static void    gb_ioreg_write(void *target, uint16_t addr, uint8_t value);
static uint8_t gb_ioreg_read(void *target, uint16_t addr);

enum gb_result gb_init(struct gb *gb, struct gb_options *options) {
    assert(gb != NULL);
    assert(options != NULL);

    enum gb_result result;

    if ((result = init_mem(gb, options)) != GB_OK)
        return result;

    if ((result = init_cart(gb, options)) != GB_OK)
        return result;

    if ((result = init_joypad(gb, options)) != GB_OK)
        return result;

    if ((result = map_ioregs(gb, options)) != GB_OK)
        return result;

    return GB_OK;
}

enum gb_result gb_close(struct gb *gb) {
    return GB_OK;
}

static enum gb_result init_mem(struct gb *gb, struct gb_options *options) {
    mem_init(&gb->mem);

    return GB_OK;
}

static enum gb_result init_cart(struct gb *gb, struct gb_options *options) {
    if (cart_init(&gb->cart, options->rom_file, &gb->mem) != CART_OK)
        return GB_ERR;

    cart_info(&gb->cart, &gb->cart_info);

    return GB_OK;
}

static enum gb_result init_joypad(struct gb *gb, struct gb_options *options) {
    joypad_init(&gb->joypad);

    return GB_OK;
}

static enum gb_result map_ioregs(struct gb *gb, struct gb_options *options) {
    gb->ioregs.read_dev.target = gb;
    gb->ioregs.read_dev.read   = gb_ioreg_read;

    gb->mem.read_dev[0xff]    = &gb->ioregs.read_dev;
    gb->mem.read_direct[0xff] = NULL;

    gb->ioregs.write_dev.target = gb;
    gb->ioregs.write_dev.write  = gb_ioreg_write;

    gb->mem.write_dev[0xff]    = &gb->ioregs.write_dev;
    gb->mem.write_direct[0xff] = NULL;

    return GB_OK;
}

static void gb_ioreg_write(void *target, uint16_t addr, uint8_t value) {
    struct gb *gb = (struct gb*)target;

    if (addr == REG_JOYP_ADDR)
        return joypad_ioreg_joyp_write(&gb->joypad, value);

    WARN("writing to ioreg (value=#%02x, addr=#%04x) has no implemented behaviour.", value, addr);
}

static uint8_t gb_ioreg_read(void *target, uint16_t addr) {
    struct gb *gb = (struct gb*)target;

    if (addr == REG_JOYP_ADDR)
        return joypad_ioreg_joyp_read(&gb->joypad);

    WARN("reading from ioreg (addr=#%04x) has no implemented behaviour.", addr);

    return 0xff;
}