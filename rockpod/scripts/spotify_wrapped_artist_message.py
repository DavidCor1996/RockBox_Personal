#!/usr/bin/env python3
"""Find and install an artist's YouTube greeting for offline Wrapped.

The iPod is deliberately offline. RockPod searches from the host, accepts
only a high-confidence Spotify Wrapped message for the actual all-time #1
artist, converts it to iPod MPEG, and installs nothing when no match exists.
"""

from __future__ import annotations

import argparse
from collections import defaultdict
from datetime import datetime
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from PIL import Image, ImageOps, UnidentifiedImageError

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from services.android_media import build_ffmpeg_command  # noqa: E402
from services.rockbox_tagcache import read_rockbox_tagcache_tracks  # noqa: E402


REJECT_PHRASES = (
    "reaction", "reacts", "compilation", "ranking", "tier list", "template",
    "how to", "tutorial", "parody", "fan made", "fan-made", "ai cover",
)
MESSAGE_PHRASES = (
    "artist message", "message from", "thank you", "thanks for listening",
    "top artist", "number one artist", "#1 artist",
)

SNAPSHOT_NAME = "data.tsv"
ART_SIZE = 104


def _device_text(value: object, limit: int = 120) -> str:
    """Keep the snapshot readable by Rockbox's compact Latin fonts."""
    return _clean(value, limit).translate(str.maketrans({
        "\u00a0": " ", "\u2018": "'", "\u2019": "'", "\u201c": '"',
        "\u201d": '"', "\u2013": "-", "\u2014": "-", "\u2026": "...",
    }))


def _cover_for_track(mount: Path, device_path: str) -> str:
    relative = str(device_path or "").lstrip("/")
    parent = (mount / relative).parent
    for name in ("cover.jpg", "folder.jpg", "cover.jpeg", "cover.bmp"):
        if (parent / name).is_file():
            return "/" + str((Path(relative).parent / name).as_posix())
    return ""


def _publish_cover(mount: Path, source_path: str, name: str) -> str:
    art_dir = mount / ".rockbox" / "spotify-wrapped"
    target = art_dir / f"{name}.bmp"
    if not source_path:
        target.unlink(missing_ok=True)
        return ""
    source = mount / source_path.lstrip("/")
    try:
        with Image.open(source) as image:
            prepared = ImageOps.fit(
                image.convert("RGB"), (ART_SIZE, ART_SIZE),
                method=Image.Resampling.LANCZOS,
            )
            art_dir.mkdir(parents=True, exist_ok=True)
            temporary = target.with_suffix(".tmp")
            prepared.save(temporary, format="BMP")
            temporary.replace(target)
    except (OSError, UnidentifiedImageError):
        target.unlink(missing_ok=True)
        return ""
    return f"/.rockbox/spotify-wrapped/{target.name}"


def _current_year_tracks(mount: Path, tracks: list[dict]) -> list[dict]:
    by_path = {
        str(track.get("device_path") or "").lstrip("/").casefold(): track
        for track in tracks
    }
    events: dict[str, list[int]] = defaultdict(lambda: [0, 0])
    for log_path in sorted((mount / ".rockbox").glob("playback*.log")):
        try:
            lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            continue
        for line in lines:
            if not line or line.startswith("#"):
                continue
            try:
                timestamp_text, elapsed_text, _length_text, device_path = line.split(":", 3)
                timestamp = int(timestamp_text)
                elapsed = int(elapsed_text)
            except (TypeError, ValueError):
                continue
            if elapsed <= 30_000 or datetime.fromtimestamp(timestamp).year != datetime.now().year:
                continue
            key = device_path.lstrip("/").casefold()
            if key not in by_path:
                continue
            events[key][0] += 1
            events[key][1] += elapsed
    annual = []
    for key, (plays, milliseconds) in events.items():
        track = dict(by_path[key])
        track["play_count"] = plays
        track["play_time"] = milliseconds
        annual.append(track)
    return annual


def _write_snapshot(mount: Path) -> str:
    """Publish a complete, deterministic Wrapped snapshot for the iPod UI."""
    lifetime_tracks = read_rockbox_tagcache_tracks(str(mount), include_runtime=True)
    lifetime_played = [
        track for track in lifetime_tracks
        if max(int(track.get("play_count") or 0), 0)
    ]
    lifetime_artist_plays: dict[str, int] = defaultdict(int)
    for track in lifetime_played:
        lifetime_artist_plays[_device_text(track.get("artist") or "Unknown artist")] += max(
            int(track.get("play_count") or 0), 0
        )
    all_time_artist = max(
        lifetime_artist_plays.items(), key=lambda item: (item[1], item[0].casefold()),
        default=("", 0),
    )[0]
    all_time_song = max(
        lifetime_played,
        key=lambda track: (
            int(track.get("play_count") or 0), int(track.get("play_time") or 0),
        ),
        default={},
    )
    annual_tracks = _current_year_tracks(mount, lifetime_tracks)
    tracks = annual_tracks or lifetime_tracks
    scope = "annual" if annual_tracks else "lifetime"
    played = [track for track in tracks if max(int(track.get("play_count") or 0), 0)]

    artists: dict[str, list] = defaultdict(lambda: [0, 0, "", -1])
    albums: dict[tuple[str, str], list] = defaultdict(lambda: [0, 0, "", -1])
    genres: dict[str, list] = defaultdict(lambda: [0, 0])
    songs = []
    for track in played:
        plays = max(int(track.get("play_count") or 0), 0)
        milliseconds = max(int(track.get("play_time") or 0), 0)
        artist = _device_text(track.get("artist") or "Unknown artist")
        album = _device_text(track.get("album") or "Unknown album")
        title = _device_text(track.get("title") or Path(track.get("device_path") or "Unknown").stem)
        genre = _device_text(track.get("genre") or "")
        path = str(track.get("device_path") or "")
        cover = _cover_for_track(mount, path)
        songs.append((plays, milliseconds, title, artist, cover, path))
        artists[artist][0] += plays
        artists[artist][1] += milliseconds
        # Keep the cover from the artist/album track with the most plays.
        if plays > artists[artist][3] and cover:
            artists[artist][2], artists[artist][3] = cover, plays
        album_key = (album, artist)
        albums[album_key][0] += plays
        albums[album_key][1] += milliseconds
        if plays > albums[album_key][3] and cover:
            albums[album_key][2], albums[album_key][3] = cover, plays
        if genre:
            genres[genre][0] += plays
            genres[genre][1] += milliseconds

    songs.sort(key=lambda row: (row[0], row[1], row[2].casefold()), reverse=True)
    artist_rows = sorted(
        ((values[0], values[1], artist, values[2]) for artist, values in artists.items()),
        reverse=True,
    )
    album_rows = sorted(
        ((values[0], values[1], album, artist, values[2])
         for (album, artist), values in albums.items()),
        reverse=True,
    )
    genre_rows = sorted(
        ((values[0], values[1], genre) for genre, values in genres.items()),
        reverse=True,
    )

    if songs:
        song = list(songs[0])
        song[4] = _publish_cover(mount, song[4], "top-song")
        songs[0] = tuple(song)
    if artist_rows:
        artist = list(artist_rows[0])
        artist[3] = _publish_cover(mount, artist[3], "top-artist")
        artist_rows[0] = tuple(artist)
    if album_rows:
        album = list(album_rows[0])
        album[4] = _publish_cover(mount, album[4], "top-album")
        album_rows[0] = tuple(album)

    lines = [
        "# rockpod-wrapped-v2",
        f"year\t{datetime.now().year}",
        f"scope\t{scope}",
        f"all_time_artist\t{all_time_artist}",
        f"plays\t{sum(row[0] for row in songs)}",
        f"seconds\t{sum(row[1] for row in songs) // 1000}",
        f"unique_tracks\t{len(played)}",
        f"unique_artists\t{len(artists)}",
        f"unique_albums\t{len(albums)}",
        f"unique_genres\t{len(genres)}",
        f"top_song_path\t/{songs[0][5].lstrip('/') if songs else ''}",
    ]
    for plays, milliseconds, title, artist, cover, _path in songs[:5]:
        lines.append(f"song\t{title}\t{artist}\t{plays}\t{milliseconds // 1000}\t{cover}")
    for plays, milliseconds, artist, cover in artist_rows[:5]:
        lines.append(f"artist\t{artist}\t\t{plays}\t{milliseconds // 1000}\t{cover}")
    for plays, milliseconds, album, artist, cover in album_rows[:5]:
        lines.append(f"album\t{album}\t{artist}\t{plays}\t{milliseconds // 1000}\t{cover}")
    for plays, milliseconds, genre in genre_rows[:5]:
        lines.append(f"genre\t{genre}\t\t{plays}\t{milliseconds // 1000}\t")

    target_dir = mount / ".rockbox" / "spotify-wrapped"
    target_dir.mkdir(parents=True, exist_ok=True)
    target = target_dir / SNAPSHOT_NAME
    temporary = target.with_suffix(".tmp")
    temporary.write_text("\n".join(lines) + "\n", encoding="utf-8")
    temporary.replace(target)
    summary = target_dir / "summary.tsv"
    summary_tmp = summary.with_suffix(".tmp")
    summary_tmp.write_text(
        f"all_time_top_artist\t{all_time_artist}\n"
        f"all_time_top_song\t{_device_text(all_time_song.get('title') or '')}\n"
        f"this_year\t{datetime.now().year}\n",
        encoding="utf-8",
    )
    summary_tmp.replace(summary)
    return all_time_artist


def _summary_artist(mount: Path) -> str:
    summary = mount / ".rockbox/spotify-wrapped/summary.tsv"
    try:
        for line in summary.read_text(encoding="utf-8").splitlines():
            key, value = line.split("\t", 1)
            if key == "all_time_top_artist" and value.strip():
                return value.strip()
    except (OSError, ValueError):
        pass
    totals: dict[str, int] = {}
    for track in read_rockbox_tagcache_tracks(str(mount), include_runtime=True):
        artist = _clean(track.get("artist"))
        plays = max(int(track.get("play_count") or 0), 0)
        if artist and plays:
            totals[artist] = totals.get(artist, 0) + plays
    if not totals:
        raise ValueError("The iPod tagcache has no lifetime plays to rank yet.")
    artist = max(totals.items(), key=lambda item: (item[1], item[0].casefold()))[0]
    summary = mount / ".rockbox/spotify-wrapped/summary.tsv"
    summary.parent.mkdir(parents=True, exist_ok=True)
    temporary = summary.with_suffix(".tmp")
    temporary.write_text(
        f"all_time_top_artist\t{artist}\n",
        encoding="utf-8",
    )
    temporary.replace(summary)
    return artist


def _clean(value: str, limit: int = 120) -> str:
    return " ".join(str(value or "").replace("\t", " ").split())[:limit]


def _run(command: list[str]) -> None:
    print("Running:", " ".join(command), flush=True)
    subprocess.run(command, check=True)


def _normalise(value: str) -> str:
    return " ".join(re.findall(r"[a-z0-9]+", str(value or "").casefold()))


def _candidate_score(entry: dict, artist: str) -> int | None:
    title = str(entry.get("title") or "")
    uploader = str(entry.get("channel") or entry.get("uploader") or "")
    description = str(entry.get("description") or "")
    title_norm = _normalise(title)
    uploader_norm = _normalise(uploader)
    blob = _normalise(f"{title} {uploader} {description}")
    artist_norm = _normalise(artist)
    if not artist_norm or artist_norm not in blob:
        return None
    if "spotify" not in blob or "wrapped" not in blob:
        return None
    if any(_normalise(phrase) in title_norm for phrase in REJECT_PHRASES):
        return None
    has_message_language = any(_normalise(phrase) in blob for phrase in MESSAGE_PHRASES)
    artist_channel = artist_norm == uploader_norm or artist_norm in uploader_norm
    spotify_channel = "spotify" in uploader_norm
    if not has_message_language or not (artist_channel or spotify_channel):
        return None
    duration = entry.get("duration")
    try:
        if duration and float(duration) > 600:
            return None
    except (TypeError, ValueError):
        pass
    score = 10
    score += 6 if artist_norm in title_norm else 0
    score += 6 if artist_channel else 0
    score += 4 if spotify_channel else 0
    score += 3 if "artist message" in title_norm else 0
    score += 2 if "#1" in title or "number one" in title_norm else 0
    return score


def _select_candidate(entries: list[dict], artist: str) -> dict | None:
    ranked = []
    for entry in entries:
        score = _candidate_score(entry, artist)
        video_id = str(entry.get("id") or "")
        if score is not None and re.fullmatch(r"[A-Za-z0-9_-]{11}", video_id):
            ranked.append((score, entry))
    return max(ranked, key=lambda pair: pair[0])[1] if ranked else None


def _discover(artist: str, yt_dlp: str) -> tuple[str, str] | None:
    query = f'ytsearch20:{artist} Spotify Wrapped artist message'
    result = subprocess.run(
        [yt_dlp, "--dump-single-json", "--flat-playlist", "--no-warnings", query],
        check=True,
        capture_output=True,
        text=True,
    )
    payload = json.loads(result.stdout)
    candidate = _select_candidate(list(payload.get("entries") or []), artist)
    if not candidate:
        return None
    video_id = str(candidate["id"])
    return f"https://www.youtube.com/watch?v={video_id}", _clean(candidate.get("title"))


def _download(url: str, destination: Path, yt_dlp: str) -> Path:
    template = destination / "artist-message-source.%(ext)s"
    _run([yt_dlp, "--no-playlist", "--merge-output-format", "mp4", "-o", str(template), url])
    candidates = sorted(
        (path for path in destination.glob("artist-message-source.*") if path.suffix != ".part"),
        key=lambda path: path.stat().st_mtime,
    )
    if not candidates:
        raise RuntimeError("yt-dlp completed without producing a video file")
    return candidates[-1]


def _remove_unmatched_message(target_dir: Path) -> None:
    for name in ("artist-message.tsv", "artist-message.mpg"):
        path = target_dir / name
        if path.exists():
            path.unlink()


def install(args: argparse.Namespace) -> Path | None:
    mount = Path(args.mount).resolve()
    if not (mount / ".rockbox").is_dir():
        raise ValueError(f"Not a Rockbox iPod mount: {mount}")
    snapshot_artist = _write_snapshot(mount)
    artist = _clean(args.artist) if args.artist else (snapshot_artist or _summary_artist(mount))
    if not artist:
        raise ValueError("Artist must not be empty")
    target_dir = mount / ".rockbox/spotify-wrapped"
    target_dir.mkdir(parents=True, exist_ok=True)
    discovered_title = ""
    selected_url = args.url
    if not args.source and not selected_url:
        discovered = _discover(artist, args.yt_dlp)
        if not discovered:
            _remove_unmatched_message(target_dir)
            print(f"No verified Spotify Wrapped YouTube message found for {artist}; installed nothing.")
            return None
        selected_url, discovered_title = discovered
        print(f"Found a verified Wrapped message: {selected_url}")
    title = _clean(args.title or discovered_title or f"A message from {artist}")
    target = target_dir / "artist-message.mpg"
    with tempfile.TemporaryDirectory(prefix="rockpod-wrapped-") as temp_dir:
        temp = Path(temp_dir)
        source = Path(args.source).resolve() if args.source else _download(selected_url, temp, args.yt_dlp)
        if not source.is_file():
            raise ValueError(f"Video source not found: {source}")
        converted = temp / "artist-message.mpg"
        _run(build_ffmpeg_command(str(source), str(converted), "video", ffmpeg_path=args.ffmpeg))
        shutil.copy2(converted, target.with_suffix(".tmp"))
        target.with_suffix(".tmp").replace(target)
    manifest = target_dir / "artist-message.tsv"
    temporary_manifest = manifest.with_suffix(".tmp")
    source_url = _clean(selected_url or str(Path(args.source).resolve()), 360)
    temporary_manifest.write_text(
        f"{artist}\t{title}\t/.rockbox/spotify-wrapped/artist-message.mpg\t{source_url}\n",
        encoding="utf-8",
    )
    temporary_manifest.replace(manifest)
    print(f"Installed an offline Wrapped message for {artist}: {target}")
    return target


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mount", required=True, help="Mounted iPod volume")
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--source", help="Local, authorised video file")
    source.add_argument("--url", help="Specific YouTube URL; otherwise RockPod searches")
    parser.add_argument("--artist", help="Artist name; defaults to Wrapped's all-time #1")
    parser.add_argument("--title", help="Message title displayed on the iPod")
    parser.add_argument("--yt-dlp", default="yt-dlp")
    parser.add_argument("--ffmpeg", default="ffmpeg")
    install(parser.parse_args())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
