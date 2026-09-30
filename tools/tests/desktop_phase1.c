/* Software-only Phase 1 contract tests. GPL-2.0-or-later. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "desktop_model.h"
#include "desktop_surface.h"
#include "desktop_usb.h"
#include "dl1xx.h"

static void library(void)
{
    /* Same-title albums under different artists keep independent database
     * identities; string filtering cannot satisfy this fixture. */
    struct song { int artist, album; const char *title; } songs[] = {
        {10, 101, "Song 1"}, {10, 101, "Song 2"},
        {10, 102, "Song 3"}, {20, 201, "Song 4"}};
    struct desktop_library l;
    desktop_library_root(&l, DESKTOP_ARTISTS);
    l.current.page = 7; l.current.selected = 2;
    assert(desktop_library_open(&l, 10));
    assert(l.current.source == DESKTOP_ALBUMS && l.current.artist == 10);
    int albums[4], count = 0;
    for (unsigned i = 0; i < 4; i++)
        if (songs[i].artist == l.current.artist &&
            (!count || albums[count-1] != songs[i].album))
            albums[count++] = songs[i].album;
    assert(count == 2 && albums[0] == 101 && albums[1] == 102);
    assert(desktop_library_open(&l, 101));
    count = 0;
    for (unsigned i = 0; i < 4; i++)
        if (songs[i].artist == l.current.artist && songs[i].album == l.current.album)
            count++;
    assert(count == 2 && l.current.source == DESKTOP_SONGS);
    assert(!desktop_library_open(&l, 1));
    assert(desktop_library_back(&l) && l.current.source == DESKTOP_ALBUMS);
    assert(l.current.artist == 10 && l.current.album == -1);
    assert(desktop_library_back(&l) && l.current.page == 7 && l.current.selected == 2);
    assert(!desktop_library_back(&l));
    desktop_library_root(&l, DESKTOP_SONGS);
    assert(l.current.artist == -1 && l.current.album == -1);
    desktop_library_root(&l, DESKTOP_PLAYLISTS);
    assert(desktop_library_open(&l, 1) && l.current.source == DESKTOP_SONGS);
}

static bool busy;
static struct desktop_rect received;
static bool present(void *ctx, const struct desktop_surface *s, struct desktop_rect r)
{
    (void)ctx; assert(s->stride == 640); received = r; return !busy;
}
static void geometry(void)
{
    struct desktop_rect src, dst;
    assert(desktop_image_layout(1600, 900, 640, 480, DESKTOP_FILL, &src, &dst));
    assert(src.x == 200 && src.width == 1200 && dst.width == 640);
    assert(desktop_image_layout(1600, 900, 640, 480, DESKTOP_FIT, &src, &dst));
    assert(dst.y == 60 && dst.height == 360);
    assert(desktop_image_layout(100, 50, 640, 480, DESKTOP_CENTER, &src, &dst));
    assert(dst.x == 270 && dst.y == 215 && dst.width == 100);
    assert(!desktop_image_layout(INT_MAX, 1, 640, 480, DESKTOP_FILL, &src, &dst));
    assert(desktop_hit((struct desktop_rect){5, 5, 2, 2}, 5, 6));
    assert(!desktop_hit((struct desktop_rect){5, 5, 2, 2}, 7, 6));
    static uint16_t pixels[640 * 480];
    struct desktop_surface s;
    assert(!desktop_surface_init(&s, pixels, 5, 640, 480, 640, present, NULL));
    assert(desktop_surface_init(&s, pixels, sizeof(pixels), 640, 480, 640, present, NULL));
    busy = true; assert(!desktop_surface_present(&s) && s.damage.width == 640);
    busy = false; assert(desktop_surface_present(&s) && s.generation == 1);
    desktop_surface_damage(&s, (struct desktop_rect){10, 20, 16, 20});
    desktop_surface_damage(&s, (struct desktop_rect){30, 25, 16, 20});
    assert(desktop_surface_present(&s));
    assert(received.x == 10 && received.y == 20 && received.width == 36 && received.height == 25);
    desktop_surface_damage(&s, (struct desktop_rect){INT_MAX, INT_MAX, INT_MAX, INT_MAX});
    assert(s.damage.width == 0);
    desktop_surface_damage(&s, (struct desktop_rect){-10, -10, 20, 20});
    assert(s.damage.x == 0 && s.damage.width == 10);
}

static void descriptors(void)
{
    uint8_t configuration[] = {9,2,34,0,1,1,0,0x80,50,
        9,4,0,0,2,3,1,1,0, 7,5,0x81,3,8,0,10, 9,5,2,2,64,0,0,0,0};
    struct usb_host_configuration c = {0};
    assert(usb_host_parse_configuration(configuration, sizeof(configuration), &c));
    assert(c.count == 1 && c.interface[0].endpoint[0].packet == 8);
    for (size_t i = 0; i < sizeof(configuration); i++)
        assert(!usb_host_parse_configuration(configuration, i, &c));
    configuration[18] = 0;
    assert(!usb_host_parse_configuration(configuration, sizeof(configuration), &c));
    configuration[18] = 7; configuration[20] = 0;
    assert(!usb_host_parse_configuration(configuration, sizeof(configuration), &c));
    uint8_t h[] = {9,0x29,4,9,0,50,0,0,255};
    struct usb_host_hub hub;
    assert(usb_host_parse_hub(h, sizeof(h), &hub));
    assert(hub.ports == 4 && hub.power_good_ms == 100);
    h[2] = 16; assert(!usb_host_parse_hub(h, sizeof(h), &hub));
    struct usb_hub_port port = {0};
    assert(usb_hub_step(&port, 0, true, false, false, 100) == USB_HUB_POWER);
    assert(usb_hub_step(&port, 1, true, true, false, 100) == USB_HUB_POWER_WAIT);
    assert(usb_hub_step(&port, 100, true, false, false, 100) == USB_HUB_POWER_WAIT);
    assert(usb_hub_step(&port, 101, true, false, false, 100) == USB_HUB_DEBOUNCE);
    assert(usb_hub_step(&port, 201, true, false, false, 100) == USB_HUB_RESET);
    assert(usb_hub_step(&port, 202, true, true, false, 100) == USB_HUB_RESET_WAIT);
    assert(usb_hub_step(&port, 252, true, false, false, 100) == USB_HUB_ENUMERATE);
    assert(usb_hub_step(&port, 253, true, true, false, 100) == USB_HUB_READY);
    assert(usb_hub_step(&port, 254, false, false, false, 100) == USB_HUB_OFF);
}

static void hid(void)
{
    struct usb_boot_keyboard old = {0};
    struct usb_key_change events[20];
    uint8_t report[8] = {2,0,4,5,0,0,0,0};
    assert(usb_boot_keyboard_report(&old, report, 8, events, 20) == 3);
    assert(events[0].usage == 0xe1 && events[0].down);
    assert(usb_boot_keyboard_report(&old, report, 8, events, 20) == 0);
    report[2] = 1;
    assert(usb_boot_keyboard_report(&old, report, 8, events, 20) == -1);
    assert(old.keys[0] == 4);
    memset(report, 0, sizeof(report));
    assert(usb_boot_keyboard_report(&old, report, 8, events, 1) == -2);
    assert(old.keys[0] == 4);
    assert(usb_boot_keyboard_report(&old, report, 8, events, 20) == 3);
    assert(!events[2].down);
    struct usb_mouse_report mouse;
    uint8_t m[] = {3, 255, 127, 128};
    assert(usb_boot_mouse_report(m, 4, &mouse));
    assert(mouse.x == -1 && mouse.y == 127 && mouse.wheel == -128);
    assert(!usb_boot_mouse_report(m, 2, &mouse));
    struct usb_capabilities caps = {0};
    uint32_t all = usb_capabilities_set(&caps, 1, USB_CAP_KEYBOARD);
    assert(usb_desktop_state(all) == USB_DESKTOP_INPUT_ONLY);
    all = usb_capabilities_set(&caps, 2, USB_CAP_MOUSE | USB_CAP_EXTERNAL_DISPLAY);
    assert(usb_desktop_state(all) == USB_DESKTOP_FULL);
    assert(usb_desktop_activate(all, USB_DESKTOP_AUTO_FULL));
    assert(!usb_desktop_activate(all, USB_DESKTOP_AUTO_OFF));
    usb_capabilities_set(&caps, 3, USB_CAP_KEYBOARD);
    all = usb_capabilities_set(&caps, 1, 0);
    assert(usb_desktop_state(all) == USB_DESKTOP_FULL);
    all = usb_capabilities_set(&caps, 2, 0);
    assert(!usb_desktop_activate(all, USB_DESKTOP_AUTO_INPUT));
}

/* Independent decoder oracle: validate emitted runs, byte order and total. */
static void decode(const uint8_t *p, size_t n, uint16_t *out, size_t expected)
{
    assert(n >= 9 && p[0] == 0xaf && p[1] == 0x6b);
    size_t i = 6, count = 0;
    assert((p[5] ? p[5] : 256) == expected);
    while (count < expected)
    {
        unsigned raw = p[i++]; if (!raw) raw = 256;
        while (raw--)
        {
            assert(i + 1 < n && count < expected);
            out[count++] = (uint16_t)p[i] << 8 | p[i + 1]; i += 2;
        }
        if (count < expected)
        {
            assert(i < n); unsigned repeat = p[i++];
            uint16_t last = out[count - 1];
            while (repeat--) { assert(count < expected); out[count++] = last; }
        }
    }
    assert(i == n);
}
static void display(void)
{
    uint8_t encoded[1024]; uint16_t p[256], decoded[256];
    const uint8_t oracle[] = {0xaf,0x6b,0,0,0,0,1,0xf8,0,255};
    for (int i = 0; i < 256; i++) p[i] = 0xf800;
    size_t n = dl1xx_encode(encoded, sizeof(encoded), 0, p, 256, true);
    assert(n == sizeof(oracle) && !memcmp(encoded, oracle, n));
    for (int pattern = 0; pattern < 4; pattern++)
        for (size_t count = 1; count <= 256; count++)
        {
            for (size_t i = 0; i < count; i++)
                p[i] = pattern == 0 ? 3 : pattern == 1 ? i : pattern == 2 ? i / 7 : i % 2;
            for (int rle = 0; rle < 2; rle++)
            {
                n = dl1xx_encode(encoded, sizeof(encoded), 0x100, p, count, rle);
                assert(n); decode(encoded, n, decoded, count);
                assert(!memcmp(decoded, p, count * 2));
                uint8_t guard[1024]; memset(guard, 0x55, sizeof(guard));
                assert(!dl1xx_encode(guard, n - 1, 0, p, count, rle));
                for (size_t j = 0; j < sizeof(guard); j++) assert(guard[j] == 0x55);
            }
        }
    assert(!dl1xx_encode(encoded, sizeof(encoded), 0xffffff, p, 1, true));
    uint8_t edid[128] = {0,255,255,255,255,255,255,0};
    edid[18] = 1; edid[19] = 3;
    uint8_t dtd[] = {0xd5,9,0x80,0xa0,0x20,0xe0,45,0x10,16,96,0xa2,0,0,0,0,0,0,0x18};
    memcpy(edid + 54, dtd, 18);
    for (int i = 0; i < 127; i++) edid[127] -= edid[i];
    struct dl1xx_mode mode;
    assert(dl1xx_edid(edid, sizeof(edid), &mode));
    assert(mode.width == 640 && mode.height == 480 && mode.clock_khz == 25170);
    edid[100] ^= 1; assert(!dl1xx_edid(edid, sizeof(edid), &mode));
    static struct dl1xx_commands commands;
    int first = dl1xx_command_acquire(&commands); commands.used[first] = 10;
    assert(dl1xx_command_submit(&commands, first));
    int second = dl1xx_command_acquire(&commands); assert(second != first);
    commands.used[second] = 10; assert(dl1xx_command_submit(&commands, second));
    assert(dl1xx_command_acquire(&commands) == -1);
    dl1xx_command_complete(&commands, first);
    assert(dl1xx_command_acquire(&commands) == first);
}
int main(void)
{
    library(); geometry(); descriptors(); hid(); display();
    puts("PASS: hierarchy, geometry, surface backpressure, descriptors, hub, HID, capabilities, EDID and DL-1xx vectors");
    return 0;
}
