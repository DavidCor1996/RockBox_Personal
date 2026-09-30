import json
import os

from PIL import Image
from PySide6.QtWidgets import QApplication

from services.rockbox_deploy import RockboxDeployService
from services.theme_designer import (
    DEFAULT_LOCKSCREEN_CLOCK,
    IPODJS_STOCK_FONT_LABEL,
    IPODJS_STOCK_FONT_REL,
    RIGHT_PANE_VIDEO_MAX_FRAMES,
    ThemeDesignerService,
    WEATHER_LOCKSCREEN_ART_HEIGHT,
)
from ui.theme_designer import ThemeDesignerWidget, _EmbeddedSimulatorPreview, _ScreenPreview


def _temp_names(path):
    return [name for name in os.listdir(path) if name.startswith("tmp")]


def _write_text(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(content)


def _write_bmp(path, width, height, color):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    image = Image.new("RGB", (width, height), color)
    image.save(path, "BMP")


def _write_rbvp(path, width, height, fps, frames):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as handle:
        handle.write(b"RBVP")
        handle.write((1).to_bytes(2, "little"))
        handle.write((32).to_bytes(2, "little"))
        handle.write(width.to_bytes(2, "little"))
        handle.write(height.to_bytes(2, "little"))
        handle.write(fps.to_bytes(2, "little"))
        handle.write(len(frames).to_bytes(2, "little"))
        handle.write((2).to_bytes(2, "little"))
        handle.write(bytes(14))
        for frame in frames:
            handle.write(frame)


def _write_weather_forecast(device_root, condition_code="rain", condition_text="Rain", temp_min="5", temp_max="9"):
    path = os.path.join(device_root, ".rockbox", "rockpod", "weather", "forecast.tsv")
    _write_text(
        path,
        "\n".join(
            [
                "rockpod_weather_v1\tMoncton, NB\t46.0878\t-64.7782\tAmerica/Halifax\t2026-07-01T12:00:00Z\t2026-07-01\tmetric",
                f"2026-07-01\t{condition_code}\t{condition_text}\t{temp_min}\t{temp_max}\t40\t10\t180\t05:45\t21:15\topen-meteo",
                "",
            ]
        ),
    )


def _make_repo(repo_root):
    _write_text(
        os.path.join(repo_root, "themes", "iPone.cfg"),
        "\n".join(
            [
                "wps: /.rockbox/wps/iPone.wps",
                "sbs: /.rockbox/wps/iPone.sbs",
                "fms: /.rockbox/wps/iPone.fms",
                "backdrop: /.rockbox/backdrops/iPone_bd.bmp",
                "background color: 100F16",
                "foreground color: F7F4FA",
                "font: /.rockbox/fonts/24 iLike.fnt",
                "iconset: /.rockbox/icons/iPone.bmp",
                "line selector start color: 2B2234",
                "line selector end color: 9D7AE6",
                "line selector text color: FCF9FF",
                "list separator color: 1A1621",
                "",
            ]
        ),
    )
    _write_text(
        os.path.join(repo_root, "wps", "iPone.wps"),
        "%wd\n%Fl(2,14-Adobe-Helvetica-Bold.fnt)\n%xl(Lockscreen,Wallpaper.bmp)\n",
    )
    _write_text(
        os.path.join(repo_root, "wps", "iPone.sbs"),
        "%wd\n%Fl(3,/.rockbox/fonts/150-Adwaitapod-Icons.fnt,2)\n%xl(Bg,iPone_bd.bmp)\n",
    )
    _write_text(os.path.join(repo_root, "wps", "iPone.fms"), "%wd\n%xl(Lockscreen,Wallpaper.bmp)\n")
    _write_bmp(os.path.join(repo_root, "backdrops", "iPone_bd.bmp"), 320, 240, "#111111")
    _write_bmp(os.path.join(repo_root, "icons", "iPone.bmp"), 16, 16, "#999999")
    _write_text(os.path.join(repo_root, "fonts", "24 iLike.fnt"), "font24\n")
    _write_text(os.path.join(repo_root, "fonts", "18-Cantarell-Regular.fnt"), "font18\n")
    _write_text(os.path.join(repo_root, "fonts", "90-Cantarell-Regular.fnt"), "font90\n")
    _write_text(os.path.join(repo_root, "fonts", "14-Adobe-Helvetica-Bold.fnt"), "font14\n")
    _write_text(os.path.join(repo_root, "fonts", "150-Adwaitapod-Icons.fnt"), "icons\n")
    for name in (
        "Wallpaper.bmp",
        "WallpaperAlt.bmp",
        "WallpaperThird.bmp",
        "WallpaperFourth.bmp",
        "WallpaperFifth.bmp",
        "WallpaperSixth.bmp",
        "iPone_bg.bmp",
        "iPone_bd.bmp",
        "SbsBackdrop.bmp",
        "ChargeWallpaper.bmp",
        "ChargeWallpaperAlt.bmp",
        "ChargeWallpaperThird.bmp",
        "ChargeWallpaperFourth.bmp",
    ):
        _write_bmp(os.path.join(repo_root, "wps", "iPone", name), 320, 240, "#222222")

    _write_text(
        os.path.join(repo_root, "themes", "iPone_nano2g.cfg"),
        "\n".join(
            [
                "wps: /.rockbox/wps/iPone_nano2g.wps",
                "sbs: /.rockbox/wps/iPone_nano2g.sbs",
                "fms: /.rockbox/wps/iPone_nano2g.fms",
                "background color: 0F0F0F",
                "foreground color: F2F2F2",
                "font: /.rockbox/fonts/12-Adobe-Helvetica.fnt",
                "line selector start color: 303030",
                "line selector end color: 909090",
                "line selector text color: FFFFFF",
                "list separator color: 202020",
                "",
            ]
        ),
    )
    _write_text(os.path.join(repo_root, "wps", "iPone_nano2g.wps"), "%wd\n")
    _write_text(os.path.join(repo_root, "wps", "iPone_nano2g.sbs"), "%wd\n")
    _write_text(os.path.join(repo_root, "wps", "iPone_nano2g.fms"), "%wd\n")
    _write_text(os.path.join(repo_root, "fonts", "12-Adobe-Helvetica.fnt"), "font12\n")
    _write_bmp(os.path.join(repo_root, "icons", "tango_icons.12x12.bmp"), 12, 12, "#888888")
    for name in (
        "Wallpaper.bmp",
        "WallpaperAlt.bmp",
        "ChargeWallpaper.bmp",
        "ChargeWallpaperAlt.bmp",
        "ChargeWallpaperThird.bmp",
        "ChargeWallpaperFourth.bmp",
        "wpsbackdrop-176x132x16.bmp",
        "BootLogo.bmp",
    ):
        _write_bmp(os.path.join(repo_root, "wps", "iPone_nano2g", name), 176, 132, "#333333")

    _write_text(
        os.path.join(repo_root, "themes", "Galaxy.cfg"),
        "\n".join(
            [
                "wps: /.rockbox/wps/Galaxy.wps",
                "sbs: /.rockbox/wps/Galaxy.sbs",
                "fms: /.rockbox/wps/Galaxy.fms",
                "background color: FFFFFF",
                "foreground color: 000000",
                "font: /.rockbox/fonts/12-Adobe-Helvetica.fnt",
                "line selector start color: E0E0E0",
                "line selector end color: B0B0B0",
                "line selector text color: 000000",
                "list separator color: 808080",
                "",
            ]
        ),
    )
    _write_text(os.path.join(repo_root, "wps", "Galaxy.wps"), "%wd\n")
    _write_text(os.path.join(repo_root, "wps", "Galaxy.sbs"), "%wd\n")
    _write_text(os.path.join(repo_root, "wps", "Galaxy.fms"), "%wd\n")
    for name in (
        "Wallpaper.bmp",
        "WallpaperAlt.bmp",
        "ChargeWallpaper.bmp",
        "ChargeWallpaperAlt.bmp",
        "ChargeWallpaperThird.bmp",
        "ChargeWallpaperFourth.bmp",
        "wpsbackdrop-160x128x2.bmp",
    ):
        _write_bmp(os.path.join(repo_root, "wps", "Galaxy", name), 160, 128, "#BBBBBB")

    _write_text(
        os.path.join(repo_root, "themes", "CoverPod_3g.cfg"),
        "\n".join(
            [
                "wps: /.rockbox/wps/CoverPod_3g.wps",
                "sbs: /.rockbox/wps/CoverPod_3g.sbs",
                "fms: /.rockbox/wps/CoverPod_3g.fms",
                "background color: FFFFFF",
                "foreground color: 000000",
                "font: /.rockbox/fonts/12-Adobe-Helvetica.fnt",
                "line selector start color: E0E0E0",
                "line selector end color: B0B0B0",
                "line selector text color: 000000",
                "list separator color: 808080",
                "",
            ]
        ),
    )
    _write_text(os.path.join(repo_root, "wps", "CoverPod_3g.wps"), "%wd\n")
    _write_text(os.path.join(repo_root, "wps", "CoverPod_3g.sbs"), "%wd\n")
    _write_text(os.path.join(repo_root, "wps", "CoverPod_3g.fms"), "%wd\n")
    for name in (
        "Wallpaper.bmp",
        "WallpaperAlt.bmp",
        "ChargeWallpaper.bmp",
        "ChargeWallpaperAlt.bmp",
        "ChargeWallpaperThird.bmp",
        "ChargeWallpaperFourth.bmp",
        "wpsbackdrop-160x128x2.bmp",
    ):
        _write_bmp(os.path.join(repo_root, "wps", "CoverPod_3g", name), 160, 128, "#BBBBBB")


def _profile(repo_root, resolution="320x240"):
    if resolution == "160x128":
        theme = "CoverPod_3g"
    elif resolution == "320x240":
        theme = "iPone"
    else:
        theme = "iPone_nano2g"
    return {
        "id": f"profile-{resolution}",
        "name": f"Profile {resolution}",
        "screen_resolution": resolution,
        "source_repo_path": repo_root,
        "device_mount_path": "",
        "backup_location": os.path.join(repo_root, ".backups", resolution),
        "selected_theme": theme,
    }


def test_loads_ipone_base_theme(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()

    variant = service.load_base_theme(repo_root, _profile(repo_root, "320x240"))

    assert variant["base_theme_id"] == "iPone"
    assert variant["font_rel"] == "fonts/24 iLike.fnt"
    assert variant["colors"]["foreground"] == "F7F4FA"


def test_ipodjs_stock_font_is_named_and_prioritized_in_designer(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()

    fonts = service.fonts_for_profile(repo_root)

    assert fonts[0]["path_rel"] == IPODJS_STOCK_FONT_REL
    assert fonts[0]["label"] == IPODJS_STOCK_FONT_LABEL


def test_generates_and_persists_custom_variant(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()

    variant = service.new_variant(repo_root, _profile(repo_root), "Midnight Glass")
    variant["font_rel"] = "fonts/18-Cantarell-Regular.fnt"
    variant["colors"]["selector_end"] = "ABCDEF"
    variant["lockscreen_clock"]["opacity"] = 64
    saved = service.save_variant(repo_root, variant)

    loaded = service.load_variant(repo_root, saved["id"])
    assert loaded["name"] == "Midnight Glass"
    assert loaded["font_rel"] == "fonts/18-Cantarell-Regular.fnt"
    assert loaded["colors"]["selector_end"] == "ABCDEF"
    assert loaded["lockscreen_clock"]["opacity"] == 64
    assert loaded["wallpaper_cycle_enabled"] is False
    assert not _temp_names(os.path.join(repo_root, "rockpod", ".theme_designer", "variants"))


def test_wallpaper_cycle_is_optional_and_default_off(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()

    variant = service.new_variant(repo_root, _profile(repo_root), "Static")
    saved = service.save_variant(repo_root, variant)
    bundle = service.build_bundle(repo_root, _profile(repo_root), saved)
    cfg = next(item["source_abs"] for item in bundle["assets"] if item["kind"] == "cfg")
    metadata = next(item["source_abs"] for item in bundle["assets"] if item["kind"] == "metadata")

    with open(cfg, "r", encoding="utf-8") as handle:
        cfg_text = handle.read()
    with open(metadata, "r", encoding="utf-8") as handle:
        metadata_data = json.load(handle)

    assert "ipone lock wallpaper:" not in cfg_text
    assert "ipone charge wallpaper:" not in cfg_text
    assert "ui engine: ipodjs\n" in cfg_text
    assert metadata_data["wallpaper_cycle_enabled"] is False


def test_ipod5g_designer_theme_keeps_rockbox_engine(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()
    profile = _profile(repo_root)
    profile["target_device_model"] = "iPod Video 5G"

    variant = service.new_variant(repo_root, profile, "5G Rockbox Theme")
    saved = service.save_variant(repo_root, variant)
    bundle = service.build_bundle(repo_root, profile, saved)
    cfg = next(
        item["source_abs"] for item in bundle["assets"]
        if item["kind"] == "cfg"
    )

    with open(cfg, "r", encoding="utf-8") as handle:
        cfg_text = handle.read()

    assert "ui engine: rockbox\n" in cfg_text
    assert "ui engine: ipodjs\n" not in cfg_text


def test_wallpaper_cycle_opt_in_exports_existing_ipone_settings(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()

    variant = service.new_variant(repo_root, _profile(repo_root), "Cycling")
    variant["wallpaper_cycle_enabled"] = True
    saved = service.save_variant(repo_root, variant)
    bundle = service.build_bundle(repo_root, _profile(repo_root), saved)
    cfg = next(item["source_abs"] for item in bundle["assets"] if item["kind"] == "cfg")
    metadata = next(item["source_abs"] for item in bundle["assets"] if item["kind"] == "metadata")

    with open(cfg, "r", encoding="utf-8") as handle:
        cfg_text = handle.read()
    with open(metadata, "r", encoding="utf-8") as handle:
        metadata_data = json.load(handle)

    assert "ipone lock wallpaper: shuffle\n" in cfg_text
    assert "ipone charge wallpaper: rotate\n" in cfg_text
    assert metadata_data["wallpaper_cycle_enabled"] is True


def test_lockscreen_clock_opacity_defaults_and_clamps(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()

    variant = service.new_variant(repo_root, _profile(repo_root), "Clock Alpha")

    assert variant["lockscreen_clock"]["opacity"] == 82
    assert variant["lockscreen_clock"]["style"] == "glass"
    assert variant["lockscreen_clock"]["glass_strength"] == "high"
    assert service._normalize_lockscreen_clock({"opacity": 5})["opacity"] == 20
    assert service._normalize_lockscreen_clock({"opacity": 200})["opacity"] == 100
    assert service._normalize_lockscreen_clock({"opacity": "bad"})["opacity"] == 82


def test_lockscreen_clock_rejects_icon_font_for_readable_sbs_and_wps_clock(tmp_dir):
    service = ThemeDesignerService()

    clock = service._normalize_lockscreen_clock({"font_rel": "fonts/150-Adwaitapod-Icons.fnt"})

    assert clock["font_rel"] == "fonts/90-Cantarell-Regular.fnt"


def test_lockscreen_clock_opacity_exports_alpha_skin_colors(tmp_dir):
    wallpaper = os.path.join(tmp_dir, "black.bmp")
    Image.new("RGB", (320, 240), "#000000").save(wallpaper, "BMP")
    service = ThemeDesignerService()
    content = (
        "%Vl(Clock,0,0,40,10,1)%Vf(FFFFFF)%acmenu\n"
        "%Vl(iPoneLockscreen,0,32,-,55,10)%Vf(FFFFFF)%ac%cl:%cM %cP\n"
        "%Vl(iPoneLockscreen,0,91,-,18,6)%Vf(FFFFFF)%ac"
        "%?if(%ss(0,7,%St(lang)), =, english)<%?cu<Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday>|%ca> "
        "%?or(%if(%ss(0,7,%St(lang)), =, chinese),%if(%St(lang), =, magyar),%if(%St(lang), =, lietuviu),"
        "%if(%St(lang), =, japanese),%if(%St(lang), =, korean))<%cb %cd|%?if(%St(lang), =, english-us)<%cb %cd|%cd %cb>>\n"
    )
    clock = service._normalize_lockscreen_clock(
        {
            "color": "FFFFFF",
            "opacity": 50,
            "x": 0,
            "y": 32,
            "width": 320,
            "height": 55,
        }
    )
    variant = {
        "screen_resolution": "320x240",
        "fit_mode": "fill",
        "wallpaper_source": wallpaper,
        "appearance_mode": "dark",
        "colors": {"background": "000000"},
    }

    updated = service._replace_lockscreen_clock_block(
        content,
        "iPoneLockscreen",
        clock,
        time_font=10,
        date_font=6,
        variant=variant,
    )

    assert "%Vl(Clock,0,0,40,10,1)%Vf(FFFFFF)%acmenu" in updated
    assert "%Vf(FFFFFF80)%ac%cl:%cM %cP" in updated
    assert "%Vl(iPoneLockscreen,0,100,-,18,6)%Vf(FFFFFF80)%ac" in updated


def test_lockscreen_clock_opacity_at_100_exports_raw_color_for_solid_clock(tmp_dir):
    wallpaper = os.path.join(tmp_dir, "black.bmp")
    Image.new("RGB", (320, 240), "#000000").save(wallpaper, "BMP")
    service = ThemeDesignerService()
    content = "%Vl(iPoneLockscreen,0,32,-,55,10)%Vf(123456)%ac%cl:%cM %cP\n"
    clock = service._normalize_lockscreen_clock({"color": "ABCDEF", "opacity": 100, "style": "solid"})
    variant = {
        "screen_resolution": "320x240",
        "fit_mode": "fill",
        "wallpaper_source": wallpaper,
        "appearance_mode": "dark",
        "colors": {"background": "000000"},
    }

    updated = service._replace_lockscreen_clock_block(
        content,
        "iPoneLockscreen",
        clock,
        time_font=10,
        date_font=6,
        variant=variant,
    )

    assert "%Vf(ABCDEF)%ac%cl:%cM %cP" in updated


def test_lockscreen_clock_glass_high_caps_render_opacity():
    service = ThemeDesignerService()
    clock = service._normalize_lockscreen_clock(
        {"color": "ABCDEF", "opacity": 100, "style": "glass", "glass_strength": "high"}
    )

    assert service._lockscreen_clock_render_opacity(clock) == 52
    assert service._lockscreen_clock_skin_color(clock) == "ABCDEF85"


def test_lockscreen_clock_skin_color_adds_alpha_when_translucent(tmp_dir):
    service = ThemeDesignerService()
    clock = service._normalize_lockscreen_clock({"color": "123ABC", "opacity": 25})

    assert service._lockscreen_clock_skin_color(clock) == "123ABC40"


def test_lockscreen_clock_glass_styles_live_text_without_panel(tmp_dir):
    wallpaper = os.path.join(tmp_dir, "wallpaper.bmp")
    Image.new("RGB", (320, 240), "#225577").save(wallpaper, "BMP")
    skin_dir = os.path.join(tmp_dir, "wps")
    os.makedirs(skin_dir, exist_ok=True)
    sbs = os.path.join(skin_dir, "iPoneD-glass-test.sbs")
    _write_text(
        sbs,
        "%xl(LsStyle,LockscreenStyle.bmp)\n"
        "%Vd(iPoneLockscreen)%?mp<\n"
        "%Vl(iPoneLockscreen,0,32,-,55,10)%Vf(FFFFFF)%ac%cl:%cM %cP\n"
        "%Vl(iPoneLockscreen,0,91,-,18,6)%Vf(FFFFFF)%ac"
        "%?if(%ss(0,7,%St(lang)), =, english)<%?cu<Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday>|%ca> "
        "%?or(%if(%ss(0,7,%St(lang)), =, chinese),%if(%St(lang), =, magyar),%if(%St(lang), =, lietuviu),"
        "%if(%St(lang), =, japanese),%if(%St(lang), =, korean))<%cb %cd|%?if(%St(lang), =, english-us)<%cb %cd|%cd %cb>>\n",
    )
    service = ThemeDesignerService()
    variant = {
        "base_theme_id": "iPone",
        "screen_resolution": "320x240",
        "fit_mode": "fill",
        "wallpaper_source": wallpaper,
        "lockscreen_clock": {
            "color": "FFFFFF",
            "opacity": 50,
            "style": "glass",
            "glass_strength": "high",
        },
    }

    service._apply_lockscreen_clock_overrides(sbs, variant)

    glass = os.path.join(skin_dir, "iPoneD-glass-test", "LockClockGlassGenerated.bmp")
    assert not os.path.exists(glass)
    with open(sbs, "r", encoding="utf-8") as handle:
        content = handle.read()
    assert "LockClockGlassGenerated" not in content
    assert "%Vd(LockClockGlass)%Vd(iPoneLockscreen)" not in content
    assert "%Vd(iPoneClock" not in content
    assert content.count("%Vl(iPoneLockscreen") >= 8
    assert "%Vf(FFFFFF80)%ac%cl:%cM %cP" in content


def test_lockscreen_clock_glass_uses_compact_layer_stack(tmp_dir):
    service = ThemeDesignerService()
    content = (
        "%Vd(iPoneLockscreen)%?mp<\n"
        "%Vl(iPoneLockscreen,0,32,-,55,10)%Vf(FFFFFF)%ac%cl:%cM %cP\n"
        "%Vl(iPoneLockscreen,0,91,-,18,6)%Vf(FFFFFF)%ac"
        "%?if(%ss(0,7,%St(lang)), =, english)<%?cu<Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday>|%ca> "
        "%?or(%if(%ss(0,7,%St(lang)), =, chinese),%if(%St(lang), =, magyar),%if(%St(lang), =, lietuviu),"
        "%if(%St(lang), =, japanese),%if(%St(lang), =, korean))<%cb %cd|%?if(%St(lang), =, english-us)<%cb %cd|%cd %cb>>\n"
    )
    clock = service._normalize_lockscreen_clock(
        {"color": "66E0FF", "opacity": 82, "style": "glass", "glass_strength": "high"}
    )

    updated = service._replace_lockscreen_clock_block(
        content,
        "iPoneLockscreen",
        clock,
        time_font=10,
        date_font=6,
    )

    assert updated.count("%Vl(iPoneLockscreen") == 8
    assert "%Vd(iPoneClock" not in updated
    assert "iPoneClockBase" not in updated
    assert "iPoneClockGlass" not in updated
    assert "iPoneClockGlassLift" not in updated
    assert "iPoneClockGlassBevelLight" not in updated
    assert "iPoneClockGlassRimLeft" not in updated
    assert "iPoneClockGlassGleam" not in updated


def test_lockscreen_clock_stretch_reserves_status_row_and_replaces_stale_scale(tmp_dir):
    service = ThemeDesignerService()
    content = (
        "%Vd(iPoneLockscreen)%Vd(iPoneClockBaseScale140)%Vd(iPoneClockGlassShineScale140)%Vd(iPoneClockStretch1)%?mp<\n"
        "%Vl(iPoneClockBaseScale140,0,24,-,92,10)%Vf(FFFFFF)%ac%cl:%cM %cP\n"
        "%Vl(iPoneClockGlassShineScale140,1,24,-,92,10)%Vf(FFFFFF40)%ac%cl:%cM %cP\n"
        "%Vl(iPoneClockStretch1,0,35,-,92,10)%Vf(FFFFFF20)%ac%cl:%cM %cP\n"
        "%Vl(iPoneClockShadow,1,25,-,92,10)%Vf(00000040)%ac%cl:%cM %cP\n"
        "%Vl(iPoneLockscreen,0,94,-,18,6)%Vf(FFFFFF)%ac"
        "%?if(%ss(0,7,%St(lang)), =, english)<%?cu<Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday>|%ca> "
        "%?or(%if(%ss(0,7,%St(lang)), =, chinese),%if(%St(lang), =, magyar),%if(%St(lang), =, lietuviu),"
        "%if(%St(lang), =, japanese),%if(%St(lang), =, korean))<%cb %cd|%?if(%St(lang), =, english-us)<%cb %cd|%cd %cb>>\n"
    )
    clock = service._normalize_lockscreen_clock(
        {
            "font_rel": "fonts/66-Cantarell-Light.fnt",
            "x": 0,
            "y": 0,
            "height": 55,
            "stretch": 160,
            "style": "glass",
            "glass_strength": "high",
        }
    )

    updated = service._replace_lockscreen_clock_block(
        content,
        "iPoneLockscreen",
        clock,
        time_font=10,
        date_font=6,
    )

    assert "iPoneClockBaseScale140" not in updated
    assert "iPoneClockGlassShineScale140" not in updated
    assert "iPoneClockStretch1" not in updated
    assert "iPoneClockShadow" not in updated
    assert "%Vd(iPoneClock" not in updated
    assert "%Vl(iPoneLockscreen,0,24,-,106,10)" in updated
    assert "%Vl(iPoneLockscreen,0,109,-,18,6)" in updated


def test_lockscreen_clock_wps_uses_designer_clock_position_and_spacing(tmp_dir):
    service = ThemeDesignerService()
    content = (
        "%Vd(Lockscreen)%Vd(iPoneClockBaseScale160)%Vd(iPoneClockStretch1)%?mp<\n"
        "%Vl(iPoneClockBaseScale160,0,32,-,106,10)%Vf(FFFFFF)%ac%cl:%cM %cP\n"
        "%Vl(iPoneClockStretch1,0,43,-,106,10)%Vf(FFFFFF20)%ac%cl:%cM %cP\n"
        "%Vl(iPoneClockShadow,1,33,-,106,10)%Vf(00000040)%ac%cl:%cM %cP\n"
        "%Vl(Lockscreen,0,101,-,20,4)%ac"
        "%?if(%ss(0,7,%St(lang)), =, english)<%?cu<Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday>|%ca> "
        "%?or(%if(%ss(0,7,%St(lang)), =, chinese),%if(%St(lang), =, magyar),%if(%St(lang), =, lietuviu),"
        "%if(%St(lang), =, japanese),%if(%St(lang), =, korean))<%cb %cd|%?if(%St(lang), =, english-us)<%cb %cd|%cd %cb>>\n"
    )
    clock = service._normalize_lockscreen_clock(
        {
            "font_rel": "fonts/66-Cantarell-Light.fnt",
            "x": 0,
            "y": 0,
            "height": 55,
            "stretch": 160,
            "style": "glass",
            "glass_strength": "high",
            "opacity": 82,
        }
    )

    updated = service._replace_lockscreen_clock_block(
        content,
        "Lockscreen",
        clock,
        time_font=10,
        date_font=4,
    )

    assert "iPoneClock" not in updated
    assert "%Vl(Lockscreen,0,24,-,106,10)" in updated
    assert "%Vl(Lockscreen,0,109,-,18,4)" in updated
    assert "%Vf(FFFFFF85)%ac%cl:%cM %cP" in updated


def test_designer_sbs_layout_removes_full_art_redraw_pulse(tmp_dir):
    sbs = os.path.join(tmp_dir, "wps", "iPoneD-pulse-test.sbs")
    _write_text(
        sbs,
        "%?if(%St(ipone right pane), =, full art)<%Vd(SbsAnimPulse)%?mp<%Vd(normal)|%Vd(normal)>|%Vd(normal)>\n"
        "%Vl(SbsAnimPulse,319,239,1,1,-)%t(0.1)%dr(0,0,1,1,000000);%t(0.1)%dr(0,0,1,1,010101)\n"
        "%Vl(iPoneLockscreen,32,8,60,16,2)%Vf(FFFFFF)%alHOLD\n"
        "%Vl(iPoneLockscreen,242,8,44,16,2)%Vf(FFFFFF)%ar%bl%%\n",
    )
    service = ThemeDesignerService()

    service._apply_designer_sbs_layout_overrides(
        sbs,
        {
            "base_theme_id": "iPone",
            "lockscreen_clock": {"color": "FFFFFF"},
        },
    )

    with open(sbs, "r", encoding="utf-8") as handle:
        content = handle.read()
    assert "SbsAnimPulse" not in content
    assert "%?if(%St(ipone right pane), =, full art)<%?mp<%Vd(normal)|%Vd(normal)>|%Vd(normal)>" in content


def test_lockscreen_clock_opacity_samples_staged_wallpaper_without_source(tmp_dir):
    wallpaper = os.path.join(tmp_dir, "staged.bmp")
    Image.new("RGB", (320, 240), "#000000").save(wallpaper, "BMP")
    service = ThemeDesignerService()
    clock = service._normalize_lockscreen_clock({"color": "FFFFFF", "opacity": 25})
    variant = {
        "screen_resolution": "320x240",
        "fit_mode": "fill",
        "_staged_lockscreen_wallpaper_source": wallpaper,
        "appearance_mode": "dark",
        "colors": {"background": "000000"},
    }

    color = service._effective_lockscreen_clock_color(variant, clock, 0, 32, 320, 55)

    assert color == "404040"


def test_lockscreen_weather_art_is_generated_between_time_and_date_when_available(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    device_root = os.path.join(tmp_dir, "device")
    _make_repo(repo_root)
    _write_text(
        os.path.join(repo_root, "wps", "iPone.sbs"),
        "\n".join(
            [
                "%wd",
                "%Fl(3,/.rockbox/fonts/150-Adwaitapod-Icons.fnt,2)",
                "%xl(Bg,iPone_bd.bmp)",
                "%Vd(iPoneLockscreen)%?mp<",
                "%Vl(iPoneLockscreen,0,32,-,55,10)%Vf(FFFFFF)%ac%cl:%cM %cP",
                "%Vl(iPoneLockscreen,0,101,-,20,6)%Vf(FFFFFF)%ac"
                "%?if(%ss(0,7,%St(lang)), =, english)<%?cu<Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday>|%ca> "
                "%?or(%if(%ss(0,7,%St(lang)), =, chinese),%if(%St(lang), =, magyar),%if(%St(lang), =, lietuviu),"
                "%if(%St(lang), =, japanese),%if(%St(lang), =, korean))<%cb %cd|%?if(%St(lang), =, english-us)<%cb %cd|%cd %cb>>",
                "",
            ]
        ),
    )
    os.makedirs(device_root, exist_ok=True)
    _write_weather_forecast(device_root, condition_code="rain", condition_text="Rain", temp_min="5", temp_max="9")
    service = ThemeDesignerService()
    profile = _profile(repo_root)
    profile["device_mount_path"] = device_root

    variant = service.new_variant(repo_root, profile, "Weather Lock")
    saved = service.save_variant(repo_root, variant)
    bundle = service.build_bundle(repo_root, profile, saved)
    sbs = next(item["source_abs"] for item in bundle["assets"] if item["kind"] == "sbs")
    weather_assets = [
        item for item in bundle["assets"]
        if item["kind"] == "wps_assets" and item["source_rel"].endswith("WeatherLockArt.bmp")
    ]

    assert weather_assets
    with Image.open(weather_assets[0]["source_abs"]) as weather_art:
        assert weather_art.size == (320, WEATHER_LOCKSCREEN_ART_HEIGHT)
        assert len(weather_art.getcolors(maxcolors=256)) > 2

    with open(sbs, "r", encoding="utf-8") as handle:
        content = handle.read()

    assert "%xl(WeatherLockArt,WeatherLockArt.bmp)" in content
    assert "%xd(WeatherLockArt)" in content
    lines = content.splitlines()
    time_line = next(line for line in lines if "%cl:%cM %cP" in line and "WeatherLock" not in line)
    weather_line = next(line for line in lines if "%Vl(WeatherLock," in line)
    date_line = next(line for line in lines if "%cb>>" in line and "%cd" in line)
    assert lines.index(time_line) < lines.index(weather_line) < lines.index(date_line)


def test_lockscreen_weather_art_is_generated_for_generic_lockscreen_viewport(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()
    profile = _profile(repo_root)
    skin_path = os.path.join(tmp_dir, "generic.wps")
    _write_text(
        skin_path,
        "%wd\n"
        "%xl(Lockscreen,Wallpaper.bmp)\n"
        "%Vl(Lockscreen,0,32,-,55,10)%Vf(FFFFFF)%ac%cl:%cM %cP\n"
        "%Vl(Lockscreen,0,101,-,20,6)%Vf(FFFFFF)%ac"
        "%?if(%ss(0,7,%St(lang)), =, english)<%?cu<Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday>|%ca> "
        "%?or(%if(%ss(0,7,%St(lang)), =, chinese),%if(%St(lang), =, magyar),%if(%St(lang), =, lietuviu),"
        "%if(%St(lang), =, japanese),%if(%St(lang), =, korean))<%cb %cd|%?if(%St(lang), =, english-us)<%cb %cd|%cd %cb>>\n",
    )

    weather_summary = {
        "code": "rain",
        "condition": "Rain",
        "temp": "5-9 C",
        "summary": "Rain 5-9 C",
        "variant_color": "F7F4FA",
        "background_color": "100F16",
        "accent_color": "9D7AE6",
        "appearance_mode": "dark",
    }

    service._apply_lockscreen_clock_overrides(
        skin_path,
        {"lockscreen_clock": DEFAULT_LOCKSCREEN_CLOCK},
        weather_summary=weather_summary,
    )

    with open(skin_path, "r", encoding="utf-8") as handle:
        content = handle.read()

    assert "%xl(WeatherLockArt,WeatherLockArt.bmp)" in content
    assert "%Vl(WeatherLock," in content
    assert "%Vl(Lockscreen," in content
    assert "%Vd(iPoneLockscreen)" not in content
    lines = content.splitlines()
    time_line = next(line for line in lines if "%cl:%cM %cP" in line)
    weather_line = next(line for line in lines if "%Vl(WeatherLock," in line)
    date_line = next(line for line in lines if "%cb>>" in line and "%cd" in line)
    assert lines.index(time_line) < lines.index(weather_line) < lines.index(date_line)


def test_lockscreen_weather_art_is_omitted_without_weather_bundle(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()
    profile = _profile(repo_root)

    variant = service.new_variant(repo_root, profile, "No Weather Lock")
    saved = service.save_variant(repo_root, variant)
    bundle = service.build_bundle(repo_root, profile, saved)
    sbs = next(item["source_abs"] for item in bundle["assets"] if item["kind"] == "sbs")

    with open(sbs, "r", encoding="utf-8") as handle:
        content = handle.read()

    assert "WeatherLockArt" not in content
    assert not [
        item for item in bundle["assets"]
        if item["kind"] == "wps_assets" and item["source_rel"].endswith("WeatherLockArt.bmp")
    ]


def test_wallpaper_conversion_respects_profile_resolution(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    source = os.path.join(tmp_dir, "wallpaper.png")
    Image.new("RGB", (800, 600), "#ff6600").save(source, "PNG")
    service = ThemeDesignerService()

    variant = service.new_variant(repo_root, _profile(repo_root), "Orange")
    variant["wallpaper_source"] = source
    saved = service.save_variant(repo_root, variant)
    bundle = service.build_bundle(repo_root, _profile(repo_root), saved)
    wallpaper = next(
        item["source_abs"] for item in bundle["assets"]
        if item["destination_rel"].endswith(f"/wps/{saved['id']}/Wallpaper.bmp")
    )

    with Image.open(wallpaper) as rendered:
        assert rendered.size == (320, 240)


def test_charging_wallpaper_generation_uses_separate_source(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    main = os.path.join(tmp_dir, "main.png")
    charging = os.path.join(tmp_dir, "charging.png")
    Image.new("RGB", (640, 640), "#00aa66").save(main, "PNG")
    Image.new("RGB", (640, 640), "#0033cc").save(charging, "PNG")
    service = ThemeDesignerService()

    variant = service.new_variant(repo_root, _profile(repo_root), "Dual")
    variant["wallpaper_source"] = main
    variant["charging_wallpaper_source"] = charging
    saved = service.save_variant(repo_root, variant)
    bundle = service.build_bundle(repo_root, _profile(repo_root), saved)
    charge = next(
        item["source_abs"] for item in bundle["assets"]
        if item["destination_rel"].endswith(f"/wps/{saved['id']}/ChargeWallpaper.bmp")
    )

    with Image.open(charge) as rendered:
        assert rendered.size == (320, 240)
        assert rendered.getpixel((10, 10))[2] > rendered.getpixel((10, 10))[1]


def test_preview_state_generation_is_profile_aware(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()

    variant = service.new_variant(repo_root, _profile(repo_root, "176x132"), "Nano")
    preview = service.build_preview_state(repo_root, _profile(repo_root, "176x132"), variant)

    assert preview["base_theme_id"] == "iPone_nano2g"
    assert preview["width"] == 176
    assert preview["height"] == 132
    assert preview["menu_backdrop_path"].endswith("wps/iPone_nano2g/wpsbackdrop-176x132x16.bmp")
    assert preview["preview_modes"] == ["simulator"]


def test_preview_state_generation_supports_coverpod_3g(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()

    variant = service.new_variant(repo_root, _profile(repo_root, "160x128"), "Classic")
    preview = service.build_preview_state(repo_root, _profile(repo_root, "160x128"), variant)

    assert preview["base_theme_id"] == "CoverPod_3g"
    assert preview["width"] == 160
    assert preview["height"] == 128
    assert preview["menu_backdrop_path"].endswith("wps/CoverPod_3g/wpsbackdrop-160x128x2.bmp")


def test_generated_bundle_deploys_with_existing_deploy_flow(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    device_root = os.path.join(tmp_dir, "device")
    os.makedirs(device_root, exist_ok=True)
    service = ThemeDesignerService()
    deploy = RockboxDeployService()
    profile = _profile(repo_root)
    profile["device_mount_path"] = device_root

    variant = service.new_variant(repo_root, profile, "Deployable")
    saved = service.save_variant(repo_root, variant)
    bundle = service.build_bundle(repo_root, profile, saved)
    diff = deploy.build_diff(profile, bundle)
    result = deploy.apply_diff(profile, diff)

    assert result["success"] is True
    assert os.path.isfile(os.path.join(device_root, ".rockbox", "themes", f"{saved['id']}.cfg"))
    assert os.path.isfile(os.path.join(device_root, ".rockbox", "wps", f"{saved['id']}.wps"))
    assert os.path.isfile(os.path.join(device_root, ".rockbox", "rockpod", "theme_designer", f"{saved['id']}.json"))
    assert not _temp_names(os.path.join(repo_root, "rockpod", ".theme_designer", "generated", saved["id"], "themes"))
    assert not _temp_names(os.path.join(repo_root, "rockpod", ".theme_designer", "generated", saved["id"]))


def test_generated_bundle_includes_fonts_referenced_by_skin_files(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()
    profile = _profile(repo_root)

    variant = service.new_variant(repo_root, profile, "Skin Fonts")
    saved = service.save_variant(repo_root, variant)
    bundle = service.build_bundle(repo_root, profile, saved)
    font_dests = {
        item["destination_rel"]
        for item in bundle["assets"]
        if item["kind"] == "font"
    }

    assert ".rockbox/fonts/24 iLike.fnt" in font_dests
    assert ".rockbox/fonts/14-Adobe-Helvetica-Bold.fnt" in font_dests
    assert ".rockbox/fonts/150-Adwaitapod-Icons.fnt" in font_dests


def test_render_budget_flags_stale_generated_clock_layers(tmp_dir):
    service = ThemeDesignerService()
    skin_path = os.path.join(tmp_dir, "stale.wps")
    _write_text(
        skin_path,
        "%Vd(Lockscreen)%Vd(iPoneClockBaseScale160)%Vd(iPoneClockStretch1)%?mp<\n"
        "%Vl(iPoneClockBaseScale160,0,32,-,144,10)%Vf(FFFFFF85)%ac%cl:%cM %cP\n"
        "%Vl(iPoneClockStretch1,0,43,-,144,10)%Vf(FFFFFF20)%ac%cl:%cM %cP\n"
        "%Vl(Lockscreen,0,153,-,18,4)%Vf(FFFFFF85)%acdate\n",
    )

    issues = service._skin_render_budget_issues(
        skin_path,
        "wps",
        {"screen_resolution": "320x240", "lockscreen_clock": DEFAULT_LOCKSCREEN_CLOCK},
    )

    assert any(issue["code"] == "stale_clock_layers" for issue in issues)


def test_render_budget_accepts_compact_lockscreen_clock_layers(tmp_dir):
    service = ThemeDesignerService()
    skin_path = os.path.join(tmp_dir, "compact.wps")
    _write_text(
        skin_path,
        "%Vd(Lockscreen)%?mp<\n"
        "%Vl(Lockscreen,1,25,-,144,10)%Vf(00000017)%ac%cl:%cM %cP\n"
        "%Vl(Lockscreen,0,24,-,144,10)%Vf(3BC384A3)%ac%cl:%cM %cP\n"
        "%Vl(Lockscreen,0,25,-,144,10)%Vf(72D8B86E)%ac%cl:%cM %cP\n"
        "%Vl(Lockscreen,0,23,-,144,10)%Vf(00000045)%ac%cl:%cM %cP\n"
        "%Vl(Lockscreen,0,22,-,144,10)%Vf(D4F2E4FF)%ac%cl:%cM %cP\n"
        "%Vl(Lockscreen,1,24,-,144,10)%Vf(5ECE9A80)%ac%cl:%cM %cP\n"
        "%Vl(Lockscreen,0,139,-,18,4)%Vf(3BC384A3)%acdate\n",
    )

    issues = service._skin_render_budget_issues(
        skin_path,
        "wps",
        {
            "screen_resolution": "320x240",
            "lockscreen_clock": {
                **DEFAULT_LOCKSCREEN_CLOCK,
                "font_rel": "fonts/90-Cantarell-Regular.fnt",
                "stretch": 160,
            },
        },
    )

    assert not [issue for issue in issues if issue["level"] in {"critical", "warn"}]


def test_right_pane_wallpaper_offset_controls_crop_focus(tmp_dir):
    source = os.path.join(tmp_dir, "right-pane.png")
    base = os.path.join(tmp_dir, "base.bmp")
    left_path = os.path.join(tmp_dir, "left-focus.bmp")
    right_path = os.path.join(tmp_dir, "right-focus.bmp")
    image = Image.new("RGB", (320, 240), "#cc0000")
    for x in range(160, 320):
        for y in range(240):
            image.putpixel((x, y), (0, 80, 220))
    image.save(source, "PNG")
    Image.new("RGB", (320, 240), "#ffffff").save(base, "BMP")
    service = ThemeDesignerService()

    service._render_right_pane_image(
        source,
        left_path,
        "320x240",
        "fill",
        "FFFFFF",
        base,
        -100,
        0,
    )
    service._render_right_pane_image(
        source,
        right_path,
        "320x240",
        "fill",
        "FFFFFF",
        base,
        100,
        0,
    )

    with Image.open(left_path) as rendered:
        assert rendered.convert("RGB").getpixel((220, 120))[0] > 160
    with Image.open(right_path) as rendered:
        assert rendered.convert("RGB").getpixel((220, 120))[2] > 160


def test_preserves_base_theme_files_when_variant_is_generated(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()
    source = os.path.join(tmp_dir, "purple.png")
    Image.new("RGB", (900, 500), "#8844ff").save(source, "PNG")
    base_wallpaper = os.path.join(repo_root, "wps", "iPone", "Wallpaper.bmp")
    with open(base_wallpaper, "rb") as handle:
        before = handle.read()

    variant = service.new_variant(repo_root, _profile(repo_root), "NoOverwrite")
    variant["wallpaper_source"] = source
    saved = service.save_variant(repo_root, variant)
    service.build_bundle(repo_root, _profile(repo_root), saved)

    with open(base_wallpaper, "rb") as handle:
        after = handle.read()
    assert before == after


def test_preview_bundle_can_rebuild_without_reusing_the_same_stage_dir(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()
    profile = _profile(repo_root)

    variant = service.new_variant(repo_root, profile, "Preview")
    first = service.build_preview_bundle(repo_root, profile, variant)
    second = service.build_preview_bundle(repo_root, profile, variant)

    first_cfg = next(item for item in first["assets"] if item["kind"] == "cfg")["source_abs"]
    second_cfg = next(item for item in second["assets"] if item["kind"] == "cfg")["source_abs"]

    assert first["id"] == second["id"]
    assert first["id"] == "ipone_preview"
    assert first_cfg != second_cfg
    assert os.path.isfile(first_cfg)
    assert os.path.isfile(second_cfg)


def test_preview_bundle_applies_unsaved_colors_to_generated_assets_and_templates(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()
    profile = _profile(repo_root)

    variant = service.new_variant(repo_root, profile, "Preview Colors")
    variant["colors"]["background"] = "224466"
    variant["colors"]["foreground"] = "EEEEDD"
    variant["colors"]["selector_start"] = "335577"
    variant["colors"]["selector_end"] = "88AA44"
    variant["colors"]["selector_text"] = "FFF199"

    bundle = service.build_preview_bundle(repo_root, profile, variant)
    wallpaper = next(
        item["source_abs"]
        for item in bundle["assets"]
        if item["kind"] == "wps_assets" and item["source_rel"].endswith("/Wallpaper.bmp")
    )
    cfg = next(item["source_abs"] for item in bundle["assets"] if item["kind"] == "cfg")

    with Image.open(wallpaper) as rendered:
        assert rendered.getpixel((10, 10)) == (0x22, 0x44, 0x66)

    with open(cfg, "r", encoding="utf-8") as handle:
        content = handle.read()
    assert "224466" in content
    assert "EEEEDD" in content
    assert not _temp_names(os.path.dirname(cfg))
    assert not _temp_names(os.path.dirname(os.path.dirname(cfg)))
    assert "88AA44" in content
    assert "FFF199" in content


def test_theme_designer_current_variant_data_includes_lockscreen_clock_glass(tmp_dir):
    app = QApplication.instance() or QApplication([])
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()
    profile = _profile(repo_root)
    variant = service.new_variant(repo_root, profile, "Widget Alpha")
    variant["lockscreen_clock"]["opacity"] = 45
    variant["lockscreen_clock"]["style"] = "glass tinted"
    variant["lockscreen_clock"]["glass_strength"] = "medium"
    variant["lockscreen_clock"]["stretch"] = 135
    preview = service.build_preview_state(repo_root, profile, variant)
    widget = ThemeDesignerWidget()

    widget.set_fonts(service.fonts_for_profile(repo_root))
    widget.load_variant(variant, preview)

    clock = widget.current_variant_data()["lockscreen_clock"]
    assert clock["opacity"] == 45
    assert clock["style"] == "glass tinted"
    assert clock["glass_strength"] == "medium"
    assert clock["stretch"] == 135
    widget.deleteLater()


def test_theme_designer_current_variant_data_includes_wallpaper_cycle_choice(tmp_dir):
    app = QApplication.instance() or QApplication([])
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    service = ThemeDesignerService()
    profile = _profile(repo_root)
    variant = service.new_variant(repo_root, profile, "Cycle")
    variant["wallpaper_cycle_enabled"] = True
    preview = service.build_preview_state(repo_root, profile, variant)
    widget = ThemeDesignerWidget()

    widget.set_fonts(service.fonts_for_profile(repo_root))
    widget.load_variant(variant, preview)

    assert widget.current_variant_data()["wallpaper_cycle_enabled"] is True
    widget.deleteLater()


def test_wayland_simulator_mode_uses_full_preview_surface(tmp_dir, monkeypatch):
    app = QApplication.instance() or QApplication([])
    monkeypatch.setenv("XDG_SESSION_TYPE", "wayland")
    monkeypatch.setattr("ui.theme_designer.shutil.which", lambda name: "/usr/bin/xdotool" if name == "xdotool" else None)
    shot = os.path.join(tmp_dir, "preview.bmp")
    _write_bmp(shot, 320, 240, "#6633CC")
    widget = ThemeDesignerWidget()
    widget._resolution_label.setText("320x240")
    widget.set_simulator_preview(shot)
    widget._preview_mode.setCurrentIndex(widget._preview_mode.findData("simulator"))

    assert widget._preview_stack.currentWidget() is widget._preview
    assert widget._base_simulator_preview_path == shot
    assert widget._preview._state["simulator_preview_path"] == shot
    widget.deleteLater()


def test_x11_simulator_mode_uses_baked_in_preview_surface(tmp_dir, monkeypatch):
    app = QApplication.instance() or QApplication([])
    monkeypatch.setenv("XDG_SESSION_TYPE", "x11")
    monkeypatch.setattr("ui.theme_designer.shutil.which", lambda name: "/usr/bin/xdotool" if name == "xdotool" else None)
    widget = ThemeDesignerWidget()
    widget._resolution_label.setText("320x240")
    widget._preview_mode.setCurrentIndex(widget._preview_mode.findData("simulator"))

    assert widget._preview_stack.currentWidget() is widget._preview
    widget.deleteLater()


def test_designer_preview_loops_deployed_right_pane_framepack(tmp_dir):
    app = QApplication.instance() or QApplication([])
    shot = os.path.join(tmp_dir, "preview.bmp")
    framepack = os.path.join(tmp_dir, "RightPaneVideo.rbvp")
    _write_bmp(shot, 320, 240, "#000000")
    red = (0xF800).to_bytes(2, "little")
    green = (0x07E0).to_bytes(2, "little")
    _write_rbvp(framepack, 2, 1, 12, [red * 2, green * 2])

    preview = _ScreenPreview()
    preview.set_preview(
        {
            "simulator_preview_path": shot,
            "simulator_video_framepack_path": framepack,
            "preview_screen": "sbs",
        },
        "simulator",
    )

    assert preview._video_frame_count == 2
    assert preview._video_timer.isActive()
    assert preview._video_frame.toImage().pixelColor(0, 0).red() > 240
    preview._tick_video_frame()
    assert preview._video_frame.toImage().pixelColor(0, 0).green() > 240
    preview._tick_video_frame()
    assert preview._video_frame_index == 0
    preview.deleteLater()


def test_designer_preview_keeps_video_off_non_sbs_screens(tmp_dir):
    app = QApplication.instance() or QApplication([])
    framepack = os.path.join(tmp_dir, "RightPaneVideo.rbvp")
    _write_rbvp(framepack, 1, 1, 12, [(0xF800).to_bytes(2, "little")])

    preview = _ScreenPreview()
    preview.set_preview(
        {
            "simulator_video_framepack_path": framepack,
            "preview_screen": "lockscreen",
        },
        "simulator",
    )

    assert preview._video_frame_count == 0
    assert not preview._video_timer.isActive()
    preview.deleteLater()


def test_video_extraction_keeps_frames_beyond_old_two_second_limit(tmp_dir, monkeypatch):
    commands = []

    def fake_run(command, **_kwargs):
        commands.append(command)
        pattern = command[-1]
        for index in range(1, 31):
            _write_bmp(pattern.replace("%08d", f"{index:08d}"), 4, 4, "#112233")

    monkeypatch.setattr("services.theme_designer.subprocess.run", fake_run)
    service = ThemeDesignerService()
    frames = service._extract_right_pane_video_frames(os.path.join(tmp_dir, "clip.mp4"))
    try:
        assert len(frames) == 30
        assert commands[0][commands[0].index("-frames:v") + 1] == str(RIGHT_PANE_VIDEO_MAX_FRAMES)
    finally:
        for path in frames:
            os.remove(path)
        os.rmdir(os.path.dirname(frames[0]))


def test_framepack_header_preserves_complete_frame_count(tmp_dir):
    service = ThemeDesignerService()
    frame_paths = []
    for index, color in enumerate(("#FF0000", "#00FF00", "#0000FF")):
        path = os.path.join(tmp_dir, f"frame-{index}.bmp")
        _write_bmp(path, 2, 1, color)
        frame_paths.append(path)
    framepack = os.path.join(tmp_dir, "RightPaneVideo.rbvp")

    service._write_right_pane_video_framepack(frame_paths, framepack)

    with open(framepack, "rb") as handle:
        header = handle.read(32)
    assert int.from_bytes(header[14:16], "little") == 3
    assert os.path.getsize(framepack) == 32 + 3 * 2 * 1 * 2


def test_video_fallback_skips_black_lead_in_frame(tmp_dir):
    black = os.path.join(tmp_dir, "black.bmp")
    visible = os.path.join(tmp_dir, "visible.bmp")
    _write_bmp(black, 156, 240, "#000000")
    _write_bmp(visible, 156, 240, "#331122")

    assert ThemeDesignerService._right_pane_video_fallback_index([black, visible]) == 1


def test_video_source_selects_dedicated_right_pane_mode(tmp_dir):
    repo_root = os.path.join(tmp_dir, "repo")
    _make_repo(repo_root)
    clip = os.path.join(tmp_dir, "clip.mp4")
    _write_text(clip, "video")
    service = ThemeDesignerService()
    variant = service.new_variant(repo_root, _profile(repo_root), "Video Mode")
    variant["right_pane_mode"] = "miniplayer"
    variant["right_pane_video_source"] = clip

    normalized = service._normalize_variant(variant, repo_root)

    assert normalized["right_pane_mode"] == "video"


def test_video_skin_keeps_clock_exclusive_to_miniplayer(tmp_dir):
    sbs = os.path.join(tmp_dir, "video.sbs")
    plain_playback = "%?mp<" + "|".join(["%Vd(normal)"] * 9) + ">"
    miniplayer_playback = "%?mp<" + "|".join(
        ["%Vd(SbsAlbumArt)%Vd(normal)"] * 7
        + ["%Vd(radio)"] * 2
    ) + ">"
    _write_text(
        sbs,
        "%?if(%cs, =, 21)<|%?mh<|%?if(%St(ipone right pane), =, full art)<|%Vd(clock)>>>\n"
        f"%?if(%St(ipone right pane), =, full art)<{plain_playback}|{miniplayer_playback}>\n",
    )
    service = ThemeDesignerService()

    service._apply_right_pane_video_skin(sbs, {"right_pane_video_source": "clip.mp4"})

    with open(sbs, "r", encoding="utf-8") as handle:
        content = handle.read()
    assert "%?if(%St(ipone right pane), =, miniplayer)<%Vd(clock)|>" in content
    assert "%?if(%St(ipone right pane), =, full art)<|%Vd(clock)>" not in content
    assert (
        "%?if(%St(ipone right pane), =, miniplayer)<"
        f"{miniplayer_playback}|{plain_playback}>"
    ) in content
    assert (
        "%?if(%St(ipone right pane), =, full art)<"
        f"{plain_playback}|{miniplayer_playback}>"
    ) not in content


def test_video_preserves_explicit_right_picture_for_miniplayer(tmp_dir, monkeypatch):
    service = ThemeDesignerService()
    frame_dir = os.path.join(tmp_dir, "frames")
    os.makedirs(frame_dir)
    source_frame = os.path.join(frame_dir, "source-frame.bmp")
    _write_bmp(source_frame, 320, 240, "#112233")
    fallback_renders = []

    monkeypatch.setattr(service, "_extract_right_pane_video_frames", lambda _source: [source_frame])
    monkeypatch.setattr(
        service,
        "_render_right_pane_video_frame",
        lambda _source, destination, *_args: _write_bmp(destination, 156, 240, "#112233"),
    )
    monkeypatch.setattr(service, "_write_right_pane_video_framepack", lambda *_args: None)
    monkeypatch.setattr(
        service,
        "_render_right_pane_image",
        lambda *args: fallback_renders.append(args),
    )

    service._apply_right_pane_video_overrides(
        tmp_dir,
        {
            "right_pane_video_source": source_frame,
            "right_pane_wallpaper_source": os.path.join(tmp_dir, "wallpaper.bmp"),
            "screen_resolution": "320x240",
            "right_pane_fit_mode": "fill",
            "colors": {"background": "000000"},
        },
        {"right_pane": ["RightPaneWallpaper.bmp"]},
        {"iPone_bd.bmp"},
    )

    assert fallback_renders == []


def test_video_without_right_picture_keeps_visible_frame_fallback(tmp_dir, monkeypatch):
    service = ThemeDesignerService()
    frame_dir = os.path.join(tmp_dir, "frames-fallback")
    os.makedirs(frame_dir)
    source_frame = os.path.join(frame_dir, "source-frame.bmp")
    _write_bmp(source_frame, 320, 240, "#112233")
    fallback_renders = []

    monkeypatch.setattr(service, "_extract_right_pane_video_frames", lambda _source: [source_frame])
    monkeypatch.setattr(
        service,
        "_render_right_pane_video_frame",
        lambda _source, destination, *_args: _write_bmp(destination, 156, 240, "#112233"),
    )
    monkeypatch.setattr(service, "_write_right_pane_video_framepack", lambda *_args: None)
    monkeypatch.setattr(
        service,
        "_render_right_pane_image",
        lambda *args: fallback_renders.append(args),
    )

    service._apply_right_pane_video_overrides(
        tmp_dir,
        {
            "right_pane_video_source": source_frame,
            "right_pane_wallpaper_source": "",
            "screen_resolution": "320x240",
            "right_pane_fit_mode": "fill",
            "colors": {"background": "000000"},
        },
        {"right_pane": ["RightPaneWallpaper.bmp"]},
        {"iPone_bd.bmp"},
    )

    assert len(fallback_renders) == 2
    assert all(args[0] == source_frame for args in fallback_renders)


def test_simulator_mode_ignores_target_build_dir_fallback(tmp_dir, monkeypatch):
    app = QApplication.instance() or QApplication([])
    monkeypatch.setenv("XDG_SESSION_TYPE", "wayland")
    monkeypatch.setattr("ui.theme_designer.shutil.which", lambda name: "/usr/bin/xdotool" if name == "xdotool" else None)
    build_dir = os.path.join(tmp_dir, "build-sim-video-5g")
    simdisk = os.path.join(build_dir, "simdisk")
    os.makedirs(simdisk, exist_ok=True)
    binary = os.path.join(build_dir, "rockboxui")
    _write_text(binary, "#!/bin/sh\n")
    os.chmod(binary, 0o755)
    shot = os.path.join(build_dir, "1UI256.bmp")
    _write_bmp(shot, 320, 240, "#5522AA")

    widget = ThemeDesignerWidget()
    widget._resolution_label.setText("320x240")
    widget.set_simulator_target(binary, simdisk)
    widget._preview_mode.setCurrentIndex(widget._preview_mode.findData("simulator"))

    assert widget._preview._state["simulator_preview_path"] == ""
    widget.deleteLater()


def test_simulator_mode_uses_explicit_snapshot_path_over_live_preview_dump(tmp_dir, monkeypatch):
    app = QApplication.instance() or QApplication([])
    monkeypatch.setenv("XDG_SESSION_TYPE", "wayland")
    monkeypatch.setattr("ui.theme_designer.shutil.which", lambda name: "/usr/bin/xdotool" if name == "xdotool" else None)
    build_dir = os.path.join(tmp_dir, "build-sim-video-5g")
    simdisk = os.path.join(build_dir, "simdisk")
    rockbox_dir = os.path.join(simdisk, ".rockbox")
    os.makedirs(rockbox_dir, exist_ok=True)
    binary = os.path.join(build_dir, "rockboxui")
    _write_text(binary, "#!/bin/sh\n")
    os.chmod(binary, 0o755)
    fallback = os.path.join(build_dir, "1UI256.bmp")
    live = os.path.join(rockbox_dir, "live_preview.bmp")
    _write_bmp(fallback, 320, 240, "#5522AA")
    _write_bmp(live, 320, 240, "#22AA55")

    widget = ThemeDesignerWidget()
    widget._resolution_label.setText("320x240")
    widget.set_simulator_target(binary, simdisk)
    widget.set_simulator_preview(fallback)
    widget._preview_mode.setCurrentIndex(widget._preview_mode.findData("simulator"))

    assert widget._preview._state["simulator_preview_path"] == fallback
    widget.deleteLater()


def test_theme_designer_does_not_launch_live_simulator_on_set_target(tmp_dir, monkeypatch):
    app = QApplication.instance() or QApplication([])
    monkeypatch.setenv("XDG_SESSION_TYPE", "x11")
    monkeypatch.setattr("ui.theme_designer.shutil.which", lambda name: "/usr/bin/xdotool" if name == "xdotool" else None)
    build_dir = os.path.join(tmp_dir, "build-sim-video-5g")
    simdisk = os.path.join(build_dir, "simdisk")
    os.makedirs(simdisk, exist_ok=True)
    binary = os.path.join(build_dir, "rockboxui")
    _write_text(binary, "#!/bin/sh\n")
    os.chmod(binary, 0o755)

    launched = {}

    class _Env:
        def __init__(self):
            self.values = {}

        def insert(self, key, value):
            self.values[key] = value

    class _Proc:
        def __init__(self, *args, **kwargs):
            launched["args"] = args
            launched["kwargs"] = kwargs
            self._state = 1
            self._env = _Env()

        def setWorkingDirectory(self, _value):
            pass

        def processEnvironment(self):
            return self._env

        def setProcessEnvironment(self, _value):
            launched["env"] = dict(self._env.values)

        def setProgram(self, program):
            launched["program"] = program

        def setArguments(self, arguments):
            launched["arguments"] = arguments

        def start(self):
            pass

        def state(self):
            return self._state

        def processId(self):
            return 1234

        errorOccurred = type("Sig", (), {"connect": lambda self, fn: None})()
        finished = type("Sig", (), {"connect": lambda self, fn: None})()

    monkeypatch.setattr("ui.process_helpers.QProcess", _Proc)
    monkeypatch.setattr("ui.theme_designer.QTimer.singleShot", lambda *_args, **_kwargs: None)

    widget = ThemeDesignerWidget()
    widget.set_simulator_target(binary, simdisk)
    widget.restart_simulator_preview()

    assert launched == {}
    assert widget._sim_menu_btn.isEnabled() is False
    widget.deleteLater()


def test_wayland_preview_buttons_are_disabled_in_manual_snapshot_mode(tmp_dir, monkeypatch):
    app = QApplication.instance() or QApplication([])
    monkeypatch.setenv("XDG_SESSION_TYPE", "wayland")
    monkeypatch.setattr("ui.theme_designer.shutil.which", lambda name: "/usr/bin/xdotool" if name == "xdotool" else None)
    build_dir = os.path.join(tmp_dir, "build-sim-video-5g")
    simdisk = os.path.join(build_dir, "simdisk")
    os.makedirs(simdisk, exist_ok=True)
    binary = os.path.join(build_dir, "rockboxui")
    _write_text(binary, "#!/bin/sh\n")
    os.chmod(binary, 0o755)

    launched = {}

    class _Env:
        def __init__(self):
            self.values = {}

        def insert(self, key, value):
            self.values[key] = value

    class _Proc:
        NotRunning = 0

        def __init__(self, *args, **kwargs):
            self._state = 1
            self._env = _Env()

        def setWorkingDirectory(self, _value):
            pass

        def processEnvironment(self):
            return self._env

        def setProcessEnvironment(self, _value):
            launched["env"] = dict(self._env.values)

        def setProgram(self, program):
            launched["program"] = program

        def setArguments(self, arguments):
            launched["arguments"] = arguments

        def start(self):
            launched["started"] = True

        def state(self):
            return self._state

        def processId(self):
            return 4321

        errorOccurred = type("Sig", (), {"connect": lambda self, fn: None})()
        finished = type("Sig", (), {"connect": lambda self, fn: None})()

    monkeypatch.setattr("ui.process_helpers.QProcess", _Proc)

    widget = ThemeDesignerWidget()
    widget.set_simulator_target(binary, simdisk)
    assert launched == {}
    assert widget._sim_play_btn.isEnabled() is False
    assert widget._sim_select_btn.isEnabled() is False
    widget.deleteLater()


def test_x11_simulator_mode_prefers_embedded_surface_when_xdotool_exists(monkeypatch):
    app = QApplication.instance() or QApplication([])
    monkeypatch.setenv("XDG_SESSION_TYPE", "x11")
    monkeypatch.setattr("ui.theme_designer.shutil.which", lambda name: "/usr/bin/xdotool" if name == "xdotool" else None)

    widget = ThemeDesignerWidget()
    widget._resolution_label.setText("320x240")
    widget._preview_mode.setCurrentIndex(widget._preview_mode.findData("simulator"))

    assert widget._preview_stack.currentWidget() is widget._preview
    assert widget.current_variant_data()["preview_screen"] == "sbs"
    widget.deleteLater()


def test_embedded_simulator_preview_delegates_xdotool_helpers(monkeypatch):
    calls = []
    monkeypatch.setattr(
        "ui.theme_designer.RockboxSimulatorService._wait_for_window_id",
        lambda pid, timeout=1.0: calls.append(("search", pid, timeout)) or "456",
    )
    monkeypatch.setattr(
        "ui.theme_designer.RockboxSimulatorService._send_key_to_window_id",
        lambda window_id, key_name: calls.append(("key", window_id, key_name)) or True,
    )
    monkeypatch.setattr("ui.theme_designer.shutil.which", lambda name: "/usr/bin/xdotool" if name == "xdotool" else None)

    preview = _EmbeddedSimulatorPreview()
    preview._active_pid = lambda: 99

    assert preview._window_ids_for_pid(77) == ["456"]
    assert preview.send_key("F5") is True
    assert calls == [
        ("search", 77, 1.0),
        ("search", 99, 1.0),
        ("key", "456", "F5"),
    ]
    preview.deleteLater()


def test_embedded_preview_finds_active_sbs_framepack(tmp_dir):
    simdisk = os.path.join(tmp_dir, "simdisk")
    config = os.path.join(simdisk, ".rockbox", "config.cfg")
    framepack = os.path.join(
        simdisk,
        ".rockbox",
        "wps",
        "iPoneD-ipone_preview",
        "RightPaneVideo.rbvp",
    )
    _write_text(config, "sbs: /.rockbox/wps/iPoneD-ipone_preview.sbs\n")
    _write_rbvp(framepack, 1, 1, 12, [(0xF800).to_bytes(2, "little")])

    preview = _EmbeddedSimulatorPreview()
    preview._simdisk_path = simdisk

    assert preview.video_framepack_path() == framepack
    preview.deleteLater()
