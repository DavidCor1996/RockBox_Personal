"""Prepare RockPod Live TV for the iPod touch 4G companion client.

The Classic Live TV service remains authoritative for scanning, line-up and
schedule generation.  This adapter only creates an iOS-compatible media cache
and serializes that existing model without exposing host filesystem paths.
"""

from __future__ import annotations

import asyncio
import hashlib
import json
import os
import re
import subprocess
import time
import uuid
from pathlib import Path


TOUCH_API_VERSION = 1
TOUCH_PROFILE = "touch4-h264-480x360-v1"
TOUCH_VIDEO_SIZE = "480:360"
TOUCH_SCOPES = {"forecast", "featured", "all"}
TOUCH_FEATURED_CALLSIGNS = {"WX", "USA", "YTV"}
_MEDIA_ID = re.compile(r"^[0-9a-f]{32}$")

# Kept here as a machine-readable parity contract with livetv_guide.c.
GUIDE_PALETTE = {
    "banner_top": "#DCEEF9",
    "banner_bottom": "#B7D6E9",
    "strip_text": "#10386B",
    "description": "#026FAF",
    "header": "#122549",
    "channel": "#122549",
    "row": "#094871",
    "grid_line": "#0A2A50",
    "selection": "#FEC425",
    "selection_text": "#10254A",
    "hint": "#0F5689",
    "dim_text": "#5C82A8",
}


def _touch_scope(value) -> str:
    value = str(value or "forecast").lower()
    return value if value in TOUCH_SCOPES else "forecast"


def _sha256_file(path: str) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        while True:
            block = handle.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def _clean_text(value, limit=500) -> str:
    return " ".join(str(value or "").replace("\t", " ").split())[:limit]


class LiveTvTouchError(RuntimeError):
    pass


class LiveTvTouchCatalog:
    """Build immutable manifests using ``services.livetv`` as the source."""

    def __init__(self, config, livetv_module=None, command_runner=None):
        self.config = config
        self.livetv = livetv_module
        self.command_runner = command_runner or subprocess.run
        base = os.path.abspath(os.path.expanduser(config.cache_dir))
        self.cache_dir = os.path.join(base, "store-companion", "livetv-touch", TOUCH_PROFILE)
        self.media_dir = os.path.join(self.cache_dir, "media")
        self.manifest_dir = os.path.join(self.cache_dir, "manifests")
        self._media = {}

    def _ffmpeg(self) -> str:
        configured = str(getattr(self.config, "ffmpeg_binary", "") or "").strip()
        return configured or "ffmpeg"

    def _media_identity(self, relative: str, source: str) -> str:
        stat = os.stat(source)
        identity = "%s\0%s\0%d\0%d" % (
            TOUCH_PROFILE, relative, stat.st_size, stat.st_mtime_ns
        )
        return hashlib.sha256(identity.encode("utf-8")).hexdigest()[:32]

    def _touch_path(self, media_id: str) -> str:
        return os.path.join(self.media_dir, media_id + ".mp4")

    def _transcode(self, source: str, target: str) -> None:
        os.makedirs(os.path.dirname(target), exist_ok=True)
        temporary = target + ".%d-%d.tmp.mp4" % (os.getpid(), time.time_ns())
        command = [
            self._ffmpeg(), "-y", "-loglevel", "error", "-i", source,
            "-map", "0:v:0", "-map", "0:a:0?",
            "-vf", "scale=%s:force_original_aspect_ratio=decrease,"
                   "pad=%s:(ow-iw)/2:(oh-ih)/2:black,format=yuv420p" % (
                       TOUCH_VIDEO_SIZE, TOUCH_VIDEO_SIZE),
            "-c:v", "libx264", "-profile:v", "baseline", "-level", "3.0",
            "-preset", "veryfast", "-b:v", "650k", "-maxrate", "800k",
            "-bufsize", "1600k", "-g", "60", "-keyint_min", "30",
            "-c:a", "aac", "-b:a", "96k", "-ar", "44100", "-ac", "2",
            "-movflags", "+faststart", temporary,
        ]
        try:
            result = self.command_runner(
                command, capture_output=True, text=True, check=False, timeout=7200
            )
        except (OSError, subprocess.SubprocessError) as exc:
            raise LiveTvTouchError("Could not run the touch video encoder: %s" % exc)
        if result.returncode or not os.path.isfile(temporary) or os.path.getsize(temporary) <= 0:
            try:
                os.remove(temporary)
            except OSError:
                pass
            raise LiveTvTouchError(
                "The touch video encoder could not prepare %s."
                % os.path.basename(source)
            )
        os.replace(temporary, target)

    def ensure_touch_media(self, relative: str, classic_path: str) -> dict:
        media_id = self._media_identity(relative, classic_path)
        target = self._touch_path(media_id)
        if not os.path.isfile(target) or os.path.getsize(target) <= 0:
            self._transcode(classic_path, target)
        item = {
            "id": media_id,
            "bytes": os.path.getsize(target),
            "sha256": _sha256_file(target),
            "download_url": "/v1/livetv/media/%s" % media_id,
        }
        self._media[media_id] = target
        return item

    @staticmethod
    def _channel_json(channel) -> dict:
        return {
            "number": int(channel.number),
            "callsign": _clean_text(channel.callsign, 16) or "CH",
            "name": _clean_text(channel.name, 100) or "Channel",
            "category": _clean_text(channel.category, 50) or "Series",
            "favourite": bool(channel.favourite),
            "parental_locked": bool(channel.parental_locked),
        }

    @staticmethod
    def _slot_json(slot, media_id: str) -> dict:
        return {
            "channel": int(slot.channel),
            "day": int(slot.day),
            "start": int(slot.start),
            "duration": int(slot.duration),
            "kind": "A" if str(slot.kind).upper().startswith("A") else "S",
            "title": _clean_text(slot.title) or "Program",
            "rating": _clean_text(slot.rating, 30) or "--",
            "description": _clean_text(slot.description, 1000) or "--",
            "block_start": int(slot.block_start),
            "block_duration": int(slot.block_duration or slot.duration),
            "media_id": media_id,
        }

    def prepare(self, scope="forecast", progress=None) -> dict:
        """Prepare and return a public manifest.

        ``scope=forecast`` contains only Weather. ``scope=featured`` is the
        touch line-up (Weather, USA Network and YTV), while ``scope=all``
        publishes every playable Classic channel.
        """
        scope = _touch_scope(scope)
        module = self.livetv
        if module is None:
            from services import livetv as module  # local RockPod tree
        library = module.LiveTvLibrary(self.config)
        lineup = module.LiveTvLineup(library)
        sync = module.LiveTvSync(library)

        if progress:
            progress(0, 1, "Scanning Live TV")
        shows, ads = library.scan(probe_durations=True)
        if not lineup.channels:
            lineup.autobuild(shows, ads)
            lineup.save()
        shows = module.ensure_weather_channel(sync, lineup, self.config, shows, ads)
        lineup.save()
        shows, ads = sync.reconcile_missing_sources(lineup, shows, ads)
        slots = module.LiveTvScheduler(lineup, shows, ads).build()

        if scope == "forecast":
            channels = [
                channel for channel in lineup.channels
                if channel.category == module.LIVETV_WEATHER_CATEGORY
            ]
        elif scope == "featured":
            channels = [
                channel for channel in lineup.channels
                if str(channel.callsign).upper() in TOUCH_FEATURED_CALLSIGNS
            ]
        else:
            channels = list(lineup.channels)
        channel_numbers = {channel.number for channel in channels}
        slots = [slot for slot in slots if slot.channel in channel_numbers]
        playable_numbers = {slot.channel for slot in slots}
        channels = [channel for channel in channels if channel.number in playable_numbers]
        if not channels or not slots:
            if scope in {"forecast", "featured"}:
                raise LiveTvTouchError(
                    "The requested touch channels are not ready. Check the Weather, "
                    "USA Network and YTV sources in the RockPod Live TV lineup."
                )
            raise LiveTvTouchError("RockPod has no playable Live TV channels.")

        items = list(shows) + list(ads)
        by_relative = {item.device_relative(): item for item in items}
        relative_paths = sorted({slot.path for slot in slots})
        media_by_relative = {}
        media = []
        for index, relative in enumerate(relative_paths, 1):
            item = by_relative.get(relative)
            if item is None:
                raise LiveTvTouchError("A scheduled Live TV file no longer has a source.")
            if progress:
                progress(index - 1, len(relative_paths), "Preparing %s" % item.title)
            classic = sync.ensure_mpeg(item)
            public = self.ensure_touch_media(relative, classic)
            media_by_relative[relative] = public["id"]
            media.append(public)
            if progress:
                progress(index, len(relative_paths), "Prepared %s" % item.title)

        public_slots = [
            self._slot_json(slot, media_by_relative[slot.path]) for slot in slots
        ]
        body = {
            "version": TOUCH_API_VERSION,
            "scope": scope,
            "timezone": time.tzname[0] if time.tzname else "local",
            "palette": dict(GUIDE_PALETTE),
            "channels": [self._channel_json(channel) for channel in channels],
            "slots": public_slots,
            "media": media,
        }
        canonical = json.dumps(body, sort_keys=True, separators=(",", ":"))
        body["revision"] = hashlib.sha256(canonical.encode("utf-8")).hexdigest()
        body["generated_at"] = int(time.time())
        os.makedirs(self.manifest_dir, exist_ok=True)
        path = os.path.join(self.manifest_dir, scope + ".json")
        temporary = path + ".tmp"
        with open(temporary, "w", encoding="utf-8") as handle:
            json.dump(body, handle, indent=2, sort_keys=True)
        os.replace(temporary, path)
        return body

    def latest_manifest(self, scope="forecast"):
        scope = _touch_scope(scope)
        path = os.path.join(self.manifest_dir, scope + ".json")
        try:
            with open(path, "r", encoding="utf-8") as handle:
                return json.load(handle)
        except (OSError, ValueError):
            return None

    def media_path(self, media_id: str):
        media_id = str(media_id or "")
        if not _MEDIA_ID.match(media_id):
            return None
        path = self._media.get(media_id) or self._touch_path(media_id)
        return path if os.path.isfile(path) and os.path.getsize(path) > 0 else None


class LiveTvTouchJobs:
    """One-worker async preparation queue used by the HTTP companion."""

    def __init__(self, catalog: LiveTvTouchCatalog):
        self.catalog = catalog
        self.jobs = []
        self.pending = asyncio.Queue()
        self.worker = None

    def start(self):
        if self.worker is None:
            self.worker = asyncio.ensure_future(self._run())

    async def stop(self):
        if self.worker is not None:
            self.worker.cancel()
            try:
                await self.worker
            except asyncio.CancelledError:
                pass
            self.worker = None

    def create(self, scope="forecast"):
        scope = _touch_scope(scope)
        for job in self.jobs:
            if job["scope"] == scope and job["state"] in {"queued", "preparing"}:
                return job, False
        manifest = self.catalog.latest_manifest(scope)
        if manifest:
            count = len(manifest.get("media", []))
            job = {
                "id": uuid.uuid4().hex,
                "scope": scope,
                "state": "ready",
                "done": count,
                "total": count,
                "label": "Ready",
                "error": "",
                "created_at": int(time.time()),
                "manifest": manifest,
            }
            self.jobs.insert(0, job)
            self.jobs = self.jobs[:20]
            return job, False
        job = {
            "id": uuid.uuid4().hex,
            "scope": scope,
            "state": "queued",
            "done": 0,
            "total": 0,
            "label": "Waiting",
            "error": "",
            "created_at": int(time.time()),
            "manifest": None,
        }
        self.jobs.insert(0, job)
        self.jobs = self.jobs[:20]
        self.pending.put_nowait(job)
        return job, True

    def get(self, job_id):
        return next((job for job in self.jobs if job["id"] == job_id), None)

    @staticmethod
    def public(job):
        return dict(job)

    async def _run(self):
        while True:
            job = await self.pending.get()
            try:
                job.update(state="preparing", label="Scanning Live TV")

                def progress(done, total, label):
                    job.update(done=int(done), total=int(total), label=_clean_text(label, 120))

                manifest = await asyncio.to_thread(
                    self.catalog.prepare, job["scope"], progress
                )
                job.update(
                    state="ready", manifest=manifest, label="Ready",
                    done=len(manifest.get("media", [])),
                    total=len(manifest.get("media", [])),
                )
            except asyncio.CancelledError:
                raise
            except Exception as exc:
                job.update(state="failed", error=_clean_text(exc, 500), label="Failed")
            finally:
                self.pending.task_done()
