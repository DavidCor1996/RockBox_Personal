#!/usr/bin/env python3
"""Verify the exact Apple Classic 35.2.0.4 reference firmware bundle.

This proves the identity and extraction boundary of the encrypted OSOS input.
It does not claim that an SVID register sequence has been re-derived: decrypting
OSOS still requires a compatible iPod and wInd3x.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import sys
import zipfile


IPSW_SIZE = 61_118_350
IPSW_SHA1 = "94fc68f5aad63a5dc8cf8b62408907ec050a4bc7"
IPSW_SHA256 = "7ef835c74b08f0bda3566001496cb764afbe0600cb1afec1145c259bc34ad7d0"
BUNDLE_NAME = "Firmware-35.9.0.4"
BUNDLE_SIZE = 93_601_792
BUNDLE_SHA256 = "0b36bdb4b93685a4cb24153201a96f44124d9f50eb7c6239e203d3e917405942"
OSOS_DIRECTORY_OFFSET = 0x5028
OSOS_OFFSET = 0x04E07000
OSOS_SIZE = 0x00A1BA53
OSOS_SHA256 = "e48ead9c3d68a39861622a9df9f584c26176683489e9fb84fd2c38f3a8f57678"


def digest(data: bytes, algorithm: str) -> str:
    return hashlib.new(algorithm, data).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def verify(path: Path) -> bytes:
    ipsw = path.read_bytes()
    require(len(ipsw) == IPSW_SIZE, f"IPSW size is {len(ipsw)}, expected {IPSW_SIZE}")
    require(digest(ipsw, "sha1") == IPSW_SHA1, "IPSW SHA-1 mismatch")
    require(digest(ipsw, "sha256") == IPSW_SHA256, "IPSW SHA-256 mismatch")

    with zipfile.ZipFile(path) as archive:
        names = set(archive.namelist())
        require(BUNDLE_NAME in names, f"missing {BUNDLE_NAME}")
        require("manifest.plist" in names, "missing manifest.plist")
        bundle = archive.read(BUNDLE_NAME)

    require(len(bundle) == BUNDLE_SIZE, "firmware bundle size mismatch")
    require(digest(bundle, "sha256") == BUNDLE_SHA256, "firmware bundle SHA-256 mismatch")
    require(bundle[:4] == b"{{~~", "firmware partition header is not Apple format")
    require(bundle[0x100:0x104] == b"]ih[", "firmware directory header is missing")

    entry = bundle[OSOS_DIRECTORY_OFFSET:OSOS_DIRECTORY_OFFSET + 40]
    require(entry[:8] == b"!ATAsoso", "OSOS directory entry moved or is invalid")
    require(int.from_bytes(entry[12:16], "little") == OSOS_OFFSET,
            "OSOS directory offset mismatch")
    require(int.from_bytes(entry[16:20], "little") == OSOS_SIZE,
            "OSOS directory size mismatch")
    require(int.from_bytes(entry[20:24], "little") == 0x08000000,
            "OSOS directory load address mismatch")

    osos = bundle[OSOS_OFFSET:OSOS_OFFSET + OSOS_SIZE]
    require(len(osos) == OSOS_SIZE, "OSOS extraction is truncated")
    require(osos[:8] == b"87021.0\x03", "OSOS IMG1 header mismatch")
    require(digest(osos, "sha256") == OSOS_SHA256, "encrypted OSOS SHA-256 mismatch")
    return osos


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ipsw", type=Path, help="official iPod_35.2.0.4.ipsw")
    parser.add_argument("--extract-osos", type=Path,
                        help="optionally write the verified encrypted OSOS container")
    args = parser.parse_args()

    try:
        osos = verify(args.ipsw)
        if args.extract_osos:
            args.extract_osos.write_bytes(osos)
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        print(f"stock firmware verify: FAIL: {error}", file=sys.stderr)
        return 1

    print(f"IPSW sha256   {IPSW_SHA256}")
    print(f"bundle sha256 {BUNDLE_SHA256}")
    print(f"OSOS sha256   {OSOS_SHA256}")
    print(f"OSOS range    0x{OSOS_OFFSET:08x}+0x{OSOS_SIZE:08x}")
    if args.extract_osos:
        print(f"OSOS written  {args.extract_osos}")
    print("stock firmware verify: identity and encrypted extraction boundary PASS")
    print("stock firmware verify: SVID provenance remains UNQUALIFIED until OSOS is decrypted")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
