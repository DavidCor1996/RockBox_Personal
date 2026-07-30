# Netflix for Desktop Mode

Status: implemented for the `desktop1080` host profile.

## Scope

Desktop Netflix is a separate `netflix_desktop.rock` plugin launched from the
1080p Snow Leopard Dock. It is not compiled for the 320x240 hardware or
simulator profiles and does not change the regular iPodJS Netflix screen.

The application is a window by default: the real desktop remains visible
outside a centered Snow Leopard window. Playback takes over the complete
1920x1080 display and contains the video without changing its aspect ratio.
Bars, when a source is not 16:9, occupy only the unused area outside the video.

## Video Sync-only data contract

The only catalogue is:

```
/.rockbox/videolist/index.tsv
```

The app consumes the Video Sync title, kind, path, show, season, episode, year,
genre, plot, lock state, poster, and watched-state fields. Relative media paths
resolve from the mounted iPod root. Poster paths resolve from
`/.rockbox/videolist` and fall back to the shipped Netflix category art.

There is no demo or hardcoded title fallback. A missing manifest produces an
empty catalogue. Rows are rejected if their path, kind, or grouping identifies:

- `/Videos/LiveTV` or another Live TV path;
- `/Videos/Downloaded`, `/YouTube`, or a YouTube kind/group;
- a generic `/Live` source.

This filtering is a second line of defense around the Video Sync manifest, not
an integration with either excluded application.

## Window and controls

The default window is `897x671` at `y=170`, leaving the Aurora desktop, menu
bar, mounted-iPod icons, and Dock visible. Its application surface has:

- Home, Movies, TV Shows, Music Videos, and Home Videos tabs;
- the selected title's poster, metadata, description, and watched state;
- a Play or Locked button;
- a five-poster carousel with previous and next controls;
- a status line identifying the Video Sync-only source.

The host pointer can click the close control, every category tab, every visible
poster, previous/next, and Play. Tabs, cards, arrows, and Play show hover
feedback. Click-wheel/keyboard navigation remains available. Labels use
foreground-only text rendering so no opaque background strip crosses the
window chrome, navigation bar, buttons, or status bar.

## Playback

Play hands the actual synchronized path to the existing registered player:

- MPEG, MPG, MPV, and M2V use `mpegplayer.rock`;
- RVP and H264 use `openh264_player.rock`.

The parameter remains `netflix:<absolute device path>`, preserving the
existing Netflix intro, resume, controls, and watched-state behavior.
On desktop1080, both players scale the source to the largest centered
1920x1080 rectangle that preserves aspect ratio. Exiting playback returns
through the existing plugin chain.

## Memory and lifecycle

The catalogue is bounded to 96 rows. The app obtains one arena from
`plugin_get_buffer()` for the exact desktop underlay, exact window bitmap,
logo, five `96x144` cached posters, and one `192x288` selected-poster cache.
Drawing performs no file reads; bitmap loads happen at entry or after an
explicit category/selection action.

The window does not call `plugin_get_audio_buffer()`, `audio_stop()`, or
`core_alloc()`. Playback ownership remains entirely with MPEGPlayer or
OpenH264 Player under their existing audio lifecycle.

## Acceptance gate

`tools/netflix_desktop_mode_sim_gate.py` creates a four-row manifest with two
Video Sync titles, one YouTube/Downloaded row, and one Live TV row. It verifies:

- only the two Video Sync rows enter the catalogue;
- the desktop remains visible outside the default window;
- pointer clicks operate the Netflix Dock icon and Play;
- the selected path is handed to MPEGPlayer with the Netflix prefix;
- the playing frame replaces the window and scales to the 1080p display.
