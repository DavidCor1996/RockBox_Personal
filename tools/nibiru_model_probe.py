#!/usr/bin/env python3
"""Validate and inventory NiBiRu's owned AGDS v2 model/animation resources.

This clean-room probe describes the fixed skeleton-track layout used by the
retail files. It reads matrices and timing keys directly from the extracted
resources and does not inspect or capture the original game's framebuffer.
"""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path


TRACK_NAME_SIZE = 252
MATRIX_FLOATS = 16
MATRIX_SIZE = MATRIX_FLOATS * 4
BIND_MATRIX_COUNT = 6
EXPECTED_TRACK_COUNT = 73
FIRST_TRACK = b"Frame_Bip01_R_Toe0Nub"
MARTIN_MESH = b"Frame_Martin_podzim_int_"
CHARACTER_MESHES = (MARTIN_MESH, b"Frame_Martin_podzim_ext_")
CHARACTER_MESHES += (b"Frame_Martin_leto",)
SCENE_MESHES = (b"Frame_Cylinder02", b"Frame_sluchatko", MARTIN_MESH)
FIRST_SKIN_BONE = b"Frame_Bip01_Head"
NODE_NAME_OFFSET = 116
NODE_SIZE = 556
NODE_DEPTH_OFFSET = 536
SKIN_HEADER_SIZE = 340


def c_string(data: bytes, offset: int, size: int) -> str:
    end = data.find(b"\0", offset, offset + size)
    if end < 0:
        raise ValueError(f"unterminated string at {offset}")
    return data[offset:end].decode("ascii", errors="strict")


def track_size(frame_count: int) -> int:
    return (
        TRACK_NAME_SIZE
        + frame_count * 4
        + (BIND_MATRIX_COUNT + frame_count) * MATRIX_SIZE
    )


def probe_mesh_named(data: bytes, marker: bytes) -> dict[str, object] | None:
    mesh_name = data.find(marker)
    if mesh_name < 0:
        return None
    node_offset = mesh_name - NODE_NAME_OFFSET
    if node_offset < 0 or node_offset + NODE_SIZE > len(data):
        raise ValueError(f"truncated {marker!r} mesh node")
    header = struct.unpack_from("<12I", data, node_offset)
    (
        _zero0,
        _zero1,
        vertex_count,
        face_count,
        normal_count,
        material_count,
        texcoord_count,
        face_normal_count,
        _one,
        max_weights_per_vertex,
        max_weights_per_face,
        bone_count,
    ) = header
    if (
        header[:2] != (0, 0)
        or vertex_count == 0
        or face_count == 0
        or normal_count != vertex_count
        or texcoord_count != vertex_count
        or face_normal_count != face_count
        or material_count == 0
        or (bone_count > 0 and max_weights_per_vertex == 0)
        or max_weights_per_face < max_weights_per_vertex
    ):
        raise ValueError(f"unsupported {marker!r} mesh header")

    vertex_offset = node_offset + NODE_SIZE
    texcoord_offset = vertex_offset + vertex_count * 3 * 4
    material_index_offset = texcoord_offset + texcoord_count * 2 * 4
    weight_offset = material_index_offset + vertex_count * 4
    if bone_count:
        # Retail mesh+0x44 is a four-entry, 64-bit influence-mask table used
        # by the normal/tangent path.  It is followed by one uint32 influence
        # count per vertex and four *direct* uint32 bone indices per vertex.
        # The executable's skin loop indexes bone[bone_index] (0x4528b7), so
        # these values must never be interpreted as bit positions.
        mask_offset = weight_offset + vertex_count * max_weights_per_vertex * 4
        influence_count_offset = mask_offset + vertex_count * 4 * 8
        bone_index_offset = influence_count_offset + vertex_count * 4
        normal_offset = bone_index_offset + vertex_count * max_weights_per_vertex * 4
        vertex_map_offset = normal_offset + normal_count * 3 * 4
        skin_name_offset = vertex_map_offset + vertex_count * 4
        if not data.startswith(b"Frame_", skin_name_offset):
            raise ValueError("skin table missing at computed mesh boundary")
        if bone_index_offset + vertex_count * max_weights_per_vertex * 4 != normal_offset:
            raise ValueError("Martin influence tables do not match retail layout")
        influence_counts = struct.unpack_from(
            f"<{vertex_count}I", data, influence_count_offset
        )
        if not all(1 <= count <= max_weights_per_vertex for count in influence_counts):
            raise ValueError("Martin influence count outside retail bounds")
        bone_indices = struct.unpack_from(
            f"<{vertex_count * max_weights_per_vertex}I", data, bone_index_offset
        )
        for vertex, count in enumerate(influence_counts):
            start = vertex * max_weights_per_vertex
            if any(index >= bone_count for index in bone_indices[start : start + count]):
                raise ValueError("Martin influence bone index outside skin table")
    else:
        mask_offset = weight_offset
        influence_count_offset = 0
        bone_index_offset = 0
        skin_name_offset = 0
        normal_offset = material_index_offset + vertex_count * 4
        vertex_map_offset = normal_offset + normal_count * 3 * 4
    if material_index_offset > normal_offset:
        raise ValueError(f"overlapping {marker!r} mesh arrays")
    vertex_map = struct.unpack_from(f"<{vertex_count}I", data, vertex_map_offset)
    if vertex_map != tuple(range(vertex_count)):
        raise ValueError(f"{marker!r} vertex map is not identity")

    face_offset = (
        skin_name_offset + bone_count * SKIN_HEADER_SIZE
        if bone_count
        else vertex_map_offset + vertex_count * 4
    )
    face_size = face_count * 3 * 4
    if face_offset + face_size > len(data):
        raise ValueError(f"truncated {marker!r} face table")
    face_data = data[face_offset : face_offset + face_size]
    faces = struct.unpack_from(f"<{face_count * 3}I", face_data)
    if max(faces) >= vertex_count:
        raise ValueError(f"{marker!r} face index outside vertex table")
    if bone_count:
        face_normal_offset = data.find(face_data, face_offset + face_size)
        if face_normal_offset < 0:
            raise ValueError("Martin face-normal table not found")
    else:
        face_normal_offset = face_offset + face_size
        face_normals = struct.unpack_from(
            f"<{face_count * 3}I", data, face_normal_offset
        )
        if max(face_normals) >= normal_count:
            raise ValueError(f"{marker!r} face-normal index outside table")

    vertices = struct.unpack_from(f"<{vertex_count * 3}f", data, vertex_offset)
    texcoords = struct.unpack_from(f"<{texcoord_count * 2}f", data, texcoord_offset)
    normals = struct.unpack_from(f"<{normal_count * 3}f", data, normal_offset)
    return {
        "name": marker.decode("ascii"),
        "node_offset": node_offset,
        "node_matrix_offset": node_offset + 380,
        "vertex_count": vertex_count,
        "face_count": face_count,
        "normal_count": normal_count,
        "material_count": material_count,
        "texcoord_count": texcoord_count,
        "bone_count": bone_count,
        "max_weights_per_vertex": max_weights_per_vertex,
        "max_weights_per_face": max_weights_per_face,
        "vertex_offset": vertex_offset,
        "texcoord_offset": texcoord_offset,
        "normal_offset": normal_offset,
        "weight_offset": weight_offset,
        "mask_offset": mask_offset,
        "influence_count_offset": influence_count_offset,
        "bone_index_offset": bone_index_offset,
        "skin_name_offset": skin_name_offset,
        "face_offset": face_offset,
        "face_normal_offset": face_normal_offset,
        "position_bounds": [
            [min(vertices[axis::3]), max(vertices[axis::3])] for axis in range(3)
        ],
        "texcoord_bounds": [
            [min(texcoords[axis::2]), max(texcoords[axis::2])] for axis in range(2)
        ],
    }


def probe_mesh(data: bytes) -> dict[str, object] | None:
    for marker in CHARACTER_MESHES:
        mesh = probe_mesh_named(data, marker)
        if mesh is not None:
            return mesh
    for name in model_node_parents(data):
        offset = data.find(name.encode("ascii")) - NODE_NAME_OFFSET
        if offset >= 0 and struct.unpack_from("<I", data, offset + 8)[0]:
            return probe_mesh_named(data, name.encode("ascii"))
    return None


def model_node_parents(data: bytes) -> dict[str, str | None]:
    """Recover the authored frame hierarchy from the model node table.

    AGDS serializes each fixed-size node header's depth at offset 536.  Mesh
    payloads follow their node headers, so sibling headers are not always
    adjacent.  The headers themselves remain depth-first; the nearest valid
    preceding header at depth - 1 is the parent used by the retail recursive
    matrix update.
    """
    model_end = len(data)
    world_name = data.find(b"Frame_World", 0, model_end)
    if model_end < 0 or world_name < NODE_NAME_OFFSET:
        raise ValueError("model frame hierarchy missing")
    candidates: list[tuple[int, int, str]] = []
    cursor = world_name
    while cursor < model_end:
        cursor = data.find(b"Frame_", cursor, model_end)
        if cursor < 0:
            break
        start = cursor - NODE_NAME_OFFSET
        if start >= 0 and start + NODE_SIZE <= model_end:
            depth = struct.unpack_from("<I", data, start + NODE_DEPTH_OFFSET)[0]
            header_prefix = struct.unpack_from("<2I", data, start)
            end = data.find(b"\0", cursor, cursor + 252)
            if (
                header_prefix == (0, 0)
                and 1 <= depth <= 64
                and end >= 0
            ):
                try:
                    name = data[cursor:end].decode("ascii", errors="strict")
                except UnicodeDecodeError:
                    name = ""
                if name.startswith("Frame_"):
                    candidates.append((start, depth, name))
        cursor += len(b"Frame_")
    candidates.sort()
    stack: list[tuple[int, str]] = []
    parents: dict[str, str | None] = {}
    for _, depth, name in candidates:
        if name in parents:
            continue
        while stack and stack[-1][0] >= depth:
            stack.pop()
        parent = stack[-1][1] if stack else None
        if stack and stack[-1][0] != depth - 1:
            raise ValueError(f"non-contiguous model hierarchy at {name}")
        parents[name] = parent
        stack.append((depth, name))
    if parents.get("Frame_World", "missing") is not None:
        raise ValueError("model hierarchy root invalid")
    if FIRST_TRACK.decode("ascii") not in parents:
        raise ValueError("model hierarchy is missing skeleton frames")
    if "Frame_Bip01" not in parents:
        raise ValueError("model skeleton root missing")
    return parents


def probe_render_meshes(data: bytes) -> list[dict[str, object]]:
    """Return all authored meshes for an intro scene or a character model."""
    scene = [probe_mesh_named(data, marker) for marker in SCENE_MESHES]
    if all(mesh is not None for mesh in scene):
        return [mesh for mesh in scene if mesh is not None]
    character = probe_mesh(data)
    if character is None:
        raise ValueError("renderable character mesh missing")
    return [character]


def probe_scene_meshes(data: bytes) -> list[dict[str, object]]:
    meshes = []
    for marker in SCENE_MESHES:
        mesh = probe_mesh_named(data, marker)
        if mesh is None:
            raise ValueError(f"scene mesh missing: {marker.decode('ascii')}")
        meshes.append(mesh)
    return meshes


def export_obj(data: bytes, mesh: dict[str, object], output: Path) -> None:
    vertex_count = int(mesh["vertex_count"])
    face_count = int(mesh["face_count"])
    vertices = struct.unpack_from(
        f"<{vertex_count * 3}f", data, int(mesh["vertex_offset"])
    )
    texcoords = struct.unpack_from(
        f"<{vertex_count * 2}f", data, int(mesh["texcoord_offset"])
    )
    normals = struct.unpack_from(
        f"<{vertex_count * 3}f", data, int(mesh["normal_offset"])
    )
    faces = struct.unpack_from(
        f"<{face_count * 3}I", data, int(mesh["face_offset"])
    )
    with output.open("w", encoding="ascii", newline="\n") as stream:
        stream.write("# Direct clean-room export from owned NiBiRu model data\n")
        stream.write("mtllib martin_intro.mtl\nusemtl Martin_podzim_int\n")
        for index in range(vertex_count):
            stream.write(
                "v {:.9g} {:.9g} {:.9g}\n".format(*vertices[index * 3 : index * 3 + 3])
            )
        for index in range(vertex_count):
            u, v = texcoords[index * 2 : index * 2 + 2]
            stream.write(f"vt {u:.9g} {1.0 - v:.9g}\n")
        for index in range(vertex_count):
            stream.write(
                "vn {:.9g} {:.9g} {:.9g}\n".format(*normals[index * 3 : index * 3 + 3])
            )
        for index in range(face_count):
            a, b, c = (value + 1 for value in faces[index * 3 : index * 3 + 3])
            stream.write(f"f {a}/{a}/{a} {b}/{b}/{b} {c}/{c}/{c}\n")


def locate_tracks(data: bytes) -> tuple[int, int, int]:
    # Standalone speaking animations carry the track count at byte 61 and
    # begin their first fixed-width name at byte 69.
    standalone_count = struct.unpack_from("<I", data, 61)[0] if len(data) >= 69 else 0
    if (
        0 < standalone_count <= 256
        and data.startswith(b"Frame_", 69)
    ):
        name_offset = 69
        track_count = standalone_count
    else:
        # A model+animation resource appends the same track table after mesh
        # and material data. The last first-bone occurrence is unambiguous.
        name_offset = data.rfind(FIRST_TRACK)
        if name_offset < 0:
            raise ValueError("first skeleton track not found")
        track_count = EXPECTED_TRACK_COUNT

    tail_size = len(data) - name_offset
    if tail_size % track_count:
        raise ValueError("skeleton track table is not fixed-width")
    per_track = tail_size // track_count
    payload = per_track - TRACK_NAME_SIZE - BIND_MATRIX_COUNT * MATRIX_SIZE
    if payload <= 0 or payload % (4 + MATRIX_SIZE):
        raise ValueError("unsupported skeleton track payload")
    frame_count = payload // (4 + MATRIX_SIZE)
    if per_track != track_size(frame_count):
        raise ValueError("skeleton track size mismatch")
    return name_offset, per_track, frame_count


def probe(path: Path) -> dict[str, object]:
    data = path.read_bytes()
    name_offset, per_track, frame_count = locate_tracks(data)
    tracks = []
    track_count = (len(data) - name_offset) // per_track
    for index in range(track_count):
        start = name_offset + index * per_track
        name = c_string(data, start, TRACK_NAME_SIZE)
        times_offset = start + TRACK_NAME_SIZE
        matrices_offset = times_offset + frame_count * 4
        times = struct.unpack_from(f"<{frame_count}I", data, times_offset)
        if any(second <= first for first, second in zip(times, times[1:])):
            raise ValueError(f"non-increasing time keys in {name}")
        first_matrix = struct.unpack_from(
            "<16f", data, matrices_offset + BIND_MATRIX_COUNT * MATRIX_SIZE
        )
        tracks.append(
            {
                "index": index,
                "name": name,
                "time_first": times[0],
                "time_last": times[-1],
                "first_frame_matrix": list(first_matrix),
            }
        )

    texture_names = []
    for marker in (b"objektyintro.bmp", b"Martin_podzim-int.bmp"):
        if marker in data:
            texture_names.append(marker.decode("ascii"))

    mesh = probe_mesh(data)
    scene_meshes = probe_scene_meshes(data) if name_offset > 69 else []
    return {
        "format": 1,
        "source": str(path.resolve()),
        "file_size": len(data),
        "model_bytes": name_offset,
        "track_count": track_count,
        "track_size": per_track,
        "frame_count": frame_count,
        "bind_matrix_count": BIND_MATRIX_COUNT,
        "textures": texture_names,
        "mesh": mesh,
        "scene_meshes": scene_meshes,
        "tracks": tracks,
        "screen_capture_derived": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("resource", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument(
        "--export-obj", type=Path, help="write the directly decoded Martin mesh"
    )
    args = parser.parse_args()
    report = probe(args.resource)
    if args.export_obj:
        if report["mesh"] is None:
            parser.error("resource does not contain Martin's mesh")
        export_obj(args.resource.read_bytes(), report["mesh"], args.export_obj)
    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
