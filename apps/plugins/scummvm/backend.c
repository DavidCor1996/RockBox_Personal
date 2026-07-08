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
#include "queen_runtime.h"
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
    bool click_pending;
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

static bool backend_is_queen(const struct scummvm_backend *backend)
{
    return backend->engine.initialized &&
        rb->strcmp(backend->target->engine, "queen") == 0;
}

static bool backend_cursor_in_queen_inventory(
    const struct scummvm_backend *backend)
{
    return backend->cursor_x >= 178 &&
        backend->cursor_y >= 2 &&
        backend->cursor_y < 60;
}

static void draw_cursor(const struct scummvm_backend *backend, int x, int y)
{
    (void)backend;
    rb->lcd_drawline(x - 4, y, x + 4, y);
    rb->lcd_drawline(x, y - 4, x, y + 4);
    rb->lcd_drawrect(x - 2, y - 2, 5, 5);
}

static void draw_sky_overlay(const struct scummvm_backend *backend)
{
    struct scummvm_sky_overlay overlay;
    uint16_t i;

    if (!backend->engine.initialized ||
        rb->strcasecmp(backend->target->engine, "sky") ||
        !scummvm_sky_runtime_overlay(&overlay))
        return;

    for (i = 0; i < overlay.count; i++) {
        const struct scummvm_sky_overlay_line *line = &overlay.lines[i];
        char text[SCUMMVM_SKY_OVERLAY_TEXT + 3];
        int x = backend->video.screen_x + 6;
        int y = backend->video.screen_y + line->y;
        int w = backend->video.width - 12;
        int h = 15;

        if (line->selectable)
            rb->snprintf(text, sizeof(text), "> %.48s", line->text);
        else
            rb->snprintf(text, sizeof(text), "%.50s", line->text);

        if (y < backend->video.screen_y)
            y = backend->video.screen_y;
        if (y + h > backend->video.screen_y + backend->video.height)
            y = backend->video.screen_y + backend->video.height - h;

        rb->lcd_set_drawmode(DRMODE_SOLID);
        rb->lcd_fillrect(x - 2, y - 1, w + 4, h + 2);
        rb->lcd_set_drawmode(DRMODE_SOLID | DRMODE_INVERSEVID);
        rb->lcd_drawrect(x - 2, y - 1, w + 4, h + 2);
        rb->lcd_putsxy(x, y + 3, text);
        rb->lcd_set_drawmode(DRMODE_SOLID);
    }
}

static void draw_queen_overlay(const struct scummvm_backend *backend)
{
    struct scummvm_queen_overlay overlay;
    uint16_t i;
    int x = backend->video.screen_x + 178;
    int y = backend->video.screen_y + 2;
    int dialog_x = backend->video.screen_x + 6;
    int dialog_y = backend->video.screen_y + 176;
    int dialog_w = backend->video.width - 12;

    if (!backend->engine.initialized ||
        rb->strcasecmp(backend->target->engine, "queen") ||
        !scummvm_queen_runtime_overlay(&overlay) ||
        !overlay.active)
        return;

    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_fillrect(x - 2, y - 1, 138, 57);
    rb->lcd_set_drawmode(DRMODE_SOLID | DRMODE_INVERSEVID);
    rb->lcd_drawrect(x - 2, y - 1, 138, 57);
    rb->lcd_putsxy(x, y + 2, "Inventory");
    for (i = 0; i < SCUMMVM_QUEEN_INVENTORY_SLOTS; i++) {
        char line[SCUMMVM_QUEEN_OVERLAY_TEXT + 4];

        rb->snprintf(line, sizeof(line), "%u %.58s",
                     (unsigned)(i + 1), overlay.inventory[i]);
        rb->lcd_putsxy(x, y + 13 + i * 8, line);
    }
    rb->lcd_putsxy(x, y + 47, overlay.verb);
    rb->lcd_set_drawmode(DRMODE_SOLID);

    if (overlay.message[0] != '\0' || overlay.option_count > 0) {
        rb->lcd_fillrect(dialog_x - 2, dialog_y - 1, dialog_w + 4, 58);
        rb->lcd_set_drawmode(DRMODE_SOLID | DRMODE_INVERSEVID);
        rb->lcd_drawrect(dialog_x - 2, dialog_y - 1, dialog_w + 4, 58);
        if (overlay.message[0] != '\0')
            rb->lcd_putsxy(dialog_x, dialog_y + 3, overlay.message);

        for (i = 0; i < SCUMMVM_QUEEN_DIALOG_OPTIONS; i++) {
            char line[SCUMMVM_QUEEN_OVERLAY_TEXT + 4];

            if (!overlay.option_active[i])
                continue;
            rb->snprintf(line, sizeof(line), "%u %.58s",
                         (unsigned)(i + 1), overlay.options[i]);
            rb->lcd_putsxy(dialog_x, dialog_y + 15 + i * 10, line);
        }
        rb->lcd_set_drawmode(DRMODE_SOLID);
    }
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
    if (!backend->engine.initialized) {
        rb->lcd_drawrect(backend->video.screen_x, backend->video.screen_y,
                         backend->video.width, backend->video.height);
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

    draw_sky_overlay(backend);
    draw_queen_overlay(backend);
    draw_cursor(backend, cursor_screen_x, cursor_screen_y);
}

static void draw_backend(const struct scummvm_backend *backend)
{
    rb->lcd_clear_display();
    if (!backend->engine.initialized)
        draw_backend_status(backend);
    draw_surface_frame(backend);

    if (backend->menu_open)
        draw_menu(backend);

    rb->lcd_update();
}

static void backend_status_screen(const char *line1, const char *line2)
{
    rb->lcd_clear_display();
    rb->lcd_putsxy(4, 12, "ScummVM backend");
    rb->lcd_putsxy(4, 34, line1 ? line1 : "");
    if (line2)
        rb->lcd_putsxy(4, 52, line2);
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
        if (backend_is_queen(backend)) {
            if (backend_cursor_in_queen_inventory(backend))
                scummvm_queen_runtime_cycle_inventory(
                    -1, backend->engine.status,
                    sizeof(backend->engine.status));
            else
                scummvm_queen_runtime_cycle_verb(
                    -1, backend->engine.status,
                    sizeof(backend->engine.status));
            break;
        }
        move_cursor(backend, -step, 0);
        break;
    case PLA_SCROLL_FWD:
    case PLA_SCROLL_FWD_REPEAT:
        if (backend_is_queen(backend)) {
            if (backend_cursor_in_queen_inventory(backend))
                scummvm_queen_runtime_cycle_inventory(
                    1, backend->engine.status,
                    sizeof(backend->engine.status));
            else
                scummvm_queen_runtime_cycle_verb(
                    1, backend->engine.status,
                    sizeof(backend->engine.status));
            break;
        }
        move_cursor(backend, step, 0);
        break;
#endif
    case PLA_SELECT:
        backend->mouse_down = true;
        break;
    case PLA_SELECT_REL:
        if (backend->mouse_down)
            backend->click_pending = true;
        backend->mouse_down = false;
        break;
    case PLA_SELECT_REPEAT:
        return false;
    case PLA_CANCEL:
        if (backend_is_queen(backend)) {
            if (backend_cursor_in_queen_inventory(backend))
                scummvm_queen_runtime_cycle_inventory(
                    1, backend->engine.status,
                    sizeof(backend->engine.status));
            else
                scummvm_queen_runtime_cycle_verb(
                    1, backend->engine.status,
                    sizeof(backend->engine.status));
            break;
        }
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
    backend_status_screen("probe game", target->path);
    scummvm_probe_game(target, &backend.probe);
    DEBUGF("scummvm: probe supported=%d data=%d detail=%s\n",
           backend.probe.supported, backend.probe.data_found,
           backend.probe.detail);
    backend_status_screen("prepare engine", backend.probe.detail);
    scummvm_engine_prepare(target, &backend.probe, &backend.engine);
    DEBUGF("scummvm: engine initialized=%d status=%s\n",
           backend.engine.initialized, backend.engine.status);
    backend_status_screen("init video", backend.engine.status);
    scummvm_video_init(&backend.video);
    scummvm_video_demo_pattern(&backend.video);
    backend_status_screen("runtime check", backend.engine.status);
    if (backend.engine.initialized &&
        rb->strcmp(target->engine, "queen") == 0 &&
        !scummvm_upstream_can_run(target))
        scummvm_queen_runtime_init(target, &backend.video,
                                   backend.engine.status,
                                   sizeof(backend.engine.status));

    backend_status_screen("first frame", backend.engine.status);
    rb->button_clear_queue();

    while (running) {
        int action;

        draw_backend(&backend);

        action = pluginlib_getaction(HZ / 30, scummvm_contexts,
                                     ARRAYLEN(scummvm_contexts));
        running = handle_action(&backend, action);
        if (backend.engine.initialized) {
            if (rb->strcmp(target->engine, "sky") == 0)
                scummvm_sky_runtime_input(backend.cursor_x,
                                          backend.cursor_y,
                                          backend.mouse_down,
                                          backend.click_pending);
            else if (rb->strcmp(target->engine, "queen") == 0 &&
                     scummvm_upstream_can_run(target)) {
                scummvm_upstream_input(backend.cursor_x, backend.cursor_y,
                                       backend.mouse_down,
                                       backend.click_pending);
            } else if (rb->strcmp(target->engine, "queen") == 0) {
                scummvm_queen_runtime_input(backend.cursor_x,
                                            backend.cursor_y,
                                            backend.click_pending,
                                            backend.engine.status,
                                            sizeof(backend.engine.status));
            }
            backend.click_pending = false;
            if (rb->strcmp(target->engine, "queen") != 0 ||
                scummvm_upstream_can_run(target)) {
                if (!scummvm_upstream_frame(target, &backend.engine,
                                            &backend.video))
                    running = false;
            }
        }
    }

    rb->lcd_clear_display();
    rb->lcd_update();
    if (backend.engine.initialized &&
        rb->strcmp(target->engine, "queen") == 0 &&
        !scummvm_upstream_can_run(target))
        scummvm_queen_runtime_save(backend.engine.status,
                                   sizeof(backend.engine.status));
    scummvm_sky_runtime_reset();
    scummvm_sky_cpt_unload();
    scummvm_upstream_shutdown();

    return PLUGIN_OK;
}
