/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 The Rockbox Community
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

#include "plugin.h"
#include "file.h"
#include <tlsf.h>
#include "mujs.h"

#define MUJS_DEFAULT_SCRIPT ROCKBOX_DIR "/scripts/theme_helper.js"
#define MUJS_SCRIPT_DIR ROCKBOX_DIR "/scripts"
#define MUJS_DATA_DIR ROCKBOX_DIR "/scripts/data"
#define MUJS_THEME_DIR ROCKBOX_DIR "/themes"
#define MUJS_WPS_DIR ROCKBOX_DIR "/wps"
#define MUJS_FONTS_DIR ROCKBOX_DIR "/fonts"
#define MUJS_ICONS_DIR ROCKBOX_DIR "/icons"
#define MUJS_BACKDROPS_DIR ROCKBOX_DIR "/backdrops"
#define MUJS_CONFIG_FILE ROCKBOX_DIR "/config.cfg"
#define MUJS_SOURCE_LIMIT (48 * 1024)
#define MUJS_TEXT_LIMIT (64 * 1024)
#define MUJS_RUN_LIMIT 120000
#define MUJS_MEM_LIMIT (768 * 1024)

static char source_buf[MUJS_SOURCE_LIMIT + 1];
static char text_buf[MUJS_TEXT_LIMIT + 1];
static char path_buf[MAX_PATH];
static char last_error[96];
static void *pool;
static size_t pool_size;
static int line;

static void mujs_put_line(const char *msg)
{
    rb->lcd_puts_scroll(0, line, msg);
    line++;
    if ((unsigned)line >= LCD_HEIGHT / rb->font_get(FONT_UI)->height)
        line = 0;
    rb->lcd_update();
}

static bool has_dotdot(const char *path)
{
    return rb->strstr(path, "../") || rb->strstr(path, "/..") ||
           !rb->strcmp(path, "..");
}

static bool starts_with(const char *s, const char *prefix)
{
    return rb->strncmp(s, prefix, rb->strlen(prefix)) == 0;
}

static bool is_read_root(const char *path)
{
    return !rb->strcmp(path, MUJS_SCRIPT_DIR) ||
           !rb->strcmp(path, MUJS_THEME_DIR) ||
           !rb->strcmp(path, MUJS_WPS_DIR) ||
           !rb->strcmp(path, MUJS_FONTS_DIR) ||
           !rb->strcmp(path, MUJS_ICONS_DIR) ||
           !rb->strcmp(path, MUJS_BACKDROPS_DIR) ||
           !rb->strcmp(path, MUJS_CONFIG_FILE) ||
           starts_with(path, MUJS_SCRIPT_DIR "/") ||
           starts_with(path, MUJS_THEME_DIR "/") ||
           starts_with(path, MUJS_WPS_DIR "/") ||
           starts_with(path, MUJS_FONTS_DIR "/") ||
           starts_with(path, MUJS_ICONS_DIR "/") ||
           starts_with(path, MUJS_BACKDROPS_DIR "/");
}

static bool is_rockbox_relative(const char *path)
{
    return !rb->strcmp(path, "themes") ||
           !rb->strcmp(path, "wps") ||
           !rb->strcmp(path, "fonts") ||
           !rb->strcmp(path, "icons") ||
           !rb->strcmp(path, "backdrops") ||
           !rb->strcmp(path, "config.cfg") ||
           starts_with(path, "themes/") ||
           starts_with(path, "wps/") ||
           starts_with(path, "fonts/") ||
           starts_with(path, "icons/") ||
           starts_with(path, "backdrops/");
}

static const char *resolve_path(js_State *J, const char *path, bool write)
{
    if (!path || !path[0] || has_dotdot(path))
        js_error(J, "bad path");

    if (path[0] == '/')
        rb->strlcpy(path_buf, path, sizeof(path_buf));
    else if (is_rockbox_relative(path))
        rb->snprintf(path_buf, sizeof(path_buf), "%s/%s", ROCKBOX_DIR, path);
    else
        rb->snprintf(path_buf, sizeof(path_buf), "%s/%s", MUJS_SCRIPT_DIR, path);

    if (write)
    {
        if (!starts_with(path_buf, MUJS_DATA_DIR "/"))
            js_error(J, "writes are limited to %s", MUJS_DATA_DIR);
    }
    else if (!is_read_root(path_buf))
    {
        js_error(J, "path is outside the script sandbox");
    }

    return path_buf;
}

static int read_text_file(const char *path, char *buf, size_t limit)
{
    int fd;
    ssize_t got;

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return fd;

    got = rb->read(fd, buf, limit);
    rb->close(fd);
    if (got < 0)
        return (int)got;

    buf[got] = '\0';
    return (int)got;
}

static void *mujs_alloc(void *ctx, void *ptr, int size)
{
    (void)ctx;

    if (size == 0)
    {
        tlsf_free(ptr);
        return NULL;
    }

    return tlsf_realloc(ptr, size);
}

static void mujs_report(js_State *J, const char *message)
{
    (void)J;
    rb->strlcpy(last_error, message, sizeof(last_error));
}

static void rb_print(js_State *J)
{
    const char *msg = js_trystring(J, 1, "");
    mujs_put_line(msg);
    js_pushundefined(J);
}

static void rb_clear(js_State *J)
{
    (void)J;
    line = 0;
    rb->lcd_clear_display();
    rb->lcd_update();
    js_pushundefined(J);
}

static void rb_ticks(js_State *J)
{
    js_pushnumber(J, *rb->current_tick);
}

static void rb_gc(js_State *J)
{
    js_gc(J, 0);
    js_pushundefined(J);
}

static void rb_alert(js_State *J)
{
    const char *msg = js_trystring(J, 1, "");
    rb->splashf(HZ, "%s", msg);
    js_pushundefined(J);
}

static void rb_quit(js_State *J)
{
    js_error(J, "quit");
}

static void rb_read_text(js_State *J)
{
    const char *path = resolve_path(J, js_trystring(J, 1, ""), false);
    int got = read_text_file(path, text_buf, MUJS_TEXT_LIMIT);

    if (got < 0)
        js_error(J, "cannot read %s", path);

    js_pushlstring(J, text_buf, got);
}

static void rb_write_text(js_State *J)
{
    const char *path = resolve_path(J, js_trystring(J, 1, ""), true);
    const char *text = js_trystring(J, 2, "");
    int fd = rb->open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (fd < 0)
        js_error(J, "cannot write %s", path);

    rb->write(fd, text, rb->strlen(text));
    rb->close(fd);
    js_pushundefined(J);
}

static void rb_exists(js_State *J)
{
    const char *path = resolve_path(J, js_trystring(J, 1, ""), false);
    js_pushboolean(J, rb->file_exists(path) || rb->dir_exists(path));
}

static void rb_list_files(js_State *J)
{
    const char *path = resolve_path(J, js_trystring(J, 1, ""), false);
    DIR *dir = rb->opendir(path);
    struct dirent *entry;
    int index = 0;

    js_newarray(J);
    if (!dir)
        return;

    while ((entry = rb->readdir(dir)))
    {
        if (!rb->strcmp(entry->d_name, ".") || !rb->strcmp(entry->d_name, ".."))
            continue;

        js_pushstring(J, entry->d_name);
        js_setindex(J, -2, index++);
    }

    rb->closedir(dir);
}

static void bind_rb(js_State *J)
{
    js_newobject(J);

    js_newcfunction(J, rb_print, "print", 1);
    js_setproperty(J, -2, "print");
    js_newcfunction(J, rb_clear, "clear", 0);
    js_setproperty(J, -2, "clear");
    js_newcfunction(J, rb_ticks, "ticks", 0);
    js_setproperty(J, -2, "ticks");
    js_newcfunction(J, rb_gc, "gc", 0);
    js_setproperty(J, -2, "gc");
    js_newcfunction(J, rb_alert, "alert", 1);
    js_setproperty(J, -2, "alert");
    js_newcfunction(J, rb_read_text, "readText", 1);
    js_setproperty(J, -2, "readText");
    js_newcfunction(J, rb_write_text, "writeText", 2);
    js_setproperty(J, -2, "writeText");
    js_newcfunction(J, rb_exists, "exists", 1);
    js_setproperty(J, -2, "exists");
    js_newcfunction(J, rb_list_files, "listFiles", 1);
    js_setproperty(J, -2, "listFiles");
    js_newcfunction(J, rb_quit, "quit", 0);
    js_setproperty(J, -2, "quit");

    js_setglobal(J, "rb");
}

enum plugin_status plugin_start(const void *parameter)
{
    const char *script = parameter ? parameter : MUJS_DEFAULT_SCRIPT;
    js_State *J;
    int err;

    rb->lcd_clear_display();
    mujs_put_line("MuJS");

    err = read_text_file(script, source_buf, MUJS_SOURCE_LIMIT);
    if (err < 0)
    {
        rb->splashf(HZ * 2, "No script: %s", script);
        return PLUGIN_ERROR;
    }
    if (err == MUJS_SOURCE_LIMIT)
    {
        rb->splashf(HZ * 2, "Script too large");
        return PLUGIN_ERROR;
    }

    pool = rb->plugin_get_buffer(&pool_size);
    if (!pool || pool_size < MUJS_MEM_LIMIT)
    {
        rb->splashf(HZ * 2, "Not enough plugin memory");
        return PLUGIN_ERROR;
    }

    init_memory_pool(pool_size, pool);

    J = js_newstate(mujs_alloc, NULL, JS_STRICT);
    if (!J)
    {
        rb->splashf(HZ * 2, "MuJS init failed");
        return PLUGIN_ERROR;
    }

    last_error[0] = '\0';
    js_setreport(J, mujs_report);
    js_setlimit(J, MUJS_RUN_LIMIT, MUJS_MEM_LIMIT);
    bind_rb(J);

    err = js_dostring(J, source_buf);
    if (err)
    {
        const char *msg = last_error[0] ? last_error : js_trystring(J, -1, "script error");
        rb->splashf(HZ * 3, "%s", msg);
        js_freestate(J);
        return PLUGIN_ERROR;
    }

    js_freestate(J);
    rb->splashf(HZ, "MuJS done");
    return PLUGIN_OK;
}
