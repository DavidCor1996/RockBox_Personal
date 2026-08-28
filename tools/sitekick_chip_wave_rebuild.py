#!/usr/bin/env python3
"""Re-render already-packaged iPod-exclusive chips in place.

``sitekick_chip_wave_append.py`` only ever *adds* ids that are missing from
data/chips.v1.tsv, so it is a silent no-op when the art behind an existing
id changes.  This script covers the other case: the chip stays at the same
id, page and icon slot, but its pixels are rebuilt from the current
``build_ipod_exclusive_image`` output.

For every id in the range it

  * re-runs build_ipod_exclusive_image and rewrites chips/<id>.bmp,
  * re-checks assert_stage_fit,
  * updates only the w/h columns of that chip's row in chips.v1.tsv,
    leaving id/slot/z/anchor/page/index/rarity/worn/name alone,
  * repaints that chip's tile in icons/page<N>.bmp at its existing slot,
  * refreshes its source.manifest hash.

chip_count, icon_pages and series.v1.tsv are untouched because no chip is
added or removed.

Usage:
    sitekick_chip_wave_rebuild.py --min-id 980 [--max-id 1049] [--out DIR]
"""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image

from sitekick_package_assets import (
    ICON_PX,
    ICONS_PER_PAGE,
    ICONS_PER_ROW,
    IPOD_EXCLUSIVE_CHIPS,
    IPOD_EXCLUSIVE_SOURCE_DIR,
    assert_stage_fit,
    build_ipod_exclusive_image,
    sha256_of,
    trim,
    write_bmp32,
)

OUT_DEFAULT = (Path(__file__).resolve().parent.parent /
               "assets/ipodjs/rockbox/sitekick")


def rebuild(out_root: Path, min_id: int, max_id: int) -> int:
    data_dir = out_root / "data"
    chips_path = data_dir / "chips.v1.tsv"
    lines = chips_path.read_text().splitlines()
    header, rows = lines[0], lines[1:]
    by_id = {int(row.split("\t", 1)[0]): idx for idx, row in enumerate(rows)}

    specs = [s for s in IPOD_EXCLUSIVE_CHIPS if min_id <= s[0] <= max_id]
    if not specs:
        print(f"no IPOD_EXCLUSIVE_CHIPS in [{min_id}, {max_id}]")
        return 1

    manifest_updates: dict[str, str] = {}
    # page -> list of (slot, tile) so each icon sheet is opened once
    page_edits: dict[int, list[tuple[int, Image.Image]]] = {}

    for (cid, slot, z, ax, ay, rarity, name, kind,
         primary_source) in specs:
        if cid not in by_id:
            raise SystemExit(
                f"chip {cid} is not packaged yet; run "
                f"sitekick_chip_wave_append.py first"
            )
        row = rows[by_id[cid]].split("\t")
        composed = build_ipod_exclusive_image(kind, out_root)
        assert_stage_fit(cid, name, ax, ay, composed.width, composed.height)
        write_bmp32(out_root / "chips" / f"{cid:04d}.bmp", composed)

        was = (int(row[5]), int(row[6]))
        row[5], row[6] = str(composed.width), str(composed.height)
        rows[by_id[cid]] = "\t".join(row)

        tile, _, _ = trim(composed)
        tile.thumbnail((ICON_PX, ICON_PX), Image.LANCZOS)
        canvas = Image.new("RGBA", (ICON_PX, ICON_PX), (0, 0, 0, 0))
        canvas.alpha_composite(
            tile, ((ICON_PX - tile.width) // 2, (ICON_PX - tile.height) // 2)
        )
        page, index = int(row[7]), int(row[8])
        page_edits.setdefault(page, []).append((index, canvas))

        manifest_updates[f"chips/{cid:04d}.bmp"] = sha256_of(
            IPOD_EXCLUSIVE_SOURCE_DIR / primary_source
        )
        note = "" if was == composed.size else f" (was {was[0]}x{was[1]})"
        print(f"  chip {cid} {name!r}: {composed.width}x{composed.height}"
              f"{note} page={page} index={index}")

    for page, edits in sorted(page_edits.items()):
        page_path = out_root / "icons" / f"page{page}.bmp"
        sheet = Image.open(page_path).convert("RGBA")
        for index, tile in edits:
            x = (index % ICONS_PER_ROW) * ICON_PX
            y = (index // ICONS_PER_ROW) * ICON_PX
            # clear the old tile first: alpha_composite would leave the
            # previous art showing through wherever the new art is thinner
            sheet.paste((0, 0, 0, 0), (x, y, x + ICON_PX, y + ICON_PX))
            sheet.alpha_composite(tile, (x, y))
        write_bmp32(page_path, sheet)
        print(f"  repainted {len(edits)} tiles in {page_path.name}")

    chips_path.write_text("\n".join([header] + rows) + "\n")

    manifest_path = out_root / "source.manifest"
    manifest_lines = manifest_path.read_text().rstrip("\n").splitlines()
    for i, line in enumerate(manifest_lines):
        key = line.split("\t", 1)[0]
        if key in manifest_updates:
            manifest_lines[i] = f"{key}\t{manifest_updates.pop(key)}"
    manifest_lines.extend(f"{k}\t{v}" for k, v in
                          sorted(manifest_updates.items()))
    manifest_path.write_text("\n".join(manifest_lines) + "\n")

    assert ICONS_PER_PAGE  # imported for the page arithmetic above
    print(f"rebuilt {len(specs)} chips in {out_root}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", type=Path, default=OUT_DEFAULT)
    ap.add_argument("--min-id", type=int, required=True)
    ap.add_argument("--max-id", type=int, default=10 ** 9)
    args = ap.parse_args()
    return rebuild(args.out, args.min_id, args.max_id)


if __name__ == "__main__":
    raise SystemExit(main())
