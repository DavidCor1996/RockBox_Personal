import copy
import struct
import sys
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "rockpod"))

from services.maker_lite_pack import (  # noqa: E402
    LEVEL_LIFE_SIM,
    MakerLitePackError,
    compile_project,
    load_project,
    parse_pack,
)


FIXTURES = ROOT / "testdata" / "maker_lite"


@pytest.mark.parametrize(
    ("filename", "ruleset"),
    [
        ("mario-test.json", "mario"),
        ("zelda-test.json", "zelda"),
        ("sonic-test.json", "sonic"),
    ],
)
def test_reference_projects_compile_deterministically(filename, ruleset):
    project = load_project(FIXTURES / filename)
    first = compile_project(project)
    second = compile_project(project)

    assert first == second
    metadata = parse_pack(first)
    assert metadata.ruleset == ruleset
    assert metadata.title == project["title"]
    expected_entities = len(project["entities"]) + (
        2 * len(project.get("room_links", []))
    )
    assert metadata.entity_count == expected_entities
    assert metadata.event_count == len(project.get("events", []))
    assert metadata.path_count == len(project.get("paths", []))
    assert metadata.path_point_count == sum(
        len(path["points"]) for path in project.get("paths", [])
    )


def test_entity_render_cell_is_preserved_in_pack_v3():
    project = load_project(FIXTURES / "mario-test.json")
    enemy = next(
        entity for entity in project["entities"] if entity["kind"] == "enemy"
    )
    enemy["render_cell"] = 777
    data = compile_project(project)
    assert struct.unpack_from("<H", data, 4)[0] == 3
    entity_count = struct.unpack_from("<H", data, 18)[0]
    entities_offset = struct.unpack_from("<I", data, 28)[0]
    decoded = [
        struct.unpack_from("<HHhhhhhhH", data, entities_offset + index * 18)
        for index in range(entity_count)
    ]
    enemy_record = next(record for record in decoded if record[0] == 4)
    assert enemy_record[-1] == 777


def test_entity_render_cell_obeys_art_budget():
    project = load_project(FIXTURES / "mario-test.json")
    project["entities"][0]["render_cell"] = 1536
    with pytest.raises(MakerLitePackError, match="render_cell"):
        compile_project(project)


def test_pack_v3_preserves_16_bit_terrain_cells():
    project = load_project(FIXTURES / "sonic-test.json")
    project["terrain"].append(
        {"point": [1, 1], "tile": 1200, "collision": ["solid"]}
    )
    data = compile_project(project)
    tiles_offset = struct.unpack_from("<I", data, 20)[0]
    width = project["size"][0]
    assert struct.unpack_from(
        "<H",
        data,
        tiles_offset + (width + 1) * 2,
    )[0] == 1200
    assert parse_pack(data).ruleset == "sonic"


def test_corrupt_payload_is_rejected():
    data = bytearray(compile_project(load_project(FIXTURES / "mario-test.json")))
    data[-1] ^= 0x55

    with pytest.raises(MakerLitePackError, match="checksum"):
        parse_pack(data)


def test_unknown_collision_is_rejected():
    project = load_project(FIXTURES / "mario-test.json")
    project["terrain"][0]["collision"] = ["solid", "imaginary"]

    with pytest.raises(MakerLitePackError, match="unknown collision"):
        compile_project(project)


def test_project_requires_one_player_and_a_goal():
    project = load_project(FIXTURES / "zelda-test.json")
    project["entities"] = [
        entity for entity in project["entities"] if entity["kind"] != "goal"
    ]
    with pytest.raises(MakerLitePackError, match="completion goal"):
        compile_project(project)

    project = load_project(FIXTURES / "zelda-test.json")
    project["entities"].append(copy.deepcopy(project["entities"][0]))
    with pytest.raises(MakerLitePackError, match="exactly one player"):
        compile_project(project)


def test_life_sim_pack_is_zelda_only_and_open_ended():
    project = load_project(FIXTURES / "zelda-test.json")
    project["gameplay"] = "life_sim"
    project["entities"] = [
        entity for entity in project["entities"]
        if entity["kind"] not in {"goal", "enemy", "door"}
    ]
    project["rooms"] = []
    project["room_links"] = []
    project["entities"].extend(
        [
            {"kind": "house", "x": 80, "y": 112, "render_cell": 120},
            {"kind": "shop", "x": 112, "y": 112, "render_cell": 121},
            {"kind": "car", "x": 144, "y": 112, "render_cell": 122},
            {"kind": "npc", "x": 176, "y": 112, "render_cell": 123},
        ]
    )
    metadata = parse_pack(compile_project(project))
    assert metadata.flags & LEVEL_LIFE_SIM
    assert metadata.entity_count == len(project["entities"])

    project["ruleset"] = "mario"
    with pytest.raises(MakerLitePackError, match="requires the Zelda"):
        compile_project(project)


def test_project_rejects_out_of_bounds_terrain():
    project = load_project(FIXTURES / "sonic-test.json")
    project["terrain"].append(
        {"rect": [59, 0, 2, 1], "tile": 1, "collision": ["solid"]}
    )

    with pytest.raises(MakerLitePackError, match="outside"):
        compile_project(project)


def test_project_rejects_unsafe_event_cycle_and_bad_path():
    project = load_project(FIXTURES / "sonic-test.json")
    project["events"][0] = {
        "trigger": "enemy_group_clear",
        "condition": "always",
        "action": "spawn_group",
        "subject": 7,
        "one_shot": False,
        "delay": 0,
    }
    with pytest.raises(MakerLitePackError, match="event cycle"):
        compile_project(project)

    project = load_project(FIXTURES / "sonic-test.json")
    project["paths"][0]["points"] = [[0, 0], [9999, 0]]
    with pytest.raises(MakerLitePackError, match="between|outside"):
        compile_project(project)


def test_pack_rejects_malicious_event_offset():
    data = bytearray(compile_project(load_project(FIXTURES / "mario-test.json")))
    data[108:112] = (0xFFFFFFF0).to_bytes(4, "little")
    with pytest.raises(MakerLitePackError, match="offset"):
        parse_pack(data)


def test_zelda_rooms_compile_reciprocal_door_pairs():
    project = load_project(FIXTURES / "zelda-test.json")
    project["size"] = [32, 14]
    project["rooms"] = [
        {"id": "west", "rect": [0, 0, 16, 14]},
        {"id": "east", "rect": [16, 0, 16, 14]},
    ]
    project["room_links"] = [
        {
            "id": "west-east",
            "from_room": "west",
            "to_room": "east",
            "from_position": [232, 112],
            "to_position": [280, 112],
            "reciprocal": True,
            "locked": True,
        }
    ]
    metadata = parse_pack(compile_project(project))
    assert metadata.entity_count == len(project["entities"]) + 2

    project["room_links"][0]["reciprocal"] = False
    with pytest.raises(MakerLitePackError, match="reciprocal"):
        compile_project(project)
