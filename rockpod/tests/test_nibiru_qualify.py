"""NiBiRu AGDS qualification tests using synthetic, non-game containers."""

import struct

from tools import nibiru_ipod_qualify as nibiru


def _adb(path):
    name_size = 7
    payload = b"process"
    record = struct.pack("<I", 0) + b"main\0\0\0\0" + struct.pack("<I", len(payload))
    path.write_bytes(struct.pack("<5I", 666, 0, 1, 1, name_size) + record + payload)


def _grp(path, encrypted=False):
    member_name = b"room.bmp"
    signature = nibiru.AGDS_GRP_SIGNATURE
    if encrypted:
        signature = nibiru.decrypt_agds(signature)
        member_name = nibiru.decrypt_agds(member_name)
    data_offset = 0x2C + 0x31
    header = signature + struct.pack("<4I", 44, 0x1A03C9E6, 2, 1) + bytes(12)
    record = member_name + bytes(0x21 - len(member_name))
    record += struct.pack("<II", data_offset, 2) + bytes(8)
    path.write_bytes(header + record + b"BM")


def test_agds_directory_probe_validates_named_archives(tmp_path):
    _adb(tmp_path / "data.adb")
    _grp(tmp_path / "gfx1.grp", encrypted=True)
    (tmp_path / "agds.cfg").write_text("path=gfx1.grp\n", encoding="ascii")

    report = nibiru.build_report(tmp_path)

    assert report["agds"]["detected"] is True
    assert report["agds"]["database"]["used_entries"] == 1
    assert report["agds"]["groups"]["gfx1.grp"]["encrypted"] is True
    assert report["agds"]["groups"]["gfx1.grp"]["extensions"] == {".bmp": 1}
    assert report["rockbox_runtime"]["status"] == (
        "engine-route-identified-needs-data-validation"
    )


def test_agds_probe_rejects_out_of_bounds_member(tmp_path):
    _grp(tmp_path / "gfx1.grp")
    blob = bytearray((tmp_path / "gfx1.grp").read_bytes())
    struct.pack_into("<I", blob, 0x2C + 0x21, len(blob) + 1)
    (tmp_path / "gfx1.grp").write_bytes(blob)

    try:
        nibiru.parse_grp(tmp_path / "gfx1.grp")
    except RuntimeError as exc:
        assert "outside file" in str(exc)
    else:
        raise AssertionError("out-of-bounds GRP member was accepted")
