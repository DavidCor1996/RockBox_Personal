#!/usr/bin/env python3
"""Exercise the production TV cover service/getter with bounded I/O stubs."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'apps/gui/albumlist_art.c').read_text()
a=s.index('static struct gui_synclist *tv_album_list;');b=s.index('\n#else\n',a)
source=s[a:b]
a=s.index('void albumlist_setup_list(')
invalidate=re.search(r'#ifdef HAVE_COMPOSITE_VIDEO_OUT\n(.*?)#endif',s[a:],re.S).group(1)
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#define HZ 100
#define HAVE_DOCKED_AMBIENT_CLOCK 1
#define ALBUMLIST_ALBUM_LEN 96
#define ALBUMLIST_ARTIST_LEN 80
#define MAX(a,b) ((a)>(b)?(a):(b))
#define TIME_BEFORE(a,b) ((a)<(b))
#define ascii_casecmp strcasecmp
struct bitmap {int id;};
struct gui_synclist {void*data;int selected_item;void*callback_draw_item;};
struct albumlist_slideshow_slot {struct bitmap bm;int index;bool valid;};
static struct albumlist_slideshow_slot slot;
static struct {const char*album,*artist;} manifest_cache[]={
 {"Twin","A"},{"Twin","B"},{"Unique","C"}};
static int manifest_cache_count=3,loads;
static bool active=true,empty=true,held,usable=true,committing,ambient_workspace_leased,fail;
static long current_tick;
static bool tv_ui_active(void){return active;}
static bool button_queue_empty(void){return empty;}
static bool button_hold(void){return held;}
static bool tagcache_is_usable(void){return usable;}
static bool tagcache_commit_active(void){return committing;}
static struct albumlist_slideshow_slot *albumlist_find_slideshow_slot(int index)
{return slot.valid&&slot.index==index?&slot:NULL;}
static struct albumlist_slideshow_slot *albumlist_get_slideshow_slot(int index,int protected)
{assert(empty&&!held&&usable&&!committing&&!ambient_workspace_leased);assert(protected==-1);loads++;
 if(fail)return NULL;slot.valid=true;slot.index=index;return &slot;}
static bool albumlist_get_album_row(void*d,int row,char*a,size_t an,char*b,size_t bn)
{
 assert(d);if(row==0)return false;
 snprintf(a,an,"%s",row==4?"Unique":"Twin");
 snprintf(b,bn,"%s",row==1?"A":row==2?"B":"");return true;
}
'''+source+'\nstatic void reset_level(void){\n'+invalidate+'}\n'+r'''
int main(void)
{
 int tree=0,other=1;struct gui_synclist list={&tree,1,(void*)1};
 assert(!albumlist_tv_service(&list,true));current_tick=24;
 assert(!albumlist_tv_service(&list,true));assert(loads==0);
 current_tick=25;empty=false;assert(!albumlist_tv_service(&list,true));empty=true;
 held=true;assert(!albumlist_tv_service(&list,true));held=false;
 usable=false;assert(!albumlist_tv_service(&list,true));usable=true;
 committing=true;assert(!albumlist_tv_service(&list,true));committing=false;
 ambient_workspace_leased=true;assert(!albumlist_tv_service(&list,true));ambient_workspace_leased=false;
 assert(!albumlist_tv_service(&list,false));assert(loads==0);
 assert(albumlist_tv_service(&list,true));assert(loads==1&&slot.index==0);
 for(int i=0;i<1000;i++){assert(albumlist_tv_cover_cached(&list)==&slot.bm);assert(!albumlist_tv_service(&list,true));}
 assert(loads==1);
 list.selected_item=2;assert(!albumlist_tv_cover_cached(&list));
 assert(!albumlist_tv_service(&list,false));current_tick+=25;
 assert(albumlist_tv_service(&list,true));assert(loads==2&&slot.index==1);
 /* Reused tree/list objects still invalidate when entering a hierarchy. */
 reset_level();assert(!albumlist_tv_cover_cached(&list));
 assert(!albumlist_tv_service(&list,true));current_tick+=25;
 assert(albumlist_tv_service(&list,true));assert(loads==3);
 list.data=&other;assert(!albumlist_tv_cover_cached(&list));
 list.selected_item=3;assert(!albumlist_tv_service(&list,false));current_tick+=25;
 assert(!albumlist_tv_service(&list,true));assert(loads==3); /* ambiguous album */
 list.selected_item=4;albumlist_tv_service(&list,false);current_tick+=25;
 assert(albumlist_tv_service(&list,true));assert(loads==4&&slot.index==2);
 ambient_workspace_leased=true;assert(!albumlist_tv_cover_cached(&list));ambient_workspace_leased=false;
 slot.valid=false;assert(!albumlist_tv_cover_cached(&list));
 list.selected_item=1;albumlist_tv_service(&list,false);current_tick+=25;fail=true;
 assert(!albumlist_tv_service(&list,true));assert(loads==5);
 for(int i=0;i<1000;i++)assert(!albumlist_tv_service(&list,true));assert(loads==5);
 active=false;assert(!albumlist_tv_service(&list,true));assert(!albumlist_tv_cover_cached(&list));
 puts("PASS: production album cover service: 250 ms idle, queued input, Hold, tagcache, ambient lease, hierarchy/selection invalidation, duplicate album matching, one load and no draw-time I/O; loader stubbed");
}
'''
with tempfile.TemporaryDirectory(prefix='tv-album-') as temp:
 p=Path(temp);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-g','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
