#!/usr/bin/env python3
"""Prepare or validate a private Snow Leopard Desktop Mode asset pack."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "rockpod"))

from services.snow_leopard_assets import (  # noqa: E402
    DEFAULT_PACK_DIR,
    SnowLeopardAssetError,
    build_pack,
    discover_assets,
    install_pack,
    validate_pack,
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source",
        action="append",
        default=[],
        help="owned 10.6 system/capture root; repeat to merge sources",
    )
    parser.add_argument("--output", default=str(DEFAULT_PACK_DIR))
    parser.add_argument("--validate", action="store_true")
    parser.add_argument("--install-device", default="")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    try:
        if args.validate:
            report = validate_pack(args.output)
        elif args.source:
            discovery = discover_assets(args.source)
            if not discovery["complete"]:
                report = discovery
            else:
                report = build_pack(args.source, args.output)
        else:
            parser.error("provide --source or --validate")
        success = bool(report.get("complete", report.get("valid", False)))
        if args.install_device:
            report = {
                "pack": report,
                "install": install_pack(args.output, args.install_device),
            }
            success = bool(report["install"].get("success"))
    except SnowLeopardAssetError as exc:
        if args.json:
            print(json.dumps({"success": False, "error": str(exc)}, indent=2))
        else:
            print(f"error: {exc}", file=sys.stderr)
        return 1

    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
    elif "install" in report:
        install = report["install"]
        print(
            f"Installed {install['asset_count']} verified assets to "
            f"{len(install['destinations'])} Desktop Mode data locations."
        )
        if install.get("replaced_xp"):
            print(
                f"Archived and removed {len(install['replaced_xp'])} "
                "legacy XP asset locations."
            )
    else:
        if "resolved_count" in report:
            print(
                f"Resolved {report['resolved_count']}/{report['required_count']} "
                "required real Snow Leopard assets."
            )
            if report["missing"]:
                print("Missing: " + ", ".join(report["missing"]))
        else:
            print(
                f"Snow Leopard pack {'valid' if report.get('valid') else 'invalid'}: "
                f"{report.get('root', args.output)}"
            )
            for error in report.get("errors", []):
                print(f"- {error}")
    return 0 if success else 2


if __name__ == "__main__":
    raise SystemExit(main())
