# Dialtone

`Dialtone` is a real Game Boy / Game Boy Color ROM project built for this repo with `GBDK-2020`, aimed first at Rockboy on clickwheel iPods.

## Vertical slice

- One district: `Luma Lane`
- Interiors: apartment, record shop, used tech shop, ramen stand, rooftop booth
- Five neighbors: Mina, Rook, Soma, Iona, Pix
- Tile-by-tile movement tuned for wheel taps instead of action-heavy play
- One daily favor loop with three rotating errands
- Inventory, media collection, decor slots, pause/options/help
- Apartment decoration with poster / lamp / shelf / tank / dock placement
- Gemini-generated startup cards and room screens converted into GB tilemaps
- 16x16-style metasprite actors with walk animation and idle motion for a more RPG-like feel
- Apartment decor still overlays as small GB-native tiles on top of the room background

## Rockboy-first controls

The ROM itself sees Game Boy buttons, but the intended Rockboy iPod 5G feel is:

- `WHEEL` = move
- `SELECT` = Game Boy `A` = confirm / talk
- `LEFT` = Game Boy `B` = pack / previous tab
- `RIGHT` = Game Boy `SELECT` = tunes / next tab
- `MENU` = Game Boy `START` = pause / back

That matches the Rockboy iPod mapping documented in this repo:

- [manual/plugins/rockboy.tex](/home/david/Documents/RockBox_Personal-master/manual/plugins/rockboy.tex:8)
- [apps/plugins/rockboy/sys_rockbox.c](/home/david/Documents/RockBox_Personal-master/apps/plugins/rockboy/sys_rockbox.c:40)

## Build

The Makefile auto-detects the local GBDK bundle at `/tmp/gbdk-4.5.0/gbdk` if present.

```bash
make -C gb/dialtone gb
```

Or provide your own toolchain path:

```bash
make -C gb/dialtone gb GBDK_HOME=/path/to/gbdk
```

Output ROM:

- `gb/dialtone/build/gb/dialtone.gb`

## Current limitation

- The current startup-art build uses `MBC5+RAM+BATT` with autobanked assets to fit the Gemini title cards
- On Rockboy, emulator autosave should stay off so the title sequence appears on each boot
- Save persistence is still not implemented in the gameplay path yet
- Day/task/inventory/decor progression currently lasts for the session only

## Scope decisions for Rockboy

- Screen-based rooms instead of streaming overworld scrolling
- Edge-triggered one-tile movement for clean clickwheel taps
- Full-screen text panels for dialogue and menus instead of dense overlay UI
- Fixed decor slots instead of drag-and-drop furniture placement
- Media collection is implemented as collectible tapes / mini-discs, not streamed music playback
- Conservative cartridge header and startup path until the richer build has been proven on-device

## Future expansion

- More furniture tiles and room themes
- More neighbors and nightly routines
- Additional district screens
- Proper music engine integration with short loops
