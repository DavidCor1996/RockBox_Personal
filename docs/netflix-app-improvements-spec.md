# Netflix App Improvements Spec

This is an addendum to `docs/netflix-ipod-app-spec.md`. That document describes
a target architecture (`apps/gui/ipodjs_netflix.c`, a versioned
`/.rockbox/rockpod/netflix/library.tsv` bundle) that was never built. This
addendum describes the Netflix app **as it actually exists** and specifies eight
changes on top of it. Nothing here migrates the app toward the unbuilt
architecture; that remains a separate, optional piece of work.

The desktop1080 application is specified separately in
`docs/netflix-desktop-mode-app-spec.md`. It consumes the same Video Sync
manifest and players but is compiled as a host-only plugin, so none of its
window, mouse, or 1080p behavior changes this regular iPod screen.

## Actual Architecture

```text
RockPod (host)
  rockpod/ui/main_window.py            metadata match UI and context menus
  rockpod/ui/dialogs/video_metadata_lookup.py
  rockpod/services/online_video_metadata.py   iTunes / OMDb / TVmaze providers
  rockpod/services/video_thumbnails.py        poster render + manifest writer
  rockpod/services/sync_engine.py             manifest row assembly + sync
        |
        |  /.rockbox/videolist/index.tsv  (tab separated, versioned header)
        |  /.rockbox/videolist/netflix/<video_id>.bmp          28x42 list
        |  /.rockbox/videolist/netflix-detail/<video_id>.bmp   96x144 detail
        |  /.rockbox/videolist/netflix-landing/<video_id>.bmp  72x108 carousel
        v
iPod (Rockbox)
  apps/root_menu.c                     the whole Netflix catalog lives here
        |
        |  filetype_load_plugin("mpegplayer" | "openh264_player",
        |                       "netflix:<path>" | "netflix-restart:<path>")
        v
  apps/plugins/mpegplayer/*            playback, OSD, resume
```

The Netflix appearance is a *skin* over the existing Videos browser, selected by
`global_settings.ui_engine_video_appearance == UI_ENGINE_VIDEO_NETFLIX`
(`videos_netflix_appearance()`). It reuses `struct video_browser_state`, the
virtual-directory scanner (`videos_scan_virtual_dir()`), and the manifest
parser. It is not a separate screen module.

Static Netflix brand assets live under `/.rockbox/ipodjs/netflix/` and are
prepared by `tools/prepare_netflix_launch_assets.sh` and
`tools/prepare_netflix_video_launch_assets.sh` from real source media recorded
in `assets/ipodjs/sources/netflix/*/SOURCES.tsv`.

### Manifest

`/.rockbox/videolist/index.tsv` is written by
`VideoThumbnailService.export_video_list_manifest()`. Header line 1 is a
version comment, line 2 is the column header. The device parser reads columns
positionally through three helpers in `root_menu.c`:

| Helper | Columns |
| --- | --- |
| `video_parse_manifest_line()` | 0-11, tolerates an 11-column legacy row |
| `video_parse_manifest_line_v4()` | 0-19 |
| `video_parse_manifest_line_v5()` | 0-21 |

`video_split_tsv()` does not require a trailing tab on the last requested
field, so a v4 parse of a longer row still yields correct columns 0-19. Adding
columns at the end is therefore backward compatible; **inserting or reordering
columns is not**, and any new column must be appended.

## Confirmed Defects

These were verified against a real synced manifest
(`.rockpod-backups/device-videolist-with-tpb-movie-20260724.tsv`, 20 rows),
not inferred:

1. Every `My Name Is Earl` episode has an empty `plot_short` and `plot_long`.
   `main_window.py::_match_video_metadata_for::on_matched` takes a
   `if is_group_match: pass` branch that writes identity, genre and year but
   deliberately writes no plot at all.
2. `Recess S03E29 "Space Cadet"` has no `netflix_poster`, no `netflix_detail`,
   no `show_art_id` and no `season_art_id` - it renders as the neutral
   text-only fallback with no way to recover on device.
3. `Trailer Park Boys - The Movie` carries `year=1999` (the film is 2006) and
   no plot, because a movie row can only ever be matched against the provider's
   movie search, and misclassified or unmatched movie rows never get there.
4. `Recess` rows use `season` values `4` and `03`, so string grouping splits
   what should be one season list.
5. `content_rating` is empty for every row on the device.
6. `VIDEO_LIST_NETFLIX_RED` is `LCD_RGBPACK(229, 9, 20)` (`#E50914`, the modern
   ribbon-era red). The dominant colour measured across every red pixel of the
   shipped period wordmark, `netflix-logo-2001.150x70x24.bmp`, is `#B4131D`.
   The top bar and the iPodJS home right pane therefore do not match the logo
   sitting on top of them.
7. In `osd_refresh()` a forced refresh issued while the OSD is hidden runs
   `osd_show(OSD_SHOW | OSD_NODRAW); hint = OSD_REFRESH_ALL;`. The stock
   control path then evaluates `volume_changed = (hint == OSD_REFRESH_VOLUME)`,
   which is false, while `progress_changed` is true - so a volume press paints
   the header band and progress strip, which time out and are cleared again.
   That is the flash. It is chrome, not the volume card. Live TV never hits it
   because `livetv_overlay_show()` / `livetv_overlay_hide()` own their own clip
   rectangle and five-second timer and bypass the hint machinery entirely.
   The current mitigation, an early return in `osd_set_volume()` whenever
   `osd.stock_layout && osd_stream_status() == STREAM_PLAYING`, suppresses the
   volume card during playback instead of fixing the promotion.
8. `stream_on_ev_complete()` sets `stream_mgr.resume_time = 0` on natural end of
   stream ("Played to end - no resume"). A completed title is therefore
   indistinguishable from a never-played one in `mpegplayer.dat`, so a watched
   state cannot be derived from resume data and needs its own record.

## Changes

### 1. Brand red

`VIDEO_LIST_NETFLIX_RED` becomes the measured `#B4131D`
(`LCD_RGBPACK(180, 19, 29)`) and `VIDEO_LIST_NETFLIX_RED_DARK` becomes a
derived shade of it, `#700C12` (`LCD_RGBPACK(112, 12, 18)`). Both the landing
top bar and the iPodJS home right pane already draw with this single constant,
so both correct together, as do the detail header, progress fill and Resume
badge. The value is measured from the shipped asset, never recalled.

### 2. Home resume title card

On the Netflix landing screen the resume entry (index 0 at depth 0) renders as
a wide card instead of the 96x144 centred poster:

- card spans the content width with the poster inset on the left;
- title, progress percentage and metadata sit to its right;
- two action pills, `RESUME` and `PLAY FROM BEGINNING`, stack under the text;
- the neighbouring carousel posters are not drawn while the wide card is up,
  because the card occupies their columns.

Input: the two pills are extra stops in the existing horizontal carousel, so no
new input mode is introduced. Wheel forward on pill 0 moves to pill 1; wheel
forward on pill 1 leaves the resume entry for the next title. Wheel back
mirrors that. Select activates the focused pill. This is the documented
resolution of the wheel conflict between "choose an action" and "choose a
title"; the alternative, a modal focus mode, would need a second input verb the
click wheel does not have.

### 3. Watched checkmark

Completion is persisted separately from resume, because resume is cleared at
EOF (defect 8).

- `/.rockbox/videolist/netflix-watched.tsv` holds one device path per line.
- `mpegplayer` appends the current path when the button loop exits because the
  stream stopped on its own - i.e. end of stream, not a user stop - and only
  when launched through a `netflix:` / `netflix-restart:` parameter. The file
  is opened, appended and closed on the exit path; no PCM, mixer or buffer API
  is touched, so `docs/plugin-audio-lifecycle-steering.md` is unaffected.
- Duplicate lines are collapsed by the writer, and the file is bounded to
  `VIDEO_WATCHED_MAX` entries; the oldest are dropped first.
- `openh264_player` writes the same record. It is not optional there: both
  `raw_resume_save()` (past 95%) and `raw_resume_clear()` (end of stream)
  *delete* the RVP resume record, so a finished RVP title leaves nothing
  behind to infer completion from either. The catalog therefore reads
  completion only from this shared list.
- `root_menu.c` loads the set once per scan into a bounded table of path CRCs,
  never from a draw callback, and draws a checkmark badge on the poster in the
  landing carousel and on the detail poster.

Badge asset: `/.rockbox/ipodjs/netflix/watched.16x16x24.bmp`, produced by
`tools/prepare_netflix_watched_badge.sh` by compositing a real glyph from an
installed font onto a disc filled with the measured brand red. It is generated,
not hand-drawn, and is recorded in
`assets/ipodjs/sources/netflix/badges/SOURCES.tsv` following the same
convention as the existing category art.

### 4. Descriptions for every show

- The group-match branch in `on_matched` writes the matched show's
  `plot_short` / `plot_long` to every episode that does not already have its
  own, and never overwrites a per-episode plot.
- A show-level synopsis column, `show_plot`, is appended to the manifest
  (making it v6) so a show or season row can show a description even when no
  episode is selected. Appending keeps every existing positional parser valid.
- `root_menu.c` shows that description on the landing screen for show and
  season directory entries.

### 5. Movie metadata match

`VideoMetadataService.search()` gains an `any` media type that merges movie and
show results, and the lookup dialog gains a Type selector defaulting to `All`.
A movie result selected for a row currently classified as a show already
rewrites `video_kind` to `movie` in `on_matched`, so a misclassified title is
repairable from the same dialog.

### 6. Cover and metadata correctness

- `season` and `episode` are normalised to base-10 integers with no leading
  zeros when the manifest row is assembled, fixing the `03` / `3` split.
- `content_rating` is exported from the matched provider result.
- `_fallback_video_manifest_entry()` rows, which exist for physical files with
  no library row, are still emitted, but the exporter records that they carry
  no artwork so the neutral text fallback is intentional rather than silent.
- Live TV is out of scope for all of the above. `/Videos/LiveTV` content is
  produced by `rockpod/services/livetv.py` on its own schedule and is not part
  of the video manifest; none of these changes read or write it.

### 7. Playback overlay and silent volume

Netflix playback no longer uses the stock full-screen/WPS chrome. Its controls
follow the stable Live TV lifecycle:

- the decoder is clipped above one bounded bottom strip while controls are
  visible;
- the strip is painted black with the measured Netflix red accent, white
  title/time text, playback state, and a red progress fill;
- hiding controls removes the clip and asks the decoder to repaint the frame;
- volume actions update `SOUND_VOLUME` and return without showing either the
  controls strip or the stock volume card.

The RVP player applies the same Netflix palette to its paused controls and also
changes Netflix volume silently. Non-Netflix MPEG and RVP playback retain their
existing stock volume feedback.

### 8. Stale frame after an action

The Netflix landing screen repainted once per action and called
`display->update()`, but a single update did not reliably reach the panel.
A focus-only change or a level change therefore left the previous frame on
screen: entering `Movies` appeared to do nothing, and the resume card's pill
focus never visibly moved even though the state had changed.

Every action-driven repaint now schedules one follow-up repaint on the next
idle tick, consumed by the existing idle branch. `ACTION_NONE` is excluded so
it cannot re-arm itself. The extra pass repaints cached pixels only and never
re-enters the artwork service point, so it adds no storage access.

## Measured Results

- `rockbox.elf` BSS on `ipod6g`: 4 890 504 -> 4 893 032 bytes, a delta of
  **2 528 bytes (2.5 KiB)** against the 120 KiB ceiling. It is the 16x16 badge
  (512 B), the 256-entry watched CRC table (1 KiB), the landing synopsis
  buffer (192 B), and bitmap bookkeeping.
- Worst-case manifest rows remain bounded below the device's 1024-byte parser
  limit. Version 7 appends four compact playback-marker columns after the
  original 23 columns; older positional fields never move.
- `ipod6g` and `ipodvideo` hardware builds and the `ipod6g` simulator all
  build; `tools/netflix_app_launch_sim_gate.py` and
  `tools/netflix_video_resume_sim_gate.py` both pass.
- Netflix MPEG controls use a clipped bottom strip, so the decoder never
  races the controls for the same pixels. Volume changes do not draw or clear
  any rectangle.

## Constraints

All of `docs/ipodjs-ui-memory-animation-steering.md` applies to the
`root_menu.c` work: draw functions paint cached pixels only, artwork decoding
stays at screen entry or in the bounded idle service point, and new fixed
buffers are reported as a BSS delta. All of
`docs/plugin-audio-lifecycle-steering.md` applies to the `mpegplayer` work.

Real assets only. Covers come from RockPod's existing artwork resolution or
fall back to the neutral text frame; no cover is invented on the device.

## Acceptance

- Landing top bar, detail header and iPodJS right pane read the same red as the
  wordmark drawn on them.
- The resume card shows both actions and the wheel reaches each, then leaves.
- A title played to the end shows a checkmark on next entry to the catalog, and
  a title stopped midway does not.
- Every show and episode on the device has a non-empty description.
- A movie row can be matched from RockPod and its year, plot and poster update.
- Season lists are not split by leading-zero season numbers.
- Changing volume during Netflix MPEG or RVP playback changes audio without
  drawing an overlay.
- Netflix MPEG controls use the clipped Live TV overlay lifecycle and the
  Netflix black/red/white palette.
- Live TV playback, guide and channel change are unchanged.
- Simulator and both hardware targets build.

## Unrelated Break Fixed In Passing

`livetv_step_channel()` was declared in `livetv.h` and called from
`livetv_change_channel()` in `mpegplayer.c`, but no definition existed
anywhere in the tree or in its git history, so `mpegplayer.rock` could not
link once a stale object was rebuilt. It is implemented here to the contract
its header comment states - step `delta` channels, wrap at both ends, skip
channels with nothing on the air, bounded to one pass over the lineup - purely
so the tree builds. Live TV behaviour is otherwise untouched and remains out
of scope for this work.
