#!/usr/bin/env python3
"""Repair existing video metadata/cover metadata for tracks already in the RockPod DB.

This one-shot repair utility re-runs the same YouTube metadata pipeline used for
newly imported videos against current video rows. It is useful when prior imports
were classified incorrectly (for example shows ending up as movies).

Examples:
  - Dry-run: ``python rockpod/scripts/fix_current_video_metadata.py``
  - Apply changes: ``python rockpod/scripts/fix_current_video_metadata.py --apply``
  - Limit to YouTube-sourced files: ``--path-filter "/YouTube/"``
"""

from __future__ import annotations

import argparse
import re
import os
import sys
from typing import Dict
from pathlib import Path

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from app.config import Config  # noqa: E402
from app.database import Database  # noqa: E402
from services.online_video_metadata import VideoMetadataService  # noqa: E402
from services.youtube_import_metadata import (
    _extract_episode_hints,
    _parse_uploader_owned_show_hint,
    build_youtube_import_metadata_payload,
)
from services.online_artwork import ITunesArtworkLookup


_VIDEO_EPISODE_OVERRIDES = {
    "disney's recess - randall's friends": {
        "show_title": "Recess",
        "title": "Randall's Friends",
        "season_number": 3,
        "episode_number": 35,
    },
    "recess - randall's friends": {
        "show_title": "Recess",
        "title": "Randall's Friends",
        "season_number": 3,
        "episode_number": 35,
    },
    "randall's friends": {
        "show_title": "Recess",
        "title": "Randall's Friends",
        "season_number": 3,
        "episode_number": 35,
    },
    "disney's recess - space cadet": {
        "show_title": "Recess",
        "title": "Space Cadet",
        "season_number": 3,
        "episode_number": 29,
    },
    "recess - space cadet": {
        "show_title": "Recess",
        "title": "Space Cadet",
        "season_number": 3,
        "episode_number": 29,
    },
    "disney's recess - dodgeball city": {
        "show_title": "Recess",
        "title": "Dodgeball City",
        "season_number": 3,
        "episode_number": 36,
    },
    "recess - dodgeball city": {
        "show_title": "Recess",
        "title": "Dodgeball City",
        "season_number": 3,
        "episode_number": 36,
    },
    "disney's recess - lord of the nerds": {
        "show_title": "Recess",
        "title": "Lord of the Nerds",
        "season_number": 3,
        "episode_number": 37,
    },
    "recess - lord of the nerds": {
        "show_title": "Recess",
        "title": "Lord of the Nerds",
        "season_number": 3,
        "episode_number": 37,
    },
    "lord of the nerds": {
        "show_title": "Recess",
        "title": "Lord of the Nerds",
        "season_number": 3,
        "episode_number": 37,
    },
    "who can stay homeless the longest - kenny vs spenny (hd)": {
        "show_title": "Kenny vs. Spenny",
        "title": "Who Can Stay Homeless the Longest?",
        "season_number": 1,
        "episode_number": 22,
    },
    "who can stay homeless the longest_ - kenny vs spenny (hd)": {
        "show_title": "Kenny vs. Spenny",
        "title": "Who Can Stay Homeless the Longest?",
        "season_number": 1,
        "episode_number": 22,
    },
    "who can stay homeless the longest": {
        "show_title": "Kenny vs. Spenny",
        "title": "Who Can Stay Homeless the Longest?",
        "season_number": 1,
        "episode_number": 22,
    },
    "triumph of the will - the french reconnect": {
        "show_title": "Triumph of the Will",
        "title": "The French ReConnection",
        "season_number": 1,
        "episode_number": 5,
    },
    "triumph of the will the french reconnect": {
        "show_title": "Triumph of the Will",
        "title": "The French ReConnection",
        "season_number": 1,
        "episode_number": 5,
    },
    "triumph of the will - the french reconnection": {
        "show_title": "Triumph of the Will",
        "title": "The French ReConnection",
        "season_number": 1,
        "episode_number": 5,
    },
    "triumph of the will the french reconnection": {
        "show_title": "Triumph of the Will",
        "title": "The French ReConnection",
        "season_number": 1,
        "episode_number": 5,
    },
    "the french reconnect": {
        "show_title": "Triumph of the Will",
        "title": "The French ReConnection",
        "season_number": 1,
        "episode_number": 5,
    },
}


def _video_override_key(value):
    text = Path(str(value or "")).stem
    text = text.replace("_", " ")
    text = re.sub(r"\s+", " ", text).strip().casefold()
    text = text.replace("?", "")
    return text


def _clean_inferred_show_title(value):
    text = str(value or "").strip()
    lower_text = text.lower()
    for ext in (
        ".3gp",
        ".aac",
        ".avi",
        ".flac",
        ".f4v",
        ".flv",
        ".m4a",
        ".m4v",
        ".mkv",
        ".mp3",
        ".mp4",
        ".mpeg",
        ".mpg",
        ".m4p",
        ".mov",
        ".ogv",
        ".opus",
        ".ogg",
        ".webm",
        ".wmv",
    ):
        if lower_text.endswith(ext):
            text = text[:-len(ext)]
            break
    text = text.replace("_", " ")
    text = re.sub(r"\s+", " ", text).strip(" ._-")
    text = re.sub(r"\s*-\s*(?:hd|4k|uhd|full hd|1080p|720p|480p|360p)\s*$", "", text, flags=re.IGNORECASE)
    text = re.sub(
        r"\s*\([^)]*\b(?:hd|4k|uhd|full hd|1080p|720p|480p|360p)\b[^)]*\)\s*$",
        "",
        text,
        flags=re.IGNORECASE,
    )
    text = re.sub(r"\s+FULL\s+EPISODE\b.*$", "", text, flags=re.IGNORECASE).strip(" ._-")
    text = re.sub(r"\s+RETRO\s+RERUN\b.*$", "", text, flags=re.IGNORECASE).strip(" ._-")
    return text


def _infer_video_episode_metadata(track_data):
    item = dict(track_data or {})
    candidates = [
        item.get("title"),
        Path(str(item.get("file_path") or "")).stem,
    ]
    for candidate in candidates:
        text = str(candidate or "").replace("_", " ")
        text = re.sub(r"\s+", " ", text).strip()
        if not text:
            continue

        match = re.match(
            r"^(.+?)\s+S(\d{1,2})E(\d{1,3})(?:\s*[-:]\s*(.+))?$",
            text,
            flags=re.IGNORECASE,
        )
        if match:
            title = (match.group(4) or item.get("title") or text).strip()
            return {
                "show_title": _clean_inferred_show_title(match.group(1)),
                "season_number": int(match.group(2)),
                "episode_number": int(match.group(3)),
                "title": title,
                "video_kind": "show",
            }

        match = re.match(
            r"^(.+?)\s*[-:]\s*(?:season|series)\s*(\d{1,2})\s*[-: ]+"
            r"(?:episode|ep)\s*(\d{1,3})(?:\s*[-:]\s*(.+))?$",
            text,
            flags=re.IGNORECASE,
        )
        if match:
            title = (match.group(4) or item.get("title") or text).strip()
            return {
                "show_title": _clean_inferred_show_title(match.group(1)),
                "season_number": int(match.group(2)),
                "episode_number": int(match.group(3)),
                "title": title,
                "video_kind": "show",
            }

        match = re.match(
            r"^(.+?)\s*[-–—:]\s*(.+)$",
            text,
            flags=re.IGNORECASE,
        )
        if match:
            left = _clean_inferred_show_title(match.group(1)).strip()
            right = _clean_inferred_show_title(match.group(2)).strip()
            left = re.sub(r"\s*\([^)]*\)\s*$", "", left).strip()
            right = re.sub(r"\s*\([^)]*\)\s*$", "", right).strip()
            if re.search(r"\bvs\.?\b", left, flags=re.IGNORECASE):
                show_title = left
                title = right
            elif re.search(r"\bvs\.?\b", right, flags=re.IGNORECASE):
                show_title = right
                title = left
            else:
                continue
            title = title or item.get("title") or text
            title = _clean_inferred_show_title(title)
            show_title = _clean_inferred_show_title(show_title)
            if show_title:
                return {
                    "show_title": show_title,
                    "title": title,
                    "video_kind": "show",
                }

        match = re.match(
            r"(?i)^(?:episode|ep)\s*(\d{1,4})\b",
            text,
        )
        if match and (("season" in text.casefold()) or (text.casefold().startswith("ep"))):
            episode_number = int(match.group(1))
            title = _clean_inferred_show_title(re.sub(r"(?i)^(?:episode|ep)\s*\d+\s*[-:.]?\s*", "", text))
            return {
                "season_number": 1,
                "episode_number": episode_number,
                "title": title,
                "video_kind": "show",
            }

    return {}


def _apply_video_episode_overrides(track_data):
    candidates = [
        track_data.get("title"),
        Path(str(track_data.get("file_path") or "")).stem,
    ]
    for value in candidates:
        override = _VIDEO_EPISODE_OVERRIDES.get(_video_override_key(value))
        if override:
            return dict(override)
    return {}


def _normalize_path(value):
    return os.path.normcase(os.path.abspath(os.path.realpath(str(value or ""))))


def _normalize_video_kind(value):
    text = str(value or "").strip().lower().replace("_", " ")
    if text in {"", "movie", "movies"}:
        return "movie"
    if text in {"show", "tv", "tv show", "tvshow", "series"}:
        return "show"
    if text in {"home video", "home videos", "home_video", "homevideo", "personal"}:
        return "home_video"
    return text


def _to_dict(track):
    return dict(track)


def _resolve_db_path(cli_db, cli_config):
    if cli_db:
        return cli_db
    return Config(cli_config).db_path


def _normalize_title(value):
    text = str(value or "").strip()
    return " ".join(text.replace("_", " ").split())


def _pending_from_track(track):
    return {"query_hint": _normalize_title(track.get("title") or track.get("file_path"))}


def _has_real_changes(track, updates):
    for key, value in (updates or {}).items():
        if str(track.get(key) or "") != str(value or ""):
            return True
    return False


def _owned_show_hint_from_track(track_data, pending_item):
    track_title = _normalize_title(track_data.get("title"))
    file_stem = _normalize_title(os.path.splitext(os.path.basename(str(track_data.get("file_path") or "")))[0])
    candidates = [
        track_title,
        file_stem,
        str((pending_item or {}).get("query_hint") or ""),
    ]
    for value in candidates:
        parsed = _parse_uploader_owned_show_hint(value)
        if parsed.get("show_title"):
            return parsed
    return {}


def _fallback_episode_updates(track):
    track_data = dict(track)
    pending_item = _pending_from_track(track_data)
    hints = _extract_episode_hints(track_data, pending_item)
    owned_show = _owned_show_hint_from_track(track_data, pending_item)
    override = _apply_video_episode_overrides(track_data)
    inferred = _infer_video_episode_metadata(track_data)

    if not (override or inferred or owned_show or (
        hints.get("show_title") and (hints.get("season_number") is not None) and (hints.get("episode_number") is not None)
    )):
        return {}

    source = {}
    if override:
        source = dict(override)
    elif inferred:
        source = dict(inferred)
    elif owned_show:
        source = {
            "show_title": owned_show["show_title"],
            "title": owned_show.get("episode_title") or track_data.get("title") or "",
        }
    else:
        source = {
            "show_title": hints["show_title"],
            "season_number": hints["season_number"],
            "episode_number": hints["episode_number"],
        }

    updates: Dict[str, object] = {
        "video_kind": "show",
        "metadata_source": "filename",
        "metadata_confidence": 0.45,
    }
    if source.get("show_title"):
        updates["show_title"] = source["show_title"]
    if source.get("season_number") is not None:
        updates["season_number"] = source["season_number"]
    if source.get("episode_number") is not None:
        updates["episode_number"] = source["episode_number"]
        updates["track_number"] = source["episode_number"]
    if source.get("title"):
        updates["title"] = source["title"]
    elif not track.get("title"):
        updates["title"] = hints.get("episode_title") or ""
    if source.get("show_title"):
        if source.get("season_number") is None:
            source["season_number"] = 1
        updates["album"] = f"Season {int(source['season_number'])}"
        updates["season_number"] = int(source["season_number"])
        if not str(track.get("artist") or "").strip():
            updates["artist"] = source["show_title"]
        if not str(track.get("album_artist") or "").strip():
            updates["album_artist"] = source["show_title"]
    if not str(track.get("artist") or "").strip():
        updates["artist"] = source.get("show_title", "")
    if not str(track.get("album_artist") or "").strip():
        updates["album_artist"] = source.get("show_title", "")
    return updates


def _video_group_key(track, updates):
    video_kind = _normalize_video_kind(updates.get("video_kind") if updates else track.get("video_kind"))
    if video_kind == "show":
        show_title = str((updates.get("show_title") if updates else track.get("show_title") or "")).strip()
        if show_title:
            return f"show:{show_title.casefold()}"
    file_path = str(track.get("file_path") or "").strip()
    if file_path:
        return f"{os.path.basename(file_path)}:{track.get('id')}"
    return f"track:{track.get('id')}"


def _local_cover_path_for_track(track):
    file_path = str(track.get("file_path") or "").strip()
    if not file_path:
        return ""
    stem = str(Path(file_path).with_suffix(""))
    candidates = [
        f"{stem}{ext}"
        for ext in (".jpg", ".jpeg", ".png", ".webp")
    ]
    for path in candidates:
        if os.path.exists(path):
            return path
    return ""


def _apply_artwork(artwork, track, updates, artwork_url, local_cover_path=""):
    group_key = _video_group_key(track, updates)
    if local_cover_path and os.path.exists(local_cover_path):
        try:
            with open(local_cover_path, "rb") as handle:
                return bool(artwork.set_manual_video_poster(group_key, handle.read()))
        except Exception:
            return False
    if not artwork_url:
        return False
    return bool(artwork.set_manual_video_poster(group_key, artwork_url))


def _matches_path_filter(file_path, value):
    if not value:
        return True
    candidate = str(value or "").strip().casefold()
    if not candidate:
        return True
    return candidate in _normalize_path(file_path).casefold()


def _matches_kind(track, kind_filter):
    if kind_filter == "all":
        return True
    return _normalize_video_kind(track.get("video_kind")) == kind_filter


def run(args):
    db_path = _resolve_db_path(args.db, args.config)
    config = Config(args.config)
    db = Database(db_path)
    service = VideoMetadataService(config=config)
    artwork = None
    if args.apply and not args.skip_artwork:
        try:
            from services.artwork_manager import ArtworkManager  # noqa: WPS433

            artwork = ArtworkManager(
                config.artwork_cache_dir,
                config,
                lookup_client=ITunesArtworkLookup(min_interval_seconds=0.5),
            )
        except Exception as exc:
            print(f"Could not initialize artwork manager ({exc}); cover updates disabled.")
            args.skip_artwork = True

    stats = {
        "checked": 0,
        "matched": 0,
        "updated": 0,
        "skipped_locked": 0,
        "skipped_filter": 0,
        "fallback_only": 0,
        "not_matched": 0,
        "artwork_updates": 0,
        "errors": 0,
    }

    print(f"Loaded {db_path}")
    for track in db.get_tracks_by_media_type("video", order_by="id"):
        track_data = _to_dict(track)

        if int(track_data.get("id") or 0) <= 0:
            continue

        file_path = str(track_data.get("file_path") or "")
        if not file_path:
            stats["skipped_filter"] += 1
            continue

        if not _matches_path_filter(file_path, args.path_filter):
            continue

        if not _matches_kind(track_data, args.video_kind):
            continue

        if args.limit > 0 and stats["checked"] >= args.limit:
            break

        if not args.force_locked and int(track_data.get("metadata_locked") or 0):
            stats["skipped_locked"] += 1
            continue

        stats["checked"] += 1
        pending_item = _pending_from_track(track_data)

        payload = None
        used_fallback = False
        try:
            payload = build_youtube_import_metadata_payload(track_data, pending_item, service)
        except Exception as exc:
            print(f"ERROR: metadata lookup failed for id={track_data.get('id')}: {exc}")
            stats["errors"] += 1

        if payload is None and args.fallback_to_filename:
            fallback = _fallback_episode_updates(track_data)
            if fallback:
                payload = {"updates": fallback, "artwork_url": ""}
                used_fallback = True

        if payload is None:
            stats["not_matched"] += 1
            continue

        updates = dict(payload.get("updates") or {})
        track_for_group = dict(track_data)
        track_for_group.update(updates)
        local_cover_path = _local_cover_path_for_track(track_for_group)

        stats["matched"] += 1
        changed = _has_real_changes(track_data, updates)

        if args.apply:
            if changed:
                db.update_track_metadata(track_data.get("id"), updates)
                stats["updated"] += 1
            if used_fallback:
                stats["fallback_only"] += 1
            if not args.skip_artwork and (payload.get("artwork_url") or local_cover_path):
                if _apply_artwork(
                    artwork,
                    track_for_group,
                    updates,
                    payload.get("artwork_url", ""),
                    local_cover_path=local_cover_path,
                ):
                    stats["artwork_updates"] += 1
            elif not args.skip_artwork and used_fallback and _normalize_video_kind(updates.get("video_kind")) == "show":
                track_info = {
                    "group_key": _video_group_key(track_for_group, updates),
                    "album": updates.get("show_title") or track_data.get("show_title") or track_data.get("title") or "",
                    "artist": updates.get("show_title") or track_data.get("artist") or "",
                    "video_kind": updates.get("video_kind") or track_data.get("video_kind") or "video",
                    "video_scope": "show",
                    "tracks": [track_for_group],
                }
                if artwork.fetch_online_artwork_now(track_info, force=True):
                    stats["artwork_updates"] += 1
        else:
            label = f"[dry-run] id={track_data.get('id')} "
            changed_text = "would update" if changed else "would refresh cover"
            if changed:
                print(f"{label}{changed_text}: {updates}")
            if payload.get("artwork_url"):
                print(f"{label}would set cover from artwork lookup")
            if local_cover_path:
                print(f"{label}would use local cover {local_cover_path}")

    if args.apply:
        db.commit()

    print("\nVideo repair summary:")
    for key, value in sorted(stats.items()):
        print(f"{key}: {value}")


def _parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--db", default="", help="Override RockPod DB path")
    parser.add_argument("--config", default="", help="Override config path")
    parser.add_argument("--path-filter", default="", help="Only process files containing this substring in path")
    parser.add_argument("--video-kind", default="all", choices=["all", "show", "movie", "home_video"], help="Limit by current video kind")
    parser.add_argument("--limit", type=int, default=0, help="Process at most N tracks (0 = no limit)")
    parser.add_argument("--apply", action="store_true", help="Write updates and artwork fixes")
    parser.add_argument("--force-locked", action="store_true", help="Apply fixes even when metadata is locked")
    parser.add_argument("--skip-artwork", action="store_true", help="Do not fetch or set posters")
    parser.add_argument("--fallback-to-filename", action="store_true", help="Fallback to parsed season/episode info when metadata lookup fails")
    return parser.parse_args(argv)


def main(argv=None):
    args = _parse_args(argv)
    run(args)


if __name__ == "__main__":
    raise SystemExit(main())
