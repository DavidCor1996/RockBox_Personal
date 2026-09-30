#!/usr/bin/env python3
"""Host memory/layout smoke test of the actual TV renderer, not composite emulation."""
import pathlib,re,subprocess,sys,tempfile,struct
root=pathlib.Path(__file__).resolve().parents[2]
build=pathlib.Path(sys.argv[1]).resolve()
out=pathlib.Path(sys.argv[2]).resolve();out.mkdir(parents=True,exist_ok=True)
source=(root/'apps/gui/tv_ui.c').read_text()
source=re.sub(r'^#include .*$','',source,flags=re.M)
# Observe actual text placement, without replacing the production rasterizer.
source=source.replace('    text_offset(x,y,width,str,color,0);',
    '    observe_label(x,y,width,str);\n    text_offset(x,y,width,str,color,0);')

font=(build/'sysfont.c').read_text()
font=re.sub(r'^#include .*$','',font,flags=re.M)
font=font[:font.index('/* Exported structure definition. */')]
frame=(root/'firmware/export/videoout.h').read_text();frame=frame[frame.index('struct videoout_tv_frame {'):frame.index('#if defined(HAVE_COMPOSITE_VIDEO_OUT)')]
stub=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "videoout_geometry.h"
#define HAVE_COMPOSITE_VIDEO_OUT 1
#define LCD_RGBPACK(r,g,b) (((r>>3)<<11)|((g>>2)<<5)|(b>>3))
#define LCD_WHITE 65535
#define FONT_SYSFIXED -1
#define FONT_UI 12
#define REPEAT_ONE 2
#define FB_UNPACK_RED(p) (((p>>11)&31)*255/31)
#define FB_UNPACK_GREEN(p) (((p>>5)&63)*255/63)
#define FB_UNPACK_BLUE(p) ((p&31)*255/31)
typedef uint16_t fb_data;
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MAX_PATH 260
#define HZ 100
#define TIME_BEFORE(a,b) ((a)<(b))
#define AUDIO_STATUS_PLAY 1
#define AUDIO_STATUS_PAUSE 2
#define FORMAT_NATIVE 1
#define FORMAT_RESIZE 2
#define FORMAT_KEEP_ASPECT 4
#define P2STR(x) ((const char *)(x))
#define ACTION_STD_CANCEL 1
#define ACTION_STD_MENU 2
#define ACTION_STD_OK 3
#define CONTEXT_STD 0
#define SYS_USB_CONNECTED 100
struct mutex {bool held;};
static void mutex_init(struct mutex*m){m->held=false;}
static void mutex_lock(struct mutex*m){assert(!m->held);m->held=true;}
static void mutex_unlock(struct mutex*m){assert(m->held);m->held=false;}
static long current_tick;
static bool queued_input,held,busy_database,output_off;
static int interrupt_after_sleep;
static unsigned presentation_count;
static uint16_t transition_samples[32];
static bool record_transition,record_narrow;
static void sleep(int ticks)
{
 current_tick+=ticks;
 if(interrupt_after_sleep==1)queued_input=true;
 if(interrupt_after_sleep==2)held=true;
 if(interrupt_after_sleep==3)busy_database=true;
 if(interrupt_after_sleep==4)output_off=true;
}
typedef unsigned ucschar_t;
struct font {int maxwidth; unsigned height; int depth;unsigned first,def,size;const unsigned char *bits,*width;const unsigned *offset;};
struct bitmap {int width,height,format;unsigned char *data;};
struct dim {int width,height;};
struct mp3entry {char path[260];char *title,*artist,*album;unsigned long elapsed,length;};
struct gui_synclist {const char*title;int selected_item,nb_items;void *data;
 const char*(*callback_get_item_name)(int,void*,char*,size_t);};
static struct {int tv_screen,tv_interface,tv_text_size,tv_overscan,tv_now_playing;
 bool tv_fit,playlist_shuffle;int repeat_mode;} global_settings;
static struct font fixed={6,8,0};
static struct mp3entry track={"/Music/track.flac","Long Track Title for TV Layout","An Artist","The Album",134000,277000};
static const char *capture, *transition_capture;
static int transition_frame;
static int art_mode,decode_count;
static int label_count,label_x[4],label_width[4];
static bool metadata_seen,plot_seen;
static void observe_label(int x,int y,int width,const char *str)
{
 if(str && !strcmp(str,"2006   Mystery")) metadata_seen=true;
 if(str && !strncmp(str,"A student",9)) plot_seen=true;
 (void)y;const char *names[]={"Apps","Videos","Music","Settings"};
 for(int i=0;i<4;i++)if(str&&!strcmp(str,names[i]))
 {label_x[i]=x;label_width[i]=width;label_count++;break;}
}
static struct font *font_get(int id);
static unsigned glyph(struct font*f,unsigned ch){return (ch<f->first || ch>=f->first+f->size)?f->def-f->first:ch-f->first;}
static int font_get_width(struct font*f,unsigned ch){return f==&fixed?6:f->width[glyph(f,ch)];}
static const unsigned char *font_get_bits(struct font*f,unsigned ch);
static const unsigned char *utf8decode(const unsigned char*p,unsigned *ch)
{ *ch=*p++;if(*ch>=192){while((*p&192)==128)p++;*ch='?';}return p; }
static size_t strlcpy(char*d,const char*s,size_t n){size_t k=strlen(s);if(n){size_t c=MIN(k,n-1);memcpy(d,s,c);d[c]=0;}return k;}
static bool videoout_active(void){return !output_off;}
static bool ui_owner;
static void videoout_ui_batch(bool b){(void)b;}
static void videoout_ui_owner(bool b){ui_owner=b;}
static struct mp3entry *audio_current_track(void){return &track;}
static int audio_state=AUDIO_STATUS_PLAY;
static int audio_status(void){return audio_state;}
static bool button_queue_empty(void){return !queued_input;}
static bool button_hold(void){return held;}
static bool tagcache_is_usable(void){return !busy_database;}
static bool tagcache_commit_active(void){return false;}
static bool find_albumart(const struct mp3entry*p,char*s,int n,const struct dim*d)
{(void)p;(void)d;snprintf(s,n,"/cover.bmp");return art_mode!=0;}
static int read_bmp_file(const char*s,struct bitmap*b,int n,int f,void*c)
{(void)s;(void)c;decode_count++;
 assert(f & FORMAT_KEEP_ASPECT);if(art_mode==2)return -1;
 b->height=MAX(2,b->width/2);assert(b->width*b->height*2<=n);
 uint16_t *pixels=(uint16_t*)b->data;
 for(int i=0;i<b->width*b->height;i++)pixels[i]=LCD_RGBPACK(0,160,100);
 if(art_mode==3)strlcpy(track.path,"/Music/changed-during-decode.flac",sizeof(track.path));
 return b->width*b->height*2;}
static void lcd_set_viewport(void*p){(void)p;}
static void lcd_clear_display(void){}
static void lcd_puts(int x,int y,const char*s){(void)x;(void)y;(void)s;}
static void lcd_update(void){}
static int get_action(int c,int t){(void)c;(void)t;return ACTION_STD_CANCEL;}
static int default_event_handler(int a){return a;}
static void settings_save(void){}
static void videoout_set_preferences(int s,int o){(void)s;(void)o;}
static void videoout_present_ui(const uint16_t*p,int w,int h)
{
 assert(w<=426 && h==240);
 if(record_transition)
 {
  assert(presentation_count<32);
  transition_samples[presentation_count++]=p[10*w];
  assert(p[0]==LCD_RGBPACK(0,160,100));
  if(record_narrow)assert(p[10*w+w-1]==LCD_WHITE);
 }
 char frame_path[512];
 const char *output_path=capture;
 if(transition_capture)
 {
  snprintf(frame_path,sizeof(frame_path),"%s/fade-%02d.ppm",
           transition_capture,transition_frame++);
  output_path=frame_path;
 }
 if(!output_path)return;
 FILE*f=fopen(output_path,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",w,h);
 for(int i=0;i<w*h;i++) {unsigned char rgb[]={((p[i]>>11)&31)*255/31,((p[i]>>5)&63)*255/63,(p[i]&31)*255/31};fwrite(rgb,1,3,f);} fclose(f);
}
'''
test=r'''
static const char *home_name(int i,void*d,char*b,size_t n)
{(void)b;(void)n;return ((const char * const *)d)[i];}
static const char *name(int i,void*d,char*b,size_t n)
{(void)d;snprintf(b,n,"Setting %d: a long readable value",i);return b;}
int main(int argc,char**argv)
{
 assert(argc==2);global_settings.tv_interface=global_settings.tv_now_playing=1;
 /* Scaled Apple RGBA must not leak invisible color into rounded edges.
  * Verify clipping and native glyph fidelity with guarded destinations. */
 const unsigned char edge_pixels[]={255,255,255,31,0,0};
 const struct ipodjs_retailos_image edge={2,1,edge_pixels};
 uint16_t guarded[7]={123,0,0,0,0,0,456};
 asset_into(guarded+1,5,1,&edge,0,0,5,1);
 assert(guarded[0]==123 && guarded[6]==456);
 assert(guarded[1]==LCD_WHITE && guarded[5]==0);
 assert(guarded[3]==((16<<11)|(32<<5)|16));
 for(int i=1;i<5;i++)assert(guarded[i]>=guarded[i+1]);
 uint16_t clipped[3]={123,0,456};
 asset_into(clipped+1,1,1,&edge,-2,0,5,1);
 assert(clipped[0]==123 && clipped[2]==456);
 assert(clipped[1]==guarded[3]);
 uint16_t native[2]={0,0x1234};
 asset_into(native,2,1,&edge,0,0,2,1);
 assert(native[0]==LCD_WHITE && native[1]==0x1234);
 asset_into_alpha(native,2,1,&edge,0,0,5,1,0);
 assert(native[0]==LCD_WHITE && native[1]==0x1234);
 assert(blend(0,LCD_WHITE,128)==((16<<11)|(32<<5)|16));
 /* Real fade loop: monotonic samples, bounded time and exact destination. */
 begin();rect(0,0,cw,TV_H,LCD_RGBPACK(0,160,100));
 rect(0,10,cw,24,LCD_WHITE);record_transition=true;
 transition_valid=false;present_transition("First",0,1,0,10,cw,24);
 assert(presentation_count==1);
 presentation_count=0;long fade_start=current_tick;
 present_transition("Second",0,1,0,10,cw,24);
 assert(presentation_count==5 && current_tick-fade_start==16);
 for(unsigned i=1;i<presentation_count;i++)
  assert(transition_samples[i]>=transition_samples[i-1]);
 assert(transition_samples[0]<LCD_WHITE);
 assert(transition_samples[presentation_count-1]==LCD_WHITE);
 presentation_count=0;present_transition("Second",0,1,0,10,cw,24);
 assert(presentation_count==1);
 for(int reason=1;reason<=4;reason++)
 {
  presentation_count=0;interrupt_after_sleep=reason;
  present_transition("Cancel",reason,1,0,10,cw,24);
  assert(presentation_count==2 && canvas[10*cw]==LCD_WHITE);
  queued_input=held=busy_database=output_off=false;
 }
 interrupt_after_sleep=0;queued_input=true;presentation_count=0;
 present_transition("Skip",0,1,0,10,cw,24);assert(presentation_count==1);
 queued_input=false;presentation_count=0;
 rect(0,10,cw,24,LCD_WHITE);
 record_narrow=true;
 present_transition("Narrow",0,1,0,10,cw/2,24);
 record_narrow=false;
 assert(canvas[10*cw+cw-1]==LCD_WHITE);
 assert(presentation_count==5);
 record_transition=false;
 /* The right-hand album plane reverses the WPS slope, not source pixels. */
 global_settings.tv_screen=1;begin();
 uint16_t projected_pixels[16];
 for(int i=0;i<16;i++)projected_pixels[i]=i%4<2?
     LCD_RGBPACK(255,0,0):LCD_RGBPACK(0,0,255);
 struct bitmap projected={4,4,1,(unsigned char*)projected_pixels};
 album_cover_project(&projected,20,30,136,186);
 assert(canvas[30*cw+20]==bg);
 assert(canvas[41*cw+20]==LCD_RGBPACK(255,0,0));
 assert(canvas[39*cw+21]!=bg);
 assert(canvas[39*cw+21]!=LCD_RGBPACK(255,0,0));
 unsigned filtered=canvas[80*cw+87];
 assert((filtered&31)>0 && (filtered>>11)>0);

 assert(canvas[31*cw+155]==LCD_RGBPACK(0,0,255));
 assert(canvas[164*cw+20]==LCD_RGBPACK(255,0,0));
 assert(canvas[166*cw+20]!=bg);
 assert(canvas[215*cw+20]==bg);
 projected.width=1;projected.height=16;
 album_cover_project(&projected,20,30,136,186);
 projected.width=16;projected.height=1;
 album_cover_project(&projected,20,30,136,186);
 album_cover_project(NULL,20,30,136,186);
 uint16_t *stripes=malloc(136*136*sizeof(*stripes));assert(stripes);
 for(int y=0;y<136;y++)for(int x=0;x<136;x++)
  stripes[y*136+x]=(y&1)?LCD_WHITE:0;
 struct bitmap stripe_cover={136,136,1,(unsigned char*)stripes};
 begin();album_cover_project(&stripe_cover,20,0,136,186);
 int level0=canvas[40*cw+155]>>11,level1=canvas[41*cw+155]>>11;
 assert(level0>8 && level0<24 && abs(level0-level1)<=2);
 free(stripes);


 struct gui_synclist list={"Composite Video",5,40,NULL,name};
 const char *lines[]={"A long confirmation message that must wrap within the safe frame.","Unicode: \xe6\x97\xa5\xe6\x9c\xac"};
 char path[1024];
 for(int wide=0;wide<2;wide++)for(int size=0;size<3;size++)for(int over=0;over<4;over++)
 {
   global_settings.tv_screen=wide;global_settings.tv_text_size=size;global_settings.tv_overscan=over;
   snprintf(path,sizeof(path),"%s/list-%d-%d-%d.ppm",argv[1],wide,size,over);capture=path;
   label_count=0;tv_ui_set_home(true,true);tv_ui_set_section(TV_MUSIC);tv_list_draw(&list);capture=NULL;
   assert(label_count==4);
   for(int i=0;i<4;i++)
   {
    int left=safe.x+i*safe.w/4,right=safe.x+(i+1)*safe.w/4;
    assert(abs((2*label_x[i]+label_width[i])-(left+right))<=1);
    assert(label_x[i]>=left && label_x[i]+label_width[i]<=right);
   }
   tv_ui_set_home(false,false);
   label_count=0;tv_list_draw(&list);assert(label_count==0);
   const unsigned char *planes[3]={(void*)1,(void*)2,(void*)3};
   assert(tv_video_prepare(planes,320,240,320,4,3,12000,60000,false,true,NULL));
   assert(prepared.overlay==video_status && prepared.overlay_height==32);
   assert(prepared.overlay_y==safe.y && prepared.overlay_width==cw);
   assert(prepared.planes[0]==planes[0] && prepared.stride==320);
   assert(tv_video_prepare(planes,320,240,320,4,3,12000,60000,true,false,NULL));
   assert(!prepared.overlay);
   tv_wps_service(true);current_tick+=200;tv_wps_service(true);
   snprintf(path,sizeof(path),"%s/wps-%d-%d-%d.ppm",argv[1],wide,size,over);capture=path;
   tv_wps_draw();capture=NULL;
   tv_dialog_draw("Confirm",lines,2,"[Yes]   No   Select: OK");
   tv_test_screen();
 }
 const char * const home[]={"Music Library","Now Playing","Playlists","Cover Flow"};
 list=(struct gui_synclist){"Music",0,4,(void*)home,home_name};
 audio_state=0;
 for(int wide=0;wide<2;wide++)
 {
  global_settings.tv_screen=wide;global_settings.tv_text_size=0;global_settings.tv_overscan=3;
  snprintf(path,sizeof(path),"%s/home-%d.ppm",argv[1],wide);capture=path;
  tv_ui_set_home(true,true);tv_ui_set_section(TV_MUSIC);tv_list_draw(&list);capture=NULL;
  uint16_t *idle_copy=malloc(cw*TV_H*sizeof(*idle_copy));
  assert(idle_copy);memcpy(idle_copy,canvas,cw*TV_H*sizeof(*idle_copy));
  tv_list_art_draw(&list,ipodjs_ui_retailos_music_cover());
  assert(!memcmp(idle_copy,canvas,cw*TV_H*sizeof(*idle_copy)));
  free(idle_copy);
  tv_ui_set_home(false,false);
 }
 audio_state=AUDIO_STATUS_PLAY;
 const char * const video_apps[]={"YouTube","Netflix","DIRECTV","Twitch"};
 const struct bitmap *app_icons[]={&app0_bm,&app1_bm,&app2_bm,&app3_bm,NULL,NULL};
 const struct bitmap *posters[]={&movie_poster_bm,&show_poster_bm,&movie_poster_bm};
 for(int wide=0;wide<2;wide++)for(int size=0;size<3;size++)for(int over=0;over<4;over++)
 {
  global_settings.tv_screen=wide;global_settings.tv_text_size=size;global_settings.tv_overscan=over;
  tv_ui_set_section(TV_VIDEOS);tv_ui_set_home(true,false);
  list=(struct gui_synclist){"Videos",1,4,(void*)video_apps,home_name};
  snprintf(path,sizeof(path),"%s/grid-%d-%d-%d.ppm",argv[1],wide,size,over);capture=path;
  tv_grid_draw(&list,app_icons,0,2);capture=NULL;assert(ui_owner);
  snprintf(path,sizeof(path),"%s/home-videos-%d-%d-%d.ppm",argv[1],wide,size,over);capture=path;
  tv_home_videos_draw(&list,app_icons,&show_banner_bm,"Death Note",false,0,19);capture=NULL;
  snprintf(path,sizeof(path),"%s/featured-%d-%d-%d.ppm",argv[1],wide,size,over);capture=path;
  tv_home_videos_draw(&list,app_icons,&movie_banner_bm,"Death Note I",true,1,19);capture=NULL;
  int dots_y=safe.y+safe.h-68;
  int dots_step=MIN(8,(safe.w-12)/19),dots_x=safe.x+(safe.w-18*dots_step)/2;
  assert(canvas[dots_y*cw+dots_x+dots_step]==LCD_WHITE);
  assert(canvas[dots_y*cw+dots_x]==LCD_RGBPACK(116,120,126));
  tv_home_videos_draw(&list,app_icons,NULL,NULL,false,0,0);
  tv_ui_set_home(true,true);tv_grid_draw(&list,app_icons,0,2);
  tv_ui_set_home(true,false);tv_grid_draw(&list,NULL,0,3);
  snprintf(path,sizeof(path),"%s/shows-%d-%d-%d.ppm",argv[1],wide,size,over);capture=path;
  tv_ui_set_home(false,false);label_count=0;
  tv_ui_set_brand(&netflix_logo_bm,LCD_RGBPACK(180,19,29));
  tv_shelf_draw("TV Shows","Death Note",posters,NULL,"Select to browse",1,28,0,NULL);capture=NULL;assert(label_count==0);
  assert(canvas[0]==app_color && canvas[cw-1]==app_color);
  posters[1]=&movie_poster_bm;
  snprintf(path,sizeof(path),"%s/movies-%d-%d-%d.ppm",argv[1],wide,size,over);capture=path;
  tv_shelf_draw("Movies","Death Note I",posters,NULL,"Select for details",1,42,0,NULL);capture=NULL;
  tv_shelf_draw("Movies","A long movie title that scrolls within the safe area",posters,NULL,"Select for details",1,42,0,NULL);

  /* All watched masks affect only their own poster; draw the cached original
   * Netflix checkmark, and remove it again when watched state changes. */
  static uint16_t unwatched[TV_MAX_W*TV_H];
  tv_shelf_draw("Movies","Death Note I",posters,NULL,"Select for details",1,42,0,&watched_bm);
  memcpy(unwatched,canvas,sizeof(canvas));
  for(unsigned mask=1;mask<8;mask++)
  {
   tv_shelf_draw("Movies","Death Note I",posters,NULL,"Select for details",1,42,mask,&watched_bm);
   int changed[3]={0};
   int cell=safe.w/3;
   for(int y=safe.y;y<safe.y+safe.h;y++)for(int x=safe.x;x<safe.x+cell*3;x++)
    if(canvas[y*cw+x]!=unwatched[y*cw+x])changed[(x-safe.x)/cell]++;
   for(int i=0;i<3;i++)assert((changed[i]>0)==((mask&(1u<<i))!=0));
  }
  snprintf(path,sizeof(path),"%s/watched-%d-%d-%d.ppm",argv[1],wide,size,over);capture=path;
  tv_shelf_draw("Movies","Death Note I",posters,NULL,"Select for details",1,42,2,&watched_bm);capture=NULL;
  tv_shelf_draw("Movies","Death Note I",posters,NULL,"Select for details",1,42,0,&watched_bm);
  assert(!memcmp(unwatched,canvas,sizeof(canvas)));
  tv_shelf_draw("TV Shows","No videos found",NULL,NULL,"Hold Left to return",0,0,0,NULL);
  snprintf(path,sizeof(path),"%s/detail-%d-%d-%d.ppm",argv[1],wide,size,over);capture=path;
  metadata_seen=plot_seen=false;
  tv_detail_draw("Death Note I",&movie_poster_bm,NULL,"2006   Mystery",
    "A student finds a mysterious notebook that gives him the power to change the world.","Play",-1);capture=NULL;
  assert(metadata_seen && plot_seen);
  tv_detail_draw("Death Note I",&movie_poster_bm,NULL,"2006",NULL,"Play",-1);
  for(int choice=0;choice<2;choice++)
  {
   snprintf(path,sizeof(path),"%s/resume-%d-%d-%d-%d.ppm",argv[1],wide,size,over,choice);capture=path;
   tv_detail_draw("Death Note I",&movie_poster_bm,NULL,"2006",NULL,"Resume",choice);capture=NULL;
   int buttonw=(safe.w-8)/2;
   assert(canvas[(safe.y+safe.h-22)*cw+safe.x+choice*(buttonw+8)+1]==app_color);
   assert(canvas[(safe.y+safe.h-22)*cw+safe.x+(1-choice)*(buttonw+8)+1]!=app_color);
  }
  tv_ui_release();assert(!app_color && !app_logo && !home_navigation);
  tv_ui_set_section(TV_MUSIC);
  const char *const albums[]={"OK ORCHESTRA","The Maybe Man","Neotheater"};
  list=(struct gui_synclist){"Albums",0,3,(void*)albums,home_name};
  snprintf(path,sizeof(path),"%s/albums-%d-%d-%d.ppm",argv[1],wide,size,over);capture=path;
  tv_list_art_draw(&list,&album_bm);capture=NULL;
  if(wide==1 && size==1 && over==1)
  {
   list.selected_item=1;transition_frame=0;transition_capture=argv[1];
   tv_list_art_draw(&list,&album_bm);transition_capture=NULL;
   assert(transition_frame==5);
  }
  tv_list_art_draw(&list,NULL);
  posters[1]=&show_poster_bm;
 }
 for (int wide=0;wide<2;wide++)for(int over=0;over<4;over++)
 {
  global_settings.tv_screen=wide;global_settings.tv_overscan=over;
  assert(tv_guide_render(TV_GUIDE_BEGIN,0,0,0,0,1,NULL));
  assert(guide_owner && guide_building && ui_owner);
  tv_guide_render(TV_GUIDE_FILL,0,0,320,26,LCD_RGBPACK(220,238,249),NULL);
  tv_guide_render(TV_GUIDE_BITMAP,4,2,64,22,0,&directv_logo_bm);
  tv_guide_render(TV_GUIDE_TEXT,75,6,108,14,LCD_RGBPACK(0,82,155),"ALF");
  tv_guide_render(TV_GUIDE_TEXT,204,6,30,14,LCD_RGBPACK(124,162,193),"guide");
  tv_guide_render(TV_GUIDE_FILL,0,26,320,15,LCD_RGBPACK(183,214,233),NULL);
  tv_guide_render(TV_GUIDE_TEXT,4,26,72,14,LCD_RGBPACK(16,56,107),"Sun 7:30p");
  tv_guide_render(TV_GUIDE_TEXT,84,26,108,14,LCD_RGBPACK(16,56,107),"7:30p - 8:00p");
  tv_guide_render(TV_GUIDE_TEXT,172,26,148,14,LCD_RGBPACK(18,37,73),"TV-PG-V");
  tv_guide_render(TV_GUIDE_FILL,0,41,320,36,LCD_RGBPACK(2,111,175),NULL);
  tv_guide_render(TV_GUIDE_PARAGRAPH,4,45,225,32,LCD_WHITE,
   "The Tanner family welcomes an unexpected visitor from another planet.");
  tv_guide_render(TV_GUIDE_FILL,0,77,320,14,LCD_RGBPACK(18,37,73),NULL);
  tv_guide_render(TV_GUIDE_TEXT,4,77,62,14,LCD_WHITE,"Sun 20");
  for(int i=0;i<3;i++)tv_guide_render(TV_GUIDE_TEXT,71+i*84,77,78,14,LCD_WHITE,i==0?"7:30p":i==1?"8:00p":"8:30p");
  const char *channels[]={"100 RTRO","101 GAME","103 MOVI","104 NEWS","105 KIDS","106 MUSIC"};
  for(int row=0;row<6;row++)
  {
   int y=91+row*22;
   tv_guide_render(TV_GUIDE_FILL,0,y,320,22,LCD_RGBPACK(9,72,113),NULL);
   tv_guide_render(TV_GUIDE_FILL,0,y,68,22,LCD_RGBPACK(18,37,73),NULL);
   tv_guide_render(TV_GUIDE_FILL,0,y+21,320,1,LCD_RGBPACK(10,42,80),NULL);
   tv_guide_render(TV_GUIDE_TEXT,3,y+4,62,14,LCD_WHITE,channels[row]);
   for(int col=0;col<3;col++)
   {
    bool selected=row==0 && col==0;
    tv_guide_render(TV_GUIDE_FILL,68+84*col,y+1,83,19,
      selected?LCD_RGBPACK(254,196,37):LCD_RGBPACK(9,72,113),NULL);
    tv_guide_render(TV_GUIDE_TEXT,71+84*col,y+4,77,14,
      selected?LCD_RGBPACK(16,37,74):LCD_WHITE,
      row==0?(col==0?"ALF":"Family Ties"):row==2?"Mortal Kombat":"Programming");
   }
  }
  tv_guide_render(TV_GUIDE_FILL,0,223,320,17,LCD_RGBPACK(15,86,137),NULL);
  tv_guide_render(TV_GUIDE_TEXT,4,224,90,14,LCD_WHITE,"All Channels");
  tv_guide_render(TV_GUIDE_TEXT,215,224,101,14,LCD_WHITE,"Guide Options");
  tv_guide_render(TV_GUIDE_FILL,112,228,6,6,LCD_RGBPACK(220,30,20),NULL);
  tv_guide_render(TV_GUIDE_TEXT,121,224,199,14,LCD_WHITE,"-12h");
  tv_guide_render(TV_GUIDE_FILL,158,228,6,6,LCD_RGBPACK(0,220,80),NULL);
  tv_guide_render(TV_GUIDE_TEXT,167,224,153,14,LCD_WHITE,"+12h");
  tv_guide_render(TV_GUIDE_FILL,206,228,6,6,LCD_RGBPACK(254,196,37),NULL);
  struct tv_guide_picture pic={0};
  assert(!tv_guide_render(TV_GUIDE_PICTURE,0,0,0,0,0,&pic));
  tv_guide_render(TV_GUIDE_PRESENT,0,0,0,0,0,NULL);
  assert(!guide_building && !guide_mutex.held);
  assert(canvas[0]==asset_sample(0,&tv_directv_header,0,0,255));
  assert(canvas[cw-1]==asset_sample(0,&tv_directv_header,
      (tv_directv_header.width-1)<<8,0,255));
  assert(canvas[(TV_H-1)*cw]==LCD_RGBPACK(2,111,175));
  assert(canvas[TV_H*cw-1]==LCD_RGBPACK(2,111,175));

  /* Draw adjacent metadata/hint labels individually into blank canvas and
   * require every pixel to stay inside its assigned TV cell. */
  struct {int x,y,w,left,right;const char *s;} labels[]={
   {3,26,64,18,74,"Sunday 12:59p"},
   {76,26,108,84,210,"11:30a - 12:30p"},
   {172,26,148,216,261,"TV-PG-V"},
   {121,224,199,220,251,"-12h"},
   {167,224,153,268,301,"+12h"},
   {215,224,101,316,396,"Guide Options"},
   {71,77,249,79,181,"12:30p"},
   {155,77,165,187,289,"12:30p"}};
  static uint16_t saved_guide[TV_MAX_W*TV_H];memcpy(saved_guide,canvas,sizeof(canvas));
  for(unsigned n=0;n<sizeof(labels)/sizeof(labels[0]);n++)
  {
   memset(canvas,0,sizeof(canvas));
   guide_text(labels[n].x,labels[n].y,labels[n].w,14,labels[n].s,LCD_WHITE);
   bool ink=false;
   for(int y=0;y<TV_H;y++)for(int x=0;x<cw;x++)if(canvas[y*cw+x])
   {ink=true;assert(x>=guide_ref_x(labels[n].left)&&x<guide_ref_x(labels[n].right));}
   assert(ink);
  }
  memcpy(canvas,saved_guide,sizeof(canvas));
  /* A real decoded preview image, converted into the same YUV contract. */
  static unsigned char py[320*180],pu[160*90],pv[160*90];
  for(int y=0;y<180;y++)for(int x=0;x<320;x++)
  {
   unsigned p=((const uint16_t*)show_banner_bm.data)[(y*show_banner_bm.height/180)*show_banner_bm.width+x*show_banner_bm.width/320];
   int red=FB_UNPACK_RED(p),green=FB_UNPACK_GREEN(p),blue=FB_UNPACK_BLUE(p);
   py[y*320+x]=((66*red+129*green+25*blue+128)>>8)+16;
   if(!(x%2)&&!(y%2)) {pu[(y/2)*160+x/2]=((-38*red-74*green+112*blue+128)>>8)+128;pv[(y/2)*160+x/2]=((112*red-94*green-18*blue+128)>>8)+128;}
  }
  pic=(struct tv_guide_picture){{py,pu,pv},320,180,320};
  snprintf(path,sizeof(path),"%s/directv-%d-%d.ppm",argv[1],wide,over);capture=path;
  assert(tv_guide_render(TV_GUIDE_PICTURE,0,0,0,0,0,&pic));capture=NULL;
  struct videoout_rect picture=guide_picture_rect();
  assert(picture.x+picture.w+1<safe.x+safe.w);
  assert(canvas[(picture.y+2)*cw+picture.x+picture.w+1]==LCD_WHITE);
  /* Run the decoder's complete frame lifecycle, not just an isolated PIG
   * draw. Its unconditional NULL cleanup must not release guide ownership. */
  for(int frame_no=0;frame_no<100;frame_no++)
  {
   tv_video_prepare(NULL,0,0,0,0,0,0,0,false,false,NULL);
   assert(guide_owner && ui_owner && !guide_mutex.held);
   assert(tv_guide_render(TV_GUIDE_PICTURE,0,0,0,0,0,&pic));
   assert(canvas[(picture.y+2)*cw+picture.x+picture.w+1]==LCD_WHITE);
  }
  unsigned before=canvas[(picture.y+4)*cw+picture.x+4];
  tv_guide_render(TV_GUIDE_BEGIN,0,0,0,0,1,NULL);
  tv_guide_render(TV_GUIDE_FILL,0,0,320,77,LCD_WHITE,NULL);
  assert(canvas[(picture.y+4)*cw+picture.x+4]==before);
  tv_guide_render(TV_GUIDE_PRESENT,0,0,0,0,0,NULL);
  const unsigned char *full_planes[3]={py,pu,pv};
  tv_video_prepare(full_planes,320,180,320,16,9,0,1000,false,false,NULL);
  assert(!guide_owner && !ui_owner);
  tv_guide_render(TV_GUIDE_RELEASE,0,0,0,0,0,NULL);
  assert(!guide_owner && !ui_owner && !guide_mutex.held);
  assert(!tv_guide_render(TV_GUIDE_PICTURE,0,0,0,0,0,&pic));
  tv_guide_render(TV_GUIDE_BEGIN,0,0,0,0,0,NULL);
  global_settings.tv_interface=0;
  tv_guide_render(TV_GUIDE_PRESENT,0,0,0,0,0,NULL);
  assert(!guide_mutex.held && !guide_building);
  tv_guide_render(TV_GUIDE_RELEASE,0,0,0,0,0,NULL);
  global_settings.tv_interface=1;
 }
 tv_wps_enter();assert(ui_owner);tv_ui_batch(false,true);assert(ui_owner);
 tv_ui_leave();assert(!ui_owner);
 art_mode=1;tv_ui_leave();tv_wps_service(true);current_tick+=200;tv_wps_service(true);
 assert(art_ready && decode_count==1 && abs(art.width-2*art.height)<=1);
 for(int i=0;i<100;i++){tv_wps_service(true);tv_wps_draw();}
 assert(decode_count==1);
 strlcpy(track.path,"/Music/new.flac",sizeof(track.path));tv_wps_service(false);
 assert(!art_ready && decode_count==1);
 current_tick+=200;tv_wps_service(true);assert(art_ready && decode_count==2);
 art_mode=2;tv_ui_leave();tv_wps_service(true);current_tick+=200;tv_wps_service(true);
 assert(!art_ready && decode_count==3);
 art_mode=3;tv_ui_leave();tv_wps_service(true);current_tick+=200;tv_wps_service(true);
 assert(!art_ready && decode_count==4);
 tv_ui_leave();assert(!art_ready && !art_track[0]);
 puts("PASS: actual TV renderer, 24 layouts each for lists, grids, banners, posters, details and album panes, Home-only centered tabs, Featured dots/focus, eight edge-to-edge DIRECTV layouts, inset preview border, PIG redraw preservation/release and disabled-output mutex cleanup, app brand/reset, video status top placement and artwork cache/failure/obsolete-result lifecycle; filtered projection and monotonic/cancellable strip transitions, ASan/UBSan; bitmap decoder stubbed, not composite validation");
}
'''
if '--home-scroll-only' in sys.argv[4:]:
 test=r'''
static const char *name(int i,void*d,char*b,size_t n)
{(void)d;snprintf(b,n,"Application %d",i);return b;}
int main(int argc,char**argv)
{
 (void)argc;(void)argv;
 global_settings.tv_interface=1;global_settings.tv_screen=1;
 global_settings.tv_overscan=1;queued_input=false;
 tv_ui_set_section(TV_APPS);tv_ui_set_home(true,false);
 struct gui_synclist list={"Applications",0,7,NULL,name};
 const struct bitmap *icons[]={&app0_bm,&app1_bm,&app2_bm,
                               &app3_bm,&app0_bm,&app1_bm};
 tv_grid_draw(&list,icons,0,3);
 long start=current_tick;
 list.selected_item=1;tv_grid_draw(&list,icons,0,3);
 assert(current_tick==start && decode_count==0);
 list.selected_item=6;tv_grid_draw(&list,icons,6,3);
 assert(current_tick==start && decode_count==0);
 tv_ui_set_home(true,true);tv_grid_draw(&list,icons,6,3);
 assert(current_tick==start && decode_count==0);
 /* Confirm the bypass is scoped to Home, not other application effects. */
 tv_ui_set_home(false,false);
 present_transition("Other",0,7,safe.x,safe.y,80,20);
 assert(current_tick-start==MAX(1,HZ*160/1000));
 puts("PASS: Home selection, page change and tabs add no animation sleep or bitmap decode; other app fade retained. Bounded ASan/UBSan check, no stress loop.");
}
'''
# Load the exact deployed Apple RGA pixels and FNT glyphs, not imitations.
assets=root/'assets/ipodjs/apple'
extra=(root/'apps/gui/ipodjs_ui.h').read_text()
extra=extra[extra.index('enum ipodjs_tv_asset {'):extra.index('const uint16_t *ipodjs_ui_tv_background')]
extra+=(root/'apps/gui/tv_ui.h').read_text().split('enum tv_section ')[1].split(';')[0].join(['enum tv_section ', ';\n'])
extra+=(root/'apps/gui/tv_guide.h').read_text()+'\n'
extra+='struct ipodjs_retailos_image { unsigned short width,height; const unsigned char *pixels; };\n'
def array(name,values,ctype='unsigned char'):
 return 'static const '+ctype+' '+name+'[]={'+','.join(map(str,values))+'};\n'
for index,size in enumerate([15,19,23]):
 data=(assets/f'retailos-fonts/{size}-Helvetica-Bold-RetailOS-Apple.fnt').read_bytes()
 mw,h,asc,depth,first,default,count,nb,no,nw=struct.unpack_from('<4H6I',data,4)
 align=2 if nb<0xffdb else 4; offset=(36+nb+align-1)//align*align
 offs=struct.unpack_from('<'+('H' if align==2 else 'I')*no,data,offset)
 widths=data[offset+no*align:offset+no*align+nw]
 extra+=array(f'bits{index}',data[36:36+nb])+array(f'offset{index}',offs,'unsigned')+array(f'width{index}',widths)
 extra+=f'static struct font apple{index}={{ {mw},{h},{depth},{first},{default},{count},bits{index},width{index},offset{index} }};\n'
extra+='static struct font *font_get(int id){return id<0?&fixed:id==0?&apple0:id==1?&apple1:&apple2;}\n'
extra+='static int ipodjs_ui_tv_font(int size){return size;}\n'
extra+='static const unsigned char *font_get_bits(struct font*f,unsigned ch){return f==&fixed?_font_bits+_sysfont_offset[ch<256?ch:63]:f->bits+f->offset[glyph(f,ch)];}\n'
def rga(name,path):
 global extra
 data=path.read_bytes();w,h=struct.unpack_from('<HH',data,4);assert len(data)==8+w*h*3
 extra+=array(name,data[8:]);return w,h
base=assets/'retailos-2.0.4'
for name,ordinal in [('background',11),('cover',6)]:
 data=(base/f'resources/{ordinal:03d}.rga').read_bytes()[8:]
 extra+=array(name,[data[i]|data[i+1]<<8 for i in range(0,len(data),3)],'uint16_t')
# Optional read-only media fixtures come from the user's synced iPod library.
# Without them, retain the Apple retail cover as the rendering stress fixture.
fixture=pathlib.Path(sys.argv[3]) if len(sys.argv)>3 else None
for name in ['app0','app1','app2','app3','show_poster','show_banner','movie_poster','movie_banner','album','netflix_logo','directv_logo','watched']:
 if fixture:
  from PIL import Image
  im=Image.open(fixture/(name+'.png')).convert('RGB');w,h=im.size
  pixels=[((r>>3)<<11)|((g>>2)<<5)|(b>>3) for r,g,b in im.getdata()]
  extra+=array(name+'_pixels',pixels,'uint16_t')
  extra+=f'static struct bitmap {name}_bm={{{w},{h},1,(unsigned char*){name}_pixels}};\n'
 else:
  extra+=f'static struct bitmap {name}_bm={{128,128,1,(unsigned char*)cover}};\n'
extra+='static const uint16_t *ipodjs_ui_tv_background(void){return background;}\n'
extra+='static struct bitmap cover_bitmap={128,128,1,(unsigned char *)cover};\nstatic struct bitmap *ipodjs_ui_retailos_music_cover(void){return &cover_bitmap;}\n'
mapping=[['statusbar-white-background'],['optionbar-white-thumb-'+p for p in ['left','center','right']],['now-playing-progressbar-'+p for p in ['left','growth','right']],['now-playing-progressfill-'+p for p in ['left','growth','right']],['now-playing-white-play'],['now-playing-white-pause'],['now-playing-white-shuffle'],['now-playing-white-repeat'],['now-playing-white-repeat-once']]
for i,names in enumerate(mapping):
 entries=[]
 for j,name in enumerate(names):
  symbol=f'pixels{i}_{j}';w,h=rga(symbol,base/f'named/{name}.rga');entries.append(f'{{{w},{h},{symbol}}}')
 extra+=f'static const struct ipodjs_retailos_image images{i}[]={{'+','.join(entries)+'};\n'
extra+='static const struct ipodjs_retailos_image *ipodjs_ui_tv_asset(enum ipodjs_tv_asset id){static const struct ipodjs_retailos_image *p[]={'+','.join(f'images{i}' for i in range(len(mapping)))+'};return p[id];}\n'
with tempfile.TemporaryDirectory(prefix='tv-render-') as temp:
 p=pathlib.Path(temp)
 c=stub+'\n'+font+'\n'+extra+'\n'+(root/'apps/gui/tv_apple_assets.h').read_text()+'\n'+(root/'apps/gui/tv_directv_assets.h').read_text()+'\n'+frame+'\nstatic struct videoout_tv_frame prepared;\nstatic void videoout_prepare_frame(const struct videoout_tv_frame*f){if(f)prepared=*f;}\n'+source+'\n'+test
 (p/'test.c').write_text(c)
 subprocess.run(['cc','-std=c99','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(root/'firmware/export'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test'),str(out)],check=True)
