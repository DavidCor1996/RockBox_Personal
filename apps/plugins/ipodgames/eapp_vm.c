#include "ipodgames.h"
#include "armemu/armemu.h"

#define IG_VM_BASE 0x18000000u
#define IG_VM_MEMORY_SIZE 0x01000000u
#define IG_VM_IMAGE_LIMIT 0x00100000u
#define IG_VM_HEAP_BASE 0x18100000u
#define IG_VM_HEAP_LIMIT 0x18e00000u
#define IG_VM_SCRATCH_BASE 0x18e00000u
#define IG_VM_STACK_TOP 0x18f0ff00u
#define IG_VM_TRAP_BASE 0x18f10000u
#define IG_VM_CALLBACK_RETURN 0x18f1fff8u
#define IG_VM_RETURN 0x18f1fffcu
#define IG_VM_TRAP_LIMIT IG_VM_CALLBACK_RETURN
#define IG_MSPAC_ASYNC_CALLBACK 0x180168f8u
#define IG_MSPAC_ASYNC_COMPLETE 0x18016924u
#define IG_MSPAC_ASYNC_OPEN_CALLBACK 0x1801688cu
#define IG_MSPAC_ASYNC_OPEN_COMPLETE 0x180168c0u
#define IG_MSPAC_AUDIO_LOADER_POINTER 0x180c7654u
#define IG_MSPAC_AUDIO_DATA_COMPLETE 0x18015fe4u
#define IG_VORTEX_ASYNC_REQUEST_CALLBACK 0x18020368u
#define IG_VM_AUDIO_OBJECTS 64
#define IG_VM_AUDIO_BLOCK_FRAMES 512
#define IG_VM_AUDIO_RATE 11025
#define IG_VM_VORTEX_AUDIO_RATE 27000
/* miscTBD[9] forwards the 1 MHz PortalPlayer free-running counter. */
#define IG_VM_EVENT_CLOCK_HZ 1000000u
#define IG_VM_MAX_IMPORTS 4096
#define IG_VM_ALLOCATIONS 1024
#define IG_VM_PHASE_LIMIT 20000000ul
#define IG_VM_FRAMEBUFFER_BYTES \
    ((size_t)LCD_WIDTH * LCD_HEIGHT * sizeof(fb_data))
#ifdef SIMULATOR
#define IG_VM_FRAME_DUMP IG_ROOT_DIR "/ipodgames-frame.ppm"
#endif

struct ig_vm_attribute
{
    u32 pointer;
    u32 stride;
    unsigned int size;
    unsigned int type;
};

struct ig_vm_texture
{
    u32 id;
    u32 pointer;
    u32 *pixels;
    size_t capacity;
    unsigned int width;
    unsigned int height;
    unsigned int format;
    unsigned int type;
    bool all_opaque;
    bool all_transparent;
    unsigned int alpha_left;
    unsigned int alpha_right;
    unsigned int alpha_top;
    unsigned int alpha_bottom;
    unsigned int revision;
    unsigned long draws;
    unsigned long alpha[3];
    char name[64];
};

struct ig_vm_raster_vertex
{
    /* Screen coordinates use 16.16 fixed point in the OpenGL bottom-left
     * coordinate system. Texture coordinates retain the guest's 16.16
     * texel representation. */
    s32 x;
    s32 y;
    s32 u;
    s32 v;
    unsigned int red;
    unsigned int green;
    unsigned int blue;
    unsigned int alpha;
};

struct ig_vm_audio_object
{
    u32 data;
    u32 bytes;
    unsigned long long position;
    u32 step;
    unsigned int rate;
    unsigned int bits;
    unsigned int channels;
    unsigned int amplitude;
    unsigned int gain_left;
    unsigned int gain_right;
    int pan;
    unsigned int pitch;
    unsigned int repeat_count;
    unsigned int repeats_remaining;
    bool allocated;
    bool playing;
    bool paused;
};

struct ig_vm_allocation
{
    u32 pointer;
    u32 size;
    u32 capacity;
    bool active;
};

enum ig_vm_framework
{
    IG_VM_FW_UNKNOWN = 0,
    IG_VM_FW_MISC,
    IG_VM_FW_OPENGLES,
    IG_VM_FW_METADATA,
    IG_VM_FW_ASYNC_FILE_IO,
    IG_VM_FW_AUDIO,
    IG_VM_FW_INPUT_EVENTS,
    IG_VM_FW_SETTINGS,
};

struct ig_vm_state
{
    machine_t machine;
    cpu_t cpu;
    unsigned char *memory;
    size_t memory_size;
    unsigned char import_framework[IG_VM_MAX_IMPORTS];
    unsigned short import_ordinal[IG_VM_MAX_IMPORTS];
    unsigned int import_count;
    unsigned long framework_calls;
    u32 last_framework_lr;
    u32 last_framework_r0;
    unsigned int last_framework_ordinal;
    unsigned int last_framework_id;
    unsigned int call_history[16];
    unsigned int call_history_next;
    unsigned int call_history_count;
#ifdef SIMULATOR
    u32 guest_history_pc[IG_VM_GUEST_HISTORY];
    u32 guest_history_r4[IG_VM_GUEST_HISTORY];
    u32 guest_history_r5[IG_VM_GUEST_HISTORY];
    u32 guest_history_sp[IG_VM_GUEST_HISTORY];
    unsigned int guest_history_next;
    unsigned int guest_history_count;
    u32 fatal_signal[5];
    bool gles_texture_trace_started;
#endif
    struct ig_vm_attribute attributes[3];
    struct ig_vm_texture textures[IG_VM_MAX_TEXTURES];
    struct ig_vm_texture *bound_texture;
    struct ig_vm_texture *bound_textures[2];
    unsigned int active_texture_unit;
    unsigned int texture_count;
    u32 next_texture_id;
    fb_data *framebuffer;
    unsigned char *texture_storage;
    size_t texture_storage_size;
    size_t texture_storage_used;
    u32 *vortex_composite_cache;
    u32 vortex_composite_source_id;
    u32 vortex_composite_mask_id;
    unsigned int vortex_composite_source_revision;
    unsigned int vortex_composite_mask_revision;
    unsigned char vortex_composite_red;
    unsigned char vortex_composite_green;
    unsigned char vortex_composite_blue;
    unsigned int vortex_composite_left;
    unsigned int vortex_composite_right;
    unsigned int vortex_composite_top;
    unsigned int vortex_composite_bottom;
    bool vortex_composite_valid;
    unsigned long draw_calls;
    unsigned long presented_frames;
#ifdef SIMULATOR
    int draw_rects[IG_VM_DRAW_HISTORY][4];
    int draw_sources[IG_VM_DRAW_HISTORY][4];
    u32 draw_texture_ids[IG_VM_DRAW_HISTORY];
    u32 draw_modes[IG_VM_DRAW_HISTORY];
    u32 draw_texture0_ids[IG_VM_DRAW_HISTORY];
    u32 draw_texture1_ids[IG_VM_DRAW_HISTORY];
    unsigned int draw_history_count;
    bool draw_trace_started;
#endif
    unsigned long fast_texture_quads;
    unsigned long fast_texture_pixels;
    unsigned long fast_opaque_pixels;
    unsigned long fast_blended_pixels;
    unsigned long fast_skipped_pixels;
    unsigned long general_raster_pixels;
    unsigned long fast_rejects[4];
    unsigned long flipped_texture_x;
    unsigned long flipped_texture_y;
    unsigned long transform_vertices;
    unsigned long transform_affine_vertices;
    unsigned long transform_perspective_vertices;
    fb_data clear_color;
    unsigned int gles_mode;
    s32 transform[16];
    bool transform_valid;
    unsigned int gles_ordinal_counts[IG_GLES_ORDINALS];
    u32 gles_last_args[IG_GLES_ORDINALS][4];
    unsigned int gles_mode_draws[IG_GLES_MODES];
    unsigned long gles_mode_alpha[IG_GLES_MODES][3];
    unsigned char gles_uniform_color[IG_GLES_MODES][4];
#ifdef SIMULATOR
    u32 gles_texture_pairs[IG_VM_TEXTURE_PAIR_HISTORY][4];
    unsigned int gles_texture_pair_count;
#endif
    unsigned int texture_subimages;
    unsigned int texture_subimage_failures;
    unsigned long texture_subimage_bytes;
    u32 texture_subimage_last[7];
    u32 pending_callback;
    u32 pending_context;
    u32 pending_bytes;
    u32 pending_status;
    bool pending_object_callback;
    u32 last_async_destination;
    u32 last_async_capacity;
    u32 last_async_bytes;
    u32 last_async_context;
    u32 last_async_callback;
    u32 last_async_inner_callback;
    u32 last_async_inner_context;
    u32 last_async_manager_callback;
    u32 last_async_manager_context;
    u32 pending_audio_context;
    unsigned int audio_resource_callbacks;
    unsigned int audio_trace;
    unsigned int audio_data_reads;
    unsigned long audio_data_bytes;
    unsigned int audio_next_handle;
    struct ig_vm_audio_object audio_objects[IG_VM_AUDIO_OBJECTS];
    unsigned int audio_old_frequency;
    unsigned int audio_output_rate;
    unsigned int audio_play_starts;
    unsigned int audio_max_voices;
    unsigned long audio_pcm_frames;
    bool audio_multivoice_probe_pass;
    bool audio_initialized;
    bool audio_playing;
    unsigned int audio_ordinal_counts[IG_AUDIO_ORDINALS];
    u32 audio_last_args[IG_AUDIO_ORDINALS][4];
    unsigned int async_reads;
    unsigned int async_opens;
    u32 async_stream_offset;
    unsigned int async_ordinal_counts[17];
    u32 async_last_args[17][4];
    unsigned int metadata_ordinal_counts[IG_METADATA_ORDINALS];
    u32 metadata_last_args[IG_METADATA_ORDINALS][4];
    int save_fds[4];
    unsigned int save_writes;
    unsigned long save_bytes;
    unsigned int save_reads;
    unsigned long save_read_bytes;
    unsigned int save_failures;
    bool save_probe_pass;
    bool save_exit_requested;
    bool is_vortex;
    bool realtime_clock;
    bool reference_raster;
    bool hardware_exec;
    bool ready_event_sent;
    bool scripted_input;
    unsigned int input_polls;
    unsigned int synthetic_input_frame;
    u32 wheel_raw;
    u32 wheel_delta;
    unsigned int button_event_type;
    unsigned int button_event_phase;
    unsigned int frontend_updates;
    unsigned int frontend_complete;
    unsigned int frontend_blocked;
    u32 frontend_mode;
    unsigned int input_handler_calls;
    unsigned int input_handler_type_counts[8];
    u32 input_handler_last[5];
    int input_wheel_handler_total;
    int input_wheel_handler_last;
    u32 input_wheel_handler_object;
    u32 input_wheel_handler_selection;
    u32 input_wheel_handler_accumulator;
    unsigned int input_select_calls;
    unsigned int input_transition_requests;
    char last_async_file[64];
    char last_save_file[64];
    u32 heap_next;
    struct ig_vm_allocation allocations[IG_VM_ALLOCATIONS];
    unsigned int allocation_count;
    u32 clock_ticks;
    unsigned int clock_fraction;
    unsigned long runtime_elapsed_ticks;
    unsigned long frame_exec_ticks;
    unsigned long frame_exec_max_ticks;
    unsigned long late_frames;
    unsigned long max_late_ticks;
    unsigned long deadline_rebases;
    unsigned long profile_guest_us;
    unsigned long profile_framework_us;
    unsigned long profile_draw_us;
    unsigned long profile_draw_mode_us[IG_GLES_MODES];
    unsigned long profile_draw_mode_calls[IG_GLES_MODES];
    unsigned long profile_lcd_us;
    unsigned int system_settings_reads;
    unsigned int system_clock_reads;
    unsigned int system_battery_reads;
    unsigned int system_brightness_gets;
    unsigned int system_brightness_sets;
    unsigned int system_settings_successes;
    unsigned int system_last_setting_capacity;
    char system_last_setting[32];
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    int brightness_original;
    int brightness_percent;
    bool brightness_changed;
#endif
    char game_directory[MAX_PATH];
    char save_directory[MAX_PATH];
};

static struct ig_vm_texture *vm_bind_texture(struct ig_vm_state *state,
                                              u32 id);

static struct ig_vm_state vm;
static struct ig_vm_state *active_vm;
static struct ig_vm_texture *vm_draw_texture(struct ig_vm_state *state);
static bool vm_mode_uses_both_textures(const struct ig_vm_state *state);
#ifdef SIMULATOR
static void vm_dump_frame_to(const struct ig_vm_state *state,
                             const char *path);
#endif
static int16_t audio_mix_buffer[IG_VM_AUDIO_BLOCK_FRAMES * 2]
    MEM_ALIGN_ATTR;

const char *ig_vm_result_name(enum ig_vm_result result)
{
    static const char *const names[] = {
        "lifecycle completed",
        "unsupported eApp image",
        "insufficient shared memory",
        "executable read failed",
        "invalid import tables",
        "ARM execution fault",
    };

    if ((unsigned int)result >= ARRAYLEN(names))
        return "unknown VM result";
    return names[result];
}

static u32 vm_u32(const unsigned char *data)
{
    return (u32)data[0] | ((u32)data[1] << 8) |
           ((u32)data[2] << 16) | ((u32)data[3] << 24);
}

static void vm_put_u32(unsigned char *data, u32 value)
{
    data[0] = value;
    data[1] = value >> 8;
    data[2] = value >> 16;
    data[3] = value >> 24;
}

static bool vm_guest_u32(struct ig_vm_state *state, u32 address, u32 *value)
{
    u32 offset;

    if (address < IG_VM_BASE)
        return false;
    offset = address - IG_VM_BASE;
    if (offset > state->memory_size || state->memory_size - offset < 4)
        return false;
    *value = vm_u32(state->memory + offset);
    return true;
}

#ifdef SIMULATOR
static void vm_trace_input_handler(struct ig_vm_state *state)
{
    u32 stack_argument = 0;
    u32 type = state->cpu.r[1];

    ++state->input_handler_calls;
    if (type < ARRAYLEN(state->input_handler_type_counts))
        ++state->input_handler_type_counts[type];
    state->input_handler_last[0] = state->cpu.r[0];
    state->input_handler_last[1] = type;
    state->input_handler_last[2] = state->cpu.r[2];
    state->input_handler_last[3] = state->cpu.r[3];
    vm_guest_u32(state, state->cpu.r[reg(&state->cpu, SP)],
                 &stack_argument);
    state->input_handler_last[4] = stack_argument;
    if (type == 6 && state->cpu.r[2] == 2)
    {
        u32 value;

        state->input_wheel_handler_last = (int)stack_argument;
        state->input_wheel_handler_total += (int)stack_argument;
        state->input_wheel_handler_object = state->cpu.r[0];
        if (vm_guest_u32(state, state->cpu.r[0] + 32, &value))
            state->input_wheel_handler_selection = value;
        if (vm_guest_u32(state, state->cpu.r[0] + 1528, &value))
            state->input_wheel_handler_accumulator = value;
    }
}

static void vm_trace_vortex_input_parser(struct ig_vm_state *state)
{
    u32 node = state->cpu.r[0];
    u32 input_mask = 0;
    unsigned int type = 0;
    unsigned int phase = 0;

    ++state->input_handler_calls;
    if (node >= IG_VM_BASE && node - IG_VM_BASE + 2 <= state->memory_size)
    {
        type = state->memory[node - IG_VM_BASE];
        phase = state->memory[node - IG_VM_BASE + 1];
    }
    if (type < ARRAYLEN(state->input_handler_type_counts))
        ++state->input_handler_type_counts[type];
    vm_guest_u32(state, state->cpu.r[1], &input_mask);
    state->input_handler_last[0] = node;
    state->input_handler_last[1] = type;
    state->input_handler_last[2] = phase;
    state->input_handler_last[3] = state->cpu.r[3];
    state->input_handler_last[4] = input_mask;
    if (type == 2 && phase == 2)
        ++state->input_select_calls;
}
#endif

static bool vm_guest_put_u32(struct ig_vm_state *state, u32 address,
                             u32 value)
{
    u32 offset;

    if (address < IG_VM_BASE)
        return false;
    offset = address - IG_VM_BASE;
    if (offset > state->memory_size || state->memory_size - offset < 4)
        return false;
    vm_put_u32(state->memory + offset, value);
    return true;
}

static u32 vm_heap_allocate(struct ig_vm_state *state, u32 requested)
{
    u32 allocated;
    u32 pointer;
    unsigned int allocation;

    if (requested > IG_VM_HEAP_LIMIT - IG_VM_HEAP_BASE - 15)
        return 0;
    allocated = (requested + 15) & ~15u;
    if (!allocated)
        return 0;
    /* Retail eApps use malloc/free for short-lived render objects. Reuse a
     * released block before growing the launch arena so those allocations do
     * not consume one tracking record per frame. */
    for (allocation = 0; allocation < state->allocation_count; ++allocation)
    {
        struct ig_vm_allocation *record = &state->allocations[allocation];

        if (!record->active && record->capacity >= allocated)
        {
            record->size = requested;
            record->active = true;
            return record->pointer;
        }
    }
    if (state->heap_next > IG_VM_HEAP_LIMIT - allocated ||
        state->allocation_count >= ARRAYLEN(state->allocations))
        return 0;
    pointer = state->heap_next;
    state->heap_next += allocated;
    state->allocations[state->allocation_count].pointer = pointer;
    state->allocations[state->allocation_count].size = requested;
    state->allocations[state->allocation_count].capacity = allocated;
    state->allocations[state->allocation_count].active = true;
    ++state->allocation_count;
    return pointer;
}

static void vm_heap_free(struct ig_vm_state *state, u32 pointer)
{
    unsigned int allocation;

    if (!pointer)
        return;
    for (allocation = state->allocation_count; allocation > 0; --allocation)
    {
        struct ig_vm_allocation *record =
            &state->allocations[allocation - 1];

        if (record->active && record->pointer == pointer)
        {
            record->active = false;
            record->size = 0;
            return;
        }
    }
}

static u32 vm_heap_reallocate(struct ig_vm_state *state, u32 pointer,
                              u32 requested)
{
    u32 replacement;
    u32 old_size = 0;
    unsigned int old_allocation = 0;
    unsigned int allocation;

    if (!pointer)
        return vm_heap_allocate(state, requested);
    if (!requested)
        return 0;
    for (allocation = state->allocation_count; allocation > 0; --allocation)
    {
        const struct ig_vm_allocation *record =
            &state->allocations[allocation - 1];

        if (record->active && record->pointer == pointer)
        {
            old_size = record->size;
            old_allocation = allocation;
            break;
        }
    }
    if (old_size)
    {
        struct ig_vm_allocation *record =
            &state->allocations[old_allocation - 1];
        u32 old_capacity = record->capacity;
        u32 new_capacity = (requested + 15) & ~15u;

        if (new_capacity <= old_capacity)
        {
            record->size = requested;
            return pointer;
        }
        if (pointer + old_capacity == state->heap_next &&
            state->heap_next <= IG_VM_HEAP_LIMIT -
                                (new_capacity - old_capacity))
        {
            state->heap_next += new_capacity - old_capacity;
            record->size = requested;
            record->capacity = new_capacity;
            return pointer;
        }
    }
    replacement = vm_heap_allocate(state, requested);
    if (!replacement)
        return 0;
    if (old_size && pointer >= IG_VM_BASE &&
        pointer - IG_VM_BASE <= state->memory_size &&
        old_size <= state->memory_size - (pointer - IG_VM_BASE))
    {
        rb->memcpy(state->memory + replacement - IG_VM_BASE,
                   state->memory + pointer - IG_VM_BASE,
                   MIN(old_size, requested));
    }
    if (old_size)
        state->allocations[old_allocation - 1].active = false;
    return replacement;
}

static bool vm_guest_string(struct ig_vm_state *state, u32 address,
                            char *text, size_t text_size)
{
    size_t index;

    if (!text_size || address < IG_VM_BASE)
        return false;
    for (index = 0; index + 1 < text_size; ++index)
    {
        u32 offset = address - IG_VM_BASE + index;

        if (offset >= state->memory_size)
            return false;
        text[index] = state->memory[offset];
        if (!text[index])
            return true;
    }
    text[text_size - 1] = '\0';
    return false;
}

static bool vm_guest_bytes(struct ig_vm_state *state, u32 address,
                           const void *data, size_t size)
{
    u32 offset;

    if (address < IG_VM_BASE)
        return false;
    offset = address - IG_VM_BASE;
    if (offset > state->memory_size || size > state->memory_size - offset)
        return false;
    rb->memcpy(state->memory + offset, data, size);
    return true;
}

static float vm_float_from_bits(u32 bits)
{
    union { u32 bits; float value; } converted;

    converted.bits = bits;
    return converted.value;
}

static u32 vm_float_to_bits(float value)
{
    union { u32 bits; float value; } converted;

    converted.value = value;
    return converted.bits;
}

static bool vm_matrix_read(struct ig_vm_state *state, u32 address,
                           float matrix[16])
{
    unsigned int index;

    for (index = 0; index < 16; ++index)
    {
        u32 bits;

        if (!vm_guest_u32(state, address + index * 4, &bits))
            return false;
        matrix[index] = vm_float_from_bits(bits);
    }
    return true;
}

static bool vm_matrix_write(struct ig_vm_state *state, u32 address,
                            const float matrix[16])
{
    unsigned int index;

    for (index = 0; index < 16; ++index)
        if (!vm_guest_put_u32(state, address + index * 4,
                              vm_float_to_bits(matrix[index])))
            return false;
    return true;
}

static void vm_matrix_identity(struct ig_vm_state *state, u32 address)
{
    float matrix[16];
    unsigned int index;

    for (index = 0; index < 16; ++index)
        matrix[index] = index % 5 == 0 ? 1.0f : 0.0f;
    vm_matrix_write(state, address, matrix);
}

static void vm_matrix_multiply(struct ig_vm_state *state, u32 destination,
                               u32 left_address, u32 right_address)
{
    float left[16];
    float right[16];
    float result[16];
    unsigned int column;
    unsigned int row;

    if (!vm_matrix_read(state, left_address, left) ||
        !vm_matrix_read(state, right_address, right))
        return;
    for (column = 0; column < 4; ++column)
        for (row = 0; row < 4; ++row)
        {
            unsigned int item;
            float value = 0.0f;

            for (item = 0; item < 4; ++item)
                value += left[item * 4 + row] *
                         right[column * 4 + item];
            result[column * 4 + row] = value;
        }
    vm_matrix_write(state, destination, result);
}

static void vm_matrix_ortho(struct ig_vm_state *state, u32 address)
{
    u32 stack = state->cpu.r[reg(&state->cpu, SP)];
    u32 top_bits;
    u32 near_bits;
    u32 far_bits;
    float left = vm_float_from_bits(state->cpu.r[1]);
    float right = vm_float_from_bits(state->cpu.r[2]);
    float bottom = vm_float_from_bits(state->cpu.r[3]);
    float top;
    float near_value;
    float far_value;
    float matrix[16] = { 0 };

    if (!vm_guest_u32(state, stack, &top_bits) ||
        !vm_guest_u32(state, stack + 4, &near_bits) ||
        !vm_guest_u32(state, stack + 8, &far_bits))
        return;
    top = vm_float_from_bits(top_bits);
    near_value = vm_float_from_bits(near_bits);
    far_value = vm_float_from_bits(far_bits);
    if (right == left || top == bottom || far_value == near_value)
        return;
    matrix[0] = 2.0f / (right - left);
    matrix[5] = 2.0f / (top - bottom);
    matrix[10] = -2.0f / (far_value - near_value);
    matrix[12] = -(right + left) / (right - left);
    matrix[13] = -(top + bottom) / (top - bottom);
    matrix[14] = -(far_value + near_value) /
                 (far_value - near_value);
    matrix[15] = 1.0f;
    vm_matrix_write(state, address, matrix);
}

static void vm_matrix_translate(struct ig_vm_state *state, u32 address)
{
    float matrix[16];
    float x = vm_float_from_bits(state->cpu.r[1]);
    float y = vm_float_from_bits(state->cpu.r[2]);
    float z = vm_float_from_bits(state->cpu.r[3]);
    unsigned int row;

    if (!vm_matrix_read(state, address, matrix))
        return;
    for (row = 0; row < 4; ++row)
        matrix[12 + row] += matrix[row] * x + matrix[4 + row] * y +
                            matrix[8 + row] * z;
    vm_matrix_write(state, address, matrix);
}

static void vm_matrix_scale(struct ig_vm_state *state, u32 address)
{
    float matrix[16];
    float scale[3];
    unsigned int column;
    unsigned int row;

    scale[0] = vm_float_from_bits(state->cpu.r[1]);
    scale[1] = vm_float_from_bits(state->cpu.r[2]);
    scale[2] = vm_float_from_bits(state->cpu.r[3]);
    if (!vm_matrix_read(state, address, matrix))
        return;
    for (column = 0; column < 3; ++column)
        for (row = 0; row < 4; ++row)
            matrix[column * 4 + row] *= scale[column];
    vm_matrix_write(state, address, matrix);
}

static float vm_sine_degrees(float degrees)
{
    const float pi = 3.14159265358979323846f;
    float radians;
    float product;
    float value;
    bool negative = false;

    while (degrees >= 360.0f)
        degrees -= 360.0f;
    while (degrees < 0.0f)
        degrees += 360.0f;
    if (degrees >= 180.0f)
    {
        degrees -= 180.0f;
        negative = true;
    }
    radians = degrees * (pi / 180.0f);
    product = radians * (pi - radians);
    value = (16.0f * product) /
            (5.0f * pi * pi - 4.0f * product);
    return negative ? -value : value;
}

static float vm_inverse_sqrt(float value)
{
    float estimate;
    unsigned int iteration;

    if (value <= 0.0f)
        return 0.0f;
    estimate = value > 1.0f ? 1.0f / value : 1.0f;
    for (iteration = 0; iteration < 5; ++iteration)
        estimate *= 1.5f - 0.5f * value * estimate * estimate;
    return estimate;
}

static void vm_matrix_rotate(struct ig_vm_state *state, u32 address)
{
    u32 stack = state->cpu.r[reg(&state->cpu, SP)];
    u32 z_bits;
    float source[16];
    float rotation[16] = { 0 };
    float result[16];
    float angle = vm_float_from_bits(state->cpu.r[1]);
    float x = vm_float_from_bits(state->cpu.r[2]);
    float y = vm_float_from_bits(state->cpu.r[3]);
    float z;
    float inverse_length;
    float sine;
    float cosine;
    float one_minus_cosine;
    unsigned int column;
    unsigned int row;

    if (!vm_guest_u32(state, stack, &z_bits) ||
        !vm_matrix_read(state, address, source))
        return;
    z = vm_float_from_bits(z_bits);
    inverse_length = vm_inverse_sqrt(x * x + y * y + z * z);
    if (inverse_length == 0.0f)
        return;
    x *= inverse_length;
    y *= inverse_length;
    z *= inverse_length;
    sine = vm_sine_degrees(angle);
    cosine = vm_sine_degrees(angle + 90.0f);
    one_minus_cosine = 1.0f - cosine;
    rotation[0] = x * x * one_minus_cosine + cosine;
    rotation[1] = y * x * one_minus_cosine + z * sine;
    rotation[2] = x * z * one_minus_cosine - y * sine;
    rotation[4] = x * y * one_minus_cosine - z * sine;
    rotation[5] = y * y * one_minus_cosine + cosine;
    rotation[6] = y * z * one_minus_cosine + x * sine;
    rotation[8] = x * z * one_minus_cosine + y * sine;
    rotation[9] = y * z * one_minus_cosine - x * sine;
    rotation[10] = z * z * one_minus_cosine + cosine;
    rotation[15] = 1.0f;
    for (column = 0; column < 4; ++column)
        for (row = 0; row < 4; ++row)
        {
            unsigned int item;
            float value = 0.0f;

            for (item = 0; item < 4; ++item)
                value += source[item * 4 + row] *
                         rotation[column * 4 + item];
            result[column * 4 + row] = value;
        }
    vm_matrix_write(state, address, result);
}

static void vm_matrix_uniform(struct ig_vm_state *state, u32 address)
{
    float matrix[16];
    unsigned int index;

    if (!vm_matrix_read(state, address, matrix))
        return;
    for (index = 0; index < 16; ++index)
    {
        float scaled = matrix[index] * 65536.0f;

        if (scaled > 2147483647.0f)
            state->transform[index] = 0x7fffffff;
        else if (scaled < -2147483648.0f)
            state->transform[index] = (s32)0x80000000u;
        else
            state->transform[index] = (s32)scaled;
    }
    state->transform_valid = true;
}

static u32 vm_system_battery_level(struct ig_vm_state *state)
{
    int percent;

    ++state->system_battery_reads;
#ifdef SIMULATOR
    if (state->scripted_input)
        percent = 75;
    else
#endif
        percent = rb->battery_level();
    percent = MIN(100, MAX(0, percent));
    return (percent + 2) / 5;
}

static u32 vm_system_clock(struct ig_vm_state *state, u32 address)
{
#ifdef SIMULATOR
    struct tm fixed;
#endif
    const struct tm *clock;
    u32 fields[6];
    unsigned int index;

    ++state->system_clock_reads;
#ifdef SIMULATOR
    if (state->scripted_input)
    {
        rb->memset(&fixed, 0, sizeof(fixed));
        fixed.tm_sec = 0;
        fixed.tm_min = 35;
        fixed.tm_hour = 17;
        fixed.tm_mday = 17;
        fixed.tm_mon = 6;
        fixed.tm_year = 126;
        clock = &fixed;
    }
    else
#endif
        clock = rb->get_time();
    if (!clock)
        return 0;
    fields[0] = clock->tm_sec;
    fields[1] = clock->tm_min;
    fields[2] = clock->tm_hour;
    fields[3] = clock->tm_mday;
    fields[4] = clock->tm_mon + 1;
    fields[5] = clock->tm_year + 1900;
    for (index = 0; index < ARRAYLEN(fields); ++index)
        if (!vm_guest_put_u32(state, address + index * 4, fields[index]))
            return 0;
    return 1;
}

static u32 vm_system_setting(struct ig_vm_state *state, u32 key_address,
                             u32 value_address, u32 size_address)
{
    char key[32];
    u32 capacity;

    ++state->system_settings_reads;
    if (!vm_guest_string(state, key_address, key, sizeof(key)) ||
        !vm_guest_u32(state, size_address, &capacity))
        return 0;
    rb->strlcpy(state->system_last_setting, key,
                sizeof(state->system_last_setting));
    state->system_last_setting_capacity = capacity;
    if (!rb->strcmp(key, "Language") && capacity >= 4)
    {
        if (!vm_guest_put_u32(state, value_address, 0))
            return 0;
        ++state->system_settings_successes;
        return 1;
    }
    if (!rb->strcmp(key, "TimeFormat") && capacity >= 3)
    {
        static const char format_12[4] = { '1', '2', '\0', '\0' };
        static const char format_24[4] = { '2', '4', '\0', '\0' };
        const char *format;
        size_t bytes = MIN((u32)sizeof(format_12), capacity);

#ifdef SIMULATOR
        if (state->scripted_input)
            format = format_12;
        else
#endif
            format = rb->global_settings->timeformat ?
                format_12 : format_24;
        if (!vm_guest_bytes(state, value_address, format, bytes))
            return 0;
        ++state->system_settings_successes;
        return 1;
    }
    return 0;
}

#ifdef HAVE_BACKLIGHT_BRIGHTNESS
static u32 vm_system_get_brightness(struct ig_vm_state *state)
{
    ++state->system_brightness_gets;
    return state->brightness_percent;
}

static void vm_system_set_brightness(struct ig_vm_state *state,
                                     unsigned int percent)
{
    int brightness;

    percent = MIN(100u, percent);
    brightness = MIN_BRIGHTNESS_SETTING +
        ((MAX_BRIGHTNESS_SETTING - MIN_BRIGHTNESS_SETTING) * percent + 50) /
        100;
    rb->backlight_set_brightness(brightness);
    state->brightness_percent = percent;
    state->brightness_changed = true;
    ++state->system_brightness_sets;
}
#endif

static void vm_system_init(struct ig_vm_state *state)
{
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    int range = MAX_BRIGHTNESS_SETTING - MIN_BRIGHTNESS_SETTING;

    state->brightness_original = rb->global_settings->brightness;
    state->brightness_percent = range > 0 ?
        ((state->brightness_original - MIN_BRIGHTNESS_SETTING) * 100 +
         range / 2) / range : 100;
#else
    (void)state;
#endif
}

static void vm_system_shutdown(struct ig_vm_state *state)
{
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    if (state->brightness_changed)
        rb->backlight_set_brightness(state->brightness_original);
#else
    (void)state;
#endif
}

static int vm_fixed_integer(struct ig_vm_state *state, u32 address)
{
    u32 value;

    if (!vm_guest_u32(state, address, &value))
        return 0;
    return (s32)value >> 16;
}

static int vm_fixed_color(struct ig_vm_state *state, u32 address)
{
    u32 value;

    if (!vm_guest_u32(state, address, &value))
        return 0;
    return MIN(255, MAX(0, (int)(((s32)value * 255) >> 16)));
}

static u32 vm_cached_rgba5551(u16 pixel)
{
    /* RGBA5551 and the iPod's RGB565 framebuffer share the red bits and the
     * upper five green bits. Replicate green's MSB and shift blue once; this
     * is exactly equivalent to the former 8-bit expand/LCD_RGBPACK path.
     * Bits 16..23 cache alpha so all source formats share the same blending
     * representation. */
    if (!(pixel & 1))
        return 0;
    return 0xff0000u | (pixel & 0xffc0u) | ((pixel >> 5) & 0x20u) |
           ((pixel >> 1) & 0x1fu);
}

static u32 vm_cached_rgb565(u16 pixel)
{
    return 0xff0000u | pixel;
}

static u32 vm_cached_rgba4444(u16 pixel)
{
    unsigned int red = (pixel >> 12) & 0xf;
    unsigned int green = (pixel >> 8) & 0xf;
    unsigned int blue = (pixel >> 4) & 0xf;
    unsigned int alpha = pixel & 0xf;

    red = (red << 1) | (red >> 3);
    green = (green << 2) | (green >> 2);
    blue = (blue << 1) | (blue >> 3);
    return (alpha * 17u << 16) | (red << 11) | (green << 5) | blue;
}

static u32 vm_cached_rgba8888(const unsigned char *pixel)
{
    if (!pixel[3])
        return 0;
    return ((u32)pixel[3] << 16) |
           LCD_RGBPACK(pixel[0], pixel[1], pixel[2]);
}

static u32 vm_cached_alpha8(unsigned char alpha)
{
    if (!alpha)
        return 0;
    return ((u32)alpha << 16) | LCD_RGBPACK(255, 255, 255);
}

static u32 vm_cached_rgb888(const unsigned char *pixel)
{
    return 0xff0000u | LCD_RGBPACK(pixel[0], pixel[1], pixel[2]);
}

static inline unsigned int vm_div255(unsigned int value)
{
    return (value + 1 + (value >> 8)) >> 8;
}

static inline void vm_blend_cached_pixel(fb_data *destination, u32 source,
                                         unsigned int vertex_alpha)
{
    unsigned int alpha = vm_div255(
        ((source >> 16) & 0xff) * vertex_alpha);
    u32 source_color = source & 0xffff;
    u32 expanded_source;
    u32 expanded_destination;

    if (!alpha)
        return;
    if (alpha >= 255)
    {
        *destination = (fb_data)source_color;
        return;
    }
#ifdef SIMULATOR
    if (active_vm && active_vm->reference_raster &&
        !(active_vm->is_vortex &&
          (active_vm->gles_mode == 20 || active_vm->gles_mode == 32)))
    {
        u32 destination_color = (u32)*destination;
        unsigned int source_red = (source_color >> 11) & 0x1f;
        unsigned int source_green = (source_color >> 5) & 0x3f;
        unsigned int source_blue = source_color & 0x1f;
        unsigned int destination_red =
            (destination_color >> 11) & 0x1f;
        unsigned int destination_green =
            (destination_color >> 5) & 0x3f;
        unsigned int destination_blue = destination_color & 0x1f;

        destination_red = (source_red * alpha +
                           destination_red * (255 - alpha) + 127) / 255;
        destination_green = (source_green * alpha +
                             destination_green * (255 - alpha) + 127) / 255;
        destination_blue = (source_blue * alpha +
                            destination_blue * (255 - alpha) + 127) / 255;
        *destination = (fb_data)((destination_red << 11) |
                                 (destination_green << 5) |
                                 destination_blue);
        return;
    }
#endif
    expanded_source = (source_color | (source_color << 16)) & 0x07e0f81f;
    expanded_destination = (*destination | ((u32)*destination << 16)) &
        0x07e0f81f;
    alpha = (alpha + 4) >> 3;
    expanded_destination =
        (expanded_source * alpha +
         expanded_destination * (32 - alpha)) >> 5;
    expanded_destination &= 0x07e0f81f;
    *destination = (fb_data)(expanded_destination |
                             (expanded_destination >> 16));
}

static void ICODE_ATTR vm_clear_framebuffer(struct ig_vm_state *state)
{
    const size_t pixels = LCD_WIDTH * LCD_HEIGHT;

    if (!state->clear_color)
    {
        rb->memset(state->framebuffer, 0, pixels * sizeof(fb_data));
        return;
    }
#if LCD_DEPTH == 16
    {
        u32 packed = (u32)state->clear_color |
                     ((u32)state->clear_color << 16);
        u32 *destination = (u32 *)state->framebuffer;
        size_t pairs = pixels / 2;

        while (pairs >= 8)
        {
            destination[0] = packed;
            destination[1] = packed;
            destination[2] = packed;
            destination[3] = packed;
            destination[4] = packed;
            destination[5] = packed;
            destination[6] = packed;
            destination[7] = packed;
            destination += 8;
            pairs -= 8;
        }
        while (pairs--)
            *destination++ = packed;
        if (pixels & 1)
            state->framebuffer[pixels - 1] = state->clear_color;
    }
#else
    {
        size_t pixel;

        for (pixel = 0; pixel < pixels; ++pixel)
            state->framebuffer[pixel] = state->clear_color;
    }
#endif
}

static u32 vm_modulate_cached_pixel(const struct ig_vm_state *state,
                                    u32 source)
{
    const unsigned char *color;
    unsigned int alpha;
    unsigned int red;
    unsigned int green;
    unsigned int blue;

    if (state->gles_mode >= IG_GLES_MODES)
        return source;
    color = state->gles_uniform_color[state->gles_mode];
    alpha = vm_div255(((source >> 16) & 0xff) * color[3]);
    red = vm_div255(((source >> 11) & 0x1f) * color[0]);
    green = vm_div255(((source >> 5) & 0x3f) * color[1]);
    blue = vm_div255((source & 0x1f) * color[2]);
    return (alpha << 16) | (red << 11) | (green << 5) | blue;
}

static u32 vm_modulate_cached_vertex(u32 source, unsigned int vertex_red,
                                     unsigned int vertex_green,
                                     unsigned int vertex_blue,
                                     unsigned int vertex_alpha)
{
    unsigned int alpha = vm_div255(
        ((source >> 16) & 0xff) * vertex_alpha);
    unsigned int red = vm_div255(
        ((source >> 11) & 0x1f) * vertex_red);
    unsigned int green = vm_div255(
        ((source >> 5) & 0x3f) * vertex_green);
    unsigned int blue = vm_div255((source & 0x1f) * vertex_blue);

    return (alpha << 16) | (red << 11) | (green << 5) | blue;
}

static inline fb_data vm_modulate_opaque_rgb565(
    u32 source, unsigned int vertex_red, unsigned int vertex_green,
    unsigned int vertex_blue)
{
    unsigned int red = vm_div255(
        ((source >> 11) & 0x1f) * vertex_red);
    unsigned int green = vm_div255(
        ((source >> 5) & 0x3f) * vertex_green);
    unsigned int blue = vm_div255((source & 0x1f) * vertex_blue);

    return (fb_data)((red << 11) | (green << 5) | blue);
}

static void vm_cache_texture_row(u32 *destination,
                                 const unsigned char *source,
                                 size_t pixels)
{
    size_t x;

    for (x = 0; x < pixels; ++x)
        destination[x] = vm_cached_rgba5551(
            load_le16_aligned(source + x * 2));
}

static void vm_cache_texture_pixels(u32 *destination,
                                    const unsigned char *source,
                                    size_t pixels, unsigned int format,
                                    unsigned int type)
{
    size_t pixel;

    if (format == 0x1908 && type == 0x8034)
    {
        vm_cache_texture_row(destination, source, pixels);
        return;
    }
    if (format == 0x1907 && type == 0x8363)
    {
        for (pixel = 0; pixel < pixels; ++pixel)
            destination[pixel] = vm_cached_rgb565(
                load_le16_aligned(source + pixel * 2));
        return;
    }
    if (format == 0x1908 && type == 0x8033)
    {
        for (pixel = 0; pixel < pixels; ++pixel)
            destination[pixel] = vm_cached_rgba4444(
                load_le16_aligned(source + pixel * 2));
        return;
    }
    for (pixel = 0; pixel < pixels; ++pixel)
    {
        if (format == 0x1908 && type == 0x1401)
            destination[pixel] = vm_cached_rgba8888(source + pixel * 4);
        else if (format == 0x1907 && type == 0x1401)
            destination[pixel] = vm_cached_rgb888(source + pixel * 3);
        else if (format == 0x1906 && type == 0x1401)
            destination[pixel] = vm_cached_alpha8(source[pixel]);
        else
            destination[pixel] = 0;
    }
}

static void vm_analyze_texture(struct ig_vm_texture *texture)
{
    size_t pixels = (size_t)texture->width * texture->height;
    size_t pixel;

    texture->all_opaque = pixels != 0;
    texture->all_transparent = pixels != 0;
    texture->alpha_left = 0;
    texture->alpha_right = texture->width;
    texture->alpha_top = 0;
    texture->alpha_bottom = texture->height;
    if (pixels && texture->format == 0x1907 &&
        (texture->type == 0x1401 || texture->type == 0x8363))
    {
        texture->all_transparent = false;
        goto analyzed;
    }
    for (pixel = 0; pixel < pixels; ++pixel)
    {
        unsigned int alpha = (texture->pixels[pixel] >> 16) & 0xff;

        if (alpha != 255)
            texture->all_opaque = false;
        if (alpha != 0)
            texture->all_transparent = false;
        if (!texture->all_opaque && !texture->all_transparent)
            break;
    }
analyzed:
    ++texture->revision;
    if (!texture->revision)
        ++texture->revision;
}

static bool vm_upload_compressed_texture(struct ig_vm_state *state)
{
    /* glCompressedTexImage2D.  The retail games use the paletted OES
     * extension: GL_PALETTE8_RGBA8_OES stores 256 RGBA8888 palette entries
     * followed by one palette index per level-zero texel. */
    static const unsigned int palette_bytes = 256 * 4;
    struct ig_vm_texture *texture = state->bound_texture;
    u32 stack = state->cpu.r[reg(&state->cpu, SP)];
    u32 height;
    u32 border;
    u32 image_size;
    u32 pointer;
    unsigned int width = state->cpu.r[3];
    size_t pixels;
    size_t texture_bytes;
    size_t source_bytes;
    u32 source_offset;
    const unsigned char *source;
    size_t pixel;

    if (!vm_guest_u32(state, stack, &height) ||
        !vm_guest_u32(state, stack + 4, &border) ||
        !vm_guest_u32(state, stack + 8, &image_size) ||
        !vm_guest_u32(state, stack + 12, &pointer) ||
        state->cpu.r[1] != 0 || state->cpu.r[2] != 0x8b96 || border != 0 ||
        !width || !height || pointer < IG_VM_BASE)
        return false;
    if ((size_t)width > (SIZE_MAX - palette_bytes) / height)
        return false;
    pixels = (size_t)width * height;
    source_bytes = palette_bytes + pixels;
    if (image_size < source_bytes || pixels > SIZE_MAX / sizeof(u32))
        return false;
    source_offset = pointer - IG_VM_BASE;
    if (source_offset > state->memory_size ||
        source_bytes > state->memory_size - source_offset)
        return false;
    if (!texture)
        texture = vm_bind_texture(state, 0);
    if (!texture)
        return false;
    texture_bytes = pixels * sizeof(*texture->pixels);
    if (texture->capacity < texture_bytes)
    {
        if (state->texture_storage_used > state->texture_storage_size ||
            texture_bytes > state->texture_storage_size -
                            state->texture_storage_used)
            return false;
        texture->pixels = (u32 *)(state->texture_storage +
                                  state->texture_storage_used);
        texture->capacity = texture_bytes;
        state->texture_storage_used += texture_bytes;
    }
    source = state->memory + source_offset;
    for (pixel = 0; pixel < pixels; ++pixel)
    {
        unsigned int palette_index = source[palette_bytes + pixel];

        texture->pixels[pixel] = vm_cached_rgba8888(
            source + palette_index * 4);
    }
    texture->width = width;
    texture->height = height;
    texture->format = state->cpu.r[2];
    texture->type = 0;
    texture->pointer = pointer;
    if (!texture->name[0] && state->last_async_file[0])
        rb->strlcpy(texture->name, state->last_async_file,
                    sizeof(texture->name));
    vm_analyze_texture(texture);
    return true;
}

static unsigned int vm_float_color(u32 value)
{
    unsigned int exponent = (value >> 23) & 0xff;
    unsigned int significand;
    unsigned int shift;
    uint64_t scaled;

    if (value >> 31 || exponent < 119)
        return 0;
    if (exponent >= 127)
        return 255;
    significand = (value & 0x7fffff) | 0x800000;
    shift = 150 - exponent;
    scaled = (uint64_t)significand * 255 + ((uint64_t)1 << (shift - 1));
    return MIN(255u, (unsigned int)(scaled >> shift));
}

static void vm_fill_color_quad(struct ig_vm_state *state, int left, int top,
                               int right, int bottom)
{
    struct ig_vm_attribute *color = &state->attributes[1];
    int y;
    int red = vm_fixed_color(state, color->pointer);
    int green = vm_fixed_color(state, color->pointer + 4);
    int blue = vm_fixed_color(state, color->pointer + 8);
    fb_data packed;

    packed = LCD_RGBPACK(red, green, blue);
    left = MAX(0, left);
    right = MIN(LCD_WIDTH, right);
    top = MAX(0, top);
    bottom = MIN(LCD_HEIGHT, bottom);
    for (y = top; y < bottom; ++y)
    {
        int screen_y = LCD_HEIGHT - 1 - y;
        int x;

        for (x = left; x < right; ++x)
            state->framebuffer[screen_y * LCD_WIDTH + x] = packed;
    }
}

static void vm_draw_textured_quad(struct ig_vm_state *state, int left,
                                  int top, int right, int bottom)
{
    struct ig_vm_attribute *texcoord = &state->attributes[1];
    struct ig_vm_texture *texture = state->bound_texture;
    int source_left = vm_fixed_integer(state, texcoord->pointer);
    int source_top = vm_fixed_integer(state, texcoord->pointer + 4);
    int source_right = vm_fixed_integer(state,
                                        texcoord->pointer + texcoord->stride);
    int source_bottom = vm_fixed_integer(
        state, texcoord->pointer + texcoord->stride * 2 + 4);
    int width = right - left;
    int height = bottom - top;
    int source_width = source_right - source_left;
    int source_height = source_bottom - source_top;
    int source_x_direction = source_width < 0 ? -1 : 1;
    int source_y_direction = source_height < 0 ? -1 : 1;
    int source_width_abs = source_width < 0 ? -source_width : source_width;
    int source_height_abs = source_height < 0 ? -source_height : source_height;
    int visible_left = MAX(0, left);
    int visible_right = MIN(LCD_WIDTH, right);
    int visible_top = MAX(0, top);
    int visible_bottom = MIN(LCD_HEIGHT, bottom);
    int source_y;
    int y_error;
    int y;

    if (width <= 0 || height <= 0 || !texture || !texture->pixels ||
        !texture->width || !texture->height)
        return;
    if (source_width < 0)
        ++state->flipped_texture_x;
    if (source_height < 0)
        ++state->flipped_texture_y;
    if (source_width_abs == width && source_height_abs == height &&
        MIN(source_left, source_right) >= 0 &&
        MIN(source_top, source_bottom) >= 0 &&
        MAX(source_left, source_right) <= (int)texture->width &&
        MAX(source_top, source_bottom) <= (int)texture->height)
    {
        int copy_width = visible_right - visible_left;

        for (y = visible_top; y < visible_bottom; ++y)
        {
            int screen_y = LCD_HEIGHT - 1 - y;
            int source_x = source_width >= 0 ?
                source_left + visible_left - left :
                source_left - 1 - (visible_left - left);
            int row_source_y = source_height >= 0 ?
                source_top + y - top : source_top - 1 - (y - top);
            const u32 *source = texture->pixels +
                row_source_y * texture->width + source_x;
            fb_data *destination = state->framebuffer +
                screen_y * LCD_WIDTH + visible_left;
            int x;

            for (x = 0; x < copy_width; ++x)
            {
                u32 pixel = source[x * source_x_direction];
#ifdef SIMULATOR
                if (state->gles_mode < IG_GLES_MODES)
                {
                    unsigned int alpha = (pixel >> 16) & 0xff;
                    unsigned int bucket = alpha == 255 ? 2 :
                                                  (alpha ? 1 : 0);

                    ++state->gles_mode_alpha[state->gles_mode][bucket];
                    ++texture->alpha[bucket];
                }
#endif
                vm_blend_cached_pixel(&destination[x], pixel, 255);
            }
        }
        ++state->fast_texture_quads;
        if (copy_width > 0 && visible_bottom > visible_top)
            state->fast_texture_pixels +=
                (unsigned long)copy_width * (visible_bottom - visible_top);
        ++texture->draws;
        return;
    }
    source_y = source_height >= 0 ?
        source_top + source_height_abs * (visible_top - top) / height :
        source_top - 1 - source_height_abs * (visible_top - top) / height;
    y_error = source_height_abs * (visible_top - top) % height;
    for (y = visible_top; y < visible_bottom; ++y)
    {
        int screen_y = LCD_HEIGHT - 1 - y;
        int source_x = source_width >= 0 ?
            source_left + source_width_abs * (visible_left - left) / width :
            source_left - 1 -
                source_width_abs * (visible_left - left) / width;
        int x_error = source_width_abs * (visible_left - left) % width;
        int x;

        if (source_y < 0 || source_y >= (int)texture->height)
            goto next_row;
        for (x = visible_left; x < visible_right; ++x)
        {
            size_t source_offset;
            u32 pixel;

            if (source_x < 0 || source_x >= (int)texture->width)
                goto next_pixel;
            source_offset = source_y * texture->width + source_x;
            pixel = texture->pixels[source_offset];
#ifdef SIMULATOR
            if (state->gles_mode < IG_GLES_MODES)
            {
                unsigned int alpha = (pixel >> 16) & 0xff;
                unsigned int bucket = alpha == 255 ? 2 :
                                              (alpha ? 1 : 0);

                ++state->gles_mode_alpha[state->gles_mode][bucket];
                ++texture->alpha[bucket];
            }
#endif
            vm_blend_cached_pixel(
                &state->framebuffer[screen_y * LCD_WIDTH + x], pixel, 255);
next_pixel:
            x_error += source_width_abs;
            while (x_error >= width)
            {
                x_error -= width;
                source_x += source_x_direction;
            }
        }
next_row:
        y_error += source_height_abs;
        while (y_error >= height)
        {
            y_error -= height;
            source_y += source_y_direction;
        }
    }
    ++texture->draws;
}

static bool vm_transform_vertex_full(struct ig_vm_state *state,
                                     const struct ig_vm_attribute *position,
                                     unsigned int vertex,
                                     struct ig_vm_raster_vertex *output)
{
    s32 input[4] = { 0, 0, 0, 65536 };
    s32 transformed[4];
    unsigned int component;

    for (component = 0; component < MIN(position->size, 4u); ++component)
    {
        u32 value;

        if (!vm_guest_u32(state, position->pointer +
                          vertex * position->stride + component * 4,
                          &value))
            return false;
        input[component] = (s32)value;
    }
    if (!state->transform_valid)
    {
        output->x = input[0];
        output->y = input[1];
        return true;
    }
    if (input[2] == 0 && input[3] == 65536)
    {
        static const unsigned char output_component[3] = { 0, 1, 3 };
        unsigned int selected;

        /* Retail vertex arrays are two-dimensional.  Preserve the matrix's
         * exact fixed-point accumulation order while omitting the guaranteed
         * zero Z term and the unused transformed Z output. */
        for (selected = 0; selected < ARRAYLEN(output_component); ++selected)
        {
            int64_t value;

            component = output_component[selected];
            value = (int64_t)state->transform[component] * input[0];
            value += (int64_t)state->transform[4 + component] * input[1];
            value += (int64_t)state->transform[12 + component] * 65536;
            transformed[component] = (s32)(value >> 16);
        }
    }
    else
    {
        for (component = 0; component < 4; ++component)
        {
            int64_t value = 0;
            unsigned int item;

            for (item = 0; item < 4; ++item)
                value += (int64_t)state->transform[item * 4 + component] *
                         input[item];
            transformed[component] = (s32)(value >> 16);
        }
    }
    ++state->transform_vertices;
    if (!transformed[3])
        return false;
    if (transformed[3] == 65536)
    {
        s32 normalized_x = transformed[0];
        s32 normalized_y = transformed[1];
        int64_t x = ((int64_t)normalized_x + 65536) * LCD_WIDTH / 2;
        int64_t y = ((int64_t)normalized_y + 65536) * LCD_HEIGHT / 2;

        ++state->transform_affine_vertices;
        if (x > 0x7fffffff || x < -0x80000000ll ||
            y > 0x7fffffff || y < -0x80000000ll)
            return false;
        output->x = (s32)x;
        output->y = (s32)y;
        return true;
    }
    {
        int64_t normalized_x = ((int64_t)transformed[0] << 16) /
                               transformed[3];
        int64_t normalized_y = ((int64_t)transformed[1] << 16) /
                               transformed[3];
        int64_t x = (normalized_x + 65536) * LCD_WIDTH / 2;
        int64_t y = (normalized_y + 65536) * LCD_HEIGHT / 2;

        ++state->transform_perspective_vertices;
        if (x > 0x7fffffff || x < -0x80000000ll ||
            y > 0x7fffffff || y < -0x80000000ll)
            return false;
        output->x = (s32)x;
        output->y = (s32)y;
    }
    return true;
}

static bool vm_load_raster_vertex(struct ig_vm_state *state,
                                  const struct ig_vm_attribute *position,
                                  const struct ig_vm_attribute *secondary,
                                  unsigned int vertex,
                                  struct ig_vm_raster_vertex *output)
{
    u32 address = secondary->pointer + vertex * secondary->stride;

    output->u = 0;
    output->v = 0;
    output->red = 255;
    output->green = 255;
    output->blue = 255;
    output->alpha = 255;
    if (!vm_transform_vertex_full(state, position, vertex, output))
        return false;
    if (secondary->size == 2)
    {
        const struct ig_vm_attribute *color = &state->attributes[2];
        u32 value;

        if (!vm_guest_u32(state, address, &value))
            return false;
        output->u = (s32)value;
        if (!vm_guest_u32(state, address + 4, &value))
            return false;
        output->v = (s32)value;
        if (color->pointer && color->type == 0x140c && color->size == 4)
        {
            u32 color_address = color->pointer + vertex * color->stride;

            output->red = vm_fixed_color(state, color_address);
            output->green = vm_fixed_color(state, color_address + 4);
            output->blue = vm_fixed_color(state, color_address + 8);
            output->alpha = vm_fixed_color(state, color_address + 12);
        }
    }
    else if (secondary->size == 4)
    {
        output->red = vm_fixed_color(state, address);
        output->green = vm_fixed_color(state, address + 4);
        output->blue = vm_fixed_color(state, address + 8);
        output->alpha = vm_fixed_color(state, address + 12);
    }
    return true;
}

static int64_t vm_raster_edge(s32 ax, s32 ay, s32 bx, s32 by,
                              s32 px, s32 py)
{
    return (int64_t)(px - ax) * (by - ay) -
           (int64_t)(py - ay) * (bx - ax);
}

static inline void vm_raster_divmod(int64_t numerator, int64_t denominator,
                                    int64_t *quotient, int64_t *remainder)
{
    *quotient = numerator / denominator;
    *remainder = numerator % denominator;
    /* Keep a Euclidean remainder so the incremental update remains exact
     * even while traversing the part of a bounding box outside a triangle. */
    if (*remainder < 0)
    {
        --*quotient;
        *remainder += denominator;
    }
}

static inline void vm_raster_advance(int64_t *value, int64_t *remainder,
                                     int64_t value_step,
                                     int64_t remainder_step, int64_t area)
{
    *value += value_step;
    *remainder += remainder_step;
    if (*remainder >= area)
    {
        ++*value;
        *remainder -= area;
    }
}

static inline void vm_raster_advance32(s32 *value, s32 *remainder,
                                       s32 value_step,
                                       s32 remainder_step, s32 area)
{
    *value += value_step;
    *remainder += remainder_step;
    if (*remainder >= area)
    {
        ++*value;
        *remainder -= area;
    }
}

static inline void vm_axis_advance32(s32 *value, s32 *remainder,
                                     s32 value_step,
                                     s32 remainder_step, s32 span)
{
    *value += value_step;
    *remainder += remainder_step;
    if (*remainder >= span)
    {
        ++*value;
        *remainder -= span;
    }
    else if (*remainder <= -span)
    {
        --*value;
        *remainder += span;
    }
}

static inline bool vm_raster_clip_edge(int64_t weight, int64_t step,
                                       int base_x, int right,
                                       int *span_left, int *span_right)
{
    int64_t distance;

    if (step > 0)
    {
        if (weight >= 0)
            return true;
        distance = (-weight + step - 1) / step;
        if (distance >= right - base_x)
            return false;
        *span_left = MAX(*span_left, base_x + (int)distance);
        return true;
    }
    if (step < 0)
    {
        if (weight < 0)
            return false;
        distance = weight / -step;
        if (distance < right - base_x)
            *span_right = MIN(*span_right,
                              base_x + (int)distance + 1);
        return *span_left < *span_right;
    }
    return weight >= 0;
}

static void ICODE_ATTR vm_raster_triangle(
    struct ig_vm_state *state,
    const struct ig_vm_raster_vertex *a,
    const struct ig_vm_raster_vertex *b,
    const struct ig_vm_raster_vertex *c,
    bool textured)
{
    /* Four fractional screen bits provide stable subpixel coverage without
     * overflowing 64-bit barycentric interpolation on the 320x240 target. */
    s32 ax = a->x >> 12;
    s32 ay = a->y >> 12;
    s32 bx = b->x >> 12;
    s32 by = b->y >> 12;
    s32 cx = c->x >> 12;
    s32 cy = c->y >> 12;
    int left = MIN(a->x, MIN(b->x, c->x)) >> 16;
    int right = (MAX(a->x, MAX(b->x, c->x)) + 65535) >> 16;
    int bottom = MIN(a->y, MIN(b->y, c->y)) >> 16;
    int top = (MAX(a->y, MAX(b->y, c->y)) + 65535) >> 16;
    int64_t area = vm_raster_edge(ax, ay, bx, by, cx, cy);
    int orientation = 1;
    struct ig_vm_texture *texture = textured ? vm_draw_texture(state) : NULL;
    bool white_vertex = a->red == 255 && a->green == 255 &&
        a->blue == 255 && a->alpha == 255 &&
        b->red == 255 && b->green == 255 && b->blue == 255 &&
        b->alpha == 255 && c->red == 255 && c->green == 255 &&
        c->blue == 255 && c->alpha == 255;
    bool opaque_vertex = a->alpha == 255 && b->alpha == 255 &&
        c->alpha == 255;
    bool white_uniform = state->gles_mode >= IG_GLES_MODES ||
        (state->gles_uniform_color[state->gles_mode][0] == 255 &&
         state->gles_uniform_color[state->gles_mode][1] == 255 &&
         state->gles_uniform_color[state->gles_mode][2] == 255 &&
         state->gles_uniform_color[state->gles_mode][3] == 255);
    bool direct_opaque = textured && texture && texture->all_opaque &&
        !vm_mode_uses_both_textures(state) && opaque_vertex &&
        (state->gles_mode >= IG_GLES_MODES ||
         state->gles_uniform_color[state->gles_mode][3] == 255);
    int64_t step_a;
    int64_t step_b;
    int64_t step_c;
    int64_t row_step_a;
    int64_t row_step_b;
    int64_t row_step_c;
    int64_t row_weight_a;
    int64_t row_weight_b;
    int64_t row_weight_c;
    int64_t component_a[6];
    int64_t component_b[6];
    int64_t component_c[6];
    int64_t value_step[6];
    int64_t remainder_step[6];
    int64_t row_value[6];
    int64_t row_remainder[6];
    int64_t row_value_step[6];
    int64_t row_remainder_step[6];
    unsigned int components;
    unsigned int component;
    int y;

    if (!area)
        return;
    if (area < 0)
    {
        area = -area;
        orientation = -1;
    }
    left = MAX(0, left);
    right = MIN(LCD_WIDTH, right);
    bottom = MAX(0, bottom);
    top = MIN(LCD_HEIGHT, top);
    if (left >= right || bottom >= top)
        return;
    step_a = (int64_t)orientation * 16 * (cy - by);
    step_b = (int64_t)orientation * 16 * (ay - cy);
    step_c = (int64_t)orientation * 16 * (by - ay);
    row_step_a = (int64_t)orientation * -16 * (cx - bx);
    row_step_b = (int64_t)orientation * -16 * (ax - cx);
    row_step_c = (int64_t)orientation * -16 * (bx - ax);
    if (textured)
    {
        component_a[0] = a->u;
        component_b[0] = b->u;
        component_c[0] = c->u;
        component_a[1] = a->v;
        component_b[1] = b->v;
        component_c[1] = c->v;
        components = white_vertex ? 2 : (opaque_vertex ? 5 : 6);
        if (!white_vertex)
        {
            component_a[2] = a->red;
            component_b[2] = b->red;
            component_c[2] = c->red;
            component_a[3] = a->green;
            component_b[3] = b->green;
            component_c[3] = c->green;
            component_a[4] = a->blue;
            component_b[4] = b->blue;
            component_c[4] = c->blue;
            component_a[5] = a->alpha;
            component_b[5] = b->alpha;
            component_c[5] = c->alpha;
        }
    }
    else
    {
        component_a[0] = a->red;
        component_b[0] = b->red;
        component_c[0] = c->red;
        component_a[1] = a->green;
        component_b[1] = b->green;
        component_c[1] = c->green;
        component_a[2] = a->blue;
        component_b[2] = b->blue;
        component_c[2] = c->blue;
        component_a[3] = a->alpha;
        component_b[3] = b->alpha;
        component_c[3] = c->alpha;
        components = 4;
    }
    for (component = 0; component < components; ++component)
    {
        int64_t numerator_step = step_a * component_a[component] +
            step_b * component_b[component] +
            step_c * component_c[component];
        int64_t numerator_row_step =
            row_step_a * component_a[component] +
            row_step_b * component_b[component] +
            row_step_c * component_c[component];

        vm_raster_divmod(numerator_step, area, &value_step[component],
                         &remainder_step[component]);
        vm_raster_divmod(numerator_row_step, area,
                         &row_value_step[component],
                         &row_remainder_step[component]);
    }

    {
        s32 px = (left << 4) + 8;
        s32 py = (bottom << 4) + 8;

        row_weight_a = orientation *
            vm_raster_edge(bx, by, cx, cy, px, py);
        row_weight_b = orientation *
            vm_raster_edge(cx, cy, ax, ay, px, py);
        row_weight_c = orientation *
            vm_raster_edge(ax, ay, bx, by, px, py);
    }
    for (component = 0; component < components; ++component)
    {
        int64_t numerator =
            row_weight_a * component_a[component] +
            row_weight_b * component_b[component] +
            row_weight_c * component_c[component];

        vm_raster_divmod(numerator, area, &row_value[component],
                         &row_remainder[component]);
    }

    for (y = bottom; y < top; ++y)
    {
        int64_t weight_a = row_weight_a;
        int64_t weight_b = row_weight_b;
        int64_t weight_c = row_weight_c;
        int64_t value[6];
        int64_t remainder[6];
        int span_left = left;
        int span_right = right;
        int span_offset;
        int skip;
        int x;

        if (!vm_raster_clip_edge(weight_a, step_a, left, right,
                                 &span_left, &span_right) ||
            !vm_raster_clip_edge(weight_b, step_b, left, right,
                                 &span_left, &span_right) ||
            !vm_raster_clip_edge(weight_c, step_c, left, right,
                                 &span_left, &span_right) ||
            span_left >= span_right)
        {
            span_left = left;
            span_right = left;
        }
        if (span_left < span_right)
        {
            span_offset = span_left - left;
            state->general_raster_pixels += span_right - span_left;
            for (component = 0; component < components; ++component)
            {
                value[component] = row_value[component];
                remainder[component] = row_remainder[component];
            }
            for (skip = 0; skip < span_offset; ++skip)
                for (component = 0; component < components; ++component)
                    vm_raster_advance(&value[component],
                                      &remainder[component],
                                      value_step[component],
                                      remainder_step[component], area);

            if (textured && direct_opaque && opaque_vertex && white_uniform &&
                texture && texture->pixels && texture->width &&
                texture->height && components <= 5 &&
                area <= 0x3fffffffu)
            {
                int width = span_right - span_left;
                bool narrow = true;

                for (component = 0; component < components; ++component)
                {
                    int64_t endpoint = value[component] +
                        value_step[component] * width;

                    narrow = narrow &&
                        value[component] >= (-2147483647LL - 1) &&
                        value[component] <= 2147483647LL &&
                        value_step[component] >= (-2147483647LL - 1) &&
                        value_step[component] <= 2147483647LL &&
                        remainder[component] >= 0 &&
                        remainder[component] <= 2147483647LL &&
                        remainder_step[component] >= 0 &&
                        remainder_step[component] <= 2147483647LL &&
                        endpoint >= (-2147483647LL - 1) &&
                        endpoint + width <= 2147483647LL;
                }
                if (narrow)
                {
                    s32 narrow_value[5];
                    s32 narrow_remainder[5];
                    s32 narrow_value_step[5];
                    s32 narrow_remainder_step[5];
                    s32 narrow_area = (s32)area;
                    fb_data *destination = state->framebuffer +
                        (LCD_HEIGHT - 1 - y) * LCD_WIDTH + span_left;

                    for (component = 0; component < components; ++component)
                    {
                        narrow_value[component] = (s32)value[component];
                        narrow_remainder[component] =
                            (s32)remainder[component];
                        narrow_value_step[component] =
                            (s32)value_step[component];
                        narrow_remainder_step[component] =
                            (s32)remainder_step[component];
                    }
                    for (x = span_left; x < span_right; ++x)
                    {
                        int source_x = narrow_value[0] >> 16;
                        int source_y = narrow_value[1] >> 16;
                        u32 pixel;

                        source_x = MIN((int)texture->width - 1,
                                       MAX(0, source_x));
                        source_y = MIN((int)texture->height - 1,
                                       MAX(0, source_y));
                        pixel = texture->pixels[
                            source_y * texture->width + source_x];
                        if (components > 2)
                            *destination++ = vm_modulate_opaque_rgb565(
                                pixel,
                                (unsigned int)narrow_value[2],
                                (unsigned int)narrow_value[3],
                                (unsigned int)narrow_value[4]);
                        else
                            *destination++ = (fb_data)pixel;
#ifdef SIMULATOR
                        if (state->gles_mode < IG_GLES_MODES)
                            ++state->gles_mode_alpha[state->gles_mode][2];
                        ++texture->alpha[2];
#endif
                        vm_raster_advance32(
                            &narrow_value[0], &narrow_remainder[0],
                            narrow_value_step[0], narrow_remainder_step[0],
                            narrow_area);
                        vm_raster_advance32(
                            &narrow_value[1], &narrow_remainder[1],
                            narrow_value_step[1], narrow_remainder_step[1],
                            narrow_area);
                        if (components > 2)
                        {
                            vm_raster_advance32(
                                &narrow_value[2], &narrow_remainder[2],
                                narrow_value_step[2],
                                narrow_remainder_step[2], narrow_area);
                            vm_raster_advance32(
                                &narrow_value[3], &narrow_remainder[3],
                                narrow_value_step[3],
                                narrow_remainder_step[3], narrow_area);
                            vm_raster_advance32(
                                &narrow_value[4], &narrow_remainder[4],
                                narrow_value_step[4],
                                narrow_remainder_step[4], narrow_area);
                        }
                    }
                    goto raster_row_complete;
                }
            }

            for (x = span_left; x < span_right; ++x)
            {
                fb_data *destination = state->framebuffer +
                    (LCD_HEIGHT - 1 - y) * LCD_WIDTH + x;

                if (textured)
                {
                    if (texture && texture->pixels && texture->width &&
                        texture->height)
                    {
                        int source_x = (s32)value[0] >> 16;
                        int source_y = (s32)value[1] >> 16;
                        u32 pixel;
#ifdef SIMULATOR
                        unsigned int alpha;
                        unsigned int bucket;
#endif

                        source_x = MIN((int)texture->width - 1,
                                       MAX(0, source_x));
                        source_y = MIN((int)texture->height - 1,
                                       MAX(0, source_y));
                        pixel = texture->pixels[
                            source_y * texture->width + source_x];
                        if (!white_uniform)
                            pixel = vm_modulate_cached_pixel(state, pixel);
                        if (!white_vertex)
                            pixel = vm_modulate_cached_vertex(
                                pixel, (unsigned int)value[2],
                                (unsigned int)value[3],
                                (unsigned int)value[4],
                                (unsigned int)value[5]);
#ifdef SIMULATOR
                        alpha = (pixel >> 16) & 0xff;
                        bucket = alpha == 255 ? 2 : (alpha ? 1 : 0);
                        if (state->gles_mode < IG_GLES_MODES)
                            ++state->gles_mode_alpha[
                                state->gles_mode][bucket];
                        ++texture->alpha[bucket];
#endif
                        if (direct_opaque)
                            *destination = (fb_data)pixel;
                        else
                            vm_blend_cached_pixel(destination, pixel, 255);
                    }
                }
                else
                {
                    u32 pixel = ((u32)value[3] << 16) |
                        LCD_RGBPACK((unsigned int)value[0],
                                    (unsigned int)value[1],
                                    (unsigned int)value[2]);

                    vm_blend_cached_pixel(destination, pixel, 255);
                }
                for (component = 0; component < components; ++component)
                    vm_raster_advance(&value[component],
                                      &remainder[component],
                                      value_step[component],
                                      remainder_step[component], area);
            }
raster_row_complete:
            ;
        }
        row_weight_a += row_step_a;
        row_weight_b += row_step_b;
        row_weight_c += row_step_c;
        for (component = 0; component < components; ++component)
            vm_raster_advance(&row_value[component],
                              &row_remainder[component],
                              row_value_step[component],
                              row_remainder_step[component], area);
    }
}

static void vm_bilinear_channel(unsigned int bottom_left,
                                unsigned int bottom_right,
                                unsigned int top_left,
                                unsigned int top_right,
                                int64_t x_start, int64_t y_start,
                                int64_t x_step, int64_t y_step,
                                int64_t *row, int64_t *column_step,
                                int64_t *row_step,
                                int64_t *column_step_row_step)
{
    int64_t horizontal = (int)bottom_right - (int)bottom_left;
    int64_t vertical = (int)top_left - (int)bottom_left;
    int64_t cross = (int)top_right - (int)bottom_right -
        (int)top_left + (int)bottom_left;

    *row = ((int64_t)bottom_left << 16) + horizontal * x_start +
        vertical * y_start +
        ((cross * x_start * y_start) >> 16);
    *column_step = horizontal * x_step +
        ((cross * x_step * y_start) >> 16);
    *row_step = vertical * y_step +
        ((cross * x_start * y_step) >> 16);
    *column_step_row_step = (cross * x_step * y_step) >> 16;
}

static bool vm_mode_uses_both_textures(const struct ig_vm_state *state)
{
    /* A texture remaining bound on the inactive unit does not make a draw
     * multi-textured.  Vortex keeps its screen mask resident on unit one
     * after the transition into the arena, while its sprite, font and HUD
     * programs continue to sample only the currently active unit.  Modes 20
     * and 32 are the two mask/composite programs observed to consume both
     * bindings. */
    return state->is_vortex &&
        (state->gles_mode == 20 || state->gles_mode == 32) &&
        state->bound_textures[0] && state->bound_textures[1];
}

static u32 *vm_vortex_composite_pixels(
    struct ig_vm_state *state, const struct ig_vm_texture *source,
    const struct ig_vm_texture *mask, unsigned int red,
    unsigned int green, unsigned int blue)
{
    size_t pixels = (size_t)LCD_WIDTH * LCD_HEIGHT;
    size_t bytes = pixels * sizeof(*state->vortex_composite_cache);
    unsigned int x;
    unsigned int y;

    if (state->vortex_composite_valid &&
        state->vortex_composite_source_id == source->id &&
        state->vortex_composite_mask_id == mask->id &&
        state->vortex_composite_source_revision == source->revision &&
        state->vortex_composite_mask_revision == mask->revision &&
        state->vortex_composite_red == red &&
        state->vortex_composite_green == green &&
        state->vortex_composite_blue == blue)
        return state->vortex_composite_cache;

    if (!state->vortex_composite_cache)
    {
        if (state->texture_storage_used > state->texture_storage_size ||
            bytes > state->texture_storage_size -
                    state->texture_storage_used)
            return NULL;
        state->vortex_composite_cache =
            (u32 *)(state->texture_storage + state->texture_storage_used);
        state->texture_storage_used += bytes;
    }

    state->vortex_composite_left = LCD_WIDTH;
    state->vortex_composite_right = 0;
    state->vortex_composite_top = LCD_HEIGHT;
    state->vortex_composite_bottom = 0;
    for (y = 0; y < LCD_HEIGHT; ++y)
    {
        for (x = 0; x < LCD_WIDTH; ++x)
        {
            size_t pixel = (size_t)y * LCD_WIDTH + x;
            u32 source_pixel = source->pixels[pixel];
            unsigned int alpha = vm_div255(
                ((source_pixel >> 16) & 0xff) *
                ((mask->pixels[pixel] >> 16) & 0xff));

            if (red != 255 || green != 255 || blue != 255)
                source_pixel = vm_modulate_cached_vertex(
                    source_pixel, red, green, blue, 255);
            state->vortex_composite_cache[pixel] =
                (source_pixel & 0xffff) | ((u32)alpha << 16);
            if (alpha)
            {
                state->vortex_composite_left =
                    MIN(state->vortex_composite_left, x);
                state->vortex_composite_right =
                    MAX(state->vortex_composite_right, x + 1);
                state->vortex_composite_top =
                    MIN(state->vortex_composite_top, y);
                state->vortex_composite_bottom =
                    MAX(state->vortex_composite_bottom, y + 1);
            }
        }
    }
    state->vortex_composite_source_id = source->id;
    state->vortex_composite_mask_id = mask->id;
    state->vortex_composite_source_revision = source->revision;
    state->vortex_composite_mask_revision = mask->revision;
    state->vortex_composite_red = red;
    state->vortex_composite_green = green;
    state->vortex_composite_blue = blue;
    state->vortex_composite_valid = true;
    return state->vortex_composite_cache;
}

static bool ICODE_ATTR vm_raster_axis_quad(
    struct ig_vm_state *state,
    const struct ig_vm_raster_vertex vertices[4],
    bool textured)
{
    const struct ig_vm_raster_vertex *bottom_left = NULL;
    const struct ig_vm_raster_vertex *bottom_right = NULL;
    const struct ig_vm_raster_vertex *top_left = NULL;
    const struct ig_vm_raster_vertex *top_right = NULL;
    struct ig_vm_texture *texture = textured ? vm_draw_texture(state) : NULL;
    s32 min_x = vertices[0].x;
    s32 max_x = vertices[0].x;
    s32 min_y = vertices[0].y;
    s32 max_y = vertices[0].y;
    int left;
    int right;
    int bottom;
    int top;
    unsigned int vertex;
    bool direct_opaque;
    bool white_vertex;
    bool opaque_vertex;
    bool transparent_vertex;
    bool constant_vertex;
    bool multitextured = vm_mode_uses_both_textures(state);
    struct ig_vm_texture *mask_texture = multitextured ?
        state->bound_textures[1] : NULL;
    bool white_uniform = state->gles_mode >= IG_GLES_MODES ||
        (state->gles_uniform_color[state->gles_mode][0] == 255 &&
         state->gles_uniform_color[state->gles_mode][1] == 255 &&
         state->gles_uniform_color[state->gles_mode][2] == 255 &&
         state->gles_uniform_color[state->gles_mode][3] == 255);

    if (multitextured)
        texture = state->bound_textures[0];
#ifdef SIMULATOR
    /* Shader-generated screen coordinates and the second sampler have no
     * representation in the generic triangle reference. Keep those three
     * programs on their shared semantic path while comparing all ordinary
     * axis quads against the exact rasterizer. */
    if (state->reference_raster &&
        !(state->is_vortex && (state->gles_mode == 19 ||
                               state->gles_mode == 20 ||
                               state->gles_mode == 32)))
        return false;
#endif

    for (vertex = 1; vertex < 4; ++vertex)
    {
        min_x = MIN(min_x, vertices[vertex].x);
        max_x = MAX(max_x, vertices[vertex].x);
        min_y = MIN(min_y, vertices[vertex].y);
        max_y = MAX(max_y, vertices[vertex].y);
    }
    if (min_x == max_x || min_y == max_y)
        return true;
    for (vertex = 0; vertex < 4; ++vertex)
    {
        const struct ig_vm_raster_vertex *item = &vertices[vertex];
        s32 distance_left = item->x - min_x;
        s32 distance_right = max_x - item->x;
        s32 distance_bottom = item->y - min_y;
        s32 distance_top = max_y - item->y;
        bool at_left;
        bool at_bottom;
        const s32 tolerance = 1 << 12;

        if (MIN(distance_left, distance_right) > tolerance ||
            MIN(distance_bottom, distance_top) > tolerance)
        {
            ++state->fast_rejects[0];
            return false;
        }
        at_left = distance_left <= distance_right;
        at_bottom = distance_bottom <= distance_top;
        if (at_left && at_bottom && !bottom_left)
            bottom_left = item;
        else if (!at_left && at_bottom && !bottom_right)
            bottom_right = item;
        else if (at_left && !at_bottom && !top_left)
            top_left = item;
        else if (!at_left && !at_bottom && !top_right)
            top_right = item;
        else
        {
            ++state->fast_rejects[1];
            return false;
        }
    }
    if (!bottom_left || !bottom_right || !top_left || !top_right)
    {
        ++state->fast_rejects[1];
        return false;
    }
    if (textured &&
        (bottom_left->u != top_left->u ||
         bottom_right->u != top_right->u ||
         bottom_left->v != bottom_right->v ||
         top_left->v != top_right->v))
    {
        ++state->fast_rejects[3];
        return false;
    }

    left = MAX(0, min_x >> 16);
    right = MIN(LCD_WIDTH, (max_x + 65535) >> 16);
    bottom = MAX(0, min_y >> 16);
    top = MIN(LCD_HEIGHT, (max_y + 65535) >> 16);
    if (left >= right || bottom >= top)
        return true;
    if (textured && (!texture || !texture->pixels || !texture->width ||
                     !texture->height ||
                     (multitextured &&
                      (!mask_texture || !mask_texture->pixels ||
                       !mask_texture->width || !mask_texture->height))))
        return true;
    white_vertex = true;
    opaque_vertex = true;
    transparent_vertex = true;
    constant_vertex = true;
    for (vertex = 0; vertex < 4; ++vertex)
    {
        white_vertex = white_vertex && vertices[vertex].red == 255 &&
            vertices[vertex].green == 255 &&
            vertices[vertex].blue == 255 &&
            vertices[vertex].alpha == 255;
        opaque_vertex = opaque_vertex && vertices[vertex].alpha == 255;
        transparent_vertex = transparent_vertex &&
            vertices[vertex].alpha == 0;
        constant_vertex = constant_vertex &&
            vertices[vertex].red == vertices[0].red &&
            vertices[vertex].green == vertices[0].green &&
            vertices[vertex].blue == vertices[0].blue &&
            vertices[vertex].alpha == vertices[0].alpha;
    }
    if (state->is_vortex && state->gles_mode == 19 && white_vertex &&
        white_uniform && texture->all_opaque &&
        texture->width == LCD_WIDTH && texture->height == LCD_HEIGHT)
    {
        int y;

        /* The background shader derives coordinates from gl_FragCoord; its
         * vertex texture coordinates are intentionally all zero.  With the
         * native-sized opaque RGB565 background this is an exact row copy. */
        for (y = bottom; y < top; ++y)
        {
            int source_y = LCD_HEIGHT - 1 - y;
            const u32 *source = texture->pixels +
                source_y * LCD_WIDTH + left;
            fb_data *destination = state->framebuffer +
                source_y * LCD_WIDTH + left;
            int x;

            for (x = left; x < right; ++x)
                *destination++ = (fb_data)*source++;
        }
        ++state->fast_texture_quads;
        state->fast_texture_pixels +=
            (unsigned long)(right - left) * (top - bottom);
        state->fast_opaque_pixels +=
            (unsigned long)(right - left) * (top - bottom);
        ++texture->draws;
        return true;
    }
    if (multitextured && constant_vertex && opaque_vertex && white_uniform &&
        texture->width == LCD_WIDTH && texture->height == LCD_HEIGHT &&
        mask_texture->width == LCD_WIDTH &&
        mask_texture->height == LCD_HEIGHT)
    {
        u32 *composite = vm_vortex_composite_pixels(
            state, texture, mask_texture, vertices[0].red,
            vertices[0].green, vertices[0].blue);
        unsigned long opaque_pixels = 0;
        unsigned long blended_pixels = 0;
        unsigned long skipped_pixels =
            (unsigned long)(right - left) * (top - bottom);
        int y;

        if (!composite)
            goto general_axis_quad;
        left = MAX(left, (int)state->vortex_composite_left);
        right = MIN(right, (int)state->vortex_composite_right);
        bottom = MAX(bottom,
                     LCD_HEIGHT - (int)state->vortex_composite_bottom);
        top = MIN(top, LCD_HEIGHT - (int)state->vortex_composite_top);

        /* Vortex's source, bgAlpha mask and constant tint are stable across
         * arena frames. Cache their exact combined RGB565/alpha result once,
         * then touch only the mask's non-empty bounding box on later draws. */
        for (y = bottom; y < top; ++y)
        {
            int source_y = LCD_HEIGHT - 1 - y;
            const u32 *source = composite +
                source_y * LCD_WIDTH + left;
            fb_data *destination = state->framebuffer +
                source_y * LCD_WIDTH + left;
            int x;

            for (x = left; x < right; ++x)
            {
                unsigned int alpha = (*source >> 16) & 0xff;

                if (!alpha)
                {
                    ++source;
                    ++destination;
                    continue;
                }
                --skipped_pixels;
                if (alpha == 255)
                {
                    *destination = (fb_data)*source;
                    ++opaque_pixels;
                }
                else
                {
                    vm_blend_cached_pixel(destination, *source, 255);
                    ++blended_pixels;
                }
                ++source;
                ++destination;
            }
        }
        ++state->fast_texture_quads;
        state->fast_texture_pixels += opaque_pixels + blended_pixels +
                                      skipped_pixels;
        state->fast_opaque_pixels += opaque_pixels;
        state->fast_blended_pixels += blended_pixels;
        state->fast_skipped_pixels += skipped_pixels;
        ++texture->draws;
        return true;
    }
general_axis_quad:
    direct_opaque = textured && !multitextured && texture->all_opaque &&
        (state->gles_mode >= IG_GLES_MODES ||
         state->gles_uniform_color[state->gles_mode][3] == 255) &&
        opaque_vertex;
    if ((!white_uniform &&
         state->gles_uniform_color[state->gles_mode][3] == 0) ||
        transparent_vertex ||
        (textured && !multitextured && texture->all_transparent))
    {
        unsigned long pixels =
            (unsigned long)(right - left) * (top - bottom);

        state->fast_skipped_pixels += pixels;
        if (textured)
        {
            ++state->fast_texture_quads;
            state->fast_texture_pixels += pixels;
            ++texture->draws;
        }
        return true;
    }

    {
        int64_t x_span = (max_x - min_x) >> 12;
        int64_t y_span = (max_y - min_y) >> 12;
        int64_t u_delta = textured ?
            (int64_t)bottom_right->u - bottom_left->u : 0;
        int64_t v_delta = textured ?
            (int64_t)top_left->v - bottom_left->v : 0;
        int64_t u_step_numerator = u_delta * 16;
        int64_t v_step_numerator = v_delta * 16;
        int64_t u_start_numerator = u_delta *
            (((int64_t)left << 4) + 8 - (min_x >> 12));
        int64_t v_numerator = v_delta *
            (((int64_t)bottom << 4) + 8 - (min_y >> 12));
        int64_t u_start;
        int64_t u_start_remainder;
        int64_t u_step;
        int64_t u_step_remainder;
        int64_t color_row[4];
        int64_t color_column_step[4];
        int64_t color_row_step[4];
        int64_t color_column_step_row_step[4];
        int64_t normalized_x_start;
        int64_t normalized_y_start;
        int64_t normalized_x_step;
        int64_t normalized_y_step;
        static const unsigned char component_offset[4] = {
            offsetof(struct ig_vm_raster_vertex, red),
            offsetof(struct ig_vm_raster_vertex, green),
            offsetof(struct ig_vm_raster_vertex, blue),
            offsetof(struct ig_vm_raster_vertex, alpha),
        };
        unsigned int component;
        int y;

        if (x_span <= 0 || y_span <= 0)
            return false;
        u_start = (int64_t)bottom_left->u +
            u_start_numerator / x_span;
        u_start_remainder = u_start_numerator % x_span;
        u_step = u_step_numerator / x_span;
        u_step_remainder = u_step_numerator % x_span;
        if (!white_vertex)
        {
            normalized_x_start =
                ((((int64_t)left << 4) + 8 - (min_x >> 12)) << 16) /
                x_span;
            normalized_y_start =
                ((((int64_t)bottom << 4) + 8 - (min_y >> 12)) << 16) /
                y_span;
            normalized_x_step = ((int64_t)16 << 16) / x_span;
            normalized_y_step = ((int64_t)16 << 16) / y_span;
            for (component = 0; component < 4; ++component)
            {
                const unsigned int *bl = (const unsigned int *)
                    ((const unsigned char *)bottom_left +
                     component_offset[component]);
                const unsigned int *br = (const unsigned int *)
                    ((const unsigned char *)bottom_right +
                     component_offset[component]);
                const unsigned int *tl = (const unsigned int *)
                    ((const unsigned char *)top_left +
                     component_offset[component]);
                const unsigned int *tr = (const unsigned int *)
                    ((const unsigned char *)top_right +
                     component_offset[component]);

                vm_bilinear_channel(
                    *bl, *br, *tl, *tr, normalized_x_start,
                    normalized_y_start, normalized_x_step,
                    normalized_y_step, &color_row[component],
                    &color_column_step[component],
                    &color_row_step[component],
                    &color_column_step_row_step[component]);
            }
        }
        for (y = bottom; y < top; ++y)
        {
            int source_y = multitextured ?
                (int)texture->height - 1 -
                    (texture->height == LCD_HEIGHT ? y :
                     y * (int)texture->height / LCD_HEIGHT) :
                (textured ?
                 (s32)((int64_t)bottom_left->v +
                       v_numerator / y_span) >> 16 : 0);
            int64_t u = u_start;
            int64_t u_remainder = u_start_remainder;
            int64_t color[4] = { 0, 0, 0, 0 };
            fb_data *destination = state->framebuffer +
                (LCD_HEIGHT - 1 - y) * LCD_WIDTH + left;
            int x;

            if (!white_vertex)
                for (component = 0; component < 4; ++component)
                    color[component] = color_row[component];
            if (textured)
                source_y = MIN((int)texture->height - 1,
                               MAX(0, source_y));
            if (textured && !multitextured && direct_opaque &&
                white_vertex && white_uniform &&
                u_step_remainder == 0 &&
                (u_step == 65536 || u_step == -65536))
            {
                int source_x = (s32)u >> 16;
                int width = right - left;
                bool in_bounds = u_step > 0 ?
                    source_x >= 0 &&
                    source_x + width <= (int)texture->width :
                    source_x < (int)texture->width &&
                    source_x - width + 1 >= 0;

                if (in_bounds)
                {
                    const u32 *source = texture->pixels +
                        source_y * texture->width + source_x;
                    int source_step = u_step > 0 ? 1 : -1;

                    for (x = left; x < right; ++x)
                    {
                        *destination++ = (fb_data)*source;
                        source += source_step;
                    }
                    goto axis_row_complete;
                }
            }
            if (textured && !multitextured && white_vertex &&
                white_uniform && x_span <= 0x3fffffffu &&
                u_start >= (-2147483647LL - 1) &&
                u_start <= 2147483647LL &&
                u_step >= (-2147483647LL - 1) &&
                u_step <= 2147483647LL &&
                u_start_remainder >= (-2147483647LL - 1) &&
                u_start_remainder <= 2147483647LL &&
                u_step_remainder >= (-2147483647LL - 1) &&
                u_step_remainder <= 2147483647LL)
            {
                int width = right - left;
                int64_t endpoint = u_start + u_step * width;

                if (endpoint >= (-2147483647LL - 1) &&
                    endpoint + width <= 2147483647LL)
                {
                    s32 narrow_u = (s32)u_start;
                    s32 narrow_remainder = (s32)u_start_remainder;
                    s32 narrow_step = (s32)u_step;
                    s32 narrow_remainder_step = (s32)u_step_remainder;
                    s32 narrow_span = (s32)x_span;

                    for (x = left; x < right; ++x)
                    {
                        int source_x = narrow_u >> 16;
                        u32 pixel;

                        source_x = MIN((int)texture->width - 1,
                                       MAX(0, source_x));
                        pixel = texture->pixels[
                            source_y * texture->width + source_x];
                        if (direct_opaque)
                            *destination++ = (fb_data)pixel;
                        else
                        {
                            vm_blend_cached_pixel(destination, pixel, 255);
                            ++destination;
                        }
                        vm_axis_advance32(
                            &narrow_u, &narrow_remainder, narrow_step,
                            narrow_remainder_step, narrow_span);
                    }
                    goto axis_row_complete;
                }
            }
            for (x = left; x < right; ++x)
            {
                u32 pixel;
                unsigned int vertex_red = 255;
                unsigned int vertex_green = 255;
                unsigned int vertex_blue = 255;
                unsigned int vertex_alpha = 255;

                if (!white_vertex)
                {
                    vertex_red = MIN(255, MAX(0,
                        (int)(color[0] >> 16)));
                    vertex_green = MIN(255, MAX(0,
                        (int)(color[1] >> 16)));
                    vertex_blue = MIN(255, MAX(0,
                        (int)(color[2] >> 16)));
                    vertex_alpha = MIN(255, MAX(0,
                        (int)(color[3] >> 16)));
                }

                if (textured)
                {
                    int source_x = multitextured ?
                        (texture->width == LCD_WIDTH ? x :
                         x * (int)texture->width / LCD_WIDTH) :
                        (s32)u >> 16;

                    source_x = MIN((int)texture->width - 1,
                                   MAX(0, source_x));
                    pixel = texture->pixels[
                        source_y * texture->width + source_x];
                    if (multitextured)
                    {
                        int mask_x = mask_texture->width == LCD_WIDTH ? x :
                            x * (int)mask_texture->width / LCD_WIDTH;
                        int mask_y = (int)mask_texture->height - 1 -
                            (mask_texture->height == LCD_HEIGHT ? y :
                             y * (int)mask_texture->height / LCD_HEIGHT);
                        unsigned int mask_alpha =
                            (mask_texture->pixels[
                                mask_y * mask_texture->width + mask_x] >> 16) &
                            0xff;
                        unsigned int alpha = vm_div255(
                            ((pixel >> 16) & 0xff) * mask_alpha);

                        pixel = (pixel & 0xffff) | ((u32)alpha << 16);
                    }
                    if (!white_uniform)
                        pixel = vm_modulate_cached_pixel(state, pixel);
                    if (!white_vertex)
                    {
                        pixel = vm_modulate_cached_vertex(
                            pixel, vertex_red, vertex_green,
                            vertex_blue, vertex_alpha);
                    }
                }
                else
                {
                    pixel = ((u32)vertex_alpha << 16) |
                        LCD_RGBPACK(vertex_red, vertex_green, vertex_blue);
                }
                if (direct_opaque)
                    destination[x - left] = (fb_data)pixel;
                else
                    vm_blend_cached_pixel(&destination[x - left], pixel, 255);
                u += u_step;
                u_remainder += u_step_remainder;
                if (u_remainder >= x_span)
                {
                    ++u;
                    u_remainder -= x_span;
                }
                else if (u_remainder <= -x_span)
                {
                    --u;
                    u_remainder += x_span;
                }
                if (!white_vertex)
                    for (component = 0; component < 4; ++component)
                        color[component] += color_column_step[component];
            }
axis_row_complete:
            v_numerator += v_step_numerator;
            if (!white_vertex)
            {
                for (component = 0; component < 4; ++component)
                {
                    color_row[component] += color_row_step[component];
                    color_column_step[component] +=
                        color_column_step_row_step[component];
                }
            }
        }
    }
    if (textured)
    {
        ++state->fast_texture_quads;
        state->fast_texture_pixels +=
            (unsigned long)(right - left) * (top - bottom);
        ++texture->draws;
    }
    if (direct_opaque)
        state->fast_opaque_pixels +=
            (unsigned long)(right - left) * (top - bottom);
    else
        state->fast_blended_pixels +=
            (unsigned long)(right - left) * (top - bottom);
    return true;
}

static void vm_draw_raster_quad(struct ig_vm_state *state,
                                const struct ig_vm_raster_vertex vertices[4],
                                bool triangle_strip, bool textured)
{
    struct ig_vm_texture *texture = textured ? vm_draw_texture(state) : NULL;
    bool transparent_vertex = vertices[0].alpha == 0 &&
        vertices[1].alpha == 0 && vertices[2].alpha == 0 &&
        vertices[3].alpha == 0;
    bool transparent_uniform = state->gles_mode < IG_GLES_MODES &&
        state->gles_uniform_color[state->gles_mode][3] == 0;

    /* Fully transparent textures, uniforms and vertex quads are no-ops
     * regardless of transform. Vortex submits its large rotated mode-6
     * effect surface with a zero-alpha uniform for long stretches; rejecting
     * it here avoids rasterizing millions of pixels which cannot contribute. */
    if ((!vm_mode_uses_both_textures(state) &&
         (transparent_uniform || transparent_vertex)) ||
        (textured && !vm_mode_uses_both_textures(state) && texture &&
         texture->all_transparent))
    {
        if (texture)
            ++texture->draws;
        return;
    }
    if (vm_raster_axis_quad(state, vertices, textured))
        return;
    vm_raster_triangle(state, &vertices[0], &vertices[1], &vertices[2],
                       textured);
    if (triangle_strip)
        vm_raster_triangle(state, &vertices[2], &vertices[1], &vertices[3],
                           textured);
    else
        vm_raster_triangle(state, &vertices[0], &vertices[2], &vertices[3],
                           textured);
    if (texture)
        ++texture->draws;
}

#ifdef SIMULATOR
static void vm_record_texture_pair(struct ig_vm_state *state)
{
    u32 texture0 = state->bound_textures[0] ?
        state->bound_textures[0]->id : 0;
    u32 texture1 = state->bound_textures[1] ?
        state->bound_textures[1]->id : 0;
    unsigned int pair;

    for (pair = 0; pair < state->gles_texture_pair_count; ++pair)
    {
        if (state->gles_texture_pairs[pair][0] == state->gles_mode &&
            state->gles_texture_pairs[pair][1] == texture0 &&
            state->gles_texture_pairs[pair][2] == texture1)
        {
            ++state->gles_texture_pairs[pair][3];
            return;
        }
    }
    if (pair >= IG_VM_TEXTURE_PAIR_HISTORY)
        return;
    state->gles_texture_pairs[pair][0] = state->gles_mode;
    state->gles_texture_pairs[pair][1] = texture0;
    state->gles_texture_pairs[pair][2] = texture1;
    state->gles_texture_pairs[pair][3] = 1;
    ++state->gles_texture_pair_count;
}
#endif

static void vm_draw_quad(struct ig_vm_state *state, unsigned int first,
                         bool triangle_strip)
{
    struct ig_vm_attribute *position = &state->attributes[0];
    struct ig_vm_attribute *secondary = &state->attributes[1];
    struct ig_vm_raster_vertex vertices[4];
    u32 position_pointer = position->pointer;
    u32 secondary_pointer = secondary->pointer;
    u32 color_pointer = state->attributes[2].pointer;
    int left;
    int top;
    int right;
    int bottom;

    if (!position->pointer || position->type != 0x140c ||
        position->size < 2 || !secondary->pointer)
        return;
#ifdef SIMULATOR
    vm_record_texture_pair(state);
#endif
    position->pointer += first * position->stride;
    secondary->pointer += first * secondary->stride;
    if (secondary->size == 2 && state->attributes[2].pointer)
        state->attributes[2].pointer += first * state->attributes[2].stride;
    if (!vm_load_raster_vertex(state, position, secondary, 0,
                               &vertices[0]))
        goto restore_pointers;
    left = right = vertices[0].x >> 16;
    top = bottom = vertices[0].y >> 16;
    {
        unsigned int vertex;

        for (vertex = 1; vertex < 4; ++vertex)
        {
            int x;
            int y;

            if (!vm_load_raster_vertex(state, position, secondary, vertex,
                                       &vertices[vertex]))
                goto restore_pointers;
            x = vertices[vertex].x >> 16;
            y = vertices[vertex].y >> 16;
            left = MIN(left, x);
            right = MAX(right, x);
            top = MIN(top, y);
            bottom = MAX(bottom, y);
        }
    }
#ifdef SIMULATOR
    {
        struct ig_vm_attribute *texcoord = &state->attributes[1];
        unsigned int draw;

        if (state->is_vortex && state->synthetic_input_frame == 1800)
        {
            int flags = O_WRONLY | O_CREAT;
            int fd;

            if (!state->draw_trace_started)
                flags |= O_TRUNC;
            fd = rb->open(IG_ROOT_DIR "/ipodgames-vortex-draw-all-1800.log",
                          flags, 0666);
            if (fd >= 0)
            {
                if (state->draw_trace_started)
                    rb->lseek(fd, 0, SEEK_END);
                rb->fdprintf(fd,
                    "mode:%u/current:%u/units:%u,%u/"
                    "rect:%d,%d,%d,%d/source:%d,%d,%d,%d/"
                    "colors:%u,%u,%u,%u;%u,%u,%u,%u;"
                    "%u,%u,%u,%u;%u,%u,%u,%u\n",
                    state->gles_mode,
                    vm_draw_texture(state) ? vm_draw_texture(state)->id : 0,
                    state->bound_textures[0] ?
                        state->bound_textures[0]->id : 0,
                    state->bound_textures[1] ?
                        state->bound_textures[1]->id : 0,
                    left, top, right, bottom,
                    vm_fixed_integer(state, texcoord->pointer),
                    vm_fixed_integer(state, texcoord->pointer + 4),
                    vm_fixed_integer(state,
                                     texcoord->pointer + texcoord->stride),
                    vm_fixed_integer(state,
                        texcoord->pointer + texcoord->stride * 2 + 4),
                    vertices[0].red, vertices[0].green, vertices[0].blue,
                    vertices[0].alpha,
                    vertices[1].red, vertices[1].green, vertices[1].blue,
                    vertices[1].alpha,
                    vertices[2].red, vertices[2].green, vertices[2].blue,
                    vertices[2].alpha,
                    vertices[3].red, vertices[3].green, vertices[3].blue,
                    vertices[3].alpha);
                rb->close(fd);
                state->draw_trace_started = true;
            }
        }

        if (state->draw_history_count == IG_VM_DRAW_HISTORY)
        {
            rb->memmove(state->draw_rects[0], state->draw_rects[1],
                        sizeof(state->draw_rects[0]) *
                        (IG_VM_DRAW_HISTORY - 1));
            rb->memmove(state->draw_sources[0], state->draw_sources[1],
                        sizeof(state->draw_sources[0]) *
                        (IG_VM_DRAW_HISTORY - 1));
            rb->memmove(state->draw_texture_ids,
                        state->draw_texture_ids + 1,
                        sizeof(state->draw_texture_ids[0]) *
                        (IG_VM_DRAW_HISTORY - 1));
            rb->memmove(state->draw_modes, state->draw_modes + 1,
                        sizeof(state->draw_modes[0]) *
                        (IG_VM_DRAW_HISTORY - 1));
            rb->memmove(state->draw_texture0_ids,
                        state->draw_texture0_ids + 1,
                        sizeof(state->draw_texture0_ids[0]) *
                        (IG_VM_DRAW_HISTORY - 1));
            rb->memmove(state->draw_texture1_ids,
                        state->draw_texture1_ids + 1,
                        sizeof(state->draw_texture1_ids[0]) *
                        (IG_VM_DRAW_HISTORY - 1));
            --state->draw_history_count;
        }
        draw = state->draw_history_count++;

        state->draw_rects[draw][0] = left;
        state->draw_rects[draw][1] = top;
        state->draw_rects[draw][2] = right;
        state->draw_rects[draw][3] = bottom;
        state->draw_sources[draw][0] =
            vm_fixed_integer(state, texcoord->pointer);
        state->draw_sources[draw][1] =
            vm_fixed_integer(state, texcoord->pointer + 4);
        state->draw_sources[draw][2] =
            vm_fixed_integer(state,
                             texcoord->pointer + texcoord->stride);
        state->draw_sources[draw][3] =
            vm_fixed_integer(state,
                             texcoord->pointer + texcoord->stride * 2 + 4);
        state->draw_texture_ids[draw] = vm_draw_texture(state) ?
            vm_draw_texture(state)->id : 0;
        state->draw_modes[draw] = state->gles_mode;
        state->draw_texture0_ids[draw] = state->bound_textures[0] ?
            state->bound_textures[0]->id : 0;
        state->draw_texture1_ids[draw] = state->bound_textures[1] ?
            state->bound_textures[1]->id : 0;
    }
#endif
#ifdef SIMULATOR
    if (state->is_vortex && state->gles_mode == 32 &&
        vm_draw_texture(state) && vm_draw_texture(state)->id == 56)
        vm_dump_frame_to(state,
                         IG_ROOT_DIR "/ipodgames-vortex-composite-pre.ppm");
#endif
    if (state->attributes[1].size == 2)
        vm_draw_raster_quad(state, vertices, triangle_strip, true);
    else if (state->attributes[1].size == 4)
        vm_draw_raster_quad(state, vertices, triangle_strip, false);
#ifdef SIMULATOR
    if (state->is_vortex && state->gles_mode == 32 &&
        vm_draw_texture(state) && vm_draw_texture(state)->id == 56)
        vm_dump_frame_to(state,
                         IG_ROOT_DIR "/ipodgames-vortex-composite-post.ppm");
#endif
    ++state->draw_calls;
    if (state->gles_mode < IG_GLES_MODES)
        ++state->gles_mode_draws[state->gles_mode];
restore_pointers:
    position->pointer = position_pointer;
    secondary->pointer = secondary_pointer;
    state->attributes[2].pointer = color_pointer;
}

static void vm_draw_triangle(struct ig_vm_state *state, unsigned int first,
                             unsigned int a, unsigned int b, unsigned int c)
{
    struct ig_vm_attribute *position = &state->attributes[0];
    struct ig_vm_attribute *secondary = &state->attributes[1];
    struct ig_vm_raster_vertex vertices[3];
    bool textured;

    if (!position->pointer || position->type != 0x140c ||
        position->size < 2 || !secondary->pointer)
        return;
    textured = secondary->size == 2;
    if (!textured && secondary->size != 4)
        return;
    if (!vm_load_raster_vertex(state, position, secondary, first + a,
                               &vertices[0]) ||
        !vm_load_raster_vertex(state, position, secondary, first + b,
                               &vertices[1]) ||
        !vm_load_raster_vertex(state, position, secondary, first + c,
                               &vertices[2]))
        return;
    vm_raster_triangle(state, &vertices[0], &vertices[1], &vertices[2],
                       textured);
    if (textured && vm_draw_texture(state))
        ++vm_draw_texture(state)->draws;
    ++state->draw_calls;
    if (state->gles_mode < IG_GLES_MODES)
        ++state->gles_mode_draws[state->gles_mode];
}

#ifdef SIMULATOR
static void vm_dump_frame_to(const struct ig_vm_state *state,
                             const char *path)
{
    unsigned char row[LCD_WIDTH * 3];
    int fd;
    int y;

    if (!state->framebuffer || !state->presented_frames)
        return;
    fd = rb->open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    rb->fdprintf(fd, "P6\n%d %d\n255\n", LCD_WIDTH, LCD_HEIGHT);
    for (y = 0; y < LCD_HEIGHT; ++y)
    {
        int x;

        for (x = 0; x < LCD_WIDTH; ++x)
        {
            fb_data pixel = state->framebuffer[y * LCD_WIDTH + x];
            row[x * 3] = RGB_UNPACK_RED(pixel);
            row[x * 3 + 1] = RGB_UNPACK_GREEN(pixel);
            row[x * 3 + 2] = RGB_UNPACK_BLUE(pixel);
        }
        if (rb->write(fd, row, sizeof(row)) != (ssize_t)sizeof(row))
            break;
    }
    rb->close(fd);
}

static void vm_dump_frame(const struct ig_vm_state *state)
{
    vm_dump_frame_to(state, IG_VM_FRAME_DUMP);
}

static void vm_dump_vortex_state(const struct ig_vm_state *state)
{
    static const u32 address = 0x18063100u;
    static const size_t bytes = 0x1000;
    static const u32 manager_address = 0x180bf500u;
    static const u32 request_address = 0x1818d000u;
    static const size_t request_bytes = 0x10000;
    int fd;

    if (!state->is_vortex || address - IG_VM_BASE > state->memory_size ||
        bytes > state->memory_size - (address - IG_VM_BASE))
        return;
    fd = rb->open(IG_ROOT_DIR "/ipodgames-vortex-state.bin",
                  O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    rb->write(fd, state->memory + address - IG_VM_BASE, bytes);
    rb->close(fd);
    fd = rb->open(IG_ROOT_DIR "/ipodgames-vortex-manager.bin",
                  O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    rb->write(fd, state->memory + manager_address - IG_VM_BASE, bytes);
    rb->close(fd);
    fd = rb->open(IG_ROOT_DIR "/ipodgames-vortex-request.bin",
                  O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    rb->write(fd, state->memory + request_address - IG_VM_BASE,
              request_bytes);
    rb->close(fd);
}

static void vm_dump_vortex_texture(const struct ig_vm_state *state,
                                   unsigned int id)
{
    const struct ig_vm_texture *texture = NULL;
    unsigned int index;
    unsigned char row[512 * 3];
    char path[MAX_PATH];
    int fd;

    for (index = 0; index < state->texture_count; ++index)
        if (state->textures[index].id == id)
            texture = &state->textures[index];
    if (!texture || !texture->pixels || !texture->width ||
        !texture->height || texture->width > 512)
        return;
    rb->snprintf(path, sizeof(path),
                 IG_ROOT_DIR "/ipodgames-vortex-texture-%u.ppm", id);
    fd = rb->open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    rb->fdprintf(fd, "P6\n%u %u\n255\n", texture->width,
                 texture->height);
    for (unsigned int y = 0; y < texture->height; ++y)
    {
        for (unsigned int x = 0; x < texture->width; ++x)
        {
            fb_data pixel = texture->pixels[y * texture->width + x];

            row[x * 3] = RGB_UNPACK_RED(pixel);
            row[x * 3 + 1] = RGB_UNPACK_GREEN(pixel);
            row[x * 3 + 2] = RGB_UNPACK_BLUE(pixel);
        }
        rb->write(fd, row, texture->width * 3);
    }
    rb->close(fd);
}

static void vm_dump_menu_sequence_frame(const struct ig_vm_state *state)
{
    static const unsigned short capture_frames[] = {
        1, 30, 90, 150, 160, 161, 162, 169, 170, 175, 181,
        194, 195, 196, 220, 269, 270, 271, 280, 290, 300, 310,
        320, 330, 340, 349, 350, 351, 360, 370, 380, 390, 400,
        410, 420, 430, 440, 450, 460, 470, 480, 482, 500,
        549, 550, 551, 599, 600, 601, 602, 603, 620, 622,
        649, 650, 651, 700, 720, 749, 750, 779, 780, 781, 782, 783,
        800, 850,
        899, 900, 901, 950, 999, 1000, 1001, 1050,
        919, 920, 921, 929, 930, 931, 939, 940, 941,
        949, 951, 959, 960, 961, 962, 963, 975, 1025, 1079, 1080,
        1082, 1100, 1200, 1300, 1379, 1380, 1382, 1450, 1550,
        1679, 1680, 1682, 1750, 1800, 1899, 1900, 1902, 1950,
        2000, 2050, 2100, 2150, 2199, 2200, 2202, 2250, 2300, 2400,
    };
    char path[MAX_PATH];
    unsigned int index;

    if (!getenv("IPODGAMES_TEST_CAPTURE_MENUS"))
        return;
    if (state->is_vortex && state->synthetic_input_frame == 1800)
    {
        int fd = rb->open(IG_ROOT_DIR "/ipodgames-vortex-draw-1800.log",
                          O_WRONLY | O_CREAT | O_TRUNC, 0666);

        if (fd >= 0)
        {
            for (index = 0; index < state->draw_history_count; ++index)
                rb->fdprintf(fd,
                    "%u=mode:%u/current:%u/units:%u,%u/"
                    "rect:%d,%d,%d,%d/source:%d,%d,%d,%d\n",
                    index, state->draw_modes[index],
                    state->draw_texture_ids[index],
                    state->draw_texture0_ids[index],
                    state->draw_texture1_ids[index],
                    state->draw_rects[index][0],
                    state->draw_rects[index][1],
                    state->draw_rects[index][2],
                    state->draw_rects[index][3],
                    state->draw_sources[index][0],
                    state->draw_sources[index][1],
                    state->draw_sources[index][2],
                    state->draw_sources[index][3]);
            rb->close(fd);
        }
    }
    for (index = 0; index < ARRAYLEN(capture_frames); ++index)
    {
        if (state->synthetic_input_frame != capture_frames[index])
            continue;
        rb->snprintf(path, sizeof(path),
                     IG_ROOT_DIR "/ipodgames-menu-%03u.ppm",
                     state->synthetic_input_frame);
        vm_dump_frame_to(state, path);
        break;
    }
}

static void vm_dump_motion_continuity_frame(
    const struct ig_vm_state *state)
{
    char path[MAX_PATH];

    if (!getenv("IPODGAMES_TEST_MOTION_CONTINUITY") ||
        state->synthetic_input_frame < 950 ||
        state->synthetic_input_frame > 1100)
        return;
    rb->snprintf(path, sizeof(path),
                 IG_ROOT_DIR "/ipodgames-continuity-%03u.ppm",
                 state->synthetic_input_frame);
    vm_dump_frame_to(state, path);
}
#endif

static unsigned int vm_retail_wheel_position(unsigned int wheel)
{
    /* RetailOS does not linearly expand the click-wheel driver's 0..95
     * coordinate.  InputEvents passes it through the platform normalizer:
     *
     *     ((119 - raw) * 8 / 3) & 0xff
     *
     * The offset and reversed polarity are both observable in the 5G OSOS
     * implementation at 0x100e95a4.  Matching it is important: the eApp
     * uses the normalized byte directly for its four-way joystick sectors.
     */
    return (((119u - (wheel % 96u)) * 8u) / 3u) & 0xffu;
}

static unsigned char vm_framework_id(const unsigned char *name)
{
    if (!rb->strncmp((const char *)name, "miscTBD", 32))
        return IG_VM_FW_MISC;
    if (!rb->strncmp((const char *)name, "OpenGLES", 32))
        return IG_VM_FW_OPENGLES;
    if (!rb->strncmp((const char *)name, "Metadata", 32))
        return IG_VM_FW_METADATA;
    if (!rb->strncmp((const char *)name, "AsyncFileIO", 32))
        return IG_VM_FW_ASYNC_FILE_IO;
    if (!rb->strncmp((const char *)name, "Audio", 32))
        return IG_VM_FW_AUDIO;
    if (!rb->strncmp((const char *)name, "InputEvents", 32))
        return IG_VM_FW_INPUT_EVENTS;
    if (!rb->strncmp((const char *)name, "Settings", 32))
        return IG_VM_FW_SETTINGS;
    return IG_VM_FW_UNKNOWN;
}

static bool vm_patch_imports(struct ig_vm_state *state,
                             const struct ig_eapp_probe *probe)
{
    u32 framework_pointer = probe->first_framework_pointer;

    state->import_count = 0;
    while (framework_pointer)
    {
        u32 offset;
        u32 count;
        u32 next_pointer;
        u32 stubs_offset;
        u32 slots_offset;
        u32 ordinal;
        unsigned char framework;

        if (framework_pointer < IG_VM_BASE)
            return false;
        offset = framework_pointer - IG_VM_BASE;
        if (offset > state->memory_size ||
            state->memory_size - offset < 56)
            return false;
        count = vm_u32(state->memory + offset + 48);
        next_pointer = vm_u32(state->memory + offset + 52);
        if (count == 0 && next_pointer == 0)
            return true;
        if (count > IG_VM_MAX_IMPORTS - state->import_count)
            return false;
        stubs_offset = offset + 56;
        if (count > (state->memory_size - stubs_offset) / 8)
            return false;
        slots_offset = stubs_offset + count * 4;
        framework = vm_framework_id(state->memory + offset);
        for (ordinal = 0; ordinal < count; ++ordinal)
        {
            u32 index = state->import_count++;
            state->import_framework[index] = framework;
            state->import_ordinal[index] = ordinal;
            vm_put_u32(state->memory + slots_offset + ordinal * 4,
                       IG_VM_TRAP_BASE + index * 4);
        }
        framework_pointer = next_pointer;
    }
    return false;
}

static bool vm_swi(machine_t *machine, cpu_t *cpu, u32 immediate)
{
    struct ig_vm_state *state = active_vm;
    unsigned char *argument;

    (void)machine;
    if (!state || immediate != 0x123456)
        return false;
    if (cpu->r[0] == 3)
    {
        argument = resolve_addr(&state->machine, cpu->r[1]);
        if (!argument)
            return false;
        cpu->r[0] = 0;
        return true;
    }
    if (cpu->r[0] == 4)
    {
        argument = resolve_addr(&state->machine, cpu->r[1]);
        if (!argument)
            return false;
        cpu->r[0] = 0;
        return true;
    }
    return false;
}

static struct ig_vm_texture *vm_bind_texture(struct ig_vm_state *state,
                                             u32 id)
{
    unsigned int index;

    for (index = 0; index < state->texture_count; ++index)
        if (state->textures[index].id == id)
        {
            state->bound_texture = &state->textures[index];
            state->bound_textures[state->active_texture_unit] =
                state->bound_texture;
            return state->bound_texture;
        }
    if (state->texture_count >= ARRAYLEN(state->textures))
    {
        state->bound_textures[state->active_texture_unit] = NULL;
        return state->bound_texture = NULL;
    }
    state->bound_texture = &state->textures[state->texture_count++];
    rb->memset(state->bound_texture, 0, sizeof(*state->bound_texture));
    state->bound_texture->id = id;
    state->bound_textures[state->active_texture_unit] =
        state->bound_texture;
    return state->bound_texture;
}

static struct ig_vm_texture *vm_draw_texture(struct ig_vm_state *state)
{
    /* Vortex's fixed shader wrappers leave the sampler-bearing unit active
     * at draw time. Multi-texture combination is handled per mode later; the
     * visible source is therefore the current unit, not always unit zero. */
    return state->bound_texture;
}

static void vm_copy_framebuffer_texture(struct ig_vm_state *state)
{
    struct ig_vm_texture *texture = state->bound_texture;
    u32 stack = state->cpu.r[reg(&state->cpu, SP)];
    u32 y_value;
    u32 width_value;
    u32 height_value;
    int source_x = (int)state->cpu.r[3];
    int source_y;
    unsigned int width;
    unsigned int height;
    size_t texture_bytes;
    unsigned int y;

    if (!texture || !vm_guest_u32(state, stack, &y_value) ||
        !vm_guest_u32(state, stack + 4, &width_value) ||
        !vm_guest_u32(state, stack + 8, &height_value) ||
        !width_value || !height_value || width_value > LCD_WIDTH ||
        height_value > LCD_HEIGHT)
        return;
    source_y = (int)y_value;
    width = width_value;
    height = height_value;
    texture_bytes = (size_t)width * height * sizeof(*texture->pixels);
    if (texture->capacity < texture_bytes)
    {
        if (texture_bytes > state->texture_storage_size -
                            state->texture_storage_used)
            return;
        texture->pixels = (u32 *)(state->texture_storage +
                                  state->texture_storage_used);
        texture->capacity = texture_bytes;
        state->texture_storage_used += texture_bytes;
    }
    texture->width = width;
    texture->height = height;
    texture->format = state->cpu.r[2];
    texture->type = 0x1401;
    for (y = 0; y < height; ++y)
    {
        unsigned int x;
        int screen_y = LCD_HEIGHT - 1 - (source_y + (int)y);

        for (x = 0; x < width; ++x)
        {
            int screen_x = source_x + (int)x;
            u32 pixel = 0;

            if (screen_x >= 0 && screen_x < LCD_WIDTH &&
                screen_y >= 0 && screen_y < LCD_HEIGHT)
                pixel = (u32)state->framebuffer[
                    screen_y * LCD_WIDTH + screen_x];
            texture->pixels[y * width + x] = 0xff0000u | pixel;
        }
    }
    texture->all_opaque = true;
    texture->all_transparent = false;
    texture->alpha_left = 0;
    texture->alpha_right = width;
    texture->alpha_top = 0;
    texture->alpha_bottom = height;
    ++texture->revision;
    if (!texture->revision)
        ++texture->revision;
}

static void vm_update_texture(struct ig_vm_state *state)
{
    struct ig_vm_texture *texture = state->bound_texture;
    u32 stack = state->cpu.r[reg(&state->cpu, SP)];
    u32 values[5];
    u32 x = state->cpu.r[2];
    u32 y = state->cpu.r[3];
    u32 width;
    u32 height;
    u32 format;
    u32 type;
    u32 pointer;
    u32 source_offset;
    size_t source_stride;
    size_t row_bytes;
    size_t bytes_per_pixel = 0;
    size_t texture_bytes;
    unsigned int row;

    for (unsigned int index = 0; index < ARRAYLEN(values); ++index)
    {
        if (!vm_guest_u32(state, stack + index * 4, &values[index]))
            goto failed;
    }
    width = values[0];
    height = values[1];
    format = values[2];
    type = values[3];
    pointer = values[4];
    state->texture_subimage_last[0] = x;
    state->texture_subimage_last[1] = y;
    state->texture_subimage_last[2] = width;
    state->texture_subimage_last[3] = height;
    state->texture_subimage_last[4] = format;
    state->texture_subimage_last[5] = type;
    state->texture_subimage_last[6] = pointer;
    if ((format == 0x1908 && type == 0x8034) ||
        (format == 0x1907 && type == 0x8363) ||
        (format == 0x1908 && type == 0x8033))
        bytes_per_pixel = 2;
    else if (format == 0x1908 && type == 0x1401)
        bytes_per_pixel = 4;
    else if (format == 0x1907 && type == 0x1401)
        bytes_per_pixel = 3;
    else if (format == 0x1906 && type == 0x1401)
        bytes_per_pixel = 1;
    if (!texture || state->cpu.r[1] != 0 || !bytes_per_pixel ||
        pointer < IG_VM_BASE || !width || !height)
        goto failed;
    /* Vortex creates its streaming background texture with a zero-sized
     * TexImage and supplies the real 320x240 dimensions in the first
     * TexSubImage call.  The stock iPod GL wrapper treats that as allocation
     * plus upload, so mirror that extension here. */
    if (!texture->width && !texture->height && !x && !y)
    {
        if ((size_t)width > SIZE_MAX / height ||
            (size_t)width * height > SIZE_MAX / sizeof(*texture->pixels))
            goto failed;
        texture_bytes = (size_t)width * height * sizeof(*texture->pixels);
        if (texture->capacity < texture_bytes)
        {
            if (state->texture_storage_used > state->texture_storage_size ||
                texture_bytes > state->texture_storage_size -
                                state->texture_storage_used)
                goto failed;
            texture->pixels = (u32 *)(state->texture_storage +
                                      state->texture_storage_used);
            texture->capacity = texture_bytes;
            state->texture_storage_used += texture_bytes;
        }
        texture->width = width;
        texture->height = height;
        texture->format = format;
        texture->type = type;
    }
    if (!texture->pixels || x > texture->width || y > texture->height ||
        width > texture->width - x || height > texture->height - y)
        goto failed;
    row_bytes = (size_t)width * bytes_per_pixel;
    source_stride = (row_bytes + 3) & ~(size_t)3;
    source_offset = pointer - IG_VM_BASE;
    if (source_offset > state->memory_size ||
        source_stride > state->memory_size - source_offset ||
        height - 1 >
            (state->memory_size - source_offset - row_bytes) /
            source_stride)
        goto failed;
    for (row = 0; row < height; ++row)
    {
        vm_cache_texture_pixels(
            texture->pixels + (size_t)(y + row) * texture->width + x,
            state->memory + source_offset + row * source_stride,
            width, format, type);
    }
    vm_analyze_texture(texture);
    ++state->texture_subimages;
    state->texture_subimage_bytes += row_bytes * height;
    return;

failed:
    ++state->texture_subimage_failures;
}

static void vm_async_read(struct ig_vm_state *state, u32 location,
                          u32 filename_address, u32 descriptor)
{
    char filename[128];
    char path[MAX_PATH];
    u32 callback = 0;
    u32 context = 0;
    u32 destination = 0;
    u32 capacity = 0;
    u32 destination_offset;
    u32 bytes = 0;
    u32 completion_bytes = 0;
    u32 status = 1;
    int fd = -1;
    off_t size;
    size_t read_size;

    if (!vm_guest_u32(state, descriptor + 8, &context) ||
        !vm_guest_u32(state, descriptor + 20, &destination) ||
        !vm_guest_u32(state, descriptor + 24, &capacity) ||
        !vm_guest_u32(state, descriptor + 52, &callback) ||
        !vm_guest_string(state, filename_address, filename, sizeof(filename)))
        goto complete;
    rb->strlcpy(state->last_async_file, filename,
                sizeof(state->last_async_file));
    state->last_async_destination = destination;
    state->last_async_capacity = capacity;
    state->last_async_context = context;
    state->last_async_callback = callback;
    vm_guest_u32(state, context + 12, &state->last_async_inner_callback);
    vm_guest_u32(state, context + 16, &state->last_async_inner_context);
    vm_guest_u32(state, state->last_async_inner_context + 356,
                 &state->last_async_manager_callback);
    vm_guest_u32(state, state->last_async_inner_context + 360,
                 &state->last_async_manager_context);
    if (rb->strstr(filename, ".wav"))
        state->pending_audio_context = state->last_async_manager_context;
    ++state->async_reads;
    if (!filename[0] || filename[0] == PATH_SEPCH ||
        rb->strstr(filename, "..") || rb->strchr(filename, '\\'))
        goto complete;
    if (location)
        rb->snprintf(path, sizeof(path), "%s/%s",
                     state->save_directory, filename);
    else
        rb->snprintf(path, sizeof(path), "%s/assets/%s",
                     state->game_directory, filename);
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        goto complete;
    size = rb->filesize(fd);
    if (size < 0 || !capacity ||
        destination < IG_VM_BASE)
        goto complete;
    completion_bytes = (u32)size;
    read_size = MIN((size_t)size, (size_t)capacity);
    destination_offset = destination - IG_VM_BASE;
    if (destination_offset > state->memory_size ||
        state->memory_size - destination_offset < read_size)
        goto complete;
    if (rb->read(fd, state->memory + destination_offset, read_size) !=
        (ssize_t)read_size)
        goto complete;
    bytes = read_size;
    status = 0;

complete:
    if (fd >= 0)
        rb->close(fd);
    vm_guest_put_u32(state, descriptor + 32, status);
    vm_guest_put_u32(state, descriptor + 36, completion_bytes);
    state->pending_callback = callback;
    state->pending_context =
        callback == IG_VORTEX_ASYNC_REQUEST_CALLBACK ? descriptor : context;
    state->pending_bytes = completion_bytes;
    state->pending_status = status;
    state->pending_object_callback = false;
    state->last_async_bytes = bytes;
    if (!status && location)
    {
        ++state->save_reads;
        state->save_read_bytes += bytes;
        rb->strlcpy(state->last_save_file, filename,
                    sizeof(state->last_save_file));
    }
}

static void vm_async_open(struct ig_vm_state *state, u32 filename_address,
                          u32 request)
{
    char filename[128];
    char path[MAX_PATH];
    u32 context = 0;
    u32 callback = 0;
    u32 status = 1;
    int fd = -1;

    if (!vm_guest_u32(state, request + 8, &context) ||
        !vm_guest_u32(state, request + 52, &callback) ||
        !vm_guest_string(state, filename_address, filename,
                         sizeof(filename)))
        goto complete;
    ++state->async_opens;
    rb->strlcpy(state->last_async_file, filename,
                sizeof(state->last_async_file));
    state->async_stream_offset = 0;
    if (!filename[0] || filename[0] == PATH_SEPCH ||
        rb->strstr(filename, "..") || rb->strchr(filename, '\\'))
        goto complete;
    rb->snprintf(path, sizeof(path), "%s/assets/%s",
                 state->game_directory, filename);
    fd = rb->open(path, O_RDONLY);
    if (fd >= 0)
        status = 0;

complete:
    if (fd >= 0)
        rb->close(fd);
    /* The stock callback stores its retailOS file handle in *context before
     * entering 0x180168c0.  Rockbox streams the asset itself later, so a
     * nonzero compatibility handle is sufficient for this completion step.
     */
    if (context)
        vm_guest_put_u32(state, context, status ? 0 : 1);
    vm_guest_put_u32(state, request + 32, status);
    vm_guest_put_u32(state, request + 36, 0);
    vm_guest_put_u32(state, request + 44, status ? (u32)-1 : 1);
    state->pending_callback = callback;
    state->pending_context =
        callback == IG_MSPAC_ASYNC_OPEN_CALLBACK ? context : request;
    state->pending_bytes = 0;
    state->pending_status = status;
    state->pending_object_callback = false;
}

static bool vm_async_read_opened(struct ig_vm_state *state, u32 request)
{
    char path[MAX_PATH];
    u32 destination = 0;
    u32 capacity = 0;
    u32 callback = 0;
    u32 destination_offset;
    u32 bytes = 0;
    u32 completion_bytes = 0;
    u32 status = 1;
    int fd = -1;
    off_t size;
    size_t read_size;

    if (!vm_guest_u32(state, request + 72, &destination) ||
        !vm_guest_u32(state, request + 76, &capacity) ||
        !vm_guest_u32(state, request + 52, &callback) ||
        !state->last_async_file[0])
        goto complete;
    ++state->async_reads;
    state->last_async_destination = destination;
    state->last_async_capacity = capacity;
    state->last_async_context = request;
    state->last_async_callback = callback;
    rb->snprintf(path, sizeof(path), "%s/assets/%s",
                 state->game_directory, state->last_async_file);
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        goto complete;
    size = rb->filesize(fd);
    if (size < 0 || !capacity || destination < IG_VM_BASE)
        goto complete;
    if (state->async_stream_offset > (u32)size ||
        rb->lseek(fd, state->async_stream_offset, SEEK_SET) !=
            (off_t)state->async_stream_offset)
        goto complete;
    read_size = MIN((size_t)size - state->async_stream_offset,
                    (size_t)capacity);
    completion_bytes = read_size;
    destination_offset = destination - IG_VM_BASE;
    if (destination_offset > state->memory_size ||
        state->memory_size - destination_offset < read_size ||
        rb->read(fd, state->memory + destination_offset, read_size) !=
            (ssize_t)read_size)
        goto complete;
    bytes = read_size;
    state->async_stream_offset += read_size;
    status = 0;

complete:
    if (fd >= 0)
        rb->close(fd);
    vm_guest_put_u32(state, request + 32, status);
    vm_guest_put_u32(state, request + 36, completion_bytes);
    state->pending_callback = callback;
    state->pending_context = request;
    state->pending_bytes = completion_bytes;
    state->pending_status = status;
    state->pending_object_callback = true;
    state->last_async_bytes = bytes;
    return status == 0;
}

static bool vm_async_close_request(struct ig_vm_state *state, u32 request)
{
    u32 callback = 0;

    if (!vm_guest_u32(state, request + 52, &callback))
        return false;
    /* The object wrapper at 0x18020474 only accepts state 2 and moves the
     * stream to state 3, so ordinal 1 is close, not another data read.  The
     * loader deliberately closes after its last chunk; rereading here would
     * overwrite that chunk before the completion callback parses it. */
    vm_guest_put_u32(state, request + 32, 0);
    vm_guest_put_u32(state, request + 36, 0);
    state->pending_callback = callback;
    state->pending_context = request;
    state->pending_bytes = 0;
    state->pending_status = 0;
    state->pending_object_callback = false;
    return true;
}

static bool vm_save_path(struct ig_vm_state *state, u32 filename_address,
                         char *path, size_t path_size)
{
    char filename[128];
    char parent[MAX_PATH];
    char *separator;

    if (!vm_guest_string(state, filename_address, filename,
                         sizeof(filename)) ||
        !filename[0] || filename[0] == PATH_SEPCH ||
        rb->strstr(filename, "..") || rb->strchr(filename, '\\') ||
        rb->snprintf(path, path_size, "%s/%s", state->save_directory,
                     filename) >= (int)path_size)
        return false;
    rb->mkdir(IG_ROOT_DIR);
    rb->mkdir(IG_ROOT_DIR "/saves");
    rb->mkdir(state->save_directory);
    rb->strlcpy(state->last_save_file, filename,
                sizeof(state->last_save_file));
    rb->strlcpy(parent, path, sizeof(parent));
    separator = rb->strrchr(parent, PATH_SEPCH);
    if (separator && separator > parent + rb->strlen(state->save_directory))
    {
        *separator = '\0';
        rb->mkdir(parent);
    }
    return true;
}

static u32 vm_save_open(struct ig_vm_state *state, u32 filename_address,
                        u32 handle_address)
{
    char path[MAX_PATH];
    unsigned int slot;
    int fd;

    if (!vm_save_path(state, filename_address, path, sizeof(path)))
        goto failed;
    for (slot = 0; slot < ARRAYLEN(state->save_fds); ++slot)
        if (state->save_fds[slot] < 0)
            break;
    if (slot >= ARRAYLEN(state->save_fds))
        goto failed;
    fd = rb->open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0 || !vm_guest_put_u32(state, handle_address, slot + 1))
    {
        if (fd >= 0)
            rb->close(fd);
        goto failed;
    }
    state->save_fds[slot] = fd;
    return 0;

failed:
    ++state->save_failures;
    return 1;
}

static u32 vm_save_write(struct ig_vm_state *state, u32 handle,
                         u32 source, u32 bytes)
{
    u32 offset;
    int fd;

    if (!handle || handle > ARRAYLEN(state->save_fds) ||
        source < IG_VM_BASE)
        goto failed;
    fd = state->save_fds[handle - 1];
    offset = source - IG_VM_BASE;
    if (fd < 0 || offset > state->memory_size ||
        bytes > state->memory_size - offset ||
        rb->write(fd, state->memory + offset, bytes) != (ssize_t)bytes)
        goto failed;
    ++state->save_writes;
    state->save_bytes += bytes;
    return 0;

failed:
    ++state->save_failures;
    return 1;
}

static u32 vm_save_close(struct ig_vm_state *state, u32 handle)
{
    int result;

    if (!handle || handle > ARRAYLEN(state->save_fds) ||
        state->save_fds[handle - 1] < 0)
        return 1;
    result = rb->close(state->save_fds[handle - 1]);
    state->save_fds[handle - 1] = -1;
    if (result < 0)
    {
        ++state->save_failures;
        return 1;
    }
    return 0;
}

static void vm_save_init(struct ig_vm_state *state)
{
    unsigned int slot;

    for (slot = 0; slot < ARRAYLEN(state->save_fds); ++slot)
        state->save_fds[slot] = -1;
}

static void vm_save_shutdown(struct ig_vm_state *state)
{
    unsigned int slot;

    for (slot = 0; slot < ARRAYLEN(state->save_fds); ++slot)
    {
        if (state->save_fds[slot] >= 0)
            rb->close(state->save_fds[slot]);
        state->save_fds[slot] = -1;
    }
}

static bool vm_save_exit_complete(struct ig_vm_state *state)
{
    u32 game_object;
    u32 input_component;

    if (state->is_vortex)
        return state->save_writes >= 3 && !state->save_failures &&
            state->memory[IG_VM_SCRATCH_BASE - IG_VM_BASE] == 6;
    if (state->save_writes < 2 || state->save_failures ||
        state->memory[IG_VM_SCRATCH_BASE - IG_VM_BASE] != 4 ||
        !vm_guest_u32(state, 0x18047280u, &game_object) ||
        !game_object ||
        !vm_guest_u32(state, game_object + 0x40, &input_component))
        return false;
    return input_component == 0;
}

static struct ig_vm_audio_object *vm_audio_object(
    struct ig_vm_state *state, u32 handle)
{
    if (!handle || handle >= ARRAYLEN(state->audio_objects) ||
        !state->audio_objects[handle].allocated)
        return NULL;
    return &state->audio_objects[handle];
}

static unsigned int vm_audio_voice_count(struct ig_vm_state *state)
{
    unsigned int handle;
    unsigned int count = 0;

    for (handle = 1; handle < ARRAYLEN(state->audio_objects); ++handle)
    {
        struct ig_vm_audio_object *object = &state->audio_objects[handle];

        if (object->allocated && object->playing)
            ++count;
    }
    return count;
}

static void vm_audio_update_gains(struct ig_vm_audio_object *object)
{
    unsigned int amplitude = MIN(object->amplitude, 32767u);
    unsigned int pitch = MIN(object->pitch, 4000u);
    int pan = object->pan;

    if (pan < -1000)
        pan = -1000;
    else if (pan > 1000)
        pan = 1000;
    object->gain_left = amplitude;
    object->gain_right = amplitude;
    if (pan > 0)
        object->gain_left = amplitude * (1000 - pan) / 1000;
    else if (pan < 0)
        object->gain_right = amplitude * (1000 + pan) / 1000;
    object->step = MAX(1u, pitch * 65536u / 1000u);
}

static bool vm_audio_rewind(struct ig_vm_audio_object *object,
                            unsigned long long source_length)
{
    if (object->repeats_remaining == 1)
    {
        object->playing = false;
        object->paused = false;
        return false;
    }
    if (object->repeats_remaining > 1)
        --object->repeats_remaining;
    object->position %= source_length;
    return true;
}

static void vm_audio_get_more(const void **start, size_t *size)
{
    struct ig_vm_state *state = active_vm;
    unsigned int handle;
    size_t frames = 0;

    if (!state || !state->audio_initialized)
        goto stopped;
    for (handle = 1; handle < ARRAYLEN(state->audio_objects); ++handle)
    {
        struct ig_vm_audio_object *object = &state->audio_objects[handle];
        unsigned long long remaining;
        size_t object_frames;

        if (!object->allocated || !object->playing || !object->step)
            continue;
        if (object->data < IG_VM_BASE || object->bits != 16 ||
            object->channels != 1 ||
            object->rate != state->audio_output_rate)
        {
            object->playing = false;
            continue;
        }
        if (object->data - IG_VM_BASE > state->memory_size ||
            object->bytes > state->memory_size -
                            (object->data - IG_VM_BASE))
        {
            object->playing = false;
            continue;
        }
        remaining = (unsigned long long)(object->bytes / 2) * 65536u;
        if (object->position >= remaining)
        {
            if (!remaining || !vm_audio_rewind(object, remaining))
                continue;
        }
        if (object->repeats_remaining != 1)
            object_frames = IG_VM_AUDIO_BLOCK_FRAMES;
        else
        {
            remaining -= object->position;
            object_frames = (size_t)((remaining + object->step - 1) /
                                     object->step);
        }
        frames = MAX(frames, MIN((size_t)IG_VM_AUDIO_BLOCK_FRAMES,
                                 object_frames));
    }
    if (!frames)
        goto stopped;
    rb->memset(audio_mix_buffer, 0,
               frames * 2 * sizeof(audio_mix_buffer[0]));
    for (handle = 1; handle < ARRAYLEN(state->audio_objects); ++handle)
    {
        struct ig_vm_audio_object *object = &state->audio_objects[handle];
        u32 source_offset;
        size_t source_frames;
        size_t frame;

        if (!object->allocated || !object->playing)
            continue;
        source_offset = object->data - IG_VM_BASE;
        source_frames = object->bytes / 2;
        for (frame = 0; frame < frames; ++frame)
        {
            const unsigned char *source;
            size_t source_frame = (size_t)(object->position >> 16);
            int16_t sample;
            int left;
            int right;

            if (source_frame >= source_frames)
            {
                unsigned long long source_length =
                    (unsigned long long)source_frames * 65536u;

                if (!source_length ||
                    !vm_audio_rewind(object, source_length))
                    break;
                source_frame = (size_t)(object->position >> 16);
            }
            source = state->memory + source_offset + source_frame * 2;
            sample = (int16_t)((u16)source[0] | ((u16)source[1] << 8));
            left = audio_mix_buffer[frame * 2] +
                   (sample * (int)object->gain_left) / 32767;
            right = audio_mix_buffer[frame * 2 + 1] +
                    (sample * (int)object->gain_right) / 32767;
            audio_mix_buffer[frame * 2] =
                (int16_t)MAX(-32768, MIN(32767, left));
            audio_mix_buffer[frame * 2 + 1] =
                (int16_t)MAX(-32768, MIN(32767, right));
            object->position += object->step;
        }
    }
    state->audio_pcm_frames += frames;
    state->audio_playing = vm_audio_voice_count(state) != 0;
    *start = audio_mix_buffer;
    *size = frames * 2 * sizeof(int16_t);
    return;

stopped:
    if (state)
        state->audio_playing = false;
    *start = NULL;
    *size = 0;
}

static void vm_audio_stop(struct ig_vm_state *state)
{
    unsigned int handle;

    rb->pcm_play_lock();
    if (rb->mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK) !=
        CHANNEL_STOPPED)
        rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    for (handle = 1; handle < ARRAYLEN(state->audio_objects); ++handle)
        state->audio_objects[handle].playing = false;
    state->audio_playing = false;
    rb->pcm_play_unlock();
}

static void vm_audio_stop_handle(struct ig_vm_state *state, u32 handle)
{
    struct ig_vm_audio_object *object = vm_audio_object(state, handle);

    if (!object)
        return;
    rb->pcm_play_lock();
    object->playing = false;
    object->paused = false;
    object->position = 0;
    if (!vm_audio_voice_count(state))
    {
        if (rb->mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK) !=
            CHANNEL_STOPPED)
            rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
        state->audio_playing = false;
    }
    rb->pcm_play_unlock();
}

static void vm_audio_pause_handle(struct ig_vm_state *state, u32 handle)
{
    struct ig_vm_audio_object *object = vm_audio_object(state, handle);

    if (!object || !object->playing)
        return;
    rb->pcm_play_lock();
    object->playing = false;
    object->paused = true;
    if (!vm_audio_voice_count(state))
    {
        if (rb->mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK) !=
            CHANNEL_STOPPED)
            rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
        state->audio_playing = false;
    }
    rb->pcm_play_unlock();
}

static void vm_audio_resume_handle(struct ig_vm_state *state, u32 handle)
{
    struct ig_vm_audio_object *object = vm_audio_object(state, handle);
    enum channel_status status;

    if (!state->audio_initialized || !object || !object->paused)
        return;
    rb->pcm_play_lock();
    object->paused = false;
    object->playing = true;
    state->audio_playing = true;
    status = rb->mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK);
    rb->pcm_play_unlock();
    if (status == CHANNEL_STOPPED)
    {
        rb->mixer_channel_set_amplitude(PCM_MIXER_CHAN_PLAYBACK,
                                        MIX_AMP_UNITY);
        rb->mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK,
                                    vm_audio_get_more, NULL, 0);
    }
}

static void vm_audio_start(struct ig_vm_state *state, u32 handle)
{
    struct ig_vm_audio_object *object = vm_audio_object(state, handle);
    enum channel_status status;
    unsigned int voices;

    if (!state->audio_initialized || !object ||
        object->rate != state->audio_output_rate || object->bits != 16 ||
        object->channels != 1 || !object->bytes)
        return;
    rb->pcm_play_lock();
    object->position = 0;
    object->repeats_remaining = object->repeat_count;
    object->paused = false;
    object->playing = true;
    vm_audio_update_gains(object);
    voices = vm_audio_voice_count(state);
    if (voices > state->audio_max_voices)
        state->audio_max_voices = voices;
    state->audio_playing = true;
    status = rb->mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK);
    rb->pcm_play_unlock();
    if (status == CHANNEL_STOPPED)
    {
        rb->mixer_channel_set_amplitude(PCM_MIXER_CHAN_PLAYBACK,
                                        MIX_AMP_UNITY);
        rb->mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK,
                                    vm_audio_get_more, NULL, 0);
    }
    ++state->audio_play_starts;
}

static u32 vm_audio_system_volume(void)
{
    int minimum = rb->sound_min(SOUND_VOLUME);
    int maximum = rb->sound_max(SOUND_VOLUME);
    int volume = rb->global_status->volume;

    volume = MAX(minimum, MIN(maximum, volume));
    if (maximum <= minimum)
        return 255;
    return (u32)(((long)(volume - minimum) * 255 +
                 (maximum - minimum) / 2) / (maximum - minimum));
}

static void vm_audio_set_system_volume(u32 normalized)
{
    int minimum = rb->sound_min(SOUND_VOLUME);
    int maximum = rb->sound_max(SOUND_VOLUME);
    int volume;

    normalized = MIN(normalized, 255u);
    if (maximum <= minimum)
        return;
    volume = minimum + (int)(((long)normalized * (maximum - minimum) +
                              127) / 255);
    rb->global_status->volume = volume;
    rb->sound_set(SOUND_VOLUME, volume);
}

static void vm_audio_init(struct ig_vm_state *state)
{
    state->audio_old_frequency = rb->mixer_get_frequency();
    state->audio_output_rate = state->is_vortex ?
        IG_VM_VORTEX_AUDIO_RATE : IG_VM_AUDIO_RATE;
#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->mixer_set_frequency(state->audio_output_rate);
    rb->pcmbuf_fade(false, true);
    state->audio_initialized = true;
}

static void vm_audio_shutdown(struct ig_vm_state *state)
{
    if (!state->audio_initialized)
        return;
    vm_audio_stop(state);
    rb->pcmbuf_fade(false, false);
    if (state->audio_old_frequency)
        rb->mixer_set_frequency(state->audio_old_frequency);
    state->audio_initialized = false;
}

#ifdef SIMULATOR
static bool vm_audio_multivoice_probe(struct ig_vm_state *state)
{
    struct ig_vm_audio_object *objects[2];
    const void *start = NULL;
    size_t size = 0;
    unsigned int handle;
    unsigned int found = 0;

    vm_audio_stop(state);
    for (handle = 1; handle < ARRAYLEN(state->audio_objects) && found < 2;
         ++handle)
    {
        struct ig_vm_audio_object *object = &state->audio_objects[handle];

        if (!object->allocated || object->data < IG_VM_BASE ||
            object->rate != state->audio_output_rate || object->bits != 16 ||
            object->channels != 1 || !object->bytes)
            continue;
        objects[found++] = object;
    }
    if (found != 2)
        return false;
    for (handle = 0; handle < 2; ++handle)
    {
        objects[handle]->position = 0;
        objects[handle]->playing = true;
        vm_audio_update_gains(objects[handle]);
    }
    state->audio_playing = true;
    vm_audio_get_more(&start, &size);
    state->audio_multivoice_probe_pass = start == audio_mix_buffer &&
        size > 0 && objects[0]->position > 0 && objects[1]->position > 0;
    vm_audio_stop(state);
    return state->audio_multivoice_probe_pass;
}
#endif

static u32 vm_audio_framework_call(struct ig_vm_state *state,
                                   unsigned int ordinal)
{
    struct ig_vm_audio_object *object;
    u32 handle = state->cpu.r[0];

    if (ordinal == 0)
    {
        handle = ++state->audio_next_handle;
        if (handle >= ARRAYLEN(state->audio_objects))
            return 0;
        object = &state->audio_objects[handle];
        rb->memset(object, 0, sizeof(*object));
        object->amplitude = 32767;
        object->pitch = 1000;
        object->repeat_count = 1;
        vm_audio_update_gains(object);
        object->allocated = true;
        return handle;
    }
    if (ordinal == 51)
        return vm_audio_system_volume();
    if (ordinal == 52)
        return 255;
    if (ordinal == 53)
    {
        vm_audio_set_system_volume(handle);
        return 0;
    }
    object = vm_audio_object(state, handle);
    if (!object)
        return 0;
    switch (ordinal)
    {
        case 1:
            vm_audio_stop_handle(state, handle);
            object->allocated = false;
            break;
        case 2:
            vm_audio_start(state, handle);
            break;
        case 3:
            vm_audio_pause_handle(state, handle);
            break;
        case 4:
            vm_audio_resume_handle(state, handle);
            break;
        case 5:
            vm_audio_stop_handle(state, handle);
            break;
        case 7:
            object->data = state->cpu.r[1];
            break;
        case 8:
            object->bytes = state->cpu.r[1];
            break;
        case 10:
            object->rate = state->cpu.r[1];
            break;
        case 11:
            object->channels = state->cpu.r[1];
            break;
        case 12:
            object->bits = state->cpu.r[1];
            break;
        case 13:
            object->amplitude = state->cpu.r[1];
            break;
        case 14:
            object->pan = (int)state->cpu.r[1];
            break;
        case 15:
            object->pitch = state->cpu.r[1];
            break;
        case 16:
            object->repeat_count = state->cpu.r[1];
            break;
        case 23:
            return object->data;
        case 39:
            return object->playing;
        default:
            break;
    }
    return 0;
}

static void vm_framework_call(struct ig_vm_state *state, u32 address)
{
#ifdef USEC_TIMER
    unsigned long profile_start = USEC_TIMER;
#endif
    u32 index = (address - IG_VM_TRAP_BASE) / 4;
    unsigned char framework = state->import_framework[index];
    unsigned int ordinal = state->import_ordinal[index];
    u32 result = 0;

    ++state->framework_calls;
    state->call_history[state->call_history_next] =
        ((unsigned int)framework << 16) | ordinal;
    state->call_history_next =
        (state->call_history_next + 1) % ARRAYLEN(state->call_history);
    if (state->call_history_count < ARRAYLEN(state->call_history))
        ++state->call_history_count;
    state->last_framework_id = framework;
    state->last_framework_ordinal = ordinal;
    state->last_framework_lr = state->cpu.r[reg(&state->cpu, LR)];
    state->last_framework_r0 = state->cpu.r[0];
    if (framework == IG_VM_FW_ASYNC_FILE_IO &&
        ordinal < ARRAYLEN(state->async_ordinal_counts))
    {
        unsigned int argument;

        ++state->async_ordinal_counts[ordinal];
        for (argument = 0; argument < 4; ++argument)
            state->async_last_args[ordinal][argument] =
                state->cpu.r[argument];
    }
    if (framework == IG_VM_FW_METADATA &&
        ordinal < ARRAYLEN(state->metadata_ordinal_counts))
    {
        unsigned int argument;

        ++state->metadata_ordinal_counts[ordinal];
        for (argument = 0; argument < 4; ++argument)
            state->metadata_last_args[ordinal][argument] =
                state->cpu.r[argument];
    }
    if (framework == IG_VM_FW_AUDIO &&
        ordinal < ARRAYLEN(state->audio_ordinal_counts))
    {
        unsigned int argument;

        ++state->audio_ordinal_counts[ordinal];
        for (argument = 0; argument < 4; ++argument)
            state->audio_last_args[ordinal][argument] =
                state->cpu.r[argument];
    }
    if (framework == IG_VM_FW_OPENGLES &&
        ordinal < ARRAYLEN(state->gles_ordinal_counts))
    {
        unsigned int argument;

        ++state->gles_ordinal_counts[ordinal];
        for (argument = 0; argument < 4; ++argument)
            state->gles_last_args[ordinal][argument] =
                state->cpu.r[argument];
#ifdef SIMULATOR
        if (state->is_vortex &&
            (ordinal == 19 || ordinal == 21 || ordinal == 99 ||
             ordinal == 105))
        {
            int flags = O_WRONLY | O_CREAT;
            int fd;
            u32 stack = state->cpu.r[reg(&state->cpu, SP)];
            u32 words[8] = { 0 };

            if (!state->gles_texture_trace_started)
                flags |= O_TRUNC;
            for (argument = 0; argument < ARRAYLEN(words); ++argument)
                vm_guest_u32(state, stack + argument * 4,
                             &words[argument]);
            fd = rb->open(IG_ROOT_DIR "/ipodgames-vortex-texture-api.log",
                          flags, 0666);
            if (fd >= 0)
            {
                if (state->gles_texture_trace_started)
                    rb->lseek(fd, 0, SEEK_END);
                rb->fdprintf(fd,
                    "frame:%u/ordinal:%u/current:%u/units:%u,%u/"
                    "r:%08x,%08x,%08x,%08x/"
                    "s:%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x\n",
                    state->synthetic_input_frame, ordinal,
                    state->bound_texture ? state->bound_texture->id : 0,
                    state->bound_textures[0] ?
                        state->bound_textures[0]->id : 0,
                    state->bound_textures[1] ?
                        state->bound_textures[1]->id : 0,
                    state->cpu.r[0], state->cpu.r[1], state->cpu.r[2],
                    state->cpu.r[3], words[0], words[1], words[2], words[3],
                    words[4], words[5], words[6], words[7]);
                rb->close(fd);
                state->gles_texture_trace_started = true;
            }
        }
#endif
    }
    if (framework == IG_VM_FW_MISC && ordinal == 0)
        result = vm_heap_allocate(state, state->cpu.r[0]);
    else if (framework == IG_VM_FW_MISC && ordinal == 9)
    {
        u32 timestamp = state->clock_ticks;

        /* Interactive Vortex must see elapsed wall time just as it did under
         * retailOS.  Advancing by a fixed amount per emulated frame stretches
         * the logo and every transition whenever software rendering misses
         * 60 fps.  Lifecycle tests retain the deterministic 60 Hz clock. */
#ifdef USEC_TIMER
        if (state->realtime_clock)
            timestamp = USEC_TIMER;
        else
#endif
        if (state->is_vortex &&
            state->cpu.r[reg(&state->cpu, LR)] == 0x1802196cu)
            timestamp += IG_VM_EVENT_CLOCK_HZ / 60;
        stw(&state->machine, &state->cpu, state->cpu.r[0], timestamp);
    }
    else if (framework == IG_VM_FW_MISC && ordinal == 10)
        result = 1000;
    else if (framework == IG_VM_FW_MISC && ordinal == 1)
        vm_heap_free(state, state->cpu.r[0]);
    else if (framework == IG_VM_FW_MISC && ordinal == 2)
        result = vm_heap_reallocate(state, state->cpu.r[0],
                                    state->cpu.r[1]);
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    else if (framework == IG_VM_FW_MISC && ordinal == 5)
        vm_system_set_brightness(state, state->cpu.r[0]);
    else if (framework == IG_VM_FW_MISC && ordinal == 6)
        result = vm_system_get_brightness(state);
#endif
    else if (framework == IG_VM_FW_MISC && ordinal == 7)
    {
        /* Foreground/resume notification. Rockbox already keeps this plugin
         * foreground-only; no additional host state is required. */
    }
    else if (framework == IG_VM_FW_MISC && ordinal == 12)
        result = vm_system_clock(state, state->cpu.r[0]);
    else if (framework == IG_VM_FW_MISC && ordinal == 13)
        result = vm_system_battery_level(state);
    else if (framework == IG_VM_FW_AUDIO)
        result = vm_audio_framework_call(state, ordinal);
    else if (framework == IG_VM_FW_ASYNC_FILE_IO && ordinal == 12)
        result = vm_save_open(state, state->cpu.r[1], state->cpu.r[2]);
    else if (framework == IG_VM_FW_ASYNC_FILE_IO && ordinal == 16)
        result = vm_save_write(state, state->cpu.r[0], state->cpu.r[1],
                               state->cpu.r[2]);
    else if (framework == IG_VM_FW_ASYNC_FILE_IO && ordinal == 14)
        result = vm_save_close(state, state->cpu.r[0]);
    else if (framework == IG_VM_FW_ASYNC_FILE_IO && ordinal == 3)
    {
        vm_async_read(state, state->cpu.r[0], state->cpu.r[1],
                      state->cpu.r[2]);
        result = 1;
    }
    else if (framework == IG_VM_FW_ASYNC_FILE_IO && ordinal == 2)
        result = vm_async_read_opened(state, state->cpu.r[0]) ? 1 : 0;
    else if (framework == IG_VM_FW_ASYNC_FILE_IO && ordinal == 1)
    {
        result = vm_async_close_request(state, state->cpu.r[0]) ? 1 : 0;
    }
    else if (framework == IG_VM_FW_ASYNC_FILE_IO && ordinal == 0)
    {
        vm_async_open(state, state->cpu.r[1], state->cpu.r[3]);
        result = 1;
    }
    else if (framework == IG_VM_FW_INPUT_EVENTS && ordinal == 0)
    {
        vm_guest_put_u32(state, state->cpu.r[0], state->wheel_delta);
        vm_guest_put_u32(state, state->cpu.r[1], state->wheel_raw);
        ++state->input_polls;
    }
    else if (framework == IG_VM_FW_INPUT_EVENTS && ordinal == 1)
    {
        /* Event records live in the per-frame scratch list and have no
         * separate host allocation to release. */
    }
    else if (framework == IG_VM_FW_SETTINGS && ordinal == 0)
        result = vm_system_setting(state, state->cpu.r[0], state->cpu.r[1],
                                   state->cpu.r[2]);
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 12)
    {
        if (state->cpu.r[0] & 0x4000)
            vm_clear_framebuffer(state);
    }
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 13)
        state->clear_color = LCD_RGBPACK(
            vm_float_color(state->cpu.r[0]),
            vm_float_color(state->cpu.r[1]),
            vm_float_color(state->cpu.r[2]));
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 0 &&
             state->cpu.r[0] >= 0x84c0 && state->cpu.r[0] <= 0x84c1)
    {
        state->active_texture_unit = state->cpu.r[0] - 0x84c0;
        state->bound_texture =
            state->bound_textures[state->active_texture_unit];
    }
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 4)
        vm_bind_texture(state, state->cpu.r[1]);
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 45)
    {
        u32 count = state->cpu.r[0];
        u32 names = state->cpu.r[1];
        u32 texture;

        for (texture = 0; texture < count; ++texture)
        {
            if (!state->next_texture_id)
                state->next_texture_id = 1;
            if (!vm_guest_put_u32(state, names + texture * 4,
                                  state->next_texture_id++))
                break;
        }
    }
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 125)
        vm_matrix_uniform(state, state->cpu.r[3]);
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 148 &&
             state->gles_mode < IG_GLES_MODES)
    {
        unsigned int component;

        for (component = 0; component < 4; ++component)
        {
            u32 value;

            if (vm_guest_u32(state, state->cpu.r[2] + component * 4,
                             &value))
                state->gles_uniform_color[state->gles_mode][component] =
                    vm_float_color(value);
        }
    }
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 165)
        vm_matrix_identity(state, state->cpu.r[0]);
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 167)
        vm_matrix_ortho(state, state->cpu.r[0]);
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 169)
        vm_matrix_translate(state, state->cpu.r[0]);
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 171)
        vm_matrix_scale(state, state->cpu.r[0]);
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 173)
        vm_matrix_rotate(state, state->cpu.r[0]);
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 175)
        vm_matrix_multiply(state, state->cpu.r[0], state->cpu.r[1],
                           state->cpu.r[2]);
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 159)
        state->gles_mode = state->cpu.r[0];
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 19)
        vm_upload_compressed_texture(state);
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 21)
        vm_copy_framebuffer_texture(state);
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 99)
    {
        struct ig_vm_texture *texture = state->bound_texture;
        u32 stack = state->cpu.r[reg(&state->cpu, SP)];
        u32 value;

        if (!texture)
            texture = vm_bind_texture(state, 0);
        if (!texture)
            goto framework_complete;
        texture->width = state->cpu.r[3];
        if (vm_guest_u32(state, stack, &value))
            texture->height = value;
        texture->format = state->cpu.r[2];
        texture->all_opaque = false;
        texture->all_transparent = false;
        if (vm_guest_u32(state, stack + 12, &value))
            texture->type = value;
        if (vm_guest_u32(state, stack + 16, &value))
            texture->pointer = value;
        if (!texture->name[0] && state->last_async_file[0] &&
            (state->is_vortex ||
             !rb->strncmp(state->last_async_file, "tex_", 4)))
            rb->strlcpy(texture->name, state->last_async_file,
                        sizeof(texture->name));
        if (texture->pointer >= IG_VM_BASE &&
            texture->width && texture->height)
        {
            size_t texture_pixels =
                (size_t)texture->width * texture->height;
            size_t source_bytes = 0;
            size_t texture_bytes = texture_pixels * sizeof(*texture->pixels);
            u32 source_offset = texture->pointer - IG_VM_BASE;

            if (texture->format == 0x1908 && texture->type == 0x8034)
                source_bytes = texture_pixels * 2;
            else if (texture->format == 0x1907 &&
                     texture->type == 0x8363)
                source_bytes = texture_pixels * 2;
            else if (texture->format == 0x1908 &&
                     texture->type == 0x8033)
                source_bytes = texture_pixels * 2;
            else if (texture->format == 0x1908 &&
                     texture->type == 0x1401)
                source_bytes = texture_pixels * 4;
            else if (texture->format == 0x1907 &&
                     texture->type == 0x1401)
                source_bytes = texture_pixels * 3;
            else if (texture->format == 0x1906 &&
                     texture->type == 0x1401)
                source_bytes = texture_pixels;

            if (texture->capacity < texture_bytes &&
                texture_bytes <= state->texture_storage_size -
                                 state->texture_storage_used)
            {
                texture->pixels = (u32 *)(state->texture_storage +
                                          state->texture_storage_used);
                texture->capacity = texture_bytes;
                state->texture_storage_used += texture_bytes;
            }
            if (source_bytes && texture->pixels &&
                source_offset <= state->memory_size &&
                state->memory_size - source_offset >= source_bytes)
            {
                vm_cache_texture_pixels(texture->pixels,
                                        state->memory + source_offset,
                                        texture_pixels, texture->format,
                                        texture->type);
                vm_analyze_texture(texture);
            }
        }
    }
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 105)
        vm_update_texture(state);
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 137 &&
             state->cpu.r[0] < ARRAYLEN(state->attributes))
    {
        struct ig_vm_attribute *attribute =
            &state->attributes[state->cpu.r[0]];
        u32 stack = state->cpu.r[reg(&state->cpu, SP)];
        u32 value;

        attribute->size = state->cpu.r[1];
        attribute->type = state->cpu.r[2];
        if (vm_guest_u32(state, stack, &value))
            attribute->stride = value;
        if (!attribute->stride)
            attribute->stride = attribute->size * 4;
        if (vm_guest_u32(state, stack + 4, &value))
            attribute->pointer = value;
    }
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 37 &&
             state->cpu.r[2] >= 3)
    {
        unsigned int first = state->cpu.r[1];
        unsigned int count = state->cpu.r[2];
#ifdef USEC_TIMER
        unsigned long draw_start = USEC_TIMER;
#endif
        if (state->cpu.r[0] == 7 && count >= 4)
        {
            unsigned int vertex;

            for (vertex = 0; vertex + 4 <= count; vertex += 4)
                vm_draw_quad(state, first + vertex, false);
        }
        else if (state->cpu.r[0] == 5)
        {
            unsigned int vertex;

            if (count == 4)
                vm_draw_quad(state, first, true);
            else
                for (vertex = 0; vertex + 2 < count; ++vertex)
                    vm_draw_triangle(state, first, vertex,
                                     vertex + 1, vertex + 2);
        }
        else if (state->cpu.r[0] == 4)
        {
            unsigned int vertex;

            for (vertex = 0; vertex + 2 < count; vertex += 3)
                vm_draw_triangle(state, first, vertex,
                                 vertex + 1, vertex + 2);
        }
        else if (state->cpu.r[0] == 6)
        {
            unsigned int vertex;

            for (vertex = 1; vertex + 1 < count; ++vertex)
                vm_draw_triangle(state, first, 0, vertex, vertex + 1);
        }
#ifdef USEC_TIMER
        {
            unsigned long draw_elapsed =
                (unsigned long)(USEC_TIMER - draw_start);

            state->profile_draw_us += draw_elapsed;
            if (state->gles_mode < IG_GLES_MODES)
            {
                state->profile_draw_mode_us[state->gles_mode] +=
                    draw_elapsed;
                ++state->profile_draw_mode_calls[state->gles_mode];
            }
        }
#endif
    }
    else if (framework == IG_VM_FW_OPENGLES && ordinal == 157)
    {
#ifdef USEC_TIMER
        unsigned long lcd_start = USEC_TIMER;
#endif
        rb->lcd_bitmap(state->framebuffer, 0, 0, LCD_WIDTH, LCD_HEIGHT);
        rb->lcd_update();
#ifdef USEC_TIMER
        state->profile_lcd_us +=
            (unsigned long)(USEC_TIMER - lcd_start);
#endif
        ++state->presented_frames;
    }

framework_complete:
    state->cpu.r[0] = result;
    state->cpu.r[PC] = state->cpu.r[reg(&state->cpu, LR)];
    fill_pipeline(&state->cpu);
#ifdef USEC_TIMER
    state->profile_framework_us +=
        (unsigned long)(USEC_TIMER - profile_start);
#endif
}

static bool vm_call_guest(struct ig_vm_state *state, u32 entry,
                          u32 r0, u32 r1, u32 r2, u32 r3)
{
    cpu_t saved_cpu = state->cpu;
    unsigned long start = state->machine.instructions;

    state->cpu.r[reg(&state->cpu, SP)] = IG_VM_STACK_TOP - 0x2000;
    state->cpu.r[reg(&state->cpu, LR)] = IG_VM_CALLBACK_RETURN;
    state->cpu.r[0] = r0;
    state->cpu.r[1] = r1;
    state->cpu.r[2] = r2;
    state->cpu.r[3] = r3;
    state->cpu.r[PC] = entry;
    fill_pipeline(&state->cpu);
    while (!state->machine.stopped &&
           state->machine.instructions - start < IG_VM_PHASE_LIMIT)
    {
        u32 address = state->cpu.r[PC] - 8;

        if (address == IG_VM_CALLBACK_RETURN)
        {
            state->cpu = saved_cpu;
            return true;
        }
        if (address >= IG_VM_TRAP_BASE && address < IG_VM_TRAP_LIMIT &&
            !(address & 3) &&
            (address - IG_VM_TRAP_BASE) / 4 < state->import_count)
        {
            vm_framework_call(state, address);
            continue;
        }
#ifdef SIMULATOR
        state->guest_history_pc[state->guest_history_next] = address;
        state->guest_history_r4[state->guest_history_next] = state->cpu.r[4];
        state->guest_history_r5[state->guest_history_next] = state->cpu.r[5];
        state->guest_history_sp[state->guest_history_next] =
            state->cpu.r[reg(&state->cpu, SP)];
        state->guest_history_next = (state->guest_history_next + 1) %
                                    IG_VM_GUEST_HISTORY;
        if (state->guest_history_count < IG_VM_GUEST_HISTORY)
            ++state->guest_history_count;
        if (address == 0x1800ff60u)
        {
            ++state->frontend_updates;
            state->frontend_mode = state->cpu.r[1];
        }
        else if (address == 0x18010024u)
            ++state->frontend_complete;
        else if (address == 0x18010038u)
            ++state->frontend_blocked;
        else if (state->is_vortex && address == 0x180114a0u)
            vm_trace_vortex_input_parser(state);
        else if (!state->is_vortex && address == 0x18009090u)
            vm_trace_input_handler(state);
        else if (!state->is_vortex && address == 0x180093c8u)
            ++state->input_select_calls;
        else if (address == 0x1800ff1cu)
            ++state->input_transition_requests;
        else if (address == 0x180047a0u)
            ++state->audio_resource_callbacks;
        else if (address == 0x180162ecu)
            state->audio_trace |= 0x01;
        else if (address == 0x180163e8u)
            state->audio_trace |= 0x02;
        else if (address == 0x180164bcu)
            state->audio_trace |= 0x04;
        else if (address == 0x18016510u)
            state->audio_trace |= 0x08;
        else if (address == 0x18016548u)
            state->audio_trace |= 0x10;
        else if (address == 0x18015fa0u)
            state->audio_trace |= 0x20;
        else if (address == 0x1800192cu && !state->fatal_signal[4])
        {
            state->fatal_signal[0] = state->cpu.r[0];
            state->fatal_signal[1] = state->cpu.r[1];
            state->fatal_signal[2] = state->cpu.r[2];
            state->fatal_signal[3] = state->cpu.r[3];
            state->fatal_signal[4] = state->cpu.r[reg(&state->cpu, LR)];
        }
#endif
#ifdef SIMULATOR
        if (state->hardware_exec)
            execute_until(&state->machine, &state->cpu, IG_VM_TRAP_BASE,
                          start + IG_VM_PHASE_LIMIT);
        else
            execute(&state->machine, &state->cpu);
#else
#ifdef USEC_TIMER
        unsigned long profile_start = USEC_TIMER;
#endif

        execute_until(&state->machine, &state->cpu, IG_VM_TRAP_BASE,
                      start + IG_VM_PHASE_LIMIT);
#ifdef USEC_TIMER
        state->profile_guest_us +=
            (unsigned long)(USEC_TIMER - profile_start);
#endif
#endif
    }
    if (!state->machine.stopped)
    {
        state->machine.fault = 3;
        state->machine.fault_address = state->cpu.r[PC] - 8;
    }
    return false;
}

static bool vm_call_guest5(struct ig_vm_state *state, u32 entry,
                           u32 r0, u32 r1, u32 r2, u32 r3, u32 stack0)
{
    u32 stack = IG_VM_STACK_TOP - 0x2000;

    vm_guest_put_u32(state, stack, stack0);
    return vm_call_guest(state, entry, r0, r1, r2, r3);
}

static bool vm_complete_audio_data(struct ig_vm_state *state,
                                   u32 audio_context)
{
    char filename[128];
    char path[MAX_PATH];
    u32 loader = 0;
    u32 destination = 0;
    u32 data_size = 0;
    u32 file_offset = 0;
    u32 destination_offset;
    int fd = -1;
    bool success = false;

    if (!vm_guest_u32(state, IG_MSPAC_AUDIO_LOADER_POINTER, &loader) ||
        !loader ||
        !vm_guest_u32(state, loader + 20, &destination) ||
        !vm_guest_u32(state, audio_context + 64, &data_size) ||
        !vm_guest_u32(state, audio_context + 68, &file_offset) ||
        !vm_guest_string(state, audio_context + 8, filename,
                         sizeof(filename)) ||
        !filename[0] || filename[0] == PATH_SEPCH ||
        rb->strstr(filename, "..") || rb->strchr(filename, '\\') ||
        destination < IG_VM_BASE)
        goto complete;
    destination_offset = destination - IG_VM_BASE;
    if (!data_size || destination_offset > state->memory_size ||
        state->memory_size - destination_offset < data_size)
        goto complete;
    rb->snprintf(path, sizeof(path), "%s/assets/%s",
                 state->game_directory, filename);
    fd = rb->open(path, O_RDONLY);
    if (fd < 0 || rb->lseek(fd, file_offset, SEEK_SET) !=
                  (off_t)file_offset ||
        rb->read(fd, state->memory + destination_offset, data_size) !=
                  (ssize_t)data_size)
        goto complete;
    ++state->audio_data_reads;
    state->audio_data_bytes += data_size;
    success = true;

complete:
    if (fd >= 0)
        rb->close(fd);
    if (!success)
        return false;
    return vm_call_guest(state, IG_MSPAC_AUDIO_DATA_COMPLETE,
                         0, audio_context, 0, 0);
}

static bool vm_complete_async(struct ig_vm_state *state)
{
    u32 callback = state->pending_callback;
    u32 context = state->pending_context;
    u32 bytes = state->pending_bytes;
    u32 status = state->pending_status;
    bool object_callback = state->pending_object_callback;

    state->pending_callback = 0;
    state->pending_object_callback = false;
    if (!callback)
        return true;
    /* Ms. PAC-MAN's wrapper first releases a retailOS-owned request object.
     * Enter its completion half directly because Rockbox owns no such object.
     */
    if (callback == IG_MSPAC_ASYNC_CALLBACK)
        return vm_call_guest(state, IG_MSPAC_ASYNC_COMPLETE,
                             context, status, bytes, 0);
    if (callback == IG_MSPAC_ASYNC_OPEN_CALLBACK)
    {
        u32 audio_context = state->pending_audio_context;

        if (!vm_call_guest(state, IG_MSPAC_ASYNC_OPEN_COMPLETE,
                           context, status, bytes, 0))
            return false;
        if (context >= IG_VM_BASE &&
            context + 4 < IG_VM_BASE + state->memory_size)
            state->memory[context + 4 - IG_VM_BASE] = 0;
        state->pending_audio_context = 0;
        if (!audio_context || status)
            return true;
        return vm_complete_audio_data(state, audio_context);
    }
    if (object_callback)
        return vm_call_guest(state, callback, context, context,
                             status, bytes);
    return vm_call_guest(state, callback, context, status, bytes, 0);
}

static bool vm_run_entry(struct ig_vm_state *state, u32 entry,
                         bool event, unsigned long *phase_instructions);

#ifdef SIMULATOR
static bool vm_armemu_signed_byte_probe(void)
{
    unsigned char memory[32];
    machine_t machine;
    cpu_t cpu;

    rb->memset(memory, 0, sizeof(memory));
    rb->memset(&machine, 0, sizeof(machine));
    /* ldrsb r0, [r1, #1] */
    memory[0] = 0xd1;
    memory[1] = 0x00;
    memory[2] = 0xd1;
    memory[3] = 0xe1;
    memory[16] = 0x11;
    memory[17] = 0x82;
    reset(&cpu);
    cpu.r[1] = 0x1010;
    cpu.r[PC] = 0x1000;
    fill_pipeline(&cpu);
    machine.cpu = &cpu;
    machine.drambase = 0x1000;
    machine.dramsize = sizeof(memory);
    machine.dram = memory;
    execute(&machine, &cpu);
    return !machine.fault && cpu.r[0] == 0xffffff82u;
}

static unsigned long vm_framebuffer_hash(const struct ig_vm_state *state)
{
    const unsigned char *pixels =
        (const unsigned char *)state->framebuffer;
    unsigned long checksum = 2166136261u;
    size_t index;

    for (index = 0; index < IG_VM_FRAMEBUFFER_BYTES; ++index)
        checksum = (checksum ^ pixels[index]) * 16777619u;
    return checksum;
}

static bool vm_wheel_sweep_probe(struct ig_vm_state *state,
                                 unsigned long *phase_instructions,
                                 struct ig_vm_report *report)
{
    unsigned long hashes[129];
    unsigned int previous = vm_retail_wheel_position(0);
    unsigned int wheel;
    unsigned int unique = 0;
    bool mapping_pass = previous == 61;

    for (wheel = 1; wheel < 96; ++wheel)
    {
        unsigned int position = vm_retail_wheel_position(wheel);
        unsigned int delta = (previous - position) & 0xffu;

        if (delta < 2 || delta > 3)
            mapping_pass = false;
        previous = position;
    }
    if (((previous - vm_retail_wheel_position(0)) & 0xffu) < 2 ||
        ((previous - vm_retail_wheel_position(0)) & 0xffu) > 3)
        mapping_pass = false;

    state->scripted_input = false;
    state->wheel_raw = 0x40000000u;
    state->wheel_delta = 0;
    if (!vm_run_entry(state, 0x1801a734u, true,
                      phase_instructions))
        return false;
    hashes[0] = vm_framebuffer_hash(state);
    vm_dump_frame_to(state, IG_ROOT_DIR "/ipodgames-wheel-0.ppm");
    vm_dump_frame_to(state, IG_ROOT_DIR "/ipodgames-motion-000.ppm");
    for (wheel = 1; wheel < ARRAYLEN(hashes); ++wheel)
    {
        unsigned int position = (256 - (wheel * 16 & 255)) & 255;

        state->wheel_raw = 0x40000000u | position;
        state->wheel_delta = (u32)-16;
        if (!vm_run_entry(state, 0x1801a734u, true,
                          phase_instructions))
            return false;
        hashes[wheel] = vm_framebuffer_hash(state);
        if (!(wheel & 31))
        {
            char path[MAX_PATH];

            rb->snprintf(path, sizeof(path),
                         IG_ROOT_DIR "/ipodgames-motion-%03u.ppm", wheel);
            vm_dump_frame_to(state, path);
        }
        if (wheel == 4 || wheel == 8 || wheel == 12 || wheel == 16)
        {
            char path[MAX_PATH];

            rb->snprintf(path, sizeof(path),
                         IG_ROOT_DIR "/ipodgames-wheel-cardinal-%u.ppm",
                         position);
            vm_dump_frame_to(state, path);
        }
    }
    for (wheel = 0; wheel < ARRAYLEN(hashes); ++wheel)
    {
        unsigned int earlier;

        for (earlier = 0; earlier < wheel; ++earlier)
            if (hashes[earlier] == hashes[wheel])
                break;
        if (earlier == wheel)
            ++unique;
    }
    state->wheel_raw = 0x30;
    state->wheel_delta = 0;
    report->wheel_mapping_pass = mapping_pass;
    report->wheel_sweep_frames = ARRAYLEN(hashes);
    report->wheel_sweep_unique_frames = unique;
    return mapping_pass && unique >= 26;
}

static bool vm_save_probe(struct ig_vm_state *state)
{
    static const unsigned char expected[] = {
        0x49, 0x50, 0x4f, 0x44, 0x47, 0x41, 0x4d, 0x45,
        0x53, 0x2d, 0x53, 0x41, 0x56, 0x45, 0x2d, 0x31,
    };
    static const char filename[] = "save/rockbox-probe.dat";
    unsigned char actual[sizeof(expected)];
    u32 name_address = IG_VM_SCRATCH_BASE + 0x6000;
    u32 data_address = IG_VM_SCRATCH_BASE + 0x6100;
    char path[MAX_PATH];
    unsigned int writes = state->save_writes;
    int fd;
    bool passed = false;

    rb->memcpy(state->memory + name_address - IG_VM_BASE,
               filename, sizeof(filename));
    rb->memcpy(state->memory + data_address - IG_VM_BASE,
               expected, sizeof(expected));
    if (!vm_call_guest(state, 0x18002458u, 1, name_address,
                       data_address, sizeof(expected)) ||
        state->save_writes != writes + 1 ||
        rb->snprintf(path, sizeof(path), "%s/%s", state->save_directory,
                     filename) >= (int)sizeof(path))
        return false;
    fd = rb->open(path, O_RDONLY);
    if (fd >= 0)
    {
        if (rb->read(fd, actual, sizeof(actual)) ==
            (ssize_t)sizeof(actual) &&
            !rb->memcmp(actual, expected, sizeof(expected)))
            passed = true;
        rb->close(fd);
    }
    rb->remove(path);
    state->save_probe_pass = passed;
    return passed;
}

static bool vm_official_save_probe(struct ig_vm_state *state)
{
    static const char filename[] = "save/ms_pac_man.dat";
    char path[MAX_PATH];
    unsigned int writes = state->save_writes;
    unsigned long bytes = state->save_bytes;
    u32 game_object;
    int fd;
    bool passed = false;

    if (!vm_guest_u32(state, 0x18047280u, &game_object) ||
        !game_object ||
        !vm_call_guest(state, 0x180105e4u, game_object, 0, 0, 0) ||
        state->save_writes != writes + 1 ||
        state->save_bytes != bytes + 76 ||
        rb->strcmp(state->last_save_file, filename) ||
        rb->snprintf(path, sizeof(path), "%s/%s", state->save_directory,
                     filename) >= (int)sizeof(path))
        return false;
    fd = rb->open(path, O_RDONLY);
    if (fd >= 0)
    {
        passed = rb->filesize(fd) == 76;
        rb->close(fd);
    }
    rb->remove(path);
    state->save_probe_pass = state->save_probe_pass && passed;
    return passed;
}
#endif

static bool vm_run_entry(struct ig_vm_state *state, u32 entry,
                         bool event, unsigned long *phase_instructions)
{
    unsigned long start = state->machine.instructions;

    if (event)
    {
        state->clock_ticks += IG_VM_EVENT_CLOCK_HZ / 60;
        state->clock_fraction += IG_VM_EVENT_CLOCK_HZ % 60;
        if (state->clock_fraction >= 60)
        {
            ++state->clock_ticks;
            state->clock_fraction -= 60;
        }
    }
    if (event && !vm_complete_async(state))
    {
        *phase_instructions = state->machine.instructions - start;
        return false;
    }
    if (event)
    {
        u32 context_offset = IG_VM_SCRATCH_BASE - IG_VM_BASE;

        vm_put_u32(state->memory + context_offset + 48, 0);
#ifdef SIMULATOR
        if (state->is_vortex && state->scripted_input)
        {
            static const unsigned short play_clicks[] = {
                180, 480, 620, 920, 1220, 1520, 1820, 2200,
            };
            unsigned int first_click =
                getenv("IPODGAMES_TEST_VORTEX_FAST") ? 180u : 780u;
            bool play_path =
                getenv("IPODGAMES_TEST_VORTEX_PLAY") != NULL;
            bool press = false;
            bool release = false;
            unsigned int click;

            ++state->synthetic_input_frame;
            /* Stock reaches "PRESS ENTER BUTTON" around ten seconds in and
             * the reference player clicks it at roughly thirteen seconds. */
            if (play_path)
            {
                for (click = 0; click < ARRAYLEN(play_clicks); ++click)
                {
                    press = press || state->synthetic_input_frame ==
                                       play_clicks[click];
                    release = release || state->synthetic_input_frame ==
                                           (unsigned int)play_clicks[click] + 2;
                }
            }
            else
            {
                press = state->synthetic_input_frame == first_click ||
                    state->synthetic_input_frame == first_click + 300 ||
                    state->synthetic_input_frame == first_click + 600 ||
                    state->synthetic_input_frame == first_click + 900;
                release = state->synthetic_input_frame == first_click + 2 ||
                    state->synthetic_input_frame == first_click + 302 ||
                    state->synthetic_input_frame == first_click + 602 ||
                    state->synthetic_input_frame == first_click + 902;
            }
            if (press || release)
            {
                state->button_event_type = 2;
                state->button_event_phase = press ? 2 : 1;
            }
            if (play_path &&
                (state->synthetic_input_frame == 1900 ||
                 state->synthetic_input_frame == 1902))
            {
                state->button_event_type = 1;
                state->button_event_phase =
                    state->synthetic_input_frame == 1900 ? 2 : 1;
            }
            if (play_path && state->synthetic_input_frame >= 500 &&
                state->synthetic_input_frame <= 510)
            {
                /* Establish the same touch-down reference used by retailOS
                 * before applying a relative wheel rotation. */
                state->wheel_raw = 0x40000000u;
                state->wheel_delta = 0;
            }
            else if (play_path && state->synthetic_input_frame >= 511 &&
                     state->synthetic_input_frame <= 622)
            {
                /* A is selected initially.  After committing it, rotate to
                 * the first-run name editor's DONE cell (normalized 32). */
                state->wheel_raw = 0x40000020u;
                state->wheel_delta = 0;
            }
            else if (play_path && state->synthetic_input_frame >= 1600 &&
                     state->synthetic_input_frame <= 1610)
            {
                state->wheel_raw = 0x40000000u;
                state->wheel_delta = 0;
            }
            else if (play_path && state->synthetic_input_frame >= 1611)
            {
                unsigned int position =
                    state->synthetic_input_frame - 1610;

                state->wheel_raw = 0x40000000u | (position & 0xff);
                state->wheel_delta = 1;
            }
            else if (getenv("IPODGAMES_TEST_VORTEX_WHEEL") &&
                state->synthetic_input_frame >= first_click + 80 &&
                state->synthetic_input_frame < first_click + 300)
            {
                unsigned int step =
                    (state->synthetic_input_frame - first_click - 80) / 10;

                state->wheel_raw = 0x40000000u | ((step * 16) & 0xff);
                state->wheel_delta = 0;
            }
            else
            {
                state->wheel_raw = 0x30;
                state->wheel_delta = 0;
            }
        }
#endif
        if (state->button_event_type)
        {
            u32 node = IG_VM_SCRATCH_BASE + 0x4000;
            u32 node_offset = node - IG_VM_BASE;

            rb->memset(state->memory + node_offset, 0, 16);
            state->memory[node_offset] = state->button_event_type;
            state->memory[node_offset + 1] = state->button_event_phase;
            vm_put_u32(state->memory + context_offset + 48, node);
            state->button_event_type = 0;
            state->button_event_phase = 0;
        }
        else if (!state->is_vortex && !state->ready_event_sent &&
            state->audio_data_reads >= 20 &&
            !state->pending_callback &&
            !state->memory[0x180c7650u - IG_VM_BASE])
        {
            u32 node = IG_VM_SCRATCH_BASE + 0x4000;
            u32 node_offset = node - IG_VM_BASE;

            rb->memset(state->memory + node_offset, 0, 16);
            state->memory[node_offset] = 2;
            state->memory[node_offset + 1] = 2;
            vm_put_u32(state->memory + context_offset + 48, node);
            state->ready_event_sent = true;
        }
        else if (!state->is_vortex && state->scripted_input &&
                 state->ready_event_sent &&
                 !state->pending_callback)
        {
#ifdef SIMULATOR
            bool test_options =
                getenv("IPODGAMES_TEST_OPTIONS_PATH") != NULL;
            bool test_pause =
                getenv("IPODGAMES_TEST_PAUSE_PATH") != NULL;
            bool test_motion =
                getenv("IPODGAMES_TEST_MOTION_CONTINUITY") != NULL;
            bool test_save_exit =
                getenv("IPODGAMES_TEST_SAVE_EXIT_PATH") != NULL;
#else
            const bool test_options = false;
            const bool test_pause = false;
            const bool test_motion = false;
            const bool test_save_exit = false;
#endif
            /* Exercise Ms. PAC-MAN's stock event mapper with a SELECT
             * release followed by a complete SELECT click.  The wheel
             * gesture below still enters through InputEvents[0].  Keeping
             * this in the simulator lifecycle gate makes input bring-up
             * deterministic while the interactive mapper is unfinished.
             */
            ++state->synthetic_input_frame;
            if (state->synthetic_input_frame == 2 &&
                !vm_call_guest(state, 0x18002c9cu, 2, 1, 0, 0))
            {
                *phase_instructions = state->machine.instructions - start;
                return false;
            }
            if ((state->synthetic_input_frame == 160 ||
                 state->synthetic_input_frame == 195 ||
                 state->synthetic_input_frame == 270 ||
                 state->synthetic_input_frame == 350 ||
                 (test_pause &&
                  state->synthetic_input_frame == 1000) ||
                 (test_save_exit &&
                  state->synthetic_input_frame == 960) ||
                 (!test_options &&
                  (state->synthetic_input_frame == 550 ||
                   state->synthetic_input_frame == 650))) &&
                !vm_call_guest(state, 0x18002c9cu, 2, 2, 0, 0))
            {
                *phase_instructions = state->machine.instructions - start;
                return false;
            }
            if ((state->synthetic_input_frame == 162 ||
                 state->synthetic_input_frame == 197 ||
                 state->synthetic_input_frame == 272 ||
                 state->synthetic_input_frame == 352 ||
                 (test_pause &&
                  state->synthetic_input_frame == 1002) ||
                 (test_save_exit &&
                  state->synthetic_input_frame == 962) ||
                 state->synthetic_input_frame == 552 ||
                 state->synthetic_input_frame == 652) &&
                !vm_call_guest(state, 0x18002c9cu, 2, 1, 0, 0))
            {
                *phase_instructions = state->machine.instructions - start;
                return false;
            }
            if ((test_pause || test_save_exit) &&
                (state->synthetic_input_frame == 900 ||
                 state->synthetic_input_frame == 902) &&
                !vm_call_guest(state, 0x18002c9cu, 1,
                               state->synthetic_input_frame == 900 ? 2 : 1,
                               0, 0))
            {
                *phase_instructions = state->machine.instructions - start;
                return false;
            }
            if (test_save_exit &&
                (state->synthetic_input_frame == 920 ||
                 state->synthetic_input_frame == 930 ||
                 state->synthetic_input_frame == 940 ||
                 state->synthetic_input_frame == 950))
            {
                u32 game_object;
                u32 input_component;

                if (!vm_guest_u32(state, 0x18047280u, &game_object) ||
                    !vm_guest_u32(state, game_object + 0x40,
                                  &input_component) ||
                    !vm_call_guest5(state, 0x18009090u, input_component,
                                    6, 2, 0, 24))
                {
                    *phase_instructions =
                        state->machine.instructions - start;
                    return false;
                }
            }
            if (state->synthetic_input_frame >= 8 &&
                state->synthetic_input_frame <= 10)
            {
                state->wheel_raw = 0x40000020u |
                    (state->synthetic_input_frame - 8) * 8;
                state->wheel_delta = state->synthetic_input_frame == 8 ?
                    0 : 8;
            }
            else if (state->synthetic_input_frame >= 170 &&
                     state->synthetic_input_frame <= 181)
            {
                /* InputEvents uses the retail wheel polarity: three positive
                 * detents move the carousel from A (3) to confirmation (0).
                 */
                state->wheel_raw = 0x40000020u |
                    (state->synthetic_input_frame - 170) * 8;
                state->wheel_delta = 8;
            }
            else if (test_options &&
                     state->synthetic_input_frame == 348)
            {
                u32 game_object;
                u32 input_component;

                if (!vm_guest_u32(state, 0x18047280u, &game_object) ||
                    !vm_guest_u32(state, game_object + 0x40,
                                  &input_component) ||
                    !vm_call_guest5(state, 0x18009090u, input_component,
                                    6, 2, 0, 24))
                {
                    *phase_instructions =
                        state->machine.instructions - start;
                    return false;
                }
                state->wheel_raw = 0x30;
                state->wheel_delta = 0;
            }
            else if (test_options &&
                     state->synthetic_input_frame >= 360 &&
                     state->synthetic_input_frame <= 361)
            {
                u32 game_object;
                u32 input_component;

                if (!vm_guest_u32(state, 0x18047280u, &game_object) ||
                    !vm_guest_u32(state, game_object + 0x40,
                                  &input_component) ||
                    !vm_call_guest5(state, 0x18009090u, input_component,
                                    6, 2, 0, (u32)-24))
                {
                    *phase_instructions =
                        state->machine.instructions - start;
                    return false;
                }
                state->wheel_raw = 0x30;
                state->wheel_delta = 0;
            }
            else if (state->synthetic_input_frame >= 750 &&
                     state->synthetic_input_frame <=
                         (test_motion ? 1100u : 850u))
            {
                /* Hold one retail click-wheel sector during live gameplay.
                 * This exercises the game's positional wheel mapper without
                 * relying on Rockbox's interactive button layer yet.
                 */
                state->wheel_raw = 0x40000018u;
                state->wheel_delta = 0;
            }
            else
            {
                state->wheel_raw = 0x00000030u;
                state->wheel_delta = 0;
            }
        }
    }
    state->cpu.r[reg(&state->cpu, SP)] = IG_VM_STACK_TOP;
    state->cpu.r[reg(&state->cpu, LR)] = IG_VM_RETURN;
    if (event)
    {
        state->cpu.r[0] = IG_VM_SCRATCH_BASE;
        state->cpu.r[1] = IG_VM_SCRATCH_BASE + 0x1000;
    }
    state->cpu.r[PC] = entry;
    fill_pipeline(&state->cpu);
    while (!state->machine.stopped &&
           state->machine.instructions - start < IG_VM_PHASE_LIMIT)
    {
        u32 address = state->cpu.r[PC] - 8;
        if (address == IG_VM_RETURN)
        {
            *phase_instructions = state->machine.instructions - start;
            return true;
        }
        if (address >= IG_VM_TRAP_BASE && address < IG_VM_TRAP_LIMIT &&
            !(address & 3) &&
            (address - IG_VM_TRAP_BASE) / 4 < state->import_count)
        {
            vm_framework_call(state, address);
            continue;
        }
#ifdef SIMULATOR
        state->guest_history_pc[state->guest_history_next] = address;
        state->guest_history_r4[state->guest_history_next] = state->cpu.r[4];
        state->guest_history_r5[state->guest_history_next] = state->cpu.r[5];
        state->guest_history_sp[state->guest_history_next] =
            state->cpu.r[reg(&state->cpu, SP)];
        state->guest_history_next = (state->guest_history_next + 1) %
                                    IG_VM_GUEST_HISTORY;
        if (state->guest_history_count < IG_VM_GUEST_HISTORY)
            ++state->guest_history_count;
        if (address == 0x1800ff60u)
        {
            ++state->frontend_updates;
            state->frontend_mode = state->cpu.r[1];
        }
        else if (address == 0x18010024u)
            ++state->frontend_complete;
        else if (address == 0x18010038u)
            ++state->frontend_blocked;
        else if (state->is_vortex && address == 0x180114a0u)
            vm_trace_vortex_input_parser(state);
        else if (!state->is_vortex && address == 0x18009090u)
            vm_trace_input_handler(state);
        else if (!state->is_vortex && address == 0x180093c8u)
            ++state->input_select_calls;
        else if (address == 0x1800ff1cu)
            ++state->input_transition_requests;
        else if (address == 0x180047a0u)
            ++state->audio_resource_callbacks;
        else if (address == 0x180162ecu)
            state->audio_trace |= 0x01;
        else if (address == 0x180163e8u)
            state->audio_trace |= 0x02;
        else if (address == 0x180164bcu)
            state->audio_trace |= 0x04;
        else if (address == 0x18016510u)
            state->audio_trace |= 0x08;
        else if (address == 0x18016548u)
            state->audio_trace |= 0x10;
        else if (address == 0x18015fa0u)
            state->audio_trace |= 0x20;
        else if (address == 0x1800192cu && !state->fatal_signal[4])
        {
            state->fatal_signal[0] = state->cpu.r[0];
            state->fatal_signal[1] = state->cpu.r[1];
            state->fatal_signal[2] = state->cpu.r[2];
            state->fatal_signal[3] = state->cpu.r[3];
            state->fatal_signal[4] = state->cpu.r[reg(&state->cpu, LR)];
        }
#endif
#ifdef SIMULATOR
        execute(&state->machine, &state->cpu);
#else
#ifdef USEC_TIMER
        unsigned long profile_start = USEC_TIMER;
#endif

        execute_until(&state->machine, &state->cpu, IG_VM_TRAP_BASE,
                      start + IG_VM_PHASE_LIMIT);
#ifdef USEC_TIMER
        state->profile_guest_us +=
            (unsigned long)(USEC_TIMER - profile_start);
#endif
#endif
    }
    *phase_instructions = state->machine.instructions - start;
    if (!state->machine.stopped)
    {
        state->machine.fault = 3;
        state->machine.fault_address = state->cpu.r[PC] - 8;
    }
    return false;
}

static bool vm_executable_path(const struct ig_game *game, char *path,
                               size_t path_size)
{
    char directory[MAX_PATH];
    char *separator;

    rb->strlcpy(directory, game->metadata_path, sizeof(directory));
    separator = rb->strrchr(directory, PATH_SEPCH);
    if (!separator)
        return false;
    *separator = '\0';
    return rb->snprintf(path, path_size, "%s/%s", directory,
                        game->executable_path) < (int)path_size;
}

static void vm_write_runtime_log(const struct ig_vm_state *state,
                                 enum ig_vm_result result)
{
    int fd;

    rb->mkdir(IG_ROOT_DIR);
    fd = rb->open(IG_ROOT_DIR "/ipodgames-runtime.log",
                  O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    rb->fdprintf(fd, "IPODGAMES-RUNTIME/1\n");
    rb->fdprintf(fd, "result=%s\n", ig_vm_result_name(result));
    rb->fdprintf(fd, "instructions=%lu\n", state->machine.instructions);
#ifdef SIMULATOR
    rb->fdprintf(fd,
                 "armemu_condition=%lu:%lu:%lu:%lu:%lu:%lu:%lu:%lu:"
                 "%lu:%lu:%lu:%lu:%lu:%lu:%lu:%lu\n",
                 state->machine.profile_condition[0],
                 state->machine.profile_condition[1],
                 state->machine.profile_condition[2],
                 state->machine.profile_condition[3],
                 state->machine.profile_condition[4],
                 state->machine.profile_condition[5],
                 state->machine.profile_condition[6],
                 state->machine.profile_condition[7],
                 state->machine.profile_condition[8],
                 state->machine.profile_condition[9],
                 state->machine.profile_condition[10],
                 state->machine.profile_condition[11],
                 state->machine.profile_condition[12],
                 state->machine.profile_condition[13],
                 state->machine.profile_condition[14],
                 state->machine.profile_condition[15]);
    rb->fdprintf(fd,
                 "armemu_top=%lu:%lu:%lu:%lu:%lu:%lu:%lu:%lu\n",
                 state->machine.profile_top[0],
                 state->machine.profile_top[1],
                 state->machine.profile_top[2],
                 state->machine.profile_top[3],
                 state->machine.profile_top[4],
                 state->machine.profile_top[5],
                 state->machine.profile_top[6],
                 state->machine.profile_top[7]);
    rb->fdprintf(fd,
                 "armemu_alu=%lu:%lu:%lu:%lu:%lu:%lu:%lu:%lu:"
                 "%lu:%lu:%lu:%lu:%lu:%lu:%lu:%lu\n",
                 state->machine.profile_alu[0],
                 state->machine.profile_alu[1],
                 state->machine.profile_alu[2],
                 state->machine.profile_alu[3],
                 state->machine.profile_alu[4],
                 state->machine.profile_alu[5],
                 state->machine.profile_alu[6],
                 state->machine.profile_alu[7],
                 state->machine.profile_alu[8],
                 state->machine.profile_alu[9],
                 state->machine.profile_alu[10],
                 state->machine.profile_alu[11],
                 state->machine.profile_alu[12],
                 state->machine.profile_alu[13],
                 state->machine.profile_alu[14],
                 state->machine.profile_alu[15]);
    rb->fdprintf(fd,
                 "armemu_alu_shape=%lu:%lu:%lu:%lu:%lu:%lu:%lu:%lu\n",
                 state->machine.profile_alu_shape[0],
                 state->machine.profile_alu_shape[1],
                 state->machine.profile_alu_shape[2],
                 state->machine.profile_alu_shape[3],
                 state->machine.profile_alu_shape[4],
                 state->machine.profile_alu_shape[5],
                 state->machine.profile_alu_shape[6],
                 state->machine.profile_alu_shape[7]);
    rb->fdprintf(fd,
                 "armemu_load_store=%lu:%lu:%lu:%lu:%lu:%lu:%lu:%lu\n",
                 state->machine.profile_load_store[0],
                 state->machine.profile_load_store[1],
                 state->machine.profile_load_store[2],
                 state->machine.profile_load_store[3],
                 state->machine.profile_load_store[4],
                 state->machine.profile_load_store[5],
                 state->machine.profile_load_store[6],
                 state->machine.profile_load_store[7]);
#endif
    rb->fdprintf(fd, "fault=%d:0x%08x:0x%08x\n",
                 state->machine.fault,
                 state->machine.fault_address,
                 state->machine.fault_instruction);
    rb->fdprintf(fd, "video=%lu:%lu\n",
                 state->draw_calls, state->presented_frames);
    rb->fdprintf(fd, "render_fast=%lu:%lu\n",
                 state->fast_texture_quads,
                 state->fast_texture_pixels);
    rb->fdprintf(fd, "render_work=%lu:%lu:%lu:%lu\n",
                 state->fast_opaque_pixels,
                 state->fast_blended_pixels,
                 state->fast_skipped_pixels,
                 state->general_raster_pixels);
    rb->fdprintf(fd, "render_reject=%lu:%lu:%lu:%lu\n",
                 state->fast_rejects[0], state->fast_rejects[1],
                 state->fast_rejects[2], state->fast_rejects[3]);
    rb->fdprintf(fd, "render_flipped=%lu:%lu\n",
                 state->flipped_texture_x,
                 state->flipped_texture_y);
    rb->fdprintf(fd, "transform=%lu:%lu:%lu\n",
                 state->transform_vertices,
                 state->transform_affine_vertices,
                 state->transform_perspective_vertices);
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    rb->fdprintf(fd, "cpu_frequency=%ld\n", *rb->cpu_frequency);
#endif
    rb->fdprintf(fd, "texture_subimages=%u:%lu:%u\n",
                 state->texture_subimages,
                 state->texture_subimage_bytes,
                 state->texture_subimage_failures);
    rb->fdprintf(fd, "audio=%u:%lu:%u:%lu:%u\n",
                 state->audio_play_starts,
                 state->audio_pcm_frames,
                 state->audio_data_reads,
                 state->audio_data_bytes,
                 state->audio_max_voices);
    rb->fdprintf(fd, "save=%u:%lu:%u:%lu:%u:%s\n",
                 state->save_reads,
                 state->save_read_bytes,
                 state->save_writes,
                 state->save_bytes,
                 state->save_failures,
                 state->last_save_file);
    rb->fdprintf(fd, "save_exit=%u\n",
                 state->save_exit_requested ? 1 : 0);
    rb->fdprintf(fd, "input_polls=%u\n", state->input_polls);
    rb->fdprintf(fd, "pacing=%lu:%lu:%lu:%lu:%lu:%lu:%lu\n",
                 state->runtime_elapsed_ticks,
                 state->presented_frames,
                 state->frame_exec_ticks,
                 state->frame_exec_max_ticks,
                 state->late_frames,
                 state->max_late_ticks,
                 state->deadline_rebases);
    rb->fdprintf(fd, "profile_us=%lu:%lu:%lu:%lu\n",
                 state->profile_guest_us,
                 state->profile_framework_us,
                 state->profile_draw_us,
                 state->profile_lcd_us);
    {
        unsigned int mode;

        for (mode = 0; mode < IG_GLES_MODES; ++mode)
            if (state->profile_draw_mode_calls[mode])
                rb->fdprintf(fd, "profile_draw_mode_%u=%lu:%lu\n",
                             mode,
                             state->profile_draw_mode_calls[mode],
                             state->profile_draw_mode_us[mode]);
    }
    rb->fdprintf(fd, "system=%u:%u:%u:%u:%u\n",
                 state->system_settings_reads,
                 state->system_clock_reads,
                 state->system_battery_reads,
                 state->system_brightness_gets,
                 state->system_brightness_sets);
    rb->fdprintf(fd, "setting=%u:%s:%u\n",
                 state->system_settings_successes,
                 state->system_last_setting,
                 state->system_last_setting_capacity);
    rb->close(fd);
}

void ig_vm_lifecycle_test(const struct ig_game *game, unsigned int frames,
                          struct ig_vm_report *report)
{
    struct ig_eapp_probe probe;
    char path[MAX_PATH];
    size_t audio_size = 0;
    unsigned char *audio_buffer;
    char *separator;
    unsigned int frame;
    unsigned long phase_instructions = 0;
    int fd = -1;

    rb->memset(report, 0, sizeof(*report));
#ifdef SIMULATOR
    report->armemu_signed_byte_pass = vm_armemu_signed_byte_probe();
    if (!report->armemu_signed_byte_pass)
    {
        report->result = IG_VM_CPU_FAULT;
        return;
    }
#endif
    ig_runtime_probe(game, &probe);
    if (probe.result != IG_PROBE_OK ||
        probe.inferred_load_base != IG_VM_BASE ||
        probe.file_size > IG_VM_IMAGE_LIMIT ||
        !vm_executable_path(game, path, sizeof(path)))
    {
        report->result = IG_VM_UNSUPPORTED_IMAGE;
        return;
    }

    audio_buffer = rb->plugin_get_audio_buffer(&audio_size);
    report->available_memory = audio_size;
    if (!audio_buffer || audio_size < IG_VM_MEMORY_SIZE +
                                     IG_VM_FRAMEBUFFER_BYTES)
    {
        report->result = IG_VM_NO_MEMORY;
        if (audio_buffer)
            rb->plugin_release_audio_buffer();
        return;
    }

    rb->memset(&vm, 0, sizeof(vm));
    rb->memset(vm.gles_uniform_color, 0xff,
               sizeof(vm.gles_uniform_color));
    rb->memset(audio_buffer, 0,
               IG_VM_MEMORY_SIZE + IG_VM_FRAMEBUFFER_BYTES);
    vm.memory = audio_buffer;
    vm.memory_size = IG_VM_MEMORY_SIZE;
    vm.framebuffer = (fb_data *)(audio_buffer + IG_VM_MEMORY_SIZE);
    vm.texture_storage = audio_buffer + IG_VM_MEMORY_SIZE +
                         IG_VM_FRAMEBUFFER_BYTES;
    vm.texture_storage_size = audio_size - IG_VM_MEMORY_SIZE -
                              IG_VM_FRAMEBUFFER_BYTES;
    vm.heap_next = IG_VM_HEAP_BASE;
    vm.scripted_input = true;
    vm.is_vortex = !rb->strcmp(game->guid, "12345");
#ifdef SIMULATOR
    vm.reference_raster = getenv("IPODGAMES_SIM_REFERENCE_RASTER") != NULL;
    vm.hardware_exec = getenv("IPODGAMES_SIM_HARDWARE_EXEC") != NULL;
#endif
    vm_system_init(&vm);
    rb->strlcpy(vm.game_directory, path, sizeof(vm.game_directory));
    separator = rb->strrchr(vm.game_directory, PATH_SEPCH);
    if (separator)
        *separator = '\0';
    rb->snprintf(vm.save_directory, sizeof(vm.save_directory),
                 IG_ROOT_DIR "/saves/%s", game->guid);
    vm_save_init(&vm);
    vm.machine.cpu = &vm.cpu;
    vm.machine.drambase = IG_VM_BASE;
    vm.machine.dramsize = IG_VM_MEMORY_SIZE;
    vm.machine.dram = vm.memory;
    vm.machine.swi = vm_swi;
    reset(&vm.cpu);
    active_vm = &vm;
    vm_audio_init(&vm);

    fd = rb->open(path, O_RDONLY);
    if (fd < 0 || rb->read(fd, vm.memory, probe.file_size) !=
                     (ssize_t)probe.file_size)
    {
        report->result = IG_VM_IO_ERROR;
        goto cleanup;
    }
    rb->close(fd);
    fd = -1;
    if (!vm_patch_imports(&vm, &probe))
    {
        report->result = IG_VM_BAD_IMPORTS;
        goto cleanup;
    }
    report->import_count = vm.import_count;

    if (!vm_run_entry(&vm, probe.header_word_14, false,
                      &phase_instructions))
    {
        report->result = IG_VM_CPU_FAULT;
        goto cleanup;
    }
    for (frame = 0; frame < frames; ++frame)
    {
        if (!vm_run_entry(&vm, probe.header_word_24, true,
                          &phase_instructions))
        {
            report->result = IG_VM_CPU_FAULT;
            goto cleanup;
        }
#ifdef SIMULATOR
        vm_dump_menu_sequence_frame(&vm);
        vm_dump_motion_continuity_frame(&vm);
#endif
        ++report->completed_frames;
        if (vm_save_exit_complete(&vm))
        {
            vm.save_exit_requested = true;
            break;
        }
    }
#ifdef SIMULATOR
    if (!getenv("IPODGAMES_TEST_GENERIC") &&
        getenv("IPODGAMES_TEST_WHEEL_SWEEP") &&
        !vm_wheel_sweep_probe(&vm, &phase_instructions, report))
    {
        report->result = IG_VM_CPU_FAULT;
        goto cleanup;
    }
    if (!getenv("IPODGAMES_TEST_GENERIC") &&
        (!vm_save_probe(&vm) || !vm_official_save_probe(&vm)))
    {
        report->result = IG_VM_IO_ERROR;
        goto cleanup;
    }
    if (!getenv("IPODGAMES_TEST_GENERIC") &&
        !vm_audio_multivoice_probe(&vm))
    {
        report->result = IG_VM_CPU_FAULT;
        goto cleanup;
    }
#endif
    report->result = IG_VM_OK;

cleanup:
    if (fd >= 0)
        rb->close(fd);
    vm_audio_shutdown(&vm);
    vm_save_shutdown(&vm);
    vm_system_shutdown(&vm);
    vm_write_runtime_log(&vm, report->result);
    report->instructions = vm.machine.instructions;
    report->framework_calls = vm.framework_calls;
    report->fault = vm.machine.fault;
    report->fault_address = vm.machine.fault_address;
    report->fault_instruction = vm.machine.fault_instruction;
    report->last_framework_id = vm.last_framework_id;
    report->last_framework_ordinal = vm.last_framework_ordinal;
    report->last_framework_lr = vm.last_framework_lr;
    report->last_framework_r0 = vm.last_framework_r0;
    report->draw_calls = vm.draw_calls;
    report->presented_frames = vm.presented_frames;
#ifdef SIMULATOR
    report->draw_history_count = vm.draw_history_count;
    rb->memcpy(report->draw_rects, vm.draw_rects,
               sizeof(report->draw_rects));
    rb->memcpy(report->draw_sources, vm.draw_sources,
               sizeof(report->draw_sources));
    rb->memcpy(report->draw_texture_ids, vm.draw_texture_ids,
               sizeof(report->draw_texture_ids));
#endif
    report->system_settings_reads = vm.system_settings_reads;
    report->system_clock_reads = vm.system_clock_reads;
    report->system_battery_reads = vm.system_battery_reads;
    report->system_brightness_gets = vm.system_brightness_gets;
    report->system_brightness_sets = vm.system_brightness_sets;
    rb->memcpy(report->gles_ordinal_counts, vm.gles_ordinal_counts,
               sizeof(report->gles_ordinal_counts));
    rb->memcpy(report->gles_last_args, vm.gles_last_args,
               sizeof(report->gles_last_args));
    rb->memcpy(report->gles_mode_draws, vm.gles_mode_draws,
               sizeof(report->gles_mode_draws));
    rb->memcpy(report->gles_mode_alpha, vm.gles_mode_alpha,
               sizeof(report->gles_mode_alpha));
    rb->memcpy(report->gles_uniform_color, vm.gles_uniform_color,
               sizeof(report->gles_uniform_color));
#ifdef SIMULATOR
    report->gles_texture_pair_count = vm.gles_texture_pair_count;
    rb->memcpy(report->gles_texture_pairs, vm.gles_texture_pairs,
               sizeof(report->gles_texture_pairs));
#endif
    report->texture_subimages = vm.texture_subimages;
    report->texture_subimage_failures = vm.texture_subimage_failures;
    report->texture_subimage_bytes = vm.texture_subimage_bytes;
    rb->memcpy(report->texture_subimage_last, vm.texture_subimage_last,
               sizeof(report->texture_subimage_last));
    report->texture_count = vm.texture_count;
    for (unsigned int texture = 0; texture < vm.texture_count; ++texture)
    {
        report->texture_ids[texture] = vm.textures[texture].id;
        report->texture_widths[texture] = vm.textures[texture].width;
        report->texture_heights[texture] = vm.textures[texture].height;
        report->texture_formats[texture] = vm.textures[texture].format;
        report->texture_types[texture] = vm.textures[texture].type;
        report->texture_draws[texture] = vm.textures[texture].draws;
        rb->memcpy(report->texture_alpha[texture], vm.textures[texture].alpha,
                   sizeof(report->texture_alpha[texture]));
        rb->strlcpy(report->texture_names[texture], vm.textures[texture].name,
                    sizeof(report->texture_names[texture]));
    }
    rb->strlcpy(report->last_async_file, vm.last_async_file,
                sizeof(report->last_async_file));
    report->last_async_destination = vm.last_async_destination;
    report->last_async_capacity = vm.last_async_capacity;
    report->last_async_bytes = vm.last_async_bytes;
    report->last_async_context = vm.last_async_context;
    report->last_async_callback = vm.last_async_callback;
    report->last_async_inner_callback = vm.last_async_inner_callback;
    report->last_async_inner_context = vm.last_async_inner_context;
    report->last_async_manager_callback = vm.last_async_manager_callback;
    report->last_async_manager_context = vm.last_async_manager_context;
    report->audio_resource_callbacks = vm.audio_resource_callbacks;
    report->audio_trace = vm.audio_trace;
    report->audio_data_reads = vm.audio_data_reads;
    report->audio_data_bytes = vm.audio_data_bytes;
    report->audio_pending_callback = vm.pending_callback;
    report->audio_pending_context = vm.pending_audio_context;
    report->audio_loader_busy = vm.memory[0x180c7650u - IG_VM_BASE];
    report->audio_play_starts = vm.audio_play_starts;
    report->audio_max_voices = vm.audio_max_voices;
    report->audio_multivoice_probe_pass =
        vm.audio_multivoice_probe_pass;
    report->audio_pcm_frames = vm.audio_pcm_frames;
    rb->memcpy(report->audio_ordinal_counts, vm.audio_ordinal_counts,
               sizeof(report->audio_ordinal_counts));
    rb->memcpy(report->audio_last_args, vm.audio_last_args,
               sizeof(report->audio_last_args));
    vm_guest_u32(&vm, vm.last_async_context + 4,
                 &report->last_async_busy);
    vm_guest_u32(&vm, vm.last_async_context + 12,
                 &report->last_async_completion);
    report->async_reads = vm.async_reads;
    report->async_opens = vm.async_opens;
    rb->memcpy(report->async_ordinal_counts, vm.async_ordinal_counts,
               sizeof(report->async_ordinal_counts));
    rb->memcpy(report->async_last_args, vm.async_last_args,
               sizeof(report->async_last_args));
    rb->memcpy(report->metadata_ordinal_counts,
               vm.metadata_ordinal_counts,
               sizeof(report->metadata_ordinal_counts));
    rb->memcpy(report->metadata_last_args, vm.metadata_last_args,
               sizeof(report->metadata_last_args));
    report->save_writes = vm.save_writes;
    report->save_bytes = vm.save_bytes;
    report->save_failures = vm.save_failures;
    report->save_probe_pass = vm.save_probe_pass;
    report->save_exit_requested = vm.save_exit_requested;
    report->game_state = vm.memory[IG_VM_SCRATCH_BASE - IG_VM_BASE];
    report->wait_flag = vm.memory[0x180c761du - IG_VM_BASE];
    report->input_polls = vm.input_polls;
    report->synthetic_input_frame = vm.synthetic_input_frame;
    report->frontend_updates = vm.frontend_updates;
    report->frontend_complete = vm.frontend_complete;
    report->frontend_blocked = vm.frontend_blocked;
    report->frontend_mode = vm.frontend_mode;
    report->input_handler_calls = vm.input_handler_calls;
    rb->memcpy(report->input_handler_type_counts,
               vm.input_handler_type_counts,
               sizeof(report->input_handler_type_counts));
    rb->memcpy(report->input_handler_last, vm.input_handler_last,
               sizeof(report->input_handler_last));
    report->input_wheel_handler_total = vm.input_wheel_handler_total;
    report->input_wheel_handler_last = vm.input_wheel_handler_last;
    report->input_wheel_handler_object = vm.input_wheel_handler_object;
    report->input_wheel_handler_selection = vm.input_wheel_handler_selection;
    report->input_wheel_handler_accumulator =
        vm.input_wheel_handler_accumulator;
    report->input_select_calls = vm.input_select_calls;
    report->input_transition_requests = vm.input_transition_requests;
    if (vm_guest_u32(&vm, 0x18047280u, &report->game_object) &&
        report->game_object)
    {
        u32 vtable;

        vm_guest_u32(&vm, report->game_object + 0x1f18,
                     &report->game_flags);
        vm_guest_u32(&vm, report->game_object + 0x1f98,
                     &report->game_countdown);
        vm_guest_u32(&vm, report->game_object + 0x1f38,
                     &report->game_input_active);
        vm_guest_u32(&vm, report->game_object + 0x1f34,
                     &report->game_input_pressed);
        vm_guest_u32(&vm, report->game_object + 0x1f3c,
                     &report->game_input_released);
        vm_guest_u32(&vm, report->game_object + 0x1f54,
                     &report->game_machine_stage);
        vm_guest_u32(&vm, report->game_object + 0x1f58,
                     &report->game_machine_stage_end);
        vm_guest_u32(&vm, report->game_object + 0x1f5c,
                     &report->game_machine_first_split);
        vm_guest_u32(&vm, report->game_object + 0x1f60,
                     &report->game_machine_second_split);
        vm_guest_u32(&vm, report->game_object + 0x1f24,
                     &report->game_frame_delta);
        if (vm_guest_u32(&vm, report->game_object + 0x40,
                         &report->component_input) &&
            vm_guest_u32(&vm, report->component_input, &vtable))
        {
            vm_guest_u32(&vm, vtable + 8, &report->component_input_method);
            vm_guest_u32(&vm, report->component_input + 16,
                         &report->component_state);
            vm_guest_u32(&vm, report->component_input + 24,
                         &report->component_next_state);
            vm_guest_u32(&vm, report->component_input + 0x5f0,
                         &report->component_flags);
            vm_guest_u32(&vm, report->component_input + 40,
                         &report->component_timer);
            vm_guest_u32(&vm, report->component_input + 44,
                         &report->component_toggle);
            vm_guest_u32(&vm, report->component_input + 28,
                         &report->component_interaction);
            vm_guest_u32(&vm, report->component_input + 32,
                         &report->component_selection);
            vm_guest_u32(&vm, report->component_input + 72,
                         &report->component_selection_count);
            vm_guest_u32(&vm, report->component_input + 0x388,
                         &report->component_name_length);
            report->component_name_cursor =
                vm.memory[report->component_input + 0x390 - IG_VM_BASE];
            vm_guest_u32(&vm, report->component_input + 0x368,
                         &report->component_name_backspace);
            vm_guest_u32(&vm, report->component_input + 0x36c,
                         &report->component_name_confirm);
            vm_guest_u32(&vm, report->component_input + 0x370,
                         &report->component_name_base);
        }
        if (vm_guest_u32(&vm, report->game_object + 0x34,
                         &report->component_loader) &&
            vm_guest_u32(&vm, report->component_loader, &vtable))
        {
            vm_guest_u32(&vm, vtable, &report->component_loader_method);
        }
        if (vm_guest_u32(&vm, report->game_object + 0x3c,
                         &report->component_scene) &&
            vm_guest_u32(&vm, report->component_scene, &vtable))
            vm_guest_u32(&vm, vtable, &report->component_scene_method);
        vm_guest_u32(&vm, report->game_object + 0x38,
                     &report->component_map);
        if (report->component_map)
        {
            u32 record;

            vm_guest_u32(&vm, report->component_map,
                         &report->loader_audio_type);
            vm_guest_u32(&vm, report->component_map + 404,
                         &report->loader_audio_index);
            record = report->component_map +
                     report->loader_audio_index * 20;
            vm_guest_u32(&vm, record + 16,
                         &report->loader_audio_slot);
            if (record + 21 >= IG_VM_BASE &&
                record + 22 < IG_VM_BASE + vm.memory_size)
            {
                report->loader_audio_pending =
                    vm.memory[record + 21 - IG_VM_BASE];
                report->loader_audio_status =
                    vm.memory[record + 22 - IG_VM_BASE];
            }
        }
    }
    report->recent_framework_count = vm.call_history_count;
    if (vm.call_history_count)
    {
        unsigned int history;
        unsigned int first = (vm.call_history_next +
            ARRAYLEN(vm.call_history) - vm.call_history_count) %
            ARRAYLEN(vm.call_history);

        for (history = 0; history < vm.call_history_count; ++history)
            report->recent_framework[history] =
                vm.call_history[(first + history) %
                                ARRAYLEN(vm.call_history)];
    }
#ifdef SIMULATOR
    report->recent_guest_count = vm.guest_history_count;
    rb->memcpy(report->fatal_signal, vm.fatal_signal,
               sizeof(report->fatal_signal));
    if (vm.guest_history_count)
    {
        unsigned int history;
        unsigned int first = (vm.guest_history_next + IG_VM_GUEST_HISTORY -
                              vm.guest_history_count) % IG_VM_GUEST_HISTORY;

        for (history = 0; history < vm.guest_history_count; ++history)
        {
            unsigned int source = (first + history) % IG_VM_GUEST_HISTORY;

            report->recent_guest_pc[history] = vm.guest_history_pc[source];
            report->recent_guest_r4[history] = vm.guest_history_r4[source];
            report->recent_guest_r5[history] = vm.guest_history_r5[source];
            report->recent_guest_sp[history] = vm.guest_history_sp[source];
        }
    }
#endif
    if (vm.framebuffer)
    {
        size_t index;
        const unsigned char *pixels = (const unsigned char *)vm.framebuffer;
        unsigned long checksum = 2166136261u;
        unsigned long nonblack = 0;

        for (index = 0; index < IG_VM_FRAMEBUFFER_BYTES; ++index)
            checksum = (checksum ^ pixels[index]) * 16777619u;
        for (index = 0; index < (size_t)LCD_WIDTH * LCD_HEIGHT; ++index)
            if (vm.framebuffer[index] != LCD_RGBPACK(0, 0, 0))
                ++nonblack;
        report->framebuffer_checksum = checksum;
        report->nonblack_pixels = nonblack;
    }
#ifdef SIMULATOR
    vm_dump_frame(&vm);
    vm_dump_vortex_state(&vm);
    {
        static const unsigned char texture_ids[] = {
            1, 3, 5, 6, 8, 11, 12, 15, 17, 21, 27, 29, 30, 31, 36, 48,
            49, 54, 55, 56, 57
        };

        for (unsigned int texture = 0;
             texture < ARRAYLEN(texture_ids); ++texture)
            vm_dump_vortex_texture(&vm, texture_ids[texture]);
    }
#endif
    active_vm = NULL;
    rb->plugin_release_audio_buffer();
}

enum ig_vm_result ig_vm_run_game(const struct ig_game *game)
{
    struct ig_eapp_probe probe;
    char path[MAX_PATH];
    char *separator;
    size_t audio_size = 0;
    unsigned char *audio_buffer;
    unsigned long phase_instructions = 0;
    enum ig_vm_result result = IG_VM_OK;
    int fd = -1;
#ifdef HAVE_WHEEL_POSITION
    int last_retail_wheel = -1;
#endif
    unsigned int frame_fraction = 0;
    unsigned long next_runtime_log_frame = 180;
    long frame_deadline;
    long runtime_start;
    bool cpu_boosted = false;
    bool running = true;

    ig_runtime_probe(game, &probe);
    if (probe.result != IG_PROBE_OK ||
        probe.inferred_load_base != IG_VM_BASE ||
        probe.file_size > IG_VM_IMAGE_LIMIT ||
        !vm_executable_path(game, path, sizeof(path)))
        return IG_VM_UNSUPPORTED_IMAGE;

    /* The VM and decoded textures occupy the shared plugin buffer.  Core owns
     * the playback-stop/ownership transition performed by this call; do not
     * pre-stop audio here (see plugin-audio-lifecycle-steering.md).
     */
    audio_buffer = rb->plugin_get_audio_buffer(&audio_size);
    if (!audio_buffer || audio_size < IG_VM_MEMORY_SIZE +
                                     IG_VM_FRAMEBUFFER_BYTES)
    {
        if (audio_buffer)
            rb->plugin_release_audio_buffer();
        return IG_VM_NO_MEMORY;
    }

    rb->memset(&vm, 0, sizeof(vm));
    rb->memset(vm.gles_uniform_color, 0xff,
               sizeof(vm.gles_uniform_color));
    rb->memset(audio_buffer, 0,
               IG_VM_MEMORY_SIZE + IG_VM_FRAMEBUFFER_BYTES);
    vm.memory = audio_buffer;
    vm.memory_size = IG_VM_MEMORY_SIZE;
    vm.framebuffer = (fb_data *)(audio_buffer + IG_VM_MEMORY_SIZE);
    vm.texture_storage = audio_buffer + IG_VM_MEMORY_SIZE +
                         IG_VM_FRAMEBUFFER_BYTES;
    vm.texture_storage_size = audio_size - IG_VM_MEMORY_SIZE -
                              IG_VM_FRAMEBUFFER_BYTES;
    vm.heap_next = IG_VM_HEAP_BASE;
    vm.wheel_raw = 0x30;
    vm.is_vortex = !rb->strcmp(game->guid, "12345");
    vm.realtime_clock = true;
#ifdef SIMULATOR
    vm.reference_raster = getenv("IPODGAMES_SIM_REFERENCE_RASTER") != NULL;
    vm.hardware_exec = getenv("IPODGAMES_SIM_HARDWARE_EXEC") != NULL;
#endif
    vm_system_init(&vm);
    rb->strlcpy(vm.game_directory, path, sizeof(vm.game_directory));
    separator = rb->strrchr(vm.game_directory, PATH_SEPCH);
    if (separator)
        *separator = '\0';
    rb->snprintf(vm.save_directory, sizeof(vm.save_directory),
                 IG_ROOT_DIR "/saves/%s", game->guid);
    vm_save_init(&vm);
    vm.machine.cpu = &vm.cpu;
    vm.machine.drambase = IG_VM_BASE;
    vm.machine.dramsize = IG_VM_MEMORY_SIZE;
    vm.machine.dram = vm.memory;
    vm.machine.swi = vm_swi;
    reset(&vm.cpu);
    active_vm = &vm;
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    rb->cpu_boost(true);
    cpu_boosted = true;
#endif
    vm_audio_init(&vm);

    fd = rb->open(path, O_RDONLY);
    if (fd < 0 || rb->read(fd, vm.memory, probe.file_size) !=
                     (ssize_t)probe.file_size)
    {
        result = IG_VM_IO_ERROR;
        goto cleanup;
    }
    rb->close(fd);
    fd = -1;
    if (!vm_patch_imports(&vm, &probe))
    {
        result = IG_VM_BAD_IMPORTS;
        goto cleanup;
    }
    if (!vm_run_entry(&vm, probe.header_word_14, false,
                      &phase_instructions))
    {
        result = IG_VM_CPU_FAULT;
        goto cleanup;
    }

#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(false);
#endif
    frame_deadline = *rb->current_tick;
    runtime_start = frame_deadline;
    while (running)
    {
        long frame_start;
        unsigned long frame_ticks;
        int button = rb->button_get_w_tmo(0);

        if (button == SYS_USB_CONNECTED)
            break;
        if (button)
        {
            int base = button & ~(BUTTON_REPEAT | BUTTON_REL);
            unsigned int phase = (button & BUTTON_REL) ? 1 : 2;
            unsigned int type = 0;

            if (base == BUTTON_MENU && (button & BUTTON_REPEAT))
            {
                running = false;
                continue;
            }
            if (!(button & BUTTON_REPEAT))
            {
                if (base == BUTTON_SELECT)
                    type = 2;
                else if (base == BUTTON_MENU)
                    type = 1;
                if (type && vm.is_vortex)
                {
                    vm.button_event_type = type;
                    vm.button_event_phase = phase;
                }
                else if (type && !vm_call_guest(&vm, 0x18002c9cu,
                                                type, phase, 0, 0))
                {
                    result = IG_VM_CPU_FAULT;
                    break;
                }
            }
        }

#ifdef HAVE_WHEEL_POSITION
        {
            int wheel = rb->wheel_status();

            if (wheel >= 0)
            {
                int retail_wheel = vm_retail_wheel_position(wheel);
                int delta = last_retail_wheel < 0 ? 0 :
                    retail_wheel - last_retail_wheel;

                if (delta > 128)
                    delta -= 256;
                else if (delta < -128)
                    delta += 256;
                vm.wheel_raw = 0x40000000u |
                    (retail_wheel & 0xff);
                vm.wheel_delta = (u32)delta;
                last_retail_wheel = retail_wheel;
            }
            else
            {
                vm.wheel_raw = 0x30;
                vm.wheel_delta = 0;
                last_retail_wheel = -1;
            }
        }
#else
        vm.wheel_raw = 0x30;
        vm.wheel_delta = 0;
#endif
        frame_start = *rb->current_tick;
        if (!vm_run_entry(&vm, probe.header_word_24, true,
                          &phase_instructions))
        {
            result = IG_VM_CPU_FAULT;
            break;
        }
        if (vm_save_exit_complete(&vm))
        {
            vm.save_exit_requested = true;
            running = false;
        }
        frame_ticks = (unsigned long)(*rb->current_tick - frame_start);
        vm.frame_exec_ticks += frame_ticks;
        if (frame_ticks > vm.frame_exec_max_ticks)
            vm.frame_exec_max_ticks = frame_ticks;
        if (vm.presented_frames >= next_runtime_log_frame)
        {
            vm.runtime_elapsed_ticks =
                (unsigned long)(*rb->current_tick - runtime_start);
            vm_write_runtime_log(&vm, result);
            do
                next_runtime_log_frame += 180;
            while (vm.presented_frames >= next_runtime_log_frame);
        }
        frame_fraction += HZ;
        frame_deadline += frame_fraction / 60;
        frame_fraction %= 60;
        if (TIME_BEFORE(*rb->current_tick, frame_deadline))
            rb->sleep(frame_deadline - *rb->current_tick);
        else
        {
            unsigned long late =
                (unsigned long)(*rb->current_tick - frame_deadline);

            ++vm.late_frames;
            if (late > vm.max_late_ticks)
                vm.max_late_ticks = late;
            if (late > HZ / 2)
            {
                frame_deadline = *rb->current_tick;
                ++vm.deadline_rebases;
            }
        }
        rb->yield();
    }
    vm.runtime_elapsed_ticks =
        (unsigned long)(*rb->current_tick - runtime_start);

cleanup:
    if (fd >= 0)
        rb->close(fd);
    vm_audio_shutdown(&vm);
    vm_save_shutdown(&vm);
    vm_system_shutdown(&vm);
    vm_write_runtime_log(&vm, result);
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    if (cpu_boosted)
        rb->cpu_boost(false);
#else
    (void)cpu_boosted;
#endif
    active_vm = NULL;
    rb->plugin_release_audio_buffer();
    return result;
}
