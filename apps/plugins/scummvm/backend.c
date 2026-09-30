/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 Rockpod contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/

#include "scummvm.h"
#include "fixedpoint.h"
#include "agds_runtime.h"
#include "engine.h"
#include "probe.h"
#include "queen_runtime.h"
#include "rbfile.h"
#include "sky_cpt_loader.h"
#include "sky_runtime.h"
#include "upstream_bridge.h"
#include "video.h"
#include "lib/pluginlib_actions.h"
#ifdef SIMULATOR
#include <stdlib.h>
#endif

#define SCUMMVM_CURSOR_STEP 4
#define SCUMMVM_CURSOR_FAST_STEP 12
#define AGDS_BITMAP_FONT_CACHE 3
#define AGDS_BITMAP_FONT_BYTES 16384
#define AGDS_FNT_HEADER_SIZE 36u

struct agds_bitmap_font {
    char descriptor[AGDS_ADB_NAME_SIZE];
    uint16_t authored_height;
    uint16_t max_width;
    uint16_t height;
    uint16_t ascent;
    uint16_t depth;
    uint32_t first_char;
    uint32_t default_char;
    uint32_t glyph_count;
    uint32_t bits_size;
    uint32_t offsets_at;
    uint32_t widths_at;
    uint32_t stamp;
    uint32_t size;
    bool valid;
    unsigned char data[AGDS_BITMAP_FONT_BYTES];
};

static struct agds_bitmap_font
    agds_bitmap_fonts[AGDS_BITMAP_FONT_CACHE];
static uint32_t agds_bitmap_font_stamp;

struct scummvm_backend {
    const struct scummvm_target *target;
    struct scummvm_probe_result probe;
    struct scummvm_engine_state engine;
    struct scummvm_video video;
    int cursor_x;
    int cursor_y;
    bool mouse_down;
    bool click_pending;
    bool look_pending;
    bool space_pending;
    bool tab_pending;
    bool escape_pending;
    bool menu_open;
#ifdef HAVE_WHEEL_POSITION
    int last_wheel;
    int wheel_velocity;
    int wheel_direction;
    long last_wheel_tick;
    bool wheel_available;
    long glide_since;
    long glide_tick;
    long glide_emit;
    int glide_x;
    int glide_y;
    int frac_x;
    int frac_y;
#endif
};

#include "agds_pointer.h"

static const struct button_mapping *scummvm_contexts[] = {
    pla_main_ctx,
#if defined(HAVE_REMOTE_LCD)
    pla_remote_ctx,
#endif
};

static void clamp_cursor(struct scummvm_backend *backend)
{
    if (backend->cursor_x < 0)
        backend->cursor_x = 0;
    else if (backend->cursor_x >= backend->video.width)
        backend->cursor_x = backend->video.width - 1;

    if (backend->cursor_y < 0)
        backend->cursor_y = 0;
    else if (backend->cursor_y >= backend->video.height)
        backend->cursor_y = backend->video.height - 1;
}

static void move_cursor(struct scummvm_backend *backend, int dx, int dy)
{
    backend->cursor_x += dx;
    backend->cursor_y += dy;
    clamp_cursor(backend);
}

static bool backend_is_queen(const struct scummvm_backend *backend)
{
    return backend->engine.initialized &&
        rb->strcmp(backend->target->engine, "queen") == 0;
}

static bool backend_is_agds(const struct scummvm_backend *backend)
{
    return backend->engine.initialized &&
        (!rb->strcasecmp(backend->target->engine, "agds") ||
         !rb->strcasecmp(backend->target->engine, "nibiru"));
}

static bool backend_cursor_in_queen_inventory(
    const struct scummvm_backend *backend)
{
    return backend->cursor_x >= 178 &&
        backend->cursor_y >= 2 &&
        backend->cursor_y < 60;
}

static void draw_cursor(const struct scummvm_backend *backend, int x, int y)
{
    (void)backend;
    rb->lcd_drawline(x - 4, y, x + 4, y);
    rb->lcd_drawline(x, y - 4, x, y + 4);
    rb->lcd_drawrect(x - 2, y - 2, 5, 5);
}

static void draw_sky_overlay(const struct scummvm_backend *backend)
{
    struct scummvm_sky_overlay overlay;
    uint16_t i;

    if (!backend->engine.initialized ||
        rb->strcasecmp(backend->target->engine, "sky") ||
        !scummvm_sky_runtime_overlay(&overlay))
        return;

    for (i = 0; i < overlay.count; i++) {
        const struct scummvm_sky_overlay_line *line = &overlay.lines[i];
        char text[SCUMMVM_SKY_OVERLAY_TEXT + 3];
        int x = backend->video.screen_x + 6;
        int y = backend->video.screen_y + line->y;
        int w = backend->video.width - 12;
        int h = 15;

        if (line->selectable)
            rb->snprintf(text, sizeof(text), "> %.48s", line->text);
        else
            rb->snprintf(text, sizeof(text), "%.50s", line->text);

        if (y < backend->video.screen_y)
            y = backend->video.screen_y;
        if (y + h > backend->video.screen_y + backend->video.height)
            y = backend->video.screen_y + backend->video.height - h;

        rb->lcd_set_drawmode(DRMODE_SOLID);
        rb->lcd_fillrect(x - 2, y - 1, w + 4, h + 2);
        rb->lcd_set_drawmode(DRMODE_SOLID | DRMODE_INVERSEVID);
        rb->lcd_drawrect(x - 2, y - 1, w + 4, h + 2);
        rb->lcd_putsxy(x, y + 3, text);
        rb->lcd_set_drawmode(DRMODE_SOLID);
    }
}

static void draw_queen_overlay(const struct scummvm_backend *backend)
{
    struct scummvm_queen_overlay overlay;
    uint16_t i;
    int x = backend->video.screen_x + 178;
    int y = backend->video.screen_y + 2;
    int dialog_x = backend->video.screen_x + 6;
    int dialog_y = backend->video.screen_y + 176;
    int dialog_w = backend->video.width - 12;

    if (!backend->engine.initialized ||
        rb->strcasecmp(backend->target->engine, "queen") ||
        !scummvm_queen_runtime_overlay(&overlay) ||
        !overlay.active)
        return;

    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_fillrect(x - 2, y - 1, 138, 57);
    rb->lcd_set_drawmode(DRMODE_SOLID | DRMODE_INVERSEVID);
    rb->lcd_drawrect(x - 2, y - 1, 138, 57);
    rb->lcd_putsxy(x, y + 2, "Inventory");
    for (i = 0; i < SCUMMVM_QUEEN_INVENTORY_SLOTS; i++) {
        char line[SCUMMVM_QUEEN_OVERLAY_TEXT + 4];

        rb->snprintf(line, sizeof(line), "%u %.58s",
                     (unsigned)(i + 1), overlay.inventory[i]);
        rb->lcd_putsxy(x, y + 13 + i * 8, line);
    }
    rb->lcd_putsxy(x, y + 47, overlay.verb);
    rb->lcd_set_drawmode(DRMODE_SOLID);

    if (overlay.message[0] != '\0' || overlay.option_count > 0) {
        rb->lcd_fillrect(dialog_x - 2, dialog_y - 1, dialog_w + 4, 58);
        rb->lcd_set_drawmode(DRMODE_SOLID | DRMODE_INVERSEVID);
        rb->lcd_drawrect(dialog_x - 2, dialog_y - 1, dialog_w + 4, 58);
        if (overlay.message[0] != '\0')
            rb->lcd_putsxy(dialog_x, dialog_y + 3, overlay.message);

        for (i = 0; i < SCUMMVM_QUEEN_DIALOG_OPTIONS; i++) {
            char line[SCUMMVM_QUEEN_OVERLAY_TEXT + 4];

            if (!overlay.option_active[i])
                continue;
            rb->snprintf(line, sizeof(line), "%u %.58s",
                         (unsigned)(i + 1), overlay.options[i]);
            rb->lcd_putsxy(dialog_x, dialog_y + 15 + i * 10, line);
        }
        rb->lcd_set_drawmode(DRMODE_SOLID);
    }
}

static uint16_t backend_read_u16le(const unsigned char *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t backend_read_u32le(const unsigned char *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static struct agds_bitmap_font *backend_agds_font(
    struct scummvm_backend *backend, int slot,
    struct scummvm_agds_font *style)
{
    struct scummvm_agds_font font;
    struct agds_bitmap_font *cache = NULL;
    char relative[MAX_PATH];
    char path[MAX_PATH];
    unsigned index;
    int fd;
    long size;

    if (slot < 0 || slot >= AGDS_VM_MAX_FONTS ||
        !scummvm_agds_runtime_font((unsigned)slot, &font))
        return NULL;
    if (style != NULL)
        *style = font;
    for (index = 0; index < AGDS_BITMAP_FONT_CACHE; index++) {
        struct agds_bitmap_font *candidate = &agds_bitmap_fonts[index];

        if (candidate->valid &&
            !rb->strcmp(candidate->descriptor, font.descriptor) &&
            candidate->authored_height == font.authored_height) {
            candidate->stamp = ++agds_bitmap_font_stamp;
            return candidate;
        }
        if (cache == NULL || !candidate->valid ||
            candidate->stamp < cache->stamp)
            cache = candidate;
    }

    rb->snprintf(relative, sizeof(relative),
                 "rockpod/fonts/%s-%u.fnt", font.descriptor,
                 (unsigned)font.authored_height);
    if (!scummvm_make_path(path, sizeof(path), backend->target->path,
                           relative)) {
        return NULL;
    }
    fd = rb->open(path, O_RDONLY);
    size = fd >= 0 ? rb->filesize(fd) : -1;
    if (fd < 0 || size < (long)AGDS_FNT_HEADER_SIZE ||
        size > AGDS_BITMAP_FONT_BYTES ||
        rb->read(fd, cache->data, size) != size) {
        if (fd >= 0)
            rb->close(fd);
        cache->valid = false;
        DEBUGF("scummvm: AGDS owned bitmap font unavailable: %s bytes=%ld\n",
               path, size);
        return NULL;
    }
    rb->close(fd);
    cache->max_width = backend_read_u16le(cache->data + 4);
    cache->height = backend_read_u16le(cache->data + 6);
    cache->ascent = backend_read_u16le(cache->data + 8);
    cache->depth = backend_read_u16le(cache->data + 10);
    cache->first_char = backend_read_u32le(cache->data + 12);
    cache->default_char = backend_read_u32le(cache->data + 16);
    cache->glyph_count = backend_read_u32le(cache->data + 20);
    cache->bits_size = backend_read_u32le(cache->data + 24);
    cache->offsets_at = (AGDS_FNT_HEADER_SIZE + cache->bits_size + 1u) & ~1u;
    cache->widths_at = cache->offsets_at + cache->glyph_count * 2u;
    cache->size = (uint32_t)size;
    if (rb->memcmp(cache->data, "RB12", 4) ||
        cache->depth != 1 ||
        cache->height == 0 || cache->max_width == 0 ||
        cache->glyph_count == 0 ||
        backend_read_u32le(cache->data + 28) != cache->glyph_count ||
        backend_read_u32le(cache->data + 32) != cache->glyph_count ||
        cache->offsets_at > cache->size ||
        cache->widths_at > cache->size ||
        cache->glyph_count > cache->size - cache->widths_at) {
        cache->valid = false;
        DEBUGF("scummvm: AGDS owned bitmap font invalid: %s\n", path);
        return NULL;
    }
    rb->strlcpy(cache->descriptor, font.descriptor,
                sizeof(cache->descriptor));
    cache->authored_height = font.authored_height;
    cache->stamp = ++agds_bitmap_font_stamp;
    cache->valid = true;
    DEBUGF("scummvm: AGDS font slot %d loaded %s %ux%u\n",
           slot, relative, (unsigned)cache->max_width,
           (unsigned)cache->height);
    return cache;
}

static uint32_t agds_text_codepoint(const char **text)
{
    const unsigned char *source = (const unsigned char *)*text;
    uint32_t code = *source++;

    if (code >= 0xc2 && code <= 0xdf &&
        (source[0] & 0xc0) == 0x80) {
        code = ((code & 0x1f) << 6) | (source[0] & 0x3f);
        source++;
    } else if (code >= 0xe0 && code <= 0xef &&
               (source[0] & 0xc0) == 0x80 &&
               (source[1] & 0xc0) == 0x80) {
        code = ((code & 0x0f) << 12) | ((source[0] & 0x3f) << 6) |
            (source[1] & 0x3f);
        source += 2;
    }
    *text = (const char *)source;
    return code;
}

static bool agds_font_glyph(
    const struct agds_bitmap_font *font, uint32_t code,
    const unsigned char **bits, unsigned *width)
{
    uint32_t glyph;
    uint32_t offset;
    uint32_t bytes;

    if (font == NULL || !font->valid)
        return false;
    if (code < font->first_char ||
        code >= font->first_char + font->glyph_count)
        code = font->default_char;
    if (code < font->first_char ||
        code >= font->first_char + font->glyph_count)
        return false;
    glyph = code - font->first_char;
    *width = font->data[font->widths_at + glyph];
    offset = backend_read_u16le(font->data + font->offsets_at + glyph * 2u);
    bytes = (*width * font->height + 1u) / 2u;
    if (offset > font->bits_size || bytes > font->bits_size - offset)
        return false;
    *bits = font->data + AGDS_FNT_HEADER_SIZE + offset;
    return true;
}

static int agds_bitmap_text_width(
    const struct agds_bitmap_font *font, const char *text)
{
    int width = 0;

    if (font == NULL) {
        rb->font_getstringsize(text, &width, NULL, FONT_SYSFIXED);
        return width;
    }
    while (*text != '\0' && *text != '\r' && *text != '\n') {
        const unsigned char *bits;
        unsigned glyph_width;
        uint32_t code = agds_text_codepoint(&text);

        if (agds_font_glyph(font, code, &bits, &glyph_width))
            width += (int)glyph_width;
    }
    return width;
}

static void agds_bitmap_putsxy(
    const struct agds_bitmap_font *font, int x, int y, const char *text)
{
    if (font == NULL) {
        rb->lcd_setfont(FONT_SYSFIXED);
        rb->lcd_putsxy(x, y, text);
        return;
    }
    while (*text != '\0' && *text != '\r' && *text != '\n') {
        const unsigned char *bits;
        unsigned width;
        uint32_t code = agds_text_codepoint(&text);

        if (!agds_font_glyph(font, code, &bits, &width))
            continue;
#if LCD_DEPTH >= 16
        {
            struct viewport *viewport =
                *(rb->screens[SCREEN_MAIN]->current_viewport);
            fb_data foreground = (fb_data)rb->lcd_get_foreground();
            unsigned fg_r = RGB_UNPACK_RED(foreground);
            unsigned fg_g = RGB_UNPACK_GREEN(foreground);
            unsigned fg_b = RGB_UNPACK_BLUE(foreground);
            int glyph_x;
            int glyph_y;

            if (viewport != NULL && viewport->buffer != NULL) {
                for (glyph_y = 0; glyph_y < font->height; glyph_y++) {
                    int destination_y = y + glyph_y;

                    if (destination_y < 0 ||
                        destination_y >= viewport->height)
                        continue;
                    for (glyph_x = 0; glyph_x < (int)width; glyph_x++) {
                        unsigned pixel =
                            (unsigned)glyph_y * width +
                            (unsigned)glyph_x;
                        unsigned packed = bits[pixel / 2u];
                        unsigned shade =
                            (packed >> (4u * (pixel & 1u))) & 0x0fu;
                        unsigned coverage = 15u - shade;
                        int destination_x = x + glyph_x;
                        fb_data *destination;
                        fb_data background;
                        unsigned bg_r;
                        unsigned bg_g;
                        unsigned bg_b;

                        if (coverage == 0 || destination_x < 0 ||
                            destination_x >= viewport->width)
                            continue;
                        destination = FBADDRBUF(
                            viewport->buffer,
                            viewport->x + destination_x,
                            viewport->y + destination_y);
                        background = *destination;
                        bg_r = RGB_UNPACK_RED(background);
                        bg_g = RGB_UNPACK_GREEN(background);
                        bg_b = RGB_UNPACK_BLUE(background);
                        *destination = LCD_RGBPACK(
                            (fg_r * coverage + bg_r * (15u - coverage) +
                             7u) / 15u,
                            (fg_g * coverage + bg_g * (15u - coverage) +
                             7u) / 15u,
                            (fg_b * coverage + bg_b * (15u - coverage) +
                             7u) / 15u);
                    }
                }
            }
        }
#else
        rb->lcd_mono_bitmap_part(bits, 0, 0, (int)width,
                                 x, y, (int)width, font->height);
#endif
        x += (int)width;
    }
}

static fb_data agds_text_color(int32_t packed, fb_data fallback)
{
    if (packed < 0)
        return fallback;
    return LCD_RGBPACK((unsigned)packed & 0xff,
                       ((unsigned)packed >> 8) & 0xff,
                       ((unsigned)packed >> 16) & 0xff);
}

static const char *agds_bitmap_wrap_line(
    const struct agds_bitmap_font *font, const char *text, int max_width,
    char *line, size_t line_size)
{
    const char *start;
    const char *scan;
    const char *end;
    const char *resume;
    const char *last_space = NULL;
    const char *after_space = NULL;
    int width = 0;
    size_t bytes;

    while (*text == ' ' || *text == '\t' || *text == '\r' ||
           *text == '\n')
        text++;
    start = text;
    scan = text;
    end = text;
    resume = text;
    while (*scan != '\0') {
        const char *next = scan;
        const unsigned char *bits;
        unsigned glyph_width = 0;
        uint32_t code;

        if (*scan == '\r' || *scan == '\n') {
            end = scan;
            resume = scan + 1;
            if (*scan == '\r' && *resume == '\n')
                resume++;
            break;
        }
        code = agds_text_codepoint(&next);
        if (agds_font_glyph(font, code, &bits, &glyph_width)) {
            (void)bits;
        } else {
            char glyph[5];
            size_t glyph_bytes = (size_t)(next - scan);

            glyph_bytes = MIN(glyph_bytes, sizeof(glyph) - 1);
            rb->memcpy(glyph, scan, glyph_bytes);
            glyph[glyph_bytes] = '\0';
            {
                int fallback_width = 0;

                rb->font_getstringsize(glyph, &fallback_width, NULL,
                                       FONT_SYSFIXED);
                glyph_width = (unsigned)MAX(0, fallback_width);
            }
        }
        if (width + (int)glyph_width > max_width && scan > start) {
            if (last_space != NULL) {
                end = last_space;
                resume = after_space;
            } else {
                end = scan;
                resume = scan;
            }
            break;
        }
        if (*scan == ' ' || *scan == '\t') {
            last_space = scan;
            after_space = next;
        }
        width += (int)glyph_width;
        end = next;
        resume = next;
        scan = next;
    }
    while (end > start && (end[-1] == ' ' || end[-1] == '\t'))
        end--;
    bytes = MIN((size_t)(end - start), line_size - 1);
    rb->memcpy(line, start, bytes);
    line[bytes] = '\0';
    return resume;
}

static void draw_agds_dialog_overlay(struct scummvm_backend *backend)
{
    struct scummvm_agds_dialog_overlay overlay;
    struct scummvm_agds_font style;
    struct agds_bitmap_font *font;
    char lines[12][128];
    const char *cursor;
    unsigned line_count = 0;
    unsigned index;
    int font_height = 8;
    int anchor_x;
    int anchor_y;
    int max_width;
    int y;

    if (!backend->engine.initialized || !backend_is_agds(backend) ||
        !scummvm_agds_runtime_dialog_overlay(&overlay))
        return;
    rb->memset(&style, 0, sizeof(style));
    style.primary_color = -1;
    style.secondary_color = -1;
    font = backend_agds_font(backend, overlay.font_slot, &style);
    /* Reflow for the physical LCD, not the authored 1024x768 text box. */
    anchor_x = backend->video.screen_x + backend->video.width / 2;
    anchor_y = backend->video.screen_y + backend->video.height - 5;
    max_width = backend->video.width - 12;
    cursor = overlay.text;
    while (*cursor != '\0' && line_count < ARRAYLEN(lines)) {
        const char *next = agds_bitmap_wrap_line(
            font, cursor, max_width, lines[line_count],
            sizeof(lines[line_count]));

        if (lines[line_count][0] == '\0' || next <= cursor)
            break;
        line_count++;
        cursor = next;
    }
    if (line_count == 0)
        return;
    if (font != NULL)
        font_height = font->height;
    else
        rb->font_getstringsize("Ag", NULL, &font_height, FONT_SYSFIXED);
    y = anchor_y - (int)line_count * (font_height + 2);
    y = MAX(backend->video.screen_y + 3, y);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_fillrect(backend->video.screen_x, y - 3,
                     backend->video.width, anchor_y - y + 6);
    rb->lcd_set_drawmode(DRMODE_FG);
    for (index = 0; index < line_count; index++) {
        int width = 0;
        int x;

        width = agds_bitmap_text_width(font, lines[index]);
        x = anchor_x - width / 2;
        x = MAX(backend->video.screen_x, MIN(
            x, backend->video.screen_x + backend->video.width - width));
        rb->lcd_set_foreground(overlay.npc ?
            LCD_RGBPACK(255, 238, 190) : LCD_WHITE);
        agds_bitmap_putsxy(font, x, y, lines[index]);
        y += font_height + 2;
    }
    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_set_drawmode(DRMODE_SOLID);
}

static void draw_agds_object_texts(struct scummvm_backend *backend)
{
    struct scummvm_agds_object_text overlay;
    unsigned count;
    unsigned index;

    if (!backend->engine.initialized || !backend_is_agds(backend))
        return;
    count = scummvm_agds_runtime_object_text_count();
    if (count == 0)
        return;
    rb->lcd_set_drawmode(DRMODE_FG);
    for (index = 0; index < count; index++) {
        char *line;
        struct scummvm_agds_font style;
        struct agds_bitmap_font *font;
        int font_height = 8;
        int x;
        int y;

        if (!scummvm_agds_runtime_object_text(index, &overlay))
            continue;
        rb->memset(&style, 0, sizeof(style));
        style.primary_color = -1;
        font = backend_agds_font(backend, overlay.font_slot, &style);
        if (font != NULL)
            font_height = font->height;
        else
            rb->font_getstringsize("Ag", NULL, &font_height,
                                   FONT_SYSFIXED);
        x = backend->video.screen_x +
            (int)((int32_t)overlay.x * backend->video.width / 1024);
        y = backend->video.screen_y +
            (int)((int32_t)overlay.y * backend->video.height / 768);
        rb->lcd_set_foreground(agds_text_color(
            overlay.color, agds_text_color(style.primary_color, LCD_WHITE)));
        line = overlay.text;
        while (*line != '\0' && y < backend->video.screen_y +
                                      backend->video.height) {
            char *end = line;
            char saved;
            int width = 0;
            int draw_x = x;

            while (*end != '\0' && *end != '\r' && *end != '\n')
                end++;
            saved = *end;
            *end = '\0';
            width = agds_bitmap_text_width(font, line);
            /* Retail text modes 2 and 3 select anchored placement.  NiBiRu's
             * credits use mode 3 (flag bit 1), centered on their authored x;
             * mode 2 (bit 0) is right anchored. */
            if ((overlay.flags & 2u) != 0)
                draw_x -= width / 2;
            else if ((overlay.flags & 1u) != 0)
                draw_x -= width;
            if (draw_x < backend->video.screen_x)
                draw_x = backend->video.screen_x;
            if (draw_x < backend->video.screen_x + backend->video.width &&
                y + font_height > backend->video.screen_y)
                agds_bitmap_putsxy(font, draw_x, y, line);
            *end = saved;
            if (saved == '\0')
                break;
            line = end + 1;
            if (saved == '\r' && *line == '\n')
                line++;
            y += font_height;
        }
    }
    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_set_drawmode(DRMODE_SOLID);
}

/* The main menu labels are baked into its 1024x768 bitmap.  Replace only
 * those tiny captions at their existing hit regions with crisp native text;
 * the retail objects, hover handlers and click dispatch remain authoritative. */
static void draw_agds_menu_labels(struct scummvm_backend *backend)
{
    static const struct {
        const char *text;
        int y;
    } labels[] = {
        { "New game", 89 }, { "Load game", 101 },
        { "Save game", 113 }, { "Options", 125 },
        { "Credits", 137 }, { "Quit", 159 }
    };
    unsigned index;

    if (!backend_is_agds(backend) || !scummvm_agds_runtime_main_menu())
        return;
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    for (index = 0; index < ARRAYLEN(labels); index++) {
        int x = backend->video.screen_x + 26;
        int y = backend->video.screen_y + labels[index].y;
        bool hover = backend->cursor_x >= 24 && backend->cursor_x < 88 &&
                     backend->cursor_y >= labels[index].y - 1 &&
                     backend->cursor_y < labels[index].y + 10;

        rb->lcd_set_foreground(LCD_RGBPACK(12, 19, 14));
        rb->lcd_fillrect(x - 2, y - 1, 65, 11);
        rb->lcd_set_foreground(hover ? LCD_WHITE :
                              LCD_RGBPACK(238, 220, 165));
        rb->lcd_putsxy(x, y, labels[index].text);
    }
    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_foreground(LCD_BLACK);
}

static void draw_backend_status(const struct scummvm_backend *backend)
{
    rb->lcd_putsf(0, 0, "ScummVM: %s", backend->target->gameid);
    rb->lcd_putsf(0, 1, "Engine: %s %s",
                  backend->probe.engine_name,
                  backend->probe.supported ? "" : "(blocked)");
    rb->lcd_putsf(0, 2, "Mouse: %03d,%03d %s",
                  backend->cursor_x, backend->cursor_y,
                  backend->mouse_down ? "down" : "up");
    rb->lcd_putsf(0, 3, "Data: %s",
                  backend->probe.data_found ? backend->probe.variant :
                  backend->probe.detail);
    rb->lcd_putsf(0, 4, "Engine data: %s",
                  backend->probe.engine_data_found ? "ready" :
                  backend->probe.engine_data_detail);
    rb->lcd_putsf(0, 5, "Run: %s", backend->engine.status);
}

static void draw_menu(const struct scummvm_backend *backend)
{
    int x = 14;
    int y = 54;
    int w = LCD_WIDTH - 28;
    int h = 96;

    (void)backend;
    rb->lcd_fillrect(x, y, w, h);
    rb->lcd_set_drawmode(DRMODE_SOLID | DRMODE_INVERSEVID);
    rb->lcd_drawrect(x, y, w, h);
    rb->lcd_putsxy(x + 8, y + 8, "ScummVM backend shell");
    rb->lcd_putsxy(x + 8, y + 28, "Select: click");
    rb->lcd_putsxy(x + 8, y + 44, "Arrows/wheel: move");
    rb->lcd_putsxy(x + 8, y + 60, "Menu: close");
    rb->lcd_putsxy(x + 8, y + 76, "Long select: exit");
    rb->lcd_set_drawmode(DRMODE_SOLID);
}

static void draw_surface_frame(struct scummvm_backend *backend)
{
    int cursor_screen_x = backend->video.screen_x + backend->cursor_x;
    int cursor_screen_y = backend->video.screen_y + backend->cursor_y;

    scummvm_video_present(&backend->video);
    if (!backend->engine.initialized) {
        rb->lcd_drawrect(backend->video.screen_x, backend->video.screen_y,
                         backend->video.width, backend->video.height);
        rb->lcd_putsxy(backend->video.screen_x + 4,
                       backend->video.screen_y + 2,
                       "engine framebuffer");
        rb->lcd_putsxy(backend->video.screen_x + 4,
                       backend->video.screen_y + 18,
                       backend->probe.detail);
        rb->lcd_putsxy(backend->video.screen_x + 4,
                       backend->video.screen_y + 34,
                       backend->engine.status);
    }

    draw_sky_overlay(backend);
    draw_queen_overlay(backend);
    draw_agds_object_texts(backend);
    draw_agds_dialog_overlay(backend);
    draw_agds_menu_labels(backend);
    if (!backend_is_agds(backend) ||
        scummvm_agds_runtime_pointer_visible())
        draw_cursor(backend, cursor_screen_x, cursor_screen_y);
}

static void draw_backend(struct scummvm_backend *backend)
{
    rb->lcd_clear_display();
    if (!backend->engine.initialized)
        draw_backend_status(backend);
    draw_surface_frame(backend);

    if (backend->menu_open)
        draw_menu(backend);

    rb->lcd_update();
}

static void backend_status_screen(const char *line1, const char *line2)
{
    rb->lcd_clear_display();
    rb->lcd_putsxy(4, 12, "ScummVM backend");
    rb->lcd_putsxy(4, 34, line1 ? line1 : "");
    if (line2)
        rb->lcd_putsxy(4, 52, line2);
    rb->lcd_update();
}

static bool handle_action(struct scummvm_backend *backend, int action)
{
    int step = SCUMMVM_CURSOR_STEP;

    switch (action) {
    case PLA_LEFT_REPEAT:
    case PLA_RIGHT_REPEAT:
    case PLA_UP_REPEAT:
    case PLA_DOWN_REPEAT:
        step = SCUMMVM_CURSOR_FAST_STEP;
        break;
    default:
        break;
    }

    switch (action) {
    case PLA_LEFT:
    case PLA_LEFT_REPEAT:
        move_cursor(backend, -step, 0);
        break;
    case PLA_RIGHT:
    case PLA_RIGHT_REPEAT:
        move_cursor(backend, step, 0);
        break;
    case PLA_UP:
    case PLA_UP_REPEAT:
#ifdef HAVE_WHEEL_POSITION
        if (backend_is_agds(backend) && backend->wheel_available) {
            if (action == PLA_UP)
                backend->escape_pending = true;
            break;
        }
#endif
        move_cursor(backend, 0, -step);
        break;
    case PLA_DOWN:
    case PLA_DOWN_REPEAT:
#ifdef HAVE_WHEEL_POSITION
        if (backend_is_agds(backend) && backend->wheel_available) {
            if (action == PLA_DOWN)
                backend->look_pending = true;
            break;
        }
#endif
        move_cursor(backend, 0, step);
        break;
#ifdef HAVE_SCROLLWHEEL
    case PLA_SCROLL_BACK:
    case PLA_SCROLL_BACK_REPEAT:
        if (backend_is_queen(backend)) {
            if (backend_cursor_in_queen_inventory(backend))
                scummvm_queen_runtime_cycle_inventory(
                    -1, backend->engine.status,
                    sizeof(backend->engine.status));
            else
                scummvm_queen_runtime_cycle_verb(
                    -1, backend->engine.status,
                    sizeof(backend->engine.status));
            break;
        }
#ifdef HAVE_WHEEL_POSITION
        if (backend_is_agds(backend) && backend->wheel_available)
            break;
#endif
        if (backend_is_agds(backend))
            move_cursor(backend, 0, -step);
        else
            move_cursor(backend, -step, 0);
        break;
    case PLA_SCROLL_FWD:
    case PLA_SCROLL_FWD_REPEAT:
        if (backend_is_queen(backend)) {
            if (backend_cursor_in_queen_inventory(backend))
                scummvm_queen_runtime_cycle_inventory(
                    1, backend->engine.status,
                    sizeof(backend->engine.status));
            else
                scummvm_queen_runtime_cycle_verb(
                    1, backend->engine.status,
                    sizeof(backend->engine.status));
            break;
        }
#ifdef HAVE_WHEEL_POSITION
        if (backend_is_agds(backend) && backend->wheel_available)
            break;
#endif
        if (backend_is_agds(backend))
            move_cursor(backend, 0, step);
        else
            move_cursor(backend, step, 0);
        break;
#endif
    case PLA_SELECT:
        backend->mouse_down = true;
        break;
    case PLA_SELECT_REL:
        if (backend->mouse_down) {
            backend->click_pending = true;
            /* Select is a mouse click.  Sending SPACE as well can advance
             * dialogue a second time or skip the scene being clicked. */
        }
        backend->mouse_down = false;
        break;
    case PLA_SELECT_REPEAT:
        if (backend_is_agds(backend)) {
            backend->mouse_down = false;
            backend->tab_pending = true;
            break;
        }
        return false;
    case PLA_CANCEL:
        if (backend_is_queen(backend)) {
            if (backend_cursor_in_queen_inventory(backend))
                scummvm_queen_runtime_cycle_inventory(
                    1, backend->engine.status,
                    sizeof(backend->engine.status));
            else
                scummvm_queen_runtime_cycle_verb(
                    1, backend->engine.status,
                    sizeof(backend->engine.status));
            break;
        }
        if (backend_is_agds(backend))
            backend->escape_pending = true;
        else
            backend->menu_open = !backend->menu_open;
        break;
    case PLA_EXIT:
        if (backend_is_agds(backend)) {
            backend->mouse_down = false;
            backend->look_pending = true;
            break;
        }
        return false;
    default:
        break;
    }

    return true;
}

enum plugin_status scummvm_backend_run(const struct scummvm_target *target)
{
    struct scummvm_backend backend;
    bool running = true;

    rb->memset(&backend, 0, sizeof(backend));
    rb->memset(agds_bitmap_fonts, 0, sizeof(agds_bitmap_fonts));
    agds_bitmap_font_stamp = 0;
    backend.target = target;
    backend.cursor_x = SCUMMVM_SURFACE_W / 2;
    backend.cursor_y = SCUMMVM_SURFACE_H / 2;
    backend.menu_open = false;
    backend_status_screen("probe game", target->path);
    scummvm_probe_game(target, &backend.probe);
    DEBUGF("scummvm: probe supported=%d data=%d detail=%s\n",
           backend.probe.supported, backend.probe.data_found,
           backend.probe.detail);
    backend_status_screen("prepare engine", backend.probe.detail);
    scummvm_engine_prepare(target, &backend.probe, &backend.engine);
    DEBUGF("scummvm: engine initialized=%d status=%s\n",
           backend.engine.initialized, backend.engine.status);
    backend_status_screen("init video", backend.engine.status);
    scummvm_video_init(&backend.video);
    if (backend_is_agds(&backend)) {
        if (!scummvm_agds_runtime_init(target, &backend.video,
                                       backend.engine.status,
                                       sizeof(backend.engine.status)))
            backend.engine.initialized = false;
        else {
#ifdef SIMULATOR
            const char *cursor_x = getenv("ROCKPOD_SCUMMVM_CURSOR_X");
            const char *cursor_y = getenv("ROCKPOD_SCUMMVM_CURSOR_Y");

            backend.cursor_x = cursor_x != NULL ? atoi(cursor_x) :
                                                  backend.video.width / 2;
            backend.cursor_y = cursor_y != NULL ? atoi(cursor_y) :
                                                  backend.video.height / 2;
            clamp_cursor(&backend);
#else
            backend.cursor_x = backend.video.width / 2;
            backend.cursor_y = backend.video.height / 2;
#endif
        }
    } else {
        scummvm_video_demo_pattern(&backend.video);
    }
    backend_status_screen("runtime check", backend.engine.status);
    if (backend.engine.initialized &&
        rb->strcmp(target->engine, "queen") == 0 &&
        !scummvm_upstream_can_run(target))
        scummvm_queen_runtime_init(target, &backend.video,
                                   backend.engine.status,
                                   sizeof(backend.engine.status));

    backend_status_screen("first frame", backend.engine.status);
#ifdef HAVE_WHEEL_POSITION
    if (backend_is_agds(&backend)) {
        agds_pointer_load_settings();
        agds_pointer_reset(&backend);
        rb->wheel_send_events(false);
    }
#endif
    rb->button_clear_queue();

    while (running) {
        int action;
        long frame_tick = *rb->current_tick;
        int timeout;

#ifdef HAVE_WHEEL_POSITION
        if (backend_is_agds(&backend))
            agds_pointer_poll(&backend);
#endif
        draw_backend(&backend);
        timeout = MAX(0, MAX(1, HZ / 50) -
                         (*rb->current_tick - frame_tick));
        action = pluginlib_getaction(timeout, scummvm_contexts,
                                     ARRAYLEN(scummvm_contexts));
        running = handle_action(&backend, action);
        if (backend.engine.initialized) {
            if (rb->strcmp(target->engine, "sky") == 0)
                scummvm_sky_runtime_input(backend.cursor_x,
                                          backend.cursor_y,
                                          backend.mouse_down,
                                          backend.click_pending);
            else if (rb->strcmp(target->engine, "queen") == 0 &&
                     scummvm_upstream_can_run(target)) {
                scummvm_upstream_input(backend.cursor_x, backend.cursor_y,
                                       backend.mouse_down,
                                       backend.click_pending);
            } else if (rb->strcmp(target->engine, "queen") == 0) {
                scummvm_queen_runtime_input(backend.cursor_x,
                                            backend.cursor_y,
                                            backend.click_pending,
                                            backend.engine.status,
                                            sizeof(backend.engine.status));
            } else if (backend_is_agds(&backend)) {
                if (backend.space_pending)
                    scummvm_agds_runtime_key("SPACE");
                if (backend.tab_pending)
                    scummvm_agds_runtime_key("TAB");
                if (backend.escape_pending)
                    scummvm_agds_runtime_key("escape");
                backend.space_pending = false;
                backend.tab_pending = false;
                backend.escape_pending = false;
                scummvm_agds_runtime_input(backend.cursor_x,
                                           backend.cursor_y,
                                           backend.click_pending,
                                           backend.look_pending);
            }
            backend.click_pending = false;
            backend.look_pending = false;
            if (backend_is_agds(&backend)) {
                if (!scummvm_agds_runtime_frame(backend.engine.status,
                                                sizeof(backend.engine.status)))
                    running = false;
            } else if (rb->strcmp(target->engine, "queen") != 0 ||
                scummvm_upstream_can_run(target)) {
                if (!scummvm_upstream_frame(target, &backend.engine,
                                            &backend.video))
                    running = false;
            }
        }
    }

    rb->lcd_clear_display();
    rb->lcd_update();
    if (backend.engine.initialized &&
        rb->strcmp(target->engine, "queen") == 0 &&
        !scummvm_upstream_can_run(target))
        scummvm_queen_runtime_save(backend.engine.status,
                                   sizeof(backend.engine.status));
#ifdef HAVE_WHEEL_POSITION
    if (backend_is_agds(&backend))
        rb->wheel_send_events(true);
#endif
    scummvm_sky_runtime_reset();
    scummvm_agds_runtime_reset();
    scummvm_sky_cpt_unload();
    scummvm_upstream_shutdown();

    return PLUGIN_OK;
}
