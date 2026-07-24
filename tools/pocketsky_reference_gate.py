#!/usr/bin/env python3
"""Validate Pocket Sky resource structure, counts, hashes, and references."""

from __future__ import annotations

import argparse
import math
import pathlib
import struct
import sys
import zlib


MAGIC = b"PSKYRSC1"
HEADER = struct.Struct("<8sHHII32sI8s")
STAR_RECORD = struct.Struct("<HfffffI2sffffff")
NAME_RECORD = struct.Struct("<HI")
CITY_RECORD = struct.Struct("<ffIII2s2x")


def load(path: pathlib.Path, kind: int) -> tuple[int, bytes]:
    data = path.read_bytes()
    if len(data) < HEADER.size:
        raise ValueError(f"truncated header: {path}")
    magic, actual_kind, version, count, size, _digest, crc, reserved = HEADER.unpack_from(data)
    payload = data[HEADER.size:]
    if magic != MAGIC or actual_kind != kind or version != 2 or reserved != b"\0" * 8:
        raise ValueError(f"invalid header: {path}")
    if len(payload) != size or zlib.crc32(payload) & 0xFFFFFFFF != crc:
        raise ValueError(f"invalid size/CRC: {path}")
    return count, payload


def cstring(pool: bytes, offset: int) -> str:
    if offset < 0 or offset >= len(pool):
        raise ValueError(f"bad string offset: {offset}")
    end = pool.find(b"\0", offset)
    if end < 0:
        raise ValueError(f"unterminated string at: {offset}")
    return pool[offset:end].decode("utf-8")


def validate(root: pathlib.Path) -> None:
    name_count, names = load(root / "names.psc", 2)
    if name_count != 333:
        raise ValueError(f"expected 333 names, found {name_count}")
    name_table_size = name_count * NAME_RECORD.size
    name_pool = names[name_table_size:]
    name_offsets: set[int] = set()
    for index in range(name_count):
        hr, offset = NAME_RECORD.unpack_from(names, index * NAME_RECORD.size)
        if not 1 <= hr <= 9110:
            raise ValueError(f"bad name HR: {hr}")
        cstring(name_pool, offset)
        name_offsets.add(offset)

    star_count, stars = load(root / "catalog.psc", 1)
    if star_count != 9110 or len(stars) != star_count * STAR_RECORD.size:
        raise ValueError("catalog count/size mismatch")
    hrs: set[int] = set()
    previous_mag = -100.0
    for index in range(star_count):
        record = STAR_RECORD.unpack_from(stars, index * STAR_RECORD.size)
        hr, _ra, _dec, _pmra, _pmdec, mag, name_offset, _spectral = record[:8]
        x, y, z, dx, dy, dz = record[8:]
        if hr in hrs or not 1 <= hr <= 9110:
            raise ValueError(f"duplicate/bad star HR: {hr}")
        if mag < previous_mag:
            raise ValueError("catalog is not sorted by magnitude")
        if name_offset != 0xFFFFFFFF and name_offset not in name_offsets:
            raise ValueError(f"star HR {hr} has invalid name offset {name_offset}")
        length = math.sqrt(x * x + y * y + z * z)
        if not 0.999999 <= length <= 1.000001:
            raise ValueError(f"star HR {hr} has invalid unit vector")
        if not all(math.isfinite(value) for value in (dx, dy, dz)):
            raise ValueError(f"star HR {hr} has invalid proper-motion vector")
        previous_mag = mag
        hrs.add(hr)
    if len(hrs) != 9110:
        raise ValueError("catalog HR set is incomplete")

    constellation_count, constellations = load(root / "constellations.psc", 3)
    if constellation_count != 88:
        raise ValueError(f"expected 88 constellations, found {constellation_count}")
    offset = 0
    for _index in range(constellation_count):
        if offset + 8 > len(constellations):
            raise ValueError("truncated constellation header")
        abbreviation = constellations[offset:offset + 3].decode("ascii")
        name_length, reserved, segments = struct.unpack_from(
            "<BBH", constellations, offset + 4
        )
        if reserved != 0:
            raise ValueError(f"invalid constellation flags: {abbreviation}")
        offset += 8
        if offset + name_length > len(constellations):
            raise ValueError(f"truncated constellation name: {abbreviation}")
        constellations[offset:offset + name_length].decode("utf-8")
        offset += name_length
        for _segment in range(segments * 2):
            if offset + 2 > len(constellations):
                raise ValueError(f"truncated constellation {abbreviation}")
            hr = struct.unpack_from("<H", constellations, offset)[0]
            offset += 2
            if hr not in hrs:
                raise ValueError(f"constellation {abbreviation} references HR {hr}")
    if offset != len(constellations):
        raise ValueError("trailing constellation data")

    city_count, cities = load(root / "cities.psc", 4)
    table_size = city_count * CITY_RECORD.size
    if city_count < 2900 or table_size > len(cities):
        raise ValueError("city count/size mismatch")
    city_pool = cities[table_size:]
    moncton_count = 0
    for index in range(city_count):
        lat, lon, _population, name_offset, tz_offset, country = CITY_RECORD.unpack_from(
            cities, index * CITY_RECORD.size
        )
        if not -90 <= lat <= 90 or not -180 <= lon <= 180:
            raise ValueError(f"invalid city coordinates at index {index}")
        name = cstring(city_pool, name_offset)
        timezone = cstring(city_pool, tz_offset)
        country.decode("ascii")
        if name == "Moncton":
            moncton_count += 1
            if (
                abs(lat - 46.09454) > 0.00001
                or abs(lon - (-64.7965)) > 0.00001
                or timezone != "America/Moncton"
                or country != b"CA"
            ):
                raise ValueError("pinned Moncton city record is invalid")
    if moncton_count != 1:
        raise ValueError(f"expected one Moncton city record, found {moncton_count}")

    print(
        f"Pocket Sky resources OK: {star_count} stars, {name_count} names, "
        f"{constellation_count} constellations, {city_count} cities"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("resource_dir", type=pathlib.Path)
    args = parser.parse_args()
    validate(args.resource_dir)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, UnicodeError, ValueError, struct.error) as error:
        print(f"pocketsky_reference_gate: {error}", file=sys.stderr)
        raise SystemExit(1)
