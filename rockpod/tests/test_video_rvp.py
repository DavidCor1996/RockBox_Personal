import os
import sys
import threading

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


class _BlockingRunner(_Runner):
    def __init__(self):
        super().__init__()
        self.video_started = threading.Event()
        self.release_video = threading.Event()
        self.video_runs = 0
        self.audio_runs = 0
        self._count_lock = threading.Lock()

    def run(self, command, cwd="", timeout=None, env=None):
        if "-f" in command and "rawvideo" in command:
            with self._count_lock:
                self.video_runs += 1
            self.video_started.set()
            assert self.release_video.wait(timeout=5)
        elif "-f" in command and "s16le" in command:
            with self._count_lock:
                self.audio_runs += 1
        return super().run(command, cwd=cwd, timeout=timeout, env=env)


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


def test_video_rvp_serializes_concurrent_preparation_of_same_bundle(tmp_dir):
    source = os.path.join(tmp_dir, "Concurrent Movie.mp4")
    with open(source, "wb") as handle:
        handle.write(b"source-video")

    runner = _BlockingRunner()
    cache_root = os.path.join(tmp_dir, "cache")
    track = {
        "file_path": source,
        "title": "Concurrent Movie",
        "media_type": "video",
        "file_hash": "same-source",
    }
    results = []
    failures = []

    def prepare():
        try:
            transcoder = VideoRvpTranscoder(
                cache_root,
                ffmpeg_path="/bin/true",
                command_runner=runner,
            )
            results.append(transcoder.prepare_track_for_sync(track, "mock-ipod")[0])
        except Exception as exc:
            failures.append(exc)

    first = threading.Thread(target=prepare)
    second = threading.Thread(target=prepare)
    first.start()
    assert runner.video_started.wait(timeout=5)
    second.start()
    runner.release_video.set()
    first.join(timeout=5)
    second.join(timeout=5)

    assert not first.is_alive()
    assert not second.is_alive()
    assert failures == []
    assert len(results) == 2
    assert runner.video_runs == 1
    assert runner.audio_runs == 1
    assert results[0]["file_hash"] == results[1]["file_hash"]


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


def test_video_rvp_pads_short_recoverable_audio_while_segmenting(tmp_dir):
    source = os.path.join(tmp_dir, "Damaged Show.mkv")
    with open(source, "wb") as handle:
        handle.write(b"source-video")

    profile = RVP_PROFILE_DEFINITIONS[RVP_PROFILE_DEFAULT]
    fps = int(profile["fps"])
    sample_rate = int(profile["sample_rate"])
    pcm_bytes_per_frame = sample_rate * 2 * 2 // fps
    segment_frames = fps * 120
    frames = segment_frames * 2 + 10
    runner = _Runner(
        frames=frames,
        audio_bytes=(segment_frames + 5) * pcm_bytes_per_frame,
    )
    transcoder = VideoRvpTranscoder(
        os.path.join(tmp_dir, "cache"),
        ffmpeg_path="/bin/true",
        command_runner=runner,
    )

    row, _info = transcoder.prepare_track_for_sync(
        {
            "file_path": source,
            "title": "Damaged Show",
            "media_type": "video",
            "file_hash": "damaged-source",
        },
        "mock-ipod",
    )

    segments = row["sync_video_segments"]
    assert len(segments) == 3
    assert os.path.getsize(segments[0]["pcm"]) == segment_frames * pcm_bytes_per_frame
    assert os.path.getsize(segments[1]["pcm"]) == segment_frames * pcm_bytes_per_frame
    assert os.path.getsize(segments[2]["pcm"]) == 10 * pcm_bytes_per_frame
    with open(segments[2]["pcm"], "rb") as handle:
        assert handle.read() == b"\0" * (10 * pcm_bytes_per_frame)


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
