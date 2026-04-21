import os

from PIL import Image
from PySide6.QtWidgets import QApplication

from services.rockbox_deploy import RockboxDeployService
from services.theme_designer import ThemeDesignerService
from ui.theme_designer import ThemeDesignerWidget


def _write_text(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(content)


def _write_bmp(path, width, height, color):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    image = Image.new("RGB", (width, height), color)
    image.save(path, "BMP")


def _make_repo(repo_root):
    _write_text(
        os.path.join(repo_root, "themes", "iPone.cfg"),
        "\n".join(
            [
                "wps: /.rockbox/wps/iPone.wps",
                "sbs: /.rockbox/wps/iPone.sbs",
                "fms: /.rockbox/wps/iPone.fms",
                "backdrop: /.rockbox/backdrops/iPone_bd.bmp",
                "background color: 100F16",
                "foreground color: F7F4FA",
                "font: /.rockbox/fonts/24 iLike.fnt",
                "iconset: /.rockbox/icons/iPone.bmp",
                "line selector start color: 2B2234",
                "line selector end color: 9D7AE6",
                "line selector text color: FCF9FF",
                "list separator color: 1A1621",
                "",
            ]
        ),
    )
    _write_text(os.path.join(repo_root, "wps", "iPone.wps"), "%wd\n%xl(Lockscreen,Wallpaper.bmp)\n")
    _write_text(os.path.join(repo_root, "wps", "iPone.sbs"), "%wd\n%xl(Bg,iPone_bd.bmp)\n")
    _write_text(os.path.join(repo_root, "wps", "iPone.fms"), "%wd\n%xl(Lockscreen,Wallpaper.bmp)\n")
    _write_bmp(os.path.join(repo_root, "backdrops", "iPone_bd.bmp"), 320, 240, "#111111")
    _write_bmp(os.path.join(repo_root, "icons", "iPone.bmp"), 16, 16, "#999999")
    _write_text(os.path.join(repo_root, "fonts", "24 iLike.fnt"), "font24\n")
    _write_text(os.path.join(repo_root, "fonts", "18-Cantarell-Regular.fnt"), "font18\n")
    for name in (
        "Wallpaper.bmp",
        "WallpaperAlt.bmp",
        "WallpaperThird.bmp",
        "WallpaperFourth.bmp",
        "WallpaperFifth.bmp",
        "WallpaperSixth.bmp",
        "iPone_bg.bmp",
        "iPone_bd.bmp",
        "SbsBackdrop.bmp",
        "ChargeWallpaper.bmp",
        "ChargeWallpaperAlt.bmp",
        "ChargeWallpaperThird.bmp",
        "ChargeWallpaperFourth.bmp",
    ):
        _write_bmp(os.path.join(repo_root, "wps", "iPone", name), 320, 240, "#222222")

    _write_text(
        os.path.join(repo_root, "themes", "iPone_nano2g.cfg"),
        "\n".join(
            [
                "wps: /.rockbox/wps/iPone_nano2g.wps",
                "sbs: /.rockbox/wps/iPone_nano2g.sbs",
                "fms: /.rockbox/wps/iPone_nano2g.fms",
                "background color: 0F0F0F",
                "foreground color: F2F2F2",
                "font: /.rockbox/fonts/12-Adobe-Helvetica.fnt",
                "line selector start color: 303030",
                "line selector end color: 909090",
                "line selector text color: FFFFFF",
                "list separator color: 202020",
                "",
            ]
        ),
    )
    _write_text(os.path.join(repo_root, "wps", "iPone_nano2g.wps"), "%wd\n")
    _write_text(os.path.join(repo_root, "wps", "iPone_nano2g.sbs"), "%wd\n")
    _write_text(os.path.join(repo_root, "wps", "iPone_nano2g.fms"), "%wd\n")
    _write_text(os.path.join(repo_root, "fonts", "12-Adobe-Helvetica.fnt"), "font12\n")
    _write_bmp(os.path.join(repo_root, "icons", "tango_icons.12x12.bmp"), 12, 12, "#888888")
    for name in (
        "Wallpaper.bmp",
        "WallpaperAlt.bmp",
        "ChargeWallpaper.bmp",
        "ChargeWallpaperAlt.bmp",
        "ChargeWallpaperThird.bmp",
        "ChargeWallpaperFourth.bmp",
        "wpsbackdrop-176x132x16.bmp",
        "BootLogo.bmp",
    ):
        _write_bmp(os.path.join(repo_root, "wps", "iPone_nano2g", name), 176, 132, "#333333")


def _profile(repo_root, resolution="320x240"):
    return {
        "id": f"profile-{resolution}",
        "name": f"Profile {resolution}",
        "screen_resolution": resolution,
        "source_repo_path": repo_root,
        "device_mount_path": "",
        "backup_location": os.path.join(repo_root, ".backups", resolution),
        "selected_theme": "iPone" if resolution == "320x240" else "iPone_nano2g",
    }


def test_loads_ipone_base_theme(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()

    variant = service.load_base_theme(repo_root, _profile(repo_root, "320x240"))

    assert variant["base_theme_id"] == "iPone"
    assert variant["font_rel"] == "fonts/24 iLike.fnt"
    assert variant["colors"]["foreground"] == "F7F4FA"


def test_generates_and_persists_custom_variant(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()

    variant = service.new_variant(repo_root, _profile(repo_root), "Midnight Glass")
    variant["font_rel"] = "fonts/18-Cantarell-Regular.fnt"
    variant["colors"]["selector_end"] = "ABCDEF"
    saved = service.save_variant(repo_root, variant)

    loaded = service.load_variant(repo_root, saved["id"])
    assert loaded["name"] == "Midnight Glass"
    assert loaded["font_rel"] == "fonts/18-Cantarell-Regular.fnt"
    assert loaded["colors"]["selector_end"] == "ABCDEF"


def test_wallpaper_conversion_respects_profile_resolution(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    source = os.path.join(tmp_dir, "wallpaper.png")
    Image.new("RGB", (800, 600), "#ff6600").save(source, "PNG")
    service = ThemeDesignerService()

    variant = service.new_variant(repo_root, _profile(repo_root), "Orange")
    variant["wallpaper_source"] = source
    saved = service.save_variant(repo_root, variant)
    bundle = service.build_bundle(repo_root, _profile(repo_root), saved)
    wallpaper = next(
        item["source_abs"] for item in bundle["assets"]
        if item["destination_rel"].endswith(f"/wps/{saved['id']}/Wallpaper.bmp")
    )

    with Image.open(wallpaper) as rendered:
        assert rendered.size == (320, 240)


def test_charging_wallpaper_generation_uses_separate_source(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    main = os.path.join(tmp_dir, "main.png")
    charging = os.path.join(tmp_dir, "charging.png")
    Image.new("RGB", (640, 640), "#00aa66").save(main, "PNG")
    Image.new("RGB", (640, 640), "#0033cc").save(charging, "PNG")
    service = ThemeDesignerService()

    variant = service.new_variant(repo_root, _profile(repo_root), "Dual")
    variant["wallpaper_source"] = main
    variant["charging_wallpaper_source"] = charging
    saved = service.save_variant(repo_root, variant)
    bundle = service.build_bundle(repo_root, _profile(repo_root), saved)
    charge = next(
        item["source_abs"] for item in bundle["assets"]
        if item["destination_rel"].endswith(f"/wps/{saved['id']}/ChargeWallpaper.bmp")
    )

    with Image.open(charge) as rendered:
        assert rendered.size == (320, 240)
        assert rendered.getpixel((10, 10))[2] > rendered.getpixel((10, 10))[1]


def test_preview_state_generation_is_profile_aware(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()

    variant = service.new_variant(repo_root, _profile(repo_root, "176x132"), "Nano")
    preview = service.build_preview_state(repo_root, _profile(repo_root, "176x132"), variant)

    assert preview["base_theme_id"] == "iPone_nano2g"
    assert preview["width"] == 176
    assert preview["height"] == 132
    assert preview["menu_backdrop_path"].endswith("wps/iPone_nano2g/wpsbackdrop-176x132x16.bmp")
    assert preview["preview_modes"] == ["simulator"]


def test_generated_bundle_deploys_with_existing_deploy_flow(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    device_root = os.path.join(tmp_dir, "device")
    os.makedirs(device_root, exist_ok=True)
    service = ThemeDesignerService()
    deploy = RockboxDeployService()
    profile = _profile(repo_root)
    profile["device_mount_path"] = device_root

    variant = service.new_variant(repo_root, profile, "Deployable")
    saved = service.save_variant(repo_root, variant)
    bundle = service.build_bundle(repo_root, profile, saved)
    diff = deploy.build_diff(profile, bundle)
    result = deploy.apply_diff(profile, diff)

    assert result["success"] is True
    assert os.path.isfile(os.path.join(device_root, ".rockbox", "themes", f"{saved['id']}.cfg"))
    assert os.path.isfile(os.path.join(device_root, ".rockbox", "wps", f"{saved['id']}.wps"))
    assert os.path.isfile(os.path.join(device_root, ".rockbox", "rockpod", "theme_designer", f"{saved['id']}.json"))


def test_preserves_base_theme_files_when_variant_is_generated(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()
    source = os.path.join(tmp_dir, "purple.png")
    Image.new("RGB", (900, 500), "#8844ff").save(source, "PNG")
    base_wallpaper = os.path.join(repo_root, "wps", "iPone", "Wallpaper.bmp")
    with open(base_wallpaper, "rb") as handle:
        before = handle.read()

    variant = service.new_variant(repo_root, _profile(repo_root), "NoOverwrite")
    variant["wallpaper_source"] = source
    saved = service.save_variant(repo_root, variant)
    service.build_bundle(repo_root, _profile(repo_root), saved)

    with open(base_wallpaper, "rb") as handle:
        after = handle.read()
    assert before == after


def test_preview_bundle_can_rebuild_without_reusing_the_same_stage_dir(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()
    profile = _profile(repo_root)

    variant = service.new_variant(repo_root, profile, "Preview")
    first = service.build_preview_bundle(repo_root, profile, variant)
    second = service.build_preview_bundle(repo_root, profile, variant)

    first_cfg = next(item for item in first["assets"] if item["kind"] == "cfg")["source_abs"]
    second_cfg = next(item for item in second["assets"] if item["kind"] == "cfg")["source_abs"]

    assert first["id"] == second["id"]
    assert first_cfg != second_cfg
    assert os.path.isfile(first_cfg)
    assert os.path.isfile(second_cfg)


def test_preview_bundle_applies_unsaved_colors_to_generated_assets_and_templates(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()
    profile = _profile(repo_root)

    variant = service.new_variant(repo_root, profile, "Preview Colors")
    variant["colors"]["background"] = "224466"
    variant["colors"]["foreground"] = "EEEEDD"
    variant["colors"]["selector_start"] = "335577"
    variant["colors"]["selector_end"] = "88AA44"
    variant["colors"]["selector_text"] = "FFF199"

    bundle = service.build_preview_bundle(repo_root, profile, variant)
    wallpaper = next(
        item["source_abs"]
        for item in bundle["assets"]
        if item["kind"] == "wps_assets" and item["source_rel"].endswith("/Wallpaper.bmp")
    )
    cfg = next(item["source_abs"] for item in bundle["assets"] if item["kind"] == "cfg")

    with Image.open(wallpaper) as rendered:
        assert rendered.getpixel((10, 10)) == (0x22, 0x44, 0x66)

    with open(cfg, "r", encoding="utf-8") as handle:
        content = handle.read()
    assert "224466" in content
    assert "EEEEDD" in content
    assert "88AA44" in content
    assert "FFF199" in content


def test_wayland_simulator_mode_uses_full_preview_surface(tmp_dir, monkeypatch):
    app = QApplication.instance() or QApplication([])
    monkeypatch.setenv("XDG_SESSION_TYPE", "wayland")
    monkeypatch.setattr("ui.theme_designer.shutil.which", lambda name: "/usr/bin/xdotool" if name == "xdotool" else None)
    shot = os.path.join(tmp_dir, "preview.bmp")
    _write_bmp(shot, 320, 240, "#6633CC")
    widget = ThemeDesignerWidget()
    widget._resolution_label.setText("320x240")
    widget.set_simulator_preview(shot)
    widget._preview_mode.setCurrentIndex(widget._preview_mode.findData("simulator"))

    assert widget._preview_stack.currentWidget() is widget._preview
    assert widget._base_simulator_preview_path == shot
    assert widget._preview._state["simulator_preview_path"] == shot
    widget.deleteLater()


def test_x11_simulator_mode_uses_baked_in_preview_surface(tmp_dir, monkeypatch):
    app = QApplication.instance() or QApplication([])
    monkeypatch.setenv("XDG_SESSION_TYPE", "x11")
    monkeypatch.setattr("ui.theme_designer.shutil.which", lambda name: "/usr/bin/xdotool" if name == "xdotool" else None)
    widget = ThemeDesignerWidget()
    widget._resolution_label.setText("320x240")
    widget._preview_mode.setCurrentIndex(widget._preview_mode.findData("simulator"))

    assert widget._preview_stack.currentWidget() is widget._preview
    widget.deleteLater()


def test_simulator_mode_ignores_target_build_dir_fallback(tmp_dir, monkeypatch):
    app = QApplication.instance() or QApplication([])
    monkeypatch.setenv("XDG_SESSION_TYPE", "wayland")
    monkeypatch.setattr("ui.theme_designer.shutil.which", lambda name: "/usr/bin/xdotool" if name == "xdotool" else None)
    build_dir = os.path.join(tmp_dir, "build-sim-video-5g")
    simdisk = os.path.join(build_dir, "simdisk")
    os.makedirs(simdisk, exist_ok=True)
    binary = os.path.join(build_dir, "rockboxui")
    _write_text(binary, "#!/bin/sh\n")
    os.chmod(binary, 0o755)
    shot = os.path.join(build_dir, "1UI256.bmp")
    _write_bmp(shot, 320, 240, "#5522AA")

    widget = ThemeDesignerWidget()
    widget._resolution_label.setText("320x240")
    widget.set_simulator_target(binary, simdisk)
    widget._preview_mode.setCurrentIndex(widget._preview_mode.findData("simulator"))

    assert widget._preview._state["simulator_preview_path"] == ""
    widget.deleteLater()


def test_simulator_mode_uses_explicit_snapshot_path_over_live_preview_dump(tmp_dir, monkeypatch):
    app = QApplication.instance() or QApplication([])
    monkeypatch.setenv("XDG_SESSION_TYPE", "wayland")
    monkeypatch.setattr("ui.theme_designer.shutil.which", lambda name: "/usr/bin/xdotool" if name == "xdotool" else None)
    build_dir = os.path.join(tmp_dir, "build-sim-video-5g")
    simdisk = os.path.join(build_dir, "simdisk")
    rockbox_dir = os.path.join(simdisk, ".rockbox")
    os.makedirs(rockbox_dir, exist_ok=True)
    binary = os.path.join(build_dir, "rockboxui")
    _write_text(binary, "#!/bin/sh\n")
    os.chmod(binary, 0o755)
    fallback = os.path.join(build_dir, "1UI256.bmp")
    live = os.path.join(rockbox_dir, "live_preview.bmp")
    _write_bmp(fallback, 320, 240, "#5522AA")
    _write_bmp(live, 320, 240, "#22AA55")

    widget = ThemeDesignerWidget()
    widget._resolution_label.setText("320x240")
    widget.set_simulator_target(binary, simdisk)
    widget.set_simulator_preview(fallback)
    widget._preview_mode.setCurrentIndex(widget._preview_mode.findData("simulator"))

    assert widget._preview._state["simulator_preview_path"] == fallback
    widget.deleteLater()


def test_theme_designer_does_not_launch_live_simulator_on_set_target(tmp_dir, monkeypatch):
    app = QApplication.instance() or QApplication([])
    monkeypatch.setenv("XDG_SESSION_TYPE", "x11")
    monkeypatch.setattr("ui.theme_designer.shutil.which", lambda name: "/usr/bin/xdotool" if name == "xdotool" else None)
    build_dir = os.path.join(tmp_dir, "build-sim-video-5g")
    simdisk = os.path.join(build_dir, "simdisk")
    os.makedirs(simdisk, exist_ok=True)
    binary = os.path.join(build_dir, "rockboxui")
    _write_text(binary, "#!/bin/sh\n")
    os.chmod(binary, 0o755)

    launched = {}

    class _Env:
        def __init__(self):
            self.values = {}

        def insert(self, key, value):
            self.values[key] = value

    class _Proc:
        def __init__(self, *args, **kwargs):
            launched["args"] = args
            launched["kwargs"] = kwargs
            self._state = 1
            self._env = _Env()

        def setWorkingDirectory(self, _value):
            pass

        def processEnvironment(self):
            return self._env

        def setProcessEnvironment(self, _value):
            launched["env"] = dict(self._env.values)

        def setProgram(self, program):
            launched["program"] = program

        def setArguments(self, arguments):
            launched["arguments"] = arguments

        def start(self):
            pass

        def state(self):
            return self._state

        def processId(self):
            return 1234

        errorOccurred = type("Sig", (), {"connect": lambda self, fn: None})()
        finished = type("Sig", (), {"connect": lambda self, fn: None})()

    monkeypatch.setattr("ui.theme_designer.QProcess", _Proc)
    monkeypatch.setattr("ui.theme_designer.QTimer.singleShot", lambda *_args, **_kwargs: None)

    widget = ThemeDesignerWidget()
    widget.set_simulator_target(binary, simdisk)
    widget.restart_simulator_preview()

    assert launched == {}
    assert widget._sim_menu_btn.isEnabled() is False
    widget.deleteLater()


def test_wayland_preview_buttons_are_disabled_in_manual_snapshot_mode(tmp_dir, monkeypatch):
    app = QApplication.instance() or QApplication([])
    monkeypatch.setenv("XDG_SESSION_TYPE", "wayland")
    monkeypatch.setattr("ui.theme_designer.shutil.which", lambda name: "/usr/bin/xdotool" if name == "xdotool" else None)
    build_dir = os.path.join(tmp_dir, "build-sim-video-5g")
    simdisk = os.path.join(build_dir, "simdisk")
    os.makedirs(simdisk, exist_ok=True)
    binary = os.path.join(build_dir, "rockboxui")
    _write_text(binary, "#!/bin/sh\n")
    os.chmod(binary, 0o755)

    launched = {}

    class _Env:
        def __init__(self):
            self.values = {}

        def insert(self, key, value):
            self.values[key] = value

    class _Proc:
        NotRunning = 0

        def __init__(self, *args, **kwargs):
            self._state = 1
            self._env = _Env()

        def setWorkingDirectory(self, _value):
            pass

        def processEnvironment(self):
            return self._env

        def setProcessEnvironment(self, _value):
            launched["env"] = dict(self._env.values)

        def setProgram(self, program):
            launched["program"] = program

        def setArguments(self, arguments):
            launched["arguments"] = arguments

        def start(self):
            launched["started"] = True

        def state(self):
            return self._state

        def processId(self):
            return 4321

        errorOccurred = type("Sig", (), {"connect": lambda self, fn: None})()
        finished = type("Sig", (), {"connect": lambda self, fn: None})()

    monkeypatch.setattr("ui.theme_designer.QProcess", _Proc)

    widget = ThemeDesignerWidget()
    widget.set_simulator_target(binary, simdisk)
    assert launched == {}
    assert widget._sim_play_btn.isEnabled() is False
    assert widget._sim_select_btn.isEnabled() is False
    widget.deleteLater()


def test_x11_simulator_mode_prefers_embedded_surface_when_xdotool_exists(monkeypatch):
    app = QApplication.instance() or QApplication([])
    monkeypatch.setenv("XDG_SESSION_TYPE", "x11")
    monkeypatch.setattr("ui.theme_designer.shutil.which", lambda name: "/usr/bin/xdotool" if name == "xdotool" else None)

    widget = ThemeDesignerWidget()
    widget._resolution_label.setText("320x240")
    widget._preview_mode.setCurrentIndex(widget._preview_mode.findData("simulator"))

    assert widget._preview_stack.currentWidget() is widget._preview
    assert widget.current_variant_data()["preview_screen"] == "wps"
    widget.deleteLater()
