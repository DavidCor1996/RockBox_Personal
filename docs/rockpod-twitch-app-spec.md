# RockPod Twitch App Specification

## Product intent

RockPod Twitch is an offline, iPod-native Twitch client for media the user is
allowed to download.  It sits immediately after YouTube in the iPodJS
Applications menu and in RockPod's Applications source list.  Its visual
language keeps the compact, thumbnail-led stock iPod layout while using
Twitch's current official Glitch, wordmark, purple, and charcoal palette. It
retains an always-visible LIVE bug during linear playback.

The app does not claim to connect an iPod to Twitch.  RockPod on the computer
downloads and converts VODs, then publishes a bounded offline library to the
iPod.  The device presents that library as live, wall-clock channels.

The first shipping target is iPod Classic 6G/7G.  iPod Video 5G/5.5G native
player work is explicitly deferred; this implementation does not alter its
VideoCore playback path.

## Authenticity and asset policy

- No generated, traced, hand-drawn, or text-substitute Twitch logos are used.
- The application icon is the current official purple Twitch Glitch on a
  uniform white rounded iPod application tile.
- The in-app wordmark and compact player Glitch are current official assets
  from Twitch's downloadable brand package.
- Resizing, colour-depth conversion, and magenta-key packaging are mechanical
  target conversions only.  Source files and provenance stay checked in under
  `assets/ipodjs/sources/twitch/`.
- Creator avatars and VOD thumbnails come from Twitch/yt-dlp metadata or the
  user's selected local files.  Placeholder states use ordinary UI chrome and
  text, never invented brand artwork.

The compact UI hierarchy still fits the stock 320×240 iPod interaction model;
no Twitch screenshots are embedded.

## RockPod data model

### Creators

Each creator has a stable key, normalized `twitch.tv/<login>` URL, display
name, configurable VOD retention count, optional avatar, and `cycle_epoch`.
The default retention is three VODs; the user may choose 1–20.

### VODs

Each VOD stores its Twitch video ID, creator key, title, game/category,
description, duration, publication date, view count, source URL, local source
path, thumbnail path, and source signatures used by incremental sync.

### Chat replay and emoji

For public VODs with retained chat, RockPod reads the replay comments used by
Twitch's web VOD page and exports them as an ordered, timestamped sidecar. Each
record retains the playback offset, display name, Twitch username colour, and
message. Twitch emotes and Unicode emoji are resolved during computer-side
sync, resized from their real CDN artwork, and packed into a fixed-record RGBA
sprite file. Emoji parsing covers presentation selectors, skin-tone modifiers,
flags, keycaps, tag sequences, and zero-width-joiner sequences. Unsupported or
unavailable artwork falls back to readable text; it is never replaced by
hand-drawn art.

Replay metadata is best-effort because Twitch does not publish a supported VOD
chat-replay API. A chat-disabled, removed, or changed replay endpoint cannot
invalidate an otherwise playable VOD.

### One live programme per creator

All retained VODs remain available in the VOD archive.  A creator's Live
Channel is a deterministic loop over that creator's retained VODs, ordered by
publication time and ID.  At time `now`:

1. Sum the positive VOD durations for that creator.
2. Compute `(now - cycle_epoch) mod total_duration`.
3. Select the single VOD containing that offset.
4. Launch it with a per-programme start epoch so the player joins at the
   correct elapsed position.

Thus RockPod may sync multiple VODs for a creator while the iPod exposes and
plays exactly one live programme for that creator at any instant.  When that
programme reaches end-of-file, the player returns a continuation token to the
Twitch app; the app re-evaluates wall-clock state and launches the next VOD.
Menu exits back to the Twitch detail view.  Live playback cannot pause or
seek.  Direct VOD playback remains pauseable and seekable.

## Device export contract

RockPod writes:

- `/.rockbox/twitch/creators.tsv`
- `/.rockbox/twitch/vods.tsv`
- `/.rockbox/twitch/thumbnails/<vod-id>.bmp` at 96×54 RGB24
- `/Twitch/videos/<vod-id>.m4v` as the default/primary media
- `/Twitch/videos/<vod-id>.mpg` as a cached recovery sibling when H.264 was
  selected, or as the primary only when H.264 preparation fails
- `/Twitch/videos/<vod-id>.twm` playback metadata
- `/Twitch/videos/<vod-id>.twc` timestamped ASCII chat stream
- `/Twitch/videos/<vod-id>.twe` fixed 14×14 RGBA emote/emoji records
- `/.rockbox/ipodjs/twitch/` authentic packaged Twitch assets

MPEG sync uses the standard RockPod MPEG conversion. H.264 sync uses the
Apple-exact profile and shared video-player routing; source audio is always
decoded and re-encoded as AAC-LC, 44.1 kHz stereo instead of being copied from
Twitch. A source that contains audio cannot pass H.264 or MPEG staging if the
result loses its audio track. Every file is staged and atomically replaced;
the device sync index avoids needless retranscodes and copies. A stale index
entry cannot make a missing device file appear current. Download, conversion,
thumbnail, and chat failures are isolated per creator/VOD so one failure does
not abort later VODs or prune previously working media. Stale cleanup is
confined to Twitch-owned stable IDs.

## Device interaction

- Header tabs: `Live Channels` and `VODs`.
- Scroll wheel: move through creators or VODs.
- Left/right: switch tabs; in detail, switch between Watch and Back.
- Select: open detail or activate the selected action.
- During Twitch playback, Select toggles the optional right-side chat rail
  without pausing or seeking the stream. The rail uses an eased 400 ms slide;
  the video continuously aspect-fits into the remaining width during that
  motion instead of being covered by the panel.
- Play: immediately watch the selected creator/VOD.
- Menu: back one level, then exit to Applications.
- Live rows show creator, current title/game, viewer count metadata, and a red
  LIVE label.  VOD rows show title, creator/game, duration, and views.
- Player OSD uses current Twitch purple/charcoal chrome. Live mode retains a
  purple Twitch strip and red LIVE bug; VOD mode puts title, progress, and time
  on separate rows so the progress rail never crosses text.
- Chat displays the five most recent messages at the current playback time,
  with Twitch username colours and real Twitch/Twemoji sprites. Its full-height
  translucent charcoal rail occupies 146 pixels at the right edge when open.
  The purple header uses the packaged official Twitch Glitch. The panel never
  overlaps video: chat and video share one animated boundary for the entire
  slide-in and slide-out transition.

## Resource and playback constraints

- The browser never calls `audio_stop()`, touches PCM/mixer state, claims the
  shared audio buffer, or mutates playlists.
- Playback ownership remains entirely in `mpegplayer` or
  `openh264_player`, following the repository's existing media lifecycle.
- The AAC chunk lookup keeps the full unsigned 32-bit `stco` position range;
  files between 2 GiB and 4 GiB must not reinterpret later audio offsets as
  negative values during live join, seek, or sequential decode.
- Browser assets live in plugin BSS: creator/VOD metadata plus one logo and a
  two-slot 96×54 thumbnail cache, budgeted below 112 KiB.
- Draw functions only paint cached pixels.  Thumbnail I/O/decoding is limited
  to one requested image from the idle action-loop service point, after a
  settle delay and only with an empty input queue.
- Chat sidecar scanning and emoji-record prefetch run only from playback service
  points. The draw callback sees five cached messages and a fixed 32-entry emoji
  cache (about 25 KiB); it never opens, seeks, reads, decodes, or allocates.
- No core allocation, framebuffer copy, storage scan, thumbnail decode, or
  metadata query occurs during drawing.
- Live joins derive position from RTC elapsed time and never write a resume
  bookmark.  Player teardown remains responsible for stopping callbacks and
  releasing the shared audio buffer before returning to the app.

## Acceptance criteria

1. Twitch appears immediately after YouTube in both Applications views and
   uses the packaged authentic Twitch icon.
2. RockPod adds/removes creators, imports local or Twitch VOD URLs, retains the
   requested number per creator, and syncs without blocking the UI thread.
3. A synced creator with multiple positive-duration VODs resolves to exactly
   one current programme and rotates at duration boundaries.
4. Live launch works on MPEG and Apple H.264 paths, joins at wall-clock
   position, disables pause/seek, advances at EOF, and returns on Menu.
5. VOD launch is pauseable/seekable and returns to its detail page.
6. The simulator and native iPod 6G builds compile.  Focused service tests cover
   URL normalization, retention, schedule resolution, incremental export, and
   menu/player integration.
7. Select toggles chat in both MPEG and Apple H.264 playback. Replay messages
   track playback and seeks, and real Twitch emotes plus Unicode emoji render
   from the bounded sprite cache without playback-memory ownership changes.
   The eased full-height panel transition reflows the video on every animation
   frame and does not obscure it.
8. Device testing covers fresh boot, Database/Files music → Twitch,
   Twitch → Database/Files music, live boundary rotation, volume, Menu exit,
   and rapid Applications/music switching without freeze or loss of audio.
9. H.264 remains the first launch choice. An H.264/VPU/AAC failure may open
   the prebuilt MPEG sibling, but a successful H.264 does not route through
   MPEG. Sustained AAC regression includes non-silent playback after seeking
   to a chunk whose file position is greater than 2,147,483,647.
