/***************************************************************************
 * RockPod USB Internet application service.
 *
 * Bulk USB and packet parsing remain fixed-memory firmware work. This app
 * service is the only layer allowed to commit received weather data to disk.
 ****************************************************************************/
#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio.h"
#include "backlight.h"
#include "button.h"
#include "crc32.h"
#include "dir.h"
#include "file.h"
#include "kernel.h"
#include "lcd.h"
#include "metadata.h"
#include "misc.h"
#include "mv.h"
#include "notification.h"
#include "notification_manager.h"
#include "powermgmt.h"
#include "pathfuncs.h"
#include "rbpaths.h"
#include "settings.h"
#include "storage.h"
#include "string-extra.h"
#ifdef HAVE_TAGCACHE
#include "tagcache.h"
#endif
#include "usb.h"
#include "usb_internet.h"
#if defined(IPOD_6G) && !defined(SIMULATOR)
#include "lcd-s5l8702.h"
#endif
#if defined(USB_ENABLE_ETHERNET) && !defined(SIMULATOR)
#include "usbstack/usb_ethernet.h"
#endif
#if defined(USB_ENABLE_IPHETH_HOST) && !defined(SIMULATOR)
#include "usb_iphone_network.h"
#include "usb_iphone_preflight.h"
#include "usb_iphone_tether.h"
#endif

#define WEATHER_DIR ROCKBOX_DIR "/rockpod/weather"
#define WEATHER_FILE WEATHER_DIR "/forecast.tsv"
#define WEATHER_TEMP WEATHER_DIR "/forecast.usb.tmp"
#define WEATHER_MAX_BYTES 32768
#define MESSAGE_MAX 1400
#define WEATHER_REQUEST_TICKS (15 * 60 * HZ)
/* The USB completion interrupt refills udp_queue while the service point
 * drains it, so an unbounded drain never returns while a companion streams:
 * the UI thread stays inside usb_internet_service() and the player looks
 * frozen until the cable is pulled.  Bound the work per call by message count
 * (one queue's worth) and by elapsed ticks, whichever comes first.  Nothing is
 * dropped; the remaining messages stay queued for the next service point. */
#define SERVICE_MESSAGE_BUDGET 12
#define SERVICE_TICK_BUDGET 2
#define FRAMEBUFFER_MAGIC "RPF1"
#define FRAMEBUFFER_HEADER_BYTES 24
#define FRAMEBUFFER_PAYLOAD_BYTES \
    ((MESSAGE_MAX - FRAMEBUFFER_HEADER_BYTES) & ~1)
#define FRAMEBUFFER_WORKER_STACK_SIZE (DEFAULT_STACK_SIZE * 2)
#define FRAMEBUFFER_BATCH_PACKETS USB_ETHERNET_UDP_BATCH_MAX
#define IPHONE_TETHER_LOG ROCKBOX_DIR "/iphone-tether.log"
#define IPHONE_TETHER_LOG_MAX 16384
#define SYNC_MAGIC "RPS1"
#define SYNC_PHONE_DIR ROCKBOX_DIR "/rockpod/phone"
#define SYNC_SITEKICK_PREVIEW ROCKBOX_DIR "/sitekick/preview/current-float.bmp"
#define SYNC_SITEKICK_PANE ROCKBOX_DIR "/sitekick/preview/pane-background.bmp"
#define SYNC_VIDEO_ROW SYNC_PHONE_DIR "/video-row.tsv"
#define SYNC_GAME_ROW SYNC_PHONE_DIR "/game-row.tsv"
#define SYNC_AUDIOBOOK_POSITIONS SYNC_PHONE_DIR "/audiobook-positions-v1.tsv"
#define SYNC_NOTIFICATION_INBOX SYNC_PHONE_DIR "/notification-inbox-v1.tsv"
#define SYNC_CLEANUP_MANIFEST SYNC_PHONE_DIR "/cleanup-v1.txt"
#define SYNC_VIDEO_INDEX ROCKBOX_DIR "/videolist/index.tsv"
#define SYNC_GAME_INDEX ROCKBOX_DIR "/rocks/games/rockboy_launcher/games.tsv"
#define SYNC_LIBRARY_CATALOG SYNC_PHONE_DIR "/library-v1.tsv"
#define SYNC_MAX_FILE 0x7fffffffu

enum usb_internet_sync_type {
    USB_INTERNET_SYNC_PROBE = 0,
    USB_INTERNET_SYNC_STATUS = 1,
    USB_INTERNET_SYNC_BEGIN = 2,
    USB_INTERNET_SYNC_DATA = 3,
    USB_INTERNET_SYNC_COMMIT = 4,
    USB_INTERNET_SYNC_ACK = 5,
    USB_INTERNET_SYNC_SITEKICK_INFO = 6,
    USB_INTERNET_SYNC_SITEKICK_DATA = 7,
    USB_INTERNET_SYNC_SITEKICK_REQUEST = 8,
    USB_INTERNET_SYNC_SAFE_EJECT = 9,
    USB_INTERNET_SYNC_SAFE = 10,
    USB_INTERNET_SYNC_LIBRARY_REQUEST = 11,
    USB_INTERNET_SYNC_LIBRARY_DATA = 12,
    USB_INTERNET_SYNC_FILE_REQUEST = 13,
    USB_INTERNET_SYNC_FILE_DATA = 14,
};

enum usb_internet_message_type {
    USB_INTERNET_WEATHER_BEGIN = 1,
    USB_INTERNET_WEATHER_DATA = 2,
    USB_INTERNET_WEATHER_COMMIT = 3,
    USB_INTERNET_WEATHER_REQUEST = 4,
    USB_INTERNET_WEATHER_ACK = 5,
};

static int weather_fd = -1;
static uint32_t weather_expected_size;
static uint32_t weather_expected_crc;
static uint32_t weather_crc;
static uint32_t weather_received;
static unsigned long weather_generation;
static long next_weather_request;
static bool previous_link;
static unsigned char service_message[MESSAGE_MAX];
static unsigned char sync_response[MESSAGE_MAX];
static int sync_fd = -1;
static uint32_t sync_request;
static uint32_t sync_expected_size;
static uint32_t sync_expected_crc;
static uint32_t sync_received;
static uint32_t sync_crc;
static char sync_final_path[MAX_PATH];
static char sync_temp_path[MAX_PATH];
static char sync_video_line[1024];
static uint32_t sync_library_request;
extern const char rbversion[];
#if defined(USB_ENABLE_ETHERNET) && !defined(SIMULATOR)
static long companion_seen;
static uint32_t framebuffer_packet_words[FRAMEBUFFER_BATCH_PACKETS]
                                        [MESSAGE_MAX / sizeof(uint32_t)];
static unsigned char framebuffer_request_message[MESSAGE_MAX];
static long framebuffer_worker_stack[
    FRAMEBUFFER_WORKER_STACK_SIZE / sizeof(long)] IBSS_ATTR;
static unsigned int framebuffer_worker_id;
static uint32_t framebuffer_request_id;
static uint32_t framebuffer_offset;
static int framebuffer_capture_width = LCD_WIDTH;
static int framebuffer_capture_height = LCD_HEIGHT;
static uint16_t framebuffer_generation;
static bool framebuffer_active;
static bool framebuffer_unchanged;
static bool framebuffer_external_only;
static bool framebuffer_video_active;

static bool ethernet_companion_mode(void)
{
    return global_settings.usb_mode == USB_MODE_INTERNET
#ifdef USB_ENABLE_IPHETH_HOST
           || global_settings.usb_mode == USB_MODE_IPHONE_TETHER
#endif
           ;
}
#endif
#if defined(USB_ENABLE_IPHETH_HOST) && !defined(SIMULATOR)
static enum usb_iphone_tether_state previous_iphone_state = USB_IPHONE_OFF;
static bool previous_iphone_route;
static char previous_tether_status[96];
static char previous_preflight_status[96];
static char previous_network_status[96];
#endif

static uint32_t get_be32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

#if defined(USB_ENABLE_ETHERNET) && !defined(SIMULATOR)
static void put_be16(unsigned char *p, uint16_t value)
{
    p[0] = value >> 8;
    p[1] = value;
}

static uint16_t get_be16(const unsigned char *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}
#endif

static void put_be32(unsigned char *p, uint32_t value)
{
    p[0] = value >> 24;
    p[1] = value >> 16;
    p[2] = value >> 8;
    p[3] = value;
}

#if defined(USB_ENABLE_ETHERNET) && !defined(SIMULATOR)
static void framebuffer_remote_action(unsigned int action)
{
    int button = BUTTON_NONE;
    intptr_t data = 0;

#ifdef HAS_BUTTON_HOLD
    if (button_hold())
        return;
#endif
    switch (action)
    {
        case 1:
            button = BUTTON_MENU;
            break;
        case 2:
            button = BUTTON_SCROLL_BACK;
            data = 1 << 24;
            break;
        case 3:
            button = BUTTON_SCROLL_FWD;
            data = 1 << 24;
            break;
        case 4:
            button = BUTTON_LEFT;
            break;
        case 5:
            button = BUTTON_SELECT;
            break;
        case 6:
            button = BUTTON_RIGHT;
            break;
        case 7:
            button = BUTTON_PLAY;
            break;
    }
    if (button == BUTTON_NONE || button_queue_full())
        return;
    button_queue_post(button, data);
    if (button != BUTTON_SCROLL_BACK && button != BUTTON_SCROLL_FWD &&
        !button_queue_full())
        button_queue_post(BUTTON_REL | button, 0);
}

static void framebuffer_handle_request(const unsigned char *message,
                                       int length)
{
    if (length < 5 || memcmp(message, FRAMEBUFFER_MAGIC, 4))
        return;

    companion_seen = current_tick;
    if (message[4] == 1 &&
        (length == 9 || length == 10 || length == 12))
    {
        bool external_only = length == 10 && (message[9] & 1) != 0;
        uint16_t client_generation = 0;

        if (length == 12)
        {
            external_only = (message[9] & 1) != 0;
            client_generation = get_be16(message + 10);
        }

#if defined(IPOD_6G)
        if (external_only != framebuffer_external_only)
        {
            framebuffer_external_only = external_only;
            if (external_only)
                backlight_on();
            lcd_external_capture_set(true, external_only);
            if (!external_only)
            {
                lcd_update();
                backlight_on();
            }
        }
        else
        {
            if (external_only)
                backlight_on();
            lcd_external_capture_set(true, external_only);
        }

        framebuffer_video_active = lcd_external_capture_video_active();
        framebuffer_capture_width = LCD_WIDTH;
        framebuffer_capture_height = LCD_HEIGHT;
        framebuffer_generation = lcd_external_capture_generation();
#else
        framebuffer_capture_width = LCD_WIDTH;
        framebuffer_capture_height = LCD_HEIGHT;
        framebuffer_video_active = false;
        framebuffer_generation = 0;
#endif
        framebuffer_request_id = get_be32(message + 5);
        framebuffer_offset = 0;
        framebuffer_unchanged = client_generation != 0 &&
                                client_generation == framebuffer_generation;
        framebuffer_active = true;
    }
    else if (message[4] == 2 && length == 6)
        framebuffer_remote_action(message[5]);
    else if (message[4] == 3 && length == 5)
    {
        framebuffer_active = false;
#if defined(IPOD_6G)
        lcd_external_capture_set(false, false);
        if (framebuffer_external_only)
        {
            framebuffer_external_only = false;
            lcd_update();
            backlight_on();
        }
#endif
    }
}

static int framebuffer_copy_pixels(unsigned char *destination,
                                   uint32_t offset, int capacity)
{
    uint32_t pixel = offset / sizeof(fb_data);
    int copied = 0;

    const fb_data *frame = FBADDR(0, 0);
    uint32_t total_pixels = (uint32_t)framebuffer_capture_width *
                            framebuffer_capture_height;

    /* Native rows are contiguous, so the capture is one flat array: resolve
     * the base once and copy the strip in a single go. */
    if (pixel < total_pixels && capacity >= (int)sizeof(fb_data))
    {
        uint32_t count = MIN((uint32_t)(capacity / (int)sizeof(fb_data)),
                             total_pixels - pixel);

        memcpy(destination, frame + pixel, count * sizeof(fb_data));
        copied = count * sizeof(fb_data);
    }
    return copied;
}

/* PackBits-style RGB565 encoding.  Literal tokens 0..127 are followed by
 * token+1 pixels; repeat tokens 128..255 are followed by one pixel repeated
 * (token&127)+2 times.  The output offset remains an uncompressed byte
 * offset, so every datagram can be decoded independently. */
static int framebuffer_pack_pixels(unsigned char *destination,
                                   uint32_t offset, int capacity,
                                   uint32_t *consumed_bytes)
{
    const fb_data *frame = FBADDR(0, 0);
    uint32_t pixel = offset / sizeof(fb_data);
    const uint32_t total_pixels = framebuffer_capture_width *
                                  framebuffer_capture_height;
    int written = 0;

    /* Reaching each pixel through FBADDR() cost a software divide and an
     * indirect get_address_fn() call on every comparison of the run scan.
     * On a divide-less ARM926 that dominated the cost of a frame. */
    *consumed_bytes = 0;
    while (pixel < total_pixels && capacity - written >= 3)
    {
        fb_data value = frame[pixel];
        uint32_t run = 1;

        while (run < 129 && pixel + run < total_pixels &&
               frame[pixel + run] == value)
            ++run;

        if (run >= 3)
        {
            destination[written++] = 0x80 | (run - 2);
            destination[written++] = value;
            destination[written++] = value >> 8;
            pixel += run;
            *consumed_bytes += run * sizeof(fb_data);
        }
        else
        {
            uint32_t literal_start = pixel;
            int max_literal = MIN(128, (capacity - written - 1) / 2);
            int literal = 0;

            while (literal < max_literal && pixel < total_pixels)
            {
                value = frame[pixel];
                run = 1;
                while (run < 3 && pixel + run < total_pixels &&
                       frame[pixel + run] == value)
                    ++run;
                if (run >= 3 && literal > 0)
                    break;
                ++literal;
                ++pixel;
            }
            if (literal == 0)
                break;
            destination[written++] = literal - 1;
            for (int i = 0; i < literal; ++i)
            {
                value = frame[literal_start + i];
                destination[written++] = value;
                destination[written++] = value >> 8;
            }
            *consumed_bytes += literal * sizeof(fb_data);
        }
    }
    return written;
}

static int framebuffer_build_packet(unsigned char *packet, uint32_t offset,
                                    uint32_t *next_offset)
{
    const uint32_t total = framebuffer_capture_width *
                           framebuffer_capture_height * sizeof(fb_data);
    int payload;
    uint32_t consumed;
    bool packed = false;

    if (framebuffer_unchanged)
    {
        memcpy(packet, FRAMEBUFFER_MAGIC, 4);
        packet[4] = 0x12;
        packet[5] = 1;
        put_be16(packet + 6, framebuffer_capture_width);
        put_be16(packet + 8, framebuffer_capture_height);
        put_be16(packet + 10, framebuffer_generation);
        put_be32(packet + 12, framebuffer_request_id);
        put_be32(packet + 16, 0);
        put_be32(packet + 20, 0);
        *next_offset = offset;
        return FRAMEBUFFER_HEADER_BYTES;
    }

    consumed = MIN((uint32_t)FRAMEBUFFER_PAYLOAD_BYTES,
                   total - offset);
    payload = 0;

    /* Pack video frames too.  Letterbox bars and static chrome collapse to
     * almost nothing, and a strip that fails to compress falls back to raw,
     * so this is lossless and never costs wire bytes.  Packing straight into
     * the datagram means a rejected pass is simply overwritten below. */
    {
        uint32_t packed_consumed;
        int packed_size = framebuffer_pack_pixels(
            packet + FRAMEBUFFER_HEADER_BYTES, offset,
            FRAMEBUFFER_PAYLOAD_BYTES, &packed_consumed);

        if (packed_size > 0 && packed_consumed > consumed)
        {
            payload = packed_size;
            consumed = packed_consumed;
            packed = true;
        }
    }
    if (!packed)
    {
        payload = framebuffer_copy_pixels(
            packet + FRAMEBUFFER_HEADER_BYTES, offset, consumed);
        consumed = payload;
    }

    memcpy(packet, FRAMEBUFFER_MAGIC, 4);
    /* The video bit is independent of the packed bit, so every packet of a
     * frame agrees on whether it is video however the strips compressed and
     * whatever order the datagrams arrive in. */
    packet[4] = framebuffer_video_active ? (packed ? 0x14 : 0x13) :
                (packed ? 0x11 : 0x10);
    packet[5] = 1; /* little-endian RGB565 */
    put_be16(packet + 6, framebuffer_capture_width);
    put_be16(packet + 8, framebuffer_capture_height);
    put_be16(packet + 10, framebuffer_generation);
    put_be32(packet + 12, framebuffer_request_id);
    put_be32(packet + 16, total);
    put_be32(packet + 20, offset);

    if (payload <= 0)
        return 0;
    *next_offset = offset + consumed;
    return FRAMEBUFFER_HEADER_BYTES + payload;
}

static void framebuffer_stream_batch(void)
{
    const uint32_t total = framebuffer_capture_width *
                           framebuffer_capture_height * sizeof(fb_data);
    struct usb_ethernet_udp_datagram datagrams[FRAMEBUFFER_BATCH_PACKETS];
    /* Size the batch to what one transmit block currently holds.  Asking for
     * more than fits fails the whole batch, which would stall the stream for
     * as long as the host kept that block size. */
    const int batch_limit = MIN(FRAMEBUFFER_BATCH_PACKETS,
                                usb_ethernet_udp_batch_capacity(
                                    FRAMEBUFFER_HEADER_BYTES +
                                    FRAMEBUFFER_PAYLOAD_BYTES));
    uint32_t next_offset = framebuffer_offset;
    int count = 0;

    while (count < batch_limit &&
           (framebuffer_unchanged || next_offset < total))
    {
        unsigned char *packet =
            (unsigned char *)framebuffer_packet_words[count];
        int length = framebuffer_build_packet(packet, next_offset,
                                              &next_offset);

        if (length <= 0)
            break;
        datagrams[count].data = packet;
        datagrams[count].length = length;
        ++count;
        if (framebuffer_unchanged)
            break;
    }
    if (count == 0)
    {
        framebuffer_active = false;
        return;
    }
    if (usb_ethernet_udp_send_batch(USB_INTERNET_FRAMEBUFFER_PORT,
                                    datagrams, count) < 0)
        return;
    framebuffer_offset = next_offset;
    if (framebuffer_unchanged || framebuffer_offset >= total)
        framebuffer_active = false;
}

static void framebuffer_worker(void)
{
    while (true)
    {
        int length;

        if (!ethernet_companion_mode() || !usb_ethernet_link_active())
        {
            framebuffer_active = false;
#if defined(IPOD_6G)
            lcd_external_capture_set(false, false);
            if (framebuffer_external_only)
            {
                framebuffer_external_only = false;
                lcd_update();
                backlight_on();
            }
#endif
            sleep(HZ / 10 > 0 ? HZ / 10 : 1);
            continue;
        }

        while ((length = usb_ethernet_udp_receive(
                    USB_INTERNET_FRAMEBUFFER_PORT,
                    framebuffer_request_message,
                    sizeof(framebuffer_request_message))) > 0)
            framebuffer_handle_request(framebuffer_request_message, length);

        if (framebuffer_active)
        {
            uint32_t previous = framebuffer_offset;
            framebuffer_stream_batch();
            if (!framebuffer_active || framebuffer_offset != previous)
            {
                /* wait_for_tx() already blocks until the controller has the
                 * block; yielding again cost a second scheduler round trip
                 * per block, and at background priority against a running
                 * decoder each trip is most of a timeslice. */
                usb_ethernet_wait_for_tx(HZ / 4);
            }
            else
                sleep(1);
        }
        else
            sleep(1);
    }
}

static void framebuffer_worker_init(void)
{
    if (framebuffer_worker_id != 0)
        return;
    framebuffer_worker_id = create_thread(
        framebuffer_worker, framebuffer_worker_stack,
        sizeof(framebuffer_worker_stack), 0, "usb frame"
        IF_PRIO(, PRIORITY_BACKGROUND) IF_COP(, CPU));
}
#endif

static bool sync_path_allowed(const char *path)
{
    static const char * const roots[] = {
        "/Photos/",
        "/Music/RockPodLink/",
        "/Playlists/RockPodLink/",
        "/Videos/RockPodLink/",
        "/Videos/TV Shows/",
        "/Videos/Movies/",
        "/Videos/Home Videos/",
        "/gameboy/",
        ROCKBOX_DIR "/roms/snes/",
        ROCKBOX_DIR "/games/smsgg/roms/",
        ROCKBOX_DIR "/games/gwatch/roms/",
        ROCKBOX_DIR "/games/genesis/roms/",
        ROCKBOX_DIR "/games/library/covers/",
        ROCKBOX_DIR "/maps/",
        ROCKBOX_DIR "/videolist/",
        SYNC_PHONE_DIR "/",
    };

    if (!path || path[0] != '/' || strstr(path, "..") ||
        strchr(path, '\\') || strlen(path) >= MAX_PATH - 10)
        return false;
    for (unsigned int i = 0; i < ARRAYLEN(roots); i++)
    {
        if (!strncmp(path, roots[i], strlen(roots[i])) &&
            path[strlen(roots[i])] != '\0')
            return true;
    }
    return false;
}

/* Phone photo imports are a flat library.  Older companions briefly used a
 * RockPodLink subdirectory, so canonicalize every ordinary image upload to
 * the root of /Photos.  Thumbnail sidecars retain their hidden cache folder. */
static void sync_canonical_path(const char *path, char *destination,
                                size_t destination_size)
{
    const char *name;

    if (strncmp(path, "/Photos/", 8))
    {
        strmemccpy(destination, path, destination_size);
        return;
    }
    name = strrchr(path, '/');
    name = name ? name + 1 : path;
    if (!strncmp(path, "/Photos/.photo_thumbs/", 22))
        snprintf(destination, destination_size,
                 "/Photos/.photo_thumbs/%s", name);
    else
        snprintf(destination, destination_size, "/Photos/%s", name);
}

static bool sync_make_parents(const char *path)
{
    char directory[MAX_PATH];

    strmemccpy(directory, path, sizeof(directory));
    for (char *cursor = directory + 1; *cursor; cursor++)
    {
        if (*cursor != '/')
            continue;
        *cursor = '\0';
        if (mkdir(directory) < 0 && !dir_exists(directory))
            return false;
        *cursor = '/';
    }
    return true;
}

static void sync_abort(void)
{
    if (sync_fd >= 0)
        close(sync_fd);
    sync_fd = -1;
    if (sync_temp_path[0])
        remove(sync_temp_path);
    sync_request = 0;
    sync_expected_size = 0;
    sync_received = 0;
    sync_final_path[0] = '\0';
    sync_temp_path[0] = '\0';
}

static void sync_ack(uint32_t request, uint32_t offset, unsigned int status)
{
    unsigned char acknowledgement[14] = {
        'R', 'P', 'S', '1', USB_INTERNET_SYNC_ACK
    };
    put_be32(acknowledgement + 5, request);
    put_be32(acknowledgement + 9, offset);
    acknowledgement[13] = status;
    usb_internet_send(USB_INTERNET_SYNC_PORT, acknowledgement,
                      sizeof(acknowledgement));
}

static const char *sync_mode_name(void)
{
#ifdef SIMULATOR
    return "Simulator";
#else
    switch (global_settings.usb_mode)
    {
#ifdef USB_ENABLE_ETHERNET
        case USB_MODE_INTERNET:
            return "iPhone USB Link";
#ifdef USB_ENABLE_IPHETH_HOST
        case USB_MODE_IPHONE_TETHER:
            return "iPhone Tether Host";
#endif
#endif
        case USB_MODE_MASS_STORAGE:
            return "Storage";
        case USB_MODE_CHARGE:
            return "Charge Only";
        default:
            return "USB";
    }
#endif
}

static void sync_storage_flush(void)
{
#if !defined(SIMULATOR) && defined(HAVE_STORAGE_FLUSH)
    storage_flush();
#endif
}

static void sync_send_status(void)
{
    sector_t size = 0;
    sector_t free = 0;
#ifndef SIMULATOR
    struct storage_info storage = { 0 };
#endif
    unsigned long long disk_bytes = 0;
    struct mp3entry *id3 = audio_current_track();
    char *text = (char *)sync_response + 5;
    int length;

    memcpy(sync_response, SYNC_MAGIC, 4);
    sync_response[4] = USB_INTERNET_SYNC_STATUS;
    volume_size(IF_MV(0,) &size, &free);
#ifndef SIMULATOR
    storage_get_info(0, &storage);
    if (storage.sector_size > 0 && storage.num_sectors > 0)
        disk_bytes = (unsigned long long)storage.sector_size *
                     (unsigned long long)storage.num_sectors;
#endif
    length = snprintf(text, sizeof(sync_response) - 5,
        "name=%s\nfirmware=%s\nmode=%s\nbattery=%d\n"
        "disk_bytes=%llu\nvolume_bytes=%llu\ncapacity_bytes=%llu\n"
        "free_bytes=%llu\nplaying=%d\n"
        "track=%s\nartist=%s\nsitekick=%d\n"
        /* The companion can name a track but had no way to fetch it: the
         * status carried no file path, so "play what the iPod is playing"
         * was impossible.  Send the path plus position so the phone can pull
         * the same file over the existing media request and follow along. */
        "track_path=%s\nelapsed_ms=%lu\nlength_ms=%lu\n",
        MODEL_NAME, rbversion, sync_mode_name(), battery_level(),
        disk_bytes,
        (unsigned long long)size * 1024ull,
        (unsigned long long)size * 1024ull,
        (unsigned long long)free * 1024ull,
        !!(audio_status() & AUDIO_STATUS_PLAY),
        id3 && id3->title ? id3->title : "",
        id3 && id3->artist ? id3->artist : "",
        file_exists(SYNC_SITEKICK_PREVIEW),
        id3 && id3->path[0] ? id3->path : "",
        id3 ? id3->elapsed : 0UL,
        id3 ? id3->length : 0UL);
    if (length > 0)
        usb_internet_send(USB_INTERNET_SYNC_PORT, sync_response,
                          MIN(length + 5, (int)sizeof(sync_response)));
}

static void sync_send_sitekick(uint32_t request, uint32_t offset)
{
    unsigned char header[12] = { 'R', 'P', 'S', 'K' };
    uint32_t pane_size = 0;
    uint32_t float_size = 0;
    uint32_t total = 0;
    int pane = open(SYNC_SITEKICK_PANE, O_RDONLY);
    int character = open(SYNC_SITEKICK_PREVIEW, O_RDONLY);
    int count = 0;

    memcpy(sync_response, SYNC_MAGIC, 4);
    sync_response[4] = USB_INTERNET_SYNC_SITEKICK_DATA;
    put_be32(sync_response + 5, request);
    if (pane >= 0 && character >= 0)
    {
        off_t pane_length = filesize(pane);
        off_t float_length = filesize(character);
        if (pane_length > 0 && float_length > 0 &&
            (uint64_t)pane_length + (uint64_t)float_length + 12 <= UINT32_MAX)
        {
            pane_size = pane_length;
            float_size = float_length;
            total = 12 + pane_size + float_size;
        }
    }
    put_be32(header + 4, pane_size);
    put_be32(header + 8, float_size);
    if (offset < total)
    {
        uint32_t cursor = offset;
        uint32_t room = MIN((uint32_t)sizeof(sync_response) - 17,
                            total - offset);
        while (room > 0)
        {
            int got = 0;
            if (cursor < sizeof(header))
            {
                got = MIN(room, (uint32_t)sizeof(header) - cursor);
                memcpy(sync_response + 17 + count, header + cursor, got);
            }
            else if (cursor < sizeof(header) + pane_size)
            {
                uint32_t file_offset = cursor - sizeof(header);
                got = MIN(room, pane_size - file_offset);
                if (lseek(pane, file_offset, SEEK_SET) < 0)
                    got = 0;
                else
                    got = read(pane, sync_response + 17 + count, got);
            }
            else
            {
                uint32_t file_offset = cursor - sizeof(header) - pane_size;
                got = MIN(room, float_size - file_offset);
                if (lseek(character, file_offset, SEEK_SET) < 0)
                    got = 0;
                else
                    got = read(character, sync_response + 17 + count, got);
            }
            if (got <= 0)
                break;
            count += got;
            cursor += got;
            room -= got;
        }
    }
    if (pane >= 0)
        close(pane);
    if (character >= 0)
        close(character);
    put_be32(sync_response + 9, total);
    put_be32(sync_response + 13, offset);
    usb_internet_send(USB_INTERNET_SYNC_PORT, sync_response, 17 + count);
}

#ifdef HAVE_TAGCACHE
static void sync_catalog_clean(char *text)
{
    while (*text)
    {
        if (*text == '\t' || *text == '\r' || *text == '\n')
            *text = ' ';
        text++;
    }
}

static void sync_catalog_tag(struct tagcache_search *tcs, int idxid, int tag,
                             char *buffer, size_t size)
{
    if (!tagcache_retrieve(tcs, idxid, tag, buffer, size))
        buffer[0] = '\0';
    sync_catalog_clean(buffer);
}

/* A host deploy generates this transport cache directly from the .tcd files
 * in about a second. Never block the live USB relay rebuilding a valid cache
 * merely because FAT mtimes make the database appear newer; serve it
 * immediately and refresh it at the next database-preserving deploy. A
 * missing cache still uses the firmware fallback below. */
static bool sync_library_catalog_current(void)
{
    char header[32];
    int input;

    input = open(SYNC_LIBRARY_CATALOG, O_RDONLY);
    if (input < 0)
        return false;
    header[0] = '\0';
    read_line(input, header, sizeof(header));
    close(input);
    return !strcmp(header, "# rockpod-library-v1");
}

/* Building the catalog walks the whole tagcache from the same service point
 * that answers the companion, so a large library goes silent on the wire for
 * several seconds and the phone declares the link dead.  Emit the ordinary
 * status reply about once a second while the walk runs: the companion treats
 * status traffic as proof of life and does not charge it against the retry
 * budget of the catalog request it is already waiting on.  sync_response is
 * free here because sync_send_library only fills it after the build returns. */
static long catalog_keepalive_deadline;

static void sync_catalog_keepalive(void)
{
    if (TIME_BEFORE(current_tick, catalog_keepalive_deadline))
        return;
    catalog_keepalive_deadline = current_tick + HZ;
    sync_send_status();
}

static bool sync_build_library_catalog(void)
{
    struct tagcache_search tcs;
    char path[MAX_PATH];
    char title[TAGCACHE_BUFSZ];
    char artist[TAGCACHE_BUFSZ];
    char album[TAGCACHE_BUFSZ];
    char albumartist[TAGCACHE_BUFSZ];
    char genre[TAGCACHE_BUFSZ];
    int output;
    int count = 0;
    int videos = 0;
    bool ok = false;

    if (!tagcache_is_fully_initialized())
        return false;
    catalog_keepalive_deadline = current_tick + HZ;
    sync_make_parents(SYNC_LIBRARY_CATALOG);
    output = open(SYNC_LIBRARY_CATALOG ".rpspart",
                  O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (output < 0)
        return false;
    if (fdprintf(output,
        "# rockpod-library-v1\n"
        "path\ttitle\tartist\talbum\talbum_artist\tgenre\tyear\tdisc\ttrack\t"
        "length_ms\tbitrate_kbps\tplay_count\tplay_time_ms\tlast_played\t"
        "rating\tlast_elapsed_ms\tlast_offset\tmtime\tartwork_path\n") < 0)
        goto finish;
    if (!tagcache_search(&tcs, tag_filename))
        goto finish;
    while (tagcache_get_next(&tcs, path, sizeof(path)))
    {
        int idxid = tcs.idx_id;
        sync_catalog_clean(path);
        sync_catalog_tag(&tcs, idxid, tag_title, title, sizeof(title));
        sync_catalog_tag(&tcs, idxid, tag_artist, artist, sizeof(artist));
        sync_catalog_tag(&tcs, idxid, tag_album, album, sizeof(album));
        sync_catalog_tag(&tcs, idxid, tag_albumartist, albumartist,
                         sizeof(albumartist));
        sync_catalog_tag(&tcs, idxid, tag_genre, genre, sizeof(genre));
        if (fdprintf(output,
            "%s\t%s\t%s\t%s\t%s\t%s\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t"
            "%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t\n",
            path, title, artist, album, albumartist, genre,
            tagcache_get_numeric(&tcs, tag_year),
            tagcache_get_numeric(&tcs, tag_discnumber),
            tagcache_get_numeric(&tcs, tag_tracknumber),
            tagcache_get_numeric(&tcs, tag_length),
            tagcache_get_numeric(&tcs, tag_bitrate),
            tagcache_get_numeric(&tcs, tag_playcount),
            tagcache_get_numeric(&tcs, tag_playtime),
            tagcache_get_numeric(&tcs, tag_lastplayed),
            tagcache_get_numeric(&tcs, tag_rating),
            tagcache_get_numeric(&tcs, tag_lastelapsed),
            tagcache_get_numeric(&tcs, tag_lastoffset),
            tagcache_get_numeric(&tcs, tag_mtime)) < 0)
            break;
        count++;
        if ((count & 0x1f) == 0)
        {
            yield();
            sync_catalog_keepalive();
        }
    }
    tagcache_search_finish(&tcs);
    {
        int video = open(SYNC_VIDEO_INDEX, O_RDONLY);
        char line[1024];
        while (video >= 0 && read_line(video, line, sizeof(line)) > 0)
        {
            if (!line[0] || line[0] == '#' || !strncmp(line, "video_id\t", 9))
                continue;
            if (fdprintf(output, "@video\t%s\n", line) < 0)
                break;
            videos++;
            if ((videos & 0x1f) == 0)
                sync_catalog_keepalive();
        }
        if (video >= 0) close(video);
    }
    ok = count > 0 || videos > 0;
finish:
    close(output);
    if (!ok)
    {
        remove(SYNC_LIBRARY_CATALOG ".rpspart");
        return false;
    }
    remove(SYNC_LIBRARY_CATALOG);
    return rename(SYNC_LIBRARY_CATALOG ".rpspart",
                  SYNC_LIBRARY_CATALOG) >= 0;
}

static bool sync_file_path_allowed(const char *path)
{
#ifdef HAVE_TAGCACHE
    struct tagcache_search tcs;
    bool found;
    const char *name;
    if (!path || path[0] != '/' || strstr(path, "..") || strchr(path, '\\'))
        return false;
    /* Album artwork is not a tagcache track, but it is safe to expose the
     * conventional cover sidecar in a Music album directory. */
    name = strrchr(path, '/');
    name = name ? name + 1 : path;
    if (!strncmp(path, "/Music/", 7) &&
        (!strcasecmp(name, "cover.jpg") ||
         !strcasecmp(name, "cover.jpeg") ||
         !strcasecmp(name, "cover.png") ||
         !strcasecmp(name, "cover.bmp") ||
         !strcasecmp(name, "cover.51x51.bmp") ||
         !strcasecmp(name, "folder.jpg") ||
         !strcasecmp(name, "folder.png")))
        return true;
    found = tagcache_is_fully_initialized() && tagcache_find_index(&tcs, path);
    if (found) tagcache_search_finish(&tcs);
    return found;
#else
    (void)path;
    return false;
#endif
}

static void sync_send_file(uint32_t request, uint32_t offset, const char *path)
{
    uint32_t total = 0;
    int input = -1;
    int count = 0;
    if (sync_file_path_allowed(path)) input = open(path, O_RDONLY);
    if (input >= 0)
    {
        off_t length = filesize(input);
        if (length > 0 && (uint64_t)length <= UINT32_MAX)
            total = (uint32_t)length;
    }
    memcpy(sync_response, SYNC_MAGIC, 4);
    sync_response[4] = USB_INTERNET_SYNC_FILE_DATA;
    put_be32(sync_response + 5, request);
    put_be32(sync_response + 9, total);
    put_be32(sync_response + 13, offset);
    if (input >= 0 && offset < total && lseek(input, offset, SEEK_SET) >= 0)
        count = read(input, sync_response + 17,
            MIN((uint32_t)(sizeof(sync_response) - 17), total - offset));
    if (input >= 0) close(input);
    usb_internet_send(USB_INTERNET_SYNC_PORT, sync_response,
                      17 + MAX(count, 0));
}
#else
static bool sync_build_library_catalog(void)
{
    return false;
}
#endif

static void sync_send_library(uint32_t request, uint32_t offset)
{
    uint32_t total = 0;
    int input;
    int count = 0;

    if (request != sync_library_request)
    {
        sync_library_request = request;
        if (!sync_library_catalog_current() &&
            !sync_build_library_catalog())
            remove(SYNC_LIBRARY_CATALOG);
    }
    input = open(SYNC_LIBRARY_CATALOG, O_RDONLY);
    if (input >= 0)
    {
        off_t length = filesize(input);
        if (length > 0 && (uint64_t)length <= UINT32_MAX)
            total = (uint32_t)length;
    }
    memcpy(sync_response, SYNC_MAGIC, 4);
    sync_response[4] = USB_INTERNET_SYNC_LIBRARY_DATA;
    put_be32(sync_response + 5, request);
    put_be32(sync_response + 9, total);
    put_be32(sync_response + 13, offset);
    if (input >= 0 && offset < total && lseek(input, offset, SEEK_SET) >= 0)
        count = read(input, sync_response + 17,
                     MIN((uint32_t)(sizeof(sync_response) - 17),
                         total - offset));
    if (input >= 0)
        close(input);
    usb_internet_send(USB_INTERNET_SYNC_PORT, sync_response,
                      17 + MAX(count, 0));
}

static bool sync_register_video_row(void)
{
    char id[32];
    char *tab;
    int source = open(SYNC_VIDEO_ROW, O_RDONLY);
    int index;
    int length;

    if (source < 0)
        return false;
    length = read(source, sync_video_line, sizeof(sync_video_line) - 1);
    close(source);
    if (length <= 0 || length >= (int)sizeof(sync_video_line) - 1)
        return false;
    sync_video_line[length] = '\0';
    while (length > 0 && (sync_video_line[length - 1] == '\r' ||
                          sync_video_line[length - 1] == '\n'))
        sync_video_line[--length] = '\0';
    if (strchr(sync_video_line, '\n') || strchr(sync_video_line, '\r'))
        return false;
    tab = strchr(sync_video_line, '\t');
    if (!tab || tab == sync_video_line || tab - sync_video_line >= (int)sizeof(id))
        return false;
    memcpy(id, sync_video_line, tab - sync_video_line);
    id[tab - sync_video_line] = '\0';
    if (!strstr(sync_video_line, "\tVideos/RockPodLink/") &&
        !strstr(sync_video_line, "\tVideos/TV Shows/") &&
        !strstr(sync_video_line, "\tVideos/Movies/") &&
        !strstr(sync_video_line, "\tVideos/Home Videos/"))
        return false;

    index = open(SYNC_VIDEO_INDEX, O_RDONLY);
    while (index >= 0 && read_line(index, sync_response,
                                    sizeof(sync_response)) > 0)
    {
        if (!strncmp((char *)sync_response, id, strlen(id)) &&
            sync_response[strlen(id)] == '\t')
        {
            close(index);
            return true;
        }
    }
    if (index >= 0)
        close(index);
    mkdir(ROCKBOX_DIR "/videolist");
    index = open(SYNC_VIDEO_INDEX, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (index < 0)
        return false;
    if (filesize(index) == 0)
    {
        static const char header[] =
            "# rockpod videolist v6\n"
            "video_id\tthumb\tpreview\ttitle\tkind\tgroup_key\tdevice_path\t"
            "show\tseason\tepisode\tduration\tlocked\tyear\tgenre\trating\t"
            "plot_short\tplot_long\tcontent_rating\tnetflix_poster\t"
            "netflix_detail\tshow_art_id\tseason_art_id\tshow_plot\n";
        if (write(index, header, sizeof(header) - 1) != sizeof(header) - 1)
        {
            close(index);
            return false;
        }
    }
    sync_video_line[length++] = '\n';
    bool ok = write(index, sync_video_line, length) == length;
    close(index);
    return ok;
}

static bool sync_register_game_row(void)
{
    char row[1024];
    char line[1024];
    char filename[MAX_PATH];
    char temp[MAX_PATH];
    char *field;
    char *end;
    char saved_end = '\0';
    int source = open(SYNC_GAME_ROW, O_RDONLY);
    int input = -1;
    int output;
    int length;
    bool ok = true;

    if (source < 0)
        return false;
    length = read(source, row, sizeof(row) - 1);
    close(source);
    if (length <= 0 || length >= (int)sizeof(row) - 1)
        return false;
    row[length] = '\0';
    while (length > 0 && (row[length - 1] == '\r' || row[length - 1] == '\n'))
        row[--length] = '\0';
    if (strchr(row, '\n') || strchr(row, '\r'))
        return false;

    /* The ROM filename is in plugin_param for multi-system launchers and in
     * rom_path for Rockboy/NES. */
    field = strrchr(row, '\t');
    if (!field || !field[1])
    {
        field = strchr(row, '\t');
        if (!field)
            return false;
        field++;
        end = strchr(field, '\t');
        if (!end)
            return false;
        saved_end = *end;
        *end = '\0';
    }
    else
        field++;
    field = strrchr(field, '/');
    field = field ? field + 1 : field;
    strmemccpy(filename, field, sizeof(filename));
    if (saved_end)
        *end = saved_end;
    if (!filename[0] || strchr(filename, '\t'))
        return false;

    sync_make_parents(SYNC_GAME_INDEX);
    snprintf(temp, sizeof(temp), "%s.rpspart", SYNC_GAME_INDEX);
    input = open(SYNC_GAME_INDEX, O_RDONLY);
    output = open(temp, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (output < 0)
    {
        if (input >= 0)
            close(input);
        return false;
    }
    if (input < 0)
    {
        static const char header[] =
            "# Rockboy launcher index\n"
            "# Format:\n"
            "# title\\trom_path\\tcover_path\\tfavorite\\tsave_hint\\t"
            "year\\tgenre\\tpublisher\\tdeveloper\\tdescription\\tplugin_param\n";
        ok = write(output, header, sizeof(header) - 1) == sizeof(header) - 1;
    }
    while (ok && input >= 0 && read_line(input, line, sizeof(line)) > 0)
    {
        char needle[MAX_PATH + 2];
        snprintf(needle, sizeof(needle), "/%s", filename);
        if (strstr(line, needle))
            continue;
        ok = fdprintf(output, "%s\n", line) >= 0;
    }
    if (input >= 0)
        close(input);
    if (ok)
        ok = write(output, row, length) == length &&
             write(output, "\n", 1) == 1;
    close(output);
    if (!ok)
    {
        remove(temp);
        return false;
    }
    remove(SYNC_GAME_INDEX);
    return rename(temp, SYNC_GAME_INDEX) >= 0;
}

static bool sync_apply_audiobook_positions(void)
{
#ifdef HAVE_TAGCACHE
    struct tagcache_search tcs;
    char line[MAX_PATH + 96];
    int input = open(SYNC_AUDIOBOOK_POSITIONS, O_RDONLY);
    int applied = 0;

    if (input < 0 || !tagcache_is_fully_initialized())
    {
        if (input >= 0)
            close(input);
        return false;
    }
    while (read_line(input, line, sizeof(line)) > 0)
    {
        char *elapsed_text;
        char *offset_text;
        char *end;
        long elapsed;
        long offset;

        if (!line[0] || line[0] == '#')
            continue;
        elapsed_text = strchr(line, '\t');
        if (!elapsed_text)
            continue;
        *elapsed_text++ = '\0';
        offset_text = strchr(elapsed_text, '\t');
        if (!offset_text)
            continue;
        *offset_text++ = '\0';
        end = strchr(offset_text, '\t');
        if (end)
            *end = '\0';
        elapsed = strtol(elapsed_text, &end, 10);
        if (!end || *end || elapsed < 0)
            continue;
        offset = strtol(offset_text, &end, 10);
        if (!end || *end || offset < 0)
            continue;
        if (line[0] != '/' || strstr(line, "..") || strchr(line, '\\') ||
            !tagcache_find_index(&tcs, line))
            continue;
        tagcache_update_numeric(tcs.idx_id, tag_lastelapsed, elapsed);
        tagcache_update_numeric(tcs.idx_id, tag_lastoffset, offset);
        applied++;
        tagcache_search_finish(&tcs);
        if ((applied & 0xf) == 0)
            yield();
    }
    close(input);
    return applied > 0;
#else
    return false;
#endif
}

static bool sync_apply_notification_inbox(void)
{
    char line[NOTIFICATION_TITLE_SIZE + NOTIFICATION_BODY_SIZE + 80];
    int input = open(SYNC_NOTIFICATION_INBOX, O_RDONLY);
    int posted = 0;
    if (input < 0)
        return false;
    while (read_line(input, line, sizeof(line)) > 0)
    {
        struct notification_request request;
        char *stable_text, *title, *body, *end;
        unsigned long stable;
        if (!line[0] || line[0] == '#')
            continue;
        stable_text = line;
        title = strchr(stable_text, '\t');
        if (!title) continue;
        *title++ = '\0';
        body = strchr(title, '\t');
        if (!body) continue;
        *body++ = '\0';
        stable = strtoul(stable_text, &end, 16);
        if (!end || *end || !stable || !title[0] || !body[0])
            continue;
        memset(&request, 0, sizeof(request));
        request.source = NOTIFICATION_SOURCE_MUSIC;
        request.kind = !strncmp(title, "Concert", 7) ?
            NOTIFICATION_MUSIC_CONCERT_ALERT : NOTIFICATION_MUSIC_NEW_RELEASE;
        request.priority = 1;
        request.stable_id = (uint32_t)stable;
        strmemccpy(request.title, title, sizeof(request.title));
        strmemccpy(request.body, body, sizeof(request.body));
        strmemccpy(request.route, "music", sizeof(request.route));
        notification_post(&request);
        posted++; /* duplicates are valid: the manager performs stable-ID dedup */
    }
    close(input);
    return posted > 0;
}

static bool sync_apply_cleanup_manifest(void)
{
    char path[MAX_PATH];
    int input = open(SYNC_CLEANUP_MANIFEST, O_RDONLY);
    int valid = 0;
    if (input < 0) return false;
    while (read_line(input, path, sizeof(path)) > 0)
    {
        if (!path[0] || path[0] == '#') continue;
        if (strncmp(path, "/Music/RockPodLink/", 19) || strstr(path, "..") ||
            strchr(path, '\\')) continue;
        remove(path); /* files only; directories are never accepted */
        valid++;
    }
    close(input);
    return valid > 0;
}

static void handle_sync_message(const unsigned char *message, int length)
{
    unsigned int type;
    uint32_t request;

    if (length < 5 || memcmp(message, SYNC_MAGIC, 4))
        return;
    type = message[4];
    if (type == USB_INTERNET_SYNC_PROBE)
    {
        sync_send_status();
        return;
    }
    if (length < 9)
        return;
    request = get_be32(message + 5);
    if (type == USB_INTERNET_SYNC_BEGIN && length > 17)
    {
        uint32_t size = get_be32(message + 9);
        const char *path = (const char *)message + 17;
        const char *path_end = memchr(path, '\0', length - 17);
        size_t path_length = path_end ? (size_t)(path_end - path) : 0;

        sync_abort();
        if (!size || size > SYNC_MAX_FILE || path_length == 0 ||
            !path_end || path_length >= (size_t)(length - 17) ||
            !sync_path_allowed(path))
        {
            sync_ack(request, 0, 1);
            return;
        }
        sync_canonical_path(path, sync_final_path,
                            sizeof(sync_final_path));
        if (!sync_make_parents(sync_final_path))
        {
            sync_ack(request, 0, 1);
            return;
        }
        snprintf(sync_temp_path, sizeof(sync_temp_path), "%s.rpspart",
                 sync_final_path);
        sync_fd = open(sync_temp_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (sync_fd < 0)
        {
            sync_abort();
            sync_ack(request, 0, 2);
            return;
        }
        sync_request = request;
        sync_expected_size = size;
        sync_expected_crc = get_be32(message + 13);
        sync_crc = 0xffffffff;
        sync_received = 0;
        sync_ack(request, 0, 0);
    }
    else if (type == USB_INTERNET_SYNC_DATA && length > 13 &&
             sync_fd >= 0 && request == sync_request)
    {
        uint32_t offset = get_be32(message + 9);
        int payload = length - 13;
        if (offset < sync_received && offset + payload <= sync_received)
        {
            sync_ack(request, sync_received, 0);
            return;
        }
        if (offset != sync_received ||
            sync_received + payload > sync_expected_size ||
            write(sync_fd, message + 13, payload) != payload)
        {
            sync_ack(request, sync_received, 3);
            return;
        }
        sync_crc = crc_32(message + 13, payload, sync_crc);
        sync_received += payload;
        sync_ack(request, sync_received, 0);
    }
    else if (type == USB_INTERNET_SYNC_COMMIT && length == 9 &&
             sync_fd >= 0 && request == sync_request)
    {
        bool valid = sync_received == sync_expected_size &&
                     (!sync_expected_crc || sync_crc == sync_expected_crc);
        close(sync_fd);
        sync_fd = -1;
        if (valid)
        {
            remove(sync_final_path);
            valid = rename(sync_temp_path, sync_final_path) >= 0;
            if (valid && !strcmp(sync_final_path, SYNC_VIDEO_ROW))
                valid = sync_register_video_row();
            if (valid && !strcmp(sync_final_path, SYNC_GAME_ROW))
                valid = sync_register_game_row();
            if (valid && !strcmp(sync_final_path, SYNC_AUDIOBOOK_POSITIONS))
                valid = sync_apply_audiobook_positions();
            if (valid && !strcmp(sync_final_path, SYNC_NOTIFICATION_INBOX))
                valid = sync_apply_notification_inbox();
            if (valid && !strcmp(sync_final_path, SYNC_CLEANUP_MANIFEST))
                valid = sync_apply_cleanup_manifest();
            sync_storage_flush();
        }
        sync_ack(request, valid ? 0xffffffff : sync_received,
                 valid ? 0 : 4);
        sync_abort();
    }
    else if (type == USB_INTERNET_SYNC_SITEKICK_REQUEST && length == 13)
        sync_send_sitekick(request, get_be32(message + 9));
    else if (type == USB_INTERNET_SYNC_LIBRARY_REQUEST && length == 13)
        sync_send_library(request, get_be32(message + 9));
    else if (type == USB_INTERNET_SYNC_FILE_REQUEST && length > 13)
    {
        const char *path = (const char *)message + 13;
        if (memchr(path, '\0', length - 13))
            sync_send_file(request, get_be32(message + 9), path);
    }
    else if (type == USB_INTERNET_SYNC_SAFE_EJECT && length == 9)
    {
        sync_abort();
        sync_storage_flush();
        memcpy(sync_response, SYNC_MAGIC, 4);
        sync_response[4] = USB_INTERNET_SYNC_SAFE;
        put_be32(sync_response + 5, request);
        usb_internet_send(USB_INTERNET_SYNC_PORT, sync_response, 9);
    }
}

static void weather_ack(uint32_t offset)
{
    unsigned char acknowledgement[9] = {
        'R', 'P', 'I', '1', USB_INTERNET_WEATHER_ACK, 0, 0, 0, 0
    };
    put_be32(acknowledgement + 5, offset);
    usb_internet_send(USB_INTERNET_WEATHER_PORT, acknowledgement,
                      sizeof(acknowledgement));
}

static void weather_abort(void)
{
    if (weather_fd >= 0)
        close(weather_fd);
    weather_fd = -1;
    weather_expected_size = 0;
    weather_received = 0;
}

/* Whether the service point has to keep running even with nothing to do.
 *
 * The relay is only pumped from get_action(), and the main screens wait with
 * TIMEOUT_BLOCK, so an idle player that nobody is touching never answers the
 * companion at all: streamed audio stops mid-track and the phone eventually
 * calls the link dead.  This deliberately keys off the cable and the link
 * rather than usb_internet_connected(): that one needs a recently seen
 * companion, which can never happen if the service point is not running.
 */
bool usb_internet_needs_service(void)
{
#if defined(USB_ENABLE_ETHERNET) && !defined(SIMULATOR)
    if (ethernet_companion_mode() && usb_ethernet_link_active())
        return true;
#endif
#if defined(USB_ENABLE_IPHETH_HOST) && !defined(SIMULATOR)
    if (global_settings.usb_mode == USB_MODE_IPHONE_TETHER)
        return true;
#endif
    return false;
}

bool usb_internet_connected(void)
{
    bool connected = false;
#if defined(USB_ENABLE_ETHERNET) && !defined(SIMULATOR)
    connected = ethernet_companion_mode() &&
                usb_ethernet_link_active() && companion_seen &&
                /* Browser viewport and large sync transfers temporarily keep
                 * the companion busy on their own acknowledged UDP stream.
                 * Do not declare the link dead merely because the weather
                 * heartbeat socket could not be serviced during that bounded
                 * transfer. Normal two-second probes refresh this lease. */
                TIME_BEFORE(current_tick, companion_seen + 90 * HZ);
#endif
#if defined(USB_ENABLE_IPHETH_HOST) && !defined(SIMULATOR)
    connected = connected || usb_iphone_network_connected();
#endif
#if defined(USB_ENABLE_ETHERNET) && defined(SIMULATOR)
    /* A marker lets UI regression tests exercise connection-gated menus
     * without pretending the simulator has a USB controller. */
    connected = file_exists(ROCKBOX_DIR "/rockpod/usb-internet.connected");
#endif
    return connected;
}

int usb_internet_send(uint16_t port, const void *data, int length)
{
#if defined(USB_ENABLE_ETHERNET) && !defined(SIMULATOR)
    if (ethernet_companion_mode())
        return usb_ethernet_udp_send(port, data, length);
    return -1;
#else
    (void)port;
    (void)data;
    (void)length;
    return -1;
#endif
}

int usb_internet_receive(uint16_t port, void *data, int capacity)
{
#if defined(USB_ENABLE_ETHERNET) && !defined(SIMULATOR)
    if (ethernet_companion_mode())
        return usb_ethernet_udp_receive(port, data, capacity);
    return 0;
#else
    (void)port;
    (void)data;
    (void)capacity;
    return 0;
#endif
}

static void post_weather_synced(void)
{
    struct notification_request request;
    memset(&request, 0, sizeof(request));
    request.source = NOTIFICATION_SOURCE_WEATHER;
    request.kind = NOTIFICATION_WEATHER_SYNCED;
    request.priority = 1;
    request.stable_id = 0x57534649;
    strmemccpy(request.title, "Weather Updated", sizeof(request.title));
    strmemccpy(request.body, "Live forecast received over USB Internet",
               sizeof(request.body));
    strmemccpy(request.route, "weather", sizeof(request.route));
    notification_post(&request);
}

#if defined(USB_ENABLE_IPHETH_HOST) && !defined(SIMULATOR)
static void post_iphone_tether_status(enum usb_iphone_tether_state state)
{
    struct notification_request request;
    if (state != USB_IPHONE_WAITING_FOR_TRUST &&
        state != USB_IPHONE_LINK && state != USB_IPHONE_ERROR)
        return;
    memset(&request, 0, sizeof(request));
    request.source = NOTIFICATION_SOURCE_SYSTEM;
    request.kind = NOTIFICATION_SYSTEM_IPHONE_TETHER;
    request.priority = state == USB_IPHONE_ERROR ? 2 : 1;
    request.stable_id = 0x4950484e;
    strmemccpy(request.title,
               state == USB_IPHONE_LINK ? "iPhone Hotspot" :
               state == USB_IPHONE_ERROR ? "iPhone Tether Error" :
               "Trust This iPod",
               sizeof(request.title));
    strmemccpy(request.body,
               state == USB_IPHONE_WAITING_FOR_TRUST ?
               usb_iphone_preflight_status() :
               usb_iphone_tether_status(),
               sizeof(request.body));
    strmemccpy(request.route, "settings", sizeof(request.route));
    notification_post(&request);
}

static void post_iphone_route_status(bool connected)
{
    struct notification_request request;
    if (!connected)
        return;
    memset(&request, 0, sizeof(request));
    request.source = NOTIFICATION_SOURCE_SYSTEM;
    request.kind = NOTIFICATION_SYSTEM_IPHONE_TETHER;
    request.priority = 1;
    request.stable_id = 0x49505254;
    strmemccpy(request.title, "iPhone Hotspot Ready",
               sizeof(request.title));
    strmemccpy(request.body, usb_iphone_network_status(),
               sizeof(request.body));
    strmemccpy(request.route, "settings", sizeof(request.route));
    notification_post(&request);
}

static void iphone_log_stage(const char *stage, const char *status)
{
    char line[192];
    int fd = open(IPHONE_TETHER_LOG, O_WRONLY | O_CREAT | O_APPEND, 0666);
    int length;
    if (fd < 0)
        return;
    if (filesize(fd) > IPHONE_TETHER_LOG_MAX)
    {
        close(fd);
        fd = open(IPHONE_TETHER_LOG, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd < 0)
            return;
    }
    length = snprintf(line, sizeof(line), "%ld\t%s\t%s\n",
                      current_tick, stage, status ? status : "unknown");
    if (length > 0)
        write(fd, line, MIN(length, (int)sizeof(line) - 1));
    close(fd);
}

static void post_iphone_stage(const char *title, const char *body,
                              uint32_t stable_id)
{
    struct notification_request request;
    memset(&request, 0, sizeof(request));
    request.source = NOTIFICATION_SOURCE_SYSTEM;
    request.kind = NOTIFICATION_SYSTEM_IPHONE_TETHER;
    request.priority = strstr(body, "failed") || strstr(body, "timed out") ?
                       2 : 1;
    request.stable_id = stable_id;
    strmemccpy(request.title, title, sizeof(request.title));
    strmemccpy(request.body, body, sizeof(request.body));
    strmemccpy(request.route, "settings", sizeof(request.route));
    notification_post(&request);
}

static void update_iphone_stage_diagnostics(void)
{
    const char *tether = usb_iphone_tether_status();
    const char *preflight = usb_iphone_preflight_status();
    const char *network_status = usb_iphone_network_status();

    if (strcmp(previous_tether_status, tether))
    {
        strmemccpy(previous_tether_status, tether,
                   sizeof(previous_tether_status));
        iphone_log_stage("usb", tether);
    }
    if (strcmp(previous_preflight_status, preflight))
    {
        strmemccpy(previous_preflight_status, preflight,
                   sizeof(previous_preflight_status));
        iphone_log_stage("trust", preflight);
        if (strcmp(preflight, "Waiting for iPhone USB multiplexor"))
            post_iphone_stage("iPhone Trust", preflight, 0x49505452);
    }
    if (strcmp(previous_network_status, network_status))
    {
        strmemccpy(previous_network_status, network_status,
                   sizeof(previous_network_status));
        iphone_log_stage("network", network_status);
        if (strcmp(network_status, "Off"))
            post_iphone_stage("iPhone Network", network_status, 0x49504e45);
    }
}
#endif

static void handle_weather_message(const unsigned char *message, int length)
{
    unsigned int type;
    if (length < 5 || memcmp(message, "RPI1", 4))
        return;
    type = message[4];
    if (type == USB_INTERNET_WEATHER_BEGIN && length == 13)
    {
        uint32_t size = get_be32(message + 5);
        weather_abort();
        if (size == 0 || size > WEATHER_MAX_BYTES)
            return;
        mkdir(ROCKBOX_DIR "/rockpod");
        mkdir(WEATHER_DIR);
        weather_fd = open(WEATHER_TEMP, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (weather_fd < 0)
            return;
        weather_expected_size = size;
        weather_expected_crc = get_be32(message + 9);
        weather_crc = 0xffffffff;
        weather_ack(0);
    }
    else if (type == USB_INTERNET_WEATHER_DATA && length > 9 &&
             weather_fd >= 0)
    {
        uint32_t offset = get_be32(message + 5);
        int payload = length - 9;
        if (offset < weather_received &&
            offset + payload <= weather_received)
        {
            weather_ack(weather_received);
            return;
        }
        if (offset != weather_received ||
            weather_received + payload > weather_expected_size)
        {
            weather_ack(weather_received);
            return;
        }
        if (write(weather_fd, message + 9, payload) != payload)
        {
            weather_abort();
            return;
        }
        weather_crc = crc_32(message + 9, payload, weather_crc);
        weather_received += payload;
        weather_ack(weather_received);
    }
    else if (type == USB_INTERNET_WEATHER_COMMIT && length == 5 &&
             weather_fd >= 0)
    {
        bool valid = weather_received == weather_expected_size &&
                     (!weather_expected_crc ||
                      weather_crc == weather_expected_crc);
        close(weather_fd);
        weather_fd = -1;
        if (valid && rename(WEATHER_TEMP, WEATHER_FILE) >= 0)
        {
            weather_generation++;
            post_weather_synced();
        }
        weather_ack(valid ? 0xffffffff : weather_received);
        weather_expected_size = 0;
        weather_received = 0;
    }
    else if (type == USB_INTERNET_WEATHER_COMMIT && length == 5 &&
             weather_fd < 0 && weather_generation > 0)
        weather_ack(0xffffffff);
}

void usb_internet_service(void)
{
    bool link;
    int length;
    int budget = SERVICE_MESSAGE_BUDGET;
    long deadline = current_tick + SERVICE_TICK_BUDGET;

#if defined(USB_ENABLE_IPHETH_HOST) && !defined(SIMULATOR)
    usb_iphone_tether_service();
    usb_iphone_preflight_service();
    usb_iphone_network_service();
    update_iphone_stage_diagnostics();
    if (usb_iphone_tether_state() != previous_iphone_state)
    {
        previous_iphone_state = usb_iphone_tether_state();
        post_iphone_tether_status(previous_iphone_state);
    }
    if (usb_iphone_network_connected() != previous_iphone_route)
    {
        previous_iphone_route = usb_iphone_network_connected();
        post_iphone_route_status(previous_iphone_route);
    }
#endif
#if defined(USB_ENABLE_ETHERNET) && !defined(SIMULATOR)
    framebuffer_worker_init();
    if (ethernet_companion_mode() &&
        usb_ethernet_link_active())
    {
        while (budget > 0 && TIME_BEFORE(current_tick, deadline) &&
               (length = usb_ethernet_udp_receive(USB_INTERNET_WEATHER_PORT,
                                                   service_message,
                                                   sizeof(service_message))) > 0)
        {
            budget--;
            if (length >= 5 && !memcmp(service_message, "RPI1", 4))
            {
                companion_seen = current_tick;
                /* Echo the presence probe so companions can distinguish an
                 * enumerated USB interface from a working RockPod link. */
                if (service_message[4] == 0)
                    usb_ethernet_udp_send(USB_INTERNET_WEATHER_PORT,
                                          service_message, 5);
            }
            handle_weather_message(service_message, length);
        }
        while (budget > 0 && TIME_BEFORE(current_tick, deadline) &&
               (length = usb_ethernet_udp_receive(USB_INTERNET_SYNC_PORT,
                                                   service_message,
                                                   sizeof(service_message))) > 0)
        {
            budget--;
            if (length >= 5 && !memcmp(service_message, SYNC_MAGIC, 4))
                companion_seen = current_tick;
            handle_sync_message(service_message, length);
        }
    }
    else
    {
        companion_seen = 0;
        framebuffer_active = false;
    }
#endif
    link = usb_internet_connected();

    if (!link)
    {
        if (previous_link)
        {
            weather_abort();
            sync_abort();
        }
        previous_link = false;
        next_weather_request = 0;
        return;
    }
    if (!previous_link || next_weather_request == 0 ||
        !TIME_BEFORE(current_tick, next_weather_request))
    {
        static const unsigned char request[] = {
            'R', 'P', 'I', '1', USB_INTERNET_WEATHER_REQUEST
        };
        if (usb_internet_send(USB_INTERNET_WEATHER_PORT, request,
                              sizeof(request)) >= 0)
            next_weather_request = current_tick + WEATHER_REQUEST_TICKS;
    }
    previous_link = true;
    while (budget > 0 && TIME_BEFORE(current_tick, deadline) &&
           (length = usb_internet_receive(USB_INTERNET_WEATHER_PORT,
                                           service_message,
                                           sizeof(service_message))) > 0)
    {
        budget--;
        handle_weather_message(service_message, length);
    }
}

unsigned long usb_internet_weather_generation(void)
{
    return weather_generation;
}
