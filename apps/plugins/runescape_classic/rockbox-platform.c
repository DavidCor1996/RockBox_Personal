#include "rockbox-platform.h"

#include "rsc-c/mudclient.h"
#include "rsc-c/game-character.h"
#include "rsc-c/surface.h"
#include "rsc-c/world.h"
#include "rsc-c/ui/ui-tabs.h"
#include "rsc-c/ui/stats-tab.h"

#undef open
#undef close
#undef read
#undef write
#undef lseek

static void *rsc_pool;
static size_t rsc_pool_size;
static void *rsc_extra_pool;
static size_t rsc_extra_pool_size;
static int rsc_quit;
static int rsc_ui_mode;
static int rsc_ui_tab;
static int rsc_panel_index;
static int rsc_context_index;
static int rsc_logoff_prompt;
static int rsc_hold_was_on;
static int rsc_debug_overlay;

static const char *rsc_offline_profiles[] = {RSC_DEFAULT_NAME,
                                            "AboveChaos"};
#ifdef HAVE_WHEEL_POSITION
static int rsc_last_wheel_pos = -1;
static int rsc_wheel_accum;
static long rsc_last_wheel_tap;
static long rsc_last_wheel_scroll;
static long rsc_last_zoom;
#endif

static void rsc_haptic_pulse(int duration_ms, int strength)
{
    if (rb->haptic_feedback_enabled == NULL) {
        return;
    }

    if (!rb->haptic_feedback_enabled()) {
        return;
    }

    rb->haptic_feedback(duration_ms, strength);
}

void rsc_haptic_menu_move(void)
{
    rsc_haptic_pulse(12, 22);
}

void rsc_haptic_menu_select(void)
{
    rsc_haptic_pulse(24, 42);
}

void rsc_haptic_action(void)
{
    rsc_haptic_pulse(32, 55);
}

void rsc_haptic_error(void)
{
    rsc_haptic_pulse(45, 70);
}

void rsc_haptic_skill(void)
{
    rsc_haptic_pulse(36, 78);
}

static void rsc_boot_status(const char *text)
{
    rb->lcd_clear_display();
    rb->lcd_putsxy(8, 8, text);
    rb->lcd_update();
}

static void rsc_draw_offline_profile_screen(mudclient *mud, int selected_profile)
{
    const char *descriptions[] = {"Administrator, max stats, full coins",
                                  "Standard new player, no mod"};
    const char *notes[] = {"Best gear, bank, shops, castle start",
                           "Female starter, bronze kit, castle start"};
    int title_w = 0;
    int text_w = 0;

    rb->lcd_clear_display();
    rb->lcd_set_foreground(FB_RGBPACK(12, 12, 12));
    rb->lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);

    rb->lcd_set_foreground(FB_RGBPACK(86, 62, 28));
    rb->lcd_fillrect(0, 0, LCD_WIDTH, 36);
    rb->lcd_set_foreground(FB_RGBPACK(137, 104, 45));
    rb->lcd_drawrect(0, 0, LCD_WIDTH, 36);
    rb->lcd_set_foreground(FB_RGBPACK(255, 220, 0));
    rb->lcd_getstringsize("RuneScape Classic", &title_w, NULL);
    rb->lcd_putsxy((LCD_WIDTH - title_w) / 2, 7, "RuneScape Classic");

    rb->lcd_set_foreground(FB_RGBPACK(38, 30, 20));
    rb->lcd_fillrect(16, 48, LCD_WIDTH - 32, 22);
    rb->lcd_set_foreground(FB_RGBPACK(160, 132, 76));
    rb->lcd_drawrect(16, 48, LCD_WIDTH - 32, 22);
    rb->lcd_set_foreground(FB_RGBPACK(235, 235, 235));
    rb->lcd_getstringsize("Select character", &text_w, NULL);
    rb->lcd_putsxy((LCD_WIDTH - text_w) / 2, 54, "Select character");

    for (int i = 0; i < 2; i++) {
        int y = 82 + (i * 56);
        int selected = i == selected_profile;

        rb->lcd_set_foreground(selected ? FB_RGBPACK(92, 74, 38)
                                        : FB_RGBPACK(32, 27, 22));
        rb->lcd_fillrect(20, y, LCD_WIDTH - 40, 44);
        rb->lcd_set_foreground(selected ? FB_RGBPACK(255, 220, 80)
                                        : FB_RGBPACK(112, 94, 62));
        rb->lcd_drawrect(20, y, LCD_WIDTH - 40, 44);

        rb->lcd_set_foreground(selected ? FB_RGBPACK(38, 25, 12)
                                        : FB_RGBPACK(16, 16, 16));
        rb->lcd_fillrect(28, y + 8, 28, 28);
        rb->lcd_set_foreground(selected ? FB_RGBPACK(232, 190, 80)
                                        : FB_RGBPACK(96, 80, 56));
        rb->lcd_drawrect(28, y + 8, 28, 28);

        if (selected) {
            rb->lcd_set_foreground(FB_RGBPACK(0, 220, 0));
            rb->lcd_putsxy(38, y + 18, ">");
        }

        rb->lcd_set_foreground(selected ? FB_RGBPACK(255, 255, 255)
                                        : FB_RGBPACK(205, 205, 205));
        rb->lcd_putsxy(66, y + 6, rsc_offline_profiles[i]);
        rb->lcd_set_foreground(FB_RGBPACK(222, 192, 118));
        rb->lcd_putsxy(66, y + 20, descriptions[i]);
        rb->lcd_set_foreground(FB_RGBPACK(170, 170, 170));
        rb->lcd_putsxy(66, y + 32, notes[i]);
    }

    rb->lcd_set_foreground(FB_RGBPACK(38, 30, 20));
    rb->lcd_fillrect(10, 210, LCD_WIDTH - 20, 22);
    rb->lcd_set_foreground(FB_RGBPACK(160, 132, 76));
    rb->lcd_drawrect(10, 210, LCD_WIDTH - 20, 22);
    rb->lcd_set_foreground(FB_RGBPACK(255, 255, 255));
    rb->lcd_getstringsize("LEFT/RIGHT change   SELECT start   MENU cancel",
                          &text_w, NULL);
    rb->lcd_putsxy((LCD_WIDTH - text_w) / 2, 216,
                   "LEFT/RIGHT change   SELECT start   MENU cancel");

    rb->lcd_update();
}

static int rsc_select_offline_profile(mudclient *mud)
{
    int selected = mud->offline_profile;

    rb->button_clear_queue();
    while (1) {
        int button = BUTTON_NONE;
        long base;

        rsc_draw_offline_profile_screen(mud, selected);
        button = rb->button_get(true);

        if (button == BUTTON_NONE || (button & BUTTON_REL)) {
            continue;
        }

        base = button & ~(BUTTON_REPEAT | BUTTON_REL);

        if (base == BUTTON_LEFT
#ifdef BUTTON_UP
            || base == BUTTON_UP
#endif
        ) {
            selected = (selected - 1 + 2) % 2;
            rsc_haptic_menu_move();
        } else if (base == BUTTON_RIGHT
#ifdef BUTTON_DOWN
                   || base == BUTTON_DOWN
#endif
        ) {
            selected = (selected + 1) % 2;
            rsc_haptic_menu_move();
        } else if (base == BUTTON_SELECT) {
            mud->offline_profile = selected;
            if (mud->offline_profile == RSC_OFFLINE_PROFILE_DAVID) {
                strcpy(mud->username, RSC_DEFAULT_NAME);
            } else {
                strcpy(mud->username, "AboveChaos");
            }
            strcpy(mud->options->username, mud->username);
            rsc_haptic_menu_select();
            return 1;
        } else if (base == BUTTON_MENU) {
            rsc_haptic_error();
            return 0;
        }
    }

    return 0;
}

enum {
    RSC_MODE_GAME = 0,
    RSC_MODE_TABS = 1,
    RSC_MODE_PANEL = 2,
    RSC_MODE_CONTEXT = 3
};

void rsc_rb_init_alloc(void)
{
    void *audio_pool = NULL;
    size_t audio_pool_size = 0;

    if (rsc_pool != NULL) {
        return;
    }

    rsc_boot_status("RSC memory");

    rsc_pool = rb->plugin_get_buffer(&rsc_pool_size);
    if (rsc_pool != NULL && rsc_pool_size > 0 &&
        init_memory_pool(rsc_pool_size, rsc_pool) != (size_t)-1) {
        rsc_extra_pool = NULL;
        rsc_extra_pool_size = 0;
    } else {
        rsc_pool = NULL;
        rsc_pool_size = 0;
    }

    rsc_boot_status("RSC memory+");

    audio_pool = rb->plugin_get_audio_buffer(&audio_pool_size);
#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
    if (audio_pool != NULL &&
        (uintptr_t)audio_pool < (uintptr_t)plugin_start_addr) {
        size_t overlay_limit =
            (uintptr_t)plugin_start_addr - (uintptr_t)audio_pool;
        if (audio_pool_size > overlay_limit) {
            audio_pool_size = overlay_limit;
        }
    }
#endif

    if (audio_pool_size > (32 * 1024 * 1024)) {
        audio_pool_size = 32 * 1024 * 1024;
    }

    if (rsc_pool != NULL) {
        if (audio_pool != NULL && audio_pool_size > 4096) {
            add_new_area(audio_pool, audio_pool_size, rsc_pool);
            rsc_extra_pool = audio_pool;
            rsc_extra_pool_size = audio_pool_size;
            rsc_pool_size += audio_pool_size;
        }
    } else if (audio_pool != NULL && audio_pool_size > 0 &&
               init_memory_pool(audio_pool_size, audio_pool) != (size_t)-1) {
        rsc_pool = audio_pool;
        rsc_pool_size = audio_pool_size;
    }
}

void *rsc_rb_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }
    return tlsf_malloc(size);
}

void *rsc_rb_calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *ptr = rsc_rb_malloc(total);
    if (ptr != NULL) {
        rb->memset(ptr, 0, total);
    }
    return ptr;
}

void *rsc_rb_realloc(void *ptr, size_t size)
{
    if (size == 0) {
        tlsf_free(ptr);
        return NULL;
    }
    return tlsf_realloc(ptr, size);
}

void rsc_rb_free(void *ptr)
{
    tlsf_free(ptr);
}

int rsc_rb_open(const char *path, int flags, ...)
{
    return rb->open(path, flags, 0666);
}

int rsc_rb_close(int fd)
{
    return rb->close(fd);
}

ssize_t rsc_rb_read(int fd, void *buf, size_t count)
{
    return rb->read(fd, buf, count);
}

ssize_t rsc_rb_write(int fd, const void *buf, size_t count)
{
    return rb->write(fd, buf, count);
}

off_t rsc_rb_lseek(int fd, off_t offset, int whence)
{
    return rb->lseek(fd, offset, whence);
}

char *rsc_rb_strtok(char *str, const char *delim)
{
    static char *last;
    return rb->strtok_r(str, delim, &last);
}

size_t rsc_rb_strcspn(const char *s, const char *reject)
{
    size_t len = 0;
    while (s[len] != '\0') {
        const char *r = reject;
        while (*r != '\0') {
            if (s[len] == *r) {
                return len;
            }
            r++;
        }
        len++;
    }
    return len;
}

double rsc_rb_atof(const char *s)
{
    int sign = 1;
    double value = 0.0;
    double place = 0.1;

    while (*s == ' ' || *s == '\t') {
        s++;
    }
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        value = value * 10.0 + (*s - '0');
        s++;
    }
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') {
            value += (*s - '0') * place;
            place *= 0.1;
            s++;
        }
    }
    return sign * value;
}

static double rsc_wrap_pi(double x)
{
    const double pi = 3.14159265358979323846;
    const double two_pi = 6.28318530717958647692;
    while (x > pi) {
        x -= two_pi;
    }
    while (x < -pi) {
        x += two_pi;
    }
    return x;
}

double rsc_rb_sin(double x)
{
    x = rsc_wrap_pi(x);
    double x2 = x * x;
    return x * (1.0 - x2 / 6.0 + (x2 * x2) / 120.0 -
                (x2 * x2 * x2) / 5040.0);
}

double rsc_rb_cos(double x)
{
    x = rsc_wrap_pi(x);
    double x2 = x * x;
    return 1.0 - x2 / 2.0 + (x2 * x2) / 24.0 -
           (x2 * x2 * x2) / 720.0;
}

double rsc_rb_sqrt(double x)
{
    if (x <= 0.0) {
        return 0.0;
    }
    double guess = x >= 1.0 ? x : 1.0;
    for (int i = 0; i < 8; i++) {
        guess = 0.5 * (guess + x / guess);
    }
    return guess;
}

double rsc_rb_pow(double base, double exp)
{
    int n = (int)exp;
    if ((double)n == exp && n >= 0 && n <= 31) {
        double result = 1.0;
        while (n-- > 0) {
            result *= base;
        }
        return result;
    }
    if (base == 2.0) {
        int whole = (int)exp;
        double frac = exp - whole;
        double result = 1.0;
        while (whole-- > 0) {
            result *= 2.0;
        }
        return result * (1.0 + frac * 0.6931471805599453 +
                         frac * frac * 0.2402265069591007);
    }
    return base;
}

float rsc_rb_powf(float base, float exp)
{
    return (float)rsc_rb_pow(base, exp);
}

float rsc_rb_floorf(float x)
{
    int i = (int)x;
    if ((float)i > x) {
        i--;
    }
    return (float)i;
}

double rsc_rb_ceil(double x)
{
    int i = (int)x;
    if ((double)i < x) {
        i++;
    }
    return (double)i;
}

double rsc_rb_fmin(double x, double y)
{
    return x < y ? x : y;
}

int rsc_rb_system(const char *cmd)
{
    (void)cmd;
    return -1;
}

void mudclient_start_application(mudclient *mud, char *title)
{
    (void)title;
    mud->game_width = LCD_WIDTH;
    mud->game_height = LCD_HEIGHT;
    mud->mouse_x = LCD_WIDTH / 2;
    mud->mouse_y = (LCD_HEIGHT - 24) / 2;
}

static int rsc_clamp(int value, int min, int max)
{
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

static void rsc_sync_player_tile(mudclient *mud)
{
    if (mud->local_player == NULL) {
        return;
    }

    mud->local_region_x = mud->local_player->current_x / MAGIC_LOC;
    mud->local_region_y = mud->local_player->current_y / MAGIC_LOC;
}

static void rsc_move_cursor(mudclient *mud, int dx, int dy)
{
    mudclient_mouse_moved(
        mud, rsc_clamp(mud->mouse_x + dx, 0, mud->game_width - 1),
        rsc_clamp(mud->mouse_y + dy, 0, mud->game_height - 1));
}

static void rsc_set_cursor(mudclient *mud, int x, int y)
{
    mudclient_mouse_moved(mud, rsc_clamp(x, 0, mud->game_width - 1),
                          rsc_clamp(y, 0, mud->game_height - 1));
}

static void rsc_move_cursor_to_player(mudclient *mud)
{
    mudclient_mouse_moved(mud, mud->game_width / 2,
                          (mud->game_height - 48) / 2);
}

static void rsc_move_cursor_to_ui_tab(mudclient *mud)
{
    int tab_spacing = UI_BUTTON_SIZE - 2;
    int x = mud->game_width - 19 - (rsc_ui_tab * tab_spacing);
    int y = 3 + (UI_BUTTON_SIZE / 2);

    rsc_ui_mode = RSC_MODE_TABS;
    mudclient_mouse_moved(mud, rsc_clamp(x, 0, mud->game_width - 1),
                          rsc_clamp(y, 0, mud->game_height - 1));
    mud->show_ui_tab = rsc_ui_tab + 1;
}

static void rsc_cycle_ui_tab(mudclient *mud, int direction)
{
    rsc_ui_tab = (rsc_ui_tab + direction + 6) % 6;
    rsc_panel_index = 0;
    rsc_haptic_menu_move();
    rsc_move_cursor_to_ui_tab(mud);
}

static void rsc_close_ui(mudclient *mud)
{
    mud->show_right_click_menu = 0;
    rsc_ui_mode = RSC_MODE_GAME;
    mud->show_ui_tab = 0;
    rsc_panel_index = 0;
    rsc_context_index = 0;
    rsc_haptic_menu_select();
    rsc_move_cursor_to_player(mud);
}

static void rsc_open_ui_tabs(mudclient *mud)
{
    if (mud->show_ui_tab > 0 && mud->show_ui_tab <= 6) {
        rsc_ui_tab = mud->show_ui_tab - 1;
    } else {
        rsc_ui_tab = 0;
    }
    rsc_panel_index = 0;
    rsc_haptic_menu_select();
    rsc_move_cursor_to_ui_tab(mud);
}

static void rsc_move_inventory_selection(mudclient *mud, int direction)
{
    int columns = mud->surface->height < 260 ? 6 : 5;
    int slot_height = ITEM_GRID_SLOT_HEIGHT;
    int count = mud->inventory_items_count > 0 ? mud->inventory_items_count : 1;
    int width = ITEM_GRID_SLOT_WIDTH * columns;
    int ui_x = mud->surface->width - width - 3;
    int ui_y = UI_BUTTON_SIZE + 1;
    int old_index = rsc_panel_index;

    rsc_panel_index = rsc_clamp(rsc_panel_index + direction, 0, count - 1);
    if (rsc_panel_index != old_index || direction == 0) {
        rsc_haptic_menu_move();
    }

    rsc_set_cursor(mud,
                   ui_x + (rsc_panel_index % columns) * ITEM_GRID_SLOT_WIDTH +
                       (ITEM_GRID_SLOT_WIDTH / 2),
                   ui_y + (rsc_panel_index / columns) * slot_height +
                       (slot_height / 2));
}

static void rsc_move_panel_selection(mudclient *mud, int direction)
{
    if (mud->show_ui_tab == INVENTORY_TAB) {
        rsc_move_inventory_selection(mud, direction);
        return;
    }

    mud->mouse_scroll_delta = direction;
    rsc_move_cursor(mud, 0, direction * 12);
}

static void rsc_enter_ui_panel(mudclient *mud)
{
    rsc_ui_mode = RSC_MODE_PANEL;
    mud->show_ui_tab = rsc_ui_tab + 1;
    rsc_panel_index = 0;
    rsc_haptic_menu_select();

    if (mud->show_ui_tab == INVENTORY_TAB) {
        rsc_move_inventory_selection(mud, 0);
    } else {
        int x = mud->ui_tab_min_x + ((mud->ui_tab_max_x - mud->ui_tab_min_x) / 2);
        int y = UI_BUTTON_SIZE + 24;
        if (mud->ui_tab_max_y > UI_BUTTON_SIZE + 32) {
            y = UI_BUTTON_SIZE + ((mud->ui_tab_max_y - UI_BUTTON_SIZE) / 2);
        }
        mudclient_mouse_moved(mud, rsc_clamp(x, 0, mud->game_width - 1),
                              rsc_clamp(y, 0, mud->game_height - 1));
    }
}

static void rsc_zoom(mudclient *mud, int direction)
{
    mud->camera_zoom = rsc_clamp(mud->camera_zoom + (direction * 44),
                                 ZOOM_MIN, ZOOM_MAX);
    mud->mouse_scroll_delta = 0;
}

#ifdef HAVE_WHEEL_POSITION
static void rsc_queue_zoom(mudclient *mud, int direction)
{
    long now = *rb->current_tick;

    if (now - rsc_last_zoom < HZ / 20) {
        return;
    }

    rsc_zoom(mud, direction);
    rsc_last_zoom = now;
}
#endif

static int rsc_cursor_is_over_game(mudclient *mud)
{
    return mud->show_ui_tab == 0 &&
           mud->mouse_y < mud->game_height - 34 &&
           !mud->show_dialog_bank && !mud->show_dialog_shop &&
           !mud->show_dialog_trade && !mud->show_dialog_duel;
}

static void rsc_click_button(mudclient *mud, int button)
{
    mudclient_mouse_pressed(mud, mud->mouse_x, mud->mouse_y, button);
    mudclient_mouse_released(mud, mud->mouse_x, mud->mouse_y, button);
}

static void rsc_click(mudclient *mud)
{
    rsc_haptic_menu_select();

    if (!mud->show_right_click_menu && rsc_cursor_is_over_game(mud) &&
        mud->menu_items_count > 1) {
        rsc_context_index = 0;
        rsc_click_button(mud, 3);
    } else {
        rsc_click_button(mud, 1);
    }
}

static void rsc_move_context_selection(mudclient *mud, int direction)
{
    int entry_height = mudclient_is_touch(mud) ? 19 : 15;
    int old_index = rsc_context_index;

    if (!mud->show_right_click_menu || mud->menu_items_count <= 0) {
        return;
    }

    rsc_ui_mode = RSC_MODE_CONTEXT;
    rsc_context_index =
        rsc_clamp(rsc_context_index + direction, 0, mud->menu_items_count - 1);
    if (rsc_context_index != old_index) {
        rsc_haptic_menu_move();
    }

    mudclient_mouse_moved(mud, mud->menu_x + (mud->menu_width / 2),
                          mud->menu_y + entry_height + 12 +
                              rsc_context_index * entry_height - 5);
}

#ifdef HAVE_WHEEL_POSITION
static int rsc_wheel_delta(int current, int previous)
{
    int delta = current - previous;

    if (delta > 48) {
        delta -= 96;
    } else if (delta < -48) {
        delta += 96;
    }

    return delta;
}

static void rsc_handle_wheel_tap(mudclient *mud, int pos)
{
    int sector = ((pos + 12) / 24) & 3;
    const int cursor_step = 18;
    long now = *rb->current_tick;

    if (now - rsc_last_wheel_tap < HZ / 20) {
        return;
    }

    rsc_last_wheel_tap = now;

    if (rsc_ui_mode == RSC_MODE_TABS) {
        if (sector == 1) {
            rsc_cycle_ui_tab(mud, 1);
        } else if (sector == 3) {
            rsc_cycle_ui_tab(mud, -1);
        }
        return;
    }

    if (sector == 0) {
        rsc_move_cursor(mud, 0, -cursor_step);
    } else if (sector == 1) {
        rsc_move_cursor(mud, cursor_step, 0);
    } else if (sector == 2) {
        rsc_move_cursor(mud, 0, cursor_step);
    } else {
        rsc_move_cursor(mud, -cursor_step, 0);
    }
}

static void rsc_handle_wheel_scroll(mudclient *mud, int direction)
{
    long now = *rb->current_tick;

    if (mud->show_right_click_menu) {
        if (now - rsc_last_wheel_scroll < HZ / 15) {
            return;
        }
        rsc_move_context_selection(mud, direction);
        rsc_last_wheel_scroll = now;
    } else if (rsc_ui_mode == RSC_MODE_TABS) {
        if (now - rsc_last_wheel_scroll < HZ / 10) {
            return;
        }
        rsc_cycle_ui_tab(mud, direction);
        rsc_last_wheel_scroll = now;
    } else if (rsc_ui_mode == RSC_MODE_PANEL) {
        if (now - rsc_last_wheel_scroll < HZ / 15) {
            return;
        }
        rsc_move_panel_selection(mud, direction);
        rsc_last_wheel_scroll = now;
    } else {
        rsc_queue_zoom(mud, direction > 0 ? -1 : 1);
    }
}

static void rsc_poll_wheel_position(mudclient *mud)
{
    int pos = rb->wheel_status();

    if (pos < 0) {
        rsc_last_wheel_pos = -1;
        rsc_wheel_accum = 0;
        return;
    }

    if (rsc_last_wheel_pos >= 0) {
        int delta = rsc_wheel_delta(pos, rsc_last_wheel_pos);
        rsc_wheel_accum += delta;

        int threshold = 4;

        if (rsc_wheel_accum >= threshold || rsc_wheel_accum <= -threshold) {
            int direction = rsc_wheel_accum > 0 ? 1 : -1;
            rsc_handle_wheel_scroll(mud, direction);
            rsc_wheel_accum -= direction * threshold;
        } else if (delta == 0) {
            rsc_handle_wheel_tap(mud, pos);
        }
    } else {
        rsc_wheel_accum = 0;
        rsc_handle_wheel_tap(mud, pos);
    }

    rsc_last_wheel_pos = pos;
}
#endif

void mudclient_poll_events(mudclient *mud)
{
    long button;
    long held;

    rsc_sync_player_tile(mud);
    mud->key_left = 0;
    mud->key_right = 0;

#ifdef HAVE_WHEEL_POSITION
    rsc_poll_wheel_position(mud);
#endif

#ifdef HAS_BUTTON_HOLD
    if (rb->button_hold()) {
        if (!rsc_hold_was_on) {
            rsc_logoff_prompt = 1;
            rb->button_clear_queue();
        }
        rsc_hold_was_on = 1;
        return;
    }
    rsc_hold_was_on = 0;
#endif

    held = rb->button_status();
    if (held & BUTTON_LEFT) {
        mud->key_left = 1;
    }
    if (held & BUTTON_RIGHT) {
        mud->key_right = 1;
    }

    while ((button = rb->button_get(false)) != BUTTON_NONE) {
        long base = button & ~(BUTTON_REPEAT | BUTTON_REL);

        if (button & BUTTON_REL) {
            continue;
        }

        if (rsc_logoff_prompt) {
            if (base == BUTTON_SELECT) {
                mudclient_save_offline_game(mud);
                rsc_quit = 1;
                mud->stop_timeout = -1;
            }
            rsc_logoff_prompt = 0;
            return;
        }

        switch (base) {
        case BUTTON_SELECT:
            if (rsc_ui_mode == RSC_MODE_TABS) {
                rsc_enter_ui_panel(mud);
            } else {
                rsc_click(mud);
            }
            break;
        case BUTTON_LEFT:
            mud->key_left = 1;
            break;
        case BUTTON_RIGHT:
            mud->key_right = 1;
            break;
        case BUTTON_MENU:
            if (!(button & BUTTON_REPEAT)) {
                if (mud->show_right_click_menu) {
                    mud->show_right_click_menu = 0;
                    rsc_ui_mode = RSC_MODE_GAME;
                    rsc_context_index = 0;
                } else if (rsc_ui_mode == RSC_MODE_GAME &&
                           mud->show_ui_tab == 0) {
                    rsc_open_ui_tabs(mud);
                } else {
                    rsc_close_ui(mud);
                }
            }
            break;
#ifdef BUTTON_PLAY
        case BUTTON_PLAY:
            if (!(button & BUTTON_REPEAT)) {
                rsc_debug_overlay = !rsc_debug_overlay;
                if (mud->options != NULL) {
                    mud->options->show_hover_tooltip = 1;
                }
                rsc_haptic_action();
            }
            break;
#endif
        case BUTTON_SCROLL_BACK:
#ifndef HAVE_WHEEL_POSITION
            if (mud->show_right_click_menu) {
                rsc_move_context_selection(mud, -1);
            } else if (rsc_ui_mode == RSC_MODE_TABS) {
                rsc_cycle_ui_tab(mud, -1);
            } else if (rsc_ui_mode == RSC_MODE_PANEL) {
                rsc_move_panel_selection(mud, -1);
            } else {
                rsc_zoom(mud, 1);
            }
#endif
            break;
        case BUTTON_SCROLL_FWD:
#ifndef HAVE_WHEEL_POSITION
            if (mud->show_right_click_menu) {
                rsc_move_context_selection(mud, 1);
            } else if (rsc_ui_mode == RSC_MODE_TABS) {
                rsc_cycle_ui_tab(mud, 1);
            } else if (rsc_ui_mode == RSC_MODE_PANEL) {
                rsc_move_panel_selection(mud, 1);
            } else {
                rsc_zoom(mud, -1);
            }
#endif
            break;
        default:
            break;
        }
    }
}

static fb_data rsc_lcd[LCD_HEIGHT][LCD_WIDTH];

static fb_data rsc_pack_colour(int32_t colour)
{
#if LCD_DEPTH == 16 && LCD_PIXELFORMAT == RGB565
    return (fb_data)(((colour & 0xf80000) >> 8) |
                     ((colour & 0x00fc00) >> 5) |
                     ((colour & 0x0000f8) >> 3));
#else
    return FB_RGBPACK((colour >> 16) & 0xff, (colour >> 8) & 0xff,
                      colour & 0xff);
#endif
}

static void rsc_lcd_hline(int x, int y, int width, fb_data colour)
{
    if (y < 0 || y >= LCD_HEIGHT) {
        return;
    }

    if (x < 0) {
        width += x;
        x = 0;
    }

    if (x + width > LCD_WIDTH) {
        width = LCD_WIDTH - x;
    }

    for (int i = 0; i < width; i++) {
        rsc_lcd[y][x + i] = colour;
    }
}

static void rsc_lcd_vline(int x, int y, int height, fb_data colour)
{
    if (x < 0 || x >= LCD_WIDTH) {
        return;
    }

    if (y < 0) {
        height += y;
        y = 0;
    }

    if (y + height > LCD_HEIGHT) {
        height = LCD_HEIGHT - y;
    }

    for (int i = 0; i < height; i++) {
        rsc_lcd[y + i][x] = colour;
    }
}

static void rsc_lcd_rect(int x, int y, int width, int height, fb_data colour)
{
    rsc_lcd_hline(x, y, width, colour);
    rsc_lcd_hline(x, y + height - 1, width, colour);
    rsc_lcd_vline(x, y, height, colour);
    rsc_lcd_vline(x + width - 1, y, height, colour);
}

static void rsc_draw_inventory_selection(mudclient *mud)
{
    if (rsc_ui_mode != RSC_MODE_PANEL || mud->show_ui_tab != INVENTORY_TAB ||
        rsc_panel_index < 0 || rsc_panel_index >= mud->inventory_items_count) {
        return;
    }

    int columns = mud->surface->height < 260 ? 6 : 5;
    int slot_height = ITEM_GRID_SLOT_HEIGHT;
    int width = ITEM_GRID_SLOT_WIDTH * columns;
    int ui_x = mud->surface->width - width - 3;
    int ui_y = UI_BUTTON_SIZE + 1;
    int slot_x = ui_x + (rsc_panel_index % columns) * ITEM_GRID_SLOT_WIDTH;
    int slot_y = ui_y + (rsc_panel_index / columns) * slot_height;
    fb_data yellow = FB_RGBPACK(255, 220, 0);
    fb_data black = FB_RGBPACK(0, 0, 0);

    rsc_lcd_rect(slot_x, slot_y, ITEM_GRID_SLOT_WIDTH, slot_height, black);
    rsc_lcd_rect(slot_x + 1, slot_y + 1, ITEM_GRID_SLOT_WIDTH - 2,
                 slot_height - 2, yellow);
    rsc_lcd_rect(slot_x + 3, slot_y + 3, ITEM_GRID_SLOT_WIDTH - 6,
                 slot_height - 6, yellow);
}

static void rsc_draw_cursor(int x, int y)
{
    fb_data yellow = FB_RGBPACK(255, 220, 0);
    fb_data black = FB_RGBPACK(0, 0, 0);

    for (int i = -6; i <= 6; i++) {
        int px = x + i;
        if (px >= 0 && px < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT) {
            rsc_lcd[y][px] = black;
        }

        int py = y + i;
        if (x >= 0 && x < LCD_WIDTH && py >= 0 && py < LCD_HEIGHT) {
            rsc_lcd[py][x] = black;
        }
    }

    for (int i = -4; i <= 4; i++) {
        int px = x + i;
        if (px >= 0 && px < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT) {
            rsc_lcd[y][px] = yellow;
        }

        int py = y + i;
        if (x >= 0 && x < LCD_WIDTH && py >= 0 && py < LCD_HEIGHT) {
            rsc_lcd[py][x] = yellow;
        }
    }

    if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT) {
        rsc_lcd[y][x] = black;
    }
}

static void rsc_debug_overlay_line(int *y, const char *text)
{
    rb->lcd_putsxy(5, *y, text);
    *y += 10;
}

static int rsc_debug_top_menu_target(mudclient *mud, struct MenuEntry **entry)
{
    int index;

    if (mud == NULL || entry == NULL || mud->menu_items_count <= 0 ||
        mud->menu_items == NULL || mud->menu_indices == NULL) {
        return 0;
    }

    index = mud->menu_indices[0];
    if (index < 0 || index >= mud->menu_items_count) {
        return 0;
    }

    *entry = &mud->menu_items[index];
    return 1;
}

static void rsc_draw_debug_overlay(mudclient *mud)
{
    unsigned fg;
    unsigned bg;
    char line[96];
    int y = 4;
    int player_lx = -1;
    int player_ly = -1;
    int player_gx = -1;
    int player_gy = -1;
    int spawn_lx;
    int spawn_ly;
    struct MenuEntry *entry = NULL;

    if (mud == NULL) {
        return;
    }

    if (mud->local_player != NULL) {
        player_lx = mud->local_player->current_x / MAGIC_LOC;
        player_ly = mud->local_player->current_y / MAGIC_LOC;
        player_gx = mud->region_x + player_lx;
        player_gy = mud->region_y + player_ly;
    }

    spawn_lx = RSC_LUMBRIDGE_CASTLE_X - mud->region_x;
    spawn_ly = RSC_LUMBRIDGE_CASTLE_Y - mud->region_y;

    fg = rb->lcd_get_foreground();
    bg = rb->lcd_get_background();
    rb->lcd_set_foreground(FB_RGBPACK(0, 0, 0));
    rb->lcd_fillrect(0, 0, LCD_WIDTH, 86);
    rb->lcd_set_foreground(FB_RGBPACK(255, 220, 0));
    rb->lcd_drawrect(0, 0, LCD_WIDTH, 86);
    rb->lcd_set_foreground(FB_RGBPACK(255, 255, 255));

    snprintf(line, sizeof(line), "RSC debug  data:%s", RSC_DATA_DIR);
    rsc_debug_overlay_line(&y, line);
    snprintf(line, sizeof(line), "player G:%d,%d L:%d,%d", player_gx,
             player_gy, player_lx, player_ly);
    rsc_debug_overlay_line(&y, line);
    snprintf(line, sizeof(line), "spawn  G:%d,%d L:%d,%d",
             RSC_LUMBRIDGE_CASTLE_X, RSC_LUMBRIDGE_CASTLE_Y, spawn_lx,
             spawn_ly);
    rsc_debug_overlay_line(&y, line);
    snprintf(line, sizeof(line), "map:%dx%d region:%d,%d",
             REGION_WIDTH, REGION_HEIGHT, mud->region_x, mud->region_y);
    rsc_debug_overlay_line(&y, line);
    snprintf(line, sizeof(line), "npcs:%d items:%d scene:%d",
             mud->npc_count, mud->ground_item_count,
             mud->scene != NULL ? mud->scene->model_count : -1);
    rsc_debug_overlay_line(&y, line);

    if (rsc_debug_top_menu_target(mud, &entry)) {
        snprintf(line, sizeof(line), "cursor L:%d,%d %s %s",
                 entry->x, entry->y, entry->action_text,
                 entry->target_text);
    } else {
        snprintf(line, sizeof(line), "cursor screen:%d,%d",
                 mud->mouse_x, mud->mouse_y);
    }
    rsc_debug_overlay_line(&y, line);

    rb->lcd_set_foreground(fg);
    rb->lcd_set_background(bg);
}

void rsc_surface_draw_rockbox(Surface *surface)
{
    int width = surface->width < LCD_WIDTH ? surface->width : LCD_WIDTH;
    int height = surface->height < LCD_HEIGHT ? surface->height : LCD_HEIGHT;

    for (int y = 0; y < height; y++) {
        int32_t *src = surface->pixels + y * surface->width;
        fb_data *dst = rsc_lcd[y];

        for (int x = 0; x < width; x++) {
            dst[x] = rsc_pack_colour(src[x]);
        }
    }

    if (surface->mud != NULL) {
        rsc_draw_inventory_selection(surface->mud);
        rsc_draw_cursor(surface->mud->mouse_x, surface->mud->mouse_y);
    }

    rb->lcd_bitmap(&rsc_lcd[0][0], 0, 0, width, height);
    if (surface->mud != NULL && rsc_debug_overlay) {
        rsc_draw_debug_overlay(surface->mud);
    }
    if (rsc_logoff_prompt) {
        unsigned fg = rb->lcd_get_foreground();
        unsigned bg = rb->lcd_get_background();
        rb->lcd_set_foreground(FB_RGBPACK(0, 0, 0));
        rb->lcd_fillrect(37, 91, 246, 58);
        rb->lcd_set_foreground(FB_RGBPACK(255, 220, 0));
        rb->lcd_drawrect(37, 91, 246, 58);
        rb->lcd_putsxy(82, 104, "Log off?");
        rb->lcd_putsxy(53, 122, "Select exits, any key stays");
        rb->lcd_set_foreground(fg);
        rb->lcd_set_background(bg);
    }
    rb->lcd_update();
}

enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;
    rsc_boot_status("RSC starting");
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(false);
#endif
    rsc_rb_init_alloc();
    if (rsc_pool == NULL || rsc_pool_size < (3 * 1024 * 1024)) {
        rb->splashf(HZ * 2, "RSC: not enough memory");
#ifdef HAVE_WHEEL_POSITION
        rb->wheel_send_events(true);
#endif
        return PLUGIN_ERROR;
    }

    init_utility_global();
    init_surface_global();
    init_world_global();
    init_stats_tab_global();

    mudclient *mud = malloc(sizeof(mudclient));
    if (mud == NULL) {
        rb->splashf(HZ * 2, "RSC: client alloc failed");
        return PLUGIN_ERROR;
    }

    mudclient_new(mud);
    if (!rsc_select_offline_profile(mud)) {
        free(mud);
        return PLUGIN_OK;
    }

    mudclient_start_application(mud, "Runescape by Andrew Gower");
    mudclient_start_application_common(mud);
    mudclient_save_offline_game(mud);

#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
    return rsc_quit ? PLUGIN_OK : PLUGIN_ERROR;
}
