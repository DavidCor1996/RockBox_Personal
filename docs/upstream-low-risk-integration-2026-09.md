# Upstream low-risk integration, September 2026

## Scope

Bring the high- and medium-priority, low-risk fixes from the September 12
upstream audit into the personal iPod 5G/6G tree. The audited upstream tip is
`57a91121f6a1a9d86931d41a2c52386e0a416d78`. This is a selective source port,
not an upstream merge: this repository has no common Git ancestor with that
upstream history. Preserve existing local modifications and custom behavior.

## Selected changes and acceptance criteria

Commit links use `https://github.com/Rockbox/rockbox/commit/<commit>`.

| Concern | Upstream commit(s) | Required behavior |
| --- | --- | --- |
| iAP stack | `104f57252b` | Increase the accessory thread from six to eight default stack units, protecting metadata queries from overflowing into the receive buffer. |
| FLAC seek recovery | `958e84c042` | Use an offset seek only when the offset is nonzero and the seek succeeds; otherwise use the saved elapsed time, including time zero. |
| Plugin return loop | `0a1858b9d1` | Break repeated WPS/plugin dispatch by returning to the plugin browser when the previous global screen was WPS. Apply the same rule to this fork's direct-path plugin dispatcher. |
| Empty playlist skins | `3af4e20792`, `db87622e7d` | Return no percentage value for an empty playlist; initialize progressbar values and normalize zero lengths before drawing. Retain this fork's existing token set. |
| Paused-track crossfade | `943b73851e` | Clear PCM after halting decoding when restarting or selecting a track from a paused state. Preserve ordinary playing-to-playing crossfade. |
| FFT scheduling | `290b06c869` | Wait for the next frame deadline through the action API; retain explicit yield when already late. |
| Glyph-cache stack | `dfe965d81e`, `513365713a` | Move temporary pathname storage into a non-inlined opener. Include the hosted-bootloader helper required by this change. Preserve glyph-cache naming and file modes. |
| File shortcuts | `0836ebbd45` | Run explicit file shortcuts using SHOW_ALL and BROWSE_DIRFILTER; browse shortcuts retain the normal browser filter. |
| Warble | `7fef95dc08`, `5d016a2c04` | Build decoder-only Warble for recording-capable targets; provide encoder API stubs that fail explicitly. Exclude native iPod Video linker symbols from PC tools. |

The source ports may use local line wrapping and remove upstream trailing
whitespace. The skin fix omits upstream context for the time-based playlist
token, which is not present here. The plugin fix also covers the equivalent
local direct-path dispatcher; it does not add playlist mutations or playback
startup calls.

## Memory and lifecycle constraints

- The iAP stack grows by `2 * DEFAULT_STACK_SIZE`: 2,048 bytes on native ARM
  iPod builds. This is fixed thread storage, not an optional UI allocation.
- No new artwork caches, framebuffers, dynamic core allocations, or plugin API
  entries are introduced. Glyph-cache pathname storage moves out of the caller
  stack frame; it does not increase the glyph-cache allocation.
- The playback fix uses the existing `pcmbuf_play_stop()` after
  `halt_decoding_track(true)`. That helper stops the playback mixer channel
  before resetting PCM state. Existing codec clock/wake and shared-buffer
  ownership paths remain authoritative.
- FFT changes only the input wait timeout. Plugin audio-buffer ownership,
  callbacks, playlist identity, and teardown remain unchanged.
- These are correctness fixes, not iPodJS navigation or artwork changes.

## Deferred work

The DesignWare ISO scheduling fix `b616047311` remains high priority but is
not low integration risk: this fork also has custom ISO interrupt recovery and
USB audio behavior. It requires a coherent target-driver port and real-device
USB speed/audio tests. The quickscreen series and Mikey inline remote support
also need broader UI or hardware integration and are excluded.

Optional reproducible packaging and cuesheet accessibility work were not in
the ranked high/medium batch. They remain separate follow-ups. Do not import
composite-output or libm4a features already implemented locally.

## Validation plan

1. Preserve every touched file before editing; audit the task-only diff against
   those copies, rather than attributing existing personal changes to this port.
2. Build a baseline native 6G firmware, then build the modified 6G and 5G
   firmware, FLAC codec, and FFT plugin in separate output directories.
3. Build the 6G simulator and modified plugin/codec. Build Warble for both iPod
   targets and exercise FLAC start, offset recovery, and time-seek behavior.
4. Use focused host regression checks for empty-playlist skin arithmetic,
   plugin return dispatch, and paused/restart versus active crossfade paths.
   Check the pre-port sources fail the relevant regressions.
5. Inspect native ELF text/data/BSS and iAP thread-stack symbol sizes; inspect
   glyph-cache function stack usage. Require a clean task-only whitespace diff.
6. Record actual results and any unrelated build blockers below. Compilation
   does not constitute device validation.

## Device acceptance before deployment

Check iAP track metadata requests; FLAC start/resume; pause then select another
track with crossfade enabled; empty-playlist theme refresh; plugin/WPS return
with and without resumable music; filtered file shortcuts; and FFT interaction
while music plays. Exercise Database and Files music, plugin entry/exit,
pause/resume, volume, and rapid switching as applicable. Firmware deployment
and upstream publication are outside this task.

## Results

Implemented all nine selected concerns across 13 existing source files. The
ten directly applicable commits were ported as patches, with the two skin
commits adapted together. The local direct-path dispatcher receives the same
return-loop guard as the upstream dispatcher. The initial integration did not
deploy firmware. No commit or upload was made; the subsequently requested
device deployment is recorded below.

Validation completed on September 12, 2026:

- Baseline native 6G firmware built successfully before source edits.
- Native 6G and 5G firmware, FLAC codec, and FFT plugin built successfully in
  fresh output directories. On the initial selective plugin build, the native
  linker script had not been generated; explicitly generating `plugin.link`
  and `codec.link` resolved this without a source change. Artifact validation
  also caught an incorrect FFT target path. The correct target is
  `apps/plugins/fft/fft.rock`; it was subsequently built and checked for nonzero
  output and the native `plugin_start` symbol on both iPods. The spurious
  artifacts from the incorrect target were removed.
- The 6G simulator, FLAC codec, and FFT plugin built successfully. This is
  compile/link evidence, not an interactive simulator navigation test.
- Complete decoder-only Warble builds succeeded for `ipod6g` and `ipodvideo`.
- Twelve Warble FLAC runs passed: two targets, 44.1/48 kHz input, and normal
  start / invalid saved offset / seek to one second. Invalid-offset output PCM
  matched normal-start output byte for byte. Three-second input and one-second
  seek produced the expected durations; 48 kHz input is resampled by Warble to
  44.1 kHz, with one or two rounding samples.
- Six host regression tests passed with undefined-behavior sanitization. They
  execute extracted production C paths for FLAC seek recovery, both plugin
  dispatchers, paused/restart/playing PCM transitions, percentage tokens,
  progressbars, and FFT waits. All six fail on the preserved pre-port sources
  (seven assertion/sanitizer failures because both plugin dispatchers fail).
  These isolate control flow and arithmetic; they do not emulate DMA or device
  audio timing.
- Task-only patch reverse/applicability and whitespace checks passed. The
  whole working tree still has pre-existing whitespace diagnostics; unrelated
  changes were not reformatted. No new 6G compiler warning diagnostics were
  introduced relative to the baseline. Builds are not warning-free.

Native 6G ELF comparison, in bytes:

| Section | Before | After | Delta |
| --- | ---: | ---: | ---: |
| text | 2,945,452 | 2,945,488 | +36 |
| data | 11,032 | 11,032 | 0 |
| BSS | 9,024,516 | 9,026,564 | +2,048 |

The iAP `thread_stack` symbol grows from `0x1800` to `0x2000` bytes, as
specified. Native `glyph_cache_load` reserves 2,060 bytes of local stack after
the port versus 2,320 before (excluding saved registers). The new non-inlined
opener reserves 284 bytes while opening the file; that temporary frame returns
before the subsequent glyph-cache processing. This is not a claim that every
point in the full call chain has a lower peak stack usage.

Re-run the host regressions with:

```sh
python3 tools/tests/test_upstream_lowrisk.py -v
```

For a configured native output directory, the selective build commands are:

```sh
make "$PWD/apps/plugins/plugin.link" "$PWD/lib/rbcodec/codecs/codec.link"
make -j4 bin "$PWD/lib/rbcodec/codecs/flac.codec" \
    "$PWD/apps/plugins/fft/fft.rock"
```

Configure Warble with `tools/configure --target=ipodvideo --type=w` (or
`ipod6g`) in its own output directory, then run `make -j4`.

Local build outputs and evidence are in
`build-upstream-lowrisk-20260912/`: `before/` holds the exact pre-port files;
`source.patch` and `source-checksums.json` isolate this batch; build logs,
`regression.log`, `negative-controls.log`, `flac-results.json`,
`flac-verification.log`, `artifacts.json`, and `memory-review.log` record
validation. The
negative-controls run intentionally returns failure. Device acceptance above
remains pending before deployment.

## Requested device deployment

After the user requested deployment, the mounted iPod 6G at
`/run/media/david/DAVID_S IPO` was updated. Both installed firmware copies
matched the exact pre-port baseline, and the FLAC/FFT headers were compatible.
The targeted deployment updated `/rockbox.ipod`, `/.rockbox/rockbox.ipod`,
`/.rockbox/codecs/flac.codec`, `/.rockbox/rocks/demos/fft.rock`, and build
information. This was not a full package deployment.

Both firmware copies match local SHA-256
`930229f46f4b46ea06702727fa197702d6d3385759152339c181e3fff9e6a2d9`.
All five installed files passed checksums before and after `sync`. All 11
database files and `config.cfg` remained byte-identical, autoupdate remained
enabled, and all 2,929 database tracks resolved to existing files. Previous
files and database/config backups are retained under
`build-upstream-lowrisk-20260912/device-backup-20260913T000927Z/`;
`deployment-result.json` records the checksums. The device was left mounted.
Reboot and physical playback/accessory acceptance checks have not been run.
