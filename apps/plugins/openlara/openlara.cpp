/***************************************************************************
 * OpenLara fixed-point frontend for the iPod 6G Rockbox port.
 * Engine code: Copyright (c) XProger, BSD-2-Clause.
 ***************************************************************************/

#include "openlara_compat.h"
extern "C" {
#include "lib/helper.h"
}
#include "fixed/game.h"

#ifdef SIMULATOR
#include <stdlib.h>
#endif

#define OPENLARA_ROOT ROCKBOX_DIR "/games/openlara"
#define OPENLARA_LEVEL_BYTES (8u * 1024u * 1024u)
#define OPENLARA_AUDIO_BLOCKS 8
#define OPENLARA_AUDIO_START_BLOCKS 3
#define OPENLARA_PATH_BYTES 260

const void *TRACKS_AD4;
const void *TITLE_SCR;
int32 fps;

static uint16 rockbox_palette[256];
static uint8 palette_rgb[256][3];
static fb_data rockbox_frame[FRAME_WIDTH * FRAME_HEIGHT];
static uint8 *level_buffer;
static uint8 *title_buffer;
static uint8 *tracks_buffer;
static size_t arena_left;
static uint8 *arena_next;
static bool level_load_failed;
static bool usb_connected;
static char data_dir[OPENLARA_PATH_BYTES];
static char settings_path[OPENLARA_PATH_BYTES];
static char save_path[OPENLARA_PATH_BYTES];
static int log_fd = -1;
static unsigned long rendered_frames;

extern int8 soundBuffer[];

static int16 *audio_ring;
static int16 audio_silence[SND_SAMPLES * 2];
static volatile int audio_queued;
static volatile int audio_read;
static int audio_write;
static bool audio_started;
static bool audio_ready;
static unsigned audio_old_frequency;

static void *arena_alloc(size_t bytes, size_t alignment)
{
    uintptr_t pos = ((uintptr_t)arena_next + alignment - 1) &
                    ~(uintptr_t)(alignment - 1);
    size_t skipped = pos - (uintptr_t)arena_next;

    if (skipped > arena_left || bytes > arena_left - skipped)
        return NULL;
    arena_next = (uint8 *)(pos + bytes);
    arena_left -= skipped + bytes;
    return (void *)pos;
}

static bool ensure_directory(const char *path)
{
    return rb->dir_exists(path) || rb->mkdir(path) >= 0;
}

static void open_log()
{
    ensure_directory(ROCKBOX_DIR "/logs");
    log_fd = rb->open(ROCKBOX_DIR "/logs/openlara.log",
                      O_WRONLY | O_CREAT | O_TRUNC, 0666);
}

static void dirname_from_parameter(const char *parameter)
{
    const char *slash;

    rb->strlcpy(data_dir, OPENLARA_ROOT, sizeof(data_dir));
    if (!parameter || !parameter[0])
        return;
    slash = rb->strrchr(parameter, '/');
    if (!slash || slash == parameter ||
        (size_t)(slash - parameter) >= sizeof(data_dir))
        return;
    memcpy(data_dir, parameter, slash - parameter);
    data_dir[slash - parameter] = '\0';
}

static long file_size(int fd)
{
    off_t current = rb->lseek(fd, 0, SEEK_CUR);
    off_t end = rb->lseek(fd, 0, SEEK_END);
    rb->lseek(fd, current, SEEK_SET);
    return (long)end;
}

static long load_file(const char *path, void *destination, size_t capacity)
{
    int fd = rb->open(path, O_RDONLY);
    long size;
    ssize_t got;

    if (fd < 0)
        return -1;
    size = file_size(fd);
    if (size <= 0 || (size_t)size > capacity)
    {
        rb->close(fd);
        return -1;
    }
    got = rb->read(fd, destination, size);
    rb->close(fd);
    return got == size ? size : -1;
}

static long load_asset(const char *name, const char *extension,
                       void *destination, size_t capacity)
{
    char path[OPENLARA_PATH_BYTES];
    long size;

    rb->snprintf(path, sizeof(path), "%s/%s.%s", data_dir, name, extension);
    size = load_file(path, destination, capacity);
    if (size >= 0)
        return size;
    rb->snprintf(path, sizeof(path), "%s/levels/%s.%s",
                 data_dir, name, extension);
    return load_file(path, destination, capacity);
}

static bool write_exact_file(const char *path, const void *data, size_t size)
{
    int fd = rb->open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    ssize_t written;

    if (fd < 0)
        return false;
    written = rb->write(fd, data, size);
    rb->close(fd);
    return written == (ssize_t)size;
}

static bool read_exact_file(const char *path, void *data, size_t size)
{
    int fd = rb->open(path, O_RDONLY);
    ssize_t got;

    if (fd < 0)
        return false;
    if (file_size(fd) != (long)size)
    {
        rb->close(fd);
        return false;
    }
    got = rb->read(fd, data, size);
    rb->close(fd);
    return got == (ssize_t)size;
}

int32 osGetSystemTimeMS()
{
    return (int32)((*rb->current_tick * 1000L) / HZ);
}

bool osSaveSettings()
{
    return write_exact_file(settings_path, &gSettings, sizeof(gSettings));
}

bool osLoadSettings()
{
    Settings loaded;

    if (!read_exact_file(settings_path, &loaded, sizeof(loaded)) ||
        loaded.version != SETTINGS_VER)
    {
        if (!tracks_buffer)
            gSettings.audio_music = 0;
        return false;
    }
    gSettings = loaded;
    if (!tracks_buffer)
        gSettings.audio_music = 0;
    return true;
}

bool osCheckSave()
{
    return rb->file_exists(save_path);
}

bool osSaveGame()
{
    int fd;

    if (gSaveGame.dataSize > sizeof(gSaveData))
        return false;
    fd = rb->open(save_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;
    bool ok = rb->write(fd, &gSaveGame, sizeof(gSaveGame)) ==
              (ssize_t)sizeof(gSaveGame) &&
              rb->write(fd, gSaveData, gSaveGame.dataSize) ==
              (ssize_t)gSaveGame.dataSize;
    rb->close(fd);
    return ok;
}

bool osLoadGame()
{
    int fd = rb->open(save_path, O_RDONLY);
    uint32 version;

    if (fd < 0)
        return false;
    bool ok = rb->read(fd, &version, sizeof(version)) ==
              (ssize_t)sizeof(version) && version == SAVEGAME_VER;
    if (ok)
    {
        gSaveGame.version = version;
        ok = rb->read(fd, (uint8 *)&gSaveGame + sizeof(version),
                      sizeof(gSaveGame) - sizeof(version)) ==
             (ssize_t)(sizeof(gSaveGame) - sizeof(version));
    }
    if (ok && gSaveGame.dataSize <= sizeof(gSaveData))
        ok = rb->read(fd, gSaveData, gSaveGame.dataSize) ==
             (ssize_t)gSaveGame.dataSize;
    else
        ok = false;
    rb->close(fd);
    return ok;
}

void osJoyVibrate(int32 index, int32 L, int32 R)
{
    (void)index;
    int strength = X_MAX(L, R) * 100 / 255;

    if (!gSettings.controls_vibration || strength <= 0 ||
        !rb->haptic_feedback || !rb->haptic_feedback_enabled ||
        !rb->haptic_feedback_enabled())
        return;
    rb->haptic_feedback(35, strength);
}

void osSetPalette(const uint16 *palette)
{
    for (int i = 0; i < 256; ++i)
    {
        uint16 p = palette[i];
        unsigned r = (p & 31) * 255 / 31;
        unsigned g = ((p >> 5) & 31) * 255 / 31;
        unsigned b = ((p >> 10) & 31) * 255 / 31;
        palette_rgb[i][0] = r;
        palette_rgb[i][1] = g;
        palette_rgb[i][2] = b;
        rockbox_palette[i] = LCD_RGBPACK(r, g, b);
    }
}

const void *osLoadScreen(LevelID id)
{
    (void)id;
    return TITLE_SCR;
}

const void *osLoadLevel(LevelID id)
{
    const char *name;

    if ((unsigned)id >= LVL_MAX || !gLevelInfo[id].data)
    {
        level_load_failed = true;
        return NULL;
    }
    name = (const char *)gLevelInfo[id].data;
    long size = load_asset(name, "PKD", level_buffer, OPENLARA_LEVEL_BYTES);
    if (size < 0)
    {
        level_load_failed = true;
        return NULL;
    }
    /* PKD serializes a 32-bit Level header followed by relative offsets. */
    if (size < 172)
    {
        level_load_failed = true;
        return NULL;
    }
    const uint16 *counts = (const uint16 *)(level_buffer + 4);
    if (counts[0] == 0 || counts[1] == 0 || counts[1] > MAX_ROOMS ||
        counts[8] > MAX_TEXTURES || counts[9] > MAX_SPRITES ||
        counts[10] > MAX_ITEMS || counts[11] > MAX_CAMERAS)
    {
        level_load_failed = true;
        return NULL;
    }
    const uint32 *offsets = (const uint32 *)(level_buffer + 32);
    for (int i = 0; i < 35; ++i)
    {
        if (offsets[i] >= (uint32)size)
        {
            level_load_failed = true;
            return NULL;
        }
    }
    if (offsets[3] > (uint32)size ||
        counts[1] * 56u > (uint32)size - offsets[3])
    {
        level_load_failed = true;
        return NULL;
    }
    if (log_fd >= 0)
        rb->fdprintf(log_fd, "level name=%s bytes=%ld\n", name, size);
    gLevelID = id;
    return level_buffer;
}

static void audio_get_more(const void **start, size_t *size)
{
    if (audio_queued > 0 && audio_ring)
    {
        *start = audio_ring + audio_read * SND_SAMPLES * 2;
        *size = SND_SAMPLES * 2 * sizeof(int16);
        audio_read = (audio_read + 1) % OPENLARA_AUDIO_BLOCKS;
        audio_queued--;
    }
    else
    {
        *start = audio_silence;
        *size = sizeof(audio_silence);
    }
}

static bool openlara_audio_init()
{
    audio_ring = (int16 *)arena_alloc(OPENLARA_AUDIO_BLOCKS *
                                      SND_SAMPLES * 2 * sizeof(int16), 16);
    if (!audio_ring)
        return false;
    memset(audio_ring, 0, OPENLARA_AUDIO_BLOCKS * SND_SAMPLES *
               2 * sizeof(int16));
    memset(audio_silence, 0, sizeof(audio_silence));
    audio_queued = audio_read = audio_write = 0;
    audio_started = false;
    audio_old_frequency = rb->mixer_get_frequency();
#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->mixer_set_frequency(SND_OUTPUT_FREQ);
    rb->pcmbuf_fade(false, true);
    audio_ready = true;
    return true;
}

static void audio_submit()
{
    int16 *block;

    if (!audio_ready)
        return;
    if (audio_queued >= OPENLARA_AUDIO_BLOCKS - 1)
        return;
    sndFill(soundBuffer);
    block = audio_ring + audio_write * SND_SAMPLES * 2;
    for (int i = 0; i < SND_SAMPLES; ++i)
    {
        int16 sample = ((int)(uint8)soundBuffer[i] - 128) << 8;
        block[i * 2] = sample;
        block[i * 2 + 1] = sample;
    }
    rb->pcm_play_lock();
    audio_queued++;
    rb->pcm_play_unlock();
    audio_write = (audio_write + 1) % OPENLARA_AUDIO_BLOCKS;
    if (!audio_started && audio_queued >= OPENLARA_AUDIO_START_BLOCKS)
    {
        rb->mixer_channel_set_amplitude(PCM_MIXER_CHAN_PLAYBACK,
                                        MIX_AMP_UNITY);
        rb->mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK,
                                    audio_get_more, NULL, 0);
        audio_started = true;
    }
}

static void audio_close()
{
    if (!audio_ready && !audio_started)
        return;
    rb->pcm_play_lock();
    if (rb->mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK) != CHANNEL_STOPPED)
        rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->pcm_play_unlock();
    audio_started = audio_ready = false;
    rb->pcmbuf_fade(false, false);
    if (audio_old_frequency)
        rb->mixer_set_frequency(audio_old_frequency);
#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
}

static void video_present()
{
    const uint8 *source = (const uint8 *)fb;

    for (int i = 0; i < FRAME_WIDTH * FRAME_HEIGHT; ++i)
        rockbox_frame[i] = rockbox_palette[source[i]];
    rb->lcd_bitmap(rockbox_frame, 0, 0, FRAME_WIDTH, FRAME_HEIGHT);
    rb->lcd_update();
    rendered_frames++;
}

#ifdef SIMULATOR
static void dump_test_frame()
{
    const char *enabled = getenv("OPENLARA_TEST_DUMP");
    const uint8 *source = (const uint8 *)fb;
    uint8 row[FRAME_WIDTH * 3];

    if (!enabled || !enabled[0])
        return;
    int fd = rb->open(ROCKBOX_DIR "/logs/openlara-frame.ppm",
                      O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    rb->fdprintf(fd, "P6\n%d %d\n255\n", FRAME_WIDTH, FRAME_HEIGHT);
    for (int y = 0; y < FRAME_HEIGHT; ++y)
    {
        for (int x = 0; x < FRAME_WIDTH; ++x)
        {
            const uint8 *color = palette_rgb[source[y * FRAME_WIDTH + x]];
            row[x * 3] = color[0];
            row[x * 3 + 1] = color[1];
            row[x * 3 + 2] = color[2];
        }
        rb->write(fd, row, sizeof(row));
    }
    rb->close(fd);
}
#endif

static uint32 input_update(int wheel_ticks)
{
    int event;
    int held;
    uint32 result = 0;

    while ((event = rb->button_get(false)) != BUTTON_NONE)
    {
        if (event == SYS_USB_CONNECTED)
        {
            usb_connected = true;
            break;
        }
        int clean = event & ~(BUTTON_REPEAT | BUTTON_REL);
#ifdef BUTTON_SCROLL_FWD
        if (clean == BUTTON_SCROLL_FWD)
            wheel_ticks = 2;
#endif
#ifdef BUTTON_SCROLL_BACK
        if (clean == BUTTON_SCROLL_BACK)
            wheel_ticks = -2;
#endif
    }
    held = rb->button_status();
#ifdef BUTTON_LEFT
    if (held & BUTTON_LEFT) result |= IK_LEFT;
#endif
#ifdef BUTTON_RIGHT
    if (held & BUTTON_RIGHT) result |= IK_RIGHT;
#endif
#ifdef BUTTON_SELECT
    if (held & BUTTON_SELECT) result |= IK_A;
#endif
#ifdef BUTTON_PLAY
    if (held & BUTTON_PLAY) result |= IK_B;
#endif
#ifdef BUTTON_MENU
    if (held & BUTTON_MENU) result |= IK_SELECT;
#endif
    if (wheel_ticks > 0) result |= IK_UP;
    if (wheel_ticks < 0) result |= IK_DOWN;
    return result;
}

static void load_optional_assets()
{
    char path[OPENLARA_PATH_BYTES];
    long size;
    uint8 *raw_title = (uint8 *)arena_alloc(FRAME_WIDTH * FRAME_HEIGHT, 16);

    if (raw_title)
    {
        rb->snprintf(path, sizeof(path), "%s/TITLE.SCR", data_dir);
        size = load_file(path, raw_title, FRAME_WIDTH * FRAME_HEIGHT);
        if (size == FRAME_WIDTH * FRAME_HEIGHT)
            TITLE_SCR = title_buffer = raw_title;
        else if (size == 240 * 160)
        {
            title_buffer = (uint8 *)arena_alloc(FRAME_WIDTH * FRAME_HEIGHT, 16);
            if (title_buffer)
            {
                for (int y = 0; y < FRAME_HEIGHT; ++y)
                    for (int x = 0; x < FRAME_WIDTH; ++x)
                        title_buffer[y * FRAME_WIDTH + x] =
                            raw_title[(y * 160 / FRAME_HEIGHT) * 240 +
                                      x * 240 / FRAME_WIDTH];
                TITLE_SCR = title_buffer;
            }
        }
    }

    rb->snprintf(path, sizeof(path), "%s/TRACKS.AD4", data_dir);
    int fd = rb->open(path, O_RDONLY);
    if (fd >= 0)
    {
        size = file_size(fd);
        if (size > 0 && (tracks_buffer = (uint8 *)arena_alloc(size, 16)))
        {
            if (rb->read(fd, tracks_buffer, size) == size)
                TRACKS_AD4 = tracks_buffer;
            else
                tracks_buffer = NULL;
        }
        rb->close(fd);
    }
}

extern "C" enum plugin_status plugin_start(const void *parameter)
{
    enum plugin_status status = PLUGIN_OK;
    size_t buffer_size = 0;
    int wheel_ticks = 0;
    long next_tick;
    int tick_fraction = 0;
#ifdef SIMULATOR
    unsigned long test_frames = 0;
    unsigned long test_limit = 0;
    const char *test_value = getenv("OPENLARA_TEST_FRAMES");
    const char *test_level = getenv("OPENLARA_TEST_LEVEL");
    if (test_value)
        test_limit = strtoul(test_value, NULL, 10);
#endif

    usb_connected = level_load_failed = false;
    rendered_frames = 0;
    dirname_from_parameter((const char *)parameter);
    ensure_directory(ROCKBOX_DIR "/games");
    ensure_directory(OPENLARA_ROOT);
    rb->snprintf(settings_path, sizeof(settings_path), "%s/openlara.cfg",
                 data_dir);
    rb->snprintf(save_path, sizeof(save_path), "%s/savegame.dat", data_dir);

    arena_next = (uint8 *)rb->plugin_get_audio_buffer(&buffer_size);
    arena_left = buffer_size;
    if (!arena_next || arena_left < 12u * 1024u * 1024u)
    {
        rb->splash(HZ * 2, "OpenLara needs 12 MB free");
        return PLUGIN_ERROR;
    }
    level_buffer = (uint8 *)arena_alloc(OPENLARA_LEVEL_BYTES, 16);
    open_log();
    if (log_fd >= 0)
        rb->fdprintf(log_fd, "start engine=fixed screen=%dx%d memory=%lu\n",
                     FRAME_WIDTH, FRAME_HEIGHT, (unsigned long)buffer_size);
    load_optional_assets();
    if (!level_buffer || !openlara_audio_init())
    {
        rb->splash(HZ * 2, "OpenLara memory setup failed");
        status = PLUGIN_ERROR;
        goto cleanup_buffer;
    }

    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    rb->lcd_update();
    backlight_ignore_timeout();
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    rb->cpu_boost(true);
#endif

    sndInit();
#ifdef SIMULATOR
    if (test_level)
    {
        if (!rb->strcasecmp(test_level, "GYM"))
            gLevelID = LVL_TR1_GYM;
        else if (!rb->strcasecmp(test_level, "LEVEL1"))
            gLevelID = LVL_TR1_1;
        else if (!rb->strcasecmp(test_level, "LEVEL2"))
            gLevelID = LVL_TR1_2;
    }
#endif
    gameInit();
    if (level_load_failed)
    {
        rb->splashf(HZ * 3, "Missing %s/TITLE.PKD", data_dir);
        status = PLUGIN_ERROR;
        goto cleanup_game;
    }

    next_tick = *rb->current_tick;
    while (!usb_connected && !level_load_failed)
    {
        keys = input_update(wheel_ticks);
        if (wheel_ticks > 0) wheel_ticks--;
        if (wheel_ticks < 0) wheel_ticks++;

        gameUpdate(1);
        audio_submit();
        gameRender();
        video_present();

#ifdef SIMULATOR
        if (test_limit && ++test_frames >= test_limit)
            break;
        if (getenv("OPENLARA_TEST_UNTHROTTLED"))
            continue;
#endif
        tick_fraction += HZ;
        next_tick += tick_fraction / 30;
        tick_fraction %= 30;
        if (TIME_BEFORE(*rb->current_tick, next_tick))
            rb->sleep(next_tick - *rb->current_tick);
        else if (*rb->current_tick - next_tick > HZ / 2)
            next_tick = *rb->current_tick;
        rb->yield();
    }

cleanup_game:
#ifdef SIMULATOR
    dump_test_frame();
#endif
    gameFree();
    sndStop();
    audio_close();
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    rb->cpu_boost(false);
#endif
    backlight_use_settings();
cleanup_buffer:
    if (log_fd >= 0)
    {
        rb->fdprintf(log_fd,
                     "exit status=%d frames=%lu load_failed=%d usb=%d\n",
                     status, rendered_frames, level_load_failed ? 1 : 0,
                     usb_connected ? 1 : 0);
        rb->close(log_fd);
        log_fd = -1;
    }
    rb->plugin_release_audio_buffer();
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_clear_display();
    rb->lcd_update();
    return usb_connected ? PLUGIN_USB_CONNECTED : status;
}
