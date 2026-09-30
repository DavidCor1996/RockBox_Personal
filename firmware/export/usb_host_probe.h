/* Manual diagnostic API; present only in the opt-in test firmware.
 * GPL-2.0-or-later. */
#ifndef USB_HOST_PROBE_H
#define USB_HOST_PROBE_H
#include "../usbhost/role_probe.h"
bool usb_host_probe_request(void);
void usb_host_probe_cancel(void);
void usb_host_probe_snapshot(struct usb_probe_report *report);
/* USB worker only; UI must use request/cancel/snapshot. */
void usb_host_probe_target_run(struct usb_probe_report *report,
                               volatile bool *cancel);
#endif
