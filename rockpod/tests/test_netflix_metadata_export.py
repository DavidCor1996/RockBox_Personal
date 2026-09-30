"""Regression tests for the Netflix catalog's metadata and manifest export.

These cover defects found in a real synced manifest: episodes matched at show
level carried no description, movies could not be matched because they were
filed as TV, and season numbers were exported unnormalised so "03" and "3"
split one season into two lists.
"""

import ast
import re
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
ROCKPOD = ROOT / "rockpod"


def _load_function(module_path, name, namespace=None):
    """Exec a single top-level function out of a module.

    sync_engine and video_thumbnails pull in PySide6 and PIL at import time,
    which these pure-logic checks do not need.
    """
    source = module_path.read_text()
    tree = ast.parse(source)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == name:
            scope = dict(namespace or {})
            exec(ast.get_source_segment(source, node), scope)
            return scope[name]
    raise AssertionError(f"{name} not found in {module_path}")


def _manifest_columns():
    """The single tuple the header line and every row are generated from."""
    source = (ROCKPOD / "services/video_thumbnails.py").read_text()
    tree = ast.parse(source)
    for node in ast.walk(tree):
        if not isinstance(node, ast.Assign):
            continue
        if not any(getattr(target, "id", "") == "_MANIFEST_COLUMNS"
                   for target in node.targets):
            continue
        return [element.value for element in node.value.elts]
    raise AssertionError("_MANIFEST_COLUMNS not found")


def test_manifest_header_and_rows_come_from_one_column_list():
    source = (ROCKPOD / "services/video_thumbnails.py").read_text()
    tree = ast.parse(source)
    body = None
    for node in ast.walk(tree):
        if (isinstance(node, ast.FunctionDef)
                and node.name == "export_video_list_manifest"):
            body = ast.get_source_segment(source, node)
            break
    assert body is not None

    # Header and rows must both be driven by _MANIFEST_COLUMNS, so they
    # cannot drift out of step the way a hand-maintained pair would.
    assert '"\\t".join(self._MANIFEST_COLUMNS)' in body
    assert "for name in self._MANIFEST_COLUMNS" in body
    assert len(_manifest_columns()) == 30


def test_manifest_appends_show_plot_without_moving_existing_columns():
    columns = _manifest_columns()
    # The device parser reads these positionally, so the prefix must be
    # stable and any new column must land at the end.
    assert columns[:22] == [
        "video_id", "thumb", "preview", "title", "kind", "group_key",
        "device_path", "show", "season", "episode", "duration", "locked",
        "year", "genre", "rating", "plot_short", "plot_long",
        "content_rating", "netflix_poster", "netflix_detail", "show_art_id",
        "season_art_id",
    ]
    assert columns[22:] == [
        "show_plot", "intro_start", "intro_end", "credits_start",
        "credits_duration", "external_rating_tenths",
        "external_rating_votes", "banner_art_id",
    ]
    assert "# rockpod videolist v8" in (
        ROCKPOD / "services/video_thumbnails.py"
    ).read_text()


@pytest.mark.parametrize(
    "value,expected",
    [
        ("03", "3"),
        ("3", "3"),
        (3, "3"),
        ("  07 ", "7"),
        ("Season 4", "4"),
        ("", ""),
        (None, ""),
        ("unknown", ""),
    ],
)
def test_manifest_ordinal_normalises_season_and_episode(value, expected):
    ordinal = _load_function(
        ROCKPOD / "services/sync_engine.py", "_manifest_ordinal", {"re": re}
    )
    assert ordinal(value) == expected


def test_manifest_rows_normalise_season_and_episode():
    source = (ROCKPOD / "services/sync_engine.py").read_text()
    assert '"season": _manifest_ordinal(row.get("season_number"))' in source
    assert '"episode": _manifest_ordinal(row.get("episode_number"))' in source


def test_tracks_table_stores_content_rating_and_show_plot():
    source = (ROCKPOD / "app/database.py").read_text()
    # content_rating was read by the manifest exporter long before it existed
    # as a column, so every synced row shipped an empty rating.
    assert "content_rating TEXT DEFAULT ''" in source
    assert "show_plot TEXT DEFAULT ''" in source
    assert "SCHEMA_VERSION = 15" in source
    assert "if current_version < 14:" in source


def test_avatar_has_verified_real_artwork_and_exact_episode_markers():
    import json

    catalog_path = ROCKPOD / "assets/imdb_video_artwork/catalog.json"
    payload = json.loads(catalog_path.read_text())
    avatar = next(
        item for item in payload["titles"]
        if item.get("imdb_id") == "tt0417299"
    )
    poster = catalog_path.parent / avatar["show_art"]
    assert poster.is_file()
    assert poster.read_bytes().startswith(b"\xff\xd8\xff")
    assert avatar["source_image"].startswith("https://")
    episodes = avatar["episodes"]
    expected_episodes = {
        *(f"1x{episode:02d}" for episode in range(1, 21)),
        *(f"2x{episode:02d}" for episode in range(1, 21)),
        *(f"3x{episode:02d}" for episode in range(1, 22)),
    }
    assert set(episodes) == expected_episodes
    assert episodes["1x01"]["intro_end"] == 46
    assert episodes["2x01"]["intro_end"] == 27
    assert episodes["2x20"]["credits_duration"] == 4
    assert all(item["intro_start"] < item["intro_end"]
               for item in episodes.values())
    assert all(item["credits_duration"] > 0
               for item in episodes.values())


def test_avatar_catalog_metadata_and_episode_markers_reach_sync():
    from services.video_thumbnails import VideoThumbnailService

    service = VideoThumbnailService.__new__(VideoThumbnailService)
    from services.imdb_video_artwork import IMDbVideoArtworkCatalog
    service._imdb_artwork = IMDbVideoArtworkCatalog()
    episode = {
        "video_kind": "show",
        "show_title": "Avatar The Last Airbender 2005 Seasons 1 to 3",
        "season_number": 1,
        "episode_number": 1,
        "duration": 23 * 60,
    }
    metadata = service.video_catalog_metadata(episode)
    markers = service.video_playback_markers(episode)

    assert metadata["imdb_id"] == "tt0417299"
    assert metadata["show_title"] == "Avatar: The Last Airbender"
    assert metadata["show_plot"]
    assert markers == {
        "intro_start": 2,
        "intro_end": 46,
        "credits_start": 23 * 60 - 20,
        "credits_duration": 20,
    }

    no_duration = dict(episode, duration=0)
    assert service.video_playback_markers(no_duration)["credits_duration"] == 20
    assert service.video_playback_markers(no_duration)["credits_start"] == 0

    finale = dict(episode, season_number=2, episode_number=20)
    assert service.video_playback_markers(finale)["credits_duration"] == 4

    extra = dict(episode, season_number=0)
    assert service.video_playback_markers(extra) == {
        "intro_start": 0,
        "intro_end": 0,
        "credits_start": 0,
        "credits_duration": 0,
    }


def test_unmarked_video_never_receives_a_timing_guess():
    from services.video_thumbnails import VideoThumbnailService
    from services.imdb_video_artwork import IMDbVideoArtworkCatalog

    service = VideoThumbnailService.__new__(VideoThumbnailService)
    service._imdb_artwork = IMDbVideoArtworkCatalog()
    markers = service.video_playback_markers({
        "video_kind": "movie",
        "title": "An Uncatalogued Movie",
        "duration": 7200,
    })
    assert markers == {
        "intro_start": 0,
        "intro_end": 0,
        "credits_start": 0,
        "credits_duration": 0,
    }


def test_skip_buttons_do_not_bypass_the_netflix_launch_logo():
    mpeg = (ROOT / "apps/plugins/mpegplayer/mpegplayer.c").read_text()
    raw = (ROOT / "apps/plugins/openh264_player.c").read_text()

    # Both players retain their pre-playback Netflix ident paths. Episode
    # markers are loaded separately and only become active once stream time or
    # decoded-frame position enters an intro/credits range.
    assert "stream_init(play_netflix_intro)" in mpeg
    assert "mpeg_netflix_load_markers(videofile);" in mpeg
    assert "netflix_intro_run(raw_pool, raw_pool_size);" in raw
    assert "raw_netflix_load_markers(path" in raw
    assert '"SKIP INTRO"' in mpeg and '"SKIP CREDITS"' in mpeg
    assert '"SKIP INTRO"' in raw and '"SKIP CREDITS"' in raw


def test_show_level_match_writes_a_description_to_every_episode():
    source = (ROCKPOD / "ui/main_window.py").read_text()
    tree = ast.parse(source)
    matched = None
    for node in ast.walk(tree):
        if isinstance(node, ast.FunctionDef) and node.name == "on_matched":
            matched = ast.get_source_segment(source, node)
            break
    assert matched is not None

    # The bulk show-level branch used to be a bare `pass`, which is why no
    # episode of a group-matched show had any plot on the device.
    assert 'updates["plot_short"] = show_plot_short' in matched
    assert 'updates["plot_long"] = show_plot_long' in matched
    assert 'updates["show_plot"]' in matched
    # content_rating is written for every match, group or single.
    assert '"content_rating": (' in matched


def test_metadata_search_covers_movies_and_shows_together():
    from services.online_video_metadata import VideoMetadataService

    class FakeProvider:
        def __init__(self):
            self.calls = []

        def search_movie(self, query, year=None):
            self.calls.append(("movie", query))
            return [{"title": "A Movie", "media_type": "movie"}]

        def search_show(self, query, year=None):
            self.calls.append(("show", query))
            return [{"title": "A Show", "media_type": "tv_show"}]

    service = VideoMetadataService.__new__(VideoMetadataService)
    provider = FakeProvider()
    service._provider = provider
    service._show_fallback = FakeProvider()
    service._config = None
    service._db = None

    results = service.search("thing", media_type="any")
    kinds = {item["media_type"] for item in results}
    assert kinds == {"movie", "tv_show"}

    # An explicit kind still narrows the search.
    provider.calls.clear()
    service.search("thing", media_type="movie")
    assert [kind for kind, _ in provider.calls] == ["movie"]


def test_metadata_search_reports_failure_when_every_catalogue_fails():
    from services.online_video_metadata import (
        VideoMetadataNetworkError,
        VideoMetadataService,
    )

    class DeadProvider:
        def search_movie(self, query, year=None):
            raise VideoMetadataNetworkError("offline")

        def search_show(self, query, year=None):
            raise VideoMetadataNetworkError("offline")

    service = VideoMetadataService.__new__(VideoMetadataService)
    service._provider = DeadProvider()
    service._show_fallback = DeadProvider()
    service._config = None
    service._db = None

    with pytest.raises(VideoMetadataNetworkError):
        service.search("thing", media_type="any")


def test_lookup_dialog_defaults_to_searching_every_type():
    source = (ROCKPOD / "ui/dialogs/video_metadata_lookup.py").read_text()
    assert 'self._type_combo.addItem("All", "any")' in source
    assert 'self._type_combo.addItem("Movie", "movie")' in source
    # The search kind must come from the selector, not the row's guessed kind.
    assert 'kind = str(self._type_combo.currentData() or "any")' in source


def test_a_movie_with_a_track_number_is_not_filed_as_a_tv_episode():
    from ui.video_library import classify_video_track

    movie = {
        "id": 1,
        "media_type": "video",
        "video_kind": "movie",
        "title": "Trailer Park Boys - The Movie",
        "track_number": 1,
        "file_path": "/media/Videos/Trailer Park Boys - The Movie.mp4",
    }
    assert classify_video_track(movie)["kind"] == "movie"


def test_a_real_episode_is_still_filed_as_a_show():
    from ui.video_library import classify_video_track

    episode = {
        "id": 2,
        "media_type": "video",
        "video_kind": "show",
        "title": "Pilot",
        "show_title": "My Name Is Earl",
        "season_number": 1,
        "episode_number": 1,
        "file_path": "/media/Videos/TV Shows/My Name Is Earl/Season 1/S01E01.mp4",
    }
    classified = classify_video_track(episode)
    assert classified["kind"] == "show"
    assert classified["label"] == "My Name Is Earl"


def test_an_unclassified_episode_is_still_recovered_from_its_path():
    from ui.video_library import classify_video_track

    episode = {
        "id": 3,
        "media_type": "video",
        "video_kind": "",
        "title": "Space Cadet",
        "file_path": "/media/Videos/TV Shows/Recess/Season 3/Space Cadet.mp4",
    }
    assert classify_video_track(episode)["kind"] == "show"
