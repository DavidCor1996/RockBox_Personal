# ScummVM Rockbox Port Specification

This document defines the first supported ScummVM shape for this iPod-focused
Rockbox tree. It is intentionally narrower than upstream ScummVM: the first
goal is a reliable plugin backend for 320x240 iPods, not a full desktop
launcher.

## Goals

- Launch games from small `.scummvm` descriptor files in the file browser.
- Keep all game data outside the firmware tree and outside the plugin binary.
- Preserve user music and playlist state unless the ScummVM backend actually
  needs primary game audio.
- Support an incremental engine allowlist, starting with low-resolution 2D
  engines that fit 320x240 and clickwheel controls.
- Keep the upstream ScummVM import isolated under `apps/plugins/scummvm/` so
  local Rockbox backend work is reviewable.

## Non-Goals

- Do not import every upstream engine at once.
- Do not provide a desktop-style ScummVM GUI launcher in the first slice.
- Do not emulate DOS, Windows, or PC hardware.
- Do not mutate playlists or use the Rockbox audio engine as a music launcher.

## Launch Descriptor

Descriptor files use the `.scummvm` extension and simple `key=value` lines:

```text
gameid=monkey
engine=scumm
path=/Games/ScummVM/monkey
savepath=/.rockbox/scummvm/saves/monkey
```

Supported keys in the first slice:

- `gameid`: ScummVM target id or a local id for saves/config.
- `engine`: engine family name, for example `scumm`, `sky`, or `queen`.
- `path`: game data directory.
- `savepath`: optional save directory. If omitted, the plugin uses
  `/.rockbox/scummvm/saves`.

Blank lines and lines beginning with `#` are ignored. Unknown keys are ignored
so descriptors can grow without breaking older builds.

Engine data shared by ScummVM engines belongs under
`/.rockbox/scummvm/engine-data`. The Sky engine requires `sky.cpt`; the loader
also accepts it in the game data directory for compatibility with upstream
ScummVM layouts. Without `sky.cpt`, the current Rockbox bridge can still render
the intro resources from `sky.dsk`, but the section bootstrap and full game loop
cannot execute.

## Backend Plan

The current plugin entry point parses and validates descriptors, then hands the
result to `scummvm_backend_run()`. That function owns the Rockbox-facing event
loop, input mapping, framebuffer presentation, and the narrow C/C++ bridge into
the imported ScummVM 1.9.0 sources.

Expected backend responsibilities:

- Provide filesystem, timer, input, graphics, save, and audio implementations
  using the Rockbox plugin API.
- Prefer 320x200 and 320x240 engines first; scale smaller modes with integer
  or letterboxed scaling.
- Use a fixed input map with an in-game menu for verb/action modifiers.
- Store saves under `/.rockbox/scummvm/saves`.
- Avoid dynamic plugin loading from upstream ScummVM.

## Candidate Engines

Initial candidates should be small and 2D:

- `sky` for Beneath a Steel Sky.
- `queen` for Flight of the Amazon Queen.
- early `scumm` targets at 320x200.

Avoid engines with high-resolution, 3D, heavy video, or large scripting/runtime
dependencies until the backend is stable.

## Current Engine Bridge

The active import is ScummVM 1.9.0, kept under
`apps/plugins/scummvm/upstream-1.9.0/`. This vintage is GPLv2-compatible with
Rockbox and builds without requiring a C++11 runtime.

Implemented bridge pieces:

- `sky`: validates `sky.dnr`/`sky.dsk`, loads real resource entries, and uses
  the imported `Sky::RncDecoder` to validate and unpack RNC-compressed
  resources. It also loads and renders the first upstream static intro screens
  through the Rockbox framebuffer: `60110`/`60111`, `60112`/`60113`, and
  `60114`/`60115`. The bridge also has a minimal Sky sequence player for the
  skip/run encoded intro animation resources and starts the floppy intro
  sequence files `60082` through `60086` after the static screens. After the
  intro resource path completes, the bridge validates the section-0 bootstrap
  resources used before the first playable scene: the fixed item resources and
  all 70 grid files (`60000` through `60069`). It also parses and validates
  `sky.cpt`, including the compact data-list table, source/ascii sections,
  dlinc aliases, diff block, and save-id table. The native runtime layer now
  initializes the core Sky script variables, enters section 0, and scans the
  active logic list from the retained compact table each frame. It also loads
  Sky script modules from `sky.dsk` and decodes the active compact's script
  stream until the first engine mcode call, reporting the real mcode and
  arguments that need native implementation next.
- `queen`: validates `queen.1`/`queen.1c`, recognizes known retail/demo file
  sizes, and recognizes rebuilt `QTBL` resource headers.
- Both engines still render through the Rockbox framebuffer shell; full engine
  script, scene, and input execution remains the next major step.

## Audio Lifecycle

If the backend has no audio, it must not touch playback. If game audio is
enabled:

- call `plugin_get_audio_buffer()` only when the backend needs the shared audio
  buffer, and do not call `audio_stop()` before it;
- use `PCM_MIXER_CHAN_PLAYBACK` for primary game audio;
- stop the mixer channel and clear callbacks before releasing plugin-owned
  memory;
- restore sample rate and mixer state on exit;
- never modify or replace the user's active playlist.

These rules are mandatory for this tree and come from
`docs/plugin-audio-lifecycle-steering.md`.

## Implementation Phases

1. Descriptor launcher and build/file-browser integration. Done.
2. Rockbox backend shell with graphics, input, save paths, and no audio. Done.
3. Engine allowlist and data probing for `sky` and `queen`. Done.
4. C++ bridge for importing ScummVM 1.9.0 engine classes. Done.
5. Imported Sky decompressor compiled and exercised through the Rockbox Sky
   resource loader. Done.
6. Queen resource-file validation through the bridge. Done.
7. Sky startup screen rendering from real game resources. Done.
8. Timed Sky static intro screen sequence from real game resources. Done.
9. Minimal Sky intro sequence player for skip/run animation resources. Done.
10. Sky section-0 fixed item and grid resource bootstrap. Done.
11. Sky engine-data probing for `sky.cpt`. Done.
12. Sky compact-table loader and bootstrap validation. Done.
13. Native Sky runtime state and active logic-list scanning. Done.
14. Sky script-module loading and first-mcode decoder. Done.
15. Full Sky or Queen engine loop: scripts, scene drawing, save/load, and
    input.
16. PCM mixer-backed audio using the required lifecycle.
17. Engine allowlist expansion and per-game input profiles.

## Test Matrix

For every phase:

- launch a valid `.scummvm` descriptor from Files;
- launch with missing descriptor keys;
- launch with a missing game data directory;
- exit back to the file browser;
- Database music -> ScummVM launcher -> Database music still works;
- Files music -> ScummVM launcher -> Files music still works.

For audio phases, also run the full matrix in
`docs/plugin-audio-lifecycle-steering.md`.
