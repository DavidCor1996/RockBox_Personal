# Ant City (Flashpoint) Port Spec

## Goal

Port **Ant City** as a native SWF asset in `apps/plugins/flashplayer` (not a
video/WWE conversion path), matching the behavior of the real Flash archive payload
with a stock iPod-style control feel.

## Asset source

- Flashpoint package:
  - `Data/Games/97db526b-abaf-4421-afab-59cc85a87d73-1656637380169.zip`
  - `content/uploads.ungrounded.net/48000/48291_Burn_their_ass.swf`
- Source metadata:
  - `content.json` -> `{"uniqueId":"97db526b-abaf-4421-afab-59cc85a87d73"}`

## Export pipeline

1. Extract only:
   - `Burn_their_ass.swf`
   - optional `content.json`
2. Normalize output directory:
   - `.rockbox/flash/antcity/antcity.swf`
3. Ship the SWF as-is.

No `.twv/.rvp` conversion and no branch graph rewriting.

## Import tool

Use:

```bash
python3 tools/flashpoint_antcity_import.py \
  --input /home/david/Flashpoint/Data/Games/97db526b-abaf-4421-afab-59cc85a87d73-1656637380169.zip \
  --output ~/.rockbox/sim/simsim/.rockbox/flash/antcity
```

The importer writes:

- `antcity.swf` (copied SWF bytes)
- `antcity.manifest.json` (source hash/id/path metadata for future audits)

## Runtime behavior

- Runtime: `apps/plugins/flashplayer`
- File association remains `.swf` -> `viewers/flashplayer` (already in
  [apps/plugins/viewers.config](/home/david/Documents/RockBox_Personal-master/apps/plugins/viewers.config))

## Stock control profile (Ant City)

This profile is path-detected for `*Burn_their_ass.swf`.

- `MENU`: exit/quit quickly (stock back behavior)
- `SELECT` / `PLAY`: press-and-release click at cursor position (`mouse_down` for both)
- `LEFT/RIGHT`: move cursor on iPod screen
- `SCROLL_FWD / SCROLL_BACK`: move cursor on iPod screen
- `wheel` motion still drives cursor position when available
- `button hold`: plugin exit behavior stays unchanged

`Ant City` is mouse-first; avoid injecting unnecessary keyboard key events for
cursor controls to keep gameplay stable.

The profile is not global. `stickrpg` keeps existing legacy mapping.

## Validation checklist

1. import script succeeds from Flashpoint zip
2. launcher opens `antcity.swf` in flashplayer
3. game starts with real SWF content
4. `MENU` exits consistently
5. `SELECT`/`PLAY` can trigger click-like actions
6. left/right + scroll controls move cursor and send key events
