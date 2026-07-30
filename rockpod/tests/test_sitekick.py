"""Tests for the host-side Sitekick chip, drop and trade helpers."""

from __future__ import annotations

from datetime import date
from pathlib import Path
import struct

import pytest
from PIL import Image

from rockpod.services import sitekick


CHIP_HEADER = "# id\tslot\tz\tax\tay\tw\th\tpage\tindex\trarity\tworn\tname"


def _row(chip_id, slot="face", rarity="common", worn=1, name=None):
    name = name or f"Chip {chip_id:04d}"
    return (f"{chip_id}\t{slot}\t0\t-10\t-10\t40\t40\t0\t{chip_id % 50}\t"
            f"{rarity}\t{worn}\t{name}")


@pytest.fixture()
def pack(tmp_path):
    """A minimal but realistic packaged asset tree."""
    (tmp_path / "data").mkdir()
    rows = [CHIP_HEADER]
    for index in range(1, 31):
        rarity = ("common", "rare", "legendary")[index % 3]
        rows.append(_row(index, rarity=rarity))
    (tmp_path / sitekick.CHIPS_FILE).write_text("\n".join(rows) + "\n",
                                                encoding="utf-8")
    return tmp_path


def test_load_chips_parses_every_row(pack):
    chips = sitekick.load_chips(pack)
    assert len(chips) == 30
    assert chips[1].name == "Chip 0001"
    assert chips[1].worn is True
    assert {chip.rarity for chip in chips.values()} == {
        "common", "rare", "legendary"}


def test_load_chips_rejects_missing_pack(tmp_path):
    with pytest.raises(sitekick.SitekickError):
        sitekick.load_chips(tmp_path)


def test_overrides_rename_and_reclassify(pack):
    (pack / sitekick.OVERRIDES_FILE).write_text(
        "# id\tname\trarity\tslot\n"
        "1\tSolar Visor\tlegendary\teyes\n"
        "2\tRust Shell\t\tshell\n",
        encoding="utf-8")

    changed = sitekick.apply_overrides(pack)
    assert changed == 2

    chips = sitekick.load_chips(pack)
    assert chips[1].name == "Solar Visor"
    assert chips[1].rarity == "legendary"
    assert chips[1].slot == "eyes"
    # A blank column must leave the packaged value alone.
    assert chips[2].name == "Rust Shell"
    assert chips[2].slot == "shell"
    assert chips[2].rarity == chips[2].rarity
    # Geometry from the packager survives an override.
    assert chips[1].ax == -10 and chips[1].w == 40


def test_overrides_ignore_unknown_chip_and_bad_values(pack):
    (pack / sitekick.OVERRIDES_FILE).write_text(
        "9999\tGhost\tlegendary\teyes\n"
        "3\tKeep\tnot-a-rarity\tnot-a-slot\n",
        encoding="utf-8")
    assert sitekick.apply_overrides(pack) == 1
    chips = sitekick.load_chips(pack)
    assert chips[3].name == "Keep"
    assert chips[3].rarity == "common"      # invalid rarity ignored
    assert 9999 not in chips


def test_drops_are_deterministic_for_a_date(pack):
    chips = sitekick.load_chips(pack)
    when = date(2026, 7, 25)
    assert sitekick.daily_chip(chips, when) == \
        sitekick.daily_chip(chips, when)
    assert sitekick.weekly_chips(chips, when) == \
        sitekick.weekly_chips(chips, when)


def test_weekly_drop_has_the_expected_rarity_mix(pack):
    chips = sitekick.load_chips(pack)
    picks = sitekick.weekly_chips(chips, date(2026, 7, 25))
    rarities = [chips[cid].rarity for cid in picks]
    assert rarities.count("common") == 2
    assert rarities.count("rare") == 2
    assert rarities.count("legendary") == 1
    assert len(set(picks)) == len(picks)


def test_different_weeks_give_different_drops(pack):
    chips = sitekick.load_chips(pack)
    first = sitekick.weekly_chips(chips, date(2026, 7, 25))
    later = sitekick.weekly_chips(chips, date(2026, 9, 5))
    assert first != later


def test_codes_resolve_to_grants(pack):
    (pack / sitekick.CODES_FILE).write_text(
        "# code\tkind\tvalue\tlabel\n"
        "SPRING24\tgrant\t7\tSpring Chip\n"
        "SPRING24\tcoins\t100\tSpring Coins\n",
        encoding="utf-8")
    grants = sitekick.redeem(pack, "spring24")     # case insensitive
    assert [g.kind for g in grants] == ["grant", "coins"]
    assert grants[1].value == 100
    assert sitekick.redeem(pack, "NOPE") == []


def test_build_drop_dedupes_and_honours_code(pack):
    (pack / sitekick.CODES_FILE).write_text(
        "BONUS\tgrant\t7\tBonus Chip\n", encoding="utf-8")
    grants = sitekick.build_drop(pack, when=date(2026, 7, 25), code="BONUS")
    chip_ids = [g.value for g in grants if g.kind == "grant"]
    assert len(chip_ids) == len(set(chip_ids))
    assert 7 in chip_ids


def test_inbox_round_trips_through_the_device_format(pack):
    grants = [sitekick.Grant("grant", 12, "Chip of the Day"),
              sitekick.Grant("coins", 50, "Weekly bonus")]
    path = sitekick.write_inbox(pack, grants)
    lines = [l for l in path.read_text(encoding="utf-8").splitlines()
             if l and not l.startswith("#")]
    assert lines == ["grant\t12\tChip of the Day", "coins\t50\tWeekly bonus"]


def test_inbox_labels_cannot_break_the_tsv(pack):
    path = sitekick.write_inbox(
        pack, [sitekick.Grant("grant", 1, "bad\tlabel\nsecond")])
    body = [l for l in path.read_text(encoding="utf-8").splitlines()
            if not l.startswith("#")]
    assert len(body) == 1
    assert body[0] == "grant\t1\tbad label second"


def test_stage_code_preserves_pending_rewards_and_skips_owned(pack):
    (pack / sitekick.CODES_FILE).write_text(
        "BLACKERY\tgrant\t7\tVillains Bob\n"
        "BLACKERY\tgrant\t8\tBox Pigtails\n",
        encoding="utf-8",
    )
    sitekick.write_inbox(
        pack, [sitekick.Grant("grant", 12, "Pending trade")]
    )

    added = sitekick.stage_code(pack, "blackery", owned=(7,))

    assert [(grant.kind, grant.value) for grant in added] == [("grant", 8)]
    assert [(grant.kind, grant.value) for grant in sitekick.read_inbox(pack)] == [
        ("grant", 12), ("grant", 8)
    ]
    assert sitekick.stage_code(pack, "BLACKERY", owned=(7,)) == []


def test_stage_code_rejects_unknown_code(pack):
    with pytest.raises(sitekick.SitekickError, match="Unknown"):
        sitekick.stage_code(pack, "not-real")


def test_outbox_reads_offers_and_clears(pack):
    (pack / "sync").mkdir()
    (pack / sitekick.OUTBOX_FILE).write_text(
        "# type\tchip\tname\n"
        "offer\t12\tChip 0012\n"
        "offer\t14\tChip 0014\n"
        "junk\tx\n",
        encoding="utf-8")
    offers = sitekick.read_outbox(pack)
    assert [o.chip_id for o in offers] == [12, 14]
    assert sitekick.clear_outbox(pack) is True
    assert sitekick.read_outbox(pack) == []


def test_settle_trade_only_ever_grants(pack):
    offers = [sitekick.Offer(12, "Chip 0012")]
    grants = sitekick.settle_trade(offers, [21])
    assert [(g.kind, g.value) for g in grants] == [("grant", 21)]

    # An offer with nothing coming back pays credit, never a deletion.
    credit = sitekick.settle_trade(offers, [])
    assert credit[0].kind == "coins"
    assert all(g.kind != "revoke" for g in credit)


def test_deploy_copies_pack_into_rockbox_dir(pack, tmp_path):
    for name in (
        "base", "chips", "icons", "sounds", "backgrounds", "preview"
    ):
        (pack / name).mkdir()
        (pack / name / f"{name}.bmp").write_bytes(b"x")
    (pack / "source.manifest").write_text("x\ty\n", encoding="utf-8")

    device = tmp_path / "IPOD"
    device.mkdir()
    copied = sitekick.deploy(pack, device)

    target = device / ".rockbox" / "sitekick"
    assert (target / "data" / "chips.v1.tsv").is_file()
    assert (target / "chips" / "chips.bmp").is_file()
    assert (target / "backgrounds" / "backgrounds.bmp").is_file()
    assert (target / "state").is_dir()
    assert (target / "sync").is_dir()
    assert copied["chips"] == 1
    assert copied["backgrounds"] == 1


def test_load_state_reads_current_equipment_dump_and_appearance(pack):
    (pack / "state").mkdir()
    header = bytearray(48)
    header[:4] = b"SKS2"
    struct.pack_into("<IIHH", header, 4, 725, 410, 3, 48)
    struct.pack_into(
        "<8H", header, 16,
        3, 0xffff, 9, 12, 0xffff, 18, 21, 0xffff,
    )
    struct.pack_into("<H", header, 32, 27)
    struct.pack_into("<I", header, 36, 123456)
    header[40] = 2
    header[41] = 4
    (pack / sitekick.SAVE_FILE).write_bytes(
        bytes(header) + struct.pack("<3H", 3, 9, 12)
    )

    state = sitekick.load_state(pack)
    assert state.exists is True
    assert state.xp == 725
    assert state.coins == 410
    assert state.owned == (3, 9, 12)
    assert state.equipped[1] is None
    assert state.equipped[5] == 18
    assert state.dump_id == 27
    assert state.dump_ready_at == 123456
    assert state.body_color == 2
    assert state.background == 4


def test_load_state_defaults_appearance_for_older_save(pack):
    (pack / "state").mkdir()
    header = bytearray(40)
    header[:4] = b"SKS2"
    struct.pack_into("<IIHH", header, 4, 1, 2, 0, 40)
    struct.pack_into("<8H", header, 16, *([0xffff] * 8))
    struct.pack_into("<H", header, 32, 0xffff)
    (pack / sitekick.SAVE_FILE).write_bytes(header)

    state = sitekick.load_state(pack)
    assert state.body_color == 0
    assert state.background == 0


def test_classic_yellow_and_artist_backgrounds_are_packaged():
    root = (
        Path(__file__).resolve().parents[2]
        / "assets" / "ipodjs" / "rockbox" / "sitekick"
    )
    assert sitekick.BODY_COLORS[-1] == (
        "Classic Yellow", "sitekick-color-6.bmp"
    )
    assert sitekick.BACKGROUNDS[-3:] == (
        ("Beatles Crosswalk", "beatles-crosswalk"),
        ("Beatles Pepperland", "beatles-pepperland"),
        ("Beatles Rooftop", "beatles-rooftop"),
    )
    for path in (
        root / "base" / "sitekick-color-6.bmp",
        root / "backgrounds" / "stage-6.bmp",
        root / "backgrounds" / "pane-6.bmp",
        root / "backgrounds" / "stage-7.bmp",
        root / "backgrounds" / "pane-7.bmp",
        root / "backgrounds" / "stage-8.bmp",
        root / "backgrounds" / "pane-8.bmp",
        root / "backgrounds" / "stage-9.bmp",
        root / "backgrounds" / "pane-9.bmp",
        root / "backgrounds" / "stage-10.bmp",
        root / "backgrounds" / "pane-10.bmp",
        root / "backgrounds" / "stage-11.bmp",
        root / "backgrounds" / "pane-11.bmp",
    ):
        assert path.is_file()

    source = (
        Path(__file__).resolve().parents[2] / "apps" / "plugins" / "sitekick.c"
    ).read_text(encoding="utf-8")
    assert "#define SK_BODY_COLOR_COUNT 7" in source
    assert "#define SK_BACKGROUND_COUNT 12" in source
    assert '"Classic Yellow"' in source
    assert '"Oliver Scrapyard", "Emma Neon Box"' in source
    assert '"Oliver Alone Crowd"' in source
    assert '"Beatles Crosswalk", "Beatles Pepperland", "Beatles Rooftop"' \
        in source


def test_load_state_accepts_expanded_512_chip_catalogue(pack):
    (pack / "state").mkdir()
    header = bytearray(48)
    header[:4] = b"SKS2"
    owned = tuple(range(382))
    struct.pack_into("<IIHH", header, 4, 1, 2, len(owned), 48)
    struct.pack_into("<8H", header, 16, *([0xffff] * 8))
    struct.pack_into("<H", header, 32, 0xffff)
    (pack / sitekick.SAVE_FILE).write_bytes(
        bytes(header) + struct.pack(f"<{len(owned)}H", *owned)
    )

    assert sitekick.load_state(pack).owned == owned


def test_ipod_exclusives_cover_every_rarity_and_reference():
    root = (
        Path(__file__).resolve().parents[2]
        / "assets" / "ipodjs" / "rockbox" / "sitekick"
    )
    chips = sitekick.load_chips(root)
    exclusives = {chip_id: chips[chip_id] for chip_id in range(900, 969)}

    assert {chip.rarity for chip in exclusives.values()} == {
        "common", "rare", "legendary"}
    assert {chip.name for chip in exclusives.values()} >= {
        "iPod Listener",
        "Abbey Road Disc",
        "Emma Stage Jacket",
        "Rockbox Badge",
        "Oliver Bowl Cut",
        "Oliver Shades",
        "Oliver LP3 Jacket",
        "Fox Ears",
        "Hound Friendship Collar",
        "Villains Bob",
        "Villains Ringer Tee",
        "Box Pigtails",
        "Box Stripe Sweater",
        "Madly Long Locks",
        "Madly Junkyard Fit",
        "Madly Scrap Heart",
        "Camden Tousle",
        "Lucky Moustache",
        "Joy Ponytail",
        "Camden Plaid Fit",
        "The Karma List",
        "Good Karma Halo",
        "Peace Sign Pair",
        "Double Thumbs Up",
        "Rock Horns Pair",
        "Turbo Bowl Cut",
        "Turbo Red Shades",
        "Turbo Windbreaker Fit",
        "Wall Brick Halo",
        "Prism Beam Shades",
        "Hybrid Mech Wings",
        "Meteora Spray Hoodie",
        "Riot Scribble Bob",
        "Riot Orange Jacket",
        "Sunnyvale Shades",
        "Trailer Park Work Shirt",
        "Playground Cap",
        "Recess Backpack",
        "Mall Headphones",
        "Food Court Hoodie",
        "Rooftop Shag",
        "Round Beat Glasses",
        "Pepper Moustache",
        "Pepper Band Jacket",
        "Rooftop Shearling",
        "Violin Beat Bass",
        "Submarine Captain",
        "Beat Drumsticks",
        "Trainer League Cap",
        "Poke Ball Toss",
        "GO Field Jacket",
        "GO Research Coat",
        "GO Raid Pass",
        "Pocket Dex",
        "GO Lure Halo",
        "GO AR Visor",
        "Rocket Grunt Fit",
        "Great Ball Toss",
        "GO Map Explorer",
    }
    assert all((root / "chips" / f"{chip_id:04d}.bmp").is_file()
               for chip_id in exclusives)


def test_ipod_costumes_have_real_neck_openings_and_seated_hair():
    root = (
        Path(__file__).resolve().parents[2]
        / "assets" / "ipodjs" / "rockbox" / "sitekick"
    )
    chips = sitekick.load_chips(root)

    for chip_id, sample in (
        (903, (34, 8)), (909, (42, 13)), (914, (42, 10)),
        (918, (44, 10)), (920, (43, 10)), (922, (46, 10)),
        (924, (47, 10)), (929, (47, 10)), (937, (48, 10)),
        (953, (48, 10)), (954, (48, 10)),
        (960, (48, 10)), (961, (48, 10)), (966, (48, 10)),
    ):
        alpha = sitekick._load_alpha_bmp(
            root / "chips" / f"{chip_id:04d}.bmp"
        ).getchannel("A")
        assert alpha.getpixel(sample) == 0
    assert chips[909].z == -1
    assert chips[914].z == -1
    assert chips[918].z == -1
    assert chips[920].z == -1
    assert chips[922].z == -1
    assert chips[924].z == -1
    assert chips[925].z == -3
    assert chips[929].z == -1
    assert chips[931].z == -3
    assert chips[937].z == -1
    assert chips[953].z == -1
    assert chips[954].z == -1
    assert chips[960].z == -1
    assert chips[961].z == -1
    assert chips[966].z == -1
    assert chips[901].ay == -44
    assert chips[912].ay == -46
    assert chips[915].ay == -52
    assert chips[916].ay == -42
    assert chips[919].ay == -43
    assert chips[921].ay == -44
    assert chips[923].ay == -43
    assert chips[926].ay == -45
    assert chips[928].ay == -48
    assert chips[935].ay == -45
    assert chips[950].ay == -46
    assert chips[956].ay == -47
    assert chips[958].ay == -45


def test_beatles_eras_use_natural_slots_and_all_rarities():
    root = (
        Path(__file__).resolve().parents[2]
        / "assets" / "ipodjs" / "rockbox" / "sitekick"
    )
    chips = sitekick.load_chips(root)
    expected = {
        950: ("hair", "common", 4),
        951: ("eyes", "common", 5),
        952: ("face", "rare", 5),
        953: ("shell", "legendary", -1),
        954: ("shell", "rare", -1),
        955: ("accessory", "legendary", 3),
        956: ("hair", "rare", 4),
        957: ("arms", "common", 2),
    }
    for chip_id, (slot, rarity, z) in expected.items():
        chip = chips[chip_id]
        assert (chip.slot, chip.rarity, chip.z, chip.worn) == (
            slot, rarity, z, True
        )

    assert {chips[chip_id].rarity for chip_id in expected} == {
        "common", "rare", "legendary"
    }
    series = (root / sitekick.SERIES_FILE).read_text(encoding="utf-8")
    assert (
        "beatles-eras\tBeatles Eras\t"
        "901,904,908,950,951,952,953,954,955,956,957"
    ) in series


def test_pokemon_and_go_chips_use_natural_slots_and_all_rarities():
    root = (
        Path(__file__).resolve().parents[2]
        / "assets" / "ipodjs" / "rockbox" / "sitekick"
    )
    chips = sitekick.load_chips(root)
    expected = {
        958: ("hair", "common", 4),
        959: ("accessory", "common", 3),
        960: ("shell", "rare", -1),
        961: ("shell", "rare", -1),
        962: ("accessory", "rare", 3),
        963: ("accessory", "legendary", 3),
        964: ("aura", "legendary", -3),
        965: ("eyes", "common", 5),
        966: ("shell", "legendary", -1),
        967: ("accessory", "rare", 3),
        968: ("aura", "legendary", -3),
    }
    for chip_id, (slot, rarity, z) in expected.items():
        chip = chips[chip_id]
        assert (chip.slot, chip.rarity, chip.z, chip.worn) == (
            slot, rarity, z, True
        )

    assert {chips[chip_id].rarity for chip_id in expected} == {
        "common", "rare", "legendary"
    }
    series = (root / sitekick.SERIES_FILE).read_text(encoding="utf-8")
    assert "pokemon-core\tPokemon\t958,959,963,966,967" in series
    assert "pokemon-go\tPokemon GO\t960,961,962,964,965,968" in series


def test_my_name_is_earl_series_uses_natural_wearable_slots():
    root = (
        Path(__file__).resolve().parents[2]
        / "assets" / "ipodjs" / "rockbox" / "sitekick"
    )
    chips = sitekick.load_chips(root)
    expected = {
        926: ("hair", "common"),
        927: ("face", "common"),
        928: ("hair", "rare"),
        929: ("shell", "rare"),
        930: ("accessory", "legendary"),
        931: ("aura", "legendary"),
    }
    for chip_id, (slot, rarity) in expected.items():
        chip = chips[chip_id]
        assert (chip.slot, chip.rarity, chip.worn) == (slot, rarity, True)

    series = (root / sitekick.SERIES_FILE).read_text(encoding="utf-8")
    assert "my-name-is-earl\tMy Name Is Earl\t926,927,928,929,930,931" \
        in series


def test_packaged_oliver_code_includes_every_oliver_era():
    root = (
        Path(__file__).resolve().parents[2]
        / "assets" / "ipodjs" / "rockbox" / "sitekick"
    )
    grants = sitekick.redeem(root, "Oliver")
    assert {grant.value for grant in grants if grant.kind == "grant"} == {
        912, 913, 914, 923, 924, 925, 935, 936, 937
    }


def test_hand_options_and_turbo_series_are_naturally_slotted():
    root = (
        Path(__file__).resolve().parents[2]
        / "assets" / "ipodjs" / "rockbox" / "sitekick"
    )
    chips = sitekick.load_chips(root)
    expected = {
        932: ("arms", "common", 2),
        933: ("arms", "rare", 2),
        934: ("arms", "legendary", 2),
        935: ("hair", "common", 4),
        936: ("eyes", "rare", 5),
        937: ("shell", "legendary", -1),
    }
    for chip_id, (slot, rarity, z) in expected.items():
        chip = chips[chip_id]
        assert (chip.slot, chip.rarity, chip.z, chip.worn) == (
            slot, rarity, z, True
        )

    series = (root / sitekick.SERIES_FILE).read_text(encoding="utf-8")
    assert "hand-gestures\tHand Gestures\t932,933,934" in series
    assert "oliver-turbo\tOliver Tree Turbo\t935,936,937" in series


def test_rockpod_media_wave_uses_natural_slots_and_all_rarities():
    root = (
        Path(__file__).resolve().parents[2]
        / "assets" / "ipodjs" / "rockbox" / "sitekick"
    )
    chips = sitekick.load_chips(root)
    expected = {
        938: ("aura", "legendary", -3),
        939: ("eyes", "rare", 5),
        940: ("arms", "legendary", 2),
        941: ("shell", "rare", -1),
        942: ("hair", "common", 4),
        943: ("shell", "rare", -1),
        944: ("eyes", "common", 5),
        945: ("shell", "rare", -1),
        946: ("hair", "common", 4),
        947: ("arms", "rare", 2),
        948: ("arms", "common", 2),
        949: ("shell", "legendary", -1),
    }
    for chip_id, (slot, rarity, z) in expected.items():
        chip = chips[chip_id]
        assert (chip.slot, chip.rarity, chip.z, chip.worn) == (
            slot, rarity, z, True
        )

    assert {chips[chip_id].rarity for chip_id in expected} == {
        "common", "rare", "legendary"
    }
    series = (root / sitekick.SERIES_FILE).read_text(encoding="utf-8")
    for row in (
        "the-wall\tThe Wall\t938,939",
        "hybrid-meteora\tHybrid Theory + Meteora\t940,941",
        "riot\tRiot!\t942,943",
        "sunnyvale\tSunnyvale\t944,945",
        "recess\tRecess\t946,947",
        "6teen\t6teen\t948,949",
    ):
        assert row in series

    for chip_id, sample in (
        (941, (48, 10)),
        (943, (48, 10)),
        (945, (48, 10)),
        (949, (48, 10)),
    ):
        alpha = sitekick._load_alpha_bmp(
            root / "chips" / f"{chip_id:04d}.bmp"
        ).getchannel("A")
        assert alpha.getpixel(sample) == 0


def test_sitekick_plugin_has_persistent_native_minigames():
    source = (
        Path(__file__).resolve().parents[2]
        / "apps" / "plugins" / "sitekick.c"
    ).read_text(encoding="utf-8")

    assert "SK_SCENE_GAMES" in source
    assert "Beat Bounce" in source
    assert "Chip Match" in source
    assert "sk_award_game" in source
    assert "sk_write_save();" in source
    assert "sk_publish_preview(true);" in source
    assert "PCM_MIXER_CHAN_BEEP" in source
    assert "pluginlib_getaction(MAX(1, HZ / 30)" in source
    assert "sk_prepare_game_icons" in source
    assert "sk_draw_cached_icon" in source
    assert "static const uint16_t ids[3] = { 15, 18, 21 };" in source


def test_sitekick_workshop_animation_is_tick_driven_and_cached():
    source = (
        Path(__file__).resolve().parents[2]
        / "apps" / "plugins" / "sitekick.c"
    ).read_text(encoding="utf-8")

    assert "static fb_data sk_float_data" in source
    assert "*rb->current_tick * 8" in source
    assert "lcd_bitmap_transparent_part" in source
    assert "button == ACTION_NONE && sk_scene == SK_SCENE_WORKSHOP" in source
    assert "MAX(1, HZ / 12)" in source
    assert "rb->core_alloc(" not in source


def test_sitekick_desktop_mode_has_mouse_draggable_persistent_chips():
    root = Path(__file__).resolve().parents[2]
    source = (root / "apps/plugins/sitekick.c").read_text(encoding="utf-8")

    assert (
        'parameter &&\n        '
        '!rb->strcmp((const char *)parameter, "-desktop")'
    ) in source
    assert 'ROCKBOX_DIR "/host-pointer"' in source
    assert "sk_dm_drag_chip" in source
    assert "sk_dm_drag_source_slot" in source
    assert "sk_dm_hit_equip_slot" in source
    assert "sk_dm_hit_collection_chip" in source
    assert "sk_equip(target, sk_dm_drag_chip);" in source
    assert "sk_write_save();" in source
    assert "rb->plugin_get_buffer(&size)" in source
    assert "plugin_get_audio_buffer" not in source
    assert "rb->core_alloc(" not in source


def test_packaged_sitekick_desktop_icons_have_expected_rga_sizes():
    root = (
        Path(__file__).resolve().parents[2]
        / "assets/ipodjs/rockbox/sitekick/desktop"
    )
    expected = {
        "icon.32x32.rga": (32, 32),
        "icon-dock-34.34x34.rga": (34, 34),
        "icon-dock-38.38x38.rga": (38, 38),
        "icon.64x64.rga": (64, 64),
        "icon-dock-66.66x66.rga": (66, 66),
        "icon-dock-70.70x70.rga": (70, 70),
    }

    for name, size in expected.items():
        payload = (root / name).read_bytes()
        magic, width, height = struct.unpack("<4sHH", payload[:8])
        assert (magic, width, height) == (b"RGA1", *size)
        assert len(payload) == 8 + width * height * 3


def test_render_current_preview_is_native_right_pane_size(tmp_path):
    root = (
        Path(__file__).resolve().parents[2]
        / "assets" / "ipodjs" / "rockbox" / "sitekick"
    )
    output = tmp_path / "current.bmp"
    sitekick.render_current_preview(root, output_path=output)
    image = Image.open(output).convert("RGB")
    assert image.size == (174, 240)
    assert image.getpixel((173, 0)) == (76, 11, 100)
    assert Image.open(tmp_path / "pane-background.bmp").size == (174, 240)
    assert Image.open(tmp_path / "current-float.bmp").size == (154, 139)


def test_render_current_preview_uses_saved_background(tmp_path):
    root = (
        Path(__file__).resolve().parents[2]
        / "assets" / "ipodjs" / "rockbox" / "sitekick"
    )
    output = tmp_path / "current.bmp"
    state = sitekick.SitekickState(background=4, body_color=2)
    sitekick.render_current_preview(root, state=state, output_path=output)
    pane = Image.open(tmp_path / "pane-background.bmp").convert("RGB")
    floating = Image.open(tmp_path / "current-float.bmp").convert("RGB")
    template = Image.open(root / "backgrounds" / "pane-4.bmp").convert("RGB")
    assert pane.getpixel((173, 100)) == template.getpixel((173, 100))
    assert floating.getpixel((0, 0)) == (255, 0, 255)
