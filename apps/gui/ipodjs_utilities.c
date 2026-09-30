/***************************************************************************
 * Native, fixed-memory Clock utilities for the iPodJS interface.
 ***************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "action.h"
#include "audio.h"
#include "button.h"
#include "crc32.h"
#include "dir.h"
#include "file.h"
#include "kernel.h"
#include "lcd.h"
#include "notification.h"
#include "notification_manager.h"
#include "powermgmt.h"
#include "rbpaths.h"
#include "root_menu.h"
#include "screen_access.h"
#include "settings.h"
#include "string-extra.h"
#include "system.h"
#include "timefuncs.h"
#include "ipodjs_retailos.h"
#include "ipodjs_ui.h"
#include "ipodjs_utilities.h"
#ifdef HAVE_RTC_ALARM
#include "rtc.h"
#endif

#define IPODJS_UTILITIES_MAGIC 0x49505531u
#define IPODJS_UTILITIES_VERSION 1u
#define IPODJS_UTILITIES_DIR ROCKBOX_DIR "/ipodjs"
#define IPODJS_UTILITIES_FILE IPODJS_UTILITIES_DIR "/utilities.v1.dat"
#define IPODJS_UTILITIES_NEW IPODJS_UTILITIES_DIR "/utilities.v1.new"
#define IPODJS_WORLD_MAX 8
#define IPODJS_STOPWATCH_LAPS 20
#define IPODJS_ALARMS_MAX 8
#define IPODJS_TIMER_DEFAULT (5 * 60)
#define IPODJS_TIMER_MAX (24 * 60 * 60 - 1)
#define IPODJS_TIMER_NOTIFICATION_ID 0x49505400u
#define IPODJS_UTILITY_ROW_H 27

struct ipodjs_world_city
{
    const char *name;
    int offset_minutes;
};

/* Fixed offsets are deliberate.  Rockbox has no timezone/DST database on
 * these targets, so the UI states that DST is not inferred. */
static const struct ipodjs_world_city ipodjs_world_catalog[] =
{
    { "Honolulu", -600 },
    { "Anchorage", -540 },
    { "Los Angeles", -480 },
    { "Denver", -420 },
    { "Chicago", -360 },
    { "New York", -300 },
    { "Halifax", -240 },
    { "St. John's", -210 },
    { "UTC", 0 },
    { "London", 0 },
    { "Paris", 60 },
    { "Cairo", 120 },
    { "Moscow", 180 },
    { "Dubai", 240 },
    { "Delhi", 330 },
    { "Bangkok", 420 },
    { "Beijing", 480 },
    { "Tokyo", 540 },
    { "Adelaide", 570 },
    { "Sydney", 600 },
    { "Auckland", 720 },
};

struct ipodjs_alarm_record
{
    uint16_t minute_of_day;
    uint8_t enabled;
    uint8_t reserved;
    int32_t last_fired_day;
};

struct ipodjs_utilities_disk
{
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t checksum;
    int16_t home_utc_offset;
    uint8_t city_count;
    uint8_t cities[IPODJS_WORLD_MAX];
    uint8_t stopwatch_running;
    uint8_t stopwatch_lap_count;
    uint16_t stopwatch_reserved;
    uint32_t stopwatch_elapsed_ticks;
    uint32_t stopwatch_laps[IPODJS_STOPWATCH_LAPS];
    int32_t stopwatch_saved_epoch;
    uint32_t timer_duration;
    uint32_t timer_remaining;
    int32_t timer_deadline;
    int32_t timer_saved_epoch;
    uint32_t timer_notification_id;
    uint8_t timer_running;
    uint8_t timer_due;
    uint8_t timer_missed;
    uint8_t alarm_count;
    struct ipodjs_alarm_record alarms[IPODJS_ALARMS_MAX];
};

struct ipodjs_utilities_runtime
{
    struct ipodjs_utilities_disk disk;
    long stopwatch_start_tick;
    long timer_start_tick;
    bool initialized;
    bool stopwatch_interrupted;
    bool timer_interrupted;
    bool service_active;
};

static struct ipodjs_utilities_runtime ipodjs_utilities;

enum ipodjs_utility_asset_mode
{
    IPODJS_UTILITY_ASSETS_NONE = 0,
    IPODJS_UTILITY_ASSETS_CLOCK,
    IPODJS_UTILITY_ASSETS_STOPWATCH,
};

struct ipodjs_clock_assets
{
    struct ipodjs_retailos_image graybar;
    struct ipodjs_retailos_frame_pack map;
    struct ipodjs_retailos_image small;
    struct ipodjs_retailos_image small_night;
    struct ipodjs_retailos_image large;
    struct ipodjs_retailos_image center_cap;
    struct ipodjs_retailos_image shadow;
    struct ipodjs_retailos_frame_pack hours;
    struct ipodjs_retailos_frame_pack minutes;
    struct ipodjs_retailos_frame_pack seconds;
    unsigned char graybar_data[IPODJS_RETAILOS_RGA_BYTES(5, 80)];
    unsigned char map_data[172 * 240];
    unsigned char small_data[IPODJS_RETAILOS_RGA_BYTES(73, 73)];
    unsigned char small_night_data[IPODJS_RETAILOS_RGA_BYTES(73, 73)];
    unsigned char large_data[IPODJS_RETAILOS_RGA_BYTES(132, 145)];
    unsigned char center_cap_data[IPODJS_RETAILOS_RGA_BYTES(73, 73)];
    unsigned char shadow_data[IPODJS_RETAILOS_RGA_BYTES(8, 26)];
    unsigned char hour_data[48 * 73 * 30];
    unsigned char minute_data[48 * 73 * 16];
    unsigned char second_data[48 * 73 * 16];
};

struct ipodjs_stopwatch_assets
{
    struct ipodjs_retailos_image play_pause;
    struct ipodjs_retailos_image small_pause;
    struct ipodjs_retailos_image small_play;
    struct ipodjs_retailos_image caps;
    struct ipodjs_retailos_image clock;
    struct ipodjs_retailos_frame_pack minutes;
    struct ipodjs_retailos_frame_pack seconds;
    unsigned char play_pause_data[IPODJS_RETAILOS_RGA_BYTES(104, 76)];
    unsigned char small_pause_data[IPODJS_RETAILOS_RGA_BYTES(23, 17)];
    unsigned char small_play_data[IPODJS_RETAILOS_RGA_BYTES(23, 17)];
    unsigned char caps_data[IPODJS_RETAILOS_RGA_BYTES(123, 123)];
    unsigned char clock_data[IPODJS_RETAILOS_RGA_BYTES(145, 181)];
    unsigned char minute_data[20 * 18 * 30];
    unsigned char second_data[72 * 124 * 30];
};

/* Clock and Stopwatch never own the LCD at the same time.  Their complete
 * official source-frame sets therefore share one fixed 428,820-byte union.
 * Switching modes happens only at a screen-entry/return service point. */
static union
{
    struct ipodjs_clock_assets clock;
    struct ipodjs_stopwatch_assets stopwatch;
} ipodjs_utility_assets;
static enum ipodjs_utility_asset_mode ipodjs_utility_asset_mode;
static bool ipodjs_utility_asset_valid;

typedef char ipodjs_utilities_state_must_fit[
    sizeof(struct ipodjs_utilities_runtime) <= 6144 ? 1 : -1];

static bool ipodjs_utilities_prepare_clock_assets(void)
{
    struct ipodjs_clock_assets *assets = &ipodjs_utility_assets.clock;
    bool valid = true;

    if (ipodjs_utility_asset_valid &&
        ipodjs_utility_asset_mode == IPODJS_UTILITY_ASSETS_CLOCK)
        return true;
    ipodjs_utility_asset_valid = false;
    ipodjs_utility_asset_mode = IPODJS_UTILITY_ASSETS_NONE;

    valid &= ipodjs_retailos_load_named_rga(
        "world-clock-graybar", assets->graybar_data,
        sizeof(assets->graybar_data), 5, 80, &assets->graybar);
    valid &= ipodjs_retailos_load_named_raw(
        "world-clock-map", assets->map_data, sizeof(assets->map_data),
        320, 240, 172, 0x0004, sizeof(assets->map_data),
        0x0dad0456, &assets->map);
    valid &= ipodjs_retailos_load_named_rga(
        "clock-small", assets->small_data, sizeof(assets->small_data),
        73, 73, &assets->small);
    valid &= ipodjs_retailos_load_named_rga(
        "clock-small-night", assets->small_night_data,
        sizeof(assets->small_night_data), 73, 73,
        &assets->small_night);
    valid &= ipodjs_retailos_load_named_rga(
        "clock-large", assets->large_data, sizeof(assets->large_data),
        132, 145, &assets->large);
    valid &= ipodjs_retailos_load_named_rga(
        "clock-center-cap", assets->center_cap_data,
        sizeof(assets->center_cap_data), 73, 73, &assets->center_cap);
    valid &= ipodjs_retailos_load_named_rga(
        "clock-shadow", assets->shadow_data, sizeof(assets->shadow_data),
        8, 26, &assets->shadow);
    valid &= ipodjs_retailos_load_animation_raw(
        IPODJS_RETAILOS_CLOCK_HOURS, assets->hour_data,
        sizeof(assets->hour_data), &assets->hours);
    valid &= ipodjs_retailos_load_animation_raw(
        IPODJS_RETAILOS_CLOCK_MINUTES, assets->minute_data,
        sizeof(assets->minute_data), &assets->minutes);
    valid &= ipodjs_retailos_load_animation_raw(
        IPODJS_RETAILOS_CLOCK_SECONDS, assets->second_data,
        sizeof(assets->second_data), &assets->seconds);

    if (valid)
    {
        ipodjs_utility_asset_mode = IPODJS_UTILITY_ASSETS_CLOCK;
        ipodjs_utility_asset_valid = true;
    }
    return valid;
}

static bool ipodjs_utilities_prepare_stopwatch_assets(void)
{
    struct ipodjs_stopwatch_assets *assets =
        &ipodjs_utility_assets.stopwatch;
    bool valid = true;

    if (ipodjs_utility_asset_valid &&
        ipodjs_utility_asset_mode == IPODJS_UTILITY_ASSETS_STOPWATCH)
        return true;
    ipodjs_utility_asset_valid = false;
    ipodjs_utility_asset_mode = IPODJS_UTILITY_ASSETS_NONE;

    valid &= ipodjs_retailos_load_named_rga(
        "stopwatch-play-pause", assets->play_pause_data,
        sizeof(assets->play_pause_data), 104, 76, &assets->play_pause);
    valid &= ipodjs_retailos_load_named_rga(
        "stopwatch-small-pause", assets->small_pause_data,
        sizeof(assets->small_pause_data), 23, 17, &assets->small_pause);
    valid &= ipodjs_retailos_load_named_rga(
        "stopwatch-small-play", assets->small_play_data,
        sizeof(assets->small_play_data), 23, 17, &assets->small_play);
    valid &= ipodjs_retailos_load_named_rga(
        "stopwatch-caps", assets->caps_data,
        sizeof(assets->caps_data), 123, 123, &assets->caps);
    valid &= ipodjs_retailos_load_named_rga(
        "stopwatch-clock", assets->clock_data,
        sizeof(assets->clock_data), 145, 181, &assets->clock);
    valid &= ipodjs_retailos_load_animation_raw(
        IPODJS_RETAILOS_STOPWATCH_MINUTES, assets->minute_data,
        sizeof(assets->minute_data), &assets->minutes);
    valid &= ipodjs_retailos_load_animation_raw(
        IPODJS_RETAILOS_STOPWATCH_SECONDS, assets->second_data,
        sizeof(assets->second_data), &assets->seconds);

    if (valid)
    {
        ipodjs_utility_asset_mode = IPODJS_UTILITY_ASSETS_STOPWATCH;
        ipodjs_utility_asset_valid = true;
    }
    return valid;
}

bool ipodjs_utilities_prepare_classic_preview(void)
{
    ipodjs_ui_prepare_retailos_status();
    return ipodjs_utilities_prepare_clock_assets();
}

static bool ipodjs_utilities_rtc_now(long *now)
{
#if CONFIG_RTC
    struct tm *clock = get_time();

    if (!clock || !valid_time(clock))
        return false;
    if (now)
    {
        struct tm copy = *clock;

        *now = (long)mktime(&copy);
    }
    return true;
#else
    (void)now;
    return false;
#endif
}

static void ipodjs_utilities_defaults(void)
{
    memset(&ipodjs_utilities, 0, sizeof(ipodjs_utilities));
    ipodjs_utilities.disk.magic = IPODJS_UTILITIES_MAGIC;
    ipodjs_utilities.disk.version = IPODJS_UTILITIES_VERSION;
    ipodjs_utilities.disk.size = sizeof(ipodjs_utilities.disk);
    ipodjs_utilities.disk.timer_duration = IPODJS_TIMER_DEFAULT;
    ipodjs_utilities.disk.timer_remaining = IPODJS_TIMER_DEFAULT;
    ipodjs_utilities.disk.timer_notification_id =
        IPODJS_TIMER_NOTIFICATION_ID;
}

static bool ipodjs_utilities_validate(void)
{
    struct ipodjs_utilities_disk *disk = &ipodjs_utilities.disk;
    int index;

    if (disk->magic != IPODJS_UTILITIES_MAGIC ||
        disk->version != IPODJS_UTILITIES_VERSION ||
        disk->size != sizeof(*disk) ||
        disk->home_utc_offset < -720 || disk->home_utc_offset > 840 ||
        disk->city_count > IPODJS_WORLD_MAX ||
        disk->stopwatch_lap_count > IPODJS_STOPWATCH_LAPS ||
        disk->timer_duration == 0 ||
        disk->timer_duration > IPODJS_TIMER_MAX ||
        disk->timer_remaining > IPODJS_TIMER_MAX ||
        disk->alarm_count > IPODJS_ALARMS_MAX)
        return false;

    for (index = 0; index < disk->city_count; index++)
    {
        if (disk->cities[index] >= ARRAYLEN(ipodjs_world_catalog))
            return false;
    }
    for (index = 0; index < disk->alarm_count; index++)
    {
        if (disk->alarms[index].minute_of_day >= 24 * 60 ||
            disk->alarms[index].enabled > 1)
            return false;
    }
    return true;
}

static uint32_t ipodjs_utilities_checksum(
    const struct ipodjs_utilities_disk *source)
{
    struct ipodjs_utilities_disk copy = *source;

    copy.checksum = 0;
    return crc_32(&copy, sizeof(copy), 0xffffffffu);
}

static uint32_t ipodjs_stopwatch_elapsed(void)
{
    uint32_t elapsed = ipodjs_utilities.disk.stopwatch_elapsed_ticks;

    if (ipodjs_utilities.disk.stopwatch_running)
        elapsed += (uint32_t)(current_tick -
                              ipodjs_utilities.stopwatch_start_tick);
    return elapsed;
}

static uint32_t ipodjs_timer_remaining(void)
{
    struct ipodjs_utilities_disk *disk = &ipodjs_utilities.disk;
    long now;

    if (!disk->timer_running)
        return disk->timer_remaining;
    if (disk->timer_deadline > 0 && ipodjs_utilities_rtc_now(&now))
        return now >= disk->timer_deadline ? 0 :
               (uint32_t)(disk->timer_deadline - now);

    {
        uint32_t elapsed = (uint32_t)(current_tick -
                           ipodjs_utilities.timer_start_tick) / HZ;

        return elapsed >= disk->timer_remaining ? 0 :
               disk->timer_remaining - elapsed;
    }
}

static void ipodjs_utilities_save(void)
{
    struct ipodjs_utilities_disk *disk = &ipodjs_utilities.disk;
    long now = 0;
    int fd;

    if (!ipodjs_utilities.initialized)
        return;

    if (disk->stopwatch_running)
    {
        disk->stopwatch_elapsed_ticks = ipodjs_stopwatch_elapsed();
        ipodjs_utilities.stopwatch_start_tick = current_tick;
    }
    if (disk->timer_running)
    {
        uint32_t remaining = ipodjs_timer_remaining();

        if (disk->timer_deadline <= 0)
        {
            disk->timer_remaining = remaining;
            ipodjs_utilities.timer_start_tick = current_tick;
        }
    }
    if (ipodjs_utilities_rtc_now(&now))
    {
        disk->stopwatch_saved_epoch = now;
        disk->timer_saved_epoch = now;
    }
    else
    {
        disk->stopwatch_saved_epoch = 0;
        disk->timer_saved_epoch = 0;
    }
    disk->magic = IPODJS_UTILITIES_MAGIC;
    disk->version = IPODJS_UTILITIES_VERSION;
    disk->size = sizeof(*disk);
    disk->checksum = ipodjs_utilities_checksum(disk);

    mkdir(IPODJS_UTILITIES_DIR);
    fd = open(IPODJS_UTILITIES_NEW,
              O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    if (write(fd, disk, sizeof(*disk)) != (ssize_t)sizeof(*disk) ||
        fsync(fd) < 0)
    {
        close(fd);
        remove(IPODJS_UTILITIES_NEW);
        return;
    }
    close(fd);
    if (rename(IPODJS_UTILITIES_NEW, IPODJS_UTILITIES_FILE) < 0)
        remove(IPODJS_UTILITIES_NEW);
}

#ifdef HAVE_RTC_ALARM
static void ipodjs_alarms_program_next(void)
{
    struct ipodjs_utilities_disk *disk = &ipodjs_utilities.disk;
    struct tm *now = get_time();
    int now_minute;
    int best = -1;
    int best_delta = 24 * 60 + 1;
    int index;

    if (!now || !valid_time(now))
        return;
    now_minute = now->tm_hour * 60 + now->tm_min;
    for (index = 0; index < disk->alarm_count; index++)
    {
        int delta;

        if (!disk->alarms[index].enabled)
            continue;
        delta = (disk->alarms[index].minute_of_day - now_minute +
                 24 * 60) % (24 * 60);
        if (delta == 0 && now->tm_sec > 0)
            delta = 24 * 60;
        if (delta < best_delta)
        {
            best = index;
            best_delta = delta;
        }
    }
    if (best < 0)
    {
        rtc_enable_alarm(false);
        return;
    }
    rtc_init();
    rtc_set_alarm(disk->alarms[best].minute_of_day / 60,
                  disk->alarms[best].minute_of_day % 60);
    rtc_enable_alarm(true);
}
#endif

static void ipodjs_timer_schedule(void)
{
    struct ipodjs_utilities_disk *disk = &ipodjs_utilities.disk;
    struct notification_request request;

    notification_cancel(NOTIFICATION_SOURCE_SYSTEM,
                        NOTIFICATION_SYSTEM_TIMER_DUE,
                        disk->timer_notification_id);
    if (!disk->timer_running || disk->timer_deadline <= 0)
        return;
    memset(&request, 0, sizeof(request));
    request.source = NOTIFICATION_SOURCE_SYSTEM;
    request.kind = NOTIFICATION_SYSTEM_TIMER_DUE;
    request.stable_id = disk->timer_notification_id;
    strmemccpy(request.title, "Timer", sizeof(request.title));
    strmemccpy(request.body, "Timer finished", sizeof(request.body));
    strmemccpy(request.route, "clock/timer", sizeof(request.route));
    notification_schedule(&request, disk->timer_deadline);
}

static void ipodjs_utilities_load(void)
{
    struct ipodjs_utilities_disk loaded;
    uint32_t checksum;
    long now = 0;
    int fd;

    if (ipodjs_utilities.initialized)
        return;
    ipodjs_utilities_defaults();
    fd = open(IPODJS_UTILITIES_FILE, O_RDONLY);
    if (fd >= 0)
    {
        if (filesize(fd) == (off_t)sizeof(loaded) &&
            read(fd, &loaded, sizeof(loaded)) == (ssize_t)sizeof(loaded))
        {
            checksum = loaded.checksum;
            loaded.checksum = 0;
            if (crc_32(&loaded, sizeof(loaded), 0xffffffffu) == checksum)
            {
                loaded.checksum = checksum;
                ipodjs_utilities.disk = loaded;
                if (!ipodjs_utilities_validate())
                    ipodjs_utilities_defaults();
            }
        }
        close(fd);
    }

    ipodjs_utilities.initialized = true;
    ipodjs_utilities.stopwatch_start_tick = current_tick;
    ipodjs_utilities.timer_start_tick = current_tick;
    if (ipodjs_utilities_rtc_now(&now))
    {
        if (ipodjs_utilities.disk.stopwatch_running)
        {
            long saved = ipodjs_utilities.disk.stopwatch_saved_epoch;

            if (saved > 0 && now >= saved)
                ipodjs_utilities.disk.stopwatch_elapsed_ticks +=
                    (uint32_t)(now - saved) * HZ;
            else
            {
                ipodjs_utilities.disk.stopwatch_running = false;
                ipodjs_utilities.stopwatch_interrupted = true;
            }
        }
        if (ipodjs_utilities.disk.timer_running)
        {
            if (ipodjs_utilities.disk.timer_deadline > 0)
            {
                if (now >= ipodjs_utilities.disk.timer_deadline)
                {
                    ipodjs_utilities.disk.timer_running = false;
                    ipodjs_utilities.disk.timer_remaining = 0;
                    ipodjs_utilities.disk.timer_due = true;
                    ipodjs_utilities.disk.timer_missed = true;
                }
                else
                    ipodjs_utilities.disk.timer_remaining =
                        ipodjs_utilities.disk.timer_deadline - now;
            }
            else
            {
                ipodjs_utilities.disk.timer_running = false;
                ipodjs_utilities.timer_interrupted = true;
            }
        }
    }
    else
    {
        if (ipodjs_utilities.disk.stopwatch_running)
        {
            ipodjs_utilities.disk.stopwatch_running = false;
            ipodjs_utilities.stopwatch_interrupted = true;
        }
        if (ipodjs_utilities.disk.timer_running)
        {
            ipodjs_utilities.disk.timer_running = false;
            ipodjs_utilities.timer_interrupted = true;
        }
    }
    ipodjs_timer_schedule();
#ifdef HAVE_RTC_ALARM
    ipodjs_alarms_program_next();
#endif
}

static void ipodjs_utility_post(unsigned kind, uint32_t stable_id,
                                const char *title, const char *body,
                                const char *route)
{
    struct notification_request request;

    memset(&request, 0, sizeof(request));
    request.source = NOTIFICATION_SOURCE_SYSTEM;
    request.kind = kind;
    request.stable_id = stable_id;
    strmemccpy(request.title, title, sizeof(request.title));
    strmemccpy(request.body, body, sizeof(request.body));
    strmemccpy(request.route, route, sizeof(request.route));
    notification_post(&request);
}

void ipodjs_utilities_service(void)
{
    struct ipodjs_utilities_disk *disk;
    bool save = false;

    if (ipodjs_utilities.service_active)
        return;
    ipodjs_utilities.service_active = true;
    ipodjs_utilities_load();
    disk = &ipodjs_utilities.disk;
    if (disk->timer_running && ipodjs_timer_remaining() == 0)
    {
        disk->timer_running = false;
        disk->timer_remaining = 0;
        disk->timer_due = true;
        disk->timer_missed = false;
        notification_cancel(NOTIFICATION_SOURCE_SYSTEM,
                            NOTIFICATION_SYSTEM_TIMER_DUE,
                            disk->timer_notification_id);
        ipodjs_utility_post(NOTIFICATION_SYSTEM_TIMER_DUE,
                            disk->timer_notification_id,
                            "Timer", "Timer finished", "clock/timer");
        save = true;
    }
#ifdef HAVE_RTC_ALARM
    {
        struct tm *now = get_time();

        if (now && valid_time(now))
        {
            struct tm copy = *now;
            long epoch = (long)mktime(&copy);
            int32_t day = epoch / (24 * 60 * 60);
            int minute = now->tm_hour * 60 + now->tm_min;
            int index;

            for (index = 0; index < disk->alarm_count; index++)
            {
                struct ipodjs_alarm_record *alarm = &disk->alarms[index];

                if (!alarm->enabled || alarm->minute_of_day != minute ||
                    alarm->last_fired_day == day)
                    continue;
                alarm->last_fired_day = day;
                ipodjs_utility_post(NOTIFICATION_SYSTEM_ALARM_DUE,
                    0x49504100u ^ ((uint32_t)index << 16) ^ (uint32_t)day,
                    "Alarm", "Daily alarm", "clock/alarms");
                save = true;
            }
        }
    }
#endif
    if (save)
    {
        ipodjs_utilities_save();
#ifdef HAVE_RTC_ALARM
        ipodjs_alarms_program_next();
#endif
    }
    ipodjs_utilities.service_active = false;
}

static void ipodjs_format_offset(char *buffer, size_t size, int minutes)
{
    char sign = minutes < 0 ? '-' : '+';
    int value = minutes < 0 ? -minutes : minutes;

    snprintf(buffer, size, "UTC%c%02d:%02d", sign,
             value / 60, value % 60);
}

static void ipodjs_format_clock(char *buffer, size_t size,
                                const struct tm *clock)
{
    int hour;

    if (!clock)
    {
        strmemccpy(buffer, "--:--", size);
        return;
    }
    if (global_settings.timeformat == 0)
        snprintf(buffer, size, "%02d:%02d", clock->tm_hour,
                 clock->tm_min);
    else
    {
        hour = clock->tm_hour % 12;
        if (hour == 0)
            hour = 12;
        snprintf(buffer, size, "%d:%02d %s", hour, clock->tm_min,
                 clock->tm_hour >= 12 ? "PM" : "AM");
    }
}

static bool ipodjs_city_time(int catalog_index, struct tm *result)
{
    struct tm *local = get_time();
    struct tm copy;
    time_t target;

    if (!local || !valid_time(local) || !result || catalog_index < 0 ||
        catalog_index >= (int)ARRAYLEN(ipodjs_world_catalog))
        return false;
    copy = *local;
    target = mktime(&copy) -
        ipodjs_utilities.disk.home_utc_offset * 60 +
        ipodjs_world_catalog[catalog_index].offset_minutes * 60;
    return gmtime_r(&target, result) != NULL;
}

static void ipodjs_draw_header(const char *title)
{
    struct screen *display = &screens[SCREEN_MAIN];
    const int battery_x = LCD_WIDTH - 31;
    const int status_x = battery_x - 19;
    const bool preserve = button_hold();
    const int old_mode = lcd_get_drawmode();

    ipodjs_ui_draw_header_background(display, LCD_WIDTH);
    display->setfont(preserve ? ipodjs_ui_font() :
                     ipodjs_ui_retailos_font(false));
    if (!preserve)
        display->set_drawmode(DRMODE_FG);
    display->set_foreground(ipodjs_ui_header_text());
    display->set_background(ipodjs_ui_header_bg());
    ipodjs_ui_puts_fit(display, 6,
                       preserve ? 5 + ipodjs_ui_text_y_offset() : 4,
                       preserve ? LCD_WIDTH - 58 : status_x - 10,
                       title, false);
    if (button_hold())
        ipodjs_ui_draw_hold_indicator(display, status_x, 4);
    else if (audio_status() & AUDIO_STATUS_PLAY)
        ipodjs_ui_draw_playback_indicator(display, status_x, preserve ? 4 : 2);
    ipodjs_ui_draw_header_battery(display, battery_x, preserve ? 5 : 3);
    display->set_drawmode(old_mode);
}

static void ipodjs_draw_base(const char *title)
{
    struct screen *display = &screens[SCREEN_MAIN];

    display->set_viewport(NULL);
    display->set_drawmode(DRMODE_SOLID);
    display->set_background(ipodjs_ui_screen_bg());
    display->clear_display();
    ipodjs_draw_header(title);
    display->setfont(ipodjs_ui_retailos_menu_font());
}

static void ipodjs_draw_row(int x, int y, int width, const char *label,
                            const char *value, bool selected, bool arrow)
{
    struct screen *display = &screens[SCREEN_MAIN];
    unsigned middle = ipodjs_ui_accent();

    if (selected)
    {
        ipodjs_ui_selection_gradient(display, x, y, width,
                                     IPODJS_UTILITY_ROW_H, &middle);
        display->set_foreground(LCD_RGBPACK(255, 255, 255));
        display->set_background(middle);
    }
    else
    {
        display->set_foreground(ipodjs_ui_row_bg());
        display->fillrect(x, y, width, IPODJS_UTILITY_ROW_H);
        display->set_foreground(ipodjs_ui_text());
        display->set_background(ipodjs_ui_row_bg());
    }
    ipodjs_ui_puts_fit(display, x + 7,
                       y + 5 + ipodjs_ui_text_y_offset(),
                       value ? width * 2 / 3 - 10 : width - 22,
                       label, false);
    if (value)
        ipodjs_ui_puts_fit(display, x + width * 2 / 3, y + 5,
                           width / 3 - (arrow ? 18 : 7), value, true);
    if (arrow)
        ipodjs_ui_draw_arrow(display, x + width - 13,
                             y + (IPODJS_UTILITY_ROW_H - 6) / 2,
                             selected ? LCD_RGBPACK(255, 255, 255) :
                                        ipodjs_ui_muted_text());
    display->set_foreground(ipodjs_ui_muted_text());
    display->hline(x, x + width - 1, y + IPODJS_UTILITY_ROW_H - 1);
}

static void ipodjs_present(void)
{
    if (!ipodjs_ui_transition_present(&screens[SCREEN_MAIN]))
        screens[SCREEN_MAIN].update();
}

static bool ipodjs_utility_hold(bool *redraw)
{
    if (!button_hold())
        return false;
    root_menu_ipodjs_handle_lockscreen();
    if (redraw)
        *redraw = true;
    return true;
}

static bool ipodjs_utility_system_event(int action, bool *redraw)
{
    if (!IS_SYSEVENT(action))
        return false;
    ipodjs_ui_handle_system_event(action, redraw);
    return true;
}

static bool ipodjs_utility_wps(int action)
{
    return action == ACTION_TREE_WPS &&
           (audio_status() & AUDIO_STATUS_PLAY);
}

static void ipodjs_draw_clock_quadrant_hand(
    const struct ipodjs_retailos_frame_pack *pack, unsigned int step,
    int x, int y, fb_data color)
{
    unsigned int frame = step % 30;
    unsigned int quarter = (step / 30) & 3;

    ipodjs_retailos_blit_mask_transform(
        &screens[SCREEN_MAIN], pack, frame, x, y, quarter, false, color);
}

static void ipodjs_draw_clock_octant_hand(
    const struct ipodjs_retailos_frame_pack *pack, unsigned int step,
    int x, int y, fb_data color)
{
    unsigned int within_quarter = step % 30;
    unsigned int quarter = (step / 30) & 3;
    bool reflect = within_quarter > 15;
    unsigned int frame = reflect ? 30 - within_quarter : within_quarter;

    ipodjs_retailos_blit_mask_transform(
        &screens[SCREEN_MAIN], pack, frame, x, y, quarter, reflect, color);
}

static void ipodjs_draw_retail_clock_hands(int x, int y,
                                           const struct tm *clock,
                                           bool night)
{
    struct ipodjs_clock_assets *assets = &ipodjs_utility_assets.clock;
    fb_data hand = night ? FB_RGBPACK(246, 246, 246) :
                           FB_RGBPACK(24, 24, 24);
    unsigned int hour_step;
    unsigned int minute_step;
    unsigned int second_step;

    if (!clock)
        return;
    hour_step = ((clock->tm_hour % 12) * 10 + clock->tm_min / 6) % 120;
    minute_step = (clock->tm_min * 2 + clock->tm_sec / 30) % 120;
    second_step = (clock->tm_sec * 2 +
                  ((current_tick % HZ) >= HZ / 2 ? 1 : 0)) % 120;
    ipodjs_draw_clock_quadrant_hand(&assets->hours, hour_step,
                                    x, y, hand);
    ipodjs_draw_clock_octant_hand(&assets->minutes, minute_step,
                                  x, y, hand);
    ipodjs_draw_clock_octant_hand(&assets->seconds, second_step,
                                  x, y, FB_RGBPACK(194, 38, 43));
    ipodjs_retailos_blit(&screens[SCREEN_MAIN], &assets->center_cap, x, y);
}

static bool ipodjs_draw_retail_clock_small(int x, int y,
                                           const struct tm *clock)
{
    struct ipodjs_clock_assets *assets = &ipodjs_utility_assets.clock;
    bool night;

    if (!ipodjs_utility_asset_valid ||
        ipodjs_utility_asset_mode != IPODJS_UTILITY_ASSETS_CLOCK || !clock)
        return false;
    night = clock->tm_hour < 6 || clock->tm_hour >= 18;
    ipodjs_retailos_blit(&screens[SCREEN_MAIN],
                         night ? &assets->small_night : &assets->small,
                         x, y);
    ipodjs_draw_retail_clock_hands(x, y, clock, night);
    return true;
}

bool ipodjs_utilities_draw_classic_preview(int x, int y,
                                           int width, int height)
{
    struct ipodjs_clock_assets *assets = &ipodjs_utility_assets.clock;
    struct tm *clock = get_time();
    int clock_x;
    int clock_y;

    if (!ipodjs_utility_asset_valid ||
        ipodjs_utility_asset_mode != IPODJS_UTILITY_ASSETS_CLOCK ||
        !clock || width < 132 || height < 145)
        return false;
    clock_x = x + (width - 132) / 2;
    clock_y = y + (height - 145) / 2;
    ipodjs_retailos_blit(&screens[SCREEN_MAIN], &assets->large,
                         clock_x, clock_y);
    /* The large face's dial is 132x132; the remaining rows are its
     * reflection. Center the 73x73 hand sprites in that dial, not in the
     * reflection-inclusive canvas or its top half. */
    ipodjs_draw_retail_clock_hands(clock_x + 29, clock_y + 29,
                                   clock, false);
    return true;
}

bool ipodjs_utilities_draw_classic_preview_map(int x, int y,
                                              int width, int height)
{
    struct ipodjs_clock_assets *assets = &ipodjs_utility_assets.clock;

    if (!ipodjs_utility_asset_valid ||
        ipodjs_utility_asset_mode != IPODJS_UTILITY_ASSETS_CLOCK ||
        width <= 0 || height <= 0 || x < 0 || y < 0 ||
        x + width > assets->map.width || y + height > assets->map.height)
        return false;

    /* Apple places the world map at native screen coordinates. A split pane
     * shows only the matching right-hand source crop, using the RetailOS
     * WorldClock_Map_Color value (white). */
    ipodjs_retailos_blit_mask_part(&screens[SCREEN_MAIN], &assets->map, 0,
                                   x, y, x, y, width, height,
                                   FB_RGBPACK(255, 255, 255));
    return true;
}

static int ipodjs_world_catalog_picker(int initial)
{
    int selected = MAX(0, MIN(initial,
                        (int)ARRAYLEN(ipodjs_world_catalog) - 1));
    bool redraw = true;

    button_clear_queue();
    while (true)
    {
        int action;

        if (ipodjs_utility_hold(&redraw))
            continue;
        if (redraw)
        {
            int visible = (LCD_HEIGHT - IPODJS_UI_HEADER_HEIGHT - 25) /
                          IPODJS_UTILITY_ROW_H;
            int top = selected >= visible ? selected - visible + 1 : 0;
            int row;

            ipodjs_draw_base("Choose City");
            for (row = 0; row < visible &&
                 top + row < (int)ARRAYLEN(ipodjs_world_catalog); row++)
            {
                int index = top + row;
                char offset[16];

                ipodjs_format_offset(offset, sizeof(offset),
                                    ipodjs_world_catalog[index].offset_minutes);
                ipodjs_draw_row(0, IPODJS_UI_HEADER_HEIGHT +
                                row * IPODJS_UTILITY_ROW_H,
                                LCD_WIDTH, ipodjs_world_catalog[index].name,
                                offset, index == selected, false);
            }
            screens[SCREEN_MAIN].set_foreground(ipodjs_ui_muted_text());
            ipodjs_ui_puts_fit(&screens[SCREEN_MAIN], 8, LCD_HEIGHT - 19,
                               LCD_WIDTH - 16, "Fixed UTC offset - no DST",
                               true);
            ipodjs_present();
            redraw = false;
        }
        action = get_action(CONTEXT_TREE, HZ / 5);
        if (ipodjs_utility_system_event(action, &redraw))
            continue;
        switch (action)
        {
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                selected = MAX(0, selected - 1);
                redraw = true;
                break;
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                selected = MIN((int)ARRAYLEN(ipodjs_world_catalog) - 1,
                               selected + 1);
                redraw = true;
                break;
            case ACTION_STD_OK:
                return selected;
            case ACTION_STD_MENU:
            case ACTION_STD_CANCEL:
                return -1;
        }
    }
}

static void ipodjs_home_offset_editor(void)
{
    int value = ipodjs_utilities.disk.home_utc_offset;
    bool redraw = true;

    button_clear_queue();
    while (true)
    {
        int action;

        if (ipodjs_utility_hold(&redraw))
            continue;
        if (redraw)
        {
            char offset[20];

            ipodjs_draw_base("Home UTC Offset");
            ipodjs_format_offset(offset, sizeof(offset), value);
            screens[SCREEN_MAIN].set_foreground(ipodjs_ui_text());
            screens[SCREEN_MAIN].set_background(ipodjs_ui_screen_bg());
            ipodjs_ui_puts_fit(&screens[SCREEN_MAIN], 20, 82,
                               LCD_WIDTH - 40, offset, true);
            screens[SCREEN_MAIN].set_foreground(ipodjs_ui_muted_text());
            ipodjs_ui_puts_fit(&screens[SCREEN_MAIN], 20, 119,
                LCD_WIDTH - 40, "Wheel: 15 minutes", true);
            ipodjs_ui_puts_fit(&screens[SCREEN_MAIN], 20, 143,
                LCD_WIDTH - 40, "Select: Save   Menu: Cancel", true);
            ipodjs_present();
            redraw = false;
        }
        action = get_action(CONTEXT_TREE, HZ / 5);
        if (ipodjs_utility_system_event(action, &redraw))
            continue;
        switch (action)
        {
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                value = MAX(-720, value - 15);
                redraw = true;
                break;
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                value = MIN(840, value + 15);
                redraw = true;
                break;
            case ACTION_STD_OK:
                ipodjs_utilities.disk.home_utc_offset = value;
                ipodjs_utilities_save();
                return;
            case ACTION_STD_MENU:
            case ACTION_STD_CANCEL:
                return;
        }
    }
}

static void ipodjs_world_draw(int selected)
{
    struct ipodjs_utilities_disk *disk = &ipodjs_utilities.disk;
    struct screen *display = &screens[SCREEN_MAIN];
    struct tm *local = get_time();
    struct tm selected_clock;
    const struct tm *clock = local;
    const char *clock_label = "Local";
    bool retail = ipodjs_utility_asset_valid &&
                  ipodjs_utility_asset_mode == IPODJS_UTILITY_ASSETS_CLOCK;
    int controls = disk->city_count + 3;
    int visible = (LCD_HEIGHT - IPODJS_UI_HEADER_HEIGHT) /
                  IPODJS_UTILITY_ROW_H;
    int top = selected >= visible ? selected - visible + 1 : 0;
    int list_width = retail ? 184 : LCD_WIDTH;
    int row;

    if (retail)
    {
        display->set_viewport(NULL);
        display->set_drawmode(DRMODE_SOLID);
        display->set_background(ipodjs_ui_screen_bg());
        display->clear_display();
        ipodjs_retailos_blit_mask(
            display, &ipodjs_utility_assets.clock.map, 0, 0, 0,
            FB_RGBPACK(118, 123, 130));
        ipodjs_draw_header("World Clock");
        display->setfont(ipodjs_ui_retailos_menu_font());
    }
    else
        ipodjs_draw_base("World Clock");
    for (row = 0; row < visible && top + row < controls; row++)
    {
        int index = top + row;
        char value[24];
        const char *label;
        bool arrow = true;

        if (index < disk->city_count)
        {
            int city = disk->cities[index];
            struct tm city_clock;

            label = ipodjs_world_catalog[city].name;
            if (ipodjs_city_time(city, &city_clock))
                ipodjs_format_clock(value, sizeof(value), &city_clock);
            else
                strmemccpy(value, "--:--", sizeof(value));
        }
        else if (index == disk->city_count)
        {
            label = "Add City";
            value[0] = '\0';
        }
        else if (index == disk->city_count + 1)
        {
            label = "Time Format";
            strmemccpy(value, global_settings.timeformat ? "12-Hour" :
                                                          "24-Hour",
                       sizeof(value));
            arrow = false;
        }
        else
        {
            label = "Home Offset";
            ipodjs_format_offset(value, sizeof(value),
                                 disk->home_utc_offset);
        }
        ipodjs_draw_row(0, IPODJS_UI_HEADER_HEIGHT +
                        row * IPODJS_UTILITY_ROW_H,
                        list_width, label, value[0] ? value : NULL,
                        index == selected, arrow);
    }
    if (retail)
    {
        char clock_text[24];

        if (selected < disk->city_count &&
            ipodjs_city_time(disk->cities[selected], &selected_clock))
        {
            clock = &selected_clock;
            clock_label = ipodjs_world_catalog[
                disk->cities[selected]].name;
        }
        ipodjs_retailos_blit(display,
                             &ipodjs_utility_assets.clock.graybar,
                             list_width - 2, 80);
        ipodjs_draw_retail_clock_small(220, 53, clock);
        ipodjs_format_clock(clock_text, sizeof(clock_text), clock);
        display->set_foreground(ipodjs_ui_muted_text());
        display->set_background(ipodjs_ui_screen_bg());
        ipodjs_ui_puts_fit(display, 190, 139, 124, clock_label, true);
        display->set_foreground(ipodjs_ui_text());
        ipodjs_ui_puts_fit(display, 190, 159, 124, clock_text, true);
        display->set_foreground(ipodjs_ui_muted_text());
        ipodjs_ui_puts_fit(display, 190, 184, 124,
                           "Fixed UTC offset", true);
    }
    ipodjs_present();
}

static enum ipodjs_utilities_result ipodjs_world_screen(void)
{
    struct ipodjs_utilities_disk *disk = &ipodjs_utilities.disk;
    int selected = 0;
    bool redraw = true;
    long next_refresh = current_tick;

    button_clear_queue();
    while (true)
    {
        int controls = disk->city_count + 3;
        int action;

        if (ipodjs_utility_hold(&redraw))
            continue;
        selected = MAX(0, MIN(selected, controls - 1));
        if (redraw)
        {
            ipodjs_world_draw(selected);
            redraw = false;
        }
        action = get_action(CONTEXT_TREE, HZ / 5);
        if (ipodjs_utility_system_event(action, &redraw))
            continue;
        if (ipodjs_utility_wps(action))
            return IPODJS_UTILITIES_WPS;
        switch (action)
        {
            case ACTION_NONE:
                if (!TIME_BEFORE(current_tick, next_refresh))
                {
                    next_refresh = current_tick + MAX(1, HZ / 2);
                    redraw = true;
                }
                break;
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                selected = MAX(0, selected - 1);
                redraw = true;
                break;
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                selected = MIN(controls - 1, selected + 1);
                redraw = true;
                break;
            case ACTION_STD_OK:
                if (selected <= disk->city_count)
                {
                    int initial = selected < disk->city_count ?
                                  disk->cities[selected] : 8;
                    int city;

                    ipodjs_ui_transition_begin(1);
                    city = ipodjs_world_catalog_picker(initial);
                    ipodjs_ui_transition_begin(-1);
                    if (city >= 0)
                    {
                        if (selected < disk->city_count)
                            disk->cities[selected] = city;
                        else if (disk->city_count < IPODJS_WORLD_MAX)
                            disk->cities[disk->city_count++] = city;
                        ipodjs_utilities_save();
                    }
                }
                else if (selected == disk->city_count + 1)
                {
                    global_settings.timeformat =
                        global_settings.timeformat ? 0 : 1;
                    settings_save();
                }
                else
                {
                    ipodjs_ui_transition_begin(1);
                    ipodjs_home_offset_editor();
                    ipodjs_ui_transition_begin(-1);
                }
                redraw = true;
                break;
            case ACTION_STD_CONTEXT:
                if (selected < disk->city_count)
                {
                    memmove(&disk->cities[selected],
                            &disk->cities[selected + 1],
                            disk->city_count - selected - 1);
                    disk->city_count--;
                    selected = MIN(selected, disk->city_count + 2);
                    ipodjs_utilities_save();
                    redraw = true;
                }
                break;
            case ACTION_STD_MENU:
            case ACTION_STD_CANCEL:
                ipodjs_utilities_save();
                return IPODJS_UTILITIES_BACK;
        }
    }
}

static void ipodjs_format_ticks(char *buffer, size_t size, uint32_t ticks)
{
    uint32_t tenths = ticks / MAX(1, HZ / 10);
    uint32_t seconds = tenths / 10;

    snprintf(buffer, size, "%02lu:%02lu:%02lu.%lu",
             (unsigned long)(seconds / 3600),
             (unsigned long)((seconds / 60) % 60),
             (unsigned long)(seconds % 60),
             (unsigned long)(tenths % 10));
}

static void ipodjs_stopwatch_draw_retail(int lap_scroll,
                                         const char *message)
{
    struct ipodjs_utilities_disk *disk = &ipodjs_utilities.disk;
    struct ipodjs_stopwatch_assets *assets =
        &ipodjs_utility_assets.stopwatch;
    struct screen *display = &screens[SCREEN_MAIN];
    uint32_t elapsed_ticks = ipodjs_stopwatch_elapsed();
    unsigned int second_step =
        (unsigned int)(((uint64_t)elapsed_ticks * 2 / HZ) % 120);
    unsigned int minute_step =
        (unsigned int)(((uint64_t)elapsed_ticks / (HZ * 30)) % 120);
    char elapsed[32];
    int visible = 5;
    int start;
    int row;

    ipodjs_draw_base("Stopwatch");
    ipodjs_retailos_blit(display, &assets->clock, 5, 27);
    ipodjs_draw_clock_quadrant_hand(
        &assets->seconds, second_step, 15, 66,
        FB_RGBPACK(235, 235, 235));
    ipodjs_draw_clock_quadrant_hand(
        &assets->minutes, minute_step, 68, 103,
        FB_RGBPACK(235, 235, 235));
    ipodjs_retailos_blit(display, &assets->caps, 16, 66);

    ipodjs_format_ticks(elapsed, sizeof(elapsed), elapsed_ticks);
    display->set_foreground(ipodjs_ui_text());
    display->set_background(ipodjs_ui_screen_bg());
    ipodjs_ui_puts_fit(display, 156, 39, 158, elapsed, true);
    display->set_foreground(ipodjs_ui_muted_text());
    ipodjs_ui_puts_fit(display, 156, 62, 158,
        disk->stopwatch_running ? "Running" : "Paused", true);

    start = MAX(0, disk->stopwatch_lap_count - visible - lap_scroll);
    for (row = 0; row < visible && start + row <
         disk->stopwatch_lap_count; row++)
    {
        int index = start + row;
        char line[48];
        char value[32];

        ipodjs_format_ticks(value, sizeof(value),
                            disk->stopwatch_laps[index]);
        snprintf(line, sizeof(line), "Lap %d  %s", index + 1, value);
        display->set_foreground(ipodjs_ui_text());
        ipodjs_ui_puts_fit(display, 158, 83 + row * 19, 154,
                           line, false);
    }
    if (disk->stopwatch_lap_count == 0)
    {
        display->set_foreground(ipodjs_ui_muted_text());
        ipodjs_ui_puts_fit(display, 158, 91, 154, "No laps", true);
    }

    ipodjs_retailos_blit(display, &assets->play_pause, 204, 145);
    ipodjs_retailos_blit(
        display, disk->stopwatch_running ? &assets->small_pause :
                                           &assets->small_play,
        245, 174);
    display->set_foreground(message && message[0] ?
        LCD_RGBPACK(194, 38, 43) : ipodjs_ui_muted_text());
    ipodjs_ui_puts_fit(display, 8, LCD_HEIGHT - 19, LCD_WIDTH - 16,
        message && message[0] ? message :
        "Select start/pause - hold Select lap", true);
    ipodjs_present();
}

static void ipodjs_stopwatch_draw(int lap_scroll, const char *message)
{
    struct ipodjs_utilities_disk *disk = &ipodjs_utilities.disk;
    struct screen *display = &screens[SCREEN_MAIN];
    char elapsed[32];
    int visible = 5;
    int start;
    int row;

    if (ipodjs_utility_asset_valid &&
        ipodjs_utility_asset_mode == IPODJS_UTILITY_ASSETS_STOPWATCH)
    {
        ipodjs_stopwatch_draw_retail(lap_scroll, message);
        return;
    }

    ipodjs_draw_base("Stopwatch");
    ipodjs_format_ticks(elapsed, sizeof(elapsed), ipodjs_stopwatch_elapsed());
    display->set_foreground(ipodjs_ui_text());
    display->set_background(ipodjs_ui_screen_bg());
    ipodjs_ui_puts_fit(display, 16, 45, LCD_WIDTH - 32, elapsed, true);
    display->set_foreground(ipodjs_ui_muted_text());
    ipodjs_ui_puts_fit(display, 16, 72, LCD_WIDTH - 32,
        disk->stopwatch_running ? "Running - hold Select for lap" :
                                  "Paused - Select to start", true);

    start = MAX(0, disk->stopwatch_lap_count - visible - lap_scroll);
    for (row = 0; row < visible && start + row <
         disk->stopwatch_lap_count; row++)
    {
        int index = start + row;
        char label[16];
        char value[32];

        snprintf(label, sizeof(label), "Lap %d", index + 1);
        ipodjs_format_ticks(value, sizeof(value),
                            disk->stopwatch_laps[index]);
        ipodjs_draw_row(24, 96 + row * 24, LCD_WIDTH - 48,
                        label, value, false, false);
    }
    if (disk->stopwatch_lap_count == 0)
    {
        display->set_foreground(ipodjs_ui_muted_text());
        ipodjs_ui_puts_fit(display, 20, 132, LCD_WIDTH - 40,
                           "No laps", true);
    }
    if (message && message[0])
    {
        display->set_foreground(LCD_RGBPACK(194, 38, 43));
        ipodjs_ui_puts_fit(display, 12, LCD_HEIGHT - 19,
                           LCD_WIDTH - 24, message, true);
    }
    else
    {
        display->set_foreground(ipodjs_ui_muted_text());
        ipodjs_ui_puts_fit(display, 12, LCD_HEIGHT - 19,
            LCD_WIDTH - 24, "Left resets while paused", true);
    }
    ipodjs_present();
}

static enum ipodjs_utilities_result ipodjs_stopwatch_screen(void)
{
    struct ipodjs_utilities_disk *disk = &ipodjs_utilities.disk;
    int lap_scroll = 0;
    const char *message = ipodjs_utilities.stopwatch_interrupted ?
                          "Interrupted run stopped" : NULL;
    bool redraw = true;
    long next_refresh = current_tick;

    ipodjs_utilities.stopwatch_interrupted = false;
    button_clear_queue();
    while (true)
    {
        int action;

        if (ipodjs_utility_hold(&redraw))
            continue;
        if (redraw)
        {
            ipodjs_stopwatch_draw(lap_scroll, message);
            redraw = false;
        }
        action = get_action(CONTEXT_TREE,
            disk->stopwatch_running ? MAX(1, HZ / 10) : HZ / 5);
        if (ipodjs_utility_system_event(action, &redraw))
            continue;
        if (ipodjs_utility_wps(action))
            return IPODJS_UTILITIES_WPS;
        switch (action)
        {
            case ACTION_NONE:
                if (disk->stopwatch_running &&
                    !TIME_BEFORE(current_tick, next_refresh))
                {
                    next_refresh = current_tick + MAX(1, HZ / 10);
                    redraw = true;
                }
                break;
            case ACTION_STD_OK:
                if (disk->stopwatch_running)
                {
                    disk->stopwatch_elapsed_ticks =
                        ipodjs_stopwatch_elapsed();
                    disk->stopwatch_running = false;
                }
                else
                {
                    disk->stopwatch_running = true;
                    ipodjs_utilities.stopwatch_start_tick = current_tick;
                }
                message = NULL;
                ipodjs_utilities_save();
                redraw = true;
                break;
            case ACTION_STD_CONTEXT:
                if (!disk->stopwatch_running)
                {
                    disk->stopwatch_running = true;
                    ipodjs_utilities.stopwatch_start_tick = current_tick;
                }
                if (disk->stopwatch_lap_count >= IPODJS_STOPWATCH_LAPS)
                    message = "Laps full";
                else
                {
                    disk->stopwatch_laps[disk->stopwatch_lap_count++] =
                        ipodjs_stopwatch_elapsed();
                    message = "Lap recorded";
                    ipodjs_utilities_save();
                }
                redraw = true;
                break;
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (disk->stopwatch_lap_count > 5)
                    lap_scroll = MIN(disk->stopwatch_lap_count - 5,
                                     lap_scroll + 1);
                redraw = true;
                break;
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                lap_scroll = MAX(0, lap_scroll - 1);
                redraw = true;
                break;
            case ACTION_STD_CANCEL:
                if (!disk->stopwatch_running)
                {
                    disk->stopwatch_elapsed_ticks = 0;
                    disk->stopwatch_lap_count = 0;
                    lap_scroll = 0;
                    message = "Reset";
                    ipodjs_utilities_save();
                    redraw = true;
                }
                break;
            case ACTION_STD_MENU:
                ipodjs_utilities_save();
                return IPODJS_UTILITIES_BACK;
        }
    }
}

static void ipodjs_format_seconds(char *buffer, size_t size,
                                  uint32_t seconds)
{
    snprintf(buffer, size, "%02lu:%02lu:%02lu",
             (unsigned long)(seconds / 3600),
             (unsigned long)((seconds / 60) % 60),
             (unsigned long)(seconds % 60));
}

static bool ipodjs_timer_editor(void)
{
    uint32_t duration = ipodjs_utilities.disk.timer_duration;
    int values[3] = { duration / 3600,
                      (duration / 60) % 60,
                      duration % 60 };
    int field = 0;
    bool redraw = true;

    button_clear_queue();
    while (true)
    {
        int action;

        if (ipodjs_utility_hold(&redraw))
            continue;
        if (redraw)
        {
            char text[32];
            int x;

            ipodjs_draw_base("Set Timer");
            snprintf(text, sizeof(text), "%02d:%02d:%02d",
                     values[0], values[1], values[2]);
            screens[SCREEN_MAIN].set_foreground(ipodjs_ui_text());
            screens[SCREEN_MAIN].set_background(ipodjs_ui_screen_bg());
            ipodjs_ui_puts_fit(&screens[SCREEN_MAIN], 30, 73,
                               LCD_WIDTH - 60, text, true);
            x = 105 + field * 38;
            screens[SCREEN_MAIN].set_foreground(ipodjs_ui_accent());
            screens[SCREEN_MAIN].fillrect(x, 96, 25, 3);
            screens[SCREEN_MAIN].set_foreground(ipodjs_ui_muted_text());
            ipodjs_ui_puts_fit(&screens[SCREEN_MAIN], 20, 121,
                               LCD_WIDTH - 40, "Hours   Minutes   Seconds",
                               true);
            ipodjs_ui_puts_fit(&screens[SCREEN_MAIN], 20, 153,
                LCD_WIDTH - 40, "Wheel changes - Select advances", true);
            ipodjs_ui_puts_fit(&screens[SCREEN_MAIN], 20, 176,
                LCD_WIDTH - 40, "Menu cancels", true);
            ipodjs_present();
            redraw = false;
        }
        action = get_action(CONTEXT_TREE, HZ / 5);
        if (ipodjs_utility_system_event(action, &redraw))
            continue;
        switch (action)
        {
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                values[field]--;
                if (values[field] < 0)
                    values[field] = field == 0 ? 23 : 59;
                redraw = true;
                break;
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                values[field]++;
                if (values[field] > (field == 0 ? 23 : 59))
                    values[field] = 0;
                redraw = true;
                break;
            case ACTION_STD_OK:
                if (field < 2)
                {
                    field++;
                    redraw = true;
                    break;
                }
                duration = values[0] * 3600 + values[1] * 60 + values[2];
                if (duration == 0)
                {
                    values[2] = 1;
                    redraw = true;
                    break;
                }
                ipodjs_utilities.disk.timer_duration = duration;
                ipodjs_utilities.disk.timer_remaining = duration;
                ipodjs_utilities.disk.timer_due = false;
                ipodjs_utilities.disk.timer_missed = false;
                return true;
            case ACTION_STD_CANCEL:
                if (field > 0)
                {
                    field--;
                    redraw = true;
                }
                else
                    return false;
                break;
            case ACTION_STD_MENU:
                return false;
        }
    }
}

static void ipodjs_timer_start(void)
{
    struct ipodjs_utilities_disk *disk = &ipodjs_utilities.disk;
    long now;

    if (disk->timer_remaining == 0)
        disk->timer_remaining = disk->timer_duration;
    disk->timer_running = true;
    disk->timer_due = false;
    disk->timer_missed = false;
    disk->timer_notification_id++;
    if ((disk->timer_notification_id & 0xffffff00u) !=
        IPODJS_TIMER_NOTIFICATION_ID)
        disk->timer_notification_id = IPODJS_TIMER_NOTIFICATION_ID + 1;
    ipodjs_utilities.timer_start_tick = current_tick;
    if (ipodjs_utilities_rtc_now(&now))
        disk->timer_deadline = now + disk->timer_remaining;
    else
        disk->timer_deadline = 0;
    ipodjs_timer_schedule();
    ipodjs_utilities_save();
}

static void ipodjs_timer_pause(void)
{
    struct ipodjs_utilities_disk *disk = &ipodjs_utilities.disk;

    disk->timer_remaining = ipodjs_timer_remaining();
    disk->timer_running = false;
    disk->timer_deadline = 0;
    notification_cancel(NOTIFICATION_SOURCE_SYSTEM,
                        NOTIFICATION_SYSTEM_TIMER_DUE,
                        disk->timer_notification_id);
    ipodjs_utilities_save();
}

static void ipodjs_timer_draw(void)
{
    struct ipodjs_utilities_disk *disk = &ipodjs_utilities.disk;
    struct screen *display = &screens[SCREEN_MAIN];
    uint32_t remaining = ipodjs_timer_remaining();
    char text[32];
    const char *status;

    ipodjs_draw_base("Timer");
    ipodjs_format_seconds(text, sizeof(text), remaining);
    display->set_foreground(ipodjs_ui_text());
    display->set_background(ipodjs_ui_screen_bg());
    ipodjs_ui_puts_fit(display, 20, 58, LCD_WIDTH - 40, text, true);
    if (disk->timer_due)
        status = disk->timer_missed ? "Missed while powered off" :
                                     "Timer finished";
    else if (disk->timer_running)
        status = "Running - Select pauses";
    else if (remaining != disk->timer_duration)
        status = "Paused - Select resumes";
    else
        status = "Select to set a countdown";
    display->set_foreground(disk->timer_due ?
                            LCD_RGBPACK(194, 38, 43) :
                            ipodjs_ui_muted_text());
    ipodjs_ui_puts_fit(display, 16, 91, LCD_WIDTH - 32, status, true);
    ipodjs_draw_row(32, 126, LCD_WIDTH - 64,
                    disk->timer_running ? "Pause" :
                    disk->timer_due ? "Acknowledge" :
                    remaining == disk->timer_duration ? "Set Timer" :
                                                        "Resume",
                    NULL, true, true);
    ipodjs_draw_row(32, 153, LCD_WIDTH - 64, "Reset / Edit",
                    NULL, false, true);
    display->set_foreground(ipodjs_ui_muted_text());
    ipodjs_ui_puts_fit(display, 12, LCD_HEIGHT - 20, LCD_WIDTH - 24,
        ipodjs_utilities.timer_interrupted ?
        "No valid RTC - interrupted timer paused" :
        "Independent of Sleep Timer", true);
    ipodjs_present();
}

static enum ipodjs_utilities_result ipodjs_timer_screen(bool edit_immediately)
{
    struct ipodjs_utilities_disk *disk = &ipodjs_utilities.disk;
    bool redraw = true;
    long next_refresh = current_tick;

    ipodjs_utilities.timer_interrupted = false;
    if (edit_immediately ||
        (!disk->timer_running && !disk->timer_due &&
         disk->timer_remaining == disk->timer_duration))
    {
        ipodjs_ui_transition_begin(1);
        if (ipodjs_timer_editor())
            ipodjs_timer_start();
        ipodjs_ui_transition_begin(-1);
    }
    button_clear_queue();
    while (true)
    {
        int action;
        uint32_t remaining;

        ipodjs_utilities_service();
        if (ipodjs_utility_hold(&redraw))
            continue;
        if (redraw)
        {
            ipodjs_timer_draw();
            redraw = false;
        }
        remaining = ipodjs_timer_remaining();
        action = get_action(CONTEXT_TREE,
            disk->timer_running && remaining <= 10 ? MAX(1, HZ / 5) :
                                                     HZ / 5);
        if (ipodjs_utility_system_event(action, &redraw))
            continue;
        if (ipodjs_utility_wps(action))
            return IPODJS_UTILITIES_WPS;
        switch (action)
        {
            case ACTION_NONE:
                if (disk->timer_running &&
                    !TIME_BEFORE(current_tick, next_refresh))
                {
                    next_refresh = current_tick +
                        (remaining <= 10 ? MAX(1, HZ / 5) : HZ);
                    redraw = true;
                }
                break;
            case ACTION_STD_OK:
                if (disk->timer_due)
                {
                    disk->timer_due = false;
                    disk->timer_missed = false;
                    disk->timer_remaining = disk->timer_duration;
                    ipodjs_utilities_save();
                }
                else if (disk->timer_running)
                    ipodjs_timer_pause();
                else if (disk->timer_remaining != disk->timer_duration)
                    ipodjs_timer_start();
                else
                {
                    ipodjs_ui_transition_begin(1);
                    if (ipodjs_timer_editor())
                        ipodjs_timer_start();
                    ipodjs_ui_transition_begin(-1);
                }
                redraw = true;
                break;
            case ACTION_STD_CONTEXT:
                if (disk->timer_running)
                    ipodjs_timer_pause();
                disk->timer_running = false;
                disk->timer_due = false;
                disk->timer_missed = false;
                disk->timer_remaining = disk->timer_duration;
                disk->timer_deadline = 0;
                notification_cancel(NOTIFICATION_SOURCE_SYSTEM,
                                    NOTIFICATION_SYSTEM_TIMER_DUE,
                                    disk->timer_notification_id);
                ipodjs_ui_transition_begin(1);
                if (ipodjs_timer_editor())
                    ipodjs_timer_start();
                else
                    ipodjs_utilities_save();
                ipodjs_ui_transition_begin(-1);
                redraw = true;
                break;
            case ACTION_STD_MENU:
            case ACTION_STD_CANCEL:
                ipodjs_utilities_save();
                return IPODJS_UTILITIES_BACK;
        }
    }
}

#ifdef HAVE_RTC_ALARM
static bool ipodjs_alarm_editor(int index)
{
    struct ipodjs_utilities_disk *disk = &ipodjs_utilities.disk;
    int hour = index < disk->alarm_count ?
               disk->alarms[index].minute_of_day / 60 : 7;
    int minute = index < disk->alarm_count ?
                 disk->alarms[index].minute_of_day % 60 : 0;
    bool enabled = index < disk->alarm_count ?
                   disk->alarms[index].enabled : true;
    int field = 0;
    bool redraw = true;

    button_clear_queue();
    while (true)
    {
        int action;

        if (ipodjs_utility_hold(&redraw))
            continue;
        if (redraw)
        {
            char text[24];

            ipodjs_draw_base(index < disk->alarm_count ?
                             "Edit Alarm" : "Add Alarm");
            snprintf(text, sizeof(text), "%02d:%02d", hour, minute);
            screens[SCREEN_MAIN].set_foreground(ipodjs_ui_text());
            screens[SCREEN_MAIN].set_background(ipodjs_ui_screen_bg());
            ipodjs_ui_puts_fit(&screens[SCREEN_MAIN], 30, 65,
                               LCD_WIDTH - 60, text, true);
            ipodjs_draw_row(42, 108, LCD_WIDTH - 84, "Enabled",
                            enabled ? "On" : "Off", field == 2, false);
            screens[SCREEN_MAIN].set_foreground(ipodjs_ui_muted_text());
            ipodjs_ui_puts_fit(&screens[SCREEN_MAIN], 16, 151,
                LCD_WIDTH - 32, "Real RTC wake alarm", true);
            ipodjs_ui_puts_fit(&screens[SCREEN_MAIN], 16, 174,
                LCD_WIDTH - 32, "Wheel changes - Select advances", true);
            ipodjs_present();
            redraw = false;
        }
        action = get_action(CONTEXT_TREE, HZ / 5);
        if (ipodjs_utility_system_event(action, &redraw))
            continue;
        switch (action)
        {
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (field == 0)
                    hour = (hour + 23) % 24;
                else if (field == 1)
                    minute = (minute + 59) % 60;
                else
                    enabled = !enabled;
                redraw = true;
                break;
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (field == 0)
                    hour = (hour + 1) % 24;
                else if (field == 1)
                    minute = (minute + 1) % 60;
                else
                    enabled = !enabled;
                redraw = true;
                break;
            case ACTION_STD_OK:
                if (field < 2)
                {
                    field++;
                    redraw = true;
                    break;
                }
                if (index >= disk->alarm_count)
                {
                    if (disk->alarm_count >= IPODJS_ALARMS_MAX)
                        return false;
                    index = disk->alarm_count++;
                    disk->alarms[index].last_fired_day = -1;
                }
                disk->alarms[index].minute_of_day = hour * 60 + minute;
                disk->alarms[index].enabled = enabled;
                ipodjs_alarms_program_next();
                ipodjs_utilities_save();
                return true;
            case ACTION_STD_CANCEL:
                if (field > 0)
                {
                    field--;
                    redraw = true;
                }
                else
                    return false;
                break;
            case ACTION_STD_MENU:
                return false;
        }
    }
}

static void ipodjs_alarms_draw(int selected)
{
    struct ipodjs_utilities_disk *disk = &ipodjs_utilities.disk;
    int count = disk->alarm_count + 1;
    int visible = (LCD_HEIGHT - IPODJS_UI_HEADER_HEIGHT - 23) /
                  IPODJS_UTILITY_ROW_H;
    int top = selected >= visible ? selected - visible + 1 : 0;
    int row;

    ipodjs_draw_base("Alarms");
    for (row = 0; row < visible && top + row < count; row++)
    {
        int index = top + row;
        char label[24];
        const char *value = NULL;

        if (index < disk->alarm_count)
        {
            snprintf(label, sizeof(label), "%02d:%02d",
                     disk->alarms[index].minute_of_day / 60,
                     disk->alarms[index].minute_of_day % 60);
            value = disk->alarms[index].enabled ? "On" : "Off";
        }
        else
            strmemccpy(label, "Add Alarm", sizeof(label));
        ipodjs_draw_row(0, IPODJS_UI_HEADER_HEIGHT +
                        row * IPODJS_UTILITY_ROW_H,
                        LCD_WIDTH, label, value, index == selected, true);
    }
    screens[SCREEN_MAIN].set_foreground(ipodjs_ui_muted_text());
    ipodjs_ui_puts_fit(&screens[SCREEN_MAIN], 10, LCD_HEIGHT - 19,
                       LCD_WIDTH - 20,
                       "Daily - wakes supported hardware", true);
    ipodjs_present();
}

static enum ipodjs_utilities_result ipodjs_alarms_screen(void)
{
    struct ipodjs_utilities_disk *disk = &ipodjs_utilities.disk;
    int selected = 0;
    bool redraw = true;

    button_clear_queue();
    while (true)
    {
        int count = disk->alarm_count + 1;
        int action;

        if (ipodjs_utility_hold(&redraw))
            continue;
        selected = MAX(0, MIN(selected, count - 1));
        if (redraw)
        {
            ipodjs_alarms_draw(selected);
            redraw = false;
        }
        action = get_action(CONTEXT_TREE, HZ / 5);
        if (ipodjs_utility_system_event(action, &redraw))
            continue;
        if (ipodjs_utility_wps(action))
            return IPODJS_UTILITIES_WPS;
        switch (action)
        {
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                selected = MAX(0, selected - 1);
                redraw = true;
                break;
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                selected = MIN(count - 1, selected + 1);
                redraw = true;
                break;
            case ACTION_STD_OK:
                ipodjs_ui_transition_begin(1);
                ipodjs_alarm_editor(selected);
                ipodjs_ui_transition_begin(-1);
                redraw = true;
                break;
            case ACTION_STD_CONTEXT:
                if (selected < disk->alarm_count)
                {
                    memmove(&disk->alarms[selected],
                            &disk->alarms[selected + 1],
                            (disk->alarm_count - selected - 1) *
                            sizeof(disk->alarms[0]));
                    disk->alarm_count--;
                    selected = MIN(selected, disk->alarm_count);
                    ipodjs_alarms_program_next();
                    ipodjs_utilities_save();
                    redraw = true;
                }
                break;
            case ACTION_STD_MENU:
            case ACTION_STD_CANCEL:
                ipodjs_utilities_save();
                return IPODJS_UTILITIES_BACK;
        }
    }
}
#endif

void ipodjs_utilities_timer_summary(char *buffer, size_t size)
{
    struct ipodjs_utilities_disk *disk;

    if (!buffer || size == 0)
        return;
    ipodjs_utilities_load();
    disk = &ipodjs_utilities.disk;
    if (disk->timer_due)
        strmemccpy(buffer, disk->timer_missed ? "Timer Missed" :
                                                "Timer Finished", size);
    else if (disk->timer_running)
    {
        char remaining[20];

        ipodjs_format_seconds(remaining, sizeof(remaining),
                              ipodjs_timer_remaining());
        snprintf(buffer, size, "%s remaining", remaining);
    }
    else
    {
        char duration[20];

        ipodjs_format_seconds(duration, sizeof(duration),
                              disk->timer_duration);
        snprintf(buffer, size, "Timer %s", duration);
    }
}

enum ipodjs_utilities_result ipodjs_utilities_open(
    enum ipodjs_utility utility, bool edit_immediately)
{
    ipodjs_utilities_load();
    ipodjs_ui_prepare_retailos_status();
    switch (utility)
    {
        case IPODJS_UTILITY_WORLD_CLOCK:
            ipodjs_utilities_prepare_clock_assets();
            return ipodjs_world_screen();
        case IPODJS_UTILITY_STOPWATCH:
            ipodjs_utilities_prepare_stopwatch_assets();
            return ipodjs_stopwatch_screen();
        case IPODJS_UTILITY_TIMER:
            return ipodjs_timer_screen(edit_immediately);
#ifdef HAVE_RTC_ALARM
        case IPODJS_UTILITY_ALARMS:
            return ipodjs_alarms_screen();
#endif
    }
    return IPODJS_UTILITIES_BACK;
}
