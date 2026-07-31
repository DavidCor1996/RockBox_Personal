"""Authentic Xbox 360 avatar profiles and bounded iPod frame packs.

The character pixels are rendered on demand from Microsoft's real Ms-PL XNA
Avatar rig by `services.xbox_avatar_render`.  RockPod composes a wardrobe out of
the genuine mesh parts, tints the original colour maps, prints real in-tree
artwork onto the real garment, and converts the result to the small RAV2 format
consumed by the Rockbox plugin.
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import struct
import time
import zlib
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw

from services.file_safety import atomic_write_text
from services.xbox_avatar_designs import (
    DEFAULT_SCALE,
    DESIGN_CHOICES,
    DESIGN_LABELS,
    SCALES,
)
from services.xbox_avatar_render import (
    AvatarRig,
    MARKETPLACE_KINDS,
    MARKETPLACE_SLOTS,
    MarketplacePack,
    BODY_PRESETS,
    BOTTOM_STYLES,
    DECAL_LABELS,
    HAIR_STYLES,
    SHOE_STYLES,
    TOP_STYLES,
    available_styles,
    render_frames,
    style_available,
)


AVATAR_ROOT = ".rockbox/achievements/avatar"
RAV1_MAGIC = b"RAV1"
RAV2_MAGIC = b"RAV2"
RAV_HEADER = struct.Struct("<4sHHHHHHI")
RAV_FLAG_DELTA = 0x01
RAV_FLAG_FILL = 0x02
RAV_FLAG_OPAQUE = 0x04
RAV_FLAG_KEYFRAMES = 0x08
TRANSPARENT_565 = 0xF81F
MAX_RAV_WIDTH = 104
MAX_RAV_HEIGHT = 168
MAX_RAV_FRAMES = 24
MAX_RAV_BYTES = 1024 * 1024
# The iPodJS Extras pane is half the 320x240 screen.  It plays the profile's
# selected emote instead of reusing the creator's turntable preview.
MENU_PREVIEW_SIZE = (160, 240)
EMOTE_CLIPS = (
    "jump", "throw", "faint", "sit-idle", "punch", "kick", "walk",
)
TURNTABLE_CLIP = "turntable"
CLIPS = (*EMOTE_CLIPS, TURNTABLE_CLIP)
PREVIEW_FRAME_WIDTH = 416
PREVIEW_FRAME_HEIGHT = 672
TURNTABLE_ANGLES = tuple(index * 360.0 / 24 for index in range(24))
STYLE_SLOTS = ("hair", "top", "bottom", "shoes")
STYLE_TABLES = {
    "hair": HAIR_STYLES,
    "top": TOP_STYLES,
    "bottom": BOTTOM_STYLES,
    "shoes": SHOE_STYLES,
}
STYLE_LABELS = {
    "original": "Original XNA",
    "boy-short": "Short crop",
    "girl-bob": "Bob",
    "shaved": "Shaved",
    "crew-tee": "Crew tee",
    "scoop-tee": "Scoop tee",
    "none": "No top",
    "jeans": "Jeans",
    "shorts": "Shorts",
    "sneakers": "Sneakers",
    "flats": "Flats",
    "heels": "Heels",
    "barefoot": "Barefoot",
}
STYLE_FIELDS = tuple(f"{slot}_style" for slot in STYLE_SLOTS)

# Every swatch tints one of Microsoft's own colour maps; `original` leaves the
# real texture untouched.
COLOUR_PALETTES = {
    "skin_colour": (
        ("original", "Original XNA", None),
        ("porcelain", "Porcelain", "#f3d7c0"),
        ("light", "Light", "#edc6a5"),
        ("medium", "Medium", "#bd8964"),
        ("tan", "Tan", "#a2704c"),
        ("deep", "Deep", "#704936"),
        ("rich", "Rich", "#4d3226"),
    ),
    "hair_colour": (
        ("original", "Original XNA", None),
        ("black", "Black", "#202124"),
        ("brown", "Brown", "#5b392b"),
        ("chestnut", "Chestnut", "#7a4a2f"),
        ("blond", "Blond", "#c49a55"),
        ("auburn", "Auburn", "#79352b"),
        ("red", "Red", "#a4442a"),
        ("silver", "Silver", "#b7b9bc"),
    ),
    "top_colour": (
        ("original", "Original XNA", None),
        ("xbox-green", "Xbox Green", "#69a72a"),
        ("blue", "Blue", "#3f74ad"),
        ("navy", "Navy", "#2b3a63"),
        ("red", "Red", "#a63239"),
        ("black", "Black", "#303236"),
        ("white", "White", "#d9dcde"),
        ("purple", "Purple", "#6d4b91"),
        ("orange", "Orange", "#c9702c"),
    ),
    "bottom_colour": (
        ("original", "Original XNA", None),
        ("denim", "Denim", "#3b526c"),
        ("black", "Black", "#292c30"),
        ("gray", "Gray", "#62676b"),
        ("khaki", "Khaki", "#8d7a5c"),
        ("white", "White", "#d4d6d8"),
    ),
    "shoes_colour": (
        ("original", "Original XNA", None),
        ("white", "White", "#d8d9d6"),
        ("black", "Black", "#292a2c"),
        ("brown", "Brown", "#654638"),
        ("red", "Red", "#90232b"),
        ("blue", "Blue", "#31527e"),
    ),
    "eye_colour": (
        ("original", "Original XNA", None),
        ("brown", "Brown", "#5d3a22"),
        ("blue", "Blue", "#3f7fb5"),
        ("green", "Green", "#3f7a49"),
        ("hazel", "Hazel", "#8a6a33"),
        ("grey", "Grey", "#6f7a80"),
    ),
    "brow_colour": (
        ("original", "Original XNA", None),
        ("black", "Black", "#26221f"),
        ("brown", "Brown", "#54382a"),
        ("blond", "Blond", "#a9854e"),
        ("auburn", "Auburn", "#6f3126"),
    ),
    "lip_colour": (
        ("original", "Original XNA", None),
        ("natural", "Natural", "#c4796f"),
        ("rose", "Rose", "#cf7d86"),
        ("red", "Red", "#b3323f"),
        ("berry", "Berry", "#8c3a58"),
    ),
}
COLOUR_FIELDS = tuple(COLOUR_PALETTES)
_COLOUR_VALUES = {
    field: {value: colour for value, _label, colour in entries}
    for field, entries in COLOUR_PALETTES.items()
}
DECAL_CHOICES = tuple(DECAL_LABELS)

# Generated designs are printed onto the real garment maps; each slot picks a
# pattern and a repeat, and the pattern is drawn in the slot's own colour
# against the shared accent.
DESIGN_SLOTS = ("top", "bottom", "shoes")
DESIGN_FIELDS = tuple(
    f"{slot}_{suffix}"
    for slot in DESIGN_SLOTS for suffix in ("design", "design_scale")
)
SCALE_LABELS = {
    "fine": "Fine", "small": "Small", "medium": "Medium",
    "large": "Large", "huge": "Huge",
}
ACCENT_PALETTE = (
    ("white", "White", "#e4e6e8"),
    ("black", "Black", "#26282b"),
    ("xbox-green", "Xbox Green", "#69a72a"),
    ("blue", "Blue", "#3f74ad"),
    ("red", "Red", "#a63239"),
    ("gold", "Gold", "#c49a55"),
    ("purple", "Purple", "#6d4b91"),
    ("orange", "Orange", "#c9702c"),
)
_ACCENT_VALUES = {value: colour for value, _label, colour in ACCENT_PALETTE}
# Imported Xbox 360 Marketplace items, validated against the installed pack.
MARKETPLACE_FIELDS = MARKETPLACE_SLOTS
APPEARANCE_FIELDS = (STYLE_FIELDS + COLOUR_FIELDS + DESIGN_FIELDS +
                     ("accent_colour", "decal") + MARKETPLACE_FIELDS)

# RockPod used to store five palette names against fixed sprite strips.  Those
# saved profiles still load: each old palette maps onto the matching colour.
LEGACY_COLOUR_FIELDS = {
    "skin": "skin_colour",
    "hair": "hair_colour",
    "top": "top_colour",
    "bottom": "bottom_colour",
    "shoes": "shoes_colour",
}

PROVENANCE_CLASSES = (
    "official-download",
    "owned-import",
    "archive-extracted",
)


def _sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _rgb565(red, green, blue):
    value = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
    return 0xF81E if value == TRANSPARENT_565 else value


def _rgb888(value):
    red = (value >> 11) & 0x1F
    green = (value >> 5) & 0x3F
    blue = value & 0x1F
    return (
        (red << 3) | (red >> 2),
        (green << 2) | (green >> 4),
        (blue << 3) | (blue >> 2),
    )


def _frame_pixels(image):
    rgba = image.convert("RGBA")
    result = []
    for red, green, blue, alpha in rgba.getdata():
        result.append(
            TRANSPARENT_565 if alpha < 96 else _rgb565(red, green, blue)
        )
    return result


def _encode_delta(previous, current, fill_runs=False):
    payload = bytearray()
    index = 0
    size = len(current)
    while index < size:
        run_limit = 0x4000 if fill_runs else 0x8000
        if previous is not None and current[index] == previous[index]:
            end = index + 1
            while (end < size and end - index < run_limit and
                   current[end] == previous[end]):
                end += 1
            payload += struct.pack("<H", end - index - 1)
            index = end
            continue
        if fill_runs:
            end = index + 1
            while (end < size and end - index < 0x4000 and
                   current[end] == current[index] and
                   (previous is None or current[end] != previous[end])):
                end += 1
            if end - index >= 3:
                payload += struct.pack("<HH", 0xC000 | (end - index - 1),
                                       current[index])
                index = end
                continue
        end = index + 1
        while end < size and end - index < run_limit:
            if previous is not None and current[end] == previous[end]:
                break
            end += 1
        payload += struct.pack("<H", 0x8000 | (end - index - 1))
        payload += struct.pack(f"<{end - index}H", *current[index:end])
        index = end
    return bytes(payload)


def encode_rav1(frames, frame_ms=125, *, opaque=False, keyframes=False):
    """Encode frames using the bounded RAV2 delta/fill stream.

    The historical function name is retained for callers. RAV2 adds constant
    fill runs and an opaque-stage flag while preserving the RAV1 header shape.
    """
    frames = [frame.convert("RGBA") for frame in frames]
    if not frames:
        raise ValueError("RAV1 requires at least one frame")
    width, height = frames[0].size
    if width > MAX_RAV_WIDTH or height > MAX_RAV_HEIGHT:
        raise ValueError("RAV1 frame exceeds the 104x168 device canvas")
    if len(frames) > MAX_RAV_FRAMES:
        raise ValueError("RAV1 exceeds the 24-frame limit")
    if any(frame.size != (width, height) for frame in frames):
        raise ValueError("RAV1 frames must have identical dimensions")

    chunks = []
    previous = None
    for frame in frames:
        pixels = _frame_pixels(frame)
        delta = _encode_delta(
            None if keyframes else previous, pixels, fill_runs=True
        )
        chunks.append(struct.pack("<I", len(delta)) + delta)
        previous = pixels

    table_bytes = 4 * len(chunks)
    data_offset = RAV_HEADER.size + table_bytes
    offsets = []
    cursor = data_offset
    for chunk in chunks:
        offsets.append(cursor)
        cursor += len(chunk)
    payload = b"".join(chunks)
    header = RAV_HEADER.pack(
        RAV2_MAGIC, width, height, len(chunks), int(frame_ms),
        RAV_FLAG_DELTA | RAV_FLAG_FILL |
        (RAV_FLAG_OPAQUE if opaque else 0) |
        (RAV_FLAG_KEYFRAMES if keyframes else 0),
        0, zlib.crc32(payload) & 0xFFFFFFFF,
    )
    encoded = header + struct.pack(f"<{len(offsets)}I", *offsets) + payload
    if len(encoded) > MAX_RAV_BYTES:
        raise ValueError(f"RAV1 clip is too large: {len(encoded)} bytes")
    return encoded


def decode_rav1(data):
    """Decode RAV1 bytes to RGBA PIL frames for RockPod's exact preview."""
    if len(data) < RAV_HEADER.size:
        raise ValueError("Truncated RAV1 header")
    magic, width, height, count, frame_ms, flags, _reserved, crc = (
        RAV_HEADER.unpack_from(data)
    )
    if magic not in (RAV1_MAGIC, RAV2_MAGIC) or not flags & RAV_FLAG_DELTA:
        raise ValueError("Unsupported RAV1 clip")
    if (not width or not height or width > MAX_RAV_WIDTH or
            height > MAX_RAV_HEIGHT or not count or count > MAX_RAV_FRAMES):
        raise ValueError("Invalid RAV1 dimensions or frame count")
    table_end = RAV_HEADER.size + count * 4
    if table_end > len(data):
        raise ValueError("Truncated RAV1 index")
    offsets = struct.unpack_from(f"<{count}I", data, RAV_HEADER.size)
    payload_start = offsets[0]
    if payload_start < table_end or payload_start > len(data):
        raise ValueError("Invalid RAV1 payload offset")
    if zlib.crc32(data[payload_start:]) & 0xFFFFFFFF != crc:
        raise ValueError("RAV1 payload CRC mismatch")

    pixels = [TRANSPARENT_565] * (width * height)
    frames = []
    for frame_index, offset in enumerate(offsets):
        if offset + 4 > len(data):
            raise ValueError("Truncated RAV1 frame")
        length = struct.unpack_from("<I", data, offset)[0]
        cursor = offset + 4
        end = cursor + length
        if end > len(data):
            raise ValueError("Truncated RAV1 frame payload")
        pixel_index = 0
        while cursor < end and pixel_index < len(pixels):
            command = struct.unpack_from("<H", data, cursor)[0]
            cursor += 2
            if magic == RAV2_MAGIC:
                kind = command & 0xC000
                run = (command & 0x3FFF) + 1
            else:
                kind = command & 0x8000
                run = (command & 0x7FFF) + 1
            if pixel_index + run > len(pixels):
                raise ValueError("RAV1 run exceeds frame")
            if magic == RAV2_MAGIC and kind == 0xC000:
                if cursor + 2 > end:
                    raise ValueError("Truncated RAV2 fill")
                value = struct.unpack_from("<H", data, cursor)[0]
                cursor += 2
                pixels[pixel_index:pixel_index + run] = [value] * run
            elif kind & 0x8000:
                byte_count = run * 2
                if cursor + byte_count > end:
                    raise ValueError("Truncated RAV1 literal")
                pixels[pixel_index:pixel_index + run] = struct.unpack_from(
                    f"<{run}H", data, cursor
                )
                cursor += byte_count
            elif frame_index == 0 or flags & RAV_FLAG_KEYFRAMES:
                raise ValueError("RAV1 keyframe may not skip pixels")
            pixel_index += run
        if cursor != end or pixel_index != len(pixels):
            raise ValueError("Malformed RAV1 frame")
        rgba = []
        for value in pixels:
            if value == TRANSPARENT_565:
                rgba.append((0, 0, 0, 0))
            else:
                rgba.append((*_rgb888(value), 255))
        image = Image.new("RGBA", (width, height))
        image.putdata(rgba)
        frames.append(image)
    return frames, frame_ms


def encode_ui_bank(rate, effects):
    """Build a bounded UIB1 bank from signed 16-bit stereo PCM effects."""
    names = ("navigate", "select", "back")
    if rate not in (44100, 48000):
        raise ValueError("UI bank rate must be 44100 or 48000 Hz")
    header_size = 12 + len(names) * 12
    payload = bytearray()
    entries = []
    for name in names:
        pcm = bytes(effects.get(name, b""))
        if len(pcm) % 4:
            raise ValueError(f"{name} PCM is not stereo-frame aligned")
        entries.append((header_size + len(payload), len(pcm)))
        payload += pcm
    result = bytearray(struct.pack("<4sIHH", b"UIB1", rate, len(names), 0))
    for index, (offset, size) in enumerate(entries):
        result += struct.pack("<III", index, offset, size)
    result += payload
    if len(result) > 96 * 1024:
        raise ValueError("UI sound bank exceeds the 96 KiB device budget")
    return bytes(result)


def profile_from_target(profile):
    """Collect every avatar field RockPod stores on a target profile.

    Sync paths used to name the handful of fields they knew about, so each new
    wardrobe option silently stopped reaching the device.  Forwarding the whole
    `xbox_avatar_*` namespace keeps that from happening again.
    """
    return {
        key[len("xbox_avatar_"):]: value
        for key, value in (profile or {}).items()
        if key.startswith("xbox_avatar_") and value is not None
    }


@dataclass(frozen=True)
class AvatarProfile:
    """One offline avatar: a real body, real wardrobe parts, and colours."""

    display_name: str = "OFFLINE PLAYER"
    body: str = "xna-boy"
    favorite_clip: str = "jump"
    hair_style: str = "original"
    top_style: str = "original"
    bottom_style: str = "original"
    shoes_style: str = "original"
    skin_colour: str = "original"
    hair_colour: str = "original"
    top_colour: str = "original"
    bottom_colour: str = "original"
    shoes_colour: str = "original"
    eye_colour: str = "original"
    brow_colour: str = "original"
    lip_colour: str = "original"
    top_design: str = "none"
    top_design_scale: str = DEFAULT_SCALE
    bottom_design: str = "none"
    bottom_design_scale: str = DEFAULT_SCALE
    shoes_design: str = "none"
    shoes_design_scale: str = DEFAULT_SCALE
    accent_colour: str = "black"
    decal: str = "original"
    costume: str = "none"
    marketplace_top: str = "none"
    headwear: str = "none"
    prop: str = "none"

    @classmethod
    def from_mapping(cls, mapping):
        def read(key):
            value = mapping.get(key)
            if value in (None, ""):
                value = mapping.get(f"xbox_avatar_{key}")
            return value

        name = " ".join(str(read("display_name") or
                            "OFFLINE PLAYER").split())[:15]
        body = str(read("body") or "xna-boy")
        clip = str(read("favorite_clip") or "jump")
        if body not in BODY_PRESETS:
            body = "xna-boy"
        if clip not in EMOTE_CLIPS:
            clip = "jump"
        family = BODY_PRESETS[body]["family"]

        values = {}
        for slot in STYLE_SLOTS:
            field = f"{slot}_style"
            choice = str(read(field) or "original")
            if choice not in STYLE_TABLES[slot] or \
                    not style_available(slot, choice, family):
                choice = "original"
            values[field] = choice
        for field in COLOUR_FIELDS:
            choice = read(field)
            if choice in (None, ""):
                legacy = next((old for old, new in LEGACY_COLOUR_FIELDS.items()
                               if new == field), None)
                choice = read(legacy) if legacy else None
            choice = str(choice or "original")
            values[field] = (choice if choice in _COLOUR_VALUES[field]
                             else "original")
        for slot in DESIGN_SLOTS:
            design = str(read(f"{slot}_design") or "none")
            values[f"{slot}_design"] = (
                design if design in DESIGN_CHOICES else "none"
            )
            scale = str(read(f"{slot}_design_scale") or DEFAULT_SCALE)
            values[f"{slot}_design_scale"] = (
                scale if scale in SCALES else DEFAULT_SCALE
            )
        accent = str(read("accent_colour") or "black")
        values["accent_colour"] = (
            accent if accent in _ACCENT_VALUES else "black"
        )
        decal = str(read("decal") or "original")
        values["decal"] = decal if decal in DECAL_LABELS else "original"
        for field in MARKETPLACE_FIELDS:
            item = str(read(field) or "none")
            values[field] = re.sub(r"[^a-z0-9-]", "", item.lower()) or "none"
        return cls(name or "OFFLINE PLAYER", body, clip, **values)

    def as_dict(self):
        return {
            "display_name": self.display_name,
            "body": self.body,
            "favorite_clip": self.favorite_clip,
            **{field: getattr(self, field) for field in APPEARANCE_FIELDS},
        }

    def render_mapping(self):
        """Translate the stored palette names into concrete render values."""
        mapping = {
            "body": self.body,
            "decal": self.decal,
        }
        for field in STYLE_FIELDS:
            mapping[field] = getattr(self, field)
        for field in COLOUR_FIELDS:
            mapping[field] = _COLOUR_VALUES[field][getattr(self, field)]
        accent = _ACCENT_VALUES[self.accent_colour]
        for field in MARKETPLACE_FIELDS:
            mapping[field] = getattr(self, field)
        for index, slot in enumerate(DESIGN_SLOTS):
            mapping[f"{slot}_design"] = getattr(self, f"{slot}_design")
            mapping[f"{slot}_design_scale"] = getattr(
                self, f"{slot}_design_scale"
            )
            # The garment's own swatch prints the pattern; `original` items
            # have no swatch of their own, so they fall back to a clean white.
            mapping[f"{slot}_design_primary"] = (
                mapping.get(f"{slot}_colour") or "#d9dcde"
            )
            mapping[f"{slot}_design_secondary"] = accent
            mapping[f"{slot}_design_seed"] = index
        return mapping

    def is_original_appearance(self):
        return (self.decal == "original" and
                all(getattr(self, field) == "none"
                    for field in MARKETPLACE_FIELDS) and
                all(getattr(self, f"{slot}_design") == "none"
                    for slot in DESIGN_SLOTS) and
                all(getattr(self, field) == "original"
                    for field in STYLE_FIELDS + COLOUR_FIELDS))

    def styles_for_body(self):
        return available_styles(BODY_PRESETS[self.body]["family"])


def _device_stage_background(size=(MAX_RAV_WIDTH, MAX_RAV_HEIGHT)):
    """Return the exact static NXE stage behind the device avatar sprite."""
    image = Image.new("RGBA", size, (226, 228, 230, 255))
    pixels = image.load()
    top = (247, 248, 249)
    bottom = (184, 188, 192)
    for y in range(size[1]):
        screen_y = y + 43
        if 48 <= screen_y < 202:
            index = screen_y - 48
            colour = tuple(
                top[channel] + (bottom[channel] - top[channel]) * index // 153
                for channel in range(3)
            )
        elif screen_y >= 202:
            colour = (70, 132, 15)
        else:
            colour = (226, 228, 230)
        for x in range(size[0]):
            pixels[x, y] = (*colour, 255)
    return image


def _menu_stage_background(size):
    """The NXE stage the Extras pane shows behind the avatar."""
    image = Image.new("RGBA", size, (226, 228, 230, 255))
    pixels = image.load()
    top = (247, 248, 249)
    bottom = (184, 188, 192)
    floor = int(size[1] * 0.86)
    for y in range(size[1]):
        if y < floor:
            colour = tuple(
                top[channel] + (bottom[channel] - top[channel]) * y // floor
                for channel in range(3)
            )
        else:
            colour = (70, 132, 15)
        for x in range(size[0]):
            pixels[x, y] = (*colour, 255)
    return image


def matte_device_frames(frames):
    background = _device_stage_background(frames[0].size)
    return [Image.alpha_composite(background, frame).convert("RGBA")
            for frame in frames]


class XboxAvatarService:
    """Build immutable avatar generations beside achievement generations."""

    def __init__(self, repo_root, profile=None):
        self.repo_root = os.path.abspath(repo_root)
        self.profile = AvatarProfile.from_mapping(profile or {})
        self.asset_root = os.path.join(
            self.repo_root, "assets", "ipodjs", "rockbox",
            "achievements", "avatar", "base",
        )
        self.source_root = os.path.join(
            self.repo_root, "assets", "ipodjs", "sources", "xbox360",
            "avatar",
        )
        self._rig_cache = None

    def _rig(self):
        if self._rig_cache is None:
            self._rig_cache = AvatarRig(self.repo_root)
        return self._rig_cache

    def available(self):
        return self._rig().available()

    def frames(self, clip, target="device", matte=False):
        """Render the real rig into preview or exact device frames."""
        if clip not in CLIPS:
            raise ValueError(f"Unsupported avatar clip: {clip}")
        rig = self._rig()
        if not rig.available():
            raise FileNotFoundError(rig.root)
        size = ((MAX_RAV_WIDTH, MAX_RAV_HEIGHT) if target == "device"
                else (PREVIEW_FRAME_WIDTH, PREVIEW_FRAME_HEIGHT))
        angles = (TURNTABLE_ANGLES if clip == TURNTABLE_CLIP else None)
        rendered = render_frames(
            rig, self.profile.render_mapping(), clip, size,
            repo_root=self.repo_root,
            supersample=2 if target == "device" else 1,
            angles=angles,
        )
        return matte_device_frames(rendered) if matte else rendered

    def frames_at_angles(self, angles, size=(280, 360)):
        """Render single turntable angles for the creator's live viewport."""
        rig = self._rig()
        if not rig.available():
            raise FileNotFoundError(rig.root)
        return render_frames(
            rig, self.profile.render_mapping(), TURNTABLE_CLIP, size,
            repo_root=self.repo_root, supersample=1, angles=list(angles),
        )

    def menu_preview_frames(self):
        """Render the profile's selected emote for the Achievements pane."""
        rig = self._rig()
        if not rig.available():
            raise FileNotFoundError(rig.root)
        frames = render_frames(
            rig, self.profile.render_mapping(), self.profile.favorite_clip,
            MENU_PREVIEW_SIZE, repo_root=self.repo_root, supersample=2,
        )
        background = _menu_stage_background(MENU_PREVIEW_SIZE)
        return [Image.alpha_composite(background, frame).convert("RGB")
                for frame in frames]

    def build_sync_assets(self, mount_root, stage_root, totals=None):
        if not self.available():
            return [], {"available": False}
        totals = dict(totals or {})
        sources = [
            *(os.path.join(self.asset_root, "sounds", f"ui-{rate}.uib")
              for rate in (44100, 48000)),
            *(os.path.join(self.asset_root, "sounds", f"unlock-{rate}.pcm")
              for rate in (44100, 48000)),
        ]
        if not all(os.path.isfile(path) for path in sources):
            return [], {"available": False, "missing": [
                path for path in sources if not os.path.isfile(path)
            ]}

        digest = hashlib.sha256(json.dumps(
            self.profile.as_dict(), sort_keys=True
        ).encode("utf-8"))
        for source in sources:
            digest.update(_sha256(source).encode("ascii"))
        digest.update(_sha256(
            os.path.join(self._rig().root, "geometry.npz")
        ).encode("ascii"))
        digest.update(json.dumps(
            self._rig().manifest, sort_keys=True
        ).encode("utf-8"))
        pack = MarketplacePack(self.repo_root)
        if pack.available():
            digest.update(_sha256(
                os.path.join(pack.root, "geometry.npz")
            ).encode("ascii"))
        generation = digest.hexdigest()[:16]
        work_root = os.path.join(os.path.abspath(stage_root), "avatar")
        generation_root = os.path.join(work_root, "generations", generation)
        shutil.rmtree(work_root, ignore_errors=True)
        os.makedirs(os.path.join(generation_root, "clips"), exist_ok=True)
        os.makedirs(os.path.join(generation_root, "sounds"), exist_ok=True)

        copied = []
        destination = os.path.join(generation_root, "portrait.80x80x24.bmp")
        _save_avatar_portrait(self.frames("jump", "preview")[0], destination)
        copied.append(destination)
        for clip in CLIPS:
            destination = os.path.join(generation_root, "clips", clip + ".rav")
            Path(destination).write_bytes(encode_rav1(
                self.frames(clip, "device", matte=True),
                200 if clip == TURNTABLE_CLIP else 125,
                opaque=True,
                keyframes=clip == TURNTABLE_CLIP,
            ))
            copied.append(destination)
        for rate in (44100, 48000):
            for stem, suffix in (("ui", "uib"), ("unlock", "pcm")):
                source = os.path.join(
                    self.asset_root, "sounds", f"{stem}-{rate}.{suffix}"
                )
                destination = os.path.join(
                    generation_root, "sounds", f"{stem}-{rate}.{suffix}"
                )
                shutil.copy2(source, destination)
                copied.append(destination)

        menu_root = os.path.join(generation_root, "menu")
        os.makedirs(menu_root, exist_ok=True)
        for index, frame in enumerate(self.menu_preview_frames()):
            destination = os.path.join(menu_root, f"frame-{index:02d}.bmp")
            frame.save(destination, "BMP")
            copied.append(destination)

        profile_path = os.path.join(generation_root, "profile.v1.tsv")
        atomic_write_text(
            profile_path,
            "key\tvalue\n"
            f"display_name\t{self.profile.display_name}\n"
            f"body\t{self.profile.body}\n"
            f"favorite_clip\t{self.profile.favorite_clip}\n"
            + "".join(
                f"{field}\t{getattr(self.profile, field)}\n"
                for field in APPEARANCE_FIELDS
            ) +
            f"games\t{int(totals.get('games') or 0)}\n"
            f"unlocked\t{int(totals.get('unlocked') or 0)}\n"
            f"achievements\t{int(totals.get('achievements') or 0)}\n"
            f"gamerscore\t{int(totals.get('gamerscore') or 0)}\n",
        )
        copied.append(profile_path)

        provenance = {
            "schema": 1,
            "class": "official-download",
            "character_source": (
                "Microsoft XNA Avatar Animation Pack 4.0 (FBX/Maya)"
            ),
            "animation_source": (
                "https://github.com/tgc-utn/xna-game-studio/tree/master/"
                "Samples/AvatarAnimPack_4_0_FBX"
            ),
            "facial_texture_source": (
                "https://github.com/tgc-utn/xna-game-studio/tree/master/"
                "Samples/AvatarAnimPack_4_0_Maya/Textures/"
                "Avatar_face_texture"
            ),
            "license": "Microsoft Permissive License (Ms-PL)",
            "ui_sound_source": "Xbox 360 SystemUpdate 17559 AvatarEditor.xex",
            "unlock_sound_source": (
                "Xbox Wire / Major Nelson Achievement Unlocked master; "
                "non-commercial personal use"
            ),
            "generated_at": int(time.time()),
        }
        provenance_path = os.path.join(generation_root, "provenance.json")
        atomic_write_text(
            provenance_path, json.dumps(provenance, indent=2, sort_keys=True) + "\n"
        )
        copied.append(provenance_path)

        manifest_path = os.path.join(generation_root, "manifest.v1.tsv")
        lines = ["path\tbytes\tsha256"]
        for path in sorted(copied):
            rel = os.path.relpath(path, generation_root).replace(os.sep, "/")
            lines.append(f"{rel}\t{os.path.getsize(path)}\t{_sha256(path)}")
        atomic_write_text(manifest_path, "\n".join(lines) + "\n")
        copied.append(manifest_path)
        current_path = os.path.join(work_root, "current")
        atomic_write_text(current_path, generation + "\n")

        assets = []
        for path in sorted(copied):
            rel = os.path.relpath(path, work_root).replace(os.sep, "/")
            assets.append(self._asset(path, f"{AVATAR_ROOT}/{rel}"))
        assets.extend(self._preference_seed_assets(work_root, mount_root))
        assets.append(self._asset(current_path, f"{AVATAR_ROOT}/current"))
        return assets, {
            "available": True,
            "generation": generation,
            "body": self.profile.body,
            "clips": len(CLIPS),
            "appearance": {
                field: getattr(self.profile, field)
                for field in APPEARANCE_FIELDS
            },
        }

    @staticmethod
    def _asset(source, destination):
        source = os.path.abspath(str(source))
        return {
            "kind": "achievement_avatar",
            "source_rel": os.path.basename(source),
            "source_abs": source,
            "destination_rel": destination,
            "exists": True,
            "size": os.path.getsize(source),
        }

    @staticmethod
    def _preference_seed_assets(work_root, mount_root):
        destination = f"{AVATAR_ROOT}/../state/avatar-preferences.v1.tsv"
        mounted = os.path.join(
            os.path.abspath(mount_root), ".rockbox", "achievements", "state",
            "avatar-preferences.v1.tsv",
        )
        if os.path.isfile(mounted):
            return []
        seed = os.path.join(work_root, "avatar-preferences.v1.tsv")
        atomic_write_text(
            seed,
            "key\tvalue\n"
            "motion\tfull\n"
            "sounds\tfull\n"
            "sounds_over_music\ton\n"
            "idle_emotes\ton\n",
        )
        return [XboxAvatarService._asset(
            seed, ".rockbox/achievements/state/avatar-preferences.v1.tsv"
        )]


def _save_avatar_portrait(frame, path):
    """Compose a profile tile from the real customized render."""
    alpha = frame.getchannel("A")
    bounds = alpha.getbbox() or (0, 0, frame.width, frame.height)
    left, top, right, _bottom = bounds
    body_width = max(1, right - left)
    head_size = max(32, min(frame.width, int(body_width * 1.55)))
    center_x = (left + right) // 2
    crop_top = max(0, top - 2)
    avatar = frame.crop((
        max(0, center_x - head_size // 2), crop_top,
        min(frame.width, center_x + head_size // 2),
        min(frame.height, crop_top + head_size),
    ))
    avatar.thumbnail((72, 72), Image.Resampling.LANCZOS)
    canvas = Image.new("RGB", (80, 80), (222, 224, 226))
    draw = ImageDraw.Draw(canvas)
    for y in range(80):
        draw.line((0, y, 79, y), fill=(112, max(115, 184 - y // 3), 38))
    canvas.paste(
        avatar.convert("RGB"), ((80 - avatar.width) // 2, 80 - avatar.height),
        avatar.getchannel("A"),
    )
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path, "BMP")
