#!/usr/bin/env python3
"""Run the full production LCD YUV mirror against a bounded fake TV scanout."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'firmware/target/arm/s5l8702/ipod6g/videoout-6g.c').read_text()
a=s.index('bool ipod6g_videoout_mirror_yuv420('); b=s.index('{',a); end=b+1; depth=1
while depth:
    depth+=(s[end]=='{')-(s[end]=='}'); end+=1
source=s[a:end]
code=r'''
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define LCD_WIDTH 320
#define LCD_HEIGHT 240
#define SVID_PLANAR_Y_WIDTH 320
#define SVID_PLANAR_C_WIDTH 160
#define SVID_PLANAR_Y_SIZE (320*240)
#define SVID_PLANAR_C_SIZE (160*120)
static bool active=true,svid_ui_owner,svid_ui_batch,svid_frame_pending,svid_frame_owned;
static bool svid_mirror_enabled=true,svid_layer_planar=true,available=true;
static uint8_t scanout[SVID_PLANAR_Y_SIZE+2*SVID_PLANAR_C_SIZE];
static uint8_t *svid_planar_write_buffer=scanout;
static int begins,presents,commits,frames;
static bool ipod6g_videoout_active(void){return active;}
static bool svid_present_frame(void){frames++;return true;}
static bool svid_begin_planar_update(void){begins++;return available;}
static bool svid_present_planar_update(void){presents++;return true;}
static void svid_commit_planar_rect(int x,int y,int w,int h)
{assert(x==238&&y==4&&w==80&&h==62);commits++;}
static void svid_copy_yuv_plane(uint8_t*d,int ds,const uint8_t*s,int ss,int w,int h)
{while(h--){memcpy(d,s,w);d+=ds;s+=ss;}}
''' + source + r'''
static bool thumbnail(void)
{
 static uint8_t luma[80*62],cb[40*31],cr[40*31];
 memset(luma,0xb1,sizeof(luma));memset(cb,0x72,sizeof(cb));memset(cr,0x83,sizeof(cr));
 return ipod6g_videoout_mirror_yuv420(luma,cb,cr,0,0,80,238,4,80,62);
}
int main(void)
{
 for(int visit=0; visit<100; visit++)
 {
  memset(scanout,0x5a,sizeof(scanout));
  int old_begins=begins,old_presents=presents,old_frames=frames,old_commits=commits;
  for(int state=1;state<4;state++)
  {
   svid_ui_owner=state&1;svid_ui_batch=state&2;
   for(int pending=0;pending<2;pending++)
   {
    svid_frame_pending=pending;
    for(int tick=0;tick<100;tick++)assert(thumbnail());
   }
  }
  assert(begins==old_begins&&presents==old_presents&&frames==old_frames&&commits==old_commits);
  for(unsigned i=0;i<sizeof(scanout);i++)assert(scanout[i]==0x5a);
  svid_ui_owner=svid_ui_batch=svid_frame_pending=false;
  assert(thumbnail());assert(begins==old_begins+1&&presents==old_presents+1&&commits==old_commits+1);
  assert(scanout[4*320+238]==0xb1&&scanout[4*320+237]==0x5a);
 }
 int old_begins=begins,old_presents=presents;
 svid_frame_pending=true;assert(thumbnail());assert(frames==1&&begins==old_begins);
 svid_frame_pending=false;svid_frame_owned=true;assert(thumbnail());assert(begins==old_begins);
 svid_frame_owned=false;active=false;assert(!thumbnail());assert(begins==old_begins);
 active=true;available=false;assert(!thumbnail());assert(begins==old_begins+1&&presents==old_presents);
 puts("PASS: 60,000 handheld YUV thumbnails leave owned TV canvas unchanged; mirroring resumes on 100 releases; native frame, inactive and unavailable paths preserved");
}
'''
with tempfile.TemporaryDirectory(prefix='tv-yuv-owner-') as tmp:
    p=Path(tmp);(p/'test.c').write_text(code)
    subprocess.run(['cc','-std=c99','-Wall','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
