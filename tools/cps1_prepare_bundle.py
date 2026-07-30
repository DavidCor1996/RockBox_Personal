#!/usr/bin/env python3
"""Stage a user-owned CPS1 archive bundle for Rockbox/iPodJS."""

from __future__ import annotations

import argparse
import io
import json
import os
import shutil
import sys
import urllib.parse
import urllib.request
import zipfile
from dataclasses import dataclass
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    Image = None


PLUGIN = "/.rockbox/rocks/games/cps1.rock"
ROM_ROOT = ".rockbox/games/cps1/roms"
COVER_ROOT = ".rockbox/games/library/covers/cps1"
MANIFEST = ".rockbox/rocks/games/cps1/games.tsv"
REPORT = ".rockbox/games/cps1/install-report.json"
BOXART_BASE = "https://thumbnails.libretro.com/MAME/Named_Boxarts/"


@dataclass(frozen=True)
class Game:
    archive: str
    title: str
    year: int
    genre: str
    art: str


GAMES = (
    Game("1941", "1941: Counter Attack", 1990, "Shoot 'em up", "1941 - Counter Attack (World).png"),
    Game("3wonders", "Three Wonders", 1991, "Action", "Three Wonders (World 910520).png"),
    Game("captcomm", "Captain Commando", 1991, "Beat 'em up", "Captain Commando (USA 910928).png"),
    Game("cawing", "Carrier Air Wing", 1990, "Shoot 'em up", "Carrier Air Wing (World 901012).png"),
    Game("cworld2j", "Capcom World 2", 1992, "Quiz", "Capcom World 2 (Japan 920611).png"),
    Game("dino", "Cadillacs and Dinosaurs", 1993, "Beat 'em up", "Cadillacs and Dinosaurs (World 930201).png"),
    Game("dynwar", "Dynasty Wars", 1989, "Beat 'em up", "Dynasty Wars (USA, B-Board 89624B-_).png"),
    Game("ffight", "Final Fight", 1989, "Beat 'em up", "Final Fight (World).png"),
    Game("forgottn", "Forgotten Worlds", 1988, "Shoot 'em up", "Forgotten Worlds (World).png"),
    Game("ghouls", "Ghouls'n Ghosts", 1988, "Platformer", "Ghouls'n Ghosts (World).png"),
    Game("knights", "Knights of the Round", 1991, "Beat 'em up", "Knights of the Round (World 911127).png"),
    Game("kod", "The King of Dragons", 1991, "Beat 'em up", "The King of Dragons (USA 910910).png"),
    Game("megaman", "Mega Man: The Power Battle", 1995, "Fighting", "Mega Man - The Power Battle (CPS1 Asia 951006).png"),
    Game("mercs", "Mercs", 1990, "Run and gun", "Mercs (World 900302).png"),
    Game("msword", "Magic Sword", 1990, "Platformer", "Magic Sword - Heroic Fantasy (World 900725).png"),
    Game("mtwins", "Mega Twins", 1990, "Platformer", "Mega Twins (World 900619).png"),
    Game("nemo", "Nemo", 1990, "Platformer", "Nemo (World 901130).png"),
    Game("pang3", "Pang! 3", 1995, "Puzzle", "Pang! 3 (Euro 950511).png"),
    Game("pnickj", "Pnickies", 1994, "Puzzle", "Pnickies (Japan 940608).png"),
    Game("punisher", "The Punisher", 1993, "Beat 'em up", "The Punisher (World 930422).png"),
    Game("qad", "Quiz & Dragons", 1992, "Quiz", "Quiz _ Dragons (US 920701).png"),
    Game("qtono2", "Quiz Tonosama no Yabou 2", 1995, "Quiz", "Quiz Tonosama no Yabou 2 Zenkoku-ban (Japan 950123).png"),
    Game("sf2", "Street Fighter II: The World Warrior", 1991, "Fighting", "Street Fighter II_ The World Warrior (World 910522).png"),
    Game("sf2ce", "Street Fighter II': Champion Edition", 1992, "Fighting", "Street Fighter II'_ Champion Edition (World 920313).png"),
    Game("sfzch", "Street Fighter Zero", 1995, "Fighting", "Street Fighter Zero (Japan 950727).png"),
    Game("slammast", "Saturday Night Slam Masters", 1993, "Fighting", "Saturday Night Slam Masters (World 930713).png"),
    Game("strider", "Strider", 1989, "Platformer", "Strider (US set 1).png"),
    Game("unsquad", "U.N. Squadron", 1989, "Shoot 'em up", "U.N. Squadron (USA).png"),
    Game("varth", "Varth: Operation Thunderstorm", 1992, "Shoot 'em up", "Varth_ Operation Thunderstorm (World 920714).png"),
    Game("willow", "Willow", 1989, "Platformer", "Willow (World).png"),
    Game("wof", "Warriors of Fate", 1992, "Beat 'em up", "Warriors of Fate (World 921031).png"),
)

UNSUPPORTED = {
    "3wonderh", "captcomb", "daimakr2", "dinopic", "dinopic2", "dynwaru",
    "ffightj2", "ffightub", "forgott1", "neogeo", "punipic", "punipic2",
    "punipic3", "sf2m1", "sf2m3", "sf2mdt", "sf2rb3", "sf2toryu",
    "sf2turyu", "sf2turyu1", "wofhfh",
}


def clean_field(value: object) -> str:
    return str(value).replace("\t", " ").replace("\r", " ").replace("\n", " ").strip()


def extract_archives(bundle: Path, destination: Path) -> list[str]:
    destination.mkdir(parents=True, exist_ok=True)
    names = []
    with zipfile.ZipFile(bundle) as outer:
        for info in outer.infolist():
            name = Path(info.filename).name
            if not name.lower().endswith(".zip") or name != info.filename:
                continue
            payload = outer.read(info)
            with zipfile.ZipFile(io.BytesIO(payload)) as inner:
                if inner.testzip() is not None:
                    raise ValueError(f"CRC failure inside {name}")
            output = destination / name
            temporary = output.with_suffix(".zip.tmp")
            temporary.write_bytes(payload)
            os.replace(temporary, output)
            names.append(Path(name).stem.lower())
    return sorted(names)


def extract_chip_roms(rom_root: Path, archives: list[str]) -> int:
    extracted = 0

    for archive_name in archives:
        archive_path = rom_root / f"{archive_name}.zip"
        destination = rom_root / archive_name
        destination.mkdir(parents=True, exist_ok=True)
        seen: set[str] = set()
        with zipfile.ZipFile(archive_path) as archive:
            for info in archive.infolist():
                if info.is_dir():
                    continue
                member = Path(info.filename).name
                if not member or member.casefold() in seen:
                    raise ValueError(
                        f"unsafe or duplicate chip ROM in {archive_path.name}: "
                        f"{info.filename}"
                    )
                seen.add(member.casefold())
                payload = archive.read(info)
                output = destination / member
                temporary = output.with_name(output.name + ".tmp")
                temporary.write_bytes(payload)
                os.replace(temporary, output)
                extracted += 1
    return extracted


def fetch_cover(game: Game, destination: Path) -> bool:
    if destination.is_file():
        return True
    if Image is None:
        raise RuntimeError("Pillow is required to prepare CPS1 artwork")
    url = BOXART_BASE + urllib.parse.quote(game.art)
    request = urllib.request.Request(url, headers={"User-Agent": "RockPod-CPS1/1"})
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            payload = response.read()
        with Image.open(io.BytesIO(payload)) as source:
            image = source.convert("RGB")
            image.thumbnail((144, 108), Image.Resampling.LANCZOS)
            canvas = Image.new("RGB", (144, 108), (27, 40, 56))
            canvas.paste(image, ((144 - image.width) // 2, (108 - image.height) // 2))
            destination.parent.mkdir(parents=True, exist_ok=True)
            temporary = destination.with_suffix(".bmp.tmp")
            canvas.save(temporary, format="BMP")
            os.replace(temporary, destination)
        return True
    except Exception as exc:
        print(f"cover warning: {game.archive}: {exc}", file=sys.stderr)
        return False


def write_manifest(stage: Path, installed: set[str], fetch_covers: bool) -> tuple[list[str], list[str]]:
    path = stage / MANIFEST
    path.parent.mkdir(parents=True, exist_ok=True)
    published = []
    missing_covers = []
    lines = [
        "# title\tplugin\tcover\tfavorite\tsave_hint\tyear\tgenre\tpublisher\tdeveloper\tdescription\tplugin_param"
    ]
    for game in GAMES:
        if game.archive not in installed:
            continue
        cover_rel = f"/{COVER_ROOT}/{game.archive}.bmp"
        cover = stage / cover_rel.lstrip("/")
        if fetch_covers:
            fetch_cover(game, cover)
        if not cover.is_file():
            missing_covers.append(game.archive)
            continue
        fields = (
            game.title,
            PLUGIN,
            cover_rel,
            "0",
            "",
            game.year,
            game.genre,
            "Capcom",
            "Capcom",
            f"The installed CPS1 arcade release of {game.title}.",
            f"/{ROM_ROOT}/{game.archive}.zip",
        )
        lines.append("\t".join(clean_field(field) for field in fields))
        published.append(game.archive)
    temporary = path.with_suffix(".tsv.tmp")
    temporary.write_text("\n".join(lines) + "\n", encoding="utf-8")
    os.replace(temporary, path)
    return published, missing_covers


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("bundle", type=Path)
    parser.add_argument("stage", type=Path)
    parser.add_argument("--fetch-covers", action="store_true")
    parser.add_argument("--extract-chip-roms", action="store_true")
    args = parser.parse_args()

    if not args.bundle.is_file():
        parser.error(f"bundle does not exist: {args.bundle}")
    args.stage.mkdir(parents=True, exist_ok=True)
    archives = extract_archives(args.bundle, args.stage / ROM_ROOT)
    extracted_chip_count = (
        extract_chip_roms(args.stage / ROM_ROOT, archives)
        if args.extract_chip_roms
        else 0
    )
    installed = set(archives)
    missing_parents = sorted(game.archive for game in GAMES if game.archive not in installed)
    published, missing_covers = write_manifest(
        args.stage, installed, args.fetch_covers
    )
    report = {
        "schema": 1,
        "source_bundle": str(args.bundle.resolve()),
        "archive_count": len(archives),
        "expected_archive_count": 137,
        "extracted_chip_count": extracted_chip_count,
        "parent_game_count": len(GAMES),
        "published_game_count": len(published),
        "published_games": published,
        "missing_parent_archives": missing_parents,
        "unsupported_archives": sorted(UNSUPPORTED & installed),
        "supported_clone_archives": sorted(
            installed - {game.archive for game in GAMES} - UNSUPPORTED
        ),
        "missing_covers": missing_covers,
    }
    report_path = args.stage / REPORT
    report_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = report_path.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    os.replace(temporary, report_path)
    print(json.dumps(report, indent=2))
    return 0 if not missing_parents and not missing_covers else 1


if __name__ == "__main__":
    raise SystemExit(main())
