#!/usr/bin/env python3
"""Exercise the production bounded remote trace, including non-button iAP."""
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[2]
source = (root / 'apps/iap/iap-remote-debug.c').read_text()
source = source[source.index('#define TRACE_COUNT'):source.index('static void export_trace')]
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MIN(a,b) ((a)<(b)?(a):(b))
#define BUTTON_REMOTE 0x10000
static long current_tick;
static struct { int volume; } global_status;
static struct { struct { bool idps; } auth; } device;
static bool nav,tv,kokkia,quiet;
static bool iap_remote_navigation_active(void) { return nav; }
static bool iap_remote_tv_active(void) { return tv; }
static bool iap_kokkia_present(void) { return kokkia; }
static bool iap_remote_input_suppressed(void) { return quiet; }
static int disable_irq_save(void) { return 0; }
static void restore_irq(int state) { (void)state; }
''' + source + r'''
int main(void)
{
    unsigned char volume[] = {3, 0x0e, 4, 0, 128};
    global_status.volume = -24;
    iap_remote_command(volume, sizeof(volume));
    assert(count == 0);
    enabled = true; nav = tv = true;
    iap_remote_command(volume, sizeof(volume));
    assert(copy_trace() == 1);
    assert(snapshot[0].state == 'C' && snapshot[0].bitmap == 4);
    assert(snapshot[0].volume == -24 && snapshot[0].policy == 3);
    assert(!memcmp(snapshot[0].raw, volume, sizeof(volume)));
    unsigned char extended[] = {4, 0, 0x29, 1};
    iap_remote_command(extended, sizeof(extended));
    assert(copy_trace() == 2 && snapshot[1].state == 'C');
    unsigned char status[] = {3, 0x0c, 4};
    iap_remote_command(status, sizeof(status));
    assert(count == 3); /* Unknown non-button commands remain observable. */
    for (int idps = 0; idps < 2; idps++)
    {
        device.auth.idps = idps;
        for (unsigned length = 0; length < 32; length++)
        {
            unsigned char *packet = calloc(length ? length : 1, 1);
            if (length) packet[0] = 3;
            if (length > 1) packet[1] = 0x0e;
            iap_remote_command(packet, length);
            free(packet);
        }
    }
    unsigned char transaction[] = {3, 0x0e, 0x12, 0x34, 0x10, 0, 127, 127, 0};
    kokkia = quiet = true; nav = tv = false;
    for (unsigned i = 0; i < 1000; i++)
    {
        current_tick = i;
        iap_remote_command(transaction, sizeof(transaction));
    }
    assert(copy_trace() == TRACE_COUNT);
    assert(snapshot[TRACE_COUNT-1].tick == 999);
    assert(snapshot[TRACE_COUNT-1].bitmap == 0x10);
    assert(snapshot[TRACE_COUNT-1].policy == 12);
    assert(!memcmp(snapshot[TRACE_COUNT-1].raw, transaction, sizeof(transaction)));
    iap_remote_action(BUTTON_REMOTE, 7, 8);
    copy_trace();
    assert(snapshot[TRACE_COUNT-1].state == 'A');
    assert(snapshot[TRACE_COUNT-1].context == 7 && snapshot[TRACE_COUNT-1].action == 8);
    puts("PASS: production remote trace; direct state/volume and extended controls, legacy/transaction bounds, policy snapshots, disabled capture and 1000-event ring wrap");
}
'''
with tempfile.TemporaryDirectory(prefix='remote-command-trace-') as temporary:
    path = pathlib.Path(temporary)
    (path / 'test.c').write_text(code)
    subprocess.run(['cc', '-std=c99', '-fsanitize=address,undefined',
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
