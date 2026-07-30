"""Bundled original roster support for Maker Brawl."""

from __future__ import annotations

import json
import hashlib
import os
import shutil
import struct
import zlib
from pathlib import Path


BRAWL_KIT_IDS = (
    "maker-brawl-volt-jack-v1",
    "maker-brawl-mossbyte-v1",
    "maker-brawl-cinderwing-v1",
    "maker-brawl-bone-corsair-v1",
)
FIGHTER_METASPRITE_FLAG = 0x2000


def install_bundled_brawl_kits(private_root: str, repo_root: str) -> list[dict]:
    """Verify and install every public, original-generated brawler kit."""

    pack_root = (
        Path(repo_root)
        / "assets"
        / "maker_lite"
        / "maker_brawl"
        / "pack"
    )
    installed = []
    for kit_id in BRAWL_KIT_IDS:
        source = pack_root / kit_id
        manifest_path = source / "kit.mlk"
        art_path = source / "art.mla"
        if not manifest_path.is_file() or not art_path.is_file():
            raise ValueError(
                "Maker Brawl pack is missing; run "
                "tools/build_maker_lite_brawl_pack.py"
            )
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        art = art_path.read_bytes()
        if (
            manifest.get("kit_id") != kit_id
            or manifest.get("ruleset") != "mario"
            or manifest.get("source", {}).get("type")
            != "original-generated"
            or len(art) < 64
            or art[:4] != b"MLAR"
        ):
            raise ValueError(f"Invalid bundled Maker Brawl kit: {kit_id}")
        cell_count = struct.unpack_from("<H", art, 8)[0]
        payload = art[64:]
        if (
            not 18 <= cell_count <= 64
            or zlib.crc32(payload) & 0xFFFFFFFF
            != struct.unpack_from("<I", art, 12)[0]
        ):
            raise ValueError(f"Corrupt bundled Maker Brawl art: {kit_id}")
        target = Path(private_root) / "kits" / kit_id
        target.parent.mkdir(parents=True, exist_ok=True)
        temporary = target.with_name(target.name + ".installing")
        if temporary.exists():
            shutil.rmtree(temporary)
        shutil.copytree(source, temporary)
        if target.exists():
            shutil.rmtree(target)
        os.replace(temporary, target)
        installed.append(manifest)
    return installed


def _read_art(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < 64 or data[:4] != b"MLAR":
        raise ValueError(f"invalid Maker Lite art: {path}")
    version, cell_size, cell_count, player_base, expected_crc = (
        struct.unpack_from("<HHHHI", data, 4)
    )
    if (
        version not in {1, 2, 3, 4}
        or cell_size != 16
        or cell_count == 0
    ):
        raise ValueError("guest composition requires a supported MLAR kit")
    content = data[64:]
    pixel_size = cell_count * 512
    table_size = 0 if version == 1 else 224
    if (
        len(content) < pixel_size + table_size
        or zlib.crc32(content) & 0xFFFFFFFF != expected_crc
    ):
        raise ValueError("guest art checksum or bounds failed")
    if version == 1:
        animations = b"".join(
            struct.pack(
                "<HBB",
                min(player_base + index % 14, cell_count - 1),
                1,
                6,
            )
            for index in range(56)
        )
    else:
        animations = content[pixel_size : pixel_size + 224]
    frames = []
    if version == 4:
        frame_count, reserved = struct.unpack_from(
            "<HH", content, pixel_size + 224
        )
        frame_data = content[pixel_size + 228 :]
        if reserved or len(frame_data) != frame_count * 36:
            raise ValueError("guest metasprite table is invalid")
        frames = [
            frame_data[index * 36 : (index + 1) * 36]
            for index in range(frame_count)
        ]
    return {
        "header": data[:64],
        "cells": [
            content[index * 512 : (index + 1) * 512]
            for index in range(cell_count)
        ],
        "animations": animations,
        "frames": frames,
        "player_base": player_base,
    }


def _single_cell_frame(cell: int) -> bytes:
    return struct.pack(
        "<BBbb16H",
        1,
        1,
        -8,
        -16,
        cell,
        *([0xFFFF] * 15),
    )


def compose_private_guest_kit(
    private_root: str,
    player_kit_id: str,
    opponent_kit_id: str,
) -> dict:
    """Compose two locally installed player kits into a private brawl kit."""

    private = Path(private_root)
    player_dir = private / "kits" / os.path.basename(player_kit_id)
    opponent_dir = private / "kits" / os.path.basename(opponent_kit_id)
    if player_dir == opponent_dir:
        raise ValueError("choose two different guest kits")
    try:
        player_manifest = json.loads(
            (player_dir / "kit.mlk").read_text(encoding="utf-8")
        )
        opponent_manifest = json.loads(
            (opponent_dir / "kit.mlk").read_text(encoding="utf-8")
        )
    except (OSError, ValueError) as exc:
        raise ValueError("guest kit manifests are unreadable") from exc
    player = _read_art(player_dir / "art.mla")
    opponent = _read_art(opponent_dir / "art.mla")

    cells = list(player["cells"])
    frames = list(player["frames"])
    if not frames:
        animation_limit = max(
            struct.unpack_from("<H", player["animations"], index * 4)[0]
            + player["animations"][index * 4 + 2]
            for index in range(56)
        )
        frames = [
            _single_cell_frame(index)
            for index in range(animation_limit)
        ]

    opponent_start = struct.unpack_from("<H", opponent["animations"], 0)[0]
    opponent_preview_cell = len(cells)
    if opponent["frames"]:
        if opponent_start >= len(opponent["frames"]):
            raise ValueError("opponent idle frame is outside its art table")
        descriptor = opponent["frames"][opponent_start]
        columns, rows, offset_x, offset_y, *source_cells = struct.unpack(
            "<BBbb16H",
            descriptor,
        )
        used = columns * rows
        remapped = []
        for source_cell in source_cells[:used]:
            if source_cell == 0xFFFF:
                remapped.append(0xFFFF)
                continue
            if source_cell >= len(opponent["cells"]):
                raise ValueError("opponent frame refers outside its atlas")
            remapped.append(len(cells))
            cells.append(opponent["cells"][source_cell])
        remapped.extend([0xFFFF] * (16 - len(remapped)))
        opponent_frame = struct.pack(
            "<BBbb16H",
            columns,
            rows,
            offset_x,
            offset_y,
            *remapped,
        )
    else:
        if opponent_start >= len(opponent["cells"]):
            raise ValueError("opponent idle cell is outside its atlas")
        opponent_frame = _single_cell_frame(len(cells))
        cells.append(opponent["cells"][opponent_start])
    opponent_frame_index = len(frames)
    frames.append(opponent_frame)
    if len(cells) > 1536 or len(frames) > 512:
        raise ValueError("composed guest kit exceeds Maker Lite art bounds")

    identity = hashlib.sha256(
        (player_kit_id + "\0" + opponent_kit_id).encode("utf-8")
    ).hexdigest()
    kit_id = f"brawl-{identity[:12]}"
    pixels = b"".join(cells)
    frame_table = struct.pack("<HH", len(frames), 0) + b"".join(frames)
    payload = pixels + player["animations"] + frame_table
    header = bytearray(player["header"])
    struct.pack_into(
        "<HHHHI",
        header,
        4,
        4,
        16,
        len(cells),
        player["player_base"],
        zlib.crc32(payload) & 0xFFFFFFFF,
    )
    header[16:48] = b"\0" * 32
    header[16 : 16 + len(kit_id)] = kit_id.encode("ascii")
    header[48 + 4] = 0

    player_name = str(
        player_manifest.get("player_name")
        or player_manifest.get("source", {}).get("title")
        or player_manifest.get("ruleset", "Player").title()
    )
    opponent_name = str(
        opponent_manifest.get("player_name")
        or opponent_manifest.get("source", {}).get("title")
        or opponent_manifest.get("ruleset", "Opponent").title()
    )
    catalog = list(player_manifest.get("asset_catalog", []))
    catalog.append(
        {
            "id": f"guest-{identity[:10]}",
            "label": opponent_name,
            "category": "Fighters",
            "type": "entity",
            "cell": opponent_preview_cell,
            "frame": opponent_frame_index,
            "kind": "enemy",
            "params": [14, 26, 100, 3],
            "flags": FIGHTER_METASPRITE_FLAG | 0x400,
        }
    )
    manifest = {
        "kit_id": kit_id,
        "ruleset": str(player_manifest.get("ruleset", "mario")),
        "revision_id": "private-brawl-composite-v1",
        "player_name": player_name,
        "cell_count": len(cells),
        "catalog_count": len(catalog),
        "entity_cells": {},
        "asset_catalog": catalog,
        "source": {
            "type": "private-composite",
            "player_kit": player_kit_id,
            "opponent_kit": opponent_kit_id,
            "player_art_sha256": hashlib.sha256(
                (player_dir / "art.mla").read_bytes()
            ).hexdigest(),
            "opponent_art_sha256": hashlib.sha256(
                (opponent_dir / "art.mla").read_bytes()
            ).hexdigest(),
        },
    }
    target = private / "kits" / kit_id
    target.mkdir(parents=True, exist_ok=True)
    (target / "art.mla").write_bytes(bytes(header) + payload)
    (target / "kit.mlk").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    (target / "provenance.tsv").write_text(
        "role\tkit_id\tart_sha256\n"
        f"player\t{player_kit_id}\t{manifest['source']['player_art_sha256']}\n"
        f"opponent\t{opponent_kit_id}\t"
        f"{manifest['source']['opponent_art_sha256']}\n",
        encoding="utf-8",
    )
    cover = player_dir / str(player_manifest.get("source_cover_file", ""))
    if cover.is_file():
        shutil.copy2(cover, target / cover.name)
        manifest["source_cover_file"] = cover.name
        (target / "kit.mlk").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    return manifest
