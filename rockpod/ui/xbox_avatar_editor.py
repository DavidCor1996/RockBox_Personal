"""RockPod creator and exact iPod preview for authentic Xbox 360 avatars."""

from __future__ import annotations

import os
from pathlib import Path

from PIL import Image
from PySide6.QtCore import Qt, QTimer, QUrl, Signal
from PySide6.QtGui import QColor, QFont, QImage, QLinearGradient, QPainter, QPixmap
from PySide6.QtWidgets import (
    QComboBox,
    QFormLayout,
    QFrame,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QPushButton,
    QStackedWidget,
    QVBoxLayout,
    QWidget,
)
from PySide6.QtQuickWidgets import QQuickWidget

from services.xbox_avatar import (
    APPEARANCE_FIELDS,
    APPEARANCE_PALETTES,
    EMOTE_CLIPS,
    XboxAvatarService,
    decode_rav1,
    encode_rav1,
)


BODY_LABELS = {
    "xna-boy": "XNA Boy — Sneakers",
    "xna-girl": "XNA Girl — Sneakers",
    "xna-girl-heels": "XNA Girl — Heels",
}
CLIP_LABELS = {
    "jump": "Jump",
    "throw": "Throw",
    "faint": "Faint",
    "sit-idle": "Sit Idle",
    "punch": "Punch",
    "kick": "Kick",
    "walk": "Walk",
}


MODEL_QML = {
    "xna-boy": "xna-boy/Xna_boy.qml",
    "xna-girl": "xna-girl/Xna_girl.qml",
    "xna-girl-heels": "xna-girl-heels/Xna_girl_heels.qml",
}


def _palette_color(field, value):
    for key, _label, color in APPEARANCE_PALETTES[field]:
        if key == value:
            return QColor(color or "#ffffff")
    return QColor("#ffffff")


class _Avatar3DView(QQuickWidget):
    """Hardware-accelerated viewport for the authentic XNA model mesh."""

    def __init__(self, repo_root, parent=None):
        super().__init__(parent)
        self._repo_root = Path(repo_root)
        self.setResizeMode(QQuickWidget.SizeRootObjectToView)
        self.setMinimumSize(280, 360)
        self.setSource(QUrl.fromLocalFile(str(
            self._repo_root / "rockpod" / "ui" / "qml" /
            "xbox_avatar_viewer.qml"
        )))

    def set_profile(self, profile):
        root = self.rootObject()
        if root is None:
            return
        body = profile.get("body", "xna-boy")
        relative = MODEL_QML.get(body, MODEL_QML["xna-boy"])
        model = self._repo_root / "rockpod" / "ui" / "qml" / \
            "xbox_avatar_models" / relative
        root.setProperty("modelSource", QUrl.fromLocalFile(str(model)))
        for field in APPEARANCE_FIELDS:
            root.setProperty(
                f"{field}Tint",
                _palette_color(field, profile.get(field, "original")),
            )


def _qimage(image):
    rgba = image.convert("RGBA")
    return QImage(
        rgba.tobytes("raw", "RGBA"), rgba.width, rgba.height,
        QImage.Format_RGBA8888,
    ).copy()


class _AvatarStage(QWidget):
    def __init__(self, ipod=False, parent=None):
        super().__init__(parent)
        self._frame = QImage()
        self._display_name = "OFFLINE PLAYER"
        self._stats = {"gamerscore": 0, "unlocked": 0, "total": 0, "games": 0}
        self._ipod = ipod
        self._orb = QPixmap()
        if ipod:
            self.setFixedSize(320, 240)
        else:
            self.setMinimumSize(260, 360)

    def set_frame(self, frame):
        self._frame = _qimage(frame) if frame is not None else QImage()
        self.update()

    def set_profile(self, display_name, stats):
        self._display_name = display_name or "OFFLINE PLAYER"
        self._stats = dict(stats or {})
        self.update()

    def set_orb(self, pixmap):
        self._orb = QPixmap(pixmap)
        self.update()

    def paintEvent(self, _event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.SmoothPixmapTransform)
        if self._ipod:
            self._paint_ipod(painter)
        else:
            self._paint_desktop(painter)

    def _paint_desktop(self, painter):
        gradient = QLinearGradient(0, 0, 0, self.height())
        gradient.setColorAt(0, QColor("#f4f5f5"))
        gradient.setColorAt(0.58, QColor("#d9dddf"))
        gradient.setColorAt(1, QColor("#8eba34"))
        painter.fillRect(self.rect(), gradient)
        painter.setBrush(QColor(0, 0, 0, 32))
        painter.setPen(Qt.NoPen)
        painter.drawEllipse(
            self.width() // 2 - 72, self.height() - 55, 144, 24
        )
        if not self._frame.isNull():
            target_height = min(self.height() - 34, 336)
            scaled = self._frame.scaled(
                208, target_height, Qt.KeepAspectRatio,
                Qt.SmoothTransformation,
            )
            painter.drawImage(
                (self.width() - scaled.width()) // 2,
                self.height() - scaled.height() - 20,
                scaled,
            )

    def _paint_ipod(self, painter):
        painter.fillRect(0, 0, 320, 240, QColor("#d7dadc"))
        header = QLinearGradient(0, 0, 0, 40)
        header.setColorAt(0, QColor("#fafafa"))
        header.setColorAt(1, QColor("#aeb3b7"))
        painter.fillRect(0, 0, 320, 40, header)
        painter.setPen(QColor("#323638"))
        font = QFont(painter.font())
        font.setBold(True)
        font.setPixelSize(16)
        painter.setFont(font)
        painter.drawText(12, 5, 150, 28, Qt.AlignVCenter, "My Xbox")
        if not self._orb.isNull():
            painter.drawPixmap(282, 4, 30, 30, self._orb)

        badge = QLinearGradient(159, 8, 271, 8)
        badge.setColorAt(0, QColor("#75ad1d"))
        badge.setColorAt(1, QColor("#3f790c"))
        painter.fillRect(158, 8, 116, 23, badge)
        painter.setPen(QColor("#ffffff"))
        font.setPixelSize(9)
        painter.setFont(font)
        painter.drawText(164, 8, 104, 23, Qt.AlignVCenter,
                         "iPOD HARDCORE")

        painter.fillRect(8, 48, 121, 166, QColor("#f7f7f7"))
        painter.setPen(QColor("#8e9498"))
        painter.drawRect(8, 48, 121, 166)
        painter.fillRect(8, 48, 6, 166, QColor("#69a719"))
        font.setPixelSize(12)
        painter.setFont(font)
        painter.setPen(QColor("#222526"))
        painter.drawText(22, 59, 100, 20, Qt.AlignLeft, self._display_name)
        font.setBold(False)
        font.setPixelSize(10)
        painter.setFont(font)
        painter.setPen(QColor("#555a5d"))
        score = int(self._stats.get("gamerscore") or 0)
        unlocked = int(self._stats.get("unlocked") or 0)
        total = int(self._stats.get("total") or 0)
        games = int(self._stats.get("games") or 0)
        painter.drawText(22, 86, 98, 18, Qt.AlignLeft, f"{score} G")
        painter.drawText(22, 108, 98, 18, Qt.AlignLeft,
                         f"{unlocked}/{total} unlocked")
        painter.drawText(22, 130, 98, 18, Qt.AlignLeft, f"{games} games")

        stage = QLinearGradient(136, 48, 136, 214)
        stage.setColorAt(0, QColor("#f7f8f8"))
        stage.setColorAt(0.68, QColor("#d9dcde"))
        stage.setColorAt(1, QColor("#86b32f"))
        painter.fillRect(136, 48, 176, 166, stage)
        painter.setPen(QColor("#9da2a5"))
        painter.drawRect(136, 48, 176, 166)
        painter.setBrush(QColor(0, 0, 0, 30))
        painter.setPen(Qt.NoPen)
        painter.drawEllipse(178, 189, 96, 15)
        if not self._frame.isNull():
            scaled = self._frame.scaled(
                104, 168, Qt.KeepAspectRatio, Qt.SmoothTransformation
            )
            painter.drawImage(172, 43, scaled)
        painter.fillRect(0, 222, 320, 18, QColor("#2d3032"))
        painter.setPen(QColor("#ffffff"))
        font.setPixelSize(9)
        painter.setFont(font)
        painter.drawText(
            8, 222, 304, 18, Qt.AlignVCenter,
            "WHEEL Emote   SELECT Play   MENU Back",
        )


class XboxAvatarEditorWidget(QWidget):
    profile_saved = Signal(dict)
    sync_requested = Signal()

    def __init__(self, repo_root, parent=None):
        super().__init__(parent)
        self.setObjectName("xbox_avatar_editor")
        self._repo_root = os.path.abspath(repo_root)
        self._frames = []
        self._device_frames = []
        self._frame_index = 0
        self._stats = {}

        root = QVBoxLayout(self)
        root.setContentsMargins(12, 10, 12, 10)
        root.setSpacing(8)
        title = QLabel("Achievements & Avatar")
        title.setObjectName("theme_hub_title")
        root.addWidget(title)
        subtitle = QLabel(
            "Build an iPod Hardcore profile from Microsoft's real XNA Xbox "
            "Avatar rigs. Preview the 4× master and exact supersampled device "
            "animation side by side."
        )
        subtitle.setWordWrap(True)
        subtitle.setObjectName("theme_hub_status")
        root.addWidget(subtitle)

        columns = QHBoxLayout()
        columns.setSpacing(10)

        left = QFrame()
        left.setObjectName("theme_hub_header")
        left_layout = QVBoxLayout(left)
        left_layout.addWidget(QLabel("AVATAR EDITOR"))
        form = QFormLayout()
        self._name = QLineEdit()
        self._name.setMaxLength(15)
        self._name.textChanged.connect(self._profile_changed)
        self._body = QComboBox()
        for body, label in BODY_LABELS.items():
            self._body.addItem(label, body)
        self._body.currentIndexChanged.connect(self._reload_animation)
        self._clip = QComboBox()
        for clip in EMOTE_CLIPS:
            self._clip.addItem(CLIP_LABELS[clip], clip)
        self._clip.currentIndexChanged.connect(self._reload_animation)
        form.addRow("Display name", self._name)
        form.addRow("Body", self._body)
        form.addRow("Animation", self._clip)
        self._appearance = {}
        appearance_labels = {
            "skin": "Skin && face",
            "hair": "Hair",
            "top": "Top",
            "bottom": "Bottom",
            "shoes": "Shoes",
        }
        for field in APPEARANCE_FIELDS:
            picker = QComboBox()
            for value, label, color in APPEARANCE_PALETTES[field]:
                picker.addItem(label, value)
                if color:
                    picker.setItemData(
                        picker.count() - 1, QColor(color), Qt.DecorationRole
                    )
            picker.currentIndexChanged.connect(self._reload_animation)
            self._appearance[field] = picker
            form.addRow(appearance_labels[field], picker)
        left_layout.addLayout(form)
        fixed = QLabel(
            "Original XNA face rig and garment meshes stay intact. "
            "Headwear and accessories require an authenticated owned import."
        )
        fixed.setWordWrap(True)
        fixed.setObjectName("theme_hub_status")
        left_layout.addWidget(fixed)
        left_layout.addStretch(1)
        columns.addWidget(left, 2)

        center = QFrame()
        center.setObjectName("theme_hub_header")
        center_layout = QVBoxLayout(center)
        center_header = QHBoxLayout()
        center_header.addWidget(QLabel("LIVE XNA 3D MODEL"))
        center_header.addStretch(1)
        self._view_toggle = QPushButton("Show Motion")
        self._view_toggle.clicked.connect(self._toggle_center_view)
        center_header.addWidget(self._view_toggle)
        center_layout.addLayout(center_header)
        self._center_stack = QStackedWidget()
        self._model_view = _Avatar3DView(self._repo_root)
        self._stage = _AvatarStage()
        self._center_stack.addWidget(self._model_view)
        self._center_stack.addWidget(self._stage)
        center_layout.addWidget(self._center_stack, 1)
        self._asset_status = QLabel()
        self._asset_status.setWordWrap(True)
        self._asset_status.setObjectName("theme_hub_diff")
        center_layout.addWidget(self._asset_status)
        columns.addWidget(center, 3)

        right = QFrame()
        right.setObjectName("theme_hub_header")
        right_layout = QVBoxLayout(right)
        right_layout.addWidget(QLabel("EXACT 320×240 iPOD EXPORT"))
        self._ipod = _AvatarStage(ipod=True)
        orb = QPixmap(str(
            Path(self._repo_root) / "assets" / "ipodjs" / "rockbox" /
            "achievements" / "xbox360-sphere-official.32x32x24.bmp"
        ))
        self._ipod.set_orb(orb)
        right_layout.addWidget(self._ipod, 0, Qt.AlignCenter)
        self._provenance = QLabel(
            "Authentic source: Microsoft XNA Avatar Animation Pack 4.0 "
            "(Ms-PL), including original garment textures and distinct sneaker "
            "and heel meshes. The creator uses real-time 3D geometry; all iPod "
            "frames are rendered from that model."
        )
        self._provenance.setWordWrap(True)
        self._provenance.setObjectName("theme_hub_status")
        right_layout.addWidget(self._provenance)
        right_layout.addStretch(1)
        columns.addWidget(right, 3)
        root.addLayout(columns, 1)

        actions = QHBoxLayout()
        self._randomize = QPushButton("Randomize Avatar")
        self._reset = QPushButton("Reset")
        self._play = QPushButton("Pause Preview")
        self._save = QPushButton("Save Profile")
        self._sync = QPushButton("Build & Sync to Target")
        self._randomize.clicked.connect(self._randomize_profile)
        self._reset.clicked.connect(self._reset_profile)
        self._play.clicked.connect(self._toggle_preview)
        self._save.clicked.connect(self._emit_saved)
        self._sync.clicked.connect(self._emit_sync)
        for button in (
            self._randomize, self._reset, self._play, self._save, self._sync
        ):
            actions.addWidget(button)
        actions.addStretch(1)
        root.addLayout(actions)

        self._timer = QTimer(self)
        self._timer.setInterval(125)
        self._timer.timeout.connect(self._advance_frame)
        self._timer.start()
        self.set_profile({})

    def set_profile(self, profile):
        self._name.blockSignals(True)
        self._body.blockSignals(True)
        self._clip.blockSignals(True)
        for picker in self._appearance.values():
            picker.blockSignals(True)
        self._name.setText(str(profile.get(
            "xbox_avatar_display_name", "OFFLINE PLAYER"
        ))[:15])
        body_index = self._body.findData(profile.get("xbox_avatar_body", "xna-boy"))
        self._body.setCurrentIndex(max(0, body_index))
        clip_index = self._clip.findData(profile.get(
            "xbox_avatar_favorite_clip", "jump"
        ))
        self._clip.setCurrentIndex(max(0, clip_index))
        for field, picker in self._appearance.items():
            index = picker.findData(profile.get(
                f"xbox_avatar_{field}", "original"
            ))
            picker.setCurrentIndex(max(0, index))
        self._name.blockSignals(False)
        self._body.blockSignals(False)
        self._clip.blockSignals(False)
        for picker in self._appearance.values():
            picker.blockSignals(False)
        self._reload_animation()

    def set_achievement_totals(self, stats):
        self._stats = dict(stats or {})
        self._profile_changed()

    def profile_values(self):
        return {
            "xbox_avatar_display_name": (
                " ".join(self._name.text().split())[:15] or "OFFLINE PLAYER"
            ),
            "xbox_avatar_body": str(self._body.currentData() or "xna-boy"),
            "xbox_avatar_favorite_clip": str(
                self._clip.currentData() or "jump"
            ),
            **{
                f"xbox_avatar_{field}": str(
                    picker.currentData() or "original"
                )
                for field, picker in self._appearance.items()
            },
        }

    def _service_profile(self):
        values = self.profile_values()
        return {
            "display_name": values["xbox_avatar_display_name"],
            "body": values["xbox_avatar_body"],
            "favorite_clip": values["xbox_avatar_favorite_clip"],
            **{
                field: values[f"xbox_avatar_{field}"]
                for field in APPEARANCE_FIELDS
            },
        }

    def _asset_path(self):
        profile = self._service_profile()
        return Path(self._repo_root) / "assets" / "ipodjs" / "sources" / \
            "xbox360" / "avatar" / "master" / profile["body"] / \
            f"{profile['favorite_clip']}.rgba.png"

    def _reload_animation(self):
        path = self._asset_path()
        self._model_view.set_profile(self._service_profile())
        try:
            service = XboxAvatarService(self._repo_root, self._service_profile())
            self._frames = service.frames(
                service.profile.favorite_clip, "preview"
            )
            device_source = service.frames(
                service.profile.favorite_clip, "device", matte=True
            )
            encoded = encode_rav1(device_source, 125, opaque=True)
            self._device_frames, frame_ms = decode_rav1(encoded)
            encoded_bytes = len(encoded)
        except (OSError, ValueError) as exc:
            self._frames = []
            self._device_frames = []
            self._asset_status.setText(f"Authentic avatar pack unavailable: {exc}")
            self._stage.set_frame(None)
            self._ipod.set_frame(None)
            return
        self._timer.setInterval(frame_ms)
        self._frame_index = 0
        self._asset_status.setText(
            f"Real 3D XNA mesh · full 360° rotation · {len(self._frames)} "
            f"model-rendered motion frames · exact RAV2 iPod export "
            f"{encoded_bytes // 1024} KiB"
        )
        self._show_frame()
        self._profile_changed()

    def _show_frame(self):
        frame = self._frames[self._frame_index] if self._frames else None
        device = (self._device_frames[self._frame_index]
                  if self._device_frames else None)
        self._stage.set_frame(frame)
        self._ipod.set_frame(device)

    def _advance_frame(self):
        if not self._frames:
            return
        self._frame_index = (self._frame_index + 1) % len(self._frames)
        self._show_frame()

    def _profile_changed(self):
        name = self.profile_values()["xbox_avatar_display_name"]
        stats = {
            "gamerscore": self._stats.get("gamerscore", 0),
            "unlocked": self._stats.get("unlocked", 0),
            "total": self._stats.get("achievements", self._stats.get("total", 0)),
            "games": self._stats.get("games", 0),
        }
        self._stage.set_profile(name, stats)
        self._ipod.set_profile(name, stats)

    def _toggle_preview(self):
        if self._timer.isActive():
            self._timer.stop()
            self._play.setText("Play Preview")
        else:
            self._timer.start()
            self._play.setText("Pause Preview")

    def _toggle_center_view(self):
        showing_model = self._center_stack.currentWidget() is self._model_view
        self._center_stack.setCurrentWidget(
            self._stage if showing_model else self._model_view
        )
        self._view_toggle.setText(
            "Show 3D Model" if showing_model else "Show Motion"
        )

    def _randomize_profile(self):
        pickers = [self._body, self._clip, *self._appearance.values()]
        for picker in pickers:
            picker.blockSignals(True)
        self._body.setCurrentIndex((self._body.currentIndex() + 1) % self._body.count())
        self._clip.setCurrentIndex((self._clip.currentIndex() + 1) % self._clip.count())
        for offset, picker in enumerate(self._appearance.values(), 1):
            picker.setCurrentIndex(
                (picker.currentIndex() + offset) % picker.count()
            )
        for picker in pickers:
            picker.blockSignals(False)
        self._reload_animation()

    def _reset_profile(self):
        pickers = [self._body, self._clip, *self._appearance.values()]
        for picker in pickers:
            picker.blockSignals(True)
        self._name.setText("OFFLINE PLAYER")
        self._body.setCurrentIndex(self._body.findData("xna-boy"))
        self._clip.setCurrentIndex(self._clip.findData("jump"))
        for picker in self._appearance.values():
            picker.setCurrentIndex(picker.findData("original"))
        for picker in pickers:
            picker.blockSignals(False)
        self._reload_animation()

    def _emit_saved(self):
        self.profile_saved.emit(self.profile_values())

    def _emit_sync(self):
        self._emit_saved()
        self.sync_requested.emit()
