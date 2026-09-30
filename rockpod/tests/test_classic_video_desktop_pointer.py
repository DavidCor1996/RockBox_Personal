"""Contracts for unchanged video overlays, album scrolling and desktop RSC."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text()


def test_playback_screensaver_toggle_is_saved_and_defaults_on():
    settings = read("apps/settings_list.c")
    assert 'OFFON_SETTING(0, ui_engine_playback_screensaver, -1, true,' in settings
    assert '"ui engine playback screensaver", NULL)' in settings
    menu = read("apps/root_menu.c")
    assert 'return "Time/Battery";' in menu
    adjust = menu.split('static bool root_menu_video_qs_adjust', 1)[1]
    toggle = adjust.split('case IPODJS_QS_SCREENSAVER:', 1)[1].split('break;', 1)[0]
    assert '!global_settings.ui_engine_playback_screensaver' in toggle
    assert 'changed = true;' in toggle
    for forbidden in ('audio_', 'playlist_', 'backlight_', 'hold_effect'):
        assert forbidden not in toggle


def test_disabled_screensaver_fails_closed_without_changing_hold():
    ui = read("apps/gui/ipodjs_ui.c")
    idle = ui.split('bool ipodjs_ui_draw_retailos_playback_idle', 1)[1]
    guard = idle.split('now = get_time();', 1)[0]
    assert 'if (!global_settings.ui_engine_playback_screensaver ||' in guard
    assert 'cache->presented = false;' in guard and 'return false;' in guard
    menu = read("apps/root_menu.c")
    hold = menu.split('/* Hold owns its own screen.', 1)[1].split('#endif', 1)[0]
    assert 'ui_engine_playback_screensaver' not in hold
    assert 'return;' in hold


def test_video_overlay_port_is_excluded():
    assert not (ROOT / "apps/gui/ipodjs_video_controls.c").exists()
    for path in ("apps/video_playback.c", "apps/plugins/openh264_player.c",
                 "apps/plugins/mpegplayer/mpegplayer.c"):
        plugin = read(path)
        assert "ipodjs_video_controls" not in plugin
        assert "video_classic_overlay" not in plugin


def test_album_scroll_is_separate_cached_state_and_stops_with_wps():
    text = read("apps/root_menu.c")
    album = text.split("static void root_menu_video_wps_album_offset", 1)[1]
    album = album.split("static void root_menu_video_draw_wps_title_offset", 1)[0]
    for forbidden in ("font_load", "open(", "core_alloc", "audio_stop"):
        assert forbidden not in album
    assert "button_hold()" in album and "button_queue_empty()" in album
    assert "global_settings.scroll_delay" in album
    assert "vp->font = font" in album
    assert "root_menu_video_draw_wps_album(info_x, album_y, info_w" in text
    assert "root_menu_video_wps_album_scroll.active = false;" in text.split(
        "static void root_menu_video_stop_wps_title_scroll(void)\n{", 1)[1].split("}", 1)[0]


def test_steam_adds_real_rsc_and_keeps_nibiru():
    steam = read("apps/plugins/steam_desktop.c")
    assert 'st_add("RuneScape Classic"' in steam
    assert 'st_add("NiBiRu: Age of Secrets"' in steam
    assert 'PLUGIN_GAMES_DIR "/runescape_classic.rock"' in steam
    assert '"-desktop" : game->parameter' in steam
    game = read("apps/plugins/runescape_classic/rockbox-platform.c")
    assert 'rsc_desktop = parameter && !rb->strcmp(parameter, "-desktop")' in game
    assert "mudclient_mouse_pressed" in game and "mudclient_mouse_released" in game
    assert "rsc_poll_wheel_position(mud);" in game  # Normal launch retained.


def test_host_pointer_click_drag_release_and_invalid_records(tmp_path):
    (tmp_path / "plugin.h").write_text(r'''
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#define SIMULATOR
#define LCD_WIDTH 320
#define LCD_HEIGHT 240
#define HZ 100
#define ROCKBOX_DIR "/.rockbox"
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define TIME_BEFORE(a,b) ((long)((a)-(b))<0)
static long tick;
static char input[14];
static int closes;
static int op(const char *p,int f) {(void)p;(void)f;return 1;}
static int rd(int f,void *p,int n) {(void)f;memcpy(p,input,n);return n;}
static int cl(int f) {(void)f;closes++;return 0;}
static const struct {
 long *current_tick;
 int (*open)(const char*,int);
 int (*read)(int,void*,int);
 int (*close)(int);
 int (*atoi)(const char*);
} api={&tick,op,rd,cl,atoi}, *rb=&api;
''')
    (tmp_path / "fixedpoint.h").write_text("")
    code = '#include "' + str(ROOT / "apps/plugins/lib/desktop_game_pointer.h") + '"\n'
    code += r'''
#include <assert.h>
static void sample(struct desktop_game_pointer *p,const char *s) {
 memcpy(input,s,13); tick+=2; desktop_game_pointer_poll(p);
}
int main(void) {
 struct desktop_game_pointer p={.wheel=-1};
 sample(&p,"0100 0100 01\n"); assert(!p.armed);
 sample(&p,"0100 0100 00\n"); assert(p.armed);
 sample(&p,"0120 0130 01\n");
 assert(p.x==120 && p.y==130 && p.buttons==1 && p.previous_buttons==0);
 sample(&p,"0150 0160 01\n");
 assert(p.x==150 && p.y==160 && p.previous_buttons==1);
 sample(&p,"0150 0160 00\n"); assert(p.buttons==0 && p.previous_buttons==1);
 sample(&p,"0150 0160 02\n"); assert(p.buttons==2);
 sample(&p,"xxxx 0160 00\n"); assert(p.x==150 && p.buttons==2);
 sample(&p,"9999 9999 00\n"); assert(p.x==319 && p.y==239);
 assert(closes==8); return 0;
}
'''
    (tmp_path / "test.c").write_text(code)
    subprocess.run(["cc", "-std=c99", "-Wall", "-Werror", "-I", str(tmp_path),
                    str(tmp_path / "test.c"), "-o", str(tmp_path / "test")], check=True)
    subprocess.run([str(tmp_path / "test")], check=True)
