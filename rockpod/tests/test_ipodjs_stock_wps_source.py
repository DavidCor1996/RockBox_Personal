"""Static checks for the stock-engine iPodJS while-playing skin."""

from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def _read(rel_path):
    return (REPO_ROOT / rel_path).read_text(encoding="utf-8", errors="replace")


def test_ipod6g_ipodjs_uses_stock_wps_with_matching_palette():
    engine = _read("apps/gui/skin_engine/skin_engine.c")
    wps = _read("wps/ipodjs-classic.wps")
    wpslist = _read("wps/WPSLIST")

    assert 'setting = "ipodjs-classic";' in engine
    assert "global_settings.ui_engine == UI_ENGINE_IPODJS" in engine
    assert "%St(ui engine dark mode)" in wps
    assert "%St(ui engine surface)" in wps
    assert "%St(ui engine accent)" in wps
    assert "%Cl(15,46,128,128,c,c,1)" in wps
    assert "%pb(0,0,290,8,noborder)" in wps
    assert "Name: ipodjs-classic" in wpslist
    assert "wps.320x240x(16|24|32): ipodjs-classic.wps" in wpslist
    assert "font.320x240x(16|24|32): 14-Adobe-Helvetica-Bold.fnt" in wpslist

    for color in (
        "005CC0", "545A64", "B61823", "008084", "378E40",
        "B88726", "D06820", "7152AA", "C34880",
    ):
        assert f"%Vf({color})" in wps


def test_iponecustom_packages_the_ipodjs_stock_font():
    wpslist = _read("wps/WPSLIST")
    theme_cfg = _read("themes/iPoneCustom.cfg")
    iponecustom = wpslist.split("Name: iPoneCustom", 1)[1].split("</theme>", 1)[0]

    assert "font.320x240x(16|24|32): 14-Adobe-Helvetica-Bold.fnt" in iponecustom
    assert "font: /.rockbox/fonts/14-Adobe-Helvetica-Bold.fnt" in theme_cfg


def test_album_rows_use_representative_albumartist_before_filtered_search():
    tagtree = _read("apps/tagtree.c")
    fast_path = tagtree.index(
        "tagcache_retrieve(album_tcs, idx_id, tag_albumartist, buf, size)"
    )
    exhaustive_path = tagtree.index(
        "tagtree_single_album_tag_value(tag_albumartist, album_seek, level,"
    )
    assert fast_path < exhaustive_path
