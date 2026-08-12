/***************************************************************************
 * Fixed-memory local notification history and scheduler for iPodJS.
 ***************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "audio.h"
#include "bmp.h"
#include "dir.h"
#include "file.h"
#include "font.h"
#include "kernel.h"
#include "lcd.h"
#include "misc.h"
#include "notification_manager.h"
#include "pathfuncs.h"
#include "rbpaths.h"
#include "rbunicode.h"
#include "settings.h"
#include "metadata.h"
#include "mv.h"
#include "power.h"
#include "powermgmt.h"
#include "string-extra.h"
#include "system.h"
#include "timefuncs.h"
#include "viewport.h"
#include "usb_internet.h"

#define NOTIFICATION_DIR ROCKBOX_DIR "/notifications"
#define NOTIFICATION_STATE NOTIFICATION_DIR "/state.v1.dat"
#define NOTIFICATION_STATE_NEW NOTIFICATION_DIR "/state.v1.new"
#define NOTIFICATION_MAGIC 0x4e4f5431u
#define NOTIFICATION_VERSION 2
#define NOTIFICATION_SCHEDULE_MAX 8
#define NOTIFICATION_BANNER_QUEUE_MAX 8
#define NOTIFICATION_BANNER_TICKS (5 * HZ)
#define NOTIFICATION_BANNER_HEIGHT 42
#define NOTIFICATION_BANNER_SLIDE_TICKS MAX(1, HZ / 5)
#define NOTIFICATION_IOS5_DIR ROCKBOX_DIR "/ipodjs/notifications"
#define NOTIFICATION_BANNER_ASSET NOTIFICATION_IOS5_DIR \
    "/notification-banner.ios5.320x42x24.bmp"
#define NOTIFICATION_ACHIEVEMENT_ICON NOTIFICATION_IOS5_DIR \
    "/achievement-icon.22x22x24.bmp"
#define NOTIFICATION_SITEKICK_ICON NOTIFICATION_IOS5_DIR \
    "/sitekick-icon.22x22x24.bmp"
#define NOTIFICATION_REGULAR_FONT ROCKBOX_DIR \
    "/ipodjs/12-Adobe-Helvetica.fnt"
#define NOTIFICATION_BOLD_FONT ROCKBOX_DIR \
    "/ipodjs/14-Adobe-Helvetica-Bold.fnt"
#define NOTIFICATION_WEATHER_DIR ROCKBOX_DIR "/rockpod/weather"
#define NOTIFICATION_WEATHER_FILE "forecast.tsv"
#define NOTIFICATION_WEATHER_FIRST_CHECK (15 * HZ)
#define NOTIFICATION_WEATHER_CHECK_PERIOD (30 * 60 * HZ)
#define NOTIFICATION_WEATHER_STALE_SECONDS (36 * 60 * 60)
#define NOTIFICATION_WEATHER_EXPIRED_SECONDS (6 * 24 * 60 * 60)
#define NOTIFICATION_BATTERY_FIRST_CHECK (20 * HZ)
#define NOTIFICATION_BATTERY_CHECK_PERIOD (60 * HZ)
#define NOTIFICATION_STORAGE_FIRST_CHECK (30 * HZ)
#define NOTIFICATION_STORAGE_CHECK_PERIOD (10 * 60 * HZ)
#define NOTIFICATION_STORAGE_LOW_KIB (2u * 1024u * 1024u)
#define NOTIFICATION_STORAGE_CRITICAL_KIB (512u * 1024u)

struct notification_schedule_record
{
    struct notification_request request;
    long deadline;
};

struct notification_disk_header
{
    uint32_t magic;
    uint32_t version;
    uint32_t checksum;
    uint32_t count;
    uint32_t schedule_count;
    unsigned long next_sequence;
};

static struct notification_record notification_history[
    NOTIFICATION_HISTORY_MAX];
static struct notification_schedule_record notification_schedules[
    NOTIFICATION_SCHEDULE_MAX];
static struct notification_record notification_banner_queue[
    NOTIFICATION_BANNER_QUEUE_MAX];
static struct notification_record notification_banner_record;
static int notification_history_count;
static int notification_schedule_count;
static int notification_banner_queue_count;
static unsigned long notification_next_sequence = 1;
static long notification_banner_start;
static long notification_banner_deadline;
static long notification_banner_next_refresh;
static int notification_banner_last_y;
static bool notification_banner_active;
static bool notification_center_active;
static bool notification_initialized;
static bool notification_dirty;
static fb_data notification_banner_pixels[
    LCD_NBELEMS(LCD_WIDTH, NOTIFICATION_BANNER_HEIGHT)] CACHEALIGN_ATTR;
static struct frame_buffer_t notification_banner_framebuffer;
static struct viewport notification_banner_viewport;
static struct bitmap notification_banner_icon;
static unsigned char notification_banner_icon_data[
    BM_SIZE(32, 32, FORMAT_NATIVE, false) + 32 * 32 / 2];
static int notification_regular_font = -2;
static int notification_bold_font = -2;
static int notification_music_status;
static bool notification_music_known;
static char notification_music_path[MAX_PATH];
static uint32_t notification_music_sequence = 1;
static long notification_weather_next_check;
static long notification_weather_mtime;
static int notification_weather_level = -1;
static long notification_battery_next_check;
static int notification_battery_level = -1;
static long notification_storage_next_check;
static int notification_storage_level = -1;

static long notification_now(void);

static void notification_post_simple(unsigned source, unsigned kind,
                                     uint32_t stable_id,
                                     const char *title, const char *body,
                                     const char *route)
{
    struct notification_request request;

    memset(&request, 0, sizeof(request));
    request.source = source;
    request.kind = kind;
    request.stable_id = stable_id;
    strmemccpy(request.title, title, sizeof(request.title));
    strmemccpy(request.body, body, sizeof(request.body));
    if (route)
        strmemccpy(request.route, route, sizeof(request.route));
    notification_post(&request);
}

static uint32_t notification_music_stable_id(unsigned kind)
{
    uint32_t now = (uint32_t)mktime(get_time());

    return 0x4d000000u ^ (kind << 20) ^ now ^
           (uint32_t)current_tick ^ notification_music_sequence++;
}

static void notification_music_service(void)
{
    int status = audio_status();
    bool playing = (status & AUDIO_STATUS_PLAY) != 0;
    bool paused = (status & AUDIO_STATUS_PAUSE) != 0;
    struct mp3entry *id3 = playing ? audio_current_track() : NULL;

    if (!global_settings.notification_music)
    {
        notification_music_known = false;
        notification_music_path[0] = '\0';
        return;
    }

    if (!notification_music_known)
    {
        notification_music_known = true;
        notification_music_status = status;
        if (id3 && id3->path[0])
            strmemccpy(notification_music_path, id3->path,
                       sizeof(notification_music_path));
        return;
    }

    if (id3 && id3->path[0] &&
        strcmp(notification_music_path, id3->path) != 0)
    {
        char body[NOTIFICATION_BODY_SIZE];
        const char *basename = id3->path;
        const char *title;

        path_basename(id3->path, &basename);
        title = id3->title && id3->title[0] ? id3->title : basename;

        if (id3->artist && id3->artist[0] && id3->album && id3->album[0])
            snprintf(body, sizeof(body), "%s - %s", id3->artist,
                     id3->album);
        else if (id3->artist && id3->artist[0])
            strmemccpy(body, id3->artist, sizeof(body));
        else
            strmemccpy(body, "Now playing", sizeof(body));
        strmemccpy(notification_music_path, id3->path,
                   sizeof(notification_music_path));
        notification_post_simple(NOTIFICATION_SOURCE_MUSIC,
            NOTIFICATION_MUSIC_NOW_PLAYING,
            notification_music_stable_id(NOTIFICATION_MUSIC_NOW_PLAYING),
            title, body,
            "now-playing");
    }

    if (playing && paused &&
        !(notification_music_status & AUDIO_STATUS_PAUSE))
        notification_post_simple(NOTIFICATION_SOURCE_MUSIC,
            NOTIFICATION_MUSIC_PAUSED,
            notification_music_stable_id(NOTIFICATION_MUSIC_PAUSED),
            "Playback Paused", "Press Play to resume", "now-playing");
    else if (playing && !paused &&
             (notification_music_status & AUDIO_STATUS_PAUSE))
        notification_post_simple(NOTIFICATION_SOURCE_MUSIC,
            NOTIFICATION_MUSIC_RESUMED,
            notification_music_stable_id(NOTIFICATION_MUSIC_RESUMED),
            "Playback Resumed", "Music is playing", "now-playing");
    else if (!playing &&
             (notification_music_status & AUDIO_STATUS_PLAY))
    {
        struct mp3entry *previous = audio_current_track();
        bool finished = previous && previous->length > 0 &&
            previous->elapsed + 2000 >= previous->length;

        notification_post_simple(NOTIFICATION_SOURCE_MUSIC,
            finished ? NOTIFICATION_MUSIC_QUEUE_FINISHED :
                       NOTIFICATION_MUSIC_STOPPED,
            notification_music_stable_id(finished ?
                NOTIFICATION_MUSIC_QUEUE_FINISHED :
                NOTIFICATION_MUSIC_STOPPED),
            finished ? "Queue Finished" : "Playback Stopped",
            finished ? "There are no more songs in the queue" :
                       "Music playback has ended", "now-playing");
        notification_music_path[0] = '\0';
    }
    notification_music_status = status;
}

static void notification_weather_service(void)
{
    DIR *directory;
    struct dirent *entry;
    long mtime = 0;
    long now;
    int level;

    if (!global_settings.notification_weather)
        return;
    if (notification_weather_next_check == 0)
        notification_weather_next_check = current_tick +
                                          NOTIFICATION_WEATHER_FIRST_CHECK;
    if (TIME_BEFORE(current_tick, notification_weather_next_check))
        return;
    notification_weather_next_check = current_tick +
                                      NOTIFICATION_WEATHER_CHECK_PERIOD;
    now = (long)mktime(get_time());
    if (now <= 0)
        return;

    directory = opendir(NOTIFICATION_WEATHER_DIR);
    if (directory)
    {
        while ((entry = readdir(directory)) != NULL)
        {
            if (!strcmp(entry->d_name, NOTIFICATION_WEATHER_FILE))
            {
                mtime = dir_get_info(directory, entry).mtime;
                break;
            }
        }
        closedir(directory);
    }
    if (mtime <= 0)
        level = 3;
    else if (now - mtime >= NOTIFICATION_WEATHER_EXPIRED_SECONDS)
        level = 2;
    else if (now - mtime >= NOTIFICATION_WEATHER_STALE_SECONDS)
        level = 1;
    else
        level = 0;

    if (level == 0)
    {
        notification_weather_level = 0;
        notification_weather_mtime = mtime;
        return;
    }
    if (level == notification_weather_level &&
        (level == 3 || notification_weather_mtime == mtime))
        return;
    notification_weather_level = level;
    notification_weather_mtime = level == 3 ? 0 : mtime;
    if (level == 3)
        notification_post_simple(NOTIFICATION_SOURCE_WEATHER,
            NOTIFICATION_WEATHER_SYNC_MISSING,
            0x57000000u ^ (uint32_t)(now / (24 * 60 * 60)),
            "Weather Not Synced", "Connect to RockPod to add a forecast",
            "weather");
    else if (level == 2)
        notification_post_simple(NOTIFICATION_SOURCE_WEATHER,
            NOTIFICATION_WEATHER_SYNC_EXPIRED,
            0x57020000u ^ (uint32_t)mtime,
            "Forecast Expired", "Sync with RockPod for accurate weather",
            "weather");
    else
        notification_post_simple(NOTIFICATION_SOURCE_WEATHER,
            NOTIFICATION_WEATHER_SYNC_STALE,
            0x57010000u ^ (uint32_t)mtime,
            "Weather May Be Outdated", "Sync with RockPod to refresh it",
            "weather");
}

static uint32_t notification_daily_stable_id(uint32_t prefix)
{
    long now = notification_now();

    return prefix ^ (uint32_t)(now > 0 ? now / (24 * 60 * 60) : 0);
}

static void notification_battery_service(void)
{
    int level;
    int warning;

    if (!global_settings.notification_battery)
    {
        notification_battery_level = -1;
        return;
    }
    if (notification_battery_next_check == 0)
        notification_battery_next_check = current_tick +
                                           NOTIFICATION_BATTERY_FIRST_CHECK;
    if (TIME_BEFORE(current_tick, notification_battery_next_check))
        return;
    notification_battery_next_check = current_tick +
                                      NOTIFICATION_BATTERY_CHECK_PERIOD;
    level = battery_level();
    if (level < 0 || level > 100)
        return;
    if (charger_inserted() || level > 25)
        warning = 0;
    else if (level <= 10)
        warning = 2;
    else if (level <= 20)
        warning = 1;
    else
        warning = notification_battery_level > 0 ?
                  notification_battery_level : 0;
    if (warning == 0)
    {
        notification_battery_level = 0;
        return;
    }
    if (warning <= notification_battery_level)
        return;
    notification_battery_level = warning;
    if (warning == 2)
        notification_post_simple(NOTIFICATION_SOURCE_BATTERY,
            NOTIFICATION_BATTERY_CRITICAL,
            notification_daily_stable_id(0xba020000u),
            "Critical Battery", "10% remaining - connect to power", NULL);
    else
        notification_post_simple(NOTIFICATION_SOURCE_BATTERY,
            NOTIFICATION_BATTERY_LOW,
            notification_daily_stable_id(0xba010000u),
            "Low Battery", "20% remaining", NULL);
}

static void notification_storage_service(void)
{
    sector_t size = 0;
    sector_t free = 0;
    int warning;
    char body[NOTIFICATION_BODY_SIZE];

    if (!global_settings.notification_storage)
    {
        notification_storage_level = -1;
        return;
    }
    if (notification_storage_next_check == 0)
        notification_storage_next_check = current_tick +
                                           NOTIFICATION_STORAGE_FIRST_CHECK;
    if (TIME_BEFORE(current_tick, notification_storage_next_check))
        return;
    notification_storage_next_check = current_tick +
                                      NOTIFICATION_STORAGE_CHECK_PERIOD;
    volume_size(IF_MV(0,) &size, &free);
    if (size == 0)
        return;
    if (free <= NOTIFICATION_STORAGE_CRITICAL_KIB ||
        free * 100 <= size)
        warning = 2;
    else if (free <= NOTIFICATION_STORAGE_LOW_KIB ||
             free * 100 <= size * 3)
        warning = 1;
    else
        warning = 0;
    if (warning == 0)
    {
        notification_storage_level = 0;
        return;
    }
    if (warning <= notification_storage_level)
        return;
    notification_storage_level = warning;
    snprintf(body, sizeof(body), "%lu MB free - remove unused media",
             (unsigned long)(free / 1024));
    notification_post_simple(NOTIFICATION_SOURCE_STORAGE,
        warning == 2 ? NOTIFICATION_STORAGE_CRITICAL :
                       NOTIFICATION_STORAGE_LOW,
        notification_daily_stable_id(warning == 2 ? 0x5a020000u :
                                                    0x5a010000u),
        warning == 2 ? "Storage Almost Full" : "Storage Running Low",
        body, NULL);
}

static int notification_load_font(const char *path, int fallback)
{
    int loaded = file_exists(path) ? font_load(path) : -1;

    if (loaded >= 0)
    {
        font_lock(loaded, true);
        return loaded;
    }
    return fallback;
}

void notification_manager_prepare_visuals(void)
{
    if (notification_regular_font == -2)
        notification_regular_font = notification_load_font(
            NOTIFICATION_REGULAR_FONT, FONT_UI);
    if (notification_bold_font == -2)
        notification_bold_font = notification_load_font(
            NOTIFICATION_BOLD_FONT, notification_regular_font);
}

int notification_manager_visual_font(bool bold)
{
    notification_manager_prepare_visuals();
    return bold ? notification_bold_font : notification_regular_font;
}

static void *notification_banner_address(int x, int y)
{
    return notification_banner_pixels + y * LCD_WIDTH + x;
}

static fb_data notification_blend_pixel(fb_data from, fb_data to,
                                        unsigned alpha)
{
    unsigned inverse = 255 - alpha;

    return FB_RGBPACK(
        (FB_UNPACK_RED(from) * inverse +
         FB_UNPACK_RED(to) * alpha) / 255,
        (FB_UNPACK_GREEN(from) * inverse +
         FB_UNPACK_GREEN(to) * alpha) / 255,
        (FB_UNPACK_BLUE(from) * inverse +
         FB_UNPACK_BLUE(to) * alpha) / 255);
}

static int notification_round_inset(int row, int height)
{
    int edge = MIN(row, height - row - 1);

    if (edge <= 0)
        return 5;
    if (edge == 1)
        return 2;
    if (edge == 2)
        return 1;
    return 0;
}

static void notification_blend_round_rect(int x, int y, int width,
                                          int height, fb_data color,
                                          unsigned alpha)
{
    int row;

    for (row = 0; row < height; ++row)
    {
        int inset = notification_round_inset(row, height);
        fb_data *pixel = notification_banner_pixels +
                         (y + row) * LCD_WIDTH + x + inset;
        int count = width - 2 * inset;

        while (count-- > 0)
        {
            *pixel = notification_blend_pixel(*pixel, color, alpha);
            pixel++;
        }
    }
}

static int notification_banner_display_y(void)
{
    long elapsed = current_tick - notification_banner_start;
    long remaining = notification_banner_deadline - current_tick;

    if (elapsed < NOTIFICATION_BANNER_SLIDE_TICKS)
        return -NOTIFICATION_BANNER_HEIGHT +
            elapsed * NOTIFICATION_BANNER_HEIGHT /
            NOTIFICATION_BANNER_SLIDE_TICKS;
    if (remaining < NOTIFICATION_BANNER_SLIDE_TICKS)
        return -(NOTIFICATION_BANNER_HEIGHT -
            remaining * NOTIFICATION_BANNER_HEIGHT /
            NOTIFICATION_BANNER_SLIDE_TICKS);
    return 0;
}

static bool notification_overlay_row(int y, int x, int width,
                                     fb_data *output)
{
    int display_y;
    int source_y;

    if (!notification_banner_active || notification_center_active ||
        y < 0 || x < 0 || x + width > LCD_WIDTH)
        return false;
    display_y = notification_banner_display_y();
    source_y = y - display_y;
    if (source_y < 0 || source_y >= NOTIFICATION_BANNER_HEIGHT)
        return false;
    memcpy(output, notification_banner_pixels + source_y * LCD_WIDTH + x,
           width * sizeof(*output));
    return true;
}

static void notification_puts_fit(int x, int y, int width,
                                  const char *text, int font)
{
    static const char ellipsis[] = "...";
    char fitted[NOTIFICATION_BODY_SIZE];
    int chars;
    int ellipsis_width;
    int text_width;
    int bytes;

    if (!text || !text[0] || width <= 0)
        return;
    strmemccpy(fitted, text, sizeof(fitted));
    font_getstringsize(fitted, &text_width, NULL, font);
    if (text_width > width)
    {
        font_getstringsize(ellipsis, &ellipsis_width, NULL, font);
        chars = utf8length((const unsigned char *)fitted);
        do
        {
            bytes = utf8seek((const unsigned char *)fitted, --chars);
            fitted[bytes] = '\0';
            font_getstringsize(fitted, &text_width, NULL, font);
        }
        while (chars > 0 && text_width + ellipsis_width > width);
        strlcat(fitted, ellipsis, sizeof(fitted));
    }
    lcd_setfont(font);
    lcd_putsxy(x, y, fitted);
}

static void notification_render_banner(void)
{
    extern struct frame_buffer_t lcd_framebuffer_default;
    struct frame_buffer_t *screen_buffer = &lcd_framebuffer_default;
    struct viewport *old_viewport;
    const char *icon_path = NULL;
    int bold_font;
    int regular_font;
    int row;

    notification_manager_prepare_visuals();
    bold_font = notification_manager_visual_font(true);
    regular_font = notification_manager_visual_font(false);

    for (row = 0; row < NOTIFICATION_BANNER_HEIGHT; ++row)
        memcpy(notification_banner_pixels + row * LCD_WIDTH,
               FBADDRBUF(screen_buffer, 0, row),
               LCD_WIDTH * sizeof(notification_banner_pixels[0]));

    notification_blend_round_rect(5, 5, LCD_WIDTH - 10, 35,
                                  FB_RGBPACK(0, 0, 0), 62);
    notification_blend_round_rect(5, 3, LCD_WIDTH - 10, 36,
                                  FB_RGBPACK(154, 156, 160), 220);
    notification_blend_round_rect(6, 4, LCD_WIDTH - 12, 34,
                                  FB_RGBPACK(249, 250, 252), 250);

    memset(&notification_banner_viewport, 0,
           sizeof(notification_banner_viewport));
    notification_banner_viewport.width = LCD_WIDTH;
    notification_banner_viewport.height = NOTIFICATION_BANNER_HEIGHT;
    notification_banner_viewport.font = regular_font;
    notification_banner_viewport.drawmode = DRMODE_FG;
    notification_banner_viewport.fg_pattern = LCD_WHITE;
    notification_banner_viewport.bg_pattern = LCD_BLACK;
    notification_banner_framebuffer.data = notification_banner_pixels;
    notification_banner_framebuffer.elems =
        LCD_NBELEMS(LCD_WIDTH, NOTIFICATION_BANNER_HEIGHT);
    notification_banner_framebuffer.stride =
        STRIDE_MAIN(LCD_WIDTH, NOTIFICATION_BANNER_HEIGHT);
    notification_banner_framebuffer.get_address_fn =
        notification_banner_address;
    viewport_set_buffer(&notification_banner_viewport,
                        &notification_banner_framebuffer, SCREEN_MAIN);
    old_viewport = lcd_set_viewport(&notification_banner_viewport);

    if (notification_banner_record.request.source ==
            NOTIFICATION_SOURCE_ACHIEVEMENTS)
        icon_path = NOTIFICATION_ACHIEVEMENT_ICON;
    else if (notification_banner_record.request.source ==
             NOTIFICATION_SOURCE_SITEKICK)
        icon_path = NOTIFICATION_SITEKICK_ICON;
    if (icon_path && file_exists(icon_path))
    {
        memset(&notification_banner_icon, 0,
               sizeof(notification_banner_icon));
        notification_banner_icon.width = 22;
        notification_banner_icon.height = 22;
        notification_banner_icon.format = FORMAT_NATIVE;
        notification_banner_icon.data = notification_banner_icon_data;
        if (read_bmp_file(icon_path, &notification_banner_icon,
                sizeof(notification_banner_icon_data),
                FORMAT_NATIVE | FORMAT_DITHER | FORMAT_TRANSPARENT,
                NULL) >= 0)
            lcd_bmp(&notification_banner_icon, 11,
                    (NOTIFICATION_BANNER_HEIGHT -
                     notification_banner_icon.height) / 2);
    }
    else if (notification_banner_record.request.source ==
             NOTIFICATION_SOURCE_MUSIC)
    {
        lcd_set_foreground(LCD_RGBPACK(42, 43, 46));
        lcd_fillrect(23, 10, 3, 15);
        lcd_hline(16, 25, 10);
        lcd_fillrect(13, 23, 9, 6);
        lcd_fillrect(20, 20, 9, 6);
    }
    else if (notification_banner_record.request.source ==
             NOTIFICATION_SOURCE_LIVETV)
    {
        lcd_set_foreground(LCD_RGBPACK(42, 43, 46));
        lcd_drawrect(12, 11, 20, 15);
        lcd_vline(22, 26, 29);
        lcd_hline(18, 26, 29);
    }
    else if (notification_banner_record.request.source ==
             NOTIFICATION_SOURCE_WEATHER)
    {
        lcd_set_foreground(LCD_RGBPACK(42, 43, 46));
        lcd_fillrect(14, 20, 18, 7);
        lcd_fillrect(18, 15, 10, 10);
        lcd_fillrect(12, 22, 22, 4);
    }
    else if (notification_banner_record.request.source ==
             NOTIFICATION_SOURCE_BATTERY)
    {
        lcd_set_foreground(LCD_RGBPACK(42, 43, 46));
        lcd_drawrect(12, 14, 20, 13);
        lcd_fillrect(32, 18, 2, 5);
        lcd_fillrect(15, 17, 5, 7);
    }
    else if (notification_banner_record.request.source ==
             NOTIFICATION_SOURCE_STORAGE)
    {
        lcd_set_foreground(LCD_RGBPACK(42, 43, 46));
        lcd_hline(13, 32, 14);
        lcd_hline(11, 34, 17);
        lcd_hline(11, 34, 25);
        lcd_vline(11, 17, 25);
        lcd_vline(34, 17, 25);
        lcd_hline(15, 30, 21);
    }
    else
    {
        lcd_set_foreground(LCD_RGBPACK(42, 43, 46));
        lcd_hline(21, 23, 13);
        lcd_hline(19, 25, 14);
        lcd_hline(18, 26, 15);
        lcd_fillrect(17, 17, 11, 7);
        lcd_hline(15, 29, 24);
        lcd_hline(18, 26, 25);
        lcd_hline(20, 24, 27);
    }

    lcd_set_drawmode(DRMODE_FG);
    lcd_set_foreground(LCD_RGBPACK(18, 18, 19));
    notification_puts_fit(41, 4, LCD_WIDTH - 50,
                          notification_banner_record.request.title,
                          bold_font);
    lcd_set_foreground(LCD_RGBPACK(56, 56, 58));
    notification_puts_fit(41, 21, LCD_WIDTH - 50,
                          notification_banner_record.request.body,
                          regular_font);
    lcd_set_viewport(old_viewport);
}

static uint32_t notification_checksum_update(uint32_t hash,
                                             const void *data, size_t size)
{
    const unsigned char *bytes = data;
    size_t index;

    for (index = 0; index < size; ++index)
    {
        hash ^= bytes[index];
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t notification_request_id(
    const struct notification_request *request)
{
    uint32_t hash = 2166136261u;
    const unsigned char *text;

    hash ^= request->source;
    hash *= 16777619u;
    hash ^= request->kind;
    hash *= 16777619u;
    for (text = (const unsigned char *)request->title; *text; ++text)
    {
        hash ^= *text;
        hash *= 16777619u;
    }
    for (text = (const unsigned char *)request->body; *text; ++text)
    {
        hash ^= *text;
        hash *= 16777619u;
    }
    return hash ? hash : 1;
}

static long notification_now(void)
{
#if CONFIG_RTC
    return (long)mktime(get_time());
#else
    return current_tick / HZ;
#endif
}

static bool notification_source_enabled(unsigned source)
{
    if (!global_settings.notifications_enabled)
        return false;
    if (source == NOTIFICATION_SOURCE_ACHIEVEMENTS)
        return global_settings.notification_achievements;
    if (source == NOTIFICATION_SOURCE_SITEKICK)
        return global_settings.notification_sitekick;
    if (source == NOTIFICATION_SOURCE_MUSIC)
        return global_settings.notification_music;
    if (source == NOTIFICATION_SOURCE_LIVETV)
        return global_settings.notification_livetv;
    if (source == NOTIFICATION_SOURCE_WEATHER)
        return global_settings.notification_weather;
    if (source == NOTIFICATION_SOURCE_BATTERY)
        return global_settings.notification_battery;
    if (source == NOTIFICATION_SOURCE_STORAGE)
        return global_settings.notification_storage;
    return source < NOTIFICATION_SOURCE_COUNT;
}

static void notification_copy_request(
    struct notification_request *destination,
    const struct notification_request *source)
{
    memset(destination, 0, sizeof(*destination));
    destination->source = source->source;
    destination->kind = source->kind;
    destination->priority = source->priority;
    destination->stable_id = source->stable_id;
    destination->timestamp = source->timestamp;
    strmemccpy(destination->title, source->title,
               sizeof(destination->title));
    strmemccpy(destination->body, source->body,
               sizeof(destination->body));
    strmemccpy(destination->route, source->route,
               sizeof(destination->route));
}

static void notification_load(void)
{
    struct notification_disk_header header;
    uint32_t checksum;
    int fd = open(NOTIFICATION_STATE, O_RDONLY);

    if (fd < 0)
        return;
    if (read(fd, &header, sizeof(header)) != (ssize_t)sizeof(header))
    {
        close(fd);
        return;
    }
    checksum = header.checksum;
    header.checksum = 0;
    if (header.magic != NOTIFICATION_MAGIC ||
        header.version != NOTIFICATION_VERSION ||
        header.count > NOTIFICATION_HISTORY_MAX ||
        header.schedule_count > NOTIFICATION_SCHEDULE_MAX ||
        read(fd, notification_history,
             header.count * sizeof(notification_history[0])) !=
            (ssize_t)(header.count * sizeof(notification_history[0])) ||
        read(fd, notification_schedules,
             header.schedule_count * sizeof(notification_schedules[0])) !=
            (ssize_t)(header.schedule_count *
                      sizeof(notification_schedules[0])))
    {
        close(fd);
        return;
    }
    close(fd);
    {
        uint32_t computed = notification_checksum_update(
            2166136261u, &header, sizeof(header));
        computed = notification_checksum_update(computed,
            notification_history,
            header.count * sizeof(notification_history[0]));
        computed = notification_checksum_update(computed,
            notification_schedules,
            header.schedule_count * sizeof(notification_schedules[0]));
        if (computed != checksum)
            return;
    }

    notification_history_count = header.count;
    notification_schedule_count = header.schedule_count;
    notification_next_sequence = MAX(1ul, header.next_sequence);
}

void notification_manager_init(void)
{
    if (notification_initialized)
        return;
    notification_initialized = true;
    lcd_set_overlay_row_hook(notification_overlay_row);
    notification_load();
}

void notification_manager_flush(void)
{
    struct notification_disk_header header;
    uint32_t checksum;
    int fd;

    notification_manager_init();
    if (!notification_dirty)
        return;
    memset(&header, 0, sizeof(header));
    header.magic = NOTIFICATION_MAGIC;
    header.version = NOTIFICATION_VERSION;
    header.count = notification_history_count;
    header.schedule_count = notification_schedule_count;
    header.next_sequence = notification_next_sequence;
    checksum = notification_checksum_update(2166136261u, &header,
                                             sizeof(header));
    checksum = notification_checksum_update(checksum, notification_history,
        notification_history_count * sizeof(notification_history[0]));
    checksum = notification_checksum_update(checksum, notification_schedules,
        notification_schedule_count * sizeof(notification_schedules[0]));
    header.checksum = checksum;

    mkdir(NOTIFICATION_DIR);
    fd = open(NOTIFICATION_STATE_NEW, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    if (write(fd, &header, sizeof(header)) != (ssize_t)sizeof(header) ||
        write(fd, notification_history,
              notification_history_count *
                  sizeof(notification_history[0])) !=
            (ssize_t)(notification_history_count *
                      sizeof(notification_history[0])) ||
        write(fd, notification_schedules,
              notification_schedule_count *
                  sizeof(notification_schedules[0])) !=
            (ssize_t)(notification_schedule_count *
                      sizeof(notification_schedules[0])))
    {
        close(fd);
        remove(NOTIFICATION_STATE_NEW);
        return;
    }
    if (fsync(fd) < 0)
    {
        close(fd);
        remove(NOTIFICATION_STATE_NEW);
        return;
    }
    close(fd);
    if (rename(NOTIFICATION_STATE_NEW, NOTIFICATION_STATE) >= 0)
        notification_dirty = false;
}

static void notification_queue_banner(
    const struct notification_record *record)
{
    if (!global_settings.notification_banners ||
        notification_banner_queue_count >= NOTIFICATION_BANNER_QUEUE_MAX)
        return;
    notification_banner_queue[notification_banner_queue_count++] = *record;
}

bool notification_post(const struct notification_request *request)
{
    struct notification_request copy;
    struct notification_record *record;
    int index;

    notification_manager_init();
    if (!request || request->source >= NOTIFICATION_SOURCE_COUNT ||
        !request->title[0] || !notification_source_enabled(request->source))
        return false;
    notification_copy_request(&copy, request);
    if (!copy.timestamp)
        copy.timestamp = notification_now();
    if (!copy.stable_id)
        copy.stable_id = notification_request_id(&copy);

    for (index = 0; index < notification_history_count; ++index)
    {
        record = &notification_history[index];
        if (record->request.source == copy.source &&
            record->request.kind == copy.kind &&
            record->request.stable_id == copy.stable_id)
            return false;
    }

    if (notification_history_count == NOTIFICATION_HISTORY_MAX)
        notification_history_count--;
    memmove(&notification_history[1], &notification_history[0],
            notification_history_count * sizeof(notification_history[0]));
    record = &notification_history[0];
    memset(record, 0, sizeof(*record));
    record->request = copy;
    record->sequence = notification_next_sequence++;
    notification_history_count++;
    notification_queue_banner(record);
    notification_dirty = true;
    notification_manager_flush();
    return true;
}

bool notification_schedule(const struct notification_request *request,
                           long rtc_deadline)
{
    int index;

    notification_manager_init();
    if (!request || rtc_deadline <= 0 ||
        !notification_source_enabled(request->source))
        return false;
    for (index = 0; index < notification_schedule_count; ++index)
    {
        struct notification_request *existing =
            &notification_schedules[index].request;
        if (existing->source == request->source &&
            existing->kind == request->kind &&
            existing->stable_id == request->stable_id)
            break;
    }
    if (index >= NOTIFICATION_SCHEDULE_MAX)
        return false;
    if (index == notification_schedule_count)
        notification_schedule_count++;
    notification_copy_request(&notification_schedules[index].request,
                              request);
    notification_schedules[index].deadline = rtc_deadline;
    notification_dirty = true;
    notification_manager_flush();
    return true;
}

void notification_cancel(unsigned source, unsigned kind, uint32_t stable_id)
{
    int index;

    notification_manager_init();
    for (index = 0; index < notification_schedule_count; ++index)
    {
        struct notification_request *request =
            &notification_schedules[index].request;
        if (request->source == source && request->kind == kind &&
            request->stable_id == stable_id)
        {
            memmove(&notification_schedules[index],
                    &notification_schedules[index + 1],
                    (notification_schedule_count - index - 1) *
                        sizeof(notification_schedules[0]));
            notification_schedule_count--;
            notification_dirty = true;
            notification_manager_flush();
            return;
        }
    }
}

void notification_manager_service(void)
{
    long now;
    int index = 0;

    notification_manager_init();

    /* The USB relay is pumped every call: it is cheap when no packet is
     * queued, and the companion stalls if it is not answered promptly. */
    usb_internet_service();

    /* These four read files or poll hardware.  While a companion is attached
     * this function is reached far more often than it used to be (get_action()
     * no longer blocks indefinitely), and running that I/O at wake rate
     * starves the audio thread.  Hold them near their previous cadence; the
     * banner work below still runs every call so animation stays smooth. */
    static long next_periodic_tick;
    if (!TIME_BEFORE(current_tick, next_periodic_tick))
    {
        next_periodic_tick = current_tick + HZ / 5;
        notification_music_service();
        notification_weather_service();
        notification_battery_service();
        notification_storage_service();
    }
    now = notification_now();
    while (index < notification_schedule_count)
    {
        struct notification_request request;

        if (now <= 0 || notification_schedules[index].deadline > now)
        {
            index++;
            continue;
        }
        request = notification_schedules[index].request;
        memmove(&notification_schedules[index],
                &notification_schedules[index + 1],
                (notification_schedule_count - index - 1) *
                    sizeof(notification_schedules[0]));
        notification_schedule_count--;
        notification_dirty = true;
        notification_post(&request);
    }

    if (notification_banner_active &&
        !TIME_BEFORE(current_tick, notification_banner_deadline))
    {
        notification_banner_active = false;
        lcd_update_rect(0, 0, LCD_WIDTH, NOTIFICATION_BANNER_HEIGHT);
    }
    if (!notification_banner_active && !notification_center_active &&
        notification_banner_queue_count > 0)
    {
        notification_banner_record = notification_banner_queue[0];
        memmove(&notification_banner_queue[0],
                &notification_banner_queue[1],
                (--notification_banner_queue_count) *
                    sizeof(notification_banner_queue[0]));
        notification_banner_start = current_tick;
        notification_banner_deadline = current_tick +
                                       NOTIFICATION_BANNER_TICKS;
        notification_banner_next_refresh = current_tick;
        notification_banner_last_y = -NOTIFICATION_BANNER_HEIGHT;
        notification_render_banner();
        notification_banner_active = true;
        if (global_settings.notification_sound)
            beep_play(1800, 70, 2500);
        lcd_update_rect(0, 0, LCD_WIDTH, NOTIFICATION_BANNER_HEIGHT);
    }
    else if (notification_banner_active)
    {
        int display_y = notification_banner_display_y();

        if (display_y != notification_banner_last_y ||
            !TIME_BEFORE(current_tick, notification_banner_next_refresh))
        {
            notification_banner_last_y = display_y;
            notification_banner_next_refresh = current_tick +
                MAX(1, HZ / 5);
            lcd_update_rect(0, 0, LCD_WIDTH,
                            NOTIFICATION_BANNER_HEIGHT);
        }
    }
    if (notification_dirty)
        notification_manager_flush();
}

int notification_manager_count(void)
{
    notification_manager_init();
    return notification_history_count;
}

int notification_manager_unread_count(void)
{
    int count = 0;
    int index;

    notification_manager_init();
    for (index = 0; index < notification_history_count; ++index)
        if (!notification_history[index].read)
            count++;
    return count;
}

bool notification_manager_get(int index, struct notification_record *record)
{
    notification_manager_init();
    if (!record || index < 0 || index >= notification_history_count)
        return false;
    *record = notification_history[index];
    return true;
}

void notification_manager_mark_read(int index)
{
    notification_manager_init();
    if (index < 0 || index >= notification_history_count ||
        notification_history[index].read)
        return;
    notification_history[index].read = true;
    notification_dirty = true;
    notification_manager_flush();
}

void notification_manager_remove(int index)
{
    notification_manager_init();
    if (index < 0 || index >= notification_history_count)
        return;
    memmove(&notification_history[index], &notification_history[index + 1],
            (notification_history_count - index - 1) *
                sizeof(notification_history[0]));
    notification_history_count--;
    notification_dirty = true;
    notification_manager_flush();
}

void notification_manager_clear(void)
{
    notification_manager_init();
    notification_history_count = 0;
    notification_banner_queue_count = 0;
    notification_banner_active = false;
    notification_dirty = true;
    notification_manager_flush();
}

void notification_manager_test_banner(void)
{
    struct notification_record *record;

    notification_manager_init();
    notification_banner_active = false;
    notification_banner_queue_count = 1;
    record = &notification_banner_queue[0];
    memset(record, 0, sizeof(*record));
    record->request.source = NOTIFICATION_SOURCE_SYSTEM;
    record->request.kind = NOTIFICATION_SYSTEM_TEST;
    record->request.stable_id = (uint32_t)current_tick;
    strmemccpy(record->request.title, "Notification Preview",
               sizeof(record->request.title));
    strmemccpy(record->request.body, "This is how alerts will appear",
               sizeof(record->request.body));
    notification_manager_service();
}

const struct notification_record *notification_manager_banner(void)
{
    notification_manager_service();
    return notification_banner_active ? &notification_banner_record : NULL;
}

long notification_manager_banner_started(void)
{
    return notification_banner_start;
}

void notification_manager_set_center_active(bool active)
{
    notification_center_active = active;
    if (active)
    {
        notification_banner_active = false;
        lcd_update_rect(0, 0, LCD_WIDTH, NOTIFICATION_BANNER_HEIGHT);
    }
}
