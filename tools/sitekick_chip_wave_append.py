#!/usr/bin/env python3
"""Append any not-yet-packaged IPOD_EXCLUSIVE_CHIPS entries.

The canonical packager (``sitekick_package_assets.py``) rebuilds the whole
catalogue from a clone of the SitekickRemastered/Art Unity repo, which this
tree does not have locally. Every RockPod-only chip wave only touches the
``IPOD_EXCLUSIVE_CHIPS`` series, which never reads that clone, so this
script reuses the packager's own helpers and constants to append whatever
ids in ``IPOD_EXCLUSIVE_CHIPS`` are missing from the already-packaged
``assets/ipodjs/rockbox/sitekick`` output, instead of requiring a full
rebuild.

Idempotent: ids already present in data/chips.v1.tsv are skipped, so
re-running after adding a new wave only appends the new tail.

It re-derives the same page/index placement the full build() would
produce (tiles keep accumulating in id order after the last existing
tile), so a future full rebuild with the real art root reproduces
byte-identical layout for every already-packaged id.

Usage:
    sitekick_chip_wave_append.py [--out <dir>]
"""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image

from sitekick_package_assets import (
    ICON_PX,
    ICONS_PER_PAGE,
    ICONS_PER_ROW,
    ICON_ROWS,
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

SK_NAME_MAX = 20

# Series appended per wave, keyed by the lowest chip id in that wave so a
# rerun can tell which blocks are already present in series.v1.tsv.
WAVE_SERIES = {
    969: (
        "fast-food-dive\tFast Food House Dive\t969,970,971,972",
        "dollhouse\tDollhouse\t973,974,975,976",
        "polaroid\tPolaroid\t977,978,979",
    ),
    980: (
        "hero-of-time\tHero of Time\t980,981,982,983,986",
        "hero-companions\tHero Companions\t984,985,987",
    ),
    988: (
        "hasanabi\tHasanAbi\t988,989,990,991",
        "qtcinderella\tQTCinderella\t992,993,994,995",
    ),
    996: (
        "mayahiga\tMaya Higa\t996,997,998,999",
        "montreal-canadiens\tMontreal Canadiens\t1000,1001,1002,1003",
        "pet-companions\tPet Companions\t1004,1005,1006,1007,1008",
    ),
    1009: (
        "pink-floyd\tPink Floyd\t1009,1010,1011",
        "portal-2\tPortal 2\t1012,1013,1014,1015",
        "osrs-runes\tOSRS Runes\t1016,1017,1018,1019,1020",
    ),
    1021: (
        "osrs-runes-2\tOSRS Runes: Popular Gear\t"
        "1021,1022,1023,1024,1025,1026,1027,1028",
    ),
    1029: (
        "polaroid-2\tPolaroid: More Prints\t1029,1030,1031",
        "killer-instinct\tKiller Instinct\t1032,1033,1034",
        "cyberpunk-2077\tCyberpunk 2077\t1035,1036,1037,1038",
    ),
    1039: (
        "cyberpunk-silverhand\tJohnny Silverhand\t1039,1040,1041,1042",
        "cyberpunk-judy\tJudy Alvarez\t1043,1044,1045",
        "killer-instinct-spinal\tSpinal\t1046,1047,1048,1049",
    ),
}


def load_chip_rows(data_dir: Path) -> tuple[list[str], set[int]]:
    path = data_dir / "chips.v1.tsv"
    lines = path.read_text().splitlines()
    header, rows = lines[0], lines[1:]
    assert header.startswith("# id\t"), f"unexpected chips.v1.tsv header: {header}"
    ids = {int(row.split("\t", 1)[0]) for row in rows}
    return rows, ids


def append(out_root: Path) -> int:
    data_dir = out_root / "data"
    chip_dir = out_root / "chips"
    icon_dir = out_root / "icons"

    rows, existing_ids = load_chip_rows(data_dir)
    start_idx = len(rows)
    print(f"existing chips: {start_idx}")

    new_specs = [spec for spec in IPOD_EXCLUSIVE_CHIPS
                if spec[0] not in existing_ids]
    if not new_specs:
        print("nothing to append; all IPOD_EXCLUSIVE_CHIPS already packaged")
        return 0

    for cid, name in ((s[0], s[6]) for s in new_specs):
        if len(name) >= SK_NAME_MAX:
            raise ValueError(
                f"chip {cid} name {name!r} is {len(name)} chars, "
                f"truncated by SK_NAME_MAX={SK_NAME_MAX} "
                f"(strlcpy keeps {SK_NAME_MAX - 1}); shorten before "
                f"packaging"
            )

    manifest_lines = []
    new_rows = []
    new_tiles: list[tuple[int, Image.Image]] = []

    for offset, (cid, slot, z, ax, ay, rarity, name, kind,
                primary_source) in enumerate(new_specs):
        composed = build_ipod_exclusive_image(kind, out_root)
        assert_stage_fit(cid, name, ax, ay, composed.width, composed.height)
        write_bmp32(chip_dir / f"{cid:04d}.bmp", composed)

        tile, _, _ = trim(composed)
        tile.thumbnail((ICON_PX, ICON_PX), Image.LANCZOS)
        canvas = Image.new("RGBA", (ICON_PX, ICON_PX), (0, 0, 0, 0))
        canvas.alpha_composite(
            tile, ((ICON_PX - tile.width) // 2, (ICON_PX - tile.height) // 2)
        )
        idx = start_idx + offset
        new_tiles.append((cid, canvas))
        new_rows.append(
            f"{cid}\t{slot}\t{z}\t{ax}\t{ay}\t{composed.width}\t"
            f"{composed.height}\t{idx // ICONS_PER_PAGE}\t"
            f"{idx % ICONS_PER_PAGE}\t{rarity}\t1\t{name}"
        )
        manifest_lines.append(
            f"chips/{cid:04d}.bmp\t"
            f"{sha256_of(IPOD_EXCLUSIVE_SOURCE_DIR / primary_source)}"
        )
        print(f"  chip {cid} {name!r}: {composed.size} "
             f"anchor=({ax},{ay}) page={idx // ICONS_PER_PAGE} "
             f"index={idx % ICONS_PER_PAGE}")

    # --- icon pages: fill any partial trailing page, then start new ones --
    end_idx = start_idx + len(new_tiles)
    first_page = start_idx // ICONS_PER_PAGE
    last_page = (end_idx - 1) // ICONS_PER_PAGE
    tiles_written = 0

    for page in range(first_page, last_page + 1):
        page_path = icon_dir / f"page{page}.bmp"
        if page_path.exists():
            sheet = Image.open(page_path).convert("RGBA")
        else:
            sheet = Image.new(
                "RGBA", (ICON_PX * ICONS_PER_ROW, ICON_PX * ICON_ROWS),
                (0, 0, 0, 0),
            )
        page_end_idx = page * ICONS_PER_PAGE + ICONS_PER_PAGE
        while tiles_written < len(new_tiles):
            idx = start_idx + tiles_written
            if idx >= page_end_idx:
                break
            _, tile = new_tiles[tiles_written]
            slot = idx % ICONS_PER_PAGE
            sheet.alpha_composite(
                tile, ((slot % ICONS_PER_ROW) * ICON_PX,
                      (slot // ICONS_PER_ROW) * ICON_PX)
            )
            tiles_written += 1
        write_bmp32(page_path, sheet)
        print(f"  wrote {page_path.name}")

    assert tiles_written == len(new_tiles)

    # --- data files ---------------------------------------------------
    chips_path = data_dir / "chips.v1.tsv"
    header = chips_path.read_text().splitlines()[0]
    chips_path.write_text(
        "\n".join([header] + rows + new_rows) + "\n"
    )

    stage_path = data_dir / "stage.v1.tsv"
    stage_lines = stage_path.read_text().splitlines()
    out_lines = []
    for line in stage_lines:
        if line.startswith("chip_count\t"):
            out_lines.append(f"chip_count\t{end_idx}")
        elif line.startswith("icon_pages\t"):
            out_lines.append(f"icon_pages\t{last_page + 1}")
        else:
            out_lines.append(line)
    stage_path.write_text("\n".join(out_lines) + "\n")

    series_path = data_dir / "series.v1.tsv"
    series_text = series_path.read_text().rstrip("\n")
    new_wave_ids = {spec[0] for spec in new_specs}
    for first_id, lines in WAVE_SERIES.items():
        if first_id in new_wave_ids and lines[0] not in series_text:
            series_text += "\n" + "\n".join(lines)
    series_path.write_text(series_text + "\n")

    manifest_path = out_root / "source.manifest"
    manifest_text = manifest_path.read_text().rstrip("\n")
    existing_manifest_keys = {
        line.split("\t", 1)[0] for line in manifest_text.splitlines()
    }
    fresh = [line for line in manifest_lines
            if line.split("\t", 1)[0] not in existing_manifest_keys]
    if fresh:
        manifest_text += "\n" + "\n".join(fresh)
    manifest_path.write_text(manifest_text + "\n")

    print(f"chip_count {start_idx} -> {end_idx}")
    print(f"icon_pages -> {last_page + 1}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", type=Path, default=OUT_DEFAULT)
    args = ap.parse_args()
    return append(args.out)


if __name__ == "__main__":
    raise SystemExit(main())
