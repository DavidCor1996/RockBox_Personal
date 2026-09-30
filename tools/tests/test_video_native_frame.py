#!/usr/bin/env python3
"""Exercise native crop/strides and surface handoff; no hardware claims."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
driver=(root/'firmware/target/arm/s5l8702/ipod6g/videoout-6g.c').read_text()
def function(name):
    start=driver.index('static bool '+name+'(')
    if driver[driver.index(')',start)+1:].lstrip().startswith(';'):
        start=driver.index('static bool '+name+'(',start+1)
    end=driver.index('\n}\n',start)+3
    return driver[start:end]
frame=(root/'firmware/export/videoout.h').read_text()
frame=frame[frame.index('struct videoout_tv_frame {'):frame.index('#if defined(HAVE_COMPOSITE_VIDEO_OUT)')]
code=r'''
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "videoout_geometry.h"
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define SVID_PLANAR_SOURCE_WIDTH 320
#define SVID_PLANAR_Y_WIDTH 320
#define SVID_PLANAR_Y_HEIGHT 240
#define SVID_PLANAR_GUARD 0
#define SVID_PLANAR_Y_SIZE (320*240)
#define SVID_PLANAR_C_SIZE (320*240/4)
#define SVID_PLANAR_FRAME_SIZE (320*240*3/2)
enum {SVID_FREE,SVID_WRITING,SVID_READY,SVID_SCANNING};
static unsigned svid_surface[2]={SVID_SCANNING,SVID_FREE};
static unsigned svid_planar_front_buffer;
static uint8_t buffers[2][SVID_PLANAR_FRAME_SIZE], *svid_planar_write_buffer;
static uint16_t svid_sample_x[320];
static int svid_tv_screen,svid_tv_overscan;
static bool svid_frame_pending,edge=true;
static uint8_t *svid_planar_buffer(unsigned i){return buffers[i];}
static void svid_presentation(bool on){(void)on;}
static void svid_extend_planar_edges(void){}
static void commit_dcache_range(void *p,size_t n){(void)p;(void)n;}
static bool svid_wait_for_field_edge(void){return edge;}
static int disable_irq_save(void){return 0;}
static void restore_irq(int x){(void)x;}
static uint32_t svid_rgb565_to_ycbcr(uint16_t p){return p;}
static uintptr_t registers[3];
#define SVID_COMPOSITOR_BASE 0
#define SVID_VP_PLANE0_PTR 0
#define SVID_VP_PLANE1_PTR 1
#define SVID_VP_PLANE2_PTR 2
#define SVID_REG(base,offset) registers[offset]
'''+frame+r'''
static struct videoout_tv_frame svid_frame;
'''+function('svid_begin_planar_write')+function('svid_present_planar_update')+function('svid_present_frame')+r'''
int main(void)
{
    static uint8_t y[736*480],u[368*240],v[384*240],snapshot[SVID_PLANAR_FRAME_SIZE];
    memset(y,180,sizeof(y));memset(u,90,sizeof(u));memset(v,160,sizeof(v));
    svid_frame=(struct videoout_tv_frame){.planes={y,u,v},.width=640,.height=448,
        .stride=736,.strides={736,368,384},.coded_width=720,.coded_height=480,
        .visible={16,16,640,448},.dar_n=640,.dar_d=448,
        .format=VIDEOOUT_YUV420P,.color=VIDEOOUT_SD_LIMITED};
    for(unsigned frame=0;frame<100;frame++)
    {
        unsigned old=svid_planar_front_buffer;
        memcpy(snapshot,buffers[old],sizeof(snapshot));
        assert(svid_present_frame());
        assert(!memcmp(snapshot,buffers[old],sizeof(snapshot)));
        assert(svid_surface[old]==SVID_FREE);
        assert(svid_surface[old^1]==SVID_SCANNING);
        assert(registers[0]==(uintptr_t)buffers[old^1]);
        assert(registers[2]-registers[0]==SVID_PLANAR_Y_SIZE);
        assert(registers[1]-registers[2]==SVID_PLANAR_C_SIZE);
    }
    unsigned front=svid_planar_front_buffer;
    memcpy(snapshot,buffers[front],sizeof(snapshot));
    edge=false;assert(!svid_present_frame());
    assert(svid_planar_front_buffer==front && svid_surface[front^1]==SVID_FREE);
    assert(!memcmp(snapshot,buffers[front],sizeof(snapshot)));
    edge=true;svid_frame.visible.x=100;assert(!svid_present_frame());
    svid_frame.visible.x=17;assert(!svid_present_frame());
    svid_frame.visible.x=16;svid_frame.strides[2]=300;assert(!svid_present_frame());
    assert(svid_planar_front_buffer==front);
    puts("PASS: native crop/unequal strides, 100 coherent surface handoffs, timeout retention and malformed-frame rejection (MMIO mocked)");
}
'''
with tempfile.TemporaryDirectory() as temp:
    p=Path(temp);(p/'test.c').write_text(code)
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-I',str(root/'firmware/export'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
