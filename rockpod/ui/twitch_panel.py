"""RockPod manager for creator-based offline Twitch channels."""

from __future__ import annotations

import os
from types import SimpleNamespace

from PySide6.QtCore import QObject, QRunnable, QThreadPool, Qt, Signal, Slot
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import (
    QAbstractItemView,
    QDialog,
    QDialogButtonBox,
    QFileDialog,
    QFormLayout,
    QFrame,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPushButton,
    QProgressDialog,
    QSpinBox,
    QTabWidget,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)


TWITCH_PURPLE = "#6441a5"
TWITCH_DARK = "#17141f"


class TwitchJobSignals(QObject):
    progress = Signal(int, int, str)
    finished = Signal(object)
    error = Signal(str)


class TwitchSyncJob(QRunnable):
    def __init__(self, db_path, config, repo_root, mount_path, video_profile):
        super().__init__()
        self.db_path = str(db_path)
        self.config = dict(
            config if isinstance(config, dict) else getattr(config, "_data", {})
        )
        self.repo_root = str(repo_root)
        self.mount_path = str(mount_path)
        self.video_profile = str(video_profile)
        self.signals = TwitchJobSignals()

    @Slot()
    def run(self):
        database = None
        try:
            from app.database import Database
            from services.twitch_app import TwitchAppService

            database = Database(self.db_path)
            service = TwitchAppService(database, self.config, self.repo_root)
            report = service.sync(
                self.mount_path,
                device=SimpleNamespace(mount_path=self.mount_path),
                progress_callback=self.signals.progress.emit,
                video_profile=self.video_profile,
            )
            self.signals.finished.emit(report)
        except Exception as exc:
            self.signals.error.emit(str(exc))
        finally:
            if database is not None:
                database.close()


class TwitchImportJob(QRunnable):
    def __init__(self, db_path, config, repo_root, url, creator_key):
        super().__init__()
        self.db_path = str(db_path)
        self.config = dict(
            config if isinstance(config, dict) else getattr(config, "_data", {})
        )
        self.repo_root = str(repo_root)
        self.url = str(url)
        self.creator_key = str(creator_key or "")
        self.signals = TwitchJobSignals()

    @Slot()
    def run(self):
        database = None
        try:
            from app.database import Database
            from services.twitch_app import TwitchAppService

            database = Database(self.db_path)
            service = TwitchAppService(database, self.config, self.repo_root)
            self.signals.finished.emit(
                service.import_url(self.url, self.creator_key)
            )
        except Exception as exc:
            self.signals.error.emit(str(exc))
        finally:
            if database is not None:
                database.close()


class TwitchCreatorDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Add Twitch Creator")
        form = QFormLayout(self)
        self.url = QLineEdit()
        self.url.setPlaceholderText("https://www.twitch.tv/creator")
        self.name = QLineEdit()
        self.name.setPlaceholderText("Optional display name")
        self.keep = QSpinBox()
        self.keep.setRange(1, 20)
        self.keep.setValue(3)
        form.addRow("Creator URL or login:", self.url)
        form.addRow("Display name:", self.name)
        form.addRow("Keep newest VODs:", self.keep)
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

    def values(self):
        return self.url.text().strip(), self.name.text().strip(), self.keep.value()


class TwitchUrlDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Import Twitch VOD")
        form = QFormLayout(self)
        self.url = QLineEdit()
        self.url.setPlaceholderText("https://www.twitch.tv/videos/123456789")
        form.addRow("Twitch VOD URL:", self.url)
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

    def value(self):
        return self.url.text().strip()


class TwitchPanel(QWidget):
    def __init__(self, service, device_provider, parent=None):
        super().__init__(parent)
        self.service = service
        self.device_provider = device_provider
        self.creators = []
        self.vods = []
        self._sync_job = None
        self._import_job = None
        self._progress = None

        outer = QVBoxLayout(self)
        outer.setContentsMargins(18, 14, 18, 14)
        hero = QFrame()
        hero.setStyleSheet(
            f"QFrame {{ background:{TWITCH_DARK}; border:1px solid #2f2938; "
            "border-radius:5px; }}"
        )
        hero_layout = QHBoxLayout(hero)
        logo = QLabel()
        logo_path = (
            self.service.repo_root / "assets/ipodjs/rockbox/twitch/"
            "twitch-wordmark-current-white.136x50.bmp"
        )
        logo.setPixmap(QPixmap(str(logo_path)))
        copy = QLabel(
            "Offline TwitchTV for iPod · multiple VODs become one live channel per creator"
        )
        copy.setWordWrap(True)
        copy.setStyleSheet("color:#ddd9e3; font-size:13px;")
        hero_layout.addWidget(logo)
        hero_layout.addWidget(copy, 1)
        outer.addWidget(hero)

        self.tabs = QTabWidget()
        self.tabs.addTab(self._build_creators_tab(), "Live Channels")
        self.tabs.addTab(self._build_vods_tab(), "VOD Library")
        outer.addWidget(self.tabs, 1)

        footer = QHBoxLayout()
        self.status = QLabel()
        self.status.setStyleSheet("color:#5d6875;")
        self.sync_button = QPushButton("Sync to iPod (H.264 + MPEG fallback)")
        self.sync_button.setStyleSheet(
            f"background:{TWITCH_PURPLE}; color:white; padding:7px 16px; "
            "font-weight:600; border-radius:4px;"
        )
        self.sync_button.clicked.connect(lambda: self.sync("h264_apple_exact"))
        footer.addWidget(self.status, 1)
        footer.addWidget(self.sync_button)
        outer.addLayout(footer)
        self.refresh()

    def _build_creators_tab(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        copy = QLabel(
            "Each creator is a linear wall-clock channel. RockPod retains several "
            "VODs, while the iPod plays exactly the one that is live now."
        )
        copy.setWordWrap(True)
        copy.setStyleSheet("color:#5d6875;")
        layout.addWidget(copy)
        controls = QHBoxLayout()
        add = QPushButton("Add Creator…")
        remove = QPushButton("Remove Creator")
        restart = QPushButton("Restart Channel Now")
        add.clicked.connect(self.add_creator)
        remove.clicked.connect(self.remove_creator)
        restart.clicked.connect(self.restart_creator)
        controls.addWidget(add)
        controls.addWidget(remove)
        controls.addWidget(restart)
        controls.addStretch(1)
        layout.addLayout(controls)
        self.creator_table = QTableWidget(0, 5)
        self.creator_table.setHorizontalHeaderLabels(
            ["Creator", "VODs", "Keep", "LIVE now", "Channel URL"]
        )
        self.creator_table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.creator_table.setSelectionMode(QAbstractItemView.SingleSelection)
        self.creator_table.setAlternatingRowColors(True)
        layout.addWidget(self.creator_table, 1)
        return page

    def _build_vods_tab(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        controls = QHBoxLayout()
        import_url = QPushButton("Import Twitch VOD URL…")
        local = QPushButton("Add Local VOD…")
        remove = QPushButton("Remove VOD")
        import_url.clicked.connect(self.import_url)
        local.clicked.connect(self.add_local_vod)
        remove.clicked.connect(self.remove_vod)
        controls.addWidget(import_url)
        controls.addWidget(local)
        controls.addWidget(remove)
        controls.addStretch(1)
        layout.addLayout(controls)
        self.vod_table = QTableWidget(0, 8)
        self.vod_table.setHorizontalHeaderLabels(
            ["Title", "Creator", "Game", "Length", "Views", "Published",
             "Chat", "Sync"]
        )
        self.vod_table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.vod_table.setSelectionMode(QAbstractItemView.ExtendedSelection)
        self.vod_table.setAlternatingRowColors(True)
        layout.addWidget(self.vod_table, 1)
        return page

    @staticmethod
    def _duration(milliseconds):
        seconds = max(0, int(milliseconds or 0) // 1000)
        hours, remainder = divmod(seconds, 3600)
        minutes, seconds = divmod(remainder, 60)
        return f"{hours}:{minutes:02d}:{seconds:02d}" if hours else f"{minutes}:{seconds:02d}"

    def _selected_creator_key(self):
        selected = self.creator_table.selectionModel().selectedRows()
        if not selected:
            return ""
        item = self.creator_table.item(selected[0].row(), 0)
        return str(item.data(Qt.UserRole) or "") if item else ""

    def _selected_vod_ids(self):
        return [
            str(self.vod_table.item(index.row(), 0).data(Qt.UserRole))
            for index in self.vod_table.selectionModel().selectedRows()
            if self.vod_table.item(index.row(), 0)
        ]

    def refresh(self):
        self.creators = self.service.list_creators()
        self.vods = self.service.list_vods()
        self.creator_table.setRowCount(len(self.creators))
        for row_index, creator in enumerate(self.creators):
            current = self.service.current_programme(creator["creator_key"])
            live_title = current[0]["title"] if current else "No playable VOD"
            values = [
                creator.get("display_name") or creator["login"],
                creator.get("vod_count") or 0,
                creator["keep_count"],
                live_title,
                creator["channel_url"],
            ]
            for column, value in enumerate(values):
                item = QTableWidgetItem(str(value))
                item.setData(Qt.UserRole, creator["creator_key"])
                self.creator_table.setItem(row_index, column, item)
        self.creator_table.resizeColumnsToContents()

        self.vod_table.setRowCount(len(self.vods))
        for row_index, vod in enumerate(self.vods):
            chat_path, emoji_path = self.service.chat_source_paths(vod)
            values = [
                vod["title"], vod.get("display_name") or vod.get("login"),
                vod.get("game") or "", self._duration(vod["duration_ms"]),
                f"{int(vod.get('view_count') or 0):,}",
                vod.get("published_date") or "",
                "Replay + emoji" if chat_path.is_file() and emoji_path.is_file()
                else "Unavailable",
                "Ready" if os.path.isfile(vod["source_path"]) else "Missing",
            ]
            for column, value in enumerate(values):
                item = QTableWidgetItem(str(value))
                item.setData(Qt.UserRole, vod["id"])
                self.vod_table.setItem(row_index, column, item)
        self.vod_table.resizeColumnsToContents()
        self.status.setText(
            f"{len(self.creators)} live channel{'s' if len(self.creators) != 1 else ''} · "
            f"{len(self.vods)} VOD{'s' if len(self.vods) != 1 else ''}"
        )

    def add_creator(self):
        dialog = TwitchCreatorDialog(self)
        if dialog.exec() != QDialog.Accepted:
            return
        try:
            creator = self.service.add_creator(*dialog.values())
        except (TypeError, ValueError) as exc:
            QMessageBox.warning(self, "Add Twitch Creator", str(exc))
            return
        self.refresh()
        self.status.setText(
            f"Added {creator['display_name'] or creator['login']} · sync to download VODs"
        )

    def remove_creator(self):
        key = self._selected_creator_key()
        if not key:
            return
        if QMessageBox.question(
            self, "Remove Twitch Creator",
            "Remove this creator and its VODs from the Twitch library? "
            "Original downloaded files are kept.",
        ) != QMessageBox.Yes:
            return
        self.service.remove_creator(key)
        self.refresh()

    def restart_creator(self):
        key = self._selected_creator_key()
        if not key:
            return
        from services.twitch_app import _local_epoch_now

        creator = self.service.update_creator(key, cycle_epoch=_local_epoch_now())
        self.refresh()
        self.status.setText(
            f"Restarted {creator['display_name'] or creator['login']} at its first VOD"
        )

    def import_url(self):
        dialog = TwitchUrlDialog(self)
        if dialog.exec() != QDialog.Accepted:
            return
        if self._import_job is not None:
            return
        self._progress = QProgressDialog(
            "Downloading Twitch VOD and metadata…", None, 0, 0, self
        )
        self._progress.setWindowTitle("Import Twitch VOD")
        self._progress.setWindowModality(Qt.WindowModal)
        self._progress.setMinimumDuration(0)
        self._progress.show()
        self._set_buttons_enabled(False)
        self._import_job = TwitchImportJob(
            self.service.db._path, self.service.config, self.service.repo_root,
            dialog.value(), self._selected_creator_key(),
        )
        self._import_job.signals.finished.connect(self._import_finished)
        self._import_job.signals.error.connect(self._job_failed)
        QThreadPool.globalInstance().start(self._import_job)

    def add_local_vod(self):
        key = self._selected_creator_key()
        if not key:
            QMessageBox.information(
                self, "Add Local Twitch VOD",
                "Select a creator in Live Channels first.",
            )
            return
        path, _ = QFileDialog.getOpenFileName(
            self, "Choose Twitch VOD", "",
            "Videos (*.mpg *.mpeg *.mp4 *.m4v *.mov *.avi *.mkv *.webm)",
        )
        if not path:
            return
        try:
            row = self.service.add_vod(path, creator_key=key)
        except ValueError as exc:
            QMessageBox.warning(self, "Add Local Twitch VOD", str(exc))
            return
        self.refresh()
        self.status.setText(f"Added {row['title']}")

    def remove_vod(self):
        ids = self._selected_vod_ids()
        if not ids:
            return
        if QMessageBox.question(
            self, "Remove Twitch VODs",
            f"Remove {len(ids)} selected VOD{'s' if len(ids) != 1 else ''}?",
        ) != QMessageBox.Yes:
            return
        for vod_id in ids:
            self.service.remove_vod(vod_id)
        self.refresh()

    def sync(self, video_profile):
        if self._sync_job is not None or self._import_job is not None:
            return
        device = self.device_provider()
        if device is None or not getattr(device, "mount_path", ""):
            QMessageBox.warning(self, "Twitch Sync", "Connect or mount an iPod first.")
            return
        self._progress = QProgressDialog("Starting Twitch sync…", None, 0, 0, self)
        self._progress.setWindowTitle("Twitch Sync to iPod")
        self._progress.setWindowModality(Qt.WindowModal)
        self._progress.setMinimumDuration(0)
        self._progress.setAutoClose(False)
        self._progress.setAutoReset(False)
        self._progress.show()
        self._set_buttons_enabled(False)
        self._sync_job = TwitchSyncJob(
            self.service.db._path, self.service.config, self.service.repo_root,
            device.mount_path, video_profile,
        )
        self._sync_job.signals.progress.connect(self._sync_status)
        self._sync_job.signals.finished.connect(self._sync_finished)
        self._sync_job.signals.error.connect(self._job_failed)
        QThreadPool.globalInstance().start(self._sync_job)

    def _set_buttons_enabled(self, enabled):
        self.sync_button.setEnabled(enabled)

    def _sync_status(self, done, total, message):
        if self._progress is None:
            return
        self._progress.setRange(0, total if total > 0 else 0)
        if total > 0:
            self._progress.setValue(max(0, min(done, total)))
        self._progress.setLabelText(str(message or "Syncing Twitch…"))
        self.status.setText(str(message or "Syncing Twitch…"))

    def _finish_job(self):
        if self._progress is not None:
            self._progress.close()
            self._progress.deleteLater()
        self._progress = None
        self._sync_job = None
        self._import_job = None
        self._set_buttons_enabled(True)

    def _import_finished(self, row):
        self._finish_job()
        self.refresh()
        self.status.setText(f"Imported {row['title']} · sync Twitch when ready")

    def _sync_finished(self, report):
        self._finish_job()
        self.refresh()
        errors = list(report.get("errors") or [])
        details = ""
        if errors:
            details = "\n\nSkipped/errors:\n" + "\n".join(errors[:6])
            if len(errors) > 6:
                details += f"\n…and {len(errors) - 6} more"
        message = (
            f"Creators: {report['creators']}\n"
            f"VODs: {report['vods']}\n"
            f"Playable VODs exported: {report.get('vods_exported', 0)}\n"
            f"Downloaded this sync: {report['vods_downloaded']}\n"
            f"Download failures skipped: {report.get('download_failures', 0)}\n"
            f"Chat replays downloaded: {report['chat_downloaded']}\n"
            f"Chat/emoji sidecars updated: {report['chat_updated']}\n"
            f"New/updated media: {report['media_updated']}\n"
            f"H.264 items recovered as MPEG: {report.get('mpeg_fallbacks', 0)}\n"
            f"MPEG device fallbacks updated: {report.get('mpeg_backups_updated', 0)}\n"
            f"MPEG device fallbacks unchanged: {report.get('mpeg_backups_unchanged', 0)}\n"
            f"MPEG fallback failures: {report.get('mpeg_backup_failures', 0)}\n"
            f"Media conversions skipped: {report.get('media_failed', 0)}\n"
            f"Previous playable files preserved: {report.get('media_preserved', 0)}\n"
            f"Unchanged media: {report['media_unchanged']}\n"
            f"Authentic assets updated: {report['assets_updated']}"
            + details
        )
        if errors:
            self.status.setText(
                f"Twitch sync completed with {len(errors)} skipped item(s)"
            )
            QMessageBox.warning(
                self, "Twitch Sync Complete with Skipped Items", message
            )
        else:
            QMessageBox.information(self, "Twitch Sync Complete", message)

    def _job_failed(self, message):
        self._finish_job()
        self.status.setText("Twitch operation failed")
        QMessageBox.warning(self, "Twitch", str(message))
