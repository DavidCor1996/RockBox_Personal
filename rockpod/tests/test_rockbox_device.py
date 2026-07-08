from services.rockbox_device import set_rockbox_ui_hold_effect


class _Device:
    def __init__(self, mount_path):
        self.mount_path = mount_path


def test_hold_effect_normalization_defaults_to_lockscreen(tmp_path):
    device = _Device(str(tmp_path))

    assert set_rockbox_ui_hold_effect(device, "invalid") is True
    config_path = tmp_path / ".rockbox" / "config.cfg"
    assert config_path.read_text(encoding="utf-8") == "ui engine hold effect: lockscreen\n"
