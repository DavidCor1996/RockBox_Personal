"""Local music playback service with a swappable backend."""

import os
from urllib.parse import urlparse

from PySide6.QtCore import QObject, QUrl, Signal, Slot


STATE_STOPPED = "stopped"
STATE_PLAYING = "playing"
STATE_PAUSED = "paused"


class NullPlaybackBackend(QObject):
    """Fallback backend used when QtMultimedia is unavailable."""

    position_changed = Signal(int)
    duration_changed = Signal(int)
    state_changed = Signal(str)
    media_finished = Signal()
    error = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._volume = 70

    def set_media(self, _path):
        self.error.emit("Audio playback backend is unavailable")

    def set_video_output(self, _output):
        pass

    def play(self):
        self.error.emit("Audio playback backend is unavailable")

    def pause(self):
        pass

    def stop(self):
        self.state_changed.emit(STATE_STOPPED)

    def set_position(self, position_ms):
        self.position_changed.emit(position_ms)

    def set_volume(self, volume):
        self._volume = max(0, min(100, int(volume)))


class QtMediaPlaybackBackend(QObject):
    """QtMultimedia-backed local audio playback."""

    position_changed = Signal(int)
    duration_changed = Signal(int)
    state_changed = Signal(str)
    media_finished = Signal()
    error = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        try:
            from PySide6.QtMultimedia import QAudioOutput, QMediaPlayer
        except ImportError:
            self._player = None
            self._audio = None
            return

        self._player = QMediaPlayer(self)
        self._audio = QAudioOutput(self)
        self._player.setAudioOutput(self._audio)
        self._audio.setVolume(0.70)

        try:
            self._player.positionChanged.connect(self._emit_position)
            self._player.durationChanged.connect(self._emit_duration)
            self._player.playbackStateChanged.connect(self._on_state_changed)
            self._player.mediaStatusChanged.connect(self._on_media_status)
            self._player.errorOccurred.connect(self._on_error)
        except RuntimeError:
            self._player = None
            self._audio = None

    @property
    def is_available(self):
        return self._player is not None

    def set_media(self, path):
        if not self._player:
            self.error.emit("Audio playback backend is unavailable")
            return
        if _is_remote_media_source(path):
            self._player.setSource(QUrl(path))
        else:
            self._player.setSource(QUrl.fromLocalFile(path))

    def set_video_output(self, output):
        if self._player:
            self._player.setVideoOutput(output)

    def play(self):
        if not self._player:
            self.error.emit("Audio playback backend is unavailable")
            return
        self._player.play()

    def pause(self):
        if self._player:
            self._player.pause()

    def stop(self):
        if self._player:
            self._player.stop()
        self.state_changed.emit(STATE_STOPPED)

    def set_position(self, position_ms):
        if self._player:
            self._player.setPosition(max(0, int(position_ms)))

    def set_volume(self, volume):
        if self._audio:
            self._audio.setVolume(max(0, min(100, int(volume))) / 100.0)

    @Slot(int)
    def _emit_position(self, position):
        self.position_changed.emit(position)

    @Slot(int)
    def _emit_duration(self, duration):
        self.duration_changed.emit(duration)

    @Slot(object)
    def _on_state_changed(self, state):
        if not self._player:
            return
        from PySide6.QtMultimedia import QMediaPlayer

        if state == QMediaPlayer.PlayingState:
            self.state_changed.emit(STATE_PLAYING)
        elif state == QMediaPlayer.PausedState:
            self.state_changed.emit(STATE_PAUSED)
        else:
            self.state_changed.emit(STATE_STOPPED)

    @Slot(object)
    def _on_media_status(self, status):
        if not self._player:
            return
        from PySide6.QtMultimedia import QMediaPlayer

        if status == QMediaPlayer.EndOfMedia:
            self.media_finished.emit()

    @Slot(object, str)
    def _on_error(self, _error, message):
        self.error.emit(message or "Unable to play this file")


class PlaybackService(QObject):
    """Playback coordinator with queue/context state."""

    state_changed = Signal(str)
    track_changed = Signal(object)
    position_changed = Signal(int, int)
    volume_changed = Signal(int)
    error = Signal(str)

    def __init__(self, backend=None, parent=None):
        super().__init__(parent)
        self._backend = backend or self._default_backend()
        self._queue = []
        self._queue_source = ""
        self._index = -1
        self._state = STATE_STOPPED
        self._position = 0
        self._duration = 0
        self._volume = 70
        self._connect_backend()
        self._backend.set_volume(self._volume)

    @property
    def state(self):
        return self._state

    @property
    def current_track(self):
        if 0 <= self._index < len(self._queue):
            return self._queue[self._index]
        return None

    @property
    def queue_source(self):
        return self._queue_source

    @property
    def current_index(self):
        return self._index

    @property
    def queue(self):
        return list(self._queue)

    @property
    def position(self):
        return self._position

    @property
    def duration(self):
        return self._duration

    @property
    def volume(self):
        return self._volume

    def set_queue(self, source_type, tracks, index=0):
        self._queue_source = source_type or ""
        self._queue = [
            t for t in _normalize_tracks(tracks)
            if _track_media_source(t)
        ]
        self._index = max(0, min(int(index or 0), len(self._queue) - 1)) if self._queue else -1

    def set_video_output(self, output):
        if hasattr(self._backend, "set_video_output"):
            self._backend.set_video_output(output)

    def play_track(self, track, queue_tracks=None, source_type=""):
        track = _normalize_track(track)
        if queue_tracks is None:
            queue_tracks = [track]
        queue = _normalize_tracks(queue_tracks)
        target_index = self._find_track_index(queue, track)
        self.set_queue(source_type, queue, target_index)
        return self.play_index(self._index)

    def play_index(self, index):
        if not self._queue:
            self._set_state(STATE_STOPPED)
            return False
        if index < 0 or index >= len(self._queue):
            return False
        self._index = index
        track = self._queue[self._index]
        source = _track_media_source(track)
        if not source:
            self._on_error("Cannot play missing media source")
            return False
        if not _is_remote_media_source(source) and not os.path.isfile(source):
            self._on_error(f"Cannot play missing local file: {source or 'unknown file'}")
            return False
        self._duration = int(float(track.get("duration", 0) or 0) * 1000)
        self._position = 0
        self.track_changed.emit(track)
        self.position_changed.emit(self._position, self._duration)
        self._backend.set_media(source)
        self._backend.play()
        self._set_state(STATE_PLAYING)
        return True

    def play(self):
        if self._state == STATE_PAUSED:
            self._backend.play()
            self._set_state(STATE_PLAYING)
            return True
        if self.current_track is not None:
            return self.play_index(self._index)
        return False

    def pause(self):
        if self._state == STATE_PLAYING:
            self._backend.pause()
            self._set_state(STATE_PAUSED)

    def toggle_play_pause(self):
        if self._state == STATE_PLAYING:
            self.pause()
        else:
            self.play()

    def stop(self):
        self._backend.stop()
        self._position = 0
        self.position_changed.emit(self._position, self._duration)
        self._set_state(STATE_STOPPED)

    def next(self):
        if self._index + 1 < len(self._queue):
            return self.play_index(self._index + 1)
        self.stop()
        return False

    def previous(self):
        if self._index > 0:
            return self.play_index(self._index - 1)
        if self.current_track is not None:
            return self.play_index(0)
        return False

    def seek(self, position_ms):
        self._position = max(0, int(position_ms))
        self._backend.set_position(self._position)
        self.position_changed.emit(self._position, self._duration)

    def set_volume(self, volume):
        self._volume = max(0, min(100, int(volume)))
        self._backend.set_volume(self._volume)
        self.volume_changed.emit(self._volume)

    def remove_tracks_from_queue(self, track_ids):
        ids = set(track_ids or [])
        if not ids or not self._queue:
            return
        current = self.current_track
        current_id = current.get("id") if current else None
        self._queue = [t for t in self._queue if t.get("id") not in ids]
        if current_id in ids:
            self.stop()
            self._index = -1
            self.track_changed.emit({})
        elif current_id:
            self._index = self._find_track_index_by_id(self._queue, current_id)

    @Slot(int)
    def _on_backend_position(self, position):
        self._position = int(position or 0)
        self.position_changed.emit(self._position, self._duration)

    @Slot(int)
    def _on_backend_duration(self, duration):
        self._duration = int(duration or 0)
        self.position_changed.emit(self._position, self._duration)

    @Slot(str)
    def _on_backend_state(self, state):
        self._set_state(state)

    @Slot()
    def _on_media_finished(self):
        self.next()

    @Slot(str)
    def _on_error(self, message):
        self._set_state(STATE_STOPPED)
        self.error.emit(message)

    def _connect_backend(self):
        self._backend.position_changed.connect(self._on_backend_position)
        self._backend.duration_changed.connect(self._on_backend_duration)
        self._backend.state_changed.connect(self._on_backend_state)
        self._backend.media_finished.connect(self._on_media_finished)
        self._backend.error.connect(self._on_error)

    def _set_state(self, state):
        if state == self._state:
            return
        self._state = state
        self.state_changed.emit(state)

    def _default_backend(self):
        if os.environ.get("QT_QPA_PLATFORM") == "offscreen":
            return NullPlaybackBackend(self)
        backend = QtMediaPlaybackBackend(self)
        if getattr(backend, "is_available", False):
            return backend
        backend.deleteLater()
        return NullPlaybackBackend(self)

    def _find_track_index(self, tracks, track):
        tid = track.get("id")
        if tid:
            idx = self._find_track_index_by_id(tracks, tid)
            if idx >= 0:
                return idx
        fp = track.get("file_path", "")
        source = _track_media_source(track)
        for idx, row in enumerate(tracks):
            if row.get("file_path") == fp or _track_media_source(row) == source:
                return idx
        return 0

    def _find_track_index_by_id(self, tracks, track_id):
        for idx, row in enumerate(tracks):
            if row.get("id") == track_id:
                return idx
        return -1


def _normalize_track(track):
    if track is None:
        return {}
    if hasattr(track, "keys"):
        return dict(track)
    if hasattr(track, "__dict__"):
        return dict(track.__dict__)
    return {}


def _normalize_tracks(tracks):
    return [_normalize_track(track) for track in (tracks or [])]


def _is_remote_media_source(value):
    parsed = urlparse(str(value or "").strip())
    return parsed.scheme in {"http", "https"}


def _track_media_source(track):
    item = _normalize_track(track)
    for key in ("file_path", "stream_url", "preview_url"):
        value = str(item.get(key) or "").strip()
        if value:
            return value
    return ""
