#!/usr/bin/env python3
"""Print a local-vs-device reconciliation report."""

import argparse
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

from app.config import Config
from app.database import Database
from services.device_detector import DeviceInfo
from services.device_inventory import verify_device_inventory
from services.reconciliation import (
    build_reconciliation_report,
    format_reconciliation_report,
)


def parse_args():
    parser = argparse.ArgumentParser(description="RockPod device reconciliation report")
    parser.add_argument("--config", help="Config file path")
    parser.add_argument("--db", help="Database path")
    parser.add_argument("--device-path", help="Mounted iPod path to verify before reporting")
    parser.add_argument("--device-key", help="Existing stable device key to report")
    parser.add_argument("--force", action="store_true", help="Force metadata re-read while verifying")
    parser.add_argument("--limit", type=int, default=25, help="Number of examples to print")
    return parser.parse_args()


def main():
    args = parse_args()
    config = Config(args.config)
    if args.db:
        config.db_path = args.db

    db = Database(config.db_path)
    skipped = []
    device_key = args.device_key
    try:
        if args.device_path:
            device = DeviceInfo(args.device_path)
            summary = verify_device_inventory(
                db,
                device,
                config.duplicate_strictness,
                args.force,
                duration_tolerance=config.get("duration_match_tolerance_seconds", 2.0),
            )
            device_key = summary["device_key"]
            skipped = summary.get("skipped", [])

        report = build_reconciliation_report(
            db,
            device_key,
            config.duplicate_strictness,
            config.get("duration_match_tolerance_seconds", 2.0),
            args.limit,
            skipped,
        )
        print(format_reconciliation_report(report))
    finally:
        db.close()


if __name__ == "__main__":
    main()
