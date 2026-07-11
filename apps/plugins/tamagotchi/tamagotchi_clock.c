#include "tamagotchi_clock.h"
#include "tamagotchi_display.h"
#include "upstream/tamalib/tamalib.h"

#define CLOCK_UNSET_TICK_LIMIT (32768u * 120u)
#define CLOCK_BOOT_STEPS 140000
#define CLOCK_BOOT_YIELD_STEPS 512
#define CLOCK_POST_SET_BASE_TICKS 1248002u
#define CLOCK_TICKS_PER_SECOND 32768u
#define CLOCK_RESYNC_THRESHOLD (CLOCK_TICKS_PER_SECOND * 2u)

static bool autosetting;

static const char post_clock_ram[MEM_RAM_SIZE + 1] =
    "82500010000000000010020000000000000064000004000620"
    "c680d74c4c00b611000050000f0000000000000fd000110013"
    "acd001b90101c0000000000f5f200000000000000000000000"
    "000000000000000000000000000000000000000000000000000"
    "0000000000000000771771771771771771d700771e20002120"
    "e650000000000000000000000000000000000000000000000000"
    "000000000000000000000000000000e31414e3002200e31414"
    "e300000020f70000c744c700c744c7000000000000000000000"
    "0000000000000000000000000000000000000000000000000ee"
    "525c52feee000000000000000000c429294600ef29290020ef"
    "2000000020fff00000000f00f005820000000000000000000000"
    "000000000000000000000000000000000000000000000000000"
    "000000000000000700000000000000000000000000000000000"
    "000000000000000000000000000000000000000000000000000"
    "000000000000000000000000000000000000000000000000000"
    "000000";

static bool valid_ipod_time(const struct tm *tm)
{
    return tm != NULL &&
           tm->tm_hour >= 0 && tm->tm_hour <= 23 &&
           tm->tm_min >= 0 && tm->tm_min <= 59;
}

static void step_many(int steps)
{
    int until_yield = CLOCK_BOOT_YIELD_STEPS;

    while (steps-- > 0)
    {
        tamalib_step();
        if (--until_yield <= 0)
        {
            rb->yield();
            until_yield = CLOCK_BOOT_YIELD_STEPS;
        }
    }
}

static u32_t seconds_since_midnight(const struct tm *tm)
{
    return (u32_t)(tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec);
}

static void apply_post_clock_state(state_t *state, const struct tm *tm)
{
    size_t i;
    u32_t ticks = CLOCK_POST_SET_BASE_TICKS +
                  seconds_since_midnight(tm) * CLOCK_TICKS_PER_SECOND;

    for (i = 0; i < MEM_RAM_SIZE; i++)
    {
        char c = post_clock_ram[i];
        u8_t value = (u8_t)(c <= '9' ? c - '0' : c - 'a' + 10);

        SET_MEMORY(state->memory, i, value);
    }

    *state->pc = 0x0167;
    *state->x = 0x02e;
    *state->y = 0x100;
    *state->a = 0x0;
    *state->b = 0x0;
    *state->np = 0x01;
    *state->sp = 0xf9;
    *state->flags = 0x0a;
    *state->tick_counter = ticks;
    *state->clk_timer_2hz_timestamp = ticks;
    *state->clk_timer_4hz_timestamp = ticks;
    *state->clk_timer_8hz_timestamp = ticks;
    *state->clk_timer_16hz_timestamp = ticks;
    *state->clk_timer_32hz_timestamp = ticks;
    *state->clk_timer_64hz_timestamp = ticks;
    *state->clk_timer_128hz_timestamp = ticks;
    *state->clk_timer_256hz_timestamp = ticks;
    *state->prog_timer_timestamp = ticks;
    *state->prog_timer_enabled = 0;
    *state->cpu_halted = 0;
}

static void set_clock_timers(state_t *state, u32_t ticks)
{
    *state->tick_counter = ticks;
    *state->clk_timer_2hz_timestamp = ticks;
    *state->clk_timer_4hz_timestamp = ticks;
    *state->clk_timer_8hz_timestamp = ticks;
    *state->clk_timer_16hz_timestamp = ticks;
    *state->clk_timer_32hz_timestamp = ticks;
    *state->clk_timer_64hz_timestamp = ticks;
    *state->clk_timer_128hz_timestamp = ticks;
    *state->clk_timer_256hz_timestamp = ticks;
    *state->prog_timer_timestamp = ticks;
}

bool tamagotchi_clock_autoset_from_ipod(void)
{
    const struct tm *tm = rb->get_time();
    state_t *state = tamalib_get_state();

    if (!valid_ipod_time(tm) || state == NULL)
        return false;

    autosetting = true;
    tamalib_set_speed(0);

    tamalib_reset();
    tamalib_refresh_hw();
    step_many(CLOCK_BOOT_STEPS);
    apply_post_clock_state(state, tm);
    tamalib_set_speed(1);
    autosetting = false;
    tamalib_refresh_hw();
    tamagotchi_display_mark_dirty();
    return true;
}

bool tamagotchi_clock_sync_to_ipod(void)
{
    const struct tm *tm = rb->get_time();
    state_t *state = tamalib_get_state();
    u32_t ticks;
    u32_t diff;

    if (!valid_ipod_time(tm) || state == NULL)
        return false;

    ticks = CLOCK_POST_SET_BASE_TICKS +
            seconds_since_midnight(tm) * CLOCK_TICKS_PER_SECOND;
    diff = *state->tick_counter > ticks ?
           *state->tick_counter - ticks : ticks - *state->tick_counter;

    if (diff < CLOCK_RESYNC_THRESHOLD)
        return true;

    set_clock_timers(state, ticks);
    tamalib_refresh_hw();
    tamagotchi_display_mark_dirty();
    return true;
}

bool tamagotchi_clock_state_needs_seed(void)
{
    state_t *state = tamalib_get_state();

    if (state == NULL || state->pc == NULL)
        return false;

    if (tamagotchi_clock_state_looks_unset())
        return true;

    if (*state->pc == 0x00a7)
        return true;

    return state->x != NULL && state->y != NULL &&
           *state->x == 0xebe && *state->y == 0x1aa &&
           *state->pc >= 0x0080 && *state->pc <= 0x00c8;
}

bool tamagotchi_clock_state_looks_unset(void)
{
    state_t *state = tamalib_get_state();

    return state != NULL &&
           state->tick_counter != NULL &&
           *state->tick_counter < CLOCK_UNSET_TICK_LIMIT;
}

bool tamagotchi_clock_is_autosetting(void)
{
    return autosetting;
}

void tamagotchi_clock_set_background_work(bool enable)
{
    autosetting = enable;
}

void tamagotchi_clock_filter_buttons(bool *a, bool *b, bool *c)
{
    if (*a && *c)
    {
        *a = false;
        *c = false;
    }
}
