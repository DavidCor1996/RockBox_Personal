#include "tamagotchi_hal.h"
#include "tamagotchi_audio.h"
#include "tamagotchi_clock.h"
#include "tamagotchi_display.h"
#include "upstream/tamalib/tamalib.h"

static const struct tamagotchi_settings *hal_settings;
static hal_t rockbox_hal;
static long tama_start_tick;
static bool halted;

static void *hal_malloc(u32_t size)
{
    (void)size;
    return NULL;
}

static void hal_free(void *ptr)
{
    (void)ptr;
}

static void hal_halt(void)
{
    halted = true;
}

static bool_t hal_is_log_enabled(log_level_t level)
{
    (void)level;
    return 0;
}

static void hal_log(log_level_t level, char *buff, ...)
{
    (void)level;
    (void)buff;
}

static timestamp_t hal_get_timestamp(void)
{
    long ticks = *rb->current_tick - tama_start_tick;
    return (timestamp_t)(((long long)ticks * 1000000) / HZ);
}

static void hal_sleep_until(timestamp_t ts)
{
    timestamp_t now = hal_get_timestamp();
    int diff = (int)(ts - now);

    if (diff <= 0)
        return;

    if (diff >= (1000000 / HZ))
        rb->sleep((diff * HZ) / 1000000);
    else
        rb->yield();
}

static void hal_update_screen(void)
{
    tamagotchi_display_render(hal_settings);
    rb->lcd_update();
}

static void hal_set_lcd_matrix(u8_t x, u8_t y, bool_t val)
{
    tamagotchi_display_set_pixel(x, y, val != 0);
}

static void hal_set_lcd_icon(u8_t icon, bool_t val)
{
    tamagotchi_display_set_icon(icon, val != 0);
}

static void hal_set_frequency(u32_t freq)
{
    tamagotchi_audio_set_frequency(freq);
}

static void hal_play_frequency(bool_t en)
{
    tamagotchi_audio_play(en != 0);
}

static int hal_handler(void)
{
    struct tamagotchi_input_state state;

    tamagotchi_hal_poll(&state);
    return state.quit_requested ? 1 : 0;
}

void tamagotchi_hal_init(const struct tamagotchi_settings *settings)
{
    hal_settings = settings;
    tama_start_tick = *rb->current_tick;
    halted = false;

    rockbox_hal.malloc = hal_malloc;
    rockbox_hal.free = hal_free;
    rockbox_hal.halt = hal_halt;
    rockbox_hal.is_log_enabled = hal_is_log_enabled;
    rockbox_hal.log = hal_log;
    rockbox_hal.sleep_until = hal_sleep_until;
    rockbox_hal.get_timestamp = hal_get_timestamp;
    rockbox_hal.update_screen = hal_update_screen;
    rockbox_hal.set_lcd_matrix = hal_set_lcd_matrix;
    rockbox_hal.set_lcd_icon = hal_set_lcd_icon;
    rockbox_hal.set_frequency = hal_set_frequency;
    rockbox_hal.play_frequency = hal_play_frequency;
    rockbox_hal.handler = hal_handler;
}

hal_t *tamagotchi_hal_get(void)
{
    return &rockbox_hal;
}

void tamagotchi_hal_poll(struct tamagotchi_input_state *state)
{
    tamagotchi_input_poll(hal_settings, state);
    tamagotchi_clock_filter_buttons(&state->a, &state->b, &state->c);
    tamalib_set_button(BTN_LEFT,
                       state->a ? BTN_STATE_PRESSED : BTN_STATE_RELEASED);
    tamalib_set_button(BTN_MIDDLE,
                       state->b ? BTN_STATE_PRESSED : BTN_STATE_RELEASED);
    tamalib_set_button(BTN_RIGHT,
                       state->c ? BTN_STATE_PRESSED : BTN_STATE_RELEASED);
}

bool tamagotchi_hal_halted(void)
{
    return halted;
}
