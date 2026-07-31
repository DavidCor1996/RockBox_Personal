#!/usr/bin/env python3
"""Search the Internet Archive's magazine collection for public-domain PDFs.

Four independent, harder-to-spoof-together signals are required, all
server-side in the query:

- ``collection:magazine_rack`` was dropped in favor of
  ``collection:pulpmagazinearchive`` — a genuinely curated periodicals
  collection (verified live: excludes a fraudulently-tagged 2020 issue of
  *National Geographic* that showed up under the broader magazine_rack
  scope, whereas magazine_rack itself is an open, largely uncurated bucket
  many uploaders post to).
- ``format:("Text PDF" OR "Additional Text PDF")`` — only items with an
  actual PDF file (the "correct file type"). A bare PDF-format check alone
  is far too broad on its own — Archive.org auto-generates this derivative
  for nearly any text-mediatype item — which is why it's combined with the
  curated collection, not used standalone.
- ``year:[1800 TO 1963]`` — the actual US copyright cutoff: pre-1964 works
  needed an explicit renewal after 28 years to stay copyrighted, so only
  works in this window can have legitimately lapsed into the public domain
  via non-renewal.
- ``licenseurl:(*publicdomain* OR *creativecommons*)`` — the rights
  metadata itself. Trusting this field alone was the original design and
  turned out to be spoofable: a live search for "national geographic"
  against the broader (uncurated) scope surfaced a currently-published,
  actively-copyrighted 2020 issue self-tagged CC0 by its uploader. Combined
  with a curated collection and an objective year cutoff, it stops being
  the single point of failure.

This is still an automated heuristic, not a legal determination — which is
why search results also carry cover art for a quick human sanity check
before anything is downloaded.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from urllib.parse import urlencode
from urllib.request import Request, urlopen


ADVANCED_SEARCH = "https://archive.org/advancedsearch.php"
MAGAZINE_COLLECTION_CLAUSE = "collection:pulpmagazinearchive"
PDF_FORMAT_CLAUSE = 'format:("Text PDF" OR "Additional Text PDF")'
YEAR_CUTOFF = 1963
YEAR_CLAUSE = f"year:[1800 TO {YEAR_CUTOFF}]"
OPEN_LICENSE_CLAUSE = "licenseurl:(*publicdomain* OR *creativecommons*)"
OPEN_RIGHTS_MARKERS = ("publicdomain", "creativecommons")


def _fetch_json(url: str) -> dict:
    request = Request(url, headers={"User-Agent": "Rockbox-Magazines-Store/1"})
    with urlopen(request, timeout=30) as response:
        return json.load(response)


def _passes_client_checks(doc: dict) -> bool:
    """Defense-in-depth re-check of what the query already filtered on."""
    licenseurl = str(doc.get("licenseurl") or "").lower()
    if not any(marker in licenseurl for marker in OPEN_RIGHTS_MARKERS):
        return False
    try:
        year = int(str(doc.get("year") or "").strip()[:4])
    except ValueError:
        return False
    return year <= YEAR_CUTOFF


def _cover_url(identifier: str) -> str:
    return f"https://archive.org/services/img/{identifier}"


def _download_cover(url: str, cover_dir: str, key: str) -> str:
    if not url or not cover_dir:
        return ""
    digest = hashlib.sha1(key.encode("utf-8", errors="ignore")).hexdigest()
    target = Path(cover_dir) / f"{digest}.jpg"
    if target.is_file():
        return str(target)
    target.parent.mkdir(parents=True, exist_ok=True)
    request = Request(url, headers={"User-Agent": "Rockbox-Magazines-Store/1"})
    try:
        with urlopen(request, timeout=15) as response:
            data = response.read()
    except OSError:
        return ""
    if not data:
        return ""
    try:
        target.write_bytes(data)
    except OSError:
        return ""
    return str(target)


def search(query: str, limit: int, cover_dir: str = "") -> list[dict]:
    text = str(query or "").strip()
    clauses = [MAGAZINE_COLLECTION_CLAUSE, PDF_FORMAT_CLAUSE, YEAR_CLAUSE, OPEN_LICENSE_CLAUSE]
    q = f"({text}) AND {' AND '.join(clauses)}" if text else " AND ".join(clauses)
    params = {
        "q": q,
        "fl[]": ["identifier", "title", "creator", "year", "licenseurl"],
        "rows": str(max(1, min(100, limit))),
        "output": "json",
    }
    url = f"{ADVANCED_SEARCH}?{urlencode(params, doseq=True)}"
    payload = _fetch_json(url)
    docs = payload.get("response", {}).get("docs", [])

    results = []
    for doc in docs:
        identifier = str(doc.get("identifier") or "").strip()
        if not identifier or not _passes_client_checks(doc):
            continue
        title = str(doc.get("title") or identifier).strip()
        cover_url = _cover_url(identifier)
        results.append(
            {
                "identifier": identifier,
                "title": title,
                "creator": str(doc.get("creator") or "").strip(),
                "year": str(doc.get("year") or "").strip(),
                "cover_url": cover_url,
                "cover_path": _download_cover(cover_url, cover_dir, identifier),
                "details_url": f"https://archive.org/details/{identifier}",
            }
        )
        if len(results) >= limit:
            break
    return results


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--query", required=True)
    parser.add_argument("--limit", type=int, default=24)
    parser.add_argument("--output", required=True)
    parser.add_argument("--cover-dir", default="")
    args = parser.parse_args()

    results = search(args.query, max(1, min(60, args.limit)), args.cover_dir)
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(results, indent=2))
    print(f"Wrote {len(results)} magazine results to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
