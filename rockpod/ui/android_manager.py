"""Rockpod's qualification and RAM-payload page for an iPod 6G Android port."""

from __future__ import annotations

import json
from pathlib import Path

from PySide6.QtCore import Signal
from PySide6.QtWidgets import (
    QComboBox,
    QFileDialog,
    QFormLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QPlainTextEdit,
    QPushButton,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)


class AndroidManagerWidget(QWidget):
    """Expose image planning plus narrowly bounded direct-boot staging."""

    dry_run_requested = Signal(str, int, int)
    fixture_requested = Signal(str, int, int, int, bool)
    commit_image_requested = Signal(str, str, int, int)
    verify_image_requested = Signal(str, int, int)
    rollback_image_requested = Signal(str, int, int)
    stage_boot_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("android_manager")
        self._last_plan = None

        layout = QVBoxLayout(self)
        layout.setContentsMargins(20, 18, 20, 18)
        layout.setSpacing(12)

        title = QLabel("Android on iPod Classic")
        title.setObjectName("section_title")
        layout.addWidget(title)

        subtitle = QLabel(
            "Android 2.0 engineering workspace for iPod Classic 6G/7G. "
            "This build can inspect sparse disk images and stage a checksum-verified, "
            "RAM-only boot payload on an existing Rockbox volume."
        )
        subtitle.setWordWrap(True)
        layout.addWidget(subtitle)

        self._lock_label = QLabel(
            "PARTITION/NOR LOCKED — payload staging may only create .rockbox/android files. "
            "It cannot resize a filesystem, change a partition table, replace Rockbox, "
            "write NOR, or perform USB/DFU actions."
        )
        self._lock_label.setObjectName("android_hardware_lock")
        self._lock_label.setWordWrap(True)
        self._lock_label.setStyleSheet(
            "QLabel { background: #fff1d6; color: #6b4200; border: 1px solid #e2b45d; "
            "padding: 8px; font-weight: bold; }"
        )
        layout.addWidget(self._lock_label)

        self._ramdiag_status = QLabel(
            "N25 volatile RAM diagnostic bundle: checking local qualification."
        )
        self._ramdiag_status.setObjectName("android_ramdiag_status")
        self._ramdiag_status.setWordWrap(True)
        layout.addWidget(self._ramdiag_status)

        self._eclair_native_status = QLabel(
            "Android 2.0 native core: checking local qualification."
        )
        self._eclair_native_status.setObjectName("android_eclair_native_status")
        self._eclair_native_status.setWordWrap(True)
        layout.addWidget(self._eclair_native_status)

        self._stage_boot = QPushButton("Stage RAM Boot Files on Connected iPod…")
        self._stage_boot.setObjectName("android_stage_ram_boot")
        self._stage_boot.setEnabled(False)
        self._stage_boot.clicked.connect(self.stage_boot_requested)
        layout.addWidget(self._stage_boot)

        form = QFormLayout()
        image_row = QHBoxLayout()
        self._image_path = QLineEdit()
        self._image_path.setObjectName("android_image_path")
        self._image_path.setPlaceholderText("Choose a regular disk-image file")
        self._image_path.textChanged.connect(self._invalidate_plan)
        browse = QPushButton("Choose…")
        browse.clicked.connect(self._choose_image)
        image_row.addWidget(self._image_path, 1)
        image_row.addWidget(browse)
        form.addRow("Disk image", image_row)

        self._android_size = QSpinBox()
        self._android_size.setObjectName("android_allocation_mib")
        self._android_size.setRange(512, 131072)
        self._android_size.setSingleStep(256)
        self._android_size.setValue(1024)
        self._android_size.setSuffix(" MiB")
        form.addRow("Android allocation", self._android_size)

        self._sector_size = QComboBox()
        self._sector_size.setObjectName("android_sector_size")
        self._sector_size.addItem("4096 bytes (observed iPod)", 4096)
        self._sector_size.addItem("512 bytes (negative/compatibility test)", 512)
        form.addRow("Logical sector", self._sector_size)

        self._fixture_size = QSpinBox()
        self._fixture_size.setObjectName("android_fixture_size_mib")
        self._fixture_size.setRange(2048, 524288)
        self._fixture_size.setSingleStep(1024)
        self._fixture_size.setValue(4096)
        self._fixture_size.setSuffix(" MiB sparse")
        form.addRow("Test-image size", self._fixture_size)
        layout.addLayout(form)

        buttons = QHBoxLayout()
        self._create_unshrunk = QPushButton("Create Unshrunk Test Image…")
        self._create_unshrunk.setObjectName("android_create_unshrunk")
        self._create_unshrunk.clicked.connect(lambda: self._choose_fixture(False))
        self._create_preshrunk = QPushButton("Create Pre-shrunk Test Image…")
        self._create_preshrunk.setObjectName("android_create_preshrunk")
        self._create_preshrunk.clicked.connect(lambda: self._choose_fixture(True))
        self._analyze = QPushButton("Analyze Image (Dry Run)")
        self._analyze.setObjectName("android_analyze_image")
        self._analyze.clicked.connect(self._emit_dry_run)
        buttons.addWidget(self._create_unshrunk)
        buttons.addWidget(self._create_preshrunk)
        buttons.addStretch(1)
        buttons.addWidget(self._analyze)
        layout.addLayout(buttons)

        transaction_buttons = QHBoxLayout()
        self._commit_image = QPushButton("Commit Layout to Test Image")
        self._commit_image.setObjectName("android_commit_image")
        self._commit_image.setEnabled(False)
        self._commit_image.clicked.connect(self._emit_commit_image)
        self._verify_image = QPushButton("Verify Test Image")
        self._verify_image.setObjectName("android_verify_image")
        self._verify_image.clicked.connect(self._emit_verify_image)
        self._rollback_image = QPushButton("Rollback Test Image")
        self._rollback_image.setObjectName("android_rollback_image")
        self._rollback_image.setEnabled(False)
        self._rollback_image.clicked.connect(self._emit_rollback_image)
        transaction_buttons.addStretch(1)
        transaction_buttons.addWidget(self._commit_image)
        transaction_buttons.addWidget(self._verify_image)
        transaction_buttons.addWidget(self._rollback_image)
        layout.addLayout(transaction_buttons)

        self._status = QLabel("Waiting for helper qualification.")
        self._status.setObjectName("android_plan_status")
        self._status.setWordWrap(True)
        layout.addWidget(self._status)

        self._details = QPlainTextEdit()
        self._details.setObjectName("android_plan_details")
        self._details.setReadOnly(True)
        self._details.setPlaceholderText(
            "A signed-off dry-run plan will show the MBR/FAT32 identity, exact sector ranges, and safety reasons here."
        )
        layout.addWidget(self._details, 1)

    def set_helper_status(self, event):
        version = event.get("helper_version", "unknown")
        self._status.setText(
            f"Helper {version} ready: regular-file-only dry runs; hardware writes disabled."
        )

    def set_ramdiag_status(self, status):
        count = len(status.get("artifacts", {}))
        profile = status.get("profile", "unknown")
        self._ramdiag_status.setText(
            f"N25 volatile boot packet installed and checksum-qualified: {count} files, "
            f"profile {profile}, with compiled reset and a 60-second automatic recovery. "
            "Hardware launch remains disabled."
        )

    def set_ramdiag_error(self, message):
        self._ramdiag_status.setText(
            f"N25 volatile boot packet unavailable or rejected: {message}"
        )

    def set_eclair_native_status(self, status):
        count = len(status.get("artifacts", {}))
        tag = status.get("aosp_tag", "unknown")
        self._eclair_native_status.setText(
            f"Android 2.0 native core checksum-qualified: {count} files from {tag}. "
            "Its storage-free ARM926 full-system boot and Binder test passed, including "
            "real Dalvik/framework execution, the official Zygote command-socket accept loop, "
            "system_server, SurfaceFlinger, the Rockpod Launcher, and PixelFlinger graphics. "
            "The N25 framebuffer bridge now includes all four Rockbox Classic panel-init paths "
            "and narrowly allowlisted PCF50635 display power. Its Rockbox-derived click-wheel, "
            "buttons, Hold lockout, Android DPAD key layout, Menu+Select reset chord, watchdog "
            "restart, and three-minute volatile-test timeout are compiled and qualified; "
            "physical LCD output, input, and reset behavior are still untested. "
            "The exact Menu+Play RAM-boot path is packaged; staging does not install it to NOR."
        )
        self._stage_boot.setEnabled(
            status.get("rockbox_menu_play_boot_packaged") is True
            and status.get("rockbox_boot_components_checksum_wrapped") is True
        )

    def set_eclair_native_error(self, message):
        self._stage_boot.setEnabled(False)
        self._eclair_native_status.setText(
            f"Android 2.0 native core unavailable or rejected: {message}"
        )

    def set_stage_result(self, result):
        count = len(result.get("files", ()))
        self._status.setText(
            f"Staged {count} checksum-verified RAM-boot files under "
            f"{result.get('device_directory', '.rockbox/android')}. "
            "Rockbox firmware, database, partition table, and NOR were preserved."
        )

    def set_error(self, message):
        self._status.setText(f"Dry run failed safely: {message}")

    def set_fixture_created(self, event):
        path = event.get("image_path", "")
        self._image_path.setText(path)
        state = "pre-shrunk" if event.get("pre_shrunk") else "unshrunk"
        self._status.setText(f"Created {state} sparse test image. Analyze it to verify the gate.")

    def set_plan(self, plan):
        self._last_plan = plan
        safety = plan["safety"]
        ready = safety.get("ready_for_image_layout_commit") is True
        installed = safety.get("image_layout_installed") is True
        matches = safety.get("image_layout_matches_plan") is True
        if installed and matches:
            headline = "VERIFIED — test image contains the committed Android container layout."
        elif ready:
            headline = "PASS — image reached the layout-commit checkpoint (image only)."
        else:
            headline = "STOP — image is not eligible for a layout commit."
        reasons = safety.get("reasons") or ["No safety reasons reported."]
        self._status.setText(headline + " " + " ".join(reasons))
        self._commit_image.setEnabled(ready)
        self._rollback_image.setEnabled(installed and matches)

        fat = plan["fat32"]
        android = plan["android"]
        rockbox = plan["rockbox_partition"]
        planned = plan["planned_rockbox_partition"]
        lines = [
            headline,
            "",
            f"Image: {plan['image_path']}",
            f"Size: {plan['image_size_bytes']} bytes; logical sector: {plan['logical_sector_size']} bytes",
            f"Identity: {plan['image_identity_sha256']}",
            f"Plan digest: {plan['plan_digest']}",
            f"MBR sector: {plan['mbr_sector_sha256']}",
            f"FAT32 boot: {plan['fat32_boot_sector_sha256']}",
            f"FAT32 FSInfo: {plan['fat32_fsinfo_sector_sha256']}",
            "",
            f"Rockbox current: LBA {rockbox['start_sector']}..{rockbox['end_sector_exclusive'] - 1}",
            f"Rockbox planned: LBA {planned['start_sector']}..{planned['end_sector_exclusive'] - 1}",
            f"Android container: LBA {android['partition_start_sector']}.."
            f"{android['partition_start_sector'] + android['partition_sector_count'] - 1}",
            f"FAT32 label: {fat['volume_label']}; declared sectors: {fat['declared_total_sectors']}",
            f"FSInfo valid: {fat['fsinfo_valid']}; estimated used clusters: {fat['estimated_used_clusters']}",
            "",
            "Safety reasons:",
            *[f"- {reason}" for reason in reasons],
            "",
            "Container regions:",
            *[
                f"- {region['name']}: offset {region['offset_bytes']}, length {region['length_bytes']}, "
                f"runtime-writable={region['writable_at_runtime']}"
                for region in android["regions"]
            ],
            "",
            "Raw protocol:",
            json.dumps(plan, indent=2, sort_keys=True),
        ]
        self._details.setPlainText("\n".join(lines))

    def set_transaction_result(self, event):
        action = event.get("event", "image transaction")
        self._status.setText(
            f"{action} passed metadata and MBR read-back verification; hardware remains locked."
        )

    def _choose_image(self):
        path, _ = QFileDialog.getOpenFileName(
            self,
            "Choose iPod disk image",
            str(Path.home()),
            "Disk images (*.img *.bin);;All files (*)",
        )
        if path:
            self._image_path.setText(path)

    def _choose_fixture(self, pre_shrunk):
        path, _ = QFileDialog.getSaveFileName(
            self,
            "Create sparse iPod test image",
            str(Path.home() / ("ipod6g-preshrunk.img" if pre_shrunk else "ipod6g-unshrunk.img")),
            "Disk images (*.img);;All files (*)",
        )
        if not path:
            return
        self.fixture_requested.emit(
            path,
            self._fixture_size.value(),
            self._android_size.value(),
            int(self._sector_size.currentData()),
            pre_shrunk,
        )

    def _emit_dry_run(self):
        self.dry_run_requested.emit(
            self._image_path.text().strip(),
            self._android_size.value(),
            int(self._sector_size.currentData()),
        )

    def _emit_commit_image(self):
        if not self._last_plan:
            return
        self.commit_image_requested.emit(
            self._image_path.text().strip(),
            self._last_plan.get("plan_digest", ""),
            self._android_size.value(),
            int(self._sector_size.currentData()),
        )

    def _emit_verify_image(self):
        self.verify_image_requested.emit(
            self._image_path.text().strip(),
            self._android_size.value(),
            int(self._sector_size.currentData()),
        )

    def _emit_rollback_image(self):
        self.rollback_image_requested.emit(
            self._image_path.text().strip(),
            self._android_size.value(),
            int(self._sector_size.currentData()),
        )

    def _invalidate_plan(self):
        self._last_plan = None
        if hasattr(self, "_commit_image"):
            self._commit_image.setEnabled(False)
            self._rollback_image.setEnabled(False)
