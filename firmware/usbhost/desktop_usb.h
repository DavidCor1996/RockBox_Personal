/* Software-only USB desktop foundation. GPL-2.0-or-later.
 * No registers, interrupts, VBUS control, or automatic role switching. */
#ifndef DESKTOP_USB_H
#define DESKTOP_USB_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

enum usb_role { USB_ROLE_DEVICE, USB_ROLE_HOST };
enum usb_capability
{
    USB_CAP_EXTERNAL_DISPLAY = 1, USB_CAP_KEYBOARD = 2, USB_CAP_MOUSE = 4,
    USB_CAP_HUB = 8, USB_CAP_ETHERNET = 16, USB_CAP_GAMEPAD = 32,
    USB_CAP_STORAGE = 64
};
enum usb_desktop_state { USB_DESKTOP_NONE, USB_DESKTOP_INPUT_ONLY,
    USB_DESKTOP_DISPLAY_ONLY, USB_DESKTOP_DISPLAY_KEYBOARD,
    USB_DESKTOP_DISPLAY_MOUSE, USB_DESKTOP_FULL };
enum usb_desktop_auto { USB_DESKTOP_AUTO_OFF, USB_DESKTOP_AUTO_DISPLAY,
                       USB_DESKTOP_AUTO_INPUT, USB_DESKTOP_AUTO_FULL };
struct usb_capabilities { uint32_t device[16]; };
uint32_t usb_capabilities_set(struct usb_capabilities *caps, unsigned slot,
                              uint32_t value);
enum usb_desktop_state usb_desktop_state(uint32_t caps);
bool usb_desktop_activate(uint32_t caps, enum usb_desktop_auto policy);

#define USB_HOST_INTERFACES 8
#define USB_HOST_ENDPOINTS 8
struct usb_host_endpoint { uint8_t address, type, interval; uint16_t packet; };
struct usb_host_interface
{
    uint8_t number, alternate, class_code, subclass, protocol, count;
    struct usb_host_endpoint endpoint[USB_HOST_ENDPOINTS];
};
struct usb_host_configuration
{
    uint8_t value, count;
    struct usb_host_interface interface[USB_HOST_INTERFACES];
};
bool usb_host_parse_configuration(const uint8_t *data, size_t length,
                                   struct usb_host_configuration *out);
struct usb_host_hub { uint8_t ports; uint16_t power_good_ms, characteristics; };
bool usb_host_parse_hub(const uint8_t *data, size_t length,
                        struct usb_host_hub *hub);

/* Timed hub actions run in a worker. Completion is explicit; ticks never do
 * I2C, control transfers, waits, or channel programming. */
enum usb_hub_stage { USB_HUB_OFF, USB_HUB_POWER, USB_HUB_POWER_WAIT,
    USB_HUB_DEBOUNCE, USB_HUB_RESET, USB_HUB_RESET_WAIT, USB_HUB_ENUMERATE,
    USB_HUB_READY, USB_HUB_FAILED };
struct usb_hub_port { enum usb_hub_stage stage; uint32_t deadline; };
enum usb_hub_stage usb_hub_step(struct usb_hub_port *port, uint32_t now_ms,
    bool connected, bool completed, bool failed, unsigned power_good_ms);

struct usb_host_channel
{
    uint8_t address, endpoint, speed, type, toggle, hub, port;
    bool busy, split;
    uint16_t max_packet;
    uint32_t deadline, transferred;
    void *buffer;
    size_t length;
};
/* Controller boundary: future target implementation must arbitrate the DWC
 * role with device mode and the existing ipheth experiment, drain channels,
 * and restore device mode on every error/detach. No backend is installed. */
struct usb_host_operations
{
    bool (*set_role)(enum usb_role role);
    bool (*submit)(struct usb_host_channel *channel);
    void (*cancel)(struct usb_host_channel *channel);
};

struct usb_boot_keyboard { uint8_t modifier, keys[6]; };
struct usb_key_change { uint8_t usage, modifiers; bool down; };
/* Returns event count, -1 for invalid/rollover, -2 if capacity is inadequate.
 * Failure preserves the previous report so rollover never releases all keys. */
int usb_boot_keyboard_report(struct usb_boot_keyboard *previous,
    const uint8_t *data, size_t length, struct usb_key_change *events,
    size_t capacity);
struct usb_mouse_report { uint8_t buttons; int x, y, wheel; };
bool usb_boot_mouse_report(const uint8_t *data, size_t length,
                           struct usb_mouse_report *out);
#endif
