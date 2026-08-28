import os
import shutil
import sys
import threading
import json
from pathlib import Path

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

import pytest

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


class _MpegRunner(_Runner):
    def __init__(self, media_info):
        super().__init__()
        self.media_info = media_info

    def run(self, command, cwd="", timeout=None, env=None):
        if "-show_entries" in command:
            self.commands.append(list(command))
            return _Result(stdout=json.dumps(self.media_info))
        if "mpeg2video" in command:
            self.commands.append(list(command))
            with open(command[-1], "wb") as handle:
                handle.write(b"mpeg-program-stream")
            return _Result()
        return super().run(command, cwd=cwd, timeout=timeout, env=env)


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


def test_video_rvp_uses_probed_duration_when_library_duration_is_missing(
    tmp_dir,
):
    source = os.path.join(tmp_dir, "Episode.mkv")
    with open(source, "wb") as handle:
        handle.write(b"source-video")

    runner = _MpegRunner(
        {
            "streams": [{"width": 1280, "height": 720}],
            "format": {"duration": "1409.386"},
        }
    )
    transcoder = VideoRvpTranscoder(
        os.path.join(tmp_dir, "cache"),
        ffmpeg_path="/bin/true",
        command_runner=runner,
    )
    row, _info = transcoder.prepare_track_for_sync(
        {
            "file_path": source,
            "title": "Episode",
            "duration": 0,
            "media_type": "video",
        },
        "mock-ipod",
    )

    assert row["duration"] == pytest.approx(1409.386)


def test_video_rvp_defaults_to_raw_profile():
    transcoder = VideoRvpTranscoder("/tmp/unused-rvp-cache")

    assert RVP_PROFILE_DEFAULT == "raw"
    assert transcoder._profile == "raw"
    assert transcoder._target_fps == 20


def test_compact_raw_profile_keeps_20fps_with_smaller_frames_and_audio(
    tmp_dir,
):
    source = os.path.join(tmp_dir, "Concert.mpg")
    with open(source, "wb") as handle:
        handle.write(b"source-video")

    runner = _Runner()
    transcoder = VideoRvpTranscoder(
        os.path.join(tmp_dir, "cache"),
        ffmpeg_path="/bin/true",
        command_runner=runner,
        profile="compact_raw",
    )
    row, info = transcoder.prepare_track_for_sync(
        {
            "file_path": source,
            "title": "Concert",
            "duration": 1.0,
            "media_type": "video",
        },
        "mock-ipod",
    )

    assert info["converted"] is True
    assert row["sync_output_ext"] == ".rvp"
    assert row["sync_video_width"] == 160
    assert row["sync_video_height"] == 120
    assert row["sync_video_fps"] == 20
    assert row["sync_video_sample_rate"] == 22050
    marker = Path(row["sync_source_path"]).read_text(encoding="utf-8")
    assert "width=160" in marker
    assert "height=120" in marker
    assert "fps=20" in marker
    assert "sample_rate=22050" in marker


def test_native_raw_profile_keeps_source_detail_and_fluid_motion(tmp_dir):
    source = os.path.join(tmp_dir, "Concert.mpg")
    with open(source, "wb") as handle:
        handle.write(b"source-video")

    runner = _Runner()
    transcoder = VideoRvpTranscoder(
        os.path.join(tmp_dir, "cache"),
        ffmpeg_path="/bin/true",
        command_runner=runner,
        profile="native_raw",
    )
    row, info = transcoder.prepare_track_for_sync(
        {
            "file_path": source,
            "title": "Concert",
            "duration": 1.0,
            "media_type": "video",
        },
        "mock-ipod",
    )

    assert info["converted"] is True
    assert row["sync_output_ext"] == ".rvp"
    assert row["sync_video_width"] == 320
    assert row["sync_video_height"] == 240
    assert row["sync_video_fps"] == 20
    assert row["sync_video_sample_rate"] == 22050
    marker = Path(row["sync_source_path"]).read_text(encoding="utf-8")
    assert "width=320" in marker
    assert "height=240" in marker
    assert "fps=20" in marker
    assert "sample_rate=22050" in marker


def test_compact_profile_rebuilds_compatible_mpeg_for_reliable_seeking(
    tmp_dir,
):
    source = os.path.join(tmp_dir, "Concert.mpg")
    with open(source, "wb") as handle:
        handle.write(b"already-compatible")
    runner = _MpegRunner(
        {
            "streams": [
                {
                    "codec_type": "video",
                    "codec_name": "mpeg2video",
                    "width": 320,
                    "height": 240,
                    "pix_fmt": "yuv420p",
                    "r_frame_rate": "20/1",
                    "has_b_frames": 0,
                },
                {
                    "codec_type": "audio",
                    "codec_name": "mp2",
                    "sample_rate": "44100",
                    "channels": 2,
                },
            ],
            "format": {"duration": "3600", "bit_rate": "500000"},
        }
    )
    transcoder = VideoRvpTranscoder(
        os.path.join(tmp_dir, "cache"),
        ffmpeg_path="/bin/true",
        command_runner=runner,
        profile="compact",
    )

    row, info = transcoder.prepare_track_for_sync(
        {
            "file_path": source,
            "title": "Concert",
            "media_type": "video",
            "file_hash": "source-hash",
        },
        "mock-ipod",
    )

    assert row["sync_source_path"] != source
    assert row["sync_output_ext"] == ".mpg"
    assert row["sync_video_fps"] == 20
    assert row["sync_transcoded"] is True
    assert row["duration"] == 3600.0
    assert info["reason"] == "seekable_mpeg2_video"
    command = next(
        command for command in runner.commands if "mpeg2video" in command
    )
    assert command[command.index("-g") + 1] == "12"
    assert command[command.index("-sc_threshold") + 1] == "0"
    assert command[command.index("-packetsize") + 1] == "2048"
    assert command[command.index("-f") + 1] == "mpeg"


def test_compact_profile_converts_other_video_to_20fps_mpeg(tmp_dir):
    source = os.path.join(tmp_dir, "Movie.mp4")
    with open(source, "wb") as handle:
        handle.write(b"source-video")
    runner = _MpegRunner(
        {
            "streams": [
                {
                    "codec_type": "video",
                    "codec_name": "h264",
                    "width": 1920,
                    "height": 1080,
                    "pix_fmt": "yuv420p",
                    "r_frame_rate": "30/1",
                    "has_b_frames": 2,
                },
                {
                    "codec_type": "audio",
                    "codec_name": "aac",
                    "sample_rate": "48000",
                    "channels": 2,
                },
            ],
            "format": {"duration": "60", "bit_rate": "5000000"},
        }
    )
    transcoder = VideoRvpTranscoder(
        os.path.join(tmp_dir, "cache"),
        ffmpeg_path="/bin/true",
        command_runner=runner,
        profile="compact",
    )

    row, info = transcoder.prepare_track_for_sync(
        {
            "file_path": source,
            "title": "Movie",
            "media_type": "video",
            "file_hash": "source-hash",
        },
        "mock-ipod",
    )

    command = next(
        command for command in runner.commands if "mpeg2video" in command
    )
    assert row["sync_output_ext"] == ".mpg"
    assert row["sync_video_fps"] == 20
    assert row["sync_transcoded"] is True
    assert info["reason"] == "seekable_mpeg2_video"
    assert "fps=20" in command[command.index("-vf") + 1]
    assert command[command.index("-q:v") + 1] == "2"
    assert command[command.index("-maxrate") + 1] == "1600k"
    assert command[command.index("-bufsize") + 1] == "800k"
    assert command[command.index("-b:a") + 1] == "112k"
    assert command[command.index("-bf") + 1] == "0"


def test_video_rvp_rejects_conversion_before_filling_local_cache(
    tmp_dir, monkeypatch
):
    source = os.path.join(tmp_dir, "Long Movie.mp4")
    with open(source, "wb") as handle:
        handle.write(b"source-video")

    runner = _Runner()
    transcoder = VideoRvpTranscoder(
        os.path.join(tmp_dir, "cache"),
        ffmpeg_path="/bin/true",
        command_runner=runner,
    )
    disk_usage = shutil._ntuple_diskusage(
        total=10 * 1024 * 1024,
        used=9 * 1024 * 1024,
        free=1024 * 1024,
    )
    monkeypatch.setattr(shutil, "disk_usage", lambda _path: disk_usage)

    with pytest.raises(RuntimeError, match="not enough local cache space"):
        transcoder.prepare_track_for_sync(
            {
                "file_path": source,
                "title": "Long Movie",
                "duration": 3600,
                "media_type": "video",
            },
            "mock-ipod",
        )

    assert not any(
        "rawvideo" in command for command in runner.commands
    )


def test_video_rvp_removes_partial_temp_file_after_ffmpeg_failure(tmp_dir):
    class _FailingRunner(_Runner):
        def run(self, command, cwd="", timeout=None, env=None):
            result = super().run(
                command, cwd=cwd, timeout=timeout, env=env
            )
            if "rawvideo" in command:
                result.returncode = 1
                result.stderr = "simulated write failure"
            return result

    source = os.path.join(tmp_dir, "Failed Movie.mp4")
    with open(source, "wb") as handle:
        handle.write(b"source-video")

    cache_root = os.path.join(tmp_dir, "cache")
    transcoder = VideoRvpTranscoder(
        cache_root,
        ffmpeg_path="/bin/true",
        command_runner=_FailingRunner(),
    )
    with pytest.raises(RuntimeError, match="video conversion failed"):
        transcoder.prepare_track_for_sync(
            {
                "file_path": source,
                "title": "Failed Movie",
                "duration": 1,
                "media_type": "video",
            },
            "mock-ipod",
        )

    leftovers = []
    for root, _dirs, files in os.walk(cache_root):
        leftovers.extend(
            os.path.join(root, name)
            for name in files
            if name.endswith(".tmp")
        )
    assert leftovers == []


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
