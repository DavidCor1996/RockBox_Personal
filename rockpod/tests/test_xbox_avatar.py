import os
import struct
from pathlib import Path

import pytest
from PIL import Image, ImageChops, ImageDraw

from services.xbox_avatar import (
    AvatarProfile,
    APPEARANCE_FIELDS,
    BODY_PRESETS,
    CLIPS,
    RAV_FLAG_KEYFRAMES,
    RAV_HEADER,
    XboxAvatarService,
    avatar_material_masks,
    decode_rav1,
    encode_rav1,
    encode_ui_bank,
    load_strip,
    render_avatar_frame,
)


def test_avatar_editor_uses_real_pack_and_emits_persistable_profile():
    from PySide6.QtWidgets import QApplication
    from ui.xbox_avatar_editor import XboxAvatarEditorWidget

    QApplication.instance() or QApplication([])
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    editor = XboxAvatarEditorWidget(repo_root)
    seen = []
    editor.profile_saved.connect(seen.append)
    try:
        editor.set_profile({
            "xbox_avatar_display_name": "DAVID",
            "xbox_avatar_body": "xna-girl",
            "xbox_avatar_favorite_clip": "throw",
            "xbox_avatar_skin": "deep",
            "xbox_avatar_hair": "blond",
            "xbox_avatar_top": "xbox-green",
            "xbox_avatar_bottom": "black",
            "xbox_avatar_shoes": "white",
        })
        assert editor._frames
        assert editor._asset_path().name == "throw.rgba.png"
        assert editor._asset_path().is_file()
        editor._emit_saved()
        assert seen == [{
            "xbox_avatar_display_name": "DAVID",
            "xbox_avatar_body": "xna-girl",
            "xbox_avatar_favorite_clip": "throw",
            "xbox_avatar_skin": "deep",
            "xbox_avatar_hair": "blond",
            "xbox_avatar_top": "xbox-green",
            "xbox_avatar_bottom": "black",
            "xbox_avatar_shoes": "white",
        }]
    finally:
        editor.close()


ROOT = Path(__file__).resolve().parents[2]


def _frames():
    result = []
    for offset in (0, 3, 7):
        frame = Image.new("RGBA", (18, 24), (0, 0, 0, 0))
        draw = ImageDraw.Draw(frame)
        draw.rectangle((4 + offset, 5, 10 + offset, 20), fill=(30, 180, 70, 255))
        result.append(frame)
    return result


def test_rav1_round_trip_preserves_transparency_timing_and_motion():
    encoded = encode_rav1(_frames(), frame_ms=125)
    decoded, frame_ms = decode_rav1(encoded)

    assert encoded[:4] == b"RAV2"
    assert frame_ms == 125
    assert len(decoded) == 3
    assert decoded[0].getpixel((0, 0))[3] == 0
    assert decoded[0].getpixel((5, 8))[3] == 255
    assert decoded[2].getpixel((12, 8))[3] == 255


def test_rav1_rejects_crc_damage():
    encoded = bytearray(encode_rav1(_frames()))
    encoded[-1] ^= 0x80
    with pytest.raises(ValueError, match="CRC"):
        decode_rav1(bytes(encoded))


def test_rav1_header_stays_bounded_for_ipod_decoder():
    encoded = encode_rav1(_frames())
    magic, width, height, count, frame_ms, flags, reserved, _crc = (
        RAV_HEADER.unpack_from(encoded)
    )
    assert (magic, width, height, count, frame_ms, flags, reserved) == (
        b"RAV2", 18, 24, 3, 125, 3, 0,
    )


def test_rav2_independent_keyframes_support_reverse_turntable_seeking():
    encoded = encode_rav1(_frames(), frame_ms=200, keyframes=True)
    _magic, _width, _height, _count, frame_ms, flags, _reserved, _crc = (
        RAV_HEADER.unpack_from(encoded)
    )
    decoded, decoded_ms = decode_rav1(encoded)
    assert frame_ms == decoded_ms == 200
    assert flags & RAV_FLAG_KEYFRAMES
    assert decoded[2].getpixel((12, 8))[3] == 255
    assert decoded[0].getpixel((5, 8))[3] == 255


def test_ui_bank_has_named_bounded_pcm_ranges():
    bank = encode_ui_bank(44100, {
        "navigate": b"\x01\x00\x01\x00" * 4,
        "select": b"\x02\x00\x02\x00" * 5,
        "back": b"\x03\x00\x03\x00" * 6,
    })
    magic, rate, count, reserved = struct.unpack_from("<4sIHH", bank)
    assert (magic, rate, count, reserved) == (b"UIB1", 44100, 3, 0)
    entries = [struct.unpack_from("<III", bank, 12 + index * 12)
               for index in range(3)]
    assert [entry[2] for entry in entries] == [16, 20, 24]
    assert all(entry[1] + entry[2] <= len(bank) for entry in entries)


def test_avatar_profile_normalizes_real_supported_presets():
    profile = AvatarProfile.from_mapping({
        "display_name": "  A very long offline player name  ",
        "body": "not-a-real-pack",
        "favorite_clip": "invented",
    })
    assert profile.display_name == "A very long off"
    assert profile.body == "xna-boy"
    assert profile.favorite_clip == "jump"
    assert all(getattr(profile, field) == "original"
               for field in APPEARANCE_FIELDS)


def test_real_xna_material_regions_drive_custom_appearance():
    source = load_strip(
        ROOT / "assets/ipodjs/sources/xbox360/avatar/master/"
        "xna-boy/jump.rgba.png", frame_width=416)[0]
    masks = avatar_material_masks(source, "xna-boy")
    assert set(masks) == set(APPEARANCE_FIELDS)
    assert all(mask.getbbox() for mask in masks.values())

    customized = render_avatar_frame(source, {
        "body": "xna-boy",
        "skin": "deep",
        "hair": "blond",
        "top": "xbox-green",
        "bottom": "khaki",
        "shoes": "black",
    })
    assert ImageChops.difference(
        source.convert("RGB"), customized.convert("RGB")
    ).getbbox()
    assert source.getchannel("A").tobytes() == customized.getchannel("A").tobytes()
    changed = ImageChops.difference(source.convert("RGB"), customized.convert("RGB"))
    assert sum(1 for pixel in changed.getdata() if pixel != (0, 0, 0)) > 1200


def test_real_xna_facial_layers_are_visible_in_preview_and_device_frames():
    source = load_strip(
        ROOT / "assets/ipodjs/sources/xbox360/avatar/master/"
        "xna-boy/jump.rgba.png", frame_width=416)[0]
    rendered = XboxAvatarService(
        ROOT, {"body": "xna-boy", "favorite_clip": "jump"}
    ).frames("jump", "preview")[0]
    changed = ImageChops.difference(
        source.convert("RGB"), rendered.convert("RGB")
    )
    assert changed.getbbox()
    assert sum(
        1 for pixel in changed.getdata() if pixel != (0, 0, 0)
    ) >= 200
    assert source.getchannel("A").tobytes() == \
        rendered.getchannel("A").tobytes()


def test_bundled_mspl_avatar_assets_decode_and_sync(tmp_path):
    service = XboxAvatarService(ROOT, {
        "display_name": "DAVID",
        "body": "xna-girl",
        "favorite_clip": "throw",
        "skin": "medium",
        "hair": "auburn",
        "top": "xbox-green",
        "bottom": "gray",
        "shoes": "black",
    })
    mount = tmp_path / "mount"
    stage = tmp_path / "stage"
    mount.mkdir()

    assets, coverage = service.build_sync_assets(
        mount, stage,
        totals={"games": 12, "unlocked": 34, "achievements": 80,
                "gamerscore": 640},
    )

    assert coverage["available"] is True
    assert coverage["clips"] == len(CLIPS) == 8
    destinations = {asset["destination_rel"] for asset in assets}
    assert any(path.endswith("/clips/throw.rav") for path in destinations)
    assert ".rockbox/achievements/state/avatar-preferences.v1.tsv" in destinations
    current = next(asset for asset in assets
                   if asset["destination_rel"].endswith("/avatar/current"))
    generation = Path(current["source_abs"]).read_text().strip()
    profile_path = stage / "avatar" / "generations" / generation / "profile.v1.tsv"
    profile_text = profile_path.read_text()
    assert "display_name\tDAVID" in profile_text
    assert "gamerscore\t640" in profile_text
    assert "skin\tmedium" in profile_text
    assert "top\txbox-green" in profile_text
    clip_path = stage / "avatar" / "generations" / generation / "clips" / "throw.rav"
    frames, frame_ms = decode_rav1(clip_path.read_bytes())
    assert frames and frame_ms == 125
    assert frames[0].size == (104, 168)
    assert os.path.getsize(clip_path) <= 200 * 1024
    original = ROOT / (
        "assets/ipodjs/rockbox/achievements/avatar/base/"
        "xna-girl/clips/throw.rav"
    )
    assert clip_path.read_bytes() != original.read_bytes()


def test_real_3d_models_and_full_rotation_turntables_are_bundled():
    qml_root = ROOT / "rockpod/ui/qml/xbox_avatar_models"
    qml_names = {
        "xna-boy": "Xna_boy.qml",
        "xna-girl": "Xna_girl.qml",
        "xna-girl-heels": "Xna_girl_heels.qml",
    }
    for body in BODY_PRESETS:
        assert (qml_root / body / qml_names[body]).is_file()
        assert list((qml_root / body / "meshes").glob("*.mesh"))

        service = XboxAvatarService(ROOT, {"body": body})
        frames = service.frames("turntable", "device")
        assert len(frames) == 24
        assert frames[0].tobytes() != frames[6].tobytes()
        assert frames[6].tobytes() != frames[12].tobytes()

        rav_path = ROOT / (
            "assets/ipodjs/rockbox/achievements/avatar/base/"
            f"{body}/clips/turntable.rav"
        )
        payload = rav_path.read_bytes()
        magic, width, height, count, frame_ms, flags, _reserved, _crc = (
            RAV_HEADER.unpack_from(payload)
        )
        assert (magic, width, height, count, frame_ms) == (
            b"RAV2", 104, 168, 24, 200,
        )
        assert flags & RAV_FLAG_KEYFRAMES
        assert len(payload) <= 1024 * 1024
