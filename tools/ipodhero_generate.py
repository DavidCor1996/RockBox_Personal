#!/usr/bin/env python3
"""Generate honestly labeled, reviewable iPod Hero charts from local audio.

This host-only analyzer combines transient spectral flux, tempo estimation,
beat-grid confidence and coarse pitch motion. It never runs on the iPod and
does not pretend its output is equivalent to a human-authored chart.
"""

from __future__ import annotations

import argparse
from array import array
from dataclasses import asdict, dataclass
import json
import math
from pathlib import Path
import re
import subprocess
import sys


REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tools"))

import ipodhero_chart as chart  # noqa: E402


RATE = 8000
FRAME = 512
HOP = 128


@dataclass
class Analysis:
    tempo_bpm: float
    first_beat_ms: int
    analyzed_onsets: int
    median_strength: float
    warning: str = "GENERATED - HUMAN TIMING AND MUSICAL REVIEW REQUIRED"


def percentile(values: list[float], fraction: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, round((len(ordered) - 1) * fraction)))
    return ordered[index]


def decode_audio(path: Path) -> array:
    command = [
        "ffmpeg", "-v", "error", "-i", str(path), "-vn", "-ac", "1",
        "-ar", str(RATE), "-f", "s16le", "-",
    ]
    result = subprocess.run(command, check=True, stdout=subprocess.PIPE)
    samples = array("h")
    samples.frombytes(result.stdout)
    if sys.byteorder != "little":
        samples.byteswap()
    if len(samples) < RATE:
        raise ValueError("audio is shorter than one second")
    return samples


def audio_length_ms(path: Path) -> int:
    result = subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "format=duration",
         "-of", "default=noprint_wrappers=1:nokey=1", str(path)],
        check=True, text=True, stdout=subprocess.PIPE,
    )
    return round(float(result.stdout.strip()) * 1000)


def frame_features(samples: array) -> tuple[list[float], list[float], list[float]]:
    energy = []
    flux = []
    crossing = []
    previous_hfc = 0.0
    for start in range(0, len(samples) - FRAME, HOP):
        block = samples[start:start + FRAME]
        square = 0
        hfc = 0
        zeroes = 0
        previous = block[0]
        for sample in block:
            square += sample * sample
            hfc += abs(sample - previous)
            if (sample < 0) != (previous < 0):
                zeroes += 1
            previous = sample
        rms = math.sqrt(square / FRAME)
        normalized_hfc = hfc / (FRAME * 32768)
        energy.append(math.log1p(rms))
        flux.append(max(0.0, normalized_hfc - previous_hfc))
        crossing.append(zeroes / FRAME)
        previous_hfc = normalized_hfc
    return energy, flux, crossing


def onset_envelope(energy: list[float], flux: list[float]) -> list[float]:
    envelope = [0.0] * len(energy)
    for index in range(2, len(energy)):
        energy_rise = max(0.0, energy[index] - energy[index - 2])
        envelope[index] = energy_rise + 5.0 * flux[index]
    return envelope


def pick_onsets(envelope: list[float]) -> list[int]:
    global_floor = percentile(envelope, 0.65)
    peaks = []
    minimum_frames = max(1, round(0.080 * RATE / HOP))
    for index in range(2, len(envelope) - 2):
        local = envelope[max(0, index - 31):index + 1]
        threshold = percentile(local, 0.65) * 1.35 + global_floor * 0.30
        if envelope[index] <= threshold or envelope[index] == 0:
            continue
        if envelope[index] < max(envelope[index - 2:index + 3]):
            continue
        if peaks and index - peaks[-1] < minimum_frames:
            if envelope[index] > envelope[peaks[-1]]:
                peaks[-1] = index
            continue
        peaks.append(index)
    if len(peaks) < 8:
        raise ValueError("not enough confident musical transients")
    return peaks


def estimate_tempo(envelope: list[float], peaks: list[int]) -> float:
    sparse = [0.0] * len(envelope)
    for index in peaks:
        sparse[index] = envelope[index]
    best_bpm = 120.0
    best_score = -1.0
    for bpm in range(60, 201):
        lag = round(60 * RATE / (bpm * HOP))
        if lag <= 0:
            continue
        score = sum(sparse[index] * sparse[index - lag]
                    for index in peaks if index >= lag)
        score /= 1.0 + abs(bpm - 120) / 240
        if score > best_score:
            best_score = score
            best_bpm = float(bpm)
    while best_bpm < 85:
        best_bpm *= 2
    while best_bpm > 175:
        best_bpm /= 2
    return best_bpm


def generated_notes(samples: array) -> tuple[list[chart.Note], Analysis]:
    energy, flux, crossing = frame_features(samples)
    envelope = onset_envelope(energy, flux)
    peaks = pick_onsets(envelope)
    tempo = estimate_tempo(envelope, peaks)
    beat_ms = 60_000 / tempo
    subdivision = beat_ms / 4
    first_ms = round(peaks[0] * HOP * 1000 / RATE)
    strengths = [envelope[index] for index in peaks]
    median_strength = percentile(strengths, 0.5)
    strong = percentile(strengths, 0.82)
    notes = []
    previous_lane = -1
    for number, peak in enumerate(peaks):
        raw_ms = peak * HOP * 1000 / RATE
        grid = first_ms + round((raw_ms - first_ms) / subdivision) * subdivision
        error = abs(raw_ms - grid)
        time_ms = round(grid if error <= min(70, subdivision * 0.36) else raw_ms)
        zcr = crossing[peak]
        lane = max(0, min(4, int((zcr - 0.015) / 0.035)))
        if lane == previous_lane and number % 3:
            lane = (lane + (1 if number % 2 else 4)) % 5
        previous_lane = lane
        lane_mask = 1 << lane
        if envelope[peak] >= strong and number % 5 == 0:
            lane_mask |= 1 << ((lane + 2) % 5)
        next_ms = (peaks[number + 1] * HOP * 1000 / RATE
                   if number + 1 < len(peaks) else raw_ms)
        duration = 0
        if next_ms - raw_ms >= 700 and envelope[peak] >= median_strength:
            duration = min(1200, max(0, round(next_ms - raw_ms - 140)))
        flags = chart.FLAGS["generated"]
        if number and time_ms - notes[-1].time_ms <= beat_ms / 3 and \
                lane_mask & (lane_mask - 1) == 0:
            flags |= chart.FLAGS["hopo"]
        phrase = number // 16 + 1
        if number % 16 < 8:
            flags |= chart.FLAGS["star"]
            if number % 16 == 7:
                flags |= chart.FLAGS["phrase_end"]
        else:
            phrase = 0
        if not notes or time_ms > notes[-1].time_ms:
            notes.append(chart.Note(time_ms, duration, lane_mask, flags, phrase))
    return notes, Analysis(tempo, first_ms, len(peaks), median_strength)


def derive(notes: list[chart.Note], difficulty: str) -> list[chart.Note]:
    minimum_gap = {"easy": 500, "medium": 286, "hard": 182, "expert": 80}[difficulty]
    lane_count = {"easy": 3, "medium": 4, "hard": 5, "expert": 5}[difficulty]
    result = []
    for source in notes:
        if result and source.time_ms - result[-1].time_ms < minimum_gap:
            continue
        lanes = source.lanes
        selected = [lane for lane in range(5) if lanes & (1 << lane)]
        mapped = [min(lane, lane_count - 1) for lane in selected]
        if difficulty == "easy":
            mapped = mapped[:1]
        elif difficulty == "medium":
            mapped = mapped[:1]
        elif difficulty == "hard":
            mapped = mapped[:2]
        lane_mask = 0
        for lane in mapped:
            lane_mask |= 1 << lane
        flags = source.flags
        if difficulty in ("easy", "medium"):
            flags &= ~chart.FLAGS["hopo"]
        result.append(chart.Note(source.time_ms, source.duration_ms,
                                 lane_mask, flags, source.phrase))
    phrase_members: dict[int, list[chart.Note]] = {}
    for note in result:
        note.flags &= ~chart.FLAGS["phrase_end"]
        if note.flags & chart.FLAGS["star"] and note.phrase:
            phrase_members.setdefault(note.phrase, []).append(note)
    for members in phrase_members.values():
        members[-1].flags |= chart.FLAGS["phrase_end"]
    return result


def slug(text: str) -> str:
    value = re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")
    return value or "song"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--audio", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--audio-offset-ms", type=int, default=0)
    parser.add_argument("--title", required=True)
    parser.add_argument("--artist", required=True)
    parser.add_argument("--index-output", type=Path)
    parser.add_argument("--device-path")
    parser.add_argument("--skin-id", default="live-stage")
    parser.add_argument("--confirm-audio-identity", action="store_true")
    args = parser.parse_args()
    if args.index_output and (not args.device_path or
                              not args.confirm_audio_identity):
        parser.error("index output requires --device-path and explicit audio confirmation")

    samples = decode_audio(args.audio)
    length_ms = audio_length_ms(args.audio)
    source_size, source_crc = chart.audio_identity(args.audio)
    expert, analysis = generated_notes(samples)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    base = slug(f"{args.artist}-{args.title}")
    report = {"analysis": asdict(analysis), "charts": {}}
    for difficulty in ("easy", "medium", "hard", "expert"):
        notes = derive(expert, difficulty)
        output = args.output_dir / f"{base}-{difficulty}.ihc"
        namespace = argparse.Namespace(
            chart=None, midi=None, notes=None,
            chart_track="ExpertSingle", midi_track="PART GUITAR",
            output=output, song_length_ms=length_ms,
            audio_offset_ms=args.audio_offset_ms,
            source_crc32=source_crc, source_size=source_size,
            difficulty=difficulty, origin="generated",
            sections=[chart.Section(0, "Intro")] + [
                chart.Section(time_ms, f"Section {number}")
                for number, time_ms in enumerate(
                    range(round(32 * 60_000 / analysis.tempo_bpm),
                          length_ms, round(32 * 60_000 / analysis.tempo_bpm)),
                    1,
                )
            ],
        )
        chart.compile_chart(namespace, notes, namespace.sections)
        report["charts"][difficulty] = {
            "events": len(notes), "path": str(output),
            "average_notes_per_second": round(len(notes) * 1000 / length_ms, 3),
        }
        if args.index_output:
            namespace.index_output = args.index_output
            namespace.device_path = args.device_path
            namespace.device_chart_path = f"charts/{output.name}"
            namespace.title = args.title
            namespace.artist = args.artist
            namespace.skin_id = args.skin_id
            chart.update_index(namespace)
    report_path = args.output_dir / f"{base}-generation-report.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(analysis.warning)
    print(f"estimated tempo: {analysis.tempo_bpm:.1f} BPM")
    print(f"review report: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
