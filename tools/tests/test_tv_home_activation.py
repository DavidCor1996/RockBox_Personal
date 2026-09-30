#!/usr/bin/env python3
"""Exercise production Home entry and late dock activation dispatch branches."""
from pathlib import Path
import subprocess, tempfile
root = Path(__file__).resolve().parents[2]
s = (root / 'apps/root_menu.c').read_text()
a = s.index('static int root_menu_video_dashboard(int *selectedp)\n{')
s = s[a:s.index('\n#endif /* HAVE_IPODJS_UI */', a)]
entry = s[s.index('#ifdef HAVE_TAGCACHE'):s.index('#endif') + 6]
a = s.index('#ifdef HAVE_TAGCACHE', s.index('while (true)'))
late = s[a:s.index('#endif', a) + 6]
assert s.index(late) < s.index('notification_manager_service();') < s.index('root_menu_video_draw_home(')
code = r'''
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#define HAVE_TAGCACHE
#define GO_TO_ROOT 1
#define TV_READY 2
#define HANDHELD_READY 3
static bool active, tabs;
static int depth, selection, stops, tv_entries;
static bool tv_ui_active(void) { return active; }
static void lcd_scroll_stop(void) { stops++; }
static int root_menu_video_finish_native_screen(int result)
{ assert(depth==1); depth--; return result; }
static int root_menu_tv_dashboard(void)
{ assert(depth==0); depth++; tv_entries++; tabs=true; return TV_READY; }
static int entry(void)
{
''' + entry + r'''
 assert(depth==0); depth++; return HANDHELD_READY;
}
static int handheld_tick(int *selectedp, int selected)
{
''' + late + r'''
 return HANDHELD_READY;
}
int main(void)
{
 for (int visit=0; visit<100; visit++)
 {
  active=false; tabs=false; selection=visit%8;
  assert(entry()==HANDHELD_READY);
  for(int tick=0; tick<20; tick++)
   assert(handheld_tick(&selection,selection)==HANDHELD_READY);
  assert(depth==1 && !tabs);
  active=true;
  assert(handheld_tick(&selection,selection)==GO_TO_ROOT);
  assert(depth==0 && selection==visit%8 && stops==visit+1);
  /* The root dispatcher immediately re-enters Home without opening an app. */
  assert(entry()==TV_READY && tabs && depth==1);
  assert(root_menu_video_finish_native_screen(GO_TO_ROOT)==GO_TO_ROOT);
  /* Already docked at initial entry takes the same dedicated TV route. */
  tabs=false;
  assert(entry()==TV_READY && tabs && depth==1);
  root_menu_video_finish_native_screen(GO_TO_ROOT);
 }
 assert(tv_entries==200 && depth==0);
 puts("PASS: 100 late dock activations and 100 docked Home entries; selection preserved, tabs active, native depth balanced");
}
'''
with tempfile.TemporaryDirectory(prefix='tv-home-activation-') as tmp:
    p=Path(tmp); (p/'test.c').write_text(code)
    subprocess.run(['cc','-std=c99','-Wall','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
