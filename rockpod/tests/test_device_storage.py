import os

from services.device_storage import analyze_device_storage


def _make_file(path, size):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as handle:
        handle.write(b"x" * size)


def test_device_storage_categories_and_free_space(tmp_dir):
    mount = os.path.join(tmp_dir, "ipod")
    _make_file(os.path.join(mount, "Music", "Album", "track.mp3"), 100)
    _make_file(os.path.join(mount, ".rockbox", "rocks", "games", "rockboy.rock"), 25)
    _make_file(os.path.join(mount, ".rockbox", "wps", "theme.wps"), 15)
    _make_file(os.path.join(mount, ".rockbox", "config.cfg"), 10)
    _make_file(os.path.join(mount, "Linux", "rockpod-linux", "rootfs.squashfs"), 40)
    _make_file(os.path.join(mount, "Notes", "todo.txt"), 5)

    summary = analyze_device_storage(mount, total_bytes=500, free_bytes=260)

    assert summary["music"] == 100
    assert summary["games_plugins"] == 25
    assert summary["themes_assets"] == 15
    assert summary["rockbox_system"] == 10
    assert summary["linux_system"] == 40
    assert summary["other"] == 50
    assert summary["free"] == 260
    assert summary["used"] == 240


def test_device_storage_classifies_rockpod_linux_boot_files(tmp_dir):
    mount = os.path.join(tmp_dir, "ipod")
    _make_file(os.path.join(mount, "EFI", "BOOT", "BOOTX64.EFI"), 20)
    _make_file(os.path.join(mount, "EFI", "BOOT", "grub.cfg"), 10)
    _make_file(os.path.join(mount, "EFI", "BOOT", "rockpod-linux", "theme.cfg"), 5)
    _make_file(os.path.join(mount, ".rockpod-linux", "state.json"), 7)

    summary = analyze_device_storage(mount, total_bytes=100, free_bytes=58)

    assert summary["linux_system"] == 42
    assert summary["other"] == 0
    assert summary["free"] == 58


def test_device_storage_classifies_debian_live_tree_as_linux(tmp_dir):
    mount = os.path.join(tmp_dir, "ipod")
    _make_file(os.path.join(mount, "live", "filesystem.squashfs"), 100)
    _make_file(os.path.join(mount, "boot", "grub", "grub.cfg"), 20)
    _make_file(os.path.join(mount, "isolinux", "isolinux.cfg"), 10)
    _make_file(os.path.join(mount, ".disk", "info"), 5)

    summary = analyze_device_storage(mount, total_bytes=200, free_bytes=65)

    assert summary["linux_system"] == 135
    assert summary["other"] == 0


def test_device_storage_without_mount_returns_zeroed_summary(tmp_dir):
    summary = analyze_device_storage(os.path.join(tmp_dir, "missing"), total_bytes=0, free_bytes=0)

    assert summary["total"] == 0
    assert summary["used"] == 0
    assert summary["free"] == 0
    assert summary["music"] == 0
