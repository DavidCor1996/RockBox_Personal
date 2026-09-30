#!/usr/bin/env python3
"""Extract selected owned NiBiRu resources from AGDS ADB/GRP archives.

The extractor is deliberately name-based and bounded: it never loads the
archive into memory, validates every index entry against the archive size,
and copies only explicitly requested resources. Game data belongs in the
private ignored asset tree, not in the Rockbox source or firmware package.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
from dataclasses import dataclass
from pathlib import Path


ADB_MAGIC = 666
ADB_HEADER_SIZE = 20
ADB_HEADER = struct.Struct("<5I")
GRP_SIGNATURE = b"AGDS group file\x1a"
GRP_HEADER_SIZE = 0x2C
GRP_RECORD_SIZE = 0x31
GRP_NAME_SIZE = 0x21
GRP_MAGIC = 0x1A03C9E6
GRP_KEY = b"Vyvojovy tym AGDS varuje: Hackerovani skodi obchodu!"
MAX_ENTRIES = 1_000_000
MAX_NAME_SIZE = 255
COPY_CHUNK = 1024 * 1024

FIRST_CUTSCENE_RESOURCES = (
    "1864",
    "1864.17e9",
    "1864.11b6",
    "1122.10e1",
    "1122.10e1.rp",
    "1864.1012",
    "1864.1012.p",
    "1864.1354",
    "1864.1354.p",
    "1864.1354.rp",
    "intro_byt.bmp",
    "intro_byt_stul.bmp",
    "martin_intro_intro.x",
    "martin_intro_mluvi1.x",
    "martin_intro_mluvi2.x",
    "martin_intro_mluvi3.x",
    "martin_podzimINT_RukaL.x",
    "sluchatko_intro.x",
    "objektyintro.bmp",
    "Martin_podzim-int.bmp",
    "1AMB_kancelar.ogg",
    "1PC_Klavesa1.wav",
    "1PC_Klavesa2.wav",
    "1PC_Klavesa3.wav",
    "1PC_Enter.wav",
    "1Telefon_zvedne.wav",
    "1Telefon_zvoni.wav",
    "martin_Intro.ogg",
)


@dataclass(frozen=True)
class Entry:
    name: str
    offset: int
    size: int


def decrypt(data: bytes) -> bytes:
    return bytes(
        value ^ 0xFF ^ GRP_KEY[index % len(GRP_KEY)]
        for index, value in enumerate(data)
    )


def read_adb_index(archive: Path) -> dict[str, Entry]:
    archive_size = archive.stat().st_size
    with archive.open("rb") as stream:
        header = stream.read(ADB_HEADER_SIZE)
        if len(header) != ADB_HEADER_SIZE:
            raise ValueError("truncated ADB header")
        magic, _writeable, total, used, name_size = ADB_HEADER.unpack(header)
        if magic != ADB_MAGIC:
            raise ValueError(f"invalid ADB magic {magic}")
        if used > total or total > MAX_ENTRIES:
            raise ValueError("unsafe ADB entry count")
        if not 0 < name_size <= MAX_NAME_SIZE:
            raise ValueError("unsafe ADB name size")

        record_size = name_size + 9
        data_offset = ADB_HEADER_SIZE + total * record_size
        if data_offset > archive_size:
            raise ValueError("ADB index extends past archive")

        entries: dict[str, Entry] = {}
        for index in range(used):
            record = stream.read(record_size)
            if len(record) != record_size:
                raise ValueError(f"truncated ADB record {index}")
            relative = struct.unpack_from("<I", record)[0]
            raw_name = record[4 : 5 + name_size]
            name = raw_name.split(b"\0", 1)[0].decode("ascii", errors="strict")
            size = struct.unpack_from("<I", record, name_size + 5)[0]
            offset = data_offset + relative
            if offset > archive_size or size > archive_size - offset:
                raise ValueError(f"ADB entry outside archive: {name}")
            key = name.casefold()
            if key in entries:
                raise ValueError(f"duplicate ADB entry: {name}")
            entries[key] = Entry(name=name, offset=offset, size=size)
    return entries


def read_grp_index(archive: Path) -> dict[str, Entry]:
    archive_size = archive.stat().st_size
    with archive.open("rb") as stream:
        header = stream.read(GRP_HEADER_SIZE)
        if len(header) != GRP_HEADER_SIZE:
            raise ValueError("truncated GRP header")
        encrypted = header[:16] != GRP_SIGNATURE
        signature = decrypt(header[:16]) if encrypted else header[:16]
        if signature != GRP_SIGNATURE:
            raise ValueError("invalid GRP signature")
        version1, magic, version2, count = struct.unpack_from("<4I", header, 16)
        if (version1, magic, version2) != (44, GRP_MAGIC, 2):
            raise ValueError("unsupported GRP version")
        if count > MAX_ENTRIES or GRP_HEADER_SIZE + count * GRP_RECORD_SIZE > archive_size:
            raise ValueError("unsafe GRP entry count")

        entries: dict[str, Entry] = {}
        for index in range(count):
            record = stream.read(GRP_RECORD_SIZE)
            if len(record) != GRP_RECORD_SIZE:
                raise ValueError(f"truncated GRP record {index}")
            name_end = record.find(b"\0", 0, GRP_NAME_SIZE)
            if name_end < 0:
                raise ValueError(f"unterminated GRP name {index}")
            raw_name = record[:name_end]
            if encrypted:
                raw_name = decrypt(raw_name)
            # GRP names are byte strings in the original engine; preserve
            # legacy high bytes losslessly while ASCII asset names still
            # compare normally.
            name = raw_name.decode("latin-1")
            offset, size = struct.unpack_from("<II", record, GRP_NAME_SIZE)
            if offset > archive_size or size > archive_size - offset:
                raise ValueError(f"GRP entry outside archive: {name}")
            key = name.casefold()
            if key in entries:
                raise ValueError(f"duplicate GRP entry: {name}")
            entries[key] = Entry(name=name, offset=offset, size=size)
    return entries


def read_index(archive: Path) -> dict[str, Entry]:
    with archive.open("rb") as stream:
        marker = stream.read(4)
    if len(marker) == 4 and struct.unpack("<I", marker)[0] == ADB_MAGIC:
        return read_adb_index(archive)
    return read_grp_index(archive)


def archives_at(path: Path) -> list[Path]:
    if path.is_file():
        return [path]
    if not path.is_dir():
        raise ValueError(f"archive source does not exist: {path}")
    preferred = [path / "models.adb", path / "data.adb"]
    return [candidate for candidate in preferred if candidate.is_file()] + sorted(
        path.glob("gfx*.grp")
    )


def safe_output_name(name: str) -> str:
    if not name or Path(name).name != name or name in {".", ".."}:
        raise ValueError(f"unsafe resource name: {name!r}")
    return name


def extract_entry(
    archive: Path, entry: Entry, output_dir: Path, force: bool
) -> dict[str, object]:
    output = output_dir / safe_output_name(entry.name)
    temporary = output.with_name(output.name + ".partial")
    if output.exists() and not force:
        raise FileExistsError(f"refusing to overwrite {output}; pass --force")

    digest = hashlib.sha256()
    remaining = entry.size
    try:
        with archive.open("rb") as source, temporary.open("wb") as target:
            source.seek(entry.offset)
            while remaining:
                chunk = source.read(min(remaining, COPY_CHUNK))
                if not chunk:
                    raise ValueError(f"short read extracting {entry.name}")
                target.write(chunk)
                digest.update(chunk)
                remaining -= len(chunk)
            target.flush()
            os.fsync(target.fileno())
        os.replace(temporary, output)
    finally:
        if temporary.exists():
            temporary.unlink()

    return {
        "name": entry.name,
        "archive": archive.name,
        "size": entry.size,
        "sha256": digest.hexdigest(),
        "source_offset": entry.offset,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "archive", type=Path, help="owned ADB/GRP file or extracted ISO game directory"
    )
    parser.add_argument("output_dir", type=Path)
    parser.add_argument(
        "--first-cutscene",
        action="store_true",
        help="extract Martin's resources and textures named by object 1122.10e1/model",
    )
    parser.add_argument("--entry", action="append", default=[])
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    requested = list(args.entry)
    if args.first_cutscene:
        requested.extend(FIRST_CUTSCENE_RESOURCES)
    requested = list(dict.fromkeys(name.casefold() for name in requested))
    if not requested:
        parser.error("select --first-cutscene and/or at least one --entry")

    located: dict[str, tuple[Path, Entry]] = {}
    sources = archives_at(args.archive)
    if not sources:
        raise SystemExit(f"no ADB/GRP archives found under {args.archive}")
    for source in sources:
        entries = read_index(source)
        for name in requested:
            if name not in located and name in entries:
                located[name] = (source, entries[name])
    missing = [name for name in requested if name not in located]
    if missing:
        raise SystemExit("missing ADB entries: " + ", ".join(missing))

    args.output_dir.mkdir(parents=True, exist_ok=True)
    extracted = [
        extract_entry(*located[name], args.output_dir, args.force)
        for name in requested
    ]
    manifest = {
        "format": 1,
        "source": str(args.archive.resolve()),
        "resources": extracted,
        "screen_capture_derived": False,
    }
    manifest_path = args.output_dir / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(f"Extracted {len(extracted)} owned resources to {args.output_dir}")
    for resource in extracted:
        print(f"{resource['name']} {resource['size']} {resource['sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
