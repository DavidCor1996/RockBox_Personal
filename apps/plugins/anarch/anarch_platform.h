#ifndef ANARCH_PLATFORM_H
#define ANARCH_PLATFORM_H

#include "plugin.h"

#define ANARCH_WIDTH 320
#define ANARCH_HEIGHT 240
#define ANARCH_SAVE_SIZE 12

enum anarch_profile {
    ANARCH_PROFILE_BALANCED = 0,
    ANARCH_PROFILE_COUNT
};

struct anarch_settings {
    int profile;
    bool music;
    bool sound;
    bool haptics;
};

enum anarch_menu_action {
    ANARCH_MENU_RESUME = 0,
    ANARCH_MENU_MAP,
    ANARCH_MENU_SAVE,
    ANARCH_MENU_RESTART,
    ANARCH_MENU_EXIT,
    ANARCH_MENU_USB
};

enum anarch_key {
    ANARCH_KEY_UP = 0,
    ANARCH_KEY_RIGHT,
    ANARCH_KEY_DOWN,
    ANARCH_KEY_LEFT,
    ANARCH_KEY_A,
    ANARCH_KEY_B,
    ANARCH_KEY_C,
    ANARCH_KEY_JUMP,
    ANARCH_KEY_STRAFE_LEFT,
    ANARCH_KEY_STRAFE_RIGHT,
    ANARCH_KEY_MAP,
    ANARCH_KEY_TOGGLE_FREELOOK,
    ANARCH_KEY_NEXT_WEAPON,
    ANARCH_KEY_PREVIOUS_WEAPON,
    ANARCH_KEY_MENU,
    ANARCH_KEY_CYCLE_WEAPON,
    ANARCH_KEY_COUNT
};

bool anarch_video_init(void);
void anarch_video_clear(void);
void anarch_video_pixel(uint16_t x, uint16_t y, uint16_t color);
void anarch_video_present(void);
bool anarch_video_guards_ok(void);
uint32_t anarch_video_hash(void);
size_t anarch_video_arena_used(void);
size_t anarch_video_arena_free(void);
#ifdef SIMULATOR
bool anarch_video_dump(void);
#endif

void anarch_input_init(void);
void anarch_input_poll(void);
void anarch_input_shutdown(void);
void anarch_input_set_menu(bool menu);
int8_t anarch_input_key(uint8_t key);
bool anarch_input_usb(void);
bool anarch_input_exit(void);
void anarch_input_clear_exit(void);
bool anarch_input_pause_requested(void);
void anarch_input_clear_pause(void);

void anarch_save_init(void);
void anarch_save_write(const uint8_t data[ANARCH_SAVE_SIZE]);
uint8_t anarch_save_read(uint8_t data[ANARCH_SAVE_SIZE]);
bool anarch_save_warning(void);
#ifdef SIMULATOR
bool anarch_save_selftest(void);
#endif

void anarch_audio_init(bool enabled);
void anarch_audio_pump(void);
void anarch_audio_pause(bool paused);
void anarch_audio_shutdown(void);
void anarch_audio_sound(uint8_t sound, uint8_t volume);
void anarch_audio_music(uint8_t command);
unsigned long anarch_audio_underruns(void);

void anarch_profile_reset(void);
void anarch_profile_render(unsigned int percent);
void anarch_profile_lcd(unsigned int ticks);
void anarch_profile_input_queue(unsigned int depth);
uint32_t anarch_profile_render_samples(void);
uint32_t anarch_profile_late_frames(void);
unsigned int anarch_profile_input_queue_worst(void);
void anarch_profile_write(uint32_t frames, unsigned long underruns,
                          bool guards_ok);

void anarch_haptic_event(uint8_t event, uint8_t data);
void anarch_haptics_set_enabled(bool enabled);

void anarch_settings_default(struct anarch_settings *settings);
void anarch_settings_load(struct anarch_settings *settings);
void anarch_settings_save(const struct anarch_settings *settings);
enum anarch_menu_action anarch_menu_run(struct anarch_settings *settings);
#ifdef SIMULATOR
bool anarch_config_selftest(void);
bool anarch_menu_selftest(void);
bool anarch_input_selftest(void);
#endif

#endif
