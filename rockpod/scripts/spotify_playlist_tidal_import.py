#!/usr/bin/env python3
"""Resolve a public Spotify playlist and download matched tracks from Tidal."""

from __future__ import annotations

import argparse
import asyncio
import base64
import html
import json
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from urllib.error import URLError
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from streamrip.config import Config  # noqa: E402
from streamrip.media.playlist import PendingPlaylistTrack, Playlist  # noqa: E402
from streamrip.rip.main import Main  # noqa: E402


SPOTIFY_PLAYLIST_RE = re.compile(r"https?://open\.spotify\.com/playlist/([A-Za-z0-9]+)")


@dataclass(frozen=True)
class SpotifyPlaylistTrack:
    title: str
    artists: tuple[str, ...]
    uri: str = ""

    @property
    def query(self) -> str:
        artist = " ".join(self.artists[:1]).strip()
        return " ".join(part for part in (self.title, artist) if part).strip()


@dataclass(frozen=True)
class SpotifyPlaylist:
    name: str
    tracks: tuple[SpotifyPlaylistTrack, ...]
    total_count: int = 0


def _decode_script_payload(value: str) -> dict:
    text = html.unescape(str(value or "").strip())
    if not text:
        return {}
    padding = "=" * ((4 - len(text) % 4) % 4)
    decoded = base64.b64decode(text + padding)
    return json.loads(decoded)


def parse_spotify_playlist_html(page_text: str, playlist_id: str = "") -> SpotifyPlaylist:
    match = re.search(
        r'<script[^>]+id=["\']initialState["\'][^>]*>(.*?)</script>',
        page_text,
        flags=re.IGNORECASE | re.DOTALL,
    )
    if not match:
        title_match = re.search(r"<title>(.*?)</title>", page_text, flags=re.IGNORECASE | re.DOTALL)
        title = html.unescape(title_match.group(1)).strip() if title_match else ""
        suffix = f" Page title: {title}" if title else ""
        raise ValueError(f"Spotify playlist metadata was not found in the page.{suffix}")

    data = _decode_script_payload(match.group(1))
    items = ((data.get("entities") or {}).get("items") or {})
    playlist = None
    wanted_uri = f"spotify:playlist:{playlist_id}" if playlist_id else ""
    if wanted_uri and wanted_uri in items:
        playlist = items[wanted_uri]
    else:
        for value in items.values():
            if isinstance(value, dict) and str(value.get("uri") or "").startswith("spotify:playlist:"):
                playlist = value
                break
    if not isinstance(playlist, dict):
        raise ValueError("Spotify playlist metadata was not found in the page state.")

    name = str(playlist.get("name") or "Spotify Playlist").strip() or "Spotify Playlist"
    content = playlist.get("content") or {}
    raw_tracks = content.get("items") or []
    tracks = []
    for item in raw_tracks:
        track = (((item or {}).get("itemV2") or {}).get("data") or {})
        title = str(track.get("name") or "").strip()
        if not title:
            continue
        artists = []
        for artist in (((track.get("artists") or {}).get("items")) or []):
            artist_name = str((((artist or {}).get("profile") or {}).get("name")) or "").strip()
            if artist_name:
                artists.append(artist_name)
        tracks.append(
            SpotifyPlaylistTrack(
                title=title,
                artists=tuple(artists),
                uri=str(track.get("uri") or "").strip(),
            )
        )

    total_count = 0
    try:
        total_count = int(content.get("totalCount") or 0)
    except (TypeError, ValueError):
        total_count = 0

    if not tracks:
        raise ValueError("No tracks were found in the Spotify playlist page.")
    return SpotifyPlaylist(name=name, tracks=tuple(tracks), total_count=total_count)


def _fetch_with_urllib(url: str, timeout: float) -> str:
    request = Request(
        url,
        headers={
            "User-Agent": (
                "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                "(KHTML, like Gecko) Chrome/125 Safari/537.36 RockPod/0.1"
            ),
            "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        },
    )
    with urlopen(request, timeout=timeout) as response:
        return response.read().decode("utf-8", errors="replace")


def _fetch_with_curl(url: str, timeout: float) -> str:
    curl = shutil.which("curl")
    if not curl:
        raise RuntimeError("curl is not installed.")
    result = subprocess.run(
        [
            curl,
            "-fsSL",
            "--max-time",
            str(max(1, int(timeout))),
            "--compressed",
            url,
        ],
        text=True,
        capture_output=True,
        check=True,
    )
    return result.stdout


def fetch_spotify_playlist(url: str, timeout: float = 25.0) -> SpotifyPlaylist:
    playlist_id = spotify_playlist_id(url)
    candidates = [
        str(url or "").strip(),
        f"https://open.spotify.com/playlist/{playlist_id}",
    ]
    errors = []
    for candidate in dict.fromkeys(candidates):
        for fetcher in (_fetch_with_urllib, _fetch_with_curl):
            try:
                page_text = fetcher(candidate, timeout)
                return parse_spotify_playlist_html(page_text, playlist_id)
            except (URLError, OSError, subprocess.CalledProcessError, RuntimeError, ValueError) as exc:
                errors.append(f"{fetcher.__name__} {candidate}: {exc}")
                continue
    detail = "\n".join(errors[-4:])
    raise RuntimeError(f"Could not fetch Spotify playlist metadata.\n{detail}")


def spotify_playlist_id(url: str) -> str:
    match = SPOTIFY_PLAYLIST_RE.match(str(url or "").strip())
    if not match:
        raise ValueError("Enter an open.spotify.com playlist URL.")
    return match.group(1)


async def import_spotify_playlist_as_tidal(
    config_path: str,
    url: str,
    output_format: str,
    quality: int,
    source: str = "tidal",
    fallback_source: str = "",
) -> SpotifyPlaylist:
    playlist = fetch_spotify_playlist(url)
    config = Config(config_path)
    output_format = str(output_format or "").strip().upper()
    if output_format:
        config.session.conversion.enabled = True
        config.session.conversion.codec = output_format
    quality = max(0, min(4, int(quality or 4)))
    config.session.qobuz.quality = quality
    config.session.tidal.quality = min(3, quality)
    config.session.deezer.quality = min(2, quality)
    config.session.lastfm.source = source
    config.session.lastfm.fallback_source = fallback_source

    async with Main(config) as main:
        client = await main.get_logged_in_client(source)
        fallback_client = await main.get_logged_in_client(fallback_source) if fallback_source else None
        folder = os.path.join(config.session.downloads.folder, playlist.name)
        pending_tracks = []
        found = 0
        failed = 0
        print(f"Resolving Spotify playlist '{playlist.name}' through {source.upper()}...", flush=True)
        if playlist.total_count and playlist.total_count > len(playlist.tracks):
            print(
                f"Spotify exposed {len(playlist.tracks)} of {playlist.total_count} tracks without a Spotify login.",
                flush=True,
            )
        for position, track in enumerate(playlist.tracks, start=1):
            query = track.query
            pages = await client.search("track", query, limit=1)
            track_client = client
            from_fallback = False
            if not pages and fallback_client is not None:
                pages = await fallback_client.search("track", query, limit=1)
                track_client = fallback_client
                from_fallback = bool(pages)
            if not pages:
                failed += 1
                print(f"Not found on {source.upper()}: {query}", flush=True)
                continue
            from streamrip.metadata import SearchResults

            result = SearchResults.from_pages(track_client.source, "track", pages).results[0]
            pending_tracks.append(
                PendingPlaylistTrack(
                    result.id,
                    track_client,
                    config,
                    folder,
                    playlist.name,
                    position,
                    main.database,
                )
            )
            found += 1
            suffix = f" via {track_client.source.upper()}" if from_fallback else ""
            print(f"Matched {position:02d}. {query}{suffix}", flush=True)

        if not pending_tracks:
            raise RuntimeError(f"No Spotify playlist tracks matched on {source.upper()}.")
        print(f"Downloading {found} matched track(s); {failed} unmatched.", flush=True)
        media = Playlist(playlist.name, config, client, pending_tracks)
        await media.rip()
    return playlist


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config-path", required=True)
    parser.add_argument("--url", required=True)
    parser.add_argument("--output-format", default="flac")
    parser.add_argument("--quality", type=int, default=4)
    parser.add_argument("--source", default="tidal")
    parser.add_argument("--fallback-source", default="")
    args = parser.parse_args()

    asyncio.run(
        import_spotify_playlist_as_tidal(
            args.config_path,
            args.url,
            args.output_format,
            args.quality,
            args.source,
            args.fallback_source,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
