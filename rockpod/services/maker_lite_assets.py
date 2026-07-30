"""Private, lossless Maker Lite art-kit import.

Commercial pixels are accepted only from a user-selected extraction bundle.
The source ROM is hashed and identified but is never copied into Rockpod or
onto the iPod.
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import struct
import tempfile
import wave
import zlib
from dataclasses import dataclass
from pathlib import Path

try:
    from PIL import Image
except ImportError:  # pragma: no cover - the UI reports this directly
    Image = None


RULESETS = {"mario": 1, "zelda": 2, "sonic": 3}
ART_MAGIC = b"MLAR"
ART_VERSION = 4
ART_HEADER_SIZE = 64
CELL_SIZE = 16
MAX_CELLS = 1536
MAX_PLAYER_FRAMES = 512
PLAYER_FRAME_CELLS = 16
PLAYER_FRAME_SIZE = 36
ACTION_NAMES = (
    "idle",
    "walk",
    "run",
    "jump",
    "fall",
    "crouch",
    "skid",
    "swim",
    "sword",
    "item",
    "roll",
    "spindash",
    "hurt",
    "complete",
)
ANIMATION_ENTRY_SIZE = 4
DIRECTION_NAMES = ("right", "down", "left", "up")
ANIMATION_TABLE_SIZE = (
    len(ACTION_NAMES) * len(DIRECTION_NAMES) * ANIMATION_ENTRY_SIZE
)
EFFECT_NAMES = ("jump", "collect", "hurt", "goal", "action", "spring")
ENTITY_KINDS = {
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
CATALOG_ID_PATTERN = re.compile(r"^[a-z0-9][a-z0-9_-]{0,31}$")
COLLISION_NAMES = {
    "solid",
    "hazard",
    "one_way",
    "slope_up",
    "slope_down",
    "water",
    "climb",
    "loop",
}
MAX_CATALOG_ASSETS = 1024
SOURCE_EXTENSIONS = {".sfc", ".smc", ".md", ".gen", ".bin"}
RECIPE_FILENAMES = {
    "maker-lite-extract.json",
    "maker_lite_extract.json",
}
BUILTIN_EXTRACTION_REVISIONS = {
    "smw-us-1.0",
    "alttp-us-1.0",
    "sonic3-us",
}
BUNDLED_NEON_NOOK_KIT_ID = "zelda-neon-nook-v1"
BUNDLED_NEON_NOOK_ROOT = (
    Path(__file__).resolve().parents[2]
    / "assets"
    / "maker_lite"
    / "neon_nook"
    / "pack"
)

# Super Mario World's clean-room reconstruction (snesrev/smw) identifies
# these original US-ROM structures. Addresses are SNES LoROM addresses; the
# helper below converts them only after the user's image passes the published
# SHA-1 check.
SMW_GFX_POINTER_LO = 0x00B992
SMW_GFX_POINTER_HI = 0x00B9C4
SMW_GFX_POINTER_BANK = 0x00B9F6
SMW_GFX32_POINTER = 0x00B8D8
SMW_GFX33_POINTER = 0x00B88B
SMW_PLAYER_HEAD_POINTERS = 0x00E00C
SMW_PLAYER_BODY_POINTERS = 0x00E0CC
SMW_PLAYER_PALETTES = 0x00B2C8
SMW_FOREGROUND_PALETTES = 0x00B190
SMW_OBJECT_PALETTES = 0x00B250
SMW_SPRITE_PALETTES = 0x00B318
SMW_LAYER3_PALETTES = 0x00B170
SMW_BACKGROUND_PALETTES = 0x00B0B0
SMW_YOSHI_BERRY_PALETTES = 0x00B674
SMW_MAP16_SELECTION = 0x0581BB
SMW_GRASS_FG_GFX = (0x14, 0x17, 0x19, 0x15)
SMW_GRASS_SPRITE_GFX = (0x00, 0x01, 0x13, 0x02)

# A Link to the Past structures identified by snesrev/zelda3. These values
# describe layout only; all resulting pixels are decoded from the user's
# hash-verified US cartridge into the ignored private kit directory.
ALTTP_LINK_GRAPHICS = 0x108000
ALTTP_LINK_PALETTE = (
    0,
    0x7FFF,
    0x237E,
    0x11B7,
    0x369E,
    0x14A5,
    0x01FF,
    0x1078,
    0x599D,
    0x3647,
    0x3B68,
    0x0A4A,
    0x12EF,
    0x2A5C,
    0x1571,
    0x7A18,
)
ALTTP_LINK_DMA_SOURCES = (
    # down, up, left, right movement groups from NMI_PrepareSprites
    (
        (0x8080, 0x8840),
        (0x8080, 0x8800),
        (0x8080, 0x8580),
        (0x8080, 0x8800),
        (0x8080, 0x8580),
    ),
    (
        (0x8040, 0x84C0),
        (0x8040, 0x8500),
        (0x8040, 0x8540),
        (0x8040, 0x8500),
        (0x8040, 0x8540),
    ),
    (
        (0x8000, 0x8400),
        (0x8000, 0x8440),
        (0x8000, 0x8480),
    ),
    (
        (0x8000, 0x8400),
        (0x8000, 0x8440),
        (0x8000, 0x8480),
    ),
)
ALTTP_SPRITE_POINTERS = {
    12: 0x14FFFC,
    21: 0x15AEC6,
    28: 0x15D394,
    82: 0x17D82F,
    83: 0x17DCEC,
}
ALTTP_SPRITE_POINTER_TABLE = (
    0x10F000, 0x10F600, 0x10FC00, 0x118200,
    0x118800, 0x118E00, 0x119400, 0x119A00,
    0x11A000, 0x11A600, 0x11AC00, 0x11B200,
    0x14FFFC, 0x1585D4, 0x158AB6, 0x158FBE,
    0x1593F8, 0x1599A6, 0x159F32, 0x15A3D7,
    0x15A8F1, 0x15AEC6, 0x15B418, 0x15B947,
    0x15BED0, 0x15C449, 0x15C975, 0x15CE7C,
    0x15D394, 0x15D8AC, 0x15DDC0, 0x15E34C,
    0x15E8E8, 0x15EE31, 0x15F3A6, 0x15F92D,
    0x15FEBA, 0x1682FF, 0x1688E0, 0x168E41,
    0x1692DF, 0x169883, 0x169CD0, 0x16A26E,
    0x16A275, 0x16A787, 0x16AA06, 0x16AE9D,
    0x16B3FF, 0x16B87E, 0x16BE6B, 0x16C13D,
    0x16C619, 0x16CBBB, 0x16D0F1, 0x16D641,
    0x16D95A, 0x16DD99, 0x16E278, 0x16E760,
    0x16ED25, 0x16F20F, 0x16F6B7, 0x16FA5F,
)
ALTTP_BG_STARTER_POINTERS = (
    0x11B800,
    0x11BCE2,
    0x11C15F,
    0x11C675,
    0x11CB84,
    0x11CF4C,
    0x11D2CE,
    0x11D726,
)

# Verified offsets in the US Sonic 3 cartridge. They identify original console
# structures, not copied art: pixels are decoded only from the user's ROM into
# the ignored private kit directory.
SONIC3_PLAYER_ART = 0x100000
SONIC3_PLAYER_PALETTE = 0x8C234
SONIC3_MAP_TABLE = 0x140FE0
SONIC3_DPLC_TABLE = 0x1428AC
SONIC3_FRAME_COUNT = 218
SONIC3_PRIVATE_STREAMS = {
    "ring": 0x15D1E6,
    "starpost": 0x15D8A2,
    "signpost": 0x15DA5A,
    "spikes_springs": 0x15EFFC,
    "monitor": 0x15D2DE,
    "aiz_cork_floor": 0x18D586,
}
SONIC3_AIZ_PALETTE = 0x8C374
SONIC3_RHINOBOT_ART = 0x16732A
SONIC3_RHINOBOT_ART_SIZE = 2720


class MakerLiteAssetError(ValueError):
    """A source or extraction bundle failed a safety/authenticity check."""


def _catalog_text(value, field, maximum):
    text = " ".join(str(value or "").split())
    if not text or len(text) > maximum or any(ord(char) < 32 for char in text):
        raise MakerLiteAssetError(
            f"Asset catalog {field} must contain 1-{maximum} printable characters"
        )
    return text


def validate_asset_catalog(
    catalog,
    cell_count: int,
    entity_cells: dict[str, int],
) -> list[dict]:
    """Validate editor parts against exact atlas cells and runtime semantics."""

    if catalog in (None, []):
        return []
    if not isinstance(catalog, list) or len(catalog) > MAX_CATALOG_ASSETS:
        raise MakerLiteAssetError(
            f"asset_catalog must contain at most {MAX_CATALOG_ASSETS} parts"
        )
    normalized = []
    seen = set()
    for index, source in enumerate(catalog):
        if not isinstance(source, dict):
            raise MakerLiteAssetError(
                f"asset_catalog[{index}] must be an object"
            )
        asset_id = str(source.get("id", "")).strip().lower()
        if not CATALOG_ID_PATTERN.fullmatch(asset_id) or asset_id in seen:
            raise MakerLiteAssetError(
                f"asset_catalog[{index}].id must be unique lowercase ASCII"
            )
        seen.add(asset_id)
        label = _catalog_text(source.get("label"), "label", 40)
        category = _catalog_text(source.get("category"), "category", 24)
        tool_type = str(source.get("type", "")).lower()
        try:
            cell = int(source.get("cell", -1))
        except (TypeError, ValueError) as exc:
            raise MakerLiteAssetError(
                f"asset_catalog[{index}].cell must be an integer"
            ) from exc
        if isinstance(source.get("cell"), bool) or not 0 < cell < cell_count:
            raise MakerLiteAssetError(
                f"asset_catalog[{index}].cell is outside the authentic atlas"
            )
        item = {
            "id": asset_id,
            "label": label,
            "category": category,
            "type": tool_type,
            "cell": cell,
        }
        if tool_type == "terrain":
            if cell >= MAX_CELLS:
                raise MakerLiteAssetError(
                    f"asset_catalog[{index}] terrain exceeds the device tile range"
                )
            collision = source.get("collision", [])
            if isinstance(collision, str):
                collision = [collision]
            if (
                not isinstance(collision, list)
                or any(str(name) not in COLLISION_NAMES for name in collision)
            ):
                raise MakerLiteAssetError(
                    f"asset_catalog[{index}] has an unknown collision type"
                )
            item["collision"] = [str(name) for name in collision]
        elif tool_type == "entity":
            kind = str(source.get("kind", "")).lower()
            if kind not in ENTITY_KINDS:
                raise MakerLiteAssetError(
                    f"asset_catalog[{index}] has an unknown entity kind"
                )
            params = source.get("params", [])
            if not isinstance(params, list) or len(params) > 4:
                raise MakerLiteAssetError(
                    f"asset_catalog[{index}].params must contain at most four integers"
                )
            try:
                params = [int(value) for value in params]
                flags = int(source.get("flags", 0))
                frame = (
                    int(source["frame"])
                    if "frame" in source
                    else None
                )
            except (TypeError, ValueError) as exc:
                raise MakerLiteAssetError(
                    f"asset_catalog[{index}] params and flags must be integers"
                ) from exc
            if (
                any(not -32768 <= value <= 32767 for value in params)
                or not 0 <= flags <= 16383
                or (frame is not None and not 0 <= frame < 512)
            ):
                raise MakerLiteAssetError(
                    f"asset_catalog[{index}] params or flags exceed device bounds"
                )
            item.update({"kind": kind, "params": params, "flags": flags})
            if frame is not None:
                if not flags & 0x2000:
                    raise MakerLiteAssetError(
                        f"asset_catalog[{index}].frame requires a "
                        "metasprite fighter flag"
                    )
                item["frame"] = frame
        else:
            raise MakerLiteAssetError(
                f"asset_catalog[{index}].type must be terrain or entity"
            )
        normalized.append(item)
    return normalized


@dataclass(frozen=True)
class SourceInfo:
    path: str
    sha256: str
    canonical_sha256: str
    sha1: str
    md5: str
    size: int
    canonical_size: int
    title: str
    platform: str
    copier_header: bool


@dataclass(frozen=True)
class SupportedRevision:
    ruleset: str
    revision_id: str
    algorithm: str
    digest: str
    platform: str


# These are revision fingerprints published by the upstream clean-room
# projects and verification catalogs used to steer extraction. SNES
# fingerprints apply after stripping a 512-byte copier header. Sonic 2 uses
# sonicretro/s2disasm's bit-perfect MD5 checks; Sonic 3 uses the catalogued US
# cartridge SHA-1.
SUPPORTED_REVISIONS = (
    SupportedRevision(
        "mario",
        "smw-us-1.0",
        "sha1",
        "6b47bb75d16514b6a476aa0c73a683a2a4c18765",
        "SNES",
    ),
    SupportedRevision(
        "zelda",
        "alttp-us-1.0",
        "sha256",
        "66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb",
        "SNES",
    ),
    SupportedRevision(
        "sonic",
        "sonic2-rev00",
        "md5",
        "8e2c29a1e65111fe2078359e685e7943",
        "Genesis",
    ),
    SupportedRevision(
        "sonic",
        "sonic2-rev01",
        "md5",
        "9feeb724052c39982d432a7851c98d3e",
        "Genesis",
    ),
    SupportedRevision(
        "sonic",
        "sonic3-us",
        "sha1",
        "75e9c4705259d84112b3e697a6c00a0813d47d71",
        "Genesis",
    ),
)


@dataclass(frozen=True)
class ImportedKit:
    kit_id: str
    ruleset: str
    directory: str
    source_sha256: str
    art_sha256: str
    cell_count: int


@dataclass(frozen=True)
class LocatedSource:
    path: str
    info: SourceInfo
    revision: SupportedRevision


@dataclass(frozen=True)
class LocatedRecipe:
    path: str
    ruleset: str
    revision_id: str


@dataclass(frozen=True)
class PrivateAssetInstallReport:
    installed: tuple[ImportedKit, ...]
    sources: tuple[LocatedSource, ...]
    recipes: tuple[LocatedRecipe, ...]
    missing_sources: tuple[str, ...]
    missing_recipes: tuple[str, ...]
    errors: tuple[str, ...]


def _sha256_file(path: str | os.PathLike[str]) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def inspect_source(path: str | os.PathLike[str]) -> SourceInfo:
    """Hash a user-selected reference ROM and read only its public header."""

    source_path = os.path.abspath(os.fspath(path))
    size = os.path.getsize(source_path)
    if size < 32 * 1024 or size > 8 * 1024 * 1024:
        raise MakerLiteAssetError("Reference image has an unexpected size")
    with open(source_path, "rb") as source:
        data = source.read()
    suffix = Path(source_path).suffix.lower()
    copier = 0
    canonical = data
    if suffix in {".sfc", ".smc"}:
        copier = 512 if len(data) % 1024 == 512 else 0
        canonical = data[copier:]
        candidates = []
        for offset in (0x7FC0, 0xFFC0):
            start = copier + offset
            if start + 21 <= len(data):
                raw_title = data[start : start + 21]
                title = raw_title.decode("ascii", "replace").rstrip("\0 ").strip()
                if title and all(0x20 <= byte < 0x7F for byte in raw_title.rstrip(b"\0 ")):
                    candidates.append(title)
        title = max(candidates, key=len) if candidates else "SNES reference"
        platform = "SNES"
    elif suffix in {".md", ".gen", ".bin"} and len(data) >= 0x150:
        title = data[0x120:0x150].decode("ascii", "replace").strip() or "Genesis reference"
        platform = "Genesis"
    else:
        raise MakerLiteAssetError("Choose an SNES or Genesis reference image")
    return SourceInfo(
        path=source_path,
        sha256=hashlib.sha256(data).hexdigest(),
        canonical_sha256=hashlib.sha256(canonical).hexdigest(),
        sha1=hashlib.sha1(canonical).hexdigest(),
        md5=hashlib.md5(canonical).hexdigest(),
        size=size,
        canonical_size=len(canonical),
        title=" ".join(title.split()),
        platform=platform,
        copier_header=bool(copier),
    )


def supported_revision(
    info: SourceInfo,
    ruleset: str,
    revisions: tuple[SupportedRevision, ...] = SUPPORTED_REVISIONS,
) -> SupportedRevision:
    """Resolve a source to one explicitly supported, published revision."""

    ruleset = str(ruleset).lower()
    digests = {
        "sha256": info.canonical_sha256,
        "sha1": info.sha1,
        "md5": info.md5,
    }
    for revision in revisions:
        if (
            revision.ruleset == ruleset
            and revision.platform == info.platform
            and digests.get(revision.algorithm) == revision.digest.lower()
        ):
            return revision
    expected = ", ".join(
        revision.revision_id
        for revision in revisions
        if revision.ruleset == ruleset
    ) or "no revisions"
    raise MakerLiteAssetError(
        f"Unsupported {ruleset or 'unknown'} source revision; expected {expected}"
    )


def detect_supported_revision(
    info: SourceInfo,
    revisions: tuple[SupportedRevision, ...] = SUPPORTED_REVISIONS,
) -> SupportedRevision:
    """Resolve a source without trusting its filename or a caller's ruleset."""

    digests = {
        "sha256": info.canonical_sha256,
        "sha1": info.sha1,
        "md5": info.md5,
    }
    matches = [
        revision
        for revision in revisions
        if (
            revision.platform == info.platform
            and digests.get(revision.algorithm) == revision.digest.lower()
        )
    ]
    if len(matches) != 1:
        raise MakerLiteAssetError("Reference image is not one supported game revision")
    return matches[0]


def _walk_private_files(
    roots,
    *,
    accepted,
    maximum: int,
):
    """Yield bounded, deterministic regular files without following symlinks."""

    pending = sorted(
        {
            os.path.realpath(os.path.abspath(os.fspath(root)))
            for root in roots
            if os.fspath(root)
        },
        reverse=True,
    )
    yielded = 0
    while pending:
        path = pending.pop()
        if os.path.isfile(path):
            if accepted(path):
                if yielded >= maximum:
                    raise MakerLiteAssetError(
                        f"Private asset scan exceeded its {maximum}-file safety limit"
                    )
                yield path
                yielded += 1
        elif os.path.isdir(path):
            try:
                entries = sorted(
                    os.scandir(path),
                    key=lambda entry: entry.name.casefold(),
                    reverse=True,
                )
            except OSError:
                continue
            for entry in entries:
                try:
                    if entry.is_symlink():
                        continue
                    if entry.is_dir(follow_symlinks=False):
                        pending.append(entry.path)
                    elif entry.is_file(follow_symlinks=False) and accepted(entry.path):
                        pending.append(entry.path)
                except OSError:
                    continue


def scan_supported_sources(
    roots,
    *,
    revisions: tuple[SupportedRevision, ...] = SUPPORTED_REVISIONS,
    maximum: int = 10000,
) -> list[LocatedSource]:
    """Find owned supported images by content hash, never by filename."""

    matches = []
    seen = set()
    for path in _walk_private_files(
        roots,
        accepted=lambda candidate: Path(candidate).suffix.lower()
        in SOURCE_EXTENSIONS,
        maximum=maximum,
    ):
        try:
            info = inspect_source(path)
            revision = detect_supported_revision(info, revisions)
        except (OSError, MakerLiteAssetError):
            continue
        key = (revision.ruleset, info.canonical_sha256)
        if key in seen:
            continue
        seen.add(key)
        matches.append(LocatedSource(path, info, revision))
    return sorted(
        matches,
        key=lambda match: (match.revision.ruleset, match.path.casefold()),
    )


def discover_extraction_recipes(
    roots,
    *,
    maximum: int = 2000,
) -> list[LocatedRecipe]:
    """Find complete, asset-free Maker Lite extraction recipe manifests."""

    result = []
    for path in _walk_private_files(
        roots,
        accepted=lambda candidate: (
            os.path.basename(candidate).lower() in RECIPE_FILENAMES
            or os.path.basename(candidate).lower().endswith(
                ".maker-lite-extract.json"
            )
        ),
        maximum=maximum,
    ):
        try:
            with open(path, "r", encoding="utf-8") as source:
                recipe = json.load(source)
        except (OSError, ValueError):
            continue
        if not isinstance(recipe, dict):
            continue
        ruleset = str(recipe.get("ruleset", "")).lower()
        revision_id = str(recipe.get("revision_id", "")).strip()
        cells = recipe.get("cells")
        if (
            ruleset not in RULESETS
            or not revision_id
            or not isinstance(cells, list)
            or not 14 <= len(cells) <= MAX_CELLS
            or not recipe.get("asset_catalog")
        ):
            continue
        result.append(LocatedRecipe(path, ruleset, revision_id))
    return sorted(result, key=lambda recipe: recipe.path.casefold())


def install_private_asset_suite(
    source_roots,
    recipe_roots,
    private_root: str | os.PathLike[str],
    *,
    revisions: tuple[SupportedRevision, ...] = SUPPORTED_REVISIONS,
) -> PrivateAssetInstallReport:
    """Match verified owned sources to complete recipes and install all kits."""

    sources = scan_supported_sources(source_roots, revisions=revisions)
    recipes = discover_extraction_recipes(recipe_roots)
    installed = []
    errors = []
    missing_recipes = []
    by_revision = {}
    for recipe in recipes:
        by_revision.setdefault((recipe.ruleset, recipe.revision_id), []).append(recipe)
    for match in sources:
        key = (match.revision.ruleset, match.revision.revision_id)
        candidates = by_revision.get(key, [])
        if (
            not candidates
            and match.revision.revision_id in BUILTIN_EXTRACTION_REVISIONS
        ):
            try:
                extractor = {
                    "smw-us-1.0": import_builtin_smw_kit,
                    "alttp-us-1.0": import_builtin_alttp_kit,
                    "sonic3-us": import_builtin_sonic3_kit,
                }[match.revision.revision_id]
                installed.append(
                    extractor(match.path, private_root, revisions=revisions)
                )
            except (OSError, MakerLiteAssetError) as exc:
                errors.append(f"{match.revision.revision_id}: {exc}")
            continue
        if not candidates:
            missing_recipes.append(match.revision.revision_id)
            continue
        if len(candidates) != 1:
            errors.append(
                f"{match.revision.revision_id}: found {len(candidates)} matching recipes"
            )
            continue
        try:
            installed.append(
                import_extraction_recipe(
                    match.path,
                    candidates[0].path,
                    private_root,
                    revisions=revisions,
                )
            )
        except (OSError, MakerLiteAssetError) as exc:
            errors.append(f"{match.revision.revision_id}: {exc}")
    source_rulesets = {match.revision.ruleset for match in sources}
    missing_sources = tuple(
        ruleset for ruleset in sorted(RULESETS) if ruleset not in source_rulesets
    )
    return PrivateAssetInstallReport(
        installed=tuple(installed),
        sources=tuple(sources),
        recipes=tuple(recipes),
        missing_sources=missing_sources,
        missing_recipes=tuple(sorted(set(missing_recipes))),
        errors=tuple(errors),
    )


def _rgb565(red: int, green: int, blue: int) -> int:
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def _atlas_payload(image_path: str) -> tuple[bytes, int]:
    if Image is None:
        raise MakerLiteAssetError("Pillow is required to import a private art kit")
    with Image.open(image_path) as source:
        image = source.convert("RGBA")
    width, height = image.size
    if width % CELL_SIZE or height % CELL_SIZE:
        raise MakerLiteAssetError("Atlas dimensions must be exact multiples of 16")
    columns = width // CELL_SIZE
    rows = height // CELL_SIZE
    cell_count = columns * rows
    if not 1 <= cell_count <= MAX_CELLS:
        raise MakerLiteAssetError("Atlas cell count is outside the device limit")

    payload = bytearray()
    pixels = image.load()
    for cell_y in range(rows):
        for cell_x in range(columns):
            for y in range(CELL_SIZE):
                for x in range(CELL_SIZE):
                    red, green, blue, alpha = pixels[
                        cell_x * CELL_SIZE + x,
                        cell_y * CELL_SIZE + y,
                    ]
                    value = 0xF81F if alpha == 0 else _rgb565(red, green, blue)
                    payload.extend(struct.pack("<H", value))
    return bytes(payload), cell_count


def _workspace_file(workspace: str, relative: object) -> str:
    name = os.fspath(relative)
    if not name or os.path.isabs(name):
        raise MakerLiteAssetError("Extraction recipe files must be relative paths")
    candidate = os.path.realpath(os.path.join(workspace, name))
    if os.path.commonpath((workspace, candidate)) != workspace:
        raise MakerLiteAssetError("Extraction recipe file escapes its workspace")
    if not os.path.isfile(candidate):
        raise MakerLiteAssetError(f"Extraction recipe file is missing: {name}")
    return candidate


def _expand_5(value: int) -> int:
    return (value << 3) | (value >> 2)


def _decode_snes_palette(data: bytes, offset: int) -> list[tuple[int, int, int, int]]:
    if offset < 0 or offset + 32 > len(data):
        raise MakerLiteAssetError("SNES palette range is outside its source file")
    colors = []
    for index in range(16):
        value = struct.unpack_from("<H", data, offset + index * 2)[0]
        colors.append(
            (
                _expand_5(value & 0x1F),
                _expand_5((value >> 5) & 0x1F),
                _expand_5((value >> 10) & 0x1F),
                255,
            )
        )
    return colors


def _lorom_offset(address: int, size: int = 1) -> int:
    """Translate one bounded SNES LoROM address to a headerless file offset."""

    if (
        not 0 <= address <= 0xFFFFFF
        or not 0 <= size <= 0x8000
        or (address & 0x8000) == 0
        or (address & 0x7FFF) + size > 0x8000
    ):
        raise MakerLiteAssetError("SNES LoROM range is invalid or crosses a bank")
    return ((address >> 16) & 0x7F) * 0x8000 + (address & 0x7FFF)


def _lorom_bytes(data: bytes, address: int, size: int) -> bytes:
    offset = _lorom_offset(address, size)
    result = data[offset : offset + size]
    if len(result) != size:
        raise MakerLiteAssetError("SNES LoROM range exceeds the verified image")
    return result


def _lorom_u16(data: bytes, address: int) -> int:
    return struct.unpack("<H", _lorom_bytes(data, address, 2))[0]


def _snesrev_decompress(
    data: bytes,
    address: int,
    *,
    offset_is_be: bool = True,
) -> bytes:
    """Decode the bounded resource format shared by snesrev projects."""

    if address & 0x8000 == 0:
        raise MakerLiteAssetError("SNES compressed stream address is invalid")
    cursor = address
    output = bytearray()

    def take() -> int:
        nonlocal cursor
        try:
            offset = _lorom_offset(cursor)
        except MakerLiteAssetError as exc:
            raise MakerLiteAssetError(
                "SNES compressed stream is truncated"
            ) from exc
        if offset >= len(data):
            raise MakerLiteAssetError("SNES compressed stream is truncated")
        value = data[offset]
        cursor += 1
        if cursor & 0xFFFF == 0:
            cursor += 0x8000
        return value

    while True:
        tag = take()
        if tag == 0xFF:
            break
        command = tag & 0xE0
        if command == 0xE0:
            command = (tag << 3) & 0xE0
            length = (((tag & 3) << 8) | take()) + 1
        else:
            length = (tag & 0x1F) + 1
        if len(output) + length > 0x10000:
            raise MakerLiteAssetError("SMW resource exceeds its extraction limit")
        if command == 0:
            output.extend(take() for _ in range(length))
        elif command == 0x20:
            output.extend([take()] * length)
        elif command == 0x40:
            first, second = take(), take()
            output.extend((first, second)[index & 1] for index in range(length))
        elif command == 0x60:
            first = take()
            output.extend((first + index) & 0xFF for index in range(length))
        elif command & 0x80:
            source = (take() << 8) | take()
            if not offset_is_be:
                source = ((source >> 8) | (source << 8)) & 0xFFFF
            if source >= len(output):
                raise MakerLiteAssetError("SMW resource copy starts outside its output")
            for _ in range(length):
                if source >= len(output):
                    raise MakerLiteAssetError("SMW resource copy exceeds its output")
                output.append(output[source])
                source += 1
        else:
            raise MakerLiteAssetError("SNES resource uses an unknown command")
    return bytes(output)


def _smw_decompress(data: bytes, address: int) -> bytes:
    """Decode a bounded SMW resource stream from an owned ROM."""

    return _snesrev_decompress(data, address)


def _smw_gfx_file(data: bytes, file_id: int) -> bytes:
    if not 0 <= file_id < 50:
        raise MakerLiteAssetError("SMW GFX file id is outside the original table")
    low = _lorom_bytes(data, SMW_GFX_POINTER_LO + file_id, 1)[0]
    high = _lorom_bytes(data, SMW_GFX_POINTER_HI + file_id, 1)[0]
    bank = _lorom_bytes(data, SMW_GFX_POINTER_BANK + file_id, 1)[0]
    return _smw_decompress(data, bank << 16 | high << 8 | low)


def _smw_3bpp_to_4bpp(data: bytes) -> bytes:
    if len(data) % 24:
        raise MakerLiteAssetError("SMW GFX file is not whole 3bpp tiles")
    output = bytearray()
    for offset in range(0, len(data), 24):
        output.extend(data[offset : offset + 16])
        for row in range(8):
            output.extend((data[offset + 16 + row], 0))
    return bytes(output)


def _smw_vram_tiles(data: bytes, files: tuple[int, int, int, int]) -> bytes:
    """Recreate four 128-tile VRAM slots in their original upload order."""

    slots = [b""] * 4
    for index, file_id in enumerate(files):
        decoded = _smw_3bpp_to_4bpp(_smw_gfx_file(data, file_id))
        if len(decoded) != 128 * 32:
            raise MakerLiteAssetError("SMW GFX file does not contain 128 tiles")
        slots[3 - index] = decoded
    return b"".join(slots)


def _smw_level_palette(
    data: bytes,
    fg_setting: int = 0,
    sprite_setting: int = 0,
) -> list[tuple[int, int, int, int]]:
    """Recreate the CGRAM rows used by an original grassland level."""

    if not 0 <= fg_setting < 8 or not 0 <= sprite_setting < 8:
        raise MakerLiteAssetError("SMW palette setting is outside its original table")
    colors = [(0, 0, 0, 255)] * 256

    def words(address: int, count: int):
        raw = _lorom_bytes(data, address, count * 2)
        result = []
        for index in range(count):
            value = struct.unpack_from("<H", raw, index * 2)[0]
            result.append(
                (
                    _expand_5(value & 0x1F),
                    _expand_5((value >> 5) & 0x1F),
                    _expand_5((value >> 10) & 0x1F),
                    255,
                )
            )
        return result

    def load_rows(source, destination: int, width: int, rows: int):
        cursor = 0
        for row in range(rows):
            start = destination + row * 16
            colors[start : start + width] = source[cursor : cursor + width]
            cursor += width

    # BufferPalettesRoutines_Levels in snesrev/smw.
    for row in range(8):
        colors[row * 16 + 1] = (239, 247, 255, 255)
        colors[128 + row * 16 + 1] = (255, 255, 255, 255)
    load_rows(words(SMW_LAYER3_PALETTES, 16), 8, 8, 2)
    load_rows(words(SMW_OBJECT_PALETTES, 60), 66, 6, 10)
    palette_offsets = (0, 12, 24, 36, 48, 60, 72, 84)
    load_rows(
        words(
            SMW_FOREGROUND_PALETTES + palette_offsets[fg_setting] * 2,
            12,
        ),
        34,
        6,
        2,
    )
    load_rows(
        words(
            SMW_SPRITE_PALETTES + palette_offsets[sprite_setting] * 2,
            12,
        ),
        226,
        6,
        2,
    )
    load_rows(words(SMW_BACKGROUND_PALETTES, 12), 2, 6, 2)
    berries = words(SMW_YOSHI_BERRY_PALETTES, 21)
    load_rows(berries[:14], 41, 7, 2)
    load_rows(berries[14:], 153, 7, 1)
    return colors


def _decode_genesis_palette(data: bytes, offset: int) -> list[tuple[int, int, int, int]]:
    if offset < 0 or offset + 32 > len(data):
        raise MakerLiteAssetError("Genesis palette range is outside its source file")
    colors = []
    for index in range(16):
        value = struct.unpack_from(">H", data, offset + index * 2)[0]
        red = (value >> 1) & 0x7
        green = (value >> 5) & 0x7
        blue = (value >> 9) & 0x7
        colors.append(
            (
                (red << 5) | (red << 2) | (red >> 1),
                (green << 5) | (green << 2) | (green >> 1),
                (blue << 5) | (blue << 2) | (blue >> 1),
                255,
            )
        )
    return colors


def _decode_snes_tile(data: bytes, offset: int) -> list[int]:
    if offset < 0 or offset + 32 > len(data):
        raise MakerLiteAssetError("SNES tile range is outside its source file")
    result = []
    for y in range(8):
        plane0 = data[offset + y * 2]
        plane1 = data[offset + y * 2 + 1]
        plane2 = data[offset + 16 + y * 2]
        plane3 = data[offset + 16 + y * 2 + 1]
        for x in range(8):
            bit = 7 - x
            result.append(
                ((plane0 >> bit) & 1)
                | (((plane1 >> bit) & 1) << 1)
                | (((plane2 >> bit) & 1) << 2)
                | (((plane3 >> bit) & 1) << 3)
            )
    return result


def _snes_tile_image(
    art: bytes,
    tile_index: int,
    palette: list[tuple[int, int, int, int]],
    *,
    flip_x: bool = False,
    flip_y: bool = False,
):
    indexes = _decode_snes_tile(art, tile_index * 32)
    image = Image.new("RGBA", (8, 8), (0, 0, 0, 0))
    pixels = image.load()
    for y in range(8):
        for x in range(8):
            source_x = 7 - x if flip_x else x
            source_y = 7 - y if flip_y else y
            color_index = indexes[source_y * 8 + source_x]
            color = palette[color_index]
            if color_index == 0:
                color = (color[0], color[1], color[2], 0)
            pixels[x, y] = color
    return image


def _smw_map16_pointers(data: bytes) -> list[int]:
    """Rebuild vanilla tileset-zero's 512 Map16 descriptor pointers."""

    selection = _lorom_bytes(data, SMW_MAP16_SELECTION, 64)
    pointers = []
    common = 0x8000
    tileset = 0x8B70
    for byte in selection:
        for bit in range(7, -1, -1):
            if byte & (1 << bit):
                pointers.append(0x0D0000 | common)
                common += 8
            else:
                pointers.append(0x0D0000 | tileset)
                tileset += 8
    if len(pointers) != 512:
        raise MakerLiteAssetError("SMW Map16 pointer table is incomplete")
    tileset = 0x8A70
    for tile_id in (*range(452, 456), *range(492, 496)):
        pointers[tile_id] = 0x0D0000 | tileset
        tileset += 8
    return pointers


def _smw_map16_cell(
    data: bytes,
    vram: bytes,
    colors: list[tuple[int, int, int, int]],
    tile_id: int,
):
    if not 0 <= tile_id < 512:
        raise MakerLiteAssetError("SMW Map16 tile id is outside page 0/1")
    pointer = _smw_map16_pointers(data)[tile_id]
    descriptors = struct.unpack("<4H", _lorom_bytes(data, pointer, 8))
    image = Image.new("RGBA", (CELL_SIZE, CELL_SIZE), (0, 0, 0, 0))
    # Vanilla Map16 stores left column first, matching the SNES upload loop.
    positions = ((0, 0), (0, 8), (8, 0), (8, 8))
    for descriptor, position in zip(descriptors, positions):
        tile = descriptor & 0x3FF
        palette_index = (descriptor >> 10) & 7
        palette = colors[palette_index * 16 : palette_index * 16 + 16]
        image.alpha_composite(
            _snes_tile_image(
                vram,
                tile,
                palette,
                flip_x=bool(descriptor & 0x4000),
                flip_y=bool(descriptor & 0x8000),
            ),
            position,
        )
    return image


def _smw_player_memory(data: bytes) -> bytes:
    """Recreate the dynamic-player source buffer initialized by SMW."""

    gfx32_address = 0x080000 | _lorom_u16(data, SMW_GFX32_POINTER)
    gfx33_address = 0x080000 | _lorom_u16(data, SMW_GFX33_POINTER)
    gfx32 = _smw_decompress(data, gfx32_address)
    gfx33 = _smw_decompress(data, gfx33_address)
    memory = bytearray(0x10000)
    if 0x2000 + len(gfx33) > len(memory):
        raise MakerLiteAssetError("SMW GFX33 exceeds the player work buffer")
    memory[0x2000 : 0x2000 + len(gfx33)] = gfx33

    # GraphicsDecompressionRoutines_DecompressGFX32And33 (snesrev/smw).
    destination = 0xACFE
    source = 0x23FF
    while source >= 0:
        for _ in range(8):
            if source < 0:
                break
            if destination < 0x2000 or destination + 2 > len(memory):
                raise MakerLiteAssetError("SMW player rearrangement exceeds RAM")
            memory[destination : destination + 2] = memory[
                0x2000 + source : 0x2000 + source + 2
            ]
            source -= 1
            destination -= 2
        for _ in range(8):
            source -= 1
            if source < 0:
                break
            if destination < 0x2000 or destination + 2 > len(memory):
                raise MakerLiteAssetError("SMW player rearrangement exceeds RAM")
            memory[destination : destination + 2] = memory[
                0x2000 + source : 0x2000 + source + 2
            ]
            source -= 1
            destination -= 2
    if 0x2000 + len(gfx32) > len(memory):
        raise MakerLiteAssetError("SMW GFX32 exceeds the player work buffer")
    memory[0x2000 : 0x2000 + len(gfx32)] = gfx32
    return bytes(memory)


def _smw_dynamic_player_tile(
    memory: bytes,
    pointer_index: int,
    palette: list[tuple[int, int, int, int]],
):
    source = (
        0x2000
        + ((pointer_index & 0xF7) << 6)
        + (0x4000 if pointer_index & 8 else 0)
    )
    image = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    for tile_offset, position in (
        (0, (0, 0)),
        (32, (8, 0)),
        (0x200, (0, 8)),
        (0x220, (8, 8)),
    ):
        image.alpha_composite(
            _snes_tile_image(memory, (source + tile_offset) // 32, palette),
            position,
        )
    return image


def _smw_player_frame(data: bytes, memory: bytes, pose: int):
    """Compose one exact big-Mario head/body frame for an original pose id."""

    frame_index = 0x46 + pose
    if not 0 <= pose < 61 or frame_index >= 192:
        raise MakerLiteAssetError("SMW player pose is outside the big-Mario table")
    head = _lorom_bytes(data, SMW_PLAYER_HEAD_POINTERS + frame_index, 1)[0]
    body = _lorom_bytes(data, SMW_PLAYER_BODY_POINTERS + frame_index, 1)[0]
    colors = _smw_level_palette(data)
    player_words = _lorom_bytes(data, SMW_PLAYER_PALETTES, 20)
    player_palette = []
    for index in range(10):
        value = struct.unpack_from("<H", player_words, index * 2)[0]
        player_palette.append(
            (
                _expand_5(value & 0x1F),
                _expand_5((value >> 5) & 0x1F),
                _expand_5((value >> 10) & 0x1F),
                255,
            )
        )
    colors[0x86 : 0x90] = player_palette
    palette = colors[0x80:0x90]
    image = Image.new("RGBA", (16, 32), (0, 0, 0, 0))
    image.alpha_composite(
        _smw_dynamic_player_tile(memory, head, palette),
        (0, 0),
    )
    image.alpha_composite(
        _smw_dynamic_player_tile(memory, body, palette),
        (0, 16),
    )
    return image


def _smw_sprite_cell(
    vram: bytes,
    colors: list[tuple[int, int, int, int]],
    charnum: int,
    flags: int,
):
    """Render one original 16x16 SNES OAM sprite tile."""

    name_table = flags & 1
    base = name_table * 256 + charnum
    palette_index = 8 + ((flags >> 1) & 7)
    palette = colors[palette_index * 16 : palette_index * 16 + 16]
    image = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    for tile, position in (
        (base, (0, 0)),
        (base + 1, (8, 0)),
        (base + 16, (0, 8)),
        (base + 17, (8, 8)),
    ):
        image.alpha_composite(
            _snes_tile_image(
                vram,
                tile,
                palette,
                flip_x=bool(flags & 0x40),
                flip_y=bool(flags & 0x80),
            ),
            position,
        )
    return image


def _snes_palette_words(
    data: bytes,
    address: int,
    count: int,
) -> list[tuple[int, int, int, int]]:
    raw = _lorom_bytes(data, address, count * 2)
    colors = []
    for index in range(count):
        value = struct.unpack_from("<H", raw, index * 2)[0]
        colors.append(
            (
                _expand_5(value & 0x1F),
                _expand_5((value >> 5) & 0x1F),
                _expand_5((value >> 10) & 0x1F),
                255,
            )
        )
    return colors


def _alttp_link_segment(
    data: bytes,
    source: int,
    palette: list[tuple[int, int, int, int]],
):
    """Compose one 16x16 Link OAM segment from its original DMA rows."""

    image = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    for row, row_source in enumerate((source, source + 0x200)):
        art = _lorom_bytes(data, 0x100000 | row_source, 64)
        for column in range(2):
            image.alpha_composite(
                _snes_tile_image(art, column, palette),
                (column * 8, row * 8),
            )
    return image


def _alttp_link_frame(data: bytes, direction: int, frame: int):
    if not 0 <= direction < len(ALTTP_LINK_DMA_SOURCES):
        raise MakerLiteAssetError("ALttP Link direction is invalid")
    group = ALTTP_LINK_DMA_SOURCES[direction]
    if not 0 <= frame < len(group):
        raise MakerLiteAssetError("ALttP Link movement frame is invalid")
    colors = [
        (
            _expand_5(value & 0x1F),
            _expand_5((value >> 5) & 0x1F),
            _expand_5((value >> 10) & 0x1F),
            255,
        )
        for value in ALTTP_LINK_PALETTE
    ]
    top_source, bottom_source = group[frame]
    image = Image.new("RGBA", (16, 32), (0, 0, 0, 0))
    image.alpha_composite(
        _alttp_link_segment(data, top_source, colors),
        (0, 0),
    )
    image.alpha_composite(
        _alttp_link_segment(data, bottom_source, colors),
        (0, 16),
    )
    return image


def _alttp_3bpp_tileset(data: bytes, tileset: int) -> bytes:
    if 0 <= tileset < len(ALTTP_SPRITE_POINTER_TABLE):
        address = ALTTP_SPRITE_POINTER_TABLE[tileset]
    elif tileset in ALTTP_SPRITE_POINTERS:
        address = ALTTP_SPRITE_POINTERS[tileset]
    else:
        raise MakerLiteAssetError(
            f"ALttP sprite tileset {tileset} is not in the private starter map"
        )
    decoded = (
        _lorom_bytes(data, address, 0x600)
        if tileset < 12
        else _snesrev_decompress(data, address, offset_is_be=False)
    )
    if len(decoded) != 0x600:
        raise MakerLiteAssetError("ALttP sprite tileset has an invalid size")
    art = bytearray(_smw_3bpp_to_4bpp(decoded))
    if tileset in {0x52, 0x53, 0x5A, 0x5B, 0x5C, 0x5E, 0x5F}:
        for offset in range(0, len(art), 32):
            for row in range(8):
                art[offset + 16 + row * 2 + 1] = 0xFF
    return bytes(art)


def _alttp_sprite_palette(
    data: bytes,
    palette_index: int,
    subindex: int | None,
) -> list[tuple[int, int, int, int]]:
    subindex = 0 if subindex is None else subindex
    if palette_index == 0:
        source = 0x9BD39E + subindex * 14
    elif palette_index == 1:
        if subindex < 11:
            source = 0x9BD446 + subindex * 14
        else:
            source = 0x9BD734 + (subindex - 11) * 180
    elif 2 <= palette_index <= 9:
        word_offset = (
            subindex * 60
            + ((palette_index - 2) >> 1) * 15
            + (palette_index & 1) * 8
        )
        source = 0x9BD218 + word_offset * 2
    elif palette_index in {10, 12}:
        source = 0x9BD4E0 + subindex * 14
    elif palette_index == 13:
        source = 0x9BD446 + subindex * 14
    elif palette_index == 14:
        source = 0x9BD308 + 2
    elif palette_index == 15:
        source = 0x9BD308 + 18
    else:
        raise MakerLiteAssetError("ALttP sprite palette index is invalid")
    subset = _snes_palette_words(data, source, 7)
    palette = [(0, 0, 0, 0)] * 16
    base = 9 if palette_index & 1 else 1
    palette[base : base + 7] = subset
    return palette


def _alttp_sprite_cell(
    data: bytes,
    tileset: int,
    tile_x: int,
    tile_y: int,
    palette_index: int,
    subindex: int | None,
):
    if not 0 <= tile_x <= 14 or not 0 <= tile_y <= 2:
        raise MakerLiteAssetError("ALttP sprite cell is outside its source sheet")
    art = _alttp_3bpp_tileset(data, tileset)
    palette = _alttp_sprite_palette(data, palette_index, subindex)
    image = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    for x, y in ((0, 0), (1, 0), (0, 1), (1, 1)):
        image.alpha_composite(
            _snes_tile_image(
                art,
                (tile_y + y) * 16 + tile_x + x,
                palette,
            ),
            (x * 8, y * 8),
        )
    return image


def _alttp_background_cells(data: bytes):
    colors = [(0, 0, 0, 255), *_snes_palette_words(data, 0x9BE6C8, 7)]
    colors.extend([(0, 0, 0, 255)] * (16 - len(colors)))
    cells = []
    for source_set, pointer in enumerate(ALTTP_BG_STARTER_POINTERS):
        decoded = _snesrev_decompress(
            data,
            pointer,
            offset_is_be=False,
        )
        if len(decoded) != 0x600:
            raise MakerLiteAssetError(
                "ALttP overworld background set is invalid"
            )
        art = _smw_3bpp_to_4bpp(decoded)
        for tile_y in (0, 2):
            for tile_x in range(0, 16, 2):
                image = Image.new("RGBA", (16, 16), colors[0])
                for x, y in ((0, 0), (1, 0), (0, 1), (1, 1)):
                    tile = _snes_tile_image(
                        art,
                        (tile_y + y) * 16 + tile_x + x,
                        colors,
                    )
                    if tile.getextrema()[3][0] == 0:
                        opaque = Image.new("RGBA", tile.size, colors[0])
                        opaque.alpha_composite(tile)
                        tile = opaque
                    image.alpha_composite(tile, (x * 8, y * 8))
                cells.append((source_set, pointer, image))
    return cells


def _decode_genesis_tile(data: bytes, offset: int) -> list[int]:
    if offset < 0 or offset + 32 > len(data):
        raise MakerLiteAssetError("Genesis tile range is outside its source file")
    result = []
    for value in data[offset : offset + 32]:
        result.extend((value >> 4, value & 0xF))
    return result


def _be16(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 2 > len(data):
        raise MakerLiteAssetError("Genesis structure exceeds the verified ROM")
    return struct.unpack_from(">H", data, offset)[0]


def _signed8(value: int) -> int:
    return value - 0x100 if value & 0x80 else value


def _signed16(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


def _nemesis_decompress(data: bytes, offset: int) -> bytes:
    """Decode Sega Nemesis art from an owned image.

    This is a small Python transcription of Clownacy's independently written
    0BSD ClownNemesis decompressor. The output limit comes from the stream's
    tile count and all input reads remain bounded.
    """

    cursor = offset

    def take() -> int:
        nonlocal cursor
        if cursor >= len(data):
            raise MakerLiteAssetError("Nemesis stream is truncated")
        value = data[cursor]
        cursor += 1
        return value

    header = (take() << 8) | take()
    xor_mode = bool(header & 0x8000)
    tile_count = header & 0x7FFF
    if not 1 <= tile_count <= 2048:
        raise MakerLiteAssetError("Nemesis stream has an invalid tile count")

    codes: dict[tuple[int, int], tuple[int, int]] = {}
    nibble = 0
    value = take()
    while value != 0xFF:
        if value & 0x80:
            nibble = value & 0xF
            value = take()
            continue
        run = ((value >> 4) & 7) + 1
        bits = value & 0xF
        code = take()
        if not 1 <= bits <= 8 or code >= (1 << bits):
            raise MakerLiteAssetError("Nemesis stream has an invalid code table")
        key = (bits, code)
        if key in codes:
            raise MakerLiteAssetError("Nemesis stream repeats a code")
        codes[key] = (nibble, run)
        value = take()

    bits_available = 0
    bit_buffer = 0

    def pop_bit() -> int:
        nonlocal bits_available, bit_buffer
        if bits_available == 0:
            bit_buffer = take()
            bits_available = 8
        result = (bit_buffer >> 7) & 1
        bit_buffer = (bit_buffer << 1) & 0xFF
        bits_available -= 1
        return result

    def pop_bits(count: int) -> int:
        result = 0
        for _ in range(count):
            result = (result << 1) | pop_bit()
        return result

    nibbles = []
    target = tile_count * 64
    while len(nibbles) < target:
        code = 0
        definition = None
        for width in range(1, 9):
            code = (code << 1) | pop_bit()
            if width == 6 and code == 0x3F:
                run = pop_bits(3) + 1
                definition = (pop_bits(4), run)
                break
            definition = codes.get((width, code))
            if definition is not None:
                break
        if definition is None:
            raise MakerLiteAssetError("Nemesis stream contains an unknown code")
        value, run = definition
        if len(nibbles) + run > target:
            raise MakerLiteAssetError("Nemesis stream exceeds its declared size")
        nibbles.extend([value] * run)

    output = bytearray()
    previous = 0
    for start in range(0, len(nibbles), 8):
        word = 0
        for value in nibbles[start : start + 8]:
            word = (word << 4) | value
        if xor_mode:
            word ^= previous
            previous = word
        output.extend(word.to_bytes(4, "big"))
    return bytes(output)


def _genesis_cell(
    art: bytes,
    palette_data: bytes,
    palette_offset: int,
    tiles: tuple[int, int, int, int],
    *,
    transparent_index: int = 0,
):
    palette = _decode_genesis_palette(palette_data, palette_offset)
    cell = Image.new("RGBA", (CELL_SIZE, CELL_SIZE), (0, 0, 0, 0))
    pixels = cell.load()
    for quadrant, tile_index in enumerate(tiles):
        indexes = _decode_genesis_tile(art, tile_index * 32)
        origin_x = (quadrant & 1) * 8
        origin_y = (quadrant >> 1) * 8
        for y in range(8):
            for x in range(8):
                color_index = indexes[y * 8 + x]
                red, green, blue, alpha = palette[color_index]
                if color_index == transparent_index:
                    alpha = 0
                pixels[origin_x + x, origin_y + y] = (
                    red,
                    green,
                    blue,
                    alpha,
                )
    return cell


def _sonic3_dplc_tiles(rom: bytes, frame_id: int) -> list[int]:
    pointer = SONIC3_DPLC_TABLE + _be16(
        rom, SONIC3_DPLC_TABLE + frame_id * 2
    )
    count = _be16(rom, pointer)
    if count > 32:
        raise MakerLiteAssetError("Sonic 3 DPLC frame is invalid")
    result = []
    for index in range(count):
        descriptor = _be16(rom, pointer + 2 + index * 2)
        source = descriptor & 0xFFF
        length = (descriptor >> 12) + 1
        if source + length > 0x1000:
            raise MakerLiteAssetError("Sonic 3 DPLC tile range is invalid")
        result.extend(range(source, source + length))
    return result


def _sonic3_player_frame(rom: bytes, frame_id: int):
    if not 0 <= frame_id < SONIC3_FRAME_COUNT:
        raise MakerLiteAssetError("Sonic 3 frame is outside the mapping table")
    loaded = _sonic3_dplc_tiles(rom, frame_id)
    pointer = SONIC3_MAP_TABLE + _be16(
        rom, SONIC3_MAP_TABLE + frame_id * 2
    )
    piece_count = _be16(rom, pointer)
    if piece_count > 16:
        raise MakerLiteAssetError("Sonic 3 mapping frame has too many pieces")
    pieces = []
    left = top = 32767
    right = bottom = -32768
    for index in range(piece_count):
        cursor = pointer + 2 + index * 6
        y = _signed8(rom[cursor])
        size = rom[cursor + 1]
        attributes = _be16(rom, cursor + 2)
        x = _signed16(_be16(rom, cursor + 4))
        columns = ((size >> 2) & 3) + 1
        rows = (size & 3) + 1
        tile = attributes & 0x7FF
        if tile + columns * rows > len(loaded):
            raise MakerLiteAssetError("Sonic 3 mapping exceeds its DPLC tiles")
        pieces.append((x, y, columns, rows, tile, attributes))
        left = min(left, x)
        top = min(top, y)
        right = max(right, x + columns * 8)
        bottom = max(bottom, y + rows * 8)
    if not pieces:
        return Image.new("RGBA", (16, 16), (0, 0, 0, 0)), 0, 0
    left = (left // 16) * 16
    top = (top // 16) * 16
    right = ((right + 15) // 16) * 16
    bottom = ((bottom + 15) // 16) * 16
    if not 1 <= (right - left) // 16 <= 4 or not 1 <= (bottom - top) // 16 <= 4:
        raise MakerLiteAssetError("Sonic 3 frame exceeds the metasprite budget")
    palette = _decode_genesis_palette(rom, SONIC3_PLAYER_PALETTE)
    image = Image.new("RGBA", (right - left, bottom - top), (0, 0, 0, 0))
    pixels = image.load()
    for x, y, columns, rows, tile, attributes in pieces:
        flip_x = bool(attributes & 0x0800)
        flip_y = bool(attributes & 0x1000)
        for column in range(columns):
            for row in range(rows):
                source_column = columns - 1 - column if flip_x else column
                source_row = rows - 1 - row if flip_y else row
                # The Mega Drive VDP advances sprite tiles down each column.
                loaded_index = tile + source_column * rows + source_row
                source_tile = loaded[loaded_index]
                indexes = _decode_genesis_tile(
                    rom,
                    SONIC3_PLAYER_ART + source_tile * 32,
                )
                for py in range(8):
                    for px in range(8):
                        source_x = 7 - px if flip_x else px
                        source_y = 7 - py if flip_y else py
                        color_index = indexes[source_y * 8 + source_x]
                        red, green, blue, alpha = palette[color_index]
                        if color_index == 0:
                            alpha = 0
                        pixels[
                            x - left + column * 8 + px,
                            y - top + row * 8 + py,
                        ] = (red, green, blue, alpha)
    return image, left, top


def _existing_library_cover(info: SourceInfo) -> str:
    """Return an already-owned Rockpod/Rockbox cover beside a source library."""

    source = Path(info.path)
    platform = "genesis" if info.platform == "Genesis" else "snes"
    for parent in source.parents:
        if parent.name != ".rockbox":
            continue
        candidate = (
            parent
            / "games"
            / "library"
            / "covers"
            / platform
            / f"{source.stem}.bmp"
        )
        if candidate.is_file():
            return str(candidate)
        break
    for suffix in (".png", ".jpg", ".jpeg", ".webp", ".bmp"):
        candidate = source.with_suffix(suffix)
        if candidate.is_file() and candidate != source:
            return str(candidate)
    return ""


def import_builtin_alttp_kit(
    source_path: str | os.PathLike[str],
    private_root: str | os.PathLike[str],
    *,
    revisions: tuple[SupportedRevision, ...] = SUPPORTED_REVISIONS,
) -> ImportedKit:
    """Build a private ALttP source kit from a hash-verified owned US ROM."""

    if Image is None:
        raise MakerLiteAssetError("Pillow is required to extract A Link to the Past")
    info = inspect_source(source_path)
    revision = supported_revision(info, "zelda", revisions)
    if revision.revision_id != "alttp-us-1.0":
        raise MakerLiteAssetError(
            "The built-in extractor requires A Link to the Past (USA)"
        )
    raw = Path(info.path).read_bytes()
    rom = raw[512:] if info.copier_header else raw
    if len(rom) != info.canonical_size:
        raise MakerLiteAssetError("ALttP copier-header normalization failed")

    cells = []
    provenance = []
    cell_keys = {}

    def add_cell(image, row: dict, *, deduplicate: bool = True) -> int:
        key = image.tobytes()
        if deduplicate and key in cell_keys:
            return cell_keys[key]
        index = len(cells)
        if index >= MAX_CELLS:
            raise MakerLiteAssetError("ALttP atlas exceeds the device budget")
        cells.append(image.copy())
        provenance.append({"index": index, **row})
        cell_keys.setdefault(key, index)
        return index

    direction_groups = {"right": 3, "down": 0, "left": 2, "up": 1}
    action_frames = {
        "idle": [0],
        "walk": [0, 1, 2],
        "run": [0, 1, 2],
        "jump": [0],
        "fall": [0],
        "crouch": [0],
        "skid": [0],
        "swim": [0, 1],
        "sword": [0, 1, 2],
        "item": [0],
        "roll": [0, 1, 2],
        "spindash": [0],
        "hurt": [0, 1],
        "complete": [0],
    }
    action_ticks = {
        "idle": 8,
        "walk": 6,
        "run": 4,
        "jump": 6,
        "fall": 6,
        "crouch": 8,
        "skid": 6,
        "swim": 6,
        "sword": 4,
        "item": 8,
        "roll": 4,
        "spindash": 6,
        "hurt": 6,
        "complete": 8,
    }
    player_frames = []
    animations = {}
    for action in ACTION_NAMES:
        directional = {}
        for direction in DIRECTION_NAMES:
            start = len(player_frames)
            group_index = direction_groups[direction]
            group = ALTTP_LINK_DMA_SOURCES[group_index]
            for source_frame in action_frames[action]:
                source_frame %= len(group)
                image = _alttp_link_frame(rom, group_index, source_frame)
                frame_cells = []
                for row in range(2):
                    frame_cells.append(
                        add_cell(
                            image.crop((0, row * 16, 16, (row + 1) * 16)),
                            {
                                "name": (
                                    f"link-{action}-{direction}-"
                                    f"{source_frame}-{row}"
                                ),
                                "extractor": "zelda3-link-dma",
                                "source_file": "@source",
                                "link_graphics": ALTTP_LINK_GRAPHICS,
                                "direction": direction,
                                "source_frame": source_frame,
                                "row": row,
                            },
                        )
                    )
                player_frames.append(
                    {
                        "columns": 1,
                        "rows": 2,
                        "offset_x": -8,
                        "offset_y": -24,
                        "cells": frame_cells,
                    }
                )
            directional[direction] = {
                "frames": list(range(start, len(player_frames))),
                "ticks_per_frame": action_ticks[action],
            }
        animations[action] = directional

    terrain_cells = []
    terrain_sources = []
    for index, (source_set, pointer, image) in enumerate(
        _alttp_background_cells(rom)
    ):
        terrain_cells.append(
            add_cell(
                image,
                {
                    "name": (
                        f"background-set-{source_set:02d}-"
                        f"tile-{index % 16:02d}"
                    ),
                    "extractor": "zelda3-bg-3bpp",
                    "source_file": "@source",
                    "compressed_pointer": pointer,
                    "source_set": source_set,
                    "tile_group": index % 16,
                },
                deduplicate=False,
            )
        )
        terrain_sources.append((source_set, index % 16))

    octorok = add_cell(
        _alttp_sprite_cell(rom, 12, 0, 0, 12, 1),
        {
            "name": "octorok",
            "extractor": "zelda3-sprite-sheet",
            "source_file": "@source",
            "sprite_id": 0x08,
            "tileset": 12,
            "sheet_cell": [0, 0],
            "palette": [12, 1],
        },
        deduplicate=False,
    )
    keese = add_cell(
        _alttp_sprite_cell(rom, 28, 0, 0, 8, None),
        {
            "name": "keese",
            "extractor": "zelda3-sprite-sheet",
            "source_file": "@source",
            "sprite_id": 0x6F,
            "tileset": 28,
            "sheet_cell": [0, 0],
            "palette": [8, 0],
        },
        deduplicate=False,
    )
    chest = add_cell(
        _alttp_sprite_cell(rom, 21, 14, 2, 8, None),
        {
            "name": "purple-chest",
            "extractor": "zelda3-sprite-sheet",
            "source_file": "@source",
            "sprite_id": 0xB4,
            "tileset": 21,
            "sheet_cell": [14, 2],
            "palette": [8, 0],
        },
        deduplicate=False,
    )
    crystal_switch = add_cell(
        _alttp_sprite_cell(rom, 82, 4, 2, 1, 19),
        {
            "name": "crystal-switch",
            "extractor": "zelda3-sprite-sheet",
            "source_file": "@source",
            "sprite_id": 0x1E,
            "tileset": 82,
            "sheet_cell": [4, 2],
            "palette": [1, 19],
        },
        deduplicate=False,
    )
    floor_switch = add_cell(
        _alttp_sprite_cell(rom, 83, 10, 0, 1, 12),
        {
            "name": "floor-switch",
            "extractor": "zelda3-sprite-sheet",
            "source_file": "@source",
            "sprite_id": 0x21,
            "tileset": 83,
            "sheet_cell": [10, 0],
            "palette": [1, 12],
        },
        deduplicate=False,
    )
    raw_sprite_cells = []
    generic_sprite_palette = _alttp_sprite_palette(rom, 8, None)
    for tileset in range(min(52, len(ALTTP_SPRITE_POINTER_TABLE))):
        art = _alttp_3bpp_tileset(rom, tileset)
        for tile_y in (0, 2):
            for tile_x in range(0, 16, 2):
                if len(cells) >= MAX_CELLS - 16:
                    break
                image = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
                for x, y in ((0, 0), (1, 0), (0, 1), (1, 1)):
                    image.alpha_composite(
                        _snes_tile_image(
                            art,
                            (tile_y + y) * 16 + tile_x + x,
                            generic_sprite_palette,
                        ),
                        (x * 8, y * 8),
                    )
                if image.getbbox() is None:
                    continue
                block = (tile_y // 2) * 8 + tile_x // 2
                cell = add_cell(
                    image,
                    {
                        "name": (
                            f"sprite-source-{tileset:02x}-"
                            f"{block:02d}"
                        ),
                        "extractor": "zelda3-sprite-sheet",
                        "source_file": "@source",
                        "tileset": tileset,
                        "sheet_cell": [tile_x, tile_y],
                        "palette": [8, 0],
                    },
                    deduplicate=False,
                )
                raw_sprite_cells.append((tileset, block, cell))

    columns = min(32, len(cells))
    while len(cells) % columns:
        add_cell(
            Image.new("RGBA", (CELL_SIZE, CELL_SIZE), (0, 0, 0, 0)),
            {
                "name": f"atlas-padding-{len(cells)}",
                "extractor": "transparent-padding",
                "source_file": "@source",
            },
            deduplicate=False,
        )
    atlas = Image.new(
        "RGBA",
        (columns * CELL_SIZE, (len(cells) // columns) * CELL_SIZE),
        (0, 0, 0, 0),
    )
    for index, cell in enumerate(cells):
        atlas.paste(
            cell,
            ((index % columns) * CELL_SIZE, (index // columns) * CELL_SIZE),
        )

    entity_cells = {
        "goal": chest,
        "enemy": octorok,
        "switch": crystal_switch,
        "item": chest,
    }
    asset_catalog = [
        *[
            {
                "id": f"bg-{source_set:02d}-{tile_index:02d}",
                "label": f"Source Set {source_set + 1} · Tile {tile_index + 1}",
                "category": f"World Set {source_set + 1}",
                "type": "terrain",
                "cell": cell,
                "collision": ["solid"],
            }
            for cell, (source_set, tile_index) in zip(
                terrain_cells,
                terrain_sources,
            )
        ],
        {
            "id": "octorok",
            "label": "Octorok",
            "category": "Enemies",
            "type": "entity",
            "cell": octorok,
            "kind": "enemy",
            "params": [16, 16, 72, 0],
            "flags": 0,
        },
        {
            "id": "keese",
            "label": "Keese",
            "category": "Enemies",
            "type": "entity",
            "cell": keese,
            "kind": "enemy",
            "params": [12, 12, 88, 0],
            "flags": 0,
        },
        {
            "id": "purple-chest",
            "label": "Purple Chest",
            "category": "Treasure",
            "type": "entity",
            "cell": chest,
            "kind": "item",
            "params": [16, 16, 0, 0],
            "flags": 0,
        },
        {
            "id": "treasure-goal",
            "label": "Treasure Goal",
            "category": "Goals",
            "type": "entity",
            "cell": chest,
            "kind": "goal",
            "params": [],
            "flags": 0,
        },
        {
            "id": "crystal-switch",
            "label": "Crystal Switch",
            "category": "Switches",
            "type": "entity",
            "cell": crystal_switch,
            "kind": "switch",
            "params": [0, 0, 0, 0],
            "flags": 0,
        },
        {
            "id": "floor-switch",
            "label": "Floor Switch",
            "category": "Switches",
            "type": "entity",
            "cell": floor_switch,
            "kind": "switch",
            "params": [0, 0, 0, 0],
            "flags": 0,
        },
        *[
            {
                "id": f"sprsrc-{tileset:02x}-{block:02d}",
                "label": (
                    f"Sprite Set 0x{tileset:02X} · "
                    f"Source {block + 1}"
                ),
                "category": f"Sprite Source 0x{tileset:02X}",
                "type": "terrain",
                "cell": cell,
                "collision": [],
            }
            for tileset, block, cell in raw_sprite_cells
        ],
    ]

    private_path = os.path.abspath(os.fspath(private_root))
    os.makedirs(private_path, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix=".maker-lite-alttp-",
        dir=private_path,
    ) as bundle:
        atlas.save(os.path.join(bundle, "atlas.png"), format="PNG", optimize=False)
        manifest = {
            "ruleset": "zelda",
            "revision_id": revision.revision_id,
            "source_sha256": info.sha256,
            "atlas": "atlas.png",
            "player_base": 0,
            "player_frames": player_frames,
            "entity_cells": entity_cells,
            "asset_catalog": asset_catalog,
            "animations": animations,
            "cells": provenance,
            "extractor_id": "rockpod-alttp-us",
            "extractor_version": 1,
            "mapping_authority": "snesrev/zelda3",
        }
        cover = _existing_library_cover(info)
        if cover:
            cover_name = "source-cover" + Path(cover).suffix.lower()
            shutil.copyfile(cover, os.path.join(bundle, cover_name))
            manifest["source_cover"] = cover_name
        _atomic_bytes(
            os.path.join(bundle, "kit.json"),
            json.dumps(manifest, indent=2, sort_keys=True).encode("utf-8")
            + b"\n",
        )
        return import_extraction_bundle(
            source_path,
            bundle,
            private_root,
            revisions=revisions,
        )


def import_builtin_smw_kit(
    source_path: str | os.PathLike[str],
    private_root: str | os.PathLike[str],
    *,
    revisions: tuple[SupportedRevision, ...] = SUPPORTED_REVISIONS,
) -> ImportedKit:
    """Build an authentic private SMW starter library from an owned US ROM."""

    if Image is None:
        raise MakerLiteAssetError("Pillow is required to extract Super Mario World")
    info = inspect_source(source_path)
    revision = supported_revision(info, "mario", revisions)
    if revision.revision_id != "smw-us-1.0":
        raise MakerLiteAssetError(
            "The built-in extractor requires Super Mario World (USA)"
        )
    raw = Path(info.path).read_bytes()
    rom = raw[512:] if info.copier_header else raw
    if len(rom) != info.canonical_size:
        raise MakerLiteAssetError("SMW copier-header normalization failed")

    foreground = _smw_vram_tiles(rom, SMW_GRASS_FG_GFX)
    sprites = _smw_vram_tiles(rom, SMW_GRASS_SPRITE_GFX)
    colors = _smw_level_palette(rom)
    player_memory = _smw_player_memory(rom)

    # Original pose ids selected by SetPlayerPose and related state handlers.
    action_poses = {
        "idle": [0],
        "walk": [0, 1, 2],
        "run": [0, 1, 2],
        "jump": [11],
        "fall": [12],
        "crouch": [60],
        "skid": [13],
        "swim": [24, 26],
        "sword": [14],
        "item": [29],
        "roll": [13],
        "spindash": [13],
        "hurt": [15],
        "complete": [38],
    }
    action_ticks = {
        "idle": 8,
        "walk": 5,
        "run": 3,
        "jump": 6,
        "fall": 6,
        "crouch": 8,
        "skid": 5,
        "swim": 5,
        "sword": 6,
        "item": 8,
        "roll": 4,
        "spindash": 4,
        "hurt": 8,
        "complete": 8,
    }

    cells = []
    provenance = []
    cell_keys = {}

    def add_cell(image, row: dict, *, deduplicate: bool = True) -> int:
        key = image.tobytes()
        if deduplicate and key in cell_keys:
            return cell_keys[key]
        index = len(cells)
        if index >= MAX_CELLS:
            raise MakerLiteAssetError("SMW atlas exceeds the device budget")
        cells.append(image.copy())
        provenance.append({"index": index, **row})
        cell_keys.setdefault(key, index)
        return index

    player_frames = []
    animations = {}
    for action in ACTION_NAMES:
        start = len(player_frames)
        for pose in action_poses[action]:
            image = _smw_player_frame(rom, player_memory, pose)
            frame_cells = []
            for row in range(2):
                frame_cells.append(
                    add_cell(
                        image.crop((0, row * 16, 16, (row + 1) * 16)),
                        {
                            "name": f"mario-pose-{pose:02d}-{row}",
                            "extractor": "smw-dynamic-player",
                            "source_file": "@source",
                            "gfx32_pointer": SMW_GFX32_POINTER,
                            "gfx33_pointer": SMW_GFX33_POINTER,
                            "head_table": SMW_PLAYER_HEAD_POINTERS,
                            "body_table": SMW_PLAYER_BODY_POINTERS,
                            "pose": pose,
                            "row": row,
                        },
                    )
                )
            player_frames.append(
                {
                    "columns": 1,
                    "rows": 2,
                    "offset_x": -8,
                    "offset_y": -31,
                    "cells": frame_cells,
                }
            )
        stop = len(player_frames)
        animations[action] = {
            "frames": list(range(start, stop)),
            "ticks_per_frame": action_ticks[action],
        }

    def add_map16(name: str, tile_id: int) -> int:
        return add_cell(
            _smw_map16_cell(rom, foreground, colors, tile_id),
            {
                "name": name,
                "extractor": "smw-map16-grassland",
                "source_file": "@source",
                "map16_selection": SMW_MAP16_SELECTION,
                "gfx_files": list(SMW_GRASS_FG_GFX),
                "map16_id": tile_id,
            },
            deduplicate=False,
        )

    ground = add_map16("grass-ledge", 0x100)
    dirt = add_map16("solid-dirt", 0x03F)
    turn_block = add_map16("turn-block", 0x11E)
    question_block = add_map16("question-block", 0x124)
    muncher = add_map16("muncher", 0x12F)
    cement = add_map16("cement-block", 0x130)
    used_block = add_map16("used-block", 0x132)
    pipe_left = add_map16("pipe-top-left", 0x033)
    pipe_right = add_map16("pipe-top-right", 0x034)
    coin = add_map16("coin", 0x02B)
    checkpoint = add_map16("midway-gate", 0x02F)
    goal = add_map16("goal-sign", 0x066)
    enemy = add_cell(
        _smw_sprite_cell(sprites, colors, 0xAA, 0x04),
        {
            "name": "galoomba",
            "extractor": "smw-oam-sprite",
            "source_file": "@source",
            "gfx_files": list(SMW_GRASS_SPRITE_GFX),
            "sprite_id": 0x0F,
            "charnum": 0xAA,
            "flags": 0x04,
            "mapping_authority": "snesrev/smw GenericSpriteOAMData",
        },
        deduplicate=False,
    )
    authored_map16 = {
        0x100,
        0x03F,
        0x11E,
        0x124,
        0x12F,
        0x130,
        0x132,
        0x033,
        0x034,
        0x02B,
        0x02F,
        0x066,
    }
    raw_map16 = []
    for tile_id in range(512):
        if len(cells) >= MAX_CELLS - 16:
            break
        if tile_id in authored_map16:
            continue
        image = _smw_map16_cell(rom, foreground, colors, tile_id)
        if image.getbbox() is None:
            continue
        cell = add_cell(
            image,
            {
                "name": f"map16-{tile_id:03x}",
                "extractor": "smw-map16-grassland",
                "source_file": "@source",
                "map16_selection": SMW_MAP16_SELECTION,
                "gfx_files": list(SMW_GRASS_FG_GFX),
                "map16_id": tile_id,
            },
            deduplicate=False,
        )
        raw_map16.append((tile_id, cell))

    columns = min(32, len(cells))
    while len(cells) % columns:
        add_cell(
            Image.new("RGBA", (CELL_SIZE, CELL_SIZE), (0, 0, 0, 0)),
            {
                "name": f"atlas-padding-{len(cells)}",
                "extractor": "transparent-padding",
                "source_file": "@source",
            },
            deduplicate=False,
        )
    atlas = Image.new(
        "RGBA",
        (columns * CELL_SIZE, (len(cells) // columns) * CELL_SIZE),
        (0, 0, 0, 0),
    )
    for index, cell in enumerate(cells):
        atlas.paste(
            cell,
            ((index % columns) * CELL_SIZE, (index // columns) * CELL_SIZE),
        )

    entity_cells = {
        "goal": goal,
        "collectible": coin,
        "enemy": enemy,
        "checkpoint": checkpoint,
        "block": question_block,
    }
    asset_catalog = [
        {
            "id": "grass-ledge",
            "label": "Grass Ledge",
            "category": "Ground",
            "type": "terrain",
            "cell": ground,
            "collision": ["solid"],
        },
        {
            "id": "solid-dirt",
            "label": "Solid Dirt",
            "category": "Ground",
            "type": "terrain",
            "cell": dirt,
            "collision": ["solid"],
        },
        {
            "id": "turn-block",
            "label": "Turn Block",
            "category": "Blocks",
            "type": "terrain",
            "cell": turn_block,
            "collision": ["solid"],
        },
        {
            "id": "question-block",
            "label": "Question Block",
            "category": "Blocks",
            "type": "entity",
            "cell": question_block,
            "kind": "block",
            "params": [1],
            "flags": 0,
        },
        {
            "id": "used-block",
            "label": "Used Block",
            "category": "Blocks",
            "type": "terrain",
            "cell": used_block,
            "collision": ["solid"],
        },
        {
            "id": "cement-block",
            "label": "Cement Block",
            "category": "Blocks",
            "type": "terrain",
            "cell": cement,
            "collision": ["solid"],
        },
        {
            "id": "pipe-left",
            "label": "Pipe Top Left",
            "category": "Pipes",
            "type": "terrain",
            "cell": pipe_left,
            "collision": ["solid"],
        },
        {
            "id": "pipe-right",
            "label": "Pipe Top Right",
            "category": "Pipes",
            "type": "terrain",
            "cell": pipe_right,
            "collision": ["solid"],
        },
        {
            "id": "muncher",
            "label": "Muncher",
            "category": "Hazards",
            "type": "terrain",
            "cell": muncher,
            "collision": ["hazard"],
        },
        {
            "id": "galoomba",
            "label": "Galoomba",
            "category": "Enemies",
            "type": "entity",
            "cell": enemy,
            "kind": "enemy",
            "params": [16, 16, 96, 0],
            "flags": 0,
        },
        {
            "id": "coin",
            "label": "Coin",
            "category": "Items",
            "type": "entity",
            "cell": coin,
            "kind": "collectible",
            "params": [1],
            "flags": 0,
        },
        {
            "id": "midway-gate",
            "label": "Midway Gate",
            "category": "Goals",
            "type": "entity",
            "cell": checkpoint,
            "kind": "checkpoint",
            "params": [],
            "flags": 0,
        },
        {
            "id": "goal-sign",
            "label": "Goal Sign",
            "category": "Goals",
            "type": "entity",
            "cell": goal,
            "kind": "goal",
            "params": [],
            "flags": 0,
        },
        *[
            {
                "id": f"map16-{tile_id:03x}",
                "label": f"Map16 0x{tile_id:03X}",
                "category": (
                    "Map16 Page 0" if tile_id < 256 else "Map16 Page 1"
                ),
                "type": "terrain",
                "cell": cell,
                "collision": ["solid"],
            }
            for tile_id, cell in raw_map16
        ],
    ]

    private_path = os.path.abspath(os.fspath(private_root))
    os.makedirs(private_path, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix=".maker-lite-smw-",
        dir=private_path,
    ) as bundle:
        atlas.save(os.path.join(bundle, "atlas.png"), format="PNG", optimize=False)
        manifest = {
            "ruleset": "mario",
            "revision_id": revision.revision_id,
            "source_sha256": info.sha256,
            "atlas": "atlas.png",
            "player_base": 0,
            "player_frames": player_frames,
            "entity_cells": entity_cells,
            "asset_catalog": asset_catalog,
            "animations": animations,
            "cells": provenance,
            "extractor_id": "rockpod-smw-us",
            "extractor_version": 1,
            "mapping_authority": "snesrev/smw",
        }
        cover = _existing_library_cover(info)
        if cover:
            cover_name = "source-cover" + Path(cover).suffix.lower()
            shutil.copyfile(cover, os.path.join(bundle, cover_name))
            manifest["source_cover"] = cover_name
        _atomic_bytes(
            os.path.join(bundle, "kit.json"),
            json.dumps(manifest, indent=2, sort_keys=True).encode("utf-8")
            + b"\n",
        )
        return import_extraction_bundle(
            source_path,
            bundle,
            private_root,
            revisions=revisions,
        )


def import_builtin_sonic3_kit(
    source_path: str | os.PathLike[str],
    private_root: str | os.PathLike[str],
    *,
    revisions: tuple[SupportedRevision, ...] = SUPPORTED_REVISIONS,
) -> ImportedKit:
    """Build the private Sonic 3 starter kit directly from an owned US ROM."""

    if Image is None:
        raise MakerLiteAssetError("Pillow is required to extract Sonic 3 art")
    info = inspect_source(source_path)
    revision = supported_revision(info, "sonic", revisions)
    if revision.revision_id != "sonic3-us":
        raise MakerLiteAssetError("The built-in extractor requires Sonic 3 (USA)")
    rom = Path(info.path).read_bytes()

    # These are original frame IDs used by Sonic 3's animation script. Maker
    # Lite stores each action contiguously so its bounded player can animate
    # without carrying the original animation interpreter onto the iPod.
    action_frames = {
        "idle": [0xBA, 0xBB, 0xBC],
        "walk": list(range(0x01, 0x09)),
        "run": list(range(0x21, 0x25)),
        "jump": list(range(0x96, 0x9B)),
        "fall": [0x9A, 0x99],
        "crouch": [0x9B, 0x9C],
        "skid": [0xA4, 0xA5, 0xA6],
        "swim": [0x91],
        "sword": [0xBA],
        "item": [0xBA],
        "roll": list(range(0x96, 0x9B)),
        "spindash": [0x86, 0x87, 0x86, 0x88, 0x86, 0x89, 0x86, 0x8A, 0x86, 0x8B],
        "hurt": [0x8F],
        "complete": [0xD0, 0xD1],
    }
    action_ticks = {
        "idle": 10,
        "walk": 4,
        "run": 3,
        "jump": 3,
        "fall": 4,
        "crouch": 8,
        "skid": 5,
        "swim": 6,
        "sword": 8,
        "item": 8,
        "roll": 3,
        "spindash": 2,
        "hurt": 8,
        "complete": 8,
    }

    cells = []
    provenance = []
    cell_keys = {}

    def add_cell(image, row: dict, *, deduplicate: bool = True) -> int:
        key = image.tobytes()
        if deduplicate and key in cell_keys:
            return cell_keys[key]
        index = len(cells)
        if index >= MAX_CELLS:
            raise MakerLiteAssetError("Sonic 3 atlas exceeds the device budget")
        cells.append(image.copy())
        provenance.append({"index": index, **row})
        cell_keys.setdefault(key, index)
        return index

    player_frames = []
    animations = {}
    for action in ACTION_NAMES:
        start = len(player_frames)
        for raw_frame in action_frames[action]:
            image, offset_x, offset_y = _sonic3_player_frame(rom, raw_frame)
            columns = image.width // CELL_SIZE
            rows = image.height // CELL_SIZE
            frame_cells = []
            for row in range(rows):
                for column in range(columns):
                    chunk = image.crop(
                        (
                            column * CELL_SIZE,
                            row * CELL_SIZE,
                            (column + 1) * CELL_SIZE,
                            (row + 1) * CELL_SIZE,
                        )
                    )
                    frame_cells.append(
                        add_cell(
                            chunk,
                            {
                                "name": f"sonic-{raw_frame:02x}-{column}-{row}",
                                "extractor": "sonic3-map-dplc",
                                "source_file": "@source",
                                "map_table": SONIC3_MAP_TABLE,
                                "dplc_table": SONIC3_DPLC_TABLE,
                                "art_offset": SONIC3_PLAYER_ART,
                                "palette_offset": SONIC3_PLAYER_PALETTE,
                                "frame": raw_frame,
                                "column": column,
                                "row": row,
                            },
                        )
                    )
            player_frames.append(
                {
                    "columns": columns,
                    "rows": rows,
                    "offset_x": offset_x,
                    "offset_y": offset_y,
                    "cells": frame_cells,
                }
            )
        stop = len(player_frames)
        animations[action] = {
            "frames": list(range(start, stop)),
            "ticks_per_frame": action_ticks[action],
        }

    def add_object_cell(
        name: str,
        stream_name: str,
        tiles: tuple[int, int, int, int],
        palette_offset: int,
    ) -> int:
        stream_offset = SONIC3_PRIVATE_STREAMS[stream_name]
        art = _nemesis_decompress(rom, stream_offset)
        result = add_cell(
            _genesis_cell(art, rom, palette_offset, tiles),
            {
                "name": name,
                "extractor": "clownnemesis-0bsd",
                "source_file": "@source",
                "stream_offset": stream_offset,
                "palette_offset": palette_offset,
                "tiles": list(tiles),
            },
            deduplicate=False,
        )
        return result

    terrain = add_object_cell(
        "angel-island-cork",
        "aiz_cork_floor",
        (0, 2, 1, 3),
        SONIC3_AIZ_PALETTE + 32,
    )
    spikes = add_object_cell(
        "spikes",
        "spikes_springs",
        (8, 12, 9, 13),
        SONIC3_PLAYER_PALETTE,
    )
    ring = add_object_cell(
        "ring",
        "ring",
        (0, 2, 1, 3),
        SONIC3_AIZ_PALETTE,
    )
    spring = add_object_cell(
        "red-spring",
        "spikes_springs",
        (17, 18, 24, 25),
        SONIC3_PLAYER_PALETTE,
    )
    checkpoint = add_object_cell(
        "starpost",
        "starpost",
        (6, 8, 7, 9),
        SONIC3_PLAYER_PALETTE,
    )
    item = add_object_cell(
        "monitor",
        "monitor",
        (5, 9, 6, 10),
        SONIC3_PLAYER_PALETTE,
    )
    goal = add_object_cell(
        "goal-sign",
        "signpost",
        (5, 9, 6, 10),
        SONIC3_PLAYER_PALETTE,
    )
    rhinobot_art = rom[
        SONIC3_RHINOBOT_ART : SONIC3_RHINOBOT_ART
        + SONIC3_RHINOBOT_ART_SIZE
    ]
    if len(rhinobot_art) != SONIC3_RHINOBOT_ART_SIZE:
        raise MakerLiteAssetError("Sonic 3 Rhinobot art is truncated")
    enemy = add_cell(
        _genesis_cell(
            rhinobot_art,
            rom,
            SONIC3_AIZ_PALETTE,
            (7, 11, 8, 12),
        ),
        {
            "name": "rhinobot",
            "extractor": "genesis4bpp-uncompressed",
            "source_file": "@source",
            "art_offset": SONIC3_RHINOBOT_ART,
            "palette_offset": SONIC3_AIZ_PALETTE,
            "tiles": [7, 11, 8, 12],
            "source_mapping": "sonicretro/skdisasm Map/DPLC - Rhinobot frame 0",
        },
        deduplicate=False,
    )
    source_palettes = {
        "ring": SONIC3_AIZ_PALETTE,
        "starpost": SONIC3_PLAYER_PALETTE,
        "signpost": SONIC3_PLAYER_PALETTE,
        "spikes_springs": SONIC3_PLAYER_PALETTE,
        "monitor": SONIC3_PLAYER_PALETTE,
        "aiz_cork_floor": SONIC3_AIZ_PALETTE + 32,
    }
    raw_source_parts = []
    for stream_name, stream_offset in SONIC3_PRIVATE_STREAMS.items():
        art = _nemesis_decompress(rom, stream_offset)
        palette_offset = source_palettes[stream_name]
        for block in range(len(art) // (4 * 32)):
            if len(cells) >= MAX_CELLS - 16:
                break
            base = block * 4
            image = _genesis_cell(
                art,
                rom,
                palette_offset,
                (base, base + 2, base + 1, base + 3),
            )
            if image.getbbox() is None:
                continue
            cell = add_cell(
                image,
                {
                    "name": f"{stream_name}-source-{block:03d}",
                    "extractor": "clownnemesis-0bsd",
                    "source_file": "@source",
                    "stream_offset": stream_offset,
                    "palette_offset": palette_offset,
                    "tiles": [base, base + 2, base + 1, base + 3],
                },
                deduplicate=False,
            )
            raw_source_parts.append((stream_name, block, cell))

    columns = min(32, len(cells))
    while len(cells) % columns:
        add_cell(
            Image.new("RGBA", (CELL_SIZE, CELL_SIZE), (0, 0, 0, 0)),
            {
                "name": f"atlas-padding-{len(cells)}",
                "extractor": "transparent-padding",
                "source_file": "@source",
            },
            deduplicate=False,
        )
    rows = len(cells) // columns
    atlas = Image.new(
        "RGBA",
        (columns * CELL_SIZE, rows * CELL_SIZE),
        (0, 0, 0, 0),
    )
    for index, cell in enumerate(cells):
        atlas.paste(
            cell,
            ((index % columns) * CELL_SIZE, (index // columns) * CELL_SIZE),
        )

    entity_cells = {
        "goal": goal,
        "collectible": ring,
        "enemy": enemy,
        "checkpoint": checkpoint,
        "spring": spring,
        "item": item,
    }
    asset_catalog = [
        {
            "id": "aiz-cork",
            "label": "Angel Island Cork",
            "category": "Ground",
            "type": "terrain",
            "cell": terrain,
            "collision": ["solid"],
        },
        {
            "id": "spikes",
            "label": "Spikes",
            "category": "Hazards",
            "type": "terrain",
            "cell": spikes,
            "collision": ["hazard"],
        },
        {
            "id": "rhinobot",
            "label": "Rhinobot",
            "category": "Enemies",
            "type": "entity",
            "cell": enemy,
            "kind": "enemy",
            "params": [16, 16, 160, 0],
            "flags": 0,
        },
        {
            "id": "ring",
            "label": "Ring",
            "category": "Items",
            "type": "entity",
            "cell": ring,
            "kind": "collectible",
            "params": [1],
            "flags": 0,
        },
        {
            "id": "red-spring",
            "label": "Red Spring",
            "category": "Gimmicks",
            "type": "entity",
            "cell": spring,
            "kind": "spring",
            "params": [16],
            "flags": 0,
        },
        {
            "id": "starpost",
            "label": "Starpost",
            "category": "Goals",
            "type": "entity",
            "cell": checkpoint,
            "kind": "checkpoint",
            "params": [],
            "flags": 0,
        },
        {
            "id": "monitor",
            "label": "Monitor",
            "category": "Items",
            "type": "entity",
            "cell": item,
            "kind": "item",
            "params": [1],
            "flags": 0,
        },
        {
            "id": "goal-sign",
            "label": "Goal Sign",
            "category": "Goals",
            "type": "entity",
            "cell": goal,
            "kind": "goal",
            "params": [],
            "flags": 0,
        },
        *[
            {
                "id": f"{stream_name[:12]}-{block:03d}",
                "label": (
                    f"{stream_name.replace('_', ' ').title()} "
                    f"Source {block + 1}"
                ),
                "category": (
                    "Source · " + stream_name.replace("_", " ").title()
                ),
                "type": "terrain",
                "cell": cell,
                "collision": [],
            }
            for stream_name, block, cell in raw_source_parts
        ],
    ]

    private_path = os.path.abspath(os.fspath(private_root))
    os.makedirs(private_path, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix=".maker-lite-sonic3-",
        dir=private_path,
    ) as bundle:
        atlas.save(os.path.join(bundle, "atlas.png"), format="PNG", optimize=False)
        manifest = {
            "ruleset": "sonic",
            "revision_id": revision.revision_id,
            "source_sha256": info.sha256,
            "atlas": "atlas.png",
            "player_base": 0,
            "player_frames": player_frames,
            "entity_cells": entity_cells,
            "asset_catalog": asset_catalog,
            "animations": animations,
            "cells": provenance,
            "extractor_id": "rockpod-sonic3-us",
            "extractor_version": 1,
            "mapping_authority": "sonicretro/skdisasm",
            "nemesis_authority": "Clownacy/clownnemesis (0BSD)",
        }
        cover = _existing_library_cover(info)
        if cover:
            cover_name = "source-cover" + Path(cover).suffix.lower()
            shutil.copyfile(cover, os.path.join(bundle, cover_name))
            manifest["source_cover"] = cover_name
        _atomic_bytes(
            os.path.join(bundle, "kit.json"),
            json.dumps(manifest, indent=2, sort_keys=True).encode("utf-8")
            + b"\n",
        )
        return import_extraction_bundle(
            source_path,
            bundle,
            private_root,
            revisions=revisions,
        )


def _tile_descriptor(value: object) -> tuple[int, bool, bool]:
    if isinstance(value, dict):
        index = int(value.get("index", -1))
        flip_x = bool(value.get("flip_x", False))
        flip_y = bool(value.get("flip_y", False))
    else:
        index = int(value)
        flip_x = False
        flip_y = False
    if index < 0:
        raise MakerLiteAssetError("Native tile indexes must be non-negative")
    return index, flip_x, flip_y


def _recipe_binary_path(
    workspace: str,
    value,
    source_info: SourceInfo | None,
) -> tuple[str, bool]:
    if str(value) == "@source":
        if source_info is None:
            raise MakerLiteAssetError("@source is unavailable for this recipe")
        return source_info.path, True
    return _workspace_file(workspace, value), False


def _native_cell(
    workspace: str,
    row: dict,
    kind: str,
    source_info: SourceInfo | None = None,
):
    art_path, art_is_source = _recipe_binary_path(
        workspace,
        row.get("file", ""),
        source_info,
    )
    palette_path, palette_is_source = _recipe_binary_path(
        workspace,
        row.get("palette_file", row.get("file", "")),
        source_info,
    )
    art = Path(art_path).read_bytes()
    palette_data = Path(palette_path).read_bytes()
    if kind == "snes4bpp" and source_info and source_info.copier_header:
        if art_is_source:
            art = art[512:]
        if palette_is_source:
            palette_data = palette_data[512:]
    base_offset = int(row.get("offset", 0))
    palette_offset = int(row.get("palette_offset", 0))
    descriptors = row.get("tiles")
    if not isinstance(descriptors, list) or len(descriptors) != 4:
        raise MakerLiteAssetError(
            "Native 16x16 cells require four 8x8 tile descriptors"
        )
    if kind == "snes4bpp":
        palette = _decode_snes_palette(palette_data, palette_offset)
        decode = _decode_snes_tile
    else:
        palette = _decode_genesis_palette(palette_data, palette_offset)
        decode = _decode_genesis_tile
    transparent = int(row.get("transparent_index", 0))
    if not 0 <= transparent < 16:
        raise MakerLiteAssetError("transparent_index must be from 0 through 15")
    cell = Image.new("RGBA", (CELL_SIZE, CELL_SIZE), (0, 0, 0, 0))
    pixels = cell.load()
    for quadrant, descriptor in enumerate(descriptors):
        tile_index, flip_x, flip_y = _tile_descriptor(descriptor)
        indexes = decode(art, base_offset + tile_index * 32)
        origin_x = (quadrant & 1) * 8
        origin_y = (quadrant >> 1) * 8
        for y in range(8):
            for x in range(8):
                source_x = 7 - x if flip_x else x
                source_y = 7 - y if flip_y else y
                color_index = indexes[source_y * 8 + source_x]
                color = palette[color_index]
                if color_index == transparent:
                    color = (color[0], color[1], color[2], 0)
                pixels[origin_x + x, origin_y + y] = color
    return cell


def _recipe_cell(
    workspace: str,
    row: dict,
    source_info: SourceInfo | None = None,
):
    kind = str(row.get("kind", "png")).lower()
    if kind == "png":
        source_path = _workspace_file(workspace, row.get("file", ""))
        with Image.open(source_path) as source:
            image = source.convert("RGBA")
        x = int(row.get("x", 0))
        y = int(row.get("y", 0))
        if x < 0 or y < 0 or x + CELL_SIZE > image.width or y + CELL_SIZE > image.height:
            raise MakerLiteAssetError("PNG cell crop is outside its source image")
        return image.crop((x, y, x + CELL_SIZE, y + CELL_SIZE))
    if kind in {"snes4bpp", "genesis4bpp"}:
        return _native_cell(workspace, row, kind, source_info)
    raise MakerLiteAssetError(f"Unsupported extraction cell kind: {kind}")


def import_extraction_recipe(
    source_path: str | os.PathLike[str],
    recipe_path: str | os.PathLike[str],
    private_root: str | os.PathLike[str],
    *,
    revisions: tuple[SupportedRevision, ...] = SUPPORTED_REVISIONS,
) -> ImportedKit:
    """Decode exact local PNG/native-console cells and compile a private kit.

    The recipe lives in a user-selected extraction workspace. It can crop exact
    16x16 PNG regions or decode four native 8x8 SNES/Genesis 4bpp tiles into a
    cell. No scaling, redrawing, network access, or ROM copying occurs.
    """

    if Image is None:
        raise MakerLiteAssetError("Pillow is required to run an extraction recipe")
    recipe_file = os.path.realpath(os.path.abspath(os.fspath(recipe_path)))
    workspace = os.path.realpath(os.path.dirname(recipe_file))
    with open(recipe_file, "r", encoding="utf-8") as source:
        recipe = json.load(source)
    if not isinstance(recipe, dict):
        raise MakerLiteAssetError("Extraction recipe root must be an object")
    ruleset = str(recipe.get("ruleset", "")).lower()
    info = inspect_source(source_path)
    revision = supported_revision(info, ruleset, revisions)
    pinned_revision = str(recipe.get("revision_id", "")).strip()
    if pinned_revision != revision.revision_id:
        raise MakerLiteAssetError(
            f"Recipe must target verified revision {revision.revision_id}"
        )
    pinned_digest = str(recipe.get("source_sha256", "")).lower()
    if pinned_digest and pinned_digest not in {
        info.sha256,
        info.canonical_sha256,
    }:
        raise MakerLiteAssetError("Recipe source_sha256 does not match the source")
    cells = recipe.get("cells")
    if not isinstance(cells, list) or not 14 <= len(cells) <= MAX_CELLS:
        raise MakerLiteAssetError("Recipe must provide 14 through 1536 cells")
    if not recipe.get("asset_catalog"):
        raise MakerLiteAssetError(
            "Extraction recipe must provide an authentic asset_catalog"
        )
    atlas_columns = min(32, len(cells))
    atlas_rows = (len(cells) + atlas_columns - 1) // atlas_columns
    atlas = Image.new(
        "RGBA",
        (atlas_columns * CELL_SIZE, atlas_rows * CELL_SIZE),
        (0, 0, 0, 0),
    )
    provenance = []
    for index, row in enumerate(cells):
        if not isinstance(row, dict):
            raise MakerLiteAssetError("Every extraction cell must be an object")
        atlas.paste(
            _recipe_cell(workspace, row, info),
            ((index % atlas_columns) * CELL_SIZE, (index // atlas_columns) * CELL_SIZE),
        )
        provenance.append(
            {
                "index": index,
                "name": str(row.get("name", f"cell-{index}")),
                "extractor": str(row.get("kind", "png")).lower(),
                "source_file": str(row.get("file", "")),
                **{
                    key: row[key]
                    for key in (
                        "x",
                        "y",
                        "offset",
                        "tiles",
                        "palette_file",
                        "palette_offset",
                        "transparent_index",
                    )
                    if key in row
                },
            }
        )
    private_path = os.path.abspath(os.fspath(private_root))
    os.makedirs(private_path, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".maker-lite-recipe-", dir=private_path) as bundle:
        atlas.save(os.path.join(bundle, "atlas.png"), format="PNG", optimize=False)
        manifest = {
            "ruleset": ruleset,
            "revision_id": revision.revision_id,
            "source_sha256": info.sha256,
            "atlas": "atlas.png",
            "player_base": int(recipe.get("player_base", -1)),
            "player_frames": recipe.get("player_frames", []),
            "entity_cells": recipe.get("entity_cells", {}),
            "asset_catalog": recipe.get("asset_catalog", []),
            "animations": recipe.get("animations", {}),
            "cells": provenance,
            "extractor_id": "rockpod-maker-lite-recipe",
            "extractor_version": 1,
            "recipe_sha256": _sha256_file(recipe_file),
        }
        for manifest_key, recipe_key in (
            ("source_cover", "source_cover"),
        ):
            relative = str(recipe.get(recipe_key, "")).strip()
            if relative:
                source_file = _workspace_file(workspace, relative)
                output_name = f"{manifest_key}{Path(source_file).suffix.lower()}"
                shutil.copyfile(source_file, os.path.join(bundle, output_name))
                manifest[manifest_key] = output_name
        effects = recipe.get("effects", {})
        if effects:
            if not isinstance(effects, dict):
                raise MakerLiteAssetError("Recipe effects must be an object")
            manifest["effects"] = {}
            for name, relative in effects.items():
                if name not in EFFECT_NAMES:
                    raise MakerLiteAssetError(f"Unknown effect name: {name}")
                source_file = _workspace_file(workspace, relative)
                output_name = f"effect-{name}.wav"
                shutil.copyfile(source_file, os.path.join(bundle, output_name))
                manifest["effects"][name] = output_name
        _atomic_bytes(
            os.path.join(bundle, "kit.json"),
            json.dumps(manifest, indent=2, sort_keys=True).encode("utf-8") + b"\n",
        )
        return import_extraction_bundle(
            source_path,
            bundle,
            private_root,
            revisions=revisions,
        )


def _atomic_bytes(path: str, data: bytes) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".maker-lite-", dir=os.path.dirname(path))
    try:
        with os.fdopen(fd, "wb") as output:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
    except Exception:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def _audio_pack(bundle: str, manifest: dict, kit_id: str) -> bytes:
    effects = manifest.get("effects", {})
    if not effects:
        return b""
    if not isinstance(effects, dict):
        raise MakerLiteAssetError("effects must map effect names to local WAV files")
    payload = bytearray()
    entries = []
    sample_rate = None
    for effect_name in EFFECT_NAMES:
        filename = effects.get(effect_name, "")
        if not filename:
            entries.append((0, 0))
            continue
        if os.path.basename(str(filename)) != filename:
            raise MakerLiteAssetError("Effect WAVs must be inside the extraction bundle")
        with wave.open(os.path.join(bundle, filename), "rb") as source:
            if source.getnchannels() != 2 or source.getsampwidth() != 2:
                raise MakerLiteAssetError(
                    f"{effect_name} must be signed 16-bit stereo PCM WAV"
                )
            if source.getcomptype() != "NONE":
                raise MakerLiteAssetError(f"{effect_name} WAV must be uncompressed")
            rate = source.getframerate()
            if rate not in {44100, 48000}:
                raise MakerLiteAssetError(
                    f"{effect_name} must use the device rate 44100 or 48000 Hz"
                )
            if sample_rate is None:
                sample_rate = rate
            elif sample_rate != rate:
                raise MakerLiteAssetError("All effect WAVs must use one sample rate")
            raw = source.readframes(source.getnframes())
        if len(raw) > sample_rate * 4:
            raise MakerLiteAssetError(f"{effect_name} exceeds the one-second effect limit")
        entries.append((len(payload), len(raw)))
        payload.extend(raw)
    header = bytearray(128)
    header[:4] = b"MLAU"
    struct.pack_into(
        "<HHII",
        header,
        4,
        1,
        len(EFFECT_NAMES),
        sample_rate or 44100,
        zlib.crc32(payload) & 0xFFFFFFFF,
    )
    encoded_id = kit_id.encode("ascii")
    header[16 : 16 + len(encoded_id)] = encoded_id
    for index, (offset, length) in enumerate(entries):
        struct.pack_into("<II", header, 48 + index * 8, offset, length)
    return bytes(header) + bytes(payload)


def _animation_definition(
    action_name: str,
    definition,
    player_base: int,
    action_index: int,
    cell_count: int,
    *,
    mirror: bool,
) -> bytes:
    if definition is None:
        frames = [player_base + action_index]
        ticks = 1
    elif isinstance(definition, list):
        frames = [int(value) for value in definition]
        ticks = 6
    elif isinstance(definition, dict):
        frames = [int(value) for value in definition.get("frames", [])]
        ticks = int(definition.get("ticks_per_frame", 6))
        mirror = bool(definition.get("mirror", mirror))
    else:
        raise MakerLiteAssetError(
            f"Animation {action_name} must be a frame list or object"
        )
    if (
        not frames
        or frames != list(range(frames[0], frames[0] + len(frames)))
        or frames[0] < 0
        or frames[-1] >= cell_count
        or len(frames) > 255
        or not 1 <= ticks <= 60
    ):
        raise MakerLiteAssetError(
            f"Animation {action_name} needs 1-255 consecutive in-range "
            "frames and a 1-60 tick rate"
        )
    return struct.pack(
        "<HBB", frames[0], len(frames), ticks | (0x80 if mirror else 0)
    )


def _animation_table(manifest: dict, player_base: int, cell_count: int) -> bytes:
    animations = manifest.get("animations", {})
    if not isinstance(animations, dict):
        raise MakerLiteAssetError("animations must be an object")
    unknown = set(animations) - set(ACTION_NAMES)
    if unknown:
        raise MakerLiteAssetError(
            f"Unknown player animation: {sorted(unknown)[0]}"
        )
    table = bytearray()
    for action_index, action_name in enumerate(ACTION_NAMES):
        definition = animations.get(action_name)
        directional = (
            isinstance(definition, dict)
            and bool(set(definition) & set(DIRECTION_NAMES))
        )
        if directional:
            unknown_direction = set(definition) - {
                *DIRECTION_NAMES,
                "default",
            }
            if unknown_direction:
                raise MakerLiteAssetError(
                    f"Animation {action_name} mixes directional definitions "
                    f"with {sorted(unknown_direction)[0]}"
                )
        generic = definition.get("default") if directional else definition
        for direction in DIRECTION_NAMES:
            explicit = definition.get(direction) if directional else None
            table.extend(
                _animation_definition(
                    f"{action_name}.{direction}",
                    explicit if explicit is not None else generic,
                    player_base,
                    action_index,
                    cell_count,
                    mirror=direction == "left" and explicit is None,
                )
            )
    return bytes(table)


def _player_frame_table(
    manifest: dict,
    cell_count: int,
) -> tuple[bytes, list[dict]]:
    source_frames = manifest.get("player_frames", [])
    if source_frames in (None, []):
        return b"", []
    if (
        not isinstance(source_frames, list)
        or not 14 <= len(source_frames) <= MAX_PLAYER_FRAMES
    ):
        raise MakerLiteAssetError(
            f"player_frames must contain 14-{MAX_PLAYER_FRAMES} frames"
        )
    encoded = bytearray(struct.pack("<HH", len(source_frames), 0))
    normalized = []
    for index, source in enumerate(source_frames):
        if not isinstance(source, dict):
            raise MakerLiteAssetError(f"player_frames[{index}] must be an object")
        try:
            columns = int(source.get("columns", 0))
            rows = int(source.get("rows", 0))
            offset_x = int(source.get("offset_x", 0))
            offset_y = int(source.get("offset_y", 0))
        except (TypeError, ValueError) as exc:
            raise MakerLiteAssetError(
                f"player_frames[{index}] dimensions must be integers"
            ) from exc
        cells = source.get("cells")
        if (
            not 1 <= columns <= 4
            or not 1 <= rows <= 4
            or not -64 <= offset_x <= 64
            or not -64 <= offset_y <= 64
            or not isinstance(cells, list)
            or len(cells) != columns * rows
        ):
            raise MakerLiteAssetError(
                f"player_frames[{index}] must be a bounded 1x1 through 4x4 grid"
            )
        packed_cells = []
        for cell in cells:
            if cell is None or int(cell) == -1:
                packed_cells.append(0xFFFF)
                continue
            cell = int(cell)
            if not 0 <= cell < cell_count:
                raise MakerLiteAssetError(
                    f"player_frames[{index}] references a cell outside the atlas"
                )
            packed_cells.append(cell)
        padded = packed_cells + [0xFFFF] * (
            PLAYER_FRAME_CELLS - len(packed_cells)
        )
        encoded.extend(
            struct.pack(
                "<BBbb16H",
                columns,
                rows,
                offset_x,
                offset_y,
                *padded,
            )
        )
        normalized.append(
            {
                "columns": columns,
                "rows": rows,
                "offset_x": offset_x,
                "offset_y": offset_y,
                "cells": [
                    -1 if cell == 0xFFFF else cell for cell in packed_cells
                ],
            }
        )
    return bytes(encoded), normalized


def import_extraction_bundle(
    source_path: str | os.PathLike[str],
    bundle_dir: str | os.PathLike[str],
    private_root: str | os.PathLike[str],
    *,
    revisions: tuple[SupportedRevision, ...] = SUPPORTED_REVISIONS,
) -> ImportedKit:
    """Compile a provenance-complete atlas prepared from a reference ROM.

    ``bundle_dir/kit.json`` pins the source SHA-256 and names ``atlas.png``.
    Every 16x16 atlas cell must have a provenance row. Pixel operations are
    limited to RGBA expansion, transparent-index mapping, and RGB565 packing.
    """

    info = inspect_source(source_path)
    bundle = os.path.abspath(os.fspath(bundle_dir))
    manifest_path = os.path.join(bundle, "kit.json")
    with open(manifest_path, "r", encoding="utf-8") as source:
        manifest = json.load(source)
    ruleset = str(manifest.get("ruleset", "")).lower()
    if ruleset not in RULESETS:
        raise MakerLiteAssetError("Kit ruleset must be mario, zelda, or sonic")
    detected_revision = supported_revision(info, ruleset, revisions)
    expected_digest = str(manifest.get("source_sha256", "")).lower()
    if expected_digest != info.sha256:
        raise MakerLiteAssetError(
            f"Reference hash mismatch: detected {info.sha256}"
        )
    atlas_name = os.path.basename(str(manifest.get("atlas", "atlas.png")))
    if atlas_name != manifest.get("atlas", atlas_name):
        raise MakerLiteAssetError("Atlas must be a file inside the extraction bundle")
    atlas_path = os.path.join(bundle, atlas_name)
    payload, cell_count = _atlas_payload(atlas_path)
    provenance = manifest.get("cells")
    if not isinstance(provenance, list):
        raise MakerLiteAssetError("Kit must contain per-cell provenance")
    cell_indexes = {row.get("index") for row in provenance if isinstance(row, dict)}
    if cell_indexes != set(range(cell_count)):
        raise MakerLiteAssetError("Every atlas cell needs exactly one provenance row")
    player_base = int(manifest.get("player_base", -1))
    player_frame_table, player_frames = _player_frame_table(
        manifest,
        cell_count,
    )
    animation_limit = len(player_frames) if player_frames else cell_count
    if player_base < 0 or player_base + 13 >= animation_limit:
        raise MakerLiteAssetError(
            "player_base must provide all 14 action frames"
        )

    revision = str(manifest.get("revision_id", "")).strip()
    if revision != detected_revision.revision_id:
        raise MakerLiteAssetError(
            "Kit revision_id does not match the verified source revision "
            f"{detected_revision.revision_id}"
        )
    kit_id = f"{ruleset}-{info.canonical_sha256[:12]}"
    encoded_id = kit_id.encode("ascii")
    animation_table = _animation_table(
        manifest,
        player_base,
        animation_limit,
    )
    version = ART_VERSION if player_frames else 3
    art_payload = payload + animation_table + player_frame_table
    header = bytearray(ART_HEADER_SIZE)
    header[:4] = ART_MAGIC
    struct.pack_into("<HHHHI", header, 4, version, CELL_SIZE, cell_count,
                     player_base, zlib.crc32(art_payload) & 0xFFFFFFFF)
    header[16 : 16 + len(encoded_id)] = encoded_id
    source_entity_cells = manifest.get("entity_cells", {})
    if not isinstance(source_entity_cells, dict):
        raise MakerLiteAssetError("entity_cells must map object names to atlas cells")
    entity_cells = {}
    for name, cell in source_entity_cells.items():
        if name not in ENTITY_KINDS:
            raise MakerLiteAssetError(f"Unknown entity atlas mapping: {name}")
        cell = int(cell)
        if not 0 < cell < min(cell_count, 256):
            raise MakerLiteAssetError(f"Entity cell for {name} is outside the atlas")
        entity_cells[name] = cell
        header[48 + ENTITY_KINDS[name]] = cell
    asset_catalog = validate_asset_catalog(
        manifest.get("asset_catalog", []),
        cell_count,
        entity_cells,
    )

    output_dir = os.path.join(os.path.abspath(os.fspath(private_root)), "kits", kit_id)
    _atomic_bytes(
        os.path.join(output_dir, "art.mla"), bytes(header) + art_payload
    )
    audio = _audio_pack(bundle, manifest, kit_id)
    if audio:
        _atomic_bytes(os.path.join(output_dir, "audio.mla"), audio)
    source_cover_file = ""
    cover_name = str(manifest.get("source_cover", "")).strip()
    if cover_name:
        if os.path.basename(cover_name) != cover_name:
            raise MakerLiteAssetError(
                "Source cover must be an image inside the extraction bundle"
            )
        suffix = Path(cover_name).suffix.lower()
        if suffix not in {".png", ".jpg", ".jpeg", ".webp", ".bmp"}:
            raise MakerLiteAssetError("Source cover uses an unsupported image format")
        cover_path = os.path.join(bundle, cover_name)
        if Image is None:
            raise MakerLiteAssetError("Pillow is required to validate source cover art")
        try:
            with Image.open(cover_path) as cover:
                cover.verify()
        except (OSError, ValueError) as exc:
            raise MakerLiteAssetError("Source cover is not a readable image") from exc
        source_cover_file = f"source-cover{suffix}"
        with open(cover_path, "rb") as cover:
            _atomic_bytes(
                os.path.join(output_dir, source_cover_file),
                cover.read(),
            )
    provenance_output = {
        "kit_id": kit_id,
        "ruleset": ruleset,
        "revision_id": revision,
        "source": {
            "sha256": info.sha256,
            "canonical_sha256": info.canonical_sha256,
            "verification_algorithm": detected_revision.algorithm,
            "verification_digest": detected_revision.digest,
            "size": info.size,
            "canonical_size": info.canonical_size,
            "copier_header_stripped_for_verification": info.copier_header,
            "title": info.title,
            "platform": info.platform,
        },
        "conversion_operations": [
            "palette/channel expansion",
            "transparent alpha to RGB565 transparent key",
            "cell-major atlas packing",
            "bounded player animation table packing",
            *(
                ["uncompressed signed 16-bit stereo effect packing"]
                if audio else []
            ),
        ],
        "cell_count": cell_count,
        "cells": provenance,
        "player_frames": player_frames,
        "entity_cells": entity_cells,
        "asset_catalog": asset_catalog,
        "source_cover_file": source_cover_file,
        "extractor": {
            "id": str(manifest.get("extractor_id", "prepared-bundle")),
            "version": manifest.get("extractor_version", 1),
            "recipe_sha256": str(manifest.get("recipe_sha256", "")),
        },
    }
    _atomic_bytes(
        os.path.join(output_dir, "kit.mlk"),
        json.dumps(provenance_output, indent=2, sort_keys=True).encode("utf-8") + b"\n",
    )
    _atomic_bytes(
        os.path.join(output_dir, "provenance.tsv"),
        (
            "kit_id\truleset\trevision_id\tsource_sha256\tart_sha256\n"
            f"{kit_id}\t{ruleset}\t{revision}\t{info.sha256}\t"
            f"{hashlib.sha256(bytes(header) + art_payload).hexdigest()}\n"
        ).encode("utf-8"),
    )
    return ImportedKit(
        kit_id=kit_id,
        ruleset=ruleset,
        directory=output_dir,
        source_sha256=info.sha256,
        art_sha256=hashlib.sha256(bytes(header) + art_payload).hexdigest(),
        cell_count=cell_count,
    )


def list_private_kits(private_root: str | os.PathLike[str]) -> list[dict]:
    kits_root = os.path.join(os.path.abspath(os.fspath(private_root)), "kits")
    result = []
    if not os.path.isdir(kits_root):
        return result
    for entry in sorted(os.scandir(kits_root), key=lambda item: item.name.casefold()):
        manifest = os.path.join(entry.path, "kit.mlk")
        art = os.path.join(entry.path, "art.mla")
        if not entry.is_dir() or not os.path.isfile(manifest) or not os.path.isfile(art):
            continue
        try:
            with open(manifest, "r", encoding="utf-8") as source:
                result.append(json.load(source))
        except (OSError, ValueError):
            continue
    return result


def install_bundled_neon_nook_kit(
    private_root: str | os.PathLike[str],
    bundle_dir: str | os.PathLike[str] | None = None,
) -> ImportedKit:
    """Install the repository's original Neon Nook kit into private data.

    Commercial source kits continue to require an owned, hash-verified ROM.
    This separate path accepts only the repository-pinned original bundle,
    verifies every declared digest plus the bounded MLAR/MLAU structures, and
    never treats generated art as authentic source-game pixels.
    """

    bundle = Path(bundle_dir or BUNDLED_NEON_NOOK_ROOT).resolve()
    package_path = bundle / "pack.json"
    try:
        package = json.loads(package_path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise MakerLiteAssetError("Bundled Neon Nook package is unreadable") from exc
    if (
        not isinstance(package, dict)
        or package.get("format") != 1
        or package.get("kit_id") != BUNDLED_NEON_NOOK_KIT_ID
        or package.get("ruleset") != "zelda"
        or not isinstance(package.get("files"), dict)
    ):
        raise MakerLiteAssetError("Bundled Neon Nook package metadata is invalid")

    required = {
        "art.mla",
        "audio.mla",
        "atlas.png",
        "kit.mlk",
        "provenance.tsv",
        "source-cover.png",
    }
    if set(package["files"]) != required:
        raise MakerLiteAssetError("Bundled Neon Nook file manifest is incomplete")
    resolved_files = {}
    for name in sorted(required):
        path = (bundle / name).resolve()
        if path.parent != bundle or not path.is_file():
            raise MakerLiteAssetError(f"Bundled Neon Nook file is missing: {name}")
        expected = str(package["files"].get(name, "")).lower()
        if not re.fullmatch(r"[0-9a-f]{64}", expected) or _sha256_file(path) != expected:
            raise MakerLiteAssetError(
                f"Bundled Neon Nook checksum mismatch: {name}"
            )
        resolved_files[name] = path

    try:
        manifest = json.loads(
            resolved_files["kit.mlk"].read_text(encoding="utf-8")
        )
    except (OSError, ValueError) as exc:
        raise MakerLiteAssetError("Bundled Neon Nook kit manifest is invalid") from exc
    original_source = (
        manifest.get("source") if isinstance(manifest, dict) else None
    )
    if (
        not isinstance(manifest, dict)
        or manifest.get("kit_id") != BUNDLED_NEON_NOOK_KIT_ID
        or manifest.get("ruleset") != "zelda"
        or manifest.get("revision_id") != "neon-nook-original-v2"
        or not isinstance(original_source, dict)
        or original_source.get("type") != "original-generated"
        or int(manifest.get("cell_count", 0)) != int(package.get("cell_count", -1))
        or int(manifest.get("catalog_count", 0))
        != int(package.get("catalog_count", -1))
    ):
        raise MakerLiteAssetError("Bundled Neon Nook provenance is inconsistent")
    entity_cells = manifest.get("entity_cells")
    if not isinstance(entity_cells, dict):
        raise MakerLiteAssetError("Bundled Neon Nook entity mappings are invalid")
    provenance = manifest.get("cells")
    if (
        not isinstance(provenance, list)
        or len(provenance) != int(manifest["cell_count"])
        or any(
            not isinstance(row, dict)
            or row.get("index") != index
            or not str(row.get("name", ""))
            or not str(row.get("source_file", ""))
            or not str(row.get("source_sha256", ""))
            or row.get("extractor") != "neon-nook-original-builder-v2"
            for index, row in enumerate(provenance)
        )
    ):
        raise MakerLiteAssetError(
            "Bundled Neon Nook per-cell provenance is invalid"
        )
    player_frames = manifest.get("player_frames")
    if (
        not isinstance(player_frames, list)
        or len(player_frames) != int(package.get("player_frame_count", -1))
    ):
        raise MakerLiteAssetError(
            "Bundled Neon Nook frame provenance is invalid"
        )
    validate_asset_catalog(
        manifest.get("asset_catalog"),
        int(manifest["cell_count"]),
        {str(name): int(cell) for name, cell in entity_cells.items()},
    )

    art = resolved_files["art.mla"].read_bytes()
    if len(art) < ART_HEADER_SIZE:
        raise MakerLiteAssetError("Bundled Neon Nook art is truncated")
    version, cell_size, cell_count, player_base, expected_crc = struct.unpack_from(
        "<HHHHI", art, 4
    )
    encoded_id = art[16:48].split(b"\0", 1)[0].decode("ascii", "replace")
    if (
        art[:4] != ART_MAGIC
        or version != ART_VERSION
        or cell_size != CELL_SIZE
        or cell_count != int(manifest["cell_count"])
        or encoded_id != BUNDLED_NEON_NOOK_KIT_ID
        or zlib.crc32(art[ART_HEADER_SIZE:]) & 0xFFFFFFFF != expected_crc
    ):
        raise MakerLiteAssetError("Bundled Neon Nook art failed validation")
    pixel_bytes = cell_count * CELL_SIZE * CELL_SIZE * 2
    frame_offset = ART_HEADER_SIZE + pixel_bytes + ANIMATION_TABLE_SIZE
    try:
        frame_count, reserved = struct.unpack_from("<HH", art, frame_offset)
    except struct.error as exc:
        raise MakerLiteAssetError(
            "Bundled Neon Nook player frame table is truncated"
        ) from exc
    expected_art_size = frame_offset + 4 + frame_count * PLAYER_FRAME_SIZE
    if (
        reserved != 0
        or frame_count != int(package.get("player_frame_count", -1))
        or not 14 <= frame_count <= MAX_PLAYER_FRAMES
        or player_base + 13 >= frame_count
        or len(art) != expected_art_size
    ):
        raise MakerLiteAssetError(
            "Bundled Neon Nook player frame table is invalid"
        )

    audio = resolved_files["audio.mla"].read_bytes()
    if len(audio) < 128:
        raise MakerLiteAssetError("Bundled Neon Nook audio is truncated")
    audio_version, effect_count, sample_rate, audio_crc = struct.unpack_from(
        "<HHII", audio, 4
    )
    audio_id = audio[16:48].split(b"\0", 1)[0].decode("ascii", "replace")
    if (
        audio[:4] != b"MLAU"
        or audio_version != 1
        or effect_count != len(EFFECT_NAMES)
        or sample_rate not in {44100, 48000}
        or audio_id != BUNDLED_NEON_NOOK_KIT_ID
        or zlib.crc32(audio[128:]) & 0xFFFFFFFF != audio_crc
    ):
        raise MakerLiteAssetError("Bundled Neon Nook audio failed validation")

    output = (
        Path(private_root).resolve()
        / "kits"
        / BUNDLED_NEON_NOOK_KIT_ID
    )
    for name in (
        "art.mla",
        "audio.mla",
        "kit.mlk",
        "provenance.tsv",
        "source-cover.png",
    ):
        _atomic_bytes(
            os.fspath(output / name),
            resolved_files[name].read_bytes(),
        )
    return ImportedKit(
        kit_id=BUNDLED_NEON_NOOK_KIT_ID,
        ruleset="zelda",
        directory=os.fspath(output),
        source_sha256=str(original_source["sprites_sha256"]),
        art_sha256=_sha256_file(output / "art.mla"),
        cell_count=cell_count,
    )
