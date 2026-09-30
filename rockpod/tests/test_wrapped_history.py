import sqlite3

import pytest

from services.wrapped_history import lifetime_history


def track(plays=9, milliseconds=900000):
    return dict(device_path="Music/song.mp3", title="Song", artist="Artist",
                play_count=plays, play_time=milliseconds)


def log(mount, timestamp=100, elapsed=30000, length=180000):
    path = mount / ".rockbox/playback.log"
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a") as stream:
        stream.write(f"{timestamp}:{elapsed}:{length}:/Music/song.mp3\n")


def test_retains_history_after_reset_removal_and_log_rotation(tmp_path):
    log(tmp_path)
    assert lifetime_history(tmp_path, [track()])[0]["play_count"] == 9
    assert lifetime_history(tmp_path, [track()])[0]["play_count"] == 9
    original = tmp_path / ".rockbox/playback.log"
    original.rename(original.with_name("playback_0020.log"))
    log(tmp_path, 101)
    restored = lifetime_history(tmp_path, [track(0, 0)])[0]
    assert restored["play_count"] == 10
    assert restored["play_time"] == 930000
    for path in (tmp_path / ".rockbox").glob("playback*.log"):
        path.unlink()
    assert lifetime_history(tmp_path, [])[0] == restored
    log(tmp_path, 102)
    assert lifetime_history(tmp_path, [track(1, 30000)])[0]["play_count"] == 11


def test_duplicate_rotated_log_does_not_double_count(tmp_path):
    log(tmp_path)
    source = tmp_path / ".rockbox/playback.log"
    source.with_name("playback_0001.log").write_bytes(source.read_bytes())
    assert lifetime_history(tmp_path, [track(0, 0)])[0]["play_count"] == 1


def test_runtime_update_before_log_flush_does_not_double_count(tmp_path):
    lifetime_history(tmp_path, [track()])
    assert lifetime_history(tmp_path, [track(10, 930000)])[0]["play_count"] == 10
    log(tmp_path)
    assert lifetime_history(tmp_path, [track(10, 930000)])[0]["play_count"] == 10


def test_short_complete_tracks_and_exact_threshold_count(tmp_path):
    log(tmp_path, 100, 30000)
    log(tmp_path, 101, 12000, 12000)
    log(tmp_path, 102, 11000, 12000)
    source = tmp_path / ".rockbox/playback.log"
    with source.open("a") as stream:
        stream.write("invalid\n9999999999999999999999999999999:30000:40000:/Music/song.mp3\n")
        stream.write("103:30000:40000:/Music/song.mp3")
    assert lifetime_history(tmp_path, [track(0, 0)])[0]["play_count"] == 2


def test_corrupt_archive_fails_without_replacing_it(tmp_path):
    directory = tmp_path / ".rockbox/spotify-wrapped"
    directory.mkdir(parents=True)
    archive = directory / "history.sqlite3"
    archive.write_bytes(b"broken archive")
    with pytest.raises(sqlite3.DatabaseError):
        lifetime_history(tmp_path, [track()])
    assert archive.read_bytes() == b"broken archive"
