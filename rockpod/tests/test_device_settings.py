from PySide6.QtWidgets import QApplication

from services.device_detector import DeviceInfo, create_mock_device
from ui.dialogs.device_settings import DeviceSettingsDialog


def test_audio_conversion_controls_disabled_by_default(config):
    app = QApplication.instance() or QApplication([])
    create_mock_device(config.mock_device_path)
    device = DeviceInfo(config.mock_device_path)

    dialog = DeviceSettingsDialog(config, device)

    assert dialog._convert_audio.isChecked() is False
    assert dialog._audio_conversion_mode.isEnabled() is False
    assert dialog._audio_conversion_codec.isEnabled() is False
    assert dialog._audio_conversion_bitrate.isEnabled() is False

    dialog._convert_audio.setChecked(True)

    assert dialog._audio_conversion_mode.isEnabled() is True
    assert dialog._audio_conversion_codec.isEnabled() is True
    assert dialog._audio_conversion_bitrate.isEnabled() is True
    assert app is not None


def test_audio_conversion_controls_enabled_when_override_is_on(config):
    app = QApplication.instance() or QApplication([])
    create_mock_device(config.mock_device_path)
    device = DeviceInfo(config.mock_device_path)
    config.set_device_override("convert_audio_for_device", True, device=device)

    dialog = DeviceSettingsDialog(config, device)

    assert dialog._convert_audio.isChecked() is True
    assert dialog._audio_conversion_mode.isEnabled() is True
    assert dialog._audio_conversion_codec.isEnabled() is True
    assert dialog._audio_conversion_bitrate.isEnabled() is True
    assert app is not None
