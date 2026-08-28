#!/usr/bin/env python3
"""Download a public TikTok profile from account-list API media formats."""

from __future__ import annotations

import argparse
import itertools
import json
import re
import time
from pathlib import Path

from yt_dlp import DownloadError, YoutubeDL
from yt_dlp.extractor.tiktok import TikTokUserIE
from yt_dlp.utils import ExtractorError
from yt_dlp.utils.traversal import traverse_obj

VIDEO_SUFFIXES = {".avi", ".m4v", ".mkv", ".mov", ".mp4", ".mpeg", ".mpg", ".webm"}


def profile_embed_videos(embed_page):
    """Return the ordered public videos rendered in a TikTok profile embed."""
    match = re.search(
        r'<script[^>]+id=[\'\"]__FRONTITY_CONNECT_STATE__[\'\"][^>]*>'
        r'(.*?)</script>',
        embed_page or "",
        re.DOTALL,
    )
    if not match:
        return []
    try:
        records = json.loads(match.group(1)).get("source", {}).get("data", {})
    except (TypeError, json.JSONDecodeError):
        return []
    record = next(
        (
            value for key, value in records.items()
            if str(key).startswith("/embed/@") and isinstance(value, dict)
        ),
        {},
    )
    return [item for item in record.get("videoList") or [] if item.get("id")]


def pinned_video_order(embed_page, newest_video_id):
    """Return pinned IDs mapped to their TikTok display priority."""
    ordered = [
        str(item.get("id") or "") for item in profile_embed_videos(embed_page)
    ]
    if not newest_video_id:
        return {}
    try:
        first_recent = ordered.index(str(newest_video_id))
    except ValueError:
        return {}
    # TikTok permits at most three pinned posts. Refuse a surprising payload
    # instead of falsely pinning a large portion of an account.
    if first_recent > 3:
        return {}
    return {
        video_id: first_recent - index
        for index, video_id in enumerate(ordered[:first_recent])
    }


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sec-uid", required=True)
    parser.add_argument("--username", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--max-items", type=int, default=0)
    parser.add_argument("--profile-only", action="store_true")
    return parser.parse_args()


def main():
    args = parse_args()
    output = Path(args.output).expanduser().resolve()
    output.mkdir(parents=True, exist_ok=True)
    print(f"[archive] Opening @{args.username} archive…", flush=True)
    options = {
        "format": "best[height<=720]/best",
        "outtmpl": str(output / "%(id)s.%(ext)s"),
        "continuedl": True,
        "overwrites": False,
        "ignoreerrors": True,
        "writethumbnail": True,
        "writeinfojson": True,
        "cookiesfrombrowser": ("firefox",),
        "postprocessors": [{
            "key": "FFmpegThumbnailsConvertor",
            "format": "jpg",
            "when": "before_dl",
        }],
        "extractor_args": {
            "tiktok": {"app_info": ["/musical_ly/35.1.3/2023501030/1233"]}
        },
    }
    seen = set()
    downloaded = 0
    skipped = 0
    cursor = int(time.time() * 1000)

    with YoutubeDL(options) as ydl:
        extractor = TikTokUserIE(ydl)
        pinned_order = {}
        for page in itertools.count(1):
            print(f"[archive] Reading account page {page}…", flush=True)
            response = extractor._download_json(
                extractor._API_BASE_URL,
                args.username,
                f"Downloading account page {page}",
                query=extractor._build_web_query(args.sec_uid, cursor),
            )
            videos = response.get("itemList") or []
            print(
                f"[archive] Account page {page}: {len(videos)} posts found",
                flush=True,
            )
            if videos and page == 1:
                embed_page = extractor._download_webpage(
                    f"https://www.tiktok.com/embed/@{args.username}",
                    args.username,
                    "Reading pinned account posts",
                    errnote=False,
                    fatal=False,
                )
                pinned_order = pinned_video_order(
                    embed_page, videos[0].get("id")
                )
                author = videos[0].get("author") or {}
                stats = videos[0].get("authorStatsV2") or videos[0].get("authorStats") or {}
                profile = {
                    "uploader": author.get("uniqueId") or args.username,
                    "channel": author.get("nickname") or "",
                    "bio": author.get("signature") or "",
                    "avatar_url": author.get("avatarLarger") or author.get("avatarMedium") or "",
                    "verified": bool(author.get("verified")),
                    "channel_follower_count": stats.get("followerCount") or 0,
                    "following_count": stats.get("followingCount") or 0,
                    "profile_like_count": stats.get("heartCount") or stats.get("heart") or 0,
                    "video_count": stats.get("videoCount") or 0,
                    "sec_uid": author.get("secUid") or args.sec_uid,
                }
                (output / ".profile.json").write_text(
                    json.dumps(profile, ensure_ascii=False, indent=2),
                    encoding="utf-8",
                )
                if args.profile_only:
                    print("PROFILE_SUMMARY " + json.dumps(profile, ensure_ascii=False))
                    return 0
                # Pinned posts can be years older than the first API page.
                # Download the real embed renditions immediately so even a
                # partially completed large archive includes its pinned row.
                embed_videos = {
                    str(item.get("id")): item
                    for item in profile_embed_videos(embed_page)
                }
                for pinned_id, priority in sorted(
                    pinned_order.items(), key=lambda item: item[1], reverse=True
                ):
                    existing_pin = next(
                        (
                            path for path in output.glob(f"{pinned_id}.*")
                            if path.suffix.lower() in VIDEO_SUFFIXES
                        ),
                        None,
                    )
                    if existing_pin is not None:
                        continue
                    embedded = embed_videos.get(pinned_id) or {}
                    play_url = embedded.get("playAddr") or ""
                    if not play_url:
                        continue
                    pinned_url = (
                        f"https://www.tiktok.com/@{args.username}/video/{pinned_id}"
                    )
                    pinned_info = {
                        "id": pinned_id,
                        "title": embedded.get("desc") or "TikTok",
                        "description": embedded.get("desc") or "",
                        "uploader": args.username,
                        "channel": profile.get("channel") or args.username,
                        "webpage_url": pinned_url,
                        "original_url": pinned_url,
                        "upload_date": time.strftime(
                            "%Y%m%d", time.gmtime(int(pinned_id) >> 32)
                        ),
                        "thumbnail": (
                            embedded.get("coverUrl")
                            or embedded.get("originCoverUrl") or ""
                        ),
                        "pin_order": priority,
                        "extractor": "TikTok",
                        "extractor_key": "TikTok",
                        "formats": [{
                            "format_id": "profile-embed",
                            "url": play_url,
                            "ext": "mp4",
                            "vcodec": "h264",
                            "acodec": "aac",
                            "width": embedded.get("width"),
                            "height": embedded.get("height"),
                        }],
                    }
                    try:
                        print(
                            f"[archive] pinned {priority} {pinned_id} "
                            f"{pinned_info['title']}",
                            flush=True,
                        )
                        ydl.process_ie_result(pinned_info, download=True)
                        downloaded += 1
                    except (DownloadError, ExtractorError) as exc:
                        print(
                            f"[archive] skipped pinned {pinned_id}: {exc}",
                            flush=True,
                        )
            for video in videos:
                video_id = str(video.get("id") or "")
                if not video_id or video_id in seen:
                    continue
                if args.max_items and len(seen) >= args.max_items:
                    break
                seen.add(video_id)
                existing = next(
                    (
                        path for path in output.glob(f"{video_id}.*")
                        if path.suffix.lower() in VIDEO_SUFFIXES
                    ),
                    None,
                )
                info_path = output / f"{video_id}.info.json"
                if existing is not None and info_path.is_file() and existing != info_path:
                    # Refresh pin state even when media was already archived.
                    # This lets a later archive pass reflect pins/unpins while
                    # retaining the no-redownload guarantee.
                    try:
                        saved_info = json.loads(info_path.read_text(encoding="utf-8"))
                        desired_order = pinned_order.get(video_id, 0)
                        if int(saved_info.get("pin_order") or 0) != desired_order:
                            saved_info["pin_order"] = desired_order
                            temp_info = info_path.with_suffix(".info.json.rockpod-tmp")
                            temp_info.write_text(
                                json.dumps(saved_info, ensure_ascii=False, indent=2),
                                encoding="utf-8",
                            )
                            temp_info.replace(info_path)
                    except (OSError, ValueError, json.JSONDecodeError):
                        pass
                    skipped += 1
                    print(
                        f"[archive] {len(seen)} {video_id} already archived",
                        flush=True,
                    )
                    continue
                webpage_url = (
                    f"https://www.tiktok.com/@{args.username}/video/{video_id}"
                )
                info = extractor._parse_aweme_video_web(
                    video, webpage_url, video_id, extract_flat=False
                )
                info["pin_order"] = pinned_order.get(video_id, 0)
                # Prime CDN links embedded directly in the web response can
                # reject a cold request. The same response also includes a
                # first-party /aweme/v1/play URL that sets the correct redirect
                # token. Prefer the best H.264 rendition through that route.
                h264 = next(
                    (
                        item for item in sorted(
                            video.get("video", {}).get("bitrateInfo") or [],
                            key=lambda item: int(item.get("Bitrate") or 0),
                            reverse=True,
                        )
                        if str(item.get("CodecType") or "").lower() == "h264"
                        and len(
                            item.get("PlayAddr", {}).get("UrlList") or []
                        ) >= 3
                    ),
                    None,
                )
                if h264:
                    play = h264["PlayAddr"]
                    info["formats"] = [{
                        "format_id": "account-h264",
                        "url": play["UrlList"][2],
                        "ext": "mp4",
                        "vcodec": "h264",
                        "acodec": "aac",
                        "width": play.get("Width"),
                        "height": play.get("Height"),
                        "tbr": int(h264.get("Bitrate") or 0) / 1000,
                        "filesize": play.get("DataSize"),
                    }]
                info.update({
                    "webpage_url": webpage_url,
                    "original_url": webpage_url,
                    "extractor": "TikTok",
                    "extractor_key": "TikTok",
                })
                print(
                    f"[archive] {len(seen)} {video_id} "
                    f"{info.get('title') or ''}",
                    flush=True,
                )
                try:
                    ydl.process_ie_result(info, download=True)
                except (DownloadError, ExtractorError) as exc:
                    print(
                        f"[archive] skipped {video_id}: {exc}",
                        flush=True,
                    )
                    continue
                media = next(
                    (
                        path for path in output.glob(f"{video_id}.*")
                        if path.suffix.lower() in VIDEO_SUFFIXES
                    ),
                    None,
                )
                if media is not None:
                    downloaded += 1
            if args.max_items and len(seen) >= args.max_items:
                break
            old_cursor = cursor
            cursor = traverse_obj(
                response,
                ("itemList", -1, "createTime", {lambda value: int(value * 1000)}),
            )
            if not cursor or old_cursor == cursor:
                cursor = old_cursor - 7 * 86_400_000
            if cursor < 1472706000000 or not response.get("hasMorePrevious"):
                break

    summary = {
        "seen": len(seen),
        "downloaded": downloaded,
        "skipped": skipped,
    }
    print("ARCHIVE_SUMMARY " + json.dumps(summary, sort_keys=True), flush=True)
    return 0 if seen else 1


if __name__ == "__main__":
    raise SystemExit(main())
