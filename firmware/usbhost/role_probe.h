/* Bounded USB role diagnostic. GPL-2.0-or-later. */
#ifndef USB_ROLE_PROBE_H
#define USB_ROLE_PROBE_H
#include <stdbool.h>
#include <stdint.h>

enum usb_probe_register
{
    USB_PROBE_AHB, USB_PROBE_USB, USB_PROBE_RESET, USB_PROBE_STATUS,
    USB_PROBE_IRQ_MASK, USB_PROBE_ID, USB_PROBE_HW, USB_PROBE_PORT
};
enum usb_probe_result
{
    USB_PROBE_IDLE, USB_PROBE_RUNNING, USB_PROBE_OK, USB_PROBE_REFUSED,
    USB_PROBE_CANCELLED, USB_PROBE_AHB_TIMEOUT, USB_PROBE_RESET_TIMEOUT,
    USB_PROBE_HOST_TIMEOUT, USB_PROBE_RESTORE_FAILED
};
struct usb_probe_report
{
    enum usb_probe_result result;
    enum usb_probe_result operation_result;
    uint32_t core_id, hardware, before, host, after, port, elapsed_ms;
    bool host_seen, connected_seen, restored, log_saved, log_skipped;
};
struct usb_probe_operations
{
    void *context;
    void (*begin)(void *);
    uint32_t (*read)(void *, enum usb_probe_register);
    void (*write)(void *, enum usb_probe_register, uint32_t);
    uint32_t (*now)(void *);
    void (*sleep_ms)(void *, unsigned);
    bool (*cancelled)(void *);
    void (*end)(void *);
};
/* Caller owns exclusive controller access. No endpoint, DMA, port-power or
 * bus-reset command is issued. All failure paths attempt DEVICE restoration. */
void usb_role_probe_run(const struct usb_probe_operations *ops,
                        struct usb_probe_report *report);
#endif
