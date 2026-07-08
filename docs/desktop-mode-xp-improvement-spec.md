# Desktop Mode XP Improvement Spec

## Goal

Desktop Mode should present a Windows XP style shell on the 320x240 iPod
simulator and target LCD: Bliss-like desktop, Luna-blue taskbar, green Start
button, two-column Start menu, Explorer-style file browser, Control
Panel-style settings, and a visible pointer.

The built firmware must stay GPL-friendly. Exact Microsoft assets are not
committed to this tree. A personal asset pack can be placed beside the plugin
data for private simulator/device use:

```
.rockbox/rocks.data/desktop_mode_xp/
```

On native/plugin-dir installs where `PLUGIN_APPS_DATA_DIR` resolves to the apps
plugin directory, use the equivalent:

```
.rockbox/rocks/apps/desktop_mode_xp/
```

## Reference Notes

- Windows XP's default visual style is Luna, with a colorful plastic look,
  rounded title bars, a green Start button, and red close buttons.
- Windows XP's default wallpaper, Bliss, is a Charles O'Rear photograph whose
  rights were obtained by Microsoft before Windows XP shipped.
- Windows XP's Start menu is a two-column launcher with pinned/frequent apps,
  documents/settings locations, all-programs access, and shutdown actions.

Reference URLs:

- `https://en.wikipedia.org/wiki/Windows_XP_visual_styles`
- `https://en.wikipedia.org/wiki/Bliss_(photograph)`
- `https://en.wikipedia.org/wiki/Features_new_to_Windows_XP`

Because those assets are proprietary, exact wallpaper, icons, cursors, sounds,
and Luna bitmaps should remain a user-provided overlay. The checked-in plugin
draws a close procedural fallback instead.

## Implemented Shell Surface

- Procedural Bliss-like wallpaper with sky, clouds, and green hills.
- Luna-like taskbar with Start button, active task buttons, and clock tray.
- Two-column Start menu:
  - left column: app/plugin launchers;
  - right column: Explorer, Settings, Refresh, Exit;
  - footer showing the personal asset-pack location.
- Desktop icons for file browser, Notepad, Calendar, Calculator, Photos,
  Game Boy, PokeMini, Plugins, and System Info.
- Explorer window for filesystem browsing and file/viewer launching.
- Settings dialog for Now Playing text and default start view.
- Software cursor rendered over every view.

## Mouse Model

The iPod click wheel is mapped as a pointer device inside the plugin:

- wheel back/forward: move cursor up/down;
- left/right: move cursor left/right;
- select release: click;
- select hold: open Start menu;
- menu: close Start menu, go back, or exit desktop;
- play hold: exit desktop.

Hover updates desktop icon, file row, settings row, and Start menu selection.
Wheel movement also advances selection so the UI remains usable when the cursor
is not perfectly positioned.

## Asset Overlay Plan

Optional loader work should look for these files in `desktop_mode_xp/`,
without making them required for build or simulator tests:

- `bliss.320x212.bmp`: desktop area excluding the 28px taskbar
  (implemented);
- `icons.32x32x16.bmp`: app icon strip in desktop order;
- `cursor.16x24x16.bmp`: arrow cursor with transparent key color;
- `luna_taskbar.320x28x16.bmp`: exact taskbar bitmap;
- `start_button.66x22x16.bmp`: exact Start button states.

Asset-loader acceptance:

- missing asset pack falls back to procedural rendering with no warning dialog;
- bad dimensions are ignored per-asset;
- no asset decode uses the shared audio buffer;
- simulator screenshots show the same layout with or without the overlay.

## Simulator Gate

Minimum automated gate:

```
make -C build-sim-ipod6g rocks
make -C build-sim-video-5g rocks
ROCKBOX_SIM_PLUGIN=/.rockbox/rocks/apps/desktop_mode.rock \
    timeout 5s build-sim-ipod6g/rockboxui --nobackground
```

Manual simulator checklist:

- Desktop opens directly with wallpaper, taskbar, Start button, and pointer.
- Select-hold opens Start menu; menu closes it.
- Click My Files or Start > My Files opens Explorer.
- Explorer row click opens folders or viewers; menu goes parent/back.
- Start > Control Panel opens settings; toggles save and return.
- Start > Shut Down exits Desktop Mode cleanly.
