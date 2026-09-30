"""Focused tests for the bounded Ollama video metadata feature."""

import json
from pathlib import Path

import pytest

from app.database import SCHEMA_VERSION
from services.ollama_video_metadata import (
    OLLAMA_VIDEO_SCHEMA,
    OllamaSuggestionError,
    OllamaVideoMetadataAgent,
    OllamaVideoMetadataRunner,
)


class _Response:
    def __init__(self, payload):
        self._payload = payload

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        return False

    def read(self):
        return json.dumps(self._payload).encode("utf-8")


def test_ollama_agent_uses_structured_json_and_redacts_absolute_path():
    requests = []

    def opener(request, timeout):
        del timeout
        requests.append(request)
        return _Response(
            {
                "message": {
                    "content": json.dumps(
                        {
                            "media_type": "tv_episode",
                            "title": "Pilot",
                            "show_title": "The Example Show",
                            "search_query": "The Example Show",
                            "year": 2020,
                            "season_number": 1,
                            "episode_number": 1,
                            "confidence": 0.97,
                            "reason": "Explicit S01E01 filename tokens.",
                        }
                    )
                }
            }
        )

    agent = OllamaVideoMetadataAgent(opener=opener)
    suggestion = agent.suggest(
        {
            "file_path": "/home/david/Videos/The Example Show/S01E01.mkv",
            "title": "Episode 1",
            "show_title": "The Example Show",
            "video_kind": "show",
        }
    )

    assert suggestion["show_title"] == "The Example Show"
    assert suggestion["confidence"] == 0.97
    body = json.loads(requests[0].data.decode("utf-8"))
    assert body["format"] == OLLAMA_VIDEO_SCHEMA
    assert body["think"] is False
    assert body["options"]["temperature"] == 0
    assert body["options"]["num_predict"] == 160
    assert "/home/david/Videos" not in json.dumps(body)
    assert body["messages"][1]["content"].find("S01E01.mkv") >= 0


def test_ollama_agent_rejects_low_confidence_suggestions():
    agent = OllamaVideoMetadataAgent(min_confidence=0.9)
    with pytest.raises(OllamaSuggestionError):
        agent.normalize_suggestion(
            {
                "media_type": "movie",
                "title": "Maybe This",
                "show_title": "",
                "search_query": "Maybe This",
                "year": None,
                "season_number": None,
                "episode_number": None,
                "confidence": 0.5,
                "reason": "Ambiguous.",
            }
        )


class _Agent:
    def __init__(self, suggestions):
        self.suggestions = suggestions
        self.calls = []

    def suggest(self, row):
        self.calls.append(row["id"])
        return dict(self.suggestions[row["id"]])


class _Provider:
    def lookup_by_imdb(self, imdb_id):
        return None

    def search_shows(self, query, year=None):
        del query, year
        return [
            {
                "provider": "fixture",
                "provider_id": "show-1",
                "imdb_id": "tt0000001",
                "tmdb_id": "1001",
                "title": "The Example Show",
                "media_type": "tv_show",
                "year": 2020,
                "genre": "Drama",
                "plot_short": "A grounded fixture synopsis.",
                "plot_long": "A grounded fixture synopsis.",
                "artwork_url": "https://example.test/show.jpg",
                "banner_url": "https://example.test/show-banner.jpg",
                "content_rating": "TV-14",
                "external_rating": 8.4,
                "external_rating_votes": 1200,
            }
        ]

    def list_seasons(self, provider_id):
        assert provider_id == "show-1"
        return [{"number": 1, "artwork_url": "https://example.test/season.jpg"}]

    def list_episodes(self, provider_id):
        assert provider_id == "show-1"
        return {
            (1, 1): {
                "title": "Pilot",
                "plot_short": "The pilot synopsis.",
                "plot_long": "The pilot synopsis.",
                "year": 2020,
            }
        }


class _Service:
    def __init__(self, provider):
        self.show_provider = provider

    def search(self, query, media_type="any", year=None):
        del year
        if media_type != "movie":
            return []
        return [
            {
                "provider": "fixture",
                "provider_id": "movie-1",
                "imdb_id": "tt0000002",
                "tmdb_id": "1002",
                "title": query,
                "media_type": "movie",
                "year": 2021,
                "genre": "Comedy",
                "plot_short": "A movie fixture synopsis.",
                "plot_long": "A movie fixture synopsis.",
                "artwork_url": "https://example.test/movie.jpg",
                "banner_url": "https://example.test/movie-banner.jpg",
                "content_rating": "PG-13",
                "external_rating": 7.2,
                "external_rating_votes": 800,
            }
        ]


def _show_suggestion():
    return {
        "media_type": "tv_episode",
        "title": "Pilot",
        "show_title": "The Example Show",
        "search_query": "The Example Show",
        "year": 2020,
        "season_number": 1,
        "episode_number": 1,
        "confidence": 0.98,
        "reason": "Fixture.",
    }


def _movie_suggestion():
    return {
        "media_type": "movie",
        "title": "Example Movie",
        "show_title": "",
        "search_query": "Example Movie",
        "year": 2021,
        "season_number": None,
        "episode_number": None,
        "confidence": 0.98,
        "reason": "Fixture.",
    }


def test_runner_limits_first_pass_and_persists_show_season_banner_art(tmp_dir):
    provider = _Provider()
    agent = _Agent({1: _show_suggestion(), 2: _movie_suggestion(), 3: _movie_suggestion()})
    runner = OllamaVideoMetadataRunner(
        agent=agent,
        service=_Service(provider),
        catalog_dir=tmp_dir,
        download_image=lambda _url: b"not-a-real-image",
    )
    rows = [
        {
            "id": 1,
            "file_path": "/videos/The Example Show/S01E01.mkv",
            "media_type": "video",
            "video_kind": "show",
            "show_title": "The Example Show (WEB)",
            "title": "Episode 1",
            "season_number": 1,
            "episode_number": 1,
            "metadata_locked": 0,
        },
        {
            "id": 2,
            "file_path": "/videos/Example Movie (2021).mp4",
            "media_type": "video",
            "video_kind": "movie",
            "title": "Example Movie",
            "metadata_locked": 0,
        },
        {
            "id": 3,
            "file_path": "/videos/Another Movie.mp4",
            "media_type": "video",
            "video_kind": "movie",
            "title": "Another Movie",
            "metadata_locked": 0,
        },
    ]

    result = runner.plan(rows, limit=1, fetch_artwork=True)
    runner.save_catalog()

    assert result["eligible"] == 3
    assert result["processed"] == 1
    assert agent.calls == [1]
    assert [update["id"] for update in result["updates"]] == [1]
    values = result["updates"][0]["values"]
    assert values["imdb_id"] == "tt0000001"
    assert values["tmdb_id"] == "1001"
    assert values["external_rating"] == 8.4
    assert values["title"] == "Pilot"
    assert values["artwork_path"].endswith("the-example-show-season-1.jpg")

    catalog = json.loads((Path(tmp_dir) / "catalog.json").read_text())
    entry = catalog["titles"][0]
    assert entry["banner_art"] == "the-example-show-banner.jpg"
    assert entry["seasons"]["1"] == "the-example-show-season-1.jpg"


def test_runner_skips_locked_rows_and_does_not_call_ollama(tmp_dir):
    provider = _Provider()
    agent = _Agent({1: _show_suggestion()})
    runner = OllamaVideoMetadataRunner(
        agent=agent,
        service=_Service(provider),
        catalog_dir=tmp_dir,
    )
    result = runner.plan(
        [
            {
                "id": 1,
                "file_path": "/videos/The Example Show/S01E01.mkv",
                "media_type": "video",
                "video_kind": "show",
                "show_title": "The Example Show",
                "metadata_locked": 1,
            }
        ],
        limit=5,
    )
    assert result["eligible"] == 0
    assert result["processed"] == 0
    assert agent.calls == []


def test_runner_repairs_unlocked_movie_identity_from_provider(tmp_dir):
    provider = _Provider()
    agent = _Agent({2: _movie_suggestion()})
    runner = OllamaVideoMetadataRunner(
        agent=agent,
        service=_Service(provider),
        catalog_dir=tmp_dir,
    )
    result = runner.plan(
        [
            {
                "id": 2,
                "file_path": "/videos/Example Movie [1080p].mp4",
                "media_type": "video",
                "video_kind": "movie",
                "title": "Example Movie [1080p]",
                "metadata_locked": 0,
            }
        ],
        limit=1,
        fetch_artwork=False,
    )
    assert result["updates"][0]["values"]["title"] == "Example Movie"


def test_database_schema_contains_external_provider_rating(db):
    columns = {
        row["name"] for row in db.fetchall("PRAGMA table_info(tracks)")
    }
    assert SCHEMA_VERSION == 25
    assert {"external_rating", "external_rating_votes"} <= columns
