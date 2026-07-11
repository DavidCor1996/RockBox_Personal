#!/usr/bin/env python3
import io
import json
import re
import unicodedata
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

from PIL import Image, ImageOps


ROOT = Path("/tmp/arduboy_collection_stage")
ROMS = ROOT / "roms"
COVERS = ROOT / "covers"
MANIFEST = ROOT / "games.tsv"
REPO_JSON = Path("/tmp/arduboy-repo.json")
COVER_SIZE = (128, 128)
COVER_INSET = (120, 116)


def clean_name(text, fallback):
    text = unicodedata.normalize("NFKD", text or fallback)
    text = text.encode("ascii", "ignore").decode("ascii")
    text = re.sub(r"[\\/:*?\"<>|]+", " ", text)
    text = re.sub(r"\s+", " ", text).strip(" .")
    if not text:
        text = fallback
    return text[:72]


def urlopen_bytes(url):
    req = urllib.request.Request(url, headers={"User-Agent": "rockbox-arduboy-stage"})
    with urllib.request.urlopen(req, timeout=60) as response:
        return response.read()


def cover_url(item):
    if item.get("banner"):
        return item["banner"]
    screenshots = item.get("screenshots") or []
    if screenshots:
        first = screenshots[0]
        if isinstance(first, dict):
            return first.get("filename") or first.get("url")
        return first
    return ""


def write_cover(image_bytes, out_path, title):
    canvas = Image.new("RGB", COVER_SIZE, (232, 233, 235))
    img = Image.open(io.BytesIO(image_bytes)).convert("RGB")
    img = ImageOps.contain(img, COVER_INSET, Image.Resampling.LANCZOS)
    x = (COVER_SIZE[0] - img.width) // 2
    y = (COVER_SIZE[1] - img.height) // 2
    canvas.paste(img, (x, y))
    canvas.save(out_path, "BMP")


def fallback_cover(out_path, title):
    canvas = Image.new("RGB", COVER_SIZE, (232, 233, 235))
    canvas.save(out_path, "BMP")


def stage_item(item):
    binary = item["binaries"][0]["filename"]
    title = item.get("title") or item["binaries"][0].get("title") or item["id"]
    safe = clean_name(title, item["id"])
    stem = f"{safe} [{item['id']}]"
    rom_path = ROMS / f"{stem}.hex"
    cover_path = COVERS / f"{stem}.bmp"

    rom_path.write_bytes(urlopen_bytes(binary))
    art = cover_url(item)
    if art:
        try:
            write_cover(urlopen_bytes(art), cover_path, title)
        except Exception:
            fallback_cover(cover_path, title)
    else:
        fallback_cover(cover_path, title)

    return {
        "id": item["id"],
        "title": title.replace("\t", " "),
        "rom": f"/.rockbox/games/arduboy/roms/{rom_path.name}",
        "cover": f"/.rockbox/games/library/covers/arduboy/{cover_path.name}",
        "license": item.get("license", ""),
    }


def main():
    ROOT.mkdir(parents=True, exist_ok=True)
    ROMS.mkdir(parents=True, exist_ok=True)
    COVERS.mkdir(parents=True, exist_ok=True)

    items = json.loads(REPO_JSON.read_text())["items"]
    staged = []
    failures = []
    with ThreadPoolExecutor(max_workers=8) as pool:
        futures = [pool.submit(stage_item, item) for item in items]
        for future in as_completed(futures):
            try:
                staged.append(future.result())
            except Exception as exc:
                failures.append(str(exc))

    staged.sort(key=lambda row: row["title"].lower())
    with MANIFEST.open("w", encoding="utf-8", newline="\n") as out:
        out.write("id\ttitle\tfile\tcover\tfavorite\tlast_played\thaptic_profile\n")
        for row in staged:
            out.write(
                f"{row['id']}\t{row['title']}\t{row['rom']}\t{row['cover']}\t0\t\t\n"
            )

    print(f"staged={len(staged)} roms={len(list(ROMS.glob('*.hex')))} covers={len(list(COVERS.glob('*.bmp')))}")
    if failures:
        print(f"failures={len(failures)}")
        for failure in failures[:20]:
            print(failure)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
