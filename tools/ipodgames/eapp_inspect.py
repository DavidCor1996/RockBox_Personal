#!/usr/bin/env python3
"""Inspect the known portions of a decrypted iPod eApp executable."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path
from typing import Any


MAX_FRAMEWORKS = 128
MAX_IMPORTS = 4096
FRAMEWORK_NAME_SIZE = 32
FRAMEWORK_FIXED_SIZE = FRAMEWORK_NAME_SIZE + 16 + 4 + 4


class EappError(ValueError):
    """Raised when an eApp is malformed or unsupported."""


def _u32(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(data):
        raise EappError(f"32-bit read outside file at 0x{offset:x}")
    return struct.unpack_from("<I", data, offset)[0]


def _framework_name(raw: bytes, offset: int) -> str:
    end = offset + FRAMEWORK_NAME_SIZE
    if offset < 0 or end > len(raw):
        raise EappError(f"framework name outside file at 0x{offset:x}")
    value = raw[offset:end].split(b"\0", 1)[0]
    if not value or any(byte < 0x20 or byte > 0x7E for byte in value):
        raise EappError(f"invalid framework name at 0x{offset:x}")
    return value.decode("ascii")


def _signed_24(value: int) -> int:
    return value - 0x1000000 if value & 0x800000 else value


def _direct_import_references(
    raw: bytes, load_base: int, frameworks: list[dict[str, Any]]
) -> int:
    targets: dict[int, tuple[dict[str, Any], int]] = {}
    for framework in frameworks:
        framework["direct_branch_imports"] = []
        framework["direct_branch_reference_count"] = 0
        for ordinal in range(framework["import_count"]):
            target = load_base + framework["stubs_file_offset"] + ordinal * 4
            targets[target] = (framework, ordinal)

    references: dict[tuple[str, int], dict[str, Any]] = {}
    for offset in range(0, len(raw) - 3, 4):
        word = _u32(raw, offset)
        if word >> 28 == 0xF or word & 0x0E000000 != 0x0A000000:
            continue
        displacement = _signed_24(word & 0x00FFFFFF) << 2
        target = (load_base + offset + 8 + displacement) & 0xFFFFFFFF
        match = targets.get(target)
        if match is None:
            continue
        framework, ordinal = match
        key = (framework["name"], ordinal)
        item = references.setdefault(
            key,
            {
                "ordinal": ordinal,
                "call_count": 0,
                "tail_branch_count": 0,
                "source_file_offsets": [],
            },
        )
        if word & 0x01000000:
            item["call_count"] += 1
        else:
            item["tail_branch_count"] += 1
        if len(item["source_file_offsets"]) < 32:
            item["source_file_offsets"].append(offset)

    for framework in frameworks:
        imports = [
            value
            for (name, _ordinal), value in references.items()
            if name == framework["name"]
        ]
        imports.sort(key=lambda item: item["ordinal"])
        framework["direct_branch_imports"] = imports
        framework["direct_branch_reference_count"] = sum(
            item["call_count"] + item["tail_branch_count"] for item in imports
        )
    return len(references)


def inspect_eapp(path: Path) -> dict[str, Any]:
    path = path.resolve()
    try:
        raw = path.read_bytes()
    except OSError as error:
        raise EappError(f"cannot read {path}: {error}") from error

    if len(raw) < 0x2C or raw[:4] != b"eapp":
        raise EappError("file is not a decrypted eapp executable")

    first_name_pointer = _u32(raw, 0x10)
    first_name_offset_hint = _u32(raw, 0x0C) + 4
    if first_name_pointer < first_name_offset_hint:
        raise EappError("invalid first framework pointer")
    load_base = first_name_pointer - first_name_offset_hint
    if load_base & 3:
        raise EappError("unaligned inferred load base")

    frameworks: list[dict[str, Any]] = []
    terminator: dict[str, Any] | None = None
    visited: set[int] = set()
    name_pointer = first_name_pointer

    while name_pointer:
        if len(frameworks) >= MAX_FRAMEWORKS:
            raise EappError("too many framework records")
        if name_pointer < load_base:
            raise EappError("framework pointer precedes load base")

        name_offset = name_pointer - load_base
        if name_offset in visited:
            raise EappError("framework list contains a cycle")
        visited.add(name_offset)

        name = _framework_name(raw, name_offset)
        uuid_offset = name_offset + FRAMEWORK_NAME_SIZE
        count_offset = uuid_offset + 16
        next_offset = count_offset + 4
        count = _u32(raw, count_offset)
        next_pointer = _u32(raw, next_offset)
        if count > MAX_IMPORTS:
            raise EappError(f"framework {name} has too many imports: {count}")

        if count == 0 and next_pointer == 0:
            terminator = {
                "name": name,
                "name_pointer": f"0x{name_pointer:08x}",
                "name_file_offset": name_offset,
                "identifier_hex": raw[uuid_offset : uuid_offset + 16].hex(),
            }
            break

        stubs_offset = next_offset + 4
        slots_offset = stubs_offset + count * 4
        end_offset = slots_offset + count * 4
        if end_offset > len(raw):
            raise EappError(f"framework {name} tables extend outside file")

        stub_words = [
            _u32(raw, stubs_offset + index * 4) for index in range(count)
        ]
        slot_words = [
            _u32(raw, slots_offset + index * 4) for index in range(count)
        ]

        frameworks.append(
            {
                "name": name,
                "name_pointer": f"0x{name_pointer:08x}",
                "name_file_offset": name_offset,
                "identifier_hex": raw[uuid_offset : uuid_offset + 16].hex(),
                "import_count": count,
                "next_name_pointer": f"0x{next_pointer:08x}",
                "stubs_file_offset": stubs_offset,
                "slots_file_offset": slots_offset,
                "table_end_file_offset": end_offset,
                "unique_stub_words": [f"0x{word:08x}" for word in sorted(set(stub_words))],
                "nonzero_slot_count": sum(word != 0 for word in slot_words),
            }
        )

        name_pointer = next_pointer

    directly_referenced_imports = _direct_import_references(
        raw, load_base, frameworks
    )
    return {
        "format": "ipod-eapp-report-v1",
        "file": {
            "filename": path.name,
            "size": len(raw),
            "sha256": hashlib.sha256(raw).hexdigest(),
        },
        "header": {
            "magic": "eapp",
            "word_04": f"0x{_u32(raw, 0x04):08x}",
            "word_08": f"0x{_u32(raw, 0x08):08x}",
            "first_record_offset": _u32(raw, 0x0C),
            "first_framework_pointer": f"0x{first_name_pointer:08x}",
            "inferred_load_base": f"0x{load_base:08x}",
            "word_14": f"0x{_u32(raw, 0x14):08x}",
            "word_18": f"0x{_u32(raw, 0x18):08x}",
            "word_1c": f"0x{_u32(raw, 0x1C):08x}",
            "word_20": f"0x{_u32(raw, 0x20):08x}",
            "word_24": f"0x{_u32(raw, 0x24):08x}",
        },
        "frameworks": frameworks,
        "framework_count": len(frameworks),
        "terminator": terminator,
        "total_import_slots": sum(item["import_count"] for item in frameworks),
        "directly_referenced_imports": directly_referenced_imports,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)

    try:
        report = inspect_eapp(args.executable)
    except EappError as error:
        print(f"eapp_inspect: {error}", file=sys.stderr)
        return 2

    if args.json:
        json.dump(report, sys.stdout, indent=2, sort_keys=True)
        print()
    else:
        print(f"File: {report['file']['filename']}")
        print(f"Load base: {report['header']['inferred_load_base']}")
        print(f"Frameworks: {report['framework_count']}")
        for framework in report["frameworks"]:
            print(
                f"  {framework['name']}: {framework['import_count']} imports, "
                f"{len(framework['direct_branch_imports'])} directly referenced, "
                f"id {framework['identifier_hex']}"
            )
            if framework["direct_branch_imports"]:
                ordinals = ", ".join(
                    str(item["ordinal"])
                    for item in framework["direct_branch_imports"]
                )
                print(f"    ordinals: {ordinals}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
