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

#define SKY_NUM_SCRIPT_VARS 838
#define SKY_MAX_LOGIC_SCAN 2048
#define SKY_MAX_SCRIPT_MODULES 16
#define SKY_MAX_CACHE_ITEMS 300
#define SKY_MAX_CACHE_LIST 60
#define SKY_CPT_MAINLIST 7
#define SKY_FILE_MODULE_0 60400

#define SKY_VAR_SCREEN 1
#define SKY_VAR_LOGIC_LIST_NO 2
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
#define SKY_ST_LOGIC 64
#define SKY_LOGIC_SCRIPT 1
#define SKY_SCRIPT_MAX_STEPS 256
#define SKY_SCRIPT_STACK_SIZE 16

static uint32_t script_vars[SKY_NUM_SCRIPT_VARS];
static const struct scummvm_target *runtime_target;
static struct scummvm_sky_resource script_modules[SKY_MAX_SCRIPT_MODULES];
static struct scummvm_sky_resource runtime_screen;
static struct scummvm_sky_resource cached_items[SKY_MAX_CACHE_ITEMS];
static uint16_t cache_build_list[SKY_MAX_CACHE_LIST];
static uint16_t runtime_palette_id;
static bool runtime_ready;

static bool runtime_mcode(uint16_t mcode,
                          uint32_t a,
                          uint32_t b,
                          uint32_t c,
                          char *status,
                          size_t status_size);

static uint16_t compact_word(const uint16_t *compact, uint16_t size,
                             uint16_t index)
{
    if (!compact || index >= size)
        return 0;

    return compact[index];
}

void scummvm_sky_runtime_reset(void)
{
    uint32_t i;

    for (i = 0; i < ARRAYLEN(script_modules); i++)
        scummvm_sky_loader_release_resource(&script_modules[i]);
    scummvm_sky_loader_release_resource(&runtime_screen);
    for (i = 0; i < ARRAYLEN(cached_items); i++)
        scummvm_sky_loader_release_resource(&cached_items[i]);

    rb->memset(script_vars, 0, sizeof(script_vars));
    rb->memset(script_modules, 0, sizeof(script_modules));
    rb->memset(&runtime_screen, 0, sizeof(runtime_screen));
    rb->memset(cached_items, 0, sizeof(cached_items));
    rb->memset(cache_build_list, 0, sizeof(cache_build_list));
    runtime_palette_id = 0;
    runtime_target = NULL;
    runtime_ready = false;
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
                                 uint16_t *script_off)
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
    return *script_no != 0;
}

static bool decode_first_active_script(
    const struct scummvm_sky_runtime_info *info,
    char *status,
    size_t status_size)
{
    const uint16_t *compact;
    const struct scummvm_sky_resource *module;
    const char *name = NULL;
    uint16_t cpt_size;
    uint16_t cpt_type;
    uint16_t logic;
    uint16_t script_no;
    uint16_t script_off;
    uint16_t module_no;
    uint16_t script_index;
    uint32_t module_words;
    uint32_t pc;
    uint16_t opcode;
    uint32_t stack[SKY_SCRIPT_STACK_SIZE];
    uint32_t stack_pos = 0;
    uint32_t steps = 0;

    compact = scummvm_sky_cpt_fetch(info->first_logic_id,
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

    if (!compact_script_slots(compact, cpt_size, &script_no, &script_off)) {
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
            if (!runtime_mcode(mcode, a, b, c, status, status_size))
                return false;
            if (mcode > 2)
                return true;
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

static bool runtime_mcode(uint16_t mcode,
                          uint32_t a,
                          uint32_t b,
                          uint32_t c,
                          char *status,
                          size_t status_size)
{
    switch (mcode) {
    case 0:
        return runtime_cache_chip((uint16_t)a, status, status_size);
    case 1:
        return runtime_cache_fast((uint16_t)a, status, status_size);
    case 2:
        return runtime_draw_screen((uint16_t)a, status, status_size);
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

bool scummvm_sky_runtime_init(const struct scummvm_target *target,
                              char *status,
                              size_t status_size)
{
    struct scummvm_sky_runtime_info info;

    runtime_target = target;
    init_script_variables();
    script_vars[SKY_VAR_CUR_SECTION] = 0;

    if (!load_script_module(0, status, status_size)) {
        runtime_ready = false;
        return false;
    }

    if (!scummvm_sky_runtime_scan(&info, status, status_size)) {
        runtime_ready = false;
        return false;
    }

    if (!decode_first_active_script(&info, status, status_size)) {
        runtime_ready = false;
        return false;
    }

    runtime_ready = true;
    return true;
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

    return decode_first_active_script(&info, status, status_size);
}
