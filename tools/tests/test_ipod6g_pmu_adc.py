#!/usr/bin/env python3
"""Fault-inject the production ADC function, with real register definitions."""
import re
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class PmuAdcTest(unittest.TestCase):
    def test_conversion_failures_and_repeated_recovery(self):
        source = (ROOT / "firmware/target/arm/s5l8702/ipod6g/pmu-6g.c").read_text()
        function = source[source.index("unsigned short pmu_read_adc("):]
        function = function[:function.index("\n/*\n * eINT")]
        timeout = re.search(r"#define PMU_ADC_TIMEOUT_US .*", source).group()
        header = (ROOT / "firmware/target/arm/s5l8702/ipod6g/pmu-target.h").read_text()
        channel = re.search(r"struct pmu_adc_channel\s*\{.*?\};", header, re.S).group()
        harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include "pcf5063x.h"
#define IPOD6G_HIBERNATE_STAGE3 1
#define IPOD6G_HIBERNATE_RUNTIME_DIAGNOSTICS 0
static uint32_t timer;
#define USEC_TIMER timer
struct mutex { int held; };
static struct mutex pmu_adc_mutex;
static int fail_reg = -1, polls, reads, cancels;
static bool stalled;
static uint8_t high = 123, low = 2;
static void mutex_lock(struct mutex *m) { assert(!m->held); m->held = 1; }
static void mutex_unlock(struct mutex *m) { assert(m->held); m->held = 0; }
static void sleep(int ticks) { timer += ticks * 10000u; assert(++polls < 100); }
static int pmu_write(int reg, unsigned char value)
{
    if (reg == PCF5063X_REG_ADCC1) {
        assert(!(value & PCF5063X_ADCC1_ADCSTART));
        ++cancels;
    }
    return reg == fail_reg ? 0x60 : 0;
}
static int pmu_write_multiple(int reg, int count, unsigned char *data)
{
    assert(count == 2 && (data[1] & PCF5063X_ADCC1_ADCSTART));
    return reg == fail_reg ? 0x60 : 0;
}
static int pmu_read_multiple(int reg, int count, unsigned char *data)
{
    assert(count == 1);
    if (reg == fail_reg) return 0x60; /* leave destination untouched */
    if (reg == PCF5063X_REG_ADCS3)
        *data = stalled ? 0 : PCF5063X_ADCS3_ADCRDY | low;
    else { assert(reg == PCF5063X_REG_ADCS1); ++reads; *data = high; }
    return 0;
}
'''
        cases = r'''
static void reset_io(void)
{
    assert(!pmu_adc_mutex.held);
    polls = reads = cancels = 0;
    fail_reg = -1;
    stalled = false;
}
int main(void)
{
    struct pmu_adc_channel ch = { .adcc1 = 0x30 };
    stalled = true;
    assert(pmu_read_adc(&ch) == 0); /* no last-good value */
    assert(!reads && cancels == 1 && polls == 25);
    reset_io();
    assert(pmu_read_adc(&ch) == 494);
    assert(reads == 1 && !cancels);
    int errors[] = { PCF5063X_REG_ADCC3, PCF5063X_REG_ADCC2,
        PCF5063X_REG_ADCS3, PCF5063X_REG_ADCS1 };
    for (unsigned int i = 0; i < sizeof(errors)/sizeof(errors[0]); ++i) {
        reset_io(); fail_reg = errors[i]; high = 255;
        assert(pmu_read_adc(&ch) == 494);
        assert(!reads && cancels == 1);
    }
    for (int i = 0; i < 5; ++i) {
        reset_io(); stalled = true; timer = UINT32_MAX - 50000u;
        assert(pmu_read_adc(&ch) == 494); /* wrap-safe timeout */
        assert(polls == 25 && !reads && cancels == 1);
        reset_io(); high = 123;
        assert(pmu_read_adc(&ch) == 494);
        assert(reads == 1 && !cancels);
    }
    reset_io(); ch.adcc1 = 0x22; high = 77;
    assert(pmu_read_adc(&ch) == 77); /* 8-bit and independent channel */
    reset_io(); stalled = true;
    assert(pmu_read_adc(&ch) == 77);
    assert(!pmu_adc_mutex.held);
    return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix="pmu-adc-") as directory:
            src = Path(directory) / "test.c"
            binary = Path(directory) / "test"
            src.write_text(harness + channel + "\n" + timeout + "\n" + function + cases)
            subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=undefined,address", "-I", str(ROOT / "firmware/export"),
                            str(src), "-o", str(binary)], check=True)
            # The harness has no heap allocations. LeakSanitizer cannot run
            # under the sandbox's process tracer; keep address/UB checks on.
            environment = dict(os.environ)
            environment["ASAN_OPTIONS"] = "detect_leaks=0"
            subprocess.run([str(binary)], check=True, timeout=5, env=environment)


if __name__ == "__main__":
    unittest.main()
