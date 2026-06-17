#!/usr/bin/env python3
"""Search streamrip sources and emit RockPod storefront JSON."""

from __future__ import annotations

import argparse
import asyncio
import hashlib
import json
import os
from pathlib import Path

import aiohttp

from streamrip.config import Config
from streamrip.rip.main import Main


HOME_TAB_LABELS = {
    "featured": "Featured",
    "new_releases": "New Releases",
    "top_albums": "Top Albums",
    "just_added": "Just Added",
    "alternative": "Alternative",
    "rock": "Rock",
    "hip_hop": "Hip-Hop/Rap",
}

HOME_TAB_SECTIONS = {
    "featured": (
        "Popular albums",
        "New releases for you",
        "Suggested new albums for you",
    ),
    "new_releases": (
        "New releases",
        "New releases for you",
        "Suggested new albums for you",
    ),
    "top_albums": (
        "Popular albums",
        "Top albums",
        "Most popular albums",
    ),
    "just_added": (
        "New releases",
        "New albums",
        "Just added",
    ),
}

HOME_TAB_SEARCH_QUERIES = {
    "new_releases": "new releases",
    "just_added": "new music",
    "alternative": "alternative",
    "rock": "rock",
    "hip_hop": "hip hop rap",
}


def _tidal_cover_url(cover_id: str, size: int = 320) -> str:
    text = str(cover_id or "").strip()
    if not text:
        return ""
    return f"https://resources.tidal.com/images/{text.replace('-', '/')}/{size}x{size}.jpg"


def _album_url(source: str, item_id: str) -> str:
    if source == "tidal":
        return f"https://tidal.com/album/{item_id}"
    if source == "qobuz":
        return f"https://open.qobuz.com/album/{item_id}"
    if source == "deezer":
        return f"https://www.deezer.com/album/{item_id}"
    return ""


def _track_url(source: str, item_id: str) -> str:
    if source == "tidal":
        return f"https://tidal.com/track/{item_id}"
    if source == "qobuz":
        return f"https://open.qobuz.com/track/{item_id}"
    if source == "deezer":
        return f"https://www.deezer.com/track/{item_id}"
    return ""


def _int_value(value, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _artist_name(item: dict) -> str:
    artists = item.get("artists")
    if isinstance(artists, list) and artists:
        names = [str(artist.get("name") or "").strip() for artist in artists if isinstance(artist, dict)]
        names = [name for name in names if name]
        if names:
            return ", ".join(names)
    artist = item.get("artist") or item.get("performer") or {}
    if isinstance(artist, dict):
        return str(artist.get("name") or "Unknown Artist")
    return str(artist or "Unknown Artist")


def _normalize_album(source: str, item: dict) -> dict:
    item_id = str(item.get("id") or "")
    title = str(item.get("title") or item.get("name") or "Untitled Album").strip()
    version = str(item.get("version") or "").strip()
    if version:
        title = f"{title} ({version})"
    cover_url = _tidal_cover_url(item.get("cover", "")) if source == "tidal" else ""
    return {
        "source": source,
        "media_type": "album",
        "id": item_id,
        "title": title,
        "artist": _artist_name(item),
        "date": str(
            item.get("releaseDate")
            or item.get("release_date")
            or item.get("release_date_original")
            or item.get("year")
            or ""
        ),
        "tracks": int(item.get("numberOfTracks") or item.get("tracks_count") or len(item.get("tracks", [])) or 0),
        "url": _album_url(source, item_id),
        "cover_url": cover_url,
        "cover_path": "",
        "section": "",
    }


def _track_artist_name(item: dict, fallback: str = "") -> str:
    artist = _artist_name(item)
    if artist and artist != "Unknown Artist":
        return artist
    performer = item.get("performer") or {}
    if isinstance(performer, dict):
        return str(performer.get("name") or fallback or "").strip()
    return str(fallback or "").strip()


def _normalize_track(source: str, item: dict, fallback_artist: str = "") -> dict:
    track_id = str(item.get("id") or "")
    duration = item.get("duration")
    if duration is None:
        duration = item.get("duration_seconds")
    if duration is None and item.get("duration_ms"):
        try:
            duration = int(item.get("duration_ms") or 0) // 1000
        except (TypeError, ValueError):
            duration = 0
    return {
        "source": source,
        "media_type": "track",
        "id": track_id,
        "title": str(item.get("title") or item.get("name") or "Untitled Track").strip(),
        "artist": _track_artist_name(item, fallback_artist),
        "track_number": _int_value(
            item.get("trackNumber")
            or item.get("track_number")
            or item.get("track_position")
            or item.get("position")
        ),
        "disc_number": _int_value(item.get("volumeNumber") or item.get("disc_number") or item.get("media_number"), 1),
        "duration": _int_value(duration),
        "url": _track_url(source, track_id),
    }


def _album_tracks(item: dict) -> list[dict]:
    tracks = item.get("tracks") or []
    if isinstance(tracks, dict):
        tracks = tracks.get("items") or tracks.get("data") or []
    if not isinstance(tracks, list):
        return []
    return [track for track in tracks if isinstance(track, dict)]


async def _download_cover(session: aiohttp.ClientSession, result: dict, cover_dir: Path) -> None:
    url = result.get("cover_url") or ""
    if not url:
        return
    digest = hashlib.sha256(url.encode("utf-8")).hexdigest()[:24]
    target = cover_dir / f"{digest}.jpg"
    result["cover_path"] = str(target)
    if target.is_file():
        return
    try:
        async with session.get(url) as response:
            if response.status != 200:
                result["cover_path"] = ""
                return
            data = await response.read()
    except Exception:
        result["cover_path"] = ""
        return
    cover_dir.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)


async def search(config_path: str, source: str, media_type: str, query: str, limit: int, cover_dir: str) -> list[dict]:
    if media_type != "album":
        raise ValueError("RockPod storefront search currently supports album results.")

    config = Config(config_path)
    async with Main(config) as main:
        client = await main.get_logged_in_client(source)
        pages = await client.search(media_type, query, limit=limit)

    results = []
    for page in pages:
        items = page.get("items", [])
        if source == "qobuz":
            items = page.get("albums", {}).get("items", [])
        elif source == "deezer":
            items = page.get("data", [])
        for item in items:
            result = _normalize_album(source, item)
            if result["id"] and result["url"]:
                results.append(result)
            if len(results) >= limit:
                break
        if len(results) >= limit:
            break

    if cover_dir:
        connector = aiohttp.TCPConnector(ssl=False)
        async with aiohttp.ClientSession(connector=connector) as session:
            await asyncio.gather(*[_download_cover(session, result, Path(cover_dir)) for result in results])
    return results


async def _search_with_client(client, source: str, media_type: str, query: str, limit: int) -> list[dict]:
    pages = await client.search(media_type, query, limit=limit)
    results = []
    for page in pages:
        items = page.get("items", [])
        if source == "qobuz":
            items = page.get("albums", {}).get("items", [])
        elif source == "deezer":
            items = page.get("data", [])
        for item in items:
            result = _normalize_album(source, item)
            if result["id"] and result["url"]:
                result["section"] = HOME_TAB_LABELS.get(str(query).replace(" ", "_"), str(query).title())
                results.append(result)
            if len(results) >= limit:
                return results
    return results


async def album_detail(config_path: str, source: str, album_id: str, cover_dir: str) -> dict:
    source = str(source or "tidal").strip().lower()
    item_id = str(album_id or "").strip()
    if source not in {"tidal", "qobuz", "deezer"}:
        raise ValueError("Album details support Tidal, Qobuz, or Deezer.")
    if not item_id:
        raise ValueError("Album details require an album id.")

    config = Config(config_path)
    async with Main(config) as main:
        client = await main.get_logged_in_client(source)
        item = await client.get_metadata(item_id, "album")

    result = _normalize_album(source, item)
    fallback_artist = result.get("artist") or ""
    track_items = [_normalize_track(source, track, fallback_artist) for track in _album_tracks(item)]
    result["track_items"] = sorted(
        track_items,
        key=lambda track: (track.get("disc_number") or 1, track.get("track_number") or 0),
    )
    if not result.get("tracks"):
        result["tracks"] = len(result["track_items"])
    if cover_dir:
        connector = aiohttp.TCPConnector(ssl=False)
        async with aiohttp.ClientSession(connector=connector) as session:
            await _download_cover(session, result, Path(cover_dir))
    return result


def _page_module_albums(page: dict, wanted_titles: tuple[str, ...], limit: int) -> list[dict]:
    results = []
    wanted = tuple(title.casefold() for title in wanted_titles)
    wanted_rank = {title: index for index, title in enumerate(wanted)}
    modules = []
    for row in page.get("rows", []):
        for module in row.get("modules", []):
            title = str(module.get("title") or "").strip()
            if wanted and title.casefold() not in wanted:
                continue
            if str(module.get("type") or "").upper() != "ALBUM_LIST":
                continue
            modules.append((wanted_rank.get(title.casefold(), len(modules)), module))
    for _rank, module in sorted(modules, key=lambda item: item[0]):
        title = str(module.get("title") or "").strip()
        items = (module.get("pagedList") or {}).get("items") or []
        for item in items:
            result = _normalize_album("tidal", item)
            if result["id"] and result["url"]:
                result["section"] = title
                results.append(result)
            if len(results) >= limit:
                return results
    return results


async def homepage(config_path: str, limit: int, cover_dir: str, home_tab: str = "featured") -> list[dict]:
    home_tab = str(home_tab or "featured").strip().lower()
    if home_tab not in HOME_TAB_LABELS:
        home_tab = "featured"
    config = Config(config_path)
    async with Main(config) as main:
        client = await main.get_logged_in_client("tidal")
        page = await client._api_request("pages/home", {"deviceType": "BROWSER"})
        preferred_sections = HOME_TAB_SECTIONS.get(home_tab, tuple())
        results = _page_module_albums(page, preferred_sections, limit)
        if len(results) < limit and home_tab in HOME_TAB_SEARCH_QUERIES:
            seen = {result["id"] for result in results}
            searched = await _search_with_client(
                client,
                "tidal",
                "album",
                HOME_TAB_SEARCH_QUERIES[home_tab],
                limit * 2,
            )
            for result in searched:
                if result["id"] in seen:
                    continue
                result["section"] = HOME_TAB_LABELS[home_tab]
                results.append(result)
                seen.add(result["id"])
                if len(results) >= limit:
                    break

    if len(results) < limit:
        seen = {result["id"] for result in results}
        for result in _page_module_albums(page, tuple(), limit * 2):
            if result["id"] in seen:
                continue
            if not result.get("section"):
                result["section"] = HOME_TAB_LABELS[home_tab]
            results.append(result)
            seen.add(result["id"])
            if len(results) >= limit:
                break

    if cover_dir:
        connector = aiohttp.TCPConnector(ssl=False)
        async with aiohttp.ClientSession(connector=connector) as session:
            await asyncio.gather(*[_download_cover(session, result, Path(cover_dir)) for result in results])
    return results[:limit]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config-path", required=True)
    parser.add_argument("--source", default="tidal")
    parser.add_argument("--media-type", default="album")
    parser.add_argument("--query", default="")
    parser.add_argument("--homepage", action="store_true")
    parser.add_argument("--home-tab", default="featured")
    parser.add_argument("--album-id", default="")
    parser.add_argument("--limit", type=int, default=24)
    parser.add_argument("--cover-dir", default="")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    if args.album_id:
        results = asyncio.run(album_detail(args.config_path, args.source, args.album_id, args.cover_dir))
    elif args.homepage:
        results = asyncio.run(homepage(args.config_path, args.limit, args.cover_dir, args.home_tab))
    else:
        results = asyncio.run(
            search(args.config_path, args.source, args.media_type, args.query, args.limit, args.cover_dir)
        )
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(results, indent=2))
    count = 1 if isinstance(results, dict) else len(results)
    print(f"Wrote {count} result{'s' if count != 1 else ''} to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
