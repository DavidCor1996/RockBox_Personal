# WWE Backstage Interactive Video Port Spec

## Target

Port the Flashpoint entry:

- Title: Can You Survive Backstage in WWE?: An Interactive WWE Adventure
- Flashpoint ID: `46ad6a0a-31c7-4b93-aab5-6599b4ac98a5`
- Platform: HTML5
- Source URL: `https://www.youtube.com/watch?v=JRyzE1tdfPM`
- Flashpoint launch command: `http://youtube.com/WWE/Backstage/index.html`
- Flashpoint notes: branching paths were rebuilt with MolleIndustria's TweeVee
  engine
- Flashpoint package size: about 181 MB

This is not a Flash/SWF port. The existing `flashplayer` work is useful as a
reference for C++ plugin build plumbing and asset-package thinking, but this
entry should not go through `gameswf` unless the Flashpoint package unexpectedly
contains a SWF wrapper. Treat it as a branching live-action video application.

## Decision

Build a native Rockbox branching-video package player and feed it preconverted
`.rvp` video nodes.

Recommended shape:

- generic viewer plugin: `apps/plugins/tweevee_player.c`, associated with
  `.twv`
- package root: `.rockbox/tweevee/<slug>/`
- manifest file: `<slug>.twv`
- video nodes: `.rvp` marker files plus `.yuv` / `.pcm` sidecars using the
  existing RockPod raw video format
- still/menu overlays: optional BMP assets generated at import time

Do not embed an HTML engine, JavaScript engine, or browser DOM for this entry.
The game mechanic is node playback plus timed choices, so a data-driven native
controller is much smaller and more reliable on iPod 5G/6G hardware.

## Current Code To Reuse

Use `apps/plugins/openh264_player.c` as the media engine reference:

- `raw_audio_stop()` / `raw_audio_shutdown()` implement the required stop,
  fade restore, mixer restore, and shared-buffer release lifecycle.
- `raw_audio_start_at_frame()` uses `PCM_MIXER_CHAN_PLAYBACK`, sets the mixer
  frequency, and starts PCM through the mixer channel.
- `parse_rvp_segments()` already parses single-file and segmented `.rvp`
  markers.
- `play_raw_segment()` already handles raw YUV420 frame pacing, PCM sync,
  pause/resume, late-frame skip, OSD, and `lcd_blit_yuv()`.
- `play_raw_rvp()` already acquires `plugin_get_audio_buffer()`, computes
  buffer layout, preloads audio, handles multi-segment videos, and logs stats.

The first implementation should factor these pieces into a small shared raw
video helper used by both `openh264_player` and `tweevee_player`. Avoid copying
the full 2,000-line plugin and then letting the two playback paths diverge.

Proposed split:

- `apps/plugins/lib/rvp_player.h`
- `apps/plugins/lib/rvp_player.c`
- `apps/plugins/openh264_player.c` keeps `.h264` scan/profiling and calls the
  shared RVP entry point for `.rvp`
- `apps/plugins/tweevee_player.c` owns branching, choices, and package state
  and calls the shared RVP segment/frame playback primitives

If a first prototype needs less churn, add a narrow exported function inside
`openh264_player.c` locally first, prove the branching model, then factor.

## Package Format

Use a line-oriented manifest, not JSON, unless a small parser already exists in
the target plugin path. Rockbox-side parsing should be simple and deterministic.

Example:

```text
ROCKPOD_TWEEVEE_V1
id=46ad6a0a-31c7-4b93-aab5-6599b4ac98a5
title=Can You Survive Backstage in WWE?
start=intro
fps=20
sample_rate=44100

[node intro]
video=intro.rvp
choice1_label=Help Dolph
choice1_target=help_dolph
choice1_button=select
choice2_label=Find security
choice2_target=security
choice2_button=right
timeout_target=late

[node help_dolph]
video=help_dolph.rvp
choice1_label=Continue
choice1_target=hallway
choice1_button=select
```

Required manifest fields:

- `start`
- each node's stable id
- each node's `video`
- zero or more choices with label, target, and button binding
- optional timeout/default target if a clip should auto-advance

Optional fields:

- `choice*_start_ms` / `choice*_end_ms` for choices that only appear near the
  end of a video
- `poster=<bmp>` for node preview/menu screens
- `resume_node` and `resume_frame` save file support
- `credits` / `source` / `version`

Save runtime state under:

```text
/.rockbox/tweevee/<slug>/save.dat
```

Keep the save format separate from the manifest so synced packages stay
read-only if desired.

## Asset Preparation

Add a host-side importer rather than doing extraction on the iPod.

Suggested script:

```text
tools/flashpoint_tweevee_prepare.py
```

Responsibilities:

1. Accept either a Flashpoint package directory or a URL/source directory
   containing the rebuilt TweeVee `index.html` and assets.
2. Locate the TweeVee data model in the HTML/JS. Record the parser rule in the
   script, not in the firmware plugin.
3. Resolve every referenced video asset.
4. Convert each video to `.rvp`:
   - video: raw YUV420p
   - dimensions: 320x240, padded/contained
   - fps: 20 by default
   - audio: signed 16-bit little-endian stereo PCM
   - sample rate: 44100 Hz
5. Split long raw outputs using the existing segmented `.rvp` format.
6. Emit `<slug>.twv`, `.rvp`, `.yuv`, `.pcm`, and optional posters.

Reuse the existing RockPod conversion rules:

- `rockpod/services/video_rvp.py` already builds the `scale,pad,fps` filter,
  writes `.rvp` markers, writes silence for audio-less clips, and segments large
  outputs.
- `tools/rvp_split_device_video.py` is a standalone device-side bundle splitter
  reference.
- `rockpod/scripts/youtube_movie_import.py` is a useful reference for
  authorized YouTube download flow, but this port should prefer the preserved
  Flashpoint package assets when available.

Do not rely on the public YouTube URL being downloadable forever. The importer
may support it as a convenience for authorized local use, but the durable port
path is Flashpoint package input.

## Playback UX

Controls for iPod 5G/6G:

- `MENU`: exit to previous menu after clean audio shutdown
- `PLAY`: pause/resume current clip
- `SELECT`: activate highlighted/default choice
- wheel left/right or previous/next: move choice selection
- volume keys / wheel volume path: use normal Rockbox sound settings

Choice UI:

- Show no UI during normal clip playback unless the node has active choices.
- At the choice window, draw a compact bottom overlay with 1-3 choices.
- Keep text short and generated from the manifest; ellipsize if necessary.
- If a timeout target exists, show a progress bar/countdown.
- If a node has exactly one continuation choice and no timeout, allow `SELECT`
  and optionally auto-advance after a short delay only if the original game did.

The overlay should be drawn on top of the last decoded frame or current video
frame. The shared RVP helper will need a callback hook such as:

```c
enum rvp_overlay_action (*overlay_cb)(long frame, long total_frames, void *ctx);
```

The callback should be able to:

- draw choices after the video blit and before `lcd_update_rect`
- request pause-like waiting at the end of a node
- return a target node id or action to the controller

## Audio Lifecycle Requirements

This is full-screen media playback. Follow
`docs/plugin-audio-lifecycle-steering.md`.

Hard requirements:

- take the shared audio buffer with `plugin_get_audio_buffer()` and let core
  stop existing playback;
- use `PCM_MIXER_CHAN_PLAYBACK` for clip audio;
- call `pcmbuf_fade(false, true)` when taking over and restore with
  `pcmbuf_fade(false, false)`;
- stop mixer callbacks before switching nodes or freeing/reusing audio memory;
- preserve sample-rate/mixer state and restore it on exit;
- never mutate the user's playlist to play this package;
- on iPod 6G/CS42L55, keep the existing wake path and do not bypass the mixer
  for long-form video audio.

For node transitions, treat each target jump like a segment switch:

- stop or pause the playback channel;
- clear callbacks referencing the previous node buffer;
- rebuild the next node's audio/frame state;
- then resume.

If back-to-back choices should feel seamless, add one-node audio prefetch later.
Do not make prefetch part of the first correctness gate.

## Implementation Milestones

### M0: Package Recon

Obtain the Flashpoint package and inspect:

- exact TweeVee version/layout;
- how node ids, labels, video URLs, and terminal states are encoded;
- count and duration of video nodes;
- whether all videos are local in the package or referenced externally;
- whether the Flashpoint "hacked" status implies custom JS that must be
  translated manually.

Deliverable: a generated manifest draft plus asset inventory.

### M1: Host Importer

Create the importer and generate a local package:

```text
/.rockbox/tweevee/wwe-backstage/wwe-backstage.twv
/.rockbox/tweevee/wwe-backstage/*.rvp
/.rockbox/tweevee/wwe-backstage/*.yuv
/.rockbox/tweevee/wwe-backstage/*.pcm
```

Pass criteria:

- every manifest target exists;
- every `.rvp` marker parses with the current RVP parser;
- total package size is predictable before sync;
- long clips are segmented.

### M2: Shared RVP Helper

Factor the reusable RVP parser/playback code out of `openh264_player.c`.

Pass criteria:

- selecting a normal `.rvp` from Videos still works;
- audio lifecycle logs are unchanged or improved;
- simulator and iPod hardware still play `phone_raw.rvp`;
- no duplicate raw-video implementation exists in the new plugin.

### M3: Minimal Branching Player

Implement `.twv` association and a node loop:

1. parse manifest;
2. start node;
3. play its `.rvp`;
4. display choices at end;
5. jump to selected target;
6. exit cleanly.

Pass criteria:

- two-node hand-authored test package works in the simulator;
- `MENU` exits;
- `PLAY` pauses/resumes;
- choice selection does not desync audio/video.

### M4: WWE Package Playability

Run the generated WWE package.

Pass criteria:

- intro node starts from the file browser;
- every reachable choice target can be visited;
- endings return to a restart/menu state;
- no missing labels or dead targets;
- package can be resumed from last node if save support is enabled.

### M5: Hardware Polish

Test on iPod Video 5G and iPod Classic 6G/7G:

- fresh boot -> package with sound;
- Database music -> package with sound;
- Files music -> package with sound;
- package exit -> Database and Files music both start with sound;
- rapid menu/package/music switching;
- pause/resume;
- volume overlay;
- long path with multiple node transitions.

## Risks

- The Flashpoint package may store the branching graph in custom JS rather than
  plain TweeVee data. Mitigation: keep the importer flexible and allow a manual
  manifest override file.
- Raw `.rvp` is large. The package is already about 181 MB in Flashpoint form;
  raw converted output may be much larger. Mitigation: use segmented `.rvp`,
  consider the existing compact 10 fps profile for low-storage devices, and keep
  a future H.264 path open after OpenH264 is proven.
- Choice overlays require RVP playback callbacks. Mitigation: first show choices
  only after a node completes; add in-video timed overlays after the basic graph
  is playable.
- YouTube source availability and legal access are unstable. Mitigation: import
  from locally obtained Flashpoint assets first.

## Non-Goals

- No embedded browser.
- No JavaScript VM for this first port.
- No SWF/gameswf work for this entry.
- No MPEG Player fork unless RVP storage becomes unacceptable.
- No playlist mutation or background music integration.

## Open Questions

- Does the preserved package include all video files locally?
- Are choices only end-of-clip, or do any choices occur while video keeps
  running?
- Does the rebuilt TweeVee graph include dead-end/restart semantics that should
  map to Rockbox buttons?
- Is the target device primarily iPod Video 5G, iPod 6G, or both for acceptance?
- Should this become a generic `.twv` viewer for other Flashpoint/TweeVee
  entries, or a one-off WWE package player after the prototype?
