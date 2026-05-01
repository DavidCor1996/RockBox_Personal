"""Online album metadata lookup using Apple's Search API plus lyrics fallback."""

import json
import re
import threading
import time
import unicodedata
from urllib.error import HTTPError, URLError
from urllib.parse import quote, urlencode
from urllib.request import Request, urlopen

from PySide6.QtCore import QObject, QThread, Signal, Slot


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
        "\u00A0": " ",
    }
)


class OnlineAlbumMetadataError(Exception):
    """Raised when album metadata could not be fetched."""


class OnlineAlbumMetadataNetworkError(OnlineAlbumMetadataError):
    """Raised when network operations fail."""


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


def _clean_lookup_value(value, preserve_punctuation=False):
    if not value:
        return ""
    value = unicodedata.normalize("NFKC", str(value)).translate(_PUNCT_TRANSLATION)
    value = value.replace("_", " ")
    value = re.sub(r"\b(feat|ft|featuring)\.?\b.*$", "", value, flags=re.IGNORECASE)
    value = re.sub(r"[\"`]+", " ", value)
    if preserve_punctuation:
        value = re.sub(r"[^\w\s&'(),?!-]", " ", value)
    else:
        value = re.sub(r"[^\w\s&-]", " ", value)
    value = re.sub(r"\s*&\s*", " and ", value)
    value = re.sub(r"\s+", " ", value)
    return value.strip()


_TIMED_LYRICS_RE = re.compile(
    r"(?:\[\d{1,2}:\d{2}(?:[.:]\d{1,3})?\])|(?:<\d{1,2}:\d{2}(?:[.:]\d{1,3})?>)"
)
_WORD_TIMED_LYRICS_RE = re.compile(r"<\d{1,2}:\d{2}(?:[.:]\d{1,3})?>")


def lyrics_text_has_timestamps(text):
    return bool(_TIMED_LYRICS_RE.search(str(text or "")))


def lyrics_text_has_word_timestamps(text):
    return bool(_WORD_TIMED_LYRICS_RE.search(str(text or "")))


class ITunesAlbumMetadataLookup:
    """Thin client for online album metadata and lyrics discovery."""

    _rate_lock = threading.Lock()
    _last_request_at = 0.0

    def __init__(self, storefront="us", min_interval_seconds=0.5, timeout=10.0, lyrics_timeout=6.0):
        self.storefront = storefront or "us"
        self.min_interval_seconds = float(min_interval_seconds or 0.5)
        self.timeout = float(timeout or 10.0)
        self.lyrics_timeout = float(lyrics_timeout or 6.0)

    def fetch_album_metadata(self, album_title, artist_name, include_lyrics=True):
        best = self._search_album(album_title, artist_name)
        if not best:
            return None
        collection_id = best.get("collectionId")
        if not collection_id:
            return self._metadata_from_collection(best, album_title, artist_name, tracks=[], source="itunes_search_partial")

        try:
            details = self._lookup_album(collection_id)
        except OnlineAlbumMetadataNetworkError:
            return self._metadata_from_collection(best, album_title, artist_name, tracks=[], source="itunes_search_partial")

        results = list(details.get("results") or [])
        if not results:
            return self._metadata_from_collection(best, album_title, artist_name, tracks=[], source="itunes_search_partial")

        collection = dict(results[0])
        track_rows = [item for item in results[1:] if item.get("wrapperType") == "track"]
        tracks = []
        for item in track_rows:
            track = {
                "title": item.get("trackName", ""),
                "artist": item.get("artistName", "") or collection.get("artistName", ""),
                "composer": item.get("composerName", ""),
                "track_number": int(item.get("trackNumber") or 0),
                "disc_number": int(item.get("discNumber") or 0),
                "duration_millis": int(item.get("trackTimeMillis") or 0),
                "preview_url": item.get("previewUrl", ""),
                "track_view_url": item.get("trackViewUrl", ""),
                "lyrics": "",
            }
            if include_lyrics:
                try:
                    track["lyrics"] = self.fetch_track_lyrics(track["artist"], track["title"])
                except OnlineAlbumMetadataNetworkError:
                    track["lyrics"] = ""
            tracks.append(track)

        return self._metadata_from_collection(collection, album_title, artist_name, tracks=tracks, source="itunes_search")

    def _metadata_from_collection(self, collection, album_title, artist_name, tracks=None, source="itunes_search"):
        artwork_url = collection.get("artworkUrl100") or collection.get("artworkUrl60") or ""
        hi_res_artwork_url = re.sub(r"/\d+x\d+bb\.", "/1200x1200bb.", artwork_url) if artwork_url else ""
        release_date = str(collection.get("releaseDate") or "")
        tracks = list(tracks or [])
        return {
            "album": collection.get("collectionName", "") or album_title or "",
            "artist": collection.get("artistName", "") or artist_name or "",
            "release_date": release_date,
            "year": int(release_date[:4]) if len(release_date) >= 4 and release_date[:4].isdigit() else None,
            "primary_genre": collection.get("primaryGenreName", ""),
            "copyright": collection.get("copyright", ""),
            "track_count": int(collection.get("trackCount") or len(tracks)),
            "collection_id": collection.get("collectionId"),
            "collection_view_url": collection.get("collectionViewUrl", ""),
            "artist_view_url": collection.get("artistViewUrl", ""),
            "artwork_url": hi_res_artwork_url or artwork_url,
            "source": source,
            "tracks": sorted(
                tracks,
                key=lambda row: (
                    int(row.get("disc_number") or 0),
                    int(row.get("track_number") or 0),
                    str(row.get("title") or "").casefold(),
                ),
            ),
        }

    def fetch_track_lyrics(self, artist_name, title):
        return self.fetch_track_lyrics_detail(artist_name, title)["lyrics"]

    def fetch_track_lyrics_detail(self, artist_name, title):
        artist_name = str(artist_name or "").strip()
        title = str(title or "").strip()
        if not artist_name or not title:
            return {"lyrics": "", "timed": False, "per_word": False, "source": ""}
        last_error = None
        for fetcher in (self._fetch_lrclib_lyrics, self._fetch_lyrics_ovh):
            try:
                lyrics, source = fetcher(artist_name, title)
            except OnlineAlbumMetadataNetworkError as exc:
                last_error = exc
                continue
            if lyrics:
                return {
                    "lyrics": lyrics,
                    "timed": lyrics_text_has_timestamps(lyrics),
                    "per_word": lyrics_text_has_word_timestamps(lyrics),
                    "source": source,
                }
        if last_error is not None:
            raise last_error
        return {"lyrics": "", "timed": False, "per_word": False, "source": ""}

    def _fetch_lrclib_lyrics(self, artist_name, title):
        data = self._get_json(
            "https://lrclib.net/api/search",
            params={
                "artist_name": artist_name,
                "track_name": title,
            },
            timeout=self.lyrics_timeout,
            return_empty_on_404=True,
        )
        if not isinstance(data, list):
            return ""

        want_artist = _normalize(artist_name)
        want_title = _normalize(title)
        best_synced = ""
        best_plain = ""
        best_score = -1

        for item in data:
            if not isinstance(item, dict):
                continue
            item_title = _normalize(item.get("trackName", ""))
            item_artist = _normalize(item.get("artistName", ""))
            if not item_title:
                continue
            score = 0
            if item_title == want_title:
                score += 70
            elif want_title and want_title in item_title:
                score += 35
            if item_artist == want_artist:
                score += 30
            elif want_artist and (want_artist in item_artist or item_artist in want_artist):
                score += 15
            synced = str(item.get("syncedLyrics") or "").strip()
            plain = str(item.get("plainLyrics") or "").strip()
            if synced:
                score += 5
            if score <= best_score:
                continue
            best_score = score
            best_synced = synced
            best_plain = plain

        return (best_synced or best_plain), "lrclib"

    def _fetch_lyrics_ovh(self, artist_name, title):
        url = f"https://api.lyrics.ovh/v1/{quote(artist_name, safe='')}/{quote(title, safe='')}"
        data = self._get_json(url, timeout=self.lyrics_timeout, return_empty_on_404=True)
        return str((data or {}).get("lyrics") or "").strip(), "lyrics.ovh"

    def _search_album(self, album_title, artist_name, limit=8):
        for candidate_album, candidate_artist in self._lookup_variants(album_title, artist_name):
            best = self._search_with_term(
                f"{candidate_album} {candidate_artist}".strip(),
                candidate_album,
                candidate_artist,
                limit=limit,
            )
            if best:
                return best
        return None

    def _lookup_variants(self, album_title, artist_name):
        raw_album = _clean_lookup_value(album_title, preserve_punctuation=True)
        raw_artist = _clean_lookup_value(artist_name, preserve_punctuation=False)
        if raw_artist.casefold() == "unknown artist":
            raw_artist = ""
        variants = []
        seen = set()
        candidates = [
            (raw_album, raw_artist),
            (raw_album, ""),
        ]
        candidates.extend(self._combined_lookup_candidates(raw_album, raw_artist))
        for album, artist in candidates:
            album = str(album or "").strip()
            artist = str(artist or "").strip()
            if not album:
                continue
            key = (album.casefold(), artist.casefold())
            if key in seen:
                continue
            seen.add(key)
            variants.append((album, artist))
        return variants

    def _combined_lookup_candidates(self, album, artist):
        if str(artist or "").strip():
            return []
        text = str(album or "").strip()
        if not text:
            return []
        variants = []
        for pattern in (r"\s+[-–—]\s+", r"\s*:\s*", r"\s+\|\s+"):
            parts = re.split(pattern, text, maxsplit=1)
            if len(parts) != 2:
                continue
            candidate_artist = _clean_lookup_value(parts[0], preserve_punctuation=True)
            candidate_album = _clean_lookup_value(parts[1], preserve_punctuation=True)
            if not candidate_album or not candidate_artist:
                continue
            variants.append((candidate_album, candidate_artist))
        return variants

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
        data = self._get_json("https://itunes.apple.com/search", params=params)
        return self._pick_best_match(album_title, artist_name, data.get("results") or [])

    def _lookup_album(self, collection_id):
        params = {
            "id": int(collection_id),
            "entity": "song",
            "country": self.storefront,
        }
        return self._get_json("https://itunes.apple.com/lookup", params=params)

    def _get_json(self, base_url, params=None, timeout=None, return_empty_on_404=False):
        timeout = self.timeout if timeout is None else float(timeout)
        url = base_url
        if params:
            url = f"{base_url}?{urlencode(params)}"
        request = Request(url, headers={"User-Agent": "RockPod/1.0"})
        self._respect_rate_limit(self.min_interval_seconds)
        try:
            with urlopen(request, timeout=timeout) as response:
                return json.loads(response.read().decode("utf-8"))
        except HTTPError as exc:
            if return_empty_on_404 and exc.code == 404:
                return {}
            raise OnlineAlbumMetadataNetworkError(f"HTTP {exc.code}") from exc
        except URLError as exc:
            raise OnlineAlbumMetadataNetworkError(str(exc)) from exc
        except json.JSONDecodeError as exc:
            raise OnlineAlbumMetadataNetworkError("Invalid JSON response") from exc

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


class AlbumMetadataWorker(QObject):
    finished = Signal(str, dict)
    error = Signal(str, str)

    def __init__(self, album_key, album_title, artist_name, lookup_client):
        super().__init__()
        self._album_key = album_key
        self._album_title = album_title
        self._artist_name = artist_name
        self._lookup_client = lookup_client

    @Slot()
    def run(self):
        try:
            result = self._lookup_client.fetch_album_metadata(
                self._album_title,
                self._artist_name,
                include_lyrics=True,
            )
            if not result:
                raise OnlineAlbumMetadataError("No matching online album metadata found")
            self.finished.emit(self._album_key, result)
        except Exception as exc:  # pragma: no cover - defensive
            self.error.emit(self._album_key, str(exc))


class AlbumMetadataFetcher(QObject):
    """Background wrapper so album metadata lookup never blocks the UI thread."""

    finished = Signal(str, dict)
    error = Signal(str, str)

    def __init__(self, config=None, parent=None, lookup_client=None):
        super().__init__(parent)
        self._lookup_client = lookup_client or ITunesAlbumMetadataLookup(
            storefront=(config.get("online_artwork_storefront", "us") if config else "us"),
            min_interval_seconds=(config.get("online_artwork_min_interval_seconds", 0.5) if config else 0.5),
        )
        self._thread = None
        self._worker = None

    @property
    def is_running(self):
        return self._thread is not None and self._thread.isRunning()

    def start(self, album_key, album_title, artist_name):
        if self.is_running or not album_key:
            return False
        self._thread = QThread()
        self._worker = AlbumMetadataWorker(album_key, album_title, artist_name, self._lookup_client)
        self._worker.moveToThread(self._thread)
        self._thread.started.connect(self._worker.run)
        self._worker.finished.connect(self._on_finished)
        self._worker.error.connect(self._on_error)
        self._worker.finished.connect(self._thread.quit)
        self._worker.error.connect(self._thread.quit)
        self._thread.finished.connect(self._cleanup)
        self._thread.start()
        return True

    def shutdown(self):
        if self._thread is not None and self._thread.isRunning():
            self._thread.quit()
            self._thread.wait(3000)
        self._cleanup()

    def _on_finished(self, album_key, result):
        self.finished.emit(album_key, result)

    def _on_error(self, album_key, message):
        self.error.emit(album_key, message)

    def _cleanup(self):
        if self._worker is not None:
            try:
                self._worker.deleteLater()
            except RuntimeError:
                pass
        if self._thread is not None:
            try:
                self._thread.deleteLater()
            except RuntimeError:
                pass
        self._worker = None
        self._thread = None
