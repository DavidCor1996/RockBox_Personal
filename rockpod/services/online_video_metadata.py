"""Replaceable video metadata providers (iTunes Search API & OMDb API fallback)."""

import html
import json
import re
import threading
import time
import urllib.parse
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

import logging
logger = logging.getLogger(__name__)


def _large_itunes_artwork_url(value):
    return re.sub(r"/\d+x\d+bb", "/600x600bb", str(value or ""))

class VideoMetadataError(Exception):
    """Raised when metadata lookup fails."""
    pass

class VideoMetadataNetworkError(VideoMetadataError):
    """Raised when a network failure occurs during lookup."""
    pass


class VideoMetadataProvider:
    """Base class for replaceable video metadata providers."""
    
    def search_movie(self, query, year=None):
        raise NotImplementedError()

    def search_show(self, query, year=None):
        raise NotImplementedError()

    def search_episode(self, show_title, season, episode, episode_title=None):
        raise NotImplementedError()

    def get_title_by_id(self, provider_id):
        raise NotImplementedError()


class ITunesVideoMetadataProvider(VideoMetadataProvider):
    """Free, zero-setup provider using Apple's iTunes Search API."""
    
    _rate_lock = threading.Lock()
    _last_request_at = 0.0

    def __init__(self, storefront="us", timeout=10.0, min_interval_seconds=0.5):
        self.storefront = storefront or "us"
        self.timeout = float(timeout or 10.0)
        self.min_interval_seconds = float(min_interval_seconds or 0.5)

    def _rate_limit_wait(self):
        with self._rate_lock:
            now = time.time()
            elapsed = now - self._last_request_at
            if elapsed < self.min_interval_seconds:
                time.sleep(self.min_interval_seconds - elapsed)
            self._last_request_at = time.time()

    def _fetch_url(self, url):
        self._rate_limit_wait()
        req = Request(
            url,
            headers={"User-Agent": "RockPod/1.0 (Digital Audio Player Manager)"}
        )
        try:
            with urlopen(req, timeout=self.timeout) as resp:
                return json.loads(resp.read().decode("utf-8"))
        except HTTPError as e:
            logger.error(f"iTunes HTTP error {e.code}: {e.reason}")
            raise VideoMetadataNetworkError(f"HTTP error {e.code}")
        except URLError as e:
            logger.error(f"iTunes URL error: {e.reason}")
            raise VideoMetadataNetworkError("Network connection failed")
        except Exception as e:
            logger.error(f"iTunes unexpected error: {str(e)}")
            raise VideoMetadataError("Failed to parse metadata response")

    def search_movie(self, query, year=None):
        term = query
        if year:
            term = f"{query} {year}"
        params = {
            "term": term,
            "country": self.storefront,
            "media": "movie",
            "entity": "movie",
            "limit": 10
        }
        url = f"https://itunes.apple.com/search?{urllib.parse.urlencode(params)}"
        data = self._fetch_url(url)
        results = []
        for item in data.get("results", []):
            rel_date = item.get("releaseDate", "")
            item_year = int(rel_date[:4]) if len(rel_date) >= 4 and rel_date[:4].isdigit() else None
            results.append({
                "provider": "itunes",
                "provider_id": str(item.get("trackId") or ""),
                "imdb_id": "",
                "title": item.get("trackName") or item.get("collectionName") or "",
                "media_type": "movie",
                "year": item_year,
                "release_date": rel_date[:10] if len(rel_date) >= 10 else "",
                "genre": item.get("primaryGenreName") or "",
                "plot_short": item.get("longDescription") or item.get("shortDescription") or "",
                "plot_long": item.get("longDescription") or "",
                "artwork_url": _large_itunes_artwork_url(item.get("artworkUrl100")),
                "runtime_seconds": int((item.get("trackTimeMillis") or 0) / 1000),
                "content_rating": item.get("contentAdvisoryRating") or ""
            })
        return results

    def search_show(self, query, year=None):
        term = query
        if year:
            term = f"{query} {year}"
        params = {
            "term": term,
            "country": self.storefront,
            "media": "tvShow",
            "entity": "tvSeason",
            "limit": 10
        }
        url = f"https://itunes.apple.com/search?{urllib.parse.urlencode(params)}"
        data = self._fetch_url(url)
        results = []
        seen_shows = set()
        for item in data.get("results", []):
            show_name = item.get("artistName") or item.get("collectionName") or ""
            if not show_name:
                continue
            show_key = show_name.lower().strip()
            if show_key in seen_shows:
                continue
            seen_shows.add(show_key)
            rel_date = item.get("releaseDate", "")
            item_year = int(rel_date[:4]) if len(rel_date) >= 4 and rel_date[:4].isdigit() else None
            results.append({
                "provider": "itunes",
                "provider_id": str(item.get("artistId") or item.get("collectionId") or ""),
                "imdb_id": "",
                "title": show_name,
                "media_type": "tv_show",
                "year": item_year,
                "release_date": rel_date[:10] if len(rel_date) >= 10 else "",
                "genre": item.get("primaryGenreName") or "",
                "plot_short": item.get("longDescription") or item.get("shortDescription") or "",
                "plot_long": item.get("longDescription") or "",
                "artwork_url": _large_itunes_artwork_url(item.get("artworkUrl100")),
                "runtime_seconds": 0,
                "content_rating": item.get("contentAdvisoryRating") or ""
            })
        return results

    def search_episode(self, show_title, season, episode, episode_title=None):
        term = f"{show_title} Season {season}"
        params = {
            "term": term,
            "country": self.storefront,
            "media": "tvShow",
            "entity": "tvEpisode",
            "limit": 50
        }
        url = f"https://itunes.apple.com/search?{urllib.parse.urlencode(params)}"
        data = self._fetch_url(url)
        episodes = []
        for item in data.get("results", []):
            item_show = item.get("artistName") or ""
            if show_title.lower() not in item_show.lower() and item_show.lower() not in show_title.lower():
                continue
            track_num = item.get("trackNumber")
            # If trackNumber doesn't match, check episode number fields
            if track_num is None:
                continue
            if int(track_num) != int(episode):
                continue
            
            rel_date = item.get("releaseDate", "")
            item_year = int(rel_date[:4]) if len(rel_date) >= 4 and rel_date[:4].isdigit() else None
            return {
                "provider": "itunes",
                "provider_id": str(item.get("trackId") or ""),
                "imdb_id": "",
                "title": item.get("trackName") or "",
                "media_type": "tv_episode",
                "year": item_year,
                "release_date": rel_date[:10] if len(rel_date) >= 10 else "",
                "genre": item.get("primaryGenreName") or "",
                "plot_short": item.get("longDescription") or item.get("shortDescription") or "",
                "plot_long": item.get("longDescription") or "",
                "artwork_url": _large_itunes_artwork_url(item.get("artworkUrl100")),
                "runtime_seconds": int((item.get("trackTimeMillis") or 0) / 1000),
                "content_rating": item.get("contentAdvisoryRating") or ""
            }
        return None


class OMDbVideoMetadataProvider(VideoMetadataProvider):
    """Rich metadata lookup using the OMDb (Open Movie Database) API (requires API key)."""
    
    def __init__(self, api_key="", timeout=10.0):
        self.api_key = api_key
        self.timeout = float(timeout or 10.0)

    def _fetch_omdb(self, params):
        if not self.api_key:
            raise VideoMetadataError("OMDb API key not configured")
        params["apikey"] = self.api_key
        url = f"http://www.omdbapi.com/?{urllib.parse.urlencode(params)}"
        req = Request(url, headers={"User-Agent": "RockPod/1.0"})
        try:
            with urlopen(req, timeout=self.timeout) as resp:
                data = json.loads(resp.read().decode("utf-8"))
                if data.get("Response") == "False":
                    logger.warning(f"OMDb returned error: {data.get('Error')}")
                    return {}
                return data
        except Exception as e:
            raise VideoMetadataNetworkError(f"OMDb lookup failed: {str(e)}")

    def search_movie(self, query, year=None):
        params = {"s": query, "type": "movie"}
        if year:
            params["y"] = str(year)
        data = self._fetch_omdb(params)
        results = []
        for item in data.get("Search", []):
            imdb_id = item.get("imdbID") or ""
            # Get full details if we want high confidence
            detail = self._fetch_omdb({"i": imdb_id}) if imdb_id else {}
            year_val = item.get("Year", "")
            item_year = int(year_val[:4]) if len(year_val) >= 4 and year_val[:4].isdigit() else None
            results.append({
                "provider": "omdb",
                "provider_id": imdb_id,
                "imdb_id": imdb_id,
                "title": item.get("Title") or "",
                "media_type": "movie",
                "year": item_year,
                "release_date": detail.get("Released") or "",
                "genre": detail.get("Genre") or "",
                "plot_short": detail.get("Plot") or "",
                "plot_long": detail.get("Plot") or "",
                "artwork_url": item.get("Poster") or "",
                "runtime_seconds": self._parse_runtime(detail.get("Runtime")),
                "content_rating": detail.get("Rated") or ""
            })
        return results

    def search_show(self, query, year=None):
        params = {"s": query, "type": "series"}
        if year:
            params["y"] = str(year)
        data = self._fetch_omdb(params)
        results = []
        for item in data.get("Search", []):
            imdb_id = item.get("imdbID") or ""
            detail = self._fetch_omdb({"i": imdb_id}) if imdb_id else {}
            year_val = item.get("Year", "")
            item_year = int(year_val[:4]) if len(year_val) >= 4 and year_val[:4].isdigit() else None
            results.append({
                "provider": "omdb",
                "provider_id": imdb_id,
                "imdb_id": imdb_id,
                "title": item.get("Title") or "",
                "media_type": "tv_show",
                "year": item_year,
                "release_date": detail.get("Released") or "",
                "genre": detail.get("Genre") or "",
                "plot_short": detail.get("Plot") or "",
                "plot_long": detail.get("Plot") or "",
                "artwork_url": item.get("Poster") or "",
                "runtime_seconds": 0,
                "content_rating": detail.get("Rated") or ""
            })
        return results

    def search_episode(self, show_title, season, episode, episode_title=None):
        params = {"t": show_title, "Season": str(season), "Episode": str(episode)}
        detail = self._fetch_omdb(params)
        if not detail:
            return None
        imdb_id = detail.get("imdbID") or ""
        year_val = detail.get("Year", "")
        item_year = int(year_val[:4]) if len(year_val) >= 4 and year_val[:4].isdigit() else None
        return {
            "provider": "omdb",
            "provider_id": imdb_id,
            "imdb_id": imdb_id,
            "title": detail.get("Title") or "",
            "media_type": "tv_episode",
            "year": item_year,
            "release_date": detail.get("Released") or "",
            "genre": detail.get("Genre") or "",
            "plot_short": detail.get("Plot") or "",
            "plot_long": detail.get("Plot") or "",
            "artwork_url": detail.get("Poster") or "",
            "runtime_seconds": self._parse_runtime(detail.get("Runtime")),
            "content_rating": detail.get("Rated") or ""
        }

    @staticmethod
    def _parse_runtime(text):
        if not text:
            return 0
        match = re.search(r"(\d+)\s*min", text, re.IGNORECASE)
        return int(match.group(1)) * 60 if match else 0


class TVmazeVideoMetadataProvider(VideoMetadataProvider):
    """No-key TV show and episode fallback using the TVmaze API."""

    def __init__(self, timeout=10.0):
        self.timeout = float(timeout or 10.0)

    def _fetch_json(self, path, params=None):
        query = urllib.parse.urlencode(params or {})
        url = f"https://api.tvmaze.com{path}"
        if query:
            url = f"{url}?{query}"
        request = Request(url, headers={"User-Agent": "RockPod/1.0"})
        try:
            with urlopen(request, timeout=self.timeout) as response:
                return json.loads(response.read().decode("utf-8"))
        except HTTPError as exc:
            if exc.code == 404:
                return {}
            raise VideoMetadataNetworkError(f"TVmaze HTTP error {exc.code}") from exc
        except URLError as exc:
            raise VideoMetadataNetworkError("TVmaze connection failed") from exc
        except (OSError, ValueError) as exc:
            raise VideoMetadataError("TVmaze response could not be read") from exc

    @staticmethod
    def _plain_text(value):
        text = html.unescape(str(value or ""))
        return re.sub(r"\s+", " ", re.sub(r"<[^>]+>", " ", text)).strip()

    @staticmethod
    def _year(value):
        text = str(value or "")
        return int(text[:4]) if len(text) >= 4 and text[:4].isdigit() else None

    def _show_result(self, item):
        image = dict(item.get("image") or {})
        externals = dict(item.get("externals") or {})
        summary = self._plain_text(item.get("summary"))
        return {
            "provider": "tvmaze",
            "provider_id": str(item.get("id") or ""),
            "imdb_id": str(externals.get("imdb") or ""),
            "tmdb_id": "",
            "title": str(item.get("name") or ""),
            "media_type": "tv_show",
            "year": self._year(item.get("premiered")),
            "release_date": str(item.get("premiered") or ""),
            "genre": ", ".join(item.get("genres") or []),
            "plot_short": summary,
            "plot_long": summary,
            "artwork_url": str(image.get("original") or image.get("medium") or ""),
            "runtime_seconds": int(item.get("averageRuntime") or item.get("runtime") or 0) * 60,
            "content_rating": "",
        }

    def search_movie(self, query, year=None):
        return []

    def search_show(self, query, year=None):
        item = self._fetch_json("/singlesearch/shows", {"q": query})
        if not item:
            return []
        result = self._show_result(item)
        if year and result.get("year") and int(year) != int(result["year"]):
            return []
        return [result]

    def search_episode(self, show_title, season, episode, episode_title=None):
        shows = self.search_show(show_title)
        if not shows:
            return None
        show = shows[0]
        episodes = self._fetch_json(f"/shows/{show['provider_id']}/episodes")
        for item in episodes or []:
            if int(item.get("season") or -1) != int(season):
                continue
            if int(item.get("number") or -1) != int(episode):
                continue
            image = dict(item.get("image") or {})
            summary = self._plain_text(item.get("summary")) or show.get("plot_short", "")
            return {
                **show,
                "provider_id": str(item.get("id") or show.get("provider_id") or ""),
                "title": str(item.get("name") or episode_title or ""),
                "media_type": "tv_episode",
                "year": self._year(item.get("airdate")) or show.get("year"),
                "release_date": str(item.get("airdate") or ""),
                "plot_short": summary,
                "plot_long": summary,
                "artwork_url": str(image.get("original") or image.get("medium") or show.get("artwork_url") or ""),
                "runtime_seconds": int(item.get("runtime") or 0) * 60,
            }
        return None

    def get_title_by_id(self, provider_id):
        item = self._fetch_json(f"/shows/{provider_id}")
        return self._show_result(item) if item else None


class VideoMetadataService:
    """Manages replaceable video metadata providers and acts as local cache lookup layer."""
    
    def __init__(self, config=None, db=None):
        self._config = config
        self._db = db
        self._provider = ITunesVideoMetadataProvider()
        self._show_fallback = TVmazeVideoMetadataProvider()
        self._configure_provider()

    def _configure_provider(self):
        if self._config:
            omdb_key = self._config.get("omdb_api_key", "").strip()
            if omdb_key:
                self._provider = OMDbVideoMetadataProvider(api_key=omdb_key)
                return
        self._provider = ITunesVideoMetadataProvider()

    def search(self, query, media_type="any", year=None):
        """Perform provider lookup for Movie, TV Show, or both.

        A library row's video_kind is a guess made from its filename, so
        restricting the search to that guess makes a misclassified movie
        unmatchable - the user only ever sees TV results. "any" searches both
        catalogues and leads with the kind the row claims to be.
        """
        media_type = str(media_type or "any").strip().casefold()
        wants_show = media_type in {"tv_show", "tv_episode", "show"}
        wants_movie = media_type == "movie"

        if wants_show:
            return self._search_shows(query, year)
        if wants_movie:
            return self._search_movies(query, year)

        errors = []
        shows = self._collect(self._search_shows, query, year, errors)
        movies = self._collect(self._search_movies, query, year, errors)
        if not shows and not movies and errors:
            # Every catalogue failed - surface it rather than reporting an
            # empty library as if the title simply were not found.
            raise errors[0]
        return shows + movies

    @staticmethod
    def _collect(search, query, year, errors):
        try:
            return search(query, year) or []
        except VideoMetadataError as exc:
            errors.append(exc)
            return []

    def _search_shows(self, query, year=None):
        results = self._provider.search_show(query, year)
        if results:
            return results
        return self._show_fallback.search_show(query, year) or []

    def _search_movies(self, query, year=None):
        return self._provider.search_movie(query, year) or []

    def lookup_episode(self, show_title, season, episode, title=None):
        """Lookup TV episode metadata."""
        result = self._provider.search_episode(show_title, season, episode, title)
        return result or self._show_fallback.search_episode(show_title, season, episode, title)
