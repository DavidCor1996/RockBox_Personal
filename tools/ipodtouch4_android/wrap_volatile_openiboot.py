#!/usr/bin/env python3
"""Create and verify the minimal unsigned Image3 consumed by narrowed SHAtter."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct

from plan_ram_boot import PlanError, regular_bytes, verified_loader


ROOT_HEADER = struct.Struct("<4s3I4s")
TAG_HEADER = struct.Struct("<4s2I")
ROOT_MAGIC = b"3gmI"
DATA_MAGIC = b"ATAD"
IMAGE_TYPE = b"ssbi"
MAX_IMAGE3_BYTES = 0x2C000
LOADER_MAX_BYTES = MAX_IMAGE3_BYTES - ROOT_HEADER.size - TAG_HEADER.size


class Image3Error(RuntimeError):
    """Raised when an Image3 is malformed or exceeds the SHAtter RAM contract."""


def sha256_bytes(body: bytes) -> str:
    return hashlib.sha256(body).hexdigest()


def make_image3(payload: bytes) -> bytes:
    if not payload or len(payload) > LOADER_MAX_BYTES:
        raise Image3Error(
            f"loader must be between 1 and {LOADER_MAX_BYTES} bytes"
        )
    data_size = TAG_HEADER.size + len(payload)
    used_size = ROOT_HEADER.size + data_size
    total_size = (used_size + 63) & ~63
    header = ROOT_HEADER.pack(
        ROOT_MAGIC, total_size, data_size, data_size, IMAGE_TYPE
    )
    tag = TAG_HEADER.pack(DATA_MAGIC, data_size, len(payload))
    return header + tag + payload + bytes(total_size - used_size)


def payload_from_image3(image: bytes) -> bytes:
    if len(image) < ROOT_HEADER.size + TAG_HEADER.size:
        raise Image3Error("Image3 is truncated")
    magic, total_size, data_size, signed_size, image_type = ROOT_HEADER.unpack_from(
        image
    )
    if magic != ROOT_MAGIC or image_type != IMAGE_TYPE:
        raise Image3Error("Image3 has the wrong magic or type")
    if total_size != len(image) or total_size > MAX_IMAGE3_BYTES or total_size % 64:
        raise Image3Error("Image3 total size is invalid")
    if data_size != signed_size or ROOT_HEADER.size + data_size > total_size:
        raise Image3Error("Image3 signed data bounds are invalid")
    tag_magic, tag_total, tag_data = TAG_HEADER.unpack_from(image, ROOT_HEADER.size)
    if tag_magic != DATA_MAGIC or tag_total != data_size:
        raise Image3Error("Image3 does not contain exactly one DATA tag")
    if tag_data != tag_total - TAG_HEADER.size:
        raise Image3Error("Image3 DATA tag bounds are invalid")
    end = ROOT_HEADER.size + TAG_HEADER.size + tag_data
    if end != ROOT_HEADER.size + data_size or any(image[end:]):
        raise Image3Error("Image3 padding or tag extent is invalid")
    return image[ROOT_HEADER.size + TAG_HEADER.size : end]


def wrap(build: Path, output: Path) -> dict[str, object]:
    try:
        loader_report = verified_loader(build)
        payload = regular_bytes(
            build / "openiboot-n81-volatile.bin", "loader binary", LOADER_MAX_BYTES
        )
    except PlanError as error:
        raise Image3Error(str(error)) from error
    image = make_image3(payload)
    if payload_from_image3(image) != payload:
        raise Image3Error("Image3 payload round trip failed")
    output.mkdir(parents=True, exist_ok=False)
    image_path = output / "openiboot-n81-volatile.img3"
    image_path.write_bytes(image)
    report = {
        "schema": 1,
        "artifact_gate_passed": True,
        "profile": "n81-shatter-unsigned-image3-volatile-no-storage",
        "product_type": "iPod4,1",
        "hardware_model": "N81AP",
        "machine_id": 3564,
        "image_type": "ibss",
        "load_address": 0x84000000,
        "maximum_image3_bytes": MAX_IMAGE3_BYTES,
        "signature_bypass_required": "SHAtter",
        "persistent_device_writes_enabled": False,
        "hardware_actions_enabled": False,
        "device_test_ready": False,
        "artifacts": {
            "loader": {
                "size": len(payload),
                "sha256": sha256_bytes(payload),
            },
            "image3": {
                "filename": image_path.name,
                "size": len(image),
                "sha256": sha256_bytes(image),
            },
        },
        "loader_qualification_sha256": sha256_bytes(
            (build / "qualification.json").read_bytes()
        ),
        "loader_openiboot_commit": loader_report["openiboot_commit"],
    }
    (output / "qualification.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("build", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        report = wrap(args.build.resolve(), args.output.resolve())
    except (OSError, Image3Error) as error:
        parser.error(str(error))
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
