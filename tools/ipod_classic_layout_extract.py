#!/usr/bin/env python3
"""Audit named view/layout/font records in the pinned iPod35 2.0.4 image.

Exports source words with offsets and hashes. Coordinate triples retain their
anchor tokens and modes; they are not misrepresented as resolved LCD rectangles.
This tool does not execute firmware or generate replacement artwork.
"""

import argparse
import hashlib
import json
import struct
from pathlib import Path

from ipod_classic_resource_extract import (
    EXPECTED_SECTIONS, RESOURCE_HEADER_LENGTH, SOURCE_RUNTIME_SEGMENTS,
    source_identity,
)


def extract(path):
    data, digest, base = source_identity(path)
    names = {}
    segments = SOURCE_RUNTIME_SEGMENTS[digest]
    for position in range(0, len(data) - 7, 4):
        pointer, token = struct.unpack_from("<II", data, position)
        if not 0x0DAD0000 <= token < 0x0DAE0000:
            continue
        for file_start, runtime, length in segments:
            if runtime <= pointer < runtime + length:
                offset = file_start + pointer - runtime
                end = data.find(b"\0", offset, offset + 256)
                if end < 0:
                    continue
                text = data[offset:end]
                if text and all(32 <= c < 127 for c in text):
                    names.setdefault(token, set()).add(text.decode("ascii"))
    records = []
    for section, count, flag, table in EXPECTED_SECTIONS:
        if section not in (b"weiV", b"tyLV", b"TNOF", b"RTSF", b"RLOC"):
            continue
        for index in range(count):
            token, relative, size = struct.unpack_from(
                "<III", data, base + table + index * 12)
            offset = base + RESOURCE_HEADER_LENGTH + relative
            if offset + size > len(data):
                raise ValueError("out-of-bounds layout resource")
            raw = data[offset:offset + size]
            record = dict(section=section.decode(), index=index,
                          token=f"0x{token:08x}",
                          names=sorted(names.get(token, ())),
                          source_offset=f"0x{offset:08x}", size=size,
                          sha256=hashlib.sha256(raw).hexdigest())
            if section == b"RTSF":
                record["family"] = raw.rstrip(b"\0").decode("ascii")
            else:
                record["words"] = [
                    dict(offset=p, value=f"0x{value:08x}",
                         signed=struct.unpack_from("<i", raw, p)[0],
                         names=sorted(names.get(value, ())))
                    for p in range(0, len(raw) - 3, 4)
                    for value in (struct.unpack_from("<I", raw, p)[0],)
                ]
            records.append(record)
    return dict(schema=1, source_sha256=digest, records=records)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    report = extract(args.source)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(f"Audited {len(report['records'])} view/layout/font/colour records")


if __name__ == "__main__":
    main()
