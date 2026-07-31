/***************************************************************************
 * Offline World Maps for colour 320x240 Rockbox targets.
 *
 * Satellite frames and Look Around imagery are always real, offline sources.
 * No storage is touched by the render loop and the fixed data below stays in
 * plugin memory.
 ****************************************************************************/

#include "plugin.h"
#include "lib/helper.h"
#include "lib/pluginlib_exit.h"
#include "lib/xlcd.h"

#if defined(HAVE_LCD_COLOR) && (LCD_WIDTH >= 320) && (LCD_HEIGHT >= 240)

#define MAR_WORLD_W 10000
#define MAR_WORLD_H 6600
#define MAR_ZOOM_COUNT 9
#define MAR_MAP_Y 22
#define MAR_SAT_W 320
#define MAR_SAT_H 160
#define MAR_STREET_H 184
#define MAR_GLOBE_FRAMES 64
#define MAR_GLOBE_TURN_FRAMES 3
#define MAR_SAT_BYTES (MAR_SAT_W * MAR_SAT_H * sizeof(fb_data))
#define MAR_SCENE_NONE -1
#define MAR_SCENE_TIMES_SQUARE -2
#define MAR_SCENE_LONDON -3
#define MAR_SCENE_BERLIN -4
#define MAR_SCENE_PARIS -5
#define MAR_SCENE_TORONTO -6
#define MAR_SCENE_TOKYO -7
#define MAR_SCENE_TORONTO_360 -8
#define MAR_SCENE_FREDERICTON -9
#define MAR_WORLD_SAT_PATH ROCKBOX_DIR "/maps/world_satellite.r16"
#define MAR_WORLD_GLOBE_PATH ROCKBOX_DIR "/maps/world_globe_%02d.r16"
#define MAR_DASHCAM_VIDEO_DIR ROCKBOX_DIR "/maps/videos"
#define MAR_VIDEO_PLAYER_PATH ROCKBOX_DIR "/rocks/viewers/mpegplayer.rock"
#define MAR_WORLD_TILE_PATH ROCKBOX_DIR "/maps/world/%d/%d_%d.r16"
#define MAR_WORLD_TILE_HI_PATH ROCKBOX_DIR "/maps/world/%d_hi/%d_%d.r16"
#define MAR_WORLD_TILE_HI2_PATH ROCKBOX_DIR "/maps/world/%d_hi2/%d_%d.r16"
#define MAR_STREET_MONCTON_PATH ROCKBOX_DIR "/maps/moncton_imagery.rgb"
#define MAR_STREET_FREDERICTON_PATH ROCKBOX_DIR "/maps/fredericton_imagery.rgb"
#define MAR_STREET_SAINT_JOHN_PATH ROCKBOX_DIR "/maps/saint_john_imagery.rgb"
#define MAR_TIMES_SQUARE_NORTH_PATH ROCKBOX_DIR "/maps/times_square_north.rgb"
#define MAR_TIMES_SQUARE_EAST_PATH ROCKBOX_DIR "/maps/times_square_east.rgb"
#define MAR_TIMES_SQUARE_SOUTH_PATH ROCKBOX_DIR "/maps/times_square_south.rgb"
#define MAR_TIMES_SQUARE_WEST_PATH ROCKBOX_DIR "/maps/times_square_west.rgb"
#define MAR_LONDON_PATH ROCKBOX_DIR "/maps/london_kartaview.rgb"
#define MAR_BERLIN_PATH ROCKBOX_DIR "/maps/berlin_kartaview.rgb"
#define MAR_PARIS_PATH ROCKBOX_DIR "/maps/paris_panoramax.rgb"
#define MAR_TOKYO_PATH ROCKBOX_DIR "/maps/tokyo_panoramax.rgb"
#define MAR_SYNC_PATH ROCKBOX_DIR "/maps/location.v1.tsv"
#define MAR_PHOTO_THUMB_PATH ROCKBOX_DIR "/maps/photos/photo_%02d.r16"
#define MAR_SYNC_TEXT_SIZE 2048
#define MAR_MAX_SYNC_PHOTOS 12
#define MAR_MAX_ROUTE_POINTS 24
#define MAR_PHOTO_THUMB_W 40
#define MAR_PHOTO_THUMB_H 30
#define MAR_BLUE LCD_RGBPACK(0, 122, 255)
#define MAR_WATER LCD_RGBPACK(191, 224, 239)
#define MAR_LAND LCD_RGBPACK(242, 241, 235)
#define MAR_LAND_TOPO LCD_RGBPACK(221, 232, 207)
#define MAR_COAST LCD_RGBPACK(111, 151, 160)
#define MAR_TEXT LCD_RGBPACK(43, 48, 52)

enum mar_mode {
    MAR_MODE_SATELLITE = 0,
    MAR_MODE_STANDARD,
    MAR_MODE_TERRAIN,
    MAR_MODE_NIGHT,
    MAR_MODE_COUNT
};

enum mar_line_kind {
    MAR_LINE_HIGHWAY,
    MAR_LINE_ROAD,
    MAR_LINE_WATER,
    MAR_LINE_BOUNDARY,
    MAR_LINE_COUNTRY_BORDER
};

enum mar_place_kind {
    MAR_CITY,
    MAR_TOWN,
    MAR_ISLAND,
    MAR_WATER_NAME
};

enum mar_screen {
    MAR_SCREEN_MAP = 0,
    MAR_SCREEN_EXPLORE,
    MAR_SCREEN_WORLD,
    MAR_SCREEN_STREET
};

struct mar_view {
    int center_x;
    int center_y;
    int zoom;
    enum mar_mode mode;
    bool labels;
    int selected_place;
    int browse_index;
    int street_heading;
    int street_scene;
    enum mar_screen screen;
};

struct mar_shape {
    const int16_t *points;
    int count;
};

struct mar_line {
    const int16_t *points;
    int count;
    enum mar_line_kind kind;
    int min_zoom;
};

struct mar_place {
    int16_t x;
    int16_t y;
    const char *name;
    enum mar_place_kind kind;
    int min_zoom;
};

struct mar_world_scene {
    int scene;
    int latitude_e6;
    int longitude_e6;
    const char *name;
    const char *detail;
    const char *video;
};

static const struct mar_world_scene mar_world_scenes[] = {
    { MAR_SCENE_TIMES_SQUARE, 40758000, -73985500,
      "Times Square", "New York  •  rotatable 360°", NULL },
    { MAR_SCENE_TORONTO_360, 43653200, -79383200,
      "Downtown Toronto", "Canada  •  rotatable 360°", NULL },
    { -10, 59180000, 25180000,
      "Kose", "Estonia  •  full dashcam video", "kose_estonia.mpg" },
    { -11, 46240000, 14360000,
      "Kranj", "Slovenia  •  full dashcam video", "kranj_slovenia.mpg" },
    { -12, 44880000, 15620000,
      "Plitvice", "Croatia to Slovenia  •  full dashcam video",
      "plitvice_ljubljana.mpg" },
    { -13, 37340000, 127920000,
      "Wonju", "South Korea  •  full dashcam video", "wonju_korea.mpg" },
    { -14, -41510000, 173960000,
      "Blenheim", "New Zealand  •  full dashcam video",
      "blenheim_havelock.mpg" },
    { -15, 39140000, -77200000,
      "Gaithersburg", "Maryland, USA  •  full dashcam video",
      "gaithersburg_maryland.mpg" },
    { -16, 35470000, -97520000,
      "Oklahoma City", "USA  •  full dashcam video", "oklahoma_i235.mpg" }
};
static bool mar_world_scene_available[ARRAYLEN(mar_world_scenes)];

static void mar_update_world_scene_availability(void)
{
    unsigned i;

    for (i = 0; i < ARRAYLEN(mar_world_scenes); i++) {
        const struct mar_world_scene *scene = &mar_world_scenes[i];
        char path[MAX_PATH];

        if (scene->video) {
            rb->snprintf(path, sizeof(path), "%s/%s", MAR_DASHCAM_VIDEO_DIR,
                         scene->video);
            mar_world_scene_available[i] = rb->file_exists(path);
        } else if (scene->scene == MAR_SCENE_TIMES_SQUARE) {
            mar_world_scene_available[i] =
                rb->file_exists(MAR_TIMES_SQUARE_NORTH_PATH);
        } else if (scene->scene == MAR_SCENE_TORONTO_360) {
            mar_world_scene_available[i] = rb->file_exists(
                ROCKBOX_DIR "/maps/toronto_360_north.rgb");
        } else {
            mar_world_scene_available[i] = false;
        }
    }
}

static int mar_world_scene_count(void)
{
    unsigned i;
    int count = 0;

    for (i = 0; i < ARRAYLEN(mar_world_scenes); i++)
        if (mar_world_scene_available[i])
            count++;
    return count;
}

static int mar_world_scene_at(int visible_index)
{
    unsigned i;

    for (i = 0; i < ARRAYLEN(mar_world_scenes); i++) {
        if (!mar_world_scene_available[i])
            continue;
        if (visible_index-- == 0)
            return (int)i;
    }
    return -1;
}

static const int zoom_scale[MAR_ZOOM_COUNT] = {
    8, 12, 18, 28, 44, 64, 96, 144, 216
};
static fb_data mar_satellite[MAR_SAT_W * MAR_STREET_H];
union mar_visual_cache {
    fb_data map_tiles[4][MAR_SAT_W * MAR_SAT_H];
    fb_data lookaround[4][MAR_SAT_W * MAR_STREET_H];
};
static union mar_visual_cache mar_visual_cache;
static int mar_satellite_zoom = -1;
static int mar_satellite_x = -1;
static int mar_satellite_y = -1;
static int mar_street_place = -1;
static int mar_world_dashcam_scene = MAR_SCENE_NONE;
static int mar_world_dashcam_frame = -1;
static int mar_lookaround_scene = MAR_SCENE_NONE;
static char mar_sync_text[MAR_SYNC_TEXT_SIZE];
struct mar_sync_state {
    bool has_location;
    int location_x, location_y;
    int photo_count;
    int photo_x[MAR_MAX_SYNC_PHOTOS], photo_y[MAR_MAX_SYNC_PHOTOS];
    bool photo_thumb_valid[MAR_MAX_SYNC_PHOTOS];
    char photo_title[MAR_MAX_SYNC_PHOTOS][28];
    fb_data photo_thumb[MAR_MAX_SYNC_PHOTOS]
                       [MAR_PHOTO_THUMB_W * MAR_PHOTO_THUMB_H];
    int route_count;
    int route_x[MAR_MAX_ROUTE_POINTS], route_y[MAR_MAX_ROUTE_POINTS];
};
static struct mar_sync_state mar_sync;

/* Web-Mercator Y values for each whole latitude from -85 through +85.
 * The satellite atlas is a standard slippy-map tile tree, not an
 * equirectangular image. One-degree interpolation keeps the conversion
 * integer-only and makes synced locations land on their actual imagery. */
static const int16_t mar_mercator_y_by_lat[] = {
    6589, 6397, 6235, 6094, 5970, 5859, 5758, 5666, 5582, 5503, 5430, 5361,
    5297, 5236, 5178, 5123, 5071, 5021, 4973, 4927, 4882, 4840, 4799, 4759,
    4721, 4683, 4647, 4612, 4578, 4545, 4512, 4481, 4450, 4420, 4390, 4362,
    4333, 4306, 4279, 4252, 4226, 4200, 4175, 4150, 4125, 4101, 4078, 4054,
    4031, 4008, 3986, 3964, 3942, 3920, 3898, 3877, 3856, 3835, 3814, 3794,
    3774, 3753, 3733, 3714, 3694, 3674, 3655, 3636, 3616, 3597, 3578, 3559,
    3540, 3522, 3503, 3484, 3466, 3447, 3429, 3410, 3392, 3373, 3355, 3337,
    3318, 3300, 3282, 3263, 3245, 3227, 3208, 3190, 3171, 3153, 3134, 3116,
    3097, 3078, 3060, 3041, 3022, 3003, 2984, 2964, 2945, 2926, 2906, 2886,
    2867, 2847, 2826, 2806, 2786, 2765, 2744, 2723, 2702, 2680, 2658, 2636,
    2614, 2592, 2569, 2546, 2522, 2499, 2475, 2450, 2425, 2400, 2374, 2348,
    2321, 2294, 2267, 2238, 2210, 2180, 2150, 2119, 2088, 2055, 2022, 1988,
    1953, 1917, 1879, 1841, 1801, 1760, 1718, 1673, 1627, 1579, 1529, 1477,
    1422, 1364, 1303, 1239, 1170, 1097, 1018, 934, 842, 741, 630, 506,
    365, 203, 11,
};

/* World coordinates match the standard Web-Mercator satellite tile grid. */
static void mar_geo_to_world(int latitude_e6, int longitude_e6, int *x, int *y)
{
    int latitude_index;
    int latitude_fraction;

    /* Native ARM long is 32 bit: use a 64-bit intermediate or ordinary
     * location coordinates overflow and land in the ocean. */
    *x = (int)(((long long)(longitude_e6 + 180000000) * MAR_WORLD_W) /
               360000000LL);
    if (latitude_e6 < -85000000)
        latitude_e6 = -85000000;
    else if (latitude_e6 > 85000000)
        latitude_e6 = 85000000;
    latitude_index = (latitude_e6 + 85000000) / 1000000;
    latitude_fraction = (latitude_e6 + 85000000) % 1000000;
    if (latitude_index >= (int)ARRAYLEN(mar_mercator_y_by_lat) - 1)
        *y = mar_mercator_y_by_lat[ARRAYLEN(mar_mercator_y_by_lat) - 1];
    else
        *y = mar_mercator_y_by_lat[latitude_index] +
             ((mar_mercator_y_by_lat[latitude_index + 1] -
               mar_mercator_y_by_lat[latitude_index]) * latitude_fraction) /
             1000000;
}

static bool mar_parse_number(const char **cursor, int *value)
{
    const char *p = *cursor;
    int sign = 1;
    long result = 0;
    bool digits = false;

    if (*p == '-') {
        sign = -1;
        p++;
    }
    while (*p >= '0' && *p <= '9') {
        result = result * 10 + (*p++ - '0');
        digits = true;
    }
    if (!digits)
        return false;
    *value = (int)(sign * result);
    *cursor = p;
    return true;
}

static bool mar_parse_pair(const char *text, int *first, int *second)
{
    if (!mar_parse_number(&text, first) || *text++ != '\t' ||
        !mar_parse_number(&text, second))
        return false;
    return true;
}

static void mar_load_photo_thumb(int index)
{
    char path[64];
    int fd;
    ssize_t got;

    if (index < 0 || index >= MAR_MAX_SYNC_PHOTOS)
        return;
    rb->snprintf(path, sizeof(path), MAR_PHOTO_THUMB_PATH, index);
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return;
    got = rb->read(fd, mar_sync.photo_thumb[index],
                   sizeof(mar_sync.photo_thumb[index]));
    rb->close(fd);
    mar_sync.photo_thumb_valid[index] =
        got == (ssize_t)sizeof(mar_sync.photo_thumb[index]);
}

static void mar_load_sync(void)
{
    int fd = rb->open(MAR_SYNC_PATH, O_RDONLY);
    ssize_t got;
    char *line;

    rb->memset(&mar_sync, 0, sizeof(mar_sync));
    if (fd < 0)
        return;
    got = rb->read(fd, mar_sync_text, sizeof(mar_sync_text) - 1);
    rb->close(fd);
    if (got <= 0)
        return;
    mar_sync_text[got] = '\0';
    line = mar_sync_text;
    while (line && *line) {
        char *next = rb->strchr(line, '\n');
        int lat, lon;
        if (next)
            *next++ = '\0';
        if (!rb->strncmp(line, "location\t", 9) &&
            mar_parse_pair(line + 9, &lat, &lon)) {
            mar_geo_to_world(lat, lon, &mar_sync.location_x, &mar_sync.location_y);
            mar_sync.has_location = true;
        } else if (!rb->strncmp(line, "photo\t", 6) &&
                   mar_parse_pair(line + 6, &lat, &lon) &&
                   mar_sync.photo_count < MAR_MAX_SYNC_PHOTOS) {
            int index = mar_sync.photo_count;
            char *title = line + 6;
            char *tab;

            tab = rb->strchr(title, '\t');
            if (tab)
                tab = rb->strchr(tab + 1, '\t');
            if (tab) {
                title = tab + 1;
                tab = rb->strchr(title, '\t');
                if (tab)
                    *tab = '\0';
                rb->strlcpy(mar_sync.photo_title[index], title,
                            sizeof(mar_sync.photo_title[index]));
            }
            mar_geo_to_world(lat, lon, &mar_sync.photo_x[index],
                             &mar_sync.photo_y[index]);
            mar_load_photo_thumb(index);
            mar_sync.photo_count++;
        } else if (!rb->strncmp(line, "point\t", 6) &&
                   mar_parse_pair(line + 6, &lat, &lon) &&
                   mar_sync.route_count < MAR_MAX_ROUTE_POINTS) {
            mar_geo_to_world(lat, lon, &mar_sync.route_x[mar_sync.route_count],
                             &mar_sync.route_y[mar_sync.route_count]);
            mar_sync.route_count++;
        }
        line = next;
    }
}

static bool mar_load_satellite_tile(int zoom, int x, int y, fb_data *buffer)
{
    char tile_path[64];
    int fd;
    ssize_t got;

    if (zoom == 0) {
        rb->snprintf(tile_path, sizeof(tile_path), MAR_WORLD_GLOBE_PATH, x);
        fd = rb->open(tile_path, O_RDONLY);
        /* Keep a valid static globe on installations that have not received
         * the rotatable-frame asset package yet. */
        if (fd < 0)
            fd = rb->open(MAR_WORLD_SAT_PATH, O_RDONLY);
    }
    else {
        /* FAT32 cannot hold a complete 65,536-file z8 atlas in one directory.
         * 8/ holds x 0..126; 8_hi/ holds x 127..253; the final two
         * columns use 8_hi2/. */
        if (zoom == 8 && x >= 254)
            rb->snprintf(tile_path, sizeof(tile_path), MAR_WORLD_TILE_HI2_PATH,
                         zoom, x, y);
        else if (zoom == 8 && x >= 127)
            rb->snprintf(tile_path, sizeof(tile_path), MAR_WORLD_TILE_HI_PATH,
                         zoom, x, y);
        else
            rb->snprintf(tile_path, sizeof(tile_path), MAR_WORLD_TILE_PATH,
                         zoom, x, y);
        fd = rb->open(tile_path, O_RDONLY);
    }
    if (fd < 0)
        return false;
    got = rb->read(fd, buffer, MAR_SAT_BYTES);
    rb->close(fd);
    return got == (ssize_t)MAR_SAT_BYTES;
}

static bool mar_load_satellite(const struct mar_view *view)
{
    int tiles = 1 << view->zoom;
    int span_x = MAR_WORLD_W / tiles;
    int span_y = MAR_WORLD_H / tiles;
    int origin_x = view->center_x - span_x / 2;
    int origin_y = view->center_y - span_y / 2;
    int x;
    int y;

    if (view->zoom == 0) {
        int globe_frame = (view->center_x * MAR_GLOBE_FRAMES) / MAR_WORLD_W;

        if (globe_frame >= MAR_GLOBE_FRAMES)
            globe_frame = 0;
        if (mar_satellite_zoom == 0 && mar_satellite_x == globe_frame)
            return true;
        mar_satellite_zoom = -1;
        if (!mar_load_satellite_tile(0, globe_frame, 0,
                                     mar_visual_cache.map_tiles[0]))
            return false;
        mar_satellite_zoom = 0;
        mar_satellite_x = globe_frame;
        mar_satellite_y = 0;
        return true;
    }

    while (origin_x < 0)
        origin_x += MAR_WORLD_W;
    while (origin_x >= MAR_WORLD_W)
        origin_x -= MAR_WORLD_W;
    if (origin_y < 0)
        origin_y = 0;
    else if (origin_y > MAR_WORLD_H - span_y)
        origin_y = MAR_WORLD_H - span_y;
    x = (origin_x * tiles) / MAR_WORLD_W;
    y = (origin_y * tiles) / MAR_WORLD_H;
    if (x >= tiles)
        x = tiles - 1;
    if (y >= tiles)
        y = tiles - 1;
    if (mar_satellite_zoom == view->zoom && mar_satellite_x == x &&
        mar_satellite_y == y)
        return true;
    mar_satellite_zoom = -1;
    if (!mar_load_satellite_tile(view->zoom, x, y,
                                 mar_visual_cache.map_tiles[0]) ||
        !mar_load_satellite_tile(view->zoom, (x + 1) % tiles, y,
                                 mar_visual_cache.map_tiles[1]) ||
        !mar_load_satellite_tile(view->zoom, x, MIN(y + 1, tiles - 1),
                                 mar_visual_cache.map_tiles[2]) ||
        !mar_load_satellite_tile(view->zoom, (x + 1) % tiles,
                                 MIN(y + 1, tiles - 1),
                                 mar_visual_cache.map_tiles[3]))
        return false;
    mar_satellite_zoom = view->zoom;
    mar_satellite_x = x;
    mar_satellite_y = y;
    return true;
}

/* Coasts are schematic, geographically arranged Maritime land masses. */
static const int16_t new_brunswick[] = {
    380, 1350, 970, 720, 1820, 400, 3100, 450, 4100, 760,
    4880, 1250, 5200, 1840, 5040, 2440, 4580, 2790, 4660, 3330,
    4250, 3680, 3650, 3620, 3070, 3960, 2500, 4280, 1850, 4210,
    1120, 3900, 540, 3220, 260, 2400, 380, 1350
};
static const int16_t nova_scotia[] = {
    4650, 3450, 5200, 3320, 5810, 3440, 6530, 3700, 7190, 4050,
    7870, 4440, 8530, 4890, 8890, 5450, 8650, 5860, 8080, 6070,
    7470, 5800, 6910, 5460, 6250, 5200, 5650, 4860, 5110, 4420,
    4750, 3990, 4650, 3450
};
static const int16_t cape_breton[] = {
    7640, 3040, 8240, 2540, 9100, 2390, 9690, 2740, 9870, 3350,
    9560, 3850, 8890, 4040, 8250, 3830, 7790, 3480, 7640, 3040
};
static const int16_t prince_edward_island[] = {
    5260, 2350, 5740, 2110, 6380, 2070, 6900, 2220, 6790, 2440,
    6180, 2530, 5540, 2510, 5260, 2350
};
static const int16_t quebec_edge[] = {
    0, 0, 4750, 0, 4300, 560, 3260, 440, 2100, 300, 900, 540, 0, 980, 0, 0
};
static const int16_t maine_edge[] = {
    0, 4000, 620, 3970, 1240, 4250, 1800, 4720, 2060, 5360,
    1740, 6600, 0, 6600, 0, 4000
};

static const struct mar_shape land_shapes[] = {
    { quebec_edge, ARRAYLEN(quebec_edge) / 2 },
    { maine_edge, ARRAYLEN(maine_edge) / 2 },
    { new_brunswick, ARRAYLEN(new_brunswick) / 2 },
    { nova_scotia, ARRAYLEN(nova_scotia) / 2 },
    { cape_breton, ARRAYLEN(cape_breton) / 2 },
    { prince_edward_island, ARRAYLEN(prince_edward_island) / 2 },
};

static const int16_t trans_canada[] = {
    1280, 1550, 2140, 1770, 2860, 1900, 3500, 2100, 4200, 2240,
    4740, 2430, 5150, 2600, 5600, 2760, 6110, 2860, 6640, 3040,
    7100, 3260, 7560, 3440, 8190, 3580, 8740, 3420, 9180, 3160
};
static const int16_t highway_101[] = {
    5350, 3670, 5630, 3990, 5890, 4340, 6120, 4730, 6410, 5150, 6760, 5480
};
static const int16_t highway_102[] = {
    5270, 3100, 5520, 3500, 5710, 3940, 5880, 4330
};
static const int16_t highway_103[] = {
    5880, 4330, 5500, 4460, 5100, 4490, 4710, 4400, 4270, 4250
};
static const int16_t nb_route_1[] = {
    1250, 3230, 1810, 3490, 2490, 3650, 3180, 3670, 3910, 3530, 4660, 3330
};
static const int16_t nb_route_8[] = {
    2450, 2140, 2780, 2510, 3040, 2890, 3360, 3230, 3670, 3580
};
static const int16_t nb_route_11[] = {
    3510, 1080, 3820, 1400, 4050, 1740, 4280, 2140, 4540, 2480
};
static const int16_t pei_route_1[] = {
    5400, 2370, 5770, 2320, 6130, 2290, 6490, 2300, 6760, 2300
};
static const int16_t ns_highway_104[] = {
    6650, 3040, 7090, 3230, 7560, 3440, 8030, 3560, 8420, 3500
};
static const int16_t ns_highway_105[] = {
    8420, 3500, 8750, 3390, 9040, 3250, 9340, 3130
};
static const int16_t dartmouth_111[] = {
    5880, 3950, 6070, 3780, 6230, 3660
};
static const int16_t saint_john_river[] = {
    680, 980, 1120, 1390, 1530, 1820, 1800, 2260, 2100, 2620,
    2360, 2950, 2630, 3270, 3000, 3470
};
static const int16_t miramichi_river[] = {
    3430, 1140, 3670, 1510, 3920, 1870, 4280, 2140, 4680, 2390
};
static const int16_t shubenacadie[] = {
    5370, 2960, 5530, 3260, 5620, 3500, 5800, 3740
};
static const int16_t provincial_boundary[] = {
    4700, 2400, 5000, 2600, 5230, 2850, 5300, 3200
};
static const int16_t canada_us_border[] = {
    0, 3970, 620, 3970, 1240, 4250, 1800, 4720, 2060, 5360
};

static const struct mar_line mar_lines[] = {
    { saint_john_river, ARRAYLEN(saint_john_river) / 2, MAR_LINE_WATER, 0 },
    { miramichi_river, ARRAYLEN(miramichi_river) / 2, MAR_LINE_WATER, 1 },
    { shubenacadie, ARRAYLEN(shubenacadie) / 2, MAR_LINE_WATER, 2 },
    { canada_us_border, ARRAYLEN(canada_us_border) / 2, MAR_LINE_COUNTRY_BORDER, 1 },
    { provincial_boundary, ARRAYLEN(provincial_boundary) / 2, MAR_LINE_BOUNDARY, 2 },
    { trans_canada, ARRAYLEN(trans_canada) / 2, MAR_LINE_HIGHWAY, 0 },
    { nb_route_1, ARRAYLEN(nb_route_1) / 2, MAR_LINE_HIGHWAY, 1 },
    { highway_101, ARRAYLEN(highway_101) / 2, MAR_LINE_HIGHWAY, 1 },
    { highway_102, ARRAYLEN(highway_102) / 2, MAR_LINE_HIGHWAY, 1 },
    { highway_103, ARRAYLEN(highway_103) / 2, MAR_LINE_ROAD, 2 },
    { nb_route_8, ARRAYLEN(nb_route_8) / 2, MAR_LINE_ROAD, 2 },
    { nb_route_11, ARRAYLEN(nb_route_11) / 2, MAR_LINE_ROAD, 2 },
    { pei_route_1, ARRAYLEN(pei_route_1) / 2, MAR_LINE_ROAD, 2 },
    { ns_highway_104, ARRAYLEN(ns_highway_104) / 2, MAR_LINE_HIGHWAY, 1 },
    { ns_highway_105, ARRAYLEN(ns_highway_105) / 2, MAR_LINE_HIGHWAY, 1 },
    { dartmouth_111, ARRAYLEN(dartmouth_111) / 2, MAR_LINE_ROAD, 3 },
};

static const struct mar_place mar_places[] = {
    { 2320, 2510, "Fredericton", MAR_CITY, 0 },
    { 2960, 3280, "Saint John", MAR_CITY, 0 },
    { 4330, 2390, "Moncton", MAR_CITY, 0 },
    { 6100, 2220, "Charlottetown", MAR_CITY, 0 },
    { 5880, 3950, "Halifax", MAR_CITY, 0 },
    { 9180, 3160, "Sydney", MAR_CITY, 0 },
    { 6800, 5480, "Yarmouth", MAR_TOWN, 1 },
    { 5480, 3100, "Truro", MAR_TOWN, 1 },
    { 4720, 3320, "Amherst", MAR_TOWN, 2 },
    { 4180, 1950, "Miramichi", MAR_TOWN, 2 },
    { 1300, 1550, "Edmundston", MAR_TOWN, 2 },
    { 7550, 3440, "Antigonish", MAR_TOWN, 2 },
    { 8650, 3200, "Baddeck", MAR_TOWN, 2 },
    { 5270, 3100, "Sackville", MAR_TOWN, 2 },
    { 4050, 2780, "Dieppe", MAR_TOWN, 2 },
    { 3690, 3570, "St. Stephen", MAR_TOWN, 2 },
    { 1980, 3300, "Grand Falls", MAR_TOWN, 2 },
    { 6100, 3650, "Dartmouth", MAR_TOWN, 2 },
    { 5480, 4200, "Lunenburg", MAR_TOWN, 2 },
    { 4960, 4480, "Bridgewater", MAR_TOWN, 2 },
    { 7080, 3530, "New Glasgow", MAR_TOWN, 2 },
    { 8410, 3510, "Port Hawkesbury", MAR_TOWN, 2 },
    { 9400, 3280, "Glace Bay", MAR_TOWN, 2 },
    { 6420, 2320, "Summerside", MAR_TOWN, 2 },
    { 3580, 1050, "Chaleur Bay", MAR_WATER_NAME, 0 },
    { 5420, 1800, "Gulf of St. Lawrence", MAR_WATER_NAME, 0 },
    { 3440, 4620, "Bay of Fundy", MAR_WATER_NAME, 0 },
    { 7450, 4580, "Atlantic Ocean", MAR_WATER_NAME, 0 },
    { 2600, 2580, "NEW BRUNSWICK", MAR_ISLAND, 0 },
    { 7150, 4750, "NOVA SCOTIA", MAR_ISLAND, 0 },
    { 6090, 2310, "Prince Edward Island", MAR_ISLAND, 1 },
};

struct mar_geo_point {
    int latitude_e6;
    int longitude_e6;
};

/* Real destination coordinates. The older x/y values remain only for the
 * regional reference geometry that is not drawn over satellite imagery. */
static const struct mar_geo_point mar_place_geo[] = {
    { 45963600, -66643100 }, { 45273300, -66063300 },
    { 46087800, -64778200 }, { 46238200, -63131100 },
    { 44648800, -63575200 }, { 46136800, -60194200 },
    { 43837100, -66118000 }, { 45365000, -63285000 },
    { 45833000, -64207000 }, { 47028000, -65501000 },
    { 47373700, -68325100 }, { 45622700, -61993400 },
    { 46100000, -60753000 }, { 45898800, -64368000 },
    { 46078000, -64687000 }, { 45195000, -67276000 },
    { 47052000, -67739000 }, { 44671300, -63577200 },
    { 44378000, -64312000 }, { 44238000, -64151000 },
    { 45623300, -62645000 }, { 45618200, -61734000 },
    { 46196000, -59956000 }, { 46239000, -63132000 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }
};

static void mar_place_to_world(int place_index, int *x, int *y)
{
    const struct mar_geo_point *point = &mar_place_geo[place_index];

    if (point->latitude_e6 != 0)
        mar_geo_to_world(point->latitude_e6, point->longitude_e6, x, y);
    else {
        *x = mar_places[place_index].x;
        *y = mar_places[place_index].y;
    }
}

static inline int mar_mid_y(void)
{
    return (MAR_MAP_Y + LCD_HEIGHT) / 2;
}

static inline int mar_x(const struct mar_view *view, int x)
{
    int span = MAR_WORLD_W / (1 << view->zoom);

    return LCD_WIDTH / 2 + (int)(((long)(x - view->center_x) * LCD_WIDTH) /
                                 MAX(1, span));
}

static inline int mar_y(const struct mar_view *view, int y)
{
    int span = MAR_WORLD_H / (1 << view->zoom);

    return mar_mid_y() + (int)(((long)(y - view->center_y) * MAR_SAT_H) /
                               MAX(1, span));
}

static void mar_color(unsigned color)
{
    rb->lcd_set_foreground(color);
}

static void mar_halo(int x, int y, const char *text, unsigned color,
                     unsigned halo)
{
    mar_color(halo);
    rb->lcd_putsxy(x - 1, y, text);
    rb->lcd_putsxy(x + 1, y, text);
    rb->lcd_putsxy(x, y - 1, text);
    rb->lcd_putsxy(x, y + 1, text);
    mar_color(color);
    rb->lcd_putsxy(x, y, text);
}

static void mar_fill_shape(const struct mar_view *view,
                           const struct mar_shape *shape, unsigned color)
{
    int y;

    mar_color(color);
    for (y = MAR_MAP_Y; y < LCD_HEIGHT; y++) {
        int xs[32];
        int n = 0;
        int i;

        for (i = 0; i < shape->count - 1 && n < (int)ARRAYLEN(xs); i++) {
            int x0 = mar_x(view, shape->points[i * 2]);
            int y0 = mar_y(view, shape->points[i * 2 + 1]);
            int x1 = mar_x(view, shape->points[i * 2 + 2]);
            int y1 = mar_y(view, shape->points[i * 2 + 3]);

            if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y))
                xs[n++] = x0 + (int)(((long)(y - y0) * (x1 - x0)) / (y1 - y0));
        }

        for (i = 0; i + 1 < n; i++) {
            int j;
            for (j = i + 1; j < n; j++) {
                if (xs[j] < xs[i]) {
                    int t = xs[i];
                    xs[i] = xs[j];
                    xs[j] = t;
                }
            }
        }
        for (i = 0; i + 1 < n; i += 2)
            rb->lcd_hline(xs[i], xs[i + 1], y);

        if (view->mode == MAR_MODE_SATELLITE && (y % 3) == 0) {
            for (i = 0; i + 1 < n; i += 2) {
                int x = xs[i] + ((y * 11) & 15);
                mar_color(((y / 3) & 1) ? LCD_RGBPACK(62, 102, 65) :
                          LCD_RGBPACK(79, 119, 72));
                for (; x < xs[i + 1]; x += 22)
                    rb->lcd_hline(x, MIN(x + 10, xs[i + 1]), y);
            }
            mar_color(color);
        }
    }
}

static void mar_stroke(const struct mar_view *view, const int16_t *points,
                       int count, unsigned color, int width)
{
    int i;

    mar_color(color);
    for (i = 0; i < count - 1; i++) {
        int x0 = mar_x(view, points[i * 2]);
        int y0 = mar_y(view, points[i * 2 + 1]);
        int x1 = mar_x(view, points[i * 2 + 2]);
        int y1 = mar_y(view, points[i * 2 + 3]);
        int w;

        for (w = 0; w < width; w++) {
            rb->lcd_drawline(x0, y0 + w, x1, y1 + w);
            if (w)
                rb->lcd_drawline(x0 + w, y0, x1 + w, y1);
        }
    }
}

static bool mar_is_destination(const struct mar_place *place)
{
    return place->kind == MAR_CITY || place->kind == MAR_TOWN;
}

static int mar_destination_count(void)
{
    unsigned i;
    int count = 0;

    for (i = 0; i < ARRAYLEN(mar_places); i++)
        if (mar_is_destination(&mar_places[i]))
            count++;
    return count;
}

static int mar_destination_at(int ordinal)
{
    unsigned i;

    for (i = 0; i < ARRAYLEN(mar_places); i++) {
        if (!mar_is_destination(&mar_places[i]))
            continue;
        if (ordinal-- == 0)
            return i;
    }
    return -1;
}

static void mar_focus_place(struct mar_view *view, int place_index)
{
    mar_place_to_world(place_index, &view->center_x, &view->center_y);
    if (view->zoom < 2)
        view->zoom = 2;
    view->selected_place = place_index;
    view->screen = MAR_SCREEN_MAP;
}

static void mar_draw_land(const struct mar_view *view)
{
    unsigned i;
    unsigned land = view->mode == MAR_MODE_SATELLITE ? LCD_RGBPACK(69, 109, 63) :
                    view->mode == MAR_MODE_NIGHT ? LCD_RGBPACK(51, 64, 65) :
                    view->mode == MAR_MODE_TERRAIN ? MAR_LAND_TOPO : MAR_LAND;

    for (i = 0; i < ARRAYLEN(land_shapes); i++)
        mar_fill_shape(view, &land_shapes[i], land);
    for (i = 0; i < ARRAYLEN(land_shapes); i++)
        mar_stroke(view, land_shapes[i].points, land_shapes[i].count,
                   view->mode == MAR_MODE_SATELLITE ? LCD_RGBPACK(30, 73, 62) :
                   view->mode == MAR_MODE_NIGHT ? LCD_RGBPACK(132, 178, 183) :
                   MAR_COAST, 1);
}

static void mar_draw_lines(const struct mar_view *view)
{
    unsigned i;

    /* At globe scale, satellite imagery is intentionally uncluttered. */
    if (view->zoom == 0)
        return;

    for (i = 0; i < ARRAYLEN(mar_lines); i++) {
        const struct mar_line *line = &mar_lines[i];
        unsigned casing;
        unsigned fill;
        int width;

        if (view->zoom < line->min_zoom)
            continue;
        if (line->kind == MAR_LINE_WATER) {
            mar_stroke(view, line->points, line->count,
                       LCD_RGBPACK(73, 153, 203), 2);
            continue;
        }
        if (line->kind == MAR_LINE_BOUNDARY || line->kind == MAR_LINE_COUNTRY_BORDER) {
            mar_stroke(view, line->points, line->count,
                       line->kind == MAR_LINE_COUNTRY_BORDER ?
                       LCD_RGBPACK(120, 96, 74) : LCD_RGBPACK(159, 159, 151), 1);
            continue;
        }
        /* Schematic road geometry must never be laid over real imagery. */
        if (view->mode == MAR_MODE_SATELLITE)
            continue;

        casing = view->mode == MAR_MODE_NIGHT ? LCD_RGBPACK(63, 70, 75) :
                 LCD_RGBPACK(201, 186, 159);
        fill = line->kind == MAR_LINE_HIGHWAY ? LCD_RGBPACK(245, 177, 67) :
               LCD_RGBPACK(255, 255, 255);
        width = line->kind == MAR_LINE_HIGHWAY ? 3 : 2;
        mar_stroke(view, line->points, line->count, casing, width + 1);
        mar_stroke(view, line->points, line->count, fill, width);
    }
}

static void mar_draw_places(const struct mar_view *view)
{
    unsigned i;
    int fw;
    int fh;

    /* Roads, labels, and regional annotations start after globe scale. */
    if (view->zoom == 0)
        return;
    rb->lcd_getstringsize("M", &fw, &fh);
    for (i = 0; i < ARRAYLEN(mar_places); i++) {
        const struct mar_place *place = &mar_places[i];
        int x;
        int y;
        int text_w;
        unsigned color;

        if (view->zoom < place->min_zoom || !view->labels)
            continue;
        int world_x;
        int world_y;

        mar_place_to_world(i, &world_x, &world_y);
        x = mar_x(view, world_x);
        y = mar_y(view, world_y);
        if (x < -70 || x > LCD_WIDTH + 30 || y < MAR_MAP_Y || y > LCD_HEIGHT - fh)
            continue;

        if (place->kind == MAR_WATER_NAME) {
            mar_color(view->mode == MAR_MODE_NIGHT ? LCD_RGBPACK(165, 214, 232) :
                      LCD_RGBPACK(57, 126, 171));
            rb->lcd_putsxy(x, y, place->name);
            continue;
        }
        color = place->kind == MAR_CITY ? MAR_TEXT : LCD_RGBPACK(79, 84, 85);
        if (place->kind == MAR_ISLAND)
            color = LCD_RGBPACK(86, 110, 65);
        if (place->kind != MAR_ISLAND) {
            mar_color(place->kind == MAR_CITY ? LCD_RGBPACK(30, 30, 30) :
                      LCD_RGBPACK(70, 70, 70));
            rb->lcd_fillrect(x - 2, y - 2, place->kind == MAR_CITY ? 5 : 3,
                             place->kind == MAR_CITY ? 5 : 3);
        }
        rb->lcd_getstringsize(place->name, &text_w, &fh);
        if (x + 5 + text_w > LCD_WIDTH)
            x -= text_w + 7;
        else
            x += 5;
        mar_halo(x, y - fh / 2, place->name, color,
                 view->mode == MAR_MODE_SATELLITE ||
                 view->mode == MAR_MODE_NIGHT ? LCD_RGBPACK(25, 42, 31) :
                 LCD_RGBPACK(255, 255, 255));
    }
}

static const char *mar_place_type(const struct mar_place *place)
{
    return place->kind == MAR_CITY ? "City" : "Destination";
}

static void mar_draw_selection(const struct mar_view *view)
{
    const struct mar_place *place;
    int x;
    int y;
    int text_w;
    int text_h;
    int bubble_x;
    int bubble_w;

    if (view->selected_place < 0 ||
        view->selected_place >= (int)ARRAYLEN(mar_places))
        return;
    place = &mar_places[view->selected_place];
    mar_place_to_world(view->selected_place, &x, &y);
    x = mar_x(view, x);
    y = mar_y(view, y);
    if (x < 0 || x >= LCD_WIDTH || y < MAR_MAP_Y || y >= LCD_HEIGHT)
        return;

    mar_color(LCD_RGBPACK(226, 63, 61));
    rb->lcd_fillrect(x - 3, y - 10, 7, 9);
    rb->lcd_fillrect(x - 1, y - 1, 3, 3);
    rb->lcd_getstringsize(place->name, &text_w, &text_h);
    bubble_w = MIN(text_w + 14, 146);
    bubble_x = x - bubble_w / 2;
    if (bubble_x < 4)
        bubble_x = 4;
    else if (bubble_x + bubble_w > LCD_WIDTH - 4)
        bubble_x = LCD_WIDTH - bubble_w - 4;
    mar_color(LCD_WHITE);
    rb->lcd_fillrect(bubble_x, y - 29, bubble_w, 17);
    mar_color(LCD_RGBPACK(197, 201, 202));
    rb->lcd_drawrect(bubble_x, y - 29, bubble_w, 17);
    mar_color(MAR_TEXT);
    rb->lcd_putsxy(bubble_x + 7, y - 26, place->name);
}

static void mar_draw_synced_items(const struct mar_view *view)
{
    int i;

    if (view->zoom == 0)
        return;

    if (mar_sync.route_count > 1) {
        mar_color(LCD_RGBPACK(0, 122, 255));
        for (i = 0; i < mar_sync.route_count - 1; i++)
            rb->lcd_drawline(mar_x(view, mar_sync.route_x[i]),
                             mar_y(view, mar_sync.route_y[i]),
                             mar_x(view, mar_sync.route_x[i + 1]),
                             mar_y(view, mar_sync.route_y[i + 1]));
    }
    for (i = 0; i < mar_sync.photo_count; i++) {
        int x = mar_x(view, mar_sync.photo_x[i]);
        int y = mar_y(view, mar_sync.photo_y[i]);

        if (x < -48 || x > LCD_WIDTH + 48 ||
            y < MAR_MAP_Y - 44 || y > LCD_HEIGHT + 8)
            continue;
        if (view->zoom >= 3 && mar_sync.photo_thumb_valid[i] &&
            x >= 22 && x <= LCD_WIDTH - 23 &&
            y >= MAR_MAP_Y + 39 && y <= LCD_HEIGHT - 6) {
            /* A bounded cache-only Photos card: the 1px shadow, white matte,
             * and small pointer intentionally echo Apple's Maps callouts. */
            mar_color(LCD_RGBPACK(53, 57, 60));
            rb->lcd_fillrect(x - 20, y - 38, 43, 34);
            mar_color(LCD_WHITE);
            rb->lcd_fillrect(x - 21, y - 39, 42, 33);
            rb->lcd_bitmap(mar_sync.photo_thumb[i], x - 19, y - 37,
                           MAR_PHOTO_THUMB_W, MAR_PHOTO_THUMB_H);
            mar_color(LCD_WHITE);
            rb->lcd_fillrect(x - 2, y - 6, 5, 5);
            mar_color(LCD_RGBPACK(54, 58, 61));
            rb->lcd_fillrect(x - 1, y - 3, 3, 3);
        } else {
            mar_color(LCD_WHITE);
            rb->lcd_fillrect(x - 5, y - 5, 11, 11);
            mar_color(LCD_RGBPACK(252, 149, 36));
            rb->lcd_fillrect(x - 3, y - 3, 7, 7);
            mar_color(LCD_WHITE);
            rb->lcd_fillrect(x - 1, y - 1, 3, 3);
        }
    }
    if (mar_sync.has_location) {
        int x = mar_x(view, mar_sync.location_x);
        int y = mar_y(view, mar_sync.location_y);

        /* The soft accuracy ring and centered dot keep the familiar stock
         * Maps location treatment legible over either aerial imagery. */
        mar_color(LCD_RGBPACK(159, 210, 249));
        xlcd_drawcircle(x, y, 7);
        mar_color(LCD_WHITE);
        xlcd_drawcircle(x, y, 5);
        mar_color(LCD_RGBPACK(0, 122, 255));
        rb->lcd_fillrect(x - 3, y - 3, 7, 7);
        mar_color(LCD_WHITE);
        rb->lcd_fillrect(x - 2, y - 2, 5, 5);
    }
}

static void mar_draw_explore(const struct mar_view *view)
{
    int count = mar_destination_count();
    int first = MAX(0, view->browse_index - 3);
    int last = MIN(count, first + 7);
    int i;
    int y = 96;
    unsigned bg = view->mode == MAR_MODE_NIGHT ? LCD_RGBPACK(31, 39, 43) :
                  LCD_RGBPACK(250, 250, 250);
    unsigned text = view->mode == MAR_MODE_NIGHT ? LCD_RGBPACK(239, 243, 244) :
                    MAR_TEXT;

    mar_color(bg);
    rb->lcd_fillrect(0, 70, LCD_WIDTH, LCD_HEIGHT - 70);
    mar_color(view->mode == MAR_MODE_NIGHT ? LCD_RGBPACK(92, 111, 118) :
              LCD_RGBPACK(213, 216, 218));
    rb->lcd_hline(0, LCD_WIDTH - 1, 70);
    mar_color(text);
    rb->lcd_putsxy(12, 78, "Explore Maritimes");
    mar_color(view->mode == MAR_MODE_NIGHT ? LCD_RGBPACK(164, 182, 189) :
              LCD_RGBPACK(106, 111, 114));
    rb->lcd_putsxy(12, 86, "Select map  Right detail  Left dashcams");

    for (i = first; i < last; i++, y += 20) {
        int place_index = mar_destination_at(i);
        const struct mar_place *place = &mar_places[place_index];

        if (i == view->browse_index) {
            mar_color(MAR_BLUE);
            rb->lcd_fillrect(7, y - 1, LCD_WIDTH - 14, 18);
        }
        mar_color(i == view->browse_index ? LCD_WHITE : text);
        rb->lcd_putsxy(15, y + 1, place->name);
        mar_color(i == view->browse_index ? LCD_RGBPACK(218, 235, 255) :
                  LCD_RGBPACK(117, 123, 126));
        rb->lcd_putsxy(184, y + 1, mar_place_type(place));
    }
}

static void mar_draw_world(const struct mar_view *view)
{
    int selected = view->browse_index;
    int first = MAX(0, selected - 3);
    int last = MIN(mar_world_scene_count(), first + 4);
    int i;
    int y = 105;
    mar_color(LCD_RGBPACK(250, 250, 250));
    rb->lcd_fillrect(0, 70, LCD_WIDTH, LCD_HEIGHT - 70);
    mar_color(LCD_RGBPACK(213, 216, 218));
    rb->lcd_hline(0, LCD_WIDTH - 1, 70);
    mar_color(MAR_TEXT);
    rb->lcd_putsxy(12, 78, "World Look Around");
    mar_color(LCD_RGBPACK(106, 111, 114));
    rb->lcd_putsxy(12, 87, "Offline real street imagery");
    for (i = first; i < last; i++, y += 35) {
        int scene_index = mar_world_scene_at(i);
        const struct mar_world_scene *scene = scene_index >= 0 ?
            &mar_world_scenes[scene_index] : NULL;
        if (!scene)
            continue;
        if (i == selected) {
            mar_color(MAR_BLUE);
            rb->lcd_fillrect(7, y, LCD_WIDTH - 14, 28);
        }
        mar_color(i == selected ? LCD_WHITE : MAR_TEXT);
        rb->lcd_putsxy(15, y + 5, scene->name);
        mar_color(i == selected ? LCD_RGBPACK(218, 235, 255) :
                  LCD_RGBPACK(106, 111, 114));
        rb->lcd_putsxy(15, y + 15, scene->detail);
    }
    (void)view;
}

static const char *mar_mode_name(enum mar_mode mode)
{
    (void)mode;
    return "Satellite";
}

static void mar_draw_status(const struct mar_view *view)
{
    struct tm *now = rb->get_time();
    char time_text[12];
    char zoom_text[24];
    int hour = now ? now->tm_hour : 9;
    int minute = now ? now->tm_min : 41;

    if (hour == 0)
        hour = 12;
    else if (hour > 12)
        hour -= 12;
    rb->snprintf(time_text, sizeof(time_text), "%d:%02d", hour, minute);
    rb->snprintf(zoom_text, sizeof(zoom_text), "%s  %dx", mar_mode_name(view->mode),
                 view->zoom + 1);

    mar_color(LCD_WHITE);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, MAR_MAP_Y);
    mar_color(LCD_RGBPACK(213, 216, 218));
    rb->lcd_hline(0, LCD_WIDTH - 1, MAR_MAP_Y - 1);
    mar_color(MAR_TEXT);
    rb->lcd_putsxy(9, 6, time_text);
    rb->lcd_putsxy(view->screen == MAR_SCREEN_EXPLORE ? 120 :
                   view->screen == MAR_SCREEN_WORLD ? 111 : 135, 6,
                   view->screen == MAR_SCREEN_EXPLORE ? "Explore" :
                   view->screen == MAR_SCREEN_WORLD ? "Look Around" : "Maps");
    mar_color(MAR_BLUE);
    rb->lcd_fillrect(LCD_WIDTH - 29, 7, 18, 8);
    mar_color(LCD_WHITE);
    rb->lcd_fillrect(LCD_WIDTH - 13, 9, 2, 4);

    if (view->mode == MAR_MODE_SATELLITE && view->zoom == 0) {
        mar_color(LCD_RGBPACK(248, 251, 253));
        rb->lcd_fillrect(0, LCD_HEIGHT - 28, LCD_WIDTH, 28);
        mar_color(LCD_RGBPACK(218, 225, 229));
        rb->lcd_hline(0, LCD_WIDTH - 1, LCD_HEIGHT - 28);
        mar_color(MAR_TEXT);
        rb->lcd_putsxy(14, LCD_HEIGHT - 20, "Earth");
        mar_color(LCD_RGBPACK(96, 103, 107));
        rb->lcd_putsxy(14, LCD_HEIGHT - 11, "Satellite globe");
    } else {
        mar_color(view->mode == MAR_MODE_NIGHT ? LCD_RGBPACK(38, 47, 52) :
                  LCD_RGBPACK(255, 255, 255));
        rb->lcd_fillrect(8, LCD_HEIGHT - 20, 116, 14);
        mar_color(view->mode == MAR_MODE_NIGHT ? LCD_RGBPACK(104, 127, 135) :
                  LCD_RGBPACK(211, 216, 218));
        rb->lcd_drawrect(8, LCD_HEIGHT - 20, 116, 14);
        mar_color(view->mode == MAR_MODE_NIGHT ? LCD_RGBPACK(235, 239, 240) :
                  MAR_TEXT);
        rb->lcd_putsxy(14, LCD_HEIGHT - 17, zoom_text);
    }
}

static void mar_draw_map_controls(const struct mar_view *view)
{
    static const char * const scale_names[] = {
        "Earth", "200 km", "100 km", "60 km", "40 km", "25 km"
    };
    unsigned panel = LCD_RGBPACK(255, 255, 255);
    unsigned border = LCD_RGBPACK(211, 216, 218);
    unsigned text = MAR_TEXT;
    int x = LCD_WIDTH - 39;

    if (view->screen != MAR_SCREEN_MAP)
        return;
    mar_color(panel);
    rb->lcd_fillrect(x, 33, 28, 48);
    mar_color(border);
    rb->lcd_drawrect(x, 33, 28, 48);
    rb->lcd_hline(x + 1, x + 26, 57);
    mar_color(text);
    rb->lcd_putsxy(x + 10, 39, "+");
    rb->lcd_putsxy(x + 10, 63, "-");

    mar_color(panel);
    rb->lcd_fillrect(LCD_WIDTH - 70, LCD_HEIGHT - 39, 62, 14);
    mar_color(border);
    rb->lcd_drawrect(LCD_WIDTH - 70, LCD_HEIGHT - 39, 62, 14);
    mar_color(text);
    rb->lcd_putsxy(LCD_WIDTH - 64, LCD_HEIGHT - 36,
                   scale_names[view->zoom]);
}

static const char *mar_street_path(int place_index)
{
    if (place_index < 0 || place_index >= (int)ARRAYLEN(mar_places))
        return NULL;
    if (!rb->strcmp(mar_places[place_index].name, "Moncton"))
        return MAR_STREET_MONCTON_PATH;
    if (!rb->strcmp(mar_places[place_index].name, "Fredericton"))
        return MAR_STREET_FREDERICTON_PATH;
    if (!rb->strcmp(mar_places[place_index].name, "Saint John"))
        return MAR_STREET_SAINT_JOHN_PATH;
    return NULL;
}

static bool mar_load_street(int place_index)
{
    const char *path = mar_street_path(place_index);
    int fd;
    ssize_t got;

    if (!path)
        return false;
    if (mar_street_place == place_index)
        return true;
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;
    got = rb->read(fd, mar_satellite, sizeof(mar_satellite));
    rb->close(fd);
    if (got != (ssize_t)sizeof(mar_satellite))
        return false;
    mar_street_place = place_index;
    return true;
}

static bool mar_load_lookaround(int scene)
{
    static const char * const times_square_paths[] = {
        MAR_TIMES_SQUARE_NORTH_PATH, MAR_TIMES_SQUARE_EAST_PATH,
        MAR_TIMES_SQUARE_SOUTH_PATH, MAR_TIMES_SQUARE_WEST_PATH
    };
    static const char * const toronto_paths[] = {
        ROCKBOX_DIR "/maps/toronto_360_north.rgb",
        ROCKBOX_DIR "/maps/toronto_360_east.rgb",
        ROCKBOX_DIR "/maps/toronto_360_south.rgb",
        ROCKBOX_DIR "/maps/toronto_360_west.rgb"
    };
    const char * const *paths = scene == MAR_SCENE_TIMES_SQUARE ?
                               times_square_paths :
                               scene == MAR_SCENE_TORONTO_360 ?
                               toronto_paths : NULL;
    int i;

    if (!paths)
        return false;
    if (mar_lookaround_scene == scene)
        return true;
    for (i = 0; i < 4; i++) {
        int fd = rb->open(paths[i], O_RDONLY);
        ssize_t got;

        if (fd < 0)
            return false;
        got = rb->read(fd, mar_visual_cache.lookaround[i],
                       sizeof(mar_visual_cache.lookaround[i]));
        rb->close(fd);
        if (got != (ssize_t)sizeof(mar_visual_cache.lookaround[i]))
            return false;
    }
    mar_lookaround_scene = scene;
    mar_satellite_zoom = -1;
    return true;
}

static int mar_world_frame_count(int scene)
{
    return scene == MAR_SCENE_TORONTO ? 3 :
           scene == MAR_SCENE_FREDERICTON ? 2 : 1;
}

static bool mar_load_world_dashcam(int scene, int frame)
{
    static const char * const toronto_paths[] = {
        ROCKBOX_DIR "/maps/toronto_panoramax_0.rgb",
        ROCKBOX_DIR "/maps/toronto_panoramax_1.rgb",
        ROCKBOX_DIR "/maps/toronto_panoramax_2.rgb"
    };
    static const char * const fredericton_paths[] = {
        ROCKBOX_DIR "/maps/fredericton_panoramax_0.rgb",
        ROCKBOX_DIR "/maps/fredericton_panoramax_1.rgb"
    };
    const char *path;
    int fd;
    ssize_t got;

    if (scene == MAR_SCENE_TORONTO) {
        if (frame < 0 || frame >= (int)ARRAYLEN(toronto_paths))
            return false;
        path = toronto_paths[frame];
    } else if (scene == MAR_SCENE_FREDERICTON) {
        if (frame < 0 || frame >= (int)ARRAYLEN(fredericton_paths))
            return false;
        path = fredericton_paths[frame];
    } else {
        path = scene == MAR_SCENE_LONDON ? MAR_LONDON_PATH :
               scene == MAR_SCENE_BERLIN ? MAR_BERLIN_PATH :
               scene == MAR_SCENE_PARIS ? MAR_PARIS_PATH :
               scene == MAR_SCENE_TOKYO ? MAR_TOKYO_PATH : NULL;
    }
    if (!path)
        return false;
    if (mar_world_dashcam_scene == scene && mar_world_dashcam_frame == frame)
        return true;
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;
    got = rb->read(fd, mar_satellite, sizeof(mar_satellite));
    rb->close(fd);
    if (got != (ssize_t)sizeof(mar_satellite))
        return false;
    mar_world_dashcam_scene = scene;
    mar_world_dashcam_frame = frame;
    return true;
}

static bool mar_street_available(const struct mar_view *view)
{
    if (view->selected_place < 0 ||
        view->selected_place >= (int)ARRAYLEN(mar_places))
        return false;
    return mar_street_path(view->selected_place) != NULL;
}

static bool mar_open_street(struct mar_view *view)
{
    if (!mar_street_available(view))
        return false;
    /* Storage is touched only on this explicit navigation action. */
    if (!mar_load_street(view->selected_place))
        return false;
    view->street_scene = MAR_SCENE_NONE;
    view->screen = MAR_SCREEN_STREET;
    return true;
}

/* Full dashcam playback belongs to the established MPEG player. Maps
 * deliberately never takes PCM ownership, so returning to music remains the
 * player's normal, tested lifecycle. */
static bool mar_open_dashcam_video(const char *video)
{
    char path[MAX_PATH];

    if (!video || !*video)
        return false;
    rb->snprintf(path, sizeof(path), "%s/%s", MAR_DASHCAM_VIDEO_DIR, video);
    if (!rb->file_exists(path)) {
        rb->splash(HZ * 2, "Dashcam video not synced");
        return false;
    }
    if (!rb->file_exists(MAR_VIDEO_PLAYER_PATH)) {
        rb->splash(HZ * 2, "MPEG player missing");
        return false;
    }
    return rb->plugin_open(MAR_VIDEO_PLAYER_PATH, path) == PLUGIN_OK;
}

static int mar_nearest_world_scene(const struct mar_view *view)
{
    unsigned i;
    int closest = -1;
    long best_distance = LONG_MAX;

    for (i = 0; i < ARRAYLEN(mar_world_scenes); i++) {
        const struct mar_world_scene *scene = &mar_world_scenes[i];
        int x;
        int y;
        int dx;
        int dy;
        long distance;

        if (!mar_world_scene_available[i])
            continue;
        mar_geo_to_world(scene->latitude_e6, scene->longitude_e6, &x, &y);
        dx = x - view->center_x;
        if (dx < 0)
            dx = -dx;
        if (dx > MAR_WORLD_W / 2)
            dx = MAR_WORLD_W - dx;
        dy = y - view->center_y;
        if (dy < 0)
            dy = -dy;
        distance = (long)dx * dx + (long)dy * dy;
        if (distance < best_distance) {
            best_distance = distance;
            closest = (int)i;
        }
    }
    return closest;
}

static bool mar_open_nearest_world_scene(struct mar_view *view)
{
    int index = mar_nearest_world_scene(view);
    const struct mar_world_scene *world_scene;
    int scene;

    if (index < 0)
        return false;
    world_scene = &mar_world_scenes[index];
    scene = world_scene->scene;
    if (world_scene->video)
        return mar_open_dashcam_video(world_scene->video);
    if (!mar_load_lookaround(scene))
        return false;
    view->selected_place = -1;
    view->street_scene = scene;
    view->street_heading = 0;
    view->screen = MAR_SCREEN_STREET;
    return true;
}

static void mar_draw_street_view(const struct mar_view *view)
{
    const struct mar_place *place = view->selected_place >= 0 ?
                                    &mar_places[view->selected_place] : NULL;
    const char *scene_name = "Times Square";
    static const char * const directions[] = { "N", "E", "S", "W" };
    char frame_text[24];
    int heading = view->street_heading & 3;

    if (place)
        scene_name = place->name;
    else if (view->street_scene != MAR_SCENE_TIMES_SQUARE) {
        unsigned i;
        for (i = 0; i < ARRAYLEN(mar_world_scenes); i++) {
            if (mar_world_scenes[i].scene == view->street_scene) {
                scene_name = mar_world_scenes[i].name;
                break;
            }
        }
    }

    if ((view->street_scene == MAR_SCENE_TIMES_SQUARE ||
         view->street_scene == MAR_SCENE_TORONTO_360) &&
        mar_lookaround_scene == view->street_scene) {
        rb->lcd_set_background(LCD_RGBPACK(235, 240, 241));
        rb->lcd_clear_display();
        rb->lcd_bitmap(mar_visual_cache.lookaround[heading], 0, MAR_MAP_Y,
                       LCD_WIDTH, MAR_STREET_H);
        mar_color(LCD_WHITE);
        rb->lcd_fillrect(0, 0, LCD_WIDTH, MAR_MAP_Y);
        mar_color(LCD_RGBPACK(213, 216, 218));
        rb->lcd_hline(0, LCD_WIDTH - 1, MAR_MAP_Y - 1);
        mar_color(MAR_TEXT);
        rb->lcd_putsxy(9, 6, "Look Around");
        rb->lcd_putsxy(184, 6, directions[heading]);
        rb->lcd_putsxy(207, 6, scene_name);
        mar_color(LCD_WHITE);
        rb->lcd_fillrect(12, LCD_HEIGHT - 31, 176, 18);
        mar_color(LCD_RGBPACK(205, 211, 214));
        rb->lcd_drawrect(12, LCD_HEIGHT - 31, 176, 18);
        mar_color(MAR_TEXT);
        rb->lcd_putsxy(19, LCD_HEIGHT - 27, "Real 360 imagery  <  >");
        return;
    }

    if ((view->street_scene == MAR_SCENE_NONE &&
         mar_street_place == view->selected_place) ||
        (view->street_scene != MAR_SCENE_NONE &&
         view->street_scene != MAR_SCENE_TIMES_SQUARE &&
         view->street_scene != MAR_SCENE_TORONTO_360 &&
         mar_world_dashcam_scene == view->street_scene)) {
        rb->lcd_set_background(LCD_RGBPACK(235, 240, 241));
        rb->lcd_clear_display();
        rb->lcd_bitmap(mar_satellite, 0, MAR_MAP_Y, LCD_WIDTH, MAR_STREET_H);
        mar_color(LCD_RGBPACK(255, 255, 255));
        rb->lcd_fillrect(0, 0, LCD_WIDTH, MAR_MAP_Y);
        mar_color(LCD_RGBPACK(213, 216, 218));
        rb->lcd_hline(0, LCD_WIDTH - 1, MAR_MAP_Y - 1);
        mar_color(MAR_TEXT);
        rb->lcd_putsxy(9, 6, view->street_scene != MAR_SCENE_NONE ?
                       "Street View" : "Satellite Detail");
        rb->lcd_putsxy(177, 6, scene_name);
        mar_color(LCD_RGBPACK(255, 255, 255));
        rb->lcd_fillrect(12, LCD_HEIGHT - 31, 152, 18);
        mar_color(LCD_RGBPACK(205, 211, 214));
        rb->lcd_drawrect(12, LCD_HEIGHT - 31, 152, 18);
        mar_color(MAR_TEXT);
        if (view->street_scene != MAR_SCENE_NONE &&
            mar_world_frame_count(view->street_scene) > 1) {
            rb->snprintf(frame_text, sizeof(frame_text), "Frame %d/%d  <  >",
                         mar_world_dashcam_frame + 1,
                         mar_world_frame_count(view->street_scene));
            rb->lcd_putsxy(19, LCD_HEIGHT - 27, frame_text);
        } else {
            rb->lcd_putsxy(19, LCD_HEIGHT - 27,
                           view->street_scene != MAR_SCENE_NONE ?
                           "Real dashcam view" : "Real satellite imagery");
        }
        return;
    }

    /* Never render synthetic scenery as a substitute for Street View. */
    rb->lcd_set_background(LCD_RGBPACK(250, 250, 250));
    rb->lcd_clear_display();
    mar_color(LCD_WHITE);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, MAR_MAP_Y);
    mar_color(LCD_RGBPACK(213, 216, 218));
    rb->lcd_hline(0, LCD_WIDTH - 1, MAR_MAP_Y - 1);
    mar_color(MAR_TEXT);
    rb->lcd_putsxy(9, 6, "Street View");
    rb->lcd_putsxy(198, 6, scene_name);
    mar_color(MAR_TEXT);
    rb->lcd_putsxy(52, 111, "Street imagery unavailable");
    mar_color(LCD_RGBPACK(106, 111, 114));
    rb->lcd_putsxy(33, 124, "Sync a real image pack to add it");
}

static void mar_draw_satellite(const struct mar_view *view)
{
    int tiles = 1 << view->zoom;
    int span_x = MAR_WORLD_W / tiles;
    int span_y = MAR_WORLD_H / tiles;
    int origin_x = view->center_x - span_x / 2;
    int origin_y = view->center_y - span_y / 2;
    int base_x;
    int base_y;
    int pixel_x;
    int pixel_y;

    while (origin_x < 0)
        origin_x += MAR_WORLD_W;
    while (origin_x >= MAR_WORLD_W)
        origin_x -= MAR_WORLD_W;
    if (origin_y < 0)
        origin_y = 0;
    else if (origin_y > MAR_WORLD_H - span_y)
        origin_y = MAR_WORLD_H - span_y;
    base_x = (origin_x * tiles) / MAR_WORLD_W;
    base_y = (origin_y * tiles) / MAR_WORLD_H;
    pixel_x = ((origin_x - (base_x * MAR_WORLD_W) / tiles) * MAR_SAT_W) /
              MAX(1, ((base_x + 1) * MAR_WORLD_W) / tiles -
              (base_x * MAR_WORLD_W) / tiles);
    pixel_y = ((origin_y - (base_y * MAR_WORLD_H) / tiles) * MAR_SAT_H) /
              MAX(1, ((base_y + 1) * MAR_WORLD_H) / tiles -
              (base_y * MAR_WORLD_H) / tiles);

    if (view->zoom == 0) {
        /* A globe frame is a complete orthographic Earth view. Do not wrap
         * its pixels: that would split and visibly duplicate the globe. */
        rb->lcd_bitmap_part(mar_visual_cache.map_tiles[0], 0, 0,
                            MAR_SAT_W, 0, MAR_MAP_Y,
                            MAR_SAT_W, MAR_SAT_H);
        return;
    }
    if (pixel_x < MAR_SAT_W && pixel_y < MAR_SAT_H)
        rb->lcd_bitmap_part(mar_visual_cache.map_tiles[0], pixel_x, pixel_y,
                            MAR_SAT_W, 0, MAR_MAP_Y,
                            MAR_SAT_W - pixel_x, MAR_SAT_H - pixel_y);
    if (pixel_x > 0 && pixel_y < MAR_SAT_H)
        rb->lcd_bitmap_part(mar_visual_cache.map_tiles[1], 0, pixel_y,
                            MAR_SAT_W, MAR_SAT_W - pixel_x, MAR_MAP_Y,
                            pixel_x, MAR_SAT_H - pixel_y);
    if (pixel_y > 0 && pixel_x < MAR_SAT_W)
        rb->lcd_bitmap_part(mar_visual_cache.map_tiles[2], pixel_x, 0,
                            MAR_SAT_W, 0, MAR_MAP_Y + MAR_SAT_H - pixel_y,
                            MAR_SAT_W - pixel_x, pixel_y);
    if (pixel_x > 0 && pixel_y > 0)
        rb->lcd_bitmap_part(mar_visual_cache.map_tiles[3], 0, 0,
                            MAR_SAT_W, MAR_SAT_W - pixel_x,
                            MAR_MAP_Y + MAR_SAT_H - pixel_y,
                            pixel_x, pixel_y);
}

static void mar_render(const struct mar_view *view)
{
    unsigned water = LCD_RGBPACK(44, 100, 132);

    rb->lcd_setfont(FONT_SYSFIXED);
    /* Retain the vector fallback code without painting it over satellite data. */
    if (false) {
        mar_draw_land(view);
        mar_draw_lines(view);
        mar_draw_places(view);
    }
    if (view->screen == MAR_SCREEN_STREET) {
        mar_draw_street_view(view);
        rb->lcd_update();
        return;
    }
    if (mar_satellite_zoom == view->zoom) {
        rb->lcd_set_background(LCD_RGBPACK(220, 238, 247));
        rb->lcd_clear_display();
        mar_draw_satellite(view);
        mar_draw_synced_items(view);
        mar_draw_selection(view);
        mar_draw_status(view);
        mar_draw_map_controls(view);
        if (view->screen == MAR_SCREEN_EXPLORE)
            mar_draw_explore(view);
        if (view->screen == MAR_SCREEN_WORLD)
            mar_draw_world(view);
        rb->lcd_update();
        return;
    }
    rb->lcd_set_background(water);
    rb->lcd_clear_display();
    mar_draw_synced_items(view);
    mar_draw_selection(view);
    mar_draw_status(view);
    mar_draw_map_controls(view);
    if (view->screen == MAR_SCREEN_EXPLORE)
        mar_draw_explore(view);
    if (view->screen == MAR_SCREEN_WORLD)
        mar_draw_world(view);
    rb->lcd_update();
}

static void mar_clamp(struct mar_view *view)
{
    while (view->center_x < 0)
        view->center_x += MAR_WORLD_W;
    while (view->center_x >= MAR_WORLD_W)
        view->center_x -= MAR_WORLD_W;
    if (view->center_y < 0)
        view->center_y = 0;
    else if (view->center_y > MAR_WORLD_H)
        view->center_y = MAR_WORLD_H;
}

static int mar_pan_step(const struct mar_view *view)
{
    /* Six screen pixels per click; repeats remain controllable at every zoom. */
    return MAX((6 << 8) / zoom_scale[view->zoom], 8);
}

static enum plugin_status mar_main(void)
{
    struct mar_view view = {
        .center_x = 3200,
        .center_y = 2346,
        .zoom = 0,
        .mode = MAR_MODE_SATELLITE,
        .labels = true,
        .selected_place = -1,
        .browse_index = 0,
        .street_heading = 0,
        .street_scene = MAR_SCENE_NONE,
        .screen = MAR_SCREEN_MAP,
    };
    bool dirty = true;

#if LCD_DEPTH > 1
    rb->lcd_set_backdrop(NULL);
#endif
    (void)mar_load_satellite(&view);
    mar_load_sync();
    mar_update_world_scene_availability();
    /* Do not interpret the launcher wheel release as an immediate zoom. */
    rb->button_clear_queue();
    while (true) {
        int button;
        int clean;
        int step;

        if (dirty) {
            mar_render(&view);
            dirty = false;
        }
        button = rb->button_get_w_tmo(HZ / 5);
#if defined(BUTTON_MENU) && defined(BUTTON_SELECT)
        if ((rb->button_status() & (BUTTON_MENU | BUTTON_SELECT)) ==
            (BUTTON_MENU | BUTTON_SELECT))
            return PLUGIN_OK;
#endif
        if (button == BUTTON_NONE)
            continue;
        clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
        step = mar_pan_step(&view);

        if (view.screen == MAR_SCREEN_STREET) {
            if (clean == BUTTON_LEFT && !(button & BUTTON_REL)) {
                if (view.street_scene == MAR_SCENE_TIMES_SQUARE ||
                    view.street_scene == MAR_SCENE_TORONTO_360) {
                    view.street_heading = (view.street_heading + 3) & 3;
                    dirty = true;
                } else if (view.street_scene != MAR_SCENE_NONE &&
                           mar_world_frame_count(view.street_scene) > 1) {
                    int count = mar_world_frame_count(view.street_scene);
                    int frame = (view.street_heading + count - 1) % count;
                    if (mar_load_world_dashcam(view.street_scene, frame)) {
                        view.street_heading = frame;
                        dirty = true;
                    }
                }
                continue;
            }
            if (clean == BUTTON_RIGHT && !(button & BUTTON_REL)) {
                if (view.street_scene == MAR_SCENE_TIMES_SQUARE ||
                    view.street_scene == MAR_SCENE_TORONTO_360) {
                    view.street_heading = (view.street_heading + 1) & 3;
                    dirty = true;
                } else if (view.street_scene != MAR_SCENE_NONE &&
                           mar_world_frame_count(view.street_scene) > 1) {
                    int count = mar_world_frame_count(view.street_scene);
                    int frame = (view.street_heading + 1) % count;
                    if (mar_load_world_dashcam(view.street_scene, frame)) {
                        view.street_heading = frame;
                        dirty = true;
                    }
                }
                continue;
            }
            if (clean == BUTTON_SELECT && !(button & BUTTON_REL)) {
                view.screen = MAR_SCREEN_MAP;
                view.street_scene = MAR_SCENE_NONE;
                (void)mar_load_satellite(&view);
                dirty = true;
                continue;
            }
#ifdef BUTTON_MENU
            if (clean == BUTTON_MENU && !(button & BUTTON_REL)) {
                view.screen = view.street_scene != MAR_SCENE_NONE ?
                              MAR_SCREEN_WORLD : MAR_SCREEN_EXPLORE;
                (void)mar_load_satellite(&view);
                dirty = true;
                continue;
            }
#endif
            exit_on_usb(button);
            continue;
        }

        if (view.screen == MAR_SCREEN_WORLD) {
#ifdef BUTTON_MENU
            if (clean == BUTTON_MENU && !(button & BUTTON_REL)) {
                view.screen = MAR_SCREEN_EXPLORE;
                dirty = true;
                continue;
            }
#endif
#ifdef HAVE_SCROLLWHEEL
            if (clean == BUTTON_SCROLL_FWD && !(button & BUTTON_REL)) {
                if (view.browse_index < mar_world_scene_count() - 1)
                    view.browse_index++;
                dirty = true;
                continue;
            }
            if (clean == BUTTON_SCROLL_BACK && !(button & BUTTON_REL)) {
                if (view.browse_index > 0)
                    view.browse_index--;
                dirty = true;
                continue;
            }
#endif
            if (clean == BUTTON_SELECT && !(button & BUTTON_REL)) {
                int scene_index = mar_world_scene_at(view.browse_index);
                const struct mar_world_scene *world_scene = scene_index >= 0 ?
                    &mar_world_scenes[scene_index] : NULL;
                if (!world_scene)
                    continue;
                int scene = world_scene->scene;

                if (world_scene->video) {
                    (void)mar_open_dashcam_video(world_scene->video);
                } else if (mar_load_lookaround(scene)) {
                    view.selected_place = -1;
                    view.street_scene = scene;
                    view.street_heading = 0;
                    view.screen = MAR_SCREEN_STREET;
                }
                dirty = true;
                continue;
            }
            exit_on_usb(button);
            continue;
        }

        if (view.screen == MAR_SCREEN_EXPLORE) {
            int count = mar_destination_count();

#ifdef BUTTON_MENU
            if (clean == BUTTON_MENU && !(button & BUTTON_REL)) {
                view.screen = MAR_SCREEN_MAP;
                dirty = true;
                continue;
            }
#endif
            if (clean == BUTTON_LEFT && !(button & BUTTON_REL)) {
                view.screen = MAR_SCREEN_WORLD;
                view.browse_index = 0;
                dirty = true;
                continue;
            }
#ifdef HAVE_SCROLLWHEEL
            if (clean == BUTTON_SCROLL_FWD && !(button & BUTTON_REL)) {
                if (view.browse_index < count - 1)
                    view.browse_index++;
                dirty = true;
                continue;
            }
            if (clean == BUTTON_SCROLL_BACK && !(button & BUTTON_REL)) {
                if (view.browse_index > 0)
                    view.browse_index--;
                dirty = true;
                continue;
            }
#endif
            if (clean == BUTTON_SELECT && !(button & BUTTON_REL)) {
                int place_index = mar_destination_at(view.browse_index);
                if (place_index >= 0) {
                    mar_focus_place(&view, place_index);
                    /* Explore changes the zoom and centre. Load that exact
                     * atlas view before its first draw, rather than showing
                     * the blue unavailable fallback from the globe cache. */
                    (void)mar_load_satellite(&view);
                }
                dirty = true;
                continue;
            }
            if (clean == BUTTON_RIGHT && !(button & BUTTON_REL)) {
                int place_index = mar_destination_at(view.browse_index);
                if (place_index >= 0) {
                    mar_focus_place(&view, place_index);
                    (void)mar_open_street(&view);
                }
                dirty = true;
                continue;
            }
            exit_on_usb(button);
            continue;
        }

#ifdef BUTTON_PLAY
        if (clean == BUTTON_PLAY && !(button & BUTTON_REL)) {
            view.screen = MAR_SCREEN_EXPLORE;
            dirty = true;
            continue;
        }
#endif
        if (clean == BUTTON_LEFT && !(button & BUTTON_REL)) {
            if (view.zoom == 0)
                view.center_x -= (MAR_WORLD_W * MAR_GLOBE_TURN_FRAMES) /
                                 MAR_GLOBE_FRAMES;
            else
                view.center_x -= step;
            mar_clamp(&view);
            (void)mar_load_satellite(&view);
            dirty = true;
        } else if (clean == BUTTON_RIGHT && !(button & BUTTON_REL)) {
            if (view.zoom == 0)
                view.center_x += (MAR_WORLD_W * MAR_GLOBE_TURN_FRAMES) /
                                 MAR_GLOBE_FRAMES;
            else
                view.center_x += step;
            mar_clamp(&view);
            (void)mar_load_satellite(&view);
            dirty = true;
        }
#ifdef BUTTON_MENU
        else if (clean == BUTTON_MENU && !(button & BUTTON_REL)) {
            view.center_y -= step;
            mar_clamp(&view);
            (void)mar_load_satellite(&view);
            dirty = true;
        }
#endif
#ifdef HAVE_SCROLLWHEEL
        else if (clean == BUTTON_SCROLL_FWD && !(button & BUTTON_REL)) {
            if (view.zoom < MAR_ZOOM_COUNT - 1)
                view.zoom++;
            (void)mar_load_satellite(&view);
            dirty = true;
        } else if (clean == BUTTON_SCROLL_BACK && !(button & BUTTON_REL)) {
            if (view.zoom > 0)
                view.zoom--;
            (void)mar_load_satellite(&view);
            dirty = true;
        }
#endif
        else if (clean == BUTTON_SELECT) {
            if (button & BUTTON_REPEAT) {
                /* Hold Select always chooses the nearest installed real
                 * Dashcam/360 scene to the map cursor. City Satellite Detail
                 * remains available from Explore with Right. */
                (void)mar_open_nearest_world_scene(&view);
            }
            else if (!(button & BUTTON_REL))
                view.labels = !view.labels;
            dirty = true;
        } else {
            exit_on_usb(button);
        }
    }
}

enum plugin_status plugin_start(const void *parameter)
{
    enum plugin_status status;

    (void)parameter;
    backlight_ignore_timeout();
    status = mar_main();
    backlight_use_settings();
    return status;
}

#else

enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;
    rb->splash(HZ * 2, "Maps needs colour 320x240+");
    return PLUGIN_ERROR;
}

#endif
