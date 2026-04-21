"""Tests for local playback queue and state behavior."""

import os

from PySide6.QtCore import QObject, Signal

from services.playback import PlaybackService, STATE_PAUSED, STATE_PLAYING, STATE_STOPPED


class FakeBackend(QObject):
    position_changed = Signal(int)
    duration_changed = Signal(int)
    state_changed = Signal(str)
    media_finished = Signal()
    error = Signal(str)

    def __init__(self):
        super().__init__()
        self.media = ""
        self.position = 0
        self.volume = 70
        self.play_calls = 0
        self.pause_calls = 0
        self.stop_calls = 0

    def set_media(self, path):
        self.media = path

    def play(self):
        self.play_calls += 1
        self.state_changed.emit(STATE_PLAYING)

    def pause(self):
        self.pause_calls += 1
        self.state_changed.emit(STATE_PAUSED)

    def stop(self):
        self.stop_calls += 1
        self.state_changed.emit(STATE_STOPPED)

    def set_position(self, position_ms):
        self.position = position_ms
        self.position_changed.emit(position_ms)

    def set_volume(self, volume):
        self.volume = volume


def _track(tmp_dir, tid, title):
    path = os.path.join(tmp_dir, f"{title}.mp3")
    with open(path, "wb") as f:
        f.write(b"audio")
    return {
        "id": tid,
        "title": title,
        "artist": "Artist",
        "album": "Album",
        "duration": 120.0,
        "file_path": path,
    }


def test_play_pause_stop_state_transitions(tmp_dir):
    backend = FakeBackend()
    service = PlaybackService(backend)
    track = _track(tmp_dir, 1, "One")

    assert service.play_track(track, [track], "library_music") is True
    assert service.state == STATE_PLAYING
    assert backend.media == track["file_path"]

    service.pause()
    assert service.state == STATE_PAUSED
    assert backend.pause_calls == 1

    service.play()
    assert service.state == STATE_PLAYING

    service.stop()
    assert service.state == STATE_STOPPED


def test_next_previous_stay_inside_queue(tmp_dir):
    backend = FakeBackend()
    service = PlaybackService(backend)
    tracks = [_track(tmp_dir, 1, "One"), _track(tmp_dir, 2, "Two")]

    service.play_track(tracks[0], tracks, "playlist_1")
    assert service.current_track["id"] == 1
    assert service.queue_source == "playlist_1"

    service.next()
    assert service.current_track["id"] == 2

    service.previous()
    assert service.current_track["id"] == 1


def test_seek_and_volume_controls(tmp_dir):
    backend = FakeBackend()
    service = PlaybackService(backend)
    track = _track(tmp_dir, 1, "One")

    service.play_track(track, [track], "library_music")
    service.seek(30_000)
    service.set_volume(33)

    assert service.position == 30_000
    assert backend.position == 30_000
    assert service.volume == 33
    assert backend.volume == 33


def test_unsupported_missing_file_reports_error():
    backend = FakeBackend()
    service = PlaybackService(backend)
    errors = []
    service.error.connect(errors.append)

    ok = service.play_track(
        {"id": 1, "title": "Missing", "file_path": "/no/such/file.mp3"},
        [{"id": 1, "title": "Missing", "file_path": "/no/such/file.mp3"}],
        "library_music",
    )

    assert ok is False
    assert service.state == STATE_STOPPED
    assert errors
    assert "Cannot play missing local file" in errors[0]


def test_removing_current_playlist_track_stops_without_crash(tmp_dir):
    backend = FakeBackend()
    service = PlaybackService(backend)
    tracks = [_track(tmp_dir, 1, "One"), _track(tmp_dir, 2, "Two")]

    service.play_track(tracks[0], tracks, "playlist_1")
    service.remove_tracks_from_queue({1})

    assert service.state == STATE_STOPPED
    assert service.current_track is None


def test_media_finished_advances_queue(tmp_dir):
    backend = FakeBackend()
    service = PlaybackService(backend)
    tracks = [_track(tmp_dir, 1, "One"), _track(tmp_dir, 2, "Two")]

    service.play_track(tracks[0], tracks, "library_music")
    backend.media_finished.emit()

    assert service.current_track["id"] == 2
