/***************************************************************************
 * Native Rockbox entry point and runtime services for Zelda3.
 ****************************************************************************/
#include "zelda3.h"
#include "tlsf.h"
#include "upstream/src/assets.h"
#include "upstream/src/audio.h"
#include "upstream/src/config.h"
#include "upstream/src/features.h"
#include "upstream/src/util.h"
#include "upstream/src/zelda_rtl.h"
#include "upstream/snes/ppu.h"
#include "lib/helper.h"
#include <stdarg.h>

struct zelda3_runtime zelda3_rb;
Config g_config;

const uint8 *g_asset_ptrs[kNumberOfAssets];
uint32 g_asset_sizes[kNumberOfAssets];
static uint8 *asset_data;

static const char *runtime_path(const char *path, char *buffer, size_t size)
{
    if (!rb->strcmp(path, "zelda3_assets.dat"))
        return ZELDA3_ASSET_PATH;
    if (!rb->strcmp(path, "saves/sram.dat"))
        return ZELDA3_SRAM_PATH;
    if (!rb->strcmp(path, "saves/sram.bak"))
        return ZELDA3_SRAM_BACKUP_PATH;
    if (!rb->strncmp(path, "saves/", 6))
    {
        rb->snprintf(buffer, size, "%s/%s", ZELDA3_SAVE_DIR, path + 6);
        return buffer;
    }
    return path;
}

static int stream_fd(void *stream)
{
    return (int)(intptr_t)stream - 1;
}

void zelda3_log(const char *format, ...)
{
    va_list ap;
    char line[256];

    if (zelda3_rb.log_fd < 0)
        return;
    va_start(ap, format);
    rb->vsnprintf(line, sizeof(line), format, ap);
    va_end(ap);
    rb->fdprintf(zelda3_rb.log_fd, "%s\n", line);
}

void zelda3_die(const char *message)
{
    zelda3_rb.fatal = true;
    zelda3_rb.quit = true;
    zelda3_log("fatal: %s", message ? message : "unknown error");
}

void zelda3_abort(void)
{
    zelda3_die("upstream abort");
}

void zelda3_exit(int status)
{
    if (status)
        zelda3_rb.fatal = true;
    zelda3_rb.quit = true;
}

void Die(const char *message)
{
    zelda3_die(message);
}

int zelda3_printf(const char *format, ...)
{
    va_list ap;
    char line[256];
    int result;

    va_start(ap, format);
    result = rb->vsnprintf(line, sizeof(line), format, ap);
    va_end(ap);
    zelda3_log("%s", line);
    return result;
}

int zelda3_puts(const char *text)
{
    zelda3_log("%s", text ? text : "");
    return 0;
}

int zelda3_fprintf(void *stream, const char *format, ...)
{
    va_list ap;
    char line[256];
    int result;
    (void)stream;

    va_start(ap, format);
    result = rb->vsnprintf(line, sizeof(line), format, ap);
    va_end(ap);
    zelda3_log("%s", line);
    return result;
}

int zelda3_sprintf(char *buffer, const char *format, ...)
{
    va_list ap;
    int result;

    va_start(ap, format);
    result = rb->vsnprintf(buffer, 4096, format, ap);
    va_end(ap);
    return result;
}

int zelda3_snprintf(char *buffer, size_t size, const char *format, ...)
{
    va_list ap;
    int result;

    va_start(ap, format);
    result = rb->vsnprintf(buffer, size, format, ap);
    va_end(ap);
    return result;
}

int zelda3_vsnprintf(char *buffer, size_t size, const char *format,
                     va_list arguments)
{
    return rb->vsnprintf(buffer, size, format, arguments);
}

char *zelda3_strdup(const char *text)
{
    size_t size = rb->strlen(text) + 1;
    char *copy = tlsf_malloc(size);

    if (copy)
        rb->memcpy(copy, text, size);
    return copy;
}

void *zelda3_fopen(const char *path, const char *mode)
{
    char resolved[MAX_PATH];
    int flags;
    int fd;

    path = runtime_path(path, resolved, sizeof(resolved));
    if (mode[0] == 'r')
        flags = O_RDONLY;
    else if (mode[0] == 'a')
        flags = O_WRONLY | O_CREAT | O_APPEND;
    else
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    fd = rb->open(path, flags, 0666);
    return fd < 0 ? NULL : (void *)(intptr_t)(fd + 1);
}

size_t zelda3_fread(void *ptr, size_t size, size_t count, void *stream)
{
    ssize_t amount;

    if (!stream || !size || !count)
        return 0;
    amount = rb->read(stream_fd(stream), ptr, size * count);
    return amount > 0 ? (size_t)amount / size : 0;
}

size_t zelda3_fwrite(const void *ptr, size_t size, size_t count, void *stream)
{
    ssize_t amount;

    if (!stream || !size || !count)
        return 0;
    amount = rb->write(stream_fd(stream), ptr, size * count);
    return amount > 0 ? (size_t)amount / size : 0;
}

int zelda3_fseek(void *stream, long offset, int whence)
{
    return !stream || rb->lseek(stream_fd(stream), offset, whence) < 0 ?
           -1 : 0;
}

long zelda3_ftell(void *stream)
{
    return stream ? (long)rb->lseek(stream_fd(stream), 0, SEEK_CUR) : -1;
}

int zelda3_fclose(void *stream)
{
    return stream ? rb->close(stream_fd(stream)) : -1;
}

int zelda3_rename(const char *old_path, const char *new_path)
{
    char old_buffer[MAX_PATH];
    char new_buffer[MAX_PATH];
    const char *old_resolved = runtime_path(old_path, old_buffer,
                                             sizeof(old_buffer));
    const char *new_resolved = runtime_path(new_path, new_buffer,
                                             sizeof(new_buffer));
    /* ZeldaWriteSram rotates sram.dat to sram.bak before every write.  Some
     * Rockbox filesystems do not replace an existing destination on rename. */
    rb->remove(new_resolved);
    return rb->rename(old_resolved, new_resolved);
}

MemBlk FindInAssetArray(int asset, int index)
{
    return FindIndexInMemblk((MemBlk){g_asset_ptrs[asset],
                                     g_asset_sizes[asset]}, index);
}

bool zelda3_assets_load(void)
{
    static const uint8 signature[] = { kAssets_Sig };
    int fd = rb->open(ZELDA3_ASSET_PATH, O_RDONLY);
    off_t length;
    uint32 offset;
    int i;

    if (fd < 0)
        return false;
    length = rb->filesize(fd);
    if (length < 88 + kNumberOfAssets * 4)
    {
        rb->close(fd);
        return false;
    }
    asset_data = tlsf_malloc((size_t)length);
    if (!asset_data || rb->read(fd, asset_data, (size_t)length) != length)
    {
        rb->close(fd);
        return false;
    }
    rb->close(fd);
    if (rb->memcmp(asset_data, signature, sizeof(signature)) ||
        *(uint32 *)(asset_data + 80) != kNumberOfAssets)
        return false;
    offset = 88 + kNumberOfAssets * 4 + *(uint32 *)(asset_data + 84);
    for (i = 0; i < kNumberOfAssets; ++i)
    {
        uint32 asset_size = *(uint32 *)(asset_data + 88 + i * 4);
        offset = (offset + 3) & ~3u;
        if ((uint64)offset + asset_size > (uint64)length)
            return false;
        g_asset_sizes[i] = asset_size;
        g_asset_ptrs[i] = asset_data + offset;
        offset += asset_size;
    }
    zelda3_log("assets loaded bytes=%lu", (unsigned long)length);
    return true;
}

void ZeldaApuLock(void)
{
}

void ZeldaApuUnlock(void)
{
}

static void ensure_directories(void)
{
    if (!rb->dir_exists(ZELDA3_DATA_DIR))
        rb->mkdir(ZELDA3_DATA_DIR);
    if (!rb->dir_exists(ZELDA3_SAVE_DIR))
        rb->mkdir(ZELDA3_SAVE_DIR);
}

static bool copy_exact_file(const char *source, const char *destination,
                            size_t expected_size)
{
    uint8 buffer[512];
    int input = rb->open(source, O_RDONLY);
    int output;
    size_t copied = 0;

    if (input < 0 || rb->filesize(input) != (off_t)expected_size)
    {
        if (input >= 0)
            rb->close(input);
        return false;
    }
    output = rb->open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (output < 0)
    {
        rb->close(input);
        return false;
    }
    while (copied < expected_size)
    {
        size_t wanted = MIN(sizeof(buffer), expected_size - copied);
        ssize_t amount = rb->read(input, buffer, wanted);

        if (amount <= 0 || rb->write(output, buffer, (size_t)amount) != amount)
            break;
        copied += (size_t)amount;
    }
    rb->close(input);
    rb->close(output);
    if (copied != expected_size)
    {
        rb->remove(destination);
        return false;
    }
    return true;
}

static void recover_sram_if_needed(void)
{
    int fd = rb->open(ZELDA3_SRAM_PATH, O_RDONLY);
    bool primary_valid = fd >= 0 && rb->filesize(fd) == 8192;

    if (fd >= 0)
        rb->close(fd);
    if (!primary_valid && copy_exact_file(ZELDA3_SRAM_BACKUP_PATH,
                                           ZELDA3_SRAM_PATH, 8192))
        zelda3_log("restored invalid/missing SRAM from backup");
}

static void set_cpu_boost(bool enable)
{
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    if (enable != zelda3_rb.cpu_boosted)
    {
        rb->cpu_boost(enable);
        zelda3_rb.cpu_boosted = enable;
    }
#else
    (void)enable;
#endif
}

static void parse_test_options(void)
{
#ifdef SIMULATOR
    const char *frames = getenv("ZELDA3_TEST_FRAMES");

    if (frames)
        zelda3_rb.test_frames = MAX(1, rb->atoi(frames));
#endif
}

static bool unthrottled_test(void)
{
#ifdef SIMULATOR
    return getenv("ZELDA3_TEST_UNTHROTTLED") != NULL;
#else
    return false;
#endif
}

static void pace_frame(void)
{
    long now;
    long remaining;

    if (unthrottled_test())
        return;
    zelda3_rb.next_frame_scaled += HZ;
    now = *rb->current_tick;
    remaining = zelda3_rb.next_frame_scaled - now * 60;
    if (remaining >= 60)
        rb->sleep(remaining / 60);
    while (!zelda3_rb.quit && *rb->current_tick * 60 <
           zelda3_rb.next_frame_scaled)
        rb->yield();
    now = *rb->current_tick;
    if (now * 60 - zelda3_rb.next_frame_scaled > HZ * 30)
        zelda3_rb.next_frame_scaled = now * 60;
}

static enum plugin_status run_game(void)
{
#ifdef SIMULATOR
    if (getenv("ZELDA3_TEST_CONTROLS"))
    {
        unsigned coverage = 0;
        bool passed = zelda3_input_self_test(&coverage);

        zelda3_log("controls selftest=%s coverage=0x%03x expected=0xfff",
                   passed ? "pass" : "fail", coverage & 0xfff);
        return passed ? PLUGIN_OK : PLUGIN_ERROR;
    }
#endif
    g_config.audio_freq = 32000;
    g_config.audio_channels = 2;
    g_config.msuvolume = 100;
    g_config.features0 = 0;

    if (!zelda3_assets_load())
    {
        rb->splash(HZ * 4,
                   "Missing/invalid /.rockbox/zelda3/zelda3_assets.dat");
        return PLUGIN_ERROR;
    }
    ZeldaInitialize();
    g_zenv.ppu->extraLeftRight = 0;
    g_wanted_zelda_features = 0;
    ZeldaEnableMsu(0);
    ZeldaSetLanguage(NULL);
    recover_sram_if_needed();
    ZeldaReadSram();
    if (!zelda3_audio_init())
        rb->splash(HZ, "Zelda audio unavailable; continuing muted");

    zelda3_rb.profile_start_tick = *rb->current_tick;
    zelda3_rb.next_frame_scaled = zelda3_rb.profile_start_tick * 60;
    while (!zelda3_rb.quit && !zelda3_rb.fatal)
    {
        long started = *rb->current_tick;
        unsigned input = zelda3_input_poll();

        if (zelda3_rb.quit)
            break;
        ZeldaRunFrame((int)input);
        zelda3_audio_frame();
        zelda3_video_draw();
        zelda3_rb.frames++;
        if ((*rb->current_tick - started) * 60 > HZ)
            zelda3_rb.overruns++;
        if (zelda3_rb.test_frames &&
            zelda3_rb.frames >= zelda3_rb.test_frames)
            zelda3_rb.quit = true;
        pace_frame();
    }
    {
        long elapsed = *rb->current_tick - zelda3_rb.profile_start_tick;
        unsigned long fps_x1000 = elapsed > 0 ?
            (zelda3_rb.frames * HZ * 1000) / (unsigned long)elapsed : 0;

        zelda3_log("profile frames=%lu ticks=%ld fps_x1000=%lu "
                   "overruns=%lu rendered=%lu unthrottled=%d",
                   zelda3_rb.frames, elapsed, fps_x1000,
                   zelda3_rb.overruns, zelda3_rb.rendered,
                   unthrottled_test());
    }
    ZeldaWriteSram();
    if (zelda3_rb.usb_connected)
        return PLUGIN_USB_CONNECTED;
    return zelda3_rb.fatal ? PLUGIN_ERROR : PLUGIN_OK;
}

enum plugin_status plugin_start(const void *parameter)
{
    size_t pool_size;
    void *pool;
    enum plugin_status status;
    (void)parameter;

    rb->memset(&zelda3_rb, 0, sizeof(zelda3_rb));
    zelda3_rb.log_fd = -1;
    ensure_directories();
    zelda3_rb.log_fd = rb->open(ZELDA3_LOG_PATH,
        O_WRONLY | O_CREAT | O_TRUNC, 0666);
    pool = rb->plugin_get_audio_buffer(&pool_size);
#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
    if ((uintptr_t)pool < (uintptr_t)plugin_start_addr)
        pool_size = MIN(pool_size,
            (size_t)((uintptr_t)plugin_start_addr - (uintptr_t)pool));
#endif
    if (!pool || init_memory_pool(pool_size, pool) == (size_t)-1)
    {
        rb->splash(HZ * 2, "Zelda3 memory setup failed");
        if (zelda3_rb.log_fd >= 0)
            rb->close(zelda3_rb.log_fd);
        rb->plugin_release_audio_buffer();
        return PLUGIN_ERROR;
    }
    set_cpu_boost(true);
    backlight_ignore_timeout();
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(false);
#endif
    zelda3_log("start pool=%lu", (unsigned long)pool_size);
    parse_test_options();
    status = run_game();
    zelda3_audio_close();
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
    backlight_use_settings();
    set_cpu_boost(false);
    zelda3_log("exit status=%d frames=%lu rendered=%lu overruns=%lu",
               status, zelda3_rb.frames, zelda3_rb.rendered,
               zelda3_rb.overruns);
    if (zelda3_rb.log_fd >= 0)
        rb->close(zelda3_rb.log_fd);
    destroy_memory_pool(pool);
    rb->plugin_release_audio_buffer();
    return status;
}
