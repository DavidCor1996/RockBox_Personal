/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
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
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#include "scummvm.h"
#include "lib/plugin_cxx.h"
#include <ctype.h>

#define SCUMMVM_MAX_VALUE        MAX_PATH
#define SCUMMVM_ENGINE_STACK_SIZE (64 * 1024)
#define SCUMMVM_ENGINE_STACK_ALIGN 16u

/* Adventure engines have substantially deeper frame/input call chains than
 * Rockbox's 8 KiB main thread permits.  Keep the plugin entrypoint on the
 * main thread for heap and lifecycle ownership, but run the engine on a
 * dedicated stack carved from the shared audio buffer while ScummVM owns it.
 * This leaves the nearly-full plugin image unchanged and still gives the C++
 * heap every byte after the explicitly bounded stack reserve. */
static void *scummvm_engine_stack;
static const struct scummvm_target *scummvm_engine_target;
static volatile enum plugin_status scummvm_engine_status;

static void scummvm_engine_thread(void)
{
    scummvm_engine_status = scummvm_backend_run(scummvm_engine_target);
}

static void launch_status(const char *line1, const char *line2)
{
    rb->lcd_clear_display();
    rb->lcd_putsxy(4, 12, "ScummVM");
    rb->lcd_putsxy(4, 34, line1 ? line1 : "");
    if (line2)
        rb->lcd_putsxy(4, 52, line2);
    rb->lcd_update();
}

static void trim_whitespace(char **start, char **end)
{
    while (*start < *end && isspace((unsigned char)**start))
        (*start)++;

    while (*end > *start && isspace((unsigned char)*((*end) - 1)))
        (*end)--;

    **end = '\0';
}

static void ensure_default_save_dirs(void)
{
    if (!rb->dir_exists(ROCKBOX_DIR "/scummvm"))
        rb->mkdir(ROCKBOX_DIR "/scummvm");

    if (!rb->dir_exists(SCUMMVM_DEFAULT_SAVE_DIR))
        rb->mkdir(SCUMMVM_DEFAULT_SAVE_DIR);

    if (!rb->dir_exists(SCUMMVM_ENGINE_DATA_DIR))
        rb->mkdir(SCUMMVM_ENGINE_DATA_DIR);
}

static bool parse_kv_line(char *line, char **key, char **value)
{
    char *eq = rb->strchr(line, '=');
    char *end;

    if (!eq)
        return false;

    *eq = '\0';
    *key = line;
    end = eq;
    trim_whitespace(key, &end);

    *value = eq + 1;
    end = *value + rb->strlen(*value);
    trim_whitespace(value, &end);

    return **key != '\0';
}

static void apply_descriptor_value(struct scummvm_target *target,
                                   const char *key, const char *value)
{
    if (!rb->strcasecmp(key, "gameid"))
        rb->strlcpy(target->gameid, value, sizeof(target->gameid));
    else if (!rb->strcasecmp(key, "engine"))
        rb->strlcpy(target->engine, value, sizeof(target->engine));
    else if (!rb->strcasecmp(key, "path"))
        rb->strlcpy(target->path, value, sizeof(target->path));
    else if (!rb->strcasecmp(key, "savepath"))
        rb->strlcpy(target->savepath, value, sizeof(target->savepath));
}

static bool load_descriptor(const char *filename, struct scummvm_target *target)
{
    int fd;
    char line[SCUMMVM_MAX_VALUE];

    rb->memset(target, 0, sizeof(*target));
    rb->strlcpy(target->savepath, SCUMMVM_DEFAULT_SAVE_DIR,
                sizeof(target->savepath));

    fd = rb->open(filename, O_RDONLY);
    if (fd < 0) {
        rb->splash(HZ * 2, "Cannot open descriptor");
        return false;
    }

    while (rb->read_line(fd, line, sizeof(line)) > 0) {
        char *cursor = line;
        char *key;
        char *value;

        while (isspace((unsigned char)*cursor))
            cursor++;

        if (*cursor == '\0' || *cursor == '#')
            continue;

        if (parse_kv_line(cursor, &key, &value))
            apply_descriptor_value(target, key, value);
    }

    rb->close(fd);
    return true;
}

static bool validate_target(struct scummvm_target *target)
{
    if (target->gameid[0] == '\0') {
        rb->splash(HZ * 2, "Missing gameid");
        return false;
    }

    if (target->engine[0] == '\0') {
        rb->splash(HZ * 2, "Missing engine");
        return false;
    }

    if (target->path[0] == '\0') {
        if (rb->snprintf(target->path, sizeof(target->path),
                         "/ScummVM/%s", target->gameid) >=
            (int)sizeof(target->path)) {
            rb->splash(HZ * 2, "Missing path");
            return false;
        }
    }

    if (!rb->dir_exists(target->path)) {
        rb->splashf(HZ * 3, "Missing data: %s", target->path);
        return false;
    }

    ensure_default_save_dirs();

    if (target->savepath[0] != '\0' && !rb->dir_exists(target->savepath))
        rb->mkdir(target->savepath);

    return true;
}

enum plugin_status plugin_start(const void *parameter)
{
    struct scummvm_target target;
    size_t cxx_buffer_size = 0;
    void *cxx_buffer;
    void *cxx_heap;
    size_t cxx_heap_size;
    uintptr_t buffer_start;
    uintptr_t buffer_end;
    uintptr_t stack_start;
    uintptr_t heap_start;
    enum plugin_status status;
    unsigned int thread_id;
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    bool boost_engine;
#endif

    if (parameter == NULL) {
        rb->splash(HZ * 3, "Open a .scummvm file");
        return PLUGIN_ERROR;
    }

    launch_status("request heap", (const char *)parameter);
    cxx_buffer = rb->plugin_get_audio_buffer(&cxx_buffer_size);
    if (!cxx_buffer || cxx_buffer_size == 0) {
        rb->splash(HZ * 2, "No ScummVM heap");
        return PLUGIN_ERROR;
    }
    buffer_start = (uintptr_t)cxx_buffer;
    buffer_end = buffer_start + cxx_buffer_size;
    stack_start = (buffer_start + SCUMMVM_ENGINE_STACK_ALIGN - 1u) &
                  ~(uintptr_t)(SCUMMVM_ENGINE_STACK_ALIGN - 1u);
    heap_start = (stack_start + SCUMMVM_ENGINE_STACK_SIZE +
                  SCUMMVM_ENGINE_STACK_ALIGN - 1u) &
                 ~(uintptr_t)(SCUMMVM_ENGINE_STACK_ALIGN - 1u);
    if (heap_start >= buffer_end ||
        buffer_end - heap_start < SCUMMVM_ENGINE_STACK_SIZE) {
        rb->splash(HZ * 2, "ScummVM memory is too small");
        rb->plugin_release_audio_buffer();
        return PLUGIN_ERROR;
    }
    scummvm_engine_stack = (void *)stack_start;
    cxx_heap = (void *)heap_start;
    cxx_heap_size = buffer_end - heap_start;
    plugin_cxx_init(cxx_heap, cxx_heap_size);
    {
        char msg[48];
        rb->snprintf(msg, sizeof(msg), "stack 64K heap %luK",
                     (unsigned long)(cxx_heap_size / 1024));
        launch_status("heap ready", msg);
    }
    DEBUGF("scummvm: engine stack=%p bytes=%u cxx heap=%p bytes=%zu "
           "avail=%zu\n", scummvm_engine_stack,
           (unsigned)SCUMMVM_ENGINE_STACK_SIZE, cxx_heap, cxx_heap_size,
           plugin_cxx_available());

    launch_status("load descriptor", (const char *)parameter);
    DEBUGF("scummvm: loading descriptor %s\n", (const char *)parameter);
    if (!load_descriptor(parameter, &target)) {
        DEBUGF("scummvm: descriptor load failed\n");
        rb->plugin_release_audio_buffer();
        return PLUGIN_ERROR;
    }

    DEBUGF("scummvm: target game=%s engine=%s path=%s save=%s\n",
           target.gameid, target.engine, target.path, target.savepath);
    launch_status("validate target", target.path);
    if (!validate_target(&target)) {
        DEBUGF("scummvm: target validation failed\n");
        rb->plugin_release_audio_buffer();
        return PLUGIN_ERROR;
    }

    launch_status("enter backend", target.engine);
    DEBUGF("scummvm: entering backend\n");
    scummvm_engine_target = &target;
    scummvm_engine_status = PLUGIN_ERROR;
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    boost_engine = !rb->strcasecmp(target.engine, "agds") ||
                   !rb->strcasecmp(target.engine, "nibiru");
    if (boost_engine)
        rb->cpu_boost(true);
#endif
    thread_id = rb->create_thread(
        scummvm_engine_thread, scummvm_engine_stack,
        SCUMMVM_ENGINE_STACK_SIZE, 0, "ScummVM engine"
        IF_PRIO(, PRIORITY_USER_INTERFACE) IF_COP(, CPU));
    if (thread_id == 0 || thread_id == UINT_MAX) {
        rb->splash(HZ * 2, "Cannot start ScummVM engine");
        status = PLUGIN_ERROR;
    } else {
        rb->thread_wait(thread_id);
        status = scummvm_engine_status;
    }
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    if (boost_engine)
        rb->cpu_boost(false);
#endif
    scummvm_engine_target = NULL;
    scummvm_engine_stack = NULL;
    rb->plugin_release_audio_buffer();
    return status;
}
