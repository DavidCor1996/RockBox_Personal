#!/usr/bin/env python3
"""Replay real dock commands through production lingo-3 dispatch and polling."""
from pathlib import Path
import re, subprocess, tempfile
root = Path(__file__).resolve().parents[2]
core = (root/'apps/iap/iap-core.c').read_text()
volume = core[core.index('unsigned char iap_volume_byte('):core.index('/* This thread is waiting for events')]
rx = core[core.index('int remote_control_rx(void)'):core.index('const unsigned char *iap_get_serbuf(')]
lingo = (root/'apps/iap/iap-lingo3.c').read_text()
start = lingo.index('case 0x04:', lingo.index('\n        case 0x0E:\n'))
block = lingo[start:lingo.index('/* Equalizer', start)]
start = lingo.index('case 0x10:', lingo.index('\n        case 0x0E:\n'))
block += lingo[start:lingo.index('\n                default:', start)]
buttons = (root/'firmware/target/arm/s5l8702/ipod6g/button-target.h').read_text()
buttons = '\n'.join(re.findall(r'^#define BUTTON_(?:RC_.*|REMOTE)\s+.*$', buttons, re.M))
ui = (root/'apps/gui/tv_ui.c').read_text()
start = ui.index('bool tv_ui_remote_volume_locked(void)')
lock_policy = ui[start:ui.index('\nvoid tv_ui_set_section', start)]
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#define HZ 100
#define MAX(a,b) ((a)>(b)?(a):(b))
#define TIME_AFTER(a,b) ((long)((b)-(a))<0)
#define TIME_BEFORE(a,b) TIME_AFTER(b,a)
#include "iap-volume-gesture.h"
static struct iap_volume_gesture iap_volume_gesture;
static long current_tick;
#define BUTTON_NONE 0
#define BUTTON_PLAY 8
#define SOUND_VOLUME 0
#define AUDIO_STATUS_PLAY 1
#define USB_ENABLE_AUDIO
#define IAP_ACK_BAD_PARAM 4
static struct {int volume;} global_status;
static struct {bool mute;int volume;} device;
static bool nav=true,quiet,kokkia,usb,active=true,home_navigation,guide_owner;
static bool tv_ui_active(void){return active;}
static int iap_navigation_volume=-1,iap_volume_button,iap_repeatbtn;
static int iap_remotebtn,iap_timeoutbtn,acks,notifications;
static int sound_min(int x){(void)x;return -60;}
static int sound_max(int x){(void)x;return 12;}
static bool iap_remote_navigation_active(void){return nav&&!kokkia;}
static bool iap_kokkia_present(void){return kokkia;}
static bool iap_remote_input_suppressed(void){return quiet;}
static bool iap_kokkia_connected(void){return kokkia;}
static int audio_status(void){return 0;}
static bool usb_audio_get_active(void){return usb;}
static void iap_remote_packet(void *p,int l,int v,int b,char s){(void)p;(void)l;(void)v;(void)b;(void)s;}
static unsigned char tx[8];
static unsigned txlen;
#define IAP_TX_INIT(l,c) do {txlen=0;tx[txlen++]=(l);tx[txlen++]=(c);} while(0)
#define IAP_TX_PUT_IPOD_TRANSID() ((void)0)
#define IAP_TX_PUT(v) do {assert(txlen<sizeof(tx));tx[txlen++]=(v);} while(0)
unsigned char iap_volume_byte(void);
void iap_set_remote_volume(void);
static void iap_send_tx(void)
{
 assert(txlen==5 && tx[0]==3 && tx[1]==13 && tx[2]==4 && tx[3]==0);
 assert(tx[4]==iap_volume_byte());notifications++;
}
static void cmd_ok(int cmd){(void)cmd;acks++;}
static int disable_irq_save(void){return 0;}
static void restore_irq(int level){(void)level;}
#define CHECKLEN(n) do{if(len<(n))return;}while(0)
''' + buttons + '\n\n' + lock_policy + volume + rx + r'''
static void dispatch(unsigned len,const unsigned char *buf,unsigned doff)
{
 int cmd=0x0e;
 switch(buf[2+doff]) {
''' + block + r'''
 }
}
static int pulse(void)
{
 int a=remote_control_rx();
 assert(remote_control_rx()==a);
 assert(remote_control_rx()==0);
 assert(remote_control_rx()==0);
 assert(!iap_repeatbtn && !iap_volume_button);
 return a;
}
int main(void)
{
 global_status.volume=6;
 /* First six packets from the user's exported trace, then the next burst. */
 const unsigned char values[]={235,238,241,238,235,232,235,238,241,244,247,250,247,244,241,238,235,232};
 const int directions[]={1,1,1,-1,-1,-1,1,1,1,1,1,1,-1,-1,-1,-1,-1,-1};
 for(unsigned n=0;n<sizeof(values);n++) {
  current_tick+=HZ; /* isolated presses */
  unsigned char p[]={3,14,4,0,values[n],1};
  dispatch(sizeof(p),p,0);
  assert(global_status.volume==6);
  assert(iap_volume_byte()==234); /* All dock queries report audio gain. */
  assert(pulse()==(directions[n]>0?BUTTON_RC_VOL_UP:BUTTON_RC_VOL_DOWN));
 }
 /* Same raw value, no dB rounding and no phantom extra step. */
 unsigned char equal[]={3,14,4,0,232,1};
 dispatch(sizeof(equal),equal,0);assert(remote_control_rx()==0);
 unsigned char end[]={3,14,4,0,255,1};
 dispatch(sizeof(end),end,0);assert(pulse()==BUTTON_RC_VOL_UP);
 assert(notifications==1 && iap_navigation_volume==234);
 current_tick+=HZ;end[4]=237;dispatch(sizeof(end),end,0);assert(pulse()==BUTTON_RC_VOL_UP);
 end[4]=0;dispatch(sizeof(end),end,0);assert(pulse()==BUTTON_RC_VOL_DOWN);
 assert(notifications==2 && iap_navigation_volume==234);
 /* IDPS offsets, ordinary/mute commands, quarantine, USB and headset. */
 current_tick+=HZ;
 unsigned char tid[]={3,14,0x12,0x34,4,0,125,1};
 dispatch(sizeof(tid),tid,2);assert(pulse()==BUTTON_RC_VOL_DOWN);
 quiet=true;tid[6]=128;dispatch(sizeof(tid),tid,2);
 assert(remote_control_rx()==0 && global_status.volume==6);quiet=false;
 unsigned char sync[]={3,14,4,0,100,0};
 /* Echoes after navigation must not change audio or add a scroll step. */
 dispatch(sizeof(sync),sync,0);assert(global_status.volume==6);
 assert(remote_control_rx()==0 && iap_navigation_volume==100);
 unsigned char absolute[]={3,14,16,0,110,110,0};
 dispatch(sizeof(absolute),absolute,0);assert(global_status.volume==6);
 assert(remote_control_rx()==0 && iap_navigation_volume==110);
 unsigned char absolute_tid[]={3,14,0x12,0x34,16,0,115,115,0};
 dispatch(sizeof(absolute_tid),absolute_tid,2);assert(global_status.volume==6);
 assert(remote_control_rx()==0 && iap_navigation_volume==115);
 /* Initial synchronization still applies before a navigation cursor exists. */
 iap_navigation_volume=-1;
 dispatch(sizeof(sync),sync,0);assert(global_status.volume==iap_volume_from_byte(100));
 assert(remote_control_rx()==0 && iap_navigation_volume==-1);
 sync[3]=1;sync[5]=1;dispatch(sizeof(sync),sync,0);assert(device.mute);
 for(int mode=0;mode<3;mode++) {
  global_status.volume=6;nav=mode!=0;kokkia=mode==1;usb=mode==2;
  sync[3]=0;dispatch(sizeof(sync),sync,0);
  assert(global_status.volume==(usb?6:iap_volume_from_byte(100)));
  assert(remote_control_rx()==0);
 }
 nav=true;kokkia=usb=false;global_status.volume=6;
 for(unsigned len=3;len<5;len++)dispatch(len,sync,0);
 assert(global_status.volume==6);
 /* A physical tap in the captured trace contains three packets. Two taps
  * separated by the captured quiet gap must still produce two selections. */
 nav=true;kokkia=usb=quiet=false;global_status.volume=6;
 iap_navigation_volume=-1;iap_volume_gesture.direction=0;
 const long ticks[]={4518,4529,4540,4669,4679,4693,6159,6168,6183,6219,6229,6242,6678,6684,6697,6729,6739,6750};
 int presses=0;
 for(unsigned n=0;n<sizeof(values);n++) {
  current_tick=ticks[n];unsigned char p[]={3,14,4,0,values[n],1};
  dispatch(sizeof(p),p,0);
  unsigned char echo[]={3,14,4,0,values[n],0};
  dispatch(sizeof(echo),echo,0);
  unsigned char absolute_echo[]={3,14,16,0,values[n],values[n],0};
  dispatch(sizeof(absolute_echo),absolute_echo,0);
  assert(global_status.volume==6);
  if(iap_volume_button) {assert(pulse()==(directions[n]>0?BUTTON_RC_VOL_UP:BUTTON_RC_VOL_DOWN));presses++;}
  else assert(remote_control_rx()==0);
 }
 assert(presses==6); /* six taps, not eighteen packet-sized jumps */
 /* Intentional holds repeat after half a second, at a bounded cadence. */
 iap_navigation_volume=128;iap_volume_gesture.direction=0;presses=0;
 for(int n=0;n<11;n++) {
  current_tick=10000+n*10;unsigned char p[]={3,14,4,0,129+n,1};
  dispatch(sizeof(p),p,0);
  if(iap_volume_button) {assert(pulse()==BUTTON_RC_VOL_UP);presses++;}
 }
 assert(presses==4);
 current_tick+=1;unsigned char reverse[]={3,14,4,0,138,1};
 dispatch(sizeof(reverse),reverse,0);assert(pulse()==BUTTON_RC_VOL_DOWN);
 /* The dock applies outgoing levels to its own attenuator. Simulate its
  * next request relative to the reply: Home and guide must stay at the
  * same hardware and software gain, with one move per physical tap. */
 for(int context=0;context<2;context++) {
  home_navigation=context==0;guide_owner=context==1;
  global_status.volume=6;iap_navigation_volume=-1;iap_volume_gesture.direction=0;
  assert(tv_ui_remote_volume_locked());
  int moves=0;
  for(unsigned n=0;n<sizeof(values);n++) {
   current_tick=ticks[n]+context*10000+30000;
   unsigned char request[]={3,14,4,0,234+3*directions[n],1};
   dispatch(sizeof(request),request,0);
   assert(global_status.volume==6 && tx[4]==234 && iap_volume_byte()==234);
   if(iap_volume_button){assert(pulse()==(directions[n]>0?BUTTON_RC_VOL_UP:BUTTON_RC_VOL_DOWN));moves++;}
   unsigned char echo[]={3,14,4,0,234,0};int before=notifications;
   dispatch(sizeof(echo),echo,0);assert(notifications==before);
  }
  assert(moves==6);
  /* Block even initial sync and mute, not just cursor echoes. */
  iap_navigation_volume=-1;
  unsigned char initial[]={3,14,4,0,10,0};
  dispatch(sizeof(initial),initial,0);assert(global_status.volume==6 && tx[4]==234);
  initial[3]=1;dispatch(sizeof(initial),initial,0);assert(global_status.volume==6 && tx[3]==0);
  unsigned char absolute[]={3,14,16,0,250,250,0};
  dispatch(sizeof(absolute),absolute,0);assert(global_status.volume==6 && tx[4]==234);
  assert(remote_control_rx()==0);
 }
 /* The actual captured dock trace does NOT reset to our reported gain
  * between packets. In particular, the first Down (241 -> 238) is still
  * above the fixed gain (234) and must be recognized as Down. */
 home_navigation=true;guide_owner=false;global_status.volume=6;
 iap_navigation_volume=-1;iap_volume_gesture.direction=0;
 int recorded_moves=0;
 for(unsigned n=0;n<sizeof(values);n++) {
  current_tick=ticks[n]+65000;
  unsigned char request[]={3,14,4,0,values[n],1};
  dispatch(sizeof(request),request,0);
  assert(global_status.volume==6 && iap_volume_byte()==234);
  if(iap_volume_button) {
   assert(pulse()==(directions[n]>0?BUTTON_RC_VOL_UP:BUTTON_RC_VOL_DOWN));
   recorded_moves++;
  }
 }
 assert(recorded_moves==6);
 /* Fullscreen releases the UI lock: its raw volume buttons reach the
  * player and the resulting real gain is reported to the dock. */
 home_navigation=guide_owner=false;assert(!tv_ui_remote_volume_locked());
 iap_navigation_volume=-1;iap_volume_gesture.direction=0;current_tick+=HZ;
 unsigned char fullscreen[]={3,14,4,0,237,1};
 dispatch(sizeof(fullscreen),fullscreen,0);assert(pulse()==BUTTON_RC_VOL_UP);
 global_status.volume=7;iap_set_remote_volume();assert(tx[4]==237);
 home_navigation=true;active=false;assert(!tv_ui_remote_volume_locked());
 puts("PASS: Home/guide lock at software and dock output; fullscreen actual-gain feedback; six tap bursts -> six steps, delayed hold/reverse, captured +/- sequence through lingo 3, sampled press/release, endpoint feedback, IDPS, wake input path, mute/sync/USB/headset isolation");
}
'''
with tempfile.TemporaryDirectory(prefix='dock-volume-') as tmp:
    p=Path(tmp);(p/'test.c').write_text(code)
    subprocess.run(['cc','-I'+str(root/'apps/iap'),'-std=c99','-Wall','-Wextra','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
