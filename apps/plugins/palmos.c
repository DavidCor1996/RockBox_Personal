/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/                \/
 *
 * Palm OS frontend harness for Rockbox.
 *
 * This is the device-side shell for a future 68k/POSE-compatible core: ROM
 * discovery, persistent paths, a 160x160 Palm display surface, cursor/tap
 * controls, and a text entry mode suited to iPod clickwheel targets.
 *
 ****************************************************************************/

#include "plugin.h"

#define PALM_DIR        ROCKBOX_DIR "/palmos"
#define PALM_ROM_DIR    PALM_DIR "/roms"
#define PALM_CARD_DIR   PALM_DIR "/cards"
#define PALM_STATE_DIR  PALM_DIR "/state"
#define PALM_STATE_FILE PALM_STATE_DIR "/palmos.state"

#define PALM_ROM_NAME   "Palm-OS-3.0-en.rom"
#define PALM_ROM_PATH   PALM_ROM_DIR "/" PALM_ROM_NAME
#define PALM_SIM_ROM    "/home/david/Downloads/Palm-OS-3.0-en.rom"

#define PALM_W 160
#define PALM_H 160

#if defined(HAVE_LCD_COLOR)
#define COL_BG       LCD_RGBPACK(18, 20, 22)
#define COL_BODY     LCD_RGBPACK(64, 68, 70)
#define COL_BEZEL    LCD_RGBPACK(35, 38, 40)
#define COL_LCD      LCD_RGBPACK(177, 191, 164)
#define COL_LCD_DARK LCD_RGBPACK(52, 65, 48)
#define COL_LCD_DIM  LCD_RGBPACK(102, 119, 91)
#define COL_INK      LCD_RGBPACK(20, 29, 21)
#define COL_DIM      LCD_RGBPACK(114, 122, 112)
#define COL_CURSOR   LCD_RGBPACK(214, 54, 50)
#define COL_ACCENT   LCD_RGBPACK(45, 88, 154)
#define COL_BUTTON   LCD_RGBPACK(92, 96, 96)
#define SET_FG(c)   rb->lcd_set_foreground(c)
#define SET_BG(c)   rb->lcd_set_background(c)
#else
#define COL_BG       LCD_WHITE
#define COL_BODY     LCD_WHITE
#define COL_BEZEL    LCD_BLACK
#define COL_LCD      LCD_WHITE
#define COL_LCD_DARK LCD_BLACK
#define COL_LCD_DIM  LCD_DARKGRAY
#define COL_INK      LCD_BLACK
#define COL_DIM      LCD_DARKGRAY
#define COL_CURSOR   LCD_BLACK
#define COL_ACCENT   LCD_BLACK
#define COL_BUTTON   LCD_DARKGRAY
#define SET_FG(c)   rb->lcd_set_foreground(c)
#define SET_BG(c)   rb->lcd_set_background(c)
#endif

enum palm_mode {
    MODE_POINTER = 0,
    MODE_TEXT,
    MODE_MENU,
};

enum text_ring {
    RING_LOWER = 0,
    RING_UPPER,
    RING_NUM,
    RING_SYMBOL,
    RING_COUNT
};

struct palm_frontend {
    enum palm_mode mode;
    enum text_ring ring;
    int cursor_x;
    int cursor_y;
    int scale;
    int screen_x;
    int screen_y;
    int last_tap_x;
    int last_tap_y;
    int drag;
    int tap_flash;
    int select_down;
    int dirty;
    int rom_size;
    unsigned int rom_checksum;
    char typed[48];
    int typed_len;
};

static struct palm_frontend ui;

static const char * const ring_chars[RING_COUNT] = {
    "abcdefghijklmnopqrstuvwxyz",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    "0123456789",
    ".,?!-_/;:@'\"()"
};

static const char * const app_names[] = {
    "Date", "Addr", "Todo", "Memo",
    "Calc", "Prefs", "Mail", "HotSync"
};

static char selected_text_char(void)
{
    const char *ring = ring_chars[ui.ring];
    int len = rb->strlen(ring);

    return ring[ui.cursor_x % len];
}

static void ensure_dirs(void)
{
    rb->mkdir(PALM_DIR);
    rb->mkdir(PALM_ROM_DIR);
    rb->mkdir(PALM_CARD_DIR);
    rb->mkdir(PALM_STATE_DIR);
}

static unsigned int checksum_update(unsigned int sum, const unsigned char *buf,
                                    int count)
{
    int i;

    for (i = 0; i < count; i++)
        sum = (sum << 5) - sum + buf[i];

    return sum;
}

static int inspect_rom(const char *path, unsigned int *checksum)
{
    unsigned char buf[512];
    int fd;
    int got;
    int total = 0;
    unsigned int sum = 5381;

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    while ((got = rb->read(fd, buf, sizeof(buf))) > 0) {
        total += got;
        sum = checksum_update(sum, buf, got);
    }

    rb->close(fd);
    *checksum = sum;
    return total;
}

static void save_frontend_state(void)
{
    int fd = rb->open(PALM_STATE_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (fd >= 0) {
        rb->write(fd, &ui, sizeof(ui));
        rb->close(fd);
    }
}

static void load_frontend_state(void)
{
    int fd = rb->open(PALM_STATE_FILE, O_RDONLY);

    if (fd >= 0) {
        if (rb->read(fd, &ui, sizeof(ui)) != (long)sizeof(ui))
            rb->memset(&ui, 0, sizeof(ui));
        rb->close(fd);
    }
}

static void palm_core_reset(void)
{
    ui.last_tap_x = -1;
    ui.last_tap_y = -1;
    ui.drag = 0;
    ui.tap_flash = 0;
    ui.select_down = 0;
    ui.dirty = 1;
    ui.typed_len = 0;
    ui.typed[0] = '\0';
}

static void palm_core_tap(int x, int y)
{
    ui.last_tap_x = x;
    ui.last_tap_y = y;
    ui.tap_flash = 5;
    ui.dirty = 1;
}

static void palm_core_key(char ch)
{
    if (ui.typed_len < (int)sizeof(ui.typed) - 1) {
        ui.typed[ui.typed_len++] = ch;
        ui.typed[ui.typed_len] = '\0';
        ui.dirty = 1;
    }
}

static void palm_core_backspace(void)
{
    if (ui.typed_len > 0) {
        ui.typed_len--;
        ui.typed[ui.typed_len] = '\0';
        ui.dirty = 1;
    }
}

static void init_layout(void)
{
    ui.scale = 1;

    if (LCD_WIDTH >= PALM_W * 2 && LCD_HEIGHT >= PALM_H + 50)
        ui.scale = 1;

    ui.screen_x = (LCD_WIDTH - PALM_W * ui.scale) / 2;
    ui.screen_y = 12;

    if (LCD_HEIGHT >= 220)
        ui.screen_y = 12;
    if (ui.screen_y + PALM_H * ui.scale > LCD_HEIGHT - 28)
        ui.screen_y = 8;

    if (ui.cursor_x < 0 || ui.cursor_x >= PALM_W)
        ui.cursor_x = PALM_W / 2;
    if (ui.cursor_y < 0 || ui.cursor_y >= PALM_H)
        ui.cursor_y = PALM_H / 2;
    if (ui.ring >= RING_COUNT)
        ui.ring = RING_LOWER;
}

static void draw_launcher_icon(int x, int y, int index)
{
    const char *label = app_names[index];
    int ix = x + 11;
    int iy = y + 2;

    SET_FG(COL_LCD_DARK);
    rb->lcd_drawrect(ix, iy, 17, 15);
    rb->lcd_hline(ix + 3, ix + 13, iy + 4);
    rb->lcd_hline(ix + 3, ix + 13, iy + 8);
    rb->lcd_vline(ix + 5, iy + 2, iy + 12);

    if (index == 4) {
        rb->lcd_hline(ix + 4, ix + 12, iy + 7);
        rb->lcd_vline(ix + 8, iy + 3, iy + 11);
    } else if (index == 7) {
        rb->lcd_drawline(ix + 3, iy + 10, ix + 13, iy + 3);
    }

    rb->lcd_putsxy(x + 1, y + 20, label);
}

static void draw_palm_screen(int sx, int sy)
{
    int x;
    int y;
    int sw = PALM_W * ui.scale;
    int sh = PALM_H * ui.scale;

    SET_FG(COL_LCD);
    rb->lcd_fillrect(sx, sy, sw, sh);
    SET_FG(COL_LCD_DARK);
    rb->lcd_drawrect(sx, sy, sw, sh);

    SET_FG(COL_LCD_DARK);
    rb->lcd_fillrect(sx + 1, sy + 1, sw - 2, 12);
    SET_FG(COL_LCD);
    rb->lcd_putsxy(sx + 4, sy + 3, "Applications");
    rb->lcd_putsxy(sx + sw - 34, sy + 3, "3.0");

    SET_FG(COL_LCD_DIM);
    rb->lcd_hline(sx + 4, sx + sw - 5, sy + 16);
    rb->lcd_vline(sx + sw - 13, sy + 18, sy + sh - 18);
    rb->lcd_drawrect(sx + sw - 11, sy + 24, 7, 40);

    for (y = 0; y < 2; y++) {
        for (x = 0; x < 4; x++)
            draw_launcher_icon(sx + 5 + x * 35, sy + 23 + y * 45,
                               y * 4 + x);
    }

    if (ui.typed_len > 0) {
        SET_FG(COL_ACCENT);
        rb->lcd_putsxy(sx + 6, sy + 128, ui.typed);
    } else {
        SET_FG(COL_LCD_DIM);
        rb->lcd_putsxy(sx + 6, sy + 128, "ROM ready");
    }

    if (ui.last_tap_x >= 0) {
        x = sx + ui.last_tap_x * ui.scale;
        y = sy + ui.last_tap_y * ui.scale;
        SET_FG(COL_ACCENT);
        rb->lcd_drawrect(x - 4, y - 4, 9, 9);
        if (ui.tap_flash > 0)
            rb->lcd_drawrect(x - 7, y - 7, 15, 15);
    }

    x = sx + ui.cursor_x * ui.scale;
    y = sy + ui.cursor_y * ui.scale;
    SET_FG(COL_CURSOR);
    rb->lcd_vline(x, y - 5, y + 5);
    rb->lcd_hline(x - 5, x + 5, y);
    rb->lcd_drawpixel(x + 1, y + 1);
}

static void draw_silkscreen(int sx, int sy, int sw)
{
    int y = sy + PALM_H + 6;
    int bx = sx + 13;
    int bw = 26;
    int i;

    SET_FG(COL_BEZEL);
    rb->lcd_fillrect(sx, y, sw, 36);
    SET_FG(COL_BUTTON);

    for (i = 0; i < 4; i++) {
        int x = bx + i * 34;
        rb->lcd_drawrect(x, y + 6, bw, 14);
        if (i == 0)
            rb->lcd_putsxy(x + 5, y + 9, "D");
        else if (i == 1)
            rb->lcd_putsxy(x + 5, y + 9, "A");
        else if (i == 2)
            rb->lcd_putsxy(x + 5, y + 9, "T");
        else
            rb->lcd_putsxy(x + 5, y + 9, "M");
    }

    SET_FG(COL_DIM);
    rb->lcd_putsxy(sx + 35, y + 24, "iPod Palm");
}

static void draw_palm_surface(void)
{
    int sx = ui.screen_x;
    int sy = ui.screen_y;
    int body_x = sx - 12;
    int body_y = sy - 12;
    int body_w = PALM_W + 24;
    int body_h = PALM_H + 58;

    if (body_x < 0)
        body_x = 0;
    if (body_y < 0)
        body_y = 0;

    SET_FG(COL_BODY);
    rb->lcd_fillrect(body_x, body_y, body_w, body_h);
    SET_FG(COL_BEZEL);
    rb->lcd_drawrect(body_x, body_y, body_w, body_h);
    rb->lcd_drawrect(body_x + 2, body_y + 2, body_w - 4, body_h - 4);

    draw_palm_screen(sx, sy);

    if (LCD_HEIGHT >= sy + PALM_H + 44)
        draw_silkscreen(sx, sy, PALM_W);
}

static void draw_status(void)
{
    SET_FG(COL_DIM);
    rb->lcd_putsxyf(4, 2, "%s  ROM %dK", ui.mode == MODE_TEXT ? "Text" :
                    ui.mode == MODE_MENU ? "Menu" : "Pointer",
                    ui.rom_size / 1024);
}

static void draw_help(void)
{
    int y = LCD_HEIGHT - 12;

    SET_FG(COL_DIM);

    if (ui.mode == MODE_TEXT) {
        rb->lcd_putsxyf(4, y, "Text %d [%c]  Select type",
                        ui.ring + 1, selected_text_char());
    } else if (ui.mode == MODE_MENU) {
        rb->lcd_putsxy(4, y, "Menu: Select reset  Hold Menu quits");
    } else {
        rb->lcd_putsxy(4, y, "Wheel move  Select tap");
    }
}

static void redraw(bool force)
{
    if (!force && !ui.dirty && ui.tap_flash <= 0)
        return;

    SET_BG(COL_BG);
    SET_FG(COL_BG);
    rb->lcd_clear_display();
    draw_palm_surface();
    draw_status();
    draw_help();
    rb->lcd_update();

    if (ui.tap_flash > 0)
        ui.tap_flash--;
    ui.dirty = ui.tap_flash > 0;
}

static void move_cursor(int dx, int dy)
{
    int old_x = ui.cursor_x;
    int old_y = ui.cursor_y;

    ui.cursor_x += dx;
    ui.cursor_y += dy;

    if (ui.cursor_x < 0)
        ui.cursor_x = 0;
    if (ui.cursor_y < 0)
        ui.cursor_y = 0;
    if (ui.cursor_x >= PALM_W)
        ui.cursor_x = PALM_W - 1;
    if (ui.cursor_y >= PALM_H)
        ui.cursor_y = PALM_H - 1;

    if (ui.cursor_x != old_x || ui.cursor_y != old_y)
        ui.dirty = 1;
}

static int wheel_step(unsigned int button)
{
    int step = (button & BUTTON_REPEAT) ? 8 : 3;

    if (ui.mode == MODE_TEXT)
        step = 1;

    return step;
}

static void handle_text_button(unsigned int bare, unsigned int button)
{
    int len = rb->strlen(ring_chars[ui.ring]);

    if (bare == BUTTON_SCROLL_FWD) {
        ui.cursor_x = (ui.cursor_x + wheel_step(button)) % len;
        ui.dirty = 1;
    } else if (bare == BUTTON_SCROLL_BACK) {
        ui.cursor_x = (ui.cursor_x + len - wheel_step(button)) % len;
        ui.dirty = 1;
    } else if (bare == BUTTON_SELECT) {
        if (button & BUTTON_REPEAT) {
            ui.ring = (ui.ring + 1) % RING_COUNT;
            ui.dirty = 1;
        } else if (!ui.select_down) {
            palm_core_key(selected_text_char());
        }
        ui.select_down = 1;
    } else if (bare == BUTTON_LEFT) {
        palm_core_backspace();
    } else if (bare == BUTTON_RIGHT) {
        palm_core_key(' ');
    } else if (bare == BUTTON_PLAY) {
        if (button & BUTTON_REPEAT) {
            ui.ring = (ui.ring + 1) % RING_COUNT;
            ui.dirty = 1;
        } else {
            palm_core_key('\n');
        }
    } else if (bare == BUTTON_MENU) {
        ui.mode = MODE_POINTER;
        ui.dirty = 1;
    }
}

static void handle_pointer_button(unsigned int bare, unsigned int button)
{
    int step = wheel_step(button);

    if (bare == BUTTON_SCROLL_FWD) {
        move_cursor(0, step);
    } else if (bare == BUTTON_SCROLL_BACK) {
        move_cursor(0, -step);
    } else if (bare == BUTTON_LEFT) {
        move_cursor(-step, 0);
    } else if (bare == BUTTON_RIGHT) {
        move_cursor(step, 0);
    } else if (bare == BUTTON_SELECT) {
        if (button & BUTTON_REPEAT) {
            ui.drag = 1;
        } else if (!ui.select_down) {
            palm_core_tap(ui.cursor_x, ui.cursor_y);
            ui.drag = 1;
        }
        ui.select_down = 1;
    } else if (bare == BUTTON_PLAY) {
        ui.mode = (button & BUTTON_REPEAT) ? MODE_TEXT : MODE_POINTER;
        ui.dirty = 1;
    } else if (bare == BUTTON_MENU) {
        ui.mode = (button & BUTTON_REPEAT) ? MODE_MENU : MODE_MENU;
        ui.dirty = 1;
    }
}

static void select_release_fallback(void)
{
    if (ui.mode == MODE_TEXT) {
        palm_core_key(selected_text_char());
    } else if (ui.mode == MODE_MENU) {
        palm_core_reset();
    } else {
        palm_core_tap(ui.cursor_x, ui.cursor_y);
    }
}

static bool handle_button(unsigned int button)
{
    unsigned int bare = button & ~(BUTTON_REPEAT | BUTTON_REL);

    if (button & BUTTON_REL) {
        if (bare == BUTTON_SELECT) {
            if (!ui.select_down)
                select_release_fallback();
            ui.drag = 0;
            ui.select_down = 0;
            ui.dirty = 1;
        }
        return false;
    }

    if (ui.mode == MODE_TEXT) {
        handle_text_button(bare, button);
    } else if (ui.mode == MODE_MENU) {
        if (bare == BUTTON_SELECT)
            palm_core_reset();
        else if (bare == BUTTON_PLAY) {
            ui.mode = MODE_TEXT;
            ui.dirty = 1;
        } else if (bare == BUTTON_MENU) {
            ui.mode = MODE_POINTER;
            ui.dirty = 1;
        }
    } else {
        handle_pointer_button(bare, button);
    }

    return false;
}

enum plugin_status plugin_start(const void *parameter)
{
    const char *rom_path = parameter ? (const char *)parameter : PALM_ROM_PATH;
    unsigned int checksum = 0;
    int size;
    bool quit = false;
    (void)parameter;

    ensure_dirs();
    load_frontend_state();
    init_layout();

    if (!rom_path[0])
        rom_path = PALM_ROM_PATH;

    size = inspect_rom(rom_path, &checksum);
#ifdef SIMULATOR
    if (size < 0) {
        rom_path = PALM_SIM_ROM;
        size = inspect_rom(rom_path, &checksum);
    }
#endif

    if (size < 0) {
        rb->splashf(HZ * 4, "Put %s in %s", PALM_ROM_NAME, PALM_ROM_DIR);
        return PLUGIN_ERROR;
    }

    ui.rom_size = size;
    ui.rom_checksum = checksum;
    init_layout();

    rb->lcd_setfont(FONT_SYSFIXED);
    palm_core_reset();

    while (!quit) {
        int button;

        redraw(false);
        button = rb->button_get_w_tmo(HZ / 12);

        if (button == BUTTON_NONE)
            continue;

        if (button == SYS_USB_CONNECTED)
            return PLUGIN_USB_CONNECTED;

        if (ui.mode == MODE_MENU &&
            ((button & ~(BUTTON_REPEAT | BUTTON_REL)) == BUTTON_MENU) &&
            (button & BUTTON_REPEAT)) {
            quit = true;
        } else {
            quit = handle_button(button);
        }
    }

    save_frontend_state();
    return PLUGIN_OK;
}
