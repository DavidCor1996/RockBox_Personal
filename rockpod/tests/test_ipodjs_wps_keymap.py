import runpy
import struct
from pathlib import Path

import pytest


fix_keymap = runpy.run_path(str(Path(__file__).resolve().parents[2] /
                               "tools/ipodjs_fix_wps_keymap.py"))["fix_keymap"]


def fixture():
    records = [(1, 0x47B2, 7), (0x08000001, 2, 3), (-1, 0, 0),
               (32, 3, 0), (18, 0x02000002, 2),
               (33, 0x02000001, 1), (-1, 0, 0)]
    return b"".join(struct.pack("<iii", *r) for r in records)


def test_only_short_select_changes_and_repair_is_idempotent():
    original = fixture()
    corrected = fix_keymap(original)
    assert [i for i, pair in enumerate(zip(original, corrected))
            if pair[0] != pair[1]] == [60]
    assert struct.unpack_from("<i", corrected, 60)[0] == 18
    assert fix_keymap(corrected) == corrected


@pytest.mark.parametrize("offset,value", [(0, 2), (4, 0), (8, 100),
                                         (16, -1), (20, 99), (60, 30)])
def test_unexpected_file_is_rejected(offset, value):
    data = bytearray(fixture())
    struct.pack_into("<i", data, offset, value)
    with pytest.raises(ValueError):
        fix_keymap(data)
