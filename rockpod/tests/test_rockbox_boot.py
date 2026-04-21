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
    assert invalid["valid"] is False
    assert "Expected 320x240" in invalid["message"]


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


def test_boot_deploy_diff_generation_device_and_simulator(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    os.makedirs(repo_root, exist_ok=True)
    _config, store = _make_store(tmp_dir, repo_root)
    device_mount = os.path.join(tmp_dir, "device")
    simdisk = os.path.join(tmp_dir, "build-sim", "simdisk")
    os.makedirs(device_mount, exist_ok=True)
    os.makedirs(simdisk, exist_ok=True)

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["device_mount_path"] = device_mount
    profile["screen_resolution"] = "320x240"
    profile["simulator_simdisk_path"] = simdisk
    profile["simulator_target"] = "build-sim"
    profile = store.save_profile(profile)

    image_path = os.path.join(tmp_dir, "boot-device.png")
    _write_image(image_path, 320, 240)

    boot = RockboxBootService()
    deploy = RockboxDeployService()
    bundle = boot.build_bundle(profile, image_path, os.path.join(tmp_dir, "staging"))

    device_profile = boot.deploy_profile(profile, "device")
    diff_device = deploy.build_diff(device_profile, bundle)
    assert diff_device["summary"]["add"] == 1
    assert diff_device["items"][0]["destination_abs"].startswith(
        os.path.join(device_mount, ".rockbox", "rockpod", "boot", "branding")
    )

    sim_target = {"simdisk_path": simdisk}
    sim_profile = boot.deploy_profile(profile, "simulator", sim_target)
    diff_sim = deploy.build_diff(sim_profile, bundle)
    assert diff_sim["summary"]["add"] == 1
    assert diff_sim["items"][0]["destination_abs"].startswith(
        os.path.join(simdisk, ".rockbox", "rockpod", "boot", "branding")
    )


def test_boot_backup_and_restore_preserves_unrelated_files(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    os.makedirs(repo_root, exist_ok=True)
    _config, store = _make_store(tmp_dir, repo_root)
    device_mount = os.path.join(tmp_dir, "device")
    staged_boot = os.path.join(device_mount, ".rockbox", "rockpod", "boot", "branding", "320x240", "boot-logo.bmp")
    unrelated = os.path.join(device_mount, ".rockbox", "playlists", "keep.m3u8")
    _write_image(staged_boot, 320, 240, 0xFF112233)
    os.makedirs(os.path.dirname(unrelated), exist_ok=True)
    with open(unrelated, "w", encoding="utf-8") as handle:
        handle.write("leave alone\n")

    profile = store.current_profile()
    profile["source_repo_path"] = repo_root
    profile["device_mount_path"] = device_mount
    profile["screen_resolution"] = "320x240"
    profile = store.save_profile(profile)

    image_path = os.path.join(tmp_dir, "boot-new.png")
    _write_image(image_path, 320, 240, 0xFF445566)

    boot = RockboxBootService()
    deploy = RockboxDeployService()
    bundle = boot.build_bundle(profile, image_path, os.path.join(tmp_dir, "staging"))
    deploy_profile = boot.deploy_profile(profile, "device")
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
