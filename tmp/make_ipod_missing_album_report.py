#!/usr/bin/env python3
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path


AUDIO_EXTS = {
    ".mp3",
    ".m4a",
    ".aac",
    ".flac",
    ".ogg",
    ".opus",
    ".wav",
    ".wma",
}


INVALID_WS = re.compile(r"\s+")
TRAILING_DISC = re.compile(r"\s*(?:cd|disc|disk)\s*\d+\s*$", re.I)


@dataclass(frozen=True)
class Album:
    artist: str
    album: str
    root: str


def normalize_text(value: str) -> str:
    value = value.replace("\x00", " ").strip()
    value = INVALID_WS.sub(" ", value)
    return value


def key_text(value: str) -> str:
    value = normalize_text(value).casefold()
    value = value.replace("&", "and")
    value = re.sub(r"[^a-z0-9]+", " ", value)
    return INVALID_WS.sub(" ", value).strip()


def album_key(artist: str, album: str) -> tuple[str, str]:
    return key_text(artist), key_text(album)


def decode_syncsafe(raw: bytes) -> int:
    total = 0
    for byte in raw:
        total = (total << 7) | (byte & 0x7F)
    return total


def decode_text_frame(data: bytes) -> str:
    if not data:
        return ""
    enc = data[0]
    payload = data[1:]
    try:
        if enc == 0:
            text = payload.decode("latin1", "replace")
        elif enc == 1:
            text = payload.decode("utf-16", "replace")
        elif enc == 2:
            text = payload.decode("utf-16-be", "replace")
        elif enc == 3:
            text = payload.decode("utf-8", "replace")
        else:
            text = payload.decode("latin1", "replace")
    except Exception:
        text = payload.decode("latin1", "replace")
    return normalize_text(text.split("\x00", 1)[0])


def read_id3_tags(path: Path) -> dict[str, str]:
    try:
        with path.open("rb") as f:
            header = f.read(10)
            if len(header) < 10 or header[:3] != b"ID3":
                return {}
            major = header[3]
            tag_size = decode_syncsafe(header[6:10])
            data = f.read(tag_size)
    except OSError:
        return {}

    tags: dict[str, str] = {}
    pos = 0
    frame_map = {
        "TALB": "album",
        "TPE1": "artist",
        "TPE2": "albumartist",
        "TCOM": "composer",
        "TIT2": "title",
        "TP1": "artist",
        "TP2": "albumartist",
        "TAL": "album",
        "TT2": "title",
    }

    while pos < len(data):
        if major == 2:
            if pos + 6 > len(data):
                break
            frame_id = data[pos : pos + 3].decode("latin1", "ignore")
            size = int.from_bytes(data[pos + 3 : pos + 6], "big")
            pos += 6
        else:
            if pos + 10 > len(data):
                break
            frame_id = data[pos : pos + 4].decode("latin1", "ignore")
            if not frame_id.strip("\x00"):
                break
            raw_size = data[pos + 4 : pos + 8]
            size = decode_syncsafe(raw_size) if major == 4 else int.from_bytes(raw_size, "big")
            pos += 10
        if size <= 0 or pos + size > len(data):
            break
        frame_data = data[pos : pos + size]
        pos += size
        target = frame_map.get(frame_id)
        if target and target not in tags:
            value = decode_text_frame(frame_data)
            if value:
                tags[target] = value
    return tags


def ffprobe_tags(path: Path) -> dict[str, str]:
    try:
        proc = subprocess.run(
            [
                "ffprobe",
                "-v",
                "error",
                "-show_entries",
                "format_tags=album,artist,album_artist,albumartist,title",
                "-of",
                "json",
                str(path),
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=5,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired):
        return {}
    try:
        raw = json.loads(proc.stdout or "{}").get("format", {}).get("tags", {})
    except json.JSONDecodeError:
        return {}
    lowered = {k.lower(): normalize_text(str(v)) for k, v in raw.items() if v}
    return {
        "album": lowered.get("album", ""),
        "artist": lowered.get("artist", ""),
        "albumartist": lowered.get("album_artist") or lowered.get("albumartist") or "",
        "title": lowered.get("title", ""),
    }


def fallback_from_path(path: Path, base: Path) -> dict[str, str]:
    try:
        rel = path.relative_to(base)
    except ValueError:
        rel = path
    parts = rel.parts
    if len(parts) >= 3:
        artist = normalize_text(parts[-3])
        album = normalize_text(TRAILING_DISC.sub("", parts[-2]))
        return {"artist": artist, "albumartist": artist, "album": album}
    if len(parts) >= 2:
        return {"album": normalize_text(TRAILING_DISC.sub("", parts[-2]))}
    return {}


def iter_audio_files(base: Path):
    for root, _, files in os.walk(base):
        for name in files:
            path = Path(root) / name
            if path.suffix.casefold() in AUDIO_EXTS:
                yield path


def read_tags(path: Path, base: Path, allow_ffprobe: bool) -> dict[str, str]:
    tags = read_id3_tags(path) if path.suffix.casefold() == ".mp3" else {}
    if allow_ffprobe and (not tags.get("album") or not (tags.get("albumartist") or tags.get("artist"))):
        probed = ffprobe_tags(path)
        tags = {**probed, **{k: v for k, v in tags.items() if v}}
    fallback = fallback_from_path(path, base)
    return {
        "album": tags.get("album") or fallback.get("album", ""),
        "artist": tags.get("artist") or fallback.get("artist", ""),
        "albumartist": tags.get("albumartist") or fallback.get("albumartist", ""),
        "title": tags.get("title", ""),
    }


def scan_albums(base: Path, *, allow_ffprobe: bool, workers: int) -> tuple[dict[tuple[str, str], Album], Counter, int]:
    paths = list(iter_audio_files(base))
    albums: dict[tuple[str, str], Album] = {}
    counts: Counter = Counter()

    def one(path: Path):
        tags = read_tags(path, base, allow_ffprobe)
        album = normalize_text(tags.get("album", ""))
        artist = normalize_text(tags.get("albumartist") or tags.get("artist", ""))
        if not album:
            return None
        if not artist:
            artist = "Unknown Artist"
        key = album_key(artist, album)
        return key, Album(artist=artist, album=album, root=str(path.parent)), path

    if workers <= 1:
        results = (one(path) for path in paths)
    else:
        with ThreadPoolExecutor(max_workers=workers) as pool:
            futures = [pool.submit(one, path) for path in paths]
            results = (future.result() for future in as_completed(futures))

    for result in results:
        if result is None:
            continue
        key, album, path = result
        albums.setdefault(key, album)
        counts[key] += 1
    return albums, counts, len(paths)


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: make_ipod_missing_album_report.py MUSIC_DIR IPOD_MOUNT OUTFILE", file=sys.stderr)
        return 2

    music_dir = Path(sys.argv[1]).resolve()
    ipod_mount = Path(sys.argv[2]).resolve()
    out_file = Path(sys.argv[3]).resolve()

    local_albums, local_counts, local_tracks = scan_albums(music_dir, allow_ffprobe=True, workers=8)
    ipod_music = ipod_mount / "iPod_Control" / "Music"
    ipod_albums, ipod_counts, ipod_tracks = scan_albums(ipod_music, allow_ffprobe=False, workers=1)

    missing_keys = sorted(
        set(local_albums) - set(ipod_albums),
        key=lambda k: (local_albums[k].artist.casefold(), local_albums[k].album.casefold()),
    )
    title_only_on_ipod = defaultdict(list)
    for artist_key, album_title_key in ipod_albums:
        title_only_on_ipod[album_title_key].append(ipod_albums[(artist_key, album_title_key)])

    out_file.parent.mkdir(parents=True, exist_ok=True)
    with out_file.open("w", encoding="utf-8") as f:
        f.write("Albums in /home/david/Music that are missing from the iPod\n")
        f.write("Generated by comparing album artist + album title tags.\n\n")
        f.write(f"Local audio files scanned: {local_tracks}\n")
        f.write(f"Local albums found: {len(local_albums)}\n")
        f.write(f"iPod audio files scanned: {ipod_tracks}\n")
        f.write(f"iPod albums found: {len(ipod_albums)}\n")
        f.write(f"Missing albums: {len(missing_keys)}\n\n")

        if not missing_keys:
            f.write("No missing albums found.\n")
        else:
            for key in missing_keys:
                album = local_albums[key]
                f.write(f"{album.artist} - {album.album}\n")
                f.write(f"  Local tracks: {local_counts[key]}\n")
                f.write(f"  Local folder: {album.root}\n")
                same_title = title_only_on_ipod.get(key[1], [])
                if same_title:
                    names = ", ".join(f"{item.artist} - {item.album}" for item in same_title[:4])
                    f.write(f"  Note: same album title exists on iPod as: {names}\n")
                f.write("\n")

    print(
        f"local_tracks={local_tracks} local_albums={len(local_albums)} "
        f"ipod_tracks={ipod_tracks} ipod_albums={len(ipod_albums)} "
        f"missing={len(missing_keys)} outfile={out_file}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
