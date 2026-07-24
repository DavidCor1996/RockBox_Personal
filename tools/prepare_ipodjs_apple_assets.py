#!/usr/bin/env python3
"""Prepare private, pixel-authentic Apple assets used by iPodJS.

Apple binaries are intentionally not stored in this repository.  This tool
accepts two official Apple downloads, verifies their exact hashes, and emits
only direct resource extractions or format conversions.  It never redraws,
traces, interpolates, or resamples artwork.
"""

from __future__ import annotations

import argparse
import hashlib
import shutil
import struct
import subprocess
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IPSW_SHA256 = (
    "66aad071f960061dcfbdfe69773a698a59b9635c18ba9cb4478f57fd69306cb7"
)
CLASSIC_IPSW_SHA256 = (
    "e753abfb11aaeaa6fd1d7257e87f4e53b6b5d923b1de0e4c9c63c30e0dac9d1a"
)
GUIDE_SHA256 = (
    "b5b8dca3c526c3eaa80507818de5541611d35e5711dc7f5fd7186c8439227d2a"
)
FIRMWARE_NAME = "Firmware-13.6.3"
CLASSIC_FIRMWARE_NAME = "Firmware-24.9.1.2"
PARTITION_TABLE_OFFSET = 0x4200
PARTITION_ENTRY = struct.Struct("<4sI4sIIIIIII")
BATTERY_HASHES = (
    "0f9dd6d4393138155768b4359a807bd68f2d8dd89e90d35143324a03a8e54ddb",
    "bb0b4180e1b100b99abf3c848f41e50e23fdcb897a72a9025653c9e713f0497b",
    "a233999c8ea8148e842a53e9208c4347c305eca25f9418797630db01c40f787a",
    "1fa74741276a703b681edccd1ee702b297165177094a48f998ba2c313346cb92",
    "18430e107cf04769089094f3ba2fd3dbd7ff540003b861b276792024bd6f3562",
)
OVERLAY_HASHES = {
    22203: "1c751d51e88357c52f27655c4a27ac901f30a1adcab073215c51e70a232bc37c",
    22206: "1c41d4b079e3272d2e1532fc5f093c790ee5f657309736cd811e93d9c68f739f",
    24379: "6fedad9b605d480b435613020420daae398bca7406eaba278a5f63505b6216e3",
    24413: "c15cdf4c0e3e2e7db82b016b7e699996a2c9d9e53fb727c5a4cdd791a3ebab05",
    24414: "7d1a113de5f13956874a4808439caa432be3746838ec88ee953806181fdb34dd",
    24263: "ed243453ea34777592517731d73f147699f41c482b58b74971aa5a3221da0b17",
    24265: "650020128e410879247b189c17400f7aa1adff8d7b431886eebbd69a365c30a0",
    24266: "37db0fa42bd6712cd2a71c27e23e42ff2a72c2edbc04a922e195693bac331f6b",
    24267: "32e4e35865666e777c96a8e6260df21cee7d8484c5c265876f37894bc7abc6db",
    24282: "6ea8f618062cba96ba4ee757aa2361e12b7e1d1acf9768b91f267c43f559e7f3",
    24283: "3ee47e4750849f873f1b96f8002c16ae92359456ac4e95bb8d104eab76f4658a",
    24286: "fbb5f7f5750fea7e2c27859b9e0ec17c1959fb378d541a0a55284e4c5de57417",
    24287: "02fac093e4edb3b3d449b3cadaff5c2d4b361783a0ad056e21fccf845f1b99b9",
    24292: "42e967baa962782f50599ad1ee268611943da7c3c9ca9f6bffa6ab6498666edc",
    24293: "ac3f2281ab1922d0039af5e15cd2dc1b12604c1360a793f0cf9c60cbd84ca67c",
    24343: "b91036f651144d4f6cd6ef6caaf77ec204f8702855d6c7dc18aa54dec13bb0e3",
    25552: "908f95b2eca45538a85b94bb37dea1c144827ff91331041eea99671f7f50bf71",
    25554: "40e4b77ef73b56ecf712755f4f9a5c47698faeb17ac5cc603d16df667b1ac0b4",
    30239: "b1057f00f0be252ff60e8f55b92f37f636cf6841538c10a134e8cf5bf6a91f3c",
    30240: "86d6f01dc6c65568e3ee1738ed93c613c21d5ffa320fcbcacfb8f5247130a4ec",
    30242: "5e11acc45c4974cc84a226eda38413181e991d60975736244c16e593c1c31b6f",
    30244: "fb01a436a0c1dcd380294df5523d750f0b6bb98ea558be22a01d80e331b3d96e",
    30248: "162bca32ab535fbf17764cb2ac4819e37bf22c6b6ece571831b1c9917f68c0d2",
}
FONT_HASH = "80c98ba888273c71b94abbe31420a0764bb27136938f36d2e3c1295b17515fcd"
FONT17_HASH = (
    "b5a8a86c19dbf93a86999767c944481382de44aba3105082bf034a93d0c825ed"
)
CLASSIC_FONT_HASH = (
    "535af7ff41f5769cd61d33d53c7991b45d95327c103caa4dce42e94065a166f4"
)
CLASSIC_CJK16_HASH = (
    "628982f2bfa9c33c6b21d65e8c8370655acd3dbb03b60cf0ac96c3203f0b404a"
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def require_hash(path: Path, expected: str, label: str) -> None:
    actual = sha256(path)
    if actual != expected:
        raise SystemExit(
            f"refusing unrecognized {label}: expected sha256={expected}, "
            f"got {actual}"
        )


def require_tool(name: str) -> str:
    path = shutil.which(name)
    if path is None:
        raise SystemExit(f"required tool is missing: {name}")
    return path


def pillow_python() -> Path:
    candidates = (ROOT / "rockpod/.venv/bin/python", Path(require_tool("python3")))
    for candidate in candidates:
        result = subprocess.run(
            [str(candidate), "-c", "import PIL"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        if result.returncode == 0:
            return candidate
    raise SystemExit("Pillow is required by ipod_stock_resource_extract.py")


def run(*args: str | Path) -> None:
    subprocess.run([str(arg) for arg in args], check=True)


def extract_resource_partition(ipsw: Path, destination: Path) -> None:
    with zipfile.ZipFile(ipsw) as archive:
        names = [name for name in archive.namelist() if name.startswith("Firmware-")]
        if names != [FIRMWARE_NAME]:
            raise SystemExit(f"unexpected firmware payloads: {names!r}")
        firmware = archive.read(FIRMWARE_NAME)

    resource_id = int.from_bytes(b"crsr", "little")
    for index in range(20):
        fields = PARTITION_ENTRY.unpack_from(
            firmware, PARTITION_TABLE_OFFSET + index * PARTITION_ENTRY.size
        )
        magic, image_id = fields[:2]
        device_offset, length = fields[3], fields[4]
        if magic != b"!ATA":
            break
        if image_id == resource_id:
            start = device_offset + 512
            end = start + length
            if end > len(firmware):
                raise SystemExit("resource partition extends beyond firmware")
            destination.write_bytes(firmware[start:end])
            return
    raise SystemExit("official Apple resource partition was not found")


def extract_classic_resource_partition(ipsw: Path, destination: Path) -> None:
    """Extract the intact FAT16 iPodResources volume from the Classic IPSW."""
    with zipfile.ZipFile(ipsw) as archive:
        names = [name for name in archive.namelist() if name.startswith("Firmware-")]
        if names != [CLASSIC_FIRMWARE_NAME]:
            raise SystemExit(f"unexpected Classic firmware payloads: {names!r}")
        firmware = archive.read(CLASSIC_FIRMWARE_NAME)

    label_at = firmware.find(b"IPODRESOURC")
    start = label_at - 0x2B
    if label_at < 0 or start < 0 or firmware[start + 54:start + 59] != b"FAT16":
        raise SystemExit("official Classic iPodResources FAT16 volume was not found")
    bytes_per_sector = int.from_bytes(firmware[start + 11:start + 13], "little")
    sectors = int.from_bytes(firmware[start + 19:start + 21], "little")
    if sectors == 0:
        sectors = int.from_bytes(firmware[start + 32:start + 36], "little")
    length = bytes_per_sector * sectors
    if bytes_per_sector != 512 or length <= 0 or start + length > len(firmware):
        raise SystemExit("invalid Classic iPodResources FAT16 geometry")
    destination.write_bytes(firmware[start:start + length])


def find_resource(images: Path, resource_id: int, width: int, height: int) -> Path:
    matches = list(images.glob(f"*-id{resource_id}-{width}x{height}.png"))
    if len(matches) != 1:
        raise SystemExit(
            f"expected one Apple resource id={resource_id} {width}x{height}, "
            f"found {len(matches)}"
        )
    require_hash(matches[0], OVERLAY_HASHES[resource_id], "Apple bitmap")
    return matches[0]


def build_battery_strip(guide: Path, temporary: Path, output: Path) -> None:
    prefix = temporary / "battery"
    run(require_tool("pdfimages"), "-f", "16", "-l", "16", "-png",
        guide, prefix)
    frames = [temporary / f"battery-{index:03d}.png" for index in range(5)]
    if any(not frame.is_file() for frame in frames):
        raise SystemExit("the five documented iPod classic battery images were not found")
    for frame, expected in zip(frames, BATTERY_HASHES):
        require_hash(frame, expected, "Apple guide battery image")

    run(require_tool("magick"), *frames, "-append", f"BMP3:{output}")
    identify = subprocess.run(
        [require_tool("identify"), "-format", "%wx%h", str(output)],
        check=True, capture_output=True, text=True,
    )
    if identify.stdout != "26x65":
        raise SystemExit(f"unexpected Apple battery strip size: {identify.stdout}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ipsw", required=True, type=Path,
                        help="official Apple iPod_13.1.3.ipsw")
    parser.add_argument("--classic-ipsw", required=True, type=Path,
                        help="official Apple iPod_24.1.1.2.ipsw")
    parser.add_argument("--guide", required=True, type=Path,
                        help="official iPod classic 120GB User Guide PDF")
    parser.add_argument(
        "--output", type=Path, default=ROOT / "assets/ipodjs/apple",
        help="private output directory (default: assets/ipodjs/apple)",
    )
    args = parser.parse_args()

    require_hash(args.ipsw, IPSW_SHA256, "Apple IPSW")
    require_hash(args.classic_ipsw, CLASSIC_IPSW_SHA256,
                 "Apple iPod classic IPSW")
    require_hash(args.guide, GUIDE_SHA256, "Apple user guide")
    args.output.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="ipodjs-apple-") as temp_name:
        temporary = Path(temp_name)
        images = temporary / "firmware-images"
        run(pillow_python(), ROOT / "tools/ipod_stock_resource_extract.py",
            args.ipsw, images)

        blank = find_resource(images, 24413, 95, 82)
        digits = find_resource(images, 24414, 95, 82)
        search_field = find_resource(images, 22203, 97, 32)
        search_selected = find_resource(images, 22206, 97, 32)
        run(require_tool("magick"), blank, "-define", "bmp:format=bmp4",
            args.output / "fast-scroll-blank.apple.95x82x32.bmp")
        run(require_tool("magick"), digits, "-define", "bmp:format=bmp4",
            args.output / "fast-scroll-123.apple.95x82x32.bmp")

        header = find_resource(images, 24379, 320, 24)
        run(require_tool("magick"), header,
            f"BMP3:{args.output / 'status-header.apple.320x24x24.bmp'}")

        pause = find_resource(images, 25552, 20, 16)
        play = find_resource(images, 25554, 20, 16)
        hold = find_resource(images, 24263, 12, 15)
        repeat = find_resource(images, 24265, 21, 19)
        repeat_one = find_resource(images, 24266, 21, 19)
        shuffle = find_resource(images, 24267, 21, 19)
        transparent_key = ("-fill", "#ff00ff", "-opaque", "white")
        run(require_tool("magick"), search_field, *transparent_key,
            f"BMP3:{args.output / 'search-field.apple.97x32x24.bmp'}")
        run(require_tool("magick"), search_selected, *transparent_key,
            f"BMP3:{args.output / 'search-selected.apple.97x32x24.bmp'}")
        run(require_tool("magick"), play, pause, *transparent_key, "-append",
            f"BMP3:{args.output / 'status-playback.apple.20x32x24.bmp'}")
        run(require_tool("magick"), hold, *transparent_key,
            f"BMP3:{args.output / 'status-hold.apple.12x15x24.bmp'}")
        run(require_tool("magick"), repeat, repeat_one, *transparent_key,
            "-append",
            f"BMP3:{args.output / 'status-repeat.apple.21x38x24.bmp'}")
        run(require_tool("magick"), shuffle, *transparent_key,
            f"BMP3:{args.output / 'status-shuffle.apple.21x19x24.bmp'}")

        volume_low = find_resource(images, 24282, 11, 17)
        volume_high = find_resource(images, 24283, 19, 18)
        brightness_low = find_resource(images, 24286, 20, 21)
        brightness_high = find_resource(images, 24287, 30, 31)
        slider_light = find_resource(images, 24292, 248, 20)
        slider_dark = find_resource(images, 24293, 248, 20)
        slider_fill = find_resource(images, 24343, 316, 20)
        progress_frame = find_resource(images, 30239, 200, 22)
        search_song = find_resource(images, 30240, 16, 16)
        search_artist = find_resource(images, 30242, 16, 16)
        search_album = find_resource(images, 30244, 16, 16)
        search_playlist = find_resource(images, 30248, 16, 16)
        run(require_tool("magick"), volume_low, *transparent_key,
            f"BMP3:{args.output / 'volume-low.apple.11x17x24.bmp'}")
        run(require_tool("magick"), volume_high, *transparent_key,
            f"BMP3:{args.output / 'volume-high.apple.19x18x24.bmp'}")
        run(require_tool("magick"), brightness_low,
            "-define", "bmp:format=bmp4",
            args.output / "brightness-low.apple.20x21x32.bmp")
        run(require_tool("magick"), brightness_high,
            "-define", "bmp:format=bmp4",
            args.output / "brightness-high.apple.30x31x32.bmp")
        run(require_tool("magick"), slider_light,
            "-define", "bmp:format=bmp4",
            args.output / "slider-light.apple.248x20x32.bmp")
        run(require_tool("magick"), slider_dark,
            "-define", "bmp:format=bmp4",
            args.output / "slider-dark.apple.248x20x32.bmp")
        run(require_tool("magick"), slider_fill,
            f"BMP3:{args.output / 'slider-fill.apple.316x20x24.bmp'}")
        run(require_tool("magick"), progress_frame,
            "-define", "bmp:format=bmp4",
            args.output / "progress-frame.apple.200x22x32.bmp")
        # Preserve the blue pixels from Apple's paMB 24343 strip and apply
        # only the alpha silhouette from Apple's paMB 30239 progress frame.
        # This supplies the renderer with stock rounded clipping without any
        # traced/repainted pixels, generated geometry, or resampling.
        progress_fill_base = temporary / "progress-fill-base.png"
        run(require_tool("magick"), "-size", "200x22", "xc:none",
            "(", slider_fill, "-crop", "200x16+2+2", "+repage", ")",
            "-geometry", "+0+3", "-compose", "over", "-composite",
            progress_fill_base)
        run(require_tool("magick"), progress_fill_base, progress_frame,
            "-compose", "DstIn", "-composite",
            "-define", "bmp:format=bmp4",
            args.output / "progress-fill.apple.200x22x32.bmp")
        run(require_tool("magick"),
            "(", progress_frame, "-crop", "16x16+92+3", "+repage", ")",
            "(", args.output / "progress-fill.apple.200x22x32.bmp",
            "-crop", "16x16+184+3", "+repage", ")",
            "-compose", "over", "-composite",
            f"BMP3:{args.output / 'progress-fill-cap.apple.16x16x24.bmp'}")
        # These are direct 16x16 Apple media/contact resources.  Keep each
        # resource intact: Search chooses among them by result type and never
        # traces or substitutes a hand-drawn glyph.
        run(require_tool("magick"), search_song,
            f"BMP3:{args.output / 'search-song.apple.16x16x24.bmp'}")
        run(require_tool("magick"), search_artist,
            f"BMP3:{args.output / 'search-artist.apple.16x16x24.bmp'}")
        run(require_tool("magick"), search_album,
            f"BMP3:{args.output / 'search-album.apple.16x16x24.bmp'}")
        run(require_tool("magick"), search_playlist,
            f"BMP3:{args.output / 'search-playlist.apple.16x16x24.bmp'}")

        resource_image = temporary / "ipod-resources.fat"
        font_ttf = temporary / "Helvetica_23.ttf"
        font17_ttf = temporary / "Helvetica_17.ttf"
        extract_resource_partition(args.ipsw, resource_image)
        run(require_tool("mcopy"), "-o", "-i", resource_image,
            "::/Resources/Fonts/Helvetica_23.ttf", font_ttf)
        run(require_tool("mcopy"), "-o", "-i", resource_image,
            "::/Resources/Fonts/Helvetica_17.ttf", font17_ttf)
        require_hash(font_ttf, FONT_HASH, "Apple Helvetica font")
        require_hash(font17_ttf, FONT17_HASH, "Apple Helvetica 17 font")
        # The overlay only renders A-Z.  Limiting the direct conversion avoids
        # a 64K sparse glyph table whose offsets exceed the small Rockbox font
        # loader's practical use here; the Apple glyph pixels are unchanged.
        run(ROOT / "tools/convttf", "-B", "-s", "65", "-l", "90",
            "-p", "23", "-o",
            args.output / "23-Helvetica-Apple.fnt", font_ttf)
        # Helvetica_17 carries an embedded strike whose packed grayscale
        # format is not compatible with Rockbox's 1-bit .fnt renderer: using
        # it produces opaque coloured glyph cells.  Rasterize the outlines
        # from the verified Apple font instead; no glyph is traced or drawn.
        run(ROOT / "tools/convttf", "-s", "32", "-l", "126",
            "-p", "17", "-o",
            args.output / "17-Helvetica-Apple.fnt", font17_ttf)

        classic_resource_image = temporary / "classic-ipod-resources.fat"
        classic_font_ttf = temporary / "Classic-Helvetica.ttf"
        classic_cjk_ttf = temporary / "Classic-CJK16.ttf"
        extract_classic_resource_partition(args.classic_ipsw,
                                           classic_resource_image)
        run(require_tool("mcopy"), "-o", "-i", classic_resource_image,
            "::/Resources/Fonts/Helvetica.ttf", classic_font_ttf)
        run(require_tool("mcopy"), "-o", "-i", classic_resource_image,
            "::/Resources/Fonts/CJK16.ttf", classic_cjk_ttf)
        require_hash(classic_font_ttf, CLASSIC_FONT_HASH,
                     "Apple Classic Helvetica font")
        require_hash(classic_cjk_ttf, CLASSIC_CJK16_HASH,
                     "Apple Classic CJK16 font")
        run(ROOT / "tools/convttf", "-s", "32", "-l", "126",
            "-p", "17", "-o",
            args.output / "17-Helvetica-Classic-Apple.fnt",
            classic_font_ttf)

        build_battery_strip(
            args.guide, temporary,
            args.output / "status-battery.apple.26x65x24.bmp",
        )

    generated = sorted(path for path in args.output.iterdir() if path.is_file())
    provenance = [
        "Private iPodJS Apple asset extraction",
        "",
        "No artwork is traced, redrawn, interpolated, or resampled.",
        "The BMP files are format conversions of exact Apple pixels.",
        "",
        "Apple iPod_13.1.3.ipsw",
        f"SHA-256: {IPSW_SHA256}",
        "Source: https://secure-appldnld.apple.com/iPod/SBML/osx/bundles/061-2965.20080313.R45jT/iPod_13.1.3.ipsw",
        "Resources: paMB 22203/22206/24263/24265/24266/24267/24282/24283,",
        "           24286/24287,",
        "           24292/24293/24343/24379/24413/24414/25552/25554/30239,",
        "           30240/30242/30244/30248,",
        "           and",
        "           Resources/Fonts/Helvetica_17.ttf and Helvetica_23.ttf",
        "",
        "Apple iPod classic 120GB User Guide",
        f"SHA-256: {GUIDE_SHA256}",
        "Resources: page 16 battery-state images (PDF objects 123-131)",
        "",
        "Apple iPod_24.1.1.2.ipsw",
        f"SHA-256: {CLASSIC_IPSW_SHA256}",
        "Resource: Resources/Fonts/Helvetica.ttf from the intact FAT16",
        "          iPodResources volume; official outlines rasterized at 17px.",
        "",
        "Generated files:",
    ]
    for path in generated:
        if path.name != "PROVENANCE.txt":
            provenance.append(f"{path.name}\t{sha256(path)}")
    provenance.append("")
    (args.output / "PROVENANCE.txt").write_text(
        "\n".join(provenance), encoding="utf-8"
    )
    print(f"prepared {len(generated)} private Apple assets in {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
