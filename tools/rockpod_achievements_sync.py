#!/usr/bin/env python3
"""Build and install the offline RockPod achievements database."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import sys
import tempfile
from pathlib import Path


REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(REPO_ROOT, "rockpod"))

from services.achievements import AchievementSyncService  # noqa: E402
from app.config import Config  # noqa: E402
from rockpod_achievements_coverage import validate as validate_coverage  # noqa: E402


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mount", help="mounted iPod or simulator root")
    parser.add_argument("--api-key", default=os.environ.get(
        "ROCKPOD_RA_WEB_API_KEY", ""))
    parser.add_argument("--username", default=os.environ.get(
        "ROCKPOD_RA_USERNAME", ""))
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    config = Config()
    api_key = args.api_key or str(config.get(
        "retroachievements_web_api_key", "") or "").strip()
    username = args.username or str(config.get(
        "retroachievements_username", "") or "").strip()

    mount = os.path.abspath(args.mount)
    if not os.path.isdir(os.path.join(mount, ".rockbox")):
        parser.error("mount does not contain .rockbox")
    service = AchievementSyncService(
        REPO_ROOT, api_key=api_key, username=username)
    with tempfile.TemporaryDirectory(prefix="rockpod-achievements-") as stage:
        assets, coverage = service.build_sync_assets(mount, stage)
        if not assets:
            raise SystemExit("No launchable games were discovered")
        if args.dry_run:
            print(json.dumps(coverage, indent=2, sort_keys=True))
            return

        current = [item for item in assets
                   if item["destination_rel"].endswith("/current")]
        ordered = [item for item in assets if item not in current] + current
        for item in ordered:
            destination = os.path.join(mount, item["destination_rel"].lstrip("/"))
            os.makedirs(os.path.dirname(destination), exist_ok=True)
            temporary = destination + ".rockpod-new"
            shutil.copy2(item["source_abs"], temporary)
            os.replace(temporary, destination)
            if sha256(item["source_abs"]) != sha256(destination):
                raise OSError(f"verification failed: {destination}")
        errors = validate_coverage(
            Path(mount), Path(REPO_ROOT))
        if errors:
            raise SystemExit(
                "Achievement coverage verification failed:\n- " +
                "\n- ".join(errors))
        print(json.dumps(coverage, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
