#!/usr/bin/env python3
"""Run the actual target presentation sampler on host memory, without MMIO."""
import pathlib,re,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[2]
s=(root/'firmware/target/arm/s5l8702/ipod6g/videoout-6g.c').read_text()
a=s.index('static bool svid_present_frame(void)');start=s.index('{',a);level=1;b=start+1
while level:
 level+=(s[b]=='{')-(s[b]=='}');b+=1
fn=s[a:b]
h=(root/'firmware/export/videoout.h').read_text();frame=h[h.index('struct videoout_tv_frame {'):h.index('#if defined(HAVE_COMPOSITE_VIDEO_OUT)')]
c=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "videoout_geometry.h"
#define SVID_PLANAR_SOURCE_WIDTH 320
#define SVID_PLANAR_Y_WIDTH 320
#define SVID_PLANAR_Y_HEIGHT 240
#define SVID_PLANAR_GUARD 0
static bool svid_frame_pending;
static uint16_t svid_sample_x[320];
static int svid_tv_screen,svid_tv_overscan;
static uint8_t storage[320*240*3/2];
static uint8_t *svid_planar_write_buffer=storage;
static void svid_presentation(bool b){assert(b);}
static bool svid_begin_planar_update(void){return true;}
static bool svid_present_planar_update(void){return true;}
static uint32_t svid_rgb565_to_ycbcr(uint16_t p){return p;}
'''+frame+'\nstatic struct videoout_tv_frame svid_frame;\n'+fn+r'''
int main(void)
{
 uint8_t y[640*480],u[320*240],v[320*240];
 memset(y,80,sizeof(y));memset(u,90,sizeof(u));memset(v,100,sizeof(v));
 for(int wide=0;wide<2;wide++)for(int fit=0;fit<2;fit++)for(int dar=0;dar<2;dar++)
 for(int over=0;over<4;over++)
 {
   svid_tv_screen=wide;svid_tv_overscan=over;
   svid_frame=(struct videoout_tv_frame){.planes={y,u,v},.width=640,.height=480,
      .stride=640,.dar_n=dar?16:4,.dar_d=dar?9:3,.fill=fit};
   svid_frame_pending=true;assert(svid_present_frame());assert(!svid_frame_pending);
   struct videoout_geometry g;
   assert(videoout_calc_geometry(640,480,dar?16:4,dar?9:3,320,240,
      (struct videoout_rect){0,0,320,240},wide,fit,over,false,&g));
   for(int py=0;py<240;py++)for(int px=0;px<320;px++)
   {
     bool content=px>=g.destination.x && px<g.destination.x+g.destination.w &&
                  py>=g.destination.y && py<g.destination.y+g.destination.h;
     assert(storage[py*320+px]==(content?80:16));
   }
   assert(storage[320*240+120*160+80]==90);
   assert(storage[320*240+160*120+120*160+80]==100);
 }
 puts("PASS: actual target sampler, 32 aspect/fit/overscan cases; no decoder or MMIO emulation");
}
'''
# Center chroma coordinates in each 160x120 plane.
c=c.replace('120*160+80','60*160+80')
with tempfile.TemporaryDirectory(prefix='tv-present-') as tmp:
 p=pathlib.Path(tmp);(p/'test.c').write_text(c)
 subprocess.run(['cc','-std=c99','-g','-fsanitize=address,undefined','-I'+str(root/'firmware/export'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
