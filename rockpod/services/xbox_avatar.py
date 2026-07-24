"""Authentic Xbox 360 avatar profiles and bounded iPod frame packs.

The bundled character pixels are renders of Microsoft's Ms-PL XNA Avatar
Animation Pack.  RockPod only composes profiles and converts those real
renders to the small RAV1 format consumed by the Rockbox plugin.
"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import struct
import time
import zlib
from collections import deque
from colorsys import rgb_to_hsv
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageOps

from services.file_safety import atomic_write_text


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
EMOTE_CLIPS = (
    "jump", "throw", "faint", "sit-idle", "punch", "kick", "walk",
)
TURNTABLE_CLIP = "turntable"
CLIPS = (*EMOTE_CLIPS, TURNTABLE_CLIP)
BODY_PRESETS = ("xna-boy", "xna-girl", "xna-girl-heels")
BODY_FAMILIES = {
    "xna-boy": "xna-boy",
    "xna-girl": "xna-girl",
    "xna-girl-heels": "xna-girl",
}
MASTER_FRAME_WIDTH = 416
MASTER_FRAME_HEIGHT = 672
FACE_PART_FILES = {
    "left_eye": "left-eye.tga",
    "right_eye": "right-eye.tga",
    "eyebrow": "eyebrow.tga",
    "mouth": "mouth.tga",
}
APPEARANCE_PALETTES = {
    "skin": (
        ("original", "Original XNA", None),
        ("light", "Light", "#edc6a5"),
        ("medium", "Medium", "#bd8964"),
        ("deep", "Deep", "#704936"),
    ),
    "hair": (
        ("original", "Original XNA", None),
        ("black", "Black", "#202124"),
        ("brown", "Brown", "#5b392b"),
        ("blond", "Blond", "#c49a55"),
        ("auburn", "Auburn", "#79352b"),
    ),
    "top": (
        ("original", "Original XNA", None),
        ("xbox-green", "Xbox Green", "#69a72a"),
        ("blue", "Blue", "#3f74ad"),
        ("red", "Red", "#a63239"),
        ("black", "Black", "#303236"),
        ("white", "White", "#d9dcde"),
    ),
    "bottom": (
        ("original", "Original XNA", None),
        ("denim", "Denim", "#3b526c"),
        ("black", "Black", "#292c30"),
        ("gray", "Gray", "#62676b"),
        ("khaki", "Khaki", "#8d7a5c"),
    ),
    "shoes": (
        ("original", "Original XNA", None),
        ("white", "White", "#d8d9d6"),
        ("black", "Black", "#292a2c"),
        ("brown", "Brown", "#654638"),
        ("red", "Red", "#90232b"),
    ),
}
APPEARANCE_FIELDS = tuple(APPEARANCE_PALETTES)
_PALETTE_COLORS = {
    category: {value: color for value, _label, color in entries}
    for category, entries in APPEARANCE_PALETTES.items()
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


@dataclass(frozen=True)
class AvatarProfile:
    display_name: str = "OFFLINE PLAYER"
    body: str = "xna-boy"
    favorite_clip: str = "jump"
    skin: str = "original"
    hair: str = "original"
    top: str = "original"
    bottom: str = "original"
    shoes: str = "original"

    @classmethod
    def from_mapping(cls, mapping):
        name = " ".join(str(mapping.get("display_name") or
                            "OFFLINE PLAYER").split())[:15]
        body = str(mapping.get("body") or "xna-boy")
        clip = str(mapping.get("favorite_clip") or "jump")
        if body not in BODY_PRESETS:
            body = "xna-boy"
        if clip not in EMOTE_CLIPS:
            clip = "jump"
        appearance = {}
        for field in APPEARANCE_FIELDS:
            value = str(mapping.get(field) or mapping.get(
                f"xbox_avatar_{field}") or "original")
            appearance[field] = (
                value if value in _PALETTE_COLORS[field] else "original"
            )
        return cls(name or "OFFLINE PLAYER", body, clip, **appearance)

    def as_dict(self):
        return {
            "display_name": self.display_name,
            "body": self.body,
            "favorite_clip": self.favorite_clip,
            **{field: getattr(self, field) for field in APPEARANCE_FIELDS},
        }

    def is_original_appearance(self):
        return all(getattr(self, field) == "original"
                   for field in APPEARANCE_FIELDS)


def _connected_components(points, width, height):
    remaining = set(points)
    components = []
    while remaining:
        start = remaining.pop()
        queue = deque((start,))
        component = [start]
        while queue:
            index = queue.popleft()
            x, y = index % width, index // width
            for nx, ny in ((x - 1, y), (x + 1, y),
                           (x, y - 1), (x, y + 1)):
                if 0 <= nx < width and 0 <= ny < height:
                    neighbor = ny * width + nx
                    if neighbor in remaining:
                        remaining.remove(neighbor)
                        component.append(neighbor)
                        queue.append(neighbor)
        if len(component) >= 2:
            components.append(component)
    return components


@lru_cache(maxsize=8)
def _load_official_face_parts(face_root, body, signature):
    """Load the facial layers referenced by Microsoft's XNA Maya scene."""
    del signature  # Asset mtimes are included in the cache key.
    body_root = Path(face_root) / body
    parts = {}
    for name, filename in FACE_PART_FILES.items():
        with Image.open(body_root / filename) as source:
            part = source.convert("RGBA")
        bounds = part.getchannel("A").getbbox()
        if not bounds:
            raise ValueError(f"Empty official XNA face texture: {filename}")
        parts[name] = part.crop(bounds)
    return parts


def load_official_face_parts(face_root, body):
    paths = [Path(face_root) / body / filename
             for filename in FACE_PART_FILES.values()]
    signature = tuple(path.stat().st_mtime_ns for path in paths)
    return _load_official_face_parts(str(Path(face_root)), body, signature)


def _head_skin_mask(masks, width, height):
    """Return the skin island attached to the real rendered hair mesh."""
    hair_bounds = masks["hair"].getbbox()
    if not hair_bounds:
        return None, None
    hair_x = (hair_bounds[0] + hair_bounds[2] - 1) / 2.0
    hair_y = (hair_bounds[1] + hair_bounds[3] - 1) / 2.0
    skin_points = {
        index for index, value in enumerate(masks["skin"].tobytes()) if value
    }
    components = _connected_components(skin_points, width, height)
    if not components:
        return None, None
    component = min(
        components,
        key=lambda points: min(
            (index % width - hair_x) ** 2 +
            (index // width - hair_y) ** 2
            for index in points
        ),
    )
    xs = [index % width for index in component]
    ys = [index // width for index in component]
    bounds = (min(xs), min(ys), max(xs) + 1, max(ys) + 1)
    if len(component) < 60 or bounds[2] - bounds[0] < 11 or \
            bounds[3] - bounds[1] < 11:
        return None, None
    mask = Image.new("L", (width, height), 0)
    data = bytearray(width * height)
    for index in component:
        data[index] = 255
    mask.frombytes(bytes(data))
    return mask.filter(ImageFilter.MaxFilter(3)), bounds


def render_official_face(frame, body, face_parts, masks=None,
                         head_bounds=None):
    """Project genuine XNA eye, brow, and mouth layers onto the head render."""
    if not face_parts:
        return frame.convert("RGBA")
    rgba = frame.convert("RGBA")
    masks = masks or avatar_material_masks(rgba, body)
    head_mask, detected = _head_skin_mask(masks, rgba.width, rgba.height)
    if head_mask is None:
        return rgba
    bounds = head_bounds or detected
    left, top, right, bottom = bounds
    face_width = right - left
    face_height = bottom - top
    eye_width = max(3, round(face_width * 0.18))
    eye_height = max(2, round(face_height * 0.11))
    brow_width = eye_width
    brow_height = max(1, round(face_height * 0.055))
    mouth_width = max(4, round(face_width * 0.29))
    mouth_height = max(2, round(face_height * 0.10))
    eye_y = top + round(face_height * 0.36) - eye_height // 2
    brow_y = top + round(face_height * 0.24) - brow_height // 2
    mouth_y = top + round(face_height * 0.66) - mouth_height // 2
    centers = (
        left + round(face_width * 0.34),
        left + round(face_width * 0.66),
    )
    placements = (
        (face_parts["left_eye"], centers[0] - eye_width // 2,
         eye_y, eye_width, eye_height),
        (face_parts["right_eye"], centers[1] - eye_width // 2,
         eye_y, eye_width, eye_height),
        (face_parts["eyebrow"], centers[0] - brow_width // 2,
         brow_y, brow_width, brow_height),
        (ImageOps.mirror(face_parts["eyebrow"]),
         centers[1] - brow_width // 2, brow_y, brow_width, brow_height),
        (face_parts["mouth"],
         left + (face_width - mouth_width) // 2,
         mouth_y, mouth_width, mouth_height),
    )
    result = rgba.copy()
    for part, x, y, width, height in placements:
        scaled = part.resize((width, height), Image.Resampling.LANCZOS)
        layer = Image.new("RGBA", rgba.size, (0, 0, 0, 0))
        layer.alpha_composite(scaled, (x, y))
        layer.putalpha(ImageChops.multiply(layer.getchannel("A"), head_mask))
        result.alpha_composite(layer)
    result.putalpha(rgba.getchannel("A"))
    return result


def _stabilized_face_bounds(masks, width, height):
    """Track a fixed-size facial plane through an animation clip."""
    detected = [
        _head_skin_mask(mask, width, height)[1]
        for mask in masks
    ]
    valid = [bounds for bounds in detected if bounds]
    if not valid:
        return [None] * len(masks)
    widths = sorted(right - left for left, _top, right, _bottom in valid)
    heights = sorted(bottom - top for _left, top, _right, bottom in valid)
    stable_width = widths[len(widths) // 2]
    stable_height = heights[len(heights) // 2]
    result = []
    previous = valid[0]
    for bounds in detected:
        bounds = bounds or previous
        left, top, right, bottom = bounds
        center_x = (left + right) // 2
        center_y = (top + bottom) // 2
        stable = (
            center_x - stable_width // 2,
            center_y - stable_height // 2,
            center_x - stable_width // 2 + stable_width,
            center_y - stable_height // 2 + stable_height,
        )
        result.append(stable)
        previous = bounds
    return result


def avatar_material_masks(frame, body):
    """Derive semantic regions from the original XNA render materials.

    These masks are not illustrated assets.  They are deterministic selections
    of pixels already emitted by the licensed XNA mesh/material render.  This
    keeps every silhouette, texture edge, pose, and anti-aliased contour from
    the source character.
    """
    rgba = frame.convert("RGBA")
    width, height = rgba.size
    masks = {field: set() for field in APPEARANCE_FIELDS}
    red_material = set()
    for index, (red, green, blue, alpha) in enumerate(rgba.getdata()):
        if alpha < 64:
            continue
        maximum = max(red, green, blue)
        minimum = min(red, green, blue)
        hue, saturation, _value = rgb_to_hsv(
            red / 255.0, green / 255.0, blue / 255.0
        )
        hue *= 360.0

        # Warm, moderately saturated XNA skin material.  The red shoe/short
        # material is much more saturated and is intentionally excluded.
        if (8 <= hue <= 42 and 0.08 <= saturation <= 0.48 and
                55 <= maximum <= 238 and red > green > blue):
            masks["skin"].add(index)
            continue

        if (red >= green * 1.42 and red >= blue * 1.32 and
                saturation >= 0.34 and maximum >= 48):
            red_material.add(index)
            continue

        if BODY_FAMILIES.get(body, body) == "xna-boy":
            cool = blue >= red + 7 and blue >= green + 2
            if cool and maximum >= 88:
                masks["top"].add(index)
            elif cool and maximum < 112:
                masks["bottom"].add(index)
            elif maximum < 78 and maximum - minimum < 27:
                masks["hair"].add(index)
        else:
            purple = (red >= green + 3 and blue >= green + 3 and
                      maximum >= 86 and saturation <= 0.28)
            if purple:
                masks["top"].add(index)
            elif maximum < 82 and maximum - minimum < 28:
                masks["hair"].add(index)

    if BODY_FAMILIES.get(body, body) == "xna-girl" and red_material:
        components = _connected_components(red_material, width, height)
        top_points = masks["top"]
        if top_points:
            tx = sum(index % width for index in top_points) / len(top_points)
            ty = sum(index // width for index in top_points) / len(top_points)
        else:
            tx, ty = width / 2, height / 2
        ranked = sorted((
            (min(
                (index % width - tx) ** 2 + (index // width - ty) ** 2
                for index in component
            ), component)
            for component in components
        ), key=lambda item: item[0])
        if ranked:
            nearest = ranked[0][0]
            for distance, component in ranked:
                if distance <= max(144, nearest + 64):
                    masks["bottom"].update(component)
                else:
                    masks["shoes"].update(component)
    else:
        masks["shoes"].update(red_material)

    # The source sneakers use a neutral upper with a small red sole. Grow from
    # that authentic sole material into adjacent otherwise-unclassified shoe
    # pixels. Classified legs and clothing form a hard boundary.
    if masks["shoes"]:
        shoe_seed = Image.new("L", rgba.size, 0)
        shoe_data = bytearray(width * height)
        for index in masks["shoes"]:
            shoe_data[index] = 255
        shoe_seed.frombytes(bytes(shoe_data))
        grown = shoe_seed.filter(ImageFilter.MaxFilter(9)).tobytes()
        occupied = set().union(*(masks[field] for field in APPEARANCE_FIELDS
                                 if field != "shoes"))
        rgba_pixels = list(rgba.getdata())
        for index, amount in enumerate(grown):
            if (amount and index not in occupied and
                    rgba_pixels[index][3] >= 64):
                masks["shoes"].add(index)

    # Select the largest neutral-dark island as hair.  This rejects small
    # shadow flecks while retaining the actual hair mesh in every pose.
    hair_components = _connected_components(masks["hair"], width, height)
    masks["hair"] = (
        set(max(hair_components, key=len)) if hair_components else set()
    )

    result = {}
    for field, points in masks.items():
        mask = Image.new("L", rgba.size, 0)
        data = bytearray(width * height)
        for index in points:
            data[index] = 255
        mask.frombytes(bytes(data))
        result[field] = mask
    return result


def _hex_rgb(value):
    value = value.lstrip("#")
    return tuple(int(value[index:index + 2], 16) for index in (0, 2, 4))


def _tint_material(image, mask, target, source_luma):
    if target is None:
        return image
    rgba = image.convert("RGBA")
    pixels = list(rgba.getdata())
    mask_data = mask.tobytes()
    target_rgb = _hex_rgb(target)
    for index, amount in enumerate(mask_data):
        if not amount:
            continue
        red, green, blue, alpha = pixels[index]
        old_luma = (54 * red + 183 * green + 19 * blue) / 256.0
        # Preserve rendered illumination while letting palette brightness move
        # the material naturally lighter or darker than its source swatch.
        scale = max(0.18, old_luma / max(1.0, source_luma))
        desired = tuple(min(255, round(channel * scale)) for channel in target_rgb)
        blend = amount / 255.0
        pixels[index] = (
            round(red + (desired[0] - red) * blend),
            round(green + (desired[1] - green) * blend),
            round(blue + (desired[2] - blue) * blend),
            alpha,
        )
    rgba.putdata(pixels)
    return rgba


def render_avatar_frame(frame, profile, masks=None, face_parts=None):
    """Apply a normalized appearance profile to one genuine XNA frame."""
    profile = profile if isinstance(profile, AvatarProfile) else \
        AvatarProfile.from_mapping(profile or {})
    masks = masks or avatar_material_masks(frame, profile.body)
    source_luma = {
        "skin": 163, "hair": 34, "top": 153,
        "bottom": 58 if BODY_FAMILIES[profile.body] == "xna-boy" else 80,
        "shoes": 78,
    }
    result = frame.convert("RGBA")
    if not profile.is_original_appearance():
        for field in APPEARANCE_FIELDS:
            result = _tint_material(
                result, masks[field],
                _PALETTE_COLORS[field][getattr(profile, field)],
                source_luma[field],
            )
    return render_official_face(
        result, profile.body, face_parts, masks=masks
    )


@lru_cache(maxsize=32)
def _source_bundle(path, body, frame_width, modified_ns):
    del modified_ns  # Included in the cache key so replaced assets invalidate.
    frames = tuple(load_strip(path, frame_width=frame_width))
    masks = tuple(avatar_material_masks(frame, body) for frame in frames)
    return frames, masks


def _fit_master_frames(frames, size):
    """Fit a whole clip into one stable viewport without per-frame jitter."""
    alpha_bounds = [frame.getchannel("A").getbbox() for frame in frames]
    valid = [bounds for bounds in alpha_bounds if bounds]
    if not valid:
        return [Image.new("RGBA", size, (0, 0, 0, 0)) for _ in frames]
    union = (
        min(bounds[0] for bounds in valid),
        min(bounds[1] for bounds in valid),
        max(bounds[2] for bounds in valid),
        max(bounds[3] for bounds in valid),
    )
    margin = max(2, round(min(size) * 0.018))
    available = (size[0] - margin * 2, size[1] - margin * 2)
    source_width = max(1, union[2] - union[0])
    source_height = max(1, union[3] - union[1])
    scale = min(available[0] / source_width, available[1] / source_height)
    scaled_size = (
        max(1, round(frames[0].width * scale)),
        max(1, round(frames[0].height * scale)),
    )
    union_center_x = (union[0] + union[2]) * scale / 2.0
    union_bottom = union[3] * scale
    offset_x = round(size[0] / 2.0 - union_center_x)
    offset_y = round(size[1] - margin - union_bottom)
    result = []
    for frame in frames:
        scaled = frame.resize(scaled_size, Image.Resampling.LANCZOS)
        canvas = Image.new("RGBA", size, (0, 0, 0, 0))
        canvas.alpha_composite(scaled, (offset_x, offset_y))
        result.append(canvas)
    return result


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
            divisor = 153
            color = tuple(
                top[channel] + (bottom[channel] - top[channel]) * index //
                divisor for channel in range(3)
            )
        elif screen_y >= 202:
            color = (70, 132, 15)
        else:
            color = (226, 228, 230)
        for x in range(size[0]):
            pixels[x, y] = (*color, 255)
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
        self.face_root = os.path.join(self.source_root, "faces")

    def _face_sources(self):
        family = BODY_FAMILIES[self.profile.body]
        return [
            os.path.join(
                self.face_root, family, filename
            )
            for filename in FACE_PART_FILES.values()
        ]

    def available(self):
        sources = [
            os.path.join(
                self.source_root, "master", self.profile.body,
                clip + ".rgba.png"
            )
            for clip in CLIPS
        ] + self._face_sources()
        return all(os.path.isfile(path) for path in sources)

    def frames(self, clip, target="device", matte=False):
        """Return stable high-resolution preview or exact device frames."""
        if clip not in CLIPS:
            raise ValueError(f"Unsupported avatar clip: {clip}")
        source = os.path.join(
            self.source_root, "master", self.profile.body,
            clip + ".rgba.png"
        )
        if os.path.isfile(source):
            stat = os.stat(source)
            frames, masks = _source_bundle(
                source, self.profile.body, MASTER_FRAME_WIDTH,
                stat.st_mtime_ns
            )
            family = BODY_FAMILIES[self.profile.body]
            face_parts = load_official_face_parts(
                self.face_root, family
            )
            if clip == TURNTABLE_CLIP:
                rendered = []
                for index, (frame, mask) in enumerate(zip(frames, masks)):
                    angle = index * 360.0 / len(frames)
                    front_angle = min(angle, 360.0 - angle)
                    base = render_avatar_frame(frame, self.profile, mask)
                    rendered.append(render_official_face(
                        base, family,
                        face_parts if front_angle <= 60.0 else None,
                        masks=mask,
                    ))
            else:
                face_bounds = _stabilized_face_bounds(
                    masks, frames[0].width, frames[0].height
                )
                rendered = [render_official_face(
                    render_avatar_frame(frame, self.profile, mask),
                    family, face_parts, masks=mask, head_bounds=bounds,
                ) for frame, mask, bounds in zip(frames, masks, face_bounds)]
            size = ((MAX_RAV_WIDTH, MAX_RAV_HEIGHT) if target == "device"
                    else (MASTER_FRAME_WIDTH, MASTER_FRAME_HEIGHT))
            fitted = (_fit_master_frames(rendered, size)
                      if target == "device" else rendered)
            return matte_device_frames(fitted) if matte else fitted
        raise FileNotFoundError(source)

    def build_sync_assets(self, mount_root, stage_root, totals=None):
        if not self.available():
            return [], {"available": False}
        totals = dict(totals or {})
        sources = [
            *(os.path.join(self.asset_root, "sounds", f"ui-{rate}.uib")
              for rate in (44100, 48000)),
            *(os.path.join(self.asset_root, "sounds", f"unlock-{rate}.pcm")
              for rate in (44100, 48000)),
            *self._face_sources(),
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
        for clip in CLIPS:
            source = os.path.join(
                self.source_root, "master", self.profile.body,
                clip + ".rgba.png"
            )
            digest.update(_sha256(source).encode("ascii"))
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


def load_strip(path, frame_width=104):
    """Split a bundled RGBA sprite strip into ordered animation frames."""
    with Image.open(path) as image:
        image = image.convert("RGBA")
        if image.width % frame_width:
            raise ValueError("Avatar strip width is not frame aligned")
        return [
            image.crop((x, 0, x + frame_width, image.height))
            for x in range(0, image.width, frame_width)
        ]


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
