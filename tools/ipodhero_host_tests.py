#!/usr/bin/env python3
"""Deterministic host gates for iPod Hero chart conversion."""

from __future__ import annotations

import argparse
from array import array
from pathlib import Path
import math
import struct
import sys
import tempfile


REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tools"))

import ipodhero_chart as chart  # noqa: E402
import ipodhero_generate as generator  # noqa: E402


def vlq(value: int) -> bytes:
    encoded = [value & 0x7F]
    value >>= 7
    while value:
        encoded.append(0x80 | (value & 0x7F))
        value >>= 7
    return bytes(reversed(encoded))


def midi_track(events: list[tuple[int, bytes]]) -> bytes:
    output = bytearray()
    previous = 0
    for tick, event in sorted(events, key=lambda item: item[0]):
        output.extend(vlq(tick - previous))
        output.extend(event)
        previous = tick
    output.extend(b"\x00\xff\x2f\x00")
    return b"MTrk" + struct.pack(">I", len(output)) + output


def midi_fixture() -> bytes:
    tempo = midi_track([
        (0, b"\xff\x51\x03\x07\xa1\x20"),
        (384, b"\xff\x51\x03\x06\x1a\x80"),
    ])
    guitar = midi_track([
        (0, b"\xff\x03\x0bPART GUITAR"),
        (192, b"\x90\x60\x64"),
        (192, b"\x80\x60\x00"),
        (384, b"\x90\x61\x64"),
        (384, b"\x90\x62\x64"),
        (480, b"\x80\x61\x00"),
        (480, b"\x80\x62\x00"),
        (576, b"\x90\x65\x64"),
        (576, b"\x90\x74\x64"),
        (576, b"\x90\x63\x64"),
        (576, b"\x80\x65\x00"),
        (576, b"\x80\x63\x00"),
        (768, b"\x90\x64\x64"),
        (768, b"\x80\x64\x00"),
        (960, b"\x80\x74\x00"),
    ])
    events = midi_track([
        (0, b"\xff\x03\x06EVENTS"),
        (0, b"\xff\x01\x0f[section intro]"),
        (384, b"\xff\x01\x0e[section fast]"),
    ])
    return (b"MThd" + struct.pack(">IHHH", 6, 1, 3, 192) +
            tempo + guitar + events)


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def run() -> None:
    sections = []
    reference = chart.load_chart(
        REPO / "apps/plugins/ipodhero/testdata/reference.chart",
        "ExpertSingle",
        sections,
    )
    check([note.time_ms for note in reference] ==
          [500, 1000, 1500, 2000, 2400, 2800],
          "tempo-map integration differs from the 1 ms reference")
    check(reference[1].lanes == 0x06, "simultaneous notes did not merge")
    check(reference[2].duration_ms == 500, "sustain duration was not integrated")
    check(reference[2].flags & chart.FLAGS["star"], "star phrase was lost")
    check(reference[5].flags & chart.FLAGS["phrase_end"],
          "star phrase end was not retained")
    check([(section.time_ms, section.name) for section in sections] ==
          [(0, "Intro"), (2000, "Fast Part")],
          "section markers were not retained")

    with tempfile.TemporaryDirectory(prefix="ipodhero-tests-") as directory:
        root = Path(directory)
        midi = root / "notes.mid"
        midi.write_bytes(midi_fixture())
        midi_sections = []
        imported = chart.load_midi(midi, "PART GUITAR", "expert",
                                   midi_sections)
        check([note.time_ms for note in imported] ==
              [500, 1000, 1400, 1800],
              "MIDI tempo integration differs from the reference")
        check(imported[1].lanes == 0x06, "MIDI chord did not merge")
        check(imported[2].flags & chart.FLAGS["forced"],
              "MIDI forced marker was lost")
        check(imported[2].flags & chart.FLAGS["star"],
              "MIDI star phrase was lost")
        check(imported[3].flags & chart.FLAGS["phrase_end"],
              "MIDI star phrase end was lost")
        check([(section.time_ms, section.name) for section in midi_sections] ==
              [(0, "intro"), (1000, "fast")],
              "MIDI section markers were lost")

        notes = root / "offset.notes.tsv"
        notes.write_text("100\t0\tg\t-\t0\n", encoding="utf-8")
        output = root / "offset.ihc"
        args = argparse.Namespace(
            chart=None,
            midi=None,
            notes=notes,
            chart_track="ExpertSingle",
            midi_track="PART GUITAR",
            output=output,
            song_length_ms=1000,
            audio_offset_ms=-125,
            source_crc32=0x12345678,
            source_size=4321,
            difficulty="expert",
            origin="authored",
        )
        chart.compile_chart(args)
        compiled = output.read_bytes()
        check(compiled[:4] == b"IHC1" and len(compiled) == 52,
              "compiled chart framing is wrong")
        check(struct.unpack_from("<i", compiled, 20)[0] == -125,
              "signed chart offset was not retained")
        check(struct.unpack_from("<I", compiled, 36)[0] ==
              chart.rockbox_crc32(compiled[40:]),
              "compiled payload CRC is wrong")

        audio = root / "song.bin"
        audio.write_bytes(bytes(range(256)) * 600)
        size, identity = chart.audio_identity(audio)
        args.source_size = size
        args.source_crc32 = identity
        args.device_path = "/Music/Test Song.flac"
        args.device_chart_path = "charts/test-expert.ihc"
        args.title = "Test Song"
        args.artist = "Test Artist"
        args.skin_id = "live-stage"
        args.index_output = root / "index.tsv"
        chart.update_index(args)
        row = args.index_output.read_text(encoding="utf-8").splitlines()[1]
        fields = row.split("\t")
        check(len(fields) == 11 and fields[0] == args.device_path,
              "index row structure is wrong")
        check(fields[1] == str(size) and fields[3] == f"{identity:08x}",
              "index row lost exact audio identity")
        check(fields[9] == args.device_chart_path,
              "index row put the chart in the wrong difficulty")

    samples = array("h", [0]) * (generator.RATE * 12)
    for beat in range(2, 23):
        start = beat * generator.RATE // 2
        for offset in range(generator.RATE // 20):
            envelope = 1.0 - offset / (generator.RATE // 20)
            samples[start + offset] = round(
                12000 * envelope * math.sin(2 * math.pi * 440 * offset /
                                             generator.RATE)
            )
    generated, analysis = generator.generated_notes(samples)
    check(110 <= analysis.tempo_bpm <= 130,
          "generator tempo estimate missed the 120 BPM fixture")
    check(len(generated) >= 18, "generator lost musical transients")
    for difficulty, limit in (("easy", 2.0), ("medium", 3.5), ("hard", 5.5)):
        derived = generator.derive(generated, difficulty)
        density = len(derived) / 12
        check(density <= limit + 0.01,
              f"{difficulty} generated density exceeds its limit")

    print("iPod Hero host chart gates passed")


if __name__ == "__main__":
    run()
