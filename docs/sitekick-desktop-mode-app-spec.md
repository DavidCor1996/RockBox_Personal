# Sitekick Desktop Mode App

Status: implemented for the `desktop1080` host profile.

## Product shape

Desktop Mode spells the original YTV product name **Sitekick**. Its Dock app
is a desktop interpretation of the original YTV Dock rather than a stretched
copy of the 320x240 plugin:

- an Aqua window opens over the existing Snow Leopard desktop;
- the purple YTV header reports chips, XP and coins;
- the lime grid work area uses the original `MAIN`, `CHIPS`, `SK-TV`,
  `TRADE`, and `HELP` sections;
- `MAIN` exposes the Sitekick, appearance, chip dump, trading, minigames and
  statistics features already available in the native plugin;
- `CHIPS` presents eight equipment slots above the collection.

The structure follows the documented original YTV Dock, whose CHIPS section
had eight equipped-chip positions, storage, and trading areas. Sitekick
Remastered informs the larger desktop spacing and readable collection grid,
while the packaged YTV logo and Sitekick art remain the visual source.

## Mouse contract

At 1920x1080 the host-pointer bridge supplies position and left-button state:

- click a bottom tab to change section;
- press an owned wearable chip in the collection to start a drag;
- the real chip icon follows the pointer as a drag ghost;
- release over one of the eight equipment slots to call the existing native
  equip operation and persist the save;
- drag an equipped chip back to the collection to unequip it.

The click-wheel and keyboard actions remain usable alongside the mouse. A
simulator gate opens the app through its Dock icon, drags starter Chip 0012 to
Equip 1, and checks both the rendered result and saved chip ID.

## Isolation and memory

Desktop behavior is enabled only when `sitekick.rock` receives `-desktop` and
only in builds whose LCD is at least 1920 pixels wide. The ordinary iPod
launch receives no parameter and compiles out the desktop window, pointer,
underlay, and drag code.

The Sitekick window uses `plugin_get_buffer()` for its exact desktop underlay,
Aqua chrome, and cached 2x stage. It never calls `plugin_get_audio_buffer()` or
`core_alloc()`. Existing mixer-channel beeps and all regular-plugin gameplay,
save, reward, trade, collection, and appearance logic are shared without
taking ownership of playback memory.
