#!/usr/bin/env python3
"""Build the Rockbox RuneScape Classic Lumbridge asset pack."""

import argparse
import bz2
import pathlib
import re
import struct

from PIL import Image, ImageDraw, ImageFont


WORLD_W = 96
WORLD_H = 96
REGION_SIZE = 48
LOCAL_SIZE = 96
TILE_COUNT = REGION_SIZE * REGION_SIZE
PACK_MAGIC = b"RSCL"
PACK_VERSION = 5
FACE_RECORD_BYTES = 28
JAGEX_TRANSPARENT = 12345678
COLOUR_TRANSPARENT = 32767
FLAG_BLOCKED = 0x01
FLAG_WALL = 0x02
FLAG_OBJECT = 0x04
TRANSPARENT_RGB = (255, 0, 255)


def get3(data, offset):
    return (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2]


def hash_name(name):
    value = 0
    for char in name.upper():
        value = value * 61 + ord(char) - 32
    return value & 0xFFFFFFFF


def read_cache_file(path):
    data = path.read_bytes()
    size = get3(data, 0)
    compressed_size = get3(data, 3)
    body = data[6 : 6 + compressed_size]
    if size != compressed_size:
        body = bz2.decompress(b"BZh1" + body)
    return body


def archive_entry(archive, name):
    count = (archive[0] << 8) | archive[1]
    offset = 2 + count * 10
    wanted = hash_name(name)

    for entry in range(count):
        base = 2 + entry * 10
        file_hash = int.from_bytes(archive[base : base + 4], "big")
        file_size = get3(archive, base + 4)
        archive_size = get3(archive, base + 7)

        if file_hash == wanted:
            body = archive[offset : offset + archive_size]
            if file_size != archive_size:
                body = bz2.decompress(b"BZh1" + body)
            return body

        offset += archive_size

    return None


def parse_entity_array(source, name):
    start = source.index(f"{name}[] = {{")
    start = source.index("{", start) + 1
    end = source.index("};", start)
    body = source[start:end]
    entries = []

    for match in re.finditer(r"\{\s*(-?\d+)\s*,\s*\{([^}]*)\}\s*\}", body):
        texture = int(match.group(1))
        coords = [float(value) for value in re.findall(r"-?\d+\.?\d*", match.group(2))]
        entries.append((texture, coords))

    return entries


def parse_skin_array(source):
    skin_length = 141
    start = source.index("gl_entities_skin_texture_positions[][SKIN_SPRITE_LENGTH] = {")
    start = source.index("{", start) + 1
    entries = []

    for match in re.finditer(r"\{\s*(-?\d+)\s*,\s*\{([^}]*)\}\s*\}", source[start:]):
        texture = int(match.group(1))
        coords = [float(value) for value in re.findall(r"-?\d+\.?\d*", match.group(2))]
        entries.append((texture, coords))
        if len(entries) >= skin_length:
            break

    return entries


def parse_skin_sprites(source):
    match = re.search(r"gl_entity_skin_sprites\[] = \{([^}]*)\}", source)
    if not match:
        return []

    return [int(value) for value in re.findall(r"\d+", match.group(1))]


def atlas_crop(gfx_dir, position):
    texture, coords = position
    if texture < 0:
        return None

    left, right, top, bottom = coords
    x = round(left * 1024)
    y = round(top * 1024)
    width = round((right - left) * 1024)
    height = round((bottom - top) * 1024)

    if width <= 0 or height <= 0:
        return None

    atlas = Image.open(gfx_dir / f"entities_{texture}.png").convert("RGBA")
    return atlas.crop((x, y, x + width, y + height))


def parse_media_positions(source):
    start = source.index("gl_media_atlas_positions[] = {")
    start = source.index("{", start) + 1
    end = source.index("};", start)
    body = source[start:end]
    positions = []

    for match in re.finditer(r"\{\s*([0-9.]+)f,\s*([0-9.]+)f,\s*([0-9.]+)f,\s*([0-9.]+)f\s*\}", body):
        positions.append(tuple(float(value) for value in match.groups()))

    return positions


def parse_model_texture_positions(source):
    start = source.index("gl_texture_atlas_positions[] = {")
    start = source.index("{", start) + 1
    end = source.index("};", start)
    body = source[start:end]
    positions = []

    for match in re.finditer(r"\{\s*([0-9.]+)f,\s*([0-9.]+)f,\s*([0-9.]+)f,\s*([0-9.]+)f\s*\}", body):
        positions.append(tuple(float(value) for value in match.groups()))

    return positions


def fill_to_rgb(fill, rsc_c_dir):
    if fill == JAGEX_TRANSPARENT or fill == COLOUR_TRANSPARENT:
        return (0, 0, 0)

    if fill >= 0:
        if not hasattr(fill_to_rgb, "positions"):
            fill_to_rgb.positions = parse_model_texture_positions(
                (rsc_c_dir / "src" / "gl" / "textures" /
                 "model_textures.c").read_text()
            )
            fill_to_rgb.atlas = Image.open(
                rsc_c_dir / "gfx" / "model_textures.png"
            ).convert("RGB")
        positions = fill_to_rgb.positions
        if fill >= len(positions):
            return (0, 0, 0)

        left, _right, top, _bottom = positions[fill]
        x = round(left * 1024)
        y = round(top * 1024)
        return fill_to_rgb.atlas.getpixel((x, y))

    fill = -(fill + 1)
    return (
        ((fill >> 10) & 0x1F) << 3,
        ((fill >> 5) & 0x1F) << 3,
        (fill & 0x1F) << 3,
    )


class ConfigReader:
    def __init__(self, strings, integers):
        self.strings = strings
        self.integers = integers
        self.string_offset = 0
        self.integer_offset = 0

    def string(self):
        start = self.string_offset
        while self.strings[self.string_offset]:
            self.string_offset += 1
        value = self.strings[start:self.string_offset].decode("latin1")
        self.string_offset += 1
        return value

    def byte(self):
        value = self.integers[self.integer_offset]
        self.integer_offset += 1
        return value

    def short(self):
        value = ((self.integers[self.integer_offset] << 8) |
                 self.integers[self.integer_offset + 1])
        self.integer_offset += 2
        return value

    def integer(self):
        value = ((self.integers[self.integer_offset] << 24) |
                 (self.integers[self.integer_offset + 1] << 16) |
                 (self.integers[self.integer_offset + 2] << 8) |
                 self.integers[self.integer_offset + 3])
        self.integer_offset += 4
        if value > 99999999:
            value = 99999999 - value
        return value


def skip_config_items(reader, version):
    count = reader.short()
    for _ in range(count * 3):
        reader.string()
    for _ in range(count):
        reader.short()
    for _ in range(count):
        reader.short() if version < 49 else reader.integer()
    for _ in range(count):
        reader.byte()
    for _ in range(count):
        reader.byte()
    for _ in range(count):
        reader.short()
    for _ in range(count):
        reader.integer()
    for _ in range(count):
        if version >= 49:
            reader.byte()
    for _ in range(count):
        if version >= 49:
            reader.byte()


def skip_config_npcs(reader, version):
    count = reader.short()
    for _ in range(count * 2):
        reader.string()
    for _ in range(count * 5):
        reader.byte()
    for _ in range(count * 12):
        reader.byte()
    for _ in range(count * 4):
        reader.integer()
    for _ in range(count * 2):
        reader.short()
    for _ in range(count * 3):
        reader.byte()
    if version >= 57:
        for _ in range(count):
            reader.string()


def load_game_config(cache_dir, rsc_c_dir):
    config = read_cache_file(cache_dir / "config85.jag")
    reader = ConfigReader(archive_entry(config, "string.dat"),
                          archive_entry(config, "integer.dat"))
    version = 85

    skip_config_items(reader, version)
    skip_config_npcs(reader, version)

    texture_count = reader.short()
    for _ in range(texture_count * 2):
        reader.string()

    animation_count = reader.short()
    for _ in range(animation_count):
        reader.string()
    for _ in range(animation_count):
        reader.integer()
    for _ in range(animation_count * 4):
        reader.byte()

    object_count = reader.short()
    for _ in range(object_count * 4):
        reader.string()
    for _ in range(object_count):
        reader.string()
    for _ in range(object_count * 4):
        reader.byte()

    wall_count = reader.short()
    wall_names = [reader.string() for _ in range(wall_count)]
    for _ in range(wall_count * 3):
        reader.string()
    wall_height = [reader.short() for _ in range(wall_count)]
    wall_front = [reader.integer() for _ in range(wall_count)]
    for _ in range(wall_count):
        reader.integer()
    for _ in range(wall_count):
        reader.byte()
    wall_interactive = [reader.byte() for _ in range(wall_count)]

    roof_count = reader.short()
    roof_height = [reader.byte() for _ in range(roof_count)]
    for _ in range(roof_count):
        reader.byte()

    tile_count = reader.short()
    tile_fill = [reader.integer() for _ in range(tile_count)]
    tile_type = [reader.byte() for _ in range(tile_count)]
    tile_blocking = [reader.byte() for _ in range(tile_count)]

    return {
        "tile_fill_rgb": [fill_to_rgb(fill, rsc_c_dir)
                          for fill in tile_fill],
        "tile_type": tile_type,
        "tile_blocking": tile_blocking,
        "wall_names": wall_names,
        "wall_height": wall_height,
        "wall_fill_rgb": [fill_to_rgb(fill, rsc_c_dir)
                          for fill in wall_front],
        "wall_interactive": wall_interactive,
        "roof_height": roof_height,
    }


def media_crop(rsc_c_dir, positions, sprite_id):
    left, right, top, bottom = positions[sprite_id]
    x0 = round(left * 1024)
    x1 = round(right * 1024)
    y0 = round(top * 1024)
    y1 = round(bottom * 1024)
    atlas = Image.open(rsc_c_dir / "gfx" / "sprites.png").convert("RGBA")
    return atlas.crop((x0, y0, x1, y1))


def compose_entity_piece(gfx_dir, positions, base_positions, skin_positions,
                         skin_lookup, sprite_id):
    piece = None

    for source in (positions[sprite_id],
                   base_positions[sprite_id],
                   skin_positions[skin_lookup.get(sprite_id, -1)] if sprite_id in skin_lookup else None):
        if source is None:
            continue

        layer = atlas_crop(gfx_dir, source)
        if layer is None:
            continue

        if piece is None:
            piece = Image.new("RGBA", layer.size, (0, 0, 0, 0))
        elif layer.size != piece.size:
            expanded = Image.new("RGBA", (
                max(piece.width, layer.width),
                max(piece.height, layer.height),
            ), (0, 0, 0, 0))
            expanded.alpha_composite(piece, ((expanded.width - piece.width) // 2,
                                             expanded.height - piece.height))
            piece = expanded

        piece.alpha_composite(layer, ((piece.width - layer.width) // 2,
                                      piece.height - layer.height))

    return piece


def paste_center_bottom(canvas, piece, center_x, bottom_y):
    if piece is None:
        return

    x = center_x - piece.width // 2
    y = bottom_y - piece.height
    canvas.alpha_composite(piece, (x, y))


def compose_default_male_view(rsc_c_dir, direction, flip=False):
    entities_c = rsc_c_dir / "src" / "gl" / "textures" / "entities.c"
    gfx_dir = rsc_c_dir / "gfx"

    if not entities_c.exists() or not gfx_dir.exists():
        raise FileNotFoundError(f"missing rsc-c atlas data under {rsc_c_dir}")

    source = entities_c.read_text()
    positions = parse_entity_array(source, "gl_entities_texture_positions")
    base_positions = parse_entity_array(source, "gl_entities_base_texture_positions")
    skin_positions = parse_skin_array(source)
    skin_lookup = {sprite: index for index, sprite in enumerate(parse_skin_sprites(source))}

    # Standing frame 0 from the canonical player animations. The client derives
    # the directional frame as animation_order * 3 + walk_frame.
    frame = direction * 3
    head = compose_entity_piece(gfx_dir, positions, base_positions, skin_positions,
                                skin_lookup, 0 + frame)
    body = compose_entity_piece(gfx_dir, positions, base_positions, skin_positions,
                                skin_lookup, 27 + frame)
    legs = compose_entity_piece(gfx_dir, positions, base_positions, skin_positions,
                                skin_lookup, 54 + frame)

    canvas = Image.new("RGBA", (44, 58), TRANSPARENT_RGB + (255,))
    paste_center_bottom(canvas, legs, 22, 58)
    paste_center_bottom(canvas, body, 22, 43)
    paste_center_bottom(canvas, head, 22, 18)

    if flip:
        canvas = canvas.transpose(Image.Transpose.FLIP_LEFT_RIGHT)

    return canvas.convert("RGB")


def write_default_male_character(rsc_c_dir, output):
    views = (
        (0, False),
        (2, False),
        (4, False),
        (2, True),
    )

    first = None
    for index, (direction, flip) in enumerate(views):
        image = compose_default_male_view(rsc_c_dir, direction, flip)
        image.save(output.with_name(f"character_male_{index}.bmp"))
        if first is None:
            first = image

    first.save(output)


def terrain_rgb(colour, height=96, decoration=0):
    if colour < 64:
        r = 255 - colour * 4
        g = 255 - (colour * 7) // 4
        b = 255 - colour * 4
    elif colour < 128:
        i = colour - 64
        r = i * 3
        g = 144
        b = 0
    elif colour < 192:
        i = colour - 128
        r = 192 - (i * 3) // 2
        g = 144 - (i * 3) // 2
        b = 0
    else:
        i = colour - 192
        r = 96 - (i * 3) // 2
        g = 48 + (i * 3) // 2
        b = 0

    if decoration:
        r += 24
        g += 24
        b += 12

    shade = (height - 96) // 10
    return (
        max(0, min(255, r + shade)),
        max(0, min(255, g + shade)),
        max(0, min(255, b + shade)),
    )


def decode_section(land_archive, map_archive, sx, sy):
    terrain_height = [0] * TILE_COUNT
    terrain_colour = [0] * TILE_COUNT
    walls_ns = [0] * TILE_COUNT
    walls_ew = [0] * TILE_COUNT
    walls_diag = [0] * TILE_COUNT
    walls_roof = [0] * TILE_COUNT
    tile_decoration = [0] * TILE_COUNT
    tile_direction = [0] * TILE_COUNT

    base = f"m0{sx // 10}{sx % 10}{sy // 10}{sy % 10}"
    hei = archive_entry(land_archive, base + ".hei")
    dat = archive_entry(map_archive, base + ".dat")
    loc = archive_entry(map_archive, base + ".loc")

    if hei:
        offset = 0
        last = 0
        tile = 0
        while tile < TILE_COUNT and offset < len(hei):
            value = hei[offset]
            offset += 1
            if value < 128:
                terrain_height[tile] = value
                tile += 1
                last = value
            else:
                for _ in range(value - 128):
                    terrain_height[tile] = last
                    tile += 1

        last = 64
        for y in range(REGION_SIZE):
            for x in range(REGION_SIZE):
                index = x * REGION_SIZE + y
                last = (terrain_height[index] + last) & 127
                terrain_height[index] = last * 2

        last = 0
        tile = 0
        while tile < TILE_COUNT and offset < len(hei):
            value = hei[offset]
            offset += 1
            if value < 128:
                terrain_colour[tile] = value
                tile += 1
                last = value
            else:
                for _ in range(value - 128):
                    terrain_colour[tile] = last
                    tile += 1

        last = 35
        for y in range(REGION_SIZE):
            for x in range(REGION_SIZE):
                index = x * REGION_SIZE + y
                last = (terrain_colour[index] + last) & 127
                terrain_colour[index] = last * 2

    if dat:
        offset = 0
        for target in (walls_ns, walls_ew, walls_diag):
            for tile in range(TILE_COUNT):
                target[tile] = dat[offset]
                offset += 1

        for tile in range(TILE_COUNT):
            value = dat[offset]
            offset += 1
            if value:
                walls_diag[tile] = value + 12000

        tile = 0
        while tile < TILE_COUNT and offset < len(dat):
            value = dat[offset]
            offset += 1
            if value < 128:
                walls_roof[tile] = value
                tile += 1
            else:
                tile += value - 128

        last = 0
        tile = 0
        while tile < TILE_COUNT and offset < len(dat):
            value = dat[offset]
            offset += 1
            if value < 128:
                tile_decoration[tile] = value
                last = value
                tile += 1
            else:
                for _ in range(value - 128):
                    tile_decoration[tile] = last
                    tile += 1

        tile = 0
        while tile < TILE_COUNT and offset < len(dat):
            value = dat[offset]
            offset += 1
            if value < 128:
                tile_direction[tile] = value
                tile += 1
            else:
                tile += value - 128

        if loc:
            offset = 0
            tile = 0
            while tile < TILE_COUNT and offset < len(loc):
                value = loc[offset]
                offset += 1
                if value < 128:
                    walls_diag[tile] = value + 48000
                    tile += 1
                else:
                    tile += value - 128

    return (terrain_colour, terrain_height, tile_decoration, walls_ns,
            walls_ew, walls_diag, walls_roof, tile_direction)


def wall_config(config, wall_value, diagonal_class=0):
    if diagonal_class == 2 or wall_value == 0:
        return (0, 0, 0, 0, 0)

    wall_id = wall_value - 1
    if wall_id < 0 or wall_id >= len(config["wall_height"]):
        return (0, 0, 0, 0, 0)

    draw = 0 if config["wall_interactive"][wall_id] else 1
    height = min(255, config["wall_height"][wall_id])
    r, g, b = config["wall_fill_rgb"][wall_id]
    return draw, height, r, g, b


def tile_config(config, decoration):
    if decoration <= 0:
        return (0, 0, 0, 0, 0)

    tile_id = decoration - 1
    if tile_id < 0 or tile_id >= len(config["tile_type"]):
        return (0, 0, 0, 0, 0)

    r, g, b = config["tile_fill_rgb"][tile_id]
    return (config["tile_type"][tile_id] & 0xFF,
            config["tile_blocking"][tile_id] & 0xFF,
            r & 0xFF, g & 0xFF, b & 0xFF)


def roof_config(config, roof):
    if roof <= 0:
        return 0

    roof_id = roof - 1
    if roof_id < 0 or roof_id >= len(config["roof_height"]):
        return 0

    return config["roof_height"][roof_id] & 0xFF


def build_region(cache_dir, center_x, center_y, config):
    land_archive = read_cache_file(cache_dir / "land63.jag")
    map_archive = read_cache_file(cache_dir / "maps63.jag")
    region = [[None for _ in range(LOCAL_SIZE)] for _ in range(LOCAL_SIZE)]
    chunks = [
        (center_x - 1, center_y - 1, 0, 0),
        (center_x, center_y - 1, REGION_SIZE, 0),
        (center_x - 1, center_y, 0, REGION_SIZE),
        (center_x, center_y, REGION_SIZE, REGION_SIZE),
    ]

    for sx, sy, ox, oy in chunks:
        (colour, height, decoration, wall_ns, wall_ew, wall_diag,
         wall_roof, tile_direction) = decode_section(land_archive,
                                                     map_archive, sx, sy)

        for x in range(REGION_SIZE):
            for y in range(REGION_SIZE):
                index = x * REGION_SIZE + y
                flags = 0
                if wall_ns[index] or wall_ew[index] or wall_diag[index]:
                    flags |= FLAG_WALL
                if wall_diag[index] >= 48000:
                    flags |= FLAG_OBJECT | FLAG_BLOCKED

                diag_class = (2 if wall_diag[index] >= 48000 else
                              1 if wall_diag[index] else 0)
                tile_type, tile_blocking, tile_r, tile_g, tile_b = tile_config(
                    config, decoration[index]
                )
                ns_draw, ns_height, ns_r, ns_g, ns_b = wall_config(
                    config, wall_ns[index]
                )
                ew_draw, ew_height, ew_r, ew_g, ew_b = wall_config(
                    config, wall_ew[index]
                )
                diag_draw, diag_height, diag_r, diag_g, diag_b = wall_config(
                    config, wall_diag[index] & 0xFF, diag_class
                )

                region[oy + y][ox + x] = (
                    colour[index] & 0xFF,
                    height[index] & 0xFF,
                    decoration[index] & 0xFF,
                    flags,
                    wall_ns[index] & 0xFF,
                    wall_ew[index] & 0xFF,
                    diag_class,
                    wall_diag[index] & 0xFF,
                    wall_roof[index] & 0xFF,
                    tile_direction[index] & 0xFF,
                    tile_type,
                    tile_blocking,
                    tile_r,
                    tile_g,
                    tile_b,
                    ns_draw,
                    ns_height,
                    ns_r,
                    ns_g,
                    ns_b,
                    ew_draw,
                    ew_height,
                    ew_r,
                    ew_g,
                    ew_b,
                    diag_draw,
                    diag_height,
                    diag_r,
                    diag_g,
                    diag_b,
                    roof_config(config, wall_roof[index]),
                )

    return region


def terrain_rgb_from_record(tile):
    colour, height, decoration = tile[:3]
    tile_r, tile_g, tile_b = tile[12:15]
    if decoration and (tile_r or tile_g or tile_b):
        return tile_r, tile_g, tile_b
    return terrain_rgb(colour, height, 0)


def world_height(region, x, y):
    x = max(0, min(WORLD_W - 1, x))
    y = max(0, min(WORLD_H - 1, y))

    for tx, ty in ((x, y), (x - 1, y), (x, y - 1), (x - 1, y - 1)):
        if 0 <= tx < WORLD_W and 0 <= ty < WORLD_H:
            tile = region[ty][tx]
            if tile[10] == 4:
                return 0

    return (region[y][x][1] - 96) * 3


def add_face(tile_faces, faces, tile_x, tile_y, vertices, colour):
    if len(vertices) not in (3, 4):
        raise ValueError("face must have 3 or 4 vertices")

    count = len(vertices)
    while len(vertices) < 4:
        vertices.append((0, 0, 0))

    if 0 <= tile_x < WORLD_W and 0 <= tile_y < WORLD_H:
        tile_faces[tile_y][tile_x].append(len(faces))

    faces.append((count, colour, tuple(vertices)))


def build_faces(region):
    tile_faces = [[[] for _ in range(WORLD_W)] for _ in range(WORLD_H)]
    faces = []

    for y in range(WORLD_H - 1):
        for x in range(WORLD_W - 1):
            tile = region[y][x]
            colour = terrain_rgb_from_record(tile)
            add_face(tile_faces, faces, x, y, [
                (x * 128, y * 128, world_height(region, x, y)),
                ((x + 1) * 128, y * 128, world_height(region, x + 1, y)),
                ((x + 1) * 128, (y + 1) * 128,
                 world_height(region, x + 1, y + 1)),
                (x * 128, (y + 1) * 128, world_height(region, x, y + 1)),
            ], colour)

            if tile[15]:
                h = tile[16]
                colour = tile[17:20]
                z1 = world_height(region, x, y)
                z2 = world_height(region, x + 1, y)
                add_face(tile_faces, faces, x, y, [
                    (x * 128, y * 128, z1),
                    ((x + 1) * 128, y * 128, z2),
                    ((x + 1) * 128, y * 128, z2 + h),
                    (x * 128, y * 128, z1 + h),
                ], colour)

            if tile[20]:
                h = tile[21]
                colour = tile[22:25]
                z1 = world_height(region, x, y)
                z2 = world_height(region, x, y + 1)
                add_face(tile_faces, faces, x, y, [
                    (x * 128, y * 128, z1),
                    (x * 128, (y + 1) * 128, z2),
                    (x * 128, (y + 1) * 128, z2 + h),
                    (x * 128, y * 128, z1 + h),
                ], colour)

            if tile[25]:
                h = tile[26]
                colour = tile[27:30]
                z1 = world_height(region, x, y)
                z2 = world_height(region, x + 1, y + 1)
                add_face(tile_faces, faces, x, y, [
                    (x * 128, y * 128, z1),
                    ((x + 1) * 128, (y + 1) * 128, z2),
                    ((x + 1) * 128, (y + 1) * 128, z2 + h),
                    (x * 128, y * 128, z1 + h),
                ], colour)

            if tile[30]:
                colour = terrain_rgb_from_record(tile)
                h = 192 + tile[30]
                add_face(tile_faces, faces, x, y, [
                    (x * 128 - 16, y * 128 - 16,
                     world_height(region, x, y) + h),
                    ((x + 1) * 128 + 16, y * 128 - 16,
                     world_height(region, x + 1, y) + h),
                    ((x + 1) * 128 + 16, (y + 1) * 128 + 16,
                     world_height(region, x + 1, y + 1) + h),
                    (x * 128 - 16, (y + 1) * 128 + 16,
                     world_height(region, x, y + 1) + h),
                ], colour)

    if len(faces) > 65535:
        raise ValueError(f"too many faces for pack: {len(faces)}")

    return faces, tile_faces


def write_pack(region, output, crop_x, crop_y, start_x, start_y):
    output.parent.mkdir(parents=True, exist_ok=True)
    cropped = [[region[crop_y + y][crop_x + x]
                for x in range(WORLD_W)] for y in range(WORLD_H)]
    faces, tile_faces = build_faces(cropped)

    with output.open("wb") as handle:
        handle.write(PACK_MAGIC)
        handle.write(struct.pack("<HHHHHH", PACK_VERSION, WORLD_W, WORLD_H,
                                 start_x, start_y, len(faces)))
        handle.write(b"\0\0\0\0")

        for y in range(WORLD_H):
            for x in range(WORLD_W):
                refs = tile_faces[y][x]
                if len(refs) > 255:
                    raise ValueError(f"too many faces on tile {x},{y}")
                start = refs[0] if refs else 0
                handle.write(bytes(cropped[y][x]))
                handle.write(struct.pack("<HB", start, len(refs)))

        for count, colour, vertices in faces:
            handle.write(struct.pack("<BBBB", count, *colour))
            for wx, wy, height in vertices:
                handle.write(struct.pack("<hhh", wx, wy, height))


def write_ui_assets(rsc_c_dir, output):
    media_c = rsc_c_dir / "src" / "gl" / "textures" / "media.c"
    sprites = rsc_c_dir / "gfx" / "sprites.png"
    if not media_c.exists() or not sprites.exists():
        raise FileNotFoundError(f"missing rsc-c media atlas under {rsc_c_dir}")

    positions = parse_media_positions(media_c.read_text())
    output.mkdir(parents=True, exist_ok=True)

    Image.new("RGB", (320, 1), (0, 0, 0)).save(output / "ui_top.bmp")

    bottom = Image.new("RGBA", (320, 15), (0, 86, 103, 255))
    bar = media_crop(rsc_c_dir, positions, 23)
    bottom.alpha_composite(bar.crop((0, 0, 320, min(15, bar.height))), (0, 0))

    for sprite_id, x in ((27, 2), (28, 63), (29, 112), (30, 154),
                         (31, 205), (32, 256)):
        label = media_crop(rsc_c_dir, positions, sprite_id)
        bottom.alpha_composite(label, (x, 4))

    bottom.convert("RGB").save(output / "ui_bottom.bmp")


def write_cover(region, output, crop_x, crop_y):
    output.parent.mkdir(parents=True, exist_ok=True)
    preview = Image.new("RGB", (WORLD_W, WORLD_H))
    for y in range(WORLD_H):
        for x in range(WORLD_W):
            colour, height, decoration, flags = region[crop_y + y][crop_x + x][:4]
            rgb = (20, 20, 20) if flags & FLAG_WALL else terrain_rgb(colour, height, decoration)
            preview.putpixel((x, y), rgb)

    cover = preview.resize((174, 174), Image.Resampling.NEAREST)
    canvas = Image.new("RGB", (174, 220), (22, 24, 20))
    canvas.paste(cover, (0, 24))
    draw = ImageDraw.Draw(canvas)
    try:
        font_big = ImageFont.truetype("/usr/share/fonts/TTF/DejaVuSans-Bold.ttf", 20)
        font_small = ImageFont.truetype("/usr/share/fonts/TTF/DejaVuSans-Bold.ttf", 14)
    except OSError:
        font_big = ImageFont.load_default()
        font_small = ImageFont.load_default()

    draw.rectangle((0, 0, 173, 28), fill=(35, 31, 22))
    draw.text((10, 4), "RuneScape", fill=(240, 215, 100), font=font_big)
    draw.rectangle((0, 194, 173, 219), fill=(35, 31, 22))
    draw.text((42, 199), "Classic", fill=(240, 215, 100), font=font_small)
    canvas.save(output)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("cache", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("--center-x", type=int, default=53)
    parser.add_argument("--center-y", type=int, default=48)
    parser.add_argument("--crop-x", type=int, default=0)
    parser.add_argument("--crop-y", type=int, default=0)
    parser.add_argument("--start-x", type=int, default=34)
    parser.add_argument("--start-y", type=int, default=60)
    parser.add_argument("--rsc-c-dir", type=pathlib.Path, default=pathlib.Path("/tmp/rsc-c"))
    args = parser.parse_args()

    config = load_game_config(args.cache, args.rsc_c_dir)
    region = build_region(args.cache, args.center_x, args.center_y, config)
    write_pack(region, args.output / "lumbridge.rsc", args.crop_x, args.crop_y, args.start_x, args.start_y)
    write_default_male_character(args.rsc_c_dir, args.output / "character_male.bmp")
    write_ui_assets(args.rsc_c_dir, args.output)
    write_cover(region, args.output / "covers" / "RuneScape Classic.bmp", args.crop_x, args.crop_y)
    write_cover(region, args.output / "covers" / "RuneScape Classic.pane.bmp", args.crop_x, args.crop_y)
    print(f"wrote {args.output}")


if __name__ == "__main__":
    main()
