"""Search and download public-domain magazine PDFs from the Internet
Archive's magazine_rack collection. Only Archive.org's own public
`advancedsearch.php` and per-item metadata/download endpoints are used, and
only items whose Archive.org rights metadata marks them public domain or
openly licensed are surfaced. This is not a scraper for a copyrighted-
content mirror: no authentication bypass, DRM circumvention, or unlicensed-
content indexing is in scope."""

from __future__ import annotations

import os
import sys
import time
from dataclasses import dataclass
from pathlib import Path


class MagazineStoreError(RuntimeError):
    """Raised when a magazine store search or download cannot be started."""


@dataclass(frozen=True)
class MagazineStoreSearchRequest:
    command: list[str]
    env: dict[str, str]
    output_path: str


@dataclass(frozen=True)
class MagazineStoreDownloadRequest:
    command: list[str]
    env: dict[str, str]
    output_dir: str
    log_path: str


class ArchiveOrgMagazineClient:
    """Builds out-of-process search/download requests against archive.org."""

    def __init__(self, config, repo_root):
        self._config = config
        self._repo_root = os.path.abspath(repo_root)

    @property
    def state_dir(self):
        cache_dir = os.path.abspath(os.path.expanduser(self._config.cache_dir))
        return os.path.join(cache_dir, "magazine-store")

    @property
    def browse_dir(self):
        return os.path.join(self.state_dir, "browse")

    @property
    def cover_dir(self):
        return os.path.join(self.browse_dir, "covers")

    @property
    def log_dir(self):
        return os.path.join(self.state_dir, "logs")

    @property
    def download_dir(self):
        return os.path.join(self.state_dir, "downloads")

    def new_log_path(self):
        os.makedirs(self.log_dir, exist_ok=True)
        stamp = time.strftime("%Y%m%d-%H%M%S")
        return os.path.join(self.log_dir, f"magazine-store-{stamp}.log")

    def prepare_search(self, query, limit=24):
        text = str(query or "").strip()
        if not text:
            raise MagazineStoreError("Enter a title, publisher, or topic to search for.")
        os.makedirs(self.browse_dir, exist_ok=True)
        os.makedirs(self.cover_dir, exist_ok=True)
        stamp = time.strftime("%Y%m%d-%H%M%S")
        output_path = os.path.join(self.browse_dir, f"search-{stamp}.json")
        script_path = (
            Path(__file__).resolve().parents[1]
            / "scripts"
            / "magazine_store_search.py"
        )
        command = [
            sys.executable,
            str(script_path),
            "--query",
            text,
            "--limit",
            str(max(1, min(60, int(limit or 24)))),
            "--output",
            output_path,
            "--cover-dir",
            self.cover_dir,
        ]
        return MagazineStoreSearchRequest(
            command=command, env=os.environ.copy(), output_path=output_path
        )

    def prepare_download(self, identifier):
        item_id = str(identifier or "").strip()
        if not item_id:
            raise MagazineStoreError("Choose a magazine to download.")
        os.makedirs(self.download_dir, exist_ok=True)
        log_path = self.new_log_path()
        script_path = (
            Path(__file__).resolve().parents[1]
            / "scripts"
            / "magazine_store_download.py"
        )
        command = [
            sys.executable,
            str(script_path),
            "--identifier",
            item_id,
            "--output-dir",
            self.download_dir,
        ]
        with open(log_path, "w") as handle:
            handle.write("RockPod Magazine Store download log\n")
            handle.write(f"Started: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
            handle.write(f"Identifier: {item_id}\n")
            handle.write(f"Command: {' '.join(command)}\n\n")
        return MagazineStoreDownloadRequest(
            command=command,
            env=os.environ.copy(),
            output_dir=self.download_dir,
            log_path=log_path,
        )


def parse_magazine_download_progress(lines):
    if isinstance(lines, str):
        items = [line.strip() for line in lines.splitlines() if line.strip()]
    else:
        items = [str(line or "").strip() for line in (lines or []) if str(line or "").strip()]
    phase = "Working"
    progress = ""
    detail = items[-1][-500:] if items else ""

    for line in items:
        if line.startswith("ROCKPOD_MAGAZINE_PROGRESS="):
            phase = "Downloading"
            progress = line.split("=", 1)[1].strip()
            detail = line[-500:]
            continue
        if line.startswith("ROCKPOD_MAGAZINE_OUTPUT="):
            phase = "Completed"
            progress = "100%"
            detail = os.path.basename(line.split("=", 1)[1].strip()) or "Download complete"

    return {"phase": phase, "progress": progress, "detail": detail}
