#ifndef SMSGG_PLATFORM_H
#define SMSGG_PLATFORM_H

#include "plugin.h"

#define SMSGG_BASE_DIR      ROCKBOX_DIR "/games/smsgg"
#define SMSGG_ROM_DIR       SMSGG_BASE_DIR "/roms"
#define SMSGG_SAVE_DIR      SMSGG_BASE_DIR "/saves"
#define SMSGG_STATE_DIR     SMSGG_BASE_DIR "/states"
#define SMSGG_CONFIG_PATH   SMSGG_BASE_DIR "/config.cfg"

extern void *smsgg_alloc_base;
extern unsigned char *smsgg_alloc_ptr;
extern size_t smsgg_alloc_free;

void smsgg_platform_init_alloc(void *base, size_t size);
void smsgg_platform_reset_temp(void);
bool smsgg_ensure_dirs(void);
const char *smsgg_basename(const char *path);
void smsgg_make_safe_name(char *dst, size_t dst_size, const char *src);

#endif
