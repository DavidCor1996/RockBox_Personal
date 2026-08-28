"""Fill in missing video metadata and register real show/season artwork.

The desktop library and the iPod manifest both read verified identity out of
``assets/imdb_video_artwork/catalog.json`` (see
:mod:`services.imdb_video_artwork`), and that file outlives the database - a
re-import after the source files move deletes and re-inserts rows, taking
their columns with them. This module therefore writes both: the durable
catalog entry - IMDb id, canonical title, aliases for every spelling the
library has used, genre, certificate, synopsis and real downloaded show and
season covers - and the per-row database fields the desktop views read
directly.

Every asset is a published cover fetched from the metadata provider. A title
with no published artwork gets no artwork; nothing is ever generated locally to
fill a hole.
"""

import io
import json
import logging
import os
import re
import unicodedata
from pathlib import Path

from models.track import compute_metadata_hash
from services.imdb_video_artwork import IMDbVideoArtworkCatalog
from services.online_video_metadata import (
    TVmazeVideoMetadataProvider,
    VideoMetadataError,
)

logger = logging.getLogger(__name__)

DEFAULT_CATALOG_DIR = (
    Path(__file__).resolve().parent.parent / "assets" / "imdb_video_artwork"
)
POSTER_MAX_EDGE = 720
POSTER_QUALITY = 88

# Titles the parser produces when a filename carries no episode name. They are
# placeholders, so a real provider title is allowed to replace them.
_PLACEHOLDER_TITLE_RE = re.compile(r"(?i)^(?:episode|ep)\s*\d{1,3}$")


def _slug(value):
    text = unicodedata.normalize("NFKD", str(value or ""))
    text = text.encode("ascii", "ignore").decode("ascii").casefold()
    text = re.sub(r"[^a-z0-9]+", "-", text).strip("-")
    return text or "title"


def _normalize(value):
    text = unicodedata.normalize("NFKC", str(value or "")).casefold()
    text = text.replace("&", " and ")
    return " ".join(re.sub(r"[^a-z0-9]+", " ", text).split())


def _as_int(value):
    try:
        return int(value or 0)
    except (TypeError, ValueError):
        return 0


def _text(row, field):
    return str((row or {}).get(field) or "").strip()


class ShowGroup:
    """Every library row that belongs to one series."""

    def __init__(self, title):
        self.title = title
        self.rows = []
        self.titles = set()

    @property
    def seasons(self):
        numbers = {_as_int(row.get("season_number")) for row in self.rows}
        return sorted(number for number in numbers if number > 0)

    @property
    def year(self):
        years = sorted({_as_int(row.get("year")) for row in self.rows} - {0})
        return years[0] if years else None

    @property
    def imdb_id(self):
        for row in self.rows:
            imdb_id = _text(row, "imdb_id")
            if imdb_id:
                return imdb_id
        return ""


def group_show_rows(rows):
    """Bucket show rows by series, merging punctuation-only name variants."""
    groups = {}
    for row in rows or []:
        if str(row.get("video_kind") or "") != "show":
            continue
        title = _text(row, "show_title")
        if not title:
            continue
        key = _normalize(title)
        group = groups.get(key)
        if group is None:
            group = groups[key] = ShowGroup(title)
        group.rows.append(row)
        group.titles.add(title)
    return groups


class VideoMetadataBackfill:
    """Resolve real metadata and artwork for the video library."""

    def __init__(
        self,
        catalog_dir=None,
        provider=None,
        download_image=None,
        dry_run=False,
    ):
        self.dry_run = bool(dry_run)
        self.catalog_dir = Path(catalog_dir or DEFAULT_CATALOG_DIR)
        self.catalog_path = self.catalog_dir / "catalog.json"
        self._provider = provider or TVmazeVideoMetadataProvider()
        self._download_image = download_image or self._default_download_image
        self._catalog = self._load_catalog()

    # ---------------------------------------------------------------- catalog

    def _load_catalog(self):
        try:
            with self.catalog_path.open("r", encoding="utf-8") as handle:
                payload = json.load(handle)
        except (OSError, ValueError):
            payload = {}
        payload.setdefault("version", 1)
        payload.setdefault(
            "usage",
            "Personal, non-commercial artwork with per-title source provenance.",
        )
        payload.setdefault("titles", [])
        return payload

    def _catalog_entry(self, imdb_id, title, kind="show"):
        imdb_id = str(imdb_id or "").casefold()
        for entry in self._catalog["titles"]:
            if imdb_id and str(entry.get("imdb_id") or "").casefold() == imdb_id:
                return entry
        for entry in self._catalog["titles"]:
            if str(entry.get("kind") or "") != kind:
                continue
            names = [entry.get("title")] + list(entry.get("aliases") or [])
            if _normalize(title) in {_normalize(name) for name in names}:
                return entry
        entry = {"imdb_id": imdb_id, "title": title, "kind": kind}
        self._catalog["titles"].append(entry)
        return entry

    def save_catalog(self):
        self.catalog_dir.mkdir(parents=True, exist_ok=True)
        tmp = self.catalog_path.with_suffix(".json.tmp")
        with tmp.open("w", encoding="utf-8") as handle:
            json.dump(self._catalog, handle, indent=2, ensure_ascii=False)
            handle.write("\n")
        os.replace(tmp, self.catalog_path)

    # ---------------------------------------------------------------- network

    @staticmethod
    def _default_download_image(url):
        from services.online_artwork import ITunesArtworkLookup

        return ITunesArtworkLookup().download_image(url)[0]

    def _store_poster(self, url, filename):
        """Download one published cover; return the stored filename or ""."""
        if not url:
            return ""
        target = self.catalog_dir / filename
        if target.is_file() and target.stat().st_size > 0:
            return filename
        if self.dry_run:
            return filename
        try:
            data = self._download_image(url)
        except Exception as exc:
            logger.warning("Poster download failed for %s: %s", url, exc)
            return ""
        if not data:
            return ""
        data = self._shrink(data)
        self.catalog_dir.mkdir(parents=True, exist_ok=True)
        tmp = target.with_suffix(target.suffix + ".tmp")
        with tmp.open("wb") as handle:
            handle.write(data)
        os.replace(tmp, target)
        return filename

    @staticmethod
    def _shrink(data):
        """Keep committed covers small without changing what they show."""
        try:
            from PIL import Image
        except ImportError:
            return data
        try:
            with Image.open(io.BytesIO(data)) as image:
                image = image.convert("RGB")
                if max(image.size) <= POSTER_MAX_EDGE:
                    return data
                scale = POSTER_MAX_EDGE / float(max(image.size))
                size = (
                    max(1, int(image.width * scale)),
                    max(1, int(image.height * scale)),
                )
                resized = image.resize(size, Image.LANCZOS)
                buffer = io.BytesIO()
                resized.save(buffer, format="JPEG", quality=POSTER_QUALITY)
                return buffer.getvalue()
        except Exception:
            return data

    # ----------------------------------------------------------------- resolve

    def resolve_show(self, group):
        """Find the provider record for a series, IMDb id first."""
        imdb_id = group.imdb_id
        if not imdb_id:
            catalog = IMDbVideoArtworkCatalog(self.catalog_path)
            _art, entry = catalog.resolve(
                {"video_kind": "show", "show_title": group.title}, scope="show"
            )
            imdb_id = str((entry or {}).get("imdb_id") or "")
        if imdb_id:
            record = self._provider.lookup_by_imdb(imdb_id)
            if record:
                return record
        for query in self._queries(group):
            results = self._provider.search_shows(query, group.year)
            if results:
                return results[0]
        return None

    @staticmethod
    def _queries(group):
        """Query spellings, widest identity first.

        The library spelling is tried before any trimmed variant because the
        extra token is often what distinguishes the wanted show - "The Office
        US" resolves to the US remake, plain "The Office" to the UK original.
        """
        seen = []
        for candidate in sorted(group.titles, key=len, reverse=True) + [group.title]:
            candidate = str(candidate or "").strip()
            if candidate and candidate not in seen:
                seen.append(candidate)
        trimmed = re.sub(r"(?i)\s+\(?(?:us|uk|au|ca)\)?$", "", seen[0]).strip()
        if trimmed and trimmed not in seen:
            seen.append(trimmed)
        return seen

    # -------------------------------------------------------------------- plan

    def plan_show(self, group, fetch_artwork=True):
        """Return the catalog entry and per-row updates for one series."""
        record = self.resolve_show(group)
        if not record:
            return None
        canonical = str(record.get("title") or group.title).strip()
        entry = self._catalog_entry(record.get("imdb_id"), canonical, "show")
        aliases = list(entry.get("aliases") or [])
        for name in sorted(group.titles) + [canonical]:
            if name and name != canonical and name not in aliases:
                aliases.append(name)
        entry.update(
            {
                "imdb_id": str(record.get("imdb_id") or entry.get("imdb_id") or ""),
                "title": canonical,
                "kind": "show",
                "aliases": aliases,
            }
        )
        for field, value in (
            ("year", record.get("year")),
            ("genre", record.get("genre")),
            ("content_rating", record.get("content_rating")),
            ("show_plot", record.get("plot_short")),
        ):
            if value and not entry.get(field):
                entry[field] = value

        episodes = {}
        provider_id = str(record.get("provider_id") or "")
        if provider_id:
            try:
                episodes = self._provider.list_episodes(provider_id)
            except VideoMetadataError as exc:
                logger.warning("Episode list failed for %s: %s", canonical, exc)

        if fetch_artwork and provider_id:
            self._plan_artwork(entry, record, group, provider_id, canonical)

        updates = [
            update
            for update in (
                self._row_update(row, record, entry, episodes, canonical)
                for row in group.rows
            )
            if update
        ]
        return {"entry": entry, "record": record, "updates": updates}

    def _plan_artwork(self, entry, record, group, provider_id, canonical):
        slug = _slug(canonical)
        stored = self._store_poster(
            record.get("artwork_url"), f"{slug}-show.jpg"
        )
        if stored:
            entry["show_art"] = stored
        wanted = set(group.seasons)
        if not wanted:
            # Nothing in the library is filed under a season, so a season
            # cover would never be shown - do not fetch what cannot be used.
            return
        try:
            seasons = self._provider.list_seasons(provider_id)
        except VideoMetadataError as exc:
            logger.warning("Season list failed for %s: %s", canonical, exc)
            return
        season_art = dict(entry.get("seasons") or {})
        for season in seasons:
            number = season["number"]
            if number not in wanted:
                continue
            stored = self._store_poster(
                season.get("artwork_url"), f"{slug}-season-{number}.jpg"
            )
            # A season with no published cover falls back to the show cover
            # rather than to anything invented for the occasion.
            if stored:
                season_art[str(number)] = stored
            elif entry.get("show_art") and str(number) not in season_art:
                season_art[str(number)] = entry["show_art"]
        if season_art:
            entry["seasons"] = dict(sorted(season_art.items(), key=lambda kv: int(kv[0])))

    def _entry_poster(self, entry, season):
        """Absolute path of the stored cover this row should display."""
        filename = ""
        if season:
            filename = str((entry.get("seasons") or {}).get(str(season)) or "")
        filename = filename or str(entry.get("show_art") or "")
        if not filename:
            return ""
        path = self.catalog_dir / filename
        return os.fspath(path) if path.is_file() else ""

    def _row_update(self, row, record, entry, episodes, canonical):
        if row.get("metadata_locked"):
            return None
        updates = {}

        def fill(field, value):
            if value and not _text(row, field):
                updates[field] = value

        fill("imdb_id", record.get("imdb_id"))
        fill("genre", record.get("genre") or entry.get("genre"))
        fill("content_rating", record.get("content_rating") or entry.get("content_rating"))
        fill("show_plot", record.get("plot_short") or entry.get("show_plot"))
        if not _as_int(row.get("year")) and record.get("year"):
            updates["year"] = int(record["year"])
        if canonical and _text(row, "show_title") != canonical:
            updates["show_title"] = canonical
            updates["artist"] = canonical
            updates["album_artist"] = canonical

        season = _as_int(row.get("season_number"))
        episode = _as_int(row.get("episode_number"))
        detail = episodes.get((season, episode)) if season and episode else None
        if detail:
            title = str(detail.get("title") or "").strip()
            current = _text(row, "title")
            if title and (not current or _PLACEHOLDER_TITLE_RE.match(current)):
                updates["title"] = title
            fill("plot_short", detail.get("plot_short"))
            fill("plot_long", detail.get("plot_long"))
            if not _as_int(row.get("year")) and detail.get("year"):
                updates["year"] = int(detail["year"])

        poster = self._entry_poster(entry, _as_int(row.get("season_number")))
        if poster and _text(row, "artwork_path") != poster:
            updates["artwork_path"] = poster

        if not updates:
            return None
        updates["metadata_source"] = record.get("provider") or "tvmaze"
        updates["metadata_confidence"] = 1.0
        merged = dict(row)
        merged.update(updates)
        updates["metadata_hash"] = compute_metadata_hash(
            merged.get("title"),
            merged.get("artist"),
            merged.get("album"),
            merged.get("album_artist"),
            merged.get("track_number"),
            merged.get("disc_number"),
            merged.get("genre"),
            merged.get("year"),
            merged.get("composer"),
            merged.get("duration"),
            merged.get("bitrate"),
            merged.get("codec"),
            merged.get("media_type"),
            merged.get("video_kind"),
            merged.get("show_title"),
            merged.get("season_number"),
            merged.get("episode_number"),
        )
        return {"id": row.get("id"), "file_path": row.get("file_path"), "values": updates}


class MovieMetadataBackfill(VideoMetadataBackfill):
    """Movie identity from a catalogue that actually carries films.

    TVmaze is television only, so movies come from the provider configured for
    the app (OMDb when a key is present, otherwise the iTunes store). A match
    is accepted only when the normalised titles agree and, where both are
    known, the years are within a year of each other - a wrong poster is worse
    than no poster.
    """

    def __init__(self, service, catalog_dir=None, download_image=None, dry_run=False):
        super().__init__(
            catalog_dir=catalog_dir,
            provider=None,
            download_image=download_image,
            dry_run=dry_run,
        )
        self._service = service

    def resolve_movie(self, row):
        title = _text(row, "title") or _text(row, "album")
        if not title:
            return None
        year = _as_int(row.get("year")) or None
        try:
            results = self._service.search(title, media_type="movie", year=year)
        except VideoMetadataError as exc:
            logger.warning("Movie lookup failed for %s: %s", title, exc)
            return None
        wanted = _normalize(title)
        for result in results or []:
            if str(result.get("media_type") or "") != "movie":
                continue
            if _normalize(result.get("title")) != wanted:
                continue
            result_year = _as_int(result.get("year"))
            if year and result_year and abs(result_year - year) > 1:
                continue
            return result
        return None

    def plan_movie(self, row, fetch_artwork=True):
        if row.get("metadata_locked"):
            return None
        record = self.resolve_movie(row)
        if not record:
            return None
        canonical = str(record.get("title") or "").strip()
        entry = self._catalog_entry(record.get("imdb_id"), canonical, "movie")
        entry.update({"title": canonical, "kind": "movie"})
        if record.get("imdb_id"):
            entry["imdb_id"] = str(record["imdb_id"])
        original = _text(row, "title")
        aliases = list(entry.get("aliases") or [])
        if original and original != canonical and original not in aliases:
            aliases.append(original)
        if aliases:
            entry["aliases"] = aliases
        for field, value in (
            ("year", record.get("year")),
            ("genre", record.get("genre")),
            ("content_rating", record.get("content_rating")),
            ("show_plot", record.get("plot_short")),
        ):
            if value and not entry.get(field):
                entry[field] = value
        if fetch_artwork:
            stored = self._store_poster(
                record.get("artwork_url"), f"{_slug(canonical)}-{_as_int(record.get('year')) or 'movie'}.jpg"
            )
            if stored:
                entry["show_art"] = stored

        updates = {}

        def fill(field, value):
            if value and not _text(row, field):
                updates[field] = value

        fill("imdb_id", record.get("imdb_id"))
        fill("genre", record.get("genre"))
        fill("content_rating", record.get("content_rating"))
        fill("plot_short", record.get("plot_short"))
        fill("plot_long", record.get("plot_long") or record.get("plot_short"))
        if not _as_int(row.get("year")) and record.get("year"):
            updates["year"] = int(record["year"])
        if not updates:
            return {"entry": entry, "record": record, "updates": []}
        updates["metadata_source"] = record.get("provider") or "online"
        updates["metadata_confidence"] = 1.0
        merged = dict(row)
        merged.update(updates)
        updates["metadata_hash"] = compute_metadata_hash(
            merged.get("title"), merged.get("artist"), merged.get("album"),
            merged.get("album_artist"), merged.get("track_number"),
            merged.get("disc_number"), merged.get("genre"), merged.get("year"),
            merged.get("composer"), merged.get("duration"), merged.get("bitrate"),
            merged.get("codec"), merged.get("media_type"), merged.get("video_kind"),
            merged.get("show_title"), merged.get("season_number"),
            merged.get("episode_number"),
        )
        return {
            "entry": entry,
            "record": record,
            "updates": [
                {"id": row.get("id"), "file_path": row.get("file_path"), "values": updates}
            ],
        }
