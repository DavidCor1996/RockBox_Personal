import json
import struct
import sys
import zlib
from pathlib import Path

import pytest
from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "rockpod"))

from services.maker_lite_export import export_project, sync_projects  # noqa: E402
from services.maker_lite_pack import parse_pack  # noqa: E402
from services.maker_lite_projects import (  # noqa: E402
    GAME_TYPES,
    MakerLiteProjectStore,
)


def _install_test_kit(private, kit_id):
    directory = private / "kits" / kit_id
    directory.mkdir(parents=True)
    payload = b"\0" * (16 * 16 * 2 * 14)
    header = bytearray(64)
    header[:4] = b"MLAR"
    struct.pack_into("<HHHHI", header, 4, 1, 16, 14, 0,
                     zlib.crc32(payload) & 0xFFFFFFFF)
    header[16:16 + len(kit_id)] = kit_id.encode("ascii")
    directory.joinpath("art.mla").write_bytes(bytes(header) + payload)
    directory.joinpath("kit.mlk").write_text("{}\n", encoding="utf-8")
    directory.joinpath("provenance.tsv").write_text("test\n", encoding="utf-8")


def _install_v4_test_kit(private, kit_id):
    directory = private / "kits" / kit_id
    directory.mkdir(parents=True)
    pixels = b"\0" * (16 * 16 * 2 * 14)
    animations = b"".join(
        struct.pack("<HBB", action, 1, 2 | (0x80 if direction == 2 else 0))
        for action in range(14)
        for direction in range(4)
    )
    frames = bytearray(struct.pack("<HH", 14, 0))
    for action in range(14):
        frames.extend(
            struct.pack(
                "<BBbb16H",
                1,
                1,
                -8,
                -16,
                action,
                *([0xFFFF] * 15),
            )
        )
    payload = pixels + animations + frames
    header = bytearray(64)
    header[:4] = b"MLAR"
    struct.pack_into(
        "<HHHHI",
        header,
        4,
        4,
        16,
        14,
        0,
        zlib.crc32(payload) & 0xFFFFFFFF,
    )
    header[16 : 16 + len(kit_id)] = kit_id.encode("ascii")
    directory.joinpath("art.mla").write_bytes(bytes(header) + payload)
    directory.joinpath("kit.mlk").write_text("{}\n", encoding="utf-8")
    directory.joinpath("provenance.tsv").write_text("test-v4\n", encoding="utf-8")


def test_project_roundtrip_autosave_and_compile(tmp_path):
    private = tmp_path / "private"
    store = MakerLiteProjectStore(str(private))
    record = store.create("mario", "mario-testkit", "Click Wheel Plains")
    loaded = store.load(record.project_id)
    assert loaded.source == record.source

    loaded.source["title"] = "Recovered Title"
    store.autosave(loaded)
    Path(loaded.directory, "project.json").write_text("{broken", encoding="utf-8")
    recovered = store.load(record.project_id)
    assert recovered.recovered
    assert recovered.source["title"] == "Recovered Title"
    assert parse_pack(store.compile(recovered)).ruleset == "mario"

    imported = store.import_file(str(Path(recovered.directory, "autosave/project.json")))
    assert imported.project_id != recovered.project_id
    assert imported.source["title"] == "Recovered Title"


@pytest.mark.parametrize(
    ("ruleset", "game_type"),
    [
        (ruleset, game_type)
        for ruleset, choices in GAME_TYPES.items()
        for game_type, _label in choices
    ],
)
def test_every_game_type_creates_a_runtime_playable_project(
    tmp_path,
    ruleset,
    game_type,
):
    store = MakerLiteProjectStore(str(tmp_path / "private"))
    record = store.create(
        ruleset,
        f"{ruleset}-testkit",
        game_type=game_type,
    )

    assert record.source["game_type"] == game_type
    assert record.source["metadata"]["description"]
    metadata = parse_pack(store.compile(record))
    assert metadata.ruleset == ruleset
    assert metadata.entity_count == len(record.source["entities"])


def test_export_cover_and_manifest_are_project_specific(tmp_path):
    private = tmp_path / "private"
    store = MakerLiteProjectStore(str(private))
    record = store.create("zelda", "zelda-testkit", "Wheel of Hyrule")
    _install_test_kit(private, "zelda-testkit")
    cover = tmp_path / "cover.png"
    Image.new("RGB", (320, 200), (30, 110, 50)).save(cover)
    record.source["metadata"].update(
        {
            "show_in_steam": True,
            "cover_source": str(cover),
            "author": "Personal Creator",
            "description": "A private test level",
        }
    )
    store.save(record)
    exported = export_project(record, str(private))
    assert Path(exported["cover"]).read_bytes()[:2] == b"BM"

    mount = tmp_path / "ipod"
    plugin = tmp_path / "maker_lite.rock"
    plugin.write_bytes(b"plugin")
    result = sync_projects([record], str(private), str(mount), str(plugin))
    manifest = Path(result["manifest"]).read_text(encoding="utf-8")
    fields = manifest.rstrip("\n").split("\t")
    assert len(fields) == 11
    assert fields[0] == "Wheel of Hyrule"
    assert fields[1] == "/.rockbox/rocks/games/maker_lite.rock"
    assert fields[10].endswith(f"/{record.project_id}/game.mlp")
    assert Path(mount, fields[2].lstrip("/")).is_file()
    assert Path(mount, fields[10].lstrip("/")).is_file()
    browser_manifest = Path(result["browser_manifest"]).read_text(
        encoding="utf-8"
    )
    assert browser_manifest == manifest
    assert result["browser_rows"] == 1
    settings = Path(
        mount,
        ".rockbox/games/maker_lite/settings",
        f"{record.project_id}.mlc",
    ).read_bytes()
    assert settings[:4] == b"MLCT"


def test_no_cover_project_stays_out_of_steam(tmp_path):
    private = tmp_path / "private"
    store = MakerLiteProjectStore(str(private))
    record = store.create("sonic", "sonic-testkit", "Quiet Hill")
    _install_test_kit(private, "sonic-testkit")
    record.source["metadata"]["show_in_steam"] = False
    store.save(record)
    mount = tmp_path / "ipod"
    result = sync_projects([record], str(private), str(mount))
    assert result["rows"] == 0
    assert Path(result["manifest"]).read_text(encoding="utf-8") == ""
    browser = Path(result["browser_manifest"]).read_text(encoding="utf-8")
    fields = browser.rstrip("\n").split("\t")
    assert result["browser_rows"] == 1
    assert len(fields) == 11
    assert fields[0] == "Quiet Hill"
    assert fields[2] == ""
    assert fields[10].endswith(f"/{record.project_id}/game.mlp")


def test_v4_metasprite_kit_syncs_to_device(tmp_path):
    private = tmp_path / "private"
    store = MakerLiteProjectStore(str(private))
    record = store.create("sonic", "sonic-v4-testkit", "Metasprite Hill")
    _install_v4_test_kit(private, "sonic-v4-testkit")

    mount = tmp_path / "ipod"
    result = sync_projects([record], str(private), str(mount))

    assert result["rows"] == 0
    installed = Path(
        mount,
        ".rockbox/games/maker_lite/kits/sonic-v4-testkit/art.mla",
    )
    assert installed.is_file()
    assert int.from_bytes(installed.read_bytes()[4:6], "little") == 4


def test_show_in_steam_requires_cover_and_valid_private_kit(tmp_path):
    private = tmp_path / "private"
    store = MakerLiteProjectStore(str(private))
    record = store.create("mario", "mario-testkit", "No Fake Cover")
    _install_test_kit(private, "mario-testkit")
    record.source["metadata"]["show_in_steam"] = True
    store.save(record)

    from services.maker_lite_export import MakerLiteExportError

    mount = tmp_path / "ipod"
    manifest = mount / ".rockbox/rocks/games/maker_lite/games.tsv"
    manifest.parent.mkdir(parents=True)
    manifest.write_text("existing-library\n", encoding="utf-8")
    browser_manifest = mount / ".rockbox/games/maker_lite/projects.tsv"
    browser_manifest.parent.mkdir(parents=True)
    browser_manifest.write_text("existing-projects\n", encoding="utf-8")
    with pytest.raises(MakerLiteExportError, match="requires readable cover"):
        sync_projects([record], str(private), str(mount))
    assert manifest.read_text(encoding="utf-8") == "existing-library\n"
    assert browser_manifest.read_text(encoding="utf-8") == "existing-projects\n"
    assert not Path(
        mount,
        ".rockbox/games/maker_lite/projects",
        record.project_id,
        "game.mlp",
    ).exists()

    record.source["metadata"]["show_in_steam"] = False
    Path(private, "kits/mario-testkit/art.mla").write_bytes(b"corrupt")
    with pytest.raises(MakerLiteExportError, match="failed validation|unreadable"):
        sync_projects([record], str(private), str(mount))
    assert manifest.read_text(encoding="utf-8") == "existing-library\n"
    assert browser_manifest.read_text(encoding="utf-8") == "existing-projects\n"


def test_native_browser_uses_all_project_index_with_legacy_fallback():
    source = Path(
        ROOT,
        "apps/plugins/maker_lite/maker_lite_storage.c",
    ).read_text(encoding="utf-8")
    header = Path(
        ROOT,
        "apps/plugins/maker_lite/maker_lite.h",
    ).read_text(encoding="utf-8")

    assert 'MAKER_LITE_ROOT "/projects.tsv"' in header
    assert 'ROCKBOX_DIR "/rocks/games/maker_lite/games.tsv"' in header
    assert "rb->open(MAKER_LITE_BROWSER_MANIFEST, O_RDONLY)" in source
    assert "rb->open(MAKER_LITE_LEGACY_MANIFEST, O_RDONLY)" in source
