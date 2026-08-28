/***************************************************************************
 * Rockbox USB CDC Ethernet Control Model class driver
 ****************************************************************************/
#ifndef USB_ETHERNET_H
#define USB_ETHERNET_H

#include "usb_ch9.h"
#include "usb_class_driver.h"

#define USB_ETHERNET_MTU 1500
#define USB_ETHERNET_FRAME_MAX 1518
/* One 320x240 frame is ~112 datagrams. Each transmit block costs a thread
 * scheduling round trip on the background streaming worker -- measured at
 * ~19ms on iPod Classic, against well under 1ms of actual wire time -- so
 * frame rate tracks blocks-per-frame, not link bandwidth. Carry as many
 * datagrams per block as the negotiated block size allows. */
#define USB_ETHERNET_UDP_BATCH_MAX 22

struct usb_ethernet_udp_datagram {
    const void *data;
    uint16_t length;
};

extern struct usb_class_driver_ep_allocation usb_ethernet_ep_allocs[3];

int usb_ethernet_set_first_interface(int interface);
int usb_ethernet_get_config_descriptor(unsigned char *dest,
                                       int max_packet_size);
void usb_ethernet_init_connection(void);
void usb_ethernet_init(void);
void usb_ethernet_disconnect(void);
void usb_ethernet_transfer_complete(int ep, int dir, int status, int length);
bool usb_ethernet_control_request(struct usb_ctrlrequest *req, void *reqdata,
                                  unsigned char *dest);
int usb_ethernet_set_interface(int interface, int alt_setting);
int usb_ethernet_get_interface(int interface);

bool usb_ethernet_link_active(void);
bool usb_ethernet_wait_for_tx(int timeout);
int usb_ethernet_send_frame(const void *frame, int length);
int usb_ethernet_udp_send(uint16_t port, const void *data, int length);
int usb_ethernet_udp_send_batch(
    uint16_t port, const struct usb_ethernet_udp_datagram *datagrams,
    int count);
int usb_ethernet_udp_batch_capacity(int length);
int usb_ethernet_udp_receive(uint16_t port, void *data, int capacity);

#endif
