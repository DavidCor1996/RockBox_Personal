#!/usr/bin/env python3
"""Run both production frame paths with the target's real ownership guard."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
vo=(root/'apps/plugins/mpegplayer/video_out_rockbox.c').read_text()
h264=(root/'apps/video_playback.c').read_text()
driver=(root/'firmware/target/arm/s5l8702/ipod6g/videoout-6g.c').read_text()
def function(s,prefix):
 a=s.index(prefix);b=s.index('{',a);end=b+1;depth=1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[a:end]
prepare=function(driver,'void ipod6g_videoout_prepare_frame(')
mirror=function(driver,'void ipod6g_videoout_mirror_rgb565(')
guard=re.search(r'    if \(svid_ui_owner.*?return;',mirror,re.S).group()
head=(root/'firmware/export/videoout.h').read_text();frame=head[head.index('struct videoout_tv_frame {'):head.index('#if defined(HAVE_COMPOSITE_VIDEO_OUT)')]
code=r'''
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "videoout_geometry.h"
#define HAVE_LCD_COLOR
#define HAVE_COMPOSITE_VIDEO_OUT
#define LCD_WIDTH 320
#define LCD_HEIGHT 240
#define VO_NON_NULL_RECT 1
#define VO_VISIBLE 2
#define CLOCK_RATE 100
#define TS_SECOND 100
#define STREAM_PAUSED 2
#define MPEG_VIDEO_DISPLAY_FILL 1
#define TV_GUIDE_PICTURE 7
#define NF_CAP_BYTES 3168
#define VIDEO_STYLE_TWITCH 1
#define VIDEO_STYLE_TWITCH_LIVE 2
#define VIDEO_STYLE_INSTAGRAM_FEED 3
#define VIDEO_INSTAGRAM_CARD_W 240
#define VIDEO_INSTAGRAM_CARD_H 180
#define VIDEO_INSTAGRAM_CARD_X 40
#define VIDEO_INSTAGRAM_CARD_Y 30
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define DEBUGF(...) ((void)0)
static bool svid_frame_owned,svid_frame_pending,svid_ui_owner,svid_ui_batch;
static bool svid_mirror_enabled=true,active=true;
static int rgb_writes,presents,lcd_frames,overlay_height,paused;
static int submitted_width,submitted_height,submitted_stride;
static bool videoout_active(void){return active;}
static bool ipod6g_videoout_active(void){return active;}
''' +frame+r'''
static struct videoout_tv_frame svid_frame;
static bool svid_present_frame(void)
{assert(svid_frame.planes[0]);presents++;svid_frame_pending=false;return true;}
''' + prepare+r'''
static void rgb_mirror(const void*source,int x,int y,int width,int height,int stride)
{
''' +guard+r'''
 rgb_writes++;
}
static void bottom_refresh(void)
{static uint16_t black[320*32];rgb_mirror(black,0,208,320,32,320);}
static bool tv_video_prepare(const unsigned char*const p[3],int w,int h,int stride,int dn,int dd,unsigned long pos,unsigned long len,bool pause,bool status,const unsigned char*caption)
{
 (void)pos;(void)len;(void)pause;(void)status;(void)caption;
 if(!p){ipod6g_videoout_prepare_frame(NULL);return false;}
 submitted_width=w;submitted_height=h;submitted_stride=stride;
 struct videoout_tv_frame f={.planes={p[0],p[1],p[2]},.width=w,.height=h,.stride=stride,.dar_n=dn,.dar_d=dd};
 ipod6g_videoout_prepare_frame(&f);return true;
}
struct tv_guide_picture {const unsigned char*planes[3];int width,height,stride,dar_n,dar_d;};
static bool guide_render(int c,int x,int y,int w,int h,unsigned color,const void*p)
{(void)c;(void)x;(void)y;(void)w;(void)h;(void)color;(void)p;assert(svid_ui_owner);return true;}
static struct {bool(*tv_video_prepare)(const unsigned char*const*,int,int,int,int,int,unsigned long,unsigned long,bool,bool,const unsigned char*);bool(*tv_guide_render)(int,int,int,int,int,unsigned,const void*);} api={tv_video_prepare,guide_render};
#define rb (&api)
struct vo_rect {int l,t,r,b;};
static struct {bool tv_yuv420;int flags,display_width,display_height,image_width,image_height,tv_dar_n,tv_dar_d,src_x,src_y,output_x,output_y,output_width,output_height;struct vo_rect rc_vid;void(*post_draw_callback)(void);} vo={.tv_yuv420=true,.flags=3,.display_width=320,.display_height=240,.image_width=320,.image_height=240,.tv_dar_n=4,.tv_dar_d=3,.output_width=320,.output_height=240};
static struct {int display_mode;} settings;
static bool mpegplayer_livetv_launch,mpegplayer_livetv_pig,mpegplayer_livetv_pin_active,mpegplayer_livetv_weather_hidden,mpegplayer_netflix_launch=true;
static int mpegplayer_yuv_overlay_height(void){return overlay_height;}
static unsigned long stream_get_time(void){return 500;}
static unsigned long stream_get_duration(void){return 10000;}
static int stream_status(void){return paused?STREAM_PAUSED:0;}
static const unsigned char*mpegplayer_tv_caption(void){return NULL;}
static void vo_draw_black(const void*r){(void)r;}
static void vo_draw_frame_thumb(uint8_t*const*p,const void*r){(void)p;(void)r;lcd_frames++;}
static bool vo_draw_frame_scaled(uint8_t*const*p){(void)p;return false;}
static void yuv_blit(uint8_t*const*p,int sx,int sy,int stride,int x,int y,int w,int h)
{(void)p;(void)sx;(void)sy;(void)stride;(void)x;(void)y;(void)w;(void)h;lcd_frames++;}
static void vo_draw_yuv_overlay(uint8_t*const*p,int h,int top){(void)p;(void)h;(void)top;}
''' + function(vo,'static bool vo_tv_frame_valid(')+'\n'+function(vo,'void vo_draw_frame(')+'\n'+function(vo,'bool vo_show(')+'\n'+function(vo,'void vo_cleanup(')+r'''
struct video_launch {int style;bool fill,instagram_feed_expanded;};
struct video_twitch_chat_state {int panel_width;};
static bool nf_caption_snapshot(unsigned char*p){(void)p;return false;}
static void nf_caption_draw(unsigned char*const*p,int w,int h){(void)p;(void)w;(void)h;}
static void video_scale_plane(uint8_t*d,int ds,int dw,int dh,const uint8_t*s,int ss,int sw,int sh)
{(void)d;(void)ds;(void)dw;(void)dh;(void)s;(void)ss;(void)sw;(void)sh;}
static void video_draw_overlay(unsigned char*const*p,const struct video_launch*l,bool pause,uint32_t pos,uint32_t len,bool visible,int w)
{(void)p;(void)l;(void)pause;(void)pos;(void)len;(void)visible;(void)w;}
static void video_twitch_chat_draw(unsigned char*const*p,struct video_twitch_chat_state*c){(void)p;(void)c;}
#define lcd_blit_yuv yuv_blit
''' +function(h264,'static void video_draw_frame(')+r'''
int main(void)
{
 static uint8_t y[320*240],u[160*120],v[160*120],scaled[320*240*3/2];
 uint8_t*planes[3]={y,u,v};
 /* Validate the MPEG integration before any thumbnail or overlay blit.
  * The presenter stub does not read these buffers; actual full-sized plane
  * reads and pixel identity are exercised by test_tv_mpeg_native.py. */
 vo.display_width=640;vo.display_height=480;vo.image_width=672;
 vo.image_height=480;vo_draw_frame(planes);
 assert(submitted_width==640&&submitted_height==480&&submitted_stride==672);
 assert(svid_frame_owned);
 vo.tv_yuv420=false;vo_draw_frame(planes);assert(!svid_frame_owned);
 vo.tv_yuv420=true;vo.display_width=674;
 vo_draw_frame(planes);assert(!svid_frame_owned);
 vo.display_width=320;vo.display_height=240;vo.image_width=320;
 vo.image_height=240;
 presents=0;
 struct video_launch launch={0};struct video_twitch_chat_state chat={0};
 for(int cycle=0;cycle<100;cycle++)
 {
  vo.flags=3;
  for(int phase=0;phase<4;phase++)
  {
   /* Playing controls -> paused -> controls hidden -> untouched playback. */
   paused=phase==1;overlay_height=phase<2?64:0;
   vo_draw_frame(planes);assert(svid_frame_owned);
   assert(!svid_frame.planes[0]&&!svid_frame.planes[1]&&!svid_frame.planes[2]);
   int before=rgb_writes;
   for(int i=0;i<100;i++)bottom_refresh();assert(rgb_writes==before);
   video_draw_frame(y,u,v,320,240,320,scaled,&launch,paused,5000,100000,overlay_height!=0,&chat);
   assert(svid_frame_owned&&!svid_frame.planes[0]);
   for(int i=0;i<100;i++)bottom_refresh();assert(rgb_writes==before);
  }
  assert(vo_show(false));assert(!svid_frame_owned);bottom_refresh();
  vo_show(true);vo_draw_frame(planes);vo_cleanup();assert(!svid_frame_owned);bottom_refresh();
  vo.flags=3;mpegplayer_livetv_launch=mpegplayer_livetv_pig=svid_ui_owner=true;
  vo_draw_frame(planes);assert(!svid_frame_owned&&svid_ui_owner);
  int before=rgb_writes;bottom_refresh();assert(rgb_writes==before);
  mpegplayer_livetv_pig=false;mpegplayer_livetv_pin_active=true;vo_draw_frame(planes);assert(!svid_frame_owned);
  mpegplayer_livetv_pin_active=false;mpegplayer_livetv_weather_hidden=true;vo_draw_frame(planes);assert(!svid_frame_owned);
  mpegplayer_livetv_weather_hidden=mpegplayer_livetv_launch=svid_ui_owner=false;
 }
 assert(rgb_writes==200&&presents==900&&lcd_frames>0);
 puts("PASS: actual MPEG/H264 frame lifetimes block 80,000 between-frame bottom RGB refreshes across pause/control-hide/untouched playback; no retained plane pointers; hide, cleanup, guide, PIN and weather handoffs restore ownership");
}
'''
assert 'tv_video_prepare(NULL' not in function(h264,'static void video_draw_frame(')
assert h264.index('tv_video_prepare(NULL',h264.index('cleanup:')) < h264.index('lcd_clear_display();',h264.index('cleanup:'))
with tempfile.TemporaryDirectory(prefix='tv-video-lifetime-') as t:
 p=Path(t);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Wno-misleading-indentation','-fsanitize=address,undefined','-I'+str(root/'firmware/export'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
