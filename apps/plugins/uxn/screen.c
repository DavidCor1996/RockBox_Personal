#include "screen.h"

/*
 * Screen semantics and blending table are adapted from the official Uxn C
 * emulator at commit 156ef01226c069ad5930c2655a10f22380a387a8.
 * See LICENSE.uxn.
 */

struct uxn_screen uxn_screen;

static const uint8_t blending[4][16] = {
    {0, 0, 0, 0, 1, 0, 1, 1, 2, 2, 0, 2, 3, 3, 3, 0},
    {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3},
    {1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1},
    {2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2}
};

static int clamp_int(int value, int low, int high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

static int signed_short(int value)
{
    return value & 0x8000 ? value - 0x10000 : value;
}

static int layer_offset(int x, int y)
{
    return x + UXN_SCREEN_MARGIN +
        (y + UXN_SCREEN_MARGIN) * UXN_SCREEN_STRIDE;
}

static void screen_resize(int width, int height)
{
    width = clamp_int(width, 8, UXN_SCREEN_MAX_WIDTH);
    height = clamp_int(height, 8, UXN_SCREEN_MAX_HEIGHT);
    if (width == uxn_screen.width && height == uxn_screen.height)
        return;

    uxn_screen.width = width;
    uxn_screen.height = height;
    rb->memset(uxn_screen.bg, 0, UXN_SCREEN_LAYER_SIZE);
    rb->memset(uxn_screen.fg, 0, UXN_SCREEN_LAYER_SIZE);
    uxn_screen.dirty = true;
}

void screen_init(uint8_t *memory)
{
    rb->memset(&uxn_screen, 0, sizeof(uxn_screen));
    uxn_screen.bg = memory;
    uxn_screen.fg = memory + UXN_SCREEN_LAYER_SIZE;
    uxn_screen.output = (fb_data *)(memory + UXN_SCREEN_LAYER_SIZE * 2);
    uxn_screen.width = LCD_WIDTH;
    uxn_screen.height = LCD_HEIGHT;
    rb->memset(memory, 0, UXN_SCREEN_LAYER_SIZE * 2);
    screen_palette();
}

void screen_palette(void)
{
    int i;
    int shift;
    fb_data colors[4];

    for (i = 0, shift = 4; i < 4; i++, shift ^= 4)
    {
        uint8_t r = (uxn.dev[0x08 + i / 2] >> shift) & 0xf;
        uint8_t g = (uxn.dev[0x0a + i / 2] >> shift) & 0xf;
        uint8_t b = (uxn.dev[0x0c + i / 2] >> shift) & 0xf;

        colors[i] = LCD_RGBPACK(r | r << 4, g | g << 4, b | b << 4);
    }
    for (i = 0; i < 16; i++)
        uxn_screen.palette[i] = colors[(i >> 2) ? i >> 2 : i & 3];
    uxn_screen.dirty = true;
}

void screen_redraw(void)
{
    int dest_w = uxn_screen.width;
    int dest_h = uxn_screen.height;
    int offset_x;
    int offset_y;
    int x;
    int y;

    if (!uxn_screen.dirty)
        return;

    if (dest_w > LCD_WIDTH || dest_h > LCD_HEIGHT)
    {
        dest_w = LCD_WIDTH;
        dest_h = uxn_screen.height * LCD_WIDTH / uxn_screen.width;
        if (dest_h > LCD_HEIGHT)
        {
            dest_h = LCD_HEIGHT;
            dest_w = uxn_screen.width * LCD_HEIGHT / uxn_screen.height;
        }
    }

    offset_x = (LCD_WIDTH - dest_w) / 2;
    offset_y = (LCD_HEIGHT - dest_h) / 2;
    for (y = 0; y < LCD_HEIGHT; y++)
        for (x = 0; x < LCD_WIDTH; x++)
            uxn_screen.output[y * LCD_WIDTH + x] = LCD_BLACK;

    for (y = 0; y < dest_h; y++)
    {
        int sy = y * uxn_screen.height / dest_h;
        for (x = 0; x < dest_w; x++)
        {
            int sx = x * uxn_screen.width / dest_w;
            int pos = layer_offset(sx, sy);
            int color = uxn_screen.fg[pos] << 2 | uxn_screen.bg[pos];

            uxn_screen.output[(offset_y + y) * LCD_WIDTH + offset_x + x] =
                uxn_screen.palette[color];
        }
    }
    rb->lcd_bitmap(uxn_screen.output, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    rb->lcd_update();
    uxn_screen.dirty = false;
}

uint8_t screen_dei(uint8_t addr)
{
    switch (addr)
    {
    case 0x22: return uxn_screen.width >> 8;
    case 0x23: return uxn_screen.width;
    case 0x24: return uxn_screen.height >> 8;
    case 0x25: return uxn_screen.height;
    case 0x28: return uxn_screen.x >> 8;
    case 0x29: return uxn_screen.x;
    case 0x2a: return uxn_screen.y >> 8;
    case 0x2b: return uxn_screen.y;
    case 0x2c: return uxn_screen.addr >> 8;
    case 0x2d: return uxn_screen.addr;
    default: return uxn.dev[addr];
    }
}

static void screen_fill(int ctrl)
{
    int x1 = ctrl & 0x10 ? 0 : uxn_screen.x;
    int x2 = ctrl & 0x10 ? uxn_screen.x : uxn_screen.width;
    int y1 = ctrl & 0x20 ? 0 : uxn_screen.y;
    int y2 = ctrl & 0x20 ? uxn_screen.y : uxn_screen.height;
    uint8_t *layer = ctrl & 0x40 ? uxn_screen.fg : uxn_screen.bg;
    int x;
    int y;

    x1 = clamp_int(x1, 0, uxn_screen.width);
    x2 = clamp_int(x2, 0, uxn_screen.width);
    y1 = clamp_int(y1, 0, uxn_screen.height);
    y2 = clamp_int(y2, 0, uxn_screen.height);
    for (y = y1; y < y2; y++)
        for (x = x1; x < x2; x++)
            layer[layer_offset(x, y)] = ctrl & 0x3;
    uxn_screen.dirty = true;
}

static void screen_pixel(int ctrl)
{
    uint8_t *layer = ctrl & 0x40 ? uxn_screen.fg : uxn_screen.bg;

    if (uxn_screen.x >= 0 && uxn_screen.y >= 0 &&
        uxn_screen.x < uxn_screen.width &&
        uxn_screen.y < uxn_screen.height)
    {
        layer[layer_offset(uxn_screen.x, uxn_screen.y)] = ctrl & 0x3;
        uxn_screen.dirty = true;
    }
    if (uxn_screen.move_x)
        uxn_screen.x++;
    if (uxn_screen.move_y)
        uxn_screen.y++;
}

static void draw_sprite(uint8_t *layer, const uint8_t *sprite, int x, int y,
                        int ctrl, bool two_bpp)
{
    int blend = ctrl & 0xf;
    bool opaque = blend % 5;
    int flip_x = ctrl & 0x10 ? -1 : 1;
    int flip_y = ctrl & 0x20 ? -1 : 1;
    int py;

    for (py = 0; py < 8; py++)
    {
        int sy = flip_y > 0 ? py : 7 - py;
        int dy = y + py;
        uint8_t plane0;
        uint8_t plane1;
        int px;

        if (dy < 0 || dy >= uxn_screen.height)
            continue;
        plane0 = sprite[sy];
        plane1 = two_bpp ? sprite[sy + 8] : 0;
        for (px = 0; px < 8; px++)
        {
            int sx = flip_x > 0 ? 7 - px : px;
            int dx = x + px;
            int color;

            if (dx < 0 || dx >= uxn_screen.width)
                continue;
            color = (plane0 >> sx) & 1;
            if (two_bpp)
                color |= (plane1 >> sx & 1) << 1;
            if (opaque || color)
                layer[layer_offset(dx, dy)] = blending[color][blend];
        }
    }
}

static void screen_sprite(int ctrl)
{
    uint8_t *layer = ctrl & 0x40 ? uxn_screen.fg : uxn_screen.bg;
    bool two_bpp = ctrl & 0x80;
    int flip_x = ctrl & 0x10 ? -1 : 1;
    int flip_y = ctrl & 0x20 ? -1 : 1;
    int addr_step = uxn_screen.move_addr << (two_bpp ? 2 : 1);
    int x = uxn_screen.x;
    int y = uxn_screen.y;
    int i;

    for (i = 0; i <= uxn_screen.length; i++)
    {
        draw_sprite(layer, &uxn.ram[uxn_screen.addr], x, y, ctrl, two_bpp);
        x += flip_x * uxn_screen.delta_y;
        y += flip_y * uxn_screen.delta_x;
        uxn_screen.addr = (uxn_screen.addr + addr_step) & 0xffff;
    }
    if (uxn_screen.move_x)
        uxn_screen.x += uxn_screen.delta_x * flip_x;
    if (uxn_screen.move_y)
        uxn_screen.y += uxn_screen.delta_y * flip_y;
    uxn_screen.dirty = true;
}

void screen_deo(uint8_t addr)
{
    switch (addr)
    {
    case 0x21:
        uxn_screen.vector = PEEK2(&uxn.dev[0x20]);
        break;
    case 0x23:
        screen_resize(PEEK2(&uxn.dev[0x22]), uxn_screen.height);
        break;
    case 0x25:
        screen_resize(uxn_screen.width, PEEK2(&uxn.dev[0x24]));
        break;
    case 0x26:
        uxn_screen.move_x = uxn.dev[0x26] & 0x1;
        uxn_screen.move_y = uxn.dev[0x26] & 0x2;
        uxn_screen.move_addr = uxn.dev[0x26] & 0x4;
        uxn_screen.length = uxn.dev[0x26] >> 4;
        uxn_screen.delta_x = uxn_screen.move_x << 3;
        uxn_screen.delta_y = uxn_screen.move_y << 2;
        break;
    case 0x28:
    case 0x29:
        uxn_screen.x = signed_short(PEEK2(&uxn.dev[0x28]));
        break;
    case 0x2a:
    case 0x2b:
        uxn_screen.y = signed_short(PEEK2(&uxn.dev[0x2a]));
        break;
    case 0x2c:
    case 0x2d:
        uxn_screen.addr = PEEK2(&uxn.dev[0x2c]);
        break;
    case 0x2e:
        if (uxn.dev[0x2e] & 0x80)
            screen_fill(uxn.dev[0x2e]);
        else
            screen_pixel(uxn.dev[0x2e]);
        break;
    case 0x2f:
        screen_sprite(uxn.dev[0x2f]);
        break;
    }
}
