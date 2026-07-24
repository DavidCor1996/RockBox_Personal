# iPod Hero Verification Record

Date: 2026-07-22

This record distinguishes verified implementation behavior from the physical
iPod qualification that is still required by `docs/ipod-hero-spec.md`.

## Current local build

| Target | Plugin size | SHA-256 |
| --- | ---: | --- |
| iPod Classic 6G/7G | 33,668 bytes | `1b7f12ea027026d5164d8c243676f486a9b238885f3a4f3e19439e4e62b40d88` |
| iPod Video 5G/5.5G | 35,556 bytes | `dd13b3daf500ec182a9de1f389fe0ef8674e8d759d3bbfa779f56909e7e556e9` |
| iPod 6G simulator | 524,480 bytes | `a26c147a6e353b7bfda773ce2ac7e96b238b71efe34e26af6af424760bfc2ed1` |
| iPod Video simulator | 525,392 bytes | `5c8d60098d8d35c8116b195922f4f31f7492337957e90a9811d6375c605c484f` |

Both native builds exclude the `autotest-*`, `sim-test.log`, and
`sim-error.log` simulator strings.

## Native panic diagnosis

The first physical launch trace stopped after `song-selected` and before
`package-loaded`. It therefore excluded playlist creation, codec startup,
seeking, and gameplay audio as causes. The failing path was the nested skin
and chart package loader.

ARM disassembly identified a native stack exhaustion that the simulator could
not reproduce. `plugin_start()` reserved 3,424 bytes for its frame, including
the complete application state. The nested `ih_skin_load()` and image/CRC
loader reserved another 1,364 and 796 bytes before Rockbox's BMP decoder used
its own stack. The SDL simulator runs with a host stack, while the iPod 6G
firmware main thread has a fixed 8 KiB stack.

The application state and single-threaded skin scratch buffers now live in the
plugin BSS. In the rebuilt iPod 6G ELF, the same frames are 660, 36 and 284
bytes respectively, removing approximately 4.6 KiB from the panic path.
Additional trace stages delimit index copy, skin load and chart load if a
subsequent hardware run fails. The corrected plugin was copied and synced to
the mounted iPod; its checksum matches the local native build and all database
checksums remained unchanged. A post-fix physical launch is still required.

## Verified host behavior

- Exact rational tempo-map integration, including tempo changes.
- Clone Hero `.chart` and standard rhythm-game MIDI import.
- Chord merging, sustains, HOPO, tap, forced-note and star-phrase semantics.
- Section marker/string-table serialization and bounded parsing.
- Signed offsets and Rockbox-compatible payload/audio CRCs.
- Exact audio-master identity and atomic `index.tsv` updates.
- Offline generated-chart density limits for Easy, Medium, Hard and Expert.
- Atomic skin preparation, decoded-memory budget and every runtime bitmap CRC.
- No network access in the device runtime or asset preparation tool.

Command:

```sh
tools/ipodhero_host_tests.py
```

## Verified simulator behavior

The no-parameter startup path now loads an in-game song library directly from
`index.tsv`. A simulator-only control-path trace verified one installed song,
selected index zero, and the exact `iPod Hero - Timing Fixture` label before
any song-package draw or gameplay begins.

A separate real-playback gate selected that fixture through the new path,
created normal Rockbox playback for its WAV, validated the decoder path and
identity, sought it to zero, and completed the chart. It recorded 17/17 hits,
17 perfect judgements, no misses, 780 ms of sustain, and no missed frame
deadlines. This is not Practice mode and exercises the new playlist start.

The restored standard fixture is a 60-second WAV with 113 authored events and
chart payload CRC `a053a31f`. Both 5G and 6G simulator roots contain a plugin
byte-for-byte equal to the corresponding current build.

Perfect autoplay on a deterministic 12-second chart:

```text
events=17
hits=17
misses=0
perfect=17
max_streak=17
sustain_ms=720
score=3544
star_activated=1
input_normalizer=pass
```

Mixed autoplay covers every judgement band, wrong input and an early sustain
release:

```text
events=17
hits=13
misses=4
perfect=6
great=3
good=3
grace=1
sustain_ms=550
broken_sustains=1
```

The lifecycle stress mode completed 20 runs, 20 scripted pause/resume cycles,
19 retries, and two additional safe simulator Hold pauses. It produced no
missed frame deadline or state-machine failure.

The host process held exactly nine open file descriptors after every one of
the twenty completed lifecycle runs (`1:9` through `20:9`), proving no
descriptor trend across pause/results/score/retry transitions in this gate.

The stock-style `Settings > Input Test` screen and bounded trace writer were
also exercised in the 6G simulator. Its 13-event fixture records press,
repeat, release, clockwise/counterclockwise wheel, a two-button combination,
the Select+Menu exit chord, and Hold on/off transitions. This verifies the
diagnostic pipeline only; physical button behavior remains a hardware gate.

Visual checkpoint: `build-sim-ipod6g/ipodhero-input-test-gate.png`.

Menus now pass raw navigation and selection events through Rockbox's standard
key-click handler, preserving the global software-click, piezo-click and
haptic settings. Main, song, pause, settings and results screens use a stock
iPod-style title bar, blue selection row, disclosure chevrons, and persistent
`MENU`/`CENTER` action labels.

Gameplay loads two verified live-performance photographs and changes camera
view every seven seconds with a 700 ms segmented wipe derived entirely from
song time. The draw function only blits cached bitmap regions. Persistent
bordered lane cells spell out `LEFT`, `MENU`, `SELECT`, `PLAY`, and `RIGHT`.
The fixed-font gameplay checkpoint is
`build-sim-ipod6g/ipodhero-gameplay-ui-final2.png`.

The current performance evidence from the perfect run is:

```text
frames=585
max_frame_ticks=0
p95_frame_ticks=0
missed_frame_deadlines=0
asset_bytes=626960
chart_bytes=272
plugin_buffer_bytes=3145728
free_headroom=2424288
package_load_ticks=1
max_active_sprites=20
```

The exact-path failure gate reports the unmatched path and host chart command.
A one-byte required-background corruption is rejected before decode as
`Skin asset CRC mismatch / background.bmp`; the valid bitmap was restored and
reverified afterward.

A semantic-preserving one-byte chart change is rejected as
`Chart payload CRC mismatch`. A damaged score header is rejected as
`Score file is corrupt; it was not modified`. The valid chart and score
database checksums were restored after both tests.

Useful visual checkpoints:

- `build-sim-ipod6g/ipodhero-final-autoplay-gate.bmp`
- `build-sim-ipod6g/ipodhero-autoplay-gate.png`
- `build-sim-ipod6g/ipodhero-lifecycle-gate.png`
- `build-sim-ipod6g/ipodhero-foo-fighters-generated.png`
- `build-sim-ipod6g/ipodhero-revised-menu.png`

## Asset evidence

The ignored local package uses pairs of CC BY 2.0 / CC BY-SA 2.0 live
performance photographs and LGPL-3.0-or-later YARG gameplay textures. The
active pairs are Foo Fighters plus a live festival rock band, and the live
festival band plus The Killers. Other prepared selectable concert backgrounds
include Iron Maiden.
`assets/ipodhero/local/PROVENANCE.md` records source pages, authors, licenses,
transformations, pinned YARG commit, and source/output SHA-256 values.

The generated chart package matches this exact existing iPod audio master:

```text
/Music/Foo Fighters/Concrete and Gold/02 - Run.flac
size=69317723
length_ms=323373
identity_crc=8487cbd4
```

Easy, Medium, Hard and Expert generated charts are present and explicitly
labeled generated/needs-human-review.

## Source-safety evidence

- No `audio_stop`, shared audio-buffer acquisition, PCM/mixer ownership,
  `core_alloc`, or private font loading in iPod Hero. Playlist creation and
  one-track insertion occur only after the explicit in-game Play action.
- The gameplay renderer performs no file access, bitmap decode, allocation,
  tag lookup or audio metadata query.
- The full-screen photo wipe is composed directly from the two cached skin
  bitmaps and uses no additional framebuffer or playback/core allocation.
- Skin/chart loading and persistence remain outside the gameplay draw path.
- Imported visual/audio data remains ignored under
  `assets/ipodhero/local/`; no music is copied into the plugin binary.

## Not yet verified

V1 must not be declared complete until physical hardware evidence covers:

- a real-device `input-trace.log` covering raw button, release, repeat,
  rolled-chord, wheel and Hold behavior;
- a complete long-track timing/calibration run;
- MP3, FLAC, AAC/ALAC and long-VBR playback;
- Files and Database entry/exit after an in-game song selection;
- pause/resume/retry, volume, automatic track change and twenty rapid switches;
- authoritative 6G frame/input timing and the 5G 20-fps floor;
- file-descriptor baseline during and after physical playback;
- the newest native plugin checksum and in-game library launch on the mounted
  iPod.

The physical package now contains seven indexed songs with four difficulties
each: the original Foo Fighters track plus Foreigner, Disturbed, Rise Against,
Halestorm, Evanescence, and a second Foo Fighters track. The six additions are
explicitly marked generated and require human timing/musical review.

The native-library cover and current iPod 6G binary above are installed on the
physical iPod. Their post-sync checksums match the local package, all live
database hashes were preserved, and the iPod remained mounted. The playback
handoff now follows PictureFlow's create/insert/sync/start order and writes a
bounded per-launch `runtime.log` so a hardware panic leaves an exact last stage.
The repaired playback path still requires a hardware launch check.
