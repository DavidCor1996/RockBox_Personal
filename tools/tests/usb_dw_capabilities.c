/* Passive USB capability gate fixtures. GPL-2.0-or-later. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "usb_dw_capabilities.h"

int main(void)
{
    struct usb_dw_capabilities caps = {
        .core_id = 0x4f54280a,
        .hwcfg = {0, (2u << 3) | (7u << 14) | (1u << 19),
                  0x08200000, 1u << 25},
        .captures = 2,
        .consistent = true
    };
    const struct usb_dw_capabilities original = caps;
    struct usb_dw_capabilities cached = {0}, second = caps;
    usb_dw_capabilities_record(&cached, &original, &second);
    assert(cached.captures == 1 && cached.consistent);
    usb_dw_capabilities_record(&cached, &original, &second);
    assert(cached.captures == 2 && cached.consistent);
    second.hwcfg[3] ^= 1;
    usb_dw_capabilities_record(&cached, &original, &second);
    assert(cached.captures == 3 && !cached.consistent);
    second = original;
    usb_dw_capabilities_record(&cached, &original, &second);
    assert(cached.captures == 4 && !cached.consistent);
    memset(&cached, 0, sizeof(cached));
    usb_dw_capabilities_record(&cached, &original, &original);
    second.core_id ^= 1;
    usb_dw_capabilities_record(&cached, &second, &second);
    assert(cached.captures == 2 && !cached.consistent);
    assert(cached.core_id == second.core_id);
    cached = original;
    cached.captures = UINT32_MAX;
    usb_dw_capabilities_record(&cached, &original, &original);
    assert(cached.captures == UINT32_MAX && cached.consistent);
    for (unsigned mode = 0; mode < 8; mode++)
    {
        caps.hwcfg[1] = original.hwcfg[1] | mode;
        enum usb_dw_host_gate expected = mode < 3 ? USB_DW_GATE_DUAL_ROLE :
            mode < 5 ? USB_DW_GATE_DEVICE_ONLY :
            mode < 7 ? USB_DW_GATE_HOST_ONLY : USB_DW_GATE_UNKNOWN;
        struct usb_dw_capabilities before = caps;
        assert(usb_dw_capabilities_gate(&caps) == expected);
        assert(memcmp(&before, &caps, sizeof(caps)) == 0);
    }
    caps = original;
    caps.captures = 0;
    assert(usb_dw_capabilities_gate(&caps) == USB_DW_GATE_UNKNOWN);
    caps = original;
    caps.consistent = false;
    assert(usb_dw_capabilities_gate(&caps) == USB_DW_GATE_UNKNOWN);
    caps = original;
    caps.core_id = 0;
    assert(usb_dw_capabilities_gate(&caps) == USB_DW_GATE_UNKNOWN);
    caps.core_id = UINT32_MAX;
    assert(usb_dw_capabilities_gate(&caps) == USB_DW_GATE_UNKNOWN);
    caps.core_id = 0x55330000; /* DWC3 is not DWC2. */
    assert(usb_dw_capabilities_gate(&caps) == USB_DW_GATE_UNKNOWN);
    for (unsigned i = 0; i < 4; i++)
    {
        caps = original;
        caps.hwcfg[i] = UINT32_MAX;
        assert(usb_dw_capabilities_gate(&caps) == USB_DW_GATE_UNKNOWN);
    }
    caps = original;
    caps.hwcfg[1] |= 3u << 3; /* Reserved DMA architecture. */
    assert(usb_dw_capabilities_gate(&caps) == USB_DW_GATE_UNKNOWN);
    caps = original;
    caps.hwcfg[2] = 1; /* No advertised FIFO memory. */
    assert(usb_dw_capabilities_gate(&caps) == USB_DW_GATE_UNKNOWN);
    memset(&caps, 0, sizeof(caps));
    caps.captures = 1;
    caps.consistent = true;
    assert(usb_dw_capabilities_gate(&caps) == USB_DW_GATE_UNKNOWN);
    puts("USB capability gate fixtures passed (no hardware qualification)");
    return 0;
}
