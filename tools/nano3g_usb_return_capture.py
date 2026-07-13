#!/usr/bin/env python3
"""Capture the transient Nano 3G read-only CDC firmware stream."""

from __future__ import annotations

import argparse
import glob
import hashlib
import os
import struct
import termios
import time
import tty
from pathlib import Path


MAGIC = b"N3GR"
HEADER = struct.Struct("<4sIHHI")
FILE_SIZE = 854_812
BODY_CHUNK = 256
EXPECTED_SHA256 = (
    "a4d8fc899cf49e35552c2303e7605d0bf54170b44e37309b01a2acdde6066f5d"
)


def fnv1a(data: bytes) -> int:
    value = 2_166_136_261
    for byte in data:
        value ^= byte
        value = value * 16_777_619 & 0xFFFFFFFF
    return value


def expected_length(offset: int) -> int | None:
    if offset == 0:
        return 8
    if offset < 8 or (offset - 8) % BODY_CHUNK:
        return None
    if offset >= FILE_SIZE:
        return None
    return min(BODY_CHUNK, FILE_SIZE - offset)


def find_tty() -> str | None:
    candidates = sorted(glob.glob("/dev/ttyACM*"))
    return candidates[-1] if candidates else None


def open_raw(path: str) -> int:
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    tty.setraw(fd, termios.TCSANOW)
    termios.tcflush(fd, termios.TCIFLUSH)
    return fd


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--timeout", type=float, default=180.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    deadline = time.monotonic() + args.timeout
    fd = None
    path = None

    while time.monotonic() < deadline:
        path = find_tty()
        if path is not None:
            try:
                fd = open_raw(path)
                break
            except OSError:
                pass
        time.sleep(0.1)

    if fd is None:
        raise SystemExit("timed out waiting for the Nano 3G CDC device")

    print(f"N3GR_TTY {path}", flush=True)
    image = bytearray(FILE_SIZE)
    received: set[int] = set()
    expected_records = 1 + (FILE_SIZE - 8 + BODY_CHUNK - 1) // BODY_CHUNK
    stream = bytearray()
    last_report = -1

    try:
        while time.monotonic() < deadline and len(received) < expected_records:
            try:
                block = os.read(fd, 16_384)
            except BlockingIOError:
                block = b""
            if not block:
                time.sleep(0.002)
                continue
            stream.extend(block)

            while True:
                start = stream.find(MAGIC)
                if start < 0:
                    if len(stream) > len(MAGIC) - 1:
                        del stream[: -(len(MAGIC) - 1)]
                    break
                if start:
                    del stream[:start]
                if len(stream) < HEADER.size:
                    break

                _, offset, length, _pass, checksum = HEADER.unpack_from(stream)
                want = expected_length(offset)
                if want is None or length != want:
                    del stream[0]
                    continue
                frame_size = HEADER.size + length
                if len(stream) < frame_size:
                    break
                data = bytes(stream[HEADER.size:frame_size])
                if fnv1a(data) != checksum:
                    del stream[0]
                    continue
                del stream[:frame_size]

                image[offset:offset + length] = data
                received.add(offset)

                percent = len(received) * 100 // expected_records
                if percent >= last_report + 5:
                    print(
                        f"N3GR_PROGRESS records={len(received)}/"
                        f"{expected_records} percent={percent}",
                        flush=True,
                    )
                    last_report = percent
    finally:
        os.close(fd)

    if len(received) != expected_records:
        raise SystemExit(
            f"incomplete capture: {len(received)}/{expected_records} records"
        )

    data = bytes(image)
    digest = hashlib.sha256(data).hexdigest()
    parsed_checksum = int.from_bytes(data[:4], "big")
    computed_checksum = (117 + sum(data[8:])) & 0xFFFFFFFF
    if data[:8] != bytes.fromhex("054207646e6e3367"):
        raise SystemExit(f"bad firmware header: {data[:8].hex()}")
    if parsed_checksum != computed_checksum:
        raise SystemExit(
            f"checksum mismatch: {parsed_checksum:08x} != {computed_checksum:08x}"
        )
    if digest != EXPECTED_SHA256:
        raise SystemExit(f"sha256 mismatch: {digest}")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    print(f"N3GR_COMPLETE size={len(data)} sha256={digest} output={args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
