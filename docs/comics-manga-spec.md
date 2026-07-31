# Rockbox Comics/Manga Reader Specification

## Scope

Comics is an offline, page-image reader for 320x240 colour iPods, forked from
the Magazines engine (`docs/magazines-reader-spec.md`) and sharing its
on-device rendering, page-turn, zoom, bookmark, and lock mechanics. It is
launched from `Extras -> Comics`. Source material is CBZ/CBR archives
imported and converted by RockPod (`rockpod/services/rockbox_comics.py`) or
`tools/comics_prepare.py`. CBZ/CBR/RAR/ZIP parsing never occurs on the
device — the plugin only ever opens plain JPEG page files.

Comics differs from Magazines in three respects: source format (CBZ/CBR
image archives instead of PDF), a per-issue reading direction (left-to-right
for Western comics, right-to-left for manga), and a Store tab that searches
and imports public-domain comics from the Internet Archive. Everything else
— shelf browsing, categories, locks, bookmarks, the scrubber, and the
page-turn animation — is the same engine and the same on-device contract.

## Reading Model

- Opening Comics first shows a category browser, identical in structure to
  Magazines' (`Categories` / `Bookmarks` tabs, Left/Right switches tabs,
  Menu backs out one level at a time to Extras).
- The front cover and back cover are single-page views; interior content is
  shown as two-page spreads, exactly as in Magazines.
- **Reading direction** is per-issue, read from the `reading_direction`
  manifest field (`ltr` or `rtl`, default `ltr`). In `rtl` issues the wheel
  and Left/Right page-turn semantics are mirrored: the wheel that would
  normally advance to the next (rightward) spread instead advances to the
  spread that is physically to the left, matching how a printed manga volume
  is read back-to-front. Zoom panning direction is unaffected — panning
  always follows the physical screen axes.
- Zoom, panning, and the 200 ms curl page-turn animation are unchanged from
  Magazines (see `docs/magazines-reader-spec.md` "Page-Turn Animation").

## Progress Scrubber And Bookmarks

Unchanged from Magazines. Reader state remains schema 1, persisted at
`PLUGIN_APPS_DATA_DIR "/comics.cfg"` (a separate file from `magazines.cfg`
so reading progress in one app never collides with the other):

```text
issue.<id>=<page>|<completed>|<opened-sequence>|<bookmark-pages>
```

## Categories And Locks

RockPod owns the editable category model, identically to Magazines. Comics
intentionally shares the same Netflix/Locked-Videos/Magazines PIN storage:

```text
/.rockbox/videolist/locked.pin
```

A single PIN unlocks locked Magazines, Comics, and Videos alike within one
plugin session. RockPod requires a valid four-digit PIN before syncing
locked comics and never deletes the shared PIN on unlock, since other
content types may still depend on it.

## Prepared Issue Format

Each issue directory under `/Comics/<issue-id>/` contains:

```text
cover.jpg
cover-pane.jpg
issue.mgi
pages/0001.jpg
pages/0002.jpg
...
```

Required manifest fields remain `schema`, `id`, `title`, and `page_count`,
identical to Magazines. New fields specific to Comics:

```text
reading_direction=ltr
source_format=cbz
prepare_profile=standard
category=Uncategorized
locked=0
preview=cover-pane.jpg
```

`reading_direction` is `ltr` or `rtl`; `source_format` records whether the
original archive was `cbz` or `cbr`, kept for troubleshooting only — the
device never re-reads the archive. The `standard` and `fine-text` page
profiles from Magazines apply unchanged (480x640 / 720x960).

## CBZ/CBR Import And Preparation (RockPod)

`rockpod/services/rockbox_comics.py` provides `RockboxComicService`, a
sibling of `RockboxMagazineService` with the same shape:

- `import_archive(source_path)` accepts a `.cbz`/`.zip` or `.cbr`/`.rar` file,
  validates it actually contains orderable raster images (`.jpg`, `.jpeg`,
  `.png`, `.gif`, `.webp` — anything else inside the archive, e.g. `.xml`
  ComicInfo sidecar files, is ignored for page ordering but `ComicInfo.xml`
  is read opportunistically for `title`/`series`/`manga` hints if present),
  dedupes by size+sha256 against the existing library, and copies it into
  `<library_root>/archives/<issue-id>.<cbz|cbr>`.
- `preparation_command(issue_id, profile, reading_direction)` builds the
  argv for `tools/comics_prepare.py`, which extracts the archive (Python
  `zipfile` for CBZ; the `unrar` or `7z` CLI, resolved via the same
  `require_command()` helper Magazines uses, for CBR — never a bundled RAR
  decompressor), sorts extracted images by natural filename order, and
  reuses Magazines' existing ImageMagick-based page-fit/crop/JPEG-quality
  pipeline to produce `pages/NNNN.jpg`, `cover.jpg`, and `cover-pane.jpg`
  from the first extracted page.
- `sync_issues()` / `remove_issues()` / category and lock management /
  catalog regeneration are identical in behavior to
  `RockboxMagazineService`, targeting `<mount>/Comics` instead of
  `<mount>/Magazines`.

## Store Tab: Internet Archive Public-Domain Comics

A new "Comics" tab in RockPod's Store (`ui/main_window.py`'s `store_tabs`
`QTabWidget`, alongside Music/Movies/Live TV/Calm/iPod Games) searches and
downloads from the Internet Archive's comics collection
(`https://archive.org`), which hosts Golden Age and other public-domain
comic scans made freely and legally redistributable by their catalogers.

This is deliberately **not** a scraper for a copyrighted-content mirror
site. The original design trusted Archive.org's self-reported `licenseurl`
field alone, scoped to two collection names (`digitalcomicmuseum`,
`GoldenAgeComics`) that turned out not to actually exist on Archive.org —
unverified at the time, confirmed later by querying them directly (0
results). A live search for "batman" surfaced *Batman Gates of Gotham #4
(2011)*, an actively-copyrighted DC Comics title an uploader had falsely
tagged CC0 to dodge moderation — proof `licenseurl` alone is spoofable.

Three independent signals are now required together, server-side in the
query: `format:("Comic Book RAR" OR "Comic Book ZIP")` (Archive.org's own
derivative-format facet — the "correct file type"); `year:[1800 TO 1963]`
(the actual US copyright-renewal cutoff — pre-1964 works needed an explicit
renewal after 28 years to stay copyrighted, so only works in this window
can have legitimately lapsed into the public domain; a bare year check
alone still isn't safe — multi-decade "collection" bundles tagged with a
single fake early year were found during testing, which is why year is
required *together with* an open licenseurl rather than alone); and
`licenseurl:(*publicdomain* OR *creativecommons*)`. Combined, a live
"batman" search dropped from ~10 actively-copyrighted false positives to a
single unrelated, genuinely old match. This is an automated heuristic, not
a legal determination, which is why every result also shows cover art in a
grid so the human can sanity-check before downloading. No authentication
bypass, DRM circumvention, or unlicensed-content indexing is in scope.

- `rockpod/services/comics_store.py`: `ArchiveOrgComicsClient` with
  `prepare_search(query, limit)` and `prepare_download(identifier)`,
  mirroring the `YoutubeMovieImporter` request-object pattern (build an
  argv + output path, run out-of-process).
- `rockpod/scripts/comics_store_search.py`: one `advancedsearch.php` call
  combining the user's query with the format and licenseurl clauses above,
  writing a JSON array of `{identifier, title, creator, year, cover_url}` to
  `--output`. Only the cover thumbnail is fetched per result — never the
  comic archive itself — keeping search cheap regardless of result count.
- `rockpod/scripts/comics_store_download.py`: fetches the chosen item's
  best available CBZ (Archive.org serves per-item `_comic.cbz` derivatives
  for scanned comics where available; PDF as a fallback, in which case
  `tools/comics_prepare.py`'s Magazines-derived PDF path is reused instead
  of the CBZ path) into the RockPod comics library's `archives/` folder,
  prints `ROCKPOD_COMIC_OUTPUT=<path>` on completion.
- `rockpod/ui/comics_store_panel.py`: `ComicsStorePanel`, mirroring
  `MovieStorePanel`'s `comic_search_requested`/`comic_download_requested`
  signal contract and `set_comic_results`/`begin_comic_download` methods.
- `main_window.py` wiring mirrors the existing Movies-tab
  `_start_movie_browse`/`_start_movie_import` pair exactly
  (`_create_child_process`, output-file/marker-line result handling,
  library rescan on completion).

## Menu And Preview Wiring (Firmware)

Comics gets its own dual root-menu registration, mirroring Magazines:

- `Extras -> Comics` in the classic Rockbox menu
  (`launch_comics_plugin()` + `MENUITEM_FUNCTION`).
- `IPODJS_EXTRAS_COMICS` in the iPodJS touch-UI Extras list, next to
  Magazines.
- `IPODJS_PREVIEW_COMICS` for the iPodJS home-screen Extras preview pane,
  sharing the same locked-PIN gating as the Magazines preview.

## Resource Boundaries

Identical to Magazines (`docs/magazines-reader-spec.md` "Resource
Boundaries") — `plugin_get_buffer()` only, fixed category/issue array
limits, no dynamic allocation while reading, USB/system events observable
throughout.

## Icon And Visual Theme

The Comics plugin uses an existing built-in Rockbox icon as a placeholder
app icon (chosen and documented by the firmware fork) until real,
non-hand-drawn artwork is supplied. Per project convention
(`rockpod/generated/*.prompt.txt`), an asset brief for a real external image
tool is written alongside this spec rather than substituting hand-drawn
pixel art — see `rockpod/generated/comics-app-icon.prompt.txt`. UI chrome
(shelf gradient, header, category rows) is drawn procedurally with the same
primitives Magazines uses, so no bitmap theme assets are required for the
reader itself, only the optional app icon.

## Acceptance Criteria

- Existing Magazines behavior is fully unaffected; `magazines.cfg` and
  `comics.cfg` never cross-read each other's state.
- CBZ and CBR archives both import, extract, and prepare into an identical
  on-device page format.
- `reading_direction=rtl` issues reverse spread-advance semantics without
  affecting zoom panning. **Implemented** in `apps/plugins/comics.c`:
  `comic_parse_manifest()` reads `reading_direction` from `issue.mgi`
  (default `ltr` when the key is absent, for older prepared issues), and a
  single `comic_advance_direction()` seam swaps which physical wheel
  gesture drives `comic_turn_page()`/`comic_scrub_step()` for `rtl`
  issues; the page-turn curl direction and boundary behaviour at page 1
  and the last page follow automatically since the underlying
  `comic_adjacent_spread()` page-number arithmetic is untouched — only
  which physical input selects its `forward` argument changes. Zoom
  panning and the `Left`/`Right` open-page-in-zoom bindings are
  unaffected, per scope. Not in scope for this pass: which page renders
  on the left vs. right half of a two-page spread bitmap
  (`comic_load_spread_bitmap()`) does not itself flip for `rtl` — spreads
  still composite the numerically lower page on the left half. Full
  manga-correct spread layout would additionally touch spread
  compositing, the curl's centre-spine shadow, and the zoom `Left`/`Right`
  page targets, and was left for a follow-up.
- Locked sync fails before copying without a valid shared PIN; the same PIN
  that unlocks Magazines or Videos also unlocks Comics.
- The Store tab only ever lists and downloads Archive.org items with
  public-domain/open rights metadata.
- Simulator, iPod 6G, and iPod Video plugin builds pass.
- RockPod tests cover CBZ/CBR import dedup, both reading directions in the
  prepared manifest, and Store search/download request-shape.
