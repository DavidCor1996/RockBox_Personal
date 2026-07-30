import json
import struct
import sys
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "rockpod"))

from services.maker_lite_assets import (  # noqa: E402
    list_private_kits,
    validate_asset_catalog,
)
from services.maker_lite_brawl import (  # noqa: E402
    BRAWL_KIT_IDS,
    FIGHTER_METASPRITE_FLAG,
    compose_private_guest_kit,
    install_bundled_brawl_kits,
)
from services.maker_lite_projects import MakerLiteProjectStore  # noqa: E402
from services.maker_lite_runtime import MakerLiteRuntime  # noqa: E402
from ui.maker_lite_preview import _load_art  # noqa: E402


def test_original_brawl_roster_installs_and_plays(tmp_path):
    private_root = tmp_path / "private"
    manifests = install_bundled_brawl_kits(
        str(private_root),
        str(ROOT),
    )

    assert {manifest["kit_id"] for manifest in manifests} == set(
        BRAWL_KIT_IDS
    )
    kits = {
        kit["kit_id"]: kit
        for kit in list_private_kits(private_root)
    }
    assert set(BRAWL_KIT_IDS) <= set(kits)
    for kit_id in BRAWL_KIT_IDS:
        kit = kits[kit_id]
        assert kit["source"]["type"] == "original-generated"
        assert sum(
            part.get("category") == "Fighters"
            for part in kit["asset_catalog"]
        ) == 4
        cells, player_base, entity_cells, animations, frames = _load_art(
            private_root / "kits" / kit_id / "art.mla"
        )
        assert len(cells) == 21
        assert player_base == 0
        assert entity_cells[4] == 14
        assert len(animations) == 14
        assert frames == []

    store = MakerLiteProjectStore(str(private_root))
    record = store.create(
        "mario",
        BRAWL_KIT_IDS[0],
        game_type="brawl",
    )
    opponent = next(
        part
        for part in kits[BRAWL_KIT_IDS[0]]["asset_catalog"]
        if part["id"] == "mossbyte"
    )
    enemy = record.source["entities"][1]
    enemy["asset_id"] = opponent["id"]
    enemy["render_cell"] = opponent["cell"]
    enemy["params"] = opponent["params"]
    assert store.compile(record).startswith(b"RPML")

    session = MakerLiteRuntime(
        str(ROOT),
        str(private_root),
    ).open_session(record.source)
    assert session.snapshot()["brawl_player_stocks"] == 3
    assert session.snapshot()["brawl_opponent_stocks"] == 3

    composite = compose_private_guest_kit(
        str(private_root),
        BRAWL_KIT_IDS[0],
        BRAWL_KIT_IDS[1],
    )
    guest = composite["asset_catalog"][-1]
    assert composite["source"]["type"] == "private-composite"
    assert guest["flags"] & FIGHTER_METASPRITE_FLAG
    cells, _base, _entity_cells, _animations, frames = _load_art(
        private_root / "kits" / composite["kit_id"] / "art.mla"
    )
    assert guest["cell"] < len(cells)
    assert guest["frame"] < len(frames)
    assert validate_asset_catalog(
        composite["asset_catalog"],
        len(cells),
        {},
    )[-1]["frame"] == guest["frame"]


def test_public_brawl_pack_has_no_commercial_guest_labels():
    forbidden = ("mario", "sonic", "spinal", "killer instinct")
    for manifest_path in (
        ROOT / "assets" / "maker_lite" / "maker_brawl" / "pack"
    ).glob("*/kit.mlk"):
        source = json.loads(manifest_path.read_text(encoding="utf-8"))
        serialized = json.dumps(
            {
                "player_name": source.get("player_name"),
                "catalog": source.get("asset_catalog"),
                "source": source.get("source"),
            }
        ).casefold()
        assert not any(label in serialized for label in forbidden)


def test_private_guest_composer_accepts_legacy_v1_opponent(tmp_path):
    private_root = tmp_path / "private"
    install_bundled_brawl_kits(str(private_root), str(ROOT))
    legacy = private_root / "kits" / "legacy-private-opponent"
    legacy.mkdir(parents=True)
    pixels = bytes(14 * 512)
    header = bytearray(64)
    header[:4] = b"MLAR"
    struct.pack_into(
        "<HHHHI",
        header,
        4,
        1,
        16,
        14,
        0,
        zlib.crc32(pixels) & 0xFFFFFFFF,
    )
    (legacy / "art.mla").write_bytes(bytes(header) + pixels)
    (legacy / "kit.mlk").write_text(
        json.dumps(
            {
                "kit_id": "legacy-private-opponent",
                "ruleset": "mario",
                "player_name": "Private Guest",
            }
        ),
        encoding="utf-8",
    )

    composite = compose_private_guest_kit(
        str(private_root),
        BRAWL_KIT_IDS[0],
        "legacy-private-opponent",
    )

    guest = composite["asset_catalog"][-1]
    assert guest["label"] == "Private Guest"
    assert guest["flags"] & FIGHTER_METASPRITE_FLAG
    assert (private_root / "kits" / composite["kit_id"] / "art.mla").is_file()
