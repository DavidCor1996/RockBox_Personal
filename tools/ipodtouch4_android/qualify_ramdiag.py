#!/usr/bin/env python3
"""Qualify the minimal N81 RAM-only diagnostic initramfs."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
from pathlib import Path, PurePosixPath
import stat
import subprocess


REQUIRED_MARKERS = (
    b"ROCKPOD N81 RAMDIAG: PID1 started",
    b"storage disabled",
    b"automatic reboot pending",
    b"/dev/fb0",
)
FORBIDDEN_MARKERS = (
    b"/dev/mmc",
    b"/dev/mtd",
    b"/dev/sd",
    b"mount",
    b"pivot_root",
    b"switch_root",
    b"nand",
    b"h2fmi",
)


class QualificationError(RuntimeError):
    """Raised when the diagnostic archive exceeds its RAM-only contract."""


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run_text(command: list[str]) -> str:
    try:
        return subprocess.run(
            command, text=True, capture_output=True, check=True
        ).stdout
    except (OSError, subprocess.CalledProcessError) as error:
        raise QualificationError(f"command failed: {' '.join(command)}: {error}") from error


def align4(value: int) -> int:
    return (value + 3) & ~3


def parse_newc(archive: bytes) -> dict[str, tuple[int, bytes]]:
    entries: dict[str, tuple[int, bytes]] = {}
    offset = 0
    while True:
        if offset + 110 > len(archive) or archive[offset : offset + 6] != b"070701":
            raise QualificationError("initramfs is not a well-formed newc archive")
        header = archive[offset + 6 : offset + 110]
        try:
            fields = [int(header[index : index + 8], 16) for index in range(0, 104, 8)]
        except ValueError as error:
            raise QualificationError("initramfs has an invalid newc header") from error
        mode, file_size, name_size = fields[1], fields[6], fields[11]
        name_start = offset + 110
        name_end = name_start + name_size
        if name_size < 1 or name_end > len(archive) or archive[name_end - 1] != 0:
            raise QualificationError("initramfs has an invalid entry name")
        try:
            name = archive[name_start : name_end - 1].decode("utf-8")
        except UnicodeDecodeError as error:
            raise QualificationError("initramfs path is not UTF-8") from error
        data_start = align4(name_end)
        data_end = data_start + file_size
        if data_end > len(archive):
            raise QualificationError("initramfs entry extends past the archive")
        if name == "TRAILER!!!":
            break
        path = PurePosixPath(name.removeprefix("./"))
        if path.is_absolute() or ".." in path.parts or not path.parts:
            raise QualificationError(f"unsafe initramfs path: {name}")
        normalized = str(path)
        if normalized in entries:
            raise QualificationError(f"duplicate initramfs path: {normalized}")
        entries[normalized] = (mode, archive[data_start:data_end])
        offset = align4(data_end)
    return entries


def qualify(init_elf: Path, initramfs: Path) -> dict[str, object]:
    for path, label in ((init_elf, "init ELF"), (initramfs, "initramfs")):
        if path.is_symlink() or not path.is_file():
            raise QualificationError(f"{label} must be a regular, non-symlink file")
    elf = init_elf.read_bytes()
    if len(elf) > 128 * 1024 or not elf.startswith(b"\x7fELF"):
        raise QualificationError("diagnostic init is not a small ELF image")
    header = run_text(["llvm-readelf", "-h", "-A", "-l", str(init_elf)])
    if "Machine:                           ARM" not in header:
        raise QualificationError("diagnostic init is not ARM")
    if "Description: ARM v7" not in header or "INTERP" in header:
        raise QualificationError("diagnostic init is not static ARMv7")
    symbols = run_text(["llvm-nm", "-a", str(init_elf)])
    if " _start" not in symbols:
        raise QualificationError("diagnostic init lacks _start")
    for marker in REQUIRED_MARKERS:
        if marker not in elf:
            raise QualificationError(f"diagnostic init lacks marker {marker!r}")
    for marker in FORBIDDEN_MARKERS:
        if marker.lower() in elf.lower():
            raise QualificationError(f"diagnostic init contains {marker!r}")

    packed = initramfs.read_bytes()
    if len(packed) > 1024 * 1024 or not packed.startswith(b"\x1f\x8b"):
        raise QualificationError("diagnostic initramfs is not a small gzip archive")
    if len(packed) < 10 or packed[4:8] != b"\0\0\0\0":
        raise QualificationError("diagnostic gzip header has a nonzero timestamp")
    try:
        entries = parse_newc(gzip.decompress(packed))
    except (OSError, EOFError) as error:
        raise QualificationError(f"cannot decompress initramfs: {error}") from error
    if set(entries) != {"dev", "proc", "sys", "init"}:
        raise QualificationError(f"unexpected initramfs entries: {sorted(entries)}")
    for directory in ("dev", "proc", "sys"):
        mode, body = entries[directory]
        if not stat.S_ISDIR(mode) or body:
            raise QualificationError(f"initramfs {directory} is not an empty directory")
    init_mode, embedded_init = entries["init"]
    if not stat.S_ISREG(init_mode) or not (init_mode & 0o111):
        raise QualificationError("initramfs /init is not executable")
    if embedded_init != elf:
        raise QualificationError("initramfs /init differs from the qualified ELF")

    return {
        "schema": 1,
        "artifact_gate_passed": True,
        "board": "apple-n81-ipod-touch-4g",
        "profile": "n81-volatile-ramdiag-no-storage",
        "storage_paths_present": False,
        "shell_present": False,
        "framebuffer_probe_compiled": True,
        "automatic_reboot_seconds": 30,
        "hardware_actions_enabled": False,
        "device_test_ready": False,
        "physical_tested": False,
        "artifacts": {
            "init": {"size": init_elf.stat().st_size, "sha256": sha256(init_elf)},
            "initramfs": {
                "size": initramfs.stat().st_size,
                "sha256": sha256(initramfs),
            },
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("init_elf", type=Path)
    parser.add_argument("initramfs", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        report = qualify(args.init_elf.resolve(), args.initramfs.resolve())
    except (OSError, QualificationError) as error:
        parser.error(str(error))
    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
