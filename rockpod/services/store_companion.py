"""Authenticated HTTP companion for the legacy RockPod Store iOS app."""

from __future__ import annotations

import asyncio
import hashlib
import hmac
import json
import os
import secrets
import time
import uuid
from pathlib import Path

import aiohttp
from aiohttp import web

from services.streamrip_import import (
    StreamripImportError,
    StreamripImporter,
    discover_imported_audio_files,
    ensure_rockbox_cover_files,
    find_existing_streamrip_source_files,
    find_existing_library_item_files,
    is_supported_streamrip_url,
)
from services.audio_transcode import AudioSyncTranscoder
from services.livetv_touch import LiveTvTouchCatalog, LiveTvTouchJobs
from services.metadata_reader import read_metadata


API_VERSION = 1
HOME_TABS = ("featured", "new_releases", "top_albums", "just_added", "alternative", "rock", "hip_hop")
HOME_FALLBACK_QUERIES = {
    "featured": "popular albums",
    "new_releases": "new releases",
    "top_albums": "top albums",
    "just_added": "new music",
    "alternative": "alternative",
    "rock": "rock",
    "hip_hop": "hip hop rap",
}


def _now():
    return int(time.time())


def _norm(value):
    return " ".join(str(value or "").casefold().split())


def _ok(data=None, status=200):
    return web.json_response({"ok": True, "data": data or {}, "error": None}, status=status)


def _error(code, message, status=400):
    return web.json_response({"ok": False, "data": None, "error": {"code": code, "message": message}}, status=status)


def _audio_file_metadata(path, index, file_count):
    """Return JSON-safe source tags, with deterministic ordering fallbacks."""
    try:
        track = read_metadata(path)
    except Exception:
        track = None

    def text_value(name):
        return str(getattr(track, name, "") or "")[:500] if track else ""

    def int_value(name, fallback=0):
        try:
            value = int(getattr(track, name, 0) or 0) if track else 0
        except (TypeError, ValueError):
            value = 0
        return value or fallback

    return {
        "title": text_value("title"),
        "artist": text_value("artist"),
        "album": text_value("album"),
        "album_artist": text_value("album_artist"),
        "genre": text_value("genre"),
        "composer": text_value("composer"),
        "comment": text_value("comment"),
        "year": int_value("year"),
        "track_number": int_value("track_number", index + 1),
        "track_count": int_value("track_total", file_count),
        "disc_number": int_value("disc_number", 1),
        "disc_count": int_value("disc_total", 1),
        "compilation": int_value("compilation"),
    }


class CompanionState:
    """Small durable state store containing only token hashes and job metadata."""

    def __init__(self, directory):
        self.directory = Path(directory)
        self.path = self.directory / "state.json"
        self.data = {"tokens": [], "jobs": [], "device_libraries": {}}
        self.load()

    def load(self):
        try:
            loaded = json.loads(self.path.read_text())
            if isinstance(loaded, dict):
                self.data.update(loaded)
        except (OSError, ValueError):
            pass

    def save(self):
        self.directory.mkdir(parents=True, exist_ok=True)
        temporary = self.path.with_suffix(".tmp")
        temporary.write_text(json.dumps(self.data, indent=2, sort_keys=True))
        os.replace(str(temporary), str(self.path))

    @staticmethod
    def token_hash(token):
        return hashlib.sha256(str(token).encode("utf-8")).hexdigest()

    def issue_token(self, client_name):
        token = secrets.token_urlsafe(32)
        self.data["tokens"].append({
            "hash": self.token_hash(token),
            "client": str(client_name or "iPod touch")[:80],
            "created_at": _now(),
        })
        self.save()
        return token

    def accepts(self, token):
        candidate = self.token_hash(token)
        return any(hmac.compare_digest(candidate, item.get("hash", "")) for item in self.data["tokens"])


class Catalog:
    def __init__(self, config, state):
        self.config = config
        self.state = state
        self.importer = StreamripImporter(config)
        self.artwork = {}
        self.remote_artwork = {}
        self.remote_artwork_cache = {}
        self.home_cache = {}

    async def _request(self, request):
        command = list(request.command)
        if "--cover-dir" in command:
            command[command.index("--cover-dir") + 1] = ""
        process = await asyncio.create_subprocess_exec(
            *command,
            env=request.env,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        try:
            stdout, stderr = await process.communicate()
        except asyncio.CancelledError:
            process.kill()
            await process.wait()
            raise
        if process.returncode:
            message = stderr.decode("utf-8", "replace").strip() or stdout.decode("utf-8", "replace").strip()
            raise StreamripImportError(message or "The store provider request failed.")
        try:
            payload = json.loads(Path(request.output_path).read_text())
        except (OSError, ValueError) as exc:
            raise StreamripImportError("The store returned invalid data: %s" % exc)
        return payload

    async def home(self, tab, limit, library_id=""):
        if tab not in HOME_TABS:
            tab = "featured"
        cache_key = (tab, limit)
        cached = self.home_cache.get(cache_key)
        if cached and time.time() - cached[0] < 600:
            return self.decorate(cached[1], library_id)
        candidates = sorted(Path(self.importer.search_dir).glob("homepage-%s-*.json" % tab), reverse=True)
        if not candidates and tab != "featured":
            candidates = sorted(Path(self.importer.search_dir).glob("homepage-featured-*.json"), reverse=True)
        for path in candidates:
            try:
                payload = json.loads(path.read_text())
            except (OSError, ValueError):
                continue
            if isinstance(payload, list) and payload:
                result = payload[:limit]
                self.home_cache[cache_key] = (time.time(), result)
                return self.decorate(result, library_id)
        result = await self._request(
            self.importer.prepare_album_search(HOME_FALLBACK_QUERIES[tab], source="tidal", limit=limit)
        )
        for item in result:
            if not item.get("section"):
                item["section"] = tab.replace("_", " ").title()
        self.home_cache[cache_key] = (time.time(), result)
        return self.decorate(result, library_id)

    async def search(self, query, source, limit, library_id=""):
        result = await self._request(self.importer.prepare_album_search(query, source=source, limit=limit))
        return self.decorate(result, library_id)

    async def detail(self, source, album_id, library_id=""):
        result = await self._request(self.importer.prepare_album_detail(source, album_id))
        return self.decorate(result, library_id)

    def _owned_sets(self, library_id):
        album_titles, tracks = {}, set()
        library = self.state.data.get("device_libraries", {}).get(str(library_id or ""), {})
        for item in library.get("tracks", []):
            artist = item.get("artist") or item.get("album_artist") or ""
            album_artist = item.get("album_artist") or artist
            album_key = (_norm(item.get("album")), _norm(album_artist))
            if album_key[0]:
                album_titles.setdefault(album_key, set()).add(_norm(item.get("title")))
            tracks.add((_norm(item.get("title")), _norm(artist)))
        return album_titles, tracks

    def decorate(self, payload, library_id=""):
        album_titles, tracks = self._owned_sets(library_id)

        def one(item):
            result = dict(item)
            cover_path = str(result.pop("cover_path", "") or "")
            if cover_path and os.path.isfile(cover_path):
                artwork_id = hashlib.sha256(os.path.realpath(cover_path).encode("utf-8")).hexdigest()[:24]
                self.artwork[artwork_id] = os.path.realpath(cover_path)
                result["artwork_url"] = "/v1/store/artwork/%s" % artwork_id
            elif str(result.get("cover_url") or "").startswith(("http://", "https://")):
                remote_url = str(result["cover_url"])
                artwork_id = hashlib.sha256(remote_url.encode("utf-8")).hexdigest()[:24]
                self.remote_artwork[artwork_id] = remote_url
                result["artwork_url"] = "/v1/store/remote-artwork/%s" % artwork_id
            if result.get("media_type") == "track":
                result["owned"] = (_norm(result.get("title")), _norm(result.get("artist"))) in tracks
            else:
                expected = int(result.get("tracks") or 0)
                present = len(album_titles.get(
                    (_norm(result.get("title")), _norm(result.get("artist"))), set()
                ))
                result["owned"] = expected > 0 and present >= expected
            if isinstance(result.get("track_items"), list):
                result["track_items"] = [one(track) for track in result["track_items"]]
            return result

        if isinstance(payload, list):
            return [one(item) for item in payload]
        return one(payload)


class ImportQueue:
    def __init__(self, config, state):
        self.config = config
        self.state = state
        self.importer = StreamripImporter(config)
        self.pending = asyncio.Queue()
        self.worker = None

    def start(self):
        queued = []
        for job in self.state.data["jobs"]:
            if job.get("state") in {"checking", "importing", "converting", "scanning"}:
                job["state"] = "failed"
                job["error"] = "The RockPod host stopped before this import completed."
            elif job.get("state") == "queued":
                queued.append(job)
        queued.sort(key=lambda job: 0 if find_existing_library_item_files(
            self.importer.music_dir, job.get("media_type"), job.get("title"), job.get("artist")
        ) else 1)
        for job in queued:
            self.pending.put_nowait(job["id"])
        self.state.save()
        self.worker = asyncio.ensure_future(self._run())

    async def stop(self):
        if self.worker:
            self.worker.cancel()
            try:
                await self.worker
            except asyncio.CancelledError:
                pass

    def create(self, item, output_format, idempotency_key="", library_id=""):
        url = str(item.get("url") or "").strip()
        if not is_supported_streamrip_url(url):
            raise StreamripImportError("This item does not contain a supported store URL.")
        for old in self.state.data["jobs"]:
            if (idempotency_key and old.get("idempotency_key") == idempotency_key
                    and old.get("library_id", "") == library_id
                    and old.get("state") not in {"failed", "device_failed"}):
                return old, False
        track_items = item.get("track_items")
        expected_files = item.get("tracks") or item.get("track_count")
        if not expected_files and isinstance(track_items, list):
            expected_files = len(track_items)
        if not expected_files and str(item.get("media_type") or "album") == "track":
            expected_files = 1
        try:
            expected_files = max(0, min(10000, int(expected_files or 0)))
        except (TypeError, ValueError):
            expected_files = 0
        job = {
            "id": uuid.uuid4().hex,
            "idempotency_key": str(idempotency_key or "")[:120],
            "library_id": str(library_id or "")[:80],
            "state": "queued",
            "title": str(item.get("title") or "Untitled"),
            "artist": str(item.get("artist") or ""),
            "media_type": str(item.get("media_type") or "album"),
            "source": str(item.get("source") or "tidal"),
            "source_id": str(item.get("id") or ""),
            "url": url,
            "format": str(output_format or "mp3").lower(),
            "created_at": _now(),
            "updated_at": _now(),
            "files": [],
            "error": "",
            "progress": 0.0,
            "progress_done": 0,
            "progress_total": expected_files,
            "progress_label": "Waiting",
        }
        self.state.data["jobs"].insert(0, job)
        self.state.save()
        self.pending.put_nowait(job["id"])
        return job, True

    def get(self, job_id):
        return next((job for job in self.state.data["jobs"] if job.get("id") == job_id), None)

    def update(self, job, state, **values):
        job.update(values)
        job["state"] = state
        job["updated_at"] = _now()
        self.state.save()

    def update_progress(self, job, done, total=0, label=""):
        try:
            done = max(0, int(done or 0))
            total = max(0, int(total or 0))
        except (TypeError, ValueError):
            done, total = 0, 0
        progress = min(1.0, float(done) / total) if total else 0.0
        job.update({
            "progress": progress,
            "progress_done": done,
            "progress_total": total,
            "progress_label": str(label or "")[:120],
            "updated_at": _now(),
        })
        self.state.save()

    def _prepare_existing_files(self, job, source_files):
        output_format = str(job.get("format") or "mp3").lower()
        direct_extensions = {"mp3": {".mp3"}, "aac": {".aac", ".m4a"}, "alac": {".m4a"}}
        allowed = direct_extensions.get(output_format, {"." + output_format})
        if all(Path(path).suffix.lower() in allowed for path in source_files):
            self.update_progress(job, len(source_files), len(source_files), "Ready to transfer")
            return list(source_files)
        if output_format not in {"mp3", "aac"}:
            raise StreamripImportError(
                "The album is already in RockPod, but this iPod transfer requires MP3 or AAC conversion."
            )

        cache_root = os.path.join(
            os.path.abspath(os.path.expanduser(self.config.cache_dir)),
            "store-companion", "device-transcodes",
        )
        transcoder = AudioSyncTranscoder(
            cache_root,
            ffmpeg_path=self.config.get("ffmpeg_binary", ""),
        )
        prepared = []
        settings = {
            "enabled": True,
            "mode": "always",
            "target_codec": output_format,
            "target_bitrate_kbps": 320,
        }
        total = len(source_files)
        for index, path in enumerate(source_files):
            row, _result = transcoder.prepare_track_for_sync(
                {"file_path": path, "codec": Path(path).suffix.lstrip(".")},
                "rockpod-store-" + str(job.get("id") or "device"),
                settings,
            )
            prepared.append(row["sync_source_path"])
            self.update_progress(
                job, index + 1, total,
                "Converted %d of %d" % (index + 1, total),
            )
        return prepared

    async def _wait_for_import_process(self, process, request, job, stall_seconds=180):
        last_progress = request.started_at
        try:
            while True:
                try:
                    return await asyncio.wait_for(process.wait(), timeout=15)
                except asyncio.TimeoutError:
                    progress_times = []
                    discovered = discover_imported_audio_files(request.output_dir, request.started_at)
                    try:
                        progress_times.append(os.path.getmtime(request.log_path))
                    except OSError:
                        pass
                    for path in discovered:
                        try:
                            progress_times.append(os.path.getmtime(path))
                        except OSError:
                            pass
                    if progress_times:
                        last_progress = max(last_progress, max(progress_times))
                    expected = int(job.get("progress_total") or 0)
                    count = len(discovered)
                    label = "Downloaded %d of %d" % (count, expected) if expected else "Downloading…"
                    self.update_progress(job, count, expected, label)
                    if time.time() - last_progress < stall_seconds:
                        continue
                    process.terminate()
                    try:
                        await asyncio.wait_for(process.wait(), timeout=5)
                    except asyncio.TimeoutError:
                        process.kill()
                        await process.wait()
                    raise StreamripImportError(
                        "The provider download made no progress for three minutes. "
                        "RockPod kept the job retryable instead of leaving it stuck."
                    )
        except asyncio.CancelledError:
            if process.returncode is None:
                process.terminate()
                try:
                    await asyncio.wait_for(process.wait(), timeout=5)
                except asyncio.TimeoutError:
                    process.kill()
                    await process.wait()
            raise

    async def _run(self):
        while True:
            job_id = await self.pending.get()
            job = self.get(job_id)
            if not job:
                continue
            try:
                self.update(job, "checking", progress=0.0, progress_done=0,
                            progress_label="Checking library")
                source_files = find_existing_streamrip_source_files(
                    self.importer.music_dir,
                    job.get("source"),
                    job.get("media_type"),
                    job.get("source_id"),
                )
                if not source_files:
                    source_files = find_existing_library_item_files(
                        self.importer.music_dir,
                        job.get("media_type"),
                        job.get("title"),
                        job.get("artist"),
                    )
                if source_files:
                    self.update(job, "converting", progress=0.0, progress_done=0,
                                progress_total=len(source_files), progress_label="Preparing songs")
                    files = await asyncio.to_thread(self._prepare_existing_files, job, source_files)
                    metadata = [_audio_file_metadata(path, index, len(files)) for index, path in enumerate(files)]
                    self.update(job, "ready", files=files, file_metadata=metadata, error="",
                                progress=1.0, progress_done=len(files), progress_total=len(files),
                                progress_label="Ready to transfer")
                    continue
                request = self.importer.prepare_import(job["url"], job["format"])
                self.update(job, "importing", progress=0.0, progress_done=0,
                            progress_label="Downloading…")
                with open(request.log_path, "ab") as log:
                    process = await asyncio.create_subprocess_exec(
                        *request.command, env=request.env, stdout=log, stderr=asyncio.subprocess.STDOUT
                    )
                    returncode = await self._wait_for_import_process(process, request, job)
                if returncode:
                    raise StreamripImportError("streamrip exited with status %d; see %s" % (returncode, request.log_path))
                files = discover_imported_audio_files(request.output_dir, request.started_at)
                ensure_rockbox_cover_files(files, request.output_dir)
                metadata = [_audio_file_metadata(path, index, len(files)) for index, path in enumerate(files)]
                self.update(job, "ready", files=files, file_metadata=metadata, error="",
                            progress=1.0, progress_done=len(files), progress_total=len(files),
                            progress_label="Ready to transfer")
            except Exception as exc:
                self.update(job, "failed", error=str(exc), progress_label="Failed")
            finally:
                self.pending.task_done()


class StoreCompanion:
    def __init__(self, config, pair_code=None):
        self.config = config
        directory = os.path.join(os.path.abspath(os.path.expanduser(config.cache_dir)), "store-companion")
        self.state = CompanionState(directory)
        self.catalog = Catalog(config, self.state)
        self.imports = ImportQueue(config, self.state)
        self.livetv = LiveTvTouchCatalog(config)
        self.livetv_jobs = LiveTvTouchJobs(self.livetv)
        self.pair_code = str(pair_code or "%06d" % secrets.randbelow(1000000))
        self.pair_expires = _now() + 600

    @web.middleware
    async def auth(self, request, handler):
        if request.path in {"/v1/status", "/v1/pair"} or request.path.startswith("/v1/store/artwork/") or request.path.startswith("/v1/store/remote-artwork/"):
            return await handler(request)
        header = request.headers.get("Authorization", "")
        token = header[7:] if header.startswith("Bearer ") else ""
        if not token or not self.state.accepts(token):
            return _error("unauthorized", "Pair this iPod with the RockPod host first.", 401)
        return await handler(request)

    def app(self):
        app = web.Application(middlewares=[self.auth], client_max_size=4 * 1024 * 1024)
        app.router.add_get("/v1/status", self.status)
        app.router.add_post("/v1/pair", self.pair)
        app.router.add_get("/v1/account", self.account)
        app.router.add_get("/v1/store/home", self.home)
        app.router.add_get("/v1/store/search", self.search)
        app.router.add_get("/v1/store/albums/{source}/{album_id}", self.detail)
        app.router.add_get("/v1/store/artwork/{artwork_id}", self.artwork)
        app.router.add_get("/v1/store/remote-artwork/{artwork_id}", self.remote_artwork)
        app.router.add_get("/v1/imports", self.import_list)
        app.router.add_post("/v1/imports", self.import_create)
        app.router.add_post("/v1/imports/clear", self.import_clear)
        app.router.add_get("/v1/imports/{job_id}", self.import_detail)
        app.router.add_delete("/v1/imports/{job_id}", self.import_delete)
        app.router.add_get("/v1/imports/{job_id}/files/{file_index}", self.import_file)
        app.router.add_post("/v1/imports/{job_id}/state", self.import_state)
        app.router.add_post("/v1/device/library", self.device_library)
        app.router.add_post("/v1/livetv/syncs", self.livetv_sync_create)
        app.router.add_get("/v1/livetv/syncs/{job_id}", self.livetv_sync_detail)
        app.router.add_get("/v1/livetv/manifest", self.livetv_manifest)
        app.router.add_get("/v1/livetv/media/{media_id}", self.livetv_media)
        app.on_startup.append(self.on_startup)
        app.on_cleanup.append(self.on_cleanup)
        return app

    async def on_startup(self, _app):
        self.imports.start()
        self.livetv_jobs.start()

    async def on_cleanup(self, _app):
        await self.livetv_jobs.stop()
        await self.imports.stop()

    async def status(self, _request):
        return _ok({
            "api_version": API_VERSION,
            "name": "RockPod Companion",
            "pairing": _now() < self.pair_expires,
            "features": ["store", "livetv-touch"],
        })

    async def pair(self, request):
        try:
            body = await request.json()
        except Exception:
            return _error("bad_json", "Send a JSON pairing request.")
        if _now() >= self.pair_expires:
            return _error("pair_expired", "Restart the host to generate a new pairing code.", 410)
        if not hmac.compare_digest(str(body.get("code") or ""), self.pair_code):
            return _error("bad_pair_code", "That pairing code is incorrect.", 403)
        return _ok({"token": self.state.issue_token(body.get("name")), "api_version": API_VERSION})

    async def account(self, _request):
        config_path = self.catalog.importer.config_path
        return _ok({"source": "tidal", "configured": os.path.isfile(config_path), "config_path": config_path})

    async def _catalog_call(self, awaitable):
        try:
            return _ok(await awaitable)
        except StreamripImportError as exc:
            return _error("provider_error", str(exc), 502)
        except Exception as exc:
            return _error("host_error", str(exc), 500)

    async def home(self, request):
        return await self._catalog_call(self.catalog.home(
            request.query.get("tab", "featured"), _limit(request), _library_id(request)
        ))

    async def search(self, request):
        query = request.query.get("q", "").strip()
        if not query:
            return _error("missing_query", "Enter an artist, album, or song.")
        return await self._catalog_call(self.catalog.search(
            query, request.query.get("source", "tidal"), _limit(request), _library_id(request)
        ))

    async def detail(self, request):
        return await self._catalog_call(self.catalog.detail(
            request.match_info["source"], request.match_info["album_id"], _library_id(request)
        ))

    async def artwork(self, request):
        path = self.catalog.artwork.get(request.match_info["artwork_id"])
        if not path or not os.path.isfile(path):
            return _error("not_found", "Artwork is no longer available.", 404)
        return web.FileResponse(path)

    async def remote_artwork(self, request):
        artwork_id = request.match_info["artwork_id"]
        cached = self.catalog.remote_artwork_cache.get(artwork_id)
        if cached:
            return web.Response(body=cached[0], content_type=cached[1])
        url = self.catalog.remote_artwork.get(artwork_id)
        if not url:
            return _error("not_found", "Artwork is no longer available.", 404)
        try:
            timeout = aiohttp.ClientTimeout(total=15)
            async with aiohttp.ClientSession(timeout=timeout) as session:
                async with session.get(url) as response:
                    if response.status != 200:
                        return _error("artwork_failed", "Artwork provider returned an error.", 502)
                    body = await response.read()
                    content_type = response.headers.get("Content-Type", "image/jpeg").split(";", 1)[0]
            self.catalog.remote_artwork_cache[artwork_id] = (body, content_type)
            return web.Response(body=body, content_type=content_type)
        except Exception as exc:
            return _error("artwork_failed", str(exc), 502)

    def _public_job(self, job):
        result = dict(job)
        file_count = len(job.get("files", []))
        metadata = job.get("file_metadata", [])
        if not isinstance(metadata, list) or len(metadata) != file_count:
            metadata = [
                _audio_file_metadata(path, index, file_count)
                for index, path in enumerate(job.get("files", []))
            ]
        result.pop("file_metadata", None)
        result["files"] = [
            dict(metadata[index], **{
                "name": os.path.basename(path),
                "bytes": os.path.getsize(path) if os.path.isfile(path) else 0,
                "download_url": "/v1/imports/%s/files/%d" % (job["id"], index),
            })
            for index, path in enumerate(job.get("files", []))
        ]
        return result

    async def import_list(self, request):
        library_id = _library_id(request)
        jobs = [job for job in self.state.data["jobs"] if job.get("library_id", "") == library_id]
        return _ok([self._public_job(job) for job in jobs[:100]])

    async def import_create(self, request):
        try:
            body = await request.json()
            job, created = self.imports.create(
                body.get("item") or {}, body.get("format") or "mp3",
                request.headers.get("Idempotency-Key", ""), _library_id(request)
            )
            return _ok(self._public_job(job), 202 if created else 200)
        except StreamripImportError as exc:
            return _error("invalid_import", str(exc))
        except Exception as exc:
            return _error("bad_request", str(exc))

    async def import_detail(self, request):
        job = self.imports.get(request.match_info["job_id"])
        if not job or job.get("library_id", "") != _library_id(request):
            return _error("not_found", "Import job not found.", 404)
        return _ok(self._public_job(job))

    async def import_delete(self, request):
        job = self.imports.get(request.match_info["job_id"])
        if not job or job.get("library_id", "") != _library_id(request):
            return _error("not_found", "Import job not found.", 404)
        if job.get("state") not in {"completed", "failed", "device_failed"}:
            return _error("job_active", "An active download cannot be removed.", 409)
        self.state.data["jobs"].remove(job)
        self.state.save()
        return _ok({"removed": 1})

    async def import_clear(self, request):
        library_id = _library_id(request)
        terminal = {"completed", "failed", "device_failed"}
        old_jobs = self.state.data["jobs"]
        kept = [job for job in old_jobs if not (
            job.get("library_id", "") == library_id and job.get("state") in terminal
        )]
        removed = len(old_jobs) - len(kept)
        self.state.data["jobs"] = kept
        self.state.save()
        return _ok({"removed": removed})

    async def import_file(self, request):
        job = self.imports.get(request.match_info["job_id"])
        if not job or job.get("library_id", "") != _library_id(request):
            return _error("not_found", "Import job not found.", 404)
        try:
            index = int(request.match_info["file_index"])
            path = job.get("files", [])[index]
        except (ValueError, IndexError):
            return _error("not_found", "Import file not found.", 404)
        if job.get("state") not in {"ready", "transferring", "completed"} or not os.path.isfile(path):
            return _error("not_ready", "This song is not ready for the iPod touch.", 409)
        return web.FileResponse(path, headers={"Content-Disposition": "attachment; filename=%s" % os.path.basename(path)})

    async def import_state(self, request):
        job = self.imports.get(request.match_info["job_id"])
        if not job or job.get("library_id", "") != _library_id(request):
            return _error("not_found", "Import job not found.", 404)
        try:
            body = await request.json()
        except Exception:
            return _error("bad_json", "Send a JSON state update.")
        state = str(body.get("state") or "")
        if state not in {"transferring", "completed", "device_failed"}:
            return _error("bad_state", "Unsupported device import state.")
        values = {"error": str(body.get("error") or "")[:500]}
        if state == "completed":
            values.update(progress=1.0, progress_label="Added to Music")
        elif state == "device_failed":
            values["progress_label"] = "Transfer failed"
        else:
            for key in ("progress", "progress_done", "progress_total"):
                if key not in body:
                    continue
                try:
                    value = float(body[key]) if key == "progress" else max(0, int(body[key]))
                except (TypeError, ValueError):
                    continue
                values[key] = min(1.0, max(0.0, value)) if key == "progress" else value
            if "progress_label" in body:
                values["progress_label"] = str(body.get("progress_label") or "")[:120]
        self.imports.update(job, state, **values)
        return _ok(self._public_job(job))

    async def device_library(self, request):
        try:
            body = await request.json()
        except Exception:
            return _error("bad_json", "Send the iPod touch Music library as JSON.")
        tracks = body.get("tracks") or []
        if not isinstance(tracks, list) or len(tracks) > 50000:
            return _error("bad_library", "The Music library inventory is invalid.")
        clean = []
        for item in tracks:
            if not isinstance(item, dict):
                continue
            clean.append({key: str(item.get(key) or "")[:500] for key in (
                "persistent_id", "title", "artist", "album", "album_artist",
                "track_number", "track_count", "disc_number", "disc_count",
            )})
        library_id = _library_id(request)
        self.state.data.setdefault("device_libraries", {})[library_id] = {
            "updated_at": _now(), "tracks": clean,
        }
        album_tracks = {}
        track_keys = set()
        for item in clean:
            artist = item.get("artist") or item.get("album_artist") or ""
            album_artist = item.get("album_artist") or artist
            album_key = (_norm(item.get("album")), _norm(album_artist))
            if album_key[0]:
                title_key = _norm(item.get("title"))
                album_tracks.setdefault(album_key, {}).setdefault(title_key, []).append(item)
            track_keys.add((_norm(item.get("title")), _norm(artist)))
        for job in self.state.data.get("jobs", []):
            if job.get("library_id", "") != library_id or job.get("state") not in {
                    "ready", "transferring", "device_failed"}:
                continue
            files = job.get("files", [])
            if not files:
                continue
            if job.get("media_type") == "track":
                imported = (_norm(job.get("title")), _norm(job.get("artist"))) in track_keys
            else:
                metadata = job.get("file_metadata", [])
                if not isinstance(metadata, list) or len(metadata) != len(files):
                    metadata = [
                        _audio_file_metadata(path, index, len(files))
                        for index, path in enumerate(files)
                    ]
                present_tracks = album_tracks.get(
                    (_norm(job.get("title")), _norm(job.get("artist"))), {}
                )
                imported = len(metadata) == len(files)
                for index, expected in enumerate(metadata):
                    title = _norm(expected.get("title"))
                    matches = present_tracks.get(title, [])
                    required = {
                        "track_number": int(expected.get("track_number") or index + 1),
                        "track_count": int(expected.get("track_count") or len(files)),
                        "disc_number": int(expected.get("disc_number") or 1),
                        "disc_count": int(expected.get("disc_count") or 1),
                    }
                    if not title or len(matches) != 1 or any(
                            int(matches[0].get(key) or 0) != value
                            for key, value in required.items()):
                        imported = False
                        break
            if imported:
                job.update(state="completed", error="", updated_at=_now())
        self.state.save()
        return _ok({"tracks": len(clean), "updated_at": _now()})

    async def livetv_sync_create(self, request):
        try:
            body = await request.json()
        except Exception:
            body = {}
        scope = str(body.get("scope") or "forecast").lower()
        if scope not in {"forecast", "featured", "all"}:
            return _error(
                "bad_scope", "Live TV scope must be forecast, featured or all."
            )
        job, created = self.livetv_jobs.create(scope)
        return _ok(self.livetv_jobs.public(job), 202 if created else 200)

    async def livetv_sync_detail(self, request):
        job = self.livetv_jobs.get(request.match_info["job_id"])
        if not job:
            return _error("not_found", "Live TV sync job not found.", 404)
        return _ok(self.livetv_jobs.public(job))

    async def livetv_manifest(self, request):
        scope = request.query.get("scope", "forecast")
        manifest = self.livetv.latest_manifest(scope)
        if not manifest:
            return _error(
                "not_ready", "Run a Live TV sync before requesting its manifest.", 404
            )
        return _ok(manifest)

    async def livetv_media(self, request):
        path = self.livetv.media_path(request.match_info["media_id"])
        if not path:
            return _error("not_found", "Live TV media not found.", 404)
        return web.FileResponse(
            path,
            headers={"Content-Disposition": "attachment; filename=channel.mp4"},
        )


def _limit(request):
    try:
        return max(1, min(50, int(request.query.get("limit", "24"))))
    except ValueError:
        return 24


def _library_id(request):
    value = str(request.headers.get("X-RockPod-Library-ID") or "default")
    return "".join(character for character in value[:80] if character.isalnum() or character in "-_.") or "default"


def create_app(config, pair_code=None):
    return StoreCompanion(config, pair_code=pair_code).app()
