/***************************************************************************
 * Bounded AGDS 2.509 object/bytecode reader for the NiBiRu runtime.
 ****************************************************************************/

#ifndef SCUMMVM_AGDS_VM_H
#define SCUMMVM_AGDS_VM_H

#include "scummvm.h"
#include "agds_loader.h"

#define AGDS_VM_MAX_CODE 16384
#define AGDS_VM_MAX_STRINGS 160
#define AGDS_VM_MAX_SCREEN_OBJECTS 64
#define AGDS_VM_AUDIO_NAME_SIZE AGDS_ADB_NAME_SIZE
#define AGDS_VM_MAX_REGION_POINTS 32
#define AGDS_VM_MAX_REGION_POLYGONS 4
#define AGDS_VM_INVENTORY_SLOTS 35
#define AGDS_VM_INVENTORY_NAME_SIZE 32
#define AGDS_VM_MAX_USE_OBJECT_HANDLERS 5
#define AGDS_VM_OBJECT_TEXT_SIZE 512
#define AGDS_VM_MAX_FONTS 100

struct scummvm_agds_point {
    int16_t x;
    int16_t y;
};

struct scummvm_agds_region {
    int16_t center_x;
    int16_t center_y;
    uint8_t polygon_count;
    uint8_t point_count;
    uint8_t polygon_start[AGDS_VM_MAX_REGION_POLYGONS];
    uint8_t polygon_size[AGDS_VM_MAX_REGION_POLYGONS];
    struct scummvm_agds_point points[AGDS_VM_MAX_REGION_POINTS];
};

enum scummvm_agds_audio_action {
    AGDS_VM_AUDIO_LOAD,
    AGDS_VM_AUDIO_RESTART,
    AGDS_VM_AUDIO_STOP
};

struct scummvm_agds_audio_event {
    enum scummvm_agds_audio_action action;
    char resource_entry[AGDS_VM_AUDIO_NAME_SIZE];
    char phase_var[AGDS_VM_AUDIO_NAME_SIZE];
    int16_t volume;
    int16_t pan;
    uint16_t cycles;
    bool play_now;
    bool ambient;
    bool synchronized;
};

struct scummvm_agds_film_event {
    char video_name[AGDS_RESOURCE_NAME_SIZE];
    char subtitles_entry[AGDS_VM_AUDIO_NAME_SIZE];
};

struct scummvm_agds_inventory_item {
    char name[AGDS_VM_INVENTORY_NAME_SIZE];
    char picture_entry[AGDS_ADB_NAME_SIZE];
    char text_entry[AGDS_ADB_NAME_SIZE];
    uint32_t count;
    uint16_t picture_tile;
    uint16_t tile_width;
    uint16_t tile_height;
};

struct scummvm_agds_object_text {
    char text[AGDS_VM_OBJECT_TEXT_SIZE];
    int16_t x;
    int16_t y;
    int16_t font_slot;
    int32_t color;
    uint16_t flags;
};

struct scummvm_agds_font {
    char descriptor[AGDS_ADB_NAME_SIZE];
    uint16_t authored_height;
    int32_t primary_color;
    int32_t secondary_color;
    uint16_t flags;
    bool used;
};

struct scummvm_agds_use_object_handler {
    uint16_t target_id;
    uint16_t ip;
};

enum scummvm_agds_character_render_owner {
    AGDS_VM_CHARACTER_RENDER_NONE = 0,
    AGDS_VM_CHARACTER_RENDER_DIRECT_SCENE,
    AGDS_VM_CHARACTER_RENDER_IDLE,
    AGDS_VM_CHARACTER_RENDER_LOCOMOTION,
    AGDS_VM_CHARACTER_RENDER_NAMED_ANIMATION,
    AGDS_VM_CHARACTER_RENDER_MODEL_ANIMATION
};

struct scummvm_agds_screen_object {
    char name[AGDS_ADB_NAME_SIZE];
    char code_entry[AGDS_ADB_NAME_SIZE];
    char picture_entry[AGDS_ADB_NAME_SIZE];
    char alpha_picture_entry[AGDS_ADB_NAME_SIZE];
    char region_entry[AGDS_ADB_NAME_SIZE];
    char character_definition[AGDS_ADB_NAME_SIZE];
    char character_pose_descriptor[AGDS_ADB_NAME_SIZE];
    int16_t x;
    int16_t y;
    int16_t z;
    int32_t animation_field_392;
    int32_t animation_field_396;
    int16_t character_direction;
    int16_t character_phase;
    uint16_t character_pose_frame;
    enum scummvm_agds_character_render_owner character_render_owner;
    int16_t region_offset_x;
    int16_t region_offset_y;
    uint16_t picture_width;
    uint16_t picture_height;
    uint16_t look_handler;
    uint16_t click_handler;
    uint16_t fallback_handler;
    uint16_t user_use_handler;
    uint16_t throw_handler;
    uint16_t use_on_handler;
    uint16_t object_id;
    uint16_t picture_tile;
    uint16_t tile_width;
    uint16_t tile_height;
    uint16_t inventory_type;
    uint8_t use_object_handler_count;
    char inventory_picture_entry[AGDS_ADB_NAME_SIZE];
    char inventory_text_entry[AGDS_ADB_NAME_SIZE];
    struct scummvm_agds_use_object_handler
        use_object_handlers[AGDS_VM_MAX_USE_OBJECT_HANDLERS];
    struct scummvm_agds_region region;
    bool background;
    bool visible;
    bool alive;
    bool ignore_region;
    bool character_model;
};

struct scummvm_agds_scene_state {
    char name[AGDS_ADB_NAME_SIZE];
    uint16_t object_count;
    int16_t camera_pitch;
    int16_t camera_distance;
    int16_t camera_fov;
    int16_t clip_x;
    int16_t clip_y;
    uint16_t clip_width;
    uint16_t clip_height;
    int32_t animation_field_392;
    int32_t animation_field_396;
    int32_t light_ambient_color;
    int32_t light_diffuse_color;
    int32_t light_specular_color;
    int16_t light_x;
    int16_t light_y;
    int16_t light_z;
    int16_t light_index;
    int16_t light_type;
    bool user_enabled;
    bool camera_set;
    bool light_set;
    bool clip_set;
    bool navigation_region_set;
    struct scummvm_agds_region navigation_region;
    struct scummvm_agds_screen_object objects[AGDS_VM_MAX_SCREEN_OBJECTS];
};

struct scummvm_agds_object {
    uint16_t id;
    uint16_t code_size;
    uint16_t opcode_base;
    uint16_t string_table;
    uint16_t string_count;
    uint16_t string_offsets[AGDS_VM_MAX_STRINGS];
    unsigned char code[AGDS_VM_MAX_CODE];
};

bool scummvm_agds_object_load(const struct scummvm_target *target,
                              const char *name,
                              struct scummvm_agds_object *object,
                              char *status, size_t status_size);
bool scummvm_agds_object_string(const struct scummvm_agds_object *object,
                                uint16_t index,
                                char *text, size_t text_size);
bool scummvm_agds_decode_opcode(const struct scummvm_agds_object *object,
                                uint16_t *ip, uint16_t *opcode);
bool scummvm_agds_vm_start_screen(
    const struct scummvm_target *target, const char *name,
    struct scummvm_agds_scene_state *scene,
    char *status, size_t status_size);
bool scummvm_agds_vm_change_screen(
    const char *name, char *status, size_t status_size);
bool scummvm_agds_vm_run_object_named(
    const char *name, char *status, size_t status_size);
bool scummvm_agds_vm_tick(char *status, size_t status_size);
bool scummvm_agds_vm_pointer(
    int16_t x, int16_t y, char *status, size_t status_size);
bool scummvm_agds_vm_click(
    int16_t x, int16_t y, bool look,
    char *status, size_t status_size);
bool scummvm_agds_vm_key(
    const char *key, bool *handled,
    char *status, size_t status_size);
bool scummvm_agds_vm_take_next_screen(char *name, size_t name_size);
bool scummvm_agds_vm_take_quit(void);
bool scummvm_agds_vm_set_global(const char *name, int32_t value);
unsigned scummvm_agds_vm_inventory_count(void);
bool scummvm_agds_vm_inventory_item(
    unsigned index, struct scummvm_agds_inventory_item *item);
bool scummvm_agds_vm_inventory_contains(const char *name);
bool scummvm_agds_vm_inventory_enabled(void);
bool scummvm_agds_vm_inventory_select(unsigned index);
void scummvm_agds_vm_inventory_deselect(void);
bool scummvm_agds_vm_inventory_click(
    unsigned index, bool look, char *status, size_t status_size);
bool scummvm_agds_vm_inventory_use(
    unsigned source_index, unsigned target_index,
    char *status, size_t status_size);
bool scummvm_agds_vm_inventory_use_at(
    unsigned source_index, int16_t x, int16_t y,
    char *status, size_t status_size);
unsigned scummvm_agds_vm_object_text_count(void);
bool scummvm_agds_vm_object_text(
    unsigned index, struct scummvm_agds_object_text *text);
bool scummvm_agds_vm_font(
    unsigned slot, struct scummvm_agds_font *font);
int scummvm_agds_vm_dialog_font(bool npc);
int32_t scummvm_agds_vm_system_value(const char *name);
bool scummvm_agds_vm_take_load_request(int *slot);
bool scummvm_agds_vm_save_game(
    int slot, char *status, size_t status_size);
bool scummvm_agds_vm_load_game(
    int slot, char *screen, size_t screen_size,
    char *status, size_t status_size);
#ifdef SIMULATOR
void scummvm_agds_vm_simulator_set_test_speed(unsigned speed);
void scummvm_agds_vm_simulator_seed_save_state(void);
void scummvm_agds_vm_simulator_scramble_save_state(void);
#endif
bool scummvm_agds_vm_take_audio_event(
    struct scummvm_agds_audio_event *event);
bool scummvm_agds_vm_take_film_event(
    struct scummvm_agds_film_event *event);
void scummvm_agds_vm_film_finished(void);
bool scummvm_agds_vm_dialog_text(
    char *text, size_t text_size, bool *npc);
bool scummvm_agds_vm_dialog_advance(void);
unsigned scummvm_agds_vm_take_scene_animation(void);
void scummvm_agds_vm_voice_state(bool playing, bool finished);
void scummvm_agds_vm_reset(void);

#endif
