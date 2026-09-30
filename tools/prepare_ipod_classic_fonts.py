#!/usr/bin/env python3
"""Prepare private Rockbox fonts from Apple's pinned Classic 2.0.4 IPSW.

Glyph outlines and real regular/bold faces come from the firmware; FreeType
rasterization is not claimed to be Apple's original text rasterizer.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import zipfile


ROOT = Path(__file__).resolve().parents[1]
IPSW_HASH = "7ef835c74b08f0bda3566001496cb764afbe0600cb1afec1145c259bc34ad7d0"
FACES = (
    ("Helvetica.ttf", "535af7ff41f5769cd61d33d53c7991b45d95327c103caa4dce42e94065a166f4",
     15, "15-Helvetica-RetailOS-Apple.fnt"),
    ("Helvetica.ttf", "535af7ff41f5769cd61d33d53c7991b45d95327c103caa4dce42e94065a166f4",
     16, "16-Helvetica-RetailOS-Apple.fnt"),
    ("Helvetica.ttf", "535af7ff41f5769cd61d33d53c7991b45d95327c103caa4dce42e94065a166f4",
     13, "13-Helvetica-RetailOS-Apple.fnt"),
    ("HelveticaBold.ttf", "ec0d7994fbab2ed50ecb20de6c664cad158a17713c3d4de8a0e3e4f56743a08c",
     19, "19-Helvetica-Bold-RetailOS-Apple.fnt"),
    ("HelveticaBold.ttf", "ec0d7994fbab2ed50ecb20de6c664cad158a17713c3d4de8a0e3e4f56743a08c",
     15, "15-Helvetica-Bold-RetailOS-Apple.fnt"),
    ("HelveticaBold.ttf", "ec0d7994fbab2ed50ecb20de6c664cad158a17713c3d4de8a0e3e4f56743a08c",
     23, "23-Helvetica-Bold-RetailOS-Apple.fnt"),
)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def verify(output):
    provenance = json.loads((output / "provenance.json").read_text())
    if provenance["ipsw_sha256"] != IPSW_HASH:
        raise ValueError("font provenance does not identify Classic 2.0.4")
    if provenance.get("glyph_metrics") != "source advances and left bearings":
        raise ValueError("Classic font spacing was not preserved")
    entries = provenance["fonts"]
    if len(entries) != len(FACES):
        raise ValueError("incomplete Classic font set")
    expected = {name: (source, sha, size) for source, sha, size, name in FACES}
    if {item["file"] for item in entries} != set(expected):
        raise ValueError("unexpected Classic font roles")
    for item in entries:
        source, sha, size = expected[item["file"]]
        if (item["source"] != "Resources/Fonts/" + source or
                item["source_sha256"] != sha or
                item["point_size"] != size or item["dpi"] != 60):
            raise ValueError("wrong source face or size: " + item["file"])
        data = (output / item["file"]).read_bytes()
        if (len(data) < 24 or data[:4] != b"RB12" or
                digest(data) != item["sha256"] or
                struct.unpack_from("<I", data, 12)[0] != 32 or
                struct.unpack_from("<I", data, 20)[0] < 64000):
            raise ValueError("corrupt Classic font: " + item["file"])
    print(f"PASS: verified all {len(FACES)} Classic font outputs and Unicode mappings")


def prepare(ipsw, output):
    if digest(ipsw.read_bytes()) != IPSW_HASH:
        raise ValueError("not the pinned official iPod35 2.0.4 IPSW")
    with zipfile.ZipFile(ipsw) as archive:
        firmware = archive.read("Firmware-35.9.0.4")
    start = firmware.find(b"IPODRESOURC") - 43
    if start < 0 or firmware[start + 54:start + 59] != b"FAT16":
        raise ValueError("missing intact resource volume")
    sector_size = struct.unpack_from("<H", firmware, start + 11)[0]
    sectors = struct.unpack_from("<H", firmware, start + 19)[0]
    sectors = sectors or struct.unpack_from("<I", firmware, start + 32)[0]
    end = start + sector_size * sectors
    if sector_size != 512 or sectors == 0 or end > len(firmware):
        raise ValueError("invalid resource volume geometry")
    ledger = []
    with tempfile.TemporaryDirectory(prefix="classic-fonts-") as work:
        work = Path(work)
        volume = work / "resources.fat"
        volume.write_bytes(firmware[start:end])
        for source_name, expected, size, name in FACES:
            source = work / source_name
            subprocess.run(["mcopy", "-o", "-i", str(volume),
                            "::/Resources/Fonts/" + source_name, str(source)],
                           check=True)
            if digest(source.read_bytes()) != expected:
                raise ValueError("font source hash mismatch: " + source_name)
            target = work / name
            subprocess.run([str(ROOT / "tools/convttf"), "-A", "-s", "32",
                            "-l", "65535", "-p", str(size), "-o",
                            str(target), str(source)], check=True,
                           stdout=subprocess.DEVNULL)
            data = target.read_bytes()
            # A broken Unicode cmap conversion emits glyph indices (only
            # ~1700 slots) instead of the source's Unicode character range.
            first, default, count = struct.unpack_from("<III", data, 12)
            if data[:4] != b"RB12" or first != 32 or count < 64000:
                raise ValueError("converter did not preserve Unicode mapping")
            ledger.append({"file": name, "sha256": digest(data),
                           "source": "Resources/Fonts/" + source_name,
                           "source_sha256": expected, "point_size": size,
                           "dpi": 60, "first_character": first,
                           "character_slots": count})
        output.mkdir(parents=True, exist_ok=True)
        for item in ledger:
            (output / item["file"]).write_bytes((work / item["file"]).read_bytes())
    (output / "provenance.json").write_text(json.dumps({
        "ipsw_sha256": IPSW_HASH,
        "source_url": "https://secure-appldnld.apple.com/iPod/SBML/osx/bundles/"
                      "061-7299.20091217.Bghyt/iPod_35.2.0.4.ipsw",
        "renderer": "Rockbox convttf / FreeType; original source outlines",
        "glyph_metrics": "source advances and left bearings",
        "fonts": ledger,
    }, indent=2) + "\n")
    verify(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ipsw", type=Path, nargs="?")
    parser.add_argument("output", type=Path, nargs="?")
    parser.add_argument("--verify", type=Path, metavar="FONT_DIRECTORY")
    args = parser.parse_args()
    if args.verify:
        if args.ipsw or args.output:
            parser.error("--verify does not accept extraction arguments")
        verify(args.verify)
    elif args.ipsw and args.output:
        prepare(args.ipsw, args.output)
    else:
        parser.error("supply IPSW OUTPUT, or --verify FONT_DIRECTORY")
