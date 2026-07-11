#ifndef TAMAGOTCHI_H
#define TAMAGOTCHI_H

#include "plugin.h"

#define TAMAGOTCHI_NAME "Tamagotchi"
#define TAMAGOTCHI_SOURCE "Tamagotchi"

#define TAMAGOTCHI_BASE_DIR ROCKBOX_DIR "/apps/tamagotchi"
#define TAMAGOTCHI_ROM_DIR TAMAGOTCHI_BASE_DIR "/roms"
#define TAMAGOTCHI_SAVE_DIR TAMAGOTCHI_BASE_DIR "/saves"
#define TAMAGOTCHI_ROM_PATH TAMAGOTCHI_ROM_DIR "/tama.b"
#define TAMAGOTCHI_GAME_BASE_DIR ROCKBOX_DIR "/games/tamagotchi"
#define TAMAGOTCHI_GAME_ROM_DIR TAMAGOTCHI_GAME_BASE_DIR "/roms"
#define TAMAGOTCHI_GAME_ROM_PATH TAMAGOTCHI_GAME_ROM_DIR "/tama.b"
#define TAMAGOTCHI_STATE_PATH TAMAGOTCHI_SAVE_DIR "/tama_p1.state"
#define TAMAGOTCHI_SESSION_PATH TAMAGOTCHI_SAVE_DIR "/session.dat"
#define TAMAGOTCHI_CONFIG_PATH TAMAGOTCHI_BASE_DIR "/config.cfg"

#define TAMA_LCD_W 32
#define TAMA_LCD_H 16
#define TAMA_ICON_COUNT 8
#define TAMA_ROM_BYTES 12288
#define TAMA_ROM_WORDS (TAMA_ROM_BYTES / 2)

enum tamagotchi_control_mode
{
    TAMA_CONTROL_STOCK_IPOD = 0,
    TAMA_CONTROL_RAW_ABC,
};

enum tamagotchi_display_mode
{
    TAMA_DISPLAY_SHELL = 0,
    TAMA_DISPLAY_FULL_LCD,
    TAMA_DISPLAY_MINIMAL,
};

enum tamagotchi_clock_mode
{
    TAMA_CLOCK_PAUSED = 0,
    TAMA_CLOCK_REAL_TIME,
    TAMA_CLOCK_FAST_FORWARD,
};

struct tamagotchi_settings
{
    enum tamagotchi_control_mode control_mode;
    enum tamagotchi_display_mode display_mode;
    enum tamagotchi_clock_mode clock_mode;
    int wheel_sensitivity;
    bool wheel_haptic_ticks;
    bool attention_haptics;
    bool haptics_enabled;
    bool beep_enabled;
    bool quiet_mode;
    bool notifications_enabled;
    bool auto_clock_done;
};

#endif
