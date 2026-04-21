#!/usr/bin/env python3
"""Print a disk-vs-DB library reconciliation report."""

import argparse
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

from app.config import Config
from app.database import Database
from services.library_scanner import (
    build_library_reconciliation_report,
    format_library_reconciliation_report,
)


def parse_args():
    parser = argparse.ArgumentParser(description="RockPod library reconciliation report")
    parser.add_argument("--config", help="Config file path")
    parser.add_argument("--db", help="Database path")
    parser.add_argument("--music-dir", help="Music directory to scan")
    parser.add_argument("--video-dir", action="append", help="Video directory to scan; repeat for multiple roots")
    parser.add_argument("--limit", type=int, default=25, help="Number of examples to print")
    return parser.parse_args()


def main():
    args = parse_args()
    config = Config(args.config)
    if args.db:
        config.db_path = args.db
    if args.music_dir:
        config.music_dir = args.music_dir
    if args.video_dir:
        config.video_dirs = args.video_dir

    db = Database(config.db_path)
    try:
        report = build_library_reconciliation_report(
            db,
            config.music_dir,
            video_dirs=config.video_dirs,
            sample_limit=args.limit,
        )
        print(format_library_reconciliation_report(report))
    finally:
        db.close()


if __name__ == "__main__":
    main()
