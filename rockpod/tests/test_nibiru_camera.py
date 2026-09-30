"""Camera scale against an independent floating-point retail transform."""
from rockpod.tests.test_nibiru_audio_queue import ROOT, function
from rockpod.tests.test_nibiru_playability import run_c
import unittest


class NibiruCameraTest(unittest.TestCase):
    def test_room_translation_and_pitch_at_multiple_depths(self):
        source = (ROOT / 'apps/plugins/scummvm/agds_runtime.c').read_text()
        preamble = r'''
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#define AGDS_CANVAS_WIDTH 1024
#define AGDS_CANVAS_HEIGHT 768
#define AGDS_CHARACTER_SCALE_MIN 8192
#define AGDS_CHARACTER_SCALE_MAX 524288
#define DEBUGF(...) ((void)0)
struct scummvm_agds_screen_object { int y; };
struct scummvm_agds_scene_state {
    bool camera_set;
    int camera_pitch, camera_distance, camera_fov;
    char name[64];
};
static struct scummvm_agds_scene_state agds_script_scene;
static struct { int height; } video = {240};
#define agds_video (&video)
static char agds_character_scale_scene[64];
static int agds_character_scale_y, agds_character_scale_pitch;
static int agds_character_scale_distance, agds_character_scale_fov;
static size_t copy(char *d,const char *s,size_t n)
{ assert(strlen(s)<n); strcpy(d,s); return strlen(s); }
static const struct {
 int (*strcmp)(const char *,const char *);
 size_t (*strlcpy)(char *,const char *,size_t);
} api = {strcmp,copy};
#define rb (&api)
static long fp_sincos(unsigned long phase,long *cosine)
{
 double angle=(uint32_t)phase * (2.0*3.141592653589793/4294967296.0);
 *cosine=lround(cos(angle)*2147483647.0);
 return lround(sin(angle)*2147483647.0);
}
'''
        functions = '\n'.join(function(source, name+'(') for name in
            ['q16_mul', 'q16_div', 'camera_sincos_q16', 'camera_tan_q16',
             'character_camera_scale_q16'])
        run_c(preamble + functions + r'''
static double reference(int pitch,int distance,int fov,int y)
{
 double p=pitch*3.141592653589793/180, f=fov*3.141592653589793/360;
 double range=distance+32, ray=range/cos(p);
 double ry=ray*tan(f)*(2.0*y/768-1), c=ry*cos(p);
 double a=fmax(3.141592653589793/180,atan(ry/ray)+p);
 double world_y=-range*tan(f+p);
 double local_z=(-range*tan(p)-c-tan(a)*c*tan(p)-world_y)/tan(a);
 /* Modelview = camera rotation * room translation * actor translation. */
 double z=range+local_z, view_z=z*cos(p)-world_y*sin(p);
 return 120/tan(f)*z/(view_z*view_z);
}
int main(void)
{
 int cameras[][3]={{13,210,50},{-3,430,56},{5,450,36},{-3,474,45}};
 struct scummvm_agds_screen_object o;
 agds_script_scene.camera_set=true;
 for(unsigned i=0;i<4;i++) {
  agds_script_scene.camera_pitch=cameras[i][0];
  agds_script_scene.camera_distance=cameras[i][1];
  agds_script_scene.camera_fov=cameras[i][2];
  for(o.y=480;o.y<=720;o.y+=8) {
   double expected=reference(cameras[i][0],cameras[i][1],cameras[i][2],o.y);
   expected=fmin(8,fmax(.125,expected));
   double actual=character_camera_scale_q16(&o,1,1)/65536.0;
   assert(fabs(actual-expected)<0.002);
  }
 }
 return 0;
}
''')
