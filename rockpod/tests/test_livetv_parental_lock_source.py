from pathlib import Path


ROOT = Path(__file__).parents[2]
GUIDE = ROOT / "apps" / "plugins" / "mpegplayer" / "livetv_guide.c"
HEADER = ROOT / "apps" / "plugins" / "mpegplayer" / "livetv.h"
HOST = ROOT / "rockpod" / "services" / "livetv.py"
ROOT_MENU = ROOT / "apps" / "root_menu.c"
MAIN_MENU = ROOT / "apps" / "menus" / "main_menu.c"


def test_livetv_parental_lock_is_synced_and_hidden_until_unlocked():
    guide = GUIDE.read_text(encoding="utf-8")
    header = HEADER.read_text(encoding="utf-8")
    host = HOST.read_text(encoding="utf-8")

    assert "bool parental_locked;" in header
    assert "parental_locked: bool = False" in host
    assert "favourite\\tparental_locked" in host
    assert "char *fields[7];" in guide
    assert "chan->parental_locked = rb->atoi(fields[6]) != 0;" in guide
    assert "chan->parental_locked && !livetv_parental_unlocked" in guide
    assert "livetv_channels[probe].parental_locked" in guide


def test_livetv_hold_select_uses_settings_pin_and_menu_cancels():
    guide = GUIDE.read_text(encoding="utf-8")

    assert 'ROCKBOX_DIR "/videolist/locked.pin"' in guide
    assert '"Unlock Settings"' in guide
    assert "#define LIVETV_BTN_UNLOCK   (BUTTON_SELECT | BUTTON_REPEAT)" in guide
    assert "case LIVETV_BTN_UNLOCK:" in guide
    assert "case LIVETV_BTN_EXIT_REL:" in guide
    prompt = guide.split("static bool livetv_parental_prompt_pin", 1)[1]
    prompt = prompt.split("static bool livetv_parental_unlock", 1)[0]
    assert "case LIVETV_BTN_EXIT:" in prompt
    assert "return false;" in prompt


def test_directv_is_a_top_level_main_menu_item_not_a_setting():
    root = ROOT_MENU.read_text(encoding="utf-8")
    stock = MAIN_MENU.read_text(encoding="utf-8")

    assert '"directv", &livetv_item' in root
    assert "if (item == &livetv_item)" in root
    assert 'return "DIRECTV";' in root
    assert 'if (title && !strcmp(title, "DIRECTV"))' in root
    assert "return IPODJS_LIVETV_PREVIEW;" in root
    defaults = root.split("root_menu_ipodjs_default_items[]", 1)[1]
    defaults = defaults.split("};", 1)[0]
    assert "&livetv_item," in defaults
    settings = root.split("root_menu_video_settings_items[]", 1)[1]
    settings = settings.split("};", 1)[0]
    assert '"DIRECTV"' not in settings
    assert "directv_settings" not in stock
