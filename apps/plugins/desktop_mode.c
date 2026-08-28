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
#include "fixedpoint.h"
#include "lib/pluginlib_bmp.h"

#define DM_ASSET_DIR \
    PLUGIN_APPS_DATA_DIR "/desktop_mode_snow_leopard"
#define DM_CONFIG_FILE PLUGIN_APPS_DATA_DIR "/desktop_mode.cfg"
#define DM_MANIFEST_FILE DM_ASSET_DIR "/manifest.json"
#define DM_LIVETV_UNDERLAY_FILE \
    PLUGIN_APPS_DATA_DIR "/desktop_mode_livetv_underlay.raw"
#define DM_LIVETV_UNDERLAY_MAGIC 0x44545631u /* "DTV1" */
#define DM_SITEKICK_UNDERLAY_FILE \
    PLUGIN_APPS_DATA_DIR "/desktop_mode_sitekick_underlay.raw"
#define DM_SITEKICK_UNDERLAY_MAGIC 0x44534b31u /* "DSK1" */
#define DM_NETFLIX_UNDERLAY_FILE \
    PLUGIN_APPS_DATA_DIR "/desktop_mode_netflix_underlay.raw"
#define DM_NETFLIX_UNDERLAY_MAGIC 0x444e4631u /* "DNF1" */

#define DM_MAX_FILES 96
#define DM_ASCII_FIRST 32
#define DM_ASCII_LAST 126
#define DM_ASCII_COUNT (DM_ASCII_LAST - DM_ASCII_FIRST + 1)
#define DM_FONT_COLUMNS 16
#define DM_FONT_ROWS ((DM_ASCII_COUNT + DM_FONT_COLUMNS - 1) / DM_FONT_COLUMNS)

/* Every number below is measured from the real Snow Leopard artwork the
 * importer cuts the pack from; see docs/desktop-mode-snow-leopard-spec.md.
 * The chrome is used at Apple's own 1:1 scale, so the layout has to follow
 * the artwork rather than the other way round.
 */
#define DM_APPLE_W 22

#if LCD_WIDTH >= 1920
#define DM_DOCK_SLOTS 10
#else
#define DM_DOCK_SLOTS 9
#endif
/* Window chrome bands are the same 1:1 heights on either panel: they are the
 * same real Finder window, used whole at 1080p and in fragments at 320x240. */
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
#define DM_F_DOCK_DIRECTV_A DM_ASSET_DIR "/1920x1080/icons/directv-dock-66.66x66.rga"
#define DM_F_DOCK_DIRECTV_B DM_ASSET_DIR "/1920x1080/icons/directv-dock-70.70x70.rga"
#define DM_F_DOCK_SITEKICK_A ROCKBOX_DIR "/sitekick/desktop/icon-dock-66.66x66.rga"
#define DM_F_DOCK_SITEKICK_B ROCKBOX_DIR "/sitekick/desktop/icon-dock-70.70x70.rga"
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
#define DM_WIN_W 897
#define DM_WIN_H 671
#define DM_WIN_SIDEBAR_W 135
#define DM_ITUNES_W 1081
#define DM_ITUNES_H 655
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
#define DM_F_DOCK_DIRECTV_A DM_ASSET_DIR "/320x240/icons/directv-dock-34.34x34.rga"
#define DM_F_DOCK_DIRECTV_B DM_ASSET_DIR "/320x240/icons/directv-dock-38.38x38.rga"
#define DM_F_DOCK_SITEKICK_A ROCKBOX_DIR "/sitekick/desktop/icon-dock-34.34x34.rga"
#define DM_F_DOCK_SITEKICK_B ROCKBOX_DIR "/sitekick/desktop/icon-dock-38.38x38.rga"
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
#define DM_WIN_W 304
#define DM_WIN_H 174
#define DM_WIN_SIDEBAR_W 86
#define DM_ITUNES_W 304
/* iTunes is the foreground media application on the iPod.  Give it the
 * complete work area below the menu bar instead of reserving nearly a quarter
 * of the LCD for a Dock that is still available after closing/minimising it.
 * The shorter captured window is extended at draw time using only its native
 * striped body rows; no Apple pixels are scaled. */
#define DM_ITUNES_H (LCD_HEIGHT - DM_MENUBAR_H)
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
#define DM_WIN_X ((LCD_WIDTH - DM_WIN_W) / 2)
#define DM_WIN_Y (DM_MENUBAR_H + (LCD_HEIGHT - DM_MENUBAR_H - \
                                  DM_DOCK_BAND - DM_WIN_H) / 2)
#define DM_DOCK_BAND (DM_DOCK_ICON + DM_DOCK_SHELF_H / 2)
#define DM_DOCK_SHELF_X ((LCD_WIDTH - DM_DOCK_SHELF_W) / 2)
#define DM_DOCK_SHELF_Y (LCD_HEIGHT - DM_DOCK_SHELF_H)
#define DM_DOCK_ICON_Y (DM_DOCK_SHELF_Y - DM_DOCK_ICON / 2)
#define DM_DOCK_LEFT \
    ((LCD_WIDTH - DM_DOCK_SLOTS * DM_DOCK_STEP) / 2 + \
     (DM_DOCK_STEP - DM_DOCK_ICON) / 2)
#define DM_SCROLLER_H DM_BODY_H
#define DM_DESKTOP_ICON_X (LCD_WIDTH - DM_DOCK_ICON - 10)
#define DM_DESKTOP_DISK_Y (DM_MENUBAR_H + 9)
#define DM_DESKTOP_DOCS_Y (DM_DESKTOP_DISK_Y + DM_DOCK_ICON + 26)
#define DM_DESKTOP_MUSIC_Y (DM_DESKTOP_DOCS_Y + DM_DOCK_ICON + 26)

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
#define DM_WHEEL_JITTER_COUNTS 2
/* pointer movement carries sixteenths of a pixel between samples */
#define DM_WHEEL_SUBPIXEL 16
#define DM_ANIMATION_FRAMES 6
#define DM_ANIMATION_TICKS (HZ * 3 / 10)
#define DM_BOOT_FRAME_COUNT 12
#define DM_BOOT_FRAME_TICKS MAX(1, HZ / 12)
#define DM_BOOT_SPINNER_SIZE 24
#define DM_BOOT_SPINNER_X ((LCD_WIDTH - DM_BOOT_SPINNER_SIZE) / 2)
#define DM_BOOT_SPINNER_Y 151
#define DM_INPUT_POLL_TICKS MAX(1, HZ / 50)
/* Match the bounded microUI profile without coupling this playback-aware,
 * asset-backed shell to the immediate-mode core. */
#define DM_CONTROL_LIMIT 64
#define DM_ABS(value) ((value) < 0 ? -(value) : (value))
#define DM_APP_BIT(app) (1u << (unsigned int)(app))

#if LCD_DEPTH < 16 || \
    !((LCD_WIDTH == 320 && LCD_HEIGHT == 240) || \
      (LCD_WIDTH == 1920 && LCD_HEIGHT == 1080))
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

    DM_ASSET_DOCK_INDICATOR = DM_ASSET_BMP_COUNT,
    DM_ASSET_CURSOR_ARROW,
    DM_ASSET_CURSOR_HAND,
    DM_ASSET_ICON_FINDER,
    DM_ASSET_ICON_ITUNES,
    DM_ASSET_ICON_PREVIEW,
    DM_ASSET_ICON_TEXTEDIT,
    DM_ASSET_ICON_CALCULATOR,
    DM_ASSET_ICON_DIRECTV,
    DM_ASSET_ICON_SITEKICK,
#if LCD_WIDTH >= 1920
    DM_ASSET_ICON_NETFLIX,
#endif
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
    DM_ASSET_DOCK_DIRECTV_34,
    DM_ASSET_DOCK_DIRECTV_38,
    DM_ASSET_DOCK_SITEKICK_34,
    DM_ASSET_DOCK_SITEKICK_38,
#if LCD_WIDTH >= 1920
    DM_ASSET_DOCK_NETFLIX_34,
    DM_ASSET_DOCK_NETFLIX_38,
#endif
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
    DM_APP_NETFLIX,
    DM_APP_PREFERENCES,
    DM_APP_DASHBOARD,
    DM_APP_TRASH,
};

enum dm_overlay
{
    DM_OVERLAY_NONE = 0,
    DM_OVERLAY_APPLE_MENU,
    DM_OVERLAY_FINDER_CONTEXT,
    DM_OVERLAY_GET_INFO,
    DM_OVERLAY_RETURN_CONFIRM,
    DM_OVERLAY_RESTART_CONFIRM,
    DM_OVERLAY_DELETE_CONFIRM,
    DM_OVERLAY_NEW_FOLDER_CONFIRM,
    DM_OVERLAY_DIAGNOSTICS,
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
    DM_ACTION_DOCK_APP,
    DM_ACTION_CLOSE,
    DM_ACTION_MINIMISE,
    DM_ACTION_FINDER_BACK,
    DM_ACTION_FINDER_FORWARD,
    DM_ACTION_FINDER_SCROLL,
    DM_ACTION_FILE_ROW,
    DM_ACTION_FINDER_SIDEBAR,
    DM_ACTION_ITUNES_PREVIOUS,
    DM_ACTION_ITUNES_PLAY_PAUSE,
    DM_ACTION_ITUNES_NEXT,
    DM_ACTION_ITUNES_WPS,
    DM_ACTION_ITUNES_PAGE,
    DM_ACTION_ITUNES_SOURCE,
    DM_ACTION_PREFERENCE,
    DM_ACTION_DESKTOP_FOLDER,
    DM_ACTION_APPLE_MENU_ROW,
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
    int cursor_x;
    int cursor_y;
    int hover_dock;
    int hover_row;
    int menu_row;
    int preference_row;
    int last_wheel;
    int wheel_velocity;
    int wheel_direction;
    long last_wheel_tick;
    long last_status_tick;
    enum dm_app minimized_app;
    unsigned int running_apps;
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
    long contact_tick;
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
    [DM_ASSET_WINDOW_SIDEBAR] = { DM_F_WIN_SIDEBAR, DM_WIN_W, DM_WIN_H },
    [DM_ASSET_WINDOW_PLAIN] = { DM_F_WIN_PLAIN, DM_WIN_W, DM_WIN_H },
    [DM_ASSET_ITUNES_WINDOW] =
        { DM_F_ITUNES_WIN, DM_ITUNES_W, DM_ITUNES_ASSET_H },
    [DM_ASSET_ITUNES_SELECTION] = { DM_F_ITUNES_SEL, DM_ITUNES_W, 17 },
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
#if LCD_WIDTH >= 1920
    [DM_ASSET_ICON_NETFLIX] =
        { DM_F_ICON_NETFLIX, DM_DOCK_ICON, DM_DOCK_ICON },
#endif
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
    [DM_ASSET_DOCK_DIRECTV_34] =
        { DM_F_DOCK_DIRECTV_A, DM_DOCK_MAG_A, DM_DOCK_MAG_A },
    [DM_ASSET_DOCK_DIRECTV_38] =
        { DM_F_DOCK_DIRECTV_B, DM_DOCK_MAG_B, DM_DOCK_MAG_B },
    [DM_ASSET_DOCK_SITEKICK_34] =
        { DM_F_DOCK_SITEKICK_A, DM_DOCK_MAG_A, DM_DOCK_MAG_A },
    [DM_ASSET_DOCK_SITEKICK_38] =
        { DM_F_DOCK_SITEKICK_B, DM_DOCK_MAG_B, DM_DOCK_MAG_B },
#if LCD_WIDTH >= 1920
    [DM_ASSET_DOCK_NETFLIX_34] =
        { DM_F_DOCK_NETFLIX_A, DM_DOCK_MAG_A, DM_DOCK_MAG_A },
    [DM_ASSET_DOCK_NETFLIX_38] =
        { DM_F_DOCK_NETFLIX_B, DM_DOCK_MAG_B, DM_DOCK_MAG_B },
#endif
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
    DM_ASSET_ICON_DIRECTV,
    DM_ASSET_ICON_SITEKICK,
#if LCD_WIDTH >= 1920
    DM_ASSET_ICON_NETFLIX,
#endif
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
    DM_ASSET_DOCK_DIRECTV_34,
    DM_ASSET_DOCK_SITEKICK_34,
#if LCD_WIDTH >= 1920
    DM_ASSET_DOCK_NETFLIX_34,
#endif
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
    DM_ASSET_DOCK_DIRECTV_38,
    DM_ASSET_DOCK_SITEKICK_38,
#if LCD_WIDTH >= 1920
    DM_ASSET_DOCK_NETFLIX_38,
#endif
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
    DM_APP_DIRECTV,
    DM_APP_SITEKICK,
#if LCD_WIDTH >= 1920
    DM_APP_NETFLIX,
#endif
    DM_APP_PREFERENCES,
    DM_APP_TRASH,
};

static const char * const dm_dock_labels[DM_DOCK_SLOTS] =
{
    "Finder", "iTunes", "Preview", "TextEdit", "Calculator", "DIRECTV",
    "Sitekick",
#if LCD_WIDTH >= 1920
    "Netflix",
#endif
    "System Preferences", "Trash"
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

static const char * const dm_context_labels[DM_CONTEXT_ROWS] =
{
    "Get Info",
    "New Folder...",
    "Delete..."
};

static struct dm_asset dm_assets[DM_ASSET_COUNT];
static struct dm_font dm_fonts[DM_FONT_COUNT];
static fb_data *dm_canvas;
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

enum dm_itunes_source
{
    DM_ITUNES_ALBUMS = 0,
    DM_ITUNES_ARTISTS,
    DM_ITUNES_SONGS,
    DM_ITUNES_VIDEOS,
    DM_ITUNES_SOURCE_COUNT
};

#define DM_ITUNES_PAGE_MAX 32
#define DM_ITUNES_TITLE_MAX 96
#define DM_ITUNES_DETAIL_MAX 64
#define DM_ITUNES_VIDEO_INDEX ROCKBOX_DIR "/videolist/index.tsv"

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

static struct dm_itunes_row dm_itunes_rows[DM_ITUNES_PAGE_MAX];
static enum dm_itunes_source dm_itunes_source = DM_ITUNES_SONGS;
static int dm_itunes_count;
static int dm_itunes_total;
static int dm_itunes_page_top;
static bool dm_itunes_database_pending;
static bool dm_itunes_has_more;
static bool dm_itunes_total_known;
#ifdef HAVE_TAGCACHE
static uint32_t dm_itunes_uniqbuf[2048];
#endif
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

static enum dm_boot_result dm_show_boot_animation(void)
{
    struct bitmap background;
    struct bitmap spinners[DM_BOOT_FRAME_COUNT];
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

/* DMF1: "DMF1", cell width, cell height, ascent, glyph count, then one
 * advance per glyph.  Every glyph shares the same baseline inside its cell
 * and starts at the pen position, so drawing is a blit at the pen and a step
 * by the advance.
 */
static bool dm_load_font(enum dm_font_id id)
{
    const struct dm_font_def *def = &dm_font_defs[id];
    struct dm_font *font = &dm_fonts[id];
    unsigned char metrics[8 + DM_ASCII_COUNT];
    size_t bytes;

    if (!dm_read_exact(def->metrics, metrics, sizeof(metrics)))
        return false;
    if (metrics[0] != 'D' || metrics[1] != 'M' || metrics[2] != 'F' ||
        metrics[3] != '1' || metrics[7] != DM_ASCII_COUNT)
        return false;
    font->cell_w = metrics[4];
    font->cell_h = metrics[5];
    font->ascent = metrics[6];
    if (font->cell_w == 0 || font->cell_h == 0)
        return false;
    rb->memcpy(font->advance, metrics + 8, DM_ASCII_COUNT);
    bytes = (size_t)font->cell_w * DM_FONT_COLUMNS *
            (size_t)font->cell_h * DM_FONT_ROWS;
    font->coverage = dm_alloc(bytes);
    if (!font->coverage || !dm_read_exact(def->coverage, font->coverage, bytes))
        return false;
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
            if (dm_last_error == DM_ERR_NONE)
                dm_last_error = DM_ERR_ASSET_INVALID;
            return false;
        }
    }
    for (id = 0; id < DM_FONT_COUNT; id++)
    {
        if (!dm_load_font((enum dm_font_id)id))
        {
            if (dm_last_error == DM_ERR_NONE)
                dm_last_error = DM_ERR_ASSET_INVALID;
            return false;
        }
    }
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

#if LCD_WIDTH < 1920
static void dm_blit_region(enum dm_asset_id id, int source_x, int source_y,
                           int x, int y, int width, int height)
{
    const struct dm_asset *asset = &dm_assets[id];
    struct dm_rect target;
    int clipped_source_x;
    int clipped_source_y;
    int row;

    if (!asset->loaded || source_x < 0 || source_y < 0 ||
        source_x >= asset->width || source_y >= asset->height)
        return;
    width = MIN(width, asset->width - source_x);
    height = MIN(height, asset->height - source_y);
    target = (struct dm_rect){ x, y, width, height };
    if (!dm_rect_intersect(&target, &dm_paint_clip))
        return;
    clipped_source_x = source_x + target.x - x;
    clipped_source_y = source_y + target.y - y;
    for (row = 0; row < target.height; row++)
        rb->memcpy(dm_canvas + (target.y + row) * LCD_WIDTH + target.x,
                   asset->pixels +
                       (clipped_source_y + row) * asset->width +
                       clipped_source_x,
                   (size_t)target.width * sizeof(fb_data));
}
#endif

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
                int target_x = x + column;
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

/* Desktop labels are white with the same one-pixel shadow Finder uses, which
 * is what keeps them legible over the bright parts of the Aurora picture.
 */
static void dm_draw_desktop_label(int x, int y, int width, const char *text)
{
    dm_draw_text_centered(&dm_fonts[DM_FONT_SMALL], x + 1, y + 1, width,
                          DM_INK, text);
    dm_draw_text_centered(&dm_fonts[DM_FONT_SMALL], x, y, width,
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
    signature = dm_signature_add(signature, state->running_apps);
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
    dm_paint_clip = state->pending_damage;
    return full;
}

static void dm_present(struct dm_state *state, bool full)
{
    const struct dm_control *hover = NULL;
    const struct dm_rect *dirty = &state->pending_damage;

    if (state->controls->hover_index >= 0 &&
        state->controls->hover_index < state->controls->count)
        hover = &state->controls->controls[state->controls->hover_index];
    if (full)
    {
        rb->lcd_bitmap(dm_canvas, 0, 0, LCD_WIDTH, LCD_HEIGHT);
        rb->lcd_update();
        state->full_update_count++;
    }
    else
    {
        rb->lcd_bitmap_part(dm_canvas, dirty->x, dirty->y, LCD_WIDTH,
                            dirty->x, dirty->y,
                            dirty->width, dirty->height);
        rb->lcd_update_rect(dirty->x, dirty->y,
                            dirty->width, dirty->height);
        state->partial_update_count++;
    }
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
        case DM_APP_NETFLIX:
            return "Netflix";
        case DM_APP_PREFERENCES:
            return "System Preferences";
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

/* The menu bar names the Finder even with no window open, the way Mac OS X
 * does, but the "Start application" setting has to name the desktop itself.
 */
static const char *dm_start_app_name(enum dm_app app)
{
    if (app == DM_APP_DESKTOP)
        return "Desktop";
    return dm_short_app_name(app);
}

static void dm_draw_menu_bar(const struct dm_state *state)
{
    static const char * const weekdays[] =
        { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char * const menus[] = { "File", "Edit", "View", "Go" };
    const struct dm_font *regular = &dm_fonts[DM_FONT_REGULAR];
    const struct dm_font *bold = &dm_fonts[DM_FONT_BOLD];
    char clock[32];
    struct tm *now = rb->get_time();
    const char *name = dm_short_app_name(state->app);
    int hour = now ? now->tm_hour : 0;
    int x;
    int i;

    dm_blit(DM_ASSET_MENUBAR, 0, 0);
    if (state->overlay == DM_OVERLAY_APPLE_MENU)
        dm_blit(DM_ASSET_APPLE_HIGHLIGHT, DM_APPLE_X, 0);
    x = DM_APPLE_X + DM_APPLE_W + 4;
    dm_draw_text(bold, x, 4, 96, DM_INK, name);
    x += dm_text_width(bold, name) + 12;
    if (state->app != DM_APP_DESKTOP)
    {
        for (i = 0; i < (int)ARRAYLEN(menus); i++)
        {
            if (x + dm_text_width(regular, menus[i]) > 210)
                break;
            dm_draw_text(regular, x, 4, 40, DM_INK, menus[i]);
            x += dm_text_width(regular, menus[i]) + 12;
        }
    }
    rb->snprintf(clock, sizeof(clock), "%s %d:%02d %s",
                 now && now->tm_wday >= 0 && now->tm_wday < 7 ?
                    weekdays[now->tm_wday] : "Sun",
                 hour % 12 ? hour % 12 : 12,
                 now ? now->tm_min : 0,
                 hour >= 12 ? "PM" : "AM");
    dm_draw_text_right(regular, LCD_WIDTH - 6, 4, 96, DM_INK, clock);
}

static void dm_draw_desktop_icons(void)
{
    const int label_width = DM_DOCK_ICON + 32;
    const int label_x = DM_DESKTOP_ICON_X - 16;
    const int label_gap = 2;

    if (!dm_settings.show_desktop_folders)
        return;
    dm_compose(DM_ASSET_ICON_DISK, DM_DESKTOP_ICON_X, DM_DESKTOP_DISK_Y);
    dm_draw_desktop_label(label_x,
                          DM_DESKTOP_DISK_Y + DM_DOCK_ICON + label_gap,
                          label_width, "iPod");
    dm_compose(DM_ASSET_ICON_FOLDER_DESKTOP, DM_DESKTOP_ICON_X,
               DM_DESKTOP_DOCS_Y);
    dm_draw_desktop_label(label_x,
                          DM_DESKTOP_DOCS_Y + DM_DOCK_ICON + label_gap,
                          label_width, "Documents");
    dm_compose(DM_ASSET_ICON_FOLDER_DESKTOP, DM_DESKTOP_ICON_X,
               DM_DESKTOP_MUSIC_Y);
    dm_draw_desktop_label(label_x,
                          DM_DESKTOP_MUSIC_Y + DM_DOCK_ICON + label_gap,
                          label_width, "Music");
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
            (LCD_WIDTH - DM_ITUNES_W) / 2,
#if LCD_WIDTH >= 1920
            DM_MENUBAR_H + (LCD_HEIGHT - DM_MENUBAR_H -
                            DM_DOCK_BAND - DM_ITUNES_H) / 2,
#else
            DM_MENUBAR_H,
#endif
            DM_ITUNES_W,
            DM_ITUNES_H
        };
    return (struct dm_rect){ DM_WIN_X, DM_WIN_Y, DM_WIN_W, DM_WIN_H };
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
    }
    animation->active = false;
    state->redraw = true;
}

static void dm_start_animation(struct dm_state *state, enum dm_app app,
                               bool minimizing)
{
    int index = dm_dock_index(app);
    struct dm_rect window = dm_app_window_rect(app);
    struct dm_rect icon;

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

static void dm_draw_dock(const struct dm_state *state)
{
    int i;

    dm_blit(DM_ASSET_DOCK_SHELF, DM_DOCK_SHELF_X, DM_DOCK_SHELF_Y);
    for (i = 0; i < DM_DOCK_SLOTS; i++)
    {
        struct dm_rect rect = dm_dock_rect(i);
        enum dm_asset_id asset = dm_dock_assets[i];
        int x = rect.x;
        int y = rect.y;

        if (state->hover_dock == i)
        {
            asset = dm_dock_assets_38[i];
            x -= 3;
            y -= 6;
        }
        else if (state->hover_dock >= 0 &&
                 DM_ABS(state->hover_dock - i) == 1)
        {
            asset = dm_dock_assets_34[i];
            x -= 1;
            y -= 2;
        }
        dm_compose(asset, x, y);
        if ((state->running_apps & DM_APP_BIT(dm_dock_apps[i])) != 0)
            dm_compose(DM_ASSET_DOCK_INDICATOR, rect.x + 9,
                       DM_DOCK_ICON_Y + DM_DOCK_ICON + 2);
    }
    if (state->hover_dock >= 0)
    {
        const char *label = dm_dock_labels[state->hover_dock];
        int width = MIN(DM_TOOLTIP_W,
                        dm_text_width(&dm_fonts[DM_FONT_SMALL], label) + 16);
        int x = dm_dock_rect(state->hover_dock).x + DM_DOCK_ICON / 2 -
                width / 2;

        x = MAX(2, MIN(x, LCD_WIDTH - width - 2));
        dm_blit_panel(DM_ASSET_TOOLTIP, x, DM_TOOLTIP_Y, width,
                      DM_TOOLTIP_CAP);
        dm_draw_text_centered(&dm_fonts[DM_FONT_SMALL], x, DM_TOOLTIP_Y + 4,
                              width, DM_INK, label);
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
    { "DEVICES", NULL, DM_BODY_Y + 2, 14, true },
    { "iPod", "/", DM_BODY_Y + 16, 16, false },
    { "PLACES", NULL, DM_BODY_Y + 34, 14, true },
    { "Applications", PLUGIN_APPS_DIR, DM_BODY_Y + 48, 16, false },
    { "Music", "/Music", DM_BODY_Y + 64, 16, false },
    { "Videos", "/Videos", DM_BODY_Y + 80, 16, false },
};

/* Finder titles the window after the folder, not the whole path. */
static const char *dm_window_title(void)
{
    const char *slash = rb->strrchr(dm_cwd, '/');

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

    dm_blit(DM_ASSET_WINDOW_SIDEBAR, DM_WIN_X, DM_WIN_Y);
    dm_draw_text_centered(&dm_fonts[DM_FONT_BOLD], DM_WIN_X, DM_WIN_Y + 5,
                          DM_WIN_W, DM_INK_TITLE, dm_window_title());

    for (i = 0; i < (int)ARRAYLEN(dm_sidebar_rows); i++)
    {
        const struct dm_sidebar_row *row = &dm_sidebar_rows[i];

        if (row->header)
        {
            dm_draw_text(small, DM_WIN_X + 6, row->y + 2, 74,
                         DM_INK_SIDEBAR_HEAD, row->label);
            continue;
        }
        if (!rb->strcmp(dm_cwd, row->path))
        {
            dm_blit(DM_ASSET_SIDEBAR_SELECTION, DM_WIN_X, row->y);
            dm_draw_text(small, DM_WIN_X + 18, row->y + 3, 62,
                         DM_INK_WHITE, row->label);
        }
        else
            dm_draw_text(small, DM_WIN_X + 18, row->y + 3, 62,
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
        dm_draw_text(regular, DM_LIST_X + 24, y + 3,
                     DM_LIST_W - 30 -
                        (dm_file_count > DM_FILE_ROWS ? DM_SCROLLER_W : 0),
                     ink, dm_files[index].name);
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
    dm_draw_text_centered(small, DM_WIN_X, DM_STATUS_Y + 7, DM_WIN_W,
                          DM_INK_STATUS, status);
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

    dm_blit(DM_ASSET_WINDOW_PLAIN, DM_WIN_X, DM_WIN_Y);
    dm_draw_text_centered(&dm_fonts[DM_FONT_BOLD], DM_WIN_X, DM_WIN_Y + 5,
                          DM_WIN_W, DM_INK_TITLE, title);
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
        dm_draw_text(regular, DM_WIN_X + 28, y + 3, DM_WIN_W - 40, ink,
                     dm_files[index].name);
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
        dm_draw_text_centered(regular, DM_WIN_X, DM_BODY_Y + 30, DM_WIN_W,
                              DM_INK_STATUS, "Nothing here");
    if (!status_text)
    {
        rb->snprintf(status, sizeof(status), "%d item%s", dm_file_count,
                     dm_file_count == 1 ? "" : "s");
        status_text = status;
    }
    dm_draw_text_centered(small, DM_WIN_X, DM_STATUS_Y + 7, DM_WIN_W,
                          DM_INK_STATUS, status_text);
}

static void dm_draw_photos(void)
{
    dm_draw_media_window("Photos", DM_ASSET_ICON_PREVIEW, NULL);
}

#define DM_ITUNES_X ((LCD_WIDTH - DM_ITUNES_W) / 2)
#if LCD_WIDTH >= 1920
#define DM_ITUNES_Y (DM_MENUBAR_H + (LCD_HEIGHT - DM_MENUBAR_H - \
                     DM_DOCK_BAND - DM_ITUNES_H) / 2)
#else
#define DM_ITUNES_Y DM_MENUBAR_H
#endif
#define DM_ITUNES_BODY_LOCAL_Y (22 + 41 + 17)
#define DM_ITUNES_BODY_Y (DM_ITUNES_Y + DM_ITUNES_BODY_LOCAL_Y)
#define DM_ITUNES_ROW_H 17
#define DM_ITUNES_BOTTOM_H 24
#define DM_ITUNES_ROWS ((DM_ITUNES_H - 22 - 41 - 17 - \
                         DM_ITUNES_BOTTOM_H) / DM_ITUNES_ROW_H)
#define DM_ITUNES_LIST_X (DM_ITUNES_X + DM_ITUNES_SOURCE_W + 1)
#define DM_ITUNES_LIST_W (DM_ITUNES_W - DM_ITUNES_SOURCE_W - 2)

static const char * const dm_itunes_source_names[DM_ITUNES_SOURCE_COUNT] =
{
    "Albums", "Artists", "Songs", "Videos"
};

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
    dm_itunes_commit_rows();
}

#ifdef HAVE_TAGCACHE
static void dm_itunes_load_tagcache(void)
{
    const struct tagcache_stat *stat = rb->tagcache_get_stat();
    struct tagcache_search search;
    char value[MAX_PATH];
    int tag = dm_itunes_source == DM_ITUNES_ALBUMS ? tag_album :
              dm_itunes_source == DM_ITUNES_ARTISTS ? tag_artist : tag_title;
    int page_size = MIN(DM_ITUNES_PAGE_MAX, DM_ITUNES_ROWS);
    int absolute = 0;

    dm_itunes_count = 0;
    dm_itunes_total = 0;
    dm_itunes_has_more = false;
    dm_itunes_total_known = false;
    if (!stat || !stat->readyvalid || !stat->ready)
    {
        dm_itunes_database_pending = true;
        dm_itunes_commit_rows();
        return;
    }
    dm_itunes_database_pending = false;
    if (!rb->tagcache_search(&search, tag))
    {
        dm_itunes_commit_rows();
        return;
    }
    if (tag != tag_title)
        rb->tagcache_search_set_uniqbuf(&search, dm_itunes_uniqbuf,
                                       sizeof(dm_itunes_uniqbuf));
    while (rb->tagcache_get_next(&search, value, sizeof(value)))
    {
        struct dm_itunes_row *row;

        if (absolute >= dm_itunes_page_top)
        {
            if (dm_itunes_count >= page_size)
            {
                dm_itunes_has_more = true;
                break;
            }
            row = &dm_itunes_rows[dm_itunes_count++];
            rb->memset(row, 0, sizeof(*row));
            rb->strlcpy(row->title, value, sizeof(row->title));
            row->idxid = search.idx_id;
            row->seek = search.result_seek;
            if (tag == tag_title)
            {
                rb->tagcache_retrieve(&search, search.idx_id, tag_filename,
                                      row->path, sizeof(row->path));
                rb->tagcache_retrieve(&search, search.idx_id, tag_artist,
                                      row->artist, sizeof(row->artist));
                rb->tagcache_retrieve(&search, search.idx_id, tag_album,
                                      row->album, sizeof(row->album));
            }
        }
        absolute++;
    }
    rb->tagcache_search_finish(&search);
    dm_itunes_total_known = !dm_itunes_has_more;
    if (tag == tag_title)
    {
        dm_itunes_total = stat->total_entries;
        dm_itunes_total_known = true;
    }
    else
        dm_itunes_total = absolute + (dm_itunes_has_more ? 1 : 0);
    dm_itunes_commit_rows();
}
#endif

static void dm_itunes_load(void)
{
    if (dm_itunes_source == DM_ITUNES_VIDEOS)
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
    int page_size = MIN(DM_ITUNES_PAGE_MAX, DM_ITUNES_ROWS);
    int next = dm_itunes_page_top + direction * page_size;

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
    dm_blit(DM_ASSET_ITUNES_WINDOW, DM_ITUNES_X, DM_ITUNES_Y);
#else
    const int captured_body_h = DM_ITUNES_ROW_H * 2;
    const int body_h = DM_ITUNES_H - DM_ITUNES_BODY_LOCAL_Y -
                       DM_ITUNES_BOTTOM_H;
    int offset = 0;

    dm_blit_region(DM_ASSET_ITUNES_WINDOW, 0, 0,
                   DM_ITUNES_X, DM_ITUNES_Y,
                   DM_ITUNES_W, DM_ITUNES_BODY_LOCAL_Y);
    while (offset < body_h)
    {
        int height = MIN(captured_body_h, body_h - offset);

        dm_blit_region(DM_ASSET_ITUNES_WINDOW, 0,
                       DM_ITUNES_BODY_LOCAL_Y,
                       DM_ITUNES_X, DM_ITUNES_BODY_Y + offset,
                       DM_ITUNES_W, height);
        offset += height;
    }
    dm_blit_region(DM_ASSET_ITUNES_WINDOW, 0,
                   DM_ITUNES_ASSET_H - DM_ITUNES_BOTTOM_H,
                   DM_ITUNES_X,
                   DM_ITUNES_Y + DM_ITUNES_H - DM_ITUNES_BOTTOM_H,
                   DM_ITUNES_W, DM_ITUNES_BOTTOM_H);
#endif
}

static void dm_draw_itunes(void)
{
    const struct mp3entry *track = rb->audio_current_track();
    const struct dm_font *regular = &dm_fonts[DM_FONT_REGULAR];
    const struct dm_font *small = &dm_fonts[DM_FONT_SMALL];
    char status[64];
    char footer[64];
    int list_width = DM_ITUNES_LIST_W;
    int name_width = list_width * 45 / 100;
    int artist_width = list_width * 25 / 100;
    int album_width = list_width - name_width - artist_width;
    int i;

    /* The furniture is sampled from the owned iTunes capture. Runtime drawing
     * is limited to library data, labels, and the native selection gradients. */
    dm_draw_itunes_chrome();
    dm_draw_text_centered(small, DM_ITUNES_X, DM_ITUNES_Y + 6, DM_ITUNES_W,
                          DM_INK_TITLE, "iTunes");
    dm_draw_text(small, DM_ITUNES_X + 7, DM_ITUNES_BODY_Y - 14,
                 DM_ITUNES_SOURCE_W - 12,
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
        dm_draw_text(small, DM_ITUNES_X + 14, y + 2,
                     DM_ITUNES_SOURCE_W - 20, ink,
                     dm_itunes_source_names[i]);
    }
    dm_draw_text(small, DM_ITUNES_LIST_X + 6, DM_ITUNES_BODY_Y - 14,
                 name_width - 8, DM_INK, "Name");
    if (LCD_WIDTH > 320)
    {
        dm_draw_text(small, DM_ITUNES_LIST_X + name_width + 5,
                     DM_ITUNES_BODY_Y - 14, artist_width - 8,
                     DM_INK, "Artist");
        dm_draw_text(small, DM_ITUNES_LIST_X + name_width + artist_width + 5,
                     DM_ITUNES_BODY_Y - 14, album_width - 8,
                     DM_INK, "Album");
    }
    for (i = 0; i < DM_ITUNES_ROWS; i++)
    {
        int y = DM_ITUNES_BODY_Y + i * DM_ITUNES_ROW_H;
        fb_data ink = DM_INK;

        if (i >= dm_itunes_count)
            break;
        if (i == dm_file_selected)
        {
            dm_blit_part(DM_ASSET_ITUNES_SELECTION, DM_ITUNES_LIST_X, y,
                         DM_ITUNES_LIST_W);
            ink = DM_INK_WHITE;
        }
        dm_draw_text(regular, DM_ITUNES_LIST_X + 6, y + 1,
                     (LCD_WIDTH > 320 ? name_width : list_width) - 8, ink,
                     dm_itunes_rows[i].title);
        if (LCD_WIDTH > 320)
        {
            dm_draw_text(regular, DM_ITUNES_LIST_X + name_width + 5, y + 1,
                         artist_width - 8, ink, dm_itunes_rows[i].artist);
            dm_draw_text(regular,
                         DM_ITUNES_LIST_X + name_width + artist_width + 5,
                         y + 1, album_width - 8, ink,
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
    dm_draw_text_centered(small, DM_ITUNES_X + 150,
                          DM_ITUNES_Y + 22 + 14,
                          DM_ITUNES_W - 185, DM_INK, status);
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
    dm_draw_text_centered(small, DM_ITUNES_LIST_X,
                          DM_ITUNES_Y + DM_ITUNES_H - 18,
                          DM_ITUNES_LIST_W - 38, DM_INK, footer);
    dm_draw_text_centered(small, DM_ITUNES_X + DM_ITUNES_W - 38,
                          DM_ITUNES_Y + DM_ITUNES_H - 18, 18,
                          DM_INK, "<");
    dm_draw_text_centered(small, DM_ITUNES_X + DM_ITUNES_W - 20,
                          DM_ITUNES_Y + DM_ITUNES_H - 18, 18,
                          DM_INK, ">");
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
    const struct dm_font *regular = &dm_fonts[DM_FONT_REGULAR];
    char value[24];
    int i;

    dm_blit(DM_ASSET_WINDOW_PLAIN, DM_WIN_X, DM_WIN_Y);
    dm_draw_text_centered(&dm_fonts[DM_FONT_BOLD], DM_WIN_X, DM_WIN_Y + 5,
                          DM_WIN_W, DM_INK_TITLE, "System Preferences");
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
        dm_draw_text(regular, DM_WIN_X + 14, y + 2, 190, ink, labels[i]);
        if (i == 0)
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
        dm_draw_text_right(regular, DM_WIN_X + DM_WIN_W - 16, y + 2, 80,
                           ink, value);
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
        dm_draw_text(&dm_fonts[DM_FONT_REGULAR], DM_MENU_X + 10, y + 3,
                     DM_MENU_W - 20, ink, dm_apple_menu_labels[i]);
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
        dm_draw_text(&dm_fonts[DM_FONT_REGULAR], DM_CONTEXT_X + 10, y + 3,
                     DM_CONTEXT_W - 20, ink, dm_context_labels[i]);
    }
}

static void dm_draw_sheet_row(int row, const char *label, const char *value)
{
    int y = DM_SHEET_Y + 34 + row * 16;

    dm_draw_text(&dm_fonts[DM_FONT_SMALL], DM_SHEET_X + 16, y, 66,
                 DM_INK_STATUS, label);
    dm_draw_text(&dm_fonts[DM_FONT_SMALL], DM_SHEET_X + 84, y,
                 DM_SHEET_W - 100, DM_INK, value);
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
    dm_draw_text(&dm_fonts[DM_FONT_BOLD], DM_SHEET_X + 52, DM_SHEET_Y + 12,
                 DM_SHEET_W - 64, DM_INK, name);
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
    dm_draw_text_centered(&dm_fonts[DM_FONT_BOLD], DM_SHEET_X,
                          DM_SHEET_Y + 10, DM_SHEET_W, DM_INK,
                          "Desktop Diagnostics");
    dm_draw_sheet_row(0, "Memory:", memory);
    dm_draw_sheet_row(1, "Controls:", controls);
    dm_draw_sheet_row(2, "Frame:", frame);
    dm_draw_sheet_row(3, "Updates:", updates);
    dm_draw_sheet_row(4, "Error:", dm_error_name(dm_last_error));
}

static void dm_draw_confirm(const struct dm_state *state)
{
    const char *title = "Return to the iPod menu?";

    if (state->overlay == DM_OVERLAY_RESTART_CONFIRM)
        title = "Restart Rockbox now?";
    else if (state->overlay == DM_OVERLAY_DELETE_CONFIRM)
        title = "Delete the selected item?";
    else if (state->overlay == DM_OVERLAY_NEW_FOLDER_CONFIRM)
        title = "Create an untitled folder?";
    dm_blit(DM_ASSET_SHEET, DM_SHEET_X, DM_SHEET_Y);
    dm_draw_text_centered(&dm_fonts[DM_FONT_BOLD], DM_SHEET_X,
                          DM_SHEET_Y + 34, DM_SHEET_W, DM_INK, title);
    dm_draw_text_centered(&dm_fonts[DM_FONT_SMALL], DM_SHEET_X,
                          DM_SHEET_Y + 62, DM_SHEET_W, DM_INK_STATUS,
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

static void dm_draw(struct dm_state *state)
{
    uint32_t signature;
    long started = *rb->current_tick;
    bool full;

    dm_update_hover(state);
    signature = dm_scene_signature(state);
    full = state->damage_full || state->frame_count == 0 ||
           signature != state->scene_signature;
    full = dm_prepare_damage(state, full);
    dm_blit(DM_ASSET_AURORA, 0, 0);
    dm_draw_desktop_icons();
    if (state->animation.active)
        dm_draw_animation(state);
    else if (state->app == DM_APP_FINDER)
        dm_draw_finder(state);
    else if (state->app == DM_APP_ITUNES)
        dm_draw_itunes();
    else if (state->app == DM_APP_PREVIEW)
        dm_draw_photos();
    else if (state->app == DM_APP_PREFERENCES)
        dm_draw_preferences(state);
    dm_draw_menu_bar(state);
    /* The 320x240 iTunes window uses the whole work area.  Reveal the Dock
     * during its genie transition and everywhere else, but do not lay it over
     * the media list once iTunes has settled. */
    if (LCD_WIDTH > 320 || state->app != DM_APP_ITUNES ||
        state->animation.active)
        dm_draw_dock(state);
    if (!state->animation.active)
    {
        if (state->overlay == DM_OVERLAY_APPLE_MENU)
            dm_draw_apple_menu(state);
        else if (state->overlay == DM_OVERLAY_FINDER_CONTEXT)
            dm_draw_finder_context_menu(state);
        else if (state->overlay == DM_OVERLAY_GET_INFO)
            dm_draw_get_info();
        else if (state->overlay == DM_OVERLAY_DIAGNOSTICS)
            dm_draw_diagnostics(state);
        else if (state->overlay == DM_OVERLAY_RETURN_CONFIRM ||
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
        dm_draw_text_centered(&dm_fonts[DM_FONT_SMALL], x,
                              DM_MENUBAR_H + 10, width, DM_INK, hint);
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

static void dm_parent_dir(char *path)
{
    char *slash;
    int length = rb->strlen(path);

    if (length <= 1)
    {
        rb->strcpy(path, "/");
        return;
    }
    if (path[length - 1] == '/')
        path[--length] = '\0';
    slash = rb->strrchr(path, '/');
    if (!slash || slash == path)
        rb->strcpy(path, "/");
    else
        *slash = '\0';
}

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

    if (next < 0 || next >= dm_history_count)
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

/* Play a track chosen in the iTunes window.
 *
 * This is the one place Desktop Mode builds a playlist, and it does so only
 * because the user asked for this track by name: it is the whole point of a
 * music application.  Everything else in the shell still leaves the user's
 * playlist alone - iTunes reports playback rather than commanding it, and
 * nothing here runs unless a track was double-clicked.
 */
static int dm_play_selected_track(void)
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
        rb->splash(HZ, "Could not start playback");
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
        rb->splash(HZ, "No playable music");
        return PLUGIN_OK;
    }
    rb->playlist_start(index, 0, 0);
    return PLUGIN_OK;
}

static int dm_open_selected_file(void)
{
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
        return dm_play_selected_track();

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
            rb->splash(HZ, "No viewer");
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
static int dm_open_itunes_selection(void)
{
    if (dm_file_selected < 0 || dm_file_selected >= dm_itunes_count)
        return PLUGIN_OK;
    if (dm_itunes_source == DM_ITUNES_VIDEOS)
        return dm_open_selected_file();

#ifdef HAVE_TAGCACHE
    {
        const struct dm_itunes_row *selected =
            &dm_itunes_rows[dm_file_selected];
        struct tagcache_search search;
        char title[DM_ITUNES_TITLE_MAX];
        char path[MAX_PATH];
        int filter_tag = dm_itunes_source == DM_ITUNES_ALBUMS ? tag_album :
                         dm_itunes_source == DM_ITUNES_ARTISTS ? tag_artist :
                         -1;
        int start_index = 0;
        int count = 0;
        int i;

        /* A song double-click should begin immediately on the iPod.  Building
         * a 2,000+ item playlist synchronously made the whole desktop appear
         * frozen.  Queue the visible iTunes page instead; Album and Artist
         * selections still query their complete, much smaller result set. */
        if (dm_itunes_source == DM_ITUNES_SONGS)
        {
            if (!selected->path[0] || !rb->file_exists(selected->path))
            {
                rb->splash(HZ, "Music file is missing");
                return PLUGIN_OK;
            }
            if (rb->playlist_create(NULL, NULL) < 0)
            {
                rb->splash(HZ, "Could not start playback");
                return PLUGIN_OK;
            }
            for (i = 0; i < dm_itunes_count; i++)
            {
                const char *song = dm_itunes_rows[i].path;

                if (!song[0] || !rb->file_exists(song))
                    continue;
                if (i == dm_file_selected)
                    start_index = count;
                if (rb->playlist_insert_track(NULL, song,
                                              PLAYLIST_INSERT_LAST,
                                              false, true) < 0)
                    break;
                count++;
            }
            if (count == 0)
            {
                rb->splash(HZ, "No playable music");
                return PLUGIN_OK;
            }
            rb->playlist_start(start_index, 0, 0);
            return PLUGIN_OK;
        }

        if (!rb->tagcache_search(&search, tag_title))
        {
            rb->splash(HZ, "Music database unavailable");
            return PLUGIN_OK;
        }
        if (filter_tag >= 0 &&
            !rb->tagcache_search_add_filter(&search, filter_tag,
                                            selected->seek))
        {
            rb->tagcache_search_finish(&search);
            rb->splash(HZ, "Music selection unavailable");
            return PLUGIN_OK;
        }
        if (rb->playlist_create(NULL, NULL) < 0)
        {
            rb->tagcache_search_finish(&search);
            rb->splash(HZ, "Could not start playback");
            return PLUGIN_OK;
        }
        while (rb->tagcache_get_next(&search, title, sizeof(title)))
        {
            int idxid = search.idx_id;

            if (!rb->tagcache_retrieve(&search, idxid, tag_filename,
                                       path, sizeof(path)))
                continue;
            if (!rb->file_exists(path))
                continue;
            if (rb->playlist_insert_track(NULL, path, PLAYLIST_INSERT_LAST,
                                          false, true) < 0)
                break;
            if (filter_tag < 0 && idxid == selected->idxid)
                start_index = count;
            count++;
            if ((count & 0xf) == 0)
                rb->yield();
        }
        rb->tagcache_search_finish(&search);
        if (count == 0)
        {
            rb->splash(HZ, "No playable music");
            return PLUGIN_OK;
        }
        rb->playlist_start(start_index, 0, 0);
    }
#else
    rb->splash(HZ, "Music database unavailable");
#endif
    return PLUGIN_OK;
}

static void dm_load_settings(void)
{
    char line[96];
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
        if (!rb->strcmp(name, "snow pointer speed"))
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
    char buffer[512];
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
        dm_cwd);
    rb->write(fd, buffer, length);
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
    registry->high_water = MAX(registry->high_water, registry->count);
}

static void dm_build_controls(struct dm_state *state)
{
    struct dm_control_registry *registry = state->controls;
    int i;

    registry->count = 0;
    registry->hover_index = -1;
    if (state->animation.active)
        return;

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
    if (state->overlay == DM_OVERLAY_GET_INFO ||
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
    if (LCD_WIDTH > 320 || state->app != DM_APP_ITUNES)
    {
        for (i = 0; i < DM_DOCK_SLOTS; i++)
            dm_register_control(state, DM_ACTION_DOCK_APP, i,
                                dm_dock_rect(i), DM_CONTROL_HAND);
    }
    if (state->app != DM_APP_DESKTOP)
    {
        dm_register_control(state, DM_ACTION_CLOSE, 0, DM_CLOSE_RECT,
                            DM_CONTROL_HAND);
        dm_register_control(state, DM_ACTION_MINIMISE, 0, DM_MINIMISE_RECT,
                            DM_CONTROL_HAND);
    }
    if (state->app == DM_APP_FINDER)
    {
        dm_register_control(state, DM_ACTION_FINDER_BACK, 0, DM_BACK_RECT,
                            DM_CONTROL_HAND);
        dm_register_control(state, DM_ACTION_FINDER_FORWARD, 0,
                            DM_FORWARD_RECT, DM_CONTROL_HAND);
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
        for (i = 0; i < (int)ARRAYLEN(dm_sidebar_rows); i++)
        {
            const struct dm_sidebar_row *row = &dm_sidebar_rows[i];

            if (!row->header)
                dm_register_control(
                    state, DM_ACTION_FINDER_SIDEBAR, i,
                    (struct dm_rect){ DM_WIN_X, row->y,
                                      DM_WIN_SIDEBAR_W, row->height },
                    DM_CONTROL_HAND | DM_CONTROL_FOCUSABLE);
        }
    }
    else if (state->app == DM_APP_ITUNES)
    {
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
        for (i = 0; i < DM_ITUNES_SOURCE_COUNT; i++)
            dm_register_control(
                state, DM_ACTION_ITUNES_SOURCE, i,
                (struct dm_rect){ DM_ITUNES_X + 1,
                                  DM_ITUNES_BODY_Y + i * DM_ITUNES_ROW_H,
                                  DM_ITUNES_SOURCE_W - 2, DM_ITUNES_ROW_H },
                DM_CONTROL_HAND | DM_CONTROL_FOCUSABLE);
        for (i = 0; i < DM_ITUNES_ROWS && i < dm_itunes_count; i++)
            dm_register_control(
                state, DM_ACTION_FILE_ROW, i,
                (struct dm_rect){ DM_ITUNES_LIST_X,
                                  DM_ITUNES_BODY_Y + i * DM_ITUNES_ROW_H,
                                  DM_ITUNES_LIST_W, DM_ITUNES_ROW_H },
                DM_CONTROL_DOUBLE_CLICK | DM_CONTROL_FOCUSABLE);
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
    else if (state->app == DM_APP_PREFERENCES)
    {
        for (i = 0; i < DM_PREF_ROWS && i < 6; i++)
            dm_register_control(
                state, DM_ACTION_PREFERENCE, i,
                (struct dm_rect){ DM_WIN_X + 6,
                                  DM_BODY_Y + i * DM_PREF_ROW_H,
                                  DM_WIN_W - 12, DM_PREF_ROW_H },
                DM_CONTROL_HAND | DM_CONTROL_FOCUSABLE);
    }
    else if (state->app == DM_APP_DESKTOP)
    {
        dm_register_control(
            state, DM_ACTION_DESKTOP_FOLDER, 0,
            (struct dm_rect){ DM_DESKTOP_ICON_X - 16,
                              DM_DESKTOP_DISK_Y, 64, 48 },
            DM_CONTROL_DOUBLE_CLICK);
        if (dm_settings.show_desktop_folders)
        {
            dm_register_control(
                state, DM_ACTION_DESKTOP_FOLDER, 1,
                (struct dm_rect){ DM_DESKTOP_ICON_X - 16,
                                  DM_DESKTOP_DOCS_Y, 64, 48 },
                DM_CONTROL_DOUBLE_CLICK);
            dm_register_control(
                state, DM_ACTION_DESKTOP_FOLDER, 2,
                (struct dm_rect){ DM_DESKTOP_ICON_X - 16,
                                  DM_DESKTOP_MUSIC_Y, 64, 48 },
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

    dm_build_controls(state);
    state->hover_dock = -1;
    state->hover_row = -1;
    state->menu_row = -1;
    state->preference_row = -1;
    control = dm_control_at(state, state->cursor_x, state->cursor_y);
    if (!control)
        return;
    state->controls->hover_index = control - state->controls->controls;
    if (control->action == DM_ACTION_DOCK_APP)
        state->hover_dock = control->value;
    else if (control->action == DM_ACTION_FILE_ROW)
        state->hover_row = control->value;
    else if (control->action == DM_ACTION_APPLE_MENU_ROW ||
             control->action == DM_ACTION_CONTEXT_MENU_ROW)
        state->menu_row = control->value;
    else if (control->action == DM_ACTION_PREFERENCE)
        state->preference_row = control->value;
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

    if (state->overlay == DM_OVERLAY_NONE &&
        state->app != DM_APP_PREFERENCES)
        return false;
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
#define DM_HOST_POINTER_POLL_TICKS MAX(1, HZ / 50)
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
    bool moved = false;
    bool down;
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
            state->mouse_down = true;
            state->dragged = false;
            state->mouse_down_tick = *rb->current_tick;
        }
        else
        {
            bool double_click =
                TIME_BEFORE(now, state->last_click_tick +
                                 DM_DOUBLE_CLICK_TICKS) &&
                DM_ABS(state->cursor_x - state->last_click_x) <= 5 &&
                DM_ABS(state->cursor_y - state->last_click_y) <= 5;

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
#endif /* SIMULATOR */

#ifdef HAVE_WHEEL_POSITION
/* Resting a finger on the wheel keeps the pointer travelling.
 *
 * Tangential motion alone means crossing the screen takes repeated strokes,
 * and reaching a corner is tedious.  Holding a finger still on the ring -
 * touching, not clicking - glides the pointer the way that point of the ring
 * faces: the top of the wheel is up, the right is right, and so on.  The
 * glide starts after a short delay so an ordinary stroke that happens to
 * pause mid-way does not drift, and it accelerates to a cap the longer it is
 * held.
 *
 * Movement accumulates in sixteenths of a pixel so slow glides stay smooth
 * instead of stepping a whole pixel per sample.
 */
/* A packet gap longer than this is a real lift rather than the ordinary
 * silence between clickwheel packets. */
#define DM_WHEEL_LIFT_TICKS (HZ / 5)
#define DM_GLIDE_DELAY 0
#define DM_GLIDE_RAMP (HZ / 2)
#define DM_GLIDE_SUBPIXEL 16
/* sixteenths of a pixel per tick: about 6 to 40 pixels a second */
#define DM_GLIDE_MIN_RATE 4
#define DM_GLIDE_MAX_RATE 13

/* Every button on the ring is also a position on it, so a finger pressing
 * Menu is a finger resting at the top of the wheel.  Gliding then would drag
 * the pointer around while the Apple menu or the return sheet is being
 * raised, so a held button suppresses the glide entirely.
 */
static bool dm_ring_button_down(void)
{
    long status = rb->button_status();

    return (status & (BUTTON_MENU | BUTTON_PLAY | BUTTON_LEFT |
                      BUTTON_RIGHT)) != 0;
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
    state->contact_tick = 0;
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
    {
        dm_glide_reset(state, now);
        return false;
    }
    if (dm_ring_button_down())
    {
        state->glide_tick = now;
        return false;
    }
    held = now - state->glide_since;
    if (held < DM_GLIDE_DELAY)
    {
        state->glide_tick = now;
        return false;
    }
    /* Displacement follows elapsed ticks, not how often this happens to be
     * polled, so the pointer travels at the same speed on a 6G and a 5G and
     * whether or not a window is being composed. */
    ticks = now - state->glide_tick;
    if (ticks <= 0)
        return false;
    state->glide_tick = now;

    rate = DM_GLIDE_MIN_RATE +
           (int)((held - DM_GLIDE_DELAY) * (DM_GLIDE_MAX_RATE -
                                            DM_GLIDE_MIN_RATE) /
                 MAX(1, DM_GLIDE_RAMP));
    rate = MIN(DM_GLIDE_MAX_RATE, rate) * MAX(1, dm_settings.pointer_speed) / 2;
    rate = (int)MIN((long)rate * ticks, (long)DM_GLIDE_MAX_RATE * HZ);

    /* wheel position 0 is the top of the ring; the pointer follows the
     * direction that point faces, so the ring reads like a compass. */
    angle = wheel * 360 / 96 - 90;
    state->glide_x += fp14_cos(angle) * rate / 16384;
    state->glide_y += fp14_sin(angle) * rate / 16384;
    if (dm_settings.reverse_wheel)
    {
        state->glide_x = -state->glide_x;
        state->glide_y = -state->glide_y;
    }
    /* Displacement integrates every poll, but a frame is only committed at
     * the interaction refresh rate: a glide must not drive the compositor
     * faster than ordinary interaction does. */
    if (TIME_BEFORE(now, state->glide_emit + HZ / 20))
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
    int scale;
    int delta;
    int direction;
    int old_x;
    int old_y;
    long now = *rb->current_tick;
    long elapsed;

    /* The driver publishes a position only on the polls that carried a new
     * clickwheel packet and reports -1 on every other one, so a single -1 is
     * not a lift - a motionless finger produces a long run of them.  Contact
     * is treated as held until the gap exceeds the lift timeout, which is
     * what makes a resting finger distinguishable from a lifted one.
     */
    if (wheel < 0)
    {
        if (state->last_wheel >= 0 &&
            TIME_BEFORE(now, state->contact_tick + DM_WHEEL_LIFT_TICKS))
            return dm_glide_wheel(state, state->last_wheel, now);
        dm_wheel_reset(state);
        return false;
    }
    state->wheel_available = true;
    state->contact_tick = now;
    if (state->last_wheel < 0)
    {
        state->last_wheel = wheel;
        state->last_wheel_tick = now;
        dm_glide_reset(state, now);
        return false;
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
    /* Hold the anchor while the movement is below the jitter threshold, so a
     * slow stroke accumulates across samples until it clears it rather than
     * being discarded a count at a time. */
    if (DM_ABS(delta) <= DM_WHEEL_JITTER_COUNTS)
        return false;
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
    scale = dm_settings.pointer_speed + state->wheel_velocity - 1;
    /* Carry the fraction of a pixel each sample leaves behind.  Truncating it
     * away made every small movement round to nothing, which is what made
     * fine positioning feel dead; keeping it means the pointer travels in
     * proportion to the finger however slowly it moves. */
    state->frac_x += dx * scale / 128;
    state->frac_y += dy * scale / 128;
    dx = state->frac_x / DM_WHEEL_SUBPIXEL;
    dy = state->frac_y / DM_WHEEL_SUBPIXEL;
    dx = MAX(-14, MIN(14, dx));
    dy = MAX(-14, MIN(14, dy));
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

static int dm_launch_external(const char *path, const char *parameter)
{
    int result;

    if (!rb->file_exists(path))
    {
        rb->splash(HZ, "Application is not installed");
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
            result = dm_launch_external(PLUGIN_APPS_DIR "/photos.rock", NULL);
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
            state->app = DM_APP_DESKTOP;
            result = dm_launch_external(PLUGIN_APPS_DIR "/text_editor.rock",
                                        NULL);
            state->running_apps &= ~DM_APP_BIT(app);
            return result;
        case DM_APP_CALCULATOR:
            state->app = DM_APP_DESKTOP;
            result = dm_launch_external(PLUGIN_APPS_DIR "/calculator.rock",
                                        NULL);
            state->running_apps &= ~DM_APP_BIT(app);
            return result;
        case DM_APP_DIRECTV:
            /* The decoder, mixer channel and window chrome live in one
             * mpegplayer instance. Commit the idle desktop first so that
             * instance can preserve it behind its Aqua window. */
            state->app = DM_APP_DESKTOP;
            state->damage_full = true;
            dm_draw(state);
            if (!dm_save_livetv_underlay())
            {
                rb->splash(HZ * 2, "Could not prepare TV window");
                state->running_apps &= ~DM_APP_BIT(app);
                return PLUGIN_ERROR;
            }
            result = dm_launch_external(PLUGIN_APPS_DIR "/livetv.rock",
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
                rb->splash(HZ * 2, "Could not prepare Sitekick window");
                state->running_apps &= ~DM_APP_BIT(app);
                return PLUGIN_ERROR;
            }
            result = dm_launch_external(PLUGIN_APPS_DIR "/sitekick.rock",
                                        "-desktop");
            state->running_apps &= ~DM_APP_BIT(app);
            return result;
#if LCD_WIDTH >= 1920
        case DM_APP_NETFLIX:
            /* Netflix is a desktop-only catalogue window. It receives the
             * committed Desktop and reads only Video Sync's videolist. */
            state->app = DM_APP_DESKTOP;
            state->damage_full = true;
            dm_draw(state);
            if (!dm_save_underlay(DM_NETFLIX_UNDERLAY_FILE,
                                  DM_NETFLIX_UNDERLAY_MAGIC))
            {
                rb->splash(HZ * 2, "Could not prepare Netflix window");
                state->running_apps &= ~DM_APP_BIT(app);
                return PLUGIN_ERROR;
            }
            result = dm_launch_external(
                PLUGIN_APPS_DIR "/netflix_desktop.rock", "-desktop");
            state->running_apps &= ~DM_APP_BIT(app);
            return result;
#endif
        case DM_APP_DASHBOARD:
            state->app = DM_APP_DESKTOP;
            result = dm_launch_external(PLUGIN_APPS_DIR "/clock.rock", NULL);
            state->running_apps &= ~DM_APP_BIT(app);
            return result;
        case DM_APP_TRASH:
            rb->splash(HZ, "Trash is empty");
            break;
        default:
            state->app = DM_APP_DESKTOP;
            break;
    }
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
            rb->splash(HZ, "No recent items");
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

static void dm_delete_selected(void)
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
    rb->splash(HZ, result == 0 ? "Item deleted" :
                                      "Could not remove item");
    dm_scan_directory();
}

static void dm_create_new_folder(void)
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
        rb->splash(HZ, "Could not create folder");
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

static int dm_activate_itunes_control(const struct dm_control *control)
{
    int status = rb->audio_status();

    switch (control->action)
    {
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
        case DM_ACTION_ITUNES_SOURCE:
            dm_itunes_source = control->value;
            dm_itunes_page_top = 0;
            dm_itunes_load();
            break;
        default:
            break;
    }
    return PLUGIN_OK;
}

static int dm_click(struct dm_state *state, bool double_click)
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
        dm_delete_selected();
        return PLUGIN_OK;
    }
    if (state->overlay == DM_OVERLAY_NEW_FOLDER_CONFIRM)
    {
        state->overlay = DM_OVERLAY_NONE;
        dm_create_new_folder();
        return PLUGIN_OK;
    }
    if (state->overlay == DM_OVERLAY_GET_INFO)
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
    if (state->overlay == DM_OVERLAY_FINDER_CONTEXT)
        return dm_handle_finder_context(state);
    control = dm_control_at(state, state->cursor_x, state->cursor_y);
    if (!control)
        return PLUGIN_OK;
    switch (control->action)
    {
        case DM_ACTION_APPLE_MENU:
            state->overlay = DM_OVERLAY_APPLE_MENU;
            break;
        case DM_ACTION_DOCK_APP:
            return dm_activate_app(state, dm_dock_apps[control->value]);
        case DM_ACTION_CLOSE:
            if (state->app != DM_APP_FINDER)
                state->running_apps &= ~DM_APP_BIT(state->app);
            state->app = DM_APP_DESKTOP;
            break;
        case DM_ACTION_MINIMISE:
            dm_start_animation(state, state->app, true);
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
                    return dm_open_itunes_selection();
                return dm_open_selected_file();
            }
            break;
        case DM_ACTION_FINDER_SIDEBAR:
            dm_navigate(dm_sidebar_rows[control->value].path);
            break;
        case DM_ACTION_ITUNES_PREVIOUS:
        case DM_ACTION_ITUNES_PLAY_PAUSE:
        case DM_ACTION_ITUNES_NEXT:
        case DM_ACTION_ITUNES_WPS:
        case DM_ACTION_ITUNES_PAGE:
        case DM_ACTION_ITUNES_SOURCE:
            return dm_activate_itunes_control(control);
        case DM_ACTION_PREFERENCE:
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
        default:
            break;
    }
    return PLUGIN_OK;
}

static void dm_secondary_click(struct dm_state *state)
{
    if (state->app == DM_APP_FINDER)
        state->overlay = DM_OVERLAY_FINDER_CONTEXT;
}

static void dm_menu_action(struct dm_state *state)
{
    if (state->overlay != DM_OVERLAY_NONE)
    {
        state->overlay = DM_OVERLAY_NONE;
        return;
    }
    if (state->app == DM_APP_FINDER && rb->strcmp(dm_cwd, "/"))
    {
        char parent[MAX_PATH];

        if (dm_history_move(-1))
            return;
        rb->strlcpy(parent, dm_cwd, sizeof(parent));
        dm_parent_dir(parent);
        dm_navigate(parent);
        return;
    }
    if (state->app != DM_APP_DESKTOP)
    {
        state->app = DM_APP_DESKTOP;
        return;
    }
    state->overlay = DM_OVERLAY_RETURN_CONFIRM;
}

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

enum plugin_status plugin_start(const void *parameter)
{
#ifdef DM_UNSUPPORTED_TARGET
    (void)parameter;
    rb->splash(HZ * 2, "Desktop needs a 320x240 color target");
    return PLUGIN_OK;
#else
    struct dm_state state =
    {
        .app = DM_APP_DESKTOP,
        .overlay = DM_OVERLAY_NONE,
        .cursor_x = LCD_WIDTH / 2,
        .cursor_y = LCD_HEIGHT / 2,
        .hover_dock = -1,
        .hover_row = -1,
        .menu_row = -1,
        .preference_row = -1,
        .last_wheel = -1,
        .running_apps = DM_APP_BIT(DM_APP_FINDER),
        .running = true,
        .redraw = true,
        .damage_full = true,
        .presented_cursor_x = -32,
        .presented_cursor_y = -32,
    };
    int result = PLUGIN_OK;
    enum dm_boot_result boot_result;
    unsigned int saved_foreground = rb->lcd_get_foreground();
    unsigned int saved_background = rb->lcd_get_background();
    int saved_drawmode = rb->lcd_get_drawmode();
    bool saved_backlight = rb->is_backlight_on(true);
    bool session_started = false;
#ifdef HAVE_WHEEL_POSITION
    bool wheel_events_owned = false;
#endif

    (void)parameter;
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
    dm_load_settings();
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
        state.running_apps |= DM_APP_BIT(state.app);
    if (state.app == DM_APP_FINDER)
        dm_scan_directory();
    else if (state.app == DM_APP_ITUNES)
    {
        dm_itunes_page_top = 0;
        dm_itunes_load();
    }
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(false);
    wheel_events_owned = true;
#endif
    while (state.running && result == PLUGIN_OK)
    {
        long button;

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
        dm_update_animation(&state);
        if (state.redraw)
        {
            dm_draw(&state);
            state.redraw = false;
        }
        button = rb->button_get_w_tmo(DM_INPUT_POLL_TICKS);
        if (TIME_AFTER(*rb->current_tick, state.last_status_tick + HZ))
        {
            state.last_status_tick = *rb->current_tick;
            state.redraw = true;
            state.damage_full = true;
        }
        if (rb->button_hold())
        {
            if (state.animation.active)
                dm_finish_animation(&state);
            state.last_wheel = -1;
            state.wheel_velocity = 0;
            state.wheel_direction = 0;
            state.mouse_down = false;
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
        if (dm_poll_host_pointer(&state))
        {
            if (state.animation.active)
                dm_finish_animation(&state);
            state.redraw = true;
        }
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
#endif
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
            if (dm_settings.drag_lock && state.mouse_down)
            {
                state.mouse_down = false;
                state.dragged = false;
                state.redraw = true;
                continue;
            }
            state.mouse_down = true;
            state.dragged = false;
            state.mouse_down_tick = *rb->current_tick;
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
            dm_menu_action(&state);
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
        dm_saved_app = state.app;
        dm_saved_cursor_x = state.cursor_x;
        dm_saved_cursor_y = state.cursor_y;
        dm_save_settings();
    }
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
