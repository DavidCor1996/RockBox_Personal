#!/usr/bin/env python3
"""Verify Club Penguin ambient-music assets and playback lifecycle."""

from __future__ import annotations

import argparse
import array
import math
import re
import sys
import zipfile
from pathlib import Path


RATE = 22050
RUNTIME_GAIN = 3 / 5


def function(source: str, name: str) -> str:
    match = re.search(
        rf"static (?:bool|void) {name}\([^)]*\)\n\{{(.*?)\n\}}",
        source,
        re.S,
    )
    if not match:
        raise SystemExit(f"FAIL missing {name}()")
    return match.group(1)


def check_pcm(path: Path, expected_seconds: int | None) -> None:
    data = path.read_bytes()
    if not data or len(data) % 2:
        raise SystemExit(f"FAIL {path}: invalid signed 16-bit PCM length")
    samples = array.array("h")
    samples.frombytes(data)
    if sys.byteorder != "little":
        samples.byteswap()
    duration = len(samples) / RATE
    if expected_seconds is not None and abs(duration - expected_seconds) > 0.01:
        raise SystemExit(
            f"FAIL {path}: expected {expected_seconds}s, got {duration:.3f}s"
        )
    peak = max(abs(sample) for sample in samples) / 32768
    rms = math.sqrt(sum(sample * sample for sample in samples) / len(samples))
    rms_db = 20 * math.log10(max(rms / 32768, 1e-12))
    runtime_peak = peak * RUNTIME_GAIN
    if runtime_peak >= 0.5:
        raise SystemExit(
            f"FAIL {path}: runtime peak {runtime_peak:.3f} is too high"
        )
    print(
        f"PASS {path.name}: {duration:.2f}s, source RMS {rms_db:.1f} dBFS, "
        f"runtime peak {runtime_peak:.3f}"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--zip", type=Path, required=True)
    args = parser.parse_args()

    check_pcm(args.assets / "music" / "town_lofi.pcm", 60)
    check_pcm(args.assets / "music" / "hockey_lofi.pcm", 60)
    check_pcm(args.assets / "music" / "emma_sewer_set.pcm", None)
    check_pcm(args.assets / "concert" / "song.pcm", None)

    source = args.source.read_text()
    if "#define CP_MUSIC_AMPLITUDE (MIX_AMP_UNITY * 3 / 5)" not in source:
        raise SystemExit("FAIL plugin music amplitude is not bounded at 60%")

    open_audio = function(source, "cp_music_open_audio")
    for required in (
        "plugin_get_audio_buffer",
        "PCM_MIXER_CHAN_PLAYBACK",
        "CP_MUSIC_AMPLITUDE",
        "pcmbuf_fade(false, true)",
    ):
        if required not in open_audio:
            raise SystemExit(f"FAIL cp_music_open_audio lacks {required}")
    for forbidden in ("audio_stop", "playlist_", "core_alloc"):
        if forbidden in open_audio:
            raise SystemExit(f"FAIL cp_music_open_audio uses {forbidden}")

    close_audio = function(source, "cp_sound_close_audio")
    stop = close_audio.find("cp_sound_stop_channel()")
    release = close_audio.find("plugin_release_audio_buffer()")
    if stop < 0 or release < 0 or stop > release:
        raise SystemExit("FAIL audio buffer can be released before channel stop")

    routing = function(source, "cp_music_switch_for_room")
    for required in (
        'cp_streq(room_id, "stage")',
        'cp_streq(room_id, "stadium")',
        'cp_streq(room_id, "emma_sewer")',
        "cp_night_city_strip(room_id)",
        "CP_TOWN_AUDIO_FILE",
        "CP_HOCKEY_AUDIO_FILE",
        "CP_CONCERT_AUDIO_FILE",
        "CP_EMMA_CONCERT_AUDIO_FILE",
    ):
        if required not in routing:
            raise SystemExit(f"FAIL room-music routing lacks {required}")

    expected = {
        ".rockbox/rocks/games/clubpenguin/music/town_lofi.pcm",
        ".rockbox/rocks/games/clubpenguin/music/hockey_lofi.pcm",
        ".rockbox/rocks/games/clubpenguin/music/emma_sewer_set.pcm",
        ".rockbox/rocks/games/clubpenguin/music/source.manifest",
    }
    with zipfile.ZipFile(args.zip) as archive:
        names = set(archive.namelist())
    missing = expected - names
    if missing:
        raise SystemExit(f"FAIL package lacks {sorted(missing)}")

    print("PASS room routing, 60% mixer gain, lifecycle, and ZIP contents")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
