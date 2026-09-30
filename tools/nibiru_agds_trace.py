#!/usr/bin/env python3
"""Inventory AGDS 2.509 object bytecode in a private NiBiRu data.adb.

This is a read-only clean-room analysis tool. It records numeric canonical
opcodes and object relationships without copying scripts or game assets.
"""

from __future__ import annotations

import argparse
import json
import struct
from collections import Counter
from pathlib import Path


OPCODE_BASE_2509 = 2217
IMMEDIATE_BYTES = {
    5: 4,   # Enter: two uint16 arguments, followed by its 12-byte header
    8: 2,   # JumpZImm16
    9: 2,   # JumpImm16
    14: 4,  # PushImm32
    15: 2,  # PushImm16
    16: 1,  # PushImm8
    17: 2,  # PushImm16_2
    18: 1,  # PushImm8_2
    21: 1,  # GetGlobalImm8
    59: 2,
    60: 2,
    61: 2,
    62: 2,
    63: 2,
    64: 2,
    65: 2,
    201: 2,
    202: 2,
    209: 2,
    229: 2,
}

OPCODE_NAMES = {
    5: "Enter", 6: "EndObject", 8: "JumpZ", 9: "Jump", 10: "Pop",
    11: "Dup", 12: "ExitProcess", 13: "SuspendProcess",
    14: "Push32", 15: "Push16", 16: "Push8", 17: "Push16_2",
    18: "Push8_2", 21: "GetGlobal", 22: "Equals",
    23: "NotEquals", 24: "Greater", 25: "Less", 26: "GreaterEqual",
    27: "LessEqual", 28: "Add", 29: "Sub", 30: "Mul", 31: "Div",
    32: "Mod", 33: "And", 34: "Or", 35: "Xor", 36: "Not",
    39: "BoolAnd", 40: "BoolOr", 41: "BoolNot", 42: "Negate",
    48: "SetGlobal", 59: "ObjectInitialise", 60: "OnLook",
    61: "OnUse", 63: "OnUseObject", 65: "OnCharacterTrap",
    66: "LoadMouseCursorFromObject", 68: "LoadRegionFromObject",
    69: "LoadPictureFromObject", 70: "LoadAnimationFromObject",
    71: "SetObjectZ", 72: "SetScreenBackground", 73: "LoadTextFromObject",
    75: "ScreenLoadRegion", 76: "ScreenLoadObject", 77: "ScreenCloneObject",
    78: "ScreenRemoveObject", 79: "SetNextScreen", 98: "DisableUser",
    99: "EnableUser", 117: "LoadAnimation", 118: "LoadSample",
    119: "SetPhaseVarControlled", 124: "NPCSay", 127: "SetTimer",
    128: "ResetState", 129: "SetAnimationZ", 133: "SetPanVolume",
    134: "SetAnimationPosition", 135: "SetPhaseVar",
    136: "SetAnimationLoop", 137: "SetAnimationSpeed",
    146: "GetRegionCenterX", 147: "GetRegionCenterY",
    151: "CompareScreenName", 158: "Quit", 159: "StartNewGame",
    165: "MoveScreenObject", 175: "AppendSharedString",
    198: "LoadPicture", 201: "SetThrowHandler", 202: "SetUseOnHandler",
    203: "PlayFilm", 209: "OnUserUse", 227: "LoadFont",
    221: "InsertAnimation",
    229: "OnKey", 231: "CommitLight", 235: "FadeScreen", 240: "LoadDialog",
    242: "HasGlobal", 252: "SetCamera", 263: "SetFog",
    264: "SetLightAmbient", 265: "SetLightDiffuse",
    266: "SetLightSpecular", 267: "SetLightPosition",
    248: "CreateSavePicture", 249: "LoadSavePicture",
    250: "LoadSaveDescription", 255: "SetObjectAnimationFields",
    283: "GetAnimationLastPhase", 284: "SetScreenClip",
    285: "SetScreenObjectZ",
    286: "ShowObjectText", 287: "ShowObjectNumber",
    288: "ApplyPictureAlpha",
}


def decode_opcode(code: bytes, ip: int) -> tuple[int, int]:
    if ip >= len(code):
        raise ValueError("opcode outside code")
    encoded = code[ip]
    ip += 1
    if encoded & 1:
        if ip >= len(code):
            raise ValueError("truncated tagged opcode")
        encoded = (encoded | code[ip] << 8) >> 1
        ip += 1
    opcode = encoded - OPCODE_BASE_2509
    if opcode < 0:
        raise ValueError("encoded value precedes 2.509 opcode base")
    return opcode, ip


def parse_object(payload: bytes) -> dict | None:
    if len(payload) < 9:
        return None
    object_id, data_size, code_size, flags = struct.unpack_from("<HHHB", payload)
    if data_size != 0 or flags != 1 or code_size == 0 or 7 + code_size > len(payload):
        return None
    code = payload[7 : 7 + code_size]
    try:
        first, first_end = decode_opcode(code, 0)
    except ValueError:
        return None
    if first != 5 or first_end + 16 > len(code):
        return None
    magic, enter_size = struct.unpack_from("<HH", code, first_end)
    if (magic, enter_size) != (0xDEAD, 12):
        return None
    extra = first_end + 4
    resource_offset, resource_count = struct.unpack_from("<HH", code, extra + 6)
    table = resource_offset + 24 if resource_count else len(code)
    if resource_count > 4096 or table + resource_count * 4 > len(code):
        raise ValueError("invalid object string table")

    counts: Counter[int] = Counter()
    ip = 0
    while ip < table:
        try:
            opcode, next_ip = decode_opcode(code, ip)
        except ValueError:
            break
        if opcode > 292:
            break
        ip = next_ip
        size = IMMEDIATE_BYTES.get(opcode, 0)
        if ip + size > table:
            break
        ip += size
        counts[opcode] += 1
        if opcode == 5:
            if ip + 12 > table:
                break
            ip += 12
        if opcode == 6:
            break
    strings = []
    for index in range(resource_count):
        relative = struct.unpack_from("<H", code, table + index * 4)[0]
        offset = table + relative
        end = code.find(b"\0", offset)
        if offset >= len(code) or end < 0:
            raise ValueError(f"invalid object string {index}")
        strings.append(code[offset:end].decode("latin-1"))
    return {
        "id": object_id,
        "code_size": code_size,
        "string_count": resource_count,
        "opcodes": counts,
        "code": code,
        "code_end": table,
        "strings": strings,
    }


def disassemble(parsed: dict) -> list[dict[str, object]]:
    code = parsed["code"]
    code_end = parsed["code_end"]
    instructions = []
    ip = 0
    while ip < code_end:
        start = ip
        opcode, ip = decode_opcode(code, ip)
        size = IMMEDIATE_BYTES.get(opcode, 0)
        if ip + size > code_end:
            raise ValueError(f"truncated immediate at {start:#x}")
        raw = code[ip : ip + size]
        ip += size
        item: dict[str, object] = {
            "offset": start,
            "opcode": opcode,
            "name": OPCODE_NAMES.get(opcode, f"Opcode{opcode}"),
        }
        if size:
            item["immediate"] = int.from_bytes(raw, "little", signed=False)
        if opcode == 5:
            if ip + 12 > code_end:
                raise ValueError("truncated Enter body")
            item["enter_body"] = list(struct.unpack_from("<6H", code, ip))
            ip += 12
        instructions.append(item)
        if opcode == 6:
            break
    return instructions


def adb_entries(path: Path):
    blob = path.read_bytes()
    if len(blob) < 20:
        raise ValueError("truncated ADB header")
    magic, _, total, used, name_size = struct.unpack_from("<5I", blob)
    if magic != 666 or used > total or not 0 < name_size <= 255:
        raise ValueError("invalid ADB header")
    record_size = name_size + 9
    data_offset = 20 + total * record_size
    if data_offset > len(blob):
        raise ValueError("truncated ADB index")
    for index in range(used):
        record = blob[20 + index * record_size : 20 + (index + 1) * record_size]
        name = record[4 : 5 + name_size].rstrip(b"\0").decode(
            "ascii", errors="replace"
        )
        offset = struct.unpack_from("<I", record)[0]
        size = struct.unpack_from("<I", record, name_size + 5)[0]
        start = data_offset + offset
        if start + size > len(blob):
            raise ValueError(f"ADB entry outside file: {name}")
        yield name, blob[start : start + size]


def trace(
    path: Path,
    wanted_objects: set[str] | None = None,
    wanted_opcodes: set[int] | None = None,
) -> dict:
    opcode_counts: Counter[int] = Counter()
    code_bytes = 0
    objects = 0
    invalid_objects = []
    largest = []
    max_strings = (0, "")
    selected = {}
    opcode_objects: dict[int, list[dict[str, object]]] = {
        opcode: [] for opcode in sorted(wanted_opcodes or set())
    }
    for name, payload in adb_entries(path):
        try:
            parsed = parse_object(payload)
        except ValueError as exc:
            invalid_objects.append({"name": name, "error": str(exc)})
            continue
        if parsed is None:
            continue
        objects += 1
        code_bytes += parsed["code_size"]
        opcode_counts.update(parsed["opcodes"])
        largest.append((parsed["code_size"], name))
        max_strings = max(max_strings, (parsed["string_count"], name))
        if wanted_objects and name.casefold() in wanted_objects:
            selected[name] = {
                "id": parsed["id"],
                "code_size": parsed["code_size"],
                "strings": parsed["strings"],
                "instructions": disassemble(parsed),
            }
        for opcode in opcode_objects:
            count = parsed["opcodes"].get(opcode, 0)
            if count:
                opcode_objects[opcode].append({"name": name, "count": count})
    largest.sort(reverse=True)
    return {
        "format": 1,
        "source": str(path.resolve()),
        "engine": "AGDS 2.509",
        "opcode_base": OPCODE_BASE_2509,
        "objects": objects,
        "code_bytes": code_bytes,
        "opcode_counts": {
            str(opcode): count for opcode, count in sorted(opcode_counts.items())
        },
        "largest_objects": [
            {"name": name, "code_size": size} for size, name in largest[:20]
        ],
        "invalid_objects": invalid_objects[:100],
        "max_string_count": {"count": max_strings[0], "name": max_strings[1]},
        "selected_objects": selected,
        "selected_opcodes": {
            str(opcode): {
                "count": sum(int(item["count"]) for item in entries),
                "objects": entries,
            }
            for opcode, entries in opcode_objects.items()
        },
        "copied_game_data": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("data_adb", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument(
        "--object", action="append", default=[],
        help="include a private object disassembly in the report",
    )
    parser.add_argument(
        "--opcode", action="append", default=[], type=int,
        help="list every object containing a canonical opcode",
    )
    args = parser.parse_args()
    report = trace(
        args.data_adb,
        {name.casefold() for name in args.object},
        set(args.opcode),
    )
    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
