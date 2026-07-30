"""Maker Lite project persistence, validation, and autosave recovery."""

from __future__ import annotations

import copy
import json
import os
import tempfile
import uuid
from dataclasses import dataclass

from services.maker_lite_pack import (
    MakerLitePackError,
    compile_project,
    validate_project,
)


class MakerLiteProjectError(ValueError):
    pass


GAME_TYPES = {
    "mario": (
        ("classic", "Classic Course"),
        ("obstacle", "Obstacle Course"),
        ("speedrun", "Checkpoint Sprint"),
        ("brawl", "Maker Brawl Arena"),
    ),
    "zelda": (
        ("adventure", "Overworld Adventure"),
        ("dungeon", "Key & Door Dungeon"),
        ("puzzle", "Switch Puzzle"),
        ("brawl", "Maker Brawl Arena"),
    ),
    "sonic": (
        ("race", "High-Speed Race"),
        ("rings", "Ring Challenge"),
        ("stunt", "Spring Stunt Course"),
        ("brawl", "Maker Brawl Arena"),
    ),
}


def _entity(kind, x, y, params=None, flags=0):
    value = {"kind": kind, "x": x, "y": y}
    if params is not None:
        value["params"] = list(params)
    if flags:
        value["flags"] = flags
    return value


def apply_game_type_template(source: dict, game_type: str) -> dict:
    """Apply a bounded, runtime-playable starter layout to a new project."""

    ruleset = str(source.get("ruleset", "")).lower()
    available = dict(GAME_TYPES.get(ruleset, ()))
    if game_type not in available:
        raise MakerLiteProjectError(
            f"Unknown {ruleset.title()} game type: {game_type}"
        )
    width, height = map(int, source["size"])
    ground_y = height - 2
    source["game_type"] = game_type
    source["title"] = available[game_type]
    source["metadata"]["description"] = (
        f"A {available[game_type].lower()} made with Maker Lite."
    )

    if game_type == "brawl":
        source["gameplay"] = "brawl"
        source["view"] = [320, 224]
        source["size"] = [24, 15]
        source["terrain"] = [
            {"rect": [2, 12, 20, 2], "tile": 1, "collision": "solid"},
            {"rect": [5, 8, 5, 1], "tile": 1, "collision": "one_way"},
            {"rect": [14, 8, 5, 1], "tile": 1, "collision": "one_way"},
            {"rect": [10, 5, 4, 1], "tile": 1, "collision": "one_way"},
        ]
        source["entities"] = [
            _entity("player", 6 * 16, 12 * 16),
            _entity("enemy", 18 * 16, 12 * 16, [14, 26, 100, 3], 0x400),
        ]
        source["events"] = []
        source["paths"] = []
        source.pop("rooms", None)
        source.pop("room_links", None)
        source["metadata"].update(
            {
                "genre": "Platform fighter",
                "description": (
                    "A three-stock one-on-one platform battle against CPU."
                ),
                "brawl_player": f"{ruleset}-guest",
                "brawl_opponent": "original-guest",
            }
        )
    elif ruleset == "mario" and game_type == "obstacle":
        source["terrain"].extend(
            [
                {"rect": [7, ground_y - 3, 5, 1], "tile": 1,
                 "collision": "solid"},
                {"rect": [17, ground_y - 5, 6, 1], "tile": 1,
                 "collision": "solid"},
                {"rect": [28, ground_y - 2, 5, 1], "tile": 1,
                 "collision": "solid"},
            ]
        )
        source["entities"].extend(
            [
                _entity("enemy", 10 * 16 + 8, (ground_y - 3) * 16),
                _entity("enemy", 20 * 16 + 8, (ground_y - 5) * 16),
                _entity("spring", 26 * 16 + 8, ground_y * 16,
                        [16, 16, 10, 0]),
            ]
        )
    elif ruleset == "mario" and game_type == "speedrun":
        for tile_x in (10, 20, 30):
            source["entities"].append(
                _entity("checkpoint", tile_x * 16 + 8, ground_y * 16,
                        [16, 32, 0, 0])
            )
        source["metadata"]["description"] = (
            "A checkpoint sprint that records the fastest completion."
        )
    elif ruleset == "zelda" and game_type == "dungeon":
        source["entities"].extend(
            [
                _entity("key", 5 * 16 + 8, 6 * 16 + 16),
                _entity("door", 11 * 16 + 8, 6 * 16 + 16,
                        [16, 16, 0, 0], 1),
                _entity("enemy", 7 * 16 + 8, 5 * 16 + 16),
            ]
        )
        source["events"].append(
            {
                "trigger": "entered_region",
                "condition": "has_key",
                "action": "open_door",
                "subject": 1,
                "value": 1,
                "region": [10 * 16, 5 * 16, 2 * 16, 3 * 16],
                "params": [0, 0],
                "one_shot": True,
            }
        )
    elif ruleset == "zelda" and game_type == "puzzle":
        source["entities"].extend(
            [
                _entity("switch", 5 * 16 + 8, 6 * 16 + 16),
                _entity("block", 9 * 16 + 8, 6 * 16 + 16,
                        [16, 16, 0, 0], 1),
                _entity("pot", 4 * 16 + 8, 9 * 16 + 16),
            ]
        )
        source["events"].append(
            {
                "trigger": "switch_on",
                "condition": "always",
                "action": "toggle_block_group",
                "subject": 1,
                "region": [0, 0, 0, 0],
                "params": [0, 0],
                "one_shot": True,
            }
        )
    elif ruleset == "sonic" and game_type == "rings":
        for tile_x in range(6, width - 5, 3):
            source["entities"].append(
                _entity("collectible", tile_x * 16 + 8,
                        (ground_y - 2 - tile_x % 2) * 16,
                        [12, 12, 100, 0])
            )
        source["metadata"]["description"] = (
            "Collect the ring trail, then reach the goal as fast as possible."
        )
    elif ruleset == "sonic" and game_type == "stunt":
        source["entities"].extend(
            _entity("spring", tile_x * 16 + 8, ground_y * 16,
                    [16, 16, strength, 0])
            for tile_x, strength in ((8, 8), (18, 10), (29, 12))
        )
        source["terrain"].extend(
            [
                {"rect": [11, ground_y - 4, 6, 1], "tile": 1,
                 "collision": "one_way"},
                {"rect": [22, ground_y - 7, 6, 1], "tile": 1,
                 "collision": "one_way"},
            ]
        )
    return source


def _atomic_json(path: str, data: dict) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".maker-lite-", dir=os.path.dirname(path))
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as output:
            json.dump(data, output, indent=2, sort_keys=True)
            output.write("\n")
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
    except Exception:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def project_template(
    ruleset: str,
    kit_id: str,
    title: str = "",
    game_type: str | None = None,
) -> dict:
    ruleset = ruleset.lower()
    if ruleset not in {"mario", "zelda", "sonic"}:
        raise MakerLiteProjectError("Unknown Maker Lite ruleset")
    project_id = uuid.uuid4().hex
    width = 16 if ruleset == "zelda" else 40
    height = 14 if ruleset == "zelda" else 15
    ground_y = height - 2
    source = {
        "format_version": 1,
        "project_id": project_id,
        "kit_id": kit_id,
        "ruleset": ruleset,
        "title": title.strip() or f"New {ruleset.title()} Level",
        "view": [320, 224] if ruleset == "sonic" else [256, 224],
        "size": [width, height],
        "tile_size": 16,
        "terrain": [{"rect": [0, ground_y, width, height - ground_y], "tile": 1,
                     "collision": "solid"}],
        "entities": [
            {"kind": "player", "x": 32, "y": ground_y * 16},
            {"kind": "goal", "x": (width - 2) * 16, "y": ground_y * 16,
             "params": [16, 32, 0, 0]},
        ],
        "events": [],
        "paths": [],
        "metadata": {
            "show_in_steam": False,
            "cover_source": "",
            "cover_fit": "contain",
            "cover_background": "#000000",
            "genre": "Platformer" if ruleset != "zelda" else "Action adventure",
            "author": "",
            "description": "",
        },
    }
    if ruleset == "zelda":
        source["rooms"] = [
            {
                "id": "room-1",
                "title": "Room 1",
                "rect": [0, 0, width, height],
            }
        ]
        source["room_links"] = []
    if game_type is not None:
        apply_game_type_template(source, game_type)
        if title.strip():
            source["title"] = title.strip()
    return source


@dataclass
class ProjectRecord:
    project_id: str
    directory: str
    source: dict
    recovered: bool = False


class MakerLiteProjectStore:
    def __init__(self, private_root: str):
        self.root = os.path.abspath(private_root)
        self.projects_root = os.path.join(self.root, "projects")

    def list(self) -> list[ProjectRecord]:
        records = []
        if not os.path.isdir(self.projects_root):
            return records
        for entry in sorted(os.scandir(self.projects_root), key=lambda item: item.name):
            if not entry.is_dir():
                continue
            try:
                records.append(self.load(entry.name))
            except (OSError, ValueError):
                continue
        return records

    def create(
        self,
        ruleset: str,
        kit_id: str,
        title: str = "",
        game_type: str | None = None,
    ) -> ProjectRecord:
        source = project_template(ruleset, kit_id, title, game_type)
        record = ProjectRecord(source["project_id"],
                               os.path.join(self.projects_root, source["project_id"]),
                               source)
        self.save(record)
        return record

    def import_file(self, path: str) -> ProjectRecord:
        with open(path, "r", encoding="utf-8") as source:
            data = json.load(source)
        validate_project(data)
        project_id = data["project_id"]
        if os.path.exists(os.path.join(self.projects_root, project_id)):
            data = copy.deepcopy(data)
            data["project_id"] = uuid.uuid4().hex
            project_id = data["project_id"]
        record = ProjectRecord(
            project_id,
            os.path.join(self.projects_root, project_id),
            data,
        )
        self.save(record)
        return record

    def load(self, project_id: str) -> ProjectRecord:
        directory = os.path.join(self.projects_root, os.path.basename(project_id))
        primary = os.path.join(directory, "project.json")
        autosave = os.path.join(directory, "autosave", "project.json")
        recovered = False
        try:
            with open(primary, "r", encoding="utf-8") as source:
                data = json.load(source)
        except (OSError, ValueError):
            with open(autosave, "r", encoding="utf-8") as source:
                data = json.load(source)
            recovered = True
        if data.get("project_id") != project_id:
            raise MakerLiteProjectError("Project directory and project_id differ")
        validate_project(data)
        return ProjectRecord(project_id, directory, data, recovered)

    def save(self, record: ProjectRecord) -> None:
        validate_project(record.source)
        _atomic_json(os.path.join(record.directory, "project.json"), record.source)
        record.recovered = False

    def autosave(self, record: ProjectRecord) -> None:
        validate_project(record.source)
        _atomic_json(
            os.path.join(record.directory, "autosave", "project.json"),
            record.source,
        )

    def compile(self, record: ProjectRecord) -> bytes:
        try:
            return compile_project(record.source)
        except MakerLitePackError as exc:
            raise MakerLiteProjectError(str(exc)) from exc

    @staticmethod
    def snapshot(record: ProjectRecord) -> dict:
        return copy.deepcopy(record.source)
