"""Tests for video thumbnail caching."""

import os
import sys
import time

from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from services.video_thumbnails import VideoThumbnailService


class _FakeCommandResult:
    returncode = 0
    stdout = ""
    stderr = ""
    log_path = ""


class _FakeFrameRunner:
    def __init__(self):
        self.commands = []
        self.cwd = ""

    def run(self, command, cwd="", timeout=None, env=None):
        self.commands.append(command)
        self.cwd = cwd
        target = command[-1]
        Image.new("RGB", (320, 180), color=(120, 140, 180)).save(target, "JPEG")
        return _FakeCommandResult()


def test_thumbnail_service_caches_rendered_result(tmp_dir):
    video_path = os.path.join(tmp_dir, "clip.mkv")
    with open(video_path, "wb") as f:
        f.write(b"video")

    runner = _FakeFrameRunner()
    service = VideoThumbnailService(tmp_dir, command_runner=runner)
    service._ffmpeg = "/usr/bin/ffmpeg"

    track = {"file_path": video_path, "duration": 120.0}
    first = service.thumbnail_path(track, size=100)
    second = service.thumbnail_path(track, size=100)

    assert os.path.exists(first)
    assert first == second
    assert len(runner.commands) == 1
    assert runner.cwd == os.path.dirname(runner.commands[0][-1])
    with Image.open(first) as rendered:
        assert rendered.size == (100, 150)


def test_thumbnail_service_prefers_local_poster_file(tmp_dir):
    show_dir = os.path.join(tmp_dir, "The Show")
    season_dir = os.path.join(show_dir, "Season 1")
    os.makedirs(season_dir, exist_ok=True)
    video_path = os.path.join(season_dir, "Episode 1.mkv")
    with open(video_path, "wb") as f:
        f.write(b"video")
    poster_path = os.path.join(show_dir, "poster.jpg")
    Image.new("RGB", (600, 900), color=(50, 80, 120)).save(poster_path, "JPEG")

    runner = _FakeFrameRunner()
    service = VideoThumbnailService(tmp_dir, command_runner=runner)
    service._ffmpeg = "/usr/bin/ffmpeg"

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
    assert runner.commands == []
    with Image.open(thumb) as rendered:
        assert rendered.size == (120, 180)


def test_thumbnail_service_prefers_online_poster_from_artwork_manager(tmp_dir):
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

    runner = _FakeFrameRunner()
    service = VideoThumbnailService(tmp_dir, artwork_manager=FakeArtworkManager(), command_runner=runner)
    service._ffmpeg = "/usr/bin/ffmpeg"

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
    assert runner.commands == []


def test_thumbnail_service_returns_empty_when_unavailable(tmp_dir):
    service = VideoThumbnailService(tmp_dir)
    service._ffmpeg = ""

    assert service.thumbnail_path({"file_path": os.path.join(tmp_dir, "missing.mkv")}) == ""


def test_video_list_thumbnail_and_manifest_generated_for_ipod(tmp_dir):
    video_path = os.path.join(tmp_dir, "Movie", "Real Movie.mpg")
    os.makedirs(os.path.dirname(video_path), exist_ok=True)
    with open(video_path, "wb") as f:
        f.write(b"video")
    poster_path = os.path.join(os.path.dirname(video_path), "Real Movie.jpg")
    Image.new("RGB", (600, 900), color=(40, 90, 130)).save(poster_path, "JPEG")

    runner = _FakeFrameRunner()
    service = VideoThumbnailService(tmp_dir, command_runner=runner)
    service._ffmpeg = "/usr/bin/ffmpeg"

    track = {
        "file_path": video_path,
        "title": "Real Movie",
        "media_type": "video",
        "video_kind": "movie",
    }
    thumb_path, thumb_hash, device_name, video_id = service.export_video_list_thumbnail(track)

    assert os.path.exists(thumb_path)
    assert thumb_hash
    assert device_name == f"{video_id}.bmp"
    with Image.open(thumb_path) as rendered:
        assert rendered.format == "BMP"
        assert rendered.size == (32, 32)
    assert runner.commands == []

    manifest_path, manifest_hash = service.export_video_list_manifest(
        [
            {
                "video_id": video_id,
                "thumb": os.path.join("thumbs", device_name),
                "title": "Real Movie",
                "kind": "movie",
                "group_key": video_path,
                "device_path": "Videos/Movies/Real Movie.mpg",
            }
        ]
    )

    assert os.path.exists(manifest_path)
    assert manifest_hash
    with open(manifest_path, "r", encoding="utf-8") as handle:
        data = handle.read()
    assert "video_id\tthumb\ttitle\tkind\tgroup_key\tdevice_path" in data
    assert f"{video_id}\tthumbs/{device_name}\tReal Movie\tmovie" in data
    assert os.path.basename(manifest_path) == "index.tsv"
    assert not [
        name for name in os.listdir(os.path.dirname(manifest_path))
        if name.startswith("tmp")
    ]


def test_video_heavy_fixture_records_thumbnail_manifest_profile(tmp_dir):
    runner = _FakeFrameRunner()
    service = VideoThumbnailService(tmp_dir, command_runner=runner)
    service._ffmpeg = "/usr/bin/ffmpeg"
    tracks = []
    for index in range(60):
        movie_dir = os.path.join(tmp_dir, "Movies", f"Movie {index:02d}")
        os.makedirs(movie_dir, exist_ok=True)
        video_path = os.path.join(movie_dir, f"Movie {index:02d}.mpg")
        poster_path = os.path.join(movie_dir, f"Movie {index:02d}.jpg")
        with open(video_path, "wb") as handle:
            handle.write(b"video")
        Image.new("RGB", (320, 480), (index * 2 % 255, 90, 130)).save(poster_path, "JPEG")
        tracks.append(
            {
                "file_path": video_path,
                "title": f"Movie {index:02d}",
                "media_type": "video",
                "video_kind": "movie",
            }
        )

    started_at = time.perf_counter()
    manifest_entries = []
    for track in tracks:
        thumb_path, thumb_hash, device_name, video_id = service.export_video_list_thumbnail(track)
        manifest_entries.append(
            {
                "video_id": video_id,
                "thumb": os.path.join("thumbs", device_name),
                "title": track["title"],
                "kind": track["video_kind"],
                "group_key": track["file_path"],
                "device_path": f"Videos/Movies/{track['title']}.mpg",
                "_thumb_path": thumb_path,
                "_thumb_hash": thumb_hash,
            }
        )
    manifest_path, manifest_hash = service.export_video_list_manifest(manifest_entries)
    with open(manifest_path, "r", encoding="utf-8") as handle:
        manifest_rows = sum(1 for line in handle if line and not line.startswith("#")) - 1
    profile = {
        "video_count": len(tracks),
        "thumbnail_count": sum(1 for entry in manifest_entries if os.path.exists(entry["_thumb_path"])),
        "manifest_rows": manifest_rows,
        "render_seconds": time.perf_counter() - started_at,
    }

    assert profile["video_count"] == 60
    assert profile["thumbnail_count"] == 60
    assert profile["manifest_rows"] == 60
    assert profile["render_seconds"] >= 0.0
    assert manifest_hash
    assert runner.commands == []
    assert not [name for name in os.listdir(os.path.dirname(manifest_path)) if name.startswith("tmp")]
