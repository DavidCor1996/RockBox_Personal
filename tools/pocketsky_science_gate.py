#!/usr/bin/env python3
"""Compare Pocket Sky's vendored math path with a host-libm reference."""

from __future__ import annotations

import argparse
import math
import pathlib
import subprocess
import sys
import tempfile


def run(command: list[str]) -> None:
    subprocess.run(command, check=True)


def build(root: pathlib.Path, output: pathlib.Path, port_math: bool) -> None:
    plugin = root / "apps/plugins/pocketsky"
    astroterm = plugin / "upstream/astroterm"
    astronomy = plugin / "upstream/astronomy_engine"
    sources = [
        root / "tools/pocketsky_science_gate.c",
        astroterm / "astro.c",
        astroterm / "coord.c",
        astronomy / "astronomy.c",
    ]
    command = [
        "cc", "-std=c99", "-O2", "-fno-builtin",
        "-DASTRONOMY_ENGINE_NO_CURRENT_TIME",
    ]
    if port_math:
        command.extend([f"-I{plugin}", f"-I{plugin / 'upstream/musl_math'}"])
        for line in (plugin / "SOURCES").read_text(encoding="utf-8").splitlines():
            if line.startswith("upstream/musl_math/") and line.endswith(".c"):
                sources.append(plugin / line)
    command.extend([f"-I{astroterm}", f"-I{astronomy}"])
    command.extend(str(source) for source in sources)
    if not port_math:
        command.append("-lm")
    command.extend(["-o", str(output)])
    run(command)


def compare(reference: str, port: str) -> tuple[int, float]:
    reference_lines = reference.splitlines()
    port_lines = port.splitlines()
    if len(reference_lines) != len(port_lines):
        raise ValueError(
            f"line count mismatch: {len(reference_lines)} != {len(port_lines)}"
        )
    maximum = 0.0
    values = 0
    for line_number, (expected, actual) in enumerate(
        zip(reference_lines, port_lines), 1
    ):
        expected_fields = expected.split()
        actual_fields = actual.split()
        if expected_fields[:2] != actual_fields[:2] or len(expected_fields) != len(actual_fields):
            raise ValueError(f"label/field mismatch on line {line_number}")
        for expected_text, actual_text in zip(expected_fields[2:], actual_fields[2:]):
            expected_value = float(expected_text)
            actual_value = float(actual_text)
            if math.isnan(expected_value) and math.isnan(actual_value):
                continue
            difference = abs(expected_value - actual_value)
            tolerance = 2.0e-8 + 2.0e-10 * abs(expected_value)
            if not math.isfinite(actual_value) or difference > tolerance:
                raise ValueError(
                    f"numerical mismatch on line {line_number}: "
                    f"{expected_value} != {actual_value} (diff {difference})"
                )
            maximum = max(maximum, difference)
            values += 1
    return values, maximum


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("resource_dir", type=pathlib.Path)
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[1]

    run([sys.executable, str(root / "tools/pocketsky_reference_gate.py"),
         str(args.resource_dir)])
    with tempfile.TemporaryDirectory(prefix="pocketsky-science-") as temp:
        temp_root = pathlib.Path(temp)
        reference_binary = temp_root / "reference"
        port_binary = temp_root / "port"
        build(root, reference_binary, False)
        build(root, port_binary, True)
        reference = subprocess.check_output(
            [str(reference_binary), str(args.resource_dir)], text=True
        )
        port = subprocess.check_output(
            [str(port_binary), str(args.resource_dir)], text=True
        )
    values, maximum = compare(reference, port)
    print(
        f"Pocket Sky science OK: 24 cases, {values} values, "
        f"maximum host/port delta {maximum:.3g}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.CalledProcessError, ValueError) as error:
        print(f"pocketsky_science_gate: {error}", file=sys.stderr)
        raise SystemExit(1)
