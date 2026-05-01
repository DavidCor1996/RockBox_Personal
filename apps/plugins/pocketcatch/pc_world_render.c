#include "pocketcatch.h"
#include "pc_red_gfx.h"
#include "pc_red_extra_gfx.h"
#include "pc_red_dojo_gfx.h"
#include "pc_red_pokecenter_gfx.h"
#include "lib/xlcd.h"

#define PC_GB_LIGHT    LCD_RGBPACK(0xe0, 0xf8, 0xd0)
#define PC_GB_MID      LCD_RGBPACK(0x88, 0xc0, 0x70)
#define PC_GB_DARK     LCD_RGBPACK(0x34, 0x68, 0x56)
#define PC_GB_DEEP     LCD_RGBPACK(0x08, 0x18, 0x20)
#define PC_GB_PANEL    LCD_RGBPACK(0xf0, 0xf7, 0xe0)

static const fb_data pc_gb_palette[4] = {
    PC_GB_DEEP,
    PC_GB_DARK,
    PC_GB_MID,
    PC_GB_LIGHT,
};

#define PC_WORLD_TILESET_MAX 96
#define PC_WORLD_TILE_CACHE_PIXELS (PC_WORLD_SUBTILE_SIZE * PC_WORLD_SUBTILE_SIZE)
#define PC_WORLD_NPC_TRAINERS 3
#define PC_BAG_ROWS 5
#define PC_BAG_ROW_H 18
#define PC_BAG_ICON_SIZE 14
#define PC_UI_BALL_SIZE 14
#define PC_UI_WEATHER_SIZE 18

static fb_data pc_world_tile_cache[PC_WORLD_TILESET_MAX][PC_WORLD_TILE_CACHE_PIXELS];
static fb_data pc_world_npc_pixels[PC_WORLD_NPC_TRAINERS][4][PC_WORLD_WALK_FRAMES]
                                  [PC_WORLD_TRAINER_MAX_W * PC_WORLD_TRAINER_MAX_H];
static fb_data pc_world_ui_ball_source_pixels[PC_BALL_MAX_W * PC_BALL_MAX_H];
static fb_data pc_world_ui_ball_small_pixels[PC_UI_BALL_SIZE * PC_UI_BALL_SIZE];
static fb_data pc_world_weather_pixels[5][PC_UI_WEATHER_SIZE * PC_UI_WEATHER_SIZE];
static fb_data pc_bag_row_pixels[PC_BAG_ROWS][PC_WORLD_CREATURE_MAX_W * PC_WORLD_CREATURE_MAX_H];
static int pc_world_cached_style_key = -1;
static int pc_world_cached_tile_count;
static struct pc_asset_bitmap pc_world_npc_assets[PC_WORLD_NPC_TRAINERS][4][PC_WORLD_WALK_FRAMES];
static struct pc_asset_bitmap pc_world_ui_ball_source;
static struct pc_asset_bitmap pc_world_ui_ball_small;
static struct pc_asset_bitmap pc_world_weather_icons[5];
static struct pc_asset_bitmap pc_bag_row_assets[PC_BAG_ROWS];
static int pc_bag_row_species[PC_BAG_ROWS] = { -1, -1, -1, -1, -1 };
static bool pc_world_ui_ball_tried;
static bool pc_world_weather_tried[5];

enum pc_world_time_mode {
    PC_WORLD_TIME_DAY = 0,
    PC_WORLD_TIME_SUNSET,
    PC_WORLD_TIME_NIGHT,
    PC_WORLD_TIME_DAWN
};

enum pc_world_weather {
    PC_WORLD_WEATHER_CLEAR = 0,
    PC_WORLD_WEATHER_CLOUDY,
    PC_WORLD_WEATHER_RAIN,
    PC_WORLD_WEATHER_FOG,
    PC_WORLD_WEATHER_SAND
};

static void draw_asset_bitmap(const struct bitmap *bmp, int x, int y)
{
#ifdef HAVE_LCD_COLOR
    rb->lcd_bitmap_transparent((const fb_data *)bmp->data, x, y,
                               bmp->width, bmp->height);
#else
    rb->lcd_bitmap_part((const fb_data *)bmp->data, 0, 0, bmp->width,
                        x, y, bmp->width, bmp->height);
#endif
}

static void draw_asset_bitmap_fit(const struct bitmap *bmp, int x, int y, int w, int h)
{
    const fb_data *src;
    int dx;
    int dy;

    if (bmp == NULL || bmp->data == NULL || w <= 0 || h <= 0)
        return;

    src = (const fb_data *)bmp->data;
    for (dy = 0; dy < h; ++dy)
    {
        int src_y = dy * bmp->height / MAX(1, h);

        for (dx = 0; dx < w; ++dx)
        {
            fb_data pixel;
            int src_x = dx * bmp->width / MAX(1, w);

            pixel = src[src_y * bmp->width + src_x];
            if (pixel == TRANSPARENT_COLOR)
                continue;
            rb->lcd_set_foreground(pixel);
            rb->lcd_drawpixel(x + dx, y + dy);
        }
    }
}

static const struct pc_asset_bitmap *ensure_npc_trainer_asset(int trainer_index,
                                                              int heading, int frame);
static const struct pc_asset_bitmap *ensure_ui_ball_asset(void);
static const struct pc_asset_bitmap *ensure_weather_icon_asset(enum pc_world_weather weather);
static const struct pc_asset_bitmap *ensure_bag_row_asset(int slot, int species_index);
static void draw_missing_asset_panel(int x, int y, int w, int h, const char *label);
static void fill_rect_outline(int x, int y, int w, int h,
                              fb_data fill, fb_data outline);
static void fit_text_small(char *buffer, size_t buffer_size,
                           const char *text, int max_width);
static void draw_flat_panel(int x, int y, int w, int h,
                            fb_data fill, fb_data outline, fb_data accent);

static void init_asset_bitmap(struct pc_asset_bitmap *asset,
                              fb_data *pixels, int capacity)
{
    rb->memset(asset, 0, sizeof(*asset));
    asset->pixels = pixels;
    asset->capacity = capacity;
}

static void scale_bitmap_nearest(const struct bitmap *src,
                                 struct bitmap *dst)
{
    int y;
    int x;
    fb_data *dst_pixels = (fb_data *)dst->data;
    const fb_data *src_pixels = (const fb_data *)src->data;

    for (y = 0; y < dst->height; ++y)
    {
        int src_y = y * src->height / MAX(1, dst->height);

        for (x = 0; x < dst->width; ++x)
        {
            int src_x = x * src->width / MAX(1, dst->width);

            dst_pixels[y * dst->width + x] =
                src_pixels[src_y * src->width + src_x];
        }
    }
}

static const struct pc_asset_bitmap *ensure_ui_ball_asset(void)
{
    char alt_path[MAX_PATH];
    int rc;

    if (pc_world_ui_ball_small.loaded)
        return &pc_world_ui_ball_small;
    if (pc_world_ui_ball_tried)
        return NULL;

    pc_world_ui_ball_tried = true;
    init_asset_bitmap(&pc_world_ui_ball_source,
                      pc_world_ui_ball_source_pixels,
                      sizeof(pc_world_ui_ball_source_pixels));
    pc_world_ui_ball_source.bmp.data = (char *)pc_world_ui_ball_source.pixels;
    rc = rb->read_bmp_file(PC_BALL_PATH, &pc_world_ui_ball_source.bmp,
                           pc_world_ui_ball_source.capacity, FORMAT_NATIVE, NULL);
    if (rc <= 0)
    {
        rb->snprintf(alt_path, sizeof(alt_path),
                     "%s/sprites/balls/ball_default_idle_0.bmp", PC_ASSET_ROOT_ALT);
        rc = rb->read_bmp_file(alt_path, &pc_world_ui_ball_source.bmp,
                               pc_world_ui_ball_source.capacity, FORMAT_NATIVE, NULL);
    }
    if (rc <= 0 || pc_world_ui_ball_source.bmp.width <= 0 ||
        pc_world_ui_ball_source.bmp.height <= 0)
    {
        rb->memset(&pc_world_ui_ball_source, 0, sizeof(pc_world_ui_ball_source));
        return NULL;
    }

    init_asset_bitmap(&pc_world_ui_ball_small,
                      pc_world_ui_ball_small_pixels,
                      sizeof(pc_world_ui_ball_small_pixels));
    pc_world_ui_ball_small.bmp.data = (char *)pc_world_ui_ball_small.pixels;
    pc_world_ui_ball_small.bmp.width = PC_UI_BALL_SIZE;
    pc_world_ui_ball_small.bmp.height = PC_UI_BALL_SIZE;
    scale_bitmap_nearest(&pc_world_ui_ball_source.bmp, &pc_world_ui_ball_small.bmp);
    pc_world_ui_ball_small.loaded = true;
    return &pc_world_ui_ball_small;
}

static const char *weather_icon_filename(enum pc_world_weather weather)
{
    switch (weather)
    {
        case PC_WORLD_WEATHER_CLOUDY:
            return "weather_cloudy.bmp";
        case PC_WORLD_WEATHER_RAIN:
            return "weather_rain.bmp";
        case PC_WORLD_WEATHER_FOG:
            return "weather_fog.bmp";
        case PC_WORLD_WEATHER_SAND:
            return "weather_sand.bmp";
        case PC_WORLD_WEATHER_CLEAR:
        default:
            return "weather_clear.bmp";
    }
}

static const struct pc_asset_bitmap *ensure_weather_icon_asset(enum pc_world_weather weather)
{
    struct pc_asset_bitmap *asset;
    char path[MAX_PATH];
    int rc;

    if ((unsigned int)weather >= ARRAYLEN(pc_world_weather_icons))
        return NULL;

    asset = &pc_world_weather_icons[weather];
    if (asset->loaded)
        return asset;
    if (pc_world_weather_tried[weather])
        return NULL;

    pc_world_weather_tried[weather] = true;
    init_asset_bitmap(asset, pc_world_weather_pixels[weather],
                      sizeof(pc_world_weather_pixels[weather]));
    asset->bmp.data = (char *)asset->pixels;
    rc = rb->snprintf(path, sizeof(path), "%s/sprites/ui/%s",
                      PC_ASSET_ROOT, weather_icon_filename(weather));
    (void)rc;
    rc = rb->read_bmp_file(path, &asset->bmp,
                           asset->capacity, FORMAT_NATIVE, NULL);
    if (rc <= 0)
    {
        rb->snprintf(path, sizeof(path), "%s/sprites/ui/%s",
                     PC_ASSET_ROOT_ALT, weather_icon_filename(weather));
        rc = rb->read_bmp_file(path, &asset->bmp,
                               asset->capacity, FORMAT_NATIVE, NULL);
    }
    if (rc <= 0 || asset->bmp.width <= 0 || asset->bmp.height <= 0)
    {
        rb->memset(asset, 0, sizeof(*asset));
        return NULL;
    }

    asset->loaded = true;
    return asset;
}

static void draw_text_small(int x, int y, fb_data color, const char *text)
{
    int old_mode = rb->lcd_get_drawmode();

    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(color);
    rb->lcd_putsxy(x, y, text);
    rb->lcd_set_drawmode(old_mode);
    rb->lcd_setfont(FONT_UI);
}

static void draw_missing_asset_panel(int x, int y, int w, int h, const char *label)
{
    char fitted[20];
    int text_w = 0;

    draw_flat_panel(x, y, w, h, PC_GB_PANEL, PC_GB_DEEP, PC_GB_DARK);
    if (label == NULL || label[0] == '\0')
        return;

    fit_text_small(fitted, sizeof(fitted), label, MAX(0, w - 6));
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_getstringsize(fitted, &text_w, NULL);
    rb->lcd_setfont(FONT_UI);
    draw_text_small(x + MAX(2, (w - text_w) / 2),
                    y + MAX(2, (h - 8) / 2),
                    PC_GB_DEEP, fitted);
}

static void fit_text_small(char *buffer, size_t buffer_size,
                           const char *text, int max_width)
{
    static const char ellipsis[] = "...";
    int text_width;
    int ellipsis_width;
    size_t len;

    rb->lcd_setfont(FONT_SYSFIXED);
    rb->strlcpy(buffer, text, buffer_size);
    rb->lcd_getstringsize(buffer, &text_width, NULL);
    if (text_width <= max_width)
    {
        rb->lcd_setfont(FONT_UI);
        return;
    }

    rb->lcd_getstringsize(ellipsis, &ellipsis_width, NULL);
    len = rb->strlen(buffer);
    while (len > 0)
    {
        buffer[--len] = '\0';
        rb->lcd_getstringsize(buffer, &text_width, NULL);
        if (text_width + ellipsis_width <= max_width)
        {
            rb->strlcat(buffer, ellipsis, buffer_size);
            rb->lcd_setfont(FONT_UI);
            return;
        }
    }

    buffer[0] = '\0';
    rb->lcd_setfont(FONT_UI);
}

static fb_data mix_color(fb_data a, fb_data b, int t, int max_t)
{
#ifdef HAVE_LCD_COLOR
    int ar = RGB_UNPACK_RED(a);
    int ag = RGB_UNPACK_GREEN(a);
    int ab = RGB_UNPACK_BLUE(a);
    int br = RGB_UNPACK_RED(b);
    int bg = RGB_UNPACK_GREEN(b);
    int bb = RGB_UNPACK_BLUE(b);
    int r = ar + ((br - ar) * t) / max_t;
    int g = ag + ((bg - ag) * t) / max_t;
    int bl = ab + ((bb - ab) * t) / max_t;

    return LCD_RGBPACK(r, g, bl);
#else
    int mixed = a + ((b - a) * t) / max_t;

    if (mixed < LCD_BLACK)
        mixed = LCD_BLACK;
    if (mixed > LCD_WHITE)
        mixed = LCD_WHITE;
    return (fb_data)mixed;
#endif
}

static int wrapped_phase(int phase, int period)
{
    int wrapped = phase % period;

    if (wrapped < 0)
        wrapped += period;
    return wrapped;
}

static enum pc_world_time_mode current_world_time_mode(void)
{
    struct tm *tm = rb->get_time();
    int hour = tm ? tm->tm_hour : 12;

    if (hour >= 6 && hour < 17)
        return PC_WORLD_TIME_DAY;
    if (hour >= 17 && hour < 20)
        return PC_WORLD_TIME_SUNSET;
    if (hour >= 20 || hour < 5)
        return PC_WORLD_TIME_NIGHT;
    return PC_WORLD_TIME_DAWN;
}

static enum pc_world_weather current_world_weather(const struct pc_world_state *world)
{
    struct tm *tm = rb->get_time();
    int day = tm ? tm->tm_yday : 0;
    int hour_bucket = tm ? (tm->tm_hour / 3) : 0;
    int roll = wrapped_phase(day * 11 + hour_bucket * 7 + world->scene * 5, 16);

    switch (world->scene)
    {
        case PC_WORLD_SCENE_VIRIDIAN_FOREST:
            if (roll < 5)
                return PC_WORLD_WEATHER_FOG;
            if (roll < 9)
                return PC_WORLD_WEATHER_RAIN;
            return PC_WORLD_WEATHER_CLEAR;

        case PC_WORLD_SCENE_PEWTER:
            if (roll < 5)
                return PC_WORLD_WEATHER_SAND;
            if (roll < 9)
                return PC_WORLD_WEATHER_CLOUDY;
            return PC_WORLD_WEATHER_CLEAR;

        case PC_WORLD_SCENE_ROUTE21_NORTH:
            if (roll < 7)
                return PC_WORLD_WEATHER_RAIN;
            if (roll < 10)
                return PC_WORLD_WEATHER_CLOUDY;
            return PC_WORLD_WEATHER_CLEAR;

        case PC_WORLD_SCENE_PALLET:
        case PC_WORLD_SCENE_ROUTE1_SOUTH:
        case PC_WORLD_SCENE_VIRIDIAN_SOUTH:
        case PC_WORLD_SCENE_ROUTE2_SOUTH:
            if (roll < 4)
                return PC_WORLD_WEATHER_CLOUDY;
            if (roll < 7)
                return PC_WORLD_WEATHER_RAIN;
            return PC_WORLD_WEATHER_CLEAR;

        default:
            return PC_WORLD_WEATHER_CLEAR;
    }
}

static const char *world_time_label(enum pc_world_time_mode time_mode)
{
    switch (time_mode)
    {
        case PC_WORLD_TIME_SUNSET:
            return "SUNSET";
        case PC_WORLD_TIME_NIGHT:
            return "NIGHT";
        case PC_WORLD_TIME_DAWN:
            return "DAWN";
        case PC_WORLD_TIME_DAY:
        default:
            return "DAY";
    }
}

static const char *world_weather_label(enum pc_world_weather weather)
{
    switch (weather)
    {
        case PC_WORLD_WEATHER_CLOUDY:
            return "CLOUDY";
        case PC_WORLD_WEATHER_RAIN:
            return "RAIN";
        case PC_WORLD_WEATHER_FOG:
            return "FOG";
        case PC_WORLD_WEATHER_SAND:
            return "SAND";
        case PC_WORLD_WEATHER_CLEAR:
        default:
            return "CLEAR";
    }
}

static fb_data tint_world_color(fb_data color,
                                enum pc_world_time_mode time_mode,
                                enum pc_world_weather weather)
{
    switch (time_mode)
    {
        case PC_WORLD_TIME_SUNSET:
            color = mix_color(color, LCD_RGBPACK(0xd8, 0xa0, 0x64), 2, 9);
            break;

        case PC_WORLD_TIME_NIGHT:
            color = mix_color(color, LCD_RGBPACK(0x24, 0x38, 0x64), 5, 8);
            break;

        case PC_WORLD_TIME_DAWN:
            color = mix_color(color, LCD_RGBPACK(0xc8, 0xde, 0xe8), 2, 9);
            break;

        case PC_WORLD_TIME_DAY:
        default:
            break;
    }

    switch (weather)
    {
        case PC_WORLD_WEATHER_CLOUDY:
            color = mix_color(color, LCD_RGBPACK(0xa8, 0xb0, 0xb8), 1, 6);
            break;

        case PC_WORLD_WEATHER_RAIN:
            color = mix_color(color, LCD_RGBPACK(0x5c, 0x88, 0xb0), 1, 5);
            break;

        case PC_WORLD_WEATHER_FOG:
            color = mix_color(color, LCD_RGBPACK(0xd8, 0xe8, 0xd8), 1, 4);
            break;

        case PC_WORLD_WEATHER_SAND:
            color = mix_color(color, LCD_RGBPACK(0xc8, 0xb0, 0x74), 1, 5);
            break;

        case PC_WORLD_WEATHER_CLEAR:
        default:
            break;
    }

    return color;
}

static void fill_capsule(int x, int y, int w, int h, fb_data color)
{
    int radius = h / 2;

    rb->lcd_set_foreground(color);
    rb->lcd_fillrect(x + radius, y, w - 2 * radius, h);
    xlcd_fillcircle(x + radius, y + radius, radius);
    xlcd_fillcircle(x + w - radius - 1, y + radius, radius);
}

static void draw_flat_panel(int x, int y, int w, int h,
                            fb_data fill, fb_data outline, fb_data accent)
{
    rb->lcd_set_foreground(LCD_RGBPACK(0xa8, 0xb8, 0x98));
    rb->lcd_fillrect(x + 2, y + 2, w, h);
    rb->lcd_set_foreground(fill);
    rb->lcd_fillrect(x, y, w, h);
    rb->lcd_set_foreground(accent);
    rb->lcd_fillrect(x, y, w, 2);
    rb->lcd_set_foreground(outline);
    rb->lcd_drawrect(x, y, w, h);
}

static bool rect_visible(int x, int y, int w, int h)
{
    return !(x >= LCD_WIDTH || y >= LCD_HEIGHT || x + w <= 0 || y + h <= 0);
}

static int block_screen_x(const struct pc_world_state *world, int tx)
{
    return world->origin_x + tx * PC_WORLD_TILE_SIZE;
}

static int block_screen_y(const struct pc_world_state *world, int ty)
{
    return world->origin_y + ty * PC_WORLD_TILE_SIZE;
}

static bool outdoor_scene(enum pc_world_scene scene)
{
    return scene == PC_WORLD_SCENE_PALLET ||
           scene == PC_WORLD_SCENE_ROUTE1_SOUTH ||
           scene == PC_WORLD_SCENE_VIRIDIAN_SOUTH ||
           scene == PC_WORLD_SCENE_ROUTE2_SOUTH ||
           scene == PC_WORLD_SCENE_ROUTE21_NORTH ||
           scene == PC_WORLD_SCENE_VIRIDIAN_FOREST ||
           scene == PC_WORLD_SCENE_PEWTER ||
           scene == PC_WORLD_SCENE_ROUTE22 ||
           scene == PC_WORLD_SCENE_ROUTE3 ||
           scene == PC_WORLD_SCENE_ROUTE4 ||
           scene == PC_WORLD_SCENE_CERULEAN ||
           scene == PC_WORLD_SCENE_ROUTE24 ||
           scene == PC_WORLD_SCENE_ROUTE25;
}

enum pc_scene_tileset_group {
    PC_SCENE_TILESET_OVERWORLD = 0,
    PC_SCENE_TILESET_REDS_HOUSE,
    PC_SCENE_TILESET_HOUSE,
    PC_SCENE_TILESET_POKECENTER,
    PC_SCENE_TILESET_GYM,
    PC_SCENE_TILESET_FOREST,
    PC_SCENE_TILESET_GATE,
    PC_SCENE_TILESET_LAB,
    PC_SCENE_TILESET_CAVERN,
    PC_SCENE_TILESET_CLUB,
    PC_SCENE_TILESET_SHIP
};

static enum pc_scene_tileset_group scene_tileset_group(enum pc_world_scene scene)
{
    switch (scene)
    {
        case PC_WORLD_SCENE_HOUSE_1F:
        case PC_WORLD_SCENE_HOUSE_2F:
            return PC_SCENE_TILESET_REDS_HOUSE;

        case PC_WORLD_SCENE_BLUES_HOUSE:
        case PC_WORLD_SCENE_VIRIDIAN_SCHOOL_HOUSE:
        case PC_WORLD_SCENE_VIRIDIAN_NICKNAME_HOUSE:
        case PC_WORLD_SCENE_ROUTE2_TRADE_HOUSE:
        case PC_WORLD_SCENE_PEWTER_NIDORAN_HOUSE:
        case PC_WORLD_SCENE_PEWTER_SPEECH_HOUSE:
        case PC_WORLD_SCENE_CERULEAN_TRADE_HOUSE:
        case PC_WORLD_SCENE_CERULEAN_TRASHED_HOUSE:
        case PC_WORLD_SCENE_BILLS_HOUSE:
            return PC_SCENE_TILESET_HOUSE;

        case PC_WORLD_SCENE_VIRIDIAN_MART:
        case PC_WORLD_SCENE_VIRIDIAN_POKECENTER:
        case PC_WORLD_SCENE_PEWTER_MART:
        case PC_WORLD_SCENE_PEWTER_POKECENTER:
        case PC_WORLD_SCENE_MT_MOON_POKECENTER:
        case PC_WORLD_SCENE_CERULEAN_MART:
        case PC_WORLD_SCENE_CERULEAN_POKECENTER:
            return PC_SCENE_TILESET_POKECENTER;

        case PC_WORLD_SCENE_VIRIDIAN_GYM:
        case PC_WORLD_SCENE_PEWTER_GYM:
        case PC_WORLD_SCENE_CERULEAN_GYM:
            return PC_SCENE_TILESET_GYM;

        case PC_WORLD_SCENE_VIRIDIAN_FOREST:
            return PC_SCENE_TILESET_FOREST;

        case PC_WORLD_SCENE_ROUTE2_GATE:
        case PC_WORLD_SCENE_ROUTE22_GATE:
        case PC_WORLD_SCENE_VIRIDIAN_FOREST_SOUTH_GATE:
        case PC_WORLD_SCENE_VIRIDIAN_FOREST_NORTH_GATE:
        case PC_WORLD_SCENE_MUSEUM_1F:
        case PC_WORLD_SCENE_MUSEUM_2F:
            return PC_SCENE_TILESET_GATE;

        case PC_WORLD_SCENE_OAKS_LAB:
            return PC_SCENE_TILESET_LAB;

        case PC_WORLD_SCENE_MT_MOON_1F:
        case PC_WORLD_SCENE_MT_MOON_B1F:
        case PC_WORLD_SCENE_MT_MOON_B2F:
            return PC_SCENE_TILESET_CAVERN;

        case PC_WORLD_SCENE_BIKE_SHOP:
            return PC_SCENE_TILESET_CLUB;

        case PC_WORLD_SCENE_CERULEAN_BADGE_HOUSE:
            return PC_SCENE_TILESET_SHIP;

        case PC_WORLD_SCENE_PALLET:
        case PC_WORLD_SCENE_ROUTE1_SOUTH:
        case PC_WORLD_SCENE_VIRIDIAN_SOUTH:
        case PC_WORLD_SCENE_ROUTE2_SOUTH:
        case PC_WORLD_SCENE_ROUTE21_NORTH:
        case PC_WORLD_SCENE_PEWTER:
        case PC_WORLD_SCENE_ROUTE3:
        case PC_WORLD_SCENE_ROUTE4:
        case PC_WORLD_SCENE_ROUTE22:
        case PC_WORLD_SCENE_CERULEAN:
        case PC_WORLD_SCENE_ROUTE24:
        case PC_WORLD_SCENE_ROUTE25:
        default:
            return PC_SCENE_TILESET_OVERWORLD;
    }
}

static void draw_spawn(const struct pc_world_state *world, int index)
{
    const struct pc_world_spawn *spawn = &world->spawns[index];
    const struct pc_creature_def *creature;
    int screen_x;
    int screen_y;
    int shadow_w = PC_WORLD_SCALE(12);
    int sprite_w = PC_WORLD_SCALE(24);
    int sprite_h = PC_WORLD_SCALE(24);

    if (!spawn->active)
        return;

    screen_x = world->origin_x + spawn->x;
    screen_y = world->origin_y + spawn->y;
    creature = pc_assets_get_creature(spawn->species_index);
    if (world->assets.creature[index].loaded)
    {
        shadow_w = MAX(PC_WORLD_SCALE(12), world->assets.creature[index].bmp.width / 2);
        sprite_w = world->assets.creature[index].bmp.width;
        sprite_h = world->assets.creature[index].bmp.height;
    }
    else if (creature != NULL)
    {
        sprite_w = PC_WORLD_SCALE(MAX(18, creature->sprite_w));
        sprite_h = PC_WORLD_SCALE(MAX(18, creature->sprite_h));
    }

    if (!rect_visible(screen_x - sprite_w / 2,
                      screen_y - sprite_h / 2,
                      sprite_w, sprite_h + PC_WORLD_FOOT_Y_OFFSET + PC_WORLD_SCALE(4)))
    {
        return;
    }

    fill_capsule(screen_x - shadow_w / 2,
                 screen_y + PC_WORLD_FOOT_Y_OFFSET,
                 shadow_w,
                 PC_WORLD_SCALE(4),
                 LCD_RGBPACK(0x60, 0x70, 0x58));

    if (world->assets.creature[index].loaded)
    {
        draw_asset_bitmap(&world->assets.creature[index].bmp,
                          screen_x - world->assets.creature[index].bmp.width / 2,
                          screen_y - world->assets.creature[index].bmp.height / 2);
    }
    else if (creature != NULL)
    {
        rb->lcd_set_foreground(creature->primary);
        xlcd_fillcircle(screen_x, screen_y, PC_WORLD_SCALE(10));
        rb->lcd_set_foreground(creature->accent);
        rb->lcd_fillrect(screen_x - PC_WORLD_SCALE(5),
                         screen_y - PC_WORLD_SCALE(2),
                         PC_WORLD_SCALE(10),
                         PC_WORLD_SCALE(4));
    }
}

static void draw_secret_item_ball(const struct pc_world_state *world, int index)
{
    const struct pc_asset_bitmap *ball = ensure_ui_ball_asset();
    int metatile_x;
    int metatile_y;
    int step = PC_WORLD_TILE_SIZE / 2;
    int screen_x;
    int screen_y;

    if (!pc_world_secret_draw_info(world, index, &metatile_x, &metatile_y))
        return;

    screen_x = world->origin_x + metatile_x * step + step / 2;
    screen_y = world->origin_y + metatile_y * step + step / 2
               - PC_WORLD_SCALE(2);

    if (screen_x < -PC_WORLD_SCALE(12) || screen_x > LCD_WIDTH + PC_WORLD_SCALE(12) ||
        screen_y < -PC_WORLD_SCALE(12) || screen_y > LCD_HEIGHT + PC_WORLD_SCALE(12))
    {
        return;
    }

    fill_capsule(screen_x - PC_WORLD_SCALE(6),
                 screen_y + PC_WORLD_SCALE(4),
                 PC_WORLD_SCALE(12),
                 PC_WORLD_SCALE(3),
                 LCD_RGBPACK(0x60, 0x70, 0x58));

    if (ball != NULL)
    {
        draw_asset_bitmap(&ball->bmp,
                          screen_x - ball->bmp.width / 2,
                          screen_y - ball->bmp.height / 2);
        return;
    }

    draw_missing_asset_panel(screen_x - 6, screen_y - 6, 12, 12, "");
}

static void draw_pokestop(const struct pc_world_state *world, int index)
{
    bool ready;
    int metatile_x;
    int metatile_y;
    int step = PC_WORLD_TILE_SIZE / 2;
    int screen_x;
    int screen_y;
    int pulse = wrapped_phase(world->frame + index * 3, 18);
    int halo = 9 + (pulse < 9 ? pulse / 3 : (18 - pulse) / 3);
    fb_data ring;
    fb_data accent;
    fb_data glow;

    if (!pc_world_pokestop_draw_info(world, index, &metatile_x, &metatile_y, &ready))
        return;

    ring = ready ? LCD_RGBPACK(0x58, 0xbd, 0xe5) : LCD_RGBPACK(0x8d, 0x79, 0xb9);
    accent = ready ? LCD_RGBPACK(0xd9, 0xf7, 0xff) : LCD_RGBPACK(0xc8, 0xbf, 0xda);
    glow = ready ? LCD_RGBPACK(0xb9, 0xf2, 0xff) : LCD_RGBPACK(0xd8, 0xc9, 0xee);
    screen_x = world->origin_x + metatile_x * step + step / 2;
    screen_y = world->origin_y + metatile_y * step + step / 2 - PC_WORLD_SCALE(2);

    if (screen_x < -PC_WORLD_SCALE(16) || screen_x > LCD_WIDTH + PC_WORLD_SCALE(16) ||
        screen_y < -PC_WORLD_SCALE(16) || screen_y > LCD_HEIGHT + PC_WORLD_SCALE(16))
    {
        return;
    }

    fill_capsule(screen_x - PC_WORLD_SCALE(8),
                 screen_y + PC_WORLD_SCALE(5),
                 PC_WORLD_SCALE(16),
                 PC_WORLD_SCALE(3),
                 LCD_RGBPACK(0x60, 0x70, 0x58));
    rb->lcd_set_foreground(glow);
    xlcd_drawcircle(screen_x, screen_y - 3, halo);
    rb->lcd_set_foreground(LCD_RGBPACK(0x6a, 0x92, 0xb8));
    rb->lcd_fillrect(screen_x - 3, screen_y + 2, 6, 12);
    rb->lcd_set_foreground(ring);
    xlcd_fillcircle(screen_x, screen_y - 3, 10);
    rb->lcd_set_foreground(PC_GB_PANEL);
    xlcd_fillcircle(screen_x, screen_y - 3, 7);
    rb->lcd_set_foreground(accent);
    rb->lcd_fillrect(screen_x - 6, screen_y - 9, 12, 12);
    rb->lcd_set_foreground(ring);
    rb->lcd_drawrect(screen_x - 6, screen_y - 9, 12, 12);
    rb->lcd_drawline(screen_x - 6, screen_y - 9, screen_x, screen_y - 14);
    rb->lcd_drawline(screen_x + 5, screen_y - 9, screen_x, screen_y - 14);
    rb->lcd_drawline(screen_x - 6, screen_y + 2, screen_x, screen_y + 7);
    rb->lcd_drawline(screen_x + 5, screen_y + 2, screen_x, screen_y + 7);
    rb->lcd_set_foreground(ring);
    xlcd_drawcircle(screen_x, screen_y - 3, 11);
}

static void draw_pokestop_overlay(const struct pc_world_state *world)
{
    const struct pc_asset_bitmap *ball = ensure_ui_ball_asset();
    static const signed char orbit_x[8] = { 0, 8, 12, 8, 0, -8, -12, -8 };
    static const signed char orbit_y[8] = { -12, -8, 0, 8, 12, 8, 0, -8 };
    int x = 10;
    int y = 6;
    int w = LCD_WIDTH - 20;
    int h = LCD_HEIGHT - 12;
    int center_x = x + w / 2;
    int center_y = y + 64;
    int phase = wrapped_phase(world->pokestop_spin_angle / 18, 8);
    int slot_x = center_x + orbit_x[phase];
    int slot_y = center_y + orbit_y[phase];
    int pulse = wrapped_phase(world->frame, 16);
    int progress_w = ((w - 52) * MIN(world->pokestop_spin_progress, PC_POKESTOP_SPIN_TARGET)) /
                     PC_POKESTOP_SPIN_TARGET;
    char line[32];

    draw_flat_panel(x, y, w, h, PC_GB_PANEL, PC_GB_DEEP,
                    world->pokestop_spun ? LCD_RGBPACK(0xa2, 0x97, 0xc5)
                                         : LCD_RGBPACK(0x68, 0xc8, 0xf1));
    draw_flat_panel(x + 8, y + 8, w - 16, 24, PC_GB_LIGHT, PC_GB_DEEP,
                    world->pokestop_spun ? LCD_RGBPACK(0xe2, 0xdb, 0xf1)
                                         : LCD_RGBPACK(0xd7, 0xf7, 0xff));
    draw_text_small(x + 16, y + 16, PC_GB_DEEP, "POKESTOP");
    draw_flat_panel(x + w - 64, y + 11, 46, 16, PC_GB_PANEL, PC_GB_DEEP,
                    world->pokestop_spun ? LCD_RGBPACK(0xb1, 0x9c, 0xd7)
                                         : LCD_RGBPACK(0x8c, 0xe1, 0xff));
    draw_text_small(x + w - 56, y + 16, PC_GB_DEEP,
                    world->pokestop_spun ? "SPUN" : "READY");
    draw_text_small(x + 16, y + 38, PC_GB_DARK,
                    world->pokestop_spun ? "Supplies collected" : "Spin the photo disc");

    rb->lcd_set_foreground(LCD_RGBPACK(0x6a, 0x92, 0xb8));
    rb->lcd_fillrect(center_x - 6, center_y + 12, 12, 34);
    fill_capsule(center_x - 18, center_y + 44, 36, 6, LCD_RGBPACK(0x70, 0x82, 0x76));
    rb->lcd_set_foreground(world->pokestop_spun ? LCD_RGBPACK(0x8d, 0x79, 0xb9)
                                                : LCD_RGBPACK(0x58, 0xbd, 0xe5));
    xlcd_fillcircle(center_x, center_y, 34);
    rb->lcd_set_foreground(PC_GB_PANEL);
    xlcd_fillcircle(center_x, center_y, 26);
    rb->lcd_set_foreground(world->pokestop_spun ? LCD_RGBPACK(0xc6, 0xbb, 0xde)
                                                : LCD_RGBPACK(0xa8, 0xee, 0xff));
    xlcd_fillcircle(center_x, center_y, 18);
    rb->lcd_set_foreground(world->pokestop_spun ? LCD_RGBPACK(0xe0, 0xd8, 0xf0)
                                                : LCD_RGBPACK(0xa8, 0xee, 0xff));
    rb->lcd_fillrect(slot_x - 11, slot_y - 11, 22, 22);
    rb->lcd_set_foreground(LCD_RGBPACK(0x58, 0xbd, 0xe5));
    rb->lcd_drawrect(slot_x - 11, slot_y - 11, 22, 22);
    rb->lcd_drawline(slot_x - 11, slot_y - 11, slot_x, slot_y - 20);
    rb->lcd_drawline(slot_x + 10, slot_y - 11, slot_x, slot_y - 20);
    rb->lcd_drawline(slot_x - 11, slot_y + 10, slot_x, slot_y + 19);
    rb->lcd_drawline(slot_x + 10, slot_y + 10, slot_x, slot_y + 19);
    rb->lcd_set_foreground(LCD_RGBPACK(0x58, 0xbd, 0xe5));
    xlcd_drawcircle(center_x, center_y, 37);
    xlcd_drawcircle(center_x, center_y, 40 + (pulse < 8 ? 0 : 1));

    draw_text_small(x + 16, y + 116, PC_GB_DEEP, "SPIN");
    fill_rect_outline(x + 16, y + 126, w - 32, 12, PC_GB_LIGHT, PC_GB_DEEP);
    rb->lcd_set_foreground(world->pokestop_spun ? LCD_RGBPACK(0x98, 0x8d, 0xbe)
                                                : LCD_RGBPACK(0x58, 0xbd, 0xe5));
    rb->lcd_fillrect(x + 18, y + 128, MAX(0, progress_w), 8);
    rb->snprintf(line, sizeof(line), "%d / %d",
                 MIN(world->pokestop_spin_progress, PC_POKESTOP_SPIN_TARGET),
                 PC_POKESTOP_SPIN_TARGET);
    draw_text_small(center_x - 24, y + 142, PC_GB_DEEP, line);

    if (world->pokestop_spun)
    {
        draw_text_small(x + 16, y + 156, PC_GB_DEEP, "SUPPLIES");
        draw_flat_panel(x + 16, y + 166, 74, 28, PC_GB_LIGHT, PC_GB_DEEP, PC_GB_MID);
        draw_flat_panel(x + 98, y + 166, 74, 28, PC_GB_LIGHT, PC_GB_DEEP, PC_GB_MID);
        if (ball != NULL)
            draw_asset_bitmap(&ball->bmp, x + 28, y + 173);
        draw_text_small(x + 48, y + 171, PC_GB_DARK, "BALLS");
        rb->snprintf(line, sizeof(line), "x%d", world->pokestop_reward_balls);
        draw_text_small(x + 48, y + 181, PC_GB_DEEP, line);
        draw_text_small(x + 112, y + 171, PC_GB_DARK, "COINS");
        rb->snprintf(line, sizeof(line), "$%d", world->pokestop_reward_money);
        draw_text_small(x + 112, y + 181, PC_GB_DEEP, line);
        draw_text_small(x + 18, y + 199, PC_GB_DARK, "Select close  Left back");
    }
    else
    {
        draw_flat_panel(x + 16, y + 156, w - 32, 34, PC_GB_LIGHT, PC_GB_DEEP, PC_GB_MID);
        draw_text_small(x + 24, y + 166, PC_GB_DARK, "Rotate the clickwheel until the bar fills");
        draw_text_small(x + 24, y + 178, PC_GB_DARK, "A cleaner spin gives a small supply bonus");
        draw_text_small(x + 24, y + 194, PC_GB_DARK, "Left back");
    }
}

static void draw_weather_overlay(const struct pc_world_state *world)
{
    enum pc_world_time_mode time_mode = current_world_time_mode();
    enum pc_world_weather weather = current_world_weather(world);
    int i;

    if (!outdoor_scene(world->scene) || world->view != PC_WORLD_VIEW_MAP)
        return;

    if (time_mode == PC_WORLD_TIME_NIGHT)
    {
        rb->lcd_set_foreground(LCD_RGBPACK(0xd8, 0xe8, 0xf0));
        for (i = 0; i < 8; ++i)
        {
            int x = wrapped_phase(world->frame * 2 + i * 37 + world->scene * 11, LCD_WIDTH);
            int y = 12 + wrapped_phase(i * 19 + world->scene * 7, 36);

            rb->lcd_drawpixel(x, y);
        }
    }

    if (weather == PC_WORLD_WEATHER_RAIN)
    {
        rb->lcd_set_foreground(LCD_RGBPACK(0x70, 0xa8, 0xd8));
        for (i = 0; i < 16; ++i)
        {
            int x = wrapped_phase(world->frame * 5 + i * 29 + world->scene * 13, LCD_WIDTH + 12) - 6;
            int y = wrapped_phase(world->frame * 9 + i * 17, LCD_HEIGHT + 18) - 9;

            rb->lcd_drawline(x, y, x - 2, y + 6);
        }
    }
    else if (weather == PC_WORLD_WEATHER_FOG)
    {
        rb->lcd_set_foreground(LCD_RGBPACK(0xc8, 0xd8, 0xc8));
        for (i = 0; i < 6; ++i)
        {
            int x = wrapped_phase(i * 41 + world->scene * 5, LCD_WIDTH + 20) - 10;
            int y = 28 + wrapped_phase(world->frame + i * 23, 96);

            xlcd_fillcircle(x, y, 5);
        }
    }
    else if (weather == PC_WORLD_WEATHER_SAND)
    {
        rb->lcd_set_foreground(LCD_RGBPACK(0xc8, 0xb0, 0x74));
        for (i = 0; i < 18; ++i)
        {
            int x = wrapped_phase(world->frame * 4 + i * 13, LCD_WIDTH + 8) - 4;
            int y = wrapped_phase(world->frame * 2 + i * 27, LCD_HEIGHT);

            rb->lcd_drawpixel(x, y);
            rb->lcd_drawpixel(x + 1, y);
        }
    }
}

static void scale_tile_to_cache(const unsigned char tiles[][8][8], int tile_count,
                                int tile_id,
                                enum pc_world_time_mode time_mode,
                                enum pc_world_weather weather)
{
    int py;
    int px;
    int sy;
    int sx;
    fb_data *dst;

    if (tile_id < 0 || tile_id >= tile_count || tile_id >= PC_WORLD_TILESET_MAX)
        return;

    dst = pc_world_tile_cache[tile_id];
    for (py = 0; py < 8; ++py)
    {
        for (px = 0; px < 8; ++px)
        {
            fb_data color = tint_world_color(pc_gb_palette[tiles[tile_id][py][px]],
                                             time_mode, weather);

            for (sy = 0; sy < PC_WORLD_PIXEL_SCALE; ++sy)
            {
                for (sx = 0; sx < PC_WORLD_PIXEL_SCALE; ++sx)
                {
                    dst[(py * PC_WORLD_PIXEL_SCALE + sy) * PC_WORLD_SUBTILE_SIZE
                        + (px * PC_WORLD_PIXEL_SCALE + sx)] = color;
                }
            }
        }
    }
}

static void select_tileset(enum pc_world_scene scene,
                           const unsigned char (**tiles)[8][8],
                           const unsigned char (**blocks)[4][4],
                           int *tile_count,
                           int *block_count)
{
    switch (scene_tileset_group(scene))
    {
        case PC_SCENE_TILESET_REDS_HOUSE:
            *tiles = pc_red_reds_house_tiles;
            *blocks = pc_red_reds_house_blocks;
            *tile_count = PC_RED_REDS_HOUSE_TILE_COUNT;
            *block_count = PC_RED_REDS_HOUSE_BLOCK_COUNT;
            break;

        case PC_SCENE_TILESET_HOUSE:
            *tiles = pc_red_house_tiles;
            *blocks = pc_red_house_blocks;
            *tile_count = PC_RED_HOUSE_TILE_COUNT;
            *block_count = PC_RED_HOUSE_BLOCK_COUNT;
            break;

        case PC_SCENE_TILESET_POKECENTER:
            *tiles = pc_red_pokecenter_tiles;
            *blocks = pc_red_pokecenter_blocks;
            *tile_count = PC_RED_POKECENTER_TILE_COUNT;
            *block_count = PC_RED_POKECENTER_BLOCK_COUNT;
            break;

        case PC_SCENE_TILESET_GYM:
            *tiles = pc_red_dojo_tiles;
            *blocks = pc_red_dojo_blocks;
            *tile_count = PC_RED_DOJO_TILE_COUNT;
            *block_count = PC_RED_DOJO_BLOCK_COUNT;
            break;

        case PC_SCENE_TILESET_FOREST:
            *tiles = pc_red_forest_tiles;
            *blocks = pc_red_forest_blocks;
            *tile_count = PC_RED_FOREST_TILE_COUNT;
            *block_count = PC_RED_FOREST_BLOCK_COUNT;
            break;

        case PC_SCENE_TILESET_GATE:
            *tiles = pc_red_gate_tiles;
            *blocks = pc_red_gate_blocks;
            *tile_count = PC_RED_GATE_TILE_COUNT;
            *block_count = PC_RED_GATE_BLOCK_COUNT;
            break;

        case PC_SCENE_TILESET_LAB:
            *tiles = pc_red_lab_tiles;
            *blocks = pc_red_lab_blocks;
            *tile_count = PC_RED_LAB_TILE_COUNT;
            *block_count = PC_RED_LAB_BLOCK_COUNT;
            break;

        case PC_SCENE_TILESET_CAVERN:
            *tiles = pc_red_cavern_tiles;
            *blocks = pc_red_cavern_blocks;
            *tile_count = PC_RED_CAVERN_TILE_COUNT;
            *block_count = PC_RED_CAVERN_BLOCK_COUNT;
            break;

        case PC_SCENE_TILESET_CLUB:
            *tiles = pc_red_club_tiles;
            *blocks = pc_red_club_blocks;
            *tile_count = PC_RED_CLUB_TILE_COUNT;
            *block_count = PC_RED_CLUB_BLOCK_COUNT;
            break;

        case PC_SCENE_TILESET_SHIP:
            *tiles = pc_red_ship_tiles;
            *blocks = pc_red_ship_blocks;
            *tile_count = PC_RED_SHIP_TILE_COUNT;
            *block_count = PC_RED_SHIP_BLOCK_COUNT;
            break;

        case PC_SCENE_TILESET_OVERWORLD:
        default:
            *tiles = pc_red_overworld_tiles;
            *blocks = pc_red_overworld_blocks;
            *tile_count = PC_RED_TILE_COUNT;
            *block_count = PC_RED_BLOCK_COUNT;
            break;
    }
}

static void ensure_tile_cache(const struct pc_world_state *world)
{
    const unsigned char (*tiles)[8][8];
    const unsigned char (*blocks)[4][4];
    int tile_count;
    int block_count;
    int tile_id;
    enum pc_world_time_mode time_mode = current_world_time_mode();
    enum pc_world_weather weather = current_world_weather(world);
    int style_key = ((int)scene_tileset_group(world->scene) << 8) |
                    ((int)time_mode << 4) |
                    (int)weather;

    if (pc_world_cached_style_key == style_key)
        return;

    select_tileset(world->scene, &tiles, &blocks, &tile_count, &block_count);
    for (tile_id = 0; tile_id < tile_count; ++tile_id)
        scale_tile_to_cache(tiles, tile_count, tile_id, time_mode, weather);

    pc_world_cached_style_key = style_key;
    pc_world_cached_tile_count = tile_count;
    (void)blocks;
    (void)block_count;
}

static void draw_cached_tile(unsigned char tile_id, int screen_x, int screen_y)
{
    int src_x = 0;
    int src_y = 0;
    int width = PC_WORLD_SUBTILE_SIZE;
    int height = PC_WORLD_SUBTILE_SIZE;

    if (tile_id >= pc_world_cached_tile_count)
        return;

    if (screen_x < 0)
    {
        src_x = -screen_x;
        width += screen_x;
        screen_x = 0;
    }
    if (screen_y < 0)
    {
        src_y = -screen_y;
        height += screen_y;
        screen_y = 0;
    }
    if (screen_x + width > LCD_WIDTH)
        width = LCD_WIDTH - screen_x;
    if (screen_y + height > LCD_HEIGHT)
        height = LCD_HEIGHT - screen_y;
    if (width <= 0 || height <= 0)
        return;

    rb->lcd_bitmap_part(pc_world_tile_cache[tile_id],
                        src_x, src_y,
                        PC_WORLD_SUBTILE_SIZE,
                        screen_x, screen_y,
                        width, height);
}

static int normalize_tileset_tile_id(unsigned char raw_tile_id, int tile_count)
{
    int tile_id = raw_tile_id;

    /* Interior blocksets use signed BG tile numbering for tiles in the 0x80-0xff range. */
    if (tile_id >= 0x80)
        tile_id -= 0x80;

    if (tile_id < 0 || tile_id >= tile_count || tile_id >= PC_WORLD_TILESET_MAX)
        return -1;

    return tile_id;
}

static void draw_map(const struct pc_world_state *world)
{
    const unsigned char (*tiles)[8][8];
    const unsigned char (*blocks)[4][4];
    int tile_count;
    int block_count;
    int by;
    int bx;
    int start_by;
    int end_by;
    int start_bx;
    int end_bx;
    int sub_y;
    int sub_x;

    ensure_tile_cache(world);
    select_tileset(world->scene, &tiles, &blocks, &tile_count, &block_count);
    (void)tiles;
    (void)tile_count;
    ((struct pc_world_state *)world)->map_dirty = false;

    rb->lcd_set_background(PC_GB_LIGHT);
    rb->lcd_clear_display();

    start_bx = MAX(0, (-world->origin_x) / PC_WORLD_TILE_SIZE);
    start_by = MAX(0, (-world->origin_y) / PC_WORLD_TILE_SIZE);
    end_bx = MIN(world->map_w, ((-world->origin_x) + LCD_WIDTH + PC_WORLD_TILE_SIZE - 1) /
                               PC_WORLD_TILE_SIZE + 1);
    end_by = MIN(world->map_h, ((-world->origin_y) + LCD_HEIGHT + PC_WORLD_TILE_SIZE - 1) /
                               PC_WORLD_TILE_SIZE + 1);

    for (by = start_by; by < end_by; ++by)
    {
        for (bx = start_bx; bx < end_bx; ++bx)
        {
            int block_x = block_screen_x(world, bx);
            int block_y = block_screen_y(world, by);
            unsigned char block_id = world->tiles[by][bx];

            if (block_id >= block_count)
                continue;
            if (block_x >= LCD_WIDTH || block_y >= LCD_HEIGHT ||
                block_x + PC_WORLD_TILE_SIZE <= 0 || block_y + PC_WORLD_TILE_SIZE <= 0)
            {
                continue;
            }

            for (sub_y = 0; sub_y < 4; ++sub_y)
            {
                for (sub_x = 0; sub_x < 4; ++sub_x)
                {
                    int tile_id = normalize_tileset_tile_id(
                        blocks[block_id][sub_y][sub_x], tile_count);

                    if (tile_id < 0)
                        continue;

                    draw_cached_tile((unsigned char)tile_id,
                                     block_x + sub_x * PC_WORLD_SUBTILE_SIZE,
                                     block_y + sub_y * PC_WORLD_SUBTILE_SIZE);
                }
            }
        }
    }
}

static const unsigned char (*select_player_frame(const struct pc_world_state *world,
                                                 bool *flip_x))[16]
{
    bool walking = world->moving && world->walk_frame != 1;

    *flip_x = false;

    switch (world->heading)
    {
        case PC_HEADING_N:
            *flip_x = world->walk_frame == 2;
            return pc_red_player_frames[walking ? 4 : 1];

        case PC_HEADING_E:
            *flip_x = true;
            return pc_red_player_frames[walking ? 5 : 2];

        case PC_HEADING_W:
            return pc_red_player_frames[walking ? 5 : 2];

        case PC_HEADING_S:
        default:
            *flip_x = world->walk_frame == 2;
            return pc_red_player_frames[walking ? 3 : 0];
    }
}

static void draw_red_sprite(const unsigned char frame[16][16],
                            int x, int y, bool flip_x)
{
    int py;
    int px;
    int sy;
    int sx;

    for (py = 0; py < 16; ++py)
    {
        for (px = 0; px < 16; ++px)
        {
            int src_x = flip_x ? (15 - px) : px;
            unsigned char shade = frame[py][src_x];

            if (shade == 3)
                continue;

            rb->lcd_set_foreground(pc_gb_palette[shade]);
            for (sy = 0; sy < PC_WORLD_PIXEL_SCALE; ++sy)
            {
                for (sx = 0; sx < PC_WORLD_PIXEL_SCALE; ++sx)
                {
                    rb->lcd_drawpixel(x + px * PC_WORLD_PIXEL_SCALE + sx,
                                      y + py * PC_WORLD_PIXEL_SCALE + sy);
                }
            }
        }
    }
}

static void draw_red_sprite_scaled(const unsigned char frame[16][16],
                                   int x, int y, int scale, bool flip_x)
{
    int py;
    int px;
    int sy;
    int sx;

    for (py = 0; py < 16; ++py)
    {
        for (px = 0; px < 16; ++px)
        {
            int src_x = flip_x ? (15 - px) : px;
            unsigned char shade = frame[py][src_x];

            if (shade == 3)
                continue;

            rb->lcd_set_foreground(pc_gb_palette[shade]);
            for (sy = 0; sy < scale; ++sy)
            {
                for (sx = 0; sx < scale; ++sx)
                {
                    rb->lcd_drawpixel(x + px * scale + sx,
                                      y + py * scale + sy);
                }
            }
        }
    }
}

static void draw_player(const struct pc_world_state *world)
{
    const struct pc_asset_bitmap *trainer =
        &world->assets.trainer[world->heading][world->walk_frame];
    const unsigned char (*frame)[16];
    bool flip_x;
    int screen_x = world->origin_x + world->player_x;
    int screen_y = world->origin_y + world->player_y;
    int x = screen_x - PC_WORLD_SCALE(8);
    int y = screen_y - PC_WORLD_SCALE(9);

    fill_capsule(screen_x - PC_WORLD_SCALE(7),
                 screen_y + PC_WORLD_FOOT_Y_OFFSET,
                 PC_WORLD_SCALE(14),
                 PC_WORLD_SCALE(3),
                 LCD_RGBPACK(0x60, 0x70, 0x58));
    if (trainer->loaded)
    {
        draw_asset_bitmap(&trainer->bmp,
                          screen_x - trainer->bmp.width / 2,
                          screen_y - trainer->bmp.height + PC_WORLD_SCALE(10));
        return;
    }

    frame = select_player_frame(world, &flip_x);
    draw_red_sprite(frame, x, y, flip_x);
}

static void draw_static_red_npc(const struct pc_world_state *world,
                                int metatile_x, int metatile_y,
                                enum pc_heading heading)
{
    const struct pc_asset_bitmap *trainer = &world->assets.trainer[heading][1];
    const unsigned char (*frame)[16];
    bool flip_x;
    int step = PC_WORLD_TILE_SIZE / 2;
    int screen_x = world->origin_x + metatile_x * step + step / 2;
    int screen_y = world->origin_y + metatile_y * step + step / 2
                   - PC_WORLD_FOOT_Y_OFFSET;

    if (screen_x < -PC_WORLD_SCALE(16) || screen_x > LCD_WIDTH + PC_WORLD_SCALE(16) ||
        screen_y < -PC_WORLD_SCALE(20) || screen_y > LCD_HEIGHT + PC_WORLD_SCALE(12))
    {
        return;
    }

    switch (heading)
    {
        case PC_HEADING_N:
            flip_x = false;
            frame = pc_red_player_frames[1];
            break;

        case PC_HEADING_E:
            flip_x = true;
            frame = pc_red_player_frames[2];
            break;

        case PC_HEADING_W:
            flip_x = false;
            frame = pc_red_player_frames[2];
            break;

        case PC_HEADING_S:
        default:
            flip_x = false;
            frame = pc_red_player_frames[0];
            break;
    }

    fill_capsule(screen_x - PC_WORLD_SCALE(7),
                 screen_y + PC_WORLD_FOOT_Y_OFFSET,
                 PC_WORLD_SCALE(14),
                 PC_WORLD_SCALE(3),
                 LCD_RGBPACK(0x60, 0x70, 0x58));
    if (trainer->loaded)
    {
        draw_asset_bitmap(&trainer->bmp,
                          screen_x - trainer->bmp.width / 2,
                          screen_y - trainer->bmp.height + PC_WORLD_SCALE(10));
        return;
    }

    draw_red_sprite(frame,
                    screen_x - PC_WORLD_SCALE(8),
                    screen_y - PC_WORLD_SCALE(9),
                    flip_x);
}

static void draw_static_named_trainer_npc(const struct pc_world_state *world,
                                          int trainer_index,
                                          int metatile_x, int metatile_y,
                                          enum pc_heading heading)
{
    const struct pc_asset_bitmap *trainer;
    int step = PC_WORLD_TILE_SIZE / 2;
    int screen_x = world->origin_x + metatile_x * step + step / 2;
    int screen_y = world->origin_y + metatile_y * step + step / 2
                   - PC_WORLD_FOOT_Y_OFFSET;

    if (screen_x < -PC_WORLD_SCALE(16) || screen_x > LCD_WIDTH + PC_WORLD_SCALE(16) ||
        screen_y < -PC_WORLD_SCALE(20) || screen_y > LCD_HEIGHT + PC_WORLD_SCALE(12))
    {
        return;
    }

    trainer = ensure_npc_trainer_asset(trainer_index, heading, 1);
    if (trainer == NULL || !trainer->loaded)
    {
        draw_static_red_npc(world, metatile_x, metatile_y, heading);
        return;
    }

    fill_capsule(screen_x - PC_WORLD_SCALE(7),
                 screen_y + PC_WORLD_FOOT_Y_OFFSET,
                 PC_WORLD_SCALE(14),
                 PC_WORLD_SCALE(3),
                 LCD_RGBPACK(0x60, 0x70, 0x58));
    draw_asset_bitmap(&trainer->bmp,
                      screen_x - trainer->bmp.width / 2,
                      screen_y - trainer->bmp.height + PC_WORLD_SCALE(10));
}

static const char *npc_trainer_name(int trainer_index)
{
    static const char *const names[PC_WORLD_NPC_TRAINERS] = {
        "ethan", "lyra", "leaf"
    };

    if (trainer_index < 0 || trainer_index >= PC_WORLD_NPC_TRAINERS)
        return names[0];
    return names[trainer_index];
}

static const struct pc_asset_bitmap *ensure_npc_trainer_asset(int trainer_index,
                                                              int heading, int frame)
{
    struct pc_asset_bitmap *asset;

    if (trainer_index < 0 || trainer_index >= PC_WORLD_NPC_TRAINERS ||
        heading < 0 || heading >= 4 ||
        frame < 0 || frame >= PC_WORLD_WALK_FRAMES)
    {
        return NULL;
    }

    asset = &pc_world_npc_assets[trainer_index][heading][frame];
    if (asset->loaded)
        return asset;

    rb->memset(asset, 0, sizeof(*asset));
    asset->pixels = pc_world_npc_pixels[trainer_index][heading][frame];
    asset->capacity = PC_WORLD_TRAINER_BYTES;
    if (!pc_assets_load_named_world_trainer(asset, npc_trainer_name(trainer_index),
                                            heading, frame))
    {
        rb->memset(asset, 0, sizeof(*asset));
        return NULL;
    }

    return asset;
}

static void draw_scene_npcs(const struct pc_world_state *world)
{
    if (world->scene == PC_WORLD_SCENE_PALLET)
    {
        draw_static_named_trainer_npc(world, 1, 3, 8, PC_HEADING_S);
        draw_static_red_npc(world, 8, 5, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 11, 14, PC_HEADING_W);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_VIRIDIAN_SOUTH)
    {
        draw_static_named_trainer_npc(world, 0, 13, 20, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 30, 8, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 0, 30, 25, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 1, 17, 9, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 18, 9, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 0, 6, 23, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 0, 17, 5, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_VIRIDIAN_FOREST)
    {
        draw_static_named_trainer_npc(world, 0, 16, 43, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 30, 33, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 0, 30, 19, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 0, 2, 18, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 1, 27, 40, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_PEWTER)
    {
        draw_static_named_trainer_npc(world, 1, 8, 15, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 17, 25, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 2, 27, 17, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 2, 26, 25, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 0, 35, 16, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_ROUTE3)
    {
        draw_static_named_trainer_npc(world, 0, 10, 6, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 0, 14, 4, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 1, 16, 9, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 0, 19, 5, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 1, 23, 4, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 0, 22, 9, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 0, 24, 6, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 1, 33, 10, PC_HEADING_N);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_ROUTE4)
    {
        draw_static_named_trainer_npc(world, 1, 9, 8, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 1, 63, 3, PC_HEADING_E);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_ROUTE22)
    {
        draw_static_named_trainer_npc(world, 0, 2, 5, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 1, 13, 5, PC_HEADING_W);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_CERULEAN)
    {
        draw_static_named_trainer_npc(world, 0, 31, 20, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 15, 18, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 9, 21, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 2, 28, 12, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 1, 29, 26, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 1, 9, 27, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 0, 4, 12, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_ROUTE24)
    {
        draw_static_named_trainer_npc(world, 0, 11, 15, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 0, 5, 20, PC_HEADING_N);
        draw_static_named_trainer_npc(world, 0, 11, 19, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 1, 10, 22, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 0, 11, 25, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 1, 10, 28, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 0, 11, 31, PC_HEADING_W);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_ROUTE25)
    {
        draw_static_named_trainer_npc(world, 0, 14, 2, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 18, 5, PC_HEADING_N);
        draw_static_named_trainer_npc(world, 0, 24, 4, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 1, 18, 8, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 0, 32, 3, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 1, 37, 4, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 2, 8, 4, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 2, 23, 9, PC_HEADING_N);
        draw_static_named_trainer_npc(world, 2, 13, 7, PC_HEADING_E);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_OAKS_LAB)
    {
        draw_static_red_npc(world, 4, 3, PC_HEADING_E);
        draw_static_red_npc(world, 6, 3, PC_HEADING_W);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_BLUES_HOUSE)
    {
        draw_static_named_trainer_npc(world, 2, 2, 3, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 1, 6, 4, PC_HEADING_W);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_VIRIDIAN_MART)
    {
        draw_static_named_trainer_npc(world, 0, 0, 5, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 2, 3, 3, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 5, 5, PC_HEADING_N);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_PEWTER_MART)
    {
        draw_static_named_trainer_npc(world, 0, 0, 5, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 1, 3, 3, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 2, 5, 5, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_VIRIDIAN_POKECENTER)
    {
        draw_static_named_trainer_npc(world, 1, 3, 1, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 10, 5, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 0, 4, 3, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 1, 11, 2, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_PEWTER_POKECENTER)
    {
        draw_static_named_trainer_npc(world, 1, 3, 1, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 11, 7, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 2, 1, 3, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 11, 2, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_MUSEUM_1F)
    {
        draw_static_named_trainer_npc(world, 0, 12, 4, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 2, 1, 4, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 15, 2, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 17, 4, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_MUSEUM_2F)
    {
        draw_static_named_trainer_npc(world, 0, 1, 7, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 0, 0, 5, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 7, 5, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 1, 11, 5, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 2, 12, 5, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_VIRIDIAN_SCHOOL_HOUSE)
    {
        draw_static_named_trainer_npc(world, 1, 3, 5, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 2, 4, 1, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_VIRIDIAN_NICKNAME_HOUSE)
    {
        draw_static_named_trainer_npc(world, 0, 5, 3, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 1, 1, 4, PC_HEADING_E);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_ROUTE2_TRADE_HOUSE)
    {
        draw_static_named_trainer_npc(world, 0, 2, 4, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 1, 4, 1, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_ROUTE2_GATE)
    {
        draw_static_named_trainer_npc(world, 0, 1, 4, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 1, 5, 4, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_VIRIDIAN_FOREST_SOUTH_GATE)
    {
        draw_static_named_trainer_npc(world, 1, 8, 4, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 2, 2, 4, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_VIRIDIAN_FOREST_NORTH_GATE)
    {
        draw_static_named_trainer_npc(world, 2, 3, 2, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 2, 5, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_PEWTER_NIDORAN_HOUSE)
    {
        draw_static_named_trainer_npc(world, 1, 3, 5, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 0, 1, 2, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_PEWTER_SPEECH_HOUSE)
    {
        draw_static_named_trainer_npc(world, 2, 2, 3, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 1, 4, 5, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_VIRIDIAN_GYM)
    {
        draw_static_named_trainer_npc(world, 0, 2, 1, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 12, 7, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 0, 11, 11, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 10, 7, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 0, 3, 7, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 13, 5, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 10, 1, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 2, 16, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 0, 6, 5, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 16, 15, PC_HEADING_E);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_PEWTER_GYM)
    {
        draw_static_named_trainer_npc(world, 2, 4, 1, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 1, 3, 6, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 0, 7, 10, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_MT_MOON_POKECENTER)
    {
        draw_static_named_trainer_npc(world, 1, 3, 1, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 4, 3, PC_HEADING_N);
        draw_static_named_trainer_npc(world, 0, 7, 3, PC_HEADING_N);
        draw_static_named_trainer_npc(world, 0, 10, 6, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 1, 11, 2, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_MT_MOON_1F)
    {
        draw_static_named_trainer_npc(world, 0, 5, 6, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 12, 16, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 1, 30, 4, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 2, 24, 31, PC_HEADING_N);
        draw_static_named_trainer_npc(world, 1, 16, 23, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 7, 22, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 30, 27, PC_HEADING_E);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_MT_MOON_B2F)
    {
        draw_static_named_trainer_npc(world, 2, 12, 8, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 0, 11, 16, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 15, 22, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 29, 11, PC_HEADING_N);
        draw_static_named_trainer_npc(world, 0, 29, 17, PC_HEADING_W);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_CERULEAN_POKECENTER)
    {
        draw_static_named_trainer_npc(world, 1, 3, 1, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 10, 5, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 4, 3, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 1, 11, 2, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_CERULEAN_MART)
    {
        draw_static_named_trainer_npc(world, 0, 0, 5, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 0, 3, 4, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 1, 6, 2, PC_HEADING_W);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_CERULEAN_TRADE_HOUSE)
    {
        draw_static_named_trainer_npc(world, 1, 5, 4, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 2, 1, 2, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_CERULEAN_TRASHED_HOUSE)
    {
        draw_static_named_trainer_npc(world, 0, 2, 1, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 1, 5, 6, PC_HEADING_W);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_CERULEAN_BADGE_HOUSE)
    {
        draw_static_named_trainer_npc(world, 0, 5, 3, PC_HEADING_E);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_BILLS_HOUSE)
    {
        draw_static_named_trainer_npc(world, 0, 4, 4, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_BIKE_SHOP)
    {
        draw_static_named_trainer_npc(world, 0, 6, 2, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 1, 5, 6, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 1, 3, PC_HEADING_N);
    }
}

static void draw_banner(const struct pc_world_state *world)
{
    char line1[PC_BANNER_LINE_CHARS];
    char line2[PC_BANNER_LINE_CHARS];
    const struct pc_asset_bitmap *trainer;
    int x = 8;
    int y = 6;
    int w = 154;
    const unsigned char (*frame)[16];
    bool flip_x;

    fit_text_small(line1, sizeof(line1), world->banner.line1, w - 52);
    fit_text_small(line2, sizeof(line2), world->banner.line2, w - 52);
    draw_flat_panel(x, y, w, 24, PC_GB_PANEL, PC_GB_DEEP, PC_GB_MID);

    trainer = &world->assets.trainer[world->heading][world->walk_frame];
    if (trainer->loaded)
    {
        draw_asset_bitmap(&trainer->bmp,
                          x + 17 - trainer->bmp.width / 2,
                          y + 20 - trainer->bmp.height);
    }
    else
    {
        frame = select_player_frame(world, &flip_x);
        draw_red_sprite_scaled(frame, x + 8, y + 4, 1, flip_x);
    }

    draw_text_small(x + 30, y + 5, PC_GB_DEEP, line1);
    if (line2[0] != '\0')
        draw_text_small(x + 30, y + 13, PC_GB_DARK, line2);
}

static void draw_hud(const struct pc_world_state *world)
{
    const struct pc_asset_bitmap *ball = ensure_ui_ball_asset();
    const struct pc_asset_bitmap *buddy_asset = NULL;
    const struct pc_creature_def *buddy_creature = NULL;
    const struct pc_asset_bitmap *weather_icon;
    enum pc_world_time_mode time_mode = current_world_time_mode();
    enum pc_world_weather weather = current_world_weather(world);
    char money[24];
    char balls[24];
    char weather_text[32];
    char nearby_text[48];
    char hint[48];
    char nearby_fitted[48];
    int nearby_count = pc_world_nearby_count(world);
    int nearby_index = nearby_count > 0 ? (world->frame / (PC_FRAME_HZ * 2)) % nearby_count : 0;
    int money_x = LCD_WIDTH - 80;
    int balls_x = LCD_WIDTH - 148;
    int weather_x = LCD_WIDTH - 148;
    int bottom_x = 12;
    int bottom_y = LCD_HEIGHT - 22;
    int bottom_w = LCD_WIDTH - 24;

    if (world->buddy_species >= 0 && world->buddy_species < PC_POKEDEX_MAX &&
        world->caught_counts[world->buddy_species] > 0)
    {
        buddy_creature = pc_assets_get_creature(world->buddy_species);
        buddy_asset = ensure_bag_row_asset(0, world->buddy_species);
    }

    rb->snprintf(money, sizeof(money), "$%u", world->money);
    rb->snprintf(balls, sizeof(balls), "%u", world->pokeballs);
    rb->snprintf(weather_text, sizeof(weather_text), "%s %s",
                 world_time_label(time_mode), world_weather_label(weather));
    weather_icon = ensure_weather_icon_asset(weather);

    if (outdoor_scene(world->scene))
        rb->snprintf(hint, sizeof(hint), "Select stop/item  Hold Select menu");
    else if (world->scene == PC_WORLD_SCENE_VIRIDIAN_MART ||
             world->scene == PC_WORLD_SCENE_PEWTER_MART ||
             world->scene == PC_WORLD_SCENE_CERULEAN_MART)
        rb->snprintf(hint, sizeof(hint), "Press Select to talk");
    else
        rb->snprintf(hint, sizeof(hint), "Hold Select for menu");

    fit_text_small(hint, sizeof(hint), hint, LCD_WIDTH - 64);
    if (nearby_count > 0 && pc_world_nearby_name(world, nearby_index, nearby_text, sizeof(nearby_text)))
    {
        char species[32];

        fit_text_small(species, sizeof(species), nearby_text, 88);
        rb->snprintf(nearby_text, sizeof(nearby_text), "Nearby %s", species);
    }
    else if (outdoor_scene(world->scene))
        rb->snprintf(nearby_text, sizeof(nearby_text), "Nearby calm");
    else
        nearby_text[0] = '\0';

    draw_flat_panel(balls_x, 8, 62, 18, PC_GB_PANEL, PC_GB_DEEP, LCD_RGBPACK(0xb8, 0xe9, 0xff));
    if (ball != NULL)
        draw_asset_bitmap(&ball->bmp, balls_x + 4, 10);
    else
        draw_missing_asset_panel(balls_x + 4, 10, 14, 14, "");
    draw_text_small(ball != NULL ? balls_x + 22 : balls_x + 10, 13, PC_GB_DEEP, balls);

    draw_flat_panel(money_x, 8, 72, 18, PC_GB_PANEL, PC_GB_DEEP, LCD_RGBPACK(0xc8, 0xef, 0xc0));
    draw_text_small(money_x + 10, 13, PC_GB_DEEP, money);

    if (outdoor_scene(world->scene))
    {
        fit_text_small(weather_text, sizeof(weather_text), weather_text,
                       weather_icon != NULL ? 88 : 110);
        draw_flat_panel(weather_x, 30, 138, 16, PC_GB_PANEL, PC_GB_DEEP, LCD_RGBPACK(0xdf, 0xf4, 0xff));
        if (weather_icon != NULL)
            draw_asset_bitmap(&weather_icon->bmp, weather_x + 6, 29);
        draw_text_small(weather_icon != NULL ? weather_x + 28 : weather_x + 10,
                        35, PC_GB_DEEP, weather_text);
    }

    if (buddy_creature != NULL)
    {
        char buddy_text[40];
        char buddy_name[32];

        draw_flat_panel(8, 32, 122, 18, PC_GB_PANEL, PC_GB_DEEP, LCD_RGBPACK(0xf2, 0xec, 0xd8));
        if (buddy_asset != NULL && buddy_asset->loaded)
            draw_asset_bitmap(&buddy_asset->bmp, 12, 30);
        fit_text_small(buddy_name, sizeof(buddy_name), buddy_creature->name, 58);
        draw_text_small(36, 36, PC_GB_DEEP, buddy_name);
        rb->snprintf(buddy_text, sizeof(buddy_text), "%d/%d", world->buddy_steps,
                     PC_BUDDY_CANDY_STEPS);
        draw_text_small(36, 44, PC_GB_DARK, buddy_text);
    }

    if (nearby_text[0] != '\0')
    {
        int nearby_w = MIN(120, MAX(94, LCD_WIDTH / 3));

        bottom_y = LCD_HEIGHT - 38;
        draw_flat_panel(bottom_x, bottom_y, bottom_w, 16,
                        PC_GB_PANEL, PC_GB_DEEP, LCD_RGBPACK(0xd7, 0xf7, 0xff));
        fit_text_small(nearby_fitted, sizeof(nearby_fitted), nearby_text, nearby_w - 18);
        fill_capsule(bottom_x + 6, bottom_y + 2, nearby_w, 12, LCD_RGBPACK(0xf0, 0xf7, 0xe0));
        draw_text_small(bottom_x + 14, bottom_y + 6, PC_GB_DARK, nearby_fitted);
        rb->lcd_set_foreground(PC_GB_MID);
        rb->lcd_drawline(bottom_x + nearby_w + 12, bottom_y + 3,
                         bottom_x + nearby_w + 12, bottom_y + 12);
        fit_text_small(hint, sizeof(hint), hint, bottom_w - nearby_w - 26);
        draw_text_small(bottom_x + nearby_w + 20, bottom_y + 6, PC_GB_DARK, hint);
    }
    else
    {
        draw_flat_panel(bottom_x, bottom_y, bottom_w, 16,
                        PC_GB_PANEL, PC_GB_DEEP, LCD_RGBPACK(0xd7, 0xf7, 0xff));
        draw_text_small(bottom_x + 10, bottom_y + 6, PC_GB_DARK, hint);
    }
}

static void fill_rect_outline(int x, int y, int w, int h,
                              fb_data fill, fb_data outline)
{
    rb->lcd_set_foreground(fill);
    rb->lcd_fillrect(x, y, w, h);
    rb->lcd_set_foreground(outline);
    rb->lcd_drawrect(x, y, w, h);
}

static int count_caught_species(const struct pc_world_state *world)
{
    int i;
    int total = 0;

    for (i = 0; i < PC_POKEDEX_MAX; ++i)
    {
        if (world->caught_counts[i] > 0)
            total++;
    }
    return total;
}

static void draw_notice(const struct pc_world_state *world)
{
    char line1[PC_BANNER_LINE_CHARS];
    char line2[PC_BANNER_LINE_CHARS];
    int w = LCD_WIDTH - 56;
    int h = world->detail.line2[0] != '\0' ? 26 : 16;
    int x = 28;
    int y = 34;

    if (world->notice_frames <= 0 || world->detail.line1[0] == '\0')
        return;

    fit_text_small(line1, sizeof(line1), world->detail.line1, w - 20);
    fit_text_small(line2, sizeof(line2), world->detail.line2, w - 20);

    if (!rb->strcmp(world->detail.line1, "Song Unlocked"))
    {
        const struct pc_asset_bitmap *ball = ensure_ui_ball_asset();
        int icon_x = x + 18;
        int icon_y = y + 10;

        fit_text_small(line2, sizeof(line2), world->detail.line2, w - 68);
        draw_flat_panel(x, y, w, 28, PC_GB_PANEL, PC_GB_DEEP, PC_GB_MID);
        if (ball != NULL)
            draw_asset_bitmap(&ball->bmp, x + 8, y + 6);
        else
            draw_missing_asset_panel(icon_x - 6, icon_y - 4, 12, 12, "");
        draw_text_small(x + 30, y + 4, PC_GB_DEEP, "POKEGEAR RADIO");
        rb->lcd_set_foreground(PC_GB_MID);
        xlcd_fillcircle(x + 20, y + 19, 5);
        rb->lcd_set_foreground(PC_GB_PANEL);
        xlcd_fillcircle(x + 20, y + 19, 2);
        rb->lcd_set_foreground(PC_GB_DEEP);
        rb->lcd_drawline(x + 16, y + 15, x + 24, y + 23);
        draw_text_small(x + 30, y + 14, PC_GB_DARK, line2);
        return;
    }

    draw_flat_panel(x, y, w, h, PC_GB_PANEL, PC_GB_DEEP, PC_GB_MID);
    draw_text_small(x + 10, y + 4, PC_GB_DEEP, line1);
    if (world->detail.line2[0] != '\0')
        draw_text_small(x + 10, y + 12, PC_GB_DARK, line2);
}

static const struct pc_asset_bitmap *ensure_bag_row_asset(int slot, int species_index)
{
    struct pc_asset_bitmap *asset;

    if (slot < 0 || slot >= PC_BAG_ROWS || species_index < 0)
        return NULL;
    if (pc_bag_row_species[slot] == species_index && pc_bag_row_assets[slot].loaded)
        return &pc_bag_row_assets[slot];

    asset = &pc_bag_row_assets[slot];
    init_asset_bitmap(asset, pc_bag_row_pixels[slot], sizeof(pc_bag_row_pixels[slot]));
    if (!pc_assets_load_world_creature(asset, species_index))
    {
        pc_bag_row_species[slot] = -1;
        rb->memset(asset, 0, sizeof(*asset));
        return NULL;
    }

    pc_bag_row_species[slot] = species_index;
    return asset;
}

static void draw_menu_overlay(const struct pc_world_state *world)
{
    static const char *const items[] = {
        "Resume", "Songs", "Buddy", "Field Moves",
        "Backpack", "Pokedex", "Save Game", "Quit"
    };
    int x = 28;
    int y = 12;
    int w = LCD_WIDTH - 56;
    int h = 176;
    int i;

    fill_rect_outline(x, y, w, h, PC_GB_PANEL, PC_GB_DEEP);
    draw_text_small(x + 44, y + 10, PC_GB_DEEP, "PAUSE");
    for (i = 0; i < 8; ++i)
    {
        int row_y = y + 28 + i * 16;

        if (i == world->menu_index)
            fill_rect_outline(x + 10, row_y - 2, w - 20, 14, PC_GB_MID, PC_GB_DEEP);
        draw_text_small(x + 18, row_y, PC_GB_DEEP, items[i]);
    }
    if (world->detail.line2[0] != '\0')
        draw_text_small(x + 10, y + 156, PC_GB_DEEP, world->detail.line2);
}

static const char *field_ability_label(enum pc_field_ability ability)
{
    switch (ability)
    {
        case PC_FIELD_ABILITY_SURF:
            return "HM03 Surf";

        case PC_FIELD_ABILITY_CUT:
            return "HM01 Cut";

        default:
            return "Field Move";
    }
}

static void draw_bag_overlay(const struct pc_world_state *world)
{
    int x = 22;
    int y = 24;
    int shown = 0;
    int i;
    char count_text[16];

    fill_rect_outline(x, y, LCD_WIDTH - 44, LCD_HEIGHT - 48, PC_GB_PANEL, PC_GB_DEEP);
    draw_text_small(x + 10, y + 8, PC_GB_DEEP, "BACKPACK");
    if (count_caught_species(world) == 0)
    {
        draw_text_small(x + 10, y + 28, PC_GB_DEEP, "No Pokemon caught yet");
        return;
    }

    for (i = 0; i < PC_POKEDEX_MAX && shown < 5; ++i)
    {
        const struct pc_creature_def *creature;
        const struct pc_asset_bitmap *asset;
        int row_y;

        if (world->caught_counts[i] == 0)
            continue;

        creature = pc_assets_get_creature(i);
        if (creature == NULL)
            continue;

        row_y = y + 26 + shown * PC_BAG_ROW_H;
        if (i == world->bag_index)
            fill_rect_outline(x + 8, row_y - 2, LCD_WIDTH - 60, 16, PC_GB_MID, PC_GB_DEEP);
        asset = ensure_bag_row_asset(shown, i);
        if (asset != NULL && asset->loaded)
            draw_asset_bitmap_fit(&asset->bmp, x + 10, row_y - 1,
                                  PC_BAG_ICON_SIZE, PC_BAG_ICON_SIZE);
        else
            draw_missing_asset_panel(x + 10, row_y - 1, PC_BAG_ICON_SIZE, PC_BAG_ICON_SIZE, "");
        draw_text_small(x + 34, row_y, PC_GB_DEEP, creature->name);
        if (i == world->buddy_species)
            draw_text_small(LCD_WIDTH - 92, row_y, PC_GB_DARK, "BUDDY");
        rb->snprintf(count_text, sizeof(count_text), "x%u", world->caught_counts[i]);
        draw_text_small(LCD_WIDTH - 58, row_y, PC_GB_DEEP, count_text);
        shown++;
    }
    draw_text_small(x + 10, y + 120, PC_GB_DEEP, "Menu/Play scroll");
    draw_text_small(x + 10, y + 134, PC_GB_DEEP, "Left back");
}

static void draw_pokedex_overlay(const struct pc_world_state *world)
{
    const struct pc_creature_def *creature = pc_assets_get_creature(world->dex_index);
    int family = pc_assets_get_family_index(world->dex_index);
    int evolve_target = pc_assets_get_evolution_target(world->dex_index);
    int evolve_cost = pc_assets_get_evolution_cost(world->dex_index);
    unsigned candy = 0;
    int x = 18;
    int y = 18;
    char line[PC_BANNER_LINE_CHARS];

    fill_rect_outline(x, y, LCD_WIDTH - 36, LCD_HEIGHT - 36, PC_GB_DEEP, PC_GB_MID);
    fill_rect_outline(x + 8, y + 8, LCD_WIDTH - 52, LCD_HEIGHT - 52, PC_GB_LIGHT, PC_GB_DEEP);
    fill_rect_outline(x + 14, y + 18, 86, 86, PC_GB_PANEL, PC_GB_DEEP);
    fill_rect_outline(x + 108, y + 18, 84, 86, PC_GB_PANEL, PC_GB_DEEP);
    draw_text_small(x + 120, y + 26, PC_GB_DEEP, "POKEDEX");
    if (creature == NULL)
        return;

    if (world->assets.dex_creature.loaded)
    {
        draw_asset_bitmap(&world->assets.dex_creature.bmp, x + 22, y + 28);
    }
    else
    {
        draw_missing_asset_panel(x + 18, y + 24, 78, 78, "NO SPRITE");
    }

    rb->snprintf(line, sizeof(line), "#%03d", creature->species_id);
    draw_text_small(x + 120, y + 44, PC_GB_DEEP, line);
    fit_text_small(line, sizeof(line), creature->name, 70);
    draw_text_small(x + 120, y + 58, PC_GB_DEEP, line);
    if (world->dex_index < PC_POKEDEX_MAX && world->caught_counts[world->dex_index] > 0)
        rb->snprintf(line, sizeof(line), "Caught %u", world->caught_counts[world->dex_index]);
    else
        rb->snprintf(line, sizeof(line), "Uncaught");
    fit_text_small(line, sizeof(line), line, 70);
    draw_text_small(x + 120, y + 74, PC_GB_DEEP, line);
    if (family >= 0 && family < PC_POKEDEX_MAX)
        candy = world->family_candy[family];
    rb->snprintf(line, sizeof(line), "Candy %u", candy);
    fit_text_small(line, sizeof(line), line, 70);
    draw_text_small(x + 120, y + 88, PC_GB_DEEP, line);
    if (evolve_target >= 0 && evolve_cost > 0)
    {
        const struct pc_creature_def *next = pc_assets_get_creature(evolve_target);

        if (next != NULL)
            rb->snprintf(line, sizeof(line), "Evolve %s %d", next->name, evolve_cost);
        else
            rb->snprintf(line, sizeof(line), "Evolve %d candy", evolve_cost);
    }
    else
    {
        rb->snprintf(line, sizeof(line), "No evolve");
    }
    fit_text_small(line, sizeof(line), line, 70);
    draw_text_small(x + 120, y + 100, PC_GB_DEEP, line);
    draw_text_small(x + 22, y + 118, PC_GB_DEEP, "Menu/Play or Left/Right scroll");
    draw_text_small(x + 22, y + 132, PC_GB_DEEP, "Select evolve  Left back");
}

static const char *mart_category_label(int category)
{
    static const char *const labels[PC_MART_CATEGORY_COUNT] = {
        "ITEMS", "HMS", "LOOKS"
    };

    if (category < 0 || category >= PC_MART_CATEGORY_COUNT)
        return labels[0];
    return labels[category];
}

static int mart_category_item_count_for_draw(int category)
{
    switch (category)
    {
        case 0:
            return 1;

        case 1:
            return PC_FIELD_ABILITY_COUNT;

        case 2:
            return pc_world_player_trainer_count();

        default:
            return 1;
    }
}

static void draw_mart_overlay(const struct pc_world_state *world)
{
    int x = 20;
    int y = 18;
    int w = LCD_WIDTH - 40;
    int h = LCD_HEIGHT - 36;
    int tab_w = (w - 24) / PC_MART_CATEGORY_COUNT;
    int count = mart_category_item_count_for_draw(world->mart_category);
    int visible_rows = 5;
    int start_index = 0;
    int end_index;
    int i;
    char line[PC_BANNER_LINE_CHARS];

    if (count > visible_rows)
    {
        start_index = world->mart_index - visible_rows / 2;
        if (start_index < 0)
            start_index = 0;
        if (start_index > count - visible_rows)
            start_index = count - visible_rows;
    }
    end_index = MIN(count, start_index + visible_rows);

    fill_rect_outline(x, y, w, h, PC_GB_PANEL, PC_GB_DEEP);
    draw_text_small(x + 10, y + 8, PC_GB_DEEP, "POKEMART");
    rb->snprintf(line, sizeof(line), "$%u  Balls %u/%u", world->money,
                 world->pokeballs, (unsigned)1000);
    draw_text_small(x + 10, y + 22, PC_GB_DEEP, line);

    for (i = 0; i < PC_MART_CATEGORY_COUNT; ++i)
    {
        int tab_x = x + 8 + i * tab_w;

        if (i == world->mart_category)
            fill_rect_outline(tab_x, y + 38, tab_w - 4, 16, PC_GB_MID, PC_GB_DEEP);
        draw_text_small(tab_x + 8, y + 42, PC_GB_DEEP, mart_category_label(i));
    }

    for (i = start_index; i < end_index; ++i)
    {
        int row_y = y + 64 + (i - start_index) * 16;
        const char *name = "";
        const char *tag = "";

        if (i == world->mart_index)
            fill_rect_outline(x + 8, row_y - 2, w - 16, 14, PC_GB_MID, PC_GB_DEEP);

        if (world->mart_category == 0)
        {
            name = "Poke Ball x10";
            tag = "$90";
        }
        else if (world->mart_category == 1)
        {
            if (i == 0)
            {
                name = "HM03 Surf";
                tag = world->ability_owned[PC_FIELD_ABILITY_SURF] ? "OWND" : "$80";
            }
            else
            {
                name = "HM01 Cut";
                tag = world->ability_owned[PC_FIELD_ABILITY_CUT] ? "OWND" : "$60";
            }
        }
        else
        {
            name = pc_world_player_trainer_name(i);
            if (!pc_world_player_trainer_assets_available(i))
                tag = "MISS";
            else if (world->player_trainer == i)
                tag = "ON";
            else if (world->owned_trainers[i])
                tag = "USE";
            else
            {
                rb->snprintf(line, sizeof(line), "$%d", pc_world_player_trainer_cost(i));
                tag = line;
            }
        }

        draw_text_small(x + 14, row_y, PC_GB_DEEP, name);
        draw_text_small(x + w - 54, row_y, PC_GB_DEEP, tag);
    }

    if (count > visible_rows)
    {
        if (start_index > 0)
            draw_text_small(x + w - 20, y + 56, PC_GB_DARK, "^");
        if (end_index < count)
            draw_text_small(x + w - 20, y + 132, PC_GB_DARK, "v");
    }

    if (world->mart_category == 0)
        draw_text_small(x + 10, y + 138, PC_GB_DEEP, "Pokeballs cost more but stops pay out");
    else if (world->mart_category == 1)
        draw_text_small(x + 10, y + 138, PC_GB_DEEP, "Buy HMs here, assign them in menu");
    else
        draw_text_small(x + 10, y + 138, PC_GB_DEEP, "Buy or equip trainer looks");
    draw_text_small(x + 10, y + 152, PC_GB_DEEP, "Left/Right tabs  Menu/Play scroll");
    draw_text_small(x + 10, y + 166, PC_GB_DEEP, "Select buy/use  Left back");
}

static void draw_field_moves_overlay(const struct pc_world_state *world)
{
    int x = 32;
    int y = 28;
    int i;
    char line[PC_BANNER_LINE_CHARS];

    fill_rect_outline(x, y, LCD_WIDTH - 64, LCD_HEIGHT - 56, PC_GB_PANEL, PC_GB_DEEP);
    draw_text_small(x + 10, y + 8, PC_GB_DEEP, "FIELD MOVES");

    for (i = 0; i < PC_FIELD_ABILITY_COUNT; ++i)
    {
        const struct pc_creature_def *partner = NULL;
        int assigned = world->ability_species[i];
        int row_y = y + 34 + i * 24;

        if (assigned >= 0 && assigned < PC_POKEDEX_MAX &&
            world->caught_counts[assigned] > 0)
        {
            partner = pc_assets_get_creature(assigned);
        }

        if (i == world->field_index)
            fill_rect_outline(x + 8, row_y - 2, LCD_WIDTH - 80, 18, PC_GB_MID, PC_GB_DEEP);
        draw_text_small(x + 14, row_y, PC_GB_DEEP, field_ability_label(i));
        draw_text_small(LCD_WIDTH - 80, row_y, PC_GB_DEEP,
                        world->ability_owned[i] ? "OWND" : "LOCK");
        if (partner != NULL)
            rb->snprintf(line, sizeof(line), "Partner %s", partner->name);
        else if (world->ability_owned[i])
            rb->snprintf(line, sizeof(line), "Unassigned");
        else
            rb->snprintf(line, sizeof(line), "Buy in mart");
        draw_text_small(x + 24, row_y + 10, PC_GB_DARK, line);
    }

    draw_text_small(x + 10, y + 98, PC_GB_DEEP, "Select assign  Left back");
    draw_text_small(x + 10, y + 112, PC_GB_DEEP, "Locked moves are sold in mart");
}

static void draw_songs_overlay(const struct pc_world_state *world)
{
    int count = pc_world_song_count();
    int x = 24;
    int y = 20;
    int start = 0;
    int shown;
    int i;
    char line[PC_BANNER_LINE_CHARS];

    fill_rect_outline(x, y, LCD_WIDTH - 48, LCD_HEIGHT - 40, PC_GB_PANEL, PC_GB_DEEP);
    draw_text_small(x + 10, y + 8, PC_GB_DEEP, "POKEGEAR SONGS");

    if (count <= 0)
    {
        draw_text_small(x + 10, y + 34, PC_GB_DEEP, "No songs unlocked yet");
        draw_text_small(x + 10, y + 48, PC_GB_DARK, "Find secret Poke Balls");
        draw_text_small(x + 10, y + 122, PC_GB_DEEP, "Left back");
        return;
    }

    start = MAX(0, world->song_index - 2);
    if (start + PC_BAG_ROWS > count)
        start = MAX(0, count - PC_BAG_ROWS);

    for (i = start, shown = 0; i < count && shown < PC_BAG_ROWS; ++i, ++shown)
    {
        int row_y = y + 30 + shown * 18;

        if (i == world->song_index)
            fill_rect_outline(x + 8, row_y - 2, LCD_WIDTH - 64, 14, PC_GB_MID, PC_GB_DEEP);
        if (pc_world_song_name(i, line, sizeof(line)))
            fit_text_small(line, sizeof(line), line, LCD_WIDTH - 84);
        else
            rb->snprintf(line, sizeof(line), "Track %d", i + 1);
        draw_text_small(x + 16, row_y, PC_GB_DEEP, line);
    }

    draw_text_small(x + 10, y + 122, PC_GB_DEEP, "Select play  Left back");
}

static void draw_buddy_overlay(const struct pc_world_state *world)
{
    const struct pc_creature_def *buddy = NULL;
    int x = 20;
    int y = 18;
    int start = MAX(0, world->buddy_index - 2);
    int shown = 0;
    int i;
    char line[PC_BANNER_LINE_CHARS];

    fill_rect_outline(x, y, LCD_WIDTH - 40, LCD_HEIGHT - 36, PC_GB_PANEL, PC_GB_DEEP);
    draw_text_small(x + 10, y + 8, PC_GB_DEEP, "BUDDY");

    if (world->buddy_species >= 0 && world->buddy_species < PC_POKEDEX_MAX &&
        world->caught_counts[world->buddy_species] > 0)
    {
        buddy = pc_assets_get_creature(world->buddy_species);
    }

    fill_rect_outline(x + 8, y + 24, LCD_WIDTH - 56, 28, PC_GB_LIGHT, PC_GB_DEEP);
    if (buddy != NULL)
    {
        fit_text_small(line, sizeof(line), buddy->name, 88);
        draw_text_small(x + 14, y + 32, PC_GB_DEEP, "Current");
        draw_text_small(x + 62, y + 32, PC_GB_DEEP, line);
        rb->snprintf(line, sizeof(line), "%d / %d steps", world->buddy_steps,
                     PC_BUDDY_CANDY_STEPS);
        draw_text_small(x + 14, y + 42, PC_GB_DARK, line);
    }
    else
    {
        draw_text_small(x + 14, y + 32, PC_GB_DEEP, "Current");
        draw_text_small(x + 62, y + 32, PC_GB_DEEP, "None");
        draw_text_small(x + 14, y + 42, PC_GB_DARK, "Assign one to earn candy");
    }

    if (count_caught_species(world) == 0)
    {
        draw_text_small(x + 10, y + 68, PC_GB_DEEP, "No Pokemon caught yet");
        draw_text_small(x + 10, y + 124, PC_GB_DEEP, "Left back");
        return;
    }

    if (start + PC_BAG_ROWS > PC_POKEDEX_MAX)
        start = MAX(0, PC_POKEDEX_MAX - PC_BAG_ROWS);

    for (i = start; i < PC_POKEDEX_MAX && shown < PC_BAG_ROWS; ++i)
    {
        const struct pc_creature_def *creature;
        const struct pc_asset_bitmap *asset;
        int row_y;

        if (world->caught_counts[i] == 0)
            continue;

        creature = pc_assets_get_creature(i);
        if (creature == NULL)
            continue;

        row_y = y + 62 + shown * PC_BAG_ROW_H;
        if (i == world->buddy_index)
            fill_rect_outline(x + 8, row_y - 2, LCD_WIDTH - 56, 16, PC_GB_MID, PC_GB_DEEP);
        asset = ensure_bag_row_asset(shown, i);
        if (asset != NULL && asset->loaded)
            draw_asset_bitmap_fit(&asset->bmp, x + 10, row_y - 1,
                                  PC_BAG_ICON_SIZE, PC_BAG_ICON_SIZE);
        else
            draw_missing_asset_panel(x + 10, row_y - 1, PC_BAG_ICON_SIZE, PC_BAG_ICON_SIZE, "");
        draw_text_small(x + 34, row_y, PC_GB_DEEP, creature->name);
        if (i == world->buddy_species)
            draw_text_small(LCD_WIDTH - 68, row_y, PC_GB_DARK, "ON");
        shown++;
    }

    draw_text_small(x + 10, y + 138, PC_GB_DEEP, "Select assign/toggle  Left back");
}

static void draw_field_assign_overlay(const struct pc_world_state *world)
{
    enum pc_field_ability ability = (enum pc_field_ability)world->field_index;
    int assigned = world->ability_species[ability];
    const struct pc_creature_def *current = NULL;
    const struct pc_creature_def *partner = NULL;
    int x = 40;
    int y = 34;
    char line[PC_BANNER_LINE_CHARS];

    if (world->mart_assign_index >= 0 && world->mart_assign_index < PC_POKEDEX_MAX &&
        world->caught_counts[world->mart_assign_index] > 0)
    {
        current = pc_assets_get_creature(world->mart_assign_index);
    }

    if (assigned >= 0 && assigned < PC_POKEDEX_MAX &&
        world->caught_counts[assigned] > 0)
    {
        partner = pc_assets_get_creature(assigned);
    }

    fill_rect_outline(x, y, LCD_WIDTH - 80, LCD_HEIGHT - 68, PC_GB_PANEL, PC_GB_DEEP);
    fit_text_small(line, sizeof(line), field_ability_label(ability), LCD_WIDTH - 112);
    draw_text_small(x + 10, y + 8, PC_GB_DEEP, "ASSIGN");
    draw_text_small(x + 56, y + 8, PC_GB_DEEP, line);

    if (current == NULL)
    {
        draw_text_small(x + 10, y + 36, PC_GB_DEEP, "No compatible Pokemon");
        draw_text_small(x + 10, y + 104, PC_GB_DEEP, "Left back");
        return;
    }

    fill_rect_outline(x + 10, y + 28, LCD_WIDTH - 100, 38, PC_GB_MID, PC_GB_DEEP);
    draw_text_small(x + 18, y + 38, PC_GB_DEEP, current->name);
    rb->snprintf(line, sizeof(line), "Caught x%u", world->caught_counts[world->mart_assign_index]);
    draw_text_small(x + 18, y + 52, PC_GB_DEEP, line);

    if (partner != NULL)
    {
        rb->snprintf(line, sizeof(line), "Current: %s", partner->name);
        draw_text_small(x + 10, y + 78, PC_GB_DEEP, line);
    }
    else
        draw_text_small(x + 10, y + 78, PC_GB_DEEP, "Current: none");

    draw_text_small(x + 10, y + 104, PC_GB_DEEP, "Menu/Play cycle  Select assign");
    draw_text_small(x + 10, y + 118, PC_GB_DEEP, "Left back");
}

void pc_world_render_frame(const struct pc_world_state *world)
{
    int i;

    draw_map(world);
    draw_weather_overlay(world);
    for (i = 0; i < PC_WORLD_POKESTOP_COUNT; ++i)
        draw_pokestop(world, i);
    for (i = 0; i < PC_WORLD_SECRET_COUNT; ++i)
        draw_secret_item_ball(world, i);
    if (outdoor_scene(world->scene))
    {
        for (i = 0; i < PC_WORLD_MAX_SPAWNS; ++i)
            draw_spawn(world, i);
    }
    draw_scene_npcs(world);
    draw_player(world);
    draw_banner(world);
    if (world->view == PC_WORLD_VIEW_MENU)
        draw_menu_overlay(world);
    else if (world->view == PC_WORLD_VIEW_BAG)
        draw_bag_overlay(world);
    else if (world->view == PC_WORLD_VIEW_POKEDEX)
        draw_pokedex_overlay(world);
    else if (world->view == PC_WORLD_VIEW_MART)
        draw_mart_overlay(world);
    else if (world->view == PC_WORLD_VIEW_SONGS)
        draw_songs_overlay(world);
    else if (world->view == PC_WORLD_VIEW_BUDDY)
        draw_buddy_overlay(world);
    else if (world->view == PC_WORLD_VIEW_FIELD_MOVES)
        draw_field_moves_overlay(world);
    else if (world->view == PC_WORLD_VIEW_FIELD_ASSIGN)
        draw_field_assign_overlay(world);
    else if (world->view == PC_WORLD_VIEW_POKESTOP)
        draw_pokestop_overlay(world);
    else
        draw_hud(world);
    draw_notice(world);
    rb->lcd_update();
}
