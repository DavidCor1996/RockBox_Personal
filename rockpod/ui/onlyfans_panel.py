"""RockPod manager for the standalone native OnlyFans application."""

from __future__ import annotations

import os
import re
import threading

from PySide6.QtCore import QObject, QRunnable, QThreadPool, Qt, Signal, Slot
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import (
    QAbstractItemView,
    QCheckBox,
    QFileDialog,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QInputDialog,
    QMessageBox,
    QProgressBar,
    QPushButton,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)


class OnlyFansSignals(QObject):
    progress = Signal(str)
    finished = Signal(object)
    error = Signal(str)


class OnlyFansJob(QRunnable):
    def __init__(self, function, *args, cancellable=False):
        super().__init__()
        self.function = function
        self.args = args
        self.cancellable = cancellable
        self.cancel_event = threading.Event()
        self.signals = OnlyFansSignals()

    def cancel(self):
        self.cancel_event.set()

    @Slot()
    def run(self):
        try:
            keywords = {"progress": self.signals.progress.emit}
            if self.cancellable:
                keywords["cancel_event"] = self.cancel_event
            result = self.function(*self.args, **keywords)
            self.signals.finished.emit(result)
        except Exception as exc:
            self.signals.error.emit(str(exc))


class OnlyFansPanel(QWidget):
    def __init__(self, service, device_provider, parent=None):
        super().__init__(parent)
        self.service = service
        self.device_provider = device_provider
        self._job = None
        self._library_stamp = None

        layout = QVBoxLayout(self)
        title = QLabel("OnlyFans · full-profile download and iPod sync")
        title.setStyleSheet("font-size:15px; font-weight:600; color:#00aff0;")
        layout.addWidget(title)
        note = QLabel(
            "One-time login setup reads your signed-in browser cookies through "
            "OF-Scraper. Imports are creator-specific, incremental and download-only. "
            "Protected media is skipped; RockPod never likes, unlikes or clicks the page."
        )
        note.setWordWrap(True)
        layout.addWidget(note)

        self.show_on_ipod = QCheckBox(
            "Show OnlyFans in Extras → Applications on the iPod"
        )
        self.show_on_ipod.setChecked(
            bool(self.service.config.get("onlyfans_show_on_ipod", True))
        )
        self.show_on_ipod.toggled.connect(self._set_visibility)
        layout.addWidget(self.show_on_ipod)

        row = QHBoxLayout()
        row.addWidget(QLabel("Profile URL:"))
        self.url = QLineEdit()
        self.url.setPlaceholderText("https://onlyfans.com/creator")
        self.url.setText("https://onlyfans.com/cum2play")
        self.url.returnPressed.connect(self.capture)
        self.login_button = QPushButton("Set Up / Refresh Login…")
        self.login_button.clicked.connect(self._setup_login)
        self.capture_button = QPushButton("Import Full Profile")
        self.capture_button.setStyleSheet(
            "background:#00aff0; color:white; font-weight:600;"
        )
        self.capture_button.clicked.connect(self.capture)
        row.addWidget(self.url, 1)
        row.addWidget(self.login_button)
        row.addWidget(self.capture_button)
        layout.addLayout(row)

        my_profile = QGroupBox("My Profile · @cum2play")
        profile_form = QFormLayout(my_profile)
        self.my_display_name = QLineEdit()
        self.my_bio = QLineEdit()
        self.my_avatar = QLineEdit()
        self.my_header = QLineEdit()

        def art_row(field, choose):
            container = QWidget()
            row_layout = QHBoxLayout(container)
            row_layout.setContentsMargins(0, 0, 0, 0)
            row_layout.addWidget(field, 1)
            button = QPushButton("Choose…")
            button.clicked.connect(choose)
            row_layout.addWidget(button)
            return container

        profile_form.addRow("Display name:", self.my_display_name)
        profile_form.addRow("Bio/info:", self.my_bio)
        profile_form.addRow(
            "Profile picture:", art_row(self.my_avatar, self._choose_my_avatar)
        )
        profile_form.addRow(
            "Header:", art_row(self.my_header, self._choose_my_header)
        )
        profile_actions = QHBoxLayout()
        save_my_profile = QPushButton("Save My Profile")
        add_my_post = QPushButton("Add Photo/Video Post…")
        save_my_profile.clicked.connect(self._save_my_profile)
        add_my_post.clicked.connect(self._add_my_post)
        profile_actions.addWidget(save_my_profile)
        profile_actions.addWidget(add_my_post)
        profile_actions.addStretch(1)
        profile_form.addRow(profile_actions)
        layout.addWidget(my_profile)

        body = QHBoxLayout()
        self.table = QTableWidget(0, 5)
        self.table.setHorizontalHeaderLabels(
            ["Profile", "Type", "Post", "Size", "Local file"]
        )
        self.table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.table.setSelectionMode(QAbstractItemView.SingleSelection)
        self.table.horizontalHeader().setStretchLastSection(True)
        self.table.itemSelectionChanged.connect(self._update_preview)
        body.addWidget(self.table, 3)
        preview_column = QVBoxLayout()
        self.logo = QLabel()
        logo = QPixmap(
            str(self.service.repo_root / "rockpod/assets/icons/onlyfans-official.png")
        )
        if not logo.isNull():
            self.logo.setPixmap(
                logo.scaled(180, 64, Qt.KeepAspectRatio, Qt.SmoothTransformation)
            )
        self.logo.setAlignment(Qt.AlignCenter)
        preview_column.addWidget(self.logo)
        self.preview = QLabel("Select captured media")
        self.preview.setAlignment(Qt.AlignCenter)
        self.preview.setMinimumSize(320, 240)
        self.preview.setStyleSheet("background:#111; color:#aaa;")
        preview_column.addWidget(self.preview, 1)
        body.addLayout(preview_column, 2)
        layout.addLayout(body, 1)

        self.progress_bar = QProgressBar()
        self.progress_bar.setTextVisible(True)
        self.progress_bar.setVisible(False)
        self.progress_bar.setStyleSheet(
            "QProgressBar { border:1px solid #8e8e93; border-radius:4px; "
            "text-align:center; background:#f2f2f7; } "
            "QProgressBar::chunk { background:#00aff0; border-radius:3px; }"
        )
        layout.addWidget(self.progress_bar)

        footer = QHBoxLayout()
        self.status = QLabel()
        self.cancel_button = QPushButton("Cancel Import")
        self.cancel_button.setEnabled(False)
        self.cancel_button.clicked.connect(self._cancel_job)
        self.sync_button = QPushButton("Sync OnlyFans to iPod")
        self.sync_button.clicked.connect(self.sync)
        footer.addWidget(self.status, 1)
        footer.addWidget(self.cancel_button)
        footer.addWidget(self.sync_button)
        layout.addLayout(footer)
        self.refresh()

    def _current_library_stamp(self):
        try:
            stat = self.service.index_path.stat()
        except OSError:
            return None
        return stat.st_mtime_ns, stat.st_size

    def refresh(self, force=False):
        stamp = self._current_library_stamp()
        if not force and stamp is not None and stamp == self._library_stamp:
            return

        profiles = self.service.list_profiles()
        rows = []
        for profile in profiles:
            for saved in profile.get("media") or []:
                item = dict(saved)
                item["username"] = profile.get("username") or ""
                item["profile_url"] = profile.get("url") or ""
                rows.append(item)

        # Rebuilding thousands of QTableWidget cells while updates and
        # selection signals are live can monopolise the GUI thread for
        # minutes.  Only column zero needs the complete post record for the
        # preview, so do not duplicate the same dictionary into every cell.
        self.table.setUpdatesEnabled(False)
        self.table.blockSignals(True)
        try:
            self.table.clearContents()
            self.table.setRowCount(len(rows))
            for index, item in enumerate(rows):
                size = ""
                if item.get("width") and item.get("height"):
                    size = f"{item['width']}×{item['height']}"
                values = (
                    "@" + item.get("username", ""),
                    item.get("type", "").title(),
                    item.get("title", ""),
                    size,
                    item.get("source_path", ""),
                )
                for column, value in enumerate(values):
                    cell = QTableWidgetItem(str(value))
                    if column == 0:
                        cell.setData(Qt.UserRole, item)
                    self.table.setItem(index, column, cell)
        finally:
            self.table.blockSignals(False)
            self.table.setUpdatesEnabled(True)

        mine = next(
            (profile for profile in profiles if profile.get("is_my_profile")),
            {},
        )
        self.my_display_name.setText(str(mine.get("display_name") or ""))
        self.my_bio.setText(str(mine.get("bio") or ""))
        self.my_avatar.setText(str(mine.get("avatar_path") or ""))
        self.my_header.setText(str(mine.get("cover_path") or ""))
        self.status.setText(
            f"{len(profiles)} profile(s) · {len(rows)} local post(s)"
        )
        self._library_stamp = self._current_library_stamp()
        if rows and self.table.currentRow() < 0:
            self.table.selectRow(0)

    def _choose_my_avatar(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Choose OnlyFans Profile Picture", "", "Images (*.jpg *.jpeg *.png *.webp *.bmp)"
        )
        if path:
            self.my_avatar.setText(path)

    def _choose_my_header(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Choose OnlyFans Header", "", "Images (*.jpg *.jpeg *.png *.webp *.bmp)"
        )
        if path:
            self.my_header.setText(path)

    def _save_my_profile(self):
        try:
            self.service.save_my_profile(
                "https://onlyfans.com/cum2play",
                self.my_display_name.text(),
                self.my_bio.text(),
                self.my_avatar.text(),
                self.my_header.text(),
            )
        except (OSError, ValueError) as exc:
            QMessageBox.warning(self, "My OnlyFans Profile", str(exc))
            return
        self.refresh()
        self.status.setText("Saved My Profile · ready for local posts")

    def _add_my_post(self):
        source, _ = QFileDialog.getOpenFileName(
            self,
            "Add OnlyFans Post",
            "",
            "Photos and videos (*.jpg *.jpeg *.png *.webp *.bmp *.mp4 *.m4v *.mov *.mkv *.webm *.avi)",
        )
        if not source:
            return
        title, accepted = QInputDialog.getText(
            self, "Post title", "Title:", text=os.path.splitext(os.path.basename(source))[0]
        )
        if not accepted:
            return
        caption, accepted = QInputDialog.getText(
            self, "Post caption", "Caption (optional):"
        )
        if not accepted:
            return
        try:
            self.service.add_my_profile_post(source, title, caption)
        except (OSError, ValueError) as exc:
            QMessageBox.warning(self, "Add OnlyFans Post", str(exc))
            return
        self.refresh()
        self.status.setText("Added a permanent local post to My Profile")

    def _update_preview(self):
        item = self.table.item(self.table.currentRow(), 0)
        row = item.data(Qt.UserRole) if item else {}
        path = row.get("source_path", "") if row else ""
        is_photo = row.get("type") == "photo" if row else False
        pixmap = QPixmap(path) if is_photo and os.path.isfile(path) else QPixmap()
        if pixmap.isNull():
            self.preview.setPixmap(QPixmap())
            self.preview.setText("Video · preview is created during sync")
        else:
            self.preview.setText("")
            self.preview.setPixmap(
                pixmap.scaled(
                    self.preview.size(), Qt.KeepAspectRatio, Qt.SmoothTransformation
                )
            )

    def _set_busy(self, busy):
        self.capture_button.setEnabled(not busy)
        self.login_button.setEnabled(not busy)
        self.sync_button.setEnabled(not busy)
        self.cancel_button.setEnabled(bool(busy and self._job and self._job.cancellable))

    def _begin_progress(self, message):
        self.status.setText(message)
        self.progress_bar.setRange(0, 0)
        self.progress_bar.setFormat("Working…")
        self.progress_bar.setVisible(True)

    def _set_progress_message(self, message):
        message = str(message or "")
        self.status.setText(message)
        matches = re.findall(r"(\d+)\s*/\s*(\d+)", message)
        if not matches:
            self.progress_bar.setRange(0, 0)
            self.progress_bar.setFormat("Working…")
            self.progress_bar.setVisible(True)
            return
        done, total = (int(value) for value in matches[-1])
        if total <= 0:
            return
        self.progress_bar.setRange(0, total)
        self.progress_bar.setValue(min(done, total))
        self.progress_bar.setFormat(f"{done} / {total}   %p%")
        self.progress_bar.setVisible(True)

    def _finish_progress(self, label="Complete"):
        self.progress_bar.setRange(0, 100)
        self.progress_bar.setValue(100)
        self.progress_bar.setFormat(label)
        self.progress_bar.setVisible(True)

    def _setup_login(self):
        url = self.url.text().strip()
        if not url:
            return
        try:
            auth_path = self.service.import_ofscraper_login_from_firefox()
        except (OSError, ValueError) as exc:
            try:
                auth_path = self.service.launch_ofscraper_login(url)
            except (OSError, ValueError) as terminal_exc:
                QMessageBox.warning(
                    self, "OnlyFans Login", f"{exc}\n\n{terminal_exc}"
                )
                return
            self.status.setText(
                "Complete the download-only login in the terminal, then click Import Full Profile"
            )
            return
        self.status.setText(
            "OnlyFans login imported securely from Firefox · ready"
        )
        QMessageBox.information(
            self,
            "OnlyFans Login",
            "The signed-in OnlyFans session was imported from Firefox without "
            "clicking or controlling the page. Credentials remain local in:\n\n"
            f"{auth_path.parent}",
        )

    def _cancel_job(self):
        if self._job and self._job.cancellable:
            self._begin_progress("Cancelling OnlyFans import…")
            self.cancel_button.setEnabled(False)
            self._job.cancel()

    def _set_visibility(self, visible):
        if hasattr(self.service.config, "set"):
            self.service.config.set("onlyfans_show_on_ipod", bool(visible))
        else:
            self.service.config["onlyfans_show_on_ipod"] = bool(visible)
        if hasattr(self.service.config, "save"):
            self.service.config.save()
        device = self.device_provider()
        mount = getattr(device, "mount_path", "") if device else ""
        if mount:
            try:
                self.service.set_device_visibility(mount, bool(visible))
                state = "shown" if visible else "hidden"
                self.status.setText(
                    f"OnlyFans is {state} in Extras → Applications"
                )
            except OSError as exc:
                self.status.setText(
                    f"OnlyFans visibility saved; iPod update failed: {exc}"
                )

    def capture(self):
        url = self.url.text().strip()
        if not url:
            return
        self._set_busy(True)
        self._begin_progress("Starting full-profile scan…")
        self._job = OnlyFansJob(
            self.service.capture_profile, url, cancellable=True
        )
        self._job.signals.progress.connect(self._set_progress_message)
        self._job.signals.finished.connect(self._capture_finished)
        self._job.signals.error.connect(self._failed)
        QThreadPool.globalInstance().start(self._job)

    def _capture_finished(self, report):
        self._set_busy(False)
        self._job = None
        self.refresh()
        self._finish_progress()
        self.status.setText(
            f"Captured {report['photos']} photo(s) and {report['videos']} video(s)"
        )

    def _failed(self, message):
        self._set_busy(False)
        self._job = None
        self.progress_bar.setRange(0, 100)
        self.progress_bar.setValue(0)
        self.progress_bar.setFormat("Failed")
        self.progress_bar.setVisible(True)
        self.status.setText("OnlyFans operation failed")
        QMessageBox.critical(self, "OnlyFans", message)

    def sync(self):
        device = self.device_provider()
        mount = getattr(device, "mount_path", None) if device else None
        if not mount:
            QMessageBox.warning(self, "OnlyFans", "Connect and mount the iPod first.")
            return
        self._set_busy(True)
        self._begin_progress("Preparing iPod-sized media…")
        self._job = OnlyFansJob(self.service.sync, mount)
        self._job.signals.progress.connect(self._set_progress_message)
        self._job.signals.finished.connect(self._sync_finished)
        self._job.signals.error.connect(self._failed)
        QThreadPool.globalInstance().start(self._job)

    def _sync_finished(self, report):
        self._set_busy(False)
        self._job = None
        self._finish_progress()
        self.status.setText(
            f"Synced {report['photos']} photo(s), {report['videos']} video(s); "
            f"{report['unchanged']} video(s) already current"
        )
