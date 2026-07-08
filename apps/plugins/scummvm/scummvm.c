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
    enum plugin_status status;

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
    plugin_cxx_init(cxx_buffer, cxx_buffer_size);
    {
        char msg[48];
        rb->snprintf(msg, sizeof(msg), "heap %luK",
                     (unsigned long)(cxx_buffer_size / 1024));
        launch_status("heap ready", msg);
    }
    DEBUGF("scummvm: cxx heap=%zu avail=%zu\n",
           cxx_buffer_size, plugin_cxx_available());

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
    status = scummvm_backend_run(&target);
    rb->plugin_release_audio_buffer();
    return status;
}
