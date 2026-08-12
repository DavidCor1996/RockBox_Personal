#!/usr/bin/env python3
"""Generate the companion's instant library cache from Rockbox tagcache."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "rockpod"))

from services.rockbox_tagcache import read_rockbox_tagcache_tracks  # noqa: E402


HEADER = (
    "# rockpod-library-v1\n"
    "path\ttitle\tartist\talbum\talbum_artist\tgenre\tyear\tdisc\ttrack\t"
    "length_ms\tbitrate_kbps\tplay_count\tplay_time_ms\tlast_played\t"
    "rating\tlast_elapsed_ms\tlast_offset\tmtime\tartwork_path\n"
)


def clean(value: object) -> str:
    return str(value if value is not None else "").replace("\t", " ").replace(
        "\r", " "
    ).replace("\n", " ")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mount", type=Path, help="mounted iPod or simdisk")
    args = parser.parse_args()
    mount = args.mount.resolve()
    tracks = read_rockbox_tagcache_tracks(str(mount))
    destination = mount / ".rockbox/rockpod/phone/library-v1.tsv"
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(".tsv.new")
    artwork_by_folder: dict[Path, str] = {}

    with temporary.open("w", encoding="utf-8", newline="\n") as output:
        output.write(HEADER)
        for track in tracks:
            path = clean(track.get("device_path"))
            if path and not path.startswith("/"):
                path = "/" + path
            full_path = mount / path.lstrip("/")
            folder = full_path.parent
            if folder not in artwork_by_folder:
                artwork_by_folder[folder] = ""
                for name in (
                    "cover.51x51.bmp", "cover.jpg", "cover.jpeg", "cover.png",
                    "cover.bmp", "folder.jpg", "folder.png",
                ):
                    candidate = folder / name
                    if candidate.is_file():
                        artwork_by_folder[folder] = "/" + candidate.relative_to(mount).as_posix()
                        break
            mtime = track.get("mtime") or 0
            if not mtime:
                try:
                    mtime = int(full_path.stat().st_mtime)
                except OSError:
                    pass
            fields = (
                path,
                track.get("title"),
                track.get("artist"),
                track.get("album"),
                track.get("album_artist"),
                track.get("genre"),
                track.get("year") or 0,
                max(int(track.get("disc_number") or 0), 0),
                max(int(track.get("track_number") or 0), 0),
                round(float(track.get("duration") or 0) * 1000),
                track.get("bitrate") or 0,
                track.get("play_count") or 0,
                track.get("play_time") or 0,
                track.get("last_played") or 0,
                track.get("rating") or 0,
                track.get("last_elapsed") or 0,
                track.get("last_offset") or 0,
                mtime,
                artwork_by_folder[folder],
            )
            output.write("\t".join(clean(field) for field in fields) + "\n")

        video_index = mount / ".rockbox/videolist/index.tsv"
        if video_index.is_file():
            with video_index.open("r", encoding="utf-8", errors="replace") as video:
                for line in video:
                    line = line.rstrip("\r\n")
                    if not line or line.startswith("#") or line.startswith("video_id\t"):
                        continue
                    output.write("@video\t" + line + "\n")
        output.flush()
        os.fsync(output.fileno())
    temporary.replace(destination)
    print(f"library cache: generated {len(tracks)} tracks at {destination}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
