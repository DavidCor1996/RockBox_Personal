/***************************************************************************
 * Dynamic isometric level renderer, shared by town and the level-1
 * dungeon (see tools/diablo_assets/dungeon_gen.py for how the latter's
 * fixed room layout is authored).
 *
 * Ports tools/diablo_assets/dun_tile.py + town.py to C, but composites a
 * moving LCD_WIDTHxLCD_HEIGHT viewport on-device instead of one fixed
 * precomputed crop. The full decoded tile art doesn't fit in the plugin
 * buffer (a level's *.cel alone can decode to several MB across all
 * referenced frames), so only a small LRU-ish cache of recently-drawn
 * 32x32 micro blocks is kept in RAM; the rest stays in <name>.cel.raw on
 * disk and is decoded on demand. See tools/diablo_assets/world_pack.py
 * for the on-disk pack layout this reads.
 *
 * Town and the dungeon use different piece geometry (16 blocks/piece for
 * town, 10 for the cathedral tileset), so block/row/piece-height counts
 * are read from the pack header at load time rather than hardcoded.
 ****************************************************************************/
#include "world.h"

#define DIABLO_DATA_DIR PLUGIN_GAMES_DATA_DIR "/diablo"

#define META_MAGIC "DWLD"
#define META_BUF_SIZE (192 * 1024)

#define PIECE_COLS 2

#define STEP_X 32
#define STEP_Y 16

#define SQUARE 0
#define TRANSPARENT_SQUARE 1
#define LEFT_TRIANGLE 2
#define RIGHT_TRIANGLE 3
#define LEFT_TRAPEZOID 4
#define RIGHT_TRAPEZOID 5

#define SOL_SOLID 0x01

static const unsigned char triangle_row_widths[31] =
{
     2,  4,  6,  8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32,
    30, 28, 26, 24, 22, 20, 18, 16, 14, 12, 10,  8,  6,  4,  2
};

struct world_data
{
    unsigned char *buf;
    size_t buf_size;

    unsigned char *palette;        /* 768 bytes */
    int dpiece_w, dpiece_h;
    uint16_t *dpiece_grid;         /* dpiece_w * dpiece_h, index = x*dpiece_h+y */
    int num_pieces;
    int blocks, rows, piece_h;     /* piece geometry: cols is always 2 */
    uint16_t *min_data;            /* num_pieces * blocks */
    unsigned char *sol_data;       /* num_pieces */
    unsigned char *bounds;         /* num_pieces * 2: min_row, max_row */
    int num_frames;
    uint32_t *cel_offsets;         /* num_frames + 1 */

    int cel_fd;
};

static struct world_data world;
static unsigned char screen_idx[LCD_WIDTH * LCD_HEIGHT];

/* Camera transform established by the last world_render_background()
 * call; world_screen_pos() reuses it so entity overlays line up with
 * the tiles already drawn. */
static int cam_origin_x, cam_origin_y;

#define MICRO_CACHE_SLOTS 96
struct micro_slot
{
    int frame; /* -1 = empty slot */
    int type;
    unsigned char idx[1024];
    unsigned char mask[1024];
};
static struct micro_slot micro_cache[MICRO_CACHE_SLOTS];
static int micro_cache_next;

static uint32_t read_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_u16(const unsigned char *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

bool world_load(const char *name)
{
    int fd, n;
    unsigned char *header;
    char path[MAX_PATH];

    world_unload();

    world.buf = rb->plugin_get_buffer(&world.buf_size);
    if (!world.buf || world.buf_size < META_BUF_SIZE)
        return false;

    rb->snprintf(path, sizeof(path), "%s/%s.meta", DIABLO_DATA_DIR, name);
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;
    n = rb->read(fd, world.buf, META_BUF_SIZE);
    rb->close(fd);
    if (n < 40 || rb->memcmp(world.buf, META_MAGIC, 4) != 0)
        return false;

    header = world.buf;
    world.dpiece_w   = read_u16(header + 4);
    world.dpiece_h   = read_u16(header + 6);
    world.num_pieces = read_u16(header + 8);
    world.num_frames = read_u16(header + 10);
    world.blocks     = read_u16(header + 12);
    world.rows       = world.blocks / PIECE_COLS;
    world.piece_h    = world.rows * 32;

    world.palette     = world.buf + read_u32(header + 16);
    world.dpiece_grid = (uint16_t *)(world.buf + read_u32(header + 20));
    world.min_data    = (uint16_t *)(world.buf + read_u32(header + 24));
    world.sol_data    = world.buf + read_u32(header + 28);
    world.bounds      = world.buf + read_u32(header + 32);
    world.cel_offsets = (uint32_t *)(world.buf + read_u32(header + 36));

    rb->snprintf(path, sizeof(path), "%s/%s.cel.raw", DIABLO_DATA_DIR, name);
    world.cel_fd = rb->open(path, O_RDONLY);
    if (world.cel_fd < 0)
        return false;

    for (n = 0; n < MICRO_CACHE_SLOTS; n++)
        micro_cache[n].frame = -1;
    micro_cache_next = 0;

    return true;
}

void world_unload(void)
{
    if (world.cel_fd >= 0)
        rb->close(world.cel_fd);
    world.cel_fd = -1;
}

static int block_source_index(int b, int blocks)
{
    return blocks - 2 + (b & 1) - (b & 0xE);
}

static void decode_square(const unsigned char *src, unsigned char *out, unsigned char *mask)
{
    rb->memcpy(out, src, 1024);
    rb->memset(mask, 1, 1024);
}

static void decode_transparent_square(const unsigned char *src, int srclen,
                                       unsigned char *out, unsigned char *mask)
{
    int pos = 0, y;

    rb->memset(out, 0, 1024);
    rb->memset(mask, 0, 1024);
    for (y = 0; y < 32; y++)
    {
        int col = 0;
        while (col < 32 && pos < srclen)
        {
            signed char control = (signed char)src[pos++];
            if (control > 0)
            {
                int v = control;
                rb->memcpy(out + y * 32 + col, src + pos, v);
                rb->memset(mask + y * 32 + col, 1, v);
                pos += v;
                col += v;
            }
            else
            {
                col += -control;
            }
        }
    }
}

/* Shared row-walker for Triangle (31 rows) and the lower half of a
 * Trapezoid (16 rows). Returns the number of source bytes consumed. */
static int decode_triangle_rows(const unsigned char *src, bool left, int row_count,
                                 unsigned char *out, unsigned char *mask)
{
    int pos = 0, i;

    for (i = 0; i < row_count; i++)
    {
        int w = triangle_row_widths[i];
        int x0;

        if (left)
        {
            if ((i % 2) == 0)
                pos += 2; /* padding bytes before even rows */
            x0 = 32 - w;  /* right-aligned: tapers away from the seam */
            rb->memcpy(out + i * 32 + x0, src + pos, w);
            rb->memset(mask + i * 32 + x0, 1, w);
            pos += w;
        }
        else
        {
            x0 = 0; /* left-aligned: tapers away from the seam */
            rb->memcpy(out + i * 32 + x0, src + pos, w);
            rb->memset(mask + i * 32 + x0, 1, w);
            pos += w;
            if ((i % 2) == 0)
                pos += 2; /* padding bytes after even rows */
        }
    }
    return pos;
}

static void decode_triangle(const unsigned char *src, bool left,
                             unsigned char *out, unsigned char *mask)
{
    rb->memset(out, 0, 1024);
    rb->memset(mask, 0, 1024);
    decode_triangle_rows(src, left, 31, out, mask);
}

static void decode_trapezoid(const unsigned char *src, bool left,
                              unsigned char *out, unsigned char *mask)
{
    int pos, i;

    rb->memset(out, 0, 1024);
    rb->memset(mask, 0, 1024);
    pos = decode_triangle_rows(src, left, 16, out, mask);
    for (i = 16; i < 32; i++)
    {
        rb->memcpy(out + i * 32, src + pos, 32);
        rb->memset(mask + i * 32, 1, 32);
        pos += 32;
    }
}

static void get_micro(int frame, int type, const unsigned char **out_idx,
                       const unsigned char **out_mask)
{
    static unsigned char raw[1536];
    int i, len;
    uint32_t off0, off1;
    struct micro_slot *slot;

    for (i = 0; i < MICRO_CACHE_SLOTS; i++)
    {
        if (micro_cache[i].frame == frame && micro_cache[i].type == type)
        {
            *out_idx = micro_cache[i].idx;
            *out_mask = micro_cache[i].mask;
            return;
        }
    }

    off0 = world.cel_offsets[frame];
    off1 = world.cel_offsets[frame + 1];
    len = (int)(off1 - off0);
    if (len < 0)
        len = 0;
    if (len > (int)sizeof(raw))
        len = (int)sizeof(raw);

    rb->lseek(world.cel_fd, off0, SEEK_SET);
    rb->read(world.cel_fd, raw, len);

    slot = &micro_cache[micro_cache_next];
    switch (type)
    {
    case SQUARE:             decode_square(raw, slot->idx, slot->mask); break;
    case TRANSPARENT_SQUARE: decode_transparent_square(raw, len, slot->idx, slot->mask); break;
    case LEFT_TRIANGLE:      decode_triangle(raw, true, slot->idx, slot->mask); break;
    case RIGHT_TRIANGLE:     decode_triangle(raw, false, slot->idx, slot->mask); break;
    case LEFT_TRAPEZOID:     decode_trapezoid(raw, true, slot->idx, slot->mask); break;
    case RIGHT_TRAPEZOID:    decode_trapezoid(raw, false, slot->idx, slot->mask); break;
    default:
        rb->memset(slot->idx, 0, 1024);
        rb->memset(slot->mask, 0, 1024);
        break;
    }
    slot->frame = frame;
    slot->type = type;
    *out_idx = slot->idx;
    *out_mask = slot->mask;
    micro_cache_next = (micro_cache_next + 1) % MICRO_CACHE_SLOTS;
}

static void blit_32x32(const unsigned char *idx, const unsigned char *mask,
                        int dst_x0, int dst_y0)
{
    int y, x;

    for (y = 0; y < 32; y++)
    {
        int py = dst_y0 + y;
        int row_off, dst_row;
        if (py < 0 || py >= LCD_HEIGHT)
            continue;
        row_off = y * 32;
        dst_row = py * LCD_WIDTH;
        for (x = 0; x < 32; x++)
        {
            int px = dst_x0 + x;
            if (px < 0 || px >= LCD_WIDTH)
                continue;
            if (mask[row_off + x])
                screen_idx[dst_row + px] = idx[row_off + x];
        }
    }
}

static void blit_piece(int piece_index, int anchor_sx, int anchor_sy)
{
    int base, min_row, max_row, b;

    if (piece_index < 0 || piece_index >= world.num_pieces)
        return;

    base = piece_index * world.blocks;
    min_row = world.bounds[piece_index * 2];
    max_row = world.bounds[piece_index * 2 + 1];

    for (b = 0; b < world.blocks; b++)
    {
        int row = b / PIECE_COLS;
        int src_i, raw, frame, type, bx, by;
        const unsigned char *idx, *mask;

        if (row < min_row || row > max_row)
            continue;

        src_i = block_source_index(b, world.blocks);
        raw = world.min_data[base + src_i];
        if (raw == 0)
            continue;
        frame = raw & 0xFFF;
        if (frame == 0)
            continue;
        type = (raw >> 12) & 7;

        get_micro(frame - 1, type, &idx, &mask);
        bx = (b % PIECE_COLS) * 32;
        by = (world.rows - 1 - row) * 32;
        blit_32x32(idx, mask, anchor_sx + bx, anchor_sy + by);
    }
}

static int dpiece_at(int dpx, int dpy)
{
    if (dpx < 0 || dpx >= world.dpiece_w || dpy < 0 || dpy >= world.dpiece_h)
        return -1;
    return world.dpiece_grid[dpx * world.dpiece_h + dpy];
}

bool world_position_passable(int x_fp, int y_fp)
{
    int dpx = x_fp >> WORLD_FP_BITS;
    int dpy = y_fp >> WORLD_FP_BITS;
    int piece = dpiece_at(dpx, dpy);

    if (piece < 0)
        return false;
    return !(world.sol_data[piece] & SOL_SOLID);
}

void world_screen_pos(int x_fp, int y_fp, int *out_sx, int *out_sy)
{
    *out_sx = ((x_fp - y_fp) * STEP_X) >> WORLD_FP_BITS;
    *out_sy = ((x_fp + y_fp) * STEP_Y) >> WORLD_FP_BITS;
    *out_sx -= cam_origin_x;
    *out_sy -= cam_origin_y;
}

void world_render_background(int cam_x_fp, int cam_y_fp)
{
    /* Player's projected screen position, at fixed-point precision,
     * defines where the camera is centered. */
    int cam_sx = ((cam_x_fp - cam_y_fp) * STEP_X) >> WORLD_FP_BITS;
    int cam_sy = ((cam_x_fp + cam_y_fp) * STEP_Y) >> WORLD_FP_BITS;

    int player_cell_x = cam_x_fp >> WORLD_FP_BITS;
    int player_cell_y = cam_y_fp >> WORLD_FP_BITS;

    /* Generous fixed radius around the player: half the screen in dPiece
     * cells, plus margin for pieces whose art reaches far above their
     * own footprint (tall building walls). */
    int radius = (LCD_WIDTH / 2) / STEP_X + (LCD_HEIGHT / 2) / STEP_Y + 10;

    int x0 = player_cell_x - radius, x1 = player_cell_x + radius;
    int y0 = player_cell_y - radius, y1 = player_cell_y + radius;
    int diag; /* draw order key = dpx + dpy, back (small) to front (large) */

    cam_origin_x = cam_sx - LCD_WIDTH / 2;
    cam_origin_y = cam_sy - LCD_HEIGHT / 2;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > world.dpiece_w) x1 = world.dpiece_w;
    if (y1 > world.dpiece_h) y1 = world.dpiece_h;

    rb->memset(screen_idx, 0, sizeof(screen_idx));

    for (diag = x0 + y0; diag <= (x1 - 1) + (y1 - 1); diag++)
    {
        int dpx;
        for (dpx = x0; dpx < x1; dpx++)
        {
            int dpy = diag - dpx;
            int piece, anchor_sx, anchor_sy;

            if (dpy < y0 || dpy >= y1)
                continue;

            piece = world.dpiece_grid[dpx * world.dpiece_h + dpy];
            anchor_sx = (dpx - dpy) * STEP_X - cam_origin_x;
            anchor_sy = (dpx + dpy) * STEP_Y - world.piece_h + 32 - cam_origin_y;
            blit_piece(piece, anchor_sx, anchor_sy);
        }
    }

    {
        static fb_data row_buf[LCD_WIDTH];
        int y, x;

        for (y = 0; y < LCD_HEIGHT; y++)
        {
            const unsigned char *srow = screen_idx + y * LCD_WIDTH;
            for (x = 0; x < LCD_WIDTH; x++)
            {
                unsigned char p = srow[x];
                row_buf[x] = LCD_RGBPACK(world.palette[p * 3],
                                        world.palette[p * 3 + 1],
                                        world.palette[p * 3 + 2]);
            }
            rb->lcd_bitmap(row_buf, 0, y, LCD_WIDTH, 1);
        }
    }
}

void world_present(void)
{
    rb->lcd_update();
}
