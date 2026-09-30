#!/usr/bin/env python3
"""Verify Featured enters Netflix through its normal filtered hierarchy."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[2]
s=(r/'apps/root_menu.c').read_text()
a=s.index('static void videos_featured_select(');b=s.index('static int videos_scrn(',a)
source=s[a:b].rsplit("#endif",1)[0]
a=s.index('    if (videos_featured_request[0] && tv_ui_active()')
b=s.index('    videos_featured_request[0]=0;',a)+len('    videos_featured_request[0]=0;')
consume=s[a:b]
code=r'''
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
struct video_entry {const char *path,*title;};
struct video_browser_state {int count,selection,depth,parent_selection;struct video_entry entries[4];};
static bool videos_netflix_enter_level(struct video_browser_state *s,int i)
{
 assert(s->depth==0);s->parent_selection=i;s->depth++;s->selection=0;
 bool show=!strcmp(s->entries[i].path,"virtual:shows");s->count=2;
 s->entries[0]=(struct video_entry){show?"virtual:show:ALF":"/Videos/Movies/Death Note.m4v",show?"ALF":"Death Note"};
 s->entries[1]=(struct video_entry){show?"virtual:show:RWBY":"/Videos/Movies/Mortal Kombat.m4v",show?"RWBY":"Mortal Kombat"};
 return true;
}
''' + source + r'''
static char videos_featured_request[65];
static bool active=true;
static bool tv_ui_active(void){return active;}
static struct video_browser_state consume_request(struct video_browser_state state)
{
 bool netflix_appearance=true;
''' + consume + r'''
 return state;
}

static struct video_browser_state root(void)
{
 return (struct video_browser_state){.count=3,.entries={{"virtual:movies","Movies"},{"virtual:shows","TV Shows"},{"virtual:locked","Locked"}}};
}
int main(void)
{
 struct video_browser_state s=root();videos_featured_select(&s,"Mortal Kombat",false);
 assert(s.depth==1&&s.parent_selection==0&&s.selection==1);
 s=root();videos_featured_select(&s,"RWBY",true);
 assert(s.depth==1&&s.parent_selection==1&&s.selection==1);
 /* Removed and newly locked promotions cannot bypass the scanned library. */
 s=root();videos_featured_select(&s,"Removed or locked",true);
 assert(s.depth==1&&s.parent_selection==1&&s.selection==0);
 s=root();s.count=0;videos_featured_select(&s,"RWBY",true);assert(!s.depth);
 strcpy(videos_featured_request,"SRWBY");s=consume_request(root());
 assert(s.depth==1&&s.selection==1&&!videos_featured_request[0]);
 s=consume_request(root());assert(s.depth==0);
 strcpy(videos_featured_request,"MMortal Kombat");s=consume_request(root());
 assert(s.depth==1&&s.selection==1&&!videos_featured_request[0]);
 active=false;strcpy(videos_featured_request,"SRWBY");s=consume_request(root());
 assert(s.depth==0&&!videos_featured_request[0]);active=true;
 puts("PASS: Featured movie/show selection, normal parent/back hierarchy, missing or locked target fallback; no direct playback or unchecked path launch");
}
'''
with tempfile.TemporaryDirectory(prefix='featured-launch-') as t:
 p=Path(t);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
