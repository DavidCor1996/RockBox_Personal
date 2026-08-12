/***************************************************************************
 * Bounded IPv4/DHCP qualification layer for iPhone USB tethering.
 ****************************************************************************/
#ifndef USB_IPHONE_NETWORK_H
#define USB_IPHONE_NETWORK_H

#include <stdbool.h>
#include <stdint.h>

void usb_iphone_network_service(void);
void usb_iphone_network_reset(void);
bool usb_iphone_network_connected(void);
uint32_t usb_iphone_network_address(void);
uint32_t usb_iphone_network_router(void);
uint32_t usb_iphone_network_dns(void);
int usb_iphone_network_udp_send(uint32_t destination, uint16_t source_port,
                                uint16_t destination_port,
                                const void *data, int length);
int usb_iphone_network_udp_receive(uint16_t port, uint32_t *source,
                                   void *data, int capacity);
const char *usb_iphone_network_status(void);

#endif
