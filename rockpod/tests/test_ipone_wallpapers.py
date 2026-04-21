import os

from PIL import Image
from PySide6.QtWidgets import QApplication

from services.ipone_wallpapers import IPoneWallpaperService
from ui.ipone_wallpaper_manager import IPoneWallpaperManagerWidget


def _write_bmp(path, width=320, height=240, color="#222244"):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    Image.new("RGB", (width, height), color).save(path, "BMP")


def _profile(repo_root, resolution="320x240"):
    return {
        "id": f"profile-{resolution}",
        "name": f"Profile {resolution}",
        "screen_resolution": resolution,
        "source_repo_path": repo_root,
        "selected_theme": "iPone" if resolution == "320x240" else "iPone_nano2g",
    }


def test_lists_theme_and_generated_wallpapers_without_intermediate_files(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _write_bmp(os.path.join(repo_root, "wps", "iPone", "Wallpaper.bmp"))
    _write_bmp(os.path.join(repo_root, "wps", "iPone", "WallpaperAlt.bmp"))
    _write_bmp(os.path.join(repo_root, "wps", "iPone", "ChargeWallpaper.bmp"))
    _write_bmp(os.path.join(repo_root, "wps", "iPone", "ChargeWallpaperAlt.bmp"))
    _write_bmp(os.path.join(repo_root, "rockpod", "generated", "lockscreen-besties-trio-v5.bmp"))
    _write_bmp(os.path.join(repo_root, "rockpod", "generated", "charge-wallpaper-new-screenshot-v6.bmp"))
    _write_bmp(os.path.join(repo_root, "rockpod", "generated", "lockscreen-solo-new-v2.bmp"))
    _write_bmp(os.path.join(repo_root, "rockpod", "generated", "lockscreen-solo-new-v2-mask.bmp"))
    _write_bmp(os.path.join(repo_root, "rockpod", "generated", "lockscreen-direct-local-v3-subject.bmp"))

    service = IPoneWallpaperService()
    candidates = service.list_candidates(repo_root, _profile(repo_root))

    lock_names = {os.path.basename(item["source_path"]) for item in candidates["lock"]}
    charge_names = {os.path.basename(item["source_path"]) for item in candidates["charge"]}

    assert "Wallpaper.bmp" in lock_names
    assert "WallpaperAlt.bmp" in lock_names
    assert "lockscreen-besties-trio-v5.bmp" in lock_names
    assert "lockscreen-solo-new-v2.bmp" in lock_names
    assert "lockscreen-solo-new-v2-mask.bmp" not in lock_names
    assert "lockscreen-direct-local-v3-subject.bmp" not in lock_names
    assert "ChargeWallpaper.bmp" in charge_names
    assert "ChargeWallpaperAlt.bmp" in charge_names
    assert "charge-wallpaper-new-screenshot-v6.bmp" in charge_names


def test_build_apply_bundle_targets_active_ipone_wallpapers(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    lock_source = os.path.join(repo_root, "rockpod", "generated", "lockscreen-besties-trio-v5.bmp")
    charge_source = os.path.join(repo_root, "rockpod", "generated", "charge-wallpaper-new-screenshot-v6.bmp")
    _write_bmp(lock_source)
    _write_bmp(charge_source)

    service = IPoneWallpaperService()
    bundle = service.build_apply_bundle(_profile(repo_root), lock_source=lock_source, charge_source=charge_source)

    destinations = {item["destination_rel"] for item in bundle["assets"]}
    assert ".rockbox/wps/iPone/Wallpaper.bmp" in destinations
    assert ".rockbox/wps/iPone/WallpaperCurrent.bmp" in destinations
    assert ".rockbox/wps/iPone/ChargeWallpaper.bmp" in destinations


def test_import_candidate_converts_to_profile_sized_bmp(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    source = os.path.join(tmp_dir, "source.png")
    os.makedirs(os.path.dirname(source), exist_ok=True)
    Image.new("RGB", (900, 500), "#8844ff").save(source, "PNG")

    service = IPoneWallpaperService()
    item = service.import_candidate(repo_root, _profile(repo_root), "lock", source)

    assert os.path.isfile(item["source_path"])
    assert item["source_path"].endswith(".bmp")
    with Image.open(item["source_path"]) as rendered:
        assert rendered.size == (320, 240)


def test_remove_candidate_deletes_generated_wallpaper_only(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    generated = os.path.join(repo_root, "rockpod", "generated", "lockscreen-test.bmp")
    theme = os.path.join(repo_root, "wps", "iPone", "Wallpaper.bmp")
    _write_bmp(generated)
    _write_bmp(theme)

    service = IPoneWallpaperService()

    assert service.remove_candidate(repo_root, generated) is True
    assert not os.path.exists(generated)

    try:
        service.remove_candidate(repo_root, theme)
    except ValueError:
        pass
    else:
        raise AssertionError("Expected ValueError for built-in theme wallpaper removal")


def test_wallpaper_widget_preserves_selection_across_candidate_refreshes():
    app = QApplication.instance() or QApplication([])
    widget = IPoneWallpaperManagerWidget()
    try:
        widget.set_profiles(
            [
                {"id": "p1", "name": "Profile 1"},
                {"id": "p2", "name": "Profile 2"},
            ],
            "p1",
        )
        widget.set_candidates(
            [
                {"id": "lock-a", "label": "Lock A", "source_path": "/tmp/lock-a.bmp", "preview_path": "", "removable": True},
                {"id": "lock-b", "label": "Lock B", "source_path": "/tmp/lock-b.bmp", "preview_path": "", "removable": True},
            ],
            [
                {"id": "charge-a", "label": "Charge A", "source_path": "/tmp/charge-a.bmp", "preview_path": "", "removable": True},
                {"id": "charge-b", "label": "Charge B", "source_path": "/tmp/charge-b.bmp", "preview_path": "", "removable": True},
            ],
        )
        widget._lock_pane._list.setCurrentRow(1)
        widget._charge_pane._list.setCurrentRow(1)

        widget.set_profiles(
            [
                {"id": "p1", "name": "Profile 1"},
                {"id": "p2", "name": "Profile 2"},
            ],
            "p1",
        )
        widget.set_candidates(
            [
                {"id": "lock-a", "label": "Lock A", "source_path": "/tmp/lock-a.bmp", "preview_path": "", "removable": True},
                {"id": "lock-b", "label": "Lock B", "source_path": "/tmp/lock-b.bmp", "preview_path": "", "removable": True},
            ],
            [
                {"id": "charge-a", "label": "Charge A", "source_path": "/tmp/charge-a.bmp", "preview_path": "", "removable": True},
                {"id": "charge-b", "label": "Charge B", "source_path": "/tmp/charge-b.bmp", "preview_path": "", "removable": True},
            ],
        )

        assert widget.current_profile_id() == "p1"
        assert widget._lock_pane.current_candidate()["id"] == "lock-b"
        assert widget._charge_pane.current_candidate()["id"] == "charge-b"
    finally:
        widget.close()
