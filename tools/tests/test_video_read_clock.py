#!/usr/bin/env python3
"""Execute compressed read-ahead and mixer-clock accounting with bounded I/O."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'apps/video_pcm.c').read_text()
def function(name):
    start=source.index('uint32_t '+name+'(')
    end=source.index('\n}\n',start)+3
    return source[start:end]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#define MIN(a,b) ((a)<(b)?(a):(b))
#define PCM_MIXER_CHAN_PLAYBACK 0
static uint32_t clock_samples, clock_base_ms, clock_sample_rate;
static bool clock_block_has_media;
static size_t waiting;
static void pcm_play_lock(void){}
static void pcm_play_unlock(void){}
static size_t mixer_channel_get_bytes_waiting(int c){(void)c;return waiting;}
static unsigned reads;
static unsigned char media[1024];
static ssize_t file_read_at(int fd,void *out,size_t size,uint32_t offset)
{
    (void)fd;reads++;
    if(offset>=sizeof(media))return 0;
    size=MIN(size,sizeof(media)-offset);memcpy(out,media+offset,size);return size;
}
#include "video_read_cache.h"
'''+function('video_pcm_get_clock_ms')+r'''
int main(void)
{
    uint8_t buffer[128]; struct video_read_cache cache={0,0};
    for(unsigned i=0;i<sizeof(media);i++)media[i]=i%251;
    const uint8_t *p=video_read_cached(1,buffer,sizeof(buffer),&cache,50,20);
    assert(p && !memcmp(p,media+50,20) && reads==1);
    p=video_read_cached(1,buffer,sizeof(buffer),&cache,70,60);
    assert(p && !memcmp(p,media+70,60) && reads==1);
    p=video_read_cached(1,buffer,sizeof(buffer),&cache,170,40);
    assert(p && !memcmp(p,media+170,40) && reads==2);
    assert(!video_read_cached(1,buffer,sizeof(buffer),&cache,1020,8));
    assert(!video_read_cached(1,buffer,sizeof(buffer),&cache,UINT32_MAX,1));
    assert(!video_read_cached(1,buffer,sizeof(buffer),&cache,0,129));
    assert(!video_read_cached(1,buffer,sizeof(buffer),&cache,0,0));
    clock_sample_rate=44100;clock_samples=44100;clock_base_ms=9000;
    clock_block_has_media=true;waiting=441*4;
    assert(video_pcm_get_clock_ms()==9990);
    waiting=0;assert(video_pcm_get_clock_ms()==10000);
    /* Underrun silence is never subtracted from the media clock. */
    clock_block_has_media=false;waiting=256*4;
    assert(video_pcm_get_clock_ms()==10000);
    clock_samples=0;clock_block_has_media=true;
    assert(video_pcm_get_clock_ms()==9000);
    puts("PASS: bounded read-ahead, seeks/EOF/oversized reads; mixer pending samples, silence freeze and seek epoch");
}
'''
with tempfile.TemporaryDirectory() as temp:
    p=Path(temp);(p/'test.c').write_text(code)
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-I',str(root/'apps'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
