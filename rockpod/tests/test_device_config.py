"""Tests for per-device config overrides."""

from services.device_detector import DeviceInfo, create_mock_device


def test_get_effective_uses_device_override_and_falls_back_to_global(config):
    create_mock_device(config.mock_device_path)
    device = DeviceInfo(config.mock_device_path)
    config.set("sync_playlists_to_device", True)

    assert config.get_effective("sync_playlists_to_device", device=device) is True

    config.set_device_override("sync_playlists_to_device", False, device=device)
    assert config.get_effective("sync_playlists_to_device", device=device) is False

    config.remove_device_override("sync_playlists_to_device", device=device)
    assert config.get_effective("sync_playlists_to_device", device=device) is True


def test_get_device_display_name_reads_override(config):
    create_mock_device(config.mock_device_path)
    device = DeviceInfo(config.mock_device_path)

    assert config.get_device_display_name(device=device) == ""

    config.set_device_override("display_name", "Car iPod", device=device)

    assert config.get_device_display_name(device=device) == "Car iPod"
