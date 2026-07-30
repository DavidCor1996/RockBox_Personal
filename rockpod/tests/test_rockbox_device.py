from services.rockbox_device import (
    set_rockbox_ui_extras_pane,
    set_rockbox_ui_hold_effect,
)


class _Device:
    def __init__(self, mount_path):
        self.mount_path = mount_path


def test_hold_effect_normalization_defaults_to_lockscreen(tmp_path):
    device = _Device(str(tmp_path))

    assert set_rockbox_ui_hold_effect(device, "invalid") is True
    config_path = tmp_path / ".rockbox" / "config.cfg"
    assert config_path.read_text(encoding="utf-8") == "ui engine hold effect: lockscreen\n"


def test_extras_pane_writes_sitekick_and_defaults_to_clock(tmp_path):
    device = _Device(str(tmp_path))
    config_path = tmp_path / ".rockbox" / "config.cfg"

    assert set_rockbox_ui_extras_pane(device, "sitekick") is True
    assert "ui engine extras pane: sitekick" in config_path.read_text(
        encoding="utf-8"
    )
    assert set_rockbox_ui_extras_pane(device, "invalid") is True
    assert "ui engine extras pane: clock" in config_path.read_text(
        encoding="utf-8"
    )
