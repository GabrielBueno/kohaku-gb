#include "gb.h"

#include <assert.h>
#include <stdio.h>
#include "log.h"

static enum gb_result init_mem(struct gb *gb, struct gb_options *options);
static enum gb_result init_cpu(struct gb *gb, struct gb_options *options);
static enum gb_result init_ppu(struct gb *gb, struct gb_options *options);
static enum gb_result init_ram(struct gb *gb, struct gb_options *options);
static enum gb_result init_cart(struct gb *gb, struct gb_options *options);
static enum gb_result init_joypad(struct gb *gb, struct gb_options *options);
static enum gb_result init_interrupt(struct gb *gb, struct gb_options *options);
static enum gb_result init_timer(struct gb *gb, struct gb_options *options);
static enum gb_result init_serial(struct gb *gb, struct gb_options *options);
static enum gb_result map_ioregs(struct gb *gb, struct gb_options *options);

static void    gb_ioreg_write(void *target, uint16_t addr, uint8_t value);
static uint8_t gb_ioreg_read(void *target, uint16_t addr);

enum gb_result gb_init(struct gb *gb, struct gb_options *options) {
    assert(gb != NULL);
    assert(options != NULL);

    enum gb_result result;

    if ((result = init_interrupt(gb, options)) != GB_OK)
        return result;

    if ((result = init_mem(gb, options)) != GB_OK)
        return result;

    if ((result = init_cart(gb, options)) != GB_OK)
        return result;

    if ((result = init_joypad(gb, options)) != GB_OK)
        return result;

    if ((result = init_cpu(gb, options)) != GB_OK)
        return result;

    if ((result = init_ppu(gb, options)) != GB_OK)
        return result;

    if ((result = init_ram(gb, options)) != GB_OK)
        return result;

    if ((result = init_timer(gb, options)) != GB_OK)
        return result;

    if ((result = init_serial(gb, options)) != GB_OK)
        return result;

    if ((result = map_ioregs(gb, options)) != GB_OK)
        return result;

    return GB_OK;
}

int gb_tick(struct gb *gb) {
    int cycles = cpu_tick(&gb->cpu);

    timer_tick(&gb->timer, cycles);

    return cycles;
}

enum gb_result gb_stop(struct gb *gb) {
    gb->running = 0;
    return GB_OK;
}

enum gb_result gb_close(struct gb *gb) {
    cpu_close(&gb->cpu);

    return GB_OK;
}

static enum gb_result init_cpu(struct gb *gb, struct gb_options *options) {
    if (cpu_init(&gb->cpu, &gb->mem, &gb->interrupt) != CPU_OK)
        return GB_ERR;

    return GB_OK;
}

static enum gb_result init_ppu(struct gb *gb, struct gb_options *options) {
    if (ppu_init(&gb->ppu, &gb->mem) != PPU_OK)
        return GB_ERR;

    return GB_OK;
}

static enum gb_result init_ram(struct gb *gb, struct gb_options *options) {
    if (ram_init(&gb->ram, &gb->mem) != RAM_OK)
        return GB_ERR;

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

static enum gb_result init_interrupt(struct gb *gb, struct gb_options *options) {
    interrupt_init(&gb->interrupt);

    return GB_OK;
}

static enum gb_result init_timer(struct gb *gb, struct gb_options *options) {
    timer_init(&gb->timer, &gb->interrupt);

    return GB_OK;
}

static enum gb_result init_serial(struct gb *gb, struct gb_options *options) {
    serial_init(&gb->serial);

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

    if (addr >= REG_ADDR_HRAM_START && addr <= REG_ADDR_HRAM_END) {
        gb->ioregs.high_ram[addr - REG_ADDR_HRAM_START] = value;
        return;
    }

    if (addr == REG_ADDR_TIMER_DIV)
        return timer_reg_div_write(&gb->timer, value);

    if (addr == REG_ADDR_TIMER_TIMA)
        return timer_reg_tima_write(&gb->timer, value);

    if (addr == REG_ADDR_TIMER_TMA)
        return timer_reg_tma_write(&gb->timer, value);

    if (addr == REG_ADDR_TIMER_TAC)
        return timer_reg_tac_write(&gb->timer, value);

    if (addr == REG_ADDR_INTERRUPT_ENABLE)
        return interrupt_reg_ie_write(&gb->interrupt, value);

    if (addr == REG_ADDR_INTERRUPT_FLAG)
        return interrupt_reg_if_write(&gb->interrupt, value);

    if (addr == REG_ADDR_JOYP)
        return joypad_ioreg_joyp_write(&gb->joypad, value);

    if (addr == REG_ADDR_SERIAL_TRANSFER_CONTROL)
        return serial_reg_trans_ctrl_write(&gb->serial, value);

    if (addr == REG_ADDR_SERIAL_TRANSFER_DATA)
        return serial_reg_trans_data_write(&gb->serial, value);

    WARN("writing to ioreg (value=#%02x, addr=#%04x) has no implemented behaviour.", value, addr);
}

static uint8_t gb_ioreg_read(void *target, uint16_t addr) {
    struct gb *gb = (struct gb*)target;

    if (addr >= REG_ADDR_HRAM_START && addr <= REG_ADDR_HRAM_END)
        return gb->ioregs.high_ram[addr - REG_ADDR_HRAM_START];

    if (addr == REG_ADDR_TIMER_DIV)
        return timer_reg_div_read(&gb->timer);

    if (addr == REG_ADDR_TIMER_TIMA)
        return timer_reg_tima_read(&gb->timer);

    if (addr == REG_ADDR_TIMER_TMA)
        return timer_reg_tma_read(&gb->timer);

    if (addr == REG_ADDR_TIMER_TAC)
        return timer_reg_tac_read(&gb->timer);

    if (addr == REG_ADDR_INTERRUPT_ENABLE)
        return gb->interrupt.enabled;

    if (addr == REG_ADDR_INTERRUPT_FLAG)
        return gb->interrupt.requested;

    if (addr == REG_ADDR_JOYP)
        return joypad_ioreg_joyp_read(&gb->joypad);

    if (addr == REG_ADDR_SERIAL_TRANSFER_CONTROL)
        return gb->serial.trans_ctrl;

    if (addr == REG_ADDR_SERIAL_TRANSFER_DATA)
        return gb->serial.trans_data;

    WARN("reading from ioreg (addr=#%04x) has no implemented behaviour.", addr);

    return 0xff;
}