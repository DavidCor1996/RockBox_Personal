#!/usr/bin/env python3
"""Build and verify the iTunes 9.2.1 iPod-video golden corpus.

It creates known input movies, records files produced by iTunes' documented
"Advanced > Create iPod or iPhone Version" action, and rejects any manifest
whose Apple provenance or output contract is incomplete.  Its ``candidate``
mode then runs RockPod's production patched-x264/FFmpeg pipeline and compares
the result against the measured Apple contract.

Apple binaries are research inputs and must not be copied into the repository.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROCKPOD_ROOT = Path(__file__).resolve().parents[1] / "rockpod"
if str(ROCKPOD_ROOT) not in sys.path:
    sys.path.insert(0, str(ROCKPOD_ROOT))

from services.apple_video_exact import (  # noqa: E402
    AppleVideoContractError,
    apple_avc_contract_failures,
    inspect_avc_contract,
    local_x264_path,
)
from services.video_rvp import VideoRvpTranscoder  # noqa: E402


SCHEMA = "rockpod.apple-video-golden.v1"
ITUNES_VERSION = "9.2.1.5"
QUICKTIME_VERSION = "7.6.6 (1673)"
QUICKTIME_H264_SHA256 = (
    "29b2483de82be654440c88d99dfbb7f81b03a6216546ee583c45f8e650fb7222"
)
ITUNES_INSTALLER_SHA1 = {
    # Official Apple 32-bit and 64-bit iTunes 9.2.1.5 installers.
    "fd86e82bc52dd5a22d922aedf2a6063c224ca48c",
    "461d9cb0053d74f8b8d1804be3d4c50176a6036d",
}
RESERVED_SYNC_TEST_URL = "https://www.youtube.com/watch?v=U2zCCFNT6Vo"

# Keep the corpus short enough to move into an offline VM while crossing the
# resolution, aspect-ratio, rate, motion, audio, and down-rate boundaries that
# can select different QuickTime iPod presets.
CORPUS_CASES = (
    {
        "id": "flat-320x240-30-silent",
        "width": 320,
        "height": 240,
        "rate": "30",
        "pattern": "color=c=0x4060a0",
        "audio": "silent",
    },
    {
        "id": "motion-320x240-30000_1001-stereo",
        "width": 320,
        "height": 240,
        "rate": "30000/1001",
        "pattern": "testsrc2",
        "audio": "stereo",
    },
    {
        "id": "widescreen-640x360-24000_1001-mono",
        "width": 640,
        "height": 360,
        "rate": "24000/1001",
        "pattern": "smptebars",
        "audio": "mono",
    },
    {
        "id": "sd-640x480-30-stereo",
        "width": 640,
        "height": 480,
        "rate": "30",
        "pattern": "testsrc2",
        "audio": "stereo",
    },
    {
        "id": "portrait-240x320-25-stereo",
        "width": 240,
        "height": 320,
        "rate": "25",
        "pattern": "testsrc2",
        "audio": "stereo",
    },
    {
        "id": "downrate-640x480-60-stereo",
        "width": 640,
        "height": 480,
        "rate": "60",
        "pattern": "testsrc2",
        "audio": "stereo",
    },
)


class GoldenError(RuntimeError):
    """The Apple golden corpus is incomplete or not authoritative."""


def _run(command: list[str]) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip()
        raise GoldenError(
            f"command failed ({result.returncode}): {' '.join(command)}"
            + (f"\n{detail}" if detail else "")
        )
    return result


def _hash_file(path: Path, algorithm: str = "sha256") -> str:
    digest = hashlib.new(algorithm)
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _find_tool(explicit: str, name: str) -> str:
    candidate = str(explicit or "").strip()
    if candidate:
        resolved = os.path.abspath(candidate)
        if os.path.isfile(resolved):
            return resolved
        found = shutil.which(candidate)
        if found:
            return found
    found = shutil.which(name)
    if not found:
        raise GoldenError(f"{name} is required")
    return found


def _top_level_atoms(path: Path) -> list[dict[str, int | str]]:
    atoms: list[dict[str, int | str]] = []
    file_size = path.stat().st_size
    with path.open("rb") as handle:
        offset = 0
        while offset + 8 <= file_size:
            handle.seek(offset)
            header = handle.read(16)
            size = int.from_bytes(header[:4], "big")
            atom_type = header[4:8].decode("latin-1")
            header_size = 8
            if size == 1:
                if len(header) < 16:
                    raise GoldenError(f"truncated 64-bit MP4 atom in {path}")
                size = int.from_bytes(header[8:16], "big")
                header_size = 16
            elif size == 0:
                size = file_size - offset
            if size < header_size or offset + size > file_size:
                raise GoldenError(f"invalid MP4 atom extent in {path}")
            atoms.append({"type": atom_type, "offset": offset, "size": size})
            offset += size
        if offset != file_size:
            raise GoldenError(f"trailing or truncated MP4 data in {path}")
    return atoms


def _ftyp(path: Path) -> dict[str, object]:
    with path.open("rb") as handle:
        header = handle.read(8)
        if len(header) != 8 or header[4:8] != b"ftyp":
            return {}
        size = int.from_bytes(header[:4], "big")
        payload = handle.read(max(0, size - 8))
    if len(payload) < 8:
        return {}
    return {
        "major_brand": payload[:4].decode("latin-1"),
        "minor_version": int.from_bytes(payload[4:8], "big"),
        "compatible_brands": [
            payload[index:index + 4].decode("latin-1")
            for index in range(8, len(payload) - 3, 4)
        ],
    }


def _fraction(value: object) -> float:
    text = str(value or "0")
    try:
        if "/" in text:
            numerator, denominator = text.split("/", 1)
            return float(numerator) / float(denominator)
        return float(text)
    except (TypeError, ValueError, ZeroDivisionError):
        return 0.0


def _int(value: object, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _stream_summary(stream: dict[str, object]) -> dict[str, object]:
    fields = (
        "index", "codec_type", "codec_name", "codec_long_name", "profile",
        "codec_tag_string", "level", "width", "height", "coded_width",
        "coded_height", "pix_fmt", "sample_aspect_ratio",
        "display_aspect_ratio", "r_frame_rate", "avg_frame_rate",
        "time_base", "start_pts", "start_time", "duration_ts", "duration",
        "bit_rate", "max_bit_rate", "bits_per_raw_sample", "nb_frames",
        "has_b_frames", "refs", "sample_rate", "channels", "channel_layout",
    )
    result = {key: stream[key] for key in fields if key in stream}
    if stream.get("tags"):
        result["tags"] = dict(stream["tags"])
    return result


def inspect_movie(path: Path, ffprobe: str) -> dict[str, object]:
    command = [
        ffprobe, "-v", "error", "-show_streams", "-show_format",
        "-show_packets", "-show_data", "-show_entries",
        (
            "stream:stream_tags:format:format_tags:"
            "packet=stream_index,pts_time,duration_time,size,pos,flags"
        ),
        "-of", "json", str(path),
    ]
    try:
        payload = json.loads(_run(command).stdout)
    except json.JSONDecodeError as exc:
        raise GoldenError(f"ffprobe returned invalid JSON for {path}") from exc
    raw_streams = [dict(item) for item in payload.get("streams", [])]
    streams = [_stream_summary(item) for item in raw_streams]
    packets = [dict(item) for item in payload.get("packets", [])]
    packet_stats: dict[str, dict[str, int]] = {}
    for stream in streams:
        index = _int(stream.get("index"), -1)
        selected = [packet for packet in packets if _int(packet.get("stream_index"), -2) == index]
        sizes = [_int(packet.get("size")) for packet in selected]
        keyframes = sum(1 for packet in selected if "K" in str(packet.get("flags") or ""))
        packet_stats[str(index)] = {
            "packets": len(selected),
            "bytes": sum(sizes),
            "max_packet_bytes": max(sizes, default=0),
            "keyframes": keyframes,
        }
    format_info = dict(payload.get("format") or {})
    avc_contract: dict[str, object] = {}
    raw_video = next(
        (item for item in raw_streams if item.get("codec_type") == "video"),
        {},
    )
    if raw_video.get("codec_name") == "h264":
        try:
            avc_contract = inspect_avc_contract(
                path, raw_video, packets
            )
        except AppleVideoContractError as exc:
            avc_contract = {"error": str(exc)}
    return {
        "path": path.name,
        "size": path.stat().st_size,
        "sha256": _hash_file(path),
        "format": format_info,
        "streams": streams,
        "packet_stats": packet_stats,
        "atoms": _top_level_atoms(path),
        "ftyp": _ftyp(path),
        "avc_contract": avc_contract,
    }


def _case_output(output_dir: Path, case_id: str) -> Path:
    matches = [
        path for suffix in (".m4v", ".mp4", ".mov")
        for path in [output_dir / f"{case_id}{suffix}"] if path.is_file()
    ]
    if len(matches) != 1:
        raise GoldenError(
            f"expected exactly one Apple output named {case_id}.m4v/.mp4/.mov"
        )
    return matches[0]


def validate_ipod_output(movie: dict[str, object]) -> list[str]:
    failures: list[str] = []
    streams = list(movie.get("streams") or [])
    video = next((item for item in streams if item.get("codec_type") == "video"), {})
    audio = next((item for item in streams if item.get("codec_type") == "audio"), {})
    if video.get("codec_name") != "h264":
        failures.append("video is not H.264")
    if "baseline" not in str(video.get("profile") or "").lower():
        failures.append("video is not H.264 Baseline")
    level = _int(video.get("level"))
    if level <= 0 or level > 30:
        failures.append("H.264 level is not <= 3.0")
    if _int(video.get("width")) > 640 or _int(video.get("height")) > 480:
        failures.append("coded dimensions exceed 640x480")
    if _fraction(video.get("r_frame_rate")) > 30.01:
        failures.append("frame rate exceeds 30 fps")
    if _int(video.get("has_b_frames")) != 0:
        failures.append("B-frames are present")
    if _int(video.get("refs"), 1) > 1:
        failures.append("more than one reference picture is present")
    if video.get("pix_fmt") != "yuv420p":
        failures.append("pixel format is not yuv420p")
    if audio:
        if audio.get("codec_name") != "aac":
            failures.append("audio is not AAC")
        if str(audio.get("profile") or "").upper() != "LC":
            failures.append("audio is not AAC-LC")
        if _int(audio.get("sample_rate")) > 48000:
            failures.append("audio rate exceeds 48 kHz")
        if _int(audio.get("channels")) > 2:
            failures.append("audio has more than two channels")
    atom_names = [str(item.get("type") or "") for item in movie.get("atoms") or []]
    for required in ("ftyp", "moov", "mdat"):
        if required not in atom_names:
            failures.append(f"missing {required} atom")
    if "moof" in atom_names:
        failures.append("fragmented MP4 is not supported")
    if video:
        contract = dict(movie.get("avc_contract") or {})
        if contract.get("error"):
            failures.append(f"H.264 syntax inspection failed: {contract['error']}")
        else:
            failures.extend(
                apple_avc_contract_failures(
                    contract,
                    _int(video.get("width")),
                    _int(video.get("height")),
                    _int(video.get("level")),
                )
            )
    return failures


def generate_corpus(output_dir: Path, ffmpeg: str, seconds: int) -> dict[str, object]:
    output_dir.mkdir(parents=True, exist_ok=True)
    cases: list[dict[str, object]] = []
    for definition in CORPUS_CASES:
        case = dict(definition)
        case_id = str(case["id"])
        path = output_dir / f"{case_id}.mov"
        option_join = ":" if "=" in str(case["pattern"]) else "="
        video_filter = (
            f"{case['pattern']}{option_join}size={case['width']}x{case['height']}:"
            f"rate={case['rate']}:duration={seconds}"
        )
        command = [ffmpeg, "-y", "-loglevel", "error", "-f", "lavfi", "-i", video_filter]
        if case["audio"] != "silent":
            command.extend([
                "-f", "lavfi", "-i",
                f"sine=frequency=997:sample_rate=48000:duration={seconds}",
            ])
        command.extend([
            "-map", "0:v:0", "-c:v", "mjpeg", "-q:v", "2",
            "-pix_fmt", "yuvj420p", "-metadata", f"title={case_id}",
        ])
        if case["audio"] == "silent":
            command.append("-an")
        else:
            channels = "1" if case["audio"] == "mono" else "2"
            command.extend([
                "-map", "1:a:0", "-c:a", "pcm_s16le", "-ar", "48000",
                "-ac", channels, "-shortest",
            ])
        command.extend(["-f", "mov", str(path)])
        _run(command)
        case.update({
            "source_file": path.name,
            "source_size": path.stat().st_size,
            "source_sha256": _hash_file(path),
        })
        cases.append(case)
    manifest = {
        "schema": SCHEMA,
        "kind": "source-corpus",
        "generator": {"ffmpeg": ffmpeg, "duration_seconds": seconds},
        "reserved_sync_test_url": RESERVED_SYNC_TEST_URL,
        "reserved_sync_test_downloaded": False,
        "cases": cases,
    }
    manifest_path = output_dir / "sources.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return manifest


def analyze_outputs(
    source_dir: Path,
    output_dir: Path,
    ffprobe: str,
    installer: Path,
    quicktime_h264: Path,
    attested: bool,
) -> dict[str, object]:
    source_manifest_path = source_dir / "sources.json"
    if not source_manifest_path.is_file():
        raise GoldenError("source directory has no sources.json")
    source_manifest = json.loads(source_manifest_path.read_text(encoding="utf-8"))
    installer_sha1 = _hash_file(installer, "sha1")
    quicktime_sha256 = _hash_file(quicktime_h264)
    provenance_failures: list[str] = []
    if installer_sha1 not in ITUNES_INSTALLER_SHA1:
        provenance_failures.append("iTunes installer SHA-1 is not an authenticated 9.2.1.5 artifact")
    if quicktime_sha256 != QUICKTIME_H264_SHA256:
        provenance_failures.append("QuickTimeH264.qtx SHA-256 does not match QuickTime 7.6.6")
    if not attested:
        provenance_failures.append(
            "operator did not attest the documented Create iPod or iPhone Version action"
        )

    analyzed_cases: list[dict[str, object]] = []
    all_failures = list(provenance_failures)
    for case in source_manifest.get("cases") or []:
        case_id = str(case.get("id") or "")
        source_path = source_dir / str(case.get("source_file") or "")
        if not source_path.is_file() or _hash_file(source_path) != case.get("source_sha256"):
            raise GoldenError(f"source corpus hash mismatch for {case_id}")
        output_path = _case_output(output_dir, case_id)
        movie = inspect_movie(output_path, ffprobe)
        failures = validate_ipod_output(movie)
        if failures:
            all_failures.extend(f"{case_id}: {failure}" for failure in failures)
        analyzed_cases.append({
            "id": case_id,
            "source_sha256": case.get("source_sha256"),
            "apple_output": movie,
            "failures": failures,
        })
    return {
        "schema": SCHEMA,
        "kind": "apple-output-contract",
        "qualified": not all_failures and len(analyzed_cases) == len(CORPUS_CASES),
        "provenance": {
            "itunes_version": ITUNES_VERSION,
            "quicktime_version": QUICKTIME_VERSION,
            "itunes_installer_sha1": installer_sha1,
            "quicktime_h264_sha256": quicktime_sha256,
            "operator_attestation": (
                "Advanced > Create iPod or iPhone Version"
                if attested else ""
            ),
        },
        "reserved_sync_test_url": RESERVED_SYNC_TEST_URL,
        "reserved_sync_test_downloaded": False,
        "failures": all_failures,
        "cases": analyzed_cases,
    }


def verify_manifest(manifest_path: Path, source_dir: Path, output_dir: Path) -> list[str]:
    payload = json.loads(manifest_path.read_text(encoding="utf-8"))
    failures: list[str] = []
    if payload.get("schema") != SCHEMA or payload.get("kind") != "apple-output-contract":
        failures.append("manifest schema/kind is invalid")
    if not payload.get("qualified"):
        failures.append("manifest is not qualified")
    if payload.get("reserved_sync_test_downloaded") is not False:
        failures.append("reserved YouTube test must remain untouched before qualification")
    provenance = dict(payload.get("provenance") or {})
    if provenance.get("itunes_installer_sha1") not in ITUNES_INSTALLER_SHA1:
        failures.append("iTunes installer provenance is invalid")
    if provenance.get("quicktime_h264_sha256") != QUICKTIME_H264_SHA256:
        failures.append("QuickTime encoder provenance is invalid")
    if provenance.get("operator_attestation") != "Advanced > Create iPod or iPhone Version":
        failures.append("Apple menu-action attestation is missing")
    cases = list(payload.get("cases") or [])
    if {item.get("id") for item in cases} != {item["id"] for item in CORPUS_CASES}:
        failures.append("golden corpus case set is incomplete")
    for case in cases:
        case_id = str(case.get("id") or "")
        source_path = source_dir / f"{case_id}.mov"
        if not source_path.is_file() or _hash_file(source_path) != case.get("source_sha256"):
            failures.append(f"{case_id}: source hash mismatch")
        movie = dict(case.get("apple_output") or {})
        output_path = output_dir / str(movie.get("path") or "")
        if not output_path.is_file() or _hash_file(output_path) != movie.get("sha256"):
            failures.append(f"{case_id}: Apple output hash mismatch")
        failures.extend(f"{case_id}: {item}" for item in validate_ipod_output(movie))
    return failures


def _first_stream(movie: dict[str, object], codec_type: str) -> dict[str, object]:
    return next(
        (
            dict(item) for item in movie.get("streams") or []
            if item.get("codec_type") == codec_type
        ),
        {},
    )


def compare_movie_contract(
    apple: dict[str, object], candidate: dict[str, object]
) -> tuple[list[str], dict[str, object]]:
    """Compare observable compatibility fields, never encoded bytes.

    A modern encoder is allowed to produce different pictures, packet sizes,
    and metadata values.  Geometry, timing, decoder syntax, audio layout,
    brands, and atom ordering must match the measured Apple contract before a
    recipe can be called Apple-equivalent.
    """
    failures = validate_ipod_output(candidate)
    apple_video = _first_stream(apple, "video")
    candidate_video = _first_stream(candidate, "video")
    apple_audio = _first_stream(apple, "audio")
    candidate_audio = _first_stream(candidate, "audio")
    exact_video_fields = (
        "profile", "level", "width", "height", "pix_fmt", "has_b_frames",
        "refs", "sample_aspect_ratio", "r_frame_rate", "avg_frame_rate",
        "time_base",
    )
    for field in exact_video_fields:
        if apple_video.get(field) != candidate_video.get(field):
            failures.append(
                f"video {field} differs: Apple={apple_video.get(field)!r}, "
                f"candidate={candidate_video.get(field)!r}"
            )
    if bool(apple_audio) != bool(candidate_audio):
        failures.append("audio-track presence differs")
    elif apple_audio:
        for field in (
            "codec_name", "profile", "sample_rate", "channels",
            "channel_layout", "time_base",
        ):
            if apple_audio.get(field) != candidate_audio.get(field):
                failures.append(
                    f"audio {field} differs: Apple={apple_audio.get(field)!r}, "
                    f"candidate={candidate_audio.get(field)!r}"
                )
    apple_atoms = [str(item.get("type") or "") for item in apple.get("atoms") or []]
    candidate_atoms = [str(item.get("type") or "") for item in candidate.get("atoms") or []]
    if apple_atoms != candidate_atoms:
        failures.append(
            f"top-level atom order differs: Apple={apple_atoms!r}, "
            f"candidate={candidate_atoms!r}"
        )
    if apple.get("ftyp") != candidate.get("ftyp"):
        failures.append("ftyp brand contract differs")
    apple_order = [item.get("codec_type") for item in apple.get("streams") or []]
    candidate_order = [item.get("codec_type") for item in candidate.get("streams") or []]
    if apple_order != candidate_order:
        failures.append(
            f"track order differs: Apple={apple_order!r}, candidate={candidate_order!r}"
        )
    for label, apple_stream, candidate_stream in (
        ("video", apple_video, candidate_video),
        ("audio", apple_audio, candidate_audio),
    ):
        if not apple_stream:
            continue
        apple_handler = (apple_stream.get("tags") or {}).get("handler_name")
        candidate_handler = (candidate_stream.get("tags") or {}).get("handler_name")
        if apple_handler != candidate_handler:
            failures.append(
                f"{label} handler differs: Apple={apple_handler!r}, "
                f"candidate={candidate_handler!r}"
            )

    def bit_rate(movie: dict[str, object], stream: dict[str, object]) -> int:
        value = _int(stream.get("bit_rate"))
        if value:
            return value
        return _int((movie.get("format") or {}).get("bit_rate"))

    apple_rate = bit_rate(apple, apple_video)
    candidate_rate = bit_rate(candidate, candidate_video)
    measurements = {
        "apple_video_bit_rate": apple_rate,
        "candidate_video_bit_rate": candidate_rate,
        "candidate_to_apple_rate": (
            candidate_rate / apple_rate if apple_rate and candidate_rate else None
        ),
        "apple_size": _int(apple.get("size")),
        "candidate_size": _int(candidate.get("size")),
    }
    return failures, measurements


def compare_candidate_outputs(
    manifest_path: Path, candidate_dir: Path, ffprobe: str
) -> dict[str, object]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if not manifest.get("qualified"):
        raise GoldenError("Apple output manifest is not qualified")
    results: list[dict[str, object]] = []
    all_failures: list[str] = []
    for case in manifest.get("cases") or []:
        case_id = str(case.get("id") or "")
        candidate_path = _case_output(candidate_dir, case_id)
        candidate = inspect_movie(candidate_path, ffprobe)
        failures, measurements = compare_movie_contract(
            dict(case.get("apple_output") or {}), candidate
        )
        all_failures.extend(f"{case_id}: {item}" for item in failures)
        results.append(
            {
                "id": case_id,
                "candidate": candidate,
                "measurements": measurements,
                "failures": failures,
            }
        )
    return {
        "schema": SCHEMA,
        "kind": "rockpod-apple-contract-comparison",
        "qualified": not all_failures and len(results) == len(CORPUS_CASES),
        "apple_manifest_sha256": _hash_file(manifest_path),
        "failures": all_failures,
        "cases": results,
    }


def generate_candidate_outputs(
    source_dir: Path,
    candidate_dir: Path,
    ffmpeg: str,
    x264: str,
) -> dict[str, object]:
    """Run RockPod's production converter over every golden-corpus input."""
    source_manifest_path = source_dir / "sources.json"
    if not source_manifest_path.is_file():
        raise GoldenError("source directory has no sources.json")
    source_manifest = json.loads(source_manifest_path.read_text(encoding="utf-8"))
    cases = list(source_manifest.get("cases") or [])
    if {item.get("id") for item in cases} != {item["id"] for item in CORPUS_CASES}:
        raise GoldenError("source corpus case set is incomplete")
    candidate_dir.mkdir(parents=True, exist_ok=True)
    transcoder = VideoRvpTranscoder(
        str(candidate_dir / ".cache"),
        ffmpeg_path=ffmpeg,
        x264_path=x264,
        profile="h264_apple_exact",
    )
    outputs: list[dict[str, object]] = []
    for case in cases:
        case_id = str(case.get("id") or "")
        source = source_dir / str(case.get("source_file") or f"{case_id}.mov")
        if not source.is_file() or _hash_file(source) != case.get("source_sha256"):
            raise GoldenError(f"source corpus hash mismatch for {case_id}")
        row, _info = transcoder.prepare_track_for_sync(
            {
                "file_path": str(source),
                "title": case_id,
                "artist": "",
                "album": "Apple exact corpus",
                "album_artist": "",
                "duration": 0,
                "media_type": "video",
                "file_hash": case.get("source_sha256"),
            },
            "apple-golden",
        )
        target = candidate_dir / f"{case_id}.m4v"
        shutil.copyfile(str(row["sync_source_path"]), target)
        outputs.append(
            {
                "id": case_id,
                "path": target.name,
                "size": target.stat().st_size,
                "sha256": _hash_file(target),
            }
        )
    return {"profile": "h264_apple_exact", "cases": outputs}


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    generate = subparsers.add_parser("generate", help="create deterministic VM input movies")
    generate.add_argument("output_dir", type=Path)
    generate.add_argument("--ffmpeg", default="")
    generate.add_argument("--seconds", type=int, default=4)

    analyze = subparsers.add_parser("analyze", help="inspect authentic iTunes outputs")
    analyze.add_argument("source_dir", type=Path)
    analyze.add_argument("apple_output_dir", type=Path)
    analyze.add_argument("manifest", type=Path)
    analyze.add_argument("--ffprobe", default="")
    analyze.add_argument("--itunes-installer", required=True, type=Path)
    analyze.add_argument("--quicktime-h264", required=True, type=Path)
    analyze.add_argument("--attest-apple-menu", action="store_true")

    verify = subparsers.add_parser("verify", help="rehash and revalidate a qualified contract")
    verify.add_argument("manifest", type=Path)
    verify.add_argument("source_dir", type=Path)
    verify.add_argument("apple_output_dir", type=Path)

    compare = subparsers.add_parser(
        "compare", help="compare RockPod outputs with a qualified Apple contract"
    )
    compare.add_argument("manifest", type=Path)
    compare.add_argument("candidate_dir", type=Path)
    compare.add_argument("report", type=Path)
    compare.add_argument("--ffprobe", default="")

    candidate = subparsers.add_parser(
        "candidate", help="encode the corpus with RockPod's production profile"
    )
    candidate.add_argument("source_dir", type=Path)
    candidate.add_argument("candidate_dir", type=Path)
    candidate.add_argument("--ffmpeg", default="")
    candidate.add_argument("--x264", default="")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    try:
        if args.command == "generate":
            if args.seconds < 1 or args.seconds > 30:
                raise GoldenError("--seconds must be between 1 and 30")
            result = generate_corpus(
                args.output_dir, _find_tool(args.ffmpeg, "ffmpeg"), args.seconds
            )
            print(json.dumps({"qualified": False, "cases": len(result["cases"])}, sort_keys=True))
            return 0
        if args.command == "analyze":
            result = analyze_outputs(
                args.source_dir,
                args.apple_output_dir,
                _find_tool(args.ffprobe, "ffprobe"),
                args.itunes_installer,
                args.quicktime_h264,
                args.attest_apple_menu,
            )
            args.manifest.parent.mkdir(parents=True, exist_ok=True)
            args.manifest.write_text(
                json.dumps(result, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
            print(json.dumps({"qualified": result["qualified"], "failures": result["failures"]}, sort_keys=True))
            return 0 if result["qualified"] else 2
        if args.command == "verify":
            failures = verify_manifest(args.manifest, args.source_dir, args.apple_output_dir)
            print(json.dumps({"qualified": not failures, "failures": failures}, sort_keys=True))
            return 0 if not failures else 2
        if args.command == "candidate":
            result = generate_candidate_outputs(
                args.source_dir,
                args.candidate_dir,
                _find_tool(args.ffmpeg, "ffmpeg"),
                _find_tool(args.x264 or local_x264_path(), "x264-apple-ipod"),
            )
            print(json.dumps(result, sort_keys=True))
            return 0
        result = compare_candidate_outputs(
            args.manifest, args.candidate_dir,
            _find_tool(args.ffprobe, "ffprobe"),
        )
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(
            json.dumps(result, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        print(json.dumps({"qualified": result["qualified"], "failures": result["failures"]}, sort_keys=True))
        return 0 if result["qualified"] else 2
    except (GoldenError, OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"apple-video-golden: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
