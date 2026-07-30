# Calm for Rockpod and iPod

## Scope

This is a personal-use, offline Calm-inspired player for the 320x240 iPod
Classic and Video targets. It is not affiliated with Calm.com, does not sign
in to Calm, scrape Calm, bypass a subscription, or redistribute Calm
catalogue audio. The checked-in app icon is the public App Store icon and is
kept only as a personal-use visual reference/asset.

Rockpod accepts a YouTube URL chosen by the user and invokes `yt-dlp` in
audio-only mode. The iPod never connects to YouTube and never receives video,
thumbnails, comments, or browser data.

## Visual reference

The source of truth is Calm's current public App Store listing:

- app icon: white Calm wordmark over a vertical cyan-to-indigo gradient;
- sampled icon stops: `#3BBEEC`, `#3BA8E8`, `#509AE7`, `#4676E3`,
  `#4E60E2`;
- white type and controls over blue, with dark navy content surfaces;
- primary sections: For You, Sleep, Meditate, Music, and Sounds.

The 320x240 adaptation keeps the stock iPod interaction model: a short status
bar, click-wheel lists, centered selection, Menu to move back one level,
Select to open/play, and Play/Pause to pause or resume. It does not reproduce
phone chrome or touch-only gestures.

## Rockpod

Rockpod exposes:

1. a `Calm Sync` source under Rockbox, listing the local sound library and
   allowing a scoped sync to the connected device or simulator;
2. a `Calm` Store tab with an `Open YouTube` button, URL field, category
   selector, and `Download Sound` action;
3. explicit text that only the audio stream is downloaded.

Downloads land in:

```text
~/.rockpod/cache/calm/library/
```

Each item consists of a Rockbox-playable audio file and a small JSON metadata
sidecar. Rockpod never asks `yt-dlp` for video or a thumbnail. A download
command must include:

```text
--no-playlist --extract-audio --audio-format mp3 --audio-quality 5
--write-info-json --no-write-thumbnail
```

## Sync bundle

Rockpod atomically replaces only this directory:

```text
/.rockbox/rockpod/calm/
    library.tsv
    database.ignore
    sounds/<youtube-id>.mp3
    assets/calm-icon.64x64x24.bmp
```

`library.tsv` is UTF-8 and begins with:

```text
rockpod_calm_v1
```

Following rows are:

```text
title<TAB>category<TAB>duration-seconds<TAB>absolute-device-path<TAB>source-url
```

Tabs/newlines in metadata are collapsed to spaces. Categories are one of
`For You`, `Sleep`, `Meditate`, `Music`, or `Sounds`. Sync uses a staging
directory next to the destination and a rename, so an interrupted copy leaves
the prior bundle readable. Obsolete files are removed only inside the Calm
bundle. The empty `database.ignore` marker makes tagcache skip the entire Calm
tree, so long-form ambience never appears as ordinary Music.

## iPod application

`calm.rock` loads the manifest into a fixed array and loads the 64x64 icon once
on entry into a fixed plugin-owned bitmap buffer. Draw paths do no I/O,
decoding, tagcache work, or core allocation.

Screens:

- Home: Calm gradient, icon/wordmark, “Take a deep breath,” and the five
  sections with synced counts.
- Library: section title and the filtered list of synced sounds.
- Player: title/category, elapsed/total time, progress bar, pause state, and
  simple mountain/wave art drawn from cached primitives.
- Empty/error: clear “Sync sounds with Rockpod” or “Calm Library Unavailable”
  messages.

Selecting a sound is an explicit primary-media request. The app creates a
one-track playlist and asks Rockbox's ordinary playback engine to decode it.
It does not request the plugin audio buffer, drive PCM/mixer callbacks, or
call `audio_stop()` before playback. Menu leaves the player for the library.
Leaving the app stops playback only when the current track belongs to Calm;
unrelated user music is never stopped.

Controls:

- Wheel: move selection; in the player, change volume.
- Select: open section/start sound; in the player, pause/resume.
- Play/Pause: pause/resume.
- Menu: player -> library -> home -> Applications.
- Hold/USB/power: handled by the standard plugin event path.

## Resource budget

- manifest: 96 entries, approximately 48 KiB in plugin BSS;
- icon: 64x64 24-bit, 12 KiB fixed plugin BSS;
- no framebuffer copy, core allocation, shared audio buffer, or dynamic
  artwork cache;
- one manifest open and one icon decode at startup only.

## Validation

Automated:

- service tests prove the `yt-dlp` command is audio-only;
- malformed metadata is sanitized;
- sync output is deterministic and scoped;
- the simulator plugin build succeeds.

Simulator:

- no-library and corrupt-library screens;
- all five categories and long-title clipping;
- play/pause, volume, Menu unwind, and USB exit;
- start a normal Database/Files track, enter Calm without selecting an item,
  and confirm playback is untouched;
- explicitly select a Calm sound, exit, then start Database and Files music;
- repeat app entry/exit and category navigation without descriptor or memory
  growth.
