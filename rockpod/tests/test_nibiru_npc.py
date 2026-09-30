"""NPC scene binding regression tests without copyrighted game data."""
from rockpod.tests.test_nibiru_audio_queue import ROOT, function
from rockpod.tests.test_nibiru_playability import run_c
import unittest


class NibiruNpcTest(unittest.TestCase):
    def test_only_inserted_model_updates_owner_and_final_pose_is_valid(self):
        source = (ROOT / 'apps/plugins/scummvm/agds_vm.c').read_text()
        run_c(r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#define MIN(a,b) ((a)<(b)?(a):(b))
#define AGDS_VM_CHARACTER_RENDER_MODEL_ANIMATION 5
struct scummvm_agds_screen_object {
    bool character_model;
    int character_render_owner, x, y, z, character_direction;
    char character_pose_descriptor[64];
    unsigned character_pose_frame;
};
struct agds_vm_animation {
    bool model_animation, inserted;
    unsigned object_index, frame, frame_count;
    int x, y, z, yaw;
    char descriptor[64];
};
static struct {
    unsigned object_count;
    struct scummvm_agds_screen_object objects[2];
} scene;
#define vm_scene (&scene)
static size_t copy(char *dst, const char *src, size_t n)
{ assert(strlen(src) < n); strcpy(dst, src); return strlen(src); }
static const struct { size_t (*strlcpy)(char *, const char *, size_t); }
    api = {copy};
#define rb (&api)
''' + function(source, 'vm_sync_model_animation(') + r'''
int main(void)
{
    struct agds_vm_animation a = {.model_animation=true, .object_index=1,
        .frame=3, .frame_count=5, .x=600, .y=450, .z=452, .yaw=230,
        .descriptor="npc.paint"};
    scene.object_count=2;
    vm_sync_model_animation(&a);
    assert(!scene.objects[1].character_model);
    a.inserted=true;
    vm_sync_model_animation(&a);
    assert(scene.objects[1].character_model);
    assert(scene.objects[1].x==600 && scene.objects[1].y==450);
    assert(scene.objects[1].character_direction==230);
    assert(scene.objects[1].character_pose_frame==3);
    assert(!strcmp(scene.objects[1].character_pose_descriptor,"npc.paint"));
    assert(!scene.objects[0].character_model);
    a.frame=5;
    vm_sync_model_animation(&a);
    assert(scene.objects[1].character_pose_frame==4);
    a.object_index=2;
    vm_sync_model_animation(&a);
    return 0;
}
''')

    def test_previous_room_position_stays_hidden_until_entry_positions_actor(self):
        source = (ROOT / 'apps/plugins/scummvm/agds_vm.c').read_text()
        run_c(r'''
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
struct agds_vm_character {
 char object[64], definition[64], active_animation[64];
 int x,y,render_owner,direction,phase,animation_pose_frame;
 bool visible,scene_positioned;
};
struct {
 struct {
  int x,y,character_render_owner,character_direction,character_phase;
  int character_pose_frame;
  bool visible,character_model;
  char character_definition[64],character_pose_descriptor[64];
 } objects[1];
} scene;
#define vm_scene (&scene)
static int vm_find_screen_object(const char *name)
{ return strcmp(name,"actor") ? -1 : 0; }
static size_t copy(char *d,const char *s,size_t n)
{ assert(strlen(s)<n); strcpy(d,s); return strlen(s); }
static const struct { size_t (*strlcpy)(char *,const char *,size_t); }
 api = {copy};
#define rb (&api)
''' + function(source, 'vm_sync_character_object(') + r'''
int main(void)
{
 struct agds_vm_character actor={.object="actor",.definition="idle",
  .visible=true,.x=759,.y=553};
 vm_sync_character_object(&actor);
 assert(!scene.objects[0].visible);
 actor.x=715;actor.y=462;actor.scene_positioned=true;
 vm_sync_character_object(&actor);
 assert(scene.objects[0].visible && scene.objects[0].x==715);
 actor.visible=false;
 vm_sync_character_object(&actor);
 assert(!scene.objects[0].visible);
 return 0;
}
''')
