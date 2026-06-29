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
 ****************************************************************************/

#include "scummvm.h"
#include "engine.h"
#include "probe.h"
#include "sky_cpt_loader.h"
#include "sky_runtime.h"
#include "upstream_bridge.h"
#include "video.h"
#include "lib/pluginlib_actions.h"

#define SCUMMVM_CURSOR_STEP 4
#define SCUMMVM_CURSOR_FAST_STEP 12

struct scummvm_backend {
    const struct scummvm_target *target;
    struct scummvm_probe_result probe;
    struct scummvm_engine_state engine;
    struct scummvm_video video;
    int cursor_x;
    int cursor_y;
    bool mouse_down;
    bool menu_open;
};

static const struct button_mapping *scummvm_contexts[] = {
    pla_main_ctx,
#if defined(HAVE_REMOTE_LCD)
    pla_remote_ctx,
#endif
};

static void clamp_cursor(struct scummvm_backend *backend)
{
    if (backend->cursor_x < 0)
        backend->cursor_x = 0;
    else if (backend->cursor_x >= SCUMMVM_SURFACE_W)
        backend->cursor_x = SCUMMVM_SURFACE_W - 1;

    if (backend->cursor_y < 0)
        backend->cursor_y = 0;
    else if (backend->cursor_y >= SCUMMVM_SURFACE_H)
        backend->cursor_y = SCUMMVM_SURFACE_H - 1;
}

static void move_cursor(struct scummvm_backend *backend, int dx, int dy)
{
    backend->cursor_x += dx;
    backend->cursor_y += dy;
    clamp_cursor(backend);
}

static void draw_cursor(const struct scummvm_backend *backend, int x, int y)
{
    (void)backend;
    rb->lcd_drawline(x - 4, y, x + 4, y);
    rb->lcd_drawline(x, y - 4, x, y + 4);
    rb->lcd_drawrect(x - 2, y - 2, 5, 5);
}

static void draw_backend_status(const struct scummvm_backend *backend)
{
    rb->lcd_putsf(0, 0, "ScummVM: %s", backend->target->gameid);
    rb->lcd_putsf(0, 1, "Engine: %s %s",
                  backend->probe.engine_name,
                  backend->probe.supported ? "" : "(blocked)");
    rb->lcd_putsf(0, 2, "Mouse: %03d,%03d %s",
                  backend->cursor_x, backend->cursor_y,
                  backend->mouse_down ? "down" : "up");
    rb->lcd_putsf(0, 3, "Data: %s",
                  backend->probe.data_found ? backend->probe.variant :
                  backend->probe.detail);
    rb->lcd_putsf(0, 4, "Engine data: %s",
                  backend->probe.engine_data_found ? "ready" :
                  backend->probe.engine_data_detail);
    rb->lcd_putsf(0, 5, "Run: %s", backend->engine.status);
}

static void draw_menu(const struct scummvm_backend *backend)
{
    int x = 14;
    int y = 54;
    int w = LCD_WIDTH - 28;
    int h = 96;

    (void)backend;
    rb->lcd_fillrect(x, y, w, h);
    rb->lcd_set_drawmode(DRMODE_SOLID | DRMODE_INVERSEVID);
    rb->lcd_drawrect(x, y, w, h);
    rb->lcd_putsxy(x + 8, y + 8, "ScummVM backend shell");
    rb->lcd_putsxy(x + 8, y + 28, "Select: click");
    rb->lcd_putsxy(x + 8, y + 44, "Arrows/wheel: move");
    rb->lcd_putsxy(x + 8, y + 60, "Menu: close");
    rb->lcd_putsxy(x + 8, y + 76, "Long select: exit");
    rb->lcd_set_drawmode(DRMODE_SOLID);
}

static void draw_surface_frame(const struct scummvm_backend *backend)
{
    int cursor_screen_x = backend->video.screen_x + backend->cursor_x;
    int cursor_screen_y = backend->video.screen_y + backend->cursor_y;

    scummvm_video_present(&backend->video);
    rb->lcd_drawrect(backend->video.screen_x, backend->video.screen_y,
                     backend->video.width, backend->video.height);
    if (!backend->engine.initialized) {
        rb->lcd_putsxy(backend->video.screen_x + 4,
                       backend->video.screen_y + 2,
                       "engine framebuffer");
        rb->lcd_putsxy(backend->video.screen_x + 4,
                       backend->video.screen_y + 18,
                       backend->probe.detail);
        rb->lcd_putsxy(backend->video.screen_x + 4,
                       backend->video.screen_y + 34,
                       backend->engine.status);
    }

    draw_cursor(backend, cursor_screen_x, cursor_screen_y);
}

static void draw_backend(const struct scummvm_backend *backend)
{
    rb->lcd_clear_display();
    draw_backend_status(backend);
    draw_surface_frame(backend);

    if (backend->menu_open)
        draw_menu(backend);

    rb->lcd_update();
}

static bool handle_action(struct scummvm_backend *backend, int action)
{
    int step = SCUMMVM_CURSOR_STEP;

    switch (action) {
    case PLA_LEFT_REPEAT:
    case PLA_RIGHT_REPEAT:
    case PLA_UP_REPEAT:
    case PLA_DOWN_REPEAT:
        step = SCUMMVM_CURSOR_FAST_STEP;
        break;
    default:
        break;
    }

    switch (action) {
    case PLA_LEFT:
    case PLA_LEFT_REPEAT:
        move_cursor(backend, -step, 0);
        break;
    case PLA_RIGHT:
    case PLA_RIGHT_REPEAT:
        move_cursor(backend, step, 0);
        break;
    case PLA_UP:
    case PLA_UP_REPEAT:
        move_cursor(backend, 0, -step);
        break;
    case PLA_DOWN:
    case PLA_DOWN_REPEAT:
        move_cursor(backend, 0, step);
        break;
#ifdef HAVE_SCROLLWHEEL
    case PLA_SCROLL_BACK:
    case PLA_SCROLL_BACK_REPEAT:
        move_cursor(backend, -step, 0);
        break;
    case PLA_SCROLL_FWD:
    case PLA_SCROLL_FWD_REPEAT:
        move_cursor(backend, step, 0);
        break;
#endif
    case PLA_SELECT:
        backend->mouse_down = true;
        break;
    case PLA_SELECT_REL:
        backend->mouse_down = false;
        break;
    case PLA_SELECT_REPEAT:
        return false;
    case PLA_CANCEL:
        backend->menu_open = !backend->menu_open;
        break;
    case PLA_EXIT:
        return false;
    default:
        break;
    }

    return true;
}

enum plugin_status scummvm_backend_run(const struct scummvm_target *target)
{
    struct scummvm_backend backend;
    bool running = true;

    rb->memset(&backend, 0, sizeof(backend));
    backend.target = target;
    backend.cursor_x = SCUMMVM_SURFACE_W / 2;
    backend.cursor_y = SCUMMVM_SURFACE_H / 2;
    backend.menu_open = false;
    scummvm_probe_game(target, &backend.probe);
    scummvm_engine_prepare(target, &backend.probe, &backend.engine);
    scummvm_video_init(&backend.video);
    scummvm_video_demo_pattern(&backend.video);

    rb->button_clear_queue();

    while (running) {
        int action;

        draw_backend(&backend);

        action = pluginlib_getaction(HZ / 30, scummvm_contexts,
                                     ARRAYLEN(scummvm_contexts));
        running = handle_action(&backend, action);
        if (backend.engine.initialized)
            scummvm_upstream_frame(target, &backend.engine, &backend.video);
    }

    rb->lcd_clear_display();
    rb->lcd_update();
    scummvm_sky_runtime_reset();
    scummvm_sky_cpt_unload();

    return PLUGIN_OK;
}
