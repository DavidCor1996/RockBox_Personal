/* S5L8702 role-only diagnostic adapter. GPL-2.0-or-later. */
#include "config.h"
#include "system.h"
#include "kernel.h"
#include "usb-designware.h"
#include "usb_host_probe.h"

extern void usb_dw_target_enable_clocks(void);
extern void usb_dw_target_disable_clocks(void);
extern void usb_dw_target_disable_irq(void);

static volatile uint32_t *probe_register(enum usb_probe_register reg)
{
    switch (reg)
    {
    case USB_PROBE_AHB: return &DWC_GAHBCFG;
    case USB_PROBE_USB: return &DWC_GUSBCFG;
    case USB_PROBE_RESET: return &DWC_GRSTCTL;
    case USB_PROBE_STATUS: return &DWC_GINTSTS;
    case USB_PROBE_IRQ_MASK: return &DWC_GINTMSK;
    case USB_PROBE_ID: return &DWC_GSNPSID;
    case USB_PROBE_HW: return &DWC_GHWCFG2;
    case USB_PROBE_PORT: return &DWC_HPRT;
    }
    return &DWC_GINTSTS;
}
static uint32_t probe_read(void *ctx, enum usb_probe_register reg)
{
    (void)ctx;
    return *probe_register(reg);
}
static void probe_write(void *ctx, enum usb_probe_register reg, uint32_t value)
{
    (void)ctx;
    *probe_register(reg) = value;
}
static void probe_begin(void *ctx)
{
    (void)ctx;
    usb_dw_target_disable_irq();
    usb_dw_target_enable_clocks();
    DWC_PCGCCTL = 0;
    DWC_GUSBCFG = usb_dw_config.phytype | TRDT(5) | FDMOD;
}
static void probe_end(void *ctx)
{
    (void)ctx;
    DWC_GAHBCFG = 0;
    DWC_GINTMSK = 0;
    usb_dw_target_disable_irq();
    /* PHY reset and clocks off retire the diagnostic even on mode failure.
     * Normal insertion subsequently initializes the ordinary device stack. */
    usb_dw_target_disable_clocks();
}
static uint32_t probe_now(void *ctx)
{
    (void)ctx;
    return (uint32_t)current_tick * (1000 / HZ);
}
static void probe_sleep(void *ctx, unsigned ms)
{
    (void)ctx;
    sleep(MAX(1, (ms * HZ + 999) / 1000));
}
static bool probe_cancelled(void *ctx)
{
    return *(volatile bool *)ctx;
}
void usb_host_probe_target_run(struct usb_probe_report *report,
                               volatile bool *cancel)
{
    const struct usb_probe_operations ops = {
        .context = (void *)cancel, .begin = probe_begin, .read = probe_read,
        .write = probe_write, .now = probe_now, .sleep_ms = probe_sleep,
        .cancelled = probe_cancelled, .end = probe_end
    };
    usb_role_probe_run(&ops, report);
}
