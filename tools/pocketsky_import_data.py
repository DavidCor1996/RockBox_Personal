#!/usr/bin/env python3
"""Build Pocket Sky resources from an unmodified Astroterm data checkout.

The Yale BSC5 terms allow redistribution of the original catalogue but not a
converted catalogue.  Therefore this tool intentionally requires the user to
provide their own canonical BSC5 file and writes generated .psc files locally.
Generated catalogue resources must not be committed or redistributed without
separate permission from the catalogue copyright holders.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import math
import pathlib
import re
import struct
import sys
import zlib


MAGIC = b"PSKYRSC1"
FORMAT_VERSION = 2
HEADER = struct.Struct("<8sHHII32sI8s")

KIND_CATALOG = 1
KIND_NAMES = 2
KIND_CONSTELLATIONS = 3
KIND_CITIES = 4

BSC5_SIZE = 291_548
BSC5_SHA256 = {
    "e471d02eaf4eecb61c12f879a1cb6432ba9d7b68a9a8c5654a1eb42a0c8cc340",
}

ASTROTERM_FILES = {
    "bsc5_names.txt":
        "9f40a19bc14d1182b3c1f88a5565174b733d14a2a9ea48b7b20712db92dab18d",
    "bsc5_constellations.txt":
        "4173e71b1641d5f9388677b47313434a2a50071921e4111e5de308d994e9bed5",
    "cities.csv":
        "7031b07c06ccc6a07109ff37a4ca535c08bd619c6be95d8d7a764845ea215971",
}
GEONAMES_MONCTON_SHA256 = (
    "78b64d9fbbb0847966a1320add75f890dfa7ab1718b3b9f9e87211b2e4ce5332"
)

BSC_HEADER = struct.Struct("<7i")
BSC_ENTRY = struct.Struct("<fdd2shff")
STAR_RECORD = struct.Struct("<HfffffI2sffffff")
NAME_RECORD = struct.Struct("<HI")
CITY_RECORD = struct.Struct("<ffIII2s2x")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def checked_read(path: pathlib.Path, expected: str | None = None) -> bytes:
    data = path.read_bytes()
    actual = sha256(data)
    if expected is not None and actual != expected:
        raise ValueError(
            f"SHA-256 mismatch for {path}: expected {expected}, got {actual}"
        )
    return data


def write_resource(
    path: pathlib.Path,
    kind: int,
    count: int,
    source_hash: str,
    payload: bytes,
) -> None:
    header = HEADER.pack(
        MAGIC,
        kind,
        FORMAT_VERSION,
        count,
        len(payload),
        bytes.fromhex(source_hash),
        zlib.crc32(payload) & 0xFFFFFFFF,
        b"\0" * 8,
    )
    path.write_bytes(header + payload)


def load_names(path: pathlib.Path) -> tuple[dict[int, tuple[int, str]], bytes]:
    source = checked_read(path, ASTROTERM_FILES[path.name])
    pool = bytearray()
    names: dict[int, tuple[int, str]] = {}

    for raw in source.decode("utf-8").splitlines():
        if not raw:
            continue
        hr_text, name = raw.split(",", 1)
        hr = int(hr_text)
        if hr < 1 or hr > 9110 or hr in names:
            raise ValueError(f"invalid or duplicate named-star HR number: {hr}")
        encoded = name.encode("utf-8") + b"\0"
        names[hr] = (len(pool), name)
        pool.extend(encoded)

    table = bytearray()
    for hr in sorted(names):
        table.extend(NAME_RECORD.pack(hr, names[hr][0]))
    return names, bytes(table + pool)


def build_catalog(path: pathlib.Path, names: dict[int, tuple[int, str]]) -> bytes:
    data = checked_read(path)
    digest = sha256(data)
    if len(data) != BSC5_SIZE or digest not in BSC5_SHA256:
        raise ValueError(
            "BSC5 must be the canonical 291548-byte binary with SHA-256 "
            + ", ".join(sorted(BSC5_SHA256))
        )

    _star0, _star1, starn, _stnum, _mprop, _nmag, nbent = BSC_HEADER.unpack_from(data)
    count = abs(starn)
    if count != 9110 or nbent != 32:
        raise ValueError(f"unexpected BSC5 header: count={count}, entry={nbent}")

    stars: list[tuple[float, bytes]] = []
    offset = BSC_HEADER.size
    for index in range(count):
        number, ra, dec, spectral, mag100, pmra, pmdec = BSC_ENTRY.unpack_from(
            data, offset + index * BSC_ENTRY.size
        )
        hr = int(number)
        if hr != index + 1:
            raise ValueError(f"unexpected BSC5 HR sequence at {index}: {number}")
        magnitude = mag100 / 100.0
        name_offset = names.get(hr, (0xFFFFFFFF, ""))[0]
        sin_ra = math.sin(ra)
        cos_ra = math.cos(ra)
        sin_dec = math.sin(dec)
        cos_dec = math.cos(dec)
        x = cos_dec * cos_ra
        y = cos_dec * sin_ra
        z = sin_dec
        dx = -sin_dec * pmdec * cos_ra - cos_dec * sin_ra * pmra
        dy = -sin_dec * pmdec * sin_ra + cos_dec * cos_ra * pmra
        dz = cos_dec * pmdec
        record = STAR_RECORD.pack(
            hr,
            float(ra),
            float(dec),
            float(pmra),
            float(pmdec),
            float(magnitude),
            name_offset,
            spectral,
            float(x),
            float(y),
            float(z),
            float(dx),
            float(dy),
            float(dz),
        )
        stars.append((magnitude, record))

    stars.sort(key=lambda item: (item[0], item[1][:2]))
    return b"".join(record for _magnitude, record in stars)


def astronomy_constellation_names(path: pathlib.Path) -> dict[str, str]:
    source = path.read_text(encoding="utf-8")
    block_match = re.search(
        r"static const constel_info_t ConstelInfo\[\] = \{(.*?)\n\};",
        source,
        re.DOTALL,
    )
    if block_match is None:
        raise ValueError(f"cannot find Astronomy Engine ConstelInfo in {path}")
    names = dict(
        re.findall(r'\{\s*"([A-Za-z]{3})",\s*"([^"]+)"\s*\}', block_match.group(1))
    )
    if len(names) != 88:
        raise ValueError(f"expected 88 Astronomy Engine constellation names, found {len(names)}")
    return names


def build_constellations(
    path: pathlib.Path, full_names: dict[str, str]
) -> tuple[int, bytes]:
    source = checked_read(path, ASTROTERM_FILES[path.name])
    payload = bytearray()
    count = 0
    for raw in source.decode("ascii").splitlines():
        fields = raw.split()
        if not fields:
            continue
        abbreviation = fields[0].encode("ascii")
        segments = int(fields[1])
        numbers = [int(value) for value in fields[2:]]
        if len(abbreviation) != 3 or len(numbers) != 2 * segments:
            raise ValueError(f"invalid constellation row: {raw}")
        if any(number < 1 or number > 9110 for number in numbers):
            raise ValueError(f"out-of-range constellation HR reference: {raw}")
        full_name = full_names[fields[0]].encode("utf-8")
        if len(full_name) > 255:
            raise ValueError(f"constellation name is too long: {fields[0]}")
        payload.extend(abbreviation + b"\0")
        payload.extend(struct.pack("<BBH", len(full_name), 0, segments))
        payload.extend(full_name)
        payload.extend(struct.pack(f"<{len(numbers)}H", *numbers))
        count += 1
    if count != 88:
        raise ValueError(f"expected 88 constellations, found {count}")
    return count, bytes(payload)


def load_default_city(path: pathlib.Path) -> dict[str, str]:
    source = checked_read(path, GEONAMES_MONCTON_SHA256)
    fields = source.decode("utf-8").rstrip("\n").split("\t")
    if (
        len(fields) != 19
        or fields[0] != "6076211"
        or fields[1] != "Moncton"
        or fields[6:9] != ["P", "PPL", "CA"]
        or fields[17] != "America/Moncton"
    ):
        raise ValueError(f"invalid pinned GeoNames Moncton record: {path}")
    return {
        "city_name": fields[1],
        "population": fields[14],
        "country_code": fields[8],
        "timezone": fields[17],
        "latitude": fields[4],
        "longitude": fields[5],
    }


def build_cities(
    path: pathlib.Path, default_city_path: pathlib.Path
) -> tuple[int, bytes, str]:
    source = checked_read(path, ASTROTERM_FILES[path.name])
    rows = list(csv.DictReader(source.decode("utf-8").splitlines()))
    if not any(row["city_name"].strip() == "Moncton" for row in rows):
        rows.append(load_default_city(default_city_path))
        rows.sort(key=lambda row: row["city_name"].casefold())
    strings = bytearray()
    offsets: dict[str, int] = {}

    def add_string(text: str) -> int:
        if text not in offsets:
            offsets[text] = len(strings)
            strings.extend(text.encode("utf-8") + b"\0")
        return offsets[text]

    table = bytearray()
    for row in rows:
        name = row["city_name"].strip()
        timezone = row["timezone"].strip()
        country = row["country_code"].strip().upper().encode("ascii")
        if not name or len(country) != 2:
            raise ValueError(f"invalid city row: {row}")
        table.extend(
            CITY_RECORD.pack(
                float(row["latitude"]),
                float(row["longitude"]),
                int(row["population"]),
                add_string(name),
                add_string(timezone),
                country,
            )
        )
    combined_hash = sha256(source + checked_read(
        default_city_path, GEONAMES_MONCTON_SHA256
    ))
    return len(rows), bytes(table + strings), combined_hash


def provenance_text(
    bsc5: pathlib.Path,
    astroterm_data: pathlib.Path,
    default_city_path: pathlib.Path,
    outputs: list[pathlib.Path],
) -> str:
    lines = [
        "Pocket Sky generated resource provenance",
        "format=PSKYRSC1",
        "astroterm_version=v1.2.0",
        "astroterm_commit=5c571959dbd7ceca95b964e9a92154192bc76a09",
        "astroterm_url=https://github.com/da-luce/astroterm",
        "astronomy_engine_commit=865d3da7d8112bbc7911238052c6af4aaf877181",
        "astronomy_engine_url=https://github.com/cosinekitty/astronomy",
        f"bsc5_path={bsc5}",
        f"bsc5_sha256={sha256(bsc5.read_bytes())}",
        "bsc5_notice=Hoffleit E.D. and Warren Jr. W.H.; ADC/NSSDC 1991",
        "bsc5_distribution=generated resource is for local use; do not redistribute without permission",
        "cities_source=GeoNames via Astroterm plus pinned GeoNames Canada Moncton record; CC BY 3.0; https://download.geonames.org/export/dump/",
        "geonames_moncton_id=6076211",
        "geonames_moncton_snapshot=CA.zip downloaded 2026-07-14",
        "geonames_ca_zip_sha256=3917a464b2ee0557a86858f117155e57419190734b9e11835b6f06ce3bd16960",
        f"input_geonames_moncton.tsv_sha256={sha256(default_city_path.read_bytes())}",
        "constellations_source=Stellarium modern sky culture via Astroterm/HYG mapping",
        "star_names_source=IAU Working Group on Star Names via Astroterm",
    ]
    for filename, expected in ASTROTERM_FILES.items():
        path = astroterm_data / filename
        lines.append(f"input_{filename}_sha256={sha256(path.read_bytes())}")
        if sha256(path.read_bytes()) != expected:
            raise ValueError(f"unexpected source hash while writing provenance: {path}")
    for path in outputs:
        lines.append(f"output_{path.name}_sha256={sha256(path.read_bytes())}")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bsc5", required=True, type=pathlib.Path)
    parser.add_argument(
        "--astroterm-data",
        required=True,
        type=pathlib.Path,
        help="Astroterm v1.2.0 data directory",
    )
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument(
        "--astronomy-c",
        required=True,
        type=pathlib.Path,
        help="Pinned Astronomy Engine C implementation",
    )
    args = parser.parse_args()

    args.output.mkdir(parents=True, exist_ok=True)
    names_path = args.astroterm_data / "bsc5_names.txt"
    constellations_path = args.astroterm_data / "bsc5_constellations.txt"
    cities_path = args.astroterm_data / "cities.csv"
    default_city_path = (
        pathlib.Path(__file__).resolve().parents[1]
        / "apps/plugins/pocketsky/data/geonames_moncton.tsv"
    )

    names, names_payload = load_names(names_path)
    catalog_payload = build_catalog(args.bsc5, names)
    constellation_count, constellation_payload = build_constellations(
        constellations_path, astronomy_constellation_names(args.astronomy_c)
    )
    city_count, city_payload, city_source_hash = build_cities(
        cities_path, default_city_path
    )

    bsc_hash = sha256(args.bsc5.read_bytes())
    outputs = [
        args.output / "catalog.psc",
        args.output / "names.psc",
        args.output / "constellations.psc",
        args.output / "cities.psc",
    ]
    write_resource(outputs[0], KIND_CATALOG, 9110, bsc_hash, catalog_payload)
    write_resource(
        outputs[1], KIND_NAMES, len(names), ASTROTERM_FILES[names_path.name], names_payload
    )
    write_resource(
        outputs[2],
        KIND_CONSTELLATIONS,
        constellation_count,
        ASTROTERM_FILES[constellations_path.name],
        constellation_payload,
    )
    write_resource(
        outputs[3], KIND_CITIES, city_count, city_source_hash, city_payload
    )
    (args.output / "PROVENANCE.txt").write_text(
        provenance_text(
            args.bsc5, args.astroterm_data, default_city_path, outputs
        ),
        encoding="utf-8",
    )

    for path in outputs:
        print(f"{path}: {path.stat().st_size} bytes {sha256(path.read_bytes())}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, UnicodeError, ValueError, struct.error) as error:
        print(f"pocketsky_import_data: {error}", file=sys.stderr)
        raise SystemExit(1)
