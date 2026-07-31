#!/usr/bin/env python3
"""Download a public-domain comic archive from the Internet Archive.

Prefers a CBZ derivative; falls back to CBR or, if neither exists, the
item's PDF (comics_prepare.py can rasterize a PDF the same way
magazine_prepare.py does). Only downloads from archive.org's own public
download endpoint.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from urllib.parse import quote
from urllib.request import Request, urlopen


METADATA = "https://archive.org/metadata/{identifier}"
DOWNLOAD = "https://archive.org/download/{identifier}/{filename}"
SAFE_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")


class DownloadError(RuntimeError):
    pass


def _fetch_json(url: str) -> dict:
    request = Request(url, headers={"User-Agent": "Rockbox-Comics-Store/1"})
    with urlopen(request, timeout=30) as response:
        return json.load(response)


def _select_file(record: dict) -> dict:
    candidates = []
    for entry in record.get("files", []):
        name = str(entry.get("name", ""))
        lowered = name.lower()
        if lowered.endswith(".cbz"):
            score = 100
        elif lowered.endswith(".cbr"):
            score = 90
        elif lowered.endswith(".pdf"):
            score = 50
        else:
            continue
        if entry.get("source") == "original":
            score += 10
        try:
            size = int(entry.get("size", 0))
        except (TypeError, ValueError):
            size = 0
        candidates.append((score, size, entry))
    if not candidates:
        raise DownloadError("item has no CBZ, CBR, or PDF file")
    candidates.sort(key=lambda item: (item[0], item[1]), reverse=True)
    return candidates[0][2]


def download(identifier: str, output_dir: str) -> Path:
    if not SAFE_ID.fullmatch(identifier):
        raise DownloadError("invalid Archive.org identifier")
    record = _fetch_json(METADATA.format(identifier=quote(identifier, safe="")))
    selected = _select_file(record)
    filename = str(selected["name"])
    size = int(selected.get("size", 0) or 0)
    url = DOWNLOAD.format(
        identifier=quote(identifier, safe=""), filename=quote(filename, safe="")
    )

    destination_dir = Path(output_dir)
    destination_dir.mkdir(parents=True, exist_ok=True)
    destination = destination_dir / f"{identifier}{Path(filename).suffix.lower()}"

    request = Request(url, headers={"User-Agent": "Rockbox-Comics-Store/1"})
    copied = 0
    with urlopen(request, timeout=120) as response, destination.open("wb") as output:
        while True:
            block = response.read(1024 * 1024)
            if not block:
                break
            output.write(block)
            copied += len(block)
            if size:
                percent = min(100, copied * 100 // size)
                print(f"ROCKPOD_COMIC_PROGRESS={percent}%", flush=True)
    return destination


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--identifier", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    try:
        destination = download(args.identifier, args.output_dir)
    except (DownloadError, OSError) as exc:
        print(f"comics_store_download: {exc}")
        return 1
    print(f"ROCKPOD_COMIC_OUTPUT={destination}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
