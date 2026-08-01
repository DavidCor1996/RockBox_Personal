#!/usr/bin/env python3
"""Build and run the bounded microUI core tests on the host."""

from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import tempfile


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--sanitizers", action="store_true")
    args = parser.parse_args()

    root = pathlib.Path(__file__).resolve().parents[1]
    source_dir = root / "apps/plugins/lib/microui"
    flags = [
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-fno-common",
        f"-I{source_dir}",
    ]
    if args.sanitizers:
        flags.extend(["-fsanitize=address,undefined", "-fno-omit-frame-pointer"])

    with tempfile.TemporaryDirectory(prefix="microui-core-gate-") as temp:
        executable = pathlib.Path(temp) / "microui-core-gate"
        command = [
            args.cc,
            *flags,
            str(source_dir / "microui.c"),
            str(source_dir / "test_microui.c"),
            "-o",
            str(executable),
        ]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        if args.sanitizers:
            environment.setdefault("ASAN_OPTIONS", "detect_leaks=0")
        subprocess.run([str(executable)], check=True, env=environment)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
