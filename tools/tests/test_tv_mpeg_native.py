#!/usr/bin/env python3
"""Exercise real TV plane copying with MPEG-sized and padded decoded frames."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'firmware/target/arm/s5l8702/ipod6g/videoout-6g.c').read_text()


def function(text, prefix):
    start = text.index(prefix)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


header = (root / 'firmware/export/videoout.h').read_text()
frame = header[header.index('struct videoout_tv_frame {'):
               header.index('#if defined(HAVE_COMPOSITE_VIDEO_OUT)')]
code = r'''
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "videoout_geometry.h"
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define SVID_PLANAR_Y_WIDTH 704
#define SVID_PLANAR_SOURCE_WIDTH 640
#define SVID_PLANAR_Y_HEIGHT 480
#define SVID_PLANAR_GUARD 32
#define FRAME_BYTES (704*480*3/2)
static uint8_t buffers[2][FRAME_BYTES];
static uint8_t *svid_planar_write_buffer;
static unsigned front, presents;
static int svid_sample_x[640],svid_tv_screen,svid_tv_overscan;
static bool svid_frame_owned,svid_frame_pending;
static bool videoout_active(void){return true;}
static void svid_presentation(bool enabled){assert(enabled);}
static bool svid_begin_planar_write(bool preserve)
{assert(!preserve);svid_planar_write_buffer=buffers[front^1];return true;}
static bool svid_present_planar_update(void){front^=1;presents++;return true;}
static uint32_t svid_rgb565_to_ycbcr(unsigned p){return p;}
''' + frame + r'''
static struct videoout_tv_frame svid_frame;
static bool svid_present_frame(void);
''' + function(source, 'void ipod6g_videoout_prepare_frame(') + '\n' + function(
    source, 'static bool svid_present_frame(void)\n{') + r'''
static uint8_t src[3][736*480];
static void check(int w,int h,int ss,int dn,int dd,int tvwide,
                  int dx,int dy,int dw,int dh)
{
    for(int p=0;p<3;p++)for(int i=0;i<736*480;i++)
        src[p][i]=(i*37+i/ss*53+p*61)&255;
    struct videoout_tv_frame f={.planes={src[0],src[1],src[2]},
        .width=w,.height=h,.stride=ss,.strides={ss,ss/2,ss/2},
        .coded_width=ss,.coded_height=h,.visible={0,0,w,h},
        .dar_n=dn,.dar_d=dd};
    svid_tv_screen=tvwide;
    unsigned before=presents;
    ipod6g_videoout_prepare_frame(&f);
    assert(presents==before+1 && svid_frame_owned);
    assert(!svid_frame.planes[0]&&!svid_frame.planes[1]&&!svid_frame.planes[2]);
    uint8_t *out=buffers[front];
    for(int p=0;p<3;p++)
    {
        int d=p?2:1,os=704/d;
        for(int y=0;y<480/d;y++)for(int x=0;x<640/d;x++)
        {
            unsigned expected=p?128:16;
            int xx=x-dx/d,yy=y-dy/d;
            if(xx>=0&&yy>=0&&xx<dw/d&&yy<dh/d)
                expected=src[p][(yy*(h/d)/(dh/d))*(ss/d)+xx*(w/d)/(dw/d)];
            assert(out[y*os+32/d+x]==expected);
        }
        out+=os*(480/d);
    }
    /* Decoder storage can be reused immediately after the synchronous call. */
    uint8_t first=buffers[front][dy*704+32+dx];
    memset(src,0,sizeof(src));
    assert(buffers[front][dy*704+32+dx]==first);
    ipod6g_videoout_prepare_frame(NULL);assert(!svid_frame_owned);
}
int main(void)
{
    check(640,480,672,4,3,0,0,0,640,480);
    check(640,360,672,16,9,0,0,60,640,360);
    check(640,360,672,16,9,1,0,0,640,480);
    check(720,480,736,4,3,0,0,0,640,480);
    check(480,360,512,4,3,0,0,0,640,480);
    puts("PASS: native MPEG pixels, padded stride, 4:3/16:9 DAR, letterbox and decoder lifetime");
}
'''
with tempfile.TemporaryDirectory(prefix='mpeg-native-') as temp:
    path = Path(temp)
    (path / 'test.c').write_text(code)
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-g',
                    '-fsanitize=address,undefined', '-I' + str(root / 'firmware/export'),
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
