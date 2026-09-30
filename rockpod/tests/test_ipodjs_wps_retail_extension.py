"""Execute the WPS input and bounded text code, plus private asset checks."""
from pathlib import Path
import re
import struct
import subprocess
import pytest

ROOT = Path(__file__).resolve().parents[2]


def compile_c(tmp_path, code, name):
    source = tmp_path / (name + '.c')
    source.write_text(code)
    output = tmp_path / name
    subprocess.run(['cc', '-std=gnu99', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', '-g', str(source), '-o', str(output)],
                   check=True)
    return output


def function(source, signature):
    return signature + source.split(signature, 1)[1].split('\n}', 1)[0] + '\n}'


def test_production_scrubber_clamps_and_preserves_pause(tmp_path):
    source = (ROOT / 'apps/gui/wps.c').read_text()
    body = function(source, 'static void ipodjs_wps_scrub_adjust(int delta)')
    program = r'''
#include <assert.h>
#include <stddef.h>
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define AUDIO_STATUS_PLAY 1
struct mp3entry { unsigned long elapsed, length; };
static struct mp3entry track = {500, 10000};
static struct mp3entry *current = &track;
static struct { int ff_rewind_min_step; } global_settings = {3};
static int status = 1, prepares, seeks;
static unsigned long position;
static struct mp3entry *audio_current_track(void) { return current; }
static int audio_status(void) { return status; }
static void audio_pre_ff_rewind(void) { prepares++; }
static void audio_ff_rewind(unsigned long value) { seeks++; position=value; }
''' + body + r'''
int main(void) {
    ipodjs_wps_scrub_adjust(-1); assert(position==0 && status==1);
    ipodjs_wps_scrub_adjust(1); assert(position==3000);
    track.elapsed=9900; status=3;
    ipodjs_wps_scrub_adjust(1); assert(position==10000 && status==3);
    ipodjs_wps_scrub_adjust(-1); assert(position==7000 && status==3);
    track.elapsed=20000;
    ipodjs_wps_scrub_adjust(-1); assert(position==7000);
    global_settings.ff_rewind_min_step=0;
    ipodjs_wps_scrub_adjust(-1); assert(position==6000);
    int old=seeks; track.length=0; ipodjs_wps_scrub_adjust(1);
    current=NULL; ipodjs_wps_scrub_adjust(1); assert(seeks==old);
    assert(prepares==seeks); return 0;
}
'''
    subprocess.run([str(compile_c(tmp_path, program, 'scrub'))], check=True)


@pytest.fixture
def text_reader(tmp_path):
    source = (ROOT / 'apps/gui/ipodjs_wps_text.c').read_text()
    source = re.sub(r'^#include .*$', '', source, flags=re.M)
    support = r'''
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#define MAX_PATH 1024
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define ARRAYLEN(a) (sizeof(a)/sizeof(a[0]))
#define ISO_8859_1 0
static off_t filesize(int fd) { struct stat st; assert(!fstat(fd,&st)); return st.st_size; }
static void yield(void) {}
static char *strmemccpy(char *to,const char *from,size_t n) {
    snprintf(to,n,"%s",from); return to;
}
static int ipodjs_ui_retailos_font(bool b) { (void)b; return 0; }
static int font_getstringsize(const char *s,void *w,void *h,int f) {
    (void)w;(void)h;(void)f; int width=0;
    for(;*s;s++) if (((unsigned char)*s & 0xc0)!=0x80) width+=8;
    return width;
}
static unsigned char *utf8encode(unsigned long c,unsigned char *out) {
    if(c<128) *out++=c;
    else if(c<2048) { *out++=0xc0|(c>>6); *out++=0x80|(c&63); }
    else if(c<65536) { *out++=0xe0|(c>>12); *out++=0x80|((c>>6)&63); *out++=0x80|(c&63); }
    else { *out++=0xf0|(c>>18); *out++=0x80|((c>>12)&63); *out++=0x80|((c>>6)&63); *out++=0x80|(c&63); }
    return out;
}
static unsigned char *iso_decode_ex(const unsigned char *p,unsigned char *out,
    int cp,int n,int capacity) {
    (void)cp; unsigned char *start=out;
    while(n-- && out-start<capacity-2) out=utf8encode(*p++,out);
    return out;
}
static bool utf16_has_bom(const unsigned char *p,bool *le) {
    *le=p[0]==255 && p[1]==254;
    return *le || (p[0]==254 && p[1]==255);
}
'''
    main = r'''
int main(int argc,char **argv) {
    assert(argc==2);
    bool found=ipodjs_wps_text_load(argv[1]);
    if(!found) { puts("MISSING"); return 0; }
    assert(ipodjs_wps_text_count()<=TEXT_LINES);
    for(int i=0;i<line_count;i++) {
        assert(lines[i]<TEXT_BYTES);
        assert(memchr(text+lines[i],0,TEXT_BYTES-lines[i]));
        puts(text+lines[i]);
    }
    ipodjs_wps_text_scroll(10000);
    assert(ipodjs_wps_text_top()==MAX(0,line_count-11));
    ipodjs_wps_text_scroll(-10000); assert(ipodjs_wps_text_top()==0);
    assert(!strcmp(ipodjs_wps_text_line(10000),"")); return 0;
}
'''
    return compile_c(tmp_path, support + source + main, 'text_reader')


def run_reader(reader, track):
    return subprocess.check_output([str(reader), str(track)]).decode('utf8')


def id3_frame(payload, version=3, claimed=None):
    size = len(payload) if claimed is None else claimed
    frame = b'USLT' + size.to_bytes(4, 'big') + b'\0\0' + payload
    n = len(frame)
    return b'ID3' + bytes([version, 0, 0]) + bytes(
        [(n >> shift) & 127 for shift in (21, 14, 7, 0)]) + frame


def test_native_lyrics_sidecars_and_embedded_unicode(text_reader, tmp_path):
    track = tmp_path / 'song.mp3'
    track.write_bytes(id3_frame(b'\x03eng\0Embedded lyric\nSecond line'))
    assert run_reader(text_reader, track) == 'Embedded lyric\nSecond line\n'
    sidecar = track.with_suffix('.lrc')
    sidecar.write_text('[ar:Artist]\n[00:01.00]First\n[00:02.00][Chorus]\n')
    out = run_reader(text_reader, track)
    assert 'First\n[Chorus]\n' in out and '[00:' not in out
    sidecar.unlink()
    payload = b'\x01eng\xff\xfe\0\0' + 'Café\nMusic 🎵'.encode('utf-16le')
    track.write_bytes(id3_frame(payload))
    assert run_reader(text_reader, track) == 'Café\nMusic 🎵\n'
    track.write_bytes(id3_frame(b'\x01eng\xff\xfe\0\0\x00\xd8'))
    assert run_reader(text_reader, track) == '�\n'


def test_native_lyrics_bounds_and_malformed_tags(text_reader, tmp_path):
    track = tmp_path / 'song.mp3'
    track.write_bytes(id3_frame(b'\x03eng\0bad', claimed=100000))
    assert run_reader(text_reader, track) == 'MISSING\n'
    track.write_bytes(b'ID3\3\0\0\xff\xff\xff\xff')
    assert run_reader(text_reader, track) == 'MISSING\n'
    track.with_suffix('.txt').write_text('a'*10000)
    out=run_reader(text_reader, track)
    assert out.replace('\n','')=='a'*4096
    assert all(len(line)<=37 for line in out.splitlines())


def test_real_resources_and_no_implicit_equalizer():
    ui=(ROOT/'apps/gui/ipodjs_ui.c').read_text()
    root=(ROOT/'apps/root_menu.c').read_text()
    assert 'ipodjs_retailos_load_opaque(6,' in ui
    assert '298, wps->scrub_data' in ui
    assert '293, wps->shuffle_data' in ui
    assert 'IPODJS_WPS_BARS' not in root
    assert 'ipodjs_ui_draw_retailos_now_playing_activity(' not in root
    assert 'return bm ? bm : ipodjs_ui_retailos_music_cover();' in root
    for name in ('ipodjs_ui_retailos_music_cover',
                 'ipodjs_ui_draw_retailos_scrubber',
                 'ipodjs_ui_draw_retailos_shuffle_selector'):
        body=ui.split(name+'(',1)[1].split('\n}',1)[0]
        for forbidden in ('open(', 'load_', 'core_alloc', 'audio_stop'):
            assert forbidden not in body
    base=ROOT/'assets/ipodjs/apple/retailos-2.0.4/resources'
    for ordinal, size in ((6,(128,128)),(298,(13,28)),(293,(25,19))):
        path=base/f'{ordinal:03}.rga'
        if not path.exists():
            pytest.skip('Private Apple asset pack unavailable')
        data=path.read_bytes()
        assert data[:8]==b'RGA1'+struct.pack('<HH',*size)
        assert len(data)==8+size[0]*size[1]*3


def test_art_readiness_uses_proxy_until_real_art_on_every_page(tmp_path):
    source=(ROOT/'apps/root_menu.c').read_text()
    body=function(source,'static struct bitmap *root_menu_video_stock_wps_art(struct mp3entry *id3)')
    code=r'''
#include <assert.h>
#include <stddef.h>
#define HAVE_ALBUMART
struct bitmap { int id; };
struct mp3entry { int id; };
static struct bitmap note={1}, cover={2};
static struct bitmap *available;
static struct bitmap *root_menu_video_buffered_art(void) { return available; }
static struct bitmap *ipodjs_ui_retailos_music_cover(void) { return &note; }
'''+body+r'''
int main(void) {
    assert(root_menu_video_stock_wps_art(NULL)==&note);
    available=&cover;
    assert(root_menu_video_stock_wps_art(NULL)==&cover);
    available=NULL;
    assert(root_menu_video_stock_wps_art(NULL)==&note);
    return 0;
}
'''
    subprocess.run([str(compile_c(tmp_path,code,'art_ready'))],check=True)


def test_shuffle_retains_track_and_does_not_report_failed_change(tmp_path):
    source=(ROOT/'apps/root_menu.c').read_text()
    body=function(source,'bool root_menu_ipodjs_wps_adjust_page(int delta)')
    code=r'''
#include <assert.h>
#include <stdbool.h>
#define IPODJS_WPS_SHUFFLE 3
#define IPODJS_WPS_LYRICS 4
#define IPODJS_WPS_INFO 5
#define AUDIO_STATUS_PLAY 1
#define HZ 100
static int root_menu_video_wps_page=3, current_tick=100, result, saves, changes, notices;
static bool root_menu_video_wps_force_full;
static struct { bool playlist_shuffle; } global_settings;
struct playlist_info { bool started; };
static struct playlist_info playlist={true};
static struct playlist_info *playlist_get_current(void) { return &playlist; }
static int audio_status(void) { return 3; }
static int playlist_randomise(struct playlist_info *p,int seed,bool keep) {
    assert(p==&playlist && seed==100 && keep); changes++; return result;
}
static int playlist_sort(struct playlist_info *p,bool keep) {
    assert(p==&playlist && keep); changes++; return result;
}
static void settings_save(void) { saves++; }
static void splash(int ticks,const char *message) { (void)ticks;(void)message;notices++; }
static void ipodjs_wps_text_scroll(int delta) { (void)delta; }
'''+body+r'''
int main(void) {
    assert(root_menu_ipodjs_wps_adjust_page(1));
    assert(global_settings.playlist_shuffle && saves==1 && changes==1);
    root_menu_ipodjs_wps_adjust_page(1); assert(saves==1 && changes==1);
    result=-1; root_menu_ipodjs_wps_adjust_page(-1);
    assert(global_settings.playlist_shuffle && saves==1 && notices==1);
    result=0; root_menu_ipodjs_wps_adjust_page(-1);
    assert(!global_settings.playlist_shuffle && saves==2);
    root_menu_video_wps_page=0;
    assert(!root_menu_ipodjs_wps_adjust_page(1)); return 0;
}
'''
    subprocess.run([str(compile_c(tmp_path,code,'shuffle'))],check=True)


def test_scrubber_paints_empty_retail_rail_and_centres_source_diamond(tmp_path):
    source = (ROOT / 'apps/gui/ipodjs_ui.c').read_text()
    body = function(source, 'void ipodjs_ui_draw_retailos_scrubber(struct screen *display, int percent)')
    code = r'''
#include <assert.h>
#include <stdbool.h>
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
struct screen { int unused; };
struct ipodjs_retailos_image { int width; };
struct ipodjs_ui_retail_wps_cache {
    bool scrub_valid, chrome_valid;
    struct ipodjs_retailos_image scrub;
};
static struct ipodjs_ui_retail_wps_cache ipodjs_ui_retail_wps = {true,true,{13}};
static int rail_calls, marker_calls, marker_x;
static void ipodjs_ui_draw_retailos_music_progress(struct screen *s,
    int x, int y, int width, int percent) {
    assert(s && x==58 && y==207 && width==204 && percent==0);
    rail_calls++;
}
static void ipodjs_retailos_blit(struct screen *s,
    struct ipodjs_retailos_image *image, int x, int y) {
    assert(s && image==&ipodjs_ui_retail_wps.scrub && y==207);
    assert(rail_calls==marker_calls+1);
    marker_x=x; marker_calls++;
}
''' + body + r'''
int main(void) {
    struct screen display;
    int percentages[]={-10,0,25,50,75,100,110};
    int centres[]={58,58,108,159,210,261,261};
    for(int i=0;i<7;i++) {
        ipodjs_ui_draw_retailos_scrubber(&display, percentages[i]);
        assert(marker_x+6==centres[i]);
    }
    ipodjs_ui_retail_wps.scrub_valid=false;
    ipodjs_ui_draw_retailos_scrubber(&display,50);
    assert(marker_calls==7 && rail_calls==7);
    return 0;
}
'''
    subprocess.run([str(compile_c(tmp_path, code, 'scrub_render'))], check=True)
