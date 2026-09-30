/***************************************************************************
 * Native AGDS bootstrap runtime.
 *
 * Pictures are decoded directly from a user-owned GRP archive into the
 * 320x240 Rockbox framebuffer.  No 800x600 intermediate surface is kept.
 ****************************************************************************/

#include "agds_runtime.h"
#include "agds_loader.h"
#include "agds_vm.h"
#include "fixedpoint.h"
#include "rbfile.h"
#include "../lib/jpeg_mem.h"
#ifdef SIMULATOR
#include <stdlib.h>
#endif

#define AGDS_IMAGE_MAX_WIDTH 4096u
#define AGDS_IMAGE_ROW_BYTES (AGDS_IMAGE_MAX_WIDTH * 4u)
#define AGDS_PCX_HEADER 128u
#define AGDS_PCX_PALETTE 769u
#define AGDS_LOGO_FRAMES 75u
#define AGDS_CANVAS_WIDTH 1024u
#define AGDS_CANVAS_HEIGHT 768u
#define AGDS_SCENE_HEADER_SIZE 24u
#define AGDS_SCENE_ENTRY_SIZE 16u
#define AGDS_SCENE_MAX_FRAMES 4096u
#define AGDS_SCENE_CACHE_LIMIT (8u * 1024u * 1024u)
#define AGDS_SCENE_PHASE_KEY_MS 150u
#define AGDS_SCENE_MAX_CATCHUP 4u
#define AGDS_SCENE_MASK_BYTES ((320u * 240u + 7u) / 8u)
#define AGDS_SCENE_COLOR_BYTES 4096u
#define AGDS_CHARACTER_HEADER_SIZE 14u
#define AGDS_CHARACTER_CACHE_BYTES (24u * 1024u)
#define AGDS_CHARACTER_SOURCE_SCALE_NUMERATOR 5
#define AGDS_CHARACTER_SOURCE_SCALE_DENOMINATOR 16
#define AGDS_CHARACTER_SCALE_MIN (1L << 13)
#define AGDS_CHARACTER_SCALE_MAX (8L << 16)
#define AGDS_AUDIO_RATE 44100u
#define AGDS_AUDIO_VOICES 8u
#define AGDS_AUDIO_MIX_FRAMES 256u
#define AGDS_AUDIO_STREAM_BYTES (16u * 1024u)
#define AGDS_AUDIO_QUEUE_BLOCKS 16u
#define AGDS_AUDIO_STACK_BYTES (8u * 1024u)

/* Provided by the plugin's C++ heap, already owned by ScummVM. */
extern void *malloc(size_t size);
extern void free(void *ptr);

struct agds_audio_output {
    struct mutex mutex;
    unsigned thread;
    bool quit;
    unsigned read;
    unsigned write;
    unsigned count;
    uint32_t submitted;
    uint32_t completed;
    bool in_flight;
    int16_t blocks[AGDS_AUDIO_QUEUE_BLOCKS][AGDS_AUDIO_MIX_FRAMES * 2u];
    unsigned char streams[AGDS_AUDIO_VOICES][AGDS_AUDIO_STREAM_BYTES];
    uintptr_t stack[AGDS_AUDIO_STACK_BYTES / sizeof(uintptr_t)];
};
#define AGDS_INVENTORY_Y 696
#define AGDS_INVENTORY_ARROW_WIDTH 97
#define AGDS_INVENTORY_SLOT_WIDTH 83
#define AGDS_INVENTORY_SLOT_HEIGHT 72
#define AGDS_INVENTORY_PAGE_ITEMS 10
#define AGDS_INVENTORY_ATLAS_COLUMNS 6
#define AGDS_FILM_DECODE_WIDTH 160u
#define AGDS_FILM_DECODE_HEIGHT 120u
#define AGDS_FILM_INPUT_BYTES (96u * 1024u)
#define AGDS_FILM_DECODE_BYTES (96u * 1024u)
#define AGDS_FILM_SUBTITLE_BYTES 16384u
#define AGDS_FILM_RATE 24u

enum agds_boot_phase {
    AGDS_BOOT_TAC,
    AGDS_BOOT_IDENTITY,
    AGDS_BOOT_MENU,
    AGDS_BOOT_ROOM
};

struct agds_resource_stream {
    struct scummvm_file file;
    uint32_t start;
    uint32_t size;
    uint32_t position;
    unsigned char buffer[4096];
    uint32_t buffered;
    uint32_t consumed;
};

struct agds_scene_stream {
    struct scummvm_file file;
    unsigned char *cache;
    uint32_t file_size;
    uint32_t table_offset;
    uint16_t frame_count;
    uint16_t frame_delay_ms;
    uint16_t frame_rate;
    uint16_t version;
    uint16_t clip;
    uint16_t clip_start;
    uint16_t clip_end;
    uint16_t frame;
    long start_tick;
    long next_tick;
    bool active;
};

struct agds_scene_color_stream {
    uint32_t remaining;
    uint32_t buffered;
    uint32_t consumed;
    unsigned char buffer[AGDS_SCENE_COLOR_BYTES];
};

struct agds_audio_voice {
    struct scummvm_file file;
    char phase_var[AGDS_VM_AUDIO_NAME_SIZE];
    uint32_t resource_start;
    uint32_t resource_size;
    uint32_t data_start;
    uint32_t data_size;
    uint32_t data_pos;
    uint32_t last_block;
    uint32_t rate;
    uint32_t step;
    uint32_t phase;
    int16_t last_left;
    int16_t last_right;
    int16_t volume;
    int16_t pan;
    uint16_t loops_left;
    uint16_t cycles;
    uint16_t channels;
    uint16_t bits;
    uint16_t buffered;
    uint16_t consumed;
    bool loaded;
    bool playing;
    bool ambient;
    bool synchronized;
    bool have_sample;
    bool phase_active;
};

struct agds_film_stream {
    struct scummvm_file file;
    uint32_t remaining;
    uint32_t frame;
    long start_tick;
    uint32_t subtitle_cursor;
    uint32_t subtitle_end_frame;
    bool active;
};

static unsigned char agds_image_row[AGDS_IMAGE_ROW_BYTES];
static unsigned char agds_mask_row[AGDS_IMAGE_ROW_BYTES];
static fb_data agds_palette[256];
static unsigned char agds_alpha_palette[256];
static struct agds_scene_stream agds_scene;
static bool agds_running;
static char agds_picture_name[AGDS_RESOURCE_NAME_SIZE];
static uint16_t agds_main_code_size;
static uint16_t agds_opcode_base;
static struct scummvm_agds_object agds_object;
static struct scummvm_agds_scene_state agds_script_scene;
static const struct scummvm_target *agds_target;
static struct scummvm_video *agds_video;
static enum agds_boot_phase agds_phase;
static unsigned agds_phase_frames;
static bool agds_advance_requested;
static bool agds_exit_requested;
static bool agds_new_game_requested;
static const char *agds_menu_message;
static int agds_menu_hover;
static bool agds_menu_redraw;
static bool agds_scene_redraw;
static uint32_t agds_drawn_scene_signature;
static uint32_t agds_background_signature;
static fb_data *agds_background_cache;
static bool agds_background_valid;
static int agds_pointer_x;
static int agds_pointer_y;
static bool agds_inventory_open;
static bool agds_inventory_redraw;
static int agds_inventory_page;
static int agds_inventory_selected;
static unsigned agds_scene_test_speed = 1;
#ifdef SIMULATOR
static bool agds_save_roundtrip_test;
static uint8_t agds_save_roundtrip_phase;
static uint8_t agds_film_test_delay;
static bool agds_scene_redraw_test;
static char agds_autostart_screen[AGDS_ADB_NAME_SIZE];
#endif
static struct agds_audio_voice agds_audio_voices[AGDS_AUDIO_VOICES];
static int16_t agds_audio_mix[AGDS_AUDIO_MIX_FRAMES * 2u];
static struct agds_audio_output *agds_audio_output;
static bool agds_audio_active;
static unsigned long agds_audio_old_frequency;
static struct agds_film_stream agds_film;
static unsigned char agds_film_input[AGDS_FILM_INPUT_BYTES];
static unsigned char agds_film_decode[AGDS_FILM_DECODE_BYTES];
static char agds_film_subtitles[AGDS_FILM_SUBTITLE_BYTES];
static char agds_film_subtitle[SCUMMVM_AGDS_DIALOG_TEXT];
static unsigned char agds_character_cache[AGDS_CHARACTER_CACHE_BYTES];
static long agds_character_cache_size;
static char agds_character_cache_request[AGDS_ADB_NAME_SIZE + 32];
static int agds_character_cache_scale_numerator =
    AGDS_CHARACTER_SOURCE_SCALE_NUMERATOR;
static int agds_character_cache_scale_denominator =
    AGDS_CHARACTER_SOURCE_SCALE_DENOMINATOR;
static char agds_character_scale_scene[AGDS_ADB_NAME_SIZE];
static int agds_character_scale_y;
static int agds_character_scale_pitch;
static int agds_character_scale_distance;
static int agds_character_scale_fov;

static bool draw_script_scene(bool clip_room_1864,
                              char *status, size_t status_size);
static uint16_t read_u16le(const unsigned char *p);
static bool scene_step(char *status, size_t status_size);

static long q16_mul(long first, long second)
{
    return (long)(((int64_t)first * second) >> 16);
}

static long q16_div(long numerator, long denominator)
{
    if (denominator == 0)
        return 0;
    return (long)(((int64_t)numerator * 65536) / denominator);
}

static long q16_floor(long value)
{
    if (value >= 0)
        return value >> 16;
    return -(long)(((uint32_t)(-value) + 0xffffu) >> 16);
}

/* The retail camera uses floating-point OpenGL, but its authored inputs are
 * integral degrees.  Compute the same trigonometric values once per actor
 * draw in Q16.16 so scaling remains inexpensive on the iPod.  angle_twice
 * stores half-degree units, preserving odd FOV half-angles exactly. */
static void camera_sincos_q16(int angle_twice, long *sine, long *cosine)
{
    int normalized = angle_twice % 720;
    unsigned long phase;
    long cosine_q31;
    long sine_q31;

    if (normalized < 0)
        normalized += 720;
    phase = (unsigned long)(((uint64_t)(unsigned)normalized << 32) / 720u);
    sine_q31 = fp_sincos(phase, &cosine_q31);
    *sine = sine_q31 >> 15;
    *cosine = cosine_q31 >> 15;
}

static long camera_tan_q16(int angle_twice)
{
    long sine;
    long cosine;

    camera_sincos_q16(angle_twice, &sine, &cosine);
    return q16_div(sine, cosine);
}

/* Recover the character depth exactly as Camera::screenToWorld does in the
 * retail executable, then differentiate the same perspective projection at
 * the authored model origin.  NCS sprites are neutral model-derived rasters
 * made at 5/16 pixel per world unit, so the result is a scale relative to
 * that source raster rather than a replacement hand-tuned size. */
static long character_camera_scale_q16(
    const struct scummvm_agds_screen_object *object,
    int source_scale_numerator, int source_scale_denominator)
{
    const struct scummvm_agds_scene_state *scene = &agds_script_scene;
    long sine_pitch;
    long cosine_pitch;
    long tangent_pitch;
    long tangent_half_fov;
    long tangent_combined;
    long tangent_clamp;
    long camera_range;
    long ray_length;
    long lower_extent;
    long upper_extent;
    long ray_height;
    long ray_slope;
    long camera_vertical;
    long angle_tangent;
    long combined_denominator;
    long pitch_offset;
    long depth;
    long view_depth;
    long focal_length;
    long depth_ratio;
    long pixels_per_unit;
    long scale;
    int screen_y;

    if (!scene->camera_set || scene->camera_fov <= 0 ||
        scene->camera_fov >= 180 || scene->camera_distance <= 0)
        return 1L << 16;

    camera_sincos_q16(scene->camera_pitch * 2,
                      &sine_pitch, &cosine_pitch);
    tangent_pitch = q16_div(sine_pitch, cosine_pitch);
    tangent_half_fov = camera_tan_q16(scene->camera_fov);
    if (cosine_pitch == 0 || tangent_half_fov <= 0)
        return 1L << 16;

    /* Camera near is hard-coded to 32 in the retail constructor. */
    camera_range = ((long)scene->camera_distance + 32L) << 16;
    ray_length = q16_div(camera_range, cosine_pitch);
    lower_extent = -q16_mul(ray_length, tangent_half_fov);
    upper_extent = -lower_extent;
    screen_y = (int)AGDS_CANVAS_HEIGHT - object->y;
    ray_height = lower_extent +
        (long)(((int64_t)(upper_extent - lower_extent) * screen_y) /
               (int)AGDS_CANVAS_HEIGHT);
    ray_height = -ray_height;

    ray_slope = q16_div(ray_height, ray_length);
    camera_vertical = q16_mul(camera_range, tangent_pitch);
    ray_height = q16_mul(ray_height, cosine_pitch);
    angle_tangent = ray_slope;
    tangent_clamp = camera_tan_q16((1 - scene->camera_pitch) * 2);
    if (angle_tangent <= tangent_clamp) {
        tangent_combined = camera_tan_q16(2);
    } else {
        combined_denominator = (1L << 16) -
            q16_mul(angle_tangent, tangent_pitch);
        tangent_combined = q16_div(angle_tangent + tangent_pitch,
                                   combined_denominator);
    }
    if (tangent_combined == 0)
        return 1L << 16;

    pitch_offset = -q16_mul(
        camera_range,
        camera_tan_q16(scene->camera_fov + scene->camera_pitch * 2));
    depth = q16_div(
        -camera_vertical - ray_height -
            q16_mul(tangent_combined,
                    q16_mul(ray_height, tangent_pitch)) - pitch_offset,
        tangent_combined);
    if (depth <= 0)
        return 1L << 16;

    /* screenToWorld returns actor-local Z.  The retail modelview first
     * translates the whole room by -(distance + near), then applies the
     * actor translation.  Perspective must use their combined distance.
     * Omitting the room translation magnifies nearby actors and makes
     * their size change too steeply across the navigation plane. */
    depth += camera_range;
    view_depth = -q16_mul(pitch_offset, sine_pitch) +
                 q16_mul(depth, cosine_pitch);
    if (view_depth <= 0)
        return 1L << 16;
    focal_length = q16_div((long)agds_video->height << 15,
                           tangent_half_fov);
    depth_ratio = q16_div(depth, view_depth);
    pixels_per_unit = q16_div(q16_mul(focal_length, depth_ratio),
                              view_depth);
    scale = pixels_per_unit * source_scale_denominator /
            source_scale_numerator;
    if (scale < AGDS_CHARACTER_SCALE_MIN)
        scale = AGDS_CHARACTER_SCALE_MIN;
    if (scale > AGDS_CHARACTER_SCALE_MAX)
        scale = AGDS_CHARACTER_SCALE_MAX;
    if (rb->strcmp(agds_character_scale_scene, scene->name) ||
        agds_character_scale_y != object->y ||
        agds_character_scale_pitch != scene->camera_pitch ||
        agds_character_scale_distance != scene->camera_distance ||
        agds_character_scale_fov != scene->camera_fov) {
        DEBUGF("agds: character projection %s y=%d camera=%d,%d,%d "
               "scale=%ld/65536\n", scene->name, object->y,
               scene->camera_pitch, scene->camera_distance,
               scene->camera_fov, scale);
        rb->strlcpy(agds_character_scale_scene, scene->name,
                    sizeof(agds_character_scale_scene));
        agds_character_scale_y = object->y;
        agds_character_scale_pitch = scene->camera_pitch;
        agds_character_scale_distance = scene->camera_distance;
        agds_character_scale_fov = scene->camera_fov;
    }
    return scale;
}

static bool draw_character_sidecar(
    const struct scummvm_agds_screen_object *object,
    char *status, size_t status_size)
{
    struct scummvm_file file;
    unsigned char header[AGDS_CHARACTER_HEADER_SIZE];
    unsigned char run[4];
    char path[AGDS_ADB_NAME_SIZE + 32];
    char high_resolution_path[AGDS_ADB_NAME_SIZE + 32];
    const char *resolved_path;
    long file_size;
    size_t position;
    uint16_t width;
    uint16_t height;
    uint16_t anchor_x;
    uint16_t anchor_y;
    uint16_t y;
    int direction;
    unsigned direction_bucket;
    int anchor_screen_x;
    int anchor_screen_y;
    long scale;

    if (object->character_definition[0] == '\0')
        return true;
    direction = object->character_direction % 360;
    if (direction < 0)
        direction += 360;
    direction_bucket = ((unsigned)direction + 22u) / 45u;
    direction_bucket = (direction_bucket % 8u) * 45u;
    if (object->character_render_owner ==
        AGDS_VM_CHARACTER_RENDER_MODEL_ANIMATION)
        direction_bucket = direction;
    if (object->character_pose_descriptor[0] == '\0') {
        rb->strlcpy(status, "NiBiRu authored character pose missing",
                    status_size);
        return false;
    }
    if (object->character_render_owner == AGDS_VM_CHARACTER_RENDER_IDLE) {
        rb->snprintf(path, sizeof(path),
                     "rockpod/characters/%s.%03u.ncs",
                     object->character_pose_descriptor, direction_bucket);
        rb->snprintf(high_resolution_path, sizeof(high_resolution_path),
                     "rockpod/characters-hq/%s.%03u.ncs",
                     object->character_pose_descriptor, direction_bucket);
    } else {
        rb->snprintf(path, sizeof(path),
                     "rockpod/characters/%s/%03u/%03u.ncs",
                     object->character_pose_descriptor, direction_bucket,
                     (unsigned)object->character_pose_frame);
        rb->snprintf(high_resolution_path, sizeof(high_resolution_path),
                     "rockpod/characters-hq/%s/%03u/%03u.ncs",
                     object->character_pose_descriptor, direction_bucket,
                     (unsigned)object->character_pose_frame);
    }
    if (rb->strcmp(agds_character_cache_request, high_resolution_path) &&
        rb->strcmp(agds_character_cache_request, path)) {
        file.fd = -1;
        resolved_path = high_resolution_path;
        agds_character_cache_scale_numerator = 1;
        agds_character_cache_scale_denominator = 1;
        if (!scummvm_file_open_game(&file, agds_target,
                                    high_resolution_path)) {
            resolved_path = path;
            agds_character_cache_scale_numerator =
                AGDS_CHARACTER_SOURCE_SCALE_NUMERATOR;
            agds_character_cache_scale_denominator =
                AGDS_CHARACTER_SOURCE_SCALE_DENOMINATOR;
            if (!scummvm_file_open_game(&file, agds_target, path)) {
                DEBUGF("agds: missing authored pose %s in scene %s\n",
                       path, agds_script_scene.name);
                rb->strlcpy(
                    status, "NiBiRu authored character pose file missing",
                    status_size);
                return false;
            }
        }
        file_size = scummvm_file_size(&file);
        if (file_size < (long)sizeof(header) ||
            file_size > (long)sizeof(agds_character_cache) ||
            scummvm_file_read(&file, agds_character_cache, file_size) !=
                file_size) {
            rb->strlcpy(status, "NiBiRu character sprite size invalid",
                        status_size);
            scummvm_file_close(&file);
            return false;
        }
        scummvm_file_close(&file);
        agds_character_cache_size = file_size;
        rb->strlcpy(agds_character_cache_request, resolved_path,
                    sizeof(agds_character_cache_request));
    }
    position = sizeof(header);
    rb->memcpy(header, agds_character_cache, sizeof(header));
    if (agds_character_cache_size < (long)sizeof(header) ||
        rb->memcmp(header, "NCS1", 4) || read_u16le(header + 4) != 1) {
        rb->strlcpy(status, "NiBiRu character sprite header invalid",
                    status_size);
        return false;
    }
    width = read_u16le(header + 6);
    height = read_u16le(header + 8);
    anchor_x = read_u16le(header + 10);
    anchor_y = read_u16le(header + 12);
    if (width == 0 || height == 0 || width > 320 || height > 240 ||
        anchor_x >= width || anchor_y >= height) {
        rb->strlcpy(status, "NiBiRu character sprite dimensions invalid",
                    status_size);
        return false;
    }
    scale = character_camera_scale_q16(
        object, agds_character_cache_scale_numerator,
        agds_character_cache_scale_denominator);
    anchor_screen_x = object->x * (int)agds_video->width /
                      (int)AGDS_CANVAS_WIDTH;
    anchor_screen_y = object->y * (int)agds_video->height /
                      (int)AGDS_CANVAS_HEIGHT;
    for (y = 0; y < height; y++) {
        unsigned char count_bytes[2];
        uint16_t run_count;
        uint16_t run_index;

        if (position + 2u > (size_t)agds_character_cache_size) {
            rb->strlcpy(status, "NiBiRu character row truncated", status_size);
            return false;
        }
        rb->memcpy(count_bytes, agds_character_cache + position, 2);
        position += 2u;
        run_count = read_u16le(count_bytes);
        if (run_count > width) {
            rb->strlcpy(status, "NiBiRu character row invalid", status_size);
            return false;
        }
        for (run_index = 0; run_index < run_count; run_index++) {
            uint16_t start;
            uint16_t length;
            uint16_t x;

            if (position + sizeof(run) >
                    (size_t)agds_character_cache_size) {
                rb->strlcpy(status, "NiBiRu character run truncated",
                            status_size);
                return false;
            }
            rb->memcpy(run, agds_character_cache + position, sizeof(run));
            position += sizeof(run);
            start = read_u16le(run);
            length = read_u16le(run + 2);
            if (length == 0 || start >= width || length > width - start ||
                (uint32_t)length * 2u > sizeof(agds_image_row) ||
                position + (size_t)length * 2u >
                    (size_t)agds_character_cache_size) {
                rb->strlcpy(status, "NiBiRu character run invalid",
                            status_size);
                return false;
            }
            rb->memcpy(agds_image_row, agds_character_cache + position,
                       (size_t)length * 2u);
            position += (size_t)length * 2u;
            for (x = 0; x < length; x++) {
                int source_x = (int)start + x - (int)anchor_x;
                int source_y = (int)y - (int)anchor_y;
                int screen_x0 = anchor_screen_x +
                    (int)q16_floor((long)source_x * scale);
                int screen_x1 = anchor_screen_x +
                    (int)q16_floor((long)(source_x + 1) * scale);
                int screen_y0 = anchor_screen_y +
                    (int)q16_floor((long)source_y * scale);
                int screen_y1 = anchor_screen_y +
                    (int)q16_floor((long)(source_y + 1) * scale);
                uint16_t rgb565;
                unsigned red;
                unsigned green;
                unsigned blue;
                fb_data color;
                int screen_y;

                if (screen_x1 <= screen_x0 || screen_y1 <= screen_y0 ||
                    screen_x1 <= 0 || screen_y1 <= 0 ||
                    screen_x0 >= agds_video->width ||
                    screen_y0 >= agds_video->height)
                    continue;
                rgb565 = read_u16le(agds_image_row + x * 2u);
                red = ((rgb565 >> 11) & 31u) * 255u / 31u;
                green = ((rgb565 >> 5) & 63u) * 255u / 63u;
                blue = (rgb565 & 31u) * 255u / 31u;
                color = LCD_RGBPACK(red, green, blue);
                if (screen_x0 < 0)
                    screen_x0 = 0;
                if (screen_y0 < 0)
                    screen_y0 = 0;
                if (screen_x1 > agds_video->width)
                    screen_x1 = agds_video->width;
                if (screen_y1 > agds_video->height)
                    screen_y1 = agds_video->height;
                for (screen_y = screen_y0; screen_y < screen_y1; screen_y++) {
                    int screen_x;
                    fb_data *destination = agds_video->pixels +
                        screen_y * agds_video->width;

                    for (screen_x = screen_x0;
                         screen_x < screen_x1; screen_x++)
                        destination[screen_x] = color;
                }
            }
        }
    }
    return true;
}

static void scene_close(void)
{
    if (agds_scene.active)
        scummvm_file_close(&agds_scene.file);
    free(agds_scene.cache);
    rb->memset(&agds_scene, 0, sizeof(agds_scene));
    agds_scene.file.fd = -1;
}

static uint16_t read_u16le(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32le(const unsigned char *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void film_close(bool notify_vm)
{
    if (agds_film.active && agds_film.file.fd >= 0)
        scummvm_file_close(&agds_film.file);
    rb->memset(&agds_film, 0, sizeof(agds_film));
    agds_film.file.fd = -1;
    agds_film_subtitles[0] = '\0';
    agds_film_subtitle[0] = '\0';
    if (notify_vm)
        scummvm_agds_vm_film_finished();
}

static bool film_read_record(bool decode,
                             char *status, size_t status_size)
{
    unsigned char size_bytes[4];
    uint32_t size;

    if (agds_film.remaining == 0) {
        film_close(true);
        return true;
    }
    if (agds_film.remaining < sizeof(size_bytes) ||
        scummvm_file_read(&agds_film.file, size_bytes,
                          sizeof(size_bytes)) != (long)sizeof(size_bytes)) {
        rb->strlcpy(status, "NiBiRu film header is truncated", status_size);
        return false;
    }
    agds_film.remaining -= sizeof(size_bytes);
    size = read_u32le(size_bytes);
    if (size == 0) {
        film_close(true);
        return true;
    }
    if (size > agds_film.remaining || size > sizeof(agds_film_input)) {
        rb->strlcpy(status, "NiBiRu film frame exceeds bound", status_size);
        return false;
    }
    if (scummvm_file_read(&agds_film.file, agds_film_input,
                          size) != (long)size) {
        rb->strlcpy(status, "NiBiRu film frame is truncated", status_size);
        return false;
    }
    if (decode) {
        struct bitmap bitmap;
        int result;

        rb->memset(&bitmap, 0, sizeof(bitmap));
        bitmap.width = AGDS_FILM_DECODE_WIDTH;
        bitmap.height = AGDS_FILM_DECODE_HEIGHT;
        bitmap.data = agds_film_decode;
        bitmap.format = FORMAT_NATIVE;
        result = decode_jpeg_mem(
            agds_film_input, size, &bitmap, sizeof(agds_film_decode),
            FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT,
            &format_native);
        if (result <= 0 || bitmap.width <= 0 || bitmap.height <= 0) {
            rb->snprintf(status, status_size,
                         "NiBiRu film JPEG decode failed (%d)", result);
            return false;
        }
        if (agds_film.frame == 0)
            DEBUGF("agds: film first JPEG decoded %dx%d -> 320x240\n",
                   bitmap.width, bitmap.height);
        {
            unsigned scale = MIN((unsigned)agds_video->width /
                                     (unsigned)bitmap.width,
                                 (unsigned)agds_video->height /
                                     (unsigned)bitmap.height);
            unsigned draw_width;
            unsigned draw_height;
            unsigned x0;
            unsigned y0;
            unsigned y;

            if (scale == 0)
                scale = 1;
            draw_width = MIN((unsigned)agds_video->width,
                             (unsigned)bitmap.width * scale);
            draw_height = MIN((unsigned)agds_video->height,
                              (unsigned)bitmap.height * scale);
            x0 = ((unsigned)agds_video->width - draw_width) / 2u;
            y0 = ((unsigned)agds_video->height - draw_height) / 2u;
            scummvm_video_clear(agds_video, LCD_BLACK);
            for (y = 0; y < draw_height; y++) {
                fb_data *destination = agds_video->pixels +
                    (y0 + y) * (unsigned)agds_video->width + x0;
                const fb_data *source = (const fb_data *)agds_film_decode +
                    (y / scale) * (unsigned)bitmap.width;
                unsigned x;

                for (x = 0; x < draw_width; x++)
                    destination[x] = source[x / scale];
            }
        }
    }
    agds_film.remaining -= size;
    agds_film.frame++;
    return true;
}

static void film_update_subtitle(void)
{
    uint32_t frame = agds_film.frame;

    if (agds_film_subtitle[0] != '\0' &&
        frame <= agds_film.subtitle_end_frame)
        return;
    agds_film_subtitle[0] = '\0';
    while (agds_film_subtitles[agds_film.subtitle_cursor] != '\0') {
        const char *line = agds_film_subtitles + agds_film.subtitle_cursor;
        const char *first_end;
        const char *second_start;
        const char *second_end;
        const char *text;
        const char *line_end;
        uint32_t begin;
        uint32_t end;
        size_t output = 0;

        if (*line != '{' ||
            (first_end = rb->strchr(line + 1, '}')) == NULL ||
            first_end[1] != '{' ||
            (second_end = rb->strchr(first_end + 2, '}')) == NULL) {
            line_end = rb->strchr(line, '\n');
            agds_film.subtitle_cursor += line_end != NULL ?
                (uint32_t)(line_end - line + 1) :
                (uint32_t)rb->strlen(line);
            continue;
        }
        second_start = first_end + 2;
        begin = (uint32_t)rb->atoi(line + 1);
        end = (uint32_t)rb->atoi(second_start);
        if (frame < begin)
            return;
        line_end = rb->strchr(second_end + 1, '\n');
        if (line_end == NULL)
            line_end = second_end + 1 + rb->strlen(second_end + 1);
        agds_film.subtitle_cursor = (uint32_t)(line_end -
            agds_film_subtitles);
        if (*line_end == '\n')
            agds_film.subtitle_cursor++;
        if (frame > end)
            continue;
        text = second_end + 1;
        while (text < line_end && (*text == ' ' || *text == '\t'))
            text++;
        while (text < line_end &&
               output + 1u < sizeof(agds_film_subtitle)) {
            char value = *text++;

            agds_film_subtitle[output++] = value == '|' ? '\n' : value;
        }
        agds_film_subtitle[output] = '\0';
        agds_film.subtitle_end_frame = end;
        return;
    }
}

static bool film_open(const struct scummvm_agds_film_event *event,
                      char *status, size_t status_size)
{
    struct scummvm_agds_resource resource;

    film_close(false);
    if (!scummvm_agds_find_resource(agds_target, event->video_name,
                                    &resource, status, status_size) ||
        resource.size < 4 ||
        !scummvm_file_open_game(&agds_film.file, agds_target,
                                resource.archive)) {
        rb->snprintf(status, status_size,
                     "NiBiRu cannot open film %.32s", event->video_name);
        film_close(true);
        return false;
    }
    agds_film.active = true;
    if (!scummvm_file_seek(&agds_film.file, (long)resource.offset)) {
        rb->snprintf(status, status_size,
                     "NiBiRu cannot seek film %.32s", event->video_name);
        film_close(true);
        return false;
    }
    agds_film.remaining = resource.size;
    agds_film.frame = 0;
    agds_film.start_tick = *rb->current_tick;
    agds_film_subtitles[0] = '\0';
    if (event->subtitles_entry[0] != '\0' &&
        !scummvm_agds_read_text(agds_target, event->subtitles_entry,
                                agds_film_subtitles,
                                sizeof(agds_film_subtitles),
                                status, status_size)) {
        film_close(true);
        return false;
    }
    agds_inventory_open = false;
    agds_inventory_selected = -1;
    scummvm_agds_vm_inventory_deselect();
    DEBUGF("agds: film stream %s bytes=%lu at 24fps\n",
           event->video_name, (unsigned long)resource.size);
    return true;
}

static bool film_process(char *status, size_t status_size)
{
    struct scummvm_agds_film_event event;
    uint32_t due;

    if (scummvm_agds_vm_take_film_event(&event) &&
        !film_open(&event, status, status_size))
        return false;
    if (!agds_film.active)
        return true;
    due = (uint32_t)MAX(0L, *rb->current_tick - agds_film.start_tick) *
          AGDS_FILM_RATE / (uint32_t)HZ + 1u;
    while (agds_film.active && agds_film.frame + 1u < due) {
        if (!film_read_record(false, status, status_size))
            return false;
    }
    if (agds_film.active && agds_film.frame < due) {
        if (!film_read_record(true, status, status_size))
            return false;
        if (agds_film.active)
            film_update_subtitle();
    }
    if (!agds_film.active)
        agds_scene_redraw = true;
    return true;
}

static void audio_voice_close(struct agds_audio_voice *voice)
{
    if (voice->loaded && voice->file.fd >= 0)
        scummvm_file_close(&voice->file);
    rb->memset(voice, 0, sizeof(*voice));
    voice->file.fd = -1;
}

static bool audio_read_at(struct agds_audio_voice *voice, uint32_t offset,
                          void *buffer, uint32_t size)
{
    if (offset > voice->resource_size ||
        size > voice->resource_size - offset)
        return false;
    return scummvm_file_seek(&voice->file,
                             (long)(voice->resource_start + offset)) &&
        scummvm_file_read(&voice->file, buffer, (long)size) == (long)size;
}

static bool audio_parse_wav(struct agds_audio_voice *voice)
{
    unsigned char header[24];
    uint32_t position = 12;
    bool have_format = false;
    bool have_data = false;

    if (voice->resource_size < 44u ||
        !audio_read_at(voice, 0, header, 12) ||
        rb->memcmp(header, "RIFF", 4) ||
        rb->memcmp(header + 8, "WAVE", 4))
        return false;
    while (position + 8u <= voice->resource_size) {
        uint32_t chunk_size;

        if (!audio_read_at(voice, position, header, 8))
            return false;
        chunk_size = read_u32le(header + 4);
        position += 8;
        if (chunk_size > voice->resource_size - position)
            return false;
        if (!rb->memcmp(header, "fmt ", 4)) {
            if (chunk_size < 16u ||
                !audio_read_at(voice, position, header, 16) ||
                read_u16le(header) != 1u)
                return false;
            voice->channels = read_u16le(header + 2);
            voice->rate = read_u32le(header + 4);
            voice->bits = read_u16le(header + 14);
            if ((voice->channels != 1u && voice->channels != 2u) ||
                (voice->bits != 8u && voice->bits != 16u) ||
                voice->rate < 8000u || voice->rate > 48000u)
                return false;
            have_format = true;
        } else if (!rb->memcmp(header, "data", 4)) {
            voice->data_start = position;
            voice->data_size = chunk_size;
            have_data = true;
        }
        position += chunk_size + (chunk_size & 1u);
        if (have_format && have_data) {
            voice->step = (voice->rate << 16) / AGDS_AUDIO_RATE;
            return voice->step != 0;
        }
    }
    return false;
}

static bool audio_voice_rewind(struct agds_audio_voice *voice)
{
    bool cached = voice->data_size != 0 &&
                  voice->buffered == voice->data_size;

    voice->data_pos = 0;
    if (!cached)
        voice->buffered = 0;
    voice->consumed = 0;
    voice->phase = 0;
    voice->have_sample = false;
    /* Short effects remain in their per-voice buffer across retriggers. */
    return cached || scummvm_file_seek(&voice->file,
                                       (long)(voice->resource_start +
                                              voice->data_start));
}

static bool audio_voice_byte(struct agds_audio_voice *voice,
                             unsigned char *value)
{
    uint32_t remaining;
    uint32_t request;
    long got;
    unsigned char *buffer = agds_audio_output->streams[
        (unsigned)(voice - agds_audio_voices)];

    if (voice->data_pos >= voice->data_size)
        return false;
    if (voice->consumed >= voice->buffered) {
        remaining = voice->data_size - voice->data_pos;
        request = MIN((uint32_t)AGDS_AUDIO_STREAM_BYTES, remaining);
        got = scummvm_file_read(&voice->file, buffer, (long)request);
        if (got <= 0)
            return false;
        voice->buffered = (uint16_t)got;
        voice->consumed = 0;
    }
    *value = buffer[voice->consumed++];
    voice->data_pos++;
    return true;
}

static bool audio_voice_source_frame(struct agds_audio_voice *voice,
                                     int16_t *left, int16_t *right)
{
    int16_t sample[2];
    unsigned channel;

    for (channel = 0; channel < voice->channels; channel++) {
        unsigned char low;
        unsigned char high;

        if (!audio_voice_byte(voice, &low))
            return false;
        if (voice->bits == 16u) {
            if (!audio_voice_byte(voice, &high))
                return false;
            sample[channel] = (int16_t)((uint16_t)low |
                                        ((uint16_t)high << 8));
        } else {
            sample[channel] = (int16_t)(((int)low - 128) << 8);
        }
    }
    *left = sample[0];
    *right = voice->channels == 2u ? sample[1] : sample[0];
    return true;
}

static bool audio_voice_next(struct agds_audio_voice *voice,
                             int16_t *left, int16_t *right)
{
    uint32_t advance;

    if (!voice->playing)
        return false;
    if (!voice->have_sample) {
        if (!audio_voice_source_frame(voice, &voice->last_left,
                                      &voice->last_right)) {
            if (voice->loops_left > 1u || voice->ambient) {
                if (voice->loops_left > 1u)
                    voice->loops_left--;
                if (!audio_voice_rewind(voice) ||
                    !audio_voice_source_frame(voice, &voice->last_left,
                                              &voice->last_right)) {
                    voice->playing = false;
                    return false;
                }
            } else {
                voice->playing = false;
                return false;
            }
        }
        voice->have_sample = true;
    }
    *left = voice->last_left;
    *right = voice->last_right;
    voice->phase += voice->step;
    advance = voice->phase >> 16;
    voice->phase &= 0xffffu;
    while (advance-- > 0) {
        if (!audio_voice_source_frame(voice, &voice->last_left,
                                      &voice->last_right)) {
            voice->have_sample = false;
            break;
        }
    }
    return true;
}

static bool audio_mix_block(int16_t *samples)
{
    unsigned frame;
    bool produced = false;

    for (frame = 0; frame < AGDS_AUDIO_MIX_FRAMES; frame++) {
        int32_t mixed_left = 0;
        int32_t mixed_right = 0;
        unsigned index;

        for (index = 0; index < AGDS_AUDIO_VOICES; index++) {
            struct agds_audio_voice *voice = &agds_audio_voices[index];
            int16_t left;
            int16_t right;
            int left_gain;
            int right_gain;

            if (!audio_voice_next(voice, &left, &right))
                continue;
            voice->last_block = agds_audio_output->submitted + 1u;
            left_gain = voice->volume;
            right_gain = voice->volume;
            if (voice->pan < 0)
                right_gain = right_gain * (100 + voice->pan) / 100;
            else if (voice->pan > 0)
                left_gain = left_gain * (100 - voice->pan) / 100;
            mixed_left += (int32_t)left * left_gain / 100;
            mixed_right += (int32_t)right * right_gain / 100;
            produced = true;
        }
        mixed_left = MAX(-32768, MIN(32767, mixed_left));
        mixed_right = MAX(-32768, MIN(32767, mixed_right));
        samples[frame * 2u] = (int16_t)mixed_left;
        samples[frame * 2u + 1u] = (int16_t)mixed_right;
    }
    return produced;
}

/* Called with the PCM lock held.  Never read files, mix voices, or take a
 * sleeping lock here: hardware invokes this from the DMA interrupt.  Copy to
 * a separate buffer so the producer can reuse the queue slot immediately. */
static void audio_get_more(const void **start, size_t *size)
{
    struct agds_audio_output *output = agds_audio_output;

    if (output->in_flight) {
        output->completed++;
        output->in_flight = false;
    }
    if (output->count == 0) {
        *start = NULL;
        *size = 0;
        return;
    }
    rb->memcpy(agds_audio_mix, output->blocks[output->read],
               sizeof(agds_audio_mix));
    output->read = (output->read + 1u) % AGDS_AUDIO_QUEUE_BLOCKS;
    output->count--;
    output->in_flight = true;
    *start = agds_audio_mix;
    *size = sizeof(agds_audio_mix);
}

static void audio_worker(void)
{
    struct agds_audio_output *output = agds_audio_output;

    for (;;) {
        bool space;
        bool ready;
        bool produced = false;

        rb->mutex_lock(&output->mutex);
        if (output->quit) {
            rb->mutex_unlock(&output->mutex);
            return;
        }
        rb->pcm_play_lock();
        space = output->count < AGDS_AUDIO_QUEUE_BLOCKS;
        rb->pcm_play_unlock();
        if (space)
            produced = audio_mix_block(output->blocks[output->write]);
        rb->pcm_play_lock();
        if (produced) {
            output->write = (output->write + 1u) % AGDS_AUDIO_QUEUE_BLOCKS;
            output->count++;
            output->submitted++;
        }
        ready = output->count >= 2u ||
                (!produced && output->count != 0);
        rb->pcm_play_unlock();
        if (ready && rb->mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK) ==
                         CHANNEL_STOPPED) {
            const void *start;
            size_t size;

            /* Protect the initial callback too, which mixer play_data may
             * invoke synchronously before enabling the channel. */
            rb->pcm_play_lock();
            audio_get_more(&start, &size);
            rb->pcm_play_unlock();
            rb->mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK,
                                        audio_get_more, start, size);
        }
        rb->mutex_unlock(&output->mutex);
        if (produced)
            rb->yield();
        else
            rb->sleep(1);
    }
}

static bool audio_begin(void)
{
    struct agds_audio_output *output = agds_audio_output;

    if (agds_audio_active)
        return true;
    agds_audio_old_frequency = rb->mixer_get_frequency();
    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->mixer_set_frequency(AGDS_AUDIO_RATE);
    rb->pcmbuf_fade(false, true);
    agds_audio_active = true;
    output->thread = rb->create_thread(
        audio_worker, output->stack, sizeof(output->stack), 0,
        "AGDS audio" IF_PRIO(, PRIORITY_PLAYBACK) IF_COP(, CPU));
    if (output->thread == 0 || output->thread == UINT_MAX) {
        output->thread = 0;
        return false;
    }
    return true;
}

/* Caller owns the voice mutex; stop DMA before discarding prepared samples. */
static void audio_flush(void)
{
    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    agds_audio_output->read = 0;
    agds_audio_output->write = 0;
    agds_audio_output->count = 0;
    agds_audio_output->completed = agds_audio_output->submitted;
    agds_audio_output->in_flight = false;
}

static bool scene_open(char *status, size_t status_size)
{
    unsigned char header[AGDS_SCENE_HEADER_SIZE];
    uint32_t table_size;
    uint32_t data_offset;
    uint16_t version;
    uint16_t timing;
    long file_size;

    scene_close();
    if (!scummvm_file_open_game(&agds_scene.file, agds_target,
                                "rockpod/intro1864.nbs")) {
        rb->strlcpy(status, "Missing rockpod/intro1864.nbs", status_size);
        return false;
    }
    file_size = scummvm_file_size(&agds_scene.file);
    if (file_size < (long)sizeof(header) ||
        scummvm_file_read(&agds_scene.file, header, sizeof(header)) !=
            (long)sizeof(header) ||
        rb->memcmp(header, "NBS1", 4) != 0 ||
        read_u16le(header + 6) != 320 || read_u16le(header + 8) != 240 ||
        read_u16le(header + 10) == 0 ||
        read_u16le(header + 10) > AGDS_SCENE_MAX_FRAMES ||
        read_u16le(header + 12) == 0 ||
        read_u16le(header + 14) != AGDS_SCENE_ENTRY_SIZE) {
        rb->strlcpy(status, "NiBiRu direct scene header invalid", status_size);
        scene_close();
        return false;
    }
    version = read_u16le(header + 4);
    timing = read_u16le(header + 12);
    if ((version < 1u || version > 3u) ||
        (version >= 2u && timing > 60u) ||
        (version == 3u && (timing != 24u || read_u16le(header + 10) != 424u))) {
        rb->strlcpy(status, "NiBiRu direct scene timing invalid", status_size);
        scene_close();
        return false;
    }
    agds_scene.frame_count = read_u16le(header + 10);
    agds_scene.frame_delay_ms = version == 1u ? timing : 0u;
    agds_scene.frame_rate = version >= 2u ? timing : 0u;
    agds_scene.version = version;
    agds_scene.clip = 1;
    agds_scene.clip_start = version == 3u ? 6u : 0u;
    agds_scene.clip_end = 315;
    agds_scene.table_offset = read_u32le(header + 16);
    data_offset = read_u32le(header + 20);
    table_size = (uint32_t)agds_scene.frame_count * AGDS_SCENE_ENTRY_SIZE;
    if (agds_scene.table_offset < AGDS_SCENE_HEADER_SIZE ||
        agds_scene.table_offset > (uint32_t)file_size ||
        table_size > (uint32_t)file_size - agds_scene.table_offset ||
        data_offset < agds_scene.table_offset + table_size ||
        data_offset > (uint32_t)file_size) {
        rb->strlcpy(status, "NiBiRu direct scene table invalid", status_size);
        scene_close();
        return false;
    }
    agds_scene.file_size = (uint32_t)file_size;
    /* The owned intro is about 2.7 MiB.  Cache its compressed deltas in the
     * existing game heap so table seeks cannot compete with audio streaming
     * on every frame.  Larger scenes retain the bounded streaming fallback. */
    if (agds_scene.file_size <= AGDS_SCENE_CACHE_LIMIT) {
        agds_scene.cache = malloc(agds_scene.file_size);
        if (agds_scene.cache != NULL &&
            (!scummvm_file_seek(&agds_scene.file, 0) ||
             scummvm_file_read(&agds_scene.file, agds_scene.cache,
                               agds_scene.file_size) != file_size)) {
            free(agds_scene.cache);
            agds_scene.cache = NULL;
        }
    }
    agds_scene.frame = agds_scene.clip_start;
    agds_scene.start_tick = *rb->current_tick;
    agds_scene.next_tick = agds_scene.start_tick;
    agds_scene.active = true;
    return true;
}

static bool scene_color_byte(struct agds_scene_color_stream *stream,
                             unsigned char *value)
{
    uint32_t request;
    long got;

    if (stream->remaining == 0)
        return false;
    if (stream->consumed >= stream->buffered) {
        request = MIN((uint32_t)sizeof(stream->buffer), stream->remaining);
        got = scummvm_file_read(&agds_scene.file, stream->buffer, request);
        if (got <= 0)
            return false;
        stream->buffered = (uint32_t)got;
        stream->consumed = 0;
    }
    *value = stream->buffer[stream->consumed++];
    stream->remaining--;
    return true;
}

static bool scene_apply_frame(char *status, size_t status_size)
{
    unsigned char entry[AGDS_SCENE_ENTRY_SIZE];
    struct agds_scene_color_stream colors;
    uint32_t offset;
    uint32_t size;
    uint32_t mask_size;
    uint32_t pixels;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint32_t pixel;
    const unsigned char *cached_colors = NULL;

    if (agds_scene.frame >= agds_scene.frame_count)
        return true;
    if (agds_scene.cache != NULL) {
        rb->memcpy(entry, agds_scene.cache + agds_scene.table_offset +
                   (uint32_t)agds_scene.frame * AGDS_SCENE_ENTRY_SIZE,
                   sizeof(entry));
    } else if (!scummvm_file_seek(
            &agds_scene.file,
            (long)(agds_scene.table_offset +
                   (uint32_t)agds_scene.frame * AGDS_SCENE_ENTRY_SIZE)) ||
        scummvm_file_read(&agds_scene.file, entry, sizeof(entry)) !=
            (long)sizeof(entry)) {
        rb->strlcpy(status, "NiBiRu direct scene entry truncated", status_size);
        return false;
    }
    offset = read_u32le(entry);
    size = read_u32le(entry + 4);
    x = read_u16le(entry + 8);
    y = read_u16le(entry + 10);
    width = read_u16le(entry + 12);
    height = read_u16le(entry + 14);
    if (width == 0 || height == 0) {
        if (size != 0) {
            rb->strlcpy(status, "NiBiRu empty scene delta invalid", status_size);
            return false;
        }
        return true;
    }
    pixels = (uint32_t)width * height;
    mask_size = (pixels + 7u) / 8u;
    if (x >= 320 || y >= 240 || width > 320u - x || height > 240u - y ||
        mask_size > AGDS_SCENE_MASK_BYTES || offset > agds_scene.file_size ||
        size < mask_size || size > agds_scene.file_size - offset) {
        rb->strlcpy(status, "NiBiRu direct scene delta invalid", status_size);
        return false;
    }
    if (agds_scene.cache != NULL) {
        rb->memcpy(agds_image_row, agds_scene.cache + offset, mask_size);
        cached_colors = agds_scene.cache + offset + mask_size;
    } else if (!scummvm_file_seek(&agds_scene.file, (long)offset) ||
               scummvm_file_read(&agds_scene.file, agds_image_row,
                                 mask_size) != (long)mask_size) {
        rb->strlcpy(status, "NiBiRu scene mask truncated", status_size);
        return false;
    }
    rb->memset(&colors, 0, sizeof(colors));
    colors.remaining = size - mask_size;
    for (pixel = 0; pixel < pixels; pixel++) {
        unsigned char low;
        unsigned char high;
        uint16_t rgb565;
        unsigned red;
        unsigned green;
        unsigned blue;
        uint32_t screen_x;
        uint32_t screen_y;

        if (!(agds_image_row[pixel >> 3] & (1u << (pixel & 7u))))
            continue;
        if (cached_colors != NULL && colors.remaining >= 2u) {
            low = *cached_colors++;
            high = *cached_colors++;
            colors.remaining -= 2u;
        } else if (cached_colors != NULL ||
                   !scene_color_byte(&colors, &low) ||
                   !scene_color_byte(&colors, &high)) {
            rb->strlcpy(status, "NiBiRu scene colors truncated", status_size);
            return false;
        }
        rgb565 = (uint16_t)low | ((uint16_t)high << 8);
        red = ((rgb565 >> 11) & 31u) * 255u / 31u;
        green = ((rgb565 >> 5) & 63u) * 255u / 63u;
        blue = (rgb565 & 31u) * 255u / 31u;
        screen_x = x + pixel % width;
        screen_y = y + pixel / width;
        agds_video->pixels[screen_y * 320u + screen_x] =
            LCD_RGBPACK(red, green, blue);
    }
    if (colors.remaining != 0) {
        rb->strlcpy(status, "NiBiRu scene colors overlong", status_size);
        return false;
    }
    return true;
}

static bool scene_step(char *status, size_t status_size)
{
    long delay_ticks = 0;
    unsigned catchup = 0;

    if (agds_scene.active && agds_scene.version == 3u) {
        unsigned request = scummvm_agds_vm_take_scene_animation();
        if (request != 0 && request <= 4u && request != agds_scene.clip) {
            static const uint16_t starts[] = {0, 6, 321, 357, 393};
            static const uint16_t ends[] = {0, 315, 351, 387, 423};
            agds_scene.clip = request;
            agds_scene.clip_start = starts[request];
            agds_scene.clip_end = ends[request];
            agds_scene.frame = agds_scene.clip_start;
            agds_scene.start_tick = *rb->current_tick;
            agds_scene.next_tick = agds_scene.start_tick;
            DEBUGF("agds: direct scene clip %u start at %ld\n",
                   request, *rb->current_tick);
        }
        if (agds_scene.clip == 0)
            return true;
    }
    if (!agds_scene.active || agds_scene.frame >= agds_scene.frame_count ||
        TIME_BEFORE(*rb->current_tick, agds_scene.next_tick))
        return true;
    if (agds_scene.frame_rate == 0u)
        delay_ticks = MAX(1l,
            (long)((uint32_t)agds_scene.frame_delay_ms * HZ /
                   (1000u * agds_scene_test_speed)));
    /* Apply a short bounded burst when storage or another plugin iteration
     * overruns.  This preserves wall-clock cadence instead of turning an
     * isolated slow frame into permanent slow motion. */
    while (agds_scene.frame < agds_scene.frame_count &&
           !TIME_BEFORE(*rb->current_tick, agds_scene.next_tick) &&
           catchup++ < AGDS_SCENE_MAX_CATCHUP) {
        uint16_t applied_frame = agds_scene.frame;
        uint32_t elapsed_ms = agds_scene.frame_rate != 0u ?
            (uint32_t)applied_frame * 1000u / agds_scene.frame_rate :
            (uint32_t)applied_frame * agds_scene.frame_delay_ms;
        uint16_t phase_frame = agds_scene.version == 3u ? applied_frame :
            (uint16_t)(elapsed_ms / AGDS_SCENE_PHASE_KEY_MS);

        if (!scene_apply_frame(status, status_size))
            return false;
        agds_scene.frame++;
        /* Legacy streams used millisecond timestamps. V3 stores one
         * original model phase per 24 Hz display frame. */
        if (agds_scene.version == 3u) {
            static const char * const phases[] = {
                "", "1122.10e1.11c6", "1122.10e1.118b",
                "1122.10e1.118d", "1122.10e1.118e"
            };
            scummvm_agds_vm_set_global(phases[agds_scene.clip],
                phase_frame == agds_scene.clip_end ? -1 :
                (int32_t)(phase_frame - agds_scene.clip_start));
        } else {
            scummvm_agds_vm_set_global("1122.10e1.11c6",
                phase_frame <= 315u ? phase_frame : -1);
            scummvm_agds_vm_set_global("1122.10e1.118b",
                phase_frame < 321u ? 0 :
                (phase_frame <= 351u ? (int32_t)(phase_frame - 321u) : -1));
            scummvm_agds_vm_set_global("1122.10e1.118d",
                phase_frame < 357u ? 0 :
                (phase_frame <= 387u ? (int32_t)(phase_frame - 357u) : -1));
            scummvm_agds_vm_set_global("1122.10e1.118e",
                phase_frame < 393u ? 0 :
                (phase_frame <= 423u ? (int32_t)(phase_frame - 393u) : -1));
        }
        /* The retail character process starts the seated roll after key 220,
         * sets its movement-notify variable to one, and waits for the chair
         * path to finish at authored key 267.  The direct scene owns that
         * exact root/chair path, so publish the same completion edge. */
        if (phase_frame >= (agds_scene.version == 3u ? 273u : 267u))
            scummvm_agds_vm_set_global("1122.10e1.1897", 0);
        if (agds_scene.frame_rate != 0u) {
            uint32_t timing_rate = (uint32_t)agds_scene.frame_rate *
                                   agds_scene_test_speed;
            uint32_t elapsed_ticks =
                ((uint32_t)(agds_scene.frame -
                    (agds_scene.version == 3u ? agds_scene.clip_start : 0u)) * HZ +
                 timing_rate - 1u) / timing_rate;
            agds_scene.next_tick = agds_scene.start_tick + elapsed_ticks;
        } else {
            agds_scene.next_tick += delay_ticks;
        }
        if (agds_scene.version == 3u &&
            applied_frame == agds_scene.clip_end) {
            DEBUGF("agds: direct scene clip %u complete at %ld\n",
                   agds_scene.clip, *rb->current_tick);
            agds_scene.clip = 0;
            return true;
        }
    }
    if (agds_scene.frame >= agds_scene.frame_count) {
        scummvm_file_close(&agds_scene.file);
        free(agds_scene.cache);
        agds_scene.cache = NULL;
        agds_scene.active = false;
#ifdef SIMULATOR
        if (agds_scene_redraw_test)
            agds_scene_redraw = true;
#endif
        DEBUGF("agds: direct scene complete (%u frames)\n",
               (unsigned)agds_scene.frame_count);
        rb->strlcpy(status, "NiBiRu intro scene complete", status_size);
    }
    return true;
}

static bool has_suffix(const char *name, const char *extension)
{
    size_t name_length = rb->strlen(name);
    size_t extension_length = rb->strlen(extension);

    return name_length >= extension_length &&
        !rb->strcasecmp(name + name_length - extension_length, extension);
}

static bool open_resource(struct scummvm_file *file,
                          const struct scummvm_target *target,
                          const struct scummvm_agds_resource *resource)
{
    long file_size;

    if (!scummvm_file_open_game(file, target, resource->archive))
        return false;
    file_size = scummvm_file_size(file);
    if (file_size < 0 || resource->offset > (uint32_t)file_size ||
        resource->size > (uint32_t)file_size - resource->offset) {
        scummvm_file_close(file);
        return false;
    }
    return true;
}

static bool read_resource_at(struct scummvm_file *file,
                             const struct scummvm_agds_resource *resource,
                             uint32_t offset, void *buffer, uint32_t size)
{
    if (offset > resource->size || size > resource->size - offset)
        return false;
    return scummvm_file_seek(file, (long)(resource->offset + offset)) &&
        scummvm_file_read(file, buffer, (long)size) == (long)size;
}

static struct agds_audio_voice *audio_find_phase(const char *phase_var)
{
    unsigned index;

    if (phase_var == NULL || phase_var[0] == '\0')
        return NULL;
    for (index = 0; index < AGDS_AUDIO_VOICES; index++) {
        if (agds_audio_voices[index].loaded &&
            !rb->strcmp(agds_audio_voices[index].phase_var, phase_var))
            return &agds_audio_voices[index];
    }
    return NULL;
}

static struct agds_audio_voice *audio_allocate_voice(const char *phase_var)
{
    struct agds_audio_voice *voice = audio_find_phase(phase_var);
    unsigned index;

    if (voice != NULL) {
        audio_voice_close(voice);
        return voice;
    }
    for (index = 0; index < AGDS_AUDIO_VOICES; index++) {
        if (!agds_audio_voices[index].loaded)
            return &agds_audio_voices[index];
    }
    for (index = 0; index < AGDS_AUDIO_VOICES; index++) {
        if (!agds_audio_voices[index].playing &&
            !agds_audio_voices[index].synchronized &&
            !agds_audio_voices[index].phase_active) {
            audio_voice_close(&agds_audio_voices[index]);
            return &agds_audio_voices[index];
        }
    }
    return NULL;
}

static void audio_stop_synchronized_voice(void)
{
    unsigned index;

    rb->mutex_lock(&agds_audio_output->mutex);
    for (index = 0; index < AGDS_AUDIO_VOICES; index++) {
        struct agds_audio_voice *voice = &agds_audio_voices[index];

        if (!voice->loaded || !voice->synchronized)
            continue;
        voice->playing = false;
        voice->have_sample = false;
        voice->phase_active = false;
    }
    if (agds_audio_active)
        audio_flush();
    rb->mutex_unlock(&agds_audio_output->mutex);
}

static bool audio_open_sidecar(struct agds_audio_voice *voice,
                               const char *resource_name)
{
    char path[AGDS_RESOURCE_NAME_SIZE + 24];
    char stem[AGDS_RESOURCE_NAME_SIZE];
    size_t length = rb->strlcpy(stem, resource_name, sizeof(stem));
    size_t index;

    if (length >= sizeof(stem))
        return false;
    for (index = length; index > 0; index--) {
        if (stem[index - 1] == '.') {
            stem[index - 1] = '\0';
            break;
        }
    }
    rb->snprintf(path, sizeof(path), "rockpod/audio/%s.wav", stem);
    if (!scummvm_file_open_game(&voice->file, agds_target, path))
        return false;
    voice->loaded = true;
    voice->resource_start = 0;
    voice->resource_size = (uint32_t)scummvm_file_size(&voice->file);
    return true;
}

static bool audio_load_event(const struct scummvm_agds_audio_event *event)
{
    struct scummvm_agds_resource resource;
    struct agds_audio_voice *voice;
    char resource_name[AGDS_RESOURCE_NAME_SIZE];
    char status[96];

    if (!scummvm_agds_read_text(agds_target, event->resource_entry,
                                resource_name, sizeof(resource_name),
                                status, sizeof(status))) {
        /* Dialogue @sound entries name an archive resource directly;
         * scripted LoadSample entries instead name an ADB descriptor. */
        rb->strlcpy(resource_name, event->resource_entry,
                    sizeof(resource_name));
    }
    /* A missing voice falls back to the subtitle timer. */
    if (event->synchronized)
        scummvm_agds_vm_voice_state(false, false);
    voice = audio_allocate_voice(event->phase_var);
    if (voice == NULL) {
        DEBUGF("agds: no audio voice for %s\n", resource_name);
        return true;
    }
    rb->memset(voice, 0, sizeof(*voice));
    voice->file.fd = -1;
    if (!audio_open_sidecar(voice, resource_name)) {
        if (!scummvm_agds_find_resource(agds_target, resource_name,
                                        &resource, status,
                                        sizeof(status)) ||
            !open_resource(&voice->file, agds_target, &resource)) {
            DEBUGF("agds: sample %s unavailable: %s\n",
                   resource_name, status);
            audio_voice_close(voice);
            return true;
        }
        voice->loaded = true;
        voice->resource_start = resource.offset;
        voice->resource_size = resource.size;
    }
    if (!audio_parse_wav(voice)) {
        DEBUGF("agds: sample %s needs PCM sidecar\n", resource_name);
        audio_voice_close(voice);
        return true;
    }
    rb->strlcpy(voice->phase_var, event->phase_var,
                sizeof(voice->phase_var));
    voice->volume = MAX(0, MIN(100, event->volume));
    voice->pan = MAX(-100, MIN(100, event->pan));
    voice->cycles = MAX(1u, event->cycles);
    voice->loops_left = voice->cycles;
    voice->ambient = event->ambient;
    voice->synchronized = event->synchronized;
    if (event->play_now) {
        voice->playing = audio_voice_rewind(voice);
    }
    voice->last_block = agds_audio_output->submitted;
    if (voice->synchronized && voice->playing) {
        scummvm_agds_vm_voice_state(true, false);
        DEBUGF("agds: voice begin %s tick=%ld duration_ms=%lu\n",
               resource_name, *rb->current_tick,
               (unsigned long)((uint64_t)voice->data_size * 1000u /
                   (voice->rate * voice->channels * (voice->bits / 8u))));
    }
    DEBUGF("agds: sample %s %s phase=%s cycles=%u\n",
           resource_name, event->play_now ? "play" : "queued",
           event->phase_var, (unsigned)voice->cycles);
    return true;
}

static bool audio_process_events(void)
{
    struct scummvm_agds_audio_event event;
    bool had_event = false;
    bool success = true;

    rb->mutex_lock(&agds_audio_output->mutex);
    while (scummvm_agds_vm_take_audio_event(&event)) {
        struct agds_audio_voice *voice;

        /* Keep already queued PCM flowing across sample triggers.  Stopping
         * the entire mixer for every keystroke clips all other voices and
         * inserts a storage/refill gap into the ambient track. */
        had_event = true;
        if (event.action == AGDS_VM_AUDIO_LOAD) {
            if (!audio_load_event(&event)) {
                success = false;
                break;
            }
            continue;
        }
        voice = audio_find_phase(event.phase_var);
        if (voice == NULL) {
            scummvm_agds_vm_set_global(event.phase_var, 0);
            continue;
        }
        if (event.action == AGDS_VM_AUDIO_STOP) {
            voice->playing = false;
            voice->have_sample = false;
            voice->phase_active = false;
            scummvm_agds_vm_set_global(event.phase_var, 0);
            DEBUGF("agds: sample phase=%s stop\n", event.phase_var);
        } else {
            voice->loops_left = voice->cycles;
            voice->playing = audio_voice_rewind(voice);
            voice->phase_active = voice->playing;
            if (voice->playing)
                scummvm_agds_vm_set_global(event.phase_var, 1);
            DEBUGF("agds: sample phase=%s restart playing=%d\n",
                   event.phase_var, voice->playing);
        }
    }
    if (success && had_event)
        success = audio_begin();
    rb->mutex_unlock(&agds_audio_output->mutex);
    return success;
}

static void audio_update_phases(void)
{
    unsigned index;
    uint32_t completed;

    rb->mutex_lock(&agds_audio_output->mutex);
    rb->pcm_play_lock();
    completed = agds_audio_output->completed;
    rb->pcm_play_unlock();
    for (index = 0; index < AGDS_AUDIO_VOICES; index++) {
        struct agds_audio_voice *voice = &agds_audio_voices[index];

        /* Decoding EOF precedes playback by the queued audio.  Only
         * notify the script after the final block has left the channel. */
        if (!voice->loaded || voice->playing ||
            (int32_t)(completed - voice->last_block) < 0)
            continue;
        if (voice->synchronized) {
            voice->synchronized = false;
            DEBUGF("agds: voice complete tick=%ld\n", *rb->current_tick);
            scummvm_agds_vm_voice_state(false, true);
        }
        if (voice->phase_active) {
            scummvm_agds_vm_set_global(voice->phase_var, 0);
            voice->phase_active = false;
        }
    }
    rb->mutex_unlock(&agds_audio_output->mutex);
}

static bool stream_open(struct agds_resource_stream *stream,
                        const struct scummvm_target *target,
                        const struct scummvm_agds_resource *resource,
                        uint32_t start, uint32_t size)
{
    rb->memset(stream, 0, sizeof(*stream));
    stream->file.fd = -1;
    if (start > resource->size || size > resource->size - start ||
        !open_resource(&stream->file, target, resource))
        return false;
    stream->start = resource->offset + start;
    stream->size = size;
    return scummvm_file_seek(&stream->file, (long)stream->start);
}

static void stream_close(struct agds_resource_stream *stream)
{
    scummvm_file_close(&stream->file);
}

static bool stream_byte(struct agds_resource_stream *stream,
                        unsigned char *value)
{
    uint32_t remaining;
    uint32_t request;
    long got;

    if (stream->position >= stream->size)
        return false;
    if (stream->consumed >= stream->buffered) {
        remaining = stream->size - stream->position;
        request = MIN((uint32_t)sizeof(stream->buffer), remaining);
        got = scummvm_file_read(&stream->file, stream->buffer, request);
        if (got <= 0)
            return false;
        stream->buffered = (uint32_t)got;
        stream->consumed = 0;
    }
    *value = stream->buffer[stream->consumed++];
    stream->position++;
    return true;
}

static bool decode_pcx_row(struct agds_resource_stream *stream,
                           unsigned char *row, uint32_t row_size,
                           bool compressed)
{
    uint32_t output = 0;

    while (output < row_size) {
        unsigned char value;
        uint32_t run = 1;

        if (!stream_byte(stream, &value))
            return false;
        if (compressed && (value & 0xc0u) == 0xc0u) {
            run = value & 0x3fu;
            if (run == 0 || !stream_byte(stream, &value))
                return false;
        }
        if (run > row_size - output)
            return false;
        while (run-- > 0)
            row[output++] = value;
    }
    return true;
}

static bool decode_pcx(const struct scummvm_target *target,
                       const struct scummvm_agds_resource *resource,
                       struct scummvm_video *video,
                       int canvas_x, int canvas_y, bool transparent,
                       uint32_t *source_width, uint32_t *source_height,
                       char *status, size_t status_size)
{
    struct scummvm_file file;
    struct agds_resource_stream stream;
    unsigned char header[AGDS_PCX_HEADER];
    unsigned char palette[AGDS_PCX_PALETTE];
    uint32_t width;
    uint32_t height;
    uint32_t bytes_per_line;
    uint32_t decoded_y = 0;
    int dy;
    int color;
    bool result = false;

    file.fd = -1;
    if (resource->size < AGDS_PCX_HEADER + AGDS_PCX_PALETTE ||
        !open_resource(&file, target, resource) ||
        !read_resource_at(&file, resource, 0, header, sizeof(header)) ||
        !read_resource_at(&file, resource,
                          resource->size - AGDS_PCX_PALETTE,
                          palette, sizeof(palette))) {
        rb->strlcpy(status, "AGDS PCX is truncated", status_size);
        goto cleanup_file;
    }
    if (header[0] != 0x0a || header[1] > 5 || header[2] > 1 ||
        header[3] != 8 || header[65] != 1 || palette[0] != 0x0c) {
        rb->strlcpy(status, "AGDS PCX format unsupported", status_size);
        goto cleanup_file;
    }
    if (read_u16le(header + 8) < read_u16le(header + 4) ||
        read_u16le(header + 10) < read_u16le(header + 6)) {
        rb->strlcpy(status, "AGDS PCX dimensions invalid", status_size);
        goto cleanup_file;
    }
    width = read_u16le(header + 8) - read_u16le(header + 4) + 1u;
    height = read_u16le(header + 10) - read_u16le(header + 6) + 1u;
    bytes_per_line = read_u16le(header + 66);
    if (width == 0 || height == 0 || width > AGDS_IMAGE_MAX_WIDTH ||
        bytes_per_line < width || bytes_per_line > AGDS_IMAGE_ROW_BYTES) {
        rb->strlcpy(status, "AGDS PCX dimensions unsupported", status_size);
        goto cleanup_file;
    }
    for (color = 0; color < 256; color++)
        agds_palette[color] = LCD_RGBPACK(palette[1 + color * 3],
                                          palette[2 + color * 3],
                                          palette[3 + color * 3]);
    scummvm_file_close(&file);
    file.fd = -1;
    if (!stream_open(&stream, target, resource, AGDS_PCX_HEADER,
                     resource->size - AGDS_PCX_HEADER - AGDS_PCX_PALETTE)) {
        rb->strlcpy(status, "Cannot stream AGDS PCX", status_size);
        return false;
    }

    for (dy = 0; dy < video->height; dy++) {
        int screen_y = (int)((uint32_t)dy * AGDS_CANVAS_HEIGHT /
                             (uint32_t)video->height);
        uint32_t wanted_y;
        int dx;

        if (screen_y < canvas_y ||
            screen_y >= canvas_y + (int)height)
            continue;
        wanted_y = (uint32_t)(screen_y - canvas_y);
        while (decoded_y <= wanted_y) {
            if (!decode_pcx_row(&stream, agds_image_row, bytes_per_line,
                                header[2] == 1)) {
                rb->strlcpy(status, "AGDS PCX RLE is truncated", status_size);
                goto cleanup_stream;
            }
            decoded_y++;
        }
        for (dx = 0; dx < video->width; dx++) {
            int screen_x = (int)((uint32_t)dx * AGDS_CANVAS_WIDTH /
                                 (uint32_t)video->width);
            uint32_t sx;
            fb_data pixel;

            if (screen_x < canvas_x ||
                screen_x >= canvas_x + (int)width)
                continue;
            sx = (uint32_t)(screen_x - canvas_x);
            pixel = agds_palette[agds_image_row[sx]];
            if (!transparent || pixel != LCD_RGBPACK(255, 0, 255))
                video->pixels[dy * video->width + dx] = pixel;
        }
    }
    *source_width = width;
    *source_height = height;
    result = true;

cleanup_stream:
    stream_close(&stream);
    return result;

cleanup_file:
    scummvm_file_close(&file);
    return false;
}

static bool decode_bmp(const struct scummvm_target *target,
                       const struct scummvm_agds_resource *resource,
                       struct scummvm_video *video,
                       int canvas_x, int canvas_y, bool transparent,
                       uint32_t *source_width, uint32_t *source_height,
                       char *status, size_t status_size)
{
    struct scummvm_file file;
    unsigned char header[54];
    unsigned char palette_entry[4];
    uint32_t image_offset;
    uint32_t width;
    int32_t signed_height;
    uint32_t height;
    uint16_t bits;
    uint32_t compression;
    uint32_t colors;
    uint64_t row_size64;
    uint32_t row_size;
    int color;
    int dy;
    bool result = false;

    file.fd = -1;
    if (!open_resource(&file, target, resource) ||
        !read_resource_at(&file, resource, 0, header, sizeof(header))) {
        rb->strlcpy(status, "AGDS BMP is truncated", status_size);
        goto cleanup;
    }
    image_offset = read_u32le(header + 10);
    width = read_u32le(header + 18);
    signed_height = (int32_t)read_u32le(header + 22);
    bits = read_u16le(header + 28);
    compression = read_u32le(header + 30);
    colors = read_u32le(header + 46);
    if (header[0] != 'B' || header[1] != 'M' ||
        read_u32le(header + 14) != 40 || width == 0 ||
        signed_height == 0 || signed_height == INT32_MIN ||
        width > AGDS_IMAGE_MAX_WIDTH ||
        (bits != 8 && bits != 24 && bits != 32) || compression != 0) {
        rb->strlcpy(status, "AGDS BMP format unsupported", status_size);
        goto cleanup;
    }
    height = signed_height < 0 ? (uint32_t)-signed_height :
                                (uint32_t)signed_height;
    row_size64 = (((uint64_t)width * bits + 31u) / 32u) * 4u;
    if (height > 16384u || row_size64 > sizeof(agds_image_row) ||
        image_offset > resource->size ||
        row_size64 * height > resource->size - image_offset) {
        rb->strlcpy(status, "AGDS BMP dimensions unsupported", status_size);
        goto cleanup;
    }
    row_size = (uint32_t)row_size64;
    if (bits == 8) {
        if (colors == 0)
            colors = 256;
        if (colors > 256 || 54u + colors * 4u > image_offset) {
            rb->strlcpy(status, "AGDS BMP palette invalid", status_size);
            goto cleanup;
        }
        for (color = 0; color < (int)colors; color++) {
            if (!read_resource_at(&file, resource, 54u + color * 4u,
                                  palette_entry, sizeof(palette_entry))) {
                rb->strlcpy(status, "AGDS BMP palette truncated", status_size);
                goto cleanup;
            }
            agds_palette[color] = LCD_RGBPACK(palette_entry[2],
                                              palette_entry[1],
                                              palette_entry[0]);
        }
    }

    for (dy = 0; dy < video->height; dy++) {
        int screen_y = (int)((uint32_t)dy * AGDS_CANVAS_HEIGHT /
                             (uint32_t)video->height);
        uint32_t source_y;
        uint32_t file_y;
        int dx;

        if (screen_y < canvas_y ||
            screen_y >= canvas_y + (int)height)
            continue;
        source_y = (uint32_t)(screen_y - canvas_y);
        file_y = signed_height > 0 ? height - 1u - source_y : source_y;

        if (!read_resource_at(&file, resource,
                              image_offset + file_y * row_size,
                              agds_image_row, row_size)) {
            rb->strlcpy(status, "AGDS BMP pixels truncated", status_size);
            goto cleanup;
        }
        for (dx = 0; dx < video->width; dx++) {
            int screen_x = (int)((uint32_t)dx * AGDS_CANVAS_WIDTH /
                                 (uint32_t)video->width);
            uint32_t sx;
            fb_data pixel;

            if (screen_x < canvas_x ||
                screen_x >= canvas_x + (int)width)
                continue;
            sx = (uint32_t)(screen_x - canvas_x);
            if (bits == 8) {
                unsigned index = agds_image_row[sx];
                if (index >= colors) {
                    rb->strlcpy(status, "AGDS BMP palette index invalid",
                                status_size);
                    goto cleanup;
                }
                pixel = agds_palette[index];
            } else {
                unsigned bytes = bits / 8;
                const unsigned char *source = agds_image_row + sx * bytes;

                pixel = LCD_RGBPACK(source[2], source[1], source[0]);
            }
            if (!transparent || pixel != LCD_RGBPACK(255, 0, 255))
                video->pixels[dy * video->width + dx] = pixel;
        }
    }
    *source_width = width;
    *source_height = height;
    result = true;

cleanup:
    scummvm_file_close(&file);
    return result;
}

static bool decode_bmp_masked(
    const struct scummvm_target *target,
    const struct scummvm_agds_resource *picture,
    const struct scummvm_agds_resource *mask,
    struct scummvm_video *video,
    int canvas_x, int canvas_y,
    uint32_t *source_width, uint32_t *source_height,
    char *status, size_t status_size)
{
    struct scummvm_file picture_file;
    struct scummvm_file mask_file;
    unsigned char picture_header[54];
    unsigned char mask_header[54];
    unsigned char palette_entry[4];
    uint32_t picture_offset;
    uint32_t mask_offset;
    uint32_t width;
    uint32_t mask_width;
    int32_t signed_height;
    int32_t signed_mask_height;
    uint32_t height;
    uint32_t mask_height;
    uint16_t bits;
    uint16_t mask_bits;
    uint32_t colors;
    uint32_t mask_colors;
    uint64_t row_size64;
    uint64_t mask_row_size64;
    uint32_t row_size;
    uint32_t mask_row_size;
    int color;
    int dy;
    bool result = false;

    picture_file.fd = -1;
    mask_file.fd = -1;
    if (!open_resource(&picture_file, target, picture) ||
        !read_resource_at(&picture_file, picture, 0,
                          picture_header, sizeof(picture_header)) ||
        !open_resource(&mask_file, target, mask) ||
        !read_resource_at(&mask_file, mask, 0,
                          mask_header, sizeof(mask_header))) {
        rb->strlcpy(status, "AGDS alpha picture is truncated", status_size);
        goto cleanup;
    }

    picture_offset = read_u32le(picture_header + 10);
    width = read_u32le(picture_header + 18);
    signed_height = (int32_t)read_u32le(picture_header + 22);
    bits = read_u16le(picture_header + 28);
    colors = read_u32le(picture_header + 46);
    mask_offset = read_u32le(mask_header + 10);
    mask_width = read_u32le(mask_header + 18);
    signed_mask_height = (int32_t)read_u32le(mask_header + 22);
    mask_bits = read_u16le(mask_header + 28);
    mask_colors = read_u32le(mask_header + 46);
    if (picture_header[0] != 'B' || picture_header[1] != 'M' ||
        mask_header[0] != 'B' || mask_header[1] != 'M' ||
        read_u32le(picture_header + 14) != 40 ||
        read_u32le(mask_header + 14) != 40 ||
        width == 0 || signed_height == 0 ||
        signed_height == INT32_MIN ||
        mask_width == 0 || signed_mask_height == 0 ||
        signed_mask_height == INT32_MIN ||
        width > AGDS_IMAGE_MAX_WIDTH ||
        mask_width > AGDS_IMAGE_MAX_WIDTH ||
        (bits != 8 && bits != 24 && bits != 32) ||
        (mask_bits != 8 && mask_bits != 24 && mask_bits != 32) ||
        read_u32le(picture_header + 30) != 0 ||
        read_u32le(mask_header + 30) != 0) {
        rb->strlcpy(status, "AGDS alpha picture format unsupported",
                    status_size);
        goto cleanup;
    }
    height = signed_height < 0 ? (uint32_t)-signed_height :
                                (uint32_t)signed_height;
    mask_height = signed_mask_height < 0 ?
        (uint32_t)-signed_mask_height : (uint32_t)signed_mask_height;
    row_size64 = (((uint64_t)width * bits + 31u) / 32u) * 4u;
    mask_row_size64 =
        (((uint64_t)mask_width * mask_bits + 31u) / 32u) * 4u;
    if (height > 16384u || mask_height > 16384u ||
        mask_width < width || mask_height < height ||
        row_size64 > sizeof(agds_image_row) ||
        mask_row_size64 > sizeof(agds_mask_row) ||
        picture_offset > picture->size || mask_offset > mask->size ||
        row_size64 * height > picture->size - picture_offset ||
        mask_row_size64 * mask_height > mask->size - mask_offset) {
        rb->strlcpy(status, "AGDS alpha picture dimensions unsupported",
                    status_size);
        goto cleanup;
    }
    row_size = (uint32_t)row_size64;
    mask_row_size = (uint32_t)mask_row_size64;

    if (bits == 8) {
        if (colors == 0)
            colors = 256;
        if (colors > 256 || 54u + colors * 4u > picture_offset) {
            rb->strlcpy(status, "AGDS alpha owner palette invalid",
                        status_size);
            goto cleanup;
        }
        for (color = 0; color < (int)colors; color++) {
            if (!read_resource_at(&picture_file, picture,
                                  54u + color * 4u,
                                  palette_entry, sizeof(palette_entry))) {
                rb->strlcpy(status, "AGDS alpha owner palette truncated",
                            status_size);
                goto cleanup;
            }
            agds_palette[color] = LCD_RGBPACK(palette_entry[2],
                                              palette_entry[1],
                                              palette_entry[0]);
        }
    }
    if (mask_bits == 8) {
        if (mask_colors == 0)
            mask_colors = 256;
        if (mask_colors > 256 ||
            54u + mask_colors * 4u > mask_offset) {
            rb->strlcpy(status, "AGDS alpha mask palette invalid",
                        status_size);
            goto cleanup;
        }
        for (color = 0; color < (int)mask_colors; color++) {
            if (!read_resource_at(&mask_file, mask, 54u + color * 4u,
                                  palette_entry, sizeof(palette_entry))) {
                rb->strlcpy(status, "AGDS alpha mask palette truncated",
                            status_size);
                goto cleanup;
            }
            /* AGDS copies the mask bitmap's first color component into the
             * owner's alpha byte. Retail NiBiRu masks are grayscale. */
            agds_alpha_palette[color] = palette_entry[2];
        }
    }

    for (dy = 0; dy < video->height; dy++) {
        int screen_y = (int)((uint32_t)dy * AGDS_CANVAS_HEIGHT /
                             (uint32_t)video->height);
        uint32_t source_y;
        uint32_t picture_file_y;
        uint32_t mask_file_y;
        int dx;

        if (screen_y < canvas_y ||
            screen_y >= canvas_y + (int)height)
            continue;
        source_y = (uint32_t)(screen_y - canvas_y);
        picture_file_y = signed_height > 0 ?
            height - 1u - source_y : source_y;
        mask_file_y = signed_mask_height > 0 ?
            mask_height - 1u - source_y : source_y;
        if (!read_resource_at(&picture_file, picture,
                              picture_offset + picture_file_y * row_size,
                              agds_image_row, row_size) ||
            !read_resource_at(&mask_file, mask,
                              mask_offset + mask_file_y * mask_row_size,
                              agds_mask_row, mask_row_size)) {
            rb->strlcpy(status, "AGDS alpha picture pixels truncated",
                        status_size);
            goto cleanup;
        }
        for (dx = 0; dx < video->width; dx++) {
            int screen_x = (int)((uint32_t)dx * AGDS_CANVAS_WIDTH /
                                 (uint32_t)video->width);
            uint32_t sx;
            fb_data source_pixel;
            fb_data destination_pixel;
            unsigned alpha;
            unsigned red;
            unsigned green;
            unsigned blue;

            if (screen_x < canvas_x ||
                screen_x >= canvas_x + (int)width)
                continue;
            sx = (uint32_t)(screen_x - canvas_x);
            if (bits == 8) {
                unsigned palette_index = agds_image_row[sx];

                if (palette_index >= colors) {
                    rb->strlcpy(status,
                                "AGDS alpha owner palette index invalid",
                                status_size);
                    goto cleanup;
                }
                source_pixel = agds_palette[palette_index];
            } else {
                unsigned bytes = bits / 8;
                const unsigned char *source =
                    agds_image_row + sx * bytes;

                source_pixel = LCD_RGBPACK(source[2], source[1], source[0]);
            }
            if (mask_bits == 8) {
                unsigned palette_index = agds_mask_row[sx];

                if (palette_index >= mask_colors) {
                    rb->strlcpy(status,
                                "AGDS alpha mask palette index invalid",
                                status_size);
                    goto cleanup;
                }
                alpha = agds_alpha_palette[palette_index];
            } else {
                unsigned bytes = mask_bits / 8;

                alpha = agds_mask_row[sx * bytes];
            }
            if (alpha == 0)
                continue;
            if (alpha == 255) {
                video->pixels[dy * video->width + dx] = source_pixel;
                continue;
            }
            destination_pixel = video->pixels[dy * video->width + dx];
            red = ((unsigned)RGB_UNPACK_RED(source_pixel) * alpha +
                   (unsigned)RGB_UNPACK_RED(destination_pixel) *
                       (255u - alpha) + 127u) / 255u;
            green = ((unsigned)RGB_UNPACK_GREEN(source_pixel) * alpha +
                     (unsigned)RGB_UNPACK_GREEN(destination_pixel) *
                         (255u - alpha) + 127u) / 255u;
            blue = ((unsigned)RGB_UNPACK_BLUE(source_pixel) * alpha +
                    (unsigned)RGB_UNPACK_BLUE(destination_pixel) *
                        (255u - alpha) + 127u) / 255u;
            video->pixels[dy * video->width + dx] =
                LCD_RGBPACK(red, green, blue);
        }
    }
    *source_width = width;
    *source_height = height;
    result = true;

cleanup:
    scummvm_file_close(&mask_file);
    scummvm_file_close(&picture_file);
    return result;
}

static bool decode_bmp_tile(
    const struct scummvm_target *target,
    const struct scummvm_agds_resource *resource,
    struct scummvm_video *video,
    uint32_t tile_x, uint32_t tile_y,
    uint32_t tile_width, uint32_t tile_height,
    int canvas_x, int canvas_y,
    char *status, size_t status_size)
{
    struct scummvm_file file;
    unsigned char header[54];
    unsigned char palette_entry[4];
    uint32_t image_offset;
    uint32_t width;
    int32_t signed_height;
    uint32_t height;
    uint16_t bits;
    uint32_t compression;
    uint32_t colors;
    uint64_t row_size64;
    uint32_t row_size;
    int color;
    int dy;
    bool result = false;

    file.fd = -1;
    if (!open_resource(&file, target, resource) ||
        !read_resource_at(&file, resource, 0, header, sizeof(header))) {
        rb->strlcpy(status, "AGDS inventory BMP is truncated", status_size);
        goto cleanup;
    }
    image_offset = read_u32le(header + 10);
    width = read_u32le(header + 18);
    signed_height = (int32_t)read_u32le(header + 22);
    bits = read_u16le(header + 28);
    compression = read_u32le(header + 30);
    colors = read_u32le(header + 46);
    if (header[0] != 'B' || header[1] != 'M' ||
        read_u32le(header + 14) != 40 || width == 0 ||
        signed_height == 0 || signed_height == INT32_MIN ||
        width > AGDS_IMAGE_MAX_WIDTH ||
        (bits != 8 && bits != 24 && bits != 32) || compression != 0) {
        rb->strlcpy(status, "AGDS inventory BMP unsupported", status_size);
        goto cleanup;
    }
    height = signed_height < 0 ? (uint32_t)-signed_height :
                                (uint32_t)signed_height;
    row_size64 = (((uint64_t)width * bits + 31u) / 32u) * 4u;
    if (height > 16384u || row_size64 > sizeof(agds_image_row) ||
        image_offset > resource->size ||
        row_size64 * height > resource->size - image_offset ||
        tile_width == 0 || tile_height == 0 ||
        tile_x > width || tile_width > width - tile_x ||
        tile_y > height || tile_height > height - tile_y) {
        rb->strlcpy(status, "AGDS inventory tile outside atlas", status_size);
        goto cleanup;
    }
    row_size = (uint32_t)row_size64;
    if (bits == 8) {
        if (colors == 0)
            colors = 256;
        if (colors > 256 || 54u + colors * 4u > image_offset) {
            rb->strlcpy(status, "AGDS inventory palette invalid",
                        status_size);
            goto cleanup;
        }
        for (color = 0; color < (int)colors; color++) {
            if (!read_resource_at(&file, resource, 54u + color * 4u,
                                  palette_entry, sizeof(palette_entry))) {
                rb->strlcpy(status, "AGDS inventory palette truncated",
                            status_size);
                goto cleanup;
            }
            agds_palette[color] = LCD_RGBPACK(palette_entry[2],
                                              palette_entry[1],
                                              palette_entry[0]);
        }
    }

    for (dy = 0; dy < video->height; dy++) {
        int screen_y = (int)((uint32_t)dy * AGDS_CANVAS_HEIGHT /
                             (uint32_t)video->height);
        uint32_t source_y;
        uint32_t file_y;
        int dx;

        if (screen_y < canvas_y ||
            screen_y >= canvas_y + (int)tile_height)
            continue;
        source_y = tile_y + (uint32_t)(screen_y - canvas_y);
        file_y = signed_height > 0 ? height - 1u - source_y : source_y;
        if (!read_resource_at(&file, resource,
                              image_offset + file_y * row_size,
                              agds_image_row, row_size)) {
            rb->strlcpy(status, "AGDS inventory pixels truncated",
                        status_size);
            goto cleanup;
        }
        for (dx = 0; dx < video->width; dx++) {
            int screen_x = (int)((uint32_t)dx * AGDS_CANVAS_WIDTH /
                                 (uint32_t)video->width);
            uint32_t source_x;
            fb_data pixel;

            if (screen_x < canvas_x ||
                screen_x >= canvas_x + (int)tile_width)
                continue;
            source_x = tile_x + (uint32_t)(screen_x - canvas_x);
            if (bits == 8) {
                unsigned palette_index = agds_image_row[source_x];

                if (palette_index >= colors) {
                    rb->strlcpy(status,
                                "AGDS inventory palette index invalid",
                                status_size);
                    goto cleanup;
                }
                pixel = agds_palette[palette_index];
            } else {
                unsigned bytes = bits / 8;
                const unsigned char *source =
                    agds_image_row + source_x * bytes;

                pixel = LCD_RGBPACK(source[2], source[1], source[0]);
            }
            if (pixel != LCD_RGBPACK(255, 0, 255))
                video->pixels[dy * video->width + dx] = pixel;
        }
    }
    result = true;

cleanup:
    scummvm_file_close(&file);
    return result;
}

static bool validate_main_object(const struct scummvm_target *target,
                                 char *status, size_t status_size)
{
    if (!scummvm_agds_object_load(target, "main", &agds_object,
                                  status, status_size))
        return false;
    agds_main_code_size = agds_object.code_size;
    agds_opcode_base = agds_object.opcode_base;
    return true;
}

static bool decode_picture_resource(
    const struct scummvm_target *target,
    struct scummvm_video *video,
    const struct scummvm_agds_resource *picture,
    int canvas_x, int canvas_y, bool transparent,
    uint32_t *width, uint32_t *height,
    char *status, size_t status_size)
{
    if (has_suffix(picture->name, ".pcx"))
        return decode_pcx(target, picture, video,
                          canvas_x, canvas_y, transparent, width, height,
                          status, status_size);
    return decode_bmp(target, picture, video,
                      canvas_x, canvas_y, transparent, width, height,
                      status, status_size);
}

static bool decode_object_picture(
    const struct scummvm_target *target,
    struct scummvm_video *video,
    const char *object_name, uint16_t string_index,
    int canvas_x, int canvas_y, bool transparent,
    uint32_t *width, uint32_t *height,
    char *status, size_t status_size)
{
    struct scummvm_agds_resource picture;
    char entry[AGDS_ADB_NAME_SIZE];
    char name[AGDS_RESOURCE_NAME_SIZE];

    if (!scummvm_agds_object_load(target, object_name, &agds_object,
                                  status, status_size) ||
        !scummvm_agds_object_string(&agds_object, string_index,
                                    entry, sizeof(entry)) ||
        !scummvm_agds_read_text(target, entry, name, sizeof(name),
                                status, status_size) ||
        !scummvm_agds_find_resource(target, name, &picture,
                                    status, status_size) ||
        !decode_picture_resource(target, video, &picture,
                                 canvas_x, canvas_y, transparent,
                                 width, height,
                                 status, status_size))
        return false;
    rb->strlcpy(agds_picture_name, picture.name,
                sizeof(agds_picture_name));
    return true;
}

static bool decode_descriptor_picture(
    const struct scummvm_target *target,
    struct scummvm_video *video,
    const char *entry, int canvas_x, int canvas_y, bool transparent,
    uint32_t *width, uint32_t *height,
    char *status, size_t status_size)
{
    struct scummvm_agds_resource picture;
    char name[AGDS_RESOURCE_NAME_SIZE];

    if (!scummvm_agds_read_text(target, entry, name, sizeof(name),
                                status, status_size) ||
        !scummvm_agds_find_resource(target, name, &picture,
                                    status, status_size) ||
        !decode_picture_resource(target, video, &picture,
                                 canvas_x, canvas_y, transparent,
                                 width, height, status, status_size))
        return false;
    rb->strlcpy(agds_picture_name, picture.name,
                sizeof(agds_picture_name));
    return true;
}

static bool decode_descriptor_picture_masked(
    const struct scummvm_target *target,
    struct scummvm_video *video,
    const char *picture_entry, const char *mask_entry,
    int canvas_x, int canvas_y,
    uint32_t *width, uint32_t *height,
    char *status, size_t status_size)
{
    struct scummvm_agds_resource picture;
    struct scummvm_agds_resource mask;
    char picture_name[AGDS_RESOURCE_NAME_SIZE];
    char mask_name[AGDS_RESOURCE_NAME_SIZE];

    if (!scummvm_agds_read_text(target, picture_entry,
                                picture_name, sizeof(picture_name),
                                status, status_size) ||
        !scummvm_agds_find_resource(target, picture_name, &picture,
                                    status, status_size) ||
        !scummvm_agds_read_text(target, mask_entry,
                                mask_name, sizeof(mask_name),
                                status, status_size) ||
        !scummvm_agds_find_resource(target, mask_name, &mask,
                                    status, status_size) ||
        !decode_bmp_masked(target, &picture, &mask, video,
                           canvas_x, canvas_y, width, height,
                           status, status_size))
        return false;
    rb->strlcpy(agds_picture_name, picture.name,
                sizeof(agds_picture_name));
    return true;
}

static void clear_inventory_strip(void)
{
    int dy;

    for (dy = 0; dy < agds_video->height; dy++) {
        int source_y = (int)((uint32_t)dy * AGDS_CANVAS_HEIGHT /
                             (uint32_t)agds_video->height);
        int dx;

        if (source_y < AGDS_INVENTORY_Y)
            continue;
        for (dx = 0; dx < agds_video->width; dx++)
            agds_video->pixels[dy * agds_video->width + dx] = LCD_BLACK;
    }
}

static bool draw_inventory_item(
    const struct scummvm_agds_inventory_item *item,
    int canvas_x, char *status, size_t status_size)
{
    struct scummvm_agds_resource picture;
    char name[AGDS_RESOURCE_NAME_SIZE];
    uint32_t tile_x;
    uint32_t tile_y;

    if (item->picture_entry[0] == '\0' ||
        item->tile_width != AGDS_INVENTORY_SLOT_WIDTH ||
        item->tile_height != AGDS_INVENTORY_SLOT_HEIGHT) {
        rb->strlcpy(status, "AGDS inventory item metadata invalid",
                    status_size);
        return false;
    }
    if (!scummvm_agds_read_text(agds_target, item->picture_entry,
                                name, sizeof(name),
                                status, status_size) ||
        !scummvm_agds_find_resource(agds_target, name, &picture,
                                    status, status_size))
        return false;
    tile_x = (item->picture_tile % AGDS_INVENTORY_ATLAS_COLUMNS) *
             item->tile_width;
    tile_y = (item->picture_tile / AGDS_INVENTORY_ATLAS_COLUMNS) *
             item->tile_height;
    return decode_bmp_tile(agds_target, &picture, agds_video,
                           tile_x, tile_y,
                           item->tile_width, item->tile_height,
                           canvas_x, AGDS_INVENTORY_Y,
                           status, status_size);
}

static bool draw_inventory_overlay(char *status, size_t status_size)
{
    uint32_t width = 0;
    uint32_t height = 0;
    unsigned count = scummvm_agds_vm_inventory_count();
    unsigned first;
    unsigned shown;
    unsigned index;

    clear_inventory_strip();
    if (!decode_descriptor_picture(
            agds_target, agds_video, "inv.18fd.p",
            0, AGDS_INVENTORY_Y, false,
            &width, &height, status, status_size) ||
        width != AGDS_INVENTORY_ARROW_WIDTH ||
        height != AGDS_INVENTORY_SLOT_HEIGHT ||
        !decode_descriptor_picture(
            agds_target, agds_video, "inv.18fe.p",
            (int)AGDS_CANVAS_WIDTH - AGDS_INVENTORY_ARROW_WIDTH,
            AGDS_INVENTORY_Y, false,
            &width, &height, status, status_size) ||
        width != AGDS_INVENTORY_ARROW_WIDTH ||
        height != AGDS_INVENTORY_SLOT_HEIGHT) {
        rb->strlcpy(status, "AGDS retail inventory arrows invalid",
                    status_size);
        return false;
    }
    if (count == 0) {
        agds_inventory_page = 0;
        first = 0;
    } else {
        first = (unsigned)agds_inventory_page *
                AGDS_INVENTORY_PAGE_ITEMS;
    }
    if (count != 0 && first >= count && agds_inventory_page > 0) {
        agds_inventory_page = (int)((count - 1u) /
                                    AGDS_INVENTORY_PAGE_ITEMS);
        first = (unsigned)agds_inventory_page *
                AGDS_INVENTORY_PAGE_ITEMS;
    }
    shown = MIN(count - first, (unsigned)AGDS_INVENTORY_PAGE_ITEMS);
    for (index = 0; index < shown; index++) {
        struct scummvm_agds_inventory_item item;

        if (!scummvm_agds_vm_inventory_item(first + index, &item) ||
            !draw_inventory_item(
                &item,
                AGDS_INVENTORY_ARROW_WIDTH +
                    (int)index * AGDS_INVENTORY_SLOT_WIDTH,
                status, status_size))
            return false;
    }
    agds_inventory_redraw = false;
    DEBUGF("agds: retail inventory page=%d items=%u selected=%d\n",
           agds_inventory_page, shown, agds_inventory_selected);
    return true;
}

static bool ensure_retail_fonts(char *status, size_t status_size)
{
    struct scummvm_agds_font font;

    if (scummvm_agds_vm_font(0, &font))
        return true;
    return scummvm_agds_vm_run_object_named(
        "main.104c", status, status_size);
}

static bool load_boot_phase(enum agds_boot_phase phase,
                            char *status, size_t status_size)
{
    uint32_t width = 0;
    uint32_t height = 0;
    const char *label;
    bool loaded;

    scummvm_video_clear(agds_video, LCD_BLACK);
    if (phase == AGDS_BOOT_TAC) {
        /* main sets main.1008.obr=1, selecting main.1008.p2. */
        loaded = decode_object_picture(agds_target, agds_video,
                                       "main.1008", 7,
                                       32, 159, false,
                                       &width, &height,
                                       status, status_size);
        label = "Adventure Company";
    } else if (phase == AGDS_BOOT_IDENTITY) {
        /* main then clears main.1008.obr, selecting main.1008.p. */
        loaded = decode_object_picture(agds_target, agds_video,
                                       "main.1008", 6,
                                       0, 0, false,
                                       &width, &height,
                                       status, status_size);
        label = "Unknown Identity";
    } else {
        /* 1009.1011.1012 installs the main-menu background. */
        loaded = decode_object_picture(agds_target, agds_video,
                                       "1009.1011.1012", 1,
                                       0, 0, false,
                                       &width, &height,
                                       status, status_size);
        label = "main menu";
    }
    if (!loaded)
        return false;
    if (phase == AGDS_BOOT_MENU) {
        if (!scummvm_agds_vm_start_screen(agds_target, "1009.1011",
                                          &agds_script_scene,
                                          status, status_size) ||
            /* Retail main initialises the persistent inventory process before
             * the menu can dispatch New Game.  Keep that authored ordering so
             * 107a's initial wallet/phone objects see the six real atlases. */
            !scummvm_agds_vm_run_object_named("inv.1090",
                                              status, status_size) ||
            !scummvm_agds_vm_run_object_named("main.1004",
                                              status, status_size) ||
            !audio_process_events() ||
            !draw_script_scene(false, status, status_size))
            return false;
        DEBUGF("agds: retail menu VM built %u objects and mouse areas\n",
               (unsigned)agds_script_scene.object_count);
    }
    agds_phase = phase;
    agds_phase_frames = 0;
    agds_advance_requested = false;
    rb->snprintf(status, status_size,
                 "AGDS 2.509 %s: %.20s %lux%lu -> 320x240",
                 label, agds_picture_name,
                 (unsigned long)width, (unsigned long)height);
    return true;
}

static uint32_t scene_visual_signature(bool include_characters)
{
    uint32_t hash = 2166136261u;
    unsigned index;

    /* Only display state belongs here.  Region/handler bookkeeping changes
     * must not trigger archive decoding.  In particular, a script can hide
     * or move an actor without a mouse event to request the next repaint. */
    for (index = 0; index < agds_script_scene.object_count; index++) {
        const struct scummvm_agds_screen_object *object =
            &agds_script_scene.objects[index];
        const uint32_t values[] = {
            index, object->alive, object->visible, object->background,
            (uint16_t)object->x, (uint16_t)object->y, (uint16_t)object->z,
            object->character_model, object->character_render_owner,
            (uint16_t)object->character_direction,
            object->character_pose_frame
        };
        unsigned field;
        const char *names[] = { object->picture_entry,
                               object->alpha_picture_entry,
                               object->character_pose_descriptor };

        if (object->character_model && !include_characters)
            continue;
        for (field = 0; field < ARRAYLEN(values); field++)
            hash = (hash ^ values[field]) * 16777619u;
        for (field = 0; field < ARRAYLEN(names); field++) {
            const unsigned char *name = (const unsigned char *)names[field];

            while (*name != '\0')
                hash = (hash ^ *name++) * 16777619u;
        }
    }
    return hash;
}

static bool draw_script_scene(bool clip_room_1864,
                              char *status, size_t status_size)
{
    uint32_t width = 0;
    uint32_t height = 0;
    bool drawn[AGDS_VM_MAX_SCREEN_OBJECTS];
    unsigned drawn_count = 0;
    unsigned index;
    int dy;
    uint32_t background_signature = scene_visual_signature(false);
    size_t background_bytes = (size_t)agds_video->width *
                              agds_video->height * sizeof(fb_data);
    bool cached;

    if (agds_background_cache == NULL)
        agds_background_cache = malloc(background_bytes);
    cached = agds_background_cache != NULL && agds_background_valid &&
             agds_background_signature == background_signature;
    if (cached)
        rb->memcpy(agds_video->pixels, agds_background_cache, background_bytes);
    else
        scummvm_video_clear(agds_video, LCD_BLACK);
    rb->memset(drawn, 0, sizeof(drawn));
    while (!cached && drawn_count < agds_script_scene.object_count) {
        int best = -1;

        for (index = 0; index < agds_script_scene.object_count; index++) {
            const struct scummvm_agds_screen_object *candidate =
                &agds_script_scene.objects[index];
            const struct scummvm_agds_screen_object *selected;

            if (drawn[index] || !candidate->alive || !candidate->visible ||
                candidate->picture_entry[0] == '\0')
                continue;
            if (best < 0) {
                best = (int)index;
                continue;
            }
            selected = &agds_script_scene.objects[best];
            if ((candidate->background && !selected->background) ||
                (candidate->background == selected->background &&
                 candidate->z < selected->z))
                best = (int)index;
        }
        if (best < 0)
            break;
        drawn[best] = true;
        drawn_count++;
        {
            struct scummvm_agds_screen_object *object =
                &agds_script_scene.objects[best];

            if (object->alpha_picture_entry[0] != '\0') {
                if (!decode_descriptor_picture_masked(
                        agds_target, agds_video,
                        object->picture_entry, object->alpha_picture_entry,
                        object->x, object->y,
                        &width, &height, status, status_size))
                    return false;
            } else if (!decode_descriptor_picture(
                           agds_target, agds_video, object->picture_entry,
                           object->x, object->y, !object->background,
                           &width, &height, status, status_size)) {
                return false;
            }
            object->picture_width = width > UINT16_MAX ? UINT16_MAX :
                                                       (uint16_t)width;
            object->picture_height = height > UINT16_MAX ? UINT16_MAX :
                                                          (uint16_t)height;
        }
    }
    if (!cached && agds_background_cache != NULL) {
        rb->memcpy(agds_background_cache, agds_video->pixels, background_bytes);
        agds_background_signature = background_signature;
        agds_background_valid = true;
    }
    for (index = 0; index < agds_script_scene.object_count; index++) {
        const struct scummvm_agds_screen_object *object =
            &agds_script_scene.objects[index];

        /* The opening scene delta stream owns its complete model layer,
         * including talking poses; do not paint a second copy over it. */
        if (!rb->strcmp(agds_script_scene.name, "1864") &&
            object->character_render_owner ==
                AGDS_VM_CHARACTER_RENDER_MODEL_ANIMATION)
            continue;
        if (object->alive && object->visible && object->character_model &&
            !draw_character_sidecar(object, status, status_size))
            return false;
    }
    if (agds_script_scene.clip_set || clip_room_1864) {
        int clip_x = agds_script_scene.clip_set ?
            agds_script_scene.clip_x : 0;
        int clip_y = agds_script_scene.clip_set ?
            agds_script_scene.clip_y : 56;
        int clip_width = agds_script_scene.clip_set ?
            agds_script_scene.clip_width : AGDS_CANVAS_WIDTH;
        int clip_height = agds_script_scene.clip_set ?
            agds_script_scene.clip_height : 640;

        /* Opcode 284 copies Process x/y/width/height into the retail screen
         * clip rectangle. NiBiRu rooms author 0,56 1024x640, leaving the top
         * letterbox and bottom inventory strip outside the room viewport. */
        for (dy = 0; dy < agds_video->height; dy++) {
            int source_y = (int)((uint32_t)dy * AGDS_CANVAS_HEIGHT /
                                 (uint32_t)agds_video->height);
            int dx;

            for (dx = 0; dx < agds_video->width; dx++) {
                int source_x = (int)((uint32_t)dx * AGDS_CANVAS_WIDTH /
                                     (uint32_t)agds_video->width);

                if (source_x < clip_x || source_x >= clip_x + clip_width ||
                    source_y < clip_y || source_y >= clip_y + clip_height)
                    agds_video->pixels[dy * agds_video->width + dx] =
                        LCD_BLACK;
            }
        }
    }
    agds_drawn_scene_signature = scene_visual_signature(true);
    return true;
}

static void log_script_scene(const char *name)
{
    unsigned index;

    (void)name;
    DEBUGF("agds: screen %s VM built %u objects\n", name,
           (unsigned)agds_script_scene.object_count);
    for (index = 0; index < agds_script_scene.object_count; index++) {
        const struct scummvm_agds_screen_object *object =
            &agds_script_scene.objects[index];

        if (object->picture_entry[0] != '\0')
            DEBUGF("agds: script picture %s %s at %d,%d z=%d bg=%d\n",
                   object->name, object->picture_entry,
                   (int)object->x, (int)object->y, (int)object->z,
                   object->background);
#ifdef SIMULATOR
        if (object->character_model)
            DEBUGF("agds: character object %s definition=%s pose=%s at %d,%d "
                   "direction=%d phase=%d pose_frame=%u\n",
                   object->name, object->character_definition,
                   object->character_pose_descriptor,
                   (int)object->x, (int)object->y,
                   (int)object->character_direction,
                   (int)object->character_phase,
                   (unsigned)object->character_pose_frame);
#endif
#ifdef SIMULATOR
        if (object->click_handler != 0 || object->look_handler != 0)
            DEBUGF("agds: interactive object %s center=%d,%d use=%u look=%u "
                   "alive=%d visible=%d\n",
                   object->name, (int)object->region.center_x,
                   (int)object->region.center_y,
                   (unsigned)object->click_handler,
                   (unsigned)object->look_handler,
                   object->alive, object->visible);
#endif
    }
}

static bool load_first_room(const char *name,
                            char *status, size_t status_size)
{
    bool opening_scene = !rb->strcmp(name, "1864");
    bool loaded;

    loaded = false;
#ifdef SIMULATOR
    if (agds_autostart_screen[0] != '\0') {
        /* A retail room never exists in a freshly zeroed VM.  Reproduce the
         * engine's authored menu bootstrap before changing to an isolated
         * room, so fonts, inventory atlases, globals, and persistent
         * character definitions have the same handles they have in play. */
        loaded = scummvm_agds_vm_start_screen(
                     agds_target, "1009.1011", &agds_script_scene,
                     status, status_size) &&
                 scummvm_agds_vm_run_object_named(
                     "inv.1090", status, status_size) &&
                 scummvm_agds_vm_run_object_named(
                     "main.1004", status, status_size) &&
                 scummvm_agds_vm_change_screen(name, status, status_size);
    } else
#endif
    {
        loaded = scummvm_agds_vm_start_screen(
            agds_target, name, &agds_script_scene, status, status_size);
    }
    if (!loaded) {
        DEBUGF("agds: scripted room %s failed: %s\n", name, status);
        return false;
    }
    /* Simulator AUTOSTART intentionally skips 1009; install the same retail
     * font/system-variable state before the opening room in that path. */
    if (!ensure_retail_fonts(status, status_size))
        return false;
    if (!audio_process_events()) {
        rb->strlcpy(status, "NiBiRu audio event setup failed", status_size);
        return false;
    }
    log_script_scene(name);
    if (!draw_script_scene(opening_scene, status, status_size)) {
        DEBUGF("agds: scripted room %s draw failed: %s\n", name, status);
        return false;
    }
    agds_phase = AGDS_BOOT_ROOM;
    agds_new_game_requested = false;
    agds_menu_message = NULL;
    agds_inventory_open = false;
    agds_inventory_redraw = false;
    agds_inventory_page = 0;
    agds_inventory_selected = -1;
    scummvm_agds_vm_inventory_deselect();
    if (opening_scene) {
        if (!scene_open(status, status_size))
            return false;
        /* Retail presents the loaded animation's first authored pose as part
         * of entering the room.  Waiting for the next host frame exposes an
         * empty room for one refresh and makes Martin appear timing-dependent. */
        if (!scene_step(status, status_size))
            return false;
        DEBUGF("agds: direct scene first pose presented during room load\n");
    }
#ifdef SIMULATOR
    if (agds_save_roundtrip_test && opening_scene) {
        /* AUTOSTART intentionally bypasses retail main/menu.  Re-enter the
         * real inventory bootstrap for this isolated save gate before
         * seeding the two objects that 107a normally adds. */
        if (!scummvm_agds_vm_run_object_named(
                "inv.1090", status, status_size))
            return false;
        scummvm_agds_vm_simulator_seed_save_state();
        if (!scummvm_agds_vm_run_object_named(
                "main.108e", status, status_size))
            return false;
        agds_save_roundtrip_phase = 1;
        DEBUGF("agds: simulator retail save roundtrip started\n");
    }
    DEBUGF("agds: simulator autostart screen %s ready\n", name);
#endif
    rb->snprintf(status, status_size,
                 "NiBiRu room %.28s: AGDS script ready", name);
    return true;
}

static bool load_script_screen(const char *name,
                               char *status, size_t status_size)
{
    bool opening_scene = !rb->strcmp(name, "1864");

    scene_close();
    if (!scummvm_agds_vm_change_screen(name, status, status_size)) {
        DEBUGF("agds: screen transition to %s failed: %s\n", name, status);
        return false;
    }
    /* Resume the room's entry processes before presenting inherited actors.
     * Entry scripts yield once while constructing the object graph, then
     * position/hide the persistent character.  Painting before this pass
     * exposes its coordinates from the previous room (over water at 10e6).
     * change_screen reset the clock, so this does not consume a timed frame. */
    if (!opening_scene && !scummvm_agds_vm_tick(status, status_size))
        return false;
    if (!audio_process_events()) {
        rb->strlcpy(status, "NiBiRu transition audio setup failed",
                    status_size);
        return false;
    }
    log_script_scene(name);
    if (!draw_script_scene(opening_scene, status, status_size))
        return false;
    agds_phase = AGDS_BOOT_ROOM;
    agds_menu_redraw = false;
    agds_scene_redraw = false;
    agds_pointer_x = -1;
    agds_pointer_y = -1;
    agds_menu_message = NULL;
    agds_inventory_open = false;
    agds_inventory_redraw = false;
    agds_inventory_page = 0;
    agds_inventory_selected = -1;
    scummvm_agds_vm_inventory_deselect();
    if (opening_scene) {
        if (!scene_open(status, status_size) ||
            !scene_step(status, status_size))
            return false;
        DEBUGF("agds: direct scene first pose presented during room load\n");
    }
    rb->snprintf(status, status_size,
                 "NiBiRu AGDS transitioned to %.28s", name);
    DEBUGF("agds: SetNextScreen -> %s (%u objects)\n", name,
           (unsigned)agds_script_scene.object_count);
    return true;
}

static bool consume_next_screen(char *status, size_t status_size)
{
    char name[AGDS_ADB_NAME_SIZE];

    if (!scummvm_agds_vm_take_next_screen(name, sizeof(name)))
        return true;
    return load_script_screen(name, status, status_size);
}

static bool consume_load_request(char *status, size_t status_size)
{
    char name[AGDS_ADB_NAME_SIZE];
    int slot;

    if (!scummvm_agds_vm_take_load_request(&slot))
        return true;
    if (!scummvm_agds_vm_load_game(slot, name, sizeof(name),
                                    status, status_size)) {
        DEBUGF("agds: load slot %d ignored: %s\n", slot, status);
        return true;
    }
    agds_new_game_requested = false;
    if (!load_script_screen(name, status, status_size))
        return false;
#ifdef SIMULATOR
    if (agds_save_roundtrip_phase == 2) {
        if (scummvm_agds_vm_inventory_count() != 2 ||
            !scummvm_agds_vm_inventory_contains("inv.10bb") ||
            !scummvm_agds_vm_inventory_contains("inv.112c")) {
            rb->strlcpy(status,
                        "NiBiRu simulator save inventory restore failed",
                        status_size);
            return false;
        }
        agds_save_roundtrip_phase = 3;
        DEBUGF("agds: simulator retail save roundtrip passed inventory=2\n");
    }
#endif
    return true;
}

static bool draw_menu_selector(char *status, size_t status_size)
{
    if (!draw_script_scene(false, status, status_size))
        return false;
    agds_menu_redraw = false;
    rb->strlcpy(status, "NiBiRu retail menu regions and handlers active",
                status_size);
    return true;
}

bool scummvm_agds_runtime_init(const struct scummvm_target *target,
                               struct scummvm_video *video,
                               char *status, size_t status_size)
{
    struct scummvm_agds_resource picture;
    struct scummvm_agds_config config;
    uint32_t width = 0;
    uint32_t height = 0;

    scummvm_agds_runtime_reset();
    agds_audio_output = malloc(sizeof(*agds_audio_output));
    if (agds_audio_output == NULL) {
        rb->strlcpy(status, "No NiBiRu audio memory", status_size);
        return false;
    }
    rb->memset(agds_audio_output, 0, sizeof(*agds_audio_output));
    rb->mutex_init(&agds_audio_output->mutex);
    scummvm_video_set_height(video, 240);
    if (!validate_main_object(target, status, status_size))
        return false;
    if (!scummvm_agds_read_config(target, &config, status, status_size))
        return false;
    agds_target = target;
    agds_video = video;
    if (!load_boot_phase(AGDS_BOOT_TAC, status, status_size)) {
        /* Synthetic format gates intentionally contain only one picture. */
        if (!scummvm_agds_find_first_picture(target, &picture,
                                             status, status_size) ||
            !decode_picture_resource(target, video, &picture,
                                     0, 0, false,
                                     &width, &height,
                                     status, status_size))
            return false;
        rb->strlcpy(agds_picture_name, picture.name,
                    sizeof(agds_picture_name));
        rb->snprintf(status, status_size,
                     "AGDS 2.509 main %uB; %.20s %lux%lu -> 320x240",
                     (unsigned)agds_main_code_size,
                     agds_picture_name, (unsigned long)width,
                     (unsigned long)height);
        agds_phase = AGDS_BOOT_TAC;
    }
    agds_running = true;
#ifdef SIMULATOR
    {
        const char *speed_text = getenv("ROCKPOD_AGDS_TEST_SPEED");
        const char *screen_text = getenv("ROCKPOD_AGDS_AUTOSTART_SCREEN");
        int speed = speed_text != NULL ? rb->atoi(speed_text) : 1;

        if (speed >= 1 && speed <= 16)
            agds_scene_test_speed = (unsigned)speed;
        scummvm_agds_vm_simulator_set_test_speed(agds_scene_test_speed);
        if (screen_text != NULL && screen_text[0] != '\0') {
            rb->strlcpy(agds_autostart_screen, screen_text,
                        sizeof(agds_autostart_screen));
            agds_new_game_requested = true;
        }
    }
    if (getenv("ROCKPOD_AGDS_AUTOSTART") != NULL)
        agds_new_game_requested = true;
    if (getenv("ROCKPOD_AGDS_SAVE_ROUNDTRIP") != NULL)
        agds_save_roundtrip_test = true;
    if (getenv("ROCKPOD_AGDS_FILM_TEST") != NULL)
        agds_film_test_delay = 50;
    if (getenv("ROCKPOD_AGDS_SCENE_REDRAW_TEST") != NULL)
        agds_scene_redraw_test = true;
#endif
    return true;
}

void scummvm_agds_runtime_input(int x, int y, bool click, bool look)
{
    char status[128];
    int canvas_x;
    int canvas_y;
    bool inventory_here;

    if (agds_film.active) {
        if (click || look) {
            film_close(true);
            agds_scene_redraw = true;
        }
        return;
    }

    if (agds_phase != AGDS_BOOT_MENU && agds_phase != AGDS_BOOT_ROOM) {
        if (click && (agds_phase == AGDS_BOOT_TAC ||
                      agds_phase == AGDS_BOOT_IDENTITY))
            agds_advance_requested = true;
        return;
    }
    if (click && agds_phase == AGDS_BOOT_ROOM &&
        scummvm_agds_vm_dialog_advance()) {
        audio_stop_synchronized_voice();
        agds_scene_redraw = true;
        return;
    }
    canvas_x = x * (int)AGDS_CANVAS_WIDTH / 320;
    canvas_y = y * (int)AGDS_CANVAS_HEIGHT / 240;
    inventory_here = agds_phase == AGDS_BOOT_ROOM &&
                     agds_script_scene.user_enabled &&
                     scummvm_agds_vm_inventory_enabled() &&
                     canvas_y >= AGDS_INVENTORY_Y;
    if (inventory_here != agds_inventory_open) {
        agds_inventory_open = inventory_here;
        agds_inventory_redraw = true;
        if (!inventory_here) {
            agds_inventory_selected = -1;
            scummvm_agds_vm_inventory_deselect();
            if (!rb->strcmp(agds_script_scene.name, "1864")) {
                clear_inventory_strip();
                agds_inventory_redraw = false;
            } else {
                agds_scene_redraw = true;
            }
        }
    }
#ifdef SIMULATOR
    if (click && agds_phase == AGDS_BOOT_ROOM)
        DEBUGF("agds: simulator room click at %d,%d (ipod %d,%d)\n",
               canvas_x, canvas_y, x, y);
#endif
    if (inventory_here) {
        if (click || look) {
            unsigned count = scummvm_agds_vm_inventory_count();
            unsigned first = (unsigned)agds_inventory_page *
                             AGDS_INVENTORY_PAGE_ITEMS;
            unsigned max_page = count == 0 ? 0 :
                (count - 1u) / AGDS_INVENTORY_PAGE_ITEMS;

            if (look) {
                unsigned page_slot;
                unsigned item;

                if (canvas_x >= AGDS_INVENTORY_ARROW_WIDTH &&
                    canvas_x < (int)AGDS_CANVAS_WIDTH -
                                   AGDS_INVENTORY_ARROW_WIDTH) {
                    page_slot = (unsigned)(
                        canvas_x - AGDS_INVENTORY_ARROW_WIDTH) /
                        AGDS_INVENTORY_SLOT_WIDTH;
                    item = first + page_slot;
                    if (page_slot < AGDS_INVENTORY_PAGE_ITEMS &&
                        item < count &&
                        !scummvm_agds_vm_inventory_click(
                            item, true, status, sizeof(status)))
                        DEBUGF("agds: inventory look failed: %s\n", status);
                }
            } else if (canvas_x < AGDS_INVENTORY_ARROW_WIDTH) {
                if (agds_inventory_page > 0)
                    agds_inventory_page--;
            } else if (canvas_x >=
                       (int)AGDS_CANVAS_WIDTH -
                           AGDS_INVENTORY_ARROW_WIDTH) {
                if ((unsigned)agds_inventory_page < max_page)
                    agds_inventory_page++;
            } else {
                unsigned page_slot =
                    (unsigned)(canvas_x - AGDS_INVENTORY_ARROW_WIDTH) /
                    AGDS_INVENTORY_SLOT_WIDTH;
                unsigned item = first + page_slot;

                if (page_slot < AGDS_INVENTORY_PAGE_ITEMS && item < count) {
                    if (agds_inventory_selected < 0) {
                        if (scummvm_agds_vm_inventory_select(item))
                            agds_inventory_selected = (int)item;
                    } else if ((unsigned)agds_inventory_selected == item) {
                        if (!scummvm_agds_vm_inventory_click(
                                item, false, status, sizeof(status)))
                            DEBUGF("agds: inventory use failed: %s\n",
                                   status);
                        agds_inventory_selected = -1;
                        scummvm_agds_vm_inventory_deselect();
                    } else {
                        if (!scummvm_agds_vm_inventory_use(
                                (unsigned)agds_inventory_selected, item,
                                status, sizeof(status)))
                            DEBUGF("agds: inventory combine failed: %s\n",
                                   status);
                        agds_inventory_selected = -1;
                        scummvm_agds_vm_inventory_deselect();
                    }
                }
            }
            agds_inventory_redraw = true;
        }
        agds_pointer_x = x;
        agds_pointer_y = y;
        return;
    }
    if (click && agds_inventory_selected >= 0) {
        if (!scummvm_agds_vm_inventory_use_at(
                (unsigned)agds_inventory_selected,
                (int16_t)canvas_x, (int16_t)canvas_y,
                status, sizeof(status)))
            DEBUGF("agds: inventory object use failed: %s\n", status);
        agds_inventory_selected = -1;
        scummvm_agds_vm_inventory_deselect();
        agds_scene_redraw = true;
        return;
    }
    if (x != agds_pointer_x || y != agds_pointer_y) {
        if (!scummvm_agds_vm_pointer((int16_t)canvas_x, (int16_t)canvas_y,
                                     status, sizeof(status))) {
            DEBUGF("agds: pointer VM failed: %s\n", status);
            return;
        }
        agds_pointer_x = x;
        agds_pointer_y = y;
        if (agds_phase == AGDS_BOOT_MENU)
            agds_menu_redraw = true;
    }
    if (look) {
        if (!scummvm_agds_vm_click(
                (int16_t)canvas_x, (int16_t)canvas_y,
                true, status, sizeof(status)))
            DEBUGF("agds: look VM failed: %s\n", status);
        else if (agds_phase == AGDS_BOOT_ROOM)
            agds_scene_redraw = true;
        return;
    }
    if (click &&
        !scummvm_agds_vm_click((int16_t)canvas_x, (int16_t)canvas_y,
                               false, status, sizeof(status)))
        DEBUGF("agds: click VM failed: %s\n", status);
    else if (click && agds_phase == AGDS_BOOT_ROOM)
        agds_scene_redraw = true;
    if (scummvm_agds_vm_take_quit())
        agds_exit_requested = true;
}

bool scummvm_agds_runtime_dialog_overlay(
    struct scummvm_agds_dialog_overlay *overlay)
{
    if (overlay == NULL)
        return false;
    if (agds_film.active && agds_film_subtitle[0] != '\0') {
        rb->strlcpy(overlay->text, agds_film_subtitle,
                    sizeof(overlay->text));
        overlay->npc = true;
        overlay->font_slot = (int16_t)scummvm_agds_vm_dialog_font(true);
    } else {
        if (!scummvm_agds_vm_dialog_text(
                overlay->text, sizeof(overlay->text), &overlay->npc))
            return false;
        overlay->font_slot =
            (int16_t)scummvm_agds_vm_dialog_font(overlay->npc);
    }
    overlay->x = (int16_t)scummvm_agds_vm_system_value("subtitle_x");
    overlay->y = (int16_t)scummvm_agds_vm_system_value("subtitle_y");
    overlay->width =
        (int16_t)scummvm_agds_vm_system_value("subtitle_width");
    overlay->type =
        (int16_t)scummvm_agds_vm_system_value("subtitle_type");
    return true;
}

unsigned scummvm_agds_runtime_object_text_count(void)
{
    if (!agds_running ||
        (agds_phase != AGDS_BOOT_MENU && agds_phase != AGDS_BOOT_ROOM))
        return 0;
    return scummvm_agds_vm_object_text_count();
}

bool scummvm_agds_runtime_object_text(
    unsigned index, struct scummvm_agds_object_text *text)
{
    return agds_running && scummvm_agds_vm_object_text(index, text);
}

bool scummvm_agds_runtime_font(
    unsigned slot, struct scummvm_agds_font *font)
{
    return agds_running && scummvm_agds_vm_font(slot, font);
}

bool scummvm_agds_runtime_main_menu(void)
{
    return agds_running && agds_phase == AGDS_BOOT_MENU &&
           !rb->strcmp(agds_script_scene.name, "1009.1011");
}

bool scummvm_agds_runtime_pointer_visible(void)
{
    return !agds_film.active;
}

void scummvm_agds_runtime_key(const char *key)
{
    char status[128];
    bool handled = false;

    if (agds_phase != AGDS_BOOT_MENU && agds_phase != AGDS_BOOT_ROOM)
        return;
    if (!scummvm_agds_vm_key(key, &handled, status, sizeof(status))) {
        DEBUGF("agds: key VM failed: %s\n", status);
        return;
    }
    if (handled && agds_phase == AGDS_BOOT_MENU)
        agds_menu_redraw = true;
    else if (handled)
        agds_scene_redraw = true;
    if (scummvm_agds_vm_take_quit())
        agds_exit_requested = true;
}

bool scummvm_agds_runtime_frame(char *status, size_t status_size)
{
    if (!agds_running) {
        rb->strlcpy(status, "AGDS runtime is not initialized", status_size);
        return false;
    }
    if (agds_exit_requested) {
        rb->strlcpy(status, "Leaving NiBiRu", status_size);
        return false;
    }
#ifdef SIMULATOR
    if (agds_phase == AGDS_BOOT_ROOM && agds_film_test_delay > 0 &&
        --agds_film_test_delay == 0) {
        struct scummvm_agds_film_event event;

        rb->memset(&event, 0, sizeof(event));
        rb->strlcpy(event.video_name, "dolesa.mjpg",
                    sizeof(event.video_name));
        if (!film_open(&event, status, status_size))
            return false;
    }
#endif
    if (agds_new_game_requested) {
        const char *name = "1864";
#ifdef SIMULATOR
        if (agds_autostart_screen[0] != '\0')
            name = agds_autostart_screen;
#endif
        if (!load_first_room(name, status, status_size))
            return false;
    } else if (agds_phase == AGDS_BOOT_TAC ||
               agds_phase == AGDS_BOOT_IDENTITY) {
        agds_phase_frames++;
        if (agds_advance_requested ||
            agds_phase_frames >= AGDS_LOGO_FRAMES) {
            enum agds_boot_phase next = agds_phase == AGDS_BOOT_TAC ?
                AGDS_BOOT_IDENTITY : AGDS_BOOT_MENU;

            if (!load_boot_phase(next, status, status_size))
                return false;
        }
    } else if (agds_phase == AGDS_BOOT_MENU) {
        audio_update_phases();
        if (!scummvm_agds_vm_tick(status, status_size)) {
            DEBUGF("agds: menu VM tick failed: %s\n", status);
            return false;
        }
        if (scene_visual_signature(true) != agds_drawn_scene_signature)
            agds_menu_redraw = true;
        if (!audio_process_events()) {
            rb->strlcpy(status, "NiBiRu menu audio event failed", status_size);
            return false;
        }
        if (!film_process(status, status_size))
            return false;
        if (agds_film.active)
            return true;
        if (!consume_load_request(status, status_size))
            return false;
        if (!consume_next_screen(status, status_size))
            return false;
        if (agds_phase != AGDS_BOOT_MENU)
            return true;
        if (agds_menu_redraw &&
            !draw_menu_selector(status, status_size))
            return false;
    } else if (agds_phase == AGDS_BOOT_ROOM) {
        audio_update_phases();
        if (!scummvm_agds_vm_tick(status, status_size)) {
            DEBUGF("agds: room VM tick failed: %s\n", status);
            return false;
        }
        if (scene_visual_signature(true) != agds_drawn_scene_signature)
            agds_scene_redraw = true;
        if (agds_inventory_open && !agds_script_scene.user_enabled) {
            agds_inventory_open = false;
            agds_inventory_selected = -1;
            scummvm_agds_vm_inventory_deselect();
            if (!rb->strcmp(agds_script_scene.name, "1864")) {
                clear_inventory_strip();
                agds_inventory_redraw = false;
            } else {
                agds_scene_redraw = true;
            }
        }
#ifdef SIMULATOR
        if (agds_save_roundtrip_phase == 1) {
            scummvm_agds_vm_simulator_scramble_save_state();
            if (!scummvm_agds_vm_run_object_named(
                    "main.108c", status, status_size))
                return false;
            agds_save_roundtrip_phase = 2;
        }
#endif
        if (!audio_process_events()) {
            rb->strlcpy(status, "NiBiRu audio event failed", status_size);
            return false;
        }
        if (!film_process(status, status_size))
            return false;
        if (agds_film.active)
            return true;
        if (!consume_load_request(status, status_size))
            return false;
        if (!consume_next_screen(status, status_size))
            return false;
        if (agds_phase != AGDS_BOOT_ROOM)
            return true;
        if (agds_scene_redraw && agds_scene.frame > 0 &&
            !rb->strcmp(agds_script_scene.name, "1864")) {
            /* intro1864.nbs contains sparse absolute deltas over the current
             * framebuffer.  Clearing and rebuilding the scripted backdrop
             * midway would discard Martin/chair/handset pixels established
             * by prior authored frames.  Its last pose remains the visual
             * owner after the stream closes, until the retail script changes
             * screen; otherwise a late VM redraw erases Martin at random. */
            agds_scene_redraw = false;
            agds_drawn_scene_signature = scene_visual_signature(true);
#ifdef SIMULATOR
            if (agds_scene_redraw_test && !agds_scene.active) {
                DEBUGF("agds: direct scene post-complete redraw preserved\n");
                agds_scene_redraw_test = false;
            }
#endif
        } else if (agds_scene_redraw) {
            if (!draw_script_scene(!rb->strcmp(agds_script_scene.name, "1864"),
                                   status, status_size))
                return false;
            agds_scene_redraw = false;
            if (agds_inventory_open)
                agds_inventory_redraw = true;
        }
        if (!scene_step(status, status_size)) {
            DEBUGF("agds: direct scene tick failed: %s\n", status);
            return false;
        }
        if (agds_inventory_open && agds_inventory_redraw &&
            !draw_inventory_overlay(status, status_size)) {
            DEBUGF("agds: retail inventory draw failed: %s\n", status);
            return false;
        }
    }
    return true;
}

void scummvm_agds_runtime_reset(void)
{
    unsigned index;

    film_close(false);
    scene_close();
    free(agds_background_cache);
    agds_background_cache = NULL;
    agds_background_valid = false;
    if (agds_audio_output != NULL && agds_audio_output->thread != 0) {
        rb->mutex_lock(&agds_audio_output->mutex);
        agds_audio_output->quit = true;
        rb->mutex_unlock(&agds_audio_output->mutex);
        rb->thread_wait(agds_audio_output->thread);
    }
    if (agds_audio_active) {
        rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
        rb->pcmbuf_fade(false, false);
        if (agds_audio_old_frequency != 0)
            rb->mixer_set_frequency(agds_audio_old_frequency);
    }
    for (index = 0; index < AGDS_AUDIO_VOICES; index++)
        audio_voice_close(&agds_audio_voices[index]);
    free(agds_audio_output);
    agds_audio_output = NULL;
    agds_audio_active = false;
    agds_audio_old_frequency = 0;
    scummvm_agds_vm_reset();
    agds_running = false;
    agds_picture_name[0] = '\0';
    agds_main_code_size = 0;
    agds_opcode_base = 0;
    agds_target = NULL;
    agds_video = NULL;
    agds_phase = AGDS_BOOT_TAC;
    agds_phase_frames = 0;
    agds_advance_requested = false;
    agds_exit_requested = false;
    agds_new_game_requested = false;
    agds_menu_message = NULL;
    agds_menu_hover = -1;
    agds_menu_redraw = false;
    agds_scene_redraw = false;
    agds_pointer_x = -1;
    agds_pointer_y = -1;
    agds_inventory_open = false;
    agds_inventory_redraw = false;
    agds_inventory_page = 0;
    agds_inventory_selected = -1;
    agds_scene_test_speed = 1;
#ifdef SIMULATOR
    scummvm_agds_vm_simulator_set_test_speed(1);
    agds_save_roundtrip_test = false;
    agds_save_roundtrip_phase = 0;
    agds_film_test_delay = 0;
    agds_scene_redraw_test = false;
    agds_autostart_screen[0] = '\0';
#endif
    agds_character_cache_size = 0;
    agds_character_cache_request[0] = '\0';
}
