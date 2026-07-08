/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * RuneScape Classic offline Lumbridge slice.
 *
 ****************************************************************************/

#include "plugin.h"
#include "lib/xlcd.h"
#include "lib/helper.h"

#if LCD_DEPTH >= 16 && LCD_WIDTH >= 320 && LCD_HEIGHT >= 240

#define RSC_DATA_DIR PLUGIN_GAMES_DATA_DIR "/runescape_classic"
#define RSC_MAP_RSC RSC_DATA_DIR "/lumbridge.rsc"
#define RSC_MAP_RSC_ALT ROCKBOX_DIR "/ipodjs/runescape_classic/lumbridge.rsc"
#define RSC_CHARACTER_BMP RSC_DATA_DIR "/character_male.bmp"
#define RSC_CHARACTER_BMP_0 RSC_DATA_DIR "/character_male_0.bmp"
#define RSC_CHARACTER_BMP_1 RSC_DATA_DIR "/character_male_1.bmp"
#define RSC_CHARACTER_BMP_2 RSC_DATA_DIR "/character_male_2.bmp"
#define RSC_CHARACTER_BMP_3 RSC_DATA_DIR "/character_male_3.bmp"
#define RSC_UI_TOP_BMP RSC_DATA_DIR "/ui_top.bmp"
#define RSC_UI_BOTTOM_BMP RSC_DATA_DIR "/ui_bottom.bmp"
#define RSC_CHARACTER_MAX_BYTES (16 * 1024)
#define RSC_CHARACTER_VIEWS 4
#define RSC_UI_MAX_BYTES (320 * 28 * 2)

#define RSC_MAP_VERSION 5
#define RSC_MAP_HEADER_BYTES 20
#define RSC_MAP_RECORD_BYTES 34
#define RSC_FACE_RECORD_BYTES 28
#define RSC_MAX_WORLD_SIZE 96
#define RSC_TILE_SIZE 128
#define RSC_VIEW_TOP 0
#define RSC_VIEW_BOTTOM 15
#define RSC_VIEW_RADIUS 18
#define RSC_CAMERA_DISTANCE 2200
#define RSC_CAMERA_HEIGHT 760
#define RSC_CAMERA_SCALE 430
#define RSC_HORIZON 26
#define RSC_MOUSE_STEP 10
#define RSC_MOUSE_REPEAT_STEP 18
#define RSC_WALK_TICKS 3
#define RSC_PATH_MAX (RSC_MAX_WORLD_SIZE * RSC_MAX_WORLD_SIZE)
#define RSC_VISIBLE_FACE_MAX 8192

#define RSC_FLAG_BLOCKED 0x01
#define RSC_FLAG_WALL 0x02
#define RSC_FLAG_OBJECT 0x04
#define RSC_BLOCKED_TILE (RSC_FLAG_BLOCKED | RSC_FLAG_WALL | RSC_FLAG_OBJECT)

#define RSC_MAP_W_PX_MAX (RSC_MAX_WORLD_SIZE * RSC_TILE_W)
#define RSC_MAP_H_PX_MAX (RSC_MAX_WORLD_SIZE * RSC_TILE_H)

#if (CONFIG_KEYPAD == IPOD_1G2G_PAD) || \
    (CONFIG_KEYPAD == IPOD_3G_PAD) || \
    (CONFIG_KEYPAD == IPOD_4G_PAD)
#define RSC_QUIT BUTTON_MENU
#define RSC_ACTION BUTTON_SELECT
#define RSC_UP BUTTON_SCROLL_BACK
#define RSC_DOWN BUTTON_SCROLL_FWD
#define RSC_LEFT BUTTON_LEFT
#define RSC_RIGHT BUTTON_RIGHT
#define RSC_INFO BUTTON_PLAY
#else
#define RSC_QUIT BUTTON_POWER
#define RSC_ACTION BUTTON_SELECT
#define RSC_UP BUTTON_UP
#define RSC_DOWN BUTTON_DOWN
#define RSC_LEFT BUTTON_LEFT
#define RSC_RIGHT BUTTON_RIGHT
#define RSC_INFO BUTTON_MENU
#endif

struct rsc_tile
{
    unsigned char colour;
    unsigned char height;
    unsigned char decoration;
    unsigned char flags;
    unsigned char wall_ns;
    unsigned char wall_ew;
    unsigned char wall_diag;
    unsigned char wall_id;
    unsigned char roof;
    unsigned char direction;
    unsigned char tile_type;
    unsigned char tile_blocking;
    unsigned char tile_r;
    unsigned char tile_g;
    unsigned char tile_b;
    unsigned char wall_ns_draw;
    unsigned char wall_ns_height;
    unsigned char wall_ns_r;
    unsigned char wall_ns_g;
    unsigned char wall_ns_b;
    unsigned char wall_ew_draw;
    unsigned char wall_ew_height;
    unsigned char wall_ew_r;
    unsigned char wall_ew_g;
    unsigned char wall_ew_b;
    unsigned char wall_diag_draw;
    unsigned char wall_diag_height;
    unsigned char wall_diag_r;
    unsigned char wall_diag_g;
    unsigned char wall_diag_b;
    unsigned char roof_height;
    unsigned short face_start;
    unsigned char face_count;
};

static struct bitmap character_bmp[RSC_CHARACTER_VIEWS];
static struct bitmap ui_top_bmp;
static struct bitmap ui_bottom_bmp;
static fb_data character_pixels[RSC_CHARACTER_VIEWS]
    [RSC_CHARACTER_MAX_BYTES / sizeof(fb_data)];
static fb_data ui_top_pixels[RSC_UI_MAX_BYTES / sizeof(fb_data)];
static fb_data ui_bottom_pixels[RSC_UI_MAX_BYTES / sizeof(fb_data)];
static bool character_loaded;
static bool ui_top_loaded;
static bool ui_bottom_loaded;
static struct rsc_tile map_tiles[RSC_MAX_WORLD_SIZE * RSC_MAX_WORLD_SIZE];
struct rsc_visible_face
{
    unsigned short face;
    short depth;
};

static struct rsc_visible_face visible_faces[RSC_VISIBLE_FACE_MAX];
static short path_prev[RSC_PATH_MAX];
static unsigned short path_queue[RSC_PATH_MAX];
static unsigned short walk_path[RSC_PATH_MAX];

static unsigned char *plugin_buffer;
static size_t plugin_buffer_size;
static const unsigned char *face_data;
static unsigned int face_count;

static int world_w;
static int world_h;
static int spawn_x;
static int spawn_y;
static int player_x;
static int player_y;
static int cursor_x;
static int cursor_y;
static int camera_x;
static int camera_y;
static int camera_rotation;
static int mouse_x;
static int mouse_y;
static int walk_path_len;
static int walk_path_pos;
static int walk_tick;
static const char *status;
static int status_ticks;

static const char *camera_names[] = {
    "Facing: North",
    "Facing: East",
    "Facing: South",
    "Facing: West",
};

static int max_int(int a, int b)
{
    return a > b ? a : b;
}

static int min_int(int a, int b)
{
    return a < b ? a : b;
}

static int clamp_int(int value, int min, int max)
{
    if (max < min)
        return min;
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}

static unsigned int read_le16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static short read_le16s(const unsigned char *p)
{
    return (short)read_le16(p);
}

static int tile_index(int x, int y)
{
    return y * world_w + x;
}

static bool read_file(const char *path, unsigned char *buffer, size_t buffer_size,
                     size_t *size_out)
{
    int fd;
    int rc;
    int file_size;
    size_t done;

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;

    file_size = rb->filesize(fd);
    if (file_size <= 0 || (size_t)file_size > buffer_size)
    {
        rb->close(fd);
        return false;
    }

    done = 0;
    while (done < (size_t)file_size)
    {
        rc = rb->read(fd, buffer + done, (size_t)file_size - done);
        if (rc <= 0)
        {
            rb->close(fd);
            return false;
        }

        done += rc;
    }

    rb->close(fd);
    if (size_out != NULL)
        *size_out = done;

    return true;
}

static bool rsc_open_map_file(int *fd_out)
{
    int fd;

    fd = rb->open(RSC_MAP_RSC, O_RDONLY);
    if (fd < 0)
        fd = rb->open(RSC_MAP_RSC_ALT, O_RDONLY);
    if (fd < 0)
        return false;

    *fd_out = fd;
    return true;
}

static size_t rsc_map_required_size_from_fd(int fd)
{
    unsigned char header[RSC_MAP_HEADER_BYTES];
    int file_size;
    int world_w;
    int world_h;
    unsigned int faces;
    size_t required;

    if (fd < 0)
        return 0;

    file_size = rb->filesize(fd);
    if (file_size < RSC_MAP_HEADER_BYTES)
        return 0;

    if (rb->lseek(fd, 0, SEEK_SET) < 0)
        return 0;
    if (rb->read(fd, header, sizeof(header)) != (int)sizeof(header))
        return 0;
    if (header[0] != 'R' || header[1] != 'S' ||
        header[2] != 'C' || header[3] != 'L')
        return 0;
    if (read_le16(header + 4) != RSC_MAP_VERSION)
        return 0;

    world_w = (int)read_le16(header + 6);
    world_h = (int)read_le16(header + 8);
    faces = read_le16(header + 14);
    if (world_w <= 0 || world_h <= 0 ||
        world_w > RSC_MAX_WORLD_SIZE || world_h > RSC_MAX_WORLD_SIZE)
        return 0;

    required = RSC_MAP_HEADER_BYTES +
               (size_t)world_w * (size_t)world_h * RSC_MAP_RECORD_BYTES +
               (size_t)faces * RSC_FACE_RECORD_BYTES;
    if (required > (size_t)file_size)
        return 0;

    return required;
}

static bool is_walkable(int x, int y)
{
    if (x < 0 || y < 0 || x >= world_w || y >= world_h)
        return false;

    return !((map_tiles[tile_index(x, y)].flags &
              (RSC_FLAG_BLOCKED | RSC_FLAG_OBJECT)) ||
             map_tiles[tile_index(x, y)].tile_blocking);
}

static void cancel_walk(void)
{
    walk_path_len = 0;
    walk_path_pos = 0;
    walk_tick = 0;
}

static bool find_walkable_spawn(int start_x, int start_y,
                               int *out_x, int *out_y)
{
    int x;
    int y;
    int radius;
    int best_x = -1;
    int best_y = -1;
    int max_radius = max_int(world_w, world_h);

    for (radius = 0; radius <= max_radius && best_x < 0; radius++)
    {
        for (y = -radius; y <= radius; y++)
        {
            for (x = -radius; x <= radius; x++)
            {
                int tx = start_x + x;
                int ty = start_y + y;

                if (tx < 0 || ty < 0 || tx >= world_w || ty >= world_h)
                    continue;

                if (is_walkable(tx, ty))
                {
                    best_x = tx;
                    best_y = ty;
                    break;
                }
            }

            if (best_x >= 0)
                break;
        }
    }

    if (best_x < 0)
        return false;

    *out_x = best_x;
    *out_y = best_y;
    return true;
}

static bool rsc_load_map(void)
{
    unsigned char header[RSC_MAP_HEADER_BYTES];
    unsigned char record[RSC_MAP_RECORD_BYTES];
    size_t required;
    size_t face_bytes;
    int fd;
    int file_size;
    int world_size;
    int i;

    if (!rsc_open_map_file(&fd))
        return false;

    file_size = rb->filesize(fd);
    if (file_size < RSC_MAP_HEADER_BYTES)
    {
        rb->close(fd);
        return false;
    }

    if (rb->read(fd, header, sizeof(header)) != (int)sizeof(header))
    {
        rb->close(fd);
        return false;
    }

    if (header[0] != 'R' || header[1] != 'S' ||
        header[2] != 'C' || header[3] != 'L')
    {
        rb->close(fd);
        return false;
    }

    if (read_le16(header + 4) != RSC_MAP_VERSION)
    {
        rb->close(fd);
        return false;
    }

    world_w = read_le16(header + 6);
    world_h = read_le16(header + 8);
    spawn_x = read_le16(header + 10);
    spawn_y = read_le16(header + 12);
    face_count = read_le16(header + 14);

    if (world_w <= 0 || world_h <= 0 ||
        world_w > RSC_MAX_WORLD_SIZE || world_h > RSC_MAX_WORLD_SIZE)
    {
        rb->close(fd);
        return false;
    }

    required = RSC_MAP_HEADER_BYTES +
               (size_t)world_w * world_h * RSC_MAP_RECORD_BYTES +
               (size_t)face_count * RSC_FACE_RECORD_BYTES;
    if ((size_t)file_size < required)
    {
        rb->close(fd);
        return false;
    }

    for (i = 0; i < world_w * world_h; i++)
    {
        if (rb->read(fd, record, sizeof(record)) != (int)sizeof(record))
        {
            rb->close(fd);
            return false;
        }

        map_tiles[i].colour = record[0];
        map_tiles[i].height = record[1];
        map_tiles[i].decoration = record[2];
        map_tiles[i].flags = record[3];
        map_tiles[i].wall_ns = record[4];
        map_tiles[i].wall_ew = record[5];
        map_tiles[i].wall_diag = record[6];
        map_tiles[i].wall_id = record[7];
        map_tiles[i].roof = record[8];
        map_tiles[i].direction = record[9];
        map_tiles[i].tile_type = record[10];
        map_tiles[i].tile_blocking = record[11];
        map_tiles[i].tile_r = record[12];
        map_tiles[i].tile_g = record[13];
        map_tiles[i].tile_b = record[14];
        map_tiles[i].wall_ns_draw = record[15];
        map_tiles[i].wall_ns_height = record[16];
        map_tiles[i].wall_ns_r = record[17];
        map_tiles[i].wall_ns_g = record[18];
        map_tiles[i].wall_ns_b = record[19];
        map_tiles[i].wall_ew_draw = record[20];
        map_tiles[i].wall_ew_height = record[21];
        map_tiles[i].wall_ew_r = record[22];
        map_tiles[i].wall_ew_g = record[23];
        map_tiles[i].wall_ew_b = record[24];
        map_tiles[i].wall_diag_draw = record[25];
        map_tiles[i].wall_diag_height = record[26];
        map_tiles[i].wall_diag_r = record[27];
        map_tiles[i].wall_diag_g = record[28];
        map_tiles[i].wall_diag_b = record[29];
        map_tiles[i].roof_height = record[30];
        map_tiles[i].face_start = read_le16(record + 31);
        map_tiles[i].face_count = record[33];
        if (map_tiles[i].face_count &&
            (unsigned int)map_tiles[i].face_start +
            map_tiles[i].face_count > face_count)
        {
            rb->close(fd);
            return false;
        }
    }

    face_bytes = (size_t)face_count * RSC_FACE_RECORD_BYTES;
    plugin_buffer = rb->plugin_get_buffer(&plugin_buffer_size);
    if (plugin_buffer == NULL || plugin_buffer_size < face_bytes)
    {
        rb->close(fd);
        return false;
    }

    if (face_bytes > 0 &&
        rb->read(fd, plugin_buffer, face_bytes) != (int)face_bytes)
    {
        rb->close(fd);
        return false;
    }

    rb->close(fd);
    face_data = plugin_buffer;

    if (!is_walkable(spawn_x, spawn_y))
    {
        world_size = max_int(world_w, world_h);
        while (world_size-- > 0)
        {
            if (find_walkable_spawn(spawn_x, spawn_y, &spawn_x, &spawn_y))
                break;
        }
        if (world_size <= 0 && !is_walkable(spawn_x, spawn_y))
            return false;
    }

    player_x = spawn_x;
    player_y = spawn_y;
    cursor_x = spawn_x;
    cursor_y = spawn_y;
    mouse_x = LCD_WIDTH / 2;
    mouse_y = (LCD_HEIGHT - RSC_VIEW_BOTTOM) / 2;
    cancel_walk();

    camera_rotation = 0;
    return true;
}

static bool load_bitmap_file(const char *path, struct bitmap *bmp,
                             fb_data *pixels, size_t pixel_bytes)
{
    int needed;
    int rc;

    rb->memset(bmp, 0, sizeof(*bmp));
    rb->memset(pixels, 0, pixel_bytes);

    needed = rb->read_bmp_file(path, bmp,
                               0, FORMAT_NATIVE | FORMAT_RETURN_SIZE, NULL);
    if (needed <= 0 || needed > (int)pixel_bytes)
        return false;

    bmp->data = (char *)pixels;
    rc = rb->read_bmp_file(path, bmp, needed, FORMAT_NATIVE, NULL);
    if (rc <= 0 || bmp->width <= 0 || bmp->height <= 0)
        return false;

    return true;
}

static bool load_character(void)
{
    static const char *paths[RSC_CHARACTER_VIEWS] = {
        RSC_CHARACTER_BMP_0,
        RSC_CHARACTER_BMP_1,
        RSC_CHARACTER_BMP_2,
        RSC_CHARACTER_BMP_3,
    };
    int i;

    character_loaded = true;
    for (i = 0; i < RSC_CHARACTER_VIEWS; i++)
    {
        if (!load_bitmap_file(paths[i], &character_bmp[i],
                              character_pixels[i],
                              sizeof(character_pixels[i])))
        {
            character_loaded = false;
            break;
        }
    }

    if (!character_loaded)
    {
        character_loaded = load_bitmap_file(RSC_CHARACTER_BMP,
                                            &character_bmp[0],
                                            character_pixels[0],
                                            sizeof(character_pixels[0]));
    }

    return character_loaded;
}

static void load_ui(void)
{
    ui_top_loaded = load_bitmap_file(RSC_UI_TOP_BMP, &ui_top_bmp,
                                     ui_top_pixels, sizeof(ui_top_pixels));
    ui_bottom_loaded = load_bitmap_file(RSC_UI_BOTTOM_BMP, &ui_bottom_bmp,
                                        ui_bottom_pixels,
                                        sizeof(ui_bottom_pixels));
}

static void set_status(const char *message)
{
    if (status == message)
        return;

    status = message;
    status_ticks = HZ * 2;
}

static void rsc_update_camera(void)
{
    camera_x = player_x;
    camera_y = player_y;
}

static void draw_text_shadow(int x, int y, const char *text)
{
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_putsxy(x + 1, y + 1, text);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_putsxy(x, y, text);
}

static void unorient_delta(int rx, int ry, int *dx, int *dy)
{
    switch (camera_rotation)
    {
        case 1:
            *dx = -ry;
            *dy = rx;
            break;
        case 2:
            *dx = -rx;
            *dy = -ry;
            break;
        case 3:
            *dx = ry;
            *dy = -rx;
            break;
        case 0:
        default:
            *dx = rx;
            *dy = ry;
            break;
    }
}

static int vertex_height(int x, int y)
{
    x = clamp_int(x, 0, world_w - 1);
    y = clamp_int(y, 0, world_h - 1);
    return ((int)map_tiles[tile_index(x, y)].height - 96) * 3;
}

static void rotate_world_units(int dx, int dy, int *rx, int *rz)
{
    switch (camera_rotation)
    {
        case 1:
            *rx = dy;
            *rz = -dx;
            break;
        case 2:
            *rx = -dx;
            *rz = -dy;
            break;
        case 3:
            *rx = -dy;
            *rz = dx;
            break;
        case 0:
        default:
            *rx = dx;
            *rz = dy;
            break;
    }
}

static bool project_world_units(int wx, int wy, int height,
                                int *sx, int *sy, int *depth_out)
{
    int rx;
    int rz;
    int depth;
    int dx = wx - camera_x * RSC_TILE_SIZE;
    int dy = wy - camera_y * RSC_TILE_SIZE;

    rotate_world_units(dx, dy, &rx, &rz);
    depth = rz + RSC_CAMERA_DISTANCE;
    if (depth < 220)
        return false;

    *sx = LCD_WIDTH / 2 + (rx * RSC_CAMERA_SCALE) / depth;
    *sy = RSC_HORIZON +
          ((RSC_CAMERA_HEIGHT - height) * RSC_CAMERA_SCALE) / depth;
    if (depth_out != NULL)
        *depth_out = depth;

    return *sx > -80 && *sx < LCD_WIDTH + 80 &&
           *sy > -80 && *sy < LCD_HEIGHT + 80;
}

static bool project_tile_center(int x, int y, int *sx, int *sy, int *depth)
{
    int height;

    if (x < 0 || y < 0 || x >= world_w || y >= world_h)
        return false;

    height = vertex_height(x, y);
    return project_world_units(x * RSC_TILE_SIZE + RSC_TILE_SIZE / 2,
                               y * RSC_TILE_SIZE + RSC_TILE_SIZE / 2,
                               height, sx, sy, depth);
}

static void draw_face(unsigned int face_index)
{
    const unsigned char *raw;
    unsigned char count;
    fb_data color;
    int sx[4];
    int sy[4];
    int depth;
    int i;

    if (face_index >= face_count)
        return;

    raw = face_data + (size_t)face_index * RSC_FACE_RECORD_BYTES;
    count = raw[0];
    if (count < 3 || count > 4)
        return;

    for (i = 0; i < count; i++)
    {
        const unsigned char *v = raw + 4 + i * 6;
        if (!project_world_units(read_le16s(v), read_le16s(v + 2),
                                 read_le16s(v + 4),
                                 &sx[i], &sy[i], &depth))
            return;
    }

    color = LCD_RGBPACK(raw[1], raw[2], raw[3]);
    rb->lcd_set_foreground(color);
    xlcd_filltriangle(sx[0], sy[0], sx[1], sy[1], sx[2], sy[2]);
    if (count == 4)
        xlcd_filltriangle(sx[0], sy[0], sx[2], sy[2], sx[3], sy[3]);
}

static int face_depth(unsigned int face_index)
{
    const unsigned char *raw;
    unsigned char count;
    int i;
    int total = 0;

    if (face_index >= face_count)
        return -1;

    raw = face_data + (size_t)face_index * RSC_FACE_RECORD_BYTES;
    count = raw[0];
    if (count < 3 || count > 4)
        return -1;

    for (i = 0; i < count; i++)
    {
        const unsigned char *v = raw + 4 + i * 6;
        int rx;
        int rz;
        int wx = read_le16s(v);
        int wy = read_le16s(v + 2);
        rotate_world_units(wx - camera_x * RSC_TILE_SIZE,
                           wy - camera_y * RSC_TILE_SIZE, &rx, &rz);
        (void)rx;
        rz += RSC_CAMERA_DISTANCE;
        if (rz < 220)
            return -1;
        total += rz;
    }

    return total / count;
}

static int collect_visible_faces(void)
{
    int count = 0;
    int rz;
    int rx;

    for (rz = RSC_VIEW_RADIUS; rz >= -RSC_VIEW_RADIUS; rz--)
    {
        for (rx = -RSC_VIEW_RADIUS; rx <= RSC_VIEW_RADIUS; rx++)
        {
            int dx;
            int dy;
            int x;
            int y;
            unsigned int i;
            const struct rsc_tile *tile;

            unorient_delta(rx, rz, &dx, &dy);
            x = camera_x + dx;
            y = camera_y + dy;
            if (x < 0 || y < 0 || x >= world_w || y >= world_h)
                continue;

            tile = &map_tiles[tile_index(x, y)];
            for (i = 0; i < tile->face_count; i++)
            {
                unsigned int face = (unsigned int)tile->face_start + i;
                int depth = face_depth(face);

                if (depth < 0 || count >= RSC_VISIBLE_FACE_MAX)
                    continue;

                visible_faces[count].face = (unsigned short)face;
                visible_faces[count].depth = (short)min_int(depth, 32767);
                count++;
            }
        }
    }

    return count;
}

static void sort_visible_faces(int count)
{
    int gap;

    for (gap = count / 2; gap > 0; gap /= 2)
    {
        int i;
        for (i = gap; i < count; i++)
        {
            int j;
            struct rsc_visible_face temp = visible_faces[i];
            for (j = i; j >= gap &&
                 visible_faces[j - gap].depth < temp.depth; j -= gap)
                visible_faces[j] = visible_faces[j - gap];
            visible_faces[j] = temp;
        }
    }
}

static void draw_world(void)
{
    int count;
    int i;

    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT - RSC_VIEW_BOTTOM);

    rb->lcd_set_foreground(LCD_RGBPACK(20, 18, 8));
    rb->lcd_fillrect(0, RSC_HORIZON, LCD_WIDTH,
                     LCD_HEIGHT - RSC_VIEW_BOTTOM - RSC_HORIZON);

    count = collect_visible_faces();
    sort_visible_faces(count);
    for (i = 0; i < count; i++)
        draw_face(visible_faces[i].face);
}

static void draw_player(void)
{
    int x;
    int y;
    int depth;

    if (!project_tile_center(player_x, player_y, &x, &y, &depth))
        return;

    if (!character_loaded)
    {
        y -= 8;
        rb->lcd_set_foreground(LCD_BLACK);
        rb->lcd_drawline(x - 7, y, x + 7, y);
        rb->lcd_drawline(x, y - 7, x, y + 7);
        rb->lcd_drawrect(x - 4, y - 4, 9, 9);
        rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_drawline(x - 6, y, x + 6, y);
        rb->lcd_drawline(x, y - 6, x, y + 6);
        rb->lcd_drawrect(x - 3, y - 3, 7, 7);
        return;
    }

    {
        int view = camera_rotation & (RSC_CHARACTER_VIEWS - 1);
        const struct bitmap *bmp = &character_bmp[view];
        if (bmp->width <= 0 || bmp->height <= 0)
            bmp = &character_bmp[0];

        x = x - bmp->width / 2;
        y = y - bmp->height;
        if (x <= -bmp->width || y <= -bmp->height ||
            x >= LCD_WIDTH || y >= LCD_HEIGHT)
            return;

        rb->lcd_bitmap_transparent((const fb_data *)bmp->data, x, y,
                                  bmp->width, bmp->height);
    }
}

static void draw_cursor(void)
{
    int x;
    int y;
    int depth;

    if (!project_tile_center(cursor_x, cursor_y, &x, &y, &depth))
        return;

    rb->lcd_set_foreground(LCD_RGBPACK(255, 230, 80));
    rb->lcd_drawline(x - 7, y, x + 7, y);
    rb->lcd_drawline(x, y - 7, x, y + 7);
    rb->lcd_drawrect(x - 4, y - 4, 9, 9);

    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_drawline(mouse_x + 1, mouse_y + 1, mouse_x + 10, mouse_y + 6);
    rb->lcd_drawline(mouse_x + 1, mouse_y + 1, mouse_x + 5, mouse_y + 12);
    rb->lcd_drawline(mouse_x + 5, mouse_y + 12, mouse_x + 7, mouse_y + 8);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_drawline(mouse_x, mouse_y, mouse_x + 9, mouse_y + 5);
    rb->lcd_drawline(mouse_x, mouse_y, mouse_x + 4, mouse_y + 11);
    rb->lcd_drawline(mouse_x + 4, mouse_y + 11, mouse_x + 6, mouse_y + 7);
}

static void draw_ui_frame(void)
{
    (void)ui_top_loaded;
    (void)ui_top_bmp;

    if (ui_bottom_loaded)
    {
        int y = LCD_HEIGHT - ui_bottom_bmp.height;
        rb->lcd_bitmap((const fb_data *)ui_bottom_bmp.data, 0, y,
                       ui_bottom_bmp.width, ui_bottom_bmp.height);
    }
    else
    {
        rb->lcd_set_foreground(LCD_RGBPACK(0, 86, 103));
        rb->lcd_fillrect(0, LCD_HEIGHT - RSC_VIEW_BOTTOM, LCD_WIDTH,
                         RSC_VIEW_BOTTOM);
    }
}

static void draw_frame(void)
{
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_clear_display();

    draw_world();
    draw_player();
    draw_cursor();
    draw_ui_frame();

    if (status_ticks > 0 && status != NULL)
        draw_text_shadow(6, LCD_HEIGHT - RSC_VIEW_BOTTOM + 2, status);

    rb->lcd_update();
}

static bool build_walk_path(int start_x, int start_y, int dest_x, int dest_y)
{
    int total = world_w * world_h;
    int head = 0;
    int tail = 0;
    int dest;
    int start;
    int current;
    int len;
    int i;

    cancel_walk();

    if (!is_walkable(dest_x, dest_y))
        return false;

    start = tile_index(start_x, start_y);
    dest = tile_index(dest_x, dest_y);
    if (start == dest)
        return true;

    for (i = 0; i < total; i++)
        path_prev[i] = -1;

    path_prev[start] = -2;
    path_queue[tail++] = (unsigned short)start;

    while (head < tail && path_prev[dest] < 0)
    {
        static const signed char dx[4] = {1, -1, 0, 0};
        static const signed char dy[4] = {0, 0, 1, -1};
        int x;
        int y;
        int dir;

        current = path_queue[head++];
        x = current % world_w;
        y = current / world_w;

        for (dir = 0; dir < 4; dir++)
        {
            int nx = x + dx[dir];
            int ny = y + dy[dir];
            int next;

            if (!is_walkable(nx, ny))
                continue;

            next = tile_index(nx, ny);
            if (path_prev[next] != -1)
                continue;

            path_prev[next] = (short)current;
            path_queue[tail++] = (unsigned short)next;
            if (next == dest)
                break;
        }
    }

    if (path_prev[dest] < 0)
        return false;

    len = 0;
    current = dest;
    while (current != start && len < RSC_PATH_MAX)
    {
        walk_path[len++] = (unsigned short)current;
        current = path_prev[current];
    }

    if (current != start)
        return false;

    for (i = 0; i < len / 2; i++)
    {
        unsigned short swap = walk_path[i];
        walk_path[i] = walk_path[len - i - 1];
        walk_path[len - i - 1] = swap;
    }

    walk_path_len = len;
    walk_path_pos = 0;
    walk_tick = 0;
    return true;
}

static void rsc_update_walk(void)
{
    int next;

    if (walk_path_pos >= walk_path_len)
        return;

    if (++walk_tick < RSC_WALK_TICKS)
        return;

    walk_tick = 0;
    next = walk_path[walk_path_pos++];
    player_x = next % world_w;
    player_y = next / world_w;

    if (walk_path_pos >= walk_path_len)
        set_status("Arrived");
}

static void update_cursor_from_mouse(void)
{
    int best_x = cursor_x;
    int best_y = cursor_y;
    int best_dist = 36 * 36;
    int x;
    int y;

    for (y = max_int(0, player_y - RSC_VIEW_RADIUS);
         y < min_int(world_h, player_y + RSC_VIEW_RADIUS); y++)
    {
        for (x = max_int(0, player_x - RSC_VIEW_RADIUS);
             x < min_int(world_w, player_x + RSC_VIEW_RADIUS); x++)
        {
            int sx;
            int sy;
            int depth;
            int dx;
            int dy;
            int dist;

            if (!is_walkable(x, y) ||
                !project_tile_center(x, y, &sx, &sy, &depth))
                continue;

            dx = sx - mouse_x;
            dy = sy - mouse_y;
            dist = dx * dx + dy * dy;
            if (dist < best_dist)
            {
                best_dist = dist;
                best_x = x;
                best_y = y;
            }
        }
    }

    cursor_x = best_x;
    cursor_y = best_y;
}

static void move_mouse(int dx, int dy)
{
    mouse_x = clamp_int(mouse_x + dx, 0, LCD_WIDTH - 12);
    mouse_y = clamp_int(mouse_y + dy, RSC_VIEW_TOP,
                        LCD_HEIGHT - RSC_VIEW_BOTTOM - 12);
    update_cursor_from_mouse();
}

static void rsc_rotate_camera(int delta)
{
    int next = (camera_rotation + delta) & 3;

    if (next == camera_rotation)
        return;

    camera_rotation = next;
    set_status(camera_names[camera_rotation]);
}

static void handle_button(int button)
{
    int clean = button & ~(BUTTON_REPEAT | BUTTON_REL);

    if (clean == 0 || (button & BUTTON_REL))
        return;

    if (button & BUTTON_REPEAT)
    {
        switch (clean)
        {
            case RSC_UP:
                move_mouse(0, -RSC_MOUSE_REPEAT_STEP);
                break;
            case RSC_DOWN:
                move_mouse(0, RSC_MOUSE_REPEAT_STEP);
                break;
            case RSC_LEFT:
                rsc_rotate_camera(-1);
                break;
            case RSC_RIGHT:
                rsc_rotate_camera(1);
                break;
            default:
                break;
        }
        return;
    }

    switch (clean)
    {
        case RSC_UP:
            move_mouse(0, -RSC_MOUSE_STEP);
            break;
        case RSC_DOWN:
            move_mouse(0, RSC_MOUSE_STEP);
            break;
        case RSC_LEFT:
            move_mouse(-RSC_MOUSE_STEP, 0);
            break;
        case RSC_RIGHT:
            move_mouse(RSC_MOUSE_STEP, 0);
            break;
        case RSC_ACTION:
            update_cursor_from_mouse();
            if (build_walk_path(player_x, player_y, cursor_x, cursor_y))
                set_status("Walking");
            else
                set_status("Not walkable");
            break;
        case RSC_INFO:
            set_status("RuneScape offline map");
            break;
        default:
            break;
    }
}

enum plugin_status plugin_start(const void *parameter)
{
    int button;
    int map_fd;
    size_t map_required;
    (void)parameter;

    rb->lcd_set_backdrop(NULL);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    backlight_ignore_timeout();

    if (!rsc_open_map_file(&map_fd))
    {
        rb->splash(HZ * 3, "RuneScape map missing");
        backlight_use_settings();
        return PLUGIN_ERROR;
    }

    map_required = rsc_map_required_size_from_fd(map_fd);
    rb->close(map_fd);
    if (map_required == 0)
    {
        rb->splash(HZ * 3, "RuneScape map invalid");
        backlight_use_settings();
        return PLUGIN_ERROR;
    }

    if (!rsc_load_map())
    {
        rb->splash(HZ * 3, "RuneScape map load failed");
        backlight_use_settings();
        return PLUGIN_ERROR;
    }

    rsc_update_camera();
    load_character();
    load_ui();

    if (character_loaded)
        set_status("Lumbridge");
    else
        set_status("Default character unavailable");

    while (true)
    {
        rsc_update_walk();
        rsc_update_camera();
        draw_frame();

        if (status_ticks > 0)
            status_ticks -= HZ / 20;

        button = rb->button_get_w_tmo(HZ / 20);
        if (button == SYS_USB_CONNECTED)
        {
            backlight_use_settings();
            return PLUGIN_USB_CONNECTED;
        }

        if ((button & ~(BUTTON_REPEAT | BUTTON_REL)) == RSC_QUIT)
            break;

        if (button != BUTTON_NONE)
            handle_button(button);
    }

    backlight_use_settings();
    return PLUGIN_OK;
}

#else

enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;
    rb->splash(HZ * 2, "RuneScape Classic needs 320x240 color");
    return PLUGIN_OK;
}

#endif
