"""Static checks for the stock-engine iPodJS while-playing skin."""

from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def _read(rel_path):
    return (REPO_ROOT / rel_path).read_text(encoding="utf-8", errors="replace")


def test_ipod6g_ipodjs_uses_stock_wps_with_verified_apple_chrome():
    engine = _read("apps/gui/skin_engine/skin_engine.c")
    wps = _read("wps/ipodjs-classic.wps")
    wpslist = _read("wps/WPSLIST")

    assert 'setting = "ipodjs-classic";' in engine
    assert "global_settings.ui_engine == UI_ENGINE_IPODJS" in engine
    assert "%Cl(15,34,128,128,c,c,1)" in wps
    assert "status-header.apple.320x24x24.bmp" in wps
    assert "status-battery.apple.26x65x24.bmp" in wps
    assert "status-playback.apple.20x32x24.bmp" in wps
    assert "progress-frame.apple.200x22x32.bmp" in wps
    assert "progress-fill-cap.apple.16x16x24.bmp" in wps
    assert "endcap,K,backdrop,F" in wps
    assert "Name: ipodjs-classic" in wpslist
    assert "wps.320x240x(16|24|32): ipodjs-classic.wps" in wpslist
    assert "font.320x240x(16|24|32): 14-Adobe-Helvetica-Bold.fnt" in wpslist

    # Fixed stock chrome must not be remapped to an album-art palette.
    assert "%St(ui engine accent)" not in wps


def test_iponecustom_packages_the_ipodjs_stock_font():
    wpslist = _read("wps/WPSLIST")
    theme_cfg = _read("themes/iPoneCustom.cfg")
    iponecustom = wpslist.split("Name: iPoneCustom", 1)[1].split("</theme>", 1)[0]

    assert "font.320x240x(16|24|32): 14-Adobe-Helvetica-Bold.fnt" in iponecustom
    assert "font: /.rockbox/fonts/14-Adobe-Helvetica-Bold.fnt" in theme_cfg


def test_album_rows_use_representative_albumartist_before_filtered_search():
    tagtree = _read("apps/tagtree.c")
    fast_path = tagtree.index(
        "tagcache_retrieve(&tcs, tcs.idx_id, tag_albumartist,"
    )
    artist_fallback = tagtree.index(
        "tagcache_retrieve(&tcs, tcs.idx_id, tag_artist,", fast_path
    )
    assert fast_path < artist_fallback
    assert "opening nested filtered searches for every row" in tagtree
