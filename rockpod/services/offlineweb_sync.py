"""Website sync helper for Rockbox Offline Internet archives."""

from __future__ import annotations

import os
import re
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import urlparse


class OfflineWebSyncError(RuntimeError):
    """Raised when website sync cannot be started."""


@dataclass(frozen=True)
class OfflineWebSyncRequest:
    command: list[str]
    env: dict[str, str]
    output_path: str
    log_path: str
    device_root: str


def _normalize_sync_url(value):
    text = str(value or "").strip()
    if not text:
        return ""
    if "://" not in text:
        text = f"https://{text}"
    parsed = urlparse(text)
    if parsed.scheme not in {"http", "https"} or not parsed.netloc:
        return ""
    cleaned = parsed._replace(fragment="")
    return cleaned.geturl()


def parse_sync_urls(value):
    if isinstance(value, (list, tuple, set)):
        tokens = []
        for item in value:
            tokens.extend(parse_sync_urls(item))
    else:
        text = str(value or "")
        tokens = [token for token in re.split(r"[\s,;]+", text) if token]

    normalized = []
    seen = set()
    for token in tokens:
        url = _normalize_sync_url(token)
        if not url or url in seen:
            continue
        seen.add(url)
        normalized.append(url)
    return normalized


class OfflineWebSyncImporter:
    """Build process requests for syncing web pages into Offline Internet."""

    def __init__(self, config):
        self._config = config

    @property
    def state_dir(self):
        cache_dir = os.path.abspath(os.path.expanduser(self._config.cache_dir))
        return os.path.join(cache_dir, "offlineweb-sync")

    @property
    def output_dir(self):
        return os.path.join(self.state_dir, "results")

    @property
    def log_dir(self):
        return os.path.join(self.state_dir, "logs")

    def prepare_sync(self, urls, device_root):
        normalized_urls = parse_sync_urls(urls)
        if not normalized_urls:
            raise OfflineWebSyncError("Enter at least one website URL to cache.")

        mount_root = os.path.abspath(os.path.expanduser(str(device_root or "").strip()))
        if not mount_root:
            raise OfflineWebSyncError("No iPod mount path is configured for website sync.")
        if not os.path.isdir(mount_root):
            raise OfflineWebSyncError(f"Website sync target does not exist: {mount_root}")
        if not os.path.isdir(os.path.join(mount_root, ".rockbox")):
            raise OfflineWebSyncError("Website sync target must contain a .rockbox directory.")

        os.makedirs(self.output_dir, exist_ok=True)
        os.makedirs(self.log_dir, exist_ok=True)
        stamp = time.strftime("%Y%m%d-%H%M%S")
        output_path = os.path.join(self.output_dir, f"sync-{stamp}.json")
        log_path = os.path.join(self.log_dir, f"sync-{stamp}.log")
        script_path = Path(__file__).resolve().parents[1] / "scripts" / "offlineweb_sync.py"

        command = [
            sys.executable,
            str(script_path),
            "--device-root",
            mount_root,
            "--output",
            output_path,
            "--log-path",
            log_path,
            "--use-firefox-cookies",
        ]
        for url in normalized_urls:
            command.extend(["--url", url])

        return OfflineWebSyncRequest(
            command=command,
            env=os.environ.copy(),
            output_path=output_path,
            log_path=log_path,
            device_root=mount_root,
        )
