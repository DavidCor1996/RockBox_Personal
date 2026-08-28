#!/usr/bin/env python3
"""Package user-licensed Pokédex effects for pokedex.rock.

This tool deliberately downloads nothing. Supply two PCM WAV files you have
the right to use: an entry scan and a short navigation effect. It produces the
bounded PCM16LE stereo ``pokedex.uib`` file consumed by the plugin.
"""

from __future__ import annotations

import argparse
import audioop
import struct
import wave
from pathlib import Path


MAGIC = b"PDX1"
EFFECTS = 2
MAX_BYTES = 96 * 1024


def load_wav(path: Path, rate: int) -> bytes:
    with wave.open(str(path), "rb") as source:
        if source.getcomptype() != "NONE" or source.getsampwidth() != 2:
            raise ValueError(f"{path}: require uncompressed 16-bit PCM WAV")
        channels = source.getnchannels()
        if channels not in (1, 2):
            raise ValueError(f"{path}: require mono or stereo WAV")
        data = source.readframes(source.getnframes())
        source_rate = source.getframerate()
    if channels == 1:
        data = audioop.tostereo(data, 2, 1, 1)
    if source_rate != rate:
        data, _ = audioop.ratecv(data, 2, 2, source_rate, rate, None)
    return data


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scan", type=Path, required=True,
                        help="user-licensed entry/scan PCM WAV")
    parser.add_argument("--nav", type=Path, required=True,
                        help="user-licensed navigation PCM WAV")
    parser.add_argument("--rate", type=int, default=44100,
                        choices=(44100, 48000))
    parser.add_argument("--output", type=Path, required=True,
                        help="normally <ipod>/.rockbox/pokedex/sounds/pokedex.uib")
    args = parser.parse_args()

    effects = [load_wav(args.scan, args.rate), load_wav(args.nav, args.rate)]
    header_size = 12 + EFFECTS * 12
    cursor = header_size
    entries = []
    for effect in effects:
        entries.append((cursor, len(effect)))
        cursor += len(effect)
    if cursor > MAX_BYTES:
        raise SystemExit(
            f"effects total {cursor} bytes; Pokedex limit is {MAX_BYTES}. "
            "Trim the source WAV files first."
        )

    output = bytearray(MAGIC + struct.pack("<IHH", args.rate, EFFECTS, 0))
    for offset, length in entries:
        output.extend(struct.pack("<III", 0, offset, length))
    output.extend(effects[0])
    output.extend(effects[1])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(output)
    print(f"Wrote {args.output} ({len(output)} bytes, {args.rate} Hz)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
