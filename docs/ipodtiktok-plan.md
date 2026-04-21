# iPodTikTok Plan

`iPodTikTok` is a new Rockbox app for the iPod clickwheel target that presents a local vertical-video-style feed with TikTok-like navigation:

- scroll wheel forward: next video
- scroll wheel backward: previous video
- double-press `Select`: like / unlike current video
- `Play`: pause / resume
- `Menu`: exit feed

## Current constraints in this tree

- Rockbox already has a video player path through `mpegplayer`.
- The current browser code in `apps/root_menu.c` only treats MPEG-family files as playable.
- `mp4`, `m4v`, and `mov` are currently discoverable but explicitly rejected with a "Use MPEG-2 (.mpg)" message.
- On iPod targets, `mpegplayer` currently uses:
  - scroll wheel for volume
  - `Select` release for zoom
  - `Left` / `Right` for previous / next file or seek

That means a true TikTok-like feed cannot be built as a thin wrapper around the existing player. The feed controls must live inside a feed-aware video playback mode.

## Seed content

Initial source clips already exist on the host machine:

- `/home/david/Downloads/ipodtiktok1.mp4`
- `/home/david/Downloads/ipodtiktok2.mp4`

These are MP4 containers, so phase 1 needs a conversion/import step before Rockbox playback.

Recommended imported target paths:

- simulator: `build-sim-video-5g/simdisk/Videos/iPodTikTok/ipodtiktok1.mpg`
- simulator: `build-sim-video-5g/simdisk/Videos/iPodTikTok/ipodtiktok2.mpg`
- hardware: `/Videos/iPodTikTok/ipodtiktok1.mpg`
- hardware: `/Videos/iPodTikTok/ipodtiktok2.mpg`

## Recommended architecture

### 1. New app plugin

Add a new app plugin:

- `apps/plugins/ipodtiktok.c`

Responsibilities:

- ensure app data paths exist
- load `feed.tsv`, `likes.dat`, and `state.dat`
- launch `mpegplayer` in a dedicated feed mode
- return to the feed launcher cleanly after playback exits

Keep phase 1 simple: the launcher can immediately enter playback if there is at least one feed item.

### 2. Feed-aware mode in `mpegplayer`

Extend `apps/plugins/mpegplayer/` with a dedicated mode enabled by a parameter such as:

```text
-feed:/path/to/feed.tsv
```

Feed mode responsibilities:

- load the ordered feed list
- start at last saved index from `state.dat`
- replace the normal iPod button mapping behavior
- keep end-of-stream behavior inside the feed instead of generic next-file playback
- show lightweight feed overlays

This avoids forking the entire video stack into a second player plugin.

### 3. App data layout

Use a dedicated app data directory under Rockbox app storage, for example:

```text
/.rockbox/rocks/apps/ipodtiktok/
```

Files:

- `feed.tsv`: ordered list of playable clips and display metadata
- `likes.dat`: liked clip ids or paths
- `state.dat`: last viewed index and optional last position

Suggested `feed.tsv` format:

```tsv
id	title	path
ipodtiktok1	Clip 1	/Videos/iPodTikTok/ipodtiktok1.mpg
ipodtiktok2	Clip 2	/Videos/iPodTikTok/ipodtiktok2.mpg
```

## Control design

Phase 1 control mapping:

- `BUTTON_SCROLL_FWD` or repeat: skip immediately to next clip
- `BUTTON_SCROLL_BACK` or repeat: skip immediately to previous clip
- double `BUTTON_SELECT|BUTTON_REL` within about `HZ/3`: toggle like
- `BUTTON_PLAY|BUTTON_REL`: pause / resume
- `BUTTON_MENU`: exit app
- `BUTTON_LEFT` / `BUTTON_RIGHT`: optional small seek, or leave unused in phase 1

Important implementation detail:

- single `Select` should not trigger another action in phase 1
- wait for a second `Select` release inside a short window before toggling like
- add a short skip cooldown after wheel navigation so one wheel flick does not jump multiple clips unintentionally

## UI direction

The UI should mimic the feel of a TikTok feed without pretending Rockbox can support the whole product model.

Phase 1 UI:

- full-screen video
- small top-left title text
- small bottom-left clip counter like `1/2`
- right-side heart indicator when liked
- brief heart pulse overlay after a successful double-press like

Not in phase 1:

- comments
- shares
- networking
- algorithmic ranking
- live metadata downloads

## Import and conversion pipeline

The two starting clips should be imported from `Downloads` and converted to MPEG-2 program stream files.

Suggested conversion target:

- 320x240 output
- 24 fps
- MPEG-2 video
- MP2 audio
- aspect-ratio padded to fill the iPod Video screen cleanly

Representative command shape:

```bash
ffmpeg -i ipodtiktok1.mp4 \
  -vf "scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2:black,fps=24" \
  -c:v mpeg2video -q:v 6 -maxrate 1500k -bufsize 1835k \
  -c:a mp2 -ar 44100 -b:a 128k \
  ipodtiktok1.mpg
```

Phase 1 does not need an in-device transcoder. A host-side helper script is enough.

## Implementation breakdown

### Phase 1: playable prototype

- add `ipodtiktok.c` to `apps/plugins/`
- add it to `apps/plugins/SOURCES`
- add feed mode parameter parsing to `mpegplayer`
- add feed-mode button loop overrides for clickwheel targets
- hard-seed `feed.tsv` with the two imported clips
- persist likes and current index

Deliverable:

- open `iPodTikTok` from Plugins > Apps
- app starts the first imported clip
- wheel moves through the two-item feed
- double-select likes and persists

### Phase 2: polish

- animated heart pulse
- nicer overlay typography and truncation
- clip title extraction from filename when metadata is missing
- optional resume behavior when re-entering the app
- optional root-menu shortcut

### Phase 3: content tooling

- host-side import script for `Downloads/ipodtiktok*.mp4`
- automatic feed.tsv regeneration
- optional simulator install helper

## Key risks

- `mpegplayer` is old and tightly owns its input loop, so feed mode changes should be isolated behind a small explicit mode flag.
- Double-press detection on clickwheel hardware needs careful debounce handling.
- Vertical videos will usually be letterboxed because the iPod Video screen is 4:3.
- MP4 source clips must be transcoded before device playback.

## Recommended first implementation step

Build the smallest real slice:

1. Create `ipodtiktok.c` and launch a new `mpegplayer` feed mode.
2. Import exactly two converted clips from the existing `Downloads` MP4s.
3. Override iPod feed controls inside `mpegplayer`.
4. Persist likes in a tiny flat file.

That gets the user-visible concept working before adding menu polish or broader video library support.
