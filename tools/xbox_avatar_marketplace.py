#!/usr/bin/env python3
"""Fit archived Xbox 360 Marketplace items onto Microsoft's real XNA rig.

The Models Resource preserves real extracted Marketplace avatar items as OBJ
meshes with their original textures.  They carry no skeleton, so this tool
attaches them to the genuine rig in one of two deterministic ways:

* head and hand items are rigid, so they are expressed once in the relevant
  bone's space and then follow that bone exactly;
* worn body items borrow the real skin - each imported vertex copies the bone
  weights of the nearest avatar vertex in the standing pose, and is then
  inverse-skinned back to the bind pose so it animates with Microsoft's own
  deformation.

Nothing is redrawn or reshaped.  Items that cannot be placed honestly (pets,
vehicles, scenery) are recorded as unsupported instead of being guessed at.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "rockpod"))

from services.xbox_avatar_render import AvatarRig  # noqa: E402


DEFAULT_CACHE = ROOT / ".cache/xbox360-marketplace"
PERCHED = False
DEFAULT_OUTPUT = ROOT / "assets/ipodjs/sources/xbox360/avatar/marketplace"

# Where the real avatar's head and feet sit in the XNA bind pose.
XNA_TOP = 149.7
XNA_FLOOR = 0.0

# Slot classification.  The archive titles state plainly what each item is, so
# these are the wardrobe categories rather than guesses about the geometry.
# Anything that is not a wearable stays unsupported.
HEAD_WORDS = ("hat", "helmet", "mask", "hood", "cap", "ears", "head", "crown",
              "face")
TOP_WORDS = ("shirt", "hoodie", "jersey", "tee")
COSTUME_WORDS = ("costume", "suit", "outfit", "uniform", "armor", "armour",
                 "spacesuit")
HAND_WORDS = ("sword", "buster", "lightsaber", "stick", "pickaxe", "cannon",
              "guitar", "hammer", "shield", "vuvuzela", "emeralds",
              "lightsaber")

# Titles the keyword pass gets wrong in either direction.
OVERRIDES = {
    "Riley in Battle Gear": "unsupported",
    "Riley in Mocap Gear": "unsupported",
    "Hulk's Super Strength": "unsupported",
    "Battlefield Cross": "unsupported",
    "Half-Pipe Special Stage": "unsupported",
    "Jurassic Park Entrance": "unsupported",
    "Shark Cage": "unsupported",
    "Snoopy's Doghouse": "unsupported",
    "Charlie Brown's Christmas Tree": "unsupported",
    "Beavis & Butthead TV": "unsupported",
    "Sonic Hoodie": "costume",
    "LEGO Construction Worker": "costume",
    "LEGO Star Wars Jyn": "costume",
    "MTV Awards Moonman": "costume",
    "Greenman Suit": "costume",
    "2013 E3 Master Chief Armor": "costume",
    "Ezio Outfit": "costume",
    "LEGO Hufflepuff Uniform": "costume",
    "Air Guitar": "hand",
    "Hockey Stick": "hand",
    "LEGO Harry Potter Broomstick": "hand",
    "Mega Man's Mega Buster": "hand",
    "Energy Sword": "hand",
    "Yoda Green Lightsaber": "hand",
    "LEGO Sword and Shield Prop": "hand",
    "LEGO Minecraft Pickaxe": "hand",
    "LEGO Cannon Prop": "hand",
    "Fiat Vuvuzela": "hand",
    "Chaos Emeralds": "hand",
    "NXE Trophy": "hand",
}


def classify(title):
    """Return the wardrobe slot for one archived item title."""
    if title in OVERRIDES:
        return OVERRIDES[title]
    text = title.lower()
    for words, slot in ((HEAD_WORDS, "head"), (TOP_WORDS, "top"),
                        (COSTUME_WORDS, "costume"), (HAND_WORDS, "hand")):
        if any(word in text for word in words):
            return slot
    return "unsupported"


# ---------------------------------------------------------------------------
# Wavefront OBJ
# ---------------------------------------------------------------------------


def read_obj(text):
    """Parse positions, texture coordinates, and triangulated faces."""
    positions, coords, faces, materials = [], [], [], []
    current = None
    for line in text.splitlines():
        if line.startswith("v "):
            positions.append([float(v) for v in line.split()[1:4]])
        elif line.startswith("vt "):
            values = line.split()[1:3]
            coords.append([float(values[0]),
                           float(values[1]) if len(values) > 1 else 0.0])
        elif line.startswith("usemtl "):
            current = line.split(None, 1)[1].strip()
        elif line.startswith("f "):
            corners = []
            for token in line.split()[1:]:
                parts = token.split("/")
                vertex = int(parts[0]) - 1
                texture = (int(parts[1]) - 1
                           if len(parts) > 1 and parts[1] else -1)
                corners.append((vertex, texture))
            for offset in range(1, len(corners) - 1):
                faces.append((corners[0], corners[offset], corners[offset + 1]))
                materials.append(current)
    return (np.asarray(positions, dtype=np.float64),
            np.asarray(coords, dtype=np.float32) if coords else np.zeros((0, 2), np.float32),
            faces, materials)


def read_mtl(text):
    """Map each material name to its diffuse texture filename."""
    result = {}
    current = None
    for raw in text.splitlines():
        # Material bodies are indented, so tokens are matched after stripping.
        line = raw.strip()
        if line.startswith("newmtl "):
            current = line.split(None, 1)[1].strip()
        elif line.lower().startswith("map_kd") and current:
            result[current] = line.split(None, 1)[1].strip().replace("\\", "/")
    return result


def read_members(path):
    """Return {name: bytes} for an archive, falling back to 7z when needed.

    A couple of the archived items use deflate64, which the standard library
    cannot decompress.
    """
    try:
        with zipfile.ZipFile(path) as archive:
            return {name: archive.read(name)
                    for name in archive.namelist()
                    if not name.endswith("/")}
    except (NotImplementedError, zipfile.BadZipFile):
        pass
    tool = shutil.which("7z") or shutil.which("7za") or shutil.which("bsdtar")
    if tool is None:
        raise ValueError("archive needs an external extractor")
    with tempfile.TemporaryDirectory(prefix="marketplace-") as temp:
        if tool.endswith("bsdtar"):
            command = [tool, "-xf", str(path), "-C", temp]
        else:
            command = [tool, "x", "-y", f"-o{temp}", str(path)]
        result = subprocess.run(command, capture_output=True, check=False)
        if result.returncode != 0:
            raise ValueError("external extraction failed")
        members = {}
        base = Path(temp)
        for item in base.rglob("*"):
            if item.is_file():
                members[str(item.relative_to(base))] = item.read_bytes()
        return members


def load_archive(path):
    """Return (positions, uvs, faces, materials, {material: image}) for a zip."""
    import io

    members = read_members(path)
    if True:
        names = list(members)
        obj_names = [n for n in names if n.lower().endswith(".obj")]
        if not obj_names:
            raise ValueError("archive has no OBJ mesh")
        obj_name = obj_names[0]
        positions, coords, faces, materials = read_obj(
            members[obj_name].decode("utf-8", "replace")
        )
        mtl_names = [n for n in names if n.lower().endswith(".mtl")]
        mapping = {}
        if mtl_names:
            mapping = read_mtl(
                members[mtl_names[0]].decode("utf-8", "replace")
            )
        images = {}
        lookup = {n.replace("\\", "/").rsplit("/", 1)[-1].lower(): n
                  for n in names}
        for material, filename in mapping.items():
            entry = lookup.get(filename.rsplit("/", 1)[-1].lower())
            if entry is None:
                continue
            with Image.open(io.BytesIO(members[entry])) as image:
                images[material] = image.convert("RGBA").copy()
        if not images:
            for name in names:
                if name.lower().endswith((".png", ".tga", ".bmp")) and \
                        "image" not in name.lower():
                    with Image.open(io.BytesIO(members[name])) as image:
                        images[None] = image.convert("RGBA").copy()
                    break
    return positions, coords, faces, materials, images


# ---------------------------------------------------------------------------
# Fitting
# ---------------------------------------------------------------------------


def rig_bounds(rig):
    """Bounding boxes of the real avatar parts in Microsoft's bind pose.

    The Marketplace captures are all taken from the same T-pose the XNA rig
    binds in, so the bind pose is the right frame to fit them against.
    """
    geometry = rig.geometry
    body = np.concatenate([geometry["boy-body/bind"], geometry["boy-head/bind"]])
    # The head mesh alone, without the hair, is what headwear is fitted to.
    head = geometry["boy-head/bind"]
    return {
        "body": (body.min(axis=0), body.max(axis=0)),
        "head": (head.min(axis=0), head.max(axis=0)),
        "top": (geometry["boy-top/bind"].min(axis=0),
                geometry["boy-top/bind"].max(axis=0)),
    }


def head_placement(positions):
    """Say whether a head capture still carries its real avatar placement.

    Ninja Ripper dumps the vertex buffer as it was submitted, so some captures
    keep the item sitting at head height above the avatar's own origin while
    others were exported as a standalone model resting on the ground.  The
    first kind can be reproduced exactly; the second has to be fitted.
    """
    low = positions.min(axis=0)
    high = positions.max(axis=0)
    if high[1] <= 0:
        return "fitted"
    return "exact" if (low[1] / high[1]) > 0.4 else "fitted"


def _align_head(positions, low, high, height, target):
    target_low, target_high = target
    if head_placement(positions) == "exact":
        # The capture is still in avatar space and already shares this rig's
        # lateral origin, so the real placement is one uniform scale away.
        # Translating it would throw away the placement being reproduced.
        return positions * (float(target_high[1]) / float(high[1]))

    head_height = float(target_high[1] - target_low[1])
    centre = (target_low + target_high) / 2.0
    width = float(target_high[0] - target_low[0])
    span = float(high[0] - low[0])
    if PERCHED and span > 0:
        # A hat rides on the upper skull rather than enclosing the head, so it
        # is sized across the brim and sat on the crown.
        placed = positions * (width * 1.05 / span)
        placed[:, 1] += (float(target_low[1]) + head_height * 0.62 -
                         placed[:, 1].min())
    else:
        # Helmets, masks and mascot heads enclose the head, so they are sized
        # to it and made concentric with it.  The small margin keeps the skull
        # inside the shell instead of letting it show through the back.
        placed = positions * (head_height * 1.10 / height)
        placed[:, 1] += centre[1] - (
            placed[:, 1].min() + placed[:, 1].max()) / 2.0
    for axis in (0, 2):
        placed[:, axis] += centre[axis] - (
            placed[:, axis].min() + placed[:, axis].max()) / 2.0
    return placed


def align(positions, slot, bounds):
    """Place a capture into the real bind pose.

    Two capture conventions appear in the archive: most items are recorded in
    avatar space with the feet at the origin, while some head items sit on
    their own origin.  The vertical span relative to the height tells them
    apart, so each is anchored the way its own convention requires.
    """
    low = positions.min(axis=0)
    high = positions.max(axis=0)
    height = float(high[1] - low[1])
    if height <= 0:
        raise ValueError("capture has no height")

    if slot == "costume":
        target_low, target_high = bounds["body"]
        scale = float(target_high[1] - target_low[1]) / height
        placed = positions * scale
        placed[:, 1] += float(target_low[1]) - placed[:, 1].min()
        centre = (target_low + target_high) / 2.0
        for axis in (0, 2):
            placed[:, axis] += centre[axis] - (
                placed[:, axis].min() + placed[:, axis].max()) / 2.0
        return placed

    if slot == "top":
        target_low, target_high = bounds["top"]
        # Tops are recorded in avatar space, so the shoulder line fixes scale.
        scale = float(target_high[1]) / float(high[1])
        placed = positions * scale
        centre = (target_low + target_high) / 2.0
        for axis in (0, 2):
            placed[:, axis] += centre[axis] - (
                placed[:, axis].min() + placed[:, axis].max()) / 2.0
        return placed

    if slot == "head":
        return _align_head(positions, low, high, height, bounds["head"])

    # Held props have no matching avatar part, so they are sized against the
    # avatar and hung from the grip.  The Xbox rig carries a dedicated prop
    # bone in the space just below the hand, and a prop hangs from it, so the
    # top of the item is placed at the origin and its body falls below.
    target_low, target_high = bounds["body"]
    extent = float(np.abs(high - low).max())
    if extent <= 0:
        raise ValueError("degenerate prop")
    scale = float(target_high[1] - target_low[1]) * 0.55 / extent
    placed = positions * scale
    # The wrist bone's local +X runs back up the arm, so a prop that hangs
    # from the hand extends along -X.  Turning the model's own up axis onto
    # +X makes it hang correctly in every pose the bone takes.
    placed = placed @ _rotation((0.0, 0.0, -90.0)).T
    grip = np.array([
        placed[:, 0].max(),
        (placed[:, 1].min() + placed[:, 1].max()) / 2.0,
        (placed[:, 2].min() + placed[:, 2].max()) / 2.0,
    ])
    return placed - grip


def transfer_skin(rig, points, source_parts):
    """Copy bone weights from the nearest real avatar vertex in the bind pose."""
    reference, bones, weights = [], [], []
    for part in source_parts:
        reference.append(rig.geometry[f"{part}/bind"])
        bones.append(rig.geometry[f"{part}/skin_bones"])
        weights.append(rig.geometry[f"{part}/skin_weights"])
    reference = np.concatenate(reference)
    bones = np.concatenate(bones)
    weights = np.concatenate(weights)

    neighbours = min(4, reference.shape[0])
    slots = np.zeros((points.shape[0], bones.shape[1]), dtype=np.int32)
    amounts = np.zeros((points.shape[0], bones.shape[1]), dtype=np.float32)
    block = 512
    for start in range(0, points.shape[0], block):
        chunk = points[start:start + block]
        distance = ((chunk[:, None, :] - reference[None, :, :]) ** 2).sum(-1)
        order = np.argpartition(distance, neighbours - 1, axis=1)
        order = order[:, :neighbours]
        picked = np.take_along_axis(distance, order, axis=1)
        share = 1.0 / np.maximum(picked, 1e-6)
        share /= share.sum(axis=1, keepdims=True)
        for row in range(chunk.shape[0]):
            tally = {}
            for neighbour, portion in zip(order[row], share[row]):
                for column in range(bones.shape[1]):
                    weight = weights[neighbour, column] * portion
                    if weight <= 0.0:
                        continue
                    bone = int(bones[neighbour, column])
                    tally[bone] = tally.get(bone, 0.0) + weight
            ranked = sorted(tally.items(), key=lambda item: item[1],
                            reverse=True)[:bones.shape[1]]
            total = sum(weight for _bone, weight in ranked) or 1.0
            for column, (bone, weight) in enumerate(ranked):
                slots[start + row, column] = bone
                amounts[start + row, column] = weight / total
    unweighted = amounts.sum(axis=1) <= 0.0
    amounts[unweighted, 0] = 1.0
    return slots, amounts


def to_bone_space(points, bind_global):
    inverse = np.linalg.inv(bind_global)
    homogeneous = np.concatenate(
        [points, np.ones((points.shape[0], 1))], axis=1
    )
    return (homogeneous @ inverse.T)[:, :3]


# ---------------------------------------------------------------------------
# Import
# ---------------------------------------------------------------------------


BONE_FOR_SLOT = {"head": "head", "hand": "hand-right"}
# Above this share of the head hidden from every side, the item is a whole
# head rather than something worn on one, so the avatar's own head comes off.
HEAD_REPLACEMENT_COVERAGE = 0.90


def _silhouette(points, triangles, angle, bounds, size=96):
    """Rasterize one mesh's outline as seen from `angle` degrees of yaw."""
    from services.xbox_avatar_render import Frame, raster_part

    radians = np.radians(angle)
    cos, sin = np.cos(radians), np.sin(radians)
    rotation = np.array([[cos, 0.0, sin], [0.0, 1.0, 0.0], [-sin, 0.0, cos]])
    view = points @ rotation.T

    low, high = bounds
    centre = (low + high) / 2.0
    span = float(max(high[0] - low[0], high[1] - low[1])) * 1.6
    if span <= 0:
        return np.zeros((size, size), dtype=bool)
    scale = size / span
    screen = np.empty((view.shape[0], 2), dtype=np.float32)
    screen[:, 0] = (view[:, 0] - centre[0]) * scale + size / 2.0
    screen[:, 1] = size / 2.0 - (view[:, 1] - centre[1]) * scale
    depth = (-view[:, 2] * scale).astype(np.float32)

    frame = Frame(size, size)
    white = np.ones((1, 1, 4), dtype=np.float32)
    layers = [([white], np.zeros(len(triangles), dtype=np.int32),
               np.zeros((len(triangles), 3, 2), dtype=np.float32), True, True)]
    shade = np.ones(view.shape[0], dtype=np.float32)
    raster_part(frame, screen, depth, triangles.astype(np.int32), shade,
                layers, 0.0)
    return frame.colour[..., 3] > 0.5


def head_coverage(rig, points, triangles):
    """Fraction of the avatar's head this item hides, seen from all sides."""
    head_points = rig.geometry["boy-head/bind"]
    head_triangles = rig.geometry["boy-head/triangles"]
    bounds = (np.minimum(head_points.min(axis=0), points.min(axis=0)),
              np.maximum(head_points.max(axis=0), points.max(axis=0)))
    hidden = 0
    total = 0
    for angle in (0.0, 90.0, 180.0, 270.0):
        head = _silhouette(head_points, head_triangles, angle, bounds)
        item = _silhouette(points, triangles, angle, bounds)
        total += int(head.sum())
        hidden += int((head & item).sum())
    return hidden / total if total else 0.0
SOURCE_PARTS = {
    "costume": ("boy-body", "boy-head"),
    "top": ("boy-body",),
}


def slug(title):
    text = re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-")
    return text or "item"


PERCH_WORDS = ("hat", "cap", "ballcap")

# Hand-checked corrections for items whose capture carries no placement, so the
# automatic fit needs nudging.  Offsets are in XNA bind units (the avatar is
# about 150 tall); rotation is degrees about the item's own centre.
CORRECTIONS = {
    "metal-sonic-helmet": {"scale": 1.06},
    "spongebob-employee-hat": {"scale": 0.92, "offset": (0.0, -4.0, 0.0)},
    "hockey-stick": {"scale": 1.10, "offset": (0.0, 0.0, 3.0)},
    "air-guitar": {"scale": 1.00, "offset": (0.0, 0.0, 4.0)},
}


def _rotation(degrees):
    rx, ry, rz = np.radians(degrees)
    cx, sx = np.cos(rx), np.sin(rx)
    cy, sy = np.cos(ry), np.sin(ry)
    cz, sz = np.cos(rz), np.sin(rz)
    mx = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]])
    my = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
    mz = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
    return mz @ my @ mx


def apply_correction(positions, name):
    """Apply a hand-checked nudge to a fitted item."""
    correction = CORRECTIONS.get(name)
    if not correction:
        return positions, False
    centre = (positions.min(axis=0) + positions.max(axis=0)) / 2.0
    placed = positions - centre
    rotate = correction.get("rotate")
    if rotate:
        placed = placed @ _rotation(rotate).T
    placed *= correction.get("scale", 1.0)
    placed += centre + np.asarray(correction.get("offset", (0.0, 0.0, 0.0)))
    return placed, True


def import_item(rig, archive, title, slot, bounds, name):
    global PERCHED
    text = title.lower()
    PERCHED = any(word in text for word in PERCH_WORDS) and not any(
        word in text for word in ("helmet", "mask", "head", "hood")
    )
    positions, coords, faces, materials, images = load_archive(archive)
    if positions.size == 0 or not faces:
        raise ValueError("empty mesh")
    raw_positions = positions
    positions = align(positions, slot, bounds)
    positions, corrected = apply_correction(positions, name)

    triangles = np.asarray(
        [[c[0] for c in face] for face in faces], dtype=np.int32
    )
    if coords.shape[0]:
        uv = np.asarray(
            [[coords[c[1]] if c[1] >= 0 else (0.0, 0.0) for c in face]
             for face in faces], dtype=np.float32
        )
    else:
        uv = np.zeros((len(faces), 3, 2), dtype=np.float32)

    names = sorted({m for m in materials if m is not None})
    if not names:
        names = [None]
    index_of = {name: index for index, name in enumerate(names)}
    texture_of_triangle = np.asarray(
        [index_of.get(m, 0) for m in materials], dtype=np.int32
    )

    placement = (head_placement(raw_positions) if slot == "head" else "fitted")
    if corrected:
        placement = "corrected"
    covers_head = slot == "head" and not PERCHED
    replaces_head = False
    if covers_head:
        replaces_head = (head_coverage(rig, positions, triangles) >=
                         HEAD_REPLACEMENT_COVERAGE)
    record = {"slot": slot, "materials": [n or "default" for n in names],
              "placement": placement,
              "covers_head": covers_head,
              "replaces_head": replaces_head}
    if slot in BONE_FOR_SLOT:
        bone = BONE_FOR_SLOT[slot]
        order = rig.manifest["attachment_bones"]
        bind_global = rig.geometry["__bind_bones__"][order.index(bone)]
        if slot == "hand":
            # `align` centres a prop on the origin; put it in the hand first so
            # the bone carries it rather than leaving it between the feet.
            positions = positions + bind_global[:3, 3]
        local = to_bone_space(positions, bind_global)
        record["attach"] = bone
        payload = {"local": local.astype(np.float32)}
    else:
        slots, amounts = transfer_skin(rig, positions, SOURCE_PARTS[slot])
        record["attach"] = "skin"
        payload = {
            "bind": positions.astype(np.float32),
            "skin_bones": slots.astype(np.int32),
            "skin_weights": amounts.astype(np.float32),
        }

    payload["triangles"] = triangles
    payload["uv"] = uv
    payload["texture"] = texture_of_triangle
    record["vertices"] = int(positions.shape[0])
    record["triangles"] = int(triangles.shape[0])
    return record, payload, [images.get(n) for n in names]


def build(cache, output):
    cache = Path(cache)
    output = Path(output)
    index = json.loads((cache / "index.json").read_text(encoding="utf-8"))
    rig = AvatarRig(ROOT)
    if not rig.available():
        raise SystemExit("run tools/xbox_avatar_rigpack.py first")

    bounds = rig_bounds(rig)
    (output / "textures").mkdir(parents=True, exist_ok=True)
    geometry = {}
    manifest = {
        "schema": 1,
        "provenance": "archive-extracted",
        "source": index["source"],
        "note": index["note"],
        "hand_drawn_or_generated_geometry": False,
        "items": {},
        "unsupported": {},
    }

    for identifier, entry in sorted(index["assets"].items()):
        title = entry["title"]
        slot = classify(title)
        if slot == "unsupported":
            manifest["unsupported"][identifier] = {
                "title": title,
                "reason": "not a wearable avatar item",
            }
            continue
        name = slug(title)
        try:
            record, payload, images = import_item(
                rig, cache / entry["file"], title, slot, bounds, name
            )
        except (ValueError, KeyError, IndexError, NotImplementedError,
                zipfile.BadZipFile, OSError) as error:
            manifest["unsupported"][identifier] = {
                "title": title, "reason": f"import failed: {error}",
            }
            continue

        files = []
        for position, image in enumerate(images):
            if image is None:
                files.append("")
                continue
            filename = f"{name}-{position}.png"
            image.save(output / "textures" / filename, "PNG", optimize=True)
            files.append(filename)
        record.update({
            "title": title,
            "asset": identifier,
            "page": entry["page"],
            "sha256": entry["sha256"],
            "textures": files,
        })
        manifest["items"][name] = record
        for key, value in payload.items():
            geometry[f"{name}/{key}"] = value

    np.savez_compressed(output / "geometry.npz", **geometry)
    (output / "marketplace.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", default=str(DEFAULT_CACHE))
    parser.add_argument("--output", default=str(DEFAULT_OUTPUT))
    args = parser.parse_args()
    manifest = build(args.cache, args.output)
    counts = {}
    for record in manifest["items"].values():
        counts[record["slot"]] = counts.get(record["slot"], 0) + 1
    print(json.dumps({
        "imported": len(manifest["items"]),
        "by_slot": counts,
        "unsupported": len(manifest["unsupported"]),
    }, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
