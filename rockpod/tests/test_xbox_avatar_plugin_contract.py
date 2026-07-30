from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
SOURCE = (REPO / "apps/plugins/achievements.c").read_text(encoding="utf-8")


def test_hold_menu_and_exact_return_contract_is_present():
    assert "SCREEN_AVATAR" in SOURCE
    assert "SCREEN_AVATAR_SETTINGS" in SOURCE
    assert "case ACTION_STD_QUICKSCREEN:" in SOURCE
    assert "avatar_return_screen = screen;" in SOURCE
    assert "screen = avatar_return_screen;" in SOURCE
    assert "wait_for_menu_release();" in SOURCE
    assert "ACTION_WPS_PLAY" in SOURCE
    assert "avatar_paused = !avatar_paused;" in SOURCE


def test_avatar_io_and_animation_follow_ipod_ui_steering():
    assert "avatar_clip_due" in SOURCE
    assert "rb->button_queue_count() == 0" in SOURCE
    assert "avatar_frame_started" in SOURCE
    assert "*rb->current_tick" in SOURCE
    assert "target %= avatar_frame_count" in SOURCE
    assert "avatar_crc32" in SOURCE
    assert "read_le32(avatar_clip_data + 16)" in SOURCE
    assert "plugin_get_audio_buffer" not in SOURCE
    assert "core_alloc" not in SOURCE


def test_dashboard_audio_isolated_to_beep_channel():
    assert "PCM_MIXER_CHAN_BEEP" in SOURCE
    assert "mixer_channel_set_buffer_hook(PCM_MIXER_CHAN_BEEP, NULL)" in SOURCE
    assert "mixer_channel_status(PCM_MIXER_CHAN_BEEP)" in SOURCE
    assert "PCM_MIXER_CHAN_PLAYBACK" not in SOURCE
    assert "mixer_set_frequency" not in SOURCE


def test_authentic_xbox_boot_is_cached_and_mixed_without_playback_memory():
    assert "run_xbox_boot();" in SOURCE
    assert "XBOX_BOOT_MAX_FRAMES 64" in SOURCE
    assert "boot-320x180.nfx" in SOURCE
    assert "boot-20000-mono.mulaw" in SOURCE
    assert "xbox_boot_pcm_more" in SOURCE
    assert "index_offset + XBOX_BOOT_INDEX_BYTES > sizeof(avatar_clip_data)" in SOURCE
    assert "rb->button_status() != BUTTON_NONE" in SOURCE
    assert "*rb->current_tick - started > HZ / 2" in SOURCE
    assert "PCM_MIXER_CHAN_BEEP" in SOURCE
    assert "PCM_MIXER_CHAN_PLAYBACK" not in SOURCE
    assert "plugin_get_audio_buffer" not in SOURCE


def test_real_pack_and_device_preferences_are_explicit():
    for clip in (
        "jump", "throw", "faint", "sit-idle", "punch", "kick", "walk",
        "turntable",
    ):
        assert f'"{clip}"' in SOURCE
    assert "AVATAR_CLIP_COUNT 8" in SOURCE
    assert "AVATAR_TURNTABLE_CLIP 7" in SOURCE
    assert "avatar_keyframes" in SOURCE
    assert "rotate_avatar(-1)" in SOURCE
    assert "rotate_avatar(1)" in SOURCE
    assert "ACH_AVATAR_PREFS" in SOURCE
    assert "motion\\t%s" in SOURCE
    assert "sounds_over_music\\t%s" in SOURCE
    assert "idle_emotes\\t%s" in SOURCE
