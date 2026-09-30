from pathlib import Path
from datetime import datetime

from PIL import Image

from scripts import spotify_wrapped_artist_message as wrapped
from scripts.spotify_wrapped_artist_message import _select_candidate


def test_selects_wrapped_message_from_artist_channel():
    result = _select_candidate(
        [
            {
                "id": "abcdefghijk",
                "title": "CHVRCHES Spotify Wrapped Artist Message",
                "channel": "CHVRCHES",
                "duration": 29,
            },
            {
                "id": "lmnopqrstuv",
                "title": "Reacting to CHVRCHES Spotify Wrapped",
                "channel": "Music Reactions",
                "duration": 500,
            },
        ],
        "CHVRCHES",
    )
    assert result["id"] == "abcdefghijk"


def test_rejects_unofficial_or_generic_wrapped_results():
    result = _select_candidate(
        [
            {
                "id": "abcdefghijk",
                "title": "My Spotify Wrapped featuring CHVRCHES",
                "channel": "Random Listener",
                "duration": 90,
            },
            {
                "id": "lmnopqrstuv",
                "title": "CHVRCHES Wrapped compilation",
                "channel": "CHVRCHES Fans",
                "duration": 200,
            },
        ],
        "CHVRCHES",
    )
    assert result is None


def test_snapshot_uses_real_cover_and_complete_rankings(tmp_path, monkeypatch):
    mount = Path(tmp_path)
    album_dir = mount / "Music" / "Artist" / "Album"
    album_dir.mkdir(parents=True)
    Image.new("RGB", (320, 320), (12, 34, 56)).save(album_dir / "cover.jpg")
    tracks = [
        {
            "device_path": "Music/Artist/Album/01 - Song.mp3",
            "title": "Song",
            "artist": "Artist",
            "album": "Album",
            "genre": "Pop",
            "play_count": 9,
            "play_time": 1_234_567,
        },
        {
            "device_path": "Music/Other/Second/01 - Else.mp3",
            "title": "Else",
            "artist": "Other",
            "album": "Second",
            "genre": "Rock",
            "play_count": 2,
            "play_time": 345_678,
        },
    ]
    monkeypatch.setattr(wrapped, "read_rockbox_tagcache_tracks", lambda *_a, **_k: tracks)

    assert wrapped._write_snapshot(mount) == "Artist"
    data = (mount / ".rockbox/spotify-wrapped/data.tsv").read_text()
    assert "plays\t11" in data
    assert "seconds\t1580" in data
    assert "unique_artists\t2" in data
    assert "top_song_path\t/Music/Artist/Album/01 - Song.mp3" in data
    assert "song\tSong\tArtist\t9\t1234\t/.rockbox/spotify-wrapped/top-song.bmp" in data
    assert "artist\tArtist\t\t9\t1234\t/.rockbox/spotify-wrapped/top-artist.bmp" in data
    with Image.open(mount / ".rockbox/spotify-wrapped/top-song.bmp") as image:
        assert image.size == (104, 104)
        pixel = image.convert("RGB").getpixel((52, 52))
        assert all(abs(actual - expected) <= 2 for actual, expected in zip(pixel, (12, 34, 56)))


def test_snapshot_keeps_lifetime_totals_when_annual_logging_starts(tmp_path, monkeypatch):
    mount = Path(tmp_path)
    album_dir = mount / "Music" / "Artist" / "Album"
    album_dir.mkdir(parents=True)
    Image.new("RGB", (40, 40), (200, 20, 40)).save(album_dir / "cover.jpg")
    rockbox = mount / ".rockbox"
    rockbox.mkdir()
    path = "Music/Artist/Album/01 - Song.mp3"
    (rockbox / "playback.log").write_text(
        f"{int(datetime.now().timestamp())}:61000:180000:/{path}\n"
    )
    monkeypatch.setattr(
        wrapped,
        "read_rockbox_tagcache_tracks",
        lambda *_a, **_k: [{
            "device_path": path,
            "title": "Song",
            "artist": "Artist",
            "album": "Album",
            "genre": "Pop",
            "play_count": 9,
            "play_time": 1_234_567,
        }],
    )

    wrapped._write_snapshot(mount)
    data = (rockbox / "spotify-wrapped/data.tsv").read_text()
    assert "scope\tlifetime" in data
    assert "plays\t9" in data
    assert "seconds\t1234" in data
    assert "song\tSong\tArtist\t9\t1234" in data


def test_snapshot_survives_database_reset_and_removed_track(tmp_path, monkeypatch):
    tracks = [{"device_path": "Music/song.mp3", "title": "Song",
               "artist": "Artist", "play_count": 9, "play_time": 900000}]
    monkeypatch.setattr(wrapped, "read_rockbox_tagcache_tracks", lambda *_a, **_k: tracks)
    wrapped._write_snapshot(tmp_path)
    target = tmp_path / ".rockbox/spotify-wrapped/data.tsv"
    original = target.read_text()
    tracks.clear()
    wrapped._write_snapshot(tmp_path)
    assert target.read_text() == original
    tracks.append({"device_path": "Music/song.mp3", "title": "Song",
                   "artist": "Artist", "play_count": 0, "play_time": 0})
    wrapped._write_snapshot(tmp_path)
    assert target.read_text() == original


def test_damaged_history_preserves_published_snapshot(tmp_path, monkeypatch):
    import sqlite3
    import pytest

    monkeypatch.setattr(wrapped, "read_rockbox_tagcache_tracks", lambda *_a, **_k: [])
    directory = tmp_path / ".rockbox/spotify-wrapped"
    directory.mkdir(parents=True)
    (directory / "data.tsv").write_text("previous recap\n")
    (directory / "history.sqlite3").write_bytes(b"damaged")
    with pytest.raises(sqlite3.DatabaseError):
        wrapped._write_snapshot(tmp_path)
    assert (directory / "data.tsv").read_text() == "previous recap\n"
