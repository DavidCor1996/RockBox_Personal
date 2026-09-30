#include "tamagotchi_menu.h"
#include "tamagotchi_clock.h"
#include "tamagotchi_display.h"
#include "tamagotchi_haptics.h"
#include "tamagotchi_state.h"
#include "upstream/tamalib/tamalib.h"
#include "lib/plugin_notifications.h"

enum menu_item
{
    MENU_RESUME = 0,
    MENU_SAVE,
    MENU_LOAD,
    MENU_RESET,
    MENU_CONTROL,
    MENU_DISPLAY,
    MENU_WHEEL,
    MENU_HAPTICS,
    MENU_BEEP,
    MENU_QUIET,
    MENU_CLOCK,
    MENU_ABOUT,
    MENU_QUIT,
    MENU_COUNT,
};

#define MENU_VISIBLE_ROWS 10

static const char *mode_name(int value, const char *a, const char *b,
                             const char *c)
{
    if (value == 0)
        return a;
    if (value == 1)
        return b;
    return c;
}

static const char *display_mode_name(enum tamagotchi_display_mode mode)
{
    switch (mode)
    {
        case TAMA_DISPLAY_SHELL:
            return "LCD";
        case TAMA_DISPLAY_FULL_LCD:
            return "Large LCD";
        case TAMA_DISPLAY_MINIMAL:
        default:
            return "Shell";
    }
}

static bool is_adjustable(int item)
{
    return item == MENU_CONTROL ||
           item == MENU_DISPLAY ||
           item == MENU_WHEEL || item == MENU_HAPTICS ||
           item == MENU_BEEP || item == MENU_QUIET;
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
        rb->lcd_set_foreground(LCD_RGBPACK(mix_channel(tr, br, row, den),
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

static void draw_menu(int selected, bool editing,
                      const struct tamagotchi_settings *settings)
{
    int i;
    int first = selected - MENU_VISIBLE_ROWS / 2;
    int y = 30;
    const int row_h = 17;
    char line[64];

    if (first < 0)
        first = 0;
    if (first > MENU_COUNT - MENU_VISIBLE_ROWS)
        first = MENU_COUNT - MENU_VISIBLE_ROWS;
    if (first < 0)
        first = 0;

    rb->lcd_clear_display();
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_RGBPACK(28, 34, 31));
#endif
    rb->lcd_fillrect(0, 0, LCD_WIDTH, 24);
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_RGBPACK(28, 34, 31));
#endif
    rb->lcd_putsxy(8, 7, (const unsigned char *)"Tamagotchi Settings");
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_RGBPACK(225, 229, 226));
#endif
    rb->lcd_fillrect(0, 24, LCD_WIDTH, LCD_HEIGHT - 24);
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_BLACK);
#endif

    for (i = first; i < first + MENU_VISIBLE_ROWS && i < MENU_COUNT; i++)
    {
        const char *text = "";

        switch (i)
        {
            case MENU_RESUME: text = "Resume"; break;
            case MENU_SAVE: text = "Save"; break;
            case MENU_LOAD: text = "Load"; break;
            case MENU_RESET: text = "Reset Tamagotchi"; break;
            case MENU_CONTROL:
                rb->snprintf(line, sizeof(line), "Control: %s",
                             settings->control_mode == TAMA_CONTROL_RAW_ABC ?
                             "Raw A/B/C" : "Stock iPod");
                text = line;
                break;
            case MENU_DISPLAY:
                rb->snprintf(line, sizeof(line), "Screen: %s",
                             display_mode_name(settings->display_mode));
                text = line;
                break;
            case MENU_WHEEL:
                rb->snprintf(line, sizeof(line), "Wheel: %s",
                             mode_name(settings->wheel_sensitivity, "Low",
                                       "Medium", "High"));
                text = line;
                break;
            case MENU_HAPTICS:
                rb->snprintf(line, sizeof(line), "Haptics: %s",
                             settings->haptics_enabled ? "On" : "Off");
                text = line;
                break;
            case MENU_BEEP:
                rb->snprintf(line, sizeof(line), "Beep sound: %s",
                             settings->beep_enabled ? "On" : "Off");
                text = line;
                break;
            case MENU_QUIET:
                rb->snprintf(line, sizeof(line), "Quiet mode: %s",
                             settings->quiet_mode ? "On" : "Off");
                text = line;
                break;
            case MENU_CLOCK:
                rb->snprintf(line, sizeof(line), "Clock: iPod time");
                text = line;
                break;
            case MENU_ABOUT: text = "About / ROM info"; break;
            case MENU_QUIT: text = "Quit"; break;
        }

        if (i == selected)
        {
            unsigned mid;

#if LCD_DEPTH > 1
            draw_ipodjs_selection(6, y - 2, LCD_WIDTH - 12, row_h, &mid);
#endif
#if LCD_DEPTH > 1
            rb->lcd_set_foreground(LCD_WHITE);
            rb->lcd_set_background(mid);
#endif
            rb->lcd_set_drawmode(DRMODE_FG);
        }
        else
        {
#if LCD_DEPTH > 1
            rb->lcd_set_foreground(LCD_RGBPACK(202, 208, 203));
#endif
            rb->lcd_hline(10, LCD_WIDTH - 22, y + row_h - 3);
#if LCD_DEPTH > 1
            rb->lcd_set_foreground(LCD_RGBPACK(35, 43, 39));
            rb->lcd_set_background(LCD_RGBPACK(225, 229, 226));
#endif
            rb->lcd_set_drawmode(DRMODE_SOLID);
        }
        rb->lcd_putsxy(10, y, (const unsigned char *)text);
        if (i == selected && editing)
            rb->lcd_putsxy(LCD_WIDTH - 28, y, (const unsigned char *)"<>");
        rb->lcd_set_drawmode(DRMODE_SOLID);
#if LCD_DEPTH > 1
        rb->lcd_set_foreground(LCD_BLACK);
#endif
        y += row_h;
    }

#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_RGBPACK(92, 92, 92));
    rb->lcd_set_background(LCD_RGBPACK(28, 34, 31));
#endif
    rb->snprintf(line, sizeof(line), "%d/%d", selected + 1, MENU_COUNT);
    rb->lcd_putsxy(LCD_WIDTH - 42, 7, (const unsigned char *)line);
#if LCD_DEPTH > 1
    rb->lcd_set_background(LCD_RGBPACK(225, 229, 226));
#endif
    rb->lcd_drawrect(LCD_WIDTH - 9, 31, 4, 174);
    if (MENU_COUNT > MENU_VISIBLE_ROWS)
    {
        int thumb_h = MAX(16, (174 * MENU_VISIBLE_ROWS) / MENU_COUNT);
        int thumb_y = 31 + ((174 - thumb_h) * selected) /
                      (MENU_COUNT - 1);

        rb->lcd_fillrect(LCD_WIDTH - 8, thumb_y + 1, 2, thumb_h - 2);
    }
    rb->lcd_putsxy(8, LCD_HEIGHT - 13,
                   (const unsigned char *)"Wheel move  Select choose");
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(LCD_BLACK);
#endif
    rb->lcd_update();
}

static void adjust_setting(struct tamagotchi_settings *settings, int item,
                           int delta)
{
    if (delta == 0)
        return;

    switch (item)
    {
        case MENU_CONTROL:
            settings->control_mode =
                settings->control_mode == TAMA_CONTROL_RAW_ABC ?
                TAMA_CONTROL_STOCK_IPOD : TAMA_CONTROL_RAW_ABC;
            break;
        case MENU_DISPLAY:
        {
            int mode = (int)settings->display_mode + delta;

            if (mode < (int)TAMA_DISPLAY_SHELL)
                mode = (int)TAMA_DISPLAY_MINIMAL;
            if (mode > (int)TAMA_DISPLAY_MINIMAL)
                mode = (int)TAMA_DISPLAY_SHELL;
            settings->display_mode = mode;
            tamagotchi_display_mark_dirty();
            break;
        }
        case MENU_WHEEL:
            settings->wheel_sensitivity += delta;
            if (settings->wheel_sensitivity < 0)
                settings->wheel_sensitivity = 2;
            if (settings->wheel_sensitivity > 2)
                settings->wheel_sensitivity = 0;
            break;
        case MENU_HAPTICS:
            settings->haptics_enabled = !settings->haptics_enabled;
            break;
        case MENU_BEEP:
            settings->beep_enabled = !settings->beep_enabled;
            break;
        case MENU_QUIET:
            settings->quiet_mode = !settings->quiet_mode;
            break;
        case MENU_CLOCK:
            settings->clock_mode = TAMA_CLOCK_REAL_TIME;
            break;
    }

    tamagotchi_config_save(settings);
}

bool tamagotchi_menu_run(struct tamagotchi_settings *settings)
{
    int selected = 0;
    bool editing = false;

    rb->button_clear_queue();
#if defined(BUTTON_MENU) && defined(BUTTON_SELECT)
    while (rb->button_status() & (BUTTON_MENU | BUTTON_SELECT))
        rb->sleep(HZ / 50);
    rb->button_clear_queue();
    rb->sleep(HZ / 10);
    rb->button_clear_queue();
#endif

    while (true)
    {
        int button;

        draw_menu(selected, editing, settings);
        button = rb->button_get(true);

        if (IS_SYSEVENT(button))
        {
            if (button == SYS_USB_CONNECTED)
                return false;
            continue;
        }

#ifdef BUTTON_SCROLL_FWD
        if (button & BUTTON_SCROLL_FWD)
        {
            if (editing)
                adjust_setting(settings, selected, 1);
            else
                selected = (selected + 1) % MENU_COUNT;
            continue;
        }
#endif
#ifdef BUTTON_SCROLL_BACK
        if (button & BUTTON_SCROLL_BACK)
        {
            if (editing)
                adjust_setting(settings, selected, -1);
            else
                selected = (selected + MENU_COUNT - 1) % MENU_COUNT;
            continue;
        }
#endif
#ifdef BUTTON_RIGHT
        if (button & BUTTON_RIGHT)
        {
            if (is_adjustable(selected))
                adjust_setting(settings, selected, 1);
            else
                selected = (selected + 1) % MENU_COUNT;
            continue;
        }
#endif
#ifdef BUTTON_LEFT
        if (button & BUTTON_LEFT)
        {
            if (is_adjustable(selected))
                adjust_setting(settings, selected, -1);
            else
                selected = (selected + MENU_COUNT - 1) % MENU_COUNT;
            continue;
        }
#endif
#ifdef BUTTON_MENU
        if (button & BUTTON_MENU)
        {
            if (editing)
            {
                editing = false;
                continue;
            }
            return true;
        }
#endif
#ifdef BUTTON_SELECT
        if (!(button & BUTTON_SELECT))
            continue;
#else
        continue;
#endif

        switch (selected)
        {
            case MENU_RESUME:
                return true;
            case MENU_SAVE:
                if (tamagotchi_state_save())
                {
                    tamagotchi_haptic_save();
                    plugin_notify_post(TAMAGOTCHI_SOURCE, TAMAGOTCHI_NAME,
                                       "Save complete", PLUGIN_NOTIFY_LOW,
                                       PLUGIN_NOTIFY_DISMISS_ON_SELECT,
                                       PLUGIN_NOTIFY_DEFAULT_TTL_MS);
                }
                else
                    rb->splash(HZ, "Save failed");
                break;
            case MENU_LOAD:
                if (tamagotchi_state_load())
                {
                    tamagotchi_haptic_load();
                    rb->splash(HZ, "State loaded");
                }
                else
                    rb->splash(HZ, "No valid state");
                break;
            case MENU_RESET:
                if (rb->yesno_pop_confirm("Reset Tamagotchi?"))
                {
                    tamalib_reset();
                    tamagotchi_state_delete();
                    settings->auto_clock_done =
                        tamagotchi_clock_autoset_from_ipod();
                    tamagotchi_config_save(settings);
                    tamagotchi_display_mark_dirty();
                }
                break;
            case MENU_CONTROL:
                editing = !editing;
                break;
            case MENU_DISPLAY:
                adjust_setting(settings, selected, 1);
                break;
            case MENU_WHEEL:
            case MENU_HAPTICS:
            case MENU_BEEP:
            case MENU_QUIET:
                editing = !editing;
                break;
            case MENU_CLOCK:
                settings->clock_mode = TAMA_CLOCK_REAL_TIME;
                break;
            case MENU_ABOUT:
                rb->splash(HZ * 3, "Real P1 via TamaLIB; ROM: tama.b");
                break;
            case MENU_QUIT:
            default:
                tamagotchi_config_save(settings);
                return false;
        }

        tamagotchi_config_save(settings);
    }
}
