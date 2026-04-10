#include "tests.h"

#include <assert.h>

#include "log.h"
#include "file.h"
#include "cartridge.h"
#include "memory.h"

const uint8_t CART_DATA[] = {
    // bank 1 start
    [0x0000] = 0x11,
    [0x0001] = 0x11,
    [0x0002] = 0x11,
    [0x0003] = 0x11,
    [0x0004] = 0x11,
    [0x0005] = 0x11,

    // bank 1 end
    [0x3ffa] = 0x12,
    [0x3ffb] = 0x12,
    [0x3ffc] = 0x12,
    [0x3ffd] = 0x12,
    [0x3ffe] = 0x12,
    [0x3fff] = 0x12,

    // bank 2 start
    [0x4000] = 0x21,
    [0x4001] = 0x21,
    [0x4002] = 0x21,
    [0x4003] = 0x21,
    [0x4004] = 0x21,
    [0x4005] = 0x21,

    // bank 2 end
    [0x7ffa] = 0x22,
    [0x7ffb] = 0x22,
    [0x7ffc] = 0x22,
    [0x7ffd] = 0x22,
    [0x7ffe] = 0x22,
    [0x7fff] = 0x22,

    // bank 3 start
    [0x8000] = 0x31,
    [0x8001] = 0x31,
    [0x8002] = 0x31,
    [0x8003] = 0x31,
    [0x8004] = 0x31,
    [0x8005] = 0x31,

    // bank 3 end
    [0x9ffa] = 0x32,
    [0x9ffb] = 0x32,
    [0x9ffc] = 0x32,
    [0x9ffd] = 0x32,
    [0x9ffe] = 0x32,
    [0x9fff] = 0x32,
};

static struct file file;
static struct cartridge cart;
static struct memmap mem;

static void setup();
static void setup_cartridge();
static void setup_memory();

static void test_cartridge();

void tests_run() {
    DEBUG("starting tests...");
    setup();
    test_cartridge();
}

static void setup() {
    setup_memory();
    setup_cartridge();
}

static void setup_memory() {

}

static void setup_cartridge() {
    

    file.data   = CART_DATA;
    file.length = sizeof(CART_DATA);

    cart_init(&cart, file, &mem);
}

static void test_cartridge() {
    DEBUG("starting test_cartridge()...");

    assert(read8(&mem, 0x0000) == 0x11);
    assert(read8(&mem, 0x0001) == 0x11);
    assert(read8(&mem, 0x0002) == 0x11);
    assert(read8(&mem, 0x0003) == 0x11);
    assert(read8(&mem, 0x0004) == 0x11);
    assert(read8(&mem, 0x0005) == 0x11);

    assert(read8(&mem, 0x3ffa) == 0x12);
    assert(read8(&mem, 0x3ffb) == 0x12);
    assert(read8(&mem, 0x3ffc) == 0x12);
    assert(read8(&mem, 0x3ffd) == 0x12);
    assert(read8(&mem, 0x3ffe) == 0x12);
    assert(read8(&mem, 0x3fff) == 0x12);

    assert(read8(&mem, 0x4000) == 0x21);
    assert(read8(&mem, 0x4001) == 0x21);
    assert(read8(&mem, 0x4002) == 0x21);
    assert(read8(&mem, 0x4003) == 0x21);
    assert(read8(&mem, 0x4004) == 0x21);
    assert(read8(&mem, 0x4005) == 0x21);

    assert(read8(&mem, 0x7ffa) == 0x22);
    assert(read8(&mem, 0x7ffb) == 0x22);
    assert(read8(&mem, 0x7ffc) == 0x22);
    assert(read8(&mem, 0x7ffd) == 0x22);
    assert(read8(&mem, 0x7ffe) == 0x22);
    assert(read8(&mem, 0x7fff) == 0x22);

    write8(&mem, 0x0001, 0xff);

    DEBUG("cartridge tests passed.");
}