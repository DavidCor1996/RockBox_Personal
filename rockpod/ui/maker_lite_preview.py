"""Live 60 Hz Maker Lite preview backed by the exact portable C runtime."""

from __future__ import annotations

import struct
import zlib

from PySide6.QtCore import QElapsedTimer, QPointF, Qt, QTimer
from PySide6.QtGui import QColor, QFont, QImage, QPainter, QPen
from PySide6.QtWidgets import (
    QCheckBox,
    QDialog,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


INPUT_LEFT = 1 << 0
INPUT_RIGHT = 1 << 1
INPUT_UP = 1 << 2
INPUT_DOWN = 1 << 3
INPUT_PRIMARY = 1 << 4
INPUT_SECONDARY = 1 << 5
INPUT_PREVIOUS = 1 << 6
INPUT_NEXT = 1 << 7
INPUT_PAUSE = 1 << 8


def _load_art(path):
    with open(path, "rb") as source:
        data = source.read()
    if len(data) < 64 or data[:4] != b"MLAR":
        raise ValueError("private art kit has an invalid header")
    version, cell_size, count, player_base, expected_crc = struct.unpack_from(
        "<HHHHI", data, 4
    )
    content = data[64:]
    pixel_bytes = count * 512
    table_entries = 14 if version == 2 else 56 if version in {3, 4} else 0
    minimum_size = pixel_bytes + table_entries * 4
    if (
        version not in {1, 2, 3, 4}
        or cell_size != 16
        or not 1 <= count <= 1536
        or len(content) < minimum_size
        or zlib.crc32(content) & 0xFFFFFFFF != expected_crc
    ):
        raise ValueError("private art kit failed bounds/checksum validation")
    frames = []
    if version == 4:
        frame_header = pixel_bytes + 56 * 4
        if len(content) < frame_header + 4:
            raise ValueError("private metasprite table is truncated")
        frame_count, reserved = struct.unpack_from("<HH", content, frame_header)
        expected_size = frame_header + 4 + frame_count * 36
        if (
            reserved != 0
            or not 14 <= frame_count <= 512
            or len(content) != expected_size
        ):
            raise ValueError("private metasprite table has invalid bounds")
        for frame_index in range(frame_count):
            offset = frame_header + 4 + frame_index * 36
            columns, rows, offset_x, offset_y, *frame_cells = (
                struct.unpack_from("<BBbb16H", content, offset)
            )
            if (
                not 1 <= columns <= 4
                or not 1 <= rows <= 4
                or not -64 <= offset_x <= 64
                or not -64 <= offset_y <= 64
            ):
                raise ValueError("private metasprite frame is invalid")
            used = frame_cells[: columns * rows]
            if any(cell != 0xFFFF and cell >= count for cell in used):
                raise ValueError("private metasprite cell is invalid")
            if any(cell != 0xFFFF for cell in frame_cells[columns * rows :]):
                raise ValueError("private metasprite padding is invalid")
            frames.append((columns, rows, offset_x, offset_y, used))
    elif len(content) != minimum_size:
        raise ValueError("private art kit has trailing content")
    payload = content[:pixel_bytes]
    cells = []
    for cell in range(count):
        image = QImage(16, 16, QImage.Format_RGBA8888)
        start = cell * 512
        for y in range(16):
            for x in range(16):
                value = struct.unpack_from(
                    "<H", payload, start + (y * 16 + x) * 2
                )[0]
                if value == 0xF81F:
                    image.setPixelColor(x, y, QColor(255, 0, 255, 0))
                else:
                    image.setPixelColor(
                        x,
                        y,
                        QColor(
                            ((value >> 11) & 31) * 255 // 31,
                            ((value >> 5) & 63) * 255 // 63,
                            (value & 31) * 255 // 31,
                        ),
                    )
        cells.append(image)
    if version in {3, 4}:
        flat = [
            struct.unpack_from("<HBB", content, pixel_bytes + index * 4)
            for index in range(56)
        ]
        animations = [
            [flat[action * 4 + direction] for direction in range(4)]
            for action in range(14)
        ]
    elif version == 2:
        flat = [
            struct.unpack_from("<HBB", content, pixel_bytes + index * 4)
            for index in range(14)
        ]
        animations = [[entry] * 4 for entry in flat]
    else:
        animations = [
            [(player_base + index, 1, 1)] * 4 for index in range(14)
        ]
    for directions in animations:
        for start, frame_count, raw_ticks in directions:
            ticks = raw_ticks & 0x7F
            if (
                frame_count == 0
                or ticks == 0
                or ticks > 60
                or start >= (len(frames) if frames else count)
                or frame_count > (len(frames) if frames else count) - start
            ):
                raise ValueError("private animation table is invalid")
    return cells, player_base, list(data[48:64]), animations, frames


class _PreviewSurface(QWidget):
    def __init__(self, source, session, art_path, button_bits, parent=None):
        super().__init__(parent)
        self.source = source
        self.session = session
        (
            self.cells,
            self.player_base,
            self.entity_cells,
            self.animations,
            self.player_frames,
        ) = _load_art(art_path)
        self.mirrored_cells = [
            image.mirrored(True, False) for image in self.cells
        ]
        self.input_mask = 0
        self.button_bits = dict(button_bits)
        self.show_overlay = True
        self.snapshot = self.session.snapshot()
        self.entity_states = self.session.entities()
        self.dynamic_states = self.session.dynamics()
        self.tiles = self._flatten_tiles()
        self.setFixedSize(640, 480)
        self.setFocusPolicy(Qt.StrongFocus)

    def _flatten_tiles(self):
        width, height = self.source["size"]
        tiles = [0] * (width * height)
        for operation in self.source.get("terrain", []):
            if "point" in operation:
                x, y = operation["point"]
                rect = [x, y, 1, 1]
            else:
                rect = operation.get("rect", [0, 0, 0, 0])
            x, y, rect_width, rect_height = map(int, rect)
            for tile_y in range(y, min(height, y + rect_height)):
                for tile_x in range(x, min(width, x + rect_width)):
                    tiles[tile_y * width + tile_x] = int(
                        operation.get("tile", 0)
                    )
        return tiles

    def advance(self):
        self.session.tick(self.input_mask)
        self.snapshot = self.session.snapshot()
        self.entity_states = self.session.entities()
        self.dynamic_states = self.session.dynamics()
        self.update()

    def _native_frame(self):
        frame = QImage(320, 240, QImage.Format_RGB32)
        frame.fill(Qt.black)
        painter = QPainter(frame)
        painter.setRenderHint(QPainter.SmoothPixmapTransform, False)
        view_width, view_height = map(int, self.source["view"])
        origin_x = (320 - view_width) // 2
        origin_y = (240 - view_height) // 2
        camera_x = self.snapshot["camera_x"] >> 16
        camera_y = self.snapshot["camera_y"] >> 16
        map_width, map_height = map(int, self.source["size"])
        tile_size = int(self.source.get("tile_size", 16))
        first_x = max(0, camera_x // tile_size)
        first_y = max(0, camera_y // tile_size)
        last_x = min(map_width, first_x + view_width // tile_size + 2)
        last_y = min(map_height, first_y + view_height // tile_size + 2)
        for tile_y in range(first_y, last_y):
            for tile_x in range(first_x, last_x):
                cell = self.tiles[tile_y * map_width + tile_x]
                if 0 <= cell < len(self.cells):
                    painter.drawImage(
                        origin_x + tile_x * tile_size - camera_x,
                        origin_y + tile_y * tile_size - camera_y,
                        self.cells[cell],
                    )
        for entity_index, entity in enumerate(self.entity_states):
            kind = entity["kind"]
            if not entity["alive"] or kind == 1:
                continue
            cell = int(entity.get("render_cell", 0))
            if not cell:
                cell = (
                    self.entity_cells[kind]
                    if kind < len(self.entity_cells)
                    else 0
                )
            authored = (
                self.source.get("entities", [])[entity_index]
                if entity_index < len(self.source.get("entities", []))
                else {}
            )
            if (
                self.source.get("gameplay") == "brawl"
                and kind == 4
                and int(authored.get("flags", 0)) & 0x2000
                and self.player_frames
                and 0 <= cell < len(self.player_frames)
            ):
                columns, rows, offset_x, offset_y, frame_cells = (
                    self.player_frames[cell]
                )
                anchor_x = origin_x + entity["x"] - camera_x
                anchor_y = origin_y + entity["y"] - camera_y
                for row in range(rows):
                    for column in range(columns):
                        frame_cell = frame_cells[row * columns + column]
                        if frame_cell == 0xFFFF:
                            continue
                        painter.drawImage(
                            anchor_x + offset_x + column * 16,
                            anchor_y + offset_y + row * 16,
                            self.cells[frame_cell],
                        )
            elif cell and cell < len(self.cells):
                painter.drawImage(
                    origin_x + entity["x"] - camera_x - tile_size // 2,
                    origin_y + entity["y"] - camera_y - tile_size,
                    self.cells[cell],
                )
        for entity in self.dynamic_states:
            kind = entity["kind"]
            cell = self.entity_cells[kind] if kind < len(self.entity_cells) else 0
            if cell and cell < len(self.cells):
                painter.drawImage(
                    origin_x + entity["x"] - camera_x - tile_size // 2,
                    origin_y + entity["y"] - camera_y - tile_size // 2,
                    self.cells[cell],
                )
        action = min(13, int(self.snapshot["action"]))
        if self.source["ruleset"] == "zelda":
            if self.snapshot.get("facing_y", 0) < 0:
                direction = 3
            elif self.snapshot.get("facing_y", 0) > 0:
                direction = 1
            elif self.snapshot.get("facing_x", 1) < 0:
                direction = 2
            else:
                direction = 0
        else:
            direction = 2 if self.snapshot.get("facing_x", 1) < 0 else 0
        start, count, raw_ticks = self.animations[action][direction]
        ticks = raw_ticks & 0x7F
        player_frame = start + (int(self.snapshot["tick"]) // ticks) % count
        player_x = (
            origin_x + (self.snapshot["player_x"] >> 16) - camera_x
        )
        player_y = (
            origin_y + (self.snapshot["player_y"] >> 16) - camera_y
        )
        if self.player_frames and player_frame < len(self.player_frames):
            columns, rows, offset_x, offset_y, frame_cells = (
                self.player_frames[player_frame]
            )
            mirrored = bool(raw_ticks & 0x80)
            for row in range(rows):
                for column in range(columns):
                    cell = frame_cells[row * columns + column]
                    if cell == 0xFFFF:
                        continue
                    if mirrored:
                        draw_x = player_x - offset_x - (column + 1) * 16
                        image = self.mirrored_cells[cell]
                    else:
                        draw_x = player_x + offset_x + column * 16
                        image = self.cells[cell]
                    painter.drawImage(
                        draw_x,
                        player_y + offset_y + row * 16,
                        image,
                    )
        elif player_frame < len(self.cells):
            image = (
                self.mirrored_cells[player_frame]
                if raw_ticks & 0x80
                else self.cells[player_frame]
            )
            painter.drawImage(
                player_x - tile_size // 2,
                player_y - tile_size,
                image,
            )
        painter.setPen(Qt.white)
        painter.setBrush(Qt.black)
        painter.drawRect(0, 0, 320, 15)
        painter.setFont(QFont("Monospace", 7))
        ruleset = self.source["ruleset"]
        if self.source.get("gameplay") == "brawl":
            hud = (
                f"P1 {self.snapshot['brawl_player_damage']:03d}% "
                f"x{self.snapshot['brawl_player_stocks']}   "
                f"CPU {self.snapshot['brawl_opponent_damage']:03d}% "
                f"x{self.snapshot['brawl_opponent_stocks']}"
            )
        elif ruleset == "sonic":
            hud = (
                f"RINGS {self.snapshot['rings']:03d}  "
                f"TICK {self.snapshot['tick']:05d}"
            )
        elif ruleset == "zelda":
            hud = (
                f"LIFE {self.snapshot['health']}  "
                f"KEYS {self.snapshot['keys']}"
            )
        else:
            hud = (
                f"x{self.snapshot['collectibles']:02d}  "
                f"TICK {self.snapshot['tick']:05d}"
            )
        painter.drawText(6, 11, hud)
        if self.snapshot["complete"]:
            painter.fillRect(72, 96, 176, 40, Qt.black)
            painter.drawRect(72, 96, 176, 40)
            if self.source.get("gameplay") == "brawl":
                result = (
                    "YOU WIN"
                    if self.snapshot["brawl_opponent_stocks"] == 0
                    else "CPU WINS"
                )
                painter.drawText(130, 120, result)
            else:
                painter.drawText(108, 120, "LEVEL COMPLETE")
        if self.show_overlay:
            self._draw_input_overlay(painter)
        painter.end()
        return frame

    def _draw_input_overlay(self, painter):
        center = QPointF(282, 199)
        painter.setOpacity(0.72)
        painter.setPen(QPen(QColor("#ffffff"), 1))
        painter.setBrush(QColor(14, 18, 24, 190))
        painter.drawEllipse(center, 30, 30)
        painter.drawEllipse(center, 10, 10)
        directions = (
            (INPUT_UP, 0, -20),
            (INPUT_RIGHT, 20, 0),
            (INPUT_DOWN, 0, 20),
            (INPUT_LEFT, -20, 0),
        )
        for bit, x, y in directions:
            painter.setBrush(
                QColor("#58a6ff") if self.input_mask & bit
                else QColor("#283444")
            )
            painter.drawEllipse(QPointF(center.x() + x, center.y() + y), 5, 5)
        painter.setOpacity(1.0)

    def paintEvent(self, event):
        del event
        painter = QPainter(self)
        painter.setRenderHint(QPainter.SmoothPixmapTransform, False)
        painter.drawImage(self.rect(), self._native_frame())
        painter.end()

    def _key_bit(self, key):
        return {
            Qt.Key_Left: INPUT_LEFT,
            Qt.Key_A: INPUT_LEFT,
            Qt.Key_Right: INPUT_RIGHT,
            Qt.Key_D: INPUT_RIGHT,
            Qt.Key_Up: INPUT_UP,
            Qt.Key_W: INPUT_UP,
            Qt.Key_Down: INPUT_DOWN,
            Qt.Key_S: INPUT_DOWN,
            Qt.Key_Return: self.button_bits["select"],
            Qt.Key_Enter: self.button_bits["select"],
            Qt.Key_Space: self.button_bits["select"],
            Qt.Key_Shift: self.button_bits["play"],
            Qt.Key_Q: self.button_bits["previous"],
            Qt.Key_E: self.button_bits["next"],
            Qt.Key_P: INPUT_PAUSE,
        }.get(key, 0)

    def keyPressEvent(self, event):
        bit = self._key_bit(event.key())
        if bit:
            self.input_mask |= bit
            event.accept()
        else:
            super().keyPressEvent(event)

    def keyReleaseEvent(self, event):
        bit = self._key_bit(event.key())
        if bit:
            self.input_mask &= ~bit
            event.accept()
        else:
            super().keyReleaseEvent(event)


class MakerLitePreviewDialog(QDialog):
    def __init__(self, source, runtime, art_path, settings, parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"Test — {source['title']}")
        self.session = runtime.open_session(source)
        action_bits = {
            "primary": INPUT_PRIMARY,
            "secondary": INPUT_SECONDARY,
            "previous": INPUT_PREVIOUS,
            "next": INPUT_NEXT,
        }
        mapping = settings["mapping"]
        button_bits = {
            button: action_bits[action] for button, action in mapping.items()
        }
        self.surface = _PreviewSurface(
            source, self.session, art_path, button_bits, self
        )
        layout = QVBoxLayout(self)
        layout.addWidget(self.surface, alignment=Qt.AlignCenter)
        self.status = QLabel()
        self.status.setAlignment(Qt.AlignCenter)
        layout.addWidget(self.status)
        controls = QHBoxLayout()
        for label, bit in (
            ("◀", INPUT_LEFT),
            ("▲", INPUT_UP),
            ("▼", INPUT_DOWN),
            ("▶", INPUT_RIGHT),
            (f"Select · {mapping['select'].title()}", button_bits["select"]),
            (f"Play · {mapping['play'].title()}", button_bits["play"]),
            (
                f"Previous · {mapping['previous'].title()}",
                button_bits["previous"],
            ),
            (f"Next · {mapping['next'].title()}", button_bits["next"]),
            ("Pause", INPUT_PAUSE),
        ):
            button = QPushButton(label)
            button.setFocusPolicy(Qt.NoFocus)
            button.pressed.connect(
                lambda selected=bit: self._set_input(selected, True)
            )
            button.released.connect(
                lambda selected=bit: self._set_input(selected, False)
            )
            controls.addWidget(button)
        layout.addLayout(controls)
        options = QHBoxLayout()
        overlay = QCheckBox("Show click-wheel input overlay")
        overlay.setChecked(True)
        overlay.toggled.connect(self._show_overlay)
        options.addWidget(overlay)
        restart = QPushButton("Restart")
        restart.clicked.connect(self._restart)
        options.addWidget(restart)
        close = QPushButton("Return to Editor")
        close.clicked.connect(self.accept)
        options.addWidget(close)
        layout.addLayout(options)
        self.timer = QTimer(self)
        self.timer.setTimerType(Qt.PreciseTimer)
        self.timer.timeout.connect(self._advance)
        self.clock = QElapsedTimer()
        self.clock.start()
        self.accumulator_ns = 0
        self.timer.start(8)
        self.surface.setFocus()
        self._advance()

    def _set_input(self, bit, enabled):
        if enabled:
            self.surface.input_mask |= bit
        else:
            self.surface.input_mask &= ~bit

    def _show_overlay(self, enabled):
        self.surface.show_overlay = enabled
        self.surface.update()

    def _restart(self):
        self.session.reset()
        self.surface.input_mask = 0
        self.surface.snapshot = self.session.snapshot()
        self.surface.entity_states = self.session.entities()
        self.surface.dynamic_states = self.session.dynamics()
        self.accumulator_ns = 0
        self.clock.restart()
        self.surface.setFocus()

    def _advance(self):
        elapsed = min(self.clock.nsecsElapsed(), 50_000_000)
        self.clock.restart()
        self.accumulator_ns += elapsed
        ticks = 0
        while self.accumulator_ns >= 16_666_667 and ticks < 3:
            self.surface.advance()
            self.accumulator_ns -= 16_666_667
            ticks += 1
        if ticks == 0:
            return
        state = self.surface.snapshot
        self.status.setText(
            f"Shared C core · 60 Hz · tick {state['tick']} · "
            f"position {state['player_x'] >> 16}, {state['player_y'] >> 16}"
        )

    def done(self, result):
        self.timer.stop()
        super().done(result)
