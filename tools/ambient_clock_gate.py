#!/usr/bin/env python3
"""Exercise the production ambient renderer/controller with a bounded host HAL."""
import argparse
from pathlib import Path
import subprocess
import shutil

parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, default=Path('/tmp/ambient-clock-gate'))
a = parser.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
r = Path(__file__).resolve().parents[1]
stubs = a.output / 'include'
stubs.mkdir(exist_ok=True)
common = r'''
#ifndef AMBIENT_TEST_HAL
#define AMBIENT_TEST_HAL
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#define HAVE_DOCKED_AMBIENT_CLOCK
#define HAVE_WHEEL_POSITION
#define ARRAYLEN(a) (sizeof(a)/sizeof((a)[0]))
#define HZ 100
#define DEBUGF(...)
#define LCD_WIDTH 320
#define LCD_HEIGHT 240
#define ROCKBOX_DIR ".rockbox"
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define TIME_AFTER(a,b) ((long)((b)-(a))<0)
#define LCD_RGBPACK(r,g,b) (((r)>>3)<<11|((g)>>2)<<5|((b)>>3))
#define RGB_UNPACK_RED(v) (((v)>>11)*255/31)
#define RGB_UNPACK_GREEN(v) ((((v)>>5)&63)*255/63)
#define RGB_UNPACK_BLUE(v) (((v)&31)*255/31)
#define FB_SCALARPACK_LCD(v) (v)
#define FB_UNPACK_SCALAR_LCD(v) (v)
#define DRMODE_SOLID 0
#define FONT_UI 1
#define CONTEXT_TREE 0
#define ALLOW_SOFTLOCK 0
#define SCREEN_MAIN 0
#define LANG_WEEKDAY_SUNDAY 0
#define LANG_MONTH_JANUARY 7
static const char *words[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat",
 "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
#define str(n) words[n]
#define ACTION_NONE 0
#define ACTION_UNKNOWN 1
#define ACTION_STD_OK 2
#define ACTION_TREE_WPS 3
#define ACTION_TREE_STOP 4
#define ACTION_TREE_POWER_MENU 5
#define ACTION_REDRAW 6
#define SYS_USB_CONNECTED 100
#define SYS_CHARGER_CONNECTED 101
#define SYS_CHARGER_DISCONNECTED 102
#define SYS_BATTERY_UPDATE 103
static bool suppressed;
static bool notification_manager_banners_suppressed(void){return suppressed;}
static void notification_manager_set_banners_suppressed(bool value){suppressed=value;}
static void default_event_handler(int action){(void)action;}
#define IS_SYSEVENT(v) ((v)>=100)
typedef uint16_t fb_data;
struct viewport {int x,y,width,height,font,drawmode; void *buffer;};
struct mp3entry {char *albumartist, *artist, *album, *path;};
static struct {bool ambient_clock; int ambient_delay, ambient_colors;
 int ambient_brightness; bool ambient_reduced_motion, ambient_weather;
 int timeformat;} global_settings = {true, 2, 0, 1, false, true, 0};
static long current_tick;
static int audio, raw, wheel = -1, action_to_send = ACTION_STD_OK;
static int action_count, theme_depth, frames, reads, stop_after = 12;
static bool output_active = true, held, queued, disconnect, start_music, raw_wake;
static fb_data framebuffer[240][320];
static struct viewport initial_viewport = {.width=320,.height=240};
static struct viewport *lcd_current_viewport = &initial_viewport;
static fb_data *backdrop = (void*)0x1234;
static struct tm test_time = {.tm_year=126,.tm_mon=8,.tm_mday=12,
                              .tm_hour=20,.tm_min=41,.tm_wday=6};
static struct mp3entry track = {NULL,"Artist","Album","/song.mp3"};
#define strlcpy test_strlcpy
#define strlcat test_strlcat
static size_t strlcpy(char *d,const char *s,size_t n)
{size_t l=strlen(s);if(n){size_t c=MIN(n-1,l);memcpy(d,s,c);d[c]=0;}return l;}
static size_t strlcat(char *d,const char *s,size_t n)
{size_t l=strlen(d);return l+strlcpy(d+l,s,n-l);}
static int read_line(int fd,char *line,int size)
{int n=0;char ch;++reads;while(n<size-1&&read(fd,&ch,1)==1){
if(ch=='\n')break;line[n++]=ch;}line[n]=0;return n;}
static int audio_status(void){return audio;}
static bool videoout_active(void){return output_active;}
static bool button_hold(void){return held;}
static int button_status(void){return raw;}
static int wheel_status(void){return wheel;}
static bool button_queue_empty(void){return !queued;}
static struct mp3entry *audio_current_track(void){return &track;}
static struct tm *get_time(void){return &test_time;}
static void yield(void){}
static void lcd_init_viewport(struct viewport *v)
{assert(v->width==320&&v->height==240&&v->buffer==NULL);}
static void lcd_set_viewport(struct viewport *v){lcd_current_viewport=v;}
static fb_data *lcd_get_backdrop(void){return backdrop;}
static void lcd_set_backdrop(fb_data *v){backdrop=v;}
static void lcd_set_drawmode(int m){(void)m;}
static void lcd_bitmap(const fb_data *p,int x,int y,int w,int h)
{assert(x>=0&&y>=0&&x+w<=320&&y+h<=240);memcpy(&framebuffer[y][x],p,w*2);}
static void lcd_update(void){++frames;}
static void lcd_clear_display(void){memset(framebuffer,0,sizeof(framebuffer));}
static void viewportmanager_theme_enable(int s,bool b,void *p)
{(void)s;(void)b;(void)p;++theme_depth;}
static void viewportmanager_theme_undo(int s,bool b)
{(void)s;(void)b;--theme_depth;}
#define MAX_PATH 260
#define FORMAT_NATIVE 1
#define FORMAT_RESIZE 2
#define FORMAT_KEEP_ASPECT 4
struct bitmap {int width,height,format;unsigned char *data;};
static fb_data art_workspace[2][384*384];
static bool art_leased, committing;
static void *albumlist_ambient_workspace(int slot,size_t *bytes)
{art_leased=true;*bytes=sizeof(art_workspace[slot]);return art_workspace[slot];}
static void albumlist_ambient_release(void){art_leased=false;}
static bool tagcache_commit_active(void){return committing;}
static int read_bmp_file(const char *path,struct bitmap *bm,size_t bytes,int flags,void *p)
{(void)flags;(void)p;++reads;assert(bytes>=384*384*2);
 if(strstr(path,"broken"))return -1;
 for(int i=0;i<384*384;++i)((fb_data*)bm->data)[i]=LCD_RGBPACK(i%255,100,60);
 return 384*384*2;}
static void action_wait_for_release(void){}
static int get_action(int context,int timeout)
{(void)context;current_tick+=timeout;if(++action_count==stop_after){
 if(raw_wake){raw=1;return 0;}
 if(disconnect){output_active=false;return 0;}
 if(start_music){audio=1;return 0;}
 return action_to_send;}return 0;}
#endif
'''
(stubs / 'test_hal.h').write_text(common)
for name in ['config.h','debug.h','system.h','kernel.h','lcd.h','action.h','button.h',
             'audio.h','settings.h','file.h','font.h','misc.h','timefuncs.h','playback.h',
             'metadata.h','videoout.h','viewport.h','screen_access.h','string-extra.h','lang.h','notification_manager.h','albumlist_art.h','bmp.h','tagcache.h']:
    (stubs / name).write_text('#include "test_hal.h"\n')
for name in ['ambient_clock.c', 'ambient_clock.h', 'ambient_clock_font.h', 'ambient_clock_icons.h']:
    shutil.copy2(r / 'apps/gui' / name, a.output / name)
test = r'''
#include "AMBIENT_SOURCE"
static void capture(const char *name)
{
 FILE *f=fopen(name,"wb");assert(f);fprintf(f,"P6\n320 240\n255\n");
 for(int y=0;y<240;++y)for(int x=0;x<320;++x){unsigned p=framebuffer[y][x];
 unsigned char rgb[]={RGB_UNPACK_RED(p),RGB_UNPACK_GREEN(p),RGB_UNPACK_BLUE(p)};
 fwrite(rgb,1,3,f);}fclose(f);
}
static void fixture(const char *stamp,const char *temp,const char *unit)
{
 FILE *f=fopen(".rockbox/rockpod/weather/forecast.tsv","w");assert(f);
 fprintf(f,"rockpod_weather_v1\tHalifax\t44\t-63\tAmerica/Halifax\t"
 "2026-09-12T23:00Z\tOpen-Meteo\t%s\n"
 "current\t%s\tclear\tClear skies\t%s\t0\t4\tNW\t1\ttest\n",unit,stamp,temp);
 fclose(f);
}
static void reset_run(void)
{audio=0;output_active=true;held=queued=false;raw=0;wheel=-1;
 action_count=0;disconnect=start_music=false;action_to_send=ACTION_STD_OK;}
int main(void)
{
 /* Production scanner must visit the entire catalog, beyond the old cap. */
 FILE *catalog=fopen(".rockbox/albumlist/index.tsv","w");assert(catalog);
 for(int i=0;i<1000;++i)fprintf(catalog,"%d\tthumb.bmp\tslide.bmp\tArtist\tAlbum\t1\tMusic\n",i);
 fclose(catalog);
 current_tick=100;art_begin();art.random=1;
 for(int i=0;i<63;++i){current_tick+=10;art_service();}
 assert(art.candidates==1000&&art.loading&&art.fd==-1);
 current_tick+=10;art_service();assert(art.ready);
 int cached_reads=reads;draw(3*HZ);assert(reads==cached_reads);
 capture("album-cached.ppm");
 char previous[MAX_PATH];strlcpy(previous,art.previous,sizeof(previous));
 current_tick+=3*HZ;
 for(int i=0;i<64;++i){current_tick+=10;art_service();}
 assert(art.candidates==999&&art.pending&&strcmp(previous,art.previous));
 int old_current=art.current;current_tick=art.shown+ART_HOLD+ART_FADE+1;
 art_service();assert(art.current!=old_current&&!art.pending);
 committing=true;int old_reads=reads;art_service();assert(old_reads==reads);committing=false;
 art_close();albumlist_ambient_release();assert(!art_leased);
 art_begin();current_tick+=HZ;art_service();assert(art.fd>=0);
 int scan_fd=art.fd;art_close();assert(fcntl(scan_fd,F_GETFD)==-1);
 art.loading=true;strlcpy(art.selected,"broken.bmp",sizeof(art.selected));
 strlcpy(art.fallback,"good.bmp",sizeof(art.fallback));
 art_service();assert(art.loading&&!art.ready&&!art.fallback[0]);
 art_service();assert(art.ready&&!art.loading);
 art_close();albumlist_ambient_release();
 remove(".rockbox/albumlist/index.tsv");memset(&art,0,sizeof(art));art.fd=-1;
 printf("PASS: 1000-album catalog, no immediate repeat, complete fade, database deferral\n");
 setenv("TZ","America/Halifax",1);tzset();
 struct tm daylight = test_time;daylight.tm_isdst=1;daylight.tm_min=0;
 assert(parse_stamp("2026-09-12T20:00")==mktime(&daylight));
 setenv("TZ","UTC",1);tzset();
 assert(parse_stamp("2026-02-30T12:00")==0);
 assert(parse_stamp("2026-13-01T12:00")==0);
 assert(parse_stamp("2026-09-12T24:00")==0);
 assert(parse_stamp("2026-09-12T2x:00")==0);
 assert(parse_stamp("2028-02-29T12:00")>0);
 FILE *forecast=fopen(".rockbox/rockpod/weather/forecast.tsv","w");assert(forecast);
 fprintf(forecast,"rockpod_weather_v1\tHalifax\t44\t-63\tAmerica/Halifax\t2026-09-12T20:00Z\t2026-09-12\tmetric\n");
 fprintf(forecast,"2026-09-12\tclear\tClear skies\t10\t20\n");
 for(int h=19;h<=21;++h)fprintf(forecast,"hourly\t2026-09-12T%02d:00\train\tRain\t%d\t20\t4\t180\t1\topen-meteo\n",h,h-2);
 fclose(forecast);weather_load();
 assert(weather.valid&&weather.forecast&&!strcmp(weather.temp,"18")&&weather.icon==6);
 int forecast_reads=reads;draw(10000);assert(reads==forecast_reads);capture("hourly.ppm");
 test_time.tm_hour=21;test_time.tm_min=0;weather_load();
 assert(weather.valid&&weather.forecast&&!strcmp(weather.temp,"19"));
 forecast=fopen(".rockbox/rockpod/weather/forecast.tsv","a");assert(forecast);
 fprintf(forecast,"current\t2026-09-12T20:55\tclear\tClear skies\t22\t0\t4\t180\t1\tlive\n");fclose(forecast);
 weather_load();assert(weather.valid&&!weather.forecast&&!strcmp(weather.temp,"22"));
 test_time.tm_mday=14;weather_load();assert(!weather.valid);
 test_time.tm_mday=12;test_time.tm_hour=18;weather_load();assert(!weather.valid);
 test_time.tm_hour=20;test_time.tm_min=41;
 printf("PASS: hourly-only sync, hour rollover, live priority, future and expired rejection\n");
 fixture("2026-09-12T20:00","18","metric");weather_load();
 assert(weather.valid&&weather.units=='C'&&!strcmp(weather.temp,"18"));
 assert(weather.icon==0);
 assert(weather_icon("clear",false)==1);
 assert(weather_icon("partly_cloudy",false)==3);
 assert(weather_icon("rain",true)==6);
 assert(weather_icon("snow",true)==7);
 assert(weather_icon("thunderstorm",true)==9);
 assert(weather_icon("invalid",true)==4);
 int before=reads;
 draw(10000);assert(reads==before);capture("balanced.ppm");
 for(unsigned i=0;i<10;++i){weather.icon=i;draw(10000);char path[40];
 snprintf(path,sizeof(path),"icon-%u.ppm",i);capture(path);}
 weather.icon=0;
 global_settings.ambient_brightness=2;draw(10000);capture("bright.ppm");
 global_settings.timeformat=1;test_time.tm_hour=9;draw(10000);
 capture("morning.ppm");test_time.tm_hour=20;global_settings.timeformat=0;
 fixture("2026-09-12T10:00","64","imperial");weather_load();
 assert(weather.valid&&weather.units=='F');draw(10000);capture("stale.ppm");
 fixture("2026-09-12T20:00","bad","metric");weather_load();
 assert(!weather.valid);draw(10000);capture("missing.ppm");
 fixture("2026-09-12T20:00","18","metric");
 assert(!ambient_clock_ready(true));
 for(int i=0;i<120;++i){current_tick+=HZ;assert(!ambient_clock_ready(false));}
 current_tick+=HZ;assert(ambient_clock_ready(false));
 audio=1;assert(!ambient_clock_ready(false));audio=0;
 held=true;assert(!ambient_clock_ready(false));held=false;
 output_active=false;assert(!ambient_clock_ready(false));output_active=true;
 wheel=10;assert(!ambient_clock_ready(false));wheel=-1;
 current_tick+=3*HZ;assert(!ambient_clock_ready(false));
 for(int i=0;i<12;++i){char album[16];snprintf(album,sizeof(album),"Album%d",i);
 track.album=album;ambient_clock_palette(LCD_RGBPACK(140,30,50),LCD_RGBPACK(20,80,190));}
 assert(palette_count==8);track.album="Album5";
 ambient_clock_palette(0,0);assert(palette_count==8);
 for(int i=0;i<100;++i){reset_run();assert(ambient_clock_run(false)==ACTION_NONE);
 assert(theme_depth==0&&lcd_current_viewport==&initial_viewport);
 assert(backdrop==(void*)0x1234);}
 reset_run();raw_wake=true;assert(ambient_clock_run(false)==ACTION_NONE);
 assert(!art_leased&&art.fd==-1);raw_wake=false;raw=0;
 reset_run();action_to_send=SYS_USB_CONNECTED;
 assert(ambient_clock_run(false)==SYS_USB_CONNECTED);
 reset_run();action_to_send=ACTION_TREE_WPS;
 assert(ambient_clock_run(false)==ACTION_TREE_WPS);
 reset_run();action_to_send=ACTION_TREE_STOP;
 assert(ambient_clock_run(false)==ACTION_TREE_STOP);
 reset_run();disconnect=true;assert(ambient_clock_run(false)==ACTION_NONE);
 reset_run();start_music=true;assert(ambient_clock_run(false)==ACTION_TREE_WPS);
 assert(theme_depth==0);reset_run();audio=1;
 before=frames;assert(ambient_clock_run(true)==ACTION_NONE&&frames==before);
 printf("PASS: weather validation, zero draw I/O, idle gates, palette bound, "
        "100 handoffs, USB, transport, disconnect, external playback\n");
 return 0;
}
'''.replace('AMBIENT_SOURCE', str(a.output / 'ambient_clock.c'))
(a.output / 'test.c').write_text(test)
(a.output / '.rockbox/rockpod/weather').mkdir(parents=True, exist_ok=True)
(a.output / '.rockbox/albumlist').mkdir(parents=True, exist_ok=True)
exe = a.output / 'gate'
subprocess.run(['cc','-std=gnu99','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all',
                '-I',str(stubs),str(a.output/'test.c'),'-o',str(exe)],check=True)
subprocess.run([str(exe)],cwd=a.output,check=True,env={'TZ':'UTC','PATH':'/usr/bin:/bin'})
from PIL import Image
for ppm in a.output.glob('*.ppm'):
    Image.open(ppm).resize((960,720)).save(ppm.with_suffix('.png'))
print(a.output)
