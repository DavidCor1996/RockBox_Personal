# Maker Lite authentic private assets

Maker Lite never downloads, commits, or exports a source ROM. Rockpod accepts
a game image selected by the user, verifies its revision, decodes exact local
pixels into `art.mla`, and stores the result below Rockpod's private data root.
Only the private device kit is synchronized.

## Bundled original kit

`zelda-neon-nook-v1` is a separate, original kit shipped with Rockpod. It uses
the Zelda/top-down runtime ruleset to provide a cozy cyberpunk life-sim starter
without copying or impersonating commercial source art. The one-click
installer verifies the committed manifest digests, MLAR and MLAU headers and
CRCs, 240 per-cell provenance rows, 186 catalog parts, and 88 directional
player frames before installing:

```text
assets/maker_lite/neon_nook/pack/
    art.mla
    audio.mla
    kit.mlk
    pack.json
    provenance.tsv
    source-cover.png
```

Its manifest declares `source.type=original-generated`. This route does not
call the authenticated extraction-bundle importer, does not satisfy a missing
Mario/Zelda/Sonic source asset, and is never labeled as ROM-authenticated
material. The source sheets, deterministic pack builder, conversion metadata,
and generated output hashes remain in the repository so the pack can be
reproduced and audited.

## Supported source revisions

| Ruleset | Revision ID | Upstream verification |
| --- | --- | --- |
| Mario / SMW | `smw-us-1.0` | SHA-1 `6b47bb75d16514b6a476aa0c73a683a2a4c18765` |
| Zelda / ALttP | `alttp-us-1.0` | SHA-256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb` |
| Sonic 2 | `sonic2-rev00` | MD5 `8e2c29a1e65111fe2078359e685e7943` |
| Sonic 2 | `sonic2-rev01` | MD5 `9feeb724052c39982d432a7851c98d3e` |
| Sonic 3 | `sonic3-us` | SHA-1 `75e9c4705259d84112b3e697a6c00a0813d47d71` |

The SMW fingerprint comes from
[`snesrev/smw`'s ROM loader](https://github.com/snesrev/smw/blob/main/assets/util.py).
The ALttP fingerprint and ROM-extraction workflow are documented by
[`snesrev/zelda3`](https://github.com/snesrev/zelda3). The Sonic fingerprints
come from
[`sonicretro/s2disasm`'s bit-perfect check](https://github.com/sonicretro/s2disasm/blob/master/chkbitperfect.lua).
The Sonic 3 fingerprint matches the US cartridge image catalogued by
[OpenRetro](https://openretro.org/smd/sonic-the-hedgehog-3/edit) and
[TASVideos](https://tasvideos.org/127G).
SNES verification ignores a recognized 512-byte copier header; the original
file's SHA-256 and its normalized SHA-256 are both recorded in `kit.mlk`.

## Extraction-recipe workflow

For a complete local set, open Maker Lite and select **Install Game Assets**.
Choose the folder containing the owned game images, then the folder containing
the extraction workspaces. Rockpod recursively identifies sources by published
content hash, identifies complete recipes by their manifest, pairs them by
ruleset and revision, shows every match before writing, and installs all
unambiguous pairs. Filenames are never trusted.

The verified US SMW, ALttP, and Sonic 3 revisions have built-in, asset-free
extractors, so they do not require separate recipe workspaces. The extractors
contain only clean-room structure maps. They reconstruct private player
frames, palettes, source terrain/object cells, and a named starter catalog
from the selected, hash-verified image.

SMW reconstructs big-Mario dynamic frames and the original grassland Map16
source library. ALttP reconstructs Link from the original DMA source rows,
multiple background source sets, named Octorok, Keese, chest, and switch
parts, and a paged sprite-source library. Sonic 3 reads original mappings,
DPLCs, uncompressed character art, palettes, and Nemesis-compressed object
streams. It emits full-size Sonic frames, named cork, spikes, ring, spring,
Rhinobot, starpost, monitor, and goal parts, plus every complete 16x16 source
group available in those streams. The source images are never copied.

The Sonic mapping authority is
[`sonicretro/skdisasm`](https://github.com/sonicretro/skdisasm); Nemesis
decoding follows the independently written 0BSD
[`ClownNemesis`](https://github.com/Clownacy/clownnemesis) implementation.
SMW and ALttP structure authority comes from `snesrev/smw` and
`snesrev/zelda3`.

For one kit, select **Import Private Kit**, then choose **Extraction recipe**.
Select the matching source game and a local `maker-lite-extract.json`. The
recipe and every ordinary referenced file must reside in one local extraction
workspace. Paths may not escape that workspace.

The recipe root contains:

```json
{
  "ruleset": "mario",
  "revision_id": "smw-us-1.0",
  "source_sha256": "optional raw-or-normalized SHA-256 pin",
  "player_base": 0,
  "entity_cells": {
    "goal": 14,
    "enemy": 15
  },
  "asset_catalog": [
    {
      "id": "smw-ground",
      "label": "SMW Ground",
      "category": "Terrain",
      "type": "terrain",
      "cell": 40,
      "collision": ["solid"]
    },
    {
      "id": "goomba",
      "label": "Goomba",
      "category": "Enemies",
      "type": "entity",
      "cell": 15,
      "kind": "enemy",
      "params": [16, 16, 96, 0]
    }
  ],
  "animations": {
    "walk": {
      "right": {"frames": [1, 2, 3, 4], "ticks_per_frame": 6},
      "left": {"frames": [1, 2, 3, 4], "ticks_per_frame": 6, "mirror": true},
      "up": {"frames": [5, 6], "ticks_per_frame": 8},
      "down": {"frames": [7, 8], "ticks_per_frame": 8}
    },
    "roll": {"frames": [20, 21, 22, 23], "ticks_per_frame": 3}
  },
  "source_cover": "my-local-cover-scan.png",
  "effects": {
    "jump": "effects/jump.wav"
  },
  "cells": []
}
```

`cells` must contain at least fourteen consecutive player action cells. Each
cell becomes one exact 16x16 atlas entry. The importer supports three decoder
types.

Animation frame lists must contain consecutive cell indexes. An action may use
one definition for every direction, or separate `right`, `down`, `left`, and
`up` definitions. Missing actions and directions fall back to the corresponding
one of the fourteen base action cells. A generic left-facing action is mirrored
losslessly; an explicit direction is not mirrored unless it sets
`"mirror": true`. Rockpod, the shared-core preview, and the native plugin
validate the same bounded 14-action by 4-direction table. The device selects
frames from the 60 Hz simulation tick, so animation timing is deterministic.

The current `MLAR` art format is version 4. Its fixed 64-byte header is followed
by contiguous 16x16 RGB565 cells, 56 little-endian animation records in
action-major `right/down/left/up` order, and a bounded multi-cell player-frame
table. Each animation record stores a start frame, frame count, and ticks per
frame; the high bit of the tick byte requests horizontal mirroring. Each player
frame preserves its original one-to-four by one-to-four cell footprint, signed
anchor offset, transparent cells, and exact 16x16 chunks. This avoids cropping
or scaling a console metasprite to one tile. The payload CRC covers pixels,
animation records, and frame descriptors. Desktop preview, export validation,
simulator fixtures, and the native plugin share the same bounds. They retain
read compatibility with bounded v1-v3 kits.

The creator accepts up to 1,024 authenticated catalog entries and presents
them in searchable 72-part pages with category, pinned, and recent views.
Catalog entities may share one gameplay kind while retaining different
`render_cell` values; dragging Octorok and Keese therefore does not collapse
both objects to the kit's fallback enemy picture.

`RPML` version 3 uses 16-bit terrain-cell references and 16-bit per-entity
render-cell references, both bounded to the resident 1,536-cell `MLAR` atlas.
The native reader remains compatible with version-1 packs (8-bit terrain and
kind-level entity art) and version-2 packs (8-bit terrain plus per-entity
art).

An exact PNG crop:

```json
{
  "name": "player-idle",
  "kind": "png",
  "file": "assets/sprites/player.png",
  "x": 32,
  "y": 16
}
```

A 2x2 set of native SNES 4bpp tiles:

```json
{
  "name": "player-run-1",
  "kind": "snes4bpp",
  "file": "extract/player.4bpp",
  "offset": 0,
  "tiles": [12, 13, 14, 15],
  "palette_file": "extract/player.cgram",
  "palette_offset": 0,
  "transparent_index": 0
}
```

A 2x2 set of native Genesis 4bpp tiles:

```json
{
  "name": "sonic-roll-1",
  "kind": "genesis4bpp",
  "file": "art/uncompressed/sonic.bin",
  "offset": 0,
  "tiles": [
    {"index": 20, "flip_x": false, "flip_y": false},
    {"index": 21},
    {"index": 22},
    {"index": 23}
  ],
  "palette_file": "art/palettes/sonic.bin",
  "palette_offset": 0,
  "transparent_index": 0
}
```

An asset-free recipe may set `file` and/or `palette_file` to the reserved
`"@source"` value. Rockpod reads those bytes directly from the already
verified selected game image. It normalizes a recognized SNES copier header
before applying offsets and never makes a workspace copy:

```json
{
  "name": "source-ground",
  "kind": "snes4bpp",
  "file": "@source",
  "offset": 65536,
  "tiles": [0, 1, 16, 17],
  "palette_file": "@source",
  "palette_offset": 131072,
  "transparent_index": 0
}
```

Offsets in this snippet are schema examples only. A real recipe must pin the
supported revision and use offsets established and reviewed against that
revision.

The importer performs no scaling or interpolation. It only decodes the native
palette/tile representation, maps the declared transparent index, converts to
RGB565, and packs cells in order. Per-cell file, crop, tile, palette, recipe
hash, source hash, and extractor version are written to private provenance.

### Authentic draggable parts catalog

`asset_catalog` is the authoritative Rockpod palette for a private kit. Each
row names one exact cell from the locally extracted atlas. Rockpod shows that
cell as the card thumbnail and drag ghost; it never substitutes a generic,
downloaded, generated, or hand-drawn picture.

Catalog rows use:

- a unique lowercase `id`, display `label`, and switchable `category`;
- `type: "terrain"`, the atlas `cell`, and zero or more validated collision
  names; or
- `type: "entity"`, the atlas `cell`, one native runtime `kind`, and optional
  bounded `params` and `flags`.

Every catalog cell must be in the resident-atlas range `0..1535` and have its
own provenance row. `entity_cells` remains the kit-level fallback for old
projects and objects without an explicit picture. Current projects preserve
the exact catalog cell in each entity's `render_cell`, so two objects that
share one runtime behavior kind can still show and render as distinct
authenticated characters. RPML v3 stores terrain and entity render cells as
little-endian 16-bit values; readers retain the bounded v1/v2 compatibility
paths described above.

Typical categories and labels are supplied by the local extraction recipe, so
they describe the actual pixels the user extracted:

```json
{
  "mario": [
    {"id": "goomba", "label": "Goomba", "category": "Enemies",
     "type": "entity", "cell": 34, "kind": "enemy"}
  ],
  "zelda": [
    {"id": "small-key", "label": "Small Key", "category": "Items",
     "type": "entity", "cell": 36, "kind": "key"}
  ],
  "sonic": [
    {"id": "ring", "label": "Ring", "category": "Items",
     "type": "entity", "cell": 33, "kind": "collectible"}
  ]
}
```

The example is illustrative schema, not bundled game art or a claim about
offsets in a particular ROM. The selected, verified game image and extraction
workspace remain the sole source of commercial pixels.

The original Neon Nook catalog follows the same bounds and per-cell provenance
rules, but its entries are described as original rather than authenticated
commercial characters. Its 187 parts include dedicated house, shop, car, NPC,
and furniture tools used by the gameplay-first starter city. Revision 2 also
records the generated 4x3 material source atlas used for the asphalt, concrete,
grass, soil, sand, wood, water, interior, roof, tile, wall, and fence cells.

The upstream clean-room projects are useful extraction authorities rather
than art downloads. `snesrev/smw` requires an owned US ROM and extracts its
images into `smw_assets.dat`; `snesrev/zelda3` likewise requires the exact US
ROM and emits Link and sprite sheets from that source. `sonicretro/s2disasm`
provides bit-perfect revision checks and native mappings. Maker Lite recipes
may consume locally generated outputs from those projects or use `@source`;
Rockpod does not fetch their repositories or any commercial pixel data during
installation.

The optional effect files must be uncompressed signed 16-bit stereo WAV at
44100 or 48000 Hz and no longer than one second. Cover art is also a
user-selected local file; Rockpod generates the exact 144x108 Steam derivative.

## Prepared-bundle compatibility

The older `kit.json` plus exact `atlas.png` workflow remains available for
existing private kits. It uses the same strict source-revision check and still
requires one provenance row for every cell. It does not make an unverified or
hand-drawn atlas authentic.
