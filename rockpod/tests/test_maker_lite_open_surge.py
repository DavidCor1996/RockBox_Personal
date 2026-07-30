import json
import sys
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "rockpod"))

from services.maker_lite_open_surge import (  # noqa: E402
    import_open_surge_subset,
    load_open_surge_brick_mapping,
)


def test_open_surge_subset_imports_bricks_and_known_entities():
    project = json.loads(
        (ROOT / "testdata/maker_lite/sonic-test.json").read_text(encoding="utf-8")
    )
    imported, diagnostics = import_open_surge_subset(
        """
        name "Imported Zone"
        author "Surge Creator"
        spawn_point 32 208
        brick 12 64 32
        entity "Collectible" 96 48 "be647e181fcd7513"
        entity "Unsupported Boss" 120 64
        object "scripted" { function = "update"; }
        """,
        project,
        brick_collision={12: "solid"},
    )
    assert imported["title"] == "Imported Zone"
    assert imported["metadata"]["author"] == "Surge Creator"
    assert imported["entities"][0]["x"] == 32
    assert imported["terrain"][-1] == {
        "point": [4, 2],
        "tile": 12,
        "collision": "solid",
    }
    assert imported["entities"][-1]["kind"] == "collectible"
    assert [item.severity for item in diagnostics] == [
        "info", "warning", "warning",
    ]
    assert "unsupported Open Surge entity" in diagnostics[1].message


def test_open_surge_out_of_bounds_is_an_error():
    project = json.loads(
        (ROOT / "testdata/maker_lite/mario-test.json").read_text(encoding="utf-8")
    )
    _imported, diagnostics = import_open_surge_subset(
        "brick 1 99984 16",
        project,
    )
    assert diagnostics[0].severity == "error"


def test_open_surge_never_rounds_or_guesses_brick_collision():
    project = json.loads(
        (ROOT / "testdata/maker_lite/sonic-test.json").read_text(
            encoding="utf-8"
        )
    )
    imported, diagnostics = import_open_surge_subset(
        "brick 7 17 32\nbrick 8 32 48 hflip",
        project,
    )
    assert imported["terrain"][-1] == {
        "point": [2, 3],
        "tile": 8,
        "collision": [],
    }
    assert [item.severity for item in diagnostics] == [
        "error", "warning", "warning",
    ]


def test_open_surge_collision_sidecar_is_strict_and_reviewed(tmp_path):
    path = tmp_path / "zone.maker-lite-bricks.json"
    path.write_text(
        json.dumps(
            {
                "format_version": 1,
                "brick_collision": {
                    "1": "solid",
                    "2": ["slope_up", "loop"],
                    "3": [],
                },
            }
        ),
        encoding="utf-8",
    )
    assert load_open_surge_brick_mapping(path) == {
        1: "solid",
        2: ["slope_up", "loop"],
        3: [],
    }

    path.write_text(
        json.dumps(
            {
                "format_version": 1,
                "brick_collision": {"01": "solid"},
            }
        ),
        encoding="utf-8",
    )
    with pytest.raises(ValueError, match="canonical"):
        load_open_surge_brick_mapping(path)
