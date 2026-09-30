/***************************************************************************
 * Clean, allocation-free AGDS 2.509 object parser.
 *
 * The retail executable's dispatch loop encodes one/two-byte opcodes as an
 * odd-tagged value, shifts it right, and uses base 2217. Canonical opcode 5
 * is Enter. Object and resource-table bounds are checked before VM use.
 ****************************************************************************/

#include "agds_vm.h"
#include "agds_loader.h"
#include "rbfile.h"

#define AGDS_OPCODE_ENTER 5u
#define AGDS_ENTER_MAGIC 0xdeadu
#define AGDS_ENTER_SIZE 12u
#define AGDS_RESOURCE_TABLE_BIAS 24u

static uint16_t read_u16le(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32le(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int32_t read_i32le(const unsigned char *p)
{
    return (int32_t)read_u32le(p);
}

bool scummvm_agds_decode_opcode(const struct scummvm_agds_object *object,
                                uint16_t *ip, uint16_t *opcode)
{
    uint16_t encoded;

    if (*ip >= object->code_size)
        return false;
    encoded = object->code[(*ip)++];
    if (encoded & 1u) {
        if (*ip >= object->code_size)
            return false;
        encoded |= (uint16_t)object->code[(*ip)++] << 8;
        encoded >>= 1;
    }
    if (encoded < object->opcode_base)
        return false;
    *opcode = encoded - object->opcode_base;
    return true;
}

bool scummvm_agds_object_load(const struct scummvm_target *target,
                              const char *name,
                              struct scummvm_agds_object *object,
                              char *status, size_t status_size)
{
    struct scummvm_agds_adb_entry entry;
    unsigned char header[7];
    uint16_t data_size;
    uint16_t ip = 0;
    uint16_t opcode;
    uint16_t magic;
    uint16_t enter_size;
    uint16_t resource_offset;
    uint16_t resource_count;
    uint32_t table;
    unsigned index;

    rb->memset(object, 0, sizeof(*object));
    if (!scummvm_agds_find_adb_entry(target, name, &entry,
                                     status, status_size))
        return false;
    if (entry.size < sizeof(header) ||
        scummvm_agds_read_adb_entry(target, &entry, 0, header,
                                    sizeof(header)) !=
            (long)sizeof(header)) {
        rb->snprintf(status, status_size,
                     "AGDS object %.32s is truncated", name);
        return false;
    }
    object->id = read_u16le(header);
    data_size = read_u16le(header + 2);
    object->code_size = read_u16le(header + 4);
    if (data_size != 0 || header[6] != 1 || object->code_size == 0 ||
        object->code_size > AGDS_VM_MAX_CODE ||
        (uint32_t)object->code_size + sizeof(header) > entry.size ||
        scummvm_agds_read_adb_entry(target, &entry, sizeof(header),
                                    object->code,
                                    object->code_size) !=
            (long)object->code_size) {
        rb->snprintf(status, status_size,
                     "AGDS object %.32s header unsupported", name);
        return false;
    }

    if (!(object->code[0] & 1u) || object->code_size < 2) {
        rb->snprintf(status, status_size,
                     "AGDS object %.32s opcode stream unsupported", name);
        return false;
    }
    object->opcode_base = (uint16_t)(((uint16_t)object->code[0] |
        ((uint16_t)object->code[1] << 8)) >> 1) - AGDS_OPCODE_ENTER;
    if (object->opcode_base != 2217u ||
        !scummvm_agds_decode_opcode(object, &ip, &opcode) ||
        opcode != AGDS_OPCODE_ENTER || ip + 4u + AGDS_ENTER_SIZE >
            object->code_size) {
        rb->snprintf(status, status_size,
                     "AGDS object %.32s is not NiBiRu 2.509", name);
        return false;
    }
    magic = read_u16le(object->code + ip);
    enter_size = read_u16le(object->code + ip + 2);
    ip += 4;
    if (magic != AGDS_ENTER_MAGIC || enter_size != AGDS_ENTER_SIZE) {
        rb->snprintf(status, status_size,
                     "AGDS object %.32s Enter invalid", name);
        return false;
    }
    resource_offset = read_u16le(object->code + ip + 6);
    resource_count = read_u16le(object->code + ip + 8);
    if (resource_count == 0) {
        object->string_table = 0;
        object->string_count = 0;
        return true;
    }
    table = (uint32_t)resource_offset + AGDS_RESOURCE_TABLE_BIAS;
    if (resource_count > AGDS_VM_MAX_STRINGS ||
        table + (uint32_t)resource_count * 4u > object->code_size) {
        rb->snprintf(status, status_size,
                     "AGDS object %.32s string table invalid", name);
        return false;
    }
    object->string_table = (uint16_t)table;
    object->string_count = resource_count;
    for (index = 0; index < resource_count; index++) {
        uint16_t relative = read_u16le(object->code + table + index * 4u);
        uint32_t offset = table + relative;

        if (offset >= object->code_size ||
            rb->memchr(object->code + offset, '\0',
                       object->code_size - offset) == NULL) {
            rb->snprintf(status, status_size,
                         "AGDS object %.32s string %u invalid",
                         name, index);
            return false;
        }
        object->string_offsets[index] = (uint16_t)offset;
    }
    return true;
}

bool scummvm_agds_object_string(const struct scummvm_agds_object *object,
                                uint16_t index,
                                char *text, size_t text_size)
{
    const char *source;

    if (!text || text_size == 0 || index >= object->string_count)
        return false;
    source = (const char *)object->code + object->string_offsets[index];
    return rb->strlcpy(text, source, text_size) < text_size;
}

/*
 * Small, bounded AGDS 2.509 process core.  It executes the retail object
 * bytecode and records screen-object state; image/audio backends stay in the
 * Rockbox runtime.  Processes retain only their name, IP and stack while
 * suspended, so a room does not pin one 16 KiB code buffer per object.
 */

#define AGDS_VM_STACK 64
#define AGDS_VM_SHARED 10
#define AGDS_VM_GLOBALS 192
#define AGDS_VM_SUSPENDED 24
#define AGDS_VM_DEPTH 8
#define AGDS_VM_INSTRUCTION_LIMIT 20000u
#define AGDS_VM_AUDIO_EVENTS 16
#define AGDS_VM_MOUSE_AREAS 100
#define AGDS_VM_SYSTEM_VARS 48
#define AGDS_VM_KEY_HANDLERS 96
#define AGDS_VM_KEY_NAME_SIZE 12
#define AGDS_VM_DIALOG_TEXT_SIZE 16384
#define AGDS_VM_DIALOG_LINE_SIZE 1024
#define AGDS_VM_DIALOG_SOUNDS 8
#define AGDS_VM_CHARACTERS 8
#define AGDS_VM_CHARACTER_ANIMATIONS 5
#define AGDS_VM_ANIMATIONS 16
#define AGDS_VM_ANIMATION_CONTROLS 8
#define AGDS_VM_CHARACTER_PATH_POINTS AGDS_VM_MAX_REGION_POINTS
#define AGDS_VM_MOTION_RECORDS AGDS_VM_CHARACTER_ANIMATIONS
#define AGDS_VM_MOTION_SAMPLES 128
#define AGDS_VM_MOTION_DESCRIPTOR_SIZE 32
#define AGDS_VM_SAVE_BUFFER_SIZE 32768
#define AGDS_VM_SAVE_VERSION 1
#define AGDS_VM_PICTURE_HANDLES 64
#define AGDS_VM_PATCH_REFS 64
#define AGDS_VM_OBJECT_TEXTS 32
#define AGDS_VM_ABS(value) ((value) < 0 ? -(value) : (value))

struct agds_vm_process {
    char name[AGDS_ADB_NAME_SIZE];
    char code_entry[AGDS_ADB_NAME_SIZE];
    uint16_t ip;
    uint16_t object_index;
    uint8_t inventory_slot_plus_one;
    int32_t stack[AGDS_VM_STACK];
    uint8_t sp;
    uint16_t timer;
    char phase_var[AGDS_ADB_NAME_SIZE];
    int16_t sample_volume;
    int16_t sample_pan;
    int16_t animation_x;
    int16_t animation_y;
    int16_t model_yaw;
    int16_t animation_z;
    bool model_animation;
    int16_t tile_width;
    int16_t tile_height;
    int32_t text_color;
    int32_t text_secondary_color;
    int32_t light_ambient_color;
    int32_t light_diffuse_color;
    int32_t light_specular_color;
    int16_t light_x;
    int16_t light_y;
    int16_t light_z;
    uint16_t text_flags;
    int16_t film_subtitles_resource;
    uint16_t cycles;
    char waiting_character[AGDS_ADB_NAME_SIZE];
    bool phase_controlled;
    bool sample_ambient;
    bool waiting_dialog;
    bool waiting_tell;
    bool waiting_film;
    bool survive_removal;
};

struct agds_vm_object_text_slot {
    struct scummvm_agds_object_text overlay;
    uint8_t object_index;
    bool used;
};

struct agds_vm_global {
    char name[AGDS_ADB_NAME_SIZE];
    int32_t value;
    bool used;
};

struct agds_vm_inventory_slot {
    char name[AGDS_VM_INVENTORY_NAME_SIZE];
    uint32_t count;
    struct scummvm_agds_screen_object object;
    bool used;
};

struct agds_vm_picture_handle {
    char descriptor[AGDS_ADB_NAME_SIZE];
    bool used;
};

struct agds_vm_patch_ref {
    char screen[AGDS_ADB_NAME_SIZE];
    char object[AGDS_ADB_NAME_SIZE];
    int16_t refs;
    bool used;
};

struct agds_vm_mouse_area {
    struct scummvm_agds_region region;
    char on_enter[AGDS_ADB_NAME_SIZE];
    char on_leave[AGDS_ADB_NAME_SIZE];
    bool used;
    bool enabled;
    bool visible;
};

struct agds_vm_key_handler {
    char key[AGDS_VM_KEY_NAME_SIZE];
    uint16_t object_index;
    uint16_t ip;
    bool used;
};

struct agds_vm_dialog_sound {
    char actor[AGDS_RESOURCE_NAME_SIZE];
    char sample[AGDS_RESOURCE_NAME_SIZE];
    int16_t step;
};

struct agds_vm_dialog_state {
    char owner[AGDS_ADB_NAME_SIZE];
    char script[AGDS_VM_DIALOG_TEXT_SIZE];
    char definitions[AGDS_VM_DIALOG_TEXT_SIZE];
    char line[AGDS_VM_DIALOG_LINE_SIZE];
    char current_definition[AGDS_RESOURCE_NAME_SIZE];
    size_t position;
    int8_t current_sound;
    uint8_t sound_count;
    bool active;
    bool text_visible;
    bool text_npc;
    struct agds_vm_dialog_sound sounds[AGDS_VM_DIALOG_SOUNDS];
};

struct agds_vm_character {
    char name[AGDS_ADB_NAME_SIZE];
    char definition[AGDS_ADB_NAME_SIZE];
    char object[AGDS_ADB_NAME_SIZE];
    char animations[AGDS_VM_CHARACTER_ANIMATIONS][AGDS_ADB_NAME_SIZE];
    char active_animation[AGDS_ADB_NAME_SIZE];
    int16_t x;
    int16_t y;
    int16_t direction;
    int16_t phase;
    uint16_t animation_frames;
    uint16_t phase_ticks;
    uint16_t phase_period_ticks;
    uint32_t animation_elapsed_ticks;
    uint16_t animation_pose_frame;
    int32_t position_x_q16;
    int32_t position_y_q16;
    int16_t final_direction;
    uint16_t locomotion_ticks;
    uint16_t locomotion_pose_frame;
    uint8_t locomotion_animation;
    uint8_t path_count;
    uint8_t path_index;
    uint8_t last_stride_animation;
    bool animation_controls[AGDS_VM_ANIMATION_CONTROLS];
    struct scummvm_agds_point path[AGDS_VM_CHARACTER_PATH_POINTS];
    enum scummvm_agds_character_render_owner render_owner;
    bool used;
    bool enabled;
    bool visible;
    bool moving;
    bool scene_positioned;
    bool leaving;
    bool stopping;
};

/* Retail AGDS keeps loaded screen animations in a scene list and associates
 * each one with its phase variable.  Preserve the authored timing and the
 * script-visible control fields independently of the eventual video
 * compositor; VM flow must not depend on a guessed completion delay. */
struct agds_vm_animation {
    char descriptor[AGDS_ADB_NAME_SIZE];
    char resource[AGDS_RESOURCE_NAME_SIZE];
    char phase_var[AGDS_ADB_NAME_SIZE];
    uint16_t frame_count;
    uint16_t frame;
    uint32_t period_scaled;
    uint32_t phase_accumulator;
    int16_t x;
    int16_t y;
    int32_t field_38a;
    int32_t field_38e;
    int32_t field_392;
    int32_t field_396;
    int32_t field_3ae;
    bool controls[AGDS_VM_ANIMATION_CONTROLS];
    bool used;
    bool active;
    bool phase_controlled;
    bool model_animation;
    bool inserted;
    uint16_t object_index;
    int16_t yaw;
    int16_t z;
};

struct agds_vm_motion_record {
    char descriptor[AGDS_VM_MOTION_DESCRIPTOR_SIZE];
    uint16_t first_sample;
    uint16_t sample_count;
};

struct agds_vm_motion_data {
    struct agds_vm_motion_record records[AGDS_VM_MOTION_RECORDS];
    int32_t root_x_q16[AGDS_VM_MOTION_SAMPLES];
    int32_t root_z_q16[AGDS_VM_MOTION_SAMPLES];
    uint16_t record_count;
    uint16_t sample_count;
    uint16_t pose_fps;
    bool loaded;
};

struct agds_vm_saved_character {
    char name[AGDS_ADB_NAME_SIZE];
    int16_t x;
    int16_t y;
    int16_t direction;
    bool enabled;
    bool visible;
};

struct agds_vm_saved_object {
    char name[AGDS_ADB_NAME_SIZE];
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t character_direction;
    int16_t character_phase;
    uint16_t character_pose_frame;
    uint8_t character_render_owner;
    bool visible;
    bool alive;
};

struct agds_vm_save_cursor {
    unsigned char *data;
    size_t position;
    size_t size;
    bool valid;
};

static const struct scummvm_target *vm_target;
static long vm_last_tick;
static struct scummvm_agds_scene_state *vm_scene;
static struct scummvm_agds_object vm_objects[AGDS_VM_DEPTH];
static struct agds_vm_process vm_suspended[AGDS_VM_SUSPENDED];
static struct agds_vm_process vm_pending[AGDS_VM_SUSPENDED];
static uint8_t vm_suspended_count;
static char vm_shared[AGDS_VM_SHARED][AGDS_ADB_NAME_SIZE];
static int8_t vm_shared_index;
static struct agds_vm_global vm_globals[AGDS_VM_GLOBALS];
static struct agds_vm_global vm_system_vars[AGDS_VM_SYSTEM_VARS];
static struct agds_vm_inventory_slot
    vm_inventory[AGDS_VM_INVENTORY_SLOTS];
static struct agds_vm_picture_handle
    vm_picture_handles[AGDS_VM_PICTURE_HANDLES];
static struct agds_vm_patch_ref vm_patch_refs[AGDS_VM_PATCH_REFS];
static struct scummvm_agds_audio_event vm_audio_events[AGDS_VM_AUDIO_EVENTS];
static struct scummvm_agds_film_event vm_film_event;
static bool vm_film_pending;
static bool vm_film_active;
static bool vm_tell_voice;
static unsigned vm_scene_animation_request;
static uint8_t vm_audio_read;
static uint8_t vm_audio_count;
static struct agds_vm_mouse_area vm_mouse_areas[AGDS_VM_MOUSE_AREAS];
static struct agds_vm_key_handler vm_key_handlers[AGDS_VM_KEY_HANDLERS];
static struct agds_vm_dialog_state vm_dialog;
static struct agds_vm_character vm_characters[AGDS_VM_CHARACTERS];
static struct agds_vm_animation vm_animations[AGDS_VM_ANIMATIONS];
static struct agds_vm_object_text_slot
    vm_object_texts[AGDS_VM_OBJECT_TEXTS];
static struct scummvm_agds_font vm_fonts[AGDS_VM_MAX_FONTS];
static unsigned char vm_animation_probe_header[512];
static struct agds_vm_motion_data vm_motion;
static unsigned char vm_save_buffer[AGDS_VM_SAVE_BUFFER_SIZE];
static struct agds_vm_saved_character
    vm_saved_characters[AGDS_VM_CHARACTERS];
static struct agds_vm_saved_object
    vm_saved_objects[AGDS_VM_MAX_SCREEN_OBJECTS];
static uint8_t vm_saved_character_count;
static uint8_t vm_saved_object_count;
static int8_t vm_load_request;
static bool vm_restore_pending;
static char vm_dialog_npc_notify[AGDS_ADB_NAME_SIZE];
static char vm_dialog_character_notify[AGDS_ADB_NAME_SIZE];
static char vm_dialog_direction_notify[AGDS_ADB_NAME_SIZE];
static bool vm_mouse_disabled;
static bool vm_inventory_enabled;
static int8_t vm_current_inventory_slot;
static int16_t vm_saved_mouse_x;
static int16_t vm_saved_mouse_y;
static char vm_next_screen[AGDS_ADB_NAME_SIZE];
static char vm_previous_screen[AGDS_ADB_NAME_SIZE];
static bool vm_quit_requested;
static uint16_t vm_intro_debug_ip;
static uint16_t vm_save_picture_width;
static uint16_t vm_save_picture_height;
#ifdef SIMULATOR
static unsigned vm_test_speed = 1;
#endif

static struct agds_vm_global *vm_find_system(const char *name, bool create);
static bool vm_pop(struct agds_vm_process *process, int32_t *value,
                   char *status, size_t status_size);
static bool vm_string(const struct scummvm_agds_object *object, int32_t id,
                      char *text, size_t text_size);
static bool vm_queue_audio(enum scummvm_agds_audio_action action,
                           const char *resource_entry,
                           const struct agds_vm_process *process,
                           bool play_now, bool synchronized,
                           char *status, size_t status_size);
static bool vm_suspend(const struct agds_vm_process *process,
                       char *status, size_t status_size);
static bool vm_run_inventory_object(unsigned slot, uint16_t ip,
                                    unsigned depth,
                                    char *status, size_t status_size);
static bool vm_run_object(const char *name, unsigned depth,
                          char *status, size_t status_size);
static bool vm_character_animation_frames(const char *descriptor,
                                          uint16_t *frames,
                                          uint16_t *period_ticks,
                                          char *status,
                                          size_t status_size);

static struct agds_vm_patch_ref *vm_find_patch_ref(
    const char *screen, const char *object, bool create)
{
    struct agds_vm_patch_ref *empty = NULL;
    unsigned index;

    for (index = 0; index < AGDS_VM_PATCH_REFS; index++) {
        struct agds_vm_patch_ref *patch = &vm_patch_refs[index];

        if (!patch->used) {
            if (empty == NULL)
                empty = patch;
            continue;
        }
        if (!rb->strcmp(patch->screen, screen) &&
            !rb->strcmp(patch->object, object))
            return patch;
    }
    if (!create || empty == NULL)
        return NULL;
    rb->memset(empty, 0, sizeof(*empty));
    rb->strlcpy(empty->screen, screen, sizeof(empty->screen));
    rb->strlcpy(empty->object, object, sizeof(empty->object));
    empty->used = true;
    return empty;
}

static void save_put(struct agds_vm_save_cursor *cursor,
                     const void *data, size_t size)
{
    if (!cursor->valid || size > cursor->size - cursor->position) {
        cursor->valid = false;
        return;
    }
    rb->memcpy(cursor->data + cursor->position, data, size);
    cursor->position += size;
}

static void save_put_u8(struct agds_vm_save_cursor *cursor, uint8_t value)
{
    save_put(cursor, &value, 1);
}

static void save_put_u16(struct agds_vm_save_cursor *cursor, uint16_t value)
{
    unsigned char data[2];

    data[0] = (unsigned char)value;
    data[1] = (unsigned char)(value >> 8);
    save_put(cursor, data, sizeof(data));
}

static void save_put_u32(struct agds_vm_save_cursor *cursor, uint32_t value)
{
    unsigned char data[4];

    data[0] = (unsigned char)value;
    data[1] = (unsigned char)(value >> 8);
    data[2] = (unsigned char)(value >> 16);
    data[3] = (unsigned char)(value >> 24);
    save_put(cursor, data, sizeof(data));
}

static void save_put_string(struct agds_vm_save_cursor *cursor,
                            const char *text, size_t size)
{
    unsigned char *destination;

    if (!cursor->valid || size > cursor->size - cursor->position) {
        cursor->valid = false;
        return;
    }
    destination = cursor->data + cursor->position;
    rb->memset(destination, 0, size);
    rb->strlcpy((char *)destination, text, size);
    cursor->position += size;
}

static void save_get(struct agds_vm_save_cursor *cursor,
                     void *data, size_t size)
{
    if (!cursor->valid || size > cursor->size - cursor->position) {
        cursor->valid = false;
        return;
    }
    rb->memcpy(data, cursor->data + cursor->position, size);
    cursor->position += size;
}

static uint8_t save_get_u8(struct agds_vm_save_cursor *cursor)
{
    uint8_t value = 0;

    save_get(cursor, &value, 1);
    return value;
}

static uint16_t save_get_u16(struct agds_vm_save_cursor *cursor)
{
    unsigned char data[2] = {0, 0};

    save_get(cursor, data, sizeof(data));
    return read_u16le(data);
}

static uint32_t save_get_u32(struct agds_vm_save_cursor *cursor)
{
    unsigned char data[4] = {0, 0, 0, 0};

    save_get(cursor, data, sizeof(data));
    return read_u32le(data);
}

static void save_get_string(struct agds_vm_save_cursor *cursor,
                            char *text, size_t size)
{
    save_get(cursor, text, size);
    if (cursor->valid && rb->memchr(text, '\0', size) == NULL)
        cursor->valid = false;
    if (size > 0)
        text[size - 1] = '\0';
}

static uint32_t save_checksum(const unsigned char *data, size_t size)
{
    uint32_t checksum = 2166136261u;
    size_t index;

    for (index = 0; index < size; index++) {
        checksum ^= data[index];
        checksum *= 16777619u;
    }
    return checksum;
}

static int vm_inventory_find(const char *name)
{
    unsigned index;

    for (index = 0; index < AGDS_VM_INVENTORY_SLOTS; index++) {
        if (vm_inventory[index].used &&
            !rb->strcasecmp(vm_inventory[index].name, name))
            return (int)index;
    }
    return -1;
}

static int vm_picture_handle_add(const char *descriptor)
{
    unsigned index;
    int empty = -1;

    for (index = 0; index < AGDS_VM_PICTURE_HANDLES; index++) {
        if (!vm_picture_handles[index].used) {
            if (empty < 0)
                empty = (int)index;
            continue;
        }
        if (!rb->strcasecmp(vm_picture_handles[index].descriptor,
                            descriptor))
            return (int)index + 1;
    }
    if (empty < 0)
        return 0;
    rb->strlcpy(vm_picture_handles[empty].descriptor, descriptor,
                sizeof(vm_picture_handles[empty].descriptor));
    vm_picture_handles[empty].used = true;
    return empty + 1;
}

static const char *vm_picture_handle_descriptor(int32_t handle)
{
    if (handle <= 0 || handle > AGDS_VM_PICTURE_HANDLES ||
        !vm_picture_handles[handle - 1].used)
        return NULL;
    return vm_picture_handles[handle - 1].descriptor;
}

static bool vm_inventory_add(const char *name, unsigned depth,
                             char *status, size_t status_size)
{
    int slot = vm_inventory_find(name);
    unsigned index;

    if (slot >= 0) {
        if (vm_inventory[slot].count != UINT32_MAX)
            vm_inventory[slot].count++;
        DEBUGF("agds: inventory add %s slot=%d count=%lu\n",
               name, slot, (unsigned long)vm_inventory[slot].count);
        return true;
    }
    for (index = 0; index < AGDS_VM_INVENTORY_SLOTS; index++) {
        if (vm_inventory[index].used)
            continue;
        rb->memset(&vm_inventory[index], 0, sizeof(vm_inventory[index]));
        rb->strlcpy(vm_inventory[index].name, name,
                    sizeof(vm_inventory[index].name));
        vm_inventory[index].count = 1;
        vm_inventory[index].used = true;
        rb->strlcpy(vm_inventory[index].object.name, name,
                    sizeof(vm_inventory[index].object.name));
        rb->strlcpy(vm_inventory[index].object.code_entry, name,
                    sizeof(vm_inventory[index].object.code_entry));
        vm_inventory[index].object.alive = true;
        vm_inventory[index].object.visible = true;
        if (!vm_run_inventory_object(index, 0, depth,
                                     status, status_size)) {
            rb->memset(&vm_inventory[index], 0,
                       sizeof(vm_inventory[index]));
            return false;
        }
        DEBUGF("agds: inventory add %s slot=%u count=1\n",
               name, index);
        return true;
    }
    rb->strlcpy(status, "AGDS inventory slot limit reached", status_size);
    return false;
}

static void vm_inventory_remove(const char *name)
{
    int slot = vm_inventory_find(name);

    if (slot < 0) {
        DEBUGF("agds: cannot remove absent inventory object %s\n", name);
        return;
    }
    if (vm_inventory[slot].count > 1) {
        vm_inventory[slot].count--;
        DEBUGF("agds: inventory remove %s slot=%d count=%lu\n",
               name, slot, (unsigned long)vm_inventory[slot].count);
        return;
    }
    DEBUGF("agds: inventory remove %s slot=%d count=0\n", name, slot);
    rb->memset(&vm_inventory[slot], 0, sizeof(vm_inventory[slot]));
}

static struct agds_vm_character *vm_find_character(const char *name,
                                                    bool create)
{
    struct agds_vm_character *empty = NULL;
    unsigned index;

    for (index = 0; index < AGDS_VM_CHARACTERS; index++) {
        struct agds_vm_character *character = &vm_characters[index];

        if (character->used && !rb->strcasecmp(character->name, name))
            return character;
        if (!character->used && empty == NULL)
            empty = character;
    }
    if (!create || empty == NULL)
        return NULL;
    rb->memset(empty, 0, sizeof(*empty));
    rb->strlcpy(empty->name, name, sizeof(empty->name));
    empty->used = true;
    empty->enabled = true;
    empty->phase = -1;
    empty->render_owner = AGDS_VM_CHARACTER_RENDER_IDLE;
    return empty;
}

static struct agds_vm_animation *vm_find_animation(const char *phase_var,
                                                    bool create)
{
    struct agds_vm_animation *empty = NULL;
    unsigned index;

    if (phase_var == NULL || phase_var[0] == '\0')
        return NULL;
    for (index = 0; index < AGDS_VM_ANIMATIONS; index++) {
        struct agds_vm_animation *animation = &vm_animations[index];

        if (animation->used &&
            !rb->strcasecmp(animation->phase_var, phase_var))
            return animation;
        if (!animation->used && empty == NULL)
            empty = animation;
    }
    if (!create || empty == NULL)
        return NULL;
    rb->memset(empty, 0, sizeof(*empty));
    rb->strlcpy(empty->phase_var, phase_var, sizeof(empty->phase_var));
    empty->used = true;
    return empty;
}

/* Keep a loaded NPC animation attached to its script object.  Only the
 * inserted animation supplies pixels; other loaded phases remain available
 * for script control without painting multiple copies of the same actor. */
static void vm_sync_model_animation(struct agds_vm_animation *animation)
{
    struct scummvm_agds_screen_object *object;
    uint32_t frame;

    if (!animation->model_animation || !animation->inserted ||
        animation->object_index >= vm_scene->object_count)
        return;
    object = &vm_scene->objects[animation->object_index];
    object->character_model = true;
    object->character_render_owner = AGDS_VM_CHARACTER_RENDER_MODEL_ANIMATION;
    object->x = animation->x;
    object->y = animation->y;
    object->z = animation->z;
    object->character_direction = animation->yaw;
    rb->strlcpy(object->character_pose_descriptor, animation->descriptor,
                sizeof(object->character_pose_descriptor));
    frame = MIN(animation->frame, animation->frame_count - 1u);
    object->character_pose_frame = frame;
}

static bool vm_probe_animation(const char *descriptor,
                               struct agds_vm_animation *animation,
                               char *status, size_t status_size)
{
    struct scummvm_agds_resource resource;
    char resource_name[AGDS_RESOURCE_NAME_SIZE];
    size_t length;
    uint32_t read_size;
    uint32_t frame_count = 0;
    uint32_t period_us = 1000000u / 24u;
    uint32_t index;

    if (!scummvm_agds_read_text(vm_target, descriptor,
                                resource_name, sizeof(resource_name),
                                status, status_size))
        return false;
    length = rb->strlen(resource_name);
    while (length > 0) {
        unsigned char last = (unsigned char)resource_name[length - 1];

        if ((last >= 'a' && last <= 'z') ||
            (last >= 'A' && last <= 'Z') ||
            (last >= '0' && last <= '9') || last == '.' ||
            last == '_' || last == '-')
            break;
        resource_name[--length] = '\0';
    }
    if (length == 0)
        return false;
    if (!scummvm_agds_find_resource(vm_target, resource_name, &resource,
                                    status, status_size)) {
        uint16_t model_frames;
        uint16_t model_period_ticks;

        /* The same retail instruction also loads model-backed X animation
         * descriptors.  Their authored key count comes from models.adb. */
        if (!vm_character_animation_frames(
                descriptor, &model_frames, &model_period_ticks,
                status, status_size)) {
            /* A scene-level X animation can contain several authored model
             * tracks and therefore has no single character phase count.  It
             * is advanced by the model scene compositor (room 1864 is the
             * first retail example), while this binding preserves its phase
             * variable and script controls. */
            if (length < 2u ||
                rb->strcasecmp(resource_name + length - 2u, ".x"))
                return false;
            rb->strlcpy(animation->descriptor, descriptor,
                        sizeof(animation->descriptor));
            rb->strlcpy(animation->resource, resource_name,
                        sizeof(animation->resource));
            animation->period_scaled = 1000000u;
            DEBUGF("agds: scene model animation %s -> %s compositor-owned\n",
                   descriptor, animation->resource);
            return true;
        }
        rb->strlcpy(animation->descriptor, descriptor,
                    sizeof(animation->descriptor));
        rb->strlcpy(animation->resource, resource_name,
                    sizeof(animation->resource));
        if (!rb->strncmp(resource_name, "martin_intro_", 13)) {
            /* The direct scene owns these phases and script-selected clips. */
            animation->frame_count = 0;
            return true;
        }
        animation->frame_count = model_frames;
        animation->period_scaled =
            (uint32_t)model_period_ticks * 1000000u;
        DEBUGF("agds: model animation %s -> %s (%u phases)\n",
               descriptor, animation->resource,
               (unsigned)animation->frame_count);
        return true;
    }
    read_size = MIN(resource.size,
                    (uint32_t)sizeof(vm_animation_probe_header));
    if (read_size < 20u ||
        scummvm_agds_read_resource(vm_target, &resource, 0,
                                   vm_animation_probe_header,
                                   read_size) != (long)read_size)
        goto invalid;

    if (read_size >= 32u &&
        !rb->memcmp(vm_animation_probe_header, "RIFF", 4) &&
        !rb->memcmp(vm_animation_probe_header + 8, "AVI ", 4)) {
        for (index = 12u; index + 32u <= read_size; index++) {
            uint32_t chunk_size;

            if (rb->memcmp(vm_animation_probe_header + index, "avih", 4))
                continue;
            chunk_size = read_u32le(vm_animation_probe_header + index + 4);
            if (chunk_size < 40u || index + 8u + chunk_size > read_size)
                goto invalid;
            period_us = read_u32le(vm_animation_probe_header + index + 8);
            frame_count = read_u32le(
                vm_animation_probe_header + index + 24);
            break;
        }
    } else {
        uint16_t magic = read_u16le(vm_animation_probe_header + 4);

        if (magic == 0xaf11u || magic == 0xaf12u) {
            frame_count = read_u16le(vm_animation_probe_header + 6);
            if (magic == 0xaf11u) {
                uint32_t delay = read_u16le(
                    vm_animation_probe_header + 16);
                period_us = delay != 0 ? delay * 1000000u / 70u :
                    1000000u / 24u;
            } else {
                uint32_t delay_ms = read_u32le(
                    vm_animation_probe_header + 16);
                period_us = delay_ms != 0 ? delay_ms * 1000u :
                    1000000u / 24u;
            }
        }
    }
    if (frame_count == 0 || frame_count > UINT16_MAX || period_us == 0)
        goto invalid;
    rb->strlcpy(animation->descriptor, descriptor,
                sizeof(animation->descriptor));
    rb->strlcpy(animation->resource, resource_name,
                sizeof(animation->resource));
    animation->frame_count = (uint16_t)frame_count;
    animation->period_scaled = period_us * (uint32_t)HZ;
    DEBUGF("agds: animation %s -> %s (%u phases, %lu us)\n",
           descriptor, animation->resource,
           (unsigned)animation->frame_count, (unsigned long)period_us);
    return true;

invalid:
    rb->snprintf(status, status_size,
                 "AGDS animation %.24s timing invalid", resource_name);
    return false;
}

static const struct agds_vm_motion_record *vm_find_motion_record(
    const char *descriptor)
{
    unsigned index;

    for (index = 0; index < vm_motion.record_count; index++) {
        if (!rb->strcmp(vm_motion.records[index].descriptor, descriptor))
            return &vm_motion.records[index];
    }
    return NULL;
}

static bool vm_load_character_motion(char *status, size_t status_size)
{
    struct scummvm_file file;
    unsigned char data[12u +
        AGDS_VM_MOTION_RECORDS * (AGDS_VM_MOTION_DESCRIPTOR_SIZE + 4u) +
        AGDS_VM_MOTION_SAMPLES * 8u];
    long size;
    uint16_t record_count;
    uint16_t sample_count;
    uint16_t pose_fps;
    size_t position;
    unsigned index;

    if (vm_motion.loaded)
        return true;
    file.fd = -1;
    if (!scummvm_file_open_game(
            &file, vm_target,
            "rockpod/characters-hq/owned-character-motion.ncm")) {
        rb->strlcpy(status, "NiBiRu owned character motion missing",
                    status_size);
        return false;
    }
    size = scummvm_file_size(&file);
    if (size < 12 || size > (long)sizeof(data) ||
        scummvm_file_read(&file, data, size) != size) {
        scummvm_file_close(&file);
        rb->strlcpy(status, "NiBiRu owned character motion invalid",
                    status_size);
        return false;
    }
    scummvm_file_close(&file);
    if (rb->memcmp(data, "NCM1", 4) || read_u16le(data + 4) != 1) {
        rb->strlcpy(status, "NiBiRu owned character motion header invalid",
                    status_size);
        return false;
    }
    pose_fps = read_u16le(data + 6);
    record_count = read_u16le(data + 8);
    sample_count = read_u16le(data + 10);
    if (pose_fps != 24 || record_count == 0 ||
        record_count > AGDS_VM_MOTION_RECORDS || sample_count == 0 ||
        sample_count > AGDS_VM_MOTION_SAMPLES ||
        12u + (size_t)record_count * 36u +
            (size_t)sample_count * 8u != (size_t)size) {
        rb->strlcpy(status, "NiBiRu owned character motion bounds invalid",
                    status_size);
        return false;
    }
    rb->memset(&vm_motion, 0, sizeof(vm_motion));
    vm_motion.record_count = record_count;
    vm_motion.sample_count = sample_count;
    vm_motion.pose_fps = pose_fps;
    position = 12;
    for (index = 0; index < record_count; index++) {
        struct agds_vm_motion_record *record = &vm_motion.records[index];

        rb->memcpy(record->descriptor, data + position,
                   AGDS_VM_MOTION_DESCRIPTOR_SIZE);
        record->descriptor[AGDS_VM_MOTION_DESCRIPTOR_SIZE - 1] = '\0';
        record->first_sample = read_u16le(data + position + 32);
        record->sample_count = read_u16le(data + position + 34);
        if (record->sample_count == 0 ||
            (uint32_t)record->first_sample + record->sample_count >
                sample_count) {
            rb->strlcpy(status,
                        "NiBiRu owned character motion record invalid",
                        status_size);
            rb->memset(&vm_motion, 0, sizeof(vm_motion));
            return false;
        }
        position += 36;
    }
    for (index = 0; index < sample_count; index++) {
        vm_motion.root_x_q16[index] = read_i32le(data + position);
        vm_motion.root_z_q16[index] = read_i32le(data + position + 4);
        position += 8;
    }
    vm_motion.loaded = true;
    DEBUGF("agds: loaded %u retail motion clips, %u samples at %u fps\n",
           (unsigned)record_count, (unsigned)sample_count,
           (unsigned)pose_fps);
    return true;
}

static bool vm_dialog_set_var(int32_t value)
{
    struct agds_vm_global *variable = vm_find_system("dialog_var", true);

    if (variable == NULL)
        return false;
    variable->value = value;
    return true;
}

static bool vm_dialog_find_definition(const char *name, int32_t *value)
{
    const char *cursor = vm_dialog.definitions;

    while (*cursor != '\0') {
        char definition[AGDS_RESOURCE_NAME_SIZE];
        char number[16];
        size_t definition_size = 0;
        size_t number_size = 0;

        while (*cursor == ' ' || *cursor == '\t')
            cursor++;
        while (*cursor != '\0' && *cursor != '=' && *cursor != '\r' &&
               *cursor != '\n') {
            if (*cursor != ' ' && *cursor != '\t' &&
                definition_size + 1 < sizeof(definition))
                definition[definition_size++] = *cursor;
            cursor++;
        }
        definition[definition_size] = '\0';
        if (*cursor == '=') {
            cursor++;
            while (*cursor == ' ' || *cursor == '\t')
                cursor++;
            while (*cursor != '\0' && *cursor != '\r' &&
                   *cursor != '\n' && *cursor != ';') {
                if (*cursor != ' ' && *cursor != '\t' &&
                    number_size + 1 < sizeof(number))
                    number[number_size++] = *cursor;
                cursor++;
            }
        }
        number[number_size] = '\0';
        while (*cursor != '\0' && *cursor != '\r' && *cursor != '\n')
            cursor++;
        while (*cursor == '\r' || *cursor == '\n')
            cursor++;
        if (definition[0] != '\0' && number[0] != '\0' &&
            !rb->strcasecmp(definition, name)) {
            *value = rb->atoi(number);
            return true;
        }
    }
    return false;
}

static void vm_dialog_parse_sound(const char *directive)
{
    struct agds_vm_dialog_sound *sound;
    const char *cursor = directive;
    const char *end;
    size_t size;

    if (vm_dialog.sound_count >= AGDS_VM_DIALOG_SOUNDS)
        return;
    while (*cursor != '\0' && *cursor != '(')
        cursor++;
    if (*cursor++ != '(')
        return;
    sound = &vm_dialog.sounds[vm_dialog.sound_count];
    rb->memset(sound, 0, sizeof(*sound));
    while (*cursor == ' ' || *cursor == '\t')
        cursor++;
    end = cursor;
    while (*end != '\0' && *end != ',')
        end++;
    if (*end != ',')
        return;
    size = (size_t)(end - cursor);
    while (size > 0 && (cursor[size - 1] == ' ' || cursor[size - 1] == '\t'))
        size--;
    if (size == 0 || size >= sizeof(sound->actor))
        return;
    rb->memcpy(sound->actor, cursor, size);
    sound->actor[size] = '\0';
    cursor = end + 1;
    while (*cursor == ' ' || *cursor == '\t')
        cursor++;
    end = cursor;
    while (*end != '\0' && *end != ',')
        end++;
    if (*end != ',')
        return;
    size = (size_t)(end - cursor);
    while (size > 0 && (cursor[size - 1] == ' ' || cursor[size - 1] == '\t'))
        size--;
    if (size == 0 || size >= sizeof(sound->sample))
        return;
    rb->memcpy(sound->sample, cursor, size);
    sound->sample[size] = '\0';
    cursor = end + 1;
    while (*cursor == ' ' || *cursor == '\t')
        cursor++;
    sound->step = (int16_t)rb->atoi(cursor);
    vm_dialog.sound_count++;
}

static bool vm_dialog_load(const char *owner, const char *script,
                           const char *definitions,
                           char *status, size_t status_size)
{
    rb->memset(&vm_dialog, 0, sizeof(vm_dialog));
    vm_dialog.current_sound = -1;
    if (!scummvm_agds_read_text(vm_target, script,
                                vm_dialog.script, sizeof(vm_dialog.script),
                                status, status_size) ||
        !scummvm_agds_read_text(vm_target, definitions,
                                vm_dialog.definitions,
                                sizeof(vm_dialog.definitions),
                                status, status_size)) {
        rb->memset(&vm_dialog, 0, sizeof(vm_dialog));
        return false;
    }
    rb->strlcpy(vm_dialog.owner, owner, sizeof(vm_dialog.owner));
    vm_dialog.active = true;
    if (!vm_dialog_set_var(-1)) {
        rb->strlcpy(status, "AGDS dialog variable limit reached", status_size);
        rb->memset(&vm_dialog, 0, sizeof(vm_dialog));
        return false;
    }
    return true;
}

static bool vm_dialog_tick(char *status, size_t status_size)
{
    struct agds_vm_global *variable;
    const char *script;
    size_t script_size;
    size_t line_size = 0;
    bool command;

    if (!vm_dialog.active)
        return true;
    variable = vm_find_system("dialog_var", true);
    if (variable == NULL) {
        rb->strlcpy(status, "AGDS dialog variable limit reached", status_size);
        return false;
    }
    if (variable->value != 0)
        return true;
    script = vm_dialog.script;
    script_size = rb->strlen(script);
    if (vm_dialog.position >= script_size) {
        vm_dialog.active = false;
        variable->value = -2;
        DEBUGF("agds: dialog complete for %s\n", vm_dialog.owner);
        return true;
    }
    command = script[vm_dialog.position] == '@';
    while (vm_dialog.position < script_size) {
        if (!command && script[vm_dialog.position] == '@')
            break;
        while (vm_dialog.position < script_size &&
               script[vm_dialog.position] != '\r' &&
               script[vm_dialog.position] != '\n') {
            if (line_size + 1 >= sizeof(vm_dialog.line)) {
                rb->strlcpy(status, "AGDS dialog line is too long", status_size);
                return false;
            }
            vm_dialog.line[line_size++] = script[vm_dialog.position++];
        }
        if (!command) {
            if (line_size + 1 >= sizeof(vm_dialog.line)) {
                rb->strlcpy(status, "AGDS dialog line is too long", status_size);
                return false;
            }
            vm_dialog.line[line_size++] = '\n';
        }
        while (vm_dialog.position < script_size &&
               (script[vm_dialog.position] == '\r' ||
                script[vm_dialog.position] == '\n'))
            vm_dialog.position++;
        if (command)
            break;
    }
    vm_dialog.line[line_size] = '\0';
    if (line_size == 0)
        return true;
    if (command) {
        const char *directive = vm_dialog.line + 1;
        int32_t value;

        if (*directive == '@')
            return true;
        if (!rb->strncasecmp(directive, "sound", 5)) {
            vm_dialog_parse_sound(directive);
            return true;
        }
        if (!vm_dialog_find_definition(directive, &value)) {
            DEBUGF("agds: unknown dialog directive %s\n", directive);
            return true;
        }
        rb->strlcpy(vm_dialog.current_definition, directive,
                    sizeof(vm_dialog.current_definition));
        vm_dialog.current_sound = -1;
        if (rb->strncasecmp(directive, "vybervarianty", 13) &&
            rb->strncasecmp(directive, "varianta", 8)) {
            unsigned index;

            for (index = 0; index < vm_dialog.sound_count; index++) {
                size_t actor_size = rb->strlen(vm_dialog.sounds[index].actor);

                if (!rb->strncasecmp(directive,
                                     vm_dialog.sounds[index].actor,
                                     actor_size)) {
                    vm_dialog.current_sound = (int8_t)index;
                    break;
                }
            }
        }
        variable->value = value;
        DEBUGF("agds: dialog directive %s=%ld\n", directive, (long)value);
    } else {
        variable->value = -3;
        DEBUGF("agds: dialog text ready (%u bytes)\n", (unsigned)line_size);
    }
    return true;
}

static bool vm_dialog_next_sound(char *name, size_t name_size)
{
    struct agds_vm_dialog_sound *sound;
    size_t length;
    size_t index;
    int carry;

    if (vm_dialog.current_sound < 0 ||
        vm_dialog.current_sound >= vm_dialog.sound_count)
        return false;
    sound = &vm_dialog.sounds[(unsigned)vm_dialog.current_sound];
    if (rb->snprintf(name, name_size, "%s.ogg", sound->sample) >=
        (int)name_size)
        return false;
    length = rb->strlen(sound->sample);
    index = length;
    carry = sound->step;
    while (index > 0 && carry != 0) {
        int value;

        index--;
        if (sound->sample[index] < '0' || sound->sample[index] > '9')
            break;
        value = sound->sample[index] - '0' + carry;
        sound->sample[index] = (char)('0' + value % 10);
        carry = value / 10;
    }
    return true;
}

static uint16_t vm_dialog_text_timer(void)
{
    static const uint16_t delays[] = {
        2944, 631, 398, 251, 158, 100, 63, 40, 25, 16, 10
    };
    struct agds_vm_global *speed_var = vm_find_system("text_speed", true);
    unsigned speed = speed_var != NULL ?
        (unsigned)MAX(0, MIN(100, speed_var->value)) / 10u : 7u;
    size_t length = rb->strlen(vm_dialog.line);
    uint32_t agds_ticks;
    uint32_t rockbox_ticks;

    if (length < 20u)
        length = 20u;
    agds_ticks = (uint32_t)delays[speed] * length / 41u + 1u;
    rockbox_ticks = (agds_ticks * HZ + 24u) / 25u;
#ifdef SIMULATOR
    rockbox_ticks = (rockbox_ticks + vm_test_speed - 1u) / vm_test_speed;
    return (uint16_t)MAX(MAX(1u, (uint32_t)HZ / vm_test_speed),
                         MIN((uint32_t)(8u * HZ), rockbox_ticks));
#else
    return (uint16_t)MAX((uint32_t)HZ,
                         MIN((uint32_t)(8u * HZ), rockbox_ticks));
#endif
}

static bool vm_dialog_start_tell(struct agds_vm_process *process,
                                 const struct scummvm_agds_object *object,
                                 bool npc, bool explicit_sound,
                                 char *status, size_t status_size)
{
    int32_t text_id;
    int32_t region_id;
    int32_t sound_id = -1;
    char ignored_region[AGDS_ADB_NAME_SIZE];
    char ignored_text[AGDS_VM_DIALOG_LINE_SIZE];
    char sound[AGDS_RESOURCE_NAME_SIZE];

    if (explicit_sound &&
        (!vm_pop(process, &sound_id, status, status_size) ||
         !vm_string(object, sound_id, sound, sizeof(sound))))
        return false;
    if (!vm_pop(process, &text_id, status, status_size) ||
        !vm_pop(process, &region_id, status, status_size) ||
        !vm_string(object, region_id, ignored_region,
                   sizeof(ignored_region)))
        return false;
    if (text_id != -1 &&
        !vm_string(object, text_id, ignored_text, sizeof(ignored_text)))
        return false;
    if (!explicit_sound || sound[0] == '\0') {
        if (!vm_dialog_next_sound(sound, sizeof(sound)))
            sound[0] = '\0';
    }
    if (sound[0] != '\0') {
        process->phase_var[0] = '\0';
        process->sample_volume = 100;
        process->sample_pan = 0;
        process->cycles = 1;
        process->sample_ambient = false;
        if (!vm_queue_audio(AGDS_VM_AUDIO_LOAD, sound, process, true, true,
                            status, status_size))
            return false;
    }
    vm_tell_voice = sound[0] != '\0';
    vm_dialog.text_visible = true;
    vm_dialog.text_npc = npc;
    process->waiting_tell = true;
    process->timer = vm_dialog_text_timer();
    DEBUGF("agds: %s subtitle ready (%u ticks)%s%s\n",
           npc ? "npc" : "player", (unsigned)process->timer,
           sound[0] != '\0' ? " voice=" : "",
           sound[0] != '\0' ? sound : "");
    return vm_suspend(process, status, status_size);
}

static bool vm_push(struct agds_vm_process *process, int32_t value,
                    char *status, size_t status_size)
{
    if (process->sp >= AGDS_VM_STACK) {
        rb->strlcpy(status, "AGDS process stack overflow", status_size);
        return false;
    }
    process->stack[process->sp++] = value;
    return true;
}

static bool vm_pop(struct agds_vm_process *process, int32_t *value,
                   char *status, size_t status_size)
{
    if (process->sp == 0) {
        rb->strlcpy(status, "AGDS process stack underflow", status_size);
        return false;
    }
    *value = process->stack[--process->sp];
    return true;
}

static bool vm_read_u8(const struct scummvm_agds_object *object,
                       struct agds_vm_process *process, uint8_t *value)
{
    if (process->ip >= object->code_size)
        return false;
    *value = object->code[process->ip++];
    return true;
}

static bool vm_read_u16(const struct scummvm_agds_object *object,
                        struct agds_vm_process *process, uint16_t *value)
{
    if (process->ip + 2u > object->code_size)
        return false;
    *value = read_u16le(object->code + process->ip);
    process->ip += 2;
    return true;
}

static bool vm_string(const struct scummvm_agds_object *object, int32_t id,
                      char *text, size_t text_size)
{
    if (id == -1) {
        text[0] = '\0';
        return true;
    }
    if (id <= -2 && id > -12)
        return rb->strlcpy(text, vm_shared[-2 - id], text_size) < text_size;
    if (id < 0 || id > UINT16_MAX)
        return false;
    return scummvm_agds_object_string(object, (uint16_t)id,
                                      text, text_size);
}

static int32_t vm_append_shared(const char *text)
{
    int32_t id = vm_shared_index;
    unsigned slot = (unsigned)(-2 - vm_shared_index);

    rb->strlcpy(vm_shared[slot], text, sizeof(vm_shared[slot]));
    vm_shared_index--;
    if (vm_shared_index <= -12)
        vm_shared_index = -2;
    return id;
}

static bool vm_clone_var_name(const char *object_name, const char *variable,
                              char *name, size_t name_size)
{
    const char *dot = rb->strrchr(object_name, '.');
    const char *cursor;

    if (dot != NULL && dot[1] != '\0') {
        for (cursor = dot + 1; *cursor >= '0' && *cursor <= '9'; cursor++)
            ;
        if (*cursor == '\0') {
            size_t prefix = (size_t)(dot - object_name);

            return rb->snprintf(name, name_size, "%.*s.%s%s",
                                (int)prefix, object_name,
                                variable, dot) < (int)name_size;
        }
    }
    return rb->snprintf(name, name_size, "%s.%s",
                        object_name, variable) < (int)name_size;
}

static struct agds_vm_global *vm_find_global(const char *name, bool create)
{
    unsigned index;
    struct agds_vm_global *empty = NULL;

    for (index = 0; index < AGDS_VM_GLOBALS; index++) {
        if (vm_globals[index].used) {
            if (!rb->strcmp(vm_globals[index].name, name))
                return &vm_globals[index];
        } else if (empty == NULL) {
            empty = &vm_globals[index];
        }
    }
    if (!create || empty == NULL)
        return NULL;
    empty->used = true;
    empty->value = 0;
    rb->strlcpy(empty->name, name, sizeof(empty->name));
    return empty;
}

static int32_t vm_system_default(const char *name)
{
    if (!rb->strcmp(name, "version") || !rb->strcmp(name, "anim_zoom") ||
        !rb->strcmp(name, "screen_curtain") ||
        !rb->strcmp(name, "music_curtain") ||
        !rb->strcmp(name, "sound_curtain") ||
        !rb->strcmp(name, "tell_close_inv"))
        return 1;
    if (!rb->strcmp(name, "sound_volume") ||
        !rb->strcmp(name, "tell_volume"))
        return 100;
    if (!rb->strcmp(name, "music_volume"))
        return 80;
    if (!rb->strcmp(name, "text_speed"))
        return 70;
    if (!rb->strcmp(name, "gfx_bright") ||
        !rb->strcmp(name, "gfx_contrast"))
        return 50;
    if (!rb->strcmp(name, "scroll_factor"))
        return 30;
    if (!rb->strcmp(name, "tell_mode") ||
        !rb->strcmp(name, "subtitle_type"))
        return 3;
    if (!rb->strcmp(name, "objtext_x") ||
        !rb->strcmp(name, "objtext_y") ||
        !rb->strcmp(name, "objtext_mode") ||
        !rb->strcmp(name, "objtext_font") ||
        !rb->strcmp(name, "subtitle_width"))
        return -1;
    return 0;
}

static struct agds_vm_global *vm_find_system(const char *name, bool create)
{
    unsigned index;
    struct agds_vm_global *empty = NULL;

    for (index = 0; index < AGDS_VM_SYSTEM_VARS; index++) {
        if (vm_system_vars[index].used) {
            if (!rb->strcmp(vm_system_vars[index].name, name))
                return &vm_system_vars[index];
        } else if (empty == NULL) {
            empty = &vm_system_vars[index];
        }
    }
    if (!create || empty == NULL)
        return NULL;
    empty->used = true;
    empty->value = vm_system_default(name);
    rb->strlcpy(empty->name, name, sizeof(empty->name));
    return empty;
}

static void vm_reset_process_state(struct agds_vm_process *process)
{
    process->phase_var[0] = '\0';
    process->sample_volume = 100;
    process->sample_pan = 0;
    process->animation_x = 0;
    process->animation_y = 0;
    process->model_yaw = 0;
    process->animation_z = 0;
    process->model_animation = false;
    process->tile_width = 0;
    process->tile_height = 0;
    process->text_color = -1;
    process->text_secondary_color = -1;
    process->light_ambient_color = 0;
    process->light_diffuse_color = 0;
    process->light_specular_color = 0;
    process->light_x = 0;
    process->light_y = 0;
    process->light_z = 0;
    process->text_flags = 0;
    process->film_subtitles_resource = -1;
    process->cycles = 1;
    process->waiting_character[0] = '\0';
    process->phase_controlled = false;
    process->sample_ambient = false;
}

static void vm_remove_object_text(unsigned object_index)
{
    unsigned index;

    for (index = 0; index < AGDS_VM_OBJECT_TEXTS; index++) {
        if (vm_object_texts[index].used &&
            vm_object_texts[index].object_index == object_index)
            vm_object_texts[index].used = false;
    }
}

static bool vm_set_object_text(unsigned object_index, const char *text,
                               const struct agds_vm_process *process,
                               char *status, size_t status_size)
{
    struct agds_vm_object_text_slot *slot = NULL;
    unsigned index;

    if (object_index >= vm_scene->object_count || text == NULL ||
        text[0] == '\0')
        return true;
    for (index = 0; index < AGDS_VM_OBJECT_TEXTS; index++) {
        if (vm_object_texts[index].used &&
            vm_object_texts[index].object_index == object_index) {
            slot = &vm_object_texts[index];
            break;
        }
        if (!vm_object_texts[index].used && slot == NULL)
            slot = &vm_object_texts[index];
    }
    if (slot == NULL) {
        rb->strlcpy(status, "AGDS object text limit reached", status_size);
        return false;
    }
    rb->memset(slot, 0, sizeof(*slot));
    if (rb->strlcpy(slot->overlay.text, text,
                    sizeof(slot->overlay.text)) >=
            sizeof(slot->overlay.text)) {
        rb->strlcpy(status, "AGDS object text exceeds bounded buffer",
                    status_size);
        return false;
    }
    slot->object_index = (uint8_t)object_index;
    slot->overlay.x = process->animation_x;
    slot->overlay.y = process->animation_y;
    {
        struct agds_vm_global *font = vm_find_system("objtext_font", false);

        slot->overlay.font_slot = font != NULL && font->value >= 0 &&
            font->value < AGDS_VM_MAX_FONTS ? (int16_t)font->value : 0;
    }
    slot->overlay.color = process->text_color;
    slot->overlay.flags = process->text_flags;
    slot->used = true;

    /* Retail 2.509 calls ScreenObject::setPosition(owner, x, y, 1)
     * immediately after creating the text surface.  Keep the text and any
     * later MoveScreenObject operation on the same authored object. */
    vm_scene->objects[object_index].x = process->animation_x;
    vm_scene->objects[object_index].y = process->animation_y;
    return true;
}

static bool vm_save_slot_exists(int32_t slot)
{
    char name[24];
    char path[MAX_PATH];

    if (slot < 0 || slot >= 99 || vm_target == NULL)
        return false;
    rb->snprintf(name, sizeof(name), "save.%ld", (long)slot);
    return scummvm_make_path(path, sizeof(path), vm_target->savepath, name) &&
        rb->file_exists(path);
}

static bool vm_queue_audio(enum scummvm_agds_audio_action action,
                           const char *resource_entry,
                           const struct agds_vm_process *process,
                           bool play_now, bool synchronized,
                           char *status, size_t status_size)
{
    unsigned slot;
    struct scummvm_agds_audio_event *event;

    if (vm_audio_count >= AGDS_VM_AUDIO_EVENTS) {
        rb->strlcpy(status, "AGDS audio event queue full", status_size);
        return false;
    }
    slot = (vm_audio_read + vm_audio_count) % AGDS_VM_AUDIO_EVENTS;
    event = &vm_audio_events[slot];
    rb->memset(event, 0, sizeof(*event));
    event->action = action;
    if (resource_entry != NULL)
        rb->strlcpy(event->resource_entry, resource_entry,
                    sizeof(event->resource_entry));
    rb->strlcpy(event->phase_var, process->phase_var,
                sizeof(event->phase_var));
    event->volume = process->sample_volume;
    event->pan = process->sample_pan;
    event->cycles = process->cycles;
    event->play_now = play_now;
    event->ambient = process->sample_ambient;
    event->synchronized = synchronized;
    vm_audio_count++;
    return true;
}

static int vm_find_screen_object(const char *name)
{
    unsigned index;

    for (index = 0; index < vm_scene->object_count; index++) {
        if (!rb->strcmp(vm_scene->objects[index].name, name))
            return (int)index;
    }
    return -1;
}

static void vm_clear_key_handlers(unsigned object_index)
{
    unsigned index;

    for (index = 0; index < AGDS_VM_KEY_HANDLERS; index++) {
        if (vm_key_handlers[index].used &&
            vm_key_handlers[index].object_index == object_index)
            vm_key_handlers[index].used = false;
    }
}

static bool vm_add_key_handler(unsigned object_index, const char *key,
                               uint16_t ip,
                               char *status, size_t status_size)
{
    struct agds_vm_key_handler *empty = NULL;
    unsigned index;

    if (key[0] == '\0' || rb->strlen(key) >= AGDS_VM_KEY_NAME_SIZE) {
        rb->strlcpy(status, "AGDS key handler name invalid", status_size);
        return false;
    }
    for (index = 0; index < AGDS_VM_KEY_HANDLERS; index++) {
        struct agds_vm_key_handler *handler = &vm_key_handlers[index];

        if (!handler->used) {
            if (empty == NULL)
                empty = handler;
            continue;
        }
        if (handler->object_index == object_index &&
            !rb->strcasecmp(handler->key, key)) {
            handler->ip = ip;
            return true;
        }
    }
    if (empty == NULL) {
        rb->strlcpy(status, "AGDS key handler limit reached", status_size);
        return false;
    }
    rb->memset(empty, 0, sizeof(*empty));
    rb->strlcpy(empty->key, key, sizeof(empty->key));
    empty->object_index = object_index;
    empty->ip = ip;
    empty->used = true;
    return true;
}

static bool vm_add_use_object_handler(
    struct scummvm_agds_screen_object *object,
    uint16_t target_id, uint16_t ip,
    char *status, size_t status_size)
{
    unsigned index;

    for (index = 0; index < object->use_object_handler_count; index++) {
        if (object->use_object_handlers[index].target_id == target_id) {
            object->use_object_handlers[index].ip = ip;
            return true;
        }
    }
    if (object->use_object_handler_count >=
            AGDS_VM_MAX_USE_OBJECT_HANDLERS) {
        rb->strlcpy(status, "AGDS OnUseObject handler limit reached",
                    status_size);
        return false;
    }
    index = object->use_object_handler_count++;
    object->use_object_handlers[index].target_id = target_id;
    object->use_object_handlers[index].ip = ip;
    return true;
}

static int vm_add_screen_object(const char *name, char *status,
                                size_t status_size)
{
    int existing = vm_find_screen_object(name);
    struct scummvm_agds_screen_object *object;

    if (existing >= 0 && vm_scene->objects[existing].alive)
        return existing;
    if (existing >= 0) {
        object = &vm_scene->objects[existing];
    } else if (vm_scene->object_count < AGDS_VM_MAX_SCREEN_OBJECTS) {
        object = &vm_scene->objects[vm_scene->object_count];
        existing = vm_scene->object_count++;
    } else {
        rb->strlcpy(status, "AGDS screen object limit reached", status_size);
        return -1;
    }
    vm_clear_key_handlers((unsigned)existing);
    vm_remove_object_text((unsigned)existing);
    rb->memset(object, 0, sizeof(*object));
    rb->strlcpy(object->name, name, sizeof(object->name));
    object->visible = true;
    object->alive = true;
    return existing;
}

static bool vm_load_region(const char *name,
                           struct scummvm_agds_region *region,
                           char *status, size_t status_size)
{
    struct scummvm_agds_adb_entry entry;
    unsigned char data[38 + AGDS_VM_MAX_REGION_POLYGONS * 2 +
                       AGDS_VM_MAX_REGION_POINTS * 6];
    uint32_t offset = 38;

    if (!scummvm_agds_find_adb_entry(vm_target, name, &entry,
                                     status, status_size) ||
        entry.size < 38u ||
        entry.size > sizeof(data) ||
        scummvm_agds_read_adb_entry(vm_target, &entry, 0, data,
                                    entry.size) != (long)entry.size) {
        rb->snprintf(status, status_size,
                     "AGDS region %.32s exceeds bounded format", name);
        return false;
    }
    rb->memset(region, 0, sizeof(*region));
    region->center_x = (int16_t)read_u16le(data + 32);
    region->center_y = (int16_t)read_u16le(data + 34);
    while (offset + 2u <= entry.size) {
        uint16_t count = read_u16le(data + offset);
        unsigned index;

        offset += 2;
        if (region->polygon_count >= AGDS_VM_MAX_REGION_POLYGONS ||
            count > AGDS_VM_MAX_REGION_POINTS - region->point_count ||
            offset + (uint32_t)count * 6u > entry.size) {
            rb->snprintf(status, status_size,
                         "AGDS region %.32s polygon invalid", name);
            return false;
        }
        region->polygon_start[region->polygon_count] = region->point_count;
        region->polygon_size[region->polygon_count] = (uint8_t)count;
        region->polygon_count++;
        for (index = 0; index < count; index++) {
            struct scummvm_agds_point *point =
                &region->points[region->point_count++];

            point->x = (int16_t)read_u16le(data + offset);
            point->y = (int16_t)read_u16le(data + offset + 2);
            offset += 6;
        }
    }
    if (offset != entry.size) {
        rb->snprintf(status, status_size,
                     "AGDS region %.32s trailing byte", name);
        return false;
    }
    return true;
}

static bool vm_region_center(const char *name, int32_t *x, int32_t *y,
                             char *status, size_t status_size)
{
    struct scummvm_agds_region region;

    if (!vm_load_region(name, &region, status, status_size))
        return false;
    *x = region.center_x;
    *y = region.center_y;
    return true;
}

static bool vm_point_on_segment(int32_t x, int32_t y,
                                const struct scummvm_agds_point *a,
                                const struct scummvm_agds_point *b)
{
    int32_t cross = (x - a->x) * (b->y - a->y) -
                    (y - a->y) * (b->x - a->x);

    if (cross != 0)
        return false;
    return x >= MIN(a->x, b->x) && x <= MAX(a->x, b->x) &&
           y >= MIN(a->y, b->y) && y <= MAX(a->y, b->y);
}

static bool vm_region_contains(const struct scummvm_agds_region *region,
                               int32_t x, int32_t y)
{
    unsigned polygon;

    for (polygon = 0; polygon < region->polygon_count; polygon++) {
        unsigned start = region->polygon_start[polygon];
        unsigned count = region->polygon_size[polygon];
        unsigned index;
        bool inside = false;

        if (count < 3)
            continue;
        for (index = 0; index < count; index++) {
            const struct scummvm_agds_point *a =
                &region->points[start + index];
            const struct scummvm_agds_point *b =
                &region->points[start + ((index + 1u) % count)];

            if (vm_point_on_segment(x, y, a, b))
                return true;
            if ((a->y > y) != (b->y > y)) {
                int32_t crossing = a->x +
                    (y - a->y) * (b->x - a->x) / (b->y - a->y);
                if (x < crossing)
                    inside = !inside;
            }
        }
        if (inside)
            return true;
    }
    return false;
}

static bool vm_region_line_inside(
    const struct scummvm_agds_region *region,
    const struct scummvm_agds_point *from,
    const struct scummvm_agds_point *to)
{
    int32_t dx = (int32_t)to->x - from->x;
    int32_t dy = (int32_t)to->y - from->y;
    unsigned distance = (unsigned)MAX(AGDS_VM_ABS(dx), AGDS_VM_ABS(dy));
    unsigned steps = MAX(1u, (distance + 3u) / 4u);
    unsigned step;
    bool from_inside = vm_region_contains(region, from->x, from->y);
    bool to_inside = vm_region_contains(region, to->x, to->y);
    bool entered = from_inside;
    bool left = false;

    for (step = 0; step <= steps; step++) {
        int32_t x = from->x + dx * (int32_t)step / (int32_t)steps;
        int32_t y = from->y + dy * (int32_t)step / (int32_t)steps;
        bool inside = vm_region_contains(region, x, y);

        if (from_inside && to_inside) {
            if (!inside)
                return false;
        } else if (!from_inside && to_inside) {
            if (inside)
                entered = true;
            else if (entered)
                return false;
        } else if (from_inside && !to_inside) {
            if (!inside)
                left = true;
            else if (left)
                return false;
        } else {
            return false;
        }
    }
    return (from_inside || entered) && (to_inside || left);
}

static uint32_t vm_path_distance(const struct scummvm_agds_point *a,
                                 const struct scummvm_agds_point *b)
{
    uint32_t dx = (uint32_t)AGDS_VM_ABS((int32_t)b->x - a->x);
    uint32_t dy = (uint32_t)AGDS_VM_ABS((int32_t)b->y - a->y);
    uint32_t high = MAX(dx, dy);
    uint32_t low = MIN(dx, dy);

    /* Bounded octile approximation used only to select authored vertices. */
    return high + (low * 3u) / 8u;
}

static bool vm_character_plan_path(
    struct agds_vm_character *character,
    const struct scummvm_agds_point *destination,
    char *status, size_t status_size)
{
    struct scummvm_agds_point nodes[AGDS_VM_MAX_REGION_POINTS + 2];
    uint32_t distances[AGDS_VM_MAX_REGION_POINTS + 2];
    int8_t previous[AGDS_VM_MAX_REGION_POINTS + 2];
    bool visited[AGDS_VM_MAX_REGION_POINTS + 2];
    uint8_t reverse[AGDS_VM_MAX_REGION_POINTS + 1];
    unsigned node_count = 2;
    unsigned reverse_count = 0;
    unsigned index;

    nodes[0].x = character->x;
    nodes[0].y = character->y;
    nodes[1] = *destination;
    character->path_count = 0;
    character->path_index = 0;
    if (!vm_scene->navigation_region_set) {
        character->path[0] = *destination;
        character->path_count = 1;
        return true;
    }
    for (index = 0; index < vm_scene->navigation_region.point_count;
         index++)
        nodes[node_count++] = vm_scene->navigation_region.points[index];
    rb->memset(visited, 0, sizeof(visited));
    for (index = 0; index < node_count; index++) {
        distances[index] = UINT32_MAX;
        previous[index] = -1;
    }
    distances[0] = 0;
    for (index = 0; index < node_count; index++) {
        unsigned candidate = node_count;
        unsigned other;

        for (other = 0; other < node_count; other++) {
            if (!visited[other] && distances[other] != UINT32_MAX &&
                (candidate == node_count ||
                 distances[other] < distances[candidate]))
                candidate = other;
        }
        if (candidate == node_count)
            break;
        if (candidate == 1)
            break;
        visited[candidate] = true;
        for (other = 1; other < node_count; other++) {
            uint32_t edge;
            uint32_t total;

            if (visited[other] || candidate == other ||
                !vm_region_line_inside(&vm_scene->navigation_region,
                                       &nodes[candidate], &nodes[other]))
                continue;
            edge = vm_path_distance(&nodes[candidate], &nodes[other]);
            total = distances[candidate] > UINT32_MAX - edge ?
                UINT32_MAX : distances[candidate] + edge;
            if (total < distances[other]) {
                distances[other] = total;
                previous[other] = (int8_t)candidate;
            }
        }
    }
    if (distances[1] == UINT32_MAX) {
        rb->snprintf(status, status_size,
                     "AGDS no authored path from %d,%d to %d,%d",
                     character->x, character->y,
                     destination->x, destination->y);
        return false;
    }
    index = 1;
    while (index != 0) {
        if (reverse_count >= ARRAYLEN(reverse) || previous[index] < 0) {
            rb->strlcpy(status, "AGDS authored path exceeds bounds",
                        status_size);
            return false;
        }
        reverse[reverse_count++] = (uint8_t)index;
        index = (unsigned)previous[index];
    }
    if (reverse_count > AGDS_VM_CHARACTER_PATH_POINTS) {
        rb->strlcpy(status, "AGDS authored character path too long",
                    status_size);
        return false;
    }
    while (reverse_count > 0)
        character->path[character->path_count++] =
            nodes[reverse[--reverse_count]];
    DEBUGF("agds: character %s authored path has %u waypoint(s)\n",
           character->name, (unsigned)character->path_count);
    return true;
}

static bool vm_suspend(const struct agds_vm_process *process,
                       char *status, size_t status_size)
{
    if (vm_suspended_count >= AGDS_VM_SUSPENDED) {
        rb->strlcpy(status, "AGDS suspended process limit reached", status_size);
        return false;
    }
    if (!rb->strcmp(process->name, "1122.10e1") &&
        process->ip != vm_intro_debug_ip) {
        vm_intro_debug_ip = process->ip;
        DEBUGF("agds: intro process suspended at %u\n",
               (unsigned)process->ip);
    }
    vm_suspended[vm_suspended_count++] = *process;
    return true;
}

static bool vm_suspend_for_character(
    struct agds_vm_process *process,
    const struct agds_vm_character *character,
    char *status, size_t status_size)
{
    rb->strlcpy(process->waiting_character, character->name,
                sizeof(process->waiting_character));
    return vm_suspend(process, status, status_size);
}

static bool vm_run_process(struct agds_vm_process *process, unsigned depth,
                           char *status, size_t status_size);

static bool vm_run_object_as(const char *name, const char *code_entry,
                             unsigned depth,
                             char *status, size_t status_size)
{
    struct agds_vm_process process;
    int object_index;

    if (depth >= AGDS_VM_DEPTH) {
        rb->strlcpy(status, "AGDS object recursion limit reached", status_size);
        return false;
    }
    object_index = vm_add_screen_object(name, status, status_size);
    if (object_index < 0)
        return false;
    rb->memset(&process, 0, sizeof(process));
    rb->strlcpy(process.name, name, sizeof(process.name));
    rb->strlcpy(process.code_entry, code_entry, sizeof(process.code_entry));
    rb->strlcpy(vm_scene->objects[object_index].code_entry, code_entry,
                sizeof(vm_scene->objects[object_index].code_entry));
    process.object_index = (uint16_t)object_index;
    vm_reset_process_state(&process);

    /* Room 1864's Martin, chair and handset are one authored skeletal scene.
     * Keep the script object for VM identity, but never route it through the
     * independent gameplay-character renderer. */
    if (!rb->strcmp(name, "1122.10e1")) {
        vm_scene->objects[object_index].character_model = false;
        vm_scene->objects[object_index].character_render_owner =
            AGDS_VM_CHARACTER_RENDER_DIRECT_SCENE;
    }
    /* 10ba is intentionally absent from this retail data.adb and is named
     * by the room's post-initialisation patch hook. patch.adb supplies the
     * patch metadata, not an executable object body. */
    if (!rb->strcmp(name, "10ba")) {
        vm_scene->objects[object_index].visible = false;
        return true;
    }
    return vm_run_process(&process, depth, status, status_size);
}

static bool vm_apply_screen_patch_refs(char *status, size_t status_size)
{
    unsigned index;

    for (index = 0; index < AGDS_VM_PATCH_REFS; index++) {
        const struct agds_vm_patch_ref *patch = &vm_patch_refs[index];
        int object_index;

        if (!patch->used || rb->strcmp(patch->screen, vm_scene->name))
            continue;
        object_index = vm_find_screen_object(patch->object);
        if (patch->refs > 0 && object_index < 0) {
            if (!vm_run_object(patch->object, 0,
                               status, status_size))
                return false;
        } else if (patch->refs <= 0 && object_index >= 0) {
            vm_scene->objects[object_index].alive = false;
            vm_scene->objects[object_index].visible = false;
        }
    }
    return true;
}

static bool vm_run_object(const char *name, unsigned depth,
                          char *status, size_t status_size)
{
    return vm_run_object_as(name, name, depth, status, status_size);
}

static bool vm_run_inventory_object(unsigned slot, uint16_t ip,
                                    unsigned depth,
                                    char *status, size_t status_size)
{
    struct agds_vm_process process;

    if (slot >= AGDS_VM_INVENTORY_SLOTS || !vm_inventory[slot].used ||
        depth >= AGDS_VM_DEPTH) {
        rb->strlcpy(status, "AGDS inventory process is invalid", status_size);
        return false;
    }
    rb->memset(&process, 0, sizeof(process));
    rb->strlcpy(process.name, vm_inventory[slot].name,
                sizeof(process.name));
    rb->strlcpy(process.code_entry, vm_inventory[slot].name,
                sizeof(process.code_entry));
    process.ip = ip;
    process.inventory_slot_plus_one = (uint8_t)slot + 1u;
    vm_reset_process_state(&process);
    return vm_run_process(&process, depth, status, status_size);
}

static void vm_sync_character_object(const struct agds_vm_character *character)
{
    int index;

    if (character == NULL || character->object[0] == '\0')
        return;
    index = vm_find_screen_object(character->object);
    if (index < 0)
        return;
    vm_scene->objects[index].x = character->x;
    vm_scene->objects[index].y = character->y;
    vm_scene->objects[index].visible = character->visible &&
                                        character->scene_positioned;
    vm_scene->objects[index].character_model = true;
    vm_scene->objects[index].character_render_owner = character->render_owner;
    rb->strlcpy(vm_scene->objects[index].character_definition,
                character->definition,
                sizeof(vm_scene->objects[index].character_definition));
    rb->strlcpy(vm_scene->objects[index].character_pose_descriptor,
                character->active_animation[0] != '\0' ?
                    character->active_animation : character->definition,
                sizeof(vm_scene->objects[index].character_pose_descriptor));
    vm_scene->objects[index].character_direction = character->direction;
    vm_scene->objects[index].character_phase = character->phase;
    vm_scene->objects[index].character_pose_frame =
        character->animation_pose_frame;
}

static void vm_apply_saved_scene_state(void)
{
    unsigned index;

    if (!vm_restore_pending || vm_scene == NULL)
        return;
    for (index = 0; index < vm_saved_object_count; index++) {
        const struct agds_vm_saved_object *saved = &vm_saved_objects[index];
        int object_index = vm_find_screen_object(saved->name);
        struct scummvm_agds_screen_object *object;

        if (object_index < 0)
            continue;
        object = &vm_scene->objects[object_index];
        object->x = saved->x;
        object->y = saved->y;
        object->z = saved->z;
        object->character_direction = saved->character_direction;
        object->character_phase = saved->character_phase;
        object->character_pose_frame = saved->character_pose_frame;
        object->character_render_owner =
            (enum scummvm_agds_character_render_owner)
                saved->character_render_owner;
        object->visible = saved->visible;
        object->alive = saved->alive;
    }
    for (index = 0; index < vm_saved_character_count; index++) {
        const struct agds_vm_saved_character *saved =
            &vm_saved_characters[index];
        struct agds_vm_character *character =
            vm_find_character(saved->name, true);

        if (character == NULL)
            continue;
        character->x = saved->x;
        character->y = saved->y;
        character->direction = saved->direction;
        character->scene_positioned = true;
        character->position_x_q16 = (int32_t)saved->x << 16;
        character->position_y_q16 = (int32_t)saved->y << 16;
        character->enabled = saved->enabled;
        character->visible = saved->visible;
        character->moving = false;
        character->leaving = false;
        character->stopping = false;
        character->phase = -1;
        character->animation_frames = 0;
        character->animation_elapsed_ticks = 0;
        character->animation_pose_frame = 0;
        character->active_animation[0] = '\0';
        character->render_owner = AGDS_VM_CHARACTER_RENDER_IDLE;
        vm_sync_character_object(character);
    }
    DEBUGF("agds: restored screen %s objects=%u characters=%u\n",
           vm_scene->name, (unsigned)vm_saved_object_count,
           (unsigned)vm_saved_character_count);
    vm_saved_character_count = 0;
    vm_saved_object_count = 0;
    vm_restore_pending = false;
}

static int16_t vm_character_direction_to(int32_t dx, int32_t dy)
{
    int32_t ax = AGDS_VM_ABS(dx);
    int32_t ay = AGDS_VM_ABS(dy);

    if (ax == 0 && ay == 0)
        return -1;
    if ((int64_t)ax * 1000 <= (int64_t)ay * 414)
        return dy > 0 ? 180 : 0;
    if ((int64_t)ay * 1000 <= (int64_t)ax * 414)
        return dx > 0 ? 90 : 270;
    if (dx > 0)
        return dy > 0 ? 135 : 45;
    return dy > 0 ? 225 : 315;
}

static uint32_t vm_character_remaining_distance(
    const struct agds_vm_character *character);

static bool vm_character_set_locomotion_clip(
    struct agds_vm_character *character, unsigned animation,
    char *status, size_t status_size)
{
    const struct agds_vm_motion_record *motion;

    if (animation >= AGDS_VM_CHARACTER_ANIMATIONS ||
        character->animations[animation][0] == '\0') {
        rb->strlcpy(status, "AGDS retail locomotion clip missing",
                    status_size);
        return false;
    }
    motion = vm_find_motion_record(character->animations[animation]);
    if (motion == NULL) {
        rb->snprintf(status, status_size,
                     "AGDS retail motion %.24s missing",
                     character->animations[animation]);
        return false;
    }
    character->locomotion_animation = (uint8_t)animation;
    character->locomotion_ticks = 0;
    character->locomotion_pose_frame = 0;
    character->animation_pose_frame = 0;
    character->render_owner = AGDS_VM_CHARACTER_RENDER_LOCOMOTION;
    rb->strlcpy(character->active_animation,
                character->animations[animation],
                sizeof(character->active_animation));
    vm_sync_character_object(character);
    DEBUGF("agds: character %s retail locomotion clip %s at %d,%d "
           "remaining=%lu\n", character->name, character->active_animation,
           character->x, character->y,
           (unsigned long)vm_character_remaining_distance(character));
    return true;
}

static uint32_t vm_character_remaining_distance(
    const struct agds_vm_character *character)
{
    struct scummvm_agds_point current;
    uint32_t distance = 0;
    unsigned index;

    current.x = character->x;
    current.y = character->y;
    for (index = character->path_index; index < character->path_count;
         index++) {
        distance += vm_path_distance(&current, &character->path[index]);
        current = character->path[index];
    }
    return distance;
}

static void vm_character_finish_movement(
    struct agds_vm_character *character)
{
    if (character->path_count > 0) {
        const struct scummvm_agds_point *destination =
            &character->path[character->path_count - 1u];

        character->x = destination->x;
        character->y = destination->y;
        character->position_x_q16 = (int32_t)character->x << 16;
        character->position_y_q16 = (int32_t)character->y << 16;
    }
    if (character->final_direction != -1)
        character->direction = character->final_direction;
    character->moving = false;
    character->stopping = false;
    character->path_count = 0;
    character->path_index = 0;
    character->locomotion_ticks = 0;
    character->locomotion_pose_frame = 0;
    character->animation_pose_frame = 0;
    character->active_animation[0] = '\0';
    character->render_owner = AGDS_VM_CHARACTER_RENDER_IDLE;
    if (character->leaving)
        character->visible = false;
    character->leaving = false;
    vm_sync_character_object(character);
    DEBUGF("agds: character %s movement complete at %d,%d direction=%d\n",
           character->name, character->x, character->y,
           character->direction);
}

static bool vm_character_begin_movement(
    struct agds_vm_character *character,
    const struct scummvm_agds_region *destination,
    int16_t final_direction, bool leaving,
    char *status, size_t status_size)
{
    struct scummvm_agds_point point;
    int16_t direction;

    if (!vm_load_character_motion(status, status_size))
        return false;
    point.x = destination->center_x;
    point.y = destination->center_y;
    if (!vm_character_plan_path(character, &point, status, status_size))
        return false;
    character->position_x_q16 = (int32_t)character->x << 16;
    character->position_y_q16 = (int32_t)character->y << 16;
    character->final_direction = final_direction;
    character->moving = true;
    character->leaving = leaving;
    character->stopping = false;
    character->last_stride_animation = 1;
    character->visible = true;
    direction = vm_character_direction_to(
        (int32_t)character->path[0].x - character->x,
        (int32_t)character->path[0].y - character->y);
    if (direction >= 0)
        character->direction = direction;
    DEBUGF("agds: character %s begins retail %s from %d,%d to %d,%d\n",
           character->name, leaving ? "leave" : "move",
           character->x, character->y, point.x, point.y);
    return vm_character_set_locomotion_clip(
        character, 0, status, status_size);
}

static void vm_character_apply_motion_sample(
    struct agds_vm_character *character,
    const struct agds_vm_motion_record *motion, uint16_t pose)
{
    uint16_t sample = motion->first_sample + pose;
    uint16_t previous = pose > 0 ? sample - 1u : sample;
    int32_t root_x = vm_motion.root_x_q16[sample] -
                     vm_motion.root_x_q16[previous];
    int32_t root_z = vm_motion.root_z_q16[sample] -
                     vm_motion.root_z_q16[previous];

    if (character->path_index < character->path_count) {
        const struct scummvm_agds_point *waypoint =
            &character->path[character->path_index];
        int32_t target_x = ((int32_t)waypoint->x << 16) -
                           character->position_x_q16;
        int32_t target_y = ((int32_t)waypoint->y << 16) -
                           character->position_y_q16;
        uint32_t absolute_x = (uint32_t)AGDS_VM_ABS(target_x);
        uint32_t absolute_y = (uint32_t)AGDS_VM_ABS(target_y);
        int64_t denominator = MAX(absolute_x, absolute_y) +
                              MIN(absolute_x, absolute_y) * 3u / 8u;
        int32_t step_x = denominator != 0 ? (int32_t)(
            ((int64_t)root_z * target_x -
             (int64_t)root_x * target_y) / denominator) : 0;
        int32_t step_y = denominator != 0 ? (int32_t)(
            ((int64_t)root_z * target_y +
             (int64_t)root_x * target_x) / denominator) : 0;
        int64_t target_length = (int64_t)target_x * target_x +
                                (int64_t)target_y * target_y;
        int64_t progress = (int64_t)step_x * target_x +
                           (int64_t)step_y * target_y;
        int16_t direction = vm_character_direction_to(target_x, target_y);

        if (direction >= 0)
            character->direction = direction;

        if (target_length == 0 || progress >= target_length) {
            character->position_x_q16 = (int32_t)waypoint->x << 16;
            character->position_y_q16 = (int32_t)waypoint->y << 16;
            character->path_index++;
            if (character->path_index < character->path_count) {
                waypoint = &character->path[character->path_index];
                direction = vm_character_direction_to(
                    ((int32_t)waypoint->x << 16) -
                        character->position_x_q16,
                    ((int32_t)waypoint->y << 16) -
                        character->position_y_q16);
                if (direction >= 0)
                    character->direction = direction;
            }
        } else if (progress > 0) {
            character->position_x_q16 += step_x;
            character->position_y_q16 += step_y;
        }
        character->x = (int16_t)((character->position_x_q16 + 32768) >> 16);
        character->y = (int16_t)((character->position_y_q16 + 32768) >> 16);
    }
}

static bool vm_character_tick_movement(
    struct agds_vm_character *character,
    char *status, size_t status_size)
{
    const struct agds_vm_motion_record *motion =
        vm_find_motion_record(character->active_animation);
    uint32_t pose;

    if (motion == NULL) {
        rb->strlcpy(status, "AGDS active retail motion missing", status_size);
        return false;
    }
    character->locomotion_ticks++;
    pose = (uint32_t)character->locomotion_ticks * vm_motion.pose_fps /
           (uint32_t)HZ;
    while (character->locomotion_pose_frame < pose &&
           character->locomotion_pose_frame + 1u < motion->sample_count) {
        character->locomotion_pose_frame++;
        vm_character_apply_motion_sample(
            character, motion, character->locomotion_pose_frame);
    }
    character->animation_pose_frame = character->locomotion_pose_frame;
    vm_sync_character_object(character);
    if (pose < motion->sample_count)
        return true;

    if (character->locomotion_animation >= 3 || character->stopping) {
        vm_character_finish_movement(character);
        return true;
    }
    if (character->path_index >= character->path_count ||
        vm_character_remaining_distance(character) <= 16u) {
        unsigned ending = character->last_stride_animation == 2 ? 4u : 3u;

        return vm_character_set_locomotion_clip(
            character, ending, status, status_size);
    }
    if (character->locomotion_animation == 1)
        character->last_stride_animation = 2;
    else
        character->last_stride_animation = 1;
    return vm_character_set_locomotion_clip(
        character, character->last_stride_animation, status, status_size);
}

static bool vm_attach_visible_characters(char *status, size_t status_size)
{
    unsigned index;

    for (index = 0; index < AGDS_VM_CHARACTERS; index++) {
        struct agds_vm_character *character = &vm_characters[index];
        int object_index;

        if (!character->used || !character->visible ||
            character->object[0] == '\0')
            continue;
        object_index = vm_add_screen_object(character->object,
                                            status, status_size);
        if (object_index < 0)
            return false;
        rb->strlcpy(vm_scene->objects[object_index].code_entry,
                    character->object,
                    sizeof(vm_scene->objects[object_index].code_entry));
        vm_sync_character_object(character);
    }
    return true;
}

static bool vm_character_animation_frames(const char *descriptor,
                                          uint16_t *frames,
                                          uint16_t *period_ticks,
                                          char *status, size_t status_size)
{
    struct scummvm_agds_adb_entry entry;
    unsigned char header[69];
    char resource[AGDS_RESOURCE_NAME_SIZE];
    size_t length;
    uint32_t track_count;
    uint32_t record_size;
    uint32_t payload;
    uint32_t frame_count;
    unsigned char times[8];
    uint32_t first_time;
    uint32_t second_time;
    uint32_t period_ms;

    if (!scummvm_agds_read_text(vm_target, descriptor,
                                resource, sizeof(resource),
                                status, status_size))
        return false;
    length = rb->strlen(resource);
    while (length > 0) {
        unsigned char last = (unsigned char)resource[length - 1];

        if ((last >= 'a' && last <= 'z') ||
            (last >= 'A' && last <= 'Z') ||
            (last >= '0' && last <= '9') || last == '.' ||
            last == '_' || last == '-')
            break;
        resource[--length] = '\0';
    }
    if (length == 0 ||
        !scummvm_agds_find_named_adb_entry(
            vm_target, "models.adb", resource, &entry,
            status, status_size) ||
        entry.size < sizeof(header) ||
        scummvm_agds_read_adb_entry(vm_target, &entry, 0, header,
                                    sizeof(header)) !=
            (long)sizeof(header))
        return false;
    track_count = read_u16le(header + 61);
    if (track_count == 0 || track_count > 256u ||
        (entry.size - sizeof(header)) % track_count != 0)
        goto invalid;
    record_size = (entry.size - sizeof(header)) / track_count;
    if (record_size <= 252u + 6u * 64u)
        goto invalid;
    payload = record_size - 252u - 6u * 64u;
    if (payload % (4u + 64u) != 0)
        goto invalid;
    frame_count = payload / (4u + 64u);
    if (frame_count == 0 || frame_count > UINT16_MAX)
        goto invalid;
    period_ms = 1;
    if (frame_count > 1) {
        if (scummvm_agds_read_adb_entry(
                vm_target, &entry, sizeof(header) + 252u,
                times, sizeof(times)) != (long)sizeof(times))
            goto invalid;
        first_time = read_u32le(times);
        second_time = read_u32le(times + 4);
        if (second_time <= first_time)
            goto invalid;
        period_ms = second_time - first_time;
    }
    *frames = (uint16_t)frame_count;
    *period_ticks = (uint16_t)MAX(
        1u, (period_ms * (uint32_t)HZ + 999u) / 1000u);
    DEBUGF("agds: character animation %s -> %s (%u phases, %lu ms)\n",
           descriptor, resource, (unsigned)*frames, (unsigned long)period_ms);
    return true;

invalid:
    rb->snprintf(status, status_size,
                 "AGDS character animation %.24s invalid", resource);
    return false;
}

static bool vm_binary(struct agds_vm_process *process, uint16_t opcode,
                      char *status, size_t status_size)
{
    int32_t left;
    int32_t right;
    int32_t result;

    if (!vm_pop(process, &right, status, status_size) ||
        !vm_pop(process, &left, status, status_size))
        return false;
    switch (opcode) {
    case 22: result = left == right; break;
    case 23: result = left != right; break;
    case 24: result = left > right; break;
    case 25: result = left < right; break;
    case 26: result = left >= right; break;
    case 27: result = left <= right; break;
    case 28: result = left + right; break;
    case 29: result = left - right; break;
    case 30: result = left * right; break;
    case 31:
        if (right == 0) goto divide_by_zero;
        result = left / right;
        break;
    case 32:
        if (right == 0) goto divide_by_zero;
        result = left % right;
        break;
    case 33: result = left & right; break;
    case 34: result = left | right; break;
    case 35: result = left ^ right; break;
    case 37: result = left << right; break;
    case 38: result = left >> right; break;
    case 39: result = left && right; break;
    case 40: result = left || right; break;
    default: return false;
    }
    return vm_push(process, result, status, status_size);

divide_by_zero:
    rb->strlcpy(status, "AGDS divide by zero", status_size);
    return false;
}

static bool vm_pop_count(struct agds_vm_process *process, unsigned count,
                         char *status, size_t status_size)
{
    int32_t ignored;

    while (count-- > 0) {
        if (!vm_pop(process, &ignored, status, status_size))
            return false;
    }
    return true;
}

static bool vm_run_process(struct agds_vm_process *process, unsigned depth,
                           char *status, size_t status_size)
{
    struct scummvm_agds_object *object;
    struct scummvm_agds_screen_object *screen_object;
    unsigned instructions = 0;

    if (depth >= AGDS_VM_DEPTH ||
        !scummvm_agds_object_load(vm_target, process->code_entry,
                                  &vm_objects[depth], status, status_size))
        return false;
    object = &vm_objects[depth];
    if (process->inventory_slot_plus_one != 0) {
        unsigned slot = process->inventory_slot_plus_one - 1u;

        if (slot >= AGDS_VM_INVENTORY_SLOTS || !vm_inventory[slot].used)
            return false;
        screen_object = &vm_inventory[slot].object;
    } else {
        if (process->object_index >= vm_scene->object_count)
            return false;
        screen_object = &vm_scene->objects[process->object_index];
    }
    screen_object->object_id = object->id;

    while (process->ip < object->code_size &&
           instructions++ < AGDS_VM_INSTRUCTION_LIMIT) {
        uint16_t opcode;
        uint16_t immediate16;
        uint8_t immediate8;
        int32_t first;
        int32_t second;
        int32_t third;
        char name[AGDS_ADB_NAME_SIZE];
        char name2[AGDS_ADB_NAME_SIZE];
        char name3[AGDS_ADB_NAME_SIZE];

        if (!scummvm_agds_decode_opcode(object, &process->ip, &opcode)) {
            rb->snprintf(status, status_size,
                         "AGDS %.24s opcode invalid at %u",
                         process->name, (unsigned)process->ip);
            return false;
        }
        if (opcode == 5) {
            if (process->ip + 16u > object->code_size)
                return false;
            process->ip += 16;
        } else if (opcode == 6 || opcode == 12) {
            return true;
        } else if (opcode == 8 || opcode == 9) {
            int16_t delta;

            if (!vm_read_u16(object, process, &immediate16))
                return false;
            delta = (int16_t)immediate16;
            if (opcode == 8) {
                if (!vm_pop(process, &first, status, status_size))
                    return false;
                if (first != 0)
                    continue;
            }
            if ((delta < 0 && (uint16_t)-delta > process->ip) ||
                (delta >= 0 && (uint32_t)process->ip + delta >
                    object->code_size)) {
                rb->strlcpy(status, "AGDS branch outside object", status_size);
                return false;
            }
            process->ip = (uint16_t)(process->ip + delta);
        } else if (opcode == 10) {
            if (!vm_pop_count(process, 1, status, status_size)) return false;
        } else if (opcode == 11) {
            if (process->sp == 0 ||
                !vm_push(process, process->stack[process->sp - 1],
                         status, status_size)) return false;
        } else if (opcode == 13) {
            return vm_suspend(process, status, status_size);
        } else if (opcode == 14) {
            uint16_t high;
            if (!vm_read_u16(object, process, &immediate16) ||
                !vm_read_u16(object, process, &high) ||
                !vm_push(process, (int32_t)((uint32_t)immediate16 |
                    ((uint32_t)high << 16)), status, status_size)) return false;
        } else if (opcode == 15 || opcode == 17) {
            if (!vm_read_u16(object, process, &immediate16) ||
                !vm_push(process, (int16_t)immediate16,
                         status, status_size)) return false;
        } else if (opcode == 16 || opcode == 18) {
            if (!vm_read_u8(object, process, &immediate8) ||
                !vm_push(process, (int8_t)immediate8,
                         status, status_size)) return false;
        } else if (opcode == 21) {
            struct agds_vm_global *global;
            if (!vm_read_u8(object, process, &immediate8) ||
                !scummvm_agds_object_string(object, immediate8,
                                             name, sizeof(name))) return false;
            global = vm_find_global(name, true);
            if (global == NULL ||
                !vm_push(process, global->value, status, status_size))
                return false;
        } else if ((opcode >= 22 && opcode <= 35) ||
                   (opcode >= 37 && opcode <= 40)) {
            if (!vm_binary(process, opcode, status, status_size)) return false;
        } else if (opcode == 36 || opcode == 41 || opcode == 42) {
            if (!vm_pop(process, &first, status, status_size)) return false;
            if (opcode == 36) first = ~first;
            else if (opcode == 41) first = !first;
            else first = -first;
            if (!vm_push(process, first, status, status_size)) return false;
        } else if (opcode == 45 || opcode == 46) {
            struct agds_vm_global *global;
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            global = vm_find_global(name, true);
            if (global == NULL ||
                !vm_push(process, global->value, status, status_size)) return false;
            global->value += opcode == 45 ? 1 : -1;
        } else if (opcode == 48) {
            struct agds_vm_global *global;
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name)) ||
                !vm_pop(process, &second, status, status_size)) return false;
            global = vm_find_global(name, true);
            if (global == NULL) return false;
            global->value = second;
            if (!rb->strcmp(name, "112b"))
                DEBUGF("agds: intro state 112b=%ld by %s\n",
                       (long)second, process->name);
        } else if (opcode >= 49 && opcode <= 58) {
            struct agds_vm_global *global;

            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name)) ||
                process->sp == 0)
                return false;
            second = process->stack[process->sp - 1];
            global = vm_find_global(name, true);
            if (global == NULL)
                return false;
            if ((opcode == 52 || opcode == 53) && second == 0) {
                rb->strlcpy(status, "AGDS global divide by zero",
                            status_size);
                return false;
            }
            switch (opcode) {
            case 49: global->value += second; break;
            case 50: global->value -= second; break;
            case 51: global->value *= second; break;
            case 52: global->value /= second; break;
            case 53: global->value %= second; break;
            case 54: global->value <<= second; break;
            case 55: global->value >>= second; break;
            case 56: global->value &= second; break;
            case 57: global->value |= second; break;
            case 58: global->value ^= second; break;
            }
        } else if (opcode == 59 || opcode == 60 || opcode == 61 ||
                   opcode == 62 || opcode == 63 || opcode == 64 ||
                   opcode == 65 || opcode == 201 || opcode == 202 ||
                   opcode == 209 || opcode == 229) {
            if (!vm_read_u16(object, process, &immediate16) ||
                (uint32_t)process->ip + immediate16 > object->code_size)
                return false;
            if (opcode == 59) {
                struct agds_vm_process child = *process;
                child.sp = 0;
                child.timer = 0;
                process->ip += immediate16;
                if (!vm_run_process(&child, depth + 1, status, status_size))
                    return false;
            } else {
                uint16_t handler = process->ip;

                if (opcode == 60)
                    screen_object->look_handler = handler;
                else if (opcode == 61)
                    screen_object->click_handler = handler;
                else if (opcode == 62)
                    screen_object->fallback_handler = handler;
                else if (opcode == 201)
                    screen_object->throw_handler = handler;
                else if (opcode == 202)
                    screen_object->use_on_handler = handler;
                else if (opcode == 209)
                    screen_object->user_use_handler = handler;
                if (opcode == 229) {
                    if (process->inventory_slot_plus_one != 0) {
                        rb->strlcpy(status,
                                    "AGDS inventory key handler invalid",
                                    status_size);
                        return false;
                    }
                    if (!vm_pop(process, &first, status, status_size) ||
                        !vm_string(object, first, name, sizeof(name)) ||
                        !vm_add_key_handler(process->object_index, name,
                                            handler, status, status_size))
                        return false;
                } else if (opcode == 63) {
                    struct scummvm_agds_object *target_object;

                    if (!vm_pop(process, &first, status, status_size) ||
                        !vm_string(object, first, name, sizeof(name)) ||
                        depth + 1u >= AGDS_VM_DEPTH) {
                        return false;
                    }
                    target_object = &vm_objects[depth + 1u];
                    if (!scummvm_agds_object_load(vm_target, name,
                                                  target_object,
                                                  status, status_size) ||
                        !vm_add_use_object_handler(
                            screen_object, target_object->id, handler,
                            status, status_size))
                        return false;
                } else if (opcode == 65 &&
                           !vm_pop_count(process, 1,
                                         status, status_size)) {
                    return false;
                }
                process->ip += immediate16;
            }
        } else if (opcode == 66 || opcode == 68 || opcode == 69 ||
                   opcode == 70) {
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            if (opcode == 68) {
                rb->strlcpy(screen_object->region_entry, name,
                            sizeof(screen_object->region_entry));
                if (!vm_load_region(name, &screen_object->region,
                                    status, status_size))
                    return false;
            } else if (opcode == 69) {
                rb->strlcpy(screen_object->picture_entry, name,
                            sizeof(screen_object->picture_entry));
            }
        } else if (opcode == 71) {
            if (!vm_pop(process, &first, status, status_size)) return false;
            screen_object->z = (int16_t)first;
        } else if (opcode == 72) {
            screen_object->background = true;
        } else if (opcode == 75) {
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name)) ||
                !vm_load_region(name, &vm_scene->navigation_region,
                                status, status_size)) return false;
            vm_scene->navigation_region_set = true;
            DEBUGF("agds: navigation region %s has %u authored points\n",
                   name,
                   (unsigned)vm_scene->navigation_region.point_count);
        } else if (opcode == 76) {
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name)) ||
                !vm_run_object(name, depth + 1, status, status_size)) return false;
        } else if (opcode == 77) {
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name2, sizeof(name2)) ||
                !vm_pop(process, &second, status, status_size) ||
                !vm_string(object, second, name, sizeof(name)) ||
                !vm_run_object_as(name, name2, depth + 1,
                                  status, status_size)) return false;
        } else if (opcode == 78) {
            int index;
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            index = vm_find_screen_object(name);
            if (index >= 0) {
                vm_scene->objects[index].visible = false;
                vm_scene->objects[index].alive = false;
                vm_clear_key_handlers((unsigned)index);
                vm_remove_object_text((unsigned)index);
                if ((uint16_t)index == process->object_index)
                    process->survive_removal = true;
            }
        } else if (opcode == 88) {
            struct agds_vm_character *character;

            if (!vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            character = vm_find_character(name, false);
            if (character != NULL) {
                character->scene_positioned = true;
                character->x = process->animation_x;
                character->y = process->animation_y;
                character->position_x_q16 = (int32_t)character->x << 16;
                character->position_y_q16 = (int32_t)character->y << 16;
                character->direction = (int16_t)second;
                character->phase = 0;
                vm_sync_character_object(character);
            }
        } else if (opcode == 85) {
            struct agds_vm_character *character;

            /* AGDS LoadCharacter(name, definition, object-template). */
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name3, sizeof(name3)) ||
                !vm_pop(process, &second, status, status_size) ||
                !vm_string(object, second, name2, sizeof(name2)) ||
                !vm_pop(process, &third, status, status_size) ||
                !vm_string(object, third, name, sizeof(name))) return false;
            character = vm_find_character(name, true);
            if (character == NULL) {
                rb->strlcpy(status, "AGDS character limit reached",
                            status_size);
                return false;
            }
            rb->strlcpy(character->definition, name2,
                        sizeof(character->definition));
            rb->strlcpy(character->object, name3,
                        sizeof(character->object));
            character->enabled = true;
            character->render_owner = AGDS_VM_CHARACTER_RENDER_IDLE;
            DEBUGF("agds: character %s loaded from %s as %s\n",
                   character->name, character->definition,
                   character->object);
        } else if (opcode == 86 || opcode == 89 || opcode == 90 ||
                   opcode == 91 || opcode == 92) {
            struct agds_vm_character *character;
            int character_object;

            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            character = vm_find_character(name, false);
            if (character == NULL)
                continue;
            character_object = vm_find_screen_object(character->object);
            if (opcode == 86) {
                if (character_object >= 0) {
                    vm_scene->objects[character_object].visible = false;
                    vm_scene->objects[character_object].alive = false;
                }
                rb->memset(character, 0, sizeof(*character));
            } else if (opcode == 89) {
                character->visible = false;
                character->moving = false;
                character->leaving = false;
                character->stopping = false;
                if (character_object >= 0)
                    vm_scene->objects[character_object].visible = false;
            } else if (opcode == 90) {
                character->visible = true;
                if (!vm_run_object(character->object, depth + 1,
                                   status, status_size)) return false;
                vm_sync_character_object(character);
            } else {
                character->enabled = opcode == 92;
            }
        } else if (opcode == 93 || opcode == 95 || opcode == 228) {
            struct agds_vm_character *character;
            struct scummvm_agds_region destination;

            if (!vm_pop(process, &third, status, status_size) ||
                !vm_pop(process, &second, status, status_size) ||
                !vm_string(object, second, name2, sizeof(name2)) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name)) ||
                !vm_load_region(name2, &destination,
                                status, status_size)) return false;
            character = vm_find_character(name, false);
            if (character != NULL) {
                if (opcode == 95) {
                    DEBUGF("agds: character %s set position %d,%d\n",
                           name, destination.center_x, destination.center_y);
                    character->scene_positioned = true;
                    character->x = destination.center_x;
                    character->y = destination.center_y;
                    character->position_x_q16 =
                        (int32_t)character->x << 16;
                    character->position_y_q16 =
                        (int32_t)character->y << 16;
                    if (third != -1)
                        character->direction = (int16_t)third;
                    character->moving = false;
                    character->leaving = false;
                    character->stopping = false;
                    character->active_animation[0] = '\0';
                    character->render_owner =
                        AGDS_VM_CHARACTER_RENDER_IDLE;
                    vm_sync_character_object(character);
                } else if (!vm_character_begin_movement(
                               character, &destination, (int16_t)third,
                               false, status, status_size)) {
                    return false;
                } else if (character->enabled) {
                    return vm_suspend_for_character(
                        process, character, status, status_size);
                }
            }
        } else if (opcode == 94) {
            struct agds_vm_character *character;
            struct scummvm_agds_region destination;

            if (!vm_pop(process, &second, status, status_size) ||
                !vm_string(object, second, name2, sizeof(name2)) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name)) ||
                !vm_load_region(name2, &destination,
                                status, status_size)) return false;
            character = vm_find_character(name, false);
            if (character != NULL) {
                if (!vm_character_begin_movement(
                        character, &destination, -1, true,
                        status, status_size))
                    return false;
                if (character->enabled)
                    return vm_suspend_for_character(
                        process, character, status, status_size);
            }
        } else if (opcode == 97) {
            struct agds_vm_character *character;

            if (!vm_pop(process, &second, status, status_size) ||
                !vm_string(object, second, name2, sizeof(name2)) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            character = vm_find_character(name, false);
            if (character != NULL) {
                /* PointTo changes facing, not position; bounded until the
                 * directional animation renderer supplies completion. */
                process->timer = 1;
                return vm_suspend(process, status, status_size);
            }
        } else if (opcode == 96) {
            struct agds_vm_character *character;

            if (!vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            character = vm_find_character(name, false);
            if (character != NULL)
                character->direction = (int16_t)second;
        } else if (opcode == 79 || opcode == 80) {
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            rb->strlcpy(vm_next_screen, name, sizeof(vm_next_screen));
            return true;
        } else if (opcode == 98 || opcode == 99) {
            vm_scene->user_enabled = opcode == 99;
            DEBUGF("agds: user %s by %s at %u\n",
                   opcode == 99 ? "enabled" : "disabled",
                   process->name, (unsigned)process->ip);
        } else if (opcode == 106) {
            rb->memset(vm_inventory, 0, sizeof(vm_inventory));
            vm_current_inventory_slot = -1;
        } else if (opcode == 109 || opcode == 245) {
            int slot;

            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            slot = vm_inventory_find(name);
            vm_current_inventory_slot = slot >= 0 ? (int8_t)slot : -1;
        } else if (opcode == 110) {
            vm_current_inventory_slot = -1;
        } else if (opcode == 162 || opcode == 163) {
            vm_inventory_enabled = opcode == 163;
            if (!vm_inventory_enabled)
                vm_current_inventory_slot = -1;
        } else if (opcode == 164) {
            if (vm_previous_screen[0] != '\0')
                rb->strlcpy(vm_next_screen, vm_previous_screen,
                            sizeof(vm_next_screen));
            return true;
        } else if (opcode == 173 || opcode == 174) {
            /* Retail AGDS defines both as intentional stubs: cursor removal
             * and relative-mouse setup are handled by the host frontend. */
        } else if (opcode == 236) {
            if (!vm_push(process, vm_scene->user_enabled,
                         status, status_size)) return false;
        } else if (opcode == 105 || opcode == 108 ||
                   opcode == 136 ||
                   opcode == 194) {
            /* Process/render state not needed to construct the object graph. */
        } else if (opcode == 284) {
            vm_scene->clip_x = process->animation_x;
            vm_scene->clip_y = process->animation_y;
            vm_scene->clip_width = process->tile_width > 0 ?
                (uint16_t)process->tile_width : 0;
            vm_scene->clip_height = process->tile_height > 0 ?
                (uint16_t)process->tile_height : 0;
            vm_scene->clip_set = vm_scene->clip_width != 0 &&
                                 vm_scene->clip_height != 0;
            DEBUGF("agds: screen clip %d,%d %ux%u\n",
                   (int)vm_scene->clip_x, (int)vm_scene->clip_y,
                   (unsigned)vm_scene->clip_width,
                   (unsigned)vm_scene->clip_height);
        } else if (opcode == 156 || opcode == 157) {
            if (!vm_pop(process, &first, status, status_size)) return false;
            if (first < 0 || first >= 99) {
                rb->strlcpy(status, "AGDS save position is invalid",
                            status_size);
                return false;
            }
            if (opcode == 156) {
                vm_load_request = (int8_t)first;
                DEBUGF("agds: retail load requested slot %ld\n",
                       (long)first);
                return true;
            }
            DEBUGF("agds: retail save requested slot %ld\n", (long)first);
            if (!scummvm_agds_vm_save_game((int)first,
                                           status, status_size))
                return false;
        } else if (opcode == 159) {
            rb->memset(vm_mouse_areas, 0, sizeof(vm_mouse_areas));
            vm_mouse_disabled = false;
        } else if (opcode == 128) {
            vm_reset_process_state(process);
        } else if (opcode == 119) {
            process->phase_controlled = true;
        } else if (opcode == 100 || opcode == 101) {
            struct agds_vm_global *global;
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            global = vm_find_global(name, true);
            if (global == NULL) return false;
            global->value |= opcode == 100 ? 2 : 4;
            rb->strlcpy(process->phase_var, name,
                        sizeof(process->phase_var));
            if (!vm_queue_audio(opcode == 100 ? AGDS_VM_AUDIO_RESTART :
                                               AGDS_VM_AUDIO_STOP,
                                NULL, process, opcode == 100, false,
                                status, status_size)) return false;
        } else if (opcode == 135) {
            struct agds_vm_global *global;
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            global = vm_find_global(name, true);
            if (global == NULL) return false;
            global->value = 0;
            rb->strlcpy(process->phase_var, name,
                        sizeof(process->phase_var));
        } else if (opcode == 118) {
            bool play_now;
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            play_now = process->sample_ambient ||
                !process->phase_controlled || process->phase_var[0] == '\0';
            if (!vm_queue_audio(AGDS_VM_AUDIO_LOAD, name, process,
                                play_now, false,
                                status, status_size)) return false;
        } else if (opcode == 117) {
            struct agds_vm_animation *animation;
            struct agds_vm_global *phase;

            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            if (process->phase_var[0] == '\0') {
                /* Retail permits a phase-less preload.  It validates and
                 * caches the resource but does not insert it into the active
                 * scene animation list. */
                DEBUGF("agds: preload animation %s without phase variable\n",
                       name);
                continue;
            }
            animation = vm_find_animation(process->phase_var, true);
            if (animation == NULL) {
                rb->strlcpy(status, "AGDS animation limit reached",
                            status_size);
                return false;
            }
            rb->strlcpy(name2, process->phase_var, sizeof(name2));
            rb->memset(animation, 0, sizeof(*animation));
            rb->strlcpy(animation->phase_var, name2,
                        sizeof(animation->phase_var));
            animation->used = true;
            if (!vm_probe_animation(name, animation, status, status_size)) {
                rb->memset(animation, 0, sizeof(*animation));
                return false;
            }
            animation->x = process->animation_x;
            animation->y = process->animation_y;
            animation->yaw = process->model_yaw;
            animation->z = process->animation_z;
            animation->object_index = process->object_index;
            animation->model_animation = process->model_animation;
            animation->inserted = !process->phase_controlled;
            animation->phase_controlled = process->phase_controlled;
            animation->active = animation->frame_count != 0;
            phase = vm_find_global(animation->phase_var, true);
            if (phase == NULL)
                return false;
            phase->value = 0;
            if (animation->frame_count != 0)
                vm_sync_model_animation(animation);
        } else if (opcode == 120 || opcode == 122 || opcode == 123 ||
                   opcode == 124 || opcode == 125) {
            bool npc = opcode == 123 || opcode == 124;
            bool explicit_sound = opcode == 122 || opcode == 124;

            return vm_dialog_start_tell(process, object, npc,
                                        explicit_sound,
                                        status, status_size);
        } else if (opcode == 130) {
            if (!vm_pop(process, &first, status, status_size)) return false;
            process->cycles = first > 0 ? (uint16_t)first : 1;
        } else if (opcode == 133) {
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_pop(process, &second, status, status_size)) return false;
            process->sample_pan = (int16_t)first;
            process->sample_volume = (int16_t)second;
        } else if (opcode == 212) {
            if (!vm_pop(process, &third, status, status_size) ||
                !vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            /* Runtime voices are phase-var keyed. The bounded mixer applies
             * clamped volume/pan on subsequent events for that phase. */
            process->sample_volume = (int16_t)MAX(0, MIN(100, second));
            process->sample_pan = (int16_t)MAX(-100, MIN(100, third));
        } else if (opcode == 203) {
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name2, sizeof(name2)) ||
                !vm_pop(process, &second, status, status_size) ||
                !vm_string(object, second, name, sizeof(name)) ||
                !scummvm_agds_read_text(vm_target, name,
                                        vm_film_event.video_name,
                                        sizeof(vm_film_event.video_name),
                                        status, status_size))
                return false;
            if (vm_film_active) {
                rb->strlcpy(status, "AGDS nested film is invalid",
                            status_size);
                return false;
            }
            vm_film_event.subtitles_entry[0] = '\0';
            if (process->film_subtitles_resource >= 0 &&
                !vm_string(object, process->film_subtitles_resource,
                           vm_film_event.subtitles_entry,
                           sizeof(vm_film_event.subtitles_entry)))
                return false;
            if (name2[0] != '\0' &&
                !vm_queue_audio(AGDS_VM_AUDIO_LOAD, name2, process,
                                true, true, status, status_size))
                return false;
            vm_film_pending = true;
            vm_film_active = true;
            process->waiting_film = true;
            DEBUGF("agds: retail film %s audio=%s subtitles=%s\n",
                   vm_film_event.video_name, name2,
                   vm_film_event.subtitles_entry);
            return vm_suspend(process, status, status_size);
        } else if (opcode == 111 || opcode == 112) {
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            if (name[0] != '\0') {
                if (opcode == 111) {
                    if (!vm_inventory_add(name, depth + 1u,
                                          status, status_size))
                        return false;
                } else {
                    if (process->inventory_slot_plus_one != 0 &&
                        !rb->strcasecmp(
                            vm_inventory[
                                process->inventory_slot_plus_one - 1u].name,
                            name))
                        process->survive_removal = true;
                    vm_inventory_remove(name);
                }
            }
        } else if (opcode == 141 || opcode == 185) {
            unsigned used = scummvm_agds_vm_inventory_count();
            int32_t value = opcode == 185 ? AGDS_VM_INVENTORY_SLOTS :
                AGDS_VM_INVENTORY_SLOTS - used;

            if (!vm_push(process, value, status, status_size)) return false;
        } else if (opcode == 186) {
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_push(process,
                         first >= 0 && first < AGDS_VM_INVENTORY_SLOTS &&
                             vm_inventory[first].used,
                         status, status_size)) return false;
        } else if (opcode == 187) {
            if (!vm_pop(process, &first, status, status_size)) return false;
            name[0] = '\0';
            if (first >= 0 && first < AGDS_VM_INVENTORY_SLOTS &&
                vm_inventory[first].used)
                rb->strlcpy(name, vm_inventory[first].name, sizeof(name));
            if (!vm_push(process, vm_append_shared(name),
                         status, status_size)) return false;
        } else if (opcode == 189 || opcode == 238) {
            int slot;

            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            slot = vm_inventory_find(name);
            if (!vm_push(process, opcode == 189 ? slot : slot >= 0,
                         status, status_size)) return false;
        } else if (opcode == 193) {
            unsigned read;
            unsigned write = 0;

            for (read = 0; read < AGDS_VM_INVENTORY_SLOTS; read++) {
                if (!vm_inventory[read].used)
                    continue;
                if (read != write)
                    vm_inventory[write] = vm_inventory[read];
                write++;
            }
            while (write < AGDS_VM_INVENTORY_SLOTS)
                rb->memset(&vm_inventory[write++], 0,
                           sizeof(vm_inventory[0]));
            vm_current_inventory_slot = -1;
        } else if (opcode == 167) {
            vm_current_inventory_slot = -1;
            process->timer = 1;
            return vm_suspend(process, status, status_size);
        } else if (opcode == 144 || opcode == 145) {
            if (!vm_push(process,
                         opcode == 144 ? vm_saved_mouse_x : vm_saved_mouse_y,
                         status, status_size)) return false;
        } else if (opcode == 177) {
            if (!vm_push(process, vm_append_shared(screen_object->name),
                         status, status_size)) return false;
        } else if (opcode == 180) {
            const char *id = rb->strrchr(screen_object->name, '.');

            id = id != NULL ? id + 1 : screen_object->name;
            if (!vm_push(process, rb->atoi(id),
                         status, status_size)) return false;
        } else if (opcode == 178 || opcode == 179) {
            struct agds_vm_global *global;

            if (opcode == 179 &&
                !vm_pop(process, &third, status, status_size)) return false;
            if (!vm_pop(process, &second, status, status_size) ||
                !vm_string(object, second, name2, sizeof(name2)) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name)) ||
                !vm_clone_var_name(name, name2, name3, sizeof(name3)))
                return false;
            global = vm_find_global(name3, opcode == 179);
            if (opcode == 178) {
                if (!vm_push(process, global != NULL ? global->value : 0,
                             status, status_size)) return false;
            } else {
                if (global == NULL)
                    return false;
                global->value = third;
                if (!vm_push(process, third,
                             status, status_size)) return false;
            }
        } else if (opcode == 73) {
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            rb->strlcpy(screen_object->inventory_text_entry, name,
                        sizeof(screen_object->inventory_text_entry));
        } else if (opcode == 258 || opcode == 129) {
            if (!vm_pop(process, &first, status, status_size)) return false;
            if (opcode == 258) process->model_yaw = (int16_t)first;
            else process->animation_z = (int16_t)first;
        } else if (opcode == 137 ||
                   opcode == 172 || opcode == 223 ||
                   opcode == 278 || opcode == 279 ||
                   opcode == 291) {
            if (!vm_pop_count(process, 1, status, status_size)) return false;
        } else if (opcode == 131 ||
                   opcode == 132 || opcode == 170 || opcode == 237) {
            if (!vm_pop_count(process, 1, status, status_size)) return false;
        } else if (opcode == 190) {
            if (!vm_pop(process, &first, status, status_size)) return false;
            screen_object->inventory_type = (uint16_t)first;
        } else if (opcode == 200) {
            const char *descriptor;

            if (!vm_pop(process, &first, status, status_size) ||
                !vm_pop(process, &second, status, status_size)) return false;
            descriptor = vm_picture_handle_descriptor(second);
            if (descriptor == NULL || first < 0 || first > UINT16_MAX) {
                rb->strlcpy(status,
                            "AGDS inventory picture handle invalid",
                            status_size);
                return false;
            }
            rb->strlcpy(screen_object->inventory_picture_entry, descriptor,
                        sizeof(screen_object->inventory_picture_entry));
            screen_object->picture_tile = (uint16_t)first;
        } else if (opcode == 221) {
            struct agds_vm_global *phase;
            unsigned scene_request = 0;
            struct agds_vm_animation *animation;

            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            if (!rb->strcmp(name, "1122.10e1.11c6"))
                scene_request = 1;
            else if (!rb->strcmp(name, "1122.10e1.118b"))
                scene_request = 2;
            else if (!rb->strcmp(name, "1122.10e1.118d"))
                scene_request = 3;
            else if (!rb->strcmp(name, "1122.10e1.118e"))
                scene_request = 4;
            phase = vm_find_global(name, true);
            if (phase == NULL) return false;
            if (scene_request != 0) {
                vm_scene_animation_request = scene_request;
                /* Insert publishes a live phase before the script resumes;
                 * otherwise a completed clip (-1) can spin in a tight loop
                 * before the compositor gets a chance to restart it. */
                if (phase->value < 0)
                    phase->value = 0;
                continue;
            }
            animation = vm_find_animation(name, false);
            if (animation == NULL) {
                phase->value = -1;
                continue;
            }
            if (animation->frame_count != 0) {
                unsigned index;

                for (index = 0; index < AGDS_VM_ANIMATIONS; index++) {
                    if (vm_animations[index].object_index ==
                        animation->object_index)
                        vm_animations[index].inserted = false;
                }
                animation->inserted = true;
                if (!animation->active) {
                    animation->frame = 0;
                    animation->phase_accumulator = 0;
                    animation->active = true;
                    DEBUGF("agds: animation restart %s\n",
                           animation->descriptor);
                }
                phase->value = animation->frame;
                vm_sync_model_animation(animation);
            }
        } else if (opcode == 127) {
            if (!vm_pop(process, &first, status, status_size)) return false;
            process->timer = first > 0 ? (uint16_t)first : 0;
            return vm_suspend(process, status, status_size);
        } else if (opcode == 134) {
            if (!vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size)) return false;
            process->animation_x = (int16_t)first;
            process->animation_y = (int16_t)second;
        } else if (opcode == 231) {
            if (!vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size)) return false;
            vm_scene->light_index = (int16_t)first;
            vm_scene->light_type = (int16_t)second;
            vm_scene->light_x = process->light_x;
            vm_scene->light_y = process->light_y;
            vm_scene->light_z = process->light_z;
            vm_scene->light_ambient_color = process->light_ambient_color;
            vm_scene->light_diffuse_color = process->light_diffuse_color;
            vm_scene->light_specular_color = process->light_specular_color;
            vm_scene->light_set = true;
            DEBUGF("agds: retail light %d type=%d position=%d,%d,%d "
                   "ambient=%06lx diffuse=%06lx specular=%06lx\n",
                   vm_scene->light_index, vm_scene->light_type,
                   vm_scene->light_x, vm_scene->light_y, vm_scene->light_z,
                   (unsigned long)vm_scene->light_ambient_color,
                   (unsigned long)vm_scene->light_diffuse_color,
                   (unsigned long)vm_scene->light_specular_color);
        } else if (opcode == 165 || opcode == 225 ||
                   opcode == 257 ||
                   opcode == 280 || opcode == 292) {
            if (opcode == 165) {
                int index;
                if (!vm_pop(process, &second, status, status_size) ||
                    !vm_pop(process, &first, status, status_size)) return false;
                if (!vm_pop(process, &third, status, status_size))
                    return false;
                if (!vm_string(object, third,
                               name, sizeof(name))) return false;
                index = vm_find_screen_object(name);
                if (index >= 0) {
                    vm_scene->objects[index].x = (int16_t)first;
                    vm_scene->objects[index].y = (int16_t)second;
                }
            } else if (!vm_pop_count(process, 2, status, status_size)) {
                return false;
            }
        } else if (opcode == 138 || opcode == 139) {
            struct agds_vm_patch_ref *patch;

            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name2, sizeof(name2)) ||
                !vm_pop(process, &second, status, status_size) ||
                !vm_string(object, second, name, sizeof(name))) return false;
            if (name[0] == '\0')
                rb->strlcpy(name, vm_scene->name, sizeof(name));
            patch = vm_find_patch_ref(name, name2, true);
            if (patch == NULL) {
                rb->strlcpy(status, "AGDS screen patch limit reached",
                            status_size);
                return false;
            }
            if (opcode == 138 && patch->refs < INT16_MAX)
                patch->refs++;
            else if (opcode == 139 && patch->refs > INT16_MIN)
                patch->refs--;
            if (!rb->strcmp(name, vm_scene->name)) {
                int index = vm_find_screen_object(name2);

                if (opcode == 138 && index < 0 &&
                    !vm_run_object(name2, depth + 1,
                                   status, status_size))
                    return false;
                if (opcode == 139 && index >= 0) {
                    vm_scene->objects[index].alive = false;
                    vm_scene->objects[index].visible = false;
                }
            }
        } else if (opcode == 140) {
            int index;
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name)) ||
                !vm_pop(process, &second, status, status_size)) return false;
            index = vm_find_screen_object(name);
            if (!vm_push(process, index >= 0 && vm_scene->objects[index].visible,
                         status, status_size)) return false;
        } else if (opcode == 142) {
            if (!vm_pop_count(process, 2, status, status_size)) return false;
        } else if (opcode == 143) {
            struct agds_vm_global *system;
            if (!vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            system = vm_find_system(name, true);
            if (system == NULL) return false;
            system->value = second;
        } else if (opcode == 148 || opcode == 149 || opcode == 150) {
            struct agds_vm_character *character;
            int32_t value = -1;

            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            character = vm_find_character(name, false);
            if (character != NULL) {
                if (opcode == 148)
                    value = character->phase;
                else if (opcode == 149)
                    value = character->x;
                else
                    value = character->y;
            }
            if (!vm_push(process, value, status, status_size)) return false;
        } else if (opcode == 151) {
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name)) ||
                !vm_push(process, !rb->strcmp(name, vm_scene->name),
                         status, status_size)) return false;
        } else if (opcode == 152 || opcode == 153) {
            if (!vm_pop_count(process, 1, status, status_size) ||
                !vm_push(process, 0, status, status_size)) return false;
        } else if (opcode == 146 || opcode == 147) {
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name)) ||
                !vm_region_center(name, &first, &second,
                                  status, status_size) ||
                !vm_push(process, opcode == 146 ? first : second,
                         status, status_size)) return false;
        } else if (opcode == 154 || opcode == 155) {
            int index;
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            index = vm_find_screen_object(name);
            if (!vm_push(process,
                         index >= 0 && vm_scene->objects[index].alive ?
                         (opcode == 154 ? vm_scene->objects[index].x :
                                          vm_scene->objects[index].y) : 0,
                         status, status_size)) return false;
        } else if (opcode == 195 || opcode == 196) {
            int index;
            int32_t value = 0;

            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            index = vm_find_screen_object(name);
            if (index >= 0 && vm_scene->objects[index].alive) {
                value = opcode == 195 ?
                    vm_scene->objects[index].picture_width :
                    vm_scene->objects[index].picture_height;
            } else {
                index = vm_inventory_find(name);
                if (index >= 0)
                    value = opcode == 195 ?
                        vm_inventory[index].object.tile_width :
                        vm_inventory[index].object.tile_height;
            }
            if (!vm_push(process, value, status, status_size)) return false;
        } else if (opcode == 158) {
            vm_quit_requested = true;
            return true;
        } else if (opcode == 166) {
            int index;
            if (!vm_pop(process, &third, status, status_size) ||
                !vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            index = vm_find_screen_object(name);
            if (index >= 0 && vm_scene->objects[index].alive) {
                vm_scene->objects[index].region_offset_x = (int16_t)second;
                vm_scene->objects[index].region_offset_y = (int16_t)third;
            }
        } else if (opcode == 197) {
            if (!vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
        } else if (opcode == 169) {
            struct agds_vm_global *system;
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            system = vm_find_system(name, true);
            if (system == NULL ||
                !vm_push(process, system->value,
                         status, status_size)) return false;
        } else if (opcode == 171) {
            if (!vm_pop_count(process, 1, status, status_size) ||
                !vm_push(process, 0, status, status_size)) return false;
        } else if (opcode == 175) {
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name)) ||
                !vm_push(process, vm_append_shared(name),
                         status, status_size)) return false;
        } else if (opcode == 176) {
            if (!vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            rb->snprintf(name2, sizeof(name2), "%.48s.%ld", name,
                         (long)second);
            if (!vm_push(process, vm_append_shared(name2),
                         status, status_size)) return false;
        } else if (opcode == 182) {
            if (!vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size)) return false;
            process->tile_width = (int16_t)first;
            process->tile_height = (int16_t)second;
            if (first >= 0 && first <= UINT16_MAX &&
                second >= 0 && second <= UINT16_MAX) {
                screen_object->tile_width = (uint16_t)first;
                screen_object->tile_height = (uint16_t)second;
            }
        } else if (opcode == 184) {
            /* Retail binds the current process name to its inventory object.
             * The bounded VM already owns that exact process/object pair. */
            if (!vm_pop_count(process, 1, status, status_size)) return false;
        } else if (opcode == 183) {
            int index;
            struct scummvm_agds_region *region;

            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            index = vm_find_screen_object(name);
            if (index < 0) index = process->object_index;
            region = &vm_scene->objects[index].region;
            rb->memset(region, 0, sizeof(*region));
            region->center_x = process->animation_x + process->tile_width / 2;
            region->center_y = process->animation_y + process->tile_height / 2;
            region->polygon_count = 1;
            region->point_count = 4;
            region->polygon_size[0] = 4;
            region->points[0].x = process->animation_x;
            region->points[0].y = process->animation_y;
            region->points[1].x = process->animation_x + process->tile_width;
            region->points[1].y = process->animation_y;
            region->points[2].x = process->animation_x + process->tile_width;
            region->points[2].y = process->animation_y + process->tile_height;
            region->points[3].x = process->animation_x;
            region->points[3].y = process->animation_y + process->tile_height;
        } else if (opcode == 192) {
            int index;
            if (!vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            index = vm_find_screen_object(name);
            if (index >= 0)
                vm_scene->objects[index].ignore_region = second > 0;
        } else if (opcode == 242) {
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name)) ||
                !vm_push(process, vm_find_global(name, false) != NULL,
                         status, status_size)) return false;
        } else if (opcode == 191) {
            if (!vm_pop(process, &first, status, status_size)) return false;
            vm_mouse_disabled = first > 0;
        } else if (opcode == 198) {
            int handle;

            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            handle = vm_picture_handle_add(name);
            if (handle == 0 ||
                !vm_push(process, handle, status, status_size)) {
                rb->strlcpy(status, "AGDS picture handle limit reached",
                            status_size);
                return false;
            }
        } else if (opcode == 226) {
            int index;

            if (!vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            index = vm_find_screen_object(name);
            if (index >= 0)
                vm_scene->objects[index].visible = second > 0;
        } else if (opcode == 233) {
            int index;

            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            index = vm_find_screen_object(name);
            if (index >= 0) {
                vm_scene->objects[index].picture_entry[0] = '\0';
                vm_scene->objects[index].character_model = false;
            }
        } else if (opcode == 294) {
            /* Inventory SetObjectName consumes the shared object name. */
            if (!vm_pop_count(process, 1, status, status_size)) return false;
        } else if (opcode == 82 || opcode == 83 || opcode == 84) {
            int index;

            if (!vm_pop(process, &second, status, status_size) ||
                !vm_string(object, second, name2, sizeof(name2)) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            if (opcode == 84) {
                if (!rb->strcmp(name, vm_scene->name) &&
                    !vm_load_region(name2, &vm_scene->navigation_region,
                                    status, status_size))
                    return false;
                if (!rb->strcmp(name, vm_scene->name))
                    vm_scene->navigation_region_set = true;
            } else {
                index = vm_find_screen_object(name);
                if (index >= 0) {
                    if (opcode == 82)
                        rb->strlcpy(
                            vm_scene->objects[index].inventory_text_entry,
                            name2,
                            sizeof(vm_scene->objects[index].
                                   inventory_text_entry));
                    else if (!vm_load_region(
                                 name2, &vm_scene->objects[index].region,
                                 status, status_size))
                        return false;
                }
            }
        } else if (opcode == 199) {
            if (!vm_pop_count(process, 1, status, status_size)) return false;
        } else if (opcode == 205) {
            unsigned index;
            struct agds_vm_mouse_area *area = NULL;

            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name3, sizeof(name3)) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name2, sizeof(name2)) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            for (index = 0; index < AGDS_VM_MOUSE_AREAS; index++) {
                if (!vm_mouse_areas[index].used) {
                    area = &vm_mouse_areas[index];
                    break;
                }
            }
            if (area == NULL) {
                rb->strlcpy(status, "AGDS mouse-area limit reached",
                            status_size);
                return false;
            }
            rb->memset(area, 0, sizeof(*area));
            if (!vm_load_region(name, &area->region,
                                status, status_size)) return false;
            rb->strlcpy(area->on_enter, name2, sizeof(area->on_enter));
            rb->strlcpy(area->on_leave, name3, sizeof(area->on_leave));
            area->used = true;
            area->enabled = true;
                if (!vm_push(process, (int32_t)index,
                         status, status_size)) return false;
        } else if (opcode == 206) {
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_pop(process, &second, status, status_size)) return false;
            if (second >= 0 && second < AGDS_VM_MOUSE_AREAS &&
                vm_mouse_areas[second].used) {
                vm_mouse_areas[second].enabled = first != 0;
                if (!vm_mouse_areas[second].enabled)
                    vm_mouse_areas[second].visible = false;
            }
        } else if (opcode == 207) {
            /* SetRain(process name); the retail-compatible engine stubs it. */
            if (!vm_pop_count(process, 1, status, status_size)) return false;
        } else if (opcode == 214) {
            /* AddSampleToSoundGroup(group, sample): retail AGDS stubs it. */
            if (!vm_pop_count(process, 2, status, status_size)) return false;
        } else if (opcode == 215) {
            if (!vm_pop_count(process, 1, status, status_size)) return false;
        } else if (opcode == 216 || opcode == 217) {
            /* Character/animation walk sounds: id, frame, sound group. */
            if (!vm_pop_count(process, 3, status, status_size)) return false;
        } else if (opcode == 218) {
            /* SetRainDensity(density, transition). */
            if (!vm_pop_count(process, 2, status, status_size)) return false;
        } else if (opcode == 219) {
            struct agds_vm_character *character;
            struct scummvm_agds_region destination;

            if (!vm_pop(process, &third, status, status_size) ||
                !vm_pop(process, &second, status, status_size) ||
                !vm_string(object, second, name2, sizeof(name2)) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name)) ||
                !vm_load_region(name2, &destination,
                                status, status_size)) return false;
            character = vm_find_character(name, false);
            if (character != NULL) {
                if (!vm_character_begin_movement(
                        character, &destination, (int16_t)third, true,
                        status, status_size))
                    return false;
                if (character->enabled)
                    return vm_suspend_for_character(
                        process, character, status, status_size);
            }
        } else if (opcode == 220) {
            struct agds_vm_character *character;

            if (!vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            character = vm_find_character(name, false);
            if (character != NULL) {
                character->final_direction = (int16_t)second;
                character->phase = -1;
                if (character->moving) {
                    unsigned ending =
                        character->last_stride_animation == 2 ? 4u : 3u;

                    character->stopping = true;
                    character->path_count = 0;
                    character->path_index = 0;
                    if (!vm_character_set_locomotion_clip(
                            character, ending, status, status_size))
                        return false;
                    return vm_suspend_for_character(
                        process, character, status, status_size);
                }
                if (second != -1)
                    character->direction = (int16_t)second;
                vm_sync_character_object(character);
            }
        } else if (opcode == 227) {
            struct scummvm_agds_font *font;

            /* Retail LoadFont(slot, descriptor) snapshots the process's
             * authored height, colors and style flags into one of 100 global
             * font records. main.104c registers NiBiRu's nine slots here. */
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name)) ||
                !vm_pop(process, &second, status, status_size)) return false;
            if (second < 0 || second >= AGDS_VM_MAX_FONTS ||
                process->tile_height <= 0) {
                rb->strlcpy(status, "AGDS font registration invalid",
                            status_size);
                return false;
            }
            font = &vm_fonts[second];
            rb->memset(font, 0, sizeof(*font));
            rb->strlcpy(font->descriptor, name, sizeof(font->descriptor));
            font->authored_height = (uint16_t)process->tile_height;
            font->primary_color = process->text_color;
            font->secondary_color = process->text_secondary_color;
            font->flags = process->text_flags;
            font->used = true;
            DEBUGF("agds: font slot %ld %s height=%u flags=%u\n",
                   (long)second, font->descriptor,
                   (unsigned)font->authored_height,
                   (unsigned)font->flags);
        } else if (opcode == 224) {
            struct agds_vm_global *global;

            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            global = vm_find_global(name, true);
            if (global == NULL) return false;
            global->value = 0;
            rb->strlcpy(vm_dialog_npc_notify, name,
                        sizeof(vm_dialog_npc_notify));
        } else if (opcode == 244) {
            struct agds_vm_global *tell_notify;
            struct agds_vm_global *direction_notify;

            if (!vm_pop(process, &second, status, status_size) ||
                !vm_string(object, second, name2, sizeof(name2)) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            tell_notify = vm_find_global(name, true);
            direction_notify = vm_find_global(name2, true);
            if (tell_notify == NULL || direction_notify == NULL)
                return false;
            tell_notify->value = 0;
            direction_notify->value = 0;
            rb->strlcpy(vm_dialog_character_notify, name,
                        sizeof(vm_dialog_character_notify));
            rb->strlcpy(vm_dialog_direction_notify, name2,
                        sizeof(vm_dialog_direction_notify));
        } else if (opcode == 254) {
            struct agds_vm_character *character;
            uint16_t frames;
            uint16_t period_ticks;

            if (!vm_pop(process, &second, status, status_size) ||
                !vm_string(object, second, name2, sizeof(name2)) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name)) ||
                !vm_character_animation_frames(name2, &frames, &period_ticks,
                                               status, status_size))
                return false;
            character = vm_find_character(name, false);
            if (character != NULL) {
                character->animation_frames = frames;
                character->phase_ticks = 0;
                character->phase_period_ticks = period_ticks;
                character->animation_elapsed_ticks = 0;
                character->animation_pose_frame = 0;
                character->phase = 0;
                character->render_owner =
                    AGDS_VM_CHARACTER_RENDER_NAMED_ANIMATION;
                rb->strlcpy(character->active_animation, name2,
                            sizeof(character->active_animation));
                vm_sync_character_object(character);
            }
        } else if (opcode == 253) {
            struct agds_vm_character *character;

            /* AGDS SetCharacterAnimation(character, descriptor, slot).
             * NiBiRu's 108a.1351 installs the authored start, left, right,
             * end-left and end-right resources in slots 0 through 4. */
            if (!vm_pop(process, &third, status, status_size) ||
                !vm_pop(process, &second, status, status_size) ||
                !vm_string(object, second, name2, sizeof(name2)) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            character = vm_find_character(name, false);
            if (character != NULL && third >= 0 &&
                third < AGDS_VM_CHARACTER_ANIMATIONS) {
                rb->strlcpy(character->animations[third], name2,
                            sizeof(character->animations[third]));
                DEBUGF("agds: character %s animation %ld = %s\n",
                       character->name, (long)third,
                       character->animations[third]);
                if (third == 0)
                    vm_sync_character_object(character);
            }
        } else if (opcode == 234) {
            if (!vm_pop_count(process, 1, status, status_size) ||
                !vm_push(process, -1, status, status_size)) return false;
        } else if (opcode == 240) {
            if (!vm_pop(process, &third, status, status_size) ||
                !vm_string(object, third, name3, sizeof(name3)) ||
                !vm_pop(process, &second, status, status_size) ||
                !vm_string(object, second, name2, sizeof(name2)) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name)) ||
                !vm_dialog_load(process->name, name2, name3,
                                status, status_size) ||
                !vm_run_object(name, depth + 1, status, status_size))
                return false;
            process->waiting_dialog = true;
            DEBUGF("agds: dialog %s loaded by %s\n", name2, process->name);
            return vm_suspend(process, status, status_size);
        } else if (opcode == 247) {
            if (!vm_pop(process, &first, status, status_size)) return false;
            process->film_subtitles_resource = (int16_t)first;
        } else if (opcode == 248) {
            if (!vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size)) return false;
            vm_save_picture_width = first > 0 && first <= UINT16_MAX ?
                (uint16_t)first : 0;
            vm_save_picture_height = second > 0 && second <= UINT16_MAX ?
                (uint16_t)second : 0;
            DEBUGF("agds: save picture buffer %ux%u\n",
                   (unsigned)vm_save_picture_width,
                   (unsigned)vm_save_picture_height);
        } else if (opcode == 249) {
            bool exists;

            if (!vm_pop(process, &first, status, status_size)) return false;
            exists = vm_save_slot_exists(first);
            if (exists) {
                rb->snprintf(screen_object->picture_entry,
                             sizeof(screen_object->picture_entry),
                             "savepic.%ld", (long)first);
            }
            if (!vm_push(process, exists, status, status_size)) return false;
        } else if (opcode == 250) {
            char text[48];
            bool exists;

            if (!vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size)) return false;
            exists = vm_save_slot_exists(first);
            if (exists) {
                rb->snprintf(text, sizeof(text), "Position %ld", (long)first);
                if (!vm_set_object_text(process->object_index, text, process,
                                        status, status_size)) return false;
            }
            if (!vm_push(process, exists, status, status_size)) return false;
        } else if (opcode == 255) {
            int index;

            if (!vm_pop(process, &third, status, status_size) ||
                !vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            index = vm_find_screen_object(name);
            if (index >= 0) {
                vm_scene->objects[index].animation_field_392 = second;
                vm_scene->objects[index].animation_field_396 = third;
            } else {
                vm_scene->animation_field_392 = second;
                vm_scene->animation_field_396 = third;
            }
        } else if (opcode == 241 || opcode == 276) {
            int32_t color;

            if (!vm_pop(process, &third, status, status_size) ||
                !vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size)) return false;
            color = (first & 0xff) | ((second & 0xff) << 8) |
                    ((third & 0xff) << 16);
            if (opcode == 241)
                process->text_color = color;
            else
                process->text_secondary_color = color;
        } else if (opcode == 275) {
            if (!vm_pop(process, &first, status, status_size)) return false;
            process->text_flags = (uint16_t)first;
        } else if (opcode == 285) {
            int index;

            /* Retail ScreenSetObjectZ resolves the current-screen object,
             * stores this value in its draw record, then removes/reinserts
             * that record in the ordered visual list. */
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_pop(process, &second, status, status_size) ||
                !vm_string(object, second, name, sizeof(name))) return false;
            index = vm_find_screen_object(name);
            if (index >= 0 && vm_scene->objects[index].alive)
                vm_scene->objects[index].z = (int16_t)first;
        } else if (opcode == 288) {
            int index;

            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name2, sizeof(name2)) ||
                !vm_pop(process, &second, status, status_size) ||
                !vm_string(object, second, name, sizeof(name))) return false;
            index = vm_find_screen_object(name);
            if (index >= 0 && vm_scene->objects[index].alive)
                rb->strlcpy(vm_scene->objects[index].alpha_picture_entry,
                            name2,
                            sizeof(vm_scene->objects[index].
                                   alpha_picture_entry));
        } else if (opcode == 286 || opcode == 287) {
            char text[AGDS_VM_OBJECT_TEXT_SIZE];
            char ignored_status[96];
            int index;

            if (!vm_pop(process, &first, status, status_size)) return false;
            if (opcode == 286 &&
                !vm_string(object, first, name2, sizeof(name2))) return false;
            if (!vm_pop(process, &second, status, status_size) ||
                !vm_string(object, second, name, sizeof(name))) return false;
            index = vm_find_screen_object(name);
            if (index < 0 || !vm_scene->objects[index].alive)
                continue;
            if (opcode == 286) {
                /* The retail handler ignores a missing text resource and
                 * continues the owning process after logging it. */
                if (!scummvm_agds_read_text(vm_target, name2, text,
                                            sizeof(text), ignored_status,
                                            sizeof(ignored_status))) {
                    DEBUGF("agds: object text %s ignored: %s\n",
                           name2, ignored_status);
                    continue;
                }
            } else {
                rb->snprintf(text, sizeof(text), "%ld", (long)first);
            }
            if (!vm_set_object_text((unsigned)index, text, process,
                                    status, status_size)) return false;
            DEBUGF("agds: object text %s at %d,%d flags=%u\n",
                   name, (int)process->animation_x,
                   (int)process->animation_y,
                   (unsigned)process->text_flags);
        } else if (opcode == 281 || opcode == 282) {
            struct agds_vm_character *character;
            struct agds_vm_animation *animation;

            if (!vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            character = vm_find_character(name, false);
            animation = vm_find_animation(name, false);
            if (second >= 0 && second < AGDS_VM_ANIMATION_CONTROLS) {
                if (character != NULL)
                    character->animation_controls[second] = opcode == 281;
                else if (animation != NULL)
                    animation->controls[second] = opcode == 281;
            }
        } else if (opcode == 283) {
            struct agds_vm_character *character;
            struct agds_vm_animation *animation;
            int32_t last_phase = -1;

            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            character = vm_find_character(name, false);
            animation = vm_find_animation(name, false);
            if (character != NULL && character->animation_frames != 0)
                last_phase = (int32_t)character->animation_frames - 1;
            else if (animation != NULL && animation->frame_count != 0)
                last_phase = (int32_t)animation->frame_count - 1;
            if (!vm_push(process, last_phase, status, status_size))
                return false;
        } else if (opcode == 260 || opcode == 261 || opcode == 262) {
            struct agds_vm_animation *animation;

            if (!vm_pop(process, &third, status, status_size)) return false;
            if (opcode != 261 &&
                !vm_pop(process, &second, status, status_size)) return false;
            if (!vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            animation = vm_find_animation(name, false);
            if (animation != NULL) {
                if (opcode == 260) {
                    animation->field_392 = second;
                    animation->field_396 = third;
                } else if (opcode == 261) {
                    animation->field_3ae = third;
                } else {
                    animation->field_38a = second;
                    animation->field_38e = third;
                }
            }
        } else if (opcode == 252) {
            int32_t lens;
            int32_t unknown;
            int32_t camera;

            /* Retail Process::setCamera pops lens, unknown, camera.  Keep the
             * authored integer triplet in scene state; the character
             * projector consumes the same ordering as the retail screen
             * camera structure. */
            if (!vm_pop(process, &lens, status, status_size) ||
                !vm_pop(process, &unknown, status, status_size) ||
                !vm_pop(process, &camera, status, status_size))
                return false;
            vm_scene->camera_pitch = (int16_t)camera;
            vm_scene->camera_distance = (int16_t)unknown;
            vm_scene->camera_fov = (int16_t)lens;
            vm_scene->camera_set = true;
            DEBUGF("agds: SetCamera %ld %ld %ld on %s\n",
                   (long)camera, (long)unknown, (long)lens,
                   vm_scene->name);
        } else if (opcode == 264 || opcode == 265 || opcode == 266) {
            int32_t color;

            if (!vm_pop(process, &third, status, status_size) ||
                !vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size)) return false;
            color = (first & 0xff) | ((second & 0xff) << 8) |
                    ((third & 0xff) << 16);
            if (opcode == 264)
                process->light_ambient_color = color;
            else if (opcode == 265)
                process->light_diffuse_color = color;
            else
                process->light_specular_color = color;
        } else if (opcode == 267) {
            if (!vm_pop(process, &third, status, status_size) ||
                !vm_pop(process, &second, status, status_size) ||
                !vm_pop(process, &first, status, status_size)) return false;
            process->light_x = (int16_t)first;
            process->light_y = (int16_t)second;
            process->light_z = (int16_t)third;
        } else if (opcode == 251) {
            if (!vm_pop_count(process, 2, status, status_size) ||
                !vm_pop(process, &first, status, status_size) ||
                !vm_string(object, first, name, sizeof(name))) return false;
            rb->strlcpy(screen_object->character_definition, name,
                        sizeof(screen_object->character_definition));
            process->model_animation = true;
        } else if (opcode == 235) {
            if (!vm_pop_count(process, 3, status, status_size)) return false;
        } else if (opcode == 263) {
            if (!vm_pop_count(process, 2, status, status_size)) return false;
        } else {
            rb->snprintf(status, status_size,
                         "AGDS %.20s opcode %u pending at %u",
                         process->name, (unsigned)opcode,
                         (unsigned)process->ip);
            return false;
        }
    }
    if (instructions >= AGDS_VM_INSTRUCTION_LIMIT) {
        rb->snprintf(status, status_size,
                     "AGDS %.24s instruction limit at %u",
                     process->name, (unsigned)process->ip);
        return false;
    }
    return true;
}

bool scummvm_agds_vm_start_screen(
    const struct scummvm_target *target, const char *name,
    struct scummvm_agds_scene_state *scene,
    char *status, size_t status_size)
{
    scummvm_agds_vm_reset();
    vm_target = target;
    vm_scene = scene;
    rb->memset(scene, 0, sizeof(*scene));
    rb->strlcpy(scene->name, name, sizeof(scene->name));
    scene->user_enabled = true;
    if (!vm_run_object(name, 0, status, status_size) ||
        !vm_apply_screen_patch_refs(status, status_size) ||
        !vm_attach_visible_characters(status, status_size)) {
        scummvm_agds_vm_reset();
        return false;
    }
    vm_last_tick = *rb->current_tick;
    rb->snprintf(status, status_size,
                 "AGDS screen %.20s: %u scripted objects",
                 name, (unsigned)scene->object_count);
    return true;
}

bool scummvm_agds_vm_change_screen(
    const char *name, char *status, size_t status_size)
{
    unsigned index;

    if (vm_target == NULL || vm_scene == NULL || name == NULL ||
        name[0] == '\0') {
        rb->strlcpy(status, "AGDS cannot change an inactive screen",
                    status_size);
        return false;
    }
    if (rb->strcmp(vm_scene->name, name))
        rb->strlcpy(vm_previous_screen, vm_scene->name,
                    sizeof(vm_previous_screen));
    /* Persistent actor coordinates belong to the previous room until its
     * entry script supplies a position.  Do not expose them on a new
     * background while that script is suspended or waiting for dialogue. */
    for (index = 0; index < AGDS_VM_CHARACTERS; index++)
        vm_characters[index].scene_positioned = false;
    rb->memset(vm_scene, 0, sizeof(*vm_scene));
    rb->memset(vm_object_texts, 0, sizeof(vm_object_texts));
    rb->strlcpy(vm_scene->name, name, sizeof(vm_scene->name));
    vm_scene->user_enabled = true;
    vm_suspended_count = 0;
    vm_tell_voice = false;
    vm_scene_animation_request = 0;
    vm_audio_read = 0;
    vm_audio_count = 0;
    rb->memset(vm_animations, 0, sizeof(vm_animations));
    vm_next_screen[0] = '\0';
    rb->memset(&vm_dialog, 0, sizeof(vm_dialog));
    vm_dialog.current_sound = -1;
    rb->memset(vm_key_handlers, 0, sizeof(vm_key_handlers));
    vm_dialog_npc_notify[0] = '\0';
    vm_dialog_character_notify[0] = '\0';
    vm_dialog_direction_notify[0] = '\0';
    for (index = 0; index < AGDS_VM_MOUSE_AREAS; index++)
        vm_mouse_areas[index].visible = false;
    if (!vm_run_object(name, 0, status, status_size) ||
        !vm_apply_screen_patch_refs(status, status_size) ||
        !vm_attach_visible_characters(status, status_size))
        return false;
    vm_apply_saved_scene_state();
    vm_last_tick = *rb->current_tick;
    rb->snprintf(status, status_size,
                 "AGDS screen %.20s: %u scripted objects",
                 name, (unsigned)vm_scene->object_count);
    return true;
}

bool scummvm_agds_vm_run_object_named(
    const char *name, char *status, size_t status_size)
{
    if (vm_target == NULL || vm_scene == NULL || name == NULL ||
        name[0] == '\0') {
        rb->strlcpy(status, "AGDS cannot run object without a screen",
                    status_size);
        return false;
    }
    return vm_run_object(name, 0, status, status_size);
}

bool scummvm_agds_vm_tick(char *status, size_t status_size)
{
    unsigned count;
    unsigned index;
    long now = *rb->current_tick;
    unsigned elapsed = MIN((unsigned long)(now - vm_last_tick),
                           (unsigned long)HZ);
    unsigned tick_scale = elapsed;

    vm_last_tick = now;
#ifdef SIMULATOR
    tick_scale *= vm_test_speed;
#endif

    if (vm_target == NULL || vm_scene == NULL)
        return true;
    for (index = 0; index < AGDS_VM_ANIMATIONS; index++) {
        struct agds_vm_animation *animation = &vm_animations[index];
        struct agds_vm_global *phase;

        if (!animation->used || !animation->active ||
            animation->frame_count == 0)
            continue;
        phase = vm_find_global(animation->phase_var, true);
        if (phase == NULL) {
            rb->strlcpy(status, "AGDS animation variable limit reached",
                        status_size);
            return false;
        }
        animation->phase_accumulator += 1000000u * tick_scale;
        while (animation->active &&
               animation->phase_accumulator >= animation->period_scaled) {
            animation->phase_accumulator -= animation->period_scaled;
            animation->frame++;
            if (animation->frame >= animation->frame_count) {
                animation->active = false;
                phase->value = -1;
                DEBUGF("agds: animation complete %s (%u phases)\n",
                       animation->descriptor,
                       (unsigned)animation->frame_count);
            } else {
                phase->value = animation->frame;
            }
        }
    }
    for (index = 0; index < AGDS_VM_ANIMATIONS; index++) {
        if (vm_animations[index].used && vm_animations[index].frame_count)
            vm_sync_model_animation(&vm_animations[index]);
    }
    for (index = 0; index < AGDS_VM_CHARACTERS; index++) {
        struct agds_vm_character *character = &vm_characters[index];
        unsigned phase_period = character->phase_period_ticks != 0 ?
            character->phase_period_ticks :
            MAX(1u, (15u * HZ + 99u) / 100u);

        if (character->used && character->moving) {
            unsigned movement_tick;

            for (movement_tick = 0;
                 movement_tick < tick_scale && character->moving;
                 movement_tick++) {
                if (!vm_character_tick_movement(character,
                                                status, status_size))
                    return false;
            }
            continue;
        }
        if (!character->used || character->phase < 0 ||
            character->animation_frames == 0)
            continue;
        character->animation_elapsed_ticks += tick_scale;
        {
            uint32_t pose_frame =
                character->animation_elapsed_ticks * 24u / (uint32_t)HZ;
            if (pose_frame > UINT16_MAX)
                pose_frame = UINT16_MAX;
            if (character->animation_pose_frame != (uint16_t)pose_frame) {
                character->animation_pose_frame = (uint16_t)pose_frame;
                vm_sync_character_object(character);
            }
        }
        character->phase_ticks += tick_scale;
        if (character->phase_ticks < phase_period)
            continue;
        character->phase += character->phase_ticks / phase_period;
        character->phase_ticks %= phase_period;
        if ((uint16_t)character->phase >= character->animation_frames) {
            character->phase = -1;
            character->animation_frames = 0;
            character->animation_elapsed_ticks = 0;
            character->animation_pose_frame = 0;
            character->active_animation[0] = '\0';
            character->render_owner = AGDS_VM_CHARACTER_RENDER_IDLE;
            DEBUGF("agds: character animation complete %s\n",
                   character->name);
        }
        vm_sync_character_object(character);
    }
    if (!vm_dialog_tick(status, status_size))
        return false;
    count = vm_suspended_count;
    rb->memcpy(vm_pending, vm_suspended,
               count * sizeof(vm_pending[0]));
    vm_suspended_count = 0;
    for (index = 0; index < count; index++) {
        struct agds_vm_character *waiting_character = NULL;
        bool owner_alive;

        if (vm_pending[index].inventory_slot_plus_one != 0) {
            unsigned slot = vm_pending[index].inventory_slot_plus_one - 1u;

            owner_alive = slot < AGDS_VM_INVENTORY_SLOTS &&
                          vm_inventory[slot].used;
        } else {
            owner_alive = vm_pending[index].object_index <
                              vm_scene->object_count &&
                          vm_scene->objects[
                              vm_pending[index].object_index].alive;
        }
        if (!owner_alive && !vm_pending[index].survive_removal)
            continue;
        if (vm_pending[index].waiting_character[0] != '\0')
            waiting_character = vm_find_character(
                vm_pending[index].waiting_character, false);
        if (waiting_character != NULL && waiting_character->moving) {
            if (!vm_suspend(&vm_pending[index], status, status_size))
                return false;
        } else if (vm_pending[index].waiting_film && vm_film_active) {
            if (!vm_suspend(&vm_pending[index], status, status_size))
                return false;
        } else if (vm_pending[index].waiting_tell && vm_tell_voice) {
            if (!vm_suspend(&vm_pending[index], status, status_size))
                return false;
        } else if (vm_pending[index].timer > elapsed) {
            /* Timers use elapsed Rockbox ticks, never rendered frames.
             * Storage and LCD work must not stretch dialogue/typing delays.
             * Keep process execution bounded to one dispatch per frame. */
            vm_pending[index].timer -= elapsed;
            if (!vm_suspend(&vm_pending[index], status, status_size))
                return false;
        } else if (vm_pending[index].waiting_dialog && vm_dialog.active) {
            if (!vm_suspend(&vm_pending[index], status, status_size))
                return false;
        } else {
            struct agds_vm_global *notify;

            vm_pending[index].timer = 0;
            vm_pending[index].waiting_character[0] = '\0';
            vm_pending[index].waiting_film = false;

            if (vm_pending[index].waiting_tell) {
                vm_pending[index].waiting_tell = false;
                vm_dialog.text_visible = false;
                if (!vm_dialog_set_var(0)) {
                    rb->strlcpy(status, "AGDS dialog variable limit reached",
                                status_size);
                    return false;
                }
                notify = vm_find_global(
                    vm_dialog.text_npc ? vm_dialog_npc_notify :
                                         vm_dialog_character_notify,
                    false);
                if (notify != NULL)
                    notify->value = 1;
            }
            vm_pending[index].waiting_dialog = false;
            if (!vm_run_process(&vm_pending[index], 0,
                                   status, status_size)) {
                return false;
            }
        }
    }
    return true;
}

bool scummvm_agds_vm_pointer(
    int16_t x, int16_t y, char *status, size_t status_size)
{
    unsigned index;

    if (vm_target == NULL || vm_scene == NULL)
        return true;
    vm_saved_mouse_x = x;
    vm_saved_mouse_y = y;
    for (index = 0; index < AGDS_VM_MOUSE_AREAS; index++) {
        struct agds_vm_mouse_area *area = &vm_mouse_areas[index];

        if (!area->used || !area->visible ||
            (!vm_mouse_disabled && area->enabled &&
             vm_region_contains(&area->region, x, y)))
            continue;
        area->visible = false;
        if (area->on_leave[0] != '\0' &&
            !vm_run_object(area->on_leave, 0, status, status_size))
            return false;
    }
    for (index = 0; index < AGDS_VM_MOUSE_AREAS; index++) {
        struct agds_vm_mouse_area *area = &vm_mouse_areas[index];

        if (vm_mouse_disabled || !area->used || !area->enabled || area->visible ||
            !vm_region_contains(&area->region, x, y))
            continue;
        area->visible = true;
        if (area->on_enter[0] != '\0' &&
            !vm_run_object(area->on_enter, 0, status, status_size))
            return false;
        break;
    }
    return true;
}

static bool vm_object_contains(const struct scummvm_agds_screen_object *object,
                               int32_t x, int32_t y)
{
    if (object->picture_width > 0 && object->picture_height > 0 &&
        x >= object->x && y >= object->y &&
        x < object->x + object->picture_width &&
        y < object->y + object->picture_height)
        return true;
    return !object->ignore_region &&
           vm_region_contains(&object->region,
                              x - object->region_offset_x,
                              y - object->region_offset_y);
}

bool scummvm_agds_vm_click(
    int16_t x, int16_t y, bool look,
    char *status, size_t status_size)
{
    int index;

    if (vm_target == NULL || vm_scene == NULL)
        return true;
    if (!vm_scene->user_enabled) {
#ifdef SIMULATOR
        DEBUGF("agds: click ignored while user disabled at %d,%d\n",
               (int)x, (int)y);
#endif
        return true;
    }
    for (index = (int)vm_scene->object_count - 1; index >= 0; index--) {
        struct scummvm_agds_screen_object *object =
            &vm_scene->objects[index];
        uint16_t handler = look ? object->look_handler :
                                  object->click_handler;
        struct agds_vm_process process;

        if (!object->alive || !object->visible || handler == 0 ||
            !vm_object_contains(object, x, y))
            continue;
        rb->memset(&process, 0, sizeof(process));
        rb->strlcpy(process.name, object->name, sizeof(process.name));
        rb->strlcpy(process.code_entry, object->code_entry,
                    sizeof(process.code_entry));
        process.ip = handler;
        process.object_index = (uint16_t)index;
        vm_reset_process_state(&process);
        if (!vm_run_process(&process, 0, status, status_size))
            return false;
        DEBUGF("agds: %s handler %s at %d,%d\n",
               look ? "look" : "click", object->name, (int)x, (int)y);
        return true;
    }
    return true;
}

bool scummvm_agds_vm_key(
    const char *key, bool *handled,
    char *status, size_t status_size)
{
    unsigned object_index;

    if (handled != NULL)
        *handled = false;
    if (vm_target == NULL || vm_scene == NULL || !vm_scene->user_enabled ||
        key == NULL || key[0] == '\0')
        return true;
    /* Retail screen lookup walks live screen objects in insertion order and
     * compares registered key names without case sensitivity. */
    for (object_index = 0; object_index < vm_scene->object_count;
         object_index++) {
        struct scummvm_agds_screen_object *object =
            &vm_scene->objects[object_index];
        unsigned index;

        if (!object->alive)
            continue;
        for (index = 0; index < AGDS_VM_KEY_HANDLERS; index++) {
            struct agds_vm_key_handler *handler = &vm_key_handlers[index];
            struct agds_vm_process process;

            if (!handler->used || handler->object_index != object_index ||
                rb->strcasecmp(handler->key, key))
                continue;
            rb->memset(&process, 0, sizeof(process));
            rb->strlcpy(process.name, object->name, sizeof(process.name));
            rb->strlcpy(process.code_entry, object->code_entry,
                        sizeof(process.code_entry));
            process.ip = handler->ip;
            process.object_index = (uint16_t)object_index;
            vm_reset_process_state(&process);
            if (!vm_run_process(&process, 0, status, status_size))
                return false;
            if (handled != NULL)
                *handled = true;
            DEBUGF("agds: key handler %s %s\n", key, object->name);
            return true;
        }
    }
    return true;
}

bool scummvm_agds_vm_take_next_screen(char *name, size_t name_size)
{
    if (name == NULL || name_size == 0 || vm_next_screen[0] == '\0')
        return false;
    rb->strlcpy(name, vm_next_screen, name_size);
    vm_next_screen[0] = '\0';
    return true;
}

bool scummvm_agds_vm_take_quit(void)
{
    bool requested = vm_quit_requested;

    vm_quit_requested = false;
    vm_mouse_disabled = false;
    return requested;
}

bool scummvm_agds_vm_set_global(const char *name, int32_t value)
{
    struct agds_vm_global *global;

    if (name == NULL || name[0] == '\0')
        return false;
    global = vm_find_global(name, true);
    if (global == NULL)
        return false;
    global->value = value;
    return true;
}

static bool vm_parse_save_payload(struct agds_vm_save_cursor *cursor,
                                  bool apply, char *screen,
                                  size_t screen_size,
                                  char *status, size_t status_size)
{
    char name[AGDS_ADB_NAME_SIZE];
    uint16_t count;
    unsigned index;

    save_get_string(cursor, name, AGDS_ADB_NAME_SIZE);
    if (screen != NULL && screen_size > 0)
        rb->strlcpy(screen, name, screen_size);

    count = save_get_u16(cursor);
    if (count > AGDS_VM_GLOBALS)
        cursor->valid = false;
    if (apply)
        rb->memset(vm_globals, 0, sizeof(vm_globals));
    for (index = 0; index < count && cursor->valid; index++) {
        int32_t value;

        save_get_string(cursor, name, AGDS_ADB_NAME_SIZE);
        value = (int32_t)save_get_u32(cursor);
        if (apply) {
            rb->strlcpy(vm_globals[index].name, name,
                        sizeof(vm_globals[index].name));
            vm_globals[index].value = value;
            vm_globals[index].used = true;
        }
    }

    count = save_get_u16(cursor);
    if (count > AGDS_VM_SYSTEM_VARS)
        cursor->valid = false;
    if (apply)
        rb->memset(vm_system_vars, 0, sizeof(vm_system_vars));
    for (index = 0; index < count && cursor->valid; index++) {
        int32_t value;

        save_get_string(cursor, name, AGDS_ADB_NAME_SIZE);
        value = (int32_t)save_get_u32(cursor);
        if (apply) {
            rb->strlcpy(vm_system_vars[index].name, name,
                        sizeof(vm_system_vars[index].name));
            vm_system_vars[index].value = value;
            vm_system_vars[index].used = true;
        }
    }

    count = save_get_u16(cursor);
    if (count > AGDS_VM_INVENTORY_SLOTS)
        cursor->valid = false;
    if (apply)
        rb->memset(vm_inventory, 0, sizeof(vm_inventory));
    for (index = 0; index < count && cursor->valid; index++) {
        char inventory_name[AGDS_VM_INVENTORY_NAME_SIZE];
        uint32_t item_count;

        save_get_string(cursor, inventory_name, sizeof(inventory_name));
        item_count = save_get_u32(cursor);
        if (item_count == 0)
            cursor->valid = false;
        if (apply && cursor->valid) {
            rb->strlcpy(vm_inventory[index].name, inventory_name,
                        sizeof(vm_inventory[index].name));
            vm_inventory[index].count = item_count;
            vm_inventory[index].used = true;
        }
    }

    count = save_get_u16(cursor);
    if (count > AGDS_VM_CHARACTERS)
        cursor->valid = false;
    if (apply) {
        rb->memset(vm_saved_characters, 0, sizeof(vm_saved_characters));
        vm_saved_character_count = (uint8_t)count;
    }
    for (index = 0; index < count && cursor->valid; index++) {
        struct agds_vm_saved_character saved;

        rb->memset(&saved, 0, sizeof(saved));
        save_get_string(cursor, saved.name, sizeof(saved.name));
        saved.x = (int16_t)save_get_u16(cursor);
        saved.y = (int16_t)save_get_u16(cursor);
        saved.direction = (int16_t)save_get_u16(cursor);
        saved.enabled = save_get_u8(cursor) != 0;
        saved.visible = save_get_u8(cursor) != 0;
        if (apply)
            vm_saved_characters[index] = saved;
    }

    count = save_get_u16(cursor);
    if (count > AGDS_VM_MAX_SCREEN_OBJECTS)
        cursor->valid = false;
    if (apply) {
        rb->memset(vm_saved_objects, 0, sizeof(vm_saved_objects));
        vm_saved_object_count = (uint8_t)count;
    }
    for (index = 0; index < count && cursor->valid; index++) {
        struct agds_vm_saved_object saved;

        rb->memset(&saved, 0, sizeof(saved));
        save_get_string(cursor, saved.name, sizeof(saved.name));
        saved.x = (int16_t)save_get_u16(cursor);
        saved.y = (int16_t)save_get_u16(cursor);
        saved.z = (int16_t)save_get_u16(cursor);
        saved.character_direction = (int16_t)save_get_u16(cursor);
        saved.character_phase = (int16_t)save_get_u16(cursor);
        saved.character_pose_frame = save_get_u16(cursor);
        saved.character_render_owner = save_get_u8(cursor);
        saved.visible = save_get_u8(cursor) != 0;
        saved.alive = save_get_u8(cursor) != 0;
        if (saved.character_render_owner >
                AGDS_VM_CHARACTER_RENDER_NAMED_ANIMATION)
            cursor->valid = false;
        if (apply)
            vm_saved_objects[index] = saved;
    }
    if (apply && cursor->valid) {
        for (index = 0; index < AGDS_VM_INVENTORY_SLOTS; index++) {
            if (!vm_inventory[index].used)
                continue;
            rb->strlcpy(vm_inventory[index].object.name,
                        vm_inventory[index].name,
                        sizeof(vm_inventory[index].object.name));
            rb->strlcpy(vm_inventory[index].object.code_entry,
                        vm_inventory[index].name,
                        sizeof(vm_inventory[index].object.code_entry));
            vm_inventory[index].object.alive = true;
            vm_inventory[index].object.visible = true;
            if (!vm_run_inventory_object(index, 0, 0,
                                         status, status_size))
                return false;
        }
        vm_restore_pending = true;
    }
    return cursor->valid && cursor->position == cursor->size;
}

bool scummvm_agds_vm_save_game(
    int slot, char *status, size_t status_size)
{
    struct agds_vm_save_cursor cursor;
    struct scummvm_file file;
    char name[24];
    char temporary_name[28];
    char path[MAX_PATH];
    char temporary_path[MAX_PATH];
    uint16_t count;
    unsigned index;
    uint32_t checksum;

    if (vm_target == NULL || vm_scene == NULL || slot < 0 || slot >= 99) {
        rb->strlcpy(status, "AGDS save slot is invalid", status_size);
        return false;
    }
    rb->memset(vm_save_buffer, 0, sizeof(vm_save_buffer));
    cursor.data = vm_save_buffer;
    cursor.position = 12;
    cursor.size = sizeof(vm_save_buffer);
    cursor.valid = true;
    save_put_string(&cursor, vm_scene->name, AGDS_ADB_NAME_SIZE);

    count = 0;
    for (index = 0; index < AGDS_VM_GLOBALS; index++)
        if (vm_globals[index].used)
            count++;
    save_put_u16(&cursor, count);
    for (index = 0; index < AGDS_VM_GLOBALS; index++) {
        if (!vm_globals[index].used)
            continue;
        save_put_string(&cursor, vm_globals[index].name,
                        sizeof(vm_globals[index].name));
        save_put_u32(&cursor, (uint32_t)vm_globals[index].value);
    }

    count = 0;
    for (index = 0; index < AGDS_VM_SYSTEM_VARS; index++)
        if (vm_system_vars[index].used)
            count++;
    save_put_u16(&cursor, count);
    for (index = 0; index < AGDS_VM_SYSTEM_VARS; index++) {
        if (!vm_system_vars[index].used)
            continue;
        save_put_string(&cursor, vm_system_vars[index].name,
                        sizeof(vm_system_vars[index].name));
        save_put_u32(&cursor, (uint32_t)vm_system_vars[index].value);
    }

    count = (uint16_t)scummvm_agds_vm_inventory_count();
    save_put_u16(&cursor, count);
    for (index = 0; index < AGDS_VM_INVENTORY_SLOTS; index++) {
        if (!vm_inventory[index].used)
            continue;
        save_put_string(&cursor, vm_inventory[index].name,
                        sizeof(vm_inventory[index].name));
        save_put_u32(&cursor, vm_inventory[index].count);
    }

    count = 0;
    for (index = 0; index < AGDS_VM_CHARACTERS; index++)
        if (vm_characters[index].used)
            count++;
    save_put_u16(&cursor, count);
    for (index = 0; index < AGDS_VM_CHARACTERS; index++) {
        const struct agds_vm_character *character = &vm_characters[index];

        if (!character->used)
            continue;
        save_put_string(&cursor, character->name, sizeof(character->name));
        save_put_u16(&cursor, (uint16_t)character->x);
        save_put_u16(&cursor, (uint16_t)character->y);
        save_put_u16(&cursor, (uint16_t)character->direction);
        save_put_u8(&cursor, character->enabled);
        save_put_u8(&cursor, character->visible);
    }

    save_put_u16(&cursor, vm_scene->object_count);
    for (index = 0; index < vm_scene->object_count; index++) {
        const struct scummvm_agds_screen_object *object =
            &vm_scene->objects[index];

        save_put_string(&cursor, object->name, sizeof(object->name));
        save_put_u16(&cursor, (uint16_t)object->x);
        save_put_u16(&cursor, (uint16_t)object->y);
        save_put_u16(&cursor, (uint16_t)object->z);
        save_put_u16(&cursor, (uint16_t)object->character_direction);
        save_put_u16(&cursor, (uint16_t)object->character_phase);
        save_put_u16(&cursor, object->character_pose_frame);
        save_put_u8(&cursor, (uint8_t)object->character_render_owner);
        save_put_u8(&cursor, object->visible);
        save_put_u8(&cursor, object->alive);
    }
    if (!cursor.valid || cursor.position > UINT16_MAX) {
        rb->strlcpy(status, "AGDS save state exceeds bounded buffer",
                    status_size);
        return false;
    }
    rb->memcpy(vm_save_buffer, "NGS1", 4);
    vm_save_buffer[4] = AGDS_VM_SAVE_VERSION;
    vm_save_buffer[5] = 0;
    vm_save_buffer[6] = (unsigned char)cursor.position;
    vm_save_buffer[7] = (unsigned char)(cursor.position >> 8);
    checksum = save_checksum(vm_save_buffer + 12, cursor.position - 12);
    vm_save_buffer[8] = (unsigned char)checksum;
    vm_save_buffer[9] = (unsigned char)(checksum >> 8);
    vm_save_buffer[10] = (unsigned char)(checksum >> 16);
    vm_save_buffer[11] = (unsigned char)(checksum >> 24);

    rb->snprintf(name, sizeof(name), "save.%d", slot);
    rb->snprintf(temporary_name, sizeof(temporary_name), "save.%d.tmp", slot);
    if (!scummvm_make_path(path, sizeof(path), vm_target->savepath, name) ||
        !scummvm_make_path(temporary_path, sizeof(temporary_path),
                           vm_target->savepath, temporary_name) ||
        !scummvm_file_open_save(&file, vm_target, temporary_name,
                                O_WRONLY | O_CREAT | O_TRUNC)) {
        rb->strlcpy(status, "AGDS cannot create save file", status_size);
        return false;
    }
    if (rb->write(file.fd, vm_save_buffer, cursor.position) !=
            (long)cursor.position) {
        scummvm_file_close(&file);
        rb->remove(temporary_path);
        rb->strlcpy(status, "AGDS save write failed", status_size);
        return false;
    }
    scummvm_file_close(&file);
    rb->remove(path);
    if (rb->rename(temporary_path, path) < 0) {
        rb->remove(temporary_path);
        rb->strlcpy(status, "AGDS save commit failed", status_size);
        return false;
    }
    rb->snprintf(status, status_size, "NiBiRu saved in position %d", slot);
    DEBUGF("agds: saved slot %d screen=%s bytes=%u inventory=%u\n",
           slot, vm_scene->name, (unsigned)cursor.position,
           scummvm_agds_vm_inventory_count());
    return true;
}

bool scummvm_agds_vm_load_game(
    int slot, char *screen, size_t screen_size,
    char *status, size_t status_size)
{
    struct scummvm_file file;
    struct agds_vm_save_cursor cursor;
    char name[24];
    long size;
    uint16_t stored_size;
    uint32_t stored_checksum;

    if (vm_target == NULL || screen == NULL || screen_size == 0 ||
        slot < 0 || slot >= 99) {
        rb->strlcpy(status, "AGDS load slot is invalid", status_size);
        return false;
    }
    rb->snprintf(name, sizeof(name), "save.%d", slot);
    if (!scummvm_file_open_save(&file, vm_target, name, O_RDONLY)) {
        rb->snprintf(status, status_size, "NiBiRu position %d is empty", slot);
        return false;
    }
    size = scummvm_file_size(&file);
    if (size < 12 || size > (long)sizeof(vm_save_buffer) ||
        scummvm_file_read(&file, vm_save_buffer, size) != size) {
        scummvm_file_close(&file);
        rb->strlcpy(status, "AGDS save file is truncated", status_size);
        return false;
    }
    scummvm_file_close(&file);
    stored_size = read_u16le(vm_save_buffer + 6);
    stored_checksum = read_u32le(vm_save_buffer + 8);
    if (rb->memcmp(vm_save_buffer, "NGS1", 4) ||
        read_u16le(vm_save_buffer + 4) != AGDS_VM_SAVE_VERSION ||
        stored_size != (uint16_t)size ||
        stored_checksum != save_checksum(vm_save_buffer + 12, size - 12)) {
        rb->strlcpy(status, "AGDS save version or checksum invalid",
                    status_size);
        return false;
    }

    cursor.data = vm_save_buffer;
    cursor.position = 12;
    cursor.size = (size_t)size;
    cursor.valid = true;
    if (!vm_parse_save_payload(&cursor, false, screen, screen_size,
                               status, status_size)) {
        rb->strlcpy(status, "AGDS save payload invalid", status_size);
        return false;
    }
    cursor.position = 12;
    cursor.valid = true;
    if (!vm_parse_save_payload(&cursor, true, screen, screen_size,
                               status, status_size)) {
        rb->strlcpy(status, "AGDS save restore failed", status_size);
        return false;
    }
    rb->snprintf(status, status_size, "NiBiRu loaded position %d", slot);
    DEBUGF("agds: loaded slot %d screen=%s bytes=%ld inventory=%u\n",
           slot, screen, size, scummvm_agds_vm_inventory_count());
    return true;
}

bool scummvm_agds_vm_take_load_request(int *slot)
{
    if (slot == NULL || vm_load_request < 0)
        return false;
    *slot = vm_load_request;
    vm_load_request = -1;
    return true;
}

unsigned scummvm_agds_vm_inventory_count(void)
{
    unsigned count = 0;
    unsigned index;

    for (index = 0; index < AGDS_VM_INVENTORY_SLOTS; index++) {
        if (vm_inventory[index].used)
            count++;
    }
    return count;
}

static int vm_inventory_slot_at(unsigned index)
{
    unsigned slot;

    for (slot = 0; slot < AGDS_VM_INVENTORY_SLOTS; slot++) {
        if (!vm_inventory[slot].used)
            continue;
        if (index-- == 0)
            return (int)slot;
    }
    return -1;
}

bool scummvm_agds_vm_inventory_enabled(void)
{
    return vm_inventory_enabled;
}

bool scummvm_agds_vm_inventory_select(unsigned index)
{
    int slot = vm_inventory_slot_at(index);

    if (!vm_inventory_enabled || slot < 0) {
        vm_current_inventory_slot = -1;
        return false;
    }
    vm_current_inventory_slot = (int8_t)slot;
    return true;
}

void scummvm_agds_vm_inventory_deselect(void)
{
    vm_current_inventory_slot = -1;
}

bool scummvm_agds_vm_inventory_item(
    unsigned index, struct scummvm_agds_inventory_item *item)
{
    unsigned slot;

    if (item == NULL)
        return false;
    slot = (unsigned)vm_inventory_slot_at(index);
    if (slot >= AGDS_VM_INVENTORY_SLOTS)
        return false;
    rb->memset(item, 0, sizeof(*item));
    rb->strlcpy(item->name, vm_inventory[slot].name,
                sizeof(item->name));
    rb->strlcpy(item->picture_entry,
                vm_inventory[slot].object.inventory_picture_entry,
                sizeof(item->picture_entry));
    rb->strlcpy(item->text_entry,
                vm_inventory[slot].object.inventory_text_entry,
                sizeof(item->text_entry));
    item->count = vm_inventory[slot].count;
    item->picture_tile = vm_inventory[slot].object.picture_tile;
    item->tile_width = vm_inventory[slot].object.tile_width;
    item->tile_height = vm_inventory[slot].object.tile_height;
    return true;
}

bool scummvm_agds_vm_inventory_contains(const char *name)
{
    return name != NULL && name[0] != '\0' && vm_inventory_find(name) >= 0;
}

static uint16_t vm_use_object_handler(
    const struct scummvm_agds_screen_object *source, uint16_t target_id)
{
    unsigned index;

    for (index = 0; index < source->use_object_handler_count; index++) {
        if (source->use_object_handlers[index].target_id == target_id)
            return source->use_object_handlers[index].ip;
    }
    return 0;
}

bool scummvm_agds_vm_inventory_click(
    unsigned index, bool look, char *status, size_t status_size)
{
    int slot = vm_inventory_slot_at(index);
    uint16_t handler;

    if (slot < 0)
        return true;
    handler = look ? vm_inventory[slot].object.look_handler :
                     vm_inventory[slot].object.click_handler;
    if (handler == 0)
        return true;
    if (!vm_run_inventory_object((unsigned)slot, handler, 0,
                                 status, status_size))
        return false;
    DEBUGF("agds: inventory %s handler %s\n",
           look ? "look" : "use", vm_inventory[slot].name);
    return true;
}

bool scummvm_agds_vm_inventory_use(
    unsigned source_index, unsigned target_index,
    char *status, size_t status_size)
{
    int source_slot = vm_inventory_slot_at(source_index);
    int target_slot = vm_inventory_slot_at(target_index);
    uint16_t handler;

    if (source_slot < 0 || target_slot < 0)
        return true;
    handler = vm_use_object_handler(
        &vm_inventory[source_slot].object,
        vm_inventory[target_slot].object.object_id);
    if (handler == 0)
        handler = vm_inventory[source_slot].object.use_on_handler;
    if (handler == 0)
        return true;
    if (!vm_run_inventory_object((unsigned)source_slot, handler, 0,
                                 status, status_size))
        return false;
    DEBUGF("agds: inventory combine %s -> %s\n",
           vm_inventory[source_slot].name,
           vm_inventory[target_slot].name);
    return true;
}

bool scummvm_agds_vm_inventory_use_at(
    unsigned source_index, int16_t x, int16_t y,
    char *status, size_t status_size)
{
    int source_slot = vm_inventory_slot_at(source_index);
    int index;

    if (source_slot < 0 || vm_scene == NULL)
        return true;
    for (index = (int)vm_scene->object_count - 1; index >= 0; index--) {
        struct scummvm_agds_screen_object *target =
            &vm_scene->objects[index];
        uint16_t handler;

        if (!target->alive || !target->visible ||
            !vm_object_contains(target, x, y))
            continue;
        handler = vm_use_object_handler(
            &vm_inventory[source_slot].object, target->object_id);
        if (handler != 0)
            return vm_run_inventory_object((unsigned)source_slot,
                                           handler, 0,
                                           status, status_size);
        if (target->use_on_handler != 0) {
            struct agds_vm_process process;

            rb->memset(&process, 0, sizeof(process));
            rb->strlcpy(process.name, target->name, sizeof(process.name));
            rb->strlcpy(process.code_entry, target->code_entry,
                        sizeof(process.code_entry));
            process.ip = target->use_on_handler;
            process.object_index = (uint16_t)index;
            vm_reset_process_state(&process);
            return vm_run_process(&process, 0, status, status_size);
        }
        return true;
    }
    return true;
}

#ifdef SIMULATOR
void scummvm_agds_vm_simulator_seed_save_state(void)
{
    char status[128];

    if (!scummvm_agds_vm_inventory_contains("inv.10bb"))
        if (!vm_inventory_add("inv.10bb", 0,
                              status, sizeof(status)))
            DEBUGF("agds: simulator wallet seed failed: %s\n", status);
    if (!scummvm_agds_vm_inventory_contains("inv.112c"))
        if (!vm_inventory_add("inv.112c", 0,
                              status, sizeof(status)))
            DEBUGF("agds: simulator cellphone seed failed: %s\n", status);
    DEBUGF("agds: simulator seeded retail save state inventory=%u\n",
           scummvm_agds_vm_inventory_count());
}

void scummvm_agds_vm_simulator_scramble_save_state(void)
{
    unsigned index;

    rb->memset(vm_inventory, 0, sizeof(vm_inventory));
    if (vm_scene != NULL && vm_scene->object_count > 0) {
        vm_scene->objects[0].visible = false;
        vm_scene->objects[0].alive = false;
        vm_scene->objects[0].x = 0;
        vm_scene->objects[0].y = 0;
    }
    for (index = 0; index < AGDS_VM_CHARACTERS; index++) {
        if (!vm_characters[index].used)
            continue;
        vm_characters[index].x = 0;
        vm_characters[index].y = 0;
        vm_characters[index].visible = false;
        vm_sync_character_object(&vm_characters[index]);
        break;
    }
    DEBUGF("agds: simulator scrambled save state inventory=0\n");
}
#endif

bool scummvm_agds_vm_take_audio_event(
    struct scummvm_agds_audio_event *event)
{
    if (event == NULL || vm_audio_count == 0)
        return false;
    *event = vm_audio_events[vm_audio_read];
    vm_audio_read = (vm_audio_read + 1u) % AGDS_VM_AUDIO_EVENTS;
    vm_audio_count--;
    return true;
}

bool scummvm_agds_vm_take_film_event(
    struct scummvm_agds_film_event *event)
{
    if (event == NULL || !vm_film_pending)
        return false;
    *event = vm_film_event;
    vm_film_pending = false;
    return true;
}

void scummvm_agds_vm_film_finished(void)
{
    vm_film_pending = false;
    vm_film_active = false;
    vm_tell_voice = false;
    vm_scene_animation_request = 0;
}

bool scummvm_agds_vm_dialog_text(char *text, size_t text_size, bool *npc)
{
    const char *source;
    size_t length;

    if (text == NULL || text_size == 0 || !vm_dialog.text_visible)
        return false;
    source = vm_dialog.line;
    while (*source == ' ' || *source == '\t')
        source++;
    length = rb->strlen(source);
    while (length > 0 &&
           (source[length - 1] == '\r' || source[length - 1] == '\n' ||
            source[length - 1] == ' ' || source[length - 1] == '\t'))
        length--;
    if (length == 0)
        return false;
    if (length >= text_size)
        length = text_size - 1;
    rb->memcpy(text, source, length);
    text[length] = '\0';
    if (npc != NULL)
        *npc = vm_dialog.text_npc;
    return true;
}

unsigned scummvm_agds_vm_take_scene_animation(void)
{
    unsigned request = vm_scene_animation_request;
    vm_scene_animation_request = 0;
    return request;
}

void scummvm_agds_vm_voice_state(bool playing, bool finished)
{
    unsigned index;

    vm_tell_voice = playing;
    if (finished) {
        for (index = 0; index < vm_suspended_count; index++)
            if (vm_suspended[index].waiting_tell)
                vm_suspended[index].timer = 0;
    }
}

bool scummvm_agds_vm_dialog_advance(void)
{
    unsigned index;
    bool found = false;

    if (!vm_dialog.text_visible)
        return false;
    vm_tell_voice = false;
    for (index = 0; index < vm_suspended_count; index++) {
        if (!vm_suspended[index].waiting_tell)
            continue;
        vm_suspended[index].timer = 0;
        found = true;
    }
    return found;
}

unsigned scummvm_agds_vm_object_text_count(void)
{
    unsigned count = 0;
    unsigned index;

    if (vm_scene == NULL)
        return 0;
    for (index = 0; index < AGDS_VM_OBJECT_TEXTS; index++) {
        unsigned object_index;

        if (!vm_object_texts[index].used)
            continue;
        object_index = vm_object_texts[index].object_index;
        if (object_index < vm_scene->object_count &&
            vm_scene->objects[object_index].alive &&
            vm_scene->objects[object_index].visible)
            count++;
    }
    return count;
}

bool scummvm_agds_vm_object_text(
    unsigned ordinal, struct scummvm_agds_object_text *text)
{
    unsigned found = 0;
    unsigned index;

    if (vm_scene == NULL || text == NULL)
        return false;
    for (index = 0; index < AGDS_VM_OBJECT_TEXTS; index++) {
        struct agds_vm_object_text_slot *slot = &vm_object_texts[index];
        unsigned object_index;

        if (!slot->used)
            continue;
        object_index = slot->object_index;
        if (object_index >= vm_scene->object_count ||
            !vm_scene->objects[object_index].alive ||
            !vm_scene->objects[object_index].visible)
            continue;
        if (found++ != ordinal)
            continue;
        *text = slot->overlay;
        text->x = vm_scene->objects[object_index].x;
        text->y = vm_scene->objects[object_index].y;
        return true;
    }
    return false;
}

bool scummvm_agds_vm_font(
    unsigned slot, struct scummvm_agds_font *font)
{
    if (slot >= AGDS_VM_MAX_FONTS || font == NULL ||
        !vm_fonts[slot].used)
        return false;
    *font = vm_fonts[slot];
    return true;
}

int scummvm_agds_vm_dialog_font(bool npc)
{
    struct agds_vm_global *font = vm_find_system(
        npc ? "npc_tell_font" : "tell_font", false);

    if (font == NULL || font->value < 0 ||
        font->value >= AGDS_VM_MAX_FONTS)
        return npc ? 3 : 1;
    return (int)font->value;
}

int32_t scummvm_agds_vm_system_value(const char *name)
{
    struct agds_vm_global *value;

    if (name == NULL)
        return 0;
    value = vm_find_system(name, false);
    return value != NULL ? value->value : vm_system_default(name);
}

#ifdef SIMULATOR
void scummvm_agds_vm_simulator_set_test_speed(unsigned speed)
{
    vm_test_speed = MAX(1u, MIN(16u, speed));
}
#endif

void scummvm_agds_vm_reset(void)
{
    vm_last_tick = *rb->current_tick;
    vm_target = NULL;
    vm_scene = NULL;
    vm_suspended_count = 0;
    vm_shared_index = -2;
    vm_audio_read = 0;
    vm_audio_count = 0;
    vm_film_pending = false;
    vm_film_active = false;
    vm_tell_voice = false;
    vm_scene_animation_request = 0;
    vm_next_screen[0] = '\0';
    vm_previous_screen[0] = '\0';
    vm_quit_requested = false;
    vm_load_request = -1;
    vm_restore_pending = false;
    vm_saved_character_count = 0;
    vm_saved_object_count = 0;
    vm_intro_debug_ip = UINT16_MAX;
    vm_save_picture_width = 0;
    vm_save_picture_height = 0;
    vm_inventory_enabled = true;
    vm_current_inventory_slot = -1;
    vm_saved_mouse_x = 0;
    vm_saved_mouse_y = 0;
    rb->memset(&vm_dialog, 0, sizeof(vm_dialog));
    vm_dialog.current_sound = -1;
    vm_dialog_npc_notify[0] = '\0';
    vm_dialog_character_notify[0] = '\0';
    vm_dialog_direction_notify[0] = '\0';
    rb->memset(vm_shared, 0, sizeof(vm_shared));
    rb->memset(vm_globals, 0, sizeof(vm_globals));
    rb->memset(vm_system_vars, 0, sizeof(vm_system_vars));
    rb->memset(vm_inventory, 0, sizeof(vm_inventory));
    rb->memset(vm_picture_handles, 0, sizeof(vm_picture_handles));
    rb->memset(vm_patch_refs, 0, sizeof(vm_patch_refs));
    rb->memset(vm_audio_events, 0, sizeof(vm_audio_events));
    rb->memset(&vm_film_event, 0, sizeof(vm_film_event));
    rb->memset(vm_mouse_areas, 0, sizeof(vm_mouse_areas));
    rb->memset(vm_key_handlers, 0, sizeof(vm_key_handlers));
    rb->memset(vm_characters, 0, sizeof(vm_characters));
    rb->memset(vm_animations, 0, sizeof(vm_animations));
    rb->memset(vm_object_texts, 0, sizeof(vm_object_texts));
    rb->memset(vm_fonts, 0, sizeof(vm_fonts));
    rb->memset(vm_saved_characters, 0, sizeof(vm_saved_characters));
    rb->memset(vm_saved_objects, 0, sizeof(vm_saved_objects));
    rb->memset(&vm_motion, 0, sizeof(vm_motion));
}
