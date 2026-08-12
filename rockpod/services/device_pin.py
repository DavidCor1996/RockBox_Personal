"""Shared 4-digit PIN that gates Settings, Locked Videos, Magazines and Comics.

The firmware keeps a single PIN file at ``/.rockbox/videolist/locked.pin`` and
reads it from two independent places:

* ``apps/root_menu.c`` (Settings lock and the Locked Videos category), and
* ``apps/plugins/mpegplayer/livetv_guide.c`` (parental-locked channels).

Both read one line, trim the newline, then require exactly four ASCII digits.
``root_menu.c`` is the stricter of the two -- it does not trim spaces or tabs
-- so anything written here must satisfy it: four digits, one trailing
newline, no BOM and no leading whitespace.
"""

from __future__ import annotations

import os


PIN_DIR_NAME = "videolist"
PIN_FILE_NAME = "locked.pin"
PIN_LENGTH = 4
PIN_DIGITS = "0123456789"


class DevicePinError(RuntimeError):
    """A user-facing failure while reading or writing the device PIN."""


def pin_directory(mount_path):
    """Return the directory holding the PIN file for ``mount_path``."""
    return os.path.join(str(mount_path or ""), ".rockbox", PIN_DIR_NAME)


def pin_path(mount_path):
    """Return the full path of the PIN file for ``mount_path``."""
    return os.path.join(pin_directory(mount_path), PIN_FILE_NAME)


def is_valid_pin(pin):
    """True when ``pin`` is exactly four ASCII digits.

    ``str.isdigit()`` is deliberately not used: it accepts non-ASCII digits
    that the firmware's ``isdigit()`` check would reject.
    """
    text = str(pin or "")
    return len(text) == PIN_LENGTH and all(ch in PIN_DIGITS for ch in text)


def read_pin(mount_path):
    """Return the PIN stored on ``mount_path``, or "" when unset or invalid."""
    path = pin_path(mount_path)
    try:
        with open(path, "r", encoding="ascii", errors="replace") as handle:
            line = handle.readline()
    except OSError:
        return ""
    # Mirrors video_trim_line(): only the newline is trimmed, so a leading
    # space fails here exactly as it fails in the firmware.
    line = line.rstrip("\r\n")
    return line if is_valid_pin(line) else ""


def pin_is_set(mount_path):
    """True when ``mount_path`` holds a PIN the firmware will accept."""
    return bool(read_pin(mount_path))


def write_pin(mount_path, pin):
    """Write ``pin`` to ``mount_path`` atomically and flush it to the device.

    Raises :class:`DevicePinError` when the PIN is malformed or the device is
    unavailable.
    """
    mount = str(mount_path or "").strip()
    if not mount or not os.path.isdir(mount):
        raise DevicePinError(f"Device is unavailable: {mount or '(none)'}")
    if not is_valid_pin(pin):
        raise DevicePinError("The PIN must be exactly 4 digits (0-9).")

    directory = pin_directory(mount)
    temporary = os.path.join(directory, PIN_FILE_NAME + ".tmp")
    final = os.path.join(directory, PIN_FILE_NAME)
    try:
        os.makedirs(directory, exist_ok=True)
        with open(temporary, "w", encoding="ascii", newline="\n") as output:
            output.write(str(pin) + "\n")
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, final)
        _sync_directory(directory)
    except OSError as exc:
        try:
            os.remove(temporary)
        except OSError:
            pass
        raise DevicePinError(f"Could not write the PIN to the device: {exc}")
    return final


def clear_pin(mount_path):
    """Remove the PIN file. Returns True when a file was actually removed."""
    directory = pin_directory(mount_path)
    path = os.path.join(directory, PIN_FILE_NAME)
    try:
        os.remove(path)
    except FileNotFoundError:
        return False
    except OSError as exc:
        raise DevicePinError(f"Could not remove the PIN from the device: {exc}")
    _sync_directory(directory)
    return True


def _sync_directory(directory):
    """Best-effort flush of a directory entry; FAT mounts may refuse this."""
    try:
        fd = os.open(directory, os.O_RDONLY)
    except OSError:
        return
    try:
        os.fsync(fd)
    except OSError:
        pass
    finally:
        os.close(fd)
