# WPS RetailOS parity extension

## Contract

Target: the current iPodJS Classic WPS on iPod 6G/7G. Keep Rockbox's WPS
action loop, playback-owned artwork, playlist identity, Menu history, Hold,
idle screen, volume, rating persistence and long-Center Lyrics behavior.
Artwork and rating retain their controls. At the user’s request the equalizer
page is removed; the appended scrubber follows rating directly.

Source graphics are from the verified private iPod35 RetailOS 2.0.4 archive
described in `ipodjs-retailos-full-port-spec.md`. This is a resource port and
native behavior implementation, not a port of Apple's executable player.
Apple's Classic guide (2009), pages 25–32, defines the comparison behaviors.

## Artwork readiness

Resource 6, `CoverFlow_Proxy_Image`, is the original 128x128 music-note cover.
Use it whenever the artwork page has no completed playback artwork, including
the first frame, permanently missing artwork, and track changes. Project it
through the same existing cover/reflection renderer as actual artwork.
Never display equalizer frames automatically while waiting for a cover.
If the private cover asset is absent, leave the cover well empty; do not
invent artwork or fall back to bars. A late completed cover must invalidate
the placeholder frame and replace it without a playback restart.

The cover has fixed native-pixel storage (32,768 bytes on hardware). Prepare
it outside drawing. Do not claim a new artwork slot or allocate from core.

## Additional controls

- Scrubber: append a page whose wheel seeks in the current track; clamp to
  track bounds and preserve paused/playing state. Use the native progress
  strip and resource 298, `NowPlaying_ProgressBar_Scrub_Image` (13x28).
  Volume changes remain available on the ordinary artwork page.
- Shuffle: append a wheel-operated selector using RetailOS option-bar parts
  and resource 293, `NowPlaying_Large_Shuffle_Image` (25x19). Use Rockbox's
  existing current-track-preserving shuffle/sort operations. Only expose
  implemented shuffle modes. Do not call random song shuffle “Albums”.
- Lyrics: add a native text page for bounded, supported lyric input, retaining
  long Center's full Lyrics plugin. All file reading belongs to explicit page
  entry; painting and wheel scrolling use cached text only. Skip unavailable
  text rather than displaying an inactive feature.
- Information: expose existing cached track comment/detail text without
  pretending Rockbox song tags are a full podcast episode database.

## Explicit compatibility boundaries

- Album shuffle needs a real album grouping/order implementation integrated
  with playlist mutation, insertion, repeat, resume and external shuffle
  changes. An “Albums” label without this backend is prohibited.
- Genius needs Apple-compatible recommendation data and playlist generation.
  Assets 357–360 alone do not supply that backend. Keep the page absent when
  such data/implementation is unavailable, as RetailOS does for missing data.
- Ratings remain Rockbox database ratings. iTunes round-trip synchronization
  requires a separate, validated Apple database writer and is not a UI edit.
- Stock long-Center commands conflict with the retained Lyrics shortcut.
  Do not replace it without the user's control-order preference.
- Podcast date/episode/chapter parity requires corresponding metadata and
  chapter-aware playback support. Do not relabel album/year as episode/date.
- Do not invent page-transition animation or claim exact timing without an
  authoritative RetailOS recording. Existing source-frame animations remain.

## Validation

Verify first entry without completed art, late art arrival, permanent no-art,
rectangular art, track changes and pause; scrub both ends and
while paused; shuffle preserves current track and can restore order; rating
still persists; lyrics/menu return retains the origin. Validate actual C
boundary/parser behavior, native and simulator builds, native text/data/BSS,
UI stack cost, source-only cached painting, and the navigation regression
gate (10 hierarchy cycles, 20 rapid Albums/Artists switches during playback).
Hardware deployment is separate from this implementation task.

## Implemented and verified (2026-09-12)

The compatibility order is artwork → rating → scrubber → shuffle
→ lyrics (when available) → information (when a comment is available) → artwork.
Long Center continues to open the existing Lyrics plugin. The current-playlist
viewer remains blocked. Track changes and new WPS sessions reset to artwork.

The static fallback uses original resource 6. The scrubber uses original
resource 298 over the empty retail rail (296), with its centre aligned to the
playhead; it replaces the filled progress display while seeking. The shuffle control uses resource 293, the light Now Playing
option-bar well (107–109), and the blue original option-bar thumb (98–100),
with original Apple fonts. These are original pixel resources assembled in
native code; the two-choice layout is not claimed as Apple's three-choice
controller. No generated graphic or synthetic animation was added.

The shuffle choices are Off and Songs. An explicit wheel adjustment reorders
through the existing playlist API with `start_current=true`; the setting is
saved only after success. Repeating the current choice does not reorder or
write settings again. Album shuffle and Genius remain unimplemented backend
items, not visible inactive buttons. Podcast chapters/dates and iTunes rating
sync likewise remain outside the implemented native text/control extension.

Lyrics accept same-basename `.lrc` and `.txt` files (UTF-8 or BOM UTF-16) and
ordinary uncompressed ID3v2.3/v2.4 USLT lyrics (Latin-1, UTF-8 or UTF-16).
Reads are limited to 4 KiB of lyric payload and at most 128 ID3 frame headers;
layout is limited to 8 KiB UTF-8 and 256 lines. Long content is truncated to
these limits. The native page strips LRC time tags and supports manual wheel
scrolling. Synchronized lyrics, other formats, extended/unsynchronized ID3
frames and the full lyric search paths remain in the existing Lyrics plugin.
Whitespace-only or unavailable lyrics are skipped. Information displays the
cached comment, without additional media-file or database parsing.

All new image preparation and text I/O is outside painting. The cover cache
is separate from playback artwork and does not call allocation, artwork-slot,
PCM, codec or mixer ownership APIs. The added native BSS is 48,128 bytes,
including the 32,768-byte note cover, 2,517 bytes of new RGA controls and bounded
text/parser storage. No framebuffer is added. The ARM Select frame remains
984 local bytes plus 16 saved-register bytes; text loading adds a 292-byte
local frame plus 36 saved-register bytes and layout uses 40 saved bytes.

Validation performed:

- 53 focused tests pass, including executed C for artwork readiness, seek
  boundaries and paused state, shuffle failure handling, and bounded Unicode
  lyric parsing; the test harness runs with undefined-behavior sanitization.
- Native iPod 6G, native iPod Video and iPod 6G simulator builds pass.
- Native text/data/BSS: 2,945,852 / 11,032 / 9,024,516 bytes (before:
  2,941,384 / 11,032 / 8,976,388).
- `tools/ipodjs_wps_retail_sim_regression.sh` verifies original note pixels
  against the private RGA at initial entry and after cycling, all supported
  pages, both seek directions while paused, shuffle selection, native lyrics,
  unchanged track/playlist identity and return to the source list.
- Navigation gate: 2,418 trace records, ten hierarchy cycles, twenty rapid
  Albums/Artists cycles, the same `/Music/test.mp3` and 1/1 playlist; process
  file descriptors are 17 at baseline and each of ten cycles. Core counters
  remain zero during playback, so this verifies no downward trend, not native
  free-memory headroom. Captures are in `/tmp/ipodjs-wps-navigation`.
- Final focused captures are in `/tmp/ipodjs-wps-retail-final`.

The simulator checks use the existing button gates with SDL dummy audio/video
through `tools/ipodjs_sim_headless.sh`. Set `IPODJS_WPS_HEADLESS=1` and
`IPODJS_NAVIGATION_SOURCE_ROOT` to a complete one-track fixture containing
`Music/test.mp3` for the focused WPS gate. The capture fixture explicitly sets
`backlight timeout: on`: Rockbox's `off` disables the backlight/LCD and is not
an “always on” setting. This is test configuration only. Desktop X11 runs
stalled before Home here; headless tests exercised the same firmware paths.
Physical audio, hardware storage timing and device testing remain unverified.
On 2026-09-12, the tested firmware was deployed to the mounted iPod 6G using
`tools/deploy_ipod6g_preserve_database.sh` with a focused package. Both root
and `.rockbox` firmware copies match SHA-256
`c5ffd89720fc7742b599a521f39a4448207f62454fd8dee9e20a6319befbca18`.
All 11 database files remained byte-identical, all 2,929 indexed tracks
validated, and disk sync completed. Other configuration was preserved with
tagcache autoupdate enabled. Rollback files and the deployment log are in
`/tmp/ipodjs-wps-deploy-50b9dl5n`. Reboot and physical playback testing remain
pending.

The separate long-Center compatibility gate also passed with the actual freshly
built `lrcplayer.rock` installed in the isolated fixture: 273 trace records,
unchanged track and 1/1 playlist, short Center remaining in Lyrics, and Menu
returning to the source song list. Captures and trace are in
`/tmp/ipodjs-wps-lyrics-compatibility-final`. A first fixture attempt lacked
that plugin; its nominal script pass was rejected and the test rerun with the
plugin present.

## Follow-up correction (2026-09-12)

Removed the equalizer page and its WPS animation refresh path at the user’s
request. Seeking uses the empty source rail with the original diamond and
reflection; normal playback keeps its filled progress bar. The diamond now
centres on the full rail range instead of being inset by half its width at
each endpoint. No new pixels, asset cache, allocation or seek backend was
introduced. Exact controller geometry remains reference-derived rather than
a recovered Apple executable implementation.

Validation for this correction: 40 focused tests pass, including executed C
for empty-rail drawing and diamond endpoint alignment. The focused simulator
gate verifies page order `[1,2,3,4,0]`, paused seeking in both directions,
shuffle, Lyrics, source return and exact resource-6 cover pixels. Native 6G,
native 5G and simulator builds succeed. Native 6G text shrinks by 400 bytes;
data/BSS are unchanged (11,032 / 9,024,516 bytes). A broader legacy source
suite has 12 failures, all reproduced against the pre-correction source;
several still require superseded assets or playback-risking core allocation.
They were not changed to conceal the failures. Captures are in
`/tmp/ipodjs-wps-correction`; focused test output is in
`/tmp/ipodjs-wps-correction-focused.log`.

The corrected headless navigation gate also passed: 2,418 trace records, ten
full-depth cycles, twenty rapid Albums/Artists switches, and the same track
and 1/1 playlist. Open file descriptors remain 17 through all ten cycles.
Captures and trace: `/tmp/ipodjs-wps-correction-nav-final`. The first navigation
fixture had its backlight disabled; it was corrected to `backlight timeout:
on` in the isolated runtime and rerun successfully.

The correction was deployed to both iPod 6G firmware locations with the
database-preserving script. Both copies match SHA-256
`461947370b221e55be690a93068e85a2d9357dff9a25d1a347a58c54ebb093a8`.
All 11 database files remained byte-identical and all 2,929 indexed tracks
validated. Other settings were unchanged; autoupdate remains enabled.
Inventory update and disk sync completed. Rollback files and deployment log:
`/tmp/ipodjs-wps-correction-deploy-mosu55lf`. Device reboot/playback validation
remains pending.
