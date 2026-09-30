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
                "content_rating": detail.get("Rated") or "",
                "external_rating": self._parse_rating(detail.get("imdbRating")),
                "external_rating_votes": self._parse_votes(detail.get("imdbVotes")),
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
                "content_rating": detail.get("Rated") or "",
                "external_rating": self._parse_rating(detail.get("imdbRating")),
                "external_rating_votes": self._parse_votes(detail.get("imdbVotes")),
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
            "content_rating": detail.get("Rated") or "",
            "external_rating": self._parse_rating(detail.get("imdbRating")),
            "external_rating_votes": self._parse_votes(detail.get("imdbVotes")),
        }

    @staticmethod
    def _parse_runtime(text):
        if not text:
            return 0
        match = re.search(r"(\d+)\s*min", text, re.IGNORECASE)
        return int(match.group(1)) * 60 if match else 0

    @staticmethod
    def _parse_rating(value):
        try:
            rating = float(value)
        except (TypeError, ValueError):
            return 0.0
        return rating if 0.0 <= rating <= 10.0 else 0.0

    @staticmethod
    def _parse_votes(value):
        digits = re.sub(r"[^0-9]", "", str(value or ""))
        try:
            return int(digits or 0)
        except ValueError:
            return 0


class TMDbVideoMetadataProvider(VideoMetadataProvider):
    """Rich movie/TV provider with posters, backdrops, seasons and ratings.

    TMDb is optional because it needs a user API key.  When configured it is
    the preferred provider: it supplies the extra artwork and TV hierarchy
    that a filename parser cannot provide.  The provider is deliberately kept
    independent of the Ollama client so metadata remains grounded in a
    catalogue response rather than model memory.
    """

    API_ROOT = "https://api.themoviedb.org/3"
    IMAGE_ROOT = "https://image.tmdb.org/t/p/original"

    def __init__(self, api_key="", language="en-US", timeout=10.0):
        self.api_key = str(api_key or "").strip()
        self.language = str(language or "en-US").strip()
        self.timeout = float(timeout or 10.0)

    def _fetch_json(self, path, params=None):
        if not self.api_key:
            raise VideoMetadataError("TMDb API key not configured")
        query = dict(params or {})
        query.update({"api_key": self.api_key, "language": self.language})
        url = f"{self.API_ROOT}{path}?{urllib.parse.urlencode(query)}"
        request = Request(url, headers={"User-Agent": "RockPod/1.0"})
        try:
            with urlopen(request, timeout=self.timeout) as response:
                return json.loads(response.read().decode("utf-8"))
        except HTTPError as exc:
            if exc.code == 404:
                return {}
            raise VideoMetadataNetworkError(f"TMDb HTTP error {exc.code}") from exc
        except URLError as exc:
            raise VideoMetadataNetworkError("TMDb connection failed") from exc
        except (OSError, ValueError) as exc:
            raise VideoMetadataError("TMDb response could not be read") from exc

    @classmethod
    def _image_url(cls, path):
        return f"{cls.IMAGE_ROOT}{path}" if path else ""

    @staticmethod
    def _year(value):
        text = str(value or "")
        return int(text[:4]) if len(text) >= 4 and text[:4].isdigit() else None

    @staticmethod
    def _rating(value):
        try:
            value = float(value or 0)
        except (TypeError, ValueError):
            return 0.0
        return value if 0.0 <= value <= 10.0 else 0.0

    @staticmethod
    def _votes(value):
        try:
            return max(0, int(value or 0))
        except (TypeError, ValueError):
            return 0

    @staticmethod
    def _genres(item):
        return ", ".join(
            str(value.get("name") or "").strip()
            for value in item.get("genres") or []
            if isinstance(value, dict) and value.get("name")
        )

    @staticmethod
    def _us_rating(items, key):
        for country in items or []:
            if str(country.get("iso_3166_1") or "").upper() != "US":
                continue
            values = country.get(key) or []
            if isinstance(values, str):
                return values.strip()
            for result in values:
                if not isinstance(result, dict):
                    continue
                value = str(result.get("certification") or result.get("rating") or "").strip()
                if value:
                    return value
        return ""

    def _movie_result(self, item, detail=None):
        detail = detail or item
        external = self._fetch_json(
            f"/movie/{item.get('id')}/external_ids"
        ) if item.get("id") else {}
        release_dates = self._fetch_json(
            f"/movie/{item.get('id')}/release_dates"
        ) if item.get("id") else {}
        return {
            "provider": "tmdb",
            "provider_id": str(item.get("id") or ""),
            "imdb_id": str(external.get("imdb_id") or ""),
            "tmdb_id": str(item.get("id") or ""),
            "title": str(detail.get("title") or detail.get("original_title") or ""),
            "media_type": "movie",
            "year": self._year(detail.get("release_date")),
            "release_date": str(detail.get("release_date") or ""),
            "genre": self._genres(detail),
            "plot_short": str(detail.get("overview") or "").strip(),
            "plot_long": str(detail.get("overview") or "").strip(),
            "artwork_url": self._image_url(detail.get("poster_path")),
            "banner_url": self._image_url(detail.get("backdrop_path")),
            "runtime_seconds": int(detail.get("runtime") or 0) * 60,
            "content_rating": self._us_rating(release_dates.get("results"), "release_dates"),
            "external_rating": self._rating(detail.get("vote_average")),
            "external_rating_votes": self._votes(detail.get("vote_count")),
            "status": str(detail.get("status") or ""),
            "original_language": str(detail.get("original_language") or ""),
        }

    def _show_result(self, item, detail=None):
        detail = detail or item
        external = self._fetch_json(
            f"/tv/{item.get('id')}/external_ids"
        ) if item.get("id") else {}
        ratings = self._fetch_json(
            f"/tv/{item.get('id')}/content_ratings"
        ) if item.get("id") else {}
        return {
            "provider": "tmdb",
            "provider_id": str(item.get("id") or ""),
            "imdb_id": str(external.get("imdb_id") or ""),
            "tmdb_id": str(item.get("id") or ""),
            "title": str(detail.get("name") or detail.get("original_name") or ""),
            "media_type": "tv_show",
            "year": self._year(detail.get("first_air_date")),
            "release_date": str(detail.get("first_air_date") or ""),
            "genre": self._genres(detail),
            "plot_short": str(detail.get("overview") or "").strip(),
            "plot_long": str(detail.get("overview") or "").strip(),
            "artwork_url": self._image_url(detail.get("poster_path")),
            "banner_url": self._image_url(detail.get("backdrop_path")),
            "runtime_seconds": int(
                detail.get("episode_run_time", [0])[0]
                if detail.get("episode_run_time") else 0
            ) * 60,
            "content_rating": self._us_rating(ratings.get("results"), "rating"),
            "external_rating": self._rating(detail.get("vote_average")),
            "external_rating_votes": self._votes(detail.get("vote_count")),
            "status": str(detail.get("status") or ""),
            "original_language": str(detail.get("original_language") or ""),
        }

    def search_movie(self, query, year=None):
        params = {"query": query, "include_adult": "false"}
        if year:
            params["year"] = str(year)
        payload = self._fetch_json("/search/movie", params)
        results = []
        for item in (payload or {}).get("results", [])[:10]:
            if not item.get("id"):
                continue
            detail = self._fetch_json(f"/movie/{item['id']}")
            results.append(self._movie_result(item, detail))
        return results

    def search_show(self, query, year=None):
        params = {"query": query, "include_adult": "false"}
        if year:
            params["first_air_date_year"] = str(year)
        payload = self._fetch_json("/search/tv", params)
        results = []
        for item in (payload or {}).get("results", [])[:10]:
            if not item.get("id"):
                continue
            detail = self._fetch_json(f"/tv/{item['id']}")
            results.append(self._show_result(item, detail))
        return results

    def lookup_by_imdb(self, imdb_id):
        payload = self._fetch_json(
            f"/find/{urllib.parse.quote(str(imdb_id or '').strip())}",
            {"external_source": "imdb_id"},
        )
        movie = next(iter(payload.get("movie_results") or []), None)
        if movie:
            return self._movie_result(movie, self._fetch_json(f"/movie/{movie['id']}"))
        show = next(iter(payload.get("tv_results") or []), None)
        if show:
            return self._show_result(show, self._fetch_json(f"/tv/{show['id']}"))
        return None

    def list_seasons(self, provider_id):
        detail = self._fetch_json(f"/tv/{provider_id}")
        return [
            {
                "number": int(season.get("season_number")),
                "episode_count": int(season.get("episode_count") or 0),
                "premiered": str(season.get("air_date") or ""),
                "summary": str(season.get("overview") or "").strip(),
                "artwork_url": self._image_url(season.get("poster_path")),
            }
            for season in detail.get("seasons") or []
            if season.get("season_number") is not None
        ]

    def list_episodes(self, provider_id):
        episodes = {}
        for season in self.list_seasons(provider_id):
            number = int(season["number"])
            payload = self._fetch_json(f"/tv/{provider_id}/season/{number}")
            for item in payload.get("episodes") or []:
                episode = item.get("episode_number")
                if episode is None:
                    continue
                episodes[(number, int(episode))] = {
                    "title": str(item.get("name") or ""),
                    "plot_short": str(item.get("overview") or "").strip(),
                    "plot_long": str(item.get("overview") or "").strip(),
                    "release_date": str(item.get("air_date") or ""),
                    "year": self._year(item.get("air_date")),
                    "runtime_seconds": int(item.get("runtime") or 0) * 60,
                    "artwork_url": self._image_url(item.get("still_path")),
                }
        return episodes

    def search_episode(self, show_title, season, episode, episode_title=None):
        shows = self.search_show(show_title)
        if not shows:
            return None
        show = shows[0]
        detail = self._fetch_json(
            f"/tv/{show['provider_id']}/season/{int(season)}"
        )
        for item in detail.get("episodes") or []:
            if int(item.get("episode_number") or -1) != int(episode):
                continue
            result = dict(show)
            summary = str(item.get("overview") or "").strip() or show.get("plot_short", "")
            result.update(
                {
                    "provider_id": str(item.get("id") or show["provider_id"]),
                    "title": str(item.get("name") or episode_title or ""),
                    "media_type": "tv_episode",
                    "year": self._year(item.get("air_date")) or show.get("year"),
                    "release_date": str(item.get("air_date") or ""),
                    "plot_short": summary,
                    "plot_long": summary,
                    "artwork_url": self._image_url(item.get("still_path")) or show.get("artwork_url", ""),
                    "runtime_seconds": int(item.get("runtime") or 0) * 60,
                }
            )
            return result
        return None

    def get_title_by_id(self, provider_id):
        item = self._fetch_json(f"/tv/{provider_id}")
        return self._show_result(item, item) if item else None


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
        results = self.search_shows(query, year)
        return results[:1]

    def search_shows(self, query, year=None):
        """Return show candidates ordered by how well they fit the query.

        ``/singlesearch`` alone picks the most popular name match, which hands
        back the 2024 "Avatar: The Last Airbender" for a 2005 library folder.
        Scoring the full result set by release year keeps a remake from
        stealing the identity of the show that is actually on disk.
        """
        candidates = []
        payload = self._fetch_json("/search/shows", {"q": query})
        for entry in payload if isinstance(payload, list) else []:
            if not isinstance(entry, dict):
                continue
            item = entry.get("show")
            if isinstance(item, dict) and item.get("id"):
                candidates.append(dict(item))
        if not candidates:
            item = self._fetch_json("/singlesearch/shows", {"q": query})
            if item:
                candidates.append(item)
        results = [self._show_result(item) for item in candidates]
        if not year:
            return results
        year = int(year)
        exact = [r for r in results if r.get("year") and int(r["year"]) == year]
        if exact:
            return exact + [r for r in results if r not in exact]
        near = sorted(
            (r for r in results if r.get("year")),
            key=lambda r: abs(int(r["year"]) - year),
        )
        return near + [r for r in results if not r.get("year")]

    def lookup_by_imdb(self, imdb_id):
        """Resolve a show straight from a verified IMDb id."""
        text = str(imdb_id or "").strip()
        if not text:
            return None
        item = self._fetch_json("/lookup/shows", {"imdb": text})
        return self._show_result(item) if item else None

    def list_seasons(self, provider_id):
        """Return every season with its published cover, when one exists."""
        seasons = []
        for item in self._fetch_json(f"/shows/{provider_id}/seasons") or []:
            number = item.get("number")
            if number is None:
                continue
            image = dict(item.get("image") or {})
            seasons.append(
                {
                    "number": int(number),
                    "episode_count": int(item.get("episodeOrder") or 0),
                    "premiered": str(item.get("premiered") or ""),
                    "summary": self._plain_text(item.get("summary")),
                    "artwork_url": str(
                        image.get("original") or image.get("medium") or ""
                    ),
                }
            )
        return seasons

    def list_episodes(self, provider_id):
        """Return every episode of a show keyed by ``season x episode``.

        One request covers a whole series, so a library with hundreds of
        episodes costs a single call per show rather than one per file.
        """
        episodes = {}
        for item in self._fetch_json(f"/shows/{provider_id}/episodes") or []:
            season = item.get("season")
            number = item.get("number")
            if season is None or number is None:
                continue
            image = dict(item.get("image") or {})
            summary = self._plain_text(item.get("summary"))
            episodes[(int(season), int(number))] = {
                "title": str(item.get("name") or ""),
                "plot_short": summary,
                "plot_long": summary,
                "release_date": str(item.get("airdate") or ""),
                "year": self._year(item.get("airdate")),
                "runtime_seconds": int(item.get("runtime") or 0) * 60,
                "artwork_url": str(image.get("original") or image.get("medium") or ""),
            }
        return episodes

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
            tmdb_key = str(self._config.get("tmdb_api_key", "") or "").strip()
            if tmdb_key:
                self._provider = TMDbVideoMetadataProvider(api_key=tmdb_key)
                return
            omdb_key = self._config.get("omdb_api_key", "").strip()
            if omdb_key:
                self._provider = OMDbVideoMetadataProvider(api_key=omdb_key)
                return
        self._provider = ITunesVideoMetadataProvider()

    @property
    def provider(self):
        """Return the configured primary provider for advanced integrations."""
        return self._provider

    @property
    def show_provider(self):
        """Return a provider that supports show hierarchy artwork and episodes."""
        if isinstance(self._provider, (TMDbVideoMetadataProvider, TVmazeVideoMetadataProvider)):
            return self._provider
        return self._show_fallback

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
