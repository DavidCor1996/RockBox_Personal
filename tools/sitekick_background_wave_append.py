#!/usr/bin/env python3
"""Append any not-yet-packaged SITEKICK_BACKGROUNDS entries.

Mirrors sitekick_chip_wave_append.py: the canonical packager
(``sitekick_package_assets.py``'s ``build()``) rebuilds the whole
background set from a clone of the SitekickRemastered repo this tree does
not have locally. Backgrounds appended by RockPod waves are sourced from
``assets/ipodjs/sources/sitekick/backgrounds`` directly (see
``BACKGROUND_SOURCE_DIR``), so this script reuses the packager's own
``cover_image``/``write_bmp32`` helpers to append whatever entries in
``SITEKICK_BACKGROUNDS`` are missing from the already-packaged
``backgrounds/stage-N.bmp`` / ``pane-N.bmp`` output, instead of requiring a
full rebuild.

Idempotent: indices already present on disk are skipped. New backgrounds
are always appended at the end, so ``sk_background`` (a saved index into
this list) never changes meaning for existing saves -- see
SK_BACKGROUND_COUNT in apps/plugins/sitekick.c, which must be bumped by
hand to match.

Usage:
    sitekick_background_wave_append.py [--out <dir>]
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from PIL import Image, ImageDraw

from sitekick_package_assets import (
    BACKGROUND_SOURCE_DIR,
    SITEKICK_BACKGROUNDS,
    STAGE_H,
    STAGE_W,
    YTV_LOGO,
    cover_image,
    sha256_of,
    write_bmp32,
)

OUT_DEFAULT = (Path(__file__).resolve().parent.parent /
               "assets/ipodjs/rockbox/sitekick")


def append(out_root: Path) -> int:
    background_dir = out_root / "backgrounds"
    manifest_path = out_root / "source.manifest"
    manifest_text = manifest_path.read_text().rstrip("\n")
    existing_keys = {line.split("\t", 1)[0]
                     for line in manifest_text.splitlines()}

    start_index = 0
    while (background_dir / f"stage-{start_index}.bmp").exists():
        start_index += 1
    print(f"existing backgrounds: {start_index}")

    if start_index >= len(SITEKICK_BACKGROUNDS):
        print("nothing to append; all SITEKICK_BACKGROUNDS already packaged")
        return 0

    new_manifest_lines = []
    for index in range(start_index, len(SITEKICK_BACKGROUNDS)):
        name, filename = SITEKICK_BACKGROUNDS[index]
        source = BACKGROUND_SOURCE_DIR / filename
        if not source.is_file():
            sys.exit(f"missing background source: {source}")
        background = Image.open(source).convert("RGBA")
        stage_background = cover_image(background, (STAGE_W, STAGE_H))
        pane_background = cover_image(background, (174, 240))
        pane_draw = ImageDraw.Draw(pane_background)
        pane_draw.rectangle((0, 0, 173, 31), fill=(76, 11, 100, 255))
        pane_draw.rectangle((0, 31, 173, 35), fill=(255, 112, 11, 255))
        pane_draw.rounded_rectangle(
            (8, 190, 165, 230), radius=6,
            fill=(247, 240, 249, 235), outline=(76, 11, 100, 255),
            width=2,
        )
        if YTV_LOGO.is_file():
            preview_logo = Image.open(YTV_LOGO).convert("RGBA")
            preview_logo.thumbnail((42, 25), Image.LANCZOS)
            pane_background.alpha_composite(preview_logo, (7, 3))
        write_bmp32(background_dir / f"stage-{index}.bmp", stage_background)
        write_bmp32(background_dir / f"pane-{index}.bmp", pane_background)
        digest = sha256_of(source)
        for key in (f"backgrounds/stage-{index}.bmp",
                   f"backgrounds/pane-{index}.bmp"):
            if key not in existing_keys:
                new_manifest_lines.append(f"{key}\t{digest}")
        print(f"  background {index} {name!r} <- {filename}")

    if new_manifest_lines:
        manifest_text += "\n" + "\n".join(new_manifest_lines)
        manifest_path.write_text(manifest_text + "\n")

    print(f"background count {start_index} -> {len(SITEKICK_BACKGROUNDS)}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", type=Path, default=OUT_DEFAULT)
    args = ap.parse_args()
    return append(args.out)


if __name__ == "__main__":
    raise SystemExit(main())
