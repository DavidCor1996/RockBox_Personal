"""Clock and queued speech completion regression tests; no game assets."""
import unittest
from rockpod.tests.test_nibiru_audio_queue import ROOT, function
from rockpod.tests.test_nibiru_playability import run_c


class NibiruTimingTest(unittest.TestCase):
    def test_speech_eof_waits_for_queued_tail(self):
        source = (ROOT / 'apps/plugins/scummvm/agds_runtime.c').read_text()
        run_c(r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#define AGDS_AUDIO_VOICES 2
#define DEBUGF(...) do {} while(0)
struct mutex { int held; };
static struct { struct mutex mutex; uint32_t completed; } output;
#define agds_audio_output (&output)
static struct agds_audio_voice {
 bool loaded, playing, synchronized, phase_active;
 uint32_t last_block;
 char phase_var[16];
} agds_audio_voices[2];
static unsigned notified, phases, irq;
static void lock(struct mutex *m) { assert(!irq && !m->held);m->held=1; }
static void unlock(struct mutex *m) { assert(!irq && m->held);m->held=0; }
static void pcm_lock(void) { assert(!irq);irq=1; }
static void pcm_unlock(void) { assert(irq);irq=0; }
static const struct {
 void (*mutex_lock)(struct mutex *), (*mutex_unlock)(struct mutex *);
 void (*pcm_play_lock)(void), (*pcm_play_unlock)(void);
} api={lock,unlock,pcm_lock,pcm_unlock};
#define rb (&api)
static void scummvm_agds_vm_voice_state(bool playing, bool finished)
{ assert(!irq && !playing && finished);notified++; }
static void scummvm_agds_vm_set_global(const char *name,int value)
{ (void)name;assert(!irq && value==0);phases++; }
''' + function(source, 'audio_update_phases(') + r'''
int main(void)
{
 agds_audio_voices[0]=(struct agds_audio_voice){.loaded=true,
   .playing=true,.synchronized=true,.last_block=19};
 output.completed=18;
 audio_update_phases();assert(!notified);
 agds_audio_voices[0].playing=false;
 audio_update_phases();assert(!notified); /* decoder EOF is too early */
 output.completed=19;
 audio_update_phases();assert(notified==1);
 audio_update_phases();assert(notified==1); /* exactly once */
 agds_audio_voices[1]=(struct agds_audio_voice){.loaded=true,
   .phase_active=true,.last_block=20};
 audio_update_phases();assert(!phases);
 output.completed=20;
 audio_update_phases();assert(phases==1);
 /* Sequence wrap does not complete a future block prematurely. */
 agds_audio_voices[0].synchronized=true;
 agds_audio_voices[0].last_block=0;
 output.completed=UINT32_MAX;
 audio_update_phases();assert(notified==1);
 output.completed=0;
 audio_update_phases();assert(notified==2);
 return 0;
}
''')

    def test_voice_sample_count_preserves_original_duration(self):
        source = (ROOT / 'apps/plugins/scummvm/agds_runtime.c').read_text()
        run_c(r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
struct agds_audio_voice {
 bool playing,have_sample,ambient;
 uint32_t phase,step,loops_left,position,length;
 int16_t last_left,last_right;
};
static bool audio_voice_source_frame(struct agds_audio_voice *v,int16_t *l,int16_t *r)
{ if(v->position==v->length)return false;*l=*r=(int16_t)v->position++;return true; }
static bool audio_voice_rewind(struct agds_audio_voice *v)
{ v->position=0;v->phase=0;return true; }
''' + function(source, 'audio_voice_next(') + r'''
int main(void)
{
 for(unsigned rate=22050;rate<=44100;rate*=2){
  struct agds_audio_voice v={.playing=true,.length=rate,.loops_left=1,
    .step=(rate<<16)/44100};
  int16_t left,right;unsigned frames=0;
  while(audio_voice_next(&v,&left,&right))frames++;
  assert(frames==44100 && v.position==rate && !v.playing);
 }
 return 0;
}
''')

    def test_intro_uses_phase_clock_and_restarts_selected_speaking_clip(self):
        source = (ROOT / 'apps/plugins/scummvm/agds_runtime.c').read_text()
        declaration = source[source.index('struct agds_scene_stream {'):
                             source.index('struct agds_scene_color_stream {')]
        run_c(r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#define HZ 100
#define MAX(a,b) ((a)>(b)?(a):(b))
#define TIME_BEFORE(a,b) ((long)((a)-(b))<0)
#define AGDS_SCENE_PHASE_KEY_MS 150u
#define AGDS_SCENE_MAX_CATCHUP 4u
#define DEBUGF(...) do {} while(0)
struct scummvm_file { int fd; };
''' + declaration + r'''
static struct agds_scene_stream agds_scene;
static unsigned agds_scene_test_speed=1, request, rendered;
static long now;
static int phases[4];
static const struct { long *current_tick; size_t (*strlcpy)(char*,const char*,size_t); }
 api={&now,NULL};
#define rb (&api)
static unsigned scummvm_agds_vm_take_scene_animation(void)
{ unsigned r=request;request=0;return r; }
static void scummvm_file_close(struct scummvm_file *f) { (void)f; }
static bool scene_apply_frame(char *s,size_t n)
{ (void)s;(void)n;rendered++;return true; }
static void scummvm_agds_vm_set_global(const char *name,int value)
{
 const char *names[]={"1122.10e1.11c6","1122.10e1.118b",
  "1122.10e1.118d","1122.10e1.118e"};
 for(unsigned i=0;i<4;i++) if(!strcmp(name,names[i])) phases[i]=value;
}
''' + function(source, 'scene_step(char *status, size_t status_size)\n{') + r'''
int main(void)
{
 agds_scene=(struct agds_scene_stream){.active=true,.version=3,
   .clip=1,.clip_start=6,.frame=6,.clip_end=315,.frame_rate=24,.frame_count=424};
 for(now=0;now<=1288;now++)assert(scene_step(NULL,0));
 assert(rendered==310 && phases[0]==-1 && agds_scene.clip==0);
 now+=1000;scene_step(NULL,0);assert(rendered==310); /* hold, no fake speech */
 request=3;scene_step(NULL,0);
 assert(agds_scene.clip==3 && phases[2]==0 && rendered==311);
 for(unsigned i=0;i<126;i++){now++;scene_step(NULL,0);}
 assert(phases[2]==-1 && agds_scene.clip==0 && rendered==341);
 request=3;scene_step(NULL,0);
 assert(phases[2]==0 && agds_scene.frame==358 && rendered==342);
 assert(phases[1]==0 && phases[3]==0); /* no unsolicited other clips */
 return 0;
}
''')


if __name__ == '__main__':
    unittest.main()
