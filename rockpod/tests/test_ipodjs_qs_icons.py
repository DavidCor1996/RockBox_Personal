"""Check complete Quick Settings coverage and render the real C icon code."""
from pathlib import Path
import re
import runpy
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def test_all_quick_settings_have_resident_icons():
    source = (ROOT / 'apps/root_menu.c').read_text()
    enum = source[source.index('enum root_menu_video_qs_item {'):]
    enum = enum[:enum.index('};') + 2]
    start = source.index('static bool root_menu_video_draw_qs_icon(')
    end = source.index('\nstatic int ', start)
    draw = source[start:end]
    items = set(re.findall(r'\bIPODJS_QS_[A-Z_]+\b', enum)) - {'IPODJS_QS_COUNT'}
    assert items == set(re.findall(r'case (IPODJS_QS_[A-Z_]+):', draw))
    for forbidden in ('read_bmp_file', 'file_exists', 'core_alloc', 'malloc'):
        assert forbidden not in draw
    assert 'root_menu_video_qs_icon_asset' not in source
    assert 'root_menu_video_qs_service_icon' not in source
    generator = runpy.run_path(str(ROOT / 'tools/generate_ipodjs_qs_icons.py'))
    masks = [tuple(generator['glyph'](name)) for name in generator['names']]
    assert len(set(masks)) == 18
    assert all(all(0 <= row < (1 << 14) for row in mask) for mask in masks)
    harness = r'''
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#define HAVE_HARDWARE_CLICK
#define USB_ENABLE_ETHERNET
#define IPOD_ACCESSORY_PROTOCOL
#define IPOD_6G
#define HAVE_VIDEOOUT_BACKLIGHT_OFF
#define LCD_RGBPACK(r,g,b) (((r)<<16)|((g)<<8)|(b))
#include "apps/gui/ipodjs_qs_icons.h"
static unsigned foreground, pixels[18][18];
static bool dark;
static bool root_menu_video_dark(void) { return dark; }
static void lcd_set_foreground(unsigned color) { foreground = color; }
static void lcd_drawpixel(int x, int y) {
    assert(x>=0 && x<18 && y>=0 && y<18); pixels[y][x]=foreground;
}
static void lcd_hline(int x, int end, int y) {
    for (;x<=end;x++) lcd_drawpixel(x,y);
}
static void lcd_drawrect(int x,int y,int w,int h) {
    lcd_hline(x,x+w-1,y);lcd_hline(x,x+w-1,y+h-1);
    for (int r=y;r<y+h;r++) { lcd_drawpixel(x,r);lcd_drawpixel(x+w-1,r); }
}
''' + enum + draw + r'''
int main(void) {
    for(int theme=0;theme<2;theme++) {
        dark=theme;
        for(int selected=0;selected<2;selected++)
            for(int i=0;i<IPODJS_QS_COUNT;i++) {
                assert(root_menu_video_draw_qs_icon(0,0,i,selected));
                int white=0;
                for(int y=0;y<18;y++) for(int x=0;x<18;x++)
                    white += pixels[y][x] == LCD_RGBPACK(246,250,255);
                assert(white>15);
            }
    }
    return 0;
}
'''
    with tempfile.TemporaryDirectory(prefix='qs-icons-test-') as temp:
        src = Path(temp) / 'test.c'; exe = Path(temp) / 'test'
        src.write_text(harness)
        subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror',
                        '-I', str(ROOT), str(src), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
