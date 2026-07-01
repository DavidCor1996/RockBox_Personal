/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ |__   _______  ___
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 Rockpod contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/

#include "sky_runtime.h"
#include "sky_cpt_loader.h"
#include "sky_loader.h"
#include "sky_sound_map.h"
#include "sky_text.h"
#include "rbfile.h"

#define SKY_NUM_SCRIPT_VARS 838
#define SKY_MAX_LOGIC_SCAN 2048
#define SKY_MAX_SCRIPT_MODULES 16
#define SKY_MAX_CACHE_ITEMS 300
#define SKY_MAX_CACHE_LIST 60
#define SKY_CPT_MAINLIST 7
#define SKY_CPT_MOVE_LIST 0x00bd
#define SKY_FILE_MODULE_0 60400

#define SKY_VAR_RESULT 0
#define SKY_VAR_SCREEN 1
#define SKY_VAR_LOGIC_LIST_NO 2
#define SKY_VAR_MOUSE_LIST_NO 6
#define SKY_VAR_DRAW_LIST_NO 8
#define SKY_VAR_CUR_ID 12
#define SKY_VAR_MOUSE_STATUS 13
#define SKY_VAR_MOUSE_STOP 14
#define SKY_VAR_SPECIAL_ITEM 17
#define SKY_VAR_GET_OFF 18
#define SKY_VAR_PLAYER_X 27
#define SKY_VAR_PLAYER_Y 28
#define SKY_VAR_PLAYER_MOOD 29
#define SKY_VAR_PLAYER_SCREEN 30
#define SKY_VAR_HIT_ID 37
#define SKY_VAR_LAYER_1_ID 42
#define SKY_VAR_LAYER_3_ID 44
#define SKY_VAR_CURSOR_ID 22
#define SKY_VAR_TEXT1 53
#define SKY_VAR_THE_CHOSEN_ONE 51
#define SKY_VAR_MENU_LENGTH 100
#define SKY_VAR_SCROLL_OFFSET 101
#define SKY_VAR_MENU 102
#define SKY_VAR_OBJECT_HELD 103
#define SKY_VAR_RND 115
#define SKY_VAR_CUR_SECTION 143
#define SKY_VAR_LAMB_GREET 109
#define SKY_VAR_JOEY_SECTION 145
#define SKY_VAR_LAMB_SECTION 146
#define SKY_VAR_S15_FLOOR 450
#define SKY_VAR_GUARDIAN_THERE 640
#define SKY_VAR_DOOR_67_68_FLAG 678
#define SKY_VAR_SC70_IRIS_FLAG 693
#define SKY_VAR_DOOR_73_75_FLAG 704
#define SKY_VAR_SC76_CABINET1_FLAG 709
#define SKY_VAR_SC76_CABINET2_FLAG 710
#define SKY_VAR_SC76_CABINET3_FLAG 711
#define SKY_VAR_DOOR_77_78_FLAG 719
#define SKY_VAR_SC80_EXIT_FLAG 720
#define SKY_VAR_SC31_LIFT_FLAG 793
#define SKY_VAR_SC32_LIFT_FLAG 797
#define SKY_VAR_SC33_SHED_DOOR_FLAG 798
#define SKY_VAR_BAND_PLAYING 804
#define SKY_VAR_COLSTON_AT_TABLE 805
#define SKY_VAR_SC36_NEXT_DEALER 806
#define SKY_VAR_SC36_DOOR_FLAG 807
#define SKY_VAR_SC37_DOOR_FLAG 808
#define SKY_VAR_SC40_LOCKER_1_FLAG 817
#define SKY_VAR_SC40_LOCKER_2_FLAG 818
#define SKY_VAR_SC40_LOCKER_3_FLAG 819
#define SKY_VAR_SC40_LOCKER_4_FLAG 820
#define SKY_VAR_SC40_LOCKER_5_FLAG 821

#define SKY_COMPACT_LOGIC 0
#define SKY_COMPACT_STATUS 1
#define SKY_COMPACT_MODE 26
#define SKY_COMPACT_BASE_SUB 27
#define SKY_COMPACT_BASE_SUB_OFF 28
#define SKY_COMPACT_ACTION_SUB 29
#define SKY_COMPACT_ACTION_SUB_OFF 30
#define SKY_COMPACT_GET_TO_SUB 31
#define SKY_COMPACT_GET_TO_SUB_OFF 32
#define SKY_COMPACT_EXTRA_SUB 33
#define SKY_COMPACT_EXTRA_SUB_OFF 34
#define SKY_COMPACT_SCREEN 3
#define SKY_COMPACT_PLACE 4
#define SKY_COMPACT_X 6
#define SKY_COMPACT_Y 7
#define SKY_COMPACT_FRAME 8
#define SKY_COMPACT_CURSOR_TEXT 9
#define SKY_COMPACT_MOUSE_REL_X 13
#define SKY_COMPACT_MOUSE_REL_Y 14
#define SKY_COMPACT_MOUSE_SIZE_X 15
#define SKY_COMPACT_MOUSE_SIZE_Y 16
#define SKY_COMPACT_ACTION_SCRIPT 17
#define SKY_COMPACT_UP_FLAG 18
#define SKY_COMPACT_DOWN_FLAG 19
#define SKY_COMPACT_GET_TO_FLAG 20
#define SKY_COMPACT_FLAG 21
#define SKY_COMPACT_SYNC 2
#define SKY_COMPACT_REQUEST 42
#define SKY_COMPACT_ALT 41
#define SKY_COMPACT_LOGIC_FIELD 0
#define SKY_COMPACT_AR_TARGET_X 51
#define SKY_COMPACT_AR_TARGET_Y 52
#define SKY_COMPACT_GRAFIX_PROG_ID 23
#define SKY_COMPACT_GRAFIX_PROG_POS 24
#define SKY_COMPACT_OFFSET 25
#define SKY_COMPACT_MOOD 22
#define SKY_COMPACT_DIR 35
#define SKY_COMPACT_LEAVING 38
#define SKY_COMPACT_TURN_PROG_ID 48
#define SKY_COMPACT_TURN_PROG_POS 49
#define SKY_COMPACT_WAITING_FOR 50
#define SKY_COMPACT_STOP_SCRIPT 36
#define SKY_COMPACT_MINI_BUMP 37
#define SKY_COMPACT_AT_WATCH 39
#define SKY_COMPACT_AT_WAS 40
#define SKY_COMPACT_SP_COLOR 44
#define SKY_COMPACT_SP_TEXT_ID 45
#define SKY_COMPACT_SP_TIME 46
#define SKY_COMPACT_MEGA_SET 54
#define SKY_COMPACT_ANIM_SCRATCH_ID 53
#define SKY_COMPACT_AR_ANIM_INDEX 47
#define SKY_ST_LOGIC 64
#define SKY_ST_COLLISION 32
#define SKY_ST_MOUSE 16
#define SKY_ST_DRAW_MASK 0x0007
#define SKY_ST_BACKGROUND 1
#define SKY_ST_FOREGROUND 2
#define SKY_ST_SORT 4
#define SKY_ST_RECREATE 8
#define SKY_ST_NO_VMASK 0x0200
#define SKY_ST_GRID_PLOT 128
#define SKY_LOGIC_NOP 0
#define SKY_LOGIC_SCRIPT 1
#define SKY_LOGIC_AR 2
#define SKY_LOGIC_AR_ANIM 3
#define SKY_LOGIC_AR_TURNING 4
#define SKY_LOGIC_ALT 5
#define SKY_LOGIC_MOD_ANIMATE 6
#define SKY_LOGIC_TURNING 7
#define SKY_LOGIC_STOPPED 11
#define SKY_LOGIC_CHOOSE 12
#define SKY_LOGIC_FRAMES 13
#define SKY_LOGIC_PAUSE 14
#define SKY_LOGIC_WAIT_SYNC 15
#define SKY_LOGIC_SIMPLE_MOD 16
#define SKY_SCRIPT_MAX_STEPS 256
#define SKY_SCRIPT_STACK_SIZE 16
#define SKY_NEXT_MEGA_SET 144
#define SKY_C_STAND_UP 138
#define SKY_SEND_SYNC 0xffff
#define SKY_LF_START_FX 0xfffe
#define SKY_TOP_LEFT_X 128
#define SKY_TOP_LEFT_Y 136
#define SKY_MENU_BAR_LEFT 47
#define SKY_MENU_BAR_RIGHT 48
#define SKY_SAVE_MAGIC 0x53594b52
#define SKY_SAVE_VERSION 1
#define SKY_GRID_FILE_START 60000
#define SKY_GRID_SIZE 120
#define SKY_ROUTE_GRID_W 42
#define SKY_ROUTE_GRID_H 26
#define SKY_ROUTE_GRID_SIZE (SKY_ROUTE_GRID_W * SKY_ROUTE_GRID_H)
#define SKY_ROUTE_WORDS 32
#define SKY_WALK_JUMP 8
#define SKY_DIR_UP 0
#define SKY_DIR_DOWN 1
#define SKY_DIR_LEFT 2
#define SKY_DIR_RIGHT 3
#define SKY_FIRST_TEXT_COMPACT 23
#define SKY_LAST_TEXT_COMPACT 33
#define SKY_FIRST_TEXT_BUFFER 274
#define SKY_LAST_TEXT_BUFFER 284
#define SKY_FIXED_TEXT_WIDTH 128
#define SKY_SOUND_FILE_BASE 60203
#define SKY_SPEECH_FILE_BASE 50000
#define SKY_AUDIO_RATE 44100
#define SKY_AUDIO_FRAMES 22050
#define SKY_AUDIO_CHANNELS 2
#define SKY_AUDIO_MIX_FRAMES 2048
#define SKY_AUDIO_DELAY_QUEUE 4
#define SKY_MUSIC_BUFFER_SIZE 4096

static uint32_t script_vars[SKY_NUM_SCRIPT_VARS];
static const struct scummvm_target *runtime_target;
static struct scummvm_sky_resource script_modules[SKY_MAX_SCRIPT_MODULES];
static struct scummvm_sky_resource runtime_screen;
static struct scummvm_sky_resource cached_items[SKY_MAX_CACHE_ITEMS];
static struct scummvm_sky_resource runtime_text_sprites[
    SKY_LAST_TEXT_BUFFER - SKY_FIRST_TEXT_BUFFER + 1];
static struct scummvm_sky_resource runtime_sound_section;
static uint16_t cache_build_list[SKY_MAX_CACHE_LIST];
static uint16_t runtime_palette_id;
static int16_t runtime_audio_pcm[SKY_AUDIO_FRAMES * SKY_AUDIO_CHANNELS];
static int16_t runtime_audio_mix[SKY_AUDIO_MIX_FRAMES * SKY_AUDIO_CHANNELS];
static uint8_t runtime_music_buffer[SKY_MUSIC_BUFFER_SIZE];
static size_t runtime_audio_pos;
static size_t runtime_audio_size;
static unsigned int runtime_audio_old_frequency;
static bool runtime_audio_active;
static bool runtime_audio_channel_started;
static uint16_t runtime_current_music;
static int runtime_music_fd = -1;
static uint32_t runtime_music_data_start;
static uint32_t runtime_music_data_size;
static uint32_t runtime_music_data_pos;
static uint16_t runtime_music_rate;
static uint16_t runtime_music_channels;
static uint16_t runtime_music_bits;
static uint32_t runtime_music_buffer_pos;
static uint32_t runtime_music_buffer_size;
static uint32_t runtime_music_step;
static uint32_t runtime_music_phase;
static int16_t runtime_music_last_left;
static int16_t runtime_music_last_right;
static bool runtime_music_have_sample;
static bool runtime_music_loop;
struct runtime_sfx_delay {
    uint8_t frames;
    uint8_t raw_sound;
    uint8_t volume;
};
static struct runtime_sfx_delay runtime_audio_delay[SKY_AUDIO_DELAY_QUEUE];
static const uint16_t runtime_speech_convert[8] = {
    0,
    600,
    1100,
    2430,
    3380,
    4530,
    5080,
    5230,
};
static int runtime_mouse_x;
static int runtime_mouse_y;
static bool runtime_mouse_down;
static bool runtime_mouse_clicked;
static bool runtime_ready;
static struct scummvm_sky_overlay runtime_overlay;

static bool runtime_mcode(uint16_t mcode,
                          uint32_t a,
                          uint32_t b,
                          uint32_t c,
                          uint16_t current_id,
                          uint16_t *compact,
                          uint16_t compact_size,
                          bool *continue_script,
                          char *status,
                          size_t status_size);
static bool runtime_step_simple_anim(uint16_t *compact,
                                     uint16_t compact_size,
                                     char *status,
                                     size_t status_size);
static bool runtime_step_mod_anim(uint16_t *compact,
                                  uint16_t compact_size,
                                  char *status,
                                  size_t status_size);
static void runtime_draw_logic_sprites(struct scummvm_video *video,
                                       const fb_data *palette);
static void runtime_draw_draw_lists(struct scummvm_video *video,
                                    const fb_data *palette);
static const struct scummvm_sky_resource *runtime_get_resource(uint16_t file_nr);
static bool runtime_save_snapshot(void);
static void runtime_overlay_clear(void);
static void runtime_overlay_message(uint32_t text_num,
                                    uint16_t value,
                                    bool selectable,
                                    int y);
static void runtime_overlay_chooser(void);
static void runtime_clear_text_mouse(bool only_mouse);
static bool runtime_alloc_text(uint32_t text_num,
                               uint16_t width,
                               uint8_t color,
                               bool center,
                               uint16_t logic,
                               uint16_t *text_id,
                               uint16_t *text_width);
static bool runtime_audio_start_speech(uint32_t text_num);
static bool runtime_build_route(uint16_t *compact,
                                uint16_t compact_size,
                                uint16_t *route_result);
static bool runtime_run_compact_script(uint16_t id,
                                       uint16_t script_no,
                                       char *status,
                                       size_t status_size);
static bool runtime_step_route(uint16_t *compact,
                               uint16_t compact_size,
                               uint16_t current_id);
static void runtime_grid_object_to_walk(const uint16_t *compact,
                                        uint16_t compact_size,
                                        bool plot);
static void runtime_grid_plot(uint16_t x,
                              uint16_t y,
                              uint16_t width,
                              const uint16_t *compact,
                              uint16_t compact_size,
                              bool plot);

static uint16_t compact_word(const uint16_t *compact, uint16_t size,
                             uint16_t index)
{
    if (!compact || index >= size)
        return 0;

    return compact[index];
}

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int16_t read_sle16(const uint8_t *p)
{
    return (int16_t)read_le16(p);
}

static uint16_t read_be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool runtime_read_all(int fd, void *dst, size_t size)
{
    uint8_t *out = (uint8_t *)dst;
    size_t done = 0;

    while (done < size) {
        ssize_t got = rb->read(fd, out + done, size - done);
        if (got <= 0)
            return false;
        done += (size_t)got;
    }

    return true;
}

static bool runtime_read_at(int fd, uint32_t offset, void *dst, size_t size)
{
    if (rb->lseek(fd, offset, SEEK_SET) != (off_t)offset)
        return false;

    return runtime_read_all(fd, dst, size);
}

static bool runtime_write_all(int fd, const void *src, size_t size)
{
    const uint8_t *in = (const uint8_t *)src;
    size_t done = 0;

    while (done < size) {
        ssize_t wrote = rb->write(fd, in + done, size - done);
        if (wrote <= 0)
            return false;
        done += (size_t)wrote;
    }

    return true;
}

static bool runtime_read_u32(int fd, uint32_t *value)
{
    uint8_t buf[4];

    if (!runtime_read_all(fd, buf, sizeof(buf)))
        return false;

    *value = read_le32(buf);
    return true;
}

static bool runtime_write_u32(int fd, uint32_t value)
{
    uint8_t buf[4];

    buf[0] = (uint8_t)value;
    buf[1] = (uint8_t)(value >> 8);
    buf[2] = (uint8_t)(value >> 16);
    buf[3] = (uint8_t)(value >> 24);
    return runtime_write_all(fd, buf, sizeof(buf));
}

void scummvm_sky_runtime_reset(void)
{
    uint32_t i;

    runtime_save_snapshot();

    if (runtime_audio_active) {
        rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
        rb->pcmbuf_fade(false, false);
        if (runtime_audio_old_frequency)
            rb->mixer_set_frequency(runtime_audio_old_frequency);
    }
    if (runtime_music_fd >= 0)
        rb->close(runtime_music_fd);
    runtime_audio_active = false;
    runtime_audio_channel_started = false;
    runtime_audio_pos = 0;
    runtime_audio_size = 0;
    runtime_audio_old_frequency = 0;
    runtime_current_music = 0;
    runtime_music_fd = -1;
    runtime_music_data_start = 0;
    runtime_music_data_size = 0;
    runtime_music_data_pos = 0;
    runtime_music_rate = 0;
    runtime_music_channels = 0;
    runtime_music_bits = 0;
    runtime_music_buffer_pos = 0;
    runtime_music_buffer_size = 0;
    runtime_music_step = 0;
    runtime_music_phase = 0;
    runtime_music_last_left = 0;
    runtime_music_last_right = 0;
    runtime_music_have_sample = false;
    runtime_music_loop = false;
    rb->memset(runtime_audio_delay, 0, sizeof(runtime_audio_delay));

    for (i = 0; i < ARRAYLEN(script_modules); i++)
        scummvm_sky_loader_release_resource(&script_modules[i]);
    scummvm_sky_loader_release_resource(&runtime_screen);
    for (i = 0; i < ARRAYLEN(cached_items); i++)
        scummvm_sky_loader_release_resource(&cached_items[i]);
    for (i = 0; i < ARRAYLEN(runtime_text_sprites); i++)
        scummvm_sky_loader_release_resource(&runtime_text_sprites[i]);
    scummvm_sky_loader_release_resource(&runtime_sound_section);

    rb->memset(script_vars, 0, sizeof(script_vars));
    rb->memset(script_modules, 0, sizeof(script_modules));
    rb->memset(&runtime_screen, 0, sizeof(runtime_screen));
    rb->memset(cached_items, 0, sizeof(cached_items));
    rb->memset(runtime_text_sprites, 0, sizeof(runtime_text_sprites));
    rb->memset(&runtime_sound_section, 0, sizeof(runtime_sound_section));
    rb->memset(cache_build_list, 0, sizeof(cache_build_list));
    rb->memset(&runtime_overlay, 0, sizeof(runtime_overlay));
    runtime_palette_id = 0;
    runtime_target = NULL;
    runtime_ready = false;
    scummvm_sky_text_reset();
}

uint32_t scummvm_sky_runtime_get_var(uint16_t index)
{
    if (index >= SKY_NUM_SCRIPT_VARS)
        return 0;

    return script_vars[index];
}

void scummvm_sky_runtime_set_var(uint16_t index, uint32_t value)
{
    if (index < SKY_NUM_SCRIPT_VARS)
        script_vars[index] = value;
}

static void init_script_variables(void)
{
    rb->memset(script_vars, 0, sizeof(script_vars));

    script_vars[SKY_VAR_LOGIC_LIST_NO] = 141;
    script_vars[SKY_VAR_LAMB_GREET] = 62;
    script_vars[SKY_VAR_JOEY_SECTION] = 1;
    script_vars[SKY_VAR_LAMB_SECTION] = 2;
    script_vars[SKY_VAR_S15_FLOOR] = 8371;
    script_vars[SKY_VAR_GUARDIAN_THERE] = 1;
    script_vars[SKY_VAR_DOOR_67_68_FLAG] = 1;
    script_vars[SKY_VAR_SC70_IRIS_FLAG] = 3;
    script_vars[SKY_VAR_DOOR_73_75_FLAG] = 1;
    script_vars[SKY_VAR_SC76_CABINET1_FLAG] = 1;
    script_vars[SKY_VAR_SC76_CABINET2_FLAG] = 1;
    script_vars[SKY_VAR_SC76_CABINET3_FLAG] = 1;
    script_vars[SKY_VAR_DOOR_77_78_FLAG] = 1;
    script_vars[SKY_VAR_SC80_EXIT_FLAG] = 1;
    script_vars[SKY_VAR_SC31_LIFT_FLAG] = 1;
    script_vars[SKY_VAR_SC32_LIFT_FLAG] = 1;
    script_vars[SKY_VAR_SC33_SHED_DOOR_FLAG] = 1;
    script_vars[SKY_VAR_BAND_PLAYING] = 1;
    script_vars[SKY_VAR_COLSTON_AT_TABLE] = 1;
    script_vars[SKY_VAR_SC36_NEXT_DEALER] = 16731;
    script_vars[SKY_VAR_SC36_DOOR_FLAG] = 1;
    script_vars[SKY_VAR_SC37_DOOR_FLAG] = 2;
    script_vars[SKY_VAR_SC40_LOCKER_1_FLAG] = 1;
    script_vars[SKY_VAR_SC40_LOCKER_2_FLAG] = 1;
    script_vars[SKY_VAR_SC40_LOCKER_3_FLAG] = 1;
    script_vars[SKY_VAR_SC40_LOCKER_4_FLAG] = 1;
    script_vars[SKY_VAR_SC40_LOCKER_5_FLAG] = 1;
}

static bool load_script_module(uint16_t module_no,
                               char *status,
                               size_t status_size)
{
    uint16_t file_nr;

    if (module_no >= ARRAYLEN(script_modules)) {
        rb->snprintf(status, status_size,
                     "Sky script module %u is out of range",
                     (unsigned)module_no);
        return false;
    }

    if (script_modules[module_no].data)
        return true;

    file_nr = (uint16_t)(SKY_FILE_MODULE_0 + module_no);
    if (!runtime_target ||
        !scummvm_sky_loader_load_resource(runtime_target, file_nr,
                                          &script_modules[module_no],
                                          status, status_size))
        return false;

    if (script_modules[module_no].size < 2 ||
        (script_modules[module_no].size & 1)) {
        rb->snprintf(status, status_size,
                     "Sky script module %u has bad size",
                     (unsigned)module_no);
        return false;
    }

    return true;
}

static uint16_t module_word(const struct scummvm_sky_resource *module,
                            uint32_t word_index)
{
    const uint8_t *p;

    if (!module->data || word_index * 2 + 1 >= module->size)
        return 0;

    p = module->data + word_index * 2;
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static bool stack_push(uint32_t *stack, uint32_t *stack_pos, uint32_t value)
{
    if (*stack_pos >= SKY_SCRIPT_STACK_SIZE)
        return false;

    stack[(*stack_pos)++] = value;
    return true;
}

static bool stack_pop(uint32_t *stack, uint32_t *stack_pos, uint32_t *value)
{
    if (*stack_pos == 0)
        return false;

    *value = stack[--(*stack_pos)];
    return true;
}

static bool compact_script_slots(const uint16_t *compact,
                                 uint16_t compact_size,
                                 uint16_t *script_no,
                                 uint16_t *script_off,
                                 uint16_t *script_index_out,
                                 uint16_t *offset_index_out)
{
    uint16_t mode = compact_word(compact, compact_size, SKY_COMPACT_MODE);
    uint16_t script_index;
    uint16_t offset_index;

    switch (mode) {
    case 0:
        script_index = SKY_COMPACT_BASE_SUB;
        offset_index = SKY_COMPACT_BASE_SUB_OFF;
        break;
    case 4:
        script_index = SKY_COMPACT_ACTION_SUB;
        offset_index = SKY_COMPACT_ACTION_SUB_OFF;
        break;
    case 8:
        script_index = SKY_COMPACT_GET_TO_SUB;
        offset_index = SKY_COMPACT_GET_TO_SUB_OFF;
        break;
    case 12:
        script_index = SKY_COMPACT_EXTRA_SUB;
        offset_index = SKY_COMPACT_EXTRA_SUB_OFF;
        break;
    default:
        return false;
    }

    *script_no = compact_word(compact, compact_size, script_index);
    *script_off = compact_word(compact, compact_size, offset_index);
    if (script_index_out)
        *script_index_out = script_index;
    if (offset_index_out)
        *offset_index_out = offset_index;
    return *script_no != 0;
}

static bool compact_set_word(uint16_t *compact,
                             uint16_t compact_size,
                             uint16_t index,
                             uint16_t value)
{
    if (!compact || index >= compact_size)
        return false;

    compact[index] = value;
    return true;
}

static const uint16_t compact_field_offsets[] = {
    0, 0, 2, 0, 4, 0, 6, 0, 8, 0, 10, 0, 0, 0, 12, 0,
    14, 0, 16, 0, 18, 0, 20, 0, 22, 0, 24, 0, 26, 0, 28, 0,
    30, 0, 32, 0, 34, 0, 36, 0, 38, 0, 40, 0, 42, 0, 44, 0,
    46, 0, 48, 0, 0, 0, 52, 0, 54, 0, 56, 0, 58, 0, 60, 0,
    62, 0, 64, 0, 66, 0, 68, 0, 70, 0, 72, 0, 74, 0, 76, 0,
    78, 0, 80, 0, 82, 0, 84, 0, 86, 0, 88, 0, 90, 0, 92, 0,
    94, 0, 96, 0, 0, 0, 100, 0, 102, 0, 104, 0, 106, 0, 0, 0,
    110, 0,
};

static const uint16_t megaset_field_offsets[] = {
    0, 0, 2, 0, 4, 0, 6, 0, 8, 0, 0, 0, 10, 0, 0, 0,
    12, 0, 0, 0, 14, 0, 0, 0, 16, 0, 0, 0, 18, 0, 0, 0,
    20, 0, 0, 0, 22, 0, 0, 0, 24, 0, 0, 0,
};

static const int8_t grid_convert_table[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, -1, 35, 36, 37, 38,
    39, 40, 41, -1, 42, 43, 44, 45, 46, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, 47, 70, 48, 49, 50,
    51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    -1, 61, 62, -1, -1, -1, -1, -1, -1, 70,
    63, 64, 65, 66, 67, 68, 69,
};

static bool compact_offset_to_word(uint16_t compact_offset,
                                   uint16_t *word_index)
{
    uint16_t byte_offset;
    uint16_t off = compact_offset;
    uint16_t set;

    if (off < ARRAYLEN(compact_field_offsets)) {
        byte_offset = compact_field_offsets[off];
        if (!byte_offset && off != 0)
            return false;
        *word_index = byte_offset / 2;
        return true;
    }

    off -= ARRAYLEN(compact_field_offsets);
    set = 0;
    while (set < 4) {
        if (off < ARRAYLEN(megaset_field_offsets)) {
            byte_offset = megaset_field_offsets[off];
            if (!byte_offset && off != 0)
                return false;
            *word_index = (uint16_t)(55 + set * 14 + byte_offset / 2);
            return true;
        }
        off -= ARRAYLEN(megaset_field_offsets);

        /* Turn tables live in separate CPT entries, not inside this compact. */
        if (off < 100)
            return false;
        off -= 100;
        set++;
    }

    if (compact_offset & 1)
        return false;

    *word_index = compact_offset / 2;
    return true;
}

static bool compact_mode_slots(uint16_t mode,
                               uint16_t *script_index,
                               uint16_t *offset_index)
{
    switch (mode) {
    case 0:
        *script_index = SKY_COMPACT_BASE_SUB;
        *offset_index = SKY_COMPACT_BASE_SUB_OFF;
        return true;
    case 4:
        *script_index = SKY_COMPACT_ACTION_SUB;
        *offset_index = SKY_COMPACT_ACTION_SUB_OFF;
        return true;
    case 8:
        *script_index = SKY_COMPACT_GET_TO_SUB;
        *offset_index = SKY_COMPACT_GET_TO_SUB_OFF;
        return true;
    case 12:
        *script_index = SKY_COMPACT_EXTRA_SUB;
        *offset_index = SKY_COMPACT_EXTRA_SUB_OFF;
        return true;
    default:
        return false;
    }
}

static bool runtime_save_name(char *name, size_t name_size)
{
    if (!runtime_target || runtime_target->gameid[0] == '\0')
        return false;

    return rb->snprintf(name, name_size, "%s.sky.sav",
                        runtime_target->gameid) < (int)name_size;
}

static bool runtime_save_path(char *path, size_t path_size)
{
    char name[96];

    return runtime_save_name(name, sizeof(name)) &&
           scummvm_make_path(path, path_size, runtime_target->savepath, name);
}

static bool runtime_save_snapshot(void)
{
    char path[MAX_PATH];
    int fd;
    uint32_t i;
    uint32_t cpt_count;

    if (!runtime_ready || !runtime_save_path(path, sizeof(path)))
        return false;

    fd = rb->open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;

    cpt_count = scummvm_sky_cpt_save_entry_count();
    if (!runtime_write_u32(fd, SKY_SAVE_MAGIC) ||
        !runtime_write_u32(fd, SKY_SAVE_VERSION) ||
        !runtime_write_u32(fd, SKY_NUM_SCRIPT_VARS)) {
        rb->close(fd);
        return false;
    }

    for (i = 0; i < SKY_NUM_SCRIPT_VARS; i++) {
        if (!runtime_write_u32(fd, script_vars[i])) {
            rb->close(fd);
            return false;
        }
    }

    if (!runtime_write_u32(fd, cpt_count) ||
        !scummvm_sky_cpt_write_save_entries(fd)) {
        rb->close(fd);
        return false;
    }

    rb->close(fd);
    return true;
}

static bool runtime_load_snapshot(void)
{
    char path[MAX_PATH];
    int fd;
    uint32_t magic;
    uint32_t version;
    uint32_t var_count;
    uint32_t cpt_count;
    uint32_t i;

    if (!runtime_save_path(path, sizeof(path)))
        return false;

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;

    if (!runtime_read_u32(fd, &magic) ||
        !runtime_read_u32(fd, &version) ||
        !runtime_read_u32(fd, &var_count) ||
        magic != SKY_SAVE_MAGIC ||
        version != SKY_SAVE_VERSION ||
        var_count != SKY_NUM_SCRIPT_VARS) {
        rb->close(fd);
        return false;
    }

    for (i = 0; i < SKY_NUM_SCRIPT_VARS; i++) {
        if (!runtime_read_u32(fd, &script_vars[i])) {
            rb->close(fd);
            return false;
        }
    }

    if (!runtime_read_u32(fd, &cpt_count) ||
        !scummvm_sky_cpt_read_save_entries(fd, cpt_count)) {
        rb->close(fd);
        init_script_variables();
        script_vars[SKY_VAR_CUR_SECTION] = 0;
        return false;
    }

    rb->close(fd);
    return true;
}

static void runtime_overlay_clear(void)
{
    rb->memset(&runtime_overlay, 0, sizeof(runtime_overlay));
}

static void runtime_overlay_message(uint32_t text_num,
                                    uint16_t value,
                                    bool selectable,
                                    int y)
{
    struct scummvm_sky_overlay_line *line;

    if (runtime_overlay.count >= SCUMMVM_SKY_OVERLAY_LINES)
        return;

    line = &runtime_overlay.lines[runtime_overlay.count++];
    if (!scummvm_sky_text_decode(text_num, line->text, sizeof(line->text)))
        rb->snprintf(line->text, sizeof(line->text), "Text %lu",
                     (unsigned long)text_num);
    line->value = value;
    line->selectable = selectable;
    line->y = y;
}

static void runtime_overlay_chooser(void)
{
    uint32_t i;
    uint16_t y = SKY_TOP_LEFT_Y;

    runtime_clear_text_mouse(false);
    runtime_overlay_clear();
    runtime_overlay.choosing = true;

    for (i = SKY_VAR_TEXT1;
         i + 1 < SKY_VAR_TEXT1 + 16 &&
         runtime_overlay.count < SCUMMVM_SKY_OVERLAY_LINES;
         i += 2) {
        uint16_t text_id = 0;
        uint16_t target_size;
        uint16_t target_type;
        uint16_t *target;
        const struct scummvm_sky_resource *resource;
        uint16_t height = 18;

        if (!script_vars[i])
            break;
        if (!runtime_alloc_text(script_vars[i],
                                SCUMMVM_SURFACE_W,
                                241,
                                false,
                                SKY_LOGIC_NOP,
                                &text_id,
                                NULL)) {
            runtime_overlay_message(script_vars[i],
                                    (uint16_t)script_vars[i],
                                    true,
                                    (int)(y - SKY_TOP_LEFT_Y));
            y += 18;
            continue;
        }

        target = scummvm_sky_cpt_fetch_mutable(text_id, &target_size,
                                               &target_type, NULL);
        resource = runtime_get_resource(
            (uint16_t)(SKY_FIRST_TEXT_BUFFER +
                       text_id - SKY_FIRST_TEXT_COMPACT));
        if (resource && resource->size >= 22)
            height = read_le16(resource->data + 8);
        if (target) {
            compact_set_word(target, target_size, SKY_COMPACT_GET_TO_FLAG,
                             (uint16_t)script_vars[i]);
            compact_set_word(target, target_size, SKY_COMPACT_DOWN_FLAG,
                             (uint16_t)script_vars[i + 1]);
            compact_set_word(target, target_size, SKY_COMPACT_STATUS,
                             (uint16_t)(compact_word(target, target_size,
                                                     SKY_COMPACT_STATUS) |
                                        SKY_ST_MOUSE));
            compact_set_word(target, target_size, SKY_COMPACT_X,
                             SKY_TOP_LEFT_X);
            compact_set_word(target, target_size, SKY_COMPACT_Y, y);
            compact_set_word(target, target_size,
                             SKY_COMPACT_MOUSE_SIZE_X,
                             SCUMMVM_SURFACE_W);
            compact_set_word(target, target_size,
                             SKY_COMPACT_MOUSE_SIZE_Y,
                             height);
        }
        y = (uint16_t)(y + height);
        (void)target_type;
    }
}

static uint16_t route_check_block(const uint16_t *grid, uint16_t pos)
{
    uint16_t best = 0xffff;
    static const int16_t dirs[] = { -1, 1, -SKY_ROUTE_GRID_W,
                                    SKY_ROUTE_GRID_W };
    uint16_t i;

    for (i = 0; i < ARRAYLEN(dirs); i++) {
        uint16_t value = grid[pos + dirs[i]];

        if (value && value < best)
            best = value;
    }

    return best;
}

static void route_clip_x(uint16_t x, uint8_t *block, int16_t *initial)
{
    if (x < SKY_TOP_LEFT_X) {
        *block = 0;
        *initial = (int16_t)(x - SKY_TOP_LEFT_X);
    } else if (x >= SKY_TOP_LEFT_X + SCUMMVM_SURFACE_W) {
        *block = (SCUMMVM_SURFACE_W - 1) >> 3;
        *initial = (int16_t)(x - (SKY_TOP_LEFT_X + SCUMMVM_SURFACE_W - 1));
    } else {
        *block = (uint8_t)((x - SKY_TOP_LEFT_X) >> 3);
        *initial = 0;
    }
}

static void route_clip_y(uint16_t y, uint8_t *block, int16_t *initial)
{
    if (y < SKY_TOP_LEFT_Y) {
        *block = 0;
        *initial = (int16_t)(y - SKY_TOP_LEFT_Y);
    } else if (y >= SKY_TOP_LEFT_Y + 192) {
        *block = (192 - 1) >> 3;
        *initial = (int16_t)(y - (SKY_TOP_LEFT_Y + 192));
    } else {
        *block = (uint8_t)((y - SKY_TOP_LEFT_Y) >> 3);
        *initial = 0;
    }
}

static uint16_t runtime_compact_grid_width(const uint16_t *compact,
                                           uint16_t compact_size)
{
    uint16_t set = compact_word(compact, compact_size,
                                SKY_COMPACT_MEGA_SET) / SKY_NEXT_MEGA_SET;
    uint16_t index = (uint16_t)(55 + set * 14);

    if (set >= 4)
        index = 55;
    return compact_word(compact, compact_size, index);
}

static uint16_t runtime_compact_megaset_word(const uint16_t *compact,
                                             uint16_t compact_size,
                                             uint16_t field)
{
    uint16_t set = compact_word(compact, compact_size,
                                SKY_COMPACT_MEGA_SET) / SKY_NEXT_MEGA_SET;
    uint16_t index;

    if (set >= 4)
        set = 0;
    index = (uint16_t)(55 + set * 14 + field);
    return compact_word(compact, compact_size, index);
}

static bool runtime_compacts_collide(const uint16_t *self,
                                     uint16_t self_size,
                                     const uint16_t *target,
                                     uint16_t target_size)
{
    uint16_t self_x = compact_word(self, self_size, SKY_COMPACT_X);
    uint16_t self_y = compact_word(self, self_size, SKY_COMPACT_Y);
    uint16_t dir = compact_word(self, self_size, SKY_COMPACT_DIR);
    int x = compact_word(target, target_size, SKY_COMPACT_X) & 0xfff8;
    int y = compact_word(target, target_size, SKY_COMPACT_Y) & 0xfff8;
    int self_col_offset = runtime_compact_megaset_word(self, self_size, 1);
    int self_col_width = runtime_compact_megaset_word(self, self_size, 2);
    int self_last_chr = runtime_compact_megaset_word(self, self_size, 3);
    int target_col_offset =
        runtime_compact_megaset_word(target, target_size, 1);
    int target_col_width =
        runtime_compact_megaset_word(target, target_size, 2);
    int target_last_chr =
        runtime_compact_megaset_word(target, target_size, 3);

    switch (dir) {
    case SKY_DIR_UP:
        x -= self_col_offset;
        x += target_col_offset;
        if (x + target_col_width < self_x)
            return false;
        x -= self_col_width;
        if (x >= self_x)
            return false;
        y += 8;
        if (y == self_y)
            return true;
        y += 8;
        return y == self_y;
    case SKY_DIR_DOWN:
        x -= self_col_offset;
        x += target_col_offset;
        if (x + target_col_width < self_x)
            return false;
        x -= self_col_width;
        if (x >= self_x)
            return false;
        y -= 8;
        if (y == self_y)
            return true;
        y -= 8;
        return y == self_y;
    case SKY_DIR_LEFT:
        if (y != self_y)
            return false;
        x += target_last_chr;
        if (x == self_x)
            return true;
        x -= 8;
        return x == self_x;
    case SKY_DIR_RIGHT:
    default:
        if (y != self_y)
            return false;
        x -= self_last_chr;
        if (x == self_x)
            return true;
        x -= 8;
        return x == self_x;
    }
}

static bool runtime_route_collision_check(uint16_t *compact,
                                          uint16_t compact_size,
                                          uint16_t current_id)
{
    const uint16_t *logic_list;
    uint16_t list_size;
    uint16_t list_type;
    uint16_t waiting_for;
    uint16_t pos = 0;
    uint32_t guard = 0;

    if ((compact_word(compact, compact_size, SKY_COMPACT_X) & 7) ||
        (compact_word(compact, compact_size, SKY_COMPACT_Y) & 7))
        return false;

    waiting_for = compact_word(compact, compact_size,
                               SKY_COMPACT_WAITING_FOR);
    if (waiting_for == 0xffff) {
        compact_set_word(compact, compact_size, SKY_COMPACT_WAITING_FOR, 0);
        return false;
    }

    if (waiting_for) {
        uint16_t target_size;
        uint16_t target_type;
        const uint16_t *target = scummvm_sky_cpt_fetch(waiting_for,
                                                       &target_size,
                                                       &target_type,
                                                       NULL);

        if (target && runtime_compacts_collide(compact, compact_size,
                                               target, target_size)) {
            (void)target_type;
            return true;
        }
        compact_set_word(compact, compact_size, SKY_COMPACT_WAITING_FOR, 0);
        (void)target_type;
    }

    logic_list = scummvm_sky_cpt_fetch((uint16_t)script_vars[SKY_VAR_LOGIC_LIST_NO],
                                       &list_size, &list_type, NULL);
    if (!logic_list)
        return false;

    while (guard++ < SKY_MAX_LOGIC_SCAN) {
        uint16_t id;
        uint16_t target_size;
        uint16_t target_type;
        uint16_t *target;

        if (pos >= list_size)
            break;
        id = logic_list[pos++];
        if (!id)
            break;
        if (id == 0xffff) {
            if (pos >= list_size)
                break;
            logic_list = scummvm_sky_cpt_fetch(logic_list[pos++],
                                               &list_size,
                                               &list_type,
                                               NULL);
            pos = 0;
            if (!logic_list)
                break;
            continue;
        }
        if (id == current_id)
            continue;

        target = scummvm_sky_cpt_fetch_mutable(id, &target_size,
                                               &target_type, NULL);
        if (!target ||
            !(compact_word(target, target_size, SKY_COMPACT_STATUS) &
              SKY_ST_COLLISION) ||
            compact_word(target, target_size, SKY_COMPACT_SCREEN) !=
            compact_word(compact, compact_size, SKY_COMPACT_SCREEN) ||
            !runtime_compacts_collide(compact, compact_size,
                                      target, target_size)) {
            (void)target_type;
            continue;
        }

        script_vars[SKY_VAR_HIT_ID] = id;
        if (compact_word(target, target_size, SKY_COMPACT_LOGIC_FIELD) !=
            SKY_LOGIC_AR_ANIM) {
            uint16_t mode = compact_word(compact, compact_size,
                                         SKY_COMPACT_MODE);
            uint16_t script_index;
            uint16_t offset_index;

            compact_set_word(compact, compact_size,
                             SKY_COMPACT_WAITING_FOR, 0xffff);
            compact_set_word(target, target_size,
                             SKY_COMPACT_WAITING_FOR, current_id);
            if (compact_mode_slots(mode, &script_index, &offset_index))
                compact_set_word(compact, compact_size, offset_index, 0);
            compact_set_word(compact, compact_size,
                             SKY_COMPACT_LOGIC_FIELD, SKY_LOGIC_SCRIPT);
            (void)target_type;
            return true;
        }

        if (compact_word(compact, compact_size, SKY_COMPACT_MINI_BUMP)) {
            char mini_status[96];

            if (runtime_run_compact_script(
                    current_id,
                    compact_word(compact, compact_size,
                                 SKY_COMPACT_MINI_BUMP),
                    mini_status,
                    sizeof(mini_status)))
                return true;
        }

        compact_set_word(compact, compact_size,
                         SKY_COMPACT_WAITING_FOR, id);
        (void)target_type;
        return true;
    }

    (void)list_type;
    return false;
}

static bool runtime_route_anim_frame(uint16_t *compact,
                                     uint16_t compact_size,
                                     uint16_t dir,
                                     uint16_t *count)
{
    static const uint16_t anim_fields[] = { 4, 5, 6, 7 };
    const uint16_t *anim;
    uint16_t anim_size;
    uint16_t anim_type;
    uint16_t anim_id;
    uint16_t ar_index;
    uint16_t word_index;

    if (dir >= ARRAYLEN(anim_fields))
        return false;

    anim_id = runtime_compact_megaset_word(compact, compact_size,
                                           anim_fields[dir]);
    anim = scummvm_sky_cpt_fetch(anim_id, &anim_size, &anim_type, NULL);
    if (!anim || anim_size < 4) {
        (void)anim_type;
        return false;
    }

    ar_index = compact_word(compact, compact_size,
                            SKY_COMPACT_AR_ANIM_INDEX);
    word_index = ar_index / 2;
    if (word_index >= anim_size || anim[word_index] == 0) {
        ar_index = 0;
        word_index = 0;
        compact_set_word(compact, compact_size,
                         SKY_COMPACT_AR_ANIM_INDEX, 0);
    }

    if (word_index + 3 >= anim_size || anim[word_index] == 0) {
        (void)anim_type;
        return false;
    }

    *count = anim[word_index];
    compact_set_word(compact, compact_size, SKY_COMPACT_FRAME,
                     anim[word_index + 1]);
    compact_set_word(compact, compact_size, SKY_COMPACT_X,
                     (uint16_t)(compact_word(compact, compact_size,
                                             SKY_COMPACT_X) +
                                (int16_t)anim[word_index + 2]));
    compact_set_word(compact, compact_size, SKY_COMPACT_Y,
                     (uint16_t)(compact_word(compact, compact_size,
                                             SKY_COMPACT_Y) +
                                (int16_t)anim[word_index + 3]));
    compact_set_word(compact, compact_size, SKY_COMPACT_AR_ANIM_INDEX,
                     (uint16_t)(ar_index + 8));
    (void)anim_type;
    return true;
}

static const uint8_t *runtime_grid_for_screen(uint16_t screen)
{
    int8_t grid_index;
    const struct scummvm_sky_resource *grid;

    if (screen >= ARRAYLEN(grid_convert_table))
        return NULL;

    grid_index = grid_convert_table[screen];
    if (grid_index < 0 || grid_index >= 70)
        return NULL;

    grid = runtime_get_resource((uint16_t)(SKY_GRID_FILE_START + grid_index));
    if (!grid || grid->size < SKY_GRID_SIZE)
        return NULL;

    return grid->data;
}

static uint8_t *runtime_mutable_grid_for_screen(uint16_t screen)
{
    return (uint8_t *)runtime_grid_for_screen(screen);
}

static bool runtime_grid_values(uint16_t x,
                                uint16_t y,
                                uint16_t width,
                                const uint16_t *compact,
                                uint16_t compact_size,
                                uint8_t **grid_out,
                                uint32_t *bit_out,
                                uint32_t *width_out)
{
    uint32_t bit_pos;
    uint32_t tmp_bits;
    uint8_t *grid;
    uint16_t screen = compact_word(compact, compact_size,
                                   SKY_COMPACT_SCREEN);

    if (y < SKY_TOP_LEFT_Y)
        return false;
    y = (uint16_t)((y - SKY_TOP_LEFT_Y) >> 3);
    if (y >= 192 >> 3)
        return false;

    bit_pos = y * 40;
    width++;
    x >>= 3;

    if (x < (SKY_TOP_LEFT_X >> 3)) {
        uint16_t left = SKY_TOP_LEFT_X >> 3;

        if (x + width < left)
            return false;
        width = (uint16_t)(width - (left - x));
        x = 0;
    } else {
        x = (uint16_t)(x - (SKY_TOP_LEFT_X >> 3));
    }

    if ((SCUMMVM_SURFACE_W >> 3) <= x)
        return false;
    if ((SCUMMVM_SURFACE_W >> 3) < x + width)
        width = (uint16_t)((SCUMMVM_SURFACE_W >> 3) - x);

    grid = runtime_mutable_grid_for_screen(screen);
    if (!grid)
        return false;

    bit_pos += x;
    tmp_bits = 0x1f - (bit_pos & 0x1f);
    bit_pos &= ~0x1fUL;
    bit_pos += tmp_bits;

    *grid_out = grid;
    *bit_out = bit_pos;
    *width_out = width;
    return true;
}

static void runtime_grid_apply_bits(uint8_t *grid,
                                    uint32_t bit_num,
                                    uint32_t width,
                                    bool plot)
{
    uint32_t i;

    for (i = 0; i < width; i++) {
        if (plot)
            grid[bit_num >> 3] |= (uint8_t)(1 << (bit_num & 7));
        else
            grid[bit_num >> 3] &= (uint8_t)~(1 << (bit_num & 7));

        if ((bit_num & 0x1f) == 0)
            bit_num += 0x3f;
        else
            bit_num--;
    }
}

static void runtime_grid_object_to_walk(const uint16_t *compact,
                                        uint16_t compact_size,
                                        bool plot)
{
    uint8_t *grid;
    uint32_t bit_num;
    uint32_t width;

    if (runtime_grid_values(compact_word(compact, compact_size,
                                         SKY_COMPACT_X),
                            compact_word(compact, compact_size,
                                         SKY_COMPACT_Y),
                            runtime_compact_grid_width(compact,
                                                       compact_size),
                            compact,
                            compact_size,
                            &grid,
                            &bit_num,
                            &width))
        runtime_grid_apply_bits(grid, bit_num, width, plot);
}

static void runtime_grid_plot(uint16_t x,
                              uint16_t y,
                              uint16_t width,
                              const uint16_t *compact,
                              uint16_t compact_size,
                              bool plot)
{
    uint8_t *grid;
    uint32_t bit_num;
    uint32_t out_width;
    uint16_t grid_width = plot && width > 0 ? (uint16_t)(width - 1) : width;

    if (runtime_grid_values(x, y, grid_width, compact, compact_size,
                            &grid, &bit_num, &out_width))
        runtime_grid_apply_bits(grid, bit_num, out_width, plot);
}

static bool runtime_init_route_grid(uint16_t *route_grid,
                                    uint16_t screen,
                                    uint16_t width)
{
    const uint8_t *screen_grid = runtime_grid_for_screen(screen);
    uint16_t *grid_pos;
    uint8_t stretch = 0;
    uint8_t bits_left = 0;
    uint32_t grid_data = 0;
    uint16_t y;
    uint16_t x;

    if (!screen_grid)
        return false;

    rb->memset(route_grid, 0, SKY_ROUTE_GRID_SIZE * sizeof(route_grid[0]));
    screen_grid += SKY_GRID_SIZE;
    grid_pos = route_grid + SKY_ROUTE_GRID_SIZE - SKY_ROUTE_GRID_W - 2;

    for (y = 0; y < SKY_ROUTE_GRID_H - 2; y++) {
        for (x = 0; x < SKY_ROUTE_GRID_W - 2; x++) {
            if (!bits_left) {
                screen_grid -= 4;
                grid_data = read_le32(screen_grid);
                bits_left = 32;
            }

            if (grid_data & 1) {
                *grid_pos = 0xffff;
                stretch = (uint8_t)width;
            } else if (stretch) {
                *grid_pos = 0xffff;
                stretch--;
            }

            grid_pos--;
            bits_left--;
            grid_data >>= 1;
        }
        grid_pos -= 2;
        stretch = 0;
    }

    return true;
}

static bool runtime_calc_route_grid(uint16_t *grid,
                                    uint8_t start_x,
                                    uint8_t start_y,
                                    uint8_t dest_x,
                                    uint8_t dest_y)
{
    int16_t dir_x;
    int16_t dir_y;
    uint8_t roi_x;
    uint8_t roi_y;
    uint16_t dest_pos;
    uint16_t start_pos;
    uint16_t walk_start;
    bool changed = true;

    if (start_y > dest_y) {
        dir_y = -SKY_ROUTE_GRID_W;
        roi_y = start_y;
    } else {
        dir_y = SKY_ROUTE_GRID_W;
        roi_y = (SKY_ROUTE_GRID_H - 1) - start_y;
    }

    if (start_x > dest_x) {
        dir_x = -1;
        roi_x = start_x + 2;
    } else {
        dir_x = 1;
        roi_x = (SKY_ROUTE_GRID_W - 1) - start_x;
    }

    dest_pos = (uint16_t)((dest_y + 1) * SKY_ROUTE_GRID_W + dest_x + 1);
    start_pos = (uint16_t)((start_y + 1) * SKY_ROUTE_GRID_W + start_x + 1);
    walk_start = start_pos;
    grid[start_pos] = 1;

    if (roi_y < SKY_ROUTE_GRID_H - 3)
        walk_start = (uint16_t)(walk_start - dir_y);
    if (roi_x < SKY_ROUTE_GRID_W - 2)
        walk_start = (uint16_t)(walk_start - dir_x);

    while (!grid[dest_pos] && changed) {
        uint16_t y_pos = walk_start;
        uint8_t y;

        changed = false;
        for (y = 0; y < roi_y; y++) {
            uint16_t x_pos = y_pos;
            uint8_t x;

            for (x = 0; x < roi_x; x++) {
                if (!grid[x_pos]) {
                    uint16_t block = route_check_block(grid, x_pos);

                    if (block < 0xffff) {
                        grid[x_pos] = (uint16_t)(block + 1);
                        changed = true;
                    }
                }
                x_pos = (uint16_t)(x_pos + dir_x);
            }
            y_pos = (uint16_t)(y_pos + dir_y);
        }

        if (!grid[dest_pos]) {
            if (roi_y < SKY_ROUTE_GRID_H - 4) {
                walk_start = (uint16_t)(walk_start - dir_y);
                roi_y++;
            }
            if (roi_x < SKY_ROUTE_GRID_W - 4) {
                walk_start = (uint16_t)(walk_start - dir_x);
                roi_x++;
            }
        }
    }

    return grid[dest_pos] != 0;
}

static uint16_t route_command_for_dir(int16_t dir)
{
    if (dir == -1)
        return SKY_DIR_RIGHT;
    if (dir == 1)
        return SKY_DIR_LEFT;
    if (dir == -SKY_ROUTE_GRID_W)
        return SKY_DIR_DOWN;
    return SKY_DIR_UP;
}

static bool runtime_make_route_data(uint16_t *route_grid,
                                    uint8_t dest_x,
                                    uint8_t dest_y,
                                    uint16_t *out,
                                    uint16_t out_words)
{
    static const int16_t dirs[] = { -1, 1, -SKY_ROUTE_GRID_W,
                                    SKY_ROUTE_GRID_W };
    uint16_t temp[SKY_ROUTE_WORDS];
    uint16_t temp_pos = SKY_ROUTE_WORDS;
    uint16_t route_pos =
        (uint16_t)((dest_y + 1) * SKY_ROUTE_GRID_W + dest_x + 1);
    uint16_t last_val = (uint16_t)(route_grid[route_pos] - 1);
    uint16_t src;
    uint16_t dst = 0;

    rb->memset(temp, 0, sizeof(temp));
    while (last_val && temp_pos >= 2) {
        int16_t walk_dir = 0;
        uint16_t i;

        temp_pos -= 2;
        for (i = 0; i < ARRAYLEN(dirs); i++) {
            if (route_grid[route_pos + dirs[i]] == last_val) {
                temp[temp_pos + 1] = route_command_for_dir(dirs[i]);
                walk_dir = dirs[i];
                break;
            }
        }

        if (!walk_dir)
            return false;

        while (last_val &&
               route_grid[route_pos + walk_dir] == last_val) {
            temp[temp_pos] += SKY_WALK_JUMP;
            last_val--;
            route_pos = (uint16_t)(route_pos + walk_dir);
        }
    }

    rb->memset(out, 0, out_words * sizeof(out[0]));
    for (src = temp_pos; src < SKY_ROUTE_WORDS && dst < out_words - 1; src++)
        out[dst++] = temp[src];
    return true;
}

static void runtime_route_prepend_initial_x(uint16_t *route,
                                            uint16_t route_words,
                                            int16_t initial_x)
{
    uint16_t dist;
    uint16_t dir;
    uint16_t i;

    if (!initial_x)
        return;

    dist = (uint16_t)(((initial_x < 0 ? -initial_x : initial_x) + 7) &
                      0xfff8);
    dir = initial_x < 0 ? SKY_DIR_RIGHT : SKY_DIR_LEFT;
    for (i = route_words - 1; i >= 2; i--)
        route[i] = route[i - 2];
    route[0] = dist;
    route[1] = dir;
}

static bool runtime_build_route(uint16_t *compact,
                                uint16_t compact_size,
                                uint16_t *route_result)
{
    uint16_t route_grid[SKY_ROUTE_GRID_SIZE];
    uint16_t route_data[SKY_ROUTE_WORDS];
    uint16_t scratch_id;
    uint16_t scratch_size;
    uint16_t scratch_type;
    uint16_t *scratch;
    uint8_t start_x;
    uint8_t start_y;
    uint8_t dest_x;
    uint8_t dest_y;
    int16_t init_start_x;
    int16_t init_start_y;
    int16_t init_dest_x;
    int16_t init_dest_y;
    uint16_t screen = compact_word(compact, compact_size,
                                   SKY_COMPACT_SCREEN);
    uint16_t grid_width = runtime_compact_grid_width(compact, compact_size);

    *route_result = 1;
    scratch_id = compact_word(compact, compact_size,
                              SKY_COMPACT_ANIM_SCRATCH_ID);
    scratch = scummvm_sky_cpt_fetch_mutable(scratch_id, &scratch_size,
                                            &scratch_type, NULL);
    if (!scratch || scratch_size < 2)
        return false;

    rb->memset(scratch, 0, scratch_size * sizeof(scratch[0]));
    if (!runtime_init_route_grid(route_grid, screen, grid_width))
        return false;

    route_clip_x(compact_word(compact, compact_size, SKY_COMPACT_X),
                 &start_x, &init_start_x);
    route_clip_y(compact_word(compact, compact_size, SKY_COMPACT_Y),
                 &start_y, &init_start_y);
    route_clip_x(compact_word(compact, compact_size, SKY_COMPACT_AR_TARGET_X),
                 &dest_x, &init_dest_x);
    route_clip_y(compact_word(compact, compact_size, SKY_COMPACT_AR_TARGET_Y),
                 &dest_y, &init_dest_y);

    if (start_x == dest_x && start_y == dest_y) {
        *route_result = 2;
        (void)init_start_y;
        (void)init_dest_x;
        (void)init_dest_y;
        (void)scratch_type;
        return true;
    }

    if (route_grid[(dest_y + 1) * SKY_ROUTE_GRID_W + dest_x + 1] ||
        !runtime_calc_route_grid(route_grid, start_x, start_y,
                                 dest_x, dest_y) ||
        !runtime_make_route_data(route_grid, dest_x, dest_y,
                                 route_data, ARRAYLEN(route_data))) {
        (void)init_start_y;
        (void)init_dest_x;
        (void)init_dest_y;
        (void)scratch_type;
        return true;
    }

    runtime_route_prepend_initial_x(route_data, ARRAYLEN(route_data),
                                    init_start_x);
    rb->memcpy(scratch, route_data,
               MIN((size_t)scratch_size, sizeof(route_data) / sizeof(route_data[0])) *
               sizeof(route_data[0]));
    compact_set_word(compact, compact_size, SKY_COMPACT_GRAFIX_PROG_ID,
                     scratch_id);
    compact_set_word(compact, compact_size, SKY_COMPACT_GRAFIX_PROG_POS, 0);
    compact_set_word(compact, compact_size, SKY_COMPACT_AR_ANIM_INDEX, 0);
    *route_result = 0;
    (void)init_start_y;
    (void)init_dest_x;
    (void)init_dest_y;
    (void)scratch_type;
    return true;
}

static bool runtime_step_route(uint16_t *compact,
                               uint16_t compact_size,
                               uint16_t current_id)
{
    uint16_t route_id = compact_word(compact, compact_size,
                                     SKY_COMPACT_GRAFIX_PROG_ID);
    uint16_t pos = compact_word(compact, compact_size,
                                SKY_COMPACT_GRAFIX_PROG_POS);
    uint16_t route_size;
    uint16_t route_type;
    uint16_t *route = scummvm_sky_cpt_fetch_mutable(route_id,
                                                    &route_size,
                                                    &route_type,
                                                    NULL);
    uint16_t distance;
    uint16_t dir;
    uint16_t step;
    uint16_t anim_count = 0;
    int dx = 0;
    int dy = 0;

    if (runtime_route_collision_check(compact, compact_size, current_id))
        return compact_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD) ==
               SKY_LOGIC_AR_ANIM;

    if (!route || pos + 1 >= route_size ||
        pos + 1 >= SKY_ROUTE_WORDS) {
        compact_set_word(compact, compact_size, SKY_COMPACT_DOWN_FLAG, 0);
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                         SKY_LOGIC_SCRIPT);
        (void)route_type;
        return false;
    }

    while (pos + 1 < route_size && route[pos] == 0) {
        pos = (uint16_t)(pos + 2);
        compact_set_word(compact, compact_size,
                         SKY_COMPACT_GRAFIX_PROG_POS, pos);
        if (pos + 1 >= route_size || route[pos] == 0) {
            compact_set_word(compact, compact_size, SKY_COMPACT_DOWN_FLAG, 0);
            compact_set_word(compact, compact_size,
                             SKY_COMPACT_LOGIC_FIELD, SKY_LOGIC_SCRIPT);
            (void)route_type;
            return false;
        }
    }

    distance = route[pos];
    dir = route[pos + 1];
    if (compact_word(compact, compact_size, SKY_COMPACT_DIR) != dir) {
        const uint16_t *turn_table;
        uint16_t turn_size;
        uint16_t turn_type;
        uint16_t table_index;
        uint16_t old_dir = compact_word(compact, compact_size,
                                        SKY_COMPACT_DIR);
        uint16_t table_id;
        uint16_t turn_id = 0;

        compact_set_word(compact, compact_size, SKY_COMPACT_DIR, dir);
        if (compact_offset_to_word((uint16_t)(158 +
                                               compact_word(compact,
                                                            compact_size,
                                                            SKY_COMPACT_MEGA_SET)),
                                    &table_index)) {
            table_id = compact_word(compact, compact_size, table_index);
            turn_table = scummvm_sky_cpt_fetch(table_id, &turn_size,
                                               &turn_type, NULL);
            if (turn_table && old_dir < 5 && dir < 5 &&
                old_dir * 5 + dir < turn_size)
                turn_id = turn_table[old_dir * 5 + dir];
            (void)turn_type;
        }

        if (turn_id) {
            compact_set_word(compact, compact_size,
                             SKY_COMPACT_TURN_PROG_ID, turn_id);
            compact_set_word(compact, compact_size,
                             SKY_COMPACT_TURN_PROG_POS, 0);
            compact_set_word(compact, compact_size,
                             SKY_COMPACT_AR_ANIM_INDEX, 0);
            compact_set_word(compact, compact_size,
                             SKY_COMPACT_LOGIC_FIELD,
                             SKY_LOGIC_AR_TURNING);
            (void)route_type;
            return true;
        }
    }
    compact_set_word(compact, compact_size, SKY_COMPACT_DIR, dir);

    if (runtime_route_anim_frame(compact, compact_size, dir, &anim_count)) {
        step = anim_count;
    } else {
        step = distance > SKY_WALK_JUMP ? SKY_WALK_JUMP : distance;
        switch (dir) {
        case SKY_DIR_UP:
            dy = -(int)step;
            break;
        case SKY_DIR_DOWN:
            dy = step;
            break;
        case SKY_DIR_LEFT:
            dx = -(int)step;
            break;
        case SKY_DIR_RIGHT:
            dx = step;
            break;
        default:
            break;
        }
        compact_set_word(compact, compact_size, SKY_COMPACT_X,
                         (uint16_t)(compact_word(compact, compact_size,
                                                 SKY_COMPACT_X) + dx));
        compact_set_word(compact, compact_size, SKY_COMPACT_Y,
                         (uint16_t)(compact_word(compact, compact_size,
                                                 SKY_COMPACT_Y) + dy));
    }

    if (step > distance)
        step = distance;
    route[pos] = (uint16_t)(distance - step);
    if (route[pos] == 0)
        compact_set_word(compact, compact_size,
                         SKY_COMPACT_GRAFIX_PROG_POS, pos + 2);
    (void)route_type;
    return true;
}

static bool decode_first_active_script(
    const struct scummvm_sky_runtime_info *info,
    char *status,
    size_t status_size)
{
    uint16_t *compact;
    const struct scummvm_sky_resource *module;
    const char *name = NULL;
    uint16_t cpt_size;
    uint16_t cpt_type;
    uint16_t logic;
    uint16_t script_no;
    uint16_t script_off;
    uint16_t script_slot_index = 0;
    uint16_t script_offset_index = 0;
    uint16_t module_no;
    uint16_t script_index;
    uint32_t module_words;
    uint32_t pc;
    uint16_t opcode;
    uint32_t stack[SKY_SCRIPT_STACK_SIZE];
    uint32_t stack_pos = 0;
    uint32_t steps = 0;

    compact = scummvm_sky_cpt_fetch_mutable(info->first_logic_id,
                                            &cpt_size, &cpt_type, &name);
    if (!compact) {
        rb->strlcpy(status, "Sky runtime has no active compact",
                    status_size);
        return false;
    }

    logic = compact_word(compact, cpt_size, SKY_COMPACT_LOGIC);
    if (logic != SKY_LOGIC_SCRIPT) {
        rb->snprintf(status, status_size,
                     "Sky runtime: first %04x logic %u %s",
                     (unsigned)info->first_logic_id,
                     (unsigned)logic,
                     name ? name : "");
        return true;
    }

    if (!compact_script_slots(compact, cpt_size, &script_no, &script_off,
                              &script_slot_index, &script_offset_index)) {
        rb->snprintf(status, status_size,
                     "Sky runtime: first %04x has unsupported mode",
                     (unsigned)info->first_logic_id);
        return true;
    }

    module_no = script_no >> 12;
    script_index = script_no & 0x0fff;
    if (!load_script_module(module_no, status, status_size))
        return false;

    module = &script_modules[module_no];
    module_words = module->size / 2;
    if (script_index >= module_words) {
        rb->snprintf(status, status_size,
                     "Sky script %u:%u outside module",
                     (unsigned)module_no, (unsigned)script_index);
        return false;
    }

    pc = script_off ? script_off : module_word(module, script_index);
    if (pc >= module_words) {
        rb->snprintf(status, status_size,
                     "Sky script %u:%u bad pc %lu",
                     (unsigned)module_no, (unsigned)script_index,
                     (unsigned long)pc);
        return false;
    }

    while (steps++ < SKY_SCRIPT_MAX_STEPS) {
        uint32_t a = 0;
        uint32_t b = 0;
        uint32_t c = 0;
        uint16_t arg_count;
        uint16_t skip;
        uint16_t mcode;

        if (pc >= module_words) {
            rb->snprintf(status, status_size,
                         "Sky script %u:%u ran past module",
                         (unsigned)module_no, (unsigned)script_index);
            return false;
        }

        opcode = module_word(module, pc++);
        switch (opcode) {
        case 0:
            if (pc >= module_words) {
                rb->strlcpy(status, "Sky script push_variable failed",
                            status_size);
                return false;
            }
            a = module_word(module, pc++) / 4;
            if (a >= SKY_NUM_SCRIPT_VARS ||
                !stack_push(stack, &stack_pos, script_vars[a])) {
                rb->strlcpy(status, "Sky script push_variable failed",
                            status_size);
                return false;
            }
            break;
        case 1:
            if (!stack_pop(stack, &stack_pos, &a) ||
                !stack_pop(stack, &stack_pos, &b) ||
                !stack_push(stack, &stack_pos, a > b ? 1 : 0)) {
                rb->strlcpy(status, "Sky script less_than failed",
                            status_size);
                return false;
            }
            break;
        case 2:
            if (pc >= module_words ||
                !stack_push(stack, &stack_pos, module_word(module, pc++))) {
                rb->strlcpy(status, "Sky script push_number failed",
                            status_size);
                return false;
            }
            break;
        case 3:
            if (!stack_pop(stack, &stack_pos, &a) ||
                !stack_pop(stack, &stack_pos, &b) ||
                !stack_push(stack, &stack_pos, a != b ? 1 : 0)) {
                rb->strlcpy(status, "Sky script not_equal failed",
                            status_size);
                return false;
            }
            break;
        case 4:
            if (!stack_pop(stack, &stack_pos, &a) ||
                !stack_pop(stack, &stack_pos, &b) ||
                !stack_push(stack, &stack_pos, (a && b) ? 1 : 0)) {
                rb->strlcpy(status, "Sky script if_and failed",
                            status_size);
                return false;
            }
            break;
        case 5:
            if (pc >= module_words ||
                !stack_pop(stack, &stack_pos, &a)) {
                rb->strlcpy(status, "Sky script skip_zero failed",
                            status_size);
                return false;
            }
            skip = module_word(module, pc++);
            if (!a)
                pc += skip / 2;
            break;
        case 6:
            if (pc >= module_words ||
                !stack_pop(stack, &stack_pos, &a)) {
                rb->strlcpy(status, "Sky script pop_var failed",
                            status_size);
                return false;
            }
            b = module_word(module, pc++) / 4;
            if (b < SKY_NUM_SCRIPT_VARS)
                script_vars[b] = a;
            break;
        case 7:
            if (!stack_pop(stack, &stack_pos, &a) ||
                !stack_pop(stack, &stack_pos, &b) ||
                !stack_push(stack, &stack_pos, b - a)) {
                rb->strlcpy(status, "Sky script minus failed", status_size);
                return false;
            }
            break;
        case 8:
            if (!stack_pop(stack, &stack_pos, &a) ||
                !stack_pop(stack, &stack_pos, &b) ||
                !stack_push(stack, &stack_pos, b + a)) {
                rb->strlcpy(status, "Sky script plus failed", status_size);
                return false;
            }
            break;
        case 9:
            if (pc >= module_words) {
                rb->strlcpy(status, "Sky script skip_always failed",
                            status_size);
                return false;
            }
            skip = module_word(module, pc++);
            pc += skip / 2;
            break;
        case 10:
            if (!stack_pop(stack, &stack_pos, &a) ||
                !stack_pop(stack, &stack_pos, &b) ||
                !stack_push(stack, &stack_pos, (a || b) ? 1 : 0)) {
                rb->strlcpy(status, "Sky script if_or failed", status_size);
                return false;
            }
            break;
        case 11:
            if (pc + 1 >= module_words) {
                rb->strlcpy(status, "Sky script mcode failed", status_size);
                return false;
            }
            arg_count = module_word(module, pc++);
            if (arg_count > 3 ||
                (arg_count >= 1 && !stack_pop(stack, &stack_pos, &a)) ||
                (arg_count >= 2 && !stack_pop(stack, &stack_pos, &b)) ||
                (arg_count >= 3 && !stack_pop(stack, &stack_pos, &c))) {
                rb->strlcpy(status, "Sky script mcode args failed",
                            status_size);
                return false;
            }
            mcode = module_word(module, pc++) / 4;
            {
                bool continue_script = true;
                if (!runtime_mcode(mcode, a, b, c, info->first_logic_id,
                                   compact, cpt_size, &continue_script,
                                   status, status_size))
                    return false;
                if (!continue_script) {
                    compact_set_word(compact, cpt_size, script_offset_index,
                                     (uint16_t)pc);
                    return true;
                }
            }
            break;
        case 15:
            if (pc >= module_words) {
                rb->strlcpy(status, "Sky script push_offset failed",
                            status_size);
                return false;
            }
            skip = module_word(module, pc++);
            if (!compact_offset_to_word(skip, &skip) ||
                !stack_push(stack, &stack_pos,
                            compact_word(compact, cpt_size, skip))) {
                rb->strlcpy(status, "Sky script push_offset failed",
                            status_size);
                return false;
            }
            break;
        case 16:
            if (pc >= module_words ||
                !stack_pop(stack, &stack_pos, &a)) {
                rb->strlcpy(status, "Sky script pop_offset failed",
                            status_size);
                return false;
            }
            skip = module_word(module, pc++);
            if (!compact_offset_to_word(skip, &skip) ||
                !compact_set_word(compact, cpt_size, skip, (uint16_t)a)) {
                rb->strlcpy(status, "Sky script pop_offset failed",
                            status_size);
                return false;
            }
            break;
        case 12:
            if (!stack_pop(stack, &stack_pos, &a) ||
                !stack_pop(stack, &stack_pos, &b) ||
                !stack_push(stack, &stack_pos, a < b ? 1 : 0)) {
                rb->strlcpy(status, "Sky script more_than failed",
                            status_size);
                return false;
            }
            break;
        case 13:
        case 19:
            rb->snprintf(status, status_size,
                         "Sky script: %04x %s exit after %lu ops",
                         (unsigned)info->first_logic_id,
                         name ? name : "",
                         (unsigned long)steps);
            return true;
        case 14:
            if (pc >= module_words ||
                !stack_pop(stack, &stack_pos, &a)) {
                rb->strlcpy(status, "Sky script switch failed",
                            status_size);
                return false;
            }
            b = module_word(module, pc++);
            while (b) {
                if (pc + 1 >= module_words) {
                    rb->strlcpy(status, "Sky script switch table failed",
                                status_size);
                    return false;
                }
                if (a == module_word(module, pc)) {
                    pc += module_word(module, pc + 1) / 2;
                    pc++;
                    break;
                }
                pc += 2;
                b--;
            }
            if (!b) {
                if (pc >= module_words) {
                    rb->strlcpy(status, "Sky script switch default failed",
                                status_size);
                    return false;
                }
                pc += module_word(module, pc) / 2;
            }
            break;
        case 17:
            if (!stack_pop(stack, &stack_pos, &a) ||
                !stack_pop(stack, &stack_pos, &b) ||
                !stack_push(stack, &stack_pos, a == b ? 1 : 0)) {
                rb->strlcpy(status, "Sky script is_equal failed",
                            status_size);
                return false;
            }
            break;
        case 18:
            if (pc >= module_words ||
                !stack_pop(stack, &stack_pos, &a)) {
                rb->strlcpy(status, "Sky script skip_nz failed",
                            status_size);
                return false;
            }
            skip = module_word(module, pc++);
            if (a)
                pc += skip / 2;
            break;
        case 20:
            pc = module_word(module, script_index);
            break;
        default:
            rb->snprintf(status, status_size,
                         "Sky script: %04x %s pc %lu op %u",
                         (unsigned)info->first_logic_id,
                         name ? name : "",
                         (unsigned long)(pc - 1),
                         (unsigned)opcode);
            return true;
        }
    }

    opcode = module_word(module, pc);
    rb->snprintf(status, status_size,
                 "Sky script: %04x %s %u:%u pc %lu guard op %u",
                 (unsigned)info->first_logic_id,
                 name ? name : "",
                 (unsigned)module_no,
                 (unsigned)script_index,
                 (unsigned long)pc,
                 (unsigned)opcode);
    return true;
}

static bool runtime_run_compact_script(uint16_t id,
                                       uint16_t script_no,
                                       char *status,
                                       size_t status_size)
{
    uint16_t *compact;
    uint16_t compact_size;
    uint16_t compact_type;
    uint16_t mode;
    uint16_t script_index;
    uint16_t offset_index;
    uint16_t saved_script;
    uint16_t saved_offset;
    uint16_t saved_logic;
    uint16_t after_logic;
    const char *name = NULL;
    struct scummvm_sky_runtime_info one;
    bool ok;

    compact = scummvm_sky_cpt_fetch_mutable(id, &compact_size,
                                            &compact_type, &name);
    if (!compact) {
        rb->snprintf(status, status_size, "Sky compact %u missing",
                     (unsigned)id);
        return false;
    }

    mode = compact_word(compact, compact_size, SKY_COMPACT_MODE);
    if (!compact_mode_slots(mode, &script_index, &offset_index)) {
        rb->snprintf(status, status_size,
                     "Sky mini-bump unsupported mode %u",
                     (unsigned)mode);
        (void)compact_type;
        return false;
    }

    saved_script = compact_word(compact, compact_size, script_index);
    saved_offset = compact_word(compact, compact_size, offset_index);
    saved_logic = compact_word(compact, compact_size,
                               SKY_COMPACT_LOGIC_FIELD);

    compact_set_word(compact, compact_size, script_index, script_no);
    compact_set_word(compact, compact_size, offset_index, 0);
    compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                     SKY_LOGIC_SCRIPT);

    rb->memset(&one, 0, sizeof(one));
    one.first_logic_id = id;
    one.first_logic_status = compact_word(compact, compact_size,
                                          SKY_COMPACT_STATUS);
    if (name)
        rb->strlcpy(one.first_logic_name, name,
                    sizeof(one.first_logic_name));

    ok = decode_first_active_script(&one, status, status_size);

    compact_set_word(compact, compact_size, script_index, saved_script);
    compact_set_word(compact, compact_size, offset_index, saved_offset);
    after_logic = compact_word(compact, compact_size,
                               SKY_COMPACT_LOGIC_FIELD);
    if (after_logic == SKY_LOGIC_SCRIPT)
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                         saved_logic);

    (void)compact_type;
    return ok;
}

static bool runtime_step_compact(uint16_t id,
                                 char *status,
                                 size_t status_size)
{
    uint16_t *compact;
    uint16_t compact_size;
    uint16_t compact_type;
    uint16_t logic;
    const char *name = NULL;

    compact = scummvm_sky_cpt_fetch_mutable(id, &compact_size,
                                            &compact_type, &name);
    if (!compact) {
        rb->snprintf(status, status_size, "Sky compact %u missing",
                     (unsigned)id);
        return false;
    }

    logic = compact_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD);
    switch (logic) {
    case SKY_LOGIC_NOP:
        rb->snprintf(status, status_size, "Sky nop %u", (unsigned)id);
        (void)compact_type;
        return true;
    case SKY_LOGIC_SCRIPT:
    {
        struct scummvm_sky_runtime_info one;

        rb->memset(&one, 0, sizeof(one));
        one.first_logic_id = id;
        one.first_logic_status = compact_word(compact, compact_size,
                                              SKY_COMPACT_STATUS);
        if (name)
            rb->strlcpy(one.first_logic_name, name,
                        sizeof(one.first_logic_name));
        (void)compact_type;
        return decode_first_active_script(&one, status, status_size);
    }
    case SKY_LOGIC_AR:
    {
        uint16_t route_result = 1;

        if (!runtime_build_route(compact, compact_size, &route_result)) {
            rb->snprintf(status, status_size, "Sky route build failed %u",
                         (unsigned)id);
            (void)compact_type;
            return true;
        }
        compact_set_word(compact, compact_size, SKY_COMPACT_DOWN_FLAG,
                         route_result);
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                         SKY_LOGIC_SCRIPT);
        (void)compact_type;
        return runtime_step_compact(id, status, status_size);
    }
    case SKY_LOGIC_AR_ANIM:
        if (runtime_step_route(compact, compact_size, id)) {
            rb->snprintf(status, status_size, "Sky route anim step %u",
                         (unsigned)id);
            (void)compact_type;
            return true;
        }
        (void)compact_type;
        return runtime_step_compact(id, status, status_size);
    case SKY_LOGIC_ALT:
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                         SKY_LOGIC_SCRIPT);
        compact_set_word(compact, compact_size, SKY_COMPACT_ACTION_SUB,
                         compact_word(compact, compact_size,
                                      SKY_COMPACT_ALT));
        compact_set_word(compact, compact_size, SKY_COMPACT_ACTION_SUB_OFF,
                         0);
        (void)compact_type;
        return runtime_step_compact(id, status, status_size);
    case SKY_LOGIC_CHOOSE:
        if (!script_vars[SKY_VAR_THE_CHOSEN_ONE]) {
            rb->snprintf(status, status_size, "Sky chooser wait %u",
                         (unsigned)id);
            (void)compact_type;
            return true;
        }
        runtime_overlay_clear();
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                         SKY_LOGIC_SCRIPT);
        (void)compact_type;
        return runtime_step_compact(id, status, status_size);
    case SKY_LOGIC_STOPPED:
        rb->snprintf(status, status_size, "Sky stopped %u", (unsigned)id);
        (void)compact_type;
        return true;
    case SKY_LOGIC_PAUSE:
    {
        uint16_t flag = compact_word(compact, compact_size,
                                     SKY_COMPACT_FLAG);
        if (flag > 1) {
            compact_set_word(compact, compact_size, SKY_COMPACT_FLAG,
                             flag - 1);
            rb->snprintf(status, status_size, "Sky pause %u",
                         (unsigned)(flag - 1));
            (void)compact_type;
            return true;
        }
        compact_set_word(compact, compact_size, SKY_COMPACT_FLAG, 0);
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                         SKY_LOGIC_SCRIPT);
        (void)compact_type;
        return runtime_step_compact(id, status, status_size);
    }
    case SKY_LOGIC_WAIT_SYNC:
        if (!compact_word(compact, compact_size, SKY_COMPACT_SYNC)) {
            rb->snprintf(status, status_size, "Sky wait sync %u",
                         (unsigned)id);
            (void)compact_type;
            return true;
        }
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                         SKY_LOGIC_SCRIPT);
        (void)compact_type;
        return runtime_step_compact(id, status, status_size);
    case SKY_LOGIC_MOD_ANIMATE:
        if (!runtime_step_mod_anim(compact, compact_size, status,
                                   status_size)) {
            (void)compact_type;
            return false;
        }
        if (compact_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD) ==
            SKY_LOGIC_SCRIPT) {
            (void)compact_type;
            return runtime_step_compact(id, status, status_size);
        }
        rb->snprintf(status, status_size, "Sky mod anim step %u",
                     (unsigned)id);
        (void)compact_type;
        return true;
    case SKY_LOGIC_AR_TURNING:
    case SKY_LOGIC_TURNING:
    {
        const uint16_t *turn;
        uint16_t turn_size;
        uint16_t turn_type;
        uint16_t turn_id = compact_word(compact, compact_size,
                                        SKY_COMPACT_TURN_PROG_ID);
        uint16_t turn_pos = compact_word(compact, compact_size,
                                         SKY_COMPACT_TURN_PROG_POS);

        turn = scummvm_sky_cpt_fetch(turn_id, &turn_size, &turn_type, NULL);
        if (!turn) {
            rb->snprintf(status, status_size,
                         "Sky turn program %u missing",
                         (unsigned)turn_id);
            (void)compact_type;
            return false;
        }

        if (turn_pos < turn_size && turn[turn_pos]) {
            compact_set_word(compact, compact_size, SKY_COMPACT_FRAME,
                             turn[turn_pos]);
            compact_set_word(compact, compact_size,
                             SKY_COMPACT_TURN_PROG_POS, turn_pos + 1);
            rb->snprintf(status, status_size, "Sky turn step %u",
                         (unsigned)id);
            (void)turn_type;
            (void)compact_type;
            return true;
        }

        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                         logic == SKY_LOGIC_AR_TURNING ?
                         SKY_LOGIC_AR_ANIM : SKY_LOGIC_SCRIPT);
        (void)turn_type;
        (void)compact_type;
        return logic == SKY_LOGIC_AR_TURNING ?
            true : runtime_step_compact(id, status, status_size);
    }
    case SKY_LOGIC_SIMPLE_MOD:
    case SKY_LOGIC_FRAMES:
        if (!runtime_step_simple_anim(compact, compact_size, status,
                                      status_size)) {
            (void)compact_type;
            return false;
        }
        if (compact_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD) ==
            SKY_LOGIC_SCRIPT) {
            (void)compact_type;
            return runtime_step_compact(id, status, status_size);
        }
        rb->snprintf(status, status_size, "Sky anim step %u",
                     (unsigned)id);
        (void)compact_type;
        return true;
    default:
        rb->snprintf(status, status_size,
                     "Sky runtime: first %04x logic %u %s",
                     (unsigned)id,
                     (unsigned)logic,
                     name ? name : "");
        (void)compact_type;
        return true;
    }
}

static bool runtime_draw_screen(uint16_t palette_id,
                                char *status,
                                size_t status_size)
{
    uint32_t screen_id = script_vars[SKY_VAR_SCREEN];

    if (!runtime_target) {
        rb->strlcpy(status, "Sky runtime has no target", status_size);
        return false;
    }

    if (screen_id == 0) {
        rb->strlcpy(status, "Sky draw needs SCREEN set", status_size);
        return false;
    }

    if (runtime_screen.file_nr != screen_id) {
        scummvm_sky_loader_release_resource(&runtime_screen);
        if (!scummvm_sky_loader_load_resource(runtime_target,
                                              (uint16_t)screen_id,
                                              &runtime_screen,
                                              status,
                                              status_size))
            return false;

        if (runtime_screen.size >= SCUMMVM_SURFACE_W * SCUMMVM_SURFACE_H) {
            rb->memset(runtime_screen.data +
                       SCUMMVM_SURFACE_W * 192,
                       0,
                       SCUMMVM_SURFACE_W * (SCUMMVM_SURFACE_H - 192));
        }
    }

    runtime_palette_id = palette_id;
    rb->snprintf(status, status_size,
                 "Sky draw screen %lu palette %u",
                 (unsigned long)screen_id,
                 (unsigned)palette_id);
    return true;
}

static bool runtime_cache_files(char *status, size_t status_size)
{
    uint32_t i;
    uint32_t loaded = 0;

    for (i = 0; i < ARRAYLEN(cache_build_list) && cache_build_list[i]; i++) {
        uint16_t file_nr = cache_build_list[i] & 0x7fff;
        uint16_t slot = file_nr & 0x07ff;

        if (slot >= ARRAYLEN(cached_items) || slot == 0x07ff)
            continue;

        if (cached_items[slot].file_nr == file_nr &&
            cached_items[slot].data)
            continue;

        scummvm_sky_loader_release_resource(&cached_items[slot]);
        if (!scummvm_sky_loader_load_resource(runtime_target,
                                              file_nr,
                                              &cached_items[slot],
                                              status,
                                              status_size))
            return false;
        loaded++;
    }

    rb->snprintf(status, status_size, "Sky cache loaded %lu files",
                 (unsigned long)loaded);
    cache_build_list[0] = 0;
    return true;
}

static bool runtime_cache_one(uint16_t file_nr,
                              char *status,
                              size_t status_size)
{
    uint16_t slot = file_nr & 0x07ff;

    if (slot >= ARRAYLEN(cached_items) || slot == 0x07ff)
        return true;

    if (cached_items[slot].file_nr == file_nr && cached_items[slot].data)
        return true;

    scummvm_sky_loader_release_resource(&cached_items[slot]);
    if (!scummvm_sky_loader_load_resource(runtime_target, file_nr,
                                          &cached_items[slot],
                                          status,
                                          status_size))
        return false;

    rb->snprintf(status, status_size, "Sky mini-load %u",
                 (unsigned)file_nr);
    return true;
}

static const struct scummvm_sky_resource *runtime_get_resource(uint16_t file_nr)
{
    uint16_t slot = file_nr & 0x07ff;
    char status[64];

    if (file_nr >= SKY_FIRST_TEXT_BUFFER &&
        file_nr <= SKY_LAST_TEXT_BUFFER) {
        uint16_t text_slot = (uint16_t)(file_nr - SKY_FIRST_TEXT_BUFFER);

        if (runtime_text_sprites[text_slot].data)
            return &runtime_text_sprites[text_slot];
        return NULL;
    }

    if (slot >= ARRAYLEN(cached_items) || slot == 0x07ff)
        return NULL;

    if (cached_items[slot].file_nr == file_nr && cached_items[slot].data)
        return &cached_items[slot];

    scummvm_sky_loader_release_resource(&cached_items[slot]);
    if (!runtime_target ||
        !scummvm_sky_loader_load_resource(runtime_target, file_nr,
                                          &cached_items[slot],
                                          status,
                                          sizeof(status)))
        return NULL;

    return &cached_items[slot];
}

static void runtime_clear_text_id(uint16_t id)
{
    uint16_t compact_size;
    uint16_t compact_type;
    uint16_t *compact;

    if (id < SKY_FIRST_TEXT_COMPACT || id > SKY_LAST_TEXT_COMPACT)
        return;

    compact = scummvm_sky_cpt_fetch_mutable(id, &compact_size,
                                            &compact_type, NULL);
    if (compact)
        compact_set_word(compact, compact_size, SKY_COMPACT_STATUS, 0);
    scummvm_sky_loader_release_resource(
        &runtime_text_sprites[id - SKY_FIRST_TEXT_COMPACT]);
    (void)compact_type;
}

static void runtime_clear_text_mouse(bool only_mouse)
{
    uint16_t id;

    runtime_overlay_clear();
    for (id = SKY_FIRST_TEXT_COMPACT; id < SKY_FIRST_TEXT_COMPACT + 10;
         id++) {
        uint16_t compact_size;
        uint16_t compact_type;
        uint16_t *compact = scummvm_sky_cpt_fetch_mutable(id,
                                                          &compact_size,
                                                          &compact_type,
                                                          NULL);

        if (compact &&
            (!only_mouse ||
             (compact_word(compact, compact_size, SKY_COMPACT_STATUS) &
              SKY_ST_MOUSE))) {
            compact_set_word(compact, compact_size, SKY_COMPACT_STATUS, 0);
            scummvm_sky_loader_release_resource(
                &runtime_text_sprites[id - SKY_FIRST_TEXT_COMPACT]);
        }
        (void)compact_type;
    }
}

static bool runtime_alloc_text(uint32_t text_num,
                               uint16_t width,
                               uint8_t color,
                               bool center,
                               uint16_t logic,
                               uint16_t *text_id,
                               uint16_t *text_width)
{
    uint16_t id;

    for (id = SKY_FIRST_TEXT_COMPACT; id <= SKY_LAST_TEXT_COMPACT; id++) {
        uint16_t compact_size;
        uint16_t compact_type;
        uint16_t buffer_id = (uint16_t)(SKY_FIRST_TEXT_BUFFER +
                                        id - SKY_FIRST_TEXT_COMPACT);
        uint16_t *compact = scummvm_sky_cpt_fetch_mutable(id,
                                                          &compact_size,
                                                          &compact_type,
                                                          NULL);

        if (!compact ||
            compact_word(compact, compact_size, SKY_COMPACT_STATUS)) {
            (void)compact_type;
            continue;
        }

        if (!scummvm_sky_text_render(
                text_num,
                width,
                color,
                center,
                &runtime_text_sprites[id - SKY_FIRST_TEXT_COMPACT],
                text_width)) {
            (void)compact_type;
            return false;
        }

        runtime_text_sprites[id - SKY_FIRST_TEXT_COMPACT].file_nr =
            buffer_id;
        compact_set_word(compact, compact_size, SKY_COMPACT_FLAG,
                         buffer_id);
        compact_set_word(compact, compact_size, SKY_COMPACT_FRAME,
                         (uint16_t)(buffer_id << 6));
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                         logic);
        compact_set_word(compact, compact_size, SKY_COMPACT_STATUS,
                         SKY_ST_LOGIC | SKY_ST_FOREGROUND |
                         SKY_ST_RECREATE);
        compact_set_word(compact, compact_size, SKY_COMPACT_SCREEN,
                         (uint16_t)script_vars[SKY_VAR_SCREEN]);
        if (text_id)
            *text_id = id;
        (void)compact_type;
        return true;
    }

    return false;
}

static bool runtime_start_speech_text(uint16_t target_id, uint32_t text_num)
{
    uint16_t target_size;
    uint16_t target_type;
    uint16_t *target;
    uint16_t text_id;
    uint16_t text_size;
    uint16_t text_type;
    uint16_t *text_compact;
    const struct scummvm_sky_resource *text_resource;
    const struct scummvm_sky_resource *target_resource;
    uint16_t target_frame;
    uint16_t color;
    int x_pos;
    int y_pos;

    target = scummvm_sky_cpt_fetch_mutable(target_id, &target_size,
                                           &target_type, NULL);
    if (!target)
        return false;

    runtime_audio_start_speech(text_num);

    color = compact_word(target, target_size, SKY_COMPACT_SP_COLOR);
    if (color == 0)
        color = 241;

    if (!runtime_alloc_text(text_num,
                            SKY_FIXED_TEXT_WIDTH,
                            (uint8_t)color,
                            true,
                            SKY_LOGIC_NOP,
                            &text_id,
                            NULL)) {
        (void)target_type;
        return false;
    }

    text_compact = scummvm_sky_cpt_fetch_mutable(text_id, &text_size,
                                                 &text_type, NULL);
    text_resource = runtime_get_resource(
        (uint16_t)(SKY_FIRST_TEXT_BUFFER +
                   text_id - SKY_FIRST_TEXT_COMPACT));
    target_frame = compact_word(target, target_size, SKY_COMPACT_FRAME);
    target_resource = runtime_get_resource(target_frame >> 6);
    if (!text_compact || !text_resource || text_resource->size < 22 ||
        !target_resource || target_resource->size < 22) {
        runtime_clear_text_id(text_id);
        (void)target_type;
        (void)text_type;
        return false;
    }

    compact_set_word(target, target_size, SKY_COMPACT_SP_TEXT_ID, text_id);
    compact_set_word(target, target_size, SKY_COMPACT_SP_TIME, 20);
    compact_set_word(text_compact, text_size, SKY_COMPACT_SCREEN,
                     compact_word(target, target_size, SKY_COMPACT_SCREEN));

    if (compact_word(target, target_size, SKY_COMPACT_SCREEN) !=
        (uint16_t)script_vars[SKY_VAR_SCREEN]) {
        compact_set_word(target, target_size, SKY_COMPACT_SP_TEXT_ID, 0);
        compact_set_word(text_compact, text_size, SKY_COMPACT_STATUS, 0);
        (void)target_type;
        (void)text_type;
        return true;
    }

    x_pos = (int)compact_word(target, target_size, SKY_COMPACT_X) +
            read_sle16(target_resource->data + 16) +
            (read_le16(target_resource->data + 6) >> 1) -
            (SKY_FIXED_TEXT_WIDTH / 2);
    if (x_pos < SKY_TOP_LEFT_X)
        x_pos = SKY_TOP_LEFT_X;
    if (x_pos + SKY_FIXED_TEXT_WIDTH >
        SKY_TOP_LEFT_X + SCUMMVM_SURFACE_W)
        x_pos = SKY_TOP_LEFT_X + SCUMMVM_SURFACE_W -
                SKY_FIXED_TEXT_WIDTH;

    y_pos = (int)compact_word(target, target_size, SKY_COMPACT_Y) +
            read_sle16(target_resource->data + 18) - 6 -
            read_le16(text_resource->data + 8);
    if (y_pos < SKY_TOP_LEFT_Y)
        y_pos = SKY_TOP_LEFT_Y;

    compact_set_word(text_compact, text_size, SKY_COMPACT_X,
                     (uint16_t)x_pos);
    compact_set_word(text_compact, text_size, SKY_COMPACT_Y,
                     (uint16_t)y_pos);
    (void)target_type;
    (void)text_type;
    return true;
}

static void runtime_music_clear_stream_state(void)
{
    runtime_music_buffer_pos = 0;
    runtime_music_buffer_size = 0;
    runtime_music_step = 0;
    runtime_music_phase = 0;
    runtime_music_last_left = 0;
    runtime_music_last_right = 0;
    runtime_music_have_sample = false;
}

static bool runtime_music_read_bytes(uint8_t *dst, uint16_t size)
{
    uint16_t done = 0;

    while (done < size) {
        if (runtime_music_buffer_pos >= runtime_music_buffer_size) {
            uint32_t remaining;
            ssize_t got;

            if (runtime_music_data_pos >= runtime_music_data_size) {
                if (!runtime_music_loop)
                    return false;
                runtime_music_data_pos = 0;
                if (rb->lseek(runtime_music_fd,
                              runtime_music_data_start,
                              SEEK_SET) < 0)
                    return false;
            }

            remaining = runtime_music_data_size - runtime_music_data_pos;
            if (remaining > sizeof(runtime_music_buffer))
                remaining = sizeof(runtime_music_buffer);
            got = rb->read(runtime_music_fd, runtime_music_buffer, remaining);
            if (got <= 0)
                return false;
            runtime_music_buffer_pos = 0;
            runtime_music_buffer_size = (uint32_t)got;
            runtime_music_data_pos += (uint32_t)got;
        }

        dst[done++] = runtime_music_buffer[runtime_music_buffer_pos++];
    }

    return true;
}

static bool runtime_music_read_source_frame(int16_t *left, int16_t *right)
{
    uint8_t sample[4];
    int32_t values[2] = { 0, 0 };
    uint16_t ch;
    uint16_t bytes_per_sample;

    if (runtime_music_fd < 0 ||
        runtime_music_channels == 0 || runtime_music_channels > 2 ||
        (runtime_music_bits != 8 && runtime_music_bits != 16))
        return false;

    bytes_per_sample = runtime_music_bits / 8;
    for (ch = 0; ch < runtime_music_channels; ch++) {
        if (!runtime_music_read_bytes(sample, bytes_per_sample))
            return false;
        if (runtime_music_bits == 8)
            values[ch] = ((int)sample[0] - 128) << 8;
        else
            values[ch] = (int16_t)read_le16(sample);
    }

    if (runtime_music_channels == 1) {
        *left = (int16_t)values[0];
        *right = (int16_t)values[0];
    } else {
        *left = (int16_t)values[0];
        *right = (int16_t)values[1];
    }

    return true;
}

static bool runtime_music_fill_frame(int16_t *left, int16_t *right)
{
    uint32_t advance;

    if (runtime_music_fd < 0 || runtime_music_step == 0)
        return false;

    if (!runtime_music_have_sample) {
        if (!runtime_music_read_source_frame(&runtime_music_last_left,
                                             &runtime_music_last_right))
            return false;
        runtime_music_have_sample = true;
    }

    *left = runtime_music_last_left;
    *right = runtime_music_last_right;

    runtime_music_phase += runtime_music_step;
    advance = runtime_music_phase >> 16;
    runtime_music_phase &= 0xffff;
    while (advance-- > 0) {
        if (!runtime_music_read_source_frame(&runtime_music_last_left,
                                             &runtime_music_last_right)) {
            runtime_music_have_sample = false;
            return true;
        }
    }

    return true;
}

static void runtime_audio_get_more(const void **start, size_t *size)
{
    uint32_t frame;
    bool produced = false;

    for (frame = 0; frame < SKY_AUDIO_MIX_FRAMES; frame++) {
        int32_t left = 0;
        int32_t right = 0;
        int16_t music_left;
        int16_t music_right;

        if (runtime_music_fill_frame(&music_left, &music_right)) {
            left = music_left / 2;
            right = music_right / 2;
            produced = true;
        }

        if (runtime_audio_pos + sizeof(int16_t) * 2 <= runtime_audio_size) {
            const int16_t *sfx = (const int16_t *)
                ((const uint8_t *)runtime_audio_pcm + runtime_audio_pos);

            left += sfx[0];
            right += sfx[1];
            runtime_audio_pos += sizeof(int16_t) * 2;
            produced = true;
        }

        if (left > 32767)
            left = 32767;
        else if (left < -32768)
            left = -32768;
        if (right > 32767)
            right = 32767;
        else if (right < -32768)
            right = -32768;
        runtime_audio_mix[frame * 2] = (int16_t)left;
        runtime_audio_mix[frame * 2 + 1] = (int16_t)right;
    }

    if (!produced) {
        runtime_audio_channel_started = false;
        *start = NULL;
        *size = 0;
        return;
    }

    *start = runtime_audio_mix;
    *size = sizeof(runtime_audio_mix);
}

static void runtime_audio_stop(void)
{
    runtime_audio_pos = 0;
    runtime_audio_size = 0;
}

static void runtime_music_stop(void)
{
    if (runtime_music_fd >= 0)
        rb->close(runtime_music_fd);
    runtime_music_fd = -1;
    runtime_current_music = 0;
    runtime_music_data_start = 0;
    runtime_music_data_size = 0;
    runtime_music_data_pos = 0;
    runtime_music_rate = 0;
    runtime_music_channels = 0;
    runtime_music_bits = 0;
    runtime_music_clear_stream_state();
    runtime_music_loop = false;
    if (runtime_audio_active && runtime_audio_size == 0) {
        rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
        runtime_audio_channel_started = false;
    }
}

static bool runtime_audio_begin(void)
{
    if (runtime_audio_active)
        return true;

    runtime_audio_old_frequency = rb->mixer_get_frequency();
    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->mixer_set_frequency(SKY_AUDIO_RATE);
    rb->pcmbuf_fade(false, true);
    runtime_audio_active = true;
    return true;
}

static void runtime_audio_play_buffer(size_t bytes)
{
    if (!runtime_audio_begin() || bytes == 0)
        return;

    runtime_audio_pos = 0;
    runtime_audio_size = bytes;
    if (!runtime_audio_channel_started ||
        rb->mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK) ==
        CHANNEL_STOPPED) {
        runtime_audio_channel_started = true;
        rb->mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK,
                                    runtime_audio_get_more,
                                    NULL,
                                    0);
    }
}

static bool runtime_music_should_loop(uint16_t section, uint16_t song)
{
    if ((section == 0 && song == 1) ||
        (section == 1 && song == 1) || (section == 1 && song == 4) ||
        (section == 2 && song == 1) || (section == 2 && song == 4) ||
        (section == 4 && song == 2) || (section == 4 && song == 3) ||
        (section == 4 && song == 5) || (section == 4 && song == 6) ||
        (section == 4 && song == 11) ||
        (section == 5 && song == 1) || (section == 5 && song == 3) ||
        (section == 5 && song == 4))
        return false;

    return true;
}

static void runtime_music_normalize_track(uint16_t *section, uint16_t *song)
{
    if ((*section == 2 && *song == 1) || (*section == 5 && *song == 1)) {
        *section = 1;
        *song = 1;
    } else if ((*section == 2 && *song == 4) ||
               (*section == 5 && *song == 4)) {
        *section = 1;
        *song = 4;
    } else if (*section == 5 && *song == 6) {
        *section = 4;
        *song = 4;
    }
}

static bool runtime_music_parse_wav(int fd,
                                    uint32_t *data_start,
                                    uint32_t *data_size,
                                    uint16_t *rate,
                                    uint16_t *channels,
                                    uint16_t *bits)
{
    uint8_t header[12];
    uint32_t pos = 12;
    bool have_fmt = false;
    bool have_data = false;
    long file_size = rb->filesize(fd);

    if (file_size < 44 ||
        !runtime_read_at(fd, 0, header, sizeof(header)) ||
        rb->memcmp(header, "RIFF", 4) ||
        rb->memcmp(header + 8, "WAVE", 4))
        return false;

    while (pos + 8 <= (uint32_t)file_size) {
        uint8_t chunk[24];
        uint32_t chunk_size;

        if (!runtime_read_at(fd, pos, chunk, 8))
            return false;
        chunk_size = read_le32(chunk + 4);
        pos += 8;
        if (pos + chunk_size > (uint32_t)file_size)
            return false;

        if (!rb->memcmp(chunk, "fmt ", 4)) {
            uint16_t format;

            if (chunk_size < 16 ||
                !runtime_read_at(fd, pos, chunk, 16))
                return false;
            format = read_le16(chunk);
            if (format != 1)
                return false;
            *channels = read_le16(chunk + 2);
            *rate = (uint16_t)read_le32(chunk + 4);
            *bits = read_le16(chunk + 14);
            if ((*channels != 1 && *channels != 2) ||
                (*bits != 8 && *bits != 16) ||
                *rate < 8000 || *rate > 48000)
                return false;
            have_fmt = true;
        } else if (!rb->memcmp(chunk, "data", 4)) {
            *data_start = pos;
            *data_size = chunk_size;
            have_data = true;
        }

        pos += chunk_size + (chunk_size & 1);
        if (have_fmt && have_data)
            return true;
    }

    return false;
}

static bool runtime_music_start(uint16_t song)
{
    char name[32];
    char path[MAX_PATH];
    uint16_t section = (uint16_t)script_vars[SKY_VAR_CUR_SECTION];
    int fd;
    uint32_t data_start = 0;
    uint32_t data_size = 0;
    uint16_t rate = 0;
    uint16_t channels = 0;
    uint16_t bits = 0;

    runtime_music_stop();
    runtime_current_music = song;
    if (song == 0 || !runtime_target)
        return true;

    runtime_music_normalize_track(&section, &song);
    rb->snprintf(name, sizeof(name), "music_%u%02u.wav",
                 (unsigned)section,
                 (unsigned)song);
    if (!scummvm_make_path(path, sizeof(path), runtime_target->path, name))
        return false;

    fd = rb->open(path, O_RDONLY);
    if (fd < 0) {
        rb->snprintf(name, sizeof(name), "music_%u%02u.WAV",
                     (unsigned)section,
                     (unsigned)song);
        if (!scummvm_make_path(path, sizeof(path), runtime_target->path, name))
            return false;
        fd = rb->open(path, O_RDONLY);
        if (fd < 0)
            return false;
    }

    if (!runtime_music_parse_wav(fd, &data_start, &data_size, &rate,
                                 &channels, &bits)) {
        rb->close(fd);
        return false;
    }

    runtime_music_fd = fd;
    runtime_music_data_start = data_start;
    runtime_music_data_size = data_size;
    runtime_music_data_pos = 0;
    runtime_music_rate = rate;
    runtime_music_channels = channels;
    runtime_music_bits = bits;
    runtime_music_loop = runtime_music_should_loop(section, song);
    runtime_music_clear_stream_state();
    runtime_music_step = ((uint32_t)rate << 16) / SKY_AUDIO_RATE;
    if (rb->lseek(runtime_music_fd, runtime_music_data_start, SEEK_SET) < 0) {
        runtime_music_stop();
        return false;
    }

    if (!runtime_audio_begin()) {
        runtime_music_stop();
        return false;
    }

    if (!runtime_audio_channel_started ||
        rb->mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK) ==
        CHANNEL_STOPPED) {
        runtime_audio_channel_started = true;
        rb->mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK,
                                    runtime_audio_get_more,
                                    NULL,
                                    0);
    }

    return true;
}

static bool runtime_audio_load_section(uint16_t section)
{
    char status[96];
    uint16_t file_nr = (uint16_t)(SKY_SOUND_FILE_BASE + section * 4);

    if (runtime_sound_section.file_nr == file_nr &&
        runtime_sound_section.data)
        return true;

    runtime_audio_stop();
    scummvm_sky_loader_release_resource(&runtime_sound_section);
    if (!runtime_target ||
        !scummvm_sky_loader_load_resource(runtime_target,
                                          file_nr,
                                          &runtime_sound_section,
                                          status,
                                          sizeof(status)))
        return false;

    return true;
}

static bool runtime_audio_tables(uint16_t *sounds_total,
                                 uint16_t *rate_offset,
                                 uint16_t *sfx_base)
{
    static const uint16_t candidates[] = { 0x7e, 0x78, 0x7c };
    uint16_t i;

    if (!runtime_sound_section.data)
        return false;

    for (i = 0; i < ARRAYLEN(candidates); i++) {
        uint16_t asm_ofs = candidates[i];

        if ((uint32_t)asm_ofs + 0x33 >= runtime_sound_section.size)
            continue;
        if (runtime_sound_section.data[asm_ofs] == 0x3c &&
            runtime_sound_section.data[asm_ofs + 0x27] == 0x8d &&
            runtime_sound_section.data[asm_ofs + 0x28] == 0x1e &&
            runtime_sound_section.data[asm_ofs + 0x2f] == 0x8d &&
            runtime_sound_section.data[asm_ofs + 0x30] == 0x36) {
            *sounds_total = runtime_sound_section.data[asm_ofs + 1];
            *rate_offset =
                read_le16(runtime_sound_section.data + asm_ofs + 0x29);
            *sfx_base =
                read_le16(runtime_sound_section.data + asm_ofs + 0x31);
            return true;
        }
    }

    return false;
}

static size_t runtime_audio_from_unsigned_pcm(const uint8_t *src,
                                              uint32_t src_size,
                                              uint16_t src_rate,
                                              uint16_t volume)
{
    uint32_t out_frames;
    uint32_t frame;
    int amp = (volume & 0x7f) << 8;

    if (src_rate == 0 || src_size == 0)
        return 0;
    if (src_rate > 11025)
        src_rate = 11025;

    out_frames = (src_size * SKY_AUDIO_RATE) / src_rate;
    if (out_frames > SKY_AUDIO_FRAMES)
        out_frames = SKY_AUDIO_FRAMES;

    for (frame = 0; frame < out_frames; frame++) {
        uint32_t src_index = (frame * src_rate) / SKY_AUDIO_RATE;
        int sample = ((int)src[src_index] - 128) * amp / 128;

        if (sample > 32767)
            sample = 32767;
        else if (sample < -32768)
            sample = -32768;
        runtime_audio_pcm[frame * 2] = (int16_t)sample;
        runtime_audio_pcm[frame * 2 + 1] = (int16_t)sample;
    }

    return out_frames * SKY_AUDIO_CHANNELS * sizeof(runtime_audio_pcm[0]);
}

static bool runtime_audio_play_raw(uint16_t raw_sound, uint16_t volume)
{
    uint16_t sounds_total;
    uint16_t rate_offset;
    uint16_t sfx_base;
    size_t bytes = 0;

    if (!runtime_audio_load_section((uint16_t)script_vars[SKY_VAR_CUR_SECTION]))
        return false;

    if (runtime_audio_tables(&sounds_total, &rate_offset, &sfx_base) &&
        raw_sound <= sounds_total &&
        (uint32_t)rate_offset + (uint32_t)raw_sound * 4 + 2 <=
            runtime_sound_section.size &&
        (uint32_t)sfx_base + (uint32_t)raw_sound * 8 + 8 <=
            runtime_sound_section.size) {
        uint16_t sample_rate = read_be16(runtime_sound_section.data +
                                         rate_offset + raw_sound * 4);
        uint32_t data_ofs = (uint32_t)read_be16(
                                runtime_sound_section.data +
                                sfx_base + raw_sound * 8) << 4;
        uint32_t data_size = read_be16(runtime_sound_section.data +
                                       sfx_base + raw_sound * 8 + 2);

        data_ofs += sfx_base;
        if (data_ofs + data_size <= runtime_sound_section.size)
            bytes = runtime_audio_from_unsigned_pcm(
                runtime_sound_section.data + data_ofs,
                data_size,
                sample_rate,
                volume ? volume : 64);
    }

    if (bytes == 0)
        return false;

    runtime_audio_play_buffer(bytes);
    return true;
}

static void runtime_audio_queue_raw(uint16_t raw_sound,
                                    uint16_t volume,
                                    uint8_t frames)
{
    uint16_t i;

    if (raw_sound > 0xff)
        return;

    for (i = 0; i < ARRAYLEN(runtime_audio_delay); i++) {
        if (runtime_audio_delay[i].frames == 0) {
            runtime_audio_delay[i].frames = frames ? frames : 1;
            runtime_audio_delay[i].raw_sound = (uint8_t)raw_sound;
            runtime_audio_delay[i].volume = (uint8_t)(volume ? volume : 64);
            return;
        }
    }
}

static void runtime_audio_tick_queue(void)
{
    uint16_t i;

    for (i = 0; i < ARRAYLEN(runtime_audio_delay); i++) {
        if (runtime_audio_delay[i].frames == 0)
            continue;
        runtime_audio_delay[i].frames--;
        if (runtime_audio_delay[i].frames == 0) {
            runtime_audio_play_raw(runtime_audio_delay[i].raw_sound,
                                   runtime_audio_delay[i].volume);
            runtime_audio_delay[i].raw_sound = 0;
            runtime_audio_delay[i].volume = 0;
        }
    }
}

static void runtime_audio_start_fx(uint16_t sound, uint16_t volume)
{
    const struct scummvm_sky_sfx_entry *entry;
    uint16_t current_room = (uint16_t)script_vars[SKY_VAR_SCREEN];
    uint16_t room;
    uint16_t mapped_volume = volume ? volume : 64;
    uint16_t i;

    if (sound == 278 && current_room == 25)
        sound = 394;

    if (sound < SCUMMVM_SKY_SFX_BASE ||
        sound >= SCUMMVM_SKY_SFX_BASE + SCUMMVM_SKY_SFX_MAP_COUNT)
        return;

    entry = &scummvm_sky_sfx_map[sound - SCUMMVM_SKY_SFX_BASE];
    if (entry->rooms[0].room != 0xff) {
        bool room_found = false;

        for (i = 0; i < SCUMMVM_SKY_SFX_ROOM_COUNT; i++) {
            room = entry->rooms[i].room;
            if (room == 0xff)
                break;
            if (room == current_room) {
                mapped_volume = (uint16_t)((entry->rooms[i].adlib_volume *
                                            127u) >> 8);
                room_found = true;
                break;
            }
        }
        if (!room_found)
            return;
    } else {
        mapped_volume = (uint16_t)((entry->rooms[0].adlib_volume *
                                    127u) >> 8);
    }

    if (entry->flags & SCUMMVM_SKY_SFXF_START_DELAY) {
        runtime_audio_queue_raw(entry->sound_no,
                                mapped_volume,
                                entry->flags & 0x7f);
        return;
    }

    runtime_audio_play_raw(entry->sound_no, mapped_volume);
}

static bool runtime_audio_start_speech(uint32_t text_num)
{
    struct scummvm_sky_resource speech;
    char status[96];
    uint16_t section = (uint16_t)(text_num >> 12);
    uint16_t speech_file;
    uint32_t data_size;
    size_t bytes;

    if (section >= ARRAYLEN(runtime_speech_convert))
        return false;

    speech_file = (uint16_t)(SKY_SPEECH_FILE_BASE +
                             runtime_speech_convert[section] +
                             (text_num & 0x0fff));
    rb->memset(&speech, 0, sizeof(speech));
    if (!runtime_target ||
        !scummvm_sky_loader_load_resource(runtime_target,
                                          speech_file,
                                          &speech,
                                          status,
                                          sizeof(status)))
        return false;

    if (speech.size <= 22) {
        scummvm_sky_loader_release_resource(&speech);
        return false;
    }

    data_size = read_le16(speech.data + 12);
    if (data_size > 22)
        data_size -= 22;
    else
        data_size = speech.size - 22;
    if (data_size > speech.size - 22)
        data_size = speech.size - 22;

    bytes = runtime_audio_from_unsigned_pcm(speech.data + 22,
                                            data_size,
                                            11025,
                                            96);
    scummvm_sky_loader_release_resource(&speech);
    if (bytes == 0)
        return false;

    runtime_audio_play_buffer(bytes);
    return true;
}

static void runtime_flush_cache(void)
{
    uint32_t i;

    for (i = 0; i < ARRAYLEN(cached_items); i++)
        scummvm_sky_loader_release_resource(&cached_items[i]);
    rb->memset(cache_build_list, 0, sizeof(cache_build_list));
}

static bool runtime_cache_fast(uint16_t list_id,
                               char *status,
                               size_t status_size)
{
    const uint16_t *list;
    uint16_t list_size;
    uint16_t list_type;
    uint32_t i = 0;

    rb->memset(cache_build_list, 0, sizeof(cache_build_list));

    list = scummvm_sky_cpt_fetch(list_id, &list_size, &list_type, NULL);
    if (!list) {
        rb->snprintf(status, status_size,
                     "Sky cache list %u missing", (unsigned)list_id);
        return false;
    }

    while (i < ARRAYLEN(cache_build_list) - 1 && i < list_size) {
        cache_build_list[i] = list[i] & 0x7fff;
        if (list[i] == 0)
            break;
        i++;
    }

    (void)list_type;
    rb->snprintf(status, status_size, "Sky cache fast list %u",
                 (unsigned)list_id);
    return true;
}

static bool runtime_cache_chip(uint16_t list_id,
                               char *status,
                               size_t status_size)
{
    const uint16_t *list;
    uint16_t list_size;
    uint16_t list_type;
    uint32_t dst = 0;
    uint32_t src = 0;

    while (dst < ARRAYLEN(cache_build_list) && cache_build_list[dst])
        dst++;

    list = scummvm_sky_cpt_fetch(list_id, &list_size, &list_type, NULL);
    if (!list) {
        rb->snprintf(status, status_size,
                     "Sky cache chip list %u missing", (unsigned)list_id);
        return false;
    }

    while (dst < ARRAYLEN(cache_build_list) - 1 && src < list_size) {
        cache_build_list[dst++] = list[src] & 0x7fff;
        if (list[src++] == 0)
            break;
    }

    (void)list_type;
    return runtime_cache_files(status, status_size);
}

static bool runtime_set_grafix_program(uint16_t *compact,
                                       uint16_t compact_size,
                                       uint16_t program_id,
                                       uint16_t logic)
{
    const uint16_t *program;
    uint16_t program_size;
    uint16_t program_type;

    program = scummvm_sky_cpt_fetch(program_id, &program_size,
                                    &program_type, NULL);
    if (!program || program_size == 0)
        return false;

    compact_set_word(compact, compact_size, SKY_COMPACT_GRAFIX_PROG_ID,
                     program_id);
    compact_set_word(compact, compact_size, SKY_COMPACT_GRAFIX_PROG_POS, 1);
    compact_set_word(compact, compact_size, SKY_COMPACT_OFFSET, program[0]);
    compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD, logic);
    (void)program_type;
    return true;
}

static bool runtime_apply_reset_block(uint16_t *compact,
                                      uint16_t compact_size,
                                      uint16_t reset_id)
{
    const uint16_t *reset;
    uint16_t reset_size;
    uint16_t reset_type;
    uint16_t i;

    reset = scummvm_sky_cpt_fetch(reset_id, &reset_size, &reset_type, NULL);
    if (!reset)
        return false;

    for (i = 0; i + 1 < reset_size && reset[i] != 0xffff; i += 2) {
        uint16_t word_index;

        if (compact_offset_to_word(reset[i], &word_index))
            compact_set_word(compact, compact_size, word_index,
                             reset[i + 1]);
    }

    (void)reset_type;
    return true;
}

static bool runtime_start_menu(uint16_t first_object)
{
    uint16_t menu_objects[20];
    uint32_t menu_length = 0;
    uint32_t i;
    uint32_t scroll;
    uint16_t rolling_x;
    uint16_t y;

    first_object /= 4;
    rb->memset(menu_objects, 0, sizeof(menu_objects));

    for (i = first_object; i < first_object + ARRAYLEN(menu_objects) &&
         i < SKY_NUM_SCRIPT_VARS; i++) {
        if (script_vars[i] && menu_length < ARRAYLEN(menu_objects))
            menu_objects[menu_length++] = (uint16_t)script_vars[i];
    }

    script_vars[SKY_VAR_MENU_LENGTH] = menu_length;
    for (i = menu_length; i < 11 && i < ARRAYLEN(menu_objects); i++)
        menu_objects[i] = (uint16_t)(51 + i - menu_length);

    if (script_vars[SKY_VAR_MENU_LENGTH] < 11)
        script_vars[SKY_VAR_SCROLL_OFFSET] = 0;
    else if (script_vars[SKY_VAR_MENU_LENGTH] <
             script_vars[SKY_VAR_SCROLL_OFFSET] + 11)
        script_vars[SKY_VAR_SCROLL_OFFSET] =
            script_vars[SKY_VAR_MENU_LENGTH] - 11;

    scroll = script_vars[SKY_VAR_SCROLL_OFFSET];
    rolling_x = SKY_TOP_LEFT_X + 28;
    y = script_vars[SKY_VAR_MENU] == 2 ? 136 : 112;

    for (i = 0; i < 11 && scroll + i < ARRAYLEN(menu_objects); i++) {
        uint16_t id = menu_objects[scroll + i];
        uint16_t target_size;
        uint16_t target_type;
        uint16_t *target;

        if (!id)
            continue;

        target = scummvm_sky_cpt_fetch_mutable(id, &target_size,
                                               &target_type, NULL);
        if (!target)
            continue;

        compact_set_word(target, target_size, SKY_COMPACT_STATUS,
                         SKY_ST_MOUSE | SKY_ST_FOREGROUND | SKY_ST_LOGIC);
        compact_set_word(target, target_size, SKY_COMPACT_SCREEN,
                         (uint16_t)script_vars[SKY_VAR_SCREEN]);
        compact_set_word(target, target_size, SKY_COMPACT_X, rolling_x);
        compact_set_word(target, target_size, SKY_COMPACT_Y, y);
        rolling_x += 24;
        (void)target_type;
    }

    for (i = SKY_MENU_BAR_LEFT; i <= SKY_MENU_BAR_RIGHT; i++) {
        uint16_t target_size;
        uint16_t target_type;
        uint16_t *target = scummvm_sky_cpt_fetch_mutable((uint16_t)i,
                                                         &target_size,
                                                         &target_type,
                                                         NULL);
        if (target) {
            compact_set_word(target, target_size, SKY_COMPACT_STATUS,
                             SKY_ST_MOUSE | SKY_ST_FOREGROUND |
                             SKY_ST_LOGIC);
            compact_set_word(target, target_size, SKY_COMPACT_SCREEN,
                             (uint16_t)script_vars[SKY_VAR_SCREEN]);
        }
        (void)target_type;
    }

    return true;
}

static bool runtime_step_simple_anim(uint16_t *compact,
                                     uint16_t compact_size,
                                     char *status,
                                     size_t status_size)
{
    const uint16_t *program;
    uint16_t program_size;
    uint16_t program_type;
    uint16_t program_id;
    uint16_t pos;
    uint16_t offset;
    uint32_t guard = 0;

    program_id = compact_word(compact, compact_size,
                              SKY_COMPACT_GRAFIX_PROG_ID);
    pos = compact_word(compact, compact_size, SKY_COMPACT_GRAFIX_PROG_POS);
    offset = compact_word(compact, compact_size, SKY_COMPACT_OFFSET);

    program = scummvm_sky_cpt_fetch(program_id, &program_size,
                                    &program_type, NULL);
    if (!program) {
        rb->snprintf(status, status_size,
                     "Sky anim program %u missing", (unsigned)program_id);
        return false;
    }

    while (pos < program_size && guard++ < 32) {
        uint16_t command = program[pos];

        if (command == 0) {
            compact_set_word(compact, compact_size, SKY_COMPACT_DOWN_FLAG, 0);
            compact_set_word(compact, compact_size,
                             SKY_COMPACT_LOGIC_FIELD, SKY_LOGIC_SCRIPT);
            compact_set_word(compact, compact_size,
                             SKY_COMPACT_GRAFIX_PROG_POS, pos);
            (void)program_type;
            return true;
        }

        if (pos + 2 >= program_size)
            break;

        compact_set_word(compact, compact_size,
                         SKY_COMPACT_GRAFIX_PROG_POS, pos + 3);

        if (command == SKY_SEND_SYNC) {
            uint16_t target_size;
            uint16_t target_type;
            uint16_t *target = scummvm_sky_cpt_fetch_mutable(program[pos + 1],
                                                             &target_size,
                                                             &target_type,
                                                             NULL);
            if (target)
                compact_set_word(target, target_size, SKY_COMPACT_SYNC,
                                 program[pos + 2]);
            (void)target_type;
            pos += 3;
            continue;
        }

        compact_set_word(compact, compact_size, SKY_COMPACT_FRAME,
                         program[pos + 2] >= 64 ?
                         program[pos + 2] :
                         (uint16_t)(program[pos + 2] + offset));
        (void)program_type;
        return true;
    }

    rb->snprintf(status, status_size,
                 "Sky anim program %u exhausted", (unsigned)program_id);
    (void)program_type;
    return false;
}

static bool runtime_step_mod_anim(uint16_t *compact,
                                  uint16_t compact_size,
                                  char *status,
                                  size_t status_size)
{
    const uint16_t *program;
    uint16_t program_size;
    uint16_t program_type;
    uint16_t program_id;
    uint16_t pos;
    uint16_t offset;
    uint32_t guard = 0;

    program_id = compact_word(compact, compact_size,
                              SKY_COMPACT_GRAFIX_PROG_ID);
    pos = compact_word(compact, compact_size, SKY_COMPACT_GRAFIX_PROG_POS);
    offset = compact_word(compact, compact_size, SKY_COMPACT_OFFSET);

    program = scummvm_sky_cpt_fetch(program_id, &program_size,
                                    &program_type, NULL);
    if (!program) {
        rb->snprintf(status, status_size,
                     "Sky mod anim program %u missing",
                     (unsigned)program_id);
        return false;
    }

    while (pos < program_size && guard++ < 32) {
        uint16_t command = program[pos];

        if (command == 0) {
            compact_set_word(compact, compact_size, SKY_COMPACT_DOWN_FLAG, 0);
            compact_set_word(compact, compact_size,
                             SKY_COMPACT_LOGIC_FIELD, SKY_LOGIC_SCRIPT);
            compact_set_word(compact, compact_size,
                             SKY_COMPACT_GRAFIX_PROG_POS, pos);
            (void)program_type;
            return true;
        }

        if (pos + 2 >= program_size)
            break;

        compact_set_word(compact, compact_size,
                         SKY_COMPACT_GRAFIX_PROG_POS, pos + 3);

        if (command == SKY_LF_START_FX) {
            runtime_audio_start_fx(program[pos + 1], program[pos + 2]);
            pos += 3;
            continue;
        }

        if (command > SKY_LF_START_FX) {
            uint16_t target_size;
            uint16_t target_type;
            uint16_t *target = scummvm_sky_cpt_fetch_mutable(program[pos + 1],
                                                             &target_size,
                                                             &target_type,
                                                             NULL);
            if (target)
                compact_set_word(target, target_size, SKY_COMPACT_SYNC,
                                 program[pos + 2]);
            (void)target_type;
            pos += 3;
            continue;
        }

        compact_set_word(compact, compact_size, SKY_COMPACT_X,
                         program[pos]);
        compact_set_word(compact, compact_size, SKY_COMPACT_Y,
                         program[pos + 1]);
        compact_set_word(compact, compact_size, SKY_COMPACT_FRAME,
                         (uint16_t)(program[pos + 2] | offset));
        (void)program_type;
        return true;
    }

    rb->snprintf(status, status_size,
                 "Sky mod anim program %u exhausted",
                 (unsigned)program_id);
    (void)program_type;
    return false;
}

static bool runtime_step_compact(uint16_t id,
                                 char *status,
                                 size_t status_size);

static bool runtime_mcode(uint16_t mcode,
                          uint32_t a,
                          uint32_t b,
                          uint32_t c,
                          uint16_t current_id,
                          uint16_t *compact,
                          uint16_t compact_size,
                          bool *continue_script,
                          char *status,
                          size_t status_size)
{
    uint16_t target_size;
    uint16_t target_type;
    uint16_t *target;

    *continue_script = true;

    switch (mcode) {
    case 0:
        return runtime_cache_chip((uint16_t)a, status, status_size);
    case 1:
        return runtime_cache_fast((uint16_t)a, status, status_size);
    case 2:
        return runtime_draw_screen((uint16_t)a, status, status_size);
    case 3:
        compact_set_word(compact, compact_size, SKY_COMPACT_DOWN_FLAG, 1);
        compact_set_word(compact, compact_size, SKY_COMPACT_AR_TARGET_X,
                         (uint16_t)a);
        compact_set_word(compact, compact_size, SKY_COMPACT_AR_TARGET_Y,
                         (uint16_t)b);
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                         SKY_LOGIC_AR);
        compact_set_word(compact, compact_size, SKY_COMPACT_X,
                         compact_word(compact, compact_size,
                                      SKY_COMPACT_X) & 0xfff8);
        compact_set_word(compact, compact_size, SKY_COMPACT_Y,
                         compact_word(compact, compact_size,
                                      SKY_COMPACT_Y) & 0xfff8);
        *continue_script = false;
        rb->snprintf(status, status_size, "Sky mcode fnAr %04x",
                     (unsigned)current_id);
        return true;
    case 4:
        compact_set_word(compact, compact_size, SKY_COMPACT_MOOD, 0);
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                         SKY_LOGIC_AR_ANIM);
        *continue_script = false;
        rb->snprintf(status, status_size, "Sky mcode fnArAnimate %04x",
                     (unsigned)current_id);
        return true;
    case 5:
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD, 0);
        rb->snprintf(status, status_size, "Sky mcode fnIdle %04x",
                     (unsigned)current_id);
        return true;
    case 6:
    {
        uint16_t new_mode;
        uint16_t script_index;
        uint16_t offset_index;

        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (!target) {
            rb->snprintf(status, status_size,
                         "Sky mcode fnInteract missing %lu",
                         (unsigned long)a);
            return false;
        }

        new_mode = (uint16_t)(compact_word(compact, compact_size,
                                           SKY_COMPACT_MODE) + 4);
        if (!compact_mode_slots(new_mode, &script_index, &offset_index)) {
            rb->snprintf(status, status_size,
                         "Sky mcode fnInteract mode %u invalid",
                         (unsigned)new_mode);
            (void)target_type;
            return false;
        }

        compact_set_word(compact, compact_size, SKY_COMPACT_MODE, new_mode);
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                         SKY_LOGIC_SCRIPT);
        compact_set_word(compact, compact_size, script_index,
                         compact_word(target, target_size,
                                      SKY_COMPACT_ACTION_SCRIPT));
        compact_set_word(compact, compact_size, offset_index, 0);
        *continue_script = false;
        (void)target_type;
        return true;
    }
    case 7:
        compact_set_word(compact, compact_size, SKY_COMPACT_MODE,
                         compact_word(compact, compact_size,
                                      SKY_COMPACT_MODE) + 4);
        compact_set_word(compact, compact_size, SKY_COMPACT_ACTION_SUB,
                         (uint16_t)(a & 0xffff));
        compact_set_word(compact, compact_size, SKY_COMPACT_ACTION_SUB_OFF,
                         (uint16_t)(a >> 16));
        *continue_script = false;
        rb->snprintf(status, status_size, "Sky mcode fnStartSub %04x",
                     (unsigned)current_id);
        return true;
    case 8:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (!target) {
            rb->snprintf(status, status_size,
                         "Sky mcode fnTheyStartSub missing %lu",
                         (unsigned long)a);
            return false;
        }
        compact_set_word(target, target_size, SKY_COMPACT_MODE,
                         compact_word(target, target_size,
                                      SKY_COMPACT_MODE) + 4);
        compact_set_word(target, target_size, SKY_COMPACT_ACTION_SUB,
                         (uint16_t)(b & 0xffff));
        compact_set_word(target, target_size, SKY_COMPACT_ACTION_SUB_OFF,
                         (uint16_t)(b >> 16));
        (void)target_type;
        rb->snprintf(status, status_size, "Sky mcode fnTheyStartSub");
        return true;
    case 9:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (!target) {
            rb->snprintf(status, status_size,
                         "Sky mcode fnAssignBase missing %lu",
                         (unsigned long)a);
            return false;
        }
        compact_set_word(target, target_size, SKY_COMPACT_MODE, 0);
        compact_set_word(target, target_size, SKY_COMPACT_LOGIC_FIELD,
                         SKY_LOGIC_SCRIPT);
        compact_set_word(target, target_size, SKY_COMPACT_BASE_SUB,
                         (uint16_t)(b & 0xffff));
        compact_set_word(target, target_size, SKY_COMPACT_BASE_SUB_OFF,
                         (uint16_t)(b >> 16));
        (void)target_type;
        rb->snprintf(status, status_size, "Sky mcode fnAssignBase");
        return true;
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
        return true;
    case 19:
    {
        uint16_t place_id = compact_word(compact, compact_size,
                                         SKY_COMPACT_PLACE);
        uint16_t *place;
        uint16_t place_size;
        uint16_t place_type;
        const uint16_t *get_to_table;
        uint16_t table_id;
        uint16_t table_size;
        uint16_t table_type;
        uint16_t i;
        uint16_t script_index;
        uint16_t offset_index;
        uint16_t new_mode = (uint16_t)(compact_word(compact, compact_size,
                                                     SKY_COMPACT_MODE) + 4);

        place = scummvm_sky_cpt_fetch_mutable(place_id, &place_size,
                                              &place_type, NULL);
        if (!place) {
            rb->snprintf(status, status_size,
                         "Sky mcode fnGetTo place %u missing",
                         (unsigned)place_id);
            return false;
        }

        table_id = compact_word(place, place_size, 5);
        get_to_table = scummvm_sky_cpt_fetch(table_id, &table_size,
                                             &table_type, NULL);
        if (!get_to_table) {
            rb->snprintf(status, status_size,
                         "Sky mcode fnGetTo table %u missing",
                         (unsigned)table_id);
            (void)place_type;
            return false;
        }

        switch (new_mode) {
        case 4:
            script_index = SKY_COMPACT_ACTION_SUB;
            offset_index = SKY_COMPACT_ACTION_SUB_OFF;
            break;
        case 8:
            script_index = SKY_COMPACT_GET_TO_SUB;
            offset_index = SKY_COMPACT_GET_TO_SUB_OFF;
            break;
        case 12:
            script_index = SKY_COMPACT_EXTRA_SUB;
            offset_index = SKY_COMPACT_EXTRA_SUB_OFF;
            break;
        default:
            rb->snprintf(status, status_size,
                         "Sky mcode fnGetTo mode %u invalid",
                         (unsigned)new_mode);
            (void)place_type;
            (void)table_type;
            return false;
        }

        for (i = 0; i + 1 < table_size && get_to_table[i]; i += 2) {
            if (get_to_table[i] == (uint16_t)a) {
                compact_set_word(compact, compact_size, SKY_COMPACT_UP_FLAG,
                                 (uint16_t)b);
                compact_set_word(compact, compact_size, SKY_COMPACT_MODE,
                                 new_mode);
                compact_set_word(compact, compact_size, script_index,
                                 get_to_table[i + 1]);
                compact_set_word(compact, compact_size, offset_index, 0);
                *continue_script = false;
                (void)place_type;
                (void)table_type;
                return true;
            }
        }

        rb->snprintf(status, status_size,
                     "Sky mcode fnGetTo target %lu missing",
                     (unsigned long)a);
        (void)place_type;
        (void)table_type;
        return false;
    }
    case 20:
    {
        uint16_t stand_index;
        uint16_t stand_id;

        if (!compact_offset_to_word((uint16_t)(SKY_C_STAND_UP +
                                               compact_word(compact,
                                                            compact_size,
                                                            SKY_COMPACT_MEGA_SET) +
                                               compact_word(compact,
                                                            compact_size,
                                                            SKY_COMPACT_DIR) * 4),
                                    &stand_index)) {
            rb->strlcpy(status, "Sky mcode fnSetToStand bad offset",
                        status_size);
            return false;
        }

        stand_id = compact_word(compact, compact_size, stand_index);
        compact_set_word(compact, compact_size, SKY_COMPACT_MOOD, 1);
        if (!runtime_set_grafix_program(compact, compact_size, stand_id,
                                        SKY_LOGIC_SIMPLE_MOD)) {
            rb->snprintf(status, status_size,
                         "Sky mcode fnSetToStand missing %u",
                         (unsigned)stand_id);
            return false;
        }
        *continue_script = false;
        return true;
    }
    case 21:
    {
        const uint16_t *turn_table;
        uint16_t turn_size;
        uint16_t turn_type;
        uint16_t table_id_index;
        uint16_t table_id;
        uint16_t old_dir = compact_word(compact, compact_size,
                                        SKY_COMPACT_DIR);
        uint16_t new_dir = (uint16_t)(a & 0xffff);
        uint16_t turn_id;

        if (!compact_offset_to_word((uint16_t)(158 +
                                               compact_word(compact,
                                                            compact_size,
                                                            SKY_COMPACT_MEGA_SET)),
                                    &table_id_index))
            return true;

        table_id = compact_word(compact, compact_size, table_id_index);
        turn_table = scummvm_sky_cpt_fetch(table_id, &turn_size,
                                           &turn_type, NULL);
        compact_set_word(compact, compact_size, SKY_COMPACT_DIR, new_dir);
        if (!turn_table || old_dir >= 5 || new_dir >= 5 ||
            old_dir * 5 + new_dir >= turn_size) {
            (void)turn_type;
            return true;
        }

        turn_id = turn_table[old_dir * 5 + new_dir];
        if (!turn_id) {
            (void)turn_type;
            return true;
        }

        compact_set_word(compact, compact_size, SKY_COMPACT_TURN_PROG_ID,
                         turn_id);
        compact_set_word(compact, compact_size, SKY_COMPACT_TURN_PROG_POS, 0);
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                         SKY_LOGIC_TURNING);
        *continue_script = false;
        (void)turn_type;
        return true;
    }
    case 22:
        compact_set_word(compact, compact_size, SKY_COMPACT_LEAVING,
                         (uint16_t)(a & 0xffff));
        if ((a / 4) < SKY_NUM_SCRIPT_VARS)
            script_vars[a / 4]++;
        return true;
    case 23:
        if (compact_word(compact, compact_size, SKY_COMPACT_LEAVING)) {
            uint16_t leaving = compact_word(compact, compact_size,
                                            SKY_COMPACT_LEAVING);
            if ((leaving / 4) < SKY_NUM_SCRIPT_VARS &&
                script_vars[leaving / 4])
                script_vars[leaving / 4]--;
            compact_set_word(compact, compact_size, SKY_COMPACT_LEAVING, 0);
        }
        return true;
    case 24:
        compact_set_word(compact, compact_size, SKY_COMPACT_ALT,
                         (uint16_t)(a & 0xffff));
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                         SKY_LOGIC_ALT);
        *continue_script = false;
        return true;
    case 26:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (target) {
            if (compact_word(target, target_size, SKY_COMPACT_STATUS) &
                SKY_ST_GRID_PLOT)
                runtime_grid_object_to_walk(target, target_size, false);
            compact_set_word(target, target_size, SKY_COMPACT_STATUS, 0);
        }
        (void)target_type;
        return true;
    case 27:
        if (!script_vars[SKY_VAR_MOUSE_STOP]) {
            script_vars[SKY_VAR_MOUSE_STATUS] &= 1;
            script_vars[SKY_VAR_GET_OFF] = 0;
        }
        return true;
    case 28:
        script_vars[SKY_VAR_MOUSE_STATUS] |= 1;
        return true;
    case 29:
        script_vars[SKY_VAR_MOUSE_STATUS] |= 4;
        return true;
    case 30:
        script_vars[SKY_VAR_MOUSE_STATUS] &= ~4UL;
        return true;
    case 31:
        script_vars[SKY_VAR_MOUSE_STOP] |= 1;
        return true;
    case 32:
        script_vars[SKY_VAR_MOUSE_STOP] = 0;
        return true;
    case 33:
        return true;
    case 34:
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD, 0);
        *continue_script = false;
        return true;
    case 35:
    case 36:
    case 37:
    case 38:
        compact_set_word(compact, compact_size, SKY_COMPACT_FLAG,
                         (uint16_t)(a & 0xffff));
        runtime_overlay_clear();
        if (!runtime_start_speech_text((uint16_t)a, b))
            runtime_overlay_message(b, 0, false, 150);
        *continue_script = false;
        return true;
    case 39:
        script_vars[SKY_VAR_THE_CHOSEN_ONE] = 0;
        runtime_overlay_chooser();
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                         SKY_LOGIC_CHOOSE);
        *continue_script = false;
        return true;
    case 40:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (target)
            compact_set_word(target, target_size, SKY_COMPACT_FRAME,
                             (uint16_t)b);
        (void)target_type;
        return true;
    case 41:
    {
        uint16_t id;

        runtime_clear_text_mouse(true);
        for (id = SKY_FIRST_TEXT_COMPACT;
             id < SKY_FIRST_TEXT_COMPACT + 10;
             id++) {
            target = scummvm_sky_cpt_fetch_mutable(id, &target_size,
                                                   &target_type, NULL);
            if (target &&
                (compact_word(target, target_size,
                              SKY_COMPACT_STATUS) & SKY_ST_MOUSE))
                compact_set_word(target, target_size,
                                 SKY_COMPACT_STATUS, 0);
            (void)target_type;
        }
        return true;
    }
    case 42:
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                         SKY_LOGIC_STOPPED);
        *continue_script = false;
        return true;
    case 43:
        compact_set_word(compact, compact_size, SKY_COMPACT_WAITING_FOR,
                         (uint16_t)a);
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                         SKY_LOGIC_PAUSE);
        compact_set_word(compact, compact_size, SKY_COMPACT_FLAG, 1);
        return true;
    case 44:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (target)
            compact_set_word(target, target_size, SKY_COMPACT_SYNC,
                             (uint16_t)b);
        (void)target_type;
        *continue_script = false;
        return true;
    case 45:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (target)
            compact_set_word(target, target_size, SKY_COMPACT_SYNC,
                             (uint16_t)b);
        (void)target_type;
        return true;
    case 46:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (target)
            compact_set_word(target, target_size, SKY_COMPACT_REQUEST,
                             (uint16_t)(b & 0xffff));
        (void)target_type;
        *continue_script = false;
        return true;
    case 47:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (target)
            compact_set_word(target, target_size, SKY_COMPACT_REQUEST, 0);
        (void)target_type;
        return true;
    case 48:
        if (!compact_word(compact, compact_size, SKY_COMPACT_REQUEST))
            return true;
        compact_set_word(compact, compact_size, SKY_COMPACT_MODE, 4);
        compact_set_word(compact, compact_size, SKY_COMPACT_ACTION_SUB,
                         compact_word(compact, compact_size,
                                      SKY_COMPACT_REQUEST));
        compact_set_word(compact, compact_size, SKY_COMPACT_ACTION_SUB_OFF,
                         0);
        compact_set_word(compact, compact_size, SKY_COMPACT_REQUEST, 0);
        *continue_script = false;
        return true;
    case 49:
        return runtime_start_menu((uint16_t)a);
    case 50:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (target) {
            compact_set_word(target, target_size, SKY_COMPACT_FRAME,
                             compact_word(target, target_size,
                                          SKY_COMPACT_FRAME) - 1);
            compact_set_word(target, target_size, SKY_COMPACT_GET_TO_FLAG,
                             0);
        }
        (void)target_type;
        return true;
    case 51:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (target) {
            int32_t dx = (int32_t)compact_word(compact, compact_size,
                                               SKY_COMPACT_X) -
                         (int32_t)compact_word(target, target_size,
                                               SKY_COMPACT_X);
            int32_t bottom = (int32_t)compact_word(target, target_size,
                                                   SKY_COMPACT_Y) +
                             (int16_t)compact_word(target, target_size,
                                                   SKY_COMPACT_MOUSE_REL_Y) +
                             compact_word(target, target_size,
                                          SKY_COMPACT_MOUSE_SIZE_Y);
            int32_t dy = (int32_t)compact_word(compact, compact_size,
                                               SKY_COMPACT_Y) - bottom;
            uint16_t dir = dx < 0 ? 3 : 2;

            if (dx < 0)
                dx = -dx;
            if (dy < 0) {
                dy = -dy;
                if (dy >= dx)
                    dir = 1;
            } else if (dy >= dx) {
                dir = 0;
            }

            compact_set_word(compact, compact_size, SKY_COMPACT_GET_TO_FLAG,
                             dir);
        }
        (void)target_type;
        return true;
    case 52:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (target) {
            uint16_t st = compact_word(target, target_size,
                                       SKY_COMPACT_STATUS);
            compact_set_word(target, target_size, SKY_COMPACT_STATUS,
                             (st & ~SKY_ST_DRAW_MASK) |
                             SKY_ST_FOREGROUND);
        }
        (void)target_type;
        return true;
    case 53:
        compact_set_word(compact, compact_size, SKY_COMPACT_STATUS,
                         (compact_word(compact, compact_size,
                                       SKY_COMPACT_STATUS) &
                          ~SKY_ST_DRAW_MASK) | SKY_ST_BACKGROUND);
        return true;
    case 54:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (target) {
            uint16_t st = compact_word(target, target_size,
                                       SKY_COMPACT_STATUS);
            compact_set_word(target, target_size, SKY_COMPACT_STATUS,
                             (st & ~SKY_ST_DRAW_MASK) |
                             SKY_ST_BACKGROUND);
        }
        (void)target_type;
        return true;
    case 55:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (target) {
            uint16_t st = compact_word(target, target_size,
                                       SKY_COMPACT_STATUS);
            compact_set_word(target, target_size, SKY_COMPACT_STATUS,
                             (st & ~SKY_ST_DRAW_MASK) | SKY_ST_SORT);
        }
        (void)target_type;
        return true;
    case 56:
        compact_set_word(compact, compact_size, SKY_COMPACT_STATUS,
                         compact_word(compact, compact_size,
                                      SKY_COMPACT_STATUS) &
                         ~SKY_ST_DRAW_MASK);
        return true;
    case 57:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (target)
            runtime_apply_reset_block(target, target_size, (uint16_t)b);
        (void)target_type;
        return true;
    case 59:
        compact_set_word(compact, compact_size, SKY_COMPACT_STATUS,
                         compact_word(compact, compact_size,
                                      SKY_COMPACT_STATUS) ^
                         SKY_ST_GRID_PLOT);
        return true;
    case 60:
        compact_set_word(compact, compact_size, SKY_COMPACT_FLAG,
                         (uint16_t)(a & 0xffff));
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                         SKY_LOGIC_PAUSE);
        *continue_script = false;
        return true;
    case 61:
        if (!runtime_set_grafix_program(compact, compact_size, (uint16_t)a,
                                        SKY_LOGIC_MOD_ANIMATE)) {
            rb->snprintf(status, status_size,
                         "Sky mcode fnRunAnimMod missing %lu",
                         (unsigned long)a);
            return false;
        }
        *continue_script = false;
        return true;
    case 62:
        if (!runtime_set_grafix_program(compact, compact_size, (uint16_t)a,
                                        SKY_LOGIC_SIMPLE_MOD)) {
            rb->snprintf(status, status_size,
                         "Sky mcode fnSimpleMod missing %lu",
                         (unsigned long)a);
            return false;
        }
        *continue_script = false;
        return true;
    case 63:
        if (!runtime_set_grafix_program(compact, compact_size, (uint16_t)a,
                                        SKY_LOGIC_FRAMES)) {
            rb->snprintf(status, status_size,
                         "Sky mcode fnRunFrames missing %lu",
                         (unsigned long)a);
            return false;
        }
        *continue_script = false;
        return true;
    case 64:
        if (compact_word(compact, compact_size, SKY_COMPACT_SYNC))
            return true;
        compact_set_word(compact, compact_size, SKY_COMPACT_LOGIC_FIELD,
                         SKY_LOGIC_WAIT_SYNC);
        *continue_script = false;
        return true;
    case 65:
        compact_set_word(compact, compact_size, SKY_COMPACT_MEGA_SET,
                         compact_word(compact, compact_size,
                                      SKY_COMPACT_MEGA_SET) +
                         SKY_NEXT_MEGA_SET);
        return true;
    case 66:
        compact_set_word(compact, compact_size, SKY_COMPACT_MEGA_SET,
                         compact_word(compact, compact_size,
                                      SKY_COMPACT_MEGA_SET) -
                         SKY_NEXT_MEGA_SET);
        return true;
    case 67:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (target)
            compact_set_word(target, target_size, SKY_COMPACT_MEGA_SET,
                             (uint16_t)(b * SKY_NEXT_MEGA_SET));
        (void)target_type;
        return true;
    case 68:
    {
        const uint16_t *move_list;
        const uint16_t *items;
        uint16_t move_size;
        uint16_t move_type;
        uint16_t item_size;
        uint16_t item_type;
        uint16_t i;

        move_list = scummvm_sky_cpt_fetch(SKY_CPT_MOVE_LIST, &move_size,
                                          &move_type, NULL);
        if (!move_list || a >= move_size)
            return true;

        items = scummvm_sky_cpt_fetch(move_list[a], &item_size,
                                      &item_type, NULL);
        if (!items)
            return true;

        for (i = 0; i < item_size && items[i]; i++) {
            target = scummvm_sky_cpt_fetch_mutable(items[i], &target_size,
                                                   &target_type, NULL);
            if (target)
                compact_set_word(target, target_size, SKY_COMPACT_SCREEN,
                                 (uint16_t)(b & 0xffff));
            (void)target_type;
        }
        (void)move_type;
        (void)item_type;
        return true;
    }
    case 69:
        rb->memset(&script_vars[SKY_VAR_TEXT1], 0,
                   16 * sizeof(script_vars[0]));
        return true;
    case 70:
    {
        uint32_t i;

        for (i = SKY_VAR_TEXT1; i + 1 < SKY_VAR_TEXT1 + 16; i += 2) {
            if (!script_vars[i]) {
                script_vars[i] = a;
                script_vars[i + 1] = b;
                break;
            }
        }
        return true;
    }
    case 73:
        script_vars[SKY_VAR_RND] = rb->rand() & a;
        return true;
    case 74:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        script_vars[SKY_VAR_RESULT] = target &&
            compact_word(target, target_size, SKY_COMPACT_SCREEN) == b;
        (void)target_type;
        return true;
    case 75:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (target)
            compact_set_word(target, target_size, SKY_COMPACT_STATUS,
                             compact_word(target, target_size,
                                          SKY_COMPACT_STATUS) ^
                             SKY_ST_MOUSE);
        (void)target_type;
        return true;
    case 76:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (target)
            compact_set_word(target, target_size, SKY_COMPACT_STATUS,
                             compact_word(target, target_size,
                                          SKY_COMPACT_STATUS) |
                             SKY_ST_MOUSE);
        (void)target_type;
        return true;
    case 77:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (target)
            compact_set_word(target, target_size, SKY_COMPACT_STATUS,
                             compact_word(target, target_size,
                                          SKY_COMPACT_STATUS) &
                             ~SKY_ST_MOUSE);
        (void)target_type;
        return true;
    case 78:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        script_vars[SKY_VAR_RESULT] = target ?
            compact_word(target, target_size, SKY_COMPACT_X) : 0;
        (void)target_type;
        return true;
    case 79:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        script_vars[SKY_VAR_RESULT] = target ?
            compact_word(target, target_size, SKY_COMPACT_Y) : 0;
        (void)target_type;
        return true;
    case 80:
    {
        const uint16_t *list;
        uint16_t list_size;
        uint16_t list_type;
        uint16_t i;

        script_vars[SKY_VAR_RESULT] = 0;
        list = scummvm_sky_cpt_fetch((uint16_t)a, &list_size, &list_type,
                                     NULL);
        if (!list)
            return true;

        for (i = 0; i + 4 < list_size && list[i]; i += 5) {
            if (b >= list[i] && b < list[i + 1] &&
                c >= list[i + 2] && c < list[i + 3])
                script_vars[SKY_VAR_RESULT] = list[i + 4];
        }
        (void)list_type;
        return true;
    }
    case 81:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        script_vars[SKY_VAR_RESULT] = target ?
            compact_word(target, target_size, SKY_COMPACT_PLACE) : 0;
        (void)target_type;
        return true;
    case 82:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (target) {
            script_vars[27] = compact_word(target, target_size,
                                           SKY_COMPACT_X);
            script_vars[28] = compact_word(target, target_size,
                                           SKY_COMPACT_Y);
            script_vars[29] = compact_word(target, target_size,
                                           SKY_COMPACT_MOOD);
            script_vars[30] = compact_word(target, target_size,
                                           SKY_COMPACT_SCREEN);
        }
        (void)target_type;
        return true;
    case 83:
        runtime_palette_id = (uint16_t)a;
        return true;
    case 84:
    {
        const uint16_t *msg_data;
        uint16_t msg_size;
        uint16_t msg_type;
        uint16_t text_id = 0;

        msg_data = scummvm_sky_cpt_fetch((uint16_t)a, &msg_size,
                                         &msg_type, NULL);
        if (msg_data && msg_size >= 5 &&
            runtime_alloc_text(b,
                               msg_data[1],
                               209,
                               false,
                               msg_data[2],
                               &text_id,
                               NULL)) {
            target = scummvm_sky_cpt_fetch_mutable(text_id, &target_size,
                                                   &target_type, NULL);
            if (target) {
                compact_set_word(target, target_size, SKY_COMPACT_X,
                                 msg_data[3]);
                compact_set_word(target, target_size, SKY_COMPACT_Y,
                                 msg_data[4]);
            }
            script_vars[SKY_VAR_RESULT] = text_id;
            (void)target_type;
        } else {
            runtime_overlay_message(b, 0, false, (int)(a & 0xffff));
            script_vars[SKY_VAR_RESULT] = a;
        }
        (void)msg_type;
        return true;
    }
    case 85:
        target = scummvm_sky_cpt_fetch_mutable((uint16_t)a, &target_size,
                                               &target_type, NULL);
        if (target)
            compact_set_word(target, target_size,
                             SKY_COMPACT_CURSOR_TEXT, (uint16_t)b);
        (void)target_type;
        return true;
    case 86:
        return runtime_cache_one((uint16_t)a, status, status_size);
    case 87:
    case 88:
        runtime_flush_cache();
        return true;
    case 89:
        script_vars[SKY_VAR_PLAYER_X] =
            compact_word(compact, compact_size, SKY_COMPACT_X);
        script_vars[SKY_VAR_PLAYER_Y] =
            compact_word(compact, compact_size, SKY_COMPACT_Y);
        return true;
    case 90:
        runtime_grid_plot((uint16_t)a, (uint16_t)b, (uint16_t)c,
                          compact, compact_size, true);
        return true;
    case 91:
        runtime_grid_plot((uint16_t)a, (uint16_t)b, (uint16_t)c,
                          compact, compact_size, false);
        return true;
    case 92:
    {
        const uint16_t *eye_table;
        uint16_t eye_size;
        uint16_t eye_type;
        uint32_t idx;

        eye_table = scummvm_sky_cpt_fetch((uint16_t)a, &eye_size,
                                          &eye_type, NULL);
        idx = ((compact_word(compact, compact_size, SKY_COMPACT_X) -
                168) >> 3) +
              (((compact_word(compact, compact_size, SKY_COMPACT_Y) -
                 256) << 2));
        script_vars[SKY_VAR_RESULT] =
            eye_table && idx < eye_size ? eye_table[idx] : 0;
        (void)eye_type;
        return true;
    }
    case 93:
        return true;
    case 94:
        return true;
    case 95:
        script_vars[SKY_VAR_CUR_SECTION] = a;
        runtime_audio_load_section((uint16_t)a);
        return true;
    case 96:
    case 97:
        *continue_script = false;
        return true;
    case 98:
        return true;
    case 99:
        script_vars[SKY_VAR_RESULT] = 0;
        return true;
    case 100:
        return true;
    case 101:
        return true;
    case 102:
        return true;
    case 103:
        runtime_audio_start_fx((uint16_t)a, (uint16_t)c);
        return true;
    case 104:
        runtime_audio_stop();
        return true;
    case 105:
        runtime_music_start((uint16_t)a);
        return true;
    case 106:
        runtime_music_stop();
        return true;
    case 107:
        return true;
    case 108:
        return true;
    case 109:
        return true;
    case 110:
        return true;
    default:
        rb->snprintf(status, status_size,
                     "Sky mcode %u pending (%lu,%lu,%lu)",
                     (unsigned)mcode,
                     (unsigned long)a,
                     (unsigned long)b,
                     (unsigned long)c);
        return true;
    }
}

bool scummvm_sky_runtime_render(struct scummvm_video *video)
{
    const uint16_t *palette_words;
    uint16_t palette_size;
    uint16_t palette_type;
    fb_data palette[256];
    uint32_t i;
    uint32_t screen_pixels;

    if (!runtime_screen.data || runtime_palette_id == 0)
        return false;

    palette_words = scummvm_sky_cpt_fetch(runtime_palette_id,
                                          &palette_size,
                                          &palette_type,
                                          NULL);
    if (!palette_words || palette_size < 384)
        return false;

    for (i = 0; i < 256; i++) {
        uint16_t rg = palette_words[i * 2];
        uint16_t b = palette_words[i * 2 + 1];
        uint8_t r = (uint8_t)(rg & 0xff);
        uint8_t g = (uint8_t)(rg >> 8);
        uint8_t blue = (uint8_t)(b & 0xff);

        palette[i] = LCD_RGBPACK(r, g, blue);
    }

    screen_pixels = runtime_screen.size;
    if (screen_pixels > SCUMMVM_SURFACE_W * SCUMMVM_SURFACE_H)
        screen_pixels = SCUMMVM_SURFACE_W * SCUMMVM_SURFACE_H;

    for (i = 0; i < screen_pixels; i++)
        video->pixels[i] = palette[runtime_screen.data[i]];

    if (script_vars[SKY_VAR_DRAW_LIST_NO])
        runtime_draw_draw_lists(video, palette);
    else
        runtime_draw_logic_sprites(video, palette);

    (void)palette_type;
    return true;
}

bool scummvm_sky_runtime_scan(struct scummvm_sky_runtime_info *info,
                              char *status,
                              size_t status_size)
{
    const uint16_t *logic_list;
    uint16_t list_size;
    uint16_t list_type;
    uint16_t pos = 0;
    uint32_t guard = 0;

    rb->memset(info, 0, sizeof(*info));
    info->logic_list_id = script_vars[SKY_VAR_LOGIC_LIST_NO];
    info->current_section = script_vars[SKY_VAR_CUR_SECTION];
    info->current_screen = script_vars[SKY_VAR_SCREEN];

    logic_list = scummvm_sky_cpt_fetch((uint16_t)info->logic_list_id,
                                       &list_size, &list_type, NULL);
    if (!logic_list || list_type != SKY_CPT_MAINLIST) {
        rb->snprintf(status, status_size,
                     "Sky runtime needs main list %lu",
                     (unsigned long)info->logic_list_id);
        return false;
    }

    while (guard++ < SKY_MAX_LOGIC_SCAN) {
        uint16_t id;
        uint16_t cpt_size;
        uint16_t cpt_type;
        const uint16_t *compact;
        const char *name = NULL;
        uint16_t cpt_status;

        if (pos >= list_size) {
            rb->strlcpy(status, "Sky logic list is unterminated",
                        status_size);
            return false;
        }

        id = logic_list[pos++];
        if (id == 0)
            break;

        if (id == 0xffff) {
            if (pos >= list_size) {
                rb->strlcpy(status, "Sky logic redirect is truncated",
                            status_size);
                return false;
            }
            info->redirects++;
            logic_list = scummvm_sky_cpt_fetch(logic_list[pos++],
                                               &list_size, &list_type, NULL);
            pos = 0;
            if (!logic_list || list_type != SKY_CPT_MAINLIST) {
                rb->strlcpy(status, "Sky logic redirect target is invalid",
                            status_size);
                return false;
            }
            continue;
        }

        compact = scummvm_sky_cpt_fetch(id, &cpt_size, &cpt_type, &name);
        if (!compact)
            continue;

        info->logic_entries++;
        cpt_status = compact_word(compact, cpt_size, SKY_COMPACT_STATUS);
        if (cpt_status & SKY_ST_LOGIC)
            info->active_logic_entries++;

        if (info->first_logic_id == 0 &&
            (cpt_status & SKY_ST_LOGIC)) {
            info->first_logic_id = id;
            info->first_logic_status = cpt_status;
            if (name)
                rb->strlcpy(info->first_logic_name, name,
                            sizeof(info->first_logic_name));
        }

        (void)cpt_type;
        (void)compact_word(compact, cpt_size, SKY_COMPACT_LOGIC);
        (void)compact_word(compact, cpt_size, SKY_COMPACT_SCREEN);
    }

    if (guard >= SKY_MAX_LOGIC_SCAN) {
        rb->strlcpy(status, "Sky logic list scan exceeded guard",
                    status_size);
        return false;
    }

    rb->snprintf(status, status_size,
                 "Sky runtime: sec %lu, list %lu, %lu ids/%lu active",
                 (unsigned long)info->current_section,
                 (unsigned long)info->logic_list_id,
                 (unsigned long)info->logic_entries,
                 (unsigned long)info->active_logic_entries);
    return true;
}

static bool runtime_step_logic_list(char *status, size_t status_size)
{
    const uint16_t *logic_list;
    uint16_t list_size;
    uint16_t list_type;
    uint16_t pos = 0;
    uint32_t guard = 0;
    uint32_t stepped = 0;

    logic_list = scummvm_sky_cpt_fetch((uint16_t)script_vars[SKY_VAR_LOGIC_LIST_NO],
                                       &list_size, &list_type, NULL);
    if (!logic_list || list_type != SKY_CPT_MAINLIST) {
        rb->snprintf(status, status_size,
                     "Sky runtime needs main list %lu",
                     (unsigned long)script_vars[SKY_VAR_LOGIC_LIST_NO]);
        return false;
    }

    while (guard++ < SKY_MAX_LOGIC_SCAN) {
        uint16_t id;
        uint16_t cpt_size;
        uint16_t cpt_type;
        uint16_t *compact;

        if (pos >= list_size) {
            rb->strlcpy(status, "Sky logic list is unterminated",
                        status_size);
            return false;
        }

        id = logic_list[pos++];
        if (id == 0)
            break;

        if (id == 0xffff) {
            if (pos >= list_size) {
                rb->strlcpy(status, "Sky logic redirect is truncated",
                            status_size);
                return false;
            }
            logic_list = scummvm_sky_cpt_fetch(logic_list[pos++],
                                               &list_size, &list_type, NULL);
            pos = 0;
            if (!logic_list || list_type != SKY_CPT_MAINLIST) {
                rb->strlcpy(status, "Sky logic redirect target is invalid",
                            status_size);
                return false;
            }
            continue;
        }

        compact = scummvm_sky_cpt_fetch_mutable(id, &cpt_size, &cpt_type,
                                                NULL);
        if (!compact)
            continue;

        if (compact_word(compact, cpt_size, SKY_COMPACT_STATUS) &
            SKY_ST_LOGIC) {
            if (compact_word(compact, cpt_size, SKY_COMPACT_STATUS) &
                SKY_ST_GRID_PLOT)
                runtime_grid_object_to_walk(compact, cpt_size, false);
            if (!runtime_step_compact(id, status, status_size))
                return false;
            compact = scummvm_sky_cpt_fetch_mutable(id, &cpt_size,
                                                    &cpt_type, NULL);
            if (compact &&
                (compact_word(compact, cpt_size, SKY_COMPACT_STATUS) &
                 SKY_ST_GRID_PLOT))
                runtime_grid_object_to_walk(compact, cpt_size, true);
            if (compact)
                compact_set_word(compact, cpt_size, SKY_COMPACT_SYNC, 0);
            stepped++;
        }
        (void)cpt_type;
    }

    if (guard >= SKY_MAX_LOGIC_SCAN) {
        rb->strlcpy(status, "Sky logic list step exceeded guard",
                    status_size);
        return false;
    }

    rb->snprintf(status, status_size, "Sky stepped %lu active compacts",
                 (unsigned long)stepped);
    return true;
}

static bool runtime_mouse_hits_compact(const uint16_t *compact,
                                       uint16_t compact_size)
{
    int x;
    int y;
    int rel_x;
    int rel_y;
    int size_x;
    int size_y;

    if (!(compact_word(compact, compact_size, SKY_COMPACT_STATUS) &
          SKY_ST_MOUSE))
        return false;
    if (compact_word(compact, compact_size, SKY_COMPACT_SCREEN) !=
        (uint16_t)script_vars[SKY_VAR_SCREEN])
        return false;

    x = compact_word(compact, compact_size, SKY_COMPACT_X);
    y = compact_word(compact, compact_size, SKY_COMPACT_Y);
    rel_x = (int16_t)compact_word(compact, compact_size,
                                  SKY_COMPACT_MOUSE_REL_X);
    rel_y = (int16_t)compact_word(compact, compact_size,
                                  SKY_COMPACT_MOUSE_REL_Y);
    size_x = compact_word(compact, compact_size, SKY_COMPACT_MOUSE_SIZE_X);
    size_y = compact_word(compact, compact_size, SKY_COMPACT_MOUSE_SIZE_Y);

    if (size_x == 0)
        size_x = 24;
    if (size_y == 0)
        size_y = 24;

    x += rel_x;
    y += rel_y;
    x -= SKY_TOP_LEFT_X;
    y -= SKY_TOP_LEFT_Y;
    return runtime_mouse_x >= x && runtime_mouse_x < x + size_x &&
           runtime_mouse_y >= y && runtime_mouse_y < y + size_y;
}

static uint16_t runtime_find_mouse_hit(void)
{
    const uint16_t *mouse_list;
    uint16_t list_size;
    uint16_t list_type;
    uint16_t pos = 0;
    uint32_t guard = 0;
    uint16_t hit = 0;

    mouse_list = scummvm_sky_cpt_fetch((uint16_t)script_vars[SKY_VAR_MOUSE_LIST_NO],
                                       &list_size, &list_type, NULL);
    if (!mouse_list)
        mouse_list = scummvm_sky_cpt_fetch((uint16_t)script_vars[SKY_VAR_LOGIC_LIST_NO],
                                           &list_size, &list_type, NULL);
    if (!mouse_list)
        return 0;

    while (guard++ < SKY_MAX_LOGIC_SCAN) {
        uint16_t id;
        uint16_t cpt_size;
        uint16_t cpt_type;
        const uint16_t *compact;

        if (pos >= list_size)
            break;

        id = mouse_list[pos++];
        if (id == 0)
            break;

        if (id == 0xffff) {
            if (pos >= list_size)
                break;
            mouse_list = scummvm_sky_cpt_fetch(mouse_list[pos++],
                                               &list_size, &list_type, NULL);
            pos = 0;
            if (!mouse_list)
                break;
            continue;
        }

        compact = scummvm_sky_cpt_fetch(id, &cpt_size, &cpt_type, NULL);
        if (compact && runtime_mouse_hits_compact(compact, cpt_size))
            hit = id;
        (void)cpt_type;
    }

    return hit;
}

static void runtime_process_input(void)
{
    uint16_t hit;
    uint16_t i;

    if (!runtime_ready)
        return;

    script_vars[SKY_VAR_MOUSE_STATUS] =
        (script_vars[SKY_VAR_MOUSE_STATUS] & ~1UL) |
        (runtime_mouse_down ? 1UL : 0UL);

    hit = runtime_find_mouse_hit();
    script_vars[SKY_VAR_HIT_ID] = hit;
    if (hit) {
        script_vars[SKY_VAR_SPECIAL_ITEM] = hit;
        script_vars[SKY_VAR_CURSOR_ID] = hit;
    } else {
        script_vars[SKY_VAR_SPECIAL_ITEM] = 0;
        script_vars[SKY_VAR_GET_OFF] = 0;
    }

    if (runtime_mouse_clicked && runtime_overlay.choosing) {
        for (i = 0; i < runtime_overlay.count; i++) {
            int top = runtime_overlay.lines[i].y;
            int bottom = top + 16;

            if (!runtime_overlay.lines[i].selectable)
                continue;
            if (runtime_mouse_y >= top && runtime_mouse_y < bottom) {
                script_vars[SKY_VAR_THE_CHOSEN_ONE] =
                    runtime_overlay.lines[i].value;
                runtime_mouse_clicked = false;
                return;
            }
        }
    }

    if (runtime_mouse_clicked && hit) {
        uint16_t cpt_size;
        uint16_t cpt_type;
        uint16_t *compact = scummvm_sky_cpt_fetch_mutable(hit, &cpt_size,
                                                          &cpt_type, NULL);
        if (compact) {
            uint16_t get_to_flag = compact_word(compact, cpt_size,
                                                SKY_COMPACT_GET_TO_FLAG);
            uint16_t down_flag = compact_word(compact, cpt_size,
                                              SKY_COMPACT_DOWN_FLAG);
            if (get_to_flag)
                script_vars[SKY_VAR_THE_CHOSEN_ONE] = get_to_flag;
            else
                script_vars[SKY_VAR_THE_CHOSEN_ONE] = hit;
            if (down_flag)
                script_vars[SKY_VAR_OBJECT_HELD] = down_flag;
        }
        (void)cpt_type;
    }

    runtime_mouse_clicked = false;
}

static void runtime_vertical_mask(struct scummvm_video *video,
                                  const fb_data *palette,
                                  uint16_t block_x,
                                  uint16_t block_y,
                                  uint16_t block_w,
                                  uint16_t block_h)
{
    uint16_t layer_var;

    if (block_w == 0 || block_h == 0)
        return;

    for (layer_var = SKY_VAR_LAYER_1_ID;
         layer_var <= SKY_VAR_LAYER_3_ID;
         layer_var++) {
        uint16_t layer_id = (uint16_t)script_vars[layer_var];
        uint16_t grid_id = (uint16_t)script_vars[layer_var + 3];
        const struct scummvm_sky_resource *layer;
        const struct scummvm_sky_resource *grid;
        uint16_t x;

        if (!layer_id || !grid_id)
            continue;

        layer = runtime_get_resource(layer_id);
        grid = runtime_get_resource(grid_id);
        if (!layer || !grid)
            continue;

        for (x = 0; x < block_w; x++) {
            int by;

            for (by = (int)(block_y + block_h - 1);
                 by >= (int)block_y;
                 by--) {
                uint16_t grid_ofs;
                uint16_t tile;
                const uint8_t *src;
                uint16_t py;

                if (block_x + x >= 20 || by < 0 || by >= 24)
                    continue;

                grid_ofs = (uint16_t)(by * 20 + block_x + x);
                if ((uint32_t)grid_ofs * 2 + 1 >= grid->size)
                    continue;
                tile = read_le16(grid->data + (uint32_t)grid_ofs * 2);
                if (!tile)
                    break;
                if (tile & 0x8000)
                    continue;

                tile--;
                if ((uint32_t)(tile + 1) * 16U * 8U > layer->size)
                    continue;
                src = layer->data + (uint32_t)tile * 16U * 8U;
                for (py = 0; py < 8; py++) {
                    uint16_t px;
                    uint16_t sy = (uint16_t)(by * 8 + py);

                    if (sy >= 192)
                        break;
                    for (px = 0; px < 16; px++) {
                        uint16_t sx = (uint16_t)((block_x + x) * 16 + px);
                        uint8_t pix;

                        if (sx >= SCUMMVM_SURFACE_W)
                            break;
                        pix = src[py * 16 + px];
                        if (pix)
                            video->pixels[sy * SCUMMVM_SURFACE_W + sx] =
                                palette[pix];
                    }
                }
            }
        }
    }
}

static void runtime_draw_sprite(struct scummvm_video *video,
                                const fb_data *palette,
                                const uint16_t *compact,
                                uint16_t compact_size)
{
    const struct scummvm_sky_resource *resource;
    const uint8_t *header;
    const uint8_t *sprite_data;
    uint16_t frame;
    uint16_t file_nr;
    uint16_t frame_index;
    uint16_t width;
    uint16_t height;
    uint16_t sprite_size;
    int16_t offset_x;
    int16_t offset_y;
    int x;
    int y;
    uint16_t draw_x;
    uint16_t draw_y;
    uint16_t start_x = 0;
    uint16_t start_y = 0;
    uint16_t draw_w;
    uint16_t draw_h;
    uint16_t block_x;
    uint16_t block_y;
    uint16_t block_w;
    uint16_t block_h;
    uint16_t status;

    status = compact_word(compact, compact_size, SKY_COMPACT_STATUS);
    if (!(status & SKY_ST_DRAW_MASK))
        return;
    if (compact_word(compact, compact_size, SKY_COMPACT_SCREEN) !=
        (uint16_t)script_vars[SKY_VAR_SCREEN])
        return;

    frame = compact_word(compact, compact_size, SKY_COMPACT_FRAME);
    file_nr = frame >> 6;
    frame_index = frame & 0x3f;
    if (file_nr == 0)
        return;

    resource = runtime_get_resource(file_nr);
    if (!resource || resource->size < 22)
        return;

    header = resource->data;
    width = read_le16(header + 6);
    height = read_le16(header + 8);
    sprite_size = read_le16(header + 10);
    offset_x = read_sle16(header + 16);
    offset_y = read_sle16(header + 18);

    if (width == 0 || height == 0 || sprite_size == 0)
        return;
    if ((uint32_t)22 + (uint32_t)(frame_index + 1) * sprite_size >
        resource->size)
        return;

    sprite_data = resource->data + 22 + (uint32_t)frame_index * sprite_size;
    x = (int)compact_word(compact, compact_size, SKY_COMPACT_X) +
        offset_x - SKY_TOP_LEFT_X;
    y = (int)compact_word(compact, compact_size, SKY_COMPACT_Y) +
        offset_y - SKY_TOP_LEFT_Y;

    draw_w = width;
    draw_h = height;
    if (x < 0) {
        if ((uint16_t)(-x) >= draw_w)
            return;
        start_x = (uint16_t)(-x);
        draw_w -= start_x;
        x = 0;
    }
    if (y < 0) {
        if ((uint16_t)(-y) >= draw_h)
            return;
        start_y = (uint16_t)(-y);
        draw_h -= start_y;
        y = 0;
    }
    if (x + draw_w > SCUMMVM_SURFACE_W)
        draw_w = (uint16_t)(SCUMMVM_SURFACE_W - x);
    if (y + draw_h > 192)
        draw_h = (uint16_t)(192 - y);
    if (draw_w == 0 || draw_h == 0)
        return;

    for (draw_y = 0; draw_y < draw_h; draw_y++) {
        uint32_t src = (uint32_t)(start_y + draw_y) * width + start_x;
        uint32_t dst = (uint32_t)(y + draw_y) * SCUMMVM_SURFACE_W + x;

        for (draw_x = 0; draw_x < draw_w; draw_x++) {
            uint8_t pix = sprite_data[src + draw_x];

            if (pix)
                video->pixels[dst + draw_x] = palette[pix];
        }
    }

    block_x = (uint16_t)(x >> 4);
    block_y = (uint16_t)(y >> 3);
    block_w = (uint16_t)(((x + draw_w + 15) >> 4) - block_x);
    block_h = (uint16_t)(((y + draw_h + 7) >> 3) - block_y);
    if ((status & SKY_ST_BACKGROUND) ||
        ((status & SKY_ST_SORT) && !(status & SKY_ST_NO_VMASK)))
        runtime_vertical_mask(video, palette, block_x, block_y,
                              block_w, block_h);
}

static bool runtime_sprite_sort_y(const uint16_t *compact,
                                  uint16_t compact_size,
                                  uint16_t *sort_y)
{
    const struct scummvm_sky_resource *resource;
    const uint8_t *header;
    uint16_t frame;
    uint16_t file_nr;
    uint16_t height;
    int16_t offset_y;
    int32_t y;

    frame = compact_word(compact, compact_size, SKY_COMPACT_FRAME);
    file_nr = frame >> 6;
    if (file_nr == 0)
        return false;

    resource = runtime_get_resource(file_nr);
    if (!resource || resource->size < 22)
        return false;

    header = resource->data;
    height = read_le16(header + 8);
    offset_y = read_sle16(header + 18);
    y = (int32_t)compact_word(compact, compact_size, SKY_COMPACT_Y) +
        offset_y + height;
    if (y < 0)
        y = 0;
    else if (y > 0xffff)
        y = 0xffff;

    *sort_y = (uint16_t)y;
    return true;
}

static void runtime_draw_compact_id(struct scummvm_video *video,
                                    const fb_data *palette,
                                    uint16_t id,
                                    uint16_t layer_mask)
{
    const uint16_t *compact;
    uint16_t cpt_size;
    uint16_t cpt_type;

    compact = scummvm_sky_cpt_fetch(id, &cpt_size, &cpt_type, NULL);
    if (compact &&
        (compact_word(compact, cpt_size, SKY_COMPACT_STATUS) &
         layer_mask))
        runtime_draw_sprite(video, palette, compact, cpt_size);
    (void)cpt_type;
}

static void runtime_walk_draw_list_layer(struct scummvm_video *video,
                                         const fb_data *palette,
                                         uint16_t draw_list_id,
                                         uint16_t layer_mask)
{
    const uint16_t *draw_list;
    uint16_t list_size;
    uint16_t list_type;
    uint16_t pos = 0;
    uint32_t guard = 0;

    draw_list = scummvm_sky_cpt_fetch(draw_list_id, &list_size, &list_type,
                                      NULL);
    while (draw_list && guard++ < SKY_MAX_LOGIC_SCAN) {
        uint16_t id;

        if (pos >= list_size)
            break;

        id = draw_list[pos++];
        if (id == 0)
            break;

        if (id == 0xffff) {
            if (pos >= list_size)
                break;
            draw_list = scummvm_sky_cpt_fetch(draw_list[pos++],
                                              &list_size,
                                              &list_type,
                                              NULL);
            pos = 0;
            continue;
        }

        runtime_draw_compact_id(video, palette, id, layer_mask);
    }
    (void)list_type;
}

struct runtime_sort_sprite {
    uint16_t id;
    uint16_t y;
};

static void runtime_draw_sorted_list(struct scummvm_video *video,
                                     const fb_data *palette,
                                     uint16_t draw_list_id)
{
    const uint16_t *draw_list;
    uint16_t list_size;
    uint16_t list_type;
    uint16_t pos = 0;
    uint32_t guard = 0;
    struct runtime_sort_sprite sorted[30];
    uint16_t count = 0;
    uint16_t i;

    draw_list = scummvm_sky_cpt_fetch(draw_list_id, &list_size, &list_type,
                                      NULL);
    while (draw_list && guard++ < SKY_MAX_LOGIC_SCAN) {
        uint16_t id;
        const uint16_t *compact;
        uint16_t cpt_size;
        uint16_t cpt_type;

        if (pos >= list_size)
            break;

        id = draw_list[pos++];
        if (id == 0)
            break;

        if (id == 0xffff) {
            if (pos >= list_size)
                break;
            draw_list = scummvm_sky_cpt_fetch(draw_list[pos++],
                                              &list_size,
                                              &list_type,
                                              NULL);
            pos = 0;
            continue;
        }

        compact = scummvm_sky_cpt_fetch(id, &cpt_size, &cpt_type, NULL);
        if (compact &&
            count < ARRAYLEN(sorted) &&
            (compact_word(compact, cpt_size, SKY_COMPACT_STATUS) &
             SKY_ST_SORT) &&
            compact_word(compact, cpt_size, SKY_COMPACT_SCREEN) ==
             (uint16_t)script_vars[SKY_VAR_SCREEN] &&
            runtime_sprite_sort_y(compact, cpt_size, &sorted[count].y)) {
            sorted[count].id = id;
            count++;
        }
        (void)cpt_type;
    }

    for (i = 0; i < count; i++) {
        uint16_t j;

        for (j = (uint16_t)(i + 1); j < count; j++) {
            if (sorted[i].y > sorted[j].y) {
                struct runtime_sort_sprite tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }

    for (i = 0; i < count; i++)
        runtime_draw_compact_id(video, palette, sorted[i].id, SKY_ST_SORT);
    (void)list_type;
}

static void runtime_draw_draw_lists(struct scummvm_video *video,
                                    const fb_data *palette)
{
    uint16_t var;

    if (!script_vars[SKY_VAR_DRAW_LIST_NO])
        return;

    for (var = SKY_VAR_DRAW_LIST_NO;
         var < SKY_NUM_SCRIPT_VARS && script_vars[var];
         var++) {
        uint16_t draw_list_id = (uint16_t)script_vars[var];

        runtime_walk_draw_list_layer(video, palette, draw_list_id,
                                     SKY_ST_BACKGROUND);
        runtime_draw_sorted_list(video, palette, draw_list_id);
        runtime_walk_draw_list_layer(video, palette, draw_list_id,
                                     SKY_ST_FOREGROUND);
    }
}

static void runtime_draw_logic_sprites(struct scummvm_video *video,
                                       const fb_data *palette)
{
    const uint16_t *logic_list;
    uint16_t list_size;
    uint16_t list_type;
    uint16_t pos = 0;
    uint32_t guard = 0;

    logic_list = scummvm_sky_cpt_fetch((uint16_t)script_vars[SKY_VAR_LOGIC_LIST_NO],
                                       &list_size, &list_type, NULL);
    if (!logic_list || list_type != SKY_CPT_MAINLIST)
        return;

    while (guard++ < SKY_MAX_LOGIC_SCAN) {
        uint16_t id;
        uint16_t cpt_size;
        uint16_t cpt_type;
        const uint16_t *compact;

        if (pos >= list_size)
            break;

        id = logic_list[pos++];
        if (id == 0)
            break;

        if (id == 0xffff) {
            if (pos >= list_size)
                break;
            logic_list = scummvm_sky_cpt_fetch(logic_list[pos++],
                                               &list_size, &list_type, NULL);
            pos = 0;
            if (!logic_list || list_type != SKY_CPT_MAINLIST)
                break;
            continue;
        }

        compact = scummvm_sky_cpt_fetch(id, &cpt_size, &cpt_type, NULL);
        if (compact)
            runtime_draw_sprite(video, palette, compact, cpt_size);
        (void)cpt_type;
    }
}

bool scummvm_sky_runtime_init(const struct scummvm_target *target,
                              char *status,
                              size_t status_size)
{
    struct scummvm_sky_runtime_info info;
    bool restored;

    runtime_target = target;
    init_script_variables();
    runtime_overlay_clear();
    script_vars[SKY_VAR_CUR_SECTION] = 0;

    if (!scummvm_sky_text_init(target, status, status_size)) {
        runtime_ready = false;
        return false;
    }

    if (!load_script_module(0, status, status_size)) {
        runtime_ready = false;
        return false;
    }

    restored = runtime_load_snapshot();

    if (!scummvm_sky_runtime_scan(&info, status, status_size)) {
        runtime_ready = false;
        return false;
    }

    if (!restored &&
        !decode_first_active_script(&info, status, status_size)) {
        runtime_ready = false;
        return false;
    }

    if (restored)
        rb->strlcpy(status, "Sky autosave restored", status_size);

    runtime_ready = true;
    return true;
}

void scummvm_sky_runtime_input(int x, int y, bool down, bool clicked)
{
    if (x < 0)
        x = 0;
    else if (x >= SCUMMVM_SURFACE_W)
        x = SCUMMVM_SURFACE_W - 1;

    if (y < 0)
        y = 0;
    else if (y >= SCUMMVM_SURFACE_H)
        y = SCUMMVM_SURFACE_H - 1;

    runtime_mouse_x = x;
    runtime_mouse_y = y;
    runtime_mouse_down = down;
    runtime_mouse_clicked |= clicked;
}

bool scummvm_sky_runtime_overlay(struct scummvm_sky_overlay *overlay)
{
    if (!overlay || !runtime_ready)
        return false;

    *overlay = runtime_overlay;
    return runtime_overlay.count > 0;
}

bool scummvm_sky_runtime_step(char *status, size_t status_size)
{
    struct scummvm_sky_runtime_info info;

    if (!runtime_ready) {
        rb->strlcpy(status, "Sky runtime is not initialized", status_size);
        return false;
    }

    if (!scummvm_sky_runtime_scan(&info, status, status_size))
        return false;

    runtime_audio_tick_queue();
    runtime_process_input();

    (void)info;
    return runtime_step_logic_list(status, status_size);
}
