"""Validated source-project compiler for Rockpod Maker Lite.

The source model is intentionally ordinary JSON for recovery and version
control.  Device packs are bounded little-endian files consumed by the shared
C runtime in ``lib/maker_lite``.
"""

from __future__ import annotations

import copy
import json
import re
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path


PACK_MAGIC = b"RPML"
PACK_VERSION = 3
PACK_HEADER_SIZE = 128
PACK_ENTITY_V1_SIZE = 16
PACK_ENTITY_SIZE = 18
MAX_RENDER_CELL = 1535
MAX_MAP_WIDTH = 512
MAX_MAP_HEIGHT = 64
MAX_ENTITIES = 192
MAX_EVENTS = 256
MAX_PATHS = 64
MAX_PATH_POINTS = 512
MAX_TITLE_BYTES = 80
ID_SIZE = 32
PACK_EVENT_SIZE = 24
PACK_PATH_SIZE = 12
PACK_PATH_POINT_SIZE = 4
LEVEL_LIFE_SIM = 1 << 0
LEVEL_BRAWL = 1 << 1

RULESETS = {
    "mario": 1,
    "zelda": 2,
    "sonic": 3,
}

COLLISION_FLAGS = {
    "solid": 1 << 0,
    "hazard": 1 << 1,
    "one_way": 1 << 2,
    "slope_up": 1 << 3,
    "slope_down": 1 << 4,
    "water": 1 << 5,
    "climb": 1 << 6,
    "loop": 1 << 7,
}

ENTITY_KINDS = {
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
EVENT_TRIGGERS = {
    "entered_region": 1,
    "switch_on": 2,
    "enemy_group_clear": 3,
}
EVENT_CONDITIONS = {
    "always": 0,
    "has_key": 1,
    "rings_gte": 2,
    "switch_on": 3,
    "enemy_group_clear": 4,
}
EVENT_ACTIONS = {
    "open_door": 1,
    "toggle_block_group": 2,
    "spawn_group": 3,
    "play_effect": 4,
    "set_checkpoint": 5,
    "complete_level": 6,
}

_ID_RE = re.compile(r"^[A-Za-z0-9_-]{1,32}$")


class MakerLitePackError(ValueError):
    """Raised when source or binary data violates the pack contract."""


@dataclass(frozen=True)
class PackMetadata:
    title: str
    ruleset: str
    ruleset_id: int
    kit_id: str
    project_id: str
    view_width: int
    view_height: int
    map_width: int
    map_height: int
    tile_size: int
    entity_count: int
    event_count: int
    path_count: int
    path_point_count: int
    crc32: int
    flags: int


def _bounded_int(value, name, minimum, maximum):
    if isinstance(value, bool):
        raise MakerLitePackError(f"{name} must be an integer")
    try:
        converted = int(value)
    except (TypeError, ValueError) as error:
        raise MakerLitePackError(f"{name} must be an integer") from error
    if converted < minimum or converted > maximum:
        raise MakerLitePackError(
            f"{name} must be between {minimum} and {maximum}"
        )
    return converted


def _validated_id(value, name):
    value = str(value or "")
    if not _ID_RE.fullmatch(value):
        raise MakerLitePackError(
            f"{name} must contain 1-32 ASCII letters, digits, '-' or '_'"
        )
    return value


def _collision_value(names):
    if isinstance(names, str):
        names = [names]
    if not isinstance(names, list):
        raise MakerLitePackError("collision must be a name or list of names")
    value = 0
    for name in names:
        try:
            value |= COLLISION_FLAGS[str(name)]
        except KeyError as error:
            raise MakerLitePackError(
                f"unknown collision flag: {name}"
            ) from error
    return value


def _apply_terrain(project, width, height):
    cells = width * height
    tiles = [0] * cells
    collision = bytearray(cells)

    for index, operation in enumerate(project.get("terrain", [])):
        if not isinstance(operation, dict):
            raise MakerLitePackError(f"terrain[{index}] must be an object")
        tile = _bounded_int(
            operation.get("tile", 0),
            "tile",
            0,
            MAX_RENDER_CELL,
        )
        flags = _collision_value(operation.get("collision", []))
        if "rect" in operation:
            rect = operation["rect"]
            if not isinstance(rect, list) or len(rect) != 4:
                raise MakerLitePackError(
                    f"terrain[{index}].rect must be [x, y, width, height]"
                )
            x, y, rect_width, rect_height = [
                _bounded_int(value, "rectangle component", 0, 65535)
                for value in rect
            ]
        elif "point" in operation:
            point = operation["point"]
            if not isinstance(point, list) or len(point) != 2:
                raise MakerLitePackError(
                    f"terrain[{index}].point must be [x, y]"
                )
            x, y = [
                _bounded_int(value, "point component", 0, 65535)
                for value in point
            ]
            rect_width = rect_height = 1
        else:
            raise MakerLitePackError(
                f"terrain[{index}] needs rect or point"
            )

        if (
            rect_width <= 0
            or rect_height <= 0
            or x + rect_width > width
            or y + rect_height > height
        ):
            raise MakerLitePackError(
                f"terrain[{index}] is outside the {width}x{height} map"
            )
        for tile_y in range(y, y + rect_height):
            row = tile_y * width
            for tile_x in range(x, x + rect_width):
                tiles[row + tile_x] = tile
                collision[row + tile_x] = flags
    encoded_tiles = bytearray(cells * 2)
    for index, tile in enumerate(tiles):
        struct.pack_into("<H", encoded_tiles, index * 2, tile)
    return encoded_tiles, collision


def _pack_entities(project, width, height, tile_size, *, require_goal=True):
    source_entities = project.get("entities", [])
    if not isinstance(source_entities, list):
        raise MakerLitePackError("entities must be a list")
    if len(source_entities) > MAX_ENTITIES:
        raise MakerLitePackError(
            f"entity count exceeds {MAX_ENTITIES}"
        )

    output = bytearray()
    player_count = 0
    goal_count = 0
    for index, source in enumerate(source_entities):
        if not isinstance(source, dict):
            raise MakerLitePackError(f"entities[{index}] must be an object")
        kind_name = str(source.get("kind") or "")
        try:
            kind = ENTITY_KINDS[kind_name]
        except KeyError as error:
            raise MakerLitePackError(
                f"unknown entity kind: {kind_name}"
            ) from error
        if kind_name == "player":
            player_count += 1
        elif kind_name == "goal":
            goal_count += 1

        x = _bounded_int(
            source.get("x"), f"entities[{index}].x",
            0, width * tile_size - 1,
        )
        y = _bounded_int(
            source.get("y"), f"entities[{index}].y",
            0, height * tile_size + 32,
        )
        flags = _bounded_int(
            source.get("flags", 0), f"entities[{index}].flags", 0, 65535
        )
        params = list(source.get("params") or [])
        if len(params) > 4:
            raise MakerLitePackError(
                f"entities[{index}].params has more than four values"
            )
        params.extend([0] * (4 - len(params)))
        params = [
            _bounded_int(
                value, f"entities[{index}].params[{param_index}]",
                -32768, 32767,
            )
            for param_index, value in enumerate(params)
        ]
        render_cell = _bounded_int(
            source.get("render_cell", 0),
            f"entities[{index}].render_cell",
            0,
            MAX_RENDER_CELL,
        )
        output.extend(
            struct.pack(
                "<HHhhhhhhH",
                kind,
                flags,
                x,
                y,
                *params,
                render_cell,
            )
        )

    if player_count != 1:
        raise MakerLitePackError("project must contain exactly one player")
    if require_goal and goal_count < 1:
        raise MakerLitePackError("project must contain a completion goal")
    return output


def _pack_events(project, width, height, tile_size):
    events = project.get("events", [])
    if not isinstance(events, list):
        raise MakerLitePackError("events must be a list")
    if len(events) > MAX_EVENTS:
        raise MakerLitePackError(f"event count exceeds {MAX_EVENTS}")
    output = bytearray()
    for index, source in enumerate(events):
        if not isinstance(source, dict):
            raise MakerLitePackError(f"events[{index}] must be an object")
        try:
            trigger = EVENT_TRIGGERS[str(source.get("trigger", ""))]
            condition = EVENT_CONDITIONS[str(source.get("condition", "always"))]
            action = EVENT_ACTIONS[str(source.get("action", ""))]
        except KeyError as error:
            raise MakerLitePackError(
                f"events[{index}] has an unknown trigger, condition, or action"
            ) from error
        region = source.get("region", [0, 0, 0, 0])
        if not isinstance(region, list) or len(region) != 4:
            raise MakerLitePackError(
                f"events[{index}].region must be [x, y, width, height]"
            )
        x, y, region_width, region_height = [
            _bounded_int(value, f"events[{index}].region", 0, 32767)
            for value in region
        ]
        if trigger == EVENT_TRIGGERS["entered_region"]:
            if (
                region_width <= 0
                or region_height <= 0
                or x + region_width > width * tile_size
                or y + region_height > height * tile_size
            ):
                raise MakerLitePackError(
                    f"events[{index}] trigger region is outside the map"
                )
        params = source.get("params", [0, 0])
        if not isinstance(params, list) or len(params) != 2:
            raise MakerLitePackError(
                f"events[{index}].params must contain two integers"
            )
        param0, param1 = [
            _bounded_int(value, f"events[{index}].params", -32768, 32767)
            for value in params
        ]
        subject = _bounded_int(
            source.get("subject", 0), f"events[{index}].subject", 0, 255
        )
        value = _bounded_int(
            source.get("value", 0), f"events[{index}].value", -32768, 32767
        )
        delay = _bounded_int(
            source.get("delay", 0), f"events[{index}].delay", 0, 65535
        )
        one_shot = bool(source.get("one_shot", True))
        if condition == EVENT_CONDITIONS["rings_gte"] and value < 0:
            raise MakerLitePackError(
                f"events[{index}] ring threshold cannot be negative"
            )
        if action == EVENT_ACTIONS["play_effect"] and not 0 <= param0 <= 5:
            raise MakerLitePackError(
                f"events[{index}] effect index must be from 0 through 5"
            )
        if action == EVENT_ACTIONS["set_checkpoint"] and (
            param0 < 0
            or param1 < 0
            or param0 >= width * tile_size
            or param1 > height * tile_size + 32
        ):
            raise MakerLitePackError(
                f"events[{index}] checkpoint is outside the map"
            )
        if (
            trigger == EVENT_TRIGGERS["enemy_group_clear"]
            and action == EVENT_ACTIONS["spawn_group"]
            and not one_shot
            and delay == 0
        ):
            raise MakerLitePackError(
                f"events[{index}] creates an undelayed event cycle"
            )
        output.extend(
            struct.pack(
                "<BBBBHhhhhhhhH2x",
                trigger,
                condition,
                action,
                1 if one_shot else 0,
                subject,
                value,
                x,
                y,
                region_width,
                region_height,
                param0,
                param1,
                delay,
            )
        )
    return output


def _pack_paths(project, width, height, tile_size):
    paths = project.get("paths", [])
    if not isinstance(paths, list):
        raise MakerLitePackError("paths must be a list")
    if len(paths) > MAX_PATHS:
        raise MakerLitePackError(f"path count exceeds {MAX_PATHS}")
    headers = bytearray()
    points = bytearray()
    point_count = 0
    ids = set()
    for index, source in enumerate(paths):
        if not isinstance(source, dict):
            raise MakerLitePackError(f"paths[{index}] must be an object")
        path_id = _bounded_int(
            source.get("id"), f"paths[{index}].id", 1, 65535
        )
        if path_id in ids:
            raise MakerLitePackError(f"duplicate path id: {path_id}")
        ids.add(path_id)
        source_points = source.get("points", [])
        if not isinstance(source_points, list) or len(source_points) < 2:
            raise MakerLitePackError(
                f"paths[{index}] must contain at least two points"
            )
        if point_count + len(source_points) > MAX_PATH_POINTS:
            raise MakerLitePackError(
                f"path point count exceeds {MAX_PATH_POINTS}"
            )
        start = point_count
        for point_index, point in enumerate(source_points):
            if not isinstance(point, list) or len(point) != 2:
                raise MakerLitePackError(
                    f"paths[{index}].points[{point_index}] must be [x, y]"
                )
            point_x = _bounded_int(
                point[0], f"paths[{index}].points[{point_index}].x",
                0, width * tile_size - 1,
            )
            point_y = _bounded_int(
                point[1], f"paths[{index}].points[{point_index}].y",
                0, height * tile_size + 32,
            )
            points.extend(struct.pack("<hh", point_x, point_y))
            point_count += 1
        speed = _bounded_int(
            source.get("speed", 64), f"paths[{index}].speed", 1, 4096
        )
        flags = 1 if source.get("ping_pong", True) else 0
        if source.get("surface", False):
            flags |= 2
        headers.extend(
            struct.pack(
                "<HHHHhh",
                path_id,
                flags,
                start,
                len(source_points),
                speed,
                0,
            )
        )
    return headers, points


def _expand_zelda_rooms(
    project, ruleset, width, height, tile_size, view_width, view_height
):
    rooms = project.get("rooms", [])
    links = project.get("room_links", [])
    if not rooms and not links:
        return project
    if ruleset != "zelda":
        raise MakerLitePackError("rooms and room_links are Zelda-only")
    if not isinstance(rooms, list) or not 1 <= len(rooms) <= 64:
        raise MakerLitePackError("Zelda projects must contain 1-64 rooms")
    if not isinstance(links, list):
        raise MakerLitePackError("room_links must be a list")
    room_by_id = {}
    occupied = []
    expected_width = view_width // tile_size
    expected_height = view_height // tile_size
    for index, room in enumerate(rooms):
        if not isinstance(room, dict):
            raise MakerLitePackError(f"rooms[{index}] must be an object")
        room_id = _validated_id(room.get("id"), f"rooms[{index}].id")
        if room_id in room_by_id:
            raise MakerLitePackError(f"duplicate room id: {room_id}")
        rect = room.get("rect")
        if not isinstance(rect, list) or len(rect) != 4:
            raise MakerLitePackError(
                f"rooms[{index}].rect must be [x, y, width, height]"
            )
        x, y, room_width, room_height = [
            _bounded_int(value, f"rooms[{index}].rect", 0, 512)
            for value in rect
        ]
        if (
            room_width != expected_width
            or room_height != expected_height
            or x + room_width > width
            or y + room_height > height
        ):
            raise MakerLitePackError(
                f"rooms[{index}] must be one {view_width}x{view_height} "
                "camera frame inside the map"
            )
        for other_x, other_y, other_width, other_height in occupied:
            if (
                x < other_x + other_width
                and x + room_width > other_x
                and y < other_y + other_height
                and y + room_height > other_y
            ):
                raise MakerLitePackError("Zelda room rectangles may not overlap")
        occupied.append((x, y, room_width, room_height))
        room_by_id[room_id] = (x, y, room_width, room_height)

    expanded = copy.deepcopy(project)
    entities = expanded.setdefault("entities", [])
    link_ids = set()

    def endpoint(link_index, field, room_id):
        point = links[link_index].get(field)
        if not isinstance(point, list) or len(point) != 2:
            raise MakerLitePackError(
                f"room_links[{link_index}].{field} must be [x, y]"
            )
        point_x = _bounded_int(
            point[0], f"room_links[{link_index}].{field}.x",
            0, width * tile_size - 1,
        )
        point_y = _bounded_int(
            point[1], f"room_links[{link_index}].{field}.y",
            0, height * tile_size + 32,
        )
        room_x, room_y, room_width, room_height = room_by_id[room_id]
        if not (
            room_x * tile_size <= point_x <
                (room_x + room_width) * tile_size
            and room_y * tile_size <= point_y <=
                (room_y + room_height) * tile_size
        ):
            raise MakerLitePackError(
                f"room_links[{link_index}].{field} is outside room {room_id}"
            )
        return point_x, point_y

    for index, link in enumerate(links):
        if not isinstance(link, dict):
            raise MakerLitePackError(f"room_links[{index}] must be an object")
        link_id = _validated_id(
            link.get("id"), f"room_links[{index}].id"
        )
        if link_id in link_ids:
            raise MakerLitePackError(f"duplicate room link id: {link_id}")
        link_ids.add(link_id)
        from_room = str(link.get("from_room", ""))
        to_room = str(link.get("to_room", ""))
        if from_room == to_room or from_room not in room_by_id or to_room not in room_by_id:
            raise MakerLitePackError(
                f"room_links[{index}] must join two existing distinct rooms"
            )
        if link.get("reciprocal", True) is not True:
            raise MakerLitePackError(
                f"room_links[{index}] must be reciprocal"
            )
        from_x, from_y = endpoint(index, "from_position", from_room)
        to_x, to_y = endpoint(index, "to_position", to_room)
        flags = (index + 1) & 0xFF
        if link.get("locked", False):
            flags |= 0x100
        entities.extend(
            [
                {
                    "kind": "door",
                    "flags": flags,
                    "x": from_x,
                    "y": from_y,
                    "params": [16, 24, to_x, to_y],
                },
                {
                    "kind": "door",
                    "flags": flags,
                    "x": to_x,
                    "y": to_y,
                    "params": [16, 24, from_x, from_y],
                },
            ]
        )
    return expanded


def validate_project(project):
    """Validate and normalize enough source data to compile a device pack."""
    if not isinstance(project, dict):
        raise MakerLitePackError("project root must be an object")
    if int(project.get("format_version", 1)) != 1:
        raise MakerLitePackError("unsupported source project version")

    ruleset = str(project.get("ruleset") or "").lower()
    if ruleset not in RULESETS:
        raise MakerLitePackError(
            f"ruleset must be one of {', '.join(RULESETS)}"
        )
    title = str(project.get("title") or "").strip()
    encoded_title = title.encode("utf-8")
    if not encoded_title or len(encoded_title) > MAX_TITLE_BYTES:
        raise MakerLitePackError(
            f"title must contain 1-{MAX_TITLE_BYTES} UTF-8 bytes"
        )
    kit_id = _validated_id(project.get("kit_id"), "kit_id")
    project_id = _validated_id(project.get("project_id"), "project_id")
    gameplay = str(project.get("gameplay", "level")).strip().lower()
    if gameplay not in {"level", "life_sim", "brawl"}:
        raise MakerLitePackError("gameplay must be level, life_sim, or brawl")
    if gameplay == "life_sim" and ruleset != "zelda":
        raise MakerLitePackError("life_sim gameplay requires the Zelda ruleset")
    flags = (
        LEVEL_LIFE_SIM
        if gameplay == "life_sim"
        else LEVEL_BRAWL if gameplay == "brawl" else 0
    )

    size = project.get("size")
    if not isinstance(size, list) or len(size) != 2:
        raise MakerLitePackError("size must be [width, height]")
    width = _bounded_int(size[0], "map width", 1, MAX_MAP_WIDTH)
    height = _bounded_int(size[1], "map height", 1, MAX_MAP_HEIGHT)
    tile_size = _bounded_int(project.get("tile_size", 16), "tile_size", 16, 16)

    default_view = [320, 224] if ruleset == "sonic" else [256, 224]
    view = project.get("view", default_view)
    if not isinstance(view, list) or len(view) != 2:
        raise MakerLitePackError("view must be [width, height]")
    view_width = _bounded_int(view[0], "view width", 1, 320)
    view_height = _bounded_int(view[1], "view height", 1, 240)
    if view_width > width * tile_size or view_height > height * tile_size:
        raise MakerLitePackError("view cannot be larger than the map")

    project = _expand_zelda_rooms(
        project, ruleset, width, height, tile_size, view_width, view_height
    )
    tiles, collision = _apply_terrain(project, width, height)
    events = _pack_events(project, width, height, tile_size)
    paths, path_points = _pack_paths(project, width, height, tile_size)
    path_ids = {
        int(path.get("id"))
        for path in project.get("paths", [])
        if isinstance(path, dict)
    }
    for index, entity in enumerate(project.get("entities", [])):
        if not isinstance(entity, dict) or entity.get("kind") != "block":
            continue
        params = entity.get("params") or []
        path_id = int(params[3]) if len(params) > 3 else 0
        if path_id and path_id not in path_ids:
            raise MakerLitePackError(
                f"entities[{index}] refers to missing path {path_id}"
            )
    entities = _pack_entities(
        project,
        width,
        height,
        tile_size,
        require_goal=not (flags & (LEVEL_LIFE_SIM | LEVEL_BRAWL)),
    )
    if flags & LEVEL_BRAWL:
        opponents = [
            entity
            for entity in project.get("entities", [])
            if isinstance(entity, dict) and entity.get("kind") == "enemy"
        ]
        if len(opponents) != 1:
            raise MakerLitePackError(
                "brawl gameplay requires exactly one enemy fighter"
            )
    return {
        "title": title,
        "title_bytes": encoded_title,
        "ruleset": ruleset,
        "ruleset_id": RULESETS[ruleset],
        "flags": flags,
        "kit_id": kit_id,
        "project_id": project_id,
        "width": width,
        "height": height,
        "tile_size": tile_size,
        "view_width": view_width,
        "view_height": view_height,
        "tiles": tiles,
        "collision": collision,
        "entities": entities,
        "entity_count": len(entities) // PACK_ENTITY_SIZE,
        "events": events,
        "event_count": len(events) // PACK_EVENT_SIZE,
        "paths": paths,
        "path_count": len(paths) // PACK_PATH_SIZE,
        "path_points": path_points,
        "path_point_count": len(path_points) // PACK_PATH_POINT_SIZE,
    }


def compile_project(project):
    """Compile a validated source project into a deterministic RPML pack."""
    normalized = validate_project(project)
    tiles = normalized["tiles"]
    collision = normalized["collision"]
    entities = normalized["entities"]
    title = normalized["title_bytes"]
    events = normalized["events"]
    paths = normalized["paths"]
    path_points = normalized["path_points"]

    tiles_offset = PACK_HEADER_SIZE
    collision_offset = tiles_offset + len(tiles)
    entities_offset = collision_offset + len(collision)
    title_offset = entities_offset + len(entities)
    events_offset = title_offset + len(title)
    paths_offset = events_offset + len(events)
    path_points_offset = paths_offset + len(paths)
    payload = bytes(
        tiles + collision + entities + title + events + paths + path_points
    )
    header = bytearray(PACK_HEADER_SIZE)
    header[0:4] = PACK_MAGIC
    struct.pack_into(
        "<HBBHHHHHHIIIIH2xI",
        header,
        4,
        PACK_VERSION,
        normalized["ruleset_id"],
        normalized["flags"],
        normalized["view_width"],
        normalized["view_height"],
        normalized["width"],
        normalized["height"],
        normalized["tile_size"],
        normalized["entity_count"],
        tiles_offset,
        collision_offset,
        entities_offset,
        title_offset,
        len(title),
        zlib.crc32(payload) & 0xFFFFFFFF,
    )
    header[44:44 + ID_SIZE] = normalized["kit_id"].encode("ascii").ljust(
        ID_SIZE, b"\0"
    )
    header[76:76 + ID_SIZE] = normalized["project_id"].encode("ascii").ljust(
        ID_SIZE, b"\0"
    )
    struct.pack_into(
        "<IHHIIH",
        header,
        108,
        events_offset,
        normalized["event_count"],
        normalized["path_count"],
        paths_offset,
        path_points_offset,
        normalized["path_point_count"],
    )
    return bytes(header) + payload


def parse_pack(data):
    """Parse pack metadata and perform the same structural checks as C."""
    if len(data) < PACK_HEADER_SIZE:
        raise MakerLitePackError("pack is too small")
    if data[:4] != PACK_MAGIC:
        raise MakerLitePackError("invalid pack magic")
    (
        version,
        ruleset_id,
        flags,
        view_width,
        view_height,
        width,
        height,
        tile_size,
        entity_count,
        tiles_offset,
        collision_offset,
        entities_offset,
        title_offset,
        title_length,
        expected_crc,
    ) = struct.unpack_from("<HBBHHHHHHIIIIH2xI", data, 4)
    if version not in {1, 2, PACK_VERSION}:
        raise MakerLitePackError("unsupported pack version")
    entity_size = PACK_ENTITY_V1_SIZE if version == 1 else PACK_ENTITY_SIZE
    tile_entry_size = 2 if version >= 3 else 1
    reverse_rulesets = {value: key for key, value in RULESETS.items()}
    if ruleset_id not in reverse_rulesets:
        raise MakerLitePackError("unsupported ruleset")
    if (
        not 1 <= width <= MAX_MAP_WIDTH
        or not 1 <= height <= MAX_MAP_HEIGHT
        or tile_size != 16
        or entity_count > MAX_ENTITIES
    ):
        raise MakerLitePackError("invalid dimensions or entity count")

    cells = width * height
    ranges = (
        (tiles_offset, cells * tile_entry_size),
        (collision_offset, cells),
        (entities_offset, entity_count * entity_size),
        (title_offset, title_length),
    )
    events_offset, event_count, path_count, paths_offset, path_points_offset, (
        path_point_count
    ) = struct.unpack_from("<IHHIIH", data, 108)
    if (
        event_count > MAX_EVENTS
        or path_count > MAX_PATHS
        or path_point_count > MAX_PATH_POINTS
    ):
        raise MakerLitePackError("invalid event or path count")
    ranges += (
        (events_offset, event_count * PACK_EVENT_SIZE),
        (paths_offset, path_count * PACK_PATH_SIZE),
        (path_points_offset, path_point_count * PACK_PATH_POINT_SIZE),
    )
    for offset, length in ranges:
        if (
            offset < PACK_HEADER_SIZE
            or offset > len(data)
            or length > len(data) - offset
        ):
            raise MakerLitePackError("invalid pack offset")
    actual_crc = zlib.crc32(data[PACK_HEADER_SIZE:]) & 0xFFFFFFFF
    if actual_crc != expected_crc:
        raise MakerLitePackError("pack checksum mismatch")

    def decode_id(offset):
        raw = data[offset:offset + ID_SIZE].split(b"\0", 1)[0]
        try:
            value = raw.decode("ascii")
        except UnicodeDecodeError as error:
            raise MakerLitePackError("invalid pack id") from error
        return _validated_id(value, "pack id")

    try:
        title = data[title_offset:title_offset + title_length].decode("utf-8")
    except UnicodeDecodeError as error:
        raise MakerLitePackError("invalid UTF-8 title") from error
    return PackMetadata(
        title=title,
        ruleset=reverse_rulesets[ruleset_id],
        ruleset_id=ruleset_id,
        kit_id=decode_id(44),
        project_id=decode_id(76),
        view_width=view_width,
        view_height=view_height,
        map_width=width,
        map_height=height,
        tile_size=tile_size,
        entity_count=entity_count,
        event_count=event_count,
        path_count=path_count,
        path_point_count=path_point_count,
        crc32=expected_crc,
        flags=flags,
    )


def load_project(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def compile_project_file(source_path, output_path):
    source_path = Path(source_path)
    output_path = Path(output_path)
    data = compile_project(load_project(source_path))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = output_path.with_name(output_path.name + ".tmp")
    with open(temporary, "wb") as handle:
        handle.write(data)
        handle.flush()
    temporary.replace(output_path)
    return parse_pack(data)
