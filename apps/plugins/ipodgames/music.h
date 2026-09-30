#ifndef IPODGAMES_MUSIC_H
#define IPODGAMES_MUSIC_H

#include "plugin.h"

#define IG_MUSIC_RATE 44100u

void ig_music_init(const char *game_directory);
unsigned int ig_music_register(const char *name);
void ig_music_play(unsigned int stream);
void ig_music_pause(bool pause);
void ig_music_stop(void);
void ig_music_repeat(unsigned int mode);
void ig_music_service(void);
bool ig_music_active(void);
void ig_music_mix(int16_t *output, size_t frames);
void ig_music_shutdown(void);
void ig_music_report(int fd);

#endif
