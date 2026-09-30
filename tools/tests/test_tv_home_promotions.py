#!/usr/bin/env python3
"""Exercise Home promotion selection with production idle/scan logic."""
import pathlib,re,subprocess,tempfile
r=pathlib.Path(__file__).resolve().parents[2]
s=(r/'apps/root_menu.c').read_text()
helpers=s[s.index('static void video_trim_line('):s.index('static bool video_parse_manifest_line(')]
locked=s[s.index('static bool video_manifest_entry_locked('):s.index('static int video_manifest_year(')]
promo=s[s.index('#define TV_HOME_PROMO_MAX'):s.index('static int tv_home_category =')]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#define HZ 100
#define MIN(a,b) ((a)<(b)?(a):(b))
#define VIDEO_BROWSER_TITLE_MAX 64
#define VIDEO_LIST_ART_ID_LEN 25
#define VIDEO_LIST_MANIFEST_LINE_MAX 1024
#define ARRAYLEN(a) ((int)(sizeof(a)/sizeof((a)[0])))
#define TIME_BEFORE(a,b) ((long)((a)-(b))<0)
static const char *VIDEO_LIST_INDEX;
static long current_tick;
static bool focused;
static bool active=true,held,video_netflix_banner_valid;
static int queued,opened,loads,lines,queue_after;
static char video_netflix_banner_art_id[25];
static bool tv_ui_active(void){return active;}
static bool button_hold(void){return held;}
static int button_queue_count(void){return queued || (queue_after && lines>=queue_after);}
static void strmemccpy(char *dst,const char *src,size_t size){snprintf(dst,size,"%s",src);}
static int read_line(int fd,char *line,size_t size)
{
    size_t n=0;char c;
    while(read(fd,&c,1)==1){if(c=='\n')break;if(n+1<size)line[n++]=c;}
    line[n]=0;lines++;return n;
}
static int test_open(const char *path,int flags){int fd=open(path,flags);if(fd>=0)opened++;return fd;}
static int test_close(int fd){assert(opened==1);opened--;return close(fd);}
#define open test_open
#define close test_close
static void video_netflix_load_banner(const char *id)
{
    loads++;video_netflix_banner_valid=id[0]!='d';
    strcpy(video_netflix_banner_art_id,video_netflix_banner_valid?id:"");
}
''' + helpers + locked + promo + r'''
static void row(FILE *f,const char *kind,const char *locked,const char *id,
                const char *show,const char *title)
{
    for(int col=0;col<30;col++)
    {
        const char *v=col==3?title:col==4?kind:col==7?show:col==11?locked:col==29?id:"";
        fprintf(f,"%s%s",col?"\t":"",v);
    }
    fputc('\n',f);
}
static bool service(bool idle)
{
    int before=loads;bool changed=tv_home_promo_service(idle,focused);
    assert(!opened && loads-before<=1);return changed;
}
int main(int argc,char **argv)
{
    assert(argc==2);VIDEO_LIST_INDEX=argv[1];
    FILE *f=fopen(VIDEO_LIST_INDEX,"w");assert(f);
    fputs("# v8\nmalformed\n",f);
    row(f,"show","1","aaaaaaaaaaaaaaaaaaaaaaaa","Locked","Episode");
    row(f,"music_video","0","aaaaaaaaaaaaaaaaaaaaaaaa","Music","Clip");
    row(f,"show","0","../../bad","Invalid","Episode");
    row(f,"show","0","aaaaaaaaaaaaaaaaaaaaaaaa","Death Note","1.28");
    row(f,"show","0","aaaaaaaaaaaaaaaaaaaaaaaa","Death Note","Another episode");
    row(f,"movie","0","bbbbbbbbbbbbbbbbbbbbbbbb","","Mortal Kombat");
    row(f,"show","0","dddddddddddddddddddddddd","Missing art","Episode");
    row(f,"show","0","cccccccccccccccccccccccc","RWBY","Episode");
    fclose(f);
    tv_home_promo_reset();assert(!service(true)&&!loads);
    current_tick=HZ;active=false;assert(!service(true));active=true;
    held=true;assert(!service(true));held=false;
    queued=1;assert(!service(true));queued=0;
    assert(!service(false));assert(!service(true));current_tick+=HZ;
    assert(service(true)&&loads==1);
    assert(tv_home_promo_count==3);
    for(int i=0;i<3;i++)assert(tv_home_promos[i].show&&strcmp(tv_home_promos[i].title,"Mortal Kombat"));
    focused=true;current_tick+=20*HZ;assert(!service(true)&&loads==1);
    for(int i=0;i<3;i++)
    {
        int before=loads;tv_home_promo_move(1);
        queued=1;assert(!service(true)&&loads==before);queued=0;
        assert(service(true)&&loads==before+1);
        assert(tv_home_promo_ready==(tv_home_promos[tv_home_promo_selected].art[0]!='d'));
    }
    assert(tv_home_promo_selected==0);
    tv_home_promo_move(-1);assert(tv_home_promo_selected==2);assert(service(true));
    focused=false;current_tick+=8*HZ;assert(service(true)&&tv_home_promo_selected==0);
    /* More than five unique shows: random distinct sets, stable while focused,
     * no movies/locked entries, and no partial carousel during scanning. */
    f=fopen(VIDEO_LIST_INDEX,"w");assert(f);
    for(int i=0;i<10;i++)
    {
        char id[25],title[32];snprintf(id,sizeof(id),"%024x",i+1);snprintf(title,sizeof(title),"Show %d",i);
        for(int episode=0;episode<5;episode++)row(f,"show","0",id,title,"Episode");
    }
    fclose(f);unsigned seen=0;
    for(int visit=0;visit<20;visit++)
    {
        tv_home_promo_reset();current_tick+=HZ;lines=0;
        assert(!service(true)&&tv_home_promo_count==0&&lines==32);
        assert(service(true)&&tv_home_promo_count==5);
        unsigned picks=0;
        for(int i=0;i<5;i++)
        {
            int n=atoi(tv_home_promos[i].title+5);assert(n>=0&&n<10);
            assert(!(picks&(1u<<n)));picks|=1u<<n;
        }
        seen|=picks;focused=true;
        char first[64];strcpy(first,tv_home_promos[0].title);
        current_tick+=20*HZ;assert(!service(true)&&!strcmp(first,tv_home_promos[0].title));
        focused=false;
    }
    assert(seen==1023);
    /* Bounded malformed catalogue and input arriving during a scan. */
    f=fopen(VIDEO_LIST_INDEX,"w");assert(f);
    for(int i=0;i<40;i++)fputs("bad\n",f);
    row(f,"show","0","bbbbbbbbbbbbbbbbbbbbbbbb","ALF","Pilot");fclose(f);
    tv_home_promo_reset();current_tick+=HZ;lines=0;
    assert(!service(true)&&lines==32);
    current_tick+=HZ;assert(service(true)&&!strcmp(tv_home_promo_title,"ALF"));
    tv_home_promo_reset();current_tick+=HZ;lines=0;queue_after=3;
    assert(!service(true)&&lines==3);queue_after=0;
    unlink(VIDEO_LIST_INDEX);current_tick+=HZ;
    assert(!service(true)&&!opened);
    puts("PASS: Home-only promotion idle gates, focused pause, bidirectional wrap, 8-second rotation, five randomized unique shows, movies excluded, locked/invalid/duplicate/missing art, 32-row and one-bitmap limits, queue interruption, EOF/reset and balanced descriptors");
}
'''
# All TV Netflix callers must pass no banner; handheld paths retain theirs.
assert 'posters, NULL,' in s
assert 'if (!tv_ui_active()) video_netflix_load_banner(detail.banner_art_id);' in s
with tempfile.TemporaryDirectory(prefix='tv-promotions-') as temp:
    p=pathlib.Path(temp);(p/'test.c').write_text(code)
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test'),str(p/'index.tsv')],check=True)
