"""Integer-pixel tile/entity canvas for the Maker Lite creator."""

from __future__ import annotations

import json
import os
import struct
import zlib

from PySide6.QtCore import QPointF, QRectF, Qt, Signal
from PySide6.QtGui import (
    QBrush,
    QColor,
    QImage,
    QKeyEvent,
    QPainter,
    QPen,
    QPixmap,
)
from PySide6.QtWidgets import QGraphicsScene, QGraphicsView

from services.maker_lite_assets import (
    COLLISION_NAMES,
    MakerLiteAssetError,
    validate_asset_catalog,
)


ASSET_MIME_TYPE = "application/x-rockpod-maker-lite-asset"


class MakerLiteCanvas(QGraphicsView):
    operation_requested = Signal(dict)
    test_requested = Signal()
    tool_picked = Signal(dict)
    brush_shape_changed = Signal(str)
    _ENTITY_KIND_IDS = {
        "player": 1,
        "goal": 2,
        "collectible": 3,
        "enemy": 4,
        "checkpoint": 5,
        "key": 6,
        "door": 7,
        "switch": 8,
        "block": 9,
        "spring": 10,
        "item": 11,
        "pot": 12,
        "npc": 13,
        "shop": 14,
        "house": 15,
        "car": 16,
        "furniture": 17,
        "decoration": 18,
    }

    def __init__(self, parent=None):
        super().__init__(parent)
        self._source = None
        self._tool = {"type": "terrain", "tile": 1, "collision": "solid"}
        self._previous_tool = dict(self._tool)
        self._cells = []
        self._entity_cells = [0] * 16
        self._asset_catalog = []
        self._player_base = 0
        self._kit_error = "Choose an authentic private kit"
        self._paint_cell = None
        self._path_start = None
        self._path_points = []
        self._selected_entity = None
        self._drag_origin = None
        self._drag_copy = False
        self._drop_preview_item = None
        self._drop_outline_item = None
        self._drop_preview_key = None
        self._drop_tool = None
        self._stroke_operations = []
        self._stroke_cells = set()
        self._stroke_items = []
        self._stroke_last_cell = None
        self._brush_shape = "freehand"
        self._shape_anchor = None
        self._pan_anchor = None
        self._space_pan = False
        self._zoom = 1.0
        self.setScene(QGraphicsScene(self))
        self.setAcceptDrops(True)
        self.viewport().setAcceptDrops(True)
        self.setMouseTracking(True)
        self.viewport().setMouseTracking(True)
        self.setRenderHint(QPainter.Antialiasing, False)
        self.setRenderHint(QPainter.SmoothPixmapTransform, False)
        self.setViewportUpdateMode(QGraphicsView.MinimalViewportUpdate)
        self.setOptimizationFlag(QGraphicsView.DontSavePainterState, True)
        self.setTransformationAnchor(QGraphicsView.AnchorUnderMouse)
        self.setDragMode(QGraphicsView.ScrollHandDrag)
        self.setBackgroundBrush(QColor("#1d2530"))
        self.setAlignment(Qt.AlignLeft | Qt.AlignTop)

    def set_project(self, source):
        self._source = source
        self._drop_tool = None
        self._stroke_operations = []
        self._stroke_cells = set()
        self._stroke_last_cell = None
        if (
            self._selected_entity is not None
            and self._selected_entity >= len(source.get("entities", []))
        ):
            self._selected_entity = None
        self._rebuild()

    def set_asset_kit(self, path):
        self._cells = []
        self._entity_cells = [0] * 16
        self._asset_catalog = []
        self._player_base = 0
        self._kit_error = "Authentic private kit is missing"
        try:
            with open(os.fspath(path), "rb") as source:
                data = source.read()
            if len(data) < 64 or data[:4] != b"MLAR":
                raise ValueError("invalid art-kit header")
            version, cell_size, cell_count, player_base, expected_crc = (
                struct.unpack_from("<HHHHI", data, 4)
            )
            content = data[64:]
            pixel_bytes = cell_count * 16 * 16 * 2
            table_bytes = (
                56 if version == 2 else 224 if version in {3, 4} else 0
            )
            expected_size = pixel_bytes + table_bytes
            player_limit = cell_count
            if version == 4:
                frame_header = expected_size
                if len(content) < frame_header + 4:
                    raise ValueError("metasprite table is truncated")
                frame_count, reserved = struct.unpack_from(
                    "<HH", content, frame_header
                )
                expected_size = frame_header + 4 + frame_count * 36
                player_limit = frame_count
                if reserved != 0 or not 14 <= frame_count <= 512:
                    raise ValueError("metasprite table has invalid bounds")
                for frame_index in range(frame_count):
                    offset = frame_header + 4 + frame_index * 36
                    columns, rows, offset_x, offset_y, *frame_cells = (
                        struct.unpack_from("<BBbb16H", content, offset)
                    )
                    used = columns * rows
                    if (
                        not 1 <= columns <= 4
                        or not 1 <= rows <= 4
                        or not -64 <= offset_x <= 64
                        or not -64 <= offset_y <= 64
                        or any(
                            cell != 0xFFFF and cell >= cell_count
                            for cell in frame_cells[:used]
                        )
                        or any(
                            cell != 0xFFFF for cell in frame_cells[used:]
                        )
                    ):
                        raise ValueError("metasprite frame is invalid")
            if (
                version not in {1, 2, 3, 4}
                or cell_size != 16
                or not 1 <= cell_count <= 1536
                or len(content) != expected_size
                or zlib.crc32(content) & 0xFFFFFFFF != expected_crc
                or player_base + 13 >= player_limit
            ):
                raise ValueError("art-kit bounds or checksum failed")
            payload = content[:pixel_bytes]
            self._player_base = player_base
            self._entity_cells = list(data[48:64])
            for cell in range(cell_count):
                image = QImage(16, 16, QImage.Format_RGBA8888)
                offset = cell * 16 * 16 * 2
                for y in range(16):
                    for x in range(16):
                        value = struct.unpack_from(
                            "<H", payload, offset + (y * 16 + x) * 2
                        )[0]
                        if value == 0xF81F:
                            image.setPixelColor(x, y, QColor(255, 0, 255, 0))
                        else:
                            red = ((value >> 11) & 31) * 255 // 31
                            green = ((value >> 5) & 63) * 255 // 63
                            blue = (value & 31) * 255 // 31
                            image.setPixelColor(x, y, QColor(red, green, blue))
                self._cells.append(image)
            manifest_path = os.path.join(
                os.path.dirname(os.fspath(path)),
                "kit.mlk",
            )
            if os.path.isfile(manifest_path):
                with open(manifest_path, "r", encoding="utf-8") as source:
                    manifest = json.load(source)
                entity_cells = {
                    name: self._entity_cells[kind_id]
                    for name, kind_id in self._ENTITY_KIND_IDS.items()
                    if (
                        name != "player"
                        and kind_id < len(self._entity_cells)
                        and self._entity_cells[kind_id]
                    )
                }
                self._asset_catalog = validate_asset_catalog(
                    manifest.get("asset_catalog", []),
                    len(self._cells),
                    entity_cells,
                )
            self._kit_error = ""
        except (
            OSError,
            ValueError,
            struct.error,
            json.JSONDecodeError,
            MakerLiteAssetError,
        ) as exc:
            self._kit_error = f"Authentic private kit unavailable: {exc}"
            self._asset_catalog = []
        self._rebuild()

    def available_entity_kinds(self):
        available = {"player"}
        available.update(
            item["kind"]
            for item in self._asset_catalog
            if item.get("type") == "entity" and item.get("kind")
        )
        for name, kind_id in self._ENTITY_KIND_IDS.items():
            if name == "player":
                continue
            if kind_id < len(self._entity_cells) and self._entity_cells[kind_id]:
                available.add(name)
        return available

    def asset_catalog(self):
        return [
            {
                **item,
                **(
                    {"params": list(item["params"])}
                    if "params" in item
                    else {}
                ),
                **(
                    {"collision": list(item["collision"])}
                    if "collision" in item
                    else {}
                ),
            }
            for item in self._asset_catalog
        ]

    def tool_preview(self, tool):
        """Return imported pixels for a placeable tool, never synthesized art."""
        if not self._cells or not isinstance(tool, dict):
            return None
        catalog_cell = tool.get("cell")
        if (
            isinstance(catalog_cell, int)
            and not isinstance(catalog_cell, bool)
            and 0 <= catalog_cell < len(self._cells)
        ):
            return self._cells[catalog_cell]
        tool_type = tool.get("type")
        if tool_type == "terrain":
            tile = tool.get("tile")
            if (
                isinstance(tile, int)
                and not isinstance(tile, bool)
                and 0 <= tile < len(self._cells)
            ):
                return self._cells[tile]
            return None
        if tool_type != "entity":
            return None
        kind = tool.get("kind")
        if kind == "player":
            cell = self._player_base
        else:
            kind_id = self._ENTITY_KIND_IDS.get(kind, 0)
            cell = (
                self._entity_cells[kind_id]
                if kind_id < len(self._entity_cells)
                else 0
            )
        if kind not in self.available_entity_kinds():
            return None
        if 0 <= cell < len(self._cells):
            return self._cells[cell]
        return None

    def tool_is_draggable(self, tool):
        if not isinstance(tool, dict):
            return False
        if tool.get("type") == "terrain" and tool.get("tile") == 0:
            return False
        return tool.get("type") in {"terrain", "entity"} and (
            self.tool_preview(tool) is not None
        )

    def set_tool(self, tool):
        if (
            isinstance(tool, dict)
            and tool.get("type") == "erase"
            and self._tool.get("type") != "erase"
        ):
            self._previous_tool = dict(self._tool)
        self._tool = dict(tool)
        self._path_start = None
        self._path_points = []
        self._restore_drag_mode()

    def set_brush_shape(self, shape):
        if shape not in {"freehand", "line", "rectangle", "fill"}:
            return
        self._brush_shape = shape
        self._shape_anchor = None
        self._clear_stroke_preview()

    def _restore_drag_mode(self):
        self.setDragMode(
            QGraphicsView.ScrollHandDrag
            if self._space_pan
            or self._tool.get("type") not in {
                "select", "terrain", "entity", "path", "event",
                "erase",
            }
            else QGraphicsView.NoDrag
        )

    def _rebuild(self):
        scene = self.scene()
        self._drop_preview_item = None
        self._drop_outline_item = None
        self._drop_preview_key = None
        self._stroke_items = []
        self._stroke_last_cell = None
        scene.clear()
        if not self._source:
            scene.setSceneRect(QRectF(0, 0, 640, 360))
            return
        width, height = self._source["size"]
        tile_size = self._source.get("tile_size", 16)
        tiles = [[0 for _ in range(width)] for _ in range(height)]
        for operation in self._source.get("terrain", []):
            if "point" in operation:
                x, y = operation["point"]
                rect = [x, y, 1, 1]
            else:
                rect = operation.get("rect", [0, 0, 0, 0])
            x, y, rect_width, rect_height = rect
            for tile_y in range(y, min(height, y + rect_height)):
                for tile_x in range(x, min(width, x + rect_width)):
                    tiles[tile_y][tile_x] = int(operation.get("tile", 0))
        pixel_width = width * tile_size
        pixel_height = height * tile_size
        rendered = QImage(
            max(1, pixel_width), max(1, pixel_height), QImage.Format_RGB32
        )
        rendered.fill(QColor("#10151c"))
        painter = QPainter(rendered)
        painter.setRenderHint(QPainter.SmoothPixmapTransform, False)
        if self._cells:
            for tile_y, row in enumerate(tiles):
                for tile_x, tile in enumerate(row):
                    if 0 <= tile < len(self._cells):
                        painter.drawImage(
                            tile_x * tile_size,
                            tile_y * tile_size,
                            self._cells[tile],
                        )
        for entity in self._source.get("entities", []):
            kind = entity["kind"]
            if kind == "player":
                cell = self._player_base
            else:
                kind_id = self._ENTITY_KIND_IDS.get(kind, 0)
                fallback_cell = (
                    self._entity_cells[kind_id]
                    if kind_id < len(self._entity_cells)
                    else 0
                )
                cell = int(entity.get("render_cell", fallback_cell))
            if self._cells and 0 <= cell < len(self._cells):
                painter.drawImage(
                    int(entity["x"]) - tile_size // 2,
                    int(entity["y"]) - tile_size,
                    self._cells[cell],
                )
        painter.end()
        scene.addPixmap(QPixmap.fromImage(rendered))
        if (
            self._selected_entity is not None
            and self._selected_entity < len(self._source.get("entities", []))
        ):
            entity = self._source["entities"][self._selected_entity]
            scene.addRect(
                int(entity["x"]) - tile_size // 2 - 3,
                int(entity["y"]) - tile_size - 3,
                tile_size + 6,
                tile_size + 6,
                QPen(QColor("#ffffff"), 2, Qt.DashLine),
            )
        room_pen = QPen(QColor("#58a6ff"), 2, Qt.DashLine)
        for room in self._source.get("rooms", []):
            rect = room.get("rect", [])
            if len(rect) != 4:
                continue
            room_x, room_y, room_width, room_height = map(int, rect)
            scene.addRect(
                room_x * tile_size,
                room_y * tile_size,
                room_width * tile_size,
                room_height * tile_size,
                room_pen,
            )
            label = scene.addText(
                str(room.get("title", room.get("id", "Room")))
            )
            label.setDefaultTextColor(QColor("#b9d7ff"))
            label.setPos(room_x * tile_size + 6, room_y * tile_size + 18)
        link_pen = QPen(QColor("#e879f9"), 2, Qt.DashLine)
        for link in self._source.get("room_links", []):
            first = link.get("from_position", [])
            second = link.get("to_position", [])
            if len(first) == 2 and len(second) == 2:
                scene.addLine(
                    int(first[0]), int(first[1]),
                    int(second[0]), int(second[1]),
                    link_pen,
                )
                for point in (first, second):
                    scene.addEllipse(
                        int(point[0]) - 5,
                        int(point[1]) - 5,
                        10,
                        10,
                        link_pen,
                        QColor("#e879f9"),
                    )
        for path in self._source.get("paths", []):
            points = path.get("points", [])
            surface = bool(path.get("surface", False))
            color = QColor("#4dd7e5") if surface else QColor("#ffcb6b")
            path_pen = QPen(color, 2)
            for first, second in zip(points, points[1:]):
                scene.addLine(
                    int(first[0]), int(first[1]),
                    int(second[0]), int(second[1]),
                    path_pen,
                )
                if surface:
                    dx = int(second[0]) - int(first[0])
                    dy = int(second[1]) - int(first[1])
                    length = max(1, abs(dx), abs(dy))
                    middle_x = (int(first[0]) + int(second[0])) // 2
                    middle_y = (int(first[1]) + int(second[1])) // 2
                    scene.addLine(
                        middle_x,
                        middle_y,
                        middle_x + dy * 8 // length,
                        middle_y - dx * 8 // length,
                        QPen(color, 1),
                    )
            for point in points:
                scene.addEllipse(
                    int(point[0]) - 3, int(point[1]) - 3, 6, 6,
                    path_pen, color,
                )
        if len(self._path_points) > 1:
            pending_pen = QPen(QColor("#4dd7e5"), 2, Qt.DashLine)
            for first, second in zip(
                self._path_points, self._path_points[1:]
            ):
                scene.addLine(*first, *second, pending_pen)
        event_pen = QPen(QColor("#c792ea"), 1, Qt.DashLine)
        for event in self._source.get("events", []):
            region = event.get("region", [0, 0, 0, 0])
            if len(region) == 4 and region[2] > 0 and region[3] > 0:
                scene.addRect(*map(int, region), event_pen)
        grid_pen = QPen(QColor(255, 255, 255, 36), 0)
        for x in range(0, pixel_width + 1, tile_size):
            scene.addLine(x, 0, x, pixel_height, grid_pen)
        for y in range(0, pixel_height + 1, tile_size):
            scene.addLine(0, y, pixel_width, y, grid_pen)
        view_width, view_height = self._source.get("view", [256, 224])
        scene.addRect(
            0, 0, view_width, view_height,
            QPen(QColor("#58a6ff"), 1, Qt.DashLine),
        )
        if self._kit_error:
            label = scene.addText(self._kit_error)
            label.setDefaultTextColor(QColor("#ffcf70"))
            label.setPos(12, 12)
        scene.setSceneRect(0, 0, pixel_width, pixel_height)

    def _operation_at(self, tool, point, erase=False):
        if not self._source or not isinstance(tool, dict):
            return None
        width, height = self._source["size"]
        tile_size = self._source.get("tile_size", 16)
        tile_x, tile_y = int(point.x()) // tile_size, int(point.y()) // tile_size
        if not (0 <= tile_x < width and 0 <= tile_y < height):
            return None
        if erase:
            return {
                "kind": "erase",
                "tile": [tile_x, tile_y],
                "pixel": [
                    tile_x * tile_size + tile_size // 2,
                    tile_y * tile_size + tile_size // 2,
                ],
            }
        if tool.get("type") == "terrain":
            return {
                "kind": "terrain",
                "value": {
                    "point": [tile_x, tile_y],
                    "tile": int(tool.get("tile", 1)),
                    "collision": tool.get("collision", "solid"),
                },
            }
        if tool.get("type") == "entity":
            default_parameters = {
                "collectible": [12, 12, 100, 0],
                "enemy": [16, 16, 0, 0],
                "checkpoint": [16, 32, 0, 0],
                "spring": [16, 16, 8, 0],
            }.get(tool["kind"], [16, 16, 0, 0])
            parameters = list(tool.get("params", default_parameters))
            parameters.extend([0] * (4 - len(parameters)))
            return {
                "kind": "entity",
                "value": {
                    "kind": tool["kind"],
                    "x": tile_x * tile_size + tile_size // 2,
                    "y": tile_y * tile_size + tile_size,
                    "params": parameters[:4],
                    "flags": int(tool.get("flags", 0)),
                    "render_cell": int(
                        tool.get("frame", tool.get("cell", 0))
                    ),
                    **(
                        {"asset_id": tool["asset_id"]}
                        if tool.get("asset_id")
                        else {}
                    ),
                },
            }
        if tool.get("type") == "event":
            action = tool.get("action", "set_checkpoint")
            params = (
                [
                    tile_x * tile_size + tile_size // 2,
                    tile_y * tile_size + tile_size,
                ]
                if action == "set_checkpoint"
                else [int(tool.get("effect", 0)), 0]
            )
            return {
                "kind": "event",
                "value": {
                    "trigger": "entered_region",
                    "condition": "always",
                    "action": action,
                    "region": [
                        tile_x * tile_size,
                        tile_y * tile_size,
                        tile_size,
                        tile_size,
                    ],
                    "params": params,
                    "one_shot": True,
                },
            }
        return None

    def _request_at(self, point, erase=False):
        if not self._source:
            return
        width, height = self._source["size"]
        tile_size = self._source.get("tile_size", 16)
        tile_x, tile_y = int(point.x()) // tile_size, int(point.y()) // tile_size
        if not (0 <= tile_x < width and 0 <= tile_y < height):
            return
        paint_cell = (tile_x, tile_y, erase)
        if paint_cell == self._paint_cell:
            return
        self._paint_cell = paint_cell
        if not erase and self._tool.get("type") == "path":
            pixel = [
                tile_x * tile_size + tile_size // 2,
                tile_y * tile_size + tile_size // 2,
            ]
            if self._tool.get("surface", False):
                if not self._path_points or self._path_points[-1] != pixel:
                    self._path_points.append(pixel)
                    self._rebuild()
                return
            if self._path_start is None:
                self._path_start = pixel
                return
            operation = {
                "kind": "path",
                "value": {
                    "points": [self._path_start, pixel],
                    "speed": int(self._tool.get("speed", 64)),
                    "ping_pong": True,
                    "entity_kind": self._tool.get("entity_kind", "block"),
                },
            }
            self._path_start = None
        else:
            operation = self._operation_at(self._tool, point, erase)
            if operation is None:
                return
        if self._tool.get("asset_id"):
            operation["asset_id"] = self._tool["asset_id"]
        self.operation_requested.emit(operation)

    def _decode_drag_tool(self, mime_data):
        if not mime_data.hasFormat(ASSET_MIME_TYPE):
            return None
        payload = bytes(mime_data.data(ASSET_MIME_TYPE))
        if not payload or len(payload) > 2048:
            return None
        try:
            tool = json.loads(payload.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            return None
        if not self.tool_is_draggable(tool):
            return None
        if tool.get("type") == "terrain":
            if set(tool) - {
                "type",
                "tile",
                "collision",
                "cell",
                "asset_id",
            }:
                return None
            tile = tool.get("tile")
            if not isinstance(tile, int) or isinstance(tile, bool):
                return None
            if tool.get("cell", tile) != tile:
                return None
            collision = tool.get("collision", "solid")
            if not (
                (
                    isinstance(collision, str)
                    and collision in COLLISION_NAMES
                )
                or (
                    isinstance(collision, list)
                    and all(
                        isinstance(name, str)
                        and name in COLLISION_NAMES
                        for name in collision
                    )
                )
            ):
                return None
        else:
            if set(tool) - {
                "type",
                "kind",
                "cell",
                "asset_id",
                "params",
                "flags",
                "frame",
            }:
                return None
            if tool.get("kind") not in self.available_entity_kinds():
                return None
            cell = tool.get("cell", 0)
            if (
                not isinstance(cell, int)
                or isinstance(cell, bool)
                or not 0 < cell < len(self._cells)
            ):
                return None
            params = tool.get("params", [])
            flags = tool.get("flags", 0)
            frame = tool.get("frame")
            if (
                not isinstance(params, list)
                or len(params) > 4
                or any(
                    not isinstance(value, int)
                    or isinstance(value, bool)
                    or not -32768 <= value <= 32767
                    for value in params
                )
                or not isinstance(flags, int)
                or isinstance(flags, bool)
                or not 0 <= flags <= 16383
                or (
                    frame is not None
                    and (
                        not isinstance(frame, int)
                        or isinstance(frame, bool)
                        or not 0 <= frame < 512
                        or not flags & 0x2000
                    )
                )
            ):
                return None
        asset_id = tool.get("asset_id")
        if asset_id is not None and (
            not isinstance(asset_id, str)
            or not 1 <= len(asset_id) <= 32
            or any(
                not (
                    char.isascii()
                    and (char.islower() or char.isdigit() or char in "_-")
                )
                for char in asset_id
            )
        ):
            return None
        if asset_id is not None:
            catalog_item = next(
                (
                    item
                    for item in self._asset_catalog
                    if item["id"] == asset_id
                ),
                None,
            )
            if catalog_item is None:
                return None
            if (
                catalog_item["type"] != tool["type"]
                or int(catalog_item["cell"]) != int(tool.get("cell", -1))
            ):
                return None
            if tool["type"] == "terrain":
                if (
                    list(catalog_item.get("collision", []))
                    != (
                        [tool["collision"]]
                        if isinstance(tool.get("collision"), str)
                        else list(tool.get("collision", []))
                    )
                ):
                    return None
            elif (
                catalog_item["kind"] != tool["kind"]
                or list(catalog_item.get("params", []))
                != list(tool.get("params", []))
                or int(catalog_item.get("flags", 0))
                != int(tool.get("flags", 0))
                or catalog_item.get("frame") != tool.get("frame")
            ):
                return None
        return tool

    def _clear_drop_preview(self):
        scene = self.scene()
        for item in (self._drop_preview_item, self._drop_outline_item):
            if item is not None and item.scene() is scene:
                scene.removeItem(item)
        self._drop_preview_item = None
        self._drop_outline_item = None
        self._drop_preview_key = None

    def _show_drop_preview(self, tool, point):
        if not self._source:
            return False
        operation = self._operation_at(tool, point)
        tile_size = int(self._source.get("tile_size", 16))
        tile_x = int(point.x()) // tile_size
        tile_y = int(point.y()) // tile_size
        left = tile_x * tile_size
        top = tile_y * tile_size
        valid = operation is not None
        image = self.tool_preview(tool)
        preview_key = (
            tool.get("type"),
            tool.get("cell"),
            tool.get("tile"),
            tool.get("kind"),
        )
        if preview_key != self._drop_preview_key:
            self._clear_drop_preview()
            self._drop_preview_key = preview_key
        if image is not None and self._drop_preview_item is None:
            self._drop_preview_item = self.scene().addPixmap(
                QPixmap.fromImage(image)
            )
            self._drop_preview_item.setZValue(1000)
        if self._drop_preview_item is not None:
            self._drop_preview_item.setPos(left, top)
            self._drop_preview_item.setOpacity(0.72 if valid else 0.35)
        color = QColor("#58d68d") if valid else QColor("#ff5f57")
        if self._drop_outline_item is None:
            self._drop_outline_item = self.scene().addRect(
                left,
                top,
                tile_size,
                tile_size,
                QPen(color, 2, Qt.DashLine),
            )
            self._drop_outline_item.setZValue(1001)
        else:
            self._drop_outline_item.setRect(
                left,
                top,
                tile_size,
                tile_size,
            )
            self._drop_outline_item.setPen(
                QPen(color, 2, Qt.DashLine)
            )
        return valid

    def _clear_stroke_preview(self):
        scene = self.scene()
        for item in self._stroke_items:
            if item is not None and item.scene() is scene:
                scene.removeItem(item)
        self._stroke_items = []

    def _start_stroke(self):
        self._clear_drop_preview()
        self._clear_stroke_preview()
        self._stroke_operations = []
        self._stroke_cells = set()
        self._stroke_last_cell = None

    @staticmethod
    def _line_cells(first, second):
        x, y = first
        target_x, target_y = second
        cells = [(x, y)]
        delta_x = abs(target_x - x)
        delta_y = -abs(target_y - y)
        step_x = 1 if x < target_x else -1
        step_y = 1 if y < target_y else -1
        error = delta_x + delta_y
        while (x, y) != (target_x, target_y):
            twice_error = error * 2
            if twice_error >= delta_y:
                error += delta_y
                x += step_x
            if twice_error <= delta_x:
                error += delta_x
                y += step_y
            cells.append((x, y))
        return cells

    def _shape_cells(self, first, second):
        if self._brush_shape == "line":
            return self._line_cells(first, second)
        left, right = sorted((first[0], second[0]))
        top, bottom = sorted((first[1], second[1]))
        return [
            (x, y)
            for y in range(top, bottom + 1)
            for x in range(left, right + 1)
        ]

    def _preview_shape_at(self, point, erase):
        tile_size = int(self._source.get("tile_size", 16))
        target = (
            int(point.x()) // tile_size,
            int(point.y()) // tile_size,
        )
        self._clear_stroke_preview()
        self._stroke_operations = []
        self._stroke_cells = set()
        for tile_x, tile_y in self._shape_cells(self._shape_anchor, target):
            self._append_stroke_cell(tile_x, tile_y, erase)

    def _terrain_grid(self):
        width, height = map(int, self._source["size"])
        grid = [[0 for _x in range(width)] for _y in range(height)]
        for operation in self._source.get("terrain", []):
            if "point" in operation:
                x, y = map(int, operation["point"])
                rect = [x, y, 1, 1]
            else:
                rect = list(map(int, operation.get("rect", [0, 0, 0, 0])))
            x, y, rect_width, rect_height = rect
            for tile_y in range(max(0, y), min(height, y + rect_height)):
                for tile_x in range(max(0, x), min(width, x + rect_width)):
                    grid[tile_y][tile_x] = int(operation.get("tile", 0))
        return grid

    def _fill_at(self, point, erase):
        tile_size = int(self._source.get("tile_size", 16))
        start = (
            int(point.x()) // tile_size,
            int(point.y()) // tile_size,
        )
        grid = self._terrain_grid()
        width, height = map(int, self._source["size"])
        if not (0 <= start[0] < width and 0 <= start[1] < height):
            return
        old_tile = grid[start[1]][start[0]]
        new_tile = 0 if erase else int(self._tool.get("tile", 1))
        if old_tile == new_tile:
            return
        self._start_stroke()
        pending = [start]
        visited = set()
        while pending and len(visited) < 4096:
            tile_x, tile_y = pending.pop()
            if (
                (tile_x, tile_y) in visited
                or not 0 <= tile_x < width
                or not 0 <= tile_y < height
                or grid[tile_y][tile_x] != old_tile
            ):
                continue
            visited.add((tile_x, tile_y))
            self._append_stroke_cell(tile_x, tile_y, erase)
            pending.extend(
                (
                    (tile_x - 1, tile_y),
                    (tile_x + 1, tile_y),
                    (tile_x, tile_y - 1),
                    (tile_x, tile_y + 1),
                )
            )
        self._finish_stroke()

    def _append_stroke_at(self, point, erase):
        tile_size = int(self._source.get("tile_size", 16))
        target = (
            int(point.x()) // tile_size,
            int(point.y()) // tile_size,
        )
        cells = []
        if self._stroke_last_cell is None:
            cells.append(target)
        else:
            cells.extend(self._line_cells(self._stroke_last_cell, target)[1:])
        self._stroke_last_cell = target
        for tile_x, tile_y in cells:
            self._append_stroke_cell(tile_x, tile_y, erase)

    def _append_stroke_cell(self, tile_x, tile_y, erase):
        tile_size = int(self._source.get("tile_size", 16))
        operation = self._operation_at(
            self._tool,
            QPointF(
                tile_x * tile_size + tile_size // 2,
                tile_y * tile_size + tile_size // 2,
            ),
            erase,
        )
        if operation is None:
            return
        if operation["kind"] == "erase":
            tile_x, tile_y = operation["tile"]
        else:
            tile_x, tile_y = operation["value"]["point"]
        cell_key = (tile_x, tile_y)
        if cell_key in self._stroke_cells or len(self._stroke_cells) >= 4096:
            return
        self._stroke_cells.add(cell_key)
        self._stroke_operations.append(operation)
        left = tile_x * tile_size
        top = tile_y * tile_size
        if erase:
            item = self.scene().addRect(
                left,
                top,
                tile_size,
                tile_size,
                QPen(QColor("#ff5f57"), 1),
                QBrush(QColor(255, 95, 87, 96)),
            )
        else:
            image = self.tool_preview(self._tool)
            if image is None:
                return
            item = self.scene().addPixmap(QPixmap.fromImage(image))
            item.setPos(left, top)
            item.setOpacity(0.82)
        item.setZValue(900)
        self._stroke_items.append(item)

    def _finish_stroke(self):
        if not self._stroke_operations:
            self._clear_stroke_preview()
            self._stroke_last_cell = None
            return
        operations = list(self._stroke_operations)
        self._stroke_operations = []
        self._stroke_cells = set()
        self._stroke_last_cell = None
        self._clear_stroke_preview()
        request = {"kind": "stroke", "operations": operations}
        if self._tool.get("asset_id"):
            request["asset_id"] = self._tool["asset_id"]
        self.operation_requested.emit(request)

    def _tool_at(self, point):
        entity_index = self._entity_at(point)
        if entity_index is not None:
            entity = self._source["entities"][entity_index]
            asset_id = str(entity.get("asset_id", ""))
            catalog = next(
                (
                    item
                    for item in self._asset_catalog
                    if item.get("id") == asset_id
                ),
                None,
            )
            if catalog:
                return {
                    "type": "entity",
                    "kind": catalog["kind"],
                    "cell": int(catalog["cell"]),
                    "asset_id": catalog["id"],
                    "params": list(catalog.get("params", [])),
                    "flags": int(catalog.get("flags", 0)),
                }
        tile_size = int(self._source.get("tile_size", 16))
        tile_x = int(point.x()) // tile_size
        tile_y = int(point.y()) // tile_size
        grid = self._terrain_grid()
        if not (
            0 <= tile_y < len(grid)
            and 0 <= tile_x < len(grid[tile_y])
        ):
            return None
        tile = grid[tile_y][tile_x]
        catalog = next(
            (
                item
                for item in self._asset_catalog
                if item.get("type") == "terrain"
                and int(item.get("cell", -1)) == tile
            ),
            None,
        )
        if not catalog:
            return None
        return {
            "type": "terrain",
            "tile": tile,
            "cell": tile,
            "collision": list(catalog.get("collision", [])),
            "asset_id": catalog["id"],
        }

    def dragEnterEvent(self, event):
        tool = self._decode_drag_tool(event.mimeData())
        if tool is None:
            event.ignore()
            return
        self._drop_tool = tool
        point = self.mapToScene(event.position().toPoint())
        self._show_drop_preview(tool, point)
        event.setDropAction(Qt.CopyAction)
        event.accept()

    def dragMoveEvent(self, event):
        tool = self._decode_drag_tool(event.mimeData())
        if tool is None:
            self._clear_drop_preview()
            self._drop_tool = None
            event.ignore()
            return
        self._drop_tool = tool
        point = self.mapToScene(event.position().toPoint())
        if self._show_drop_preview(tool, point):
            event.setDropAction(Qt.CopyAction)
            event.accept()
        else:
            event.ignore()

    def dragLeaveEvent(self, event):
        self._clear_drop_preview()
        self._drop_tool = None
        event.accept()

    def dropEvent(self, event):
        tool = self._decode_drag_tool(event.mimeData())
        point = self.mapToScene(event.position().toPoint())
        operation = self._operation_at(tool, point) if tool is not None else None
        self._clear_drop_preview()
        self._drop_tool = None
        if operation is None:
            event.ignore()
            return
        self.set_tool(tool)
        if tool.get("asset_id"):
            operation["asset_id"] = tool["asset_id"]
        self.operation_requested.emit(operation)
        event.setDropAction(Qt.CopyAction)
        event.accept()

    def _entity_at(self, point):
        if not self._source:
            return None
        tile_size = int(self._source.get("tile_size", 16))
        best = None
        best_distance = tile_size * tile_size * 2
        for index, entity in enumerate(self._source.get("entities", [])):
            dx = int(entity["x"]) - int(point.x())
            dy = int(entity["y"]) - tile_size // 2 - int(point.y())
            distance = dx * dx + dy * dy
            if distance <= best_distance:
                best = index
                best_distance = distance
        return best

    def _snapped_entity_position(self, point):
        tile_size = int(self._source.get("tile_size", 16))
        width, height = map(int, self._source["size"])
        return (
            max(
                tile_size // 2,
                min(
                    width * tile_size - tile_size // 2,
                    int(point.x()) // tile_size * tile_size + tile_size // 2,
                ),
            ),
            max(
                tile_size,
                min(
                    height * tile_size,
                    int(point.y()) // tile_size * tile_size + tile_size,
                ),
            ),
        )

    def _finish_surface_path(self):
        if (
            self._tool.get("type") != "path"
            or not self._tool.get("surface", False)
            or len(self._path_points) < 2
        ):
            return
        self.operation_requested.emit(
            {
                "kind": "path",
                "value": {
                    "points": list(self._path_points),
                    "speed": int(self._tool.get("speed", 256)),
                    "ping_pong": False,
                    "surface": True,
                },
            }
        )
        self._path_points = []
        self._rebuild()

    def mousePressEvent(self, event):
        if event.button() == Qt.MiddleButton:
            self._pan_anchor = event.position()
            self.setCursor(Qt.ClosedHandCursor)
            event.accept()
            return
        if self._space_pan:
            super().mousePressEvent(event)
            return
        if not self._source or event.button() not in {Qt.LeftButton, Qt.RightButton}:
            super().mousePressEvent(event)
            return
        point = self.mapToScene(event.position().toPoint())
        self._clear_drop_preview()
        if event.modifiers() & Qt.AltModifier:
            tool = self._tool_at(point)
            if tool is not None:
                self.set_tool(tool)
                self.tool_picked.emit(tool)
            event.accept()
            return
        if event.button() == Qt.LeftButton:
            best = self._entity_at(point)
            if (
                self._tool.get("type") != "erase"
                and (best is not None or self._tool.get("type") == "select")
            ):
                self._selected_entity = best
                self._drag_origin = (
                    best,
                    int(point.x()),
                    int(point.y()),
                ) if best is not None else None
                self._drag_copy = bool(
                    event.modifiers()
                    & (Qt.ControlModifier | Qt.MetaModifier)
                )
                self._rebuild()
                event.accept()
                return
        if (
            event.button() == Qt.RightButton
            or self._tool.get("type") in {"terrain", "erase"}
        ):
            self._selected_entity = None
            self._drag_origin = None
            self._drag_copy = False
            erase = (
                event.button() == Qt.RightButton
                or self._tool.get("type") == "erase"
            )
            if self._brush_shape == "fill":
                self._fill_at(point, erase)
            else:
                self._start_stroke()
                if self._brush_shape in {"line", "rectangle"}:
                    tile_size = int(self._source.get("tile_size", 16))
                    self._shape_anchor = (
                        int(point.x()) // tile_size,
                        int(point.y()) // tile_size,
                    )
                    self._preview_shape_at(point, erase)
                else:
                    self._append_stroke_at(point, erase)
            event.accept()
            return
        if self._tool.get("type") == "select":
            if event.button() == Qt.RightButton:
                self._selected_entity = self._entity_at(point)
                self._rebuild()
                event.accept()
                return
        self._selected_entity = None
        self._drag_origin = None
        self._drag_copy = False
        self._paint_cell = None
        self._request_at(
            point,
            erase=(
                event.button() == Qt.RightButton
                or self._tool.get("type") == "erase"
            ),
        )

    def mouseMoveEvent(self, event):
        if self._pan_anchor is not None and event.buttons() & Qt.MiddleButton:
            delta = event.position() - self._pan_anchor
            self._pan_anchor = event.position()
            self.horizontalScrollBar().setValue(
                self.horizontalScrollBar().value() - int(delta.x())
            )
            self.verticalScrollBar().setValue(
                self.verticalScrollBar().value() - int(delta.y())
            )
            event.accept()
            return
        if (
            event.buttons() & Qt.LeftButton
            and self._drag_origin is not None
            and self._selected_entity is not None
        ):
            point = self.mapToScene(event.position().toPoint())
            entity = self._source["entities"][self._selected_entity]
            self._show_drop_preview(
                {"type": "entity", "kind": entity["kind"]},
                point,
            )
            event.accept()
            return
        if (
            self._stroke_operations
            and (
                event.buttons() & Qt.RightButton
                or event.buttons() & Qt.LeftButton
            )
        ):
            point = self.mapToScene(event.position().toPoint())
            erase = bool(
                event.buttons() & Qt.RightButton
                or self._tool.get("type") == "erase"
            )
            if (
                self._shape_anchor is not None
                and self._brush_shape in {"line", "rectangle"}
            ):
                self._preview_shape_at(point, erase)
            else:
                self._append_stroke_at(point, erase)
            event.accept()
            return
        if not event.buttons() and self.tool_is_draggable(self._tool):
            self._show_drop_preview(
                self._tool,
                self.mapToScene(event.position().toPoint()),
            )
            event.accept()
            return
        super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event):
        if event.button() == Qt.MiddleButton and self._pan_anchor is not None:
            self._pan_anchor = None
            self.unsetCursor()
            event.accept()
            return
        if (
            self._stroke_operations
            and event.button() in {Qt.LeftButton, Qt.RightButton}
        ):
            self._finish_stroke()
            self._shape_anchor = None
            self._paint_cell = None
            event.accept()
            return
        if (
            event.button() == Qt.LeftButton
            and self._drag_origin is not None
            and self._selected_entity is not None
        ):
            point = self.mapToScene(event.position().toPoint())
            x, y = self._snapped_entity_position(point)
            entity = self._source["entities"][self._selected_entity]
            self._clear_drop_preview()
            if self._drag_copy:
                value = {
                    key: (
                        list(source_value)
                        if isinstance(source_value, list)
                        else source_value
                    )
                    for key, source_value in entity.items()
                }
                value["x"] = x
                value["y"] = y
                self.operation_requested.emit(
                    {
                        "kind": "entity",
                        "value": value,
                        **(
                            {"asset_id": value["asset_id"]}
                            if value.get("asset_id")
                            else {}
                        ),
                    }
                )
            elif x != int(entity["x"]) or y != int(entity["y"]):
                self.operation_requested.emit(
                    {
                        "kind": "move_entity",
                        "index": self._selected_entity,
                        "x": x,
                        "y": y,
                    }
                )
            self._drag_origin = None
            self._drag_copy = False
            event.accept()
            return
        self._paint_cell = None
        super().mouseReleaseEvent(event)

    def leaveEvent(self, event):
        if not self._stroke_operations and self._drag_origin is None:
            self._clear_drop_preview()
        super().leaveEvent(event)

    def mouseDoubleClickEvent(self, event):
        if (
            event.button() == Qt.LeftButton
            and self._tool.get("type") == "path"
            and self._tool.get("surface", False)
        ):
            point = self.mapToScene(event.position().toPoint())
            tile_size = int(self._source.get("tile_size", 16))
            pixel = [
                int(point.x()) // tile_size * tile_size + tile_size // 2,
                int(point.y()) // tile_size * tile_size + tile_size // 2,
            ]
            if not self._path_points or self._path_points[-1] != pixel:
                self._path_points.append(pixel)
            self._finish_surface_path()
            event.accept()
            return
        super().mouseDoubleClickEvent(event)

    def keyPressEvent(self, event: QKeyEvent):
        if event.key() == Qt.Key_P and not event.isAutoRepeat():
            self.test_requested.emit()
            event.accept()
            return
        if event.key() == Qt.Key_Space and not event.isAutoRepeat():
            self._space_pan = True
            self._restore_drag_mode()
            event.accept()
            return
        if event.key() == Qt.Key_E and not event.isAutoRepeat():
            if self._tool.get("type") == "erase":
                self.set_tool(self._previous_tool)
            else:
                self.set_tool({"type": "erase"})
            event.accept()
            return
        brush_shortcuts = {
            Qt.Key_B: "freehand",
            Qt.Key_L: "line",
            Qt.Key_R: "rectangle",
            Qt.Key_F: "fill",
        }
        if event.key() in brush_shortcuts and not event.isAutoRepeat():
            shape = brush_shortcuts[event.key()]
            self.set_brush_shape(shape)
            self.brush_shape_changed.emit(shape)
            event.accept()
            return
        if (
            event.key() == Qt.Key_D
            and event.modifiers()
            & (Qt.ControlModifier | Qt.MetaModifier)
            and self._selected_entity is not None
        ):
            entity = dict(self._source["entities"][self._selected_entity])
            entity["x"], entity["y"] = self._snapped_entity_position(
                QPointF(int(entity["x"]) + 16, int(entity["y"]))
            )
            self.operation_requested.emit(
                {
                    "kind": "entity",
                    "value": entity,
                    **(
                        {"asset_id": entity["asset_id"]}
                        if entity.get("asset_id")
                        else {}
                    ),
                }
            )
            event.accept()
            return
        if (
            event.key() in {
                Qt.Key_Left,
                Qt.Key_Right,
                Qt.Key_Up,
                Qt.Key_Down,
            }
            and self._selected_entity is not None
        ):
            entity = self._source["entities"][self._selected_entity]
            step = (
                int(self._source.get("tile_size", 16))
                if event.modifiers() & Qt.ShiftModifier
                else 1
            )
            delta_x = (
                step if event.key() == Qt.Key_Right
                else -step if event.key() == Qt.Key_Left
                else 0
            )
            delta_y = (
                step if event.key() == Qt.Key_Down
                else -step if event.key() == Qt.Key_Up
                else 0
            )
            width, height = map(int, self._source["size"])
            tile_size = int(self._source.get("tile_size", 16))
            self.operation_requested.emit(
                {
                    "kind": "move_entity",
                    "index": self._selected_entity,
                    "x": max(
                        tile_size // 2,
                        min(
                            width * tile_size - tile_size // 2,
                            int(entity["x"]) + delta_x,
                        ),
                    ),
                    "y": max(
                        tile_size,
                        min(
                            height * tile_size,
                            int(entity["y"]) + delta_y,
                        ),
                    ),
                }
            )
            event.accept()
            return
        if (
            event.key() in {Qt.Key_Delete, Qt.Key_Backspace}
            and self._selected_entity is not None
        ):
            self.operation_requested.emit(
                {
                    "kind": "delete_entity",
                    "index": self._selected_entity,
                }
            )
            self._selected_entity = None
            event.accept()
            return
        if (
            event.key() in {Qt.Key_Return, Qt.Key_Enter}
            and self._tool.get("type") == "path"
            and self._tool.get("surface", False)
        ):
            self._finish_surface_path()
            event.accept()
            return
        if event.key() == Qt.Key_Escape:
            if self._path_points or self._selected_entity is not None:
                self._path_points = []
                self._selected_entity = None
                self._drag_origin = None
                self._drag_copy = False
                self._clear_drop_preview()
                self._rebuild()
                event.accept()
                return
        super().keyPressEvent(event)

    def keyReleaseEvent(self, event: QKeyEvent):
        if event.key() == Qt.Key_Space and not event.isAutoRepeat():
            self._space_pan = False
            self._restore_drag_mode()
            event.accept()
            return
        super().keyReleaseEvent(event)

    def wheelEvent(self, event):
        delta = event.pixelDelta().y()
        if not delta:
            delta = event.angleDelta().y()
        if not delta:
            event.ignore()
            return
        factor = 2.0 ** (delta / 480.0)
        proposed = self._zoom * factor
        bounded = max(0.25, min(8.0, proposed))
        actual = bounded / self._zoom
        if actual != 1.0:
            self._zoom = bounded
            self.scale(actual, actual)
        event.accept()
