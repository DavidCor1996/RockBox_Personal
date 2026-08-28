#!/usr/bin/env python3
"""Prove the iPod overlay contains the complete native Zelda3 game engine."""

from __future__ import annotations

import argparse
import shutil
import subprocess
from pathlib import Path

EXCLUDED_PLATFORM_SOURCES = {
    "config.c",            # Rockbox supplies Config directly.
    "glsl_shader.c",       # Desktop-only OpenGL presentation.
    "main.c",              # Replaced by the Rockbox plugin entry point.
    "opengl.c",            # Desktop-only OpenGL presentation.
    "zelda_cpu_infra.c",   # Optional original-CPU comparison emulator.
}
REQUIRED_SYMBOLS = {
    "ZeldaInitialize",
    "ZeldaRunFrame",
    "ZeldaRenderAudio",
    "ZeldaReadSram",
    "ZeldaWriteSram",
    "Module_MainRouting",
    "FileSelect_Main",
    "Overworld_LoadAndBuildScreen",
    "Dungeon_LoadAndDrawRoom",
    "Link_Main",
    "Sprite_Main",
    "Credits_LoadNextScene_Dungeon",
}
FORBIDDEN_SYMBOLS = {
    "EmuInitialize",
    "EmuRunFrameWithCompare",
    "EmuSynchronizeWholeState",
    "MsuPlayer_Mix",
    "opus_decode",
    "snes_init",
}
FORBIDDEN_MEDIA_SUFFIXES = {
    ".sfc", ".smc", ".dat", ".png", ".bmp", ".jpg", ".jpeg",
    ".ico", ".wav", ".pcm", ".flac", ".mp3", ".opus",
}


def fail(message: str) -> None:
    raise SystemExit(f"FAIL: {message}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path("build-hw-ipod6g"))
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    repo = args.repo.resolve()
    build_dir = args.build_dir.resolve()
    port = repo / "apps" / "plugins" / "zelda3"
    source_dir = port / "upstream" / "src"
    elf = build_dir / "apps" / "plugins" / "zelda3" / "zelda3.elf"
    link_map = build_dir / "apps" / "plugins" / "zelda3" / "zelda3.map"
    overlay = build_dir / "apps" / "plugins" / "zelda3" / "zelda3.ovl"
    nm = shutil.which("arm-elf-eabi-nm")

    for artifact in (elf, link_map, overlay):
        if not artifact.is_file():
            fail(f"missing hardware build artifact: {artifact}")
    if nm is None:
        fail("arm-elf-eabi-nm is not on PATH")

    expected_sources = {
        path.name for path in source_dir.glob("*.c")
        if path.name not in EXCLUDED_PLATFORM_SOURCES
    }
    expected_sources.update({"ppu.c", "dma.c", "dsp.c"})
    map_text = link_map.read_text(encoding="utf-8", errors="replace")
    missing_objects = sorted(
        source for source in expected_sources
        if f"/{Path(source).stem}.o" not in map_text
    )
    if missing_objects:
        fail(f"native source objects absent from link map: {', '.join(missing_objects)}")

    nm_text = subprocess.run(
        [nm, "--defined-only", str(elf)],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    ).stdout
    defined = {line.split()[-1] for line in nm_text.splitlines() if line.split()}
    missing_symbols = sorted(REQUIRED_SYMBOLS - defined)
    if missing_symbols:
        fail(f"game-progression symbols absent: {', '.join(missing_symbols)}")
    forbidden = sorted(FORBIDDEN_SYMBOLS & defined)
    if forbidden:
        fail(f"emulator/optional codec symbols linked: {', '.join(forbidden)}")

    undefined = subprocess.run(
        [nm, "-u", str(elf)],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    ).stdout.strip()
    if undefined:
        fail(f"unresolved overlay symbols:\n{undefined}")

    bundled_media = sorted(
        path.relative_to(port).as_posix()
        for path in port.rglob("*")
        if path.is_file() and path.suffix.lower() in FORBIDDEN_MEDIA_SUFFIXES
    )
    if bundled_media:
        fail(f"copyrighted/binary media unexpectedly bundled: {', '.join(bundled_media)}")

    print(
        "PASS: native Zelda3 overlay contains "
        f"{len(expected_sources)} upstream engine/PPU/DSP source objects"
    )
    print(f"PASS: {len(REQUIRED_SYMBOLS)} start-to-ending subsystem symbols present")
    print("PASS: optional SNES CPU comparison emulator and Opus/MSU paths absent")
    print("PASS: all overlay symbols resolve and no ROM-derived media is bundled")
    print(f"overlay_bytes={overlay.stat().st_size}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
