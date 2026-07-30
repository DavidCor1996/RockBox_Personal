"""Procedurally generated garment designs for the Xbox 360 avatar creator.

These patterns are computed from parameters, not drawn: each generator is a
closed-form function of the pixel grid, so a design is fully described by its
name, two colours, and a seed.  They are applied by multiplying the real
Microsoft colour map's own shading into the pattern, so the garment keeps its
original weave, seams, and lighting while taking on a new print.

Character art still comes only from the real XNA rig; this module never touches
skin, faces, or geometry.
"""

from __future__ import annotations

import numpy as np


TILE = 256


def _grid(size=TILE):
    axis = np.linspace(0.0, 1.0, size, endpoint=False, dtype=np.float32)
    return np.meshgrid(axis, axis)


def _mix(mask, primary, secondary):
    mask = np.clip(mask, 0.0, 1.0)[..., None].astype(np.float32)
    return secondary * (1.0 - mask) + primary * mask


def _rng(seed):
    return np.random.default_rng(int(seed) & 0xFFFFFFFF)


def solid(primary, secondary, seed, repeat):
    del secondary, seed, repeat
    return np.broadcast_to(primary, (TILE, TILE, 3)).copy()


def stripes(primary, secondary, seed, repeat):
    del seed
    _x, y = _grid()
    return _mix((np.sin(y * repeat * 2.0 * np.pi) > 0).astype(np.float32),
                primary, secondary)


def pinstripe(primary, secondary, seed, repeat):
    del seed
    x, _y = _grid()
    phase = np.mod(x * repeat * 2.0, 1.0)
    return _mix((phase < 0.18).astype(np.float32), primary, secondary)


def diagonal(primary, secondary, seed, repeat):
    del seed
    x, y = _grid()
    return _mix((np.mod((x + y) * repeat, 1.0) < 0.5).astype(np.float32),
                primary, secondary)


def chevron(primary, secondary, seed, repeat):
    del seed
    x, y = _grid()
    wave = np.abs(np.mod(x * repeat, 1.0) - 0.5) * 2.0
    return _mix((np.mod(y * repeat + wave, 1.0) < 0.5).astype(np.float32),
                primary, secondary)


def checks(primary, secondary, seed, repeat):
    del seed
    x, y = _grid()
    cell = (np.floor(x * repeat) + np.floor(y * repeat)) % 2.0
    return _mix(cell, primary, secondary)


def plaid(primary, secondary, seed, repeat):
    del seed
    x, y = _grid()
    warp = (np.mod(x * repeat, 1.0) < 0.34).astype(np.float32)
    weft = (np.mod(y * repeat, 1.0) < 0.34).astype(np.float32)
    return _mix(np.clip(warp * 0.65 + weft * 0.65, 0.0, 1.0),
                primary, secondary)


def tartan(primary, secondary, seed, repeat):
    del seed
    x, y = _grid()
    def band(value):
        phase = np.mod(value * repeat, 1.0)
        return ((phase < 0.10).astype(np.float32) * 1.0 +
                ((phase > 0.42) & (phase < 0.52)).astype(np.float32) * 0.55)
    return _mix(np.clip(band(x) + band(y), 0.0, 1.0), primary, secondary)


def dots(primary, secondary, seed, repeat):
    del seed
    x, y = _grid()
    fx = np.mod(x * repeat, 1.0) - 0.5
    fy = np.mod(y * repeat, 1.0) - 0.5
    radius = np.sqrt(fx * fx + fy * fy)
    return _mix(np.clip((0.30 - radius) * TILE / repeat, 0.0, 1.0),
                primary, secondary)


def halftone(primary, secondary, seed, repeat):
    del seed
    x, y = _grid()
    fx = np.mod(x * repeat, 1.0) - 0.5
    fy = np.mod(y * repeat, 1.0) - 0.5
    radius = np.sqrt(fx * fx + fy * fy)
    size = 0.10 + 0.34 * y
    return _mix(np.clip((size - radius) * TILE / repeat, 0.0, 1.0),
                primary, secondary)


def rings(primary, secondary, seed, repeat):
    del seed
    x, y = _grid()
    radius = np.sqrt((x - 0.5) ** 2 + (y - 0.5) ** 2)
    return _mix((np.mod(radius * repeat * 2.0, 1.0) < 0.5).astype(np.float32),
                primary, secondary)


def rays(primary, secondary, seed, repeat):
    del seed
    x, y = _grid()
    angle = np.arctan2(y - 0.5, x - 0.5) / (2.0 * np.pi) + 0.5
    return _mix((np.mod(angle * max(3.0, repeat * 2.0), 1.0) < 0.5
                 ).astype(np.float32), primary, secondary)


def waves(primary, secondary, seed, repeat):
    del seed
    x, y = _grid()
    offset = np.sin(x * repeat * 2.0 * np.pi) * 0.12
    return _mix((np.mod((y + offset) * repeat, 1.0) < 0.5).astype(np.float32),
                primary, secondary)


def argyle(primary, secondary, seed, repeat):
    del seed
    x, y = _grid()
    fx = np.abs(np.mod(x * repeat, 1.0) - 0.5)
    fy = np.abs(np.mod(y * repeat, 1.0) - 0.5)
    diamond = (fx + fy < 0.42).astype(np.float32)
    lattice = (np.abs(fx - fy) < 0.04).astype(np.float32) * 0.7
    return _mix(np.clip(diamond + lattice, 0.0, 1.0), primary, secondary)


def houndstooth(primary, secondary, seed, repeat):
    del seed
    x, y = _grid()
    fx = np.mod(x * repeat, 1.0)
    fy = np.mod(y * repeat, 1.0)
    mask = ((fx < 0.5) ^ (fy < 0.5)).astype(np.float32)
    tooth = ((np.abs(fx - fy) < 0.25) | (np.abs(fx + fy - 1.0) < 0.25)
             ).astype(np.float32)
    return _mix(np.clip(mask + tooth * 0.5, 0.0, 1.0), primary, secondary)


def camo(primary, secondary, seed, repeat):
    generator = _rng(seed)
    x, y = _grid()
    field = np.zeros_like(x)
    for _ in range(6):
        fx, fy = generator.uniform(1.0, max(2.0, repeat)), generator.uniform(
            1.0, max(2.0, repeat))
        px, py = generator.uniform(0.0, 1.0), generator.uniform(0.0, 1.0)
        field += np.sin((x + px) * fx * 2.0 * np.pi) * np.cos(
            (y + py) * fy * 2.0 * np.pi)
    return _mix((field > 0.2).astype(np.float32), primary, secondary)


def marble(primary, secondary, seed, repeat):
    generator = _rng(seed)
    x, y = _grid()
    field = np.sin((x + generator.uniform(0, 1)) * repeat * np.pi)
    field += np.sin((y + generator.uniform(0, 1)) * repeat * np.pi * 0.7)
    field += np.sin((x + y) * repeat * np.pi * 1.3)
    return _mix((np.mod(field, 1.0) < 0.5).astype(np.float32),
                primary, secondary)


def fade(primary, secondary, seed, repeat):
    del seed, repeat
    _x, y = _grid()
    return _mix(1.0 - y, primary, secondary)


def grid_lines(primary, secondary, seed, repeat):
    del seed
    x, y = _grid()
    line = ((np.mod(x * repeat, 1.0) < 0.08) |
            (np.mod(y * repeat, 1.0) < 0.08)).astype(np.float32)
    return _mix(line, primary, secondary)


def triangles(primary, secondary, seed, repeat):
    del seed
    x, y = _grid()
    fx = np.mod(x * repeat, 1.0)
    fy = np.mod(y * repeat, 1.0)
    return _mix((fy < fx).astype(np.float32), primary, secondary)


def static_noise(primary, secondary, seed, repeat):
    generator = _rng(seed)
    blocks = max(4, int(repeat * 8))
    field = generator.random((blocks, blocks)).astype(np.float32)
    field = np.repeat(np.repeat(field, TILE // blocks + 1, 0),
                      TILE // blocks + 1, 1)[:TILE, :TILE]
    return _mix((field > 0.5).astype(np.float32), primary, secondary)


DESIGNS = {
    "solid": ("Solid", solid),
    "stripes": ("Stripes", stripes),
    "pinstripe": ("Pinstripe", pinstripe),
    "diagonal": ("Diagonal", diagonal),
    "chevron": ("Chevron", chevron),
    "checks": ("Checks", checks),
    "plaid": ("Plaid", plaid),
    "tartan": ("Tartan", tartan),
    "dots": ("Polka dots", dots),
    "halftone": ("Halftone", halftone),
    "rings": ("Rings", rings),
    "rays": ("Sunburst", rays),
    "waves": ("Waves", waves),
    "argyle": ("Argyle", argyle),
    "houndstooth": ("Houndstooth", houndstooth),
    "camo": ("Camo", camo),
    "marble": ("Marble", marble),
    "fade": ("Fade", fade),
    "grid": ("Grid", grid_lines),
    "triangles": ("Triangles", triangles),
    "static": ("Static", static_noise),
}
DESIGN_LABELS = {key: label for key, (label, _fn) in DESIGNS.items()}
DESIGN_CHOICES = ("none", *DESIGNS)

# Repeat counts offered in the creator, smallest to largest.
SCALES = {"fine": 12.0, "small": 8.0, "medium": 5.0, "large": 3.0,
          "huge": 2.0}
DEFAULT_SCALE = "medium"


def generate(name, primary, secondary, seed=0, scale=DEFAULT_SCALE):
    """Return one RGB design tile in 0..1, or None when no design is set."""
    entry = DESIGNS.get(name)
    if entry is None:
        return None
    repeat = SCALES.get(scale, SCALES[DEFAULT_SCALE])
    primary = np.asarray(primary, dtype=np.float32)
    secondary = np.asarray(secondary, dtype=np.float32)
    tile = entry[1](primary, secondary, seed, repeat)
    return np.clip(tile, 0.0, 1.0).astype(np.float32)


def apply_design(texture, tile):
    """Print a design onto a real colour map, keeping its own shading.

    The Microsoft map supplies the weave and lighting; the tile supplies the
    colour at each texel, so a patterned garment still reads as fabric.
    """
    if tile is None or texture is None:
        return texture
    from PIL import Image

    height, width = tile.shape[:2]
    if texture.shape[:2] != (height, width):
        image = Image.fromarray(
            (np.clip(texture, 0, 1) * 255).astype(np.uint8), "RGBA"
        ).resize((width, height), Image.Resampling.LANCZOS)
        texture = np.asarray(image, dtype=np.float32) / 255.0
    luma = (texture[..., :3] * np.array([0.2126, 0.7152, 0.0722],
                                        dtype=np.float32)).sum(-1)
    lit = luma > 0.02
    reference = max(float(np.median(luma[lit])) if lit.any() else 0.5, 1e-3)
    scale = np.clip(luma / reference, 0.35, 1.7)[..., None]
    return np.concatenate(
        [np.clip(tile * scale, 0.0, 1.0), texture[..., 3:]], axis=-1
    ).astype(np.float32)
