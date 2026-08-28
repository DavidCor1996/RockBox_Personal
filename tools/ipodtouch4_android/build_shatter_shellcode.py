#!/usr/bin/env python3
"""Build and qualify only the narrowed SHAtter volatile DFU shellcode."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

from prepare_shatter_transport import MARKER, PATCH_PATH, sha256


HERE = Path(__file__).resolve().parent
LOCK_PATH = HERE / "source-lock.json"
SHELLCODE_MAX = 1024


class BuildError(RuntimeError):
    """Raised when the narrowed shellcode does not satisfy its safety contract."""


def build(source: Path, output: Path, prefix: str) -> dict[str, object]:
    lock = json.loads(LOCK_PATH.read_text(encoding="utf-8"))["dfu_transport"]
    try:
        marker = json.loads((source / MARKER).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise BuildError(f"prepared-source marker is invalid: {error}") from error
    expected_marker = {
        "archive_sha256": lock["archive_sha256"],
        "commit": lock["commit"],
        "patch_sha256": sha256(PATCH_PATH),
        "profile": "n81-shatter-volatile-no-storage",
    }
    for name, expected in expected_marker.items():
        if marker.get(name) != expected:
            raise BuildError(f"prepared source has unexpected {name}")

    assembly_path = source / "src" / "SHAtter-shellcode.S"
    try:
        assembly = assembly_path.read_text(encoding="utf-8")
    except OSError as error:
        raise BuildError(f"cannot read shellcode source: {error}") from error
    required = {
        ".set LOAD_ADDRESS,                  0x84000000",
        ".set MAX_SIZE,                         0x2c000",
        "PWND:[SHAtter]",
        "image3_load_no_signature_check",
        "jump_to(0, LOAD_ADDRESS, 0)",
    }
    missing = sorted(marker for marker in required if marker not in assembly)
    if missing:
        raise BuildError(f"shellcode lacks volatile loader contract: {missing}")
    if re.search(r"\bnor_(?:power_on|init)\b", assembly):
        raise BuildError("shellcode retains a NOR initialization call")

    output.mkdir(parents=True, exist_ok=False)
    object_path = output / "SHAtter-shellcode.o"
    binary_path = output / "SHAtter-shellcode-n81-no-storage.bin"
    try:
        subprocess.run(
            [prefix + "as", "-mthumb", "--fatal-warnings", "-o", str(object_path), str(assembly_path)],
            check=True,
        )
        subprocess.run(
            [prefix + "objcopy", "-O", "binary", str(object_path), str(binary_path)],
            check=True,
        )
        object_path.unlink()
        body = binary_path.read_bytes()
    except (OSError, subprocess.CalledProcessError) as error:
        raise BuildError(f"shellcode build failed: {error}") from error
    if not body or len(body) > SHELLCODE_MAX:
        raise BuildError(f"shellcode size {len(body)} exceeds {SHELLCODE_MAX} bytes")

    report = {
        "schema": 1,
        "artifact_gate_passed": True,
        "profile": "n81-shatter-volatile-no-storage",
        "upstream_commit": lock["commit"],
        "cpid": lock["cpid"],
        "secure_rom": lock["secure_rom"],
        "load_address": lock["load_address"],
        "maximum_image3_bytes": lock["maximum_image3_bytes"],
        "shellcode_maximum_bytes": SHELLCODE_MAX,
        "nor_initialization_present": False,
        "persistent_write_surface_present": False,
        "upstream_cli_staged": False,
        "hardware_actions_enabled": False,
        "device_test_ready": False,
        "physical_dfu_tested": False,
        "artifact": {
            "filename": binary_path.name,
            "size": len(body),
            "sha256": hashlib.sha256(body).hexdigest(),
        },
    }
    (output / "qualification.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--cross-prefix", default="arm-elf-eabi-")
    args = parser.parse_args()
    try:
        report = build(args.source.resolve(), args.output.resolve(), args.cross_prefix)
    except BuildError as error:
        parser.error(str(error))
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
