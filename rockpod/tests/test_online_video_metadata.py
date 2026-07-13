from services.online_video_metadata import TVmazeVideoMetadataProvider, VideoMetadataService


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
