#!/usr/bin/env python3
"""Exercise actual padded RGB/art/video code with ASan and UBSan."""
import importlib.util, pathlib, subprocess, tempfile, os
root=pathlib.Path(__file__).resolve().parents[2]
os.environ.setdefault('ASAN_OPTIONS','detect_leaks=0')
spec=importlib.util.spec_from_file_location('base',root/'tools/tests/composite_enhanced_gate.py')
base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)
s=(root/'firmware/target/arm/s5l8702/ipod6g/videoout-6g.c').read_text()
extend=s[s.index('static void svid_extend_planar_edges'):s.index('static bool svid_present_planar_update')]
yuv=s[s.index('bool ipod6g_videoout_mirror_yuv420'):s.index('#ifndef VIDEOOUT_ENHANCED_TEST',s.index('bool ipod6g_videoout_mirror_yuv420'))]
pre=base.prelude.replace('#define SVID_PLANAR_GUARD 0','#define VIDEOOUT_ENHANCED_TEST 1\n#define SVID_PLANAR_GUARD 32\n#define SVID_PLANAR_SOURCE_WIDTH 640\n#define SVID_PLANAR_Y_HEIGHT 480').replace('Y_WIDTH 640','Y_WIDTH 704').replace('Y_SIZE 307200','Y_SIZE 337920').replace('C_WIDTH 320','C_WIDTH 352').replace('C_SIZE 76800','C_SIZE 84480').replace('FRAME_SIZE 460800','FRAME_SIZE 506880')
stubs='''
static bool ipod6g_videoout_active(void){return svid_active;}
static bool svid_begin_planar_update(void){return true;}
static bool svid_present_planar_update(void){svid_extend_planar_edges();return true;}
static void svid_commit_planar_rect(int x,int y,int w,int h)
{assert(x>=32&&x+w<=672&&y>=0&&y+h<=480);}
'''
tests=r'''
static uint16_t screen[320*240];
static uint8_t saved[506880], payload[151776];
static uint8_t yy[320*240],cc[160*120];
static void guards(void)
{
 uint8_t *p=svid_planar_write_buffer;
 for(int plane=0;plane<3;plane++){
  int div=plane?2:1, stride=704/div, g=32/div, w=640/div,h=480/div;
  for(int y=0;y<h;y++)for(int x=0;x<g;x++){
   assert(p[y*stride+x]==p[y*stride+g]);
   assert(p[y*stride+g+w+x]==p[y*stride+g+w-1]);
  }
  p+=stride*h;
 }
}
int main(void)
{
 for(int i=0;i<320*240;i++)screen[i]=(i*137+19)&65535;
 svid_rgb565_expand(screen,0,0,320,240,320);svid_extend_planar_edges();guards();
 for(int y=0;y<240;y++)for(int x=0;x<320;x++){
  assert(svid_planar_write_buffer[y*2*704+x*2+32]==(screen[y*320+x]&255));
  assert(svid_planar_write_buffer[337920+y*352+x+16]==(screen[y*320+x]>>8));
 }
 memcpy(saved,svid_planar_write_buffer,sizeof(saved));
 for(int w=1;w<=32;w++){
  memset(svid_planar_write_buffer,0x55,sizeof(saved));
  svid_rgb565_expand(screen,0,0,w,240,320);
  svid_rgb565_expand(screen+w,w,0,320-w,240,320);
  svid_extend_planar_edges();guards();
  assert(!memcmp(saved,svid_planar_write_buffer,sizeof(saved)));
 }
 memset(payload,177,sizeof(payload));
 assert(ipod6g_videoout_art_write(0,payload,sizeof(payload)));
 assert(ipod6g_videoout_art_finish());
 assert(ipod6g_videoout_art_bind(screen+34*320+10,320,10,34));
 svid_rgb565_expand(screen,0,0,320,240,320);svid_extend_planar_edges();guards();
 assert(svid_planar_write_buffer[68*704+20+32]==177);
 assert(svid_planar_write_buffer[337920+34*352+10+16]==177);
 assert(svid_planar_write_buffer[422400+34*352+10+16]==177);
 ipod6g_videoout_art_clear();
 for(unsigned i=0;i<sizeof(yy);i++)yy[i]=(i*17+23)&255;
 for(unsigned i=0;i<sizeof(cc);i++)cc[i]=(i*29+7)&255;
 assert(ipod6g_videoout_mirror_yuv420(yy,cc,cc,0,0,320,0,0,320,240));guards();
 for(int y=0;y<480;y++)for(int x=0;x<640;x++){
  unsigned total=0,n=0;
  for(int j=0;j<=(y&1);j++)for(int i=0;i<=(x&1);i++)
   {total+=yy[MIN(y/2+j,239)*320+MIN(x/2+i,319)];n++;}
  assert(svid_planar_write_buffer[y*704+x+32]==(total+n/2)/n);
 }
 assert(!ipod6g_videoout_mirror_yuv420(yy,cc,cc,0,0,320,-1,0,320,240));
 puts("Padded RGB, odd left strips, artwork and actual YUV mirror passed; visible geometry preserved");
}
'''
with tempfile.TemporaryDirectory(prefix='edge-guard-') as td:
 p=pathlib.Path(td);header=root/'firmware/target/arm/s5l8702/ipod6g/videoout-scale2x.h'
 (p/'gate.c').write_text(pre+'\n#include "'+str(header)+'"\n'+extend+base.production+stubs+yuv+tests)
 subprocess.run(['cc','-std=c11','-g','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'gate.c'),'-o',str(p/'gate')],check=True)
 subprocess.run([str(p/'gate')],check=True)
