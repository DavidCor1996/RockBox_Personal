"""WPS pages stay in the core lifecycle and reuse the playback artwork."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def test_rectangular_player_art_is_accepted_without_loading(tmp_path):
    root = (ROOT / 'apps/root_menu.c').read_text()
    body = root.split('static struct bitmap *root_menu_video_buffered_art(void)', 1)[1]
    body = body.split('static void root_menu_video_prepare_wps_assets', 1)[0]
    native = body.split('#ifdef IPOD_6G', 1)[1].split('#else', 1)[0]
    for forbidden in ('open(', 'read_bmp', 'core_alloc', 'claim_aa', 'audio_stop'):
        assert forbidden not in native
    code = r'''
#include <assert.h>
#include <stddef.h>
#define IPODJS_STOCK_ART_SIZE 128
#define WPS_MAX_ALBUMART 3
struct bitmap { int width, height; unsigned char *data; };
static unsigned char pixels;
static struct bitmap art[3];
static int playback_current_aa_hid(int i) { return i; }
static int bufgetdata(int h, int n, void *out) {
    (void)n; *(struct bitmap **)out = &art[h]; return 1;
}
static struct bitmap *get(void) {
    int handle; struct bitmap *bm = NULL;
'''+native+r'''
}
int main(void) {
    art[0]=(struct bitmap){64,64,&pixels};
    art[1]=(struct bitmap){128,96,&pixels};
    art[2]=(struct bitmap){0,0,0};
    assert(get()==&art[1]);
    art[1]=(struct bitmap){86,128,&pixels}; assert(get()==&art[1]);
    art[1]=(struct bitmap){128,128,&pixels}; assert(get()==&art[1]);
    art[1]=(struct bitmap){128,129,&pixels}; assert(get()==&art[0]);
    art[0].data=0; assert(get()==NULL);
    return 0;
}
'''
    (tmp_path/'test.c').write_text(code)
    subprocess.run(['cc','-std=c99','-Wall','-Werror',str(tmp_path/'test.c'),
                    '-o',str(tmp_path/'test')], check=True)
    subprocess.run([str(tmp_path/'test')],check=True)


def test_select_and_rating_preserve_wps_and_audio_lifecycle():
    root = (ROOT / 'apps/root_menu.c').read_text()
    select = root.split('void root_menu_ipodjs_wps_select(void)',1)[1]
    select = select.split('static void ipodjs_video_draw_wps_full',1)[0]
    assert '(root_menu_video_wps_page + 1) % IPODJS_WPS_PAGE_COUNT' in select
    assert 'tagcache_search_finish(&tcs)' in select
    assert 'int rating = stars * 2;' in select
    assert 'tagcache_update_numeric' in select
    for forbidden in ('playlist_', 'audio_stop', 'audio_play', 'core_alloc',
                      'adjust_volume', 'runtimedb ='):
        assert forbidden not in select
    wps = (ROOT/'apps/gui/wps.c').read_text()
    assert 'root_menu_ipodjs_wps_select();' in wps
    assert 'root_menu_ipodjs_wps_adjust_rating(' in wps
    assert 'PLUGIN_APPS_DIR "/lrcplayer.rock"' in wps
    assert '"playlist-blocked"' in wps


def test_rating_uses_original_assets_and_source_strip_geometry():
    ui = (ROOT/'apps/gui/ipodjs_ui.c').read_text()
    assert '301, wps->rating_star_data' in ui
    assert '302, wps->rating_dot_data' in ui
    assert '300, wps->star_data' in ui
    editor = ui.split('void ipodjs_ui_draw_retailos_rating_editor',1)[1]
    editor = editor.split('bool ipodjs_ui_retailos_now_playing_animation',1)[0]
    assert '97 + i * 26, 200' in editor
    assert 'i < 5' in editor
    for forbidden in ('open(', 'load_', 'alloc(', 'tagcache'):
        assert forbidden not in editor


def test_actual_rating_handler_clamps_and_clears_without_volume(tmp_path):
    root = (ROOT/'apps/root_menu.c').read_text()
    function = 'bool root_menu_ipodjs_wps_adjust_rating' + root.split(
        'bool root_menu_ipodjs_wps_adjust_rating',1)[1].split(
        'static void ipodjs_video_draw_wps_full',1)[0]
    code = r'''
#include <stdbool.h>
#include <assert.h>
#define HAVE_TAGCACHE
#define HZ 100
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define IPODJS_WPS_RATING 1
#define tag_rating 7
static int root_menu_video_wps_page;
static bool root_menu_video_wps_force_full;
struct mp3entry { int tagcache_idx, rating; };
static struct mp3entry track = {42,0};
static int writes, saved, notices;
static struct mp3entry *audio_current_track(void) { return &track; }
static void tagcache_update_numeric(int idx, int tag, int value) {
    assert(idx==41 && tag==tag_rating); writes++; saved=value;
}
static void splash(int ticks,const char *msg) {(void)ticks;(void)msg;notices++;}
'''+function+r'''
int main(void) {
    assert(!root_menu_ipodjs_wps_adjust_rating(1)); assert(writes==0);
    root_menu_video_wps_page=IPODJS_WPS_RATING;
    for(int i=1;i<=5;i++) {
        assert(root_menu_ipodjs_wps_adjust_rating(1)); assert(saved==i*2);
    }
    root_menu_ipodjs_wps_adjust_rating(1); assert(track.rating==10 && writes==5);
    for(int i=4;i>=0;i--) {
        root_menu_ipodjs_wps_adjust_rating(-1); assert(saved==i*2);
    }
    root_menu_ipodjs_wps_adjust_rating(-1); assert(track.rating==0 && writes==10);
    track.tagcache_idx=0;
    assert(root_menu_ipodjs_wps_adjust_rating(1));
    assert(notices==1 && writes==10); return 0;
}
'''
    (tmp_path/'rating.c').write_text(code)
    subprocess.run(['cc','-std=c99','-Wall','-Werror',str(tmp_path/'rating.c'),
                    '-o',str(tmp_path/'rating')],check=True)
    subprocess.run([str(tmp_path/'rating')],check=True)
