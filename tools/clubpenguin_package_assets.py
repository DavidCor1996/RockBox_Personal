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
    ("My Place", "Enter your offline igloo.", 1763, 1309, 110, "player_home",
     160, 170),
    ("Town", "Enter Town.", 1034, 1102, 135, "town", 160, 170),
    ("Plaza", "Enter the Plaza.", 1942, 1044, 150, "plaza", 160, 170),
    ("Dock", "Enter the Dock.", 322, 1276, 140, "dock", 160, 170),
    ("Ski Village", "Enter Ski Village.", 848, 505, 120, "ski_village",
     160, 170),
    ("Dojo", "Enter the Dojo.", 1492, 133, 110, "dojo", 160, 170),
    ("Cove", "Enter the Cove.", 2170, 472, 125, "cove", 160, 170),
    ("Beach", "Enter the Beach.", 271, 679, 130, "beach", 160, 170),
    ("Snow Forts", "Enter the Snow Forts.", 1475, 1061, 120, "snow_forts",
     160, 170),
    ("Forest", "Enter the Forest.", 1950, 688, 120, "forest", 160, 170),
    ("Mine", "Enter the Mine Shack.", 1755, 389, 110, "mine", 160, 170),
    ("Iceberg", "Enter the Iceberg.", 2416, 290, 110, "iceberg", 160, 170),
]

WORLD_SOURCE_WIDTH = 2713
WORLD_SOURCE_HEIGHT = 1823
WORLD_OUTPUT_WIDTH = 320
WORLD_OUTPUT_HEIGHT = 220


ROOM_ROWS = [
    ("player_home", "My Place", "rooms/player_home.bmp",
     160, 170, 45, 120, 275, 198),
    ("town", "Town", "rooms/town.bmp", 160, 170, 25, 120, 295, 198),
    ("plaza", "Plaza", "rooms/plaza.bmp", 160, 170, 20, 120, 300, 198),
    ("dock", "Dock", "rooms/dock.bmp", 160, 170, 25, 100, 295, 198),
    ("ski_village", "Ski Village", "rooms/ski_village.bmp",
     160, 170, 25, 115, 295, 198),
    ("dojo", "Dojo", "rooms/dojo.bmp", 160, 170, 35, 135, 285, 198),
    ("cove", "Cove", "rooms/cove.bmp", 160, 170, 25, 115, 295, 198),
    ("beach", "Beach", "rooms/beach.bmp", 160, 170, 40, 110, 280, 198),
    ("snow_forts", "Snow Forts", "rooms/snow_forts.bmp",
     160, 170, 20, 105, 300, 198),
    ("forest", "Forest", "rooms/forest.bmp", 160, 170, 35, 100, 285, 198),
    ("mine", "Mine Shack", "rooms/mine.bmp", 160, 170, 30, 120, 290, 198),
    ("iceberg", "Iceberg", "rooms/iceberg.bmp",
     160, 170, 35, 105, 285, 198),
]


INTERACTION_ROWS = [
    ("mine", "cart_surfer", 246, 127, 28, "minigame", "cart_surfer",
     "Play Cart Surfer", 0, 0),
    ("town", "to_dock", 8, 170, 24, "room", "dock",
     "Go to the Dock", 272, 170),
    ("town", "to_snow_forts", 295, 170, 24, "room", "snow_forts",
     "Go to the Snow Forts", 45, 170),
    ("dock", "to_ski_village", 72, 120, 25, "room", "ski_village",
     "Go to Ski Village", 275, 170),
    ("dock", "to_town", 272, 120, 25, "room", "town",
     "Go to Town", 45, 170),
    ("ski_village", "to_beach", 25, 155, 25, "room", "beach",
     "Go to the Beach", 255, 165),
    ("ski_village", "to_dock", 295, 155, 25, "room", "dock",
     "Go to the Dock", 85, 155),
    ("beach", "to_ski_village", 258, 120, 26, "room", "ski_village",
     "Go to Ski Village", 45, 160),
    ("snow_forts", "to_town", 52, 125, 25, "room", "town",
     "Go to Town", 275, 170),
    ("snow_forts", "to_plaza", 266, 125, 25, "room", "plaza",
     "Go to the Plaza", 45, 170),
    ("plaza", "to_snow_forts", 20, 165, 25, "room", "snow_forts",
     "Go to the Snow Forts", 275, 165),
    ("plaza", "to_forest", 300, 165, 25, "room", "forest",
     "Go to the Forest", 55, 150),
    ("forest", "to_plaza", 35, 120, 26, "room", "plaza",
     "Go to the Plaza", 275, 165),
    ("forest", "to_cove", 285, 115, 26, "room", "cove",
     "Go to the Cove", 55, 135),
    ("cove", "to_forest", 35, 120, 26, "room", "forest",
     "Go to the Forest", 265, 135),
]


VANILLA_ROOMS = "media/default/svanilla/media/play/v2/content/global/rooms"
LEGACY_ROOMS = "media/default/slegacy/media/play/v2/content/global/rooms"

ROOM_SWFS = {
    "player_home": "media/default/fix/Igloo1.swf",
    "town": f"{VANILLA_ROOMS}/town.swf",
    "plaza": f"{VANILLA_ROOMS}/plaza.swf",
    "dock": f"{VANILLA_ROOMS}/dock.swf",
    "ski_village": f"{VANILLA_ROOMS}/village.swf",
    "dojo": f"{VANILLA_ROOMS}/dojo.swf",
    "cove": f"{VANILLA_ROOMS}/cove.swf",
    "beach": f"{VANILLA_ROOMS}/beach.swf",
    "snow_forts": f"{VANILLA_ROOMS}/forts.swf",
    "forest": f"{VANILLA_ROOMS}/forest.swf",
    "mine": f"{VANILLA_ROOMS}/mine.swf",
    "iceberg": f"{LEGACY_ROOMS}/berg.swf",
}


def validate_interactions() -> None:
    rooms = {row[0]: row for row in ROOM_ROWS}
    seen: set[tuple[str, str]] = set()
    room_links: set[tuple[str, str]] = set()

    world_targets = {row[5] for row in WORLD_ROWS}
    missing_map_entries = set(rooms) - world_targets
    if missing_map_entries:
        raise SystemExit(
            "rooms missing map entries: " + ", ".join(sorted(missing_map_entries))
        )

    for row in INTERACTION_ROWS:
        room, interaction_id, x, y, radius, action, target, _, to_x, to_y = row
        key = (room, interaction_id)

        if room not in rooms:
            raise SystemExit(f"interaction has unknown room: {room}")
        if key in seen:
            raise SystemExit(
                f"duplicate interaction id in room: {room}/{interaction_id}"
            )
        seen.add(key)
        if not (0 <= x < WORLD_OUTPUT_WIDTH and
                0 <= y < WORLD_OUTPUT_HEIGHT and radius > 0):
            raise SystemExit(
                f"interaction is outside room bounds: {room}/{interaction_id}"
            )
        if action == "room":
            if target not in rooms:
                raise SystemExit(
                    f"interaction has unknown target: {room}/{target}"
                )
            target_room = rooms[target]
            _, _, _, _, _, left, top, right, bottom = target_room
            if (to_x, to_y) != (0, 0) and not (
                    left <= to_x <= right and top <= to_y <= bottom):
                raise SystemExit(
                    f"interaction spawn is not walkable: "
                    f"{room}/{interaction_id}"
                )
            room_links.add((room, target))
        elif action == "minigame":
            if target != "cart_surfer":
                raise SystemExit(f"unknown minigame target: {target}")
        elif action not in {"map", "message"}:
            raise SystemExit(f"unknown interaction action: {action}")

    for room, target in room_links:
        if (target, room) not in room_links:
            raise SystemExit(f"room link has no return path: {room} -> {target}")


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def run_ffmpeg(args: list[str]) -> None:
    subprocess.run(["ffmpeg", "-y", "-hide_banner", "-loglevel", "error",
                    *args], check=True)


def run_magick(args: list[str]) -> None:
    subprocess.run(["magick", *args], check=True)


def make_player_frame(source: Path, frame: tuple[str, int, int, int, int],
                      out: Path) -> None:
    sheet, x, y, w, h = frame
    run_ffmpeg([
        "-i", str(source / "images" / sheet),
        "-f", "lavfi",
        "-i", "color=c=magenta:s=36x38",
        "-filter_complex",
        f"[0:v]crop={w}:{h}:{x}:{y},"
        "scale=34:36:force_original_aspect_ratio=decrease[fg];"
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
    validate_interactions()
    data = out / "data"
    data.mkdir(parents=True, exist_ok=True)
    with (data / "world.tsv").open("w", encoding="utf-8") as f:
        f.write("# name\tdetail\tx\ty\tradius\ttarget\tto_x\tto_y\n")
        for name, detail, x, y, radius, target, to_x, to_y in WORLD_ROWS:
            screen_x = round(x * WORLD_OUTPUT_WIDTH / WORLD_SOURCE_WIDTH)
            screen_y = round(y * WORLD_OUTPUT_HEIGHT / WORLD_SOURCE_HEIGHT)
            screen_radius = max(
                8,
                round(radius * min(
                    WORLD_OUTPUT_WIDTH / WORLD_SOURCE_WIDTH,
                    WORLD_OUTPUT_HEIGHT / WORLD_SOURCE_HEIGHT,
                )),
            )
            row = (name, detail, screen_x, screen_y, screen_radius, target,
                   to_x, to_y)
            f.write("\t".join(str(value) for value in row) + "\n")

    with (data / "rooms.tsv").open("w", encoding="utf-8") as f:
        f.write("# id\ttitle\tbmp\tstart_x\tstart_y\t"
                "walk_left\twalk_top\twalk_right\twalk_bottom\n")
        for row in ROOM_ROWS:
            f.write("\t".join(str(value) for value in row) + "\n")

    with (data / "interactions.tsv").open("w", encoding="utf-8") as f:
        f.write("# room\tid\tx\ty\tradius\taction\ttarget\tlabel\t"
                "to_x\tto_y\n")
        for row in INTERACTION_ROWS:
            f.write("\t".join(str(value) for value in row) + "\n")


def write_manifest(out: Path, source: Path, generated: list[Path],
                   waddle_source: Path | None = None) -> None:
    commit = git_commit(source)
    with (out / "source.manifest").open("w", encoding="utf-8") as f:
        f.write("CLUBPENGUIN_ASSET_MANIFEST_V1\n")
        f.write("source_repo=despedite/clubpenguinfreeroam\n")
        f.write(f"source_commit={commit}\n")
        f.write("source_path=images/sprite-sheet0.png\n")
        f.write("source_path=images/sprite2-sheet0.png\n")
        f.write("source_path=images/sprite2-sheet1.png\n")
        if waddle_source is not None:
            f.write("room_source_repo=nhaar/Waddle-Forever\n")
            f.write(f"room_source_commit={git_commit(waddle_source)}\n")
            cover_logo = (waddle_source / "media/default/websites/modern/"
                          "assets/sites/default/themes/snowball/img/"
                          "club-penguin-logo.png")
            f.write("cover_source=media/default/websites/modern/assets/"
                    "sites/default/themes/snowball/img/club-penguin-logo.png"
                    f"\t{sha256(cover_logo)}\n")
            for room_id, rel in ROOM_SWFS.items():
                swf = waddle_source / rel
                f.write(f"room_source={room_id}\t{rel}\t{sha256(swf)}\n")
        for path in generated:
            f.write(f"generated={path.relative_to(out)}\n")
            f.write(f"sha256={sha256(path)}\n")
        f.write("world_scale=320x220\n")
        f.write("world_coordinate_source=2713x1823\n")
        f.write("player_strip=13x36x38 animation=4x3 selector=1\n")
        f.write("data=data/world.tsv\n")
        f.write("data=data/rooms.tsv\n")
        f.write("data=data/interactions.tsv\n")


def package_room_frames(room_frames: Path, waddle_source: Path, out: Path,
                        generated: list[Path]) -> None:
    rooms_out = out / "rooms"
    rooms_out.mkdir(parents=True, exist_ok=True)

    for room_id, rel in ROOM_SWFS.items():
        frame = room_frames / f"{room_id}.png"
        swf = waddle_source / rel
        room_out = rooms_out / f"{room_id}.bmp"

        if not frame.exists():
            raise SystemExit(f"missing rendered room frame: {frame}")
        if not swf.exists():
            raise SystemExit(f"missing preserved room SWF: {swf}")

        run_ffmpeg([
            "-i", str(frame),
            # The preserved captures are 968x777 Ruffle window screenshots.
            # Crop the displayed 760x480 SWF canvas (scaled to 808x510)
            # before preparing the full device playfield. Scaling the whole
            # window made rooms occupy only about two thirds of the LCD.
            "-vf", "crop=808:510:80:118,scale=320:220",
            "-pix_fmt", "bgr24",
            str(room_out),
        ])
        generated.append(room_out)


def package_cart_surfer(cart_export: Path, waddle_source: Path, out: Path,
                        generated: list[Path]) -> None:
    cart_out = out / "minigames" / "cart_surfer"
    data_out = out / "data"
    source_swf = waddle_source / "media/default/fix/CartSurfer2006.swf"
    title_source = cart_export / "frames" / "1.png"
    tunnel_source = (cart_export / "sprites" / "DefineSprite_201" /
                     "1.png")
    cart_source = cart_export / "sprites" / "DefineSprite_173"
    cart_frames = [1, 9, 5, 10, 15, 22]
    track_frames = [1, 2, 3, 4]

    required = [source_swf, title_source, tunnel_source]
    required.extend(cart_source / f"{frame}.png" for frame in cart_frames)
    required.extend(
        cart_export / "sprites" / "DefineSprite_201" / f"{frame}.png"
        for frame in track_frames
    )
    for path in required:
        if not path.exists():
            raise SystemExit(f"missing Cart Surfer extraction: {path}")
    if shutil.which("magick") is None:
        raise SystemExit("ImageMagick is required to package Cart Surfer")

    cart_out.mkdir(parents=True, exist_ok=True)
    title_out = cart_out / "title.bmp"
    tunnel_out = cart_out / "tunnel.bmp"
    strip_out = cart_out / "cart.bmp"

    run_ffmpeg(["-i", str(title_source), "-vf", "scale=320:220",
                "-pix_fmt", "bgr24", str(title_out)])
    run_ffmpeg(["-i", str(tunnel_source), "-vf", "scale=320:220",
                "-pix_fmt", "bgr24", str(tunnel_out)])

    prepared: list[Path] = []
    for index, frame in enumerate(cart_frames):
        prepared_frame = cart_out / f".cart_frame_{index}.bmp"
        run_magick([
            str(cart_source / f"{frame}.png"), "-trim", "+repage",
            "-resize", "38x38", "-gravity", "center", "-background",
            "magenta", "-alpha", "remove", "-alpha", "off", "-extent",
            "40x40", f"BMP3:{prepared_frame}",
        ])
        prepared.append(prepared_frame)

    hstack_args: list[str] = []
    for frame in prepared:
        hstack_args.extend(["-i", str(frame)])
    cart_row = cart_out / ".cart_row.bmp"
    run_ffmpeg([
        *hstack_args,
        "-filter_complex", f"hstack=inputs={len(prepared)},"
        "pad=320:40:0:0:color=magenta,format=bgr24",
        str(cart_row),
    ])
    for frame in prepared:
        frame.unlink()

    track_prepared: list[Path] = []
    tunnel_frames = cart_export / "sprites" / "DefineSprite_201"
    for index, frame in enumerate(track_frames):
        prepared_frame = cart_out / f".track_frame_{index}.bmp"
        run_ffmpeg([
            "-i", str(tunnel_frames / f"{frame}.png"),
            "-vf", "scale=320:220,crop=80:24:120:125",
            "-pix_fmt", "bgr24", str(prepared_frame),
        ])
        track_prepared.append(prepared_frame)

    hstack_args = []
    for frame in track_prepared:
        hstack_args.extend(["-i", str(frame)])
    track_row = cart_out / ".track_row.bmp"
    run_ffmpeg([
        *hstack_args,
        "-filter_complex", f"hstack=inputs={len(track_prepared)},"
        "format=bgr24", str(track_row),
    ])
    run_ffmpeg([
        "-i", str(cart_row), "-i", str(track_row),
        "-filter_complex", "vstack=inputs=2,format=bgr24", str(strip_out),
    ])
    cart_row.unlink()
    track_row.unlink()
    for frame in track_prepared:
        frame.unlink()

    cart_data = data_out / "cart_surfer.tsv"
    cart_data.write_text(
        "# key\tvalue\n"
        "lives\t4\n"
        "score_ollie\t20\n"
        "score_backflip\t100\n"
        "score_spin\t80\n"
        "score_flap\t50\n"
        "score_grind\t80\n"
        "score_slide\t40\n"
        "score_lean\t10\n"
        "repeat_divisor\t2\n"
        "max_grind_ticks\t28\n"
        "max_slide_ticks\t32\n"
        "max_lean_ticks\t38\n"
        "coin_divisor\t10\n"
        "segments\t1,1,4,2,1,5,3,1,4,2,5,3,1,1,1,4,2,4,2,1,1,5,3,1,1,6\n",
        encoding="utf-8",
    )

    source_manifest = cart_out / "source.manifest"
    source_manifest.write_text(
        "CLUBPENGUIN_CART_SURFER_MANIFEST_V1\n"
        "source_repo=nhaar/Waddle-Forever\n"
        f"source_commit={git_commit(waddle_source)}\n"
        "source_path=media/default/fix/CartSurfer2006.swf\n"
        f"source_sha256={sha256(source_swf)}\n"
        "extractor=JPEXS_FFDec_26.2.1\n"
        "title_frame=main:1\n"
        "tunnel_frame=DefineSprite_201:1\n"
        "cart_frames=DefineSprite_173:1,9,5,10,15,22\n"
        "track_frames=DefineSprite_201:1,2,3,4\n"
        "atlas=320x64 cart=6x40x40 track=4x80x24\n",
        encoding="utf-8",
    )

    generated.extend([title_out, tunnel_out, strip_out, cart_data,
                      source_manifest])


def package_interface(ui_export: Path, waddle_source: Path, out: Path,
                      generated: list[Path]) -> None:
    ui_out = out / "ui"
    source_swf = (waddle_source /
                  "media/default/recreation/interfaces/2010_july.swf")
    source_frame = ui_export / "frames" / "1.png"
    toolbar_out = ui_out / "toolbar.bmp"

    for path in (source_swf, source_frame):
        if not path.exists():
            raise SystemExit(f"missing Club Penguin interface source: {path}")
    if shutil.which("magick") is None:
        raise SystemExit("ImageMagick is required to package the interface")

    ui_out.mkdir(parents=True, exist_ok=True)
    run_magick([
        str(source_frame), "-crop", "563x40+99+440", "+repage",
        "-resize", "320x20!", "-background", "#0b9bd0",
        "-alpha", "remove", "-alpha", "off", f"BMP3:{toolbar_out}",
    ])

    ui_manifest = ui_out / "source.manifest"
    ui_manifest.write_text(
        "CLUBPENGUIN_INTERFACE_MANIFEST_V1\n"
        "source_repo=nhaar/Waddle-Forever\n"
        f"source_commit={git_commit(waddle_source)}\n"
        "source_path=media/default/recreation/interfaces/2010_july.swf\n"
        f"source_sha256={sha256(source_swf)}\n"
        "extractor=JPEXS_FFDec_26.2.1\n"
        "toolbar_frame=main:1 crop=563x40+99+440\n",
        encoding="utf-8",
    )
    generated.extend([toolbar_out, ui_manifest])


def package_freeroam(source: Path, out: Path,
                     room_frames: Path | None = None,
                     waddle_source: Path | None = None,
                     cart_export: Path | None = None,
                     ui_export: Path | None = None) -> None:
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
        "-vf", "scale=320:220",
        "-pix_fmt", "bgr24",
        str(world_out),
    ])
    generated.append(world_out)

    frames = [
        ("sprite2-sheet0.png", 1, 1, 36, 35),
        ("sprite2-sheet0.png", 39, 1, 35, 35),
        ("sprite2-sheet0.png", 76, 36, 34, 35),
        ("sprite2-sheet0.png", 1, 77, 22, 41),
        ("sprite2-sheet0.png", 88, 73, 24, 41),
        ("sprite2-sheet1.png", 1, 1, 22, 41),
        ("sprite2-sheet0.png", 76, 1, 37, 33),
        ("sprite2-sheet0.png", 31, 38, 28, 36),
        ("sprite2-sheet0.png", 1, 38, 28, 37),
        ("sprite2-sheet0.png", 31, 76, 23, 42),
        ("sprite2-sheet0.png", 61, 73, 25, 40),
        ("sprite2-sheet1.png", 25, 1, 22, 41),
    ]
    frame_paths = []
    for i, frame in enumerate(frames):
        frame_path = out / f".player_frame_{i}.bmp"
        make_player_frame(source, frame, frame_path)
        frame_paths.append(frame_path)

    selector_path = out / ".player_selector.bmp"
    run_magick([
        str(frame_paths[0]), "-trim", "+repage", "-resize", "20x22",
        "-gravity", "south", "-background", "magenta", "-extent",
        "36x38", f"BMP3:{selector_path}",
    ])
    frame_paths.append(selector_path)

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

    cover_logo = None
    if waddle_source is not None:
        cover_logo = (waddle_source / "media/default/websites/modern/"
                      "assets/sites/default/themes/snowball/img/"
                      "club-penguin-logo.png")
    if cover_logo is not None and cover_logo.exists():
        run_magick([
            str(world_src), "-crop", "2600x1660+55+55", "+repage",
            "-resize", "120x140^", "-gravity", "west",
            "-extent", "120x140", "(", str(cover_logo), "-resize",
            "110x53", ")", "-gravity", "north", "-geometry", "+0+8",
            "-composite",
            f"BMP3:{cover_out}",
        ])
    else:
        run_ffmpeg([
            "-i", str(world_src),
            "-vf", "scale=120:140:force_original_aspect_ratio=increase,"
                   "crop=120:140",
            "-pix_fmt", "bgr24", str(cover_out),
        ])
    generated.append(cover_out)
    shutil.copyfile(cover_out, cover_space_out)
    shutil.copyfile(cover_out, cover_lower_out)
    shutil.copyfile(cover_out, cover_pane_out)
    shutil.copyfile(cover_out, cover_lower_pane_out)
    generated.extend([
        cover_space_out, cover_lower_out, cover_pane_out, cover_lower_pane_out
    ])

    if room_frames is not None and waddle_source is not None:
        package_room_frames(room_frames, waddle_source, out, generated)

    if cart_export is not None and waddle_source is not None:
        package_cart_surfer(cart_export, waddle_source, out, generated)

    if ui_export is not None and waddle_source is not None:
        package_interface(ui_export, waddle_source, out, generated)

    write_tsvs(out)
    write_manifest(out, source, generated, waddle_source)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path,
                        help="path to a checked-out preserved asset repo")
    parser.add_argument("--out", type=Path,
                        default=Path("assets/ipodjs/rockbox/clubpenguin"))
    parser.add_argument("--room-frames", type=Path,
                        help="directory of rendered <room_id>.png frames")
    parser.add_argument("--waddle-source", type=Path,
                        help="path to a pinned nhaar/Waddle-Forever checkout")
    parser.add_argument("--cart-export", type=Path,
                        help="JPEXS export of the pinned CartSurfer2006 SWF")
    parser.add_argument("--ui-export", type=Path,
                        help="JPEXS export of the pinned 2010 interface SWF")
    args = parser.parse_args()

    if (args.room_frames is None) != (args.waddle_source is None):
        parser.error("--room-frames and --waddle-source must be used together")
    if args.cart_export is not None and args.waddle_source is None:
        parser.error("--cart-export requires --waddle-source")
    if args.ui_export is not None and args.waddle_source is None:
        parser.error("--ui-export requires --waddle-source")

    package_freeroam(
        args.source.resolve(),
        args.out,
        args.room_frames.resolve() if args.room_frames else None,
        args.waddle_source.resolve() if args.waddle_source else None,
        args.cart_export.resolve() if args.cart_export else None,
        args.ui_export.resolve() if args.ui_export else None,
    )


if __name__ == "__main__":
    main()
