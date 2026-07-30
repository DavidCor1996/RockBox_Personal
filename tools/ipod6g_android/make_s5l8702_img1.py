#!/usr/bin/env python3
"""Wrap a raw S5L8702 payload in the unsigned RAM-execution IMG1 format."""

from __future__ import annotations

import argparse
from pathlib import Path
import struct


HEADER_SIZE = 0x800
BODY_ALIGNMENT = 16
MAX_BODY_SIZE = 0x3C000


def build_img1(payload: bytes) -> bytes:
    padded_size = (len(payload) + BODY_ALIGNMENT - 1) & -BODY_ALIGNMENT
    if not payload:
        raise ValueError("IMG1 payload is empty")
    if padded_size > MAX_BODY_SIZE:
        raise ValueError("IMG1 payload enters the reserved top 16 KiB of IRAM")
    header = struct.pack(
        "<4s3sBIIIII32sHH16s",
        b"8702",
        b"1.0",
        2,
        0,
        padded_size,
        padded_size,
        padded_size,
        0,
        b"",
        0,
        0,
        b"",
    )
    return (
        header
        + bytes(HEADER_SIZE - len(header))
        + payload
        + bytes(padded_size - len(payload))
    )


def main(argv=None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("payload", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args(argv)
    payload = args.payload.resolve(strict=True).read_bytes()
    image = build_img1(payload)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(image)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValueError as error:
        raise SystemExit(f"cannot create S5L8702 IMG1: {error}")
