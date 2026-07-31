"""Host-side Sitekick support: chip metadata, drops, codes and trading.

The iPod plugin is deliberately offline and self-contained: it owns the
player's chips, loadout and XP, and it never invents a grant.  Everything
that needs a calendar, a curator or a second player is settled here and
handed to the device as a plain TSV drop.

Split of responsibilities:

* device  - owned chips, equipped loadout, XP, coins (authoritative)
* RockPod - chip names/rarity/slot overrides, chip-of-the-day/week/month,
            redemption codes, trade settlement

The device applies ``sync/inbox.v1.tsv`` exactly once and deletes it; it
stages trade offers into ``sync/outbox.v1.tsv`` for RockPod to pick up.
"""

from __future__ import annotations

import hashlib
import shutil
import struct
from dataclasses import dataclass, replace
from datetime import date
from pathlib import Path

CHIPS_FILE = "data/chips.v1.tsv"
SERIES_FILE = "data/series.v1.tsv"
INBOX_FILE = "sync/inbox.v1.tsv"
OUTBOX_FILE = "sync/outbox.v1.tsv"
CODES_FILE = "data/codes.v1.tsv"
OVERRIDES_FILE = "data/overrides.v1.tsv"
SAVE_FILE = "state/save.v1.dat"
PREVIEW_FILE = "preview/current.bmp"
STAGE_FILE = "data/stage.v1.tsv"

RARITIES = ("common", "rare", "legendary")
MAX_CHIPS = 1024
SLOTS = ("aura", "shell", "arms", "face", "eyes", "hair", "antenna",
         "accessory", "none")
BODY_COLORS = (
    ("Classic", None),
    ("Slime", "sitekick-color-1.bmp"),
    ("YTV Purple", "sitekick-color-2.bmp"),
    ("Ooze Orange", "sitekick-color-3.bmp"),
    ("Aqua", "sitekick-color-4.bmp"),
    ("Hot Pink", "sitekick-color-5.bmp"),
    ("Classic Yellow", "sitekick-color-6.bmp"),
)
BACKGROUNDS = (
    ("Sitekick Splash", "sitekick-splash"),
    ("Butterfly", "butterfly"),
    ("Purple Gear", "purple-gear"),
    ("Blue Gear", "blue-gear"),
    ("Aqua Leaf", "aqua-leaf"),
    ("Ooze Grid", "ooze-grid"),
    ("Oliver Scrapyard", "oliver-scrapyard"),
    ("Emma Neon Box", "emma-neon-box"),
    ("Oliver Alone Crowd", "oliver-alone-crowd"),
    ("Beatles Crosswalk", "beatles-crosswalk"),
    ("Beatles Pepperland", "beatles-pepperland"),
    ("Beatles Rooftop", "beatles-rooftop"),
    ("Hasan News Studio", "hasan-news-studio"),
    ("QTC Spotlight Stage", "qtc-spotlight-stage"),
    ("Maya Wildlife Perch", "maya-wildlife-perch"),
    ("Habs Home Ice", "habs-home-ice"),
    ("Polaroid Darkroom", "polaroid-darkroom"),
    ("KI Arena Lightning", "ki-arena-lightning"),
    ("Cyberpunk Night City", "cyberpunk-night-city"),
)

# Chip counts per drop, mirroring the original game's cadence.
WEEKLY_MIX = (("common", 2), ("rare", 2), ("legendary", 1))
MONTHLY_MIX = (("common", 5), ("rare", 5), ("legendary", 5))


class SitekickError(RuntimeError):
    """Raised when a Sitekick asset pack or sync file cannot be used."""


@dataclass(frozen=True)
class Chip:
    id: int
    slot: str
    z: int
    ax: int
    ay: int
    w: int
    h: int
    page: int
    index: int
    rarity: str
    worn: bool
    name: str

    def to_row(self) -> str:
        return "\t".join((
            str(self.id), self.slot, str(self.z), str(self.ax), str(self.ay),
            str(self.w), str(self.h), str(self.page), str(self.index),
            self.rarity, "1" if self.worn else "0", self.name,
        ))


@dataclass(frozen=True)
class Grant:
    kind: str          # "grant" or "coins"
    value: int
    label: str


@dataclass(frozen=True)
class Offer:
    chip_id: int
    name: str


@dataclass(frozen=True)
class SitekickState:
    xp: int = 0
    coins: int = 250
    owned: tuple[int, ...] = ()
    equipped: tuple[int | None, ...] = (None,) * 8
    dump_id: int | None = None
    dump_ready_at: int = 0
    body_color: int = 0
    background: int = 0
    exists: bool = False


@dataclass(frozen=True)
class SitekickSnapshot:
    root: Path
    state: SitekickState
    chips: dict[int, Chip]
    offers: tuple[Offer, ...]
    preview_path: Path


# ---------------------------------------------------------------------------
# chip table
# ---------------------------------------------------------------------------

def _pack_root(root) -> Path:
    path = Path(root)
    if not (path / CHIPS_FILE).is_file():
        raise SitekickError(f"no Sitekick chip table under {path}")
    return path


def load_chips(root) -> dict[int, Chip]:
    """Read chips.v1.tsv as emitted by tools/sitekick_package_assets.py."""
    path = _pack_root(root) / CHIPS_FILE
    chips: dict[int, Chip] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) < 12:
            continue
        try:
            chip = Chip(
                id=int(fields[0]),
                slot=fields[1],
                z=int(fields[2]),
                ax=int(fields[3]),
                ay=int(fields[4]),
                w=int(fields[5]),
                h=int(fields[6]),
                page=int(fields[7]),
                index=int(fields[8]),
                rarity=fields[9] if fields[9] in RARITIES else "common",
                worn=fields[10] == "1",
                name=fields[11],
            )
        except ValueError:
            continue
        chips[chip.id] = chip
    if not chips:
        raise SitekickError(f"chip table {path} is empty")
    return chips


def load_overrides(root) -> dict[int, dict[str, str]]:
    """Read the human-authored name/rarity/slot overrides, if present.

    The Sitekick Remastered art carries no chip names, rarities or slots --
    those lived in the retired game server -- so they are authored here
    rather than guessed by the packager.
    """
    path = Path(root) / OVERRIDES_FILE
    overrides: dict[int, dict[str, str]] = {}
    if not path.is_file():
        return overrides
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) < 2:
            continue
        try:
            chip_id = int(fields[0])
        except ValueError:
            continue
        entry: dict[str, str] = {}
        if len(fields) > 1 and fields[1].strip():
            entry["name"] = fields[1].strip()[:19]
        if len(fields) > 2 and fields[2].strip() in RARITIES:
            entry["rarity"] = fields[2].strip()
        if len(fields) > 3 and fields[3].strip() in SLOTS:
            entry["slot"] = fields[3].strip()
        if entry:
            overrides[chip_id] = entry
    return overrides


def apply_overrides(root) -> int:
    """Fold overrides.v1.tsv into chips.v1.tsv.  Returns rows changed."""
    root = _pack_root(root)
    chips = load_chips(root)
    overrides = load_overrides(root)
    if not overrides:
        return 0

    changed = 0
    for chip_id, entry in overrides.items():
        chip = chips.get(chip_id)
        if chip is None:
            continue
        updated = replace(chip, **entry)
        if updated != chip:
            chips[chip_id] = updated
            changed += 1

    if changed:
        header = ("# id\tslot\tz\tax\tay\tw\th\tpage\tindex\trarity\tworn"
                  "\tname")
        body = "\n".join(chips[cid].to_row() for cid in sorted(chips))
        (root / CHIPS_FILE).write_text(header + "\n" + body + "\n",
                                       encoding="utf-8")
    return changed


# ---------------------------------------------------------------------------
# mounted-device state and preview
# ---------------------------------------------------------------------------


# SKS3 header layout, mirroring apps/plugins/sitekick.c's SK_SAVE3_* macros
# exactly (SK_SLOT_COUNT=8 equip positions, SK_SHOP_SLOTS=6, SK_PRESET_COUNT=8,
# SK_PRESET_NAME_MAX=21). SKS3 adds shop stock + presets after SKS2's
# equipment/dump/appearance header; the device has written SKS3 (never SKS2)
# since presets shipped, so it must be understood here too.
_SK_SLOT_COUNT = 8
_SAVE2_BASE_HEADER = 16 + _SK_SLOT_COUNT * 2                    # 32
_SAVE2_DUMP_HEADER = _SAVE2_BASE_HEADER + 8                     # 40
_SAVE2_HEADER = _SAVE2_DUMP_HEADER + 8                           # 48
_SHOP_SLOTS = 6
_SAVE2_SHOP_HEADER = _SAVE2_HEADER + 4 + _SHOP_SLOTS * 2         # 64
_PRESET_COUNT = 8
_PRESET_NAME_MAX = 21
_PRESET_BYTES = 3 + _PRESET_NAME_MAX + _SK_SLOT_COUNT * 2        # 40
_SAVE3_HEADER = _SAVE2_SHOP_HEADER + 2 + _PRESET_COUNT * _PRESET_BYTES  # 386


def load_state(root) -> SitekickState:
    """Read the current ID-based Sitekick save from a packaged asset root.

    The device writes SKS3 (adds shop stock + presets on top of SKS2's
    equipment/dump/appearance header); SKS2 is still accepted for saves
    written before presets existed.
    """
    path = Path(root) / SAVE_FILE
    if not path.is_file():
        return SitekickState()
    data = path.read_bytes()
    if len(data) >= 4 and data[:4] == b"SKS3":
        return _load_state_v3(data, path)
    if len(data) >= 4 and data[:4] == b"SKS2":
        return _load_state_v2(data, path)
    raise SitekickError(f"unsupported Sitekick save {path}")


def _load_state_v2(data: bytes, path: Path) -> SitekickState:
    if len(data) < 32:
        raise SitekickError(f"unsupported Sitekick save {path}")

    xp, coins = struct.unpack_from("<II", data, 4)
    owned_count, header_size = struct.unpack_from("<HH", data, 12)
    header_size = header_size or 32
    if header_size not in (32, 40, 48):
        raise SitekickError(f"unsupported Sitekick save header {header_size}")
    if owned_count > MAX_CHIPS or len(data) < header_size + owned_count * 2:
        raise SitekickError(f"truncated Sitekick save {path}")

    equipped = tuple(
        None if chip_id == 0xffff else chip_id
        for chip_id in struct.unpack_from("<8H", data, 16)
    )
    dump_id = None
    dump_ready_at = 0
    if header_size >= 40:
        raw_dump = struct.unpack_from("<H", data, 32)[0]
        dump_id = None if raw_dump == 0xffff else raw_dump
        dump_ready_at = struct.unpack_from("<I", data, 36)[0]
    body_color = 0
    background = 0
    if header_size >= 48:
        body_color = min(data[40], len(BODY_COLORS) - 1)
        background = min(data[41], len(BACKGROUNDS) - 1)
    owned = tuple(
        struct.unpack_from("<H", data, header_size + index * 2)[0]
        for index in range(owned_count)
    )
    return SitekickState(
        xp=xp,
        coins=coins,
        owned=owned,
        equipped=equipped,
        dump_id=dump_id,
        dump_ready_at=dump_ready_at,
        body_color=body_color,
        background=background,
        exists=True,
    )


def _load_state_v3(data: bytes, path: Path) -> SitekickState:
    if len(data) < _SAVE3_HEADER:
        raise SitekickError(f"unsupported Sitekick save {path}")

    xp, coins = struct.unpack_from("<II", data, 4)
    owned_count, header_size = struct.unpack_from("<HH", data, 12)
    if (header_size != _SAVE3_HEADER or owned_count > MAX_CHIPS or
            len(data) < _SAVE3_HEADER + owned_count * 2):
        raise SitekickError(f"truncated Sitekick save {path}")

    equipped = tuple(
        None if chip_id == 0xffff else chip_id
        for chip_id in struct.unpack_from("<8H", data, 16)
    )
    raw_dump = struct.unpack_from("<H", data, _SAVE2_BASE_HEADER)[0]
    dump_id = None if raw_dump == 0xffff else raw_dump
    dump_ready_at = struct.unpack_from("<I", data, _SAVE2_BASE_HEADER + 4)[0]
    body_color = min(data[_SAVE2_DUMP_HEADER], len(BODY_COLORS) - 1)
    background = min(data[_SAVE2_DUMP_HEADER + 1], len(BACKGROUNDS) - 1)
    owned = tuple(
        struct.unpack_from("<H", data, _SAVE3_HEADER + index * 2)[0]
        for index in range(owned_count)
    )
    return SitekickState(
        xp=xp,
        coins=coins,
        owned=owned,
        equipped=equipped,
        dump_id=dump_id,
        dump_ready_at=dump_ready_at,
        body_color=body_color,
        background=background,
        exists=True,
    )


def _stage_values(root) -> dict[str, int]:
    values = {
        "stage_w": 240,
        "stage_h": 192,
        "body_origin_x": 120,
        "body_origin_y": 70,
    }
    path = Path(root) / STAGE_FILE
    if not path.is_file():
        return values
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t", 1)
        if len(fields) != 2 or fields[0] not in values:
            continue
        try:
            values[fields[0]] = int(fields[1])
        except ValueError:
            pass
    return values


def _load_alpha_bmp(path):
    """Load the packager's BI_RGB BGRA bitmap without discarding byte alpha."""
    from PIL import Image

    data = Path(path).read_bytes()
    if len(data) < 54 or data[:2] != b"BM":
        raise SitekickError(f"invalid Sitekick bitmap {path}")
    offset = struct.unpack_from("<I", data, 10)[0]
    width, raw_height = struct.unpack_from("<ii", data, 18)
    depth = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    if width <= 0 or raw_height == 0 or depth != 32 or compression != 0:
        image = Image.open(path)
        return image.convert("RGBA")
    height = abs(raw_height)
    stride = width * 4
    raw = data[offset:offset + stride * height]
    if len(raw) != stride * height:
        raise SitekickError(f"truncated Sitekick bitmap {path}")
    orientation = -1 if raw_height > 0 else 1
    return Image.frombytes("RGBA", (width, height), raw, "raw", "BGRA",
                           stride, orientation)


def render_current_preview(root, state: SitekickState | None = None,
                           output_path=None) -> Path:
    """Compose current, fixed-background, and floating-character previews."""
    from PIL import Image, ImageDraw, ImageFont

    root = _pack_root(root)
    state = state or load_state(root)
    chips = load_chips(root)
    stage = _stage_values(root)
    stage_image = Image.new(
        "RGBA", (stage["stage_w"], stage["stage_h"]), (0, 0, 0, 0)
    )
    body_filename = BODY_COLORS[state.body_color][1] \
        if 0 <= state.body_color < len(BODY_COLORS) else None
    body_path = root / "base" / (body_filename or "sitekick.bmp")
    if not body_path.is_file():
        body_path = root / "base" / "sitekick.bmp"
    body = _load_alpha_bmp(body_path)
    background_index = state.background \
        if 0 <= state.background < len(BACKGROUNDS) else 0

    layers = []
    for slot_index, chip_id in enumerate(state.equipped):
        chip = chips.get(chip_id) if chip_id is not None else None
        if chip is None or chip.w <= 0 or chip.h <= 0:
            continue
        path = root / "chips" / f"{chip.id:04d}.bmp"
        if path.is_file():
            layers.append((chip.z, slot_index, chip, path))
    # Match the device compositor exactly: z first, then equipment slot.
    layers.sort(key=lambda item: (item[0], item[1]))

    def paste_chip(item):
        _z, _slot_index, chip, path = item
        layer = _load_alpha_bmp(path)
        stage_image.alpha_composite(
            layer,
            (stage["body_origin_x"] + chip.ax,
             stage["body_origin_y"] + chip.ay),
        )

    for item in layers:
        if item[0] < 0:
            paste_chip(item)
    stage_image.alpha_composite(body, (0, 0))
    for item in layers:
        if item[0] >= 0:
            paste_chip(item)

    pane_path = root / "backgrounds" / f"pane-{background_index}.bmp"
    if pane_path.is_file():
        pane = _load_alpha_bmp(pane_path).convert("RGB")
    else:
        pane = Image.new("RGB", (174, 240), (185, 221, 66))
    draw = ImageDraw.Draw(pane)
    draw.rectangle((0, 0, 173, 31), fill=(76, 11, 100))
    draw.rectangle((0, 31, 173, 35), fill=(255, 112, 11))
    logo_path = root / "base" / "ytv-logo.bmp"
    if logo_path.is_file():
        logo = _load_alpha_bmp(logo_path)
        logo.thumbnail((42, 25), Image.Resampling.LANCZOS)
        pane.paste(logo, (7, 3), logo)
    else:
        draw.text((10, 8), "SITEKICK", fill=(255, 216, 31),
                  font=ImageFont.load_default())
    draw.rounded_rectangle((8, 190, 165, 230), radius=6,
                           fill=(247, 240, 249), outline=(76, 11, 100),
                           width=2)
    draw.text((16, 197), f"XP {state.xp}", fill=(42, 8, 55),
              font=ImageFont.load_default())
    draw.text((16, 213), f"COINS {state.coins}", fill=(42, 8, 55),
              font=ImageFont.load_default())

    output = Path(output_path) if output_path else root / PREVIEW_FILE
    output.parent.mkdir(parents=True, exist_ok=True)
    background_output = output.with_name("pane-background.bmp")
    float_output = output.with_name("current-float.bmp")

    bounds = stage_image.getbbox()
    character = stage_image.crop(bounds) if bounds else stage_image
    character.thumbnail((148, 129), Image.Resampling.LANCZOS)
    floating = Image.new("RGBA", (154, 139), (255, 0, 255, 0))
    floating.paste(
        character,
        ((154 - character.width) // 2, (139 - character.height) // 2),
        character,
    )
    preview = pane.copy()
    preview.paste(floating, (10, 40), floating)

    for image, path in (
        (pane, background_output),
        (floating, float_output),
        (preview, output),
    ):
        temporary = path.with_suffix(path.suffix + ".tmp")
        image.save(temporary, format="BMP")
        temporary.replace(path)
    return output


def sync_current_sitekick(device_root) -> SitekickSnapshot:
    """Import the mounted state and refresh its native iPod preview."""
    root = Path(device_root)
    if root.name != "sitekick":
        root = root / ".rockbox" / "sitekick"
    chips = load_chips(root)
    state = load_state(root)
    preview_path = render_current_preview(root, state)
    return SitekickSnapshot(
        root=root,
        state=state,
        chips=chips,
        offers=tuple(read_outbox(root)),
        preview_path=preview_path,
    )


# ---------------------------------------------------------------------------
# deterministic drops
# ---------------------------------------------------------------------------

def _seeded_pick(chips: dict[int, Chip], seed: str, rarity: str,
                 count: int, exclude: set[int] | None = None) -> list[int]:
    """Pick `count` chips of a rarity, deterministically for a given seed.

    Using a hash rather than random() means the same week always produces
    the same drop on every machine, so a device that syncs twice does not
    see the list change underneath it.
    """
    pool = sorted(cid for cid, chip in chips.items()
                  if chip.rarity == rarity and cid not in (exclude or set()))
    if not pool:
        return []
    ranked = sorted(
        pool,
        key=lambda cid: hashlib.sha256(
            f"{seed}:{rarity}:{cid}".encode()).hexdigest(),
    )
    return ranked[:count]


def daily_chip(chips: dict[int, Chip], when: date | None = None) -> int | None:
    when = when or date.today()
    picks = _seeded_pick(chips, f"sitekick-day-{when.isoformat()}",
                         "common", 1)
    return picks[0] if picks else None


def weekly_chips(chips: dict[int, Chip],
                 when: date | None = None) -> list[int]:
    when = when or date.today()
    year, week, _ = when.isocalendar()
    seed = f"sitekick-week-{year}-{week:02d}"
    chosen: list[int] = []
    for rarity, count in WEEKLY_MIX:
        chosen.extend(_seeded_pick(chips, seed, rarity, count, set(chosen)))
    return chosen


def monthly_chips(chips: dict[int, Chip],
                  when: date | None = None) -> list[int]:
    when = when or date.today()
    seed = f"sitekick-month-{when.year}-{when.month:02d}"
    chosen: list[int] = []
    for rarity, count in MONTHLY_MIX:
        chosen.extend(_seeded_pick(chips, seed, rarity, count, set(chosen)))
    return chosen


# ---------------------------------------------------------------------------
# redemption codes
# ---------------------------------------------------------------------------

def load_codes(root) -> dict[str, list[Grant]]:
    """Read codes.v1.tsv: ``code<TAB>kind<TAB>value<TAB>label``."""
    path = Path(root) / CODES_FILE
    codes: dict[str, list[Grant]] = {}
    if not path.is_file():
        return codes
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) < 3:
            continue
        code = fields[0].strip().upper()
        kind = fields[1].strip()
        if kind not in ("grant", "coins"):
            continue
        try:
            value = int(fields[2])
        except ValueError:
            continue
        label = fields[3].strip() if len(fields) > 3 else code
        codes.setdefault(code, []).append(Grant(kind, value, label))
    return codes


def redeem(root, code: str) -> list[Grant]:
    """Resolve a redemption code into grants.  Unknown codes yield []."""
    return load_codes(root).get(str(code or "").strip().upper(), [])


# ---------------------------------------------------------------------------
# sync files
# ---------------------------------------------------------------------------

def write_inbox(root, grants) -> Path:
    """Write the grant drop the device consumes once, then deletes."""
    root = Path(root)
    path = root / INBOX_FILE
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["# kind\tvalue\tlabel"]
    for grant in grants:
        label = grant.label.replace("\t", " ").replace("\n", " ")
        lines.append(f"{grant.kind}\t{grant.value}\t{label}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def read_inbox(root) -> list[Grant]:
    """Read rewards already waiting for the device without consuming them."""
    path = Path(root) / INBOX_FILE
    grants: list[Grant] = []
    if not path.is_file():
        return grants
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) < 2 or fields[0] not in ("grant", "coins"):
            continue
        try:
            value = int(fields[1])
        except ValueError:
            continue
        grants.append(Grant(
            fields[0], value, fields[2] if len(fields) > 2 else ""
        ))
    return grants


def merge_inbox(root, grants) -> tuple[Path, list[Grant]]:
    """Append new unique rewards while preserving a pending trade or drop."""
    pending = read_inbox(root)
    seen = {(grant.kind, grant.value) for grant in pending}
    added: list[Grant] = []
    for grant in grants:
        key = (grant.kind, grant.value)
        if key in seen:
            continue
        seen.add(key)
        pending.append(grant)
        added.append(grant)
    return write_inbox(root, pending), added


def stage_code(root, code: str, owned=()) -> list[Grant]:
    """Stage one code's unowned chips for the next Sitekick launch."""
    code = str(code or "").strip()
    resolved = redeem(root, code)
    if not resolved:
        raise SitekickError(f"Unknown Sitekick secret code: {code or '(blank)'}")

    chips = load_chips(root)
    owned_ids = {int(chip_id) for chip_id in owned}
    grants = [
        grant for grant in resolved
        if grant.kind == "coins"
        or (grant.value in chips and grant.value not in owned_ids)
    ]
    _path, added = merge_inbox(root, grants)
    return added


def read_outbox(root) -> list[Offer]:
    """Read chips the device has staged as a trade offer."""
    path = Path(root) / OUTBOX_FILE
    offers: list[Offer] = []
    if not path.is_file():
        return offers
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) < 2 or fields[0] != "offer":
            continue
        try:
            chip_id = int(fields[1])
        except ValueError:
            continue
        offers.append(Offer(chip_id, fields[2] if len(fields) > 2 else ""))
    return offers


def clear_outbox(root) -> bool:
    path = Path(root) / OUTBOX_FILE
    if path.is_file():
        path.unlink()
        return True
    return False


def settle_trade(offers, incoming) -> list[Grant]:
    """Turn an accepted swap into grants for this device.

    RockPod owns the peer side; this only produces what *this* iPod gains,
    so a failed or partial trade can never silently delete chips the player
    already owns.
    """
    grants: list[Grant] = []
    for chip_id in incoming:
        grants.append(Grant("grant", int(chip_id), f"Trade {chip_id}"))
    if offers and not incoming:
        grants.append(Grant("coins", 25 * len(offers), "Trade credit"))
    return grants


def build_drop(root, when: date | None = None, code: str | None = None,
               include_weekly: bool = True,
               include_monthly: bool = False) -> list[Grant]:
    """Assemble everything owed to the device into one grant list."""
    chips = load_chips(root)
    when = when or date.today()
    grants: list[Grant] = []
    seen: set[int] = set()

    def add(chip_id: int, label: str) -> None:
        if chip_id in seen or chip_id not in chips:
            return
        seen.add(chip_id)
        grants.append(Grant("grant", chip_id, label))

    daily = daily_chip(chips, when)
    if daily is not None:
        add(daily, "Chip of the Day")
    if include_weekly:
        for chip_id in weekly_chips(chips, when):
            add(chip_id, "Chip of the Week")
    if include_monthly:
        for chip_id in monthly_chips(chips, when):
            add(chip_id, "Monthly List")
    if code:
        for grant in redeem(root, code):
            if grant.kind == "coins":
                grants.append(grant)
            else:
                add(grant.value, grant.label)
    return grants


# ---------------------------------------------------------------------------
# deployment
# ---------------------------------------------------------------------------

ASSET_DIRS = (
    "base", "chips", "icons", "data", "sounds", "backgrounds", "preview"
)


def deploy(pack_root, device_root) -> dict[str, int]:
    """Copy a packaged asset tree to ``<device>/.rockbox/sitekick``."""
    pack_root = _pack_root(pack_root)
    target = Path(device_root)
    if target.name != "sitekick":
        target = target / ".rockbox" / "sitekick"

    copied: dict[str, int] = {}
    for name in ASSET_DIRS:
        src = pack_root / name
        if not src.is_dir():
            continue
        dst = target / name
        dst.mkdir(parents=True, exist_ok=True)
        count = 0
        for item in sorted(src.iterdir()):
            if item.is_file():
                shutil.copy2(item, dst / item.name)
                count += 1
        copied[name] = count

    for name in ("state", "sync"):
        (target / name).mkdir(parents=True, exist_ok=True)

    manifest = pack_root / "source.manifest"
    if manifest.is_file():
        shutil.copy2(manifest, target / "source.manifest")
    return copied
