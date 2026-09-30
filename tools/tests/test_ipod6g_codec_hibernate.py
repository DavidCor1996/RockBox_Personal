#!/usr/bin/env python3
"""Exercise the actual CS42L55 driver against a rail-loss/I2C fault model."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class CodecHibernateTest(unittest.TestCase):
    def test_rail_loss_settings_timing_and_each_io_failure(self):
        driver = (ROOT / 'firmware/drivers/audio/cs42l55.c').read_text()
        driver = re.sub(r'^#include .*$', '', driver, flags=re.M)
        harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#define IPOD_6G 1
#define IPOD6G_HIBERNATE_STAGE3 1
#define AUDIOHW_SETTING(...)
#define ARRAYLEN(a) (sizeof(a) / sizeof((a)[0]))
#define HZ 100
#include "cs42l55.h"
enum { HW_FREQ_8, HW_FREQ_11, HW_FREQ_12, HW_FREQ_16, HW_FREQ_22,
       HW_FREQ_24, HW_FREQ_32, HW_FREQ_44, HW_FREQ_48 };
static unsigned char bank[64];
static bool reset_low, checking;
static unsigned int now, reset_at, release_at, powered_at;
static int reads, writes, fail_read, fail_write, drop_reg = -1;
static void defaults(void)
{
    memset(bank, 0, sizeof(bank));
    bank[PWRCTL1] = 0x0f; bank[PWRCTL2] = 0xff;
    bank[CLKCTL2] = 0x0b;
}
static void sleep(int ticks) { assert(ticks > 0); now += ticks * 10000u; }
static void udelay(unsigned int us) { now += us; }
static void cscodec_power(bool state) { (void)state; }
static void cscodec_clock(bool state) { (void)state; }
static void cscodec_reset(bool state)
{
    if (state) { reset_at = now; defaults(); }
    else {
        if (checking) assert(now - reset_at >= 1000);
        release_at = now;
    }
    reset_low = state;
}
static unsigned char cscodec_read(int reg) { return bank[reg]; }
static void cscodec_write(int reg, unsigned char value) { bank[reg] = value; }
static bool cscodec_read_checked(int reg, unsigned char *value)
{
    assert(!reset_low && reg != STATUS && reg >= PWRCTL1 && reg <= CPCTL);
    if (++reads == fail_read) return false;
    *value = bank[reg]; return true;
}
static bool cscodec_write_checked(int reg, unsigned char value)
{
    assert(!reset_low && reg != STATUS && reg != CHIPVERSION);
    assert(now - release_at >= 1);
    if (++writes == fail_write) return false;
    if (reg == drop_reg) return true; /* ACK with no register change */
    if (checking && reg == PWRCTL1 && !(value & PWRCTL1_PDN_CODEC) &&
            (bank[PWRCTL1] & PWRCTL1_PDN_CODEC)) {
        assert(bank[CLKCTL1] & CLKCTL1_MASTER);
        assert((bank[PLAYCTL] & 3) == 3);
        for (int r = HPACTL; r <= LINEBCTL; r++) assert(bank[r] == 0xc4);
        powered_at = now;
    }
    if (checking && reg >= HPACTL && reg <= LINEBCTL && !(value & 0x80))
        assert(now - powered_at >= 75000);
    if (checking && reg == PLAYCTL && (value & 3) != 3)
        assert(now - powered_at >= 75000);
    bank[reg] = value;
    return true;
}
'''
        cases = r'''
static void clear_faults(void)
{
    reads = writes = fail_read = fail_write = 0; drop_reg = -1;
}
static void setup(int freq, bool idle)
{
    checking = false; clear_faults();
    audiohw_preinit(); audiohw_postinit();
    audiohw_set_volume(-250, -370);
    audiohw_set_lineout_volume(-110, -170);
    audiohw_enable_lineout(true);
    audiohw_set_bass(45); audiohw_set_treble(-30);
    audiohw_set_bass_cutoff(3); audiohw_set_treble_cutoff(2);
    audiohw_set_prescaler(0);
    cscodec_write(MSTAVOL, 0xf3); cscodec_write(MSTBVOL, 0xf3);
    audiohw_set_frequency(freq);
    if (idle) audiohw_idle_powerdown();
    clear_faults();
}
static void equal_bank(const unsigned char *saved)
{
    for (int r = PWRCTL1; r <= CPCTL; r++)
        if (r != STATUS) assert(bank[r] == saved[r]);
    assert(bass == 45 && treble == -30 && active_dsp_modules == DSP_MODULE_TONE);
}
static void lose_rail(void)
{
    /* This removes MASTER, output enables and every saved volume/tone byte. */
    defaults(); checking = true; clear_faults();
    assert(!(bank[CLKCTL1] & CLKCTL1_MASTER));
}
int main(void)
{
    unsigned char saved[64]; int restore_writes = 0, restore_reads = 0;
    /* Active and idle codec policies, multiple sample rates and repeated loss. */
    for (int cycle = 0; cycle < 12; cycle++) {
        setup(cycle % 2 ? HW_FREQ_44 : HW_FREQ_48, cycle % 3 == 0);
        memcpy(saved, bank, sizeof(saved));
        assert(audiohw_hibernate_save());
        equal_bank(saved); assert(!writes); /* save itself is read-only */
        audiohw_idle_powerdown(); lose_rail();
        assert(audiohw_hibernate_restore()); equal_bank(saved);
        restore_writes = writes; restore_reads = reads;
        assert(!reset_low);
        audiohw_idle_powerup(); /* subsequent Play from paused/stopped */
        assert(bank[CLKCTL1] & CLKCTL1_MASTER);
        assert(!(bank[PWRCTL1] & PWRCTL1_PDN_CODEC));
        assert(!(bank[PLAYCTL] & 3));
        assert(bank[HPACTL] == saved[HPACTL]);
        audiohw_set_treble(0);
        assert(bass == 45 && active_dsp_modules == DSP_MODULE_TONE);
    }
    /* Failed snapshot refuses entry without muting/resetting the live codec. */
    for (int nth = 1; nth <= CPCTL - PWRCTL1; nth++) {
        setup(HW_FREQ_48, false); memcpy(saved, bank, sizeof(saved));
        fail_read = nth;
        assert(!audiohw_hibernate_save()); equal_bank(saved);
        assert(!reset_low && !writes);
        checking = true;
        assert(!audiohw_hibernate_restore()); assert(reset_low && !writes);
    }
    /* Every restore write error must hold hardware reset, including the final
       power-policy/unmute writes. No false success or partial audible state. */
    for (int nth = 1; nth <= restore_writes; nth++) {
        setup(HW_FREQ_48, false); assert(audiohw_hibernate_save());
        lose_rail(); fail_write = nth;
        assert(!audiohw_hibernate_restore()); assert(reset_low);
    }
    for (int nth = 1; nth <= restore_reads; nth++) {
        setup(HW_FREQ_44, false); assert(audiohw_hibernate_save());
        lose_rail(); fail_read = nth;
        assert(!audiohw_hibernate_restore()); assert(reset_low);
    }
    setup(HW_FREQ_48, false); assert(audiohw_hibernate_save());
    lose_rail(); drop_reg = CLKCTL1;
    /* The model's clock assertion cannot hold if MASTER was dropped; detect
       the missing write through the driver's readback instead. */
    checking = false;
    assert(!audiohw_hibernate_restore()); assert(reset_low);
    setup(HW_FREQ_44, false); assert(audiohw_hibernate_save());
    lose_rail(); assert(audiohw_hibernate_restore());
    clear_faults(); assert(!audiohw_hibernate_restore());
    assert(reset_low && !writes); /* saved image cannot be reused */
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / 'codec.c'
            binary = Path(tmp) / 'codec-test'
            src.write_text(harness + driver + cases)
            subprocess.run(['cc', '-std=gnu99', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                            '-I', str(ROOT / 'firmware/export'), str(src), '-o', str(binary)], check=True)
            env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0',
                       UBSAN_OPTIONS='halt_on_error=1')
            subprocess.run([str(binary)], check=True, env=env)

    def test_dma_stays_blocked_on_restore_failure(self):
        source = (ROOT / 'firmware/target/arm/s5l8702/pcm-s5l8702.c').read_text()
        def function(name):
            start = re.search(r'^(?:void|bool) ' + name + r'\(', source, re.M).start()
            end = source.index('\n}', start) + 2
            return source[start:end]
        harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#define IPOD_6G 1
#define HAVE_CS42L55 1
#define IPOD6G_HIBERNATE_STAGE3 1
#define I2STXCOM_START 0xe
static bool pcm_dma_start_inhibit, pcm_hibernate_codec_blocked;
static bool restore_ok;
static int queued, restored, stopped;
static size_t pcm_remaining;
static unsigned int power[2], I2SCLKCON, I2STXCOM;
#define PWRCON(n) power[n]
static bool audiohw_hibernate_restore(void) { ++restored; return restore_ok; }
static void pcm_play_dma_stop(void) { ++stopped; }
static void pcm_dma_apply_settings(void) {}
static void audiohw_idle_powerup(void) {}
static void dma_play_callback(void *p) { (void)p; ++queued; }
'''
        cases = r'''
int main(void)
{
    int samples[8] = {0};
    pcm_hibernate_codec_blocked = true;
    pcm_play_dma_start(samples, sizeof(samples));
    assert(!queued && !stopped);
    restore_ok = false;
    assert(!pcm_hibernate_resume_complete());
    assert(pcm_hibernate_codec_blocked && restored == 1 && !queued);
    pcm_dma_start_inhibit = false; /* USB source cleanup cannot clear this block */
    pcm_play_dma_start(samples, sizeof(samples));
    assert(!queued && !stopped);
    restore_ok = true;
    assert(pcm_hibernate_resume_complete());
    assert(!pcm_hibernate_codec_blocked && !queued); /* no transport Play */
    pcm_dma_start_inhibit = true;
    pcm_play_dma_start(samples, sizeof(samples));
    assert(!queued); /* codec completion must not override USB ownership */
    pcm_dma_start_inhibit = false;
    pcm_play_dma_start(samples, sizeof(samples));
    assert(queued == 1 && stopped == 1 && pcm_remaining == sizeof(samples));
    assert(I2SCLKCON == 1 && I2STXCOM == I2STXCOM_START);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / 'pcm.c'
            binary = Path(tmp) / 'pcm-test'
            src.write_text(harness + function('pcm_hibernate_resume_complete') +
                           '\n' + function('pcm_play_dma_start') + cases)
            subprocess.run(['cc', '-std=gnu99', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address,undefined', str(src), '-o', str(binary)],
                           check=True)
            subprocess.run([str(binary)], check=True,
                           env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0',
                                    UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    unittest.main()
