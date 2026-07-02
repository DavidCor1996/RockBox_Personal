# MuJS Rockbox Scripting Plugin Spec

## Goal

Embed a small JavaScript runtime as a controlled Rockbox plugin so simple tools
can be added without rebuilding firmware. The first version should run local
scripts from a sandbox, expose a small Rockbox UI/file API, and target music
utilities, calculators, flashcards, and metadata helpers rather than arbitrary
web JavaScript.

## Runtime Choice

Use MuJS first because it is small, C-based, and easier to sandbox than a larger
modern ECMAScript runtime. QuickJS can be evaluated later if ES2020 features,
modules, or bytecode become important enough to justify the size and complexity.

## First Slice

- Add `mujs.rock` as an application plugin.
- Embed MuJS under a plugin-local compatibility layer.
- Load scripts from:

```text
/.rockbox/scripts/
```

- Show a script browser listing `.js` files.
- Execute one script at a time.
- Provide a minimal host API named `rb`.
- Enforce memory, operation-count, and wall-clock limits.
- Store script data only under:

```text
/.rockbox/scripts/data/
```

The first slice does not need background execution, networking, modules, or
integration with WPS tags.

## Host API

The `rb` object should be deliberately small:

```javascript
rb.print(text)
rb.clear()
rb.alert(title, text)
rb.menu(title, items)
rb.input(title, initial)
rb.getButton(timeout_ms)
rb.ticks()
rb.readText(path)
rb.writeText(path, text)
rb.listFiles(path, suffix)
rb.nowPlaying()
rb.playbackState()
rb.random(max)
rb.quit()
```

Rules:

- `path` arguments are relative to `/.rockbox/scripts/` unless explicitly
  documented otherwise.
- Writes outside `/.rockbox/scripts/data/` are rejected.
- `nowPlaying()` returns a plain object with available metadata fields such as
  artist, album, title, genre, year, track number, elapsed, duration, and file
  path.
- `playbackState()` is read-only in the first slice.
- Strings returned to JavaScript are copied and bounded.

## Script Manifest

Each script may have an optional sidecar manifest:

```text
/.rockbox/scripts/bpm_tapper.js
/.rockbox/scripts/bpm_tapper.json
```

Manifest fields:

- `title`: display name;
- `category`: `music`, `utility`, `learning`, `metadata`, `theme`, or `game`;
- `description`: short text for RockPod and the script browser;
- `permissions`: requested capabilities, initially `metadata`, `read_data`,
  `write_data`, and `file_list`;
- `entry`: optional function name, defaulting to top-level script execution.

If the manifest is missing, the script is shown by filename and receives only
basic UI and data-folder access.

## Example Scripts

Target scripts for the first useful bundle:

- BPM tapper;
- chord and scale finder;
- random album picker;
- playlist rule tester reading exported playlist text;
- flashcard review using TSV decks;
- date/time calculator;
- unit converter;
- LRC timing offset helper;
- theme asset manifest checker;
- now-playing metadata dumper.

These scripts should be small enough to serve as API examples and regression
fixtures.

## UI Model

The plugin owns the screen while the script runs. Scripts do not draw arbitrary
pixels in the first slice. Instead, they compose UI through host primitives:

- text output page;
- menu picker;
- alert dialog;
- text input;
- button polling.

This keeps scripts simple, avoids layout bugs, and lets the host adapt to
320x240 and other Rockbox targets.

## Sandbox

The sandbox must protect the player from bad scripts:

- fixed maximum heap for the MuJS context;
- instruction/operation budget checked from the interpreter hook or a periodic
  host callback;
- maximum script file size;
- maximum string size crossing the host boundary;
- maximum output buffer size;
- maximum call depth if MuJS exposes a safe guard;
- no dynamic native loading;
- no network APIs;
- no raw filesystem APIs;
- no playback mutation APIs in the first slice.

On timeout or memory failure, the plugin shows a concise error and returns to the
script browser without rebooting or corrupting script data.

## Music Utility Path

The first Rockbox-specific win is a music utility bundle:

- `bpm_tapper.js`: tap select/play to estimate BPM, save recent tempos.
- `scale_finder.js`: choose root and mode, show notes and common chords.
- `random_album.js`: read a host-generated album list and pick one with filters.
- `metadata_check.js`: show current track metadata and missing fields.
- `playlist_rules.js`: test simple filter expressions against exported playlist
  or database text.

RockPod can generate supporting text files from the user's music library:

```text
/.rockbox/scripts/data/albums.tsv
/.rockbox/scripts/data/tracks.tsv
/.rockbox/scripts/data/genres.tsv
```

## RockPod Integration

RockPod should manage scripts as installable bundles:

- install `.js` and `.json` files to `/.rockbox/scripts/`;
- install bundle data to `/.rockbox/scripts/data/`;
- validate manifests before deployment;
- mark scripts as safe, experimental, or local-user-provided;
- optionally generate music-library TSV exports for metadata scripts.

RockPod should not silently install scripts with broader permissions than the
plugin supports.

## Testing

No device deployment should happen until the simulator and hardware gates below
pass. Scripting is higher risk than a fixed utility plugin, so the release gate
must prove bad scripts fail closed and cannot cause playback, filesystem, or UI
damage.

### Simulator Gate

- Build and run on the iPod 6G simulator and iPod Video simulator.
- Run fixture scripts for:
  - hello world;
  - menu/input/alert flow;
  - file read/write under `scripts/data/`;
  - now-playing metadata read;
  - script exception with filename/line reporting;
  - infinite loop;
  - deep recursion;
  - huge string creation;
  - oversized script file;
  - repeated launch/exit cycles.
- Verify writes outside `/.rockbox/scripts/data/` are rejected, including paths
  using `..`, absolute paths, duplicate slashes, and mixed path separators.
- Verify malformed manifests are ignored or reported without blocking the script
  browser.
- Verify large scripts, large strings, and recursive scripts fail gracefully.
- Verify no playback state changes after running scripts.
- Run repeated script execution under AddressSanitizer where the simulator build
  supports it.
- Confirm `git diff` shows no filesystem changes outside expected
  `/.rockbox/scripts/data/` test outputs.

### Sandbox Gate

- Infinite loops must terminate through the operation or wall-clock budget and
  return to the script browser.
- Heap exhaustion must return a controlled error without corrupting script data.
- Host API calls must validate all argument types and sizes before use.
- Host API strings returned to JS must have maximum lengths.
- Output buffering must be capped and scrollable.
- `rb.quit()` must unwind cleanly even from nested host API calls.
- A failing script must not poison the next script run; the next script must get
  a fresh context.
- Scripts must not be able to access arbitrary C pointers, native symbols,
  firmware internals, or unrestricted filesystem paths.

### Performance Gate

- Script browser open time should be bounded by the number of scripts, not by
  scanning script data directories recursively.
- A normal utility script should start in less than one visible UI interaction
  beat on simulator targets.
- Long-running scripts must yield to input checks often enough that menu/back or
  timeout handling remains responsive.
- Repeated script runs must not increase plugin heap usage after returning to the
  browser.
- Host file reads must enforce a maximum file size for first-slice scripts.
- Metadata reads must be snapshots; scripts must not hold pointers into playback
  structures.

### Playback And System Gate

- Start Database playback, open `mujs.rock`, run each fixture script, exit, and
  verify playback resumes/continues according to normal plugin behavior.
- Repeat from Files playback.
- Verify the plugin does not call `audio_stop()`, `plugin_get_audio_buffer()`,
  `plugin_release_audio_buffer()`, PCM APIs, mixer APIs, playlist mutation APIs,
  or tagcache write APIs in the first slice.
- Verify a malicious or broken script cannot change volume, stop playback,
  mutate playlists, delete files, or write outside `scripts/data/`.
- Verify volume changes, hold switch behavior, and menu exit still work after
  repeated plugin launches.
- Verify the database and file browser still open after plugin exit without a
  reboot.

### Hardware Gate

- Test on iPod Classic 6G/7G and iPod Video 5G/5.5G before marking the feature
  stable.
- Run the full fixture script pack on hardware.
- Run at least 100 script launch/exit cycles across multiple scripts.
- Test while music is playing and while no music is playing.
- Confirm no audible glitch, lockup, delayed button response, battery-draining
  busy loop, or persistent slowdown after plugin exit.
- Power-cycle after scripts write data and verify the script browser and data
  files still load.

### Release Blockers

- Any crash, reboot, database playback failure after exit, uncontrolled infinite
  loop, write outside `scripts/data/`, heap growth across runs, or script-driven
  playback mutation is a release blocker.
- Any first-slice code path touching PCM, mixer, playlist mutation, or shared
  audio buffer ownership is a release blocker.

## Later Slices

- Structured drawing API for simple charts and gauges.
- Read-only access to selected playlist/database exports.
- Scriptable flashcard scheduler with a host-provided date store.
- Script packaging format for RockPod.
- Optional bytecode/precompile cache if runtime startup becomes slow.
- Evaluate QuickJS for a second runtime only if real scripts need newer language
  features.

## Non-Goals

- No network access.
- No background scripts.
- No direct access to arbitrary firmware internals.
- No arbitrary filesystem writes.
- No PCM, mixer, playlist mutation, or shared audio buffer changes in the first
  slice.
- No attempt to run browser apps or npm packages.
