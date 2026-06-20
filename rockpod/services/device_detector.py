"""Device detection for Rockbox iPods — real and mock modes."""

import logging
import hashlib
import os
import shutil
import time
from pathlib import Path

from PySide6.QtCore import QObject, Signal, QTimer

logger = logging.getLogger(__name__)

# Markers that indicate a Rockbox installation
ROCKBOX_MARKERS = [
    ".rockbox",
    ".rockbox/config.cfg",
    ".rockbox/rockbox-info.txt",
]

# Typical Linux mount points to scan
MOUNT_SCAN_PATHS = [
    "/media",
    "/mnt",
    "/run/media",
]

ROCKBOX_TARGET_INFO = {
    "ipod1g2g": {"model": "iPod 1G/2G", "device_type": "ipod", "screen_resolution": "160x128"},
    "ipod3g": {"model": "iPod 3G", "device_type": "ipod", "screen_resolution": "160x128"},
    "ipod4g": {"model": "iPod 4G", "device_type": "ipod", "screen_resolution": "220x176"},
    "ipodcolor": {"model": "iPod Color / Photo", "device_type": "ipod", "screen_resolution": "220x176"},
    "ipodvideo": {"model": "iPod Video 5G", "device_type": "video", "screen_resolution": "320x240"},
    "ipodvideo64mb": {"model": "iPod Video 5.5G", "device_type": "video", "screen_resolution": "320x240"},
    "ipod6g": {"model": "iPod Classic 6G", "device_type": "classic", "screen_resolution": "320x240"},
    "ipodmini1g": {"model": "iPod mini 1G", "device_type": "mini", "screen_resolution": "138x110"},
    "ipodmini2g": {"model": "iPod mini 2G", "device_type": "mini", "screen_resolution": "138x110"},
    "ipodnano1g": {"model": "iPod nano 1G", "device_type": "nano", "screen_resolution": "176x132"},
    "ipodnano2g": {"model": "iPod nano 2G", "device_type": "nano", "screen_resolution": "176x132"},
}


class DeviceInfo:
    """Information about a detected Rockbox device."""

    def __init__(self, mount_path):
        normalized_mount = os.path.normpath(str(mount_path or ""))
        self.mount_path = normalized_mount
        self.name = os.path.basename(normalized_mount) or "iPod"
        self.is_rockbox = False
        self.rockbox_version = ""
        self.rockbox_target = ""
        self.detected_model = ""
        self.device_type = ""
        self.screen_resolution = ""
        self.music_path = ""
        self.total_space = 0
        self.used_space = 0
        self.free_space = 0
        self.serial_or_signature = ""
        self.stable_device_key = ""
        self._detect()

    def _detect(self):
        if not os.path.isdir(self.mount_path):
            return

        # Check for Rockbox markers
        for marker in ROCKBOX_MARKERS:
            if os.path.exists(os.path.join(self.mount_path, marker)):
                self.is_rockbox = True
                break

        # Read Rockbox version if available
        info_file = os.path.join(self.mount_path, ".rockbox", "rockbox-info.txt")
        if os.path.isfile(info_file):
            try:
                with open(info_file, "r", encoding="utf-8", errors="replace") as f:
                    info_text = f.read()
                for line in info_text.splitlines():
                    if ":" not in line:
                        continue
                    key, value = line.split(":", 1)
                    label = key.strip().lower()
                    value = value.strip()
                    if label == "version":
                        self.rockbox_version = value
                    elif label == "target":
                        self.rockbox_target = value
            except OSError:
                pass

        self._apply_target_metadata()

        # Determine music path
        music_candidates = ["Music", "MUSIC", "music"]
        for mc in music_candidates:
            mp = os.path.join(self.mount_path, mc)
            if os.path.isdir(mp):
                self.music_path = mp
                break
        if not self.music_path:
            self.music_path = os.path.join(self.mount_path, "Music")

        # Disk space
        try:
            usage = shutil.disk_usage(self.mount_path)
            self.total_space = usage.total
            self.used_space = usage.used
            self.free_space = usage.free
        except OSError:
            pass

        self.serial_or_signature = self._build_signature()
        self.stable_device_key = f"rockbox:{self.serial_or_signature}"

    def refresh_space(self):
        """Refresh only capacity/free-space data without re-reading other metadata."""
        try:
            usage = shutil.disk_usage(self.mount_path)
            self.total_space = usage.total
            self.used_space = usage.used
            self.free_space = usage.free
        except OSError:
            pass

    def _build_signature(self):
        """Create a device key stable across routine Rockbox/runtime changes."""
        h = hashlib.sha256()
        h.update((os.path.basename(self.mount_path) or "iPod").encode("utf-8", "ignore"))
        h.update(str(self.total_space or 0).encode("ascii"))
        return h.hexdigest()[:24]

    def _apply_target_metadata(self):
        target_key = str(self.rockbox_target or "").strip().lower()
        info = ROCKBOX_TARGET_INFO.get(target_key)
        if info:
            self.detected_model = info["model"]
            self.device_type = info["device_type"]
            self.screen_resolution = info["screen_resolution"]
            self.name = info["model"]
            return
        if target_key:
            pretty = target_key.replace("_", " ").replace("-", " ").strip()
            self.detected_model = pretty.title()
            self.device_type = "rockbox"
        else:
            self.detected_model = ""
            self.device_type = ""
        self.screen_resolution = ""

    @property
    def space_used_pct(self):
        if self.total_space <= 0:
            return 0.0
        return (self.used_space / self.total_space) * 100.0

    def __repr__(self):
        rb = "Rockbox" if self.is_rockbox else "Unknown"
        return f"<DeviceInfo {self.name} [{rb}] at {self.mount_path}>"


class DeviceDetector(QObject):
    """Monitors for Rockbox device connections."""

    device_connected = Signal(object)      # DeviceInfo
    device_disconnected = Signal(str)      # mount_path
    device_space_updated = Signal(object)  # DeviceInfo

    def __init__(self, config):
        super().__init__()
        self._config = config
        self._current_device = None
        self._poll_timer = None
        self._poll_interval_connected_ms = int(self._config.get("device_poll_interval_connected_ms", 5000))
        self._poll_interval_disconnected_ms = int(self._config.get("device_poll_interval_disconnected_ms", 1500))
        self._debounce_polls = max(1, int(self._config.get("device_detection_debounce_polls", 2)))
        self._space_refresh_seconds = max(1.0, float(self._config.get("device_space_refresh_seconds", 15.0)))
        self._pending_signature = ""
        self._pending_connect_count = 0
        self._pending_disconnect_count = 0
        self._last_space_refresh_at = 0.0

    @property
    def current_device(self):
        return self._current_device

    @property
    def is_connected(self):
        return self._current_device is not None

    def start_polling(self, interval_ms=None):
        """Start periodic device scanning."""
        self._poll_timer = QTimer(self)
        self._poll_timer.timeout.connect(self._poll)
        if interval_ms is not None:
            self._poll_interval_connected_ms = int(interval_ms)
            self._poll_interval_disconnected_ms = int(interval_ms)
        self._poll_timer.start(self._poll_interval_disconnected_ms)
        # Defer the initial probe until the event loop is running so startup
        # doesn't block the UI while connect handlers do follow-up work.
        QTimer.singleShot(0, self._poll)

    def stop_polling(self):
        if self._poll_timer:
            self._poll_timer.stop()
            self._poll_timer = None

    def _poll(self):
        """Check for connected devices."""
        started = time.perf_counter()
        candidate = self._probe_connected_candidate()
        action = "none"
        now = time.monotonic()

        if candidate:
            self._pending_disconnect_count = 0
            signature = candidate["signature"]
            if self._current_device and self._current_device.mount_path == candidate["mount_path"]:
                self._pending_signature = ""
                self._pending_connect_count = 0
                self._set_poll_interval(self._poll_interval_connected_ms)
                if self._maybe_refresh_connected_device(now):
                    action = "space-update"
                else:
                    action = "same-device-suppressed"
            else:
                if signature != self._pending_signature:
                    self._pending_signature = signature
                    self._pending_connect_count = 1
                else:
                    self._pending_connect_count += 1
                action = f"connect-pending-{self._pending_connect_count}"
                if self._pending_connect_count >= self._debounce_polls:
                    device = self._build_device_info(candidate)
                    self._current_device = device
                    self._pending_signature = ""
                    self._pending_connect_count = 0
                    self._last_space_refresh_at = now
                    self._set_poll_interval(self._poll_interval_connected_ms)
                    self.device_connected.emit(device)
                    action = "connected"
                    if candidate["mode"] == "mock":
                        logger.info("Mock device connected: %s", device.mount_path)
                    elif candidate["mode"] == "override":
                        logger.info("Device found at override path: %s", device.mount_path)
                    else:
                        logger.info("Rockbox device detected: %s", device)
        else:
            self._pending_signature = ""
            self._pending_connect_count = 0
            if self._current_device:
                self._pending_disconnect_count += 1
                action = f"disconnect-pending-{self._pending_disconnect_count}"
                if self._pending_disconnect_count >= self._debounce_polls:
                    old_path = self._current_device.mount_path
                    self._current_device = None
                    self._pending_disconnect_count = 0
                    self._last_space_refresh_at = 0.0
                    self._set_poll_interval(self._poll_interval_disconnected_ms)
                    self.device_disconnected.emit(old_path)
                    action = "disconnected"
                    logger.info("Device disconnected: %s", old_path)
            else:
                self._set_poll_interval(self._poll_interval_disconnected_ms)
                action = "absent"

        elapsed_ms = (time.perf_counter() - started) * 1000.0
        logger.debug(
            "Device poll interval=%dms result=%s candidate=%s elapsed=%.2fms",
            self._poll_timer.interval() if self._poll_timer else 0,
            action,
            candidate["mount_path"] if candidate else "",
            elapsed_ms,
        )

    def _set_poll_interval(self, interval_ms):
        if self._poll_timer and self._poll_timer.interval() != interval_ms:
            self._poll_timer.setInterval(interval_ms)

    def _maybe_refresh_connected_device(self, now):
        if not self._current_device:
            return False
        if now - self._last_space_refresh_at < self._space_refresh_seconds:
            return False
        before = (
            self._current_device.total_space,
            self._current_device.used_space,
            self._current_device.free_space,
        )
        self._current_device.refresh_space()
        self._last_space_refresh_at = now
        after = (
            self._current_device.total_space,
            self._current_device.used_space,
            self._current_device.free_space,
        )
        if after != before:
            self.device_space_updated.emit(self._current_device)
            return True
        return False

    def _probe_connected_candidate(self):
        # Check mock device first.
        if self._config.mock_device_enabled:
            mock_path = self._config.mock_device_path
            if mock_path and os.path.isdir(mock_path):
                return {
                    "mount_path": mock_path,
                    "signature": f"mock:{os.path.realpath(mock_path)}",
                    "mode": "mock",
                }

        # Check explicit override.
        override = self._config.device_mount_path
        if override:
            candidate = self._probe_mount_path(override)
            if candidate:
                candidate["mode"] = "override"
                return candidate

        return self._scan_mount_points()

    def _probe_mount_path(self, mount_path):
        mount_path = os.path.normpath(str(mount_path or ""))
        try:
            is_dir = bool(mount_path) and os.path.isdir(mount_path)
        except OSError:
            return None
        if not is_dir:
            return None
        for marker in ROCKBOX_MARKERS:
            try:
                if os.path.exists(os.path.join(mount_path, marker)):
                    return {
                        "mount_path": mount_path,
                        "signature": f"mount:{os.path.realpath(mount_path)}",
                        "mode": "scan",
                    }
            except OSError:
                return None
        return None

    def _build_device_info(self, candidate):
        device = DeviceInfo(candidate["mount_path"])
        override_name = self._config.get_device_display_name(device=device)
        if override_name:
            device.name = override_name
        if candidate.get("mode") == "mock":
            if not override_name:
                device.name = "Mock iPod"
            device.is_rockbox = True
        return device

    def _scan_mount_points(self):
        """Scan common mount paths for a Rockbox device using cheap marker checks."""
        for base in MOUNT_SCAN_PATHS:
            try:
                if not os.path.isdir(base):
                    continue
            except OSError:
                continue
            try:
                entries = os.listdir(base)
            except OSError:
                continue
            for entry in entries:
                candidate = os.path.join(base, entry)
                try:
                    if not os.path.isdir(candidate):
                        continue
                except OSError:
                    continue
                found = self._probe_mount_path(candidate)
                if found:
                    return found
                try:
                    for sub in os.listdir(candidate):
                        sub_path = os.path.join(candidate, sub)
                        try:
                            if not os.path.isdir(sub_path):
                                continue
                        except OSError:
                            continue
                        found = self._probe_mount_path(sub_path)
                        if found:
                            return found
                except OSError:
                    continue
        return None

    def eject_device(self):
        """Attempt to safely unmount the current device."""
        if not self._current_device:
            return False, "No device connected"

        mount = self._current_device.mount_path

        # For mock devices, just disconnect
        if self._config.mock_device_enabled:
            self._current_device = None
            self.device_disconnected.emit(mount)
            return True, "Mock device ejected"

        # Try sync + umount on Linux
        try:
            import subprocess
            subprocess.run(["sync"], check=True, timeout=30)
            result = subprocess.run(
                ["umount", mount], capture_output=True, text=True, timeout=30
            )
            if result.returncode == 0:
                self._current_device = None
                self.device_disconnected.emit(mount)
                return True, f"Device ejected: {mount}"
            else:
                return False, f"Unmount failed: {result.stderr.strip()}"
        except Exception as e:
            return False, f"Eject error: {e}"

    def refresh_device(self):
        """Force a re-detection of the current device."""
        if self._current_device:
            self._current_device._detect()
            self._last_space_refresh_at = time.monotonic()
            self.device_space_updated.emit(self._current_device)


def create_mock_device(path, num_tracks=0):
    """Create a fake Rockbox device directory structure for testing.

    Returns the DeviceInfo for the created mock device.
    """
    os.makedirs(path, exist_ok=True)

    # Create .rockbox directory structure
    rb_dir = os.path.join(path, ".rockbox")
    os.makedirs(rb_dir, exist_ok=True)

    with open(os.path.join(rb_dir, "rockbox-info.txt"), "w") as f:
        f.write("Version: mock-v1.0\n")
        f.write("Target: ipod6g\n")

    with open(os.path.join(rb_dir, "config.cfg"), "w") as f:
        f.write("# Rockbox config\n")

    # Create Music directory
    music_dir = os.path.join(path, "Music")
    os.makedirs(music_dir, exist_ok=True)

    # Create some fake tagnavi and database structure dirs
    os.makedirs(os.path.join(rb_dir, "database"), exist_ok=True)

    logger.info("Created mock Rockbox device at %s", path)
    return DeviceInfo(path)
