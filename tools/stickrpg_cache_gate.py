#!/usr/bin/env python3
"""Preflight Stick RPG Flash cache files before an iPod hardware run."""

from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path


EXPECTED_ACTION_BODY = 1_414_783
EXPECTED_ACTION_LEN = 9_860
EXPECTED_FWS_SIZE = 3_054_794
EXPECTED_GSD_MIN_ENTRIES = 490
EXPECTED_GSC_MIN_ENTRIES = 2_400


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def u16(data: bytes, pos: int) -> int:
    return struct.unpack_from("<H", data, pos)[0]


def u32(data: bytes, pos: int) -> int:
    return struct.unpack_from("<I", data, pos)[0]


def i16(data: bytes, pos: int) -> int:
    return struct.unpack_from("<h", data, pos)[0]


def swf_rect_len(data: bytes) -> int:
    if len(data) < 9:
        raise ValueError("FWS too short for RECT")
    nbits = data[8] >> 3
    return (5 + 4 * nbits + 7) // 8


def parse_fws(path: Path) -> dict[str, object]:
    data = path.read_bytes()
    if len(data) < 16:
        raise ValueError(f"{path} is too short")
    if data[:3] != b"FWS":
        raise ValueError(f"{path} is not an uncompressed FWS file")

    declared = u32(data, 4)
    if declared != len(data):
        raise ValueError(
            f"FWS declared size {declared} does not match file size {len(data)}"
        )
    if len(data) != EXPECTED_FWS_SIZE:
        raise ValueError(
            f"FWS size {len(data)} does not match expected {EXPECTED_FWS_SIZE}"
        )

    frame_rate_raw = u16(data, 8 + swf_rect_len(data))
    frame_count = u16(data, 8 + swf_rect_len(data) + 2)
    pos = 8 + swf_rect_len(data) + 4
    tags = []
    action_target = None

    while pos + 2 <= len(data):
        header = pos
        record = u16(data, pos)
        pos += 2
        tag_type = record >> 6
        tag_len = record & 0x3F
        if tag_len == 0x3F:
            if pos + 4 > len(data):
                raise ValueError(f"long tag header truncated at {header}")
            tag_len = u32(data, pos)
            pos += 4
        body = pos
        end = body + tag_len
        if end > len(data):
            raise ValueError(
                f"tag type {tag_type} at {header} exceeds FWS size: {end}"
            )
        tags.append((header, body, end, tag_type, tag_len))
        if body == EXPECTED_ACTION_BODY and tag_type == 12:
            action_target = tags[-1]
        pos = end
        if tag_type == 0:
            break

    if action_target is None:
        raise ValueError(
            f"expected DoAction body at {EXPECTED_ACTION_BODY} not found"
        )
    if action_target[4] != EXPECTED_ACTION_LEN:
        raise ValueError(
            f"DoAction length {action_target[4]} does not match "
            f"{EXPECTED_ACTION_LEN}"
        )

    checksum = 0
    chunk_count = 0
    with path.open("rb") as f:
        f.seek(action_target[1])
        remaining = action_target[4]
        while remaining:
            chunk = f.read(min(512, remaining))
            if not chunk:
                raise ValueError("short read inside expected DoAction block")
            checksum = (checksum + sum(chunk)) & 0xFFFFFFFF
            chunk_count += 1
            remaining -= len(chunk)

    return {
        "declared": declared,
        "frame_rate_raw": frame_rate_raw,
        "frame_count": frame_count,
        "tag_count": len(tags),
        "action_header": action_target[0],
        "action_body": action_target[1],
        "action_end": action_target[2],
        "action_len": action_target[4],
        "action_chunks": chunk_count,
        "action_checksum": checksum,
    }


def parse_gsc(path: Path) -> dict[str, int]:
    data = path.read_bytes()
    if len(data) < 6:
        raise ValueError(f"{path} is too short")
    if data[:3] != b"gsc" or data[3] != 7:
        raise ValueError(f"{path} has bad gsc header/version")

    pos = 4
    count = 0
    max_id = 0
    max_payload = 0
    while True:
        if pos + 2 > len(data):
            raise ValueError("gsc ended before character terminator")
        character_id = i16(data, pos)
        pos += 2
        if character_id == -1:
            break
        if pos + 4 > len(data):
            raise ValueError("gsc truncated before payload size")
        payload_size = u32(data, pos)
        pos += 4
        if payload_size < 0 or pos + payload_size > len(data):
            raise ValueError(f"gsc bad payload for id {character_id}")
        count += 1
        max_id = max(max_id, character_id)
        max_payload = max(max_payload, payload_size)
        pos += payload_size

    if count < EXPECTED_GSC_MIN_ENTRIES:
        raise ValueError(f"gsc has only {count} entries")
    return {
        "entries": count,
        "max_id": max_id,
        "max_payload": max_payload,
        "bytes_used": pos,
    }


def parse_gsd(path: Path) -> dict[str, int]:
    data = path.read_bytes()
    if len(data) < 8:
        raise ValueError(f"{path} is too short")
    if u32(data, 0) != 0x43445347 or u32(data, 4) != 1:
        raise ValueError(f"{path} has bad gsd header/version")

    pos = 8
    count = 0
    max_id = 0
    max_stream = 0
    max_payload = 0
    while pos < len(data):
        if pos + 16 > len(data):
            raise ValueError("gsd truncated inside entry header")
        character_id = u32(data, pos)
        tag_type = u32(data, pos + 4)
        stream_pos = u32(data, pos + 8)
        payload_size = u32(data, pos + 12)
        pos += 16
        if tag_type not in (2, 22, 32, 83):
            raise ValueError(f"gsd impossible tag type {tag_type}")
        if payload_size <= 0 or payload_size > 16 * 1024 * 1024:
            raise ValueError(f"gsd bad payload size for id {character_id}")
        if pos + payload_size > len(data):
            raise ValueError(f"gsd payload for id {character_id} is truncated")
        count += 1
        max_id = max(max_id, character_id)
        max_stream = max(max_stream, stream_pos)
        max_payload = max(max_payload, payload_size)
        pos += payload_size

    if count < EXPECTED_GSD_MIN_ENTRIES:
        raise ValueError(f"gsd has only {count} entries")
    return {
        "entries": count,
        "max_id": max_id,
        "max_stream": max_stream,
        "max_payload": max_payload,
        "bytes_used": pos,
    }


def require_file(path: Path) -> None:
    if not path.is_file():
        raise ValueError(f"missing {path}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--base",
        type=Path,
        default=Path("build-sim-ipod6g/simdisk/.rockbox/flash/stickrpg"),
        help="directory containing stickrpg.swf.fws/.gsc/.gsd",
    )
    args = parser.parse_args()

    base = args.base
    fws = base / "stickrpg.swf.fws"
    gsc = base / "stickrpg.swf.gsc"
    gsd = base / "stickrpg.swf.gsd"
    for path in (fws, gsc, gsd):
        require_file(path)

    fws_info = parse_fws(fws)
    gsc_info = parse_gsc(gsc)
    gsd_info = parse_gsd(gsd)

    print(f"PASS fws size={fws_info['declared']} sha256={sha256(fws)}")
    print(
        "PASS fws tags={tag_count} frames={frame_count} "
        "action={action_body}-{action_end} len={action_len} "
        "chunks={action_chunks} checksum={action_checksum:08x}".format(
            **fws_info
        )
    )
    print(f"PASS gsc entries={gsc_info['entries']} sha256={sha256(gsc)}")
    print(
        "PASS gsd entries={entries} max_stream={max_stream} "
        "sha256={sha}".format(sha=sha256(gsd), **gsd_info)
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValueError as exc:
        print(f"FAIL {exc}", file=sys.stderr)
        raise SystemExit(1)
