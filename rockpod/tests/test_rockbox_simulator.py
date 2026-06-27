import os
import shutil
import subprocess

from app.config import Config
from services.rockbox_deploy import RockboxDeployService
from services.rockbox_profiles import RockboxProfileStore
from services.rockbox_simulator import RockboxSimulatorService
from services.rockbox_themes import RockboxThemeService


def _temp_names(path):
    return [name for name in os.listdir(path) if name.startswith("tmp")]


class _FakeCommandResult:
    def __init__(self, returncode=0, stdout="", stderr=""):
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr
        self.log_path = ""


class _FakeCommandRunner:
    def __init__(self, results):
        self.results = list(results)
        self.commands = []

    def run(self, command, cwd="", timeout=None, env=None):
        self.commands.append([str(part) for part in command])
        if self.results:
            return self.results.pop(0)
        return _FakeCommandResult()


def _make_file(path, content=b"x"):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    mode = "wb" if isinstance(content, bytes) else "w"
    with open(path, mode) as handle:
        handle.write(content)


def _make_repo_with_ipone_assets(repo_root):
    _make_file(os.path.join(repo_root, "themes", "iPone.cfg"), "theme\n")
    _make_file(os.path.join(repo_root, "wps", "iPone.wps"), "wps\n")
    _make_file(os.path.join(repo_root, "wps", "iPone.sbs"), "sbs\n")
    _make_file(os.path.join(repo_root, "wps", "iPone.fms"), "fms\n")
    _make_file(os.path.join(repo_root, "backdrops", "iPone_bd.bmp"))
    _make_file(os.path.join(repo_root, "icons", "iPone.bmp"))
    _make_file(os.path.join(repo_root, "fonts", "24 iLike.fnt"), "font\n")
    _make_file(os.path.join(repo_root, "wps", "iPone", "Wallpaper.bmp"))


def _make_store(tmp_dir, repo_root):
    config = Config(os.path.join(tmp_dir, "sim-config.json"))
    return config, RockboxProfileStore(config, repo_root)


def _make_sim_target(repo_root, name="build-sim-video-5g"):
    build_dir = os.path.join(repo_root, name)
    simdisk = os.path.join(build_dir, "simdisk")
    _make_file(os.path.join(build_dir, "rockboxui"), b"#!/bin/sh\n")
    os.makedirs(simdisk, exist_ok=True)
    return build_dir, simdisk


def test_simulator_target_discovery(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    build_dir, simdisk = _make_sim_target(repo_root, "build-sim-video-5g")
    _make_sim_target(repo_root, "build-sim-3g")
    _make_sim_target(repo_root, "build-sim-nano2g")
    os.makedirs(os.path.join(repo_root, "build-sim-bad"), exist_ok=True)

    service = RockboxSimulatorService()
    targets = service.discover_targets(repo_root)

    ids = {item["id"] for item in targets}
    assert ids == {"build-sim-3g", "build-sim-nano2g", "build-sim-video-5g"}
    video = next(item for item in targets if item["id"] == "build-sim-video-5g")
    assert video["binary_path"] == os.path.join(build_dir, "rockboxui")
    assert video["simdisk_path"] == simdisk
    assert video["rockbox_root"] == os.path.join(simdisk, ".rockbox")
    assert video["screen_resolution"] == "320x240"
    classic3g = next(item for item in targets if item["id"] == "build-sim-3g")
    assert classic3g["screen_resolution"] == "160x128"
    assert classic3g["device_model"] == "iPod 3G"
    nano = next(item for item in targets if item["id"] == "build-sim-nano2g")
    assert nano["screen_resolution"] == "176x132"


def test_fallback_ipodvideo_target_uses_local_video_sim_build(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    build_dir, simdisk = _make_sim_target(repo_root)
    service = RockboxSimulatorService()

    target = service.fallback_ipodvideo_target(
        {
            "source_repo_path": repo_root,
            "screen_resolution": "320x240",
        }
    )

    assert target["id"] == "build-sim-video-5g"
    assert target["build_dir"] == build_dir
    assert target["binary_path"] == os.path.join(build_dir, "rockboxui")
    assert target["simdisk_path"] == simdisk
    assert target["rockbox_root"] == os.path.join(simdisk, ".rockbox")
    assert target["device_model"] == "iPod Classic / Video"
    assert service.fallback_ipodvideo_target(
        {"source_repo_path": repo_root, "screen_resolution": "176x132"}
    ) is None


def test_profile_simulator_binding_persists(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    build_dir, simdisk = _make_sim_target(repo_root)
    config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile = store.save_profile(profile)
    target = {
        "id": "build-sim-video-5g",
        "binary_path": os.path.join(build_dir, "rockboxui"),
        "simdisk_path": simdisk,
    }

    service = RockboxSimulatorService()
    updated = service.bind_profile(profile, target)
    store.save_profile(updated)

    reloaded = RockboxProfileStore(Config(config._path), repo_root)
    current = reloaded.current_profile()
    assert current["simulator_target"] == "build-sim-video-5g"
    assert current["simulator_binary_path"] == os.path.join(build_dir, "rockboxui")
    assert current["simulator_simdisk_path"] == simdisk
    assert current["simulator_screenshot_dir"] == os.path.join(repo_root, "simshots")


def test_theme_designer_preview_target_reset_rebuilds_clean_simdisk(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    build_dir, simdisk = _make_sim_target(repo_root)
    _make_file(os.path.join(simdisk, ".rockbox", "config.cfg"), "base\n")
    profile = {"id": "profile-320x240", "source_repo_path": repo_root}
    target = {
        "id": "build-sim-video-5g",
        "name": "build-sim-video-5g",
        "build_dir": build_dir,
        "binary_path": os.path.join(build_dir, "rockboxui"),
        "simdisk_path": simdisk,
    }

    service = RockboxSimulatorService()
    preview = service.theme_designer_preview_target(profile, target)
    dirty_path = os.path.join(preview["simdisk_path"], ".rockbox", "dirty.txt")
    _make_file(dirty_path, "dirty\n")

    rebuilt = service.theme_designer_preview_target(profile, target, reset=True)

    assert rebuilt["simdisk_path"] == preview["simdisk_path"]
    assert not os.path.exists(dirty_path)
    with open(os.path.join(rebuilt["simdisk_path"], ".rockbox", "config.cfg"), "r", encoding="utf-8") as handle:
        assert handle.read() == "base\n"
    assert not _temp_names(os.path.join(rebuilt["simdisk_path"], ".rockbox"))


def test_simdisk_deploy_diff_and_apply_only_touch_rockbox_paths(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo_with_ipone_assets(repo_root)
    _build_dir, simdisk = _make_sim_target(repo_root)
    _make_file(os.path.join(simdisk, "notes.txt"), "leave alone\n")

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["selected_theme"] = "iPone"
    profile["screen_resolution"] = "320x240"
    profile["simulator_target"] = "build-sim-video-5g"
    profile["simulator_simdisk_path"] = simdisk
    profile = store.save_profile(profile)
    target = {
        "id": "build-sim-video-5g",
        "name": "build-sim-video-5g",
        "build_dir": os.path.join(repo_root, "build-sim-video-5g"),
        "binary_path": os.path.join(repo_root, "build-sim-video-5g", "rockboxui"),
        "simdisk_path": simdisk,
        "screen_resolution": "320x240",
        "device_model": "iPod Classic / Video",
    }

    theme_service = RockboxThemeService()
    simulator_service = RockboxSimulatorService()
    deploy_service = RockboxDeployService()
    bundle = theme_service.bundle_for_theme("iPone", repo_root)
    sim_profile = simulator_service.simulator_profile(profile, target)

    diff = deploy_service.build_diff(sim_profile, bundle)
    assert diff["summary"]["add"] > 0
    assert all(item["destination_rel"].startswith(".rockbox/") for item in diff["items"])
    assert all(item["destination_abs"].startswith(os.path.join(simdisk, ".rockbox")) for item in diff["items"])

    result = deploy_service.apply_diff(sim_profile, diff)
    assert result["success"] is True

    with open(os.path.join(simdisk, "notes.txt"), "r", encoding="utf-8") as handle:
        assert handle.read() == "leave alone\n"
    assert os.path.isfile(os.path.join(simdisk, ".rockbox", "themes", "iPone.cfg"))
    assert os.path.isfile(os.path.join(simdisk, ".rockbox", "wps", "iPone.wps"))


def test_simulator_rollback_restores_prior_files(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo_with_ipone_assets(repo_root)
    _build_dir, simdisk = _make_sim_target(repo_root)
    _make_file(os.path.join(simdisk, ".rockbox", "themes", "iPone.cfg"), "old theme\n")
    _make_file(os.path.join(simdisk, "keep.txt"), "leave alone\n")

    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["selected_theme"] = "iPone"
    profile["screen_resolution"] = "320x240"
    profile["simulator_target"] = "build-sim-video-5g"
    profile["simulator_simdisk_path"] = simdisk
    profile = store.save_profile(profile)
    target = {
        "id": "build-sim-video-5g",
        "name": "build-sim-video-5g",
        "build_dir": os.path.join(repo_root, "build-sim-video-5g"),
        "binary_path": os.path.join(repo_root, "build-sim-video-5g", "rockboxui"),
        "simdisk_path": simdisk,
        "screen_resolution": "320x240",
        "device_model": "iPod Classic / Video",
    }

    theme_service = RockboxThemeService()
    simulator_service = RockboxSimulatorService()
    deploy_service = RockboxDeployService()
    bundle = theme_service.bundle_for_theme("iPone", repo_root)
    sim_profile = simulator_service.simulator_profile(profile, target)
    diff = deploy_service.build_diff(sim_profile, bundle)
    assert any(
        item["destination_rel"] == ".rockbox/themes/iPone.cfg" and item["status"] == "overwrite"
        for item in diff["items"]
    )

    result = deploy_service.apply_diff(sim_profile, diff)
    assert result["success"] is True
    restore = deploy_service.restore_latest_backup(sim_profile)
    assert restore["success"] is True

    with open(os.path.join(simdisk, ".rockbox", "themes", "iPone.cfg"), "r", encoding="utf-8") as handle:
        assert handle.read() == "old theme\n"
    with open(os.path.join(simdisk, "keep.txt"), "r", encoding="utf-8") as handle:
        assert handle.read() == "leave alone\n"


def test_screenshot_path_handling_and_capture(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    build_dir, simdisk = _make_sim_target(repo_root)
    dump_path = os.path.join(simdisk, "dump 0001.bmp")
    _make_file(dump_path, b"BMFAKE")

    service = RockboxSimulatorService()
    profile = {
        "id": "ipod_320x240",
        "name": "iPod Classic / Video",
        "source_repo_path": repo_root,
        "simulator_target": "build-sim-video-5g",
        "simulator_screenshot_dir": os.path.join(repo_root, "simshots"),
    }
    target = {
        "id": "build-sim-video-5g",
        "build_dir": build_dir,
        "simdisk_path": simdisk,
    }

    shots_dir = service.ensure_screenshot_dir(profile, target)
    assert shots_dir == os.path.join(repo_root, "simshots", "build-sim-video-5g")
    assert os.path.isdir(shots_dir)

    result = service.capture_screenshot(profile, target)
    assert result["success"] is True
    assert result["source_path"] == dump_path
    assert result["captured_path"].startswith(shots_dir)
    assert os.path.isfile(result["captured_path"])


def test_latest_screenshot_prefers_build_ui_capture(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    build_dir, simdisk = _make_sim_target(repo_root)
    ui_capture = os.path.join(build_dir, "1UI256.bmp")
    _make_file(ui_capture, b"BMFAKEUI")
    shots_dir = os.path.join(repo_root, "simshots", "build-sim-video-5g")
    _make_file(os.path.join(shots_dir, "20260420-dump_0001.bmp"), b"BMOLDER")

    service = RockboxSimulatorService()
    profile = {
        "id": "ipod_320x240",
        "name": "iPod Classic / Video",
        "source_repo_path": repo_root,
        "simulator_target": "build-sim-video-5g",
        "simulator_screenshot_dir": os.path.join(repo_root, "simshots"),
    }
    target = {
        "id": "build-sim-video-5g",
        "build_dir": build_dir,
        "simdisk_path": simdisk,
    }

    assert service.latest_screenshot(profile, target) == ui_capture


def test_latest_screenshot_skips_build_ui_capture_for_theme_designer_target(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    build_dir, simdisk = _make_sim_target(repo_root)
    ui_capture = os.path.join(build_dir, "1UI256.bmp")
    _make_file(ui_capture, b"BMFAKEUI")
    live_preview = os.path.join(simdisk, ".rockbox", "live_preview.bmp")
    _make_file(live_preview, b"BMLIVE")

    service = RockboxSimulatorService()
    profile = {
        "id": "ipod_320x240",
        "name": "iPod Classic / Video",
        "source_repo_path": repo_root,
        "simulator_target": "build-sim-video-5g",
        "simulator_screenshot_dir": os.path.join(repo_root, "simshots"),
    }
    target = {
        "id": "build-sim-video-5g-theme-designer",
        "build_dir": build_dir,
        "simdisk_path": simdisk,
    }

    assert service.latest_screenshot(profile, target) == live_preview


def test_latest_screenshot_prefers_theme_designer_snapshot_capture(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    build_dir, simdisk = _make_sim_target(repo_root)
    preview_root = os.path.join(tmp_dir, "preview-root")
    capture_dir = os.path.join(preview_root, "captures")
    os.makedirs(capture_dir, exist_ok=True)
    live_preview = os.path.join(simdisk, ".rockbox", "live_preview.bmp")
    snapshot = os.path.join(capture_dir, "preview-20260420-000000.bmp")
    _make_file(live_preview, b"BMLIVE")
    _make_file(snapshot, b"BMSNAP")

    service = RockboxSimulatorService()
    profile = {
        "id": "ipod_320x240",
        "name": "iPod Classic / Video",
        "source_repo_path": repo_root,
        "simulator_target": "build-sim-video-5g",
        "simulator_screenshot_dir": os.path.join(repo_root, "simshots"),
    }
    target = {
        "id": "build-sim-video-5g-theme-designer",
        "build_dir": build_dir,
        "simdisk_path": simdisk,
        "preview_root": preview_root,
    }

    assert service.latest_screenshot(profile, target) == snapshot


def test_latest_screenshot_ignores_stale_screenshot_dir_for_theme_designer_target(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    build_dir, simdisk = _make_sim_target(repo_root)
    shots_dir = os.path.join(repo_root, "simshots", "build-sim-video-5g")
    _make_file(os.path.join(shots_dir, "20260420-dump_0001.bmp"), b"BMOLDER")

    service = RockboxSimulatorService()
    profile = {
        "id": "ipod_320x240",
        "name": "iPod Classic / Video",
        "source_repo_path": repo_root,
        "simulator_target": "build-sim-video-5g",
        "simulator_screenshot_dir": os.path.join(repo_root, "simshots"),
    }
    target = {
        "id": "build-sim-video-5g-theme-designer",
        "build_dir": build_dir,
        "simdisk_path": simdisk,
    }

    assert service.latest_screenshot(profile, target) == ""


def test_capture_theme_preview_sequence_records_default_ipone_screens(tmp_dir):
    service = RockboxSimulatorService()
    target = {"id": "build-sim-video-5g-theme-designer"}
    calls = []

    def fake_capture(_target, preview_screen="wps", timeout=6.0):
        calls.append((preview_screen, timeout))
        path = os.path.join(tmp_dir, f"{preview_screen}.bmp")
        _make_file(path, b"BMFAKE")
        return path

    service.capture_theme_preview = fake_capture

    results = service.capture_theme_preview_sequence(target, timeout=1.5)

    assert [item["screen"] for item in results] == ["sbs", "wps", "lockscreen"]
    assert [item[0] for item in calls] == ["sbs", "wps", "lockscreen"]
    assert all(item["success"] for item in results)
    assert all(item["captured_path"].endswith(f"{item['screen']}.bmp") for item in results)
    assert {item[1] for item in calls} == {1.5}


def test_capture_theme_preview_sequence_filters_blank_custom_screens(tmp_dir):
    service = RockboxSimulatorService()
    calls = []

    def fake_capture(_target, preview_screen="wps", timeout=6.0):
        calls.append(preview_screen)
        if preview_screen == "wps":
            path = os.path.join(tmp_dir, "wps.bmp")
            _make_file(path, b"BMFAKE")
            return path
        return ""

    service.capture_theme_preview = fake_capture

    results = service.capture_theme_preview_sequence({}, screens=["", "wps", "missing"])

    assert calls == ["wps", "missing"]
    assert results[0]["success"] is True
    assert results[1] == {"screen": "missing", "captured_path": "", "success": False}


def test_xdotool_helpers_use_shared_command_runner(monkeypatch):
    monkeypatch.setattr(
        "services.rockbox_simulator.shutil.which",
        lambda name: "/usr/bin/xdotool" if name == "xdotool" else None,
    )
    runner = _FakeCommandRunner([
        _FakeCommandResult(stdout="123\n"),
        _FakeCommandResult(returncode=0),
        _FakeCommandResult(returncode=0),
    ])

    window_id = RockboxSimulatorService._wait_for_window_id(77, timeout=0.2, command_runner=runner)
    sent = RockboxSimulatorService._send_key_to_window_id(window_id, "F5", command_runner=runner)
    moved = RockboxSimulatorService._move_window_offscreen(window_id, command_runner=runner)

    assert window_id == "123"
    assert sent is True
    assert moved is True
    assert runner.commands == [
        ["xdotool", "search", "--pid", "77"],
        ["xdotool", "key", "--window", "123", "F5"],
        ["xdotool", "windowmove", "123", "5000", "5000"],
    ]


def test_activate_theme_preview_updates_preview_config_files(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _build_dir, simdisk = _make_sim_target(repo_root)
    preview_root = os.path.join(tmp_dir, "preview-root")
    theme_cfg = os.path.join(simdisk, ".rockbox", "themes", "preview.cfg")
    _make_file(
        theme_cfg,
        "\n".join(
            [
                "wps: /.rockbox/wps/preview.wps",
                "sbs: /.rockbox/wps/preview.sbs",
                "fms: /.rockbox/wps/preview.fms",
                "backdrop: /.rockbox/backdrops/preview_bd.bmp",
                "font: /.rockbox/fonts/preview.fnt",
                "background color: 112233",
                "",
            ]
        ),
    )
    existing = "volume: 3\nwps: /.rockbox/wps/iPone.wps\n"
    _make_file(os.path.join(preview_root, ".config", "rockbox.org", "config.cfg"), existing)
    _make_file(os.path.join(simdisk, ".rockbox", "config.cfg"), existing)
    _make_file(os.path.join(simdisk, "config.cfg"), existing)
    for stale in (
        os.path.join(preview_root, ".config", "rockbox.org", ".resume.cfg"),
        os.path.join(preview_root, ".config", "rockbox.org", ".resume.cfg.new"),
        os.path.join(preview_root, ".config", "rockbox.org", ".resume.cfg.old"),
        os.path.join(simdisk, ".rockbox", ".resume.cfg"),
        os.path.join(simdisk, ".rockbox", ".resume.cfg.new"),
        os.path.join(simdisk, ".rockbox", ".resume.cfg.old"),
        os.path.join(simdisk, ".resume.cfg"),
        os.path.join(simdisk, ".resume.cfg.new"),
        os.path.join(simdisk, ".resume.cfg.old"),
    ):
        _make_file(stale, "resume\n")

    service = RockboxSimulatorService()
    target = {"simdisk_path": simdisk, "preview_root": preview_root}

    assert service.activate_theme_preview(target, "preview", preview_screen="sbs") is True

    for path in (
        os.path.join(preview_root, ".config", "rockbox.org", "config.cfg"),
        os.path.join(simdisk, ".rockbox", "config.cfg"),
        os.path.join(simdisk, "config.cfg"),
    ):
        with open(path, "r", encoding="utf-8") as handle:
            text = handle.read()
        assert "volume: 3" not in text
        assert not _temp_names(os.path.dirname(path))
        assert "theme: /.rockbox/themes/preview.cfg" in text
        assert "start in screen: root" in text
        assert "tagcache_autoupdate: off" in text
        assert "wps: /.rockbox/wps/preview.wps" in text
        assert "sbs: /.rockbox/wps/preview.sbs" in text
        assert "font: /.rockbox/fonts/preview.fnt" in text
        if path.endswith(".rockbox/config.cfg"):
            assert os.path.exists(os.path.join(os.path.dirname(path), "database.ignore"))

    for stale in (
        os.path.join(preview_root, ".config", "rockbox.org", ".resume.cfg"),
        os.path.join(preview_root, ".config", "rockbox.org", ".resume.cfg.new"),
        os.path.join(preview_root, ".config", "rockbox.org", ".resume.cfg.old"),
        os.path.join(simdisk, ".rockbox", ".resume.cfg"),
        os.path.join(simdisk, ".rockbox", ".resume.cfg.new"),
        os.path.join(simdisk, ".rockbox", ".resume.cfg.old"),
        os.path.join(simdisk, ".resume.cfg"),
        os.path.join(simdisk, ".resume.cfg.new"),
        os.path.join(simdisk, ".resume.cfg.old"),
    ):
        assert not os.path.exists(stale)


def test_activate_theme_preview_seeds_wps_runtime_state(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _build_dir, simdisk = _make_sim_target(repo_root)
    preview_root = os.path.join(tmp_dir, "preview-root")
    theme_cfg = os.path.join(simdisk, ".rockbox", "themes", "preview.cfg")
    _make_file(
        theme_cfg,
        "\n".join(
            [
                "wps: /.rockbox/wps/preview.wps",
                "sbs: /.rockbox/wps/preview.sbs",
                "background color: 112233",
                "",
            ]
        ),
    )
    _make_file(os.path.join(simdisk, "Music", "Artist", "01 - Track.flac"), "audio\n")

    service = RockboxSimulatorService()
    target = {"simdisk_path": simdisk, "preview_root": preview_root}

    assert service.activate_theme_preview(target, "preview", preview_screen="wps") is True

    for path in (
        os.path.join(preview_root, ".config", "rockbox.org", "config.cfg"),
        os.path.join(simdisk, ".rockbox", "config.cfg"),
        os.path.join(simdisk, "config.cfg"),
    ):
        with open(path, "r", encoding="utf-8") as handle:
            text = handle.read()
        assert "start in screen: wps" in text
        assert "tagcache_autoupdate: off" in text
        assert "repeat: all" in text
        assert "theme: /.rockbox/themes/preview.cfg" in text
        if path.endswith(".rockbox/config.cfg"):
            assert os.path.exists(os.path.join(os.path.dirname(path), "database.ignore"))

    with open(os.path.join(simdisk, ".rockbox", ".playlist_control"), "r", encoding="utf-8") as handle:
        playlist = handle.read()
    assert "A:0:0:/Music/Artist/01 - Track.flac" in playlist

    for resume in (
        os.path.join(preview_root, ".config", "rockbox.org", ".resume.cfg"),
        os.path.join(preview_root, ".config", "rockbox.org", ".resume.cfg.new"),
        os.path.join(simdisk, ".rockbox", ".resume.cfg"),
        os.path.join(simdisk, ".rockbox", ".resume.cfg.new"),
    ):
        with open(resume, "r", encoding="utf-8") as handle:
            text = handle.read()
        assert "CRT: 1" in text
        assert "TRT: 1" in text


def test_theme_designer_preview_target_resets_even_if_rmtree_leaves_root_behind(tmp_dir, monkeypatch):
    repo_root = os.path.join(tmp_dir, "repo")
    build_dir, simdisk = _make_sim_target(repo_root)
    profile = {"id": "ipod-320x240", "source_repo_path": repo_root}
    target = {"id": "build-sim-video-5g", "name": "build-sim-video-5g", "simdisk_path": simdisk}
    service = RockboxSimulatorService()

    preview_root = os.path.join(repo_root, "rockpod", ".theme_designer", "simulator", profile["id"], target["id"])
    os.makedirs(os.path.join(preview_root, "simdisk", ".rockbox"), exist_ok=True)
    _make_file(os.path.join(preview_root, "simdisk", ".rockbox", "stale.txt"), "stale\n")
    original_rmtree = shutil.rmtree

    def _stub_rmtree(path, ignore_errors=False):
        if os.path.abspath(path) == os.path.abspath(preview_root):
            stale_file = os.path.join(preview_root, "simdisk", ".rockbox", "stale.txt")
            if os.path.exists(stale_file):
                os.remove(stale_file)
            return
        return original_rmtree(path, ignore_errors=ignore_errors)

    monkeypatch.setattr("services.rockbox_simulator.shutil.rmtree", _stub_rmtree)

    preview = service.theme_designer_preview_target(profile, target, reset=True)

    assert os.path.isdir(preview["simdisk_path"])
    assert not os.path.exists(os.path.join(preview["simdisk_path"], ".rockbox", "stale.txt"))


def test_launch_with_rom_reports_no_autoload_support(tmp_dir, monkeypatch):
    repo_root = os.path.join(tmp_dir, "repo")
    build_dir, simdisk = _make_sim_target(repo_root)
    service = RockboxSimulatorService()
    target = {
        "id": "build-sim-video-5g",
        "build_dir": build_dir,
        "binary_path": os.path.join(build_dir, "rockboxui"),
        "simdisk_path": simdisk,
    }

    launched_cmd = {}

    class _Proc:
        pid = 1234

    def fake_popen(cmd, **kwargs):
        launched_cmd["cmd"] = cmd
        launched_cmd["kwargs"] = kwargs
        return _Proc()

    monkeypatch.setattr(subprocess, "Popen", fake_popen)
    rom_path = os.path.join(simdisk, ".rockbox", "rocks", "games", "roms", "Tetris.gb")
    result = service.launch_with_rom(target, rom_path)

    assert result["pid"] == 1234
    assert result["autoload_supported"] is False
    assert result["rom_path"] == rom_path
    assert "--nobackground" in launched_cmd["cmd"]
    assert "--root" in launched_cmd["cmd"]
