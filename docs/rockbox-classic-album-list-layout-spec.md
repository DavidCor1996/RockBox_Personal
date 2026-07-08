# Rockbox Classic Album List Layout Spec

Scope: make Rockbox's database album list match the stock iPod classic 7G
full-screen list treatment while keeping this repo's current iPone/Rockbox color
scheme. The preferred repo outcome is full-screen album browsing with artwork
that is not smaller than the stock-feeling 7G artwork surfaces, while other
menus/lists remain compact or half-screen.

This is for the normal album list, not Cover Flow.

## Feasibility Conclusion

This is feasible.

The local code already has the right hook points:

- `apps/tree.c` initializes Rockbox database lists through `gui_synclist`.
- `apps/gui/list.c` and `apps/gui/bitmap/list.c` own row height, selection, and
  list drawing.
- `apps/gui/albumlist_art.c` already detects tagcache album rows and installs a
  custom draw callback.
- `apps/tagtree.c` exposes `tagtree_get_album_art_row()` so the list renderer
  can tell when the current database view is an album list.
- RockPod already generates `.rockbox/albumlist/index.tsv` and 32x32 album
  thumbnails for the current album-list-art path.

The important caveat: the current 32x32 thumbnail album-list mode looks too
small for the desired 7G-style album browsing feel. The preferred path is not a
huge artwork-row redesign; it is a modest thumbnail increase with a slightly
lower visible row count.

## Online Research

Reference material checked:

- Apple Support Community thread with attached iPod classic screenshots of the
  Genre -> Artist -> Albums flow:
  <https://discussions.apple.com/thread/252675162>
- TidBITS 2007 iPod classic coverage and marketing imagery:
  <https://tidbits.com/2007/09/10/apple-introduces-ipod-touch-wi-fi-itunes-store-and-new-ipods/>
- iPod classic display/background specs:
  <https://en.wikipedia.org/wiki/IPod_Classic>

Findings from the stock screenshots:

- The album list is a plain text list, not a thumbnail grid and not Cover Flow.
- The screen is 320x240.
- A thin top status/title bar remains visible.
- The album list uses the full screen width rather than the split/half-screen
  menu layout used by the stock main menu.
- The selected row is a single horizontal highlight bar.
- The list is dense, with roughly 9 to 10 visible rows depending on title/status
  area and font metrics.
- Album names are left aligned, bold/dark, and clipped/truncated rather than
  wrapped.
- The checked reference album-list screenshot does not show album art in the
  rows. Other 7G playback and preview surfaces use much larger art than the
  current 32x32 Rockbox album-list thumbnails, so a repo-specific visual album
  list should target larger art instead of preserving the current tiny
  thumbnail size.
- Some rows, such as "All Albums", can show a right chevron when they enter a
  deeper view.

The user requirement modifies the visual target:

- Match stock layout, spacing, row density, and list behavior.
- Use the stock-like full-screen album-list layout only for album browsing.
- Keep other menu/list screens in their existing compact or half-screen layout.
- Keep this repo's current color scheme. Do not switch to Apple's white
  background and bright blue selector.
- Album browsing should show artwork a bit larger than the current 32x32
  thumbnails. A lower visible album count is acceptable as long as the list
  still feels compact and scrollable.

## Current Rockbox Differences

### Current iPone theme/list behavior

`themes/iPone.cfg` currently uses:

- dark background: `100F16`;
- light foreground: `F7F4FA`;
- gradient selector from `2B2234` to `9D7AE6`;
- selector text color `FCF9FF`;
- separator color `1A1621`;
- scrollbar off;
- statusbar off;
- font: `18-Cantarell-Bold.fnt`;
- list separator height 1.

This color language should remain.

### Current list geometry

`apps/gui/list.c` sets default list row height from the active UI font. For
non-touchscreen targets, row height normally equals the font height.

The same file currently adds a small default list top inset:

- `LIST_TOP_INSET_MAIN` is 5.
- Default list viewports move down by 5 pixels and shrink by 5 pixels.

For iPone, the SBS handles title/status-like text separately. That means the
generic list often uses most of the display area, with the theme's dark colors
and full-width selector style.

There is also an existing iClassic-specific viewport branch in
`gui_synclist_set_viewport_defaults()` that forces a left-pane menu viewport
when the SBS filename contains `iClassic`:

- `x = 0`;
- `y = 16`;
- `width = 144`;
- `height = 224`.

That left-pane behavior is appropriate for stock-like main menus and other
compact list surfaces, but it should not apply to the stock-style album list.
The album list should be full screen like the stock 7G album browser, while
other screens/lists keep their compact or half-screen presentation.

### Current album-list-art path

`apps/gui/albumlist_art.c` adds thumbnail rows when the current tagtree view is
an album list:

- thumbnail size is 32x32;
- text padding is 6;
- row height is forced to at least 34 pixels;
- a small lookup cache maps album/artist to `.rockbox/albumlist/index.tsv`;
- an 8-entry bitmap cache stores loaded thumbnails.

This is useful for a visual album browser, but it is the main blocker for both
possible targets:

- 235 px available list height / 34 px rows = about 6 visible rows.
- Stock Classic album list shows roughly 9 to 10 rows.
- 32x32 thumbnails are also smaller than the desired stock-feeling artwork
  scale, so they look cramped when the goal is a richer visual album browser.

## Preferred Outcome

When browsing Rockbox Database -> Albums on the iPod Video / 5G target using
the iPone theme:

- the album list uses a full-screen list viewport, not a split/half-screen menu
  pane;
- the album list uses either compact stock-like text rows or a larger-art visual
  row mode;
- compact text mode shows about 9 to 10 album rows on the 320x240 screen;
- larger-art visual mode shows fewer rows, but the art is modestly larger than
  the current 32x32 thumbnails;
- the title/header area remains stable and does not reduce the list to a tiny
  viewport;
- the selected row is a single full-width horizontal highlight;
- text is left aligned with a small inset;
- long album names scroll or truncate through the existing Rockbox selected-row
  behavior;
- no album thumbnails are drawn in compact text mode;
- current iPone colors, selector gradient, and font family remain the default;
- thumbnail album-list mode remains available separately, with a larger-art
  profile for this repo's preferred visual album browser.

Other lists should not inherit this viewport:

- root menu/main menu can remain half-screen/compact when the active theme uses
  that layout;
- Settings, Files, Plugins, and normal database category lists should keep their
  current viewport behavior;
- only the album-list database level should get the full-screen 7G-style list.

## Non-Goals

- Do not implement Cover Flow.
- Do not change the global Rockbox list style for every menu.
- Do not copy Apple's white background or exact blue selector.
- Do not remove the existing RockPod album-list thumbnail export.
- Do not break voice/talk menu behavior.
- Do not require a real iPod for first validation.

## Proposed Design

### 1. Add an album-list display mode

Add an explicit album-list display mode, preferably a theme/config setting:

- `album list style: classic compact`
- `album list style: classic art rows`
- `album list style: thumbnails legacy`
- `album list style: default`

Initial default for iPone should be `classic art rows`, with a modest thumbnail
size increase over 32x32. `classic compact` should remain available if the user
wants maximum row density.

Rationale:

- It avoids surprising users who like the thumbnail mode.
- It keeps the Classic match scoped to album browsing.
- It avoids changing every Rockbox list row globally.

This mode must include both row geometry and viewport behavior. "Classic
compact" means a compact row height inside a full-screen album-list viewport,
not a half-screen main-menu pane.

### 2. Rework `albumlist_art_setup_list()`

Rename or split the current album-list setup into a more general helper:

```c
void albumlist_setup_list(struct gui_synclist *list);
```

Behavior:

- Detect album-list rows using the existing `tagtree_get_album_art_row()`.
- If style is `thumbnails`, keep the existing 32x32 thumbnail callback.
- If style is `classic art rows`, use a larger thumbnail target than 32x32,
  apply a full-screen album-list viewport, and tune row height around the chosen
  art size.
- If style is `classic compact`, install a compact callback or set compact row
  geometry and use default text drawing.
- If style is `default`, leave the normal Rockbox list unchanged.

This is less invasive than changing tagtree itself.

### 3. Compact row geometry

Target geometry for 320x240:

- viewport: full width, with only the normal top title/status allowance;
- row height: 22 px first target;
- visible rows: 10 when the list viewport is about 220 px high;
- left text inset: 4 to 6 px;
- right padding: 6 px;
- no icon column for album rows unless a submenu chevron is implemented.

Why 22 px:

- It is close to the stock screenshot density.
- It gives 9 to 10 rows with a thin header/status area.
- It fits the current 18 px iPone font with breathing room.

If simulator screenshots show the 18 px bold font makes the row too tall or too
heavy, test one of the existing smaller fonts:

- `16-Adobe-Helvetica.fnt`;
- `16-Adobe-Helvetica-Bold.fnt`;
- `15-Adobe-Helvetica.fnt`;
- `14-Nimbus.fnt`.

Font changes should be theme-scoped. Do not shrink every menu unless the user
accepts the broader visual change.

### 3a. Larger-art row geometry

If album art remains visible in Database -> Albums, do not keep the current
32x32 thumbnail size as the target.

Prototype sizes:

- 40x40: conservative first step; about 5 visible rows plus title/status;
- 44x44: preferred first target; a clear bump without making rows feel huge;
- 48x48: upper first-pass candidate if 44x44 still feels too small.

The implementation should measure against screenshots before picking a default.
The selected row height should be the art size plus 4 to 6 px padding. Text
should sit beside the cover with album title first, and optional artist/count
metadata only if it does not make the row feel crowded.

RockPod must export matching album-list thumbnails for the selected size rather
than only `_ALBUM_LIST_THUMB_SIZE = (32, 32)`.

### 4. Keep current colors

The compact album list should keep these iPone theme values:

- current background;
- current foreground;
- current selector gradient;
- current selector text;
- current separator color.

The stock Classic target is layout-only:

- full-screen album-list viewport;
- row count;
- row height;
- text placement;
- selected-row shape;
- header/list proportions.

### 5. Optional chevron support

Stock Classic rows use a right chevron for rows that enter another screen.

Rockbox currently uses generic icons/cursors differently. Chevron support should
be optional and only added after the compact row height is validated.

Possible implementation:

- add a tiny right chevron asset or draw a primitive chevron;
- only draw it for album-list rows that enter a child table, such as
  "All Albums" or album entries if they open track lists;
- keep it inside the row's right padding and avoid reducing text width too much.

This is polish, not a blocker for the first layout match.

### 6. Thumbnail mode stays separate

Keep current `.rockbox/albumlist/` thumbnail generation and rendering for a
future "visual album list" mode.

For Classic compact:

- do not load thumbnails;
- do not read the albumlist manifest during drawing;
- do not force row height to 34 px;
- do not add thumbnail text indentation.

This also improves list scroll performance for large album libraries.

For Classic art rows:

- load only pre-sized BMP thumbnails generated by RockPod;
- prefer a size-specific album-list manifest or include thumbnail dimensions in
  the manifest;
- keep cache size bounded because larger thumbnails consume more RAM;
- measure scroll smoothness in simulator before making it default.

### 7. Keep compact viewports for non-album lists

Do not globally remove existing compact/half-screen list behavior. The
full-screen override should be guarded by album-list detection.

Implementation options:

- add an album-list viewport override inside the album-list setup helper;
- add a `gui_synclist` flag for "full-screen album list" and apply it in
  `gui_synclist_set_viewport_defaults()`;
- or add a tree/database-specific parent viewport only when
  `tagtree_get_album_art_row()` proves the current view is an album list.

The guard should be strict enough that Artists, Genres, Files, Settings,
Plugins, and root menu lists keep their existing compact behavior.

## Implementation Plan

### Phase 1: Spec validation screenshot

1. Capture current Rockbox/iPone album-list screenshot in simulator.
2. Save alongside the stock reference notes.
3. Count:
   - visible rows;
   - row height;
   - title/header height;
   - left inset;
   - selector height.

### Phase 2: Compact album-list mode

1. Add a setting or theme-controlled flag for album-list style.
2. Rename/split `albumlist_art_setup_list()` into a generic album-list setup.
3. Add a compact setup path:
   - disable album thumbnails;
   - force a full-screen album-list viewport for this album level only;
   - force album-list row height to 22 px on 320x240 iPone/iPod Video;
   - preserve current selector/color behavior.
4. Add a Classic art-row setup path:
   - use full-screen album-list viewport for this album level only;
   - test 40, 44, and 48 px art sizes;
   - update RockPod album-list thumbnail export for the selected size;
   - preserve current selector/color behavior.
5. Keep the existing 32x32 thumbnail code behind a legacy thumbnail style.

### Phase 3: Theme tuning

1. Test with current `18-Cantarell-Bold.fnt`.
2. If it feels too heavy compared with stock, test a theme-local 15/16 px
   Helvetica/Nimbus font option.
3. Keep color values from `themes/iPone.cfg`.
4. Tune list top inset only for album-list compact mode if needed.

### Phase 4: Optional chevrons

1. Add primitive or bitmap right chevron only if screenshots show it is needed.
2. Ensure long album titles still have enough room.
3. Verify RTL behavior or disable chevron in RTL until tested.

### Phase 5: RockPod support

No RockPod change is required for compact text mode.

Optional later work:

- expose an iPone device-profile setting for album-list style;
- keep exporting album-list thumbnails only when thumbnail style is enabled;
- avoid planning `.rockbox/albumlist/thumbs` files for compact-only users.

## Validation

Simulator target:

```bash
tools/simulator_first_gate.sh --target ipodvideo --skip-build --theme-tests --smoke --manual-checklist
```

Manual checks:

- Open Database -> Albums.
- Confirm Database -> Albums uses a full-screen list viewport.
- Confirm compact album list shows about 9 to 10 visible rows.
- Confirm no album thumbnails appear in compact mode.
- Confirm Classic art-row mode shows album art larger than the current 32x32
  thumbnails and does not clip text.
- Confirm current iPone colors remain unchanged.
- Confirm selected row uses the current iPone selector gradient.
- Confirm long album names scroll/truncate cleanly.
- Confirm Artists -> Album -> Tracks still navigates correctly.
- Confirm root menu/main menu and other compact list screens remain half-screen
  or compact where the active theme currently makes them compact.
- Confirm Files, Settings, Plugins, and non-album database lists are not forced
  into the full-screen album-list viewport or compact album-list row height.
- Confirm thumbnail mode still works when enabled.

Screenshot checks:

- capture before and after screenshots of Database -> Albums;
- capture one long-album-title case;
- capture one "All Albums" or submenu row case;
- compare visible-row count against the stock reference.

## Acceptance Criteria

- Compact album list on 320x240 shows 9 to 10 visible album rows.
- Classic art-row album list uses modestly larger thumbnails than 32x32,
  preferably around 44x44 if simulator screenshots confirm the balance.
- Compact album list uses the full 320px screen width, apart from intentional
  margins/status/title allowances.
- Other lists retain their existing compact or half-screen viewport behavior.
- The album list does not draw 32x32 thumbnails in compact mode.
- Current iPone/Rockbox color scheme is preserved.
- Selection row remains full-width and visually consistent with the theme.
- Album browsing navigation behavior is unchanged.
- Voice/talk menu behavior is unchanged.
- Thumbnail album-list mode remains available or can be re-enabled.
- Simulator screenshot evidence exists before any hardware test.

## Risks

- Stock screenshots are external references and not pixel-perfect technical
  documents. Treat dimensions as measured targets, then tune by simulator
  screenshots.
- Forcing row height and full-screen viewport only in album lists requires
  careful detection; if the detection is too broad it could affect Artists,
  Genres, root menu, Settings, or file browser lists.
- Smaller fonts may improve density but affect the wider iPone theme if applied
  globally.
- Chevron polish can create clipping or RTL issues; keep it optional.
- Removing thumbnails from the default album list may feel like a regression to
  users who prefer visual browsing. Keep modes separate.
- Larger thumbnails reduce visible row count and increase bitmap cache memory,
  but a 40-48 px target keeps the risk modest. Measure scroll performance before
  locking the default.

## Open Questions

- Should compact album-list style be the default for iPone only, or for all
  iPod Video themes?
- Should the full-screen album-list override apply to iClassic themes too, while
  leaving their main/root menu left-pane layout intact?
- Should RockPod stop exporting album-list thumbnails when compact mode is the
  selected device profile?
- What modest art size best matches the desired 7G feel: 40, 44, 48, or another
  measured value from stock screenshots?
- Should a 15/16 px font be used only for database album lists, or should the
  existing 18 px font remain for visual consistency?
- Should the stock-style chevron be added in the first implementation or left
  for a polish pass?
