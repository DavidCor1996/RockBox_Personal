#!/usr/bin/env python3
"""Compile the shared C core and run all committed Maker Lite test levels."""

from __future__ import annotations

import argparse
import os
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "rockpod"))

from services.maker_lite_pack import (  # noqa: E402
    PACK_ENTITY_SIZE,
    compile_project_file,
)


def run(command, **kwargs):
    return subprocess.run(command, check=True, text=True, **kwargs)


def _write_crc(pack: bytearray) -> None:
    struct.pack_into("<I", pack, 40, zlib.crc32(pack[128:]) & 0xFFFFFFFF)


def _assert_bad_content(executable: Path, path: Path, pack: bytearray) -> None:
    _write_crc(pack)
    path.write_bytes(pack)
    result = subprocess.run(
        [str(executable), str(path), "1"],
        check=False,
        text=True,
        capture_output=True,
    )
    if result.returncode == 0 or "invalid pack content" not in result.stderr:
        raise SystemExit(
            f"malformed pack was not rejected as content: "
            f"code={result.returncode} stderr={result.stderr.strip()}"
        )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
    parser.add_argument("--frames", type=int, default=600)
    parser.add_argument(
        "--sanitize",
        action="store_true",
        help="run the host core with address and undefined-behavior sanitizers",
    )
    args = parser.parse_args()

    fixtures = ROOT / "testdata" / "maker_lite"
    with tempfile.TemporaryDirectory(prefix="maker-lite-host-") as temp_name:
        temp = Path(temp_name)
        executable = temp / "maker_lite_host_gate"
        optimization = ["-O2"]
        if args.sanitize:
            optimization = [
                "-O1",
                "-g",
                "-fno-omit-frame-pointer",
                "-fsanitize=address,undefined",
            ]
        compile_command = [
                args.cc,
                "-std=c99",
                *optimization,
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "lib" / "maker_lite"),
                str(ROOT / "lib" / "maker_lite" / "maker_lite.c"),
                str(ROOT / "tools" / "maker_lite_host_gate.c"),
                "-o",
                str(executable),
            ]
        run(compile_command)
        for source in sorted(fixtures.glob("*-test.json")):
            pack = temp / f"{source.stem}.mlp"
            metadata = compile_project_file(source, pack)
            result = run(
                [str(executable), str(pack), str(args.frames)],
                capture_output=True,
            )
            output = result.stdout.strip()
            repeated = run(
                [str(executable), str(pack), str(args.frames)],
                capture_output=True,
            ).stdout.strip()
            if output != repeated:
                raise SystemExit(
                    f"nondeterministic shared-core trace:\n{output}\n{repeated}"
                )
            if f"ruleset={metadata.ruleset_id}" not in output:
                raise SystemExit(f"wrong runtime ruleset: {output}")
            if "complete=1" not in output:
                raise SystemExit(f"fixture did not reach its goal: {output}")
            expected_features = {
                "mario": 0x5C3,
                "zelda": 0xDCC,
                "sonic": 0x7F3,
            }[metadata.ruleset]
            marker = f"features={expected_features:03x}"
            if marker not in output:
                raise SystemExit(
                    f"{metadata.ruleset} did not exercise its action graph: {output}"
                )
            print(f"PASS {metadata.ruleset}: {output}")

            if metadata.ruleset == "mario":
                original = bytearray(pack.read_bytes())
                event_offset = struct.unpack_from("<I", original, 108)[0]
                bad_event = bytearray(original)
                struct.pack_into("<h", bad_event, event_offset + 12, 0)
                _assert_bad_content(
                    executable, temp / "bad-event.mlp", bad_event
                )

                path_offset = struct.unpack_from("<I", original, 116)[0]
                bad_path = bytearray(original)
                struct.pack_into("<h", bad_path, path_offset + 8, 0)
                _assert_bad_content(
                    executable, temp / "bad-path.mlp", bad_path
                )

                entity_offset = struct.unpack_from("<I", original, 28)[0]
                entity_count = struct.unpack_from("<H", original, 18)[0]
                bad_reference = bytearray(original)
                found_block = False
                for index in range(entity_count):
                    record = entity_offset + index * PACK_ENTITY_SIZE
                    if struct.unpack_from("<H", original, record)[0] == 9:
                        struct.pack_into("<h", bad_reference, record + 14, 999)
                        found_block = True
                        break
                if not found_block:
                    raise SystemExit("Mario fixture has no moving block")
                _assert_bad_content(
                    executable, temp / "bad-path-reference.mlp", bad_reference
                )
                print("PASS malformed pack content rejection")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
