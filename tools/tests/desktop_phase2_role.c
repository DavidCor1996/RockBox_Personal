/* Fault-injected tests for the first physical USB gate. GPL-2.0-or-later. */
#include "role_probe.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

enum fault { NONE, AHB_BUSY, RESET_STUCK, HOST_STUCK, DEVICE_STUCK, ROLE_LOST };
struct controller {
    uint32_t time, started, usb, mode, reset_at;
    unsigned writes, begins, ends;
    enum fault fault;
    int cancel_after;
    bool reset;
};
static void begin(void *ctx)
{
    struct controller *c = ctx;
    c->started = c->time;
    c->begins++;
    c->usb = (1u << 30) | 0x1408;
}
static uint32_t read_reg(void *ctx, enum usb_probe_register reg)
{
    struct controller *c = ctx;
    uint32_t elapsed = c->time - c->started;
    switch (reg) {
    case USB_PROBE_USB: return c->usb;
    case USB_PROBE_RESET:
        if (c->fault == AHB_BUSY) return 0;
        if (c->reset && (c->fault == RESET_STUCK ||
                         c->time - c->reset_at < 30)) return 1;
        return 1u << 31;
    case USB_PROBE_STATUS:
        return c->fault == ROLE_LOST && elapsed > 500 ? 0 : c->mode;
    case USB_PROBE_PORT: return elapsed > 250 ? 1 : 0;
    case USB_PROBE_ID: return 0x4f54280a;
    case USB_PROBE_HW: return 0x228f5910;
    default: return 0;
    }
}
static void write_reg(void *ctx, enum usb_probe_register reg, uint32_t value)
{
    struct controller *c = ctx;
    c->writes++;
    /* Reject any peripheral, DMA or power command even on fault paths. */
    assert(reg == USB_PROBE_USB || reg == USB_PROBE_RESET ||
           reg == USB_PROBE_AHB || reg == USB_PROBE_IRQ_MASK);
    if (reg == USB_PROBE_RESET) {
        assert(value == 1);
        c->reset = true;
        c->reset_at = c->time;
    } else if (reg == USB_PROBE_USB) {
        assert((value & ((1u << 29) | (1u << 30))) !=
               ((1u << 29) | (1u << 30)));
        assert((value & 0x1fffffff) == 0x1408);
        c->usb = value;
        if (value & (1u << 29)) {
            assert(c->time - c->reset_at >= 31); /* reset plus PHY settling */
            if (c->fault != HOST_STUCK) c->mode = 1;
        } else if (c->fault != DEVICE_STUCK) c->mode = 0;
    } else assert(value == 0);
}
static uint32_t now(void *ctx) { return ((struct controller *)ctx)->time; }
static void sleep_ms(void *ctx, unsigned ms)
{
    assert(ms > 0 && ms <= 50);
    ((struct controller *)ctx)->time += ms;
}
static bool cancelled(void *ctx)
{
    struct controller *c = ctx;
    return c->cancel_after >= 0 &&
           c->time - c->started >= (uint32_t)c->cancel_after;
}
static void end(void *ctx) { ((struct controller *)ctx)->ends++; }
static struct usb_probe_report run(enum fault fault, int cancel, uint32_t start)
{
    struct controller c = { .fault = fault, .cancel_after = cancel, .time = start };
    struct usb_probe_report r;
    const struct usb_probe_operations o = { &c, begin, read_reg, write_reg,
        now, sleep_ms, cancelled, end };
    usb_role_probe_run(&o, &r);
    assert(c.begins == 1 && c.ends == 1);
    assert(r.elapsed_ms <= 12100);
    assert(c.usb & (1u << 30));
    assert(!(c.usb & (1u << 29)));
    assert(r.core_id == 0x4f54280a);
    assert(r.restored == (fault != DEVICE_STUCK));
    return r;
}
int main(void)
{
    struct usb_probe_report r = run(NONE, -1, 0);
    assert(r.result == USB_PROBE_OK && r.host_seen && r.connected_seen);
    assert(r.elapsed_ms >= 10000);
    assert(run(AHB_BUSY, -1, 0).result == USB_PROBE_AHB_TIMEOUT);
    assert(run(RESET_STUCK, -1, 0).result == USB_PROBE_RESET_TIMEOUT);
    assert(run(HOST_STUCK, -1, 0).result == USB_PROBE_HOST_TIMEOUT);
    r = run(DEVICE_STUCK, -1, 0);
    assert(r.result == USB_PROBE_RESTORE_FAILED && r.operation_result == USB_PROBE_OK);
    assert(run(ROLE_LOST, -1, 0).result == USB_PROBE_HOST_TIMEOUT);
    for (int cancel = 0; cancel <= 10000; cancel += 10)
        assert(run(NONE, cancel, 0).result == USB_PROBE_CANCELLED);
    assert(run(DEVICE_STUCK, 200, 0).result == USB_PROBE_RESTORE_FAILED);
    assert(run(NONE, -1, UINT32_MAX - 100).result == USB_PROBE_OK);
    puts("PASS: role probe success, 1001 cancellation points, failures, restore, wraparound and write boundaries");
}
