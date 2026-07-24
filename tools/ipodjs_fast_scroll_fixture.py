#!/usr/bin/env python3
"""Build a deterministic A-Z tagcache for the iPodJS simulator gate."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
ROCKPOD = REPO / "rockpod"
sys.path.insert(0, str(ROCKPOD))

from services.rockbox_tagcache import write_rockbox_tagcache_tracks  # noqa: E402


NAMES = (
    "Aster", "Birch", "Cedar", "Dogwood", "Elm", "Fir", "Ginkgo",
    "Hazel", "Iris", "Juniper", "Kalmia", "Laurel", "Maple", "Nettle",
    "Oak", "Pine", "Quince", "Rowan", "Spruce", "Thyme", "Umbrella",
    "Violet", "Willow", "Xylosma", "Yarrow", "Zinnia",
)

# Album Artist is deliberately a shorter, deduplicated list.  The physical
# library can have many artists/albums but fewer album artists; this fixture
# keeps that route below the ordinary 12-row fast-scroll gate so the simulator
# catches regressions where only Artists and Albums show the stock overlay.
ALBUM_ARTIST_NAMES = (NAMES[0], "Bírch", *NAMES[2:8])


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mount", type=Path)
    parser.add_argument("source_track", type=Path)
    args = parser.parse_args()

    mount = args.mount.resolve()
    source_track = args.source_track.resolve()
    if not source_track.is_file():
        parser.error(f"source track is unavailable: {source_track}")

    tracks = []
    for index, name in enumerate(NAMES, 1):
        album_artist = ALBUM_ARTIST_NAMES[(index - 1) % len(ALBUM_ARTIST_NAMES)]
        relative = Path("Music") / "Fast Scroll" / name / f"{index:02d}.mp3"
        target = mount / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        os.symlink(source_track, target)
        tracks.append(
            {
                "device_path": relative.as_posix(),
                "title": f"{name} Song",
                "artist": f"{name} Artist",
                "album_artist": f"{album_artist} Album Artist",
                "album": f"{name} Album",
                "genre": "Rock",
                "track_number": 1,
                "disc_number": 1,
                "duration": 125,
                "bitrate": 256,
            }
        )

    result = write_rockbox_tagcache_tracks(
        str(mount), tracks, require_tracks=True
    )
    print(f"prepared {result['track_count']} A-Z simulator tracks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
