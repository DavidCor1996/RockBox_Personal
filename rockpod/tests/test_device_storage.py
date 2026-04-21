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
    _make_file(os.path.join(mount, "Notes", "todo.txt"), 5)

    summary = analyze_device_storage(mount, total_bytes=500, free_bytes=300)

    assert summary["music"] == 100
    assert summary["games_plugins"] == 25
    assert summary["themes_assets"] == 15
    assert summary["rockbox_system"] == 10
    assert summary["other"] == 50
    assert summary["free"] == 300
    assert summary["used"] == 200


def test_device_storage_without_mount_returns_zeroed_summary(tmp_dir):
    summary = analyze_device_storage(os.path.join(tmp_dir, "missing"), total_bytes=0, free_bytes=0)

    assert summary["total"] == 0
    assert summary["used"] == 0
    assert summary["free"] == 0
    assert summary["music"] == 0
