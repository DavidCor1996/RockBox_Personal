"""Render real Xbox 360 avatars from Microsoft's XNA rig pack.

`tools/xbox_avatar_rigpack.py` extracts the genuine mesh parts, every original
UV channel, the embedded Microsoft texture maps, and the baked per-frame
skinned vertex positions from the Ms-PL XNA Avatar Animation Pack.  This module
composes a wardrobe from those real parts and rasterizes them through the
authentic Xbox avatar material model.

Nothing here draws character art.  Faces come from Microsoft's own eye, brow,
mouth, eye-shadow, and facial-hair textures projected through the head's own
dedicated UV channels, which is why the face lands correctly in every pose and
at every turntable angle.  Colour choices tint the original colour maps and
decals print real in-tree artwork across the front of the real garment mesh.
"""

from __future__ import annotations

import json
import os

import numpy as np
from PIL import Image

from services.xbox_avatar_designs import apply_design, generate


RIG_RELATIVE = "assets/ipodjs/sources/xbox360/avatar/rig"

# Order matters only for coincident surfaces; the depth buffer does the rest.
DRAW_ORDER = ("body", "head", "bottom", "top", "shoes", "hair")

# Real garment/skin colour maps are tinted; the head composites its own layers.
SKIN_CHANNEL = "__ColorMap"
HEAD_LAYER_ORDER = (
    "__SkinFeaturesIntensityMap",
    "__FacialHairIntensityMap",
    "__EyeShadowIntensityMap",
    "__MouthIntensityMap",
    "__EyeIntensityMap",
    "__EyeBrowIntensityMap",
)

# Studio key/fill matching the Xbox avatar stage lighting.
KEY_DIRECTION = np.array([-0.34, 0.42, 1.0])
FILL_DIRECTION = np.array([0.55, 0.10, -0.65])
AMBIENT = 0.62
KEY_STRENGTH = 0.40
FILL_STRENGTH = 0.12

# Clothing sits fractionally in front of skin so coincident faces do not fight.
DEPTH_BIAS = {"body": 0.0, "head": 0.0, "hair": -0.35,
              "top": -0.30, "bottom": -0.30, "shoes": -0.30}


def rig_root(repo_root):
    return os.path.join(os.path.abspath(repo_root), *RIG_RELATIVE.split("/"))


class AvatarRig:
    """Lazy reader for the extracted real-geometry rig pack."""

    def __init__(self, repo_root):
        self.root = rig_root(repo_root)
        self._manifest = None
        self._geometry = None
        self._clips = {}
        self._textures = {}

    # -- pack access ------------------------------------------------------

    def available(self):
        return (os.path.isfile(os.path.join(self.root, "rig.json")) and
                os.path.isfile(os.path.join(self.root, "geometry.npz")))

    @property
    def manifest(self):
        if self._manifest is None:
            with open(os.path.join(self.root, "rig.json"), encoding="utf-8") as handle:
                self._manifest = json.load(handle)
        return self._manifest

    @property
    def geometry(self):
        if self._geometry is None:
            self._geometry = np.load(os.path.join(self.root, "geometry.npz"))
        return self._geometry

    def clip_positions(self, clip):
        if clip not in self._clips:
            path = os.path.join(self.root, "clips", f"{clip}.npz")
            self._clips[clip] = dict(np.load(path))
        return self._clips[clip]

    def texture(self, name):
        """Return one Microsoft texture map as float RGBA in 0..1."""
        if name not in self._textures:
            entry = self.manifest["textures"].get(name)
            if not entry or not entry["file"]:
                self._textures[name] = None
            else:
                path = os.path.join(self.root, "textures", entry["file"])
                with Image.open(path) as source:
                    image = source.convert("RGBA")
                self._textures[name] = (
                    np.asarray(image, dtype=np.float32) / 255.0
                )
        return self._textures[name]

    def part_channels(self, part):
        return self.manifest["parts"][part]["channels"]

    def part_textures(self, part):
        return self.manifest["parts"][part]["textures"]


# ---------------------------------------------------------------------------
# Sampling and shading helpers
# ---------------------------------------------------------------------------


def sample(texture, uv, clamp):
    """Nearest-neighbour sample of a real Microsoft map at UV coordinates."""
    height, width = texture.shape[:2]
    u = uv[:, 0]
    v = 1.0 - uv[:, 1]
    if clamp:
        u = np.clip(u, 0.0, 1.0 - 1e-6)
        v = np.clip(v, 0.0, 1.0 - 1e-6)
    else:
        u = np.mod(u, 1.0)
        v = np.mod(v, 1.0)
    x = np.clip((u * width).astype(np.int32), 0, width - 1)
    y = np.clip((v * height).astype(np.int32), 0, height - 1)
    return texture[y, x]


def vertex_normals(positions, triangles):
    normals = np.zeros_like(positions)
    a = positions[triangles[:, 0]]
    b = positions[triangles[:, 1]]
    c = positions[triangles[:, 2]]
    face = np.cross(b - a, c - a)
    for column in range(3):
        np.add.at(normals, triangles[:, column], face)
    length = np.linalg.norm(normals, axis=1, keepdims=True)
    return normals / np.maximum(length, 1e-8)


def _tint(colour, target):
    """Recolour a real map while keeping its original shading and detail."""
    if target is None:
        return colour
    luma = (colour[..., :3] * np.array([0.2126, 0.7152, 0.0722])).sum(-1)
    reference = max(float(np.median(luma[luma > 0.02])) if (luma > 0.02).any()
                    else 0.5, 1e-3)
    scale = np.clip(luma / reference, 0.18, 1.85)[..., None]
    tinted = np.clip(np.asarray(target, dtype=np.float32) * scale, 0.0, 1.0)
    return np.concatenate([tinted, colour[..., 3:]], axis=-1)


def hex_rgb(value):
    if value is None:
        return None
    value = value.lstrip("#")
    return np.array([int(value[index:index + 2], 16) / 255.0
                     for index in (0, 2, 4)], dtype=np.float32)


# ---------------------------------------------------------------------------
# Rasterizer
# ---------------------------------------------------------------------------


class Frame:
    """A depth-buffered RGBA target in linear float space."""

    def __init__(self, width, height):
        self.width = width
        self.height = height
        self.colour = np.zeros((height, width, 4), dtype=np.float32)
        self.depth = np.full((height, width), np.inf, dtype=np.float32)

    def to_image(self, supersample=1):
        data = np.clip(self.colour, 0.0, 1.0)
        rgba = (data * 255.0 + 0.5).astype(np.uint8)
        image = Image.fromarray(rgba, "RGBA")
        if supersample > 1:
            image = image.resize(
                (self.width // supersample, self.height // supersample),
                Image.Resampling.LANCZOS,
            )
        return image


def _sample_layers(layers, batch, bary, valid):
    """Composite every material layer for a batch of triangles at once."""
    count = bary.shape[0]
    colour = np.zeros((count, 4), dtype=np.float32)
    for maps, map_of_triangle, uv_corners, clamp, replace in layers:
        ids = map_of_triangle[batch]
        for map_id in np.unique(ids):
            if map_id < 0 or map_id >= len(maps):
                continue
            texture = maps[map_id]
            if texture is None:
                continue
            selected = valid & (ids == map_id)
            if not selected.any():
                continue
            corners = uv_corners[batch[selected]]
            weights = bary[selected]
            uv = (weights[:, 0:1] * corners[:, 0] +
                  weights[:, 1:2] * corners[:, 1] +
                  weights[:, 2:3] * corners[:, 2])
            sampled = sample(texture, uv, clamp)
            alpha = sampled[:, 3:4]
            if replace:
                colour[selected] = sampled
            else:
                base = colour[selected]
                base[:, :3] = (base[:, :3] * (1.0 - alpha) +
                               sampled[:, :3] * alpha)
                base[:, 3:] = np.clip(base[:, 3:] + alpha, 0.0, 1.0)
                colour[selected] = base
    return colour


def _raster_batch(frame, batch, tile, geometry, shade, layers, depth_values,
                  triangles, bias):
    """Rasterize triangles that all fit inside one `tile`-sized square."""
    ax, ay, bx, by, cx, cy, area, minx, miny = geometry
    offsets = np.arange(tile, dtype=np.float32)
    grid_x = minx[batch][:, None, None] + offsets[None, None, :] + 0.5
    grid_y = miny[batch][:, None, None] + offsets[None, :, None] + 0.5

    inv_area = (1.0 / area[batch])[:, None, None]
    tax, tay = ax[batch][:, None, None], ay[batch][:, None, None]
    tbx, tby = bx[batch][:, None, None], by[batch][:, None, None]
    tcx, tcy = cx[batch][:, None, None], cy[batch][:, None, None]
    w0 = ((tbx - grid_x) * (tcy - grid_y) -
          (tcx - grid_x) * (tby - grid_y)) * inv_area
    w1 = ((tcx - grid_x) * (tay - grid_y) -
          (tax - grid_x) * (tcy - grid_y)) * inv_area
    w2 = 1.0 - w0 - w1

    columns = (minx[batch][:, None, None] +
               np.arange(tile, dtype=np.int32)[None, None, :])
    rows = (miny[batch][:, None, None] +
            np.arange(tile, dtype=np.int32)[None, :, None])
    columns = np.broadcast_to(columns, w0.shape)
    rows = np.broadcast_to(rows, w0.shape)
    inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
    inside &= (columns >= 0) & (columns < frame.width)
    inside &= (rows >= 0) & (rows < frame.height)
    if not inside.any():
        return

    corner = triangles[batch]
    z = (w0 * depth_values[corner[:, 0]][:, None, None] +
         w1 * depth_values[corner[:, 1]][:, None, None] +
         w2 * depth_values[corner[:, 2]][:, None, None]) + bias

    flat_rows = np.clip(rows, 0, frame.height - 1)
    flat_columns = np.clip(columns, 0, frame.width - 1)
    candidate = inside & (z < frame.depth[flat_rows, flat_columns])
    if not candidate.any():
        return

    triangle_index, _row_index, _column_index = np.nonzero(candidate)
    target_rows = flat_rows[candidate]
    target_columns = flat_columns[candidate]
    depths = z[candidate]

    # Triangles inside one batch can cover the same pixel, so keep only the
    # nearest candidate per pixel before any texture work is done.
    pixel = target_rows.astype(np.int64) * frame.width + target_columns
    order = np.lexsort((depths, pixel))
    sorted_pixel = pixel[order]
    leading = np.empty(sorted_pixel.shape, dtype=bool)
    leading[0] = True
    np.not_equal(sorted_pixel[1:], sorted_pixel[:-1], out=leading[1:])
    winners = order[leading]

    target_rows = target_rows[winners]
    target_columns = target_columns[winners]
    depths = depths[winners]
    owners = batch[triangle_index[winners]]
    bary = np.stack([w0[candidate][winners], w1[candidate][winners],
                     w2[candidate][winners]], axis=1)

    colour = _sample_layers(layers, owners, bary,
                            np.ones(bary.shape[0], dtype=bool))
    visible = colour[:, 3] > 0.0
    if not visible.any():
        return

    corner = triangles[owners]
    light = (bary[:, 0] * shade[corner[:, 0]] +
             bary[:, 1] * shade[corner[:, 1]] +
             bary[:, 2] * shade[corner[:, 2]])
    colour[:, :3] *= light[:, None]

    rows_keep = target_rows[visible]
    columns_keep = target_columns[visible]
    colour_keep = colour[visible]
    existing = frame.colour[rows_keep, columns_keep]
    alpha = colour_keep[:, 3:4]
    frame.colour[rows_keep, columns_keep] = np.concatenate([
        existing[:, :3] * (1.0 - alpha) + colour_keep[:, :3] * alpha,
        np.clip(existing[:, 3:] * (1.0 - alpha) + alpha, 0.0, 1.0),
    ], axis=1)
    opaque = colour_keep[:, 3] >= 0.5
    if opaque.any():
        frame.depth[rows_keep[opaque], columns_keep[opaque]] = (
            depths[visible][opaque]
        )


def raster_part(frame, screen, depth_values, triangles, shade, layers, bias):
    """Rasterize one real mesh part with its authentic material layers.

    Triangles are bucketed by bounding-box size and each bucket is rasterized
    as one batched array operation, which keeps a whole avatar frame inside a
    handful of NumPy calls instead of thousands of per-triangle passes.
    """
    x = screen[:, 0]
    y = screen[:, 1]
    ax, ay = x[triangles[:, 0]], y[triangles[:, 0]]
    bx, by = x[triangles[:, 1]], y[triangles[:, 1]]
    cx, cy = x[triangles[:, 2]], y[triangles[:, 2]]
    area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay)
    # Back faces are dropped: the real meshes are closed and consistently wound.
    visible = area < -1e-9

    minx = np.floor(np.minimum(np.minimum(ax, bx), cx)).astype(np.int32)
    maxx = np.ceil(np.maximum(np.maximum(ax, bx), cx)).astype(np.int32)
    miny = np.floor(np.minimum(np.minimum(ay, by), cy)).astype(np.int32)
    maxy = np.ceil(np.maximum(np.maximum(ay, by), cy)).astype(np.int32)
    visible &= (maxx > minx) & (maxy > miny)
    visible &= (maxx > 0) & (minx < frame.width)
    visible &= (maxy > 0) & (miny < frame.height)
    if not visible.any():
        return

    span = np.maximum(maxx - minx, maxy - miny)
    geometry = (ax, ay, bx, by, cx, cy, area, minx, miny)
    remaining = np.nonzero(visible)[0]
    tile = 2
    while remaining.size:
        batch = remaining[span[remaining] <= tile]
        remaining = remaining[span[remaining] > tile]
        if batch.size:
            _raster_batch(frame, batch, tile, geometry, shade, layers,
                          depth_values, triangles, bias)
        tile *= 2
        if tile > max(frame.width, frame.height) * 2:
            if remaining.size:
                _raster_batch(frame, remaining, tile, geometry, shade, layers,
                              depth_values, triangles, bias)
            break


# ---------------------------------------------------------------------------
# Real wardrobe assembled from the Microsoft mesh parts
# ---------------------------------------------------------------------------


BODY_PRESETS = {
    "xna-boy": {"family": "boy", "body": "boy-body", "head": "boy-head"},
    "xna-girl": {"family": "girl", "body": "girl-body", "head": "girl-head"},
    "xna-girl-heels": {"family": "girl", "body": "girl-body-heelleg",
                       "head": "girl-head"},
}

# Every entry is a real mesh from the Ms-PL pack; `None` simply omits it.
HAIR_STYLES = {
    "original": None,
    "boy-short": "boy-hair",
    "girl-bob": "girl-hair",
    "shaved": "",
}
TOP_STYLES = {
    "original": None,
    "crew-tee": "boy-top",
    "scoop-tee": "girl-top",
    "none": "",
}
BOTTOM_STYLES = {
    "original": None,
    "jeans": "boy-bottoms",
    "shorts": "girl-bottoms",
}
SHOE_STYLES = {
    "original": None,
    "sneakers": "boy-shoes",
    "flats": "girl-shoes",
    "heels": "girl-highheels",
    "barefoot": "",
}
DEFAULT_STYLES = {
    "boy": {"hair": "boy-hair", "top": "boy-top", "bottom": "boy-bottoms",
            "shoes": "boy-shoes"},
    "girl": {"hair": "girl-hair", "top": "girl-top", "bottom": "girl-bottoms",
             "shoes": "girl-shoes"},
}

# Microsoft fitted every garment to one body.  Hair sits above the shoulders
# and swaps freely, and the girl body is the narrower of the two, so it wears
# either wardrobe.  The broader boy body pushes through the girl-fitted top,
# and the heels require the girl heel-leg body, so those two stay family-bound.
STYLE_BODIES = {
    "scoop-tee": ("girl",),
    "heels": ("girl",),
}


def style_available(slot, choice, family):
    del slot
    allowed = STYLE_BODIES.get(choice)
    return allowed is None or family in allowed


def available_styles(family):
    """Return the real style choices offered for one body family."""
    tables = {"hair": HAIR_STYLES, "top": TOP_STYLES,
              "bottom": BOTTOM_STYLES, "shoes": SHOE_STYLES}
    return {
        slot: [choice for choice in table
               if style_available(slot, choice, family)]
        for slot, table in tables.items()
    }


def resolve_wardrobe(profile):
    """Map a profile onto concrete real mesh parts."""
    preset = BODY_PRESETS.get(profile.get("body") or "xna-boy",
                              BODY_PRESETS["xna-boy"])
    family = preset["family"]
    defaults = DEFAULT_STYLES[family]
    parts = {"body": preset["body"], "head": preset["head"]}
    for slot, table in (("hair", HAIR_STYLES), ("top", TOP_STYLES),
                        ("bottom", BOTTOM_STYLES), ("shoes", SHOE_STYLES)):
        choice = profile.get(f"{slot}_style") or "original"
        if choice not in table or not style_available(slot, choice, family):
            choice = "original"
        mesh = table[choice]
        if mesh is None:
            mesh = defaults[slot]
        parts[slot] = mesh or None
    if preset["body"] == "girl-body" and parts["shoes"] == "girl-highheels":
        parts["body"] = "girl-body-heelleg"
    elif preset["body"] == "girl-body-heelleg" and \
            parts["shoes"] != "girl-highheels":
        parts["body"] = "girl-body"
    return parts


# ---------------------------------------------------------------------------
# Material construction
# ---------------------------------------------------------------------------


def _saturation_mask(texture):
    """Select the coloured region of a real map (iris, lips) from its own hue."""
    maximum = texture[..., :3].max(axis=-1)
    minimum = texture[..., :3].min(axis=-1)
    chroma = maximum - minimum
    return np.clip((chroma - 0.05) / 0.20, 0.0, 1.0)[..., None]


def _tint_coloured_region(texture, target):
    """Recolour only the pixels the original artwork already colours."""
    if target is None or texture is None:
        return texture
    mask = _saturation_mask(texture)
    tinted = _tint(texture, target)
    result = texture.copy()
    result[..., :3] = texture[..., :3] * (1.0 - mask) + tinted[..., :3] * mask
    return result


def _channel_index(rig, part, name, occurrence=0):
    seen = 0
    for index, channel in enumerate(rig.part_channels(part)):
        if channel == name:
            if seen == occurrence:
                return index
            seen += 1
    return None


def _uv(rig, part, index):
    return rig.geometry[f"{part}/uv{index}"]


def _tex_ids(rig, part, index):
    return rig.geometry[f"{part}/tex{index}"]


def _bound_textures(rig, part):
    return [rig.texture(name) for name in rig.part_textures(part)]


def _clamp_flags(rig, part):
    return ["ClampUV" in name for name in rig.part_textures(part)]


def _texture_files(rig, part):
    """Resolve each bound texture to the original Microsoft file it came from.

    Several texture objects on one mesh point at the same TGA, so identity has
    to be compared by source file rather than by texture name.
    """
    textures = rig.manifest["textures"]
    return [textures.get(name, {}).get("source", name)
            for name in rig.part_textures(part)]


def _colour_channel(rig, part):
    """Pick the colour-map channel whose texture bindings actually resolve.

    The heel-leg body carries spare decal channels ahead of its colour map, so
    the first matching channel is not always the one with live bindings.
    """
    bound = len(rig.part_textures(part))
    candidates = [index for index, channel
                  in enumerate(rig.part_channels(part))
                  if channel == SKIN_CHANNEL]
    candidates += [index for index in range(len(rig.part_channels(part)))
                   if index not in candidates]
    for index in candidates:
        ids = _tex_ids(rig, part, index)
        if ((ids >= 0) & (ids < bound)).any():
            return index
    return 0


def body_layers(rig, part, skin):
    index = _colour_channel(rig, part)
    maps = [_tint(texture, skin) if texture is not None else None
            for texture in _bound_textures(rig, part)]
    ids = _tex_ids(rig, part, index).copy()
    ids[(ids < 0) | (ids >= len(maps))] = 0
    return [(maps, ids, _uv(rig, part, index), False, True)]


def head_layers(rig, part, skin, eye, brow, lip):
    """Compose the head from Microsoft's own facial texture channels."""
    base = _bound_textures(rig, part)
    names = rig.part_textures(part)
    clamps = _clamp_flags(rig, part)
    prepared = []
    for name, texture in zip(names, base):
        if texture is None:
            prepared.append(None)
        elif "SkinFeatures" in name:
            prepared.append(_tint(texture, skin))
        elif "EyeBrow" in name:
            prepared.append(_tint(texture, brow))
        elif "Eye" in name and "Shadow" not in name:
            prepared.append(_tint_coloured_region(texture, eye))
        elif "Mouth" in name:
            prepared.append(_tint_coloured_region(texture, lip))
        else:
            prepared.append(texture)

    layers = []
    for order, channel in enumerate(HEAD_LAYER_ORDER):
        for occurrence in range(2):
            index = _channel_index(rig, part, channel, occurrence)
            if index is None:
                continue
            ids = _tex_ids(rig, part, index)
            clamp = any(clamps[value] for value in np.unique(ids) if value >= 0)
            layers.append((prepared, ids, _uv(rig, part, index), clamp,
                           order == 0))
    return layers


def garment_layers(rig, part, colour, design=None):
    """The item's real colour map followed by its own Microsoft decal channel.

    A generated design replaces the flat tint: the pattern supplies the colour
    at each texel while the original map still supplies the weave and shading.
    """
    clamps = _clamp_flags(rig, part)
    base = _bound_textures(rig, part)
    colour_index = _colour_channel(rig, part)
    if design is not None:
        maps = [apply_design(texture, design) if texture is not None else None
                for texture in base]
    else:
        maps = [_tint(texture, colour) if texture is not None else None
                for texture in base]
    ids = _tex_ids(rig, part, colour_index).copy()
    ids[(ids < 0) | (ids >= len(maps))] = 0
    layers = [(maps, ids, _uv(rig, part, colour_index), False, True)]

    decal_index = None
    for candidate in ("__DecalMap", "__DecalColorMap"):
        decal_index = _channel_index(rig, part, candidate)
        if decal_index is not None:
            break
    if decal_index is None:
        return layers
    decal_ids = _tex_ids(rig, part, decal_index)
    # A decal channel bound to the item's own colour or intensity map carries
    # no artwork, so it is skipped instead of being painted over the garment.
    files = _texture_files(rig, part)
    usable = []
    for index, texture in enumerate(base):
        # A decal channel pointing back at the item's own colour or intensity
        # map carries no artwork; painting it would undo the colour choice.
        if (texture is None or files[index] == files[colour_index] or
                texture[..., 3].max() <= 0.0 or texture[..., 3].min() >= 0.99):
            usable.append(None)
        else:
            usable.append(texture)
    clamp = any(clamps[value] for value in np.unique(decal_ids) if value >= 0)
    layers.append((usable, decal_ids, _uv(rig, part, decal_index), clamp,
                   False))
    return layers


# ---------------------------------------------------------------------------
# Decals sourced from real artwork already in this tree
# ---------------------------------------------------------------------------


# Every decal is real artwork already shipped in this tree: the Rockbox
# project marks and the stock iPod resources extracted from Apple's own
# firmware.  `keyed` lifts the graphic off its flat background; the album
# artwork prints full frame the way a real tour shirt does.
DECAL_SOURCES = {
    "none": None,
    "original": None,
    # The Rockbox marks are amber badges by design, so they print full frame.
    "rockbox": ("apps/bitmaps/native/rockboxlogo.240x74x16.bmp", False),
    "rockbox-icon": ("apps/bitmaps/native/rockboxicon.130x130x16.bmp", False),
    "apple": (".rockbox/ipodjs/apple-logo-white.48x58x24.bmp", True),
    "album-art": (".rockbox/ipodjs/default_album_artwork.128x128x24.bmp", False),
    "ipod-play": (".rockbox/ipodjs/play.12x12x24.bmp", True),
    "ipod-volume": (".rockbox/ipodjs/volume_full.24x24x24.bmp", True),
}
DECAL_LABELS = {
    "none": "No print",
    "original": "Original XNA",
    "rockbox": "Rockbox logo",
    "rockbox-icon": "Rockbox icon",
    "apple": "iPod Apple mark",
    "album-art": "iPod album art print",
    "ipod-play": "iPod play glyph",
    "ipod-volume": "iPod volume glyph",
}


def load_decal_art(repo_root, name):
    """Load a real in-tree bitmap and key its flat background to transparency."""
    entry = DECAL_SOURCES.get(name)
    if not entry:
        return None
    relative, keyed = entry
    path = os.path.join(os.path.abspath(repo_root), *relative.split("/"))
    if not os.path.isfile(path):
        return None
    with Image.open(path) as source:
        image = source.convert("RGB")
    data = np.asarray(image, dtype=np.float32) / 255.0
    if not keyed:
        alpha = np.ones(data.shape[:2], dtype=np.float32)
    else:
        corners = np.stack([data[0, 0], data[0, -1], data[-1, 0], data[-1, -1]])
        background = np.median(corners, axis=0)
        distance = np.abs(data - background).sum(axis=-1)
        alpha = np.clip((distance - 0.10) / 0.30, 0.0, 1.0)
        if alpha.max() <= 0.0:
            alpha = np.ones(data.shape[:2], dtype=np.float32)
    return np.concatenate([data, alpha[..., None]], axis=-1).astype(np.float32)


def _pad_transparent(art, border=3):
    height, width = art.shape[:2]
    padded = np.zeros((height + border * 2, width + border * 2, 4),
                      dtype=np.float32)
    padded[border:border + height, border:border + width] = art
    return padded


def planar_decal_layer(rig, part, reference_pose, art, *, low=0.34, high=0.74,
                       width=0.52):
    """Print real artwork across the front of a garment in object space.

    The Microsoft garment UV islands are not contiguous over the chest, so a
    decal is projected straight onto the front of the mesh instead.  The
    projection is computed once from the standing pose, so the print stays
    attached to the fabric through every animation frame.
    """
    if art is None:
        return None
    triangles = rig.geometry[f"{part}/triangles"]
    normals = vertex_normals(reference_pose, triangles)
    facing = normals[triangles].mean(axis=1)[:, 2]

    y = reference_pose[:, 1]
    span = float(y.max() - y.min())
    if span <= 0:
        return None
    bottom = float(y.min()) + span * low
    top = float(y.min()) + span * high
    centre = float(reference_pose[:, 0].min() + reference_pose[:, 0].max()) / 2.0
    half = float(reference_pose[:, 0].max() - reference_pose[:, 0].min()) * width / 2.0
    if half <= 0 or top <= bottom:
        return None

    padded = _pad_transparent(art)
    scale_u = art.shape[1] / padded.shape[1]
    scale_v = art.shape[0] / padded.shape[0]
    offset_u = (1.0 - scale_u) / 2.0
    offset_v = (1.0 - scale_v) / 2.0

    u = (reference_pose[:, 0] - (centre - half)) / (2.0 * half)
    v = (reference_pose[:, 1] - bottom) / (top - bottom)
    uv = np.stack([offset_u + u * scale_u, offset_v + v * scale_v], axis=1)
    uv_corners = uv[triangles].astype(np.float32)

    ids = np.where(facing > 0.45, 0, -1).astype(np.int32)
    within = ((uv_corners[..., 0] > offset_u) &
              (uv_corners[..., 0] < 1.0 - offset_u) &
              (uv_corners[..., 1] > offset_v) &
              (uv_corners[..., 1] < 1.0 - offset_v)).any(axis=1)
    ids[~within] = -1
    if not (ids >= 0).any():
        return None
    return ([padded], ids, uv_corners, True, False)


# ---------------------------------------------------------------------------
# Camera and top level rendering
# ---------------------------------------------------------------------------


def yaw_matrix(degrees):
    angle = np.radians(degrees)
    cos, sin = np.cos(angle), np.sin(angle)
    return np.array([[cos, 0.0, sin], [0.0, 1.0, 0.0], [-sin, 0.0, cos]],
                    dtype=np.float32)


def _posed(rig, parts, clip, frame_index):
    positions = rig.clip_positions(clip)
    result = {}
    for slot, part in parts.items():
        if not part:
            continue
        data = positions[part]
        result[slot] = data[min(frame_index, data.shape[0] - 1)].astype(
            np.float32
        )
    return result


def _fit(all_points, size, margin_ratio=0.018):
    stacked = np.concatenate(all_points)
    low = stacked.min(axis=0)
    high = stacked.max(axis=0)
    margin = max(2.0, min(size) * margin_ratio)
    available_x = size[0] - margin * 2
    available_y = size[1] - margin * 2
    width = max(high[0] - low[0], 1e-3)
    height = max(high[1] - low[1], 1e-3)
    scale = min(available_x / width, available_y / height)
    centre_x = (low[0] + high[0]) / 2.0
    return scale, centre_x, low[1], margin


def _project(pose, scale, centre_x, floor_y, margin, render_size):
    screen = np.empty((pose.shape[0], 2), dtype=np.float32)
    screen[:, 0] = (pose[:, 0] - centre_x) * scale + render_size[0] / 2.0
    screen[:, 1] = render_size[1] - margin - (pose[:, 1] - floor_y) * scale
    return screen, (-pose[:, 2] * scale).astype(np.float32)


def garment_clipping(rig, body_part, garment_part, slot="top", size=(104, 168),
                     supersample=2):
    """Measure body pixels that win the depth test through a worn garment.

    Microsoft fitted each garment to a particular body, so a cross-body
    combination can leave shoulders or hips poking through.  Rendering the two
    parts against a shared camera and comparing depth buffers detects that
    directly instead of guessing from vertex proximity.
    """
    positions = rig.clip_positions("turntable")
    poses = {"body": positions[body_part][0].astype(np.float32),
             "garment": positions[garment_part][0].astype(np.float32)}
    render_size = (size[0] * supersample, size[1] * supersample)
    camera = _fit(list(poses.values()), render_size)
    buffers = {}
    for name, pose in poses.items():
        part = body_part if name == "body" else garment_part
        frame = Frame(*render_size)
        triangles = rig.geometry[f"{part}/triangles"]
        screen, depth = _project(pose, *camera, render_size)
        shade = np.ones(pose.shape[0], dtype=np.float32)
        layers = (body_layers(rig, part, None) if name == "body"
                  else garment_layers(rig, part, None, None))
        bias = 0.0 if name == "body" else DEPTH_BIAS.get(slot, 0.0) * camera[0]
        raster_part(frame, screen, depth, triangles, shade, layers, bias)
        buffers[name] = frame
    covered = buffers["garment"].colour[..., 3] > 0.5
    through = covered & (buffers["body"].depth <
                         buffers["garment"].depth - 1e-4)
    return int(through.sum()), int(covered.sum())


def resolve_marketplace(profile, pack):
    """Pick the imported items a profile wears, dropping anything unknown."""
    if pack is None or not pack.available():
        return {}
    chosen = {}
    for slot in MARKETPLACE_SLOTS:
        name = profile.get(slot)
        if not name or name == "none":
            continue
        record = pack.manifest["items"].get(name)
        if record and record["slot"] == MARKETPLACE_KINDS[slot]:
            chosen[slot] = name
    return chosen


def render_frames(rig, profile, clip, size, *, repo_root=None, supersample=2,
                  angles=None, frames=None, marketplace=None):
    """Render one real animation clip or turntable into RGBA frames."""
    parts = resolve_wardrobe(profile)
    if marketplace is None and repo_root is not None:
        marketplace = MarketplacePack(repo_root)
    worn = resolve_marketplace(profile, marketplace)
    # A full-body costume stands in for the ordinary wardrobe, exactly as it
    # does on a real Xbox 360 avatar.
    if "costume" in worn:
        for slot in ("top", "bottom", "shoes"):
            parts[slot] = None
    # A helmet, mask or mascot head encloses the head the way it does on a
    # real avatar, so the hair underneath is taken off rather than left to
    # poke through it.  A hat or cap is worn over the hair and keeps it.
    # A whole-head item is the head, exactly as on a real avatar, so the
    # avatar's own head and hair come off rather than showing through an
    # opening the item leaves.  Anything that only partly encloses the head
    # keeps both, since hiding the hair alone would bare the scalp.
    headwear = worn.get("headwear")
    if headwear and marketplace.manifest["items"][headwear].get(
            "replaces_head"):
        parts["head"] = None
        parts["hair"] = None
    positions = rig.clip_positions(clip)
    frame_count = frames or max(
        positions[part].shape[0] for part in parts.values() if part
    )
    orientations = list(angles) if angles is not None else [0.0] * frame_count
    if angles is not None:
        frame_count = len(orientations)

    skin = hex_rgb(profile.get("skin_colour"))
    hair_colour = hex_rgb(profile.get("hair_colour"))
    top_colour = hex_rgb(profile.get("top_colour"))
    bottom_colour = hex_rgb(profile.get("bottom_colour"))
    shoe_colour = hex_rgb(profile.get("shoes_colour"))
    eye_colour = hex_rgb(profile.get("eye_colour"))
    brow_colour = hex_rgb(profile.get("brow_colour"))
    lip_colour = hex_rgb(profile.get("lip_colour"))

    designs = {}
    for slot in ("top", "bottom", "shoes"):
        name = profile.get(f"{slot}_design") or "none"
        if name == "none":
            continue
        primary = hex_rgb(profile.get(f"{slot}_design_primary") or "#d9dcde")
        secondary = hex_rgb(profile.get(f"{slot}_design_secondary")
                            or "#303236")
        designs[slot] = generate(
            name, primary, secondary,
            seed=int(profile.get(f"{slot}_design_seed") or 0),
            scale=profile.get(f"{slot}_design_scale") or "medium",
        )

    decal_name = profile.get("decal") or "original"
    decal_art = (load_decal_art(repo_root, decal_name)
                 if repo_root and decal_name not in ("original", "none")
                 else None)

    # Pose every frame once so the clip shares one stable camera.
    posed_frames = []
    for index in range(frame_count):
        posed = {}
        for slot, part in parts.items():
            if not part:
                continue
            data = positions[part]
            pose = data[min(index, data.shape[0] - 1)].astype(np.float32)
            if orientations[index]:
                centre = np.array([0.0, 0.0, 0.0], dtype=np.float32)
                pose = (pose - centre) @ yaw_matrix(orientations[index]).T
            posed[slot] = pose
        posed_frames.append(posed)

    marketplace_frames = []
    for index in range(frame_count):
        posed = {}
        for slot, name in worn.items():
            points = pose_marketplace_item(marketplace, rig, name, clip, index)
            if orientations[index]:
                points = points @ yaw_matrix(orientations[index]).T
            posed[slot] = points.astype(np.float32)
        marketplace_frames.append(posed)

    render_size = (size[0] * supersample, size[1] * supersample)
    scale, centre_x, floor_y, margin = _fit(
        [pose for frame in posed_frames for pose in frame.values()] +
        [pose for frame in marketplace_frames for pose in frame.values()],
        render_size,
    )

    layer_cache = {}
    results = []
    for index in range(frame_count):
        posed = posed_frames[index]
        frame = Frame(*render_size)
        for slot in DRAW_ORDER:
            part = parts.get(slot)
            if not part or slot not in posed:
                continue
            pose = posed[slot]
            triangles = rig.geometry[f"{part}/triangles"]
            screen, depth = _project(pose, scale, centre_x, floor_y, margin,
                                     render_size)

            normals = vertex_normals(pose, triangles)
            key = normals @ (KEY_DIRECTION / np.linalg.norm(KEY_DIRECTION))
            fill = normals @ (FILL_DIRECTION / np.linalg.norm(FILL_DIRECTION))
            shade = (AMBIENT + KEY_STRENGTH * np.clip(key, 0, None) +
                     FILL_STRENGTH * np.clip(fill, 0, None)).astype(np.float32)

            if slot not in layer_cache:
                if slot == "head":
                    layer_cache[slot] = head_layers(
                        rig, part, skin, eye_colour, brow_colour, lip_colour,
                    )
                elif slot == "body":
                    layer_cache[slot] = body_layers(rig, part, skin)
                else:
                    colour = {"hair": hair_colour, "top": top_colour,
                              "bottom": bottom_colour,
                              "shoes": shoe_colour}[slot]
                    layers = garment_layers(rig, part, colour,
                                            designs.get(slot))
                    if decal_art is not None and slot == "top":
                        # The print is projected from the standing pose so it
                        # stays put on the fabric through every frame.
                        reference = rig.clip_positions("turntable")[part][0]
                        printed = planar_decal_layer(
                            rig, part, reference.astype(np.float32), decal_art
                        )
                        if printed is not None:
                            layers.append(printed)
                    layer_cache[slot] = layers

            raster_part(frame, screen, depth, triangles, shade,
                        layer_cache[slot], DEPTH_BIAS.get(slot, 0.0) * scale)

        for slot in MARKETPLACE_SLOTS:
            name = worn.get(slot)
            if name is None:
                continue
            pose = marketplace_frames[index][slot]
            triangles = marketplace.geometry[f"{name}/triangles"]
            screen, depth = _project(pose, scale, centre_x, floor_y, margin,
                                     render_size)
            normals = vertex_normals(pose, triangles)
            key = normals @ (KEY_DIRECTION / np.linalg.norm(KEY_DIRECTION))
            fill = normals @ (FILL_DIRECTION / np.linalg.norm(FILL_DIRECTION))
            shade = (AMBIENT + KEY_STRENGTH * np.clip(key, 0, None) +
                     FILL_STRENGTH * np.clip(fill, 0, None)).astype(np.float32)
            if slot not in layer_cache:
                layer_cache[slot] = marketplace_layers(marketplace, name)
            raster_part(frame, screen, depth, triangles, shade,
                        layer_cache[slot], -0.55 * scale)
        results.append(frame.to_image(supersample))
    return results


# ---------------------------------------------------------------------------
# Archived Xbox 360 Marketplace items
# ---------------------------------------------------------------------------


MARKETPLACE_RELATIVE = "assets/ipodjs/sources/xbox360/avatar/marketplace"
# Wardrobe slots an imported item can occupy, in draw order.
MARKETPLACE_SLOTS = ("costume", "marketplace_top", "headwear", "prop")
MARKETPLACE_KINDS = {"costume": "costume", "marketplace_top": "top",
                     "headwear": "head", "prop": "hand"}


class MarketplacePack:
    """Lazy reader for the imported Marketplace item pack."""

    def __init__(self, repo_root):
        self.root = os.path.join(os.path.abspath(repo_root),
                                 *MARKETPLACE_RELATIVE.split("/"))
        self._manifest = None
        self._geometry = None
        self._textures = {}

    def available(self):
        return (os.path.isfile(os.path.join(self.root, "marketplace.json")) and
                os.path.isfile(os.path.join(self.root, "geometry.npz")))

    @property
    def manifest(self):
        if self._manifest is None:
            path = os.path.join(self.root, "marketplace.json")
            with open(path, encoding="utf-8") as handle:
                self._manifest = json.load(handle)
        return self._manifest

    @property
    def geometry(self):
        if self._geometry is None:
            self._geometry = np.load(os.path.join(self.root, "geometry.npz"))
        return self._geometry

    def items(self, kind=None):
        entries = self.manifest["items"]
        if kind is None:
            return entries
        return {name: record for name, record in entries.items()
                if record["slot"] == kind}

    def texture(self, filename):
        if filename not in self._textures:
            if not filename:
                self._textures[filename] = None
            else:
                path = os.path.join(self.root, "textures", filename)
                with Image.open(path) as source:
                    image = source.convert("RGBA")
                self._textures[filename] = (
                    np.asarray(image, dtype=np.float32) / 255.0
                )
        return self._textures[filename]


def pose_marketplace_item(pack, rig, name, clip, frame_index):
    """Return the world-space vertices of one imported item for a frame."""
    record = pack.manifest["items"][name]
    positions = rig.clip_positions(clip)
    if record["attach"] == "skin":
        bind = pack.geometry[f"{name}/bind"]
        slots = pack.geometry[f"{name}/skin_bones"]
        weights = pack.geometry[f"{name}/skin_weights"]
        matrices = positions["__skin__"]
        matrices = matrices[min(frame_index, matrices.shape[0] - 1)]
        homogeneous = np.concatenate(
            [bind, np.ones((bind.shape[0], 1), np.float32)], axis=1
        )
        result = np.zeros((bind.shape[0], 3), dtype=np.float32)
        for column in range(slots.shape[1]):
            chosen = matrices[slots[:, column]]
            result += np.einsum(
                "vij,vj->vi", chosen[:, :3, :], homogeneous
            ) * weights[:, column][:, None]
        return result
    order = rig.manifest["attachment_bones"]
    bones = positions["__bones__"]
    bones = bones[min(frame_index, bones.shape[0] - 1)]
    matrix = bones[order.index(record["attach"])]
    local = pack.geometry[f"{name}/local"]
    homogeneous = np.concatenate(
        [local, np.ones((local.shape[0], 1), np.float32)], axis=1
    )
    return (homogeneous @ matrix.T)[:, :3].astype(np.float32)


def marketplace_layers(pack, name, colour=None):
    """Material layers for an imported item, using its original textures."""
    record = pack.manifest["items"][name]
    maps = [pack.texture(filename) for filename in record["textures"]]
    if colour is not None:
        maps = [_tint(texture, colour) if texture is not None else None
                for texture in maps]
    ids = pack.geometry[f"{name}/texture"]
    uv = pack.geometry[f"{name}/uv"]
    return [(maps, ids, uv, True, True)]
