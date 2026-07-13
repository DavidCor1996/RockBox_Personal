import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from services.video_rvp import (
    RVP_PROFILE_DEFINITIONS,
    RVP_PROFILE_DEFAULT,
    RVP_FPS,
    RVP_HEIGHT,
    RVP_WIDTH,
    VideoRvpTranscoder,
)


class _Result:
    def __init__(self, returncode=0, stderr="", stdout=""):
        self.returncode = returncode
        self.stderr = stderr
        self.stdout = stdout
        self.log_path = ""


class _Runner:
    def __init__(self, frames=RVP_FPS, audio_bytes=4096):
        self.commands = []
        self.frames = frames
        self.audio_bytes = audio_bytes

    def run(self, command, cwd="", timeout=None, env=None):
        self.commands.append(list(command))
        target = command[-1]
        if "-f" in command and "rawvideo" in command:
            width = RVP_WIDTH
            height = RVP_HEIGHT
            if "-vf" in command:
                vf_index = command.index("-vf") + 1
                if vf_index < len(command):
                    filter_text = command[vf_index]
                    for token in filter_text.split(","):
                        if token.startswith("scale="):
                            scale_value = token.split("=", 1)[1]
                            parts = scale_value.split(":")
                            width, height = int(parts[0]), int(parts[1])
                            break
            with open(target, "wb") as handle:
                handle.truncate(width * height * 3 // 2 * self.frames)
        elif "-f" in command and "s16le" in command:
            with open(target, "wb") as handle:
                handle.truncate(self.audio_bytes)
        return _Result()


def test_video_rvp_transcoder_creates_bundle_and_sync_metadata(tmp_dir):
    source = os.path.join(tmp_dir, "Movie.mp4")
    with open(source, "wb") as handle:
        handle.write(b"source-video")

    runner = _Runner()
    transcoder = VideoRvpTranscoder(os.path.join(tmp_dir, "cache"), ffmpeg_path="/bin/true", command_runner=runner)
    row, info = transcoder.prepare_track_for_sync(
        {
            "file_path": source,
            "title": "Movie",
            "album": "Movies",
            "artist": "",
            "album_artist": "",
            "duration": 1.0,
            "media_type": "video",
            "file_hash": "source_hash",
        },
        "mock-ipod",
    )

    assert info["converted"] is True
    assert row["sync_output_ext"] == ".rvp"
    assert row["codec"] == "RVP"
    assert row["file_size"] > 0
    assert set(row["sync_video_bundle_paths"]) == {"rvp", "yuv", "pcm"}
    for path in row["sync_video_bundle_paths"].values():
        assert os.path.isfile(path)

    with open(row["sync_video_bundle_paths"]["rvp"], "r", encoding="utf-8") as handle:
        marker = handle.read()
    compact_profile = RVP_PROFILE_DEFINITIONS[RVP_PROFILE_DEFAULT]
    assert f"width={compact_profile['max_width']}" in marker
    assert f"height={compact_profile['max_height']}" in marker
    assert "fit=contain" in marker
    assert "video=" in marker
    assert "audio=" in marker
    assert len(runner.commands) >= 2
    assert any("-show_entries" in cmd for cmd in runner.commands if isinstance(cmd, list))
    assert any("rawvideo" in cmd for cmd in runner.commands if isinstance(cmd, list))
    assert any("s16le" in cmd for cmd in runner.commands if isinstance(cmd, list))


def test_video_rvp_ffmpeg_filter_fits_screen_without_cropping_portrait():
    command = VideoRvpTranscoder._video_command("ffmpeg", "/tmp/in.mp4", "/tmp/out.yuv", 240, 320)
    vf = command[command.index("-vf") + 1]

    assert vf == (
        "scale=240:320:force_original_aspect_ratio=decrease,"
        "pad=240:320:(ow-iw)/2:(oh-ih)/2:black,fps=20"
    )


def test_video_rvp_fit_dimensions_prefers_full_width_for_landscape():
    width, height = VideoRvpTranscoder._fit_dimensions(1920, 1080)
    assert (width, height) == (320, 240)


def test_video_rvp_fit_dimensions_prefers_full_height_for_portrait():
    width, height = VideoRvpTranscoder._fit_dimensions(1080, 1920)
    assert (width, height) == (320, 240)


def test_video_rvp_segments_long_video_before_sync(tmp_dir):
    source = os.path.join(tmp_dir, "Long Show.mpg")
    with open(source, "wb") as handle:
        handle.write(b"source-video")

    compact_profile = RVP_PROFILE_DEFINITIONS[RVP_PROFILE_DEFAULT]
    compact_fps = int(compact_profile["fps"])
    compact_sample_rate = int(compact_profile["sample_rate"])
    compact_pcm_bytes_per_frame = compact_sample_rate * 2 * 2 // compact_fps
    frames = compact_fps * 120 * 3 + 10
    runner = _Runner(
        frames=frames,
        audio_bytes=frames * compact_pcm_bytes_per_frame,
    )
    transcoder = VideoRvpTranscoder(os.path.join(tmp_dir, "cache"), ffmpeg_path="/bin/true", command_runner=runner)
    row, _info = transcoder.prepare_track_for_sync(
        {
            "file_path": source,
            "title": "Long Show",
            "album": "Shows",
            "artist": "",
            "album_artist": "",
            "duration": float(frames) / compact_fps,
            "media_type": "video",
            "file_hash": "long_source_hash",
        },
        "mock-ipod",
    )

    assert len(row["sync_video_segments"]) == 4
    assert "yuv" in row["sync_video_bundle_paths"]
    assert "pcm" in row["sync_video_bundle_paths"]
    with open(row["sync_video_bundle_paths"]["rvp"], "r", encoding="utf-8") as handle:
        marker = handle.read()
    assert "segments=4" in marker
    assert "segment1_video=" in marker
    assert "segment2_audio=" in marker
    for segment in row["sync_video_segments"]:
        assert os.path.isfile(segment["yuv"])
        assert os.path.isfile(segment["pcm"])


def test_video_rvp_reuses_digest_until_a_component_changes(tmp_dir, monkeypatch):
    source = os.path.join(tmp_dir, "Cached Movie.mp4")
    with open(source, "wb") as handle:
        handle.write(b"source-video")
    transcoder = VideoRvpTranscoder(
        os.path.join(tmp_dir, "cache"),
        ffmpeg_path="/bin/true",
        command_runner=_Runner(),
    )
    track = {
        "file_path": source,
        "title": "Cached Movie",
        "album": "Movies",
        "media_type": "video",
        "file_hash": "source-hash",
    }
    first, _info = transcoder.prepare_track_for_sync(track, "mock-ipod")
    original_hash = VideoRvpTranscoder._bundle_hash
    calls = []

    def counted_hash(*paths):
        calls.append(paths)
        return original_hash(*paths)

    monkeypatch.setattr(VideoRvpTranscoder, "_bundle_hash", staticmethod(counted_hash))
    second, _info = transcoder.prepare_track_for_sync(track, "mock-ipod")
    assert second["file_hash"] == first["file_hash"]
    assert calls == []

    yuv_path = second["sync_video_bundle_paths"]["yuv"]
    stat = os.stat(yuv_path)
    os.utime(yuv_path, ns=(stat.st_atime_ns, stat.st_mtime_ns + 1_000_000))
    transcoder.prepare_track_for_sync(track, "mock-ipod")
    assert len(calls) == 1
