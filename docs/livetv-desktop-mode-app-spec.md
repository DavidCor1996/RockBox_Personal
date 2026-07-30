# Live TV in Desktop Mode — Specification

Status: implemented 2026-07-28. Builds on
`docs/livetv-directv-guide-spec.md` (schedule model, guide rendering, audio
lifecycle) and `docs/desktop-mode-snow-leopard-spec.md` (Mac shell, window
model, authenticity rule, Rockpod parity session). This document does not
restate either; it specifies only what changes to make Live TV appear as a
Snow Leopard application inside Desktop Mode.

Primary targets: iPod Classic 6G/7G and iPod Video 5G/5.5G, both 320x240.
Host target: the 1920x1080 Desktop Mode runtime on Linux, macOS, and Windows.

Implementation note: later milestone language in this original design is
superseded by the shipped behavior below. DIRECTV has an eighth Dock slot,
opens on the interactive guide inside its Aqua window, keeps the Desktop and
Dock visible, and uses the entire 1080p window content rectangle after SELECT.
MENU returns from playback to the guide; MENU from the guide closes the app.
The standalone iPod Live TV entry retains its original full-screen behavior.

## Product Goal

Live TV becomes an application launchable from inside Desktop Mode, appearing
identically on the physical iPod and in the Rockpod parity session, because
both run the same `desktop_mode.rock` and `livetv.rock` binaries against the
same verified pack — there is no separate Rockpod implementation to build.

Its default window is the real interactive DIRECTV guide. The currently tuned
channel continues decoding in the picture-in-guide box. SELECT tunes the
highlighted live programme; at 1080p the decoded channel fills the complete
window content area beneath the Aqua title bar. MENU returns to the guide,
then closes the guide back to the unchanged Desktop on the next MENU press.

## Why This Is Not A Desktop Mode Compositor Feature

Desktop Mode's compositor assembles one 320x240 plane from cached pixels and
commits it with a single `lcd_bitmap()` / `lcd_update()` call. It never opens
files, decodes bitmaps, or claims the shared audio buffer from a paint
function (`docs/desktop-mode-snow-leopard-spec.md`, Compositor and Memory,
Audio, and Storage Invariants). Video decode is exactly the kind of work that
is forbidden there.

Live TV's own guide already solves the harder version of this problem: the
DIRECTV grid is painted with ordinary `lcd_*` calls, committed with
`lcd_update_rect()` over regions that exclude the tuned channel's rectangle,
while `vo_setup()` blits decoded video into that rectangle on its own thread
(`apps/plugins/mpegplayer/video_out_rockbox.c`, `livetv_pig` /
`youtube_embedded` cases). That is already a windowed video player; it just
draws DIRECTV chrome around itself today.

The correct integration is therefore: **mpegplayer draws Snow Leopard window
chrome itself**, in the same plugin instance that already owns the video
thread and the audio buffer, when Desktop Mode launches it. Desktop Mode
never touches Live TV's pixels or its audio; it only launches and receives
control back, exactly as it already does for TextEdit, Calculator, and
Photos (`dm_launch_external()` in `apps/plugins/desktop_mode.c`).

This keeps both steering documents intact: Desktop Mode's compositor never
grows a video/audio path, and Live TV's audio lifecycle
(`docs/plugin-audio-lifecycle-steering.md`) is untouched — same
`stream_init()` / `stream_close()` / `stream_exit()`, same
`PCM_MIXER_CHAN_PLAYBACK`, same channel-change-is-`stream_close()`+
`stream_open()` behavior it already has standalone.

## Launch Path

Rockbox tears down the calling plugin on `rb->plugin_open()`, so "launching
Live TV from Desktop Mode" is the same handoff-and-return pattern already
used for every non-native Desktop Mode application, not a second live
window:

```
apps/plugins/desktop_mode.c   dm_open_selected_file() recognizes livetv.rock
    -> rb->plugin_open(PLUGIN_APPS_DIR "/livetv.rock", "-desktop")

apps/plugins/livetv.c         plugin_start() reads the "-desktop" parameter
    -> rb->plugin_open(VIEWERS_DIR "/mpegplayer.rock", "-livetvdm:/Videos/LiveTV")
                                                          ^^^^^^^^^ new prefix

apps/plugins/mpegplayer/mpegplayer.c
    recognizes "-livetvdm:" the same way it recognizes "-livetv:" today,
    and additionally sets mpegplayer_livetv_desktop = true
```

`livetv.c` currently ignores its `parameter` argument (`(void)parameter;`).
It gains one branch: when invoked with `"-desktop"`, it launches mpegplayer
with `LIVETV_PARAM_PREFIX_DESKTOP "-livetvdm:"` instead of
`LIVETV_PARAM_PREFIX "-livetv:"`. `Extras -> Applications -> Live TV`
(`apps/root_menu.c:5027`) is unchanged and keeps launching with `NULL`,
which keeps producing today's plain DIRECTV experience with no Mac chrome —
that entry point is for someone who just wants to watch TV, not open a
Desktop Mode window.

Inside Desktop Mode, only `livetv.rock` gets this special case in
`dm_open_selected_file()`; every other `.rock` file still opens with `NULL`,
matching current behavior for games and utilities launched from Finder.

`DM_APP_DIRECTV` is a permanent Dock slot between Calculator and System
Preferences. It launches `livetv.rock -desktop`; Finder launching
`livetv.rock` uses the same desktop parameter.

## Window Chrome

Mac chrome must come from the same real Snow Leopard capture Desktop Mode
already uses, per its Non-Negotiable Authenticity Rule. It must not be
invented, and it should not cost mpegplayer — already the most
memory-pressured plugin in this tree — the full window frame Desktop Mode
itself pays for.

Desktop Mode's `window-plain` asset is a single 304x174x16 BMP
(105,792 bytes decoded) that supplies title bar, body, and status bar
together; Live TV's window only needs the title bar strip with its three
real traffic lights, not a body it will paint itself. The importer gains one
additional derived logical ID:

```
chrome/title-bar-cap.304x24x16.bmp   (14,592 bytes decoded)
```

This is not a new Apple capture. It is a lossless pixel slice of the rows
already present in the existing `window-plain` source crop — the same
operation the spec already permits for repeated strips and nine-slice fills
— recorded in the manifest with its own provenance entry pointing at the
same source capture `window-plain` was cut from. `tools/prepare_snow_leopard_
desktop_assets.py` gains this derivation; no new mounted 10.6 source is
required, and existing packs simply regenerate with the new logical ID
present.

mpegplayer loads exactly this one 14.6 KB bitmap (via a small BMP loader
mirroring `dm_load_bmp_asset`'s format, not the general Desktop Mode asset
pipeline) plus the DIRECTV wordmark it already loads for the guide banner,
which doubles as the window's title — this avoids loading the Lucida Grande
atlas into mpegplayer at all. Traffic-light hit rectangles are fixed pixel
offsets within that captured strip, the same offsets Desktop Mode's own
shell already uses for Finder/iTunes windows, duplicated as constants in
`apps/plugins/mpegplayer/livetv.c` rather than shared code — mpegplayer and
desktop_mode are separate plugins with no shared runtime to put it in.

Actual mpegplayer headroom for these 14.6 KB plus a handful of state bytes
must still be measured against a real build, not assumed; see Verification.

## Window States

Two states, both driven by the existing `mpegplayer_livetv_pig` /
`livetv_show_guide` machinery in `mpegplayer.c`, gated by the new
`mpegplayer_livetv_desktop` flag:

| State | Trigger | Content |
| --- | --- | --- |
| `LIVETV_DM_WINDOWED` | default on entry; MENU from fullscreen | Title-bar cap with traffic lights at the standard Desktop Mode app window position (`8,24,304,174`, matching Finder/iTunes/Preview/Preferences). Below it: the tuned channel at PIG proportions (`LIVETV_PIG_W`/`LIVETV_PIG_H`), plus the existing one-line mini-guide strip (`livetv_draw_mini_guide()`) showing channel, title, and time in the already-sampled DIRECTV palette. |
| `LIVETV_DM_FULLSCREEN` | green click from windowed | The unmodified existing full 320x240 DIRECTV experience — interactive guide grid and tuned full-screen watch — pixel-identical to today's `Extras -> Applications -> Live TV`. Mac menu bar and Dock are off-screen because this state owns the whole LCD, the same way it already does standalone. |

There is no third "full interactive grid guide, but small" state. The
windowed view is deliberately the compact mini-guide, not a shrunk copy of
the six-row channel grid — cramming that grid into a ~150px content area
would violate the DIRECTV guide's own measured layout, which is sized
against the full screen (`docs/livetv-directv-guide-spec.md` §4.1). Anyone
who wants the interactive grid clicks green.

Control mapping when `mpegplayer_livetv_desktop` is set:

| Input | `LIVETV_DM_WINDOWED` | `LIVETV_DM_FULLSCREEN` |
| --- | --- | --- |
| Green click | enter `LIVETV_DM_FULLSCREEN` | — |
| MENU | close (see Close, below) | return to `LIVETV_DM_WINDOWED` (today's "back to guide, channel keeps playing" binding, retargeted) |
| Red click | close | — |
| Yellow click | minimize (see Minimize, below) | — |
| Wheel / Left / Right | channel up/down (existing) | unchanged (volume / channel per existing table) |

This reuses the guide spec's existing bindings verbatim; only what MENU
returns to changes, and only while the desktop flag is set.

### Close (red, or MENU from windowed)

Runs the exact exit path Live TV already uses today: `stream_close()`,
`stream_exit()`, restore of mixer/sample-rate state, `plugin_release_audio_
buffer()` only after callbacks are clear — the full sequence in
`docs/plugin-audio-lifecycle-steering.md` §5, unchanged. `rb->plugin_open()`
returns, and control lands back in `dm_open_selected_file()`'s caller inside
Desktop Mode, which redraws its saved desktop/Finder state exactly as it
does today after Calculator or TextEdit exits.

### Minimize (yellow)

Rockbox runs one plugin at a time, so there is no way for Live TV's decode
thread to keep running in the background while Desktop Mode's compositor
also owns the screen — unlike Finder or iTunes, which are native windows
inside Desktop Mode's own process and can genuinely persist. Minimize
therefore also runs the Close path underneath.

What makes this honestly "minimize" rather than a relabeled close: Live TV
has no session state to lose. Per its own spec, programming is driven
entirely by the wall clock and there is no pause — "the viewer may join
mid-show." A relaunch at any later instant lands on exactly what a real
broadcast would be showing, which is indistinguishable from having left it
running. `dm_activate_app()` records this by setting `state->minimized_app =
DM_APP_LIVETV` instead of clearing `running_apps` immediately the way
TextEdit/Calculator do today; relaunching (from Finder, in the first cut)
clears it again.

The genie-style shrink-into-the-Dock animation real macOS plays is
deliberately **not** in the first cut. Desktop Mode's minimize animation
targets a Dock icon position, and Live TV has no Dock slot to target (see
below) — animating toward an unmeasured, made-up point would violate the
spec's own rule that every window/Dock geometry number is measured from real
artwork, not chosen. M1 ships the functional contract (clean stop, restore
lands on live programming); the visual genie ships alongside the Dock slot
in M3, once that geometry is real.

## Dock Slot

The measured Dock now contains eight slots. DIRECTV uses the user's owned
wordmark artwork through the same private, checksummed asset pipeline as the
Snow Leopard icons, including the 32/34/38-pixel iPod variants and the
64/66/70-pixel 1080p variants. The running indicator remains visible behind
the window because mpegplayer restores the exact Desktop underlay after every
stream reopen.

## Rockpod Parity Session

Verified against `rockpod/services/desktop_mode.py:382-394`: the parity
session builder already symlinks every top-level entry of the connected
device's filesystem except `.rockbox` into the isolated simulator root
(`for source in entries: ... destination.symlink_to(source, ...)`). `/Videos`
is one such top-level entry, and `/Videos/LiveTV` lives underneath it — so
the whole Live TV content tree (`channels.tsv`, `guide.tsv`, `logos/`,
`shows/`, `ads/`) is already exposed into the parity session today, with no
new Rockpod code. This is the same mechanism that already lets Desktop
Mode's Finder and iTunes open real on-device files in a parity session
(`docs/desktop-mode-snow-leopard-spec.md`, `View iPod on Desktop`).

This was the one open question worth checking before writing this spec,
since the Live TV content tree is deliberately excluded from the *ordinary
video library scan* (`docs/livetv-directv-guide-spec.md` §6.1) — but that
exclusion is a tagcache/Video-Sync-time filter on the PC side, not a
filesystem visibility restriction, so it does not affect what the parity
session's device-root symlink exposes.

Remaining work here is verification, not new code: confirm
`rb->dir_exists(LIVETV_ROOT)` in `livetv.c` resolves correctly through the
symlink chain inside a parity session (it should, by the same precedent as
Finder/iTunes), and add a parity-session capture to the focused gate below.

## Memory, Audio, and Storage Invariants

Unchanged from `docs/plugin-audio-lifecycle-steering.md`; Live TV's audio
path is not modified by this feature. Added invariants specific to the new
chrome:

- mpegplayer loads the title-bar-cap bitmap and computes traffic-light hit
  rectangles once at desktop-mode entry, not per frame;
- the video thread's PIG blit and `lcd_update_rect()` exclusion region are
  unchanged from today's guide/PIG behavior — windowed mode is a
  differently-positioned PIG, not a new compositing path;
- no Lucida Grande font atlas is loaded by mpegplayer;
- minimize/close never call `audio_stop()` directly; they run the existing
  `stream_close()`/`stream_exit()` sequence exactly as MENU-to-leave does
  today.

## Implementation Plan

### M0: Baseline verification

- confirm `rb->dir_exists(LIVETV_ROOT)` and the schedule resolver work
  correctly inside a Rockpod parity session through the `/Videos` symlink;
- confirm current mpegplayer buffer headroom on `ipod6g` and `ipodvideo`
  with a real Live TV session running, as a pre-change baseline.

### M1: Windowed and fullscreen states

- add `title-bar-cap` to the importer and pack manifest, derived from the
  existing `window-plain` source crop;
- add `LIVETV_PARAM_PREFIX_DESKTOP` to `livetv.c` and the `"-desktop"`
  branch in `plugin_start()`;
- add `mpegplayer_livetv_desktop` to `mpegplayer.c`; implement
  `LIVETV_DM_WINDOWED` rendering (title-bar cap, PIG video, mini-guide
  strip) and the green/MENU state transitions in the table above;
- add the `livetv.rock` special case to `dm_open_selected_file()` in
  `desktop_mode.c`;
- implement minimize as the functional contract only (state tracking, clean
  stop, restore lands on live programming), no Dock animation.

Exit: on-device and in the parity session, opening Live TV from Desktop
Mode's Finder shows a real Mac-chrome window with live PIG video; green
reaches the unmodified full-screen guide; MENU/red return cleanly to Desktop
Mode; the audio matrix in `docs/plugin-audio-lifecycle-steering.md` passes
unchanged.

### M2: Measured budget

- add a checked-in size delta for mpegplayer's new chrome load, following
  the discipline in `docs/desktop-mode-snow-leopard-size-report.md`
  ("budget must be measured, not guessed") rather than asserting the
  14.6 KB estimate above is safe;
- if headroom is too tight on either target, the fallback is a smaller
  slice (traffic lights only, no title-bar background fill) before
  borrowing playback or video-decode memory — never the latter.

### M3: Dock slot and minimize genie (stretch)

- remeasure the real Dock capture for an 8th icon's pitch and
  magnification neighbors;
- add the `DM_APP_LIVETV` Dock entry and the genie-into-Dock minimize
  animation, now that it has a real geometric target.

## Files Expected to Change

```
apps/plugins/livetv.c
apps/plugins/mpegplayer/mpegplayer.c
apps/plugins/mpegplayer/livetv.[ch]
apps/plugins/mpegplayer/video_out_rockbox.c   (windowed PIG placement only)
apps/plugins/desktop_mode.c                   (dm_open_selected_file case)
tools/prepare_snow_leopard_desktop_assets.py  (title-bar-cap derivation)
docs/desktop-mode-snow-leopard-size-report.md (M2 delta)
```

No Rockpod changes are required for the parity session (see above).

## Verification

- `tools/desktop_mode_snow_leopard_sim_gate.sh` gains a Live TV capture:
  windowed entry, green to fullscreen, MENU back to windowed, minimize,
  restore, close;
- `tools/livetv_guide_sim_gate.py` gains a parity-session run confirming
  `/Videos/LiveTV` resolves through the Rockpod device-root symlink;
- audio matrix from `docs/plugin-audio-lifecycle-steering.md`: Database
  music -> Live TV (windowed) -> fullscreen -> windowed -> close ->
  Database music resumes; repeat from Files music; rapid open/close/
  minimize does not freeze or leave stale mixer state.

## Physical iPod Gate

Run on both 6G and 5G before release, in addition to the existing Live TV
hardware gate:

1. start Database music, open Desktop Mode, open Live TV from Finder;
2. confirm windowed chrome renders correctly and PIG video/audio are in
   sync;
3. green to fullscreen, watch a channel change, MENU back to windowed;
4. minimize, confirm clean audio/video stop, restore from Finder, confirm
   the channel now showing matches the current wall-clock schedule;
5. close, confirm Database music resumes with unchanged playlist identity
   and elapsed time;
6. repeat the cycle ten times; verify no descriptor or memory trend and no
   input latency regression versus the existing Desktop Mode hardware gate.

## Definition of Done

- Live TV opens as a Mac-chrome window from inside Desktop Mode, on-device
  and in the Rockpod parity session, using only real captured Snow Leopard
  pixels already in the pack plus one losslessly-derived slice;
- green, red, and MENU behave as specified above; minimize's functional
  contract (clean stop, live restore) works even though its genie animation
  is deferred;
- Live TV's existing standalone entry point and audio lifecycle guarantees
  are unchanged;
- the parity session requires no new Rockpod code, verified against the
  existing device-root symlink mechanism.
