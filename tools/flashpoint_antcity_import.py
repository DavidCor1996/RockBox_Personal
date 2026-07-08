#!/usr/bin/env python3
"""Extract Ant City SWF and metadata from a Flashpoint archive."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import zipfile


FLASHPOINT_UNIQUE_ID = "97db526b-abaf-4421-afab-59cc85a87d73"
CANONICAL_SWF_NAME = "antcity.swf"


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def find_swf_in_dir(root: Path) -> Path:
    candidates = sorted(root.rglob("*.swf"))
    if not candidates:
        raise SystemExit(f"no SWF in {root}")
    for candidate in candidates:
        if "Burn_their_ass" in candidate.name:
            return candidate
    return candidates[0]


def parse_content_json(path: Path) -> dict[str, object]:
    if not path.exists():
        return {}
    try:
        with path.open("r", encoding="utf-8") as fh:
            data = json.load(fh)
    except (OSError, ValueError):
        return {}
    return data if isinstance(data, dict) else {}


def copy_with_manifest(source_swf: Path, content_json: dict[str, object] | None,
                      output_dir: Path, source_label: str) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    target_swf = output_dir / CANONICAL_SWF_NAME
    shutil.copy2(source_swf, target_swf)

    manifest = {
        "title": "Ant City",
        "source_label": source_label,
        "source_unique_id": content_json.get("uniqueId") if content_json else None,
        "content_version": content_json.get("version") if content_json else None,
        "source_swf": str(source_swf),
        "source_swf_size": source_swf.stat().st_size,
        "source_swf_sha256": sha256_file(source_swf),
        "output_swf": str(target_swf),
        "output_swf_size": target_swf.stat().st_size,
        "output_swf_sha256": sha256_file(target_swf),
    }
    if source_json := content_json:
        manifest["content_json"] = source_json

    with (output_dir / "antcity.manifest.json").open("w", encoding="utf-8") as fh:
        json.dump(manifest, fh, indent=2, sort_keys=True)

    print(f"ANTCITY_WROTE_SWF={target_swf}")
    print(f"ANTCITY_WROTE_MANIFEST={output_dir / 'antcity.manifest.json'}")


def extract_from_zip(archive: Path, output_dir: Path) -> None:
    with zipfile.ZipFile(archive) as zf:
        names = zf.namelist()
        content_path = next(
            (name for name in names if name.endswith("/content.json")), None
        )
        if content_path is not None:
            with zf.open(content_path) as fh:
                raw = fh.read().decode("utf-8", "replace")
            # Write a temporary copy so we can reuse filesystem helpers.
            tmp_root = output_dir.parent / f".{archive.stem}_meta"
            tmp_root.mkdir(parents=True, exist_ok=True)
            json_path = tmp_root / "content.json"
            json_path.write_text(raw, encoding="utf-8")
            content_json = parse_content_json(json_path)
            json_path.unlink()
            tmp_root.rmdir()
        else:
            content_json = {}

        swf_candidates = [
            name for name in names
            if name.lower().endswith(".swf") and "content/" in name
        ]
        if not swf_candidates:
            raise SystemExit(f"no SWF in zip {archive}")

        swf_name = next(
            (name for name in swf_candidates if "Burn_their_ass" in name),
            swf_candidates[0],
        )
        tmp_root = output_dir.parent / f".{archive.stem}_tmp"
        tmp_root.mkdir(parents=True, exist_ok=True)
        target = tmp_root / "antcity.swf"
        try:
            with zf.open(swf_name) as in_fh:
                with target.open("wb") as out_fh:
                    out_fh.write(in_fh.read())
            if content_json.get("uniqueId") not in (None, "", FLASHPOINT_UNIQUE_ID):
                raise SystemExit(
                    "archive content uniqueId does not match expected Ant City id"
                )
            copy_with_manifest(target, content_json, output_dir, f"zip:{archive}")
        finally:
            if target.exists():
                target.unlink()
            if tmp_root.exists():
                tmp_root.rmdir()


def extract_from_dir(root: Path, output_dir: Path) -> None:
    if not root.is_dir():
        raise SystemExit(f"{root} is not a directory")

    source_swf = find_swf_in_dir(root)
    content_json = None
    content = root / "content.json"
    if content.exists():
        content_json = parse_content_json(content)
        if content_json.get("uniqueId") not in (None, "", FLASHPOINT_UNIQUE_ID):
            raise SystemExit("directory content uniqueId does not match expected Ant City id")
    elif root.parent.joinpath("content.json").exists():
        content_json = parse_content_json(root.parent / "content.json")
        if content_json.get("uniqueId") not in (None, "", FLASHPOINT_UNIQUE_ID):
            raise SystemExit("directory content uniqueId does not match expected Ant City id")

    copy_with_manifest(
        source_swf,
        content_json,
        output_dir,
        f"dir:{root}",
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Import Ant City from a Flashpoint package."
    )
    parser.add_argument(
        "--input",
        required=True,
        help="Flashpoint zip or extracted directory containing content.json + SWF",
    )
    parser.add_argument(
        "--output",
        required=True,
        help="Destination folder for .rockbox/flash/antcity artifacts",
    )
    args = parser.parse_args()

    src = Path(args.input).expanduser()
    out = Path(args.output).expanduser()
    if not src.exists():
        raise SystemExit(f"input not found: {src}")

    if src.suffix.lower() == ".zip":
        extract_from_zip(src, out)
    elif src.is_dir():
        extract_from_dir(src, out)
    else:
        raise SystemExit(f"unsupported input type: {src}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
