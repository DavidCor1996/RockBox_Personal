#!/usr/bin/env python3
"""Execute the actual table readers on valid, truncated and overflowing data."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'apps/mp4_demux.c').read_text()
functions=[]
for name in ('read_chunk_stts_audio','read_chunk_stts','read_chunk_co64_audio','read_chunk_co64','read_chunk_stss'):
 a=s.index('static bool '+name+'(');b=s.index('\n}\n',a)+3;functions.append(s[a:b])
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define MP4V_MAX_STTS 4096
#define MP4V_MAX_STSS 8192
struct time_entry {uint32_t sample_count,sample_delta;};
struct result {
 struct time_entry stts[4096],audio_stts[4096];
 uint32_t num_stts,audio_num_stts,num_stco,audio_num_stco;
 uint32_t chunk_offsets_cap,audio_chunk_offsets_cap;
 uint32_t chunk_offsets[8],audio_chunk_offsets[8];
 uint32_t stss[MP4V_MAX_STSS],num_stss;
};
struct stream {uint32_t data[8200];unsigned pos,len;bool eof;};
struct mp4_parse_ctx {struct stream stream;struct result *res;};
static uint32_t stream_read_uint32(struct stream *s){if(s->pos>=s->len){s->eof=true;return 0;}return s->data[s->pos++];}
static void stream_skip(struct stream *s,size_t n){s->pos+=n/4;if(s->pos>s->len)s->eof=true;}
'''+'\n'.join(functions)+r'''
int main(void){
 bool (*readers[])(struct mp4_parse_ctx*,size_t)={read_chunk_stts_audio,read_chunk_stts,read_chunk_co64_audio,read_chunk_co64};
 struct result r={0};r.chunk_offsets_cap=r.audio_chunk_offsets_cap=8;
 for(int i=0;i<4;i++){
  struct mp4_parse_ctx c={.res=&r,.stream={.data={0,1,0,120},.len=4}};
  assert(readers[i](&c,24));
  for(size_t n=0;n<24;n++){
   c.stream=(struct stream){.data={0,1,0,120},.len=4};assert(!readers[i](&c,n));
  }
  c.stream=(struct stream){.data={0,0xffffffff,0,120},.len=4};assert(!readers[i](&c,24));
  c.stream=(struct stream){.data={0,1,1,120},.len=4};
  if(i>=2)assert(!readers[i](&c,24));
  c.stream=(struct stream){.data={0,1},.len=2};
  assert(!readers[i](&c,24));
 }
 struct mp4_parse_ctx c={.res=&r,.stream={.data={0,4678},.len=4680}};
 for(unsigned i=0;i<4678;i++)c.stream.data[i+2]=i+1;
 assert(read_chunk_stss(&c,16+4678*4));
 assert(r.num_stss==4678 && r.stss[4677]==4678);
 c.stream.pos=0;assert(!read_chunk_stss(&c,15));
 c.stream.data[1]=8193;c.stream.pos=0;assert(!read_chunk_stss(&c,16+8193*4));
 c.stream.data[1]=1;c.stream.data[2]=0;c.stream.pos=0;assert(!read_chunk_stss(&c,20));
 c.stream.data[1]=2;c.stream.data[2]=2;c.stream.data[3]=1;c.stream.pos=0;assert(!read_chunk_stss(&c,24));
 puts("PASS: production MP4 timing/co64/sync readers; 4678-entry movie index, truncated/oversized tables, invalid keyframes and high offsets");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
