/***************************************************************************
 * Private RetailOS Settings/About and dialog presentation. No action,
 * playback, playlist or allocation ownership lives in this module.
 ***************************************************************************/
#include "config.h"
#include "ipodjs_settings.h"
#include "ipodjs_ui.h"
#include "ipodjs_retailos.h"
#include "audio.h"
#include "button.h"
#include "font.h"
#include "file.h"
#include "misc.h"
#include "string.h"
#include "string-extra.h"
#include "stdio.h"
#include "stdlib.h"
#include <limits.h>
#include "storage.h"
#include "disk.h"
#include "version.h"
#include "tagcache.h"
#include "lang.h"
#include "viewport.h"

#define RGA(w,h) IPODJS_RETAILOS_RGA_BYTES(w,h)
static struct {
    bool tried, about_valid, dialog_valid;
    struct ipodjs_retailos_image band, logo, dots[2], legends[4];
    struct ipodjs_retailos_image capacity[5][3], edge, overlay[3], button[2][3];
    unsigned char band_pixels[RGA(320,39)], logo_pixels[RGA(64,72)];
    unsigned char dot_pixels[2][RGA(6,6)], legend_pixels[4][RGA(12,12)];
    unsigned char capacity_pixels[5][3][RGA(16,30)];
    unsigned char edge_pixels[RGA(2,30)];
    unsigned char overlay_pixels[3][RGA(9,80)];
    unsigned char button_pixels[2][3][RGA(6,26)];
} retail;

/* About owns these only while its action loop is active. No scroll thread,
 * retained stack viewport, allocation or background file read is involved. */
static struct {
    int count;
    bool collecting;
    long started;
    struct {
        int x, y, width, height, font, extent, offset;
        char value[128];
    } cells[16];
} about_scroll;

#define ABOUT_MAX_APPS 256
static struct { char name[64], space[24]; } about_apps[ABOUT_MAX_APPS];

static void clipped_text(struct screen *d, int x, int y, int width,
    int font, int offset, const char *value)
{
    struct viewport vp;
    viewport_set_defaults(&vp, d->screen_type);
    vp.x = x; vp.y = y; vp.width = width;
    vp.height = font_get(font)->height; vp.font = font;
    struct viewport *saved = d->set_viewport(&vp);
    d->set_drawmode(DRMODE_FG);
    d->set_foreground(LCD_RGBPACK(255,255,255));
    d->putsxy(-offset, 0, value);
    d->set_viewport(saved);
}

void ipodjs_settings_prepare(void)
{
    if (retail.tried || audio_status())
        return;
    retail.tried = true;
    bool valid = ipodjs_retailos_load_resource_rga(393, retail.band_pixels,
        sizeof(retail.band_pixels), 320, 39, &retail.band);
    valid &= ipodjs_retailos_load_resource_rga(425, retail.logo_pixels,
        sizeof(retail.logo_pixels), 64, 72, &retail.logo);
    for (int i = 0; i < 2; i++)
        valid &= ipodjs_retailos_load_resource_rga(423 + i,
            retail.dot_pixels[i], sizeof(retail.dot_pixels[i]),
            6, 6, &retail.dots[i]);
    for (int i = 0; i < 4; i++)
        valid &= ipodjs_retailos_load_resource_rga(397 + i,
            retail.legend_pixels[i], sizeof(retail.legend_pixels[i]),
            12, 12, &retail.legends[i]);
    for (int style = 0; style < 5; style++)
        for (int part = 0; part < 3; part++)
            valid &= ipodjs_retailos_load_resource_rga(408 + style * 3 + part,
                retail.capacity_pixels[style][part],
                sizeof(retail.capacity_pixels[style][part]),
                part == 1 ? 1 : 16, 30, &retail.capacity[style][part]);
    valid &= ipodjs_retailos_load_resource_rga(407, retail.edge_pixels,
        sizeof(retail.edge_pixels), 2, 30, &retail.edge);
    retail.about_valid = valid;
    valid = true;
    static const unsigned overlay_ids[3] = {83, 85, 84};
    for (int part = 0; part < 3; part++)
        valid &= ipodjs_retailos_load_resource_rga(overlay_ids[part],
            retail.overlay_pixels[part], sizeof(retail.overlay_pixels[part]),
            part == 1 ? 9 : 8, 80, &retail.overlay[part]);
    for (int state = 0; state < 2; state++)
        for (int part = 0; part < 3; part++)
        {
            /* Source order is left, right, fill; compositor order L,F,R. */
            unsigned id = 89 + state * 3 + (part == 1 ? 2 : part == 2 ? 1 : 0);
            valid &= ipodjs_retailos_load_resource_rga(id,
                retail.button_pixels[state][part],
                sizeof(retail.button_pixels[state][part]),
                part == 1 ? 1 : state ? 6 : 5, state ? 26 : 22,
                &retail.button[state][part]);
        }
    retail.dialog_valid = valid;
}

/* Preserve native endcaps even for spans narrower than both caps together. */
static void span(struct screen *d, const struct ipodjs_retailos_image p[3],
    int x, int y, int width)
{
    if (width <= 0)
        return;
    int left = MIN((int)p[0].width, (width + 1) / 2);
    int right = MIN((int)p[2].width, width - left);
    ipodjs_retailos_blit_part(d, &p[0], 0, 0, x, y, left, p[0].height);
    for (int col = left; col < width - right; col += p[1].width)
        ipodjs_retailos_blit_part(d, &p[1], 0, 0, x + col, y,
            MIN((int)p[1].width, width - right - col), p[1].height);
    if (right)
        ipodjs_retailos_blit_part(d, &p[2], p[2].width - right, 0,
            x + width - right, y, right, p[2].height);
}

static void text(struct screen *d, int x, int y, int width,
    const char *value, bool bold, bool center)
{
    int font = bold ? ipodjs_ui_retailos_font(false) :
        ipodjs_ui_retailos_detail_font();
    d->setfont(font);
    d->set_foreground(LCD_RGBPACK(255,255,255));
    int pixels;
    font_getstringsize(value, &pixels, NULL, font);
    if (pixels > width && about_scroll.collecting)
    {
        if (about_scroll.count < (int)ARRAYLEN(about_scroll.cells))
        {
            int i = about_scroll.count++;
            about_scroll.cells[i].x = x; about_scroll.cells[i].y = y;
            about_scroll.cells[i].width = width;
            about_scroll.cells[i].height = font_get(font)->height;
            about_scroll.cells[i].font = font;
            about_scroll.cells[i].extent = pixels - width;
            about_scroll.cells[i].offset = 0;
            strmemccpy(about_scroll.cells[i].value, value,
                sizeof(about_scroll.cells[i].value));
        }
        clipped_text(d, x, y, width, font, 0, value);
        return;
    }
    ipodjs_ui_puts_fit(d, x, y, width, value, center);
}

static void space_label(char *out, size_t size, unsigned long long kib)
{
    const char *unit = kib >= 1048576 ? "GB" : kib >= 1024 ? "MB" : "KB";
    unsigned long divisor = kib >= 1048576 ? 1048576 : kib >= 1024 ? 1024 : 1;
    unsigned long tenths = (kib * 10 + divisor / 2) / divisor;
    snprintf(out, size, "%lu.%lu %s", tenths / 10, tenths % 10, unit);
}

static bool decimal_value(const char *p, unsigned long *value)
{
    unsigned long number = 0;
    if (!*p) return false;
    for (; *p; p++)
    {
        if (*p < '0' || *p > '9' || number > (ULONG_MAX - (*p - '0')) / 10)
            return false;
        number = number * 10 + (*p - '0');
    }
    *value = number;
    return true;
}

static bool inventory_value(const char *line, const char *key,
    unsigned limit, unsigned *index, unsigned long *value)
{
    size_t length = strlen(key);
    if (strncmp(line, key, length) || line[length] < '0' ||
        line[length] >= '0' + (int)limit || line[length + 1] != '\t')
        return false;
    *index = line[length] - '0';
    return decimal_value(line + length + 2, value);
}

void ipodjs_about_snapshot(struct ipodjs_about_data *data)
{
    sector_t size = 0, free = 0;
    memset(data, 0, sizeof(*data));
    strmemccpy(data->serial, "Unavailable", sizeof(data->serial));
    strmemccpy(data->model, MODEL_NAME, sizeof(data->model));
    strmemccpy(data->version, rbversion, sizeof(data->version));
    for (unsigned i = 0; i < ARRAYLEN(data->count); i++)
        strmemccpy(data->count[i], "--", sizeof(data->count[i]));
    for (unsigned i = 0; i < ARRAYLEN(data->space); i++)
        strmemccpy(data->space[i], "--", sizeof(data->space[i]));
    volume_size(IF_MV(0,) &size, &free);
    free = MIN(free, size);
    space_label(data->used, sizeof(data->used), size - free);
    space_label(data->free, sizeof(data->free), free);
    /* Host-maintained inventory: bounded 272-line read, no directory walk,
     * tagtree traversal or metadata decode while displaying About. */
    char line[128];
    unsigned long kib[4] = {0};
    unsigned found = 0;
    unsigned long long app_sum = 0;
    bool valid_apps = true;
#ifdef SIMULATOR
    unsigned long snapshot_disk[2] = {0};
    bool snapshot_volume = false;
#endif
    int fd = open(ROCKBOX_DIR "/ipodjs/about.tsv", O_RDONLY);
    if (fd >= 0)
    {
        if (read_line(fd, line, sizeof(line)) > 0 &&
            !strcmp(line, "ipodjs-about-v2"))
            for (int row = 0; row < ABOUT_MAX_APPS + 16 &&
                read_line(fd, line, sizeof(line)) > 0; row++)
            {
                unsigned index; unsigned long value;
                if (inventory_value(line, "count", 7, &index, &value))
                    snprintf(data->count[index], sizeof(data->count[index]),
                        "%lu", value);
                if (inventory_value(line, "kib", 2, &index, &value))
                {
                    kib[index] = value;
                    found |= 1u << index;
                }
                if (!strncmp(line, "app\t", 4))
                {
                    char *name = line + 4;
                    char *separator = strchr(name, '\t');
                    if (!separator || separator == name)
                        valid_apps = false;
                    else
                    {
                        *separator++ = '\0';
                        if (data->app_count >= ABOUT_MAX_APPS || strlen(name) >= 64 ||
                            !strcmp(name, "OnlyFans") || !decimal_value(separator, &value))
                            valid_apps = false;
                        else
                        {
                            unsigned i = data->app_count++;
                            strmemccpy(about_apps[i].name, name, sizeof(about_apps[i].name));
                            space_label(about_apps[i].space, sizeof(about_apps[i].space), value);
                            app_sum += value;
                        }
                    }
                }
#ifdef SIMULATOR
                if (inventory_value(line, "disk", 2, &index, &value))
                {
                    snapshot_disk[index] = value;
                    snapshot_volume = true;
                }
#endif
            }
        close(fd);
    }
#ifdef SIMULATOR
    /* Never present the developer's host disk as the player's capacity. */
    size = snapshot_disk[0];
    free = MIN(snapshot_disk[1], size);
    space_label(data->used, sizeof(data->used), size - free);
    space_label(data->free, sizeof(data->free), free);
    if (!size)
    {
        strmemccpy(data->used, "--", sizeof(data->used));
        strmemccpy(data->free, "--", sizeof(data->free));
    }
#endif
    unsigned long long sum = (unsigned long long)kib[0] + kib[1];
    data->inventory = found == 3 && size > 0 && sum <= size - free &&
        valid_apps && app_sum == kib[1];
    if (!valid_apps || app_sum != kib[1]) data->app_count = 0;
    if (data->inventory)
    {
        /* Other includes OnlyFans, shared system data and filesystem overhead. */
        kib[2] = size - free - sum;
        for (int i = 0; i < 3; i++)
        {
            data->percent[i] = (unsigned long long)kib[i] * 10000 / size;
            space_label(data->space[i], sizeof(data->space[i]), kib[i]);
        }
    }
#ifdef HAVE_TAGCACHE
    struct tagcache_stat *stat = tagcache_get_stat();
    if (stat->ready && stat->total_entries >= 0
#ifdef SIMULATOR
        && !snapshot_volume
#endif
        )
        snprintf(data->count[0], sizeof(data->count[0]), "%d", stat->total_entries);
#endif
    fd = open("/iPod_Control/Device/SysInfo", O_RDONLY);
    if (fd >= 0)
    {
        for (int row = 0; row < 32 && read_line(fd, line, sizeof(line)) > 0; row++)
        {
            char *value = strchr(line, ':');
            if (!value) continue;
            *value++ = '\0';
            while (*value == ' ') value++;
            if (!strcmp(line, "pszSerialNumber") || !strcmp(line, "SerialNumber"))
                strmemccpy(data->serial, value, sizeof(data->serial));
            if (!strcmp(line, "ModelNumStr"))
                strmemccpy(data->model, value, sizeof(data->model));
        }
        close(fd);
    }
}

void ipodjs_about_draw(struct screen *d, const struct ipodjs_about_data *data,
    int page, int app_top)
{
    about_scroll.count = 0;
    about_scroll.collecting = true;
    about_scroll.started = current_tick;
    d->set_viewport(NULL);
    d->set_drawmode(DRMODE_SOLID);
    d->set_background(LCD_RGBPACK(255,255,255));
    d->clear_display();
    bool background = ipodjs_ui_draw_retailos_background(d);
    if (!background)
    {
        d->set_background(LCD_RGBPACK(0,0,0));
        d->clear_display();
    }
    if (retail.about_valid && background)
    {
        /* SettingsMenu_AboutTop_Template: native band and logo, no resize. */
        ipodjs_retailos_blit(d, &retail.band, 0, 61);
        ipodjs_retailos_blit(d, &retail.logo, 128, 40);
    }
    text(d, 8, 64, 112, data->used, true, true);
    text(d, 200, 64, 112, data->free, true, true);
    text(d, 8, 81, 112, "Used", true, true);
    text(d, 200, 81, 112, "Free", true, true);
    if (page == 0)
    {
        static const char *labels[] = {"Music", "Apps", "Other"};
        static const int styles[] = {1, 2, 4};
        if (retail.about_valid && data->inventory)
        {
            unsigned cumulative = 0;
            int ends[3];
            for (int i = 0; i < 3; i++)
            {
                cumulative += data->percent[i];
                ends[i] = 304 * cumulative / 10000;
            }
            /* Only the outside of the bar is rounded. Interior category
             * boundaries use Apple's two-pixel edge, not nested endcaps. */
            for (int col = 0; col < 304; col++)
            {
                int style = 0;
                for (int i = 0; i < 3; i++)
                    if (col < ends[i]) { style = styles[i]; break; }
                int part = col < 16 ? 0 : col >= 288 ? 2 : 1;
                int source_x = part == 0 ? col : part == 2 ? col - 288 : 0;
                ipodjs_retailos_blit_part(d, &retail.capacity[style][part],
                    source_x, 0, 8 + col, 135, 1, 30);
            }
            for (int i = 0; i < 3; i++)
                if (ends[i] > 16 && ends[i] < 288 &&
                    (i == 0 || ends[i] != ends[i - 1]))
                    ipodjs_retailos_blit(d, &retail.edge, 8 + ends[i] - 1, 135);
        }
        else
            text(d, 8, 143, 304, "Capacity breakdown unavailable", false, true);
        for (int i = 0; i < 3; i++)
        {
            /* User-requested Rockbox categories, with native Apple glyphs
             * and font sizes. Three equal columns replace four media types. */
            int x = 14 + i * 106;
            int label_x = x + 16;
            if (retail.about_valid)
                ipodjs_retailos_blit(d, &retail.legends[i == 2 ? 3 : i], x, 176);
            text(d, label_x, 176, 78, labels[i], true, false);
            text(d, label_x, 194, 78, data->space[i], false, false);
        }
    }
    else if (page == 1)
    {
        /* App-owned binaries, assets and content; wheel pages through all
         * installed app rows. Source AboutCount geometry and 15pt faces. */
        for (int i = 0; i < 4 && app_top + i < (int)data->app_count; i++)
        {
            int y = 135 + i * 18;
            text(d, 18, y, 166,
                about_apps[app_top + i].name, true, false);
            text(d, 192, y, 114,
                about_apps[app_top + i].space, false, false);
        }
        if (!data->app_count)
            text(d, 8, 153, 304, "App storage unavailable", false, true);
        else if (data->app_count > 4)
            ipodjs_ui_draw_retailos_scrollbar(d, 320, 130, 80,
                app_top, 4, data->app_count);
    }
    else if (page == 2)
    {
        static const char *labels[] = {"Songs", "Videos", "Podcasts", "iTunes U",
                                       "Photos", "Games", "Contacts"};
        for (int i = 0; i < 7; i++)
        {
            int y = 135 + (i < 4 ? i : i - 4) * 18;
            text(d, i < 4 ? 19 : 174, y, 90, labels[i], true, false);
            text(d, i < 4 ? 110 : 277, y, 40, data->count[i], false, false);
        }
    }
    else
    {
        static const char *labels[] = {"Serial Number", "Model", "Version"};
        const char *values[] = {data->serial, data->model, data->version};
        for (int i = 0; i < 3; i++)
        {
            text(d, 18, 153 + i * 18, 142, labels[i], true, false);
            text(d, 192, 153 + i * 18, 128, values[i], false, false);
        }
    }
    for (int i = 0; retail.about_valid && i < 4; i++)
        ipodjs_retailos_blit(d, &retail.dots[i == page ? 0 : 1],
            137 + i * 13, 220);
    about_scroll.collecting = false;
}

void ipodjs_about_scroll(struct screen *d)
{
    if (button_hold() || !button_queue_empty()) return;
    long elapsed = current_tick - about_scroll.started;
    if (elapsed < HZ) return;
    for (int i = 0; i < about_scroll.count; i++)
    {
        int extent = about_scroll.cells[i].extent;
        /* 24px/s, a one-second pause at both ends; never change font size
         * or replace part of a capacity, serial or version with dots. */
        long cycle = ((2 * extent + 48) * HZ + 23) / 24;
        int phase = ((elapsed - HZ) % cycle) * 24 / HZ;
        int offset = phase <= extent ? phase : phase <= extent + 24 ? extent :
            phase <= 2 * extent + 24 ? 2 * extent + 24 - phase : 0;
        if (offset == about_scroll.cells[i].offset) continue;
        about_scroll.cells[i].offset = offset;
        int x = about_scroll.cells[i].x, y = about_scroll.cells[i].y;
        int width = about_scroll.cells[i].width, height = about_scroll.cells[i].height;
        if (!ipodjs_ui_draw_retailos_background_rect(d, x, y, width, height))
        {
            d->set_viewport(NULL);
            d->set_foreground(LCD_RGBPACK(0,0,0));
            d->fillrect(x, y, width, height);
        }
        if (retail.about_valid && y >= 61 && y + height <= 100)
            ipodjs_retailos_blit_part(d, &retail.band, x, y - 61,
                x, y, width, height);
        clipped_text(d, x, y, width, about_scroll.cells[i].font, offset,
            about_scroll.cells[i].value);
        d->update_rect(x, y, width, height);
    }
}

bool ipodjs_settings_dialog_available(struct screen *d)
{
    return d && retail.dialog_valid && ipodjs_ui_enabled(d->screen_type) &&
        !button_hold() && get_current_activity() != ACTIVITY_PLUGIN;
}

bool ipodjs_settings_draw_button(struct screen *d, int x, int y, int width,
    bool active, const char *label)
{
    if (!ipodjs_settings_dialog_available(d))
        return false;
    span(d, retail.button[active ? 0 : 1], x, y, width);
    text(d, x + 5, y + (active ? 4 : 6), width - 10, label, true, true);
    return true;
}

bool ipodjs_settings_draw_notice(struct screen *d, int x, int y,
    int width, int height)
{
    if (!ipodjs_settings_dialog_available(d) || width < 16 || height != 80)
        return false;
    span(d, retail.overlay, x, y, width);
    return true;
}

bool ipodjs_settings_draw_confirmation(struct screen *d,
    const char *const *lines, int count, const char *accept,
    const char *cancel, int default_button, int seconds)
{
    if (!ipodjs_settings_dialog_available(d) || count <= 0 || count > 5)
        return false;
    /* Never elide a question or filename in a confirmation. The ordinary
     * scrolling dialog remains the fallback for long/localized content. */
    int font = ipodjs_ui_retailos_font(true);
    for (int i = 0; i < count; i++)
    {
        int width;
        font_getstringsize(P2STR((unsigned char *)lines[i]), &width, NULL, font);
        if (width > 296)
            return false;
    }
    d->set_viewport(NULL);
    d->set_drawmode(DRMODE_SOLID);
    d->set_background(LCD_RGBPACK(255,255,255));
    d->clear_display();
    /* TwoButtonDialog_ResetCancelTemplate: the active caps are at 92/144
     * for the left choice and 152/220 for the right choice. The 5px caps
     * make these spans 57/73px, not equal halves of the 135px normal pill.
     * Preserve the source 22/26px active/normal heights. */
    span(d, retail.overlay, 18, 168, 284);
    for (int i = 0; i < count; i++)
    {
        d->setfont(ipodjs_ui_retailos_font(true));
        d->set_foreground(LCD_RGBPACK(0,0,0));
        ipodjs_ui_puts_fit(d, 12, 35 + i * 22, 296,
            P2STR((unsigned char *)lines[i]), true);
    }
    span(d, retail.button[1], 91, 190, 135);
    span(d, retail.button[0], default_button == 1 ? 152 : 92, 192,
         default_button == 1 ? 73 : 57);
    text(d, 96, 196, 50, accept, true, true);
    text(d, 158, 196, 59, cancel, true, true);
    if (seconds >= 0)
    {
        char remaining[24];
        snprintf(remaining, sizeof(remaining), "%d", seconds);
        text(d, 260, 196, 40, remaining, true, true);
    }
    return true;
}

bool ipodjs_settings_draw_message(struct screen *d,
    const char *const *lines, int count)
{
    int font = ipodjs_ui_retailos_detail_font();
    int height = font_get(font)->height;
    if (!ipodjs_settings_dialog_available(d) || count <= 0 ||
        count > 68 / height)
        return false;
    for (int i = 0; i < count; i++)
    {
        int width;
        font_getstringsize(P2STR((unsigned char *)lines[i]), &width, NULL, font);
        if (width > 268) return false;
    }
    d->set_viewport(NULL);
    d->set_drawmode(DRMODE_SOLID);
    d->set_background(LCD_RGBPACK(255,255,255));
    d->clear_display();
    span(d, retail.overlay, 18, 80, 284);
    for (int i = 0; i < count; i++)
        text(d, 26, 80 + (80 - count * height) / 2 + i * height, 268,
            P2STR((unsigned char *)lines[i]), false, true);
    return true;
}
