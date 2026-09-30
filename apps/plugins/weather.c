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
#include "weather_font.h"

#define WEATHER_DIR ROCKBOX_DIR "/rockpod/weather"
#define FORECAST_PATH WEATHER_DIR "/forecast.tsv"
#define WEATHER_ICON_DIR WEATHER_DIR "/icons"
#define MAX_DAYS 7
#define MAX_HOURS (16 * 24 + 1)
#define FILE_BUF 512

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
    bool has_current;
};

static struct weather_state weather;
static char file_buf[FILE_BUF];
/* Fixed plugin BSS: 93,440 bytes on RGB565; no playback allocation. */
static const char *const icon_names[] = {
    "clear_day", "clear_night", "partly_cloudy", "cloudy", "rain",
    "drizzle", "snow", "fog", "thunderstorm", "unknown"
};
static fb_data large_icons[10][64 * 64];
static fb_data small_icons[10][24 * 24];
static bool icons_ready[10][2];

static bool weather_valid_time(const struct tm *tm)
{
    return tm && tm->tm_mon >= 0 && tm->tm_mon < 12 &&
           tm->tm_mday >= 1 && tm->tm_mday <= 31 &&
           tm->tm_hour >= 0 && tm->tm_hour < 24 &&
           tm->tm_min >= 0 && tm->tm_min < 60;
}

#ifdef HAVE_LCD_COLOR
#define WEATHER_HEADER_TOP      LCD_RGBPACK(73, 119, 172)
#define WEATHER_HEADER_BOTTOM   LCD_RGBPACK(19, 54, 99)
#define WEATHER_HEADER_BORDER   LCD_RGBPACK(106, 161, 211)
#define WEATHER_CARD            LCD_RGBPACK(30, 75, 124)
#define WEATHER_CARD_BORDER     LCD_RGBPACK(74, 118, 164)
#define WEATHER_TEXT            LCD_RGBPACK(255, 255, 255)
#define WEATHER_MUTED           LCD_RGBPACK(184, 213, 241)
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

static void weather_clean_icon_transparency(struct bitmap *bm)
{
#ifdef HAVE_LCD_COLOR
    fb_data *pixels;
    int count;

    if (!bm || !bm->data)
        return;

    pixels = (fb_data *)bm->data;
    count = bm->width * bm->height;
    for (int i = 0; i < count; i++)
    {
        unsigned px = pixels[i];
        int r = RGB_UNPACK_RED(px);
        int g = RGB_UNPACK_GREEN(px);
        int b = RGB_UNPACK_BLUE(px);

        if (r >= 150 && b >= 170 && g + 28 < r && g + 28 < b)
            pixels[i] = TRANSPARENT_COLOR;
    }
#else
    (void)bm;
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

    rb->strcpy(weather.status, weather.has_current ?
               "Live conditions" : "Synced forecast");
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

    if (weather.day_count > 0 && !weather.has_current)
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

    if (weather.has_current && weather.hour_count > 0 &&
        weather_stamp_key(weather.hours[0].stamp) == now)
        return 0;

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
    char *line;
    char *cursor;

    rb->memset(&weather, 0, sizeof(weather));
    weather.selected = 0;
    rb->strcpy(weather.units, "metric");

    fd = rb->open(FORECAST_PATH, O_RDONLY);
    if (fd < 0)
        return false;

    if (rb->read_line(fd, file_buf, sizeof(file_buf)) <= 0)
    {
        rb->close(fd);
        return false;
    }
    line = file_buf;
    trim_line(line);
    cursor = line;
    if (rb->strcmp(next_field(&cursor), "rockpod_weather_v1"))
    {
        rb->close(fd);
        return false;
    }
    copy_field(weather.location, sizeof(weather.location), &cursor);
    next_field(&cursor);
    next_field(&cursor);
    next_field(&cursor);
    copy_field(weather.generated, sizeof(weather.generated), &cursor);
    next_field(&cursor);
    copy_field(weather.units, sizeof(weather.units), &cursor);

    while (rb->read_line(fd, file_buf, sizeof(file_buf)) > 0)
    {
        char first[24];
        trim_line(line);
        if (!line[0])
            continue;
        cursor = line;
        copy_field(first, sizeof(first), &cursor);
        if (!rb->strcmp(first, "current") ||
            !rb->strcmp(first, "hourly"))
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
            if (!rb->strcmp(first, "current"))
                weather.has_current = true;
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

    rb->close(fd);
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
        return night ? "cloudy" : "partly_cloudy";
    if (code && (rb->strstr(code, "cloudy") ||
                 rb->strstr(code, "overcast")))
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

static void prepare_icons(void)
{
    char path[MAX_PATH];
    struct bitmap bm;
    for (int i = 0; i < 10; i++)
    {
        rb->snprintf(path, sizeof(path), WEATHER_ICON_DIR
                     "/%s.64x64x24.bmp", icon_names[i]);
        rb->memset(&bm, 0, sizeof(bm));
        bm.data = (unsigned char *)large_icons[i];
        icons_ready[i][0] = rb->read_bmp_file(path, &bm,
            sizeof(large_icons[i]), FORMAT_NATIVE | FORMAT_TRANSPARENT,
            NULL) > 0 && bm.width == 64 && bm.height == 64;
        if (icons_ready[i][0])
        {
            weather_clean_icon_transparency(&bm);
            for (int y = 0; y < 24; y++)
                for (int x = 0; x < 24; x++)
                    small_icons[i][y * 24 + x] =
                        large_icons[i][(y * 64 / 24) * 64 + x * 64 / 24];
            icons_ready[i][1] = true;
        }
        rb->yield();
    }
}

static void puts_fit(int x, int y, int width, const char *text, bool center)
{
    int w = 0, len = 0;
    if (!text)
        return;
    while (text[len])
    {
        unsigned ch = (unsigned char)text[len];
        if (ch < 32 || ch > 126)
            ch = '?';
        int advance = weather_text_width[ch - 32];
        if (w + advance > width)
            break;
        w += advance;
        len++;
    }
    if (center)
        x += (width - w) / 2;
    rb->lcd_set_drawmode(DRMODE_FG);
    for (int i = 0; i < len; i++)
    {
        unsigned ch = (unsigned char)text[i];
        if (ch < 32 || ch > 126)
            ch = '?';
        int glyph = ch - 32;
        int advance = weather_text_width[glyph];
        rb->lcd_mono_bitmap(weather_text_bits + weather_text_offset[glyph],
                            x, y, advance, 12);
        x += advance;
    }
    rb->lcd_set_drawmode(DRMODE_SOLID);
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
    static const char *const names[] = {"Sun", "Mon", "Tue", "Wed",
                                        "Thu", "Fri", "Sat"};
    static const int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int year = rb->atoi(date), month = rb->atoi(date + 5);
    int day = rb->atoi(date + 8);
    struct tm *now = rb->get_time();
    if (month < 1 || month > 12 || day < 1 || day > 31)
        return "--";
    if (weather_valid_time(now) && year == now->tm_year + 1900 &&
        month == now->tm_mon + 1 && day == now->tm_mday)
        return "Today";
    year -= month < 3;
    return names[(year + year / 4 - year / 100 + year / 400 +
                  offsets[month - 1] + day) % 7];
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
    rb->lcd_set_viewport(NULL);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_backdrop(NULL);
    fill_vgradient(0, 0, LCD_WIDTH, LCD_HEIGHT,
                   WEATHER_BLUE_BOTTOM, WEATHER_HEADER_BOTTOM);
}

static bool code_matches(const char *code, const char *name)
{
    return code && !rb->strcmp(code, name);
}

static void draw_weather_picture(int cx, int cy, int size, const char *code,
                                 bool night)
{
    const char *name = weather_icon_name(code, night);
    for (int i = 0; i < 10; i++)
    {
        int small = size < 40;
        int extent = small ? 24 : 64;
        if (!rb->strcmp(name, icon_names[i]) && icons_ready[i][small])
        {
            rb->lcd_bitmap_transparent(small ? small_icons[i] : large_icons[i],
                cx - extent / 2, cy - extent / 2, extent, extent);
            return;
        }
    }

    /* Missing optional artwork must not become a misleading condition. */
    rb->lcd_set_foreground(WEATHER_MUTED);
    rb->lcd_putsxy(cx - 6, cy - 4, "--");
}

static void draw_empty(void)
{
    draw_background();
    draw_header("Weather", "No Data");
    draw_panel(24, 62, LCD_WIDTH - 48, 108, false);
    rb->lcd_setfont(FONT_SYSFIXED);
    draw_weather_picture(LCD_WIDTH / 2, 96, 58, "cloudy", false);
    rb->lcd_set_foreground(WEATHER_TEXT);
    rb->lcd_set_background(WEATHER_CARD);
    puts_fit(26, 132, LCD_WIDTH - 52, "No Weather", true);
    rb->lcd_set_foreground(WEATHER_MUTED);
    puts_fit(26, 150, LCD_WIDTH - 52, "Sync with RockPod", true);
    rb->lcd_update();
}

/* Embedded Helvetica numerals: no font cache, file I/O or allocation. */
static void draw_temperature(int x, int y, const char *text)
{
    rb->lcd_set_drawmode(DRMODE_FG);
    for (; *text; text++)
    {
        int glyph = *text >= '-' && *text <= '9' ? *text - '-' : 0;
        int width = _sysfont_width[glyph];
        rb->lcd_mono_bitmap(_font_bits + _sysfont_offset[glyph],
                            x, y, width, 35);
        x += width + 1;
    }
    rb->lcd_set_drawmode(DRMODE_SOLID);
}

static void draw_atmosphere(const char *code)
{
    unsigned phase = (unsigned long)*rb->current_tick / MAX(1, HZ / 8);
    bool rain = code_is_rain(code), snow = code_is_snow(code);
    rb->lcd_set_foreground(WEATHER_MUTED);
    if (rain || snow)
    {
        for (unsigned i = 0; i < 9; i++)
        {
            int x = 13 + (i * 23 + phase / 3) % 70;
            int y = 26 + (i * 17 + phase * (rain ? 4 : 1)) % 61;
            if (rain)
                rb->lcd_drawline(x, y, x - 2, y + 4);
            else
                rb->lcd_fillrect(x, y, 2, 2);
        }
    }
    else if (code_matches(code, "clear"))
    {
        for (unsigned i = 0; i < 4; i++)
        {
            int x = 15 + i * 21;
            int y = 30 + (i * 19) % 49;
            rb->lcd_drawpixel(x, y);
            if ((phase / 3 + i) % 4 == 0)
            {
                rb->lcd_hline(x - 2, x + 2, y);
                rb->lcd_vline(x, y - 2, y + 2);
            }
        }
    }
}

static void draw_overview(void)
{
    struct weather_day *day = &weather.days[weather.selected];
    int hi = selected_day_current_hour();
    struct weather_hour *hour = hi >= 0 ? &weather.hours[hi] : NULL;
    char line[64];
    const char *unit = !rb->strcmp(weather.units, "imperial") ? "F" : "C";
    draw_background();
    rb->lcd_set_foreground(WEATHER_TEXT);
    puts_fit(10, 5, LCD_WIDTH - 20, weather.location, true);
    draw_atmosphere(hour ? hour->code : day->code);
    unsigned phase = (unsigned long)*rb->current_tick / MAX(1, HZ / 8) % 64;
    int drift = (phase < 32 ? phase : 64 - phase) / 4 - 4;
    draw_weather_picture(51 + drift, 59, 64,
                         hour ? hour->code : day->code,
                         hour ? hour_is_night(hi) : false);
    rb->lcd_set_foreground(WEATHER_TEXT);
    draw_temperature(100, 25, hour && hour->temp[0] ? hour->temp :
                     (day->temp_max[0] ? day->temp_max : "--"));
    rb->lcd_drawrect(166, 30, 4, 4);
    puts_fit(176, 29, 24, unit, false);
    puts_fit(100, 62, 208, hour ? hour->text : day->text, false);
    rb->snprintf(line, sizeof(line), "H %s  L %s   Rain %s%%",
                 day->temp_max, day->temp_min, day->precip);
    rb->lcd_set_foreground(WEATHER_MUTED);
    puts_fit(100, 78, 212, line, false);
    for (int i = 0; i < weather.day_count; i++)
    {
        int y = 94 + i * 18;
        struct weather_day *d = &weather.days[i];
        if (i == weather.selected)
        {
            rb->lcd_set_foreground(WEATHER_TEXT);
            rb->lcd_fillrect(5, y + 6, 2, 2);
        }
        draw_weather_picture(85, y + 9, 24, d->code, false);
        rb->lcd_set_foreground(WEATHER_TEXT);
        puts_fit(14, y + 2, 48, day_label(d->date), false);
        puts_fit(107, y + 2, 100, d->text, false);
        rb->snprintf(line, sizeof(line), "%s / %s", d->temp_max, d->temp_min);
        puts_fit(213, y + 2, 94, line, true);
    }
    rb->lcd_set_foreground(WEATHER_MUTED);
    puts_fit(10, 223, 300, "Select: Details       Menu: Back", true);
    rb->lcd_update();
}

static void draw_detail(void)
{
    struct weather_day *day = &weather.days[weather.selected];
    int first = selected_day_current_hour();
    char line[64];
    int shown = 0;
    bool imperial = !rb->strcmp(weather.units, "imperial");
    draw_background();
    draw_header(weather.location, day_label(day->date));
    draw_weather_picture(45, 59, 64, day->code, false);
    rb->lcd_set_foreground(WEATHER_TEXT);
    puts_fit(87, 32, 222, day->text, false);
    rb->snprintf(line, sizeof(line), "High %s   Low %s %s",
                 day->temp_max, day->temp_min, imperial ? "F" : "C");
    puts_fit(87, 51, 222, line, false);
    rb->snprintf(line, sizeof(line), "Rain %s%%", day->precip);
    puts_fit(87, 70, 222, line, false);
    rb->lcd_set_foreground(WEATHER_CARD_BORDER);
    rb->lcd_hline(12, LCD_WIDTH - 13, 94);
    rb->lcd_hline(12, LCD_WIDTH - 13, 174);
    for (int i = MAX(0, first); i < weather.hour_count && shown < 6; i++)
    {
        struct weather_hour *hour = &weather.hours[i];
        int x = 11 + shown * 50;
        if (!hour_is_for_day(hour, day))
            continue;
        rb->lcd_set_foreground(WEATHER_MUTED);
        rb->snprintf(line, sizeof(line), "%2.2s:00", hour->stamp + 11);
        puts_fit(x, 99, 48, line, true);
        draw_weather_picture(x + 24, 123, 24, hour->code, hour_is_night(i));
        rb->lcd_set_foreground(WEATHER_TEXT);
        rb->snprintf(line, sizeof(line), "%s%s", hour->temp, imperial ? "F" : "C");
        puts_fit(x, 140, 48, line, true);
        rb->lcd_set_foreground(WEATHER_MUTED);
        rb->snprintf(line, sizeof(line), "%s%%", hour->precip);
        puts_fit(x, 157, 48, line, true);
        shown++;
    }
    if (!shown)
        puts_fit(12, 125, 296, "Hourly forecast unavailable", true);
    rb->lcd_set_foreground(WEATHER_TEXT);
    rb->snprintf(line, sizeof(line), "Wind %s %s  Bearing %s",
                 day->wind_speed, imperial ? "mph" : "km/h", day->wind_dir);
    puts_fit(12, 181, 296, line, false);
    rb->snprintf(line, sizeof(line), "Sunrise %s    Sunset %s", day->sunrise, day->sunset);
    puts_fit(12, 198, 296, line, false);
    rb->lcd_set_foreground(WEATHER_MUTED);
    puts_fit(10, 223, 300, "Select: Forecast      Menu: Back", true);
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
    long last_frame = *rb->current_tick;
    int last_hour;
#ifdef USB_ENABLE_ETHERNET
    unsigned long weather_generation;
#endif

    (void)parameter;
    weather.loaded = load_forecast();
    prepare_icons();
    last_hour = weather_now_key();
#ifdef USB_ENABLE_ETHERNET
    weather_generation = rb->usb_internet_weather_generation();
#endif
    draw_weather();

    while (!done)
    {
#ifdef USB_ENABLE_ETHERNET
        rb->usb_internet_service();
        if (weather_generation != rb->usb_internet_weather_generation())
        {
            weather_generation = rb->usb_internet_weather_generation();
            weather.loaded = load_forecast();
            weather.selected = 0;
            draw_weather();
        }
#endif
        action = rb->get_action(CONTEXT_LIST, MAX(1, HZ / 8));
        if (action == ACTION_NONE &&
            TIME_AFTER(*rb->current_tick, last_frame + MAX(1, HZ / 8)))
        {
            if (last_hour != weather_now_key())
            {
                last_hour = weather_now_key();
                weather_status();
            }
            if (weather.loaded && !weather.detail)
                draw_weather();
            last_frame = *rb->current_tick;
        }
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
