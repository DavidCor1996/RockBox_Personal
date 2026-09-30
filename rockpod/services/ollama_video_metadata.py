"""Ollama-guided video identity repair and grounded metadata backfill.

Ollama is used as a local parser and disambiguation assistant.  It never
supplies IDs, ratings, artwork URLs, or prose that is written directly to the
library.  Those values come from RockPod's provider layer after the LLM has
produced a structured search suggestion.  This keeps a plausible-sounding
model response from becoming a bad poster or a wrong show identity.
"""

import json
import logging
import os
import re
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

from services.video_metadata_backfill import (
    MovieMetadataBackfill,
    VideoMetadataBackfill,
    group_show_rows,
)
from services.online_video_metadata import VideoMetadataError, VideoMetadataService

logger = logging.getLogger(__name__)

DEFAULT_OLLAMA_URL = "http://127.0.0.1:11434"
DEFAULT_OLLAMA_MODEL = "qwen3:8b"
DEFAULT_MIN_CONFIDENCE = 0.86
DEFAULT_SAMPLE_LIMIT = 5

OLLAMA_VIDEO_SCHEMA = {
    "type": "object",
    "additionalProperties": False,
    "properties": {
        "media_type": {
            "type": "string",
            "enum": ["movie", "tv_show", "tv_episode", "unknown"],
        },
        "title": {"type": "string"},
        "show_title": {"type": "string"},
        "search_query": {"type": "string"},
        "year": {"type": ["integer", "null"]},
        "season_number": {"type": ["integer", "null"]},
        "episode_number": {"type": ["integer", "null"]},
        "confidence": {"type": "number", "minimum": 0, "maximum": 1},
        "reason": {"type": "string"},
    },
    "required": [
        "media_type",
        "title",
        "show_title",
        "search_query",
        "year",
        "season_number",
        "episode_number",
        "confidence",
        "reason",
    ],
}


class OllamaVideoMetadataError(Exception):
    """Base error for the local Ollama metadata assistant."""


class OllamaUnavailableError(OllamaVideoMetadataError):
    """Raised when the configured Ollama endpoint cannot be reached."""


class OllamaSuggestionError(OllamaVideoMetadataError):
    """Raised when Ollama returns malformed or unsafe metadata guidance."""


def _safe_text(value, maximum=240):
    text = re.sub(r"\s+", " ", str(value or "")).strip()
    return text[:maximum]


def _safe_int(value, minimum=0, maximum=9999):
    if value in (None, ""):
        return None
    try:
        result = int(value)
    except (TypeError, ValueError):
        return None
    if result < minimum or result > maximum:
        return None
    return result


def _safe_confidence(value):
    try:
        return max(0.0, min(1.0, float(value)))
    except (TypeError, ValueError):
        return 0.0


def _normalize_title(value):
    return " ".join(re.sub(r"[^a-z0-9]+", " ", str(value or "")).casefold().split())


class OllamaVideoMetadataAgent:
    """Call Ollama's structured-output chat endpoint for one video identity."""

    def __init__(
        self,
        base_url=DEFAULT_OLLAMA_URL,
        model=DEFAULT_OLLAMA_MODEL,
        timeout=90.0,
        min_confidence=DEFAULT_MIN_CONFIDENCE,
        opener=None,
    ):
        self.base_url = str(base_url or DEFAULT_OLLAMA_URL).rstrip("/")
        self.model = str(model or DEFAULT_OLLAMA_MODEL).strip()
        self.timeout = max(5.0, float(timeout or 90.0))
        self.min_confidence = max(
            0.0, min(1.0, float(min_confidence or DEFAULT_MIN_CONFIDENCE))
        )
        self._opener = opener or urlopen

    @classmethod
    def from_config(cls, config):
        return cls(
            base_url=config.get("ollama_base_url", DEFAULT_OLLAMA_URL),
            model=config.get("ollama_video_metadata_model", DEFAULT_OLLAMA_MODEL),
            timeout=config.get("ollama_video_metadata_timeout_seconds", 90.0),
            min_confidence=config.get(
                "ollama_video_metadata_min_confidence", DEFAULT_MIN_CONFIDENCE
            ),
        )

    def _url(self, path):
        base = self.base_url
        if base.endswith("/api"):
            return f"{base}{path}"
        return f"{base}/api{path}"

    @staticmethod
    def _path_context(file_path):
        path = Path(str(file_path or ""))
        # Keep prompts useful without sending the user's absolute filesystem
        # path to the local model or to a future remote-compatible endpoint.
        parts = [part for part in path.parts if part not in {"/", "\\"}]
        return {
            "filename": path.name,
            "parent_folders": parts[-4:-1],
        }

    def build_context(self, row):
        row = dict(row or {})
        return {
            "file": self._path_context(row.get("file_path")),
            "current_metadata": {
                "title": _safe_text(row.get("title")),
                "album": _safe_text(row.get("album")),
                "show_title": _safe_text(row.get("show_title")),
                "video_kind": _safe_text(row.get("video_kind"), 40),
                "year": _safe_int(row.get("year"), 1888, 2200),
                "season_number": _safe_int(row.get("season_number"), 1, 999),
                "episode_number": _safe_int(row.get("episode_number"), 1, 9999),
            },
        }

    def _request(self, context):
        system = (
            "You are RockPod's conservative local video filename parser. "
            "Identify the movie or television title represented by the supplied "
            "filename and folder names. Return only the requested JSON object. "
            "Do not invent an IMDb/TMDb id, rating, cast, artwork, synopsis, or "
            "release year. If identity is uncertain, use media_type unknown and "
            "confidence 0. A search_query must be a clean title suitable for a "
            "metadata catalogue. Preserve season and episode numbers when they "
            "are explicit; do not guess them."
        )
        user = json.dumps(context, ensure_ascii=False, sort_keys=True)
        body = {
            "model": self.model,
            "messages": [
                {"role": "system", "content": system},
                {"role": "user", "content": user},
            ],
            "stream": False,
            # Qwen3 enables its reasoning channel by default. This task is a
            # short, deterministic classification pass; reasoning tokens only
            # add latency and can consume the request timeout before JSON is
            # returned.
            "think": False,
            "format": OLLAMA_VIDEO_SCHEMA,
            "options": {"temperature": 0, "num_predict": 160},
        }
        request = Request(
            self._url("/chat"),
            data=json.dumps(body).encode("utf-8"),
            headers={
                "Content-Type": "application/json",
                "Accept": "application/json",
                "User-Agent": "RockPod/1.0",
            },
            method="POST",
        )
        try:
            with self._opener(request, timeout=self.timeout) as response:
                return json.loads(response.read().decode("utf-8"))
        except HTTPError as exc:
            raise OllamaUnavailableError(
                f"Ollama HTTP error {exc.code}"
            ) from exc
        except (URLError, OSError) as exc:
            raise OllamaUnavailableError(
                "Ollama is not running or cannot be reached"
            ) from exc
        except (TypeError, ValueError, json.JSONDecodeError) as exc:
            raise OllamaVideoMetadataError(
                "Ollama returned an unreadable response"
            ) from exc

    def normalize_suggestion(self, payload):
        if not isinstance(payload, dict):
            raise OllamaSuggestionError("Ollama returned a non-object suggestion")

        media_type = _safe_text(payload.get("media_type"), 30).casefold()
        if media_type not in {"movie", "tv_show", "tv_episode", "unknown"}:
            media_type = "unknown"
        title = _safe_text(payload.get("title"))
        show_title = _safe_text(payload.get("show_title"))
        search_query = _safe_text(payload.get("search_query"))
        year = _safe_int(payload.get("year"), 1888, 2200)
        season = _safe_int(payload.get("season_number"), 1, 999)
        episode = _safe_int(payload.get("episode_number"), 1, 9999)
        confidence = _safe_confidence(payload.get("confidence"))
        reason = _safe_text(payload.get("reason"), 500)

        if media_type == "movie":
            show_title = ""
            season = None
            episode = None
        elif media_type in {"tv_show", "tv_episode"}:
            show_title = show_title or title
            search_query = search_query or show_title
        if media_type == "unknown":
            confidence = 0.0

        if not title and media_type != "unknown":
            raise OllamaSuggestionError("Ollama returned no title")
        if not search_query and media_type != "unknown":
            raise OllamaSuggestionError("Ollama returned no search query")
        if confidence < self.min_confidence:
            raise OllamaSuggestionError(
                f"Ollama confidence {confidence:.2f} is below the "
                f"{self.min_confidence:.2f} safety threshold"
            )

        return {
            "media_type": media_type,
            "title": title,
            "show_title": show_title,
            "search_query": search_query,
            "year": year,
            "season_number": season,
            "episode_number": episode,
            "confidence": confidence,
            "reason": reason,
        }

    def suggest(self, row):
        response = self._request(self.build_context(row))
        message = response.get("message") if isinstance(response, dict) else None
        content = message.get("content") if isinstance(message, dict) else None
        if isinstance(content, dict):
            payload = content
        else:
            try:
                payload = json.loads(str(content or ""))
            except (TypeError, ValueError, json.JSONDecodeError) as exc:
                raise OllamaSuggestionError(
                    "Ollama did not return valid structured JSON"
                ) from exc
        return self.normalize_suggestion(payload)


class OllamaVideoMetadataRunner:
    """Run bounded, provider-grounded Ollama metadata plans."""

    def __init__(
        self,
        config=None,
        agent=None,
        service=None,
        catalog_dir=None,
        download_image=None,
        dry_run=False,
    ):
        self.config = config
        if agent is None:
            if config is not None and not config.get(
                "ollama_video_metadata_enabled", False
            ):
                raise OllamaVideoMetadataError(
                    "Ollama video metadata is disabled in RockPod Preferences"
                )
            agent = OllamaVideoMetadataAgent.from_config(config or {})
        self.agent = agent
        self.service = service or VideoMetadataService(config=config)
        self.catalog_dir = catalog_dir
        self.download_image = download_image
        self.dry_run = bool(dry_run)
        provider = getattr(self.service, "show_provider", None)
        if provider is None:
            provider = getattr(self.service, "_show_fallback", None)
        self._show_backfill = VideoMetadataBackfill(
            catalog_dir=catalog_dir,
            provider=provider,
            download_image=download_image,
            dry_run=self.dry_run,
        )
        self._movie_backfill = MovieMetadataBackfill(
            self.service,
            catalog_dir=catalog_dir,
            download_image=download_image,
            dry_run=self.dry_run,
            catalog_data=self._show_backfill._catalog,
        )

    @staticmethod
    def _eligible(rows):
        return [
            dict(row)
            for row in rows or []
            if str(row.get("media_type") or "") == "video"
            and not bool(row.get("metadata_locked"))
            and str(row.get("video_kind") or "") in {"show", "movie"}
        ]

    @staticmethod
    def _sample_targets(rows, limit):
        shows = group_show_rows([row for row in rows if row.get("video_kind") == "show"])
        movies = sorted(
            (row for row in rows if row.get("video_kind") == "movie"),
            key=lambda row: (
                str(row.get("title") or row.get("file_path") or "").casefold(),
                int(row.get("id") or 0),
            ),
        )
        targets = []
        for key in sorted(shows, key=lambda item: item.casefold()):
            group = shows[key]
            # A sample is intentionally one representative episode per show.
            # Full runs use the complete group and update every unlocked row.
            representative = sorted(
                group.rows,
                key=lambda row: int(row.get("id") or 0),
            )[0]
            sample = type(group)(group.title)
            sample.rows = [representative]
            sample.titles = set(group.titles)
            targets.append(("show", sample, representative))
        for row in movies:
            targets.append(("movie", row, row))
        if limit is None:
            return targets
        return targets[: max(0, int(limit))]

    def _guided_show_group(self, group, suggestion):
        title = suggestion.get("show_title") or suggestion.get("title") or group.title
        rows = []
        for row in group.rows:
            guided = dict(row)
            guided["show_title"] = title
            if suggestion.get("year"):
                guided["year"] = suggestion["year"]
            rows.append(guided)
        guided_group = next(iter(group_show_rows(rows).values()), None)
        if guided_group is None:
            return None
        guided_group.titles.update(group.titles)
        if suggestion.get("search_query"):
            guided_group.titles.add(suggestion["search_query"])
        return guided_group

    def plan(self, rows, limit=DEFAULT_SAMPLE_LIMIT, fetch_artwork=True, progress=None):
        eligible = self._eligible(rows)
        targets = self._sample_targets(eligible, limit)
        updates = []
        unmatched = []
        suggestions = []
        errors = []

        for index, (kind, target, representative) in enumerate(targets, start=1):
            label = str(
                representative.get("show_title")
                or representative.get("title")
                or representative.get("file_path")
                or "video"
            )
            if progress:
                progress(index, len(targets), label)
            try:
                suggestion = self.agent.suggest(representative)
                suggestions.append({"label": label, **suggestion})
                if kind == "show":
                    if suggestion["media_type"] not in {"tv_show", "tv_episode"}:
                        raise OllamaSuggestionError("Ollama classified the show as a movie")
                    guided = self._guided_show_group(target, suggestion)
                    plan = self._show_backfill.plan_show(
                        guided,
                        fetch_artwork=fetch_artwork,
                        repair_identity=True,
                    ) if guided else None
                else:
                    if suggestion["media_type"] != "movie":
                        raise OllamaSuggestionError("Ollama did not classify this item as a movie")
                    guided = dict(target)
                    guided["title"] = suggestion.get("title") or guided.get("title")
                    if suggestion.get("year"):
                        guided["year"] = suggestion["year"]
                    guided["_metadata_search_title"] = (
                        suggestion.get("search_query") or suggestion.get("title")
                    )
                    plan = self._movie_backfill.plan_movie(
                        guided,
                        fetch_artwork=fetch_artwork,
                        repair_identity=True,
                    )
                if not plan:
                    unmatched.append(label)
                    continue
                canonical = str(plan.get("record", {}).get("title") or "").strip()
                if canonical and kind == "show":
                    original_by_id = {
                        row.get("id"): str(row.get("show_title") or "").strip()
                        for row in target.rows
                    }
                    for update in plan.get("updates") or []:
                        original = original_by_id.get(update.get("id"), "")
                        if original and _normalize_title(original) != _normalize_title(canonical):
                            update.setdefault("values", {}).update(
                                {
                                    "show_title": canonical,
                                    "artist": canonical,
                                    "album_artist": canonical,
                                }
                            )
                elif canonical and kind == "movie":
                    original = str(representative.get("title") or "").strip()
                    if original and _normalize_title(original) != _normalize_title(canonical):
                        for update in plan.get("updates") or []:
                            update.setdefault("values", {})["title"] = canonical
                for update in plan.get("updates") or []:
                    update_values = update.setdefault("values", {})
                    update_values["metadata_confidence"] = suggestion["confidence"]
                updates.extend(plan.get("updates") or [])
            except (VideoMetadataError, OllamaVideoMetadataError) as exc:
                logger.warning("Ollama metadata skipped %s: %s", label, exc)
                unmatched.append(label)
                errors.append({"label": label, "error": str(exc)})
            except Exception as exc:  # one bad title must not stop a batch
                logger.exception("Ollama metadata failed for %s", label)
                unmatched.append(label)
                errors.append({"label": label, "error": str(exc)})

        return {
            "updates": updates,
            "unmatched": unmatched,
            "errors": errors,
            "suggestions": suggestions,
            "processed": len(targets),
            "eligible": len(eligible),
            "limit": limit,
            "catalog_path": os.fspath(self._show_backfill.catalog_path),
        }

    def save_catalog(self):
        self._show_backfill.save_catalog()
