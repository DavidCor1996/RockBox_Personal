#!/usr/bin/env python3
"""Exercise actual driver ownership setters and mirror guard without MMIO."""
import pathlib, re, subprocess, tempfile
root = pathlib.Path(__file__).resolve().parents[2]
s = (root/'firmware/target/arm/s5l8702/ipod6g/videoout-6g.c').read_text()
def function(name):
    a=s.index('void '+name+'('); b=s.index('{',a); depth=1; end=b+1
    while depth:
        depth+=(s[end]=='{')-(s[end]=='}'); end+=1
    return s[a:end]
mirror=function('ipod6g_videoout_mirror_rgb565')
guard=re.search(r'    if \(svid_ui_owner.*?return;', mirror, re.S).group()
c=r'''
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define LCD_WIDTH 320
#define LCD_HEIGHT 240
static bool svid_ui_owner,svid_ui_batch,svid_frame_owned,svid_mirror_enabled=true;
static bool active=true;
static int handoffs;
static bool ipod6g_videoout_active(void){return active;}
'''+function('ipod6g_videoout_ui_owner')+'\n'+function('ipod6g_videoout_ui_batch')+r'''
static void mirror(const void *source,int x,int y,int width,int height,int stride)
{
'''+guard+r'''
 handoffs++;
}
static void lcd_refresh(void){static uint16_t frame[320*240];mirror(frame,0,0,320,240,320);}
int main(void)
{
 lcd_refresh();assert(handoffs==1);
 for(int visit=0;visit<100;visit++)
 {
   ipod6g_videoout_ui_owner(true);
   /* Skin/scroll/status updates continue while the semantic TV frame owns TV. */
   for(int tick=0;tick<100;tick++)
   {
     ipod6g_videoout_ui_batch(true);lcd_refresh();
     ipod6g_videoout_ui_batch(false);lcd_refresh();
   }
   assert(handoffs==visit+1);
   /* Modal/plugin/browser exit resumes ordinary LCD mirroring. */
   ipod6g_videoout_ui_owner(false);lcd_refresh();
   assert(handoffs==visit+2);
 }
 svid_frame_owned=true;lcd_refresh();assert(handoffs==101);
 svid_frame_owned=false;active=false;lcd_refresh();assert(handoffs==101);
 active=true;lcd_refresh();assert(handoffs==102);
 puts("PASS: actual mirror guard blocks 20,000 intervening LCD refreshes; 100 owner exits restore mirroring; video and inactive guards preserved");
}
'''
wps=(root/'apps/gui/wps.c').read_text()
enter=wps[wps.index('static void gwps_enter_wps('):wps.index('static long do_wps_exit(')]
assert enter.index('tv_wps_enter();')<enter.index('wps_state_init();')
leave=wps[wps.index('static void gwps_leave_wps('):wps.index('static void restore_theme(')]
assert leave.index('tv_ui_leave();')<leave.index('root_menu_ipodjs_leave_wps_frame();')
assert 'svid_frame_owned = svid_frame_pending = svid_ui_batch = svid_ui_owner = false;' in s
with tempfile.TemporaryDirectory(prefix='tv-owner-') as tmp:
 p=pathlib.Path(tmp);(p/'test.c').write_text(c)
 subprocess.run(['cc','-std=c99','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
