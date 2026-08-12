#!/usr/bin/env python3
"""Build and run the pinned official Uxn opcode conformance ROM."""

from __future__ import annotations

import argparse
import base64
import pathlib
import subprocess
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "tests/uxn/opctest.rom.b64"


def run(command: list[str]) -> None:
    print("+", " ".join(command))
    subprocess.run(command, check=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cc", default="cc", help="host C compiler")
    parser.add_argument(
        "--sim-build",
        default="build-sim-ipod6g",
        help="simulator build directory",
    )
    parser.add_argument(
        "--skip-plugin-build",
        action="store_true",
        help="only run the host opcode checks",
    )
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="uxn-regression-") as tmp_name:
        tmp = pathlib.Path(tmp_name)
        rom = tmp / "opctest.rom"
        harness = tmp / "uxn_core_test"
        rom.write_bytes(base64.b64decode(FIXTURE.read_bytes()))
        run(
            [
                args.cc,
                "-std=gnu99",
                "-O2",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "apps/plugins/uxn"),
                str(ROOT / "tests/uxn/uxn_core_test.c"),
                str(ROOT / "apps/plugins/uxn/uxn.c"),
                "-o",
                str(harness),
            ]
        )
        run([str(harness), str(rom)])

    if not args.skip_plugin_build:
        build = (ROOT / args.sim_build).resolve()
        target = build / "apps/plugins/uxn/uxn.rock"
        run(["make", "-C", str(build), str(target), "-j4"])
    print("Uxn regression: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
