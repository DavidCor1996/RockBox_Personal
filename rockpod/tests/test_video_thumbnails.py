"""Tests for video thumbnail caching."""

import os
import sys

from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from services.video_thumbnails import VideoThumbnailService


def test_thumbnail_service_caches_rendered_result(tmp_dir, monkeypatch):
    video_path = os.path.join(tmp_dir, "clip.mkv")
    with open(video_path, "wb") as f:
        f.write(b"video")

    service = VideoThumbnailService(tmp_dir)
    service._ffmpeg = "/usr/bin/ffmpeg"
    calls = []

    def fake_run(command, check, capture_output, text):
        calls.append(command)
        target = command[-1]
        Image.new("RGB", (320, 180), color=(120, 140, 180)).save(target, "JPEG")

        class Result:
            returncode = 0

        return Result()

    monkeypatch.setattr("services.video_thumbnails.subprocess.run", fake_run)

    track = {"file_path": video_path, "duration": 120.0}
    first = service.thumbnail_path(track, size=100)
    second = service.thumbnail_path(track, size=100)

    assert os.path.exists(first)
    assert first == second
    assert len(calls) == 1
    with Image.open(first) as rendered:
        assert rendered.size == (100, 150)


def test_thumbnail_service_prefers_local_poster_file(tmp_dir, monkeypatch):
    show_dir = os.path.join(tmp_dir, "The Show")
    season_dir = os.path.join(show_dir, "Season 1")
    os.makedirs(season_dir, exist_ok=True)
    video_path = os.path.join(season_dir, "Episode 1.mkv")
    with open(video_path, "wb") as f:
        f.write(b"video")
    poster_path = os.path.join(show_dir, "poster.jpg")
    Image.new("RGB", (600, 900), color=(50, 80, 120)).save(poster_path, "JPEG")

    service = VideoThumbnailService(tmp_dir)
    service._ffmpeg = "/usr/bin/ffmpeg"

    def fail_run(*args, **kwargs):
        raise AssertionError("ffmpeg should not run when a poster file exists")

    monkeypatch.setattr("services.video_thumbnails.subprocess.run", fail_run)

    thumb = service.thumbnail_path(
        {
            "file_path": video_path,
            "show_title": "The Show",
            "season_number": 1,
            "_video_scope": "show",
        },
        size=120,
    )

    assert os.path.exists(thumb)
    with Image.open(thumb) as rendered:
        assert rendered.size == (120, 180)


def test_thumbnail_service_prefers_online_poster_from_artwork_manager(tmp_dir, monkeypatch):
    video_path = os.path.join(tmp_dir, "Movie", "movie.mkv")
    os.makedirs(os.path.dirname(video_path), exist_ok=True)
    with open(video_path, "wb") as f:
        f.write(b"video")

    poster_path = os.path.join(tmp_dir, "online-poster.jpg")
    Image.new("RGB", (1200, 1800), color=(90, 110, 140)).save(poster_path, "JPEG")

    class FakeArtworkManager:
        def __init__(self):
            self.calls = []

        def get_video_poster(self, video_info, size="thumb", allow_online=None):
            self.calls.append((video_info, size, allow_online))
            return poster_path

    service = VideoThumbnailService(tmp_dir, artwork_manager=FakeArtworkManager())
    service._ffmpeg = "/usr/bin/ffmpeg"

    def fail_run(*args, **kwargs):
        raise AssertionError("ffmpeg should not run when online poster is available")

    monkeypatch.setattr("services.video_thumbnails.subprocess.run", fail_run)

    thumb = service.thumbnail_path(
        {
            "file_path": video_path,
            "title": "Movie",
            "video_kind": "movie",
        },
        size=140,
    )

    assert os.path.exists(thumb)
    with Image.open(thumb) as rendered:
        assert rendered.size == (140, 210)
    assert service._artwork.calls[0][2] is False


def test_thumbnail_service_returns_empty_when_unavailable(tmp_dir):
    service = VideoThumbnailService(tmp_dir)
    service._ffmpeg = ""

    assert service.thumbnail_path({"file_path": os.path.join(tmp_dir, "missing.mkv")}) == ""
