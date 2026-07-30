"""Conservative importer for the documented Open Surge 0.6 ``.lev`` subset."""

from __future__ import annotations

import copy
import json
import re
import shlex
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class ImportDiagnostic:
    line: int
    severity: str
    message: str


# Early Maker Lite prototypes documented this non-upstream brace form. Keep it
# readable for user files made with those builds, while the command parser
# below is the authoritative Open Surge path.
_LEGACY_BRICK_RE = re.compile(
    r"^\s*brick\s*\{\s*id\s*=\s*(\d+)\s*;\s*x\s*=\s*(-?\d+)\s*;"
    r"\s*y\s*=\s*(-?\d+)\s*;\s*\}\s*$",
    re.IGNORECASE,
)
_LEGACY_ENTITY_RE = re.compile(
    r'^\s*entity\s*\{\s*name\s*=\s*"([^"]+)"\s*;\s*x\s*=\s*(-?\d+)'
    r"\s*;\s*y\s*=\s*(-?\d+)\s*;\s*\}\s*$",
    re.IGNORECASE,
)
_ENTITY_NAMES = {
    "ring": "collectible",
    "collectible": "collectible",
    "checkpoint": "checkpoint",
    "goal": "goal",
    "goal capsule": "goal",
    "spring": "spring",
    "yellow spring": "spring",
    "red spring": "spring",
    "enemy": "enemy",
    "basic enemy": "enemy",
}
_COLLISIONS = {
    "solid",
    "hazard",
    "one_way",
    "slope_up",
    "slope_down",
    "water",
    "climb",
    "loop",
}


def load_open_surge_brick_mapping(
    path: str | Path,
) -> dict[int, str | list[str]]:
    """Load a reviewed, asset-free Open Surge brick collision sidecar."""

    with open(path, "r", encoding="utf-8") as source:
        root = json.load(source)
    if not isinstance(root, dict) or root.get("format_version") != 1:
        raise ValueError("Open Surge brick mapping must use format_version 1")
    mapping = root.get("brick_collision")
    if not isinstance(mapping, dict) or len(mapping) > 1536:
        raise ValueError("brick_collision must contain at most 1536 entries")
    normalized: dict[int, str | list[str]] = {}
    for raw_id, raw_collision in mapping.items():
        try:
            brick_id = int(raw_id, 10)
        except (TypeError, ValueError) as error:
            raise ValueError(
                "brick collision IDs must be decimal integers"
            ) from error
        if not 0 <= brick_id <= 1535 or str(brick_id) != str(raw_id):
            raise ValueError(
                "brick collision IDs must be canonical values 0-1535"
            )
        collisions = (
            [raw_collision]
            if isinstance(raw_collision, str)
            else raw_collision
        )
        if (
            not isinstance(collisions, list)
            or any(
                not isinstance(value, str) or value not in _COLLISIONS
                for value in collisions
            )
            or len(set(collisions)) != len(collisions)
        ):
            raise ValueError(
                f"brick {brick_id} has an unknown or duplicate collision"
            )
        normalized[brick_id] = (
            collisions[0] if len(collisions) == 1 else list(collisions)
        )
    return normalized


def _tokens(line: str) -> list[str]:
    lexer = shlex.shlex(line, posix=True)
    lexer.whitespace_split = True
    lexer.commenters = "#"
    return list(lexer)


def _integer(value: str, line: int, label: str) -> int:
    try:
        return int(value, 10)
    except ValueError as error:
        raise ValueError(f"line {line}: {label} must be an integer") from error


def import_open_surge_subset(
    text: str,
    project: dict,
    *,
    brick_collision: dict[int, str | list[str]] | None = None,
) -> tuple[dict, list[ImportDiagnostic]]:
    """Import bounded, explicit level commands without interpreting scripts.

    Supported upstream commands are ``name``, ``author``, ``spawn_point``,
    ``brick`` and a small allowlist of ``entity`` names. Brick placement must
    be on Maker Lite's exact 16-pixel grid. Because collision belongs to the
    referenced Open Surge brickset rather than the level line, callers may
    supply a reviewed ID-to-collision map. Unmapped bricks are decorative and
    produce a warning instead of being guessed solid.
    """

    imported = copy.deepcopy(project)
    terrain = imported.setdefault("terrain", [])
    entities = imported.setdefault("entities", [])
    metadata = imported.setdefault("metadata", {})
    diagnostics: list[ImportDiagnostic] = []
    width, height = map(int, imported["size"])
    tile_size = int(imported.get("tile_size", 16))
    collision_map = brick_collision or {}

    def add_brick(line_number: int, values: list[str]) -> None:
        if len(values) < 3 or len(values) > 5:
            diagnostics.append(
                ImportDiagnostic(
                    line_number,
                    "error",
                    "brick syntax is: brick ID X Y [layer] [flip]",
                )
            )
            return
        try:
            brick_id = _integer(values[0], line_number, "brick ID")
            pixel_x = _integer(values[1], line_number, "brick X")
            pixel_y = _integer(values[2], line_number, "brick Y")
        except ValueError as error:
            diagnostics.append(
                ImportDiagnostic(line_number, "error", str(error))
            )
            return
        if not 0 <= brick_id <= 1535:
            diagnostics.append(
                ImportDiagnostic(
                    line_number,
                    "error",
                    "brick ID exceeds Maker Lite's 0-1535 atlas index",
                )
            )
            return
        if pixel_x % tile_size or pixel_y % tile_size:
            diagnostics.append(
                ImportDiagnostic(
                    line_number,
                    "error",
                    "brick is not aligned to the exact 16-pixel Maker Lite grid",
                )
            )
            return
        tile_x, tile_y = pixel_x // tile_size, pixel_y // tile_size
        if not (0 <= tile_x < width and 0 <= tile_y < height):
            diagnostics.append(
                ImportDiagnostic(
                    line_number, "error", "brick is outside the project map"
                )
            )
            return
        collision = copy.deepcopy(collision_map.get(brick_id, []))
        if brick_id not in collision_map:
            diagnostics.append(
                ImportDiagnostic(
                    line_number,
                    "warning",
                    f"brick {brick_id} imported as decorative; map its "
                    "brickset collision explicitly",
                )
            )
        terrain.append(
            {
                "point": [tile_x, tile_y],
                "tile": brick_id,
                "collision": collision,
            }
        )
        optional = {value.casefold() for value in values[3:]}
        unsupported = optional - {
            "noflip",
        }
        if unsupported:
            diagnostics.append(
                ImportDiagnostic(
                    line_number,
                    "warning",
                    "Open Surge layer/flip metadata is not imported: "
                    + ", ".join(sorted(unsupported)),
                )
            )

    def add_entity(line_number: int, values: list[str]) -> None:
        if len(values) not in {3, 4}:
            diagnostics.append(
                ImportDiagnostic(
                    line_number,
                    "error",
                    'entity syntax is: entity "NAME" X Y [ID]',
                )
            )
            return
        source_name = values[0]
        try:
            pixel_x = _integer(values[1], line_number, "entity X")
            pixel_y = _integer(values[2], line_number, "entity Y")
        except ValueError as error:
            diagnostics.append(
                ImportDiagnostic(line_number, "error", str(error))
            )
            return
        kind = _ENTITY_NAMES.get(source_name.casefold())
        if not kind:
            diagnostics.append(
                ImportDiagnostic(
                    line_number,
                    "warning",
                    f"unsupported Open Surge entity: {source_name}",
                )
            )
            return
        if not (
            0 <= pixel_x < width * tile_size
            and 0 <= pixel_y <= height * tile_size + 32
        ):
            diagnostics.append(
                ImportDiagnostic(
                    line_number, "error", "entity is outside the project map"
                )
            )
            return
        params = {
            "collectible": [12, 12, 100, 0],
            "checkpoint": [16, 32, 0, 0],
            "spring": [16, 16, 8, 0],
            "enemy": [16, 16, 0, 0],
            "goal": [16, 48, 0, 0],
        }[kind]
        entities.append(
            {
                "kind": kind,
                "x": pixel_x,
                "y": pixel_y,
                "params": params,
            }
        )
        if len(values) == 4:
            diagnostics.append(
                ImportDiagnostic(
                    line_number,
                    "info",
                    "Open Surge entity ID is intentionally not carried into "
                    "the Maker Lite runtime",
                )
            )

    for line_number, line in enumerate(text.splitlines(), 1):
        stripped = line.strip()
        if not stripped or stripped.startswith("//") or stripped.startswith("#"):
            continue
        legacy_brick = _LEGACY_BRICK_RE.match(line)
        if legacy_brick:
            add_brick(line_number, list(legacy_brick.groups()))
            continue
        legacy_entity = _LEGACY_ENTITY_RE.match(line)
        if legacy_entity:
            add_entity(line_number, list(legacy_entity.groups()))
            continue
        try:
            tokens = _tokens(line)
        except ValueError as error:
            diagnostics.append(
                ImportDiagnostic(
                    line_number, "error", f"invalid quoting: {error}"
                )
            )
            continue
        if not tokens:
            continue
        command = tokens[0].casefold()
        values = tokens[1:]
        if command == "brick":
            add_brick(line_number, values)
        elif command == "entity":
            add_entity(line_number, values)
        elif command == "spawn_point" and len(values) == 2:
            try:
                x = _integer(values[0], line_number, "spawn X")
                y = _integer(values[1], line_number, "spawn Y")
            except ValueError as error:
                diagnostics.append(
                    ImportDiagnostic(line_number, "error", str(error))
                )
                continue
            if not (
                0 <= x < width * tile_size
                and 0 <= y <= height * tile_size + 32
            ):
                diagnostics.append(
                    ImportDiagnostic(
                        line_number, "error", "spawn point is outside the map"
                    )
                )
                continue
            player = next(
                (
                    entity
                    for entity in entities
                    if entity.get("kind") == "player"
                ),
                None,
            )
            if player is None:
                diagnostics.append(
                    ImportDiagnostic(
                        line_number, "error", "project has no player to move"
                    )
                )
            else:
                player["x"], player["y"] = x, y
        elif command == "name" and len(values) == 1:
            imported["title"] = values[0][:80]
        elif command == "author" and len(values) == 1:
            metadata["author"] = values[0][:80]
        elif command in {
            "requires", "act", "readonly", "theme", "bgtheme", "music",
            "setup", "players", "waterlevel", "watercolor", "license",
        }:
            diagnostics.append(
                ImportDiagnostic(
                    line_number,
                    "info",
                    f"Open Surge {command} metadata is not part of the "
                    "Maker Lite level subset",
                )
            )
        else:
            diagnostics.append(
                ImportDiagnostic(
                    line_number,
                    "warning",
                    "unsupported Open Surge syntax; no conversion was guessed",
                )
            )
    return imported, diagnostics
