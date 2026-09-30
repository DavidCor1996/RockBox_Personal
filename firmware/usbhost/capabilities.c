/* Passive DesignWare capability decoder. GPL-2.0-or-later.
 * Field definitions: Linux dwc2/hw.h, revision
 * 551c722f40809618230001baccf219193e22fc5a. No imported runtime. */
#include "usb_dw_capabilities.h"
#include <string.h>

void usb_dw_capabilities_record(struct usb_dw_capabilities *cached,
        const struct usb_dw_capabilities *first,
        const struct usb_dw_capabilities *second)
{
    bool consistent = first->core_id == second->core_id &&
        memcmp(first->hwcfg, second->hwcfg, sizeof(first->hwcfg)) == 0;
    if (cached->captures)
        consistent = consistent && cached->consistent &&
            first->core_id == cached->core_id &&
            memcmp(first->hwcfg, cached->hwcfg, sizeof(first->hwcfg)) == 0;

    uint32_t captures = cached->captures;
    *cached = *first;
    cached->consistent = consistent;
    cached->captures = captures == UINT32_MAX ? captures : captures + 1;
}

enum usb_dw_host_gate usb_dw_capabilities_gate(
        const struct usb_dw_capabilities *caps)
{
    /* DWC2 OTG core identity, not the newer DWC3 controller. */
    if (!caps->captures || !caps->consistent ||
        (caps->core_id & 0xfffff000u) != 0x4f542000u)
        return USB_DW_GATE_UNKNOWN;

    /* GHWCFG1 may legitimately be zero (bidirectional endpoints).
     * Reject suspicious required capability words, preserving raw evidence. */
    for (unsigned i = 0; i < 4; i++)
        if (caps->hwcfg[i] == UINT32_MAX ||
            (i != 0 && caps->hwcfg[i] == 0))
            return USB_DW_GATE_UNKNOWN;

    if (((caps->hwcfg[1] >> 3) & 3u) == 3u ||
        (caps->hwcfg[2] >> 16) == 0)
        return USB_DW_GATE_UNKNOWN;

    switch (caps->hwcfg[1] & 7u)
    {
    case 0:
    case 1:
    case 2:
        return USB_DW_GATE_DUAL_ROLE;
    case 3:
    case 4:
        return USB_DW_GATE_DEVICE_ONLY;
    case 5:
    case 6:
        return USB_DW_GATE_HOST_ONLY;
    default:
        return USB_DW_GATE_UNKNOWN;
    }
}
