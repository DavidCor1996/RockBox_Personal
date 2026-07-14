#!/usr/bin/env python3
"""Generate little-endian, 32-bit SM64 audio data for native iPod builds."""

from __future__ import annotations

import subprocess
from pathlib import Path


def write_inc(source: Path, destination: Path) -> None:
    data = source.read_bytes()
    with destination.open("w", encoding="ascii") as stream:
        for offset in range(0, len(data), 32):
            block = data[offset : offset + 32]
            stream.write(",".join(f"0x{byte:X}" for byte in block))
            stream.write(",\n")


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    upstream = root / "apps/plugins/sm64/upstream"
    pc_sound = upstream / "build/us_pc/sound"
    output = upstream / "build/us_rockbox32/sound32"
    assembler = upstream / "tools/assemble_sound.py"
    banks = upstream / "sound/sound_banks"
    sequences_json = upstream / "sound/sequences.json"
    sequence_inputs = [pc_sound / "sequences/00_sound_player.m64"]
    sequence_inputs.extend(sorted((upstream / "sound/sequences/us").glob("*.m64")))

    required = [pc_sound / "samples", *sequence_inputs]
    missing = [str(path) for path in required if not path.exists()]
    if missing:
        raise SystemExit(
            "prepare the verified US PC assets first; missing: " + ", ".join(missing)
        )

    output.mkdir(parents=True, exist_ok=True)
    ctl = output / "sound_data.ctl"
    tbl = output / "sound_data.tbl"
    sequence_bin = output / "sequences.bin"
    bank_sets = output / "bank_sets"
    common = ["-DVERSION_US", "--endian", "little", "--bitwidth", "32"]

    subprocess.run(
        [
            "python3",
            str(assembler),
            str(pc_sound / "samples"),
            str(banks),
            str(ctl),
            str(tbl),
            *common,
        ],
        check=True,
    )
    subprocess.run(
        [
            "python3",
            str(assembler),
            "--sequences",
            str(sequence_bin),
            str(bank_sets),
            str(banks),
            str(sequences_json),
            *(str(path) for path in sequence_inputs),
            *common,
        ],
        check=True,
    )

    for binary in (ctl, tbl, sequence_bin, bank_sets):
        write_inc(binary, output / f"{binary.name}.inc.c")

    print(f"PASS: generated 32-bit SM64 audio assets in {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
