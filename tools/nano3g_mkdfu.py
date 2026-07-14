#!/usr/bin/env python3
"""Build and audit transient iPod Nano 3G wInd3x DFU payloads.

This intentionally supports only the volatile type-2 execute wrapper used by
the Nano 3G bring-up.  Persistent installer (type 3) images are outside this
tool's scope and are rejected by the verifier.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import tempfile


SIGNATURE = b"87021.0"
HEADER_SIZE = 0x800
TRANSIENT_FORMAT = 2
MAX_BODY_SIZE = 0x20000
LENGTH_OFFSETS = (0x0C, 0x10, 0x14)


class DfuError(ValueError):
    """Raised when a payload is not a safe transient Nano 3G wrapper."""


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def build_wrapper(body: bytes) -> bytes:
    if not body:
        raise DfuError("bootloader body is empty")
    if len(body) > MAX_BODY_SIZE:
        raise DfuError(
            f"bootloader body is {len(body)} bytes; transient IRAM limit is "
            f"{MAX_BODY_SIZE} bytes"
        )

    header = bytearray(HEADER_SIZE)
    header[: len(SIGNATURE)] = SIGNATURE
    header[7] = TRANSIENT_FORMAT
    for offset in LENGTH_OFFSETS:
        struct.pack_into("<I", header, offset, len(body))
    return bytes(header) + body


def inspect_wrapper(wrapper: bytes, expected_body: bytes | None = None) -> dict:
    if len(wrapper) <= HEADER_SIZE:
        raise DfuError("wrapper has no bootloader body")
    if wrapper[:7] != SIGNATURE:
        raise DfuError("unexpected DFU signature")
    if wrapper[7] != TRANSIENT_FORMAT:
        raise DfuError(
            f"DFU format is {wrapper[7]}; only volatile type 2 is allowed"
        )

    lengths = tuple(struct.unpack_from("<I", wrapper, off)[0]
                    for off in LENGTH_OFFSETS)
    if lengths[0] == 0 or len(set(lengths)) != 1:
        raise DfuError(f"inconsistent body lengths: {lengths}")
    body_size = lengths[0]
    if body_size > MAX_BODY_SIZE:
        raise DfuError(
            f"declared body is {body_size} bytes; transient IRAM limit is "
            f"{MAX_BODY_SIZE} bytes"
        )
    if len(wrapper) != HEADER_SIZE + body_size:
        raise DfuError(
            f"wrapper size is {len(wrapper)}, expected "
            f"{HEADER_SIZE + body_size}"
        )
    if any(wrapper[0x08:0x0C]) or any(wrapper[0x18:HEADER_SIZE]):
        raise DfuError("reserved header bytes are not zero")

    body = wrapper[HEADER_SIZE:]
    if expected_body is not None and body != expected_body:
        raise DfuError("wrapped body does not match the expected bootloader")

    return {
        "format": TRANSIENT_FORMAT,
        "header_bytes": HEADER_SIZE,
        "body_bytes": body_size,
        "wrapper_bytes": len(wrapper),
        "body_sha256": sha256(body),
        "wrapper_sha256": sha256(wrapper),
        "persistent": False,
    }


def atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.",
                                     dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as handle:
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, path)
    except BaseException:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def command_build(args: argparse.Namespace) -> dict:
    body = args.body.read_bytes()
    wrapper = build_wrapper(body)
    report = inspect_wrapper(wrapper, body)
    atomic_write(args.output, wrapper)
    report.update({"body": str(args.body), "output": str(args.output)})
    return report


def command_verify(args: argparse.Namespace) -> dict:
    expected = args.body.read_bytes() if args.body is not None else None
    report = inspect_wrapper(args.wrapper.read_bytes(), expected)
    report.update({"wrapper": str(args.wrapper)})
    if args.body is not None:
        report["body"] = str(args.body)
    return report


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    build = subparsers.add_parser(
        "build", help="create a volatile type-2 DFU execute wrapper")
    build.add_argument("body", type=Path, help="raw bootloader.bin")
    build.add_argument("output", type=Path, help="output .dfu path")
    build.set_defaults(function=command_build)

    verify = subparsers.add_parser(
        "verify", help="audit an existing wrapper without changing it")
    verify.add_argument("wrapper", type=Path, help="type-2 .dfu path")
    verify.add_argument(
        "--body", type=Path,
        help="also require an exact match with this raw bootloader")
    verify.set_defaults(function=command_verify)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        report = args.function(args)
    except (DfuError, OSError) as error:
        raise SystemExit(f"error: {error}") from error
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
