#!/usr/bin/env python3
"""Fetch real Generation 1 Pokedex data/sprites from PokeAPI and deploy them
to a Rockbox device or simulator `simdisk` tree.

This is a standalone tool: it has no dependency on the `rockpod` package.
It fetches real species data (types, base stats, height/weight, genus, and
the real English flavor text) from PokeAPI, downloads the real Game Boy
front sprite for each species from the PokeAPI/sprites repository, and
writes everything the `pokedex.rock` plugin reads: `pokedex.v1.tsv`,
`sprites/<id>.bmp`, and a `source.manifest` provenance file.

Nothing here is invented or hand-drawn. A species whose fetch fails is
skipped entirely rather than replaced with a placeholder.

See docs/pokedex-spec.md for the on-device data model and asset policy.

Usage:
    pokedex_fetch_pokeapi.py --out-dir /tmp/pokedex_pack
    pokedex_fetch_pokeapi.py --deploy-root build-sim-ipod6g/simdisk
    pokedex_fetch_pokeapi.py --deploy-root /run/media/$USER/IPOD --start 1 --end 20
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import struct
import sys
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone
from io import BytesIO
from pathlib import Path

USER_AGENT = "RockPod Pokedex importer (standalone)"
POKEAPI_ROOT = "https://pokeapi.co/api/v2"
SPRITE_ROOT = (
    "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/"
    "pokemon/versions/generation-i/red-blue/transparent"
)
DEX_COUNT = 151
REL_DIR = ".rockbox/pokedex"
PREFERRED_FLAVOR_VERSIONS = ("red", "blue", "yellow")

TSV_COLUMNS = (
    "dex_id", "name", "genus", "type1", "type2", "height_display",
    "weight_display", "hp", "atk", "def", "spa", "spd", "spe", "flavor_text",
)


def _fetch_json(url: str, timeout: int = 20) -> dict:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return json.loads(response.read().decode("utf-8"))


def _fetch_bytes(url: str, timeout: int = 20) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return response.read()


def _sanitize(value) -> str:
    """Collapse embedded newlines/form-feeds/tabs so a field is TSV-safe."""
    text = str(value or "").replace("\x0c", " ")
    return " ".join(text.split())


def _display_name(species_payload: dict, fallback: str) -> str:
    for entry in species_payload.get("names") or []:
        if entry.get("language", {}).get("name") == "en":
            name = entry.get("name") or fallback
            break
    else:
        name = fallback
    # Transliterate characters that are real but not reliably present in
    # every Rockbox bitmap font, so the name still renders everywhere.
    name = name.replace("’", "'")
    name = name.replace("♀", "(F)")
    name = name.replace("♂", "(M)")
    return _sanitize(name)


def _height_display(decimeters: int) -> str:
    total_inches = round(decimeters * 10 / 2.54)
    feet, inches = divmod(total_inches, 12)
    return f"{feet}'{inches:02d}\""


def _weight_display(hectograms: int) -> str:
    pounds = hectograms * 0.1 * 2.20462
    return f"{pounds:.1f} lbs."


def _flavor_text(species_payload: dict) -> str:
    by_version = {}
    for entry in species_payload.get("flavor_text_entries") or []:
        if entry.get("language", {}).get("name") != "en":
            continue
        version = entry.get("version", {}).get("name")
        if version and version not in by_version:
            by_version[version] = entry.get("flavor_text", "")
    for version in PREFERRED_FLAVOR_VERSIONS:
        if version in by_version:
            return _sanitize(by_version[version])
    if by_version:
        return _sanitize(next(iter(by_version.values())))
    return ""


def _genus(species_payload: dict) -> str:
    for entry in species_payload.get("genera") or []:
        if entry.get("language", {}).get("name") == "en":
            return _sanitize(entry.get("genus", ""))
    return ""


def _stat(mon_payload: dict, stat_name: str) -> int:
    for entry in mon_payload.get("stats") or []:
        if entry.get("stat", {}).get("name") == stat_name:
            return int(entry.get("base_stat", 0))
    return 0


def fetch_species(dex_id: int, errors: list[str]):
    try:
        mon = _fetch_json(f"{POKEAPI_ROOT}/pokemon/{dex_id}")
        species = _fetch_json(f"{POKEAPI_ROOT}/pokemon-species/{dex_id}")
    except (urllib.error.URLError, urllib.error.HTTPError, TimeoutError,
            OSError, ValueError) as exc:
        errors.append(f"#{dex_id:03d}: species fetch failed: {exc}")
        return None

    types = sorted(mon.get("types") or [], key=lambda entry: entry["slot"])
    type_names = [entry["type"]["name"] for entry in types]
    type1 = type_names[0] if len(type_names) > 0 else ""
    type2 = type_names[1] if len(type_names) > 1 else ""

    return {
        "dex_id": dex_id,
        "name": _display_name(species, mon.get("name", f"pokemon-{dex_id}")),
        "genus": _genus(species),
        "type1": type1,
        "type2": type2,
        "height_display": _height_display(int(mon.get("height", 0))),
        "weight_display": _weight_display(int(mon.get("weight", 0))),
        "hp": _stat(mon, "hp"),
        "atk": _stat(mon, "attack"),
        "def": _stat(mon, "defense"),
        "spa": _stat(mon, "special-attack"),
        "spd": _stat(mon, "special-defense"),
        "spe": _stat(mon, "speed"),
        "flavor_text": _flavor_text(species),
    }


def write_bmp32(path: Path, image) -> None:
    """Write a bottom-up 32bpp BI_RGB BGRA BMP.

    Same layout as tools/sitekick_package_assets.py's write_bmp32(): Rockbox's
    reader (apps/recorder/bmp.c) takes BI_RGB 32bpp in BGRA order and derives
    a 4bpp alpha plane from it, which is what FORMAT_TRANSPARENT blits need.
    """
    image = image.convert("RGBA")
    width, height = image.size
    pixels = image.load()
    rows = []
    for y in range(height - 1, -1, -1):
        row = bytearray()
        for x in range(width):
            r, g, b, a = pixels[x, y]
            row += bytes((b, g, r, a))
        rows.append(bytes(row))
    body = b"".join(rows)
    header = struct.pack("<2sIHHI", b"BM", 14 + 40 + len(body), 0, 0, 14 + 40)
    info = struct.pack("<IiiHHIIiiII", 40, width, height, 1, 32, 0,
                        len(body), 2835, 2835, 0, 0)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + info + body)


def fetch_sprite(dex_id: int, sprites_dir: Path, errors: list[str]):
    try:
        from PIL import Image
    except ImportError:
        errors.append(
            f"#{dex_id:03d}: sprite skipped: Pillow is not installed "
            "(pip install --user pillow)")
        return None
    try:
        raw = _fetch_bytes(f"{SPRITE_ROOT}/{dex_id}.png")
    except (urllib.error.URLError, urllib.error.HTTPError, TimeoutError,
            OSError) as exc:
        errors.append(f"#{dex_id:03d}: sprite fetch failed: {exc}")
        return None
    image = Image.open(BytesIO(raw))
    out_path = sprites_dir / f"{dex_id:03d}.bmp"
    write_bmp32(out_path, image)
    return out_path


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    digest.update(path.read_bytes())
    return digest.hexdigest()


def build_pack(out_dir: Path, start: int, end: int, delay: float) -> tuple[int, list[str]]:
    bundle_dir = out_dir / REL_DIR
    sprites_dir = bundle_dir / "sprites"
    sprites_dir.mkdir(parents=True, exist_ok=True)

    errors: list[str] = []
    manifest_lines: list[str] = []
    rows: list[str] = []

    for dex_id in range(start, end + 1):
        entry = fetch_species(dex_id, errors)
        if entry is None:
            continue
        sprite_path = fetch_sprite(dex_id, sprites_dir, errors)
        if sprite_path is None:
            continue
        rows.append("\t".join(str(entry[col]) for col in TSV_COLUMNS))
        manifest_lines.append(
            f"sprites/{sprite_path.name}\t{sha256_of(sprite_path)}")
        if delay:
            time.sleep(delay)

    if not rows:
        return 0, errors

    generated_at = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    header = "\t".join(
        ["pokedex_v1", generated_at, "pokeapi.co", str(len(rows))])
    tsv_path = bundle_dir / "pokedex.v1.tsv"
    tsv_path.write_text(header + "\n" + "\n".join(rows) + "\n",
                         encoding="utf-8")
    manifest_lines.insert(0, f"pokedex.v1.tsv\t{sha256_of(tsv_path)}")
    (bundle_dir / "source.manifest").write_text(
        "\n".join(manifest_lines) + "\n", encoding="utf-8")

    return len(rows), errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path,
                         default=Path("/tmp/pokedex_pack"),
                         help="Directory to write the pack under "
                              "(<out-dir>/.rockbox/pokedex/).")
    parser.add_argument("--deploy-root", type=Path, default=None,
                         help="Device mount point or simdisk root to copy "
                              "the finished pack onto, in addition to "
                              "--out-dir.")
    parser.add_argument("--start", type=int, default=1)
    parser.add_argument("--end", type=int, default=DEX_COUNT)
    parser.add_argument("--delay", type=float, default=0.1,
                         help="Seconds to sleep between species (be kind "
                              "to the free PokeAPI/GitHub raw endpoints).")
    args = parser.parse_args()

    if args.start < 1 or args.end > DEX_COUNT or args.start > args.end:
        parser.error(f"--start/--end must be within 1..{DEX_COUNT}")

    written, errors = build_pack(args.out_dir, args.start, args.end,
                                  args.delay)
    print(f"Fetched {written} species into "
          f"{args.out_dir / REL_DIR}")
    for warning in errors:
        print(f"warning: {warning}", file=sys.stderr)

    if not written:
        print("error: no species were fetched successfully",
              file=sys.stderr)
        return 1

    if args.deploy_root is not None:
        src_root = args.out_dir / REL_DIR
        dst_root = args.deploy_root / REL_DIR
        if dst_root.exists():
            shutil.rmtree(dst_root)
        shutil.copytree(src_root, dst_root)
        print(f"Deployed to {dst_root}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
