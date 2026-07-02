#!/usr/bin/env python3
"""Run the MuJS theme-helper script in a Rockbox simulator."""

from __future__ import annotations

import argparse
import re
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

OPEN_PLUGIN_ENTRY_SIZE = 568
OPEN_PLUGIN_NAME_OFFSET = 12
OPEN_PLUGIN_NAME_SIZE = 33
OPEN_PLUGIN_PATH_OFFSET = 45
OPEN_PLUGIN_PATH_SIZE = 261
OPEN_PLUGIN_PARAM_OFFSET = 306
OPEN_PLUGIN_PARAM_SIZE = 261
START_SCREEN_HASH = 0x8E2A0CC9
OPEN_PLUGIN_CHECKSUM = (
    (OPEN_PLUGIN_ENTRY_SIZE << 16)
    + 0
    + 4
    + 8
    + OPEN_PLUGIN_NAME_OFFSET
    + OPEN_PLUGIN_PATH_OFFSET
    + OPEN_PLUGIN_PARAM_OFFSET
)

REPO_ROOT = Path(__file__).resolve().parents[1]
PLUGIN_PATH = "/.rockbox/rocks/apps/mujs.rock"
SCRIPT_PATH = "/.rockbox/scripts/theme_helper.js"
REPORT_PATH = ".rockbox/scripts/data/theme_helper_report.txt"


def lang_id(build_dir: Path, name: str) -> int:
    for line in (build_dir / "lang_enum.h").read_text(encoding="utf-8", errors="replace").splitlines():
        if name in line:
            match = re.search(r"/\*\s*(\d+)\s*\*/", line)
            if match:
                return int(match.group(1))
    raise SystemExit(f"missing {name}")


def lang_last_index(build_dir: Path) -> int:
    last = None
    for line in (build_dir / "lang_enum.h").read_text(encoding="utf-8", errors="replace").splitlines():
        match = re.search(r"/\*\s*(\d+)\s*\*/", line)
        if match:
            last = int(match.group(1))
        if "LANG_LAST_INDEX_IN_ARRAY" in line:
            if last is None:
                raise SystemExit("could not derive LANG_LAST_INDEX_IN_ARRAY")
            return last + 1
    raise SystemExit("missing LANG_LAST_INDEX_IN_ARRAY")


def write_cstring(entry: bytearray, offset: int, size: int, value: str) -> None:
    encoded = value.encode("utf-8")
    if len(encoded) >= size:
        raise ValueError(f"open plugin field is too long: {value}")
    entry[offset : offset + size] = b"\0" * size
    entry[offset : offset + len(encoded)] = encoded


def write_open_plugin(build_dir: Path, simdisk: Path) -> None:
    entry = bytearray(OPEN_PLUGIN_ENTRY_SIZE)
    start_screen = lang_id(build_dir, "LANG_START_SCREEN")
    checksum = OPEN_PLUGIN_CHECKSUM + lang_last_index(build_dir)
    struct.pack_into("<IiI", entry, 0, START_SCREEN_HASH, start_screen, checksum)
    write_cstring(entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE, "mujs.rock")
    write_cstring(entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE, PLUGIN_PATH)
    write_cstring(entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, SCRIPT_PATH)

    plugin_dat = simdisk / ".rockbox/rocks/plugin.dat"
    plugin_dat.parent.mkdir(parents=True, exist_ok=True)
    plugin_dat.write_bytes(entry)

    config = simdisk / ".rockbox/config.cfg"
    lines = []
    if config.exists():
        lines = config.read_text(encoding="utf-8", errors="replace").splitlines()
    lines = [line for line in lines if not line.startswith("start in screen:")]
    lines = [line for line in lines if not line.startswith("openplugin:")]
    lines.append("start in screen: plugin")
    config.parent.mkdir(parents=True, exist_ok=True)
    config.write_text("\n".join(lines) + "\n", encoding="utf-8")


def stage_files(build_dir: Path, simdisk: Path) -> Path:
    built_plugin = build_dir / "apps/plugins/mujs/mujs.rock"
    if not built_plugin.is_file():
        raise SystemExit(f"missing built plugin: {built_plugin}")

    target_plugin = simdisk / PLUGIN_PATH.lstrip("/")
    target_script = simdisk / SCRIPT_PATH.lstrip("/")
    target_data = simdisk / ".rockbox/scripts/data/theme_helper_sample.wps"
    report = simdisk / REPORT_PATH

    target_plugin.parent.mkdir(parents=True, exist_ok=True)
    target_script.parent.mkdir(parents=True, exist_ok=True)
    target_data.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(built_plugin, target_plugin)
    shutil.copy2(REPO_ROOT / "apps/plugins/mujs/scripts/theme_helper.js", target_script)
    shutil.copy2(REPO_ROOT / "apps/plugins/mujs/scripts/data/theme_helper_sample.wps", target_data)
    if report.exists():
        report.unlink()
    return report


def validate_report(report: Path) -> None:
    text = report.read_text(encoding="utf-8", errors="replace")
    required = [
        "theme-helper-report",
        "mode=ipone-designer-dependency-check",
        "theme=",
        "wps=ok ",
        "sbs=ok ",
        "fms=ok ",
        "font=ok ",
        "iconset=ok ",
        "backdrop=",
        "missing-total=0",
    ]
    missing = [item for item in required if item not in text]
    if missing:
        raise SystemExit(f"theme helper report missing fields: {', '.join(missing)}")
    print(text.rstrip())


def run_gate(build_dir: Path, timeout: int) -> None:
    binary = build_dir / "rockboxui"
    simdisk = build_dir / "simdisk"
    if not binary.is_file():
        raise SystemExit(f"missing simulator binary: {binary}")
    if not simdisk.is_dir():
        raise SystemExit(f"missing simulator disk: {simdisk}")

    report = stage_files(build_dir, simdisk)
    write_open_plugin(build_dir, simdisk)

    proc = subprocess.Popen(
        [str(binary), "--root", str(simdisk)],
        cwd=build_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    deadline = time.monotonic() + timeout
    try:
        while time.monotonic() < deadline:
            if report.exists():
                validate_report(report)
                return
            if proc.poll() is not None:
                break
            time.sleep(0.2)
        output = proc.stdout.read() if proc.stdout else ""
        raise SystemExit("MuJS theme helper did not complete\n" + output[-2000:])
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=3)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", default="build-sim-ipod6g")
    parser.add_argument("--timeout", type=int, default=15)
    args = parser.parse_args()
    build_dir = Path(args.build_dir)
    if not build_dir.is_absolute():
        build_dir = REPO_ROOT / build_dir
    run_gate(build_dir, args.timeout)
    return 0


if __name__ == "__main__":
    sys.exit(main())
