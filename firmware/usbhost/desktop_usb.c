/* Software-only USB desktop foundation. GPL-2.0-or-later. */
#include "desktop_usb.h"

uint32_t usb_capabilities_set(struct usb_capabilities *c, unsigned slot,
                              uint32_t value)
{
    uint32_t all = 0;
    if (slot < 16) c->device[slot] = value;
    for (unsigned i = 0; i < 16; i++) all |= c->device[i];
    return all;
}

enum usb_desktop_state usb_desktop_state(uint32_t caps)
{
    bool k = (caps & USB_CAP_KEYBOARD) != 0, m = (caps & USB_CAP_MOUSE) != 0;
    if (!(caps & USB_CAP_EXTERNAL_DISPLAY))
        return k || m ? USB_DESKTOP_INPUT_ONLY : USB_DESKTOP_NONE;
    return k && m ? USB_DESKTOP_FULL : k ? USB_DESKTOP_DISPLAY_KEYBOARD :
           m ? USB_DESKTOP_DISPLAY_MOUSE : USB_DESKTOP_DISPLAY_ONLY;
}

bool usb_desktop_activate(uint32_t caps, enum usb_desktop_auto policy)
{
    switch (policy)
    {
        case USB_DESKTOP_AUTO_DISPLAY: return caps & USB_CAP_EXTERNAL_DISPLAY;
        case USB_DESKTOP_AUTO_INPUT:
            return (caps & (USB_CAP_KEYBOARD | USB_CAP_MOUSE)) ==
                          (USB_CAP_KEYBOARD | USB_CAP_MOUSE);
        case USB_DESKTOP_AUTO_FULL:
            return usb_desktop_state(caps) == USB_DESKTOP_FULL;
        default: return false;
    }
}

static unsigned le16(const uint8_t *p) { return p[0] | (unsigned)p[1] << 8; }

bool usb_host_parse_configuration(const uint8_t *p, size_t size,
                                   struct usb_host_configuration *out)
{
    struct usb_host_configuration result = {0};
    struct usb_host_interface *iface = NULL;
    unsigned expected = 0, seen_numbers = 0;
    bool numbers[256] = {false};
    if (!p || !out || size < 9 || p[0] < 9 || p[1] != 2 ||
        le16(p + 2) > size || le16(p + 2) < p[0] || !p[5]) return false;
    size = le16(p + 2);
    result.value = p[5];
    for (size_t pos = p[0]; pos < size;)
    {
        unsigned n = p[pos], type = pos + 1 < size ? p[pos + 1] : 0;
        const uint8_t *d = p + pos;
        if (n < 2 || n > size - pos) return false;
        if (type == 4)
        {
            if (n < 9 || result.count == USB_HOST_INTERFACES ||
                (iface && iface->count != expected) ||
                d[4] > USB_HOST_ENDPOINTS) return false;
            for (unsigned i = 0; i < result.count; i++)
                if (result.interface[i].number == d[2] &&
                    result.interface[i].alternate == d[3]) return false;
            iface = &result.interface[result.count++];
            iface->number = d[2]; iface->alternate = d[3];
            iface->class_code = d[5]; iface->subclass = d[6];
            iface->protocol = d[7]; expected = d[4];
            if (!numbers[d[2]]) { numbers[d[2]] = true; seen_numbers++; }
        }
        else if (type == 5)
        {
            if (n < 7 || !iface || iface->count >= expected ||
                !(d[2] & 15) || (d[2] & 0x70) ||
                !(le16(d + 4) & 0x7ff) || (le16(d + 4) & 0xe000))
                return false;
            for (unsigned i = 0; i < iface->count; i++)
                if (iface->endpoint[i].address == d[2]) return false;
            struct usb_host_endpoint *ep = &iface->endpoint[iface->count++];
            ep->address = d[2]; ep->type = d[3] & 3;
            ep->packet = le16(d + 4) & 0x7ff; ep->interval = d[6];
        }
        pos += n;
    }
    if (!iface || iface->count != expected || seen_numbers != p[4]) return false;
    *out = result;
    return true;
}

bool usb_host_parse_hub(const uint8_t *p, size_t size, struct usb_host_hub *hub)
{
    if (!p || !hub || size < 7 || p[1] != 0x29 || !p[2] || p[2] > 15 ||
        p[0] > size || p[0] < 7 + 2 * ((p[2] + 8) / 8)) return false;
    /* USB 2.0 bPwrOn2PwrGood has 2 ms units. */
    *hub = (struct usb_host_hub){p[2], p[5] * 2u, le16(p + 3)};
    return true;
}

enum usb_hub_stage usb_hub_step(struct usb_hub_port *p, uint32_t now,
    bool connected, bool done, bool failed, unsigned power_good_ms)
{
    bool expired = (int32_t)(now - p->deadline) >= 0;
    if (failed) p->stage = USB_HUB_FAILED;
    else if (!connected && p->stage >= USB_HUB_DEBOUNCE)
        p->stage = USB_HUB_OFF;
    else switch (p->stage)
    {
        case USB_HUB_OFF:
            p->stage = USB_HUB_POWER; p->deadline = now + 1000; break;
        case USB_HUB_POWER:
            if (done) { p->stage = USB_HUB_POWER_WAIT;
                        p->deadline = now + power_good_ms; }
            else if (expired) p->stage = USB_HUB_FAILED;
            break;
        case USB_HUB_POWER_WAIT:
            if (expired && connected) { p->stage = USB_HUB_DEBOUNCE;
                                        p->deadline = now + 100; }
            break;
        case USB_HUB_DEBOUNCE:
            if (expired) { p->stage = USB_HUB_RESET;
                           p->deadline = now + 1000; }
            break;
        case USB_HUB_RESET:
            if (done) { p->stage = USB_HUB_RESET_WAIT;
                        p->deadline = now + 50; }
            else if (expired) p->stage = USB_HUB_FAILED;
            break;
        case USB_HUB_RESET_WAIT:
            if (expired) { p->stage = USB_HUB_ENUMERATE;
                           p->deadline = now + 5000; }
            break;
        case USB_HUB_ENUMERATE:
            if (done) p->stage = USB_HUB_READY;
            else if (expired) p->stage = USB_HUB_FAILED;
            break;
        default: break;
    }
    return p->stage;
}

static bool has_key(const uint8_t *keys, uint8_t key)
{
    for (int i = 0; i < 6; i++) if (keys[i] == key) return true;
    return false;
}

int usb_boot_keyboard_report(struct usb_boot_keyboard *old,
    const uint8_t *p, size_t n, struct usb_key_change *events, size_t capacity)
{
    struct usb_key_change changes[20];
    int count = 0;
    if (!p || !old || !events || n != 8 || p[1]) return -1;
    for (int i = 2; i < 8; i++)
    {
        if (p[i] && p[i] < 4) return -1;
        for (int j = 2; j < i; j++) if (p[i] && p[i] == p[j]) return -1;
    }
    for (int i = 0; i < 8; i++)
        if ((old->modifier ^ p[0]) & (1 << i))
            changes[count++] = (struct usb_key_change)
                {0xe0 + i, p[0], (p[0] & (1 << i)) != 0};
    for (int i = 0; i < 6; i++)
        if (old->keys[i] && !has_key(p + 2, old->keys[i]))
            changes[count++] = (struct usb_key_change){old->keys[i], p[0], false};
    for (int i = 0; i < 6; i++)
        if (p[i + 2] && !has_key(old->keys, p[i + 2]))
            changes[count++] = (struct usb_key_change){p[i + 2], p[0], true};
    if ((size_t)count > capacity) return -2;
    for (int i = 0; i < count; i++) events[i] = changes[i];
    old->modifier = p[0];
    for (int i = 0; i < 6; i++) old->keys[i] = p[i + 2];
    return count;
}

bool usb_boot_mouse_report(const uint8_t *p, size_t n,
                           struct usb_mouse_report *out)
{
    /* Three-byte boot reports; optional fourth signed wheel byte. */
    if (!p || !out || (n != 3 && n != 4)) return false;
    *out = (struct usb_mouse_report){p[0] & 7, (int8_t)p[1],
                                    (int8_t)p[2], n == 4 ? (int8_t)p[3] : 0};
    return true;
}
