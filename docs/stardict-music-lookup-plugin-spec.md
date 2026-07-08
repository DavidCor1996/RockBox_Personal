# StarDict Music Lookup Plugin Spec

## Goal

Add a fast offline lookup layer for music knowledge on 320x240 iPods using
StarDict-style indexed dictionaries. The first version should work as a general
dictionary plugin and as a music-aware helper that can look up the current
artist, album, genre, or selected text from Rockbox metadata.

## First Slice

- Add `stardict_lookup.rock` as an application plugin.
- Support read-only StarDict dictionaries installed under:

```text
/.rockbox/dicts/
```

- Load `.ifo`, `.idx`, and `.dict` dictionaries.
- Support uncompressed `.dict` first. Treat `.dict.dz` as a later slice unless a
  small gzip block reader is already available without pulling in heavy code.
- Provide a dictionary picker, search box, prefix results list, and entry viewer.
- Use Rockbox text rendering, list widgets, and file APIs only.
- Keep memory usage bounded by streaming index pages rather than loading large
  dictionaries into plugin RAM.

## Music Mode

Music mode uses the same dictionary backend but offers metadata-driven lookup
actions:

- lookup current artist;
- lookup current album;
- lookup current genre;
- lookup current track title;
- lookup selected word or phrase from a local text/LRC note file.

The plugin should accept an optional launch parameter so future WPS, database,
file browser, or RockPod flows can open directly into a query:

```text
stardict_lookup.rock artist:"Cocteau Twins"
stardict_lookup.rock album:"Heaven or Las Vegas"
stardict_lookup.rock genre:"shoegaze"
```

If launched without a parameter, it opens the normal search UI.

## Dictionary Profiles

The plugin should not assume every dictionary is a word dictionary. A small
sidecar profile file allows RockPod or users to label the dictionary:

```text
/.rockbox/dicts/music-artists.profile
```

Profile fields:

- `type`: `word`, `artist`, `album`, `genre`, `theory`, `liner_notes`, or
  `custom`;
- `title`: display name;
- `default_field`: `artist`, `album`, `genre`, `track`, or `word`;
- `casefold`: whether keys should compare case-insensitively;
- `normalize_punctuation`: whether punctuation should be ignored for matching.

Profiles are optional. If missing, the dictionary is treated as a normal word
dictionary.

## Music Dictionary Sources

Rockbox should not fetch data on-device. RockPod or a host-side tool can build
dictionaries from user-provided or public-data exports:

- MusicBrainz artist and release metadata;
- Wikidata/Wikipedia excerpts;
- Discogs exports where the user has rights to use the data;
- user-written liner notes;
- genre glossaries;
- chord, scale, mode, tempo, and production term references;
- sample/source annotations curated by the user.

The on-device plugin only reads the generated dictionary files.

## Index Strategy

The `.idx` file can be large, so the plugin should use a small seek cache:

- scan the index once to build or update a compact `.rbidx` acceleration file;
- store every Nth key with the source offset into `.idx`;
- binary-search `.rbidx`, then linear-scan a bounded range in `.idx`;
- keep only the current result window and current entry text in memory.

Generated acceleration files live beside the dictionary:

```text
/.rockbox/dicts/music-artists.rbidx
```

If `.rbidx` is missing, stale, or corrupt, the plugin rebuilds it. If rebuilding
fails, it falls back to slower linear prefix search.

## Entry Rendering

First slice supports plain text and minimal markup cleanup:

- strip HTML tags conservatively;
- decode common entities;
- wrap text for the active viewport;
- support page up/down, line scroll, and jump to top/bottom;
- show source dictionary and matched key in a compact header.

Later slices may support simple formatting, cross-reference links, images, and
multiple result tabs.

## Controls

Default iPod controls:

- wheel: scroll list or entry;
- select: open result or activate focused action;
- menu: back;
- play/pause: switch between search/results/entry actions;
- left/right: page through entry or move cursor in search input.

The search editor should reuse existing Rockbox text entry where possible.

## RockPod Integration

RockPod should provide a host-side builder for music dictionaries:

- import CSV/TSV/JSON records;
- normalize artist, album, and genre keys;
- generate `.ifo`, `.idx`, `.dict`, and `.profile`;
- optionally prebuild `.rbidx`;
- install dictionaries to `/.rockbox/dicts/`;
- expose recommended bundles such as music theory, genre glossary, and artist
  notes.

RockPod should also support "Lookup metadata on device" by installing a music
dictionary and optional launcher shortcuts.

## Testing

No device deployment should happen until the simulator and hardware gates below
pass. The plugin is read-only for dictionaries, but it can still create slowdowns
through index scans, large entry rendering, or repeated filesystem seeks, so
performance is part of correctness.

### Simulator Gate

- Build and run on the iPod 6G simulator and iPod Video simulator.
- Open an uncompressed dictionary with at least 100k entries.
- Open a stress dictionary with:
  - very long keys;
  - duplicate keys;
  - empty entries;
  - multi-kilobyte entries;
  - mixed ASCII/UTF-8 text;
  - punctuation-heavy artist and album names.
- Verify prefix lookup for first, middle, last, missing, mixed-case, and
  punctuation-heavy keys.
- Verify corrupt `.ifo`, truncated `.idx`, missing `.dict`, and stale `.rbidx`
  fail gracefully and return to the dictionary picker.
- Verify a generated `.rbidx` is invalidated when the dictionary mtime or size
  changes.
- Verify long entries wrap and scroll without allocating the full dictionary.
- Verify launch parameters for artist, album, genre, track, and word lookup.
- Run repeated open/search/back/exit cycles under AddressSanitizer where the
  simulator build supports it.
- Confirm `git diff` shows no dictionary or playback/database files modified
  except expected `.rbidx` cache creation.

### Performance Gate

- Cold open of a 100k-entry dictionary must remain visibly bounded: show a
  progress message if index-cache generation exceeds 250 ms in the simulator.
- Cached prefix lookup should complete within one UI interaction beat on iPod
  simulator targets, with no multi-second blocking scans.
- Entry rendering should stream/wrap incrementally. A single huge entry must not
  freeze input for more than one second without a progress/abort path.
- The plugin must keep a fixed result-window cache and current-entry buffer; it
  must never allocate memory proportional to the full `.idx` or `.dict`.
- Repeated lookups should not increase plugin heap use after returning to the
  search results screen.

### Playback And System Gate

- Start Database playback, open `stardict_lookup.rock`, search, view entries,
  exit, and verify playback resumes/continues according to normal plugin
  behavior.
- Repeat from Files playback.
- Verify the plugin does not call `audio_stop()`, `plugin_get_audio_buffer()`,
  `plugin_release_audio_buffer()`, PCM APIs, mixer APIs, playlist mutation APIs,
  or tagcache write APIs in the first slice.
- Verify volume changes, hold switch behavior, and menu exit still work after
  repeated plugin launches.
- Verify the database and file browser still open after plugin exit without a
  reboot.

### Hardware Gate

- Test on iPod Classic 6G/7G and iPod Video 5G/5.5G before marking the feature
  stable.
- Use three dictionary sizes:
  - tiny smoke dictionary under 100 entries;
  - normal music dictionary around 10k entries;
  - large dictionary at 100k entries or the largest size intended for release.
- Time cold cache creation, cached lookup, result scrolling, and plugin exit.
- Confirm no audible glitch, lockup, or delayed button response during normal
  playback plus lookup.
- Power-cycle after cache generation and verify dictionaries still open.

### Release Blockers

- Any crash, reboot, database playback failure after exit, corrupted `.rbidx`
  loop, unbounded scan on every launch, or multi-second uninterruptible lookup is
  a release blocker.
- Any first-slice code path touching PCM, mixer, playlist mutation, or shared
  audio buffer ownership is a release blocker.

## Later Slices

- `.dict.dz` support.
- Fuzzy matching and alias redirects.
- Multi-dictionary search.
- Cross-reference links between artist, album, and genre dictionaries.
- Optional WPS/database context menu hook.
- Host-side MusicBrainz export pipeline in RockPod.

## Non-Goals

- No network access on-device.
- No playback, PCM, mixer, playlist, or shared audio buffer changes.
- No full HTML renderer.
- No database write-back in the first slice.
- No copyrighted liner notes or encyclopedia dumps bundled by default.
