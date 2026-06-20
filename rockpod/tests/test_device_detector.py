"""Tests for cheap, debounced device detection."""

import os

from services.device_detector import DeviceDetector
from services.device_detector import DeviceInfo, create_mock_device


def _temp_names(path):
    return [name for name in os.listdir(path) if name.startswith("tmp")]


class _FakeCommandResult:
    def __init__(self, command, returncode=0, stdout="", stderr="", log_path=""):
        self.command = command
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr
        self.log_path = log_path

    def failure_message(self):
        message = f"Command failed with exit status {self.returncode}: {' '.join(self.command)}"
        if self.log_path:
            message = f"{message}\nLog: {self.log_path}"
        return message


class _FakeCommandRunner:
    def __init__(self, results):
        self.results = list(results)
        self.commands = []

    def run(self, command, cwd="", timeout=None, env=None):
        command = [str(part) for part in command]
        self.commands.append(command)
        if self.results:
            result = self.results.pop(0)
            result.command = command
            return result
        return _FakeCommandResult(command)


class _FakeTimer:
    def __init__(self, interval=0):
        self._interval = interval

    def interval(self):
        return self._interval

    def setInterval(self, interval):
        self._interval = interval


def _candidate(path="/tmp/ipod", mode="scan"):
    return {"mount_path": path, "signature": f"{mode}:{path}", "mode": mode}


def test_same_device_repeatedly_detected_does_not_reinitialize(config, monkeypatch):
    config.mock_device_enabled = False
    detector = DeviceDetector(config)
    detector._debounce_polls = 1
    detector._poll_timer = _FakeTimer()

    connects = []
    detector.device_connected.connect(lambda device: connects.append(device.mount_path))

    built = []

    class _Device:
        def __init__(self, mount_path):
            self.mount_path = mount_path
            self.total_space = 0
            self.used_space = 0
            self.free_space = 0

        def refresh_space(self):
            pass

    monkeypatch.setattr(detector, "_probe_connected_candidate", lambda: _candidate())
    monkeypatch.setattr(detector, "_build_device_info", lambda candidate: built.append(candidate["mount_path"]) or _Device(candidate["mount_path"]))

    detector._poll()
    detector._poll()
    detector._poll()

    assert connects == ["/tmp/ipod"]
    assert built == ["/tmp/ipod"]


def test_connect_disconnect_debounce_requires_stable_state(config, monkeypatch):
    config.mock_device_enabled = False
    detector = DeviceDetector(config)
    detector._debounce_polls = 2
    detector._poll_timer = _FakeTimer()

    connects = []
    disconnects = []
    detector.device_connected.connect(lambda device: connects.append(device.mount_path))
    detector.device_disconnected.connect(disconnects.append)

    sequence = iter([_candidate(), _candidate(), None, None])

    class _Device:
        def __init__(self, mount_path):
            self.mount_path = mount_path
            self.total_space = 0
            self.used_space = 0
            self.free_space = 0

        def refresh_space(self):
            pass

    monkeypatch.setattr(detector, "_probe_connected_candidate", lambda: next(sequence))
    monkeypatch.setattr(detector, "_build_device_info", lambda candidate: _Device(candidate["mount_path"]))

    detector._poll()
    assert connects == []
    detector._poll()
    assert connects == ["/tmp/ipod"]
    detector._poll()
    assert disconnects == []
    detector._poll()
    assert disconnects == ["/tmp/ipod"]


def test_reconnect_path_switches_between_fast_and_slow_polling(config, monkeypatch):
    config.mock_device_enabled = False
    detector = DeviceDetector(config)
    detector._debounce_polls = 1
    detector._poll_interval_connected_ms = 5000
    detector._poll_interval_disconnected_ms = 1000
    detector._poll_timer = _FakeTimer(1000)

    connects = []
    disconnects = []
    detector.device_connected.connect(lambda device: connects.append(device.mount_path))
    detector.device_disconnected.connect(disconnects.append)

    sequence = iter([_candidate(), None, _candidate("/tmp/ipod-two")])

    class _Device:
        def __init__(self, mount_path):
            self.mount_path = mount_path
            self.total_space = 0
            self.used_space = 0
            self.free_space = 0

        def refresh_space(self):
            pass

    monkeypatch.setattr(detector, "_probe_connected_candidate", lambda: next(sequence))
    monkeypatch.setattr(detector, "_build_device_info", lambda candidate: _Device(candidate["mount_path"]))

    detector._poll()
    assert detector._poll_timer.interval() == 5000
    detector._poll()
    assert detector._poll_timer.interval() == 1000
    detector._poll()
    assert detector._poll_timer.interval() == 5000
    assert connects == ["/tmp/ipod", "/tmp/ipod-two"]
    assert disconnects == ["/tmp/ipod"]


def test_space_update_is_throttled_for_same_connected_device(config, monkeypatch):
    config.mock_device_enabled = False
    detector = DeviceDetector(config)
    detector._debounce_polls = 1
    detector._space_refresh_seconds = 60.0
    detector._poll_timer = _FakeTimer()

    updates = []
    detector.device_space_updated.connect(lambda device: updates.append(device.mount_path))

    class _Device:
        def __init__(self, mount_path):
            self.mount_path = mount_path
            self.total_space = 10
            self.used_space = 5
            self.free_space = 5
            self.refresh_calls = 0

        def refresh_space(self):
            self.refresh_calls += 1

    device = _Device("/tmp/ipod")
    detector._current_device = device
    detector._last_space_refresh_at = 1000.0

    monkeypatch.setattr(detector, "_probe_connected_candidate", lambda: _candidate())
    monkeypatch.setattr("services.device_detector.time.monotonic", lambda: 1005.0)

    detector._poll()

    assert updates == []
    assert device.refresh_calls == 0


def test_stable_device_key_ignores_config_changes(tmp_dir):
    mount = os.path.join(tmp_dir, "ipod")
    create_mock_device(mount)
    assert not _temp_names(os.path.join(mount, ".rockbox"))

    first = DeviceInfo(mount).stable_device_key

    with open(os.path.join(mount, ".rockbox", "config.cfg"), "a", encoding="utf-8") as handle:
        handle.write("\nresume next track: on\n")

    second = DeviceInfo(mount).stable_device_key

    assert first == second


def test_stable_device_key_ignores_rockbox_info_changes(tmp_dir):
    mount = os.path.join(tmp_dir, "ipod")
    create_mock_device(mount)

    first = DeviceInfo(mount).stable_device_key

    with open(os.path.join(mount, ".rockbox", "rockbox-info.txt"), "w", encoding="utf-8") as handle:
        handle.write("Version: custom-build\nTarget: ipod6g\n")

    second = DeviceInfo(mount).stable_device_key

    assert first == second


def test_device_info_reads_rockbox_target_metadata(tmp_dir):
    mount = os.path.join(tmp_dir, "ipod")
    create_mock_device(mount)

    info = DeviceInfo(mount)

    assert info.rockbox_target == "ipod6g"
    assert info.detected_model == "iPod Classic 6G"
    assert info.device_type == "classic"
    assert info.screen_resolution == "320x240"


def test_probe_mount_path_normalizes_trailing_slash(config):
    create_mock_device(config.mock_device_path)
    detector = DeviceDetector(config)
    result = detector._probe_mount_path(config.mock_device_path + "/")

    assert result is not None
    assert result["mount_path"] == os.path.normpath(config.mock_device_path)


def test_eject_device_uses_shared_command_runner(config, tmp_dir):
    config.mock_device_enabled = False
    mount = os.path.join(tmp_dir, "ipod")
    create_mock_device(mount)
    runner = _FakeCommandRunner([_FakeCommandResult([], 0), _FakeCommandResult([], 0)])
    detector = DeviceDetector(config, command_runner=runner)
    detector._current_device = DeviceInfo(mount)
    disconnected = []
    detector.device_disconnected.connect(disconnected.append)

    ok, message = detector.eject_device()

    assert ok is True
    assert "Device ejected" in message
    assert runner.commands == [["sync"], ["umount", mount]]
    assert disconnected == [mount]
    assert detector.current_device is None


def test_eject_device_reports_runner_failure_with_log(config, tmp_dir):
    config.mock_device_enabled = False
    mount = os.path.join(tmp_dir, "ipod")
    create_mock_device(mount)
    log_path = os.path.join(tmp_dir, "umount.log")
    runner = _FakeCommandRunner([
        _FakeCommandResult([], 0),
        _FakeCommandResult([], 32, stderr="", log_path=log_path),
    ])
    detector = DeviceDetector(config, command_runner=runner)
    detector._current_device = DeviceInfo(mount)

    ok, message = detector.eject_device()

    assert ok is False
    assert "Unmount failed" in message
    assert "exit status 32" in message
    assert log_path in message
    assert detector.current_device is not None


def test_scan_mount_points_ignores_io_errors_and_finds_valid_device(config, monkeypatch):
    config.mock_device_enabled = False
    detector = DeviceDetector(config)

    broken = "/run/media/david/Media"
    good_parent = "/run/media/david"
    good_device = "/run/media/david/IPOD"

    def fake_isdir(path):
        normalized = os.path.normpath(path)
        if normalized in {"/run/media", good_parent, good_device}:
            return True
        if normalized == broken:
            raise OSError("Input/output error")
        return False

    def fake_listdir(path):
        normalized = os.path.normpath(path)
        if normalized == "/run/media":
            return ["david"]
        if normalized == good_parent:
            return ["Media", "IPOD"]
        if normalized == broken:
            raise OSError("Input/output error")
        return []

    def fake_probe(path):
        return _candidate(good_device) if os.path.normpath(path) == good_device else None

    monkeypatch.setattr("services.device_detector.os.path.isdir", fake_isdir)
    monkeypatch.setattr("services.device_detector.os.listdir", fake_listdir)
    monkeypatch.setattr(detector, "_probe_mount_path", fake_probe)

    result = detector._scan_mount_points()

    assert result == _candidate(good_device)
