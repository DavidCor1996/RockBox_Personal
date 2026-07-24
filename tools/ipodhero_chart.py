#!/usr/bin/env python3
"""Compile authored TSV or Clone Hero charts into bounded IHC1 charts."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from fractions import Fraction
from pathlib import Path
import os
import struct
import tempfile


LANES = {"g": 1, "r": 2, "y": 4, "b": 8, "o": 16}
FLAGS = {
    "hopo": 1,
    "tap": 2,
    "star": 4,
    "phrase_end": 8,
    "generated": 16,
    "forced": 32,
}
DIFFICULTIES = {"easy": 0, "medium": 1, "hard": 2, "expert": 3}
ORIGINS = {"authored": 0, "imported": 1, "generated": 2}


@dataclass
class Note:
    time_ms: int
    duration_ms: int
    lanes: int
    flags: int
    phrase: int


@dataclass
class Section:
    time_ms: int
    name: str


def tempo_converter(tempos: list[tuple[int, int]], resolution: int):
    if resolution <= 0 or resolution > 7680:
        raise ValueError("chart resolution is out of range")
    by_tick = {0: 120_000}
    for tick, bpm in tempos:
        if tick < 0 or bpm <= 0:
            raise ValueError("invalid tempo map")
        by_tick[tick] = bpm
    ordered = sorted(by_tick.items())
    segments: list[tuple[int, int, Fraction]] = []
    elapsed = Fraction(0)
    for index, (tick, bpm) in enumerate(ordered):
        if index:
            old_tick, old_bpm = ordered[index - 1]
            elapsed += Fraction(
                (tick - old_tick) * 60_000_000,
                old_bpm * resolution,
            )
        segments.append((tick, bpm, elapsed))

    def tick_ms(tick: int) -> int:
        if tick < 0:
            raise ValueError("negative MIDI/chart tick")
        selected_tick, selected_bpm, selected_ms = segments[0]
        for start_tick, bpm, start_ms in segments[1:]:
            if start_tick > tick:
                break
            selected_tick, selected_bpm, selected_ms = start_tick, bpm, start_ms
        value = selected_ms + Fraction(
            (tick - selected_tick) * 60_000_000,
            selected_bpm * resolution,
        )
        return int(value + Fraction(1, 2))

    return tick_ms


def rockbox_crc32(data: bytes, crc: int = 0xFFFFFFFF) -> int:
    for byte in data:
        crc ^= byte << 24
        for _ in range(8):
            if crc & 0x80000000:
                crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFF
    return crc


def audio_identity(path: Path) -> tuple[int, int]:
    size = path.stat().st_size
    with path.open("rb") as handle:
        beginning = handle.read(64 * 1024)
        if size > 64 * 1024:
            handle.seek(max(0, size - 64 * 1024))
            ending = handle.read(64 * 1024)
        else:
            ending = b""
    return size, rockbox_crc32(ending, rockbox_crc32(beginning))


def update_index(args: argparse.Namespace) -> None:
    fields = [
        args.device_path,
        str(args.source_size),
        str(args.song_length_ms),
        f"{args.source_crc32:08x}",
        args.title,
        args.artist,
        "", "", "", "",
        args.skin_id,
    ]
    difficulty_field = 6 + DIFFICULTIES[args.difficulty]
    fields[difficulty_field] = args.device_chart_path
    lines = []
    replaced = False
    if args.index_output.exists():
        lines = args.index_output.read_text(encoding="utf-8").splitlines()
    if not lines:
        lines.append(
            "# path\tsize\tlength_ms\tcrc\ttitle\tartist\t"
            "easy\tmedium\thard\texpert\tskin"
        )
    for number, line in enumerate(lines):
        if not line or line.startswith("#"):
            continue
        current = line.split("\t")
        if len(current) != 11 or current[0] != args.device_path:
            continue
        if current[1:4] != fields[1:4]:
            raise ValueError("existing index row identifies a different audio master")
        current[4] = args.title
        current[5] = args.artist
        current[difficulty_field] = args.device_chart_path
        current[10] = args.skin_id
        lines[number] = "\t".join(current)
        replaced = True
        break
    if not replaced:
        lines.append("\t".join(fields))
    args.index_output.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary_name = tempfile.mkstemp(
        prefix=f".{args.index_output.name}.", dir=args.index_output.parent
    )
    try:
        with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as handle:
            handle.write("\n".join(lines) + "\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary_name, args.index_output)
    except Exception:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise
    print(f"updated {args.index_output}: {args.device_path}")


def parse_lanes(value: str) -> int:
    if value.lower().startswith("0x"):
        mask = int(value, 16)
    else:
        mask = 0
        for lane in value.lower().replace(",", ""):
            if lane not in LANES:
                raise ValueError(f"unknown lane {lane!r}")
            mask |= LANES[lane]
    if mask == 0 or mask & ~0x1F:
        raise ValueError(f"invalid lane mask: {value}")
    return mask


def parse_flags(value: str) -> int:
    if not value or value == "-":
        return 0
    result = 0
    for flag in value.lower().split(","):
        if flag not in FLAGS:
            raise ValueError(f"unknown flag {flag!r}")
        result |= FLAGS[flag]
    return result


def load_notes(path: Path) -> list[Note]:
    notes = []
    for line_number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) not in (3, 4, 5):
            raise ValueError(f"{path}:{line_number}: expected 3-5 tab-separated fields")
        time_ms, duration_ms = int(fields[0]), int(fields[1])
        flags = parse_flags(fields[3]) if len(fields) >= 4 else 0
        phrase = int(fields[4]) if len(fields) == 5 else 0
        if time_ms < 0 or duration_ms < 0 or phrase not in range(65536):
            raise ValueError(f"{path}:{line_number}: value out of range")
        notes.append(Note(time_ms, duration_ms, parse_lanes(fields[2]), flags, phrase))
    notes.sort(key=lambda note: note.time_ms)
    merged: list[Note] = []
    for note in notes:
        if merged and merged[-1].time_ms == note.time_ms:
            current = merged[-1]
            current.lanes |= note.lanes
            current.duration_ms = max(current.duration_ms, note.duration_ms)
            current.flags |= note.flags
            if current.phrase != note.phrase:
                current.phrase = 0
        else:
            merged.append(note)
    if not merged:
        raise ValueError("chart has no notes")
    return merged


def chart_sections(path: Path) -> dict[str, list[str]]:
    sections: dict[str, list[str]] = {}
    current = ""
    for raw in path.read_text(encoding="utf-8-sig").splitlines():
        line = raw.strip()
        if not line or line.startswith("//"):
            continue
        if line.startswith("[") and line.endswith("]"):
            current = line[1:-1]
            sections.setdefault(current, [])
        elif line not in ("{", "}") and current:
            sections[current].append(line)
    return sections


def chart_assignment(line: str) -> tuple[int, list[str]]:
    left, separator, right = line.partition("=")
    if not separator:
        raise ValueError(f"invalid .chart event: {line}")
    return int(left.strip()), right.strip().split()


def load_chart(path: Path, track: str,
               sections_out: list[Section] | None = None) -> list[Note]:
    sections = chart_sections(path)
    resolution = 192
    hopo_frequency = None
    for line in sections.get("Song", []):
        key, separator, value = line.partition("=")
        if separator and key.strip().lower() == "resolution":
            resolution = int(value.strip().strip('"'))
        elif separator and key.strip().lower() == "hopofreq":
            hopo_frequency = int(value.strip().strip('"'))
    if resolution <= 0 or resolution > 7680:
        raise ValueError(".chart resolution is out of range")

    tempos = []
    for line in sections.get("SyncTrack", []):
        tick, event = chart_assignment(line)
        if event and event[0] == "B" and len(event) >= 2:
            tempos.append((tick, int(event[1])))
    tick_ms = tempo_converter(tempos, resolution)
    if sections_out is not None:
        for line in sections.get("Events", []):
            tick, event = chart_assignment(line)
            if not event or event[0] != "E":
                continue
            text = " ".join(event[1:]).strip().strip('"')
            if text.lower().startswith("section "):
                name = text[8:].replace("_", " ").strip()
                if name:
                    sections_out.append(Section(tick_ms(tick), name[:47]))

    note_events: dict[int, list[tuple[int, int]]] = {}
    modifiers: dict[int, int] = {}
    star_ranges: list[tuple[int, int, int]] = []
    phrase = 1
    for line in sections.get(track, []):
        tick, event = chart_assignment(line)
        if len(event) >= 3 and event[0] == "N":
            lane = int(event[1])
            duration = int(event[2])
            if 0 <= lane < 5:
                note_events.setdefault(tick, []).append((lane, duration))
            elif lane == 5:
                modifiers[tick] = modifiers.get(tick, 0) | FLAGS["forced"]
            elif lane == 6:
                modifiers[tick] = modifiers.get(tick, 0) | FLAGS["tap"]
        elif len(event) >= 3 and event[0] == "S" and event[1] == "2":
            duration = int(event[2])
            star_ranges.append((tick, tick + duration, phrase))
            phrase += 1

    notes = []
    phrase_notes: dict[int, list[Note]] = {}
    previous_tick = None
    previous_mask = 0
    hopo_threshold = hopo_frequency or resolution // 3
    for tick in sorted(note_events):
        lane_mask = 0
        end_tick = tick
        for lane, duration in note_events[tick]:
            lane_mask |= 1 << lane
            end_tick = max(end_tick, tick + duration)
        flags = modifiers.get(tick, 0)
        natural_hopo = (
            previous_tick is not None
            and tick - previous_tick <= hopo_threshold
            and lane_mask & (lane_mask - 1) == 0
            and previous_mask & (previous_mask - 1) == 0
            and lane_mask != previous_mask
        )
        if flags & FLAGS["forced"]:
            natural_hopo = not natural_hopo
        if natural_hopo:
            flags |= FLAGS["hopo"]
        phrase_id = 0
        for start, end, candidate in star_ranges:
            if start <= tick < end:
                flags |= FLAGS["star"]
                phrase_id = candidate
                break
        note = Note(
            tick_ms(tick),
            max(0, tick_ms(end_tick) - tick_ms(tick)),
            lane_mask,
            flags,
            phrase_id,
        )
        notes.append(note)
        if phrase_id:
            phrase_notes.setdefault(phrase_id, []).append(note)
        previous_tick = tick
        previous_mask = lane_mask
    for members in phrase_notes.values():
        members[-1].flags |= FLAGS["phrase_end"]
    if not notes:
        raise ValueError(f".chart track [{track}] has no five-lane notes")
    return notes


def midi_vlq(data: bytes, position: int) -> tuple[int, int]:
    value = 0
    for _ in range(4):
        if position >= len(data):
            raise ValueError("truncated MIDI variable-length value")
        byte = data[position]
        position += 1
        value = (value << 7) | (byte & 0x7F)
        if not byte & 0x80:
            return value, position
    raise ValueError("oversized MIDI variable-length value")


def midi_track(data: bytes) -> tuple[str, list[tuple[int, int]],
                                     list[tuple[int, int, int]],
                                     list[tuple[int, str]]]:
    position = 0
    tick = 0
    running = None
    name = ""
    tempos = []
    active: dict[tuple[int, int], list[int]] = {}
    notes: list[tuple[int, int, int]] = []
    texts = []

    while position < len(data):
        delta, position = midi_vlq(data, position)
        tick += delta
        if position >= len(data):
            raise ValueError("truncated MIDI event")
        status = data[position]
        if status & 0x80:
            position += 1
            if status < 0xF0:
                running = status
        elif running is not None:
            status = running
        else:
            raise ValueError("MIDI running status has no prior event")

        if status == 0xFF:
            if position >= len(data):
                raise ValueError("truncated MIDI meta event")
            kind = data[position]
            position += 1
            length, position = midi_vlq(data, position)
            if length > len(data) - position:
                raise ValueError("truncated MIDI meta payload")
            payload = data[position:position + length]
            position += length
            if kind == 0x03:
                name = payload.decode("utf-8", "replace")
            elif kind == 0x51:
                if len(payload) != 3:
                    raise ValueError("invalid MIDI tempo event")
                micros = int.from_bytes(payload, "big")
                if micros == 0:
                    raise ValueError("zero MIDI tempo")
                tempos.append((tick, 60_000_000_000 // micros))
            elif kind in (0x01, 0x06):
                texts.append((tick, payload.decode("utf-8", "replace")))
            elif kind == 0x2F:
                break
            continue
        if status in (0xF0, 0xF7):
            length, position = midi_vlq(data, position)
            if length > len(data) - position:
                raise ValueError("truncated MIDI system event")
            position += length
            continue

        command = status & 0xF0
        channel = status & 0x0F
        length = 1 if command in (0xC0, 0xD0) else 2
        if position + length > len(data):
            raise ValueError("truncated MIDI channel event")
        first = data[position]
        second = data[position + 1] if length == 2 else 0
        position += length
        if command == 0x90 and second > 0:
            active.setdefault((channel, first), []).append(tick)
        elif command in (0x80, 0x90):
            starts = active.get((channel, first), [])
            if starts:
                start = starts.pop(0)
                notes.append((start, tick, first))
    return name, tempos, notes, texts


def load_midi(path: Path, track_name: str, difficulty: str,
              sections_out: list[Section] | None = None) -> list[Note]:
    data = path.read_bytes()
    if len(data) < 14 or data[:4] != b"MThd":
        raise ValueError("not a Standard MIDI file")
    header_length = int.from_bytes(data[4:8], "big")
    if header_length < 6 or 8 + header_length > len(data):
        raise ValueError("invalid MIDI header")
    track_count = int.from_bytes(data[10:12], "big")
    division = int.from_bytes(data[12:14], "big")
    if division & 0x8000 or division == 0:
        raise ValueError("SMPTE-time MIDI is not supported")
    position = 8 + header_length
    tracks = []
    all_tempos = []
    for _ in range(track_count):
        if position + 8 > len(data) or data[position:position + 4] != b"MTrk":
            raise ValueError("missing MIDI track chunk")
        length = int.from_bytes(data[position + 4:position + 8], "big")
        position += 8
        if length > len(data) - position:
            raise ValueError("truncated MIDI track chunk")
        parsed = midi_track(data[position:position + length])
        tracks.append(parsed)
        all_tempos.extend(parsed[1])
        position += length
    selected = next((track for track in tracks
                     if track[0].strip().upper() == track_name.upper()), None)
    if selected is None:
        raise ValueError(f"MIDI track {track_name!r} was not found")

    base = {"easy": 60, "medium": 72, "hard": 84, "expert": 96}[difficulty]
    playable: dict[int, list[tuple[int, int]]] = {}
    modifiers: dict[int, int] = {}
    star_ranges = []
    for start, end, pitch in selected[2]:
        if base <= pitch < base + 5:
            playable.setdefault(start, []).append((pitch - base, end))
        elif pitch == base + 5:
            modifiers[start] = modifiers.get(start, 0) | FLAGS["forced"]
        elif pitch == base + 8:
            modifiers[start] = modifiers.get(start, 0) | FLAGS["tap"]
        elif pitch == 116:
            star_ranges.append((start, end, len(star_ranges) + 1))
    if not playable:
        raise ValueError(f"MIDI track {track_name!r} has no {difficulty} notes")

    tick_ms = tempo_converter(all_tempos, division)
    if sections_out is not None:
        for parsed in tracks:
            if parsed[0].strip().upper() != "EVENTS":
                continue
            for tick, raw_text in parsed[3]:
                text = raw_text.strip().strip("[]")
                if text.lower().startswith("section "):
                    name = text[8:].replace("_", " ").strip()
                    if name:
                        sections_out.append(Section(tick_ms(tick), name[:47]))
    result = []
    previous_tick = None
    previous_mask = 0
    phrase_notes: dict[int, list[Note]] = {}
    for tick in sorted(playable):
        lane_mask = 0
        end_tick = tick
        for lane, note_end in playable[tick]:
            lane_mask |= 1 << lane
            end_tick = max(end_tick, note_end)
        flags = modifiers.get(tick, 0)
        natural_hopo = (
            previous_tick is not None
            and tick - previous_tick <= division // 3
            and lane_mask & (lane_mask - 1) == 0
            and previous_mask & (previous_mask - 1) == 0
            and lane_mask != previous_mask
        )
        if flags & FLAGS["forced"]:
            natural_hopo = not natural_hopo
        if natural_hopo:
            flags |= FLAGS["hopo"]
        phrase_id = next((number for start, end, number in star_ranges
                          if start <= tick < end), 0)
        if phrase_id:
            flags |= FLAGS["star"]
        note = Note(tick_ms(tick), tick_ms(end_tick) - tick_ms(tick),
                    lane_mask, flags, phrase_id)
        if phrase_id:
            phrase_notes.setdefault(phrase_id, []).append(note)
        result.append(note)
        previous_tick = tick
        previous_mask = lane_mask
    for members in phrase_notes.values():
        members[-1].flags |= FLAGS["phrase_end"]
    return result


def compile_chart(args: argparse.Namespace,
                  supplied_notes: list[Note] | None = None,
                  supplied_sections: list[Section] | None = None) -> None:
    sections = list(supplied_sections if supplied_sections is not None else
                    getattr(args, "sections", []))
    if supplied_notes is not None:
        notes = supplied_notes
    elif args.chart:
        notes = load_chart(args.chart, args.chart_track, sections)
    elif args.midi:
        notes = load_midi(args.midi, args.midi_track, args.difficulty,
                          sections)
    else:
        notes = load_notes(args.notes)
    if len(notes) > 20000 or notes[-1].time_ms > args.song_length_ms + 2000:
        raise ValueError("chart exceeds runtime limits")
    note_payload = b"".join(
        struct.pack("<IIBBH", note.time_ms, note.duration_ms, note.lanes, note.flags, note.phrase)
        for note in notes
    )
    strings = bytearray()
    section_payload = bytearray()
    sections.sort(key=lambda section: section.time_ms)
    if len(sections) > 512:
        raise ValueError("chart has more than 512 sections")
    for section in sections:
        encoded = section.name.encode("utf-8")
        if (section.time_ms < 0 or section.time_ms > args.song_length_ms or
                not encoded or len(encoded) > 95 or b"\0" in encoded):
            raise ValueError("invalid chart section")
        offset = len(strings)
        strings.extend(encoded + b"\0")
        section_payload.extend(struct.pack("<II", section.time_ms, offset))
    payload = note_payload + bytes(section_payload) + bytes(strings)
    if 40 + len(payload) > 256 * 1024:
        raise ValueError("compiled chart exceeds 256 KiB")
    header = struct.pack(
        "<4sHHIIIiIIBBBBI",
        b"IHC1", 40, 0, len(notes), len(sections), args.song_length_ms,
        args.audio_offset_ms, args.source_crc32, args.source_size & 0xFFFFFFFF,
        5, DIFFICULTIES[args.difficulty],
        ORIGINS[args.origin or
                ("imported" if args.chart or args.midi else "authored")], 0,
        rockbox_crc32(payload),
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary_name = tempfile.mkstemp(prefix=f".{args.output.name}.", dir=args.output.parent)
    try:
        with os.fdopen(fd, "wb") as handle:
            handle.write(header)
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary_name, args.output)
    except Exception:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise
    print(f"wrote {args.output}: {len(notes)} events, {len(sections)} sections, "
          f"crc={rockbox_crc32(payload):08x}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--notes", type=Path, help="authored note TSV")
    source.add_argument("--chart", type=Path, help="Clone Hero .chart file")
    source.add_argument("--midi", type=Path, help="rhythm-game Standard MIDI file")
    parser.add_argument("--chart-track", default="ExpertSingle")
    parser.add_argument("--midi-track", default="PART GUITAR")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--song-length-ms", required=True, type=int)
    parser.add_argument("--audio-offset-ms", type=int, default=0)
    parser.add_argument("--source-crc32", type=lambda value: int(value, 16), default=0)
    parser.add_argument("--source-size", type=int, default=0)
    parser.add_argument("--audio", type=Path,
                        help="exact audio master used to derive identity")
    parser.add_argument("--confirm-audio-identity", action="store_true",
                        help="confirm the chart was checked against --audio")
    parser.add_argument("--index-output", type=Path,
                        help="atomically add/update this index.tsv")
    parser.add_argument("--device-path",
                        help="exact Rockbox path for --index-output")
    parser.add_argument("--device-chart-path",
                        help="chart path stored in --index-output")
    parser.add_argument("--title", default="")
    parser.add_argument("--artist", default="")
    parser.add_argument("--skin-id", default="live-stage")
    parser.add_argument("--difficulty", choices=DIFFICULTIES, default="expert")
    parser.add_argument("--origin", choices=ORIGINS)
    args = parser.parse_args()
    if args.song_length_ms <= 0 or not -30000 <= args.audio_offset_ms <= 30000:
        parser.error("invalid song length or audio offset")
    if args.audio is not None:
        size, crc = audio_identity(args.audio)
        if args.source_size not in (0, size) or args.source_crc32 not in (0, crc):
            parser.error("declared source identity does not match --audio")
        args.source_size = size
        args.source_crc32 = crc
    if args.index_output is not None:
        if (args.audio is None or not args.confirm_audio_identity or
                not args.device_path or not args.device_chart_path or
                not args.title or not args.artist):
            parser.error(
                "--index-output requires --audio, --confirm-audio-identity, "
                "--device-path, --device-chart-path, --title, and --artist"
            )
    compile_chart(args)
    if args.index_output is not None:
        update_index(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
