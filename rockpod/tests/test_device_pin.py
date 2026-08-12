"""The shared device PIN must satisfy both firmware readers byte-for-byte."""

import os
from pathlib import Path

import pytest

from services.device_pin import (
    DevicePinError,
    clear_pin,
    is_valid_pin,
    pin_is_set,
    pin_path,
    read_pin,
    write_pin,
)


ROOT = Path(__file__).parents[2]
ROOT_MENU = ROOT / "apps" / "root_menu.c"
GUIDE = ROOT / "apps" / "plugins" / "mpegplayer" / "livetv_guide.c"


def test_pin_path_matches_the_firmware_location(tmp_path):
    assert pin_path(tmp_path) == os.path.join(
        str(tmp_path), ".rockbox", "videolist", "locked.pin"
    )
    assert 'VIDEO_LIST_ROOT "/locked.pin"' in ROOT_MENU.read_text(encoding="utf-8")
    assert 'ROCKBOX_DIR "/videolist/locked.pin"' in GUIDE.read_text(encoding="utf-8")


def test_write_pin_emits_four_digits_and_one_newline(tmp_path):
    written = write_pin(tmp_path, "2468")

    assert written == pin_path(tmp_path)
    raw = Path(written).read_bytes()
    assert raw == b"2468\n"
    assert read_pin(tmp_path) == "2468"
    assert pin_is_set(tmp_path) is True


def test_write_pin_creates_the_videolist_directory(tmp_path):
    assert not (tmp_path / ".rockbox" / "videolist").exists()
    write_pin(tmp_path, "0000")
    assert (tmp_path / ".rockbox" / "videolist" / "locked.pin").is_file()


def test_write_pin_replaces_an_existing_pin(tmp_path):
    write_pin(tmp_path, "1111")
    write_pin(tmp_path, "9876")
    assert Path(pin_path(tmp_path)).read_bytes() == b"9876\n"
    assert not os.path.exists(pin_path(tmp_path) + ".tmp")


@pytest.mark.parametrize(
    "pin", ["", "123", "12345", "12a4", "12 4", " 123", "12.4", "١٢٣٤", None]
)
def test_write_pin_rejects_anything_the_firmware_would_reject(tmp_path, pin):
    assert is_valid_pin(pin) is False
    with pytest.raises(DevicePinError):
        write_pin(tmp_path, pin)
    assert not os.path.exists(pin_path(tmp_path))


def test_write_pin_rejects_a_missing_mount(tmp_path):
    with pytest.raises(DevicePinError):
        write_pin(tmp_path / "not-mounted", "1234")


def test_read_pin_reports_unset_for_missing_or_malformed_files(tmp_path):
    assert read_pin(tmp_path) == ""
    assert pin_is_set(tmp_path) is False

    directory = tmp_path / ".rockbox" / "videolist"
    directory.mkdir(parents=True)
    (directory / "locked.pin").write_bytes(b"\n")
    assert read_pin(tmp_path) == ""
    (directory / "locked.pin").write_bytes(b"abcd\n")
    assert read_pin(tmp_path) == ""
    (directory / "locked.pin").write_bytes(b" 1234\n")
    assert read_pin(tmp_path) == ""


def test_read_pin_tolerates_a_crlf_line_ending(tmp_path):
    directory = tmp_path / ".rockbox" / "videolist"
    directory.mkdir(parents=True)
    (directory / "locked.pin").write_bytes(b"1234\r\n")
    assert read_pin(tmp_path) == "1234"


def test_clear_pin_removes_the_file_once(tmp_path):
    write_pin(tmp_path, "1234")
    assert clear_pin(tmp_path) is True
    assert read_pin(tmp_path) == ""
    assert clear_pin(tmp_path) is False


def test_settings_lock_needs_a_valid_pin_before_it_locks_anyone_out():
    """"Lock Settings Menu" sits inside the menu it guards, so an absent PIN
    must leave Settings reachable rather than stranding the user."""
    root = ROOT_MENU.read_text(encoding="utf-8")

    assert "static bool videos_settings_lock_active(void)" in root
    active = root.split("static bool videos_settings_lock_active(void)", 1)[1]
    active = active.split("\n}", 1)[0]
    assert "global_settings.ui_engine_lock_settings" in active
    assert "videos_read_shared_pin(expected, sizeof(expected)) == 0" in active

    assert "if (videos_settings_lock_active() &&" in root

    # Locked Videos must still refuse on a malformed PIN rather than fall open.
    unlock = root.split("static bool videos_unlock_with_shared_pin", 1)[1]
    unlock = unlock.split("\n}", 1)[0]
    assert "if (rc < 0)" in unlock and "if (rc > 0)" in unlock
    assert unlock.count("return false;") >= 3


def test_rockpod_exposes_the_pin_editor_under_the_device_menu():
    main_window = (ROOT / "rockpod" / "ui" / "main_window.py").read_text(
        encoding="utf-8"
    )
    dialog = (ROOT / "rockpod" / "ui" / "dialogs" / "device_pin.py").read_text(
        encoding="utf-8"
    )

    device_menu = main_window.split('menubar.addMenu("Device")', 1)[1]
    device_menu = device_menu.split("def _connect_signals", 1)[0]
    assert '"Set Device PIN...", self._show_device_pin' in device_menu
    assert "from ui.dialogs.device_pin import DevicePinDialog" in main_window
    assert "from services.device_pin import" in dialog
