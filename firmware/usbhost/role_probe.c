/* Bounded USB role diagnostic. GPL-2.0-or-later. */
#include "role_probe.h"
#include <string.h>

#define FORCE_HOST (1u << 29)
#define FORCE_DEVICE (1u << 30)
#define CORE_HOST 1u
#define AHB_IDLE (1u << 31)
#define CORE_RESET 1u
#define PORT_CONNECTED 1u

static bool wait_bits(const struct usb_probe_operations *o,
                       enum usb_probe_register reg, uint32_t mask,
                       uint32_t expected, bool allow_cancel)
{
    uint32_t start = o->now(o->context);
    do
    {
        if ((o->read(o->context, reg) & mask) == expected) return true;
        if (allow_cancel && o->cancelled(o->context)) return false;
        o->sleep_ms(o->context, 10);
    } while ((uint32_t)(o->now(o->context) - start) < 1000);
    return false;
}

void usb_role_probe_run(const struct usb_probe_operations *o,
                        struct usb_probe_report *r)
{
    uint32_t start = o->now(o->context), observing;
    memset(r, 0, sizeof(*r));
    r->result = USB_PROBE_RUNNING;
    o->begin(o->context);
    r->core_id = o->read(o->context, USB_PROBE_ID);
    r->hardware = o->read(o->context, USB_PROBE_HW);
    r->before = o->read(o->context, USB_PROBE_USB);
    o->write(o->context, USB_PROBE_AHB, 0);
    o->write(o->context, USB_PROBE_IRQ_MASK, 0);
    r->operation_result = USB_PROBE_AHB_TIMEOUT;
    if (!wait_bits(o, USB_PROBE_RESET, AHB_IDLE, AHB_IDLE, true))
        goto restore;
    o->write(o->context, USB_PROBE_RESET, CORE_RESET);
    r->operation_result = USB_PROBE_RESET_TIMEOUT;
    if (!wait_bits(o, USB_PROBE_RESET, CORE_RESET, 0, true))
        goto restore;
    /* DWC2 requires at least three PHY clocks after reset clears before
     * accessing the PHY domain. Yielding also keeps the worker responsive. */
    o->sleep_ms(o->context, 1);
    if (!wait_bits(o, USB_PROBE_RESET, AHB_IDLE, AHB_IDLE, true))
        goto restore;
    if (o->cancelled(o->context)) goto restore;
    o->write(o->context, USB_PROBE_USB,
             (r->before & ~FORCE_DEVICE) | FORCE_HOST);
    o->sleep_ms(o->context, 50);
    r->operation_result = USB_PROBE_HOST_TIMEOUT;
    if (!wait_bits(o, USB_PROBE_STATUS, CORE_HOST, CORE_HOST, true))
        goto restore;
    r->host_seen = true;
    r->host = o->read(o->context, USB_PROBE_USB);
    observing = o->now(o->context);
    do
    {
        r->port = o->read(o->context, USB_PROBE_PORT);
        r->connected_seen |= (r->port & PORT_CONNECTED) != 0;
        if (o->cancelled(o->context)) goto restore;
        if (!(o->read(o->context, USB_PROBE_STATUS) & CORE_HOST))
            goto restore;
        o->sleep_ms(o->context, 20);
    } while ((uint32_t)(o->now(o->context) - observing) < 10000);
    r->operation_result = USB_PROBE_OK;
restore:
    if (o->cancelled(o->context))
        r->operation_result = USB_PROBE_CANCELLED;
    o->write(o->context, USB_PROBE_USB,
             (r->before & ~FORCE_HOST) | FORCE_DEVICE);
    o->sleep_ms(o->context, 50);
    /* Cancellation must never cancel the restoration attempt. */
    r->restored = wait_bits(o, USB_PROBE_STATUS, CORE_HOST, 0, false);
    r->after = o->read(o->context, USB_PROBE_USB);
    r->result = r->restored ? r->operation_result : USB_PROBE_RESTORE_FAILED;
    o->end(o->context);
    r->elapsed_ms = o->now(o->context) - start;
}
