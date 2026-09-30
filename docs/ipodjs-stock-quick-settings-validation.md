# Stock-style Quick Settings implementation and validation

Implemented on 2026-09-12 in `apps/root_menu.c`. The implementation preserves
setting adjustment/persistence handlers and parent entry/exit ownership while
sharing list geometry, measured value layout, selected-row rendering and child
transition presentation. Wheel steps repaint affected rows; scrolling repaints
the list; live slider adjustment repaints its bounded region. Theme changes,
Hold and status changes retain complete redraws.

The viewport top is retained independently of selection. Existing endpoint
wrapping is intentional. Volume/Brightness editors and maintenance/accessory
children return to the saved parent selection and viewport. The help footer is
flat, and its height accommodates the cached detail font. Removed unused
mini-meter and gloss-bitmap helpers after replacing inline meters with values.

Concurrent resident-icon work in this tree replaced the old disk-backed icon
cache. That implementation is preserved: every Quick Settings icon paints a
resident glyph, so there is no missing-icon I/O or idle icon service. This
supersedes the specification's old icon-loading approach and does not add a
bitmap workspace. The icon generator/header changes are separate concurrent
work, not authored as part of this layout pass.

## Reproducible focused gate

`tools/ipodjs_quick_settings_sim_regression.sh BUILD_DIR OUTPUT_DIR` reuses the
navigation gate's isolated fixture and screenshot helpers. It exercises:

- list and slider entry/return, immediate slider deltas;
- first/last wrapping, scrolling and retained parent viewport;
- two UI cache refreshes and restart cancellation;
- dark/light appearance, all three font sizes and both densities;
- Hold and ten additional Quick Settings/editor round trips;
- monotonic eight-sample transitions lasting 24-28 ticks at HZ=100.

Set `IPODJS_NAVIGATION_SOURCE_ROOT` to a complete simulator fixture. Optional
`IPODJS_QS_PLAYBACK=1` starts the fixture's tagged music before the same journey
and additionally checks playing state, stable track/playlist identity,
monotonic elapsed time, stable core readings and open-file counts. The optional
playback entry sequence assumes a single tagged artist/album/track fixture.
The script disables notification banners in its temporary configuration for
unobscured captures, without changing the source fixture or player settings.

The final saved playback trace verified 155 Quick Settings list frames and
56 complete transitions. Playback stayed active on `/Music/test.mp3`, playlist
index/count stayed 1/1, and Rockbox open-file count stayed at 12. Core available
and allocatable readings were both zero throughout those playing frames,
consistent with playback owning the remaining arena; this verifies no trend,
not available headroom. The test used SDL dummy audio and cannot prove physical
audibility or codec wake behavior.

Screenshots and trace are under `test-artifacts/quick-settings/`. Inspect
`overview.png` for list, Volume, maintenance, lower rows, large dark text and
restart confirmation. The simulator configuration omits Kokkia; native builds
compile its scrolling/status/child paths, but live accessory behavior requires
hardware.

## Builds and resource audit

Fresh isolated builds passed for ipod6g simulator, native ipod6g and native
ipodvideo. The simulator also exercises compilation without the native
accessory/composite paths. Build directories are `/tmp/qs-sim-build`,
`/tmp/qs-hw6` and `/tmp/qs-hw5`; no device deployment was performed.

| Native image | Text | Data | BSS |
| --- | ---: | ---: | ---: |
| Saved pre-pass ipod6g ELF | 2,878,580 | 10,868 | 8,982,980 |
| Final isolated ipod6g ELF | 2,876,932 | 10,868 | 8,969,540 |
| Final isolated ipodvideo ELF | 2,818,408 | 10,764 | 5,315,076 |

The measured 6G BSS delta is -13,440 bytes. This includes concurrent icon and
other existing work in the dirty personal tree; it is not attributable solely
to layout changes. This pass adds no static cache, framebuffer or core
allocation. The native Quick Settings function's generated ARM frame is 288
bytes including saved registers, versus 232 bytes in the saved pre-pass object.
The extracted row-text helper uses 64 bytes. Most render/child functions are
inlined into the owning function. These are function-frame measurements, not a
claim that the entire main-thread call chain uses only 288 bytes.

Native firmware SHA-256:

- ipod6g: `3ef1cee8e683cfebee78a5327dc10d68a7efa8f10f61396bf88a6d38d75a802d`
- ipodvideo: `dd09dfdb66e58be6aba348189b1ffd199be9f441f2c27337f7dabd69a7dc401f`

Six existing focused icon/cache-memory tests passed with the repository Python
virtual environment. Shell syntax and changed-file whitespace checks passed.
Native builds retain unrelated pre-existing warnings; no new Quick Settings
warnings remain.

## Remaining verification

The unmodified `tools/ipodjs_navigation_sim_regression.sh` was attempted twice
with 10 hierarchy and 20 rapid-switch cycles requested. Both runs stopped at
WPS Hold release before those stress loops: the existing lifecycle-host skin
showed a blank music frame and did not produce the expected Now Playing
return trace. This path was not modified by the Quick Settings pass. The full
navigation gate is therefore not a pass.

An isolated follow-up used the same gate's WPS-Menu-only journey and its
unmodified `run_music_fd_stress` and `run_album_artist_switch_stress` helpers,
without entering the failing WPS Hold path. All ten full-depth hierarchy
cycles and twenty Albums/Artists cycles passed. Process descriptors remained
23 at baseline and after every hierarchy cycle. Playback identity remained
stable, elapsed time remained monotonic, and no `Loading Music` frame was
emitted. The trace and descriptor report are saved beside the Quick Settings
captures. This validates navigation stress independently; it does not convert
the unmodified full gate into a pass.

The initial source simdisk also had recursive nested preview directories and
no Music directory. Tests used a separate synthetic tagged MP3 fixture with
copied assets. The host database tool had an unrelated `__PCTOOL__` logf build
failure; fixture generation used a temporary host-only copy excluding the
LCD panic-dump function. No production logf source was changed.

Physical 6G/5G playback, Files-origin playback, paused-entry behavior, live USB
and Kokkia events, and graceful confirmed reboot remain hardware/manual gates.
Simulator evidence does not authorize describing the implementation as
hardware-validated. Preserve the repository's database and dual-firmware
rules on any subsequently requested deployment.
