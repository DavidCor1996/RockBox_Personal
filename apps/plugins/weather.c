/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/

#include "plugin.h"
#include "bmp.h"

#define WEATHER_DIR ROCKBOX_DIR "/rockpod/weather"
#define FORECAST_PATH WEATHER_DIR "/forecast.tsv"
#define BG_DIR WEATHER_DIR "/backgrounds"
#define WEATHER_ICON_DIR WEATHER_DIR "/icons"
#define MAX_DAYS 7
#define MAX_HOURS (16 * 24)
#define FILE_BUF 32768
#define WEATHER_ICON_MAX 64

struct weather_day
{
    char date[11];
    char code[24];
    char text[40];
    char temp_min[8];
    char temp_max[8];
    char precip[8];
    char wind_speed[8];
    char wind_dir[8];
    char sunrise[8];
    char sunset[8];
    char source[20];
};

struct weather_hour
{
    char stamp[17];
    char code[24];
    char text[40];
    char temp[8];
    char precip[8];
    char wind_speed[8];
    char wind_dir[8];
    char is_day[4];
    char source[20];
};

struct weather_state
{
    char location[40];
    char generated[24];
    char units[12];
    char status[28];
    struct weather_day days[MAX_DAYS];
    struct weather_hour hours[MAX_HOURS];
    int day_count;
    int hour_count;
    int selected;
    bool detail;
    bool loaded;
};

static struct weather_state weather;
static char file_buf[FILE_BUF];
static unsigned char *bg_data = NULL;
static size_t bg_data_size;
static struct bitmap bg_bmp;
static bool bg_loaded;
static char bg_loaded_path[MAX_PATH];
static struct bitmap icon_bmp;
static unsigned char icon_data[BM_SCALED_SIZE(WEATHER_ICON_MAX,
                                              WEATHER_ICON_MAX,
                                              FORMAT_NATIVE, 0)];
static char icon_loaded_path[MAX_PATH];
static int icon_loaded_size;
static bool icon_loaded;
#ifdef HAVE_LCD_COLOR
#define WEATHER_BG_BUFFER_BYTES BM_SCALED_SIZE(LCD_WIDTH, LCD_HEIGHT, FORMAT_NATIVE, 0)
static unsigned char bg_fallback[WEATHER_BG_BUFFER_BYTES];
#else
#define WEATHER_BG_BUFFER_BYTES BM_SCALED_SIZE(LCD_WIDTH, LCD_HEIGHT, FORMAT_MONO, 0)
static unsigned char bg_fallback[WEATHER_BG_BUFFER_BYTES];
#endif

static bool weather_valid_time(const struct tm *tm)
{
    return tm && tm->tm_mon >= 0 && tm->tm_mon < 12 &&
           tm->tm_mday >= 1 && tm->tm_mday <= 31 &&
           tm->tm_hour >= 0 && tm->tm_hour < 24 &&
           tm->tm_min >= 0 && tm->tm_min < 60;
}

#ifdef HAVE_LCD_COLOR
#define WEATHER_HEADER_TOP      LCD_RGBPACK(254, 255, 255)
#define WEATHER_HEADER_BOTTOM   LCD_RGBPACK(177, 182, 185)
#define WEATHER_HEADER_BORDER   LCD_RGBPACK(121, 149, 163)
#define WEATHER_CARD            LCD_RGBPACK(246, 248, 250)
#define WEATHER_CARD_BORDER     LCD_RGBPACK(174, 181, 188)
#define WEATHER_TEXT            LCD_RGBPACK(20, 22, 24)
#define WEATHER_MUTED           LCD_RGBPACK(92, 98, 106)
#define WEATHER_BLUE_TOP        LCD_RGBPACK(60, 184, 255)
#define WEATHER_BLUE_BOTTOM     LCD_RGBPACK(52, 122, 181)
#else
#define WEATHER_HEADER_TOP      LCD_WHITE
#define WEATHER_HEADER_BOTTOM   LCD_WHITE
#define WEATHER_HEADER_BORDER   LCD_BLACK
#define WEATHER_CARD            LCD_WHITE
#define WEATHER_CARD_BORDER     LCD_BLACK
#define WEATHER_TEXT            LCD_BLACK
#define WEATHER_MUTED           LCD_BLACK
#define WEATHER_BLUE_TOP        LCD_BLACK
#define WEATHER_BLUE_BOTTOM     LCD_BLACK
#endif

static void fill_vgradient(int x, int y, int w, int h, unsigned top,
                           unsigned bottom)
{
#ifdef HAVE_LCD_COLOR
    int i;
    int r1 = RGB_UNPACK_RED(top);
    int g1 = RGB_UNPACK_GREEN(top);
    int b1 = RGB_UNPACK_BLUE(top);
    int r2 = RGB_UNPACK_RED(bottom);
    int g2 = RGB_UNPACK_GREEN(bottom);
    int b2 = RGB_UNPACK_BLUE(bottom);

    if (h <= 1)
    {
        rb->lcd_set_foreground(top);
        rb->lcd_fillrect(x, y, w, h);
        return;
    }

    for (i = 0; i < h; i++)
    {
        rb->lcd_set_foreground(LCD_RGBPACK(
            r1 + (r2 - r1) * i / (h - 1),
            g1 + (g2 - g1) * i / (h - 1),
            b1 + (b2 - b1) * i / (h - 1)));
        rb->lcd_hline(x, x + w - 1, y + i);
    }
#else
    (void)bottom;
    rb->lcd_set_foreground(top);
    rb->lcd_fillrect(x, y, w, h);
#endif
}

static char *next_field(char **cursor)
{
    char *start = *cursor;
    char *tab;

    if (!start)
        return "";

    tab = rb->strchr(start, '\t');
    if (tab)
    {
        *tab = '\0';
        *cursor = tab + 1;
    }
    else
        *cursor = NULL;

    return start;
}

static void copy_field(char *dst, size_t dst_size, char **cursor)
{
    rb->strlcpy(dst, next_field(cursor), dst_size);
}

static void trim_line(char *line)
{
    size_t len = rb->strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        line[--len] = '\0';
}

static void weather_status(void)
{
    struct tm *tm = rb->get_time();

    rb->strcpy(weather.status, "Synced forecast");
    if (!weather.generated[0] || !weather_valid_time(tm))
        return;

    if (weather.day_count > 0 && rb->strcmp(weather.days[weather.day_count - 1].date, "0000-00-00") > 0)
    {
        int today = (tm->tm_year + 1900) * 10000 + (tm->tm_mon + 1) * 100 + tm->tm_mday;
        int last = rb->atoi(weather.days[weather.day_count - 1].date) * 10000 +
                   rb->atoi(weather.days[weather.day_count - 1].date + 5) * 100 +
                   rb->atoi(weather.days[weather.day_count - 1].date + 8);
        if (last < today)
        {
            rb->strcpy(weather.status, "Expired forecast");
            return;
        }
    }

    if (weather.day_count > 0)
        rb->strcpy(weather.status, "Updated by RockPod");
}

static int weather_stamp_key(const char *stamp)
{
    char buf[5];
    int year;
    int month;
    int day;
    int hour;

    if (!stamp || rb->strlen(stamp) < 13)
        return -1;

    rb->memcpy(buf, stamp, 4);
    buf[4] = '\0';
    year = rb->atoi(buf);
    rb->memcpy(buf, stamp + 5, 2);
    buf[2] = '\0';
    month = rb->atoi(buf);
    rb->memcpy(buf, stamp + 8, 2);
    buf[2] = '\0';
    day = rb->atoi(buf);
    rb->memcpy(buf, stamp + 11, 2);
    buf[2] = '\0';
    hour = rb->atoi(buf);

    if (year < 2000 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23)
        return -1;

    return (((year * 100) + month) * 100 + day) * 100 + hour;
}

static int weather_now_key(void)
{
    struct tm *tm = rb->get_time();

    if (!weather_valid_time(tm))
        return -1;

    return ((((tm->tm_year + 1900) * 100 + tm->tm_mon + 1) * 100 +
             tm->tm_mday) * 100 + tm->tm_hour);
}

static int current_hour_index(void)
{
    int now = weather_now_key();
    int best_past = -1;
    int best_future = -1;
    int best_past_index = -1;
    int best_future_index = -1;

    for (int i = 0; i < weather.hour_count; i++)
    {
        int key = weather_stamp_key(weather.hours[i].stamp);
        if (key < 0)
            continue;
        if (now < 0)
            return i;
        if (key <= now && key > best_past)
        {
            best_past = key;
            best_past_index = i;
        }
        else if (key > now && (best_future < 0 || key < best_future))
        {
            best_future = key;
            best_future_index = i;
        }
    }

    return best_past_index >= 0 ? best_past_index : best_future_index;
}

static bool load_forecast(void)
{
    int fd;
    ssize_t got;
    char *line;
    char *save;
    char *cursor;

    rb->memset(&weather, 0, sizeof(weather));
    weather.selected = 0;
    rb->strcpy(weather.units, "metric");

    fd = rb->open(FORECAST_PATH, O_RDONLY);
    if (fd < 0)
        return false;

    got = rb->read(fd, file_buf, sizeof(file_buf) - 1);
    rb->close(fd);
    if (got <= 0)
        return false;
    file_buf[got] = '\0';

    line = rb->strtok_r(file_buf, "\n", &save);
    if (!line)
        return false;
    trim_line(line);
    cursor = line;
    if (rb->strcmp(next_field(&cursor), "rockpod_weather_v1"))
        return false;
    copy_field(weather.location, sizeof(weather.location), &cursor);
    next_field(&cursor);
    next_field(&cursor);
    next_field(&cursor);
    copy_field(weather.generated, sizeof(weather.generated), &cursor);
    next_field(&cursor);
    copy_field(weather.units, sizeof(weather.units), &cursor);

    while ((line = rb->strtok_r(NULL, "\n", &save)))
    {
        char first[24];
        trim_line(line);
        if (!line[0])
            continue;
        cursor = line;
        copy_field(first, sizeof(first), &cursor);
        if (!rb->strcmp(first, "hourly"))
        {
            struct weather_hour *hour;

            if (weather.hour_count >= MAX_HOURS)
                continue;

            hour = &weather.hours[weather.hour_count];
            copy_field(hour->stamp, sizeof(hour->stamp), &cursor);
            copy_field(hour->code, sizeof(hour->code), &cursor);
            copy_field(hour->text, sizeof(hour->text), &cursor);
            copy_field(hour->temp, sizeof(hour->temp), &cursor);
            copy_field(hour->precip, sizeof(hour->precip), &cursor);
            copy_field(hour->wind_speed, sizeof(hour->wind_speed), &cursor);
            copy_field(hour->wind_dir, sizeof(hour->wind_dir), &cursor);
            copy_field(hour->is_day, sizeof(hour->is_day), &cursor);
            copy_field(hour->source, sizeof(hour->source), &cursor);
            weather.hour_count++;
            continue;
        }

        if (weather.day_count >= MAX_DAYS)
            continue;

        struct weather_day *day = &weather.days[weather.day_count];
        rb->strlcpy(day->date, first, sizeof(day->date));
        copy_field(day->code, sizeof(day->code), &cursor);
        copy_field(day->text, sizeof(day->text), &cursor);
        copy_field(day->temp_min, sizeof(day->temp_min), &cursor);
        copy_field(day->temp_max, sizeof(day->temp_max), &cursor);
        copy_field(day->precip, sizeof(day->precip), &cursor);
        copy_field(day->wind_speed, sizeof(day->wind_speed), &cursor);
        copy_field(day->wind_dir, sizeof(day->wind_dir), &cursor);
        copy_field(day->sunrise, sizeof(day->sunrise), &cursor);
        copy_field(day->sunset, sizeof(day->sunset), &cursor);
        copy_field(day->source, sizeof(day->source), &cursor);
        weather.day_count++;
    }

    weather.loaded = weather.day_count > 0;
    weather_status();
    return weather.loaded;
}

static bool code_is_rain(const char *code)
{
    return !rb->strcmp(code, "rain") || !rb->strcmp(code, "drizzle") ||
           !rb->strcmp(code, "thunderstorm") || !rb->strcmp(code, "sleet");
}

static bool code_is_snow(const char *code)
{
    return !rb->strcmp(code, "snow");
}

static bool is_night_now(void)
{
    struct tm *tm = rb->get_time();

    if (!weather_valid_time(tm))
        return false;
    return tm->tm_hour < 6 || tm->tm_hour >= 19;
}

static bool hour_is_night(int hour_index)
{
    if (hour_index >= 0 && hour_index < weather.hour_count &&
        weather.hours[hour_index].is_day[0])
        return rb->atoi(weather.hours[hour_index].is_day) == 0;
    return is_night_now();
}

static const char *weather_icon_name(const char *code, bool night)
{
    if (code && rb->strstr(code, "clear"))
        return night ? "clear_night" : "clear_day";
    if (code && rb->strstr(code, "partly_cloudy"))
        return "partly_cloudy";
    if (code && rb->strstr(code, "cloudy"))
        return "cloudy";
    if (code && rb->strstr(code, "drizzle"))
        return "drizzle";
    if (code && rb->strstr(code, "rain"))
        return "rain";
    if (code && rb->strstr(code, "snow"))
        return "snow";
    if (code && rb->strstr(code, "fog"))
        return "fog";
    if (code && rb->strstr(code, "thunder"))
        return "thunderstorm";
    return "unknown";
}

static const char *background_path(void)
{
    int hour = current_hour_index();
    const char *code = hour >= 0 ? weather.hours[hour].code :
        (weather.day_count > 0 ? weather.days[0].code : "");

    if (hour_is_night(hour))
        return BG_DIR "/night.bmp";
    if (code_is_snow(code))
        return BG_DIR "/snow_day.bmp";
    if (code_is_rain(code))
        return BG_DIR "/rain_day.bmp";
    return BG_DIR "/clear_day.bmp";
}

static struct bitmap *load_weather_icon(const char *code, bool night, int size)
{
    char path[MAX_PATH];
    const char *name;
    int rc;

    size = MAX(16, MIN(size, WEATHER_ICON_MAX));
    name = weather_icon_name(code, night);
    rb->snprintf(path, sizeof(path), WEATHER_ICON_DIR "/%s.64x64x24.bmp",
                 name);

    if (icon_loaded && icon_loaded_size == size &&
        !rb->strcmp(icon_loaded_path, path))
        return &icon_bmp;

    icon_loaded = false;
    icon_loaded_path[0] = '\0';
    if (!rb->file_exists(path))
        return NULL;

    rb->memset(&icon_bmp, 0, sizeof(icon_bmp));
    icon_bmp.width = size;
    icon_bmp.height = size;
    icon_bmp.format = FORMAT_NATIVE;
    icon_bmp.data = icon_data;
    rc = rb->read_bmp_file(path, &icon_bmp, sizeof(icon_data),
                           FORMAT_NATIVE | FORMAT_RESIZE |
                           FORMAT_TRANSPARENT | FORMAT_DITHER, NULL);
    if (rc < 0)
        return NULL;

    rb->strlcpy(icon_loaded_path, path, sizeof(icon_loaded_path));
    icon_loaded_size = size;
    icon_loaded = true;
    return &icon_bmp;
}

static void load_background(void)
{
    const char *path = background_path();
#ifdef HAVE_LCD_COLOR
    const int needed_bytes = BM_SCALED_SIZE(LCD_WIDTH, LCD_HEIGHT, FORMAT_NATIVE, 0);
#else
    const int needed_bytes = BM_SCALED_SIZE(LCD_WIDTH, LCD_HEIGHT, FORMAT_MONO, 0);
#endif
    int decode_buffer_bytes = WEATHER_BG_BUFFER_BYTES;

    if (!bg_data)
        bg_data = (unsigned char *)rb->plugin_get_buffer(&bg_data_size);

    if (bg_loaded && !rb->strcmp(bg_loaded_path, path))
        return;

    if (bg_data && bg_data_size >= needed_bytes)
    {
        decode_buffer_bytes = needed_bytes;
        bg_bmp.data = bg_data;
    }
    else
        bg_bmp.data = (unsigned char *)bg_fallback;

    bg_bmp.width = LCD_WIDTH;
    bg_bmp.height = LCD_HEIGHT;
    bg_bmp.format = FORMAT_NATIVE;
    bg_loaded = rb->read_bmp_file(path, &bg_bmp, decode_buffer_bytes,
                                  FORMAT_NATIVE | FORMAT_RESIZE |
                                  FORMAT_DITHER, NULL) > 0;
    if (bg_loaded)
        rb->strlcpy(bg_loaded_path, path, sizeof(bg_loaded_path));
    else
        bg_loaded_path[0] = '\0';
}

static void puts_fit(int x, int y, int width, const char *text, bool center)
{
    char buf[64];
    int w, h, len;

    if (!text || !text[0])
        return;

    rb->strlcpy(buf, text, sizeof(buf));
    len = rb->strlen(buf);
    rb->lcd_getstringsize(buf, &w, &h);
    while (len > 1 && w > width)
    {
        buf[--len] = '\0';
        rb->lcd_getstringsize(buf, &w, &h);
    }
    if (center && w < width)
        x += (width - w) / 2;
    rb->lcd_putsxy(x, y, buf);
}

static void draw_header(const char *title, const char *right)
{
    fill_vgradient(0, 0, LCD_WIDTH, 20, WEATHER_HEADER_TOP,
                   WEATHER_HEADER_BOTTOM);
    rb->lcd_set_foreground(WEATHER_HEADER_BORDER);
    rb->lcd_hline(0, LCD_WIDTH - 1, 19);
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_foreground(WEATHER_TEXT);
    rb->lcd_set_background(WEATHER_HEADER_BOTTOM);
    puts_fit(6, 4, 170, title ? title : "Weather", false);
    rb->lcd_set_foreground(WEATHER_MUTED);
    puts_fit(LCD_WIDTH - 126, 4, 118, right ? right : "", true);
}

static void draw_panel(int x, int y, int w, int h, bool active)
{
#ifdef HAVE_LCD_COLOR
    if (active)
        fill_vgradient(x, y, w, h, WEATHER_BLUE_TOP, WEATHER_BLUE_BOTTOM);
    else
    {
        rb->lcd_set_foreground(WEATHER_CARD);
        rb->lcd_fillrect(x, y, w, h);
    }
#else
    rb->lcd_set_foreground(active ? LCD_BLACK : LCD_WHITE);
    rb->lcd_fillrect(x, y, w, h);
#endif
    rb->lcd_set_foreground(active ? WEATHER_BLUE_BOTTOM : WEATHER_CARD_BORDER);
    rb->lcd_drawrect(x, y, w, h);
}

static const char *day_label(const char *date)
{
    if (weather.day_count > 0 && !rb->strcmp(date, weather.days[0].date))
        return "Today";
    return date && date[5] ? date + 5 : "--";
}

static bool hour_is_for_day(const struct weather_hour *hour,
                            const struct weather_day *day)
{
    return hour && day && day->date[0] &&
           !rb->strncmp(hour->stamp, day->date, 10);
}

static int selected_day_current_hour(void)
{
    int hour = current_hour_index();

    if (hour >= 0 && hour_is_for_day(&weather.hours[hour],
                                     &weather.days[weather.selected]))
        return hour;

    for (int i = 0; i < weather.hour_count; i++)
    {
        if (hour_is_for_day(&weather.hours[i], &weather.days[weather.selected]))
            return i;
    }

    return -1;
}

static void draw_background(void)
{
    load_background();
    rb->lcd_set_viewport(NULL);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    if (bg_loaded)
        rb->lcd_bitmap((const fb_data *)bg_bmp.data, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    else
    {
#ifdef HAVE_LCD_COLOR
        rb->lcd_set_background(LCD_RGBPACK(28, 36, 48));
#else
        rb->lcd_set_background(LCD_WHITE);
#endif
        rb->lcd_clear_display();
#ifdef HAVE_LCD_COLOR
        fill_vgradient(0, 0, LCD_WIDTH, LCD_HEIGHT,
                       LCD_RGBPACK(102, 151, 198),
                       LCD_RGBPACK(227, 235, 242));
#endif
    }
}

static bool code_matches(const char *code, const char *name)
{
    return code && !rb->strcmp(code, name);
}

static void draw_weather_picture(int cx, int cy, int size, const char *code)
{
    int hour = current_hour_index();
    struct bitmap *icon = load_weather_icon(code, hour_is_night(hour), size);
    int r = size / 2;
    bool rain = code_is_rain(code);
    bool snow = code_is_snow(code);
    bool cloudy = code_matches(code, "cloudy") ||
                  code_matches(code, "overcast") ||
                  code_matches(code, "fog");
    bool night = is_night_now();

#ifdef HAVE_LCD_COLOR
    unsigned sun = LCD_RGBPACK(255, 204, 62);
    unsigned cloud = LCD_RGBPACK(235, 239, 244);
    unsigned cloud_shadow = LCD_RGBPACK(174, 184, 195);
    unsigned blue = LCD_RGBPACK(65, 139, 210);
    unsigned moon = LCD_RGBPACK(237, 240, 210);
#else
    unsigned sun = LCD_BLACK;
    unsigned cloud = LCD_WHITE;
    unsigned cloud_shadow = LCD_BLACK;
    unsigned blue = LCD_BLACK;
    unsigned moon = LCD_WHITE;
#endif

    if (icon)
    {
        rb->lcd_bitmap((const fb_data *)icon->data, cx - icon->width / 2,
                       cy - icon->height / 2, icon->width, icon->height);
        return;
    }

    if (night && !rain && !snow)
    {
        rb->lcd_set_foreground(moon);
        rb->lcd_fillrect(cx - r / 2, cy - r / 2, r, r);
        rb->lcd_set_foreground(WEATHER_CARD);
        rb->lcd_fillrect(cx - r / 5, cy - r / 2, r, r);
    }
    else if (!cloudy && !rain && !snow)
    {
        int i;
        rb->lcd_set_foreground(sun);
        rb->lcd_fillrect(cx - r / 2, cy - r / 2, r, r);
        for (i = 0; i < 8; i++)
        {
            int dx = (i & 1) ? r : r / 2;
            int dy = (i & 2) ? r : r / 2;
            rb->lcd_drawline(cx, cy, cx + ((i & 4) ? -dx : dx),
                             cy + ((i & 2) ? -dy : dy));
        }
    }

    if (cloudy || rain || snow)
    {
        rb->lcd_set_foreground(cloud_shadow);
        rb->lcd_fillrect(cx - r + 4, cy + 3, size - 8, r / 2);
        rb->lcd_set_foreground(cloud);
        rb->lcd_fillrect(cx - r, cy, size, r / 2);
        rb->lcd_fillrect(cx - r / 2, cy - r / 3, r, r / 2);
        rb->lcd_fillrect(cx + r / 6, cy - r / 5, r / 2, r / 2);
        rb->lcd_set_foreground(cloud_shadow);
        rb->lcd_drawrect(cx - r, cy, size, r / 2 + 1);
    }

    if (rain)
    {
        int i;
        rb->lcd_set_foreground(blue);
        for (i = -2; i <= 2; i++)
            rb->lcd_drawline(cx + i * 9, cy + r / 2 + 8,
                             cx + i * 9 - 4, cy + r / 2 + 18);
    }
    else if (snow)
    {
        int i;
        rb->lcd_set_foreground(cloud);
        for (i = -2; i <= 2; i++)
        {
            int x = cx + i * 9;
            int y = cy + r / 2 + 11;
            rb->lcd_hline(x - 3, x + 3, y);
            rb->lcd_vline(x, y - 3, y + 3);
        }
    }
}

static void draw_empty(void)
{
    draw_background();
    draw_header("Weather", "No Data");
    draw_panel(24, 62, LCD_WIDTH - 48, 108, false);
    rb->lcd_setfont(FONT_SYSFIXED);
    draw_weather_picture(LCD_WIDTH / 2, 96, 58, "cloudy");
    rb->lcd_set_foreground(WEATHER_TEXT);
    rb->lcd_set_background(WEATHER_CARD);
    puts_fit(26, 132, LCD_WIDTH - 52, "No Weather", true);
    rb->lcd_set_foreground(WEATHER_MUTED);
    puts_fit(26, 150, LCD_WIDTH - 52, "Sync with RockPod", true);
    rb->lcd_update();
}

static void draw_overview(void)
{
    int i;
    struct weather_day *today = &weather.days[weather.selected];
    int hour_index = selected_day_current_hour();
    struct weather_hour *hour = hour_index >= 0 ? &weather.hours[hour_index] : NULL;
    char hero_temp[32];

    draw_background();
    rb->lcd_setfont(FONT_SYSFIXED);
    draw_header(weather.location[0] ? weather.location : "Weather",
                weather.status);

    draw_panel(10, 28, LCD_WIDTH - 20, 58, false);
    draw_weather_picture(44, 56, 42, hour ? hour->code : today->code);
    if (hour && hour->temp[0])
        rb->snprintf(hero_temp, sizeof(hero_temp), "%s%s Now",
                     hour->temp,
                     !rb->strcmp(weather.units, "imperial") ? "F" : "C");
    else
        rb->snprintf(hero_temp, sizeof(hero_temp), "%s / %s%s",
                     today->temp_max[0] ? today->temp_max : "--",
                     today->temp_min[0] ? today->temp_min : "--",
                     !rb->strcmp(weather.units, "imperial") ? "F" : "C");
    rb->lcd_set_foreground(WEATHER_TEXT);
    rb->lcd_set_background(WEATHER_CARD);
    puts_fit(82, 42, 142,
             hour && hour->text[0] ? hour->text :
             (today->text[0] ? today->text : "Weather"), false);
    rb->lcd_set_foreground(WEATHER_MUTED);
    puts_fit(82, 61, 142, hero_temp, false);

    for (i = 0; i < weather.day_count; i++)
    {
        struct weather_day *day = &weather.days[i];
        int y = 94 + i * 19;
        bool active = i == weather.selected;
        char temps[24];
        char precip[16];

        draw_panel(10, y, LCD_WIDTH - 20, 17, active);
        rb->lcd_set_foreground(active ? LCD_WHITE : WEATHER_TEXT);
        rb->lcd_set_background(active ? WEATHER_BLUE_BOTTOM : WEATHER_CARD);
        rb->snprintf(temps, sizeof(temps), "%s/%s%s", day->temp_max[0] ? day->temp_max : "--",
                     day->temp_min[0] ? day->temp_min : "--",
                     !rb->strcmp(weather.units, "imperial") ? "F" : "C");
        rb->snprintf(precip, sizeof(precip), "%s%%", day->precip[0] ? day->precip : "--");
        puts_fit(18, y + 3, 44, day_label(day->date), false);
        puts_fit(62, y + 3, 112, day->text[0] ? day->text : "Weather", false);
        puts_fit(180, y + 3, 62, temps, true);
        puts_fit(248, y + 3, 46, precip, true);
    }

    rb->lcd_set_foreground(WEATHER_MUTED);
    rb->lcd_set_background(WEATHER_CARD);
    puts_fit(14, LCD_HEIGHT - 18, 120, "Select: Details", false);
    puts_fit(LCD_WIDTH - 112, LCD_HEIGHT - 18, 100, "Menu: Back", true);
    rb->lcd_update();
}

static void draw_detail(void)
{
    struct weather_day *day = &weather.days[weather.selected];
    int current_hour = selected_day_current_hour();
    int shown = 0;
    char line[64];

    draw_background();
    draw_header(weather.location[0] ? weather.location : "Weather",
                day_label(day->date));
    draw_panel(14, 30, LCD_WIDTH - 28, LCD_HEIGHT - 50, false);
    rb->lcd_setfont(FONT_SYSFIXED);
    draw_weather_picture(52, 70, 54,
                         current_hour >= 0 ? weather.hours[current_hour].code :
                         day->code);
    rb->lcd_set_foreground(WEATHER_TEXT);
    rb->lcd_set_background(WEATHER_CARD);
    puts_fit(90, 48, LCD_WIDTH - 112, day_label(day->date), false);
    rb->lcd_set_foreground(WEATHER_MUTED);
    puts_fit(90, 68, LCD_WIDTH - 112,
             current_hour >= 0 && weather.hours[current_hour].text[0] ?
             weather.hours[current_hour].text :
             (day->text[0] ? day->text : "Weather"), false);
    rb->lcd_set_foreground(WEATHER_TEXT);
    if (current_hour >= 0 && weather.hours[current_hour].temp[0])
        rb->snprintf(line, sizeof(line), "Now %s %s",
                     weather.hours[current_hour].temp,
                     !rb->strcmp(weather.units, "imperial") ? "F" : "C");
    else
        rb->snprintf(line, sizeof(line), "High %s  Low %s %s",
                     day->temp_max[0] ? day->temp_max : "--",
                     day->temp_min[0] ? day->temp_min : "--",
                     !rb->strcmp(weather.units, "imperial") ? "F" : "C");
    puts_fit(28, 104, LCD_WIDTH - 56, line, false);
    rb->snprintf(line, sizeof(line), "Precipitation %s%%", day->precip[0] ? day->precip : "--");
    puts_fit(28, 126, LCD_WIDTH - 56, line, false);
    rb->snprintf(line, sizeof(line), "Wind %s %s", day->wind_speed[0] ? day->wind_speed : "--",
                 day->wind_dir[0] ? day->wind_dir : "");
    puts_fit(28, 148, LCD_WIDTH - 56, line, false);
    rb->snprintf(line, sizeof(line), "Sun %s / %s", day->sunrise[0] ? day->sunrise : "--",
                 day->sunset[0] ? day->sunset : "--");
    puts_fit(28, 170, LCD_WIDTH - 56, line, false);
    rb->lcd_set_foreground(WEATHER_MUTED);
    for (int i = 0; i < weather.hour_count && shown < 2; i++)
    {
        struct weather_hour *hour = &weather.hours[i];
        if (!hour_is_for_day(hour, day))
            continue;
        if (current_hour >= 0 && i < current_hour)
            continue;
        rb->snprintf(line, sizeof(line), "%2.2s:00  %s%s  %s",
                     hour->stamp + 11,
                     hour->temp[0] ? hour->temp : "--",
                     !rb->strcmp(weather.units, "imperial") ? "F" : "C",
                     hour->text[0] ? hour->text : "Weather");
        puts_fit(28, 184 + shown * 12, LCD_WIDTH - 56, line, false);
        shown++;
    }
    rb->lcd_set_foreground(WEATHER_MUTED);
    puts_fit(28, 208, LCD_WIDTH - 56, day->source[0] ? day->source : "RockPod", true);
    puts_fit(14, LCD_HEIGHT - 18, 126, "Select: Forecast", false);
    puts_fit(LCD_WIDTH - 112, LCD_HEIGHT - 18, 100, "Menu: Back", true);
    rb->lcd_update();
}

static void draw_weather(void)
{
    if (!weather.loaded)
        draw_empty();
    else if (weather.detail)
        draw_detail();
    else
        draw_overview();
}

enum plugin_status plugin_start(const void *parameter)
{
    int action;
    bool done = false;

    (void)parameter;
    weather.loaded = load_forecast();
    draw_weather();

    while (!done)
    {
        action = rb->get_action(CONTEXT_LIST, TIMEOUT_BLOCK);
        switch (action)
        {
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (weather.loaded && weather.selected > 0)
                {
                    weather.selected--;
                    draw_weather();
                }
                break;
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (weather.loaded && weather.selected + 1 < weather.day_count)
                {
                    weather.selected++;
                    draw_weather();
                }
                break;
            case ACTION_STD_OK:
                if (weather.loaded)
                {
                    weather.detail = !weather.detail;
                    draw_weather();
                }
                break;
            case ACTION_STD_CANCEL:
            case ACTION_STD_MENU:
                if (weather.detail)
                {
                    weather.detail = false;
                    draw_weather();
                }
                else
                    done = true;
                break;
            default:
                if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
                    return PLUGIN_USB_CONNECTED;
                break;
        }
    }

    return PLUGIN_OK;
}
