"""Personal music sharing over a configurable relay."""

from __future__ import annotations

import json
import os
import secrets
import socket
import time
import uuid
from dataclasses import dataclass
from urllib import parse, request

from services.file_safety import atomic_write_json
from services.streamrip_import import streamrip_url_info


VALID_SHARE_KINDS = {"album", "track", "playlist"}


class MusicSharingError(RuntimeError):
    """Raised when music sharing cannot complete."""


@dataclass(frozen=True)
class MusicShareSettings:
    relay_url: str
    pair_code: str
    display_name: str
    sender_id: str


def _now_iso():
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


def _clean_text(value, limit=500):
    text = " ".join(str(value or "").split())
    return text[:limit]


def normalize_share_item(item):
    data = dict(item or {})
    kind = str(data.get("kind") or data.get("media_type") or "album").strip().lower()
    if kind == "song":
        kind = "track"
    if kind not in VALID_SHARE_KINDS:
        kind = "album"
    url = _clean_text(data.get("url"), 1000)
    info = streamrip_url_info(url)
    media_type = info.get("media_type") or kind
    if media_type not in VALID_SHARE_KINDS:
        media_type = kind
    title = _clean_text(data.get("title") or data.get("name"), 240)
    artist = _clean_text(data.get("artist") or data.get("album_artist"), 240)
    return {
        "kind": media_type,
        "media_type": media_type,
        "title": title,
        "artist": artist,
        "url": url,
        "source": info.get("source") or _clean_text(data.get("source"), 80),
        "source_id": info.get("id") or _clean_text(data.get("id"), 160),
        "note": _clean_text(data.get("note"), 500),
    }


def share_item_to_store_result(item):
    data = normalize_share_item(item)
    return {
        "source": data.get("source", ""),
        "media_type": data.get("media_type") or data.get("kind") or "album",
        "id": data.get("source_id", ""),
        "title": data.get("title", ""),
        "artist": data.get("artist", ""),
        "url": data.get("url", ""),
        "section": "Shared",
    }


class MusicSharingService:
    """Stores local share history and exchanges share cards via a relay."""

    def __init__(self, config):
        self._config = config
        self._store_path = os.path.join(config.get("cache_dir"), "music-sharing.json")
        self._ensure_identity()

    def _ensure_identity(self):
        changed = False
        if not self._config.get("music_share_sender_id", ""):
            self._config.set("music_share_sender_id", str(uuid.uuid4()))
            changed = True
        if not self._config.get("music_share_pair_code", ""):
            self._config.set("music_share_pair_code", secrets.token_urlsafe(12))
            changed = True
        if not self._config.get("music_share_display_name", ""):
            self._config.set("music_share_display_name", self._default_display_name())
            changed = True
        if changed:
            self._config.save()

    @staticmethod
    def _default_display_name():
        for value in (os.environ.get("USER"), os.environ.get("USERNAME"), socket.gethostname()):
            text = _clean_text(value, 80)
            if text:
                return text
        return "RockPod"

    def settings(self):
        return MusicShareSettings(
            relay_url=str(self._config.get("music_share_relay_url", "") or "").strip(),
            pair_code=str(self._config.get("music_share_pair_code", "") or "").strip(),
            display_name=str(self._config.get("music_share_display_name", "") or "").strip(),
            sender_id=str(self._config.get("music_share_sender_id", "") or "").strip(),
        )

    def set_settings(self, relay_url=None, pair_code=None, display_name=None):
        if relay_url is not None:
            self._config.set("music_share_relay_url", str(relay_url or "").strip().rstrip("/"))
        if pair_code is not None:
            code = str(pair_code or "").strip()
            if not code:
                raise MusicSharingError("Pair code cannot be blank.")
            self._config.set("music_share_pair_code", code)
        if display_name is not None:
            name = _clean_text(display_name, 80)
            if not name:
                raise MusicSharingError("Display name cannot be blank.")
            self._config.set("music_share_display_name", name)
        self._config.save()

    def _load_store(self):
        if not os.path.exists(self._store_path):
            return {"inbox": [], "outbox": [], "last_seen": 0}
        try:
            with open(self._store_path, "r") as handle:
                data = json.load(handle)
        except (OSError, json.JSONDecodeError):
            return {"inbox": [], "outbox": [], "last_seen": 0}
        if not isinstance(data, dict):
            return {"inbox": [], "outbox": [], "last_seen": 0}
        data.setdefault("inbox", [])
        data.setdefault("outbox", [])
        data.setdefault("last_seen", 0)
        return data

    def _save_store(self, data):
        atomic_write_json(self._store_path, data)

    def inbox(self):
        return list(self._load_store().get("inbox") or [])

    def outbox(self):
        return list(self._load_store().get("outbox") or [])

    def history(self):
        data = self._load_store()
        return list(data.get("inbox") or []), list(data.get("outbox") or [])

    def add_inbox_message(self, message):
        data = self._load_store()
        normalized = self._normalize_message(message, direction="inbox")
        if not normalized:
            return False
        if self._has_message(data.get("inbox") or [], normalized["id"]):
            return False
        data["inbox"].insert(0, normalized)
        data["inbox"] = data["inbox"][:200]
        self._save_store(data)
        return True

    def _add_outbox_message(self, message):
        data = self._load_store()
        normalized = self._normalize_message(message, direction="outbox")
        if not normalized:
            return False
        if self._has_message(data.get("outbox") or [], normalized["id"]):
            return False
        data["outbox"].insert(0, normalized)
        data["outbox"] = data["outbox"][:200]
        self._save_store(data)
        return True

    @staticmethod
    def _has_message(messages, message_id):
        wanted = str(message_id or "")
        return bool(wanted) and any(str(item.get("id") or "") == wanted for item in messages or [])

    def _normalize_message(self, message, direction):
        raw = dict(message or {})
        item = normalize_share_item(raw.get("item") or raw)
        if not item.get("title") and not item.get("url"):
            return None
        return {
            "id": str(raw.get("id") or uuid.uuid4()),
            "created_at": str(raw.get("created_at") or _now_iso()),
            "received_at": str(raw.get("received_at") or _now_iso()),
            "direction": direction,
            "sender_id": str(raw.get("sender_id") or ""),
            "sender_name": _clean_text(raw.get("sender_name") or "Shared", 80),
            "item": item,
        }

    def build_message(self, item):
        settings = self.settings()
        normalized = normalize_share_item(item)
        if not normalized.get("title"):
            raise MusicSharingError("Enter a title before sharing.")
        if not normalized.get("url"):
            raise MusicSharingError("Enter a store URL so the recipient can use Buy.")
        return {
            "id": str(uuid.uuid4()),
            "created_at": _now_iso(),
            "sender_id": settings.sender_id,
            "sender_name": settings.display_name,
            "item": normalized,
        }

    def send(self, item, timeout=12.0):
        settings = self.settings()
        relay_url = settings.relay_url.rstrip("/")
        if not relay_url:
            raise MusicSharingError("Enter a relay URL before sharing.")
        if not settings.pair_code:
            raise MusicSharingError("Enter a pair code before sharing.")
        message = self.build_message(item)
        payload = json.dumps({"pair_code": settings.pair_code, "message": message}).encode("utf-8")
        req = request.Request(
            f"{relay_url}/v1/share",
            data=payload,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        self._open_json(req, timeout=timeout)
        self._add_outbox_message(message)
        return message

    def fetch(self, timeout=12.0):
        settings = self.settings()
        relay_url = settings.relay_url.rstrip("/")
        if not relay_url:
            raise MusicSharingError("Enter a relay URL before refreshing.")
        if not settings.pair_code:
            raise MusicSharingError("Enter a pair code before refreshing.")
        query = parse.urlencode({"pair_code": settings.pair_code})
        req = request.Request(f"{relay_url}/v1/inbox?{query}", method="GET")
        data = self._open_json(req, timeout=timeout)
        messages = data.get("messages") if isinstance(data, dict) else []
        if not isinstance(messages, list):
            messages = []
        added = 0
        for message in messages:
            if not isinstance(message, dict):
                continue
            if str(message.get("sender_id") or "") == settings.sender_id:
                continue
            if self.add_inbox_message(message):
                added += 1
        return added

    @staticmethod
    def _open_json(req, timeout):
        try:
            with request.urlopen(req, timeout=timeout) as response:
                raw = response.read()
        except OSError as exc:
            raise MusicSharingError(f"Relay request failed: {exc}") from exc
        if not raw:
            return {}
        try:
            return json.loads(raw.decode("utf-8"))
        except json.JSONDecodeError as exc:
            raise MusicSharingError("Relay returned invalid JSON.") from exc
