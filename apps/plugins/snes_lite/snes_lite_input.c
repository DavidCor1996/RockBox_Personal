/***************************************************************************
 * iPod click-wheel controls and iPodJS-style in-emulator menu.
 ***************************************************************************/

#include "snes_lite.h"
#include "libretro/libretro-common/include/libretro.h"

#define MENU_HEADER_H 22
#define MENU_FOOTER_H 18
#define MENU_ROW_H 20
#define MENU_COUNT 11

static uint16_t joypad;
#ifdef HAS_BUTTON_HOLD
static bool hold_exit_initialized;
static bool hold_exit_armed;
#endif

static bool hold_exit_requested(void)
{
#ifdef HAS_BUTTON_HOLD
    bool hold_now = rb->button_hold();

    /* Do not immediately exit if the plugin was launched while locked. */
    if (!hold_exit_initialized)
    {
        hold_exit_initialized = true;
        hold_exit_armed = hold_now;
        return false;
    }
    if (hold_now && !hold_exit_armed)
    {
        hold_exit_armed = true;
        return true;
    }
    hold_exit_armed = hold_now;
#endif
    return false;
}

#ifdef SIMULATOR
static void set_joy(unsigned id)
{
    joypad |= 1u << id;
}
#endif

#if defined(HAVE_WHEEL_POSITION) || defined(IPOD_6G)
static uint16_t wheel_direction_for_position(int position)
{
    int zone;

    if (position < 0)
        return 0;
    zone = ((position + 6) / 12) & 7;
    switch (zone)
    {
        case 0: return 1u << RETRO_DEVICE_ID_JOYPAD_UP;
        case 1: return (1u << RETRO_DEVICE_ID_JOYPAD_UP) |
                       (1u << RETRO_DEVICE_ID_JOYPAD_RIGHT);
        case 2: return 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
        case 3: return (1u << RETRO_DEVICE_ID_JOYPAD_RIGHT) |
                       (1u << RETRO_DEVICE_ID_JOYPAD_DOWN);
        case 4: return 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
        case 5: return (1u << RETRO_DEVICE_ID_JOYPAD_DOWN) |
                       (1u << RETRO_DEVICE_ID_JOYPAD_LEFT);
        case 6: return 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
        default: return (1u << RETRO_DEVICE_ID_JOYPAD_LEFT) |
                        (1u << RETRO_DEVICE_ID_JOYPAD_UP);
    }
}
#endif

static uint16_t map_profile_buttons(unsigned profile, int held,
                                    bool start_combo, bool select_combo,
                                    bool shoulder_left, bool shoulder_right)
{
    static const unsigned mappings[][4] = {
        /* Centre, Play, Previous, Next */
        { RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_Y,
          RETRO_DEVICE_ID_JOYPAD_X, RETRO_DEVICE_ID_JOYPAD_A },
        { RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_B,
          RETRO_DEVICE_ID_JOYPAD_X, RETRO_DEVICE_ID_JOYPAD_Y },
        { RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_A,
          RETRO_DEVICE_ID_JOYPAD_Y, RETRO_DEVICE_ID_JOYPAD_X },
        { RETRO_DEVICE_ID_JOYPAD_Y, RETRO_DEVICE_ID_JOYPAD_B,
          RETRO_DEVICE_ID_JOYPAD_X, RETRO_DEVICE_ID_JOYPAD_A },
        { RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_A,
          RETRO_DEVICE_ID_JOYPAD_L, RETRO_DEVICE_ID_JOYPAD_R },
        { RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_A,
          RETRO_DEVICE_ID_JOYPAD_Y, RETRO_DEVICE_ID_JOYPAD_X },
    };
    uint16_t state = 0;

    if (profile >= ARRAYLEN(mappings))
        profile = SNES_INPUT_PLATFORMER;
#ifdef BUTTON_SELECT
    if ((held & BUTTON_SELECT) && !start_combo &&
        !shoulder_left && !shoulder_right)
        state |= 1u << mappings[profile][0];
#endif
#ifdef BUTTON_PLAY
    if ((held & BUTTON_PLAY) && !select_combo &&
        !shoulder_left && !shoulder_right)
        state |= 1u << mappings[profile][1];
#endif
#ifdef BUTTON_LEFT
    if ((held & BUTTON_LEFT) && !shoulder_left)
        state |= 1u << mappings[profile][2];
#endif
#ifdef BUTTON_RIGHT
    if ((held & BUTTON_RIGHT) && !shoulder_right)
        state |= 1u << mappings[profile][3];
#endif
    return state;
}

static uint16_t map_held_state(unsigned profile, int held, int wheel_position)
{
    bool start_combo = false;
    bool select_combo = false;
    bool shoulder_left = false;
    bool shoulder_right = false;
    uint16_t state = 0;

#if defined(HAVE_WHEEL_POSITION) || defined(IPOD_6G)
    state |= wheel_direction_for_position(wheel_position);
#else
    (void)wheel_position;
#ifdef BUTTON_UP
    if (held & BUTTON_UP)
        state |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
#endif
#ifdef BUTTON_DOWN
    if (held & BUTTON_DOWN)
        state |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
#endif
#endif
#if defined(BUTTON_MENU) && defined(BUTTON_SELECT)
    start_combo = (held & (BUTTON_MENU | BUTTON_SELECT)) ==
                  (BUTTON_MENU | BUTTON_SELECT);
#endif
#if defined(BUTTON_MENU) && defined(BUTTON_PLAY)
    select_combo = (held & (BUTTON_MENU | BUTTON_PLAY)) ==
                   (BUTTON_MENU | BUTTON_PLAY);
#endif
#if defined(BUTTON_MENU) && defined(BUTTON_LEFT)
    shoulder_left = (held & (BUTTON_MENU | BUTTON_LEFT)) ==
                    (BUTTON_MENU | BUTTON_LEFT);
#endif
#if defined(BUTTON_MENU) && defined(BUTTON_RIGHT)
    shoulder_right = (held & (BUTTON_MENU | BUTTON_RIGHT)) ==
                     (BUTTON_MENU | BUTTON_RIGHT);
#endif
    state |= map_profile_buttons(profile, held, start_combo, select_combo,
                                 shoulder_left, shoulder_right);
    if (start_combo)
        state |= 1u << RETRO_DEVICE_ID_JOYPAD_START;
    if (select_combo)
        state |= 1u << RETRO_DEVICE_ID_JOYPAD_SELECT;
    if (shoulder_left)
        state |= 1u << RETRO_DEVICE_ID_JOYPAD_L;
    if (shoulder_right)
        state |= 1u << RETRO_DEVICE_ID_JOYPAD_R;
    return state;
}

bool snes_lite_input_selftest(unsigned *coverage)
{
    uint16_t seen = 0;
    bool passed = true;

#if (defined(HAVE_WHEEL_POSITION) || defined(IPOD_6G)) && \
    defined(BUTTON_SELECT) && \
    defined(BUTTON_PLAY) && defined(BUTTON_LEFT) && \
    defined(BUTTON_RIGHT) && defined(BUTTON_MENU)
    static const struct {
        int held;
        int wheel_position;
        uint16_t expected;
    } tests[] = {
        { BUTTON_SELECT, -1, 1u << RETRO_DEVICE_ID_JOYPAD_B },
        { BUTTON_PLAY, -1, 1u << RETRO_DEVICE_ID_JOYPAD_A },
        { BUTTON_LEFT, -1, 1u << RETRO_DEVICE_ID_JOYPAD_Y },
        { BUTTON_RIGHT, -1, 1u << RETRO_DEVICE_ID_JOYPAD_X },
        { BUTTON_MENU | BUTTON_SELECT, -1,
          1u << RETRO_DEVICE_ID_JOYPAD_START },
        { BUTTON_MENU | BUTTON_PLAY, -1,
          1u << RETRO_DEVICE_ID_JOYPAD_SELECT },
        { BUTTON_MENU | BUTTON_LEFT, -1,
          1u << RETRO_DEVICE_ID_JOYPAD_L },
        { BUTTON_MENU | BUTTON_RIGHT, -1,
          1u << RETRO_DEVICE_ID_JOYPAD_R },
        { 0, 0, 1u << RETRO_DEVICE_ID_JOYPAD_UP },
        { 0, 12, (1u << RETRO_DEVICE_ID_JOYPAD_UP) |
                 (1u << RETRO_DEVICE_ID_JOYPAD_RIGHT) },
        { 0, 24, 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT },
        { 0, 36, (1u << RETRO_DEVICE_ID_JOYPAD_RIGHT) |
                 (1u << RETRO_DEVICE_ID_JOYPAD_DOWN) },
        { 0, 48, 1u << RETRO_DEVICE_ID_JOYPAD_DOWN },
        { 0, 60, (1u << RETRO_DEVICE_ID_JOYPAD_DOWN) |
                 (1u << RETRO_DEVICE_ID_JOYPAD_LEFT) },
        { 0, 72, 1u << RETRO_DEVICE_ID_JOYPAD_LEFT },
        { 0, 84, (1u << RETRO_DEVICE_ID_JOYPAD_LEFT) |
                 (1u << RETRO_DEVICE_ID_JOYPAD_UP) },
    };
    unsigned i;

    for (i = 0; i < ARRAYLEN(tests); ++i)
    {
        uint16_t actual = map_held_state(SNES_INPUT_ACTION,
                                         tests[i].held,
                                         tests[i].wheel_position);
        seen |= actual;
        if (actual != tests[i].expected)
            passed = false;
    }
    if (seen != 0x0fff)
        passed = false;
#else
    passed = false;
#endif
    if (coverage)
        *coverage = seen;
    return passed;
}

void snes_lite_input_poll(void)
{
    int held = rb->button_status();
    int event = rb->button_get(false);
    int wheel_position = -1;

    if (hold_exit_requested())
    {
        snes_lite_log("exit requested by hold switch");
        snes_lite.quit_requested = true;
        return;
    }
#ifdef HAVE_WHEEL_POSITION
    wheel_position = rb->wheel_status();
#endif
    joypad = map_held_state(snes_lite.config.input_profile, held,
                            wheel_position);
#ifdef SIMULATOR
    if (getenv("SNES_LITE_TEST_AUTOPLAY") &&
        snes_lite.profile_frames % 60 < 3)
    {
        if ((snes_lite.profile_frames / 60) & 1)
            set_joy(RETRO_DEVICE_ID_JOYPAD_B);
        else
            set_joy(RETRO_DEVICE_ID_JOYPAD_START);
    }
#endif
    if ((event & BUTTON_REPEAT) != 0)
    {
#ifdef BUTTON_MENU
        if ((event & BUTTON_MENU) != 0 &&
            (held & ~BUTTON_MENU) == 0)
            snes_lite.menu_requested = true;
#endif
    }
    if (event == SYS_USB_CONNECTED)
        snes_lite.quit_requested = true;
}

int16_t snes_lite_input_state(unsigned port, unsigned device,
                              unsigned index, unsigned id)
{
    (void)index;
    if (port != 0 || device != RETRO_DEVICE_JOYPAD)
        return 0;
    if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
        return (int16_t)joypad;
    if (id > 15)
        return 0;
    return (joypad & (1u << id)) != 0;
}

static void draw_three_tone(int x, int y, int width, int height,
                            unsigned top, unsigned middle, unsigned bottom)
{
    int upper = MAX(1, height / 3);

    rb->lcd_set_foreground(top);
    rb->lcd_fillrect(x, y, width, upper);
    rb->lcd_set_foreground(middle);
    rb->lcd_fillrect(x, y + upper, width, MAX(1, height - upper * 2));
    rb->lcd_set_foreground(bottom);
    rb->lcd_fillrect(x, y + height - upper, width, upper);
}

static void menu_value(int item, char *value, size_t size)
{
    value[0] = '\0';
    switch (item)
    {
        case 3:
            rb->strlcpy(value,
                snes_lite.config.performance_preset == SNES_PERF_FAST ?
                "Fast" :
                (snes_lite.config.performance_preset == SNES_PERF_MAX ?
                 "Max" :
                 (snes_lite.config.performance_preset == SNES_PERF_QUALITY ?
                  "Quality" : "Balanced")), size);
            break;
        case 4:
            if (snes_lite.config.frameskip < 0)
                rb->strlcpy(value, "Auto", size);
            else
                rb->snprintf(value, size, "%d", snes_lite.config.frameskip);
            break;
        case 5:
            rb->strlcpy(value,
                snes_lite.config.video_mode == SNES_VIDEO_FULLSCREEN ?
                "Full Screen" : "Native", size);
            break;
        case 6:
            rb->strlcpy(value,
                snes_lite.config.audio == SNES_AUDIO_ON ? "Best" :
                (snes_lite.config.audio == SNES_AUDIO_LOW ? "Economy" :
                 (snes_lite.config.audio == SNES_AUDIO_AUTO ?
                  "Auto" : "Off")), size);
            break;
        case 7:
            rb->strlcpy(value, snes_lite_input_profile_name(
                snes_lite.config.input_profile), size);
            break;
        case 8:
            rb->strlcpy(value, snes_lite.config.show_fps ? "On" : "Off",
                        size);
            break;
    }
}

static void draw_menu(int selected)
{
    static const char *const items[] = {
        "Resume Game", "Save Game", "Reload Save", "Performance",
        "Frameskip", "Display", "Sound", "Controls", "Show FPS",
        "Restart Game", "Quit to Games"
    };
    int visible = (LCD_HEIGHT - MENU_HEADER_H - MENU_FOOTER_H) / MENU_ROW_H;
    int top = selected >= visible ? selected - visible + 1 : 0;
    int row;

    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    draw_three_tone(0, 0, LCD_WIDTH, MENU_HEADER_H,
                    LCD_RGBPACK(252, 253, 253),
                    LCD_RGBPACK(216, 219, 223),
                    LCD_RGBPACK(174, 178, 183));
    rb->lcd_set_foreground(LCD_RGBPACK(92, 96, 101));
    rb->lcd_hline(0, LCD_WIDTH - 1, MENU_HEADER_H - 1);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_set_background(LCD_RGBPACK(216, 219, 223));
    rb->lcd_putsxy(7, 3, snes_lite.rom.title[0] ?
                   snes_lite.rom.title : SNES_LITE_NAME);

    for (row = 0; row < visible && top + row < MENU_COUNT; row++)
    {
        int item = top + row;
        int y = MENU_HEADER_H + row * MENU_ROW_H;
        char value[48];
        int value_width = 0;
        int value_height;

        if (item == selected)
        {
            draw_three_tone(0, y, LCD_WIDTH, MENU_ROW_H,
                            LCD_RGBPACK(107, 200, 254),
                            LCD_RGBPACK(38, 146, 226),
                            LCD_RGBPACK(0, 92, 192));
            rb->lcd_set_foreground(LCD_WHITE);
            rb->lcd_set_background(LCD_RGBPACK(38, 146, 226));
        }
        else
        {
            rb->lcd_set_foreground(LCD_WHITE);
            rb->lcd_fillrect(0, y, LCD_WIDTH, MENU_ROW_H);
            rb->lcd_set_foreground(LCD_BLACK);
            rb->lcd_set_background(LCD_WHITE);
        }
        rb->lcd_putsxy(7, y + 3, items[item]);
        menu_value(item, value, sizeof(value));
        if (value[0])
        {
            rb->lcd_getstringsize((const unsigned char *)value,
                                  &value_width, &value_height);
            rb->lcd_putsxy(LCD_WIDTH - value_width - 17, y + 3, value);
            rb->lcd_putsxy(LCD_WIDTH - 10, y + 3, ">");
        }
    }

    rb->lcd_set_foreground(LCD_RGBPACK(232, 233, 235));
    rb->lcd_fillrect(0, LCD_HEIGHT - MENU_FOOTER_H,
                     LCD_WIDTH, MENU_FOOTER_H);
    rb->lcd_set_foreground(LCD_RGBPACK(99, 101, 103));
    rb->lcd_set_background(LCD_RGBPACK(232, 233, 235));
    rb->lcd_putsxy(7, LCD_HEIGHT - 15, "MENU  Back");
    rb->lcd_putsxy(LCD_WIDTH - 118, LCD_HEIGHT - 15, "SELECT  Choose");
    rb->lcd_update();
}

static void adjust_setting(int selected, int direction)
{
    if (direction == 0)
        direction = 1;
    switch (selected)
    {
        case 3:
            snes_lite.config.performance_preset =
                (snes_lite.config.performance_preset +
                 (direction > 0 ? 1 : 3)) % 4;
            snes_lite.config.performance_mode =
                snes_lite.config.performance_preset != SNES_PERF_QUALITY;
            snes_lite.variables_changed = true;
            break;
        case 4:
            snes_lite.config.frameskip += direction > 0 ? 1 : -1;
            if (snes_lite.config.frameskip > 4)
                snes_lite.config.frameskip = -1;
            if (snes_lite.config.frameskip < -1)
                snes_lite.config.frameskip = 4;
            snes_lite.effective_frameskip =
                snes_lite.config.frameskip < 0 ? 1 :
                snes_lite.config.frameskip;
            snes_lite.variables_changed = true;
            break;
        case 5:
            snes_lite.config.video_mode =
                (snes_lite.config.video_mode + 1) % 2;
            break;
        case 6:
            snes_lite.config.audio =
                (snes_lite.config.audio + (direction > 0 ? 1 : 3)) % 4;
            rb->splash(HZ, "Sound setting applies next launch");
            break;
        case 7:
            snes_lite.config.input_profile =
                (snes_lite.config.input_profile +
                 (direction > 0 ? 1 : SNES_INPUT_SPORTS)) %
                (SNES_INPUT_SPORTS + 1);
            break;
        case 8:
            snes_lite.config.show_fps = !snes_lite.config.show_fps;
            break;
    }
}

void snes_lite_menu(void)
{
    int selected = 0;
    bool done = false;
    bool redraw = true;

    rb->button_clear_queue();
    while (!done)
    {
        int button;
        int clean;

        if (hold_exit_requested())
        {
            snes_lite_log("exit requested by hold switch in menu");
            snes_lite.quit_requested = true;
            break;
        }
        if (redraw)
        {
            draw_menu(selected);
            redraw = false;
        }
        button = rb->button_get_w_tmo(HZ / 10);
        if (button == BUTTON_NONE)
            continue;
        clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
        if (button == SYS_USB_CONNECTED)
        {
            snes_lite.quit_requested = true;
            break;
        }
#ifdef BUTTON_MENU
        if (clean == BUTTON_MENU)
        {
            done = true;
            continue;
        }
#endif
#ifdef BUTTON_SCROLL_BACK
        if (clean == BUTTON_SCROLL_BACK)
        {
            selected = (selected + MENU_COUNT - 1) % MENU_COUNT;
            redraw = true;
            continue;
        }
#endif
#ifdef BUTTON_SCROLL_FWD
        if (clean == BUTTON_SCROLL_FWD)
        {
            selected = (selected + 1) % MENU_COUNT;
            redraw = true;
            continue;
        }
#endif
#ifdef BUTTON_PLAY
        if (clean == BUTTON_PLAY)
        {
            selected = (selected + 1) % MENU_COUNT;
            redraw = true;
            continue;
        }
#endif
#ifdef BUTTON_LEFT
        if (clean == BUTTON_LEFT && selected >= 3 && selected <= 8)
        {
            adjust_setting(selected, -1);
            redraw = true;
            continue;
        }
#endif
#ifdef BUTTON_RIGHT
        if (clean == BUTTON_RIGHT && selected >= 3 && selected <= 8)
        {
            adjust_setting(selected, 1);
            redraw = true;
            continue;
        }
#endif
#ifdef BUTTON_SELECT
        if (clean == BUTTON_SELECT)
        {
            switch (selected)
            {
                case 0:
                    done = true;
                    break;
                case 1:
                    rb->splash(HZ / 2, snes_lite_sram_save() ?
                               "Game saved" : "Save failed");
                    redraw = true;
                    break;
                case 2:
                    snes_lite_sram_load();
                    rb->splash(HZ / 2, "Save reloaded");
                    redraw = true;
                    break;
                case 3: case 4: case 5: case 6: case 7: case 8:
                    adjust_setting(selected, 1);
                    redraw = true;
                    break;
                case 9:
                    snes_lite.reset_requested = true;
                    done = true;
                    break;
                case 10:
                    snes_lite.quit_requested = true;
                    done = true;
                    break;
            }
        }
#endif
    }
    snes_lite_config_save_game(&snes_lite.config);
    rb->button_clear_queue();
    rb->lcd_setfont(FONT_SYSFIXED);
    snes_lite_video_redraw();
}
