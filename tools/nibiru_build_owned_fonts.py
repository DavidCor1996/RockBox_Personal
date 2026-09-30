#!/usr/bin/env python3
"""Build compact Rockbox fonts from an owned NiBiRu installation.

AGDS 2.509 registers nine fonts in ``main.104c``.  The resources use a
``.flc`` suffix but contain signed TrueType fonts.  This tool extracts those
resources only into a temporary directory and converts the exact faces at
the retail-authored heights scaled from 1024x768 to the iPod's 320x240 game
surface, with a readable 12-pixel minimum for text.  Generated fonts belong beside the user's private game data; no
copyrighted font is copied into the Rockbox source or firmware package.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path

from nibiru_extract_owned_assets import archives_at, decrypt, read_index


ROOT = Path(__file__).resolve().parents[1]
CANVAS_HEIGHT = 768
IPOD_HEIGHT = 240
FONT_LIMIT = 0x17F


@dataclass(frozen=True)
class FontSlot:
    slot: int
    descriptor: str
    authored_height: int
    primary: tuple[int, int, int]
    secondary: tuple[int, int, int] | None
    flags: int


# Exact main.104c registrations from the owned AGDS 2.509 bytecode.
FONT_SLOTS = (
    FontSlot(0, "109f", 18, (252, 171, 78), (0, 0, 0), 64),
    FontSlot(1, "109f", 16, (183, 164, 131), (0, 0, 0), 68),
    FontSlot(2, "109f", 16, (148, 166, 174), (0, 0, 0), 68),
    FontSlot(3, "10a0", 16, (183, 164, 131), (0, 0, 0), 76),
    FontSlot(4, "10a1", 12, (10, 10, 10), None, 64),
    FontSlot(5, "109f", 16, (0, 0, 0), None, 68),
    FontSlot(6, "10a1", 12, (10, 10, 10), None, 64),
    FontSlot(7, "10a2", 16, (100, 0, 0), None, 68),
    FontSlot(8, "10a2", 32, (100, 0, 0), None, 68),
)


def scaled_height(authored: int) -> int:
    return max(12, (authored * IPOD_HEIGHT + CANVAS_HEIGHT // 2) // CANVAS_HEIGHT)


def locate_entries(game_dir: Path) -> dict[str, tuple[Path, object]]:
    located: dict[str, tuple[Path, object]] = {}
    for archive in archives_at(game_dir):
        for key, entry in read_index(archive).items():
            located.setdefault(key, (archive, entry))
    return located


def read_entry(located: dict[str, tuple[Path, object]], name: str) -> bytes:
    match = located.get(name.casefold())
    if match is None:
        raise ValueError(f"owned NiBiRu resource is missing: {name}")
    archive, entry = match
    with archive.open("rb") as stream:
        stream.seek(entry.offset)
        data = stream.read(entry.size)
    if len(data) != entry.size:
        raise ValueError(f"short read for owned NiBiRu resource: {name}")
    return data


def descriptor_name(data: bytes, descriptor: str) -> str:
    while data.endswith(b"\0"):
        data = data[:-1]
    if not data:
        raise ValueError(f"empty font descriptor: {descriptor}")
    if data[0] < 0x20 or data[0] >= 0x7F:
        data = decrypt(data)
    try:
        name = data.decode("latin-1")
    except UnicodeDecodeError as error:
        raise ValueError(f"invalid font descriptor: {descriptor}") from error
    if not name or Path(name).name != name:
        raise ValueError(f"unsafe font resource in {descriptor}: {name!r}")
    return name


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("game_dir", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument(
        "--convttf", type=Path, default=ROOT / "tools/convttf"
    )
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    if not args.convttf.is_file() or not os.access(args.convttf, os.X_OK):
        raise SystemExit(f"Rockbox convttf is unavailable: {args.convttf}")
    located = locate_entries(args.game_dir)
    descriptors = sorted({slot.descriptor for slot in FONT_SLOTS})
    resources = {
        descriptor: descriptor_name(read_entry(located, descriptor), descriptor)
        for descriptor in descriptors
    }
    args.output_dir.mkdir(parents=True, exist_ok=True)
    generated: dict[tuple[str, int], dict[str, object]] = {}

    with tempfile.TemporaryDirectory(
        prefix=".nibiru-owned-fonts-", dir=args.output_dir
    ) as temporary:
        temporary_path = Path(temporary)
        source_paths: dict[str, Path] = {}
        for descriptor, resource in resources.items():
            data = read_entry(located, resource)
            if data[:4] not in (b"\0\1\0\0", b"OTTO", b"ttcf"):
                raise ValueError(
                    f"owned resource {resource} is not a TrueType/OpenType font"
                )
            source = temporary_path / descriptor
            source.write_bytes(data)
            source_paths[descriptor] = source

        for slot in FONT_SLOTS:
            key = (slot.descriptor, slot.authored_height)
            if key in generated:
                continue
            filename = f"{slot.descriptor}-{slot.authored_height}.fnt"
            output = args.output_dir / filename
            if output.exists() and not args.force:
                raise FileExistsError(
                    f"refusing to overwrite {output}; pass --force"
                )
            pending = temporary_path / filename
            command = (
                str(args.convttf),
                "-p", str(scaled_height(slot.authored_height)),
                "-s", "32",
                "-l", str(FONT_LIMIT),
                "-o", str(pending),
                str(source_paths[slot.descriptor]),
            )
            result = subprocess.run(
                command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, check=False,
            )
            if result.returncode != 0 or not pending.is_file():
                raise RuntimeError(
                    f"convttf failed for {filename}:\n{result.stdout}"
                )
            os.replace(pending, output)
            generated[key] = {
                "file": filename,
                "authored_height": slot.authored_height,
                "ipod_height": scaled_height(slot.authored_height),
                "resource": resources[slot.descriptor],
                "sha256": sha256(output),
                "size": output.stat().st_size,
            }

    manifest = {
        "format": 1,
        "canvas": [1024, 768],
        "surface": [320, 240],
        "screen_capture_derived": False,
        "source": str(args.game_dir.resolve()),
        "fonts": list(generated.values()),
        "slots": [
            {
                "slot": slot.slot,
                "descriptor": slot.descriptor,
                "authored_height": slot.authored_height,
                "primary": list(slot.primary),
                "secondary": (
                    list(slot.secondary) if slot.secondary is not None else None
                ),
                "flags": slot.flags,
            }
            for slot in FONT_SLOTS
        ],
    }
    manifest_path = args.output_dir / "manifest.json"
    if manifest_path.exists() and not args.force:
        raise FileExistsError(
            f"refusing to overwrite {manifest_path}; pass --force"
        )
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(
        f"Built {len(generated)} exact owned font variants for "
        f"{len(FONT_SLOTS)} AGDS slots in {args.output_dir}"
    )
    for item in generated.values():
        print(
            f"{item['file']} {item['authored_height']}->{item['ipod_height']}px "
            f"{item['size']} {item['sha256']}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
