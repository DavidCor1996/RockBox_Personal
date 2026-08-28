#!/usr/bin/env python3
"""Read-only target selection for the iPod touch 4G Android port."""

from __future__ import annotations

import argparse
import hashlib
import json
import secrets
import subprocess
from pathlib import Path


SUPPORTED_PRODUCT = "iPod4,1"
SUPPORTED_HARDWARE = "N81AP"


class DeviceProbeError(RuntimeError):
    """Raised when the connected device cannot enter this port workflow."""


def parse_ideviceinfo(text: str) -> dict[str, str]:
    properties: dict[str, str] = {}
    for raw_line in text.splitlines():
        key, separator, value = raw_line.partition(": ")
        if separator and key and value:
            properties[key] = value
    return properties


def qualify_device(properties: dict[str, str]) -> dict[str, object]:
    product = properties.get("ProductType", "")
    hardware = properties.get("HardwareModel", "")
    version = properties.get("ProductVersion", "")
    if product != SUPPORTED_PRODUCT or hardware != SUPPORTED_HARDWARE:
        found = "/".join(value or "unknown" for value in (product, hardware))
        raise DeviceProbeError(
            f"unsupported device {found}; this port is only for "
            f"{SUPPORTED_PRODUCT}/{SUPPORTED_HARDWARE}"
        )
    if not version:
        raise DeviceProbeError("connected device did not report ProductVersion")
    return {
        "qualified": True,
        "product_type": product,
        "hardware_model": hardware,
        "ios_version": version,
        "port_profile": "ipodtouch4g-eclair-volatile-no-storage",
        "persistent_device_writes_enabled": False,
        "device_boot_enabled": False,
        "next_gate": "signed-recovery volatile loader and RAM-only Linux smoke",
    }


def canonical_chip_id(value: str) -> str:
    try:
        number = int(value, 0) if value.lower().startswith("0x") else int(value, 10)
    except ValueError as error:
        raise DeviceProbeError("connected device reported an invalid UniqueChipID") from error
    if number <= 0 or number >= 1 << 64:
        raise DeviceProbeError("connected device reported an out-of-range UniqueChipID")
    return f"{number:x}"


def make_continuity_bundle(
    properties: dict[str, str], nonce: bytes | None = None
) -> dict[str, object]:
    report = qualify_device(properties)
    chip_id = canonical_chip_id(properties.get("UniqueChipID", ""))
    actual_nonce = secrets.token_bytes(32) if nonce is None else nonce
    if len(actual_nonce) != 32:
        raise DeviceProbeError("continuity nonce must be exactly 32 bytes")
    digest = hashlib.sha256(actual_nonce + chip_id.encode("ascii")).hexdigest()
    return {
        "schema": 1,
        "qualified": True,
        "product_type": report["product_type"],
        "hardware_model": report["hardware_model"],
        "ios_version": report["ios_version"],
        "chip_id_encoding": "lowercase-hex-without-leading-zeroes",
        "nonce": actual_nonce.hex(),
        "chip_id_sha256": digest,
        "raw_device_identifier_retained": False,
        "persistent_device_writes_enabled": False,
        "device_boot_enabled": False,
    }


def read_connected_device() -> dict[str, str]:
    result = subprocess.run(
        ["ideviceinfo"], text=True, capture_output=True, check=False, timeout=10
    )
    if result.returncode != 0:
        detail = result.stderr.strip() or "ideviceinfo found no paired device"
        raise DeviceProbeError(detail)
    return parse_ideviceinfo(result.stdout)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input",
        type=Path,
        help="parse saved ideviceinfo output instead of querying a device",
    )
    parser.add_argument(
        "--continuity-output",
        type=Path,
        help="write a salted one-way N81-to-DFU continuity bundle",
    )
    args = parser.parse_args()
    try:
        if args.input:
            properties = parse_ideviceinfo(args.input.read_text(encoding="utf-8"))
        else:
            properties = read_connected_device()
        report = qualify_device(properties)
        if args.continuity_output:
            bundle = make_continuity_bundle(properties)
            args.continuity_output.write_text(
                json.dumps(bundle, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
            report["dfu_continuity_bundle_written"] = True
    except (OSError, subprocess.SubprocessError, DeviceProbeError) as error:
        parser.error(str(error))
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
