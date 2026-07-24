#ifndef ROCKBOX_PICODRIVE_H
#define ROCKBOX_PICODRIVE_H

#include "plugin.h"

#ifdef CPU
#undef CPU
#endif
#include <stdint.h>

#define PD_BASE_DIR ROCKBOX_DIR "/games/genesis"
#define PD_ROM_DIR PD_BASE_DIR "/roms"
#define PD_SAVE_DIR PD_BASE_DIR "/saves"
#define PD_STATE_DIR PD_BASE_DIR "/states"
#define PD_CONFIG_DIR PD_BASE_DIR "/config"
#define PD_LOG_PATH PD_BASE_DIR "/picodrive.log"

#define PD_MAX_ROM_SIZE (10u * 1024u * 1024u)
#define PD_AUDIO_RATE_FULL 32000
#define PD_AUDIO_RATE_LOW 22050

struct pd_settings
{
    int frameskip;
    int video_mode;
    int audio_rate;
    int six_button;
    int show_fps;
};

struct pd_runtime
{
    unsigned char *arena_base;
    unsigned char *arena_ptr;
    unsigned char *arena_end;
    size_t arena_size;
    size_t arena_used;
    unsigned char *rom;
    size_t rom_size;
    uint32_t rom_crc;
    char rom_path[MAX_PATH];
    char save_path[MAX_PATH];
    char state_path[MAX_PATH];
    char config_path[MAX_PATH];
    struct pd_settings settings;
    bool failed;
    bool audio_started;
    bool usb_connected;
    bool cpu_boosted;
    unsigned displayed_fps;
};

extern struct pd_runtime pd;

void pd_arena_init(void *base, size_t size);
void *pd_malloc(size_t size);
void *pd_calloc(size_t count, size_t size);
void *pd_realloc(void *ptr, size_t size);
void pd_free(void *ptr);
void pd_log(const char *format, ...);

#endif
