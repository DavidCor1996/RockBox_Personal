"""USB visual port: provenance, complete animation and bounded memory."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text()


def body(source, name):
    return source.split(name + "(", 1)[1].split("\n}", 1)[0]


def test_sync_badge_is_not_the_connector_mask():
    for tool in ("ipod_classic_resource_extract", "verify_ipod_classic_resource_dump"):
        registry = read(f"tools/{tool}.py")
        assert '"disk-mode-sync-icon": 392,' in registry
        assert '"disk-mode-connected-icon": 562,' in registry
    symbols = ROOT / "assets/ipodjs/apple/retailos-2.0.4/image-symbol-map.tsv"
    if symbols.exists():
        line = next(line for line in symbols.read_text().splitlines()
                    if "\tDiskModeImage_SyncIcon\t" in line)
        assert "\t392\t0x0dad0a9a\t112\t112\t" in line


def test_usb_reuses_transition_workspace_and_loads_before_ack():
    ui = read("apps/gui/ipodjs_ui.c")
    prepare = body(ui, "ipodjs_ui_prepare_retailos_usb")
    assert "ipodjs_ui_transition_cancel()" in prepare
    assert "11, raw, sizeof(ipodjs_ui_animation_frames)" in prepare
    assert "392, ipodjs_ui_animation_new" in prepare
    assert "raw[i * 3 + 2] != 255" in prepare
    assert "sizeof(fb_data) > 3" in prepare
    assert "IPODJS_RETAILOS_DISK_MODE_SYNC_ARROWS" in prepare
    for forbidden in ("core_alloc(", "audio_stop(", "plugin_get_audio_buffer("):
        assert forbidden not in prepare
    usb = body(read("apps/gui/usb_screen.c"), "gui_usb_screen_run")
    assert usb.index("ipodjs_ui_usb_prepare()") < usb.index("font_disable_all()")
    assert usb.index("ipodjs_ui_usb_prepare()") < usb.index("usb_acknowledge(")


def test_usb_draw_is_cached_source_art_with_no_procedural_substitutes():
    ui = read("apps/gui/ipodjs_ui.c")
    draw = body(ui, "ipodjs_ui_draw_usb_connected")
    for forbidden in ("glass_gradient", "rounded_gradient", "draw_usb_battery",
                      "draw_usb_lock", "draw_usb_sync_mark", "load_", "open(",
                      "font_load", "core_alloc", "ui_engine_dark_mode"):
        assert forbidden not in draw
    assert "&cache->badge, 104, 46" in draw
    assert "hold_background" in draw
    assert "ipodjs_ui_draw_retailos_battery" in draw
    assert "white_lock" in draw
    assert "122, 62, 76, 76" in draw
    assert 'cache->valid ? "iPod" : "USB"' in draw
    mask = body(ui, "ipodjs_ui_draw_usb_sync_asset")
    assert "122, 62, FB_RGBPACK(0, 0, 0)" in mask
    frame = body(ui, "ipodjs_ui_usb_sync_frame")
    assert "cache->sync_arrows.frame_count" in frame
    assert "elapsed * 12u / HZ" in frame


def test_usb_font_roles_are_warmed_before_storage_handoff():
    ui = read("apps/gui/ipodjs_ui.c")
    prepare = body(ui, "ipodjs_ui_usb_prepare")
    assert "ipodjs_ui_prepare_retailos_fonts()" in prepare
    assert "ipodjs_ui_usb_small_font()" in prepare
    assert "ipodjs_ui_retailos_font(true)" in prepare
    assert '"Eject before disconnecting."' in prepare
    assert '"16-Helvetica-RetailOS-Apple.fnt"' in read(
        "tools/prepare_ipod_classic_fonts.py")
