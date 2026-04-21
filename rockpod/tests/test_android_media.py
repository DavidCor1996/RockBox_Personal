"""Tests for Android media detection and iPod conversion workflow."""

import os

from services.android_media import (
    ANDROID_PHOTO_EXTENSIONS,
    ANDROID_VIDEO_EXTENSIONS,
    IPODTIKTOK_FEED_PATH,
    build_device_output_relpath,
    build_ffmpeg_command,
    build_import_filename,
    discover_android_sources,
    import_android_media,
    looks_like_android_root,
    scan_android_media,
)


def _write_file(path, payload=b"x"):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as handle:
        handle.write(payload)


def test_looks_like_android_root_accepts_media_folders(tmp_dir):
    root = os.path.join(tmp_dir, "Phone")
    os.makedirs(os.path.join(root, "DCIM", "Camera"), exist_ok=True)
    assert looks_like_android_root(root) is True
    assert looks_like_android_root(os.path.join(root, "DCIM")) is True


def test_discover_android_sources_includes_configured_path(tmp_dir):
    root = os.path.join(tmp_dir, "Pixel")
    os.makedirs(os.path.join(root, "Pictures"), exist_ok=True)
    found = discover_android_sources(root)
    assert root in found
    assert found[0] == root


def test_scan_android_media_collects_photos_and_videos(tmp_dir):
    root = os.path.join(tmp_dir, "Phone")
    photo = os.path.join(root, "DCIM", "Camera", "IMG_0001.jpg")
    video = os.path.join(root, "Movies", "VID_0001.mp4")
    ignored = os.path.join(root, "Movies", "notes.txt")
    _write_file(photo, b"photo")
    _write_file(video, b"video")
    _write_file(ignored, b"ignored")

    items = scan_android_media(root)

    kinds = {item["kind"] for item in items}
    paths = {os.path.basename(item["source_path"]) for item in items}
    assert kinds == {"photo", "video"}
    assert paths == {"IMG_0001.jpg", "VID_0001.mp4"}


def test_build_import_filename_is_stable_for_same_source():
    item = {
        "source_path": "/phone/DCIM/Camera/IMG_1234.jpg",
        "kind": "photo",
        "size": 12345,
        "mtime": 1712345678.0,
    }
    assert build_import_filename(item) == build_import_filename(item)
    assert build_import_filename(item).endswith(".mpg")


def test_build_device_output_relpath_uses_target_subdir():
    item = {
        "source_path": "/phone/Movies/clip.mp4",
        "kind": "video",
        "size": 20,
        "mtime": 1712345678.0,
    }
    rel_path = build_device_output_relpath(item, "Videos/Android Phone")
    assert rel_path.startswith(os.path.join("Videos", "Android Phone"))
    assert rel_path.endswith(".mpg")


def test_build_ffmpeg_command_for_video_uses_mpeg2_profile():
    command = build_ffmpeg_command("/tmp/in.mp4", "/tmp/out.mpg", "video")
    assert command[0] == "ffmpeg"
    assert "-c:v" in command
    assert "mpeg2video" in command
    assert "-map" in command
    assert "0:a:0?" in command


def test_build_ffmpeg_command_for_photo_uses_loop_and_silence():
    command = build_ffmpeg_command("/tmp/in.jpg", "/tmp/out.mpg", "photo", photo_duration_seconds=6.0)
    assert "-loop" in command
    assert "1" in command
    assert "anullsrc=channel_layout=stereo:sample_rate=44100" in command
    assert "-shortest" in command


def test_import_android_media_converts_files_and_skips_existing(tmp_dir, monkeypatch):
    source_root = os.path.join(tmp_dir, "Phone")
    device_mount = os.path.join(tmp_dir, "iPod")
    os.makedirs(device_mount, exist_ok=True)
    photo = os.path.join(source_root, "DCIM", "Camera", "IMG_0001.jpg")
    video = os.path.join(source_root, "Movies", "VID_0001.mp4")
    _write_file(photo, b"photo")
    _write_file(video, b"video")

    calls = []

    class Result:
        returncode = 0
        stdout = ""
        stderr = ""

    def fake_run(command, check=False, capture_output=True, text=True):
        calls.append(command)
        target = command[-1]
        _write_file(target, b"converted")
        return Result()

    monkeypatch.setattr("services.android_media.subprocess.run", fake_run)

    first = import_android_media(
        source_root,
        device_mount,
        device_subdir="Videos/Android Phone",
        ffmpeg_path="/usr/bin/ffmpeg",
    )
    second = import_android_media(
        source_root,
        device_mount,
        device_subdir="Videos/Android Phone",
        ffmpeg_path="/usr/bin/ffmpeg",
    )

    assert len(calls) == 2
    assert first["scanned"] == 2
    assert first["imported"] == 2
    assert first["imported_photos"] == 1
    assert first["imported_videos"] == 1
    assert not first["failures"]
    assert second["imported"] == 0
    assert second["skipped_existing"] == 2
    for rel_path in first["imported_paths"]:
        assert rel_path.endswith(".mpg")
        assert os.path.isfile(os.path.join(device_mount, rel_path))


def test_extension_sets_cover_expected_android_media_types():
    assert ".jpg" in ANDROID_PHOTO_EXTENSIONS
    assert ".heic" in ANDROID_PHOTO_EXTENSIONS
    assert ".mp4" in ANDROID_VIDEO_EXTENSIONS
    assert ".3gp" in ANDROID_VIDEO_EXTENSIONS


def test_import_android_media_tiktok_mode_writes_feed_and_sequential_names(tmp_dir, monkeypatch):
    source_root = os.path.join(tmp_dir, "Phone")
    device_mount = os.path.join(tmp_dir, "iPod")
    os.makedirs(device_mount, exist_ok=True)
    video_a = os.path.join(source_root, "Movies", "clip-one.mp4")
    video_b = os.path.join(source_root, "Movies", "clip-two.mp4")
    _write_file(video_a, b"a")
    _write_file(video_b, b"b")

    class Result:
        returncode = 0
        stdout = ""
        stderr = ""

    def fake_run(command, check=False, capture_output=True, text=True):
        target = command[-1]
        _write_file(target, b"converted")
        return Result()

    monkeypatch.setattr("services.android_media.subprocess.run", fake_run)

    report = import_android_media(
        source_root,
        device_mount,
        include_photos=False,
        include_videos=True,
        for_tiktok_plugin=True,
        ffmpeg_path="/usr/bin/ffmpeg",
    )

    assert report["for_tiktok_plugin"] is True
    assert report["device_subdir"] == os.path.join("Videos", "iPodTikTok")
    assert report["imported_paths"] == [
        os.path.join("Videos", "iPodTikTok", "ipodtiktok1.mpg"),
        os.path.join("Videos", "iPodTikTok", "ipodtiktok2.mpg"),
    ]
    feed_path = os.path.join(device_mount, IPODTIKTOK_FEED_PATH)
    assert os.path.isfile(feed_path)
    with open(feed_path, "r", encoding="utf-8") as handle:
        feed_text = handle.read()
    assert "id\ttitle\tpath" in feed_text
    assert "/Videos/iPodTikTok/ipodtiktok1.mpg" in feed_text
    assert "/Videos/iPodTikTok/ipodtiktok2.mpg" in feed_text
