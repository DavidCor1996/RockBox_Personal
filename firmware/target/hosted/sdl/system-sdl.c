/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2006 by Daniel Everton <dan@iocaine.org>
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
#include <SDL_thread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#ifdef ROCKPOD_IOS_EMBED
#include <pthread.h>
#endif
#ifdef __unix__
#include <unistd.h>
#endif
#include "system.h"
#include "kernel.h"
#include "thread-sdl.h"
#include "system-sdl.h"
#include "sim-ui-defines.h"
#if SDL_MAJOR_VERSION > 1
#include "window-sdl.h"
#endif
#include "button-sdl.h"
#include "lcd-bitmap.h"
#ifdef HAVE_REMOTE_LCD
#include "lcd-remote-bitmap.h"
#endif
#include "panic.h"
#include "debug.h"
#include "fs_defines.h"
#include "strlcpy.h"
#include "strlcat.h"

#if defined(RG_NANO) && !defined(SIMULATOR)
#include <signal.h>
#include "instant_play.h"
#endif

#define SIMULATOR_DEFAULT_ROOT "simdisk"

#if SDL_MAJOR_VERSION == 1
SDL_Surface *gui_surface;
#endif

bool            background = true;          /* use backgrounds by default */
#ifdef HAVE_REMOTE_LCD
bool            showremote = true;          /* include remote by default */
#endif
bool            mapping = false;
const char      *audiodev = NULL;
bool            debug_buttons = false;

bool            sim_alarm_wakeup = false;
const char     *sim_root_dir = SIMULATOR_DEFAULT_ROOT;
static char     sim_root_buf[MAX_PATH];
static bool     sim_root_override = false;

static SDL_Thread *evt_thread = NULL;

static void sdl_set_default_sim_root(const char *argv0)
{
    char exe_path[MAX_PATH];
    const char *slash = NULL;

    if (sim_root_override || !argv0 || !*argv0)
        return;

#ifdef __unix__
    if (!realpath(argv0, exe_path))
#endif
    {
        strlcpy(exe_path, argv0, sizeof(exe_path));
    }

    slash = strrchr(exe_path, '/');
#ifdef _WIN32
    {
        const char *backslash = strrchr(exe_path, '\\');
        if (backslash && (!slash || backslash > slash))
            slash = backslash;
    }
#endif

    if (!slash)
        return;

    sim_root_buf[0] = '\0';
    if ((size_t)(slash - exe_path) >= sizeof(sim_root_buf))
        return;

    memcpy(sim_root_buf, exe_path, (size_t)(slash - exe_path));
    sim_root_buf[slash - exe_path] = '\0';
    strlcat(sim_root_buf, "/", sizeof(sim_root_buf));
    strlcat(sim_root_buf, SIMULATOR_DEFAULT_ROOT, sizeof(sim_root_buf));
    sim_root_dir = sim_root_buf;
}

#ifdef DEBUG
bool debug_audio = false;
#endif

bool debug_wps = false;
int wps_verbose_level = 3;

#if defined(SIMULATOR) && defined(__linux__) && SDL_MAJOR_VERSION > 1
static void sdl_apply_linux_renderer_workaround(void)
{
    /* sdl2-compat on Wayland can open an all-black renderer for these sims. */
    setenv("SDL_VIDEODRIVER", "x11", 0);
    setenv("SDL_RENDER_DRIVER", "software", 0);
}
#endif

/* The Apple special-case exists because macOS/Cocoa must pump events on the
 * process main thread.  The iPhone companion does not use UIKit video at all
 * -- it runs the offscreen driver and copies captured frames into its own
 * UIImageView -- and its core runs on a background dispatch queue, so there
 * is no main thread to hand events to.  Use the ordinary threaded event
 * path there, which is the configuration the desktop simulator uses. */
#if !defined(__APPLE__) || defined(ROCKPOD_IOS_EMBED)
/*
 * This thread will read the buttons in an interrupt like fashion, and
 * also initializes SDL_INIT_VIDEO and the surfaces
 *
 * it must be done in the same thread (at least on windows) because events only
 * work in the thread that called SDL_InitSubSystem(SDL_INIT_VIDEO)
 *
 * This is an SDL thread and relies on preemptive behavoir of the host
 **/
static int sdl_event_thread(void * param)
{
#ifdef __WIN32 /* Fails on Linux and MacOS */
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
    SDL_InitSubSystem(SDL_INIT_VIDEO);
    sdl_window_setup();
#endif

#if !defined(SIMULATOR)
#if defined(HAVE_TOUCHSCREEN)
    /* SDL touch screen fix: Work around a SDL assumption that returns
       relative mouse coordinates when you get to the screen edges
       using the touchscreen and a disabled mouse cursor.
     */
    uint8_t hiddenCursorData = 0;
    SDL_Cursor *hiddenCursor = SDL_CreateCursor(&hiddenCursorData, &hiddenCursorData, 8, 1, 0, 0);

    SDL_ShowCursor(SDL_ENABLE);
    SDL_SetCursor(hiddenCursor);
#else /* !HAVE_TOUCHSCREEN */
    /* Explicitly disable the cursor on non-touch targets */
    SDL_ShowCursor(SDL_DISABLE);
#endif /* !HAVE_TOUCHSCREEN */
#endif /* !SIMULATOR */

#if SDL_MAJOR_VERSION == 1
    SDL_InitSubSystem(SDL_INIT_VIDEO);

    SDL_Surface *picture_surface = NULL;
    int depth;
    Uint32 flags;

    depth = LCD_DEPTH;
    if (depth < 8)
        depth = 16;

    flags = SDL_HWSURFACE|SDL_DOUBLEBUF|SDL_FULLSCREEN;

    if ((gui_surface = SDL_SetVideoMode(LCD_WIDTH, LCD_HEIGHT, depth, flags)) == NULL) {
        panicf("%s", SDL_GetError());
    }

    if (background && picture_surface != NULL)
        SDL_BlitSurface(picture_surface, NULL, gui_surface, NULL);
#endif

    /* let system_init proceed */
    SDL_SemPost((SDL_sem *)param);

    /* finally enter the button loop */
    gui_message_loop();

    /* Order here is relevent to prevent deadlocks and use of destroyed
       sync primitives by kernel threads */
#ifdef HAVE_SDL_THREADS
    sim_thread_shutdown(); /* not needed for native threads */
#endif
    return 0;
}
#endif

static bool quitting;

void sdl_sys_quit(void)
{
    quitting = true;
    sys_poweroff();
}

void power_off(void)
{
    /* Shut down SDL event loop */
    SDL_Event event;
    memset(&event, 0, sizeof(SDL_Event));
    event.type = SDL_USEREVENT;
    SDL_PushEvent(&event);
#ifdef HAVE_SDL_THREADS
    /* since sim_thread_shutdown() grabs the mutex we need to let it free,
     * otherwise SDL_WaitThread will deadlock */
    struct thread_entry* t = sim_thread_unlock();

    if (!evt_thread) /* no event thread on MacOS */
        sim_thread_shutdown();
#endif
    /* wait for event thread to finish */
    SDL_WaitThread(evt_thread, NULL);

#if defined(RG_NANO) && !defined(SIMULATOR)
    /* Reset volume/brightness to the values before launching rockbox */
    ip_reset_values();
    ip_power_off();
#endif

#ifdef HAVE_SDL_THREADS
    /* lock again before entering the scheduler */
    sim_thread_lock(t);
    /* sim_thread_shutdown() will cause sim_do_exit() to be called via longjmp,
     * but only if we let the sdl thread scheduler exit the other threads */
    while(1) yield();
#else
    sim_do_exit();
#endif
}

void sim_do_exit()
{
#ifdef SIMULATOR
    extern SDL_Cursor *sdl_focus_cursor;
    extern SDL_Cursor *sdl_arrow_cursor;
    if (sdl_focus_cursor)
        SDL_FreeCursor(sdl_focus_cursor);
    if (sdl_arrow_cursor)
        SDL_FreeCursor(sdl_arrow_cursor);
#endif

    sim_kernel_shutdown();

#if SDL_MAJOR_VERSION > 1
    SDL_UnlockMutex(window_mutex);
    SDL_DestroyMutex(window_mutex);
#endif

    SDL_Quit();
#ifdef ROCKPOD_IOS_EMBED
    /* The simulator is one screen inside RockPod Link. Powering it off must
     * return to the companion rather than terminating the entire iOS app. */
    extern void rockpod_ios_simulator_did_exit(void);
    rockpod_ios_simulator_did_exit();
    pthread_exit(NULL);
#else
    exit(EXIT_SUCCESS);
#endif
}

uintptr_t *stackbegin;
uintptr_t *stackend;
void system_init(void)
{
    SDL_sem *s;
    /* fake stack, OS manages size (and growth) */
    stackbegin = stackend = (uintptr_t*)&s;

#if defined(SIMULATOR) && defined(__linux__) && SDL_MAJOR_VERSION > 1
    sdl_apply_linux_renderer_workaround();
#endif

#if defined(RG_NANO) && !defined(SIMULATOR)
    /* Set system volume to max with amixer */
    system("amixer -q sset 'Headphone' 63 unmute");

    /* Instant play handling */
    struct sigaction ip_sa;
    ip_sa.sa_handler = ip_handle_sigusr1;
    sigaction(SIGUSR1, &ip_sa, NULL);
#endif

    if (SDL_InitSubSystem(SDL_INIT_TIMER))
        panicf("%s", SDL_GetError());

#ifdef SIMULATOR
    {
        SDL_version compiled;
        SDL_version linked;

        SDL_VERSION(&compiled);
        SDL_GetVersion(&linked);
        printf("Rockbox compiled with SDL %u.%u.%u but running on SDL %u.%u.%u\n",
               compiled.major, compiled.minor, compiled.patch,
               linked.major, linked.minor, linked.patch);
    }
#endif

#ifndef __WIN32  /* Fails on Windows */
    SDL_InitSubSystem(SDL_INIT_VIDEO);

#if SDL_MAJOR_VERSION > 1
    sdl_window_setup();
#endif
#endif

#if !defined(__APPLE__) || defined(ROCKPOD_IOS_EMBED)
    s = SDL_CreateSemaphore(0); /* 0-count so it blocks */

    #if SDL_MAJOR_VERSION > 1
        evt_thread = SDL_CreateThread(sdl_event_thread, NULL, s);
    #else
        evt_thread = SDL_CreateThread(sdl_event_thread, s);
    #endif /* SDL_MAJOR_VERSION */

    /* A plain SDL_SemWait() deadlocks system_init() forever if the event
     * thread never starts -- SDL_CreateThread() returning NULL, or the thread
     * blocking before it posts.  Rockbox then never boots and the panel keeps
     * showing the single frame produced by sdl_window_setup() above, which is
     * exactly the "one black frame and nothing further" seen on iOS.  Wait
     * with a deadline and carry on regardless; events are a nicety, a player
     * that never starts is not. */
    if (!evt_thread)
        printf("sdl_event_thread failed to start: %s\n", SDL_GetError());
    else if (SDL_SemWaitTimeout(s, 5000) != 0)
        printf("sdl_event_thread did not signal within 5s; continuing\n");

    /* cleanup */
    SDL_DestroySemaphore(s);
#else
    SDL_AddEventWatch(sdl_event_filter, NULL);
#endif
}


void system_reboot(void)
{
#if defined(RG_NANO) && !defined(SIMULATOR)
    /* Reset volume/brightness to the values before launching rockbox */
    ip_reset_values();
    SDL_Quit();
    exit(EXIT_SUCCESS);
#endif

#ifdef HAVE_SDL_THREADS
    sim_thread_exception_wait();
#else
    sim_do_exit();
#endif
}

void system_exception_wait(void)
{
    if (evt_thread)
    {
        while (!quitting)
            SDL_Delay(10);
    }
    system_reboot();
}

int hostfs_init(void)
{
    /* stub */
    return 0;
}

#ifdef HAVE_STORAGE_FLUSH
int hostfs_flush(void)
{
#ifdef __unix__
    sync();
#endif
    return 0;
}
#endif /* HAVE_STORAGE_FLUSH */

void sys_handle_argv(int argc, char *argv[])
{
    if (argc >= 1)
        sdl_set_default_sim_root(argv[0]);

    if (argc >= 1)
    {
        int x;
        for (x = 1; x < argc; x++)
        {
#ifdef DEBUG
            if (!strcmp("--debugaudio", argv[x]))
            {
                debug_audio = true;
                printf("Writing debug audio file.\n");
            }
            else
#endif
                if (!strcmp("--debugwps", argv[x]))
            {
                debug_wps = true;
                printf("WPS debug mode enabled.\n");
            }
            else if (!strcmp("--nobackground", argv[x]))
            {
                background = false;
                printf("Disabling background image.\n");
            }
            else if (!strcmp("--fullscreen", argv[x]))
            {
                /* Rockpod's parity session fills the host display with the
                 * 320x240 panel instead of showing it in a small window. */
                sdl_fullscreen = true;
                background = false;
                printf("Filling the host display.\n");
            }
#ifdef HAVE_REMOTE_LCD
            else if (!strcmp("--noremote", argv[x]))
            {
                showremote = false;
                background = false;
                printf("Disabling remote image.\n");
            }
#endif
#if SDL_MAJOR_VERSION > 1
            else if (!strcmp("--zoom", argv[x]))
            {
                x++;
                if(x < argc)
                    display_zoom=atof(argv[x]);
                else
                    display_zoom = 2;
                printf("Window zoom is %f\n", display_zoom);
            }
#endif
            else if (!strcmp("--alarm", argv[x]))
            {
                sim_alarm_wakeup = true;
                printf("Simulating alarm wakeup.\n");
            }
            else if (!strcmp("--root", argv[x]))
            {
                x++;
                if (x < argc)
                {
                    sim_root_dir = argv[x];
                    sim_root_override = true;
                    printf("Root directory: %s\n", sim_root_dir);
                }
            }
            else if (!strcmp("--mapping", argv[x]))
            {
                    mapping = true;
                    printf("Printing click coords with drag radii.\n");
            }
            else if (!strcmp("--debugbuttons", argv[x]))
            {
                    debug_buttons = true;
                    printf("Printing background button clicks.\n");
            }
            else if (!strcmp("--audiodev", argv[x]))
            {
                x++;
                if (x < argc)
                {
                    audiodev = argv[x];
                    printf("Audio device: '%s'\n", audiodev);
                }
            }
            else
            {
                printf("rockboxui\n");
                printf("Arguments:\n");
#ifdef DEBUG
                printf("  --debugaudio \t Write raw PCM data to audiodebug.raw\n");
#endif
                printf("  --debugwps \t Print advanced WPS debug info\n");
                printf("  --nobackground \t Disable the background image\n");
#ifdef HAVE_REMOTE_LCD
                printf("  --noremote \t Disable the remote image (will disable backgrounds)\n");
#endif
                printf("  --zoom [VAL]\t Window zoom (will disable backgrounds)\n");
                printf("  --alarm \t Simulate a wake-up on alarm\n");
                printf("  --root [DIR]\t Set root directory\n");
                printf("  --mapping \t Output coordinates and radius for mapping backgrounds\n");
                printf("  --audiodev [NAME] \t Audio device name to use\n");
                exit(0);
            }
        }
    }
#if SDL_MAJOR_VERSION > 1
    if (display_zoom != 1) {
        background = false;
    }
#endif
}
