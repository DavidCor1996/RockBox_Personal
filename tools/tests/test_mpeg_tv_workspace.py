#!/usr/bin/env python3
"""Check production MPEG arena partitioning against three VGA reference frames."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
src = (root / 'apps/plugins/mpegplayer/alloc.c').read_text()
header = (root / 'apps/plugins/mpegplayer/mpegplayer.h').read_text()


def function(text, prefix):
    start = text.index(prefix)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


pool = header[header.index('#ifdef HAVE_COMPOSITE_VIDEO_OUT'):
              header.index('/** MPEG audio buffer **/')]
globals_ = src[src.index('/* Main allocator */'):
               src.index('#if defined(DEBUG)')]
code = r'''
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
typedef int mpeg2_alloc_t;
#define SHAREDBSS_ATTR
#define MPEG_ALLOC_MPEG2_BUFFER 7
#define CACHEALIGN_UP(x) (((x)+31)&~31)
#define ALIGN_BUFFER(p,n,a) do {(void)(p);(void)(n);(void)(a);} while(0)
#define DEBUGF(...) ((void)0)
#define IF_COP(...)
static struct {void *(*memset)(void *,int,size_t);} api={memset};
#define rb (&api)
''' + pool + globals_ + '\n'.join(function(src, name) for name in [
    'static void * mpeg_malloc_internal (', 'void *mpeg_malloc(size_t',
    'bool mpeg_alloc_init(', 'void * mpeg2_malloc(', 'void * mpeg2_bufalloc(',
    'void * mpeg2_get_buf(']) + r'''
static _Alignas(32) unsigned char arena[8*1024*1024];
int main(void)
{
    assert(mpeg_alloc_init(arena,sizeof(arena)));
    assert(mpeg2_bufalloc(18192,0));   /* Measured hosted decoder state. */
    assert(mpeg2_bufalloc(1222660,1)); /* Actual compressed chunk allocation. */
    bool all=true;
    for(int frame=0;frame<3;frame++) {
        all &= mpeg2_malloc(640*480,2)!=NULL;
        all &= mpeg2_malloc(320*240,2)!=NULL;
        all &= mpeg2_malloc(320*240,2)!=NULL;
    }
#ifdef HAVE_COMPOSITE_VIDEO_OUT
    assert(all);
    size_t spare=0;
    assert(mpeg2_get_buf(&spare));
    assert(spare>=320*240*3/2); /* LCD thumbnail fits after reference planes. */
    unsigned char *pcm=mpeg_malloc(176400,9);
    assert(pcm==arena+LIBMPEG2_ALLOC_SIZE);
    assert(pcm>=mpeg2_mallocbuf+mpeg2_bufsize);
    puts("PASS: TV workspace holds decoder, three VGA frames and thumbnail; PCM partition is separate");
#else
    assert(!all); /* Reproduce the old failure; other targets keep their size. */
    puts("PASS: reproduced the former 2 MiB VGA allocation failure");
#endif
}
'''
with tempfile.TemporaryDirectory(prefix='mpeg-workspace-') as temp:
    path = Path(temp)
    (path / 'test.c').write_text(code)
    for defines in ([], ['-DHAVE_COMPOSITE_VIDEO_OUT']):
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', *defines,
                        str(path / 'test.c'), '-o', str(path / 'test')], check=True)
        subprocess.run([str(path / 'test')], check=True,
                       env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
