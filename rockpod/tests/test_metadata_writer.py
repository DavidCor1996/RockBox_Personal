"""Tests for writing metadata back to media files."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from mutagen.id3 import ID3

from services.metadata_writer import MetadataWriteError, write_track_metadata_to_file


class _FakeAudio:
    def __init__(self, tags=None):
        self.tags = tags
        self.saved = False

    def add_tags(self):
        if self.tags is None:
            self.tags = ID3()

    def save(self):
        self.saved = True


def test_write_track_metadata_to_file_updates_id3_frames():
    audio = _FakeAudio(ID3())

    write_track_metadata_to_file(
        "/music/test.mp3",
        {
            "title": "Song",
            "artist": "Artist",
            "album": "Album",
            "album_artist": "Album Artist",
            "genre": "Rock",
            "year": 2026,
            "track_number": 2,
            "track_total": 10,
            "disc_number": 1,
            "disc_total": 2,
            "composer": "Writer",
            "comment": "Note",
            "compilation": 1,
        },
        mutagen_file_func=lambda _path: audio,
    )

    assert audio.saved is True
    assert audio.tags.getall("TIT2")[0].text[0] == "Song"
    assert audio.tags.getall("TPE1")[0].text[0] == "Artist"
    assert audio.tags.getall("TALB")[0].text[0] == "Album"
    assert audio.tags.getall("TPE2")[0].text[0] == "Album Artist"
    assert audio.tags.getall("TCON")[0].text[0] == "Rock"
    assert str(audio.tags.getall("TDRC")[0].text[0]) == "2026"
    assert audio.tags.getall("TRCK")[0].text[0] == "2/10"
    assert audio.tags.getall("TPOS")[0].text[0] == "1/2"
    assert audio.tags.getall("TCOM")[0].text[0] == "Writer"
    assert audio.tags.getall("COMM")[0].text[0] == "Note"
    assert audio.tags.getall("TCMP")[0].text[0] == "1"


def test_write_track_metadata_to_file_updates_mp4_shapes():
    audio = _FakeAudio({})

    write_track_metadata_to_file(
        "/videos/show.m4v",
        {
            "title": "Pilot",
            "artist": "Show Creator",
            "album": "Season 1",
            "album_artist": "The Show",
            "track_number": 2,
            "track_total": 8,
            "disc_number": 1,
            "disc_total": 1,
            "show_title": "The Show",
            "season_number": 1,
            "episode_number": 2,
            "compilation": 0,
        },
        mutagen_file_func=lambda _path: audio,
    )

    assert audio.saved is True
    assert audio.tags["\xa9nam"] == ["Pilot"]
    assert audio.tags["\xa9ART"] == ["Show Creator"]
    assert audio.tags["\xa9alb"] == ["Season 1"]
    assert audio.tags["aART"] == ["The Show"]
    assert audio.tags["trkn"] == [(2, 8)]
    assert audio.tags["disk"] == [(1, 1)]
    assert audio.tags["tvsh"] == ["The Show"]
    assert audio.tags["tvsn"] == [1]
    assert audio.tags["tves"] == [2]
    assert "cpil" not in audio.tags


def test_write_track_metadata_to_file_only_changes_requested_mp4_fields():
    audio = _FakeAudio(
        {
            "\xa9nam": ["Custom Title"],
            "\xa9ART": ["Artist"],
            "\xa9alb": ["Album"],
            "\xa9gen": ["Rock"],
            "trkn": [(2, 8)],
            "disk": [(1, 2)],
            "tvsh": ["The Show"],
            "tvsn": [1],
            "tves": [2],
        }
    )

    write_track_metadata_to_file(
        "/videos/show.m4v",
        {"comment": "Updated", "track_number": 3},
        mutagen_file_func=lambda _path: audio,
    )

    assert audio.tags["\xa9cmt"] == ["Updated"]
    assert audio.tags["\xa9nam"] == ["Custom Title"]
    assert audio.tags["\xa9ART"] == ["Artist"]
    assert audio.tags["\xa9alb"] == ["Album"]
    assert audio.tags["\xa9gen"] == ["Rock"]
    assert audio.tags["trkn"] == [(3, 8)]
    assert audio.tags["disk"] == [(1, 2)]
    assert audio.tags["tvsh"] == ["The Show"]
    assert audio.tags["tvsn"] == [1]
    assert audio.tags["tves"] == [2]


def test_write_track_metadata_to_file_rejects_unsupported_container():
    try:
        write_track_metadata_to_file(
            "/music/test.wav",
            {"title": "Song"},
            mutagen_file_func=lambda _path: _FakeAudio({}),
        )
    except MetadataWriteError as exc:
        assert "not supported yet" in str(exc)
    else:
        raise AssertionError("Expected MetadataWriteError")
