#!/usr/bin/env python3
"""Package preserved Club Penguin assets for the Rockbox iPod plugin.

The runtime intentionally loads prepared BMP and TSV files. This host-side
tool converts/copies real source art into that package; it does not generate
replacement room art.
"""

from __future__ import annotations

import argparse
import hashlib
import shutil
import subprocess
from pathlib import Path


WORLD_ROWS = [
    ("My Place", "Source-scale preserved area. Offline membership is free.",
     427, 240, 45, "", 427, 240),
    ("Town", "Town marker in the preserved offline area.", 585, 382, 48, "",
     427, 240),
    ("Plaza", "Plaza marker in the preserved offline area.", 638, 292, 50,
     "", 427, 240),
    ("Dock", "Dock marker in the preserved offline area.", 112, 362, 50, "",
     427, 240),
    ("Ski Village", "Ski Village marker in the preserved offline area.",
     320, 257, 44, "", 427, 240),
    ("Dojo", "Dojo marker in the preserved offline area.", 494, 52, 44, "",
     427, 240),
    ("Cove", "Cove marker in the preserved offline area.", 723, 161, 50, "",
     427, 240),
    ("Beach", "Beach marker in the preserved offline area.", 126, 112, 42,
     "", 427, 240),
    ("Snow Forts", "Snow Forts marker in the preserved offline area.", 456,
     352, 45, "", 427, 240),
    ("Forest", "Forest marker in the preserved offline area.", 694, 410, 44,
     "", 427, 240),
    ("Mine", "Mine marker in the preserved offline area.", 756, 332, 42, "",
     427, 240),
    ("Iceberg", "Iceberg marker in the preserved offline area.", 760, 67, 42,
     "", 427, 240),
]


ROOM_ROWS = [
    ("player_home", "My Place", "rooms/player_home.bmp", 160, 150),
    ("town", "Town", "rooms/town.bmp", 160, 150),
    ("plaza", "Plaza", "rooms/plaza.bmp", 160, 150),
    ("dock", "Dock", "rooms/dock.bmp", 160, 150),
    ("ski_village", "Ski Village", "rooms/ski_village.bmp", 160, 150),
    ("dojo", "Dojo", "rooms/dojo.bmp", 160, 150),
    ("cove", "Cove", "rooms/cove.bmp", 160, 150),
]


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def run_ffmpeg(args: list[str]) -> None:
    subprocess.run(["ffmpeg", "-y", "-hide_banner", "-loglevel", "error",
                    *args], check=True)


def make_player_frame(source: Path, frame: tuple[str, int, int, int, int],
                      out: Path) -> None:
    sheet, x, y, w, h = frame
    run_ffmpeg([
        "-i", str(source / "images" / sheet),
        "-f", "lavfi",
        "-i", "color=c=magenta:s=40x42",
        "-filter_complex",
        f"[0:v]crop={w}:{h}:{x}:{y}[fg];"
        "[1:v][fg]overlay=(main_w-overlay_w)/2:"
        "(main_h-overlay_h)/2:format=auto,format=bgr24",
        "-frames:v", "1",
        str(out),
    ])


def git_commit(path: Path) -> str:
    if not (path / ".git").exists():
        return "unknown"
    try:
        return subprocess.check_output(
            ["git", "-C", str(path), "rev-parse", "HEAD"],
            text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
    except subprocess.SubprocessError:
        return "unknown"


def write_tsvs(out: Path) -> None:
    data = out / "data"
    data.mkdir(parents=True, exist_ok=True)
    with (data / "world.tsv").open("w", encoding="utf-8") as f:
        f.write("# name\tdetail\tx\ty\tradius\ttarget\tto_x\tto_y\n")
        for row in WORLD_ROWS:
            f.write("\t".join(str(value) for value in row) + "\n")

    with (data / "rooms.tsv").open("w", encoding="utf-8") as f:
        f.write("# id\ttitle\tbmp\tstart_x\tstart_y\n")
        for row in ROOM_ROWS:
            f.write("\t".join(str(value) for value in row) + "\n")


def write_manifest(out: Path, source: Path, generated: list[Path]) -> None:
    commit = git_commit(source)
    with (out / "source.manifest").open("w", encoding="utf-8") as f:
        f.write("CLUBPENGUIN_ASSET_MANIFEST_V1\n")
        f.write(f"source_repo={source}\n")
        f.write(f"source_commit={commit}\n")
        for path in generated:
            f.write(f"generated={path.relative_to(out)}\n")
            f.write(f"sha256={sha256(path)}\n")
        f.write("world_scale=854x480\n")
        f.write("player_strip=16x40x42\n")
        f.write("data=data/world.tsv\n")
        f.write("data=data/rooms.tsv\n")


def package_freeroam(source: Path, out: Path) -> None:
    world_src = source / "images" / "sprite-sheet0.png"
    player_src = source / "images" / "sprite2-sheet0.png"
    player_src2 = source / "images" / "sprite2-sheet1.png"
    cover_dir = out / "covers"
    generated: list[Path] = []

    if not world_src.exists():
        raise SystemExit(f"missing real source asset: {world_src}")
    if not player_src.exists():
        raise SystemExit(f"missing real source asset: {player_src}")
    if not player_src2.exists():
        raise SystemExit(f"missing real source asset: {player_src2}")
    if shutil.which("ffmpeg") is None:
        raise SystemExit("ffmpeg is required to convert the preserved PNGs")

    out.mkdir(parents=True, exist_ok=True)
    cover_dir.mkdir(parents=True, exist_ok=True)

    world_out = out / "world.bmp"
    player_out = out / "player.bmp"
    cover_out = cover_dir / "ClubPenguin.bmp"
    cover_space_out = cover_dir / "Club Penguin.bmp"
    cover_lower_out = cover_dir / "clubpenguin.bmp"
    cover_pane_out = cover_dir / "Club Penguin.pane.bmp"
    cover_lower_pane_out = cover_dir / "clubpenguin.pane.bmp"

    run_ffmpeg([
        "-i", str(world_src),
        "-vf", "scale=854:480",
        "-pix_fmt", "bgr24",
        str(world_out),
    ])
    generated.append(world_out)

    frames = [
        ("sprite2-sheet0.png", 1, 1, 36, 35),
        ("sprite2-sheet0.png", 39, 1, 35, 35),
        ("sprite2-sheet0.png", 1, 1, 36, 35),
        ("sprite2-sheet0.png", 76, 36, 34, 35),
        ("sprite2-sheet0.png", 1, 77, 22, 41),
        ("sprite2-sheet0.png", 88, 73, 24, 41),
        ("sprite2-sheet0.png", 1, 77, 22, 41),
        ("sprite2-sheet1.png", 1, 1, 22, 41),
        ("sprite2-sheet0.png", 76, 1, 37, 33),
        ("sprite2-sheet0.png", 31, 38, 28, 36),
        ("sprite2-sheet0.png", 1, 38, 28, 37),
        ("sprite2-sheet0.png", 31, 38, 28, 36),
        ("sprite2-sheet0.png", 31, 76, 23, 42),
        ("sprite2-sheet0.png", 61, 73, 25, 40),
        ("sprite2-sheet1.png", 25, 1, 22, 41),
        ("sprite2-sheet0.png", 61, 73, 25, 40),
    ]
    frame_paths = []
    for i, frame in enumerate(frames):
        frame_path = out / f".player_frame_{i}.bmp"
        make_player_frame(source, frame, frame_path)
        frame_paths.append(frame_path)

    hstack_args = []
    for frame_path in frame_paths:
        hstack_args.extend(["-i", str(frame_path)])
    run_ffmpeg([
        *hstack_args,
        "-filter_complex", f"hstack=inputs={len(frame_paths)},format=bgr24",
        str(player_out),
    ])
    for frame_path in frame_paths:
        frame_path.unlink()
    generated.append(player_out)

    run_ffmpeg([
        "-i", str(world_src),
        "-vf", "scale=174:174:force_original_aspect_ratio=increase,"
               "crop=174:174",
        "-pix_fmt", "bgr24",
        str(cover_out),
    ])
    generated.append(cover_out)
    shutil.copyfile(cover_out, cover_space_out)
    shutil.copyfile(cover_out, cover_lower_out)
    shutil.copyfile(cover_out, cover_pane_out)
    shutil.copyfile(cover_out, cover_lower_pane_out)
    generated.extend([
        cover_space_out, cover_lower_out, cover_pane_out, cover_lower_pane_out
    ])

    write_tsvs(out)
    write_manifest(out, source, generated)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path,
                        help="path to a checked-out preserved asset repo")
    parser.add_argument("--out", type=Path,
                        default=Path("assets/ipodjs/rockbox/clubpenguin"))
    args = parser.parse_args()

    package_freeroam(args.source.resolve(), args.out)


if __name__ == "__main__":
    main()
