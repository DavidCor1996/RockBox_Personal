#!/usr/bin/env python3
"""Run the production TV Home loop through real launch/return decisions."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'apps/root_menu.c').read_text()
a=s.index('static int tv_home_category =');b=s.index('\n#endif\nstatic int root_menu_video_dashboard',a)
source=s[a:b]
constants=sorted(set(re.findall(r'\b(?:ACTION|CONTEXT|BUTTON|GO_TO)_[A-Z_]+',source))|{'GO_TO_WPS'})
defs='\n'.join(f'#define {v} {1<<i}' for i,v in enumerate(constants))
code=r'''
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define ARRAYLEN(a) ((int)(sizeof(a)/sizeof(*(a))))
#define HZ 100
#define VIDEO_BROWSER_TITLE_MAX 64
#define ALLOW_SOFTLOCK 0x10000000
#define SYS_USB_CONNECTED 0x20000000
#define Icon_NOICON 0
#define LAST_ITEM_IN_LIST__NEXTLIST(c) {-1,0,c}
enum tv_section {TV_APPS,TV_VIDEOS,TV_MUSIC,TV_SETTINGS,TV_SECTION_COUNT};
struct menu_item_ex {int id;};
struct button_mapping {int action,button,prereq;};
struct bitmap {int unused;};
struct root_menu_video_application_item {const char *label;int(*function)(void*);};
struct gui_synclist {const char*(*name)(int,void*,char*,size_t);int count,selected;};
static struct menu_item_ex youtube_item={10},twitch_item={11},videos={1},livetv_item={2},file_browser={3},db_browser={4},wps_item={5},playlists={6},pictureflow_item={7},menu_={8},system_menu_={9};
static int native_depth,draws,stops,launches[128],nlaunches,napps=3;
static int script[128],length,cursor,section;
static bool active=true;
static int app0(void*p){(void)p;launches[nlaunches++]=100;return 0;}
static int app1(void*p){(void)p;launches[nlaunches++]=101;return 0;}
static int app2(void*p){(void)p;launches[nlaunches++]=102;return 0;}
static struct root_menu_video_application_item root_menu_video_application_items[]={{"Calendar",app0},{"Photos",app1},{"Games",app2}};
static int root_menu_video_application_count(void){return napps;}
static struct root_menu_video_application_item *root_menu_video_application_item_at(int i){assert(i>=0&&i<napps);return &root_menu_video_application_items[i];}
static void root_menu_video_enter_native_screen(void){assert(native_depth==0);native_depth++;}
static int root_menu_video_finish_native_screen(int ret){assert(native_depth==1);native_depth--;return ret;}
static void root_menu_video_prepare_application_visibility(void){}
static bool tv_ui_active(void){return active;}
static void tv_ui_set_section(int s){section=s;}
static bool home_focus;
static void tv_ui_set_home(bool active,bool t){home_focus=active;(void)t;}
static void root_menu_video_prepare_application_icons(bool tv){assert(tv);}
static struct bitmap *root_menu_video_application_icon(const struct root_menu_video_application_item *i){(void)i;return NULL;}
static struct bitmap *root_menu_video_buffered_art(void){return NULL;}
static void tv_grid_draw(struct gui_synclist*l,const struct bitmap*const*i,int f,int c){(void)i;assert(c==2||c==3);assert(f<=l->selected&&l->selected<f+c*2);}
static void tv_list_art_draw(struct gui_synclist*l,const struct bitmap*b){(void)l;(void)b;}
static bool tv_home_promo_ready,enable_featured;
static int tv_home_promo_selected,tv_home_promo_count;
static struct tv_home_promo {char title[64];bool show;} tv_home_promos[3]={
 {"Death Note",true},{"Mortal Kombat",false},{"RWBY",true}};
static char videos_featured_request[VIDEO_BROWSER_TITLE_MAX+1];
static void strmemccpy(char*d,const char*s,size_t n){snprintf(d,n,"%s",s);}
static int videos_scrn(void *p){assert(!native_depth);const char*s=p;launches[nlaunches++]=s[0]=='S'?200:201;return 0;}
static void tv_home_promo_move(int n){tv_home_promo_selected=(tv_home_promo_selected+n+3)%3;}

static char tv_home_promo_title[64];
static struct bitmap video_netflix_banner_bm;
static void tv_home_promo_reset(void){tv_home_promo_ready=false;tv_home_promo_count=0;tv_home_promo_selected=0;}
static bool tv_home_promo_service(bool idle,bool focused){(void)focused;if(idle&&enable_featured&&!tv_home_promo_count){tv_home_promo_count=3;return true;}return false;}
static void tv_home_videos_draw(struct gui_synclist*l,const struct bitmap*const*i,const struct bitmap*b,const char*t,bool f,int s,int c)
{(void)b;(void)t;(void)f;(void)s;(void)c;tv_grid_draw(l,i,0,2);}
static int audio_status(void){return 1;}
static bool ipodjs_ui_handle_system_event(int a,void*p){(void)a;(void)p;return false;}
static int default_event_handler(int a){return a;}
static void gui_synclist_init(struct gui_synclist*l,const char*(*n)(int,void*,char*,size_t),void*d,bool b,int s,void*p){(void)d;(void)b;(void)s;(void)p;l->name=n;}
static void gui_synclist_set_title(struct gui_synclist*l,const char*t,int icon){(void)l;(void)icon;assert(t&&*t);}
static void gui_synclist_set_nb_items(struct gui_synclist*l,int n){l->count=n;}
static void gui_synclist_select_item(struct gui_synclist*l,int n){l->selected=n;}
static void gui_synclist_draw_native(struct gui_synclist*l){draws++;assert(l->selected>=0);assert(!l->count||l->selected<l->count);for(int i=0;i<l->count;i++)assert(l->name(i,NULL,NULL,0)[0]);}
static void gui_synclist_scroll_stop(struct gui_synclist*l){(void)l;stops++;}
'''+defs+r'''
static bool remote=true;
static int get_action_statuscode(void*p){(void)p;return remote?ACTION_REMOTE:0;}
static const struct button_mapping fallback[]={{0,0,0}};
static const struct button_mapping *get_context_mapping(int c){assert(!(c&CONTEXT_PLUGIN));return fallback;}
static int root_menu_video_launch_menu_item(const struct menu_item_ex*i){assert(native_depth==0);launches[nlaunches++]=i->id;return i==&wps_item?GO_TO_WPS:GO_TO_PREVIOUS;}
static int get_custom_action(int c,int t,const struct button_mapping*(*map)(int))
{
 assert(c==(CONTEXT_MAINMENU|CONTEXT_PLUGIN|ALLOW_SOFTLOCK));assert(t==HZ/5);
 const struct button_mapping*m=map(CONTEXT_MAINMENU|CONTEXT_PLUGIN);
 assert(m[0].action==ACTION_TREE_PGLEFT&&m[1].action==ACTION_TREE_PGRIGHT);
 assert(map(CONTEXT_TREE|CONTEXT_PLUGIN)==fallback);
 if(cursor==length){active=false;return 0;}return script[cursor++];
}
'''+source+r'''
static void run(const int *keys,int n,int result)
{
 assert(n<=ARRAYLEN(script));memcpy(script,keys,n*sizeof(*keys));length=n;cursor=0;
 active=true;nlaunches=0;draws=stops=0;tv_home_category=TV_MUSIC;
 memset(tv_home_selected,0,sizeof(tv_home_selected));
 assert(root_menu_tv_dashboard()==result);assert(!native_depth);assert(!home_focus);assert(draws&&stops);
}
int main(void)
{
 int keys[]={ACTION_TREE_PGLEFT,ACTION_TREE_PGLEFT,ACTION_STD_NEXT,
 ACTION_TREE_PGRIGHT,ACTION_STD_OK,ACTION_TREE_PGRIGHT,ACTION_STD_OK,
 ACTION_STD_MENU,ACTION_TREE_PGRIGHT,ACTION_STD_NEXT,ACTION_STD_OK,
 ACTION_TREE_PGRIGHT,ACTION_STD_OK,ACTION_STD_CANCEL,ACTION_TREE_PGRIGHT,
 ACTION_STD_NEXT,ACTION_STD_NEXT,ACTION_STD_OK};
 for(int i=0;i<100;i++)
 {
  run(keys,ARRAYLEN(keys),GO_TO_WPS);
  assert(nlaunches==5&&launches[0]==101&&launches[1]==102&&launches[2]==10&&launches[3]==1&&launches[4]==5);
  assert(section==TV_MUSIC&&tv_home_selected[TV_MUSIC]==1);
 }
 /* Videos has one horizontal row below Featured. Right and Left follow
  * apps; Down at the row edge stays put, and Up returns to categories. */
 int grid[]={ACTION_TREE_PGLEFT,ACTION_STD_NEXT,ACTION_TREE_PGRIGHT,ACTION_STD_OK,
 ACTION_TREE_PGRIGHT,ACTION_STD_OK,ACTION_TREE_PGRIGHT,ACTION_STD_OK,
 ACTION_TREE_PGLEFT,ACTION_STD_OK,ACTION_STD_PREV,ACTION_TREE_PGRIGHT};
 run(grid,ARRAYLEN(grid),GO_TO_ROOT);
 assert(nlaunches==4&&launches[0]==1&&launches[1]==2&&launches[2]==11&&launches[3]==2);
 assert(section==TV_MUSIC);
 int edges[]={ACTION_TREE_PGLEFT,ACTION_STD_NEXT,ACTION_TREE_PGLEFT,
 ACTION_STD_NEXT,ACTION_STD_NEXT,ACTION_STD_OK};
 run(edges,ARRAYLEN(edges),GO_TO_ROOT);assert(nlaunches==1&&launches[0]==10);
 /* The wheel retains sequential detents even on a two-dimensional grid. */
 remote=false;
 int wheel[]={ACTION_TREE_PGLEFT,ACTION_STD_NEXT,ACTION_STD_NEXT,ACTION_STD_OK};
 run(wheel,ARRAYLEN(wheel),GO_TO_ROOT);assert(nlaunches==1&&launches[0]==1);remote=true;
 int back[]={ACTION_STD_NEXT,ACTION_STD_NEXT,ACTION_STD_CANCEL,ACTION_TREE_PGRIGHT,
 ACTION_STD_OK,ACTION_STD_OK};
 run(back,ARRAYLEN(back),GO_TO_ROOT);assert(nlaunches==1&&launches[0]==8);
 int usb[]={SYS_USB_CONNECTED};run(usb,1,GO_TO_ROOT);assert(!nlaunches);
 napps=0;int empty[]={ACTION_TREE_PGLEFT,ACTION_TREE_PGLEFT,ACTION_STD_NEXT,ACTION_STD_OK};
 run(empty,ARRAYLEN(empty),GO_TO_ROOT);assert(!nlaunches);
 enable_featured=true;
 int featured[]={ACTION_TREE_PGLEFT,ACTION_NONE,ACTION_STD_NEXT,
 ACTION_TREE_PGRIGHT,ACTION_STD_OK};
 run(featured,ARRAYLEN(featured),GO_TO_VIDEOS);
 assert(!nlaunches && !native_depth && !strcmp(videos_featured_request,"MMortal Kombat"));
 int show[]={ACTION_TREE_PGLEFT,ACTION_NONE,ACTION_STD_NEXT,ACTION_STD_OK};
 run(show,ARRAYLEN(show),GO_TO_VIDEOS);
 assert(!nlaunches && !native_depth && !strcmp(videos_featured_request,"SDeath Note"));
 /* Returning via the root dispatcher restores Home rather than keeping its
  * large list frame underneath Netflix's image decoder. */
 int returned[]={ACTION_TREE_PGLEFT,ACTION_NONE,ACTION_STD_NEXT,
 ACTION_STD_NEXT,ACTION_TREE_PGRIGHT,ACTION_STD_OK};
 run(returned,ARRAYLEN(returned),GO_TO_ROOT);
 assert(nlaunches==1&&launches[0]==1);
 puts("PASS: production TV Home loop, 100 app-to-app/library/WPS routes, spatial grids, tab focus, edges, wheel steps, Back/Home, USB, empty Apps and balanced ownership; launch targets stubbed");
}
'''
with tempfile.TemporaryDirectory(prefix='tv-home-') as tmp:
 p=Path(tmp);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-g','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
