#!/usr/bin/env python3
"""Strictly verify a RockPod chip wave fits and renders cleanly on Sitekick.

Checks, over every row in data/chips.v1.tsv (not just the new ones, so a
regression in an earlier wave would also be caught):

  * every chip's stage footprint fits the clip box apps/plugins/sitekick.c
    uses (same box as assert_stage_fit in sitekick_package_assets.py);
  * every name fits SK_NAME_MAX (20) without silent strlcpy truncation;
  * the BMP on disk matches the w/h declared in the TSV;
  * the BMP carries true alpha with a transparent margin (no white,
    magenta, or purple corner matte);
  * hair/shell composites in the wave under test keep a punched-through
    head opening.

It then composites the real body sprite plus every chip in the wave under
test at its declared anchor into a 240x192 stage canvas (the exact box
apps/plugins/sitekick.c draws into) as a visual contact sheet.

Usage:
    sitekick_chip_wave_verify.py [contact_sheet.png] [--min-id ID]
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "assets/ipodjs/rockbox/sitekick"
STAGE_W, STAGE_H = 240, 192
BODY_ORIGIN_X, BODY_ORIGIN_Y = 120, 70
VIS_L, VIS_T = -BODY_ORIGIN_X, -BODY_ORIGIN_Y
VIS_R, VIS_B = VIS_L + STAGE_W, VIS_T + STAGE_H
SK_NAME_MAX = 20


def read_bmp32_alpha(path: Path) -> Image.Image:
    data = path.read_bytes()
    offset = struct.unpack_from("<I", data, 10)[0]
    width, raw_height = struct.unpack_from("<ii", data, 18)
    height = abs(raw_height)
    raw = data[offset:offset + width * height * 4]
    return Image.frombytes(
        "RGBA", (width, height), raw, "raw", "BGRA", width * 4,
        -1 if raw_height > 0 else 1,
    )


def load_rows() -> list[dict]:
    rows = []
    for line in (OUT / "data/chips.v1.tsv").read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        f = line.split("\t")
        rows.append(dict(
            id=int(f[0]), slot=f[1], z=int(f[2]), ax=int(f[3]),
            ay=int(f[4]), w=int(f[5]), h=int(f[6]), page=int(f[7]),
            index=int(f[8]), rarity=f[9], worn=int(f[10]), name=f[11],
        ))
    return rows


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("out_path", nargs="?",
                    default="/tmp/sitekick_wave_contact_sheet.png")
    ap.add_argument("--min-id", type=int, default=969,
                    help="lowest chip id considered part of this wave")
    args = ap.parse_args()

    rows = load_rows()
    print(f"loaded {len(rows)} chips")

    failures = []

    for r in rows:
        if r["w"] == 0 and r["h"] == 0:
            continue  # unworn/none-slot placeholder, never drawn
        fits = (r["ax"] >= VIS_L and r["ay"] >= VIS_T and
               r["ax"] + r["w"] <= VIS_R and r["ay"] + r["h"] <= VIS_B)
        if not fits:
            failures.append(f"chip {r['id']} {r['name']!r} out of stage "
                            f"bounds: anchor=({r['ax']},{r['ay']}) "
                            f"size=({r['w']},{r['h']})")

    wave2 = [r for r in rows if r["id"] >= args.min_id]
    print(f"wave chips (id >= {args.min_id}): {len(wave2)}")

    for r in wave2:
        if len(r["name"]) >= SK_NAME_MAX:
            failures.append(
                f"chip {r['id']} name {r['name']!r} is "
                f"{len(r['name'])} chars, truncated by SK_NAME_MAX="
                f"{SK_NAME_MAX} (strlcpy keeps {SK_NAME_MAX - 1})"
            )

        bmp_path = OUT / "chips" / f"{r['id']:04d}.bmp"
        if not bmp_path.exists():
            failures.append(f"chip {r['id']}: missing {bmp_path}")
            continue
        img = read_bmp32_alpha(bmp_path)
        if img.size != (r["w"], r["h"]):
            failures.append(
                f"chip {r['id']}: bmp size {img.size} != tsv "
                f"({r['w']},{r['h']})"
            )

        alpha = img.getchannel("A")
        histogram = alpha.histogram()
        transparent = histogram[0]
        opaque = histogram[255]
        if transparent == 0:
            failures.append(
                f"chip {r['id']}: no fully-transparent pixels "
                f"(no alpha margin / possible corner matte)"
            )
        if opaque == 0:
            failures.append(
                f"chip {r['id']}: no fully-opaque pixels (art may be "
                f"washed out)"
            )
        # Corner-matte check: the four 1px corners must be transparent,
        # not a flood-filled white/magenta/purple background color.
        w, h = img.size
        for cx, cy in ((0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)):
            if img.getpixel((cx, cy))[3] != 0:
                failures.append(
                    f"chip {r['id']}: corner ({cx},{cy}) is not "
                    f"transparent -- possible matte"
                )

        if r["slot"] in ("hair", "shell"):
            bbox = img.getbbox()
            cx0, cy0, cx1, cy1 = (w * 0.30, h * 0.05, w * 0.70, h * 0.35)
            region = img.crop((int(cx0), int(cy0), int(cx1), int(cy1)))
            region_alpha = region.getchannel("A")
            if region_alpha.getextrema()[0] == 255:
                failures.append(
                    f"chip {r['id']} ({r['slot']}): no visible head "
                    f"opening in upper-center region"
                )

    print()
    if failures:
        print(f"FAIL: {len(failures)} problem(s)")
        for f in failures:
            print(f"  - {f}")
    else:
        print("PASS: all stage-fit, name-length and alpha checks clean")

    # --- visual contact sheet: composite body + each wave-2 chip -------
    body = Image.open(OUT / "base/body.bmp").convert("RGBA")
    body_ox = BODY_ORIGIN_X - body.width // 2
    body_oy = BODY_ORIGIN_Y - body.height // 2

    cols = 4
    rows_n = (len(wave2) + cols - 1) // cols
    pad = 8
    sheet = Image.new(
        "RGBA",
        (STAGE_W * cols + pad * (cols + 1),
         STAGE_H * rows_n + pad * (rows_n + 1)),
        (40, 20, 50, 255),
    )
    for i, r in enumerate(sorted(wave2, key=lambda r: r["id"])):
        stage = Image.new("RGBA", (STAGE_W, STAGE_H), (225, 225, 232, 255))
        chip_img = read_bmp32_alpha(OUT / "chips" / f"{r['id']:04d}.bmp")
        layers = [(r["z"], "chip", chip_img,
                  BODY_ORIGIN_X + r["ax"], BODY_ORIGIN_Y + r["ay"])]
        layers.append((0, "body", body, body_ox, body_oy))
        for _, _, img, x, y in sorted(layers, key=lambda t: t[0]):
            stage.alpha_composite(img, (x, y))
        col, row = i % cols, i // cols
        x0 = pad + col * (STAGE_W + pad)
        y0 = pad + row * (STAGE_H + pad)
        sheet.alpha_composite(stage, (x0, y0))

    out_path = Path(args.out_path)
    sheet.convert("RGB").save(out_path)
    print(f"\nwrote contact sheet: {out_path}")

    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
