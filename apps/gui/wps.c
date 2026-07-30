/***************************************************************************
 * __________               __   ___.
 * Open      \______   \ ____   ____ |  | _\_ |__   _______   ___
 * Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 * Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 * Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 * \/            \/      \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 Jerome Kuptz
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"

#include "system.h"
#include "debug.h"
#include "file.h"
#include "lcd.h"
#include "font.h"
#include "backlight.h"
#include "action.h"
#include "kernel.h"
#include "button.h"
#include "filetypes.h"
#include "settings.h"
#include "skin_engine/skin_engine.h"
#include "audio.h"
#include "usb.h"
#include "status.h"
#include "storage.h"
#include "screens.h"
#include "playlist.h"
#include "icons.h"
#include "lang.h"
#include "bookmark.h"
#include "misc.h"
#include "sound.h"
#include "onplay.h"
#include "abrepeat.h"
#include "playback.h"
#include "splash.h"
#include "cuesheet.h"
#include "ata_idle_notify.h"
#include "root_menu.h"
#include "backdrop.h"
#include "quickscreen.h"
#include "shortcuts.h"
#include "pitchscreen.h"
#include "appevents.h"
#include "viewport.h"
#include "pcmbuf.h"
#include "option_select.h"
#include "playlist_viewer.h"
#include "wps.h"
#include "statusbar-skinned.h"
#include "skin_engine/wps_internals.h"
#include "open_plugin.h"
#include "plugin.h"
#include "gui/ipodjs_trace.h"
#include "gui/ipodjs_ui.h"
#ifdef IPOD_ACCESSORY_PROTOCOL
#include "iap.h"
#endif

#ifdef USB_ENABLE_AUDIO
#include "usbstack/usb_audio.h"
#endif

#define FF_REWIND_MAX_PERCENT 3
#define MIN_FF_REWIND_STEP 500

static struct wps_state wps_state;
static bool wps_playback_events_registered;

static void track_info_callback(unsigned short id, void *param);

#define WPS_DEFAULTCFG WPS_DIR "/rockbox_default.wps"
#ifdef HAVE_REMOTE_LCD
#define RWPS_DEFAULTCFG WPS_DIR "/rockbox_default.rwps"
#define DEFAULT_WPS(screen) ((screen) == SCREEN_MAIN ? \
WPS_DEFAULTCFG:RWPS_DEFAULTCFG)
#else
#define DEFAULT_WPS(screen) (WPS_DEFAULTCFG)
#endif

char* wps_default_skin(enum screen_type screen)
{
    static char *skin_buf[NB_SCREENS] = {
        #if LCD_DEPTH > 1
        "%X(d)\n"
        #endif
        "%s%?it<%?in<%in. |>%it|%fn>\n"
        "%s%?ia<%ia|%?d(2)<%d(2)|%(root%)>>\n"
        "%s%?id<%id|%?d(1)<%d(1)|%(root%)>> %?iy<%(%iy%)|>\n\n"
        "%al%pc/%pt%ar[%pp:%pe]\n"
        "%fbkBit %?fv<avg|> %?iv<%(id3v%iv%)|%(no id3%)>\n"
        "%pb\n%pm\n",
        #ifdef HAVE_REMOTE_LCD
        #if LCD_REMOTE_DEPTH > 1
        "%X(d)\n"
        #endif
        "%s%?ia<%ia|%?d(2)<%d(2)|%(root%)>>\n"
        "%s%?it<%?in<%in. |>%it|%fn>\n"
        "%al%pc/%pt%ar[%pp:%pe]\n"
        "%fbkBit %?fv<avg|> %?iv<%(id3v%iv%)|%(no id3%)>\n"
        "%pb\n",
        #endif
    };
    return skin_buf[screen];
}

static bool ipodjs_wps_controls(void)
{
#if defined(IPOD_6G)
    return global_settings.ui_engine == UI_ENGINE_IPODJS;
#else
    return false;
#endif
}

static unsigned int wps_partial_refresh(unsigned int refresh)
{
    if (ipodjs_wps_controls())
        refresh |= SKIN_REFRESH_STATIC;

    return refresh;
}

static void update_non_static(void)
{
    FOR_NB_SCREENS(i)
        skin_update(WPS, i, wps_partial_refresh(SKIN_REFRESH_NON_STATIC));
}

void wps_do_action(enum wps_do_action_type action, bool updatewps)
{
    if (action == WPS_PLAYPAUSE)
    {
        struct wps_state *state = get_wps_state();
        if (state->paused)
            action = WPS_PLAY;
        state->paused = !state->paused;
    }

    if (action == WPS_PLAY)
    {
        DEBUGF("wps: audio_resume\n");
        audio_resume();
        get_wps_state()->paused = false;
    }
    else
    {
        DEBUGF("wps: audio_pause action=%d\n", action);
        audio_pause();
        get_wps_state()->paused = true;
        if (global_settings.pause_rewind) {
            unsigned long elapsed = audio_current_track()->elapsed;
            long newpos = elapsed - (global_settings.pause_rewind * 1000);
            audio_pre_ff_rewind();
            audio_ff_rewind(newpos > 0 ? newpos : 0);
        }
    }

    enum current_activity act = get_current_activity();
    bool refresh = (act == ACTIVITY_FM || act == ACTIVITY_WPS || act == ACTIVITY_RECORDING);

    if (updatewps && refresh)
        update_non_static();
}

#ifdef HAVE_TOUCHSCREEN
static int skintouch_to_wps(void)
{
    int offset = 0;
    struct gui_wps *gwps = skin_get_gwps(WPS, SCREEN_MAIN);
    int button = skin_get_touchaction(gwps, &offset);
    switch (button)
    {
        case ACTION_STD_PREV: return ACTION_WPS_SKIPPREV;
        case ACTION_STD_PREVREPEAT: return ACTION_WPS_SEEKBACK;
        case ACTION_STD_NEXT: return ACTION_WPS_SKIPNEXT;
        case ACTION_STD_NEXTREPEAT: return ACTION_WPS_SEEKFWD;
        case ACTION_STD_MENU: return ACTION_WPS_MENU;
        case ACTION_STD_CONTEXT: return ACTION_WPS_CONTEXT;
        case ACTION_STD_QUICKSCREEN: return ACTION_WPS_QUICKSCREEN;
        #ifdef HAVE_HOTKEY
        case ACTION_STD_HOTKEY: return ACTION_WPS_HOTKEY;
        #endif
    }
    return button;
}
#endif

static bool ffwd_rew(int button, bool seek_from_end)
{
    unsigned int step = 0;
    unsigned int max_step = 0;
    int ff_rewind_count = 0;
    int direction = -1;
    bool exit = false;
    bool usb = false;
    bool ff_rewind = false;
    const long ff_rw_accel = (global_settings.ff_rewind_accel + 3);
    struct wps_state *gstate = get_wps_state();
    struct mp3entry *old_id3 = gstate->id3;

    if (button == ACTION_NONE)
    {
        status_set_ffmode(0);
        return usb;
    }
    while (!exit)
    {
        struct mp3entry *id3 = gstate->id3;
        if (id3 != old_id3)
        {
            ff_rewind = false;
            ff_rewind_count = 0;
            old_id3 = id3;
        }
        if (id3 && seek_from_end)
            id3->elapsed = id3->length;

        switch ( button )
        {
            case ACTION_WPS_SEEKFWD:
                direction = 1;
            case ACTION_WPS_SEEKBACK:
                if (ff_rewind)
                {
                    if (direction == 1)
                        max_step = (id3->length - (id3->elapsed + ff_rewind_count)) * FF_REWIND_MAX_PERCENT / 100;
                    else
                        max_step = (id3->elapsed + ff_rewind_count) * FF_REWIND_MAX_PERCENT / 100;

                    max_step = MAX(max_step, MIN_FF_REWIND_STEP);
                    if (step > max_step) step = max_step;
                    ff_rewind_count += step * direction;
                    step += step >> ff_rw_accel;
                }
                else
                {
                    if ((audio_status() & AUDIO_STATUS_PLAY) && id3 && id3->length )
                    {
                        audio_pre_ff_rewind();
                        status_set_ffmode(direction > 0 ? STATUS_FASTFORWARD : STATUS_FASTBACKWARD);
                        ff_rewind = true;
                        step = 1000 * global_settings.ff_rewind_min_step;
                    }
                    else break;
                }

                if (direction > 0) {
                    if ((id3->elapsed + ff_rewind_count) > id3->length) ff_rewind_count = id3->length - id3->elapsed;
                } else {
                    if ((int)(id3->elapsed + ff_rewind_count) < 0) ff_rewind_count = -id3->elapsed;
                }

                gstate->ff_rewind_count = ff_rewind_count;
                FOR_NB_SCREENS(i)
                    skin_update(WPS, i,
                        wps_partial_refresh(SKIN_REFRESH_PLAYER_PROGRESS |
                                            SKIN_REFRESH_DYNAMIC));
                break;

            case ACTION_WPS_STOPSEEK:
                id3->elapsed = id3->elapsed + ff_rewind_count;
                audio_ff_rewind(id3->elapsed);
                gstate->ff_rewind_count = 0;
                ff_rewind = false;
                status_set_ffmode(0);
                exit = true;
                break;

            default:
                if(default_event_handler(button) == SYS_USB_CONNECTED) {
                    status_set_ffmode(0);
                    usb = true; exit = true;
                }
                break;
        }
        if (!exit)
        {
            button = get_action(CONTEXT_WPS|ALLOW_SOFTLOCK,TIMEOUT_BLOCK);
            #ifdef HAVE_TOUCHSCREEN
            if (button == ACTION_TOUCHSCREEN) button = skintouch_to_wps();
            #endif
            if (button != ACTION_WPS_SEEKFWD && button != ACTION_WPS_SEEKBACK && button != 0 && !IS_SYSEVENT(button))
                button = ACTION_WPS_STOPSEEK;
        }
    }
    return usb;
}

static void gwps_caption_backlight(struct wps_state *state)
{
    #if defined(HAVE_BACKLIGHT) || defined(HAVE_REMOTE_LCD)
    if (state->id3)
    {
        #ifdef HAVE_BACKLIGHT
        if (global_settings.caption_backlight)
        {
            int n = global_settings.backlight_timeout * 1000;
            if (n < 1000) n = 5000;
            if (((state->id3->elapsed < 1000) || ((state->id3->length - state->id3->elapsed) < (unsigned)n)) && (state->paused == false))
                backlight_on();
        }
        #endif
        #ifdef HAVE_REMOTE_LCD
        if (global_settings.remote_caption_backlight)
        {
            int n = global_settings.remote_backlight_timeout * 1000;
            if (n < 1000) n = 5000;
            if (((state->id3->elapsed < 1000) || ((state->id3->length - state->id3->elapsed) < (unsigned)n)) && (state->paused == false))
                remote_backlight_on();
        }
        #endif
    }
    #else
    (void) state;
    #endif
}

static void change_dir(int direction)
{
    if (global_settings.prevent_skip) return;
    if (direction < 0) audio_prev_dir();
    else if (direction > 0) audio_next_dir();
    action_wait_for_release();
}

static void prev_track(unsigned long skip_thresh)
{
    struct wps_state *state = get_wps_state();
    if (state->id3->elapsed < skip_thresh) { audio_prev(); return; }
    else {
        if (state->id3->cuesheet) { curr_cuesheet_skip(state->id3->cuesheet, -1, state->id3->elapsed); return; }
        audio_pre_ff_rewind(); audio_ff_rewind(0);
    }
}

static void next_track(void)
{
    struct wps_state *state = get_wps_state();
    if (state->id3->cuesheet) { if (curr_cuesheet_skip(state->id3->cuesheet, 1, state->id3->elapsed)) return; }
    audio_next();
}

static void play_hop(int direction)
{
    struct wps_state *state = get_wps_state();
    struct cuesheet *cue = state->id3->cuesheet;
    long step = global_settings.skip_length*1000;
    long elapsed = state->id3->elapsed;
    long remaining = state->id3->length - elapsed;

    if (cue && (cue->curr_track_idx+1 < cue->track_count))
    {
        int next = cue->curr_track_idx+1;
        remaining = cue->tracks[next].offset - elapsed;
    }

    if (step < 0)
    {
        if (direction < 0) { prev_track(DEFAULT_SKIP_THRESH); return; }
        else if (remaining < DEFAULT_SKIP_THRESH*2) { next_track(); return; }
        else elapsed += (remaining - DEFAULT_SKIP_THRESH*2);
    }
    else if (!global_settings.prevent_skip && (!step || (direction > 0 && step >= remaining) || (direction < 0 && elapsed < DEFAULT_SKIP_THRESH)))
    {
        if (direction > 0) next_track();
        else if (direction < 0) {
            if (step > 0 && global_settings.rewind_across_tracks && elapsed < DEFAULT_SKIP_THRESH && playlist_check(-1)) {
                bool audio_paused = (audio_status() & AUDIO_STATUS_PAUSE)?true:false;
                if (!audio_paused) audio_pause();
                audio_prev(); audio_ff_rewind(-step);
                if (!audio_paused) audio_resume();
                return;
            }
            prev_track(DEFAULT_SKIP_THRESH);
        }
        return;
    }
    else if (direction == 1 && step >= remaining) { system_sound_play(SOUND_TRACK_NO_MORE); return; }
    else if (direction == -1 && elapsed < step) elapsed = 0;
    else elapsed += step * direction;

    audio_pre_ff_rewind(); audio_ff_rewind(elapsed);
}

#if defined(HAVE_LCD_ENABLE) || defined(HAVE_LCD_SLEEP)
static void wps_lcd_activation_hook(unsigned short id, void *param)
{
    (void)id; (void)param;
    skin_request_full_update(WPS);
    button_queue_post(BUTTON_NONE, 0);
}
#endif

static void gwps_leave_wps(bool theme_enabled)
{
    bool restore_skin_theme = theme_enabled;

    if (restore_skin_theme)
        skin_render_inhibit_flush(true);
    FOR_NB_SCREENS(i) {
        struct gui_wps *gwps = skin_get_gwps(WPS, i);
        gwps->display->scroll_stop();
        if (restore_skin_theme)
            viewportmanager_theme_undo(i, skin_has_sbs(gwps));
    }
    if (restore_skin_theme)
        skin_render_inhibit_flush(false);
    #if defined(HAVE_LCD_ENABLE) || defined(HAVE_LCD_SLEEP)
    remove_event(LCD_EVENT_ACTIVATION, wps_lcd_activation_hook);
    #endif
    wps_state_deinit();
    sb_skin_set_update_delay(DEFAULT_UPDATE_DELAY);
    #ifdef HAVE_TOUCHSCREEN
    touchscreen_set_mode(global_settings.touch_mode);
    #endif
}

static void restore_theme(void)
{
    FOR_NB_SCREENS(i) {
        struct gui_wps *gwps = skin_get_gwps(WPS, i);
        gwps->display->scroll_stop();
        viewportmanager_theme_enable(i, skin_has_sbs(gwps), NULL);
    }
}

static void gwps_enter_wps(bool theme_enabled)
{
    wps_state_init();
    if (theme_enabled)
        restore_theme();
    FOR_NB_SCREENS(i) {
        struct gui_wps *gwps = skin_get_gwps(WPS, i);
        struct screen *display = gwps->display;
        display->scroll_stop();
        sb_set_title_text(NULL, Icon_NOICON, i);
        #if LCD_DEPTH > 1
        if (display->depth > 1) {
            struct skin_viewport *svp = skin_find_item(VP_DEFAULT_LABEL_STRING, SKIN_FIND_VP, gwps->data);
            if (svp) {
                svp->vp.fg_pattern = display->get_foreground();
                svp->vp.bg_pattern = display->get_background();
            }
        }
        #endif
        #ifdef HAVE_BACKDROP_IMAGE
        skin_backdrop_show(gwps->data->backdrop_id);
        #endif
        display->clear_display();
        skin_update(WPS, i, SKIN_REFRESH_ALL);
    }
    #ifdef HAVE_TOUCHSCREEN
    struct gui_wps *gwps = skin_get_gwps(WPS, SCREEN_MAIN);
    skin_disarm_touchregions(gwps);
    if (gwps->data->touchregions < 0) touchscreen_set_mode(TOUCHSCREEN_BUTTON);
    #endif
    send_event(GUI_EVENT_ACTIONUPDATE, (void*)1);
}

static long do_wps_exit(long action, bool bookmark)
{
    audio_pause(); update_non_static();
    if (bookmark) bookmark_autobookmark(true);
    audio_stop(); ab_reset_markers(); gwps_leave_wps(true);
    #ifdef HAVE_RECORDING
    if (action == ACTION_WPS_REC) return GO_TO_RECSCREEN;
    #endif
    return global_settings.browse_current ? GO_TO_PREVIOUS_BROWSER : GO_TO_PREVIOUS;
}

static long do_party_mode(long action)
{
    if (global_settings.party_mode)
    {
        switch (action) {
            case ACTION_WPS_PLAY: case ACTION_WPS_SEEKFWD: case ACTION_WPS_SEEKBACK:
            case ACTION_WPS_SKIPPREV: case ACTION_WPS_SKIPNEXT: case ACTION_WPS_ABSETB_NEXTDIR:
            case ACTION_WPS_ABSETA_PREVDIR: case ACTION_WPS_STOP: return ACTION_NONE;
            default: break;
        }
    }
    return action;
}

static inline int action_wpsab_single(long button)
{
    #ifdef ACTION_WPSAB_SINGLE
    static int wps_ab_state = 0;
    if (button == ACTION_WPSAB_SINGLE && ab_repeat_mode_enabled()) {
        switch (wps_ab_state) {
            case 0: button = ACTION_WPS_ABSETA_PREVDIR; break;
            case 1: button = ACTION_WPS_ABSETB_NEXTDIR; break;
            case 2: button = ACTION_WPS_ABRESET; break;
        }
        wps_ab_state = (wps_ab_state+1) % 3;
    }
    #endif
    return button;
}

long gui_wps_show(void)
{
    #ifdef USB_ENABLE_AUDIO
    if (usb_audio_get_active() && usb_audio_get_playing()) splash(HZ*2, ID2P(LANG_USB_DAC_ACTIVE));
    #endif
    long button = 0;
    bool restore = true, exit = false, bookmark = false, update = false, theme_enabled = true;
    #ifdef HAS_BUTTON_HOLD
    bool last_hold = button_hold();
    #endif
    long last_left = 0, last_right = 0;
    struct wps_state *state = get_wps_state();
#ifdef IPOD_ACCESSORY_PROTOCOL
    bool last_kokkia_present = false;
    bool last_kokkia_valid = false;
#endif

    ab_reset_markers();
    wps_state_init();

    while ( 1 )
    {
        bool hotkey = false;
        bool audio_paused = (audio_status() & AUDIO_STATUS_PAUSE)?true:false;
#ifdef IPOD_ACCESSORY_PROTOCOL
        bool kokkia_present = iap_kokkia_present();

        if (ipodjs_wps_controls() &&
            (!last_kokkia_valid ||
             kokkia_present != last_kokkia_present))
        {
            last_kokkia_present = kokkia_present;
            last_kokkia_valid = true;
            skin_request_full_update(WPS);
            update = true;
        }
        if (iap_take_kokkia_connection_event())
        {
            /* Home owns the connection animation.  Consume WPS connections
             * silently so they are not replayed after navigating back. */
            if (ipodjs_wps_controls())
            {
                skin_request_full_update(WPS);
                update = true;
            }
        }
#endif
        if (state->paused != audio_paused) {
            state->paused = audio_paused;
            DEBUGF("wps: paused state changed to %d\n", state->paused);
        }

        if (restore) {
            restore = false;
            #if defined(HAVE_LCD_ENABLE) || defined(HAVE_LCD_SLEEP)
            add_event(LCD_EVENT_ACTIVATION, wps_lcd_activation_hook);
            #endif
            sb_skin_set_update_delay(0);
            skin_request_full_update(WPS);
            update = true;
            gwps_enter_wps(theme_enabled);
#if defined(IPOD_6G)
            if (ipodjs_wps_controls())
                ipodjs_trace_wps("full-skin", 0, 0, LCD_WIDTH,
                                  LCD_HEIGHT, NULL, 0, 0);
#endif
            theme_enabled = true;
        } else {
            bool skin_updated = false;
            bool skin_full_updated = false;

            gwps_caption_backlight(state);
            #ifdef HAS_BUTTON_HOLD
            if (button_hold() != last_hold) {
                last_hold = button_hold();
                global_status.last_volume_change = 0;
#if defined(IPOD_6G)
                /* The stock iPodJS header owns a dynamic Hold branch. A full
                 * WPS clear would erase static art and metadata before the
                 * bounded indicator refresh restores them. */
                if (!ipodjs_wps_controls())
#endif
                    skin_request_full_update(WPS);
#if defined(IPOD_6G)
                if (ipodjs_wps_controls() && last_hold)
                    ipodjs_trace_screen("Lockscreen", "skin", 0, 0, 0,
                                        0, 0, LCD_WIDTH, LCD_HEIGHT);
#endif
                update = true;
            }
            #endif
            FOR_NB_SCREENS(i) {
                #if defined(HAVE_LCD_ENABLE) || defined(HAVE_LCD_SLEEP)
                if (lcd_active() || (i != SCREEN_MAIN))
                    #endif
                {
                    bool full_update = skin_do_full_update(WPS, i);
                    if (update || full_update)
                    {
                        skin_update(WPS, i, full_update ?
                                    SKIN_REFRESH_ALL :
                                    wps_partial_refresh(
                                        SKIN_REFRESH_NON_STATIC));
                        skin_updated = true;
                        skin_full_updated |= full_update;
                    }
                }
            }
#if defined(IPOD_6G)
            if (ipodjs_wps_controls() && skin_updated)
                ipodjs_trace_wps(skin_full_updated ? "full-skin" : "bottom",
                                  0, 0, LCD_WIDTH, LCD_HEIGHT,
                                  NULL, 0, 0);
#else
            (void)skin_updated;
            (void)skin_full_updated;
#endif
            update = false;
        }

        if (exit) return do_wps_exit(button, bookmark);
        if (button && !IS_SYSEVENT(button)) storage_spin();

        button = skin_wait_for_action(WPS,
                                      CONTEXT_WPS|ALLOW_SOFTLOCK,
                                      HZ/5);
        if (!(audio_status() & AUDIO_STATUS_PLAY)) exit = true;
        #ifdef HAVE_TOUCHSCREEN
        if (button == ACTION_TOUCHSCREEN) button = skintouch_to_wps();
        #endif
        button = do_party_mode(button);
        button = action_wpsab_single(button);
        switch(button)
        {
            #ifdef HAVE_HOTKEY
            case ACTION_WPS_HOTKEY:
                hotkey = true;
                if (!global_settings.hotkey_wps) break;
                if (get_hotkey(global_settings.hotkey_wps)->flags & HOTKEY_FLAG_NOSBS) {
                    theme_enabled = false; gwps_leave_wps(false);
                    onplay(state->id3->path, FILE_ATTR_AUDIO, CONTEXT_WPS, hotkey, ONPLAY_NO_CUSTOMACTION);
                    if (!audio_status()) { gwps_leave_wps(true); return GO_TO_ROOT; }
                    restore = true; break;
                }
                #endif
            case ACTION_WPS_CONTEXT:
            {
                int plugin_ret;
#if defined(IPOD_6G)
                if (ipodjs_wps_controls())
                    ipodjs_trace_screen("WPS Lyrics", "launch", 0, 0, 0,
                                        0, 0, LCD_WIDTH, LCD_HEIGHT);
#endif
                theme_enabled = false; gwps_leave_wps(false);
                plugin_ret = open_plugin_run(PLUGIN_APPS_DIR "/lrcplayer.rock");
                if (!(audio_status() & AUDIO_STATUS_PLAY)) { gwps_leave_wps(true); return GO_TO_WPS; }
#if defined(IPOD_6G)
                if (ipodjs_wps_controls())
                    ipodjs_trace_screen("WPS Lyrics Return",
                        plugin_ret == PLUGIN_GOTO_ROOT ? "menu" : "other",
                        plugin_ret, 0, 0, 0, 0, LCD_WIDTH, LCD_HEIGHT);
                if (ipodjs_wps_controls() &&
                    plugin_ret == PLUGIN_GOTO_ROOT)
                {
                    /* WPS was already left before plugin_load().  The plugin
                     * restored the theme during normal teardown, so return
                     * directly without reconstructing or redrawing WPS. */
                    return GO_TO_ROOT;
                }
#else
                (void)plugin_ret;
#endif
                restore = true; break;
            }

            case ACTION_WPS_BROWSE:
            {
#if defined(IPOD_6G)
                if (ipodjs_wps_controls())
                {
                    /* Stock iPodJS semantics: a short center press does not
                     * leave Now Playing. Long center is ACTION_WPS_CONTEXT
                     * and launches Lyrics directly above. */
                    ipodjs_trace_screen("WPS Select", "ignored", 0, 0, 0,
                                        0, 0, LCD_WIDTH, LCD_HEIGHT);
                    update = true;
                    break;
                }
#endif
                int sel_action = global_settings.wps_select_action;
                if (sel_action == 1) {
                    #ifdef HAVE_TAGCACHE
                    gwps_leave_wps(true); return GO_TO_DBBROWSER;
                    #endif
                } else if (sel_action == 2) {
                    theme_enabled = false; gwps_leave_wps(false);
                    filetype_load_plugin("pictureflow", NULL);
                    if (!(audio_status() & AUDIO_STATUS_PLAY)) { gwps_leave_wps(true); return GO_TO_WPS; }
                    restore = true;
                } else if (sel_action == 3) { gwps_leave_wps(true); return GO_TO_FILEBROWSER; }
                else if (sel_action == 4) {
                    theme_enabled = false; gwps_leave_wps(false);
                    open_plugin_run(PLUGIN_APPS_DIR "/lrcplayer.rock");
                    if (!(audio_status() & AUDIO_STATUS_PLAY)) { gwps_leave_wps(true); return GO_TO_WPS; }
                    restore = true;
                } else { gwps_leave_wps(true); return GO_TO_PREVIOUS_BROWSER; }
            } break;

            case ACTION_WPS_PLAY:
                wps_do_action(WPS_PLAYPAUSE, true);
                break;

            case ACTION_WPS_VOLUP:
            case ACTION_WPS_VOLDOWN:
                adjust_volume(button == ACTION_WPS_VOLUP ? 1 : -1);
                update = true; break;

            case ACTION_WPS_SEEKFWD:
                if (current_tick - last_right < HZ) {
                    if (state->id3->cuesheet && playlist_check(1)) audio_next();
                    else change_dir(1);
                } else ffwd_rew(ACTION_WPS_SEEKFWD, false);
                last_right = last_left = 0; break;

            case ACTION_WPS_SEEKBACK:
                if (current_tick - last_left < HZ) {
                    if (state->id3->cuesheet && playlist_check(-1)) audio_prev();
                    else change_dir(-1);
                } else if (global_settings.rewind_across_tracks && get_wps_state()->id3->elapsed < DEFAULT_SKIP_THRESH && playlist_check(-1)) {
                    bool was_paused = (audio_status() & AUDIO_STATUS_PAUSE)?true:false;
                    if (!was_paused) audio_pause();
                    audio_prev(); ffwd_rew(ACTION_WPS_SEEKBACK, true);
                    if (!was_paused) audio_resume();
                } else ffwd_rew(ACTION_WPS_SEEKBACK, false);
                last_left = last_right = 0; break;

            case ACTION_WPS_SKIPPREV:
                last_left = current_tick;
                if ( ab_repeat_mode_enabled() && ab_after_A_marker(state->id3->elapsed) ) ab_jump_to_A_marker();
                else play_hop(-1);
                break;

            case ACTION_WPS_SKIPNEXT:
                last_right = current_tick;
                if ( ab_repeat_mode_enabled() ) {
                    if ( ab_before_A_marker(state->id3->elapsed) ) ab_jump_to_A_marker();
                } else play_hop(1);
                break;

            case ACTION_WPS_ABSETB_NEXTDIR:
                if (ab_repeat_mode_enabled()) { ab_set_B_marker(state->id3->elapsed); ab_jump_to_A_marker(); }
                else change_dir(1); break;
            case ACTION_WPS_ABSETA_PREVDIR:
                if (ab_repeat_mode_enabled()) ab_set_A_marker(state->id3->elapsed);
                else change_dir(-1); break;

            case ACTION_WPS_MENU:
#if defined(IPOD_6G)
                if (ipodjs_wps_controls())
                    action_wait_for_release();
#endif
                gwps_leave_wps(true);
                return GO_TO_ROOT;

            #ifdef HAVE_QUICKSCREEN
            case ACTION_WPS_QUICKSCREEN:
                gwps_leave_wps(true);
                if (!global_settings.shortcuts_replaces_qs) {
                    int ret = quick_screen_quick(button);
                    if (ret == QUICKSCREEN_IN_USB) return GO_TO_ROOT;
                    else if (ret != QUICKSCREEN_GOTO_SHORTCUTS_MENU) { restore = true; break; }
                }
                global_status.last_screen = GO_TO_SHORTCUTMENU;
                int s_ret = do_shortcut_menu(NULL);
                return (s_ret == GO_TO_PREVIOUS ? GO_TO_WPS : s_ret);
                #endif

                #ifdef HAVE_PITCHCONTROL
            case ACTION_WPS_PITCHSCREEN:
                gwps_leave_wps(true);
                if (1 == gui_syncpitchscreen_run()) return GO_TO_ROOT;
                restore = true; break;
            #endif

            case ACTION_WPS_ABRESET: if (ab_repeat_mode_enabled()) { ab_reset_markers(); update = true; } break;

            case ACTION_WPS_STOP:
                theme_enabled = false; gwps_leave_wps(false);
                filetype_load_plugin("pictureflow", NULL);
                if (!(audio_status() & AUDIO_STATUS_PLAY)) { gwps_leave_wps(true); return GO_TO_WPS; }
                restore = true; break;

            case ACTION_WPS_LIST_BOOKMARKS:
                gwps_leave_wps(true);
                if (bookmark_load_menu() == BOOKMARK_USB_CONNECTED) return GO_TO_ROOT;
                restore = true; break;

            case ACTION_WPS_CREATE_BOOKMARK: gwps_leave_wps(true); bookmark_create_menu(); restore = true; break;

            case ACTION_WPS_ID3SCREEN:
                gwps_leave_wps(true);
                if (browse_id3(audio_current_track(), playlist_get_display_index(), playlist_amount(), NULL, 1, NULL)) return GO_TO_ROOT;
                restore = true; break;

            case ACTION_REDRAW: skin_request_full_update(WPS); skin_request_full_update(CUSTOM_STATUSBAR); break;

            case ACTION_NONE: update = true; ffwd_rew(button, false); break;

            #ifdef HAVE_RECORDING
            case ACTION_WPS_REC: exit = true; break;
            #endif
            case ACTION_WPS_VIEW_PLAYLIST:
#if defined(IPOD_6G)
                if (ipodjs_wps_controls())
                {
                    /* No WPS gesture may hand iPodJS directly to the current
                     * playlist viewer. It is intentionally reachable only
                     * through normal menu navigation. */
                    ipodjs_trace_screen("WPS Select", "playlist-blocked",
                                        0, 0, 0, 0, 0,
                                        LCD_WIDTH, LCD_HEIGHT);
                    update = true;
                    break;
                }
#endif
                gwps_leave_wps(true);
                return GO_TO_PLAYLIST_VIEWER;

            default:
                switch(default_event_handler(button)) {
                    case SYS_USB_CONNECTED: case SYS_CALL_INCOMING: case BUTTON_MULTIMEDIA_STOP:
                        gwps_leave_wps(true); return GO_TO_ROOT;
                }
                update = true; break;
        }
    }
    return GO_TO_ROOT;
}

struct wps_state *get_wps_state(void) { return &wps_state; }

static void track_info_callback(unsigned short id, void *param)
{
    struct wps_state *state = get_wps_state();
    if (id == PLAYBACK_EVENT_TRACK_CHANGE || id == PLAYBACK_EVENT_CUR_TRACK_READY) {
        state->id3 = ((struct track_event *)param)->id3;
        if (state->id3->cuesheet) cue_find_current_track(state->id3->cuesheet, state->id3->elapsed);
    }
    #ifdef AUDIO_FAST_SKIP_PREVIEW
    else if (id == PLAYBACK_EVENT_TRACK_SKIP) state->id3 = audio_current_track();
    #endif
    if (id == PLAYBACK_EVENT_NEXTTRACKID3_AVAILABLE) state->nid3 = audio_next_track();
    skin_request_full_update(WPS);
}

void wps_state_init(void)
{
    struct wps_state *state = get_wps_state();
    state->paused = (audio_status() & AUDIO_STATUS_PAUSE) ? true : false;
    if(audio_status() & AUDIO_STATUS_PLAY) { state->id3 = audio_current_track(); state->nid3 = audio_next_track(); }
    else { state->id3 = NULL; state->nid3 = NULL; }
    skin_request_full_update(WPS);
    if (wps_playback_events_registered)
        return;
    add_event(PLAYBACK_EVENT_TRACK_CHANGE, track_info_callback);
    add_event(PLAYBACK_EVENT_NEXTTRACKID3_AVAILABLE, track_info_callback);
    add_event(PLAYBACK_EVENT_CUR_TRACK_READY, track_info_callback);
    #ifdef AUDIO_FAST_SKIP_PREVIEW
    add_event(PLAYBACK_EVENT_TRACK_SKIP, track_info_callback);
    #endif
    wps_playback_events_registered = true;
    DEBUGF("wps: playback callbacks registered\n");
}

void wps_state_deinit(void)
{
    if (!wps_playback_events_registered)
        return;

    remove_event(PLAYBACK_EVENT_TRACK_CHANGE, track_info_callback);
    remove_event(PLAYBACK_EVENT_NEXTTRACKID3_AVAILABLE, track_info_callback);
    remove_event(PLAYBACK_EVENT_CUR_TRACK_READY, track_info_callback);
    #ifdef AUDIO_FAST_SKIP_PREVIEW
    remove_event(PLAYBACK_EVENT_TRACK_SKIP, track_info_callback);
    #endif
    wps_playback_events_registered = false;
    DEBUGF("wps: playback callbacks unregistered\n");
}
