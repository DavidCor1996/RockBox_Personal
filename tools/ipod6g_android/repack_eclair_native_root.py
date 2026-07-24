#!/usr/bin/env python3
"""Inject the Classic key layout into an already-qualified Eclair RAM root."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import stat

from package_eclair_native_root import (
    CLICKWHEEL_KEY_LAYOUT,
    DATA_FILES,
    DIRECTORIES,
    LOCAL_FILES,
    RUNTIME_FILES,
    add_newc_entry,
)
from qualify_n25_ramdiag import QualificationError, parse_newc, regular, sha256


LEGACY_DIRECTORIES = set(DIRECTORIES) - {
    "system/usr",
    "system/usr/keylayout",
}
LEGACY_FILES = {"init.rc", *RUNTIME_FILES, *DATA_FILES}


def build_archive(entries: dict[str, tuple[int, bytes]]) -> bytes:
    output = bytearray()
    inode = 1
    for relative in sorted(DIRECTORIES):
        add_newc_entry(
            output,
            inode,
            relative,
            stat.S_IFDIR | DIRECTORIES[relative],
            b"",
            2,
        )
        inode += 1
    for relative in sorted({*LEGACY_FILES, *LOCAL_FILES}):
        mode, body = entries[relative]
        add_newc_entry(output, inode, relative, mode, body, 1)
        inode += 1
    add_newc_entry(output, inode, "TRAILER!!!", 0, b"", 1)
    return bytes(output)


def repack(source: Path, destination: Path) -> dict:
    source_initramfs = regular(str(source / "eclair-native-initramfs.cpio.gz"))
    source_report_path = regular(str(source / "qualification.json"))
    source_report = json.loads(source_report_path.read_text(encoding="utf-8"))
    if source_report.get("hardware_actions_enabled") is not False:
        raise QualificationError("source root does not retain the hardware lock")
    if source_report.get("persistent_storage_nodes") is not False:
        raise QualificationError("source root exposes persistent storage")
    if source_report.get("initramfs", {}).get("sha256") != sha256(
        source_initramfs
    ):
        raise QualificationError("source root report checksum mismatch")

    compressed = source_initramfs.read_bytes()
    if compressed[:3] != b"\x1f\x8b\x08" or compressed[4:8] != b"\0\0\0\0":
        raise QualificationError("source root is not deterministic gzip")
    entries = parse_newc(gzip.decompress(compressed))
    expected = LEGACY_DIRECTORIES | LEGACY_FILES
    if set(entries) != expected:
        raise QualificationError(
            f"unexpected source root entries: {sorted(set(entries) ^ expected)}"
        )
    for relative in LEGACY_FILES:
        record = source_report.get("runtime_files", {}).get(relative, {})
        body = entries[relative][1]
        if record.get("sha256") != hashlib.sha256(body).hexdigest():
            raise QualificationError(f"source runtime checksum mismatch: {relative}")

    if destination.exists() and any(destination.iterdir()):
        raise QualificationError("destination must be absent or empty")
    destination.mkdir(parents=True, exist_ok=True)

    keylayout_relative = "system/usr/keylayout/iPod_Classic_Click_Wheel.kl"
    keylayout = CLICKWHEEL_KEY_LAYOUT.read_bytes()
    entries[keylayout_relative] = (stat.S_IFREG | 0o644, keylayout)
    archive = build_archive(entries)
    output_initramfs = destination / "eclair-native-initramfs.cpio.gz"
    with output_initramfs.open("wb") as raw:
        with gzip.GzipFile(
            filename="", mode="wb", fileobj=raw, compresslevel=9, mtime=0
        ) as stream:
            stream.write(archive)

    report = dict(source_report)
    report["schema"] = 2
    report["repacked_from_initramfs_sha256"] = sha256(source_initramfs)
    report["n25_clickwheel_keylayout_packaged"] = True
    report["keylayout_injection_gate_passed"] = True
    report["keylayout_source_sha256"] = hashlib.sha256(keylayout).hexdigest()
    runtime_files = dict(report.get("runtime_files", {}))
    runtime_files[keylayout_relative] = {
        "size": len(keylayout),
        "sha256": hashlib.sha256(keylayout).hexdigest(),
    }
    report["runtime_files"] = runtime_files
    report["initramfs"] = {
        "file": output_initramfs.name,
        "uncompressed_size": len(archive),
        "compressed_size": output_initramfs.stat().st_size,
        "sha256": sha256(output_initramfs),
    }
    report_path = destination / "qualification.json"
    report_path.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    (destination / "SHA256SUMS").write_text(
        f"{sha256(output_initramfs)}  {output_initramfs.name}\n"
        f"{sha256(report_path)}  {report_path.name}\n",
        encoding="utf-8",
    )
    return report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    report = repack(args.source.resolve(), args.destination.resolve())
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (QualificationError, json.JSONDecodeError, OSError) as error:
        print(json.dumps({"keylayout_injection_gate_passed": False,
                          "error": str(error)}))
        raise SystemExit(1)
