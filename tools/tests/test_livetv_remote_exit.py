#!/usr/bin/env python3
"""Run raw dock gestures through the production guide's complete event loop."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'apps/plugins/mpegplayer/livetv_guide.c').read_text()
def function(prefix):
 a=s.index(prefix);b=s.index('{',a);end=b+1;depth=1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[a:end]
mpeg=(root/'apps/plugins/mpegplayer/mpegplayer.c').read_text()
saved=s;s=mpeg;playback=function('static int livetv_playback_button(');s=saved
a=s.index('#define LIVETV_BTN_UP       BUTTON_SCROLL_BACK');b=s.index('\n#else',a)
keys=s[a:b]
code=r'''
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>
#define HAVE_IPODJS_UI
#define BUTTON_NONE 0
#define BUTTON_REPEAT (1<<24)
#define BUTTON_REL (1<<25)
#define BUTTON_RC_UP (1<<1)
#define BUTTON_RC_DOWN (1<<2)
#define BUTTON_RC_LEFT (1<<3)
#define BUTTON_RC_RIGHT (1<<4)
#define BUTTON_RC_PLAY (1<<5)
#define BUTTON_RC_SELECT (1<<6)
#define BUTTON_RC_MENU (1<<7)
#define BUTTON_RC_STOP (1<<8)
#define BUTTON_RC_VOL_UP (1<<9)
#define BUTTON_RC_VOL_DOWN (1<<10)
#define BUTTON_SCROLL_BACK (1<<11)
#define BUTTON_SCROLL_FWD (1<<12)
#define BUTTON_LEFT (1<<13)
#define BUTTON_RIGHT (1<<14)
#define BUTTON_SELECT (1<<15)
#define BUTTON_MENU (1<<16)
#define BUTTON_PLAY (1<<17)
#define HZ 100
#define MAX_PATH 260
#define TIME_BEFORE(a,b) ((a)<(b))
#define LIVETV_GUIDE_EXIT 1
#define LIVETV_GUIDE_WATCH 2
#define LIVETV_GUIDE_TUNE 3
static long now=100;
static bool livetv_ready=true;
static int livetv_channel_cur=0;
static int script[32],length,cursor,moves,unlocks,channels;
static void no_op(void){}
static void splash(int t,const char*s){(void)t;(void)s;assert(false);}
static struct {long*current_tick;void(*button_clear_queue)(void);void(*splash)(int,const char*);} api={&now,no_op,splash};
#define rb (&api)
static int mpeg_button_get(int timeout){assert(timeout==HZ/4);assert(cursor<length);return script[cursor++];}
static int mpeg_sysevent(void){return 0;}
static void livetv_guide_enter(void){}
static void livetv_guide_draw(void){}
static void livetv_guide_draw_rows(void){}
static void livetv_guide_move(int channel,int time){if(channel){assert(!time);channels+=channel;}else{assert(time==-1);moves++;}}
static void livetv_guide_jump_hours(int h){(void)h;assert(false);}
static int livetv_options_run(void){assert(false);return 0;}
static void livetv_parental_finish_unlock(void){unlocks++;}
static int livetv_guide_selected_channel(void){return 0;}
static bool livetv_guide_selection_is_live(void){return true;}
static void livetv_reminder_run(void){assert(false);}
static bool livetv_tune(int c,char*p,size_t n,void*o){(void)c;(void)p;(void)n;(void)o;return true;}
static void livetv_save_state(void){}
''' + r'''
#define MPEG_PAUSE (BUTTON_PLAY|BUTTON_REL)
#define MPEG_ZOOM (BUTTON_SELECT|BUTTON_REL)
#define MPEG_RC_PAUSE (BUTTON_RC_PLAY|BUTTON_REL)
#define MPEG_RC_ZOOM (BUTTON_RC_SELECT|BUTTON_REL)
#define MPEG_MENU BUTTON_MENU
static bool mpegplayer_livetv_launch=true,mpegplayer_livetv_desktop=false;
''' + playback+'\n'+keys+'\n'+function('static int livetv_remote_button(')+'\n'+function('int livetv_guide_run(void)')+r'''
static void run(const int*keys,int n,int result)
{memcpy(script,keys,n*sizeof(int));length=n;cursor=moves=unlocks=channels=0;assert(livetv_guide_run()==result);assert(cursor==length);}
int main(void)
{
 int controls[]={MPEG_PAUSE,MPEG_ZOOM,MPEG_RC_PAUSE,MPEG_RC_ZOOM};
 for(unsigned i=0;i<sizeof(controls)/sizeof(controls[0]);i++)
 {
  assert(livetv_playback_button(controls[i])==MPEG_MENU);
  mpegplayer_livetv_launch=false;
  assert(livetv_playback_button(controls[i])==controls[i]);
  mpegplayer_livetv_launch=true;mpegplayer_livetv_desktop=true;
  assert(livetv_playback_button(controls[i])==controls[i]);
  mpegplayer_livetv_desktop=false;
 }
 assert(livetv_playback_button(BUTTON_RC_PLAY)==BUTTON_RC_PLAY);
 assert(livetv_playback_button(BUTTON_RC_SELECT|BUTTON_REPEAT)==(BUTTON_RC_SELECT|BUTTON_REPEAT));
 for(int i=0;i<100;i++)
 {
  int held[]={BUTTON_RC_LEFT,BUTTON_RC_LEFT|BUTTON_REPEAT};
  run(held,2,LIVETV_GUIDE_EXIT);assert(!moves&&!unlocks);
  assert(livetv_remote_button(BUTTON_RC_LEFT|BUTTON_REPEAT)==BUTTON_NONE);
  assert(livetv_remote_button(BUTTON_RC_LEFT|BUTTON_REL)==BUTTON_NONE);
  int short_then_back[]={BUTTON_RC_LEFT,BUTTON_RC_LEFT|BUTTON_REL,
    BUTTON_RC_LEFT,BUTTON_RC_LEFT|BUTTON_REPEAT};
  run(short_then_back,4,LIVETV_GUIDE_EXIT);assert(moves==1);
 }
 int menu[]={BUTTON_MENU,BUTTON_MENU|BUTTON_REL};run(menu,2,LIVETV_GUIDE_EXIT);
 int remote_menu[]={BUTTON_RC_MENU,BUTTON_RC_MENU|BUTTON_REL};run(remote_menu,2,LIVETV_GUIDE_EXIT);
 /* Both halves stay inside the guide. No trailing release reaches the
  * fullscreen mapper, which would correctly interpret a NEW click as Guide. */
 for(int i=0;i<100;i++)
 {
  int select[]={BUTTON_RC_SELECT,BUTTON_RC_SELECT|BUTTON_REL};
  run(select,2,LIVETV_GUIDE_WATCH);
  assert(livetv_playback_button(BUTTON_NONE)==BUTTON_NONE);
  int play[]={BUTTON_RC_PLAY,BUTTON_RC_PLAY|BUTTON_REL};
  run(play,2,LIVETV_GUIDE_WATCH);
  assert(livetv_playback_button(BUTTON_NONE)==BUTTON_NONE);
 }
 /* Directional and volume-coded dock arrows scroll channels. Neither
  * release edges nor selecting fullscreen add an extra step. */
 int arrows[]={BUTTON_RC_UP,BUTTON_RC_VOL_UP,BUTTON_RC_DOWN,BUTTON_RC_VOL_DOWN};
 for(unsigned i=0;i<sizeof(arrows)/sizeof(arrows[0]);i++)
 {
  int scroll[]={arrows[i],arrows[i]|BUTTON_REL,arrows[i],
    arrows[i]|BUTTON_REPEAT,arrows[i]|BUTTON_REL,
    BUTTON_RC_PLAY,BUTTON_RC_PLAY|BUTTON_REL};
  run(scroll,7,LIVETV_GUIDE_WATCH);
  assert(channels==(i<2?-3:3));assert(!moves&&!unlocks);
 }
 int unlock[]={BUTTON_SELECT|BUTTON_REPEAT,BUTTON_SELECT|BUTTON_REPEAT|BUTTON_REL,BUTTON_MENU,BUTTON_MENU|BUTTON_REL};
 run(unlock,4,LIVETV_GUIDE_EXIT);assert(unlocks==1);
 puts("PASS: production guide consumes 100 held-Left exit gestures without waiting for release; short Left, repeated/released suppression, local/remote Menu, Select and parental unlock preserved");
}
'''
mpeg=(root/'apps/plugins/mpegplayer/mpegplayer.c').read_text()
assert 'case LIVETV_GUIDE_EXIT:\n        return VIDEO_STOP;' in mpeg
assert 'button = livetv_playback_button(button);' in mpeg
with tempfile.TemporaryDirectory(prefix='livetv-exit-') as t:
 p=Path(t);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
