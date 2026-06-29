/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/            \/
 *
 * Copyright (C) 2026
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#include "plugin.h"
#include "lib/helper.h"
#include "lib/pluginlib_exit.h"

#if defined(HAVE_LCD_COLOR) && (LCD_WIDTH >= 320) && (LCD_HEIGHT >= 240)

#define NB_MAPS_DIR ROCKBOX_DIR "/maps/new_brunswick"
#define NB_WORLD_W 10000
#define NB_WORLD_H 8200
#define NB_ZOOM_COUNT 6
#define NB_DETAIL_ZOOM 3
#define NB_MAP_FRAME_W 320
#define NB_MAP_FRAME_H 184
#define NB_PANEL_BG LCD_RGBPACK(248, 248, 246)
#define NB_PANEL_BORDER LCD_RGBPACK(190, 194, 190)
#define NB_TEXT_DARK LCD_RGBPACK(34, 38, 35)
#define NB_BLUE LCD_RGBPACK(0, 122, 255)

enum nb_map_mode {
    NB_MODE_STREET = 0,
    NB_MODE_TOPO,
    NB_MODE_IMAGERY,
    NB_MODE_COUNT
};

enum nb_line_kind {
    NB_LINE_COAST,
    NB_LINE_WATER,
    NB_LINE_ROAD_MAJOR,
    NB_LINE_ROAD_MINOR,
    NB_LINE_TRAIL
};

enum nb_place_kind {
    NB_PLACE_CITY,
    NB_PLACE_TOWN,
    NB_PLACE_PARK,
    NB_PLACE_WATER
};

struct nb_view {
    int center_x;
    int center_y;
    int zoom;
    enum nb_map_mode mode;
    bool labels;
};

struct nb_line {
    const int16_t *points;
    int count;
    const char *name;
    enum nb_line_kind kind;
    int min_zoom;
};

struct nb_place {
    int16_t x;
    int16_t y;
    const char *name;
    enum nb_place_kind kind;
    int min_zoom;
};

static const int zoom_scale[NB_ZOOM_COUNT] = { 8, 13, 22, 38, 66, 112 };
static const int tile_grid_for_zoom[NB_ZOOM_COUNT] = { 0, 0, 0, 10, 20, 40 };
static const char *tile_scale_for_zoom[NB_ZOOM_COUNT] = {
    "", "", "", "40 km", "20 km", "10 km"
};
static fb_data nb_tile_buf[NB_MAP_FRAME_W * NB_MAP_FRAME_H];
static int nb_tile_cache_mode = -1;
static int nb_tile_cache_zoom = -1;
static int nb_tile_cache_col = -1;
static int nb_tile_cache_row = -1;
static bool nb_tile_cache_valid;

static const int16_t nb_outline[] = {
    760, 1900, 1300, 970, 2450, 460, 3600, 520, 4520, 820,
    5450, 700, 6450, 1170, 7320, 2050, 7720, 3320, 8480, 4060,
    9330, 5200, 9140, 6000, 8400, 6460, 7490, 6280, 6640, 6880,
    5750, 7210, 5040, 6920, 4440, 7350, 3530, 7060, 3020, 6200,
    2360, 5900, 1770, 5100, 1290, 4620, 1050, 3600, 780, 2860,
    760, 1900,
};

static const int16_t saint_john_river[] = {
    1220, 1020, 1450, 1600, 1910, 2200, 2050, 2850, 2540, 3440,
    3010, 3920, 3560, 4290, 3970, 4710, 4380, 5310, 4880, 5870,
    5410, 6310, 6030, 6740, 6730, 7040,
};

static const int16_t miramichi_river[] = {
    5230, 2240, 5470, 2750, 5920, 3260, 6460, 3610, 6940, 3960,
    7480, 4310, 8070, 4560,
};

static const int16_t restigouche_river[] = {
    2480, 540, 3050, 570, 3780, 780, 4540, 820, 5280, 740,
};

static const int16_t petitcodiac_river[] = {
    7410, 6470, 7770, 6710, 8270, 6850, 8770, 7050,
};

static const int16_t bay_of_fundy_coast[] = {
    3300, 7040, 3870, 7450, 4720, 7350, 5350, 7020, 6130, 7010,
    6950, 7300, 7850, 7030, 8740, 6810, 9200, 6070,
};

static const int16_t route_2[] = {
    1270, 1390, 1820, 2130, 2360, 3030, 3050, 3870, 3690, 4470,
    4410, 5140, 5170, 5850, 5930, 6350, 6790, 6590, 7690, 6540,
};

static const int16_t route_1[] = {
    1120, 5360, 1760, 5570, 2440, 6090, 3200, 6640, 3940, 7060,
    4910, 7140, 5900, 6960, 6770, 7030, 7600, 6990, 8450, 6740,
    9210, 6040,
};

static const int16_t route_7[] = {
    4410, 5140, 4690, 5590, 5040, 6100, 5390, 6500, 5900, 6960,
};

static const int16_t route_8[] = {
    4410, 5140, 4820, 4590, 5280, 4010, 5780, 3500, 6270, 3290,
    6940, 3960,
};

static const int16_t route_11[] = {
    5280, 740, 5850, 1140, 6410, 1640, 6950, 2210, 7240, 2820,
    7460, 3540, 8070, 4560, 8640, 5480, 8770, 6480,
};

static const int16_t route_15[] = {
    6930, 6530, 7540, 6390, 8170, 6380, 8770, 6480,
};

static const int16_t route_17[] = {
    1270, 1390, 1820, 1220, 2450, 1020, 3050, 570,
};

static const int16_t route_108[] = {
    2450, 2700, 3250, 2580, 4100, 2570, 4970, 2700, 5920, 3260,
};

static const int16_t route_126[] = {
    6930, 6530, 6960, 5900, 7050, 5230, 7240, 4590, 7460, 3540,
};

static const int16_t fundy_trail[] = {
    6100, 6960, 6500, 7220, 7070, 7240, 7600, 6990,
};

static const struct nb_line nb_lines[] = {
    { nb_outline, ARRAYLEN(nb_outline) / 2, "New Brunswick", NB_LINE_COAST, 0 },
    { bay_of_fundy_coast, ARRAYLEN(bay_of_fundy_coast) / 2, "Bay of Fundy", NB_LINE_COAST, 0 },
    { saint_john_river, ARRAYLEN(saint_john_river) / 2, "Saint John River", NB_LINE_WATER, 0 },
    { miramichi_river, ARRAYLEN(miramichi_river) / 2, "Miramichi River", NB_LINE_WATER, 1 },
    { restigouche_river, ARRAYLEN(restigouche_river) / 2, "Restigouche River", NB_LINE_WATER, 1 },
    { petitcodiac_river, ARRAYLEN(petitcodiac_river) / 2, "Petitcodiac River", NB_LINE_WATER, 2 },
    { route_2, ARRAYLEN(route_2) / 2, "Route 2", NB_LINE_ROAD_MAJOR, 0 },
    { route_1, ARRAYLEN(route_1) / 2, "Route 1", NB_LINE_ROAD_MAJOR, 0 },
    { route_7, ARRAYLEN(route_7) / 2, "Route 7", NB_LINE_ROAD_MAJOR, 1 },
    { route_8, ARRAYLEN(route_8) / 2, "Route 8", NB_LINE_ROAD_MAJOR, 1 },
    { route_11, ARRAYLEN(route_11) / 2, "Route 11", NB_LINE_ROAD_MAJOR, 1 },
    { route_15, ARRAYLEN(route_15) / 2, "Route 15", NB_LINE_ROAD_MAJOR, 1 },
    { route_17, ARRAYLEN(route_17) / 2, "Route 17", NB_LINE_ROAD_MINOR, 2 },
    { route_108, ARRAYLEN(route_108) / 2, "Route 108", NB_LINE_ROAD_MINOR, 2 },
    { route_126, ARRAYLEN(route_126) / 2, "Route 126", NB_LINE_ROAD_MINOR, 2 },
    { fundy_trail, ARRAYLEN(fundy_trail) / 2, "Fundy Trail", NB_LINE_TRAIL, 3 },
};

static const struct nb_place nb_places[] = {
    { 4502, 4864, "Fredericton", NB_PLACE_CITY, 0 },
    { 6124, 6266, "Saint John", NB_PLACE_CITY, 0 },
    { 8006, 4465, "Moncton", NB_PLACE_CITY, 0 },
    { 4501, 170, "Campbellton", NB_PLACE_TOWN, 1 },
    { 6387, 1015, "Bathurst", NB_PLACE_CITY, 0 },
    { 6651, 2409, "Miramichi", NB_PLACE_CITY, 0 },
    { 1319, 1552, "Edmundston", NB_PLACE_CITY, 0 },
    { 2690, 4402, "Woodstock", NB_PLACE_TOWN, 1 },
    { 3233, 6622, "St. Stephen", NB_PLACE_TOWN, 2 },
    { 4788, 5134, "Oromocto", NB_PLACE_TOWN, 2 },
    { 6690, 5388, "Sussex", NB_PLACE_TOWN, 2 },
    { 8831, 4906, "Sackville", NB_PLACE_TOWN, 2 },
    { 8547, 4233, "Shediac", NB_PLACE_TOWN, 2 },
    { 2877, 6414, "Grand Falls", NB_PLACE_TOWN, 2 },
    { 8800, 5510, "Kouchibouguac", NB_PLACE_PARK, 2 },
    { 6550, 7290, "Fundy", NB_PLACE_PARK, 2 },
    { 4070, 2000, "Mount Carleton", NB_PLACE_PARK, 2 },
    { 6840, 2500, "Chaleur Bay", NB_PLACE_WATER, 1 },
    { 7480, 7200, "Bay of Fundy", NB_PLACE_WATER, 0 },
};

static int nb_tile_grid(const struct nb_view *view)
{
    if (view->zoom < NB_DETAIL_ZOOM)
        return 0;

    return tile_grid_for_zoom[view->zoom];
}

static void nb_tile_for_view(const struct nb_view *view,
                             int *grid, int *col, int *row)
{
    long x = view->center_x;
    long y = view->center_y;
    int g = nb_tile_grid(view);

    if (x < 0)
        x = 0;
    else if (x >= NB_WORLD_W)
        x = NB_WORLD_W - 1;

    if (y < 0)
        y = 0;
    else if (y >= NB_WORLD_H)
        y = NB_WORLD_H - 1;

    *grid = g;
    *col = (int)((x * g) / NB_WORLD_W);
    *row = (int)((y * g) / NB_WORLD_H);
}

static const char *nb_tile_prefix(enum nb_map_mode mode)
{
    return mode == NB_MODE_IMAGERY ? "imagery" : "street";
}

static bool nb_load_tile(enum nb_map_mode mode, int zoom, int col, int row)
{
    char path[MAX_PATH];
    const char *prefix = nb_tile_prefix(mode);
    int fd;
    ssize_t got;
    size_t want = sizeof(nb_tile_buf);

    if (nb_tile_cache_valid && nb_tile_cache_mode == (int)mode &&
        nb_tile_cache_zoom == zoom &&
        nb_tile_cache_col == col && nb_tile_cache_row == row)
        return true;

    rb->snprintf(path, sizeof(path), "%s/%s_z%d_%02d_%02d.r16",
                 NB_MAPS_DIR, prefix, zoom, col, row);

    fd = rb->open(path, O_RDONLY);
    if (fd < 0 && zoom == NB_DETAIL_ZOOM) {
        rb->snprintf(path, sizeof(path), "%s/%s_%02d_%02d.r16",
                     NB_MAPS_DIR, prefix, col, row);
        fd = rb->open(path, O_RDONLY);
    }

    if (fd < 0) {
        nb_tile_cache_valid = false;
        return false;
    }

    got = rb->read(fd, nb_tile_buf, want);
    rb->close(fd);

    if (got != (ssize_t)want) {
        nb_tile_cache_valid = false;
        return false;
    }

    nb_tile_cache_valid = true;
    nb_tile_cache_mode = mode;
    nb_tile_cache_zoom = zoom;
    nb_tile_cache_col = col;
    nb_tile_cache_row = row;
    return true;
}

static inline int nb_world_to_screen_x(const struct nb_view *view, int x)
{
    return LCD_WIDTH / 2 + (int)(((long)(x - view->center_x) *
                                  zoom_scale[view->zoom]) >> 8);
}

static inline int nb_world_to_screen_y(const struct nb_view *view, int y)
{
    return LCD_HEIGHT / 2 + (int)(((long)(y - view->center_y) *
                                   zoom_scale[view->zoom]) >> 8);
}

static int nb_pan_step_x(const struct nb_view *view)
{
    int step;

    if (view->zoom >= NB_DETAIL_ZOOM)
        step = NB_WORLD_W / nb_tile_grid(view);
    else
        step = (28 << 8) / zoom_scale[view->zoom];

    if (step < 24)
        step = 24;

    return step;
}

static int nb_pan_step_y(const struct nb_view *view)
{
    int step;

    if (view->zoom >= NB_DETAIL_ZOOM) {
        int grid = nb_tile_grid(view);
        step = NB_WORLD_H / grid;
    }
    else
        step = (28 << 8) / zoom_scale[view->zoom];

    if (step < 24)
        step = 24;

    return step;
}

static void nb_set_color(unsigned color)
{
    rb->lcd_set_foreground(color);
}

static void nb_fill_panel(int x, int y, int w, int h)
{
    nb_set_color(NB_PANEL_BG);
    rb->lcd_fillrect(x + 2, y, w - 4, h);
    rb->lcd_fillrect(x, y + 2, w, h - 4);

    nb_set_color(NB_PANEL_BORDER);
    rb->lcd_hline(x + 2, x + w - 3, y);
    rb->lcd_hline(x + 2, x + w - 3, y + h - 1);
    rb->lcd_vline(x, y + 2, y + h - 3);
    rb->lcd_vline(x + w - 1, y + 2, y + h - 3);
}

static void nb_puts_halo(int x, int y, const char *text,
                         unsigned fg, unsigned halo)
{
    nb_set_color(halo);
    rb->lcd_putsxy(x - 1, y, text);
    rb->lcd_putsxy(x + 1, y, text);
    rb->lcd_putsxy(x, y - 1, text);
    rb->lcd_putsxy(x, y + 1, text);
    nb_set_color(fg);
    rb->lcd_putsxy(x, y, text);
}

static void nb_clamp_view(struct nb_view *view)
{
    if (view->center_x < 0)
        view->center_x = 0;
    else if (view->center_x > NB_WORLD_W)
        view->center_x = NB_WORLD_W;

    if (view->center_y < 0)
        view->center_y = 0;
    else if (view->center_y > NB_WORLD_H)
        view->center_y = NB_WORLD_H;
}

static unsigned nb_line_color(enum nb_line_kind kind, enum nb_map_mode mode)
{
    switch (kind) {
    case NB_LINE_COAST:
        return mode == NB_MODE_IMAGERY ? LCD_RGBPACK(42, 122, 152) :
                                         LCD_RGBPACK(60, 126, 152);
    case NB_LINE_WATER:
        return LCD_RGBPACK(49, 126, 196);
    case NB_LINE_ROAD_MAJOR:
        return mode == NB_MODE_IMAGERY ? LCD_RGBPACK(255, 207, 82) :
                                         LCD_RGBPACK(245, 178, 44);
    case NB_LINE_ROAD_MINOR:
        return mode == NB_MODE_IMAGERY ? LCD_RGBPACK(255, 255, 255) :
                                         LCD_RGBPACK(255, 255, 255);
    case NB_LINE_TRAIL:
        return LCD_RGBPACK(70, 136, 74);
    }

    return LCD_BLACK;
}

static void nb_draw_background(const struct nb_view *view)
{
    int i;

    if (view->zoom >= NB_DETAIL_ZOOM &&
        (view->mode == NB_MODE_STREET || view->mode == NB_MODE_IMAGERY)) {
        int grid;
        int col;
        int row;

        nb_tile_for_view(view, &grid, &col, &row);
        rb->lcd_set_background(view->mode == NB_MODE_STREET ?
                               LCD_RGBPACK(241, 241, 238) :
                               LCD_RGBPACK(242, 242, 238));
        rb->lcd_clear_display();

        if (nb_load_tile(view->mode, view->zoom, col, row)) {
            rb->lcd_bitmap(nb_tile_buf, 0, 22,
                           NB_MAP_FRAME_W, NB_MAP_FRAME_H);
            return;
        }

        nb_set_color(LCD_RGBPACK(84, 88, 84));
        rb->lcd_putsxy(16, 74, "NB tile missing");
        rb->lcd_putsxy(16, 88, NB_MAPS_DIR);
        return;
    }

    rb->lcd_set_background(view->mode == NB_MODE_TOPO ?
                           LCD_RGBPACK(217, 224, 197) :
                           LCD_RGBPACK(236, 238, 225));
    rb->lcd_clear_display();

    nb_set_color(LCD_RGBPACK(122, 183, 215));
    rb->lcd_fillrect(0, LCD_HEIGHT - 54, LCD_WIDTH, 54);

    if (view->mode == NB_MODE_TOPO) {
        nb_set_color(LCD_RGBPACK(197, 212, 177));
        for (i = 24; i < LCD_HEIGHT - 50; i += 24)
            rb->lcd_hline(0, LCD_WIDTH - 1, i);

        nb_set_color(LCD_RGBPACK(182, 205, 162));
        for (i = -120; i < LCD_WIDTH; i += 58)
            rb->lcd_drawline(i, LCD_HEIGHT - 56, i + 160, 0);
    }
}

static void nb_draw_line_width(int x1, int y1, int x2, int y2, int width)
{
    int i;

    rb->lcd_drawline(x1, y1, x2, y2);

    for (i = 1; i < width; i++) {
        rb->lcd_drawline(x1, y1 + i, x2, y2 + i);
        rb->lcd_drawline(x1 + i, y1, x2 + i, y2);
    }
}

static bool nb_line_visible(enum nb_line_kind kind, enum nb_map_mode mode)
{
    if (mode == NB_MODE_IMAGERY &&
        (kind == NB_LINE_ROAD_MINOR || kind == NB_LINE_TRAIL))
        return false;

    if (mode == NB_MODE_STREET || mode == NB_MODE_IMAGERY)
        return true;

    return kind != NB_LINE_ROAD_MINOR;
}

static void nb_draw_polyline(const struct nb_view *view,
                             const struct nb_line *line)
{
    int i;
    int width = 1;
    int casing = 0;

    if (view->zoom < line->min_zoom || !nb_line_visible(line->kind, view->mode))
        return;

    if (line->kind == NB_LINE_ROAD_MAJOR) {
        width = view->zoom >= 2 ? 3 : 2;
        casing = width + 2;
    }
    else if (line->kind == NB_LINE_ROAD_MINOR) {
        width = 2;
        casing = 3;
    }

    if (casing > 0) {
        nb_set_color(view->mode == NB_MODE_IMAGERY ?
                     LCD_RGBPACK(66, 72, 68) :
                     LCD_RGBPACK(196, 198, 190));

        for (i = 0; i < line->count - 1; i++) {
            int x1 = nb_world_to_screen_x(view, line->points[i * 2]);
            int y1 = nb_world_to_screen_y(view, line->points[i * 2 + 1]);
            int x2 = nb_world_to_screen_x(view, line->points[i * 2 + 2]);
            int y2 = nb_world_to_screen_y(view, line->points[i * 2 + 3]);

            nb_draw_line_width(x1, y1, x2, y2, casing);
        }
    }

    nb_set_color(nb_line_color(line->kind, view->mode));

    for (i = 0; i < line->count - 1; i++) {
        int x1 = nb_world_to_screen_x(view, line->points[i * 2]);
        int y1 = nb_world_to_screen_y(view, line->points[i * 2 + 1]);
        int x2 = nb_world_to_screen_x(view, line->points[i * 2 + 2]);
        int y2 = nb_world_to_screen_y(view, line->points[i * 2 + 3]);

        nb_draw_line_width(x1, y1, x2, y2, width);
    }

    if (view->labels && view->zoom >= line->min_zoom + 2 && line->name) {
        int mid = (line->count / 2) * 2;
        int x = nb_world_to_screen_x(view, line->points[mid]);
        int y = nb_world_to_screen_y(view, line->points[mid + 1]);

        if (x > -40 && x < LCD_WIDTH + 40 && y > -10 && y < LCD_HEIGHT + 10) {
            nb_puts_halo(x + 2, y + 2, line->name,
                         LCD_RGBPACK(70, 74, 70), NB_PANEL_BG);
        }
    }
}

static unsigned nb_place_color(enum nb_place_kind kind, enum nb_map_mode mode)
{
    (void)mode;

    switch (kind) {
    case NB_PLACE_CITY:
        return LCD_RGBPACK(35, 35, 35);
    case NB_PLACE_TOWN:
        return LCD_RGBPACK(72, 72, 72);
    case NB_PLACE_PARK:
        return LCD_RGBPACK(42, 124, 56);
    case NB_PLACE_WATER:
        return LCD_RGBPACK(38, 108, 172);
    }

    return LCD_BLACK;
}

static void nb_draw_places(const struct nb_view *view)
{
    unsigned i;
    int font_w;
    int font_h;

    rb->lcd_getstringsize("M", &font_w, &font_h);

    for (i = 0; i < ARRAYLEN(nb_places); i++) {
        const struct nb_place *place = &nb_places[i];
        int x;
        int y;
        int dot = place->kind == NB_PLACE_CITY ? 5 : 3;

        if (view->zoom < place->min_zoom)
            continue;

        if (view->mode == NB_MODE_IMAGERY && place->kind == NB_PLACE_WATER)
            continue;

        x = nb_world_to_screen_x(view, place->x);
        y = nb_world_to_screen_y(view, place->y);

        if (x < -60 || x > LCD_WIDTH + 60 || y < -20 || y > LCD_HEIGHT + 20)
            continue;

        nb_set_color(nb_place_color(place->kind, view->mode));
        rb->lcd_fillrect(x - dot / 2, y - dot / 2, dot, dot);

        if (view->labels) {
            int text_w;
            int text_h;
            int label_x = x + 5;
            int label_y = y - font_h / 2;

            rb->lcd_getstringsize(place->name, &text_w, &text_h);
            if (label_x + text_w > LCD_WIDTH - 3)
                label_x = x - text_w - 6;
            if (label_x < 3)
                label_x = 3;
            if (label_y < 24)
                label_y = 24;
            if (label_y + text_h > LCD_HEIGHT - 28)
                label_y = LCD_HEIGHT - text_h - 28;

            nb_puts_halo(label_x, label_y, place->name,
                         view->mode == NB_MODE_IMAGERY ? NB_TEXT_DARK :
                         nb_place_color(place->kind, view->mode),
                         NB_PANEL_BG);
        }
    }
}

static void nb_draw_detail_label(const struct nb_view *view)
{
    char buf[48];
    int grid;
    int col;
    int row;

    if (view->zoom < NB_DETAIL_ZOOM ||
        (view->mode != NB_MODE_STREET && view->mode != NB_MODE_IMAGERY))
        return;

    nb_tile_for_view(view, &grid, &col, &row);
    nb_fill_panel(8, 29, 144, 18);
    nb_set_color(NB_TEXT_DARK);
    rb->snprintf(buf, sizeof(buf), "z%d %02d,%02d/%d",
                 view->zoom, col, row, grid);
    rb->lcd_putsxy(18, 34, buf);

    nb_fill_panel(LCD_WIDTH - 76, 29, 68, 18);
    nb_set_color(NB_BLUE);
    rb->lcd_putsxy(LCD_WIDTH - 65, 34, tile_scale_for_zoom[view->zoom]);
}

static const char *nb_mode_name(enum nb_map_mode mode)
{
    switch (mode) {
    case NB_MODE_STREET:
        return "Street";
    case NB_MODE_TOPO:
        return "Topo";
    case NB_MODE_IMAGERY:
        return "Imagery";
    case NB_MODE_COUNT:
        break;
    }

    return "";
}

static void nb_draw_status(const struct nb_view *view)
{
    char buf[64];

    nb_fill_panel(7, 5, 206, 18);
    nb_set_color(LCD_RGBPACK(126, 130, 126));
    rb->lcd_putsxy(15, 10, "Search New Brunswick");

    nb_fill_panel(LCD_WIDTH - 90, 5, 82, 18);
    nb_set_color(NB_BLUE);
    rb->snprintf(buf, sizeof(buf), "%s", nb_mode_name(view->mode));
    rb->lcd_putsxy(LCD_WIDTH - 78, 10, buf);

    nb_fill_panel(8, LCD_HEIGHT - 25, 134, 18);
    nb_set_color(NB_TEXT_DARK);
    rb->snprintf(buf, sizeof(buf), "z%d  %s", view->zoom + 1,
                 view->labels ? "Labels" : "Clean");
    rb->lcd_putsxy(18, LCD_HEIGHT - 20, buf);

    nb_fill_panel(LCD_WIDTH - 54, LCD_HEIGHT - 25, 46, 18);
    nb_set_color(NB_BLUE);
    rb->lcd_putsxy(LCD_WIDTH - 41, LCD_HEIGHT - 20, "NB");
}

static void nb_render_map(const struct nb_view *view)
{
    bool tiled = view->zoom >= NB_DETAIL_ZOOM &&
                 (view->mode == NB_MODE_STREET ||
                  view->mode == NB_MODE_IMAGERY);
    unsigned i;

    rb->lcd_setfont(FONT_SYSFIXED);
    nb_draw_background(view);

    if (!tiled || view->mode == NB_MODE_TOPO) {
        for (i = 0; i < ARRAYLEN(nb_lines); i++)
            nb_draw_polyline(view, &nb_lines[i]);
    }

    nb_draw_places(view);

    nb_draw_detail_label(view);
    nb_draw_status(view);
    rb->lcd_update();
}

static void nb_reset_view(struct nb_view *view)
{
    view->center_x = 5200;
    view->center_y = 4200;
    view->zoom = 0;
    view->mode = NB_MODE_STREET;
    view->labels = true;
}

static enum plugin_status nb_maps_main(void)
{
    struct nb_view view;
    bool dirty = true;

    nb_reset_view(&view);

#if LCD_DEPTH > 1
    rb->lcd_set_backdrop(NULL);
#endif

    while (true) {
        int button;
        int clean;
        int step_x;
        int step_y;

        if (dirty) {
            nb_render_map(&view);
            dirty = false;
        }

        button = rb->button_get_w_tmo(HZ / 10);
#if defined(BUTTON_MENU) && defined(BUTTON_SELECT)
        if ((rb->button_status() & (BUTTON_MENU | BUTTON_SELECT)) ==
            (BUTTON_MENU | BUTTON_SELECT))
            return PLUGIN_OK;
#endif

        if (button == BUTTON_NONE)
            continue;

        clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
        step_x = nb_pan_step_x(&view);
        step_y = nb_pan_step_y(&view);

        if (clean == BUTTON_LEFT && !(button & BUTTON_REL)) {
            view.center_x -= step_x;
            nb_clamp_view(&view);
            dirty = true;
        }
        else if (clean == BUTTON_RIGHT && !(button & BUTTON_REL)) {
            view.center_x += step_x;
            nb_clamp_view(&view);
            dirty = true;
        }
#ifdef BUTTON_MENU
        else if (clean == BUTTON_MENU && !(button & BUTTON_REL)) {
            view.center_y -= step_y;
            nb_clamp_view(&view);
            dirty = true;
        }
#endif
#ifdef BUTTON_PLAY
        else if (clean == BUTTON_PLAY && !(button & BUTTON_REL)) {
            view.center_y += step_y;
            nb_clamp_view(&view);
            dirty = true;
        }
#endif
#ifdef HAVE_SCROLLWHEEL
        else if (clean == BUTTON_SCROLL_FWD && !(button & BUTTON_REL)) {
            if (view.zoom < NB_ZOOM_COUNT - 1)
                view.zoom++;
            dirty = true;
        }
        else if (clean == BUTTON_SCROLL_BACK && !(button & BUTTON_REL)) {
            if (view.zoom > 0)
                view.zoom--;
            dirty = true;
        }
#endif
        else if (clean == BUTTON_SELECT) {
            if (button & BUTTON_REPEAT)
                view.labels = !view.labels;
            else if (!(button & BUTTON_REL)) {
                view.mode++;
                if (view.mode >= NB_MODE_COUNT)
                    view.mode = NB_MODE_STREET;
            }
            dirty = true;
        }
        else {
            exit_on_usb(button);
        }
    }
}

enum plugin_status plugin_start(const void *parameter)
{
    enum plugin_status status;

    (void)parameter;

    backlight_ignore_timeout();
    status = nb_maps_main();
    backlight_use_settings();

    return status;
}

#else

enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;
    rb->splash(HZ * 2, "NB Maps needs color 320x240+");
    return PLUGIN_ERROR;
}

#endif
