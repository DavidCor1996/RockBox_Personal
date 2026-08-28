/***************************************************************************
 * Rockbox platform layer for the Zelda3 source port.
 ****************************************************************************/
#ifndef ZELDA3_ROCKBOX_H
#define ZELDA3_ROCKBOX_H

#include "plugin.h"

#define ZELDA3_DATA_DIR ROCKBOX_DIR "/zelda3"
#define ZELDA3_ASSET_PATH ZELDA3_DATA_DIR "/zelda3_assets.dat"
#define ZELDA3_SAVE_DIR ZELDA3_DATA_DIR "/saves"
#define ZELDA3_SRAM_PATH ZELDA3_SAVE_DIR "/sram.dat"
#define ZELDA3_SRAM_BACKUP_PATH ZELDA3_SAVE_DIR "/sram.bak"
#define ZELDA3_LOG_PATH ZELDA3_DATA_DIR "/zelda3.log"

struct zelda3_runtime {
    bool quit;
    bool fatal;
    bool audio_ready;
    bool audio_started;
    bool cpu_boosted;
    bool usb_connected;
    bool hold_initialized;
    bool hold_armed;
    unsigned long frames;
    unsigned long rendered;
    unsigned long overruns;
    unsigned long test_frames;
    long profile_start_tick;
    long next_frame_scaled;
    int log_fd;
};

extern struct zelda3_runtime zelda3_rb;

void zelda3_log(const char *format, ...) ATTRIBUTE_PRINTF(1, 2);
void zelda3_die(const char *message);
void zelda3_abort(void);
void zelda3_exit(int status);
int zelda3_printf(const char *format, ...) ATTRIBUTE_PRINTF(1, 2);
int zelda3_puts(const char *text);
int zelda3_fprintf(void *stream, const char *format, ...)
    ATTRIBUTE_PRINTF(2, 3);
int zelda3_sprintf(char *buffer, const char *format, ...)
    ATTRIBUTE_PRINTF(2, 3);
int zelda3_snprintf(char *buffer, size_t size, const char *format, ...)
    ATTRIBUTE_PRINTF(3, 4);
int zelda3_vsnprintf(char *buffer, size_t size, const char *format,
                     va_list arguments);
char *zelda3_strdup(const char *text);

void *zelda3_fopen(const char *path, const char *mode);
size_t zelda3_fread(void *ptr, size_t size, size_t count, void *stream);
size_t zelda3_fwrite(const void *ptr, size_t size, size_t count, void *stream);
int zelda3_fseek(void *stream, long offset, int whence);
long zelda3_ftell(void *stream);
int zelda3_fclose(void *stream);
int zelda3_rename(const char *old_path, const char *new_path);

bool zelda3_assets_load(void);
void zelda3_video_draw(void);
unsigned zelda3_input_poll(void);
bool zelda3_input_self_test(unsigned *coverage);
bool zelda3_audio_init(void);
void zelda3_audio_frame(void);
void zelda3_audio_pause(bool pause);
void zelda3_audio_close(void);

#endif
