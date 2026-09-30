#!/usr/bin/env python3
"""Prepare the optional 640x480 profile from verified owned Snow Leopard assets.

Leaves the existing pack and its manifest unchanged. Source hashes must match
that pack before any conversion; the supplemental manifest records provenance.
"""
import argparse
from dataclasses import replace
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "rockpod"))
from services import snow_leopard_assets as assets


def prepare(pack: Path, destination: Path):
    manifest = json.loads((pack / "manifest.json").read_text())
    profile = replace(assets.PANEL_PROFILES[0], key="640x480", width=640,
        height=480, window_w=608, window_h=382, sidebar_w=110,
        itunes_w=608, itunes_h=382, itunes_source_w=110,
        list_selection_w=497, scroller_h=305)
    records = {}
    for spec in assets._profile_specs(profile):
        original = manifest["assets"][spec.asset_id.replace("640x480", "320x240")]
        source = Path(original["source_root"]) / original["source_path"]
        if assets.sha256(source) != original["source_sha256"]:
            raise ValueError(f"Owned source changed: {source}")
        target = destination / spec.output
        result = assets._convert(source, target, spec)
        records[spec.asset_id] = {**result, "path": spec.output,
            "source": str(source), "source_sha256": assets.sha256(source),
            "output_sha256": assets.sha256(target)}
    (destination / "desktop640-provenance.json").write_text(json.dumps({
        "personal_use_only": True, "assets": records}, indent=2) + "\n")
    print(f"Prepared {len(records)} assets in {destination}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--pack", type=Path, default=assets.DEFAULT_PACK_DIR)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    prepare(args.pack, args.output)
