"""Static checks for Rockbox album-art first-load source behavior."""

from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def _read(rel_path):
    return (REPO_ROOT / rel_path).read_text(encoding="utf-8", errors="replace")


def test_size_specific_album_art_prefers_bmp_before_jpeg():
    albumart = _read("apps/recorder/albumart.c")

    assert "sized_extension_order[] = { 2, 0, 1 }" in albumart
    assert "prefer_bmp ? sized_extension_order[i] : i" in albumart
    assert "try_exts(path, pathlen, *size_string != '\\0')" in albumart


def test_external_bmp_album_art_skips_jpeg_decode_overhead():
    buffering = _read("apps/buffering.c")

    assert "albumart_path_is_bmp" in buffering
    assert "if (aa->embedded_albumart != NULL || !albumart_path_is_bmp(file))" in buffering
    assert "JPEG_DECODE_OVERHEAD" in buffering
