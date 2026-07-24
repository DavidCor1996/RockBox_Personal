#!/usr/bin/env python3
"""Read-only extractor for bitmap resources in official click-wheel iPod IPSWs.

The 5G OS image is not encrypted.  Its ``paMB`` resources use the format
documented by the GPL-licensed iPodWizard source.  This tool deliberately has
no firmware-writing path: it only emits PNG files and a provenance manifest.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import struct
import zipfile
from dataclasses import dataclass
from pathlib import Path

from PIL import Image


IPOD_5G_13_SHA256 = (
    "66aad071f960061dcfbdfe69773a698a59b9635c18ba9cb4478f57fd69306cb7"
)
PARTITION_TABLE_OFFSET = 0x4200
PARTITION_ENTRY = struct.Struct("<4sI4sIIIIIII")
RESOURCE_HEADER = struct.Struct("<III")
RESOURCE_SECTION = struct.Struct("<4sIII")
RESOURCE_ENTRY = struct.Struct("<hHII")
PICTURE_HEADER = struct.Struct("<8sIIHHHHI")
PICTURE_HEADER_5G = struct.Struct("<HHHH8sIII")


@dataclass(frozen=True)
class Picture:
    resource_id: int
    width: int
    height: int
    row_bytes: int
    bit_depth: int
    texture_format: int
    pixels: bytes
    source_offset: int
    source_size: int


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_official_5g_osos(ipsw: Path) -> tuple[bytes, str, str]:
    archive_bytes = ipsw.read_bytes()
    archive_hash = sha256(archive_bytes)
    if archive_hash != IPOD_5G_13_SHA256:
        raise SystemExit(
            "refusing unrecognized IPSW: expected Apple iPod_13.1.3.ipsw "
            f"sha256={IPOD_5G_13_SHA256}, got {archive_hash}"
        )

    with zipfile.ZipFile(io.BytesIO(archive_bytes)) as archive:
        firmware_names = [
            name for name in archive.namelist() if name.startswith("Firmware-")
        ]
        if firmware_names != ["Firmware-13.6.3"]:
            raise SystemExit(f"unexpected firmware payloads: {firmware_names!r}")
        firmware = archive.read(firmware_names[0])

    for index in range(20):
        offset = PARTITION_TABLE_OFFSET + index * PARTITION_ENTRY.size
        fields = PARTITION_ENTRY.unpack_from(firmware, offset)
        magic, image_id_value, _padding = fields[:3]
        device_offset, length = fields[3], fields[4]
        if magic != b"!ATA":
            break
        if image_id_value == int.from_bytes(b"soso", "little"):
            # Firmware format 3 excludes the first 512-byte sector at the
            # partition's device offset from the image payload/checksum.
            start = device_offset + 512
            end = start + length
            if end > len(firmware):
                raise SystemExit("OS image extends beyond firmware payload")
            return firmware[start:end], archive_hash, firmware_names[0]

    raise SystemExit("official 5G OS image (soso) was not found")


def decode_picture(
    os_image: bytes, offset: int, size: int, resource_id: int
) -> Picture | None:
    if offset < 0 or size < PICTURE_HEADER.size or offset + size > len(os_image):
        return None

    header = os_image[offset : offset + PICTURE_HEADER.size]
    zeroes, height, width, row_bytes, bit_depth, _unknown, texture, block_len = (
        PICTURE_HEADER.unpack(header)
    )
    if zeroes != b"\0" * 8:
        texture, _unknown, row_bytes, bit_depth, zeroes, height, width, block_len = (
            PICTURE_HEADER_5G.unpack(header)
        )
        if texture == 0 or zeroes != b"\0" * 8:
            return None

    if not (0 < width <= 511 and 0 < height <= 511):
        return None
    if bit_depth not in (1, 2, 4, 16):
        return None
    minimum_row = (width * bit_depth + 7) // 8
    if row_bytes < minimum_row or row_bytes * height > block_len:
        return None
    if PICTURE_HEADER.size + block_len > size:
        return None

    pixel_start = offset + PICTURE_HEADER.size
    pixels = os_image[pixel_start : pixel_start + row_bytes * height]
    return Picture(
        resource_id=resource_id,
        width=width,
        height=height,
        row_bytes=row_bytes,
        bit_depth=bit_depth,
        texture_format=texture,
        pixels=pixels,
        source_offset=offset,
        source_size=size,
    )


def iter_pictures(os_image: bytes):
    known_types = {
        b" rtS", b"paMB", b"uneM", b"epyT", b"weiV", b"dmCT",
        b"enoN", b"mTDL", b"stiB", b"boot",
    }
    seen: set[tuple[int, int, int, str]] = set()

    for block_offset in range(0, len(os_image) - RESOURCE_HEADER.size, 4):
        version, length, section_count = RESOURCE_HEADER.unpack_from(
            os_image, block_offset
        )
        first_type = os_image[
            block_offset + RESOURCE_HEADER.size :
            block_offset + RESOURCE_HEADER.size + 4
        ]
        if (
            version != 3
            or first_type not in known_types
            or not (0 < length < 0xFFFF)
            or not (0 < section_count < 50)
        ):
            continue

        section_table = block_offset + RESOURCE_HEADER.size
        sections = []
        total_items = 0
        valid = True
        for index in range(section_count):
            section_offset = section_table + index * RESOURCE_SECTION.size
            if section_offset + RESOURCE_SECTION.size > len(os_image):
                valid = False
                break
            section = RESOURCE_SECTION.unpack_from(os_image, section_offset)
            if section[0] not in known_types or section[1] > 10000:
                valid = False
                break
            sections.append(section)
            total_items += section[1]
        if not valid or total_items > 20000:
            continue

        data_start = (
            section_table
            + section_count * RESOURCE_SECTION.size
            + total_items * RESOURCE_ENTRY.size
        )
        if data_start > len(os_image):
            continue

        for section_type, item_count, _unknown, table_relative in sections:
            if section_type != b"paMB":
                continue
            table_start = block_offset + table_relative
            table_end = table_start + item_count * RESOURCE_ENTRY.size
            if table_start < block_offset or table_end > len(os_image):
                continue
            for item in range(item_count):
                entry_offset = table_start + item * RESOURCE_ENTRY.size
                resource_id, _unknown, data_relative, size = (
                    RESOURCE_ENTRY.unpack_from(os_image, entry_offset)
                )
                picture_offset = data_start + data_relative
                picture = decode_picture(
                    os_image, picture_offset, size, resource_id
                )
                if picture is None:
                    continue
                key = (
                    picture.resource_id,
                    picture.width,
                    picture.height,
                    sha256(picture.pixels),
                )
                if key in seen:
                    continue
                seen.add(key)
                yield picture


def picture_to_image(picture: Picture) -> Image.Image:
    has_alpha = picture.bit_depth == 16 and picture.texture_format == 0x1444
    mode = "RGBA" if has_alpha else "RGB"
    output = bytearray(picture.width * picture.height * len(mode))
    write = 0

    for y in range(picture.height):
        row = picture.pixels[y * picture.row_bytes : (y + 1) * picture.row_bytes]
        for x in range(picture.width):
            if picture.bit_depth in (1, 2, 4):
                shift = 8 - picture.bit_depth - (x * picture.bit_depth) % 8
                mask = (1 << picture.bit_depth) - 1
                value = (row[(x * picture.bit_depth) // 8] >> shift) & mask
                grey = 255 - value * 255 // mask
                rgba = (grey, grey, grey, 255)
            else:
                first = x * 2
                if picture.texture_format == 0x1444:
                    value = row[first] | row[first + 1] << 8
                    rgba = (
                        ((value >> 8) & 0xF) * 17,
                        ((value >> 4) & 0xF) * 17,
                        (value & 0xF) * 17,
                        ((value >> 12) & 0xF) * 17,
                    )
                else:
                    if picture.texture_format == 0x2565:
                        high, low = row[first], row[first + 1]
                    else:
                        high, low = row[first + 1], row[first]
                    rgba = (
                        ((high >> 3) & 0x1F) * 255 // 31,
                        (((high & 0x07) << 3) | (low >> 5)) * 255 // 63,
                        (low & 0x1F) * 255 // 31,
                        255,
                    )
            output[write : write + len(mode)] = rgba[: len(mode)]
            write += len(mode)

    return Image.frombytes(mode, (picture.width, picture.height), bytes(output))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ipsw", type=Path, help="official Apple iPod_13.1.3.ipsw")
    parser.add_argument("output", type=Path, help="directory for PNGs and manifest")
    args = parser.parse_args()

    os_image, archive_hash, payload_name = read_official_5g_osos(args.ipsw)
    args.output.mkdir(parents=True, exist_ok=True)
    pictures = sorted(
        iter_pictures(os_image),
        key=lambda item: (item.resource_id, item.source_offset),
    )

    manifest_path = args.output / "manifest.tsv"
    with manifest_path.open("w", encoding="utf-8", newline="") as manifest:
        writer = csv.writer(manifest, delimiter="\t")
        writer.writerow(
            (
                "resource_id", "width", "height", "bit_depth",
                "texture_format", "source_offset", "source_size",
                "pixel_sha256", "file",
            )
        )
        for index, picture in enumerate(pictures):
            filename = (
                f"{index:04d}-id{picture.resource_id}-"
                f"{picture.width}x{picture.height}.png"
            )
            picture_to_image(picture).save(args.output / filename, optimize=False)
            writer.writerow(
                (
                    picture.resource_id,
                    picture.width,
                    picture.height,
                    picture.bit_depth,
                    f"0x{picture.texture_format:04x}",
                    f"0x{picture.source_offset:x}",
                    picture.source_size,
                    sha256(picture.pixels),
                    filename,
                )
            )

    provenance = args.output / "PROVENANCE.txt"
    provenance.write_text(
        "\n".join(
            (
                "Source: official Apple iPod_13.1.3.ipsw",
                f"IPSW SHA-256: {archive_hash}",
                f"Payload: {payload_name}",
                f"Payload OS-image SHA-256: {sha256(os_image)}",
                "Method: read-only paMB extraction; no pixels synthesized",
                f"Images: {len(pictures)}",
                "",
            )
        ),
        encoding="utf-8",
    )
    print(f"extracted {len(pictures)} Apple bitmap resources to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
