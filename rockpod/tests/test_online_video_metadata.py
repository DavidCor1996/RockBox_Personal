from services.online_video_metadata import (
    TMDbVideoMetadataProvider,
    TVmazeVideoMetadataProvider,
    VideoMetadataService,
)


def _show_payload():
    return {
        "id": 4011,
        "name": "Kenny vs. Spenny",
        "premiered": "2003-08-26",
        "genres": ["Comedy"],
        "runtime": 30,
        "externals": {"imdb": "tt0384746"},
        "image": {"original": "https://example.test/kvs.jpg"},
        "summary": "<p>Friends compete.</p>",
    }


def test_tvmaze_show_result_uses_canonical_poster_and_identity(monkeypatch):
    provider = TVmazeVideoMetadataProvider()
    monkeypatch.setattr(provider, "_fetch_json", lambda path, params=None: _show_payload())

    result = provider.search_show("Kenny vs Spenny")[0]

    assert result["title"] == "Kenny vs. Spenny"
    assert result["imdb_id"] == "tt0384746"
    assert result["artwork_url"] == "https://example.test/kvs.jpg"
    assert result["plot_short"] == "Friends compete."


def test_tvmaze_episode_lookup_returns_episode_and_show_poster(monkeypatch):
    provider = TVmazeVideoMetadataProvider()

    def fake_fetch(path, params=None):
        if path == "/singlesearch/shows":
            return _show_payload()
        return [{
            "id": 99,
            "name": "Who Can Drink More Beer?",
            "season": 2,
            "number": 1,
            "airdate": "2005-10-16",
            "runtime": 30,
            "summary": "<p>The beer competition.</p>",
            "image": None,
        }]

    monkeypatch.setattr(provider, "_fetch_json", fake_fetch)
    result = provider.search_episode("Kenny vs. Spenny", 2, 1)

    assert result["title"] == "Who Can Drink More Beer?"
    assert result["media_type"] == "tv_episode"
    assert result["year"] == 2005
    assert result["artwork_url"] == "https://example.test/kvs.jpg"


def test_service_falls_back_to_tvmaze_for_shows(monkeypatch):
    service = VideoMetadataService()
    monkeypatch.setattr(service._provider, "search_show", lambda query, year=None: [])
    monkeypatch.setattr(
        service._show_fallback,
        "search_show",
        lambda query, year=None: [{"title": "Recess", "media_type": "tv_show"}],
    )

    assert service.search("Recess", "tv_show")[0]["title"] == "Recess"


def test_tmdb_provider_exposes_rating_banner_and_season_art(monkeypatch):
    provider = TMDbVideoMetadataProvider(api_key="test-key")
    show = {
        "id": 100,
        "name": "Example Show",
        "first_air_date": "2020-01-02",
        "genres": [{"name": "Drama"}],
        "overview": "A grounded show.",
        "poster_path": "/poster.jpg",
        "backdrop_path": "/backdrop.jpg",
        "episode_run_time": [42],
        "vote_average": 8.4,
        "vote_count": 1200,
        "status": "Ended",
        "original_language": "en",
        "seasons": [
            {
                "season_number": 1,
                "episode_count": 2,
                "air_date": "2020-01-02",
                "overview": "First season.",
                "poster_path": "/season.jpg",
            }
        ],
    }

    def fake_fetch(path, params=None):
        del params
        if path == "/search/tv":
            return {"results": [{"id": 100, "name": "Example Show"}]}
        if path == "/tv/100":
            return show
        if path == "/tv/100/external_ids":
            return {"imdb_id": "tt0000100"}
        if path == "/tv/100/content_ratings":
            return {"results": [{"iso_3166_1": "US", "rating": "TV-14"}]}
        if path == "/tv/100/season/1":
            return {
                "episodes": [
                    {
                        "id": 1001,
                        "episode_number": 1,
                        "name": "Pilot",
                        "overview": "The pilot.",
                        "air_date": "2020-01-02",
                        "runtime": 42,
                        "still_path": "/still.jpg",
                    }
                ]
            }
        raise AssertionError(path)

    monkeypatch.setattr(provider, "_fetch_json", fake_fetch)
    result = provider.search_show("Example Show")[0]

    assert result["tmdb_id"] == "100"
    assert result["imdb_id"] == "tt0000100"
    assert result["banner_url"].endswith("/backdrop.jpg")
    assert result["external_rating"] == 8.4
    assert result["content_rating"] == "TV-14"
    assert provider.list_seasons("100")[0]["artwork_url"].endswith("/season.jpg")
    assert provider.list_episodes("100")[(1, 1)]["title"] == "Pilot"
