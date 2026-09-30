/* Optional TV art service. Draw only publishes a path and binds cached
 * pixels; all filesystem work occurs after an idle WPS action timeout.
 * Workspace: 4096 bytes plus two MAX_PATH strings and scalar state.
 * The payload lives in unused target scanout memory, never core/audio. */
#include "config.h"
#if defined(VIDEOOUT_ENHANCED_TEST) && defined(HAVE_COMPOSITE_VIDEO_OUT) && !defined(SIMULATOR)
#include "videoout_art.h"
#include "videoout.h"
#include "kernel.h"
#include "button.h"
#include "file.h"
#include "audio.h"
#include "metadata.h"
#include "tagcache.h"
#include <string.h>
#include "strlcpy.h"
#include <stdint.h>

static char requested[MAX_PATH], loading[MAX_PATH];
static unsigned char chunk[4096];
static int fd = -1;
static unsigned received;
static uint32_t checksum, expected;
static bool ready, attempted, changed;
static long settled;

static void close_art(void)
{
    if (fd >= 0) close(fd);
    fd = -1;
}

void tv_art_leave(void)
{
    close_art();
    requested[0] = loading[0] = 0;
    ready = attempted = changed = false;
    videoout_art_clear();
}

bool tv_art_changed(void)
{
    bool result = changed;
    changed = false;
    return result;
}

void tv_art_draw(const char *path, const fb_data *pixels, int stride,
                 int x, int y)
{
    if (!path || !*path) return;
    if (strcmp(requested, path))
    {
        strlcpy(requested, path, sizeof(requested));
        ready = false;
        settled = current_tick + HZ;
        videoout_art_clear();
    }
    if (ready && !videoout_art_bind((const uint16_t *)pixels, stride, x, y))
    {
        ready = attempted = false;
        loading[0] = 0;
        settled = current_tick + HZ;
    }
}

static uint32_t le32(const unsigned char *p)
{
    return p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 |
           (uint32_t)p[3]<<24;
}

void tv_art_service(bool idle)
{
    struct mp3entry *track = audio_current_track();
    if (!idle || !button_queue_empty() || button_hold())
    {
        settled = current_tick + HZ;
        return;
    }
    if (!requested[0] || !track || strcmp(track->path, requested) ||
        !videoout_active())
    {
        close_art();
        loading[0] = 0;
        ready = attempted = false;
        videoout_art_clear();
        return;
    }
    if (TIME_BEFORE(current_tick, settled) || !tagcache_is_usable() ||
        tagcache_commit_active())
        return;
    if (strcmp(loading, requested))
    {
        close_art();
        strlcpy(loading, requested, sizeof(loading));
        ready = attempted = false;
    }
    if (ready || (attempted && fd < 0)) return;
    if (fd < 0)
    {
        /* Reuse chunk as path scratch before reading the bounded payload. */
        char *path = (char *)chunk;
        strlcpy(path, requested, sizeof(chunk));
        char *slash = strrchr(path, '/');
        attempted = true;
        if (!slash) return;
        strlcpy(slash+1, "cover.tvart", sizeof(chunk)-(slash+1-path));
        fd = open(path, O_RDONLY);
        if (fd < 0) return;
        if (filesize(fd) != VIDEOOUT_ART_SIZE + 16 ||
            read(fd, chunk, 16) != 16 || memcmp(chunk, "TVART001", 8) ||
            le32(chunk+8) != VIDEOOUT_ART_SIZE)
        {
            close_art();
            return;
        }
        expected = le32(chunk+12);
        checksum = 2166136261u;
        received = 0;
    }
    /* At most 16 KiB per idle service, checking input between each read.
     * No directory enumeration, bitmap decode, or tagcache search. */
    for (int n = 0; n < 4 && button_queue_empty(); n++)
    {
        unsigned count = MIN(sizeof(chunk), VIDEOOUT_ART_SIZE-received);
        if (read(fd, chunk, count) != (ssize_t)count ||
            !videoout_art_write(received, chunk, count))
        {
            close_art();
            videoout_art_clear();
            return;
        }
        for (unsigned i = 0; i < count; i++)
            checksum = (checksum ^ chunk[i]) * 16777619u;
        received += count;
        if (received == VIDEOOUT_ART_SIZE)
        {
            close_art();
            ready = checksum == expected && videoout_art_finish();
            if (!ready) videoout_art_clear();
            changed = ready;
            return;
        }
        yield();
    }
}
#endif
