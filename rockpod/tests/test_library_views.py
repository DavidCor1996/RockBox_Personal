"""Tests for library browsing helper logic."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from ui.library_views import (
    album_group_diagnostics,
    album_display_label,
    filter_tracks_for_search,
    group_tracks_by_album,
    group_tracks_by_artist,
    group_tracks_by_genre,
    summarize_tracks,
)
from ui.status_bar import format_status_summary


def _track(title, artist="Artist", album="Album", genre="Rock", duration=60, size=1024):
    return {
        "title": title,
        "artist": artist,
        "album": album,
        "album_artist": "",
        "genre": genre,
        "duration": duration,
        "file_size": size,
    }


def test_album_grouping_sorts_by_artist_album_and_track_order():
    tracks = [
        {**_track("Second", "B Artist", "B Album"), "track_number": 2},
        {**_track("First", "B Artist", "B Album"), "track_number": 1},
        _track("Other", "A Artist", "A Album"),
    ]

    albums = group_tracks_by_album(tracks)

    assert [album["album"] for album in albums] == ["A Album", "B Album"]
    assert [track["title"] for track in albums[1]["tracks"]] == ["First", "Second"]


def test_album_grouping_merges_featured_artist_tracks():
    tracks = [
        {**_track("One", "Artist", "Album"), "track_number": 1},
        {**_track("Two", "Artist feat. Guest", "Album"), "track_number": 2},
    ]

    albums = group_tracks_by_album(tracks)

    assert len(albums) == 1
    assert [track["title"] for track in albums[0]["tracks"]] == ["One", "Two"]
    assert albums[0]["artist"] == "Artist"


def test_album_artist_overrides_track_artist_for_grouping():
    tracks = [
        {**_track("One", "Singer A", "Album"), "album_artist": "Band", "track_number": 1},
        {**_track("Two", "Singer B feat. Guest", "Album"), "album_artist": "Band", "track_number": 2},
    ]

    albums = group_tracks_by_album(tracks)

    assert len(albums) == 1
    assert albums[0]["artist"] == "Band"


def test_self_titled_album_label_makes_artist_explicit():
    assert album_display_label("Paramore", "Paramore") == "Paramore\nby Paramore"

    albums = group_tracks_by_album([
        {**_track("Fast in My Car", "Paramore", "Paramore"), "album_artist": "Paramore", "track_number": 1},
        {**_track("Now", "Paramore", "Paramore"), "album_artist": "Paramore", "track_number": 2},
    ])

    assert albums[0]["label"] == "Paramore\nby Paramore"


def test_compilation_album_stays_grouped():
    tracks = [
        {**_track("One", "Artist A", "Hits"), "compilation": 1},
        {**_track("Two", "Artist B feat. Guest", "Hits"), "compilation": 1},
    ]

    albums = group_tracks_by_album(tracks)

    assert len(albums) == 1
    assert albums[0]["artist"] == "Various Artists"


def test_album_group_diagnostics_include_group_key_and_source_artists():
    tracks = [
        _track("One", "Artist", "Album"),
        _track("Two", "Artist feat. Guest", "Album"),
    ]

    diagnostics = album_group_diagnostics(group_tracks_by_album(tracks))

    assert len(diagnostics) == 1
    assert diagnostics[0]["group_key"]
    assert diagnostics[0]["source_artists"] == ["Artist", "Artist feat. Guest"]


def test_album_grouping_merges_collaborator_artist_variants():
    tracks = [
        _track("One", "Artist", "Album"),
        _track("Two", "Artist & Guest", "Album"),
        _track("Three", "Artist, Guest", "Album"),
    ]

    albums = group_tracks_by_album(tracks)

    assert len(albums) == 1
    assert [track["title"] for track in albums[0]["tracks"]] == ["One", "Three", "Two"]


def test_artist_and_genre_grouping_use_unknown_fallbacks():
    groups = group_tracks_by_artist([{"title": "No Artist"}])
    genres = group_tracks_by_genre([{"title": "No Genre"}])

    assert "Unknown Artist" in groups
    assert "Unknown Genre" in genres


def test_filter_tracks_for_search_matches_current_rows_only():
    tracks = [
        _track("Airbag", "Radiohead", "OK Computer", "Rock"),
        _track("Breathe", "Pink Floyd", "Dark Side", "Prog"),
    ]

    results = filter_tracks_for_search(tracks, "radio")

    assert [track["title"] for track in results] == ["Airbag"]


def test_summarize_tracks_and_status_formatting():
    summary = summarize_tracks([
        _track("One", duration=60, size=1024 * 1024),
        _track("Two", duration=120, size=2 * 1024 * 1024),
    ])

    assert summary == {"count": 2, "duration": 180, "size": 3 * 1024 * 1024}
    assert format_status_summary(2, 180, 3 * 1024 * 1024) == "2 songs, 3 minutes, 3.0 MB"
    assert format_status_summary(1, 180, 3 * 1024 * 1024, item_label="videos") == "1 video, 3 minutes, 3.0 MB"
