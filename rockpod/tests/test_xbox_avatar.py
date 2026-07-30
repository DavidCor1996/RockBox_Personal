import json
import sys
import os
import struct
from pathlib import Path

import numpy as np
import pytest
from PIL import Image, ImageChops, ImageDraw

from services.xbox_avatar import (
    AvatarProfile,
    APPEARANCE_FIELDS,
    COLOUR_PALETTES,
    CLIPS,
    RAV_FLAG_KEYFRAMES,
    RAV_HEADER,
    STYLE_LABELS,
    XboxAvatarService,
    decode_rav1,
    encode_rav1,
    encode_ui_bank,
)
from services.xbox_avatar_render import (
    AvatarRig,
    BODY_PRESETS,
    DECAL_LABELS,
    DECAL_SOURCES,
    available_styles,
    garment_clipping,
    head_layers,
    render_frames,
    resolve_wardrobe,
)


ROOT = Path(__file__).resolve().parents[2]


def _rig():
    rig = AvatarRig(ROOT)
    if not rig.available():
        pytest.skip("real XNA rig pack is not installed")
    return rig


def _frames():
    result = []
    for offset in (0, 3, 7):
        frame = Image.new("RGBA", (18, 24), (0, 0, 0, 0))
        draw = ImageDraw.Draw(frame)
        draw.rectangle((4 + offset, 5, 10 + offset, 20), fill=(30, 180, 70, 255))
        result.append(frame)
    return result


# ---------------------------------------------------------------------------
# Bounded device formats
# ---------------------------------------------------------------------------


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


# ---------------------------------------------------------------------------
# Real rig pack provenance
# ---------------------------------------------------------------------------


def test_rig_pack_is_real_microsoft_geometry_with_every_original_channel():
    rig = _rig()
    manifest = rig.manifest
    assert manifest["license"] == "Microsoft Permissive License (Ms-PL)"
    assert manifest["hand_drawn_or_generated_geometry"] is False
    assert "AvatarAnimPack" in manifest["archive"]

    # Both heads must expose Microsoft's dedicated facial UV channels, which is
    # what makes the face land on the head instead of being placed by guesswork.
    for head in ("boy-head", "girl-head"):
        channels = set(rig.part_channels(head))
        assert {"__EyeIntensityMap", "__EyeBrowIntensityMap",
                "__MouthIntensityMap"} <= channels
        assert manifest["parts"][head]["triangles"] > 0

    for part in ("boy-top", "girl-top", "boy-bottoms", "girl-bottoms",
                 "boy-shoes", "girl-shoes", "girl-highheels", "boy-hair",
                 "girl-hair"):
        assert manifest["parts"][part]["vertices"] > 0
        assert any(rig.texture(name) is not None
                   for name in rig.part_textures(part))

    for clip in CLIPS:
        assert clip in manifest["clips"]


def test_rig_clip_positions_animate_every_worn_part():
    rig = _rig()
    positions = rig.clip_positions("jump")
    for part in ("boy-body", "boy-head", "boy-top", "boy-bottoms",
                 "boy-shoes", "boy-hair"):
        data = positions[part]
        assert data.shape[0] == 14
        assert not np.allclose(data[0], data[len(data) // 2])


# ---------------------------------------------------------------------------
# Profile schema
# ---------------------------------------------------------------------------


def test_avatar_profile_normalizes_real_supported_presets():
    profile = AvatarProfile.from_mapping({
        "display_name": "  A very long offline player name  ",
        "body": "not-a-real-pack",
        "favorite_clip": "invented",
        "hair_style": "mohawk",
        "decal": "not-a-decal",
    })
    assert profile.display_name == "A very long off"
    assert profile.body == "xna-boy"
    assert profile.favorite_clip == "jump"
    # Unrecognised values fall back to each field's own documented default.
    defaults = AvatarProfile()
    assert all(getattr(profile, field) == getattr(defaults, field)
               for field in APPEARANCE_FIELDS)
    assert profile.is_original_appearance()


def test_legacy_palette_profiles_still_load():
    profile = AvatarProfile.from_mapping({
        "body": "xna-girl", "skin": "deep", "hair": "blond",
        "top": "xbox-green", "bottom": "black", "shoes": "white",
    })
    assert profile.skin_colour == "deep"
    assert profile.hair_colour == "blond"
    assert profile.top_colour == "xbox-green"
    assert profile.render_mapping()["skin_colour"] == "#704936"


def test_profile_rejects_garments_microsoft_did_not_fit_to_the_body():
    # The girl-fitted top and the heels belong to the girl body only.
    boy = AvatarProfile.from_mapping({
        "body": "xna-boy", "top_style": "scoop-tee", "shoes_style": "heels",
    })
    assert boy.top_style == "original"
    assert boy.shoes_style == "original"

    girl = AvatarProfile.from_mapping({
        "body": "xna-girl", "top_style": "scoop-tee", "shoes_style": "heels",
    })
    assert girl.top_style == "scoop-tee"
    assert girl.shoes_style == "heels"

    offered = available_styles("boy")
    assert "scoop-tee" not in offered["top"]
    assert "heels" not in offered["shoes"]
    assert set(offered) == {"hair", "top", "bottom", "shoes"}
    for slot, choices in offered.items():
        for choice in choices:
            assert choice in STYLE_LABELS


def test_every_style_and_colour_choice_is_offered_with_a_label():
    for field, entries in COLOUR_PALETTES.items():
        assert entries[0][0] == "original"
        assert all(colour is None or colour.startswith("#")
                   for _value, _label, colour in entries)
    assert set(DECAL_SOURCES) == set(DECAL_LABELS)


# ---------------------------------------------------------------------------
# Wardrobe rendering
# ---------------------------------------------------------------------------


def test_wardrobe_resolves_to_real_meshes_and_swaps_the_heel_leg_body():
    parts = resolve_wardrobe({"body": "xna-girl", "shoes_style": "heels"})
    assert parts["shoes"] == "girl-highheels"
    assert parts["body"] == "girl-body-heelleg"

    parts = resolve_wardrobe({"body": "xna-girl", "shoes_style": "sneakers"})
    assert parts["body"] == "girl-body"
    assert parts["shoes"] == "boy-shoes"

    bare = resolve_wardrobe({"body": "xna-boy", "hair_style": "shaved",
                             "top_style": "none", "shoes_style": "barefoot"})
    assert bare["hair"] is None and bare["top"] is None
    assert bare["shoes"] is None and bare["body"] == "boy-body"


def _head_only(profile, angle=0.0, size=(104, 168), supersample=3):
    import services.xbox_avatar_render as render

    original = render.DRAW_ORDER
    render.DRAW_ORDER = ("head",)
    try:
        return render.render_frames(
            _rig(), profile, "turntable", size, repo_root=ROOT,
            supersample=supersample, angles=[angle],
        )[0]
    finally:
        render.DRAW_ORDER = original


def test_face_features_sit_on_the_head_and_track_rotation(monkeypatch):
    """The face must be projected through the head's own UV channels."""
    profile = {"body": "xna-boy"}
    with_face = _head_only(profile)

    # Suppress only the eye, brow, and mouth layers; the skin base stays.
    import services.xbox_avatar_render as render
    real = render.head_layers
    monkeypatch.setattr(
        render, "head_layers",
        lambda rig, part, skin, *_a: real(rig, part, skin, None, None, None)[:2],
    )
    without_face = _head_only(profile)
    monkeypatch.undo()

    head_box = with_face.getchannel("A").getbbox()
    difference = ImageChops.difference(
        with_face.convert("RGB"), without_face.convert("RGB")
    ).getbbox()
    assert difference is not None, "face layers produced no pixels"

    left, top, right, bottom = difference
    hleft, htop, hright, hbottom = head_box
    assert hleft <= left and right <= hright
    assert htop <= top and bottom <= hbottom

    # The features must cluster on the face, not drift onto the neck or slide
    # off to one side, which is exactly how the old 2D placement failed.
    mask = np.asarray(ImageChops.difference(
        with_face.convert("RGB"), without_face.convert("RGB")
    ).convert("L"), dtype=np.float32)
    rows, columns = np.nonzero(mask > 8)
    assert rows.size > 20
    height = hbottom - htop
    width = hright - hleft
    assert (rows.mean() - htop) / height < 0.62
    assert 0.25 < (columns.mean() - hleft) / width < 0.75


def test_face_is_hidden_from_behind_and_moves_with_the_turntable():
    front = np.asarray(_head_only({"body": "xna-boy"}, 0.0), dtype=np.int16)
    turned = np.asarray(_head_only({"body": "xna-boy"}, 30.0), dtype=np.int16)
    back = np.asarray(_head_only({"body": "xna-boy"}, 180.0), dtype=np.int16)

    assert np.abs(front - turned).sum() > 0, "head does not rotate"

    # The eye texture is the only strongly blue-dominant material on the head,
    # so it stands in for "the face is visible from this angle".
    def eye_pixels(frame):
        rgb = frame[..., :3].astype(np.int32)
        opaque = frame[..., 3] > 128
        blue = (rgb[..., 2] > rgb[..., 0] + 25) & (rgb[..., 2] > rgb[..., 1] + 15)
        return int((blue & opaque).sum())

    assert eye_pixels(front) > 0
    assert eye_pixels(back) == 0


def test_colour_choices_only_repaint_their_own_material():
    rig = _rig()
    base = render_frames(rig, {"body": "xna-boy"}, "turntable", (104, 168),
                         repo_root=ROOT, supersample=2, angles=[0])[0]
    green = render_frames(rig, {"body": "xna-boy", "top_colour": "#69a72a"},
                          "turntable", (104, 168), repo_root=ROOT,
                          supersample=2, angles=[0])[0]
    assert ImageChops.difference(
        base.convert("RGB"), green.convert("RGB")
    ).getbbox()
    # Recolouring must not move the silhouette.
    assert base.getchannel("A").tobytes() == green.getchannel("A").tobytes()


def test_chest_prints_come_from_real_in_tree_artwork():
    for name, entry in DECAL_SOURCES.items():
        if entry is None:
            assert name in ("none", "original")
            continue
        relative, keyed = entry
        assert isinstance(keyed, bool)
        assert (ROOT / relative).is_file(), relative

    rig = _rig()
    plain = render_frames(rig, {"body": "xna-boy", "top_colour": "#303236"},
                          "turntable", (104, 168), repo_root=ROOT,
                          supersample=2, angles=[0])[0]
    printed = render_frames(
        rig, {"body": "xna-boy", "top_colour": "#303236", "decal": "rockbox"},
        "turntable", (104, 168), repo_root=ROOT, supersample=2, angles=[0],
    )[0]
    difference = ImageChops.difference(
        plain.convert("RGB"), printed.convert("RGB")
    ).getbbox()
    assert difference is not None
    # The print belongs on the chest, not the legs or the head.
    body_box = plain.getchannel("A").getbbox()
    height = body_box[3] - body_box[1]
    assert difference[1] >= body_box[1] + height * 0.18
    assert difference[3] <= body_box[1] + height * 0.60


def _clipping_ratio(rig, body, garment, slot="top"):
    through, covered = garment_clipping(rig, body, garment, slot)
    assert covered > 0
    return through / covered


def test_offered_tops_cover_the_torso_they_are_worn_on():
    """Guards the fitting rule against regressions in the wardrobe tables.

    A top is a closed shell over the torso, so body pixels winning the depth
    test through it are always clipping.  Bottoms and shoes deliberately leave
    skin on show, so the same measurement says nothing useful about them.
    """
    rig = _rig()
    for family, body, preset in (("boy", "boy-body", "xna-boy"),
                                 ("girl", "girl-body", "xna-girl")):
        for choice in available_styles(family)["top"]:
            garment = resolve_wardrobe({"body": preset,
                                        "top_style": choice})["top"]
            if not garment:
                continue
            ratio = _clipping_ratio(rig, body, garment)
            assert ratio <= 0.06, (
                f"{body} + {garment} leaves {ratio:.1%} of the top's pixels "
                "showing body underneath"
            )

    # The rule exists because Microsoft cut the scoop tee for the narrower
    # girl body; on the boy body the shoulders push straight through it.
    assert _clipping_ratio(rig, "boy-body", "girl-top") > 0.10


# ---------------------------------------------------------------------------
# Device pack
# ---------------------------------------------------------------------------


def test_bundled_mspl_avatar_assets_decode_and_sync(tmp_path):
    service = XboxAvatarService(ROOT, {
        "display_name": "DAVID",
        "body": "xna-girl",
        "favorite_clip": "throw",
        "skin_colour": "medium",
        "hair_colour": "auburn",
        "top_colour": "xbox-green",
        "bottom_colour": "gray",
        "shoes_colour": "black",
        "hair_style": "boy-short",
        "decal": "rockbox",
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
    generation_root = stage / "avatar" / "generations" / generation
    profile_text = (generation_root / "profile.v1.tsv").read_text()
    assert "display_name\tDAVID" in profile_text
    assert "gamerscore\t640" in profile_text
    assert "skin_colour\tmedium" in profile_text
    assert "decal\trockbox" in profile_text

    for clip in CLIPS:
        payload = (generation_root / "clips" / f"{clip}.rav").read_bytes()
        frames, frame_ms = decode_rav1(payload)
        assert frames[0].size == (104, 168)
        assert len(payload) <= 1024 * 1024
        assert frame_ms == (200 if clip == "turntable" else 125)


def test_appearance_changes_produce_a_new_generation(tmp_path):
    def generation_for(profile):
        stage = tmp_path / f"stage-{abs(hash(json.dumps(profile, sort_keys=True)))}"
        assets, _coverage = XboxAvatarService(ROOT, profile).build_sync_assets(
            tmp_path / "mount", stage, totals={},
        )
        current = next(asset for asset in assets
                       if asset["destination_rel"].endswith("/avatar/current"))
        return Path(current["source_abs"]).read_text().strip()

    (tmp_path / "mount").mkdir(exist_ok=True)
    plain = generation_for({"body": "xna-boy"})
    printed = generation_for({"body": "xna-boy", "decal": "apple"})
    assert plain != printed


def test_turntable_covers_a_full_rotation_for_every_body():
    rig = _rig()
    for body in BODY_PRESETS:
        service = XboxAvatarService(ROOT, {"body": body})
        frames = service.frames("turntable", "device")
        assert len(frames) == 24
        assert frames[0].tobytes() != frames[6].tobytes()
        assert frames[6].tobytes() != frames[12].tobytes()
    assert rig.available()


# ---------------------------------------------------------------------------
# Creator
# ---------------------------------------------------------------------------


def test_avatar_editor_uses_real_rig_and_emits_persistable_profile():
    from PySide6.QtWidgets import QApplication
    from ui.xbox_avatar_editor import XboxAvatarEditorWidget

    QApplication.instance() or QApplication([])
    editor = XboxAvatarEditorWidget(str(ROOT))
    seen = []
    editor.profile_saved.connect(seen.append)
    try:
        editor.set_profile({
            "xbox_avatar_display_name": "DAVID",
            "xbox_avatar_body": "xna-girl",
            "xbox_avatar_favorite_clip": "throw",
            "xbox_avatar_skin_colour": "deep",
            "xbox_avatar_hair_colour": "blond",
            "xbox_avatar_top_colour": "xbox-green",
            "xbox_avatar_shoes_style": "heels",
            "xbox_avatar_decal": "apple",
        })
        assert editor._frames
        editor._emit_saved()
        assert len(seen) == 1
        saved = seen[0]
        assert saved["xbox_avatar_display_name"] == "DAVID"
        assert saved["xbox_avatar_body"] == "xna-girl"
        assert saved["xbox_avatar_shoes_style"] == "heels"
        assert saved["xbox_avatar_decal"] == "apple"
        assert saved["xbox_avatar_skin_colour"] == "deep"
    finally:
        editor.close()


def test_editor_only_offers_garments_that_fit_the_selected_body():
    from PySide6.QtWidgets import QApplication
    from ui.xbox_avatar_editor import XboxAvatarEditorWidget

    QApplication.instance() or QApplication([])
    editor = XboxAvatarEditorWidget(str(ROOT))
    try:
        editor.set_profile({"xbox_avatar_body": "xna-boy"})
        shoes = editor._appearance["shoes_style"]
        assert shoes.findData("heels") < 0
        editor.set_profile({"xbox_avatar_body": "xna-girl"})
        shoes = editor._appearance["shoes_style"]
        assert shoes.findData("heels") >= 0
    finally:
        editor.close()


# ---------------------------------------------------------------------------
# Generated designs
# ---------------------------------------------------------------------------


def test_generated_designs_are_parametric_and_reproducible():
    from services.xbox_avatar_designs import (
        DESIGNS, DESIGN_CHOICES, SCALES, generate,
    )

    white = np.array([1.0, 1.0, 1.0], dtype=np.float32)
    black = np.array([0.0, 0.0, 0.0], dtype=np.float32)
    assert DESIGN_CHOICES[0] == "none"
    assert generate("none", white, black) is None

    for name in DESIGNS:
        tile = generate(name, white, black, seed=3)
        assert tile.shape[2] == 3
        assert tile.min() >= 0.0 and tile.max() <= 1.0
        # A design has to actually vary, except for the deliberate flat fill.
        if name != "solid":
            assert tile.std() > 0.01, name
        again = generate(name, white, black, seed=3)
        assert np.array_equal(tile, again), f"{name} is not reproducible"

    coarse = generate("stripes", white, black, scale="huge")
    fine = generate("stripes", white, black, scale="fine")
    assert not np.array_equal(coarse, fine)
    assert set(SCALES) >= {"fine", "medium", "huge"}


def test_designs_keep_the_real_garment_shading_and_silhouette():
    rig = _rig()
    plain = render_frames(rig, {"body": "xna-boy"}, "turntable", (104, 168),
                          repo_root=ROOT, supersample=2, angles=[0])[0]
    patterned = render_frames(
        rig,
        {"body": "xna-boy", "top_design": "plaid",
         "top_design_primary": "#a63239", "top_design_secondary": "#2b3a63"},
        "turntable", (104, 168), repo_root=ROOT, supersample=2, angles=[0],
    )[0]
    assert ImageChops.difference(
        plain.convert("RGB"), patterned.convert("RGB")
    ).getbbox()
    assert plain.getchannel("A").tobytes() == patterned.getchannel("A").tobytes()

    # The pattern belongs on the top, so the legs must be untouched.
    box = plain.getchannel("A").getbbox()
    height = box[3] - box[1]
    lower = (0, box[1] + int(height * 0.62), plain.width, plain.height)
    assert ImageChops.difference(
        plain.convert("RGB").crop(lower), patterned.convert("RGB").crop(lower)
    ).getbbox() is None


def test_design_profile_fields_round_trip_and_reject_junk():
    from services.xbox_avatar import DESIGN_SLOTS

    profile = AvatarProfile.from_mapping({
        "top_design": "argyle", "top_design_scale": "large",
        "bottom_design": "not-a-pattern", "accent_colour": "gold",
        "top_colour": "red",
    })
    assert profile.top_design == "argyle"
    assert profile.top_design_scale == "large"
    assert profile.bottom_design == "none"
    assert profile.accent_colour == "gold"

    mapping = profile.render_mapping()
    assert mapping["top_design_primary"] == "#a63239"
    assert mapping["top_design_secondary"] == "#c49a55"
    for slot in DESIGN_SLOTS:
        assert f"{slot}_design" in mapping
    assert not profile.is_original_appearance()


# ---------------------------------------------------------------------------
# Archived Marketplace items
# ---------------------------------------------------------------------------


def _pack():
    from services.xbox_avatar_render import MarketplacePack

    pack = MarketplacePack(ROOT)
    if not pack.available():
        pytest.skip("marketplace pack is not installed")
    return pack


def test_marketplace_pack_records_archive_provenance():
    pack = _pack()
    manifest = pack.manifest
    assert manifest["provenance"] == "archive-extracted"
    assert manifest["hand_drawn_or_generated_geometry"] is False
    assert "spriters-resource" in manifest["source"]
    assert manifest["items"], "no items were imported"
    for name, record in manifest["items"].items():
        assert record["slot"] in {"head", "top", "costume", "hand"}
        assert record["vertices"] > 0 and record["triangles"] > 0
        assert record["sha256"] and record["page"].startswith("https://")
        # Every item must carry at least one of its original textures.
        assert any(record["textures"]), name
        for filename in record["textures"]:
            if filename:
                assert (ROOT / "assets/ipodjs/sources/xbox360/avatar"
                        / "marketplace/textures" / filename).is_file()


def test_every_marketplace_item_lands_on_the_avatar():
    """Guards against an item drifting off the body or exploding in scale."""
    rig = _rig()
    pack = _pack()
    body = rig.clip_positions("turntable")["boy-body"][0]
    head = rig.clip_positions("turntable")["boy-head"][0]
    reference_low = np.minimum(body.min(axis=0), head.min(axis=0))
    reference_high = np.maximum(body.max(axis=0), head.max(axis=0))
    span = float(reference_high[1] - reference_low[1])

    from services.xbox_avatar_render import pose_marketplace_item

    for name, record in pack.manifest["items"].items():
        points = pose_marketplace_item(pack, rig, name, "turntable", 0)
        assert np.isfinite(points).all(), f"{name} produced invalid geometry"
        low, high = points.min(axis=0), points.max(axis=0)
        # Nothing may be more than one avatar away from the avatar.
        assert (low > reference_low - span).all(), name
        assert (high < reference_high + span).all(), name
        if record["slot"] == "head":
            # Headwear belongs on the head, not around the knees.
            assert high[1] > reference_low[1] + span * 0.60, name


def test_marketplace_items_follow_the_real_skeleton():
    rig = _rig()
    pack = _pack()
    from services.xbox_avatar_render import pose_marketplace_item

    rigid = next(name for name, record in pack.manifest["items"].items()
                 if record["slot"] == "head")
    skinned = next(name for name, record in pack.manifest["items"].items()
                   if record["slot"] == "costume")
    for name in (rigid, skinned):
        first = pose_marketplace_item(pack, rig, name, "jump", 0)
        later = pose_marketplace_item(pack, rig, name, "jump", 7)
        assert not np.allclose(first, later), f"{name} does not animate"


def test_costume_replaces_the_ordinary_wardrobe():
    from services.xbox_avatar_render import resolve_wardrobe

    pack = _pack()
    costume = next(name for name, record in pack.manifest["items"].items()
                   if record["slot"] == "costume")
    rig = _rig()
    plain = render_frames(rig, {"body": "xna-boy"}, "turntable", (104, 168),
                          repo_root=ROOT, supersample=2, angles=[0],
                          marketplace=pack)[0]
    dressed = render_frames(rig, {"body": "xna-boy", "costume": costume},
                            "turntable", (104, 168), repo_root=ROOT,
                            supersample=2, angles=[0], marketplace=pack)[0]
    assert ImageChops.difference(
        plain.convert("RGB"), dressed.convert("RGB")
    ).getbbox()
    # The base wardrobe itself is untouched; the costume simply hides it.
    assert resolve_wardrobe({"body": "xna-boy"})["top"] == "boy-top"


def test_marketplace_selection_round_trips_through_the_profile():
    pack = _pack()
    headwear = next(name for name, record in pack.manifest["items"].items()
                    if record["slot"] == "head")
    profile = AvatarProfile.from_mapping({
        "headwear": headwear, "prop": "Not A Real Item!!",
    })
    assert profile.headwear == headwear
    # Unknown names are sanitised here and rejected by the renderer.
    assert profile.prop == "notarealitem"
    from services.xbox_avatar_render import resolve_marketplace
    worn = resolve_marketplace(profile.render_mapping(), pack)
    assert worn.get("headwear") == headwear
    assert "prop" not in worn


def test_exact_placement_items_are_only_scaled_never_moved():
    """Captures that kept their avatar-space placement must be reproduced.

    Translating one of these would discard the very placement the Xbox used,
    so the import may only apply a uniform scale to them.  This reconstructs
    the world-space item from the stored bone-space geometry and requires it
    to be the raw capture times a single scalar.
    """
    import json as _json

    rig = _rig()
    pack = _pack()
    exact = {name for name, record in pack.manifest["items"].items()
             if record.get("placement") == "exact"}
    assert exact, "no capture retained its avatar-space placement"

    cache = ROOT / ".cache/xbox360-marketplace/index.json"
    if not cache.is_file():
        pytest.skip("marketplace download cache is not present")
    index = _json.loads(cache.read_text(encoding="utf-8"))["assets"]

    sys.path.insert(0, str(ROOT / "tools"))
    from xbox_avatar_marketplace import head_placement, read_members, read_obj

    order = rig.manifest["attachment_bones"]
    bind = rig.geometry["__bind_bones__"][order.index("head")]

    for name in sorted(exact):
        record = pack.manifest["items"][name]
        entry = index[record["asset"]]
        members = read_members(ROOT / ".cache/xbox360-marketplace" /
                               entry["file"])
        obj = next(n for n in members if n.lower().endswith(".obj"))
        raw, _uv, _faces, _materials = read_obj(
            members[obj].decode("utf-8", "replace")
        )
        assert head_placement(raw) == "exact", name

        local = pack.geometry[f"{name}/local"]
        homogeneous = np.concatenate(
            [local, np.ones((local.shape[0], 1), np.float32)], axis=1
        )
        world = (homogeneous @ bind.T)[:, :3]
        assert world.shape == raw.shape

        scale = float(world[:, 1].max()) / float(raw[:, 1].max())
        assert scale > 0
        assert np.allclose(world, raw * scale, atol=1e-3), (
            f"{name} was translated, not just scaled"
        )


def test_placement_is_recorded_for_every_item():
    """Every item states how it was placed, and only the hand-checked ones
    claim a correction."""
    pack = _pack()
    sys.path.insert(0, str(ROOT / "tools"))
    from xbox_avatar_marketplace import CORRECTIONS

    corrected = set()
    for name, record in pack.manifest["items"].items():
        placement = record.get("placement")
        assert placement in {"exact", "fitted", "corrected"}, name
        if placement == "corrected":
            corrected.add(name)
    assert corrected == set(CORRECTIONS)


def test_sync_forwards_every_avatar_field_from_the_stored_profile():
    """A wardrobe choice must survive the whole sync path to the device.

    The sync services used to name the few avatar fields they knew about, so
    every option added afterwards was silently dropped before reaching the
    iPod and the device kept rendering the default character.
    """
    from services.xbox_avatar import profile_from_target

    stored = {
        "id": "target",
        "source_repo_path": str(ROOT),
        "xbox_avatar_display_name": "DAVID",
        "xbox_avatar_body": "xna-boy",
        "xbox_avatar_favorite_clip": "walk",
        "xbox_avatar_headwear": "metal-sonic-helmet",
        "xbox_avatar_prop": "air-guitar",
        "xbox_avatar_decal": "rockbox",
        "xbox_avatar_top_design": "argyle",
        "xbox_avatar_top_colour": "black",
        "retroachievements_username": "ignored",
    }
    forwarded = profile_from_target(stored)
    assert forwarded["headwear"] == "metal-sonic-helmet"
    assert forwarded["prop"] == "air-guitar"
    assert forwarded["decal"] == "rockbox"
    assert "retroachievements_username" not in forwarded

    profile = AvatarProfile.from_mapping(forwarded)
    for field in APPEARANCE_FIELDS:
        assert getattr(profile, field) == forwarded.get(field, getattr(
            AvatarProfile(), field))

    # The sync services must hand the service the same complete mapping.
    import inspect
    from services import rockbox_games

    source = inspect.getsource(rockbox_games)
    assert source.count("profile_from_target(profile)") >= 2
    assert '"skin", "hair", "top", "bottom", "shoes"' not in source


def test_synced_generation_records_the_chosen_marketplace_items(tmp_path):
    from services.xbox_avatar import profile_from_target

    pack = _pack()
    headwear = next(name for name, record in pack.manifest["items"].items()
                    if record["slot"] == "head")
    stored = {
        "xbox_avatar_display_name": "DAVID",
        "xbox_avatar_body": "xna-boy",
        "xbox_avatar_headwear": headwear,
        "xbox_avatar_decal": "rockbox",
    }
    (tmp_path / "mount").mkdir()
    assets, coverage = XboxAvatarService(
        ROOT, profile_from_target(stored)
    ).build_sync_assets(tmp_path / "mount", tmp_path / "stage", totals={})
    assert coverage["available"] is True
    current = next(asset for asset in assets
                   if asset["destination_rel"].endswith("/avatar/current"))
    generation = Path(current["source_abs"]).read_text().strip()
    profile_text = (tmp_path / "stage" / "avatar" / "generations" /
                    generation / "profile.v1.tsv").read_text()
    assert f"headwear\t{headwear}" in profile_text
    assert "decal\trockbox" in profile_text


def test_editor_round_trips_every_appearance_field():
    """Anything the creator can set must come back out unchanged.

    This is the guarantee that a change made in RockPod survives being saved
    and reloaded, so it can reach the device intact.
    """
    from PySide6.QtWidgets import QApplication
    from ui.xbox_avatar_editor import XboxAvatarEditorWidget

    QApplication.instance() or QApplication([])
    pack = _pack()
    headwear = next(name for name, record in pack.manifest["items"].items()
                    if record["slot"] == "head")
    prop = next(name for name, record in pack.manifest["items"].items()
                if record["slot"] == "hand")

    wanted = {
        "xbox_avatar_display_name": "DAVID",
        "xbox_avatar_body": "xna-girl",
        "xbox_avatar_favorite_clip": "walk",
        "xbox_avatar_hair_style": "girl-bob",
        "xbox_avatar_top_style": "scoop-tee",
        "xbox_avatar_bottom_style": "shorts",
        "xbox_avatar_shoes_style": "heels",
        "xbox_avatar_skin_colour": "deep",
        "xbox_avatar_hair_colour": "blond",
        "xbox_avatar_top_colour": "black",
        "xbox_avatar_bottom_colour": "khaki",
        "xbox_avatar_shoes_colour": "red",
        "xbox_avatar_eye_colour": "green",
        "xbox_avatar_brow_colour": "auburn",
        "xbox_avatar_lip_colour": "berry",
        "xbox_avatar_top_design": "argyle",
        "xbox_avatar_top_design_scale": "large",
        "xbox_avatar_bottom_design": "camo",
        "xbox_avatar_bottom_design_scale": "small",
        "xbox_avatar_shoes_design": "stripes",
        "xbox_avatar_shoes_design_scale": "fine",
        "xbox_avatar_accent_colour": "gold",
        "xbox_avatar_decal": "rockbox",
        "xbox_avatar_headwear": headwear,
        "xbox_avatar_prop": prop,
    }
    editor = XboxAvatarEditorWidget(str(ROOT))
    try:
        editor.set_profile(wanted)
        produced = editor.profile_values()
        for key, value in wanted.items():
            assert produced.get(key) == value, key
        for field in APPEARANCE_FIELDS:
            assert f"xbox_avatar_{field}" in produced, field
    finally:
        editor.close()


def test_chosen_colour_reaches_the_rendered_garment():
    """A colour picked in RockPod must change the exported pixels."""
    from services.xbox_avatar import profile_from_target

    _rig()
    default = XboxAvatarService(ROOT, profile_from_target({
        "xbox_avatar_body": "xna-boy",
    }))
    black = XboxAvatarService(ROOT, profile_from_target({
        "xbox_avatar_body": "xna-boy", "xbox_avatar_top_colour": "black",
    }))
    assert black.profile.top_colour == "black"
    assert black.profile.render_mapping()["top_colour"] == "#303236"
    plain = default.frames("turntable", "device")[0]
    dark = black.frames("turntable", "device")[0]
    assert ImageChops.difference(
        plain.convert("RGB"), dark.convert("RGB")
    ).getbbox(), "top colour did not change the render"
