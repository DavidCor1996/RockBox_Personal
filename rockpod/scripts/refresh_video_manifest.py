#!/usr/bin/env python3
"""Rewrite the iPod's video manifest and posters from the repaired library.

A metadata repair only reaches the device once ``.rockbox/videolist/index.tsv``
and the poster bitmaps beside it are rebuilt. A full sync would do that, but it
also walks every source file; this rebuilds just the manifest and its artwork
for the videos already present on the mount.
"""

import argparse
import os
import shutil
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from app.config import Config  # noqa: E402
from app.database import Database  # noqa: E402
from services.device_detector import DeviceDetector, DeviceInfo  # noqa: E402
from services.sync_engine import SyncEngine, SyncPlan  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", required=True, help="mounted iPod path")
    parser.add_argument("--db", default="", help="library database (default: configured)")
    parser.add_argument("--apply", action="store_true", help="write to the device")
    args = parser.parse_args()

    config = Config()
    if args.db:
        config.db_path = args.db
    db = Database(config.db_path)
    device = DeviceInfo(args.device)
    detector = DeviceDetector(config)
    detector._current_device = device

    engine = SyncEngine(db, config, detector)
    engine._current_device_key = device.stable_device_key or ""

    plan = SyncPlan()
    engine._populate_video_list_artwork_sync_plan(plan, [], args.device, {})

    copies = list(plan.artwork_to_copy) + list(plan.generated_to_copy)
    print(f"manifest + artwork files to write: {len(copies)}")
    for source, rel_path, _key in copies[:10]:
        print(f"  {rel_path}")
    if len(copies) > 10:
        print(f"  ... and {len(copies) - 10} more")

    if not args.apply:
        print("\ndry run - nothing was written. Re-run with --apply.")
        return

    written = 0
    for source, rel_path, _key in copies:
        target = Path(args.device) / rel_path
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        written += 1
    os.sync()
    print(f"wrote {written} files to {args.device}")
    db.close()


if __name__ == "__main__":
    main()
