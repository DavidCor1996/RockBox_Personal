#ifndef ROCKPOD_USB_INTERNET_H
#define ROCKPOD_USB_INTERNET_H

#include <stdbool.h>
#include <stdint.h>

#define USB_INTERNET_WEATHER_PORT 47700
#define USB_INTERNET_CLUB_PENGUIN_PORT 47701
#define USB_INTERNET_SYNC_PORT 47703
#define USB_INTERNET_FRAMEBUFFER_PORT 47704

bool usb_internet_connected(void);
bool usb_internet_needs_service(void);
void usb_internet_service(void);
unsigned long usb_internet_weather_generation(void);
int usb_internet_send(uint16_t port, const void *data, int length);
int usb_internet_receive(uint16_t port, void *data, int capacity);

#endif
