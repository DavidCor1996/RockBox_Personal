#!/usr/bin/env python3
"""Exercise the actual native compositor, driver entry and LCD scaler."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
target = root / 'firmware/target/arm/s5l8702/ipod6g'
driver = (target / 'videoout-6g.c').read_text()
entry = driver[driver.index('bool ipod6g_videoout_native_yuv('):]
entry = entry[:entry.index('\n#endif')]
app = (root / 'apps/video_playback.c').read_text()
scaler = app[app.index('static void video_scale_plane('):
             app.index('static uint8_t video_clamp_yuv(')]
header = (root / 'firmware/export/videoout.h').read_text()
frame = header[header.index('struct videoout_frame\n'):]
frame = frame[:frame.index('};') + 2]
prelude = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define LCD_WIDTH 320
#define LCD_HEIGHT 240
#define SVID_PLANAR_GUARD GUARD
#define SVID_PLANAR_Y_WIDTH (640+2*GUARD)
#define SVID_PLANAR_C_WIDTH (320+GUARD)
#define SVID_PLANAR_Y_SIZE (SVID_PLANAR_Y_WIDTH*480)
#define SVID_PLANAR_C_SIZE (SVID_PLANAR_C_WIDTH*240)
#define FRAME_SIZE (SVID_PLANAR_Y_SIZE+2*SVID_PLANAR_C_SIZE)
static uint8_t output[2*FRAME_SIZE+2];
static uint8_t *svid_planar_write_buffer;
static unsigned svid_planar_front_buffer;
static bool svid_frame_owned;
static bool svid_mirror_enabled=true, svid_layer_planar=true, active=true;
static bool present=true;
static unsigned presentations;
static bool ipod6g_videoout_active(void) { return active; }
static uint8_t *svid_planar_buffer(unsigned index)
{ return output+1+index*FRAME_SIZE; }
static bool svid_present_planar_update(void)
{
    presentations++;
    if (present) svid_planar_front_buffer ^= 1;
    return present;
}
'''
tests = r'''
static uint8_t source[3][672*480], low[3][320*240];
static uint8_t expected[3][640*480];
static unsigned char *lcd[3]={low[0],low[1],low[2]};

static void check(int width,int height,int dw,int dh,int ox,int oy,bool overlay)
{
    struct videoout_frame f={{source[0],source[1],source[2]},
                            width,height,672,ox,oy,dw,dh};
    for(int p=0;p<3;p++) {
        int d=p?2:1;
        for(int i=0;i<672*480;i++) source[p][i]=(i*37+i/672*53+p*61)&255;
        memset(low[p],p?128:16,sizeof(low[p]));
        video_scale_plane(low[p]+oy/d*(320/d)+ox/d,320/d,dw/d,dh/d,
                          source[p],672/d,width/d,height/d);
    }
    /* Change a single chroma sample: the whole tile must retain all of its
     * composited planes, including the unchanged luma. */
    if(overlay) low[1][oy/2*160+ox/2]^=127;
    for(int p=0;p<3;p++) {
        int d=p?2:1;
        videoout_scale2x(expected[p],640/d,low[p],320/d,320/d,240/d);
    }
    memset(output,0xa5,sizeof(output));
    unsigned front=svid_planar_front_buffer;
    assert(ipod6g_videoout_native_yuv(&f,lcd));
    assert(svid_planar_front_buffer==(front^1));
    uint8_t *base=svid_planar_buffer(svid_planar_front_buffer);
    for(int p=0;p<3;p++) {
        int d=p?2:1, ds=SVID_PLANAR_Y_WIDTH/d;
        unsigned stepx=((unsigned)(width/d)<<16)/(dw*2/d);
        unsigned stepy=((unsigned)(height/d)<<16)/(dh*2/d);
        for(int y=0;y<480/d;y++) for(int x=0;x<640/d;x++) {
            unsigned want=expected[p][y*(640/d)+x];
            int xx=x-ox*2/d, yy=y-oy*2/d;
            if(xx>=0 && yy>=0 && xx<dw*2/d && yy<dh*2/d &&
               !(overlay && xx<4/d && yy<4/d)) {
                unsigned ax=xx*stepx, ay=yy*stepy;
                unsigned ix=ax>>16, iy=ay>>16;
                unsigned fx=(ax>>8)&255, fy=(ay>>8)&255;
                unsigned nx=ix+1<(unsigned)(width/d)?ix+1:ix;
                unsigned ny=iy+1<(unsigned)(height/d)?iy+1:iy;
                unsigned total=source[p][iy*(672/d)+ix]*(256-fx)*(256-fy)
                    +source[p][iy*(672/d)+nx]*fx*(256-fy)
                    +source[p][ny*(672/d)+ix]*(256-fx)*fy
                    +source[p][ny*(672/d)+nx]*fx*fy;
                want=(total+32768)>>16;
            }
            assert(base[y*ds+GUARD/d+x]==want);
        }
        /* No writes to row padding or the front frame during composition. */
        for(int y=0;y<480/d;y++) for(int x=0;x<GUARD/d;x++) {
            assert(base[y*ds+x]==0xa5);
            assert(base[y*ds+GUARD/d+640/d+x]==0xa5);
        }
        base+=ds*(480/d);
    }
    for(int i=0;i<FRAME_SIZE;i++)assert(svid_planar_buffer(front)[i]==0xa5);
    assert(output[0]==0xa5 && output[sizeof(output)-1]==0xa5);
    for(int p=0;p<3;p++)for(int i=0;i<672*480;i++)
        assert(source[p][i]==((i*37+i/672*53+p*61)&255));

    /* Timeout cannot publish the incomplete/new frame. */
    front=svid_planar_front_buffer; present=false;
    assert(ipod6g_videoout_native_yuv(&f,lcd));
    assert(front==svid_planar_front_buffer); present=true;
    unsigned n=presentations;
    svid_frame_owned=true;
    assert(!ipod6g_videoout_native_yuv(&f,lcd));
    assert(presentations==n);
    svid_frame_owned=false;
    f.width=641; assert(!ipod6g_videoout_native_yuv(&f,lcd)); f.width=width;
    f.x=320; assert(!ipod6g_videoout_native_yuv(&f,lcd)); f.x=ox;
    f.stride=1; assert(!ipod6g_videoout_native_yuv(&f,lcd)); f.stride=672;
    f.height=0; assert(!ipod6g_videoout_native_yuv(&f,lcd)); f.height=height;
    f.display_width=0; assert(!ipod6g_videoout_native_yuv(&f,lcd));
    f.display_width=dw;
    active=false; assert(!ipod6g_videoout_native_yuv(&f,lcd)); active=true;
    assert(!ipod6g_videoout_native_yuv(NULL,lcd));
    assert(!ipod6g_videoout_native_yuv(&f,NULL));
    assert(n==presentations);
}
int main(void)
{
    check(640,480,320,240,0,0,false);
    check(640,480,320,240,0,0,true);
    check(640,360,320,180,0,30,true);
    check(426,240,320,180,0,30,false);
    check(480,360,160,120,4,58,true);
    check(640,2,320,2,0,238,false);
    check(2,480,2,240,318,0,false);
    puts("Native detail, aspect rectangles, overlays, strides, bounds and fallback passed");
}
'''
with tempfile.TemporaryDirectory(prefix='native-video-') as td:
    path = Path(td)
    code = (prelude + frame + '\n#include "videoout-scale2x.h"\n'
            '#include "videoout-native.h"\n' + scaler + entry + tests)
    (path / 'gate.c').write_text(code)
    for guard in (0, 32):
        subprocess.run(['cc', '-std=c11', '-g', '-O1', '-Wall', '-Wextra',
                        '-Werror', '-fsanitize=address,undefined',
                        f'-DGUARD={guard}', '-I', str(target),
                        str(path / 'gate.c'), '-o', str(path / 'gate')],
                       check=True)
        subprocess.run([str(path / 'gate')], check=True,
                       env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
