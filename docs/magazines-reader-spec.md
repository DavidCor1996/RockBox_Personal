# Rockbox Magazines Reader Specification

## Scope

Magazines is an offline, page-image reader for 320x240 colour iPods. It is
launched from `Extras -> Magazines`; PDFs are converted and synced by RockPod
or `tools/magazine_prepare.py`. PDF parsing never occurs on the device.

## Reading Model

- Opening Magazines first shows a category browser. Selecting a category opens
  its six-cover shelf; Menu returns to categories, then to Extras.
- The top-level selection screen has `Categories` and `Bookmarks` tabs.
  Left/Right changes tabs and the wheel moves through the active tab.
- The front cover and back cover are single-page views.
- Interior content is shown as two-page spreads.
- The click wheel changes spreads.
- Left opens the left page and Right opens the right page in zoom view.
- Select opens the left page when no side has been chosen.
- Zoom starts at 200%. The wheel continuously changes magnification in bounded
  steps from 100% page-fit through 500%; repeated wheel input moves faster.
- In zoom view, Left/Right pan horizontally and Menu/Play pan vertically.
  Holding Menu+Select exits zoom and returns to the unchanged spread.
- The position map remains visible while panning and reports the exact zoom
  percentage.
- Page turns use a bounded 200 ms curl animation and coalesce queued wheel
  pulses into one turn.

## Progress Scrubber And Bookmarks

Play opens the progress scrubber without decoding or reading another page.
The current spread remains cached behind the overlay.

- Wheel: move one spread.
- Repeating wheel gesture: move five spreads.
- Left/Right: jump to the previous or next bookmark, wrapping at the ends.
- Play: add or remove a bookmark at the highlighted spread.
- Select: decode and open the highlighted spread.
- Menu: cancel and return to the current spread.

The scrubber displays the highlighted page range, percentage, progress bar,
and bookmark markers. The selected range sits in a blue focus panel; an
existing bookmark adds a gold outline and changes the action text to
`Play: remove`. Adding or removing a bookmark displays an explicit
confirmation. Each issue supports eight bookmarks.

Reader state remains schema 1:

```text
issue.<id>=<page>|<completed>|<opened-sequence>|<bookmark-pages>
```

Bookmark pages are comma-separated spread starts. An empty fourth field and
the former value `0` both mean no bookmarks, preserving compatibility with
existing state files.

The `Bookmarks` tab flattens saved bookmarks into issue-and-page rows. Selecting
a row opens that issue directly at the bookmarked spread. Holding Select for
half a second opens a page-specific deletion confirmation; confirming removes
and persists the bookmark, clamps the selection to the remaining rows, and
refreshes the tab. Locked issue titles and bookmark pages are excluded until
the session has already passed the shared PIN prompt. An empty tab shows
`No saved bookmarks` without creating a placeholder issue.

## Page-Turn Animation

Page turns use the already-decoded current and adjacent spreads. A 14-frame,
250 ms eased curl progressively reveals the destination, adds a moving cast
shadow, curves and vertically tightens the folding edge, highlights the paper
crest, and retains a subtle centre-spine shadow. The animation allocates no
new framebuffer, performs no page I/O, and continues to observe USB/system
events between frames. Queued click-wheel pulses are coalesced after the turn.

## Categories And Locks

RockPod owns the editable category model. Every issue is assigned to exactly
one category; older issues and deleted-category members fall back to
`Uncategorized`. Category names are limited to 48 display characters.
Magazine Sync can create a category while assigning selected magazines,
rename it, delete it with reassignment to `Uncategorized`, and bulk lock or
unlock selected issues.

The top-level device category browser lists only categories containing
unlocked issues. All locked issues are collapsed into one `Locked:` row:
locked category names, cover thumbnails, titles, and progress are not loaded
or displayed before authentication. Selecting `Locked:` opens the same Apple
surface-backed click-wheel four-digit prompt used by the main-menu Settings
lock. A correct PIN reveals a nested list of the actual locked category names;
selecting one then loads its shelf. Authorization lasts only for that plugin
session.

Category rows use the installed 18-pixel bold Helvetica font, a dark continuous
wood field, light text, and gold selected text. They do not use per-row
selection slabs. Shelves use 68x88 cover targets and thin separators instead
of dark bars behind titles.

Magazines intentionally shares Netflix/Locked Videos PIN storage:

```text
/.rockbox/videolist/locked.pin
```

RockPod requires a valid four-digit PIN before syncing locked content and
writes it atomically. It does not delete the shared PIN when magazines are
unlocked or removed because videos may still depend on it.

## Prepared Issue Format

Each issue directory contains:

```text
cover.jpg
cover-pane.jpg
issue.mgi
pages/0001.jpg
pages/0002.jpg
...
```

Required manifest fields remain `schema`, `id`, `title`, and `page_count`.
New preparation metadata is advisory and ignored by older readers:

```text
prepare_profile=standard
crop_margins=0
prepared_width=480
prepared_height=640
category=Uncategorized
locked=0
preview=cover-pane.jpg
```

Two host preparation profiles are supported:

| Profile | Page limit | JPEG quality | Margin crop | Use |
| --- | ---: | ---: | --- | --- |
| Standard | 480x640 | 86 | No | Photos and storage efficiency |
| Fine Text | 720x960 | 90 | Yes | Small print and dense layouts |

Fine Text trims near-white outer margins before fitting and centering the page
on a 720x960 paper-coloured canvas. It consumes more storage and preparation
time but gives zoom level 2 more source detail.

`cover.jpg` is the fast 68x88 shelf image. `cover-pane.jpg` is a 348x480,
quality-92 derivative used by the iPodJS Extras right-pane slideshow. Keeping
the pane derivative separate prevents a 58- or 68-pixel shelf thumbnail from
being enlarged across half the display.

## Store Tab: Internet Archive Public-Domain PDFs

A "Magazines" tab in RockPod's Store (`ui/main_window.py`'s `store_tabs`
`QTabWidget`, alongside Music/Movies/Comics/Live TV/Calm/iPod Games)
searches and downloads from the Internet Archive's `magazine_rack`
collection — a dedicated periodicals collection, not a bare PDF search
(Archive.org auto-generates a "Text PDF" derivative for nearly any
text-mediatype item, so PDF format alone would be far too broad to mean
"magazine").

The original design trusted Archive.org's self-reported `licenseurl` field
alone, scoped to the broad `collection:magazine_rack` (an open, largely
uncurated bucket). A live search for "national geographic" surfaced a
currently-published, actively-copyrighted 2020 issue an uploader had
falsely tagged CC0 — proof `licenseurl` alone is spoofable, and `magazine_
rack` isn't curated enough to trust as a scope on its own.

Filtering now requires four independent signals together, all server-side
in one `advancedsearch.php` call: `collection:pulpmagazinearchive` (a
genuinely curated periodicals collection — verified live to exclude the
fraudulent National Geographic item that `magazine_rack` let through);
`format:("Text PDF" OR "Additional Text PDF")` (the "correct file type" —
a bare PDF-format check alone is too broad on its own, since Archive.org
auto-generates this derivative for nearly any text-mediatype item);
`year:[1800 TO 1963]` (the actual US copyright-renewal cutoff — see
`docs/comics-manga-spec.md` for why year alone isn't sufficient either);
and `licenseurl:(*publicdomain* OR *creativecommons*)`. Combined, a live
"national geographic" search dropped from several actively-copyrighted
false positives to zero results. This is an automated heuristic, not a
legal determination, which is why results also show cover art in a grid
for a human sanity check. Only the cover thumbnail is fetched per search
result; download fetches exactly one selected PDF file per item, nothing
more.

- `rockpod/services/magazine_store.py`: `ArchiveOrgMagazineClient`, the
  same request-object pattern as `ArchiveOrgComicsClient`.
- `rockpod/scripts/magazine_store_search.py` / `magazine_store_download.py`:
  CLI search/download scripts run out-of-process, mirroring the Comics
  Store scripts exactly.
- `rockpod/ui/magazine_store_panel.py`: `MagazineStorePanel`, mirroring
  `ComicsStorePanel`'s signal/method contract.
- `main_window.py` wiring downloads into a staging directory, then calls
  `RockboxMagazineService.import_pdf()` to register it in the library
  (dedup by sha256, slugified issue id) before deleting the staged copy —
  the same staging pattern used for the Comics Store.

## Resource Boundaries

- The plugin uses `plugin_get_buffer()`, never the playback audio buffer.
- Zoom retains a single completed page in one decoder workspace sized for a
  720x960 page. Continuous scaling renders through the existing page-turn
  composition buffer; no duplicate page copy or additional framebuffer is
  allocated, and wheel zoom never re-decodes the JPEG.
- The scrubber paints only cached pixels and fixed geometry. Page I/O occurs
  only after Select confirms a jump.
- Bookmarks live in the existing issue state allocation; no dynamic
  allocation is performed while reading.
- Categories use fixed arrays for at most 32 categories and 64 issue indices.
  The PIN prompt caches the three shipped iPodJS Apple surfaces in about 44 KiB
  of the plugin arena before its draw loop. No cover or page is decoded before
  successful authorization.
- USB, shutdown, and default system events remain observable in reader,
  scrubber, animation, and zoom loops.

## Acceptance Criteria

- Existing 480x640 issues and schema-1 state files still open.
- Cover/back and interior spread rules remain unchanged.
- Wheel scrubbing reaches the first and last spread without invalid pages.
- Bookmark add, remove, wraparound navigation, persistence, and the eight-item
  limit work.
- The Bookmarks tab lists only real saved bookmarks, opens the selected spread,
  survives recent/title re-sorting, and reveals no locked issue metadata before
  authorization.
- Forward and backward curls begin and end on exact cached spread pixels,
  remain within the 250 ms budget, and handle cover/back single pages.
- Selecting a scrub target refreshes adjacent-page caches and resume state.
- Continuous 100%-500% zoom and four-direction panning work with Standard and
  Fine Text pages, preserve the focal point while scaling, and clamp at every
  page edge.
- Menu alone pans upward, Play pans downward, and Menu+Select exits zoom
  without changing the current spread.
- Category assignment, rename, deletion/reassignment, and re-preparation
  persistence work in RockPod.
- Locked sync fails before copying without a valid PIN; the correct PIN opens
  a locked issue and an incorrect PIN reveals no page.
- Simulator, iPod 6G, and iPod Video plugin builds pass.
- RockPod tests cover profile selection and preparation command propagation.
- Preparation tests cover both manifest profiles.
