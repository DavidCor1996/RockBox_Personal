"""Static safety checks for iPodJS cache and memory maintenance."""

from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def _read(rel_path):
    return (REPO_ROOT / rel_path).read_text(encoding="utf-8", errors="replace")


def _function_body(source, name, next_name):
    return source.split(name, 1)[1].split(next_name, 1)[0]


def test_ipodjs_quick_settings_exposes_cache_memory_submenu_and_scrolling():
    root_menu = _read("apps/root_menu.c")

    assert "IPODJS_QS_CACHE_MEMORY" in root_menu
    assert 'return "Cache & Memory";' in root_menu
    assert 'strmemccpy(buf, "Open", buf_size);' in root_menu
    assert "root_menu_video_qs_row_height" in root_menu
    assert "root_menu_video_qs_visible_rows" in root_menu
    assert "root_menu_video_qs_top" in root_menu
    assert "root_menu_video_draw_qs_scrollbar(top, visible);" in root_menu
    assert "root_menu_video_cache_memory_menu(changed_settings," in root_menu


def test_ipodjs_refresh_only_invalidates_owned_disposable_caches():
    root_menu = _read("apps/root_menu.c")
    refresh = _function_body(
        root_menu,
        "root_menu_video_clear_disposable_caches",
        "root_menu_video_draw_qs_icon",
    )

    for required in (
        "ipodjs_ui_label_cache_reset();",
        "root_menu_video_clear_asset_caches();",
        "root_menu_video_clear_preview_caches();",
        "root_menu_video_clear_database_view_caches();",
        "video_thumb_lookup_cache[i].valid = false;",
        "video_thumb_bitmap_cache[i].valid = false;",
    ):
        assert required in refresh

    for forbidden in (
        "audio_stop(",
        "audio_flush_and_reload_tracks(",
        "plugin_get_audio_buffer(",
        "plugin_release_audio_buffer(",
        "core_alloc_maximum(",
        "dircache_disable(",
        "tagcache_shutdown(",
        "settings_save(",
        "status_save(",
        "remove(",
        "root_menu_video_aa_slot",
        "playback_current_aa_hid(",
    ):
        assert forbidden not in refresh


def test_ipodjs_refresh_avoids_zeroing_large_preview_pixel_storage():
    root_menu = _read("apps/root_menu.c")
    cache_switch = root_menu.split(
        "static void root_menu_video_preview_use_cache", 1
    )[1].split("static const char *root_menu_video_preview_asset_name", 1)[0]
    clear_preview = _function_body(
        root_menu,
        "root_menu_video_clear_preview_caches",
        "root_menu_video_clear_database_view_caches",
    )

    assert "memset(&root_menu_video_preview_storage" not in cache_switch
    assert "memset(&root_menu_video_preview_storage" not in clear_preview
    assert "root_menu_video_preview_slots[i].valid = false;" in clear_preview
    assert "root_menu_video_menu_preview_slots[i].valid = false;" in clear_preview
    assert "root_menu_video_preview_source_caches[i].loaded = false;" in clear_preview


def test_ipodjs_ram_reset_uses_guarded_graceful_reboot():
    root_menu = _read("apps/root_menu.c")
    request = root_menu.split("static bool root_menu_request_reboot(void)", 2)[2]
    request = request.split("static void root_menu_open_power_menu", 1)[0]
    maintenance = root_menu.split(
        "static bool root_menu_video_cache_memory_menu", 1
    )[1].split("static int root_menu_video_quick_settings", 1)[0]

    assert "charger_inserted()" in request
    assert "charging_splash();" in request
    assert "sys_reboot();" in request
    assert "system_reboot();" not in request
    assert "root_menu_video_confirm_reboot()" in maintenance
    assert maintenance.index("settings_save();") < maintenance.index(
        "root_menu_request_reboot()"
    )
    assert maintenance.index("status_save(false);") < maintenance.index(
        "root_menu_request_reboot()"
    )


def test_ipodjs_cache_assets_and_spec_are_packaged_sources():
    root_menu = _read("apps/root_menu.c")
    spec = _read("docs/ipodjs-quick-settings-cache-memory-spec.md")

    assert "IPODJS_QS_GLYPH_CACHE_MEMORY" in root_menu
    assert (REPO_ROOT / "apps/gui/ipodjs_qs_icons.h").is_file()
    assert "Refresh UI Cache" in spec
    assert "Restart to Clear RAM" in spec
    assert "must never seize, shrink, clear, or release" in spec
