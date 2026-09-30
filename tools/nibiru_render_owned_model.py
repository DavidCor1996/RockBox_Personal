#!/usr/bin/env python3
"""Render a NiBiRu AGDS v2 mesh directly from owned extracted resources.

This is a development preview for the 320x240 software-render path. It reads
the retail model geometry and texture atlas; it never launches or captures the
Windows game.
"""

from __future__ import annotations

import argparse
import bisect
import math
import struct
from pathlib import Path

import numpy as np
from PIL import Image

from nibiru_model_probe import (
    BIND_MATRIX_COUNT,
    MATRIX_SIZE,
    SKIN_HEADER_SIZE,
    locate_tracks,
    model_node_parents,
    probe_mesh,
    probe_render_meshes,
)


WIDTH = 320
HEIGHT = 240
SOURCE_SCALE = 5.0 / 16.0

# Room 1864's values, traced at the original OpenGL calls.  These are the
# actual AGDS camera and model transforms, not screen-space calibration.
CAMERA_PITCH_DEGREES = 13.0
CAMERA_DISTANCE = 210.0
CAMERA_FOV_DEGREES = 50.0
CAMERA_NEAR = 32.0
CAMERA_FAR = 4096.0
CAMERA_ASPECT = 4.0 / 3.0
CAMERA_Z_TRANSLATION = -242.0
ACTOR_TRANSLATION = (5.949142, -189.070908, -116.186943)
ACTOR_ROTATION_DEGREES = 160.0
ROOT_CORRECTION = (4.098798, 0.0, 10.778934)

# Room object 1864.11b6 creates GL_LIGHT0 directly from these retail script
# values.  Its final zero makes GL_POSITION directional (w = 0), so there is
# no distance attenuation across Martin or the chair.
ROOM_LIGHT_DIRECTION = (0.0, 40.0, 70.0)
ROOM_LIGHT_AMBIENT = np.array((10.0, 10.0, 20.0), dtype=np.float64) / 255.0
ROOM_LIGHT_DIFFUSE = np.array((90.0, 211.0, 252.0), dtype=np.float64) / 255.0
OPENGL_GLOBAL_AMBIENT = np.array((0.2, 0.2, 0.2), dtype=np.float64)


def decode_array(data: bytes, offset: int, count: int, width: int) -> np.ndarray:
    return np.frombuffer(
        data, dtype="<f4", count=count * width, offset=offset
    ).reshape(count, width)


def translation_matrix(x: float, y: float, z: float) -> np.ndarray:
    matrix = np.eye(4, dtype=np.float64)
    matrix[:3, 3] = (x, y, z)
    return matrix


def original_scene_transforms() -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Reproduce room 1864's original fixed-function camera/actor transform.

    Animation-track matrices in the retail resource already contain the
    DirectX-to-renderer handedness conversion.  The bind-pose node matrices do
    not, so their fallback path applies the Z reflection separately.
    """
    pitch = math.radians(CAMERA_PITCH_DEGREES)
    forward = np.array(
        (0.0, -math.tan(pitch) * CAMERA_DISTANCE, -CAMERA_DISTANCE),
        dtype=np.float64,
    )
    forward /= np.linalg.norm(forward)
    up = np.array((0.0, 1.0, 0.0), dtype=np.float64)
    side = np.cross(forward, up)
    side /= np.linalg.norm(side)
    corrected_up = np.cross(side, forward)
    view = np.eye(4, dtype=np.float64)
    view[:3, :3] = np.array((side, corrected_up, -forward))

    focal = 1.0 / math.tan(math.radians(CAMERA_FOV_DEGREES) / 2.0)
    projection = np.zeros((4, 4), dtype=np.float64)
    projection[0, 0] = focal / CAMERA_ASPECT
    projection[1, 1] = focal
    projection[2, 2] = (CAMERA_FAR + CAMERA_NEAR) / (CAMERA_NEAR - CAMERA_FAR)
    projection[2, 3] = (
        2.0 * CAMERA_FAR * CAMERA_NEAR / (CAMERA_NEAR - CAMERA_FAR)
    )
    projection[3, 2] = -1.0

    angle = math.radians(ACTOR_ROTATION_DEGREES)
    rotation = np.array(
        (
            (math.cos(angle), 0.0, math.sin(angle), 0.0),
            (0.0, 1.0, 0.0, 0.0),
            (-math.sin(angle), 0.0, math.cos(angle), 0.0),
            (0.0, 0.0, 0.0, 1.0),
        ),
        dtype=np.float64,
    )
    room_modelview = view @ translation_matrix(0.0, 0.0, CAMERA_Z_TRANSLATION)
    actor_modelview = (
        room_modelview
        @ translation_matrix(*ACTOR_TRANSLATION)
        @ rotation
        @ translation_matrix(*ROOT_CORRECTION)
    )
    return projection, room_modelview, actor_modelview


def original_scene_matrix() -> np.ndarray:
    projection, _, actor_modelview = original_scene_transforms()
    return projection @ actor_modelview


def animation_track_matrices(data: bytes, frame: int) -> dict[str, np.ndarray]:
    name_offset, record_size, frame_count = locate_tracks(data)
    if not 0 <= frame < frame_count:
        raise ValueError(f"frame must be between 0 and {frame_count - 1}")
    local: dict[str, np.ndarray] = {}
    track_count = (len(data) - name_offset) // record_size
    for index in range(track_count):
        start = name_offset + index * record_size
        end = data.find(b"\0", start, start + 252)
        name = data[start:end].decode("ascii")
        matrix_offset = (
            start
            + 252
            + frame_count * 4
            + (BIND_MATRIX_COUNT + frame) * MATRIX_SIZE
        )
        local[name] = np.array(
            struct.unpack_from("<16f", data, matrix_offset), dtype=np.float32
        ).reshape(4, 4)
    return local


def _rotation_quaternion(matrix: np.ndarray) -> np.ndarray:
    """Convert a column-vector 3x3 rotation matrix to w,x,y,z."""
    trace = float(np.trace(matrix))
    if trace > 0.0:
        scale = math.sqrt(trace + 1.0) * 2.0
        quaternion = np.array(
            (
                0.25 * scale,
                (matrix[2, 1] - matrix[1, 2]) / scale,
                (matrix[0, 2] - matrix[2, 0]) / scale,
                (matrix[1, 0] - matrix[0, 1]) / scale,
            ),
            dtype=np.float64,
        )
    else:
        axis = int(np.argmax(np.diag(matrix)))
        if axis == 0:
            scale = math.sqrt(1.0 + matrix[0, 0] - matrix[1, 1] - matrix[2, 2]) * 2.0
            quaternion = np.array(
                ((matrix[2, 1] - matrix[1, 2]) / scale, 0.25 * scale,
                 (matrix[0, 1] + matrix[1, 0]) / scale,
                 (matrix[0, 2] + matrix[2, 0]) / scale), dtype=np.float64
            )
        elif axis == 1:
            scale = math.sqrt(1.0 + matrix[1, 1] - matrix[0, 0] - matrix[2, 2]) * 2.0
            quaternion = np.array(
                ((matrix[0, 2] - matrix[2, 0]) / scale,
                 (matrix[0, 1] + matrix[1, 0]) / scale, 0.25 * scale,
                 (matrix[1, 2] + matrix[2, 1]) / scale), dtype=np.float64
            )
        else:
            scale = math.sqrt(1.0 + matrix[2, 2] - matrix[0, 0] - matrix[1, 1]) * 2.0
            quaternion = np.array(
                ((matrix[1, 0] - matrix[0, 1]) / scale,
                 (matrix[0, 2] + matrix[2, 0]) / scale,
                 (matrix[1, 2] + matrix[2, 1]) / scale, 0.25 * scale),
                dtype=np.float64,
            )
    return quaternion / np.linalg.norm(quaternion)


def _quaternion_matrix(quaternion: np.ndarray) -> np.ndarray:
    w, x, y, z = quaternion
    return np.array(
        (
            (1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y - z * w),
             2.0 * (x * z + y * w)),
            (2.0 * (x * y + z * w), 1.0 - 2.0 * (x * x + z * z),
             2.0 * (y * z - x * w)),
            (2.0 * (x * z - y * w), 2.0 * (y * z + x * w),
             1.0 - 2.0 * (x * x + y * y)),
        ),
        dtype=np.float64,
    )


def interpolate_track_matrix(
    first: np.ndarray, second: np.ndarray, fraction: float
) -> np.ndarray:
    """Interpolate a retail row-vector SRT matrix without shearing bones."""
    if fraction <= 0.0:
        return first
    if fraction >= 1.0:
        return second
    first_scale = np.linalg.norm(first[:3, :3], axis=1)
    second_scale = np.linalg.norm(second[:3, :3], axis=1)
    first_rotation = (first[:3, :3] / first_scale[:, None]).T
    second_rotation = (second[:3, :3] / second_scale[:, None]).T
    first_quaternion = _rotation_quaternion(first_rotation)
    second_quaternion = _rotation_quaternion(second_rotation)
    dot = float(np.dot(first_quaternion, second_quaternion))
    if dot < 0.0:
        second_quaternion = -second_quaternion
        dot = -dot
    if dot > 0.9995:
        quaternion = first_quaternion + fraction * (
            second_quaternion - first_quaternion
        )
        quaternion /= np.linalg.norm(quaternion)
    else:
        angle = math.acos(max(-1.0, min(1.0, dot)))
        sine = math.sin(angle)
        quaternion = (
            math.sin((1.0 - fraction) * angle) / sine * first_quaternion
            + math.sin(fraction * angle) / sine * second_quaternion
        )
    scale = first_scale + fraction * (second_scale - first_scale)
    result = np.eye(4, dtype=np.float32)
    result[:3, :3] = (_quaternion_matrix(quaternion).T * scale[:, None]).astype(
        np.float32
    )
    result[3, :3] = (
        first[3, :3] + fraction * (second[3, :3] - first[3, :3])
    )
    return result


def animation_track_matrices_at(
    data: bytes, time_ms: int
) -> dict[str, np.ndarray]:
    """Sample all authored tracks at a millisecond timestamp."""
    name_offset, record_size, frame_count = locate_tracks(data)
    times = struct.unpack_from(f"<{frame_count}I", data, name_offset + 252)
    upper = bisect.bisect_right(times, time_ms)
    if upper == 0:
        return animation_track_matrices(data, 0)
    if upper >= frame_count:
        return animation_track_matrices(data, frame_count - 1)
    lower = upper - 1
    span = times[upper] - times[lower]
    fraction = (time_ms - times[lower]) / span
    first = animation_track_matrices(data, lower)
    second = animation_track_matrices(data, upper)
    return {
        name: interpolate_track_matrix(first[name], second[name], fraction)
        for name in first
    }


def skin_vertices(
    data: bytes,
    mesh: dict[str, object],
    frame: int,
    animation_data: bytes | None = None,
    local_matrices: dict[str, np.ndarray] | None = None,
) -> np.ndarray:
    vertex_count = int(mesh["vertex_count"])
    vertices = decode_array(data, int(mesh["vertex_offset"]), vertex_count, 3)
    local = local_matrices or animation_track_matrices(animation_data or data, frame)
    world = animation_world_matrices(data, local)

    weights = decode_array(data, int(mesh["weight_offset"]), vertex_count, 4)
    influence_counts = np.frombuffer(
        data,
        dtype="<u4",
        count=vertex_count,
        offset=int(mesh["influence_count_offset"]),
    )
    bone_indices = np.frombuffer(
        data,
        dtype="<u4",
        count=vertex_count * 4,
        offset=int(mesh["bone_index_offset"]),
    ).reshape(vertex_count, 4)
    homogeneous = np.column_stack((vertices, np.ones(vertex_count, dtype=np.float32)))
    skinned = np.zeros((vertex_count, 3), dtype=np.float32)
    skin_start = int(mesh["skin_name_offset"])
    for bone in range(int(mesh["bone_count"])):
        header = skin_start + bone * SKIN_HEADER_SIZE
        end = data.find(b"\0", header, header + 200)
        name = data[header:end].decode("ascii")
        bone_matrix = np.array(
            # Retail 0x454f7c composes world * bone+0x110 into bone+0xd0;
            # the skin loop at 0x4524d0 consumes that resulting 0xd0 matrix.
            struct.unpack_from("<16f", data, header + 272),
            dtype=np.float32,
        ).reshape(4, 4)
        transform = bone_matrix @ world[name]
        for influence in range(4):
            # Retail 0x4528b7 reads a direct uint32 index from mesh+0x4c,
            # multiplies it by the 0x154-byte bone-record size, then uses the
            # matching weight from mesh+0x40.  Treating this as a bit mask
            # assigns most vertices to the wrong bone and tears Martin apart.
            selected = (
                (influence_counts > influence)
                & (bone_indices[:, influence] == bone)
                & (weights[:, influence] != 0.0)
            )
            if not np.any(selected):
                continue
            transformed = homogeneous[selected] @ transform
            skinned[selected] += transformed[:, :3] * weights[selected, influence, None]
    return skinned


def skin_normals(
    data: bytes,
    mesh: dict[str, object],
    frame: int,
    animation_data: bytes | None = None,
    local_matrices: dict[str, np.ndarray] | None = None,
) -> np.ndarray:
    """Skin authored normals like retail 0x452881..0x452afa.

    The original loop applies each weighted bone's upper 3x3 and normalizes
    the result after accumulation.  Translation is intentionally excluded.
    """
    vertex_count = int(mesh["vertex_count"])
    normals = decode_array(data, int(mesh["normal_offset"]), vertex_count, 3)
    local = local_matrices or animation_track_matrices(animation_data or data, frame)
    world = animation_world_matrices(data, local)
    weights = decode_array(data, int(mesh["weight_offset"]), vertex_count, 4)
    influence_counts = np.frombuffer(
        data,
        dtype="<u4",
        count=vertex_count,
        offset=int(mesh["influence_count_offset"]),
    )
    bone_indices = np.frombuffer(
        data,
        dtype="<u4",
        count=vertex_count * 4,
        offset=int(mesh["bone_index_offset"]),
    ).reshape(vertex_count, 4)
    skinned = np.zeros((vertex_count, 3), dtype=np.float64)
    skin_start = int(mesh["skin_name_offset"])
    for bone in range(int(mesh["bone_count"])):
        header = skin_start + bone * SKIN_HEADER_SIZE
        end = data.find(b"\0", header, header + 200)
        name = data[header:end].decode("ascii")
        bone_matrix = np.array(
            struct.unpack_from("<16f", data, header + 272),
            dtype=np.float64,
        ).reshape(4, 4)
        transform = bone_matrix @ world[name]
        for influence in range(4):
            selected = (
                (influence_counts > influence)
                & (bone_indices[:, influence] == bone)
                & (weights[:, influence] != 0.0)
            )
            if not np.any(selected):
                continue
            transformed = normals[selected] @ transform[:3, :3]
            skinned[selected] += (
                transformed * weights[selected, influence, None]
            )
    lengths = np.linalg.norm(skinned, axis=1)
    valid = lengths > 1e-12
    skinned[valid] /= lengths[valid, None]
    skinned[~valid] = normals[~valid]
    return skinned.astype(np.float32)


def retail_vertex_lighting(
    normals: np.ndarray,
    modelview: np.ndarray,
    room_modelview: np.ndarray,
) -> np.ndarray:
    """Evaluate room 1864's fixed-function diffuse lighting per vertex."""
    normal_matrix = np.linalg.inv(modelview[:3, :3]).T
    eye_normals = (normal_matrix @ normals.T).T
    lengths = np.linalg.norm(eye_normals, axis=1)
    valid = lengths > 1e-12
    eye_normals[valid] /= lengths[valid, None]
    eye_normals[~valid] = 0.0

    # glLightfv transforms a directional GL_POSITION by the current
    # model-view rotation at definition time; translation is ignored for w=0.
    light = room_modelview[:3, :3] @ np.asarray(
        ROOM_LIGHT_DIRECTION, dtype=np.float64
    )
    light /= np.linalg.norm(light)
    diffuse = np.maximum(0.0, eye_normals @ light)
    # Both materials in the original intro resource are white.  Retail
    # 0x45e852 submits that color as GL_AMBIENT_AND_DIFFUSE; specular and
    # emission are zero, leaving the OpenGL default global ambient plus the
    # authored light ambient and diffuse terms.
    return np.clip(
        OPENGL_GLOBAL_AMBIENT
        + ROOM_LIGHT_AMBIENT
        + diffuse[:, None] * ROOM_LIGHT_DIFFUSE,
        0.0,
        1.0,
    ).astype(np.float32)


def animation_world_matrices(
    model_data: bytes,
    local: dict[str, np.ndarray],
) -> dict[str, np.ndarray]:
    """Compose authored frame keys exactly like retail function 0x454e26.

    The executable multiplies the parent world matrix by the local key in
    column-major storage.  These arrays are the transposed row-vector view of
    that memory, so the equivalent operation is local @ parent.
    """
    parents = model_node_parents(model_data)
    # Partial clips leave unkeyed bones in their authored model pose.
    local = dict(local)
    for name in parents:
        if name not in local:
            node = model_data.find(name.encode("ascii") + b"\0") - 116
            local[name] = np.asarray(
                struct.unpack_from("<16f", model_data, node + 380),
                dtype=np.float32,
            ).reshape(4, 4)
    world: dict[str, np.ndarray] = {}

    def compose(name: str) -> np.ndarray:
        if name not in world:
            if name not in parents:
                raise ValueError(
                    f"animation frame absent from model hierarchy: {name}"
                )
            parent = parents[name]
            if parent is None or parent not in local:
                world[name] = local[name]
            else:
                world[name] = local[name] @ compose(parent)
        return world[name]

    for name in local:
        compose(name)
    return world


def render_character_pose(
    model_path: Path,
    texture_path: Path,
    animation_path: Path,
    frame: int,
    yaw_degrees: float,
    time_ms: int | None = None,
    source_scale: float = SOURCE_SCALE,
) -> Image.Image:
    """Render one owned authored pose for the bounded iPod sprite path."""
    data = model_path.read_bytes()
    animation_data = animation_path.read_bytes()
    mesh = probe_mesh(data)
    if mesh is None:
        raise ValueError("renderable character mesh missing")
    local = (
        animation_track_matrices_at(animation_data, time_ms)
        if time_ms is not None
        else animation_track_matrices(animation_data, frame)
    )
    world = animation_world_matrices(data, local)
    vertices = skin_vertices(data, mesh, frame, animation_data, local)
    root = world["Frame_Bip01"][3, :3]
    vertices = vertices.copy()
    vertices[:, 0] -= root[0]
    vertices[:, 2] -= root[2]
    angle = math.radians(yaw_degrees)
    old_x = vertices[:, 0].copy()
    old_z = vertices[:, 2].copy()
    vertices[:, 0] = math.cos(angle) * old_x + math.sin(angle) * old_z
    vertices[:, 2] = -math.sin(angle) * old_x + math.cos(angle) * old_z

    pixels = np.zeros((HEIGHT, WIDTH, 3), dtype=np.uint8)
    zbuffer = np.full((HEIGHT, WIDTH), -np.inf, dtype=np.float32)
    coverage = np.zeros((HEIGHT, WIDTH), dtype=bool)
    transform = np.zeros((4, 4), dtype=np.float64)
    if not 0.125 <= source_scale <= 1.0:
        raise ValueError("character source scale must be between 0.125 and 1")
    transform[0, 0] = source_scale / (WIDTH / 2.0)
    transform[1, 1] = source_scale / (HEIGHT / 2.0)
    transform[1, 3] = 1.0 - 220.0 / (HEIGHT / 2.0)
    transform[2, 2] = 1.0 / 1024.0
    transform[3, 3] = 1.0
    rasterize_mesh(
        data,
        mesh,
        vertices,
        np.asarray(Image.open(texture_path).convert("RGB")),
        transform,
        pixels,
        zbuffer,
        coverage,
    )
    return Image.fromarray(
        np.dstack((pixels, coverage.astype(np.uint8) * 255)), "RGBA"
    )


def rasterize_mesh(
    data: bytes,
    mesh: dict[str, object],
    vertices: np.ndarray,
    texture: np.ndarray,
    transform: np.ndarray,
    pixels: np.ndarray,
    zbuffer: np.ndarray,
    coverage: np.ndarray,
    vertex_colors: np.ndarray | None = None,
) -> None:
    vertex_count = int(mesh["vertex_count"])
    face_count = int(mesh["face_count"])
    texcoords = decode_array(data, int(mesh["texcoord_offset"]), vertex_count, 2)
    faces = np.frombuffer(
        data,
        dtype="<u4",
        count=face_count * 3,
        offset=int(mesh["face_offset"]),
    ).reshape(face_count, 3)
    homogeneous = np.column_stack(
        (vertices, np.ones(vertex_count, dtype=np.float64))
    )
    clip = (transform @ homogeneous.T).T
    reciprocal_w = 1.0 / clip[:, 3]
    projected = clip[:, :3] * reciprocal_w[:, None]
    screen = np.column_stack(
        (
            (projected[:, 0] + 1.0) * (WIDTH / 2.0),
            (1.0 - (projected[:, 1] + 1.0) / 2.0) * HEIGHT,
        )
    )
    depth = -projected[:, 2]
    texture_h, texture_w = texture.shape[:2]

    for face in faces:
        points = screen[face]
        area = (
            (points[1, 0] - points[0, 0]) * (points[2, 1] - points[0, 1])
            - (points[1, 1] - points[0, 1]) * (points[2, 0] - points[0, 0])
        )
        if abs(area) < 1e-5:
            continue
        x0 = max(0, math.floor(float(points[:, 0].min())))
        x1 = min(WIDTH - 1, math.ceil(float(points[:, 0].max())))
        y0 = max(0, math.floor(float(points[:, 1].min())))
        y1 = min(HEIGHT - 1, math.ceil(float(points[:, 1].max())))
        if x0 > x1 or y0 > y1:
            continue
        uv = texcoords[face]
        face_depth = depth[face]
        face_reciprocal_w = reciprocal_w[face]
        for y in range(y0, y1 + 1):
            py = y + 0.5
            for x in range(x0, x1 + 1):
                px = x + 0.5
                w0 = (
                    (points[1, 0] - px) * (points[2, 1] - py)
                    - (points[1, 1] - py) * (points[2, 0] - px)
                ) / area
                w1 = (
                    (points[2, 0] - px) * (points[0, 1] - py)
                    - (points[2, 1] - py) * (points[0, 0] - px)
                ) / area
                w2 = 1.0 - w0 - w1
                if w0 < 0.0 or w1 < 0.0 or w2 < 0.0:
                    continue
                z = w0 * face_depth[0] + w1 * face_depth[1] + w2 * face_depth[2]
                if z <= zbuffer[y, x]:
                    continue
                perspective_weight = (
                    w0 * face_reciprocal_w[0]
                    + w1 * face_reciprocal_w[1]
                    + w2 * face_reciprocal_w[2]
                )
                if perspective_weight <= 0.0:
                    continue
                st = (
                    w0 * uv[0] * face_reciprocal_w[0]
                    + w1 * uv[1] * face_reciprocal_w[1]
                    + w2 * uv[2] * face_reciprocal_w[2]
                ) / perspective_weight
                tx = max(0, min(texture_w - 1, int(st[0] * texture_w)))
                # AGDS stores the model UV origin at the bottom-left while
                # Pillow exposes decoded BMP rows from the top-left.
                ty = max(0, min(texture_h - 1, int((1.0 - st[1]) * texture_h)))
                color = texture[ty, tx].astype(np.float32)
                if vertex_colors is not None:
                    lighting = (
                        w0 * vertex_colors[face[0]] * face_reciprocal_w[0]
                        + w1 * vertex_colors[face[1]] * face_reciprocal_w[1]
                        + w2 * vertex_colors[face[2]] * face_reciprocal_w[2]
                    ) / perspective_weight
                    color *= lighting
                pixels[y, x] = np.clip(color, 0.0, 255.0).astype(np.uint8)
                zbuffer[y, x] = z
                coverage[y, x] = True


def room_canvas(background_path: Path) -> Image.Image:
    room = Image.open(background_path).convert("RGB")
    canvas = Image.new("RGB", (WIDTH, HEIGHT), (0, 0, 0))
    room = room.resize((WIDTH, round(room.height * SOURCE_SCALE)), Image.Resampling.NEAREST)
    canvas.paste(room, (0, round(56 * SOURCE_SCALE)))
    return canvas


def apply_room_foreground(rendered: Image.Image, foreground_path: Path) -> None:
    foreground = Image.open(foreground_path).convert("RGB")
    foreground = foreground.resize(
        (round(foreground.width * SOURCE_SCALE), round(foreground.height * SOURCE_SCALE)),
        Image.Resampling.NEAREST,
    )
    fg = np.asarray(foreground)
    mask = Image.fromarray(
        np.any(fg != np.array([255, 0, 255]), axis=2).astype(np.uint8) * 255
    )
    foreground_x = round(335 * SOURCE_SCALE)
    foreground_y = round(511 * SOURCE_SCALE)
    room_bottom = round(696 * SOURCE_SCALE)
    visible_height = max(0, room_bottom - foreground_y)
    rendered.paste(
        foreground.crop((0, 0, foreground.width, visible_height)),
        (foreground_x, foreground_y),
        mask.crop((0, 0, foreground.width, visible_height)),
    )


def compose_room(background_path: Path, foreground_path: Path) -> Image.Image:
    rendered = room_canvas(background_path)
    apply_room_foreground(rendered, foreground_path)
    return rendered


def render(
    model_path: Path,
    texture_path: Path,
    output_path: Path | None,
    objects_texture_path: Path | None,
    animation_path: Path | None,
    background_path: Path | None,
    foreground_path: Path | None,
    frame: int | None,
    time_ms: int | None = None,
) -> Image.Image:
    data = model_path.read_bytes()
    animation_data = animation_path.read_bytes() if animation_path else data
    if background_path:
        pixels = np.asarray(room_canvas(background_path)).copy()
    else:
        pixels = np.zeros((HEIGHT, WIDTH, 3), dtype=np.uint8)
    zbuffer = np.full((HEIGHT, WIDTH), -np.inf, dtype=np.float32)
    coverage = np.zeros((HEIGHT, WIDTH), dtype=bool)
    martin_texture = np.asarray(Image.open(texture_path).convert("RGB"))
    if objects_texture_path is None:
        objects_texture_path = texture_path.with_name("objektyintro.bmp")
    objects_texture = np.asarray(Image.open(objects_texture_path).convert("RGB"))
    projection, room_modelview, actor_modelview = original_scene_transforms()
    local_matrices = None
    if time_ms is not None:
        local_matrices = animation_track_matrices_at(animation_data, time_ms)
    elif frame is not None:
        local_matrices = animation_track_matrices(animation_data, frame)
    # Animated node matrices are already expressed in the authored DirectX
    # scene space.  In particular, Martin and Frame_Cylinder02 both travel in
    # negative Z toward the telephone.  Mirroring that root translation makes
    # him roll away from the desk even if his skin and chair remain attached.
    bind_flip_z = np.diag((1.0, 1.0, -1.0, 1.0))
    for mesh in probe_render_meshes(data):
        vertex_count = int(mesh["vertex_count"])
        if int(mesh["bone_count"]):
            vertices = (
                skin_vertices(
                    data,
                    mesh,
                    frame if frame is not None else 0,
                    animation_data,
                    local_matrices,
                )
                if local_matrices is not None
                else decode_array(data, int(mesh["vertex_offset"]), vertex_count, 3)
            )
            normals = (
                skin_normals(
                    data,
                    mesh,
                    frame if frame is not None else 0,
                    animation_data,
                    local_matrices,
                )
                if local_matrices is not None
                else decode_array(data, int(mesh["normal_offset"]), vertex_count, 3)
            )
            texture = martin_texture
            modelview = (
                actor_modelview
                if local_matrices is not None
                else actor_modelview @ bind_flip_z
            )
        else:
            vertices = decode_array(
                data, int(mesh["vertex_offset"]), vertex_count, 3
            )
            normals = decode_array(
                data, int(mesh["normal_offset"]), vertex_count, 3
            )
            texture = objects_texture
            track_name = str(mesh["name"])
            if local_matrices is not None and track_name in local_matrices:
                # The chair and handset are authored animation tracks (the
                # final two records in this scene), not fixed set dressing.
                modelview = actor_modelview @ local_matrices[track_name].T
            else:
                local = np.array(
                    struct.unpack_from(
                        "<16f", data, int(mesh["node_matrix_offset"])
                    ),
                    dtype=np.float64,
                ).reshape(4, 4).T
                modelview = actor_modelview @ bind_flip_z @ local
        transform = projection @ modelview
        vertex_colors = retail_vertex_lighting(
            normals, modelview, room_modelview
        )
        rasterize_mesh(
            data, mesh, vertices, texture, transform,
            pixels, zbuffer, coverage, vertex_colors,
        )

    if background_path:
        rendered = Image.fromarray(pixels, "RGB")
    else:
        rgba = np.dstack((pixels, coverage.astype(np.uint8) * 255))
        rendered = Image.fromarray(rgba, "RGBA")
    if foreground_path:
        apply_room_foreground(rendered, foreground_path)
    if background_path:
        # Room 1864 is a 1024x640 authored tile positioned at canvas y=56.
        # Retail clips at absolute canvas y=696; applying that same scissor to
        # model pixels prevents feet/chair geometry leaking into the black UI
        # band below the room.
        clipped = np.asarray(rendered).copy()
        clipped[round(696 * SOURCE_SCALE) :, :] = 0
        rendered = Image.fromarray(clipped, rendered.mode)
    if output_path is not None:
        rendered.save(output_path)
    return rendered


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model", type=Path)
    parser.add_argument("texture", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--objects-texture", type=Path)
    parser.add_argument(
        "--animation", type=Path,
        help="standalone animation tracks to apply to the model geometry",
    )
    parser.add_argument("--background", type=Path)
    parser.add_argument("--foreground", type=Path)
    parser.add_argument(
        "--frame", type=int, default=0,
        help="direct animation frame to skin; use -1 for the unskinned bind mesh",
    )
    parser.add_argument(
        "--time-ms", type=int,
        help="sample the authored animation tracks at this timestamp",
    )
    args = parser.parse_args()
    render(
        args.model,
        args.texture,
        args.output,
        args.objects_texture,
        args.animation,
        args.background,
        args.foreground,
        None if args.frame < 0 else args.frame,
        args.time_ms,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
