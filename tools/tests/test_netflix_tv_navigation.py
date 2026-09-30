#!/usr/bin/env python3
"""Production dock gesture, keymap and Netflix selection, including resume rows."""
from pathlib import Path
import runpy,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
base=runpy.run_path(str(root/'tools/tests/test_tv_remote_navigation.py'))
s=(root/'apps/root_menu.c').read_text()
a=s.index('/* A TV poster shelf');b=s.index('static int videos_netflix_browser(',a)
code=base['code'].replace('int main(void)','int base_main(void)')+r'''
#define HZ 100
#define CONTEXT_PLUGIN (1<<23)
#define TIME_AFTER(a,b) ((long)((b)-(a))<0)
#define TIME_BEFORE(a,b) TIME_AFTER(b,a)
#include "iap-volume-gesture.h"
struct video_entry {bool is_resume;};
struct video_browser_state {int count,selection;struct video_entry entries[8];};
static bool tv_ui_active(void){return active;}
static const struct button_mapping *get_context_mapping(int c)
{return get_context_mapping_remote(c);}
'''+s[a:b]+r'''
static int netflix_key(unsigned button,unsigned prev)
{
 const struct button_mapping*m=videos_netflix_keymap(CONTEXT_TREE|CONTEXT_PLUGIN|CONTEXT_REMOTE);
 for(;m->action_code>=0;m++)
  if(m->button_code==button&&(!m->prereq_button_code||m->prereq_button_code==prev))return m->action_code;
 return lookup(CONTEXT_STD,button,prev);
}
int main(void)
{
 base_main();active=true;kokkia=false;global_settings.tv_interface=1;
 assert(netflix_key(BUTTON_RC_RIGHT,0)==ACTION_STD_NEXT);
 assert(netflix_key(BUTTON_RC_LEFT|BUTTON_REL,BUTTON_RC_LEFT)==ACTION_STD_PREV);
 assert(netflix_key(BUTTON_RC_LEFT|BUTTON_REPEAT,BUTTON_RC_LEFT)==ACTION_STD_CANCEL);
 assert(netflix_key(BUTTON_RC_LEFT|BUTTON_REL,BUTTON_RC_LEFT|BUTTON_REPEAT)==ACTION_NONE);
 assert(netflix_key(BUTTON_RC_PLAY|BUTTON_REL,BUTTON_RC_PLAY)==ACTION_STD_OK);
 struct video_browser_state state={.count=8,.entries={{0},{1},{0},{1}}};
 struct iap_volume_gesture gesture={0};int resume=0,actions=0;
 /* Two captured three-packet taps: each reaches precisely the next title,
  * even when a Continue Watching entry is between them. */
 const long ticks[]={6159,6168,6183,6219,6229,6242};
 for(unsigned i=0;i<sizeof(ticks)/sizeof(*ticks);i++)
  if(iap_volume_gesture_step(&gesture,BUTTON_RC_VOL_DOWN,ticks[i])) {
   assert(netflix_key(BUTTON_RC_VOL_DOWN,0)==ACTION_STD_NEXT);
   assert(videos_netflix_move(&state,1,&resume));actions++;
  }
 assert(actions==2&&state.selection==2&&resume==0);
 assert(videos_netflix_move(&state,-1,&resume)&&state.selection==1&&resume==0);
 assert(videos_netflix_move(&state,-1,&resume)&&state.selection==0);
 assert(videos_netflix_move(&state,-1,&resume)&&state.selection==7);
 state.count=0;assert(!videos_netflix_move(&state,1,&resume));
 state.count=1;state.selection=0;assert(!videos_netflix_move(&state,1,&resume));
 active=false;state.count=8;state.selection=1;resume=0;
 assert(!videos_netflix_move(&state,1,&resume)&&resume==1);
 assert(videos_netflix_move(&state,1,&resume)&&state.selection==2&&resume==0);
 puts("PASS: dock bursts -> one TV shelf step; resume rows, horizontal arrows, held Left, Select and handheld actions");
}
'''
with tempfile.TemporaryDirectory(prefix='netflix-navigation-') as tmp:
 p=Path(tmp);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-I'+str(root/'apps/iap'),'-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
