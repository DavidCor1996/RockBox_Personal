/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Offline Internet browser for local early-web archives.
 *
 ****************************************************************************/

#include "plugin.h"
#include "lib/video_player.h"

#define OW_ROOT          ROCKBOX_DIR "/offlineweb"
#define OW_INDEX         OW_ROOT "/index.html"
#define OW_SHORTCUTS     "rockbox:shortcuts"
#define OW_GOOGLE        "rockbox:google"
#define OW_GOOGLE_SEARCH "rockbox:google-search"
#define OW_LIVE_PAGE     ROCKBOX_DIR "/offlineweb/cache/live.html"
#define OW_LIVE_TEMP     ROCKBOX_DIR "/offlineweb/cache/live.tmp"
#define OW_LIVE_BITMAP   ROCKBOX_DIR "/offlineweb/cache/live.bmp"
#define OW_BROWSER_PORT  47702
#define OW_BROWSER_MAX   (256 * 1024)
#define OW_BROWSER_PACKET_MAX 1400
#define OW_PAGES         OW_ROOT "/cache/pages.tsv"
#define OW_HISTORY       OW_ROOT "/cache/history.tsv"
#define OW_FAVORITES     OW_ROOT "/cache/favorites.tsv"
#define OW_YOUTUBE_SUBSCRIPTIONS \
    OW_ROOT "/archive/www.youtube.com/subscriptions.html"
#define OW_INSTAGRAM_PROFILES \
    OW_ROOT "/archive/www.instagram.com/profiles.html"
#define OW_ONLYFANS_PROFILES \
    OW_ROOT "/archive/onlyfans.com/profiles.html"
#define OW_MAX_PAGES     128
#define OW_MAX_LINES     384
#define OW_MAX_LINKS     192
#define OW_LINE_LEN      88
#define OW_FIELD_LEN     80
#define OW_STACK_LEN     32

#define OW_STYLE_TEXT    0x00
#define OW_STYLE_HEADING 0x01
#define OW_STYLE_LINK    0x02
#define OW_STYLE_IMAGE   0x04
#define OW_STYLE_RULE    0x08
#define OW_STYLE_MEDIA   0x10
#define OW_STYLE_META    0x20

#define OW_TOP_NAV_H     19
#define OW_TOP_H         38
#define OW_BOTTOM_H      24
#define OW_MARGIN_X      5
#define OW_SCROLLBAR_W   4
#define OW_SCROLL_STEP   6
#define OW_SCROLL_REPEAT 14
#define OW_IMAGE_MAX_H   (LCD_HEIGHT - OW_TOP_H - OW_BOTTOM_H - 8)
#define OW_IMAGE_DECODE_SLACK (64 * 1024 / sizeof(fb_data))
#define OW_BROWSER_CONTINUE -1000
#define OW_BROWSER_BACK     -1001

#ifdef HAVE_LCD_COLOR
#define OW_IPODJS_HEADER_TOP       LCD_RGBPACK(252, 253, 253)
#define OW_IPODJS_HEADER_BOTTOM    LCD_RGBPACK(174, 178, 183)
#define OW_IPODJS_TOOLBAR_MID      LCD_RGBPACK(218, 221, 225)
#define OW_IPODJS_ADDRESS_BG       LCD_RGBPACK(247, 247, 247)
#define OW_IPODJS_ADDRESS_BORDER   LCD_RGBPACK(132, 136, 141)
#define OW_IPODJS_HEADER_DARK      LCD_RGBPACK(24, 29, 38)
#define OW_IPODJS_HEADER_DARK_LINE LCD_RGBPACK(54, 60, 70)
#define OW_IPODJS_DARK_ADDRESS     LCD_RGBPACK(37, 41, 48)
#define OW_IPODJS_SCREEN_BG        LCD_RGBPACK(255, 255, 255)
#define OW_IPODJS_DARK_BG          LCD_RGBPACK(18, 20, 24)
#define OW_IPODJS_DARK_PANEL       LCD_RGBPACK(24, 27, 32)
#define OW_IPODJS_TEXT             LCD_RGBPACK(0, 0, 0)
#define OW_IPODJS_DARK_TEXT        LCD_RGBPACK(239, 242, 246)
#define OW_IPODJS_MUTED_TEXT       LCD_RGBPACK(99, 101, 103)
#define OW_IPODJS_DARK_MUTED       LCD_RGBPACK(166, 173, 184)
#define OW_IPODJS_SPLIT            LCD_RGBPACK(210, 210, 210)
#define OW_IPODJS_MEDIA_TOP        LCD_RGBPACK(246, 247, 248)
#define OW_IPODJS_MEDIA_BOTTOM     LCD_RGBPACK(218, 220, 223)
#define OW_IPODJS_SHADOW           LCD_RGBPACK(142, 145, 149)
#define OW_IPODJS_ACTIVE_BOTTOM    LCD_RGBPACK(0, 92, 192)
#define OW_INSTAGRAM_NAV            LCD_RGBPACK(18, 18, 20)
#define OW_ONLYFANS_NAV             LCD_RGBPACK(0, 145, 234)
#define OW_YOUTUBE_BLUE             LCD_RGBPACK(0, 51, 204)
#define OW_YOUTUBE_PANEL            LCD_RGBPACK(230, 241, 250)
#define OW_YOUTUBE_PANEL_LINE       LCD_RGBPACK(153, 187, 221)
#endif

struct ow_page {
    char title[OW_FIELD_LEN];
    char url[OW_FIELD_LEN];
    char path[MAX_PATH];
    char source[32];
    char neighborhood[32];
    char author[48];
    char archived[32];
    char keywords[96];
};

struct ow_link {
    char label[OW_FIELD_LEN];
    char target[MAX_PATH];
};

struct ow_render_line {
    char text[OW_LINE_LEN];
    char image_path[MAX_PATH];
    int image_height;
    int link;
    unsigned char style;
};

struct ow_render {
    struct ow_render_line lines[OW_MAX_LINES];
    int line_count;
    struct ow_link links[OW_MAX_LINKS];
    int link_count;
};

struct ow_youtube_page {
    bool active;
    char video_path[MAX_PATH];
    char frame_path[MAX_PATH];
    char title[OW_FIELD_LEN];
    char uploader[OW_FIELD_LEN];
    char added[32];
    char duration[24];
    char views[32];
    char description[OW_LINE_LEN];
    int frame_count;
    int frame_index;
    long frame_start_tick;
    bool chrome_drawn;
};

static struct ow_page pages[OW_MAX_PAGES];
static int page_count;
static struct ow_render render;
static struct ow_youtube_page youtube_page;
static char current_path[MAX_PATH];
static char back_stack[OW_STACK_LEN][MAX_PATH];
static int back_count;
static int browser_scroll;
static int browser_selected_link;
static int browser_zoom;
/* Decoder workspace is plugin-owned.  Keep it separate from Rockbox's audio
 * and playback buffers so browser artwork can never interrupt music. */
static fb_data image_pixels[LCD_WIDTH * OW_IMAGE_MAX_H +
                            OW_IMAGE_DECODE_SLACK];
static int image_cache_line = -1;
static int image_cache_width;
static int image_cache_height;
static int youtube_frame_fd = -1;
static bool online_mode;
static uint32_t live_request_id;
struct ow_live_transfer {
    bool active;
    uint32_t request_id;
    uint32_t expected_size;
    uint32_t expected_crc;
    uint32_t received;
    uint32_t crc;
    long deadline;
    int fd;
};
static struct ow_live_transfer live_transfer;
static int live_visual_scroll;
static int live_queued_scroll;
/* Shared transfer buffer keeps 1360-byte payloads out of the plugin stack and
 * cuts a typical live viewport from dozens of stop-and-wait USB packets to a
 * small handful. */
static unsigned char live_packet[OW_BROWSER_PACKET_MAX];
#ifdef SIMULATOR
static bool remote_preview_mode;
#endif

static const char *ow_basename(const char *path);
static int ow_line_height(void);
static int ow_line_for_link(int link);

static bool ow_online_connected(void)
{
#ifdef USB_ENABLE_ETHERNET
    return rb->usb_internet_connected();
#else
    return false;
#endif
}

static uint32_t ow_get_be32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static void ow_put_be32(unsigned char *p, uint32_t value)
{
    p[0] = value >> 24;
    p[1] = value >> 16;
    p[2] = value >> 8;
    p[3] = value;
}

static bool ow_browser_send(const void *packet, int length, int retry_ticks)
{
#ifdef USB_ENABLE_ETHERNET
    long deadline = *rb->current_tick + MAX(1, retry_ticks);

    do
    {
        rb->usb_internet_service();
        if (rb->usb_internet_send(OW_BROWSER_PORT, packet, length) >= 0)
            return true;
        rb->sleep(1);
    }
    while (ow_online_connected() &&
           TIME_BEFORE(*rb->current_tick, deadline));
#else
    (void)packet;
    (void)length;
    (void)retry_ticks;
#endif
    return false;
}

static void ow_browser_ack(uint32_t request_id, uint32_t offset)
{
#ifdef USB_ENABLE_ETHERNET
    unsigned char packet[13] = { 'R', 'P', 'B', '1', 5 };
    ow_put_be32(packet + 5, request_id);
    ow_put_be32(packet + 9, offset);
    ow_browser_send(packet, sizeof(packet), HZ / 5);
#else
    (void)request_id;
    (void)offset;
#endif
}

static void ow_draw_loading(const char *message)
{
    rb->lcd_set_viewport(NULL);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_putsxy(8, 8, "Safari");
    rb->lcd_putsxy(8, LCD_HEIGHT / 2 - 5, message);
    rb->lcd_update();
}

static bool ow_install_live_bundle(void)
{
    unsigned char header[12];
    unsigned char buffer[512];
    uint32_t html_size;
    uint32_t bitmap_size;
    int source = -1;
    int output = -1;
    bool ok = false;

    source = rb->open(OW_LIVE_TEMP, O_RDONLY);
    if (source < 0 || rb->read(source, header, sizeof(header)) !=
                      (int)sizeof(header) || rb->memcmp(header, "RPWB", 4))
        goto done;
    html_size = ow_get_be32(header + 4);
    bitmap_size = ow_get_be32(header + 8);
    if (!html_size || html_size + bitmap_size + sizeof(header) >
                       OW_BROWSER_MAX)
        goto done;

    output = rb->open(OW_LIVE_PAGE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (output < 0)
        goto done;
    for (uint32_t left = html_size; left > 0;)
    {
        int chunk = MIN(left, (uint32_t)sizeof(buffer));
        if (rb->read(source, buffer, chunk) != chunk ||
            rb->write(output, buffer, chunk) != chunk)
            goto done;
        left -= chunk;
    }
    rb->close(output);
    output = -1;

    if (bitmap_size)
    {
        output = rb->open(OW_LIVE_BITMAP,
                          O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (output < 0)
            goto done;
        for (uint32_t left = bitmap_size; left > 0;)
        {
            int chunk = MIN(left, (uint32_t)sizeof(buffer));
            if (rb->read(source, buffer, chunk) != chunk ||
                rb->write(output, buffer, chunk) != chunk)
                goto done;
            left -= chunk;
        }
        rb->close(output);
        output = -1;
    }
    ok = true;

done:
    if (output >= 0)
        rb->close(output);
    if (source >= 0)
        rb->close(source);
    rb->remove(OW_LIVE_TEMP);
    return ok;
}

static bool ow_fetch_live_request(unsigned char request_type,
                                  const char *value)
{
#if defined(USB_ENABLE_ETHERNET)
    unsigned char *packet = live_packet;
    uint32_t expected_size = 0;
    uint32_t expected_crc = 0;
    uint32_t received = 0;
    uint32_t crc = 0xffffffff;
    long deadline;
    int fd = -1;
    int length;

    if (!ow_online_connected())
        return false;
    live_request_id++;
    if (!live_request_id)
        live_request_id = 1;
    rb->memcpy(packet, "RPB1", 4);
    packet[4] = request_type;
    ow_put_be32(packet + 5, live_request_id);
    rb->strlcpy((char *)packet + 9, value, OW_BROWSER_PACKET_MAX - 9);
    length = 9 + rb->strlen((char *)packet + 9);
    if (!ow_browser_send(packet, length, HZ))
        return false;

    ow_draw_loading("Loading web page...");
    deadline = *rb->current_tick + 25 * HZ;
    while (TIME_BEFORE(*rb->current_tick, deadline))
    {
        rb->usb_internet_service();
        length = rb->usb_internet_receive(OW_BROWSER_PORT, packet,
                                          OW_BROWSER_PACKET_MAX);
        if (length <= 0)
        {
            rb->sleep(1);
            continue;
        }
        if (length < 9 || rb->memcmp(packet, "RPB1", 4) ||
            ow_get_be32(packet + 5) != live_request_id)
            continue;
        if (packet[4] == 2 && length == 17)
        {
            expected_size = ow_get_be32(packet + 9);
            expected_crc = ow_get_be32(packet + 13);
            if (!expected_size || expected_size > OW_BROWSER_MAX)
                break;
            if (fd >= 0)
                rb->close(fd);
            fd = rb->open(OW_LIVE_TEMP, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd < 0)
                break;
            received = 0;
            crc = 0xffffffff;
            ow_browser_ack(live_request_id, 0);
        }
        else if (packet[4] == 3 && length > 13 && fd >= 0)
        {
            uint32_t offset = ow_get_be32(packet + 9);
            int payload = length - 13;
            if (offset < received && offset + payload <= received)
            {
                ow_browser_ack(live_request_id, received);
                continue;
            }
            if (offset != received || received + payload > expected_size ||
                rb->write(fd, packet + 13, payload) != payload)
                break;
            crc = rb->crc_32(packet + 13, payload, crc);
            received += payload;
            ow_browser_ack(live_request_id, received);
        }
        else if (packet[4] == 4 && fd >= 0)
        {
            bool valid = received == expected_size &&
                         (!expected_crc || crc == expected_crc);
            rb->close(fd);
            fd = -1;
            if (valid && ow_install_live_bundle())
            {
                ow_browser_ack(live_request_id, 0xffffffff);
                return true;
            }
            break;
        }
        else if (packet[4] == 6)
        {
            packet[MIN(length, OW_BROWSER_PACKET_MAX - 1)] = '\0';
            rb->splashf(HZ * 2, "Web error: %s", packet + 9);
            break;
        }
    }
    if (fd >= 0)
        rb->close(fd);
    rb->remove(OW_LIVE_TEMP);
#else
    (void)request_type;
    (void)value;
#endif
    return false;
}

static bool ow_fetch_live_page(const char *url)
{
    return ow_fetch_live_request(1, url);
}

static bool ow_remote_command(const char *command)
{
    return ow_fetch_live_request(8, command);
}

static void ow_live_transfer_reset(void)
{
    if (live_transfer.fd >= 0)
        rb->close(live_transfer.fd);
    live_transfer.fd = -1;
    live_transfer.active = false;
    rb->remove(OW_LIVE_TEMP);
}

static void ow_live_scroll_cancel(void)
{
    ow_live_transfer_reset();
    live_visual_scroll = 0;
    live_queued_scroll = 0;
}

/* Start a viewport refresh without blocking the click-wheel/UI thread. */
static bool ow_live_command_begin(int scroll)
{
#if defined(USB_ENABLE_ETHERNET)
    unsigned char packet[64];
    char command[32];
    int length;

    if (live_transfer.active || !ow_online_connected() || !scroll)
        return false;
    live_request_id++;
    if (!live_request_id)
        live_request_id = 1;
    rb->snprintf(command, sizeof(command), "scroll:%d", scroll);
    rb->memcpy(packet, "RPB1", 4);
    packet[4] = 8;
    ow_put_be32(packet + 5, live_request_id);
    rb->strlcpy((char *)packet + 9, command, sizeof(packet) - 9);
    length = 9 + rb->strlen((char *)packet + 9);
    if (!ow_browser_send(packet, length, HZ / 4))
        return false;

    rb->memset(&live_transfer, 0, sizeof(live_transfer));
    live_transfer.active = true;
    live_transfer.request_id = live_request_id;
    live_transfer.crc = 0xffffffff;
    live_transfer.deadline = *rb->current_tick + 25 * HZ;
    live_transfer.fd = -1;
    rb->remove(OW_LIVE_TEMP);
    return true;
#else
    (void)scroll;
    return false;
#endif
}

/* Returns 1 for a newly installed viewport, 0 while pending, and -1 on error. */
static int ow_live_command_service(void)
{
#if defined(USB_ENABLE_ETHERNET)
    unsigned char *packet = live_packet;
    int length;

    if (!live_transfer.active)
        return 0;
    if (!ow_online_connected() ||
        !TIME_BEFORE(*rb->current_tick, live_transfer.deadline))
    {
        ow_live_transfer_reset();
        return -1;
    }
    rb->usb_internet_service();
    while ((length = rb->usb_internet_receive(OW_BROWSER_PORT, packet,
                                               OW_BROWSER_PACKET_MAX)) > 0)
    {
        if (length < 9 || rb->memcmp(packet, "RPB1", 4) ||
            ow_get_be32(packet + 5) != live_transfer.request_id)
            continue;
        if (packet[4] == 2 && length == 17)
        {
            live_transfer.expected_size = ow_get_be32(packet + 9);
            live_transfer.expected_crc = ow_get_be32(packet + 13);
            if (!live_transfer.expected_size ||
                live_transfer.expected_size > OW_BROWSER_MAX)
            {
                ow_live_transfer_reset();
                return -1;
            }
            if (live_transfer.fd >= 0)
                rb->close(live_transfer.fd);
            live_transfer.fd = rb->open(OW_LIVE_TEMP,
                O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (live_transfer.fd < 0)
            {
                ow_live_transfer_reset();
                return -1;
            }
            live_transfer.received = 0;
            live_transfer.crc = 0xffffffff;
            ow_browser_ack(live_transfer.request_id, 0);
        }
        else if (packet[4] == 3 && length > 13 && live_transfer.fd >= 0)
        {
            uint32_t offset = ow_get_be32(packet + 9);
            int payload = length - 13;
            if (offset < live_transfer.received &&
                offset + payload <= live_transfer.received)
            {
                ow_browser_ack(live_transfer.request_id,
                               live_transfer.received);
                continue;
            }
            if (offset != live_transfer.received ||
                live_transfer.received + payload >
                    live_transfer.expected_size ||
                rb->write(live_transfer.fd, packet + 13, payload) != payload)
            {
                ow_live_transfer_reset();
                return -1;
            }
            live_transfer.crc = rb->crc_32(packet + 13, payload,
                                           live_transfer.crc);
            live_transfer.received += payload;
            ow_browser_ack(live_transfer.request_id, live_transfer.received);
        }
        else if (packet[4] == 4 && live_transfer.fd >= 0)
        {
            bool valid = live_transfer.received ==
                         live_transfer.expected_size &&
                         (!live_transfer.expected_crc ||
                          live_transfer.crc == live_transfer.expected_crc);
            rb->close(live_transfer.fd);
            live_transfer.fd = -1;
            if (valid && ow_install_live_bundle())
            {
                ow_browser_ack(live_transfer.request_id, 0xffffffff);
                live_transfer.active = false;
                return 1;
            }
            ow_live_transfer_reset();
            return -1;
        }
        else if (packet[4] == 6)
        {
            ow_live_transfer_reset();
            return -1;
        }
    }
#endif
    return 0;
}

#ifdef HAVE_LCD_COLOR
static bool ow_dark(void)
{
    return rb->global_settings && rb->global_settings->ui_engine_dark_mode;
}

static unsigned ow_color_screen(void)
{
    return ow_dark() ? OW_IPODJS_DARK_BG : OW_IPODJS_SCREEN_BG;
}

static unsigned ow_color_panel(void)
{
    return ow_dark() ? OW_IPODJS_DARK_PANEL : OW_IPODJS_SCREEN_BG;
}

static unsigned ow_color_header(void)
{
    return ow_dark() ? OW_IPODJS_HEADER_DARK : OW_IPODJS_HEADER_BOTTOM;
}

static unsigned ow_color_split(void)
{
    return ow_dark() ? OW_IPODJS_HEADER_DARK_LINE : OW_IPODJS_SPLIT;
}

static unsigned ow_color_text(void)
{
    return ow_dark() ? OW_IPODJS_DARK_TEXT : OW_IPODJS_TEXT;
}

static unsigned ow_color_muted(void)
{
    return ow_dark() ? OW_IPODJS_DARK_MUTED : OW_IPODJS_MUTED_TEXT;
}

static unsigned ow_color_selected(void);

static unsigned ow_color_link(void)
{
    return ow_color_selected();
}

static unsigned ow_color_selected(void)
{
    if (rb->global_settings == NULL)
        return OW_IPODJS_ACTIVE_BOTTOM;

    switch (rb->global_settings->ui_engine_accent)
    {
        case UI_ENGINE_ACCENT_GRAPHITE: return LCD_RGBPACK(84, 90, 100);
        case UI_ENGINE_ACCENT_U2: return LCD_RGBPACK(182, 24, 35);
        case UI_ENGINE_ACCENT_TEAL: return LCD_RGBPACK(0, 128, 132);
        case UI_ENGINE_ACCENT_GREEN: return LCD_RGBPACK(55, 142, 64);
        case UI_ENGINE_ACCENT_GOLD: return LCD_RGBPACK(184, 135, 38);
        case UI_ENGINE_ACCENT_ORANGE: return LCD_RGBPACK(208, 104, 32);
        case UI_ENGINE_ACCENT_PURPLE: return LCD_RGBPACK(113, 82, 170);
        case UI_ENGINE_ACCENT_PINK: return LCD_RGBPACK(195, 72, 128);
        case UI_ENGINE_ACCENT_BLUE:
        default: return OW_IPODJS_ACTIVE_BOTTOM;
    }
}

static unsigned ow_color_selected_top(void)
{
    unsigned color = ow_color_selected();
    int r = RGB_UNPACK_RED(color);
    int g = RGB_UNPACK_GREEN(color);
    int b = RGB_UNPACK_BLUE(color);

    return LCD_RGBPACK((r * 143 + 255 * 112) / 255,
                       (g * 143 + 255 * 112) / 255,
                       (b * 143 + 255 * 112) / 255);
}
#endif

static void ow_mkdirs(void)
{
    rb->mkdir(OW_ROOT);
    rb->mkdir(OW_ROOT "/geocities");
    rb->mkdir(OW_ROOT "/angelfire");
    rb->mkdir(OW_ROOT "/tripod");
    rb->mkdir(OW_ROOT "/yahoo");
    rb->mkdir(OW_ROOT "/myspace");
    rb->mkdir(OW_ROOT "/archive");
    rb->mkdir(OW_ROOT "/images");
    rb->mkdir(OW_ROOT "/gifs");
    rb->mkdir(OW_ROOT "/midi");
    rb->mkdir(OW_ROOT "/cache");
    rb->mkdir(OW_ROOT "/assets");
}

static void ow_chomp(char *s)
{
    int len = rb->strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
        s[--len] = '\0';
}

static char *ow_field(char **line)
{
    char *start = *line;
    char *tab = rb->strchr(start, '\t');
    if (tab)
    {
        *tab = '\0';
        *line = tab + 1;
    }
    else
    {
        *line = start + rb->strlen(start);
    }
    return start;
}

static void ow_load_pages(void)
{
    int fd;
    char line[512];

    page_count = 0;
    fd = rb->open(OW_PAGES, O_RDONLY);
    if (fd < 0)
        return;

    while (page_count < OW_MAX_PAGES &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *p = line;
        struct ow_page *page;

        ow_chomp(line);
        if (line[0] == '#' || line[0] == '\0')
            continue;

        page = &pages[page_count];
        rb->strlcpy(page->title, ow_field(&p), sizeof(page->title));
        rb->strlcpy(page->url, ow_field(&p), sizeof(page->url));
        rb->strlcpy(page->path, ow_field(&p), sizeof(page->path));
        rb->strlcpy(page->source, ow_field(&p), sizeof(page->source));
        rb->strlcpy(page->neighborhood, ow_field(&p),
                    sizeof(page->neighborhood));
        rb->strlcpy(page->author, ow_field(&p), sizeof(page->author));
        rb->strlcpy(page->archived, ow_field(&p), sizeof(page->archived));
        rb->strlcpy(page->keywords, ow_field(&p), sizeof(page->keywords));
        page_count++;
    }
    rb->close(fd);
}

static int ow_find_page_by_path(const char *path)
{
    int i;
    for (i = 0; i < page_count; i++)
        if (!rb->strcmp(pages[i].path, path))
            return i;
    return -1;
}

static bool ow_is_shortcuts_path(const char *path)
{
    return path && !rb->strcmp(path, OW_SHORTCUTS);
}

static bool ow_is_youtube_list_path(const char *path)
{
    return path &&
        (rb->strstr(path, "/www.youtube.com/subscriptions.html") ||
         rb->strstr(path, "/www.youtube.com/channel-"));
}

static bool ow_is_social_path(const char *path)
{
    return path &&
        (rb->strstr(path, "/www.instagram.com/") ||
         rb->strstr(path, "/instagram.com/") ||
         rb->strstr(path, "/onlyfans.com/"));
}

static bool ow_is_social_list_path(const char *path)
{
    return ow_is_social_path(path) &&
           rb->strstr(path, "/profiles.html");
}

static const char *ow_social_profiles_path(const char *path)
{
    if (path && rb->strstr(path, "instagram.com/"))
        return OW_INSTAGRAM_PROFILES;
    if (path && rb->strstr(path, "onlyfans.com/"))
        return OW_ONLYFANS_PROFILES;
    return NULL;
}

static bool ow_uses_link_wheel(const char *path)
{
    return ow_is_shortcuts_path(path) || ow_is_youtube_list_path(path) ||
           ow_is_social_list_path(path);
}

static bool ow_has_ext(const char *path, const char *exts)
{
    const char *dot = rb->strrchr(path, '.');
    return dot && rb->strcasestr(exts, dot) != NULL;
}

static void ow_parent_dir(const char *path, char *dir, size_t size)
{
    char *slash;
    rb->strlcpy(dir, path, size);
    slash = rb->strrchr(dir, '/');
    if (slash && slash != dir)
        *slash = '\0';
    else
        rb->strlcpy(dir, "/", size);
}

static int ow_open_candidate(const char *path, char *resolved, size_t size)
{
    int fd;

    if (!path || !path[0])
        return -1;

    if (resolved && size > 0)
        rb->strlcpy(resolved, path, size);

    fd = rb->open_utf8(path, O_RDONLY);
    if (fd >= 0)
        return fd;

    return rb->open(path, O_RDONLY);
}

static int ow_url_to_archive_path(const char *url, char *out, size_t size)
{
    const char *host;
    const char *path;
    const char *host_end;
    const char *path_end;
    const char *mark;
    int scheme_len = 0;
    int host_len;
    int path_len;

    if (!url || !out || size <= 0)
        return -1;

    if (!rb->strncasecmp(url, "http://", 7))
        scheme_len = 7;
    else if (!rb->strncasecmp(url, "https://", 8))
        scheme_len = 8;
    else
        return -1;

    host = url + scheme_len;
    path = rb->strchr(host, '/');
    host_end = path ? path : host + rb->strlen(host);
    if (!path)
        path = "/index.html";

    host_len = host_end - host;
    if (host_len <= 0 || host_len >= 96)
        return -1;

    path_end = path + rb->strlen(path);
    mark = rb->strchr(path, '?');
    if (mark && mark < path_end)
        path_end = mark;
    mark = rb->strchr(path, '#');
    if (mark && mark < path_end)
        path_end = mark;
    path_len = path_end - path;
    if (path_len <= 0 || (path_len == 1 && path[0] == '/'))
    {
        path = "/index.html";
        path_len = rb->strlen(path);
    }

    rb->snprintf(out, size, OW_ROOT "/archive/%.*s%.*s",
                 host_len, host, path_len, path);
    return 0;
}

static int ow_open_read_resolved(const char *path, char *resolved, size_t size)
{
    char candidate[MAX_PATH];
    int fd;

    if (ow_url_to_archive_path(path, candidate, sizeof(candidate)) == 0)
    {
        fd = ow_open_candidate(candidate, resolved, size);
        if (fd >= 0)
            return fd;
        if (rb->strrchr(candidate, '.') == NULL)
        {
            rb->strlcat(candidate, ".html", sizeof(candidate));
            fd = ow_open_candidate(candidate, resolved, size);
            if (fd >= 0)
                return fd;
        }
    }

    fd = ow_open_candidate(path, resolved, size);
    if (fd >= 0)
        return fd;

    if (path && path[0] == '/' && path[1] != '\0')
    {
        fd = ow_open_candidate(path + 1, resolved, size);
        if (fd >= 0)
            return fd;
    }
    else if (path && path[0] != '/')
    {
        rb->snprintf(candidate, sizeof(candidate), "/%s", path);
        fd = ow_open_candidate(candidate, resolved, size);
        if (fd >= 0)
            return fd;
    }

    if (resolved && size > 0)
        rb->strlcpy(resolved, path ? path : "", size);
    return -1;
}

static bool ow_archive_host_root(const char *base, char *out, size_t size)
{
    const char *prefix = OW_ROOT "/archive/";
    const char *host;
    const char *slash;
    int host_len;

    if (!base || !out || size <= 0)
        return false;

    host = rb->strstr(base, prefix);
    if (!host)
        return false;
    host += rb->strlen(prefix);
    slash = rb->strchr(host, '/');
    if (!slash)
        return false;

    host_len = slash - host;
    if (host_len <= 0 || host_len >= MAX_PATH)
        return false;

    rb->snprintf(out, size, "%s%.*s", prefix, host_len, host);
    return true;
}

static void ow_strip_query_fragment(char *path)
{
    char *mark;

    if (!path)
        return;

    mark = rb->strchr(path, '#');
    if (mark)
        *mark = '\0';
    mark = rb->strchr(path, '?');
    if (mark)
        *mark = '\0';
}

static void ow_join_path(char *out, size_t size, const char *base,
                         const char *href)
{
    char dir[MAX_PATH];

    if (!href || !href[0])
    {
        out[0] = '\0';
        return;
    }

    if (!rb->strncasecmp(href, "file://", 7))
        href += 7;

    if (!rb->strncasecmp(href, "rockbox:", 8))
    {
        rb->strlcpy(out, href, size);
        return;
    }

    if (!rb->strncmp(href, OW_ROOT, rb->strlen(OW_ROOT)) &&
        (href[rb->strlen(OW_ROOT)] == '/' ||
         href[rb->strlen(OW_ROOT)] == '\0'))
    {
        rb->strlcpy(out, href, size);
        ow_strip_query_fragment(out);
        return;
    }

    if (href[0] == '/')
    {
        if (ow_archive_host_root(base, dir, sizeof(dir)))
            rb->snprintf(out, size, "%s%s", dir, href);
        else
            rb->strlcpy(out, href, size);
        ow_strip_query_fragment(out);
        return;
    }

    if (!rb->strncasecmp(href, "http://", 7) ||
        !rb->strncasecmp(href, "https://", 8))
    {
        int i;
        for (i = 0; i < page_count; i++)
        {
            if (!rb->strcasecmp(pages[i].url, href))
            {
                rb->strlcpy(out, pages[i].path, size);
                return;
            }
        }
        rb->strlcpy(out, href, size);
        ow_strip_query_fragment(out);
        return;
    }

    ow_parent_dir(base, dir, sizeof(dir));
    rb->snprintf(out, size, "%s/%s", dir, href);
    ow_strip_query_fragment(out);
}

static void ow_log_path(const char *file, const char *path)
{
    int page = ow_find_page_by_path(path);
    int fd;

    if (ow_is_shortcuts_path(path))
        return;

    fd = rb->open(file, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
        return;

    if (page >= 0)
        rb->fdprintf(fd, "%s\t%s\t%s\n", pages[page].title,
                     pages[page].url, pages[page].path);
    else
        rb->fdprintf(fd, "%s\t%s\t%s\n", path, path, path);
    rb->close(fd);
}

static int ow_chars_per_line(void)
{
    int w = 6;
    int h;
    int chars;

    rb->lcd_getstringsize("M", &w, &h);
    if (w <= 0)
        w = 6;
    chars = (LCD_WIDTH - OW_MARGIN_X * 2 - OW_SCROLLBAR_W) / w;
    if (browser_zoom > 0)
        chars = chars * 100 / (100 + browser_zoom * 30);
    if (chars < 12)
        chars = 12;
    if (chars >= OW_LINE_LEN)
        chars = OW_LINE_LEN - 1;
    return chars;
}

static void ow_add_line_styled(const char *text, unsigned char style, int link)
{
    struct ow_render_line *line;

    if (render.line_count >= OW_MAX_LINES)
        return;

    line = &render.lines[render.line_count++];
    rb->strlcpy(line->text, text ? text : "", sizeof(line->text));
    line->image_path[0] = '\0';
    line->image_height = 0;
    line->style = style;
    line->link = link;
}

static void ow_add_image(const char *path, const char *label, int link,
                         int height)
{
    struct ow_render_line *line;

    if (render.line_count >= OW_MAX_LINES)
        return;

    line = &render.lines[render.line_count++];
    rb->snprintf(line->text, sizeof(line->text), "%s",
                 label && label[0] ? label : ow_basename(path));
    rb->strlcpy(line->image_path, path ? path : "", sizeof(line->image_path));
    line->image_height = MIN(MAX(height, 36), OW_IMAGE_MAX_H);
    line->style = OW_STYLE_IMAGE;
    line->link = link;
}

static void ow_add_media(const char *path, const char *label, int link)
{
    struct ow_render_line *line;

    if (render.line_count >= OW_MAX_LINES)
        return;

    line = &render.lines[render.line_count++];
    rb->snprintf(line->text, sizeof(line->text), "%s",
                 label && label[0] ? label : ow_basename(path));
    rb->strlcpy(line->image_path, path ? path : "", sizeof(line->image_path));
    line->image_height = 0;
    line->style = OW_STYLE_MEDIA;
    line->link = link;
}

static void ow_add_line(const char *text)
{
    ow_add_line_styled(text, OW_STYLE_TEXT, -1);
}

static void ow_add_blank(void)
{
    if (render.line_count > 0 &&
        render.lines[render.line_count - 1].text[0] == '\0')
    {
        return;
    }
    ow_add_line("");
}

static void ow_ensure_text_line(unsigned char style, int link)
{
    if (render.line_count <= 0)
    {
        ow_add_line_styled("", style, link);
        return;
    }

    if (render.lines[render.line_count - 1].style != style ||
        render.lines[render.line_count - 1].link != link)
    {
        if (render.lines[render.line_count - 1].text[0] != '\0')
            ow_add_line_styled("", style, link);
    }
}

static char ow_entity_char(const char **p)
{
    const char *s = *p;

    if (!rb->strncasecmp(s, "&amp;", 5))
    {
        *p += 5;
        return '&';
    }
    if (!rb->strncasecmp(s, "&lt;", 4))
    {
        *p += 4;
        return '<';
    }
    if (!rb->strncasecmp(s, "&gt;", 4))
    {
        *p += 4;
        return '>';
    }
    if (!rb->strncasecmp(s, "&quot;", 6))
    {
        *p += 6;
        return '"';
    }
    if (!rb->strncasecmp(s, "&#39;", 5) ||
        !rb->strncasecmp(s, "&apos;", 6))
    {
        *p += (s[1] == '#') ? 5 : 6;
        return '\'';
    }

    (*p)++;
    return *s;
}

static void ow_append_word(const char *word, int len, int *col,
                           unsigned char style, int link)
{
    int max_cols = ow_chars_per_line();
    char chunk[OW_LINE_LEN];

    if (len <= 0 || render.line_count >= OW_MAX_LINES)
        return;

    while (len > 0)
    {
        int take;
        struct ow_render_line *line;

        ow_ensure_text_line(style, link);
        line = &render.lines[render.line_count - 1];

        if (*col > 0 && *col + len + 1 > max_cols)
        {
            ow_add_line_styled("", style, link);
            *col = 0;
            continue;
        }

        if (*col > 0)
        {
            rb->strlcat(line->text, " ", sizeof(line->text));
            (*col)++;
        }

        take = MIN(len, max_cols - *col);
        if (take <= 0)
        {
            ow_add_line_styled("", style, link);
            *col = 0;
            continue;
        }

        rb->memcpy(chunk, word, take);
        chunk[take] = '\0';
        rb->strlcat(line->text, chunk, sizeof(line->text));
        *col += take;
        word += take;
        len -= take;

        if (len > 0)
        {
            ow_add_line_styled("", style, link);
            *col = 0;
        }
    }
}

static void ow_add_text(const char *text, int *col,
                        unsigned char style, int link)
{
    const char *p = text;
    char word[OW_LINE_LEN];
    int len = 0;

    while (*p)
    {
        char c;

        if (*p == '<')
            break;

        if (*p == '&')
            c = ow_entity_char(&p);
        else
            c = *p++;

        if (c == '\r' || c == '\n' || c == '\t' || c == ' ')
        {
            ow_append_word(word, len, col, style, link);
            len = 0;
            continue;
        }

        if (len < (int)sizeof(word) - 1)
            word[len++] = c;
        else
        {
            ow_append_word(word, len, col, style, link);
            len = 0;
            word[len++] = c;
        }
    }

    ow_append_word(word, len, col, style, link);
}

static void ow_attr_value(const char *tag, const char *attr,
                          char *out, size_t size)
{
    char *p = rb->strcasestr(tag, attr);
    out[0] = '\0';
    if (!p)
        return;
    p += rb->strlen(attr);
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p != '=')
        return;
    p++;
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p == '"' || *p == '\'')
    {
        char q = *p++;
        int n = 0;
        while (*p && *p != q && n < (int)size - 1)
            out[n++] = *p++;
        out[n] = '\0';
    }
    else
    {
        int n = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '>' &&
               n < (int)size - 1)
            out[n++] = *p++;
        out[n] = '\0';
    }
}

static int ow_attr_int(const char *tag, const char *attr, int fallback)
{
    char value[24];
    int out = 0;
    int i = 0;

    ow_attr_value(tag, attr, value, sizeof(value));
    while (value[i] >= '0' && value[i] <= '9')
    {
        out = out * 10 + value[i] - '0';
        i++;
    }
    return out > 0 ? out : fallback;
}

static bool ow_resolve_image_path(const char *path, char *resolved,
                                  size_t size)
{
    int fd;
    char candidate[MAX_PATH];

    if (path && !ow_has_ext(path, ".bmp"))
    {
        rb->snprintf(candidate, sizeof(candidate), "%s.bmp", path);
        fd = ow_open_read_resolved(candidate, resolved, size);
        if (fd >= 0)
        {
            rb->close(fd);
            return true;
        }
    }

    fd = ow_open_read_resolved(path, resolved, size);
    if (fd >= 0)
    {
        rb->close(fd);
        if (ow_has_ext(resolved, ".bmp.jpg.jpeg"))
            return true;
    }

    rb->strlcpy(resolved, path ? path : "", size);
    return false;
}

static int ow_add_link(const char *base, const char *tag, const char *label)
{
    char href[MAX_PATH];
    char path[MAX_PATH];
    struct ow_link *link;

    if (render.link_count >= OW_MAX_LINKS)
        return -1;
    ow_attr_value(tag, "href", href, sizeof(href));
    if (!href[0])
        ow_attr_value(tag, "src", href, sizeof(href));
    if (!href[0])
        return -1;

    ow_join_path(path, sizeof(path), base, href);
    link = &render.links[render.link_count];
    rb->strlcpy(link->target, path, sizeof(link->target));
    if (label && label[0])
        rb->strlcpy(link->label, label, sizeof(link->label));
    else
        rb->strlcpy(link->label, href, sizeof(link->label));
    render.link_count++;
    return render.link_count - 1;
}

static int ow_add_direct_link(const char *target, const char *label)
{
    struct ow_link *link;

    if (render.link_count >= OW_MAX_LINKS)
        return -1;

    link = &render.links[render.link_count];
    rb->strlcpy(link->target, target ? target : "", sizeof(link->target));
    rb->strlcpy(link->label, label && label[0] ? label : link->target,
                sizeof(link->label));
    render.link_count++;
    return render.link_count - 1;
}

static const char *ow_basename(const char *path)
{
    const char *slash = rb->strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void ow_render_shortcuts(void)
{
    int i;

    rb->memset(&render, 0, sizeof(render));

    if (page_count <= 0)
    {
        ow_add_line("No offline websites found.");
        ow_add_line("Use RockPod Website Sync to add pages.");
        return;
    }

    for (i = 0; i < page_count && render.line_count < OW_MAX_LINES - 2; i++)
    {
        char meta[OW_LINE_LEN];
        int link = ow_add_direct_link(pages[i].path, pages[i].title);

        if (link < 0)
            break;

        ow_add_line_styled(pages[i].title, OW_STYLE_LINK, link);
        meta[0] = '\0';
        if (pages[i].source[0])
            rb->strlcpy(meta, pages[i].source, sizeof(meta));
        if (pages[i].archived[0])
        {
            if (meta[0])
                rb->strlcat(meta, " - ", sizeof(meta));
            rb->strlcat(meta, pages[i].archived, sizeof(meta));
        }
        if (!meta[0] && pages[i].neighborhood[0])
            rb->strlcpy(meta, pages[i].neighborhood, sizeof(meta));
        if (!meta[0])
            rb->strlcpy(meta, pages[i].url, sizeof(meta));
        ow_add_line_styled(meta, OW_STYLE_META, link);
        if (i + 1 < page_count)
            ow_add_line_styled("", OW_STYLE_RULE, -1);
    }
}

static void ow_render_google(void)
{
    int link;

    rb->memset(&render, 0, sizeof(render));
    ow_add_blank();
    ow_add_blank();
    ow_add_line_styled("Google", OW_STYLE_HEADING, -1);
    ow_add_line("Search the web from your iPod");
    ow_add_blank();
    link = ow_add_direct_link(OW_GOOGLE_SEARCH, "Google Search");
    ow_add_line_styled("Google Search", OW_STYLE_LINK, link);
    ow_add_line_styled("Select, then use the Click Wheel keyboard",
                       OW_STYLE_META, link);
}

static void ow_render_html(const char *path)
{
    int fd;
    char line[512];
    char resolved[MAX_PATH];
    int col = 0;
    int active_link = -1;
    unsigned char active_style = OW_STYLE_TEXT;
    bool skip_content = false;
    bool skip_head = false;
    bool skip_tag_continuation = false;
    int page = ow_find_page_by_path(path);

    rb->memset(&render, 0, sizeof(render));
    fd = ow_open_read_resolved(path, resolved, sizeof(resolved));
    if (fd < 0)
    {
        ow_add_line("Cannot open page");
        ow_add_line(path);
        return;
    }

    if (page >= 0 &&
        !rb->strcasestr(pages[page].url, "instagram.com") &&
        !rb->strcasestr(pages[page].url, "onlyfans.com") &&
        !rb->strcasestr(pages[page].url, "youtube.com") &&
        !rb->strcasestr(pages[page].url, "youtu.be"))
    {
        char meta[OW_LINE_LEN];

        if (pages[page].url[0])
            ow_add_line_styled(pages[page].url, OW_STYLE_META, -1);
        meta[0] = '\0';
        if (pages[page].source[0])
            rb->strlcpy(meta, pages[page].source, sizeof(meta));
        if (pages[page].author[0])
        {
            if (meta[0])
                rb->strlcat(meta, " - ", sizeof(meta));
            rb->strlcat(meta, pages[page].author, sizeof(meta));
        }
        if (pages[page].archived[0])
        {
            if (meta[0])
                rb->strlcat(meta, " - ", sizeof(meta));
            rb->strlcat(meta, pages[page].archived, sizeof(meta));
        }
        if (meta[0])
            ow_add_line_styled(meta, OW_STYLE_META, -1);
        ow_add_line_styled("", OW_STYLE_RULE, -1);
    }

    while (rb->read_line(fd, line, sizeof(line)) > 0 &&
           render.line_count < OW_MAX_LINES - 2)
    {
        char *p = line;
        while (*p)
        {
            char *tag = rb->strchr(p, '<');
            if (skip_tag_continuation)
            {
                tag = rb->strchr(p, '>');
                if (!tag)
                    break;
                p = tag + 1;
                skip_tag_continuation = false;
                continue;
            }
            if (!tag)
            {
                if (!skip_content && !skip_head)
                    ow_add_text(p, &col, active_style, active_link);
                break;
            }
            *tag = '\0';
            if (!skip_content && !skip_head)
                ow_add_text(p, &col, active_style, active_link);
            p = tag + 1;
            tag = rb->strchr(p, '>');
            if (!tag)
            {
                skip_tag_continuation = true;
                break;
            }
            *tag = '\0';
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
                p++;

            if (!rb->strncasecmp(p, "script", 6) ||
                !rb->strncasecmp(p, "style", 5))
            {
                skip_content = true;
            }
            else if (!rb->strncasecmp(p, "/script", 7) ||
                     !rb->strncasecmp(p, "/style", 6))
            {
                skip_content = false;
            }
            else if (!rb->strncasecmp(p, "head", 4))
            {
                skip_head = true;
            }
            else if (!rb->strncasecmp(p, "/head", 5))
            {
                skip_head = false;
            }
            else if (!skip_content && !skip_head)
            {
                if (!rb->strncasecmp(p, "youtube2007 ", 12))
                {
                    char value[MAX_PATH];

                    youtube_page.active = true;
                    ow_attr_value(p, "video", value, sizeof(value));
                    ow_join_path(youtube_page.video_path,
                                 sizeof(youtube_page.video_path),
                                 resolved, value);
                    ow_attr_value(p, "frame-file", value, sizeof(value));
                    ow_join_path(youtube_page.frame_path,
                                 sizeof(youtube_page.frame_path),
                                 resolved, value);
                    ow_attr_value(p, "title", youtube_page.title,
                                  sizeof(youtube_page.title));
                    ow_attr_value(p, "uploader", youtube_page.uploader,
                                  sizeof(youtube_page.uploader));
                    ow_attr_value(p, "added", youtube_page.added,
                                  sizeof(youtube_page.added));
                    ow_attr_value(p, "duration", youtube_page.duration,
                                  sizeof(youtube_page.duration));
                    ow_attr_value(p, "views", youtube_page.views,
                                  sizeof(youtube_page.views));
                    ow_attr_value(p, "description",
                                  youtube_page.description,
                                  sizeof(youtube_page.description));
                    youtube_page.frame_count =
                        ow_attr_int(p, "frame-count", 1);
                    ow_add_direct_link(youtube_page.video_path,
                                       youtube_page.title);
                    col = 0;
                }
                else if (!rb->strncasecmp(p, "youtubelink ", 12))
                {
                    char href[MAX_PATH];
                    char label[OW_FIELD_LEN];
                    char target[MAX_PATH];
                    int link;

                    ow_attr_value(p, "href", href, sizeof(href));
                    ow_attr_value(p, "label", label, sizeof(label));
                    ow_join_path(target, sizeof(target), resolved, href);
                    link = ow_add_direct_link(target, label);
                    ow_add_line_styled(label, OW_STYLE_HEADING, link);
                    col = 0;
                }
                else if (!rb->strncasecmp(p, "br", 2) ||
                    !rb->strncasecmp(p, "p", 1) ||
                    !rb->strncasecmp(p, "/p", 2) ||
                    !rb->strncasecmp(p, "div", 3) ||
                    !rb->strncasecmp(p, "/div", 4) ||
                    !rb->strncasecmp(p, "tr", 2))
                {
                    ow_add_blank();
                    col = 0;
                }
                else if (!rb->strncasecmp(p, "hr", 2))
                {
                    ow_add_line_styled("----------------------------------------",
                                       OW_STYLE_RULE, -1);
                    col = 0;
                }
                else if (!rb->strncasecmp(p, "li", 2))
                {
                    ow_add_blank();
                    col = 0;
                    ow_add_text("*", &col, OW_STYLE_TEXT, -1);
                }
                else if (!rb->strncasecmp(p, "h1", 2) ||
                         !rb->strncasecmp(p, "h2", 2) ||
                         !rb->strncasecmp(p, "h3", 2) ||
                         !rb->strncasecmp(p, "h4", 2) ||
                         !rb->strncasecmp(p, "h5", 2) ||
                         !rb->strncasecmp(p, "h6", 2))
                {
                    ow_add_blank();
                    active_style = OW_STYLE_HEADING;
                    col = 0;
                }
                else if (!rb->strncasecmp(p, "/h", 2))
                {
                    active_style = OW_STYLE_TEXT;
                    ow_add_blank();
                    col = 0;
                }
                else if (!rb->strncasecmp(p, "a ", 2))
                {
                    active_link = ow_add_link(resolved, p, "");
                    col = 0;
                }
                else if (!rb->strncasecmp(p, "/a", 2))
                {
                    active_link = -1;
                    col = 0;
                }
                else if (!rb->strncasecmp(p, "img ", 4))
                {
                    char alt[OW_FIELD_LEN];
                    char src[MAX_PATH];
                    char image_path[MAX_PATH];
                    int width;
                    int height;
                    int link;

                    alt[0] = '\0';
                    ow_attr_value(p, "alt", alt, sizeof(alt));
                    if (!alt[0])
                        ow_attr_value(p, "title", alt, sizeof(alt));
                    width = ow_attr_int(p, "width", LCD_WIDTH - OW_MARGIN_X * 2);
                    height = ow_attr_int(p, "height", 72);
                    if (width > 0 && width > LCD_WIDTH - OW_MARGIN_X * 2)
                        height = height * (LCD_WIDTH - OW_MARGIN_X * 2) / width;
                    src[0] = '\0';
                    image_path[0] = '\0';
                    ow_attr_value(p, "src", src, sizeof(src));
                    if (src[0])
                        ow_join_path(image_path, sizeof(image_path),
                                     resolved, src);
                    if (ow_attr_int(p, "data-static", 0))
                        link = -1;
                    else if (active_link >= 0)
                        link = active_link;
                    else
                        link = ow_add_link(resolved, p, alt);
                    if (image_path[0])
                    {
                        ow_add_image(image_path,
                                     alt[0] ? alt :
                                     ow_basename(image_path),
                                     link, height);
                        col = 0;
                    }
                }
                else if (!rb->strncasecmp(p, "video ", 6) ||
                         !rb->strncasecmp(p, "audio ", 6) ||
                         !rb->strncasecmp(p, "source ", 7) ||
                         !rb->strncasecmp(p, "embed ", 6) ||
                         !rb->strncasecmp(p, "bgsound ", 8) ||
                         !rb->strncasecmp(p, "object ", 7))
                {
                    char label[OW_FIELD_LEN];
                    int link;

                    label[0] = '\0';
                    ow_attr_value(p, "title", label, sizeof(label));
                    if (!label[0])
                        ow_attr_value(p, "alt", label, sizeof(label));
                    link = ow_add_link(resolved, p, label);
                    if (link >= 0)
                    {
                        ow_add_media(render.links[link].target,
                                     label[0] ? label :
                                     ow_basename(render.links[link].target),
                                     link);
                        col = 0;
                    }
                }
            }
            p = tag + 1;
        }
    }
    rb->close(fd);

    if (render.line_count <= 0)
        ow_add_line("(blank page)");
}

static void ow_render_path(const char *path)
{
    if (youtube_frame_fd >= 0)
        rb->close(youtube_frame_fd);
    youtube_frame_fd = -1;
    rb->memset(&youtube_page, 0, sizeof(youtube_page));
    if (online_mode && !rb->strcmp(path, OW_GOOGLE))
        ow_render_google();
    else if (online_mode &&
             (!rb->strncasecmp(path, "http://", 7) ||
              !rb->strncasecmp(path, "https://", 8)))
        ow_render_html(OW_LIVE_PAGE);
    else if (ow_is_shortcuts_path(path))
        ow_render_shortcuts();
    else
        ow_render_html(path);
}

static int ow_open_path(const char *path, bool push_back);

static const char *ow_title_for_path(const char *path)
{
    int page = ow_find_page_by_path(path);

    if (online_mode && !rb->strcmp(path, OW_GOOGLE))
        return "Google";
    if (online_mode &&
        (!rb->strncasecmp(path, "http://", 7) ||
         !rb->strncasecmp(path, "https://", 8)))
        return "Safari";
    if (ow_is_shortcuts_path(path))
        return "Offline Websites";
    if (page >= 0 && pages[page].title[0])
        return pages[page].title;
    if (!rb->strcmp(path, OW_INDEX))
        return "RockSearch Offline Internet";
    return ow_basename(path);
}

static int ow_line_height(void)
{
    int w;
    int h;

    rb->lcd_getstringsize("Ag", &w, &h);
    if (h < 8)
        h = 8;
    return h + 2 + browser_zoom * 2;
}

static int ow_line_item_height(const struct ow_render_line *line)
{
    if (line && (line->style & OW_STYLE_IMAGE))
        return MIN(MAX(line->image_height, 24), OW_IMAGE_MAX_H);
    if (line && (line->style & OW_STYLE_MEDIA))
        return ow_line_height() * 2 + 6;
    return ow_line_height();
}

static int ow_content_height(void)
{
    int i;
    int height = 0;

    for (i = 0; i < render.line_count; i++)
        height += ow_line_item_height(&render.lines[i]) +
                  ((render.lines[i].style & OW_STYLE_IMAGE) ? 1 : 0);
    return height;
}

static int ow_line_top(int row)
{
    int i;
    int y = 0;

    for (i = 0; i < row && i < render.line_count; i++)
        y += ow_line_item_height(&render.lines[i]) +
             ((render.lines[i].style & OW_STYLE_IMAGE) ? 1 : 0);
    return y;
}

static void ow_clamp_browser_scroll(void)
{
    int viewport_h = LCD_HEIGHT - OW_TOP_H - OW_BOTTOM_H - 2;
    int max_scroll;

    max_scroll = MAX(0, ow_content_height() - viewport_h);

    if (browser_scroll < 0)
        browser_scroll = 0;
    if (browser_scroll > max_scroll)
        browser_scroll = max_scroll;
}

static int ow_first_visible_link(void)
{
    int i;
    int y = OW_TOP_H + 2 - browser_scroll;

    for (i = 0; i < render.line_count && y < LCD_HEIGHT - OW_BOTTOM_H; i++)
    {
        int item_h = ow_line_item_height(&render.lines[i]);

        if (render.lines[i].link >= 0 && y + item_h > OW_TOP_H + 2)
            return render.lines[i].link;
        y += item_h + ((render.lines[i].style & OW_STYLE_IMAGE) ? 1 : 0);
    }
    return -1;
}

static int ow_dominant_visible_link(void)
{
    int viewport_top = browser_scroll;
    int viewport_bottom = browser_scroll +
        LCD_HEIGHT - OW_TOP_H - OW_BOTTOM_H - 2;
    int best_link = -1;
    int best_visible = 0;
    int i;
    int top = 0;

    for (i = 0; i < render.line_count; i++)
    {
        int item_h = ow_line_item_height(&render.lines[i]);
        int bottom = top + item_h;
        int visible = MIN(bottom, viewport_bottom) -
                      MAX(top, viewport_top);

        if (render.lines[i].link >= 0 && visible > best_visible)
        {
            best_visible = visible;
            best_link = render.lines[i].link;
        }
        top = bottom +
              ((render.lines[i].style & OW_STYLE_IMAGE) ? 1 : 0);
        if (top >= viewport_bottom)
            break;
    }
    return best_link;
}

static void ow_pick_visible_link(void)
{
    int row = ow_line_for_link(browser_selected_link);
    int viewport_h = LCD_HEIGHT - OW_TOP_H - OW_BOTTOM_H - 2;

    if (row >= 0)
    {
        int top = ow_line_top(row);
        int bottom = top + ow_line_item_height(&render.lines[row]);

        if (bottom > browser_scroll && top < browser_scroll + viewport_h)
            return;
    }
    browser_selected_link = ow_first_visible_link();
}

static int ow_line_for_link(int link)
{
    int i;

    for (i = 0; i < render.line_count; i++)
        if (render.lines[i].link == link)
            return i;
    return -1;
}

static void ow_ensure_link_visible(int link)
{
    int row = ow_line_for_link(link);
    int viewport_h = LCD_HEIGHT - OW_TOP_H - OW_BOTTOM_H - 2;
    int top;
    int bottom;

    if (row < 0)
        return;

    top = ow_line_top(row);
    bottom = top + ow_line_item_height(&render.lines[row]);
    if (top < browser_scroll)
        browser_scroll = top;
    else if (bottom > browser_scroll + viewport_h)
        browser_scroll = bottom - viewport_h;
    ow_clamp_browser_scroll();
}

static void ow_move_selected_link(int delta)
{
    int link;

    if (render.link_count <= 0)
        return;

    link = browser_selected_link;
    if (link < 0)
        link = (delta >= 0) ? 0 : render.link_count - 1;
    else
        link += delta;

    if (link < 0)
        link = 0;
    if (link >= render.link_count)
        link = render.link_count - 1;

    browser_selected_link = link;
    ow_ensure_link_visible(link);
}

static void ow_draw_bar_text(int x, int y, int width, const char *text)
{
    char buf[OW_LINE_LEN];
    int w;
    int h;
    int len;

    rb->strlcpy(buf, text ? text : "", sizeof(buf));
    len = rb->strlen(buf);
    rb->lcd_getstringsize(buf, &w, &h);
    while (len > 1 && w > width)
    {
        buf[--len] = '\0';
        rb->lcd_getstringsize(buf, &w, &h);
    }
    rb->lcd_putsxy(x, y, buf);
}

#ifdef HAVE_LCD_COLOR
static void ow_fill_two_tone(int x, int y, int width, int height,
                             unsigned top, unsigned bottom)
{
    int upper = MAX(1, height / 2);

    rb->lcd_set_foreground(top);
    rb->lcd_fillrect(x, y, width, upper);
    rb->lcd_set_foreground(bottom);
    rb->lcd_fillrect(x, y + upper, width, height - upper);
}

static void ow_fill_round_rect(int x, int y, int width, int height,
                               unsigned color)
{
    if (width < 4 || height < 4)
        return;
    rb->lcd_set_foreground(color);
    rb->lcd_fillrect(x + 2, y, width - 4, height);
    rb->lcd_fillrect(x, y + 2, width, height - 4);
    rb->lcd_fillrect(x + 1, y + 1, width - 2, height - 2);
}

static void ow_draw_globe(int x, int y, bool selected)
{
    unsigned color = selected ? LCD_WHITE : ow_color_selected();

    rb->lcd_set_foreground(color);
    rb->lcd_hline(x + 3, x + 11, y);
    rb->lcd_hline(x + 1, x + 13, y + 2);
    rb->lcd_hline(x + 1, x + 13, y + 12);
    rb->lcd_hline(x + 3, x + 11, y + 14);
    rb->lcd_vline(x, y + 3, y + 11);
    rb->lcd_vline(x + 14, y + 3, y + 11);
    rb->lcd_hline(x + 1, x + 13, y + 7);
    rb->lcd_vline(x + 5, y + 1, y + 13);
    rb->lcd_vline(x + 9, y + 1, y + 13);
}

static void ow_draw_play_badge(int x, int y, bool selected)
{
    unsigned rim = selected ? LCD_WHITE : OW_IPODJS_ADDRESS_BORDER;
    unsigned face = selected ? ow_color_selected() : OW_IPODJS_ADDRESS_BG;
    int i;

    ow_fill_round_rect(x, y, 23, 23, rim);
    ow_fill_round_rect(x + 2, y + 2, 19, 19, face);
    rb->lcd_set_foreground(selected ? LCD_WHITE : ow_color_selected());
    for (i = 0; i < 8; i++)
        rb->lcd_vline(x + 8 + i / 2, y + 7 + i / 2, y + 15 - i / 2);
}

static void ow_draw_tab_label(int center, int y, const char *label,
                              unsigned color)
{
    int width;
    int height;

    rb->lcd_getstringsize(label, &width, &height);
    rb->lcd_set_foreground(color);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_putsxy(center - width / 2, y, label);
    rb->lcd_set_drawmode(DRMODE_SOLID);
}

static bool ow_draw_site_footer(const char *path)
{
    int page = ow_find_page_by_path(path);
    const char *url = page >= 0 ? pages[page].url : path;
    unsigned color;
    bool onlyfans;
    int y;

    onlyfans = rb->strcasestr(url, "onlyfans.com") != NULL;
    if (!onlyfans && !rb->strcasestr(url, "instagram.com"))
        return false;

    y = LCD_HEIGHT - OW_BOTTOM_H;
    rb->lcd_set_foreground(ow_dark() ? ow_color_panel() : LCD_WHITE);
    rb->lcd_fillrect(0, y, LCD_WIDTH, OW_BOTTOM_H);
    rb->lcd_set_foreground(ow_color_split());
    rb->lcd_hline(0, LCD_WIDTH - 1, y);
    color = onlyfans ? OW_ONLYFANS_NAV :
            (ow_dark() ? LCD_WHITE : OW_INSTAGRAM_NAV);
    ow_draw_tab_label(32, y + 3, "Home", color);
    ow_draw_tab_label(96, y + 3, onlyfans ? "Alerts" : "Search",
                      ow_color_muted());
    ow_draw_tab_label(160, y + 3, onlyfans ? "New" : "Post",
                      ow_color_muted());
    ow_draw_tab_label(224, y + 3, onlyfans ? "Messages" : "Activity",
                      ow_color_muted());
    ow_draw_tab_label(288, y + 3, "Profile", ow_color_muted());
    rb->lcd_set_foreground(color);
    rb->lcd_hline(12, 52, LCD_HEIGHT - 2);
    return true;
}
#endif

static bool ow_cache_image_line(int line_index, int width)
{
    struct bitmap bm;
    struct ow_render_line *line = &render.lines[line_index];
    char resolved[MAX_PATH];
    int rc = -1;
    int wanted_h = MIN(MAX(line->image_height, 24), OW_IMAGE_MAX_H);
    int wanted_w = MIN(width, LCD_WIDTH);

    image_cache_line = -1;
    image_cache_width = 0;
    image_cache_height = 0;
    if (!line->image_path[0])
        return false;
    if (!ow_resolve_image_path(line->image_path, resolved, sizeof(resolved)))
        return false;

    rb->memset(&bm, 0, sizeof(bm));
    bm.width = wanted_w;
    bm.height = wanted_h;
    bm.data = (unsigned char *)image_pixels;

    if (ow_has_ext(resolved, ".bmp"))
    {
        rc = rb->read_bmp_file(resolved, &bm, sizeof(image_pixels),
                               FORMAT_NATIVE | FORMAT_RESIZE |
                               FORMAT_KEEP_ASPECT, NULL);
    }
#ifdef HAVE_JPEG
    else if (ow_has_ext(resolved, ".jpg.jpeg"))
    {
        rc = rb->read_jpeg_file(resolved, &bm, sizeof(image_pixels),
                                FORMAT_NATIVE | FORMAT_RESIZE |
                                FORMAT_KEEP_ASPECT, NULL);
    }
#endif

    if (rc <= 0 || bm.width <= 0 || bm.height <= 0)
        return false;

    image_cache_line = line_index;
    image_cache_width = MIN(bm.width, width);
    image_cache_height = MIN(bm.height, wanted_h);
    return true;
}

static bool ow_service_visible_image(void)
{
    int i;
    int y = OW_TOP_H + 2 - browser_scroll;

    for (i = 0; i < render.line_count && y < LCD_HEIGHT - OW_BOTTOM_H; i++)
    {
        int item_h = ow_line_item_height(&render.lines[i]);

        if ((render.lines[i].style & OW_STYLE_IMAGE) &&
            y + item_h > OW_TOP_H + 2)
        {
            if (image_cache_line == i)
                return false;
            if (!ow_cache_image_line(i, LCD_WIDTH - OW_MARGIN_X * 2))
                render.lines[i].style = OW_STYLE_MEDIA;
            return true;
        }
        y += item_h + ((render.lines[i].style & OW_STYLE_IMAGE) ? 1 : 0);
    }
    return false;
}

static bool ow_draw_image_line(int line_index, int x, int y, int width)
{
    int draw_x;

    if (image_cache_line != line_index || image_cache_width <= 0 ||
        image_cache_height <= 0)
    {
        if (!ow_cache_image_line(line_index, width))
            return false;
    }

    draw_x = x + MAX(0, (width - image_cache_width) / 2);
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(OW_IPODJS_SHADOW);
    rb->lcd_fillrect(draw_x + 2, y + 2, image_cache_width,
                     image_cache_height);
#endif
    rb->lcd_bitmap(image_pixels, draw_x, y, image_cache_width,
                   image_cache_height);
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(ow_dark() ? ow_color_split() :
                           OW_IPODJS_ADDRESS_BORDER);
    rb->lcd_drawrect(draw_x, y, image_cache_width, image_cache_height);
#endif
    return true;
}

static void ow_draw_scrollbar(void)
{
    int viewport_h = LCD_HEIGHT - OW_TOP_H - OW_BOTTOM_H - 2;
    int content_h = ow_content_height();
    int max_scroll = MAX(0, content_h - viewport_h);
    int track_y = OW_TOP_H + 2;
    int thumb_h;
    int thumb_y;

    if (max_scroll <= 0)
        return;

    thumb_h = MAX(12, viewport_h * viewport_h / content_h);
    thumb_y = track_y + browser_scroll * (viewport_h - thumb_h) / max_scroll;
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(ow_color_split());
#else
    rb->lcd_set_foreground(LCD_BLACK);
#endif
    rb->lcd_fillrect(LCD_WIDTH - 3, track_y, 2, viewport_h);
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(ow_color_muted());
#endif
    rb->lcd_fillrect(LCD_WIDTH - 4, thumb_y, 3, thumb_h);
}

static void ow_draw_header(const char *path)
{
    char address[OW_FIELD_LEN];
    int page = ow_find_page_by_path(path);
    bool instagram =
        (page >= 0 && rb->strcasestr(pages[page].url, "instagram.com")) ||
        rb->strcasestr(path, "instagram.com");
    bool onlyfans =
        (page >= 0 && rb->strcasestr(pages[page].url, "onlyfans.com")) ||
        rb->strcasestr(path, "onlyfans.com");

    if (online_mode && !rb->strcmp(path, OW_GOOGLE))
        rb->strlcpy(address, "google.com", sizeof(address));
    else if (online_mode &&
             (!rb->strncasecmp(path, "http://", 7) ||
              !rb->strncasecmp(path, "https://", 8)))
    {
        const char *url = path;
        if (!rb->strncasecmp(url, "https://", 8))
            url += 8;
        else if (!rb->strncasecmp(url, "http://", 7))
            url += 7;
        rb->strlcpy(address, url, sizeof(address));
    }
    else if (ow_is_shortcuts_path(path))
        rb->strlcpy(address, "Saved Pages", sizeof(address));
    else if (page >= 0 && pages[page].url[0])
    {
        const char *url = pages[page].url;

        if (!rb->strncmp(url, "https://", 8))
            url += 8;
        else if (!rb->strncmp(url, "http://", 7))
            url += 7;
        rb->strlcpy(address, url, sizeof(address));
    }
    else
        rb->strlcpy(address, ow_basename(path), sizeof(address));

#ifdef HAVE_LCD_COLOR
    if (instagram || onlyfans)
    {
        fb_data background = instagram ? LCD_WHITE : OW_ONLYFANS_NAV;
        fb_data foreground = instagram ? LCD_BLACK : LCD_WHITE;

        rb->lcd_set_drawmode(DRMODE_SOLID);
        rb->lcd_set_background(background);
        rb->lcd_set_foreground(background);
        rb->lcd_fillrect(0, 0, LCD_WIDTH, OW_TOP_H);
        rb->lcd_set_foreground(foreground);
        rb->lcd_set_drawmode(DRMODE_FG);
        if (back_count > 0)
            rb->lcd_putsxy(OW_MARGIN_X, 5, "<");
        ow_draw_bar_text(back_count > 0 ? 18 : OW_MARGIN_X, 5,
                         LCD_WIDTH - (back_count > 0 ? 23 : OW_MARGIN_X * 2),
                         instagram ? "Instagram" : "OnlyFans");
        rb->lcd_set_drawmode(DRMODE_SOLID);
        rb->lcd_set_foreground(instagram ? OW_IPODJS_ADDRESS_BORDER :
                               OW_ONLYFANS_NAV);
        rb->lcd_hline(0, LCD_WIDTH - 1, OW_TOP_H - 1);
        return;
    }
#endif

#ifdef HAVE_LCD_COLOR
    ow_fill_two_tone(0, 0, LCD_WIDTH, OW_TOP_NAV_H,
                     ow_dark() ? ow_color_header() :
                     OW_IPODJS_HEADER_TOP,
                     ow_dark() ? OW_IPODJS_HEADER_DARK_LINE :
                     OW_IPODJS_TOOLBAR_MID);
#else
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, OW_TOP_NAV_H);
#endif
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(ow_color_text());
    rb->lcd_set_background(OW_IPODJS_TOOLBAR_MID);
#else
    rb->lcd_set_foreground(LCD_BLACK);
#endif
    rb->lcd_set_drawmode(DRMODE_FG);
    if (back_count > 0)
    {
#ifdef HAVE_LCD_COLOR
        ow_fill_round_rect(4, 2, 25, 15, OW_IPODJS_ADDRESS_BORDER);
        ow_fill_round_rect(5, 3, 23, 13, OW_IPODJS_TOOLBAR_MID);
        rb->lcd_set_foreground(ow_color_text());
        rb->lcd_putsxy(10, 3, "<");
#else
        rb->lcd_putsxy(OW_MARGIN_X, 2, "<");
#endif
    }
    ow_draw_bar_text(back_count > 0 ? 34 : OW_MARGIN_X, 2,
                     LCD_WIDTH - (back_count > 0 ? 39 : OW_MARGIN_X * 2),
                     ow_title_for_path(path));
    rb->lcd_set_drawmode(DRMODE_SOLID);
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(ow_dark() ? ow_color_header() :
                           OW_IPODJS_TOOLBAR_MID);
    rb->lcd_fillrect(0, OW_TOP_NAV_H, LCD_WIDTH,
                     OW_TOP_H - OW_TOP_NAV_H);
    ow_fill_round_rect(5, OW_TOP_NAV_H + 2, LCD_WIDTH - 10, 15,
                       ow_dark() ? OW_IPODJS_HEADER_DARK_LINE :
                       OW_IPODJS_ADDRESS_BORDER);
    ow_fill_round_rect(6, OW_TOP_NAV_H + 3, LCD_WIDTH - 12, 13,
                       ow_dark() ? OW_IPODJS_DARK_ADDRESS :
                       OW_IPODJS_ADDRESS_BG);
    rb->lcd_set_foreground(ow_color_muted());
    rb->lcd_set_background(ow_dark() ? OW_IPODJS_DARK_ADDRESS :
                           OW_IPODJS_ADDRESS_BG);
#endif
    ow_draw_bar_text(10, OW_TOP_NAV_H + 3, LCD_WIDTH - 20, address);
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(ow_color_split());
#endif
    rb->lcd_hline(0, LCD_WIDTH - 1, OW_TOP_H - 1);
}

static bool ow_draw_youtube_bitmap(const char *path, int x, int y,
                                   int width, int height)
{
    struct bitmap bm;
    char resolved[MAX_PATH];
    int rc;

    if (!ow_resolve_image_path(path, resolved, sizeof(resolved)))
        return false;
    rb->memset(&bm, 0, sizeof(bm));
    bm.width = width;
    bm.height = height;
    bm.data = (unsigned char *)image_pixels;
    rc = rb->read_bmp_file(resolved, &bm, sizeof(image_pixels),
                           FORMAT_NATIVE | FORMAT_RESIZE |
                           FORMAT_KEEP_ASPECT, NULL);
    if (rc <= 0 || bm.width <= 0 || bm.height <= 0)
        return false;
    rb->lcd_bitmap(image_pixels, x + MAX(0, (width - bm.width) / 2),
                   y + MAX(0, (height - bm.height) / 2),
                   bm.width, bm.height);
    return true;
}

static bool ow_draw_youtube_frame(int index)
{
    off_t offset;
    size_t frame_bytes = 210 * 158 * 2;
    ssize_t read_bytes;

    if (sizeof(fb_data) != 2 || !youtube_page.frame_path[0])
        return false;
    if (youtube_frame_fd < 0)
        youtube_frame_fd = rb->open(youtube_page.frame_path, O_RDONLY);
    if (youtube_frame_fd < 0)
        return false;
    offset = (off_t)index * (off_t)frame_bytes;
    if (rb->lseek(youtube_frame_fd, offset, SEEK_SET) < 0)
        return false;
    read_bytes = rb->read(youtube_frame_fd, image_pixels, frame_bytes);
    if (read_bytes != (ssize_t)frame_bytes)
        return false;
    rb->lcd_bitmap(image_pixels, 4, 62, 210, 158);
    return true;
}

static void ow_draw_youtube_page(void)
{
    char meta[OW_LINE_LEN];
    int old_fg = rb->lcd_get_foreground();
    int old_bg = rb->lcd_get_background();

    if (youtube_page.chrome_drawn)
    {
        ow_draw_youtube_frame(youtube_page.frame_index);
        rb->lcd_update_rect(4, 62, 210, 158);
        rb->lcd_set_foreground(old_fg);
        rb->lcd_set_background(old_bg);
        return;
    }

    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    ow_draw_youtube_bitmap(
        OW_ROOT "/assets/youtube-logo-2006.bmp", 5, 2, 100, 40);
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(OW_YOUTUBE_BLUE);
#else
    rb->lcd_set_foreground(LCD_BLACK);
#endif
    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_putsxy(112, 5, "Videos | Categories");
    rb->lcd_putsxy(112, 20, "Channels | Community");
    rb->lcd_set_drawmode(DRMODE_SOLID);
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(OW_IPODJS_SPLIT);
#else
    rb->lcd_set_foreground(LCD_BLACK);
#endif
    rb->lcd_hline(0, LCD_WIDTH - 1, 43);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_drawmode(DRMODE_FG);
    ow_draw_bar_text(5, 46, LCD_WIDTH - 10, youtube_page.title);
    rb->lcd_set_drawmode(DRMODE_SOLID);

    youtube_page.frame_start_tick = *rb->current_tick;
    ow_draw_youtube_frame(youtube_page.frame_index);
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(OW_YOUTUBE_PANEL);
    rb->lcd_fillrect(218, 62, LCD_WIDTH - 218, 158);
    rb->lcd_set_foreground(OW_YOUTUBE_PANEL_LINE);
    rb->lcd_drawrect(218, 62, LCD_WIDTH - 218, 158);
#else
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_drawrect(218, 62, LCD_WIDTH - 218, 158);
#endif
    rb->lcd_set_foreground(LCD_BLACK);
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_background(OW_YOUTUBE_PANEL);
#endif
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_putsxy(223, 67, "From:");
    ow_draw_bar_text(223, 82, 91, youtube_page.uploader);
    if (youtube_page.added[0])
    {
        rb->lcd_putsxy(223, 101, "Added:");
        ow_draw_bar_text(223, 116, 91, youtube_page.added);
    }
    if (youtube_page.views[0])
        ow_draw_bar_text(223, 137, 91, youtube_page.views);
    rb->lcd_putsxy(223, 158, "Rate:");
    ow_draw_youtube_bitmap(
        OW_ROOT "/assets/youtube-stars-5-2007.bmp",
        223, 173, 70, 14);
    if (youtube_page.duration[0])
    {
        rb->snprintf(meta, sizeof(meta), "Time %s",
                     youtube_page.duration);
        ow_draw_bar_text(223, 194, 91, meta);
    }
    rb->lcd_set_drawmode(DRMODE_SOLID);
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(OW_YOUTUBE_BLUE);
#else
    rb->lcd_set_foreground(LCD_BLACK);
#endif
    rb->lcd_fillrect(0, 222, LCD_WIDTH, LCD_HEIGHT - 222);
    rb->lcd_set_foreground(LCD_WHITE);
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_background(OW_YOUTUBE_BLUE);
#endif
    rb->lcd_set_drawmode(DRMODE_FG);
    ow_draw_bar_text(6, 225, LCD_WIDTH - 12,
                     "Select Full Screen       Menu Back");
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_update();
    youtube_page.chrome_drawn = true;
    rb->lcd_set_foreground(old_fg);
    rb->lcd_set_background(old_bg);
}

static void ow_draw_browser(const char *path)
{
    int old_fg = rb->lcd_get_foreground();
    int old_bg = rb->lcd_get_background();
    struct viewport content_vp;
    struct viewport *old_vp;
    int viewport_h = LCD_HEIGHT - OW_TOP_H - OW_BOTTOM_H - 2;
    int line_h;
    int i;
    int y;
    char status[OW_LINE_LEN];
    bool site_footer = false;

    if (youtube_page.active)
    {
        ow_draw_youtube_page();
        return;
    }

    ow_clamp_browser_scroll();
    ow_pick_visible_link();
    line_h = ow_line_height();

#ifdef HAVE_LCD_COLOR
    rb->lcd_set_background(ow_color_screen());
    rb->lcd_set_foreground(ow_color_screen());
#else
    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_foreground(LCD_WHITE);
#endif
    rb->lcd_clear_display();
    rb->lcd_set_drawmode(DRMODE_SOLID);

    rb->memset(&content_vp, 0, sizeof(content_vp));
    content_vp.x = 0;
    content_vp.y = OW_TOP_H + 2;
    content_vp.width = LCD_WIDTH;
    content_vp.height = viewport_h;
    content_vp.font = FONT_UI;
    content_vp.drawmode = DRMODE_SOLID;
#if LCD_DEPTH > 1
    content_vp.fg_pattern = ow_color_text();
    content_vp.bg_pattern = ow_color_screen();
#endif
    old_vp = rb->lcd_set_viewport(&content_vp);

    y = -browser_scroll;
    for (i = 0; i < render.line_count && y < viewport_h; i++)
    {
        struct ow_render_line *line = &render.lines[i];
        bool selected = line->link >= 0 && line->link == browser_selected_link;
        int item_h = ow_line_item_height(line);

        if (y + item_h <= 0)
        {
            y += item_h + ((line->style & OW_STYLE_IMAGE) ? 1 : 0);
            continue;
        }

        if (selected && !(line->style & OW_STYLE_IMAGE))
        {
#ifdef HAVE_LCD_COLOR
            ow_fill_two_tone(0, y - 1, LCD_WIDTH, item_h,
                             ow_dark() ? ow_color_selected() :
                             ow_color_selected_top(),
                             ow_color_selected());
#else
            rb->lcd_set_foreground(LCD_BLACK);
            rb->lcd_set_drawmode(DRMODE_COMPLEMENT);
            rb->lcd_fillrect(0, y - 1, LCD_WIDTH, item_h);
#endif
            rb->lcd_set_drawmode(DRMODE_SOLID);
        }

        if (line->style & OW_STYLE_RULE)
        {
#ifdef HAVE_LCD_COLOR
            rb->lcd_set_foreground(ow_color_split());
#else
            rb->lcd_set_foreground(LCD_BLACK);
#endif
            rb->lcd_hline(OW_MARGIN_X, LCD_WIDTH - OW_MARGIN_X, y + line_h / 2);
        }
        else if ((line->style & OW_STYLE_IMAGE) && line->image_path[0])
        {
            bool drew;
#ifdef HAVE_LCD_COLOR
            rb->lcd_set_background(ow_color_panel());
            rb->lcd_set_foreground(ow_color_muted());
#else
            rb->lcd_set_background(LCD_WHITE);
            rb->lcd_set_foreground(LCD_BLACK);
#endif
            drew = y >= 0 &&
                   ow_draw_image_line(i, OW_MARGIN_X, y,
                                      LCD_WIDTH - OW_MARGIN_X * 2);
            if (!drew)
                rb->lcd_putsxy(OW_MARGIN_X, y, line->text);
            else if (selected)
            {
#ifdef HAVE_LCD_COLOR
                rb->lcd_set_foreground(ow_color_selected());
#else
                rb->lcd_set_foreground(LCD_BLACK);
#endif
                rb->lcd_drawrect(OW_MARGIN_X, y,
                                 LCD_WIDTH - OW_MARGIN_X * 2 -
                                 OW_SCROLLBAR_W, item_h);
            }
        }
        else if (line->style & OW_STYLE_MEDIA)
        {
#ifdef HAVE_LCD_COLOR
            ow_fill_two_tone(OW_MARGIN_X, y,
                             LCD_WIDTH - OW_MARGIN_X * 2 -
                             OW_SCROLLBAR_W, item_h - 2,
                             selected ? ow_color_selected_top() :
                             (ow_dark() ? ow_color_header() :
                              OW_IPODJS_MEDIA_TOP),
                             selected ? ow_color_selected() :
                             (ow_dark() ? OW_IPODJS_HEADER_DARK_LINE :
                              OW_IPODJS_MEDIA_BOTTOM));
            rb->lcd_set_foreground(selected ? LCD_WHITE : ow_color_text());
            rb->lcd_set_background(selected ? ow_color_selected() :
                                   OW_IPODJS_MEDIA_BOTTOM);
            ow_draw_play_badge(OW_MARGIN_X + 5, y + 4, selected);
            rb->lcd_set_foreground(selected ? LCD_WHITE : ow_color_text());
#else
            rb->lcd_set_foreground(LCD_BLACK);
            rb->lcd_drawrect(OW_MARGIN_X, y, LCD_WIDTH - OW_MARGIN_X * 2,
                             item_h - 1);
#endif
            rb->lcd_set_drawmode(DRMODE_FG);
            ow_draw_bar_text(OW_MARGIN_X + 34, y + 2,
                             LCD_WIDTH - OW_MARGIN_X * 2 - 40, line->text);
            rb->lcd_putsxy(OW_MARGIN_X + 34, y + line_h + 2,
                           "Select to play");
            rb->lcd_set_drawmode(DRMODE_SOLID);
        }
        else
        {
            int text_x = OW_MARGIN_X;

            if (ow_is_shortcuts_path(path) && line->link >= 0)
                text_x = 26;
#ifdef HAVE_LCD_COLOR
            if (line->style & OW_STYLE_HEADING)
                rb->lcd_set_foreground(ow_color_text());
            else if (line->style & OW_STYLE_META)
                rb->lcd_set_foreground(selected ? LCD_WHITE :
                                       ow_color_muted());
            else if (line->link >= 0)
                rb->lcd_set_foreground(selected ? LCD_WHITE :
                                       ow_color_link());
            else
                rb->lcd_set_foreground(ow_color_text());
#else
            rb->lcd_set_foreground(LCD_BLACK);
#endif
#ifdef HAVE_LCD_COLOR
            rb->lcd_set_background(selected ? ow_color_selected() :
                                   ow_color_panel());
#else
            rb->lcd_set_background(LCD_WHITE);
#endif
#ifdef HAVE_LCD_COLOR
            if (ow_is_shortcuts_path(path) &&
                (line->style & OW_STYLE_LINK))
                ow_draw_globe(6, y + 1, selected);
#endif
            rb->lcd_set_drawmode(DRMODE_FG);
            rb->lcd_putsxy(text_x, y, line->text);
            rb->lcd_set_drawmode(DRMODE_SOLID);
        }
        y += item_h + ((line->style & OW_STYLE_IMAGE) ? 1 : 0);
    }

    rb->lcd_set_viewport(old_vp);
    ow_draw_header(path);
    ow_draw_scrollbar();

#ifdef HAVE_LCD_COLOR
    site_footer = ow_draw_site_footer(path);
#endif
    if (!site_footer)
    {
#ifdef HAVE_LCD_COLOR
        ow_fill_two_tone(0, LCD_HEIGHT - OW_BOTTOM_H, LCD_WIDTH, OW_BOTTOM_H,
                         ow_dark() ? ow_color_header() :
                         OW_IPODJS_HEADER_TOP,
                         ow_dark() ? OW_IPODJS_HEADER_DARK_LINE :
                         OW_IPODJS_TOOLBAR_MID);
        rb->lcd_set_foreground(ow_color_split());
#else
        rb->lcd_set_foreground(LCD_BLACK);
#endif
        rb->lcd_hline(0, LCD_WIDTH - 1, LCD_HEIGHT - OW_BOTTOM_H);
#ifdef HAVE_LCD_COLOR
        rb->lcd_set_foreground(ow_color_muted());
        rb->lcd_set_background(ow_color_panel());
#else
        rb->lcd_set_foreground(LCD_BLACK);
#endif
        if (ow_is_shortcuts_path(path))
            rb->snprintf(status, sizeof(status),
                         "Wheel Browse     Select Open");
        else
        {
            int footer_viewport_h =
                LCD_HEIGHT - OW_TOP_H - OW_BOTTOM_H - 2;
            int max_scroll =
                MAX(0, ow_content_height() - footer_viewport_h);
            int percent = max_scroll > 0 ?
                          browser_scroll * 100 / max_scroll : 100;

            rb->snprintf(status, sizeof(status),
                         "Menu Back     %d%%     Zoom %d",
                         percent, browser_zoom + 1);
        }
        rb->lcd_set_drawmode(DRMODE_FG);
        ow_draw_bar_text(OW_MARGIN_X, LCD_HEIGHT - OW_BOTTOM_H + 4,
                         LCD_WIDTH - OW_MARGIN_X * 2, status);
        rb->lcd_set_drawmode(DRMODE_SOLID);
    }

    rb->lcd_update();
    rb->lcd_set_foreground(old_fg);
    rb->lcd_set_background(old_bg);
}

static void ow_scroll_browser(int delta)
{
    int direction = delta < 0 ? -1 : 1;
    int distance = delta < 0 ? -delta : delta;

    /*
     * The click wheel reports coarse repeat events.  Treat them as input
     * velocity rather than literal pixel distances so archived pages move
     * continuously instead of jumping by an entire text/image row.
     */
    browser_scroll += direction *
        (distance > OW_SCROLL_STEP ? 12 : 6);
    ow_clamp_browser_scroll();
    browser_selected_link = ow_is_social_path(current_path) ?
                            ow_dominant_visible_link() :
                            ow_first_visible_link();
}

static int ow_play_youtube_video(void)
{
    static char launch_path[MAX_PATH];

    if (!youtube_page.video_path[0])
        return OW_BROWSER_CONTINUE;
    if (youtube_frame_fd >= 0)
    {
        rb->close(youtube_frame_fd);
        youtube_frame_fd = -1;
    }
    rb->snprintf(launch_path, sizeof(launch_path), "youtube:%s",
                 youtube_page.video_path);
    return rb->plugin_open(
        plugin_video_player_for(youtube_page.video_path), launch_path);
}

static void ow_draw_remote_page(const char *path, int cursor_x)
{
    struct bitmap bm;
    int old_fg = rb->lcd_get_foreground();
    int old_bg = rb->lcd_get_background();
    int rc;
    int screen_x = 5 + cursor_x;
    int screen_y = OW_TOP_H + 2 + 85;

    rb->lcd_set_viewport(NULL);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    ow_draw_header(path);

    rb->memset(&bm, 0, sizeof(bm));
    bm.width = 310;
    bm.height = 170;
    bm.data = (unsigned char *)image_pixels;
    {
        unsigned char magic[2] = { 0, 0 };
        int fd = rb->open(OW_LIVE_BITMAP, O_RDONLY);
        if (fd >= 0)
        {
            rb->read(fd, magic, sizeof(magic));
            rb->close(fd);
        }
        if (magic[0] == 0xff && magic[1] == 0xd8)
            rc = rb->read_jpeg_file(OW_LIVE_BITMAP, &bm,
                                    sizeof(image_pixels),
                                    FORMAT_NATIVE | FORMAT_RESIZE, NULL);
        else
            rc = rb->read_bmp_file(OW_LIVE_BITMAP, &bm,
                                   sizeof(image_pixels),
                                   FORMAT_NATIVE | FORMAT_RESIZE, NULL);
    }
    if (rc > 0)
    {
        int width = MIN(bm.width, 310);
        int height = MIN(bm.height, 170);
        /* Keep the last complete viewport on screen while the iPhone renders
         * and transfers the next one. Translating the old bitmap exposed a
         * white strip at its trailing edge and made a healthy scroll look
         * like a partially loaded page. */
        int offset = 0;
        if (offset >= 0)
            rb->lcd_bitmap(image_pixels + offset * bm.width, 5,
                           OW_TOP_H + 2, width, height - offset);
        else
            rb->lcd_bitmap(image_pixels, 5, OW_TOP_H + 2 - offset,
                           width, height + offset);
    }
    else
    {
        rb->lcd_set_foreground(LCD_BLACK);
        rb->lcd_set_background(LCD_WHITE);
        rb->lcd_putsxy(12, 96, "Live page unavailable");
    }

#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(ow_color_selected());
#else
    rb->lcd_set_foreground(LCD_BLACK);
#endif
    rb->lcd_hline(screen_x - 3, screen_x + 3, screen_y - 7);
    rb->lcd_hline(screen_x - 3, screen_x + 3, screen_y + 7);
    rb->lcd_vline(screen_x - 7, screen_y - 3, screen_y + 3);
    rb->lcd_vline(screen_x + 7, screen_y - 3, screen_y + 3);
    rb->lcd_drawline(screen_x - 6, screen_y - 4,
                     screen_x - 4, screen_y - 6);
    rb->lcd_drawline(screen_x + 4, screen_y - 6,
                     screen_x + 6, screen_y - 4);
    rb->lcd_drawline(screen_x - 6, screen_y + 4,
                     screen_x - 4, screen_y + 6);
    rb->lcd_drawline(screen_x + 4, screen_y + 6,
                     screen_x + 6, screen_y + 4);
    rb->lcd_fillrect(screen_x - 1, screen_y - 1, 3, 3);

#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(ow_dark() ? OW_IPODJS_DARK_PANEL :
                           OW_IPODJS_TOOLBAR_MID);
#else
    rb->lcd_set_foreground(LCD_WHITE);
#endif
    rb->lcd_fillrect(0, 212, LCD_WIDTH, LCD_HEIGHT - 212);
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(ow_color_split());
#else
    rb->lcd_set_foreground(LCD_BLACK);
#endif
    rb->lcd_hline(0, LCD_WIDTH - 1, 212);
    rb->lcd_set_background(ow_dark() ? OW_IPODJS_DARK_PANEL :
                           OW_IPODJS_TOOLBAR_MID);
    rb->lcd_set_foreground(ow_color_text());
    ow_draw_bar_text(5, 215, LCD_WIDTH - 10,
                     "Wheel Scroll   Select Tap   Play URL");
    rb->lcd_set_foreground(old_fg);
    rb->lcd_set_background(old_bg);
    rb->lcd_update();
}

static void ow_input_to_url(const char *input, char *url, size_t size)
{
    char encoded[220];
    size_t out = 0;
    static const char hex[] = "0123456789ABCDEF";

    if (!rb->strncasecmp(input, "http://", 7) ||
        !rb->strncasecmp(input, "https://", 8))
    {
        rb->strlcpy(url, input, size);
        return;
    }
    if (!rb->strchr(input, ' ') && rb->strchr(input, '.'))
    {
        rb->snprintf(url, size, "https://%s", input);
        return;
    }
    for (size_t i = 0; input[i] && out + 3 < sizeof(encoded); i++)
    {
        unsigned char c = input[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')
            encoded[out++] = c;
        else if (c == ' ')
            encoded[out++] = '+';
        else
        {
            encoded[out++] = '%';
            encoded[out++] = hex[c >> 4];
            encoded[out++] = hex[c & 15];
        }
    }
    encoded[out] = '\0';
    rb->snprintf(url, size, "https://www.google.com/search?q=%s", encoded);
}

static int ow_show_remote_page(const char *initial_path)
{
    char path[MAX_PATH];
    int cursor_x = 155;
    bool redraw = true;

    rb->strlcpy(path, initial_path, sizeof(path));
    rb->memset(&live_transfer, 0, sizeof(live_transfer));
    live_transfer.fd = -1;
    live_visual_scroll = 0;
    live_queued_scroll = 0;
    while (true)
    {
        int button;
        char command[224];

        if (redraw)
        {
            ow_draw_remote_page(path, cursor_x);
            redraw = false;
        }
        button = rb->button_get_w_tmo(HZ / 4);
        if (button == BUTTON_NONE)
        {
            int refreshed = ow_live_command_service();
            if (refreshed != 0)
            {
                if (refreshed > 0)
                    live_visual_scroll = 0;
                redraw = true;
                if (live_queued_scroll)
                {
                    int queued = live_queued_scroll;
                    live_queued_scroll = 0;
                    if (!ow_live_command_begin(queued))
                        live_queued_scroll = queued;
                }
            }
#ifdef SIMULATOR
            if (remote_preview_mode)
                continue;
#endif
            if (!ow_online_connected())
            {
                ow_live_scroll_cancel();
                rb->splash(HZ, "Internet connection lost");
                return PLUGIN_OK;
            }
            continue;
        }
        if ((button & BUTTON_MENU) && (button & BUTTON_SELECT))
        {
            ow_live_scroll_cancel();
            return PLUGIN_OK;
        }
#ifdef SIMULATOR
        if (remote_preview_mode)
        {
            if (button == BUTTON_MENU || button == (BUTTON_MENU | BUTTON_REL))
            {
                ow_live_scroll_cancel();
                return PLUGIN_OK;
            }
            if (button == (BUTTON_SCROLL_FWD | BUTTON_REL))
                button = BUTTON_SCROLL_FWD;
            else if (button == (BUTTON_SCROLL_BACK | BUTTON_REL))
                button = BUTTON_SCROLL_BACK;
            if (button != BUTTON_SCROLL_FWD &&
                button != (BUTTON_SCROLL_FWD | BUTTON_REPEAT) &&
                button != BUTTON_SCROLL_BACK &&
                button != (BUTTON_SCROLL_BACK | BUTTON_REPEAT))
                continue;
        }
#endif

        switch (button)
        {
            case BUTTON_SCROLL_FWD:
            case BUTTON_SCROLL_FWD | BUTTON_REPEAT:
            {
                int delta = button & BUTTON_REPEAT ? 42 : 18;
                live_visual_scroll = MIN(64, live_visual_scroll + delta);
                if (live_transfer.active)
                    live_queued_scroll += delta;
                else if (!ow_live_command_begin(delta))
                    live_queued_scroll += delta;
                redraw = true;
                break;
            }

            case BUTTON_SCROLL_BACK:
            case BUTTON_SCROLL_BACK | BUTTON_REPEAT:
            {
                int delta = button & BUTTON_REPEAT ? -42 : -18;
                live_visual_scroll = MAX(-64, live_visual_scroll + delta);
                if (live_transfer.active)
                    live_queued_scroll += delta;
                else if (!ow_live_command_begin(delta))
                    live_queued_scroll += delta;
                redraw = true;
                break;
            }

            case BUTTON_LEFT:
                cursor_x = MAX(8, cursor_x - 24);
                redraw = true;
                break;

            case BUTTON_RIGHT:
                cursor_x = MIN(302, cursor_x + 24);
                redraw = true;
                break;

            case BUTTON_LEFT | BUTTON_REPEAT:
                ow_live_scroll_cancel();
                ow_draw_loading("Going back...");
                ow_remote_command("back");
                redraw = true;
                break;

            case BUTTON_RIGHT | BUTTON_REPEAT:
                ow_live_scroll_cancel();
                ow_draw_loading("Going forward...");
                ow_remote_command("forward");
                redraw = true;
                break;

            case BUTTON_SELECT:
                ow_live_scroll_cancel();
                rb->snprintf(command, sizeof(command), "tap:%d:85", cursor_x);
                ow_draw_loading("Opening...");
                ow_remote_command(command);
                redraw = true;
                break;

            case BUTTON_SELECT | BUTTON_REPEAT:
#if defined(HAVE_IPODJS_UI) && defined(HAVE_TAGCACHE)
            {
                char input[160] = "";
                if (rb->root_menu_ipodjs_text_input("Form Text", input,
                                                     sizeof(input)))
                {
                    ow_live_scroll_cancel();
                    char encoded[180];
                    size_t out = 0;
                    static const char hex[] = "0123456789ABCDEF";
                    for (size_t i = 0; input[i] && out + 3 < sizeof(encoded); i++)
                    {
                        unsigned char c = input[i];
                        if ((c >= 'A' && c <= 'Z') ||
                            (c >= 'a' && c <= 'z') ||
                            (c >= '0' && c <= '9') || c == '-' ||
                            c == '_' || c == '.' || c == ' ')
                            encoded[out++] = c == ' ' ? '+' : c;
                        else
                        {
                            encoded[out++] = '%';
                            encoded[out++] = hex[c >> 4];
                            encoded[out++] = hex[c & 15];
                        }
                    }
                    encoded[out] = '\0';
                    rb->snprintf(command, sizeof(command), "text:%s", encoded);
                    ow_remote_command(command);
                    redraw = true;
                }
                break;
            }
#else
                break;
#endif

            case BUTTON_PLAY:
#if defined(HAVE_IPODJS_UI) && defined(HAVE_TAGCACHE)
            {
                char input[160] = "";
                char url[MAX_PATH];
                if (rb->root_menu_ipodjs_text_input("Address or Search", input,
                                                     sizeof(input)))
                {
                    ow_live_scroll_cancel();
                    ow_input_to_url(input, url, sizeof(url));
                    ow_draw_loading("Loading live page...");
                    if (ow_fetch_live_page(url))
                    {
                        rb->strlcpy(path, url, sizeof(path));
                    }
                    redraw = true;
                }
                break;
            }
#else
                break;
#endif

            case BUTTON_PLAY | BUTTON_REPEAT:
                ow_live_scroll_cancel();
                ow_draw_loading("Reloading...");
                ow_remote_command("reload");
                redraw = true;
                break;

            case BUTTON_MENU:
            case BUTTON_MENU | BUTTON_REL:
                ow_live_scroll_cancel();
                return PLUGIN_OK;

            default:
                if (button == SYS_USB_CONNECTED ||
                    rb->default_event_handler(button) == SYS_USB_CONNECTED)
                {
                    ow_live_scroll_cancel();
                    return PLUGIN_USB_CONNECTED;
                }
                break;
        }
    }
}

static int ow_follow_selected_link(void)
{
    int link = browser_selected_link;

    if (link < 0)
        link = ow_first_visible_link();
    if (link < 0 || link >= render.link_count)
    {
        rb->splash(HZ, "No link here");
        return OW_BROWSER_CONTINUE;
    }

    if (online_mode &&
        !rb->strcmp(render.links[link].target, OW_GOOGLE_SEARCH))
    {
#if defined(HAVE_IPODJS_UI) && defined(HAVE_TAGCACHE)
        char input[160] = "";
        char url[MAX_PATH];
        char encoded[220];
        size_t out = 0;

        if (!rb->root_menu_ipodjs_text_input("Google", input,
                                             sizeof(input)))
            return OW_BROWSER_CONTINUE;
        if (!rb->strncasecmp(input, "http://", 7) ||
            !rb->strncasecmp(input, "https://", 8))
            rb->strlcpy(url, input, sizeof(url));
        else
        {
            static const char hex[] = "0123456789ABCDEF";
            for (size_t i = 0; input[i] && out + 3 < sizeof(encoded); i++)
            {
                unsigned char c = input[i];
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                    (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                    c == '.')
                    encoded[out++] = c;
                else if (c == ' ')
                    encoded[out++] = '+';
                else
                {
                    encoded[out++] = '%';
                    encoded[out++] = hex[c >> 4];
                    encoded[out++] = hex[c & 15];
                }
            }
            encoded[out] = '\0';
            rb->snprintf(url, sizeof(url),
                         "https://www.google.com/search?q=%s", encoded);
        }
        return ow_open_path(url, true);
#else
        rb->splash(HZ, "Native keyboard unavailable");
        return OW_BROWSER_CONTINUE;
#endif
    }

    return ow_open_path(render.links[link].target, true);
}

static int ow_show_page(const char *path)
{
    bool redraw = true;
    bool youtube_paused = false;
    bool youtube_menu_held = false;
    bool social_menu_held = false;
    int button;

    rb->strlcpy(current_path, path, sizeof(current_path));
    ow_log_path(OW_HISTORY, path);
    ow_render_path(path);
    browser_scroll = 0;
    image_cache_line = -1;
    browser_selected_link = -1;
    ow_pick_visible_link();
    if ((rb->strstr(path, "/www.youtube.com/subscriptions.html") ||
         rb->strstr(path, "/www.youtube.com/channel-")) &&
        render.link_count > 0)
        browser_selected_link = 0;
    if (youtube_page.active)
        return ow_play_youtube_video();

    while (true)
    {
        if (redraw)
        {
            if (!youtube_page.active)
                ow_service_visible_image();
            ow_draw_browser(path);
            redraw = false;
        }

        button = rb->button_get_w_tmo(
            youtube_page.active ? MAX(1, HZ / 48) : HZ / 4);
        if ((button & BUTTON_MENU) && (button & BUTTON_SELECT))
            return PLUGIN_OK;

        switch (button)
        {
            case BUTTON_NONE:
                /* Keep draining companion heartbeats while a live page is
                 * displayed.  The fetch loop already services USB, but the
                 * steady-state browser loop previously only inspected the
                 * timestamp and let an otherwise healthy link expire. */
#ifdef USB_ENABLE_ETHERNET
                if (online_mode)
                    rb->usb_internet_service();
#endif
                if (online_mode && !ow_online_connected())
                {
                    rb->splash(HZ, "Internet connection lost");
                    return PLUGIN_OK;
                }
                if (youtube_page.active && !youtube_paused)
                {
                    int frame = (int)(
                        (*rb->current_tick -
                         youtube_page.frame_start_tick) * 24L / HZ);
                    frame = MIN(frame, youtube_page.frame_count - 1);
                    if (frame != youtube_page.frame_index)
                    {
                        youtube_page.frame_index = frame;
                        redraw = true;
                    }
                }
                else if (ow_service_visible_image())
                    redraw = true;
                break;

            case BUTTON_SCROLL_FWD:
                if (youtube_page.active)
                    redraw = true;
                else if (ow_uses_link_wheel(path))
                    ow_move_selected_link(1);
                else
                    ow_scroll_browser(OW_SCROLL_STEP);
                redraw = true;
                break;

            case BUTTON_SCROLL_FWD | BUTTON_REPEAT:
                if (youtube_page.active)
                    redraw = true;
                else if (ow_uses_link_wheel(path))
                    ow_move_selected_link(1);
                else
                    ow_scroll_browser(OW_SCROLL_REPEAT);
                redraw = true;
                break;

            case BUTTON_SCROLL_BACK:
                if (youtube_page.active)
                    redraw = true;
                else if (ow_uses_link_wheel(path))
                    ow_move_selected_link(-1);
                else
                    ow_scroll_browser(-OW_SCROLL_STEP);
                redraw = true;
                break;

            case BUTTON_SCROLL_BACK | BUTTON_REPEAT:
                if (youtube_page.active)
                    redraw = true;
                else if (ow_uses_link_wheel(path))
                    ow_move_selected_link(-1);
                else
                    ow_scroll_browser(-OW_SCROLL_REPEAT);
                redraw = true;
                break;

            case BUTTON_LEFT:
                ow_move_selected_link(-1);
                redraw = true;
                break;

            case BUTTON_LEFT | BUTTON_REPEAT:
                ow_move_selected_link(-3);
                redraw = true;
                break;

            case BUTTON_RIGHT:
                ow_move_selected_link(1);
                redraw = true;
                break;

            case BUTTON_RIGHT | BUTTON_REPEAT:
                ow_move_selected_link(3);
                redraw = true;
                break;

            case BUTTON_MENU:
                break;

            case BUTTON_MENU | BUTTON_REPEAT:
                if (youtube_page.active)
                {
                    youtube_menu_held = true;
                    break;
                }
                if (ow_is_social_path(path))
                {
                    social_menu_held = true;
                    break;
                }
                if (back_count > 0)
                {
                    back_count--;
                    return OW_BROWSER_BACK;
                }
                return PLUGIN_OK;

            case BUTTON_MENU | BUTTON_REL:
                if (youtube_page.active && youtube_menu_held)
                    return ow_open_path(
                        OW_YOUTUBE_SUBSCRIPTIONS, true);
                if (social_menu_held)
                    return ow_open_path(
                        ow_social_profiles_path(path), true);
                if (back_count > 0)
                {
                    back_count--;
                    return OW_BROWSER_BACK;
                }
                return PLUGIN_OK;

            case BUTTON_PLAY:
                if (youtube_page.active)
                {
                    youtube_paused = !youtube_paused;
                    if (!youtube_paused)
                    {
                        youtube_page.frame_start_tick =
                            *rb->current_tick -
                            youtube_page.frame_index * HZ / 24;
                    }
                    redraw = true;
                    break;
                }
                browser_zoom = (browser_zoom + 1) % 3;
                ow_render_path(path);
                browser_scroll = 0;
                image_cache_line = -1;
                browser_selected_link = -1;
                ow_pick_visible_link();
                redraw = true;
                break;

            case BUTTON_PLAY | BUTTON_REPEAT:
                break;

            case BUTTON_SELECT:
            {
                int ret;
                int saved_scroll = browser_scroll;
                int saved_link = browser_selected_link;

                if (youtube_page.active)
                    ret = ow_play_youtube_video();
                else
                    ret = ow_follow_selected_link();
                if (ret == OW_BROWSER_BACK)
                {
                    rb->strlcpy(current_path, path, sizeof(current_path));
                    ow_render_path(path);
                    browser_scroll = saved_scroll;
                    browser_selected_link = saved_link;
                    image_cache_line = -1;
                    ow_clamp_browser_scroll();
                    redraw = true;
                    break;
                }
                if (ret == OW_BROWSER_CONTINUE)
                {
                    redraw = true;
                    break;
                }
                return ret;
            }

            default:
                if (button == SYS_USB_CONNECTED ||
                    rb->default_event_handler(button) == SYS_USB_CONNECTED)
                    return PLUGIN_USB_CONNECTED;
                break;
        }
    }
}

static int ow_open_path(const char *path, bool push_back)
{
    char plugin[MAX_PATH];
    char path_copy[MAX_PATH];
    int attr;

    rb->strlcpy(path_copy, path ? path : "", sizeof(path_copy));

    if (ow_is_shortcuts_path(path_copy))
        return ow_show_page(path_copy);

    if (online_mode &&
        (!rb->strncasecmp(path_copy, "http://", 7) ||
         !rb->strncasecmp(path_copy, "https://", 8)))
    {
        if (!ow_fetch_live_page(path_copy))
        {
            if (!ow_online_connected())
                rb->splash(HZ * 2, "No USB Internet connection");
            else
                rb->splash(HZ * 2, "Page could not be loaded");
            return OW_BROWSER_CONTINUE;
        }
        return ow_show_remote_page(path_copy);
    }

    if (ow_has_ext(path_copy, ".gif.png.jpg.jpeg.bmp"))
    {
        attr = rb->filetype_get_attr(path_copy);
        if (rb->filetype_get_plugin(attr, plugin, sizeof(plugin)))
            return rb->plugin_open(plugin, path_copy);
        rb->splashf(HZ * 2, "No image viewer: %s",
                    ow_basename(path_copy));
        return OW_BROWSER_CONTINUE;
    }

    if (ow_has_ext(path_copy,
                   ".mid.midi.wav.mod.xm.s3m.it.mpg.mpeg.m2v.mp4.m4v.avi.mov"))
    {
        attr = rb->filetype_get_attr(path_copy);
        if (rb->filetype_get_plugin(attr, plugin, sizeof(plugin)))
            return rb->plugin_open(plugin, path_copy);
        rb->splashf(HZ * 2, "No media player: %s",
                    ow_basename(path_copy));
        return OW_BROWSER_CONTINUE;
    }

    if (push_back && current_path[0] && back_count < OW_STACK_LEN)
        rb->strlcpy(back_stack[back_count++], current_path, MAX_PATH);

    return ow_show_page(path_copy);
}

enum plugin_status plugin_start(const void *parameter)
{
    int ret;
    const char *start_path = parameter;

    ow_mkdirs();
    ow_load_pages();

#ifdef SIMULATOR
    if (start_path && !rb->strcmp(start_path, "rockbox:remote-preview"))
    {
        remote_preview_mode = true;
        online_mode = true;
        return ow_show_remote_page("https://www.google.com/");
    }
#endif

    online_mode = start_path && !rb->strcmp(start_path, "online");
    if (online_mode)
    {
#ifdef USB_ENABLE_ETHERNET
        if (!ow_online_connected())
        {
            rb->splash(HZ * 2, "Connect USB Internet first");
            return PLUGIN_OK;
        }
#else
        rb->splash(HZ * 2, "USB Internet is unavailable");
        return PLUGIN_OK;
#endif
        start_path = "https://www.google.com/";
    }

    ret = ow_open_path(start_path && start_path[0] ?
                       start_path : OW_SHORTCUTS, false);
    /* A failed initial live fetch means "stay in Safari", not "close the
     * plugin".  Keep the native Google landing/search surface available so
     * the user can retry or enter another address after a relay timeout. */
    if (ret == OW_BROWSER_CONTINUE && online_mode)
        ret = ow_show_page(OW_GOOGLE);
    if (ret == PLUGIN_USB_CONNECTED)
        return PLUGIN_USB_CONNECTED;
    if (ret == PLUGIN_GOTO_PLUGIN)
        return PLUGIN_GOTO_PLUGIN;
    return PLUGIN_OK;
}
