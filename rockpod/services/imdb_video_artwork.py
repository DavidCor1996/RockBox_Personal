"""Deterministic access to the personal IMDb video-artwork catalog."""

import json
import os
import re
import unicodedata
from pathlib import Path


_CATALOG_DIR = Path(__file__).resolve().parent.parent / "assets" / "imdb_video_artwork"
_CATALOG_PATH = _CATALOG_DIR / "catalog.json"


def _normalize(value):
    text = unicodedata.normalize("NFKC", str(value or "")).casefold()
    text = text.replace("&", " and ")
    text = re.sub(r"[^a-z0-9]+", " ", text)
    return " ".join(text.split())


class IMDbVideoArtworkCatalog:
    """Resolve verified show, season, and movie artwork without fuzzy web hits."""

    def __init__(self, catalog_path=None):
        self.catalog_path = Path(catalog_path or _CATALOG_PATH)
        self.asset_dir = self.catalog_path.parent
        self._entries = None

    def resolve(self, track, scope=""):
        track = dict(track or {})
        kind = str(track.get("video_kind") or "movie").strip().casefold()
        primary_title = (
            track.get("show_title") if kind == "show" else track.get("title")
        )
        title = str(primary_title or track.get("album") or "").strip()
        imdb_id = str(track.get("imdb_id") or "").strip().casefold()
        year = self._as_int(track.get("year"))
        entry = self._find_entry(imdb_id, title, kind, year)
        if not entry:
            return "", {}

        resolved_scope = str(scope or "").strip().casefold()
        if not resolved_scope:
            resolved_scope = "season" if kind == "show" and self._as_int(
                track.get("season_number")
            ) else ("show" if kind == "show" else "movie")

        filename = str(entry.get("show_art") or "")
        season = self._as_int(track.get("season_number"))
        if resolved_scope == "season" and season:
            filename = str((entry.get("seasons") or {}).get(str(season)) or filename)
        path = self.asset_dir / filename
        if not filename or not path.is_file():
            return "", entry
        return os.fspath(path), entry

    def _find_entry(self, imdb_id, title, kind, year):
        entries = self._load_entries()
        if imdb_id:
            for entry in entries:
                if str(entry.get("imdb_id") or "").casefold() == imdb_id:
                    return entry

        normalized_title = _normalize(title)
        if not normalized_title:
            return None
        for entry in entries:
            entry_kind = str(entry.get("kind") or "").casefold()
            if kind == "show" and entry_kind != "show":
                continue
            if kind != "show" and entry_kind == "show":
                continue
            names = [entry.get("title")] + list(entry.get("aliases") or [])
            if normalized_title not in {_normalize(name) for name in names}:
                continue
            entry_year = self._as_int(entry.get("year"))
            if year and entry_year and year != entry_year:
                continue
            return entry
        return None

    def _load_entries(self):
        if self._entries is not None:
            return self._entries
        try:
            with self.catalog_path.open("r", encoding="utf-8") as handle:
                payload = json.load(handle)
            self._entries = list(payload.get("titles") or [])
        except (OSError, ValueError, TypeError):
            self._entries = []
        return self._entries

    @staticmethod
    def _as_int(value):
        try:
            return int(value or 0)
        except (TypeError, ValueError):
            return 0
