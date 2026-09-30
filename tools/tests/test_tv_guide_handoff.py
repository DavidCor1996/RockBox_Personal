#!/usr/bin/env python3
"""A modal LCD update must completely replace the prior TV guide geometry."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[2]
s=(r/'apps/plugins/mpegplayer/livetv_guide.c').read_text()
a=s.index('static bool livetv_tv_drawing');b=s.index('/* Fill a rectangle,',a)
code=r'''
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#define HAVE_COMPOSITE_VIDEO_OUT 1
#include "tv_guide.h"
static bool owner;
static int full,partial;
static bool render(enum tv_guide_command cmd,int x,int y,int w,int h,unsigned c,const void*d)
{(void)x;(void)y;(void)w;(void)h;(void)c;(void)d;assert(cmd==TV_GUIDE_RELEASE);bool old=owner;owner=false;return old;}
static void update(void){full++;}
static void update_rect(int x,int y,int w,int h){assert(x==2&&y==3&&w==40&&h==50);partial++;}
static struct {bool(*tv_guide_render)(enum tv_guide_command,int,int,int,int,unsigned,const void*);
 void(*lcd_update)(void);void(*lcd_update_rect)(int,int,int,int);} api={render,update,update_rect},*rb=&api;
''' + s[a:b] + r'''
int main(void)
{
 for(int i=0;i<100;i++)
 {
  owner=true;livetv_tv(TV_GUIDE_RELEASE,0,0,0,0,0,NULL);
  assert(!owner&&livetv_tv_restore_lcd);
  livetv_commit_rect(2,3,40,50);assert(full==i+1&&!livetv_tv_restore_lcd);
  livetv_commit_rect(2,3,40,50);assert(partial==2*i+1);
  livetv_tv(TV_GUIDE_RELEASE,0,0,0,0,0,NULL);
  livetv_commit_rect(2,3,40,50);assert(full==i+1&&partial==2*i+2);
 }
 puts("PASS: 100 TV guide-to-modal handoffs commit a full first LCD frame; later and handheld updates remain partial");
}
'''
with tempfile.TemporaryDirectory(prefix='tv-guide-handoff-') as t:
 p=Path(t);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'apps/gui'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
