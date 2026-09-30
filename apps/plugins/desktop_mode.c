/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> ) \___\|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/             \/
 *
 * Copyright (C) 2026 The Rockbox Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the license, or (at your option) any later version.
 *
 ****************************************************************************/

#include "plugin.h"
#include <errno.h>
#include "fixedpoint.h"
#include "lib/pluginlib_bmp.h"
#include "lib/desktop_model.h"
#include "lib/desktop_surface.h"
#include "lib/photo_library.h"

#define DM_ASSET_DIR \
    PLUGIN_APPS_DATA_DIR "/desktop_mode_snow_leopard"
#define DM_CONFIG_FILE PLUGIN_APPS_DATA_DIR "/desktop_mode.cfg"
#define DM_MANIFEST_FILE DM_ASSET_DIR "/manifest.json"
#define DM_VIDEOOUT_LAUNCH_TOKEN "videoout-active"
#define DM_SIMULATOR_LAUNCH_TOKEN "simulator-desktop"
#define DM_LIVETV_UNDERLAY_FILE \
    PLUGIN_APPS_DATA_DIR "/desktop_mode_livetv_underlay.raw"
#define DM_LIVETV_UNDERLAY_MAGIC 0x44545631u /* "DTV1" */
#define DM_SITEKICK_UNDERLAY_FILE \
    PLUGIN_APPS_DATA_DIR "/desktop_mode_sitekick_underlay.raw"
#define DM_SITEKICK_UNDERLAY_MAGIC 0x44534b31u /* "DSK1" */
#define DM_NETFLIX_UNDERLAY_FILE \
    PLUGIN_APPS_DATA_DIR "/desktop_mode_netflix_underlay.raw"
#define DM_NETFLIX_UNDERLAY_MAGIC 0x444e4631u /* "DNF1" */
#define DM_STEAM_UNDERLAY_FILE \
    PLUGIN_APPS_DATA_DIR "/desktop_mode_steam_underlay.raw"
#define DM_STEAM_UNDERLAY_MAGIC 0x44535431u /* "DST1" */
#define DM_F_DASH_CLOCK \
    DM_ASSET_DIR "/320x240/dashboard/world-clock.74x74x16.rga"
#define DM_F_DASH_ICAL \
    DM_ASSET_DIR "/320x240/dashboard/ical.104x51x16.rga"
#define DM_F_DASH_WEATHER \
    DM_ASSET_DIR "/320x240/dashboard/weather.104x59x16.rga"
#define DM_F_DASH_STICKIES \
    DM_ASSET_DIR "/320x240/dashboard/stickies.96x88x16.rga"
#define DM_F_DASH_ITUNES \
    DM_ASSET_DIR "/320x240/dashboard/itunes.196x82x16.rga"

#define DM_MAX_FILES 96
#define DM_ASCII_FIRST 32
#define DM_ASCII_LAST 126
#define DM_ASCII_COUNT (DM_ASCII_LAST - DM_ASCII_FIRST + 1)
#define DM_FONT_COLUMNS 16
#define DM_FONT_ROWS ((DM_ASCII_COUNT + DM_FONT_COLUMNS - 1) / DM_FONT_COLUMNS)

/* Set by the session before any geometry or hit testing is performed. */
static bool dm_fullscreen;
static int dm_window_dx;
static int dm_window_dy;

/* Every number below is measured from the real Snow Leopard artwork the
 * importer cuts the pack from; see docs/desktop-mode-snow-leopard-spec.md.
 * On an iPod 6G the compositor is deliberately the native 320x240 panel.
 * The DCP750 video-out driver performs the final NTSC conversion and scales
 * that complete frame into its qualified 648x432 viewport.  Desktop Mode
 * must never treat the TV output raster as its drawing canvas.  The optional
 * 1920x1080 profile is host-simulator only.
 */
#define DM_APPLE_W 22

#define DM_DOCK_SLOTS 8
/* Window chrome bands are the same 1:1 heights on either profile: they are
 * the same real Finder window, used whole by the host simulator and in
 * fragments by the native 320x240 TV-out source. */
#define DM_WIN_TITLE_H 24
#define DM_WIN_TOOLBAR_H 29
#define DM_WIN_STATUS_H 24
#define DM_BODY_Y (DM_WIN_Y + DM_WIN_TITLE_H + DM_WIN_TOOLBAR_H)
#define DM_BODY_H \
    (DM_WIN_H - DM_WIN_TITLE_H - DM_WIN_TOOLBAR_H - DM_WIN_STATUS_H)
#define DM_STATUS_Y (DM_BODY_Y + DM_BODY_H)
#define DM_LIST_X (DM_WIN_X + DM_WIN_SIDEBAR_W + 1)
#define DM_LIST_W (DM_WIN_W - DM_WIN_SIDEBAR_W - 1)
#define DM_FILE_ROWS (DM_BODY_H / DM_ROW_H)
#define DM_SCROLLER_W 16
#define DM_SCROLLER_X (DM_WIN_X + DM_WIN_W - DM_SCROLLER_W - 1)

/* Panel profile: filenames and geometry are generated from the same table
 * the importer uses, so a pack and the shell can never disagree about a
 * size.  At 320x240 the 1:1 artwork is used in fragments; at 1920x1080 it
 * is used at the size Apple drew it.
 */
#if LCD_WIDTH >= 1920
#define DM_F_BOOT_BACKGROUND DM_ASSET_DIR "/1920x1080/boot/background.1920x1080x16.bmp"
#define DM_F_AURORA DM_ASSET_DIR "/1920x1080/desktop/aurora.1920x1080x16.bmp"
#define DM_F_MENUBAR DM_ASSET_DIR "/1920x1080/desktop/menubar.1920x22x16.bmp"
#define DM_F_APPLE_HL DM_ASSET_DIR "/1920x1080/desktop/apple-highlight.22x22x16.bmp"
#define DM_F_DOCK_SHELF DM_ASSET_DIR "/1920x1080/desktop/dock-shelf.608x52x16.bmp"
#define DM_F_WIN_SIDEBAR DM_ASSET_DIR "/1920x1080/chrome/window-sidebar.897x671x16.bmp"
#define DM_F_WIN_PLAIN DM_ASSET_DIR "/1920x1080/chrome/window-plain.897x671x16.bmp"
#define DM_F_ITUNES_WIN DM_ASSET_DIR "/1920x1080/chrome/itunes-window.1081x655x16.bmp"
#define DM_F_ITUNES_SEL DM_ASSET_DIR "/1920x1080/chrome/itunes-selection.1081x17x16.bmp"
#define DM_F_LIST_SEL DM_ASSET_DIR "/1920x1080/chrome/list-selection.761x19x16.bmp"
#define DM_F_SIDEBAR_SEL DM_ASSET_DIR "/1920x1080/chrome/sidebar-selection.135x19x16.bmp"
#define DM_F_MENU_PANEL DM_ASSET_DIR "/1920x1080/chrome/menu-panel.226x279x16.bmp"
#define DM_F_MENU_SEL DM_ASSET_DIR "/1920x1080/chrome/menu-selection.224x19x16.bmp"
#define DM_F_CONTEXT DM_ASSET_DIR "/1920x1080/chrome/context-panel.200x90x16.bmp"
#define DM_F_SHEET DM_ASSET_DIR "/1920x1080/chrome/sheet.520x260x16.bmp"
#define DM_F_TOOLTIP DM_ASSET_DIR "/1920x1080/chrome/tooltip.180x22x16.bmp"
#define DM_F_SCROLL_TRACK DM_ASSET_DIR "/1920x1080/chrome/scroller-track.16x430x16.bmp"
#define DM_F_SCROLL_THUMB DM_ASSET_DIR "/1920x1080/chrome/scroller-thumb.16x80x16.bmp"
#define DM_F_ICON_FINDER DM_ASSET_DIR "/1920x1080/icons/finder.64x64.rga"
#define DM_F_ICON_ITUNES DM_ASSET_DIR "/1920x1080/icons/itunes.64x64.rga"
#define DM_F_ICON_PREVIEW DM_ASSET_DIR "/1920x1080/icons/preview.64x64.rga"
#define DM_F_ICON_TEXTEDIT DM_ASSET_DIR "/1920x1080/icons/textedit.64x64.rga"
#define DM_F_ICON_CALCULATOR DM_ASSET_DIR "/1920x1080/icons/calculator.64x64.rga"
#define DM_F_ICON_DIRECTV DM_ASSET_DIR "/1920x1080/icons/directv.64x64.rga"
#define DM_F_ICON_SITEKICK ROCKBOX_DIR "/sitekick/desktop/icon.64x64.rga"
#define DM_F_ICON_STEAM ROCKBOX_DIR "/ipodjs/steam/desktop/icon.64x64.rga"
#define DM_F_ICON_NETFLIX \
    ROCKBOX_DIR "/ipodjs/netflix/desktop/icon.64x64.rga"
#define DM_F_ICON_SYSTEM_PREFERENCES DM_ASSET_DIR "/1920x1080/icons/system-preferences.64x64.rga"
#define DM_F_ICON_DASHBOARD DM_ASSET_DIR "/1920x1080/icons/dashboard.64x64.rga"
#define DM_F_ICON_DISK DM_ASSET_DIR "/1920x1080/icons/disk.64x64.rga"
#define DM_F_ICON_TRASH_EMPTY DM_ASSET_DIR "/1920x1080/icons/trash-empty.64x64.rga"
#define DM_F_ICON_FOLDER_DESKTOP DM_ASSET_DIR "/1920x1080/icons/folder-desktop.64x64.rga"
#define DM_F_ICON_FOLDER DM_ASSET_DIR "/1920x1080/icons/folder.16x16.rga"
#define DM_F_ICON_DOCUMENT DM_ASSET_DIR "/1920x1080/icons/document.16x16.rga"
#define DM_F_DOCK_FINDER_A DM_ASSET_DIR "/1920x1080/icons/finder-dock-66.66x66.rga"
#define DM_F_DOCK_FINDER_B DM_ASSET_DIR "/1920x1080/icons/finder-dock-70.70x70.rga"
#define DM_F_DOCK_ITUNES_A DM_ASSET_DIR "/1920x1080/icons/itunes-dock-66.66x66.rga"
#define DM_F_DOCK_ITUNES_B DM_ASSET_DIR "/1920x1080/icons/itunes-dock-70.70x70.rga"
#define DM_F_DOCK_PREVIEW_A DM_ASSET_DIR "/1920x1080/icons/preview-dock-66.66x66.rga"
#define DM_F_DOCK_PREVIEW_B DM_ASSET_DIR "/1920x1080/icons/preview-dock-70.70x70.rga"
#define DM_F_DOCK_TEXTEDIT_A DM_ASSET_DIR "/1920x1080/icons/textedit-dock-66.66x66.rga"
#define DM_F_DOCK_TEXTEDIT_B DM_ASSET_DIR "/1920x1080/icons/textedit-dock-70.70x70.rga"
#define DM_F_DOCK_CALCULATOR_A DM_ASSET_DIR "/1920x1080/icons/calculator-dock-66.66x66.rga"
#define DM_F_DOCK_CALCULATOR_B DM_ASSET_DIR "/1920x1080/icons/calculator-dock-70.70x70.rga"
#define DM_F_DOCK_DASHBOARD_A DM_ASSET_DIR "/1920x1080/icons/dashboard-dock-66.66x66.rga"
#define DM_F_DOCK_DASHBOARD_B DM_ASSET_DIR "/1920x1080/icons/dashboard-dock-70.70x70.rga"
#define DM_F_DOCK_DIRECTV_A DM_ASSET_DIR "/1920x1080/icons/directv-dock-66.66x66.rga"
#define DM_F_DOCK_DIRECTV_B DM_ASSET_DIR "/1920x1080/icons/directv-dock-70.70x70.rga"
#define DM_F_DOCK_SITEKICK_A ROCKBOX_DIR "/sitekick/desktop/icon-dock-66.66x66.rga"
#define DM_F_DOCK_SITEKICK_B ROCKBOX_DIR "/sitekick/desktop/icon-dock-70.70x70.rga"
#define DM_F_DOCK_STEAM_A \
    ROCKBOX_DIR "/ipodjs/steam/desktop/icon-dock-66.66x66.rga"
#define DM_F_DOCK_STEAM_B \
    ROCKBOX_DIR "/ipodjs/steam/desktop/icon-dock-70.70x70.rga"
#define DM_F_DOCK_NETFLIX_A \
    ROCKBOX_DIR "/ipodjs/netflix/desktop/icon-dock-66.66x66.rga"
#define DM_F_DOCK_NETFLIX_B \
    ROCKBOX_DIR "/ipodjs/netflix/desktop/icon-dock-70.70x70.rga"
#define DM_F_DOCK_SYSTEM_PREFERENCES_A DM_ASSET_DIR "/1920x1080/icons/system-preferences-dock-66.66x66.rga"
#define DM_F_DOCK_SYSTEM_PREFERENCES_B DM_ASSET_DIR "/1920x1080/icons/system-preferences-dock-70.70x70.rga"
#define DM_F_DOCK_TRASH_EMPTY_A DM_ASSET_DIR "/1920x1080/icons/trash-empty-dock-66.66x66.rga"
#define DM_F_DOCK_TRASH_EMPTY_B DM_ASSET_DIR "/1920x1080/icons/trash-empty-dock-70.70x70.rga"
#define DM_MENUBAR_H 22
#define DM_DOCK_SHELF_W 608
#define DM_DOCK_SHELF_H 52
#define DM_DOCK_ICON 64
#define DM_DOCK_STEP 60
#define DM_DOCK_MAG_A 66
#define DM_DOCK_MAG_B 70
#define DM_CHROME_W 897
#define DM_CHROME_H 671
#define DM_WIN_SIDEBAR_W 135
#define DM_ITUNES_CHROME_W 1081
#define DM_ITUNES_CHROME_H 655
#define DM_ITUNES_ASSET_H 655
#define DM_ITUNES_SOURCE_W 206
#define DM_LIST_SEL_W 761
#define DM_ROW_H 19
#define DM_MENU_W 226
#define DM_MENU_H 279
#define DM_CONTEXT_W 200
#define DM_CONTEXT_H 90
#define DM_SHEET_W 520
#define DM_SHEET_H 260
#define DM_TOOLTIP_W 180
#define DM_TOOLTIP_H 22
#define DM_SCROLLER_H_ASSET 430
#define DM_SCROLLER_THUMB_H 80
#define DM_ICON_SMALL 16
#elif LCD_WIDTH == 640
#define DM_F_BOOT_BACKGROUND DM_ASSET_DIR "/640x480/boot/background.640x480x16.bmp"
#define DM_F_AURORA DM_ASSET_DIR "/640x480/desktop/aurora.640x480x16.bmp"
#define DM_F_MENUBAR DM_ASSET_DIR "/640x480/desktop/menubar.640x21x16.bmp"
#define DM_F_APPLE_HL DM_ASSET_DIR "/640x480/desktop/apple-highlight.22x21x16.bmp"
#define DM_F_DOCK_SHELF DM_ASSET_DIR "/640x480/desktop/dock-shelf.288x26x16.bmp"
#define DM_F_WIN_SIDEBAR DM_ASSET_DIR "/640x480/chrome/window-sidebar.608x382x16.bmp"
#define DM_F_WIN_PLAIN DM_ASSET_DIR "/640x480/chrome/window-plain.608x382x16.bmp"
#define DM_F_ITUNES_WIN DM_ASSET_DIR "/640x480/chrome/itunes-window.608x382x16.bmp"
#define DM_F_ITUNES_SEL DM_ASSET_DIR "/640x480/chrome/itunes-selection.608x17x16.bmp"
#define DM_F_LIST_SEL DM_ASSET_DIR "/640x480/chrome/list-selection.497x19x16.bmp"
#define DM_F_SIDEBAR_SEL DM_ASSET_DIR "/640x480/chrome/sidebar-selection.110x19x16.bmp"
#define DM_F_MENU_PANEL DM_ASSET_DIR "/640x480/chrome/menu-panel.152x124x16.bmp"
#define DM_F_MENU_SEL DM_ASSET_DIR "/640x480/chrome/menu-selection.150x19x16.bmp"
#define DM_F_CONTEXT DM_ASSET_DIR "/640x480/chrome/context-panel.124x71x16.bmp"
#define DM_F_SHEET DM_ASSET_DIR "/640x480/chrome/sheet.240x112x16.bmp"
#define DM_F_TOOLTIP DM_ASSET_DIR "/640x480/chrome/tooltip.104x18x16.bmp"
#define DM_F_SCROLL_TRACK DM_ASSET_DIR "/640x480/chrome/scroller-track.16x305x16.bmp"
#define DM_F_SCROLL_THUMB DM_ASSET_DIR "/640x480/chrome/scroller-thumb.16x36x16.bmp"
#define DM_F_ICON_FINDER DM_ASSET_DIR "/640x480/icons/finder.32x32.rga"
#define DM_F_ICON_ITUNES DM_ASSET_DIR "/640x480/icons/itunes.32x32.rga"
#define DM_F_ICON_PREVIEW DM_ASSET_DIR "/640x480/icons/preview.32x32.rga"
#define DM_F_ICON_TEXTEDIT DM_ASSET_DIR "/640x480/icons/textedit.32x32.rga"
#define DM_F_ICON_CALCULATOR DM_ASSET_DIR "/640x480/icons/calculator.32x32.rga"
#define DM_F_ICON_DIRECTV DM_ASSET_DIR "/640x480/icons/directv.32x32.rga"
#define DM_F_ICON_SITEKICK ROCKBOX_DIR "/sitekick/desktop/icon.32x32.rga"
#define DM_F_ICON_STEAM ROCKBOX_DIR "/ipodjs/steam/desktop/icon.32x32.rga"
#define DM_F_ICON_NETFLIX \
    ROCKBOX_DIR "/ipodjs/netflix/desktop/icon.32x32.rga"
#define DM_F_ICON_SYSTEM_PREFERENCES DM_ASSET_DIR "/640x480/icons/system-preferences.32x32.rga"
#define DM_F_ICON_DASHBOARD DM_ASSET_DIR "/640x480/icons/dashboard.32x32.rga"
#define DM_F_ICON_DISK DM_ASSET_DIR "/640x480/icons/disk.32x32.rga"
#define DM_F_ICON_TRASH_EMPTY DM_ASSET_DIR "/640x480/icons/trash-empty.32x32.rga"
#define DM_F_ICON_FOLDER_DESKTOP DM_ASSET_DIR "/640x480/icons/folder-desktop.32x32.rga"
#define DM_F_ICON_FOLDER DM_ASSET_DIR "/640x480/icons/folder.16x16.rga"
#define DM_F_ICON_DOCUMENT DM_ASSET_DIR "/640x480/icons/document.16x16.rga"
#define DM_F_DOCK_FINDER_A DM_ASSET_DIR "/640x480/icons/finder-dock-34.34x34.rga"
#define DM_F_DOCK_FINDER_B DM_ASSET_DIR "/640x480/icons/finder-dock-38.38x38.rga"
#define DM_F_DOCK_ITUNES_A DM_ASSET_DIR "/640x480/icons/itunes-dock-34.34x34.rga"
#define DM_F_DOCK_ITUNES_B DM_ASSET_DIR "/640x480/icons/itunes-dock-38.38x38.rga"
#define DM_F_DOCK_PREVIEW_A DM_ASSET_DIR "/640x480/icons/preview-dock-34.34x34.rga"
#define DM_F_DOCK_PREVIEW_B DM_ASSET_DIR "/640x480/icons/preview-dock-38.38x38.rga"
#define DM_F_DOCK_TEXTEDIT_A DM_ASSET_DIR "/640x480/icons/textedit-dock-34.34x34.rga"
#define DM_F_DOCK_TEXTEDIT_B DM_ASSET_DIR "/640x480/icons/textedit-dock-38.38x38.rga"
#define DM_F_DOCK_CALCULATOR_A DM_ASSET_DIR "/640x480/icons/calculator-dock-34.34x34.rga"
#define DM_F_DOCK_CALCULATOR_B DM_ASSET_DIR "/640x480/icons/calculator-dock-38.38x38.rga"
#define DM_F_DOCK_DASHBOARD_A DM_ASSET_DIR "/640x480/icons/dashboard-dock-34.34x34.rga"
#define DM_F_DOCK_DASHBOARD_B DM_ASSET_DIR "/640x480/icons/dashboard-dock-38.38x38.rga"
#define DM_F_DOCK_DIRECTV_A DM_ASSET_DIR "/640x480/icons/directv-dock-34.34x34.rga"
#define DM_F_DOCK_DIRECTV_B DM_ASSET_DIR "/640x480/icons/directv-dock-38.38x38.rga"
#define DM_F_DOCK_SITEKICK_A ROCKBOX_DIR "/sitekick/desktop/icon-dock-34.34x34.rga"
#define DM_F_DOCK_SITEKICK_B ROCKBOX_DIR "/sitekick/desktop/icon-dock-38.38x38.rga"
#define DM_F_DOCK_STEAM_A \
    ROCKBOX_DIR "/ipodjs/steam/desktop/icon-dock-34.34x34.rga"
#define DM_F_DOCK_STEAM_B \
    ROCKBOX_DIR "/ipodjs/steam/desktop/icon-dock-38.38x38.rga"
#define DM_F_DOCK_NETFLIX_A \
    ROCKBOX_DIR "/ipodjs/netflix/desktop/icon-dock-34.34x34.rga"
#define DM_F_DOCK_NETFLIX_B \
    ROCKBOX_DIR "/ipodjs/netflix/desktop/icon-dock-38.38x38.rga"
#define DM_F_DOCK_SYSTEM_PREFERENCES_A DM_ASSET_DIR "/640x480/icons/system-preferences-dock-34.34x34.rga"
#define DM_F_DOCK_SYSTEM_PREFERENCES_B DM_ASSET_DIR "/640x480/icons/system-preferences-dock-38.38x38.rga"
#define DM_F_DOCK_TRASH_EMPTY_A DM_ASSET_DIR "/640x480/icons/trash-empty-dock-34.34x34.rga"
#define DM_F_DOCK_TRASH_EMPTY_B DM_ASSET_DIR "/640x480/icons/trash-empty-dock-38.38x38.rga"
#define DM_MENUBAR_H 21
#define DM_DOCK_SHELF_W 288
#define DM_DOCK_SHELF_H 26
#define DM_DOCK_ICON 32
#define DM_DOCK_STEP 34
#define DM_DOCK_MAG_A 34
#define DM_DOCK_MAG_B 38
#define DM_CHROME_W 608
#define DM_CHROME_H 382
#define DM_WIN_SIDEBAR_W 110
#define DM_ITUNES_CHROME_W 608
/* iTunes uses the same compact work-area window as Finder.  Its fullscreen
 * state expands inside Desktop Mode; normal mode keeps the Dock visible. */
#define DM_ITUNES_CHROME_H DM_ITUNES_ASSET_H
#define DM_ITUNES_ASSET_H 382
#define DM_ITUNES_SOURCE_W 110
#define DM_LIST_SEL_W 497
#define DM_ROW_H 19
#define DM_MENU_W 152
#define DM_MENU_H 124
#define DM_CONTEXT_W 124
#define DM_CONTEXT_H 71
#define DM_SHEET_W 240
#define DM_SHEET_H 112
#define DM_TOOLTIP_W 104
#define DM_TOOLTIP_H 18
#define DM_SCROLLER_H_ASSET 305
#define DM_SCROLLER_THUMB_H 36
#define DM_ICON_SMALL 16
#else
#define DM_F_BOOT_BACKGROUND DM_ASSET_DIR "/320x240/boot/background.320x240x16.bmp"
#define DM_F_AURORA DM_ASSET_DIR "/320x240/desktop/aurora.320x240x16.bmp"
#define DM_F_MENUBAR DM_ASSET_DIR "/320x240/desktop/menubar.320x21x16.bmp"
#define DM_F_APPLE_HL DM_ASSET_DIR "/320x240/desktop/apple-highlight.22x21x16.bmp"
#define DM_F_DOCK_SHELF DM_ASSET_DIR "/320x240/desktop/dock-shelf.288x26x16.bmp"
#define DM_F_WIN_SIDEBAR DM_ASSET_DIR "/320x240/chrome/window-sidebar.304x174x16.bmp"
#define DM_F_WIN_PLAIN DM_ASSET_DIR "/320x240/chrome/window-plain.304x174x16.bmp"
#define DM_F_ITUNES_WIN DM_ASSET_DIR "/320x240/chrome/itunes-window.304x174x16.bmp"
#define DM_F_ITUNES_SEL DM_ASSET_DIR "/320x240/chrome/itunes-selection.304x17x16.bmp"
#define DM_F_LIST_SEL DM_ASSET_DIR "/320x240/chrome/list-selection.217x19x16.bmp"
#define DM_F_SIDEBAR_SEL DM_ASSET_DIR "/320x240/chrome/sidebar-selection.86x19x16.bmp"
#define DM_F_MENU_PANEL DM_ASSET_DIR "/320x240/chrome/menu-panel.152x124x16.bmp"
#define DM_F_MENU_SEL DM_ASSET_DIR "/320x240/chrome/menu-selection.150x19x16.bmp"
#define DM_F_CONTEXT DM_ASSET_DIR "/320x240/chrome/context-panel.124x71x16.bmp"
#define DM_F_SHEET DM_ASSET_DIR "/320x240/chrome/sheet.240x112x16.bmp"
#define DM_F_TOOLTIP DM_ASSET_DIR "/320x240/chrome/tooltip.104x18x16.bmp"
#define DM_F_SCROLL_TRACK DM_ASSET_DIR "/320x240/chrome/scroller-track.16x97x16.bmp"
#define DM_F_SCROLL_THUMB DM_ASSET_DIR "/320x240/chrome/scroller-thumb.16x36x16.bmp"
#define DM_F_ICON_FINDER DM_ASSET_DIR "/320x240/icons/finder.32x32.rga"
#define DM_F_ICON_ITUNES DM_ASSET_DIR "/320x240/icons/itunes.32x32.rga"
#define DM_F_ICON_PREVIEW DM_ASSET_DIR "/320x240/icons/preview.32x32.rga"
#define DM_F_ICON_TEXTEDIT DM_ASSET_DIR "/320x240/icons/textedit.32x32.rga"
#define DM_F_ICON_CALCULATOR DM_ASSET_DIR "/320x240/icons/calculator.32x32.rga"
#define DM_F_ICON_DIRECTV DM_ASSET_DIR "/320x240/icons/directv.32x32.rga"
#define DM_F_ICON_SITEKICK ROCKBOX_DIR "/sitekick/desktop/icon.32x32.rga"
#define DM_F_ICON_STEAM ROCKBOX_DIR "/ipodjs/steam/desktop/icon.32x32.rga"
#define DM_F_ICON_NETFLIX \
    ROCKBOX_DIR "/ipodjs/netflix/desktop/icon.32x32.rga"
#define DM_F_ICON_SYSTEM_PREFERENCES DM_ASSET_DIR "/320x240/icons/system-preferences.32x32.rga"
#define DM_F_ICON_DASHBOARD DM_ASSET_DIR "/320x240/icons/dashboard.32x32.rga"
#define DM_F_ICON_DISK DM_ASSET_DIR "/320x240/icons/disk.32x32.rga"
#define DM_F_ICON_TRASH_EMPTY DM_ASSET_DIR "/320x240/icons/trash-empty.32x32.rga"
#define DM_F_ICON_FOLDER_DESKTOP DM_ASSET_DIR "/320x240/icons/folder-desktop.32x32.rga"
#define DM_F_ICON_FOLDER DM_ASSET_DIR "/320x240/icons/folder.16x16.rga"
#define DM_F_ICON_DOCUMENT DM_ASSET_DIR "/320x240/icons/document.16x16.rga"
#define DM_F_DOCK_FINDER_A DM_ASSET_DIR "/320x240/icons/finder-dock-34.34x34.rga"
#define DM_F_DOCK_FINDER_B DM_ASSET_DIR "/320x240/icons/finder-dock-38.38x38.rga"
#define DM_F_DOCK_ITUNES_A DM_ASSET_DIR "/320x240/icons/itunes-dock-34.34x34.rga"
#define DM_F_DOCK_ITUNES_B DM_ASSET_DIR "/320x240/icons/itunes-dock-38.38x38.rga"
#define DM_F_DOCK_PREVIEW_A DM_ASSET_DIR "/320x240/icons/preview-dock-34.34x34.rga"
#define DM_F_DOCK_PREVIEW_B DM_ASSET_DIR "/320x240/icons/preview-dock-38.38x38.rga"
#define DM_F_DOCK_TEXTEDIT_A DM_ASSET_DIR "/320x240/icons/textedit-dock-34.34x34.rga"
#define DM_F_DOCK_TEXTEDIT_B DM_ASSET_DIR "/320x240/icons/textedit-dock-38.38x38.rga"
#define DM_F_DOCK_CALCULATOR_A DM_ASSET_DIR "/320x240/icons/calculator-dock-34.34x34.rga"
#define DM_F_DOCK_CALCULATOR_B DM_ASSET_DIR "/320x240/icons/calculator-dock-38.38x38.rga"
#define DM_F_DOCK_DASHBOARD_A DM_ASSET_DIR "/320x240/icons/dashboard-dock-34.34x34.rga"
#define DM_F_DOCK_DASHBOARD_B DM_ASSET_DIR "/320x240/icons/dashboard-dock-38.38x38.rga"
#define DM_F_DOCK_DIRECTV_A DM_ASSET_DIR "/320x240/icons/directv-dock-34.34x34.rga"
#define DM_F_DOCK_DIRECTV_B DM_ASSET_DIR "/320x240/icons/directv-dock-38.38x38.rga"
#define DM_F_DOCK_SITEKICK_A ROCKBOX_DIR "/sitekick/desktop/icon-dock-34.34x34.rga"
#define DM_F_DOCK_SITEKICK_B ROCKBOX_DIR "/sitekick/desktop/icon-dock-38.38x38.rga"
#define DM_F_DOCK_STEAM_A \
    ROCKBOX_DIR "/ipodjs/steam/desktop/icon-dock-34.34x34.rga"
#define DM_F_DOCK_STEAM_B \
    ROCKBOX_DIR "/ipodjs/steam/desktop/icon-dock-38.38x38.rga"
#define DM_F_DOCK_NETFLIX_A \
    ROCKBOX_DIR "/ipodjs/netflix/desktop/icon-dock-34.34x34.rga"
#define DM_F_DOCK_NETFLIX_B \
    ROCKBOX_DIR "/ipodjs/netflix/desktop/icon-dock-38.38x38.rga"
#define DM_F_DOCK_SYSTEM_PREFERENCES_A DM_ASSET_DIR "/320x240/icons/system-preferences-dock-34.34x34.rga"
#define DM_F_DOCK_SYSTEM_PREFERENCES_B DM_ASSET_DIR "/320x240/icons/system-preferences-dock-38.38x38.rga"
#define DM_F_DOCK_TRASH_EMPTY_A DM_ASSET_DIR "/320x240/icons/trash-empty-dock-34.34x34.rga"
#define DM_F_DOCK_TRASH_EMPTY_B DM_ASSET_DIR "/320x240/icons/trash-empty-dock-38.38x38.rga"
#define DM_MENUBAR_H 21
#define DM_DOCK_SHELF_W 288
#define DM_DOCK_SHELF_H 26
#define DM_DOCK_ICON 32
#define DM_DOCK_STEP 34
#define DM_DOCK_MAG_A 34
#define DM_DOCK_MAG_B 38
#define DM_CHROME_W 304
#define DM_CHROME_H 174
#define DM_WIN_SIDEBAR_W 86
#define DM_ITUNES_CHROME_W 304
/* iTunes uses the same compact work-area window as Finder.  Its fullscreen
 * state expands inside Desktop Mode; normal mode keeps the Dock visible. */
#define DM_ITUNES_CHROME_H DM_ITUNES_ASSET_H
#define DM_ITUNES_ASSET_H 174
#define DM_ITUNES_SOURCE_W 86
#define DM_LIST_SEL_W 217
#define DM_ROW_H 19
#define DM_MENU_W 152
#define DM_MENU_H 124
#define DM_CONTEXT_W 124
#define DM_CONTEXT_H 71
#define DM_SHEET_W 240
#define DM_SHEET_H 112
#define DM_TOOLTIP_W 104
#define DM_TOOLTIP_H 18
#define DM_SCROLLER_H_ASSET 97
#define DM_SCROLLER_THUMB_H 36
#define DM_ICON_SMALL 16
#endif

#if LCD_WIDTH < 1920
/* The TV path carries the native source through an overscan-prone composite
 * viewport. Keep user-facing edges inside an 8px source-space margin; the
 * host profile remains free to use its wider desktop canvas. */
#define DM_SOURCE_SAFE_MARGIN 8
#else
#define DM_SOURCE_SAFE_MARGIN 0
#endif

#define DM_APPLE_X 4
#define DM_MENU_X 3
#define DM_MENU_Y DM_MENUBAR_H
#define DM_MENU_ROW_H 19
#define DM_MENU_TOP 7
#define DM_APPLE_MENU_ROWS 6
#define DM_CONTEXT_ROWS 3
#define DM_CONTEXT_X 96
#define DM_CONTEXT_Y 84
#define DM_SHEET_X ((LCD_WIDTH - DM_SHEET_W) / 2)
#define DM_SHEET_Y 56
#define DM_TOOLTIP_Y (DM_DOCK_ICON_Y - DM_TOOLTIP_H - 4)
#define DM_TOOLTIP_CAP 4
#define DM_PREF_ROW_H 16
#define DM_PREF_ROWS (DM_BODY_H / DM_PREF_ROW_H)


/* Placements derived from the panel: the window is centred in the work area,
 * the Dock sits on the bottom edge with its real shelf, and the desktop icons
 * run down the right margin the way Finder arranges them.
 */
#define DM_DOCK_SHELF_X ((LCD_WIDTH - DM_DOCK_SHELF_W) / 2)
#define DM_DOCK_SHELF_Y (LCD_HEIGHT - DM_DOCK_SHELF_H)
#define DM_DOCK_ICON_Y (DM_DOCK_SHELF_Y - DM_DOCK_ICON / 2)
/* The Dock band is the union of the real shelf and its icons.  Using the
 * shelf's half-height here left a three-pixel gap on the native panel and
 * allowed moved windows to overlap the Dock. */
#define DM_DOCK_BAND (LCD_HEIGHT - DM_DOCK_ICON_Y)
#define DM_DOCK_REFLECTION_MAX_ALPHA 112
#define DM_DOCK_LEFT \
    ((LCD_WIDTH - DM_DOCK_SLOTS * DM_DOCK_STEP) / 2 + \
     (DM_DOCK_STEP - DM_DOCK_ICON) / 2)
#define DM_SCROLLER_H DM_BODY_H
#define DM_DESKTOP_ICON 20
#define DM_DESKTOP_LABEL_W 50
#define DM_DESKTOP_ITEM_W 50
#define DM_DESKTOP_ITEM_H 38
#define DM_DESKTOP_ICON_X \
    (LCD_WIDTH - DM_DESKTOP_ICON - DM_SOURCE_SAFE_MARGIN - 10)
#define DM_DESKTOP_LABEL_X \
    (DM_DESKTOP_ICON_X - (DM_DESKTOP_LABEL_W - DM_DESKTOP_ICON) / 2)
#define DM_DESKTOP_ITEM_X \
    (DM_DESKTOP_ICON_X - (DM_DESKTOP_ITEM_W - DM_DESKTOP_ICON) / 2)
#define DM_DESKTOP_DISK_Y (DM_MENUBAR_H + 8)
#define DM_DESKTOP_DOCS_Y (DM_DESKTOP_DISK_Y + DM_DESKTOP_ICON + 20)
#define DM_DESKTOP_MUSIC_Y (DM_DESKTOP_DOCS_Y + DM_DESKTOP_ICON + 20)
#define DM_DESKTOP_LAUNCHPAD_X 12
#define DM_DESKTOP_LAUNCHPAD_Y (DM_MENUBAR_H + 8)
#define DM_DESKTOP_LAUNCHPAD_W 50
#define DM_DESKTOP_LAUNCHPAD_H 38
#define DM_DESKTOP_LAUNCHPAD_ICON 20

#if LCD_WIDTH >= 1920
#define DM_NORMAL_WIN_BASE_X ((LCD_WIDTH - DM_CHROME_W) / 2)
#define DM_NORMAL_WIN_BASE_Y (DM_MENUBAR_H + \
    (LCD_HEIGHT - DM_MENUBAR_H - DM_DOCK_BAND - DM_CHROME_H) / 2)
#define DM_ITUNES_NORMAL_BASE_X ((LCD_WIDTH - DM_ITUNES_CHROME_W) / 2)
#define DM_ITUNES_NORMAL_BASE_Y (DM_MENUBAR_H + \
    (LCD_HEIGHT - DM_MENUBAR_H - DM_DOCK_BAND - DM_ITUNES_CHROME_H) / 2)
#else
/* These are the measured 320x240 source-space bounds.  In particular, the
 * window ends at y=198, exactly where the Dock band begins; the video-out
 * compositor scales this complete source frame later. */
#define DM_NORMAL_WIN_BASE_X 8
#define DM_NORMAL_WIN_BASE_Y 24
#define DM_ITUNES_NORMAL_BASE_X 8
#define DM_ITUNES_NORMAL_BASE_Y 24
#endif
#define DM_NORMAL_WIN_X (DM_NORMAL_WIN_BASE_X + dm_window_dx)
#define DM_NORMAL_WIN_Y (DM_NORMAL_WIN_BASE_Y + dm_window_dy)
#define DM_NORMAL_BODY_BASE_Y \
    (DM_NORMAL_WIN_BASE_Y + DM_WIN_TITLE_H + DM_WIN_TOOLBAR_H)
#define DM_WIN_X (dm_fullscreen ? 0 : DM_NORMAL_WIN_X)
#define DM_WIN_Y (dm_fullscreen ? DM_MENUBAR_H : DM_NORMAL_WIN_Y)
#define DM_WIN_W (dm_fullscreen ? LCD_WIDTH : DM_CHROME_W)
#define DM_WIN_H (dm_fullscreen ? LCD_HEIGHT - DM_MENUBAR_H : DM_CHROME_H)
#define DM_ITUNES_W (dm_fullscreen ? LCD_WIDTH : DM_ITUNES_CHROME_W)
#define DM_ITUNES_H (dm_fullscreen ? LCD_HEIGHT - DM_MENUBAR_H : \
                     DM_ITUNES_CHROME_H)
#define DM_ITUNES_X (dm_fullscreen ? 0 : \
                     DM_ITUNES_NORMAL_BASE_X + dm_window_dx)
#define DM_ITUNES_Y (dm_fullscreen ? DM_MENUBAR_H : \
                     DM_ITUNES_NORMAL_BASE_Y + dm_window_dy)

/* Title-bar traffic lights, in the real 1:1 positions Apple drew them. */
#define DM_TRAFFIC_RECT \
    ((struct dm_rect){ DM_WIN_X + 8, DM_WIN_Y + 5, 62, 15 })
#define DM_CLOSE_RECT \
    ((struct dm_rect){ DM_WIN_X + 8, DM_WIN_Y + 5, 16, 15 })
#define DM_MINIMISE_RECT \
    ((struct dm_rect){ DM_WIN_X + 29, DM_WIN_Y + 5, 16, 15 })
#define DM_ZOOM_RECT \
    ((struct dm_rect){ DM_WIN_X + 50, DM_WIN_Y + 5, 16, 15 })
#define DM_BACK_RECT \
    ((struct dm_rect){ DM_WIN_X + 8, DM_WIN_Y + 26, 27, 21 })
#define DM_FORWARD_RECT \
    ((struct dm_rect){ DM_WIN_X + 35, DM_WIN_Y + 26, 27, 21 })

/* Real Snow Leopard label colours, sampled from the same captures. */
#define DM_INK LCD_RGBPACK(0, 0, 0)
#define DM_INK_WHITE LCD_RGBPACK(255, 255, 255)
#define DM_INK_TITLE LCD_RGBPACK(60, 60, 60)
#define DM_INK_STATUS LCD_RGBPACK(70, 70, 70)
#define DM_INK_SIDEBAR LCD_RGBPACK(40, 44, 50)
#define DM_INK_SIDEBAR_HEAD LCD_RGBPACK(110, 120, 133)

#define DM_DOUBLE_CLICK_TICKS (HZ * 35 / 100)
/* pointer movement carries sixteenths of a pixel between samples */
#define DM_WHEEL_SUBPIXEL 16
#define DM_WHEEL_PACKET_CAP 6
#define DM_ANIMATION_FRAMES 6
#define DM_ANIMATION_TICKS (HZ * 3 / 10)
#define DM_BOOT_FRAME_COUNT 12
#define DM_BOOT_FRAME_TICKS MAX(1, HZ / 12)
#define DM_BOOT_SPINNER_SIZE 24
#define DM_BOOT_SPINNER_X ((LCD_WIDTH - DM_BOOT_SPINNER_SIZE) / 2)
#define DM_BOOT_SPINNER_Y 151
#define DM_INPUT_POLL_TICKS MAX(1, HZ / 60)
#define DM_POINTER_UPDATE_TICKS MAX(1, HZ / 50)
#define DM_LAUNCHPAD_COLUMNS 4
#define DM_VISIBLE_WINDOWS 3
#define DM_APP_MENU_COUNT 4
/* Match the bounded microUI profile without coupling this playback-aware,
 * asset-backed shell to the immediate-mode core. */
#define DM_CONTROL_LIMIT 64
#define DM_ABS(value) ((value) < 0 ? -(value) : (value))
#define DM_APP_BIT(app) (1u << (unsigned int)(app))

#if LCD_DEPTH < 16 || \
    !((LCD_WIDTH == 320 && LCD_HEIGHT == 240) || \
      (LCD_WIDTH == 1920 && LCD_HEIGHT == 1080) || \
      (LCD_WIDTH == 640 && LCD_HEIGHT == 480))
#define DM_UNSUPPORTED_TARGET
#endif

/* Opaque chrome arrives as RGB565 BMP; anything with a soft edge arrives as
 * RGB565 plus Apple's own 8-bit coverage, because Desktop Mode composites it
 * over real chrome rather than keying it out.
 */
enum dm_asset_id
{
    DM_ASSET_AURORA = 0,
    DM_ASSET_MENUBAR,
    DM_ASSET_APPLE_HIGHLIGHT,
    DM_ASSET_DOCK_SHELF,
    DM_ASSET_WINDOW_SIDEBAR,
    DM_ASSET_WINDOW_PLAIN,
    DM_ASSET_ITUNES_WINDOW,
    DM_ASSET_ITUNES_SELECTION,
    DM_ASSET_LIST_SELECTION,
    DM_ASSET_SIDEBAR_SELECTION,
    DM_ASSET_MENU_PANEL,
    DM_ASSET_MENU_SELECTION,
    DM_ASSET_CONTEXT_PANEL,
    DM_ASSET_SHEET,
    DM_ASSET_TOOLTIP,
    DM_ASSET_SCROLLER_TRACK,
    DM_ASSET_SCROLLER_THUMB,
    DM_ASSET_BMP_COUNT,

    DM_ASSET_DASH_CLOCK = DM_ASSET_BMP_COUNT,
    DM_ASSET_DASH_ICAL,
    DM_ASSET_DASH_WEATHER,
    DM_ASSET_DASH_STICKIES,
    DM_ASSET_DASH_ITUNES,

    DM_ASSET_DOCK_INDICATOR,
    DM_ASSET_CURSOR_ARROW,
    DM_ASSET_CURSOR_HAND,
    DM_ASSET_ICON_FINDER,
    DM_ASSET_ICON_ITUNES,
    DM_ASSET_ICON_PREVIEW,
    DM_ASSET_ICON_TEXTEDIT,
    DM_ASSET_ICON_CALCULATOR,
    DM_ASSET_ICON_DIRECTV,
    DM_ASSET_ICON_SITEKICK,
    DM_ASSET_ICON_STEAM,
    DM_ASSET_ICON_NETFLIX,
    DM_ASSET_ICON_PREFERENCES,
    DM_ASSET_ICON_DASHBOARD,
    DM_ASSET_ICON_DISK,
    DM_ASSET_ICON_TRASH_EMPTY,
    DM_ASSET_ICON_FOLDER_DESKTOP,
    DM_ASSET_ICON_FOLDER,
    DM_ASSET_ICON_DOCUMENT,
    DM_ASSET_DOCK_FINDER_34,
    DM_ASSET_DOCK_FINDER_38,
    DM_ASSET_DOCK_ITUNES_34,
    DM_ASSET_DOCK_ITUNES_38,
    DM_ASSET_DOCK_PREVIEW_34,
    DM_ASSET_DOCK_PREVIEW_38,
    DM_ASSET_DOCK_TEXTEDIT_34,
    DM_ASSET_DOCK_TEXTEDIT_38,
    DM_ASSET_DOCK_CALCULATOR_34,
    DM_ASSET_DOCK_CALCULATOR_38,
    DM_ASSET_DOCK_DASHBOARD_34,
    DM_ASSET_DOCK_DASHBOARD_38,
    DM_ASSET_DOCK_DIRECTV_34,
    DM_ASSET_DOCK_DIRECTV_38,
    DM_ASSET_DOCK_SITEKICK_34,
    DM_ASSET_DOCK_SITEKICK_38,
    DM_ASSET_DOCK_STEAM_34,
    DM_ASSET_DOCK_STEAM_38,
    DM_ASSET_DOCK_NETFLIX_34,
    DM_ASSET_DOCK_NETFLIX_38,
    DM_ASSET_DOCK_PREFERENCES_34,
    DM_ASSET_DOCK_PREFERENCES_38,
    DM_ASSET_DOCK_TRASH_34,
    DM_ASSET_DOCK_TRASH_38,
    DM_ASSET_COUNT
};

enum dm_font_id
{
    DM_FONT_REGULAR = 0,
    DM_FONT_BOLD,
    DM_FONT_SMALL,
    DM_FONT_COUNT
};

enum dm_app
{
    DM_APP_DESKTOP = 0,
    DM_APP_FINDER,
    DM_APP_ITUNES,
    DM_APP_PHOTOS,
    DM_APP_PREVIEW,
    DM_APP_TEXTEDIT,
    DM_APP_CALCULATOR,
    DM_APP_DIRECTV,
    DM_APP_SITEKICK,
    DM_APP_STEAM,
    DM_APP_NETFLIX,
    DM_APP_PREFERENCES,
    DM_APP_DASHBOARD,
    DM_APP_LAUNCHPAD,
    DM_APP_TRASH,
    DM_APP_COUNT,
};

enum dm_overlay
{
    DM_OVERLAY_NONE = 0,
    DM_OVERLAY_APPLE_MENU,
    DM_OVERLAY_APP_MENU,
    DM_OVERLAY_FINDER_CONTEXT,
    DM_OVERLAY_GET_INFO,
    DM_OVERLAY_RETURN_CONFIRM,
    DM_OVERLAY_RESTART_CONFIRM,
    DM_OVERLAY_DELETE_CONFIRM,
    DM_OVERLAY_NEW_FOLDER_CONFIRM,
    DM_OVERLAY_DIAGNOSTICS,
    DM_OVERLAY_WALLPAPER_RECENT,
    DM_OVERLAY_PHOTO_PIN,
    DM_OVERLAY_NOTICE,
};

enum dm_boot_result
{
    DM_BOOT_ERROR = -1,
    DM_BOOT_DONE = 0,
    DM_BOOT_USB_CONNECTED = 1,
};

enum dm_error
{
    DM_ERR_NONE = 0,
    DM_ERR_ASSET_MISSING,
    DM_ERR_ASSET_INVALID,
    DM_ERR_PLUGIN_BUFFER,
    DM_ERR_ARENA_OVERFLOW,
    DM_ERR_CONTROL_OVERFLOW,
};

enum dm_control_action
{
    DM_ACTION_NONE = 0,
    DM_ACTION_APPLE_MENU,
    DM_ACTION_MENU_BAR,
    DM_ACTION_DOCK_APP,
    DM_ACTION_WINDOW_DRAG,
    DM_ACTION_FOCUS_WINDOW,
    DM_ACTION_CLOSE,
    DM_ACTION_MINIMISE,
    DM_ACTION_FULLSCREEN,
    DM_ACTION_FINDER_BACK,
    DM_ACTION_FINDER_FORWARD,
    DM_ACTION_FINDER_SCROLL,
    DM_ACTION_FILE_ROW,
    DM_ACTION_FINDER_SIDEBAR,
    DM_ACTION_ITUNES_BACK,
    DM_ACTION_ITUNES_PREVIOUS,
    DM_ACTION_ITUNES_PLAY_PAUSE,
    DM_ACTION_ITUNES_NEXT,
    DM_ACTION_ITUNES_WPS,
    DM_ACTION_ITUNES_PAGE,
    DM_ACTION_ITUNES_SCROLL,
    DM_ACTION_ITUNES_SOURCE,
    DM_ACTION_ITUNES_SEEK,
    DM_ACTION_ITUNES_VOLUME,
    DM_ACTION_ITUNES_SHUFFLE,
    DM_ACTION_ITUNES_REPEAT,
    DM_ACTION_ITUNES_SEARCH,
    DM_ACTION_DASHBOARD_PREVIOUS,
    DM_ACTION_DASHBOARD_PLAY_PAUSE,
    DM_ACTION_DASHBOARD_NEXT,
    DM_ACTION_DASHBOARD_ITUNES,
    DM_ACTION_PREFERENCE,
    DM_ACTION_PREFERENCE_TAB,
    DM_ACTION_WALLPAPER_RECENT,
    DM_ACTION_PHOTO_PIN,
    DM_ACTION_DESKTOP_FOLDER,
    DM_ACTION_DESKTOP_LAUNCHPAD,
    DM_ACTION_LAUNCHPAD_APP,
    DM_ACTION_TEXTEDIT_EDIT,
    DM_ACTION_CALCULATOR_KEY,
    DM_ACTION_APPLE_MENU_ROW,
    DM_ACTION_APP_MENU_ROW,
    DM_ACTION_CONTEXT_MENU_ROW,
    DM_ACTION_MODAL_CONFIRM,
    DM_ACTION_MODAL_DISMISS,
};

enum dm_control_flags
{
    DM_CONTROL_HAND = 0x01,
    DM_CONTROL_FOCUSABLE = 0x02,
    DM_CONTROL_DOUBLE_CLICK = 0x04,
};

struct dm_asset_def
{
    const char *path;
    short width;
    short height;
};

/* Opaque assets keep a plain RGB565 plane; coverage assets keep Apple's
 * 8-bit alpha beside it so the compositor can blend real soft edges.
 */
struct dm_asset
{
    fb_data *pixels;
    unsigned char *coverage;
    short width;
    short height;
    bool loaded;
};

struct dm_font_def
{
    const char *coverage;
    const char *metrics;
};

struct dm_font
{
    unsigned char *coverage;
    unsigned char advance[DM_ASCII_COUNT];
    unsigned char cell_w;
    unsigned char cell_h;
    unsigned char ascent;
    signed char origin;
    bool loaded;
};

struct dm_file
{
    char name[MAX_PATH];
    bool is_dir;
    off_t size;
    time_t mtime;
};

struct dm_rect
{
    int x;
    int y;
    int width;
    int height;
};

struct dm_control
{
    uint32_t id;
    struct dm_rect bounds;
    enum dm_control_action action;
    short value;
    unsigned short flags;
};

struct dm_control_registry
{
    struct dm_control controls[DM_CONTROL_LIMIT];
    int count;
    int high_water;
    int hover_index;
    uint32_t focus_id;
};

struct dm_settings
{
    int pointer_speed;
    bool reverse_wheel;
    bool show_desktop_folders;
    bool drag_lock;
    bool restore_session;
    enum dm_app start_app;
};

struct dm_animation
{
    bool active;
    bool minimizing;
    enum dm_app app;
    long started;
    int last_frame;
    struct dm_rect current;
    struct dm_rect from;
    struct dm_rect to;
};

struct dm_state
{
    enum dm_app app;
    enum dm_overlay overlay;
    bool fullscreen;
    int cursor_x;
    int cursor_y;
    int hover_dock;
    int hover_row;
    int hover_launchpad;
    int menu_row;
    int app_menu;
    int desktop_selected;
    int preference_row;
    int last_wheel;
    int wheel_velocity;
    int wheel_direction;
    long last_wheel_tick;
    long last_status_tick;
    enum dm_app minimized_app;
    unsigned int running_apps;
    enum dm_app window_stack[DM_VISIBLE_WINDOWS];
    int window_stack_count;
    signed char window_dx[DM_APP_COUNT];
    signed char window_dy[DM_APP_COUNT];
    bool window_dragging;
    int window_drag_x;
    int window_drag_y;
    long window_drag_tick;
    int dock_stage;
    long dock_hover_tick;
    struct dm_animation animation;
    bool mouse_down;
    bool dragged;
    long mouse_down_tick;
    long last_click_tick;
    int last_click_x;
    int last_click_y;
    bool running;
    bool redraw;
    bool damage_full;
    int presented_cursor_x;
    int presented_cursor_y;
    struct dm_rect presented_hover;
    enum dm_control_action presented_hover_action;
    struct dm_rect pending_damage;
    struct dm_rect requested_damage;
    uint32_t scene_signature;
    unsigned long frame_count;
    unsigned long full_update_count;
    unsigned long partial_update_count;
    long last_frame_ticks;
    long worst_frame_ticks;
    struct dm_control_registry *controls;
    /* Absolute-wheel targets steer the pointer directly.  Anything that never
     * reports a wheel contact - the simulator, or a future target without
     * HAVE_WHEEL_POSITION - falls back to four-direction pointer motion.
     */
    bool wheel_available;
    long hint_until;
    /* Rockpod parity session: the host's own mouse drives the pointer. */
    bool host_pointer;
    bool host_secondary;
    bool host_context;
    int host_click;
    long host_poll_tick;
    /* Held-touch pointer glide, in sixteenths of a pixel. */
    long glide_since;
    long glide_tick;
    long glide_emit;
    int glide_x;
    int glide_y;
    int frac_x;
    int frac_y;
    /* Continuous motion for a currently-held direction button (four-way
     * fallback, or the one-pixel nudge alongside an available wheel).  The
     * button driver's own press/repeat/release cadence is too coarse to
     * glide from directly - repeats arrive in bursts, with a long initial
     * gap - so a hold is timed here instead and advanced every loop tick,
     * the same way a wheel drag glides in dm_glide_wheel().
     */
    int held_dir_x;
    int held_dir_y;
    long held_seen_tick;
    long held_emit_tick;
};

static const struct dm_asset_def dm_asset_defs[DM_ASSET_COUNT] =
{
    [DM_ASSET_AURORA] = { DM_F_AURORA, LCD_WIDTH, LCD_HEIGHT },
    [DM_ASSET_MENUBAR] = { DM_F_MENUBAR, LCD_WIDTH, DM_MENUBAR_H },
    [DM_ASSET_APPLE_HIGHLIGHT] = { DM_F_APPLE_HL, 22, DM_MENUBAR_H },
    [DM_ASSET_DOCK_SHELF] =
        { DM_F_DOCK_SHELF, DM_DOCK_SHELF_W, DM_DOCK_SHELF_H },
    [DM_ASSET_WINDOW_SIDEBAR] =
        { DM_F_WIN_SIDEBAR, DM_CHROME_W, DM_CHROME_H },
    [DM_ASSET_WINDOW_PLAIN] =
        { DM_F_WIN_PLAIN, DM_CHROME_W, DM_CHROME_H },
    [DM_ASSET_ITUNES_WINDOW] =
        { DM_F_ITUNES_WIN, DM_ITUNES_CHROME_W, DM_ITUNES_ASSET_H },
    [DM_ASSET_ITUNES_SELECTION] =
        { DM_F_ITUNES_SEL, DM_ITUNES_CHROME_W, 17 },
    [DM_ASSET_LIST_SELECTION] = { DM_F_LIST_SEL, DM_LIST_SEL_W, 19 },
    [DM_ASSET_SIDEBAR_SELECTION] =
        { DM_F_SIDEBAR_SEL, DM_WIN_SIDEBAR_W, 19 },
    [DM_ASSET_MENU_PANEL] = { DM_F_MENU_PANEL, DM_MENU_W, DM_MENU_H },
    [DM_ASSET_MENU_SELECTION] = { DM_F_MENU_SEL, DM_MENU_W - 2, 19 },
    [DM_ASSET_CONTEXT_PANEL] = { DM_F_CONTEXT, DM_CONTEXT_W, DM_CONTEXT_H },
    [DM_ASSET_SHEET] = { DM_F_SHEET, DM_SHEET_W, DM_SHEET_H },
    [DM_ASSET_TOOLTIP] = { DM_F_TOOLTIP, DM_TOOLTIP_W, DM_TOOLTIP_H },
    [DM_ASSET_SCROLLER_TRACK] =
        { DM_F_SCROLL_TRACK, 16, DM_SCROLLER_H_ASSET },
    [DM_ASSET_SCROLLER_THUMB] =
        { DM_F_SCROLL_THUMB, 16, DM_SCROLLER_THUMB_H },
    [DM_ASSET_DASH_CLOCK] = { DM_F_DASH_CLOCK, 74, 74 },
    [DM_ASSET_DASH_ICAL] = { DM_F_DASH_ICAL, 104, 51 },
    [DM_ASSET_DASH_WEATHER] = { DM_F_DASH_WEATHER, 104, 59 },
    [DM_ASSET_DASH_STICKIES] = { DM_F_DASH_STICKIES, 96, 88 },
    [DM_ASSET_DASH_ITUNES] = { DM_F_DASH_ITUNES, 196, 82 },
    [DM_ASSET_DOCK_INDICATOR] =
        { DM_ASSET_DIR "/desktop/dock-indicator.14x8.rga", 14, 8 },
    [DM_ASSET_CURSOR_ARROW] =
        { DM_ASSET_DIR "/cursor/arrow.14x20.rga", 14, 20 },
    [DM_ASSET_CURSOR_HAND] =
        { DM_ASSET_DIR "/cursor/pointing-hand.16x18.rga", 16, 18 },
    [DM_ASSET_ICON_FINDER] = { DM_F_ICON_FINDER, DM_DOCK_ICON, DM_DOCK_ICON },
    [DM_ASSET_ICON_ITUNES] = { DM_F_ICON_ITUNES, DM_DOCK_ICON, DM_DOCK_ICON },
    [DM_ASSET_ICON_PREVIEW] =
        { DM_F_ICON_PREVIEW, DM_DOCK_ICON, DM_DOCK_ICON },
    [DM_ASSET_ICON_TEXTEDIT] =
        { DM_F_ICON_TEXTEDIT, DM_DOCK_ICON, DM_DOCK_ICON },
    [DM_ASSET_ICON_CALCULATOR] =
        { DM_F_ICON_CALCULATOR, DM_DOCK_ICON, DM_DOCK_ICON },
    [DM_ASSET_ICON_DIRECTV] =
        { DM_F_ICON_DIRECTV, DM_DOCK_ICON, DM_DOCK_ICON },
    [DM_ASSET_ICON_SITEKICK] =
        { DM_F_ICON_SITEKICK, DM_DOCK_ICON, DM_DOCK_ICON },
    [DM_ASSET_ICON_STEAM] =
        { DM_F_ICON_STEAM, DM_DOCK_ICON, DM_DOCK_ICON },
    [DM_ASSET_ICON_NETFLIX] =
        { DM_F_ICON_NETFLIX, DM_DOCK_ICON, DM_DOCK_ICON },
    [DM_ASSET_ICON_PREFERENCES] =
        { DM_F_ICON_SYSTEM_PREFERENCES, DM_DOCK_ICON, DM_DOCK_ICON },
    [DM_ASSET_ICON_DASHBOARD] =
        { DM_F_ICON_DASHBOARD, DM_DOCK_ICON, DM_DOCK_ICON },
    [DM_ASSET_ICON_DISK] = { DM_F_ICON_DISK, DM_DOCK_ICON, DM_DOCK_ICON },
    [DM_ASSET_ICON_TRASH_EMPTY] =
        { DM_F_ICON_TRASH_EMPTY, DM_DOCK_ICON, DM_DOCK_ICON },
    [DM_ASSET_ICON_FOLDER_DESKTOP] =
        { DM_F_ICON_FOLDER_DESKTOP, DM_DOCK_ICON, DM_DOCK_ICON },
    [DM_ASSET_ICON_FOLDER] =
        { DM_F_ICON_FOLDER, DM_ICON_SMALL, DM_ICON_SMALL },
    [DM_ASSET_ICON_DOCUMENT] =
        { DM_F_ICON_DOCUMENT, DM_ICON_SMALL, DM_ICON_SMALL },
    [DM_ASSET_DOCK_FINDER_34] =
        { DM_F_DOCK_FINDER_A, DM_DOCK_MAG_A, DM_DOCK_MAG_A },
    [DM_ASSET_DOCK_FINDER_38] =
        { DM_F_DOCK_FINDER_B, DM_DOCK_MAG_B, DM_DOCK_MAG_B },
    [DM_ASSET_DOCK_ITUNES_34] =
        { DM_F_DOCK_ITUNES_A, DM_DOCK_MAG_A, DM_DOCK_MAG_A },
    [DM_ASSET_DOCK_ITUNES_38] =
        { DM_F_DOCK_ITUNES_B, DM_DOCK_MAG_B, DM_DOCK_MAG_B },
    [DM_ASSET_DOCK_PREVIEW_34] =
        { DM_F_DOCK_PREVIEW_A, DM_DOCK_MAG_A, DM_DOCK_MAG_A },
    [DM_ASSET_DOCK_PREVIEW_38] =
        { DM_F_DOCK_PREVIEW_B, DM_DOCK_MAG_B, DM_DOCK_MAG_B },
    [DM_ASSET_DOCK_TEXTEDIT_34] =
        { DM_F_DOCK_TEXTEDIT_A, DM_DOCK_MAG_A, DM_DOCK_MAG_A },
    [DM_ASSET_DOCK_TEXTEDIT_38] =
        { DM_F_DOCK_TEXTEDIT_B, DM_DOCK_MAG_B, DM_DOCK_MAG_B },
    [DM_ASSET_DOCK_CALCULATOR_34] =
        { DM_F_DOCK_CALCULATOR_A, DM_DOCK_MAG_A, DM_DOCK_MAG_A },
    [DM_ASSET_DOCK_CALCULATOR_38] =
        { DM_F_DOCK_CALCULATOR_B, DM_DOCK_MAG_B, DM_DOCK_MAG_B },
    [DM_ASSET_DOCK_DASHBOARD_34] =
        { DM_F_DOCK_DASHBOARD_A, DM_DOCK_MAG_A, DM_DOCK_MAG_A },
    [DM_ASSET_DOCK_DASHBOARD_38] =
        { DM_F_DOCK_DASHBOARD_B, DM_DOCK_MAG_B, DM_DOCK_MAG_B },
    [DM_ASSET_DOCK_DIRECTV_34] =
        { DM_F_DOCK_DIRECTV_A, DM_DOCK_MAG_A, DM_DOCK_MAG_A },
    [DM_ASSET_DOCK_DIRECTV_38] =
        { DM_F_DOCK_DIRECTV_B, DM_DOCK_MAG_B, DM_DOCK_MAG_B },
    [DM_ASSET_DOCK_SITEKICK_34] =
        { DM_F_DOCK_SITEKICK_A, DM_DOCK_MAG_A, DM_DOCK_MAG_A },
    [DM_ASSET_DOCK_SITEKICK_38] =
        { DM_F_DOCK_SITEKICK_B, DM_DOCK_MAG_B, DM_DOCK_MAG_B },
    [DM_ASSET_DOCK_STEAM_34] =
        { DM_F_DOCK_STEAM_A, DM_DOCK_MAG_A, DM_DOCK_MAG_A },
    [DM_ASSET_DOCK_STEAM_38] =
        { DM_F_DOCK_STEAM_B, DM_DOCK_MAG_B, DM_DOCK_MAG_B },
    [DM_ASSET_DOCK_NETFLIX_34] =
        { DM_F_DOCK_NETFLIX_A, DM_DOCK_MAG_A, DM_DOCK_MAG_A },
    [DM_ASSET_DOCK_NETFLIX_38] =
        { DM_F_DOCK_NETFLIX_B, DM_DOCK_MAG_B, DM_DOCK_MAG_B },
    [DM_ASSET_DOCK_PREFERENCES_34] =
        { DM_F_DOCK_SYSTEM_PREFERENCES_A, DM_DOCK_MAG_A, DM_DOCK_MAG_A },
    [DM_ASSET_DOCK_PREFERENCES_38] =
        { DM_F_DOCK_SYSTEM_PREFERENCES_B, DM_DOCK_MAG_B, DM_DOCK_MAG_B },
    [DM_ASSET_DOCK_TRASH_34] =
        { DM_F_DOCK_TRASH_EMPTY_A, DM_DOCK_MAG_A, DM_DOCK_MAG_A },
    [DM_ASSET_DOCK_TRASH_38] =
        { DM_F_DOCK_TRASH_EMPTY_B, DM_DOCK_MAG_B, DM_DOCK_MAG_B },
};

static const struct dm_font_def dm_font_defs[DM_FONT_COUNT] =
{
    [DM_FONT_REGULAR] =
        { DM_ASSET_DIR "/fonts/lucida-grande-11.alpha",
          DM_ASSET_DIR "/fonts/lucida-grande-11.metrics" },
    [DM_FONT_BOLD] =
        { DM_ASSET_DIR "/fonts/lucida-grande-bold-11.alpha",
          DM_ASSET_DIR "/fonts/lucida-grande-bold-11.metrics" },
    [DM_FONT_SMALL] =
        { DM_ASSET_DIR "/fonts/lucida-grande-9.alpha",
          DM_ASSET_DIR "/fonts/lucida-grande-9.metrics" },
};

static const char dm_boot_background_path[] = DM_F_BOOT_BACKGROUND;

static const char * const dm_boot_spinner_paths[DM_BOOT_FRAME_COUNT] =
{
    DM_ASSET_DIR "/boot/spinner-00.24x24x16.bmp",
    DM_ASSET_DIR "/boot/spinner-01.24x24x16.bmp",
    DM_ASSET_DIR "/boot/spinner-02.24x24x16.bmp",
    DM_ASSET_DIR "/boot/spinner-03.24x24x16.bmp",
    DM_ASSET_DIR "/boot/spinner-04.24x24x16.bmp",
    DM_ASSET_DIR "/boot/spinner-05.24x24x16.bmp",
    DM_ASSET_DIR "/boot/spinner-06.24x24x16.bmp",
    DM_ASSET_DIR "/boot/spinner-07.24x24x16.bmp",
    DM_ASSET_DIR "/boot/spinner-08.24x24x16.bmp",
    DM_ASSET_DIR "/boot/spinner-09.24x24x16.bmp",
    DM_ASSET_DIR "/boot/spinner-10.24x24x16.bmp",
    DM_ASSET_DIR "/boot/spinner-11.24x24x16.bmp",
};

/* OS icons come from the owned Snow Leopard source. DIRECTV comes from the
 * user's Live TV artwork. Sitekick's icon ships with its GPL asset pack so
 * the desktop shell and the ordinary iPod plugin use the same robot art. */
static const enum dm_asset_id dm_dock_assets[DM_DOCK_SLOTS] =
{
    DM_ASSET_ICON_FINDER,
    DM_ASSET_ICON_ITUNES,
    DM_ASSET_ICON_PREVIEW,
    DM_ASSET_ICON_TEXTEDIT,
    DM_ASSET_ICON_CALCULATOR,
    DM_ASSET_ICON_DASHBOARD,
    DM_ASSET_ICON_PREFERENCES,
    DM_ASSET_ICON_TRASH_EMPTY,
};

static const enum dm_asset_id dm_dock_assets_34[DM_DOCK_SLOTS] =
{
    DM_ASSET_DOCK_FINDER_34,
    DM_ASSET_DOCK_ITUNES_34,
    DM_ASSET_DOCK_PREVIEW_34,
    DM_ASSET_DOCK_TEXTEDIT_34,
    DM_ASSET_DOCK_CALCULATOR_34,
    DM_ASSET_DOCK_DASHBOARD_34,
    DM_ASSET_DOCK_PREFERENCES_34,
    DM_ASSET_DOCK_TRASH_34,
};

static const enum dm_asset_id dm_dock_assets_38[DM_DOCK_SLOTS] =
{
    DM_ASSET_DOCK_FINDER_38,
    DM_ASSET_DOCK_ITUNES_38,
    DM_ASSET_DOCK_PREVIEW_38,
    DM_ASSET_DOCK_TEXTEDIT_38,
    DM_ASSET_DOCK_CALCULATOR_38,
    DM_ASSET_DOCK_DASHBOARD_38,
    DM_ASSET_DOCK_PREFERENCES_38,
    DM_ASSET_DOCK_TRASH_38,
};

static const enum dm_app dm_dock_apps[DM_DOCK_SLOTS] =
{
    DM_APP_FINDER,
    DM_APP_ITUNES,
    DM_APP_PREVIEW,
    DM_APP_TEXTEDIT,
    DM_APP_CALCULATOR,
    DM_APP_DASHBOARD,
    DM_APP_PREFERENCES,
    DM_APP_TRASH,
};

static const char * const dm_dock_labels[DM_DOCK_SLOTS] =
{
    "Finder", "iTunes", "Preview", "TextEdit", "Calculator", "Dashboard",
    "System Preferences", "Trash"
};

static const enum dm_app dm_launchpad_apps[] =
{
    DM_APP_FINDER,
    DM_APP_ITUNES,
    DM_APP_PREVIEW,
    DM_APP_TEXTEDIT,
    DM_APP_CALCULATOR,
    DM_APP_DASHBOARD,
    DM_APP_DIRECTV,
    DM_APP_SITEKICK,
    DM_APP_STEAM,
    DM_APP_NETFLIX,
    DM_APP_PREFERENCES,
    DM_APP_TRASH,
};

static const char * const dm_apple_menu_labels[DM_APPLE_MENU_ROWS] =
{
    "About This iPod",
    "System Preferences...",
    "Recent Items",
    "Sleep Display",
    "Restart Rockbox...",
    "Return to iPod..."
};

static const char * const dm_app_menu_names[DM_APP_MENU_COUNT] =
{
    "File", "Edit", "View", "Window"
};

static const char * const dm_app_menu_labels[DM_APP_MENU_COUNT]
                                                [DM_APP_MENU_COUNT] =
{
    { "New Finder Window", "Close Window", "Show Desktop", "Exit Desktop..." },
    { "Open TextEdit", "Clear Selection", "Preferences...", "Cancel" },
    { "Toggle Full Screen", "Show/Hide Icons", "Launchpad", "Refresh" },
    { "Minimize", "Next Window", "Bring to Front", "Show Desktop" },
};

static const char * const dm_context_labels[DM_CONTEXT_ROWS] =
{
    "Get Info",
    "New Folder...",
    "Delete..."
};

static struct dm_asset dm_assets[DM_ASSET_COUNT];
static struct dm_font dm_fonts[DM_FONT_COUNT];
static fb_data *dm_canvas;
static struct desktop_surface dm_surface;
static struct dm_settings dm_settings =
{
    .pointer_speed = 2,
    .reverse_wheel = false,
    .show_desktop_folders = true,
    .drag_lock = false,
    .restore_session = true,
    .start_app = DM_APP_DESKTOP,
};
static struct dm_file dm_files[DM_MAX_FILES];
static int dm_file_count;
static int dm_file_selected;
static int dm_file_top;

#define DM_TEXTEDIT_NOTE_SIZE 192
static char dm_textedit_note[DM_TEXTEDIT_NOTE_SIZE] =
    "A little Mac for your iPod.";
static long dm_calculator_value;
static long dm_calculator_accumulator;
static char dm_calculator_operation;
static bool dm_calculator_new_value = true;

enum dm_itunes_source
{
    DM_ITUNES_SONGS = 0,
    DM_ITUNES_ALBUMS,
    DM_ITUNES_ARTISTS,
    DM_ITUNES_GENRES,
    DM_ITUNES_VIDEOS,
    DM_ITUNES_PLAYLISTS,
    DM_ITUNES_SOURCE_COUNT
};

#define DM_ITUNES_PAGE_MAX 32
#define DM_ITUNES_TITLE_MAX 96
#define DM_ITUNES_DETAIL_MAX 64
#define DM_ITUNES_VIDEO_INDEX ROCKBOX_DIR "/videolist/index.tsv"
#define DM_WEATHER_FORECAST ROCKBOX_DIR "/rockpod/weather/forecast.tsv"
#define DM_WEATHER_ICON_DIR ROCKBOX_DIR "/rockpod/weather"
#define DM_DASH_WEATHER_ICON_SIZE 32
#define DM_DASH_WEATHER_ICON_BYTES \
    (BM_SIZE(DM_DASH_WEATHER_ICON_SIZE, DM_DASH_WEATHER_ICON_SIZE, \
             FORMAT_NATIVE, false) + \
     DM_DASH_WEATHER_ICON_SIZE * sizeof(uint32_t) * 4 * 3 + 3)

struct dm_itunes_row
{
    char title[DM_ITUNES_TITLE_MAX];
    char artist[DM_ITUNES_DETAIL_MAX];
    char album[DM_ITUNES_DETAIL_MAX];
    char path[MAX_PATH];
    unsigned long duration;
    int idxid;
    int32_t seek;
};

/* Artwork is loaded from the synced library's existing 51px sidecars, one
 * row at a time from the idle service point.  Apple's captured native list
 * uses 17-pixel rows, so four 15px targets fit inside those rows.  The BMP
 * scaler needs three temporary ARGB rows in addition to the native output;
 * keep that bounded workspace in each slot so real covers decode reliably. */
#define DM_ITUNES_ART_SIZE (LCD_WIDTH == 640 ? 40 : 15)
#define DM_ITUNES_ART_SLOTS (LCD_WIDTH == 640 ? 12 : 4)
#define DM_ITUNES_ART_DATA_SIZE \
    (BM_SIZE(DM_ITUNES_ART_SIZE, DM_ITUNES_ART_SIZE, FORMAT_NATIVE, false) + \
     DM_ITUNES_ART_SIZE * sizeof(uint32_t) * 4 * 3 + 3)

struct dm_itunes_art_slot
{
    bool attempted;
    bool valid;
    int row;
    char path[MAX_PATH];
    struct bitmap bitmap;
    unsigned char data[DM_ITUNES_ART_DATA_SIZE];
};

static struct dm_itunes_row dm_itunes_rows[DM_ITUNES_PAGE_MAX];
static struct dm_itunes_art_slot dm_itunes_art[DM_ITUNES_ART_SLOTS];
static long dm_itunes_art_not_before;
static enum dm_itunes_source dm_itunes_source = DM_ITUNES_SONGS;
static struct desktop_library dm_library;
static char dm_library_title[4][DM_ITUNES_TITLE_MAX];
static char dm_itunes_playlist[MAX_PATH];
static int dm_itunes_count;
static int dm_itunes_total;
static int dm_itunes_page_top;
static bool dm_itunes_database_pending;
static bool dm_itunes_has_more;
static bool dm_itunes_total_known;
static char dm_itunes_search[32];
static unsigned char dm_itunes_visible[DM_ITUNES_PAGE_MAX];
static int dm_itunes_visible_count;
static int dm_dashboard_track_total;
static struct tm dm_dashboard_time;
struct dm_dashboard_weather
{
    bool available;
    bool icon_valid;
    bool night;
    long checked_tick;
    char icon[24];
    char condition[32];
    char location[32];
    char temperature[16];
    char icon_path[MAX_PATH];
    struct bitmap icon_bitmap;
    unsigned char icon_data[DM_DASH_WEATHER_ICON_BYTES]
        __attribute__((aligned(4)));
};
static struct dm_dashboard_weather dm_dashboard_weather;
#ifdef HAVE_TAGCACHE
static uint32_t dm_itunes_uniqbuf[2048];
#endif
static bool dm_desktop_preferences;
static bool dm_wallpaper_picker;
static int dm_wallpaper_scale;
static int dm_auto_activate;
static int dm_display_mode;
static char dm_wallpaper_path[MAX_PATH];
static char dm_wallpaper_recent[4][MAX_PATH];
static char dm_wallpaper_status[64];
static char dm_photo_root[MAX_PATH] = "/Photos";
/* Session-only credentials. Never saved or sent to the trace. */
static char dm_photo_codes[8][5];
static int dm_photo_code_count;
static char dm_photo_pending[MAX_PATH];
static char dm_photo_expected[5], dm_photo_entered[5];
static bool dm_photo_pending_dir;
static bool dm_photo_wallpaper_private;
static const char *dm_photo_prompt = "Enter photo code";

/* Require an ordinary path in this collection. Reject aliases and hidden
 * sidecars so neither recents nor a crafted config can bypass a folder lock. */
static const char *dm_photo_relative(const char *path)
{
    size_t n = rb->strlen(dm_photo_root);
    if (rb->strncasecmp(path, dm_photo_root, n) ||
        (path[n] && path[n] != '/')) return NULL;
    const char *rel = path + n;
    if (*rel) rel++;
    for (const char *p = rel; *p; p++)
        if (*p == '\\' || ((*p == '.' || *p == '/') &&
                           (p == rel || p[-1] == '/'))) return NULL;
    return rel;
}

/* 0: authorized, 1: code required, -1: unreadable/invalid lock metadata.
 * Ancestors apply too: opening a recent file must not skip its folder lock.
 * FAT is case-insensitive, so lock matching must be case-insensitive as well. */
static int dm_photo_access(const char *path, bool use_session, char *expected)
{
    char line[MAX_PATH + 16];
    const char *rel = dm_photo_relative(path);
    int count, result = 0;
    if (!rel) return -1;
    int fd = rb->open(PLUGIN_APPS_DATA_DIR "/photos.locks", O_RDONLY);
    if (fd < 0)
    {
#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
        return *rb->__errno() == ENOENT ? 0 : -1;
#else
        return errno == ENOENT ? 0 : -1;
#endif
    }
    while ((count = rb->read_line(fd, line, sizeof(line))) > 0)
    {
        char *pin = rb->strrchr(line, '|');
        if (count >= (int)sizeof(line) - 1 || !pin || pin == line ||
            rb->strlen(pin + 1) != 4)
        { result = -1; break; }
        *pin++ = 0;
        bool valid = true;
        for (int i = 0; i < 4; i++)
            if (pin[i] < '0' || pin[i] > '9') valid = false;
        if (!valid) { result = -1; break; }
        size_t n = rb->strlen(line);
        if (rb->strncasecmp(rel, line, n) || (rel[n] && rel[n] != '/'))
            continue;
        bool verified = false;
        if (use_session)
            for (int i = 0; i < dm_photo_code_count; i++)
                if (!rb->strcmp(pin, dm_photo_codes[i])) verified = true;
        if (!verified)
        {
            if (expected) rb->strlcpy(expected, pin, 5);
            result = 1;
            break;
        }
    }
    if (count < 0) result = -1;
    rb->close(fd);
    return result;
}

static char dm_cwd[MAX_PATH] = "/";
#define DM_HISTORY_COUNT 8
static char dm_history[DM_HISTORY_COUNT][MAX_PATH];
static int dm_history_count;
static int dm_history_index;
static enum dm_app dm_saved_app = DM_APP_DESKTOP;
static int dm_saved_cursor_x = LCD_WIDTH / 2;
static int dm_saved_cursor_y = LCD_HEIGHT / 2;
static unsigned char *dm_arena;
static size_t dm_arena_left;
static size_t dm_arena_total;
static enum dm_error dm_last_error;
static struct dm_rect dm_paint_clip = { 0, 0, LCD_WIDTH, LCD_HEIGHT };

static bool dm_point_in_rect(int x, int y, struct dm_rect rect)
{
    return x >= rect.x && x < rect.x + rect.width &&
           y >= rect.y && y < rect.y + rect.height;
}

static void *dm_alloc(size_t bytes)
{
    size_t aligned = ALIGN_UP(bytes, 4);
    void *result;

    if (!dm_arena || aligned > dm_arena_left)
    {
        dm_last_error = DM_ERR_ARENA_OVERFLOW;
        return NULL;
    }
    result = dm_arena;
    dm_arena += aligned;
    dm_arena_left -= aligned;
    return result;
}

static bool dm_asset_files_present(void)
{
    int id;
    int frame;

    if (!rb->file_exists(DM_MANIFEST_FILE))
    {
        dm_last_error = DM_ERR_ASSET_MISSING;
        DEBUGF("desktop mode: missing %s\n", DM_MANIFEST_FILE);
        return false;
    }
    for (id = 0; id < DM_ASSET_COUNT; id++)
    {
        if (!rb->file_exists(dm_asset_defs[id].path))
        {
            dm_last_error = DM_ERR_ASSET_MISSING;
            DEBUGF("desktop mode: missing asset %d %s\n",
                   id, dm_asset_defs[id].path);
            return false;
        }
    }
    if (!rb->file_exists(dm_boot_background_path))
    {
        dm_last_error = DM_ERR_ASSET_MISSING;
        return false;
    }
    for (frame = 0; frame < DM_BOOT_FRAME_COUNT; frame++)
    {
        if (!rb->file_exists(dm_boot_spinner_paths[frame]))
        {
            dm_last_error = DM_ERR_ASSET_MISSING;
            return false;
        }
    }
    for (id = 0; id < DM_FONT_COUNT; id++)
    {
        if (!rb->file_exists(dm_font_defs[id].coverage) ||
            !rb->file_exists(dm_font_defs[id].metrics))
        {
            dm_last_error = DM_ERR_ASSET_MISSING;
            return false;
        }
    }
    return true;
}

static NO_INLINE enum dm_boot_result dm_show_boot_animation(void)
{
    /* The native main thread has an 8 KiB stack.  These descriptors remain
     * live across every BMP decode and frame delay, so keeping them automatic
     * needlessly adds to the decoder's peak stack use. */
    static struct bitmap background;
    static struct bitmap spinners[DM_BOOT_FRAME_COUNT];
    unsigned char *arena;
    size_t buffer_size;
    size_t background_bytes =
        BM_SIZE(LCD_WIDTH, LCD_HEIGHT, FORMAT_NATIVE, false);
    size_t spinner_bytes =
        BM_SIZE(DM_BOOT_SPINNER_SIZE, DM_BOOT_SPINNER_SIZE,
                FORMAT_NATIVE, false);
    int frame;

    if (!dm_asset_files_present())
        return DM_BOOT_ERROR;
    arena = rb->plugin_get_buffer(&buffer_size);
    if (!arena ||
        buffer_size < background_bytes +
                      spinner_bytes * DM_BOOT_FRAME_COUNT)
    {
        dm_last_error = DM_ERR_PLUGIN_BUFFER;
        return DM_BOOT_ERROR;
    }

    rb->memset(&background, 0, sizeof(background));
    background.width = LCD_WIDTH;
    background.height = LCD_HEIGHT;
    background.format = FORMAT_NATIVE;
    background.data = arena;
    arena += ALIGN_UP(background_bytes, 4);
    if (rb->read_bmp_file(
            dm_boot_background_path, &background, background_bytes,
        FORMAT_NATIVE, NULL) <= 0 ||
        background.width != LCD_WIDTH ||
        background.height != LCD_HEIGHT)
    {
        dm_last_error = DM_ERR_ASSET_INVALID;
        return DM_BOOT_ERROR;
    }

    for (frame = 0; frame < DM_BOOT_FRAME_COUNT; frame++)
    {
        struct bitmap *spinner = &spinners[frame];

        rb->memset(spinner, 0, sizeof(*spinner));
        spinner->width = DM_BOOT_SPINNER_SIZE;
        spinner->height = DM_BOOT_SPINNER_SIZE;
        spinner->format = FORMAT_NATIVE;
        spinner->data = arena;
        arena += ALIGN_UP(spinner_bytes, 4);
        if (rb->read_bmp_file(
                dm_boot_spinner_paths[frame], spinner, spinner_bytes,
            FORMAT_NATIVE, NULL) <= 0 ||
            spinner->width != DM_BOOT_SPINNER_SIZE ||
            spinner->height != DM_BOOT_SPINNER_SIZE)
        {
            dm_last_error = DM_ERR_ASSET_INVALID;
            return DM_BOOT_ERROR;
        }
    }

    while (true)
    {
        long button = rb->button_get(false);

        if (button == BUTTON_NONE)
            break;
        if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
            return DM_BOOT_USB_CONNECTED;
    }

    for (frame = 0; frame < DM_BOOT_FRAME_COUNT; frame++)
    {
        long deadline;

        rb->lcd_bitmap(
            (const fb_data *)background.data,
            0, 0, LCD_WIDTH, LCD_HEIGHT);
        rb->lcd_bitmap(
            (const fb_data *)spinners[frame].data,
            DM_BOOT_SPINNER_X, DM_BOOT_SPINNER_Y,
            DM_BOOT_SPINNER_SIZE, DM_BOOT_SPINNER_SIZE);
        rb->lcd_update();
        deadline = *rb->current_tick + DM_BOOT_FRAME_TICKS;
        while (TIME_BEFORE(*rb->current_tick, deadline))
        {
            long button = rb->button_get_w_tmo(
                MAX(1, deadline - *rb->current_tick));

            if (button != BUTTON_NONE)
            {
                if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
                    return DM_BOOT_USB_CONNECTED;
                rb->button_clear_queue();
                return DM_BOOT_DONE;
            }
            if (rb->button_hold())
                return DM_BOOT_DONE;
        }
    }
    rb->button_clear_queue();
    return DM_BOOT_DONE;
}

static bool dm_read_exact(const char *path, void *buffer, size_t bytes)
{
    int fd = rb->open(path, O_RDONLY);
    ssize_t read_bytes;

    if (fd < 0)
        return false;
    read_bytes = rb->read(fd, buffer, bytes);
    rb->close(fd);
    return read_bytes == (ssize_t)bytes;
}

static bool dm_load_bmp_asset(enum dm_asset_id id)
{
    const struct dm_asset_def *def = &dm_asset_defs[id];
    struct dm_asset *asset = &dm_assets[id];
    struct bitmap bitmap;
    size_t bytes = BM_SIZE(def->width, def->height, FORMAT_NATIVE, false);
    void *data = dm_alloc(bytes);

    if (!data)
        return false;
    rb->memset(&bitmap, 0, sizeof(bitmap));
    bitmap.width = def->width;
    bitmap.height = def->height;
    bitmap.format = FORMAT_NATIVE;
    bitmap.data = data;
    if (rb->read_bmp_file(def->path, &bitmap, bytes, FORMAT_NATIVE, NULL) <= 0 ||
        bitmap.width != def->width || bitmap.height != def->height)
        return false;
    asset->pixels = (fb_data *)data;
    asset->coverage = NULL;
    asset->width = def->width;
    asset->height = def->height;
    asset->loaded = true;
    return true;
}

/* RGA1: "RGA1", uint16 width, uint16 height, then width*height triples of
 * little-endian RGB565 plus Apple's 8-bit coverage.
 */
static bool dm_load_rga_asset(enum dm_asset_id id)
{
    const struct dm_asset_def *def = &dm_asset_defs[id];
    struct dm_asset *asset = &dm_assets[id];
    int count = def->width * def->height;
    unsigned char header[8];
    unsigned char triple[3];
    fb_data *pixels = dm_alloc((size_t)count * sizeof(fb_data));
    unsigned char *coverage = dm_alloc((size_t)count);
    int fd;
    int index;

    if (!pixels || !coverage)
        return false;
    fd = rb->open(def->path, O_RDONLY);
    if (fd < 0)
        return false;
    if (rb->read(fd, header, sizeof(header)) != (ssize_t)sizeof(header) ||
        header[0] != 'R' || header[1] != 'G' || header[2] != 'A' ||
        header[3] != '1' ||
        (header[4] | (header[5] << 8)) != def->width ||
        (header[6] | (header[7] << 8)) != def->height)
    {
        rb->close(fd);
        return false;
    }
    for (index = 0; index < count; index++)
    {
        if (rb->read(fd, triple, sizeof(triple)) != (ssize_t)sizeof(triple))
        {
            rb->close(fd);
            return false;
        }
        pixels[index] = (fb_data)(triple[0] | (triple[1] << 8));
        coverage[index] = triple[2];
    }
    rb->close(fd);
    asset->pixels = pixels;
    asset->coverage = coverage;
    asset->width = def->width;
    asset->height = def->height;
    asset->loaded = true;
    return true;
}

/* DMF2: "DMF2", cell width, cell height, ascent, glyph count, signed atlas
 * origin, reserved byte, then one advance per glyph.  Every glyph shares the
 * same baseline inside its cell.  The origin lets the atlas retain negative
 * left side bearings while the pen still advances by the font's real advance.
 */
static bool dm_load_font(enum dm_font_id id)
{
    const struct dm_font_def *def = &dm_font_defs[id];
    struct dm_font *font = &dm_fonts[id];
    unsigned char metrics[10 + DM_ASCII_COUNT];
    size_t bytes;

    if (!dm_read_exact(def->metrics, metrics, sizeof(metrics)))
    {
        DEBUGF("desktop mode: font %d metrics read failed %s\n", id,
               def->metrics);
        return false;
    }
    if (metrics[0] != 'D' || metrics[1] != 'M' || metrics[2] != 'F' ||
        metrics[3] != '2' || metrics[7] != DM_ASCII_COUNT)
    {
        DEBUGF("desktop mode: font %d metrics header invalid\n", id);
        return false;
    }
    font->cell_w = metrics[4];
    font->cell_h = metrics[5];
    font->ascent = metrics[6];
    font->origin = metrics[8] < 128 ? (signed char)metrics[8] :
                                      (signed char)(metrics[8] - 256);
    if (font->cell_w == 0 || font->cell_h == 0)
        return false;
    rb->memcpy(font->advance, metrics + 10, DM_ASCII_COUNT);
    bytes = (size_t)font->cell_w * DM_FONT_COLUMNS *
            (size_t)font->cell_h * DM_FONT_ROWS;
    font->coverage = dm_alloc(bytes);
    if (!font->coverage || !dm_read_exact(def->coverage, font->coverage, bytes))
    {
        DEBUGF("desktop mode: font %d coverage read failed (%lu bytes)\n", id,
               (unsigned long)bytes);
        return false;
    }
    font->loaded = true;
    return true;
}

static bool dm_load_assets(void)
{
    int id;
    size_t size;

    if (!dm_asset_files_present())
        return false;
    dm_arena = rb->plugin_get_buffer(&size);
    dm_arena_left = size;
    dm_arena_total = size;
    if (!dm_arena)
    {
        dm_last_error = DM_ERR_PLUGIN_BUFFER;
        return false;
    }

    dm_canvas = dm_alloc((size_t)LCD_WIDTH * LCD_HEIGHT * sizeof(fb_data));
    if (!dm_canvas)
        return false;
    for (id = 0; id < DM_ASSET_COUNT; id++)
    {
        bool ok = id < DM_ASSET_BMP_COUNT ?
                      dm_load_bmp_asset((enum dm_asset_id)id) :
                      dm_load_rga_asset((enum dm_asset_id)id);

        if (!ok)
        {
            DEBUGF("desktop mode: asset load failed %d %s\n", id,
                   dm_asset_defs[id].path);
            if (dm_last_error == DM_ERR_NONE)
                dm_last_error = DM_ERR_ASSET_INVALID;
            return false;
        }
    }
    for (id = 0; id < DM_FONT_COUNT; id++)
    {
        if (!dm_load_font((enum dm_font_id)id))
        {
            DEBUGF("desktop mode: font load failed %d\n", id);
            if (dm_last_error == DM_ERR_NONE)
                dm_last_error = DM_ERR_ASSET_INVALID;
            return false;
        }
    }
    return true;
}

/* Wallpaper preparation is an explicit operation. The unused tail of the
 * plugin arena is scratch; it is never playback memory. Commit only after
 * successful decode, replacing the existing Aurora plane. No second cached
 * background is retained and redraws only copy the damaged pixels. */
static bool dm_wallpaper_apply(const char *path)
{
    char preview[MAX_PATH];
    if (path[0])
    {
        if (dm_photo_access(path, true, NULL) != 0) return false;
        /* Sync previews cover GIF/PNG/PPM as well as large originals. Always
         * authorize the original before reading its hidden preview. */
        const char *rel = dm_photo_relative(path);
        int n = rb->snprintf(preview, sizeof(preview),
                            "%s/.photo_previews/%s.bmp", dm_photo_root, rel);
        if (n > 0 && n < (int)sizeof(preview) && rb->file_exists(preview))
            path = preview;
    }
    struct bitmap bitmap;
    struct desktop_rect source, destination;
    int original_w, original_h, result;
    bool bmp = false;
    const char *ext = rb->strrchr(path, '.');
    size_t capacity = MIN(dm_arena_left, (size_t)1024 * 1024);
    int (*decode)(const char *, struct bitmap *, int, int,
                  const struct custom_format *) = rb->read_bmp_file;
    rb->memset(&bitmap, 0, sizeof(bitmap));
    bitmap.data = dm_arena;
    if (!path[0]) path = DM_F_AURORA;
    else if (!ext || (rb->strcasecmp(ext, ".bmp") &&
                     rb->strcasecmp(ext, ".jpg") &&
                     rb->strcasecmp(ext, ".jpe") &&
                     rb->strcasecmp(ext, ".jpeg"))) return false;
    ext = rb->strrchr(path, '.');
    bmp = ext && !rb->strcasecmp(ext, ".bmp");
#ifdef HAVE_JPEG
    if (!bmp) decode = rb->read_jpeg_file;
#else
    if (!bmp) return false;
#endif
    result = decode(path, &bitmap, capacity,
                    FORMAT_NATIVE | FORMAT_RETURN_SIZE, NULL);
    if (result <= 0 || bitmap.width <= 0 || bitmap.height <= 0) return false;
    original_w = bitmap.width;
    original_h = bitmap.height;
    if (!desktop_image_layout(original_w, original_h, LCD_WIDTH, LCD_HEIGHT,
                              dm_wallpaper_scale, &source, &destination))
        return false;
    /* Bounded decoder output, including its own scratch rows. Large JPEGs
     * use Rockbox's scaled IDCT; pathological images fail without replacing
     * the current wallpaper. */
    bitmap.width = LCD_WIDTH;
    bitmap.height = LCD_HEIGHT;
    result = decode(path, &bitmap, capacity,
                    FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT, NULL);
    if (result <= 0 || bitmap.width <= 0 || bitmap.height <= 0) return false;
    fb_data *pixels = dm_assets[DM_ASSET_AURORA].pixels;
    fb_data *decoded = (fb_data *)bitmap.data;
    for (int y = 0; y < LCD_HEIGHT; y++)
    {
        for (int x = 0; x < LCD_WIDTH; x++)
        {
            fb_data pixel = LCD_RGBPACK(30, 39, 53);
            if (desktop_hit(destination, x, y))
            {
                int sx = source.x + (int64_t)(x - destination.x) *
                         source.width / destination.width;
                int sy = source.y + (int64_t)(y - destination.y) *
                         source.height / destination.height;
                sx = (int64_t)sx * bitmap.width / original_w;
                sy = (int64_t)sy * bitmap.height / original_h;
                pixel = decoded[sy * bitmap.width + sx];
            }
            pixels[y * LCD_WIDTH + x] = pixel;
        }
        if ((y & 15) == 0) rb->yield();
    }
    return true;
}

static bool dm_wallpaper_select(const char *path)
{
    char selected[MAX_PATH];
    rb->strlcpy(selected, path, sizeof(selected));
    if (!dm_wallpaper_apply(selected))
    {
        rb->strlcpy(dm_wallpaper_status, "Image unavailable or too large",
                    sizeof(dm_wallpaper_status));
        return false;
    }
    if (selected[0])
    {
        int found = 3;
        for (int i = 0; i < 4; i++)
            if (!rb->strcmp(selected, dm_wallpaper_recent[i]))
            { found = i; break; }
        for (int i = found; i > 0; i--)
            rb->strlcpy(dm_wallpaper_recent[i], dm_wallpaper_recent[i - 1],
                        sizeof(dm_wallpaper_recent[i]));
        rb->strlcpy(dm_wallpaper_recent[0], selected,
                    sizeof(dm_wallpaper_recent[0]));
    }
    dm_photo_wallpaper_private = selected[0] &&
        dm_photo_access(selected, false, NULL) != 0;
    rb->strlcpy(dm_wallpaper_path, selected, sizeof(dm_wallpaper_path));
    rb->strlcpy(dm_wallpaper_status, "Wallpaper updated",
                sizeof(dm_wallpaper_status));
    return true;
}

/* ------------------------------------------------------------------ */
/* Compositor                                                          */
/*                                                                     */
/* Every frame is assembled in one off-screen RGB565 plane and handed  */
/* to the LCD once.  The plugin API exposes no framebuffer, and real   */
/* Snow Leopard chrome is full of soft edges and antialiased Lucida    */
/* Grande, so the shell needs somewhere it can read back what it has   */
/* already drawn in order to blend over it.                            */
/* ------------------------------------------------------------------ */

static bool dm_rect_intersect(struct dm_rect *rect,
                              const struct dm_rect *clip);
static void dm_update_hover(struct dm_state *state);

static inline fb_data dm_blend(fb_data destination, fb_data source,
                               unsigned int coverage)
{
    unsigned int alpha = coverage + (coverage >> 7);
    unsigned int inverse = 256 - alpha;
    unsigned int red =
        ((source >> 11) * alpha + (destination >> 11) * inverse) >> 8;
    unsigned int green =
        (((source >> 5) & 0x3f) * alpha +
         ((destination >> 5) & 0x3f) * inverse) >> 8;
    unsigned int blue =
        ((source & 0x1f) * alpha + (destination & 0x1f) * inverse) >> 8;

    return (fb_data)((red << 11) | (green << 5) | blue);
}

static void dm_blit(enum dm_asset_id id, int x, int y)
{
    const struct dm_asset *asset = &dm_assets[id];
    struct dm_rect target;
    int source_x;
    int source_y;
    int row;

    if (!asset->loaded)
        return;
    target = (struct dm_rect){ x, y, asset->width, asset->height };
    if (!dm_rect_intersect(&target, &dm_paint_clip))
        return;
    source_x = target.x - x;
    source_y = target.y - y;
    for (row = 0; row < target.height; row++)
        rb->memcpy(dm_canvas + (target.y + row) * LCD_WIDTH + target.x,
                   asset->pixels + (source_y + row) * asset->width +
                       source_x,
                   (size_t)target.width * sizeof(fb_data));
}

static void dm_blit_part(enum dm_asset_id id, int x, int y, int width)
{
    const struct dm_asset *asset = &dm_assets[id];
    struct dm_rect target;
    int source_x;
    int source_y;
    int row;

    if (!asset->loaded)
        return;
    target = (struct dm_rect){ x, y, MIN(width, asset->width), asset->height };
    if (!dm_rect_intersect(&target, &dm_paint_clip))
        return;
    source_x = target.x - x;
    source_y = target.y - y;
    for (row = 0; row < target.height; row++)
        rb->memcpy(dm_canvas + (target.y + row) * LCD_WIDTH + target.x,
                   asset->pixels + (source_y + row) * asset->width +
                       source_x,
                   (size_t)target.width * sizeof(fb_data));
}

/* Scale only the loaded chrome directly into the compositor plane.  This is
 * used for the explicit fullscreen state, so the source-profile window can
 * fill the available 320x219 work area below the menu bar without a second
 * framebuffer or a temporary bitmap.  The DCP750's separate video-out
 * compositor then scales the finished native frame to (36,24) 648x432. */
static void dm_blit_scaled(enum dm_asset_id id, int x, int y,
                           int width, int height)
{
    const struct dm_asset *asset = &dm_assets[id];
    struct dm_rect target;
    int row;
    int column;

    if (!asset->loaded || width <= 0 || height <= 0)
        return;
    target = (struct dm_rect){ x, y, width, height };
    if (!dm_rect_intersect(&target, &dm_paint_clip))
        return;
    for (row = target.y - y; row < target.y - y + target.height; row++)
    {
        int source_y = row * asset->height / height;
        fb_data *destination = dm_canvas + (y + row) * LCD_WIDTH;

        for (column = target.x - x;
             column < target.x - x + target.width; column++)
        {
            int source_x = column * asset->width / width;

            destination[x + column] =
                asset->pixels[source_y * asset->width + source_x];
        }
    }
}

/* Reflow a bounded strip of captured chrome directly into the compositor.
 * Native iTunes uses this only for its already-owned library background: no
 * file or bitmap work is introduced into the paint path. */
static void dm_blit_asset_region(enum dm_asset_id id, int x, int y,
                                 int source_x, int source_y,
                                 int source_width, int source_height,
                                 int width, int height)
{
    const struct dm_asset *asset = &dm_assets[id];
    struct dm_rect target;
    int row;
    int column;

    if (!asset->loaded || source_x < 0 || source_y < 0 ||
        source_width <= 0 || source_height <= 0 ||
        source_x + source_width > asset->width ||
        source_y + source_height > asset->height || width <= 0 ||
        height <= 0)
        return;
    target = (struct dm_rect){ x, y, width, height };
    if (!dm_rect_intersect(&target, &dm_paint_clip))
        return;
    for (row = target.y - y; row < target.y - y + target.height; row++)
    {
        int source_row = source_y + row * source_height / height;
        fb_data *destination = dm_canvas + (y + row) * LCD_WIDTH;

        for (column = target.x - x;
             column < target.x - x + target.width; column++)
        {
            int pixel_x = source_x + column * source_width / width;

            destination[x + column] =
                asset->pixels[source_row * asset->width + pixel_x];
        }
    }
}

static void dm_blit_window(enum dm_asset_id id)
{
    dm_blit_scaled(id, DM_WIN_X, DM_WIN_Y, DM_WIN_W, DM_WIN_H);
}

/* Panels are horizontally uniform between their end caps, so any width can be
 * drawn from the real artwork: real left cap, real repeated middle column,
 * real right cap.  Used for tooltips, which hug their label the way Snow
 * Leopard's do.
 */
static void dm_blit_panel(enum dm_asset_id id, int x, int y, int width,
                          int cap)
{
    const struct dm_asset *asset = &dm_assets[id];
    struct dm_rect target_rect;
    int row;
    int column;

    if (!asset->loaded || width < cap * 2 || x < 0 ||
        x + width > LCD_WIDTH || y < 0 || y + asset->height > LCD_HEIGHT)
        return;
    target_rect = (struct dm_rect){ x, y, width, asset->height };
    if (!dm_rect_intersect(&target_rect, &dm_paint_clip))
        return;
    for (row = target_rect.y - y;
         row < target_rect.y - y + target_rect.height; row++)
    {
        const fb_data *source = asset->pixels + row * asset->width;
        fb_data *target = dm_canvas + (y + row) * LCD_WIDTH + x;

        for (column = target_rect.x - x;
             column < target_rect.x - x + target_rect.width; column++)
        {
            if (column < cap)
                target[column] = source[column];
            else if (column >= width - cap)
                target[column] = source[asset->width - (width - column)];
            else
                target[column] = source[cap];
        }
    }
}

static void dm_compose(enum dm_asset_id id, int x, int y)
{
    const struct dm_asset *asset = &dm_assets[id];
    int row;
    int column;

    if (!asset->loaded || !asset->coverage)
        return;
    for (row = 0; row < asset->height; row++)
    {
        int target_y = y + row;
        const fb_data *source;
        const unsigned char *alpha;
        fb_data *target;

        if (target_y < dm_paint_clip.y ||
            target_y >= dm_paint_clip.y + dm_paint_clip.height)
            continue;
        source = asset->pixels + row * asset->width;
        alpha = asset->coverage + row * asset->width;
        target = dm_canvas + target_y * LCD_WIDTH;
        for (column = 0; column < asset->width; column++)
        {
            int target_x = x + column;
            unsigned int value = alpha[column];

            if (target_x < dm_paint_clip.x ||
                target_x >= dm_paint_clip.x + dm_paint_clip.width ||
                value == 0)
                continue;
            target[target_x] = value == 255 ?
                source[column] :
                dm_blend(target[target_x], source[column], value);
        }
    }
}

/* A small decoded bitmap can be painted into the same compositor plane as
 * the captured assets.  This is deliberately a cached-only draw helper:
 * image I/O and resizing happen in the bounded service functions below. */
static void dm_blit_bitmap(const struct bitmap *bitmap, int x, int y,
                           bool transparent)
{
    struct dm_rect target;
    int source_x;
    int source_y;
    int row;
    int column;
    const fb_data *source;

    if (!bitmap || !bitmap->data || bitmap->width <= 0 ||
        bitmap->height <= 0)
        return;
    target = (struct dm_rect){ x, y, bitmap->width, bitmap->height };
    if (!dm_rect_intersect(&target, &dm_paint_clip))
        return;
    source_x = target.x - x;
    source_y = target.y - y;
    source = (const fb_data *)bitmap->data;
    for (row = 0; row < target.height; row++)
    {
        fb_data *destination = dm_canvas +
            (target.y + row) * LCD_WIDTH + target.x;

        for (column = 0; column < target.width; column++)
        {
            fb_data pixel = source[(source_y + row) * bitmap->width +
                                   source_x + column];

            if (!transparent || pixel != TRANSPARENT_COLOR)
                destination[column] = pixel;
        }
    }
}

/* Coverage-aware nearest-neighbour scaling for the compact Launchpad.  The
 * native window body cannot hold three rows of 32-pixel icons plus labels,
 * but it can hold the same loaded artwork at a bounded 19-pixel size.  This
 * stays in the existing compose plane and does not create a scratch bitmap. */
static void dm_compose_scaled(enum dm_asset_id id, int x, int y,
                              int width, int height)
{
    const struct dm_asset *asset = &dm_assets[id];
    struct dm_rect target;
    int row;
    int column;

    if (!asset->loaded || !asset->coverage || width <= 0 || height <= 0)
        return;
    target = (struct dm_rect){ x, y, width, height };
    if (!dm_rect_intersect(&target, &dm_paint_clip))
        return;
    for (row = target.y - y; row < target.y - y + target.height; row++)
    {
        int source_y = row * asset->height / height;
        fb_data *destination = dm_canvas + (y + row) * LCD_WIDTH;

        for (column = target.x - x;
             column < target.x - x + target.width; column++)
        {
            int source_x = column * asset->width / width;
            unsigned int value = asset->coverage[
                source_y * asset->width + source_x];
            int target_x = x + column;

            if (value == 0)
                continue;
            destination[target_x] = value == 255 ?
                asset->pixels[source_y * asset->width + source_x] :
                dm_blend(destination[target_x],
                         asset->pixels[source_y * asset->width + source_x],
                         value);
        }
    }
}

/* The real Snow Leopard shelf reflects the lower edge of each Dock icon.
 * Paint that reflection into the existing canvas before the icon itself; the
 * shelf remains the only Dock background and the short, fading copy costs no
 * second framebuffer or asset allocation. */
static void dm_compose_dock_reflection(enum dm_asset_id id, int x, int y)
{
    const struct dm_asset *asset = &dm_assets[id];
    int reflection_y;
    int reflection_height;
    int row;
    int column;

    if (!asset->loaded || !asset->coverage)
        return;
    reflection_y = y + asset->height;
    reflection_height = MIN(asset->height, LCD_HEIGHT - reflection_y);
    if (reflection_height <= 0)
        return;
    for (row = 0; row < reflection_height; row++)
    {
        int source_y = asset->height - 1 - row;
        unsigned int fade = DM_DOCK_REFLECTION_MAX_ALPHA *
                             (reflection_height - row) /
                             (reflection_height + 1);
        fb_data *target = dm_canvas + (reflection_y + row) * LCD_WIDTH;

        if (reflection_y + row < dm_paint_clip.y ||
            reflection_y + row >= dm_paint_clip.y + dm_paint_clip.height)
            continue;
        for (column = 0; column < asset->width; column++)
        {
            int target_x = x + column;
            unsigned int value = asset->coverage[source_y * asset->width +
                                                 column] * fade / 255;

            if (target_x < dm_paint_clip.x ||
                target_x >= dm_paint_clip.x + dm_paint_clip.width ||
                value == 0)
                continue;
            target[target_x] = dm_blend(target[target_x],
                asset->pixels[source_y * asset->width + column], value);
        }
    }
}

static void dm_fill_rect(struct dm_rect rect, fb_data colour)
{
    int row;
    int column;

    if (!dm_rect_intersect(&rect, &dm_paint_clip))
        return;
    for (row = rect.y; row < rect.y + rect.height; row++)
    {
        fb_data *target = dm_canvas + row * LCD_WIDTH;

        for (column = rect.x; column < rect.x + rect.width; column++)
            target[column] = colour;
    }
}

/* Dashboard in Snow Leopard dims the desktop without replacing it.  Blend the
 * already-painted Aurora pixels in place so the original wallpaper remains
 * visible through the widget layer and no second framebuffer is needed. */
static void dm_dim_rect(struct dm_rect rect, fb_data colour,
                        unsigned int coverage)
{
    int row;
    int column;

    if (!dm_rect_intersect(&rect, &dm_paint_clip))
        return;
    for (row = rect.y; row < rect.y + rect.height; row++)
    {
        fb_data *target = dm_canvas + row * LCD_WIDTH;

        for (column = rect.x; column < rect.x + rect.width; column++)
            target[column] = dm_blend(target[column], colour, coverage);
    }
}

static void dm_put_pixel(int x, int y, fb_data colour)
{
    if (x < dm_paint_clip.x || x >= dm_paint_clip.x + dm_paint_clip.width ||
        y < dm_paint_clip.y || y >= dm_paint_clip.y + dm_paint_clip.height)
        return;
    if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT)
        dm_canvas[y * LCD_WIDTH + x] = colour;
}

static void dm_draw_line(int x0, int y0, int x1, int y1, fb_data colour)
{
    int dx = DM_ABS(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -DM_ABS(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    while (1)
    {
        int twice;

        dm_put_pixel(x0, y0, colour);
        if (x0 == x1 && y0 == y1)
            break;
        twice = error * 2;
        if (twice >= dy)
        {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx)
        {
            error += dx;
            y0 += sy;
        }
    }
}

static void dm_fill_circle(int cx, int cy, int radius, fb_data colour)
{
    int y;

    for (y = -radius; y <= radius; y++)
    {
        int x = radius;

        while (x > 0 && x * x + y * y > radius * radius)
            x--;
        dm_fill_rect((struct dm_rect){ cx - x, cy + y, x * 2 + 1, 1 },
                     colour);
    }
}

static int dm_text_width(const struct dm_font *font, const char *text)
{
    int width = 0;

    while (*text)
    {
        unsigned char ch = *text++;

        if (ch < DM_ASCII_FIRST || ch > DM_ASCII_LAST)
            ch = '?';
        width += font->advance[ch - DM_ASCII_FIRST];
    }
    return width;
}

static void dm_draw_text(const struct dm_font *font, int x, int y,
                         int max_width, fb_data ink, const char *text)
{
    int atlas_width;

    if (!font->loaded)
        return;
    atlas_width = font->cell_w * DM_FONT_COLUMNS;
    while (*text && max_width > 0)
    {
        unsigned char ch = *text++;
        int index;
        int advance;
        int row;

        if (ch < DM_ASCII_FIRST || ch > DM_ASCII_LAST)
            ch = '?';
        index = ch - DM_ASCII_FIRST;
        advance = font->advance[index];
        if (advance > max_width)
            break;
        for (row = 0; row < font->cell_h; row++)
        {
            const unsigned char *alpha;
            fb_data *target;
            int column;
            int target_y = y + row;

            if (target_y < dm_paint_clip.y ||
                target_y >= dm_paint_clip.y + dm_paint_clip.height)
                continue;
            alpha = font->coverage +
                    ((index / DM_FONT_COLUMNS) * font->cell_h + row) *
                        atlas_width +
                    (index % DM_FONT_COLUMNS) * font->cell_w;
            target = dm_canvas + target_y * LCD_WIDTH;
            for (column = 0; column < font->cell_w; column++)
            {
                int target_x = x + font->origin + column;
                unsigned int value = alpha[column];

                if (target_x < dm_paint_clip.x ||
                    target_x >= dm_paint_clip.x + dm_paint_clip.width ||
                    value == 0)
                    continue;
                target[target_x] = value == 255 ?
                    ink : dm_blend(target[target_x], ink, value);
            }
        }
        x += advance;
        max_width -= advance;
    }
}

static void dm_draw_text_centered(const struct dm_font *font, int x, int y,
                                  int width, fb_data ink, const char *text)
{
    int text_width = dm_text_width(font, text);

    dm_draw_text(font, x + MAX(0, (width - text_width) / 2), y, width,
                 ink, text);
}

static void dm_draw_text_right(const struct dm_font *font, int right, int y,
                               int max_width, fb_data ink, const char *text)
{
    dm_draw_text(font, right - MIN(max_width, dm_text_width(font, text)), y,
                 max_width, ink, text);
}

/* All live text is positioned by the same cell geometry as Apple's controls:
 * y is the top of the control and the atlas cell is vertically centred in
 * that control.  The glyph ink itself then lands on the baseline encoded by
 * the Lucida Grande ascent metric. */
static int dm_font_top_in_box(const struct dm_font *font, int y, int height)
{
    return y + MAX(0, (height - font->cell_h) / 2);
}

static void dm_draw_text_in_box(const struct dm_font *font, int x, int y,
                                int width, int height, fb_data ink,
                                const char *text)
{
    dm_draw_text(font, x, dm_font_top_in_box(font, y, height), width, ink,
                 text);
}

static void dm_draw_text_centered_in_box(const struct dm_font *font, int x,
                                         int y, int width, int height,
                                         fb_data ink, const char *text)
{
    dm_draw_text_centered(font, x,
                          dm_font_top_in_box(font, y, height), width, ink,
                          text);
}

static void dm_draw_text_right_in_box(const struct dm_font *font, int right,
                                      int y, int height, int max_width,
                                      fb_data ink, const char *text)
{
    dm_draw_text_right(font, right,
                       dm_font_top_in_box(font, y, height), max_width, ink,
                       text);
}

/* The Calendar date uses the owned Lucida Grande Bold atlas at a measured 2x
 * scale.  This keeps the real glyph shapes and coverage while avoiding another
 * resident font atlas for the one large Dashboard numeral. */
static void dm_draw_text_scaled_centered_in_box(const struct dm_font *font,
                                                 int x, int y, int width,
                                                 int height, int scale,
                                                 fb_data ink,
                                                 const char *text)
{
    const char *cursor = text;
    int atlas_width;
    int text_width = dm_text_width(font, text) * scale;
    int target_x = x + MAX(0, (width - text_width) / 2);
    int target_y = y + MAX(0, (height - font->cell_h * scale) / 2);

    if (!font->loaded || scale <= 0)
        return;
    atlas_width = font->cell_w * DM_FONT_COLUMNS;
    while (*cursor)
    {
        unsigned char ch = *cursor++;
        int index;
        int advance;
        int row;

        if (ch < DM_ASCII_FIRST || ch > DM_ASCII_LAST)
            ch = '?';
        index = ch - DM_ASCII_FIRST;
        advance = font->advance[index] * scale;
        if (target_x + advance > x + width)
            break;
        for (row = 0; row < font->cell_h; row++)
        {
            const unsigned char *alpha = font->coverage +
                ((index / DM_FONT_COLUMNS) * font->cell_h + row) *
                    atlas_width +
                (index % DM_FONT_COLUMNS) * font->cell_w;
            int vertical;

            for (vertical = 0; vertical < scale; vertical++)
            {
                int paint_y = target_y + row * scale + vertical;
                fb_data *target;
                int column;

                if (paint_y < dm_paint_clip.y ||
                    paint_y >= dm_paint_clip.y + dm_paint_clip.height)
                    continue;
                target = dm_canvas + paint_y * LCD_WIDTH;
                for (column = 0; column < font->cell_w; column++)
                {
                    unsigned int value = alpha[column];
                    int horizontal;

                    if (value == 0)
                        continue;
                    for (horizontal = 0; horizontal < scale; horizontal++)
                    {
                        int paint_x = target_x +
                            (font->origin + column) * scale + horizontal;

                        if (paint_x < x || paint_x >= x + width ||
                            paint_x < dm_paint_clip.x ||
                            paint_x >= dm_paint_clip.x +
                                       dm_paint_clip.width)
                            continue;
                        target[paint_x] = value == 255 ? ink :
                            dm_blend(target[paint_x], ink, value);
                    }
                }
            }
        }
        target_x += advance;
    }
}

/* Desktop labels are white with the same one-pixel shadow Finder uses, which
 * is what keeps them legible over the bright parts of the Aurora picture.
 */
static void dm_draw_desktop_label(int x, int y, int width, const char *text,
                                  bool selected)
{
    if (selected)
    {
        int text_width = MIN(width - 4,
            dm_text_width(&dm_fonts[DM_FONT_SMALL], text));

        dm_fill_rect((struct dm_rect)
        {
            x + (width - text_width) / 2 - 2, y - 1,
            text_width + 4, dm_fonts[DM_FONT_SMALL].cell_h + 2
        }, LCD_RGBPACK(56, 117, 215));
        dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_SMALL], x, y - 1,
                                     width, dm_fonts[DM_FONT_SMALL].cell_h + 2,
                                     DM_INK_WHITE, text);
        return;
    }
    dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_SMALL], x + 1, y,
                                 width, dm_fonts[DM_FONT_SMALL].cell_h + 2,
                                 DM_INK, text);
    dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_SMALL], x, y - 1,
                                 width, dm_fonts[DM_FONT_SMALL].cell_h + 2,
                                 DM_INK_WHITE, text);
}

static void dm_rect_union(struct dm_rect *destination,
                          const struct dm_rect *source)
{
    int right;
    int bottom;

    if (source->width <= 0 || source->height <= 0)
        return;
    if (destination->width <= 0 || destination->height <= 0)
    {
        *destination = *source;
        return;
    }
    right = MAX(destination->x + destination->width,
                source->x + source->width);
    bottom = MAX(destination->y + destination->height,
                 source->y + source->height);
    destination->x = MIN(destination->x, source->x);
    destination->y = MIN(destination->y, source->y);
    destination->width = right - destination->x;
    destination->height = bottom - destination->y;
}

static void dm_request_damage(struct dm_state *state, struct dm_rect rect)
{
    dm_rect_union(&state->requested_damage, &rect);
    state->redraw = true;
}

static uint32_t dm_signature_add(uint32_t signature, unsigned int value)
{
    return (signature ^ value) * 16777619u;
}

static uint32_t dm_signature_string(uint32_t signature, const char *text)
{
    while (*text)
        signature = dm_signature_add(signature, (unsigned char)*text++);
    return signature;
}

static uint32_t dm_scene_signature(const struct dm_state *state)
{
    uint32_t signature = 2166136261u;
    bool hint_visible = !state->wheel_available && !state->host_pointer &&
        TIME_BEFORE(*rb->current_tick, state->hint_until);

    signature = dm_signature_add(signature, state->app);
    signature = dm_signature_add(signature, state->overlay);
    signature = dm_signature_add(signature, state->fullscreen);
    signature = dm_signature_add(signature, state->running_apps);
    signature = dm_signature_add(signature, state->desktop_selected + 1);
    signature = dm_signature_add(signature, state->app_menu + 1);
    signature = dm_signature_add(signature, state->window_stack_count);
    for (int i = 0; i < state->window_stack_count; i++)
        signature = dm_signature_add(signature, state->window_stack[i]);
    if (state->app > DM_APP_DESKTOP && state->app < DM_APP_COUNT)
    {
        signature = dm_signature_add(signature,
            (unsigned char)state->window_dx[state->app]);
        signature = dm_signature_add(signature,
            (unsigned char)state->window_dy[state->app]);
    }
    signature = dm_signature_add(signature, dm_file_selected + 1);
    signature = dm_signature_add(signature, dm_file_top);
    signature = dm_signature_add(signature, dm_file_count);
    signature = dm_signature_string(signature, dm_cwd);
    signature = dm_signature_add(signature, dm_itunes_source);
    signature = dm_signature_add(signature, dm_itunes_page_top);
    signature = dm_signature_add(signature, dm_itunes_count);
    signature = dm_signature_add(signature, dm_itunes_total);
    signature = dm_signature_add(signature, dm_itunes_database_pending);
    signature = dm_signature_add(signature, dm_itunes_has_more);
    signature = dm_signature_add(signature, dm_itunes_visible_count);
    signature = dm_signature_string(signature, dm_itunes_search);
    signature = dm_signature_add(signature,
                                 rb->global_settings->playlist_shuffle);
    signature = dm_signature_add(signature,
                                 rb->global_settings->repeat_mode);
    signature = dm_signature_add(signature, state->animation.active);
    signature = dm_signature_add(signature, state->animation.last_frame + 1);
    signature = dm_signature_add(signature, state->animation.current.x);
    signature = dm_signature_add(signature, state->animation.current.y);
    signature = dm_signature_add(signature, state->animation.current.width);
    signature = dm_signature_add(signature, state->animation.current.height);
    signature = dm_signature_add(signature, dm_settings.pointer_speed);
    signature = dm_signature_add(signature, dm_settings.reverse_wheel);
    signature = dm_signature_add(signature, dm_settings.show_desktop_folders);
    signature = dm_signature_add(signature, dm_settings.drag_lock);
    signature = dm_signature_add(signature, dm_settings.restore_session);
    signature = dm_signature_add(signature, dm_settings.start_app);
    signature = dm_signature_add(signature, hint_visible);
    return signature;
}

static bool dm_prepare_damage(struct dm_state *state, bool full)
{
    static const struct dm_rect screen = { 0, 0, LCD_WIDTH, LCD_HEIGHT };
    const struct dm_control *hover = NULL;
    struct dm_rect dirty = { 0, 0, 0, 0 };
    struct dm_rect cursor;

    if (state->controls->hover_index >= 0 &&
        state->controls->hover_index < state->controls->count)
        hover = &state->controls->controls[state->controls->hover_index];
    if (!full)
    {
        cursor = (struct dm_rect){ state->presented_cursor_x,
                                  state->presented_cursor_y, 16, 20 };
        dm_rect_union(&dirty, &cursor);
        cursor = (struct dm_rect){ state->cursor_x, state->cursor_y, 16, 20 };
        dm_rect_union(&dirty, &cursor);
        dm_rect_union(&dirty, &state->presented_hover);
        if (hover)
            dm_rect_union(&dirty, &hover->bounds);
        dm_rect_union(&dirty, &state->requested_damage);
        if (state->presented_hover_action == DM_ACTION_DOCK_APP ||
            (hover && hover->action == DM_ACTION_DOCK_APP))
        {
            struct dm_rect dock =
            {
                0, MAX(DM_MENUBAR_H, DM_TOOLTIP_Y - 2), LCD_WIDTH,
                LCD_HEIGHT - MAX(DM_MENUBAR_H, DM_TOOLTIP_Y - 2)
            };

            dm_rect_union(&dirty, &dock);
        }
        if (!dm_rect_intersect(&dirty, &screen) ||
            dirty.width * dirty.height > LCD_WIDTH * LCD_HEIGHT * 2 / 3)
            full = true;
    }
    state->pending_damage = full ? screen : dirty;
    state->requested_damage = (struct dm_rect){ 0, 0, 0, 0 };
    dm_paint_clip = state->pending_damage;
    return full;
}

static bool dm_lcd_present(void *context, const struct desktop_surface *surface,
                           struct desktop_rect dirty)
{
    (void)context;
    rb->lcd_bitmap_part((const fb_data *)surface->pixels,
                        dirty.x, dirty.y, surface->stride,
                        dirty.x, dirty.y, dirty.width, dirty.height);
    rb->lcd_update_rect(dirty.x, dirty.y, dirty.width, dirty.height);
    return true;
}

static void dm_present(struct dm_state *state, bool full)
{
    const struct dm_control *hover = NULL;
    const struct dm_rect *dirty = &state->pending_damage;

    if (state->controls->hover_index >= 0 &&
        state->controls->hover_index < state->controls->count)
        hover = &state->controls->controls[state->controls->hover_index];
    desktop_surface_damage(&dm_surface, (struct desktop_rect)
        {dirty->x, dirty->y, dirty->width, dirty->height});
    desktop_surface_present(&dm_surface);
    if (full) state->full_update_count++;
    else state->partial_update_count++;
    state->presented_cursor_x = state->cursor_x;
    state->presented_cursor_y = state->cursor_y;
    state->presented_hover = hover ? hover->bounds :
                                     (struct dm_rect){ 0, 0, 0, 0 };
    state->presented_hover_action = hover ? hover->action : DM_ACTION_NONE;
}

static bool dm_write_all(int fd, const void *data, size_t size)
{
    const unsigned char *cursor = data;

    while (size > 0)
    {
        ssize_t written = rb->write(fd, cursor, size);

        if (written <= 0)
            return false;
        cursor += written;
        size -= written;
    }
    return true;
}

/* A plugin launch replaces desktop_mode's memory image, so the framebuffer
 * cannot be passed by pointer. Save the exact rendered desktop immediately
 * before launching Live TV; mpegplayer reads it before stream_init() and
 * keeps its copy entirely outside the decoder/audio buffers. */
static bool dm_save_underlay(const char *path, uint32_t magic)
{
    const uint32_t header[] =
    {
        magic,
        LCD_WIDTH,
        LCD_HEIGHT,
    };
    int fd;
    bool ok;

    rb->mkdir(PLUGIN_APPS_DATA_DIR);
    fd = rb->open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;

    ok = dm_write_all(fd, header, sizeof(header)) &&
         dm_write_all(fd, dm_canvas,
                      (size_t)LCD_WIDTH * LCD_HEIGHT * sizeof(fb_data));
    rb->close(fd);
    if (!ok)
        rb->remove(path);
    return ok;
}

static bool dm_save_livetv_underlay(void)
{
    return dm_save_underlay(DM_LIVETV_UNDERLAY_FILE,
                            DM_LIVETV_UNDERLAY_MAGIC);
}

static const char *dm_app_name(enum dm_app app)
{
    switch (app)
    {
        case DM_APP_FINDER:
            return "Finder";
        case DM_APP_ITUNES:
            return "iTunes";
        case DM_APP_PREVIEW:
            return "Preview";
        case DM_APP_TEXTEDIT:
            return "TextEdit";
        case DM_APP_CALCULATOR:
            return "Calculator";
        case DM_APP_DIRECTV:
            return "DIRECTV";
        case DM_APP_SITEKICK:
            return "Sitekick";
        case DM_APP_STEAM:
            return "Steam";
        case DM_APP_NETFLIX:
            return "Netflix";
        case DM_APP_PREFERENCES:
            return "System Preferences";
        case DM_APP_LAUNCHPAD:
            return "Launchpad";
        case DM_APP_DASHBOARD:
            return "Dashboard";
        case DM_APP_TRASH:
            return "Trash";
        default:
            return "Finder";
    }
}

static const char *dm_short_app_name(enum dm_app app)
{
    if (app == DM_APP_PREFERENCES)
        return "Preferences";
    return dm_app_name(app);
}

static enum dm_app dm_menu_owner(const struct dm_state *state)
{
    /* Finder owns the desktop in Snow Leopard, so its menus remain available
     * even when no Finder window is open.  This also makes View > Launchpad
     * reachable from the first screen instead of hiding it behind another
     * application window. */
    return state->app == DM_APP_DESKTOP ? DM_APP_FINDER : state->app;
}

/* The menu bar names the Finder even with no window open, the way Mac OS X
 * does, but the "Start application" setting has to name the desktop itself.
 */
static const char *dm_start_app_name(enum dm_app app)
{
    if (app == DM_APP_DESKTOP)
        return "Desktop";
    return dm_short_app_name(app);
}

static struct dm_rect dm_menu_bar_rect(const struct dm_state *state,
                                       int index)
{
    const struct dm_font *regular = &dm_fonts[DM_FONT_REGULAR];
    const struct dm_font *bold = &dm_fonts[DM_FONT_BOLD];
    const char *name = dm_short_app_name(dm_menu_owner(state));
    int x = DM_APPLE_X + DM_APPLE_W + 4 + dm_text_width(bold, name) + 8;
    int i;

    for (i = 0; i < index; i++)
        x += dm_text_width(regular, dm_app_menu_names[i]) + 8;
    return (struct dm_rect)
    {
        x - 3, 0,
        dm_text_width(regular, dm_app_menu_names[index]) + 6,
        DM_MENUBAR_H
    };
}

static void dm_draw_menu_bar(const struct dm_state *state)
{
    static const char * const weekdays[] =
        { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    const struct dm_font *regular = &dm_fonts[DM_FONT_REGULAR];
    const struct dm_font *bold = &dm_fonts[DM_FONT_BOLD];
    char clock[32];
    struct tm *now = rb->get_time();
    const char *name = dm_short_app_name(dm_menu_owner(state));
    int hour = now ? now->tm_hour : 0;
    int x;
    int i;

    dm_blit(DM_ASSET_MENUBAR, 0, 0);
    if (state->overlay == DM_OVERLAY_APPLE_MENU)
        dm_blit(DM_ASSET_APPLE_HIGHLIGHT, DM_APPLE_X, 0);
    x = DM_APPLE_X + DM_APPLE_W + 4;
    dm_draw_text_in_box(bold, x, 0, 96, DM_MENUBAR_H, DM_INK, name);
    x += dm_text_width(bold, name) + 8;
    for (i = 0; i < DM_APP_MENU_COUNT; i++)
    {
        struct dm_rect item = dm_menu_bar_rect(state, i);

        if (item.x + item.width > 210)
            break;
        if (state->overlay == DM_OVERLAY_APP_MENU &&
            state->app_menu == i)
            dm_fill_rect(item, LCD_RGBPACK(66, 116, 194));
        dm_draw_text_in_box(regular, item.x + 3, 0, item.width - 6,
                            DM_MENUBAR_H,
                            state->overlay == DM_OVERLAY_APP_MENU &&
                            state->app_menu == i ? DM_INK_WHITE : DM_INK,
                            dm_app_menu_names[i]);
    }
    rb->snprintf(clock, sizeof(clock), "%s %d:%02d %s",
                 now && now->tm_wday >= 0 && now->tm_wday < 7 ?
                    weekdays[now->tm_wday] : "Sun",
                 hour % 12 ? hour % 12 : 12,
                 now ? now->tm_min : 0,
                 hour >= 12 ? "PM" : "AM");
    dm_draw_text_right_in_box(regular, LCD_WIDTH - 6, 0, DM_MENUBAR_H, 96,
                              DM_INK, clock);
}

static void dm_draw_launchpad_shortcut(const struct dm_state *state)
{
    static const enum dm_asset_id icons[] =
    {
        DM_ASSET_ICON_FINDER,
        DM_ASSET_ICON_ITUNES,
        DM_ASSET_ICON_PREVIEW,
        DM_ASSET_ICON_TEXTEDIT,
    };
    int x = DM_DESKTOP_LAUNCHPAD_X;
    int y = DM_DESKTOP_LAUNCHPAD_Y;
    int tile = 9;
    int i;

    /* There is no Launchpad icon in the Snow Leopard source.  Use a compact
     * app-grid made only from the real captured Apple application icons, so
     * the shortcut is visibly distinct from Dashboard without inventing a
     * painted glyph or reusing Dashboard's icon. */
    for (i = 0; i < (int)ARRAYLEN(icons); i++)
        dm_compose_scaled(icons[i], x + 1 + (i % 2) * 9,
                          y + (i / 2) * 9, tile, tile);
    dm_draw_desktop_label(x, y + DM_DESKTOP_LAUNCHPAD_ICON + 2,
                          DM_DESKTOP_LAUNCHPAD_W, "Launchpad",
                          state->desktop_selected == 3);
}

static void dm_draw_desktop_icons(const struct dm_state *state)
{
    const int label_width = DM_DESKTOP_LABEL_W;
    const int label_x = DM_DESKTOP_LABEL_X;
    const int label_gap = 2;

    if (dm_settings.show_desktop_folders)
    {
        dm_compose_scaled(DM_ASSET_ICON_DISK, DM_DESKTOP_ICON_X,
                          DM_DESKTOP_DISK_Y, DM_DESKTOP_ICON, DM_DESKTOP_ICON);
        dm_draw_desktop_label(label_x,
                              DM_DESKTOP_DISK_Y + DM_DESKTOP_ICON + label_gap,
                              label_width, "iPod",
                              state->desktop_selected == 0);
        dm_compose_scaled(DM_ASSET_ICON_FOLDER_DESKTOP, DM_DESKTOP_ICON_X,
                          DM_DESKTOP_DOCS_Y, DM_DESKTOP_ICON, DM_DESKTOP_ICON);
        dm_draw_desktop_label(label_x,
                              DM_DESKTOP_DOCS_Y + DM_DESKTOP_ICON + label_gap,
                              label_width, "Documents",
                              state->desktop_selected == 1);
        dm_compose_scaled(DM_ASSET_ICON_FOLDER_DESKTOP, DM_DESKTOP_ICON_X,
                          DM_DESKTOP_MUSIC_Y, DM_DESKTOP_ICON, DM_DESKTOP_ICON);
        dm_draw_desktop_label(label_x,
                              DM_DESKTOP_MUSIC_Y + DM_DESKTOP_ICON + label_gap,
                              label_width, "Music",
                              state->desktop_selected == 2);
    }
    dm_draw_launchpad_shortcut(state);
}

static struct dm_rect dm_dock_rect(int index)
{
    struct dm_rect rect =
    {
        DM_DOCK_LEFT + index * DM_DOCK_STEP,
        DM_DOCK_ICON_Y,
        DM_DOCK_ICON,
        DM_DOCK_ICON
    };
    return rect;
}

static int dm_dock_index(enum dm_app app)
{
    int i;

    for (i = 0; i < DM_DOCK_SLOTS; i++)
    {
        if (dm_dock_apps[i] == app)
            return i;
    }
    return -1;
}

static bool dm_app_has_window(enum dm_app app)
{
    return app == DM_APP_FINDER || app == DM_APP_ITUNES ||
           app == DM_APP_PREVIEW || app == DM_APP_TEXTEDIT ||
           app == DM_APP_CALCULATOR || app == DM_APP_PREFERENCES ||
           app == DM_APP_LAUNCHPAD;
}

static void dm_sync_window_geometry(const struct dm_state *state,
                                    enum dm_app app)
{
    dm_window_dx = 0;
    dm_window_dy = 0;
    if (!state->fullscreen && app > DM_APP_DESKTOP && app < DM_APP_COUNT)
    {
        dm_window_dx = state->window_dx[app];
        dm_window_dy = state->window_dy[app];
    }
}

static void dm_stack_remove(struct dm_state *state, enum dm_app app)
{
    int keep = 0;
    int i;

    for (i = 0; i < state->window_stack_count; i++)
    {
        if (state->window_stack[i] != app)
            state->window_stack[keep++] = state->window_stack[i];
    }
    state->window_stack_count = keep;
}

static void dm_stack_raise(struct dm_state *state, enum dm_app app)
{
    int i;

    if (!dm_app_has_window(app))
        return;
    dm_stack_remove(state, app);
    if (state->window_stack_count == DM_VISIBLE_WINDOWS)
    {
        for (i = 1; i < state->window_stack_count; i++)
            state->window_stack[i - 1] = state->window_stack[i];
        state->window_stack_count--;
    }
    state->window_stack[state->window_stack_count++] = app;
}

/* Closing the front window exposes the most recently raised survivor.  This
 * keeps the small in-plugin focus stack useful on the host profile and makes
 * the TV-out shell feel like a desktop instead of a collection of unrelated
 * launches.  The survivor already owns its cached application state, so no
 * filesystem or tagcache work is needed here. */
static void dm_close_active_window(struct dm_state *state)
{
    enum dm_app closing = state->app;

    dm_stack_remove(state, closing);
    if (closing != DM_APP_FINDER)
        state->running_apps &= ~DM_APP_BIT(closing);
    state->fullscreen = false;
    dm_fullscreen = false;
    if (state->window_stack_count > 0)
    {
        state->app = state->window_stack[state->window_stack_count - 1];
        dm_stack_raise(state, state->app);
    }
    else
        state->app = DM_APP_DESKTOP;
}

static enum dm_asset_id dm_window_asset(enum dm_app app)
{
    if (app == DM_APP_FINDER)
        return DM_ASSET_WINDOW_SIDEBAR;
    if (app == DM_APP_ITUNES)
        return DM_ASSET_ITUNES_WINDOW;
    return DM_ASSET_WINDOW_PLAIN;
}

static struct dm_rect dm_app_window_rect(enum dm_app app)
{
    if (app == DM_APP_ITUNES)
        return (struct dm_rect)
        {
            DM_ITUNES_X,
            DM_ITUNES_Y,
            DM_ITUNES_W,
            DM_ITUNES_H
        };
    return (struct dm_rect){ DM_WIN_X, DM_WIN_Y, DM_WIN_W, DM_WIN_H };
}

/* Launchpad is an in-window application list.  The native body has only 97
 * rows, so derive one item rectangle from the actual window body and use it
 * for both painting and hit-testing.  This also keeps the host profile and a
 * fullscreen window bounded without a second layout table. */
static struct dm_rect dm_launchpad_item_rect(int index)
{
    struct dm_rect window = dm_app_window_rect(DM_APP_LAUNCHPAD);
    int rows = ((int)ARRAYLEN(dm_launchpad_apps) + DM_LAUNCHPAD_COLUMNS - 1) /
               DM_LAUNCHPAD_COLUMNS;
    int body_y = window.y + DM_WIN_TITLE_H + DM_WIN_TOOLBAR_H;
    int body_height = window.height - DM_WIN_TITLE_H - DM_WIN_TOOLBAR_H -
                      DM_WIN_STATUS_H;
    int content_x = window.x + 4;
    int content_width = window.width - 8;
    int cell_width = content_width / DM_LAUNCHPAD_COLUMNS;
    int row_height = MAX(1, body_height / MAX(1, rows));
    int column = index % DM_LAUNCHPAD_COLUMNS;
    int row = index / DM_LAUNCHPAD_COLUMNS;

    return (struct dm_rect)
    {
        content_x + column * cell_width,
        body_y + 1 + row * row_height,
        column == DM_LAUNCHPAD_COLUMNS - 1 ?
            window.x + window.width - 4 -
                (content_x + column * cell_width) : cell_width,
        row_height
    };
}

static int dm_launchpad_icon_size(void)
{
    struct dm_rect item = dm_launchpad_item_rect(0);
    int size = item.height - dm_fonts[DM_FONT_SMALL].cell_h - 2;

    return MAX(1, MIN(DM_DOCK_ICON, size));
}

static struct dm_rect dm_app_title_button_rect(enum dm_app app,
                                               struct dm_rect window,
                                               int source_x)
{
    int source_width = app == DM_APP_ITUNES ? DM_ITUNES_CHROME_W : DM_CHROME_W;
    int source_height = app == DM_APP_ITUNES ? DM_ITUNES_ASSET_H : DM_CHROME_H;
    int x = window.x + source_x * window.width / source_width;
    int y = window.y + 5 * window.height / source_height;
    int width = MAX(1, 16 * window.width / source_width);
    int height = MAX(1, 15 * window.height / source_height);

    return (struct dm_rect)
    {
        MAX(window.x, x - 2),
        MAX(window.y, y - 2),
        MIN(window.x + window.width, x + width + 2) - MAX(window.x, x - 2),
        MIN(window.y + window.height, y + height + 2) - MAX(window.y, y - 2)
    };
}

static struct dm_rect dm_app_window_title_drag_rect(enum dm_app app)
{
    struct dm_rect window = dm_app_window_rect(app);

    return (struct dm_rect)
    {
        window.x + 72, window.y + 2,
        MAX(1, window.width - 80), DM_WIN_TITLE_H - 4
    };
}

/* Minimise and restore scale the window's own chrome straight into the
 * compose buffer, so no second full-size scratch bitmap is needed.
 */
static void dm_prepare_animation_frame(struct dm_state *state, int frame)
{
    struct dm_animation *animation = &state->animation;

    animation->current.x =
        animation->from.x +
        (animation->to.x - animation->from.x) * frame /
            (DM_ANIMATION_FRAMES - 1);
    animation->current.y =
        animation->from.y +
        (animation->to.y - animation->from.y) * frame /
            (DM_ANIMATION_FRAMES - 1);
    animation->current.width =
        animation->from.width +
        (animation->to.width - animation->from.width) * frame /
            (DM_ANIMATION_FRAMES - 1);
    animation->current.height =
        animation->from.height +
        (animation->to.height - animation->from.height) * frame /
            (DM_ANIMATION_FRAMES - 1);
    animation->last_frame = frame;
}

static void dm_finish_animation(struct dm_state *state)
{
    struct dm_animation *animation = &state->animation;

    if (!animation->active)
        return;
    if (animation->minimizing)
    {
        state->minimized_app = animation->app;
        state->app = DM_APP_DESKTOP;
    }
    else
    {
        state->minimized_app = DM_APP_DESKTOP;
        state->app = animation->app;
        dm_stack_raise(state, animation->app);
    }
    animation->active = false;
    state->redraw = true;
}

static void dm_start_animation(struct dm_state *state, enum dm_app app,
                               bool minimizing)
{
    int index = dm_dock_index(app);
    struct dm_rect window;
    struct dm_rect icon;

    dm_sync_window_geometry(state, app);
    window = dm_app_window_rect(app);
    if (minimizing)
        dm_stack_remove(state, app);
    if (index < 0)
    {
        state->app = minimizing ? DM_APP_DESKTOP : app;
        state->redraw = true;
        return;
    }
    icon = dm_dock_rect(index);
    state->animation.active = true;
    state->animation.minimizing = minimizing;
    state->animation.app = app;
    state->animation.started = *rb->current_tick;
    state->animation.last_frame = -1;
    state->animation.from = minimizing ? window : icon;
    state->animation.to = minimizing ? icon : window;
    dm_prepare_animation_frame(state, 0);
    state->redraw = true;
}

static bool dm_update_animation(struct dm_state *state)
{
    struct dm_animation *animation = &state->animation;
    long elapsed;
    int frame;

    if (!animation->active)
        return false;
    elapsed = *rb->current_tick - animation->started;
    frame = MIN(
        DM_ANIMATION_FRAMES - 1,
        (int)(elapsed * DM_ANIMATION_FRAMES /
              MAX(1, DM_ANIMATION_TICKS)));
    if (frame >= DM_ANIMATION_FRAMES - 1)
    {
        dm_finish_animation(state);
        return true;
    }
    if (frame != animation->last_frame)
    {
        dm_prepare_animation_frame(state, frame);
        state->redraw = true;
        return true;
    }
    return false;
}

static void dm_draw_animation(const struct dm_state *state)
{
    const struct dm_animation *animation = &state->animation;
    const struct dm_asset *asset = &dm_assets[dm_window_asset(animation->app)];
    int width = animation->current.width;
    int height = animation->current.height;
    int row;
    int column;

    if (!asset->loaded || width <= 0 || height <= 0)
        return;
    for (row = 0; row < height; row++)
    {
        int target_y = animation->current.y + row;
        const fb_data *source;
        fb_data *target;

        if (target_y < dm_paint_clip.y ||
            target_y >= dm_paint_clip.y + dm_paint_clip.height)
            continue;
        source = asset->pixels + (row * asset->height / height) * asset->width;
        target = dm_canvas + target_y * LCD_WIDTH;
        for (column = 0; column < width; column++)
        {
            int target_x = animation->current.x + column;

            if (target_x < dm_paint_clip.x ||
                target_x >= dm_paint_clip.x + dm_paint_clip.width)
                continue;
            target[target_x] = source[column * asset->width / width];
        }
    }
}

/* The legacy BMP contains Aurora outside the captured shelf's sloping
 * end caps. Keep the measured 34/52 source contour, but leave its exterior
 * transparent. The clean middle column supplies unmatted edge pixels;
 * blending a captured edge would carry purple Aurora into any new wallpaper.
 * This paints cached pixels only and needs no mask or extra framebuffer. */
static void dm_draw_dock_shelf(void)
{
    const struct dm_asset *asset = &dm_assets[DM_ASSET_DOCK_SHELF];
    if (!asset->loaded || asset->height < 2) return;
    for (int row = 0; row < asset->height; row++)
    {
        int y = DM_DOCK_SHELF_Y + row;
        if (y < dm_paint_clip.y ||
            y >= dm_paint_clip.y + dm_paint_clip.height) continue;
        const fb_data *source = asset->pixels + row * asset->width;
        fb_data *target = dm_canvas + y * LCD_WIDTH;
        int inset = (256 + 16 * 256 * (asset->height - 1 - row) /
                     (asset->height - 1)) * asset->height / 26;
        for (int column = 0; column < asset->width; column++)
        {
            int x = DM_DOCK_SHELF_X + column;
            if (x < dm_paint_clip.x ||
                x >= dm_paint_clip.x + dm_paint_clip.width) continue;
            int distance = MIN(column, asset->width - 1 - column) * 256;
            int alpha = MIN(255, distance - inset + 256);
            if (alpha <= 0) continue;
            fb_data pixel = distance < inset + 512 ?
                            source[asset->width / 2] : source[column];
            target[x] = alpha == 255 ? pixel : dm_blend(target[x], pixel, alpha);
        }
    }
}

static void dm_draw_dock(const struct dm_state *state)
{
    int i;

    dm_draw_dock_shelf();
    for (i = 0; i < DM_DOCK_SLOTS; i++)
    {
        struct dm_rect rect = dm_dock_rect(i);
        enum dm_asset_id asset = dm_dock_assets[i];
        int x = rect.x;
        int y = rect.y;

        if (state->hover_dock == i && state->dock_stage >= 2)
        {
            asset = dm_dock_assets_38[i];
            x -= 3;
            y -= 6;
        }
        else if (state->hover_dock == i && state->dock_stage >= 1)
        {
            asset = dm_dock_assets_34[i];
            x -= 1;
            y -= 2;
        }
        else if (state->dock_stage >= 2 && state->hover_dock >= 0 &&
                 DM_ABS(state->hover_dock - i) == 1)
        {
            asset = dm_dock_assets_34[i];
            x -= 1;
            y -= 2;
        }
        dm_compose_dock_reflection(asset, x, y);
        dm_compose(asset, x, y);
        if ((state->running_apps & DM_APP_BIT(dm_dock_apps[i])) != 0)
            dm_compose(DM_ASSET_DOCK_INDICATOR, rect.x + 9,
                       DM_DOCK_ICON_Y + DM_DOCK_ICON + 2);
    }
    if (state->hover_dock >= 0 && state->dock_stage >= 3)
    {
        const char *label = dm_dock_labels[state->hover_dock];
        int width = MIN(DM_TOOLTIP_W,
                        dm_text_width(&dm_fonts[DM_FONT_SMALL], label) + 16);
        int x = dm_dock_rect(state->hover_dock).x + DM_DOCK_ICON / 2 -
                width / 2;

        x = MAX(2, MIN(x, LCD_WIDTH - width - 2));
        dm_blit_panel(DM_ASSET_TOOLTIP, x, DM_TOOLTIP_Y, width,
                      DM_TOOLTIP_CAP);
        dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_SMALL], x, DM_TOOLTIP_Y,
                                     width, DM_TOOLTIP_H, DM_INK, label);
    }
}

static enum dm_asset_id dm_app_icon_asset(enum dm_app app)
{
    switch (app)
    {
        case DM_APP_FINDER:
            return DM_ASSET_ICON_FINDER;
        case DM_APP_ITUNES:
            return DM_ASSET_ICON_ITUNES;
        case DM_APP_PREVIEW:
            return DM_ASSET_ICON_PREVIEW;
        case DM_APP_TEXTEDIT:
            return DM_ASSET_ICON_TEXTEDIT;
        case DM_APP_CALCULATOR:
            return DM_ASSET_ICON_CALCULATOR;
        case DM_APP_DIRECTV:
            return DM_ASSET_ICON_DIRECTV;
        case DM_APP_SITEKICK:
            return DM_ASSET_ICON_SITEKICK;
        case DM_APP_STEAM:
            return DM_ASSET_ICON_STEAM;
        case DM_APP_NETFLIX:
            return DM_ASSET_ICON_NETFLIX;
        case DM_APP_PREFERENCES:
            return DM_ASSET_ICON_PREFERENCES;
        case DM_APP_TRASH:
            return DM_ASSET_ICON_TRASH_EMPTY;
        default:
            return DM_ASSET_ICON_DASHBOARD;
    }
}

static void dm_draw_launchpad(const struct dm_state *state)
{
    const struct dm_font *small = &dm_fonts[DM_FONT_SMALL];
    int icon_size = dm_launchpad_icon_size();
    int i;

    dm_blit_window(DM_ASSET_WINDOW_PLAIN);
    dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_BOLD], DM_WIN_X, DM_WIN_Y,
                                 DM_WIN_W, DM_WIN_TITLE_H, DM_INK_TITLE,
                                 "Launchpad");
    for (i = 0; i < (int)ARRAYLEN(dm_launchpad_apps); i++)
    {
        enum dm_app app = dm_launchpad_apps[i];
        struct dm_rect item = dm_launchpad_item_rect(i);
        const char *name = dm_short_app_name(app);
        int x = item.x + (item.width - icon_size) / 2;
        int label_width = MIN(item.width - 4, dm_text_width(small, name));
        int label_x = x + icon_size / 2 - label_width / 2;

        /* Anchor the label to the icon centre, then clamp its readable box
         * to the same tile.  Centering the full string inside the tile made
         * a clipped label appear left-biased whenever its measured width was
         * wider than the compact native column. */
        label_x = MAX(item.x + 2,
                      MIN(label_x, item.x + item.width - 2 - label_width));

        if (state->hover_launchpad == i)
        {
            dm_fill_rect((struct dm_rect)
            {
                label_x - 2,
                item.y + icon_size,
                label_width + 4, small->cell_h + 2
            }, LCD_RGBPACK(56, 117, 215));
        }
        dm_compose_scaled(dm_app_icon_asset(app), x, item.y,
                          icon_size, icon_size);
        dm_draw_text_in_box(small, label_x, item.y + icon_size,
                            label_width, small->cell_h + 2,
                            state->hover_launchpad == i ?
                            DM_INK_WHITE : DM_INK, name);
    }
}

/* Sidebar rows, measured against the real source-list metrics. */
struct dm_sidebar_row
{
    const char *label;
    const char *path;
    short y;
    short height;
    bool header;
};

static const struct dm_sidebar_row dm_sidebar_rows[] =
{
    { "DEVICES", NULL, DM_NORMAL_BODY_BASE_Y + 2, 14, true },
    { "iPod", "/", DM_NORMAL_BODY_BASE_Y + 16, 16, false },
    { "PLACES", NULL, DM_NORMAL_BODY_BASE_Y + 34, 14, true },
    { "Applications", PLUGIN_APPS_DIR, DM_NORMAL_BODY_BASE_Y + 48, 16, false },
    { "Music", "/Music", DM_NORMAL_BODY_BASE_Y + 64, 16, false },
    { "Videos", "/Videos", DM_NORMAL_BODY_BASE_Y + 80, 16, false },
};

static int dm_sidebar_row_y(const struct dm_sidebar_row *row)
{
    return DM_BODY_Y + row->y - DM_NORMAL_BODY_BASE_Y;
}

/* Finder titles the window after the folder, not the whole path. */
static const char *dm_window_title(void)
{
    const char *slash = rb->strrchr(dm_cwd, '/');

    if (dm_wallpaper_picker) return "Choose Wallpaper";
    if (!slash || !slash[1])
        return "iPod";
    return slash + 1;
}

static void dm_draw_finder(const struct dm_state *state)
{
    const struct dm_font *regular = &dm_fonts[DM_FONT_REGULAR];
    const struct dm_font *small = &dm_fonts[DM_FONT_SMALL];
    char status[48];
    int i;

    dm_blit_window(DM_ASSET_WINDOW_SIDEBAR);
    dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_BOLD], DM_WIN_X, DM_WIN_Y,
                                 DM_WIN_W, DM_WIN_TITLE_H, DM_INK_TITLE,
                                 dm_window_title());

    for (i = 0; !dm_wallpaper_picker && i < (int)ARRAYLEN(dm_sidebar_rows); i++)
    {
        const struct dm_sidebar_row *row = &dm_sidebar_rows[i];
        int y = dm_sidebar_row_y(row);

        if (row->header)
        {
            dm_draw_text_in_box(small, DM_WIN_X + 6, y, 74, row->height,
                                DM_INK_SIDEBAR_HEAD, row->label);
            continue;
        }
        if (!rb->strcmp(dm_cwd, row->path))
        {
            dm_blit(DM_ASSET_SIDEBAR_SELECTION, DM_WIN_X, y);
            dm_draw_text_in_box(small, DM_WIN_X + 18, y, 62, row->height,
                                DM_INK_WHITE, row->label);
        }
        else
            dm_draw_text_in_box(small, DM_WIN_X + 18, y, 62, row->height,
                                DM_INK_SIDEBAR, row->label);
    }

    for (i = 0; i < DM_FILE_ROWS; i++)
    {
        int index = dm_file_top + i;
        int y = DM_BODY_Y + i * DM_ROW_H;
        fb_data ink = DM_INK;

        if (index >= dm_file_count)
            break;
        if (index == dm_file_selected)
        {
            dm_blit(DM_ASSET_LIST_SELECTION, DM_LIST_X, y);
            ink = DM_INK_WHITE;
        }
        dm_compose(dm_files[index].is_dir ? DM_ASSET_ICON_FOLDER :
                                            DM_ASSET_ICON_DOCUMENT,
                   DM_LIST_X + 4, y + 2);
        dm_draw_text_in_box(regular, DM_LIST_X + 24, y,
                            DM_LIST_W - 30 -
                                (dm_file_count > DM_FILE_ROWS ?
                                 DM_SCROLLER_W : 0),
                            DM_ROW_H, ink, dm_files[index].name);
    }
    if (dm_file_count > DM_FILE_ROWS)
    {
        int maximum = dm_file_count - DM_FILE_ROWS;
        int travel = DM_SCROLLER_H - DM_SCROLLER_THUMB_H;

        dm_blit(DM_ASSET_SCROLLER_TRACK, DM_SCROLLER_X, DM_BODY_Y);
        dm_blit(DM_ASSET_SCROLLER_THUMB, DM_SCROLLER_X,
                DM_BODY_Y + dm_file_top * travel / maximum);
    }
    rb->snprintf(status, sizeof(status), "%d item%s", dm_file_count,
                 dm_file_count == 1 ? "" : "s");
    dm_draw_text_centered_in_box(small, DM_WIN_X, DM_STATUS_Y,
                                 DM_WIN_W, DM_WIN_STATUS_H, DM_INK_STATUS,
                                 dm_wallpaper_picker ? "Enter: choose   Menu: up / cancel" : status);
    (void)state;
}

static void dm_scan_directory(void);

/* Photos and iTunes are windows over the same bounded snapshot Finder uses,
 * so neither needs its own scan, list or paint code.
 */
enum dm_media_kind
{
    DM_MEDIA_PICTURES = 0,
    DM_MEDIA_MUSIC,
};

static enum dm_media_kind dm_media_kind;
static char dm_media_root[MAX_PATH];

static bool dm_name_has_suffix(const char *name, const char *suffix)
{
    int name_length = rb->strlen(name);
    int suffix_length = rb->strlen(suffix);

    return name_length > suffix_length &&
           !rb->strcasecmp(name + name_length - suffix_length, suffix);
}

static bool dm_media_match(enum dm_media_kind kind, const char *name)
{
    static const char * const pictures[] =
        { ".bmp", ".jpg", ".jpeg", ".png", ".gif", ".ppm" };
    const char * const *suffixes = pictures;
    int count = (int)ARRAYLEN(pictures);
    int i;

    if (kind == DM_MEDIA_MUSIC)
        return (rb->filetype_get_attr(name) & FILE_ATTR_MASK) ==
               FILE_ATTR_AUDIO;

    for (i = 0; i < count; i++)
    {
        if (dm_name_has_suffix(name, suffixes[i]))
            return true;
    }
    return false;
}

static void dm_scan_media(enum dm_media_kind kind)
{
    static const char * const roots[] = { "/Pictures", "/Music" };
    const char *root = roots[kind];
    int keep = 0;
    int i;

    dm_media_kind = kind;
    if (!rb->dir_exists(root))
        root = "/";
    rb->strlcpy(dm_media_root, root, sizeof(dm_media_root));
    rb->strlcpy(dm_cwd, root, sizeof(dm_cwd));
    dm_scan_directory();
    /* Keep folders so the window can be browsed, and only the media this
     * application can actually open. */
    for (i = 0; i < dm_file_count; i++)
    {
        if (dm_files[i].is_dir || dm_media_match(kind, dm_files[i].name))
        {
            if (keep != i)
                dm_files[keep] = dm_files[i];
            keep++;
        }
    }
    dm_file_count = keep;
    dm_file_selected = 0;
    dm_file_top = 0;
}

/* Shared list body for the windowed media applications. */
static void dm_draw_media_window(const char *title, enum dm_asset_id icon,
                                 const char *status_text)
{
    const struct dm_font *regular = &dm_fonts[DM_FONT_REGULAR];
    const struct dm_font *small = &dm_fonts[DM_FONT_SMALL];
    char status[48];
    int i;

    dm_blit_window(DM_ASSET_WINDOW_PLAIN);
    dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_BOLD], DM_WIN_X, DM_WIN_Y,
                                 DM_WIN_W, DM_WIN_TITLE_H, DM_INK_TITLE,
                                 title);
    for (i = 0; i < DM_FILE_ROWS; i++)
    {
        int index = dm_file_top + i;
        int y = DM_BODY_Y + i * DM_ROW_H;
        fb_data ink = DM_INK;

        if (index >= dm_file_count)
            break;
        if (index == dm_file_selected)
        {
            dm_blit_part(DM_ASSET_LIST_SELECTION, DM_WIN_X + 1, y,
                         DM_WIN_W - 2);
            ink = DM_INK_WHITE;
        }
        dm_compose(dm_files[index].is_dir ? DM_ASSET_ICON_FOLDER : icon,
                   DM_WIN_X + 8, y + 2);
        dm_draw_text_in_box(regular, DM_WIN_X + 28, y, DM_WIN_W - 40,
                            DM_ROW_H, ink, dm_files[index].name);
    }
    if (dm_file_count > DM_FILE_ROWS)
    {
        int maximum = dm_file_count - DM_FILE_ROWS;
        int travel = DM_SCROLLER_H - DM_SCROLLER_THUMB_H;

        dm_blit(DM_ASSET_SCROLLER_TRACK, DM_SCROLLER_X, DM_BODY_Y);
        dm_blit(DM_ASSET_SCROLLER_THUMB, DM_SCROLLER_X,
                DM_BODY_Y + dm_file_top * travel / maximum);
    }
    if (dm_file_count == 0)
        dm_draw_text_centered_in_box(regular, DM_WIN_X, DM_BODY_Y + 24,
                                     DM_WIN_W, 26, DM_INK_STATUS,
                                     "Nothing here");
    if (!status_text)
    {
        rb->snprintf(status, sizeof(status), "%d item%s", dm_file_count,
                     dm_file_count == 1 ? "" : "s");
        status_text = status;
    }
    dm_draw_text_centered_in_box(small, DM_WIN_X, DM_STATUS_Y,
                                 DM_WIN_W, DM_WIN_STATUS_H, DM_INK_STATUS,
                                 status_text);
}

static void dm_draw_photos(void)
{
    dm_draw_media_window("Photos", DM_ASSET_ICON_PREVIEW, NULL);
}

static void dm_draw_textedit(void)
{
    const struct dm_font *regular = &dm_fonts[DM_FONT_REGULAR];
    const struct dm_font *small = &dm_fonts[DM_FONT_SMALL];
    const char *cursor = dm_textedit_note;
    int y = DM_BODY_Y + 6;
    int row;

    dm_blit_window(DM_ASSET_WINDOW_PLAIN);
    dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_BOLD], DM_WIN_X, DM_WIN_Y,
                                 DM_WIN_W, DM_WIN_TITLE_H, DM_INK_TITLE,
                                 "TextEdit");
    dm_fill_rect((struct dm_rect){ DM_WIN_X + 8, DM_BODY_Y + 3,
                                  DM_WIN_W - 16, DM_BODY_H - 6 },
                 LCD_RGBPACK(255, 255, 255));
    for (row = 0; row < 6 && *cursor; row++)
    {
        char line[48];
        int length = 0;

        while (cursor[length] && cursor[length] != '\n' &&
               length < (int)sizeof(line) - 1)
            length++;
        rb->memcpy(line, cursor, length);
        line[length] = '\0';
        dm_draw_text_in_box(regular, DM_WIN_X + 13, y, DM_WIN_W - 26,
                            regular->cell_h, DM_INK, line);
        cursor += length;
        if (*cursor == '\n')
            cursor++;
        y += regular->cell_h + 2;
    }
    dm_draw_text_centered_in_box(small, DM_WIN_X, DM_STATUS_Y,
                                 DM_WIN_W, DM_WIN_STATUS_H, DM_INK_STATUS,
                                 "Click the page to edit");
}

#define DM_CALCULATOR_COLUMNS 4
#define DM_CALCULATOR_ROWS 4

static struct dm_rect dm_calculator_key_rect(int index)
{
    int keypad_y = DM_BODY_Y + 27;
    int keypad_h = MAX(DM_CALCULATOR_ROWS, DM_BODY_H - 29);
    int column = index % DM_CALCULATOR_COLUMNS;
    int row = index / DM_CALCULATOR_COLUMNS;
    int left = DM_WIN_X + 8;
    int width = DM_WIN_W - 16;

    return (struct dm_rect)
    {
        left + column * width / DM_CALCULATOR_COLUMNS,
        keypad_y + row * keypad_h / DM_CALCULATOR_ROWS,
        (column + 1) * width / DM_CALCULATOR_COLUMNS -
            column * width / DM_CALCULATOR_COLUMNS - 1,
        (row + 1) * keypad_h / DM_CALCULATOR_ROWS -
            row * keypad_h / DM_CALCULATOR_ROWS - 1
    };
}

static void dm_draw_calculator(void)
{
    static const char keys[] = "789/456*123-0C=+";
    char value[24];
    int i;

    dm_blit_window(DM_ASSET_WINDOW_PLAIN);
    dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_BOLD], DM_WIN_X, DM_WIN_Y,
                                 DM_WIN_W, DM_WIN_TITLE_H, DM_INK_TITLE,
                                 "Calculator");
    dm_fill_rect((struct dm_rect){ DM_WIN_X + 8, DM_BODY_Y + 3,
                                  DM_WIN_W - 16, 20 },
                 LCD_RGBPACK(45, 48, 52));
    rb->snprintf(value, sizeof(value), "%ld", dm_calculator_value);
    dm_draw_text_right_in_box(&dm_fonts[DM_FONT_BOLD],
                              DM_WIN_X + DM_WIN_W - 14, DM_BODY_Y + 3, 20,
                              DM_WIN_W - 28, DM_INK_WHITE, value);
    for (i = 0; i < (int)sizeof(keys) - 1; i++)
    {
        struct dm_rect key = dm_calculator_key_rect(i);
        char label[2] = { keys[i], '\0' };
        fb_data colour = keys[i] < '0' || keys[i] > '9' ?
            LCD_RGBPACK(218, 221, 226) : LCD_RGBPACK(242, 243, 245);

        dm_fill_rect(key, colour);
        dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_REGULAR], key.x,
                                     key.y, key.width, key.height, DM_INK,
                                     label);
    }
}

#if LCD_WIDTH >= 1920
#define DM_ITUNES_BODY_LOCAL_Y (22 + 41 + 17)
#define DM_ITUNES_BOTTOM_H 24
#define DM_ITUNES_ROW_H 20
#else
/* Native iTunes keeps the captured title and transport controls, then
 * reflows the captured library pane eight pixels upward. The compact strip
 * leaves room for five 15-pixel music rows without changing the real Aqua
 * furniture or the 24-pixel footer. */
#define DM_ITUNES_BODY_LOCAL_Y 72
#define DM_ITUNES_BOTTOM_H 24
#define DM_ITUNES_ROW_H (LCD_WIDTH >= 640 ? 20 : 15)
#define DM_ITUNES_NOW_PANEL_H 34
#endif
#define DM_ITUNES_BODY_Y (DM_ITUNES_Y + DM_ITUNES_BODY_LOCAL_Y)
#define DM_ITUNES_ROWS ((DM_ITUNES_H - DM_ITUNES_BODY_LOCAL_Y - \
                         DM_ITUNES_BOTTOM_H) / DM_ITUNES_ROW_H)
#if LCD_WIDTH >= 1920
#define DM_ITUNES_LIST_X (DM_ITUNES_X + DM_ITUNES_SOURCE_W + 1)
#define DM_ITUNES_LIST_W (DM_ITUNES_W - DM_ITUNES_SOURCE_W - 2)
#else
#define DM_ITUNES_SOURCE_ROW_H 13
#define DM_ITUNES_HEADER_H 16
#define DM_ITUNES_SCROLLER_W 15
#define DM_ITUNES_SCROLLER_MIN_THUMB_H (DM_ITUNES_SCROLLER_W + 3)
#define DM_ITUNES_ALBUM_COLUMNS 2
#define DM_ITUNES_ALBUM_ROWS (LCD_WIDTH == 640 ? 6 : 2)
#define DM_ITUNES_ALBUM_TILES \
    (DM_ITUNES_ALBUM_COLUMNS * DM_ITUNES_ALBUM_ROWS)
#define DM_ITUNES_SCROLLER_X \
    (DM_ITUNES_X + DM_ITUNES_W - DM_ITUNES_SCROLLER_W - 1)
#define DM_ITUNES_SCROLLER_Y DM_ITUNES_BODY_Y
#define DM_ITUNES_SCROLLER_H \
    (DM_ITUNES_H - DM_ITUNES_BODY_LOCAL_Y - DM_ITUNES_BOTTOM_H)
#define DM_ITUNES_LIST_X (DM_ITUNES_X + DM_ITUNES_SOURCE_W + 1)
#define DM_ITUNES_LIST_W \
    (DM_ITUNES_W - DM_ITUNES_SOURCE_W - DM_ITUNES_SCROLLER_W - 3)
#define DM_ITUNES_NAME_X (DM_ITUNES_LIST_X + 21)
#define DM_ITUNES_ARTIST_X (DM_ITUNES_LIST_X + DM_ITUNES_LIST_W * 3 / 5)
#define DM_ITUNES_NAME_W (DM_ITUNES_ARTIST_X - DM_ITUNES_NAME_X - 4)
#define DM_ITUNES_ARTIST_W \
    (DM_ITUNES_LIST_X + DM_ITUNES_LIST_W - DM_ITUNES_ARTIST_X - 2)
#define DM_ITUNES_ALBUM_CELL_W (DM_ITUNES_LIST_W / DM_ITUNES_ALBUM_COLUMNS)
#define DM_ITUNES_ALBUM_CELL_H (DM_ITUNES_SCROLLER_H / DM_ITUNES_ALBUM_ROWS)
#endif

#if LCD_WIDTH < 1920
/* Albums use the same four cached personal covers as the song list, but give
 * each result a stable tile so the source reads as iTunes' album browser
 * instead of a second text-only table.  The final row/column absorbs any
 * integer-division remainder, keeping every hit target inside the window. */
static struct dm_rect dm_itunes_album_item_rect(int index)
{
    int column = index % DM_ITUNES_ALBUM_COLUMNS;
    int row = index / DM_ITUNES_ALBUM_COLUMNS;
    int x = DM_ITUNES_LIST_X + column * DM_ITUNES_ALBUM_CELL_W;
    int y = DM_ITUNES_BODY_Y + row * DM_ITUNES_ALBUM_CELL_H;
    int right = DM_ITUNES_LIST_X + DM_ITUNES_LIST_W;
    int bottom = DM_ITUNES_BODY_Y + DM_ITUNES_SCROLLER_H;

    return (struct dm_rect)
    {
        x, y,
        column == DM_ITUNES_ALBUM_COLUMNS - 1 ? right - x :
                                                DM_ITUNES_ALBUM_CELL_W,
        row == DM_ITUNES_ALBUM_ROWS - 1 ? bottom - y :
                                          DM_ITUNES_ALBUM_CELL_H
    };
}
#endif

static int dm_itunes_page_capacity(void)
{
#if LCD_WIDTH < 1920
    if (dm_itunes_source == DM_ITUNES_ALBUMS)
        return DM_ITUNES_ALBUM_TILES;
#endif
    return DM_ITUNES_ROWS;
}

static const char * const dm_itunes_source_names[DM_ITUNES_SOURCE_COUNT] =
{
    "Songs", "Albums", "Artists", "Genres", "Movies", "Playlists"
};

static bool dm_itunes_row_matches(const struct dm_itunes_row *row)
{
    if (!dm_itunes_search[0])
        return true;
    return rb->strcasestr(row->title, dm_itunes_search) != NULL ||
           rb->strcasestr(row->artist, dm_itunes_search) != NULL ||
           rb->strcasestr(row->album, dm_itunes_search) != NULL;
}

static void dm_itunes_art_reset(void)
{
    int i;

    for (i = 0; i < DM_ITUNES_ART_SLOTS; i++)
    {
        dm_itunes_art[i].attempted = false;
        dm_itunes_art[i].valid = false;
        dm_itunes_art[i].row = -1;
        dm_itunes_art[i].path[0] = '\0';
    }
    dm_itunes_art_not_before = *rb->current_tick + HZ / 3;
}

static bool dm_itunes_cover_path(const char *track_path, char *path,
                                 size_t path_size)
{
    static const char * const names[] =
    {
        "cover.51x51.bmp",
        "cover.138x138.bmp",
        "cover.bmp",
        "folder.bmp",
        "albumart.bmp",
    };
    char directory[MAX_PATH];
    char *slash;
    int i;

    if (!track_path || !track_path[0])
        return false;
    rb->strlcpy(directory, track_path, sizeof(directory));
    slash = rb->strrchr(directory, '/');
    if (!slash)
        return false;
    *slash = '\0';
    for (i = 0; i < (int)ARRAYLEN(names); i++)
    {
        rb->snprintf(path, path_size, "%s/%s", directory, names[i]);
        if (rb->file_exists(path))
            return true;
    }
    path[0] = '\0';
    return false;
}

static struct dm_itunes_art_slot *dm_itunes_art_for_row(int row)
{
    int i;

    for (i = 0; i < DM_ITUNES_ART_SLOTS; i++)
    {
        if (dm_itunes_art[i].valid && dm_itunes_art[i].row == row)
            return &dm_itunes_art[i];
    }
    return NULL;
}

/* Load at most one real synced album-art sidecar.  The service is called only
 * after a quiet settle period with an empty input queue; the renderer never
 * opens or decodes a cover. */
static bool dm_itunes_art_service_one(void)
{
    struct dm_itunes_art_slot *slot;
    char path[MAX_PATH];
    int i;
    int rc;

    if (TIME_BEFORE(*rb->current_tick, dm_itunes_art_not_before) ||
        rb->button_queue_count() != 0 || rb->button_hold())
        return false;
#ifdef HAVE_TAGCACHE
    const struct tagcache_stat *stat = rb->tagcache_get_stat();
    if (!stat || !stat->ready || stat->commit_step) return false;
#endif
    for (i = 0; i < MIN(dm_itunes_count, DM_ITUNES_ART_SLOTS); i++)
    {
        slot = &dm_itunes_art[i];
        if (slot->attempted)
            continue;
        slot->attempted = true;
        slot->row = i;
        if (!dm_itunes_cover_path(dm_itunes_rows[i].path, path,
                                  sizeof(path)))
            return false;
        rb->memset(&slot->bitmap, 0, sizeof(slot->bitmap));
        slot->bitmap.width = dm_itunes_source == DM_ITUNES_ALBUMS ? DM_ITUNES_ART_SIZE : 15;
        slot->bitmap.height = slot->bitmap.width;
        slot->bitmap.format = FORMAT_NATIVE;
        slot->bitmap.data = slot->data;
        rc = rb->read_bmp_file(path, &slot->bitmap,
                               sizeof(slot->data),
                               FORMAT_NATIVE | FORMAT_RESIZE |
                               FORMAT_KEEP_ASPECT | FORMAT_DITHER, NULL);
        if (rc <= 0)
            return false;
        rb->strlcpy(slot->path, path, sizeof(slot->path));
        slot->valid = true;
        return true;
    }
    return false;
}

#ifdef HAVE_TAGCACHE
/* A unique tag_album result carries the album tag seek, but its category
 * record is not guaranteed to be a playable master-index entry.  Resolve a
 * single representative track through that album seek so artwork lookup and
 * the secondary artist label use the same real file iTunes would show. */
static void dm_itunes_fill_album_representative(struct dm_itunes_row *row)
{
    /* Main-thread-only search storage avoids nested large ARM stack frames. */
    static struct tagcache_search search;
    char value[MAX_PATH];
    int idxid;

    if (row->path[0] || !rb->tagcache_search(&search, tag_title))
        return;
    if (!rb->tagcache_search_add_filter(&search, tag_album, row->seek))
    {
        rb->tagcache_search_finish(&search);
        return;
    }
    if (dm_library.current.artist >= 0 &&
        !rb->tagcache_search_add_filter(&search, tag_artist,
                                        dm_library.current.artist))
    {
        rb->tagcache_search_finish(&search);
        return;
    }
    if (rb->tagcache_get_next(&search, value, sizeof(value)))
    {
        idxid = search.idx_id;
        if (rb->tagcache_retrieve(&search, idxid, tag_filename,
                                  row->path, sizeof(row->path)))
        {
            row->idxid = idxid;
            rb->tagcache_retrieve(&search, idxid, tag_artist,
                                  row->artist, sizeof(row->artist));
            rb->tagcache_retrieve(&search, idxid, tag_album,
                                  row->album, sizeof(row->album));
        }
    }
    rb->tagcache_search_finish(&search);
}
#endif

static void dm_itunes_rebuild_visible(void)
{
    int i;

    dm_itunes_visible_count = 0;
    for (i = 0; i < dm_itunes_count; i++)
    {
        dm_itunes_visible[dm_itunes_visible_count++] = i;
    }
    dm_file_selected = dm_itunes_visible_count > 0 ?
                       dm_itunes_visible[0] : -1;
}

static int dm_itunes_split_tabs(char *line, char **fields, int maximum)
{
    int count = 0;
    char *cursor = line;

    while (count < maximum)
    {
        char *tab;

        fields[count++] = cursor;
        tab = rb->strchr(cursor, '\t');
        if (!tab)
            break;
        *tab = '\0';
        cursor = tab + 1;
    }
    return count;
}

static bool dm_itunes_live_video(const char *path)
{
    return rb->strstr(path, "/Videos/Live/") != NULL ||
           rb->strstr(path, "/Live/") != NULL;
}

static void dm_itunes_commit_rows(void)
{
    int i;

    dm_itunes_art_reset();
    dm_file_count = dm_itunes_count;
    dm_file_selected = dm_itunes_count > 0 ? 0 : -1;
    dm_file_top = 0;
    rb->strlcpy(dm_cwd, "/", sizeof(dm_cwd));
    for (i = 0; i < dm_itunes_count; i++)
    {
        rb->strlcpy(dm_files[i].name, dm_itunes_rows[i].path[0] ?
                    dm_itunes_rows[i].path : dm_itunes_rows[i].title,
                    sizeof(dm_files[i].name));
        dm_files[i].is_dir = false;
    }
    dm_itunes_rebuild_visible();
#ifdef SIMULATOR
    DEBUGF("desktop library: source=%d depth=%d artist=%ld album=%ld rows=%d pending=%d\n",
           dm_itunes_source, dm_library.depth, (long)dm_library.current.artist,
           (long)dm_library.current.album, dm_itunes_count,
           dm_itunes_database_pending);
    const struct mp3entry *playing = rb->audio_current_track();
    DEBUGF("desktop resources: audio=%d elapsed=%lu arena=%lu playlist=%d\n",
        rb->audio_status(), playing ? playing->elapsed : 0,
        (unsigned long)dm_arena_left, rb->playlist_amount());
    for (int i = 0; i < dm_itunes_count; i++)
        DEBUGF("desktop row: %d %s | %s | %s\n", i,
               dm_itunes_rows[i].title, dm_itunes_rows[i].artist,
               dm_itunes_rows[i].album);
#endif
}

static void dm_itunes_load_videos(void)
{
    char line[1024];
    int fd;
    int absolute = 0;

    dm_itunes_count = 0;
    dm_itunes_total = 0;
    dm_itunes_database_pending = false;
    dm_itunes_has_more = false;
    dm_itunes_total_known = true;
    fd = rb->open(DM_ITUNES_VIDEO_INDEX, O_RDONLY);
    if (fd < 0)
    {
        dm_itunes_commit_rows();
        return;
    }
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *fields[12];
        int field_count;
        struct dm_itunes_row *row;

        if (line[0] == '\0' || line[0] == '#' ||
            !rb->strncmp(line, "video_id\t", 9))
            continue;
        field_count = dm_itunes_split_tabs(line, fields, ARRAYLEN(fields));
        if (field_count < 12 || fields[6][0] == '\0' ||
            rb->atoi(fields[11]) != 0 || dm_itunes_live_video(fields[6]))
            continue;
        if (dm_itunes_search[0] &&
            !rb->strcasestr(fields[3], dm_itunes_search) &&
            !rb->strcasestr(fields[4], dm_itunes_search) &&
            !rb->strcasestr(fields[7], dm_itunes_search)) continue;
        if (absolute >= dm_itunes_page_top &&
            dm_itunes_count < MIN(DM_ITUNES_PAGE_MAX, DM_ITUNES_ROWS))
        {
            row = &dm_itunes_rows[dm_itunes_count++];
            rb->memset(row, 0, sizeof(*row));
            rb->strlcpy(row->title, fields[3][0] ? fields[3] : fields[6],
                        sizeof(row->title));
            rb->strlcpy(row->artist, fields[4], sizeof(row->artist));
            rb->strlcpy(row->album, fields[7], sizeof(row->album));
            rb->strlcpy(row->path, fields[6], sizeof(row->path));
            row->duration = rb->atoi(fields[10]);
        }
        absolute++;
    }
    rb->close(fd);
    dm_itunes_total = absolute;
    dm_itunes_has_more =
        dm_itunes_page_top + dm_itunes_count < dm_itunes_total;
    dm_itunes_commit_rows();
}

#ifdef HAVE_TAGCACHE
static bool dm_itunes_filters(struct tagcache_search *search)
{
    const struct desktop_library_level *level = &dm_library.current;
    return (level->artist < 0 || rb->tagcache_search_add_filter(
                search, tag_artist, level->artist)) &&
           (level->album < 0 || rb->tagcache_search_add_filter(
                search, tag_album, level->album)) &&
           (level->genre < 0 || rb->tagcache_search_add_filter(
                search, tag_genre, level->genre));
}

static NO_INLINE void dm_itunes_load_tagcache(void)
{
    const struct tagcache_stat *stat = rb->tagcache_get_stat();
    /* Main-thread-only search storage avoids nested large ARM stack frames. */
    static struct tagcache_search search;
    static struct dm_itunes_row candidate;
    char value[MAX_PATH];
    int tag = dm_itunes_source == DM_ITUNES_ALBUMS ? tag_album :
              dm_itunes_source == DM_ITUNES_ARTISTS ? tag_artist :
              dm_itunes_source == DM_ITUNES_GENRES ? tag_genre : tag_title;
    int page_size = MIN(DM_ITUNES_PAGE_MAX, dm_itunes_page_capacity());
    int absolute = 0, scanned = 0;

    dm_itunes_count = dm_itunes_total = 0;
    dm_itunes_has_more = dm_itunes_total_known = false;
    dm_itunes_database_pending = !stat || !stat->readyvalid || !stat->ready;
    if (dm_itunes_database_pending || !rb->tagcache_search(&search, tag))
    {
        dm_itunes_commit_rows();
        return;
    }
    if (!dm_itunes_filters(&search))
        goto finish;
    if (tag != tag_title)
        rb->tagcache_search_set_uniqbuf(&search, dm_itunes_uniqbuf,
                                       sizeof(dm_itunes_uniqbuf));
    while (rb->tagcache_get_next(&search, value, sizeof(value)))
    {
        if ((++scanned & 31) == 0) rb->yield();
        rb->memset(&candidate, 0, sizeof(candidate));
        rb->strlcpy(candidate.title, value, sizeof(candidate.title));
        candidate.idxid = search.idx_id;
        candidate.seek = search.result_seek;
        if (tag == tag_title)
        {
            candidate.duration = rb->tagcache_get_numeric(&search, tag_length);
            rb->tagcache_retrieve(&search, search.idx_id, tag_filename,
                                  candidate.path, sizeof(candidate.path));
            rb->tagcache_retrieve(&search, search.idx_id, tag_artist,
                                  candidate.artist, sizeof(candidate.artist));
            rb->tagcache_retrieve(&search, search.idx_id, tag_album,
                                  candidate.album, sizeof(candidate.album));
        }
        /* Match the indexed result before pagination: search covers the
         * complete current hierarchy, not only its visible first page. */
        if (!dm_itunes_row_matches(&candidate)) continue;
        if (absolute++ < dm_itunes_page_top) continue;
        if (dm_itunes_count == page_size)
        {
            dm_itunes_has_more = true;
            break;
        }
        dm_itunes_rows[dm_itunes_count++] = candidate;
    }
finish:
    rb->tagcache_search_finish(&search);
    if (tag == tag_album)
        for (int i = 0; i < dm_itunes_count; i++)
            dm_itunes_fill_album_representative(&dm_itunes_rows[i]);
    dm_itunes_total = absolute;
    dm_itunes_total_known = !dm_itunes_has_more;
    dm_itunes_commit_rows();
}
#endif

/* Playlist browsing uses the configured catalog and M3U paths. It never
 * opens/replaces the live playlist until the user explicitly plays a row. */
static NO_INLINE void dm_itunes_load_playlists(void)
{
    static struct dm_itunes_row candidate;
    char line[MAX_PATH + 128];
    int absolute = 0;
    int fd = -1;
    DIR *directory = NULL;
    struct dirent *entry;
    const char *catalog = (const char *)rb->global_settings->playlist_catalog_dir;
    if (!catalog[0]) catalog = "/Playlists";
    dm_itunes_count = dm_itunes_total = 0;
    dm_itunes_database_pending = dm_itunes_has_more = false;
    if (dm_itunes_playlist[0]) fd = rb->open(dm_itunes_playlist, O_RDONLY);
    else directory = rb->opendir(catalog);
    while (fd >= 0 || directory)
    {
        rb->memset(&candidate, 0, sizeof(candidate));
        if (fd >= 0)
        {
            if (rb->read_line(fd, line, sizeof(line)) <= 0) break;
            if (!line[0] || line[0] == '#') continue;
            if (line[0] == '/') rb->strlcpy(candidate.path, line,
                                           sizeof(candidate.path));
            else
            {
                rb->strlcpy(candidate.path, dm_itunes_playlist,
                            sizeof(candidate.path));
                char *slash = rb->strrchr(candidate.path, '/');
                if (slash) slash[1] = 0;
                rb->strlcat(candidate.path, line, sizeof(candidate.path));
            }
            const char *base = rb->strrchr(line, '/');
            rb->strlcpy(candidate.title, base ? base + 1 : line,
                        sizeof(candidate.title));
        }
        else
        {
            entry = rb->readdir(directory);
            if (!entry) break;
            const char *ext = rb->strrchr(entry->d_name, '.');
            if (!ext || (rb->strcasecmp(ext, ".m3u") &&
                         rb->strcasecmp(ext, ".m3u8"))) continue;
            rb->snprintf(candidate.path, sizeof(candidate.path), "%s/%s",
                         catalog, entry->d_name);
            rb->strlcpy(candidate.title, entry->d_name,
                        sizeof(candidate.title));
        }
        if (!dm_itunes_row_matches(&candidate)) continue;
        if (absolute++ < dm_itunes_page_top) continue;
        if (dm_itunes_count == MIN(DM_ITUNES_PAGE_MAX, DM_ITUNES_ROWS))
        {
            dm_itunes_has_more = true;
            break;
        }
        candidate.seek = absolute;
        dm_itunes_rows[dm_itunes_count++] = candidate;
        rb->yield();
    }
    if (fd >= 0) rb->close(fd);
    if (directory) rb->closedir(directory);
    dm_itunes_total = absolute;
    dm_itunes_total_known = !dm_itunes_has_more;
    dm_itunes_commit_rows();
}

static void dm_itunes_load(void)
{
    if (dm_itunes_source == DM_ITUNES_PLAYLISTS || dm_itunes_playlist[0])
        dm_itunes_load_playlists();
    else if (dm_itunes_source == DM_ITUNES_VIDEOS)
        dm_itunes_load_videos();
    else
    {
#ifdef HAVE_TAGCACHE
        dm_itunes_load_tagcache();
#else
        dm_itunes_count = 0;
        dm_itunes_total = 0;
        dm_itunes_database_pending = false;
        dm_itunes_has_more = false;
        dm_itunes_total_known = true;
        dm_itunes_commit_rows();
#endif
    }
}

static void dm_itunes_change_page(int direction)
{
    int step = 1;
    int next;

#if LCD_WIDTH < 1920
    if (dm_itunes_source == DM_ITUNES_ALBUMS)
        step = DM_ITUNES_ALBUM_TILES;
#endif
    next = dm_itunes_page_top + direction * step;

    if (direction > 0 && !dm_itunes_has_more)
        return;
    next = MAX(0, next);
    if (dm_itunes_total_known && next >= dm_itunes_total)
        return;
    dm_itunes_page_top = next;
    dm_itunes_load();
}

static void dm_draw_itunes_chrome(void)
{
#if LCD_WIDTH >= 1920
    if (dm_fullscreen)
        dm_blit_scaled(DM_ASSET_ITUNES_WINDOW, DM_ITUNES_X, DM_ITUNES_Y,
                       DM_ITUNES_W, DM_ITUNES_H);
    else
        dm_blit(DM_ASSET_ITUNES_WINDOW, DM_ITUNES_X, DM_ITUNES_Y);
#else
    /* The native profile uses the same captured iTunes chrome as the host
     * profile. Keep its title/footer pixels intact, then compact the captured
     * transport strip from 42px to 34px and reflow the captured library slice
     * into the freed eight pixels. The three transport buttons are reblitted
     * from their real captured crops at uniform scale, so they get smaller
     * without becoming vertically squashed. */
    dm_blit(DM_ASSET_ITUNES_WINDOW, DM_ITUNES_X, DM_ITUNES_Y);
    if (!dm_fullscreen)
    {
        /* A clean captured gap between Next and Now Playing supplies the
         * authentic left-side Aqua gradient behind the reduced controls. */
        dm_blit_asset_region(DM_ASSET_ITUNES_WINDOW, DM_ITUNES_X,
                             DM_ITUNES_Y + 22, 150, 22, 1, 42,
                             DM_ITUNES_W, 34);
        dm_blit_asset_region(DM_ASSET_ITUNES_WINDOW, DM_ITUNES_X + 159,
                             DM_ITUNES_Y + 22, 159, 22,
                             DM_ITUNES_W - 159, 42,
                             DM_ITUNES_W - 159, 34);
        dm_blit_asset_region(DM_ASSET_ITUNES_WINDOW, DM_ITUNES_X,
                             DM_ITUNES_Y + 56, 0, 64, DM_ITUNES_W, 16,
                             DM_ITUNES_W, 16);
        dm_blit_asset_region(DM_ASSET_ITUNES_WINDOW,
                             DM_ITUNES_X, DM_ITUNES_Y + 72, 0, 80,
                             DM_ITUNES_W, 70,
                             DM_ITUNES_W, 78);

        /* Source rectangles are the measured native transport controls. */
        dm_blit_asset_region(DM_ASSET_ITUNES_WINDOW,
                             DM_ITUNES_X + 33, DM_ITUNES_Y + 25,
                             30, 24, 31, 35, 27, 30);
        dm_blit_asset_region(DM_ASSET_ITUNES_WINDOW,
                             DM_ITUNES_X + 70, DM_ITUNES_Y + 26,
                             66, 23, 38, 38, 30, 30);
        dm_blit_asset_region(DM_ASSET_ITUNES_WINDOW,
                             DM_ITUNES_X + 109, DM_ITUNES_Y + 25,
                             106, 24, 33, 35, 27, 30);
    }
#endif
}

static void dm_draw_itunes(void)
{
    const struct mp3entry *track = rb->audio_current_track();
    const struct dm_font *small = &dm_fonts[DM_FONT_SMALL];
    char status[64];
    char footer[64];
#if LCD_WIDTH >= 1920
    const struct dm_font *regular = &dm_fonts[DM_FONT_REGULAR];
    int list_width = DM_ITUNES_LIST_W;
    int name_width = list_width * 45 / 100;
    int artist_width = list_width * 25 / 100;
    int album_width = list_width - name_width - artist_width;
#endif
    int i;

    if (track && !track->path[0]) track = NULL;
#if LCD_WIDTH < 1920
    {
        struct dm_rect now_panel =
            { DM_ITUNES_X + 159, DM_ITUNES_Y + 22,
              MAX(54, DM_ITUNES_W - 180), DM_ITUNES_NOW_PANEL_H };
        int source_row_h = DM_ITUNES_SOURCE_ROW_H;
        int scroll_capacity = dm_itunes_page_capacity();
        int visible = MIN(dm_itunes_visible_count, scroll_capacity);
        int progress = 0;
        const char *repeat_label =
            rb->global_settings->repeat_mode == REPEAT_ONE ? "Repeat 1" :
            rb->global_settings->repeat_mode == REPEAT_ALL ? "Repeat All" :
                                                            "Repeat";

        dm_draw_itunes_chrome();
        dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_BOLD], DM_ITUNES_X,
                                     DM_ITUNES_Y + 1, DM_ITUNES_W, 19,
                                     DM_INK_TITLE, dm_library.depth ?
                                     dm_library_title[dm_library.depth] : "iTunes");

        if (dm_library.depth)
            dm_draw_text_in_box(small, DM_ITUNES_X + 66, DM_ITUNES_Y + 2,
                                36, 17, DM_INK, "< Back");
        if (track)
        {
            dm_draw_text_centered_in_box(small, now_panel.x + 3,
                                         now_panel.y + 4,
                                         now_panel.width - 6, 11, DM_INK,
                                         track->title ? track->title :
                                         track->path);
            dm_draw_text_centered_in_box(small, now_panel.x + 3,
                                         now_panel.y + 14,
                                         now_panel.width - 6, 11,
                                         DM_INK_STATUS,
                                         track->artist ? track->artist :
                                         (track->album ? track->album :
                                          "Now Playing"));
            if (track->length > 0)
                progress = MIN(now_panel.width - 10,
                               (int)(track->elapsed *
                               (unsigned long)(now_panel.width - 10) /
                               track->length));
        }
        else
            dm_draw_text_centered_in_box(small, now_panel.x, now_panel.y + 5,
                                         now_panel.width, 17, DM_INK_STATUS,
                                         dm_itunes_database_pending ?
                                         "Loading Library" : "Not Playing");
        dm_fill_rect((struct dm_rect){ now_panel.x + 5,
                                      now_panel.y + now_panel.height - 7,
                                      now_panel.width - 10, 2 },
                     LCD_RGBPACK(188, 192, 181));
        if (progress > 0)
            dm_fill_rect((struct dm_rect){ now_panel.x + 5,
                                          now_panel.y + now_panel.height - 7,
                                          progress, 2 },
                         LCD_RGBPACK(71, 128, 205));
        dm_draw_text_in_box(small, DM_ITUNES_X + 7,
                            DM_ITUNES_BODY_Y - DM_ITUNES_HEADER_H,
                            DM_ITUNES_SOURCE_W - 12, DM_ITUNES_HEADER_H,
                            DM_INK_SIDEBAR_HEAD, "LIBRARY");
        for (i = 0; i < DM_ITUNES_SOURCE_COUNT; i++)
        {
            int y = DM_ITUNES_BODY_Y + i * source_row_h;
            fb_data ink = DM_INK_SIDEBAR;

            if (i == (int)dm_itunes_source)
            {
                dm_blit_scaled(DM_ASSET_SIDEBAR_SELECTION, DM_ITUNES_X + 1,
                               y, DM_ITUNES_SOURCE_W - 2, source_row_h);
                ink = DM_INK_WHITE;
            }
            dm_draw_text_in_box(small, DM_ITUNES_X + 14, y,
                                DM_ITUNES_SOURCE_W - 20, source_row_h, ink,
                                dm_itunes_source_names[i]);
        }

#if LCD_WIDTH < 1920
        if (dm_itunes_source == DM_ITUNES_ALBUMS)
        {
            dm_fill_rect((struct dm_rect){ DM_ITUNES_LIST_X, DM_ITUNES_BODY_Y,
                DM_ITUNES_LIST_W, DM_ITUNES_SCROLLER_H }, LCD_RGBPACK(250,250,250));
            dm_draw_text_in_box(small, DM_ITUNES_LIST_X + 5,
                                DM_ITUNES_BODY_Y - DM_ITUNES_HEADER_H,
                                DM_ITUNES_LIST_W - 10, DM_ITUNES_HEADER_H,
                                DM_INK, "Albums");
            for (i = 0; i < visible && i < DM_ITUNES_ALBUM_TILES; i++)
            {
                int row_index = dm_itunes_visible[i];
                struct dm_rect tile = dm_itunes_album_item_rect(i);
                const struct dm_itunes_row *row =
                    &dm_itunes_rows[row_index];
                struct dm_itunes_art_slot *art =
                    dm_itunes_art_for_row(row_index);
                fb_data title_ink = DM_INK;
                fb_data detail_ink = DM_INK_STATUS;

                if (row_index == dm_file_selected)
                {
                    dm_blit_scaled(DM_ASSET_ITUNES_SELECTION,
                                   tile.x, tile.y, tile.width, tile.height);
                    title_ink = detail_ink = DM_INK_WHITE;
                }
                if (art)
                    dm_blit_bitmap(&art->bitmap, tile.x + 3, tile.y + 3,
                                   false);
                dm_draw_text_in_box(small, tile.x + DM_ITUNES_ART_SIZE + 8, tile.y + 3,
                                    tile.width - DM_ITUNES_ART_SIZE - 11, 11, title_ink,
                                    row->title);
                dm_draw_text_in_box(small, tile.x + DM_ITUNES_ART_SIZE + 8, tile.y + 16,
                                    tile.width - DM_ITUNES_ART_SIZE - 11, 11, detail_ink,
                                    row->artist[0] ? row->artist :
                                                     "Unknown Artist");
            }
        }
        else
#endif
        {
            dm_draw_text_in_box(small, DM_ITUNES_NAME_X,
                                DM_ITUNES_BODY_Y - DM_ITUNES_HEADER_H,
                                DM_ITUNES_NAME_W, DM_ITUNES_HEADER_H,
                                DM_INK, "Name");
            dm_draw_text_in_box(small, DM_ITUNES_ARTIST_X,
                                DM_ITUNES_BODY_Y - DM_ITUNES_HEADER_H,
                                DM_ITUNES_ARTIST_W, DM_ITUNES_HEADER_H,
                                DM_INK, "Artist");

            for (i = 0; i < visible; i++)
            {
                int row_index = dm_itunes_visible[i];
                int y = DM_ITUNES_BODY_Y + i * DM_ITUNES_ROW_H;
                const struct dm_itunes_row *row =
                    &dm_itunes_rows[row_index];
                const char *secondary = row->artist[0] ? row->artist :
                                                                  row->album;
                struct dm_itunes_art_slot *art =
                    dm_itunes_art_for_row(row_index);
                fb_data title_ink = DM_INK;
                fb_data detail_ink = DM_INK_STATUS;

                if (row_index == dm_file_selected)
                {
                    dm_blit_scaled(DM_ASSET_ITUNES_SELECTION,
                                   DM_ITUNES_LIST_X, y, DM_ITUNES_LIST_W,
                                   DM_ITUNES_ROW_H);
                    title_ink = detail_ink = DM_INK_WHITE;
                }
                if (art)
                    dm_blit_bitmap(&art->bitmap, DM_ITUNES_LIST_X + 2,
                                   y + 1, false);
                if (track && row->path[0] && !rb->strcmp(track->path, row->path))
                    dm_fill_rect((struct dm_rect){ DM_ITUNES_LIST_X, y + 2,
                        2, DM_ITUNES_ROW_H - 4 }, LCD_RGBPACK(35,110,55));
                dm_draw_text_in_box(small, DM_ITUNES_NAME_X, y,
                                    DM_ITUNES_NAME_W, DM_ITUNES_ROW_H,
                                    title_ink, row->title);
                dm_draw_text_in_box(small, DM_ITUNES_ARTIST_X, y,
                                    DM_ITUNES_ARTIST_W - (LCD_WIDTH == 640 ? 45 : 0),
                                    DM_ITUNES_ROW_H, detail_ink, secondary);
                if (LCD_WIDTH == 640 && row->duration)
                {
                    char duration[16];
                    rb->snprintf(duration, sizeof(duration), "%lu:%02lu",
                                 row->duration / 60000,
                                 row->duration / 1000 % 60);
                    dm_draw_text_right_in_box(small,
                        DM_ITUNES_LIST_X + DM_ITUNES_LIST_W - 4, y,
                        DM_ITUNES_ROW_H, 40, detail_ink, duration);
                }
            }
        }

        if (!visible)
            dm_draw_text_centered_in_box(small, DM_ITUNES_LIST_X,
                DM_ITUNES_BODY_Y + 8, DM_ITUNES_LIST_W, 20, DM_INK_STATUS,
                dm_itunes_database_pending ? "Library is not ready" :
                dm_itunes_search[0] ? "No matching items" : "No items");
        if (dm_itunes_total_known && dm_itunes_total > scroll_capacity)
        {
            int track_height = DM_ITUNES_SCROLLER_H;
            int thumb_height = MAX(DM_ITUNES_SCROLLER_MIN_THUMB_H,
                                   track_height * scroll_capacity /
                                   dm_itunes_total);
            int travel = track_height - thumb_height;
            int maximum = dm_itunes_total - scroll_capacity;

            dm_blit_scaled(DM_ASSET_SCROLLER_TRACK,
                           DM_ITUNES_SCROLLER_X, DM_ITUNES_SCROLLER_Y,
                           DM_ITUNES_SCROLLER_W, track_height);
            dm_blit_scaled(DM_ASSET_SCROLLER_THUMB,
                           DM_ITUNES_SCROLLER_X,
                           DM_ITUNES_SCROLLER_Y +
                           (maximum ? dm_itunes_page_top * travel / maximum : 0),
                           DM_ITUNES_SCROLLER_W, thumb_height);
        }

        if (dm_itunes_search[0])
            rb->snprintf(status, sizeof(status), "Find: %s",
                         dm_itunes_search);
        else
            rb->strlcpy(status, "Search", sizeof(status));
        dm_draw_text_in_box(small, DM_ITUNES_X + 6,
                            DM_ITUNES_Y + DM_ITUNES_H -
                            DM_ITUNES_BOTTOM_H, 88, DM_ITUNES_BOTTOM_H,
                            dm_itunes_search[0] ? LCD_RGBPACK(35, 82, 154) :
                                                 DM_INK_STATUS, status);
        dm_draw_text_in_box(small, DM_ITUNES_X + 96,
                            DM_ITUNES_Y + DM_ITUNES_H -
                            DM_ITUNES_BOTTOM_H, 44, DM_ITUNES_BOTTOM_H,
                            rb->global_settings->playlist_shuffle ?
                            LCD_RGBPACK(35, 82, 154) : DM_INK_STATUS, "Shuffle");
        dm_draw_text_in_box(small, DM_ITUNES_X + 143,
                            DM_ITUNES_Y + DM_ITUNES_H -
                            DM_ITUNES_BOTTOM_H, 45, DM_ITUNES_BOTTOM_H,
                            rb->global_settings->repeat_mode != REPEAT_OFF ?
                            LCD_RGBPACK(35, 82, 154) : DM_INK_STATUS,
                            repeat_label);
        rb->snprintf(footer, sizeof(footer), "%d-%d / %d",
                     dm_itunes_total ? dm_itunes_page_top + 1 : 0,
                     dm_itunes_page_top + dm_itunes_count, dm_itunes_total);
        dm_draw_text_right_in_box(small, DM_ITUNES_X + DM_ITUNES_W - 42,
                                  DM_ITUNES_Y + DM_ITUNES_H -
                                  DM_ITUNES_BOTTOM_H, DM_ITUNES_BOTTOM_H, 66,
                                  DM_INK_STATUS, footer);
        dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_BOLD],
                                     DM_ITUNES_X + DM_ITUNES_W - 40,
                                     DM_ITUNES_Y + DM_ITUNES_H -
                                     DM_ITUNES_BOTTOM_H, 18,
                                     DM_ITUNES_BOTTOM_H, DM_INK, "<");
        dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_BOLD],
                                     DM_ITUNES_X + DM_ITUNES_W - 20,
                                     DM_ITUNES_Y + DM_ITUNES_H -
                                     DM_ITUNES_BOTTOM_H, 18,
                                     DM_ITUNES_BOTTOM_H, DM_INK, ">");
        return;
    }
#else
    /* The furniture is sampled from the owned iTunes capture. Runtime drawing
     * is limited to library data, labels, and the native selection gradients. */
    dm_draw_itunes_chrome();
    dm_draw_text_centered_in_box(small, DM_ITUNES_X, DM_ITUNES_Y,
                                 DM_ITUNES_W, 22, DM_INK_TITLE, dm_library.depth ?
                                     dm_library_title[dm_library.depth] : "iTunes");
    dm_draw_text_in_box(small, DM_ITUNES_X + 7, DM_ITUNES_BODY_Y - 17,
                        DM_ITUNES_SOURCE_W - 12, 17,
                        DM_INK_SIDEBAR_HEAD, "LIBRARY");
    for (i = 0; i < DM_ITUNES_SOURCE_COUNT; i++)
    {
        int y = DM_ITUNES_BODY_Y + i * DM_ITUNES_ROW_H;
        fb_data ink = DM_INK_SIDEBAR;

        if (i == (int)dm_itunes_source)
        {
            dm_blit_panel(DM_ASSET_SIDEBAR_SELECTION, DM_ITUNES_X + 1, y,
                          DM_ITUNES_SOURCE_W - 2, 2);
            ink = DM_INK_WHITE;
        }
        dm_draw_text_in_box(small, DM_ITUNES_X + 14, y,
                            DM_ITUNES_SOURCE_W - 20, DM_ITUNES_ROW_H, ink,
                            dm_itunes_source_names[i]);
    }
    dm_draw_text_in_box(small, DM_ITUNES_LIST_X + 6, DM_ITUNES_BODY_Y - 17,
                        name_width - 8, 17, DM_INK, "Name");
    if (LCD_WIDTH > 320)
    {
        dm_draw_text_in_box(small, DM_ITUNES_LIST_X + name_width + 5,
                            DM_ITUNES_BODY_Y - 17, artist_width - 8, 17,
                            DM_INK, "Artist");
        dm_draw_text_in_box(small,
                            DM_ITUNES_LIST_X + name_width + artist_width + 5,
                            DM_ITUNES_BODY_Y - 17, album_width - 8, 17,
                            DM_INK, "Album");
    }
    for (i = 0; i < DM_ITUNES_ROWS; i++)
    {
        int y = DM_ITUNES_BODY_Y + i * DM_ITUNES_ROW_H;
        struct dm_itunes_art_slot *art = dm_itunes_art_for_row(i);
        fb_data ink = DM_INK;

        if (i >= dm_itunes_count)
            break;
        if (i == dm_file_selected)
        {
            dm_blit_part(DM_ASSET_ITUNES_SELECTION, DM_ITUNES_LIST_X, y,
                         DM_ITUNES_LIST_W);
            ink = DM_INK_WHITE;
        }
        if (art)
            dm_blit_bitmap(&art->bitmap, DM_ITUNES_LIST_X + 2, y + 1,
                           false);
        dm_draw_text_in_box(regular, DM_ITUNES_LIST_X + 23, y,
                            (LCD_WIDTH > 320 ? name_width : list_width) - 27,
                            DM_ITUNES_ROW_H, ink, dm_itunes_rows[i].title);
        if (LCD_WIDTH > 320)
        {
            dm_draw_text_in_box(regular,
                                DM_ITUNES_LIST_X + name_width + 5, y,
                                artist_width - 8, DM_ITUNES_ROW_H, ink,
                                dm_itunes_rows[i].artist);
            dm_draw_text_in_box(regular,
                                DM_ITUNES_LIST_X + name_width + artist_width +
                                5, y, album_width - 8, DM_ITUNES_ROW_H, ink,
                                dm_itunes_rows[i].album);
        }
    }
    if (track)
        rb->snprintf(status, sizeof(status), "%s  %d:%02d",
                     track->title ? track->title : track->path,
                     (int)(track->elapsed / 60000),
                     (int)((track->elapsed / 1000) % 60));
    else if (dm_itunes_database_pending)
        rb->strlcpy(status, "Loading database...", sizeof(status));
    else
        rb->strlcpy(status, dm_itunes_source_names[dm_itunes_source],
                    sizeof(status));
    dm_draw_text_centered_in_box(small, DM_ITUNES_X + 150,
                                 DM_ITUNES_Y + 22, DM_ITUNES_W - 185, 29,
                                 DM_INK, status);
    if (dm_itunes_total_known)
        rb->snprintf(footer, sizeof(footer), "%d-%d of %d %s",
                     dm_itunes_total ? dm_itunes_page_top + 1 : 0,
                     dm_itunes_page_top + dm_itunes_count, dm_itunes_total,
                     dm_itunes_source_names[dm_itunes_source]);
    else
        rb->snprintf(footer, sizeof(footer), "%d-%d %s",
                     dm_itunes_page_top + 1,
                     dm_itunes_page_top + dm_itunes_count,
                     dm_itunes_source_names[dm_itunes_source]);
    dm_draw_text_centered_in_box(small, DM_ITUNES_LIST_X,
                                 DM_ITUNES_Y + DM_ITUNES_H -
                                 DM_ITUNES_BOTTOM_H,
                                 DM_ITUNES_LIST_W - 38, DM_ITUNES_BOTTOM_H,
                                 DM_INK, footer);
    dm_draw_text_centered_in_box(small, DM_ITUNES_X + DM_ITUNES_W - 38,
                                 DM_ITUNES_Y + DM_ITUNES_H -
                                 DM_ITUNES_BOTTOM_H, 18, DM_ITUNES_BOTTOM_H,
                                 DM_INK, "<");
    dm_draw_text_centered_in_box(small, DM_ITUNES_X + DM_ITUNES_W - 20,
                                 DM_ITUNES_Y + DM_ITUNES_H -
                                 DM_ITUNES_BOTTOM_H, 18, DM_ITUNES_BOTTOM_H,
                                 DM_INK, ">");
#endif
}

static void dm_dashboard_weather_copy_field(char **cursor, char *dst,
                                            size_t dst_size)
{
    char *start = *cursor;
    char *end = start;
    size_t length;

    while (*end && *end != '\t' && *end != '\n' && *end != '\r')
        end++;
    length = MIN((size_t)(end - start), dst_size - 1);
    rb->memcpy(dst, start, length);
    dst[length] = '\0';
    *cursor = *end == '\t' ? end + 1 : end;
}

static const char *dm_dashboard_weather_icon_name(const char *icon)
{
    if (icon && rb->strstr(icon, "clear"))
        return dm_dashboard_weather.night ? "clear_night" : "clear_day";
    if (icon && rb->strstr(icon, "partly_cloudy"))
        return "partly_cloudy";
    if (icon && rb->strstr(icon, "cloudy"))
        return "cloudy";
    if (icon && rb->strstr(icon, "drizzle"))
        return "drizzle";
    if (icon && rb->strstr(icon, "rain"))
        return "rain";
    if (icon && rb->strstr(icon, "snow"))
        return "snow";
    if (icon && rb->strstr(icon, "fog"))
        return "fog";
    if (icon && rb->strstr(icon, "thunder"))
        return "thunderstorm";
    return "unknown";
}

static bool dm_dashboard_weather_load_icon(void)
{
    char path[MAX_PATH];
    char fallback[MAX_PATH];
    const char *name;
    fb_data *pixels;
    int count;
    int rc;

    if (!dm_dashboard_weather.available)
        return false;
    name = dm_dashboard_weather_icon_name(dm_dashboard_weather.icon);
    rb->snprintf(path, sizeof(path), DM_WEATHER_ICON_DIR
                 "/apple-icons/%s.40x40x24.bmp", name);
    if (!rb->file_exists(path))
    {
        rb->snprintf(fallback, sizeof(fallback), DM_WEATHER_ICON_DIR
                     "/icons/%s.40x40x24.bmp", name);
        if (!rb->file_exists(fallback))
            return false;
        rb->strlcpy(path, fallback, sizeof(path));
    }
    if (dm_dashboard_weather.icon_valid &&
        !rb->strcmp(dm_dashboard_weather.icon_path, path))
        return true;

    rb->memset(&dm_dashboard_weather.icon_bitmap, 0,
               sizeof(dm_dashboard_weather.icon_bitmap));
    dm_dashboard_weather.icon_bitmap.width = DM_DASH_WEATHER_ICON_SIZE;
    dm_dashboard_weather.icon_bitmap.height = DM_DASH_WEATHER_ICON_SIZE;
    dm_dashboard_weather.icon_bitmap.format = FORMAT_NATIVE;
    dm_dashboard_weather.icon_bitmap.data = dm_dashboard_weather.icon_data;
    rc = rb->read_bmp_file(path, &dm_dashboard_weather.icon_bitmap,
                           sizeof(dm_dashboard_weather.icon_data),
                           FORMAT_NATIVE | FORMAT_TRANSPARENT | FORMAT_RESIZE |
                           FORMAT_KEEP_ASPECT | FORMAT_DITHER, NULL);
    if (rc <= 0)
    {
        dm_dashboard_weather.icon_valid = false;
        return false;
    }
    pixels = (fb_data *)dm_dashboard_weather.icon_bitmap.data;
    count = dm_dashboard_weather.icon_bitmap.width *
            dm_dashboard_weather.icon_bitmap.height;
    for (int i = 0; i < count; i++)
    {
        unsigned pixel = pixels[i];
        int red = FB_UNPACK_RED(pixel);
        int green = FB_UNPACK_GREEN(pixel);
        int blue = FB_UNPACK_BLUE(pixel);

        if (red >= 170 && blue >= 190 && green + 18 < red &&
            green + 18 < blue)
            pixels[i] = TRANSPARENT_COLOR;
    }
    rb->strlcpy(dm_dashboard_weather.icon_path, path,
                sizeof(dm_dashboard_weather.icon_path));
    dm_dashboard_weather.icon_valid = true;
    return true;
}

/* Read the same bounded, offline forecast bundle used by the Weather app.
 * The dashboard caches only the selected current/hourly row; its paint path
 * never opens forecast.tsv or decodes the weather icon. */
static void dm_dashboard_weather_refresh(void)
{
    char line[512];
    char field[64];
    char units[12] = "metric";
    char daily_icon[24] = "";
    char daily_condition[32] = "";
    char daily_min[8] = "";
    char daily_max[8] = "";
    char *next;
    int fd;
    bool current_found = false;
    bool hourly_found = false;
    int best_past = -1;
    int best_future = -1;
    int now_key = -1;
    const struct tm *now = rb->get_time();

    if (dm_dashboard_weather.checked_tick &&
        TIME_BEFORE(*rb->current_tick,
                    dm_dashboard_weather.checked_tick + HZ * 60))
        return;
    dm_dashboard_weather.checked_tick = *rb->current_tick;
    dm_dashboard_weather.available = false;
    dm_dashboard_weather.icon_valid = false;
    dm_dashboard_weather.icon[0] = '\0';
    dm_dashboard_weather.condition[0] = '\0';
    dm_dashboard_weather.location[0] = '\0';
    dm_dashboard_weather.temperature[0] = '\0';

    if (now && now->tm_year >= 100)
        now_key = (((now->tm_year + 1900) * 100 + now->tm_mon + 1) * 100 +
                   now->tm_mday) * 100 + now->tm_hour;
    fd = rb->open(DM_WEATHER_FORECAST, O_RDONLY);
    if (fd < 0)
        return;
    if (rb->read_line(fd, line, sizeof(line)) <= 0)
        goto done;
    next = line;
    dm_dashboard_weather_copy_field(&next, field, sizeof(field));
    if (rb->strcmp(field, "rockpod_weather_v1"))
        goto done;
    dm_dashboard_weather_copy_field(&next, dm_dashboard_weather.location,
                                    sizeof(dm_dashboard_weather.location));
    for (int i = 0; i < 5; i++)
        dm_dashboard_weather_copy_field(&next, field, sizeof(field));
    dm_dashboard_weather_copy_field(&next, units, sizeof(units));

    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char first[24];

        next = line;
        dm_dashboard_weather_copy_field(&next, first, sizeof(first));
        if (!first[0])
            continue;
        if (!rb->strcmp(first, "current"))
        {
            char stamp[24];
            char icon[24];
            char condition[32];
            char temperature[8];
            char skip[16];
            char is_day[4];

            dm_dashboard_weather_copy_field(&next, stamp, sizeof(stamp));
            dm_dashboard_weather_copy_field(&next, icon, sizeof(icon));
            dm_dashboard_weather_copy_field(&next, condition,
                                            sizeof(condition));
            dm_dashboard_weather_copy_field(&next, temperature,
                                            sizeof(temperature));
            dm_dashboard_weather_copy_field(&next, skip, sizeof(skip));
            dm_dashboard_weather_copy_field(&next, skip, sizeof(skip));
            dm_dashboard_weather_copy_field(&next, skip, sizeof(skip));
            dm_dashboard_weather_copy_field(&next, is_day, sizeof(is_day));
            if (!current_found && icon[0] && condition[0] && temperature[0])
            {
                rb->strlcpy(dm_dashboard_weather.icon, icon,
                            sizeof(dm_dashboard_weather.icon));
                rb->strlcpy(dm_dashboard_weather.condition, condition,
                            sizeof(dm_dashboard_weather.condition));
                rb->snprintf(dm_dashboard_weather.temperature,
                             sizeof(dm_dashboard_weather.temperature),
                             "%s %c", temperature,
                             units[0] == 'i' ? 'F' : 'C');
                dm_dashboard_weather.night = is_day[0] && rb->atoi(is_day) == 0;
                dm_dashboard_weather.available = true;
                current_found = true;
            }
            continue;
        }
        if (!rb->strcmp(first, "hourly"))
        {
            char stamp[24];
            char icon[24];
            char condition[32];
            char temperature[8];
            char skip[16];
            char is_day[4];
            int key = -1;

            dm_dashboard_weather_copy_field(&next, stamp, sizeof(stamp));
            dm_dashboard_weather_copy_field(&next, icon, sizeof(icon));
            dm_dashboard_weather_copy_field(&next, condition,
                                            sizeof(condition));
            dm_dashboard_weather_copy_field(&next, temperature,
                                            sizeof(temperature));
            dm_dashboard_weather_copy_field(&next, skip, sizeof(skip));
            dm_dashboard_weather_copy_field(&next, skip, sizeof(skip));
            dm_dashboard_weather_copy_field(&next, skip, sizeof(skip));
            dm_dashboard_weather_copy_field(&next, is_day, sizeof(is_day));
            if (current_found || !icon[0] || !condition[0] ||
                !temperature[0])
                continue;
            if (strlen(stamp) >= 13)
                key = rb->atoi(stamp) * 1000000 +
                      rb->atoi(stamp + 5) * 10000 +
                      rb->atoi(stamp + 8) * 100 + rb->atoi(stamp + 11);
            if (now_key < 0 || key < 0)
            {
                if (!hourly_found)
                {
                    best_past = 0;
                    hourly_found = true;
                }
            }
            else if (key <= now_key && key > best_past)
            {
                best_past = key;
                hourly_found = true;
            }
            else if (best_past < 0 &&
                     (best_future < 0 || key < best_future))
            {
                best_future = key;
                hourly_found = true;
                rb->strlcpy(dm_dashboard_weather.icon, icon,
                            sizeof(dm_dashboard_weather.icon));
                rb->strlcpy(dm_dashboard_weather.condition, condition,
                            sizeof(dm_dashboard_weather.condition));
                rb->snprintf(dm_dashboard_weather.temperature,
                             sizeof(dm_dashboard_weather.temperature),
                             "%s %c", temperature,
                             units[0] == 'i' ? 'F' : 'C');
                dm_dashboard_weather.night = is_day[0] && rb->atoi(is_day) == 0;
            }
            if ((best_past == key || (best_past == 0 && key < 0)) &&
                hourly_found)
            {
                rb->strlcpy(dm_dashboard_weather.icon, icon,
                            sizeof(dm_dashboard_weather.icon));
                rb->strlcpy(dm_dashboard_weather.condition, condition,
                            sizeof(dm_dashboard_weather.condition));
                rb->snprintf(dm_dashboard_weather.temperature,
                             sizeof(dm_dashboard_weather.temperature),
                             "%s %c", temperature,
                             units[0] == 'i' ? 'F' : 'C');
                dm_dashboard_weather.night = is_day[0] && rb->atoi(is_day) == 0;
            }
            continue;
        }
        if (!daily_condition[0] && rb->strchr(first, '-'))
        {
            dm_dashboard_weather_copy_field(&next, daily_icon,
                                            sizeof(daily_icon));
            dm_dashboard_weather_copy_field(&next, daily_condition,
                                            sizeof(daily_condition));
            dm_dashboard_weather_copy_field(&next, daily_min,
                                            sizeof(daily_min));
            dm_dashboard_weather_copy_field(&next, daily_max,
                                            sizeof(daily_max));
        }
    }
    if (!dm_dashboard_weather.available && !current_found && hourly_found)
        dm_dashboard_weather.available = true;
    if (!dm_dashboard_weather.available && !current_found &&
        daily_condition[0])
    {
        rb->strlcpy(dm_dashboard_weather.icon, daily_icon,
                    sizeof(dm_dashboard_weather.icon));
        rb->strlcpy(dm_dashboard_weather.condition, daily_condition,
                    sizeof(dm_dashboard_weather.condition));
        rb->snprintf(dm_dashboard_weather.temperature,
                     sizeof(dm_dashboard_weather.temperature), "%s-%s %c",
                     daily_min, daily_max, units[0] == 'i' ? 'F' : 'C');
        dm_dashboard_weather.available = true;
    }
done:
    rb->close(fd);
    if (dm_dashboard_weather.available)
        dm_dashboard_weather_load_icon();
}

static void dm_dashboard_refresh_cache(bool refresh_library)
{
    const struct tm *now = rb->get_time();

    if (now)
        dm_dashboard_time = *now;
#ifdef HAVE_TAGCACHE
    if (refresh_library)
    {
        const struct tagcache_stat *stat = rb->tagcache_get_stat();

        dm_dashboard_track_total = stat && stat->readyvalid && stat->ready ?
                                   stat->total_entries : 0;
    }
#else
    if (refresh_library)
        dm_dashboard_track_total = 0;
#endif
    dm_dashboard_weather_refresh();
}

static void dm_draw_dashboard(void)
{
    static const char * const months[] =
    {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    static const char * const weekdays[] =
    {
        "Sunday", "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday"
    };
    const struct dm_font *small = &dm_fonts[DM_FONT_SMALL];
    const struct tm *now = &dm_dashboard_time;
    const struct mp3entry *track = rb->audio_current_track();
    const int clock_x = 8;
    const int top_y = DM_MENUBAR_H + 9;
    const int calendar_x = 90;
    const int weather_x = 202;
    const int lower_y = DM_MENUBAR_H + 91;
    const int sticky_x = 8;
    const int player_x = 112;
    const int clock_cx = clock_x + 37;
    const int clock_cy = top_y + 39;
    int month_index = MAX(0, MIN(now->tm_mon, 11));
    int weekday_index = MAX(0, MIN(now->tm_wday, 6));
    char value[64];
    int progress = 0;
    int minute_angle;
    int hour_angle;
    int second_angle;

    /* Snow Leopard leaves the desktop visible below Dashboard's translucent
     * smoked overlay.  Aurora is already in the compositor, so dim it in
     * place instead of replacing it with a flat rectangle. */
    dm_dim_rect((struct dm_rect){ 0, DM_MENUBAR_H, LCD_WIDTH,
                                 LCD_HEIGHT - DM_MENUBAR_H },
                LCD_RGBPACK(8, 11, 17), 156);

    /* These are direct, checksum-tracked pixels from the user's owned
     * Snow Leopard AdditionalEssentials package.  Only live content is
     * painted over the original widget skins. */
    dm_compose(DM_ASSET_DASH_CLOCK, clock_x, top_y);
    dm_compose(DM_ASSET_DASH_ICAL, calendar_x, top_y);
    dm_compose(DM_ASSET_DASH_WEATHER, weather_x, top_y);
    dm_compose(DM_ASSET_DASH_STICKIES, sticky_x, lower_y);
    dm_compose(DM_ASSET_DASH_ITUNES, player_x, lower_y);

    minute_angle = now->tm_min * 6 - 90;
    hour_angle = (now->tm_hour % 12) * 30 + now->tm_min / 2 - 90;
    second_angle = now->tm_sec * 6 - 90;
    dm_draw_line(clock_cx, clock_cy,
                 clock_cx + fp14_cos(hour_angle) * 14 / 16384,
                 clock_cy + fp14_sin(hour_angle) * 14 / 16384,
                 LCD_RGBPACK(32, 32, 32));
    dm_draw_line(clock_cx, clock_cy,
                 clock_cx + fp14_cos(minute_angle) * 21 / 16384,
                 clock_cy + fp14_sin(minute_angle) * 21 / 16384,
                 LCD_RGBPACK(24, 24, 24));
    dm_draw_line(clock_cx, clock_cy,
                 clock_cx + fp14_cos(second_angle) * 22 / 16384,
                 clock_cy + fp14_sin(second_angle) * 22 / 16384,
                 LCD_RGBPACK(196, 33, 31));
    dm_fill_circle(clock_cx, clock_cy, 2, LCD_RGBPACK(40, 40, 40));

    dm_draw_text_centered_in_box(small, calendar_x + 2, top_y + 5, 45, 14,
                                 DM_INK_WHITE, weekdays[weekday_index]);
    dm_draw_text_centered_in_box(small, calendar_x + 2, top_y + 29, 45, 13,
                                 LCD_RGBPACK(194, 197, 202),
                                 months[month_index]);
    rb->snprintf(value, sizeof(value), "%d", now->tm_mday);
    dm_draw_text_scaled_centered_in_box(&dm_fonts[DM_FONT_BOLD],
                                        calendar_x + 50, top_y + 5,
                                        52, 41, 2, DM_INK_WHITE, value);

    /* The real Snow Leopard frame is content-transparent.  Fill it with the
     * synced forecast's actual icon and values, using the same Lucida Grande
     * atlas as the rest of Desktop Mode instead of stale placeholder text. */
    if (dm_dashboard_weather.available)
    {
        char location[16];
        char *comma;

        rb->strlcpy(location, dm_dashboard_weather.location,
                    sizeof(location));
        comma = rb->strchr(location, ',');
        if (comma)
            *comma = '\0';
        if (dm_dashboard_weather.icon_valid)
            dm_blit_bitmap(&dm_dashboard_weather.icon_bitmap,
                           weather_x + 8, top_y + 18, true);
        dm_draw_text_centered_in_box(small, weather_x + 4, top_y + 3,
                                     88, 12, DM_INK_WHITE, location);
        dm_draw_text_scaled_centered_in_box(&dm_fonts[DM_FONT_BOLD],
                            weather_x + 46, top_y + 14, 54, 30, 2,
                            DM_INK_WHITE, dm_dashboard_weather.temperature);
        dm_draw_text_centered_in_box(small, weather_x + 46, top_y + 43,
                            54, 13,
                            DM_INK_WHITE, dm_dashboard_weather.condition);
    }
    else
        dm_draw_text_centered_in_box(small, weather_x, top_y + 20, 104, 17,
                                     DM_INK_WHITE, "Sync Weather");

    dm_draw_text_in_box(&dm_fonts[DM_FONT_BOLD], sticky_x + 11,
                        lower_y + 11, 74, 15, LCD_RGBPACK(55, 50, 24),
                        "Desktop");
    rb->snprintf(value, sizeof(value), "%d songs", dm_dashboard_track_total);
    dm_draw_text_in_box(small, sticky_x + 11, lower_y + 31, 74,
                        small->cell_h, LCD_RGBPACK(70, 63, 28), value);
    dm_draw_text_in_box(small, sticky_x + 11, lower_y + 49, 74,
                        small->cell_h, LCD_RGBPACK(70, 63, 28),
                        "Select LCD");
    dm_draw_text_in_box(small, sticky_x + 11, lower_y + 62, 74,
                        small->cell_h, LCD_RGBPACK(70, 63, 28),
                        "for iTunes");

    if (track)
    {
        dm_draw_text_in_box(small, player_x + 77,
                            lower_y + 14, 103, 12, LCD_RGBPACK(48, 61, 9),
                            track->title ? track->title : track->path);
        rb->snprintf(value, sizeof(value), "%s%s%s",
                     track->artist ? track->artist : "",
                     track->artist && track->album ? " - " : "",
                     track->album ? track->album : "");
        dm_draw_text_in_box(small, player_x + 77, lower_y + 29, 103,
                            small->cell_h, LCD_RGBPACK(62, 75, 15), value);
        if (track->length > 0)
            progress = 101 * track->elapsed / track->length;
        rb->snprintf(value, sizeof(value), "%d:%02d / %d:%02d",
                     (int)(track->elapsed / 60000),
                     (int)((track->elapsed / 1000) % 60),
                     (int)(track->length / 60000),
                     (int)((track->length / 1000) % 60));
    }
    else
    {
        dm_draw_text_centered_in_box(small, player_x + 75,
                                     lower_y + 18, 106, 20,
                                     LCD_RGBPACK(48, 61, 9),
                                     "No music playing");
        rb->strlcpy(value, "--:-- / --:--", sizeof(value));
    }
    dm_fill_rect((struct dm_rect){ player_x + 77, lower_y + 49, 101, 2 },
                 LCD_RGBPACK(108, 127, 34));
    if (progress > 0)
        dm_fill_rect((struct dm_rect){ player_x + 77, lower_y + 49,
                                      progress, 2 },
                     LCD_RGBPACK(59, 77, 8));
    dm_draw_text_centered_in_box(small, player_x + 75, lower_y + 54, 106, 11,
                                 LCD_RGBPACK(62, 75, 15), value);
    if ((rb->audio_status() & AUDIO_STATUS_PLAY) != 0 &&
        (rb->audio_status() & AUDIO_STATUS_PAUSE) == 0)
        dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_BOLD], player_x + 29,
                                     lower_y + 34, 23, 16,
                                     LCD_RGBPACK(110, 110, 110), "||");
}

static void dm_draw_preferences(const struct dm_state *state)
{
    static const char * const labels[] =
    {
        "Mouse speed",
        "Reverse wheel direction",
        "Drag lock",
        "Show desktop folders",
        "Start application",
        "Restore last session"
    };
    static const char * const desktop_labels[] =
    {
        "Default / Restore Default", "Choose Image...", "Scaling",
        "Recent Wallpapers...", "Auto Activate", "Display Mode"
    };
    const struct dm_font *regular = &dm_fonts[DM_FONT_REGULAR];
    char value[24];
    int i;

    dm_blit_window(DM_ASSET_WINDOW_PLAIN);
    dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_BOLD], DM_WIN_X, DM_WIN_Y,
                                 DM_WIN_W, DM_WIN_TITLE_H, DM_INK_TITLE,
                                 "System Preferences");
    /* Clear captured Finder toolbar controls before presenting settings tabs. */
    dm_blit_asset_region(DM_ASSET_WINDOW_PLAIN, DM_WIN_X + 1, DM_WIN_Y + 24,
                         DM_CHROME_W - 20, 24, 1, 29, DM_WIN_W - 2, 29);
    dm_blit_scaled(DM_ASSET_SIDEBAR_SELECTION,
                   DM_WIN_X + (dm_desktop_preferences ? DM_WIN_W / 2 : 4),
                   DM_WIN_Y + 28, DM_WIN_W / 2 - 8, 20);
    dm_draw_text_in_box(regular, DM_WIN_X + 10, DM_WIN_Y + 26,
                        DM_WIN_W / 2 - 10, 22,
                        dm_desktop_preferences ? DM_INK_STATUS : DM_INK_WHITE,
                        "Mouse & Session");
    dm_draw_text_in_box(regular, DM_WIN_X + DM_WIN_W / 2, DM_WIN_Y + 26,
                        DM_WIN_W / 2 - 10, 22,
                        dm_desktop_preferences ? DM_INK_WHITE : DM_INK_STATUS,
                        "Desktop");
    dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_SMALL], DM_WIN_X,
                                 DM_STATUS_Y, DM_WIN_W, DM_WIN_STATUS_H,
                                 DM_INK_STATUS, dm_wallpaper_status);
    for (i = 0; i < (int)ARRAYLEN(labels); i++)
    {
        int y = DM_BODY_Y + i * DM_PREF_ROW_H;
        fb_data ink = DM_INK;

        if (i >= DM_PREF_ROWS)
            break;
        if (state->preference_row == i)
        {
            dm_blit_part(DM_ASSET_LIST_SELECTION, DM_WIN_X + 6, y,
                         DM_WIN_W - 12);
            ink = DM_INK_WHITE;
        }
        dm_draw_text_in_box(regular, DM_WIN_X + 14, y, 190, DM_PREF_ROW_H,
                            ink, dm_desktop_preferences ? desktop_labels[i] : labels[i]);
        if (dm_desktop_preferences)
        {
            static const char * const scales[] = { "Fill", "Fit", "Center" };
            static const char * const modes[] = { "Off", "Mirror", "Desktop" };
            static const char * const automatic[] =
                { "Off", "Display", "Input", "Full Setup" };
            rb->strlcpy(value, i == 2 ? scales[dm_wallpaper_scale] :
                        i == 4 ? automatic[dm_auto_activate] :
                        i == 5 ? modes[dm_display_mode] : "", sizeof(value));
        }
        else if (i == 0)
            rb->snprintf(value, sizeof(value), "%d",
                         dm_settings.pointer_speed);
        else if (i == 1)
            rb->strlcpy(value, dm_settings.reverse_wheel ? "On" : "Off",
                        sizeof(value));
        else if (i == 2)
            rb->strlcpy(value, dm_settings.drag_lock ? "On" : "Off",
                        sizeof(value));
        else if (i == 3)
            rb->strlcpy(value,
                        dm_settings.show_desktop_folders ? "On" : "Off",
                        sizeof(value));
        else if (i == 4)
            rb->strlcpy(value, dm_start_app_name(dm_settings.start_app),
                        sizeof(value));
        else
            rb->strlcpy(value, dm_settings.restore_session ? "On" : "Off",
                        sizeof(value));
        dm_draw_text_right_in_box(regular, DM_WIN_X + DM_WIN_W - 16, y,
                                  DM_PREF_ROW_H, 80, ink, value);
    }
}

static struct dm_rect dm_photo_pin_rect(int index)
{
    return (struct dm_rect){ DM_SHEET_X + 12 + (index % 6) * 36,
                            DM_SHEET_Y + 46 + (index / 6) * 26, 34, 24 };
}

static void dm_draw_photo_pin(void)
{
    dm_blit(DM_ASSET_SHEET, DM_SHEET_X, DM_SHEET_Y);
    dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_BOLD], DM_SHEET_X,
        DM_SHEET_Y + 3, DM_SHEET_W, 20, DM_INK, dm_photo_prompt);
    char masked[5] = "----";
    for (size_t i = 0; i < rb->strlen(dm_photo_entered); i++) masked[i] = '*';
    dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_REGULAR], DM_SHEET_X,
        DM_SHEET_Y + 23, DM_SHEET_W, 20, DM_INK, masked);
    for (int i = 0; i < 12; i++)
    {
        char digit[2] = { '0' + i, 0 };
        struct dm_rect rect = dm_photo_pin_rect(i);
        dm_fill_rect(rect, LCD_RGBPACK(225, 229, 235));
        dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_SMALL], rect.x,
            rect.y, rect.width, rect.height, DM_INK,
            i == 10 ? "Clear" : i == 11 ? "OK" : digit);
    }
}

static void dm_draw_recent_wallpapers(void)
{
    dm_blit(DM_ASSET_SHEET, DM_SHEET_X, DM_SHEET_Y);
    dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_BOLD], DM_SHEET_X,
                                 DM_SHEET_Y + 3, DM_SHEET_W, 20, DM_INK,
                                 "Recent Wallpapers");
    for (int i = 0; i < 4; i++)
    {
        const char *base = rb->strrchr(dm_wallpaper_recent[i], '/');
        dm_draw_text_in_box(&dm_fonts[DM_FONT_SMALL], DM_SHEET_X + 10,
                            DM_SHEET_Y + 26 + i * 18, DM_SHEET_W - 20, 18,
                            DM_INK, base ? base + 1 : "");
    }
}

static void dm_draw_apple_menu(const struct dm_state *state)
{
    int i;

    dm_blit(DM_ASSET_MENU_PANEL, DM_MENU_X, DM_MENU_Y);
    for (i = 0; i < DM_APPLE_MENU_ROWS; i++)
    {
        int y = DM_MENU_Y + DM_MENU_TOP + i * DM_MENU_ROW_H;
        fb_data ink = DM_INK;

        if (state->menu_row == i)
        {
            dm_blit(DM_ASSET_MENU_SELECTION, DM_MENU_X + 1, y);
            ink = DM_INK_WHITE;
        }
        dm_draw_text_in_box(&dm_fonts[DM_FONT_REGULAR], DM_MENU_X + 10, y,
                            DM_MENU_W - 20, DM_MENU_ROW_H, ink,
                            dm_apple_menu_labels[i]);
    }
}

static int dm_app_menu_panel_x(const struct dm_state *state)
{
    struct dm_rect item = dm_menu_bar_rect(state, state->app_menu);

    return MAX(2, MIN(item.x, LCD_WIDTH - DM_MENU_W - 2));
}

static void dm_draw_app_menu(const struct dm_state *state)
{
    int panel_x = dm_app_menu_panel_x(state);
    int i;

    dm_blit(DM_ASSET_MENU_PANEL, panel_x, DM_MENU_Y);
    for (i = 0; i < DM_APP_MENU_COUNT; i++)
    {
        int y = DM_MENU_Y + DM_MENU_TOP + i * DM_MENU_ROW_H;
        fb_data ink = DM_INK;

        if (state->menu_row == i)
        {
            dm_blit(DM_ASSET_MENU_SELECTION, panel_x + 1, y);
            ink = DM_INK_WHITE;
        }
        dm_draw_text_in_box(&dm_fonts[DM_FONT_REGULAR], panel_x + 10, y,
                            DM_MENU_W - 20, DM_MENU_ROW_H, ink,
                            dm_app_menu_labels[state->app_menu][i]);
    }
}

static void dm_draw_finder_context_menu(const struct dm_state *state)
{
    int i;

    dm_blit(DM_ASSET_CONTEXT_PANEL, DM_CONTEXT_X, DM_CONTEXT_Y);
    for (i = 0; i < DM_CONTEXT_ROWS; i++)
    {
        int y = DM_CONTEXT_Y + DM_MENU_TOP + i * DM_MENU_ROW_H;
        fb_data ink = DM_INK;

        if (state->menu_row == i)
        {
            dm_blit_part(DM_ASSET_MENU_SELECTION, DM_CONTEXT_X + 1, y,
                         DM_CONTEXT_W - 2);
            ink = DM_INK_WHITE;
        }
        dm_draw_text_in_box(&dm_fonts[DM_FONT_REGULAR], DM_CONTEXT_X + 10, y,
                            DM_CONTEXT_W - 20, DM_MENU_ROW_H, ink,
                            dm_context_labels[i]);
    }
}

static void dm_draw_sheet_row(int row, const char *label, const char *value)
{
    int y = DM_SHEET_Y + 34 + row * 16;

    dm_draw_text_in_box(&dm_fonts[DM_FONT_SMALL], DM_SHEET_X + 16, y, 66,
                        16, DM_INK_STATUS, label);
    dm_draw_text_in_box(&dm_fonts[DM_FONT_SMALL], DM_SHEET_X + 84, y,
                        DM_SHEET_W - 100, 16, DM_INK, value);
}

static void dm_draw_get_info(void)
{
    const char *name = "No selection";
    char size[32] = "--";
    char modified[32] = "--";
    struct tm tm;
    bool is_dir = false;

    if (dm_file_selected >= 0 && dm_file_selected < dm_file_count)
    {
        name = dm_files[dm_file_selected].name;
        is_dir = dm_files[dm_file_selected].is_dir;
        if (!is_dir)
            rb->snprintf(size, sizeof(size), "%ld bytes",
                         (long)dm_files[dm_file_selected].size);
        if (rb->gmtime_r(&dm_files[dm_file_selected].mtime, &tm))
            rb->snprintf(modified, sizeof(modified), "%04d-%02d-%02d",
                         tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    }
    dm_blit(DM_ASSET_SHEET, DM_SHEET_X, DM_SHEET_Y);
    dm_compose(is_dir ? DM_ASSET_ICON_FOLDER_DESKTOP : DM_ASSET_ICON_DOCUMENT,
               DM_SHEET_X + 12, DM_SHEET_Y + 8);
    dm_draw_text_in_box(&dm_fonts[DM_FONT_BOLD], DM_SHEET_X + 52,
                        DM_SHEET_Y + 8, DM_SHEET_W - 64, 22, DM_INK, name);
    dm_draw_sheet_row(0, "Kind:", is_dir ? "Folder" : "Document");
    dm_draw_sheet_row(1, "Size:", size);
    dm_draw_sheet_row(2, "Modified:", modified);
    dm_draw_sheet_row(3, "Where:", dm_cwd);
}

static const char *dm_error_name(enum dm_error error)
{
    switch (error)
    {
        case DM_ERR_NONE:
            return "none";
        case DM_ERR_ASSET_MISSING:
            return "asset missing";
        case DM_ERR_ASSET_INVALID:
            return "asset invalid";
        case DM_ERR_PLUGIN_BUFFER:
            return "plugin buffer";
        case DM_ERR_ARENA_OVERFLOW:
            return "arena overflow";
        case DM_ERR_CONTROL_OVERFLOW:
            return "control overflow";
    }
    return "unknown";
}

static void dm_draw_diagnostics(const struct dm_state *state)
{
    char memory[32];
    char controls[32];
    char frame[32];
    char updates[32];

    rb->snprintf(memory, sizeof(memory), "%lu / %lu KiB",
                 (unsigned long)(dm_arena_total - dm_arena_left) / 1024,
                 (unsigned long)dm_arena_total / 1024);
    rb->snprintf(controls, sizeof(controls), "%d now, %d peak",
                 state->controls->count, state->controls->high_water);
    rb->snprintf(frame, sizeof(frame), "%ld / %ld ticks",
                 state->last_frame_ticks, state->worst_frame_ticks);
    rb->snprintf(updates, sizeof(updates), "%lu full, %lu partial",
                 state->full_update_count, state->partial_update_count);
    dm_blit(DM_ASSET_SHEET, DM_SHEET_X, DM_SHEET_Y);
    dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_BOLD], DM_SHEET_X,
                                 DM_SHEET_Y + 7, DM_SHEET_W, 20, DM_INK,
                                 "Desktop Diagnostics");
    dm_draw_sheet_row(0, "Memory:", memory);
    dm_draw_sheet_row(1, "Controls:", controls);
    dm_draw_sheet_row(2, "Frame:", frame);
    dm_draw_sheet_row(3, "Updates:", updates);
    dm_draw_sheet_row(4, "Error:", dm_error_name(dm_last_error));
}

static const char *dm_notice_text;

static void dm_notice(struct dm_state *state, const char *message)
{
    dm_notice_text = message;
#ifdef SIMULATOR
    DEBUGF("desktop notice: %s\n", message);
#endif
    state->overlay = DM_OVERLAY_NOTICE;
    state->redraw = state->damage_full = true;
}

static void dm_draw_confirm(const struct dm_state *state)
{
    const char *title = "Return to the iPod menu?";

    if (state->overlay == DM_OVERLAY_NOTICE)
        title = dm_notice_text;
    else if (state->overlay == DM_OVERLAY_RESTART_CONFIRM)
        title = "Restart Rockbox now?";
    else if (state->overlay == DM_OVERLAY_DELETE_CONFIRM)
        title = "Delete the selected item?";
    else if (state->overlay == DM_OVERLAY_NEW_FOLDER_CONFIRM)
        title = "Create an untitled folder?";
    dm_blit(DM_ASSET_SHEET, DM_SHEET_X, DM_SHEET_Y);
    dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_BOLD], DM_SHEET_X,
                                 DM_SHEET_Y + 27, DM_SHEET_W, 28, DM_INK,
                                 title);
    dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_SMALL], DM_SHEET_X,
                                 DM_SHEET_Y + 56, DM_SHEET_W, 23,
                                 DM_INK_STATUS, state->overlay == DM_OVERLAY_NOTICE ?
                                 "Select or Menu dismisses." :
                                 "Select confirms.  Menu cancels.");
}

static bool dm_pointer_is_hand(const struct dm_state *state)
{
    int index = state->controls->hover_index;

    return index >= 0 && index < state->controls->count &&
           (state->controls->controls[index].flags & DM_CONTROL_HAND) != 0;
}

static void dm_draw_cursor(const struct dm_state *state)
{
    dm_compose(dm_pointer_is_hand(state) ? DM_ASSET_CURSOR_HAND :
                                           DM_ASSET_CURSOR_ARROW,
               state->cursor_x, state->cursor_y);
}

/* Keep at most two inactive windows visible behind the front window.  They
 * reuse the loaded plain chrome and paint through the current damage clip;
 * there is no per-window framebuffer or content redraw. */
static void dm_draw_inactive_windows(struct dm_state *state)
{
    int i;

    if (state->fullscreen || state->app == DM_APP_DESKTOP)
        return;
    for (i = 0; i < state->window_stack_count; i++)
    {
        enum dm_app app = state->window_stack[i];

        if (app == state->app || app == state->minimized_app)
            continue;
        dm_sync_window_geometry(state, app);
        dm_blit_window(DM_ASSET_WINDOW_PLAIN);
        dm_fill_rect((struct dm_rect)
        {
            DM_WIN_X + 5, DM_WIN_Y + 3, DM_WIN_W - 10, DM_WIN_TITLE_H - 6
        }, LCD_RGBPACK(208, 211, 216));
        dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_BOLD], DM_WIN_X,
                                     DM_WIN_Y, DM_WIN_W, DM_WIN_TITLE_H,
                                     LCD_RGBPACK(105, 108, 113),
                                     dm_app_name(app));
    }
    dm_sync_window_geometry(state, state->app);
}

static void dm_draw(struct dm_state *state)
{
    uint32_t signature;
    long started = *rb->current_tick;
    bool full;

    dm_fullscreen = state->fullscreen;
    dm_sync_window_geometry(state, state->app);
    dm_update_hover(state);
    signature = dm_scene_signature(state);
    full = state->damage_full || state->frame_count == 0 ||
           signature != state->scene_signature;
    full = dm_prepare_damage(state, full);
    dm_blit(DM_ASSET_AURORA, 0, 0);
    if (!state->fullscreen)
        dm_draw_desktop_icons(state);
    dm_draw_inactive_windows(state);
    if (state->animation.active)
        dm_draw_animation(state);
    else if (state->app == DM_APP_FINDER)
        dm_draw_finder(state);
    else if (state->app == DM_APP_ITUNES)
        dm_draw_itunes();
    else if (state->app == DM_APP_PREVIEW)
        dm_draw_photos();
    else if (state->app == DM_APP_TEXTEDIT)
        dm_draw_textedit();
    else if (state->app == DM_APP_CALCULATOR)
        dm_draw_calculator();
    else if (state->app == DM_APP_PREFERENCES)
        dm_draw_preferences(state);
    else if (state->app == DM_APP_LAUNCHPAD)
        dm_draw_launchpad(state);
    else if (state->app == DM_APP_DASHBOARD)
        dm_draw_dashboard();
    dm_draw_menu_bar(state);
    /* Dashboard is a dedicated desktop layer. Ordinary windows, including
     * the compact iTunes window, retain the Dock. */
    if (!state->fullscreen && state->app != DM_APP_DASHBOARD)
        dm_draw_dock(state);
    if (!state->animation.active)
    {
        if (state->overlay == DM_OVERLAY_PHOTO_PIN)
            dm_draw_photo_pin();
        else if (state->overlay == DM_OVERLAY_WALLPAPER_RECENT)
            dm_draw_recent_wallpapers();
        else if (state->overlay == DM_OVERLAY_APPLE_MENU)
            dm_draw_apple_menu(state);
        else if (state->overlay == DM_OVERLAY_APP_MENU)
            dm_draw_app_menu(state);
        else if (state->overlay == DM_OVERLAY_FINDER_CONTEXT)
            dm_draw_finder_context_menu(state);
        else if (state->overlay == DM_OVERLAY_GET_INFO)
            dm_draw_get_info();
        else if (state->overlay == DM_OVERLAY_DIAGNOSTICS)
            dm_draw_diagnostics(state);
        else if (state->overlay == DM_OVERLAY_NOTICE ||
                 state->overlay == DM_OVERLAY_RETURN_CONFIRM ||
                 state->overlay == DM_OVERLAY_RESTART_CONFIRM ||
                 state->overlay == DM_OVERLAY_DELETE_CONFIRM ||
                 state->overlay == DM_OVERLAY_NEW_FOLDER_CONFIRM)
            dm_draw_confirm(state);
    }
    if (!state->wheel_available && !state->host_pointer &&
        TIME_BEFORE(*rb->current_tick, state->hint_until))
    {
        static const char hint[] = "Wheel up/down, Prev/Next left/right";
        int width = MIN(LCD_WIDTH - 8,
                        dm_text_width(&dm_fonts[DM_FONT_SMALL], hint) + 16);
        int x = (LCD_WIDTH - width) / 2;

        dm_blit_panel(DM_ASSET_TOOLTIP, x, DM_MENUBAR_H + 6, width,
                      DM_TOOLTIP_CAP);
        dm_draw_text_centered_in_box(&dm_fonts[DM_FONT_SMALL], x,
                                     DM_MENUBAR_H + 6, width, DM_TOOLTIP_H,
                                     DM_INK, hint);
    }
    dm_draw_cursor(state);
    dm_present(state, full);
    state->last_frame_ticks = *rb->current_tick - started;
    state->worst_frame_ticks = MAX(state->worst_frame_ticks,
                                   state->last_frame_ticks);
    state->frame_count++;
    state->scene_signature = signature;
    state->damage_full = false;
}

static void dm_scan_directory(void);

static void dm_join_path(char *output, size_t size, const char *directory,
                         const char *name)
{
    if (!rb->strcmp(directory, "/"))
        rb->snprintf(output, size, "/%s", name);
    else
        rb->snprintf(output, size, "%s/%s", directory, name);
}

static void dm_history_reset(void)
{
    dm_history_count = 1;
    dm_history_index = 0;
    rb->strlcpy(dm_history[0], dm_cwd, sizeof(dm_history[0]));
}

static void dm_history_push(const char *path)
{
    int i;

    if (!rb->strcmp(path, dm_cwd))
        return;
    dm_history_count = dm_history_index + 1;
    if (dm_history_count == DM_HISTORY_COUNT)
    {
        for (i = 1; i < dm_history_count; i++)
            rb->strlcpy(dm_history[i - 1], dm_history[i],
                        sizeof(dm_history[i - 1]));
        dm_history_count--;
        dm_history_index--;
    }
    rb->strlcpy(dm_history[dm_history_count], path,
                sizeof(dm_history[dm_history_count]));
    dm_history_index = dm_history_count;
    dm_history_count++;
}

static bool dm_navigate(const char *path)
{
    if (dm_wallpaper_picker && dm_photo_access(path, true, NULL) != 0)
        return false;
    if (!rb->dir_exists(path))
        return false;
    dm_history_push(path);
    rb->strlcpy(dm_cwd, path, sizeof(dm_cwd));
    dm_scan_directory();
    return true;
}

static bool dm_history_move(int direction)
{
    int next = dm_history_index + direction;

    if (next < 0 || next >= dm_history_count || dm_wallpaper_picker)
        return false;
    dm_history_index = next;
    rb->strlcpy(dm_cwd, dm_history[dm_history_index], sizeof(dm_cwd));
    dm_scan_directory();
    return true;
}

static int dm_file_compare(const struct dm_file *left,
                           const struct dm_file *right)
{
    if (left->is_dir != right->is_dir)
        return left->is_dir ? -1 : 1;
    return rb->strcasecmp(left->name, right->name);
}

static void dm_sort_files(void)
{
    int i;

    for (i = 1; i < dm_file_count; i++)
    {
        int j = i;

        while (j > 0 &&
               dm_file_compare(&dm_files[j], &dm_files[j - 1]) < 0)
        {
            struct dm_file temporary = dm_files[j];

            dm_files[j] = dm_files[j - 1];
            dm_files[j - 1] = temporary;
            j--;
        }
    }
}

static void dm_scan_directory(void)
{
    DIR *directory;
    struct dirent *entry;

    dm_file_count = 0;
    dm_file_selected = 0;
    dm_file_top = 0;
    directory = rb->opendir(dm_cwd);
    if (!directory)
        return;
    while ((entry = rb->readdir(directory)) != NULL &&
           dm_file_count < DM_MAX_FILES)
    {
        struct dirinfo info;

        if (!rb->strcmp(entry->d_name, ".") ||
            !rb->strcmp(entry->d_name, ".."))
            continue;
        info = rb->dir_get_info(directory, entry);
        if (dm_wallpaper_picker && (entry->d_name[0] == '.' ||
            (!(info.attribute & ATTR_DIRECTORY) &&
             !photo_library_supported(entry->d_name)))) continue;
        rb->strlcpy(dm_files[dm_file_count].name, entry->d_name,
                    sizeof(dm_files[dm_file_count].name));
        dm_files[dm_file_count].is_dir =
            (info.attribute & ATTR_DIRECTORY) != 0;
        dm_files[dm_file_count].size = info.size;
        dm_files[dm_file_count].mtime = info.mtime;
        dm_file_count++;
    }
    rb->closedir(directory);
    dm_sort_files();
}

static void dm_start_desktop_playlist(int index)
{
    if (rb->global_settings->playlist_shuffle)
        index = rb->playlist_shuffle(*rb->current_tick, index);
    rb->playlist_start(index, 0, 0);
}

/* Play a track chosen in the iTunes window.
 *
 * This is the one place Desktop Mode builds a playlist, and it does so only
 * because the user asked for this track by name: it is the whole point of a
 * music application.  Everything else in the shell still leaves the user's
 * playlist alone - iTunes reports playback rather than commanding it, and
 * nothing here runs unless a track was double-clicked.
 */
static int dm_play_selected_track(struct dm_state *state)
{
    char path[MAX_PATH];
    int index = 0;
    int count = 0;
    int i;

    if (dm_file_selected < 0 || dm_file_selected >= dm_file_count ||
        dm_files[dm_file_selected].is_dir)
        return PLUGIN_OK;

    if (rb->playlist_create(dm_cwd, NULL) < 0)
    {
        dm_notice(state, "Could not start playback");
        return PLUGIN_OK;
    }
    /* Queue the whole folder so the album keeps playing, and begin at the
     * track that was chosen. */
    for (i = 0; i < dm_file_count; i++)
    {
        if (dm_files[i].is_dir || !dm_media_match(DM_MEDIA_MUSIC,
                                                  dm_files[i].name))
            continue;
        dm_join_path(path, sizeof(path), dm_cwd, dm_files[i].name);
        if (rb->playlist_insert_track(NULL, path, PLAYLIST_INSERT_LAST,
                                      false, true) < 0)
            break;
        if (i == dm_file_selected)
            index = count;
        count++;
    }
    if (count == 0)
    {
        dm_notice(state, "No playable music");
        return PLUGIN_OK;
    }
    dm_start_desktop_playlist(index);
    return PLUGIN_OK;
}

static void dm_save_settings(void);

static void dm_photo_request(struct dm_state *state, const char *path,
                             bool directory)
{
    char selected[MAX_PATH];
    rb->strlcpy(selected, path, sizeof(selected));
    int access = selected[0] ?
        dm_photo_access(selected, true, dm_photo_expected) : 0;
    state->damage_full = state->redraw = true;
    if (access == 1)
    {
        rb->strlcpy(dm_photo_pending, selected, sizeof(dm_photo_pending));
        dm_photo_pending_dir = directory;
        dm_photo_entered[0] = 0;
        dm_photo_prompt = "Enter photo code";
        state->overlay = DM_OVERLAY_PHOTO_PIN;
        return;
    }
    if (access < 0)
    {
        rb->strlcpy(dm_wallpaper_status, "Photo access unavailable",
                    sizeof(dm_wallpaper_status));
        return;
    }
    if (directory) dm_navigate(selected);
    else if (dm_wallpaper_select(selected))
    {
        dm_save_settings();
        dm_wallpaper_picker = false;
        state->app = DM_APP_PREFERENCES;
    }
}

static void dm_photo_pin_input(struct dm_state *state, int value)
{
    size_t n = rb->strlen(dm_photo_entered);
    if (value >= 0 && value <= 9 && n < 4)
    {
        dm_photo_entered[n] = '0' + value;
        dm_photo_entered[n + 1] = 0;
    }
    else if (value == 10) rb->memset(dm_photo_entered, 0, 5);
    else if (value == 11)
    {
        if (rb->strcmp(dm_photo_entered, dm_photo_expected))
        {
            dm_photo_prompt = "Wrong code. Try again";
            rb->memset(dm_photo_entered, 0, 5);
        }
        else if (dm_photo_code_count < (int)ARRAYLEN(dm_photo_codes))
        {
            rb->strlcpy(dm_photo_codes[dm_photo_code_count++],
                        dm_photo_entered, 5);
            rb->memset(dm_photo_entered, 0, 5);
            rb->memset(dm_photo_expected, 0, 5);
            state->overlay = DM_OVERLAY_NONE;
            /* Re-read all locks before opening, including ancestor locks. */
            dm_photo_request(state, dm_photo_pending, dm_photo_pending_dir);
        }
        else dm_photo_prompt = "Session full. Reopen Desktop";
    }
    state->redraw = state->damage_full = true;
}

static int dm_open_selected_file(struct dm_state *state)
{
    /* Picker activation must use dm_photo_request, even from an app menu. */
    if (dm_wallpaper_picker) return PLUGIN_OK;
    char path[MAX_PATH];
    char plugin[MAX_PATH];
    int attribute;
    int length;
    int result = PLUGIN_OK;

    if (dm_file_selected < 0 || dm_file_selected >= dm_file_count)
        return PLUGIN_OK;
    if (dm_files[dm_file_selected].name[0] == '/')
        rb->strlcpy(path, dm_files[dm_file_selected].name, sizeof(path));
    else
        dm_join_path(path, sizeof(path), dm_cwd,
                     dm_files[dm_file_selected].name);
    if (dm_files[dm_file_selected].is_dir)
    {
        dm_navigate(path);
        return PLUGIN_OK;
    }

    attribute = rb->filetype_get_attr(path);
    if ((attribute & FILE_ATTR_MASK) == FILE_ATTR_AUDIO)
        return dm_play_selected_track(state);

    length = rb->strlen(path);
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
    if (length > 5 && !rb->strcasecmp(path + length - 5, ".rock"))
    {
        const char *parameter =
            !rb->strcasecmp(dm_files[dm_file_selected].name, "livetv.rock") ?
                "-desktop" : NULL;

        result = rb->plugin_open(path, parameter);
    }
    else
    {
        if (rb->filetype_get_plugin(attribute, plugin, sizeof(plugin)))
            result = rb->plugin_open(plugin, path);
        else
            dm_notice(state, "No viewer");
    }
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(false);
#endif
    return result;
}

/* Start the actual files represented by the current iTunes library row.
 * Songs queue the on-device library and start at the chosen track. Albums
 * and Artists use the tagcache seek captured with the visible row, then queue
 * only matching files. Videos hand their absolute device path to Rockbox's
 * registered MPEG/OpenH264 viewer.
 */
static bool dm_itunes_back(void)
{
    if (!desktop_library_back(&dm_library)) return false;
    dm_itunes_source = (enum dm_itunes_source)dm_library.current.source;
    dm_itunes_page_top = dm_library.current.page;
    dm_itunes_search[0] = 0;
    dm_itunes_playlist[0] = 0;
    dm_itunes_load();
    dm_file_selected = MIN(dm_itunes_count - 1, dm_library.current.selected);
    return true;
}

/* Prepare the complete current result before replacing live playback. The
 * file is temporary: insertion copies its entries into Rockbox's dynamic
 * playlist, so a later selection cannot rewrite the playing queue. */
#define DM_QUEUE_FILE PLUGIN_APPS_DATA_DIR "/desktop_queue.m3u8"
static NO_INLINE int dm_itunes_prepare_queue(const struct dm_itunes_row *selected)
{
    static struct dm_itunes_row candidate;
    char line[MAX_PATH + 128];
    int count = 0, start = -1, matched = 0, scanned = 0;
    int output = rb->open(DM_QUEUE_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    int input = -1;
    bool ok = output >= 0;
#ifdef HAVE_TAGCACHE
    static struct tagcache_search search;
    bool searching = false;
#endif
    if (!ok) return -1;
    if (dm_itunes_playlist[0])
    {
        input = rb->open(dm_itunes_playlist, O_RDONLY);
        ok = input >= 0;
    }
    else
    {
#ifdef HAVE_TAGCACHE
        searching = rb->tagcache_search(&search, tag_title);
        ok = searching && dm_itunes_filters(&search);
#else
        ok = false;
#endif
    }
    while (ok)
    {
        if ((++scanned & 31) == 0) rb->yield();
        bool is_selected;
        rb->memset(&candidate, 0, sizeof(candidate));
        if (input >= 0)
        {
            int length = rb->read_line(input, line, sizeof(line));
            if (length <= 0) { ok = length == 0; break; }
            if (!line[0] || line[0] == '#') continue;
            if (line[0] == '/')
                rb->strlcpy(candidate.path, line, sizeof(candidate.path));
            else
            {
                rb->strlcpy(candidate.path, dm_itunes_playlist,
                            sizeof(candidate.path));
                char *slash = rb->strrchr(candidate.path, '/');
                if (slash) slash[1] = 0;
                rb->strlcat(candidate.path, line, sizeof(candidate.path));
            }
            const char *base = rb->strrchr(line, '/');
            rb->strlcpy(candidate.title, base ? base + 1 : line,
                        sizeof(candidate.title));
            if (!dm_itunes_row_matches(&candidate)) continue;
            is_selected = ++matched == selected->seek;
        }
        else
        {
#ifdef HAVE_TAGCACHE
            if (!rb->tagcache_get_next(&search, candidate.title,
                                       sizeof(candidate.title))) break;
            rb->tagcache_retrieve(&search, search.idx_id, tag_filename,
                                  candidate.path, sizeof(candidate.path));
            rb->tagcache_retrieve(&search, search.idx_id, tag_artist,
                                  candidate.artist, sizeof(candidate.artist));
            rb->tagcache_retrieve(&search, search.idx_id, tag_album,
                                  candidate.album, sizeof(candidate.album));
            if (!dm_itunes_row_matches(&candidate)) continue;
            is_selected = search.idx_id == selected->idxid;
#else
            break;
#endif
        }
        if (!candidate.path[0] || !rb->file_exists(candidate.path)) continue;
        size_t length = rb->strlen(candidate.path);
        ok = rb->write(output, candidate.path, length) == (ssize_t)length &&
             rb->write(output, "\n", 1) == 1;
        if (is_selected) start = count;
        count++;
    }
#ifdef HAVE_TAGCACHE
    if (searching) rb->tagcache_search_finish(&search);
#endif
    if (input >= 0) rb->close(input);
    if (rb->close(output) < 0) ok = false;
    return ok && count > 0 ? start : -1;
}

static int dm_open_itunes_selection(struct dm_state *state)
{
    if (dm_file_selected < 0 || dm_file_selected >= dm_itunes_count)
        return PLUGIN_OK;
    if (dm_itunes_source == DM_ITUNES_VIDEOS)
        return dm_open_selected_file(state);
    const struct dm_itunes_row *selected = &dm_itunes_rows[dm_file_selected];
    dm_library.current.page = dm_itunes_page_top;
    dm_library.current.selected = dm_file_selected;
    if (desktop_library_open(&dm_library, selected->seek))
    {
        if (dm_itunes_source == DM_ITUNES_PLAYLISTS)
            rb->strlcpy(dm_itunes_playlist, selected->path,
                        sizeof(dm_itunes_playlist));
        rb->strlcpy(dm_library_title[dm_library.depth], selected->title,
                    sizeof(dm_library_title[0]));
        dm_itunes_source = (enum dm_itunes_source)dm_library.current.source;
        dm_itunes_page_top = 0;
        dm_itunes_search[0] = 0;
        dm_itunes_load();
        return PLUGIN_OK;
    }
    if (!selected->path[0] || !rb->file_exists(selected->path))
    {
        dm_notice(state, "Music file is missing");
        return PLUGIN_OK;
    }
    int start_index = dm_itunes_prepare_queue(selected);
    if (start_index < 0)
        dm_notice(state, "Could not prepare music queue");
    else if (rb->playlist_create(NULL, NULL) < 0 ||
             rb->playlist_insert_playlist(NULL, DM_QUEUE_FILE,
                                          PLAYLIST_INSERT_LAST, false) < 0)
        dm_notice(state, "Could not start playback");
    else
        dm_start_desktop_playlist(start_index);
    rb->remove(DM_QUEUE_FILE);
    return PLUGIN_OK;
}

static void dm_load_settings(void)
{
    char line[MAX_PATH + 64];
    int fd = rb->open(DM_CONFIG_FILE, O_RDONLY);

    if (fd < 0)
        return;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *name = NULL;
        char *value = NULL;

        if (!rb->settings_parseline(line, &name, &value) ||
            !name || !value)
            continue;
        if (!rb->strcmp(name, "wallpaper path"))
            rb->strlcpy(dm_wallpaper_path, value, sizeof(dm_wallpaper_path));
        else if (!rb->strcmp(name, "wallpaper scale"))
            dm_wallpaper_scale = MAX(0, MIN(2, rb->atoi(value)));
        else if (!rb->strcmp(name, "desktop auto activate"))
            dm_auto_activate = MAX(0, MIN(3, rb->atoi(value)));
        else if (!rb->strcmp(name, "desktop display mode"))
            dm_display_mode = MAX(0, MIN(2, rb->atoi(value)));
        else if (!rb->strncmp(name, "wallpaper recent ", 17))
        {
            int i = rb->atoi(name + 17);
            if (i >= 0 && i < 4)
                rb->strlcpy(dm_wallpaper_recent[i], value,
                            sizeof(dm_wallpaper_recent[i]));
        }
        else if (!rb->strcmp(name, "snow pointer speed"))
            dm_settings.pointer_speed =
                MAX(1, MIN(4, rb->atoi(value)));
        else if (!rb->strcmp(name, "snow reverse wheel"))
            dm_settings.reverse_wheel = !rb->strcasecmp(value, "on");
        else if (!rb->strcmp(name, "snow desktop folders"))
            dm_settings.show_desktop_folders =
                !rb->strcasecmp(value, "on");
        else if (!rb->strcmp(name, "snow drag lock"))
            dm_settings.drag_lock = !rb->strcasecmp(value, "on");
        else if (!rb->strcmp(name, "snow restore session"))
            dm_settings.restore_session = !rb->strcasecmp(value, "on");
        else if (!rb->strcmp(name, "snow start app"))
        {
            if (!rb->strcasecmp(value, "finder"))
                dm_settings.start_app = DM_APP_FINDER;
            else if (!rb->strcasecmp(value, "itunes"))
                dm_settings.start_app = DM_APP_ITUNES;
            else
                dm_settings.start_app = DM_APP_DESKTOP;
        }
        else if (!rb->strcmp(name, "snow last app"))
        {
            if (!rb->strcasecmp(value, "finder"))
                dm_saved_app = DM_APP_FINDER;
            else if (!rb->strcasecmp(value, "itunes"))
                dm_saved_app = DM_APP_ITUNES;
            else
                dm_saved_app = DM_APP_DESKTOP;
        }
        else if (!rb->strcmp(name, "snow cursor x"))
            dm_saved_cursor_x = MAX(0, MIN(LCD_WIDTH - 2, rb->atoi(value)));
        else if (!rb->strcmp(name, "snow cursor y"))
            dm_saved_cursor_y = MAX(0, MIN(LCD_HEIGHT - 2, rb->atoi(value)));
        else if (!rb->strcmp(name, "snow finder path") && value[0] == '/')
            rb->strlcpy(dm_cwd, value, sizeof(dm_cwd));
    }
    rb->close(fd);
}

static void dm_save_settings(void)
{
    char buffer[MAX_PATH + 512];
    int fd;
    int length;

    rb->mkdir(PLUGIN_APPS_DATA_DIR);
    fd = rb->open(DM_CONFIG_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    length = rb->snprintf(
        buffer, sizeof(buffer),
        "snow pointer speed: %d\n"
        "snow reverse wheel: %s\n"
        "snow drag lock: %s\n"
        "snow desktop folders: %s\n"
        "snow restore session: %s\n"
        "snow start app: %s\n"
        "snow last app: %s\n"
        "snow cursor x: %d\n"
        "snow cursor y: %d\n"
        "snow finder path: %s\n",
        dm_settings.pointer_speed,
        dm_settings.reverse_wheel ? "on" : "off",
        dm_settings.drag_lock ? "on" : "off",
        dm_settings.show_desktop_folders ? "on" : "off",
        dm_settings.restore_session ? "on" : "off",
        dm_settings.start_app == DM_APP_FINDER ? "finder" :
        dm_settings.start_app == DM_APP_ITUNES ? "itunes" : "desktop",
        dm_saved_app == DM_APP_FINDER ? "finder" :
        dm_saved_app == DM_APP_ITUNES ? "itunes" : "desktop",
        dm_saved_cursor_x,
        dm_saved_cursor_y,
        dm_photo_relative(dm_cwd) ? dm_photo_root : dm_cwd);
    rb->write(fd, buffer, MIN(length, (int)sizeof(buffer) - 1));
    rb->fdprintf(fd, "wallpaper path: %s\nwallpaper scale: %d\n"
                 "desktop auto activate: %d\ndesktop display mode: %d\n",
                 dm_wallpaper_path, dm_wallpaper_scale,
                 dm_auto_activate, dm_display_mode);
    for (int i = 0; i < 4; i++)
        rb->fdprintf(fd, "wallpaper recent %d: %s\n", i,
                     dm_wallpaper_recent[i]);
    rb->close(fd);
}

static uint32_t dm_control_id(enum dm_control_action action, int value)
{
    return ((uint32_t)action << 16) | ((unsigned int)value & 0xffffu);
}

static bool dm_rect_intersect(struct dm_rect *rect,
                              const struct dm_rect *clip)
{
    int right = MIN(rect->x + rect->width, clip->x + clip->width);
    int bottom = MIN(rect->y + rect->height, clip->y + clip->height);

    rect->x = MAX(rect->x, clip->x);
    rect->y = MAX(rect->y, clip->y);
    rect->width = right - rect->x;
    rect->height = bottom - rect->y;
    return rect->width > 0 && rect->height > 0;
}

static void dm_register_control(struct dm_state *state,
                                enum dm_control_action action, int value,
                                struct dm_rect bounds,
                                unsigned int flags)
{
    static const struct dm_rect screen = { 0, 0, LCD_WIDTH, LCD_HEIGHT };
    struct dm_control_registry *registry = state->controls;
    struct dm_control *control;

    if (!dm_rect_intersect(&bounds, &screen))
        return;
    if (registry->count >= DM_CONTROL_LIMIT)
    {
        dm_last_error = DM_ERR_CONTROL_OVERFLOW;
        return;
    }
    control = &registry->controls[registry->count++];
    control->id = dm_control_id(action, value);
    control->bounds = bounds;
    control->action = action;
    control->value = value;
    control->flags = flags;
    if (action != DM_ACTION_NONE && action != DM_ACTION_WINDOW_DRAG)
        control->flags |= DM_CONTROL_FOCUSABLE;
    registry->high_water = MAX(registry->high_water, registry->count);
}

static void dm_build_controls(struct dm_state *state)
{
    struct dm_control_registry *registry = state->controls;
    struct dm_rect window;
    int i;

    registry->count = 0;
    registry->hover_index = -1;
    dm_sync_window_geometry(state, state->app);
    if (state->animation.active)
        return;

    if (dm_wallpaper_picker && state->overlay == DM_OVERLAY_NONE)
    {
        for (i = 0; i < DM_FILE_ROWS && dm_file_top + i < dm_file_count; i++)
            dm_register_control(state, DM_ACTION_FILE_ROW, dm_file_top + i,
                (struct dm_rect){ DM_LIST_X, DM_BODY_Y + i * DM_ROW_H,
                                  DM_LIST_W, DM_ROW_H },
                DM_CONTROL_DOUBLE_CLICK | DM_CONTROL_FOCUSABLE);
        if (dm_file_count > DM_FILE_ROWS)
            dm_register_control(state, DM_ACTION_FINDER_SCROLL, 0,
                (struct dm_rect){ DM_SCROLLER_X, DM_BODY_Y,
                                  DM_SCROLLER_W, DM_SCROLLER_H },
                DM_CONTROL_HAND);
        return;
    }
    if (state->overlay == DM_OVERLAY_PHOTO_PIN)
    {
        for (i = 0; i < 12; i++)
            dm_register_control(state, DM_ACTION_PHOTO_PIN, i,
                                dm_photo_pin_rect(i), DM_CONTROL_HAND);
        return;
    }
    if (state->overlay == DM_OVERLAY_WALLPAPER_RECENT)
    {
        for (i = 0; i < 4; i++)
            if (dm_wallpaper_recent[i][0])
                dm_register_control(state, DM_ACTION_WALLPAPER_RECENT, i,
                    (struct dm_rect){ DM_SHEET_X + 8,
                        DM_SHEET_Y + 26 + i * 18, DM_SHEET_W - 16, 18 },
                    DM_CONTROL_HAND | DM_CONTROL_FOCUSABLE);
        return;
    }
    if (state->overlay == DM_OVERLAY_APPLE_MENU)
    {
        for (i = 0; i < DM_APPLE_MENU_ROWS; i++)
        {
            struct dm_rect row =
            {
                DM_MENU_X + 1,
                DM_MENU_Y + DM_MENU_TOP + i * DM_MENU_ROW_H,
                DM_MENU_W - 2, DM_MENU_ROW_H
            };

            dm_register_control(state, DM_ACTION_APPLE_MENU_ROW, i, row,
                                DM_CONTROL_HAND | DM_CONTROL_FOCUSABLE);
        }
        return;
    }
    if (state->overlay == DM_OVERLAY_APP_MENU)
    {
        int panel_x = dm_app_menu_panel_x(state);

        for (i = 0; i < DM_APP_MENU_COUNT; i++)
        {
            struct dm_rect row =
            {
                panel_x + 1,
                DM_MENU_Y + DM_MENU_TOP + i * DM_MENU_ROW_H,
                DM_MENU_W - 2, DM_MENU_ROW_H
            };

            dm_register_control(state, DM_ACTION_APP_MENU_ROW, i, row,
                                DM_CONTROL_HAND | DM_CONTROL_FOCUSABLE);
        }
        return;
    }
    if (state->overlay == DM_OVERLAY_FINDER_CONTEXT)
    {
        for (i = 0; i < DM_CONTEXT_ROWS; i++)
        {
            struct dm_rect row =
            {
                DM_CONTEXT_X + 1,
                DM_CONTEXT_Y + DM_MENU_TOP + i * DM_MENU_ROW_H,
                DM_CONTEXT_W - 2, DM_MENU_ROW_H
            };

            dm_register_control(state, DM_ACTION_CONTEXT_MENU_ROW, i, row,
                                DM_CONTROL_HAND | DM_CONTROL_FOCUSABLE);
        }
        return;
    }
    if (state->overlay == DM_OVERLAY_RETURN_CONFIRM ||
        state->overlay == DM_OVERLAY_RESTART_CONFIRM ||
        state->overlay == DM_OVERLAY_DELETE_CONFIRM ||
        state->overlay == DM_OVERLAY_NEW_FOLDER_CONFIRM)
    {
        dm_register_control(state, DM_ACTION_MODAL_CONFIRM, 0,
                            (struct dm_rect){ 0, 0, LCD_WIDTH, LCD_HEIGHT },
                            DM_CONTROL_FOCUSABLE);
        return;
    }
    if (state->overlay == DM_OVERLAY_NOTICE ||
        state->overlay == DM_OVERLAY_GET_INFO ||
        state->overlay == DM_OVERLAY_DIAGNOSTICS)
    {
        dm_register_control(state, DM_ACTION_MODAL_DISMISS, 0,
                            (struct dm_rect){ 0, 0, LCD_WIDTH, LCD_HEIGHT },
                            DM_CONTROL_FOCUSABLE);
        return;
    }

    dm_register_control(state, DM_ACTION_APPLE_MENU, 0,
                        (struct dm_rect){ 0, 0,
                                          DM_APPLE_X + DM_APPLE_W,
                                          DM_MENUBAR_H },
                        DM_CONTROL_HAND);
    for (i = 0; i < DM_APP_MENU_COUNT; i++)
    {
        struct dm_rect item = dm_menu_bar_rect(state, i);

        if (item.x + item.width > 210)
            break;
        dm_register_control(state, DM_ACTION_MENU_BAR, i, item,
                            DM_CONTROL_HAND);
    }
    if (!state->fullscreen && state->app != DM_APP_LAUNCHPAD &&
        state->app != DM_APP_DASHBOARD)
    {
        for (i = 0; i < DM_DOCK_SLOTS; i++)
            dm_register_control(state, DM_ACTION_DOCK_APP, i,
                                dm_dock_rect(i), DM_CONTROL_HAND);
    }
    if (state->app != DM_APP_DESKTOP && dm_app_has_window(state->app))
    {
        if (!state->fullscreen)
        {
            for (i = 0; i < state->window_stack_count; i++)
            {
                enum dm_app app = state->window_stack[i];

                if (app == state->app || app == state->minimized_app)
                    continue;
                dm_sync_window_geometry(state, app);
                dm_register_control(
                    state, DM_ACTION_FOCUS_WINDOW, app,
                    (struct dm_rect){ DM_WIN_X, DM_WIN_Y,
                                      DM_WIN_W, DM_WIN_TITLE_H },
                    DM_CONTROL_HAND);
            }
            dm_sync_window_geometry(state, state->app);
        }
        window = dm_app_window_rect(state->app);
        dm_register_control(state, DM_ACTION_CLOSE, 0,
                            dm_app_title_button_rect(state->app, window, 8),
                            DM_CONTROL_HAND);
        dm_register_control(
            state, DM_ACTION_MINIMISE, 0,
            dm_app_title_button_rect(state->app, window, 29),
            DM_CONTROL_HAND);
        dm_register_control(
            state, DM_ACTION_FULLSCREEN, 0,
            dm_app_title_button_rect(state->app, window, 50),
            DM_CONTROL_HAND);
        if (!state->fullscreen)
            dm_register_control(state, DM_ACTION_WINDOW_DRAG, 0,
                                dm_app_window_title_drag_rect(state->app), 0);
    }
    if (state->app == DM_APP_FINDER)
    {
        if (!dm_wallpaper_picker)
        {
            dm_register_control(state, DM_ACTION_FINDER_BACK, 0, DM_BACK_RECT,
                                DM_CONTROL_HAND);
            dm_register_control(state, DM_ACTION_FINDER_FORWARD, 0,
                                DM_FORWARD_RECT, DM_CONTROL_HAND);
        }
        if (dm_file_count > DM_FILE_ROWS)
            dm_register_control(
                state, DM_ACTION_FINDER_SCROLL, 0,
                (struct dm_rect){ DM_SCROLLER_X, DM_BODY_Y,
                                  DM_SCROLLER_W, DM_SCROLLER_H },
                DM_CONTROL_HAND);
        for (i = 0; i < DM_FILE_ROWS && dm_file_top + i < dm_file_count; i++)
            dm_register_control(
                state, DM_ACTION_FILE_ROW, dm_file_top + i,
                (struct dm_rect){ DM_LIST_X, DM_BODY_Y + i * DM_ROW_H,
                                  DM_LIST_W, DM_ROW_H },
                DM_CONTROL_DOUBLE_CLICK | DM_CONTROL_FOCUSABLE);
        for (i = 0; !dm_wallpaper_picker && i < (int)ARRAYLEN(dm_sidebar_rows); i++)
        {
            const struct dm_sidebar_row *row = &dm_sidebar_rows[i];

            if (!row->header)
                dm_register_control(
                    state, DM_ACTION_FINDER_SIDEBAR, i,
                    (struct dm_rect){ DM_WIN_X, dm_sidebar_row_y(row),
                                      DM_WIN_SIDEBAR_W, row->height },
                    DM_CONTROL_HAND | DM_CONTROL_FOCUSABLE);
        }
    }
    else if (state->app == DM_APP_ITUNES)
    {
        if (dm_library.depth)
            dm_register_control(state, DM_ACTION_ITUNES_BACK, 0,
                (struct dm_rect){ DM_ITUNES_X + 64, DM_ITUNES_Y + 1, 40, 19 },
                DM_CONTROL_HAND | DM_CONTROL_FOCUSABLE);
#if LCD_WIDTH < 1920
        int visible = MIN(dm_itunes_visible_count, dm_itunes_page_capacity());

        dm_register_control(
            state, DM_ACTION_ITUNES_PREVIOUS, 0,
            (struct dm_rect){ DM_ITUNES_X + 30, DM_ITUNES_Y + 24, 31, 35 },
            DM_CONTROL_HAND);
        dm_register_control(
            state, DM_ACTION_ITUNES_PLAY_PAUSE, 0,
            (struct dm_rect){ DM_ITUNES_X + 66, DM_ITUNES_Y + 23, 38, 38 },
            DM_CONTROL_HAND);
        dm_register_control(
            state, DM_ACTION_ITUNES_NEXT, 0,
            (struct dm_rect){ DM_ITUNES_X + 106, DM_ITUNES_Y + 24, 33, 35 },
            DM_CONTROL_HAND);
        dm_register_control(
            state, DM_ACTION_ITUNES_WPS, 0,
            (struct dm_rect){ DM_ITUNES_X + 159, DM_ITUNES_Y + 22,
                              MAX(54, DM_ITUNES_W - 200), 34 },
            DM_CONTROL_HAND);
        dm_register_control(
            state, DM_ACTION_ITUNES_SEEK, 0,
            (struct dm_rect){ DM_ITUNES_X + 164, DM_ITUNES_Y + 49,
                              MAX(54, DM_ITUNES_W - 180) - 10, 7 },
            DM_CONTROL_HAND);
        dm_register_control(
            state, DM_ACTION_ITUNES_VOLUME, -1,
            (struct dm_rect){ DM_ITUNES_X + DM_ITUNES_W - 49,
                              DM_ITUNES_Y + 29, 18, 18 },
            DM_CONTROL_HAND);
        dm_register_control(
            state, DM_ACTION_ITUNES_VOLUME, 1,
            (struct dm_rect){ DM_ITUNES_X + DM_ITUNES_W - 26,
                              DM_ITUNES_Y + 29, 18, 18 },
            DM_CONTROL_HAND);
        dm_register_control(
            state, DM_ACTION_ITUNES_SEARCH, 0,
            (struct dm_rect){ DM_ITUNES_X + 2,
                              DM_ITUNES_Y + DM_ITUNES_H - 17, 92, 16 },
            DM_CONTROL_HAND);
        dm_register_control(
            state, DM_ACTION_ITUNES_SHUFFLE, 0,
            (struct dm_rect){ DM_ITUNES_X + 94,
                              DM_ITUNES_Y + DM_ITUNES_H - 17, 47, 16 },
            DM_CONTROL_HAND);
        dm_register_control(
            state, DM_ACTION_ITUNES_REPEAT, 0,
            (struct dm_rect){ DM_ITUNES_X + 141,
                              DM_ITUNES_Y + DM_ITUNES_H - 17, 45, 16 },
            DM_CONTROL_HAND);
#else
        dm_register_control(
            state, DM_ACTION_ITUNES_PREVIOUS, 0,
            (struct dm_rect){ DM_ITUNES_X + 30, DM_ITUNES_Y + 24, 31, 35 },
            DM_CONTROL_HAND);
        dm_register_control(
            state, DM_ACTION_ITUNES_PLAY_PAUSE, 0,
            (struct dm_rect){ DM_ITUNES_X + 66, DM_ITUNES_Y + 23, 38, 38 },
            DM_CONTROL_HAND);
        dm_register_control(
            state, DM_ACTION_ITUNES_NEXT, 0,
            (struct dm_rect){ DM_ITUNES_X + 106, DM_ITUNES_Y + 24, 33, 35 },
            DM_CONTROL_HAND);
        dm_register_control(
            state, DM_ACTION_ITUNES_WPS, 0,
            (struct dm_rect){ DM_ITUNES_X + 159, DM_ITUNES_Y + 22,
                              DM_ITUNES_W - 200, 41 },
            DM_CONTROL_HAND);
#endif
        dm_register_control(
            state, DM_ACTION_ITUNES_PAGE, -1,
            (struct dm_rect){ DM_ITUNES_X + DM_ITUNES_W - 38,
                              DM_ITUNES_Y + DM_ITUNES_H - 20, 18, 18 },
            DM_CONTROL_HAND);
        dm_register_control(
            state, DM_ACTION_ITUNES_PAGE, 1,
            (struct dm_rect){ DM_ITUNES_X + DM_ITUNES_W - 20,
                              DM_ITUNES_Y + DM_ITUNES_H - 20, 18, 18 },
            DM_CONTROL_HAND);
        if (dm_itunes_total_known &&
            dm_itunes_total > dm_itunes_page_capacity())
            dm_register_control(
                state, DM_ACTION_ITUNES_SCROLL, 0,
                (struct dm_rect){ DM_ITUNES_SCROLLER_X,
                                  DM_ITUNES_SCROLLER_Y,
                                  DM_ITUNES_SCROLLER_W,
                                  DM_ITUNES_SCROLLER_H },
                DM_CONTROL_HAND);
        for (i = 0; i < DM_ITUNES_SOURCE_COUNT; i++)
            dm_register_control(
                state, DM_ACTION_ITUNES_SOURCE, i,
#if LCD_WIDTH < 1920
                (struct dm_rect){ DM_ITUNES_X + 1,
                                  DM_ITUNES_BODY_Y +
                                  i * DM_ITUNES_SOURCE_ROW_H,
                                  DM_ITUNES_SOURCE_W - 2,
                                  DM_ITUNES_SOURCE_ROW_H },
#else
                (struct dm_rect){ DM_ITUNES_X + 1,
                                  DM_ITUNES_BODY_Y + i * DM_ITUNES_ROW_H,
                                  DM_ITUNES_SOURCE_W - 2, DM_ITUNES_ROW_H },
#endif
                DM_CONTROL_HAND | DM_CONTROL_FOCUSABLE);
#if LCD_WIDTH < 1920
        if (dm_itunes_source == DM_ITUNES_ALBUMS)
        {
            for (i = 0; i < visible && i < DM_ITUNES_ALBUM_TILES; i++)
                dm_register_control(
                    state, DM_ACTION_FILE_ROW, dm_itunes_visible[i],
                    dm_itunes_album_item_rect(i),
                    DM_CONTROL_DOUBLE_CLICK | DM_CONTROL_FOCUSABLE);
        }
        else
#endif
        {
            for (i = 0;
#if LCD_WIDTH < 1920
                 i < visible;
#else
                 i < DM_ITUNES_ROWS && i < dm_itunes_count;
#endif
                 i++)
                dm_register_control(
                    state, DM_ACTION_FILE_ROW,
#if LCD_WIDTH < 1920
                    dm_itunes_visible[i],
#else
                    i,
#endif
                    (struct dm_rect){ DM_ITUNES_LIST_X,
                                      DM_ITUNES_BODY_Y + i * DM_ITUNES_ROW_H,
                                      DM_ITUNES_LIST_W, DM_ITUNES_ROW_H },
                    DM_CONTROL_DOUBLE_CLICK | DM_CONTROL_FOCUSABLE);
        }
    }
    else if (state->app == DM_APP_PREVIEW)
    {
        for (i = 0; i < DM_FILE_ROWS && dm_file_top + i < dm_file_count; i++)
            dm_register_control(
                state, DM_ACTION_FILE_ROW, dm_file_top + i,
                (struct dm_rect){ DM_WIN_X + 1, DM_BODY_Y + i * DM_ROW_H,
                                  DM_WIN_W - 2, DM_ROW_H },
                DM_CONTROL_DOUBLE_CLICK | DM_CONTROL_FOCUSABLE);
    }
    else if (state->app == DM_APP_LAUNCHPAD)
    {
        for (i = 0; i < (int)ARRAYLEN(dm_launchpad_apps); i++)
            dm_register_control(
                state, DM_ACTION_LAUNCHPAD_APP, i,
                dm_launchpad_item_rect(i),
                DM_CONTROL_HAND | DM_CONTROL_FOCUSABLE);
    }
    else if (state->app == DM_APP_TEXTEDIT)
    {
        dm_register_control(
            state, DM_ACTION_TEXTEDIT_EDIT, 0,
            (struct dm_rect){ DM_WIN_X + 8, DM_BODY_Y + 3,
                              DM_WIN_W - 16, DM_BODY_H - 6 },
            DM_CONTROL_HAND | DM_CONTROL_FOCUSABLE);
    }
    else if (state->app == DM_APP_CALCULATOR)
    {
        for (i = 0; i < DM_CALCULATOR_COLUMNS * DM_CALCULATOR_ROWS; i++)
            dm_register_control(state, DM_ACTION_CALCULATOR_KEY, i,
                                dm_calculator_key_rect(i),
                                DM_CONTROL_HAND | DM_CONTROL_FOCUSABLE);
    }
    else if (state->app == DM_APP_PREFERENCES)
    {
        for (i = 0; i < 2; i++)
            dm_register_control(state, DM_ACTION_PREFERENCE_TAB, i,
                (struct dm_rect){ DM_WIN_X + i * DM_WIN_W / 2,
                    DM_WIN_Y + 26, DM_WIN_W / 2, 22 },
                DM_CONTROL_HAND | DM_CONTROL_FOCUSABLE);
        for (i = 0; i < DM_PREF_ROWS && i < 6; i++)
            dm_register_control(
                state, DM_ACTION_PREFERENCE, i,
                (struct dm_rect){ DM_WIN_X + 6,
                                  DM_BODY_Y + i * DM_PREF_ROW_H,
                                  DM_WIN_W - 12, DM_PREF_ROW_H },
                DM_CONTROL_HAND | DM_CONTROL_FOCUSABLE);
    }
    else if (state->app == DM_APP_DASHBOARD)
    {
        int player_y = DM_MENUBAR_H + 91;

        dm_register_control(
            state, DM_ACTION_DASHBOARD_PREVIOUS, 0,
            (struct dm_rect){ 124, player_y + 32, 18, 20 },
            DM_CONTROL_HAND);
        dm_register_control(
            state, DM_ACTION_DASHBOARD_PLAY_PAUSE, 0,
            (struct dm_rect){ 142, player_y + 29, 22, 25 },
            DM_CONTROL_HAND);
        dm_register_control(
            state, DM_ACTION_DASHBOARD_NEXT, 0,
            (struct dm_rect){ 164, player_y + 32, 18, 20 },
            DM_CONTROL_HAND);
        dm_register_control(
            state, DM_ACTION_DASHBOARD_ITUNES, 0,
            (struct dm_rect){ 188, player_y + 8, 106, 66 },
            DM_CONTROL_HAND);
    }
    else if (state->app == DM_APP_DESKTOP)
    {
        dm_register_control(
            state, DM_ACTION_DESKTOP_LAUNCHPAD, 0,
            (struct dm_rect){ DM_DESKTOP_LAUNCHPAD_X,
                              DM_DESKTOP_LAUNCHPAD_Y,
                              DM_DESKTOP_LAUNCHPAD_W,
                              DM_DESKTOP_LAUNCHPAD_H },
            DM_CONTROL_HAND);
        if (dm_settings.show_desktop_folders)
        {
            dm_register_control(
                state, DM_ACTION_DESKTOP_FOLDER, 0,
                (struct dm_rect){ DM_DESKTOP_ITEM_X, DM_DESKTOP_DISK_Y,
                                  DM_DESKTOP_ITEM_W, DM_DESKTOP_ITEM_H },
                DM_CONTROL_DOUBLE_CLICK);
            dm_register_control(
                state, DM_ACTION_DESKTOP_FOLDER, 1,
                (struct dm_rect){ DM_DESKTOP_ITEM_X, DM_DESKTOP_DOCS_Y,
                                  DM_DESKTOP_ITEM_W, DM_DESKTOP_ITEM_H },
                DM_CONTROL_DOUBLE_CLICK);
            dm_register_control(
                state, DM_ACTION_DESKTOP_FOLDER, 2,
                (struct dm_rect){ DM_DESKTOP_ITEM_X, DM_DESKTOP_MUSIC_Y,
                                  DM_DESKTOP_ITEM_W, DM_DESKTOP_ITEM_H },
                DM_CONTROL_DOUBLE_CLICK);
        }
    }
}

static const struct dm_control *dm_control_at(const struct dm_state *state,
                                               int x, int y)
{
    int i;

    for (i = state->controls->count - 1; i >= 0; i--)
    {
        const struct dm_control *control = &state->controls->controls[i];

        if (dm_point_in_rect(x, y, control->bounds))
            return control;
    }
    return NULL;
}

static void dm_update_hover(struct dm_state *state)
{
    const struct dm_control *control;
    int old_dock = state->hover_dock;

    dm_build_controls(state);
    state->hover_dock = -1;
    state->hover_row = -1;
    state->hover_launchpad = -1;
    state->menu_row = -1;
    state->preference_row = -1;
    control = dm_control_at(state, state->cursor_x, state->cursor_y);
    if (control)
    {
        state->controls->hover_index = control - state->controls->controls;
        if (control->action == DM_ACTION_DOCK_APP)
            state->hover_dock = control->value;
        else if (control->action == DM_ACTION_FILE_ROW)
            state->hover_row = control->value;
        else if (control->action == DM_ACTION_APPLE_MENU_ROW ||
                 control->action == DM_ACTION_APP_MENU_ROW ||
                 control->action == DM_ACTION_CONTEXT_MENU_ROW)
            state->menu_row = control->value;
        else if (control->action == DM_ACTION_PREFERENCE)
            state->preference_row = control->value;
        else if (control->action == DM_ACTION_LAUNCHPAD_APP)
            state->hover_launchpad = control->value;
    }
    if (old_dock != state->hover_dock)
    {
        state->dock_stage = 0;
        state->dock_hover_tick = *rb->current_tick;
    }
}

static bool dm_update_dock_stage(struct dm_state *state)
{
    long elapsed;
    int stage;

    if (state->hover_dock < 0)
        return false;
    elapsed = *rb->current_tick - state->dock_hover_tick;
    stage = elapsed >= HZ / 3 ? 3 :
            elapsed >= HZ / 10 ? 2 :
            elapsed >= HZ / 20 ? 1 : 0;
    if (stage == state->dock_stage)
        return false;
    state->dock_stage = stage;
    state->redraw = true;
    return true;
}

/* Previous/Next offer deterministic focus inside compact controls where
 * pixel-perfect pointer placement is needlessly difficult. The desktop and
 * Finder canvas remain pointer-driven; this is an assist for menus, sheets,
 * and System Preferences rather than a replacement interaction mode. */
static bool dm_focus_move(struct dm_state *state, int direction)
{
    struct dm_control_registry *registry = state->controls;
    int focused = -1;
    int first = -1;
    int last = -1;
    int next;
    int i;

    dm_build_controls(state);
    for (i = 0; i < registry->count; i++)
    {
        const struct dm_control *control = &registry->controls[i];

        if ((control->flags & DM_CONTROL_FOCUSABLE) == 0)
            continue;
        if (first < 0)
            first = i;
        last = i;
        if (control->id == registry->focus_id)
            focused = i;
    }
    if (first < 0)
        return false;
    if (focused < 0)
        next = direction > 0 ? first : last;
    else
    {
        next = focused;
        do
        {
            next += direction > 0 ? 1 : -1;
            if (next >= registry->count)
                next = 0;
            else if (next < 0)
                next = registry->count - 1;
        }
        while ((registry->controls[next].flags & DM_CONTROL_FOCUSABLE) == 0 &&
               next != focused);
    }
    registry->focus_id = registry->controls[next].id;
    state->cursor_x = registry->controls[next].bounds.x +
                      registry->controls[next].bounds.width / 2;
    state->cursor_y = registry->controls[next].bounds.y +
                      registry->controls[next].bounds.height / 2;
    dm_update_hover(state);
    return true;
}

/* Four-direction fallback step, in the same 1-6 pixel band the absolute
 * wheel produces at ordinary speeds.
 */
static int dm_fallback_step(const struct dm_state *state)
{
    (void)state;
    return MAX(1, MIN(6, dm_settings.pointer_speed * 2));
}

/* Cadence of held-direction glide steps, matching the wheel glide's own
 * interaction refresh rate (dm_glide_wheel()). */
#define DM_HELD_DIR_STEP_TICKS (HZ / 20)
/* If a direction button's release event is ever missed - or the target has
 * no release detection at all - this bounds how long the pointer can keep
 * drifting on stale contact. */
#define DM_HELD_DIR_TIMEOUT_TICKS (HZ / 3)

static void dm_held_dir_touch(struct dm_state *state, int dx, int dy,
                              long now)
{
    if (dx)
        state->held_dir_x = dx;
    if (dy)
        state->held_dir_y = dy;
    state->held_seen_tick = now;
}

static void dm_held_dir_release_x(struct dm_state *state, int dx)
{
    if (state->held_dir_x == dx)
        state->held_dir_x = 0;
}

static void dm_held_dir_release_y(struct dm_state *state, int dy)
{
    if (state->held_dir_y == dy)
        state->held_dir_y = 0;
}

/* Advances the pointer for a held direction button every loop tick rather
 * than waiting on the button driver's own press/repeat cadence, which is
 * what made holding a direction move in stop-start bursts instead of
 * smoothly.  Mirrors the wheel-available nudge (1px) or four-direction
 * fallback step (1-6px) that the caller used to apply per button event.
 */
static bool dm_advance_held_direction(struct dm_state *state, long now)
{
    int old_x = state->cursor_x;
    int old_y = state->cursor_y;
    int step;

    if (state->held_dir_x == 0 && state->held_dir_y == 0)
        return false;
    if (!TIME_BEFORE(now, state->held_seen_tick + DM_HELD_DIR_TIMEOUT_TICKS))
    {
        state->held_dir_x = 0;
        state->held_dir_y = 0;
        return false;
    }
    if (TIME_BEFORE(now, state->held_emit_tick + DM_HELD_DIR_STEP_TICKS))
        return false;
    state->held_emit_tick = now;
    step = state->wheel_available ? 1 : dm_fallback_step(state);
    if (state->held_dir_x)
        state->cursor_x = MAX(0, MIN(LCD_WIDTH - 2,
                              state->cursor_x + state->held_dir_x * step));
    if (state->held_dir_y)
        state->cursor_y = MAX(0, MIN(LCD_HEIGHT - 2,
                              state->cursor_y + state->held_dir_y * step));
    if (state->cursor_x == old_x && state->cursor_y == old_y)
        return false;
    if (state->mouse_down)
        state->dragged = true;
    dm_update_hover(state);
    return true;
}

/* Window motion is capped at 25Hz while the cursor remains at the normal
 * interaction cadence.  This prevents title-bar dragging from turning every
 * clickwheel sample into a full-window composition. */
static bool dm_update_window_drag(struct dm_state *state, bool force)
{
    int app = state->app;
    int dx;
    int dy;
    int window_x;
    int window_y;
    struct dm_rect window;
    long now = *rb->current_tick;

    if (!state->window_dragging || !dm_app_has_window(app) ||
        state->fullscreen)
        return false;
    if (!force && TIME_BEFORE(now, state->window_drag_tick + HZ / 25))
        return false;
    dx = state->cursor_x - state->window_drag_x;
    dy = state->cursor_y - state->window_drag_y;
    if (dx == 0 && dy == 0)
        return false;
    dm_sync_window_geometry(state, app);
    window = dm_app_window_rect(app);
    window_x = MAX(DM_SOURCE_SAFE_MARGIN,
                   MIN(LCD_WIDTH - DM_SOURCE_SAFE_MARGIN - window.width,
                       window.x + dx));
    window_y = MAX(DM_MENUBAR_H,
                   MIN(DM_DOCK_ICON_Y - window.height, window.y + dy));
    state->window_dx[app] += window_x - window.x;
    state->window_dy[app] += window_y - window.y;
    state->window_drag_x = state->cursor_x;
    state->window_drag_y = state->cursor_y;
    state->window_drag_tick = now;
    dm_sync_window_geometry(state, state->app);
    state->damage_full = true;
    state->redraw = true;
    return true;
}

static bool dm_pointer_sample(struct dm_state *state, int x, int y,
                              unsigned int buttons)
{
    bool moved = false, down;
    long now = *rb->current_tick;
    x = MAX(0, MIN(LCD_WIDTH - 2, x));
    y = MAX(0, MIN(LCD_HEIGHT - 2, y));
    if (x != state->cursor_x || y != state->cursor_y)
    {
        state->cursor_x = x;
        state->cursor_y = y;
        if (state->mouse_down)
            state->dragged = true;
        dm_update_hover(state);
        moved = true;
    }

    down = (buttons & 1u) != 0;
    if (down != state->mouse_down)
    {
        if (down)
        {
            const struct dm_control *pressed;

            dm_update_hover(state);
            pressed = dm_control_at(state, state->cursor_x, state->cursor_y);
            state->mouse_down = true;
            state->dragged = false;
            state->mouse_down_tick = *rb->current_tick;
            state->window_dragging = pressed &&
                pressed->action == DM_ACTION_WINDOW_DRAG;
            state->window_drag_x = state->cursor_x;
            state->window_drag_y = state->cursor_y;
            state->window_drag_tick = now;
        }
        else
        {
            bool double_click =
                TIME_BEFORE(now, state->last_click_tick +
                                 DM_DOUBLE_CLICK_TICKS) &&
                DM_ABS(state->cursor_x - state->last_click_x) <= 5 &&
                DM_ABS(state->cursor_y - state->last_click_y) <= 5;

            dm_update_window_drag(state, true);
            state->window_dragging = false;
            state->mouse_down = false;
            if (!state->dragged)
                state->host_click = double_click ? 2 : 1;
            state->last_click_tick = now;
            state->last_click_x = state->cursor_x;
            state->last_click_y = state->cursor_y;
            state->dragged = false;
        }
        moved = true;
    }
    if (((buttons & 2u) != 0) != state->host_secondary)
    {
        state->host_secondary = (buttons & 2u) != 0;
        if (state->host_secondary)
            state->host_context = true;
        moved = true;
    }
    return moved;
}

#ifdef SIMULATOR
/* Host pointer, for Rockpod's "Display this iPod on this computer".
 *
 * Somebody sitting at a computer has a real mouse; making them steer the
 * pointer with a simulated click wheel would be absurd.  The simulator
 * publishes the host pointer in panel coordinates and this reads it, so the
 * mouse moves the Snow Leopard pointer directly and its buttons are the
 * primary and secondary click.  Simulator-only: the record does not exist on
 * hardware, where the wheel is the pointer.
 */
/* "%04d %04d %02u\n": four, four and two fixed-width fields */
#define DM_HOST_POINTER_RECORD 13
#define DM_HOST_POINTER_POLL_TICKS MAX(1, HZ / 60)
/* The plugin reads through the simulated filesystem, where an absolute host
 * path would be resolved under the simulator root and never found, so the
 * record lives at a fixed location inside that filesystem. */
#define DM_HOST_POINTER_FILE ROCKBOX_DIR "/host-pointer"

static bool dm_poll_host_pointer(struct dm_state *state)
{
    char record[DM_HOST_POINTER_RECORD + 1];
    int fd;
    int x;
    int y;
    unsigned int buttons;
    long now = *rb->current_tick;

    if (TIME_BEFORE(now,
                    state->host_poll_tick + DM_HOST_POINTER_POLL_TICKS))
        return false;
    state->host_poll_tick = now;

    fd = rb->open(DM_HOST_POINTER_FILE, O_RDONLY);
    if (fd < 0)
        return false;
    if (rb->read(fd, record, DM_HOST_POINTER_RECORD) != DM_HOST_POINTER_RECORD)
    {
        rb->close(fd);
        return false;
    }
    rb->close(fd);
    if (record[4] != ' ' || record[9] != ' ')
        return false;
    record[4] = '\0';
    record[9] = '\0';
    record[12] = '\0';
    x = rb->atoi(record);
    y = rb->atoi(record + 5);
    buttons = (unsigned int)rb->atoi(record + 10);
    if (x < 0 || y < 0)
        return false;

    state->host_pointer = true;
    return dm_pointer_sample(state, x, y, buttons);
}
#endif /* SIMULATOR */

#ifdef HAVE_WHEEL_POSITION
/* Resting a finger on the wheel keeps the pointer travelling.
 *
 * Tangential motion alone means crossing the screen takes repeated strokes,
 * and reaching a corner is tedious.  Holding a finger still on the ring -
 * touching, not clicking - glides the pointer the way that point of the ring
 * faces: the top of the wheel is up, the right is right, and so on.  The
 * glide begins on contact so the pointer acknowledges the finger immediately,
 * then accelerates gently to a cap the longer it is held.
 *
 * Movement accumulates in sixteenths of a pixel so slow glides stay smooth
 * instead of stepping a whole pixel per sample.
 */
#define DM_GLIDE_RAMP (HZ * 3 / 4)
#define DM_GLIDE_SUBPIXEL 16
/* Sixteenths of a pixel per tick.  Stationary glide is only a bounded
 * long-distance assist; ordinary wheel motion owns precision pointing. */
#define DM_GLIDE_MIN_RATE 5
#define DM_GLIDE_MAX_RATE 14

/* Ring buttons have positions and Select is the mouse button.  Gliding under
 * either would drag the pointer while a menu is raised or between mouse-down
 * and mouse-up, so any held clickwheel button suppresses stationary glide.
 */
static bool dm_ring_button_down(void)
{
    long status = rb->button_status();

    return (status & (BUTTON_MENU | BUTTON_PLAY | BUTTON_LEFT |
                      BUTTON_RIGHT | BUTTON_SELECT)) != 0;
}

static void dm_glide_reset(struct dm_state *state, long now)
{
    state->glide_since = now;
    state->glide_tick = now;
    state->glide_emit = now;
    state->glide_x = 0;
    state->glide_y = 0;
}

static void dm_wheel_reset(struct dm_state *state)
{
    state->last_wheel = -1;
    state->wheel_velocity = 0;
    state->wheel_direction = 0;
    state->last_wheel_tick = 0;
    state->glide_since = 0;
    state->glide_tick = 0;
    state->glide_emit = 0;
    state->glide_x = 0;
    state->glide_y = 0;
    state->frac_x = 0;
    state->frac_y = 0;
}

static bool dm_glide_wheel(struct dm_state *state, int wheel, long now)
{
    long held;
    long ticks;
    int old_x;
    int old_y;
    int angle;
    int rate;
    int moved_x;
    int moved_y;

    if (state->glide_since == 0)
        dm_glide_reset(state, now);
    if (dm_ring_button_down() || state->mouse_down)
    {
        /* Re-arm from rest.  Keeping the old held duration here made glide
         * resume at maximum speed on Select release and moved the pointer
         * between mouse-down and the click. */
        dm_glide_reset(state, now);
        return false;
    }
    held = now - state->glide_since;
    /* Displacement follows elapsed ticks, not how often this happens to be
     * polled, so the pointer travels at the same speed on a 6G and a 5G and
     * whether or not a window is being composed. */
    ticks = now - state->glide_tick;
    if (ticks <= 0)
        return false;
    state->glide_tick = now;

    rate = DM_GLIDE_MIN_RATE +
           (int)(held * (DM_GLIDE_MAX_RATE - DM_GLIDE_MIN_RATE) /
                 MAX(1, DM_GLIDE_RAMP));
    rate = MIN(DM_GLIDE_MAX_RATE, rate) *
           (MAX(1, dm_settings.pointer_speed) + 2) / 4;
    rate = (int)MIN((long)rate * ticks, (long)DM_GLIDE_MAX_RATE * HZ);

    /* wheel position 0 is the top of the ring; the pointer follows the
     * direction that point faces, so the ring reads like a compass. */
    angle = wheel * 360 / 96 - 90;
    moved_x = fp14_cos(angle) * rate / 16384;
    moved_y = fp14_sin(angle) * rate / 16384;
    if (dm_settings.reverse_wheel)
    {
        moved_x = -moved_x;
        moved_y = -moved_y;
    }
    state->glide_x += moved_x;
    state->glide_y += moved_y;
    /* Displacement integrates every poll, but a frame is only committed at
     * the interaction refresh rate: a glide must not drive the compositor
     * faster than ordinary interaction does. */
    if (TIME_BEFORE(now, state->glide_emit + DM_POINTER_UPDATE_TICKS))
        return false;
    moved_x = state->glide_x / DM_GLIDE_SUBPIXEL;
    moved_y = state->glide_y / DM_GLIDE_SUBPIXEL;
    if (moved_x == 0 && moved_y == 0)
        return false;
    state->glide_emit = now;
    state->glide_x -= moved_x * DM_GLIDE_SUBPIXEL;
    state->glide_y -= moved_y * DM_GLIDE_SUBPIXEL;
    old_x = state->cursor_x;
    old_y = state->cursor_y;
    state->cursor_x = MAX(0, MIN(LCD_WIDTH - 2, state->cursor_x + moved_x));
    state->cursor_y = MAX(0, MIN(LCD_HEIGHT - 2, state->cursor_y + moved_y));
    if (state->cursor_x == old_x && state->cursor_y == old_y)
        return false;
    if (state->mouse_down)
        state->dragged = true;
    dm_update_hover(state);
    return true;
}
#endif /* HAVE_WHEEL_POSITION */

static bool dm_poll_wheel(struct dm_state *state)
{
#ifdef HAVE_WHEEL_POSITION
    int wheel = rb->wheel_status();
    int old;
    int angle_old;
    int angle_new;
    int dx;
    int dy;
    int gain16;
    int delta;
    int direction;
    int old_x;
    int old_y;
    long now = *rb->current_tick;
    long elapsed;

    /* Current iPod firmware retains the absolute position while the wheel is
     * touched, so -1 is a definite lift and must brake immediately. */
    if (wheel < 0)
    {
        dm_wheel_reset(state);
        return false;
    }
    state->wheel_available = true;
    if (state->last_wheel < 0)
    {
        int touch_dx;
        int touch_dy;

        state->last_wheel = wheel;
        state->last_wheel_tick = now;
        dm_glide_reset(state, now);
        /* A clickwheel touch is the start of a pointing gesture.  Give it one
         * deterministic pixel in the direction of the touched point so the
         * cursor responds on the first packet, then let the subpixel glide
         * continue smoothly from there.  One pixel is small enough that a
         * Select press still lands on the intended control. */
        angle_new = wheel * 360 / 96 - 90;
        touch_dx = fp14_cos(angle_new);
        touch_dy = fp14_sin(angle_new);
        if (dm_settings.reverse_wheel)
        {
            touch_dx = -touch_dx;
            touch_dy = -touch_dy;
        }
        touch_dx = touch_dx > 4096 ? 1 : touch_dx < -4096 ? -1 : 0;
        touch_dy = touch_dy > 4096 ? 1 : touch_dy < -4096 ? -1 : 0;
        old_x = state->cursor_x;
        old_y = state->cursor_y;
        state->cursor_x = MAX(0, MIN(LCD_WIDTH - 2,
                                     state->cursor_x + touch_dx));
        state->cursor_y = MAX(0, MIN(LCD_HEIGHT - 2,
                                     state->cursor_y + touch_dy));
        if (state->cursor_x == old_x && state->cursor_y == old_y)
            return false;
        dm_update_hover(state);
        return true;
    }
    old = state->last_wheel;
    if (old == wheel)
        return dm_glide_wheel(state, wheel, now);
    dm_glide_reset(state, now);
    delta = wheel - old;
    if (delta > 48)
        delta -= 96;
    else if (delta < -48)
        delta += 96;
    /* Every hardware count contributes to the subpixel accumulator.  The
     * clickwheel driver already reports a stable absolute position while the
     * finger rests, so a second deadband here only makes careful motion feel
     * sticky. */
    direction = delta < 0 ? -1 : 1;
    elapsed = now - state->last_wheel_tick;
    if (state->wheel_direction != 0 &&
        direction != state->wheel_direction)
        state->wheel_velocity = 0;
    if (elapsed > HZ / 4)
        state->wheel_velocity = 0;
    state->wheel_direction = direction;
    state->last_wheel = wheel;
    state->last_wheel_tick = now;
    angle_old = old * 360 / 96 - 90;
    angle_new = wheel * 360 / 96 - 90;
    dx = fp14_cos(angle_new) - fp14_cos(angle_old);
    dy = fp14_sin(angle_new) - fp14_sin(angle_old);
    if (dm_settings.reverse_wheel)
    {
        dx = -dx;
        dy = -dy;
    }
    if (elapsed <= 0)
        elapsed = 1;
    state->wheel_velocity = MIN(
        4, (state->wheel_velocity * 2 +
            MIN(4, DM_ABS(delta) * HZ / elapsed / 12)) / 3);
    /* Begin every gesture at precision gain and blend travel speed in over
     * the next eight counts after the four-count precision band.  Fractional
     * gain avoids a packet boundary that suddenly jumps several pixels. */
    gain16 = 32 + (MAX(1, dm_settings.pointer_speed) - 1) * 12 *
             MIN(8, MAX(0, DM_ABS(delta) - 4)) / 8 +
             state->wheel_velocity * 6;
    /* Carry the fraction of a pixel each sample leaves behind.  Truncating it
     * away made every small movement round to nothing, which is what made
     * fine positioning feel dead; keeping it means the pointer travels in
     * proportion to the finger however slowly it moves. */
    state->frac_x += dx * gain16 / 4096;
    state->frac_y += dy * gain16 / 4096;
    dx = state->frac_x / DM_WHEEL_SUBPIXEL;
    dy = state->frac_y / DM_WHEEL_SUBPIXEL;
    dx = MAX(-DM_WHEEL_PACKET_CAP, MIN(DM_WHEEL_PACKET_CAP, dx));
    dy = MAX(-DM_WHEEL_PACKET_CAP, MIN(DM_WHEEL_PACKET_CAP, dy));
    if (dx == 0 && dy == 0)
        return false;
    state->frac_x -= dx * DM_WHEEL_SUBPIXEL;
    state->frac_y -= dy * DM_WHEEL_SUBPIXEL;
    old_x = state->cursor_x;
    old_y = state->cursor_y;
    state->cursor_x = MAX(0, MIN(LCD_WIDTH - 2, state->cursor_x + dx));
    state->cursor_y = MAX(0, MIN(LCD_HEIGHT - 2, state->cursor_y + dy));
    if (state->cursor_x == old_x && state->cursor_y == old_y)
        return false;
    if (state->mouse_down)
        state->dragged = true;
    dm_update_hover(state);
    return true;
#else
    (void)state;
    return false;
#endif
}

static int dm_launch_external(struct dm_state *state, const char *path,
                              const char *parameter)
{
    int result;

    if (!rb->file_exists(path))
    {
        dm_notice(state, "Application is not installed");
        return PLUGIN_OK;
    }
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
    result = rb->plugin_open(path, parameter);
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(false);
#endif
    return result;
}

static int dm_activate_app(struct dm_state *state, enum dm_app app)
{
    int result;

    state->overlay = DM_OVERLAY_NONE;
    state->fullscreen = false;
    dm_fullscreen = false;
    if (app != DM_APP_DESKTOP && app != DM_APP_TRASH)
        state->running_apps |= DM_APP_BIT(app);
    if (state->minimized_app == app)
    {
        dm_start_animation(state, app, false);
        return PLUGIN_OK;
    }
    switch (app)
    {
        case DM_APP_FINDER:
            if (state->app != DM_APP_FINDER)
                dm_scan_directory();
            state->app = app;
            break;
        case DM_APP_ITUNES:
            dm_itunes_page_top = 0;
            dm_itunes_load();
            state->app = app;
            break;
        case DM_APP_PREFERENCES:
            state->app = app;
            break;
        case DM_APP_PHOTOS:
            state->app = DM_APP_DESKTOP;
            result = dm_launch_external(state, PLUGIN_APPS_DIR "/photos.rock", NULL);
            state->running_apps &= ~DM_APP_BIT(app);
            return result;
        case DM_APP_PREVIEW:
            /* Photos is a window, not a takeover: the shell lists the
             * pictures folder itself and only hands a chosen image to the
             * real viewer, which is the one thing that has to own the LCD. */
            dm_scan_media(DM_MEDIA_PICTURES);
            state->app = app;
            break;
        case DM_APP_TEXTEDIT:
            state->app = app;
            break;
        case DM_APP_CALCULATOR:
            state->app = app;
            break;
        case DM_APP_DIRECTV:
            /* The decoder, mixer channel and window chrome live in one
             * mpegplayer instance. Commit the idle desktop first so that
             * instance can preserve it behind its Aqua window. */
            state->app = DM_APP_DESKTOP;
            state->damage_full = true;
            dm_draw(state);
            if (!dm_save_livetv_underlay())
            {
                dm_notice(state, "Could not prepare TV window");
                state->running_apps &= ~DM_APP_BIT(app);
                return PLUGIN_OK;
            }
            result = dm_launch_external(state, PLUGIN_APPS_DIR "/livetv.rock",
                                        "-desktop");
            state->running_apps &= ~DM_APP_BIT(app);
            return result;
        case DM_APP_SITEKICK:
            /* Like Live TV, Sitekick replaces the shell plugin image while
             * it runs. Hand it the exact committed Desktop so its Aqua
             * window can remain genuinely windowed. */
            state->app = DM_APP_DESKTOP;
            state->damage_full = true;
            dm_draw(state);
            if (!dm_save_underlay(DM_SITEKICK_UNDERLAY_FILE,
                                  DM_SITEKICK_UNDERLAY_MAGIC))
            {
                dm_notice(state, "Could not prepare Sitekick window");
                state->running_apps &= ~DM_APP_BIT(app);
                return PLUGIN_OK;
            }
            result = dm_launch_external(state, PLUGIN_APPS_DIR "/sitekick.rock",
                                        "-desktop");
            state->running_apps &= ~DM_APP_BIT(app);
            return result;
        case DM_APP_STEAM:
            state->app = DM_APP_DESKTOP;
            state->damage_full = true;
            dm_draw(state);
            if (!dm_save_underlay(DM_STEAM_UNDERLAY_FILE,
                                  DM_STEAM_UNDERLAY_MAGIC))
            {
                dm_notice(state, "Could not prepare Steam window");
                state->running_apps &= ~DM_APP_BIT(app);
                return PLUGIN_OK;
            }
            result = dm_launch_external(state,
                PLUGIN_APPS_DIR "/steam_desktop.rock", "-desktop");
            state->running_apps &= ~DM_APP_BIT(app);
            return result;
        case DM_APP_NETFLIX:
            /* Netflix is a desktop-only catalogue window. It receives the
             * committed Desktop and reads only Video Sync's videolist. */
            state->app = DM_APP_DESKTOP;
            state->damage_full = true;
            dm_draw(state);
            if (!dm_save_underlay(DM_NETFLIX_UNDERLAY_FILE,
                                  DM_NETFLIX_UNDERLAY_MAGIC))
            {
                dm_notice(state, "Could not prepare Netflix window");
                state->running_apps &= ~DM_APP_BIT(app);
                return PLUGIN_OK;
            }
            result = dm_launch_external(state,
                PLUGIN_APPS_DIR "/netflix_desktop.rock", "-desktop");
            state->running_apps &= ~DM_APP_BIT(app);
            return result;
        case DM_APP_DASHBOARD:
            dm_dashboard_refresh_cache(true);
            state->app = app;
            break;
        case DM_APP_LAUNCHPAD:
            state->app = app;
            break;
        case DM_APP_TRASH:
            dm_notice(state, "Trash is empty");
            break;
        default:
            state->app = DM_APP_DESKTOP;
            break;
    }
    dm_stack_raise(state, state->app);
    return PLUGIN_OK;
}

static int dm_handle_app_menu(struct dm_state *state)
{
    int menu = state->app_menu;
    int row = state->menu_row;

    state->overlay = DM_OVERLAY_NONE;
    if (menu == 0)
    {
        if (row == 0)
            return dm_activate_app(state, DM_APP_FINDER);
        if (row == 1)
            dm_close_active_window(state);
        else if (row == 2)
        {
            state->fullscreen = false;
            dm_fullscreen = false;
            if (state->app != DM_APP_DESKTOP &&
                dm_app_has_window(state->app))
                dm_start_animation(state, state->app, true);
            else
                state->app = DM_APP_DESKTOP;
        }
        else if (row == 3)
            state->overlay = DM_OVERLAY_RETURN_CONFIRM;
    }
    else if (menu == 1)
    {
        if (row == 0)
            return dm_activate_app(state, DM_APP_TEXTEDIT);
        if (row == 1)
            state->desktop_selected = -1;
        else if (row == 2)
            return dm_activate_app(state, DM_APP_PREFERENCES);
    }
    else if (menu == 2)
    {
        if (row == 0 && dm_app_has_window(state->app))
        {
            state->fullscreen = !state->fullscreen;
            dm_fullscreen = state->fullscreen;
            if (state->app == DM_APP_ITUNES)
                dm_itunes_load();
        }
        else if (row == 1)
        {
            dm_settings.show_desktop_folders =
                !dm_settings.show_desktop_folders;
            dm_save_settings();
        }
        else if (row == 2)
            return dm_activate_app(state, DM_APP_LAUNCHPAD);
        else if (row == 3)
            state->damage_full = true;
    }
    else if (menu == 3)
    {
        if (row == 0 && state->app != DM_APP_DESKTOP)
        {
            state->fullscreen = false;
            dm_start_animation(state, state->app, true);
        }
        else if (row == 1 && state->window_stack_count > 1)
        {
            int index;

            for (index = state->window_stack_count - 1; index >= 0; index--)
            {
                enum dm_app app = state->window_stack[index];

                if (app != state->app)
                    return dm_activate_app(state, app);
            }
        }
        else if (row == 2)
            dm_stack_raise(state, state->app);
        else if (row == 3 && state->app != DM_APP_DESKTOP)
        {
            state->fullscreen = false;
            dm_fullscreen = false;
            if (dm_app_has_window(state->app))
                dm_start_animation(state, state->app, true);
            else
                state->app = DM_APP_DESKTOP;
        }
    }
    dm_fullscreen = state->fullscreen;
    state->damage_full = true;
    return PLUGIN_OK;
}

static int dm_handle_apple_menu(struct dm_state *state)
{
    switch (state->menu_row)
    {
        case 0:
            state->overlay = DM_OVERLAY_DIAGNOSTICS;
            break;
        case 1:
            state->app = DM_APP_PREFERENCES;
            state->overlay = DM_OVERLAY_NONE;
            break;
        case 2:
            dm_notice(state, "No recent items");
            break;
        case 3:
            rb->backlight_off();
            state->overlay = DM_OVERLAY_NONE;
            break;
        case 4:
            state->overlay = DM_OVERLAY_RESTART_CONFIRM;
            break;
        case 5:
            state->overlay = DM_OVERLAY_RETURN_CONFIRM;
            break;
    }
    return PLUGIN_OK;
}

static void dm_selected_path(char *path, size_t size)
{
    if (dm_file_selected < 0 || dm_file_selected >= dm_file_count)
    {
        path[0] = '\0';
        return;
    }
    dm_join_path(path, size, dm_cwd, dm_files[dm_file_selected].name);
}

static void dm_delete_selected(struct dm_state *state)
{
    char path[MAX_PATH];
    int result;

    dm_selected_path(path, sizeof(path));
    if (!path[0])
        return;
    if (dm_files[dm_file_selected].is_dir)
        result = rb->rmdir(path);
    else
        result = rb->remove(path);
    dm_notice(state, result == 0 ? "Item deleted" :
                                      "Could not remove item");
    dm_scan_directory();
}

static void dm_create_new_folder(struct dm_state *state)
{
    char path[MAX_PATH];
    char name[40] = "untitled folder";
    int number = 2;

    dm_join_path(path, sizeof(path), dm_cwd, name);
    while (rb->dir_exists(path) && number < 100)
    {
        rb->snprintf(name, sizeof(name), "untitled folder %d", number++);
        dm_join_path(path, sizeof(path), dm_cwd, name);
    }
    if (rb->mkdir(path) < 0)
        dm_notice(state, "Could not create folder");
    dm_scan_directory();
}

static int dm_handle_finder_context(struct dm_state *state)
{
    switch (state->menu_row)
    {
        case 0:
            state->overlay = DM_OVERLAY_GET_INFO;
            break;
        case 1:
            state->overlay = DM_OVERLAY_NEW_FOLDER_CONFIRM;
            break;
        case 2:
            if (dm_file_selected >= 0 &&
                dm_file_selected < dm_file_count)
                state->overlay = DM_OVERLAY_DELETE_CONFIRM;
            else
                state->overlay = DM_OVERLAY_NONE;
            break;
        default:
            state->overlay = DM_OVERLAY_NONE;
            break;
    }
    return PLUGIN_OK;
}

static int dm_activate_itunes_control(struct dm_state *state,
                                      const struct dm_control *control)
{
    int status = rb->audio_status();

    switch (control->action)
    {
        case DM_ACTION_ITUNES_BACK:
            dm_itunes_back();
            break;
        case DM_ACTION_ITUNES_PREVIOUS:
            rb->audio_prev();
            break;
        case DM_ACTION_ITUNES_PLAY_PAUSE:
            if ((status & AUDIO_STATUS_PAUSE) != 0)
                rb->audio_resume();
            else if ((status & AUDIO_STATUS_PLAY) != 0)
                rb->audio_pause();
            break;
        case DM_ACTION_ITUNES_NEXT:
            rb->audio_next();
            break;
        case DM_ACTION_ITUNES_WPS:
            return PLUGIN_GOTO_WPS;
        case DM_ACTION_ITUNES_PAGE:
            dm_itunes_change_page(control->value);
            break;
        case DM_ACTION_ITUNES_SCROLL:
#if LCD_WIDTH < 1920
            if (dm_itunes_total_known &&
                dm_itunes_total > dm_itunes_page_capacity())
            {
                int track_height = DM_ITUNES_SCROLLER_H;
                int thumb_height = MAX(DM_ITUNES_SCROLLER_MIN_THUMB_H,
                                       track_height * dm_itunes_page_capacity() /
                                       dm_itunes_total);
                int travel = MAX(1, track_height - thumb_height);
                int maximum = dm_itunes_total - dm_itunes_page_capacity();
                int position = state->cursor_y - DM_ITUNES_SCROLLER_Y -
                               thumb_height / 2;

                dm_itunes_page_top = MAX(0, MIN(maximum,
                    position * maximum / travel));
                dm_itunes_load();
            }
#endif
            break;
        case DM_ACTION_ITUNES_SOURCE:
            dm_itunes_source = control->value;
            desktop_library_root(&dm_library,
                                 (enum desktop_media)dm_itunes_source);
            dm_itunes_playlist[0] = dm_itunes_search[0] = 0;
            dm_itunes_page_top = 0;
            dm_itunes_load();
            break;
        case DM_ACTION_ITUNES_SEEK:
        {
            const struct mp3entry *track = rb->audio_current_track();
            int width = MAX(54, DM_ITUNES_W - 180) - 10;
            int offset = MAX(0, MIN(width,
                state->cursor_x - (DM_ITUNES_X + 164)));

            if (track && track->length > 0)
            {
                rb->audio_pre_ff_rewind();
                rb->audio_ff_rewind((long)(track->length *
                                    (unsigned long)offset / width));
            }
            break;
        }
        case DM_ACTION_ITUNES_VOLUME:
        {
            int minimum = rb->sound_min(SOUND_VOLUME);
            int maximum = rb->sound_max(SOUND_VOLUME);
            int volume = rb->global_status->volume + control->value;

            if (rb->global_settings->volume_limit >= minimum)
                maximum = MIN(maximum, rb->global_settings->volume_limit);
            volume = MAX(minimum, MIN(maximum, volume));
            rb->global_status->volume = volume;
            rb->sound_set(SOUND_VOLUME, volume);
            break;
        }
        case DM_ACTION_ITUNES_SHUFFLE:
            rb->global_settings->playlist_shuffle =
                !rb->global_settings->playlist_shuffle;
            rb->settings_save();
            break;
        case DM_ACTION_ITUNES_REPEAT:
            if (rb->global_settings->repeat_mode == REPEAT_OFF)
                rb->global_settings->repeat_mode = REPEAT_ALL;
            else if (rb->global_settings->repeat_mode == REPEAT_ALL)
                rb->global_settings->repeat_mode = REPEAT_ONE;
            else
                rb->global_settings->repeat_mode = REPEAT_OFF;
            rb->settings_save();
            break;
        case DM_ACTION_ITUNES_SEARCH:
        {
            char query[sizeof(dm_itunes_search)];

            rb->strlcpy(query, dm_itunes_search, sizeof(query));
#ifdef HAVE_WHEEL_POSITION
            rb->wheel_send_events(true);
#endif
            if (rb->kbd_input(query, sizeof(query), NULL) == 0)
            {
                rb->strlcpy(dm_itunes_search, query,
                            sizeof(dm_itunes_search));
                dm_itunes_page_top = 0;
                dm_itunes_load();
            }
#ifdef HAVE_WHEEL_POSITION
            rb->wheel_send_events(false);
#endif
            break;
        }
        default:
            break;
    }
    state->damage_full = true;
    return PLUGIN_OK;
}

/* Keep the large action dispatch out of plugin_start.  GCC otherwise inlines
 * every action's temporary buffers into the main loop's permanent frame,
 * reducing the stack available to unrelated first-frame BMP and LCD calls. */
static NO_INLINE int dm_click(struct dm_state *state, bool double_click)
{
    const struct dm_control *control;

    dm_update_hover(state);
    if (state->overlay == DM_OVERLAY_RETURN_CONFIRM)
    {
        state->running = false;
        return PLUGIN_OK;
    }
    if (state->overlay == DM_OVERLAY_RESTART_CONFIRM)
    {
        dm_save_settings();
        rb->sys_reboot();
        return PLUGIN_OK;
    }
    if (state->overlay == DM_OVERLAY_DELETE_CONFIRM)
    {
        state->overlay = DM_OVERLAY_NONE;
        dm_delete_selected(state);
        return PLUGIN_OK;
    }
    if (state->overlay == DM_OVERLAY_NEW_FOLDER_CONFIRM)
    {
        state->overlay = DM_OVERLAY_NONE;
        dm_create_new_folder(state);
        return PLUGIN_OK;
    }
    if (state->overlay == DM_OVERLAY_GET_INFO ||
        state->overlay == DM_OVERLAY_NOTICE)
    {
        state->overlay = DM_OVERLAY_NONE;
        return PLUGIN_OK;
    }
    if (state->overlay == DM_OVERLAY_DIAGNOSTICS)
    {
        state->overlay = DM_OVERLAY_NONE;
        return PLUGIN_OK;
    }
    if (state->overlay == DM_OVERLAY_APPLE_MENU)
    {
        if (state->menu_row >= 0)
            return dm_handle_apple_menu(state);
        state->overlay = DM_OVERLAY_NONE;
        return PLUGIN_OK;
    }
    if (state->overlay == DM_OVERLAY_APP_MENU)
    {
        if (state->menu_row >= 0)
            return dm_handle_app_menu(state);
        state->overlay = DM_OVERLAY_NONE;
        return PLUGIN_OK;
    }
    if (state->overlay == DM_OVERLAY_FINDER_CONTEXT)
        return dm_handle_finder_context(state);
    control = dm_control_at(state, state->cursor_x, state->cursor_y);
    if (!control)
    {
        if (state->app == DM_APP_DESKTOP)
        {
            state->desktop_selected = -1;
            state->damage_full = true;
        }
        return PLUGIN_OK;
    }
    switch (control->action)
    {
        case DM_ACTION_APPLE_MENU:
            state->overlay = DM_OVERLAY_APPLE_MENU;
            break;
        case DM_ACTION_MENU_BAR:
            state->app_menu = control->value;
            state->overlay = DM_OVERLAY_APP_MENU;
            break;
        case DM_ACTION_DOCK_APP:
            return dm_activate_app(state, dm_dock_apps[control->value]);
        case DM_ACTION_FOCUS_WINDOW:
            return dm_activate_app(state, (enum dm_app)control->value);
        case DM_ACTION_WINDOW_DRAG:
            break;
        case DM_ACTION_CLOSE:
            dm_close_active_window(state);
            break;
        case DM_ACTION_MINIMISE:
            state->fullscreen = false;
            dm_fullscreen = false;
            dm_start_animation(state, state->app, true);
            break;
        case DM_ACTION_FULLSCREEN:
            state->fullscreen = !state->fullscreen;
            dm_fullscreen = state->fullscreen;
            if (state->app == DM_APP_ITUNES)
                dm_itunes_load();
            state->damage_full = true;
            break;
        case DM_ACTION_FINDER_BACK:
            dm_history_move(-1);
            break;
        case DM_ACTION_FINDER_FORWARD:
            dm_history_move(1);
            break;
        case DM_ACTION_FINDER_SCROLL:
        {
            int maximum = dm_file_count - DM_FILE_ROWS;
            int travel = MAX(1, DM_SCROLLER_H - DM_SCROLLER_THUMB_H);

            dm_file_top = MAX(
                0, MIN(maximum,
                       (state->cursor_y - DM_BODY_Y -
                        DM_SCROLLER_THUMB_H / 2) * maximum / travel));
            dm_file_selected = MAX(
                dm_file_top,
                MIN(dm_file_selected, dm_file_top + DM_FILE_ROWS - 1));
            break;
        }
        case DM_ACTION_FILE_ROW:
            dm_file_selected = control->value;
            if (double_click)
            {
                if (state->app == DM_APP_ITUNES &&
                    !dm_files[dm_file_selected].is_dir)
                    return dm_open_itunes_selection(state);
                if (dm_wallpaper_picker)
                {
                    char path[MAX_PATH];
                    dm_join_path(path, sizeof(path), dm_cwd,
                                 dm_files[dm_file_selected].name);
                    dm_photo_request(state, path,
                                     dm_files[dm_file_selected].is_dir);
                    return PLUGIN_OK;
                }
                return dm_open_selected_file(state);
            }
            break;
        case DM_ACTION_FINDER_SIDEBAR:
            dm_navigate(dm_sidebar_rows[control->value].path);
            break;
        case DM_ACTION_ITUNES_BACK:
        case DM_ACTION_ITUNES_PREVIOUS:
        case DM_ACTION_ITUNES_PLAY_PAUSE:
        case DM_ACTION_ITUNES_NEXT:
        case DM_ACTION_ITUNES_WPS:
        case DM_ACTION_ITUNES_PAGE:
        case DM_ACTION_ITUNES_SCROLL:
        case DM_ACTION_ITUNES_SOURCE:
        case DM_ACTION_ITUNES_SEEK:
        case DM_ACTION_ITUNES_VOLUME:
        case DM_ACTION_ITUNES_SHUFFLE:
        case DM_ACTION_ITUNES_REPEAT:
        case DM_ACTION_ITUNES_SEARCH:
            return dm_activate_itunes_control(state, control);
        case DM_ACTION_DASHBOARD_PREVIOUS:
            rb->audio_prev();
            state->damage_full = true;
            break;
        case DM_ACTION_DASHBOARD_PLAY_PAUSE:
            if ((rb->audio_status() & AUDIO_STATUS_PAUSE) != 0)
                rb->audio_resume();
            else if ((rb->audio_status() & AUDIO_STATUS_PLAY) != 0)
                rb->audio_pause();
            state->damage_full = true;
            break;
        case DM_ACTION_DASHBOARD_NEXT:
            rb->audio_next();
            state->damage_full = true;
            break;
        case DM_ACTION_DASHBOARD_ITUNES:
            return dm_activate_app(state, DM_APP_ITUNES);
        case DM_ACTION_PHOTO_PIN:
            dm_photo_pin_input(state, control->value);
            break;
        case DM_ACTION_WALLPAPER_RECENT:
            state->overlay = DM_OVERLAY_NONE;
            dm_photo_request(state, dm_wallpaper_recent[control->value], false);
            break;
        case DM_ACTION_PREFERENCE_TAB:
            dm_desktop_preferences = control->value != 0;
            state->damage_full = true;
            break;
        case DM_ACTION_PREFERENCE:
            if (dm_desktop_preferences)
            {
                switch (control->value)
                {
                    case 0: dm_wallpaper_select(""); break;
                    case 1:
                        dm_wallpaper_picker = true;
                        if (!dm_navigate(dm_photo_root))
                        {
                            rb->strlcpy(dm_cwd, dm_photo_root, sizeof(dm_cwd));
                            dm_file_count = dm_file_top = 0;
                            dm_file_selected = -1;
                        }
                        dm_history_reset();
                        state->app = DM_APP_FINDER;
                        break;
                    case 2:
                    {
                        int previous = dm_wallpaper_scale;
                        dm_wallpaper_scale = (previous + 1) % 3;
                        if (!dm_wallpaper_apply(dm_wallpaper_path))
                            dm_wallpaper_scale = previous;
                        break;
                    }
                    case 3: state->overlay = DM_OVERLAY_WALLPAPER_RECENT; break;
                    case 4:
                        dm_auto_activate = (dm_auto_activate + 1) % 4;
                        rb->strlcpy(dm_wallpaper_status,
                            "Saved for future USB host support",
                            sizeof(dm_wallpaper_status));
                        break;
                    case 5:
                        dm_display_mode = (dm_display_mode + 1) % 3;
                        rb->strlcpy(dm_wallpaper_status,
                            "Saved for future external display",
                            sizeof(dm_wallpaper_status));
                        break;
                }
                dm_save_settings();
                state->damage_full = true;
                break;
            }
            switch (control->value)
            {
                case 0:
                dm_settings.pointer_speed =
                    dm_settings.pointer_speed % 4 + 1;
                break;
                case 1:
                dm_settings.reverse_wheel =
                    !dm_settings.reverse_wheel;
                break;
                case 2:
                dm_settings.drag_lock = !dm_settings.drag_lock;
                break;
                case 3:
                dm_settings.show_desktop_folders =
                    !dm_settings.show_desktop_folders;
                break;
                case 4:
                if (dm_settings.start_app == DM_APP_DESKTOP)
                    dm_settings.start_app = DM_APP_FINDER;
                else if (dm_settings.start_app == DM_APP_FINDER)
                    dm_settings.start_app = DM_APP_ITUNES;
                else
                    dm_settings.start_app = DM_APP_DESKTOP;
                break;
                case 5:
                dm_settings.restore_session =
                    !dm_settings.restore_session;
                break;
            }
            dm_save_settings();
            break;
        case DM_ACTION_DESKTOP_FOLDER:
            state->desktop_selected = control->value;
            if (!double_click)
                break;
            if (control->value == 0)
                dm_navigate("/");
            else if (control->value == 1)
                dm_navigate("/Documents");
            else
                dm_navigate("/Music");
            state->running_apps |= DM_APP_BIT(DM_APP_FINDER);
            state->app = DM_APP_FINDER;
            break;
        case DM_ACTION_DESKTOP_LAUNCHPAD:
            state->desktop_selected = 3;
            return dm_activate_app(state, DM_APP_LAUNCHPAD);
        case DM_ACTION_LAUNCHPAD_APP:
            return dm_activate_app(state, dm_launchpad_apps[control->value]);
        case DM_ACTION_TEXTEDIT_EDIT:
        {
            char note[DM_TEXTEDIT_NOTE_SIZE];

            rb->strlcpy(note, dm_textedit_note, sizeof(note));
#ifdef HAVE_WHEEL_POSITION
            rb->wheel_send_events(true);
#endif
            if (rb->kbd_input(note, sizeof(note), NULL) == 0)
                rb->strlcpy(dm_textedit_note, note,
                            sizeof(dm_textedit_note));
#ifdef HAVE_WHEEL_POSITION
            rb->wheel_send_events(false);
#endif
            state->damage_full = true;
            break;
        }
        case DM_ACTION_CALCULATOR_KEY:
        {
            static const char keys[] = "789/456*123-0C=+";
            char key = keys[control->value];

            if (key >= '0' && key <= '9')
            {
                if (dm_calculator_new_value)
                    dm_calculator_value = 0;
                if (dm_calculator_value <= 99999999L)
                    dm_calculator_value = dm_calculator_value * 10 +
                                          key - '0';
                dm_calculator_new_value = false;
            }
            else if (key == 'C')
            {
                dm_calculator_value = 0;
                dm_calculator_accumulator = 0;
                dm_calculator_operation = 0;
                dm_calculator_new_value = true;
            }
            else if (key == '=')
            {
                long right = dm_calculator_value;

                if (dm_calculator_operation == '+')
                    dm_calculator_value = dm_calculator_accumulator + right;
                else if (dm_calculator_operation == '-')
                    dm_calculator_value = dm_calculator_accumulator - right;
                else if (dm_calculator_operation == '*')
                    dm_calculator_value = dm_calculator_accumulator * right;
                else if (dm_calculator_operation == '/')
                    dm_calculator_value = right ?
                        dm_calculator_accumulator / right : 0;
                dm_calculator_operation = 0;
                dm_calculator_new_value = true;
            }
            else
            {
                dm_calculator_accumulator = dm_calculator_value;
                dm_calculator_operation = key;
                dm_calculator_new_value = true;
            }
            state->damage_full = true;
            break;
        }
        default:
            break;
    }
    return PLUGIN_OK;
}

static void dm_secondary_click(struct dm_state *state)
{
    if (dm_wallpaper_picker || state->overlay == DM_OVERLAY_PHOTO_PIN)
        return;
    if (state->app == DM_APP_FINDER)
        state->overlay = DM_OVERLAY_FINDER_CONTEXT;
}

static void dm_menu_action(struct dm_state *state)
{
    if (state->overlay != DM_OVERLAY_NONE)
    {
        if (state->overlay == DM_OVERLAY_PHOTO_PIN)
        {
            rb->memset(dm_photo_entered, 0, 5);
            rb->memset(dm_photo_expected, 0, 5);
            dm_photo_pending[0] = 0;
        }
        state->overlay = DM_OVERLAY_NONE;
        return;
    }
    if (dm_wallpaper_picker)
    {
        if (rb->strcasecmp(dm_cwd, dm_photo_root))
        {
            char parent[MAX_PATH];
            rb->strlcpy(parent, dm_cwd, sizeof(parent));
            char *slash = rb->strrchr(parent, '/');
            if (slash == parent) slash[1] = 0;
            else if (slash) *slash = 0;
            dm_navigate(parent);
        }
        else
        {
            dm_wallpaper_picker = false;
            state->app = DM_APP_PREFERENCES;
        }
        state->damage_full = true;
        return;
    }
    if (state->app == DM_APP_ITUNES && dm_itunes_back())
    {
        state->damage_full = true;
        return;
    }
    if (state->app != DM_APP_DESKTOP)
    {
        state->fullscreen = false;
        dm_fullscreen = false;
        if (dm_app_has_window(state->app))
            dm_start_animation(state, state->app, true);
        else
            state->app = DM_APP_DESKTOP;
        state->damage_full = true;
        return;
    }
    state->overlay = DM_OVERLAY_RETURN_CONFIRM;
}

/* Common input consumer for simulator, companion and future HID adapters.
 * Text carries one Unicode code point; the current bitmap font covers ASCII,
 * so unsupported characters are rejected rather than corrupting UTF-8. */
static int dm_input_event(struct dm_state *state, const struct desktop_event *e)
{
    state->redraw = true;
    if (state->overlay == DM_OVERLAY_PHOTO_PIN)
    {
        if (e->type == DESKTOP_TEXT || (e->type == DESKTOP_KEY && e->down &&
            (e->value == DESKTOP_ENTER || e->value == DESKTOP_BACKSPACE)))
        {
            int value = e->type == DESKTOP_TEXT ? e->value - '0' :
                        e->value == DESKTOP_ENTER ? 11 : 10;
            if (e->type != DESKTOP_TEXT || (value >= 0 && value <= 9))
                dm_photo_pin_input(state, value);
            return PLUGIN_OK;
        }
    }
    unsigned buttons = (state->mouse_down ? 1 : 0) |
                       (state->host_secondary ? 2 : 0);
    if (e->type == DESKTOP_POINTER_MOVE)
    {
        state->host_pointer = true;
        dm_pointer_sample(state, e->x, e->y, buttons);
    }
    else if (e->type == DESKTOP_POINTER_BUTTON)
    {
        if (e->value < 1 || e->value > 2) return PLUGIN_OK;
        unsigned mask = 1u << (e->value - 1);
        dm_pointer_sample(state, state->cursor_x, state->cursor_y,
                          e->down ? buttons | mask : buttons & ~mask);
    }
    else if (e->type == DESKTOP_POINTER_WHEEL)
    {
        if (state->app == DM_APP_ITUNES) dm_itunes_change_page(e->value > 0 ? -1 : 1);
        else if (state->app == DM_APP_FINDER)
            dm_file_top = MAX(0, MIN(MAX(0, dm_file_count - DM_FILE_ROWS),
                                     dm_file_top - e->value));
        state->damage_full = true;
    }
    else if (e->type == DESKTOP_KEY && e->down)
    {
        switch (e->value)
        {
            case DESKTOP_TAB:
                dm_focus_move(state, e->modifiers & DESKTOP_MOD_SHIFT ? -1 : 1);
                break;
            case DESKTOP_UP: case DESKTOP_LEFT: dm_focus_move(state, -1); break;
            case DESKTOP_DOWN: case DESKTOP_RIGHT: dm_focus_move(state, 1); break;
            case DESKTOP_ENTER:
                state->damage_full = true;
                return dm_click(state, true);
            case DESKTOP_ESCAPE: dm_menu_action(state); break;
            case DESKTOP_BACKSPACE:
                if (state->app == DM_APP_ITUNES && dm_itunes_search[0])
                {
                    dm_itunes_search[rb->strlen(dm_itunes_search) - 1] = 0;
                    dm_itunes_page_top = 0;
                    dm_itunes_load();
                }
                else if (state->app == DM_APP_TEXTEDIT && dm_textedit_note[0])
                    dm_textedit_note[rb->strlen(dm_textedit_note) - 1] = 0;
                state->damage_full = true;
                break;
            case 'w':
                if (e->modifiers & DESKTOP_MOD_CTRL)
                {
                    if (state->overlay != DM_OVERLAY_NONE)
                        dm_menu_action(state);
                    else
                        dm_close_active_window(state);
                    state->damage_full = true;
                }
                break;
            case ' ':
                if (state->app == DM_APP_ITUNES)
                {
                    struct dm_control play = {.action = DM_ACTION_ITUNES_PLAY_PAUSE};
                    dm_activate_itunes_control(state, &play);
                }
                break;
        }
    }
    else if (e->type == DESKTOP_TEXT && e->value >= 32 && e->value < 127)
    {
        char *text = state->app == DM_APP_ITUNES ? dm_itunes_search :
                     state->app == DM_APP_TEXTEDIT ? dm_textedit_note : NULL;
        size_t capacity = state->app == DM_APP_ITUNES ? sizeof(dm_itunes_search) :
                                                                    sizeof(dm_textedit_note);
        if (text && rb->strlen(text) + 1 < capacity)
        {
            size_t n = rb->strlen(text);
            text[n] = e->value; text[n + 1] = 0;
            if (state->app == DM_APP_ITUNES)
            { dm_itunes_page_top = 0; dm_itunes_load(); }
        }
        state->damage_full = true;
    }
    state->redraw = true;
    return PLUGIN_OK;
}

#ifdef SIMULATOR
/* Synthetic input adapter. One sequence-numbered record, acknowledged by
 * sequence rather than consuming repeated reports as repeated key presses. */
static int dm_poll_input_event(struct dm_state *state)
{
    static int sequence;
    static long poll;
    char line[96];
    int serial, type, down, modifiers;
    struct desktop_event event;
    if (TIME_BEFORE(*rb->current_tick, poll + DM_HOST_POINTER_POLL_TICKS))
        return PLUGIN_OK;
    poll = *rb->current_tick;
    int fd = rb->open(ROCKBOX_DIR "/desktop-event", O_RDONLY);
    if (fd < 0) return PLUGIN_OK;
    int n = rb->read_line(fd, line, sizeof(line));
    rb->close(fd);
    int fields[7];
    char *cursor = line, *end;
    if (n <= 0) return PLUGIN_OK;
    for (int i = 0; i < 7; i++)
    {
        long value = rb->strtol(cursor, &end, 10);
        if (end == cursor || value < -32768 || value > 32767)
            return PLUGIN_OK;
        fields[i] = value;
        cursor = end;
    }
    serial = fields[0]; type = fields[1]; event.x = fields[2];
    event.y = fields[3]; event.value = fields[4];
    modifiers = fields[5]; down = fields[6];
    if (serial == sequence || type < DESKTOP_POINTER_MOVE || type > DESKTOP_TEXT)
        return PLUGIN_OK;
    sequence = serial; event.type = type;
    event.down = down != 0; event.modifiers = modifiers;
    return dm_input_event(state, &event);
}
#endif

static bool dm_button(long button, long mask)
{
    return (button & mask) == mask;
}

/* Desktop Mode consumes raw plugin buttons rather than Rockbox actions.
 * Translate the standard 30-pin iAP remote controls to their clickwheel
 * equivalents so a dock's Previous/Next, Play, Menu and Stop keys remain
 * useful.  Up/Down stay as remote values and are handled as vertical pointer
 * movement in the main loop below. */
static long dm_remote_button(long button)
{
#ifdef BUTTON_RC_PLAY
    long flags = button & (BUTTON_REPEAT | BUTTON_REL);
    long base = button & ~(BUTTON_REPEAT | BUTTON_REL);

    switch (base)
    {
    case BUTTON_RC_LEFT:
        return BUTTON_LEFT | flags;
    case BUTTON_RC_RIGHT:
        return BUTTON_RIGHT | flags;
    case BUTTON_RC_PLAY:
    case BUTTON_RC_SELECT:
        return BUTTON_SELECT | flags;
    case BUTTON_RC_MENU:
    case BUTTON_RC_STOP:
        return BUTTON_MENU | flags;
    default:
        break;
    }
#else
    (void)button;
#endif
    return button;
}

static enum plugin_status dm_missing_assets_screen(void)
{
    rb->lcd_setfont(FONT_UI);
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_background(LCD_RGBPACK(245, 245, 245));
    rb->lcd_set_foreground(LCD_RGBPACK(20, 20, 20));
#endif
    rb->lcd_clear_display();
    rb->lcd_putsxy(8, 10,
                   (const unsigned char *)"Snow Leopard assets required");
    rb->lcd_putsxy(8, 38,
                   (const unsigned char *)"Open Rockpod > Device >");
    rb->lcd_putsxy(8, 56,
                   (const unsigned char *)"Desktop Mode, import an owned");
    rb->lcd_putsxy(8, 74,
                   (const unsigned char *)"Mac OS X 10.6 source, then");
    rb->lcd_putsxy(8, 92,
                   (const unsigned char *)"install the verified pack.");
    rb->lcd_putsxy(8, 126,
                   (const unsigned char *)"Expected under .rockbox/rocks/");
    rb->lcd_putsxy(8, 144,
                   (const unsigned char *)"apps/ (iPod) or rocks.data/ (host)");
    rb->lcd_putsxyf(8, 162, "Error: %s", dm_error_name(dm_last_error));
    rb->lcd_putsxy(8, 176,
                   (const unsigned char *)"Menu: Return to iPod");
    rb->lcd_update();
    while (1)
    {
        long button = rb->button_get(true);

        if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
            return PLUGIN_USB_CONNECTED;
#ifdef BUTTON_MENU
        if (button & BUTTON_MENU)
            break;
#endif
#ifdef BUTTON_PLAY
        if (button & BUTTON_PLAY)
            break;
#endif
    }
    return PLUGIN_OK;
}

static volatile bool dm_video_out_connected;
#ifdef SIMULATOR
static bool dm_simulator_session;
#endif

static void dm_video_out_event(unsigned short id, void *data)
{
    if (id == SYS_EVENT_VIDEOOUT_CHANGED)
        dm_video_out_connected = data != NULL;
}

static bool dm_video_out_ready(void)
{
#ifdef SIMULATOR
    return dm_simulator_session;
#elif defined(IPOD_6G)
    return dm_video_out_connected;
#else
    return false;
#endif
}

enum plugin_status plugin_start(const void *parameter)
{
#ifdef DM_UNSUPPORTED_TARGET
    (void)parameter;
    rb->splash(HZ * 2, "Desktop needs a 320x240 color target");
    return PLUGIN_OK;
#else
    /* Plugins execute on Rockbox's main thread.  Keep the long-lived desktop
     * session in plugin BSS so drawing and filesystem calls retain the full
     * main-stack budget. */
    static struct dm_state state;
    int result = PLUGIN_OK;
    enum dm_boot_result boot_result;
    unsigned int saved_foreground = rb->lcd_get_foreground();
    unsigned int saved_background = rb->lcd_get_background();
    int saved_drawmode = rb->lcd_get_drawmode();
    bool saved_backlight = rb->is_backlight_on(true);
    bool session_started = false;
    bool videoout_event_registered = false;
#ifdef HAVE_WHEEL_POSITION
    bool wheel_events_owned = false;
#endif

    rb->memset(&state, 0, sizeof(state));
    state.app = DM_APP_DESKTOP;
    state.overlay = DM_OVERLAY_NONE;
    state.cursor_x = LCD_WIDTH / 2;
    state.cursor_y = LCD_HEIGHT / 2;
    state.hover_dock = -1;
    state.hover_row = -1;
    state.hover_launchpad = -1;
    state.menu_row = -1;
    state.desktop_selected = -1;
    state.preference_row = -1;
    state.last_wheel = -1;
    state.running_apps = DM_APP_BIT(DM_APP_FINDER);
    state.running = true;
    state.redraw = true;
    state.damage_full = true;
    state.presented_cursor_x = -32;
    state.presented_cursor_y = -32;

#ifdef SIMULATOR
    dm_simulator_session = parameter != NULL &&
        !rb->strcmp((const char *)parameter, DM_SIMULATOR_LAUNCH_TOKEN);
#elif defined(IPOD_6G)
    dm_video_out_connected = parameter != NULL &&
        !rb->strcmp((const char *)parameter, DM_VIDEOOUT_LAUNCH_TOKEN);
    if (dm_video_out_connected)
        videoout_event_registered =
            rb->add_event(SYS_EVENT_VIDEOOUT_CHANGED, dm_video_out_event);
#else
    (void)parameter;
#endif
    desktop_library_root(&dm_library, DESKTOP_SONGS);
    dm_fullscreen = false;
    if (!dm_video_out_ready() ||
        (dm_video_out_connected && !videoout_event_registered))
    {
        rb->splash(HZ * 2, "Video dock required");
        result = PLUGIN_OK;
        goto cleanup;
    }
    rb->lcd_setfont(FONT_UI);
    boot_result = dm_show_boot_animation();
    if (boot_result == DM_BOOT_USB_CONNECTED)
    {
        result = PLUGIN_USB_CONNECTED;
        goto cleanup;
    }
    if (boot_result == DM_BOOT_ERROR)
    {
        result = dm_missing_assets_screen();
        goto cleanup;
    }
    if (!dm_load_assets())
    {
        result = dm_missing_assets_screen();
        goto cleanup;
    }
    state.controls = dm_alloc(sizeof(*state.controls));
    if (!state.controls)
    {
        result = dm_missing_assets_screen();
        goto cleanup;
    }
    rb->memset(state.controls, 0, sizeof(*state.controls));
    state.controls->hover_index = -1;
    desktop_surface_init(&dm_surface, (uint16_t *)dm_canvas,
                         LCD_WIDTH * LCD_HEIGHT * sizeof(fb_data),
                         LCD_WIDTH, LCD_HEIGHT, LCD_WIDTH, dm_lcd_present, NULL);
    dm_load_settings();
    photo_library_root(dm_photo_root, sizeof(dm_photo_root),
                       dm_arena, dm_arena_left);
    if (dm_wallpaper_path[0] && !dm_wallpaper_apply(dm_wallpaper_path))
        rb->strlcpy(dm_wallpaper_status, "Saved wallpaper locked or unavailable",
                    sizeof(dm_wallpaper_status));
    session_started = true;
    state.hint_until = *rb->current_tick + HZ * 4;
    if (!rb->dir_exists(dm_cwd))
        rb->strcpy(dm_cwd, "/");
    dm_history_reset();
    state.app = dm_settings.restore_session ?
                dm_saved_app : dm_settings.start_app;
    if (dm_settings.restore_session)
    {
        state.cursor_x = dm_saved_cursor_x;
        state.cursor_y = dm_saved_cursor_y;
    }
    if (state.app != DM_APP_DESKTOP)
    {
        state.running_apps |= DM_APP_BIT(state.app);
        dm_stack_raise(&state, state.app);
    }
    if (state.app == DM_APP_FINDER)
        dm_scan_directory();
    else if (state.app == DM_APP_ITUNES)
    {
        dm_itunes_page_top = 0;
        dm_itunes_load();
    }
    else if (state.app == DM_APP_DASHBOARD)
        dm_dashboard_refresh_cache(true);
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(false);
    wheel_events_owned = true;
#endif
#ifdef HAVE_DOCKED_AMBIENT_CLOCK
    rb->ambient_clock_ready(true);
#endif
    while (state.running && result == PLUGIN_OK)
    {
        long button;

        if (!dm_video_out_ready())
        {
            state.running = false;
            break;
        }

#ifdef HAVE_TAGCACHE
        if (state.app == DM_APP_ITUNES && dm_itunes_database_pending)
        {
            const struct tagcache_stat *stat = rb->tagcache_get_stat();

            if (stat && stat->readyvalid && stat->ready)
            {
                dm_itunes_load();
                state.redraw = true;
            }
        }
#endif
        if (state.app == DM_APP_ITUNES && !state.animation.active &&
            rb->button_queue_count() == 0 && dm_itunes_art_service_one())
            state.redraw = true;
        dm_update_animation(&state);
        dm_update_dock_stage(&state);
        if (state.redraw)
        {
            dm_draw(&state);
            state.redraw = false;
        }
        button = rb->button_get_w_tmo(DM_INPUT_POLL_TICKS);
#ifdef HAVE_DOCKED_AMBIENT_CLOCK
        if (rb->ambient_clock_ready(button != BUTTON_NONE ||
                state.app != DM_APP_DESKTOP || state.overlay != DM_OVERLAY_NONE ||
                state.animation.active
#ifdef HAVE_WHEEL_POSITION
                || rb->wheel_status() >= 0
#endif
                ))
        {
#ifdef HAVE_WHEEL_POSITION
            rb->wheel_send_events(true);
#endif
            int wake = rb->ambient_clock_run(false);
#ifdef HAVE_WHEEL_POSITION
            rb->wheel_send_events(false);
#endif
            state.redraw = state.damage_full = true;
            state.last_wheel = -1;
            if (wake == ACTION_TREE_WPS)
            {
                result = PLUGIN_GOTO_WPS;
                break;
            }
            if (IS_SYSEVENT(wake))
                button = wake;
            else if (wake == ACTION_TREE_STOP)
                button = BUTTON_PLAY | BUTTON_REPEAT;
            else if (wake == ACTION_TREE_POWER_MENU)
                button = BUTTON_MENU | BUTTON_SELECT;
        }
#endif
        if (TIME_AFTER(*rb->current_tick, state.last_status_tick + HZ))
        {
            state.last_status_tick = *rb->current_tick;
            dm_request_damage(&state,
                (struct dm_rect){ 0, 0, LCD_WIDTH, DM_MENUBAR_H });
            if (state.app == DM_APP_ITUNES)
                dm_request_damage(&state,
                    (struct dm_rect){ DM_ITUNES_X, DM_ITUNES_Y,
                                      DM_ITUNES_W, 56 });
            else if (state.app == DM_APP_DASHBOARD)
            {
                dm_dashboard_refresh_cache(false);
                dm_request_damage(&state,
                    (struct dm_rect){ 0, DM_MENUBAR_H, LCD_WIDTH,
                                      LCD_HEIGHT - DM_MENUBAR_H });
            }
        }
        if (rb->button_hold())
        {
            if (dm_photo_code_count || dm_photo_entered[0] ||
                state.overlay == DM_OVERLAY_PHOTO_PIN)
            {
                rb->memset(dm_photo_codes, 0, sizeof(dm_photo_codes));
                rb->memset(dm_photo_entered, 0, sizeof(dm_photo_entered));
                rb->memset(dm_photo_expected, 0, sizeof(dm_photo_expected));
                dm_photo_code_count = 0;
                dm_photo_pending[0] = 0;
                if (state.overlay == DM_OVERLAY_PHOTO_PIN)
                    state.overlay = DM_OVERLAY_NONE;
                if (dm_photo_wallpaper_private)
                {
                    dm_wallpaper_apply("");
                    dm_photo_wallpaper_private = false;
                }
                if (dm_wallpaper_picker) dm_navigate(dm_photo_root);
                state.redraw = state.damage_full = true;
            }
            if (state.animation.active)
                dm_finish_animation(&state);
            state.last_wheel = -1;
            state.wheel_velocity = 0;
            state.wheel_direction = 0;
            state.mouse_down = false;
            state.window_dragging = false;
            state.held_dir_x = 0;
            state.held_dir_y = 0;
            continue;
        }
        if (dm_poll_wheel(&state))
        {
            if (state.animation.active)
                dm_finish_animation(&state);
            state.redraw = true;
        }
        if (dm_advance_held_direction(&state, *rb->current_tick))
        {
            if (state.animation.active)
                dm_finish_animation(&state);
            state.redraw = true;
        }
#ifdef SIMULATOR
        result = dm_poll_input_event(&state);
        if (result != PLUGIN_OK) break;
        if (dm_poll_host_pointer(&state))
        {
            if (state.animation.active)
                dm_finish_animation(&state);
            state.redraw = true;
        }
#endif
        if (state.host_click)
        {
            bool double_click = state.host_click == 2;

            state.host_click = 0;
            result = dm_click(&state, double_click);
            state.redraw = true;
            if (result != PLUGIN_OK)
                break;
        }
        if (state.host_context)
        {
            state.host_context = false;
            dm_secondary_click(&state);
            state.redraw = true;
        }
        dm_update_window_drag(&state, false);
        if (button == BUTTON_NONE)
            continue;
        if (state.animation.active)
            dm_finish_animation(&state);
        if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
        {
            result = PLUGIN_USB_CONNECTED;
            break;
        }
#ifdef BUTTON_RC_UP
        if (button & BUTTON_RC_UP)
        {
            if (button & BUTTON_REL)
                dm_held_dir_release_y(&state, -1);
            else
                dm_held_dir_touch(&state, 0, -1, *rb->current_tick);
            continue;
        }
        if (button & BUTTON_RC_DOWN)
        {
            if (button & BUTTON_REL)
                dm_held_dir_release_y(&state, 1);
            else
                dm_held_dir_touch(&state, 0, 1, *rb->current_tick);
            continue;
        }
#endif
        button = dm_remote_button(button);
#ifdef BUTTON_SELECT
        if (button == BUTTON_SELECT)
        {
            const struct dm_control *pressed;

            if (dm_settings.drag_lock && state.mouse_down)
            {
                state.mouse_down = false;
                state.dragged = false;
                state.window_dragging = false;
                state.redraw = true;
                continue;
            }
            dm_update_hover(&state);
            pressed = dm_control_at(&state, state.cursor_x, state.cursor_y);
            state.mouse_down = true;
            state.dragged = false;
            state.mouse_down_tick = *rb->current_tick;
            state.window_dragging = pressed &&
                pressed->action == DM_ACTION_WINDOW_DRAG;
            state.window_drag_x = state.cursor_x;
            state.window_drag_y = state.cursor_y;
            state.window_drag_tick = *rb->current_tick;
            state.redraw = true;
            continue;
        }
#ifdef BUTTON_REL
        if (dm_button(button, BUTTON_SELECT | BUTTON_REL))
        {
            long now = *rb->current_tick;
            bool double_click =
                TIME_BEFORE(now, state.last_click_tick +
                                 DM_DOUBLE_CLICK_TICKS) &&
                DM_ABS(state.cursor_x - state.last_click_x) <= 5 &&
                DM_ABS(state.cursor_y - state.last_click_y) <= 5;

            dm_update_window_drag(&state, true);
            state.window_dragging = false;
            state.mouse_down = state.dragged && dm_settings.drag_lock;
            if (!state.dragged)
                result = dm_click(&state, double_click);
            state.last_click_tick = now;
            state.last_click_x = state.cursor_x;
            state.last_click_y = state.cursor_y;
            state.redraw = true;
            continue;
        }
#endif
#endif
#if defined(BUTTON_PLAY) && defined(BUTTON_REL)
        if (dm_button(button, BUTTON_PLAY | BUTTON_REL))
        {
            dm_secondary_click(&state);
            state.redraw = true;
            continue;
        }
#endif
#if defined(BUTTON_PLAY) && defined(BUTTON_REPEAT)
        if (dm_button(button, BUTTON_PLAY | BUTTON_REPEAT))
        {
            state.overlay = DM_OVERLAY_RETURN_CONFIRM;
            state.redraw = true;
            continue;
        }
#endif
#ifdef BUTTON_MENU
#ifdef BUTTON_REPEAT
        if (dm_button(button, BUTTON_MENU | BUTTON_REPEAT))
        {
            state.overlay = DM_OVERLAY_APPLE_MENU;
            state.redraw = true;
            continue;
        }
#endif
#ifdef BUTTON_REL
        if (dm_button(button, BUTTON_MENU | BUTTON_REL))
#else
        if (button & BUTTON_MENU)
#endif
        {
            const struct desktop_event escape =
                { .type = DESKTOP_KEY, .value = DESKTOP_ESCAPE, .down = true };
            result = dm_input_event(&state, &escape);
            state.redraw = true;
            continue;
        }
#endif
#if defined(BUTTON_SCROLL_FWD) && defined(BUTTON_SCROLL_BACK)
#ifdef BUTTON_REL
        if (dm_button(button, BUTTON_SCROLL_FWD | BUTTON_REL))
        {
            dm_held_dir_release_y(&state, 1);
            continue;
        }
        if (dm_button(button, BUTTON_SCROLL_BACK | BUTTON_REL))
        {
            dm_held_dir_release_y(&state, -1);
            continue;
        }
#endif
        if (!state.wheel_available &&
            (button & (BUTTON_SCROLL_FWD | BUTTON_SCROLL_BACK)))
        {
            dm_held_dir_touch(&state, 0,
                              button & BUTTON_SCROLL_FWD ? 1 : -1,
                              *rb->current_tick);
            continue;
        }
#endif
#ifdef BUTTON_LEFT
#ifdef BUTTON_REL
        if (dm_button(button, BUTTON_LEFT | BUTTON_REL))
        {
            dm_held_dir_release_x(&state, -1);
            continue;
        }
#endif
        if (button & BUTTON_LEFT)
        {
            if (dm_focus_move(&state, -1))
                state.redraw = true;
            else if (state.wheel_available && state.app == DM_APP_FINDER)
                dm_history_move(-1);
            else
                dm_held_dir_touch(&state, -1, 0, *rb->current_tick);
            state.redraw = true;
            continue;
        }
#endif
#ifdef BUTTON_RIGHT
#ifdef BUTTON_REL
        if (dm_button(button, BUTTON_RIGHT | BUTTON_REL))
        {
            dm_held_dir_release_x(&state, 1);
            continue;
        }
#endif
        if (button & BUTTON_RIGHT)
        {
            if (dm_focus_move(&state, 1))
                state.redraw = true;
            else if (state.wheel_available && state.app == DM_APP_FINDER)
                dm_history_move(1);
            else
                dm_held_dir_touch(&state, 1, 0, *rb->current_tick);
            state.redraw = true;
            continue;
        }
#endif
    }
cleanup:
    if (videoout_event_registered)
        rb->remove_event(SYS_EVENT_VIDEOOUT_CHANGED, dm_video_out_event);
#ifdef HAVE_WHEEL_POSITION
    if (wheel_events_owned)
        rb->wheel_send_events(true);
#endif
    state.mouse_down = false;
    state.dragged = false;
    state.held_dir_x = 0;
    state.held_dir_y = 0;
    state.host_click = 0;
    state.host_context = false;
    if (session_started)
    {
        dm_saved_app = dm_wallpaper_picker ? DM_APP_DESKTOP : state.app;
        dm_saved_cursor_x = state.cursor_x;
        dm_saved_cursor_y = state.cursor_y;
        dm_save_settings();
    }
    rb->memset(dm_photo_codes, 0, sizeof(dm_photo_codes));
    rb->memset(dm_photo_entered, 0, sizeof(dm_photo_entered));
    rb->memset(dm_photo_expected, 0, sizeof(dm_photo_expected));
    rb->lcd_set_viewport(NULL);
    rb->lcd_set_foreground(saved_foreground);
    rb->lcd_set_background(saved_background);
    rb->lcd_set_drawmode(saved_drawmode);
    rb->lcd_setfont(FONT_UI);
    if (saved_backlight)
        rb->backlight_on();
    else
        rb->backlight_off();
    return result;
#endif
}
