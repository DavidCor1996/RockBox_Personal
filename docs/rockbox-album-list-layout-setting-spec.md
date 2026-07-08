# Rockbox Album List Layout Setting Spec

## Goal

Add a Theme Settings option that lets the user choose between the recent
full-screen album-art list and the previous compact/themed album list behavior.

The option should make the recent album-list work reversible at runtime without
changing themes, rebuilding, or editing config files by hand.

## User-Facing Setting

Menu path:

`Settings -> Theme Settings -> Album List Layout`

Values:

- `Full`: current recent behavior. Database -> Albums uses the full screen,
  hides the themed side pane, draws 40x40 album covers, uses 44px album rows,
  skips non-album special rows at the top, and shows the title `Albums`.
- `Compact`: previous compact behavior. Database -> Albums keeps the active
  theme/list viewport, does not force the full-screen album-list viewport, and
  does not hide the SBS/right-side pane. It should use normal compact list row
  sizing while still showing small album-art thumbnails beside album rows.

Default:

- `Full` for this repo, because the recent stock-7G-inspired album screen is
  the preferred default.

Config key:

- `album list layout: full`
- `album list layout: compact`

Internal enum:

```c
enum album_list_layout {
    ALBUM_LIST_LAYOUT_FULL = 0,
    ALBUM_LIST_LAYOUT_COMPACT = 1,
};
```

## Implementation Plan

### 1. Add Global Setting

Add `album_list_layout` to `struct user_settings` in `apps/settings.h`.

Add a `CHOICE_SETTING` in `apps/settings_list.c`:

- flag: `F_THEMESETTING`
- cfg name: `album list layout`
- values: `full,compact`
- default: `ALBUM_LIST_LAYOUT_FULL`
- language id: new `LANG_ALBUM_LIST_LAYOUT`
- value talk ids: `LANG_SET_BOOL_YES/NO` is not ideal; prefer dedicated
  language strings `LANG_FULL` and `LANG_COMPACT` if already present, otherwise
  add `LANG_ALBUM_LIST_LAYOUT_FULL` and `LANG_ALBUM_LIST_LAYOUT_COMPACT`.

### 2. Add Theme Menu Item

Add a `MENUITEM_SETTING` in `apps/menus/theme_menu.c` near `show_icons`,
`cursor_style`, and `sep_menu`.

The item should not live under playback or database settings because it changes
the visual layout of a themed list surface, not the tagcache query.

### 3. Gate Album List Setup

Update `apps/gui/albumlist_art.c`:

- Always reset list callbacks and fullscreen state at entry as it does now.
- If the current view is not an album-row tagtree view, keep current behavior.
- If `global_settings.album_list_layout == ALBUM_LIST_LAYOUT_COMPACT`:
  - do not call `gui_synclist_set_fullscreen_albumlist(list, true)`;
  - install only the compact album-art drawer;
  - do not force `show_icons = false`;
  - do not force row height to `ALBUMLIST_ROW_HEIGHT`;
  - do not clamp selection/start item to the first album row;
  - return after the normal reset path.
- If the setting is `Full`, keep the recent behavior:
  - call `gui_synclist_set_fullscreen_albumlist(list, true)`;
  - install `albumlist_art_draw_item`;
  - use 40x40 thumbnails and 44px rows;
  - clamp first visible item to the first album row.

This keeps all full-screen special handling isolated to the album-list mode that
explicitly requests it.

### 4. Preserve Current Full Mode

`Full` must keep the current fixes:

- no purple/right-side themed pane on the album list;
- title row says `Albums`;
- album rows show covers from `.rockbox/albumlist/index.tsv`;
- first visible row is a real album, not `[Random]` or another special row;
- other database root/category screens keep normal iPone layout.

### 5. Restore Compact Mode Semantics

`Compact` should mean “how it behaved before the recent full-screen album-list
commits,” not a new third layout.

Expected compact behavior:

- the active SBS/theme can keep its side panel;
- normal list viewport rules apply;
- album rows show small thumbnails that fit the compact row height;
- normal row height from the selected font/list settings;
- normal show-icons behavior from `global_settings.show_icons`;
- normal tagtree special entries remain visible according to Rockbox’s standard
  database list behavior.

## Tests

### Source Tests

Extend `rockpod/tests/test_album_list_layout_source.py`:

- assert `album_list_layout` exists in `apps/settings.h`;
- assert `album list layout` appears in `apps/settings_list.c`;
- assert `Album List Layout` or `LANG_ALBUM_LIST_LAYOUT` appears in
  `apps/menus/theme_menu.c`;
- assert `albumlist_setup_list()` checks
  `global_settings.album_list_layout`;
- assert compact mode returns before `gui_synclist_set_fullscreen_albumlist`;
- assert full mode still installs `albumlist_art_draw_item`.

### Simulator Screenshots

Add or extend a capture script to produce two proof sets:

- `docs/album-list-layout-shots/full/03-albums-list.png`
- `docs/album-list-layout-shots/compact/03-albums-list.png`

Full expected:

- title `Albums`;
- full-width album list;
- 40x40 cover art visible;
- no purple/right-side pane;
- first row is a real album.

Compact expected:

- title `Albums`;
- current theme viewport/side pane remains visible if the theme uses one;
- small album cover thumbnails remain visible in normal compact rows;
- row density matches normal compact list behavior.

The capture script should explicitly set the config value before launching the
simulator so the proof is deterministic.

### Build/Test Gates

Minimum gates before pushing:

```bash
./rockpod/.venv/bin/python -m pytest rockpod/tests/test_album_list_layout_source.py -q
make -C build-sim-video-5g all
tools/ipone_album_list_capture.sh build-sim-video-5g docs/album-list-layout-shots/full
IPONE_ALBUMLIST_LAYOUT=compact tools/ipone_album_list_capture.sh build-sim-video-5g docs/album-list-layout-shots/compact
make -C build-hw-ipodvideo-5g all
```

If the iPod is mounted and deployment is requested, install only after those
tests pass and do not eject unless explicitly requested.

## Acceptance Criteria

- Theme Settings contains `Album List Layout`.
- Changing the setting persists in `.cfg` as `album list layout`.
- `Full` reproduces the recent full-width cover-list UI.
- `Compact` reproduces the previous compact/theme-controlled album list.
- Switching modes does not require restarting Rockbox.
- Non-album lists are unchanged in both modes.
- WPS/SBS behavior outside the album list is unchanged.
- Simulator proof screenshots exist for both modes.

## Non-Goals

- Do not add Cover Flow.
- Do not change colors.
- Do not add a third mode yet.
- Do not change RockPod artwork export format beyond what the current full mode
  already needs.
- Do not make compact mode the repo default unless explicitly requested later.
