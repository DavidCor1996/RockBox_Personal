#!/usr/bin/env python3
"""Correct the legacy iPod 6G WPS Select remap, preserving all other bytes."""
import argparse
import struct
from pathlib import Path


def fix_keymap(data):
    if len(data) < 24 or len(data) % 12:
        raise ValueError("Invalid keymap size")
    records = list(struct.iter_unpack("<iii", data))
    # Format version, current iPod 6G action ABI, total record count.
    if records[0] != (1, 0x47B2, len(records)):
        raise ValueError("Not the expected iPod 6G keymap ABI")
    mappings = records[1:]
    contexts = []
    for record in mappings:
        if record == (-1, 0, 0):
            break
        contexts.append(record)
    else:
        raise ValueError("Missing context-table terminator")
    wps = [r for r in contexts if r[0] == 0x08000001]
    if len(wps) != 1:
        raise ValueError("Expected exactly one WPS context")
    _, start, count = wps[0]
    if (start <= len(contexts) or count < 1 or start + count >= len(mappings)
            or mappings[start + count] != (-1, 0, 0)):
        raise ValueError("Invalid WPS mapping bounds")
    matches = [i for i in range(start, start + count)
               if mappings[i][1:] == (0x02000001, 1)]
    if len(matches) != 1 or mappings[matches[0]][0] not in (18, 33):
        raise ValueError("Expected one short Select Browse/Playlist mapping")
    result = bytearray(data)
    # ACTION_WPS_VIEW_PLAYLIST (33) -> ACTION_WPS_BROWSE (18).
    # The existing iPodJS WPS Browse handler cycles art/rating/bars.
    struct.pack_into("<i", result, (matches[0] + 1) * 12, 18)
    return bytes(result)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    original = args.source.read_bytes()
    corrected = fix_keymap(original)
    with args.output.open("xb") as stream:
        stream.write(corrected)
    print("Verified: only WPS short Select action corrected; all other bytes preserved")
