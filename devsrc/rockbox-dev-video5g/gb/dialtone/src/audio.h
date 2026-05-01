#ifndef DIALTONE_AUDIO_H
#define DIALTONE_AUDIO_H

#include <gb/gb.h>
#include <stdint.h>

#define RECORD_NONE 0u
#define RECORD_RAIN_LOOP 1u
#define RECORD_VENDING_DREAMS 2u
#define RECORD_ROOFTOP_SET 3u

#define SFX_MENU_MOVE 0u
#define SFX_CONFIRM 1u
#define SFX_CANCEL 2u
#define SFX_DOOR 3u
#define SFX_REWARD 4u
#define SFX_FART 5u

void dt_audio_init(void) BANKED;
void dt_audio_update(void) BANKED;
void dt_audio_play_sfx(uint8_t sfx_id) BANKED;
void dt_audio_play_title_theme(void) BANKED;
void dt_audio_stop_music(void) BANKED;
void dt_audio_set_home_record(uint8_t record_id) BANKED;
uint8_t dt_audio_get_home_record(void) BANKED;
void dt_audio_sync(uint8_t room_id) BANKED;
uint8_t dt_record_from_media(uint8_t media_flag) BANKED;
const char *dt_record_name(uint8_t record_id) BANKED;
void dt_home_record_menu(uint8_t media, uint8_t room_id) BANKED;

#endif
