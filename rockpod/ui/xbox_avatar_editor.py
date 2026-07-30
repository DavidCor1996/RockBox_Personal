"""RockPod creator and exact iPod preview for authentic Xbox 360 avatars."""

from __future__ import annotations

import os
import random
from pathlib import Path

from PIL import Image
from PySide6.QtCore import Qt, QTimer, Signal
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

from services.xbox_avatar import (
    ACCENT_PALETTE,
    APPEARANCE_FIELDS,
    AvatarProfile,
    COLOUR_FIELDS,
    COLOUR_PALETTES,
    DESIGN_SLOTS,
    EMOTE_CLIPS,
    SCALE_LABELS,
    STYLE_LABELS,
    STYLE_SLOTS,
    XboxAvatarService,
    decode_rav1,
    encode_rav1,
)
from services.xbox_avatar_designs import DESIGN_LABELS
from services.xbox_avatar_render import MARKETPLACE_KINDS, MarketplacePack
from services.xbox_avatar_render import DECAL_LABELS


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


FIELD_LABELS = {
    "hair_style": "Hair style",
    "top_style": "Top",
    "bottom_style": "Bottom",
    "shoes_style": "Shoes",
    "skin_colour": "Skin tone",
    "hair_colour": "Hair colour",
    "top_colour": "Top colour",
    "bottom_colour": "Bottom colour",
    "shoes_colour": "Shoe colour",
    "eye_colour": "Eye colour",
    "brow_colour": "Eyebrow colour",
    "lip_colour": "Lip colour",
    "decal": "Chest print",
    "accent_colour": "Pattern accent",
    "top_design": "Top pattern",
    "top_design_scale": "Top pattern size",
    "bottom_design": "Bottom pattern",
    "bottom_design_scale": "Bottom pattern size",
    "shoes_design": "Shoe pattern",
    "shoes_design_scale": "Shoe pattern size",
    "costume": "Marketplace costume",
    "marketplace_top": "Marketplace top",
    "headwear": "Headwear",
    "prop": "Held prop",
}
TURNTABLE_ANGLE_STEP = 15.0


class _AvatarTurntable(QWidget):
    """Drag-to-rotate view rendered by the same code that builds the export."""

    def __init__(self, repo_root, parent=None):
        super().__init__(parent)
        self._repo_root = repo_root
        self._profile = {}
        self._angle_index = 0
        self._cache = {}
        self._drag_origin = None
        self._drag_start_index = 0
        self.setMinimumSize(280, 360)
        self.setCursor(Qt.OpenHandCursor)

    def _refresh_style_choices(self):
        """Offer only the real garments Microsoft fitted to the chosen body."""
        profile = AvatarProfile.from_mapping({
            "body": str(self._body.currentData() or "xna-boy")
        })
        allowed = profile.styles_for_body()
        for slot in STYLE_SLOTS:
            picker = self._appearance[f"{slot}_style"]
            previous = picker.currentData()
            picker.blockSignals(True)
            picker.clear()
            for choice in allowed[slot]:
                picker.addItem(STYLE_LABELS.get(choice, choice), choice)
            index = picker.findData(previous)
            picker.setCurrentIndex(max(0, index))
            picker.blockSignals(False)

    def _body_changed(self):
        self._refresh_style_choices()
        self._reload_animation()

    def set_profile(self, profile):
        if profile != self._profile:
            self._profile = dict(profile)
            self._cache.clear()
        self.update()

    def _frame(self):
        if self._angle_index in self._cache:
            return self._cache[self._angle_index]
        try:
            service = XboxAvatarService(self._repo_root, self._profile)
            frames = service.frames_at_angles(
                [self._angle_index * TURNTABLE_ANGLE_STEP],
                size=(self.width() or 280, self.height() or 360),
            )
        except (OSError, ValueError):
            return None
        self._cache[self._angle_index] = frames[0]
        return frames[0]

    def mousePressEvent(self, event):
        self._drag_origin = event.position().x()
        self._drag_start_index = self._angle_index
        self.setCursor(Qt.ClosedHandCursor)

    def mouseMoveEvent(self, event):
        if self._drag_origin is None:
            return
        delta = event.position().x() - self._drag_origin
        steps = int(delta / 12.0)
        index = (self._drag_start_index - steps) % 24
        if index != self._angle_index:
            self._angle_index = index
            self.update()

    def mouseReleaseEvent(self, _event):
        self._drag_origin = None
        self.setCursor(Qt.OpenHandCursor)

    def paintEvent(self, _event):
        painter = QPainter(self)
        gradient = QLinearGradient(0, 0, 0, self.height())
        gradient.setColorAt(0.0, QColor("#f7f8f9"))
        gradient.setColorAt(1.0, QColor("#b8bcc0"))
        painter.fillRect(self.rect(), gradient)
        frame = self._frame()
        if frame is not None:
            image = _qimage(frame)
            painter.drawImage(
                (self.width() - image.width()) // 2,
                (self.height() - image.height()) // 2,
                image,
            )
        painter.setPen(QColor("#4a4f55"))
        painter.drawText(8, self.height() - 8, "Drag to rotate 360°")


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
        self._body.currentIndexChanged.connect(self._body_changed)
        self._clip = QComboBox()
        for clip in EMOTE_CLIPS:
            self._clip.addItem(CLIP_LABELS[clip], clip)
        self._clip.currentIndexChanged.connect(self._reload_animation)
        form.addRow("Display name", self._name)
        form.addRow("Body", self._body)
        form.addRow("Animation", self._clip)
        self._appearance = {}
        for slot in STYLE_SLOTS:
            field = f"{slot}_style"
            picker = QComboBox()
            picker.currentIndexChanged.connect(self._reload_animation)
            self._appearance[field] = picker
            form.addRow(FIELD_LABELS[field], picker)
        for field in COLOUR_FIELDS:
            picker = QComboBox()
            for value, label, colour in COLOUR_PALETTES[field]:
                picker.addItem(label, value)
                if colour:
                    picker.setItemData(
                        picker.count() - 1, QColor(colour), Qt.DecorationRole
                    )
            picker.currentIndexChanged.connect(self._reload_animation)
            self._appearance[field] = picker
            form.addRow(FIELD_LABELS[field], picker)
        for slot in DESIGN_SLOTS:
            design = QComboBox()
            design.addItem("No pattern", "none")
            for value, label in DESIGN_LABELS.items():
                design.addItem(label, value)
            design.currentIndexChanged.connect(self._reload_animation)
            self._appearance[f"{slot}_design"] = design
            form.addRow(FIELD_LABELS[f"{slot}_design"], design)

            scale = QComboBox()
            for value, label in SCALE_LABELS.items():
                scale.addItem(label, value)
            scale.setCurrentIndex(max(0, scale.findData("medium")))
            scale.currentIndexChanged.connect(self._reload_animation)
            self._appearance[f"{slot}_design_scale"] = scale
            form.addRow(FIELD_LABELS[f"{slot}_design_scale"], scale)

        accent = QComboBox()
        for value, label, colour in ACCENT_PALETTE:
            accent.addItem(label, value)
            accent.setItemData(accent.count() - 1, QColor(colour),
                               Qt.DecorationRole)
        accent.setCurrentIndex(max(0, accent.findData("black")))
        accent.currentIndexChanged.connect(self._reload_animation)
        self._appearance["accent_colour"] = accent
        form.addRow(FIELD_LABELS["accent_colour"], accent)

        decal = QComboBox()
        for value, label in DECAL_LABELS.items():
            decal.addItem(label, value)
        decal.currentIndexChanged.connect(self._reload_animation)
        self._appearance["decal"] = decal
        form.addRow(FIELD_LABELS["decal"], decal)

        # Archived Xbox 360 Marketplace items, listed from the installed pack.
        pack = MarketplacePack(self._repo_root)
        catalogue = pack.items() if pack.available() else {}
        for field, kind in MARKETPLACE_KINDS.items():
            picker = QComboBox()
            picker.addItem("None", "none")
            for name, record in sorted(
                catalogue.items(), key=lambda item: item[1]["title"]
            ):
                if record["slot"] == kind:
                    picker.addItem(record["title"], name)
            picker.currentIndexChanged.connect(self._reload_animation)
            self._appearance[field] = picker
            form.addRow(FIELD_LABELS[field], picker)
        self._refresh_style_choices()
        left_layout.addLayout(form)
        fixed = QLabel(
            "Every part is a real mesh from Microsoft's XNA Avatar pack and "
            "every colour tints an original texture. Chest prints use real "
            "Rockbox and stock iPod artwork already in this tree. Headwear "
            "and accessories need an authenticated owned import."
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
        center_header.addWidget(QLabel("LIVE XNA MODEL — 360°"))
        center_header.addStretch(1)
        self._view_toggle = QPushButton("Show Motion")
        self._view_toggle.clicked.connect(self._toggle_center_view)
        center_header.addWidget(self._view_toggle)
        center_layout.addLayout(center_header)
        self._center_stack = QStackedWidget()
        self._model_view = _AvatarTurntable(self._repo_root)
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

    def _refresh_style_choices(self):
        """Offer only the real garments Microsoft fitted to the chosen body."""
        profile = AvatarProfile.from_mapping({
            "body": str(self._body.currentData() or "xna-boy")
        })
        allowed = profile.styles_for_body()
        for slot in STYLE_SLOTS:
            picker = self._appearance[f"{slot}_style"]
            previous = picker.currentData()
            picker.blockSignals(True)
            picker.clear()
            for choice in allowed[slot]:
                picker.addItem(STYLE_LABELS.get(choice, choice), choice)
            index = picker.findData(previous)
            picker.setCurrentIndex(max(0, index))
            picker.blockSignals(False)

    def _body_changed(self):
        self._refresh_style_choices()
        self._reload_animation()

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
        self._refresh_style_choices()
        defaults = AvatarProfile()
        for field, picker in self._appearance.items():
            index = picker.findData(profile.get(
                f"xbox_avatar_{field}",
                getattr(defaults, field, "original"),
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
                f"xbox_avatar_{field}": str(picker.currentData() or "")
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

    def _reload_animation(self):
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
            f"Real XNA meshes · full 360° rotation · {len(self._frames)} "
            f"rig-rendered motion frames · exact RAV2 iPod export "
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
        self._body.blockSignals(True)
        self._body.setCurrentIndex(random.randrange(self._body.count()))
        self._body.blockSignals(False)
        self._refresh_style_choices()
        pickers = [self._clip, *self._appearance.values()]
        for picker in pickers:
            picker.blockSignals(True)
            if picker.count():
                picker.setCurrentIndex(random.randrange(picker.count()))
            picker.blockSignals(False)
        self._reload_animation()

    def _reset_profile(self):
        pickers = [self._body, self._clip, *self._appearance.values()]
        for picker in pickers:
            picker.blockSignals(True)
        self._name.setText("OFFLINE PLAYER")
        self._body.setCurrentIndex(self._body.findData("xna-boy"))
        self._clip.setCurrentIndex(self._clip.findData("jump"))
        for picker in pickers:
            picker.blockSignals(False)
        self._refresh_style_choices()
        defaults = AvatarProfile()
        for field, picker in self._appearance.items():
            picker.blockSignals(True)
            picker.setCurrentIndex(
                max(0, picker.findData(getattr(defaults, field, "original")))
            )
            picker.blockSignals(False)
        self._reload_animation()

    def _emit_saved(self):
        self.profile_saved.emit(self.profile_values())

    def _emit_sync(self):
        self._emit_saved()
        self.sync_requested.emit()
