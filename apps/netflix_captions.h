/* Bounded, pre-rendered Netflix captions shared by MPEG and H.264.
 * NFS1: LE magic, cue count, then fixed start/end milliseconds + 2-bit mask.
 * File reads run only in the player's service loop; draw uses cached pixels.
 * Two 3168-byte masks avoid touching playback's shared buffer allocator. */
#ifndef NETFLIX_CAPTIONS_H
#define NETFLIX_CAPTIONS_H
#include "netflix_preferences.h"
#define NF_CAP_W 288
#define NF_CAP_H 44
#define NF_CAP_BYTES (NF_CAP_W * NF_CAP_H / 4)
#define NF_CAP_RECORD (8 + NF_CAP_BYTES)
static struct {
    int fd;
    unsigned count;
    unsigned cue;
    int active;
    bool visible;
    bool enabled;
    bool initialized;
    uint32_t start, end;
    struct mutex lock;
    unsigned char pixels[2][NF_CAP_BYTES];
} nf_caps = { .fd = -1 };

static void nf_caption_close(void)
{
    if (nf_caps.fd >= 0) NF_API(close)(nf_caps.fd);
    nf_caps.fd = -1;
    if (nf_caps.initialized)
    {
        NF_API(mutex_lock)(&nf_caps.lock);
        nf_caps.visible = false;
        NF_API(mutex_unlock)(&nf_caps.lock);
    }
}

static void nf_caption_open(const char *video, bool netflix)
{
    char path[MAX_PATH];
    uint32_t header[2];
    nf_caption_close();
    if (!nf_caps.initialized)
    {
        NF_API(mutex_init)(&nf_caps.lock);
        nf_caps.initialized = true;
    }
    nf_caps.enabled = true;
    NF_API(strlcpy)(path,video,sizeof(path));
    char *dot = NF_API(strrchr)(path,'.');
    if (dot) NF_API(strlcpy)(dot,".nfs",sizeof(path)-(dot-path));
    nf_caps.fd = dot ? NF_API(open)(path,O_RDONLY) : -1;
    if (nf_caps.fd < 0)
    {
        if (!netflix || !(nf_preferences() & NF_SUBTITLES)) return;
        if (NF_API(snprintf)(path,sizeof(path),"%s.nfs",video) >= (int)sizeof(path)) return;
        nf_caps.fd = NF_API(open)(path,O_RDONLY);
        if (nf_caps.fd < 0) return;
    }
    if (NF_API(read)(nf_caps.fd, header, sizeof(header)) != sizeof(header) ||
        header[0] != 0x3153464e || header[1] > 20000 ||
        NF_API(lseek)(nf_caps.fd, 0, SEEK_END) !=
            (off_t)(8 + header[1] * NF_CAP_RECORD))
    {
        nf_caption_close();
        return;
    }
    nf_caps.count = header[1];
    nf_caps.cue = 0;
    nf_caps.start = nf_caps.end = 0;
}

static void nf_caption_service(uint32_t now)
{
    unsigned low = 0, high = nf_caps.count;
    uint32_t times[2];
    uint32_t start = 0, end = UINT32_MAX;
    bool visible = false;
    int slot = 1 - nf_caps.active;
    if (nf_caps.fd < 0 || (now >= nf_caps.start && now < nf_caps.end))
        return;
    /* Normal playback advances sequentially. Only a seek needs the binary
     * search: random reads compete with the MPEG stream on the same disk. */
    if (now >= nf_caps.start)
    {
        unsigned candidate = nf_caps.cue + (nf_caps.visible ? 1 : 0);
        if (candidate >= nf_caps.count)
        {
            low = nf_caps.count;
            goto located;
        }
        if (NF_API(lseek)(nf_caps.fd, 8 + candidate * NF_CAP_RECORD, SEEK_SET) < 0 ||
            NF_API(read)(nf_caps.fd, times, sizeof(times)) != sizeof(times))
            goto failed;
        if (now < times[1])
        {
            low = candidate;
            goto have_times;
        }
        low = candidate + 1;
    }
    /* First cue ending after now; handles forward and backward seeks. */
    while (low < high)
    {
        unsigned mid = low + (high - low) / 2;
        if (NF_API(lseek)(nf_caps.fd, 8 + mid * NF_CAP_RECORD, SEEK_SET) < 0 ||
            NF_API(read)(nf_caps.fd, times, sizeof(times)) != sizeof(times))
            goto failed;
        if (times[1] <= now) low = mid + 1;
        else high = mid;
    }
located:
    if (low < nf_caps.count)
    {
        if (NF_API(lseek)(nf_caps.fd, 8 + low * NF_CAP_RECORD, SEEK_SET) < 0 ||
            NF_API(read)(nf_caps.fd, times, sizeof(times)) != sizeof(times) ||
            times[1] <= times[0]) goto failed;
    have_times:
        if (times[1] <= times[0]) goto failed;
        if (now >= times[0])
        {
            start = times[0]; end = times[1]; visible = true;
            if (NF_API(read)(nf_caps.fd, nf_caps.pixels[slot], NF_CAP_BYTES) != NF_CAP_BYTES)
                goto failed;
        }
        else { start = now; end = times[0]; }
    }
    else start = now;
    NF_API(mutex_lock)(&nf_caps.lock);
    nf_caps.cue = low;
    nf_caps.active = slot;
    nf_caps.visible = visible;
    nf_caps.start = start; nf_caps.end = end;
    NF_API(mutex_unlock)(&nf_caps.lock);
    return;
failed:
    nf_caption_close();
}

static bool nf_caption_snapshot(unsigned char *mask)
{
    if (!nf_caps.initialized) return false;
    NF_API(mutex_lock)(&nf_caps.lock);
    bool visible = nf_caps.visible && nf_caps.enabled;
    if (visible) NF_API(memcpy)(mask,nf_caps.pixels[nf_caps.active],NF_CAP_BYTES);
    NF_API(mutex_unlock)(&nf_caps.lock);
    return visible;
}
static void nf_caption_toggle(void)
{
    if (!nf_caps.initialized) return;
    NF_API(mutex_lock)(&nf_caps.lock);
    nf_caps.enabled = !nf_caps.enabled;
    NF_API(mutex_unlock)(&nf_caps.lock);
}

static void nf_caption_draw(uint8_t * const *planes, int width, int height)
{
    if (!nf_caps.initialized) return;
    NF_API(mutex_lock)(&nf_caps.lock);
    if (nf_caps.visible && nf_caps.enabled)
    {
        const unsigned char *mask = nf_caps.pixels[nf_caps.active];
        static const unsigned char luma[4] = { 0, 16, 140, 235 };
        int left = (width - NF_CAP_W) / 2;
        int top = height - 14 - NF_CAP_H;
        for (int y = 0; y < NF_CAP_H; y++)
        {
            int dy = y + top;
            if (dy < 0 || dy >= height) continue;
            uint8_t *py = planes[0] + dy * width;
            uint8_t *pu = planes[1] + (dy / 2) * (width / 2);
            uint8_t *pv = planes[2] + (dy / 2) * (width / 2);
            const unsigned char *row = mask + y * (NF_CAP_W / 4);
            for (int b = 0; b < NF_CAP_W / 4; b++)
            {
                unsigned packed = row[b];
                /* Transparent groups dominate a caption mask. Skip them
                 * before computing addresses or visiting individual pixels. */
                if (!packed) continue;
                for (int j = 0; j < 4; j++, packed >>= 2)
                {
                    unsigned shade = packed & 3;
                    int dx = left + b * 4 + j;
                    if (!shade || dx < 0 || dx >= width) continue;
                    py[dx] = luma[shade];
                    pu[dx / 2] = pv[dx / 2] = 128;
                }
            }
        }
    }
    NF_API(mutex_unlock)(&nf_caps.lock);
}
#endif
