"""Filesystem-based device storage breakdown for iPod summary UI."""

from __future__ import annotations

import os
from datetime import datetime

from PySide6.QtCore import QObject, QThread, Signal, Slot


THEME_PREFIXES = (
    ".rockbox/wps/",
    ".rockbox/themes/",
    ".rockbox/backdrops/",
    ".rockbox/icons/",
    ".rockbox/fonts/",
)


def analyze_device_storage(mount_path, total_bytes=0, free_bytes=0):
    mount_path = os.path.abspath(str(mount_path or ""))
    categories = {
        "music": 0,
        "games_plugins": 0,
        "themes_assets": 0,
        "rockbox_system": 0,
        "other": 0,
    }
    if not mount_path or not os.path.isdir(mount_path):
        return _build_summary(categories, total_bytes, free_bytes, 0)

    walked_used = 0
    for root, _dirs, files in os.walk(mount_path):
        for name in files:
            path = os.path.join(root, name)
            try:
                size = os.path.getsize(path)
            except OSError:
                continue
            walked_used += size
            rel = os.path.relpath(path, mount_path).replace(os.sep, "/")
            categories[_classify_relative_path(rel)] += size

    return _build_summary(categories, total_bytes, free_bytes, walked_used)


def _build_summary(categories, total_bytes, free_bytes, walked_used):
    total = max(int(total_bytes or 0), 0)
    free = max(int(free_bytes or 0), 0)
    reported_used = max(total - free, 0) if total else walked_used
    if reported_used > walked_used:
        categories["other"] += reported_used - walked_used
    used = sum(categories.values())
    if not total:
        total = used + free
    if total and free <= 0:
        free = max(total - used, 0)
    return {
        "total": total,
        "used": used,
        "free": free,
        "music": categories["music"],
        "games_plugins": categories["games_plugins"],
        "themes_assets": categories["themes_assets"],
        "rockbox_system": categories["rockbox_system"],
        "other": categories["other"],
        "scanned_at": datetime.now().isoformat(timespec="seconds"),
    }


def _classify_relative_path(rel_path):
    rel = str(rel_path or "").replace("\\", "/")
    if rel.startswith("Music/"):
        return "music"
    if rel.startswith(".rockbox/rocks/"):
        return "games_plugins"
    if any(rel.startswith(prefix) for prefix in THEME_PREFIXES):
        return "themes_assets"
    if rel.startswith(".rockbox/"):
        return "rockbox_system"
    return "other"


class DeviceStorageWorker(QObject):
    finished = Signal(str, dict)
    error = Signal(str, str)

    def __init__(self, device_key, mount_path, total_bytes, free_bytes):
        super().__init__()
        self._device_key = device_key
        self._mount_path = mount_path
        self._total_bytes = total_bytes
        self._free_bytes = free_bytes

    @Slot()
    def run(self):
        try:
            result = analyze_device_storage(self._mount_path, self._total_bytes, self._free_bytes)
            self.finished.emit(self._device_key, result)
        except Exception as exc:  # pragma: no cover - defensive
            self.error.emit(self._device_key, str(exc))


class DeviceStorageAnalyzer(QObject):
    finished = Signal(str, dict)
    error = Signal(str, str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._thread = None
        self._worker = None
        self._active_device_key = ""

    @property
    def is_running(self):
        return self._thread is not None and self._thread.isRunning()

    def start(self, device_key, mount_path, total_bytes=0, free_bytes=0):
        if not device_key or not mount_path:
            return False
        if self.is_running:
            return False
        self._active_device_key = device_key
        self._thread = QThread()
        self._worker = DeviceStorageWorker(device_key, mount_path, total_bytes, free_bytes)
        self._worker.moveToThread(self._thread)
        self._thread.started.connect(self._worker.run)
        self._worker.finished.connect(self._on_finished)
        self._worker.error.connect(self._on_error)
        self._worker.finished.connect(self._thread.quit)
        self._worker.error.connect(self._thread.quit)
        self._thread.finished.connect(self._cleanup)
        self._thread.start()
        return True

    def shutdown(self):
        if self._thread is not None and self._thread.isRunning():
            self._thread.quit()
            self._thread.wait(3000)
        self._cleanup()

    def _on_finished(self, device_key, result):
        self.finished.emit(device_key, result)

    def _on_error(self, device_key, message):
        self.error.emit(device_key, message)

    def _cleanup(self):
        if self._worker is not None:
            try:
                self._worker.deleteLater()
            except RuntimeError:
                pass
        if self._thread is not None:
            try:
                self._thread.deleteLater()
            except RuntimeError:
                pass
        self._worker = None
        self._thread = None
        self._active_device_key = ""
