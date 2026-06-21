#ifndef ROCKBOY_PROFILER_H
#define ROCKBOY_PROFILER_H

#include "settings.h"

extern struct options options;

enum rockboy_profile_mode {
    ROCKBOY_PROFILE_OFF = 0,
    ROCKBOY_PROFILE_OVERLAY = 1,
    ROCKBOY_PROFILE_OVERLAY_AND_LOG = 2,
};

enum rockboy_profile_counter {
    ROCKBOY_TIME_FRAME = 0,
    ROCKBOY_TIME_CPU,
    ROCKBOY_TIME_RENDER,
    ROCKBOY_TIME_SCALE,
    ROCKBOY_TIME_BLIT,
    ROCKBOY_TIME_AUDIO_MIX,
    ROCKBOY_TIME_PCM_WAIT,
    ROCKBOY_TIME_SAVE,
    ROCKBOY_TIME_COUNT,
};

enum rockboy_profile_event {
    ROCKBOY_EVENT_CPU_OPS = 0,
    ROCKBOY_EVENT_SLOW_MEM_READS,
    ROCKBOY_EVENT_SLOW_MEM_WRITES,
    ROCKBOY_EVENT_VRAM_DIRTY_WRITES,
    ROCKBOY_EVENT_LCD_LINES,
    ROCKBOY_EVENT_LCD_DMG_LINES,
    ROCKBOY_EVENT_LCD_CGB_LINES,
    ROCKBOY_EVENT_LCD_NO_SPRITE_LINES,
    ROCKBOY_EVENT_LCD_NO_WINDOW_LINES,
    ROCKBOY_EVENT_LCD_DMG_BG_ONLY_ELIGIBLE,
    ROCKBOY_EVENT_LCD_DMG_BG_WINDOW_NO_SPR_ELIGIBLE,
    ROCKBOY_EVENT_LCD_CGB_NO_SPRITE_LINES,
    ROCKBOY_EVENT_LCD_DMG_BG_ONLY_USED,
    ROCKBOY_EVENT_LCD_DMG_BG_ONLY_REJECTED,
    ROCKBOY_EVENT_LCD_CGB_BG_ONLY_ELIGIBLE,
    ROCKBOY_EVENT_LCD_CGB_BG_ONLY_USED,
    ROCKBOY_EVENT_LCD_CGB_BG_ONLY_REJECTED,
    ROCKBOY_EVENT_COUNT,
};

struct rockboy_profile_totals {
    unsigned long total[ROCKBOY_TIME_COUNT];
    unsigned long peak[ROCKBOY_TIME_COUNT];
    unsigned long samples[ROCKBOY_TIME_COUNT];
    unsigned long events[ROCKBOY_EVENT_COUNT];
    unsigned long rendered_frames;
    unsigned long skipped_frames;
    unsigned long pcm_underruns;
};

void rockboy_profile_reset(void);
void rockboy_profile_start(const char *rom_path);
void rockboy_profile_add_enabled(enum rockboy_profile_counter which,
                                 unsigned long ticks);
void rockboy_profile_frame_rendered_enabled(void);
void rockboy_profile_frame_skipped_enabled(void);
void rockboy_profile_pcm_underrun_enabled(void);
void rockboy_profile_count_enabled(enum rockboy_profile_event which,
                                   unsigned long count);
const struct rockboy_profile_totals *rockboy_profile_get_totals(void);
void rockboy_profile_log_summary(const char *rom_path);

static inline bool rockboy_profile_is_enabled(void)
{
    return options.profile != ROCKBOY_PROFILE_OFF;
}

#define rockboy_profile_add(which, ticks) \
    do { \
        if (rockboy_profile_is_enabled()) \
            rockboy_profile_add_enabled((which), (ticks)); \
    } while (0)

#define rockboy_profile_frame_rendered() \
    do { \
        if (rockboy_profile_is_enabled()) \
            rockboy_profile_frame_rendered_enabled(); \
    } while (0)

#define rockboy_profile_frame_skipped() \
    do { \
        if (rockboy_profile_is_enabled()) \
            rockboy_profile_frame_skipped_enabled(); \
    } while (0)

#define rockboy_profile_pcm_underrun() \
    do { \
        if (rockboy_profile_is_enabled()) \
            rockboy_profile_pcm_underrun_enabled(); \
    } while (0)

#define rockboy_profile_count(which, count) \
    do { \
        if (rockboy_profile_is_enabled()) \
            rockboy_profile_count_enabled((which), (count)); \
    } while (0)

#endif
