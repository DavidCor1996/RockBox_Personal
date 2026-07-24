"""UI safety tests for the Android engineering workspace."""

from PySide6.QtWidgets import QApplication, QLabel, QLineEdit, QPushButton

from ui.android_manager import AndroidManagerWidget


def test_android_page_keeps_partition_nor_lock_and_no_install_button():
    app = QApplication.instance() or QApplication([])
    widget = AndroidManagerWidget()

    lock = widget.findChild(QLabel, "android_hardware_lock")
    button_texts = [button.text() for button in widget.findChildren(QPushButton)]

    assert "PARTITION/NOR LOCKED" in lock.text()
    assert "cannot resize a filesystem" in lock.text()
    assert not any(text.strip().lower().startswith("install") for text in button_texts)


def test_android_page_emits_image_only_dry_run_request(tmp_path):
    app = QApplication.instance() or QApplication([])
    widget = AndroidManagerWidget()
    image = tmp_path / "test.img"
    image.write_bytes(b"fixture")
    seen = []
    widget.dry_run_requested.connect(lambda *args: seen.append(args))

    widget.findChild(QLineEdit, "android_image_path").setText(str(image))
    widget.findChild(QPushButton, "android_analyze_image").click()

    assert seen == [(str(image), 1024, 4096)]


def test_android_page_reports_qualified_ramdiag_without_launch_control():
    app = QApplication.instance() or QApplication([])
    widget = AndroidManagerWidget()

    widget.set_ramdiag_status(
        {
            "profile": "ram-only-no-storage",
            "artifacts": {"a": {}, "b": {}, "c": {}, "d": {}},
        }
    )

    status = widget.findChild(QLabel, "android_ramdiag_status")
    assert "checksum-qualified" in status.text()
    assert "60-second automatic recovery" in status.text()
    assert "Hardware launch remains disabled" in status.text()


def test_android_page_labels_eclair_native_core_as_classic_panel_init():
    app = QApplication.instance() or QApplication([])
    widget = AndroidManagerWidget()

    widget.set_eclair_native_status(
        {
            "aosp_tag": "android-2.0_r1",
            "artifacts": {str(index): {} for index in range(6)},
        }
    )

    status = widget.findChild(QLabel, "android_eclair_native_status")
    assert "native core checksum-qualified" in status.text()
    assert "full-system boot and Binder test passed" in status.text()
    assert "system_server, SurfaceFlinger, the Rockpod Launcher" in status.text()
    assert "all four Rockbox Classic panel-init paths" in status.text()
    assert "PCF50635 display power" in status.text()
    assert "Rockbox-derived click-wheel" in status.text()
    assert "Android DPAD key layout" in status.text()
    assert "three-minute volatile-test timeout" in status.text()
    assert "physical LCD output" in status.text()
    assert "Menu+Play RAM-boot path is packaged" in status.text()
    assert "does not install it to NOR" in status.text()


def test_android_page_enables_explicit_ram_boot_staging_only_after_gate():
    app = QApplication.instance() or QApplication([])
    widget = AndroidManagerWidget()
    button = widget.findChild(QPushButton, "android_stage_ram_boot")
    seen = []
    widget.stage_boot_requested.connect(lambda: seen.append(True))

    assert button.isEnabled() is False
    widget.set_eclair_native_status(
        {
            "aosp_tag": "android-2.0_r1",
            "artifacts": {},
            "rockbox_menu_play_boot_packaged": True,
            "rockbox_boot_components_checksum_wrapped": True,
        }
    )
    assert button.isEnabled() is True
    button.click()
    assert seen == [True]
