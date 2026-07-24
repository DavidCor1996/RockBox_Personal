#!/usr/bin/env python3
"""Verify the pinned Java 6 host toolchain used by Eclair's ECJ build."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


LOCK = Path(__file__).resolve().parent / "eclair/host-toolchain-lock.json"


class VerificationError(RuntimeError):
    """The supplied host JDK does not match the pinned toolchain."""


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def regular(path: Path, description: str) -> Path:
    if path.is_symlink() or not path.is_file():
        raise VerificationError(f"{description} is not a regular file")
    return path.resolve()


def verify(archive: Path, jdk: Path) -> dict:
    lock = json.loads(LOCK.read_text(encoding="utf-8"))
    if lock.get("schema") != 1 or lock.get("java_version") != "1.6.0-119":
        raise VerificationError("unsupported host-toolchain lock")
    archive = regular(archive, "JDK archive")
    expected = lock["archive"]
    if archive.stat().st_size != expected["size"]:
        raise VerificationError("JDK archive size mismatch")
    actual_sha = sha256(archive)
    if actual_sha != expected["sha256"]:
        raise VerificationError("JDK archive checksum mismatch")
    java = regular(jdk / "bin/java", "java")
    javac = regular(jdk / "bin/javac", "javac")
    java_text = subprocess.run(
        [str(java), "-version"], text=True, capture_output=True,
        timeout=5, check=True,
    ).stderr.strip()
    javac_text = subprocess.run(
        [str(javac), "-version"], text=True, capture_output=True,
        timeout=5, check=True,
    ).stderr.strip()
    if 'openjdk version "1.6.0-119"' not in java_text:
        raise VerificationError("java runtime version mismatch")
    if javac_text != "javac 1.6.0-119":
        raise VerificationError("javac version mismatch")
    return {
        "schema": 1,
        "vendor": lock["vendor"],
        "distribution": lock["distribution"],
        "java_version": lock["java_version"],
        "package_version": lock["package_version"],
        "archive": {
            "file": archive.name,
            "size": archive.stat().st_size,
            "sha256": actual_sha,
            "uuid": expected["uuid"],
        },
        "jdk_directory": str(jdk.resolve()),
        "java_version_output": java_text.splitlines(),
        "javac_version_output": javac_text,
        "network_used": False,
        "hardware_accessed": False,
        "toolchain_gate_passed": True,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("jdk", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        report = verify(args.archive, args.jdk)
        if args.output:
            output = args.output.resolve()
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(
                json.dumps(report, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
    except (OSError, json.JSONDecodeError, subprocess.SubprocessError,
            VerificationError) as error:
        parser.exit(1, f"verify_eclair_host_jdk: {error}\n")
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
