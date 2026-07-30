#!/usr/bin/env python3
"""Reject commercial/private Maker Lite payloads from repository packaging."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
PRIVATE_SUFFIXES = {".sfc", ".smc", ".gen", ".bin"}
FORBIDDEN_MARKERS = (b"MLAR", b"RPML")


def bundled_original_payloads(root: Path) -> tuple[set[str], list[str]]:
    pack_root = root / "assets/maker_lite/neon_nook/pack"
    index_path = pack_root / "pack.json"
    kit_path = pack_root / "kit.mlk"
    failures: list[str] = []
    try:
        index = json.loads(index_path.read_text(encoding="utf-8"))
        kit = json.loads(kit_path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        return set(), [f"bundled original pack metadata is unreadable: {error}"]
    if (
        index.get("kit_id") != "zelda-neon-nook-v1"
        or kit.get("kit_id") != index.get("kit_id")
        or kit.get("source", {}).get("type") != "original-generated"
    ):
        failures.append("bundled Neon Nook pack is not declared original-generated")
    allowed: set[str] = set()
    for name, expected in index.get("files", {}).items():
        path = pack_root / name
        try:
            actual = hashlib.sha256(path.read_bytes()).hexdigest()
        except OSError as error:
            failures.append(f"bundled original pack file is unreadable: {path}: {error}")
            continue
        if actual != expected:
            failures.append(f"bundled original pack digest mismatch: {path}")
            continue
        allowed.add(path.relative_to(root).as_posix())
    return allowed, failures


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=REPO)
    args = parser.parse_args()
    root = args.root.resolve()
    candidates = subprocess.run(
        [
            "git", "ls-files", "-z", "--cached", "--others",
            "--exclude-standard", "--",
            "apps/plugins/maker_lite",
            "lib/maker_lite",
            "rockpod/services",
            "rockpod/ui",
            "rockpod/tests",
            "tools",
            "docs",
            "testdata/maker_lite",
            "assets/maker_lite",
        ],
        cwd=root,
        check=True,
        capture_output=True,
    ).stdout.split(b"\0")
    allowed_original, failures = bundled_original_payloads(root)
    for raw in candidates:
        if not raw:
            continue
        relative = Path(raw.decode())
        text = relative.as_posix()
        if "maker_lite" not in text and "maker-lite" not in text:
            continue
        if (
            text.startswith("testdata/maker_lite/")
            and relative.suffix.lower() == ".json"
        ):
            continue
        path = root / relative
        suffix = relative.suffix.lower()
        if suffix in PRIVATE_SUFFIXES:
            failures.append(f"commercial ROM-like file is packaged: {text}")
        if path.is_file() and path.stat().st_size <= 16 * 1024 * 1024:
            data = path.read_bytes()
            if (
                suffix == ".md"
                and len(data) >= 0x104
                and data[0x100:0x104] == b"SEGA"
            ):
                failures.append(f"Genesis ROM-like file is packaged: {text}")
            if any(marker in data for marker in FORBIDDEN_MARKERS):
                if (
                    suffix in {".mla", ".mlp", ".mlk"}
                    and text not in allowed_original
                ):
                    failures.append(f"compiled private payload is packaged: {text}")
    if failures:
        raise SystemExit("\n".join(failures))
    print(
        "PASS: no Maker Lite commercial ROM or private compiled kit is packaged; "
        f"{len(allowed_original)} hashed original Neon Nook files verified"
    )


if __name__ == "__main__":
    main()
