"""RockPod manager for the standalone native TikTok feed."""

from __future__ import annotations

import os
import subprocess
from types import SimpleNamespace

from PySide6.QtCore import QObject, QRunnable, QRect, QThreadPool, Qt, Signal, Slot
from PySide6.QtGui import QColor, QPainter, QPixmap
from PySide6.QtWidgets import (
    QAbstractItemView,
    QDialog,
    QDialogButtonBox,
    QFileDialog,
    QFormLayout,
    QHeaderView,
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
    QTextEdit,
    QVBoxLayout,
    QWidget,
)


class TikTokArchiveSignals(QObject):
    progress = Signal(str)
    finished = Signal(object)
    error = Signal(str)


class TikTokArchiveJob(QRunnable):
    """Archive on a dedicated SQLite connection so RockPod stays responsive."""

    def __init__(self, db_path, config, repo_root, account_url):
        super().__init__()
        self.db_path = db_path
        # Config deliberately is not a Mapping. Snapshot its values before
        # crossing into the worker thread; dict(config) raises TypeError and
        # previously left an indeterminate progress dialog with no job.
        self.config = dict(
            config if isinstance(config, dict) else getattr(config, "_data", {})
        )
        self.repo_root = str(repo_root)
        self.account_url = account_url
        self.signals = TikTokArchiveSignals()

    @Slot()
    def run(self):
        database = None
        try:
            from app.database import Database
            from services.tiktok_app import TikTokAppService

            database = Database(self.db_path)
            service = TikTokAppService(database, self.config, self.repo_root)
            report = service.prepare_account_archive(
                self.account_url, progress_callback=self.signals.progress.emit
            )
            self.signals.finished.emit(report)
        except Exception as exc:
            self.signals.error.emit(str(exc))
        finally:
            if database is not None:
                database.close()


class TikTokSyncSignals(QObject):
    progress = Signal(int, int, str)
    finished = Signal(object)
    error = Signal(str)


class TikTokSyncJob(QRunnable):
    """Run downloads, conversion, and iPod writes without blocking RockPod."""

    def __init__(self, db_path, config, repo_root, mount_path):
        super().__init__()
        self.db_path = db_path
        self.config = dict(
            config if isinstance(config, dict) else getattr(config, "_data", {})
        )
        self.repo_root = str(repo_root)
        self.mount_path = str(mount_path)
        self.signals = TikTokSyncSignals()

    @Slot()
    def run(self):
        database = None
        try:
            from app.database import Database
            from services.tiktok_app import TikTokAppService

            database = Database(self.db_path)
            service = TikTokAppService(database, self.config, self.repo_root)
            report = service.sync(
                self.mount_path,
                device=SimpleNamespace(mount_path=self.mount_path),
                progress_callback=self.signals.progress.emit,
            )
            self.signals.finished.emit(report)
        except Exception as exc:
            self.signals.error.emit(str(exc))
        finally:
            if database is not None:
                database.close()


class TikTokPreview(QWidget):
    """RockPod preview of the 320x240 feed and its preserved video aspect."""

    def __init__(self, logo_path, heart_path, parent=None):
        super().__init__(parent)
        self.logo = QPixmap(str(logo_path))
        self.heart = QPixmap(str(heart_path))
        self.row = {}
        self.setMinimumSize(480, 360)

    def update_data(self, row=None):
        self.row = dict(row or {})
        self.update()

    def paintEvent(self, _event):
        painter = QPainter(self)
        scale = min(self.width() / 320.0, self.height() / 240.0)
        painter.translate(
            (self.width() - 320 * scale) / 2,
            (self.height() - 240 * scale) / 2,
        )
        painter.scale(scale, scale)
        painter.fillRect(0, 0, 320, 240, QColor("black"))

        thumbnail_path = self.row.get("thumbnail_path") or ""
        thumbnail = QPixmap(thumbnail_path) if os.path.isfile(thumbnail_path) else QPixmap()
        if not thumbnail.isNull():
            fitted = thumbnail.scaled(
                136,
                240,
                Qt.KeepAspectRatio,
                Qt.SmoothTransformation,
            )
            painter.drawPixmap(
                92 + (136 - fitted.width()) // 2,
                (240 - fitted.height()) // 2,
                fitted,
            )

        painter.fillRect(0, 0, 92, 240, QColor("#08080a"))
        painter.fillRect(228, 0, 92, 240, QColor("#08080a"))
        painter.fillRect(91, 0, 1, 240, QColor("#303034"))
        painter.fillRect(228, 0, 1, 240, QColor("#303034"))
        if not self.logo.isNull():
            painter.drawPixmap(QRect(8, 10, 76, 18), self.logo, QRect(8, 11, 76, 18))
        painter.setPen(QColor("#aeb3bf"))
        painter.drawText(6, 70, "Following")
        painter.drawText(6, 84, "Saved")
        painter.drawText(6, 98, "History")
        painter.setPen(QColor("white"))
        painter.drawText(6, 56, "For You")
        painter.fillRect(6, 59, 43, 2, QColor("#25f4ee"))

        painter.setPen(QColor("white"))
        creator = str(self.row.get("creator") or "@creator")
        metrics = painter.fontMetrics()
        painter.drawText(6, 133, metrics.elidedText(creator, Qt.ElideRight, 80))
        caption = str(
            self.row.get("description") or self.row.get("title") or "TikTok"
        )
        painter.setPen(QColor("#bebec4"))
        painter.drawText(6, 151, metrics.elidedText(caption, Qt.ElideRight, 80))
        painter.setPen(QColor("#aeb3bf"))
        painter.drawText(234, 28, "PLAYING")
        if not self.heart.isNull():
            painter.drawPixmap(258, 56, 32, 32, self.heart)
        painter.setPen(QColor("white"))
        painter.drawText(265, 104, str(int(self.row.get("like_count") or 0)))
        painter.setPen(QColor("#aeb3bf"))
        painter.drawText(244, 122, "Likes")
        painter.drawText(240, 148, "Comments")
        painter.setPen(QColor("white"))
        painter.drawText(268, 165, str(int(self.row.get("comment_count") or 0)))
        painter.end()


class TikTokUrlDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Import TikTok URL")
        form = QFormLayout(self)
        self.url = QLineEdit()
        self.url.setPlaceholderText("https://www.tiktok.com/@creator/video/…")
        form.addRow("TikTok URL:", self.url)
        buttons = QDialogButtonBox(QDialogButtonBox.Import | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

    def value(self):
        return self.url.text().strip()


class TikTokAccountDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Follow TikTok Account")
        form = QFormLayout(self)
        self.url = QLineEdit()
        self.url.setPlaceholderText("https://www.tiktok.com/@creator")
        form.addRow("Account URL:", self.url)
        note = QLabel(
            "RockPod keeps the newest 10 posts from this account. Older "
            "followed posts are removed on sync; manual imports are never pruned."
        )
        note.setWordWrap(True)
        form.addRow(note)
        buttons = QDialogButtonBox(QDialogButtonBox.Save | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

    def value(self):
        return self.url.text().strip()


class TikTokMetadataDialog(QDialog):
    def __init__(self, row, parent=None):
        super().__init__(parent)
        self.setWindowTitle("TikTok Metadata")
        self.setMinimumWidth(540)
        row = dict(row or {})
        form = QFormLayout(self)
        self.title = QLineEdit(row.get("title") or "")
        self.creator = QLineEdit(row.get("creator") or "")
        self.description = QTextEdit(row.get("description") or "")
        self.description.setMaximumHeight(100)
        self.upload_date = QLineEdit(row.get("upload_date") or "")
        self.likes = QSpinBox()
        self.likes.setRange(0, 2_000_000_000)
        self.likes.setValue(int(row.get("like_count") or 0))
        self.comments = QSpinBox()
        self.comments.setRange(0, 2_000_000_000)
        self.comments.setValue(int(row.get("comment_count") or 0))
        self.thumbnail = QLineEdit(row.get("thumbnail_path") or "")
        choose = QPushButton("Choose…")
        choose.clicked.connect(self.choose_thumbnail)
        thumb_row = QHBoxLayout()
        thumb_row.addWidget(self.thumbnail, 1)
        thumb_row.addWidget(choose)
        form.addRow("Caption:", self.title)
        form.addRow("Creator:", self.creator)
        form.addRow("Description:", self.description)
        form.addRow("Posted:", self.upload_date)
        form.addRow("Likes:", self.likes)
        form.addRow("Comments:", self.comments)
        form.addRow("Thumbnail:", thumb_row)
        buttons = QDialogButtonBox(QDialogButtonBox.Save | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

    def choose_thumbnail(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Choose TikTok Thumbnail", "", "Images (*.png *.jpg *.jpeg *.bmp)"
        )
        if path:
            self.thumbnail.setText(path)

    def values(self):
        return {
            "title": self.title.text(),
            "creator": self.creator.text(),
            "description": self.description.toPlainText(),
            "upload_date": self.upload_date.text(),
            "like_count": self.likes.value(),
            "comment_count": self.comments.value(),
            "thumbnail_path": self.thumbnail.text(),
        }


class TikTokPanel(QWidget):
    def __init__(self, service, device_provider, parent=None):
        super().__init__(parent)
        self.service = service
        self.device_provider = device_provider
        self.rows = []
        self._archive_job = None
        self._archive_progress = None
        self._sync_job = None
        self._sync_progress = None

        layout = QVBoxLayout(self)
        intro = QLabel(
            "Native offline TikTok feed · wheel swipes · double-Select likes · "
            "Left changes feed · Right opens profile · hold Menu for actions"
        )
        intro.setStyleSheet("font-size:15px; font-weight:600;")
        layout.addWidget(intro)
        import_row = QHBoxLayout()
        import_row.addWidget(QLabel("TikTok video URL:"))
        self.url_input = QLineEdit()
        self.url_input.setPlaceholderText(
            "https://www.tiktok.com/@creator/video/…"
        )
        self.url_input.returnPressed.connect(self.import_url_from_field)
        import_button = QPushButton("Import URL")
        import_button.setStyleSheet(
            "background:#25f4ee; color:#111; font-weight:600;"
        )
        import_button.clicked.connect(self.import_url_from_field)
        import_row.addWidget(self.url_input, 1)
        import_row.addWidget(import_button)
        layout.addLayout(import_row)
        self.tabs = QTabWidget()
        self.tabs.addTab(self._build_library_tab(), "For You / Manual")
        self.tabs.addTab(self._build_following_tab(), "Following")
        self.tabs.addTab(self._build_archive_tab(), "Archive")
        self.tabs.addTab(self._build_profiles_tab(), "Profiles")
        layout.addWidget(self.tabs, 1)
        footer = QHBoxLayout()
        self.status = QLabel()
        self.sync_button = QPushButton("Sync TikTok to iPod")
        self.sync_button.setStyleSheet(
            "background:#fe2c55; color:white; font-weight:600;"
        )
        self.sync_button.clicked.connect(self.sync)
        footer.addWidget(self.status, 1)
        footer.addWidget(self.sync_button)
        layout.addLayout(footer)
        self.refresh()

    def _build_library_tab(self):
        page = QWidget()
        layout = QHBoxLayout(page)
        left = QVBoxLayout()
        controls = QHBoxLayout()
        add = QPushButton("Add Video")
        import_url = QPushButton("Import TikTok URL…")
        edit = QPushButton("Edit Metadata")
        remove = QPushButton("Remove")
        add.clicked.connect(self.add_video)
        import_url.clicked.connect(self.import_url)
        edit.clicked.connect(self.edit_selected)
        remove.clicked.connect(self.remove_selected)
        for button in (add, import_url, edit, remove):
            controls.addWidget(button)
        controls.addStretch(1)
        left.addLayout(controls)
        self.table = QTableWidget(0, 7)
        self.table.setHorizontalHeaderLabels(
            ["Caption", "Creator", "Source", "Posted", "Likes", "Length", "Sync"]
        )
        self.table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.table.setSelectionMode(QAbstractItemView.ExtendedSelection)
        self.table.horizontalHeader().setSectionResizeMode(
            QHeaderView.Interactive
        )
        self.table.horizontalHeader().setSectionResizeMode(
            0, QHeaderView.Stretch
        )
        for column, width in enumerate((240, 120, 90, 90, 80, 70, 70)):
            if column:
                self.table.setColumnWidth(column, width)
        self.table.doubleClicked.connect(self.edit_selected)
        self.table.itemSelectionChanged.connect(self.update_preview)
        left.addWidget(self.table, 1)
        layout.addLayout(left, 3)
        self.preview = TikTokPreview(
            self.service.repo_root
            / "assets/ipodjs/rockbox/tiktok/tiktok-header-official.320x40.bmp",
            self.service.repo_root
            / "assets/ipodjs/rockbox/tiktok/tiktok-heart-liked.32x32.bmp",
        )
        layout.addWidget(self.preview, 2)
        return page

    def _build_following_tab(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        note = QLabel(
            "Each followed account contributes its newest 10 TikToks. On every "
            "sync, new posts replace older followed posts on the iPod. Manual "
            "imports remain permanently until you remove them."
        )
        note.setWordWrap(True)
        layout.addWidget(note)
        controls = QHBoxLayout()
        add = QPushButton("Follow Account…")
        remove = QPushButton("Stop Following")
        refresh = QPushButton("Refresh Followed Accounts")
        add.clicked.connect(self.add_account)
        remove.clicked.connect(self.remove_account)
        refresh.clicked.connect(self.refresh_accounts)
        controls.addWidget(add)
        controls.addWidget(remove)
        controls.addWidget(refresh)
        controls.addStretch(1)
        layout.addLayout(controls)
        account_row = QHBoxLayout()
        account_row.addWidget(QLabel("Account URL:"))
        self.account_url_input = QLineEdit()
        self.account_url_input.setPlaceholderText(
            "https://www.tiktok.com/@creator"
        )
        self.account_url_input.returnPressed.connect(self.add_account_from_field)
        account_button = QPushButton("Follow + fetch profile")
        account_button.clicked.connect(self.add_account_from_field)
        account_row.addWidget(self.account_url_input, 1)
        account_row.addWidget(account_button)
        layout.addLayout(account_row)
        self.account_table = QTableWidget(0, 7)
        self.account_table.setHorizontalHeaderLabels(
            ["Account", "Display name", "Followers", "Likes", "Videos", "Retention", "URL"]
        )
        self.account_table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.account_table.setSelectionMode(QAbstractItemView.SingleSelection)
        layout.addWidget(self.account_table, 1)
        return page

    def _build_archive_tab(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        note = QLabel(
            "Full profile archives are permanent and separate from Following. "
            "Preparing again adds new public posts without deleting older ones."
        )
        note.setWordWrap(True)
        layout.addWidget(note)
        row = QHBoxLayout()
        row.addWidget(QLabel("Profile or video URL:"))
        self.archive_url_input = QLineEdit()
        self.archive_url_input.setPlaceholderText(
            "https://www.tiktok.com/@creator (or one of their video URLs)"
        )
        self.archive_url_input.returnPressed.connect(self.archive_account_from_field)
        prepare = QPushButton("Prepare Full Profile Archive")
        prepare.clicked.connect(self.archive_account_from_field)
        row.addWidget(self.archive_url_input, 1)
        row.addWidget(prepare)
        layout.addLayout(row)
        self.archive_account_table = QTableWidget(0, 8)
        self.archive_account_table.setHorizontalHeaderLabels(
            [
                "Account", "Display name", "Followers", "Profile videos",
                "Archived", "Remaining", "Local size", "URL",
            ]
        )
        self.archive_account_table.setSelectionBehavior(QAbstractItemView.SelectRows)
        layout.addWidget(self.archive_account_table)
        self.archive_table = QTableWidget(0, 6)
        self.archive_table.setHorizontalHeaderLabels(
            ["Caption", "Creator", "Posted", "Likes", "Length", "Profile URL"]
        )
        self.archive_table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.archive_table.horizontalHeader().setSectionResizeMode(
            QHeaderView.Interactive
        )
        self.archive_table.horizontalHeader().setSectionResizeMode(
            0, QHeaderView.Stretch
        )
        for column, width in enumerate((260, 120, 90, 80, 70, 220)):
            if column:
                self.archive_table.setColumnWidth(column, width)
        layout.addWidget(self.archive_table, 1)
        return page

    def _build_profiles_tab(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        note = QLabel(
            "Real creator metadata captured from imported videos and refreshed "
            "accounts. Fields remain blank when TikTok does not expose them."
        )
        note.setWordWrap(True)
        layout.addWidget(note)
        self.profile_table = QTableWidget(0, 9)
        self.profile_table.setHorizontalHeaderLabels(
            [
                "Username", "Display name", "Bio", "Followers", "Following",
                "Likes", "Videos", "Followed", "Profile URL",
            ]
        )
        self.profile_table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.profile_table.setSelectionMode(QAbstractItemView.SingleSelection)
        layout.addWidget(self.profile_table, 1)
        return page

    @staticmethod
    def _duration(milliseconds):
        seconds = max(0, int(milliseconds or 0) // 1000)
        return f"{seconds // 60}:{seconds % 60:02d}"

    @staticmethod
    def _size(value):
        amount = float(max(0, int(value or 0)))
        for suffix in ("B", "KiB", "MiB", "GiB"):
            if amount < 1024 or suffix == "GiB":
                return f"{amount:.1f} {suffix}" if suffix != "B" else f"{int(amount)} B"
            amount /= 1024

    def refresh(self):
        self.rows = self.service.list_videos()
        self.table.setUpdatesEnabled(False)
        self.table.setRowCount(len(self.rows))
        for index, row in enumerate(self.rows):
            values = [
                row["title"],
                row["creator"],
                {
                    "manual": "Manual",
                    "following": "Following",
                    "archive": "Archived profile",
                }.get(row["source_type"], row["source_type"]),
                row["upload_date"],
                f"{int(row['like_count'] or 0):,}",
                self._duration(row["duration_ms"]),
                "Ready" if os.path.isfile(row["source_path"]) else "Missing",
            ]
            for column, value in enumerate(values):
                item = QTableWidgetItem(str(value))
                item.setData(Qt.UserRole, row["id"])
                self.table.setItem(index, column, item)
        self.table.setUpdatesEnabled(True)
        accounts = [
            account for account in self.service.list_account_syncs()
            if account.get("sync_mode") != "archive"
        ]
        self.account_table.setRowCount(len(accounts))
        for index, account in enumerate(accounts):
            values = [
                "@" + (account.get("username") or "")
                if account.get("username")
                else account.get("account_name") or "TikTok account",
                account.get("display_name") or "",
                f"{int(account.get('follower_count') or 0):,}",
                f"{int(account.get('likes_count') or 0):,}",
                f"{int(account.get('video_count') or 0):,}",
                "Full archive" if account.get("sync_mode") == "archive"
                else "Newest 10 posts",
                account["account_url"],
            ]
            for column, value in enumerate(values):
                item = QTableWidgetItem(value)
                item.setData(Qt.UserRole, account["account_url"])
                self.account_table.setItem(index, column, item)
        self.account_table.resizeColumnsToContents()
        archive_rows = [row for row in self.rows if row["source_type"] == "archive"]
        archive_accounts = [
            account for account in self.service.list_account_syncs()
            if account.get("sync_mode") == "archive"
        ]
        archive_counts = {}
        for row in archive_rows:
            archive_counts[row.get("account_url") or ""] = (
                archive_counts.get(row.get("account_url") or "", 0) + 1
            )
        self.archive_account_table.setRowCount(len(archive_accounts))
        for index, account in enumerate(archive_accounts):
            estimate = self.service.archive_storage_estimate(account["account_url"])
            values = [
                "@" + (account.get("username") or "")
                if account.get("username") else account.get("account_name") or "",
                account.get("display_name") or "",
                f"{int(account.get('follower_count') or 0):,}",
                f"{int(account.get('video_count') or 0):,}",
                f"{archive_counts.get(account['account_url'], 0):,}",
                f"{estimate['remaining']:,}" if estimate["total"] else "Unknown",
                self._size(estimate["downloaded_bytes"]),
                account["account_url"],
            ]
            for column, value in enumerate(values):
                self.archive_account_table.setItem(
                    index, column, QTableWidgetItem(str(value))
                )
        self.archive_account_table.resizeColumnsToContents()
        self.archive_table.setUpdatesEnabled(False)
        self.archive_table.setRowCount(len(archive_rows))
        for index, row in enumerate(archive_rows):
            values = [
                row["title"], row["creator"], row["upload_date"],
                f"{int(row['like_count'] or 0):,}",
                self._duration(row["duration_ms"]), row["account_url"],
            ]
            for column, value in enumerate(values):
                item = QTableWidgetItem(str(value))
                item.setData(Qt.UserRole, row["id"])
                self.archive_table.setItem(index, column, item)
        self.archive_table.setUpdatesEnabled(True)
        profiles = self.service.list_profiles()
        self.profile_table.setRowCount(len(profiles))
        for index, profile in enumerate(profiles):
            values = [
                "@" + profile["username"] if profile["username"] else "",
                profile["display_name"],
                profile["bio"],
                f"{int(profile['follower_count'] or 0):,}",
                f"{int(profile['following_count'] or 0):,}",
                f"{int(profile['likes_count'] or 0):,}",
                f"{int(profile['video_count'] or 0):,}",
                "Yes" if profile["followed"] else "No",
                profile["account_url"],
            ]
            for column, value in enumerate(values):
                item = QTableWidgetItem(str(value))
                item.setData(Qt.UserRole, profile["account_url"])
                self.profile_table.setItem(index, column, item)
        self.profile_table.resizeColumnsToContents()
        self.preview.update_data(self.rows[0] if self.rows else None)
        manual = sum(row["source_type"] == "manual" for row in self.rows)
        followed = sum(row["source_type"] == "following" for row in self.rows)
        archived = sum(row["source_type"] == "archive" for row in self.rows)
        self.status.setText(
            f"{manual} manual · {followed} following · {archived} archived TikToks"
        )

    def _selected_ids(self):
        return sorted(
            {
                self.table.item(index.row(), 0).data(Qt.UserRole)
                for index in self.table.selectionModel().selectedRows()
                if self.table.item(index.row(), 0)
            }
        )

    def update_preview(self):
        ids = self._selected_ids()
        row = next((row for row in self.rows if row["id"] in ids), None)
        self.preview.update_data(row or (self.rows[0] if self.rows else None))

    def add_video(self):
        path, _ = QFileDialog.getOpenFileName(
            self,
            "Add Manual TikTok",
            "",
            "Videos (*.mpg *.mpeg *.mp4 *.m4v *.mov *.avi *.mkv *.webm)",
        )
        if not path:
            return
        try:
            row = self.service.add_video(path, source_type="manual")
        except (OSError, ValueError) as exc:
            QMessageBox.warning(self, "Add TikTok", str(exc))
            return
        dialog = TikTokMetadataDialog(row, self)
        if dialog.exec() == QDialog.Accepted:
            self.service.update_video(row["id"], dialog.values())
        self.refresh()

    def import_url(self):
        dialog = TikTokUrlDialog(self)
        if dialog.exec() != QDialog.Accepted:
            return
        self.status.setText("Downloading TikTok and metadata…")
        self.repaint()
        try:
            row = self.service.import_url(dialog.value())
        except (OSError, ValueError, subprocess.SubprocessError) as exc:
            QMessageBox.warning(self, "Import TikTok URL", str(exc))
            self.status.setText("TikTok import failed")
            return
        self.refresh()
        self.status.setText(f"Imported {row['title']} · ready to sync")

    def import_url_from_field(self):
        url = self.url_input.text().strip()
        if not url:
            self.url_input.setFocus()
            return
        self.status.setText("Downloading TikTok, thumbnail, and profile metadata…")
        self.repaint()
        try:
            row = self.service.import_url(url)
        except (OSError, ValueError, subprocess.SubprocessError) as exc:
            QMessageBox.warning(self, "Import TikTok URL", str(exc))
            self.status.setText("TikTok import failed")
            return
        self.url_input.clear()
        self.refresh()
        self.status.setText(f"Imported {row['title']} · manual item will be kept")

    def edit_selected(self):
        ids = self._selected_ids()
        if len(ids) != 1:
            QMessageBox.information(self, "Edit TikTok", "Select one TikTok to edit.")
            return
        row = self.service.get_video(ids[0])
        dialog = TikTokMetadataDialog(row, self)
        if dialog.exec() == QDialog.Accepted:
            self.service.update_video(ids[0], dialog.values())
            self.refresh()

    def remove_selected(self):
        ids = self._selected_ids()
        if not ids:
            return
        manual = [
            row for row in self.rows if row["id"] in ids and row["source_type"] == "manual"
        ]
        text = (
            "Remove the selected TikToks from the feed? "
            + ("Manual source files are kept." if manual else "")
        )
        if QMessageBox.question(self, "Remove TikTok", text) != QMessageBox.Yes:
            return
        for video_id in ids:
            self.service.remove_video(video_id)
        self.refresh()

    def add_account(self):
        dialog = TikTokAccountDialog(self)
        if dialog.exec() != QDialog.Accepted:
            return
        try:
            account = self.service.add_account_sync(dialog.value())
        except ValueError as exc:
            QMessageBox.warning(self, "Follow TikTok Account", str(exc))
            return
        self.refresh()
        self.status.setText(f"Following {account['account_name']} · newest 10 retained")

    def add_account_from_field(self):
        url = self.account_url_input.text().strip()
        if not url:
            self.account_url_input.setFocus()
            return
        self.status.setText("Adding followed account and fetching profile…")
        self.repaint()
        try:
            account = self.service.add_account_sync(url)
            self.service.refresh_account_profile(account["account_url"])
        except (OSError, ValueError, subprocess.SubprocessError) as exc:
            QMessageBox.warning(self, "Follow TikTok Account", str(exc))
            return
        self.account_url_input.clear()
        self.refresh()
        self.status.setText(
            f"Following {account['account_name']} · use Refresh to fetch newest 10"
        )

    def remove_account(self):
        selected = self.account_table.selectionModel().selectedRows()
        if not selected:
            return
        account_url = self.account_table.item(selected[0].row(), 0).data(Qt.UserRole)
        if QMessageBox.question(
            self,
            "Stop Following",
            "Stop refreshing this account? Its current downloaded posts remain "
            "until the next rolling sync or manual removal.",
        ) != QMessageBox.Yes:
            return
        self.service.remove_account_sync(account_url)
        self.refresh()

    def archive_account_from_field(self):
        url = self.archive_url_input.text().strip()
        if not url:
            self.archive_url_input.setFocus()
            return
        try:
            estimate = self.service.archive_storage_estimate(url)
        except (OSError, ValueError) as exc:
            QMessageBox.warning(self, "TikTok Account Archive", str(exc))
            return
        if (
            estimate["estimated_remaining_bytes"]
            and estimate["estimated_remaining_bytes"] > estimate["free_bytes"]
        ):
            QMessageBox.warning(
                self,
                "TikTok Account Archive",
                "The full archive is estimated to need "
                f"{self._size(estimate['estimated_remaining_bytes'])}, but only "
                f"{self._size(estimate['free_bytes'])} is free.",
            )
            return
        if estimate["estimated_remaining_bytes"]:
            answer = QMessageBox.question(
                self,
                "Prepare Full Profile Archive",
                f"Resume this archive with {estimate['remaining']:,} posts "
                f"remaining (about {self._size(estimate['estimated_remaining_bytes'])})?\n\n"
                "Existing media will be verified and skipped; it will not be downloaded again.",
            )
            if answer != QMessageBox.Yes:
                return

        self.status.setText("Preparing resumable full account archive locally…")
        self._archive_progress = QProgressDialog(
            "Reading profile and resuming archive…", None, 0, 0, self
        )
        self._archive_progress.setWindowTitle("TikTok Account Archive")
        self._archive_progress.setMinimumDuration(0)
        self._archive_progress.show()
        try:
            self._archive_job = TikTokArchiveJob(
                self.service.db._path,
                self.service.config,
                self.service.repo_root,
                url,
            )
            self._archive_job.signals.progress.connect(self._archive_status)
            self._archive_job.signals.finished.connect(self._archive_finished)
            self._archive_job.signals.error.connect(self._archive_failed)
            QThreadPool.globalInstance().start(self._archive_job)
        except Exception as exc:
            self._archive_failed(str(exc))

    def _archive_status(self, line):
        if not line or self._archive_progress is None:
            return
        if line.startswith("[archive]"):
            self._archive_progress.setLabelText(line[len("[archive]"):].strip())
            self.status.setText("TikTok archive: " + line[len("[archive]"):].strip())

    def _archive_finished(self, report):
        if self._archive_progress is not None:
            self._archive_progress.close()
        self._archive_progress = None
        self._archive_job = None
        self.archive_url_input.clear()
        self.refresh()
        self.status.setText(
            f"Prepared {report['videos']} account TikToks locally · ready for iPod sync"
        )

    def _archive_failed(self, message):
        if self._archive_progress is not None:
            self._archive_progress.close()
        self._archive_progress = None
        self._archive_job = None
        QMessageBox.warning(self, "TikTok Account Archive", message)
        self.status.setText("TikTok archive preparation failed")

    def refresh_accounts(self):
        self.status.setText("Refreshing the newest 10 posts from followed accounts…")
        self.repaint()
        try:
            report = self.service.sync_account_uploads()
        except (OSError, ValueError, subprocess.SubprocessError) as exc:
            QMessageBox.warning(self, "Refresh TikTok Accounts", str(exc))
            self.status.setText("Followed account refresh failed")
            return
        self.refresh()
        self.status.setText(
            f"Refreshed {report['accounts']} accounts · "
            f"pruned {report['videos_removed']} older posts · "
            f"{report.get('errors', 0)} unavailable skipped"
        )

    def sync(self):
        if self._sync_job is not None:
            return
        device = self.device_provider()
        if device is None or not getattr(device, "mount_path", ""):
            QMessageBox.warning(self, "TikTok Sync", "Connect or mount an iPod first.")
            return
        self._sync_progress = QProgressDialog(
            "Starting TikTok sync…", None, 0, 0, self
        )
        self._sync_progress.setWindowTitle("Sync TikTok to iPod")
        self._sync_progress.setWindowModality(Qt.WindowModal)
        self._sync_progress.setMinimumDuration(0)
        self._sync_progress.setAutoClose(False)
        self._sync_progress.setAutoReset(False)
        self._sync_progress.show()
        self.sync_button.setEnabled(False)
        self.status.setText("TikTok sync starting…")
        try:
            self._sync_job = TikTokSyncJob(
                self.service.db._path,
                self.service.config,
                self.service.repo_root,
                device.mount_path,
            )
            self._sync_job.signals.progress.connect(self._sync_status)
            self._sync_job.signals.finished.connect(self._sync_finished)
            self._sync_job.signals.error.connect(self._sync_failed)
            QThreadPool.globalInstance().start(self._sync_job)
        except Exception as exc:
            self._sync_failed(str(exc))

    def _sync_status(self, done, total, message):
        if self._sync_progress is None:
            return
        if total > 0:
            self._sync_progress.setRange(0, total)
            self._sync_progress.setValue(max(0, min(done, total)))
        else:
            self._sync_progress.setRange(0, 0)
        label = str(message or "Syncing TikTok…")
        self._sync_progress.setLabelText(label)
        self.status.setText(label)

    def _finish_sync_ui(self):
        if self._sync_progress is not None:
            self._sync_progress.close()
            self._sync_progress.deleteLater()
        self._sync_progress = None
        self._sync_job = None
        self.sync_button.setEnabled(True)

    def _sync_finished(self, report):
        self._finish_sync_ui()
        self.refresh()
        self.status.setText(
            f"TikTok synced: {report['videos']} clips · "
            f"{report['stale_files_removed']} rolling files removed · "
            f"{report.get('videos_skipped', 0) + report.get('following', {}).get('errors', 0)} "
            "unavailable skipped"
        )

    def _sync_failed(self, message):
        self._finish_sync_ui()
        self.status.setText("TikTok sync failed")
        QMessageBox.warning(self, "TikTok Sync", str(message))
