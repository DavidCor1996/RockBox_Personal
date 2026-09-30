# Docked ambient clock validation

Implementation scope: iPod 6G core, stock and iPodJS Home, normal Files/Music
browsers, and the idle Desktop Mode shell. No physical deployment performed.

## Reproducible checks

- `python3 tools/generate_ambient_font.py`: rebuilds the const ROM atlases from
  `assets/ambient-clock/AdwaitaSans-Regular.ttf`; license in adjacent OFL.txt.
- `python3 tools/generate_ambient_icons.py`: reproduces all ten modern weather
  icons from pinned Lucide SVGs, using librsvg and Pillow. Sources and ISC license
  are in `assets/ambient-clock/weather`. No hand-drawn icon geometry is used.
- `python3 tools/ambient_clock_gate.py`: compiles the production renderer and
  controller with ASan and UBSan. The gate exercises weather and date validation (including host daylight saving time),
  all ten weather icon renders and day/night condition mapping,
  palette capacity, idle/reset conditions, rendering without file reads,
  100 complete enter/exit cycles, USB, transport, undocking, and externally
  started playback. Output PNGs are 3x enlargements of actual 320x240 renders.
- `python3 tools/ambient_clock_sim_gate.py`: drives the real simulator with
  its headless SDL driver, enters Preview through the settings menus, checks
  rendered weather data, checks optical centering of the date, numerals, AM/PM, weather group and
  location to within two source pixels, and verifies three wake/reentry cycles. File descriptor
  counts remain 5, 5, 5. The live test caught and fixed viewport initialization
  and host DST interpretation issues that the isolated HAL did not expose.
- iPod 6G native firmware and all plugins build successfully with plugin API 288.
- iPod 6G simulator firmware and all plugins build successfully.
- iPod Video simulator firmware builds without the ambient-clock capability.

## Album slideshow checks

`python3 tools/ambient_clock_sim_gate.py --art` builds an isolated synced
catalog from four existing sourced album-cover photographs. Synthetic Halifax
weather is explicitly labeled test data. It checks optical centering, captures
the initial album, a transition and the next album, and compares background
pixels to prove the artwork changes. The final real simulator run passes all
checks, including three wake/reentry cycles with file-descriptor counts
`[5, 5, 5]`. The exit path clears artwork outside the host list viewport and
consumes the wake-button release so Select cannot reopen Preview. The test
publishes every SDL render to avoid sampling an intermediate dirty rectangle.
Covers are test fixtures only. Screenshots are in `docked-ambient-clock-shots`:
`album-lock-screen.png`, `album-crossfade.png`, and `album-next.png`.

The sanitizer gate additionally checks a 1,000-row catalog, selection excluding
the current album, complete transition ownership, database-commit deferral and
zero reads while drawing an actual cached image. It uses a mocked decoder;
the real simulator test covers actual BMP decoding and compositing.

## Resource audit

The native `apps/gui/ambient_clock.o` currently uses 7,366 bytes of BSS and no
writable data section. The code and read-only font/icon data occupy approximately
63 KiB of text/rodata. The BSS stays below the specification's 16 KiB ceiling.
Full-screen album art leases two existing 384px album-list slideshow buffers;
the exclusive lease adds one boolean to that module, not new image storage.
The renderer uses one 320-pixel scanline and a 41x31 RGB grid; no audio allocation
or second framebuffer. Native disassembly now inlines the renderer into the ambient loop: its local
frame is 860 bytes plus 36 bytes of saved registers. The weather loader has a
204-byte local frame plus saved registers. This is a static call-frame audit,
not a measured whole-thread high-water mark.


## Remaining physical checks

TV overscan, interlace shimmer, glass readability, RGB565 dithering, NTSC/PAL
scaling, ≤100ms input latency, dock removal, and the eight-hour soak still need
real hardware. Native/simulator compilation and host HAL tests do not establish
those results. Active playback and Database navigation on the device remain a
required deployment gate.

The standard navigation script's first run was blocked by a pre-existing nested
`.rockbox` tree under simdisk artwork that exceeds the filesystem path limit.
The retry with a preserved isolated fixture was blocked by unavailable X11
window access. The focused ambient gate uses the SDL dummy driver and passes;
the full navigation regression remains unverified in this environment.
The host has no `pdflatex`, `latex`, or `latexmk`; manual PDF rendering is not
available. The menu/config documentation changes were inspected as TeX source.

## Physical installation

Installed on the connected iPod 6G using the database-preserving package
installer. Verified all 11 live database files unchanged and 2,929 indexed
tracks readable. User settings are preserved, with tagcache autoupdate enabled.
The glass-numeral refinement uses the same plugin API and assets, so its
follow-up install updates both firmware copies and build metadata only.
This is installation verification, not physical composite visual validation.

Final installed firmware SHA-256: `878ca349ef8fc6066049cbb0aca89ae3c61895fc98455e528ee3748b7417e34c`.
Both volume-root and `.rockbox` copies match; final disk sync completed.

## Hourly-weather compatibility and design refinement

The ambient loader formerly required a `current` row; the normal RockPod
weather producer supplies daily and hourly rows. The loader now chooses the
current-hour forecast when fresh live data is absent. Sanitizer tests cover
hourly-only sync, moving to the next hour, preferring live observations, and
rejecting future or expired data. Drawing remains free of file reads.
The local-hour boundary triggers an idle cache reload for unattended operation.
`tools/ambient_clock_sim_gate.py --art --hourly` exercises this producer format
with full-screen album photography and the refined glass design. This revision
is not yet installed: the iPod is disconnected.

The final synthetic hourly-only real-simulator run passed weather rendering,
optical centering, album transition and three wake/reentry cycles, with file
descriptor counts [5, 5, 5]. A brief device reconnection confirmed the real
Moncton cache has 384 hourly rows and seven daily rows, with no current row,
and includes a forecast covering the current hour. The iPod disconnected
before its complete cache could be copied into the simulator or the new
firmware could be installed. No device writes were made during that check.

## Latest deployment completed

The hourly-weather fix and refined glass design are now installed. Both
firmware locations match SHA-256
`09fdf78e2ae19d57d07fe3260ef8f0d17503ba2d3d92a97ecb3ef20fdb7f84aa`.
Validated 2,929 indexed tracks before installation, preserved the current
database and settings byte-for-byte, confirmed autoupdate is enabled, and
completed disk sync. The installed Desktop Mode plugin already matches the
latest package, so this revision required only firmware and build metadata.
This supersedes the earlier disconnected/pending-installation notes.
