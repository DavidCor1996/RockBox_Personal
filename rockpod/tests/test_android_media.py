"""Tests for Android media detection and iPod conversion workflow."""

import os

from services.android_media import (
    ANDROID_PHOTO_EXTENSIONS,
    ANDROID_VIDEO_EXTENSIONS,
    IPODTIKTOK_FEED_PATH,
    IPODTIKTOK_MANIFEST_PATH,
    build_device_output_relpath,
    build_ffmpeg_command,
    build_import_filename,
    discover_android_sources,
    import_android_media,
    looks_like_android_root,
    rebuild_tiktok_feed,
    scan_android_media,
)


def _write_file(path, payload=b"x"):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as handle:
        handle.write(payload)


class _FakeCommandResult:
    def __init__(self, returncode=0, stdout="", stderr="", log_path=""):
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr
        self.log_path = log_path

    def failure_message(self):
        return f"Command failed with exit status {self.returncode}: fake ffmpeg\nLog: {self.log_path}"


class _FakeCommandRunner:
    def __init__(self, result=None, write_output=True):
        self.result = result or _FakeCommandResult()
        self.write_output = write_output
        self.commands = []
        self.cwd = []

    def run(self, command, cwd="", timeout=None, env=None):
        self.commands.append(command)
        self.cwd.append(cwd)
        if self.write_output and self.result.returncode == 0:
            _write_file(command[-1], b"converted")
        return self.result


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
    assert "-bf" in command
    assert "0" in command
    assert "-g" in command
    assert "12" in command
    assert "+low_delay" in command
    assert "-map" in command
    assert "0:a:0?" in command


def test_build_ffmpeg_command_for_photo_uses_loop_and_silence():
    command = build_ffmpeg_command("/tmp/in.jpg", "/tmp/out.mpg", "photo", photo_duration_seconds=6.0)
    assert "-loop" in command
    assert "1" in command
    assert "anullsrc=channel_layout=stereo:sample_rate=44100" in command
    assert "-shortest" in command


def test_import_android_media_converts_files_and_skips_existing(tmp_dir):
    source_root = os.path.join(tmp_dir, "Phone")
    device_mount = os.path.join(tmp_dir, "iPod")
    os.makedirs(device_mount, exist_ok=True)
    photo = os.path.join(source_root, "DCIM", "Camera", "IMG_0001.jpg")
    video = os.path.join(source_root, "Movies", "VID_0001.mp4")
    _write_file(photo, b"photo")
    _write_file(video, b"video")

    runner = _FakeCommandRunner()

    first = import_android_media(
        source_root,
        device_mount,
        device_subdir="Videos/Android Phone",
        ffmpeg_path="/usr/bin/ffmpeg",
        command_runner=runner,
    )
    second = import_android_media(
        source_root,
        device_mount,
        device_subdir="Videos/Android Phone",
        ffmpeg_path="/usr/bin/ffmpeg",
        command_runner=runner,
    )

    assert len(runner.commands) == 2
    assert all(command[0] == "/usr/bin/ffmpeg" for command in runner.commands)
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


def test_import_android_media_tiktok_mode_writes_feed_and_sequential_names(tmp_dir):
    source_root = os.path.join(tmp_dir, "Phone")
    device_mount = os.path.join(tmp_dir, "iPod")
    os.makedirs(device_mount, exist_ok=True)
    video_a = os.path.join(source_root, "Movies", "clip-one.mp4")
    video_b = os.path.join(source_root, "Movies", "clip-two.mp4")
    _write_file(video_a, b"a")
    _write_file(video_b, b"b")

    runner = _FakeCommandRunner()

    report = import_android_media(
        source_root,
        device_mount,
        include_photos=False,
        include_videos=True,
        for_tiktok_plugin=True,
        ffmpeg_path="/usr/bin/ffmpeg",
        command_runner=runner,
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
    assert len(runner.commands) == 2
    apps_dir = os.path.join(device_mount, ".rockbox", "rocks", "apps")
    assert sorted(os.listdir(apps_dir)) == [
        ".ipodtiktok_feed.tsv",
        ".ipodtiktok_import_manifest.tsv",
    ]


def test_import_android_media_records_runner_failure_and_removes_temp(tmp_dir):
    source_root = os.path.join(tmp_dir, "Phone")
    device_mount = os.path.join(tmp_dir, "iPod")
    os.makedirs(device_mount, exist_ok=True)
    video = os.path.join(source_root, "Movies", "VID_0001.mp4")
    _write_file(video, b"video")
    result = _FakeCommandResult(returncode=7, stderr="", log_path=os.path.join(tmp_dir, "ffmpeg.log"))
    runner = _FakeCommandRunner(result=result, write_output=False)

    report = import_android_media(
        source_root,
        device_mount,
        include_photos=False,
        include_videos=True,
        ffmpeg_path="/usr/bin/ffmpeg",
        command_runner=runner,
    )

    assert report["imported"] == 0
    assert len(report["failures"]) == 1
    assert "exit status 7" in report["failures"][0]["error"]
    assert result.log_path in report["failures"][0]["error"]
    assert not [
        path for path, _dirs, files in os.walk(device_mount)
        for name in files
        if name.endswith(".rockpod_tmp")
    ]


def test_import_android_media_rejects_unsafe_device_root(tmp_dir):
    source_root = os.path.join(tmp_dir, "Phone")
    video = os.path.join(source_root, "Movies", "VID_0001.mp4")
    _write_file(video, b"video")

    try:
        import_android_media(
            source_root,
            os.path.abspath(os.sep),
            include_photos=False,
            include_videos=True,
            ffmpeg_path="/usr/bin/ffmpeg",
            command_runner=_FakeCommandRunner(),
        )
    except RuntimeError as exc:
        assert "unsafe device root" in str(exc)
    else:
        raise AssertionError("Android import accepted an unsafe device root")


def test_tiktok_feed_ignores_manifest_rows_outside_device_root(tmp_dir):
    device_mount = os.path.join(tmp_dir, "iPod")
    os.makedirs(device_mount, exist_ok=True)
    valid_rel = os.path.join("Videos", "iPodTikTok", "ipodtiktok1.mpg")
    _write_file(os.path.join(device_mount, valid_rel), b"video")
    manifest_path = os.path.join(device_mount, IPODTIKTOK_MANIFEST_PATH)
    os.makedirs(os.path.dirname(manifest_path), exist_ok=True)
    with open(manifest_path, "w", encoding="utf-8") as handle:
        handle.write("signature\ttitle\trel_path\tsource_rel_path\n")
        handle.write(f"good\tGood\t{valid_rel}\tMovies/good.mp4\n")
        handle.write("bad\tBad\t../escaped.mpg\tMovies/bad.mp4\n")

    result = rebuild_tiktok_feed(device_mount)

    assert result["feed_entries"] == 1
    assert "good" in result["manifest_rows"]
    assert "bad" not in result["manifest_rows"]
    feed_path = os.path.join(device_mount, IPODTIKTOK_FEED_PATH)
    with open(feed_path, "r", encoding="utf-8") as handle:
        feed = handle.read()
    assert "ipodtiktok1" in feed
    assert "escaped" not in feed
