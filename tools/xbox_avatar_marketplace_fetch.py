#!/usr/bin/env python3
"""Fetch the archived Xbox 360 Avatar Marketplace models for a personal pack.

The Models Resource preserves real extracted Xbox 360 Marketplace avatar items.
This tool downloads them once into a local cache so the importer can work
offline; it does not modify or redistribute them.  Provenance for every item is
recorded so the pack can be labelled `archive-extracted` rather than being
mistaken for an official Microsoft download.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import time
import urllib.error
import urllib.request
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SECTION = "https://models.spriters-resource.com/xbox_360/avatarmarketplace/"
BASE = "https://models.spriters-resource.com"
AGENT = "Mozilla/5.0 (X11; Linux x86_64) RockPod personal avatar importer"
DEFAULT_CACHE = ROOT / ".cache/xbox360-marketplace"


def _get(url, referer=None):
    request = urllib.request.Request(url, headers={
        "User-Agent": AGENT,
        **({"Referer": referer} if referer else {}),
    })
    with urllib.request.urlopen(request, timeout=90) as response:
        return response.read()


def list_assets():
    """Return {asset id: title} for the whole Marketplace section."""
    html = _get(SECTION).decode("utf-8", "replace")
    found = re.findall(
        r'href="/xbox_360/avatarmarketplace/asset/(\d+)/"[^>]*>(.*?)</a>',
        html, re.S,
    )
    assets = {}
    for identifier, title in found:
        text = re.sub(r"<[^>]+>", " ", title)
        text = " ".join(text.split()).replace("view_in_ar", "").strip()
        text = (text.replace("&amp;", "&").replace("&#039;", "'")
                    .replace("&quot;", '"'))
        if text and identifier not in assets:
            assets[identifier] = text
    return assets


def fetch_asset(identifier, cache, delay=1.0):
    """Download one asset zip, reusing the cache when it is already present."""
    destination = cache / f"{identifier}.zip"
    if destination.is_file() and destination.stat().st_size > 0:
        return destination, False
    page_url = f"{BASE}/xbox_360/avatarmarketplace/asset/{identifier}/"
    page = _get(page_url).decode("utf-8", "replace")
    match = re.search(r'/media/assets/\d+/\d+\.zip\?updated=\d+', page)
    if not match:
        raise ValueError(f"no download link for asset {identifier}")
    time.sleep(delay)
    payload = _get(BASE + match.group(0), referer=page_url)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(payload)
    return destination, True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", default=str(DEFAULT_CACHE))
    parser.add_argument("--delay", type=float, default=1.0,
                        help="seconds to wait between requests")
    parser.add_argument("--limit", type=int, default=0)
    args = parser.parse_args()

    cache = Path(args.cache)
    cache.mkdir(parents=True, exist_ok=True)
    assets = list_assets()
    print(f"marketplace lists {len(assets)} assets")

    index = {}
    for position, (identifier, title) in enumerate(sorted(assets.items()), 1):
        if args.limit and position > args.limit:
            break
        try:
            path, downloaded = fetch_asset(identifier, cache, args.delay)
        except (urllib.error.URLError, ValueError, OSError) as error:
            print(f"  [{position:3d}/{len(assets)}] {title}: FAILED {error}")
            continue
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        index[identifier] = {
            "title": title,
            "file": path.name,
            "bytes": path.stat().st_size,
            "sha256": digest,
            "page": f"{BASE}/xbox_360/avatarmarketplace/asset/{identifier}/",
        }
        state = "downloaded" if downloaded else "cached"
        print(f"  [{position:3d}/{len(assets)}] {title}: {state} "
              f"{path.stat().st_size // 1024} KiB")
        if downloaded:
            time.sleep(args.delay)

    (cache / "index.json").write_text(
        json.dumps({
            "source": SECTION,
            "provenance": "archive-extracted",
            "note": (
                "Real extracted Xbox 360 Avatar Marketplace items preserved by "
                "The Models Resource. Personal, non-commercial use."
            ),
            "assets": index,
        }, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(f"indexed {len(index)} assets into {cache}")


if __name__ == "__main__":
    main()
