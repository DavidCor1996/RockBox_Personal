# RockPod Spotify Wrapped

## Product contract

`Spotify Wrapped` is an offline, personal listening-history application for
the iPod Classic and iPod Video. It has no Spotify account, network transport,
streaming, or service API. It presents the iPod's own library history using
the real 2024 Wrapped card language while retaining ordinary iPod click-wheel
navigation and an authentic classic Spotify application icon.

The application appears in both `Extras -> Spotify Wrapped` and the iPodJS
`Applications` grid. Its stories preview the current scope's number-one song
through Rockbox's normal decoder, including the user's ReplayGain and DSP
settings. It never claims the plugin audio buffer or replaces the user's
playlist.

## History sources

RockPod publishes an atomic `data.tsv` snapshot after connection and after
sync. This avoids parsing target-width tagcache rows inside a simulator plugin,
keeps displayed values identical to the host's verified database reader, and
prepares three 104x104 BMPs from the winning tracks' actual `cover.jpg` files.
No invented cover is installed.

The snapshot is derived from two history sources:

1. **This Year** is calculated from Rockbox's local `/.rockbox/playback.log`
   and rotated `playback_####.log` files. A listen qualifies after 30 seconds,
   or the complete duration for shorter tracks. The timestamped log supports
   listening minutes, top tracks/artists/albums/genres, favourite month, and
   favourite listening hour.
2. **All Time** is calculated from the tagcache runtime fields `playcount` and
   `playtime`. It works immediately for listening that pre-dates the playback
   log, but cannot claim a calendar year because legacy tagcache values carry
   only a monotonically increasing last-played serial.

Playback logging defaults to enabled on the custom iPod build. Existing device
configuration is explicitly migrated to `play log: on` during the physical
deploy. Runtime data gathering remains enabled, so each finished track updates
tagcache play count, play time, and last played state as before.

The snapshot also records the exact local path of the number-one song. Wrapped
temporarily disables runtime, autoresume, and playback-log gathering only for
its own preview. Opening the recap therefore cannot inflate the winning song's
future rank. The settings are restored before ordinary user playback resumes.

## Top-song preview lifecycle

Wrapped starts the number-one song at 0:00 and repeats it for as long as the
stories remain open. If the saved volume is louder than -18 dB, the app applies
that temporary ceiling; quieter settings are never raised, and a stricter user
volume limit still wins. The exact saved volume is restored on exit.

The preview is a queued entry immediately after the current item, not a new or
replacement playlist. Wrapped records the prior playlist index, file, elapsed
position, byte offset, pause state, repeat mode, and volume. On exit it removes
the queued preview and resumes that state. If playback was initially stopped,
it returns to stopped playback; if the playlist was initially empty, it is
empty again. When music is active at entry, Wrapped reuses the loaded UI font
rather than loading a new glyph cache that could shrink playback memory.

## Stories

Left and Right advance through story cards. Each card has a bold palette,
progress segment, short editorial reveal, and fixed-buffer pixel entrance.
The experience has:

- **Intro** — explicitly labels annual or all-time scope;
- **Minutes** — minutes, hours, and completed plays;
- **Top Song** and **Top 5 Songs** — real cover, artist, and play count;
- **Top Artist** and **Top 5 Artists** — real cover, minutes, and plays;
- **Top Album** and **Top 5 Albums** — real cover, artist, minutes, and plays;
- **Top Genres** — five ranked genres;
- **Your Mix** — unique tracks, artists, albums, and genres;
- **Your Type** — a transparent local personality based on listening
  concentration, never a fabricated global percentile;
- **Recap** — top artist, song, and album share card;
- **Artist Message** — only appears when RockPod found and installed a verified
  Spotify Wrapped video greeting for the actual all-time number-one artist.

When a year has no timestamped log records, the Overview truthfully says that
annual logging begins now and continues to display All Time results. It does
not fabricate a year from aggregate data.

## Artist message media

RockPod may install an artist-specific, user-authorised public video at:

```text
/.rockbox/spotify-wrapped/artist-message.tsv
```

The tab-separated record contains:

```text
artist<TAB>title<TAB>video_path<TAB>source_url
```

The app exposes the message only when its `artist` matches the actual all-time
top artist and the local video exists. Select dispatches the file to the
existing MPEG player; the player owns audio lifecycle and restores the normal
Rockbox path on exit. No remote media URL is ever exposed to the iPod.

RockPod searches YouTube from the connected host for the actual all-time top
artist. A candidate must explicitly identify Spotify Wrapped, use artist-message
language, identify the artist, and originate from either that artist's channel
or Spotify's channel. Reaction, compilation, tutorial, template, parody, and
fan-made results are rejected. A match is converted to the normal iPod MPEG
profile, atomically copied, and recorded in the manifest. If the completed
search has no high-confidence match, RockPod removes any stale message and the
iPod omits the Artist Message tab entirely. Network/search errors preserve the
last verified message rather than deleting it.

## Visual and memory contract

The recap follows Spotify's official 2024 press-kit visual system: sequential
progress bars, saturated flat yellow/coral/lime/cyan/violet cards, black or
cream high-contrast type, stepped pixel edge geometry, oversized personalized
numbers, concise reveal copy, ranked cards, and prominent real artwork. The
old generated record-collage background is intentionally unused. The authentic
Spotify header mark and host-prepared winning covers are decoded once at
startup into fixed buffers; drawing never opens or decodes a file. Packaged
14px, 18px, and 35px fonts make typography deterministic and readable.

The Applications grid uses authentic Spotify iOS application artwork extracted
from Spotify 1.8.1's iOS 6 package. It replaces the unsuitable 2009 horizontal
press wordmark, whose outlined lettering became rough and illegible at 46px.

The plugin has fixed rank tables and line buffers only. It scans logs and
tagcache before the interactive draw loop, yielding periodically. Draw paths
only paint already-built results; they perform no file I/O, tagcache query,
allocation, or media decode. No full-screen cache, `core_alloc()`, or playback
memory is used.

## Verification

- confirm `gather runtime data: on` and `play log: on` in the mounted config;
- play a tagged track for more than 30 seconds, finish or skip it, then verify
  both a tagcache count increase and a timestamped playback-log entry;
- launch Wrapped from stopped playback and from active Database/Files playback;
  verify the top song starts at 0:00, loops, and the original track, elapsed
  time, pause state, repeat mode, volume, and playlist count return on exit;
- launch above -18 dB and verify the preview is capped, then verify the exact
  louder setting returns on exit;
- remain in Wrapped for more than 30 seconds and verify its preview creates no
  playback-log row and no tagcache runtime increment;
- render every story at 320x240 in the iPod 6G simulator and inspect it at
  native resolution;
- build the iPod 6G plugin and copy only the plugin, snapshot, and generated
  hero covers required by this feature.
