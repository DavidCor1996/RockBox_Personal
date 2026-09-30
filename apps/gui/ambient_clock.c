/***************************************************************************
 * Docked ambient clock -- cached, allocation-free composite idle surface.
 * Copyright (C) 2026
 * SPDX-License-Identifier: GPL-2.0-or-later
 ****************************************************************************/
#include "config.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "system.h"
#include "kernel.h"
#include "lcd.h"
#include "action.h"
#include "button.h"
#include "audio.h"
#include "settings.h"
#include "file.h"
#include "font.h"
#include "misc.h"
#include "string-extra.h"
#include "lang.h"
#include "notification_manager.h"
#include "timefuncs.h"
#include "playback.h"
#include "metadata.h"
#include "videoout.h"
#include "ambient_clock.h"
#include "viewport.h"
#include "screen_access.h"
#ifdef USB_ENABLE_ETHERNET
#include "usb_internet.h"
#endif
#include "ambient_clock_font.h"
#include "ambient_clock_icons.h"
#include "albumlist_art.h"
#include "bmp.h"
#include "tagcache.h"

#define PALETTES 8
#define GRID_W 41
#define GRID_H 31
#define WEATHER_PATH ROCKBOX_DIR "/rockpod/weather/forecast.tsv"

struct ambient_palette
{
    unsigned int color[3];
    unsigned long key;
};

static struct ambient_palette palettes[PALETTES];
static unsigned palette_count;
static unsigned palette_next;
static long last_poll;
static long idle_since;
static bool poll_initialized;
/* Small fixed drawing workspace; covers lease existing albumlist storage.
 * Glyph atlases are const ROM, never core allocations. */
static unsigned char field[GRID_H][GRID_W][3];
static fb_data scanline[LCD_WIDTH];
static char weather_line[512];
static struct ambient_weather
{
    char location[40];
    char condition[40];
    char temp[8];
    char units;
    unsigned char icon;
    time_t observed;
    bool valid, forecast;
} weather;

static bool clock_valid(const struct tm *tm)
{
    return tm && tm->tm_year >= 120 && tm->tm_year <= 199 &&
           tm->tm_mon >= 0 && tm->tm_mon < 12 &&
           tm->tm_mday >= 1 && tm->tm_mday <= 31 &&
           tm->tm_hour >= 0 && tm->tm_hour < 24 &&
           tm->tm_min >= 0 && tm->tm_min < 60;
}

static unsigned long hash_text(unsigned long hash, const char *text)
{
    if (text)
        while (*text)
            hash = (hash ^ (unsigned char)*text++) * 16777619u;
    return hash;
}

void ambient_clock_palette(unsigned int dominant, unsigned int accent)
{
    struct mp3entry *id3 = audio_current_track();
    unsigned long key = 2166136261u;
    unsigned slot = palette_count;
    if (!id3)
        return;
    key = hash_text(key, id3->albumartist ? id3->albumartist : id3->artist);
    key = hash_text(key ^ 255, id3->album ? id3->album : id3->path);
    for (unsigned i = 0; i < palette_count; ++i)
        if (palettes[i].key == key)
        {
            slot = i;
            break;
        }
    if (slot < palette_count)
    {
        memmove(&palettes[slot], &palettes[slot + 1],
                (palette_count - slot - 1) * sizeof(palettes[0]));
        slot = palette_count - 1;
    }
    else if (palette_count == PALETTES)
    {
        memmove(&palettes[0], &palettes[1],
                (PALETTES - 1) * sizeof(palettes[0]));
        slot = PALETTES - 1;
    }
    else
        ++palette_count;
    palettes[slot].key = key;
    palettes[slot].color[0] = dominant;
    palettes[slot].color[1] = accent;
    palettes[slot].color[2] = LCD_RGBPACK(
        (RGB_UNPACK_RED(dominant) + RGB_UNPACK_RED(accent)) / 2,
        (RGB_UNPACK_GREEN(dominant) + RGB_UNPACK_GREEN(accent)) / 2,
        (RGB_UNPACK_BLUE(dominant) + RGB_UNPACK_BLUE(accent)) / 2);
    palette_next = (slot + 1) % PALETTES;

}

bool ambient_clock_ready(bool activity)
{
    long now = current_tick;
    /* A gap means another screen owned the UI; never inherit its idle time. */
    if (!poll_initialized || TIME_AFTER(now, last_poll + 2 * HZ) || activity ||
        audio_status() || !videoout_active() ||
        !global_settings.ambient_clock ||
        button_hold() || button_status() || !button_queue_empty()
#ifdef HAVE_WHEEL_POSITION
        || wheel_status() >= 0
#endif
       )
    {
        idle_since = now;
        poll_initialized = true;
    }
    last_poll = now;
    return TIME_AFTER(now, idle_since +
                      MAX(1, MIN(10, global_settings.ambient_delay)) * 60 * HZ);
}

static char *next_field(char **cursor)
{
    char *result = *cursor;
    char *tab = strchr(result, '\t');
    if (tab)
    {
        *tab = '\0';
        *cursor = tab + 1;
    }
    else
    {
        *cursor = result + strlen(result);
        result[strcspn(result, "\r\n")] = '\0';
    }
    return result;
}

static time_t parse_stamp(const char *stamp)
{
    struct tm tm = {0};
    int year, month, day, hour, minute;
    if (strlen(stamp) != 16 || stamp[4] != '-' || stamp[7] != '-' ||
        stamp[10] != 'T' || stamp[13] != ':')
        return 0;
    for (int i = 0; i < 16; ++i)
        if (i != 4 && i != 7 && i != 10 && i != 13 &&
            (stamp[i] < '0' || stamp[i] > '9'))
            return 0;
    year = atoi(stamp);
    month = atoi(stamp + 5);
    day = atoi(stamp + 8);
    hour = atoi(stamp + 11);
    minute = atoi(stamp + 14);
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_isdst = -1;
    static const unsigned char days[] =
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (!clock_valid(&tm) || day > days[month - 1] +
        (month == 2 && year % 4 == 0))
        return 0;
    return mktime(&tm);
}

/* One bounded cache read per entry, never called by a render function. */
static unsigned weather_icon(const char *code, bool day)
{
    if (!strcmp(code, "clear"))
        return day ? 0 : 1;
    if (!strcmp(code, "partly_cloudy"))
        return day ? 2 : 3;
    static const char *codes[] = {
        "cloudy", "drizzle", "rain", "snow", "fog", "thunderstorm"
    };
    for (unsigned i = 0; i < ARRAYLEN(codes); ++i)
        if (!strcmp(code, codes[i]))
            return i + 4;
    return 4;
}

static void weather_load(void)
{
    int fd;
    char *cursor;
    memset(&weather, 0, sizeof(weather));
    if (!global_settings.ambient_weather)
        return;
    fd = open(WEATHER_PATH, O_RDONLY);
    if (fd < 0)
        return;
    if (read_line(fd, weather_line, sizeof(weather_line)) <= 0)
        goto done;
    cursor = weather_line;
    if (strcmp(next_field(&cursor), "rockpod_weather_v1"))
        goto done;
    strlcpy(weather.location, next_field(&cursor), sizeof(weather.location));
    for (int i = 0; i < 5; ++i)
        next_field(&cursor);
    weather.units = !strcmp(next_field(&cursor), "imperial") ? 'F' : 'C';
    struct tm now_tm = *get_time();
    now_tm.tm_isdst = -1;
    time_t now = clock_valid(&now_tm) ? mktime(&now_tm) : 0;
    int best_priority = 0;
    for (int i = 0; i < 420 && button_queue_empty() && !audio_status(); ++i)
    {
        if (read_line(fd, weather_line, sizeof(weather_line)) <= 0)
            break;
        cursor = weather_line;
        char *type = next_field(&cursor);
        bool forecast = !strcmp(type, "hourly");
        if (forecast || !strcmp(type, "current"))
        {
            struct ambient_weather candidate = weather;
            char *end, code[24];
            candidate.observed = parse_stamp(next_field(&cursor));
            strlcpy(code, next_field(&cursor), sizeof(code));
            strlcpy(candidate.condition, next_field(&cursor),
                    sizeof(candidate.condition));
            strlcpy(candidate.temp, next_field(&cursor), sizeof(candidate.temp));
            for (int field = 0; field < 3; ++field)
                next_field(&cursor);
            candidate.icon = weather_icon(code, strcmp(next_field(&cursor), "0"));
            long temp = strtol(candidate.temp, &end, 10);
            candidate.valid = candidate.observed && end != candidate.temp &&
                              !*end && temp >= -150 && temp <= 150;
            candidate.forecast = forecast;
            long age = (long)(now - candidate.observed);
            if (!candidate.valid || !now || age < 0 ||
                age >= (forecast ? 3600 : 86401))
                continue;
            /* Fresh live weather wins; otherwise use this hour's synced
             * forecast before showing an older observation with its age. */
            int priority = forecast ? 2 : age < 10800 ? 3 : 1;
            if (priority > best_priority ||
                (priority == best_priority && candidate.observed > weather.observed))
            {
                weather = candidate;
                best_priority = priority;
            }
        }
        yield();
    }
 done:
    close(fd);
}

static int smooth(int t)
{
    return (t * t / 256) * (768 - 2 * t) / 256;
}

static int triangle(unsigned long seconds, unsigned period)
{
    int t = seconds % period;
    return t < (int)period / 2 ? t * 512 / period :
                               (period - t) * 512 / period;
}

/* Linear-light interpolation using a square-law approximation. */
static int blend(int a, int b, int weight)
{
    unsigned v = (a * a * (256 - weight) + b * b * weight) / 256;
    unsigned root = 0;
    for (unsigned bit = 128; bit; bit >>= 1)
        if ((root + bit) * (root + bit) <= v)
            root += bit;
    return root;
}

static void make_field(unsigned long elapsed)
{
    static const unsigned int fallback[3] = {
        LCD_RGBPACK(70, 86, 156), LCD_RGBPACK(170, 88, 57),
        LCD_RGBPACK(121, 57, 113)
    };
    static const unsigned int neutral[3] = {
        LCD_RGBPACK(92, 100, 112), LCD_RGBPACK(92, 100, 112),
        LCD_RGBPACK(92, 100, 112)
    };
    int color[3][3];
    unsigned long seconds = elapsed / HZ;
    bool still = global_settings.ambient_reduced_motion;
    unsigned cycle = still ? 0 : seconds / 90;
    int weight = still || seconds % 90 < 60 ? 0 :
                 smooth((seconds % 90 - 60) * 256 / 30);
    unsigned count = palette_count;
    const unsigned int *a = global_settings.ambient_colors == 2 ?
                            neutral : fallback;
    const unsigned int *b = a;
    if (count && global_settings.ambient_colors != 2)
    {
        if (global_settings.ambient_colors == 1)
            cycle = weight = 0;
        unsigned latest = (palette_next + PALETTES - 1) % PALETTES;
        a = palettes[(latest + count - cycle % count) % count].color;
        b = palettes[(latest + count - (cycle + 1) % count) % count].color;
    }
    for (int i = 0; i < 3; ++i)
    {
        color[i][0] = blend(RGB_UNPACK_RED(a[i]), RGB_UNPACK_RED(b[i]), weight);
        color[i][1] = blend(RGB_UNPACK_GREEN(a[i]),
                            RGB_UNPACK_GREEN(b[i]), weight);
        color[i][2] = blend(RGB_UNPACK_BLUE(a[i]),
                            RGB_UNPACK_BLUE(b[i]), weight);
    }
    int drift = still ? 0 : (triangle(seconds, 240) - 128) * 12 / 128;
    int drift2 = still ? 0 : (triangle(seconds, 193) - 128) * 12 / 128;
    int cx[3] = {230 + drift, 36 - drift2, 310 - drift};
    int cy[3] = {24 + drift2, 218 + drift, 152 - drift2};
    for (int y = 0; y < GRID_H; ++y)
        for (int x = 0; x < GRID_W; ++x)
        {
            int rgb[3] = {8, 12, 20};
            for (int i = 0; i < 3; ++i)
            {
                int dx = x * 8 - cx[i], dy = y * 8 - cy[i];
                int w = MAX(0, 256 - (dx * dx + dy * dy) / 105);
                w = w * w / 256;
                for (int c = 0; c < 3; ++c)
                    rgb[c] += color[i][c] * w / 560;
            }
            for (int c = 0; c < 3; ++c)
                field[y][x][c] = MIN(255, rgb[c]);
        }
}

struct label
{
    char text[80];
    int y;
    bool large;
    int brightness;
    int center;
    int start_x;
};

static int glyph(const unsigned char **text, bool large)
{
    unsigned ch = *(*text)++;
    if (large)
        return ch == ':' ? 10 : MIN(9, MAX(0, (int)ch - '0'));
    if (ch == 0xc2 && **text == 0xb0)
    {
        ++*text;
        return 95;
    }
    if (ch >= 128)
    {
        while ((**text & 0xc0) == 0x80)
            ++*text;
        return '?' - 32;
    }
    return ch >= 32 && ch <= 126 ? ch - 32 : '?' - 32;
}

static int label_width(const struct label *label)
{
    const unsigned char *s = (const unsigned char *)label->text;
    int width = 0;
    while (*s)
    {
        int g = glyph(&s, label->large);
        width += label->large ? (g == 10 ? 26 : 60) :
                                ambient_text_width[g];
    }
    return width;
}

/* Center visible ink, including narrow first/last digits and punctuation. */
static void label_bounds(const struct label *label, int *left, int *right)
{
    const unsigned char *s = (const unsigned char *)label->text;
    const unsigned char *bounds = label->large ? ambient_digits_bounds :
                                               ambient_text_bounds;
    int cursor = 0;
    *left = LCD_WIDTH;
    *right = 0;
    while (*s)
    {
        int g = glyph(&s, label->large);
        if (bounds[g * 2 + 1])
        {
            *left = MIN(*left, cursor + bounds[g * 2]);
            *right = MAX(*right, cursor + bounds[g * 2 + 1]);
        }
        cursor += label->large ? (g == 10 ? 26 : 60) : ambient_text_width[g];
    }
    if (!*right)
        *left = 0;
}

static void paint_label(const struct label *label, int y, int dx, int dy,
                        int level)
{
    int row = y - label->y - dy;
    int w = label->large ? 60 : 16;
    int h = label->large ? 100 : 20;
    int x = label->start_x + dx;
    const unsigned char *s = (const unsigned char *)label->text;
    if (row < 0 || row >= h)
        return;
    while (*s)
    {
        int g = glyph(&s, label->large);
        const unsigned char *bits = label->large ? ambient_digits_bits :
                                                  ambient_text_bits;
        int advance = label->large ? (g == 10 ? 26 : 60) :
                                    ambient_text_width[g];
        for (int col = 0; col < advance; ++col)
        {
            int px = x + col;
            if (px < 24 || px >= LCD_WIDTH - 24)
                continue;
            unsigned pos = g * w * h + row * w + col;
            int alpha = (bits[pos / 2] >> ((pos & 1) ? 0 : 4)) & 15;
            if (!alpha)
                continue;
            unsigned bg = FB_UNPACK_SCALAR_LCD(scanline[px]);
            int r = RGB_UNPACK_RED(bg), b = RGB_UNPACK_BLUE(bg);
            int green = RGB_UNPACK_GREEN(bg);
            int fg = label->brightness * level / 256;
            if (label->large)
            {
                /* Two-pixel bevel from the licensed glyph mask. Refract
                 * still-unpainted pixels to the right, so the photograph
                 * stays visible inside the glass, without a new frame copy. */
                int near[4] = {0, 0, 0, 0};
                int offsets[4] = {-2*w, -2, 2*w, 2};
                bool inside[4] = {row >= 2, col >= 2,
                                  row + 2 < h, col + 2 < w};
                for (int edge = 0; edge < 4; ++edge)
                    if (inside[edge])
                    {
                        unsigned p = pos + offsets[edge];
                        near[edge] = (bits[p/2] >> ((p&1) ? 0 : 4)) & 15;
                    }
                int light = MAX(0, MAX(alpha-near[0], alpha-near[1]));
                int rim = MAX(light, MAX(alpha-near[2], alpha-near[3]));
                unsigned refracted = FB_UNPACK_SCALAR_LCD(
                    scanline[MIN(LCD_WIDTH-1, px+2)]);
                int lift = (78 + (h-row)*42/h) * level/256;
                int glass[3] = {
                    MIN(255, RGB_UNPACK_RED(refracted)*3/4 + lift),
                    MIN(255, RGB_UNPACK_GREEN(refracted)*3/4 + lift),
                    MIN(255, RGB_UNPACK_BLUE(refracted)*3/4 + lift + 5*level/256)
                };
                /* A white upper edge and cooler lower reflection make the
                 * numerals read as curved transparent material. */
                int reflection = MIN(255, 216 + light*2) * level/256;
                for (int c = 0; c < 3; ++c)
                    glass[c] += (reflection-glass[c])*rim/15;
                scanline[px] = FB_SCALARPACK_LCD(LCD_RGBPACK(
                    r + (glass[0]-r)*alpha/15,
                    green + (glass[1]-green)*alpha/15,
                    b + (glass[2]-b)*alpha/15));
                continue;
            }
            scanline[px] = FB_SCALARPACK_LCD(LCD_RGBPACK(
                r + (fg - r) * alpha / 15,
                green + (fg - green) * alpha / 15,
                b + (fg * 98 / 100 - b) * alpha / 15));
        }
        x += advance;
    }
}

#define ART_ROOT ROCKBOX_DIR "/albumlist"
#define ART_HOLD (24 * HZ)
#define ART_FADE (6 * HZ)
/* The two decoded covers borrow existing albumlist slots. No framebuffer,
 * audio-buffer or core-arena allocation is made by the ambient screen. */
static struct {
    struct bitmap image[2];
    size_t capacity[2];
    int current, fd, candidates, failures;
    bool ready, pending, loading, discard_tail;
    long shown, retry;
    uint32_t random;
    char line[1024], selected[MAX_PATH], fallback[MAX_PATH];
    char previous[MAX_PATH], selected_id[MAX_PATH];
} art;

static uint32_t art_random(void)
{
    uint32_t n = art.random;
    n ^= n << 13; n ^= n >> 17; n ^= n << 5;
    return art.random = n;
}

static void art_close(void)
{
    if (art.fd >= 0)
        close(art.fd);
    art.fd = -1;
}

static void art_begin(void)
{
    memset(&art, 0, sizeof(art));
    art.fd = -1;
    art.random = (uint32_t)current_tick ^ (uint32_t)mktime(get_time()) ^ 0x91e10da5;
    if (!art.random) art.random = 1;
    for (int i = 0; i < 2; ++i)
        art.image[i].data = albumlist_ambient_workspace(i, &art.capacity[i]);
}

static void art_path(char *dest, const char *path)
{
    if (*path == '/')
        strlcpy(dest, path, MAX_PATH);
    else
        snprintf(dest, MAX_PATH, "%s/%s", ART_ROOT, path);
}

/* One bounded catalog batch OR one decode per idle call, never from draw().
 * Reservoir sampling scans every row, so library size has no RAM/cap limit.
 * Exclude the current album; a one-album library simply keeps its cover. */
static void art_service(void)
{
    if (!button_queue_empty() || button_status() || button_hold() ||
        audio_status() || tagcache_commit_active() ||
#ifdef HAVE_WHEEL_POSITION
        wheel_status() >= 0 ||
#endif
        !TIME_AFTER(current_tick, art.retry))
        return;
    if (!art.image[0].data || !art.image[1].data)
        return;
    if (art.pending)
    {
        if (TIME_AFTER(current_tick, art.shown + ART_HOLD + ART_FADE))
        {
            art.current ^= 1;
            art.pending = false;
            art.shown = current_tick;
        }
        return;
    }
    if (art.ready && !TIME_AFTER(current_tick, art.shown + 2 * HZ))
        return;
    if (art.loading)
    {
        int slot = art.ready ? art.current ^ 1 : art.current;
        struct bitmap *bm = &art.image[slot];
        bm->width = bm->height = 384;
        bm->format = FORMAT_NATIVE;
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
        cpu_boost(true);
#endif
        int result = read_bmp_file(art.selected, bm, art.capacity[slot],
            FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT, NULL);
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
        cpu_boost(false);
#endif
        if (result > 0 && bm->width > 0 && bm->height > 0)
        {
            art.loading = false;
            strlcpy(art.previous, art.selected_id, sizeof(art.previous));
            art.failures = 0;
            if (art.ready)
            {
                art.pending = true;
                /* A slow catalog/decode still gets the complete fade. */
                if (TIME_AFTER(current_tick, art.shown + ART_HOLD))
                    art.shown = current_tick - ART_HOLD;
            }
            else
            {
                art.ready = true;
                art.shown = current_tick;
            }
        }
        else if (art.fallback[0])
        {
            strlcpy(art.selected, art.fallback, sizeof(art.selected));
            art.fallback[0] = '\0';
        }
        else
        {
            art.loading = false;
            art.retry = current_tick + (++art.failures >= 4 ? 60 * HZ : HZ);
        }
        return;
    }
    if (art.fd < 0)
    {
        art.fd = open(ART_ROOT "/index.tsv", O_RDONLY);
        art.candidates = 0;
        art.discard_tail = false;
        if (art.fd < 0)
        {
            art.retry = current_tick + 60 * HZ;
            return;
        }
    }
    for (int batch = 0; batch < 16; ++batch)
    {
        bool continuation = art.discard_tail;
        int n = read_line(art.fd, art.line, sizeof(art.line));
        art.discard_tail = strlen(art.line) == sizeof(art.line) - 1;
        if (n <= 0)
        {
            art_close();
            art.loading = art.candidates > 0;
            if (!art.loading) art.retry = current_tick + 60 * HZ;
            return;
        }
        if (continuation) continue;
        if (art.line[0] == '#' || !strncmp(art.line, "album_id\t", 9))
            continue;
        char *fields[7], *cursor = art.line;
        int count = 0;
        while (count < 7)
        {
            bool more = strchr(cursor, '\t') != NULL;
            fields[count++] = next_field(&cursor);
            if (!more) break;
        }
        if (count < 6 || !fields[0][0] || !strcmp(fields[0], art.previous))
            continue;
        ++art.candidates;
        if (art_random() % (unsigned)art.candidates)
            continue;
        strlcpy(art.selected_id, fields[0], sizeof(art.selected_id));
        if (count >= 7 && fields[2][0])
            art_path(art.selected, fields[2]);
        else
            snprintf(art.selected, sizeof(art.selected), "%s/slides/%s.bmp",
                     ART_ROOT, fields[0]);
        art.fallback[0] = '\0';
        if (fields[1][0]) art_path(art.fallback, fields[1]);
    }
    yield();
}

/* Bilinear, aspect-fill sampling from cached pixels. Slow six-pixel drift
 * keeps cover photography alive without exposing borders or stretching it. */
static unsigned art_pixel(int slot, int x, int y, unsigned long elapsed)
{
    struct bitmap *bm = &art.image[slot];
    int scale = MIN(bm->width * 256 / LCD_WIDTH,
                    bm->height * 256 / LCD_HEIGHT);
    scale = MAX(1, scale * 97 / 100);
    int pan = global_settings.ambient_reduced_motion ? 0 :
              triangle(elapsed / 4 + slot * 800, 1200) * 6 - 768;
    int sx = (bm->width * 256 - LCD_WIDTH * scale) / 2 + x * scale;
    int sy = (bm->height * 256 - LCD_HEIGHT * scale) / 2 + y * scale + pan;
    sx = MAX(0, MIN((bm->width - 1) * 256, sx));
    sy = MAX(0, MIN((bm->height - 1) * 256, sy));
    int ix = sx / 256, iy = sy / 256, fx = sx % 256, fy = sy % 256;
    int nx = MIN(ix + 1, bm->width - 1), ny = MIN(iy + 1, bm->height - 1);
    fb_data *pixels = (fb_data *)bm->data;
    unsigned a = FB_UNPACK_SCALAR_LCD(pixels[iy * bm->width + ix]);
    unsigned b = FB_UNPACK_SCALAR_LCD(pixels[iy * bm->width + nx]);
    unsigned c = FB_UNPACK_SCALAR_LCD(pixels[ny * bm->width + ix]);
    unsigned d = FB_UNPACK_SCALAR_LCD(pixels[ny * bm->width + nx]);
#define ART_LERP(channel) (((channel(a) * (256-fx) + channel(b) * fx) * (256-fy) + \
                           (channel(c) * (256-fx) + channel(d) * fx) * fy) / 65536)
    unsigned result = LCD_RGBPACK(ART_LERP(RGB_UNPACK_RED),
                        ART_LERP(RGB_UNPACK_GREEN), ART_LERP(RGB_UNPACK_BLUE));
#undef ART_LERP
    return result;
}

static void draw(unsigned long elapsed)
{
    struct tm now = *get_time();
    now.tm_isdst = -1;
    struct label labels[5] = {
        { "", 28, true, 238, 0, 0 }, { "", 20, false, 220, 0, 0 },
        { "", 169, false, 240, 0, 0 }, { "", 190, false, 214, 0, 0 },
        { "", 134, false, 190, 0, 0 }
    };
    bool valid = clock_valid(&now);
    bool show_icon = false;
    if (valid)
    {
        int hour = now.tm_hour;
        hour = (hour + 11) % 12 + 1;
        snprintf(labels[0].text, sizeof(labels[0].text), "%d:%02d",
                 hour, now.tm_min);
        const char *day = str(LANG_WEEKDAY_SUNDAY +
                              MAX(0, MIN(6, now.tm_wday)));
        const char *month = str(LANG_MONTH_JANUARY + now.tm_mon);
        bool supported = true;
        for (const unsigned char *p = (const unsigned char *)day; *p; ++p)
            supported &= *p >= 32 && *p < 127;
        for (const unsigned char *p = (const unsigned char *)month; *p; ++p)
            supported &= *p >= 32 && *p < 127;
        if (supported)
            snprintf(labels[1].text, sizeof(labels[1].text), "%s, %s %d",
                     day, month, now.tm_mday);
        else
            snprintf(labels[1].text, sizeof(labels[1].text), "%04d-%02d-%02d",
                     now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);
        strlcpy(labels[4].text, now.tm_hour < 12 ? "AM" : "PM",
                sizeof(labels[4].text));
    }
    else
        strlcpy(labels[1].text, "Set time in Settings", sizeof(labels[1].text));
    if (global_settings.ambient_weather)
    {
        long age = valid && weather.valid ?
                   (long)(mktime(&now) - weather.observed) : -1;
        if (age >= 0 && age <= 86400 && (!weather.forecast || age < 3600))
        {
            show_icon = true;
            snprintf(labels[2].text, sizeof(labels[2].text), "%s°%c  %s",
                     weather.temp, weather.units, weather.condition);
            if (weather.forecast)
                snprintf(labels[3].text, sizeof(labels[3].text),
                         "Forecast: %s", weather.location);
            else if (age >= 10800)
                snprintf(labels[3].text, sizeof(labels[3].text),
                         "Updated %ldh ago", age / 3600);
            else
                strlcpy(labels[3].text, weather.location,
                        sizeof(labels[3].text));
        }
        else
            strlcpy(labels[3].text, "Weather unavailable",
                    sizeof(labels[3].text));
    }
    /* Ellipsize bounded text rather than letting long cache fields overlap. */
    for (int i = 1; i < 5; ++i)
    {
        size_t len = strlen(labels[i].text);
        int max_width = i == 2 ? (show_icon ? 150 : 184) :
                        i == 3 ? 184 : 256;
        if (label_width(&labels[i]) > max_width)
        {
            do
            {
                --len;
                while (len &&
                       ((unsigned char)labels[i].text[len] & 0xc0) == 0x80)
                    --len;
                labels[i].text[len] = '\0';
            } while (len && label_width(&labels[i]) > max_width - 16);
            strlcat(labels[i].text, "...", sizeof(labels[i].text));
        }
    }
    int icon_x = 0;
    if (show_icon)
    {
        int left, right;
        label_bounds(&labels[2], &left, &right);
        int width = right - left;
        unsigned icon = MIN(9, weather.icon);
        int icon_left = ambient_icon_bounds[icon * 2];
        int icon_right = ambient_icon_bounds[icon * 2 + 1];
        int total = width + 10 + icon_right - icon_left;
        icon_x = (LCD_WIDTH - total) / 2 - icon_left;
        labels[2].center = icon_x + icon_right + 10 + width / 2;
    }
    for (int i = 0; i < 5; ++i)
    {
        int left, right;
        label_bounds(&labels[i], &left, &right);
        int center = labels[i].center ? labels[i].center : LCD_WIDTH / 2;
        labels[i].start_x = center - (left + right) / 2;
    }
    unsigned long seconds = elapsed / HZ;
    int dx = triangle(seconds + 180, 720) * 8 / 256 - 4;
    int dy = triangle(seconds + 270, 1080) * 6 / 256 - 3;
    int brightness[] = {150, 232, 256};
    int level = brightness[MAX(0, MIN(2, global_settings.ambient_brightness))];
    if (elapsed < 3 * HZ / 4)
        level = level * elapsed / (3 * HZ / 4);
    make_field(elapsed);
    int fade = art.pending ? MAX(0, MIN(256,
        (current_tick - art.shown - ART_HOLD) * 256 / ART_FADE)) : 0;
    /* Smoothstep keeps the start and finish of a dissolve gentle. */
    fade = fade * fade * (768 - 2 * fade) / 65536;
    if (art.ready && global_settings.ambient_weather)
    {
        /* A tiny nine-tap glass backdrop, reusing the existing color grid. */
        for (int gy = 0; gy < GRID_H; ++gy)
            for (int gx = 0; gx < GRID_W; ++gx)
            {
                int rgb[3] = {0, 0, 0};
                for (int oy = -1; oy <= 1; ++oy)
                    for (int ox = -1; ox <= 1; ++ox)
                    {
                        int x = MAX(0, MIN(LCD_WIDTH-1, gx*8+ox*12));
                        int y = MAX(0, MIN(LCD_HEIGHT-1, gy*8+oy*12));
                        unsigned a = art_pixel(art.current, x, y, elapsed);
                        unsigned b = fade ? art_pixel(art.current^1, x, y, elapsed) : a;
                        rgb[0] += (RGB_UNPACK_RED(a)*(256-fade)+RGB_UNPACK_RED(b)*fade)/256;
                        rgb[1] += (RGB_UNPACK_GREEN(a)*(256-fade)+RGB_UNPACK_GREEN(b)*fade)/256;
                        rgb[2] += (RGB_UNPACK_BLUE(a)*(256-fade)+RGB_UNPACK_BLUE(b)*fade)/256;
                    }
                for (int c = 0; c < 3; ++c) field[gy][gx][c] = rgb[c] * 100 / (9*256);
            }
    }
    static const unsigned char dither[4][4] = {
        {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}
    };
    for (int y = 0; y < LCD_HEIGHT; ++y)
    {
        int gy = y / 8, fy = y % 8;
        for (int x = 0; x < LCD_WIDTH; ++x)
        {
            int gx = x / 8, fx = x % 8;
            int rgb[3];
            for (int c = 0; c < 3; ++c)
            {
                int a = field[gy][gx][c] * (8 - fx) + field[gy][gx + 1][c] * fx;
                int b = field[gy + 1][gx][c] * (8 - fx) +
                        field[gy + 1][gx + 1][c] * fx;
                rgb[c] = ((a * (8 - fy) + b * fy) / 64) * level / 256;
            }
            if (art.ready)
            {
                unsigned a = art_pixel(art.current, x, y, elapsed);
                unsigned b = fade ? art_pixel(art.current ^ 1, x, y, elapsed) : a;
                rgb[0] = (RGB_UNPACK_RED(a) * (256-fade) + RGB_UNPACK_RED(b)*fade)/256;
                rgb[1] = (RGB_UNPACK_GREEN(a) * (256-fade) + RGB_UNPACK_GREEN(b)*fade)/256;
                rgb[2] = (RGB_UNPACK_BLUE(a) * (256-fade) + RGB_UNPACK_BLUE(b)*fade)/256;
                /* A consistent scrim preserves the photo and clock contrast. */
                for (int c = 0; c < 3; ++c) rgb[c] = rgb[c] * level * 132 / 65536;
            }
            int noise = dither[y & 3][x & 3];
            scanline[x] = FB_SCALARPACK_LCD(LCD_RGBPACK(
                MIN(255, rgb[0] + noise / 2),
                MIN(255, rgb[1] + noise / 4),
                MIN(255, rgb[2] + noise / 2)));
        }
        if (global_settings.ambient_weather)
        {
            /* A single frosted capsule; the underlying field is already
             * low-frequency, so no blur buffer is needed. */
            int top = 162 + dy, bottom = 218 + dy;
            int left = 56 + dx, right = 264 + dx;
            int radius = 24;
            if (y >= top && y < bottom)
                for (int x = left; x < right; ++x)
                {
                    int cx = MAX(left + radius, MIN(right - radius - 1, x));
                    int cy = MAX(top + radius, MIN(bottom - radius - 1, y));
                    int dist = (x - cx) * (x - cx) + (y - cy) * (y - cy);
                    if (dist > radius * radius)
                        continue;
                    unsigned bg = FB_UNPACK_SCALAR_LCD(scanline[x]);
                    if (art.ready)
                    {
                        int rgb[3], gx = x/8, fx = x%8, gy = y/8, fy = y%8;
                        for (int c = 0; c < 3; ++c)
                            rgb[c] = ((field[gy][gx][c]*(8-fx)+field[gy][gx+1][c]*fx)*(8-fy) +
                                (field[gy+1][gx][c]*(8-fx)+field[gy+1][gx+1][c]*fx)*fy)*level/16384;
                        bg = LCD_RGBPACK(rgb[0], rgb[1], rgb[2]);
                    }
                    int rim = y == top || x == left ||
                              dist > (radius - 1) * (radius - 1);
                    int shine = (rim ? 48 : 24 + (bottom - y) / 5) *
                                level / 256;
                    scanline[x] = FB_SCALARPACK_LCD(LCD_RGBPACK(
                        MIN(255, RGB_UNPACK_RED(bg) + shine),
                        MIN(255, RGB_UNPACK_GREEN(bg) + shine),
                        MIN(255, RGB_UNPACK_BLUE(bg) + shine)));
                }
        }
        if (show_icon && y >= 168 + dy && y < 192 + dy)
        {
            unsigned icon = MIN(9, weather.icon);
            int row = y - 168 - dy;
            unsigned tint = icon < 4 ? LCD_RGBPACK(255, 218, 142) :
                icon == 6 || icon == 7 ? LCD_RGBPACK(176, 217, 255) :
                LCD_RGBPACK(236, 240, 248);
            for (int col = 0; col < 24; ++col)
            {
                int x = icon_x + dx + col;
                unsigned pos = row * 24 + col;
                int alpha = (ambient_icon_bits[icon][pos / 2] >>
                            ((pos & 1) ? 0 : 4)) & 15;
                unsigned bg = FB_UNPACK_SCALAR_LCD(scanline[x]);
                int r = RGB_UNPACK_RED(bg), g = RGB_UNPACK_GREEN(bg);
                int b = RGB_UNPACK_BLUE(bg);
                scanline[x] = FB_SCALARPACK_LCD(LCD_RGBPACK(
                    r + (RGB_UNPACK_RED(tint) * level / 256 - r) * alpha / 15,
                    g + (RGB_UNPACK_GREEN(tint) * level / 256 - g) * alpha / 15,
                    b + (RGB_UNPACK_BLUE(tint) * level / 256 - b) *
                        alpha / 15));
            }
        }
        for (int i = 0; i < 5; ++i)
            paint_label(&labels[i], y, dx, dy, level);
        lcd_bitmap(scanline, 0, y, LCD_WIDTH, 1);
        if (!(y % 16))
            yield();
    }
    lcd_update();
}

int ambient_clock_run(bool preview)
{
    int result = ACTION_NONE;
    if (audio_status() || button_hold() ||
        (!preview && !videoout_active()))
        return result;
    DEBUGF("ambient: preparing preview=%d\n", preview);
    weather_load();
    if (audio_status() || !button_queue_empty() ||
        (!preview && !videoout_active()))
        return result;
    struct viewport viewport = {
        .x = 0, .y = 0, .width = LCD_WIDTH, .height = LCD_HEIGHT,
        .font = FONT_UI, .drawmode = DRMODE_SOLID, .buffer = NULL
    };
    struct viewport *saved = lcd_current_viewport;
    fb_data *backdrop = lcd_get_backdrop();
    bool banners_suppressed = notification_manager_banners_suppressed();
    notification_manager_set_banners_suppressed(true);
    viewportmanager_theme_enable(SCREEN_MAIN, false, NULL);
    lcd_init_viewport(&viewport);
    lcd_set_viewport(&viewport);
    lcd_set_backdrop(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
#ifdef USB_ENABLE_ETHERNET
    unsigned long weather_generation = usb_internet_weather_generation();
#endif
    DEBUGF("ambient: entered\n");
    long started = current_tick;
    long next_frame = started;
    struct tm weather_tm = *get_time();
    weather_tm.tm_isdst = -1;
    time_t weather_hour = mktime(&weather_tm) / 3600;
    art_begin();
    while (preview || videoout_active())
    {
        if (audio_status())
        {
            result = ACTION_TREE_WPS;
            break;
        }
        int action = get_action(CONTEXT_TREE | ALLOW_SOFTLOCK, HZ / 20);
        if (action == SYS_CHARGER_CONNECTED ||
            action == SYS_CHARGER_DISCONNECTED || action == SYS_BATTERY_UPDATE)
        {
            default_event_handler(action);
            continue;
        }
        if (IS_SYSEVENT(action) || action == ACTION_TREE_WPS ||
            action == ACTION_TREE_STOP || action == ACTION_TREE_POWER_MENU
#ifdef HAVE_VOLUME_IN_LIST
            || action == ACTION_LIST_VOLUP || action == ACTION_LIST_VOLDOWN
#endif
           )
        {
            result = action;
            break;
        }
        if (action != ACTION_NONE && action != ACTION_UNKNOWN &&
            action != ACTION_REDRAW)
            break;
        /* Backlight filtering may consume the mapped action. Raw activity
         * must still dismiss an unattended display on its first press. */
        if (button_hold() || button_status() || !button_queue_empty()
#ifdef HAVE_WHEEL_POSITION
            || wheel_status() >= 0
#endif
           )
        {
#ifdef BUTTON_PLAY
            if (button_status() == BUTTON_PLAY)
                result = ACTION_TREE_WPS;
#endif
            action_wait_for_release();
            break;
        }
#ifdef USB_ENABLE_ETHERNET
        /* Only an explicit cache update triggers this read. */
        unsigned long generation = usb_internet_weather_generation();
        if (generation != weather_generation && button_queue_empty())
        {
            weather_load();
            weather_generation = generation;
        }
#endif
        /* Hourly forecasts advance while the iPod stays docked overnight.
         * Read once on an hour change, not from a drawing callback. */
        weather_tm = *get_time();
        weather_tm.tm_isdst = -1;
        time_t hour = mktime(&weather_tm) / 3600;
        if (hour != weather_hour && button_queue_empty())
        {
            weather_load();
            weather_hour = hour;
        }
        if (TIME_AFTER(current_tick, started + HZ / 2))
            art_service();
        if (TIME_AFTER(current_tick, next_frame) && button_queue_empty())
        {
            draw((unsigned long)(current_tick - started));
            next_frame = current_tick + (art.pending ? HZ / 8 : HZ / 4);
        }
    }
    art_close();
    albumlist_ambient_release();
    DEBUGF("ambient: exit action=%d\n", result);
    viewportmanager_theme_undo(SCREEN_MAIN, false);
    lcd_set_backdrop(backdrop);
    /* List hosts repaint only their viewport. Remove the full-screen photo
     * from the restored theme's margins as well. */
    lcd_set_viewport(NULL);
    lcd_clear_display();
    lcd_update();
    lcd_set_viewport(saved);
    notification_manager_set_banners_suppressed(banners_suppressed);
    idle_since = last_poll = current_tick;
    return result;
}
