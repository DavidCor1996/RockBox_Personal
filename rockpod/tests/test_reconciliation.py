"""Tests for reconciliation and diagnostic reports."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from services.reconciliation import build_missing_tag_report, format_missing_tag_report


class FakeDb:
    def __init__(self, rows):
        self._rows = rows

    def get_all_tracks(self, order_by=None):
        return list(self._rows)


def test_missing_tag_report_filters_to_flac_and_aiff():
    db = FakeDb(
        [
            {
                "file_path": "/music/good.flac",
                "codec": "FLAC",
                "title": "Song",
                "artist": "Artist",
                "album": "Album",
                "album_artist": "Artist",
                "genre": "Rock",
                "year": 2020,
                "track_number": 1,
                "track_total": 10,
                "disc_number": 1,
                "disc_total": 1,
                "composer": "Writer",
                "comment": "Note",
            },
            {
                "file_path": "/music/missing.aiff",
                "codec": "AIFF",
                "title": "Song",
                "artist": "Artist",
                "album": "Album",
                "album_artist": "",
                "genre": "",
                "year": None,
                "track_number": 2,
                "track_total": None,
                "disc_number": 1,
                "disc_total": None,
                "composer": "",
                "comment": "",
            },
            {
                "file_path": "/music/ignored.mp3",
                "codec": "MP3",
                "title": "",
                "artist": "",
                "album": "",
                "album_artist": "",
                "genre": "",
                "year": None,
                "track_number": None,
                "track_total": None,
                "disc_number": None,
                "disc_total": None,
                "composer": "",
                "comment": "",
            },
        ]
    )

    report = build_missing_tag_report(db)

    assert report["track_count"] == 2
    assert report["tracks_with_missing_tags_count"] == 1
    assert report["tracks_with_missing_tags_by_codec"] == {"AIFF": 1, "FLAC": 0}
    assert report["missing_field_counts"]["album_artist"] == 1
    assert report["missing_field_counts"]["genre"] == 1
    assert report["missing_field_counts"]["year"] == 1
    assert report["missing_field_counts"]["track_total"] == 1
    assert report["missing_field_counts"]["disc_total"] == 1
    assert report["missing_field_counts"]["composer"] == 1
    assert report["missing_field_counts"]["comment"] == 1
    assert report["tracks_with_missing_tags"][0]["file_path"] == "/music/missing.aiff"


def test_format_missing_tag_report_handles_clean_library():
    report = {
        "codecs": ["AIFF", "FLAC"],
        "track_count": 12,
        "tracks_with_missing_tags_count": 0,
        "tracks_with_missing_tags_by_codec": {"AIFF": 0, "FLAC": 0},
        "missing_field_counts": {
            "title": 0,
            "artist": 0,
            "album": 0,
            "album_artist": 0,
            "genre": 0,
            "year": 0,
            "track_number": 0,
            "track_total": 0,
            "disc_number": 0,
            "disc_total": 0,
            "composer": 0,
            "comment": 0,
        },
        "tracks_with_missing_tags": [],
    }

    text = format_missing_tag_report(report)

    assert "Tracks scanned: 12" in text
    assert "Tracks with missing tags: 0" in text
    assert "No missing FLAC/AIFF tags were found." in text
