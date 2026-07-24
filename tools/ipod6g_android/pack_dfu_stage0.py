#!/usr/bin/env python3
"""Create deterministic raw-deflate input and constants for N25 DFU stage zero."""

from __future__ import annotations

import argparse
import binascii
import hashlib
import json
import zlib
from pathlib import Path


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--uboot", required=True)
    parser.add_argument("--deflate", required=True)
    parser.add_argument("--header", required=True)
    parser.add_argument("--report")
    args = parser.parse_args()

    source = Path(args.uboot).resolve(strict=True).read_bytes()
    compressor = zlib.compressobj(9, zlib.DEFLATED, -15)
    compressed = compressor.compress(source) + compressor.flush()
    if zlib.decompress(compressed, -15) != source:
        raise SystemExit("raw DEFLATE round trip failed")

    Path(args.deflate).write_bytes(compressed)
    crc = binascii.crc32(source) & 0xFFFFFFFF
    Path(args.header).write_text(
        "#ifndef N25_DFU_PAYLOAD_CONSTANTS_H\n"
        "#define N25_DFU_PAYLOAD_CONSTANTS_H\n"
        f"#define UBOOT_UNCOMPRESSED_SIZE {len(source)}u\n"
        f"#define UBOOT_COMPRESSED_SIZE {len(compressed)}u\n"
        f"#define UBOOT_CRC32 0x{crc:08x}u\n"
        "#endif\n",
        encoding="ascii",
    )
    if args.report:
        report = {
            "format": "raw-deflate",
            "uncompressed_size": len(source),
            "compressed_size": len(compressed),
            "crc32": f"{crc:08x}",
            "uncompressed_sha256": sha256(source),
            "compressed_sha256": sha256(compressed),
        }
        Path(args.report).write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

