/***************************************************************************
 * Experimental SNES Lite frontend for Rockbox_Personal.
 ***************************************************************************/

#ifndef SNES_LITE_H
#define SNES_LITE_H

#include "plugin.h"

#define SNES_LITE_NAME "SNES Lite"
#define SNES_LITE_ROM_DIR ROCKBOX_DIR "/roms/snes"
#define SNES_LITE_SAVE_DIR ROCKBOX_DIR "/saves/snes"
#define SNES_LITE_CONFIG_DIR ROCKBOX_DIR "/config/snes_lite"
#define SNES_LITE_CONFIG_PATH ROCKBOX_DIR "/config/snes_lite.cfg"
#define SNES_LITE_LOG_PATH ROCKBOX_DIR "/logs/snes_lite.log"
#define SNES_LITE_MAX_ROM_SIZE (6 * 1024 * 1024 + 512)

enum snes_lite_audio_mode {
    SNES_AUDIO_OFF = 0,
    SNES_AUDIO_AUTO,
    SNES_AUDIO_ON,
    SNES_AUDIO_LOW,
};

enum snes_lite_input_profile {
    SNES_INPUT_PLATFORMER = 0,
    SNES_INPUT_RPG,
    SNES_INPUT_ACTION,
    SNES_INPUT_FIGHTING,
    SNES_INPUT_RACING,
    SNES_INPUT_SPORTS,
};

enum snes_lite_video_mode {
    SNES_VIDEO_FULLSCREEN = 0,
    SNES_VIDEO_NATIVE,
};

enum snes_lite_performance_preset {
    SNES_PERF_BALANCED = 0,
    SNES_PERF_FAST,
    SNES_PERF_MAX,
    SNES_PERF_QUALITY,
};

struct snes_lite_config {
    int frameskip;
    enum snes_lite_audio_mode audio;
    enum snes_lite_input_profile input_profile;
    bool show_fps;
    bool performance_mode;
    enum snes_lite_video_mode video_mode;
    enum snes_lite_performance_preset performance_preset;
};

struct snes_lite_rom_info {
    char title[22];
    char basename[MAX_PATH];
    char save_path[MAX_PATH];
    size_t size;
    unsigned char map_mode;
    unsigned char cartridge_type;
    bool hirom;
    const char *unsupported_chip;
};

struct snes_lite_runtime {
    struct snes_lite_config config;
    struct snes_lite_rom_info rom;
    unsigned char *rom_data;
    fb_data *video;
    size_t arena_size;
    size_t arena_used;
    unsigned long frame_count;
    unsigned long rendered_frames;
    unsigned long skipped_frames;
    unsigned long profile_frames;
    unsigned long profile_rendered;
    unsigned long core_ticks;
    unsigned long video_scale_ticks;
    unsigned long video_lcd_ticks;
    unsigned long profile_start_tick;
    unsigned long total_start_tick;
    long frame_deadline;
    int frame_tick_fraction;
    unsigned frame_rate_milli;
    long fps_tick;
    int displayed_fps;
    int displayed_render_fps;
    int effective_frameskip;
    int stable_fast_windows;
    int fixed_skip_counter;
    bool menu_requested;
    bool quit_requested;
    bool reset_requested;
    bool save_requested;
    bool core_failed;
    bool audio_available;
    bool audio_started;
    bool cpu_boosted;
    bool variables_changed;
    unsigned audio_rate;
    const void *last_video_data;
    unsigned last_video_width;
    unsigned last_video_height;
    size_t last_video_pitch;
};

extern struct snes_lite_runtime snes_lite;

void snes_lite_arena_init(void *buffer, size_t size);
void *snes_lite_malloc(size_t size);
void *snes_lite_try_malloc(size_t size);
void *snes_lite_calloc(size_t count, size_t size);
void snes_lite_free(void *ptr);
void snes_lite_core_abort(int status);
int snes_lite_sprintf(char *buffer, const char *format, ...)
    ATTRIBUTE_PRINTF(2, 3);
long snes_lite_strtol(const char *text, char **end, int base);
char *snes_lite_strncpy(char *destination, const char *source, size_t count);
int snes_lite_sscanf(const char *text, const char *format, ...);
void *snes_lite_bsearch(const void *key, const void *base, size_t count,
                        size_t size, int (*compare)(const void *,
                                                   const void *));
long snes_lite_time(long *value);
float sinf(float value);
float cosf(float value);
float tanf(float value);
float sqrtf(float value);

void snes_lite_config_defaults(struct snes_lite_config *config);
void snes_lite_config_load(struct snes_lite_config *config);
void snes_lite_config_load_game(struct snes_lite_config *config,
                                const char *rom_path);
void snes_lite_config_save(const struct snes_lite_config *config);
void snes_lite_config_save_game(const struct snes_lite_config *config);
const char *snes_lite_input_profile_name(enum snes_lite_input_profile profile);

bool snes_lite_rom_load(const char *path);
bool snes_lite_sram_load(void);
bool snes_lite_sram_save(void);

void snes_lite_video_refresh(const void *data, unsigned width,
                             unsigned height, size_t pitch);
void snes_lite_video_redraw(void);
bool snes_lite_video_selftest(void);
void snes_lite_input_poll(void);
int16_t snes_lite_input_state(unsigned port, unsigned device,
                              unsigned index, unsigned id);
void snes_lite_menu(void);

bool snes_lite_core_start(void);
void snes_lite_core_run(void);
void snes_lite_core_reset(void);
void snes_lite_core_stop(void);
void *snes_lite_core_sram(size_t *size);
bool snes_lite_core_audio_enabled(void);
uint32_t snes_lite_core_achievement_peek(uint32_t address,
                                         uint32_t num_bytes,
                                         void *userdata);

bool snes_lite_audio_init(void);
void snes_lite_audio_submit(const int16_t *data, size_t frames);
void snes_lite_audio_pause(bool pause);
void snes_lite_audio_close(void);
unsigned snes_lite_audio_rate(void);
void snes_lite_audio_buffer_status(unsigned *occupancy,
                                   bool *underrun_likely);

void snes_lite_log(const char *format, ...) ATTRIBUTE_PRINTF(1, 2);

#endif
