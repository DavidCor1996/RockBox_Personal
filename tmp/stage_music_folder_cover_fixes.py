#!/usr/bin/env python3
import argparse
import shutil
import subprocess
from pathlib import Path


AUDIO_EXTS = {
    ".aac",
    ".aif",
    ".aiff",
    ".flac",
    ".m4a",
    ".mp3",
    ".mp4",
    ".ogg",
    ".opus",
    ".wav",
}


def has_audio(album_dir: Path) -> bool:
    return any(p.is_file() and p.suffix.lower() in AUDIO_EXTS for p in album_dir.iterdir())


def cover_source(ipod_album: Path, local_album: Path) -> Path | None:
    names = ("cover.jpg", "cover.jpeg", "folder.jpg", "folder.jpeg", "cover.png", "folder.png")
    for base in (ipod_album, local_album):
        if not base.exists():
            continue
        files = {p.name.lower(): p for p in base.iterdir() if p.is_file()}
        for name in names:
            if name in files:
                return files[name]
    return None


def identify(path: Path) -> str:
    proc = subprocess.run(
        ["identify", "-format", "%wx%h %[colorspace] %[interlace]", str(path)],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )
    return proc.stdout.strip() if proc.returncode == 0 else ""


def generate_jpg(src: Path, dst: Path, size: tuple[int, int]) -> bool:
    width, height = size
    dst.parent.mkdir(parents=True, exist_ok=True)
    proc = subprocess.run(
        [
            "magick",
            str(src),
            "-auto-orient",
            "-resize",
            f"{width}x{height}^",
            "-gravity",
            "center",
            "-extent",
            f"{width}x{height}",
            "-colorspace",
            "sRGB",
            "-type",
            "TrueColor",
            "-strip",
            "-interlace",
            "none",
            "-quality",
            "90",
            str(dst),
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if proc.returncode != 0:
        print(f"FAILED\t{dst}\t{proc.stderr.strip()}")
        return False
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ipod-music", default="/run/media/david/DAVID_S IPO/Music")
    parser.add_argument("--local-music", default="/home/david/Music")
    parser.add_argument("--stage", default="/tmp/ipod_music_cover_fixes")
    parser.add_argument("--write-stage", action="store_true")
    args = parser.parse_args()

    ipod_music = Path(args.ipod_music)
    local_music = Path(args.local_music)
    stage = Path(args.stage)
    sizes = ((51, 51), (128, 128), (138, 138), (288, 75))

    work = []
    missing_cover = []
    skipped = []
    for ipod_album in sorted(p for p in ipod_music.iterdir() if p.is_dir()):
        if not has_audio(ipod_album):
            continue
        local_album = local_music / ipod_album.name
        src = cover_source(ipod_album, local_album)
        if src is None:
            skipped.append(ipod_album.name)
            continue
        rel = ipod_album.relative_to(ipod_music)
        if not (ipod_album / "cover.jpg").exists():
            missing_cover.append((src, stage / rel / "cover.jpg", ipod_album.name))
        for size in sizes:
            width, height = size
            target_name = f"cover.{width}x{height}.jpg"
            target = ipod_album / target_name
            expected = f"{width}x{height} sRGB None"
            if not target.exists() or identify(target) != expected:
                work.append((src, stage / rel / target_name, ipod_album.name, size))

    print(f"Albums skipped without usable source cover: {len(skipped)}")
    for name in skipped:
        print(f"SKIP\t{name}")
    print(f"Missing literal cover.jpg files to stage: {len(missing_cover)}")
    for _, _, name in missing_cover:
        print(f"COVER\t{name}")
    print(f"WPS-sized cover files to stage: {len(work)}")
    for _, _, name, size in work:
        print(f"WPS\t{size[0]}x{size[1]}\t{name}")

    if not args.write_stage:
        return 0

    if stage.exists():
        shutil.rmtree(stage)
    staged = 0
    for src, dst, _, _ in work:
        if generate_jpg(src, dst, tuple(map(int, dst.stem.rsplit(".", 1)[1].split("x")))):
            staged += 1
    for src, dst, _ in missing_cover:
        dst.parent.mkdir(parents=True, exist_ok=True)
        if generate_jpg(src, dst, (600, 600)):
            staged += 1
    print(f"Staged files: {staged}")
    print(stage)
    return 0 if staged == len(work) + len(missing_cover) else 1


if __name__ == "__main__":
    raise SystemExit(main())
