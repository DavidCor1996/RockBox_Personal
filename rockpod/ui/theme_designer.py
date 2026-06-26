"""Guided iPone designer."""

from __future__ import annotations

import os
import shutil
import subprocess

from PySide6.QtCore import Qt, QRectF, Signal, QTimer, QProcess, QEvent
from PySide6.QtGui import QColor, QFont, QPainter, QPainterPath, QPen, QPixmap, QWindow
from PySide6.QtWidgets import (
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
    QPushButton,
    QScrollArea,
    QSizePolicy,
    QStackedLayout,
    QVBoxLayout,
    QWidget,
)

from services.rockbox_simulator import RockboxSimulatorService
from ui.process_helpers import start_hidden_process, start_qprocess


COLOR_PROFILES = {
    "custom": {
        "label": "Custom",
        "colors": {},
    },
    "default": {
        "label": "Default iPone",
        "colors": {
            "background": "100F16",
            "foreground": "F7F4FA",
            "selector_start": "2B2234",
            "selector_end": "9D7AE6",
            "selector_text": "FCF9FF",
            "list_separator": "1A1621",
        },
    },
    "blue": {
        "label": "Blue Glass",
        "colors": {
            "background": "0D111B",
            "foreground": "F3F8FF",
            "selector_start": "172A47",
            "selector_end": "4FA3FF",
            "selector_text": "F8FBFF",
            "list_separator": "142033",
        },
    },
    "green": {
        "label": "Green Glass",
        "colors": {
            "background": "0E1512",
            "foreground": "F2FFF8",
            "selector_start": "17342A",
            "selector_end": "55D08A",
            "selector_text": "F7FFF9",
            "list_separator": "14241D",
        },
    },
    "teal": {
        "label": "Teal Mint",
        "colors": {
            "background": "071312",
            "foreground": "E9FFFA",
            "selector_start": "0E3A35",
            "selector_end": "22D3C5",
            "selector_text": "03110F",
            "list_separator": "12302E",
        },
    },
    "indigo": {
        "label": "Indigo Glow",
        "colors": {
            "background": "0B1020",
            "foreground": "F3F5FF",
            "selector_start": "1B2553",
            "selector_end": "A5B4FC",
            "selector_text": "050816",
            "list_separator": "172044",
        },
    },
    "rose": {
        "label": "Rose Pop",
        "colors": {
            "background": "16080F",
            "foreground": "FFF1F6",
            "selector_start": "4A1730",
            "selector_end": "FB7185",
            "selector_text": "19030A",
            "list_separator": "34111F",
        },
    },
    "amber": {
        "label": "Amber Warm",
        "colors": {
            "background": "151006",
            "foreground": "FFF8E6",
            "selector_start": "46320C",
            "selector_end": "FBBF24",
            "selector_text": "170F02",
            "list_separator": "33240A",
        },
    },
    "orange": {
        "label": "Orange Energy",
        "colors": {
            "background": "160B05",
            "foreground": "FFF4EC",
            "selector_start": "4B210C",
            "selector_end": "FB923C",
            "selector_text": "180801",
            "list_separator": "35180A",
        },
    },
    "graphite": {
        "label": "Graphite Steel",
        "colors": {
            "background": "0D1014",
            "foreground": "F4F7FA",
            "selector_start": "242B35",
            "selector_end": "94A3B8",
            "selector_text": "05070A",
            "list_separator": "1D232B",
        },
    },
    "cyberpunk": {
        "label": "Cyberpunk Neon",
        "colors": {
            "background": "070B10",
            "foreground": "FCEE0A",
            "selector_start": "12333B",
            "selector_end": "00F0FF",
            "selector_text": "05070A",
            "list_separator": "FF003C",
        },
    },
}


LIGHT_COLOR_PROFILES = {
    "default": {
        "background": "F5F1FA",
        "foreground": "15121D",
        "selector_start": "DCD3EA",
        "selector_end": "9D7AE6",
        "selector_text": "15121D",
        "list_separator": "DED6E8",
    },
    "blue": {
        "background": "F1F7FF",
        "foreground": "0D111B",
        "selector_start": "D3E7FF",
        "selector_end": "4FA3FF",
        "selector_text": "07111F",
        "list_separator": "D8E8F8",
    },
    "green": {
        "background": "F1FBF5",
        "foreground": "0E1512",
        "selector_start": "D5F0DF",
        "selector_end": "55D08A",
        "selector_text": "0B1710",
        "list_separator": "D7EADF",
    },
    "teal": {
        "background": "EFFFFB",
        "foreground": "06211E",
        "selector_start": "C7F4EE",
        "selector_end": "14B8A6",
        "selector_text": "021B19",
        "list_separator": "D7EFEA",
    },
    "indigo": {
        "background": "F3F5FF",
        "foreground": "10142A",
        "selector_start": "DDE2FF",
        "selector_end": "A5B4FC",
        "selector_text": "050816",
        "list_separator": "D7DBF5",
    },
    "rose": {
        "background": "FFF1F6",
        "foreground": "2B0713",
        "selector_start": "FFD6E2",
        "selector_end": "FDA4AF",
        "selector_text": "23030B",
        "list_separator": "F4D3DC",
    },
    "amber": {
        "background": "FFF8E6",
        "foreground": "241500",
        "selector_start": "FFE8A3",
        "selector_end": "FBBF24",
        "selector_text": "1F1300",
        "list_separator": "F0DFC0",
    },
    "orange": {
        "background": "FFF4EC",
        "foreground": "2A1000",
        "selector_start": "FFDCC2",
        "selector_end": "FDBA74",
        "selector_text": "210A00",
        "list_separator": "EFD7C7",
    },
    "graphite": {
        "background": "F6F8FA",
        "foreground": "111827",
        "selector_start": "DDE3EA",
        "selector_end": "94A3B8",
        "selector_text": "05070A",
        "list_separator": "D8DEE6",
    },
    "cyberpunk": {
        "background": "FCEE0A",
        "foreground": "090A0F",
        "selector_start": "FFE94A",
        "selector_end": "00D8F5",
        "selector_text": "05070A",
        "list_separator": "FF003C",
    },
}


def _profile_colors(profile_id, appearance_mode):
    profile_id = str(profile_id or "custom")
    if str(appearance_mode or "dark") == "light" and profile_id in LIGHT_COLOR_PROFILES:
        return LIGHT_COLOR_PROFILES[profile_id]
    return COLOR_PROFILES.get(profile_id, {}).get("colors", {})


class _ColorButton(QPushButton):
    color_changed = Signal(str)

    def __init__(self, label, parent=None):
        super().__init__(label, parent)
        self._hex = "000000"
        self.clicked.connect(self._choose)
        self._sync_style()

    def set_hex(self, value):
        text = str(value or "").strip().lstrip("#").upper()
        if len(text) == 6:
            self._hex = text
            self._sync_style()

    def hex(self):
        return self._hex

    def _choose(self):
        color = QColorDialog.getColor(QColor(f"#{self._hex}"), self, "Select Color")
        if color.isValid():
            self._hex = color.name()[1:].upper()
            self._sync_style()
            self.color_changed.emit(self._hex)

    def _sync_style(self):
        text_color = "#111111" if QColor(f"#{self._hex}").lightness() > 150 else "#f7f7f7"
        self.setText(f"#{self._hex}")
        self.setStyleSheet(
            "QPushButton {"
            f"background: #{self._hex}; color: {text_color};"
            "border: 1px solid #6d6d6d; border-radius: 4px; padding: 6px 10px;"
            "}"
        )


class _RightPaneCropWidget(QWidget):
    position_changed = Signal(int, int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(260, 360)
        self.setMouseTracking(True)
        self._image_path = ""
        self._fit_mode = "fill"
        self._offset_x = 0
        self._offset_y = 0
        self._matte_hex = "100F16"
        self._drag_start = None
        self._drag_offsets = (0, 0)

    def set_image(self, image_path, fit_mode, offset_x, offset_y, matte_hex):
        self._image_path = str(image_path or "").strip()
        self._fit_mode = str(fit_mode or "fill").strip().lower() or "fill"
        self._offset_x = self._clamp_offset(offset_x)
        self._offset_y = self._clamp_offset(offset_y)
        self._matte_hex = str(matte_hex or "100F16").strip().lstrip("#")[:6] or "100F16"
        self.update()

    def offsets(self):
        return self._offset_x, self._offset_y

    def center(self):
        self._offset_x = 0
        self._offset_y = 0
        self.position_changed.emit(self._offset_x, self._offset_y)
        self.update()

    def paintEvent(self, _event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.fillRect(self.rect(), QColor("#111116"))
        pane = self._pane_rect()
        painter.fillRect(pane, QColor(f"#{self._matte_hex}"))

        pixmap = QPixmap(self._image_path) if self._image_path and os.path.isfile(self._image_path) else QPixmap()
        if pixmap.isNull():
            painter.setPen(QColor("#F7F4FA"))
            painter.drawText(pane, Qt.AlignCenter | Qt.TextWordWrap, "Choose a right-side image first")
        else:
            draw_rect = self._image_draw_rect(pixmap)
            scale = pane.width() / 160.0
            mapped = QRectF(
                pane.x() + draw_rect.x() * scale,
                pane.y() + draw_rect.y() * scale,
                draw_rect.width() * scale,
                draw_rect.height() * scale,
            )
            painter.save()
            painter.setClipRect(pane)
            painter.drawPixmap(mapped, pixmap, QRectF(pixmap.rect()))
            painter.restore()

        painter.setPen(QPen(QColor("#FFFFFF"), 2))
        painter.drawRoundedRect(pane, 10, 10)
        painter.setPen(QPen(QColor(255, 255, 255, 90), 1))
        painter.drawLine(pane.center().x(), pane.top(), pane.center().x(), pane.bottom())
        painter.drawLine(pane.left(), pane.center().y(), pane.right(), pane.center().y())

    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton and self._pane_rect().contains(event.position()):
            self._drag_start = event.position()
            self._drag_offsets = (self._offset_x, self._offset_y)
            self.setCursor(Qt.ClosedHandCursor)
        super().mousePressEvent(event)

    def mouseMoveEvent(self, event):
        if self._drag_start is None:
            if self._pane_rect().contains(event.position()):
                self.setCursor(Qt.OpenHandCursor)
            else:
                self.unsetCursor()
            super().mouseMoveEvent(event)
            return

        pane = self._pane_rect()
        pane_scale = 160.0 / max(1.0, pane.width())
        delta_x = (event.position().x() - self._drag_start.x()) * pane_scale
        delta_y = (event.position().y() - self._drag_start.y()) * pane_scale
        extra_x, extra_y = self._extra_space()
        self._offset_x = self._dragged_offset(self._drag_offsets[0], extra_x, delta_x)
        self._offset_y = self._dragged_offset(self._drag_offsets[1], extra_y, delta_y)
        self.position_changed.emit(self._offset_x, self._offset_y)
        self.update()
        super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event):
        self._drag_start = None
        self.unsetCursor()
        super().mouseReleaseEvent(event)

    def _pane_rect(self):
        margin = 16
        available = self.rect().adjusted(margin, margin, -margin, -margin)
        scale = min(available.width() / 160.0, available.height() / 240.0)
        width = 160.0 * scale
        height = 240.0 * scale
        return QRectF(
            available.center().x() - width / 2,
            available.center().y() - height / 2,
            width,
            height,
        )

    def _image_draw_rect(self, pixmap):
        pane_width = 160.0
        pane_height = 240.0
        image_width = max(1.0, float(pixmap.width()))
        image_height = max(1.0, float(pixmap.height()))
        if self._fit_mode == "stretch":
            return QRectF(0, 0, pane_width, pane_height)
        if self._fit_mode == "fit":
            scale = min(pane_width / image_width, pane_height / image_height)
            width = image_width * scale
            height = image_height * scale
            left = self._offset_position(pane_width - width, self._offset_x)
            top = self._offset_position(pane_height - height, self._offset_y)
            return QRectF(left, top, width, height)
        scale = max(pane_width / image_width, pane_height / image_height)
        width = image_width * scale
        height = image_height * scale
        left = -self._offset_position(width - pane_width, self._offset_x)
        top = -self._offset_position(height - pane_height, self._offset_y)
        return QRectF(left, top, width, height)

    def _extra_space(self):
        pixmap = QPixmap(self._image_path) if self._image_path and os.path.isfile(self._image_path) else QPixmap()
        if pixmap.isNull() or self._fit_mode == "stretch":
            return 0.0, 0.0
        pane_width = 160.0
        pane_height = 240.0
        image_width = max(1.0, float(pixmap.width()))
        image_height = max(1.0, float(pixmap.height()))
        if self._fit_mode == "fit":
            scale = min(pane_width / image_width, pane_height / image_height)
            return max(0.0, pane_width - image_width * scale), max(0.0, pane_height - image_height * scale)
        scale = max(pane_width / image_width, pane_height / image_height)
        return max(0.0, image_width * scale - pane_width), max(0.0, image_height * scale - pane_height)

    def _dragged_offset(self, start_offset, extra_space, delta):
        if extra_space <= 0:
            return 0
        direction = 1 if self._fit_mode == "fit" else -1
        return self._clamp_offset(start_offset + direction * (delta / extra_space) * 200.0)

    @staticmethod
    def _offset_position(extra_space, offset):
        extra_space = max(0.0, float(extra_space))
        if extra_space <= 0:
            return 0.0
        return extra_space * ((_RightPaneCropWidget._clamp_offset(offset) + 100) / 200.0)

    @staticmethod
    def _clamp_offset(value):
        try:
            number = int(round(float(value)))
        except (TypeError, ValueError):
            number = 0
        return max(-100, min(100, number))


class _RightPanePositionDialog(QDialog):
    def __init__(self, parent, image_path, fit_mode, offset_x, offset_y, matte_hex):
        super().__init__(parent)
        self.setWindowTitle("Position Right-Side Image")
        self._offset_x = int(offset_x or 0)
        self._offset_y = int(offset_y or 0)

        layout = QVBoxLayout(self)
        hint = QLabel("Drag the image until the right side is framed the way you want.")
        hint.setWordWrap(True)
        layout.addWidget(hint)

        self._crop = _RightPaneCropWidget()
        self._crop.set_image(image_path, fit_mode, self._offset_x, self._offset_y, matte_hex)
        self._crop.position_changed.connect(self._set_offsets)
        layout.addWidget(self._crop, 1)

        actions = QHBoxLayout()
        center = QPushButton("Center")
        center.clicked.connect(self._crop.center)
        actions.addWidget(center)
        actions.addStretch(1)
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        actions.addWidget(buttons)
        layout.addLayout(actions)

    def offsets(self):
        return self._offset_x, self._offset_y

    def _set_offsets(self, offset_x, offset_y):
        self._offset_x = int(offset_x)
        self._offset_y = int(offset_y)


class _ScreenPreview(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFocusPolicy(Qt.StrongFocus)
        self.setMinimumSize(320, 420)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self._state = {
            "theme_name": "",
            "width": 320,
            "height": 240,
            "font_label": "",
            "wallpaper_path": "",
            "charging_wallpaper_path": "",
            "menu_backdrop_path": "",
            "simulator_preview_path": "",
            "simulator_status_text": "",
            "colors": {},
        }
        self._mode = "main_menu"

    def set_preview(self, state, mode):
        self._state = dict(state or {})
        self._mode = mode or "main_menu"
        self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.fillRect(self.rect(), QColor("#0b0b0d"))
        if self._paint_simulator_surface(painter, QRectF(self.rect())):
            return
        painter.setPen(QColor("#F7F4FA"))
        font = QFont()
        font.setPointSize(11)
        font.setBold(True)
        painter.setFont(font)
        text = self._state.get("simulator_status_text") or "Simulator preview unavailable"
        painter.drawText(self.rect().adjusted(16, 16, -16, -16), Qt.AlignCenter | Qt.TextWordWrap, text)

    def _paint_simulator_surface(self, painter, target_rect):
        shot = self._state.get("simulator_preview_path", "")
        if shot and os.path.isfile(shot):
            pixmap = QPixmap()
            pixmap.load(shot)
            if not pixmap.isNull():
                fitted = pixmap.scaled(
                    target_rect.toRect().size(),
                    Qt.KeepAspectRatio,
                    Qt.SmoothTransformation,
                )
                x = int(target_rect.center().x() - fitted.width() / 2)
                y = int(target_rect.center().y() - fitted.height() / 2)
                painter.drawPixmap(x, y, fitted)
                return True
        return False

    def mousePressEvent(self, event):
        super().mousePressEvent(event)
        self.setFocus(Qt.MouseFocusReason)

class _EmbeddedSimulatorPreview(QWidget):
    _QT_KEY_MAP = {
        Qt.Key_Left: "Left",
        Qt.Key_Right: "Right",
        Qt.Key_Up: "Up",
        Qt.Key_Down: "Down",
        Qt.Key_Return: "Return",
        Qt.Key_Enter: "Return",
        Qt.Key_Escape: "Escape",
        Qt.Key_Backspace: "BackSpace",
        Qt.Key_Space: "space",
        Qt.Key_A: "a",
        Qt.Key_D: "d",
        Qt.Key_S: "s",
        Qt.Key_W: "w",
    }

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFocusPolicy(Qt.StrongFocus)
        self._binary_path = ""
        self._simdisk_path = ""
        self._process = None
        self._hidden_process = None
        self._window = None
        self._container = None
        self._attach_attempts = 0

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        self._layout = layout

        self._placeholder = QLabel("Simulator unavailable")
        self._placeholder.setAlignment(Qt.AlignCenter)
        self._placeholder.setWordWrap(True)
        self._layout.addWidget(self._placeholder)

    def supports_live_embed(self):
        return (
            os.environ.get("XDG_SESSION_TYPE", "").strip().lower() != "wayland"
            and shutil.which("xdotool") is not None
        )

    def supports_window_controls(self):
        return shutil.which("xdotool") is not None

    def preview_capture_path(self):
        simdisk = str(self._simdisk_path or "").strip()
        if not simdisk:
            return ""
        return os.path.join(simdisk, ".rockbox", "live_preview.bmp")

    def preview_capture_sim_path(self):
        if not self._simdisk_path:
            return ""
        return "/.rockbox/live_preview.bmp"

    def status_text(self):
        return self._placeholder.text().strip()

    def set_target(self, binary_path, simdisk_path):
        binary = str(binary_path or "").strip()
        simdisk = str(simdisk_path or "").strip()
        if binary == self._binary_path and simdisk == self._simdisk_path:
            return
        self.shutdown()
        self._binary_path = binary
        self._simdisk_path = simdisk
        if not self._can_launch():
            self._placeholder.setText("Simulator unavailable")
        else:
            self._placeholder.setText("Press Refresh Preview to rebuild the simulator preview")
        self._show_placeholder()

    def ensure_running(self):
        if not self._can_launch():
            self._placeholder.setText("Simulator unavailable")
            self._show_placeholder()
            return
        if self._process and self._process.state() != QProcess.NotRunning:
            return
        if self._hidden_process and self._hidden_process.poll() is None:
            return
        if self.supports_live_embed() or self.supports_window_controls():
            self._start_process()
        else:
            self._start_hidden_process()

    def restart(self):
        self.shutdown()
        if self._can_launch():
            self.ensure_running()

    def shutdown(self):
        self._clear_container()
        if self._hidden_process and self._hidden_process.poll() is None:
            self._hidden_process.terminate()
            try:
                self._hidden_process.wait(timeout=1.0)
            except subprocess.TimeoutExpired:
                self._hidden_process.kill()
                self._hidden_process.wait(timeout=1.0)
        self._hidden_process = None
        if self._process:
            if self._process.state() != QProcess.NotRunning:
                self._process.terminate()
                if not self._process.waitForFinished(500):
                    self._process.kill()
                    self._process.waitForFinished(500)
            self._process.deleteLater()
            self._process = None
        self._attach_attempts = 0

    def _can_launch(self):
        return bool(self._binary_path and self._simdisk_path and os.path.isfile(self._binary_path) and os.path.isdir(self._simdisk_path))

    def _start_process(self):
        self._placeholder.setText("Starting simulator..." if self.supports_live_embed() else "Starting baked-in simulator preview...")
        self._show_placeholder()
        self._process = start_qprocess(
            self,
            self._simulator_command(),
            cwd=os.path.dirname(self._binary_path),
            env=self._simulator_env(hidden=not self.supports_live_embed() and not self.supports_window_controls()),
            error_handler=self._on_process_error,
            finished_handler=self._on_process_finished,
        )
        if self.supports_live_embed():
            self._schedule_attach()

    def _start_hidden_process(self):
        self._placeholder.setText("Starting baked-in simulator preview...")
        self._show_placeholder()
        self._hidden_process = start_hidden_process(
            self._simulator_command(),
            cwd=os.path.dirname(self._binary_path),
            env=self._simulator_env(hidden=True),
        )

    def _simulator_command(self):
        return [self._binary_path, "--nobackground", "--root", self._simdisk_path]

    def _simulator_env(self, hidden=False):
        env = {}
        runtime_root = self._runtime_root()
        if runtime_root:
            env["RBROOT"] = runtime_root
        capture_path = self.preview_capture_sim_path()
        if capture_path:
            env["ROCKPOD_SIM_PREVIEW_BMP"] = capture_path
            env["ROCKPOD_SIM_PREVIEW_INTERVAL_MS"] = "200"
            if hidden:
                env["ROCKPOD_SIM_HIDDEN"] = "1"
        return env

    def _schedule_attach(self):
        self._attach_attempts = 0
        QTimer.singleShot(300, self._try_attach)

    def _try_attach(self):
        if self._container is not None:
            return
        if not self._process or self._process.state() == QProcess.NotRunning:
            self._placeholder.setText("Simulator exited")
            self._show_placeholder()
            return
        pid = int(self._process.processId() or 0)
        if pid <= 0:
            self._retry_attach("Waiting for simulator process...")
            return
        ids = self._window_ids_for_pid(pid)
        if not ids:
            self._retry_attach("Waiting for simulator window...")
            return
        self._attach_window(int(ids[-1]))

    def _retry_attach(self, message):
        self._attach_attempts += 1
        if self._attach_attempts >= 15:
            self._placeholder.setText(message)
            self._show_placeholder()
            return
        self._placeholder.setText(message)
        self._show_placeholder()
        QTimer.singleShot(250, self._try_attach)

    def _attach_window(self, win_id):
        self._clear_container()
        self._window = QWindow.fromWinId(win_id)
        if self._window is None:
            self._placeholder.setText("Could not attach simulator window")
            self._show_placeholder()
            return
        self._container = QWidget.createWindowContainer(self._window, self)
        self._container.setFocusPolicy(Qt.StrongFocus)
        self._layout.addWidget(self._container)
        self._container.show()
        self._container.setFocus(Qt.OtherFocusReason)
        self._container.activateWindow()
        self._window.requestActivate()
        self._placeholder.hide()

    def _clear_container(self):
        if self._container:
            self._layout.removeWidget(self._container)
            self._container.deleteLater()
            self._container = None
        self._window = None

    def _show_placeholder(self):
        self._clear_container()
        self._placeholder.show()

    def mousePressEvent(self, event):
        super().mousePressEvent(event)
        self.setFocus(Qt.MouseFocusReason)
        if self._container is not None:
            self._container.setFocus(Qt.MouseFocusReason)
            self._container.activateWindow()
            if self._window is not None:
                self._window.requestActivate()

    def focusInEvent(self, event):
        super().focusInEvent(event)
        if self._container is not None:
            self._container.setFocus(Qt.OtherFocusReason)
            self._container.activateWindow()
            if self._window is not None:
                self._window.requestActivate()

    def _on_process_finished(self):
        self._clear_container()
        self._placeholder.setText("Simulator exited")
        self._placeholder.show()

    def _on_process_error(self, _error):
        self._clear_container()
        self._placeholder.setText("Simulator failed to start")
        self._placeholder.show()

    def send_control(self, action):
        key_name = {
            "menu": "Escape",
            "up": "Up",
            "down": "Down",
            "left": "Left",
            "right": "Right",
            "select": "Return",
            "play": "space",
            "hold": "h",
        }.get(str(action or "").strip().lower())
        if not key_name:
            return False
        return self.send_key(key_name)

    def forward_qt_key(self, key):
        key_name = self._QT_KEY_MAP.get(key)
        if not key_name:
            return False
        return self.send_key(key_name)

    def send_key(self, key_name):
        if shutil.which("xdotool") is None:
            return False
        pid = self._active_pid()
        if pid <= 0:
            return False
        window_ids = self._window_ids_for_pid(pid)
        if not window_ids:
            return False
        return RockboxSimulatorService._send_key_to_window_id(window_ids[-1], key_name)

    def _active_pid(self):
        if self._process and self._process.state() != QProcess.NotRunning:
            return int(self._process.processId() or 0)
        if self._hidden_process and self._hidden_process.poll() is None:
            return int(self._hidden_process.pid)
        return 0

    def _runtime_root(self):
        simdisk = str(self._simdisk_path or "").strip()
        if not simdisk:
            return ""
        return os.path.dirname(simdisk)

    @staticmethod
    def _window_ids_for_pid(pid):
        window_id = RockboxSimulatorService._wait_for_window_id(pid, timeout=1.0)
        return [window_id] if window_id else []

    def schedule_controls(self, actions, delay_ms=1600, step_ms=500):
        queue = [str(action or "").strip().lower() for action in (actions or []) if str(action or "").strip()]
        if not queue:
            return

        def _dispatch(index=0):
            if index >= len(queue):
                return
            self.send_control(queue[index])
            if index + 1 < len(queue):
                QTimer.singleShot(step_ms, lambda: _dispatch(index + 1))

        QTimer.singleShot(delay_ms, lambda: _dispatch(0))


class ThemeDesignerWidget(QWidget):
    profile_selected = Signal(str)
    variant_selected = Signal(str)
    preview_changed = Signal(dict, str)
    simulator_refresh_requested = Signal(dict)
    save_requested = Signal(dict)
    rename_requested = Signal(str, str)
    duplicate_requested = Signal(str)
    delete_requested = Signal(str)
    deploy_device_requested = Signal(dict)
    deploy_simulator_requested = Signal(dict)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFocusPolicy(Qt.StrongFocus)
        self._profiles = {}
        self._variants = {}
        self._current_variant_id = ""
        self._base_wallpaper_path = ""
        self._base_charging_wallpaper_path = ""
        self._base_right_pane_wallpaper_path = ""
        self._base_menu_backdrop_path = ""
        self._base_simulator_preview_path = ""
        self._right_pane_offset_x = 0
        self._right_pane_offset_y = 0

        self.setStyleSheet(
            "QFrame#DesignerSection {"
            "border: 1px solid rgba(130, 130, 145, 80);"
            "border-radius: 8px;"
            "background: rgba(255, 255, 255, 18);"
            "}"
            "QLabel#SectionTitle { font-weight: 700; }"
            "QLabel#SectionHint { color: #8d8d98; }"
        )

        root = QHBoxLayout(self)
        root.setContentsMargins(12, 10, 12, 10)
        root.setSpacing(12)

        controls_scroll = QScrollArea()
        controls_scroll.setWidgetResizable(True)
        controls_scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        controls_scroll.setMinimumWidth(340)
        controls_host = QWidget()
        controls_scroll.setWidget(controls_host)
        controls = QVBoxLayout(controls_host)
        controls.setContentsMargins(0, 0, 0, 0)
        controls.setSpacing(10)

        header = QFrame()
        header.setObjectName("DesignerSection")
        header_layout = QGridLayout(header)
        header_layout.setContentsMargins(10, 8, 10, 8)
        header_layout.setHorizontalSpacing(8)
        header_layout.setVerticalSpacing(6)
        title = QLabel("iPone Designer")
        title.setObjectName("SectionTitle")
        hint = QLabel("Pick a saved design or start a fresh draft.")
        hint.setObjectName("SectionHint")
        self._profile_combo = QComboBox()
        self._profile_combo.currentIndexChanged.connect(self._emit_profile_selected)
        self._variant_combo = QComboBox()
        self._variant_combo.currentIndexChanged.connect(self._emit_variant_selected)
        self._new_btn = QPushButton("New")
        self._new_btn.clicked.connect(self._load_unsaved_draft)
        header_layout.addWidget(title, 0, 0, 1, 3)
        header_layout.addWidget(hint, 1, 0, 1, 3)
        header_layout.addWidget(QLabel("Device"), 2, 0)
        header_layout.addWidget(self._profile_combo, 2, 1, 1, 2)
        header_layout.addWidget(QLabel("Design"), 3, 0)
        header_layout.addWidget(self._variant_combo, 3, 1)
        header_layout.addWidget(self._new_btn, 3, 2)
        controls.addWidget(header)

        details = QFrame()
        details.setObjectName("DesignerSection")
        details_form = QFormLayout(details)
        details_form.setContentsMargins(10, 8, 10, 8)
        self._name_edit = QLineEdit()
        self._name_edit.setPlaceholderText("My iPone")
        self._name_edit.textChanged.connect(self._sync_preview)
        self._base_label = QLabel("")
        self._resolution_label = QLabel("")
        self._target_label = QLabel("")
        details_form.addRow("Name", self._name_edit)
        details_form.addRow("Target", self._target_label)
        controls.addWidget(details)

        wallpapers = QFrame()
        wallpapers.setObjectName("DesignerSection")
        wallpapers_layout = QVBoxLayout(wallpapers)
        wallpapers_layout.setContentsMargins(10, 8, 10, 8)
        wallpapers_layout.setSpacing(8)
        wallpapers_title = QLabel("Artwork")
        wallpapers_title.setObjectName("SectionTitle")
        wallpapers_hint = QLabel("Default artwork is used unless a photo is selected.")
        wallpapers_hint.setObjectName("SectionHint")
        wallpapers_layout.addWidget(wallpapers_title)
        wallpapers_layout.addWidget(wallpapers_hint)
        wallpapers_form = QFormLayout()
        self._wallpaper_edit = QLineEdit()
        self._wallpaper_edit.setPlaceholderText("Default")
        self._wallpaper_edit.textChanged.connect(self._sync_preview)
        self._charging_wallpaper_edit = QLineEdit()
        self._charging_wallpaper_edit.setPlaceholderText("Default")
        self._charging_wallpaper_edit.textChanged.connect(self._sync_preview)
        self._right_pane_wallpaper_edit = QLineEdit()
        self._right_pane_wallpaper_edit.setPlaceholderText("Default")
        self._right_pane_wallpaper_edit.textChanged.connect(self._sync_preview)
        self._fit_combo = QComboBox()
        self._fit_combo.addItems(["fill", "fit", "stretch"])
        self._charge_fit_combo = QComboBox()
        self._charge_fit_combo.addItems(["fill", "fit", "stretch"])
        self._right_pane_fit_combo = QComboBox()
        self._right_pane_fit_combo.addItems(["fill", "fit", "stretch"])
        self._fit_combo.currentIndexChanged.connect(self._sync_preview)
        self._charge_fit_combo.currentIndexChanged.connect(self._sync_preview)
        self._right_pane_fit_combo.currentIndexChanged.connect(self._sync_preview)
        wallpapers_form.addRow("Main", self._path_row(self._wallpaper_edit, self._choose_wallpaper))
        wallpapers_form.addRow("Charging", self._path_row(self._charging_wallpaper_edit, self._choose_charging_wallpaper))
        wallpapers_form.addRow(
            "Right side",
            self._path_row(
                self._right_pane_wallpaper_edit,
                self._choose_right_pane_wallpaper,
                [("Position", self._choose_right_pane_position)],
            ),
        )
        wallpapers_layout.addLayout(wallpapers_form)
        self._artwork_advanced_toggle = QPushButton("Show fit options")
        self._artwork_advanced_toggle.setCheckable(True)
        self._artwork_advanced_toggle.toggled.connect(self._set_artwork_advanced_visible)
        self._artwork_advanced_options = QWidget()
        artwork_advanced_form = QFormLayout(self._artwork_advanced_options)
        artwork_advanced_form.setContentsMargins(0, 0, 0, 0)
        artwork_advanced_form.addRow("Main fit", self._fit_combo)
        artwork_advanced_form.addRow("Charging fit", self._charge_fit_combo)
        artwork_advanced_form.addRow("Right fit", self._right_pane_fit_combo)
        self._artwork_advanced_options.hide()
        wallpapers_layout.addWidget(self._artwork_advanced_toggle)
        wallpapers_layout.addWidget(self._artwork_advanced_options)
        controls.addWidget(wallpapers)

        theme_form_frame = QFrame()
        theme_form_frame.setObjectName("DesignerSection")
        theme_layout = QVBoxLayout(theme_form_frame)
        theme_layout.setContentsMargins(10, 8, 10, 8)
        theme_layout.setSpacing(8)
        theme_title = QLabel("Look")
        theme_title.setObjectName("SectionTitle")
        theme_hint = QLabel("Choose dark or light, then pick an accent.")
        theme_hint.setObjectName("SectionHint")
        theme_layout.addWidget(theme_title)
        theme_layout.addWidget(theme_hint)
        theme_form = QFormLayout()
        self._font_combo = QComboBox()
        self._font_combo.currentIndexChanged.connect(self._sync_preview)
        self._appearance_mode_combo = QComboBox()
        self._appearance_mode_combo.addItem("Dark", "dark")
        self._appearance_mode_combo.addItem("Light", "light")
        self._appearance_mode_combo.currentIndexChanged.connect(self._apply_color_profile)
        self._color_profile_combo = QComboBox()
        for key in (
            "default",
            "blue",
            "green",
            "teal",
            "indigo",
            "rose",
            "amber",
            "orange",
            "graphite",
            "cyberpunk",
            "custom",
        ):
            profile = COLOR_PROFILES[key]
            self._color_profile_combo.addItem(profile["label"], key)
        self._color_profile_combo.currentIndexChanged.connect(self._apply_color_profile)
        self._background_btn = _ColorButton("Background")
        self._foreground_btn = _ColorButton("Foreground")
        self._selector_start_btn = _ColorButton("Highlight Start")
        self._selector_end_btn = _ColorButton("Highlight End")
        self._selector_text_btn = _ColorButton("Highlight Text")
        self._separator_btn = _ColorButton("Separator")
        for button in (
            self._background_btn,
            self._foreground_btn,
            self._selector_start_btn,
            self._selector_end_btn,
            self._selector_text_btn,
            self._separator_btn,
        ):
            button.color_changed.connect(self._sync_preview)
        theme_form.addRow("Mode", self._appearance_mode_combo)
        theme_form.addRow("Accent", self._color_profile_combo)
        theme_layout.addLayout(theme_form)
        self._advanced_color_toggle = QPushButton("Show advanced colors and font")
        self._advanced_color_toggle.setCheckable(True)
        self._advanced_color_toggle.toggled.connect(self._set_color_advanced_visible)
        self._advanced_color_options = QWidget()
        advanced_color_form = QFormLayout(self._advanced_color_options)
        advanced_color_form.setContentsMargins(0, 0, 0, 0)
        advanced_color_form.addRow("Font", self._font_combo)
        advanced_color_form.addRow("Background", self._background_btn)
        advanced_color_form.addRow("Text", self._foreground_btn)
        advanced_color_form.addRow("Highlight start", self._selector_start_btn)
        advanced_color_form.addRow("Highlight end", self._selector_end_btn)
        advanced_color_form.addRow("Highlight text", self._selector_text_btn)
        advanced_color_form.addRow("Separator", self._separator_btn)
        self._advanced_color_options.hide()
        theme_layout.addWidget(self._advanced_color_toggle)
        theme_layout.addWidget(self._advanced_color_options)
        controls.addWidget(theme_form_frame)

        action_row = QHBoxLayout()
        self._save_btn = QPushButton("Save")
        self._save_btn.clicked.connect(self._emit_save)
        self._rename_btn = QPushButton("Rename")
        self._rename_btn.clicked.connect(self._emit_rename)
        self._duplicate_btn = QPushButton("Duplicate")
        self._duplicate_btn.clicked.connect(self._emit_duplicate)
        self._delete_btn = QPushButton("Delete")
        self._delete_btn.clicked.connect(self._emit_delete)
        action_row.addWidget(self._save_btn)
        action_row.addWidget(self._rename_btn)
        action_row.addWidget(self._duplicate_btn)
        action_row.addWidget(self._delete_btn)
        controls.addLayout(action_row)

        self._status = QLabel("Change the look or artwork, then update the preview.")
        self._status.setWordWrap(True)
        controls.addWidget(self._status)
        controls.addStretch(1)
        root.addWidget(controls_scroll, 0)

        preview_col = QVBoxLayout()
        preview_col.setSpacing(8)
        preview_header = QHBoxLayout()
        self._preview_mode = QComboBox()
        self._preview_mode.addItem("Simulator", "simulator")
        self._preview_mode.currentIndexChanged.connect(self._sync_preview)
        self._preview_mode.hide()
        preview_header.addWidget(QLabel("Preview"))
        self._preview_screen = QComboBox()
        self._preview_screen.addItem("Menu", "sbs")
        self._preview_screen.addItem("Now Playing", "wps")
        self._preview_screen.addItem("Lockscreen", "lockscreen")
        self._preview_screen.currentIndexChanged.connect(self._sync_preview)
        preview_header.addWidget(self._preview_screen)
        self._preview_screen.blockSignals(True)
        self._preview_screen.setCurrentIndex(0)
        self._preview_screen.blockSignals(False)
        preview_header.addStretch(1)
        preview_col.addLayout(preview_header)

        preview_host = QWidget()
        self._preview_stack = QStackedLayout(preview_host)
        self._preview_stack.setContentsMargins(0, 0, 0, 0)
        self._preview = _ScreenPreview()
        self._simulator_surface = _EmbeddedSimulatorPreview()
        self._preview.installEventFilter(self)
        self._simulator_surface.installEventFilter(self)
        self._preview_stack.addWidget(self._preview)
        self._preview_stack.addWidget(self._simulator_surface)
        preview_col.addWidget(preview_host, 1)
        controls_row = QGridLayout()
        controls_row.setHorizontalSpacing(6)
        controls_row.setVerticalSpacing(6)
        self._sim_menu_btn = QPushButton("Menu")
        self._sim_up_btn = QPushButton("Up")
        self._sim_down_btn = QPushButton("Down")
        self._sim_left_btn = QPushButton("Left")
        self._sim_select_btn = QPushButton("Select")
        self._sim_right_btn = QPushButton("Right")
        self._sim_play_btn = QPushButton("Play")
        for button, action in (
            (self._sim_menu_btn, "menu"),
            (self._sim_up_btn, "up"),
            (self._sim_down_btn, "down"),
            (self._sim_left_btn, "left"),
            (self._sim_select_btn, "select"),
            (self._sim_right_btn, "right"),
            (self._sim_play_btn, "play"),
        ):
            button.clicked.connect(lambda _checked=False, name=action: self._send_simulator_control(name))
        controls_row.addWidget(self._sim_menu_btn, 0, 0)
        controls_row.addWidget(self._sim_up_btn, 0, 1)
        controls_row.addWidget(self._sim_play_btn, 0, 2)
        controls_row.addWidget(self._sim_left_btn, 1, 0)
        controls_row.addWidget(self._sim_select_btn, 1, 1)
        controls_row.addWidget(self._sim_right_btn, 1, 2)
        controls_row.addWidget(self._sim_down_btn, 2, 1)
        for button in (
            self._sim_menu_btn,
            self._sim_up_btn,
            self._sim_down_btn,
            self._sim_left_btn,
            self._sim_select_btn,
            self._sim_right_btn,
            self._sim_play_btn,
        ):
            button.setEnabled(False)
            button.setToolTip("Refresh Preview now generates a fresh simulator snapshot instead of keeping a live simulator running.")
        self._simulator_controls_widget = QWidget()
        self._simulator_controls_widget.setLayout(controls_row)
        self._simulator_controls_widget.hide()
        preview_col.addWidget(self._simulator_controls_widget)

        preview_action_row = QHBoxLayout()
        self._refresh_preview_btn = QPushButton("Update Preview")
        self._refresh_preview_btn.clicked.connect(self._emit_refresh_preview)
        preview_action_row.addWidget(self._refresh_preview_btn)
        preview_action_row.addStretch(1)
        preview_col.addLayout(preview_action_row)

        deploy_row = QHBoxLayout()
        self._deploy_device_btn = QPushButton("Install on iPod")
        self._deploy_device_btn.clicked.connect(self._emit_deploy_device)
        self._deploy_sim_btn = QPushButton("Install to Simulator")
        self._deploy_sim_btn.clicked.connect(self._emit_deploy_simulator)
        deploy_row.addWidget(self._deploy_device_btn)
        deploy_row.addWidget(self._deploy_sim_btn)
        preview_col.addLayout(deploy_row)
        root.addLayout(preview_col, 1)

    def set_profiles(self, profiles, selected_id):
        self._profiles = {item["id"]: item for item in profiles}
        self._profile_combo.blockSignals(True)
        self._profile_combo.clear()
        selected_index = 0
        for index, profile in enumerate(profiles):
            self._profile_combo.addItem(profile["name"], profile["id"])
            if profile["id"] == selected_id:
                selected_index = index
        self._profile_combo.setCurrentIndex(selected_index)
        self._profile_combo.blockSignals(False)

    def set_fonts(self, fonts):
        current = self._font_combo.currentData()
        self._font_combo.blockSignals(True)
        self._font_combo.clear()
        for font in fonts:
            self._font_combo.addItem(font["label"], font["path_rel"])
        if current:
            index = self._font_combo.findData(current)
            if index >= 0:
                self._font_combo.setCurrentIndex(index)
        self._font_combo.blockSignals(False)

    def set_variants(self, variants, selected_variant_id=""):
        self._variants = {item["id"]: item for item in variants if item.get("id")}
        self._variant_combo.blockSignals(True)
        self._variant_combo.clear()
        self._variant_combo.addItem("Unsaved Draft", "")
        selected_index = 0
        for index, variant in enumerate(variants, start=1):
            self._variant_combo.addItem(variant["name"], variant["id"])
            if variant["id"] == selected_variant_id:
                selected_index = index
        self._variant_combo.setCurrentIndex(selected_index)
        self._variant_combo.blockSignals(False)

    def load_variant(self, variant, preview_state):
        variant = dict(variant or {})
        self._current_variant_id = variant.get("id", "")
        combo_index = self._variant_combo.findData(self._current_variant_id)
        if combo_index >= 0:
            self._variant_combo.blockSignals(True)
            self._variant_combo.setCurrentIndex(combo_index)
            self._variant_combo.blockSignals(False)
        elif not self._current_variant_id:
            self._variant_combo.blockSignals(True)
            self._variant_combo.setCurrentIndex(0)
            self._variant_combo.blockSignals(False)

        self._name_edit.setText(variant.get("name", ""))
        self._base_label.setText(variant.get("base_theme_id", ""))
        self._resolution_label.setText(variant.get("screen_resolution", ""))
        self._target_label.setText(
            f"{variant.get('base_theme_id', '')} / {variant.get('screen_resolution', '')}".strip(" /")
        )
        self._wallpaper_edit.setText(variant.get("wallpaper_source", ""))
        self._charging_wallpaper_edit.setText(variant.get("charging_wallpaper_source", ""))
        self._right_pane_wallpaper_edit.setText(variant.get("right_pane_wallpaper_source", ""))
        self._fit_combo.setCurrentText(variant.get("fit_mode", "fill"))
        self._charge_fit_combo.setCurrentText(variant.get("charging_fit_mode", "fill"))
        self._right_pane_fit_combo.setCurrentText(variant.get("right_pane_fit_mode", "fill"))
        self._right_pane_offset_x = self._clamp_offset(variant.get("right_pane_offset_x", 0))
        self._right_pane_offset_y = self._clamp_offset(variant.get("right_pane_offset_y", 0))
        self._set_font_value(variant.get("font_rel", ""))
        colors = variant.get("colors", {})
        self._set_appearance_mode_value(variant.get("appearance_mode", "dark"))
        self._set_color_profile_value(variant.get("color_profile", "custom"))
        self._background_btn.set_hex(colors.get("background", "100F16"))
        self._foreground_btn.set_hex(colors.get("foreground", "F7F4FA"))
        self._selector_start_btn.set_hex(colors.get("selector_start", "2B2234"))
        self._selector_end_btn.set_hex(colors.get("selector_end", "9D7AE6"))
        self._selector_text_btn.set_hex(colors.get("selector_text", "FCF9FF"))
        self._separator_btn.set_hex(colors.get("list_separator", "1A1621"))
        self._base_wallpaper_path = preview_state.get("wallpaper_path", "")
        self._base_charging_wallpaper_path = preview_state.get("charging_wallpaper_path", "")
        self._base_right_pane_wallpaper_path = preview_state.get("right_pane_wallpaper_path", "")
        self._base_menu_backdrop_path = preview_state.get("menu_backdrop_path", "")
        self._base_simulator_preview_path = preview_state.get("simulator_preview_path", "")
        self._status.setText(
            f"Base {variant.get('base_theme_id', '')} at {variant.get('screen_resolution', '')}. "
            "Save to persist the variant, or deploy directly after saving."
        )
        self._rename_btn.setEnabled(bool(self._current_variant_id))
        self._duplicate_btn.setEnabled(bool(self._current_variant_id))
        self._delete_btn.setEnabled(bool(self._current_variant_id))
        self._sync_preview()

    def current_profile_id(self):
        return self._profile_combo.currentData() or ""

    def current_variant_data(self):
        return {
            "id": self._current_variant_id,
            "name": self._name_edit.text().strip(),
            "base_theme_id": self._base_label.text().strip(),
            "screen_resolution": self._resolution_label.text().strip(),
            "wallpaper_source": self._wallpaper_edit.text().strip(),
            "charging_wallpaper_source": self._charging_wallpaper_edit.text().strip(),
            "right_pane_wallpaper_source": self._right_pane_wallpaper_edit.text().strip(),
            "fit_mode": self._fit_combo.currentText(),
            "charging_fit_mode": self._charge_fit_combo.currentText(),
            "right_pane_fit_mode": self._right_pane_fit_combo.currentText(),
            "right_pane_offset_x": self._right_pane_offset_x,
            "right_pane_offset_y": self._right_pane_offset_y,
            "font_rel": self._font_combo.currentData() or "",
            "color_profile": self._color_profile_combo.currentData() or "custom",
            "appearance_mode": self._appearance_mode_combo.currentData() or "dark",
            "colors": {
                "background": self._background_btn.hex(),
                "foreground": self._foreground_btn.hex(),
                "selector_start": self._selector_start_btn.hex(),
                "selector_end": self._selector_end_btn.hex(),
                "selector_text": self._selector_text_btn.hex(),
                "list_separator": self._separator_btn.hex(),
            },
            "preview_screen": self._preview_screen.currentData() or "sbs",
        }

    def set_status(self, text):
        self._status.setText(text)

    def set_simulator_preview(self, path):
        self._base_simulator_preview_path = str(path or "").strip()
        self._sync_preview()

    def set_simulator_target(self, binary_path, simdisk_path):
        self._simulator_surface.set_target(binary_path, simdisk_path)

    def set_preview_screen(self, screen_id):
        index = self._preview_screen.findData(screen_id)
        if index < 0:
            return
        self._preview_screen.blockSignals(True)
        self._preview_screen.setCurrentIndex(index)
        self._preview_screen.blockSignals(False)
        self._sync_preview()

    def _path_row(self, edit, browse_slot, extra_buttons=None):
        row = QWidget()
        layout = QHBoxLayout(row)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(6)
        browse = QPushButton("Choose")
        browse.clicked.connect(browse_slot)
        clear = QPushButton("Reset")
        clear.clicked.connect(edit.clear)
        layout.addWidget(edit, 1)
        layout.addWidget(browse)
        for label, slot in (extra_buttons or []):
            button = QPushButton(label)
            button.clicked.connect(slot)
            layout.addWidget(button)
        layout.addWidget(clear)
        return row

    def _set_artwork_advanced_visible(self, visible):
        self._artwork_advanced_options.setVisible(bool(visible))
        self._artwork_advanced_toggle.setText("Hide fit options" if visible else "Show fit options")

    def _set_color_advanced_visible(self, visible):
        self._advanced_color_options.setVisible(bool(visible))
        self._advanced_color_toggle.setText(
            "Hide advanced colors and font" if visible else "Show advanced colors and font"
        )

    def _emit_profile_selected(self):
        profile_id = self.current_profile_id()
        if profile_id:
            self.profile_selected.emit(profile_id)

    def _emit_variant_selected(self):
        variant_id = self._variant_combo.currentData() or ""
        if variant_id:
            self.variant_selected.emit(variant_id)
        else:
            self._load_unsaved_draft()

    def _load_unsaved_draft(self):
        self._current_variant_id = ""
        self.variant_selected.emit("")

    def _emit_save(self):
        self.save_requested.emit(self.current_variant_data())

    def _emit_rename(self):
        if self._current_variant_id:
            self.rename_requested.emit(self._current_variant_id, self._name_edit.text().strip())

    def _emit_duplicate(self):
        if self._current_variant_id:
            self.duplicate_requested.emit(self._current_variant_id)

    def _emit_delete(self):
        if self._current_variant_id:
            self.delete_requested.emit(self._current_variant_id)

    def _emit_deploy_device(self):
        self.deploy_device_requested.emit(self.current_variant_data())

    def _emit_deploy_simulator(self):
        self.deploy_simulator_requested.emit(self.current_variant_data())

    def _emit_refresh_preview(self):
        self.simulator_refresh_requested.emit(self.current_variant_data())

    def _choose_wallpaper(self):
        path, _ = QFileDialog.getOpenFileName(self, "Select Wallpaper", "", "Images (*.png *.jpg *.jpeg *.bmp)")
        if path:
            self._wallpaper_edit.setText(path)

    def _choose_charging_wallpaper(self):
        path, _ = QFileDialog.getOpenFileName(self, "Select Charging Wallpaper", "", "Images (*.png *.jpg *.jpeg *.bmp)")
        if path:
            self._charging_wallpaper_edit.setText(path)

    def _choose_right_pane_wallpaper(self):
        path, _ = QFileDialog.getOpenFileName(self, "Select Right Pane Wallpaper", "", "Images (*.png *.jpg *.jpeg *.bmp)")
        if path:
            self._right_pane_wallpaper_edit.setText(path)

    def _choose_right_pane_position(self):
        image_path = self._right_pane_wallpaper_edit.text().strip() or self._base_right_pane_wallpaper_path
        if not image_path or not os.path.isfile(image_path):
            self._status.setText("Choose a right-side image before positioning it.")
            return
        dialog = _RightPanePositionDialog(
            self,
            image_path,
            self._right_pane_fit_combo.currentText(),
            self._right_pane_offset_x,
            self._right_pane_offset_y,
            self._background_btn.hex(),
        )
        if dialog.exec() == QDialog.Accepted:
            self._right_pane_offset_x, self._right_pane_offset_y = dialog.offsets()
            self._sync_preview()

    @staticmethod
    def _clamp_offset(value):
        try:
            number = int(value)
        except (TypeError, ValueError):
            number = 0
        return max(-100, min(100, number))

    def _set_font_value(self, value):
        index = self._font_combo.findData(value)
        if index >= 0:
            self._font_combo.setCurrentIndex(index)

    def _set_color_profile_value(self, value):
        index = self._color_profile_combo.findData(value)
        if index < 0:
            index = self._color_profile_combo.findData("custom")
        if index >= 0:
            self._color_profile_combo.blockSignals(True)
            self._color_profile_combo.setCurrentIndex(index)
            self._color_profile_combo.blockSignals(False)

    def _set_appearance_mode_value(self, value):
        mode = "light" if str(value or "").strip().lower() == "light" else "dark"
        index = self._appearance_mode_combo.findData(mode)
        if index >= 0:
            self._appearance_mode_combo.blockSignals(True)
            self._appearance_mode_combo.setCurrentIndex(index)
            self._appearance_mode_combo.blockSignals(False)

    def _apply_color_profile(self):
        profile_id = self._color_profile_combo.currentData() or "custom"
        colors = _profile_colors(profile_id, self._appearance_mode_combo.currentData() or "dark")
        if not colors:
            self._sync_preview()
            return
        self._background_btn.set_hex(colors["background"])
        self._foreground_btn.set_hex(colors["foreground"])
        self._selector_start_btn.set_hex(colors["selector_start"])
        self._selector_end_btn.set_hex(colors["selector_end"])
        self._selector_text_btn.set_hex(colors["selector_text"])
        self._separator_btn.set_hex(colors["list_separator"])
        self._sync_preview()

    def _sync_preview(self, *_args):
        simulator_mode = self._preview_mode.currentData() == "simulator"
        self._preview_stack.setCurrentWidget(self._preview)
        simulator_preview_path = self._resolved_simulator_preview_path()
        variant = self.current_variant_data()
        state = {
            "theme_name": self._name_edit.text().strip(),
            "width": int((self._resolution_label.text().split("x", 1)[0] or "320")) if "x" in self._resolution_label.text() else 320,
            "height": int((self._resolution_label.text().split("x", 1)[1] or "240")) if "x" in self._resolution_label.text() else 240,
            "font_label": os.path.basename(self._font_combo.currentData() or ""),
            "wallpaper_path": self._wallpaper_edit.text().strip() or self._base_wallpaper_path,
            "charging_wallpaper_path": self._charging_wallpaper_edit.text().strip() or self._base_charging_wallpaper_path,
            "right_pane_wallpaper_path": self._right_pane_wallpaper_edit.text().strip() or self._base_right_pane_wallpaper_path,
            "menu_backdrop_path": self._base_menu_backdrop_path,
            "simulator_preview_path": simulator_preview_path,
            "simulator_status_text": self._simulator_surface.status_text(),
            "colors": variant["colors"],
        }
        self._preview.set_preview(state, self._preview_mode.currentData())
        self.preview_changed.emit(variant, self._preview_mode.currentData() or "main_menu")

    def restart_simulator_preview(self, post_actions=None):
        if self._preview_mode.currentData() == "simulator":
            self._sync_preview()

    def shutdown_simulator_preview(self):
        self._simulator_surface.shutdown()
        if self._preview_mode.currentData() == "simulator":
            self._sync_preview()

    def _resolved_simulator_preview_path(self):
        path = str(getattr(self, "_base_simulator_preview_path", "") or "").strip()
        if path and os.path.isfile(path):
            return path
        return ""

    def _send_simulator_control(self, action):
        self._simulator_surface.send_control(action)

    def eventFilter(self, obj, event):
        if (
            obj in (self._preview, self._simulator_surface)
            and event.type() == QEvent.KeyPress
            and self._preview_mode.currentData() == "simulator"
            and self._simulator_surface.forward_qt_key(event.key())
        ):
            return True
        return super().eventFilter(obj, event)
