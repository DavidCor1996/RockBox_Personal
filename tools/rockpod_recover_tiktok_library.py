#!/usr/bin/env python3
"""Recover RockPod TikTok DB rows from a mounted feed and local originals."""

import argparse
import csv
import json
import sys
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "rockpod"))

from app.config import Config  # noqa: E402
from app.database import Database  # noqa: E402
from services.tiktok_app import (  # noqa: E402
    TIKTOK_MEDIA_PIPELINE_VERSION,
    TikTokAppService,
    _source_signature,
)


def load_info(source: Path) -> dict:
    info_path = source.with_suffix(".info.json")
    try:
        return json.loads(info_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mount", type=Path)
    parser.add_argument(
        "--sources", type=Path,
        default=Path.home() / ".rockpod" / "tiktok",
    )
    args = parser.parse_args()
    mount = args.mount.resolve()
    feed_path = mount / ".rockbox/rocks/apps/.ipodtiktok_feed.tsv"
    profiles_path = mount / ".rockbox/tiktok/profiles.tsv"
    if not feed_path.is_file() or not args.sources.is_dir():
        parser.error("mounted TikTok feed or local originals are missing")

    originals = {
        path.stem: path for path in args.sources.rglob("*.mp4") if path.is_file()
    }
    config = Config()
    database = Database(config.db_path)
    service = TikTokAppService(database, config, REPO)
    recovered = 0
    missing = []
    account_modes = {}

    with feed_path.open("r", encoding="utf-8", errors="replace") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            video_id = str(row.get("id") or "")
            remote_id = video_id.removeprefix("tt_")
            source = originals.get(remote_id)
            if source is None:
                missing.append(video_id)
                continue
            info = load_info(source)
            username = str(info.get("uploader") or row.get("creator") or "").lstrip("@")
            account_url = str(info.get("uploader_url") or "")
            if not account_url and username:
                account_url = f"https://www.tiktok.com/@{username}"
            thumbnail = next(
                (path for path in source.parent.glob(f"{remote_id}.*")
                 if path.suffix.lower() in {".jpg", ".jpeg", ".png", ".webp"}),
                None,
            )
            source_type = str(row.get("source_type") or "manual")
            service._upsert_profile(info, account_url)
            service.add_video(
                source,
                remote_id=remote_id,
                title=info.get("title") or row.get("title") or "TikTok",
                creator=row.get("creator") or (f"@{username}" if username else "TikTok"),
                description=info.get("description") or row.get("description") or "",
                duration_ms=int(float(info.get("duration") or 0) * 1000),
                upload_date=info.get("upload_date") or "",
                like_count=int(info.get("like_count") or row.get("likes") or 0),
                comment_count=int(info.get("comment_count") or row.get("comments") or 0),
                source_url=info.get("webpage_url") or "",
                source_type=source_type,
                account_url=account_url,
                thumbnail_path=str(thumbnail or ""),
            )
            signature = f"{TIKTOK_MEDIA_PIPELINE_VERSION}:{_source_signature(source)}"
            database.execute(
                "UPDATE tiktok_videos SET last_synced_source_hash=? WHERE id=?",
                (signature, video_id),
            )
            if account_url and source_type in {"following", "archive"}:
                account_modes[account_url] = source_type
            recovered += 1

    if profiles_path.is_file():
        with profiles_path.open("r", encoding="utf-8", errors="replace") as handle:
            for profile in csv.DictReader(handle, delimiter="\t"):
                service._upsert_profile(
                    {
                        "uploader": profile.get("username"),
                        "channel": profile.get("display_name"),
                        "bio": profile.get("bio"),
                        "follower_count": profile.get("followers") or 0,
                        "following_count": profile.get("following") or 0,
                        "likes_count": profile.get("likes") or 0,
                        "video_count": profile.get("videos") or 0,
                    },
                    profile.get("account_url") or "",
                )

    for account_url, source_type in account_modes.items():
        service.add_account_sync(
            account_url, "archive" if source_type == "archive" else "rolling"
        )
    database.close()
    if missing:
        print(f"Hard stop: {len(missing)} feed rows lack local originals")
        return 2
    print(f"Recovered {recovered} TikTok rows and {len(account_modes)} account syncs.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
