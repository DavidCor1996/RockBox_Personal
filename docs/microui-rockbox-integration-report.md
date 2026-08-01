# microUI Rockbox Integration Report

## Scope

The pinned microUI core is available as an optional plugin library on color
targets at least 320x240. `microui_demo.rock` is the qualification consumer.
Desktop Mode does not link or allocate a `mu_Context`: it adopts the bounded
control-registry, stable-ID focus, error, diagnostic, and damage-tracking
patterns while retaining its existing playback-aware model and cached-pixel
compositor.

This separation follows the integration specification's initial-port rule not
to migrate existing plugins. It also prevents decorative Desktop UI work from
taking ownership of playback or shared audio memory.

## Desktop Mode improvements

- A fixed 64-entry control registry is rebuilt from the visible UI. Stable IDs
  carry actions and values, and one dispatch path owns click behavior.
- Hover, selection, focus, and activation are separate. Pointer hover no
  longer mutates Finder or media selection.
- Previous/Next can traverse focusable menu, modal, and Preferences controls;
  Finder and the desktop canvas remain pointer-driven.
- The cached framebuffer compositor clips every paint primitive to a damage
  rectangle. Cursor and hover changes use partial LCD updates, with a bounded
  full-frame fallback for scene changes, Dock magnification, and large unions.
- The Apple menu exposes runtime diagnostics for plugin-arena use, registry
  high-water, frame time, update counts, and the last bounded error.
- Startup failures identify missing/invalid assets or insufficient plugin
  memory. All exits share one cleanup path and restore wheel events, input,
  viewport, LCD colors/draw mode/font baseline, and backlight state.

## microUI compatibility work

- Official upstream pin: `0850aba860959c3e75fb3e97120ca92957f9d057`.
- 48 KiB command arena; bounded roots, pools, stacks, widths, and 64-entry
  focus registry.
- Signed 32-bit integer controls with checked parse/format helpers; no float,
  `strtod`, stdio, dynamic allocation, SDL renderer, or libm dependency.
- Command storage and records are pointer-aligned. Overflow returns an
  emergency command only for fixed-size callers and stops variable-size text
  writes before they touch it.
- The Rockbox adapter renders clip, rect, text, and vector-icon commands
  directly, tracks dirty pixels, translates click-wheel focus, opens the
  Rockbox keyboard, clears pressed state on Hold/USB, and restores display
  state on shutdown.
- The demo is event-driven while idle and covers movable/scrollable windows,
  nested clipping, wrapped text, buttons, checkbox, integer slider, text box,
  tree nodes, popup, modal, a dense stress page, and diagnostics.

## Measured baseline

The 6G hardware ELF reports `mu_Context` at 53,520 bytes and adapter state at
2,184 bytes (55,704 bytes combined), comfortably below the 96 KiB release
ceiling.

| Hardware ELF | text | data | BSS | total |
| --- | ---: | ---: | ---: | ---: |
| 6G microUI demo | 21,438 | 132 | 55,808 | 77,378 |
| 6G Desktop Mode | 38,658 | 304 | 53,816 | 92,778 |
| 5G microUI demo | 21,966 | 132 | 55,808 | 77,906 |
| 5G Desktop Mode | 39,858 | 304 | 53,816 | 93,978 |

These values should be refreshed from the build maps whenever either profile
changes.

Host tests run both normally and with address/undefined-behavior sanitizers.
The simulator gate captures Widgets, Stress, and Diagnostics pages and verifies
wheel focus plus stable-ID activation. Explicit Desktop Mode and microUI demo
plugin builds cover iPod 6G and Video 5G hardware/simulator targets; Desktop
Mode also builds for the 1920x1080 simulator.

## Outstanding hardware evidence

Physical-device qualification is still required for average/p95/worst render
time, stack high-water, music-playing responsiveness, playlist/playback
continuity, Hold behavior, and USB transitions. No firmware deploy was
performed as part of this implementation.
