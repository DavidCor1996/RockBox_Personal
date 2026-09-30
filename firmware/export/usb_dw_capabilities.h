/* Passive DesignWare capability diagnostic. GPL-2.0-or-later. */
#ifndef USB_DW_CAPABILITIES_H
#define USB_DW_CAPABILITIES_H

#include <stdbool.h>
#include <stdint.h>

struct usb_dw_capabilities
{
    uint32_t core_id;
    uint32_t hwcfg[4];
    uint32_t captures;
    bool consistent;
};

enum usb_dw_host_gate
{
    USB_DW_GATE_UNKNOWN,
    USB_DW_GATE_DUAL_ROLE,
    USB_DW_GATE_DEVICE_ONLY,
    USB_DW_GATE_HOST_ONLY
};

/* Pure decoder; a passing gate permits investigation, not host operation. */
enum usb_dw_host_gate usb_dw_capabilities_gate(
        const struct usb_dw_capabilities *caps);
void usb_dw_capabilities_record(struct usb_dw_capabilities *cached,
        const struct usb_dw_capabilities *first,
        const struct usb_dw_capabilities *second);

/* Target hook: normal DEVICE initialization only, with clocks already valid.
 * The UI reads a RAM copy, including after controller shutdown. */
void usb_dw_target_capture_capabilities(void);
void usb_dw_get_capabilities(struct usb_dw_capabilities *caps);

#endif
