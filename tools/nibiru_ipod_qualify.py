#!/usr/bin/env python3
"""Read-only qualification report for a user-owned NiBiRu disc or folder.

The tool inventories names, sizes, and the source hash. It never extracts game
data and never writes beside the supplied source. An ISO requires `7z` or
`7zz` for listing only.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import struct
from collections import Counter
from pathlib import Path


INTERESTING_EXTENSIONS = {
    ".exe", ".dll", ".dat", ".bin", ".pak", ".cab", ".adb", ".grp",
    ".cfg", ".avi", ".mpg", ".mpeg", ".wmv", ".flc", ".ogg", ".mp3",
    ".wav", ".bmp", ".pcx", ".jpg", ".png",
}

AGDS_KEY = b"Vyvojovy tym AGDS varuje: Hackerovani skodi obchodu!"
AGDS_GRP_SIGNATURE = b"AGDS group file\x1a"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def prefix_md5(path: Path, size: int = 5000) -> str:
    with path.open("rb") as handle:
        return hashlib.md5(handle.read(size), usedforsecurity=False).hexdigest()


def directory_entries(root: Path) -> list[dict]:
    entries = []
    for path in sorted(root.rglob("*")):
        if path.is_file():
            entries.append(
                {"path": path.relative_to(root).as_posix(), "size": path.stat().st_size}
            )
    return entries


def iso_entries(path: Path) -> tuple[list[dict], str]:
    executable = shutil.which("7zz") or shutil.which("7z")
    if not executable:
        raise RuntimeError("ISO listing requires 7zz or 7z; no extraction is performed")
    result = subprocess.run(
        [executable, "l", "-slt", "--", str(path)],
        check=False,
        capture_output=True,
        text=True,
        timeout=120,
    )
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or "7z could not list the ISO")
    entries = []
    current: dict[str, object] = {}
    for raw in result.stdout.splitlines() + [""]:
        if not raw:
            if current.get("path") and not current.get("folder"):
                entries.append(
                    {"path": str(current["path"]), "size": int(current.get("size", 0))}
                )
            current = {}
            continue
        if raw.startswith("Path = "):
            current["path"] = raw[7:]
        elif raw.startswith("Size = "):
            current["size"] = raw[7:]
        elif raw.startswith("Folder = "):
            current["folder"] = raw[9:] == "+"
    return entries, executable


def decrypt_agds(data: bytes) -> bytes:
    return bytes(
        value ^ 0xFF ^ AGDS_KEY[index % len(AGDS_KEY)]
        for index, value in enumerate(data)
    )


def parse_adb(path: Path) -> dict:
    size = path.stat().st_size
    with path.open("rb") as handle:
        header = handle.read(0x14)
        if len(header) != 0x14:
            raise RuntimeError(f"{path.name}: truncated ADB header")
        magic, writable, total, used, name_size = struct.unpack("<5I", header)
        if magic != 666:
            raise RuntimeError(f"{path.name}: invalid ADB magic")
        if used > total or total > 200_000 or not 0 < name_size <= 255:
            raise RuntimeError(f"{path.name}: unsafe ADB index")
        record_size = name_size + 9
        data_offset = 0x14 + record_size * total
        if data_offset > size:
            raise RuntimeError(f"{path.name}: truncated ADB index")
        first = ""
        main_location: tuple[int, int] | None = None
        for index in range(used):
            handle.seek(0x14 + index * record_size)
            record = handle.read(record_size)
            if len(record) != record_size:
                raise RuntimeError(f"{path.name}: truncated ADB record")
            offset = struct.unpack_from("<I", record, 0)[0]
            entry_size = struct.unpack_from("<I", record, name_size + 5)[0]
            if data_offset + offset + entry_size > size:
                raise RuntimeError(f"{path.name}: ADB entry outside file")
            if index == 0:
                first = record[4:4 + name_size + 1].split(b"\0", 1)[0].decode(
                    "ascii", errors="replace"
                )
            name = record[4:4 + name_size + 1].split(b"\0", 1)[0].decode(
                "ascii", errors="replace"
            )
            if name == "main":
                main_location = (data_offset + offset, entry_size)
        main_object = None
        if main_location:
            offset, entry_size = main_location
            handle.seek(offset)
            main = handle.read(entry_size)
            if len(main) >= 9:
                object_id, object_data, code_size, flags = struct.unpack_from(
                    "<HHHB", main
                )
                encoded = struct.unpack_from("<H", main, 7)[0]
                decoded = encoded >> 1 if encoded & 1 else encoded
                main_object = {
                    "id": object_id,
                    "data_size": object_data,
                    "code_size": code_size,
                    "flags": flags,
                    "first_decoded_opcode": decoded,
                    "opcode_base_for_enter_5": decoded - 5,
                }
    return {
        "writable": bool(writable),
        "total_entries": total,
        "used_entries": used,
        "name_size": name_size,
        "data_offset": data_offset,
        "first_entry": first,
        "md5_first_5000": prefix_md5(path),
        "main_object": main_object,
    }


def parse_grp(path: Path) -> dict:
    file_size = path.stat().st_size
    counts = Counter()
    with path.open("rb") as handle:
        header = bytearray(handle.read(0x2C))
        if len(header) != 0x2C:
            raise RuntimeError(f"{path.name}: truncated GRP header")
        encrypted = header[:16] != AGDS_GRP_SIGNATURE
        if encrypted:
            header[:16] = decrypt_agds(bytes(header[:16]))
        if header[:16] != AGDS_GRP_SIGNATURE:
            raise RuntimeError(f"{path.name}: invalid GRP signature")
        version1, magic, version2, entries = struct.unpack_from("<4I", header, 0x10)
        if (version1, magic, version2) != (44, 0x1A03C9E6, 2):
            raise RuntimeError(f"{path.name}: unsupported GRP version")
        if entries > 500_000 or 0x2C + entries * 0x31 > file_size:
            raise RuntimeError(f"{path.name}: truncated GRP index")
        first = ""
        for index in range(entries):
            handle.seek(0x2C + index * 0x31)
            record = bytearray(handle.read(0x31))
            if len(record) != 0x31:
                raise RuntimeError(f"{path.name}: truncated GRP record")
            end = record.find(0, 0, 0x21)
            if end < 0:
                raise RuntimeError(f"{path.name}: unterminated GRP member")
            raw_name = bytes(record[:end])
            if encrypted:
                raw_name = decrypt_agds(raw_name)
            name = raw_name.decode("ascii", errors="replace")
            offset, member_size = struct.unpack_from("<II", record, 0x21)
            if offset + member_size > file_size:
                raise RuntimeError(f"{path.name}: GRP member outside file")
            if index == 0:
                first = name
            counts[Path(name).suffix.lower()] += 1
    return {
        "entries": entries,
        "encrypted": encrypted,
        "first_member": first,
        "extensions": dict(sorted(counts.items())),
        "md5_first_5000": prefix_md5(path),
    }


def parse_agds_config(path: Path) -> dict:
    video_mode = ""
    archives = []
    for raw in path.read_text(encoding="ascii", errors="strict").splitlines():
        line = raw.strip()
        if line.lower().startswith("videomode="):
            video_mode = line.split("=", 1)[1]
        elif line.lower().startswith("path="):
            archives.append(line.split("=", 1)[1])
    parsed_mode = None
    match = re.fullmatch(r"(\d+)x(\d+)x(\d+)", video_mode)
    if match:
        parsed_mode = {
            "width": int(match.group(1)),
            "height": int(match.group(2)),
            "depth": int(match.group(3)),
        }
    return {
        "video_mode": video_mode,
        "parsed_video_mode": parsed_mode,
        "archives": archives,
    }


def detect_executable_version(path: Path) -> str:
    if not path.is_file():
        return ""
    data = path.read_bytes()
    marker = b"AGDS version "
    start = data.find(marker)
    if start < 0:
        marker = b"AGDS "
        start = data.find(marker)
    if start < 0:
        return ""
    end = start
    while end < min(len(data), start + 80) and data[end] not in b"\r\n\0":
        end += 1
    return data[start:end].decode("ascii", errors="replace")


def inspect_agds_directory(root: Path) -> dict:
    adb = root / "data.adb"
    groups = sorted(root.glob("gfx*.grp"))
    if not adb.is_file() or not groups:
        return {
            "detected": False,
            "reason": "Need an installed game folder containing data.adb and gfx*.grp",
        }
    config_path = root / "agds.cfg"
    return {
        "detected": True,
        "database": parse_adb(adb),
        "groups": {group.name: parse_grp(group) for group in groups},
        "config_present": config_path.is_file(),
        "config": parse_agds_config(config_path) if config_path.is_file() else None,
        "executable_version": detect_executable_version(root / "nibiru.exe"),
    }


def build_report(source: Path) -> dict:
    if source.is_dir():
        entries = directory_entries(source)
        source_kind = "directory"
        source_hash = ""
        lister = "filesystem"
    elif source.is_file():
        entries, lister = iso_entries(source)
        source_kind = "disc-image"
        source_hash = sha256(source)
    else:
        raise RuntimeError(f"source does not exist: {source}")

    extensions = Counter(Path(item["path"]).suffix.lower() for item in entries)
    interesting = [
        item for item in entries
        if Path(item["path"]).suffix.lower() in INTERESTING_EXTENSIONS
    ]
    executables = [item for item in entries if item["path"].lower().endswith(".exe")]
    agds = inspect_agds_directory(source) if source.is_dir() else {
        "detected": any(item["path"].lower().endswith(".grp") for item in entries),
        "needs_installed_folder": True,
        "reason": "Disc inventory found; parse AGDS containers after installation/extraction",
    }
    main_object = agds.get("database", {}).get("main_object") or {}
    config_mode = (agds.get("config") or {}).get("parsed_video_mode")
    retail_2509 = (
        main_object.get("opcode_base_for_enter_5") == 2217
        and config_mode == {"width": 1024, "height": 768, "depth": 32}
    )
    return {
        "format": 2,
        "source": str(source.resolve()),
        "source_kind": source_kind,
        "source_sha256": source_hash,
        "lister": lister,
        "file_count": len(entries),
        "total_bytes": sum(int(item["size"]) for item in entries),
        "extensions": dict(sorted(extensions.items())),
        "executables": executables,
        "interesting_files": interesting,
        "agds": agds,
        "rockbox_runtime": {
            "status": (
                "retail-agds-2509-bootstrap-validated"
                if retail_2509
                else "engine-route-identified-needs-data-validation"
            ),
            "reason": (
                "The owned US executable is AGDS 2.509, not upstream's 2.511 "
                "label. Rockbox validates opcode base 2217, ADB/GRP bounds, "
                "configured archive order, and the real startup logo. The full "
                "scene VM, audio, animation, and save path still require porting."
                if retail_2509
                else "AGDS containers were found, but this folder has not matched "
                "the qualified 2.509 opcode and 1024x768 configuration contract."
            ),
            "iso_alone_is_sufficient": "edition-dependent",
            "next_gate": (
                "Validate the exact owned edition, render the first room at "
                "320x240, execute its initial AGDS processes, and play one sound."
            ),
        },
        "copied_game_data": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="owned ISO file or installed game folder")
    parser.add_argument("--output", type=Path, help="optional JSON report path")
    args = parser.parse_args()
    try:
        report = build_report(args.source)
    except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
        parser.error(str(exc))
    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
