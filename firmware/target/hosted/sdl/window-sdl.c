/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
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

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "sim-ui-defines.h"
#include "window-sdl.h"
#include "lcd-sdl.h"
#include "misc.h"
#include "panic.h"
#include "sim_tasks.h"
#include "screendump.h"

/* Preview paths are absolute host paths supplied by the regression harness.
 * The simulator filesystem macros would remap them into the virtual player
 * root, so publishing the SDL staging BMP must use the host libc calls. */
#ifdef remove
#undef remove
#endif
#ifdef rename
#undef rename
#endif

extern SDL_Surface *lcd_surface;
#ifdef HAVE_REMOTE_LCD
extern SDL_Surface *remote_surface;
#endif

SDL_Texture  *gui_texture;
SDL_Surface  *sim_lcd_surface;

SDL_mutex *window_mutex;

SDL_Window   *sdlWindow;
static SDL_Renderer *sdlRenderer;
static SDL_Surface  *picture_surface;

static bool new_gui_texture_needed = true;
static bool window_adjustment_needed;
double display_zoom = 1;
static bool rockpod_preview_enabled;
static bool rockpod_preview_hidden;
static Uint32 rockpod_preview_interval_ms;
static Uint32 rockpod_preview_last_ticks;
static char rockpod_preview_path[MAX_PATH];
static bool rockpod_auto_dump_once;
static bool rockpod_auto_dump_done;
static Uint32 rockpod_auto_dump_after_ticks;
static const char rockpod_auto_dump_flagfile[] = "tmp/cherryblossom-sim-autodump.flag";

/* panicf() renders its message on the simulated LCD.  It is therefore not
 * safe until the LCD surface exists; using it for an SDL startup failure can
 * turn the useful SDL error into a null-surface crash. */
static void sdl_window_startup_fatal(const char *operation)
{
    fprintf(stderr, "%s failed: %s\n", operation, SDL_GetError());
    exit(EXIT_FAILURE);
}

static void rockpod_preview_configure(void)
{
    const char *auto_dump = getenv("ROCKPOD_SIM_AUTO_DUMP_ONCE");
    rockpod_auto_dump_once = auto_dump &&
        (!strcmp(auto_dump, "1") || !strcasecmp(auto_dump, "true") || !strcasecmp(auto_dump, "yes"));
    rockpod_auto_dump_done = false;
    rockpod_auto_dump_after_ticks = 1500;
    if (!rockpod_auto_dump_once && access(rockpod_auto_dump_flagfile, F_OK) == 0)
        rockpod_auto_dump_once = true;

    const char *path = getenv("ROCKPOD_SIM_PREVIEW_BMP");
    if (!path || !*path)
    {
        rockpod_preview_enabled = false;
        rockpod_preview_path[0] = '\0';
        return;
    }

    snprintf(rockpod_preview_path, sizeof(rockpod_preview_path), "%s", path);
    rockpod_preview_enabled = true;
    rockpod_preview_hidden = false;
    rockpod_preview_last_ticks = 0;
    rockpod_preview_interval_ms = 250;

    const char *interval = getenv("ROCKPOD_SIM_PREVIEW_INTERVAL_MS");
    if (interval && *interval)
    {
        long value = strtol(interval, NULL, 10);
        /* Zero is a regression-only mode that publishes every render.  It
         * guarantees the final full LCD update replaces any intermediate
         * dirty rectangle on an otherwise static screen. */
        if (value >= 0 && value <= 5000)
            rockpod_preview_interval_ms = (Uint32)value;
    }

    const char *hidden = getenv("ROCKPOD_SIM_HIDDEN");
    if (hidden && (!strcmp(hidden, "1") || !strcasecmp(hidden, "true") || !strcasecmp(hidden, "yes")))
        rockpod_preview_hidden = true;

    const char *delay_ms = getenv("ROCKPOD_SIM_AUTO_DUMP_DELAY_MS");
    if (delay_ms && *delay_ms)
    {
        long value = strtol(delay_ms, NULL, 10);
        if (value >= 0 && value <= 30000)
            rockpod_auto_dump_after_ticks = (Uint32)value;
    }
}

static void rockpod_preview_capture_if_needed(void)
{
    if (!rockpod_preview_enabled || !rockpod_preview_path[0])
        return;

    Uint32 now = SDL_GetTicks();
    if (rockpod_preview_last_ticks && now - rockpod_preview_last_ticks < rockpod_preview_interval_ms)
        return;

    int width = 0;
    int height = 0;
    if (SDL_GetRendererOutputSize(sdlRenderer, &width, &height) != 0 || width <= 0 || height <= 0)
        return;

    SDL_Surface *frame = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (frame == NULL)
        return;

    if (SDL_RenderReadPixels(sdlRenderer, NULL, SDL_PIXELFORMAT_ARGB8888, frame->pixels, frame->pitch) != 0)
    {
        SDL_FreeSurface(frame);
        return;
    }

    char tmp_path[MAX_PATH];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.bmp", rockpod_preview_path);
    if (SDL_SaveBMP(frame, tmp_path) == 0)
    {
        remove(rockpod_preview_path);
        rename(tmp_path, rockpod_preview_path);
        rockpod_preview_last_ticks = now;
    }
    else
    {
        remove(tmp_path);
    }

    SDL_FreeSurface(frame);
}

static void get_window_dimensions(int *w, int *h)
{
    if (background)
    {
        *w = UI_WIDTH;
        *h = UI_HEIGHT;
    }
    else
    {
#ifdef HAVE_REMOTE_LCD
        if (showremote)
        {
            *w = SIM_LCD_WIDTH > SIM_REMOTE_WIDTH ? SIM_LCD_WIDTH : SIM_REMOTE_WIDTH;
            *h = SIM_LCD_HEIGHT + SIM_REMOTE_HEIGHT;
        }
        else
#endif
        {
            *w = SIM_LCD_WIDTH;
            *h = SIM_LCD_HEIGHT;
        }
    }
}

#if defined(__APPLE__) || defined(__WIN32)
static void restore_aspect_ratio(int w, int h)
{
    float aspect_ratio = (float) h / w;
    int original_height = h;
    int original_width = w;

    if ((SDL_GetWindowFlags(sdlWindow) & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN))
        || display_zoom)
        return;

    SDL_GetWindowSize(sdlWindow, &w, &h);
    if (w != original_width || h != original_height)
    {
        SDL_DisplayMode sdl_dm;
        h = w * aspect_ratio;
        if (SDL_GetCurrentDisplayMode(0, &sdl_dm) || h <= sdl_dm.h)
            SDL_SetWindowSize(sdlWindow, w, h);
    }
}
#endif

static void rebuild_gui_texture(void)
{
    SDL_Surface *gui_surface;
    int prev_w, prev_h, x, y,
        w, h, depth = LCD_DEPTH < 8 ? 16 : LCD_DEPTH;
    Uint32 flags = SDL_GetWindowFlags(sdlWindow);

    get_window_dimensions(&w, &h);
    SDL_RenderGetLogicalSize(sdlRenderer, &prev_w, &prev_h);
    SDL_RenderSetLogicalSize(sdlRenderer, w, h);
    if ((gui_texture = SDL_CreateTexture(sdlRenderer, SDL_MasksToPixelFormatEnum(depth,
                                         0, 0, 0, 0), SDL_TEXTUREACCESS_STREAMING, w, h)) == NULL)
        panicf("%s", SDL_GetError());

    /* Did background change? */
    if ((flags & SDL_WINDOW_RESIZABLE) &&
        !(flags & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN)) &&
        prev_w && prev_w != w)
    {
        SDL_GetWindowSize(sdlWindow, &x, NULL);

        /* Maintain LCD's size */
        float ratio = (float) x / prev_w;
        SDL_SetWindowSize(sdlWindow, w * ratio, h * ratio);

        /* move LCD back into previous position */
        SDL_GetWindowPosition(sdlWindow, &x, &y);
        if (background)
        {
            x -= (UI_LCD_POSX * ratio);
            y -= (UI_LCD_POSY * ratio);
        }
        else
        {
            x += (UI_LCD_POSX * ratio);
            y += (UI_LCD_POSY * ratio);
        }
        SDL_SetWindowPosition(sdlWindow, x > 0 ? x : 0, y > 0 ? y : 0);
    }

    if (background && picture_surface &&
        (gui_surface = SDL_ConvertSurface(picture_surface, sim_lcd_surface->format, 0)))
    {
        SDL_UpdateTexture(gui_texture, NULL, gui_surface->pixels, gui_surface->pitch);
        SDL_FreeSurface(gui_surface);
    }

    sdl_gui_update(lcd_surface, 0, 0, SIM_LCD_WIDTH, SIM_LCD_HEIGHT,
                   SIM_LCD_WIDTH, SIM_LCD_HEIGHT,
                   background ? UI_LCD_POSX : 0, background? UI_LCD_POSY : 0);

#ifdef HAVE_REMOTE_LCD
    sdl_gui_update(remote_surface, 0, 0, LCD_REMOTE_WIDTH, LCD_REMOTE_HEIGHT,
                   LCD_REMOTE_WIDTH, LCD_REMOTE_HEIGHT,
                   background ? UI_REMOTE_POSX : 0,
                   background? UI_REMOTE_POSY : LCD_HEIGHT);
#endif
}

void sdl_window_render(void)
{
    if (new_gui_texture_needed)
    {
        new_gui_texture_needed = false;
        if (gui_texture)
            SDL_DestroyTexture(gui_texture);
        rebuild_gui_texture();
    }

    SDL_RenderClear(sdlRenderer);
    SDL_RenderCopy(sdlRenderer, gui_texture, NULL, NULL);
    /* Read the composed backbuffer before Present swaps or invalidates it.
     * Reading afterward can return the previous LCD frame on SDL renderers. */
    rockpod_preview_capture_if_needed();
    SDL_RenderPresent(sdlRenderer);

    if (rockpod_auto_dump_once && !rockpod_auto_dump_done &&
        SDL_GetTicks() >= rockpod_auto_dump_after_ticks)
    {
        rockpod_auto_dump_done = true;
        screen_dump();
    }

}

bool sdl_window_adjust(void)
{
    int w, h;

    if (!window_adjustment_needed)
        return false;
    window_adjustment_needed = false;

    get_window_dimensions(&w, &h);

    if (!(SDL_GetWindowFlags(sdlWindow) & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN))
        && display_zoom)
    {
        SDL_SetWindowSize(sdlWindow, display_zoom * w, display_zoom * h);
    }
#if defined(__APPLE__) || defined(__WIN32)
    int logical_w, logical_h;
    SDL_RenderGetLogicalSize(sdlRenderer, &logical_w, &logical_h);
    if (logical_w == w && logical_h == h) /* background unchanged */
        restore_aspect_ratio(w, h);
#endif
    display_zoom = 0;
    sdl_window_render();
    return true;
}

void sdl_window_adjustment_needed(bool destroy_texture)
{
    window_adjustment_needed = true;
    new_gui_texture_needed = destroy_texture;

    /* For MacOS and Windows, we're on a main or
    display thread already, and can immediately
    adjust the window.
    On Linux, we have to defer the update, until
    it is handled by the main thread later. */
#if defined (__APPLE__) || defined(__WIN32)
    sdl_window_adjust();
#endif
}

void sdl_window_setup(void)
{
    int width, height;
    int depth = LCD_DEPTH < 8 ? 16 : LCD_DEPTH;
    Uint32 flags = 0;

    rockpod_preview_configure();

#if 0
    /* Fullscreen mode might be desired */
    flags |= SDL_WINDOW_FULLSCREEN;
#else
    if (display_zoom == 1)
        flags |= SDL_WINDOW_RESIZABLE;
#endif
    if (rockpod_preview_hidden)
        flags |= SDL_WINDOW_HIDDEN;

    if (!(picture_surface = SDL_LoadBMP("UI256.bmp")))
        background = false;

    get_window_dimensions(&width, &height);

    if ((sdlWindow = SDL_CreateWindow(UI_TITLE, SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED, width * display_zoom,
                                   height * display_zoom , flags)) == NULL)
        sdl_window_startup_fatal("SDL window creation");
    if ((sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_PRESENTVSYNC)) == NULL)
        sdl_window_startup_fatal("SDL renderer creation");

    /* Surface for LCD content only. Needs to fit largest LCD */
    int surface_width =
#ifdef HAVE_REMOTE_LCD
        SIM_LCD_WIDTH > SIM_REMOTE_WIDTH ? SIM_LCD_WIDTH : SIM_REMOTE_WIDTH;
    int surface_height =
        SIM_LCD_HEIGHT > SIM_REMOTE_HEIGHT ? SIM_LCD_HEIGHT : SIM_REMOTE_HEIGHT;
#else
        SIM_LCD_WIDTH;
    int surface_height = SIM_LCD_HEIGHT;
#endif

    if (depth == 16)
        sim_lcd_surface = SDL_CreateRGBSurfaceWithFormat(0, surface_width,
                              surface_height, 16, SDL_PIXELFORMAT_RGB565);
    else
        sim_lcd_surface = SDL_CreateRGBSurface(0, surface_width,
                              surface_height, depth, 0, 0, 0, 0);

    if (sim_lcd_surface == NULL)
        sdl_window_startup_fatal("SDL LCD surface creation");

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, display_zoom == 1 ? "best" : "nearest");
    window_mutex = SDL_CreateMutex();
    display_zoom = 0; /* reset to 0 unless/until user requests a scale level change */
}
