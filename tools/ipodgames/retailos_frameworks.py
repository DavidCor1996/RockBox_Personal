#!/usr/bin/env python3
"""Recover eApp framework export tables from a decrypted retailOS image."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path
from typing import Any

try:
    from .eapp_inspect import EappError, inspect_eapp
except ImportError:
    from eapp_inspect import EappError, inspect_eapp


MAX_EXPORTS = 4096


class FrameworkError(ValueError):
    """Raised when no trustworthy retailOS framework registry can be found."""


def _u32(raw: bytes, offset: int) -> int:
    return struct.unpack_from("<I", raw, offset)[0]


def _name_at(raw: bytes, offset: int) -> str | None:
    field = raw[offset : offset + 32]
    end = field.find(b"\0")
    if end <= 0 or any(field[end:]) or any(byte < 0x20 or byte > 0x7E for byte in field[:end]):
        return None
    return field[:end].decode("ascii")


def inspect_retailos(
    path: Path, load_base: int = 0x10000000
) -> dict[str, Any]:
    path = path.resolve()
    try:
        raw = path.read_bytes()
    except OSError as error:
        raise FrameworkError(f"cannot read {path}: {error}") from error

    frameworks: list[dict[str, Any]] = []
    occupied_until = 0
    for offset in range(0, len(raw) - 64, 4):
        if offset < occupied_until:
            continue
        name = _name_at(raw, offset)
        if name is None or _u32(raw, offset + 32) != 1:
            continue
        count = _u32(raw, offset + 36)
        if count == 0 or count > MAX_EXPORTS:
            continue
        end = offset + 44 + count * 4 + 20
        if end > len(raw) or _u32(raw, end - 4) != count:
            continue
        implementation_offsets = [
            _u32(raw, offset + 44 + index * 4) for index in range(count)
        ]
        if not all(address != 0 for address in implementation_offsets):
            continue
        runtime_addresses = [load_base + value for value in implementation_offsets]
        if any(address > 0xFFFFFFFF for address in runtime_addresses):
            continue
        identifier_offset = offset + 44 + count * 4
        next_record_offset = _u32(raw, offset + 40)
        frameworks.append(
            {
                "name": name,
                "record_file_offset": offset,
                "version": 1,
                "export_count": count,
                "next_record_file_offset": next_record_offset,
                "identifier_hex": raw[
                    identifier_offset : identifier_offset + 16
                ].hex(),
                "implementation_offsets": [
                    f"0x{value:08x}" for value in implementation_offsets
                ],
                "implementation_runtime_addresses": [
                    f"0x{address:08x}" for address in runtime_addresses
                ],
            }
        )
        occupied_until = end

    if not frameworks:
        raise FrameworkError("no validated framework export records found")
    return {
        "format": "ipod-retailos-framework-report-v2",
        "file": {
            "filename": path.name,
            "size": len(raw),
            "sha256": hashlib.sha256(raw).hexdigest(),
        },
        "load_base": f"0x{load_base:08x}",
        "framework_count": len(frameworks),
        "frameworks": frameworks,
    }


def match_eapp_imports(
    retailos_report: dict[str, Any], eapp_report: dict[str, Any]
) -> list[dict[str, Any]]:
    by_identifier = {
        framework["identifier_hex"]: framework
        for framework in retailos_report["frameworks"]
    }
    matches: list[dict[str, Any]] = []
    for imported in eapp_report["frameworks"]:
        exported = by_identifier.get(imported["identifier_hex"])
        if exported is None:
            matches.append(
                {
                    "name": imported["name"],
                    "identifier_hex": imported["identifier_hex"],
                    "status": "missing",
                }
            )
            continue
        if exported["export_count"] != imported["import_count"]:
            status = "count-mismatch"
        else:
            status = "matched"
        used = []
        for reference in imported["direct_branch_imports"]:
            ordinal = reference["ordinal"]
            used.append(
                {
                    **reference,
                    "implementation_offset": exported[
                        "implementation_offsets"
                    ][ordinal],
                    "implementation_runtime_address": exported[
                        "implementation_runtime_addresses"
                    ][ordinal],
                }
            )
        matches.append(
            {
                "name": imported["name"],
                "identifier_hex": imported["identifier_hex"],
                "status": status,
                "import_count": imported["import_count"],
                "export_count": exported["export_count"],
                "directly_referenced_exports": used,
            }
        )
    return matches


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("retailos", type=Path)
    parser.add_argument("--eapp", type=Path)
    parser.add_argument(
        "--load-base",
        type=lambda value: int(value, 0),
        default=0x10000000,
        help="retailOS runtime load address (default: 0x10000000)",
    )
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    try:
        report = inspect_retailos(args.retailos, args.load_base)
        if args.eapp:
            eapp_report = inspect_eapp(args.eapp)
            report["eapp"] = eapp_report["file"]
            report["matches"] = match_eapp_imports(report, eapp_report)
    except (FrameworkError, EappError) as error:
        print(f"retailos_frameworks: {error}", file=sys.stderr)
        return 2

    if args.json:
        json.dump(report, sys.stdout, indent=2, sort_keys=True)
        print()
    else:
        print(f"File: {report['file']['filename']}")
        print(f"Frameworks: {report['framework_count']}")
        for framework in report["frameworks"]:
            print(
                f"  {framework['name']}: {framework['export_count']} exports, "
                f"id {framework['identifier_hex']}"
            )
        for match in report.get("matches", []):
            print(
                f"  eApp {match['name']}: {match['status']}, "
                f"{len(match.get('directly_referenced_exports', []))} used"
            )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
