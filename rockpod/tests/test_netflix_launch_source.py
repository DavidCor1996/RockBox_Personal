from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def test_netflix_launch_pack_is_complete_and_packaged():
    launch = ROOT / "assets/ipodjs/rockbox/netflix/launch"
    frames = sorted(launch.glob("frame-*.320x180x24.bmp"))

    assert len(frames) == 40
    assert all(frame.stat().st_size > 170_000 for frame in frames)
    assert (launch / "intro-44100-stereo.pcm").stat().st_size > 500_000
    assert (launch / "intro-320x180.rgb565").stat().st_size == 40 * 320 * 180 * 2
    assert 75_000 < (launch / "intro-20000-mono.mulaw").stat().st_size < 85_000
    assert "youtube.com/watch?v=GV3HUDMQ-F8" in (
        launch / "SOURCES.tsv"
    ).read_text()
    assert 'tree_copy("$ROOT/assets/ipodjs/rockbox"' in (
        ROOT / "tools/buildzip.pl"
    ).read_text()


def test_video_launch_uses_official_2013_ident_pack():
    launch = ROOT / "assets/ipodjs/rockbox/netflix/video-launch"
    source = ROOT / "assets/ipodjs/sources/netflix/video-launch"
    frames = sorted(launch.glob("frame-*.320x180x24.bmp"))
    intro = (ROOT / "apps/plugins/netflix_intro.h").read_text()

    assert len(frames) == 12
    assert all(frame.stat().st_size > 170_000 for frame in frames)
    assert (launch / "intro-44100-stereo.pcm").stat().st_size > 500_000
    assert '"/ipodjs/netflix/video-launch"' in intro
    assert "NETFLIX_INTRO_FRAMES 12" in intro
    assert "youtube.com/watch?v=mnZeRCrMRFg" in (
        source / "SOURCES.tsv"
    ).read_text()


def test_netflix_categories_use_fixed_photographic_assets():
    categories = ROOT / "assets/ipodjs/rockbox/netflix/categories"
    source = ROOT / "assets/ipodjs/sources/netflix/categories"
    root_menu = (ROOT / "apps/root_menu.c").read_text()

    for name in ("movies", "tv-shows", "music-videos", "home-videos"):
        assert (categories / f"{name}.72x108x24.bmp").stat().st_size > 20_000
        assert (categories / f"{name}.96x144x24.bmp").stat().st_size > 40_000
        assert (source / f"{name}.source.png").stat().st_size > 1_000_000
    assert "VIDEO_LIST_MOVIES_ART_ID" in root_menu
    assert "VIDEO_LIST_SHOWS_ART_ID" in root_menu
    assert "VIDEO_LIST_MUSIC_ART_ID" in root_menu
    assert "VIDEO_LIST_HOME_ART_ID" in root_menu
    assert "first real poster in each rail" not in root_menu


def test_netflix_browser_marks_video_launch_parameters():
    source = (ROOT / "apps/root_menu.c").read_text()

    assert '"netflix:%s"' in source
    assert '"netflix-restart:%s"' in source
    assert "video_launch_entry(entry);" in source
    assert "switch (video_launch_entry(&state->entries[selected]))" in source
    assert "netflix_appearance = videos_netflix_appearance();" in source
    assert "if (ipodjs_ui_netflix_launch())" in source
    assert "VIDEO_LIST_NETFLIX_LAST" in source
    assert "video_netflix_resume_progress(&resume_entry)" in source
    assert "CONTINUE FROM %u%%" in source
    assert "const char * const labels[2]" in source


def test_netflix_season_paths_support_colons_in_show_titles():
    source = (ROOT / "apps/root_menu.c").read_text()
    season_browser = source.split(
        'strncmp(state->current_path, "virtual:season:", 15) == 0', 1
    )[1].split("static void videos_scan_folder", 1)[0]
    show_plot = source.split("video_netflix_load_show_detail", 1)[1].split(
        "static bool video_netflix_resume_progress", 1
    )[0]

    assert "strrchr(show_buf, ':')" in season_browser
    assert "strchr(show_buf, ':')" not in season_browser
    assert "strrchr(show_buf, ':')" in show_plot


def test_netflix_banners_are_full_width_aspect_fit_and_never_overlaid():
    source = (ROOT / "apps/root_menu.c").read_text()
    landing = source.split(
        "static void video_draw_netflix_landing(", 1
    )[1].split(
        "/* Resolve the series metadata", 1
    )[0]
    detail = source.split(
        "static void video_draw_netflix_detail(", 1
    )[1].split(
        "static int video_netflix_detail_screen", 1
    )[0]
    browser = source.split(
        "static int videos_netflix_browser(", 1
    )[1].split(
        "case ACTION_STD_CANCEL:", 1
    )[0]
    select_action = browser.split(
        "case ACTION_STD_OK:", 1
    )[1].split(
        "case ACTION_STD_CONTEXT:", 1
    )[0]
    context_action = browser.split("case ACTION_STD_CONTEXT:", 1)[1]

    assert "video_draw_netflix_banner(" not in landing
    assert "Browse remains cover-art-first" in landing
    assert "#define VIDEO_LIST_NETFLIX_BANNER_W 320" in source
    assert "#define VIDEO_LIST_NETFLIX_BANNER_H 180" in source
    assert "#define VIDEO_LIST_NETFLIX_BANNER_Y" in source
    assert "#define VIDEO_LIST_NETFLIX_FOOTER_Y" in source
    assert "if (video_draw_netflix_banner(display," in detail
    assert "video_draw_netflix_banner_footer" in detail
    assert "display->update();\n        return;" in detail
    assert "video_draw_netflix_banner_strip" not in source
    assert "video_draw_netflix_bar_logo" in source
    assert "notification_manager_set_banners_suppressed(true);" in source
    assert "notification_manager_set_banners_suppressed(false);" in source
    assert "(void)video_netflix_detail_screen(" in select_action
    assert "Episodes and movies open their asset-backed Details" in select_action
    assert "videos_netflix_enter_level" in select_action
    assert "video_netflix_detail_screen" in context_action
    assert "video_netflix_show_info_screen" in context_action


def test_netflix_detail_offers_resume_or_play_from_beginning():
    root_menu = (ROOT / "apps/root_menu.c").read_text()
    footer = root_menu.split(
        "static void video_draw_netflix_banner_footer(", 1
    )[1].split("static void video_draw_netflix_detail", 1)[0]
    player = (
        ROOT / "apps/plugins/mpegplayer/mpegplayer.c"
    ).read_text()
    raw = (ROOT / "apps/plugins/openh264_player.c").read_text()
    intro = (ROOT / "apps/plugins/netflix_intro.h").read_text()

    assert '"PLAY FROM BEGINNING"' in root_menu
    assert "video_netflix_resume_progress(&screen_entry)" in root_menu
    assert '"RESUME"' in footer
    assert '"PLAY FROM BEGINNING"' in footer
    assert "video_launch_entry_at(entry," in root_menu
    assert "selected_action == 1" in root_menu
    assert "NETFLIX_RESTART_PARAMETER_PREFIX" in intro
    assert "netflix_restart" in player
    assert "settings.resume_time = 0;" in player
    assert "netflix_restart" in raw
    assert "if (netflix_restart)" in raw
    assert "resume_frame = 0;" in raw


def test_netflix_app_launch_reuses_fixed_ui_workspace_and_beep_channel():
    source = (ROOT / "apps/gui/ipodjs_ui.c").read_text()

    assert "bool ipodjs_ui_netflix_launch(void)" in source
    assert "ipodjs_ui_animation_old" in source
    assert "ipodjs_ui_animation_new" in source
    assert "IPODJS_NETFLIX_PACK" in source
    assert "IPODJS_NETFLIX_SOUND" in source
    assert "PCM_MIXER_CHAN_BEEP" in source
    assert "PCM_MIXER_CHAN_PLAYBACK" not in source
    assert "core_alloc" not in source
    assert "audio_stop()" not in source
    assert "button_get_w_tmo(0)" in source
    assert source.count("ipodjs_netflix_beep_detach();") >= 2
    assert "mixer_channel_set_buffer_hook(PCM_MIXER_CHAN_BEEP, NULL)" in source
    assert "#define IPODJS_NETFLIX_PCM_CHUNK_FRAMES 512" in source
    assert "(size_t)IPODJS_NETFLIX_PCM_CHUNK_FRAMES" in source


def test_video_players_run_intro_after_taking_audio_buffer():
    intro = (ROOT / "apps/plugins/netflix_intro.h").read_text()
    stream = (ROOT / "apps/plugins/mpegplayer/stream_mgr.c").read_text()
    raw = (ROOT / "apps/plugins/openh264_player.c").read_text()

    assert "PCM_MIXER_CHAN_PLAYBACK" in intro
    assert "plugin_get_audio_buffer(&memsize)" in stream
    assert stream.index("plugin_get_audio_buffer(&memsize)") < stream.index(
        "netflix_intro_run(mem, memsize)"
    )
    assert raw.index("plugin_get_audio_buffer(&raw_pool_size)") < raw.index(
        "netflix_intro_run(raw_pool, raw_pool_size)"
    )


def test_netflix_playback_uses_yuv_theme_and_shared_video_volume():
    player = (
        ROOT / "apps/plugins/mpegplayer/mpegplayer.c"
    ).read_text()
    video_out = (
        ROOT / "apps/plugins/mpegplayer/video_out_rockbox.c"
    ).read_text()
    raw = (ROOT / "apps/plugins/openh264_player.c").read_text()

    assert "MPEG_NETFLIX_OVERLAY_H" in player
    assert "mpegplayer_yuv_overlay_draw" in player
    assert "vo_draw_yuv_overlay" in video_out
    assert "mpegplayer_yuv_overlay_y" in video_out
    assert "mpeg_netflix_draw_overlay" not in player
    assert "osd.netflix_layout = mpegplayer_netflix_launch" in player
    assert "stream_vo_set_clip(NULL);" in player
    assert (
        "if (mpegplayer_livetv_launch || mpegplayer_netflix_launch)"
    ) in player
    assert "RAW_NETFLIX_OVERLAY_H" in raw
    assert "raw_osd_draw_netflix" in raw
    assert "raw_blit_netflix_volume" in raw
    assert "raw_yuv_text_scaled" in raw
    assert "raw_osd_show_volume();" in raw
    assert "percent >= 100 ? RAW_VIDEO_VOLUME_SEGMENTS" in raw


def test_netflix_mpeg_and_rvp_launches_resume():
    player = (
        ROOT / "apps/plugins/mpegplayer/mpegplayer.c"
    ).read_text()
    settings = (
        ROOT / "apps/plugins/mpegplayer/mpeg_settings.c"
    ).read_text()
    raw = (ROOT / "apps/plugins/openh264_player.c").read_text()

    assert "if (mpegplayer_netflix_launch)" in settings
    assert "return MPEG_START_SEEK;" in settings
    assert "raw_resume_load(path, total_video_frames" in raw
    assert "raw_resume_save(path, last_position" in raw
    assert "raw_resume_clear(path);" in raw
    assert "record.frame <= 0" in raw
    assert "record.frame < fps * 3" not in raw
    assert "resume_time == 0" in settings
    assert "mpeg_resume_available(const char *filename)" in settings
    assert "!mpeg_resume_available(videofile)" in player
    assert "netflix_launch && resume_frame == 0" in raw


def test_native_launch_frames_preserve_source_detail():
    import struct
    from PIL import Image

    launch = ROOT / "assets/ipodjs/rockbox/netflix/launch"
    raw = (launch / "intro-320x180.rgb565").read_bytes()
    for frame in (8, 16, 24, 32):
        start = frame * 320 * 180 * 2
        values = struct.unpack_from("<57600H", raw, start)
        source = Image.open(launch / f"frame-{frame:02d}.320x180x24.bmp")
        error = 0
        for value, (red, green, blue) in zip(values, source.convert("RGB").getdata()):
            error += ((value >> 11) * 255 / 31 - red) ** 2
            error += (((value >> 5) & 63) * 255 / 63 - green) ** 2
            error += ((value & 31) * 255 / 31 - blue) ** 2
        assert error / (320 * 180 * 3) < 32
