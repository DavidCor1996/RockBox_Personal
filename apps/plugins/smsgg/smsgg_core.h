#ifndef SMSGG_CORE_H
#define SMSGG_CORE_H

#include "plugin.h"
#include "upstream/shared.h"

#define SMSGG_MAX_ROM_SIZE      0x200000
#define SMSGG_SRAM_SIZE         0x8000
#define SMSGG_STATE_SIZE        (60 * 1024)
#define SMSGG_FB_WIDTH          256
#define SMSGG_FB_HEIGHT         240

struct smsgg_core
{
    uint8 *rom;
    uint8 *sram;
    uint8 *framebuffer;
    uint8 *state_buffer;
    uint32_t crc;
    size_t rom_size;
    bool loaded;
    bool is_gg;
};

bool smsgg_core_load(struct smsgg_core *core, const char *path,
                     bool audio_enabled);
void smsgg_core_unload(struct smsgg_core *core);
void smsgg_core_reset(struct smsgg_core *core);
void smsgg_core_run_frame(bool skip);
void smsgg_core_set_buttons(uint8 pad, uint8 system);
void smsgg_core_set_audio(bool enabled);
bool smsgg_core_save_state(struct smsgg_core *core, const char *path);
bool smsgg_core_load_state(struct smsgg_core *core, const char *path);
bool smsgg_core_save_sram(struct smsgg_core *core, const char *path);
bool smsgg_core_load_sram(struct smsgg_core *core, const char *path);

#endif
