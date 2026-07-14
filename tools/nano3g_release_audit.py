#!/usr/bin/env python3
"""Audit a Nano 3G firmware/package pair and emit release manifests."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import subprocess
import sys
import tempfile

if __package__:
    from .nano3g_deploy import (
        DeployError,
        load_package,
        sha256_file,
        validate_firmware,
    )
else:
    from nano3g_deploy import DeployError, load_package, sha256_file, validate_firmware


FIRMWARE_PATH = PurePosixPath(".rockbox/rockbox.ipod")


class AuditError(RuntimeError):
    """A release artifact or build record is inconsistent."""


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def inspect_artifacts(firmware_path: Path, package_path: Path) -> tuple[dict, dict]:
    firmware, firmware_hash, checksum = validate_firmware(firmware_path)
    entries, package_hash = load_package(package_path)
    packaged_firmware = next(
        (entry.data for entry in entries if entry.relative == FIRMWARE_PATH), None
    )
    if packaged_firmware is None:
        raise AuditError("package is missing .rockbox/rockbox.ipod")
    if packaged_firmware != firmware:
        raise AuditError("package firmware does not match the explicit image")

    ordered = sorted(entries, key=lambda entry: str(entry.relative))
    plugins = [entry for entry in ordered if str(entry.relative).endswith(".rock")]
    codecs = [entry for entry in ordered if str(entry.relative).endswith(".codec")]

    def manifest(selected) -> str:
        return "".join(
            f"{sha256_bytes(entry.data)}  {entry.relative}\n" for entry in selected
        )

    report = {
        "firmware": {
            "path": str(firmware_path),
            "bytes": len(firmware),
            "sha256": firmware_hash,
            "model_tag": firmware[4:8].decode("ascii"),
            "checksum": f"{checksum:08x}",
        },
        "package": {
            "path": str(package_path),
            "bytes": package_path.stat().st_size,
            "sha256": package_hash,
            "files": len(ordered),
            "plugins": len(plugins),
            "codecs": len(codecs),
            "firmware_matches": True,
        },
    }
    manifests = {
        "package-files.sha256": manifest(ordered),
        "plugins.sha256": manifest(plugins),
        "codecs.sha256": manifest(codecs),
    }
    return report, manifests


def _run(command: list[str], cwd: Path) -> str:
    try:
        result = subprocess.run(
            command, cwd=cwd, check=True, text=True, capture_output=True
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        raise AuditError(f"command failed: {' '.join(command)}: {exc}") from exc
    return result.stdout.rstrip()


def source_record(repo: Path) -> dict:
    status = _run(
        ["git", "status", "--porcelain=v1", "--untracked-files=normal"], repo
    ).splitlines()
    return {
        "repo": str(repo.resolve()),
        "commit": _run(["git", "rev-parse", "HEAD"], repo),
        "branch": _run(["git", "branch", "--show-current"], repo),
        "dirty": bool(status),
        "status": status,
    }


def toolchain_record(repo: Path) -> dict:
    compilers = {}
    for name in ("arm-elf-eabi-gcc", "arm-none-eabi-g++"):
        try:
            compilers[name] = _run([name, "--version"], repo).splitlines()[0]
        except AuditError:
            compilers[name] = None
    return compilers


def build_record(build_dir: Path) -> dict:
    info_path = build_dir / "rockbox-info.txt"
    makefile_path = build_dir / "Makefile"
    if not info_path.is_file() or not makefile_path.is_file():
        raise AuditError(f"build directory is incomplete: {build_dir}")

    fields = {}
    for line in info_path.read_text(encoding="utf-8").splitlines():
        if ": " in line:
            key, value = line.split(": ", 1)
            fields[key] = value
    if fields.get("Target") != "ipodnano3g" or fields.get("Target id") != "117":
        raise AuditError("rockbox-info.txt is not a Nano 3G build")

    configure = next(
        (
            line.split("=", 1)[1].strip()
            for line in makefile_path.read_text(encoding="utf-8").splitlines()
            if line.startswith("CONFIGURE_OPTIONS=")
        ),
        None,
    )
    return {
        "directory": str(build_dir.resolve()),
        "clean_package_command": "make -j4 zip",
        "configure_options": configure,
        "rockbox_info_sha256": sha256_file(info_path),
        "rockbox_info": fields,
    }


def atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as handle:
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


def write_release_record(output_dir: Path, report: dict, manifests: dict) -> None:
    output_dir.mkdir(parents=True, exist_ok=False)
    for name, contents in manifests.items():
        atomic_write(output_dir / name, contents.encode("utf-8"))
    atomic_write(
        output_dir / "release-audit.json",
        (json.dumps(report, indent=2, sort_keys=True) + "\n").encode("utf-8"),
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firmware", required=True, type=Path)
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--output-dir", type=Path)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        report, manifests = inspect_artifacts(args.firmware, args.package)
        report.update(
            {
                "created_at_utc": datetime.now(timezone.utc).isoformat(),
                "source": source_record(args.repo),
                "toolchains": toolchain_record(args.repo),
                "build": build_record(args.build_dir),
            }
        )
        if args.output_dir is not None:
            write_release_record(args.output_dir, report, manifests)
            report["record_directory"] = str(args.output_dir)
    except (AuditError, DeployError, OSError, ValueError) as exc:
        print(f"N3G_AUDIT_FAIL {exc}", file=sys.stderr)
        return 1
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
