"""Online artwork lookup using Plex first for video, then Apple's iTunes Search API."""

import json
import os
import re
import threading
import time
import unicodedata
import xml.etree.ElementTree as ET
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode
from urllib.request import Request, urlopen


_PUNCT_TRANSLATION = str.maketrans(
    {
        "\u2018": "'",
        "\u2019": "'",
        "\u201a": "'",
        "\u201b": "'",
        "\u2032": "'",
        "\u201c": '"',
        "\u201d": '"',
        "\u201e": '"',
        "\u2033": '"',
        "\u2010": "-",
        "\u2011": "-",
        "\u2012": "-",
        "\u2013": "-",
        "\u2014": "-",
        "\u2212": "-",
        "\u00a0": " ",
    }
)


class OnlineArtworkError(Exception):
    pass


class OnlineArtworkRateLimitError(OnlineArtworkError):
    pass


class OnlineArtworkForbiddenError(OnlineArtworkError):
    pass


class OnlineArtworkNetworkError(OnlineArtworkError):
    pass


def _normalize(value):
    if not value:
        return ""
    value = unicodedata.normalize("NFKC", str(value)).translate(_PUNCT_TRANSLATION)
    value = value.strip().casefold()
    value = re.sub(r"\b(feat|ft)\.?\b", "featuring", value)
    value = re.sub(r"\bfeaturing\b.*$", "", value)
    value = re.sub(r"\s*&\s*", " and ", value)
    value = re.sub(r"[`´]", "'", value)
    value = re.sub(r"\s*[-_/]+\s*", " ", value)
    value = re.sub(r"\s+", " ", value)
    return value.strip()


def _strip_video_suffixes(value):
    text = str(value or "").strip()
    if not text:
        return ""
    text = re.sub(r"\s*[\[(]\d{4}[\])]\s*$", "", text).strip()
    text = re.sub(r"(?i)\b(?:complete|full)\s+series\b", "", text).strip(" -_:")
    text = re.sub(r"(?i)\bseason\s+\d+\b", "", text).strip(" -_:")
    text = re.sub(r"\s+", " ", text)
    return text.strip()


def _first_int(value):
    match = re.search(r"(?<!\d)(\d{1,2})(?!\d)", str(value or ""))
    return int(match.group(1)) if match else None


def _season_number_from_label(value):
    match = re.search(r"(?i)\bseason\s+(\d{1,2})\b", str(value or ""))
    if match:
        return int(match.group(1))
    return _first_int(value)


class ITunesArtworkLookup:
    """Thin client for album artwork discovery via Apple endpoints."""

    _rate_lock = threading.Lock()
    _last_request_at = 0.0

    def __init__(self, storefront="us", min_interval_seconds=0.5, timeout=10.0):
        self.storefront = storefront or "us"
        self.min_interval_seconds = float(min_interval_seconds or 0.5)
        self.timeout = float(timeout or 10.0)
        self.plex_base_url = str(os.getenv("ROCKPOD_PLEX_URL") or "http://127.0.0.1:32400").rstrip("/")
        self.plex_token = self._discover_plex_token()

    def search_album_art(self, album_title, artist_name, limit=8):
        best = self._search_with_term(f"{album_title} {artist_name}".strip(), album_title, artist_name, limit=limit)
        source = "itunes_search"
        if not best and album_title:
            best = self._search_with_term(str(album_title).strip(), album_title, artist_name, limit=limit)
            source = "itunes_search_album_only"
        if not best:
            return None
        artwork_url = best.get("artworkUrl100") or best.get("artworkUrl60") or ""
        if not artwork_url:
            return None
        hi_res_url = re.sub(r"/\d+x\d+bb\.", "/1200x1200bb.", artwork_url)
        return {
            "album": best.get("collectionName", ""),
            "artist": best.get("artistName", ""),
            "collection_id": best.get("collectionId"),
            "artwork_url": hi_res_url,
            "preview_url": artwork_url,
            "source": source,
        }

    def search_video_art(self, title, video_kind="movie", season_label="", limit=8, relaxed=False):
        plex_result = self._search_plex_video_art(
            title,
            video_kind=video_kind,
            season_label=season_label,
            file_paths=None,
            limit=limit,
        )
        if plex_result:
            return plex_result

        title = (title or "").strip()
        season_label = (season_label or "").strip()
        video_kind = (video_kind or "movie").strip()
        if not title:
            return None

        stripped_title = _strip_video_suffixes(title)
        title_variants = []
        for candidate in (title, stripped_title):
            candidate = (candidate or "").strip()
            if candidate and candidate not in title_variants:
                title_variants.append(candidate)

        candidates = []
        if video_kind == "show":
            for candidate_title in title_variants:
                if season_label:
                    candidates.append(
                        (
                            f"{candidate_title} {season_label}".strip(),
                            "tvShow",
                            "tvSeason",
                            "itunes_tv_season_search",
                        )
                    )
                candidates.append((candidate_title, "tvShow", "tvSeason", "itunes_tv_search"))
                candidates.append((candidate_title, "tvShow", "tvEpisode", "itunes_tv_episode_search"))
        else:
            for candidate_title in title_variants:
                candidates.append((candidate_title, "movie", "movie", "itunes_movie_search"))
                if season_label:
                    candidates.append(
                        (
                            f"{candidate_title} {season_label}".strip(),
                            "movie",
                            "movie",
                            "itunes_movie_search_fallback",
                        )
                    )

        for term, media, entity, source in candidates:
            best = self._search_video_term(
                term,
                title,
                season_label,
                media,
                entity,
                video_kind,
                limit=limit,
                relaxed=relaxed,
            )
            if not best:
                continue
            artwork_url = best.get("artworkUrl100") or best.get("artworkUrl60") or ""
            if not artwork_url:
                continue
            hi_res_url = re.sub(r"/\d+x\d+bb\.", "/1200x1200bb.", artwork_url)
            return {
                "title": best.get("trackName") or best.get("collectionName") or title,
                "artist": best.get("artistName", ""),
                "collection_id": best.get("collectionId") or best.get("trackId"),
                "artwork_url": hi_res_url,
                "preview_url": artwork_url,
                "source": source,
            }
        return None

    def search_video_art_relaxed(self, title, video_kind="movie", season_label="", limit=20):
        plex_result = self._search_plex_video_art(
            title,
            video_kind=video_kind,
            season_label=season_label,
            file_paths=None,
            limit=limit,
        )
        if plex_result:
            return plex_result
        return self.search_video_art(
            title,
            video_kind=video_kind,
            season_label=season_label,
            limit=limit,
            relaxed=True,
        )

    def search_video_art_from_info(self, video_info, limit=8, relaxed=False):
        if not hasattr(video_info, "get"):
            return None

        title = str(video_info.get("album") or "").strip()
        season_label = str(video_info.get("artist") or "").strip()
        video_kind = str(video_info.get("video_kind") or "movie").strip()
        file_paths = []
        for track in list(video_info.get("tracks") or []):
            if hasattr(track, "get"):
                path = str(track.get("file_path") or "").strip()
                if path:
                    file_paths.append(path)
        plex_result = self._search_plex_video_art(
            title,
            video_kind=video_kind,
            season_label=season_label,
            file_paths=file_paths,
            limit=limit,
        )
        if plex_result:
            return plex_result
        if relaxed:
            return self.search_video_art_relaxed(title, video_kind=video_kind, season_label=season_label, limit=20)
        return self.search_video_art(title, video_kind=video_kind, season_label=season_label, limit=limit)

    def _search_with_term(self, term, album_title, artist_name, limit=8):
        term = (term or "").strip()
        if not term:
            return None
        params = {
            "term": term,
            "media": "music",
            "entity": "album",
            "country": self.storefront,
            "limit": int(limit or 8),
        }
        data = self._get_json("https://itunes.apple.com/search", params)
        results = data.get("results") or []
        return self._pick_best_match(album_title, artist_name, results)

    def _search_video_term(self, term, title, season_label, media, entity, video_kind, limit=8, relaxed=False):
        term = (term or "").strip()
        if not term:
            return None
        params = {
            "term": term,
            "media": media,
            "entity": entity,
            "country": self.storefront,
            "limit": int(limit or 8),
        }
        data = self._get_json("https://itunes.apple.com/search", params)
        results = data.get("results") or []
        return self._pick_best_video_match(title, season_label, results, video_kind, relaxed=relaxed)

    def download_image(self, url):
        request = Request(url, headers={"User-Agent": "RockPod/1.0"})
        self._respect_rate_limit(self.min_interval_seconds)
        try:
            with urlopen(request, timeout=self.timeout) as response:
                return response.read(), response.headers.get_content_type() or "image/jpeg"
        except HTTPError as exc:
            raise self._map_http_error(exc) from exc
        except URLError as exc:
            raise OnlineArtworkNetworkError(str(exc)) from exc

    def _get_json(self, base_url, params):
        query = urlencode(params)
        request = Request(f"{base_url}?{query}", headers={"User-Agent": "RockPod/1.0"})
        self._respect_rate_limit(self.min_interval_seconds)
        try:
            with urlopen(request, timeout=self.timeout) as response:
                return json.loads(response.read().decode("utf-8"))
        except HTTPError as exc:
            raise self._map_http_error(exc) from exc
        except URLError as exc:
            raise OnlineArtworkNetworkError(str(exc)) from exc

    def _search_plex_video_art(self, title, video_kind="movie", season_label="", file_paths=None, limit=8):
        title = str(title or "").strip()
        if not title or not self.plex_token:
            return None
        title_variants = []
        for candidate in (title, _strip_video_suffixes(title)):
            candidate = str(candidate or "").strip()
            if candidate and candidate not in title_variants:
                title_variants.append(candidate)
        file_set = {
            os.path.normcase(os.path.realpath(path))
            for path in list(file_paths or [])
            if str(path or "").strip()
        }
        for query in title_variants:
            root = self._plex_search(query, limit=max(int(limit or 8), 8))
            if root is None:
                continue
            best = self._pick_best_plex_video_match(
                root,
                title=title,
                season_label=season_label,
                video_kind=video_kind,
                file_paths=file_set,
            )
            if best:
                return best
            if video_kind == "show" and file_set:
                for rating_key in self._plex_candidate_show_keys(root, title):
                    leaves_root = self._plex_fetch_metadata_xml(f"/library/metadata/{rating_key}/allLeaves")
                    if leaves_root is None:
                        continue
                    best = self._pick_best_plex_video_match(
                        leaves_root,
                        title=title,
                        season_label=season_label,
                        video_kind=video_kind,
                        file_paths=file_set,
                    )
                    if best:
                        return best
        return None

    def _plex_search(self, query, limit=8):
        params = {
            "query": str(query or "").strip(),
            "limit": int(limit or 8),
        }
        return self._plex_fetch_metadata_xml("/search", params=params)

    def _plex_fetch_metadata_xml(self, path, params=None):
        params = dict(params or {})
        params["X-Plex-Token"] = self.plex_token
        request = Request(
            f"{self.plex_base_url}{path}?{urlencode(params)}",
            headers={"User-Agent": "RockPod/1.0"},
        )
        try:
            with urlopen(request, timeout=min(self.timeout, 3.0)) as response:
                return ET.fromstring(response.read())
        except Exception:
            return None

    def _plex_candidate_show_keys(self, root, title):
        want_titles = [_normalize(title), _normalize(_strip_video_suffixes(title))]
        want_titles = [item for item in want_titles if item]
        keys = []
        seen = set()
        for item in root.iter("Directory"):
            if str(item.attrib.get("type") or "").strip().casefold() != "show":
                continue
            item_title = _normalize(item.attrib.get("title", ""))
            if not item_title:
                continue
            score = 0
            for want in want_titles:
                if item_title == want:
                    score = max(score, 3)
                elif want in item_title or item_title in want:
                    score = max(score, 2)
            if score <= 0:
                continue
            rating_key = str(item.attrib.get("ratingKey") or "").strip()
            if not rating_key or rating_key in seen:
                continue
            seen.add(rating_key)
            keys.append((score, rating_key))
        keys.sort(key=lambda item: (-item[0], item[1]))
        return [rating_key for _score, rating_key in keys[:4]]

    def _pick_best_plex_video_match(self, root, title, season_label, video_kind, file_paths):
        want_titles = [_normalize(title), _normalize(_strip_video_suffixes(title))]
        want_titles = [item for item in want_titles if item]
        want_season = _season_number_from_label(season_label)
        best = None
        best_score = 0
        for item in root.iter():
            if item is root:
                continue
            item_type = str(item.attrib.get("type") or "").strip().casefold()
            if video_kind == "movie":
                if item_type != "movie":
                    continue
            elif item_type not in {"show", "season", "episode"}:
                continue

            part_files = {
                os.path.normcase(os.path.realpath(part.attrib.get("file", "")))
                for part in item.findall(".//Part")
                if part.attrib.get("file")
            }
            title_candidates = [
                _normalize(item.attrib.get("title", "")),
                _normalize(item.attrib.get("grandparentTitle", "")),
                _normalize(item.attrib.get("parentTitle", "")),
            ]
            title_candidates = [candidate for candidate in title_candidates if candidate]
            score = 0
            if file_paths and part_files and (file_paths & part_files):
                score += 300
            for want in want_titles:
                if any(candidate == want for candidate in title_candidates):
                    score += 160
                    break
                if any(want in candidate or candidate in want for candidate in title_candidates):
                    score += 90
                    break
            if video_kind == "show":
                if item_type == "show":
                    score += 40
                elif item_type == "season":
                    score += 25
                elif item_type == "episode":
                    score += 15
                if want_season is not None:
                    parent_index = _first_int(item.attrib.get("parentIndex", ""))
                    index = _first_int(item.attrib.get("index", ""))
                    if parent_index == want_season or (item_type == "season" and index == want_season):
                        score += 20
            thumb = self._plex_thumb_for_item(item, video_kind)
            if not thumb:
                continue
            if score > best_score:
                best_score = score
                best = {
                    "title": item.attrib.get("title")
                    or item.attrib.get("grandparentTitle")
                    or title,
                    "artist": item.attrib.get("parentTitle")
                    or item.attrib.get("grandparentTitle")
                    or "",
                    "collection_id": item.attrib.get("ratingKey"),
                    "artwork_url": self._plex_image_url(thumb),
                    "preview_url": self._plex_image_url(thumb),
                    "source": "plex_search",
                }
        if best_score < 120:
            return None
        return best

    @staticmethod
    def _plex_thumb_for_item(item, video_kind):
        attrib = item.attrib
        if video_kind == "movie":
            return attrib.get("thumb") or attrib.get("art") or ""
        return (
            attrib.get("grandparentThumb")
            or attrib.get("parentThumb")
            or attrib.get("thumb")
            or attrib.get("grandparentArt")
            or attrib.get("parentArt")
            or attrib.get("art")
            or ""
        )

    def _plex_image_url(self, thumb_path):
        path = str(thumb_path or "").strip()
        if not path:
            return ""
        separator = "&" if "?" in path else "?"
        return f"{self.plex_base_url}{path}{separator}X-Plex-Token={self.plex_token}"

    @staticmethod
    def _discover_plex_token():
        env_token = str(os.getenv("ROCKPOD_PLEX_TOKEN") or "").strip()
        if env_token:
            return env_token
        candidates = [
            os.path.expanduser("~/.local/share/plex/Plex Media Server/.LocalAdminToken"),
            os.path.expanduser("~/.config/plex/.LocalAdminToken"),
        ]
        for path in candidates:
            try:
                with open(path, "r", encoding="utf-8") as handle:
                    token = handle.read().strip()
                if token:
                    return token
            except OSError:
                continue
        prefs_path = os.path.expanduser("~/.local/share/plex/Plex Media Server/Preferences.xml")
        try:
            with open(prefs_path, "r", encoding="utf-8") as handle:
                data = handle.read()
        except OSError:
            return ""
        match = re.search(r'PlexOnlineToken="([^"]+)"', data)
        return match.group(1).strip() if match else ""

    @classmethod
    def _respect_rate_limit(cls, min_interval_seconds=0.5):
        with cls._rate_lock:
            now = time.monotonic()
            wait = max(0.0, float(min_interval_seconds) - (now - cls._last_request_at))
            if wait > 0:
                time.sleep(wait)
            cls._last_request_at = time.monotonic()

    def _pick_best_match(self, album_title, artist_name, results):
        want_album = _normalize(album_title)
        want_artist = _normalize(artist_name)
        best = None
        best_score = 0
        for item in results:
            album = _normalize(item.get("collectionName", ""))
            artist = _normalize(item.get("artistName", ""))
            if not album:
                continue
            score = 0
            if album == want_album:
                score += 70
            elif want_album and want_album in album:
                score += 40
            if artist == want_artist:
                score += 25
            elif want_artist and (want_artist in artist or artist in want_artist):
                score += 15
            if item.get("collectionType") == "Album":
                score += 5
            if score > best_score:
                best = item
                best_score = score
        if best_score < 70:
            return None
        return best

    def _pick_best_video_match(self, title, season_label, results, video_kind, relaxed=False):
        title_variants = [_normalize(title), _normalize(_strip_video_suffixes(title))]
        title_variants = [variant for variant in title_variants if variant]
        want_season = _normalize(season_label)
        best = None
        best_score = 0
        for item in results:
            collection = _normalize(item.get("collectionName", "") or item.get("collectionCensoredName", ""))
            track = _normalize(item.get("trackName", "") or item.get("trackCensoredName", ""))
            artist = _normalize(item.get("artistName", ""))
            kind = _normalize(item.get("kind", ""))
            wrapper = _normalize(item.get("wrapperType", ""))

            score = 0
            title_match = ""
            for want_title in title_variants:
                for candidate in (artist, collection, track):
                    if candidate == want_title:
                        title_match = candidate
                        score += 70
                        break
                    if want_title and (want_title in candidate or candidate in want_title):
                        title_match = candidate
                        score += 40
                        break
                if title_match:
                    break
            if not title_match:
                if relaxed and title_variants:
                    title_words = set(title_variants[0].split())
                    candidate_words = set((artist or collection or track).split())
                    overlap = len(title_words & candidate_words)
                    if overlap >= max(1, min(2, len(title_words))):
                        score += 25 + (5 * overlap)
                    else:
                        continue
                else:
                    continue

            if want_season:
                season_sources = [collection, track]
                if any(candidate == want_season for candidate in season_sources):
                    score += 20
                elif any(want_season in candidate for candidate in season_sources):
                    score += 10

            if video_kind == "show":
                if "season" in kind:
                    score += 10
                if "tv" in kind or "tv" in wrapper:
                    score += 5
            else:
                if "feature movie" in kind or "movie" in kind:
                    score += 10
                if "track" in wrapper:
                    score += 5

            if score > best_score:
                best = item
                best_score = score

        threshold = 25 if relaxed else 45
        if best_score < threshold:
            return None
        return best

    @staticmethod
    def _map_http_error(exc):
        if exc.code == 429:
            return OnlineArtworkRateLimitError("HTTP 429 Too Many Requests")
        if exc.code == 403:
            return OnlineArtworkForbiddenError("HTTP 403 Forbidden")
        return OnlineArtworkNetworkError(f"HTTP {exc.code}")
