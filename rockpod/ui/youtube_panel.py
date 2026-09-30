"""Finished RockPod manager for the standalone offline YouTube app."""

from __future__ import annotations

import os
from datetime import datetime
from types import SimpleNamespace

from PySide6.QtCore import QObject, QRunnable, QThreadPool, Qt, Signal, Slot
from PySide6.QtGui import QColor, QPainter, QPen, QPixmap
from PySide6.QtWidgets import (
    QCheckBox,
    QAbstractItemView,
    QComboBox,
    QColorDialog,
    QDialog,
    QDialogButtonBox,
    QFileDialog,
    QFormLayout,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPushButton,
    QProgressDialog,
    QSpinBox,
    QDoubleSpinBox,
    QTabWidget,
    QTableWidget,
    QTableWidgetItem,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)


YT_BLUE = "#0033cc"
YT_LIGHT_BLUE = "#e6f1fa"
YT_RED = "#cc0000"
FIRST_LIVE_URL = "https://www.youtube.com/watch?v=U2zCCFNT6Vo"


class YoutubeSyncSignals(QObject):
    progress = Signal(int, int, str)
    finished = Signal(object)
    error = Signal(str)


class YoutubeImportSignals(QObject):
    finished = Signal(object)
    error = Signal(str)


class YoutubeImportJob(QRunnable):
    """Download and register a URL without blocking RockPod's UI thread."""

    def __init__(self, db_path, config, repo_root, url, destination, is_live):
        super().__init__()
        self.db_path = db_path
        self.config = dict(
            config if isinstance(config, dict) else getattr(config, "_data", {})
        )
        self.repo_root = str(repo_root)
        self.url = str(url)
        self.destination = str(destination)
        self.is_live = bool(is_live)
        self.signals = YoutubeImportSignals()

    @Slot()
    def run(self):
        database = None
        try:
            from app.database import Database
            from services.youtube_app import YoutubeAppService

            database = Database(self.db_path)
            service = YoutubeAppService(database, self.config, self.repo_root)
            row = (
                service.import_live_url(self.url)
                if self.is_live
                else service.import_url(self.url, self.destination)
            )
            self.signals.finished.emit(row)
        except Exception as exc:
            self.signals.error.emit(str(exc))
        finally:
            if database is not None:
                database.close()


class YoutubeSyncJob(QRunnable):
    """Run downloads, conversions and device writes without blocking Qt."""

    def __init__(self, db_path, config, repo_root, mount_path, live_only=False):
        super().__init__()
        self.db_path = db_path
        self.config = dict(
            config if isinstance(config, dict) else getattr(config, "_data", {})
        )
        self.repo_root = str(repo_root)
        self.mount_path = str(mount_path)
        self.live_only = bool(live_only)
        self.signals = YoutubeSyncSignals()

    @Slot()
    def run(self):
        database = None
        try:
            from app.database import Database
            from services.youtube_app import YoutubeAppService

            database = Database(self.db_path)
            service = YoutubeAppService(database, self.config, self.repo_root)
            report = service.sync(
                self.mount_path,
                device=SimpleNamespace(mount_path=self.mount_path),
                refresh_channels=not self.live_only,
                progress_callback=self.signals.progress.emit,
                live_only=self.live_only,
            )
            self.signals.finished.emit(report)
        except Exception as exc:
            self.signals.error.emit(str(exc))
        finally:
            if database is not None:
                database.close()


class ColorButton(QPushButton):
    value_changed = Signal(str)

    def __init__(self, value="#ffffff", parent=None):
        super().__init__(parent)
        self.value = value
        self.clicked.connect(self.choose)
        self.set_value(value)

    def set_value(self, value):
        color = QColor(str(value or "#ffffff"))
        self.value = color.name() if color.isValid() else "#ffffff"
        self.setText(self.value)
        self.setStyleSheet(
            f"background:{self.value}; color:"
            f"{'white' if color.lightness() < 120 else 'black'};"
        )

    def choose(self):
        color = QColorDialog.getColor(QColor(self.value), self)
        if color.isValid():
            self.set_value(color.name())
            self.value_changed.emit(self.value)


class YoutubePreview(QWidget):
    """Scaled approximation of the real 320x240 device renderer."""

    def __init__(self, logo_path, parent=None):
        super().__init__(parent)
        self.logo = QPixmap(str(logo_path))
        self.profile = {}
        self.video = {}
        self.setMinimumSize(480, 360)

    def update_data(self, profile, video=None):
        self.profile = dict(profile or {})
        self.video = dict(video or {})
        self.update()

    def paintEvent(self, _event):
        painter = QPainter(self)
        scale = min(self.width() / 320.0, self.height() / 240.0)
        left = (self.width() - 320 * scale) / 2
        top = (self.height() - 240 * scale) / 2
        painter.translate(left, top)
        painter.scale(scale, scale)
        background = QColor(self.profile.get("background_color") or "#ffffff")
        module = QColor(self.profile.get("module_color") or YT_LIGHT_BLUE)
        text = QColor(self.profile.get("text_color") or "#000000")
        link = QColor(self.profile.get("link_color") or YT_BLUE)
        painter.fillRect(0, 0, 320, 240, background)
        if not self.logo.isNull():
            painter.drawPixmap(5, 0, 112, 43, self.logo)
        painter.setPen(link)
        painter.drawText(128, 29, "Home")
        painter.drawText(188, 29, "Videos")
        painter.setPen(QColor(YT_RED))
        painter.drawText(250, 29, "Profile")
        painter.drawLine(0, 43, 319, 43)
        banner_path = self.profile.get("banner_image") or ""
        banner = QPixmap(banner_path) if os.path.isfile(banner_path) else QPixmap()
        has_banner = not banner.isNull()
        if has_banner:
            scaled = banner.scaled(
                312,
                56,
                Qt.KeepAspectRatioByExpanding,
                Qt.SmoothTransformation,
            )
            alignment = str(self.profile.get("banner_alignment") or "center").lower()
            crop_x = {
                "left": 0,
                "center": max(0, (scaled.width() - 312) // 2),
                "right": max(0, scaled.width() - 312),
            }.get(alignment, max(0, (scaled.width() - 312) // 2))
            vertical_alignment = str(
                self.profile.get("banner_vertical_alignment") or "center"
            ).lower()
            crop_y = {
                "top": 0,
                "center": max(0, (scaled.height() - 56) // 2),
                "bottom": max(0, scaled.height() - 56),
            }.get(vertical_alignment, max(0, (scaled.height() - 56) // 2))
            painter.drawPixmap(4, 47, 312, 56, scaled, crop_x, crop_y, 312, 56)
            module_y, module_h = 107, 56
            picture_x, picture_y, picture_size = 10, 111, 48
            text_x, name_y = 66, 123
            stats_y = (140, 157)
        else:
            module_y, module_h = 49, 73
            picture_x, picture_y, picture_size = 10, 54, 64
            text_x, name_y = 82, 67
            stats_y = (84, 101)
        painter.fillRect(4, module_y, 312, module_h, module)
        painter.setPen(QPen(QColor("#b4cfe7")))
        painter.drawRect(4, module_y, 311, module_h - 1)
        image_path = self.profile.get("profile_image") or ""
        image = QPixmap(image_path) if os.path.isfile(image_path) else QPixmap()
        painter.fillRect(picture_x, picture_y, picture_size, picture_size, QColor("white"))
        if not image.isNull():
            fitted = image.scaled(
                picture_size,
                picture_size,
                Qt.KeepAspectRatioByExpanding,
                Qt.SmoothTransformation,
            )
            source_x = max(0, (fitted.width() - picture_size) // 2)
            source_y = max(0, (fitted.height() - picture_size) // 2)
            painter.drawPixmap(
                picture_x,
                picture_y,
                picture_size,
                picture_size,
                fitted,
                source_x,
                source_y,
                picture_size,
                picture_size,
            )
        painter.setPen(link)
        painter.drawText(
            text_x,
            name_y,
            self.profile.get("display_name") or self.profile.get("username") or "You",
        )
        painter.setPen(text)
        painter.drawText(
            text_x,
            stats_y[0],
            f"Channel Views: {int(self.profile.get('channel_views') or 0):,}",
        )
        painter.drawText(
            text_x,
            stats_y[1],
            f"Video Views: {int(self.profile.get('video_views') or 0):,}",
        )
        about_y = 166 if has_banner else 128
        painter.fillRect(4, about_y, 312, 18 if has_banner else 22, module)
        painter.setPen(link)
        painter.drawText(10, about_y + 15, "About Me")
        painter.setPen(text)
        painter.drawText(10, about_y + 39, (self.profile.get("about_me") or "")[:48])
        videos_y = 207 if has_banner else 191
        painter.fillRect(4, videos_y, 312, 15 if has_banner else 22, module)
        painter.setPen(link)
        painter.drawText(10, videos_y + 15, "Videos")
        if self.video:
            painter.drawText(112, videos_y + 15, str(self.video.get("title") or "")[:28])
        painter.end()


class YoutubeMetadataDialog(QDialog):
    def __init__(self, row, parent=None):
        super().__init__(parent)
        self.setWindowTitle("YouTube Video Metadata")
        self.setMinimumWidth(560)
        self.row = dict(row or {})
        form = QFormLayout(self)
        self.title = QLineEdit(self.row.get("title") or "")
        self.uploader = QLineEdit(self.row.get("uploader") or "")
        self.upload_date = QLineEdit(self.row.get("upload_date") or "")
        self.views = QSpinBox()
        self.views.setRange(0, 2_000_000_000)
        self.views.setValue(int(self.row.get("view_count") or 0))
        self.rating = QDoubleSpinBox()
        self.rating.setRange(0, 5)
        self.rating.setDecimals(2)
        self.rating.setValue(float(self.row.get("rating_average") or 0))
        self.ratings = QSpinBox()
        self.ratings.setRange(0, 2_000_000_000)
        self.ratings.setValue(int(self.row.get("rating_count") or 0))
        self.category = QLineEdit(self.row.get("category") or "People & Blogs")
        self.tags = QLineEdit(self.row.get("tags") or "")
        self.description = QTextEdit(self.row.get("description") or "")
        self.description.setMaximumHeight(110)
        self.thumbnail = QLineEdit(self.row.get("thumbnail_path") or "")
        choose = QPushButton("Choose…")
        choose.clicked.connect(self.choose_thumbnail)
        thumb_row = QHBoxLayout()
        thumb_row.addWidget(self.thumbnail, 1)
        thumb_row.addWidget(choose)
        self.show_home = QCheckBox("Featured Videos")
        self.show_home.setChecked(bool(self.row.get("show_home")))
        self.show_profile = QCheckBox("Profile Videos")
        self.show_profile.setChecked(bool(self.row.get("show_profile", True)))
        membership = QHBoxLayout()
        membership.addWidget(self.show_home)
        membership.addWidget(self.show_profile)
        membership.addStretch(1)
        form.addRow("Title:", self.title)
        form.addRow("Uploader:", self.uploader)
        form.addRow("Upload date:", self.upload_date)
        form.addRow("Views:", self.views)
        form.addRow("Rating:", self.rating)
        form.addRow("Ratings:", self.ratings)
        form.addRow("Category:", self.category)
        form.addRow("Tags:", self.tags)
        form.addRow("Description:", self.description)
        form.addRow("Thumbnail:", thumb_row)
        form.addRow("Publish to:", membership)
        buttons = QDialogButtonBox(
            QDialogButtonBox.Save | QDialogButtonBox.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

    def choose_thumbnail(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Choose YouTube Thumbnail", "", "Images (*.png *.jpg *.jpeg *.bmp)"
        )
        if path:
            self.thumbnail.setText(path)

    def values(self):
        return {
            "title": self.title.text(),
            "uploader": self.uploader.text(),
            "upload_date": self.upload_date.text(),
            "view_count": self.views.value(),
            "rating_average": self.rating.value(),
            "rating_count": self.ratings.value(),
            "category": self.category.text(),
            "tags": self.tags.text(),
            "description": self.description.toPlainText(),
            "thumbnail_path": self.thumbnail.text(),
            "show_home": self.show_home.isChecked(),
            "show_profile": self.show_profile.isChecked(),
        }


class YoutubeUrlImportDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Import YouTube URL")
        self.setMinimumWidth(520)
        form = QFormLayout(self)
        self.url = QLineEdit()
        self.url.setPlaceholderText("https://www.youtube.com/watch?v=…")
        self.destination = QComboBox()
        self.destination.addItem("Home", "home")
        self.destination.addItem("Profile", "profile")
        self.destination.addItem("Home + Profile", "both")
        self.destination.addItem("Library only", "library")
        note = QLabel(
            "RockPod downloads a local offline copy, metadata and thumbnail; "
            "Sync converts it to the iPod's MPEG format."
        )
        note.setWordWrap(True)
        note.setStyleSheet("color:#5d6875;")
        form.addRow("YouTube URL:", self.url)
        form.addRow("Add to:", self.destination)
        form.addRow(note)
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.button(QDialogButtonBox.Ok).setText("Import")
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

    def values(self):
        return self.url.text().strip(), self.destination.currentData()


class YoutubeChannelSyncDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Add YouTube Channel")
        self.setMinimumWidth(560)
        form = QFormLayout(self)
        self.url = QLineEdit()
        self.url.setPlaceholderText("https://www.youtube.com/@creator")
        note = QLabel(
            "Each YouTube sync downloads this channel's three newest uploads. "
            "Older uploads from this channel are removed from the iPod during "
            "the same sync, so it never occupies more than three slots."
        )
        note.setWordWrap(True)
        note.setStyleSheet("color:#5d6875;")
        form.addRow("Channel URL:", self.url)
        form.addRow(note)
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.button(QDialogButtonBox.Ok).setText("Add Channel")
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

    def value(self):
        return self.url.text().strip()


class YoutubeLiveImportDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Add YouTube Live Video")
        self.setMinimumWidth(560)
        form = QFormLayout(self)
        self.url = QLineEdit(FIRST_LIVE_URL)
        self.url.setPlaceholderText("https://www.youtube.com/watch?v=…")
        note = QLabel(
            "RockPod downloads an offline broadcast and starts it live now. "
            "Opening it on the iPod joins the wall-clock position; pause, "
            "rewind and fast-forward are disabled. A newer live item from "
            "the same creator replaces the previous one."
        )
        note.setWordWrap(True)
        note.setStyleSheet("color:#5d6875;")
        form.addRow("YouTube URL:", self.url)
        form.addRow(note)
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.button(QDialogButtonBox.Ok).setText("Download and Go Live")
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

    def value(self):
        return self.url.text().strip()


class YoutubePanel(QWidget):
    def __init__(self, service, device_provider, parent=None):
        super().__init__(parent)
        self.service = service
        self.device_provider = device_provider
        self.rows = []
        self._sync_job = None
        self._sync_progress = None
        self._import_job = None
        self._import_progress = None
        outer = QVBoxLayout(self)
        outer.setContentsMargins(18, 14, 18, 14)
        hero = QFrame()
        hero.setStyleSheet(
            "QFrame { background:#f5f5f5; border:1px solid #c8c8c8; "
            "border-radius:5px; }"
        )
        hero_layout = QHBoxLayout(hero)
        logo = QLabel()
        logo.setPixmap(
            QPixmap(
                str(
                    self.service.repo_root
                    / "assets/ipodjs/rockbox/youtube/youtube-logo-2006.bmp"
                )
            ).scaledToWidth(150, Qt.SmoothTransformation)
        )
        copy = QLabel(
            "Offline YouTube for your iPod · Home, Videos and your 2007 profile"
        )
        copy.setStyleSheet("color:#555; font-size:13px;")
        hero_layout.addWidget(logo)
        hero_layout.addWidget(copy, 1)
        outer.addWidget(hero)
        self.tabs = QTabWidget()
        self.tabs.addTab(self._build_library_tab(), "Library")
        self.tabs.addTab(self._build_live_tab(), "Live")
        self.tabs.addTab(self._build_channel_sync_tab(), "Channel Sync")
        self.tabs.addTab(self._build_profile_tab(), "Profile")
        self.tabs.addTab(self._build_appearance_tab(), "Appearance")
        outer.addWidget(self.tabs, 1)
        footer = QHBoxLayout()
        self.status = QLabel("")
        self.status.setStyleSheet("color:#5d6875;")
        self.sync_mpeg_button = QPushButton("Sync as MPEG")
        self.sync_mpeg_button.setStyleSheet(
            f"background:{YT_BLUE}; color:white; padding:7px 16px; "
            "font-weight:600; border-radius:4px;"
        )
        self.sync_mpeg_button.clicked.connect(lambda: self.sync("quality"))
        self.sync_h264_button = QPushButton("Sync as H.264")
        self.sync_h264_button.setStyleSheet(
            "background:#202124; color:white; padding:7px 16px; "
            "font-weight:600; border-radius:4px;"
        )
        self.sync_h264_button.clicked.connect(
            lambda: self.sync("h264_apple_exact")
        )
        footer.addWidget(self.status, 1)
        footer.addWidget(self.sync_mpeg_button)
        footer.addWidget(self.sync_h264_button)
        outer.addLayout(footer)
        self.refresh()

    def _build_library_tab(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        controls = QHBoxLayout()
        add = QPushButton("Add Video")
        import_url = QPushButton("Import YouTube URL…")
        upload = QPushButton("Upload to Profile")
        edit = QPushButton("Edit Metadata")
        remove = QPushButton("Remove")
        up = QPushButton("Move Home Up")
        down = QPushButton("Move Home Down")
        add.clicked.connect(lambda: self.add_video(False))
        import_url.clicked.connect(self.import_from_url)
        upload.clicked.connect(lambda: self.add_video(True))
        edit.clicked.connect(self.edit_selected)
        remove.clicked.connect(self.remove_selected)
        up.clicked.connect(lambda: self.move_home(-1))
        down.clicked.connect(lambda: self.move_home(1))
        for button in (add, import_url, upload, edit, remove, up, down):
            controls.addWidget(button)
        controls.addStretch(1)
        layout.addLayout(controls)
        self.table = QTableWidget(0, 9)
        self.table.setHorizontalHeaderLabels(
            [
                "Title", "Uploader", "Length", "Rating", "Views", "Home",
                "Profile", "Live", "Sync",
            ]
        )
        self.table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.table.setSelectionMode(QAbstractItemView.ExtendedSelection)
        self.table.setAlternatingRowColors(True)
        self.table.doubleClicked.connect(self.edit_selected)
        layout.addWidget(self.table, 1)
        return page

    def _build_live_tab(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        copy = QLabel(
            "Live is linear, wall-clock playback. Each creator can have one "
            "current broadcast; viewers join what is on now and cannot seek."
        )
        copy.setWordWrap(True)
        copy.setStyleSheet("color:#5d6875;")
        layout.addWidget(copy)
        controls = QHBoxLayout()
        add = QPushButton("Add Live URL…")
        from_library = QPushButton("Go Live from Library")
        end = QPushButton("End Live")
        self.live_sync_button = QPushButton("Sync Live to iPod")
        add.clicked.connect(self.import_live_url)
        from_library.clicked.connect(self.go_live_from_library)
        end.clicked.connect(self.end_selected_live)
        self.live_sync_button.clicked.connect(
            lambda: self.sync(live_only=True)
        )
        controls.addWidget(add)
        controls.addWidget(from_library)
        controls.addWidget(end)
        controls.addWidget(self.live_sync_button)
        controls.addStretch(1)
        layout.addLayout(controls)
        self.live_table = QTableWidget(0, 4)
        self.live_table.setHorizontalHeaderLabels(
            ["Creator", "Live video", "Length", "Started"]
        )
        self.live_table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.live_table.setSelectionMode(QAbstractItemView.SingleSelection)
        self.live_table.setAlternatingRowColors(True)
        layout.addWidget(self.live_table, 1)
        return page

    def _build_channel_sync_tab(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        copy = QLabel(
            "Keep the newest three uploads from selected channels on your iPod. "
            "They refresh whenever you choose either video sync format."
        )
        copy.setWordWrap(True)
        copy.setStyleSheet("color:#5d6875;")
        layout.addWidget(copy)
        controls = QHBoxLayout()
        add = QPushButton("Add Channel…")
        remove = QPushButton("Remove Channel")
        add.clicked.connect(self.add_channel_sync)
        remove.clicked.connect(self.remove_channel_sync)
        controls.addWidget(add)
        controls.addWidget(remove)
        controls.addStretch(1)
        layout.addLayout(controls)
        self.channel_table = QTableWidget(0, 3)
        self.channel_table.setHorizontalHeaderLabels(
            ["Channel", "Keep on iPod", "Channel URL"]
        )
        self.channel_table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.channel_table.setSelectionMode(QAbstractItemView.SingleSelection)
        self.channel_table.setAlternatingRowColors(True)
        layout.addWidget(self.channel_table, 1)
        return page

    def _profile_field(self, layout, label, multiline=False):
        widget = QTextEdit() if multiline else QLineEdit()
        if multiline:
            widget.setMaximumHeight(90)
        layout.addRow(label, widget)
        return widget

    def _build_profile_tab(self):
        page = QWidget()
        grid = QGridLayout(page)
        editor = QFrame()
        form = QFormLayout(editor)
        self.username = self._profile_field(form, "Username:")
        self.display_name = self._profile_field(form, "Display name:")
        self.about = self._profile_field(form, "About Me:", True)
        self.city = self._profile_field(form, "City:")
        self.country = self._profile_field(form, "Country:")
        self.occupation = self._profile_field(form, "Occupation:")
        self.interests = self._profile_field(form, "Interests:")
        self.movies = self._profile_field(form, "Movies:")
        self.music = self._profile_field(form, "Music:")
        self.books = self._profile_field(form, "Books:")
        self.website = self._profile_field(form, "Website:")
        self.joined = self._profile_field(form, "Joined:")
        self.profile_image = QLineEdit()
        choose = QPushButton("Choose…")
        choose.clicked.connect(self.choose_profile_image)
        image_row = QHBoxLayout()
        image_row.addWidget(self.profile_image, 1)
        image_row.addWidget(choose)
        form.addRow("Profile image:", image_row)
        self.banner_image = QLineEdit()
        banner_choose = QPushButton("Choose…")
        banner_choose.clicked.connect(self.choose_banner_image)
        banner_row = QHBoxLayout()
        banner_row.addWidget(self.banner_image, 1)
        banner_row.addWidget(banner_choose)
        form.addRow("Channel banner:", banner_row)
        self.banner_alignment = QComboBox()
        self.banner_alignment.addItem("Left", "left")
        self.banner_alignment.addItem("Center", "center")
        self.banner_alignment.addItem("Right", "right")
        form.addRow("Banner horizontal:", self.banner_alignment)
        self.banner_vertical_alignment = QComboBox()
        self.banner_vertical_alignment.addItem("Top", "top")
        self.banner_vertical_alignment.addItem("Center", "center")
        self.banner_vertical_alignment.addItem("Bottom", "bottom")
        form.addRow("Banner vertical:", self.banner_vertical_alignment)
        self.channel_views = QSpinBox()
        self.video_views = QSpinBox()
        self.subscribers = QSpinBox()
        self.friends = QSpinBox()
        for spin in (self.channel_views, self.video_views, self.subscribers, self.friends):
            spin.setRange(0, 2_000_000_000)
        form.addRow("Channel Views:", self.channel_views)
        form.addRow("Video Views:", self.video_views)
        form.addRow("Subscribers:", self.subscribers)
        form.addRow("Friends:", self.friends)
        save = QPushButton("Save Profile")
        save.clicked.connect(self.save_profile)
        form.addRow(save)
        self.preview = YoutubePreview(
            self.service.repo_root
            / "assets/ipodjs/rockbox/youtube/youtube-logo-2006.bmp"
        )
        grid.addWidget(editor, 0, 0)
        grid.addWidget(self.preview, 0, 1)
        grid.setColumnStretch(1, 1)
        return page

    def _build_appearance_tab(self):
        page = QWidget()
        form = QFormLayout(page)
        self.background_color = ColorButton("#ffffff")
        self.module_color = ColorButton(YT_LIGHT_BLUE)
        self.text_color = ColorButton("#000000")
        self.link_color = ColorButton(YT_BLUE)
        self.show_about = QCheckBox("Show About Me module")
        self.show_videos = QCheckBox("Show Videos module")
        self.show_favorites = QCheckBox("Show Favorites module")
        self.show_main_menu = QCheckBox("Show YouTube on Main Menu")
        self.show_main_menu.setToolTip(
            "Off by default. YouTube always remains in Extras → Applications."
        )
        form.addRow("Page background:", self.background_color)
        form.addRow("Module headers:", self.module_color)
        form.addRow("Text:", self.text_color)
        form.addRow("Links:", self.link_color)
        form.addRow(self.show_about)
        form.addRow(self.show_videos)
        form.addRow(self.show_favorites)
        form.addRow(self.show_main_menu)
        save = QPushButton("Save Appearance")
        save.clicked.connect(self.save_profile)
        form.addRow(save)
        return page

    def refresh(self):
        self.rows = self.service.list_videos()
        self.table.setRowCount(len(self.rows))
        for index, row in enumerate(self.rows):
            values = [
                row["title"],
                row["uploader"],
                self._duration(row["duration_ms"]),
                f"{float(row['rating_average'] or 0):.2f}",
                f"{int(row['view_count'] or 0):,}",
                "✓" if row["show_home"] else "",
                "✓" if row["show_profile"] else "",
                "LIVE" if row.get("is_live") else "",
                "Ready" if os.path.isfile(row["source_path"]) else "Missing",
            ]
            for column, value in enumerate(values):
                item = QTableWidgetItem(str(value))
                item.setData(Qt.UserRole, row["id"])
                self.table.setItem(index, column, item)
        self.table.resizeColumnsToContents()
        live_rows = self.service.list_live_videos()
        self.live_table.setRowCount(len(live_rows))
        for index, row in enumerate(live_rows):
            values = [
                row.get("uploader") or "YouTube creator",
                row.get("title") or "Live video",
                self._duration(row.get("duration_ms")),
                self._live_started(row.get("live_start_epoch")),
            ]
            for column, value in enumerate(values):
                item = QTableWidgetItem(str(value))
                item.setData(Qt.UserRole, row["id"])
                self.live_table.setItem(index, column, item)
        self.live_table.resizeColumnsToContents()
        channels = self.service.list_channel_syncs()
        self.channel_table.setRowCount(len(channels))
        for index, channel in enumerate(channels):
            values = [
                channel.get("channel_name") or "YouTube channel",
                "Latest 3 uploads",
                channel["channel_url"],
            ]
            for column, value in enumerate(values):
                item = QTableWidgetItem(value)
                item.setData(Qt.UserRole, channel["channel_url"])
                self.channel_table.setItem(index, column, item)
        self.channel_table.resizeColumnsToContents()
        profile = self.service.get_profile()
        self._load_profile(profile)
        self.preview.update_data(profile, self.rows[0] if self.rows else None)
        self.status.setText(
            f"{len(self.rows)} video{'s' if len(self.rows) != 1 else ''} in the offline library · "
            f"{len(live_rows)} live"
        )

    @staticmethod
    def _duration(milliseconds):
        seconds = max(0, int(milliseconds or 0) // 1000)
        return f"{seconds // 60}:{seconds % 60:02d}"

    @staticmethod
    def _live_started(epoch):
        try:
            return datetime.fromtimestamp(int(epoch)).strftime("%b %d, %H:%M")
        except (OSError, OverflowError, TypeError, ValueError):
            return "Now"

    def _selected_ids(self):
        return sorted(
            {
                self.table.item(index.row(), 0).data(Qt.UserRole)
                for index in self.table.selectionModel().selectedRows()
                if self.table.item(index.row(), 0)
            }
        )

    def add_video(self, profile_only=False):
        path, _ = QFileDialog.getOpenFileName(
            self,
            "Upload Video to Offline YouTube",
            "",
            "Videos (*.mpg *.mpeg *.mp4 *.m4v *.mov *.avi *.mkv *.webm)",
        )
        if not path:
            return
        try:
            row = self.service.add_video(
                path, show_profile=profile_only, show_home=False
            )
        except (OSError, ValueError) as exc:
            QMessageBox.warning(self, "Add YouTube Video", str(exc))
            return
        dialog = YoutubeMetadataDialog(row, self)
        if dialog.exec() == QDialog.Accepted:
            self.service.update_video(row["id"], dialog.values())
        self.refresh()

    def import_from_url(self):
        dialog = YoutubeUrlImportDialog(self)
        if dialog.exec() != QDialog.Accepted:
            return
        url, destination = dialog.values()
        self._start_import(url, destination, False)

    def import_live_url(self):
        dialog = YoutubeLiveImportDialog(self)
        if dialog.exec() != QDialog.Accepted:
            return
        self._start_import(dialog.value(), "library", True)

    def _start_import(self, url, destination, is_live):
        if self._import_job is not None:
            return
        self._import_progress = QProgressDialog(
            "Downloading the YouTube live source…" if is_live else
            "Downloading YouTube video and metadata…",
            None,
            0,
            0,
            self,
        )
        self._import_progress.setWindowTitle(
            "Add YouTube Live Video" if is_live else "Import YouTube URL"
        )
        self._import_progress.setWindowModality(Qt.WindowModal)
        self._import_progress.setMinimumDuration(0)
        self._import_progress.setAutoClose(False)
        self._import_progress.show()
        self.status.setText(self._import_progress.labelText())
        self.sync_mpeg_button.setEnabled(False)
        self.sync_h264_button.setEnabled(False)
        self._import_job = YoutubeImportJob(
            self.service.db._path,
            self.service.config,
            self.service.repo_root,
            url,
            destination,
            is_live,
        )
        self._import_job.signals.finished.connect(
            lambda row: self._import_finished(row, is_live)
        )
        self._import_job.signals.error.connect(
            lambda message: self._import_failed(message, is_live)
        )
        QThreadPool.globalInstance().start(self._import_job)

    def _finish_import_ui(self):
        if self._import_progress is not None:
            self._import_progress.close()
            self._import_progress.deleteLater()
        self._import_progress = None
        self._import_job = None
        self.sync_mpeg_button.setEnabled(True)
        self.sync_h264_button.setEnabled(True)

    def _import_finished(self, row, is_live):
        self._finish_import_ui()
        self.refresh()
        if is_live:
            self.status.setText(
                f"{row['uploader']} is live with {row['title']} · sync when ready"
            )
        else:
            self.status.setText(
                f"Imported {row['title']} · click Sync YouTube to iPod when ready"
            )

    def _import_failed(self, message, is_live):
        self._finish_import_ui()
        title = "Add YouTube Live Video" if is_live else "Import YouTube URL"
        QMessageBox.warning(self, title, str(message))
        self.status.setText(
            "YouTube live import failed" if is_live else "YouTube import failed"
        )

    def go_live_from_library(self):
        ids = self._selected_ids()
        if len(ids) != 1:
            QMessageBox.information(
                self, "Go Live", "Select one library video first."
            )
            return
        row = self.service.set_live_video(ids[0], True)
        self.refresh()
        self.status.setText(f"{row['uploader']} is now live")

    def end_selected_live(self):
        selected = self.live_table.selectionModel().selectedRows()
        if not selected:
            return
        video_id = self.live_table.item(selected[0].row(), 0).data(Qt.UserRole)
        self.service.set_live_video(video_id, False)
        self.refresh()
        self.status.setText("Live broadcast ended")

    def add_channel_sync(self):
        dialog = YoutubeChannelSyncDialog(self)
        if dialog.exec() != QDialog.Accepted:
            return
        try:
            channel = self.service.add_channel_sync(dialog.value())
        except ValueError as exc:
            QMessageBox.warning(self, "Add YouTube Channel", str(exc))
            return
        self.refresh()
        self.status.setText(
            f"Added {channel['channel_url']} · its latest 3 uploads will sync next time"
        )

    def remove_channel_sync(self):
        selected = self.channel_table.selectionModel().selectedRows()
        if not selected:
            return
        channel_url = self.channel_table.item(selected[0].row(), 0).data(Qt.UserRole)
        if QMessageBox.question(
            self,
            "Remove YouTube Channel",
            "Stop refreshing this channel? Existing synced videos stay until "
            "you remove them from the library.",
        ) != QMessageBox.Yes:
            return
        self.service.remove_channel_sync(channel_url)
        self.refresh()

    def edit_selected(self):
        ids = self._selected_ids()
        if len(ids) != 1:
            QMessageBox.information(self, "Edit Metadata", "Select one video to edit.")
            return
        row = self.service.get_video(ids[0])
        dialog = YoutubeMetadataDialog(row, self)
        if dialog.exec() == QDialog.Accepted:
            self.service.update_video(ids[0], dialog.values())
            self.refresh()

    def remove_selected(self):
        ids = self._selected_ids()
        if not ids:
            return
        if QMessageBox.question(
            self,
            "Remove from YouTube",
            f"Remove {len(ids)} selected video{'s' if len(ids) != 1 else ''} "
            "from the offline YouTube library? Source files are kept.",
        ) != QMessageBox.Yes:
            return
        for video_id in ids:
            self.service.remove_video(video_id)
        self.refresh()

    def move_home(self, delta):
        ids = self._selected_ids()
        if len(ids) != 1:
            return
        home = [row for row in self.rows if row["show_home"]]
        current = next((i for i, row in enumerate(home) if row["id"] == ids[0]), -1)
        target = current + delta
        if current < 0 or target < 0 or target >= len(home):
            return
        home[current], home[target] = home[target], home[current]
        for order, row in enumerate(home, 1):
            self.service.update_video(row["id"], {"home_order": order})
        self.refresh()

    def choose_profile_image(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Choose Profile Picture", "", "Images (*.png *.jpg *.jpeg *.bmp)"
        )
        if path:
            self.profile_image.setText(path)

    def choose_banner_image(self):
        path, _ = QFileDialog.getOpenFileName(
            self,
            "Choose YouTube Channel Banner",
            "",
            "Images (*.png *.jpg *.jpeg *.bmp)",
        )
        if path:
            self.banner_image.setText(path)
            self.preview.update_data(
                self._profile_values(), self.rows[0] if self.rows else None
            )

    def _load_profile(self, profile):
        line_fields = {
            "username": self.username,
            "display_name": self.display_name,
            "city": self.city,
            "country": self.country,
            "occupation": self.occupation,
            "interests": self.interests,
            "movies": self.movies,
            "music": self.music,
            "books": self.books,
            "website": self.website,
            "joined": self.joined,
            "profile_image": self.profile_image,
            "banner_image": self.banner_image,
        }
        for key, widget in line_fields.items():
            widget.setText(str(profile.get(key) or ""))
        self.about.setPlainText(str(profile.get("about_me") or ""))
        alignment = str(profile.get("banner_alignment") or "center").lower()
        index = self.banner_alignment.findData(alignment)
        self.banner_alignment.setCurrentIndex(index if index >= 0 else 1)
        vertical = str(
            profile.get("banner_vertical_alignment") or "center"
        ).lower()
        index = self.banner_vertical_alignment.findData(vertical)
        self.banner_vertical_alignment.setCurrentIndex(index if index >= 0 else 1)
        for key, widget in {
            "channel_views": self.channel_views,
            "video_views": self.video_views,
            "subscribers": self.subscribers,
            "friends": self.friends,
        }.items():
            widget.setValue(int(profile.get(key) or 0))
        self.background_color.set_value(profile.get("background_color") or "#ffffff")
        self.module_color.set_value(profile.get("module_color") or YT_LIGHT_BLUE)
        self.text_color.set_value(profile.get("text_color") or "#000000")
        self.link_color.set_value(profile.get("link_color") or YT_BLUE)
        self.show_about.setChecked(bool(profile.get("show_about", True)))
        self.show_videos.setChecked(bool(profile.get("show_videos", True)))
        self.show_favorites.setChecked(bool(profile.get("show_favorites", True)))
        self.show_main_menu.setChecked(bool(profile.get("show_on_main_menu", False)))

    def _profile_values(self):
        return {
                "username": self.username.text(),
                "display_name": self.display_name.text(),
                "about_me": self.about.toPlainText(),
                "city": self.city.text(),
                "country": self.country.text(),
                "occupation": self.occupation.text(),
                "interests": self.interests.text(),
                "movies": self.movies.text(),
                "music": self.music.text(),
                "books": self.books.text(),
                "website": self.website.text(),
                "joined": self.joined.text(),
                "profile_image": self.profile_image.text(),
                "banner_image": self.banner_image.text(),
                "banner_alignment": self.banner_alignment.currentData(),
                "banner_vertical_alignment": (
                    self.banner_vertical_alignment.currentData()
                ),
                "channel_views": self.channel_views.value(),
                "video_views": self.video_views.value(),
                "subscribers": self.subscribers.value(),
                "friends": self.friends.value(),
                "background_color": self.background_color.value,
                "module_color": self.module_color.value,
                "text_color": self.text_color.value,
                "link_color": self.link_color.value,
                "show_about": self.show_about.isChecked(),
                "show_videos": self.show_videos.isChecked(),
                "show_favorites": self.show_favorites.isChecked(),
                "show_on_main_menu": self.show_main_menu.isChecked(),
            }

    def save_profile(self):
        profile = self.service.save_profile(self._profile_values())
        self.preview.update_data(profile, self.rows[0] if self.rows else None)
        self.status.setText("YouTube profile saved")

    def sync(self, video_profile="quality", live_only=False):
        if self._sync_job is not None:
            return
        self.save_profile()
        device = self.device_provider()
        if device is None or not getattr(device, "mount_path", ""):
            QMessageBox.warning(self, "YouTube Sync", "Connect or mount an iPod first.")
            return
        label = "YouTube Live sync" if live_only else "YouTube sync"
        self._sync_progress = QProgressDialog(
            f"Starting {label}…", None, 0, 0, self
        )
        self._sync_progress.setWindowTitle(f"{label} to iPod")
        self._sync_progress.setWindowModality(Qt.WindowModal)
        self._sync_progress.setMinimumDuration(0)
        self._sync_progress.setAutoClose(False)
        self._sync_progress.setAutoReset(False)
        self._sync_progress.show()
        self.sync_mpeg_button.setEnabled(False)
        self.sync_h264_button.setEnabled(False)
        self.live_sync_button.setEnabled(False)
        self.status.setText(f"{label} starting…")
        try:
            job_config = dict(
                self.service.config
                if isinstance(self.service.config, dict)
                else getattr(self.service.config, "_data", {})
            )
            job_config["video_sync_profile"] = video_profile
            if live_only:
                job_config["video_sync_profile"] = "h264_apple_exact"
            self._sync_job = YoutubeSyncJob(
                self.service.db._path,
                job_config,
                self.service.repo_root,
                device.mount_path,
                live_only=live_only,
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
        label = str(message or "Syncing YouTube…")
        self._sync_progress.setLabelText(label)
        self.status.setText(label)

    def _finish_sync_ui(self):
        if self._sync_progress is not None:
            self._sync_progress.close()
            self._sync_progress.deleteLater()
        self._sync_progress = None
        self._sync_job = None
        self.sync_mpeg_button.setEnabled(True)
        self.sync_h264_button.setEnabled(True)
        self.live_sync_button.setEnabled(True)

    def _sync_finished(self, report):
        self._finish_sync_ui()
        self.refresh()
        self.status.setText(
            f"YouTube synced: {report['videos']} videos · "
            f"{report.get('live_videos', 0)} live · "
            f"{report['media_updated']} media updates · "
            f"{report['local_state_merged']} iPod changes imported"
        )
        QMessageBox.information(
            self,
            "YouTube Sync Complete",
            f"Videos: {report['videos']}\n"
            f"Live broadcasts: {report.get('live_videos', 0)}\n"
            f"New/updated media: {report['media_updated']}\n"
            f"Unchanged media: {report['media_unchanged']}\n"
            f"Channels refreshed: {report['channels_refreshed']}\n"
            f"Older channel videos removed: {report['channel_videos_removed']}\n"
            f"Historical assets updated: {report['assets_updated']}\n"
            f"Device ratings/favorites imported: {report['local_state_merged']}",
        )

    def _sync_failed(self, message):
        self._finish_sync_ui()
        self.status.setText("YouTube sync failed")
        QMessageBox.warning(self, "YouTube Sync", str(message))
