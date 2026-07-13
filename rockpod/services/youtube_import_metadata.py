"""Helpers for auto-matching metadata for imported YouTube videos."""

import os
import re
from pathlib import Path


_VIDEO_EXTENSIONS = {
    ".3gp",
    ".aac",
    ".avi",
    ".flac",
    ".flv",
    ".m3u8",
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
}


_VIDEO_EPISODE_OVERRIDES = {
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


def _apply_video_episode_overrides(track_data):
    track_data = dict(track_data or {})
    if track_data.get("metadata_locked") and (
        track_data.get("show_title")
        and track_data.get("season_number") is not None
        and track_data.get("episode_number") is not None
    ):
        return {}
    if (
        str(track_data.get("metadata_source") or "").strip() not in {"", "filename"}
        and track_data.get("show_title")
        and track_data.get("season_number") is not None
        and track_data.get("episode_number") is not None
    ):
        return {}
    candidates = [
        track_data.get("title"),
        Path(str(track_data.get("file_path") or "")).stem,
    ]
    for candidate in candidates:
        override = _VIDEO_EPISODE_OVERRIDES.get(_video_override_key(candidate))
        if override:
            return dict(override)
    return {}


def _strip_trailing_video_extension(path_or_title):
    text = str(path_or_title or "").strip()
    if not text:
        return text
    for extension in _VIDEO_EXTENSIONS:
        if text.lower().endswith(extension):
            return text[: -len(extension)]
    return text


def _safe_int(value):
    try:
        if value is None:
            return None
        text = str(value).strip()
        if not text:
            return None
        return int(text)
    except (TypeError, ValueError):
        return None


def _normalize_title_text(value):
    text = str(value or "").strip()
    if not text:
        return ""
    text = _strip_trailing_video_extension(text)
    text = text.replace("_", " ")
    text = text.replace("-", " ")
    text = re.sub(r"\s*\([^)]*\)\s*", " ", text)
    text = re.sub(r"\s*\[[^\]]*\]\s*", " ", text)
    text = re.sub(r"\s+", " ", text).strip(" -_")
    return text.strip()


def _normalize_for_match(value):
    return " ".join(str(value or "").strip().casefold().split())


def _looks_like_episode_title(value):
    value = _normalize_for_match(value)
    if not value:
        return False
    return bool(
        re.search(
            r"\b(?:s\d{1,2}e\d{1,3}|"
            r"\d{1,2}x\d{1,3}|"
            r"\bseason\s*\d{1,2}.*\b(?:episode|ep)\s*\d{1,3}|"
            r"\b(?:episode|ep)\s*\d{1,4}\b)",
            value,
        )
    )


def _strip_episode_suffix(value):
    text = _normalize_title_text(value)
    if not text:
        return ""

    if re.search(r"(?i)\b(?:episode|ep)\b", text):
        text = re.sub(
            r"\s*[-_:.]?\s*"
            r"(?:season\s*\d{1,2}\s*[-_:.]?\s*)?"
            r"(?:episode|ep)\s*\d{1,4}.*$",
            "",
            text,
        )

    if re.search(r"(?i)\bs\d{1,2}e\d{1,3}\b", text):
        text = re.sub(r"\s*S\d{1,2}E\d{1,3}.*$", "", text, flags=re.IGNORECASE)
    text = re.sub(r"\b\d{1,2}x\d{1,3}\s*.*$", "", text, flags=re.IGNORECASE)
    return re.sub(r"\s+", " ", text).strip(" -_")


def _clean_show_title(value):
    text = _normalize_title_text(value)
    if not text:
        return ""
    text = re.sub(r"(?i)\b(?:episode|ep|full\s+series|complete\s+series)\b.*$", "", text)
    text = re.sub(r"\s+S\d{1,2}E\d{1,3}.*$", "", text)
    text = re.sub(r"\s+\d{1,2}x\d{1,3}.*$", "", text)
    text = re.sub(r"\s{2,}", " ", text)
    return text.strip(" -_")


def _parse_uploader_owned_show_hint(value, uploader=""):
    text = _normalize_title_text(value)
    if not text:
        return {}

    candidates = [text]
    if " - " in text:
        candidates.append(text.rsplit(" - ", 1)[-1].strip())

    normalized_uploader = _normalize_for_match(uploader)
    for candidate in candidates:
        match = re.match(r"(?i)^(?P<host>.+?)['’]\s*s\s+(?P<episode>.+)$", candidate)
        if not match:
            continue

        host = match.group("host").strip()
        episode = match.group("episode").strip()
        if not host or not episode:
            continue

        host_key = _normalize_for_match(host)
        if normalized_uploader and host_key and host_key != normalized_uploader:
            continue
        if not normalized_uploader and len(host.split()) < 2:
            continue

        return {
            "show_title": _clean_show_title(episode),
            "episode_title": _normalize_title_text(episode),
        }

    return {}


def _parse_episode_hint_from_title(value, uploader=""):
    text = _normalize_title_text(value)
    if not text:
        return {}

    patterns = [
        re.compile(
            r"(?i)^(?P<show>.+?)\s*[-_:.]?\s*"
            r"S(?P<season>\d{1,2})E(?P<episode>\d{1,3})\b"
            r"(?:\s*[-:.]?\s*(?P<episode_title>.+))?$"
        ),
        re.compile(
            r"(?i)^(?P<show>.+?)\s*[-_:.]?\s*"
            r"(?P<season>\d{1,2})x(?P<episode>\d{1,3})\b"
            r"(?:\s*[-:.]?\s*(?P<episode_title>.+))?$"
        ),
        re.compile(
            r"(?i)^(?P<show>.+?)\s*[-_:.]?\s*"
            r"(?:season|series)\s*(?P<season>\d{1,2})\s*[-_.:]?\s*"
            r"(?:episode|ep)\s*(?P<episode>\d{1,3})"
            r"(?:\s*[-:.]?\s*(?P<episode_title>.+))?$"
        ),
        re.compile(
            r"(?i)^(?P<show>.+?)\s*[-_:.]?\s*"
            r"(?:episode|ep)\s*(?P<episode>\d{1,4})\b"
            r"(?:\s*[-:.]?\s*(?P<episode_title>.+))?$"
        ),
    ]

    for pattern in patterns:
        match = pattern.match(text)
        if not match:
            continue
        show_title = _clean_show_title(match.group("show"))
        season_text = match.groupdict().get("season")
        episode = _safe_int(match.group("episode"))
        if episode is None:
            continue
        season = _safe_int(season_text)
        if season_text is not None and season is None:
            continue
        if season is None:
            season = 1
        episode_title = str(match.group("episode_title") or "").strip()
        if episode_title:
            episode_title = _normalize_title_text(episode_title)
        return {
            "show_title": show_title,
            "season_number": season,
            "episode_number": episode,
            "episode_title": episode_title,
            "from_title": text,
        }

    match = re.match(r"(?i)^(.+?)\s*[-:]\s*(?:episode|ep)\s*(\d{1,4})\s*(?:[-:].*)?$", text)
    if match:
        show_title = _clean_show_title(match.group(1))
        episode = _safe_int(match.group(2))
        if episode:
            return {
                "show_title": show_title,
                "season_number": 1,
                "episode_number": episode,
                "from_title": text,
            }

    owner_show = _parse_uploader_owned_show_hint(text, uploader=uploader)
    if owner_show:
        owner_show["season_number"] = None
        owner_show["episode_number"] = None
        return owner_show

    return {}


def _parse_competition_show_hint(value):
    """Recognize YouTube episode titles such as ``Title - Kenny vs. Spenny``."""
    text = _strip_trailing_video_extension(str(value or "").replace("_", " ")).strip()
    text = re.sub(r"\s+", " ", text)
    match = re.match(r"^(.+?)\s+[-–—:]\s+(.+)$", text)
    if not match:
        return {}

    left = re.sub(r"\s*\([^)]*\)\s*$", "", match.group(1)).strip()
    right = re.sub(r"\s*\([^)]*\)\s*$", "", match.group(2)).strip()

    def is_show_name(candidate):
        return bool(
            re.search(r"\bvs\.?\b", candidate, flags=re.IGNORECASE)
            and not re.search(r"\s[-–—:]\s", candidate)
            and not re.search(r"\[[^]]+\]", candidate)
            and len(candidate.split()) <= 6
        )

    if is_show_name(left):
        show_title, episode_title = left, right
    elif is_show_name(right):
        show_title, episode_title = right, left
    else:
        return {}

    return {
        "show_title": _clean_show_title(show_title),
        "season_number": 1,
        "episode_number": None,
        "episode_title": _normalize_title_text(episode_title),
    }


def _extract_episode_hints(track_data, pending_item):
    track_data = dict(track_data or {})
    pending_item = dict(pending_item or {})

    overrides = _apply_video_episode_overrides(track_data)
    if overrides:
        return {
            "show_title": _clean_show_title(overrides.get("show_title")),
            "season_number": _safe_int(overrides.get("season_number")),
            "episode_number": _safe_int(overrides.get("episode_number")),
            "episode_title": _normalize_title_text(overrides.get("title") or track_data.get("title") or ""),
            "uploader": str(pending_item.get("uploader") or "").strip(),
        }

    hints = {
        "show_title": _clean_show_title(
            pending_item.get("show_title") or track_data.get("show_title")
        ),
        "season_number": _safe_int(
            pending_item.get("season_number")
            if pending_item.get("season_number") is not None
            else track_data.get("season_number")
        ),
        "episode_number": _safe_int(
            pending_item.get("episode_number")
            if pending_item.get("episode_number") is not None
            else track_data.get("episode_number")
        ),
        "episode_title": _normalize_title_text(
            pending_item.get("episode_title") or track_data.get("title")
        ),
        "uploader": str(pending_item.get("uploader") or "").strip(),
    }

    query_hint = _normalize_title_text(pending_item.get("query_hint"))
    track_title = _normalize_title_text(track_data.get("title"))
    file_title = _normalize_title_text(os.path.basename(str(track_data.get("file_path") or "")))
    parsed_sources = [query_hint, track_title, file_title]
    raw_sources = [
        pending_item.get("query_hint"),
        track_data.get("title"),
        os.path.basename(str(track_data.get("file_path") or "")),
    ]
    for source in raw_sources:
        parsed = _parse_competition_show_hint(source)
        if not parsed:
            continue
        hints["show_title"] = hints["show_title"] or parsed.get("show_title") or ""
        if hints["season_number"] is None:
            hints["season_number"] = parsed.get("season_number")
        if parsed.get("episode_title"):
            hints["episode_title"] = parsed["episode_title"]
        break

    for source in parsed_sources:
        parsed = _parse_episode_hint_from_title(source, uploader=hints["uploader"])
        if not parsed:
            continue
        if not hints["show_title"] and parsed.get("show_title"):
            hints["show_title"] = parsed.get("show_title") or hints["show_title"]
        if hints["season_number"] is None and parsed.get("season_number") is not None:
            hints["season_number"] = parsed.get("season_number")
        if hints["episode_number"] is None and parsed.get("episode_number") is not None:
            hints["episode_title"] = str(hints["episode_title"] or parsed.get("episode_title") or "").strip()
            hints["episode_number"] = parsed.get("episode_number")
        if not hints["episode_title"] and parsed.get("episode_title"):
            hints["episode_title"] = str(parsed["episode_title"] or "").strip()

    return hints


def _candidate_queries(track_data, pending_item, hints):
    pending_item = dict(pending_item or {})
    track_data = dict(track_data or {})
    query_hint = _normalize_title_text(pending_item.get("query_hint"))
    track_title = _normalize_title_text(track_data.get("title"))
    file_title = _normalize_title_text(os.path.basename(str(track_data.get("file_path") or "")))
    show_title = _normalize_title_text(hints.get("show_title"))
    episode_title = _normalize_title_text(hints.get("episode_title"))
    season_number = hints.get("season_number")
    episode_number = hints.get("episode_number")
    uploader = _normalize_title_text(hints.get("uploader"))

    queries = []
    seen = set()

    def add(candidate):
        normalized = _normalize_for_match(candidate)
        if not normalized or normalized in seen:
            return
        seen.add(normalized)
        queries.append(candidate.strip())

    add(query_hint)
    add(track_title)
    add(_strip_episode_suffix(query_hint))
    add(_strip_episode_suffix(track_title))
    add(_strip_episode_suffix(file_title))

    if show_title:
        if episode_title and _normalize_for_match(episode_title) != _normalize_for_match(show_title):
            add(f"{show_title} {episode_title}")
        if season_number is not None and episode_number is not None:
            add(f"{show_title} S{season_number:02d}E{episode_number:02d}")
        add(show_title)
        if uploader and uploader.casefold() != show_title.casefold():
            add(f"{uploader} {show_title}")
            add(f"{show_title} {uploader}")

    if uploader:
        add(f"{uploader} {query_hint or track_title}")
        add(f"{uploader} {show_title}".strip())
    add(file_title)

    return queries


def youtube_metadata_lookup_query(track_data, pending_item):
    query_hint = _normalize_title_text((pending_item or {}).get("query_hint"))
    if query_hint:
        return query_hint

    track_title = _normalize_title_text(
        (track_data or {}).get("show_title") or (track_data or {}).get("title")
    )
    if track_title:
        return track_title

    file_path = str((track_data or {}).get("file_path") or "").strip()
    if not file_path:
        return ""

    file_stem = os.path.splitext(os.path.basename(file_path))[0]
    return _normalize_title_text(file_stem)


def _choose_best_query_match(query, results):
    exact = []
    containing = []
    rest = []
    target = _normalize_for_match(query)
    if not target:
        return results[0] if results else None

    for result in results or []:
        title = _normalize_for_match(result.get("title") or "")
        if not title:
            continue
        if title == target:
            exact.append((title, result))
        elif target in title or title in target:
            containing.append((title, result))
        else:
            rest.append((title, result))

    if exact:
        return exact[0][1]
    if containing:
        return containing[0][1]
    if rest:
        return rest[0][1]
    return results[0]


def choose_video_metadata_match(query, results):
    if not results:
        return None
    if not query:
        return results[0]
    return _choose_best_query_match(query, results)


def _year_from_track(track_data):
    year_value = track_data.get("year")
    if str(year_value or "").isdigit():
        year = int(year_value)
        return year if year >= 1800 else None
    return None


def _search_metadata(service, track_data, pending_item, hints):
    query_candidates = _candidate_queries(track_data, pending_item, hints)
    if not query_candidates:
        return None, "", ""

    year = _year_from_track(track_data)
    track_kind = str(track_data.get("video_kind") or "").strip().lower()
    has_show_evidence = bool(
        hints.get("show_title")
        or hints.get("season_number") is not None
        or hints.get("episode_number") is not None
        or track_kind == "show"
        or _looks_like_episode_title(_normalize_title_text(pending_item.get("query_hint") or track_data.get("title")))
    )
    preferred_kind = "tv_show" if (
        track_kind == "show"
        or hints.get("show_title")
        or hints.get("season_number")
        or hints.get("episode_number")
        or _looks_like_episode_title(_normalize_title_text(pending_item.get("query_hint") or track_data.get("title")))
    ) else "movie"
    search_order = [preferred_kind]
    alternate_kind = "movie" if preferred_kind == "tv_show" else "tv_show"
    if has_show_evidence:
        search_order.append(alternate_kind)

    for media_kind in search_order:
        for query in query_candidates:
            results = service.search(query, media_type=media_kind, year=year)
            if results:
                return results, media_kind, query
    return None, "", ""


def _coalesce_match_to_updates(match, track_data, hints):
    media_kind = str(match.get("media_type") or "").strip()
    updates = {
        "metadata_source": "online",
        "metadata_confidence": 1.0,
        "imdb_id": str(match.get("imdb_id") or ""),
        "tmdb_id": str(match.get("tmdb_id") or ""),
        "genre": str(match.get("genre") or track_data.get("genre") or ""),
        "year": match.get("year") or track_data.get("year") or None,
        "plot_short": str(match.get("plot_short") or ""),
        "plot_long": str(match.get("plot_long") or ""),
    }

    if media_kind in {"movie", "tv_episode"}:
        updates["title"] = str(
            match.get("episode_title") or match.get("title") or track_data.get("title") or ""
        )
        updates["video_kind"] = "movie" if media_kind == "movie" else "show"
        if hints.get("show_title"):
            updates["show_title"] = str(hints.get("show_title"))
            updates["video_kind"] = "show"
        if hints.get("season_number"):
            updates["season_number"] = hints.get("season_number")
        if hints.get("episode_number"):
            updates["episode_number"] = hints.get("episode_number")
        return _complete_show_updates(updates) if updates["video_kind"] == "show" else updates

    if media_kind == "tv_show":
        updates["show_title"] = str(
            match.get("title")
            or hints.get("show_title")
            or track_data.get("show_title")
            or track_data.get("title")
            or ""
        )
        updates["video_kind"] = "show"
        if hints.get("season_number"):
            updates["season_number"] = hints.get("season_number")
        if hints.get("episode_number"):
            updates["episode_number"] = hints.get("episode_number")
        if hints.get("episode_title"):
            updates["title"] = str(hints.get("episode_title"))
        return _complete_show_updates(updates)

    return updates


def _complete_show_updates(updates):
    updates = dict(updates or {})
    show_title = str(updates.get("show_title") or "").strip()
    season = _safe_int(updates.get("season_number"))
    episode = _safe_int(updates.get("episode_number"))
    if show_title:
        updates["artist"] = show_title
        updates["album_artist"] = show_title
    if season is not None:
        updates["album"] = "Specials" if season == 0 else f"Season {season:02d}"
    if episode is not None:
        updates["track_number"] = episode
    return updates


def _youtube_fallback_payload(track_data, pending_item, hints):
    has_source_metadata = any(
        pending_item.get(key) not in (None, "", [])
        for key in (
            "uploader",
            "source_url",
            "description",
            "thumbnail",
            "poster_path",
            "year",
            "show_title",
            "season_number",
            "episode_number",
            "categories",
        )
    )
    if not has_source_metadata:
        return None
    title = str(
        pending_item.get("episode_title")
        or pending_item.get("query_hint")
        or track_data.get("title")
        or ""
    ).strip()
    uploader = str(pending_item.get("uploader") or "").strip()
    show_title = str(hints.get("show_title") or "").strip()
    categories = list(pending_item.get("categories") or [])
    updates = {
        "metadata_source": "youtube",
        "metadata_confidence": 0.65,
        "title": title,
        "artist": show_title or uploader,
        "album_artist": show_title or uploader,
        "genre": str(categories[0] if categories else track_data.get("genre") or ""),
        "year": pending_item.get("year") or track_data.get("year") or None,
        "comment": str(pending_item.get("source_url") or ""),
        "plot_short": str(pending_item.get("description") or "")[:1000],
        "plot_long": str(pending_item.get("description") or ""),
        "video_kind": "show" if show_title else "movie",
    }
    if show_title:
        updates["show_title"] = show_title
    if hints.get("season_number") is not None:
        updates["season_number"] = hints["season_number"]
    if hints.get("episode_number") is not None:
        updates["episode_number"] = hints["episode_number"]
    if updates["video_kind"] == "show":
        updates = _complete_show_updates(updates)
    return {
        "updates": updates,
        "artwork_url": str(pending_item.get("thumbnail") or ""),
    }


def _has_show_lookup_evidence(track_data, pending_item, hints):
    if str(track_data.get("video_kind") or "").strip().lower() == "show":
        return True

    if _clean_show_title(track_data.get("show_title") or ""):
        return True

    if hints.get("show_title"):
        return True

    if hints.get("season_number") is not None or hints.get("episode_number") is not None:
        return True

    query_hint = _normalize_title_text(pending_item.get("query_hint") or "")
    if _looks_like_episode_title(query_hint):
        return True

    title = _normalize_title_text(track_data.get("title") or "")
    if _looks_like_episode_title(title):
        return True

    return False


def build_youtube_import_metadata_payload(track_data, pending_item, service):
    track_data = dict(track_data or {})
    if track_data.get("media_type") != "video":
        return None
    if track_data.get("metadata_locked"):
        return None
    pending_item = dict(pending_item or {})
    hints = _extract_episode_hints(track_data, pending_item)

    if track_data.get("metadata_source") and str(track_data.get("metadata_source")).strip() not in {"", "filename"}:
        if not _has_show_lookup_evidence(track_data, pending_item, hints):
            return None

    query = youtube_metadata_lookup_query(track_data, pending_item)
    if not query:
        return None

    results, matched_kind, matched_query = _search_metadata(service, track_data, pending_item, hints)
    if not results:
        return _youtube_fallback_payload(track_data, pending_item, hints)

    match = choose_video_metadata_match(matched_query, results)
    if not match:
        return _youtube_fallback_payload(track_data, pending_item, hints)

    if match.get("media_type") == "tv_show" and hints.get("season_number") and hints.get("episode_number"):
        episode = service.lookup_episode(
            match.get("title") or "",
            hints.get("season_number"),
            hints.get("episode_number"),
            hints.get("episode_title") or track_data.get("title"),
        )
        if episode:
            enriched = dict(match)
            enriched.update(
                {
                    "media_type": "tv_episode",
                    "episode_title": episode.get("title") or enriched.get("episode_title") or "",
                    "plot_short": episode.get("plot_short") or enriched.get("plot_short") or "",
                    "plot_long": episode.get("plot_long") or enriched.get("plot_long") or "",
                    "release_date": episode.get("release_date") or enriched.get("release_date") or "",
                    "runtime_seconds": episode.get("runtime_seconds") or enriched.get("runtime_seconds") or 0,
                    "genre": episode.get("genre") or enriched.get("genre") or "",
                }
            )
            match = enriched

    updates = _coalesce_match_to_updates(match, track_data, hints)
    return {
        "updates": updates,
        "artwork_url": match.get("artwork_url") or "",
    }
