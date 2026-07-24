/***************************************************************************
 * Minimal libretro shim and arena for the isolated Snes9x2002 core.
 ***************************************************************************/

#include "snes_lite.h"
#include "libretro/libretro-common/include/libretro.h"
#include <stdarg.h>

static unsigned char *arena_ptr;
static unsigned char *arena_end;
static retro_audio_buffer_status_callback_t audio_status_callback;
void retro_set_environment(retro_environment_t cb);
void retro_set_video_refresh(retro_video_refresh_t cb);
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb);
void retro_set_input_poll(retro_input_poll_t cb);
void retro_set_input_state(retro_input_state_t cb);
void retro_init(void);
void retro_deinit(void);
bool retro_load_game(const struct retro_game_info *game);
void retro_unload_game(void);
void retro_run(void);
void retro_reset(void);
void retro_get_system_av_info(struct retro_system_av_info *info);
void *retro_get_memory_data(unsigned type);
size_t retro_get_memory_size(unsigned type);

void snes_lite_arena_init(void *buffer, size_t size)
{
    uintptr_t aligned = ((uintptr_t)buffer + 15) & ~(uintptr_t)15;

    arena_ptr = (unsigned char *)aligned;
    arena_end = (unsigned char *)buffer + size;
    snes_lite.arena_size = arena_end > arena_ptr ?
                           (size_t)(arena_end - arena_ptr) : 0;
    snes_lite.arena_used = 0;
}

void *snes_lite_malloc(size_t size)
{
    unsigned char *result;

    size = (size + 15) & ~(size_t)15;
    if (size == 0)
        size = 16;
    if (!arena_ptr || size > (size_t)(arena_end - arena_ptr))
    {
        snes_lite.core_failed = true;
        return NULL;
    }
    result = arena_ptr;
    arena_ptr += size;
    snes_lite.arena_used += size;
    return result;
}

void *snes_lite_try_malloc(size_t size)
{
    bool failed = snes_lite.core_failed;
    void *result = snes_lite_malloc(size);

    snes_lite.core_failed = failed;
    return result;
}

void *snes_lite_calloc(size_t count, size_t size)
{
    size_t total;
    void *result;

    if (count && size > (size_t)-1 / count)
        return NULL;
    total = count * size;
    result = snes_lite_malloc(total);
    if (result)
        rb->memset(result, 0, total);
    return result;
}

void snes_lite_free(void *ptr)
{
    (void)ptr;
}

void snes_lite_core_abort(int status)
{
    (void)status;
    snes_lite.core_failed = true;
}

int snes_lite_sprintf(char *buffer, const char *format, ...)
{
    va_list ap;
    int result;

    va_start(ap, format);
    result = rb->vsnprintf(buffer, 256, format, ap);
    va_end(ap);
    return result;
}

long snes_lite_strtol(const char *text, char **end, int base)
{
    long value = 0;
    int sign = 1;
    const char *cursor = text;

    while (*cursor == ' ' || *cursor == '\t')
        cursor++;
    if (*cursor == '-')
    {
        sign = -1;
        cursor++;
    }
    else if (*cursor == '+')
        cursor++;
    if (base == 0)
        base = 10;
    while (*cursor >= '0' && *cursor <= '9')
    {
        int digit = *cursor - '0';
        if (digit >= base)
            break;
        value = value * base + digit;
        cursor++;
    }
    if (end)
        *end = (char *)cursor;
    return value * sign;
}

char *snes_lite_strncpy(char *destination, const char *source, size_t count)
{
    size_t i;

    for (i = 0; i < count && source[i]; i++)
        destination[i] = source[i];
    while (i < count)
        destination[i++] = '\0';
    return destination;
}

int snes_lite_sscanf(const char *text, const char *format, ...)
{
    (void)text;
    (void)format;
    return 0;
}

void *snes_lite_bsearch(const void *key, const void *base, size_t count,
                        size_t size, int (*compare)(const void *,
                                                   const void *))
{
    size_t i;
    const unsigned char *item = base;

    for (i = 0; i < count; i++, item += size)
    {
        if (compare(key, item) == 0)
            return (void *)item;
    }
    return NULL;
}

long snes_lite_time(long *value)
{
    long result = *rb->current_tick / HZ;

    if (value)
        *value = result;
    return result;
}

/*
 * Rockbox plugins do not link libm, but the DSP-1 projection code needs real
 * trigonometry.  Reduce to [-pi/2, pi/2] before evaluating the polynomial so
 * the table setup and the per-frame projection stay accurate and bounded.
 */
float sinf(float value)
{
    const float pi = 3.14159265358979323846f;
    const float half_pi = 1.57079632679489661923f;
    const float two_pi = 6.28318530717958647692f;
    float squared;

    while (value > pi)
        value -= two_pi;
    while (value < -pi)
        value += two_pi;

    if (value > half_pi)
        value = pi - value;
    else if (value < -half_pi)
        value = -pi - value;

    squared = value * value;
    return value *
           (1.0f + squared *
           (-0.1666666716f + squared *
           (0.0083333477f + squared *
           (-0.0001984090f + squared *
           (0.0000027526f + squared * -0.0000000239f)))));
}

float cosf(float value)
{
    return sinf(value + 1.57079632679489661923f);
}

float tanf(float value)
{
    float cosine = cosf(value);

    if (cosine == 0.0f)
        return sinf(value) < 0.0f ? -1.0e30f : 1.0e30f;
    return sinf(value) / cosine;
}

float sqrtf(float value)
{
    float result;
    int i;

    if (value <= 0.0f)
        return 0.0f;
    result = value > 1.0f ? value : 1.0f;
    for (i = 0; i < 8; i++)
        result = (result + value / result) * 0.5f;
    return result;
}

static bool environment_cb(unsigned command, void *data)
{
    switch (command)
    {
        case RETRO_ENVIRONMENT_GET_OVERSCAN:
            *(bool *)data = false;
            return true;
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            return *(enum retro_pixel_format *)data ==
                   RETRO_PIXEL_FORMAT_RGB565;
        case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
        case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
        case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY:
            return true;
        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
            *(bool *)data = snes_lite.variables_changed;
            snes_lite.variables_changed = false;
            return true;
        case RETRO_ENVIRONMENT_GET_VARIABLE:
        {
            struct retro_variable *variable = data;
            variable->value = NULL;
            if (!rb->strcmp(variable->key, "snes9x2002_frameskip"))
                /* Audio occupancy is not a reliable render-load signal on
                 * Rockbox. It caused sustained 31-of-32 frame drops while
                 * the emulator itself was maintaining native speed. */
                variable->value = snes_lite.effective_frameskip > 0 ?
                                  "fixed_interval" : "disabled";
            else if (!rb->strcmp(variable->key,
                                 "snes9x2002_frameskip_threshold"))
                variable->value = "45";
            else if (!rb->strcmp(variable->key,
                                 "snes9x2002_frameskip_interval"))
            {
                static char interval[4];
                rb->snprintf(interval, sizeof(interval), "%d",
                             MAX(1, snes_lite.effective_frameskip));
                variable->value = interval;
            }
            else if (!rb->strcmp(variable->key, "snes9x2002_transparency"))
                /* Background color math and transparency are required for
                 * correct rendering. Never trade them for speed. */
                variable->value = "enabled";
            else if (!rb->strcmp(variable->key,
                                 "snes9x2002_low_pass_filter"))
                variable->value = snes_lite.config.audio == SNES_AUDIO_LOW ||
                                  snes_lite.config.audio == SNES_AUDIO_OFF ?
                                  "disabled" : "enabled";
            else if (!rb->strcmp(variable->key,
                                 "snes9x2002_low_pass_range"))
                variable->value = "65";
            else if (!rb->strcmp(variable->key,
                                 "snes9x2002_overclock_cycles"))
                variable->value = "disabled";
            return variable->value != NULL;
        }
        case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK:
            audio_status_callback = data ?
                ((struct retro_audio_buffer_status_callback *)data)->callback :
                NULL;
            return true;
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
            return false;
        default:
            return false;
    }
}

static size_t audio_cb(const int16_t *data, size_t frames)
{
    snes_lite_audio_submit(data, frames);
    return frames;
}

static void video_cb(const void *data, unsigned width, unsigned height,
                     size_t pitch)
{
    snes_lite_video_refresh(data, width, height, pitch);
}

static void input_poll_cb(void)
{
    snes_lite_input_poll();
}

static int16_t input_state_cb(unsigned port, unsigned device,
                              unsigned index, unsigned id)
{
    return snes_lite_input_state(port, device, index, id);
}

bool snes_lite_core_start(void)
{
    struct retro_game_info game;
    struct retro_system_av_info av_info;

    rb->memset(&game, 0, sizeof(game));
    game.path = NULL;
    game.data = snes_lite.rom_data;
    game.size = snes_lite.rom.size;

    retro_set_environment(environment_cb);
    retro_set_video_refresh(video_cb);
    retro_set_audio_sample_batch(audio_cb);
    retro_set_input_poll(input_poll_cb);
    retro_set_input_state(input_state_cb);
    retro_init();
    if (snes_lite.core_failed)
        return false;
    if (!retro_load_game(&game) || snes_lite.core_failed)
        return false;
    rb->memset(&av_info, 0, sizeof(av_info));
    retro_get_system_av_info(&av_info);
    snes_lite.frame_rate_milli =
        (unsigned)(av_info.timing.fps * 1000.0 + 0.5);
    snes_lite_log("timing fps_milli=%u audio_rate=%u",
                  snes_lite.frame_rate_milli, snes_lite.audio_rate);
    return true;
}

void snes_lite_core_run(void)
{
    long start_tick = *rb->current_tick;

    if (audio_status_callback)
    {
        unsigned occupancy = 0;
        bool underrun_likely = false;

        snes_lite_audio_buffer_status(&occupancy, &underrun_likely);
        audio_status_callback(snes_lite.audio_available, occupancy,
                              underrun_likely);
    }
    retro_run();
    snes_lite.core_ticks += *rb->current_tick - start_tick;
}

bool snes_lite_audio_interpolation(void)
{
    return snes_lite.config.audio != SNES_AUDIO_LOW;
}

void snes_lite_core_reset(void)
{
    retro_reset();
}

void snes_lite_core_stop(void)
{
    retro_unload_game();
    retro_deinit();
}

void *snes_lite_core_sram(size_t *size)
{
    *size = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    return retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
}

bool snes_lite_core_audio_enabled(void)
{
    return snes_lite.audio_available;
}

uint32_t snes_lite_core_achievement_peek(uint32_t address,
                                         uint32_t num_bytes,
                                         void *userdata)
{
    const uint8_t *memory = NULL;
    size_t memory_size = 0;
    uint32_t value = 0;
    uint32_t index;

    (void)userdata;
    if (address < 0x20000)
    {
        memory = retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
        memory_size = retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
    }
    else if (address < 0xA0000)
    {
        address -= 0x20000;
        memory = retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
        memory_size = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    }
    if (!memory || address >= memory_size)
        return 0;
    for (index = 0; index < num_bytes && index < 4; ++index)
    {
        if (address + index >= memory_size)
            break;
        value |= (uint32_t)memory[address + index] << (index * 8);
    }
    return value;
}
