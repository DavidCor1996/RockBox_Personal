/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_ /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/                \/
 *
 * iPod-focused runtime services for the Flash player plugin.
 *
 ****************************************************************************/

#include "lib/plugin_cxx_compat.h"
#include "ipod_engine.h"

#define IPOD_ENGINE_DEFAULT_FPS_X100 3500
#define IPOD_ENGINE_MIN_FPS_X100     100
#define IPOD_ENGINE_MAX_FPS_X100     6000
#define IPOD_ENGINE_MAX_DT_TICKS     (HZ / 4)

void ipod_engine_get_profile(struct ipod_engine_profile *profile)
{
    if (!profile)
        return;

    profile->target = "unsupported";
    profile->model = MODEL_NAME;
    profile->lcd_width = LCD_WIDTH;
    profile->lcd_height = LCD_HEIGHT;
    profile->plugin_buffer_min = 3 * 1024 * 1024;
    profile->supported = false;

#if defined(IPOD_6G)
    profile->target = "ipod6g";
    profile->supported = true;
#elif defined(SIMULATOR)
    profile->target = "simulator";
    profile->supported = true;
#endif
}

bool ipod_engine_supported_target(void)
{
    struct ipod_engine_profile profile;

    ipod_engine_get_profile(&profile);
    return profile.supported;
}

void ipod_engine_memory_init(struct ipod_engine_memory *memory)
{
    if (!memory)
        return;

    rb->memset(memory, 0, sizeof(*memory));
}

bool ipod_engine_acquire_memory(struct ipod_engine_memory *memory)
{
    if (!memory)
        return false;

    ipod_engine_memory_init(memory);
    memory->plugin = (unsigned char *)rb->plugin_get_buffer(
        &memory->plugin_size);
    memory->shared = rb->plugin_get_audio_buffer(&memory->shared_size);
    memory->shared_acquired = memory->shared != NULL;

    if (memory->plugin && memory->plugin_size > 0 &&
        memory->shared && memory->shared_size > 0)
        return true;

    ipod_engine_release_memory(memory);
    return false;
}

void ipod_engine_release_memory(struct ipod_engine_memory *memory)
{
    if (!memory)
        return;

    if (memory->shared_acquired)
        rb->plugin_release_audio_buffer();

    ipod_engine_memory_init(memory);
}

void ipod_engine_frame_clock_reset(struct ipod_engine_frame_clock *clock,
                                   int fps_x100)
{
    long now;

    if (!clock)
        return;

    if (fps_x100 < IPOD_ENGINE_MIN_FPS_X100 ||
        fps_x100 > IPOD_ENGINE_MAX_FPS_X100)
        fps_x100 = IPOD_ENGINE_DEFAULT_FPS_X100;

    now = *rb->current_tick;
    clock->fps_x100 = fps_x100;
    clock->start_tick = now;
    clock->last_tick = now;
    clock->frame_index = 0;
}

int ipod_engine_frame_timeout(const struct ipod_engine_frame_clock *clock)
{
    long now;
    long target;
    long wait;

    if (!clock || clock->fps_x100 <= 0)
        return HZ / 35;

    now = *rb->current_tick;
    target = clock->start_tick +
        ((long)(clock->frame_index + 1) * HZ * 100) / clock->fps_x100;
    wait = target - now;

    if (wait <= 0)
        return 0;
    if (wait > HZ / 8)
        return HZ / 8;

    return (int)wait;
}

float ipod_engine_frame_advance(struct ipod_engine_frame_clock *clock)
{
    long now;
    long ticks;

    if (!clock || clock->fps_x100 <= 0)
        return 1.0f / 35.0f;

    now = *rb->current_tick;
    ticks = now - clock->last_tick;

    if (ticks <= 0)
        ticks = (HZ * 100) / clock->fps_x100;
    if (ticks > IPOD_ENGINE_MAX_DT_TICKS)
        ticks = IPOD_ENGINE_MAX_DT_TICKS;

    clock->last_tick = now;
    clock->frame_index++;

    return (float)ticks / (float)HZ;
}
