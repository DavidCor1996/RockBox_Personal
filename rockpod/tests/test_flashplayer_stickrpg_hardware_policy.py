from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
FLASHPLAYER_SOURCE = (
    REPO_ROOT / "apps" / "plugins" / "flashplayer" / "flashplayer.cpp"
)
SPRITE_SOURCE = (
    REPO_ROOT
    / "apps"
    / "plugins"
    / "flashplayer"
    / "gameswf"
    / "gameswf"
    / "gameswf_sprite.cpp"
)
MENU_GATE = REPO_ROOT / "tools" / "stickrpg_authentic_menu_sim_gate.py"
SAVE_GATE = REPO_ROOT / "tools" / "stickrpg_save_sim_gate.py"
OBJECT_SOURCE = (
    REPO_ROOT
    / "apps"
    / "plugins"
    / "flashplayer"
    / "gameswf"
    / "gameswf"
    / "gameswf_object.cpp"
)
SHAREDOBJECT_SOURCE = (
    REPO_ROOT
    / "apps"
    / "plugins"
    / "flashplayer"
    / "gameswf"
    / "gameswf"
    / "gameswf_as_classes"
    / "as_sharedobject.cpp"
)
PLAYER_SOURCE = (
    REPO_ROOT
    / "apps"
    / "plugins"
    / "flashplayer"
    / "gameswf"
    / "gameswf"
    / "gameswf_player.cpp"
)


def test_stickrpg_hardware_defaults_preserve_the_authored_timeline():
    source = FLASHPLAYER_SOURCE.read_text(encoding="utf-8")
    policy_start = source.index(
        "g.stickrpg_fast_load = "
        "g.input_profile == FLASH_INPUT_PROFILE_STICKRPG;"
    )
    simulator_overrides = source.index("#ifdef SIMULATOR", policy_start)
    hardware_policy = source[policy_start:simulator_overrides]

    assert "g.prime_stickrpg = false;" in hardware_policy
    assert "g.stickrpg_gameplay_shortcut_enabled = false;" in hardware_policy
    assert "g.prime_stickrpg = g.stickrpg_fast_load;" not in hardware_policy
    assert (
        "g.stickrpg_gameplay_shortcut_enabled = g.stickrpg_fast_load;"
        not in hardware_policy
    )


def test_hardware_log_records_the_effective_stickrpg_policy():
    source = FLASHPLAYER_SOURCE.read_text(encoding="utf-8")

    assert (
        'flash_logf("stickrpg policy profile=%d fast=%d prime=%d "'
        in source
    )

    log_function = source[
        source.index("static void flash_logf(") :
        source.index("static void flash_log_open(")
    ]
    assert "if (!g.runtime_ready)\n        return;" not in log_function


def test_nested_sprites_restore_their_owned_actionscript_environment():
    source = SPRITE_SOURCE.read_text(encoding="utf-8")
    advance = source[
        source.index("void sprite_instance::advance(float delta_time)") :
        source.index("void sprite_instance::display()", source.index(
            "void sprite_instance::advance(float delta_time)"
        ))
    ]
    on_event = source[
        source.index("bool sprite_instance::on_event(const event_id& id)") :
        source.index("const char* sprite_instance::call_method_args", source.index(
            "bool sprite_instance::on_event(const event_id& id)"
        ))
    ]

    repair = (
        "if (m_as_environment.get_target() != this)\n"
        "\t\t{\n"
        "\t\t\tm_as_environment.set_target(this);"
    )
    assert repair in advance
    assert repair in on_event


def test_stickrpg_restores_the_original_container_intro_callback():
    source = FLASHPLAYER_SOURCE.read_text(encoding="utf-8")

    assert "static void stickrpg_host_done_intro(" in source
    assert '"doneIntro", host_callback' in source
    assert (
        'root_sprite->get_environment()->set_local(\n'
        '                    "doneIntro", host_callback);'
        in source
    )
    assert "stickrpg authored UI ready" in source


def test_stock_ipod_controls_use_authored_hit_targets_and_wheel_sectors():
    source = FLASHPLAYER_SOURCE.read_text(encoding="utf-8")

    assert "collect_runtime_mouse_targets(" in source
    assert "get_topmost_mouse_entity(" in source
    assert "PIXELS_TO_TWIPS((float)stage_x)" in source
    assert "snap_stickrpg_cursor(0, -1);" in source
    assert "snap_stickrpg_cursor(0, 1);" in source
    assert "static const unsigned char wheel_map[8]" in source
    assert "STICKRPG_DIR_UP | STICKRPG_DIR_RIGHT" in source
    assert 'flash_logf("stickrpg long Menu exit");' in source


def test_authentic_menu_gate_disables_shortcuts_and_uses_original_button_path():
    source = MENU_GATE.read_text(encoding="utf-8")

    assert '"FLASHPLAYER_PRIME_STICKRPG": "0"' in source
    assert '"FLASHPLAYER_STICKRPG_SHORTCUT": "0"' in source
    assert '"stickrpg host doneIntro pregame=1 black=1 intro=1"' in source
    assert '"stickrpg authored UI ready"' in source
    assert '"stickrpg authored UI presented"' in source
    assert '"stickrpg cursor snap dir=0,-1"' in source
    assert '"stickrpg authored UI state"' in source


def test_hardware_progress_splits_first_advance_display_and_lcd_update():
    source = FLASHPLAYER_SOURCE.read_text(encoding="utf-8")

    for marker in (
        "first advance begin",
        "first advance complete",
        "first display begin",
        "first display complete",
        "first lcd update begin",
        "first lcd update complete",
        "authored menu clips ready",
        "authored menu presented",
    ):
        assert f'"{marker}"' in source


def test_hardware_frame_log_does_not_use_unsupported_float_varargs():
    source = FLASHPLAYER_SOURCE.read_text(encoding="utf-8")
    frame_log = source[
        source.index('flash_logf("frame n=') :
        source.index(");", source.index('flash_logf("frame n=')) + 2
    ]

    assert "%f" not in frame_log
    assert "%.1f" not in frame_log
    assert "p10=" in frame_log
    assert '"curve max pixel error x100=%d"' in source


def test_plain_object_lookup_recovery_does_not_change_movie_clip_semantics():
    source = OBJECT_SOURCE.read_text(encoding="utf-8")

    assert "if (!is(AS_CHARACTER))" in source
    assert 'strcmp(it->first.c_str(), name.c_str()) == 0' in source
    assert "tu_string::stricmp(it->first.c_str(), name.c_str())" not in source


def test_sharedobject_matches_flash_close_and_nested_save_semantics():
    sharedobject = SHAREDOBJECT_SOURCE.read_text(encoding="utf-8")
    player = PLAYER_SOURCE.read_text(encoding="utf-8")

    assert 'out = "RBSO2\\n";' in sharedobject
    assert "child->is(AS_ARRAY) ? 'A' : 'O'" in sharedobject
    assert "serialize_object(&out, data, 0, &count);" in sharedobject
    assert "static void flush_all();" not in sharedobject
    assert "void as_sharedobject::flush_all()" in sharedobject
    assert "as_sharedobject::flush_all();" in player


def test_sharedobject_host_write_is_transactional_and_recovers_backup():
    source = FLASHPLAYER_SOURCE.read_text(encoding="utf-8")
    writer = source[
        source.index('extern "C" int flashplayer_sharedobject_write') :
        source.index('extern "C" void flashplayer_trace_sharedobject')
    ]
    reader = source[
        source.index('extern "C" int flashplayer_sharedobject_read') :
        source.index('extern "C" int flashplayer_sharedobject_write')
    ]

    assert '"%s.tmp", path' in writer
    assert '"%s.bak", path' in writer
    assert "had_previous = rb->rename(path, backup) >= 0;" in writer
    assert "rb->rename(temporary, path)" in writer
    assert "rb->rename(backup, path);" in writer
    assert 'rb->snprintf(backup, sizeof(backup), "%s.bak", path);' in reader


def test_save_gate_uses_original_save_and_continue_across_processes():
    source = SAVE_GATE.read_text(encoding="utf-8")

    assert '"FLASHPLAYER_AUTORUN_CALL_SAVE_FRAME": "150"' in source
    assert '"stickrpg entered gameplay after character creation"' in source
    assert '"O\\tmyObj\\n"' in source
    assert '"A\\tobjArray\\n"' in source
    assert "value_records < 75" in source
    assert '"sharedobject load name=xgensrpg"' in source
    assert '"sharedobject load-miss name=xgensrpg"' in source
