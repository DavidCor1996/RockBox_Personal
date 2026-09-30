#include "tamagotchi_display.h"
#include "lib/plugin_notifications.h"
#include "pluginbitmaps/tamagotchi_shell.h"

static bool lcd_matrix[TAMA_LCD_H][TAMA_LCD_W];
static bool lcd_icons[TAMA_ICON_COUNT];
static int selected_icon;
static bool dirty;

static void tama_set_color(unsigned color);
static void tama_set_text_color(unsigned foreground, unsigned background);
static void draw_ipodjs_selection(int x, int y, int w, int h,
                                  unsigned *midp);

static const char * const action_short_names[TAMA_ICON_COUNT] =
{
    "MEAL", "LIGHT", "GAME", "MED",
    "TOILET", "METER", "TRAIN", "ALERT"
};

void tamagotchi_display_init(void)
{
    rb->memset(lcd_matrix, 0, sizeof(lcd_matrix));
    rb->memset(lcd_icons, 0, sizeof(lcd_icons));
    selected_icon = 0;
    dirty = true;
}

void tamagotchi_display_set_pixel(int x, int y, bool value)
{
    if (x < 0 || x >= TAMA_LCD_W || y < 0 || y >= TAMA_LCD_H)
        return;

    if (lcd_matrix[y][x] != value)
    {
        lcd_matrix[y][x] = value;
        dirty = true;
    }
}

void tamagotchi_display_set_icon(int icon, bool value)
{
    if (icon < 0 || icon >= TAMA_ICON_COUNT)
        return;

    if (lcd_icons[icon] != value)
    {
        lcd_icons[icon] = value;
        dirty = true;
    }
}

void tamagotchi_display_set_selected_icon(int icon)
{
    if (icon < 0)
        icon = 0;
    if (icon >= TAMA_ICON_COUNT)
        icon = TAMA_ICON_COUNT - 1;

    if (selected_icon != icon)
    {
        selected_icon = icon;
        dirty = true;
    }
}

bool tamagotchi_display_is_dirty(void)
{
    return dirty || plugin_notify_active();
}

void tamagotchi_display_mark_dirty(void)
{
    dirty = true;
}

static void puts_center(int y, const char *text)
{
    int width;
    int height;

    rb->font_getstringsize(text, &width, &height, FONT_UI);
    rb->lcd_putsxy((LCD_WIDTH - width) / 2, y, (const unsigned char *)text);
}

static void puts_center_text(int y, const char *text,
                             unsigned foreground, unsigned background)
{
    tama_set_text_color(foreground, background);
    puts_center(y, text);
}

static void draw_loading_bar(int x, int y, int w, int h)
{
    int span = w - 8;
    int segment = span / 3;
    int pos = ((*rb->current_tick / (HZ / 6 + 1)) % (span + segment)) -
              segment;

    tama_set_color(LCD_RGBPACK(255, 255, 255));
    rb->lcd_fillrect(x, y, w, h);
    tama_set_color(LCD_RGBPACK(77, 119, 88));
    rb->lcd_drawrect(x, y, w, h);
    tama_set_color(LCD_RGBPACK(77, 119, 88));

    if (pos < 0)
    {
        segment += pos;
        pos = 0;
    }
    if (pos + segment > span)
        segment = span - pos;

    if (segment > 0)
        rb->lcd_fillrect(x + 4 + pos, y + 4, segment, h - 8);
}

void tamagotchi_display_show_loading(const char *status)
{
    int shell_x = (LCD_WIDTH - BMPWIDTH_tamagotchi_shell) / 2;
    int shell_y = 29;

    rb->lcd_set_background(LCD_RGBPACK(241, 244, 241));
    rb->lcd_set_foreground(LCD_RGBPACK(28, 34, 31));
    rb->lcd_clear_display();

    tama_set_color(LCD_RGBPACK(28, 34, 31));
    rb->lcd_fillrect(0, 0, LCD_WIDTH, 24);
    puts_center_text(7, "Tamagotchi", LCD_RGBPACK(255, 255, 255),
                     LCD_RGBPACK(28, 34, 31));

    rb->lcd_bitmap(tamagotchi_shell, shell_x, shell_y,
                   BMPWIDTH_tamagotchi_shell,
                   BMPHEIGHT_tamagotchi_shell);
    puts_center_text(shell_y + 18, "TAMAGOTCHI", LCD_RGBPACK(35, 43, 39),
                     LCD_RGBPACK(196, 221, 209));
    puts_center_text(shell_y + 82, status, LCD_RGBPACK(35, 43, 39),
                     LCD_RGBPACK(166, 184, 142));

    draw_loading_bar(48, 203, LCD_WIDTH - 96, 13);

    rb->lcd_update();
}

static int lcd_scale_for_mode(enum tamagotchi_display_mode mode)
{
    int sx = (LCD_WIDTH - 12) / TAMA_LCD_W;
    int sy = (LCD_HEIGHT - 30) / TAMA_LCD_H;
    int scale = sx < sy ? sx : sy;

    if (mode == TAMA_DISPLAY_FULL_LCD)
    {
        sx = (LCD_WIDTH - 8) / TAMA_LCD_W;
        sy = (LCD_HEIGHT - 26) / TAMA_LCD_H;
        scale = sx < sy ? sx : sy;
    }
    else if (mode == TAMA_DISPLAY_MINIMAL)
    {
        sx = LCD_WIDTH / TAMA_LCD_W;
        sy = LCD_HEIGHT / TAMA_LCD_H;
        scale = sx < sy ? sx : sy;
    }

    if (scale < 2)
        scale = 2;
    if (scale > 12)
        scale = 12;
    return scale;
}

static void tama_set_color(unsigned color)
{
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(color);
#else
    (void)color;
#endif
}

static void tama_set_text_color(unsigned foreground, unsigned background)
{
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(foreground);
    rb->lcd_set_background(background);
#else
    (void)foreground;
    (void)background;
#endif
}

static int mix_channel(int a, int b, int pos, int den)
{
    if (den <= 0)
        return b;
    return a + ((b - a) * pos) / den;
}

static unsigned rgb_blend(unsigned background, unsigned foreground, int alpha)
{
    int br = RGB_UNPACK_RED(background);
    int bg = RGB_UNPACK_GREEN(background);
    int bb = RGB_UNPACK_BLUE(background);
    int fr = RGB_UNPACK_RED(foreground);
    int fg = RGB_UNPACK_GREEN(foreground);
    int fb = RGB_UNPACK_BLUE(foreground);

    alpha = MAX(0, MIN(alpha, 255));
    return LCD_RGBPACK((br * (255 - alpha) + fr * alpha) / 255,
                       (bg * (255 - alpha) + fg * alpha) / 255,
                       (bb * (255 - alpha) + fb * alpha) / 255);
}

static void gradient_rect(int x, int y, int w, int h,
                          unsigned top, unsigned bottom)
{
    int row;
    int tr = RGB_UNPACK_RED(top);
    int tg = RGB_UNPACK_GREEN(top);
    int tb = RGB_UNPACK_BLUE(top);
    int br = RGB_UNPACK_RED(bottom);
    int bg = RGB_UNPACK_GREEN(bottom);
    int bb = RGB_UNPACK_BLUE(bottom);
    int den = h > 1 ? h - 1 : 1;

    if (h <= 0 || w <= 0)
        return;

    for (row = 0; row < h; row++)
    {
        tama_set_color(LCD_RGBPACK(mix_channel(tr, br, row, den),
                                   mix_channel(tg, bg, row, den),
                                   mix_channel(tb, bb, row, den)));
        rb->lcd_hline(x, x + w - 1, y + row);
    }
}

static void glass_gradient(int x, int y, int w, int h,
                           unsigned top, unsigned mid, unsigned bottom)
{
    int upper;

    if (h <= 1)
    {
        gradient_rect(x, y, w, h, top, bottom);
        return;
    }

    upper = MAX(1, (h * 45) / 100);
    gradient_rect(x, y, w, upper, top, mid);
    gradient_rect(x, y + upper, w, h - upper, mid, bottom);
}

static unsigned ipodjs_accent(void)
{
    if (rb->global_settings == NULL)
        return LCD_RGBPACK(0, 92, 192);

    switch (rb->global_settings->ui_engine_accent)
    {
        case UI_ENGINE_ACCENT_GRAPHITE:
            return LCD_RGBPACK(84, 90, 100);
        case UI_ENGINE_ACCENT_U2:
            return LCD_RGBPACK(182, 24, 35);
        case UI_ENGINE_ACCENT_TEAL:
            return LCD_RGBPACK(0, 128, 132);
        case UI_ENGINE_ACCENT_GREEN:
            return LCD_RGBPACK(55, 142, 64);
        case UI_ENGINE_ACCENT_GOLD:
            return LCD_RGBPACK(184, 135, 38);
        case UI_ENGINE_ACCENT_ORANGE:
            return LCD_RGBPACK(208, 104, 32);
        case UI_ENGINE_ACCENT_PURPLE:
            return LCD_RGBPACK(113, 82, 170);
        case UI_ENGINE_ACCENT_PINK:
            return LCD_RGBPACK(195, 72, 128);
        case UI_ENGINE_ACCENT_BLUE:
        default:
            return LCD_RGBPACK(0, 92, 192);
    }
}

static void draw_ipodjs_selection(int x, int y, int w, int h,
                                  unsigned *midp)
{
    unsigned accent = ipodjs_accent();
    unsigned top;
    unsigned bottom;

    top = rgb_blend(accent, LCD_RGBPACK(255, 255, 255), 112);
    bottom = rgb_blend(accent, LCD_RGBPACK(0, 0, 0), 70);
    glass_gradient(x, y, w, h, top, accent, bottom);
    if (midp != NULL)
        *midp = accent;
}

static void draw_lcd_pixels(int x0, int y0, int scale,
                            enum tamagotchi_display_mode mode)
{
    int x;
    int y;

    if (mode != TAMA_DISPLAY_MINIMAL)
    {
        tama_set_color(LCD_RGBPACK(158, 177, 143));
        rb->lcd_fillrect(x0 - 3, y0 - 3, TAMA_LCD_W * scale + 6,
                         TAMA_LCD_H * scale + 6);
        tama_set_color(LCD_RGBPACK(55, 67, 50));
        rb->lcd_drawrect(x0 - 3, y0 - 3, TAMA_LCD_W * scale + 6,
                         TAMA_LCD_H * scale + 6);
    }

    for (y = 0; y < TAMA_LCD_H; y++)
    {
        for (x = 0; x < TAMA_LCD_W; x++)
        {
            if (lcd_matrix[y][x])
                tama_set_color(mode == TAMA_DISPLAY_MINIMAL ?
                               LCD_RGBPACK(0, 0, 0) :
                               LCD_RGBPACK(18, 29, 18));
            else
                tama_set_color(mode == TAMA_DISPLAY_MINIMAL ?
                               LCD_RGBPACK(255, 255, 255) :
                               LCD_RGBPACK(143, 163, 130));

            rb->lcd_fillrect(x0 + x * scale, y0 + y * scale,
                             scale > 4 ? scale - 1 : scale,
                             scale > 4 ? scale - 1 : scale);
        }
    }
}

static void draw_icons(int y)
{
    int i;
    int strip_w = TAMA_ICON_COUNT * 18;
    int x0 = (LCD_WIDTH - strip_w) / 2;

    for (i = 0; i < TAMA_ICON_COUNT; i++)
    {
        int x = x0 + i * 18;

        if (i == selected_icon)
        {
            tama_set_color(LCD_RGBPACK(0, 0, 0));
            rb->lcd_fillrect(x + 4, y + 10, 8, 2);
        }

        tama_set_color(lcd_icons[i] ? LCD_RGBPACK(0, 0, 0) :
                  LCD_RGBPACK(170, 170, 170));
        rb->lcd_drawrect(x + 3, y, 10, 8);
        if (lcd_icons[i])
            rb->lcd_fillrect(x + 5, y + 2, 6, 4);
    }

}

static void draw_action_strip(void)
{
    int i;
    int cell_w = LCD_WIDTH / 4;
    int y = LCD_HEIGHT - 42;

    tama_set_color(LCD_RGBPACK(230, 235, 231));
    rb->lcd_fillrect(0, y, LCD_WIDTH, 42);
    tama_set_color(LCD_RGBPACK(90, 99, 92));
    rb->lcd_hline(0, LCD_WIDTH - 1, y);

    for (i = 0; i < TAMA_ICON_COUNT; i++)
    {
        int col = i % 4;
        int row = i / 4;
        int x = col * cell_w;
        int item_y = y + 2 + row * 20;
        int text_w;
        int text_h;

        if (i == selected_icon)
        {
            unsigned mid;

            draw_ipodjs_selection(x + 4, item_y, cell_w - 8, 17, &mid);
            tama_set_text_color(LCD_RGBPACK(255, 255, 255), mid);
            rb->lcd_set_drawmode(DRMODE_FG);
        }
        else
        {
            tama_set_text_color(lcd_icons[i] ? LCD_RGBPACK(35, 43, 39) :
                                LCD_RGBPACK(82, 91, 85),
                                LCD_RGBPACK(230, 235, 231));
            rb->lcd_set_drawmode(DRMODE_SOLID);
        }

        rb->font_getstringsize(action_short_names[i], &text_w, &text_h,
                               FONT_UI);
        rb->lcd_putsxy(x + (cell_w - text_w) / 2, item_y + 4,
                       (const unsigned char *)action_short_names[i]);
        rb->lcd_set_drawmode(DRMODE_SOLID);
    }
}

static void draw_shell_pixels(int x0, int y0, int scale)
{
    int x;
    int y;

    tama_set_color(LCD_RGBPACK(157, 179, 143));
    rb->lcd_fillrect(x0 - 5, y0 - 5, TAMA_LCD_W * scale + 10,
                     TAMA_LCD_H * scale + 10);
    tama_set_color(LCD_RGBPACK(55, 67, 50));
    rb->lcd_drawrect(x0 - 5, y0 - 5, TAMA_LCD_W * scale + 10,
                     TAMA_LCD_H * scale + 10);

    for (y = 0; y < TAMA_LCD_H; y++)
    {
        for (x = 0; x < TAMA_LCD_W; x++)
        {
            tama_set_color(lcd_matrix[y][x] ? LCD_RGBPACK(18, 29, 18) :
                           LCD_RGBPACK(141, 163, 129));
            rb->lcd_fillrect(x0 + x * scale, y0 + y * scale,
                             scale - 1, scale - 1);
        }
    }
}

static void draw_shell_icons(int x0, int y0)
{
    int i;

    for (i = 0; i < TAMA_ICON_COUNT; i++)
    {
        int x = x0 + (i % 4) * 36;
        int y = y0 + (i / 4) * 118;

        tama_set_color(i == selected_icon ? LCD_RGBPACK(35, 43, 39) :
                       LCD_RGBPACK(96, 110, 95));
        rb->lcd_drawrect(x, y, 15, 9);
        if (lcd_icons[i] || i == selected_icon)
            rb->lcd_fillrect(x + 3, y + 2, 9, 5);
    }
}

static void draw_tamagotchi_shell(void)
{
    int shell_x = (LCD_WIDTH - BMPWIDTH_tamagotchi_shell) / 2;
    int shell_y = 24;
    int lcd_scale = 3;
    int lcd_x = shell_x + 59;
    int lcd_y = shell_y + 58;

    tama_set_color(LCD_RGBPACK(242, 244, 241));
    rb->lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);

    rb->lcd_bitmap(tamagotchi_shell, shell_x, shell_y,
                   BMPWIDTH_tamagotchi_shell,
                   BMPHEIGHT_tamagotchi_shell);
    draw_shell_icons(shell_x + 48, shell_y + 34);
    draw_shell_pixels(lcd_x, lcd_y, lcd_scale);
}

void tamagotchi_display_render(const struct tamagotchi_settings *settings)
{
    int scale = lcd_scale_for_mode(settings->display_mode);
    int lcd_x = (LCD_WIDTH - TAMA_LCD_W * scale) / 2;
    int lcd_y;
    int icons_y;

    rb->lcd_set_background(LCD_RGBPACK(242, 244, 241));
    rb->lcd_clear_display();

    if (settings->display_mode == TAMA_DISPLAY_MINIMAL)
    {
        draw_tamagotchi_shell();
        draw_action_strip();
        plugin_notify_render_overlay();
        dirty = false;
        return;
    }
    else
        lcd_y = (LCD_HEIGHT - (TAMA_LCD_H * scale + 18)) / 2;

    draw_lcd_pixels(lcd_x, lcd_y, scale, settings->display_mode);
    icons_y = lcd_y + TAMA_LCD_H * scale + 8;
    if (settings->display_mode != TAMA_DISPLAY_MINIMAL)
        draw_icons(icons_y);
    else
        draw_action_strip();
    plugin_notify_render_overlay();

    dirty = false;
}
