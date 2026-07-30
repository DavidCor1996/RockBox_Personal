import json
import sys
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "rockpod"))

from services.maker_lite_runtime import MakerLiteRuntime  # noqa: E402
from services.maker_lite_projects import project_template  # noqa: E402


FIXTURES = ROOT / "testdata/maker_lite"


@pytest.mark.parametrize(
    "filename",
    ["mario-test.json", "zelda-test.json", "sonic-test.json"],
)
def test_live_preview_uses_deterministic_shared_core(
    tmp_path, filename
):
    source = json.loads((FIXTURES / filename).read_text(encoding="utf-8"))
    source["entities"][1]["render_cell"] = 777
    runtime = MakerLiteRuntime(str(ROOT), str(tmp_path / "private"))
    first = runtime.open_session(source)
    assert first.entities()[1]["render_cell"] == 777
    original_block = next(
        entity for entity in first.entities() if entity["kind"] == 9
    )
    for _ in range(20):
        first.tick(0)
    first_snapshot = first.snapshot()
    first_digest = first.digest()
    moved_block = next(
        entity for entity in first.entities() if entity["kind"] == 9
    )
    assert first.event_fired(0)
    assert (moved_block["x"], moved_block["y"]) != (
        original_block["x"],
        original_block["y"],
    )

    second = runtime.open_session(source)
    for _ in range(20):
        second.tick(0)
    assert second.snapshot() == first_snapshot
    assert second.digest() == first_digest
    second.tick(1)
    assert second.digest() != first_digest
    second.reset()
    for _ in range(20):
        second.tick(0)
    assert second.digest() == first_digest


def _interaction_project(ruleset, entities):
    source = project_template(ruleset, f"{ruleset}-interaction-kit")
    source["project_id"] = f"{ruleset}-interaction-test"
    source["size"] = [24, 15]
    source["terrain"] = [
        {"rect": [0, 13, 24, 2], "tile": 1, "collision": "solid"}
    ]
    source["entities"] = [
        {"kind": "player", "x": 48, "y": 208},
        *entities,
        {"kind": "goal", "x": 352, "y": 208, "params": [16, 32]},
    ]
    return source


def test_dynamic_fire_pot_and_ring_scatter_use_shared_core(tmp_path):
    runtime = MakerLiteRuntime(str(ROOT), str(tmp_path / "private"))

    mario = runtime.open_session(
        _interaction_project(
            "mario",
            [
                {"kind": "item", "x": 48, "y": 208},
                {"kind": "item", "x": 48, "y": 208},
            ],
        )
    )
    mario.tick(0)
    mario.tick(1 << 5)
    assert any(dynamic["kind"] == 11 for dynamic in mario.dynamics())

    zelda = runtime.open_session(
        _interaction_project(
            "zelda",
            [{"kind": "pot", "x": 48, "y": 208}],
        )
    )
    zelda.tick(1 << 5)
    zelda.tick(0)
    zelda.tick(1 << 5)
    assert any(dynamic["kind"] == 12 for dynamic in zelda.dynamics())

    sonic = runtime.open_session(
        _interaction_project(
            "sonic",
            [
                {"kind": "collectible", "x": 48, "y": 208},
                {"kind": "enemy", "x": 48, "y": 208,
                 "flags": 0x800, "params": [16, 16, 0, 0]},
            ],
        )
    )
    for _ in range(90):
        sonic.tick(0)
    assert sonic.snapshot()["rings"] == 0
    assert sum(dynamic["kind"] == 3 for dynamic in sonic.dynamics()) > 0


def test_stock_sonic_play_modifier_and_previous_next_camera(tmp_path):
    runtime = MakerLiteRuntime(str(ROOT), str(tmp_path / "private"))
    source = _interaction_project("sonic", [])
    source["size"] = [40, 15]
    source["terrain"][0]["rect"] = [0, 13, 40, 2]
    source["entities"][0]["x"] = 320

    sonic = runtime.open_session(source)
    for _ in range(8):
        sonic.tick(0)
    assert sonic.snapshot()["grounded"]
    sonic.tick(1 << 5)
    assert sonic.snapshot()["action"] == 10
    sonic.tick((1 << 5) | (1 << 4))
    assert sonic.snapshot()["action"] == 11

    neutral = runtime.open_session(source)
    neutral.tick(0)
    neutral_camera = neutral.snapshot()["camera_x"]
    glance = runtime.open_session(source)
    glance.tick(1 << 7)
    assert glance.snapshot()["camera_x"] > neutral_camera


def _life_project(entities):
    source = project_template("zelda", "zelda-neon-nook-v1", "Life Test")
    source["project_id"] = "neon-nook-life-test"
    source["gameplay"] = "life_sim"
    source["size"] = [16, 14]
    source["terrain"] = [
        {"rect": [0, 0, 16, 14], "tile": 1, "collision": []}
    ]
    source["rooms"] = []
    source["room_links"] = []
    source["entities"] = [
        {"kind": "player", "x": 64, "y": 112},
        *entities,
    ]
    return source


def _press(session, input_mask):
    session.tick(input_mask)
    session.tick(0)


def test_brawl_mode_runs_two_fighters_with_damage_and_stocks(tmp_path):
    runtime = MakerLiteRuntime(str(ROOT), str(tmp_path / "brawl-runtime"))
    source = project_template(
        "mario",
        "maker-brawl-volt-jack-v1",
        game_type="brawl",
    )
    source["project_id"] = "maker-brawl-runtime-test"
    source["entities"][1]["x"] = 112
    session = runtime.open_session(source)

    initial = session.snapshot()
    assert initial["brawl_player_stocks"] == 3
    assert initial["brawl_opponent_stocks"] == 3
    assert initial["brawl_opponent_entity"] == 1
    for tick in range(180):
        session.tick((1 << 4) if tick % 12 == 0 else 0)
    after_attack = session.snapshot()
    assert after_attack["brawl_opponent_damage"] >= 7
    assert session.entities()[1]["alive"]


def test_life_sim_shop_debt_house_and_style_loop(tmp_path):
    runtime = MakerLiteRuntime(str(ROOT), str(tmp_path / "life-runtime"))
    source = _life_project(
        [
            {"kind": "shop", "x": 72, "y": 112, "render_cell": 121},
            {
                "kind": "house",
                "x": 32,
                "y": 112,
                "render_cell": 122,
                "params": [32, 32, 122, 12],
            },
            *[
                {
                    "kind": "collectible",
                    "x": 64,
                    "y": 112,
                    "render_cell": 123,
                    "params": [12, 12, 10, 0],
                }
                for _ in range(20)
            ],
        ]
    )
    session = runtime.open_session(source)
    session.tick(0)
    assert session.snapshot()["collectibles"] == 20
    assert session.snapshot()["debt"] == 500

    _press(session, 1 << 4)
    assert session.snapshot()["interaction"] == 2
    _press(session, 1 << 7)
    _press(session, 1 << 4)
    assert session.snapshot()["collectibles"] == 0
    assert session.snapshot()["credits"] == 1000

    _press(session, 1 << 7)
    _press(session, 1 << 4)
    assert session.snapshot()["car_level"] == 1
    assert session.snapshot()["credits"] == 700
    _press(session, 1 << 5)
    assert session.snapshot()["interaction"] == 0

    for _ in range(16):
        session.tick(1 << 0)
    session.tick(0)
    _press(session, 1 << 4)
    assert session.snapshot()["interaction"] == 1
    _press(session, 1 << 7)
    _press(session, 1 << 4)
    assert session.snapshot()["debt"] == 0
    assert session.snapshot()["credits"] == 200
    _press(session, 1 << 7)
    _press(session, 1 << 4)
    assert session.snapshot()["house_level"] == 1
    assert session.snapshot()["debt"] == 1500
    _press(session, 1 << 7)
    _press(session, 1 << 4)
    assert session.snapshot()["house_style"] == 1


def test_life_sim_car_and_repeatable_npc_work(tmp_path):
    runtime = MakerLiteRuntime(str(ROOT), str(tmp_path / "life-runtime"))
    car = runtime.open_session(
        _life_project(
            [{"kind": "car", "x": 72, "y": 112, "render_cell": 130}]
        )
    )
    _press(car, 1 << 4)
    assert car.snapshot()["car_active"] == 1
    assert not next(
        entity for entity in car.entities() if entity["kind"] == 16
    )["alive"]
    start_x = car.snapshot()["player_x"]
    for _ in range(10):
        car.tick(1 << 1)
    assert car.snapshot()["player_x"] - start_x > 20 * car.FIXED_ONE
    car.tick(1 << 4)
    car.tick(0)
    assert car.snapshot()["car_active"] == 0
    assert next(
        entity for entity in car.entities() if entity["kind"] == 16
    )["alive"]

    npc = runtime.open_session(
        _life_project(
            [
                {
                    "kind": "npc",
                    "x": 72,
                    "y": 112,
                    "render_cell": 131,
                    "params": [14, 16, 64, 0],
                }
            ]
        )
    )
    _press(npc, 1 << 4)
    assert npc.snapshot()["interaction"] == 3
    _press(npc, 1 << 4)
    assert npc.snapshot()["credits"] == 25
    assert npc.snapshot()["job_cooldown"] > 0
    _press(npc, 1 << 4)
    assert npc.snapshot()["credits"] == 25

    roaming = runtime.open_session(
        _life_project(
            [
                {
                    "kind": "npc",
                    "x": 120,
                    "y": 112,
                    "render_cell": 132,
                    "params": [14, 16, 64, 0],
                }
            ]
        )
    )
    before = next(
        entity for entity in roaming.entities() if entity["kind"] == 13
    )
    for _ in range(100):
        roaming.tick(0)
    after = next(
        entity for entity in roaming.entities() if entity["kind"] == 13
    )
    assert (after["x"], after["y"]) != (before["x"], before["y"])


def test_life_sim_furniture_pickup_place_and_sale(tmp_path):
    runtime = MakerLiteRuntime(str(ROOT), str(tmp_path / "life-runtime"))
    source = _life_project(
        [
            {
                "kind": "furniture",
                "x": 72,
                "y": 112,
                "render_cell": 133,
                "params": [14, 14, 85, 0],
            }
        ]
    )
    moving = runtime.open_session(source)
    _press(moving, 1 << 4)
    assert moving.snapshot()["interaction"] == 4
    _press(moving, 1 << 4)
    assert moving.snapshot()["furniture_held_entity"] == 1
    assert moving.snapshot()["carrying"] == 1
    assert not moving.entities()[1]["alive"]
    for _ in range(12):
        moving.tick(1 << 1)
    _press(moving, 1 << 4)
    assert moving.snapshot()["furniture_held_entity"] == 0xFFFF
    assert moving.snapshot()["carrying"] == 0
    assert moving.entities()[1]["alive"]
    assert moving.entities()[1]["x"] > 72

    selling = runtime.open_session(source)
    _press(selling, 1 << 4)
    _press(selling, 1 << 7)
    _press(selling, 1 << 4)
    assert selling.snapshot()["credits"] == 85
    assert not selling.entities()[1]["alive"]
