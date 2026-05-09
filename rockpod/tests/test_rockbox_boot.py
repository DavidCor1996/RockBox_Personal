import os

from PySide6.QtGui import QImage

from app.config import Config
from services.rockbox_boot import RockboxBootService
from services.rockbox_deploy import RockboxDeployService
from services.rockbox_profiles import RockboxProfileStore


def _make_store(tmp_dir, repo_root):
    config = Config(os.path.join(tmp_dir, "boot-config.json"))
    return config, RockboxProfileStore(config, repo_root)


def _write_image(path, width, height, color=0xFF335577):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    image = QImage(width, height, QImage.Format_RGB32)
    image.fill(color)
    assert image.save(path)


def _write_bytes(path, data=b"rockbox"):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as handle:
        handle.write(data)


def test_boot_image_validation(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    os.makedirs(repo_root, exist_ok=True)
    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["screen_resolution"] = "320x240"
    profile = store.save_profile(profile)

    valid_path = os.path.join(tmp_dir, "boot-valid.png")
    invalid_path = os.path.join(tmp_dir, "boot-invalid.png")
    _write_image(valid_path, 320, 240)
    _write_image(invalid_path, 300, 240)

    service = RockboxBootService()
    valid = service.validate_image(valid_path, profile)
    invalid = service.validate_image(invalid_path, profile)

    assert valid["valid"] is True
    assert invalid["valid"] is True
    assert invalid["requires_resize"] is True
    assert "Will scale 300x240 to 320x240" in invalid["message"]


def test_boot_preview_generation(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    os.makedirs(repo_root, exist_ok=True)
    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["screen_resolution"] = "176x132"
    profile = store.save_profile(profile)

    image_path = os.path.join(tmp_dir, "boot-preview-source.png")
    _write_image(image_path, 176, 132)

    service = RockboxBootService()
    result = service.generate_preview(image_path, profile, os.path.join(tmp_dir, "cache"))

    assert result["success"] is True
    assert os.path.isfile(result["preview_path"])
    preview = QImage(result["preview_path"])
    assert preview.width() == 176
    assert preview.height() == 132


def test_boot_preview_cache_path_changes_per_source_image(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    os.makedirs(repo_root, exist_ok=True)
    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["screen_resolution"] = "320x240"
    profile = store.save_profile(profile)

    first_path = os.path.join(tmp_dir, "boot-preview-a.png")
    second_path = os.path.join(tmp_dir, "boot-preview-b.png")
    _write_image(first_path, 320, 240, 0xFF112233)
    _write_image(second_path, 320, 240, 0xFF445566)

    service = RockboxBootService()
    first = service.generate_preview(first_path, profile, os.path.join(tmp_dir, "cache"))
    second = service.generate_preview(second_path, profile, os.path.join(tmp_dir, "cache"))

    assert first["success"] is True
    assert second["success"] is True
    assert first["preview_path"] != second["preview_path"]


def test_boot_preview_falls_back_to_source_image_when_png_write_fails(tmp_dir, monkeypatch):
    repo_root = os.path.join(tmp_dir, "repo")
    os.makedirs(repo_root, exist_ok=True)
    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["screen_resolution"] = "176x132"
    profile = store.save_profile(profile)

    image_path = os.path.join(tmp_dir, "boot-preview-fallback.png")
    _write_image(image_path, 176, 132)

    monkeypatch.setattr(QImage, "save", lambda self, *_args, **_kwargs: False)

    service = RockboxBootService()
    result = service.generate_preview(image_path, profile, os.path.join(tmp_dir, "cache"))

    assert result["success"] is True
    assert result["preview_path"] == os.path.abspath(image_path)


def test_boot_validation_supports_160x128_profiles(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    os.makedirs(repo_root, exist_ok=True)
    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["screen_resolution"] = "160x128"
    profile = store.save_profile(profile)

    valid_path = os.path.join(tmp_dir, "boot-3g-valid.png")
    invalid_path = os.path.join(tmp_dir, "boot-3g-invalid.png")
    _write_image(valid_path, 160, 128)
    _write_image(invalid_path, 160, 120)

    service = RockboxBootService()
    valid = service.validate_image(valid_path, profile)
    invalid = service.validate_image(invalid_path, profile)

    assert valid["valid"] is True
    assert invalid["valid"] is True
    assert invalid["requires_resize"] is True
    assert "Will scale 160x120 to 160x128" in invalid["message"]


def test_boot_preview_generation_scales_mismatched_source(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    os.makedirs(repo_root, exist_ok=True)
    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["screen_resolution"] = "160x128"
    profile = store.save_profile(profile)

    image_path = os.path.join(tmp_dir, "boot-preview-source-wide.png")
    _write_image(image_path, 240, 120)

    service = RockboxBootService()
    result = service.generate_preview(image_path, profile, os.path.join(tmp_dir, "cache"))

    assert result["success"] is True
    preview = QImage(result["preview_path"])
    assert preview.width() == 160
    assert preview.height() == 128


def test_boot_bundle_generation_scales_mismatched_source(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    os.makedirs(repo_root, exist_ok=True)
    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["screen_resolution"] = "320x240"
    profile = store.save_profile(profile)

    image_path = os.path.join(tmp_dir, "boot-bundle-source-tall.png")
    _write_image(image_path, 300, 400)

    service = RockboxBootService()
    bundle = service.build_bundle(profile, image_path, os.path.join(tmp_dir, "staging"))

    assert bundle["assets"]
    rendered = QImage(bundle["assets"][0]["source_abs"])
    assert rendered.width() == 320
    assert rendered.height() == 240


def test_boot_source_bundle_targets_repo_assets_for_device(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    os.makedirs(repo_root, exist_ok=True)
    _config, store = _make_store(tmp_dir, repo_root)

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["screen_resolution"] = "176x132"
    profile = store.save_profile(profile)

    image_path = os.path.join(tmp_dir, "boot-device.png")
    _write_image(image_path, 176, 132)

    boot = RockboxBootService()
    deploy = RockboxDeployService()
    bundle = boot.build_source_bundle(profile, image_path, os.path.join(tmp_dir, "staging"))
    diff = deploy.build_diff(boot.source_profile(profile), bundle)

    assert diff["summary"]["add"] == 2
    assert {
        item["destination_rel"] for item in diff["items"]
    } == {
        "apps/bitmaps/native/rockboxlogo.176x54x16.bmp",
        "wps/iPone_nano2g/BootLogo.bmp",
    }
    assert all(item["destination_abs"].startswith(repo_root) for item in diff["items"])
    assert all(asset["preserve_metadata"] is False for asset in bundle["assets"])


def test_boot_source_deploy_uses_fresh_destination_timestamp(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    os.makedirs(repo_root, exist_ok=True)
    _config, store = _make_store(tmp_dir, repo_root)

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["screen_resolution"] = "320x240"
    profile = store.save_profile(profile)

    image_path = os.path.join(tmp_dir, "boot-device.png")
    _write_image(image_path, 320, 240)

    boot = RockboxBootService()
    deploy = RockboxDeployService()
    bundle = boot.build_source_bundle(profile, image_path, os.path.join(tmp_dir, "staging"))
    os.utime(bundle["assets"][0]["source_abs"], (1, 1))
    diff = deploy.build_diff(boot.source_profile(profile), bundle)
    result = deploy.apply_diff(boot.source_profile(profile), diff)

    assert result["success"] is True
    assert os.path.getmtime(diff["items"][0]["destination_abs"]) > 1


def test_boot_rebuild_firmware_uses_expected_build_dir(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    build_dir = os.path.join(repo_root, "build-hw-ipodnano2g")
    os.makedirs(build_dir, exist_ok=True)
    _config, store = _make_store(tmp_dir, repo_root)

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["screen_resolution"] = "176x132"
    profile = store.save_profile(profile)

    calls = []

    def fake_run(command, check, capture_output, text):
        calls.append(command)
        _write_bytes(os.path.join(build_dir, "rockbox.ipod"))
        class _Result:
            returncode = 0
            stdout = "ok"
            stderr = ""
        return _Result()

    boot = RockboxBootService()
    result = boot.rebuild_firmware(profile, runner=fake_run)

    assert result["success"] is True
    assert result["artifact_path"] == os.path.join(build_dir, "rockbox.ipod")
    assert calls == [["make", "-C", build_dir, "-j4", os.path.join(build_dir, "rockbox.ipod")]]


def test_boot_firmware_build_dir_prefers_video_for_ambiguous_320x240_profile(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    video_dir = os.path.join(repo_root, "build-hw-ipodvideo-5g")
    classic_dir = os.path.join(repo_root, "build-hw-ipod6g")
    os.makedirs(video_dir, exist_ok=True)
    os.makedirs(classic_dir, exist_ok=True)
    _config, store = _make_store(tmp_dir, repo_root)

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["target_device_model"] = "iPod Classic / Video"
    profile["screen_resolution"] = "320x240"
    profile = store.save_profile(profile)

    assert RockboxBootService().firmware_build_dir(profile) == video_dir


def test_boot_firmware_build_dir_uses_6g_for_classic_profile(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    video_dir = os.path.join(repo_root, "build-hw-ipodvideo-5g")
    classic_dir = os.path.join(repo_root, "build-hw-ipod6g")
    os.makedirs(video_dir, exist_ok=True)
    os.makedirs(classic_dir, exist_ok=True)
    _config, store = _make_store(tmp_dir, repo_root)

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["target_device_model"] = "iPod Classic 6G"
    profile["screen_resolution"] = "320x240"
    profile = store.save_profile(profile)

    assert RockboxBootService().firmware_build_dir(profile) == classic_dir


def test_boot_full_install_builds_and_runs_rockbox_fullinstall(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    build_dir = os.path.join(repo_root, "build-hw-ipodvideo-5g")
    mount_root = os.path.join(tmp_dir, "device")
    os.makedirs(build_dir, exist_ok=True)
    os.makedirs(mount_root, exist_ok=True)
    _config, store = _make_store(tmp_dir, repo_root)

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["device_mount_path"] = mount_root
    profile["screen_resolution"] = "320x240"
    profile = store.save_profile(profile)

    calls = []
    progress = []
    artifact = os.path.join(build_dir, "rockbox.ipod")

    def fake_run(command, check, capture_output, text):
        calls.append(command)
        if command[:2] == ["make", "-C"] and command[-1] == artifact:
            _write_bytes(artifact)

        class _Result:
            returncode = 0
            stdout = "ok"
            stderr = ""

        return _Result()

    result = RockboxBootService().full_install_firmware(
        profile,
        runner=fake_run,
        progress_callback=lambda current, total, label: progress.append((current, total, label)),
    )

    assert result["success"] is True
    assert result["artifact_path"] == artifact
    assert calls == [
        ["make", "-C", build_dir, "-j4", artifact],
        ["make", "-C", build_dir, f"PREFIX={mount_root}", "fullinstall"],
    ]
    assert progress[0] == (1, 2, "Building rockbox.ipod")
    assert progress[-1] == (2, 2, "Full installing Rockbox to iPod")


def test_invalidate_firmware_boot_assets_removes_stale_generated_outputs(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    build_dir = os.path.join(repo_root, "build-hw-ipodvideo-5g")
    generated_c = os.path.join(build_dir, "apps", "bitmaps", "native", "rockboxlogo.320x98x16.c")
    generated_o = os.path.join(build_dir, "apps", "bitmaps", "native", "rockboxlogo.320x98x16.o")
    generated_h = os.path.join(build_dir, "bitmaps", "rockboxlogo.h")
    firmware = os.path.join(build_dir, "rockbox.ipod")
    unrelated = os.path.join(build_dir, "apps", "bitmaps", "native", "usblogo.176x48x16.c")
    for path in (generated_c, generated_o, generated_h, firmware, unrelated):
        _write_bytes(path)
    _config, store = _make_store(tmp_dir, repo_root)

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["screen_resolution"] = "320x240"
    profile = store.save_profile(profile)

    removed = RockboxBootService().invalidate_firmware_boot_assets(profile)

    assert set(removed) == {generated_c, generated_o, generated_h, firmware}
    assert not os.path.exists(generated_c)
    assert not os.path.exists(generated_o)
    assert not os.path.exists(generated_h)
    assert not os.path.exists(firmware)
    assert os.path.isfile(unrelated)


def test_boot_firmware_bundle_targets_rockbox_ipod(tmp_dir):
    artifact = os.path.join(tmp_dir, "rockbox.ipod")
    _write_bytes(artifact)

    bundle = RockboxBootService().build_firmware_bundle(artifact)

    assert bundle["assets"][0]["destination_rel"] == "rockbox.ipod"
    assert bundle["assets"][0]["source_abs"] == os.path.abspath(artifact)


def test_nano2g_bootloader_requirements_report_encrypted_install(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    os.makedirs(repo_root, exist_ok=True)
    _config, store = _make_store(tmp_dir, repo_root)

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["screen_resolution"] = "176x132"
    profile = store.save_profile(profile)

    requirements = RockboxBootService().bootloader_requirements(profile)

    assert requirements["required"] is True
    assert requirements["install_supported"] is False
    assert requirements["output_name"] == "bootloader-ipodnano2g.ipod"
    assert requirements["install_name"] == "bootloader-ipodnano2g.ipodx"
    assert ".ipodx" in requirements["message"]


def test_boot_rebuild_bootloader_configures_missing_build_tree(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    os.makedirs(os.path.join(repo_root, "tools"), exist_ok=True)
    _write_bytes(os.path.join(repo_root, "tools", "configure"), b"#!/bin/sh\n")
    _config, store = _make_store(tmp_dir, repo_root)

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["screen_resolution"] = "176x132"
    profile = store.save_profile(profile)

    calls = []
    build_dir = os.path.join(repo_root, "build-bootloader-ipodnano2g")
    artifact = os.path.join(build_dir, "bootloader-ipodnano2g.ipod")

    def fake_run(command, check, capture_output, text, cwd=None):
        calls.append({"command": command, "cwd": cwd})

        class _Result:
            returncode = 0
            stdout = "ok"
            stderr = ""

        if command[0].endswith("configure"):
            with open(os.path.join(cwd, "Makefile"), "w", encoding="utf-8") as handle:
                handle.write("all:\n")
        elif command[0] == "make":
            _write_bytes(artifact)
        return _Result()

    boot = RockboxBootService()
    result = boot.rebuild_bootloader(profile, runner=fake_run)

    assert result["success"] is True
    assert result["artifact_path"] == artifact
    assert calls[0]["command"] == [
        os.path.join(repo_root, "tools", "configure"),
        "--target=ipodnano2g",
        "--type=b",
    ]
    assert calls[0]["cwd"] == build_dir
    assert calls[1]["command"] == ["make", "-C", build_dir, "-j4"]


def test_nano2g_bootloader_stage_bundle_stages_marker_and_repairs_legacy_startup_files(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    mount_root = os.path.join(tmp_dir, "device")
    build_dir = os.path.join(repo_root, "build-hw-ipodnano2g")
    os.makedirs(os.path.join(mount_root, ".rockbox", "rocks"), exist_ok=True)
    os.makedirs(os.path.join(build_dir, "apps", "plugins"), exist_ok=True)
    _write_bytes(os.path.join(build_dir, "apps", "plugins", "crypt_firmware.rock"))
    artifact = os.path.join(build_dir, "bootloader-ipodnano2g.ipod")
    _write_bytes(artifact, b"nano2g bootloader")
    _config, store = _make_store(tmp_dir, repo_root)

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["device_mount_path"] = mount_root
    profile["screen_resolution"] = "176x132"
    profile = store.save_profile(profile)

    boot = RockboxBootService()
    boot.save_bootloader_stage_metadata(
        profile,
        {
            "original_config": 'volume: -20\nstart in screen: root\nopenplugin: "Start Screen", "old", "/bad", "/bad"\n',
            "original_plugin_dat_exists": True,
            "original_plugin_dat_base64": "QUJD",
        },
    )
    bundle, metadata = boot.build_bootloader_stage_bundle(profile, artifact, os.path.join(tmp_dir, "staging"))

    config_asset = next(asset for asset in bundle["assets"] if asset["kind"] == "bootloader_restore_config")
    with open(config_asset["source_abs"], "r", encoding="utf-8") as handle:
        assert handle.read() == 'volume: -20\nstart in screen: root\nopenplugin: "Start Screen", "old", "/bad", "/bad"\n'
    plugin_dat_asset = next(asset for asset in bundle["assets"] if asset["kind"] == "bootloader_restore_plugin_dat")
    with open(plugin_dat_asset["source_abs"], "rb") as handle:
        assert handle.read() == b"ABC"
    marker_asset = next(asset for asset in bundle["assets"] if asset["kind"] == "bootloader_stage_marker")
    with open(marker_asset["source_abs"], "r", encoding="utf-8") as handle:
        assert handle.read() == "pending\n"

    assert metadata["artifact_sha256"] == boot._hash_file(artifact)
    assert metadata["marker_rel"] == ".rockbox/rockpod/boot/nano2g-encrypt-pending"
    assert metadata["original_plugin_dat_exists"] is True


def test_nano2g_bootloader_restore_bundle_restores_plugin_dat_or_removes_when_absent(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _config, store = _make_store(tmp_dir, repo_root)
    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["device_mount_path"] = os.path.join(tmp_dir, "device")
    profile["screen_resolution"] = "176x132"
    profile = store.save_profile(profile)

    boot = RockboxBootService()
    boot.save_bootloader_stage_metadata(
        profile,
        {
            "original_config": "volume: -20\n",
            "original_plugin_dat_exists": True,
            "original_plugin_dat_base64": "QUJD",
        },
    )
    bundle = boot.build_bootloader_restore_config_bundle(profile, os.path.join(tmp_dir, "restore"))
    plugin_asset = next(asset for asset in bundle["assets"] if asset["kind"] == "bootloader_restore_plugin_dat")
    with open(plugin_asset["source_abs"], "rb") as handle:
        assert handle.read() == b"ABC"

    boot.save_bootloader_stage_metadata(
        profile,
        {
            "original_config": "volume: -20\n",
            "original_plugin_dat_exists": False,
            "original_plugin_dat_base64": "",
        },
    )
    bundle = boot.build_bootloader_restore_config_bundle(profile, os.path.join(tmp_dir, "restore2"))
    plugin_asset = next(asset for asset in bundle["assets"] if asset["kind"] == "bootloader_restore_plugin_dat")
    assert plugin_asset["action"] == "remove"


def test_nano2g_bootloader_stage_bundle_restores_config_without_plugin_dat_backup(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    mount_root = os.path.join(tmp_dir, "device")
    build_dir = os.path.join(repo_root, "build-hw-ipodnano2g")
    os.makedirs(os.path.join(build_dir, "apps", "plugins"), exist_ok=True)
    _write_bytes(os.path.join(build_dir, "apps", "plugins", "crypt_firmware.rock"))
    artifact = os.path.join(build_dir, "bootloader-ipodnano2g.ipod")
    _write_bytes(artifact, b"nano2g bootloader")
    _config, store = _make_store(tmp_dir, repo_root)

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["device_mount_path"] = mount_root
    profile["screen_resolution"] = "176x132"
    profile = store.save_profile(profile)

    boot = RockboxBootService()
    boot.save_bootloader_stage_metadata(
        profile,
        {
            "original_config": "volume: -23\n",
            "original_plugin_dat_exists": False,
        },
    )
    bundle, _metadata = boot.build_bootloader_stage_bundle(profile, artifact, os.path.join(tmp_dir, "staging"))

    config_asset = next(asset for asset in bundle["assets"] if asset["kind"] == "bootloader_restore_config")
    plugin_asset = next(asset for asset in bundle["assets"] if asset["kind"] == "bootloader_restore_plugin_dat")
    with open(config_asset["source_abs"], "r", encoding="utf-8") as handle:
        assert handle.read() == "volume: -23\n"
    assert plugin_asset["action"] == "remove"


def test_nano2g_bootloader_stage_status_tracks_encrypted_output_and_hash(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    mount_root = os.path.join(tmp_dir, "device")
    os.makedirs(os.path.join(mount_root, ".rockbox"), exist_ok=True)
    os.makedirs(os.path.join(mount_root, ".rockbox", "rockpod", "boot"), exist_ok=True)
    _config, store = _make_store(tmp_dir, repo_root)

    artifact = os.path.join(tmp_dir, "bootloader-ipodnano2g.ipod")
    _write_bytes(artifact, b"plain bootloader")
    _write_bytes(os.path.join(mount_root, ".rockbox", "b.ipod"), b"plain bootloader")
    _write_bytes(os.path.join(mount_root, ".rockbox", "b.ipodx"), b"encrypted bootloader")
    _write_bytes(os.path.join(mount_root, ".rockbox", "rockpod", "boot", "nano2g-encrypt-pending"), b"pending\n")

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["device_mount_path"] = mount_root
    profile["screen_resolution"] = "176x132"
    profile = store.save_profile(profile)

    boot = RockboxBootService()
    boot.save_bootloader_stage_metadata(
        profile,
        {
            "artifact_sha256": boot._hash_file(artifact),
            "original_config": "volume: -20\n",
        },
    )

    status = boot.bootloader_stage_status(profile, artifact)

    assert status["plain_staged"] is True
    assert status["encrypted_ready"] is True
    assert status["marker_present"] is True
    assert status["stage_pending"] is False
    assert status["metadata_present"] is True
    assert status["staged_input_matches_metadata"] is True
    assert status["current_artifact_matches"] is True


def test_nano2g_bootloader_stage_ready_for_install_accepts_matching_staged_input(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    mount_root = os.path.join(tmp_dir, "device")
    os.makedirs(os.path.join(mount_root, ".rockbox", "rockpod", "boot"), exist_ok=True)
    artifact = os.path.join(tmp_dir, "bootloader-ipodnano2g.ipod")
    staged_input = os.path.join(mount_root, ".rockbox", "b.ipod")
    encrypted_output = os.path.join(mount_root, ".rockbox", "b.ipodx")
    _write_bytes(artifact, b"new plain bootloader")
    _write_bytes(staged_input, b"old staged bootloader")
    _write_bytes(encrypted_output, b"encrypted bootloader")
    _config, store = _make_store(tmp_dir, repo_root)

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["device_mount_path"] = mount_root
    profile["screen_resolution"] = "176x132"
    profile = store.save_profile(profile)

    boot = RockboxBootService()
    boot.save_bootloader_stage_metadata(
        profile,
        {
            "artifact_sha256": boot._hash_file(staged_input),
            "original_config": "volume: -20\n",
        },
    )

    status = boot.bootloader_stage_status(profile, artifact)

    assert status["encrypted_ready"] is True
    assert status["staged_input_matches_metadata"] is True
    assert status["current_artifact_matches"] is False
    assert boot.bootloader_stage_ready_for_install(profile, artifact) is True


def test_resolve_disk_nodes_uses_mount_source_and_parent_disk(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    mount_root = os.path.join(tmp_dir, "device")
    os.makedirs(mount_root, exist_ok=True)
    _config, store = _make_store(tmp_dir, repo_root)

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["device_mount_path"] = mount_root
    profile["screen_resolution"] = "176x132"
    profile = store.save_profile(profile)

    calls = []

    def fake_run(command, check, capture_output, text):
        calls.append(command)
        class _Result:
            returncode = 0
            stdout = ""
            stderr = ""
        result = _Result()
        if command[:2] == ["findmnt", "-no"]:
            result.stdout = "/dev/sda2\n"
        elif command[:3] == ["lsblk", "-no", "PKNAME"]:
            result.stdout = "sda\n"
        return result

    result = RockboxBootService().resolve_disk_nodes(profile, runner=fake_run)

    assert result["success"] is True
    assert result["partition_path"] == "/dev/sda2"
    assert result["disk_path"] == "/dev/sda"
    assert calls == [
        ["findmnt", "-no", "SOURCE", mount_root],
        ["lsblk", "-no", "PKNAME", "/dev/sda2"],
    ]


def test_install_encrypted_bootloader_unmounts_installs_and_remounts(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    mount_root = os.path.join(tmp_dir, "device")
    tool_dir = os.path.join(repo_root, "utils", "ipodpatcher")
    os.makedirs(tool_dir, exist_ok=True)
    os.makedirs(mount_root, exist_ok=True)
    binary_path = os.path.join(tool_dir, "ipodpatcher")
    _write_bytes(binary_path, b"#!/bin/sh\n")
    os.chmod(binary_path, 0o755)
    encrypted_path = os.path.join(tmp_dir, "bootloader-ipodnano2g.ipodx")
    _write_bytes(encrypted_path, b"encrypted bootloader")
    _config, store = _make_store(tmp_dir, repo_root)

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["device_mount_path"] = mount_root
    profile["screen_resolution"] = "176x132"
    profile = store.save_profile(profile)

    calls = []

    def fake_run(command, check, capture_output, text):
        calls.append(command)
        class _Result:
            returncode = 0
            stdout = ""
            stderr = ""
        result = _Result()
        if command[:2] == ["findmnt", "-no"]:
            result.stdout = "/dev/sda2\n"
        elif command[:3] == ["lsblk", "-no", "PKNAME"]:
            result.stdout = "sda\n"
        elif command[:2] == ["udisksctl", "unmount"]:
            result.stdout = "Unmounted /dev/sda2\n"
        elif command[0] == "pkexec":
            result.stdout = "[INFO] Bootloader added\n"
        elif command[:2] == ["udisksctl", "mount"]:
            result.stdout = "Mounted /dev/sda2\n"
        return result

    result = RockboxBootService().install_encrypted_bootloader(profile, encrypted_path, runner=fake_run)

    assert result["success"] is True
    assert calls == [
        ["findmnt", "-no", "SOURCE", mount_root],
        ["lsblk", "-no", "PKNAME", "/dev/sda2"],
        ["udisksctl", "unmount", "-b", "/dev/sda2"],
        ["pkexec", binary_path, "/dev/sda", "-a", encrypted_path],
        ["udisksctl", "mount", "-b", "/dev/sda2"],
    ]


def test_install_encrypted_bootloader_copies_input_off_mount_before_unmount(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    mount_root = os.path.join(tmp_dir, "device")
    rockbox_root = os.path.join(mount_root, ".rockbox")
    tool_dir = os.path.join(repo_root, "utils", "ipodpatcher")
    os.makedirs(tool_dir, exist_ok=True)
    os.makedirs(rockbox_root, exist_ok=True)
    binary_path = os.path.join(tool_dir, "ipodpatcher")
    _write_bytes(binary_path, b"#!/bin/sh\n")
    os.chmod(binary_path, 0o755)
    encrypted_path = os.path.join(rockbox_root, "b.ipodx")
    _write_bytes(encrypted_path, b"encrypted bootloader")
    _config, store = _make_store(tmp_dir, repo_root)

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["device_mount_path"] = mount_root
    profile["screen_resolution"] = "176x132"
    profile = store.save_profile(profile)

    calls = []

    def fake_run(command, check, capture_output, text):
        calls.append(command)
        class _Result:
            returncode = 0
            stdout = ""
            stderr = ""
        result = _Result()
        if command[:2] == ["findmnt", "-no"]:
            result.stdout = "/dev/sda2\n"
        elif command[:3] == ["lsblk", "-no", "PKNAME"]:
            result.stdout = "sda\n"
        elif command[:2] == ["udisksctl", "unmount"]:
            result.stdout = "Unmounted /dev/sda2\n"
        elif command[0] == "pkexec":
            result.stdout = "[INFO] Bootloader added\n"
            assert command[4] != encrypted_path
            assert os.path.isfile(command[4])
        elif command[:2] == ["udisksctl", "mount"]:
            result.stdout = "Mounted /dev/sda2\n"
        return result

    result = RockboxBootService().install_encrypted_bootloader(profile, encrypted_path, runner=fake_run)

    assert result["success"] is True
    assert calls[3][0:4] == ["pkexec", binary_path, "/dev/sda", "-a"]


def test_install_encrypted_bootloader_treats_ipodpatcher_err_output_as_failure(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    mount_root = os.path.join(tmp_dir, "device")
    tool_dir = os.path.join(repo_root, "utils", "ipodpatcher")
    os.makedirs(tool_dir, exist_ok=True)
    os.makedirs(mount_root, exist_ok=True)
    binary_path = os.path.join(tool_dir, "ipodpatcher")
    _write_bytes(binary_path, b"#!/bin/sh\n")
    os.chmod(binary_path, 0o755)
    encrypted_path = os.path.join(tmp_dir, "bootloader-ipodnano2g.ipodx")
    _write_bytes(encrypted_path, b"encrypted bootloader")
    _config, store = _make_store(tmp_dir, repo_root)

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["device_mount_path"] = mount_root
    profile["screen_resolution"] = "176x132"
    profile = store.save_profile(profile)

    def fake_run(command, check, capture_output, text):
        class _Result:
            returncode = 0
            stdout = ""
            stderr = ""
        result = _Result()
        if command[:2] == ["findmnt", "-no"]:
            result.stdout = "/dev/sda2\n"
        elif command[:3] == ["lsblk", "-no", "PKNAME"]:
            result.stdout = "sda\n"
        elif command[:2] == ["udisksctl", "unmount"]:
            result.stdout = "Unmounted /dev/sda2\n"
        elif command[0] == "pkexec":
            result.stderr = "[ERR] --add-bootloader failed.\n"
        elif command[:2] == ["udisksctl", "mount"]:
            result.stdout = "Mounted /dev/sda2\n"
        return result

    result = RockboxBootService().install_encrypted_bootloader(profile, encrypted_path, runner=fake_run)

    assert result["success"] is False
    assert result["message"] == "ipodpatcher failed to install the encrypted bootloader"


def test_boot_backup_and_restore_preserves_unrelated_files(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    os.makedirs(repo_root, exist_ok=True)
    _config, store = _make_store(tmp_dir, repo_root)
    staged_boot = os.path.join(repo_root, "apps", "bitmaps", "native", "rockboxlogo.320x98x16.bmp")
    unrelated = os.path.join(repo_root, "wps", "keep.wps")
    _write_image(staged_boot, 320, 240, 0xFF112233)
    os.makedirs(os.path.dirname(unrelated), exist_ok=True)
    with open(unrelated, "w", encoding="utf-8") as handle:
        handle.write("leave alone\n")

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["screen_resolution"] = "320x240"
    profile = store.save_profile(profile)

    image_path = os.path.join(tmp_dir, "boot-new.png")
    _write_image(image_path, 320, 240, 0xFF445566)

    boot = RockboxBootService()
    deploy = RockboxDeployService()
    bundle = boot.build_source_bundle(profile, image_path, os.path.join(tmp_dir, "staging"))
    deploy_profile = boot.source_profile(profile)
    diff = deploy.build_diff(deploy_profile, bundle)
    assert diff["items"][0]["status"] == "overwrite"

    result = deploy.apply_diff(deploy_profile, diff)
    assert result["success"] is True
    restore = deploy.restore_latest_backup(deploy_profile)
    assert restore["success"] is True

    restored = QImage(staged_boot)
    assert restored.width() == 320
    assert restored.height() == 240
    with open(unrelated, "r", encoding="utf-8") as handle:
        assert handle.read() == "leave alone\n"
