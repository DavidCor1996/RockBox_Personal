#!/usr/bin/env python3
"""Compile the actual target cache/compositor and scaler under ASan/UBSan."""
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[2]
source = (root/'firmware/target/arm/s5l8702/ipod6g/videoout-6g.c').read_text()
start = source.index('/* With 32-pixel guards')
end = source.index('\n#endif', source.index('static void svid_rgb565_expand', start))
production = source[start:end]
prelude = r'''
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define SVID_PLANAR_GUARD 0
#define LCD_WIDTH 320
#define LCD_HEIGHT 240
#define SVID_PLANAR_Y_WIDTH 640
#define SVID_PLANAR_Y_SIZE 307200
#define SVID_PLANAR_C_WIDTH 320
#define SVID_PLANAR_C_SIZE 76800
#define SVID_PLANAR_FRAME_SIZE 460800
#define SVID_PLANAR_BUFFER_COUNT 2
#define VIDEOOUT_ART_WIDTH 136
#define VIDEOOUT_ART_HEIGHT 186
#define VIDEOOUT_ART_SIZE 151776u
static uint32_t svid_output_framebuffer[480][640];
static uint8_t *svid_planar_write_buffer=(void *)svid_output_framebuffer;
static bool svid_active=true, svid_layer_planar=true, svid_mirror_enabled=true;
static uint32_t svid_rgb565_to_ycbcr(uint16_t p) { return p; }
'''
tests = r'''
static uint16_t expected_pixels[136*186];
static uint8_t payload[151776];
static uint8_t saved[460800];
int main(void)
{
    /* Every shape and stride, including one-pixel inputs and edge clamping.
     * Compare the production interpolator against an independent weighted
     * reference while checking row padding and allocation guard bytes. */
    for (unsigned h=1; h<=25; h++) for (unsigned w=1; w<=33; w++)
    {
        unsigned ss=w+3, ds=2*w+7;
        uint8_t src[28*36], dst[50*73+2];
        memset(dst,0xa5,sizeof(dst));
        for (unsigned k=0;k<sizeof(src);k++) src[k]=(k*37+13)&255;
        videoout_scale2x(dst+1,ds,src,ss,w,h);
        for (unsigned y=0;y<2*h;y++) for (unsigned x=0;x<2*w;x++)
        {
            unsigned total=0,n=0;
            for (unsigned j=0;j<=(y&1);j++)
                for (unsigned i=0;i<=(x&1);i++)
                { total+=src[MIN(y/2+j,h-1)*ss+MIN(x/2+i,w-1)]; n++; }
            assert(dst[1+y*ds+x]==(total+n/2)/n);
        }
        assert(dst[0]==0xa5);
        for (unsigned y=0;y<2*h;y++)
            for (unsigned x=2*w;x<ds;x++) assert(dst[1+y*ds+x]==0xa5);
    }
    for (unsigned i=0;i<sizeof(payload);i++) payload[i]=(i*11+7)&255;
    for (unsigned i=0;i<136*186;i++) expected_pixels[i]=(i*17)&65535;
    assert(!ipod6g_videoout_art_write(151777,payload,1));
    assert(!ipod6g_videoout_art_finish());
    for (unsigned off=0; off<sizeof(payload);)
    {
        unsigned n=MIN(4096,sizeof(payload)-off);
        assert(ipod6g_videoout_art_write(off,payload+off,n)); off+=n;
    }
    assert(ipod6g_videoout_art_finish());
    assert(!ipod6g_videoout_art_bind(expected_pixels,136,200,0));
    assert(ipod6g_videoout_art_bind(expected_pixels,136,10,34));
    memset(svid_planar_write_buffer,0x33,460800);
    svid_art_composite(expected_pixels,10,34,136,186,136);
    assert(!memcmp(svid_planar_write_buffer+68*640+20,payload,272));
    assert(!memcmp(svid_planar_write_buffer+307200+34*320+10,payload+101184,136));
    assert(!memcmp(svid_planar_write_buffer+384000+34*320+10,payload+126480,136));
    memcpy(saved,svid_planar_write_buffer,sizeof(saved));
    expected_pixels[185*136+135]^=1;
    memset(svid_planar_write_buffer,0x33,460800);
    svid_art_composite(expected_pixels,10,34,136,186,136);
    for (unsigned i=0;i<460800;i++) assert(svid_planar_write_buffer[i]==0x33);
    expected_pixels[185*136+135]^=1;
    /* Arbitrary dirty rectangles must compose exactly their intersection. */
    for (int y=0;y<186;y+=17) for (int x=0;x<136;x+=13)
    {
        int w=MIN(13,136-x),h=MIN(17,186-y);
        svid_art_composite(expected_pixels+y*136+x,x+10,y+34,w,h,136);
    }
    assert(!memcmp(saved,svid_planar_write_buffer,sizeof(saved)));
    ipod6g_videoout_art_clear();
    assert(!ipod6g_videoout_art_finish());
    assert(!ipod6g_videoout_art_bind(expected_pixels,136,10,34));
    /* RGB expansion must respect the final pixel and all three plane ends. */
    svid_rgb565_expand(expected_pixels,319,239,1,1,1);
    /* Reproduce a far-left strip repaint. Partitioned and full-frame RGB
     * conversion must agree even for odd strip widths and row boundaries. */
    static uint16_t screen[320*240];
    for (unsigned i=0;i<320*240;i++) screen[i]=(i*137+19)&65535;
    svid_rgb565_expand(screen,0,0,320,240,320);
    memcpy(saved,svid_planar_write_buffer,sizeof(saved));
    for (int strip=1;strip<=17;strip++)
    {
        memset(svid_planar_write_buffer,0xa5,460800);
        svid_rgb565_expand(screen,0,0,strip,240,320);
        svid_rgb565_expand(screen+strip,strip,0,320-strip,240,320);
        assert(!memcmp(saved,svid_planar_write_buffer,sizeof(saved)));
        for (int row=0;row<240;row++)
            svid_rgb565_expand(screen+row*320,0,row,strip,1,320);
        assert(!memcmp(saved,svid_planar_write_buffer,sizeof(saved)));
    }
    puts("Production scaler, plane boundaries, dirty rectangles, cache validation and left-strip partitioning passed");
}
'''
with tempfile.TemporaryDirectory(prefix='composite-gate-') as directory:
    path = pathlib.Path(directory)
    scale = root/'firmware/target/arm/s5l8702/ipod6g/videoout-scale2x.h'
    (path/'gate.c').write_text(prelude + '\n#include "'+str(scale)+'"\n' + production + tests)
    subprocess.run(['cc','-std=c11','-g','-O1','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-fno-omit-frame-pointer',
                    str(path/'gate.c'),'-o',str(path/'gate')],check=True)
    subprocess.run([str(path/'gate')],check=True)
