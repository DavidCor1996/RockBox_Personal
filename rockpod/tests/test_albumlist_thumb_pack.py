import os
import struct

from PIL import Image

from services.albumlist_export import (
    THUMB_PACK_MAGIC,
    THUMB_PACK_SIZE,
    THUMB_PACK_VERSION,
    _write_thumb_pack,
)


def _record_size():
    return 4 + THUMB_PACK_SIZE * THUMB_PACK_SIZE * 2


def test_thumb_pack_layout_matches_firmware_reader(tmp_dir):
    from pathlib import Path

    thumb = os.path.join(tmp_dir, "thumb.bmp")
    Image.new("RGB", (40, 40), (255, 0, 0)).save(thumb)
    small = os.path.join(tmp_dir, "small.bmp")
    Image.new("RGB", (40, 30), (0, 0, 255)).save(small)

    pack = Path(tmp_dir) / "thumbs.pack"
    _write_thumb_pack(pack, [thumb, None, small, os.path.join(tmp_dir, "missing.bmp")])

    data = pack.read_bytes()
    assert data[:4] == THUMB_PACK_MAGIC
    version, width, height, reserved, count = struct.unpack_from("<HHHHI", data, 4)
    assert version == THUMB_PACK_VERSION
    assert (width, height) == (THUMB_PACK_SIZE, THUMB_PACK_SIZE)
    assert reserved == 0
    assert count == 4
    assert len(data) == 16 + count * _record_size()

    # Record 0: present 40x40 pure red -> RGB565 0xF800 little-endian.
    offset = 16
    present, _, rec_w, rec_h = struct.unpack_from("<BBBB", data, offset)
    assert (present, rec_w, rec_h) == (1, 40, 40)
    first_pixel = struct.unpack_from("<H", data, offset + 4)[0]
    assert first_pixel == 0xF800

    # Record 1: absent.
    offset = 16 + _record_size()
    present = data[offset]
    assert present == 0
    assert data[offset + 4:offset + 4 + 16] == b"\x00" * 16

    # Record 2: present 40x30 pure blue -> 0x001F, padded to record size.
    offset = 16 + 2 * _record_size()
    present, _, rec_w, rec_h = struct.unpack_from("<BBBB", data, offset)
    assert (present, rec_w, rec_h) == (1, 40, 30)
    pixel = struct.unpack_from("<H", data, offset + 4)[0]
    assert pixel == 0x001F
    payload_end = offset + 4 + rec_w * rec_h * 2
    record_end = offset + _record_size()
    assert data[payload_end:record_end] == b"\x00" * (record_end - payload_end)

    # Record 3: missing source file -> absent.
    offset = 16 + 3 * _record_size()
    assert data[offset] == 0
