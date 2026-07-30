import hashlib
import json
import sys
import wave
from pathlib import Path

import pytest
from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "rockpod"))

from services.maker_lite_assets import (  # noqa: E402
    BUNDLED_NEON_NOOK_KIT_ID,
    MakerLiteAssetError,
    SupportedRevision,
    _lorom_offset,
    _snesrev_decompress,
    _smw_3bpp_to_4bpp,
    _smw_decompress,
    _smw_map16_pointers,
    detect_supported_revision,
    discover_extraction_recipes,
    import_extraction_bundle,
    import_extraction_recipe,
    inspect_source,
    install_bundled_neon_nook_kit,
    install_private_asset_suite,
    scan_supported_sources,
)


def test_smw_asset_free_helpers_follow_lorom_and_clean_room_formats():
    rom = bytearray(512 * 1024)
    stream = bytes(
        (
            0x02,
            ord("a"),
            ord("b"),
            ord("c"),
            0x23,
            ord("x"),
            0x44,
            ord("1"),
            ord("2"),
            0x62,
            0x40,
            0x82,
            0x00,
            0x00,
            0xFF,
        )
    )
    rom[_lorom_offset(0x008000) : _lorom_offset(0x008000) + len(stream)] = stream
    assert _smw_decompress(bytes(rom), 0x008000) == b"abcxxxx12121@ABabc"

    little_endian_stream = bytes(
        (0x02, ord("a"), ord("b"), ord("c"), 0x82, 0x00, 0x00, 0xFF)
    )
    start = _lorom_offset(0x00FFFC)
    rom[start : start + len(little_endian_stream)] = little_endian_stream
    assert _snesrev_decompress(
        bytes(rom), 0x00FFFC, offset_is_be=False
    ) == b"abcabc"

    source_3bpp = bytes(range(24))
    converted = _smw_3bpp_to_4bpp(source_3bpp)
    assert len(converted) == 32
    assert converted[:16] == source_3bpp[:16]
    assert converted[16:] == bytes(
        value
        for source in source_3bpp[16:]
        for value in (source, 0)
    )

    # All-zero selection chooses the sequential tileset-specific Map16 bank.
    selection_offset = _lorom_offset(0x0581BB, 64)
    rom[selection_offset : selection_offset + 64] = b"\0" * 64
    pointers = _smw_map16_pointers(bytes(rom))
    assert pointers[:2] == [0x0D8B70, 0x0D8B78]
    assert pointers[452] == 0x0D8A70
    assert pointers[492] == 0x0D8A90


def _reference_and_bundle(tmp_path, ruleset="mario", cells=16):
    rom = tmp_path / ("reference.gen" if ruleset == "sonic" else "reference.sfc")
    data = bytearray(512 * 1024)
    if ruleset == "sonic":
        data[0x120:0x150] = b"SONIC THE HEDGEHOG 2".ljust(0x30)
    else:
        data[0x7FC0:0x7FC0 + 21] = b"REFERENCE GAME".ljust(21)
    rom.write_bytes(data)
    digest = hashlib.sha256(data).hexdigest()
    bundle = tmp_path / "bundle"
    bundle.mkdir()
    atlas = Image.new("RGBA", (cells * 16, 16))
    for index in range(cells):
        color = (index * 13 % 255, index * 29 % 255, index * 47 % 255, 255)
        for y in range(16):
            for x in range(16):
                atlas.putpixel((index * 16 + x, y), color)
    atlas.save(bundle / "atlas.png")
    (bundle / "kit.json").write_text(
        json.dumps(
            {
                "ruleset": ruleset,
                "revision_id": "test-revision",
                "source_sha256": digest,
                "atlas": "atlas.png",
                "player_base": 0,
                "cells": [
                    {"index": index, "name": f"cell-{index}", "source_rect": [0, 0, 16, 16]}
                    for index in range(cells)
                ],
            }
        ),
        encoding="utf-8",
    )
    return rom, bundle


def _fixture_revisions(rom, ruleset="mario"):
    info = inspect_source(rom)
    return (
        SupportedRevision(
            ruleset,
            "test-revision",
            "sha256",
            info.canonical_sha256,
            info.platform,
        ),
    )


def test_private_kit_import_is_deterministic_and_does_not_copy_rom(tmp_path):
    rom, bundle = _reference_and_bundle(tmp_path)
    cover = Image.new("RGB", (300, 400), (31, 63, 127))
    cover.save(bundle / "cover.png")
    manifest_path = bundle / "kit.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["source_cover"] = "cover.png"
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    private = tmp_path / "private"
    revisions = _fixture_revisions(rom)
    first = import_extraction_bundle(rom, bundle, private, revisions=revisions)
    first_bytes = (Path(first.directory) / "art.mla").read_bytes()
    second = import_extraction_bundle(rom, bundle, private, revisions=revisions)

    assert first.kit_id == second.kit_id
    assert first_bytes == (Path(second.directory) / "art.mla").read_bytes()
    assert not list(private.rglob("*.sfc"))
    assert inspect_source(rom).title == "REFERENCE GAME"
    kit_manifest = json.loads(Path(first.directory, "kit.mlk").read_text())
    assert kit_manifest["source_cover_file"] == "source-cover.png"
    assert Path(first.directory, "source-cover.png").is_file()


def test_private_kit_rejects_wrong_source_and_missing_provenance(tmp_path):
    rom, bundle = _reference_and_bundle(tmp_path)
    manifest_path = bundle / "kit.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["source_sha256"] = "0" * 64
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    with pytest.raises(MakerLiteAssetError, match="hash mismatch"):
        import_extraction_bundle(
            rom,
            bundle,
            tmp_path / "private",
            revisions=_fixture_revisions(rom),
        )

    manifest["source_sha256"] = hashlib.sha256(rom.read_bytes()).hexdigest()
    manifest["cells"].pop()
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    with pytest.raises(MakerLiteAssetError, match="Every atlas cell"):
        import_extraction_bundle(
            rom,
            bundle,
            tmp_path / "private",
            revisions=_fixture_revisions(rom),
        )


def test_private_kit_packs_only_user_supplied_pcm_effects(tmp_path):
    rom, bundle = _reference_and_bundle(tmp_path)
    effect = bundle / "jump.wav"
    with wave.open(str(effect), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(44100)
        output.writeframes(b"\0\0\0\0" * 64)
    manifest_path = bundle / "kit.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["effects"] = {"jump": effect.name}
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    kit = import_extraction_bundle(
        rom,
        bundle,
        tmp_path / "private",
        revisions=_fixture_revisions(rom),
    )
    audio = Path(kit.directory, "audio.mla").read_bytes()
    assert audio[:4] == b"MLAU"
    assert int.from_bytes(audio[8:12], "little") == 44100
    assert len(audio) == 128 + 64 * 4


def test_private_kit_rejects_an_unpublished_revision_by_default(tmp_path):
    rom, bundle = _reference_and_bundle(tmp_path)
    with pytest.raises(MakerLiteAssetError, match="Unsupported mario source revision"):
        import_extraction_bundle(rom, bundle, tmp_path / "private")


def test_private_kit_persists_a_runtime_true_authentic_asset_catalog(tmp_path):
    rom, bundle = _reference_and_bundle(tmp_path, cells=20)
    manifest_path = bundle / "kit.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["entity_cells"] = {"enemy": 15}
    manifest["asset_catalog"] = [
        {
            "id": "ground",
            "label": "Ground",
            "category": "Terrain",
            "type": "terrain",
            "cell": 14,
            "collision": "solid",
        },
        {
            "id": "goomba",
            "label": "Goomba",
            "category": "Enemies",
            "type": "entity",
            "cell": 15,
            "kind": "enemy",
            "params": [16, 16, 96, 0],
            "flags": 1024,
        },
    ]
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    kit = import_extraction_bundle(
        rom,
        bundle,
        tmp_path / "private",
        revisions=_fixture_revisions(rom),
    )
    private_manifest = json.loads(
        Path(kit.directory, "kit.mlk").read_text(encoding="utf-8")
    )
    assert private_manifest["entity_cells"] == {"enemy": 15}
    assert private_manifest["asset_catalog"] == [
        {
            "id": "ground",
            "label": "Ground",
            "category": "Terrain",
            "type": "terrain",
            "cell": 14,
            "collision": ["solid"],
        },
        {
            "id": "goomba",
            "label": "Goomba",
            "category": "Enemies",
            "type": "entity",
            "cell": 15,
            "kind": "enemy",
            "params": [16, 16, 96, 0],
            "flags": 1024,
        },
    ]


def test_private_kit_accepts_distinct_render_cells_for_one_entity_kind(tmp_path):
    rom, bundle = _reference_and_bundle(tmp_path, cells=20)
    manifest_path = bundle / "kit.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["entity_cells"] = {"enemy": 15}
    manifest["asset_catalog"] = [
        {
            "id": "wrong-enemy",
            "label": "Wrong Enemy",
            "category": "Enemies",
            "type": "entity",
            "cell": 14,
            "kind": "enemy",
        }
    ]
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    kit = import_extraction_bundle(
        rom,
        bundle,
        tmp_path / "private",
        revisions=_fixture_revisions(rom),
    )
    private_manifest = json.loads(
        Path(kit.directory, "kit.mlk").read_text(encoding="utf-8")
    )
    assert private_manifest["asset_catalog"][0]["cell"] == 14
    assert private_manifest["entity_cells"]["enemy"] == 15


def test_extraction_recipe_builds_exact_cells_without_a_prepared_atlas(tmp_path):
    rom, _ = _reference_and_bundle(tmp_path)
    workspace = tmp_path / "workspace"
    workspace.mkdir()
    sheet = Image.new("RGBA", (16, 16), (248, 120, 32, 255))
    sheet.putpixel((0, 0), (0, 0, 0, 0))
    sheet.save(workspace / "sheet.png")
    recipe = {
        "ruleset": "mario",
        "revision_id": "test-revision",
        "source_sha256": hashlib.sha256(rom.read_bytes()).hexdigest(),
        "player_base": 0,
        "entity_cells": {"enemy": 13},
        "asset_catalog": [
            {
                "id": "source-enemy",
                "label": "Source Enemy",
                "category": "Enemies",
                "type": "entity",
                "cell": 13,
                "kind": "enemy",
            }
        ],
        "cells": [
            {
                "name": f"action-{index}",
                "kind": "png",
                "file": "sheet.png",
                "x": 0,
                "y": 0,
            }
            for index in range(14)
        ],
    }
    recipe_path = workspace / "maker-lite-extract.json"
    recipe_path.write_text(json.dumps(recipe), encoding="utf-8")
    kit = import_extraction_recipe(
        rom,
        recipe_path,
        tmp_path / "private",
        revisions=_fixture_revisions(rom),
    )
    manifest = json.loads(Path(kit.directory, "kit.mlk").read_text())
    assert manifest["extractor"]["id"] == "rockpod-maker-lite-recipe"
    assert manifest["extractor"]["recipe_sha256"]
    assert manifest["cells"][0]["source_file"] == "sheet.png"
    assert manifest["asset_catalog"][0]["id"] == "source-enemy"
    art = Path(kit.directory, "art.mla").read_bytes()
    assert art[:4] == b"MLAR"
    assert int.from_bytes(art[8:10], "little") == 14

    del recipe["asset_catalog"]
    recipe_path.write_text(json.dumps(recipe), encoding="utf-8")
    with pytest.raises(MakerLiteAssetError, match="authentic asset_catalog"):
        import_extraction_recipe(
            rom,
            recipe_path,
            tmp_path / "private-no-catalog",
            revisions=_fixture_revisions(rom),
        )


def test_native_recipe_reads_the_verified_source_without_copying_it(tmp_path):
    rom, _ = _reference_and_bundle(tmp_path)
    data = bytearray(rom.read_bytes())
    art_offset = 0x10000
    palette_offset = 0x20000
    for tile in range(4):
        for y in range(8):
            data[art_offset + tile * 32 + y * 2] = 0xFF
    data[palette_offset + 2 : palette_offset + 4] = (0x001F).to_bytes(
        2, "little"
    )
    rom.write_bytes(data)
    workspace = tmp_path / "native-workspace"
    workspace.mkdir()
    recipe = {
        "ruleset": "mario",
        "revision_id": "test-revision",
        "source_sha256": hashlib.sha256(data).hexdigest(),
        "player_base": 0,
        "entity_cells": {"enemy": 13},
        "asset_catalog": [
            {
                "id": "source-enemy",
                "label": "Source Enemy",
                "category": "Enemies",
                "type": "entity",
                "cell": 13,
                "kind": "enemy",
            }
        ],
        "cells": [
            {
                "name": f"native-action-{index}",
                "kind": "snes4bpp",
                "file": "@source",
                "offset": art_offset,
                "tiles": [0, 1, 2, 3],
                "palette_file": "@source",
                "palette_offset": palette_offset,
            }
            for index in range(14)
        ],
    }
    recipe_path = workspace / "maker-lite-extract.json"
    recipe_path.write_text(json.dumps(recipe), encoding="utf-8")
    revisions = _fixture_revisions(rom)

    kit = import_extraction_recipe(
        rom,
        recipe_path,
        tmp_path / "private",
        revisions=revisions,
    )

    art = Path(kit.directory, "art.mla").read_bytes()
    assert art[64:66] == b"\x00\xf8"
    manifest = json.loads(Path(kit.directory, "kit.mlk").read_text())
    assert manifest["cells"][0]["source_file"] == "@source"
    assert not list((tmp_path / "private").rglob("*.sfc"))


def test_suite_scanner_matches_sources_and_recipes_by_verified_revision(tmp_path):
    rom, _ = _reference_and_bundle(tmp_path)
    revisions = _fixture_revisions(rom)
    workspace = tmp_path / "suite"
    workspace.mkdir()
    sheet = Image.new("RGBA", (16, 16), (32, 96, 192, 255))
    sheet.save(workspace / "sheet.png")
    recipe = {
        "ruleset": "mario",
        "revision_id": "test-revision",
        "source_sha256": hashlib.sha256(rom.read_bytes()).hexdigest(),
        "player_base": 0,
        "entity_cells": {"enemy": 13},
        "asset_catalog": [
            {
                "id": "enemy",
                "label": "Enemy",
                "category": "Enemies",
                "type": "entity",
                "cell": 13,
                "kind": "enemy",
            }
        ],
        "cells": [
            {
                "name": f"cell-{index}",
                "kind": "png",
                "file": "sheet.png",
                "x": 0,
                "y": 0,
            }
            for index in range(14)
        ],
    }
    recipe_path = workspace / "mario.maker-lite-extract.json"
    recipe_path.write_text(json.dumps(recipe), encoding="utf-8")
    decoy = workspace / "wrong.sfc"
    decoy.write_bytes(b"\x01" * (512 * 1024))

    sources = scan_supported_sources([tmp_path], revisions=revisions)
    recipes = discover_extraction_recipes([tmp_path])
    assert len(sources) == 1
    assert detect_supported_revision(sources[0].info, revisions).ruleset == "mario"
    assert [item.path for item in recipes] == [str(recipe_path)]

    report = install_private_asset_suite(
        [tmp_path],
        [tmp_path],
        tmp_path / "private",
        revisions=revisions,
    )
    assert [kit.ruleset for kit in report.installed] == ["mario"]
    assert report.missing_sources == ("sonic", "zelda")
    assert not report.missing_recipes
    assert not report.errors


def test_private_kit_packs_bounded_player_animation_frames(tmp_path):
    rom, bundle = _reference_and_bundle(tmp_path)
    manifest_path = bundle / "kit.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["animations"] = {
        "walk": {"frames": [1, 2], "ticks_per_frame": 3}
    }
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    kit = import_extraction_bundle(
        rom,
        bundle,
        tmp_path / "private",
        revisions=_fixture_revisions(rom),
    )
    art = Path(kit.directory, "art.mla").read_bytes()
    assert int.from_bytes(art[4:6], "little") == 3
    table = 64 + 16 * 16 * 2 * 16
    assert tuple(art[table + 16 : table + 20]) == (1, 0, 2, 3)
    assert tuple(art[table + 24 : table + 28]) == (1, 0, 2, 0x83)

    manifest["animations"]["walk"] = {
        "right": {"frames": [1, 2]},
        "frames": [1, 2],
    }
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    with pytest.raises(MakerLiteAssetError, match="mixes directional"):
        import_extraction_bundle(
            rom,
            bundle,
            tmp_path / "private-mixed",
            revisions=_fixture_revisions(rom),
        )


def test_private_kit_packs_original_size_player_metasprites(tmp_path):
    rom, bundle = _reference_and_bundle(tmp_path, cells=20)
    manifest_path = bundle / "kit.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["player_frames"] = [
        {
            "columns": 2,
            "rows": 2,
            "offset_x": -16,
            "offset_y": -32,
            "cells": [0, 1, 2, 3],
        }
        for _index in range(14)
    ]
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")

    kit = import_extraction_bundle(
        rom,
        bundle,
        tmp_path / "private",
        revisions=_fixture_revisions(rom),
    )

    art = Path(kit.directory, "art.mla").read_bytes()
    assert int.from_bytes(art[4:6], "little") == 4
    frame_header = 64 + 20 * 512 + 56 * 4
    assert int.from_bytes(art[frame_header : frame_header + 2], "little") == 14
    assert tuple(art[frame_header + 4 : frame_header + 8]) == (
        2,
        2,
        0xF0,
        0xE0,
    )
    assert [
        int.from_bytes(
            art[frame_header + 8 + index * 2 : frame_header + 10 + index * 2],
            "little",
        )
        for index in range(4)
    ] == [0, 1, 2, 3]
    private_manifest = json.loads(
        Path(kit.directory, "kit.mlk").read_text(encoding="utf-8")
    )
    assert private_manifest["player_frames"][0]["offset_y"] == -32

    manifest["player_frames"][0]["cells"][0] = 999
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    with pytest.raises(MakerLiteAssetError, match="outside the atlas"):
        import_extraction_bundle(
            rom,
            bundle,
            tmp_path / "private-invalid",
            revisions=_fixture_revisions(rom),
        )


def test_bundled_neon_nook_kit_is_complete_and_digest_pinned(tmp_path):
    private = tmp_path / "private"
    kit = install_bundled_neon_nook_kit(private)
    installed = Path(kit.directory)
    manifest = json.loads((installed / "kit.mlk").read_text(encoding="utf-8"))

    assert kit.kit_id == BUNDLED_NEON_NOOK_KIT_ID
    assert kit.ruleset == "zelda"
    assert kit.cell_count >= 200
    assert len(manifest["asset_catalog"]) >= 180
    assert len(manifest["player_frames"]) >= 56
    assert manifest["source"]["type"] == "original-generated"
    assert {
        asset["category"] for asset in manifest["asset_catalog"]
    } >= {
        "Villagers",
        "Terrain",
        "Architecture",
        "Tools & Collectibles",
        "Furniture & Decor",
    }
    assert {
        "art.mla",
        "audio.mla",
        "kit.mlk",
        "provenance.tsv",
        "source-cover.png",
    } <= {path.name for path in installed.iterdir()}

    bundle = tmp_path / "corrupt-pack"
    source_bundle = ROOT / "assets/maker_lite/neon_nook/pack"
    bundle.mkdir()
    for path in source_bundle.iterdir():
        if path.is_file():
            bundle.joinpath(path.name).write_bytes(path.read_bytes())
    bundle.joinpath("art.mla").write_bytes(b"corrupt")
    with pytest.raises(MakerLiteAssetError, match="checksum mismatch"):
        install_bundled_neon_nook_kit(tmp_path / "bad-private", bundle)
