/***************************************************************************
 * Narrow Apple iPhone USB Ethernet host transport.
 ****************************************************************************/
#ifndef USB_IPHONE_TETHER_H
#define USB_IPHONE_TETHER_H

#include <stdbool.h>

enum usb_iphone_tether_state
{
    USB_IPHONE_OFF = 0,
    USB_IPHONE_WAITING,
    USB_IPHONE_ENUMERATING,
    USB_IPHONE_WAITING_FOR_TRUST,
    USB_IPHONE_LINK,
    USB_IPHONE_ERROR
};

void usb_iphone_tether_start(void);
void usb_iphone_tether_stop(void);
void usb_iphone_tether_service(void);
bool usb_iphone_tether_link_active(void);
enum usb_iphone_tether_state usb_iphone_tether_state(void);
const char *usb_iphone_tether_status(void);
int usb_iphone_tether_send_frame(const void *frame, int length);
int usb_iphone_tether_receive_frame(void *frame, int capacity);
bool usb_iphone_tether_get_mac(unsigned char mac[6]);
bool usb_iphone_tether_get_udid(char *udid, int capacity);
bool usb_iphone_tether_mux_available(void);
int usb_iphone_tether_mux_send(const void *packet, int length);
int usb_iphone_tether_mux_receive(void *packet, int capacity);

#endif
