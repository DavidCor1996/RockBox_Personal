#ifndef SM64_ROCKBOX_H
#define SM64_ROCKBOX_H

#include "plugin.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define SM64_SAVE_DIR ROCKBOX_DIR "/games/n64/saves"
#define SM64_SAVE_PATH SM64_SAVE_DIR "/Super Mario 64.sav"
#define SM64_LOG_PATH PLUGIN_GAMES_DATA_DIR "/sm64/sm64.log"

struct sm64_rockbox_state {
    bool quit;
    bool usb_connected;
    bool fatal;
    bool audio_ready;
    bool audio_started;
    bool render_frame;
    unsigned long frames;
    unsigned long rendered_frames;
    unsigned long skipped_frames;
    unsigned long overruns;
    long frame_started;
    long next_frame_scaled;
    int test_frames;
};

extern struct sm64_rockbox_state sm64_rb;

void sm64_logf(const char *format, ...);
int sm64_sprintf(char *buffer, const char *format, ...);
int sm64_snprintf(char *buffer, size_t size, const char *format, ...);
int sm64_puts(const char *text);
void sm64_abort(void);
void sm64_exit(int status);

int sm64_rockbox_save_read(void *buffer, size_t size);
int sm64_rockbox_save_write(const void *buffer, size_t size);

bool sm64_audio_init(void);
int sm64_audio_buffered(void);
int sm64_audio_desired(void);
void sm64_audio_play(const uint8_t *buffer, size_t length);
void sm64_audio_shutdown(void);

void sm64_input_init(void);
void sm64_input_read(void *pad);

void sm64_video_set_render_allowed(bool allowed);
void sm64_video_dump_test_frame(void);

#endif
