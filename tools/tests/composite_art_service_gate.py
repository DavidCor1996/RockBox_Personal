#!/usr/bin/env python3
"""Exercise the production idle loader with host filesystem/hardware shims."""
import pathlib, subprocess, tempfile, struct
root=pathlib.Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='tvart-service-') as td:
    work=pathlib.Path(td)
    data=bytes((i*7)&255 for i in range(151776))
    checksum=2166136261
    for b in data: checksum=((checksum^b)*16777619)&0xffffffff
    (work/'cover.tvart').write_bytes(b'TVART001'+struct.pack('<II',len(data),checksum)+data)
    for name in ('config.h','lcd.h','videoout.h','kernel.h','button.h','file.h',
                 'audio.h','metadata.h','tagcache.h','strlcpy.h'):
        (work/name).write_text('/* host shim supplied by prelude */\n')
    prelude=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#define VIDEOOUT_ENHANCED_TEST 1
#define HAVE_COMPOSITE_VIDEO_OUT 1
#define MAX_PATH 260
#define HZ 100
#define VIDEOOUT_ART_SIZE 151776u
#define TIME_BEFORE(a,b) ((a)<(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
typedef uint16_t fb_data;
struct mp3entry { char path[MAX_PATH]; };
static struct mp3entry track;
static long current_tick;
static int opens, live, binds;
static bool queued, held, commit, connected=true, fail_read, bad_crc;
static const char *fixture;
static unsigned offset_mock;
static uint16_t pixels[136*186];
static size_t test_strlcpy(char *d,const char *s,size_t n)
{ size_t len=strlen(s); if(n){size_t k=len<n?len:n-1;memcpy(d,s,k);d[k]=0;}return len; }
static int test_open(const char *path,int flags)
{ (void)path;opens++;int f=open(fixture,flags);if(f>=0)live++;return f; }
static int test_close(int f) {live--;return close(f);}
static ssize_t test_read(int f,void *p,size_t n)
{if(fail_read)return -1;ssize_t k=read(f,p,n);if(bad_crc&&k>16)((char*)p)[0]^=1;return k;}
static off_t filesize(int f){struct stat s;assert(!fstat(f,&s));return s.st_size;}
static bool button_queue_empty(void){return !queued;}
static bool button_hold(void){return held;}
static bool tagcache_is_usable(void){return true;}
static bool tagcache_commit_active(void){return commit;}
static bool videoout_active(void){return connected;}
static struct mp3entry *audio_current_track(void){return &track;}
static void yield(void){}
static void videoout_art_clear(void){offset_mock=0;}
static bool videoout_art_write(unsigned off,const void *p,unsigned n)
{(void)p;if(off!=offset_mock||n>151776-off)return false;offset_mock+=n;return true;}
static bool videoout_art_finish(void){return offset_mock==151776;}
static bool videoout_art_bind(const uint16_t *p,int s,int x,int y)
{(void)p;(void)s;(void)x;(void)y;binds++;return true;}
#define strlcpy test_strlcpy
#define open test_open
#define close test_close
#define read test_read
'''
    tests=r'''
static void request(void)
{tv_art_draw(track.path,pixels,136,10,34);}
static void complete(void)
{current_tick+=200;for(int i=0;i<20;i++){tv_art_service(true);current_tick+=20;}}
int main(int argc,char **argv)
{
    assert(argc==2); fixture=argv[1]; strcpy(track.path,"/Music/album/track.mp3");
    request(); assert(opens==0);tv_art_service(true);assert(opens==0);
    current_tick=200; queued=true;tv_art_service(true);assert(opens==0);
    queued=false; held=true;tv_art_service(true);assert(opens==0);held=false;
    current_tick=400;commit=true;tv_art_service(true);assert(opens==0);commit=false;
    tv_art_service(true);assert(live==1&&offset_mock==16384);
    complete();assert(ready&&live==0&&tv_art_changed()&&!tv_art_changed());
    request();assert(binds==1);complete();assert(opens==1);
    tv_art_leave();assert(live==0&&!ready);
    request();bad_crc=true;complete();assert(!ready&&live==0&&offset_mock==0);
    int before=opens;complete();assert(opens==before);bad_crc=false;tv_art_leave();
    request();fail_read=true;complete();assert(!ready&&live==0);fail_read=false;
    tv_art_leave();request();current_tick+=200;tv_art_service(true);assert(live==1);
    strcpy(track.path,"/Music/other/track.mp3");request();assert(live==1);
    current_tick+=200;tv_art_service(true);assert(live==1);
    tv_art_leave();assert(live==0);
    request();current_tick+=200;tv_art_service(true);assert(live==1);
    connected=false;tv_art_service(true);assert(live==0&&!ready);
    puts("Production idle service: settling, input/Hold/commit, CRC, I/O failure, track change and FD cleanup passed");
}
'''
    (work/'gate.c').write_text(prelude+'\n#include "'+str(root/'apps/gui/videoout_art.c')+'"\n'+tests)
    subprocess.run(['cc','-std=gnu11','-g','-O1','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-I'+str(work),str(work/'gate.c'),
                    '-o',str(work/'gate')],check=True)
    subprocess.run([str(work/'gate'),str(work/'cover.tvart')],check=True)
