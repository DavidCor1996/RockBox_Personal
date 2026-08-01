#include "anarch_platform.h"
#include "lib/configfile.h"

#include <fcntl.h>

#define ANARCH_CONFIG_FILE ROCKBOX_DIR "/games/anarch/anarch.cfg"
#define ANARCH_CONFIG_VERSION 1
#define MENU_X 35
#define MENU_Y 20
#define MENU_W 250
#define MENU_H 200
#define MENU_LINE_H 25

static bool haptics_enabled = true;
#ifdef SIMULATOR
static bool menu_test;
static const int *menu_test_events;
static size_t menu_test_event_count;
static size_t menu_test_event_index;
#endif

static void menu_haptic(int duration, int strength)
{
    if (haptics_enabled && rb->haptic_feedback_enabled != NULL &&
        rb->haptic_feedback != NULL && rb->haptic_feedback_enabled())
        rb->haptic_feedback(duration, strength);
}

void anarch_haptics_set_enabled(bool enabled)
{
    haptics_enabled = enabled;
}

void anarch_haptic_event(uint8_t event, uint8_t data)
{
    static long last_tick;
    long now = *rb->current_tick;
    int duration = 12;
    int strength = 28;

    (void)data;
    if (!haptics_enabled || rb->haptic_feedback_enabled == NULL ||
        rb->haptic_feedback == NULL ||
        !rb->haptic_feedback_enabled() ||
        TIME_BEFORE(now, last_tick + MAX(1, HZ / 12)))
        return;
    if (event == 1 || event == 2)
    {
        duration = event == 2 ? 70 : 35;
        strength = event == 2 ? 85 : 60;
    }
    else if (event == 7)
    {
        duration = 45;
        strength = 70;
    }
    last_tick = now;
    rb->haptic_feedback(duration, strength);
}

void anarch_settings_default(struct anarch_settings *settings)
{
    settings->profile = ANARCH_PROFILE_BALANCED;
    settings->music = true;
    settings->sound = true;
    settings->haptics = true;
}

static void settings_table(struct anarch_settings *settings,
                           struct configdata table[4])
{
    static char *profile_names[] = { "balanced" };
    struct configdata values[4] = {
        { TYPE_ENUM, 0, ANARCH_PROFILE_COUNT, .int_p = &settings->profile,
          .name = "profile", .values = profile_names },
        { TYPE_BOOL, 0, 1, .bool_p = &settings->music,
          .name = "music", .values = NULL },
        { TYPE_BOOL, 0, 1, .bool_p = &settings->sound,
          .name = "sound", .values = NULL },
        { TYPE_BOOL, 0, 1, .bool_p = &settings->haptics,
          .name = "haptics", .values = NULL }
    };

    rb->memcpy(table, values, sizeof(values));
}

void anarch_settings_load(struct anarch_settings *settings)
{
    struct configdata table[4];

    anarch_settings_default(settings);
    settings_table(settings, table);
    if (configfile_load(ANARCH_CONFIG_FILE, table, ARRAYLEN(table),
                        ANARCH_CONFIG_VERSION) < 0)
        configfile_save(ANARCH_CONFIG_FILE, table, ARRAYLEN(table),
                        ANARCH_CONFIG_VERSION);
    if (settings->profile < 0 || settings->profile >= ANARCH_PROFILE_COUNT)
        settings->profile = ANARCH_PROFILE_BALANCED;
    anarch_haptics_set_enabled(settings->haptics);
}

void anarch_settings_save(const struct anarch_settings *settings)
{
    struct anarch_settings writable = *settings;
    struct configdata table[4];

    settings_table(&writable, table);
    configfile_save(ANARCH_CONFIG_FILE, table, ARRAYLEN(table),
                    ANARCH_CONFIG_VERSION);
    anarch_haptics_set_enabled(settings->haptics);
}

static void draw_box(const char *title)
{
    anarch_video_present();
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_fillrect(MENU_X, MENU_Y, MENU_W, MENU_H);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_drawrect(MENU_X, MENU_Y, MENU_W, MENU_H);
    rb->lcd_putsxy(MENU_X + 10, MENU_Y + 7, title);
}

static void draw_row(int row, const char *text, bool selected)
{
    int y = MENU_Y + 29 + row * MENU_LINE_H;

    rb->lcd_set_foreground(selected ? LCD_RGBPACK(30, 105, 210) : LCD_BLACK);
    rb->lcd_fillrect(MENU_X + 7, y - 3, MENU_W - 14, MENU_LINE_H - 2);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_putsxy(MENU_X + 14, y, text);
}

static int menu_event(void)
{
#ifdef SIMULATOR
    if (menu_test)
    {
        if (menu_test_event_index < menu_test_event_count)
            return menu_test_events[menu_test_event_index++];
        return BUTTON_PLAY;
    }
#endif
    int event = rb->button_get_w_tmo(MAX(1, HZ / 10));

    if (event == SYS_USB_CONNECTED ||
        rb->default_event_handler(event) == SYS_USB_CONNECTED)
        return SYS_USB_CONNECTED;
#ifdef HAS_BUTTON_HOLD
    if (rb->button_hold())
    {
        rb->button_clear_queue();
        return BUTTON_NONE;
    }
#endif
    return event;
}

static bool run_options(struct anarch_settings *settings, bool *usb)
{
    static const int count = 5;
    int selected = 0;
    bool redraw = true;

    while (true)
    {
        int event;
        int clean;
        char text[80];

        if (redraw)
        {
            draw_box("OPTIONS");
            rb->snprintf(text, sizeof(text), "Profile: Balanced");
            draw_row(0, text, selected == 0);
            rb->snprintf(text, sizeof(text), "Music: %s",
                         settings->music ? "On" : "Off");
            draw_row(1, text, selected == 1);
            rb->snprintf(text, sizeof(text), "Sound: %s",
                         settings->sound ? "On" : "Off");
            draw_row(2, text, selected == 2);
            rb->snprintf(text, sizeof(text), "Haptics: %s",
                         settings->haptics ? "On" : "Off");
            draw_row(3, text, selected == 3);
            draw_row(4, "Back", selected == 4);
            rb->lcd_set_foreground(LCD_LIGHTGRAY);
            rb->lcd_putsxy(MENU_X + 10, MENU_Y + MENU_H - 18,
                           "160x120 30fps rays x2 fog+dither");
            rb->lcd_update();
            redraw = false;
        }
        event = menu_event();
        if (event == SYS_USB_CONNECTED)
        {
            *usb = true;
            return false;
        }
        clean = event & ~(BUTTON_REPEAT | BUTTON_REL);
        if (clean == BUTTON_SCROLL_BACK)
        {
            selected = selected > 0 ? selected - 1 : count - 1;
            menu_haptic(8, 22);
            redraw = true;
        }
        else if (clean == BUTTON_SCROLL_FWD)
        {
            selected = (selected + 1) % count;
            menu_haptic(8, 22);
            redraw = true;
        }
        else if (clean == BUTTON_SELECT && !(event & BUTTON_REL))
        {
            if (selected == 1)
                settings->music = !settings->music;
            else if (selected == 2)
                settings->sound = !settings->sound;
            else if (selected == 3)
                settings->haptics = !settings->haptics;
            else if (selected == 4)
                return true;
            anarch_settings_save(settings);
            menu_haptic(15, 35);
            redraw = true;
        }
        else if ((clean == BUTTON_PLAY || clean == BUTTON_MENU) &&
                 !(event & BUTTON_REL))
            return true;
    }
}

enum anarch_menu_action anarch_menu_run(struct anarch_settings *settings)
{
    static const char *const items[] = {
        "Resume", "Map", "Options", "Save", "Restart Level", "Exit"
    };
    const int count = ARRAYLEN(items);
    int selected = 0;
    bool redraw = true;

    anarch_audio_pause(true);
    rb->button_clear_queue();
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
    while (true)
    {
        int event;
        int clean;

        if (redraw)
        {
            int i;

            draw_box("ANARCH PAUSED");
            for (i = 0; i < count; ++i)
                draw_row(i, items[i], selected == i);
            rb->lcd_update();
            redraw = false;
        }
        event = menu_event();
        if (event == SYS_USB_CONNECTED)
        {
#ifdef HAVE_WHEEL_POSITION
            rb->wheel_send_events(false);
#endif
            return ANARCH_MENU_USB;
        }
        clean = event & ~(BUTTON_REPEAT | BUTTON_REL);
        if (clean == BUTTON_SCROLL_BACK)
        {
            selected = selected > 0 ? selected - 1 : count - 1;
            menu_haptic(8, 22);
            redraw = true;
        }
        else if (clean == BUTTON_SCROLL_FWD)
        {
            selected = (selected + 1) % count;
            menu_haptic(8, 22);
            redraw = true;
        }
        else if ((clean == BUTTON_PLAY || clean == BUTTON_MENU) &&
                 !(event & BUTTON_REL))
        {
#ifdef HAVE_WHEEL_POSITION
            rb->wheel_send_events(false);
#endif
            return ANARCH_MENU_RESUME;
        }
        else if (clean == BUTTON_SELECT && !(event & BUTTON_REL))
        {
            bool usb = false;

            menu_haptic(15, 35);
            if (selected == 2)
            {
                run_options(settings, &usb);
                if (usb)
                {
#ifdef HAVE_WHEEL_POSITION
                    rb->wheel_send_events(false);
#endif
                    return ANARCH_MENU_USB;
                }
                redraw = true;
                continue;
            }
#ifdef HAVE_WHEEL_POSITION
            rb->wheel_send_events(false);
#endif
            if (selected == 0)
                return ANARCH_MENU_RESUME;
            if (selected == 1)
                return ANARCH_MENU_MAP;
            if (selected == 3)
                return ANARCH_MENU_SAVE;
            if (selected == 4)
                return ANARCH_MENU_RESTART;
            return ANARCH_MENU_EXIT;
        }
    }
}

#ifdef SIMULATOR
bool anarch_config_selftest(void)
{
    struct anarch_settings saved;
    struct anarch_settings loaded;

    anarch_settings_default(&saved);
    saved.music = false;
    saved.sound = false;
    saved.haptics = false;
    anarch_settings_save(&saved);
    anarch_settings_load(&loaded);
    return loaded.profile == saved.profile && loaded.music == saved.music &&
           loaded.sound == saved.sound && loaded.haptics == saved.haptics;
}

static enum anarch_menu_action run_menu_script(
    struct anarch_settings *settings, const int *events, size_t count)
{
    enum anarch_menu_action action;

    menu_test = true;
    menu_test_events = events;
    menu_test_event_count = count;
    menu_test_event_index = 0;
    action = anarch_menu_run(settings);
    menu_test = false;
    return action;
}

bool anarch_menu_selftest(void)
{
    static const int resume[] = { BUTTON_SELECT };
    static const int map[] = { BUTTON_SCROLL_FWD, BUTTON_SELECT };
    static const int options[] = {
        BUTTON_SCROLL_FWD, BUTTON_SCROLL_FWD, BUTTON_SELECT,
        BUTTON_SCROLL_FWD, BUTTON_SELECT, BUTTON_PLAY, BUTTON_PLAY
    };
    static const int save[] = {
        BUTTON_SCROLL_FWD, BUTTON_SCROLL_FWD, BUTTON_SCROLL_FWD,
        BUTTON_SELECT
    };
    static const int restart[] = {
        BUTTON_SCROLL_FWD, BUTTON_SCROLL_FWD, BUTTON_SCROLL_FWD,
        BUTTON_SCROLL_FWD, BUTTON_SELECT
    };
    static const int exit[] = {
        BUTTON_SCROLL_FWD, BUTTON_SCROLL_FWD, BUTTON_SCROLL_FWD,
        BUTTON_SCROLL_FWD, BUTTON_SCROLL_FWD, BUTTON_SELECT
    };
    static const int usb[] = { SYS_USB_CONNECTED };
    struct anarch_settings settings;
    bool original_music;

    anarch_settings_default(&settings);
    original_music = settings.music;
    if (run_menu_script(&settings, resume, ARRAYLEN(resume)) !=
        ANARCH_MENU_RESUME ||
        run_menu_script(&settings, map, ARRAYLEN(map)) != ANARCH_MENU_MAP ||
        run_menu_script(&settings, options, ARRAYLEN(options)) !=
        ANARCH_MENU_RESUME || settings.music == original_music ||
        run_menu_script(&settings, save, ARRAYLEN(save)) != ANARCH_MENU_SAVE ||
        run_menu_script(&settings, restart, ARRAYLEN(restart)) !=
        ANARCH_MENU_RESTART ||
        run_menu_script(&settings, exit, ARRAYLEN(exit)) != ANARCH_MENU_EXIT ||
        run_menu_script(&settings, usb, ARRAYLEN(usb)) != ANARCH_MENU_USB)
        return false;
    return true;
}
#endif
