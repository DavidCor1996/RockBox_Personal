import importlib.util
import io
import sys
import zipfile
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "cps1_prepare_bundle", ROOT / "tools/cps1_prepare_bundle.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def _inner_zip() -> bytes:
    output = io.BytesIO()
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("sf2_09.bin", b"sound-rom-fixture")
    return output.getvalue()


def test_extract_archives_validates_and_preserves_inner_zip(tmp_path):
    bundle = tmp_path / "bundle.zip"
    with zipfile.ZipFile(bundle, "w", zipfile.ZIP_STORED) as archive:
        archive.writestr("sf2.zip", _inner_zip())

    destination = tmp_path / "roms"
    assert MODULE.extract_archives(bundle, destination) == ["sf2"]
    with zipfile.ZipFile(destination / "sf2.zip") as extracted:
        assert extracted.testzip() is None
        assert extracted.read("sf2_09.bin") == b"sound-rom-fixture"


def test_extract_chip_roms_writes_crc_validated_direct_files(tmp_path):
    rom_root = tmp_path / "roms"
    rom_root.mkdir()
    (rom_root / "sf2.zip").write_bytes(_inner_zip())

    assert MODULE.extract_chip_roms(rom_root, ["sf2"]) == 1
    assert (
        rom_root / "sf2" / "sf2_09.bin"
    ).read_bytes() == b"sound-rom-fixture"


def test_manifest_publishes_only_parent_with_real_cached_cover(tmp_path):
    cover = tmp_path / MODULE.COVER_ROOT / "sf2.bmp"
    cover.parent.mkdir(parents=True)
    Image.new("RGB", (144, 108), (27, 40, 56)).save(cover)

    published, missing = MODULE.write_manifest(tmp_path, {"sf2"}, False)
    manifest = (tmp_path / MODULE.MANIFEST).read_text(encoding="utf-8")

    assert published == ["sf2"]
    assert missing == []
    assert "Street Fighter II: The World Warrior" in manifest
    assert "/.rockbox/games/cps1/roms/sf2.zip" in manifest
    assert manifest.count("\n") == 2
