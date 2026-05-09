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


def test_nano2g_candidates_include_generated_wallpapers_with_size_metadata(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _write_bmp(os.path.join(repo_root, "wps", "iPone_nano2g", "Wallpaper.bmp"), 176, 132)
    _write_bmp(os.path.join(repo_root, "wps", "iPone_nano2g", "ChargeWallpaper.bmp"), 176, 132)
    _write_bmp(os.path.join(repo_root, "rockpod", "generated", "lockscreen-nano2g-clean.bmp"), 176, 132)
    _write_bmp(os.path.join(repo_root, "rockpod", "generated", "lockscreen-classic-legacy.bmp"), 320, 240)
    _write_bmp(os.path.join(repo_root, "rockpod", "generated", "charge-wallpaper-nano2g-clean.bmp"), 176, 132)
    _write_bmp(os.path.join(repo_root, "rockpod", "generated", "charge-wallpaper-classic-legacy.bmp"), 320, 240)

    service = IPoneWallpaperService()
    candidates = service.list_candidates(repo_root, _profile(repo_root, "176x132"))

    lock_items = {
        os.path.basename(item["source_path"]): item for item in candidates["lock"]
    }
    charge_items = {
        os.path.basename(item["source_path"]): item for item in candidates["charge"]
    }

    assert lock_items["lockscreen-nano2g-clean.bmp"]["width"] == 176
    assert lock_items["lockscreen-nano2g-clean.bmp"]["height"] == 132
    assert lock_items["lockscreen-classic-legacy.bmp"]["width"] == 320
    assert lock_items["lockscreen-classic-legacy.bmp"]["height"] == 240
    assert charge_items["charge-wallpaper-nano2g-clean.bmp"]["width"] == 176
    assert charge_items["charge-wallpaper-nano2g-clean.bmp"]["height"] == 132
    assert charge_items["charge-wallpaper-classic-legacy.bmp"]["width"] == 320
    assert charge_items["charge-wallpaper-classic-legacy.bmp"]["height"] == 240


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


def test_build_apply_bundle_targets_blackery_wallpapers(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    lock_source = os.path.join(repo_root, "rockpod", "generated", "lockscreen-blackery.bmp")
    charge_source = os.path.join(repo_root, "rockpod", "generated", "charge-wallpaper-blackery.bmp")
    _write_bmp(os.path.join(repo_root, "wps", "Blackery", "Wallpaper.bmp"))
    _write_bmp(os.path.join(repo_root, "wps", "Blackery", "ChargeWallpaper.bmp"))
    _write_bmp(lock_source)
    _write_bmp(charge_source)

    service = IPoneWallpaperService()
    profile = _profile(repo_root)
    profile["selected_theme"] = "Blackery"

    candidates = service.list_candidates(repo_root, profile)
    lock_names = {os.path.basename(item["source_path"]) for item in candidates["lock"]}
    charge_names = {os.path.basename(item["source_path"]) for item in candidates["charge"]}
    assert "Wallpaper.bmp" in lock_names
    assert "ChargeWallpaper.bmp" in charge_names

    bundle = service.build_apply_bundle(profile, lock_source=lock_source, charge_source=charge_source)

    destinations = {item["destination_rel"] for item in bundle["assets"]}
    assert ".rockbox/wps/Blackery/Wallpaper.bmp" in destinations
    assert ".rockbox/wps/Blackery/WallpaperCurrent.bmp" in destinations
    assert ".rockbox/wps/Blackery/ChargeWallpaper.bmp" in destinations


def test_build_apply_bundle_targets_ipone_nano2g_wallpapers(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    lock_source = os.path.join(repo_root, "rockpod", "generated", "lockscreen-nano2g.bmp")
    charge_source = os.path.join(repo_root, "rockpod", "generated", "charge-wallpaper-nano2g.bmp")
    _write_bmp(lock_source, 176, 132, "#BBBBBB")
    _write_bmp(charge_source, 176, 132, "#777777")

    service = IPoneWallpaperService()
    profile = _profile(repo_root, "176x132")
    bundle = service.build_apply_bundle(profile, lock_source=lock_source, charge_source=charge_source)

    destinations = {item["destination_rel"] for item in bundle["assets"]}
    assert ".rockbox/wps/iPone_nano2g/Wallpaper.bmp" in destinations
    assert ".rockbox/wps/iPone_nano2g/WallpaperCurrent.bmp" in destinations
    assert ".rockbox/wps/iPone_nano2g/ChargeWallpaper.bmp" in destinations
    assert ".rockbox/wps/Wallpaper.bmp" in destinations
    assert ".rockbox/wps/ChargeWallpaper.bmp" in destinations


def test_build_apply_bundle_normalizes_nano2g_wallpaper_sources(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    lock_source = os.path.join(repo_root, "rockpod", "generated", "lockscreen-classic-sized.bmp")
    charge_source = os.path.join(repo_root, "rockpod", "generated", "charge-classic-sized.bmp")
    _write_bmp(lock_source, 320, 240, "#BBBBBB")
    _write_bmp(charge_source, 320, 240, "#777777")

    service = IPoneWallpaperService()
    profile = _profile(repo_root, "176x132")
    bundle = service.build_apply_bundle(profile, lock_source=lock_source, charge_source=charge_source)

    for item in bundle["assets"]:
        if item["kind"] not in {"lock_wallpaper", "charge_wallpaper"}:
            continue
        with Image.open(item["source_abs"]) as rendered:
            assert rendered.size == (176, 132)


def test_nano2g_theme_references_profile_wallpapers_and_menu_backdrop():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

    with open(os.path.join(repo_root, "wps", "iPone_nano2g.wps"), "r", encoding="utf-8") as handle:
        wps = handle.read()
    with open(os.path.join(repo_root, "wps", "iPone_nano2g.fms"), "r", encoding="utf-8") as handle:
        fms = handle.read()
    with open(os.path.join(repo_root, "wps", "iPone_nano2g.sbs"), "r", encoding="utf-8") as handle:
        sbs = handle.read()

    assert "iPone_nano2g/Wallpaper.bmp" in wps
    assert "iPone_nano2g/ChargeWallpaper.bmp" in wps
    assert "iPone_nano2g/Wallpaper.bmp" in fms
    assert "iPone_nano2g/ChargeWallpaper.bmp" in fms
    assert "iPone_nano2g/wpsbackdrop-176x132x16.bmp" in sbs
    assert "%xl(Wallpaper,Wallpaper.bmp)" not in sbs
    assert "LockMusic" not in wps
    assert "LockClock" not in wps
    assert "LockRadio" not in fms
    assert "LockClock" not in fms


def test_nano2g_lockscreen_source_no_longer_draws_opaque_top_or_unlock_bars():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

    with open(os.path.join(repo_root, "apps", "action.c"), "r", encoding="utf-8") as handle:
        action_c = handle.read()
    with open(os.path.join(repo_root, "apps", "root_menu.c"), "r", encoding="utf-8") as handle:
        root_menu_c = handle.read()

    assert 'lcd_fillrect(0, 0, LCD_WIDTH, 18);' not in action_c
    assert 'lcd_fillrect(18, 103, 140, 10);' not in action_c
    assert 'lcd_fillrect(0, 0, LCD_WIDTH, 18);' not in root_menu_c
    assert 'root_menu_nano2g_fill_roundish(18, 118, 140, 10, NANO2G_DASH_PANEL);' not in root_menu_c
    assert '"Slide hold to unlock"' not in action_c
    assert '"Slide hold switch to unlock"' not in root_menu_c


def test_build_apply_bundle_targets_ipone_3g_wallpapers(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    lock_source = os.path.join(repo_root, "rockpod", "generated", "lockscreen-3g.bmp")
    charge_source = os.path.join(repo_root, "rockpod", "generated", "charge-wallpaper-3g.bmp")
    _write_bmp(lock_source, 160, 128, "#BBBBBB")
    _write_bmp(charge_source, 160, 128, "#777777")

    service = IPoneWallpaperService()
    profile = _profile(repo_root, "160x128")
    profile["selected_theme"] = "iPone_3g"
    bundle = service.build_apply_bundle(profile, lock_source=lock_source, charge_source=charge_source)

    destinations = {item["destination_rel"] for item in bundle["assets"]}
    assert ".rockbox/wps/iPone_3g/Wallpaper.bmp" in destinations
    assert ".rockbox/wps/iPone_3g/WallpaperCurrent.bmp" in destinations
    assert ".rockbox/wps/iPone_3g/ChargeWallpaper.bmp" in destinations


def test_build_apply_bundle_defaults_160x128_to_coverpod_wallpapers(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    lock_source = os.path.join(repo_root, "rockpod", "generated", "lockscreen-coverpod.bmp")
    charge_source = os.path.join(repo_root, "rockpod", "generated", "charge-wallpaper-coverpod.bmp")
    _write_bmp(lock_source, 160, 128, "#BBBBBB")
    _write_bmp(charge_source, 160, 128, "#777777")

    service = IPoneWallpaperService()
    profile = _profile(repo_root, "160x128")
    bundle = service.build_apply_bundle(profile, lock_source=lock_source, charge_source=charge_source)

    destinations = {item["destination_rel"] for item in bundle["assets"]}
    assert ".rockbox/wps/CoverPod_3g/Wallpaper.bmp" in destinations
    assert ".rockbox/wps/CoverPod_3g/WallpaperCurrent.bmp" in destinations
    assert ".rockbox/wps/CoverPod_3g/ChargeWallpaper.bmp" in destinations


def test_build_apply_bundle_targets_galaxy_wallpapers(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    lock_source = os.path.join(repo_root, "rockpod", "generated", "lockscreen-galaxy.bmp")
    charge_source = os.path.join(repo_root, "rockpod", "generated", "charge-wallpaper-galaxy.bmp")
    _write_bmp(lock_source, 160, 128, "#BBBBBB")
    _write_bmp(charge_source, 160, 128, "#777777")

    service = IPoneWallpaperService()
    profile = _profile(repo_root, "160x128")
    profile["selected_theme"] = "Galaxy"
    bundle = service.build_apply_bundle(profile, lock_source=lock_source, charge_source=charge_source)

    destinations = {item["destination_rel"] for item in bundle["assets"]}
    assert ".rockbox/wps/Galaxy/Wallpaper.bmp" in destinations
    assert ".rockbox/wps/Galaxy/WallpaperCurrent.bmp" in destinations
    assert ".rockbox/wps/Galaxy/ChargeWallpaper.bmp" in destinations


def test_160x128_import_uses_four_greyscale_levels(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    source = os.path.join(tmp_dir, "gradient.png")
    os.makedirs(os.path.dirname(source), exist_ok=True)
    image = Image.new("RGB", (320, 128))
    for x in range(320):
        value = int(255 * x / 319)
        for y in range(128):
            image.putpixel((x, y), (value, value, value))
    image.save(source, "PNG")

    service = IPoneWallpaperService()
    item = service.import_candidate(repo_root, _profile(repo_root, "160x128"), "lock", source)

    with Image.open(item["source_path"]) as rendered:
        colors = {color for _count, color in rendered.convert("RGB").getcolors(maxcolors=1024)}

    assert colors == {(0, 0, 0), (85, 85, 85), (170, 170, 170), (255, 255, 255)}


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
        widget.set_themes(
            [
                {"id": "iPone", "name": "iPone"},
                {"id": "Blackery", "name": "Blackery"},
            ],
            "Blackery",
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
        assert widget.current_theme_id() == "Blackery"
        assert widget.current_selection()["theme_id"] == "Blackery"
        assert widget._lock_pane.current_candidate()["id"] == "lock-b"
        assert widget._charge_pane.current_candidate()["id"] == "charge-b"
    finally:
        widget.close()


def test_wallpaper_widget_theme_selection_follows_profile_refresh():
    app = QApplication.instance() or QApplication([])
    widget = IPoneWallpaperManagerWidget()
    try:
        widget.set_themes(
            [
                {"id": "iPone", "name": "iPone"},
                {"id": "Blackery", "name": "Blackery"},
            ],
            "Blackery",
        )
        assert widget.current_theme_id() == "Blackery"

        widget.set_themes(
            [
                {"id": "iPone", "name": "iPone"},
                {"id": "Blackery", "name": "Blackery"},
            ],
            "iPone",
        )
        assert widget.current_theme_id() == "iPone"
    finally:
        widget.close()
