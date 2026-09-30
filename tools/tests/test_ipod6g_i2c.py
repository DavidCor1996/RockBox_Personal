#!/usr/bin/env python3
"""Exercise the actual polled I2C driver with stalled register models."""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class I2cTest(unittest.TestCase):
    def test_progress_timeout_and_error_cleanup(self):
        source = (ROOT / "firmware/target/arm/s5l8702/i2c-s5l8702.c").read_text()
        source = source[source.index("static struct mutex i2c_mtx[2];"):]
        source = source[:source.index("void i2c_preinit(")]
        harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define S5L8702 8702
#define CONFIG_CPU S5L8702
struct mutex { int held; };
static void mutex_init(struct mutex *m) { m->held = 0; }
static void mutex_lock(struct mutex *m) { assert(!m->held); m->held = 1; }
static void mutex_unlock(struct mutex *m) { assert(m->held); m->held = 0; }
static uint32_t timer;
static uint32_t get_timer(void) { timer += 1000; return timer; }
#define USEC_TIMER get_timer()
static uint32_t registers[2][5];
static bool nack;
static uint32_t *status(int bus) {
    if (nack) registers[bus][1] |= 1;
    return &registers[bus][1];
}
#define IICUNK10(bus) registers[bus][0]
#define IICSTAT(bus) (*status(bus))
#define IICCON(bus) registers[bus][2]
#define IICSTA2(bus) registers[bus][3]
#define IICDS(bus) registers[bus][4]
#define I2CCLKGATE(bus) bus
static int on_count, off_count;
static void clockgate_enable(int bus, bool on) {
    (void)bus;
    if (on) ++on_count; else ++off_count;
}
static void udelay(int us) { timer += us; }
void i2c_bus_lock(int bus);
void i2c_bus_unlock(int bus);
'''
        cases = r'''
static void reset_io(int bus) {
    assert(!i2c_mtx[bus].held);
    for (int i = 0; i < 5; ++i) registers[bus][i] = 0;
    IICSTA2(bus) = 1 << 8;
    nack = false;
    on_count = off_count = 0;
}
int main(void) {
    unsigned char data[64] = {0};
    i2c_init();
    for (int bus = 0; bus < 2; ++bus) {
        for (int cycle = 0; cycle < 5; ++cycle) {
            reset_io(bus);
            timer = UINT32_MAX - 30000u;
            IICUNK10(bus) = 1;
            assert(i2c_read(bus, 0xe6, 0x57, 1, data) == 0x60);
            assert(!i2c_mtx[bus].held && on_count == 1 && off_count == 1);
            assert(registers[bus][1] == 0 && IICCON(bus) == 0);
            reset_io(bus);
            IICSTA2(bus) = 0; /* START progress never arrives */
            assert(i2c_write(bus, 0xe6, 0x54, 1, data) == 0x60);
            assert(!i2c_mtx[bus].held && on_count == 1 && off_count == 1);
            reset_io(bus); nack = true;
            assert(i2c_write(bus, 0xe6, 0x54, 1, data) == 0x60);
            assert(!i2c_mtx[bus].held && on_count == 1 && off_count == 1);
            reset_io(bus);
            uint32_t start = timer;
            assert(i2c_write(bus, 0xe6, 0x54, 64, data) == 0);
            assert(timer - start > 100000u); /* per-progress, not total budget */
            assert(!i2c_mtx[bus].held && on_count == 1 && off_count == 1);
            reset_io(bus);
            assert(i2c_read(bus, 0xe6, 0x57, 64, data) == 0);
            assert(!i2c_mtx[bus].held && on_count == 1 && off_count == 1);
        }
    }
    return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix="ipod-i2c-") as directory:
            src, binary = Path(directory) / "test.c", Path(directory) / "test"
            src.write_text(harness + source + cases)
            subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=undefined,bounds", str(src), "-o", str(binary)],
                           check=True)
            subprocess.run([str(binary)], check=True, timeout=5)


if __name__ == "__main__":
    unittest.main()
