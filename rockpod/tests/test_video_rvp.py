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


class _H264Runner(_Runner):
    def __init__(self, output_profile="Constrained Baseline"):
        super().__init__()
        self.output_profile = output_profile

    @staticmethod
    def _write_minimal_mp4(path):
        with open(path, "wb") as handle:
            handle.write(b"\x00\x00\x00\x10ftypM4V \x00\x00\x00\x00")
            handle.write(b"\x00\x00\x00\x08moov")
            handle.write(b"\x00\x00\x00\x09mdat\x00")

    def run(self, command, cwd="", timeout=None, env=None):
        self.commands.append(list(command))
        if "-show_entries" in command:
            if str(command[-1]).endswith(".tmp") or str(command[-1]).endswith(".m4v"):
                return _Result(
                    stdout=json.dumps(
                        {
                            "streams": [
                                {
                                    "codec_type": "video",
                                    "codec_name": "h264",
                                    "profile": self.output_profile,
                                    "level": 30,
                                    "width": 640,
                                    "height": 480,
                                    "pix_fmt": "yuv420p",
                                    "r_frame_rate": "24000/1001",
                                    "has_b_frames": 0,
                                    "refs": 1,
                                    "bit_rate": "1500000",
                                },
                                {
                                    "codec_type": "audio",
                                    "codec_name": "aac",
                                    "profile": "LC",
                                    "sample_rate": "48000",
                                    "channels": 2,
                                    "bit_rate": "128000",
                                },
                            ],
                            "format": {
                                "format_name": "mov,mp4,m4a,3gp,3g2,mj2",
                                "duration": "60.0",
                                "bit_rate": "1628000",
                            },
                        }
                    )
                )
            return _Result(
                stdout=json.dumps(
                    {
                        "streams": [
                            {
                                "codec_type": "video",
                                "codec_name": "h264",
                                "width": 1920,
                                "height": 1080,
                                "r_frame_rate": "24000/1001",
                            }
                        ],
                        "format": {"duration": "60.0"},
                    }
                )
            )
        if "libx264" in command:
            self._write_minimal_mp4(command[-1])
            return _Result()
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


def test_h264_apple_exact_profile_builds_measured_encode_and_mux_commands(tmp_dir):
    source = os.path.join(tmp_dir, "Apple Source.mov")
    with open(source, "wb") as handle:
        handle.write(b"source-video")
    transcoder = VideoRvpTranscoder(
        os.path.join(tmp_dir, "cache"),
        ffmpeg_path="/bin/true",
        profile="h264_1500k_compat",
    )
    encode = transcoder._h264_encode_command(
        "/opt/x264-apple-ipod", source, "/tmp/video.h264",
        640, 480, "24000/1001", 24000, 1001,
    )
    mux = transcoder._h264_mux_command(
        "/usr/bin/ffmpeg", source, "/tmp/video.h264", "/tmp/video.m4v",
        24000, 1001,
    )

    assert transcoder._profile == "h264_apple_exact"
    assert encode[encode.index("--profile") + 1] == "baseline"
    assert encode[encode.index("--level") + 1] == "3.0"
    assert encode[encode.index("--bitrate") + 1] == "1500"
    assert encode[encode.index("--slices") + 1] == "2"
    assert encode[encode.index("--keyint") + 1] == "96"
    assert encode[encode.index("--threads") + 1] == "32"
    assert encode[encode.index("--vbv-maxrate") + 1] == "4006"
    assert encode[encode.index("--vbv-bufsize") + 1] == "4014"
    assert encode[encode.index("--nal-hrd") + 1] == "vbr"
    assert encode[encode.index("--colorprim") + 1] == "smpte170m"
    assert encode[encode.index("--transfer") + 1] == "bt709"
    assert encode[encode.index("--chromaloc") + 1] == "2"
    assert "--no-deblock" in encode
    assert mux[mux.index("-q:a") + 1] == "0.28"
    assert mux[mux.index("-ar") + 1] == "44100"
    assert mux[mux.index("-video_track_timescale") + 1] == "24000"
    assert mux[mux.index("-brand") + 1] == "M4V "


def test_h264_apple_exact_uses_24_threads_when_downscaling_hd_source(tmp_dir):
    transcoder = VideoRvpTranscoder(
        os.path.join(tmp_dir, "cache"), profile="h264_apple_exact"
    )

    command = transcoder._h264_encode_command(
        "/opt/x264-apple-ipod", "/tmp/source.mp4", "/tmp/video.mp4",
        640, 360, "30/1", 30, 1,
        source_width=1920, source_height=1080,
    )

    assert command[command.index("--threads") + 1] == "24"


def test_apple_exact_x264_patch_keeps_threaded_and_odd_mb_slices_canonical():
    patch = (
        Path(__file__).resolve().parents[2]
        / "tools"
        / "patches"
        / "x264-apple-ipod-exact.patch"
    ).read_text(encoding="utf-8")

    assert "RockPod Apple-iPod exact bitstream patch v5" in patch
    assert "h->fenc->i_frame > 0" in patch
    assert "(height * width * i_slice_num) / h->param.i_slice_count" in patch
    assert "height * width * i_slice_num + round_bias" not in patch


def test_h264_relative_x264_path_is_resolved_before_cache_cwd(tmp_dir):
    transcoder = VideoRvpTranscoder(
        os.path.join(tmp_dir, "cache"),
        profile="h264_apple_exact",
        x264_path="rockpod/.tools/x264-apple-ipod",
    )

    assert os.path.isabs(transcoder._x264_path)
    assert transcoder._x264_path.endswith(
        os.path.join("rockpod", ".tools", "x264-apple-ipod")
    )


def test_h264_exact_source_is_validated_and_reused_without_reencoding(
    tmp_dir, monkeypatch,
):
    source = os.path.join(tmp_dir, "already-exact.m4v")
    with open(source, "wb") as handle:
        handle.write(b"validated-apple-file")
    transcoder = VideoRvpTranscoder(
        os.path.join(tmp_dir, "cache"),
        ffmpeg_path="/bin/true",
        profile="h264_apple_exact",
    )
    monkeypatch.setattr(
        transcoder, "_probe_mpeg_info",
        lambda *_args: {
            "duration": 60.0,
            "video": {
                "width": 640, "height": 360, "r_frame_rate": "30/1",
            },
        },
    )
    validation = {"digest": "exact", "bitrate_kbps": 1548}
    monkeypatch.setattr(
        transcoder, "_validate_h264_file",
        lambda *_args: validation,
    )
    monkeypatch.setattr(
        transcoder, "_ensure_h264_file",
        lambda *_args: (_ for _ in ()).throw(
            AssertionError("exact input was re-encoded")
        ),
    )

    row, info = transcoder.prepare_track_for_sync(
        {"file_path": source, "title": "Exact", "media_type": "Video"},
        "mock-ipod",
    )

    assert row["sync_source_path"] == source
    assert row["sync_output_ext"] == ".m4v"
    assert row["sync_video_validation_digest"] == "exact"
    assert info["converted"] is False
    assert info["reason"] == "already_apple_ipod_exact"


def test_h264_apple_exact_small_bucket_and_downrate_filter(tmp_dir):
    transcoder = VideoRvpTranscoder(
        os.path.join(tmp_dir, "cache"), profile="h264_apple_exact"
    )
    command = transcoder._h264_encode_command(
        "/opt/x264-apple-ipod", "/tmp/source.mov", "/tmp/video.h264",
        320, 240, "60/1", 30, 1,
    )

    assert command[command.index("--level") + 1] == "1.3"
    assert command[command.index("--bitrate") + 1] == "768"
    assert command[command.index("--vbv-maxrate") + 1] == "770"
    assert command[command.index("--vbv-bufsize") + 1] == "1999"
    assert "--no-deblock" not in command
    assert "select_every:2,0" in command[command.index("--vf") + 1]


def test_h264_ipod_video_target_uses_5g_dimensions_and_contract(tmp_dir):
    transcoder = VideoRvpTranscoder(
        os.path.join(tmp_dir, "cache"), profile="h264_apple_exact"
    )

    assert transcoder._fit_apple_dimensions(
        1920, 1080, max_width=320, max_height=240
    ) == (320, 180)
    width, height, contract, profile_key = transcoder._h264_device_contract(
        "ipodvideo"
    )
    assert (width, height) == (320, 240)
    assert "ipod-video-5g" in contract
    assert "ipod-video-5g" in profile_key


def test_h264_ipod_video_cache_is_distinct_from_6g_cache(tmp_dir):
    source = os.path.join(tmp_dir, "same-source.mp4")
    Path(source).write_bytes(b"same-source")
    transcoder = VideoRvpTranscoder(
        os.path.join(tmp_dir, "cache"), profile="h264_apple_exact"
    )
    row = {"file_hash": "same-source-hash"}
    _, _, contract_5g, key_5g = transcoder._h264_device_contract("ipodvideo")
    _, _, contract_6g, key_6g = transcoder._h264_device_contract("ipod6g")

    path_5g = transcoder._cache_h264_path(
        source, row, "same-device", 320, 180, 30, 1,
        contract=contract_5g, profile_key=key_5g,
    )
    path_6g = transcoder._cache_h264_path(
        source, row, "same-device", 640, 360, 30, 1,
        contract=contract_6g, profile_key=key_6g,
    )

    assert path_5g != path_6g


def test_h264_storage_estimate_is_far_smaller_than_raw():
    h264 = VideoRvpTranscoder.estimated_profile_bytes(
        "h264_apple_exact", 3600
    )
    raw = VideoRvpTranscoder.estimated_profile_bytes("raw", 3600)

    assert 650_000_000 < h264 < 750_000_000
    assert raw > h264 * 10


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
