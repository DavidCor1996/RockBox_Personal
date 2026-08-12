#ifndef USB_IPHONE_PREFLIGHT_H
#define USB_IPHONE_PREFLIGHT_H

#include <stdbool.h>

void usb_iphone_preflight_service(void);
void usb_iphone_preflight_reset(void);
bool usb_iphone_preflight_complete(void);
const char *usb_iphone_preflight_status(void);

#endif
