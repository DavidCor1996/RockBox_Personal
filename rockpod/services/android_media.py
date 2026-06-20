"""Android phone media import and iPod-compatible video conversion."""

import hashlib
import os
import re
import shutil
import time
from pathlib import Path

from PySide6.QtCore import QObject, QThread, Signal, Slot, Qt

from services.command_runner import CommandRunner
from services.file_safety import atomic_write_text
from services.path_safety import resolve_under_root, validate_device_root


ANDROID_VIDEO_EXTENSIONS = {
    ".mp4", ".m4v", ".mov", ".mkv", ".avi", ".webm", ".3gp",
}
ANDROID_PHOTO_EXTENSIONS = {
    ".jpg", ".jpeg", ".png", ".webp", ".heic", ".heif", ".bmp", ".gif",
}
ANDROID_MEDIA_DIR_NAMES = {
    "camera", "dcim", "download", "downloads", "movies", "photos", "pictures", "video", "videos",
}
ANDROID_SKIP_DIR_NAMES = {
    ".thumbnails", ".trashed", "android", "data", "obb", "cache", "lost.dir",
}

IPOD_VIDEO_FPS = 20
IPOD_VIDEO_SIZE = "320:240"
IPOD_VIDEO_QSCALE = "8"
IPOD_VIDEO_MAXRATE = "900k"
IPOD_VIDEO_BUFSIZE = "512k"
IPOD_AUDIO_BITRATE = "96k"
IPODTIKTOK_DEVICE_DIR = os.path.join("Videos", "iPodTikTok")
IPODTIKTOK_FEED_PATH = os.path.join(".rockbox", "rocks", "apps", ".ipodtiktok_feed.tsv")
IPODTIKTOK_MANIFEST_PATH = os.path.join(".rockbox", "rocks", "apps", ".ipodtiktok_import_manifest.tsv")


def _sanitize_component(value, max_len=64):
    text = str(value or "").strip()
    if not text:
        return "media"
    text = re.sub(r"[<>:\"/\\|?*]+", "_", text)
    text = re.sub(r"\s+", "_", text)
    text = re.sub(r"_+", "_", text).strip("._- ")
    if len(text) > max_len:
        text = text[:max_len].rstrip("._- ")
    return text or "media"


def _sanitize_dir_component(value, max_len=64):
    text = str(value or "").strip()
    if not text:
        return "Videos"
    text = re.sub(r"[<>:\"/\\|?*]+", "_", text)
    text = re.sub(r"\s+", " ", text).strip("._- ")
    if len(text) > max_len:
        text = text[:max_len].rstrip("._- ")
    return text or "Videos"


def _normalize_rel_dir(path):
    parts = []
    for part in Path(str(path or "")).parts:
        if part in ("", ".", os.sep):
            continue
        parts.append(_sanitize_dir_component(part))
    return os.path.join(*parts) if parts else "Videos"


def _media_kind(path):
    ext = Path(path).suffix.lower()
    if ext in ANDROID_VIDEO_EXTENSIONS:
        return "video"
    if ext in ANDROID_PHOTO_EXTENSIONS:
        return "photo"
    return ""


def looks_like_android_root(path):
    """Heuristic for a mounted Android storage root or one of its media folders."""
    if not path or not os.path.isdir(path):
        return False
    lower_name = os.path.basename(os.path.normpath(path)).casefold()
    if lower_name in ANDROID_MEDIA_DIR_NAMES:
        return True
    for name in ("DCIM", "Pictures", "Movies", "Download", "Downloads", "Camera"):
        if os.path.isdir(os.path.join(path, name)):
            return True
    return False


def discover_android_sources(configured_path=""):
    """Return likely mounted Android storage roots."""
    candidates = []
    seen = set()

    def add(path):
        if not path or not looks_like_android_root(path):
            return
        real = os.path.realpath(path)
        if real in seen:
            return
        seen.add(real)
        candidates.append(path)

    add(configured_path)

    user = os.environ.get("USER", "")
    uid = os.getuid() if hasattr(os, "getuid") else None
    search_roots = []
    if uid is not None:
        search_roots.append(os.path.join("/run/user", str(uid), "gvfs"))
    if user:
        search_roots.extend(
            [
                os.path.join("/run/media", user),
                os.path.join("/media", user),
            ]
        )
    search_roots.append("/mnt")

    for root in search_roots:
        if not os.path.isdir(root):
            continue
        add(root)
        try:
            entries = sorted(os.listdir(root))
        except OSError:
            continue
        for entry in entries:
            child = os.path.join(root, entry)
            if not os.path.isdir(child):
                continue
            add(child)
            try:
                grandchildren = sorted(os.listdir(child))
            except OSError:
                continue
            for grandchild in grandchildren:
                grandchild_path = os.path.join(child, grandchild)
                if os.path.isdir(grandchild_path):
                    add(grandchild_path)

    return candidates


def android_scan_roots(source_root):
    """Pick the most relevant subtrees from a mounted Android storage root."""
    if not source_root or not os.path.isdir(source_root):
        return []

    base_name = os.path.basename(os.path.normpath(source_root)).casefold()
    if base_name in ANDROID_MEDIA_DIR_NAMES:
        return [source_root]

    roots = []
    preferred = ("DCIM", "Pictures", "Movies", "Download", "Downloads", "Camera")
    for name in preferred:
        child = os.path.join(source_root, name)
        if os.path.isdir(child):
            roots.append(child)
    if not roots:
        roots.append(source_root)
    return roots


def scan_android_media(source_root, include_photos=True, include_videos=True):
    """List importable media under a mounted Android phone storage root."""
    items = []
    seen = set()
    scan_roots = android_scan_roots(source_root)

    for scan_root in scan_roots:
        for root, dirs, files in os.walk(scan_root, followlinks=True):
            dirs[:] = [
                directory
                for directory in dirs
                if not directory.startswith(".")
                and directory.casefold() not in ANDROID_SKIP_DIR_NAMES
            ]
            for filename in sorted(files):
                if filename.startswith("."):
                    continue
                source_path = os.path.join(root, filename)
                real_path = os.path.realpath(source_path)
                if real_path in seen:
                    continue
                seen.add(real_path)
                kind = _media_kind(source_path)
                if not kind:
                    continue
                if kind == "photo" and not include_photos:
                    continue
                if kind == "video" and not include_videos:
                    continue
                try:
                    stat = os.stat(source_path)
                except OSError:
                    continue
                items.append(
                    {
                        "source_path": source_path,
                        "kind": kind,
                        "size": int(stat.st_size or 0),
                        "mtime": float(stat.st_mtime or 0.0),
                        "relative_path": os.path.relpath(source_path, source_root),
                    }
                )

    items.sort(key=lambda item: (item["mtime"], item["source_path"]), reverse=True)
    return items


def build_import_filename(item):
    """Create a stable device filename from the source path + file identity."""
    source_path = str((item or {}).get("source_path") or "")
    source_name = Path(source_path).stem
    source_size = int((item or {}).get("size") or 0)
    source_mtime = float((item or {}).get("mtime") or 0.0)
    digest = hashlib.md5(
        f"{source_path}|{source_size}|{source_mtime:.6f}".encode("utf-8", "ignore")
    ).hexdigest()[:10]
    if source_mtime > 0:
        stamp = time.strftime("%Y%m%d_%H%M%S", time.localtime(source_mtime))
    else:
        stamp = "unknown_time"
    kind_prefix = "IMG" if (item or {}).get("kind") == "photo" else "VID"
    stem = _sanitize_component(source_name, max_len=40)
    return f"{kind_prefix}_{stamp}_{stem}_{digest}.mpg"


def build_device_output_relpath(item, device_subdir):
    subdir = _normalize_rel_dir(device_subdir or "Videos/Android Phone")
    return os.path.join(subdir, build_import_filename(item))


def _source_signature(item):
    rel_path = str((item or {}).get("relative_path") or (item or {}).get("source_path") or "")
    kind = str((item or {}).get("kind") or "")
    size = int((item or {}).get("size") or 0)
    mtime = float((item or {}).get("mtime") or 0.0)
    return hashlib.md5(
        f"{kind}|{rel_path}|{size}|{mtime:.6f}".encode("utf-8", "ignore")
    ).hexdigest()


def _tiktok_clip_title(item):
    source_name = Path(str((item or {}).get("source_path") or "")).stem
    title = re.sub(r"[_-]+", " ", source_name).strip()
    title = re.sub(r"\s+", " ", title)
    return title or "Clip"


def _parse_tiktok_index(rel_path):
    name = Path(str(rel_path or "")).stem
    match = re.fullmatch(r"ipodtiktok(\d+)", name, flags=re.IGNORECASE)
    return int(match.group(1)) if match else 0


def _load_tiktok_manifest(device_mount):
    manifest_path = resolve_under_root(device_mount, IPODTIKTOK_MANIFEST_PATH)
    rows = {}
    if not os.path.isfile(manifest_path):
        return rows
    try:
        with open(manifest_path, "r", encoding="utf-8") as handle:
            for line in handle:
                line = line.rstrip("\n")
                if not line or line.startswith("signature\t"):
                    continue
                parts = line.split("\t")
                if len(parts) < 4:
                    continue
                signature, title, rel_path, source_rel_path = parts[:4]
                rows[signature] = {
                    "title": title,
                    "rel_path": rel_path,
                    "source_rel_path": source_rel_path,
                }
    except OSError:
        return {}
    return rows


def _save_tiktok_manifest(device_mount, rows):
    manifest_path = resolve_under_root(device_mount, IPODTIKTOK_MANIFEST_PATH)
    ordered = sorted(
        rows.items(),
        key=lambda item: (_parse_tiktok_index(item[1].get("rel_path", "")), item[1].get("rel_path", "")),
    )
    lines = ["signature\ttitle\trel_path\tsource_rel_path"]
    for signature, row in ordered:
        lines.append(
            f"{signature}\t{row.get('title', '')}\t{row.get('rel_path', '')}\t{row.get('source_rel_path', '')}"
        )
    atomic_write_text(manifest_path, "\n".join(lines) + "\n")
    return manifest_path


def rebuild_tiktok_feed(device_mount, manifest_rows=None):
    manifest_rows = dict(manifest_rows or _load_tiktok_manifest(device_mount))
    device_mount = validate_device_root(device_mount)
    feed_path = resolve_under_root(device_mount, IPODTIKTOK_FEED_PATH)

    existing = []
    filtered_manifest = {}
    for signature, row in manifest_rows.items():
        rel_path = row.get("rel_path", "")
        try:
            full_path = resolve_under_root(device_mount, rel_path)
        except ValueError:
            continue
        if not rel_path or not os.path.isfile(full_path):
            continue
        filtered_manifest[signature] = row
        existing.append((signature, row))

    existing.sort(
        key=lambda item: (_parse_tiktok_index(item[1].get("rel_path", "")), item[1].get("rel_path", ""))
    )
    lines = ["id\ttitle\tpath"]
    for _signature, row in existing:
        rel_path = row.get("rel_path", "")
        clip_id = Path(rel_path).stem or f"clip{len(existing)}"
        title = row.get("title", "") or clip_id
        device_path = "/" + rel_path.replace(os.sep, "/").lstrip("/")
        lines.append(f"{clip_id}\t{title}\t{device_path}")
    atomic_write_text(feed_path, "\n".join(lines) + "\n")

    return {
        "feed_path": IPODTIKTOK_FEED_PATH,
        "feed_entries": len(existing),
        "manifest_rows": filtered_manifest,
    }


def build_ffmpeg_command(
    source_path,
    dest_path,
    kind,
    photo_duration_seconds=8.0,
    ffmpeg_path="ffmpeg",
):
    """Return the ffmpeg command for one Android media item."""
    scale_filter = (
        f"scale={IPOD_VIDEO_SIZE}:force_original_aspect_ratio=decrease,"
        f"pad={IPOD_VIDEO_SIZE}:(ow-iw)/2:(oh-ih)/2:black,fps={IPOD_VIDEO_FPS}"
    )
    base = [
        ffmpeg_path,
        "-hide_banner",
        "-loglevel",
        "error",
        "-y",
    ]
    if kind == "photo":
        duration = max(float(photo_duration_seconds or 0.0), 1.0)
        return base + [
            "-loop",
            "1",
            "-i",
            source_path,
            "-f",
            "lavfi",
            "-i",
            "anullsrc=channel_layout=stereo:sample_rate=44100",
            "-t",
            f"{duration:.2f}",
            "-vf",
            scale_filter,
            "-map",
            "0:v:0",
            "-map",
            "1:a:0",
            "-shortest",
            "-c:v",
            "mpeg2video",
            "-pix_fmt",
            "yuv420p",
            "-bf",
            "0",
            "-g",
            "12",
            "-flags",
            "+low_delay",
            "-q:v",
            IPOD_VIDEO_QSCALE,
            "-maxrate",
            IPOD_VIDEO_MAXRATE,
            "-bufsize",
            IPOD_VIDEO_BUFSIZE,
            "-c:a",
            "mp2",
            "-ar",
            "44100",
            "-ac",
            "2",
            "-b:a",
            IPOD_AUDIO_BITRATE,
            dest_path,
        ]
    return base + [
        "-i",
        source_path,
        "-vf",
        scale_filter,
        "-map",
        "0:v:0",
        "-map",
        "0:a:0?",
        "-c:v",
        "mpeg2video",
        "-pix_fmt",
        "yuv420p",
        "-bf",
        "0",
        "-g",
        "12",
        "-flags",
        "+low_delay",
        "-q:v",
        IPOD_VIDEO_QSCALE,
        "-maxrate",
        IPOD_VIDEO_MAXRATE,
        "-bufsize",
        IPOD_VIDEO_BUFSIZE,
        "-c:a",
        "mp2",
        "-ar",
        "44100",
        "-ac",
        "2",
        "-b:a",
        IPOD_AUDIO_BITRATE,
        dest_path,
    ]


def import_android_media(
    source_root,
    device_mount,
    device_subdir="Videos/Android Phone",
    include_photos=True,
    include_videos=True,
    photo_duration_seconds=8.0,
    for_tiktok_plugin=False,
    ffmpeg_path="",
    command_runner=None,
    progress_callback=None,
    cancel_check=None,
):
    """Convert mounted Android media into Rockbox-friendly MPEG files on the iPod."""
    ffmpeg_bin = ffmpeg_path or shutil.which("ffmpeg") or ""
    if not ffmpeg_bin:
        raise RuntimeError("ffmpeg is required for Android media import")
    if not source_root or not os.path.isdir(source_root):
        raise RuntimeError("Android source path is missing or not mounted")
    try:
        device_mount = validate_device_root(device_mount)
    except ValueError as exc:
        raise RuntimeError(str(exc)) from exc

    if for_tiktok_plugin:
        device_subdir = IPODTIKTOK_DEVICE_DIR
    runner = command_runner or CommandRunner(log_dir=os.path.join(device_mount, ".rockbox", "rockpod-logs"))
    items = scan_android_media(source_root, include_photos=include_photos, include_videos=include_videos)
    tiktok_manifest = _load_tiktok_manifest(device_mount) if for_tiktok_plugin else {}
    report = {
        "source_root": source_root,
        "device_mount": device_mount,
        "device_subdir": _normalize_rel_dir(device_subdir),
        "scanned": len(items),
        "imported": 0,
        "imported_photos": 0,
        "imported_videos": 0,
        "skipped_existing": 0,
        "failures": [],
        "cancelled": False,
        "imported_paths": [],
        "for_tiktok_plugin": bool(for_tiktok_plugin),
        "feed_path": "",
        "feed_entries": 0,
    }

    for index, item in enumerate(items, start=1):
        if cancel_check and cancel_check():
            report["cancelled"] = True
            break
        if progress_callback:
            progress_callback(
                index,
                len(items),
                f"Converting {os.path.basename(item['source_path'])}",
            )

        if for_tiktok_plugin:
            signature = _source_signature(item)
            existing = tiktok_manifest.get(signature, {})
            rel_path = existing.get("rel_path", "")
            if not rel_path:
                next_index = 1
                used = {
                    _parse_tiktok_index(row.get("rel_path", ""))
                    for row in tiktok_manifest.values()
                    if row.get("rel_path")
                }
                while next_index in used:
                    next_index += 1
                rel_path = os.path.join(IPODTIKTOK_DEVICE_DIR, f"ipodtiktok{next_index}.mpg")
            title = _tiktok_clip_title(item)
        else:
            rel_path = build_device_output_relpath(item, device_subdir)
            signature = ""
            title = ""
        dest_path = resolve_under_root(device_mount, rel_path)
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        if os.path.exists(dest_path):
            report["skipped_existing"] += 1
            if for_tiktok_plugin:
                tiktok_manifest[signature] = {
                    "title": title,
                    "rel_path": rel_path,
                    "source_rel_path": str(item.get("relative_path") or ""),
                }
            continue

        tmp_path = dest_path + ".rockpod_tmp"
        command = build_ffmpeg_command(
            item["source_path"],
            tmp_path,
            item["kind"],
            photo_duration_seconds=photo_duration_seconds,
            ffmpeg_path=ffmpeg_bin,
        )
        try:
            result = runner.run(command, cwd=os.path.dirname(tmp_path))
        except OSError as exc:
            raise RuntimeError(f"ffmpeg launch failed: {exc}") from exc

        if result.returncode != 0 or not os.path.exists(tmp_path):
            try:
                if os.path.exists(tmp_path):
                    os.remove(tmp_path)
            except OSError:
                pass
            message = (
                result.stderr
                or result.stdout
                or getattr(result, "failure_message", lambda: "")()
                or f"ffmpeg exited with {result.returncode}"
            ).strip()
            report["failures"].append({"path": item["source_path"], "error": message})
            continue

        os.replace(tmp_path, dest_path)
        report["imported"] += 1
        if item["kind"] == "photo":
            report["imported_photos"] += 1
        else:
            report["imported_videos"] += 1
        report["imported_paths"].append(rel_path)
        if for_tiktok_plugin:
            tiktok_manifest[signature] = {
                "title": title,
                "rel_path": rel_path,
                "source_rel_path": str(item.get("relative_path") or ""),
            }

    if for_tiktok_plugin:
        _save_tiktok_manifest(device_mount, tiktok_manifest)
        feed_info = rebuild_tiktok_feed(device_mount, tiktok_manifest)
        report["feed_path"] = feed_info["feed_path"]
        report["feed_entries"] = feed_info["feed_entries"]

    return report


class AndroidImportWorker(QObject):
    """Background worker that imports Android media into a connected iPod."""

    progress = Signal(int, int, str)
    finished = Signal(dict)
    error = Signal(str)
    cancelled = Signal()

    def __init__(
        self,
        source_root,
        device_mount,
        device_subdir,
        include_photos,
        include_videos,
        photo_duration_seconds,
        for_tiktok_plugin=False,
        ffmpeg_path="",
    ):
        super().__init__()
        self._source_root = source_root
        self._device_mount = device_mount
        self._device_subdir = device_subdir
        self._include_photos = include_photos
        self._include_videos = include_videos
        self._photo_duration_seconds = photo_duration_seconds
        self._for_tiktok_plugin = for_tiktok_plugin
        self._ffmpeg_path = ffmpeg_path
        self._cancelled = False

    def cancel(self):
        self._cancelled = True

    @Slot()
    def run(self):
        try:
            report = import_android_media(
                self._source_root,
                self._device_mount,
                device_subdir=self._device_subdir,
                include_photos=self._include_photos,
                include_videos=self._include_videos,
                photo_duration_seconds=self._photo_duration_seconds,
                for_tiktok_plugin=self._for_tiktok_plugin,
                ffmpeg_path=self._ffmpeg_path,
                progress_callback=self.progress.emit,
                cancel_check=lambda: self._cancelled,
            )
            if report.get("cancelled"):
                self.cancelled.emit()
            self.finished.emit(report)
        except Exception as exc:
            self.error.emit(str(exc))


class AndroidMediaImporter(QObject):
    """Threaded coordinator for Android phone media imports."""

    import_progress = Signal(int, int, str)
    import_finished = Signal(dict)
    import_error = Signal(str)
    import_cancelled = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._thread = None
        self._worker = None

    @property
    def is_running(self):
        return self._thread is not None and self._thread.isRunning()

    def start_import(
        self,
        source_root,
        device_mount,
        device_subdir,
        include_photos=True,
        include_videos=True,
        photo_duration_seconds=8.0,
        for_tiktok_plugin=False,
        ffmpeg_path="",
    ):
        if self.is_running:
            return False
        self._thread = QThread()
        self._worker = AndroidImportWorker(
            source_root,
            device_mount,
            device_subdir,
            include_photos,
            include_videos,
            photo_duration_seconds,
            for_tiktok_plugin=for_tiktok_plugin,
            ffmpeg_path=ffmpeg_path,
        )
        self._worker.moveToThread(self._thread)
        self._thread.started.connect(self._worker.run)
        self._worker.progress.connect(self.import_progress, Qt.QueuedConnection)
        self._worker.finished.connect(self._on_finished, Qt.QueuedConnection)
        self._worker.error.connect(self._on_error, Qt.QueuedConnection)
        self._worker.cancelled.connect(self.import_cancelled, Qt.QueuedConnection)
        self._thread.start()
        return True

    def cancel_import(self):
        if self._worker:
            self._worker.cancel()

    def shutdown(self):
        if self._worker:
            self._worker.cancel()
        if self._thread and self._thread.isRunning():
            self._thread.quit()
            self._thread.wait(5000)
        self._thread = None
        self._worker = None

    @Slot(dict)
    def _on_finished(self, report):
        self._cleanup()
        self.import_finished.emit(report)

    @Slot(str)
    def _on_error(self, message):
        self._cleanup()
        self.import_error.emit(message)

    def _cleanup(self):
        if self._thread:
            self._thread.quit()
            self._thread.wait(5000)
        self._thread = None
        self._worker = None
