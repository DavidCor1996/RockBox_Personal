#!/usr/bin/env python3
"""Extract a compact rig pack from Microsoft's real XNA Avatar FBX files.

This tool reads the genuine `AvatarAnimPack_4_0_FBX` scenes (Ms-PL) and writes
the real mesh parts, every original UV channel, the embedded Microsoft texture
maps, and the baked per-frame skinned vertex positions.  Nothing is drawn,
generated, traced, or substituted: every number written here is evaluated from
Microsoft's own geometry, skin clusters, and animation curves.

The resulting pack lets RockPod compose avatars from the real wardrobe meshes
and render them through the authentic Xbox avatar material model instead of
tinting flat pre-rendered sprite strips.
"""

from __future__ import annotations

import argparse
import io
import json
import struct
import zlib
from pathlib import Path

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "assets/ipodjs/sources/xbox360/avatar/rig"

# Real mesh nodes in the Microsoft scene, keyed by the name RockPod uses.
PARTS = {
    "boy-body": "boy_body_mesh_Natal",
    "boy-head": "BlendShapeEarDefault_Natal",
    "boy-hair": "boy_hair_Natal",
    "boy-top": "boy_top_Natal",
    "boy-bottoms": "boy_bottoms_Natal",
    "boy-shoes": "boy_shoes_Natal",
    "girl-body": "girl_body_mesh_Natal",
    "girl-body-heelleg": "girl_body_heelleg_mesh_Natal",
    "girl-head": "girlHead_BlendShapeEarDefault_Natal",
    "girl-hair": "girl_hair_Natal",
    "girl-top": "girl_top_Natal",
    "girl-bottoms": "girl_bottoms_Natal",
    "girl-shoes": "girl_shoes_Natal",
    "girl-highheels": "girl_highheels_mesh_Natal",
}

# Clip name -> (FBX file stem, device frame count).  The frame counts match the
# cadence already deployed to the iPod so device budgets do not move.
CLIPS = {
    "jump": ("jump", 14),
    "throw": ("throw", 10),
    "faint": ("faint", 11),
    "sit-idle": ("sitIdle", 16),
    "punch": ("punch", 12),
    "kick": ("kick", 12),
    "walk": ("walk", 16),
}
TURNTABLE_CLIP = "turntable"
TURNTABLE_ANGLES = 24

# Bones that imported Marketplace props ride on.  A hat is rigid geometry, so
# it is expressed in the head bone's space once and then follows the real
# animation exactly as Microsoft's own meshes do.
ATTACHMENT_BONES = {
    "head": "HEAD__Skeleton",
    "back": "BACKB__Skeleton",
    "hand-left": "LF_W__Skeleton",
    "hand-right": "RT_W__Skeleton",
    "base": "BASE__Skeleton",
}
# The turntable stands the avatar in the neutral pose the jump clip opens on
# and rotates the camera around it, exactly as the existing device pack does.
TURNTABLE_SOURCE = ("jump", 0.0)


# ---------------------------------------------------------------------------
# Kaydara FBX 6100 binary reader
# ---------------------------------------------------------------------------


class Node:
    __slots__ = ("name", "props", "children")

    def __init__(self, name, props, children):
        self.name = name
        self.props = props
        self.children = children

    def find(self, name):
        return [child for child in self.children if child.name == name]

    def first(self, name):
        for child in self.children:
            if child.name == name:
                return child
        return None


def _read_prop(buf, pos):
    kind = chr(buf[pos])
    pos += 1
    if kind == "Y":
        return struct.unpack_from("<h", buf, pos)[0], pos + 2
    if kind == "C":
        return buf[pos], pos + 1
    if kind == "I":
        return struct.unpack_from("<i", buf, pos)[0], pos + 4
    if kind == "F":
        return struct.unpack_from("<f", buf, pos)[0], pos + 4
    if kind == "D":
        return struct.unpack_from("<d", buf, pos)[0], pos + 8
    if kind == "L":
        return struct.unpack_from("<q", buf, pos)[0], pos + 8
    if kind in "fdlib":
        length, encoding, compressed = struct.unpack_from("<III", buf, pos)
        pos += 12
        raw = buf[pos:pos + compressed]
        pos += compressed
        if encoding:
            raw = zlib.decompress(raw)
        code = {"f": "f", "d": "d", "l": "q", "i": "i", "b": "b"}[kind]
        return list(struct.unpack(f"<{length}{code}", raw)), pos
    if kind in "SR":
        size = struct.unpack_from("<I", buf, pos)[0]
        pos += 4
        raw = buf[pos:pos + size]
        pos += size
        return (raw.decode("utf-8", "replace") if kind == "S" else raw), pos
    raise ValueError(f"Unsupported FBX property type {kind!r}")


def _read_node(buf, pos):
    end, count, _length = struct.unpack_from("<III", buf, pos)
    pos += 12
    name_length = buf[pos]
    pos += 1
    name = buf[pos:pos + name_length].decode("utf-8", "replace")
    pos += name_length
    if end == 0:
        return None, pos
    props = []
    for _ in range(count):
        value, pos = _read_prop(buf, pos)
        props.append(value)
    children = []
    while pos < end:
        child, pos = _read_node(buf, pos)
        if child is None:
            break
        children.append(child)
    return Node(name, props, children), end


def read_fbx(path):
    buf = Path(path).read_bytes()
    if not buf.startswith(b"Kaydara FBX Binary"):
        raise ValueError(f"{path} is not a binary FBX scene")
    version = struct.unpack_from("<I", buf, 23)[0]
    if version != 6100:
        raise ValueError(f"Unsupported FBX version {version}")
    pos = 27
    children = []
    while pos < len(buf) - 16:
        node, pos = _read_node(buf, pos)
        if node is None:
            break
        children.append(node)
    return Node("Root", [], children)


def object_name(value):
    """Return the bare object name from an FBX 6100 ``name\\0\\1class`` token."""
    return value.split("\x00\x01")[0]


def object_class(value):
    """Return the class part of an FBX 6100 ``name\\0\\1class`` token."""
    parts = value.split("\x00\x01")
    return parts[1] if len(parts) > 1 else ""


# ---------------------------------------------------------------------------
# Scene indexing and node transforms
# ---------------------------------------------------------------------------


FBX_TIME_UNIT = 46186158000.0


class Scene:
    def __init__(self, root):
        self.root = root
        objects = root.first("Objects")
        self.models = {}
        self.deformers = {}
        self.subdeformers = {}
        self.textures = {}
        self.videos = {}
        for node in objects.children:
            name = object_name(node.props[0]) if node.props else ""
            if node.name == "Model":
                self.models[name] = node
            elif node.name == "Deformer":
                kind = node.props[1] if len(node.props) > 1 else ""
                (self.subdeformers if kind == "Cluster"
                 else self.deformers)[name] = node
            elif node.name == "Texture":
                self.textures[name] = node
            elif node.name == "Video":
                self.videos[name] = node

        self.parent = {}
        self.children_of = {}
        self.connections = []
        for link in root.first("Connections").children:
            if len(link.props) < 3 or link.props[0] != "OO":
                continue
            child, parent = link.props[1], link.props[2]
            self.connections.append((object_name(child), object_class(child),
                                     object_name(parent), object_class(parent)))
        for child, child_class, parent, parent_class in self.connections:
            if child_class == "Model" and parent_class == "Model":
                self.parent[child] = parent
                self.children_of.setdefault(parent, []).append(child)

        self._local_cache = {}
        self._anim = self._read_take()

    # -- transforms -------------------------------------------------------

    @staticmethod
    def _properties(node):
        result = {}
        block = node.first("Properties60")
        if block is None:
            return result
        for prop in block.children:
            if prop.props:
                result[prop.props[0]] = prop.props[3:]
        return result

    def _static_local(self, name):
        if name not in self._local_cache:
            props = self._properties(self.models[name])
            self._local_cache[name] = (
                list(props.get("Lcl Translation", [0.0, 0.0, 0.0])),
                list(props.get("Lcl Rotation", [0.0, 0.0, 0.0])),
                list(props.get("Lcl Scaling", [1.0, 1.0, 1.0])),
            )
        return self._local_cache[name]

    def _read_take(self):
        take = self.root.first("Takes").first("Take")
        self.take_start, self.take_end = take.first("LocalTime").props[:2]
        curves = {}
        for model in take.find("Model"):
            name = object_name(model.props[0])
            channels = {}
            transform = model.first("Channel")
            if transform is None:
                continue
            for group in transform.find("Channel"):
                key = group.props[0]
                if key not in ("T", "R", "S"):
                    continue
                axes = []
                for axis in group.find("Channel"):
                    keys = axis.first("Key")
                    default = axis.first("Default")
                    if keys is None or not keys.props:
                        axes.append((None, None,
                                     default.props[0] if default else 0.0))
                        continue
                    count = axis.first("KeyCount").props[0]
                    flat = keys.props
                    stride = len(flat) // count
                    times = np.array(flat[0::stride][:count], dtype=np.float64)
                    values = np.array(flat[1::stride][:count],
                                      dtype=np.float64)
                    axes.append((times, values, values[0]))
                channels[key] = axes
            if channels:
                curves[name] = channels
        return curves

    def _sample(self, name, time):
        static = self._static_local(name)
        translation, rotation, scaling = (list(value) for value in static)
        channels = self._anim.get(name)
        if channels:
            for key, target in (("T", translation), ("R", rotation),
                                ("S", scaling)):
                axes = channels.get(key)
                if not axes:
                    continue
                for index, (times, values, default) in enumerate(axes[:3]):
                    if times is None:
                        target[index] = default
                    else:
                        target[index] = float(np.interp(time, times, values))
        return translation, rotation, scaling

    @staticmethod
    def _compose(translation, rotation, scaling):
        rx, ry, rz = np.radians(rotation)
        cx, sx = np.cos(rx), np.sin(rx)
        cy, sy = np.cos(ry), np.sin(ry)
        cz, sz = np.cos(rz), np.sin(rz)
        mx = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]])
        my = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
        mz = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
        matrix = np.eye(4)
        matrix[:3, :3] = (mz @ my @ mx) * np.asarray(scaling)
        matrix[:3, 3] = translation
        return matrix

    def global_transforms(self, time):
        """Return every model's world matrix at ``time`` FBX ticks."""
        result = {}

        def resolve(name):
            cached = result.get(name)
            if cached is not None:
                return cached
            local = self._compose(*self._sample(name, time))
            parent = self.parent.get(name)
            matrix = resolve(parent) @ local if parent in self.models else local
            result[name] = matrix
            return matrix

        for name in self.models:
            resolve(name)
        return result


# ---------------------------------------------------------------------------
# Mesh extraction
# ---------------------------------------------------------------------------


def _layer_uv(layer, corner_count):
    values = np.asarray(layer.first("UV").props, dtype=np.float32)
    values = values.reshape(-1, 2)
    reference = layer.first("ReferenceInformationType").props[0]
    if reference == "IndexToDirect":
        index = np.asarray(layer.first("UVIndex").props, dtype=np.int64)
        values = values[index]
    if values.shape[0] != corner_count:
        raise ValueError("UV layer does not match the polygon corner count")
    return values


def extract_mesh(scene, node_name):
    """Return the real geometry, every original UV channel, and its textures."""
    node = scene.models[node_name]
    positions = np.asarray(node.first("Vertices").props,
                           dtype=np.float64).reshape(-1, 3)
    raw = np.asarray(node.first("PolygonVertexIndex").props, dtype=np.int64)

    polygons = []
    current = []
    for corner, value in enumerate(raw):
        if value < 0:
            current.append((corner, int(~value)))
            polygons.append(current)
            current = []
        else:
            current.append((corner, int(value)))
    if current:
        polygons.append(current)

    triangles = []
    corners = []
    polygon_of_triangle = []
    for polygon_index, polygon in enumerate(polygons):
        for offset in range(1, len(polygon) - 1):
            fan = (polygon[0], polygon[offset], polygon[offset + 1])
            triangles.append([vertex for _corner, vertex in fan])
            corners.append([corner for corner, _vertex in fan])
            polygon_of_triangle.append(polygon_index)
    triangles = np.asarray(triangles, dtype=np.int32)
    corners = np.asarray(corners, dtype=np.int64)
    polygon_of_triangle = np.asarray(polygon_of_triangle, dtype=np.int64)

    uv_layers = node.find("LayerElementUV")
    texture_layers = node.find("LayerElementTexture")
    connected = [child for child, child_class, parent, _parent_class
                 in scene.connections
                 if parent == node_name and child_class == "Texture"]

    channels = []
    for index, layer in enumerate(uv_layers):
        name = layer.first("Name").props[0]
        uv = _layer_uv(layer, raw.shape[0])[corners]
        # Texture bindings are per polygon: the head binds the left-eye map to
        # the left face polygons and the right-eye map to the right ones.
        texture_of_triangle = np.full(len(triangles), -1, dtype=np.int32)
        if index < len(texture_layers):
            ids = texture_layers[index].first("TextureId")
            if ids is not None and ids.props:
                values = np.asarray(ids.props, dtype=np.int32)
                if values.shape[0] == 1:
                    texture_of_triangle[:] = values[0]
                else:
                    texture_of_triangle = values[polygon_of_triangle].astype(
                        np.int32
                    )
        channels.append({"name": name, "uv": uv.astype(np.float32),
                         "texture_of_triangle": texture_of_triangle})

    material_layer = node.first("LayerElementMaterial")
    if material_layer is None:
        material_of_triangle = np.zeros(len(triangles), dtype=np.int32)
    else:
        values = np.asarray(material_layer.first("Materials").props,
                            dtype=np.int32)
        mapping = material_layer.first("MappingInformationType").props[0]
        if mapping == "AllSame" or values.shape[0] == 1:
            material_of_triangle = np.full(len(triangles), int(values[0]),
                                           dtype=np.int32)
        else:
            material_of_triangle = values[polygon_of_triangle].astype(np.int32)

    materials = [child for child, child_class, parent, _parent_class
                 in scene.connections
                 if parent == node_name and child_class == "Material"]
    return {
        "positions": positions,
        "triangles": triangles,
        "channels": channels,
        "material_of_triangle": material_of_triangle,
        "materials": materials,
        "textures": connected,
    }


def extract_skin(scene, node_name):
    """Return (bone names, control point indices, weights) for a real skin."""
    skins = [child for child, child_class, parent, _parent_class
             in scene.connections
             if parent == node_name and child_class == "Deformer"]
    if not skins:
        return [], None, None
    skin = skins[0]
    clusters = [child for child, child_class, parent, _parent_class
                in scene.connections
                if parent == skin and child_class == "SubDeformer"]
    bones, binds, entries = [], [], []
    for cluster_name in clusters:
        cluster = scene.subdeformers[cluster_name]
        links = [child for child, child_class, parent, _parent_class
                 in scene.connections
                 if parent == cluster_name and child_class == "Model"]
        if not links:
            continue
        indexes = cluster.first("Indexes")
        weights = cluster.first("Weights")
        if indexes is None or weights is None or not indexes.props:
            continue
        # FBX 6100 clusters store `Transform` as the complete inverse bind
        # matrix (mesh bind global folded into the inverse of the link bind
        # global), so the posed matrix is simply BoneCurrent @ Transform.
        transform = np.asarray(cluster.first("Transform").props,
                               dtype=np.float64).reshape(4, 4).T
        bone_index = len(bones)
        bones.append(links[0])
        binds.append(transform)
        entries.append((bone_index,
                        np.asarray(indexes.props, dtype=np.int64),
                        np.asarray(weights.props, dtype=np.float64)))
    return bones, np.asarray(binds), entries


def bone_bind_globals(scene, names):
    """Return each bone's world matrix in Microsoft's own bind pose.

    The bind pose is what the mesh control points and every imported item are
    expressed in, so it -- not the first animation frame -- is the frame a
    rigid attachment has to be taken out of.
    """
    found = {}
    for _part, node_name in PARTS.items():
        skins = [child for child, child_class, parent, _parent_class
                 in scene.connections
                 if parent == node_name and child_class == "Deformer"]
        if not skins:
            continue
        clusters = [child for child, child_class, parent, _parent_class
                    in scene.connections
                    if parent == skins[0] and child_class == "SubDeformer"]
        for cluster_name in clusters:
            cluster = scene.subdeformers[cluster_name]
            links = [child for child, child_class, parent, _parent_class
                     in scene.connections
                     if parent == cluster_name and child_class == "Model"]
            if not links or links[0] in found:
                continue
            link = cluster.first("TransformLink")
            if link is None:
                continue
            found[links[0]] = np.asarray(
                link.props, dtype=np.float64
            ).reshape(4, 4).T
    missing = [name for name in names if name not in found]
    if missing:
        raise ValueError(f"no bind matrix for {missing}")
    return np.stack([found[name] for name in names])


def attachment_matrices(scene, time):
    """Return the world matrix of every prop attachment bone at `time`."""
    globals_at_time = scene.global_transforms(time)
    return np.stack([globals_at_time[bone]
                     for bone in ATTACHMENT_BONES.values()])


def skin_matrix_stack(scene, bones, binds, time):
    globals_at_time = scene.global_transforms(time)
    return np.stack([globals_at_time[bone] @ bind
                     for bone, bind in zip(bones, binds)])


def dense_skin(mesh, bones, binds, entries, bone_index, limit=4):
    """Return per-vertex (bone slot, weight) rows against the global bone table.

    Imported Marketplace geometry is fitted by copying these rows from the
    nearest real avatar vertex, so a costume deforms with Microsoft's own skin
    instead of being animated by guesswork.
    """
    count = mesh["positions"].shape[0]
    weights = {}
    for local_index, indexes, values in entries:
        slot = bone_index[bones[local_index]]
        for vertex, weight in zip(indexes, values):
            if weight <= 0.0:
                continue
            weights.setdefault(int(vertex), []).append((slot, float(weight)))
    slots = np.zeros((count, limit), dtype=np.int32)
    amounts = np.zeros((count, limit), dtype=np.float32)
    for vertex, pairs in weights.items():
        pairs.sort(key=lambda item: item[1], reverse=True)
        pairs = pairs[:limit]
        total = sum(weight for _slot, weight in pairs) or 1.0
        for column, (slot, weight) in enumerate(pairs):
            slots[vertex, column] = slot
            amounts[vertex, column] = weight / total
    unweighted = amounts.sum(axis=1) <= 0.0
    amounts[unweighted, 0] = 1.0
    return slots, amounts


def bake_positions(mesh, entries, matrices):
    """Apply the real linear-blend skin for one evaluated frame."""
    positions = mesh["positions"]
    homogeneous = np.concatenate(
        [positions, np.ones((positions.shape[0], 1))], axis=1
    )
    accumulated = np.zeros_like(positions)
    total = np.zeros((positions.shape[0], 1))
    for bone_index, indexes, weights in entries:
        matrix = matrices[bone_index]
        transformed = homogeneous[indexes] @ matrix.T
        accumulated[indexes] += transformed[:, :3] * weights[:, None]
        total[indexes, 0] += weights
    unweighted = total[:, 0] <= 1e-6
    if unweighted.any():
        accumulated[unweighted] = positions[unweighted]
        total[unweighted, 0] = 1.0
    return accumulated / total


# ---------------------------------------------------------------------------
# Textures
# ---------------------------------------------------------------------------


def _video_filename(video):
    relative = video.first("RelativeFilename")
    if relative is None or not relative.props:
        return ""
    return relative.props[0].replace("\\", "/").rsplit("/", 1)[-1]


def export_textures(scene, names, destination):
    """Write the Microsoft texture maps embedded in the real FBX scene.

    Several texture objects reference the same original TGA and only the first
    one carries the embedded bytes, so images are keyed by source filename and
    every texture resolves through that shared table.
    """
    destination.mkdir(parents=True, exist_ok=True)
    images = {}
    for video in scene.videos.values():
        content = video.first("Content")
        if content is None or not content.props or not content.props[0]:
            continue
        filename = _video_filename(video)
        if not filename or filename in images:
            continue
        with Image.open(io.BytesIO(bytes(content.props[0]))) as source:
            images[filename] = source.convert("RGBA")

    written = {}
    for filename, image in images.items():
        path = destination / (Path(filename).stem + ".png")
        image.save(path, "PNG", optimize=True)
        written[filename] = {"file": path.name, "size": list(image.size)}

    bindings = {}
    for name in sorted(names):
        video_name = None
        for child, child_class, parent, _parent_class in scene.connections:
            if parent == name and child_class == "Video":
                video_name = child
                break
        video = scene.videos.get(video_name or name)
        if video is None:
            continue
        filename = _video_filename(video)
        entry = written.get(filename)
        bindings[name] = {
            "source": filename,
            "file": entry["file"] if entry else "",
            "size": entry["size"] if entry else None,
        }
    return bindings


# ---------------------------------------------------------------------------
# Pack building
# ---------------------------------------------------------------------------


def build(fbx_root, output_root):
    fbx_root = Path(fbx_root)
    output_root = Path(output_root)
    output_root.mkdir(parents=True, exist_ok=True)

    base = Scene(read_fbx(fbx_root / "jump.fbx"))
    manifest = {
        "schema": 1,
        "source": "Microsoft XNA Game Studio 4.0 Avatar Animation Pack (FBX)",
        "license": "Microsoft Permissive License (Ms-PL)",
        "archive": (
            "https://github.com/tgc-utn/xna-game-studio/tree/master/"
            "Samples/AvatarAnimPack_4_0_FBX"
        ),
        "hand_drawn_or_generated_geometry": False,
        "parts": {},
        "clips": {},
        "textures": {},
        "attachment_bones": list(ATTACHMENT_BONES),
    }

    meshes = {}
    skins = {}
    texture_names = set()
    geometry = {}
    bone_order = []
    for _part, node_name in PARTS.items():
        for bone in extract_skin(base, node_name)[0]:
            if bone not in bone_order:
                bone_order.append(bone)
    bone_index = {bone: index for index, bone in enumerate(bone_order)}
    manifest["bones"] = bone_order

    for part, node_name in PARTS.items():
        mesh = extract_mesh(base, node_name)
        bones, binds, entries = extract_skin(base, node_name)
        meshes[part] = mesh
        skins[part] = (bones, binds, entries)
        slots, amounts = dense_skin(mesh, bones, binds, entries, bone_index)
        geometry[f"{part}/skin_bones"] = slots
        geometry[f"{part}/skin_weights"] = amounts
        geometry[f"{part}/triangles"] = mesh["triangles"]
        geometry[f"{part}/material"] = mesh["material_of_triangle"]
        channel_names = []
        for index, channel in enumerate(mesh["channels"]):
            geometry[f"{part}/uv{index}"] = channel["uv"]
            geometry[f"{part}/tex{index}"] = channel["texture_of_triangle"]
            channel_names.append(channel["name"])
        texture_names.update(mesh["textures"])
        manifest["parts"][part] = {
            "node": node_name,
            "vertices": int(mesh["positions"].shape[0]),
            "triangles": int(mesh["triangles"].shape[0]),
            "channels": channel_names,
            "textures": mesh["textures"],
            "materials": mesh["materials"],
            "bones": len(bones),
        }

    geometry["__bind_bones__"] = bone_bind_globals(
        base, list(ATTACHMENT_BONES.values())
    ).astype(np.float32)
    for part, node_name in PARTS.items():
        mesh = meshes[part]
        geometry[f"{part}/bind"] = mesh["positions"].astype(np.float32)
    np.savez_compressed(output_root / "geometry.npz", **geometry)
    manifest["textures"] = export_textures(base, texture_names,
                                           output_root / "textures")

    clips_root = output_root / "clips"
    clips_root.mkdir(parents=True, exist_ok=True)
    def bone_binds(scene):
        """Resolve the inverse bind matrix of every table bone in `scene`."""
        found = {}
        for _part, node_name in PARTS.items():
            bones, binds, _entries = extract_skin(scene, node_name)
            for bone, bind in zip(bones, binds):
                found.setdefault(bone, bind)
        return np.stack([found[bone] for bone in bone_order])

    for clip, (stem, frames) in CLIPS.items():
        scene = base if stem == "jump" else Scene(read_fbx(fbx_root / f"{stem}.fbx"))
        clip_binds = bone_binds(scene)
        times = np.linspace(scene.take_start, scene.take_end, frames,
                            endpoint=False)
        baked = {}
        for part, node_name in PARTS.items():
            bones, binds, entries = extract_skin(scene, node_name)
            mesh = extract_mesh(scene, node_name)
            positions = np.stack([
                bake_positions(mesh, entries,
                               skin_matrix_stack(scene, bones, binds, time))
                for time in times
            ])
            baked[part] = positions.astype(np.float32)
        baked["__bones__"] = np.stack([
            attachment_matrices(scene, time) for time in times
        ]).astype(np.float32)
        baked["__skin__"] = np.stack([
            skin_matrix_stack(scene, bone_order, clip_binds, time)
            for time in times
        ]).astype(np.float32)
        np.savez_compressed(clips_root / f"{clip}.npz", **baked)
        manifest["clips"][clip] = {"frames": frames, "source": f"{stem}.fbx"}

    # The turntable reuses one real evaluated pose; the camera orbits it.
    stem, phase = TURNTABLE_SOURCE
    scene = base if stem == "jump" else Scene(read_fbx(fbx_root / f"{stem}.fbx"))
    turntable_binds = bone_binds(scene)
    time = scene.take_start + (scene.take_end - scene.take_start) * phase
    baked = {}
    for part, node_name in PARTS.items():
        bones, binds, entries = extract_skin(scene, node_name)
        mesh = extract_mesh(scene, node_name)
        pose = bake_positions(mesh, entries,
                              skin_matrix_stack(scene, bones, binds, time))
        baked[part] = pose[None, :, :].astype(np.float32)
    baked["__bones__"] = attachment_matrices(scene, time)[None].astype(
        np.float32
    )
    baked["__skin__"] = skin_matrix_stack(
        scene, bone_order, turntable_binds, time
    )[None].astype(np.float32)
    np.savez_compressed(clips_root / f"{TURNTABLE_CLIP}.npz", **baked)
    manifest["clips"][TURNTABLE_CLIP] = {
        "frames": 1, "angles": TURNTABLE_ANGLES, "source": f"{stem}.fbx",
    }

    (output_root / "rig.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fbx-root", required=True,
                        help="directory holding the real AvatarAnimPack FBX files")
    parser.add_argument("--output-root", default=str(DEFAULT_OUTPUT))
    args = parser.parse_args()
    manifest = build(args.fbx_root, args.output_root)
    print(json.dumps({
        "parts": len(manifest["parts"]),
        "clips": {name: value["frames"]
                  for name, value in manifest["clips"].items()},
        "textures": len(manifest["textures"]),
    }, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
