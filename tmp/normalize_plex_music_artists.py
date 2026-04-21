#!/usr/bin/env python3

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path

from mutagen import File


AUDIO_EXTS = {
    ".flac",
    ".mp3",
    ".m4a",
    ".aac",
    ".ogg",
    ".opus",
    ".wav",
    ".aiff",
    ".aif",
    ".alac",
    ".wma",
    ".ape",
    ".wv",
}

SKIP_ALBUM_ARTISTS = {
    "various artists",
    "various",
    "soundtrack",
}


@dataclass
class TrackInfo:
    path: Path
    album: str
    albumartist: str
    artist: str


def audio_files(root: Path) -> list[Path]:
    return sorted(
        path
        for path in root.rglob("*")
        if path.is_file() and path.suffix.lower() in AUDIO_EXTS
    )


def read_easy_tags(path: Path):
    return File(path, easy=True)


def first_value(tags, key: str) -> str:
    if tags is None or tags.tags is None:
        return ""
    values = tags.tags.get(key)
    if not values:
        return ""
    if isinstance(values, list):
        return str(values[0]).strip()
    return str(values).strip()


def split_semicolon_artists(value: str) -> list[str]:
    return [part.strip() for part in value.split(";") if part.strip()]


def collect_tracks(root: Path) -> dict[Path, list[TrackInfo]]:
    grouped: dict[Path, list[TrackInfo]] = defaultdict(list)
    for path in audio_files(root):
        tags = read_easy_tags(path)
        if tags is None or tags.tags is None:
            continue
        grouped[path.parent].append(
            TrackInfo(
                path=path,
                album=first_value(tags, "album"),
                albumartist=first_value(tags, "albumartist"),
                artist=first_value(tags, "artist"),
            )
        )
    return grouped


def dominant_albumartist(tracks: list[TrackInfo]) -> str:
    counts = Counter(
        track.albumartist
        for track in tracks
        if track.albumartist and track.albumartist.strip().lower() not in SKIP_ALBUM_ARTISTS
    )
    if not counts:
        return ""
    winner, count = counts.most_common(1)[0]
    return winner if count >= 1 else ""


def plan_changes(grouped: dict[Path, list[TrackInfo]]):
    changes = []
    for folder, tracks in grouped.items():
        dominant = dominant_albumartist(tracks)
        if not dominant:
            continue
        multi_artist_count = sum(1 for track in tracks if ";" in track.artist)
        multi_artist_ratio = multi_artist_count / len(tracks) if tracks else 0.0
        for track in tracks:
            track_changes = {}
            if track.albumartist != dominant:
                track_changes["albumartist"] = dominant
            title_or_name = f"{track.path.stem} {track.path.name}".lower()
            looks_featured = any(token in title_or_name for token in ("feat.", "ft.", "featuring"))
            if ";" in track.artist and (looks_featured or multi_artist_ratio < 0.7 or "albumartist" in track_changes):
                track_changes["artist"] = dominant
            if track_changes:
                changes.append((folder, track, track_changes))
    return changes


def apply_change(track: TrackInfo, track_changes: dict[str, str]) -> None:
    tags = read_easy_tags(track.path)
    if tags is None:
        raise RuntimeError(f"Could not read tags for {track.path}")
    for key, value in track_changes.items():
        tags[key] = [value]
    tags.save()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=str(Path.home() / "Music"))
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()

    root = Path(args.root).expanduser()
    grouped = collect_tracks(root)
    changes = plan_changes(grouped)

    print(f"Folders scanned: {len(grouped)}")
    print(f"Tracks to change: {len(changes)}")

    current_folder = None
    for folder, track, track_changes in changes:
        if folder != current_folder:
            current_folder = folder
            print(f"\n[{folder}]")
        print(f"- {track.path.name}")
        print(f"  albumartist: {track.albumartist!r} -> {track_changes.get('albumartist', track.albumartist)!r}")
        print(f"  artist: {track.artist!r} -> {track_changes.get('artist', track.artist)!r}")
        if args.apply:
            apply_change(track, track_changes)

    if args.apply:
        print("\nApplied changes.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
