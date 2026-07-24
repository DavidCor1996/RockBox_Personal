#ifndef IPODGAMES_H
#define IPODGAMES_H

#include "plugin.h"

#define IG_ROOT_DIR ROCKBOX_DIR "/ipodgames"
#define IG_GAMES_DIR IG_ROOT_DIR "/games"
#define IG_METADATA_FILE "game.igame"

#define IG_MAX_GAMES 64
#define IG_NAME_SIZE 64
#define IG_GUID_SIZE 32
#define IG_VERSION_SIZE 24
#define IG_STATE_SIZE 32
#define IG_HASH_SIZE 65
#define IG_AUDIO_ORDINALS 61
#define IG_METADATA_ORDINALS 152
#define IG_GLES_ORDINALS 179
#define IG_GLES_MODES 50
#define IG_VM_MAX_TEXTURES 96
#define IG_VM_GUEST_HISTORY 32
#define IG_VM_DRAW_HISTORY 16
#define IG_VM_TEXTURE_PAIR_HISTORY 128

struct ig_game
{
    char name[IG_NAME_SIZE];
    char guid[IG_GUID_SIZE];
    char version[IG_VERSION_SIZE];
    char executable_state[IG_STATE_SIZE];
    char executable_sha256[IG_HASH_SIZE];
    char metadata_path[MAX_PATH];
    char executable_path[MAX_PATH];
    unsigned long build_id;
    unsigned long executable_size;
    int platform_id;
    bool metadata_valid;
};

struct ig_catalog
{
    struct ig_game games[IG_MAX_GAMES];
    int count;
    int invalid_count;
};

enum ig_probe_result
{
    IG_PROBE_OK = 0,
    IG_PROBE_NO_EXECUTABLE,
    IG_PROBE_OPEN_FAILED,
    IG_PROBE_SHORT_HEADER,
    IG_PROBE_BAD_MAGIC,
    IG_PROBE_BAD_POINTER,
    IG_PROBE_BAD_FRAMEWORK,
};

struct ig_eapp_probe
{
    enum ig_probe_result result;
    unsigned long file_size;
    unsigned long first_record_offset;
    unsigned long first_framework_pointer;
    unsigned long inferred_load_base;
    unsigned long header_word_14;
    unsigned long header_word_18;
    unsigned long header_word_24;
    unsigned long total_imports;
    unsigned int framework_count;
};

enum ig_vm_result
{
    IG_VM_OK = 0,
    IG_VM_UNSUPPORTED_IMAGE,
    IG_VM_NO_MEMORY,
    IG_VM_IO_ERROR,
    IG_VM_BAD_IMPORTS,
    IG_VM_CPU_FAULT,
};

struct ig_vm_report
{
    enum ig_vm_result result;
    size_t available_memory;
    unsigned long instructions;
    unsigned long framework_calls;
    unsigned long fault_address;
    unsigned long fault_instruction;
    unsigned long last_framework_lr;
    unsigned long last_framework_r0;
    unsigned long framebuffer_checksum;
    unsigned long nonblack_pixels;
    unsigned long draw_calls;
    unsigned long presented_frames;
    int draw_rects[IG_VM_DRAW_HISTORY][4];
    int draw_sources[IG_VM_DRAW_HISTORY][4];
    unsigned int draw_texture_ids[IG_VM_DRAW_HISTORY];
    unsigned int draw_history_count;
    unsigned int gles_ordinal_counts[IG_GLES_ORDINALS];
    unsigned int gles_last_args[IG_GLES_ORDINALS][4];
    unsigned int gles_mode_draws[IG_GLES_MODES];
    unsigned long gles_mode_alpha[IG_GLES_MODES][3];
    unsigned char gles_uniform_color[IG_GLES_MODES][4];
    unsigned int gles_texture_pairs[IG_VM_TEXTURE_PAIR_HISTORY][4];
    unsigned int gles_texture_pair_count;
    unsigned int texture_subimages;
    unsigned int texture_subimage_failures;
    unsigned long texture_subimage_bytes;
    unsigned int texture_subimage_last[7];
    unsigned int texture_count;
    unsigned int texture_ids[IG_VM_MAX_TEXTURES];
    unsigned int texture_widths[IG_VM_MAX_TEXTURES];
    unsigned int texture_heights[IG_VM_MAX_TEXTURES];
    unsigned int texture_formats[IG_VM_MAX_TEXTURES];
    unsigned int texture_types[IG_VM_MAX_TEXTURES];
    unsigned long texture_draws[IG_VM_MAX_TEXTURES];
    unsigned long texture_alpha[IG_VM_MAX_TEXTURES][3];
    char texture_names[IG_VM_MAX_TEXTURES][64];
    unsigned int last_framework_ordinal;
    unsigned int last_framework_id;
    unsigned int import_count;
    unsigned int completed_frames;
    unsigned int recent_framework[16];
    unsigned int recent_framework_count;
    unsigned int recent_guest_pc[IG_VM_GUEST_HISTORY];
    unsigned int recent_guest_r4[IG_VM_GUEST_HISTORY];
    unsigned int recent_guest_r5[IG_VM_GUEST_HISTORY];
    unsigned int recent_guest_sp[IG_VM_GUEST_HISTORY];
    unsigned int recent_guest_count;
    unsigned int fatal_signal[5];
    char last_async_file[64];
    unsigned long last_async_destination;
    unsigned long last_async_capacity;
    unsigned long last_async_bytes;
    unsigned int last_async_context;
    unsigned int last_async_callback;
    unsigned int last_async_inner_callback;
    unsigned int last_async_inner_context;
    unsigned int last_async_manager_callback;
    unsigned int last_async_manager_context;
    unsigned int last_async_busy;
    unsigned int last_async_completion;
    unsigned int audio_resource_callbacks;
    unsigned int audio_trace;
    unsigned int audio_data_reads;
    unsigned long audio_data_bytes;
    unsigned int audio_pending_callback;
    unsigned int audio_pending_context;
    unsigned int audio_loader_busy;
    unsigned int audio_play_starts;
    unsigned int audio_max_voices;
    unsigned int audio_multivoice_probe_pass;
    unsigned long audio_pcm_frames;
    unsigned int audio_ordinal_counts[IG_AUDIO_ORDINALS];
    unsigned int audio_last_args[IG_AUDIO_ORDINALS][4];
    unsigned int async_reads;
    unsigned int async_opens;
    unsigned int async_ordinal_counts[17];
    unsigned int async_last_args[17][4];
    unsigned int metadata_ordinal_counts[IG_METADATA_ORDINALS];
    unsigned int metadata_last_args[IG_METADATA_ORDINALS][4];
    unsigned int save_writes;
    unsigned long save_bytes;
    unsigned int save_failures;
    unsigned int save_probe_pass;
    unsigned int save_exit_requested;
    unsigned int game_state;
    unsigned int wait_flag;
    unsigned int input_polls;
    unsigned int synthetic_input_frame;
    unsigned int game_object;
    unsigned int game_flags;
    unsigned int game_countdown;
    unsigned int game_input_active;
    unsigned int game_input_pressed;
    unsigned int game_input_released;
    unsigned int frontend_updates;
    unsigned int frontend_complete;
    unsigned int frontend_blocked;
    unsigned int frontend_mode;
    unsigned int component_input;
    unsigned int component_input_method;
    unsigned int component_loader;
    unsigned int component_loader_method;
    unsigned int loader_audio_type;
    unsigned int loader_audio_index;
    unsigned int loader_audio_slot;
    unsigned int loader_audio_pending;
    unsigned int loader_audio_status;
    unsigned int component_scene;
    unsigned int component_scene_method;
    unsigned int component_map;
    unsigned int game_machine_stage;
    unsigned int game_machine_stage_end;
    unsigned int game_machine_first_split;
    unsigned int game_machine_second_split;
    unsigned int game_frame_delta;
    unsigned int component_state;
    unsigned int component_next_state;
    unsigned int component_flags;
    unsigned int component_timer;
    unsigned int component_toggle;
    unsigned int component_interaction;
    unsigned int component_selection;
    unsigned int component_selection_count;
    unsigned int component_name_length;
    unsigned int component_name_cursor;
    unsigned int component_name_backspace;
    unsigned int component_name_confirm;
    unsigned int component_name_base;
    unsigned int input_handler_calls;
    unsigned int input_handler_type_counts[8];
    unsigned int input_handler_last[5];
    int input_wheel_handler_total;
    int input_wheel_handler_last;
    unsigned int input_wheel_handler_object;
    unsigned int input_wheel_handler_selection;
    unsigned int input_wheel_handler_accumulator;
    unsigned int input_select_calls;
    unsigned int input_transition_requests;
    unsigned int wheel_mapping_pass;
    unsigned int wheel_sweep_frames;
    unsigned int wheel_sweep_unique_frames;
    unsigned int armemu_signed_byte_pass;
    unsigned int system_settings_reads;
    unsigned int system_clock_reads;
    unsigned int system_battery_reads;
    unsigned int system_brightness_gets;
    unsigned int system_brightness_sets;
    int fault;
};

void ig_catalog_scan(struct ig_catalog *catalog);
bool ig_game_load(const char *path, struct ig_game *game);
const char *ig_probe_result_name(enum ig_probe_result result);
void ig_runtime_probe(const struct ig_game *game, struct ig_eapp_probe *probe);
const char *ig_vm_result_name(enum ig_vm_result result);
void ig_vm_lifecycle_test(const struct ig_game *game, unsigned int frames,
                          struct ig_vm_report *report);
enum ig_vm_result ig_vm_run_game(const struct ig_game *game);

#endif
