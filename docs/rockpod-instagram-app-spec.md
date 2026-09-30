# RockPod Instagram Application

## Product shape

- Standalone offline application under `Extras -> Applications -> Instagram`.
- Home-first social feed: the newest synced posts across every imported
  account are ordered by date and presented one post at a time with author,
  avatar, square media, likes, caption, date, and photo/video identity.
- Home remembers the selected carousel group in `state.cfg`. A later launch
  resumes there instead of returning to the newest post every time.
- Synced videos autoplay in place as silent, square 30 fps feed previews and
  repeat until the wheel moves to another post. Menu returns to the same Home
  card without relaunching it, Right opens its author profile, and Select
  toggles the same persistent like used by photo posts.
- Right from a Home post opens its author profile. Profile pages show the real
  avatar, display name, username, verification state, bio, post/follower/
  following counts, and a click-wheel-selectable three-column media grid.
- Center opens the selected photo or video. Left or Menu returns from a
  profile to Home; Right on a profile cycles to the next synced account.
- The visual target is Instagram 1.0 from 2010: square media, navy navigation
  chrome, a warm cream feed, restrained blue selection accents, compact
  metadata, disclosure chevrons, and persistent position scrollbars. Modern
  gradients, Stories rings, floating controls, and card UI are intentionally
  excluded.
- The bottom Home/Search/+/Profile bar reproduces Instagram's app
  identity while Home and Profile remain the functional offline sections.
  Menu exits only from Home, following the stock iPod one-level-back model.
- The Extras right pane uses profile pictures cropped edge-to-edge, like the
  YouTube, TikTok, and OnlyFans preview panes.

## Import and metadata

- Import is provided by the maintained open-source `gallery-dl` Instagram
  extractor, authenticated from the user's existing Firefox session.
- Original media is retained locally. Captions, post dates, like counts,
  profile name, avatar, verification and available profile counts are kept.
- A per-profile gallery-dl archive makes updates incremental and prevents
  completed media from being downloaded again.
- Posts are sorted newest-first. Media from the same profile with the same
  non-empty description is emitted as one carousel group, shown once in Home
  and the profile grid. Its real children remain individually selectable with
  Left/Right in the viewer and use one persistent group like.
- Styled Unicode profile names and biographies are preserved verbatim. During
  sync, RockPod rasterizes those exact code points with the official Noto Sans
  Math font and uses the official Noto Emoji tulip for U+1F337. The bounded
  profile text bitmaps retain Jasmine's script lettering and flowers without
  missing-glyph squares or runtime font loading.

## Device format and safety

- Photos are converted on the host to fixed, bounded BMP views. Videos use
  the established 320x240, 30 fps MPEG-2 pipeline and the existing
  mpegplayer audio lifecycle.
- Video sync keeps two indexed outputs: the ordinary 320x240 media file with
  audio for explicit playback, and a separate audio-free 180x180 square feed
  preview centered on the 320x240 cream canvas. DeviceSyncIndex signatures
  skip both conversions when the source is already current.
- Every photo has one precomputed 480x300 (1.5x) view made directly from the
  real source photo. Selecting a photo opens the viewer; the wheel changes
  zoom and wheel position pans within that cached bitmap without runtime
  scaling, repeated file reads, or additional framebuffer allocation.
- Play/Pause pauses or resumes video; Select toggles a persistent device-local
  like.
  Likes survive app restarts and library resyncs in `likes.tsv`; the UI uses
  transparent gray and Instagram-blue states derived mechanically from the
  authentic Glyphish 29-heart asset used by the early iOS-era interface,
  never a generated or hand-drawn substitute.
- Each post receives a dedicated 160x160 feed image plus a 72x72 profile-grid
  thumbnail. Existing libraries without feed images fall back safely to their
  thumbnails until the next sync.
- Render paths only decode already-synced assets. They do not access the
  network, scan directories, or allocate full-screen transition buffers.
- Instagram owns 414,620 bytes of fixed BSS image workspace: three grid
  thumbnails, one feed image, one 480x300 photo-viewer frame plus decoder
  scratch, logo/avatar buffers, and bounded name/bio bitmaps. It does not call
  `font_load`, `core_alloc`, claim playback memory, or change audio/playlist
  state.
- The current ARM audit is 572,500 bytes of Instagram-plugin BSS and 21,600
  bytes of text. The additional remembered autoplay paths are fixed at 96
  entries. Mpegplayer embeds one 20x20 source-derived marker in its read-only
  image instead of reserving the former full-width chrome buffer; its BSS is
  now 1,820,368 bytes, 38,572 bytes lower than the previous 1,858,940-byte
  build. No render, zoom, carousel, like, or navigation action grows either
  footprint.
- Brand artwork is a real Instagram app icon; no generated or hand-drawn
  logo is used.
- App chrome follows Instagram's early blue navigation and dark tab-bar
  treatment, without copying the source screenshot's account-specific tab
  name. It belongs to Instagram's Home/Profile screens and never gets stamped
  around the decoded video frame.
- The video overlay follows Instagram 4.0 from June 2013, when video first
  shipped: one small translucent play marker in the upper-right of the square,
  with no transport bar, author text, likes, caption, or app navigation over
  the media. The 20x20 marker is mechanically isolated from a period iPhone
  feed screenshot published by The Next Web. It appears briefly at launch,
  clears while playback continues, and remains on a paused cover as the
  period-appropriate resume cue.

## Authentic artwork and post options refresh

- Use the sourced 2013 wordmark and actual Instagram Home/activity/profile
  glyphs. Three working tabs replace the inactive upload placeholder.
- Hold Select on a feed or profile post for Read caption, Open author profile,
  Like/unlike, or Favorite/unfavorite author. Play retains its existing quick
  like/profile-favorite behavior. Likes and favorites are stored locally.
- The caption reader scrolls with the wheel and returns with Menu. New imports
  and syncs retain up to 2,200 caption characters; existing shortened imports
  need reimporting to recover text no longer in their cache.
- Video feed chrome uses the same compiled header/tab artwork. Playback and
  audio lifecycle are unchanged.
- Caption storage adds 193,920 bytes for 96 posts, line offsets 4,408 bytes,
  and TSV parsing uses bounded static 4KiB buffers instead of larger stacks.
  The two shared RGB565 branding bitmaps total 33,280 bytes per plugin. No
  core allocation, playback-buffer ownership, or draw-time I/O is added.

Validation for this refresh: the iPod 6G native and simulator builds pass, as
do all 15 existing Instagram host checks. Simulator captures cover the
Applications icon, Instagram feed, post options, caption reader, profile grid,
photo viewer, and real cached video feed. Package entries match the built
Instagram/video plugins and Twitter icon. The broader navigation regression
stops at “simulator did not enter a new list after Artist”; it is not a passing
playback/navigation stress result. No physical deployment was performed for
this refresh.
