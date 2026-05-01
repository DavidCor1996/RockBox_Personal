#ifndef ROCKBOY_PROFILER_H
#define ROCKBOY_PROFILER_H

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

struct rockboy_profile_totals {
    unsigned long total[ROCKBOY_TIME_COUNT];
    unsigned long peak[ROCKBOY_TIME_COUNT];
    unsigned long samples[ROCKBOY_TIME_COUNT];
    unsigned long rendered_frames;
    unsigned long skipped_frames;
    unsigned long pcm_underruns;
};

void rockboy_profile_reset(void);
void rockboy_profile_add(enum rockboy_profile_counter which, unsigned long ticks);
void rockboy_profile_frame_rendered(void);
void rockboy_profile_frame_skipped(void);
void rockboy_profile_pcm_underrun(void);
const struct rockboy_profile_totals *rockboy_profile_get_totals(void);
bool rockboy_profile_is_enabled(void);
void rockboy_profile_log_summary(const char *rom_path);

#endif
