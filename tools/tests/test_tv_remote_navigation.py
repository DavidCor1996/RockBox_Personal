#!/usr/bin/env python3
"""Execute the production TV policy and remote keymaps, including defaults."""
import pathlib,re,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[2]
key=(root/'apps/keymaps/keymap-ipod.c').read_text();a=key.index('static const struct button_mapping remote_full_standard');b=key.index('#endif /* BUTTON_REMOTE != 0 */',a);maps=key[a:b]
core=(root/'apps/iap/iap-core.c').read_text();a=core.index('bool iap_remote_tv_active(');b=core.index('bool iap_remote_input_suppressed(',a);policy=core[a:b]
driver=(root/'firmware/drivers/button.c').read_text()
start=driver.index('#define WHEEL_ACCEL_FACTOR');end=driver.index('#endif /* HAVE_WHEEL_ACCELERATION */',start)
acceleration=driver[start:end]
config=(root/'firmware/export/config/ipod6g.h').read_text()
acceleration='\n'.join(re.findall(r'^#define WHEEL_ACCEL(?:ERATION|_START)\s+.*$',config,re.M))+'\n'+acceleration
buttons=(root/'firmware/target/arm/s5l8702/ipod6g/button-target.h').read_text();buttons='\n'.join(re.findall(r'^#define BUTTON_(?:RC_.*|REMOTE)\s+.*$',buttons,re.M))
identifiers=sorted(set(re.findall(r'\b(?:ACTION|CONTEXT)_[A-Z_]+',maps)))
# Distinct context modifier, retaining the production mapping relationships.
defs='\n'.join('#define %s %d'%(v,(1<<20 if v=='CONTEXT_REMOTE' else 1<<19 if v=='CONTEXT_CUSTOM' else i+1)) for i,v in enumerate(identifiers))
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#define MAX(a,b) ((a)>(b)?(a):(b))
#define IPOD_ACCESSORY_PROTOCOL 1
#define HAVE_COMPOSITE_VIDEO_OUT 1
#define CONFIG_TUNER 0
#define BUTTON_NONE 0
#define BUTTON_REL 0x02000000
#define BUTTON_REPEAT 0x04000000
#define BUTTON_PLAY 8
struct button_mapping {int action_code;unsigned button_code,prereq_button_code;};
#define LAST_ITEM_IN_LIST {-1,0,0}
#define LAST_ITEM_IN_LIST__NEXTLIST(c) {-(c)-100,0,0}
static struct { int dock_remote_mode,tv_interface,tv_now_playing; } global_settings;
static bool active,kokkia;
static bool videoout_active(void){return active;}
static bool iap_kokkia_present(void){return kokkia;}
'''+buttons+'\n'+defs+'\n'+policy+'\n'+maps+'\n'+acceleration+r'''
static int lookup(int context,unsigned button,unsigned previous)
{
 for(int pass=0;pass<5;pass++)
 {
  const struct button_mapping *m=get_context_mapping_remote(context|CONTEXT_REMOTE);
  for(;m->action_code>=0;m++)
   if(m->button_code==button && (!m->prereq_button_code || m->prereq_button_code==previous))
    return m->action_code;
  if(m->action_code==-1)return ACTION_NONE;
  context=-m->action_code-100;
 }
 assert(false);return -1;
}
int main(void)
{
 /* TV mode MUST work with the old saved "playback" remote preference. */
 active=true;global_settings.tv_interface=1;
 assert(iap_remote_navigation_active() && iap_remote_tv_active());
 assert(!global_settings.dock_remote_mode);
 const unsigned ups[]={BUTTON_RC_UP,BUTTON_RC_VOL_UP};
 const unsigned downs[]={BUTTON_RC_DOWN,BUTTON_RC_VOL_DOWN};
 for(int i=0;i<2;i++)
 {
  assert(lookup(CONTEXT_TREE,ups[i],0)==ACTION_STD_PREV);
  /* Actual dock events carry data=0, unlike wheel delta in bits 24..30.
   * Feed that through the same function used by gui_synclist_do_button. */
  int cursor=5;
  for(int repeat=0;repeat<4;repeat++)
  {
   int action=lookup(CONTEXT_TREE,downs[i]|(repeat?BUTTON_REPEAT:0),repeat?downs[i]:0);
   assert(action==(repeat?ACTION_STD_NEXTREPEAT:ACTION_STD_NEXT));
   cursor+=button_apply_acceleration(0);
  }
  assert(cursor==9);
  cursor-=button_apply_acceleration(0);assert(cursor==8);
  assert(button_apply_acceleration(3u<<24)==3);
  assert(button_apply_acceleration(0x80000000u | (2u<<24) | (WHEEL_ACCEL_START*2))>2);
  assert(lookup(CONTEXT_TREE,downs[i],0)==ACTION_STD_NEXT);
  assert(lookup(CONTEXT_TREE,ups[i]|BUTTON_REPEAT,ups[i])==ACTION_STD_PREVREPEAT);
  assert(lookup(CONTEXT_WPS,ups[i],0)==ACTION_WPS_MENU);
  assert(lookup(CONTEXT_WPS,downs[i],0)==ACTION_WPS_MENU);
 }
 assert(lookup(CONTEXT_MAINMENU,BUTTON_RC_LEFT|BUTTON_REL,BUTTON_RC_LEFT)==ACTION_TREE_PGLEFT);
 assert(lookup(CONTEXT_MAINMENU,BUTTON_RC_RIGHT,0)==ACTION_TREE_PGRIGHT);
 assert(lookup(CONTEXT_MAINMENU,BUTTON_RC_VOL_DOWN,0)==ACTION_STD_NEXT);
 assert(lookup(CONTEXT_TREE,BUTTON_RC_PLAY|BUTTON_REL,BUTTON_RC_PLAY)==ACTION_STD_OK);
 assert(lookup(CONTEXT_TREE,BUTTON_RC_SELECT|BUTTON_REL,BUTTON_RC_SELECT)==ACTION_STD_OK);
 assert(lookup(CONTEXT_TREE,BUTTON_RC_MENU|BUTTON_REL,BUTTON_RC_MENU)==ACTION_STD_CANCEL);
 assert(lookup(CONTEXT_TREE,BUTTON_RC_MENU|BUTTON_REPEAT,BUTTON_RC_MENU)==ACTION_STD_MENU);
 assert(lookup(CONTEXT_TREE,BUTTON_RC_MENU|BUTTON_REL,BUTTON_RC_MENU|BUTTON_REPEAT)==ACTION_NONE);
 assert(lookup(CONTEXT_WPS,BUTTON_RC_MENU|BUTTON_REL,BUTTON_RC_MENU)==ACTION_WPS_BROWSE);
 assert(lookup(CONTEXT_WPS,BUTTON_RC_MENU|BUTTON_REPEAT,BUTTON_RC_MENU)==ACTION_WPS_MENU);
 assert(lookup(CONTEXT_WPS,BUTTON_RC_SELECT|BUTTON_REL,BUTTON_RC_SELECT)==ACTION_WPS_PLAY);
 assert(lookup(CONTEXT_MAINMENU,BUTTON_RC_LEFT|BUTTON_REPEAT,BUTTON_RC_LEFT)==ACTION_STD_CANCEL);
 assert(lookup(CONTEXT_MAINMENU,BUTTON_RC_LEFT|BUTTON_REPEAT,BUTTON_RC_LEFT|BUTTON_REPEAT)==ACTION_NONE);
 assert(lookup(CONTEXT_MAINMENU,BUTTON_RC_LEFT|BUTTON_REL,BUTTON_RC_LEFT|BUTTON_REPEAT)==ACTION_NONE);
 assert(lookup(CONTEXT_WPS,BUTTON_RC_PLAY|BUTTON_REL,BUTTON_RC_PLAY)==ACTION_WPS_PLAY);
 assert(lookup(CONTEXT_WPS,BUTTON_RC_LEFT|BUTTON_REPEAT,BUTTON_RC_LEFT)==ACTION_WPS_BROWSE);
 active=false;assert(!iap_remote_navigation_active());
 assert(lookup(CONTEXT_WPS,BUTTON_RC_VOL_UP,0)==ACTION_WPS_VOLUP);
 active=true;kokkia=true;assert(!iap_remote_navigation_active());
 kokkia=false;global_settings.tv_interface=0;global_settings.tv_now_playing=1;
 assert(iap_remote_tv_active());
 global_settings.tv_now_playing=0;global_settings.dock_remote_mode=1;
 assert(iap_remote_navigation_active() && !iap_remote_tv_active());
 puts("PASS: actual zero-data remote scrolling, repeat, wheel acceleration, TV policy and keymaps: default playback preference, category/list/WPS, short/held Menu, Select/Play, inactive TV and Kokkia isolation");
}
'''
with tempfile.TemporaryDirectory(prefix='tv-keys-') as tmp:
 p=pathlib.Path(tmp);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
