/* Game-owned music, prepared as 44.1 kHz stereo S16LE by the import tool.
 * File I/O belongs to the VM thread. The PCM callback only consumes ready
 * blocks, with a fixed 32 KiB lookahead; it never accesses the filesystem.
 */
#include "music.h"

#define MUSIC_STREAMS 8
#define MUSIC_NAME_BYTES 128
#define MUSIC_BLOCKS 16
#define MUSIC_FRAMES 512
#define MUSIC_BYTES (MUSIC_FRAMES * 4)

static struct
{
    char directory[MAX_PATH];
    char names[MUSIC_STREAMS][MUSIC_NAME_BYTES];
    unsigned char data[MUSIC_BLOCKS][MUSIC_BYTES];
    unsigned int frames[MUSIC_BLOCKS];
    unsigned int count, stream, repeat;
    unsigned int read, write, offset, ready;
    unsigned long samples, underruns, failures, starts;
    int fd;
    bool requested, playing, paused, eof;
} music;

void ig_music_init(const char *game_directory)
{
    rb->memset(&music, 0, sizeof(music));
    music.fd = -1;
    rb->strlcpy(music.directory, game_directory, sizeof(music.directory));
}

unsigned int ig_music_register(const char *name)
{
    unsigned int index = music.count;

    if (!name[0] || name[0] == '/' || rb->strstr(name, "..") ||
        rb->strchr(name, '\\') || rb->strlen(name) >= MUSIC_NAME_BYTES ||
        index >= MUSIC_STREAMS)
        return ~0u;
    rb->strlcpy(music.names[index], name, MUSIC_NAME_BYTES);
    ++music.count;
    return index;
}

void ig_music_stop(void)
{
    rb->pcm_play_lock();
    music.playing = false;
    music.paused = false;
    music.requested = false;
    music.read = music.write = music.ready = music.offset = 0;
    music.eof = false;
    rb->pcm_play_unlock();
    if (music.fd >= 0)
        rb->close(music.fd);
    music.fd = -1;
}

void ig_music_play(unsigned int stream)
{
    if (stream >= music.count)
        return;
    ig_music_stop();
    music.stream = stream;
    music.requested = true;
}

void ig_music_pause(bool pause)
{
    rb->pcm_play_lock();
    music.paused = pause;
    rb->pcm_play_unlock();
}

void ig_music_repeat(unsigned int mode)
{
    music.repeat = MIN(mode, 2u);
}

static bool music_open(void)
{
    char path[MAX_PATH];
    off_t size;

    if (rb->snprintf(path, sizeof(path), "%s/assets/%s.pcm",
                     music.directory, music.names[music.stream]) >=
        (int)sizeof(path))
        return false;
    music.fd = rb->open(path, O_RDONLY);
    if (music.fd < 0)
        return false;
    size = rb->filesize(music.fd);
    if (size <= 0 || size % 4)
    {
        rb->close(music.fd);
        music.fd = -1;
        return false;
    }
    return true;
}

void ig_music_service(void)
{
    unsigned int blocks;

    if (music.requested)
    {
        music.requested = false;
        if (!music_open())
        {
            ++music.failures;
            return;
        }
        ++music.starts;
    }
    if (music.fd < 0)
        return;

    for (blocks = 0; blocks < MUSIC_BLOCKS; ++blocks)
    {
        unsigned int slot;
        ssize_t bytes;

        rb->pcm_play_lock();
        if (music.ready == MUSIC_BLOCKS || music.eof)
        {
            rb->pcm_play_unlock();
            break;
        }
        slot = music.write;
        rb->pcm_play_unlock();

        /* This slot is unpublished: the callback cannot reach it yet. */
        bytes = rb->read(music.fd, music.data[slot], MUSIC_BYTES);
        if (bytes == 0 && music.repeat)
        {
            if (music.repeat == 2)
            {
                rb->close(music.fd);
                music.fd = -1;
                music.stream = (music.stream + 1) % music.count;
                if (!music_open())
                    bytes = -1;
            }
            else if (rb->lseek(music.fd, 0, SEEK_SET) < 0)
                bytes = -1;
            if (bytes == 0)
                bytes = rb->read(music.fd, music.data[slot], MUSIC_BYTES);
        }
        if (bytes <= 0 || bytes % 4)
        {
            rb->pcm_play_lock();
            music.eof = true;
            rb->pcm_play_unlock();
            if (bytes < 0 || bytes % 4)
                ++music.failures;
            break;
        }
        rb->pcm_play_lock();
        music.frames[slot] = bytes / 4;
        music.write = (slot + 1) % MUSIC_BLOCKS;
        ++music.ready;
        music.playing = true;
        rb->pcm_play_unlock();
    }
}

bool ig_music_active(void)
{
    return music.playing && !music.paused;
}

/* Called with PCM serialized, after the destination has been cleared. */
void ig_music_mix(int16_t *output, size_t frames)
{
    size_t frame;

    if (!ig_music_active())
        return;
    for (frame = 0; frame < frames; ++frame)
    {
        const unsigned char *source;

        if (!music.ready)
        {
            if (music.eof)
                music.playing = false;
            else
                ++music.underruns;
            break;
        }
        source = music.data[music.read] + music.offset * 4;
        output[frame * 2] = (int16_t)(source[0] | (uint16_t)source[1] << 8);
        output[frame * 2 + 1] = (int16_t)(source[2] | (uint16_t)source[3] << 8);
        ++music.samples;
        if (++music.offset == music.frames[music.read])
        {
            music.offset = 0;
            music.read = (music.read + 1) % MUSIC_BLOCKS;
            --music.ready;
        }
    }
}

void ig_music_shutdown(void)
{
    /* The caller stops the mixer channel before releasing this workspace. */
    ig_music_stop();
}

void ig_music_report(int fd)
{
    rb->fdprintf(fd, "music=%u:%lu:%lu:%lu:%lu\n", music.count,
                 music.starts, music.samples, music.underruns, music.failures);
}
