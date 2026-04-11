This directory stages upstream C-Dogs SDL asset sources for the Rockbox `cdogs`
plugin work.

Source:
- Upstream repository: `https://github.com/cxong/cdogs-sdl`
- Local fetch used for this import: `/tmp/cdogs-sdl-assets`

Imported trees:
- `graphics/`
- `missions/`
- `sounds/`
- `music/`
- `data/`
- `doc/`

Purpose:
- Keep the full upstream/open asset set in-repo while the Rockbox plugin is
  being brought toward asset/layout parity.
- Serve as the canonical source for future bitmap/audio conversion passes.

Notes:
- This is a staging/vendor copy, not yet a fully integrated Rockbox asset pack.
- The plugin currently still uses only a small subset of these assets.
