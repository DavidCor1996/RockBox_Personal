"""Tests for video browser grouping."""

import os
import sys

from PySide6.QtWidgets import QApplication

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from ui.video_library import VideoGridView, build_video_browser_groups, classify_video_track


def test_classify_show_episode_from_series_metadata():
    track = {
        "id": 1,
        "title": "Pilot",
        "show_title": "The Show",
        "season_number": 1,
        "episode_number": 2,
        "video_kind": "show",
    }

    info = classify_video_track(track)

    assert info["kind"] == "show"
    assert info["label"] == "The Show"
    assert info["episode_label"] == "S01E02 - Pilot"


def test_classify_home_video_from_path_keywords():
    track = {
        "id": 2,
        "title": "Clip 0001",
        "file_path": "/Videos/Home Videos/Birthday/clip_0001.mp4",
    }

    info = classify_video_track(track)

    assert info["kind"] == "home_video"


def test_classify_music_video_as_its_own_type():
    track = {
        "id": 3,
        "title": "Live Performance",
        "artist": "The Band",
        "video_kind": "music_video",
        "show_title": "Stale Show Metadata",
    }

    info = classify_video_track(track)

    assert info["kind"] == "music_video"
    assert info["label"] == "Live Performance"
    assert info["sub_label"] == "The Band"


def test_classify_concert_as_its_own_type():
    info = classify_video_track(
        {"id": 4, "title": "Live at Red Rocks", "artist": "The Band",
         "video_kind": "concert"}
    )

    assert info["kind"] == "concert"
    assert info["sub_label"] == "The Band"


def test_build_video_browser_groups_collapses_show_to_one_group():
    tracks = [
        {"id": 1, "title": "Pilot", "show_title": "The Show", "season_number": 1, "episode_number": 1, "video_kind": "show"},
        {"id": 2, "title": "Episode Two", "show_title": "The Show", "season_number": 1, "episode_number": 2, "video_kind": "show"},
        {"id": 3, "title": "Movie", "artist": "Director", "album": "Movie", "file_path": "/Videos/Movies/Movie.mp4"},
    ]

    grouped = build_video_browser_groups(tracks)

    assert len(grouped["show"]) == 1
    only_show = next(iter(grouped["show"].values()))
    assert only_show["label"] == "The Show"
    assert [track["title"] for track in only_show["tracks"]] == ["Pilot", "Episode Two"]
    assert len(grouped["movie"]) == 1


def test_build_video_browser_groups_keeps_music_videos_separate():
    grouped = build_video_browser_groups(
        [
            {"id": 1, "title": "Movie", "video_kind": "movie"},
            {"id": 2, "title": "Performance", "video_kind": "music_video"},
        ]
    )

    assert [track["title"] for track in grouped["movie"]] == ["Movie"]
    assert [track["title"] for track in grouped["music_video"]] == ["Performance"]


def test_build_video_browser_groups_keeps_concerts_separate():
    grouped = build_video_browser_groups(
        [{"id": 1, "title": "Movie", "video_kind": "movie"},
         {"id": 2, "title": "Live Set", "video_kind": "concert"}]
    )

    assert [track["title"] for track in grouped["concert"]] == ["Live Set"]


def test_video_grid_view_shows_one_cover_per_show_until_opened():
    app = QApplication.instance() or QApplication([])

    class FakeThumbs:
        def thumbnail_path(self, track, size=232):
            return ""

    view = VideoGridView(FakeThumbs())
    view.set_tracks([
        {"id": 1, "title": "Pilot", "show_title": "The Show", "season_number": 1, "episode_number": 1, "video_kind": "show"},
        {"id": 2, "title": "Episode Two", "show_title": "The Show", "season_number": 1, "episode_number": 2, "video_kind": "show"},
        {"id": 3, "title": "Movie", "video_kind": "movie"},
    ])

    assert view._grids["show"].count() == 1
    assert view._grids["movie"].count() == 1

    view._show_scope_key = "show:the show"
    view._refresh_tabs()

    assert view._grids["show"].count() == 2


def test_specials_group_with_show_in_browser_groups():
    tracks = [
        {"id": 1, "title": "Pilot", "show_title": "Death Note", "season_number": 1, "episode_number": 1, "video_kind": "show"},
        {"id": 2, "title": "Special 1", "show_title": "Death Note", "album": "Specials", "season_number": 0, "episode_number": 1, "video_kind": "show"},
    ]

    grouped = build_video_browser_groups(tracks)

    assert len(grouped["show"]) == 1
    only_show = next(iter(grouped["show"].values()))
    assert only_show["label"] == "Death Note"


def test_specials_placeholder_show_title_groups_under_real_show():
    tracks = [
        {
            "id": 1,
            "title": "Special 1",
            "show_title": "Specials",
            "artist": "Death Note (2006)",
            "album": "Specials",
            "video_kind": "show",
            "file_path": "/Videos/TV Shows/Death Note (2006)/Specials/01 - Special.mkv",
        },
        {
            "id": 2,
            "title": "Episode 1",
            "show_title": "Death Note (2006)",
            "album": "Season 1",
            "video_kind": "show",
            "file_path": "/Videos/TV Shows/Death Note (2006)/Season 1/01 - Episode.mkv",
        },
    ]

    grouped = build_video_browser_groups(tracks)

    assert len(grouped["show"]) == 1
    only_show = next(iter(grouped["show"].values()))
    assert only_show["label"] == "Death Note (2006)"


def test_noisy_complete_series_show_name_is_normalized():
    track = {
        "id": 1,
        "title": "Episode 1",
        "show_title": "6teen 2004 complete series 202508",
        "video_kind": "show",
        "file_path": "/Videos/TV Shows/6teen (2004)/Season 1/01 - Episode.mkv",
    }

    info = classify_video_track(track)

    assert info["kind"] == "show"
    assert info["label"] == "6teen 2004"
