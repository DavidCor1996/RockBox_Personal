/***************************************************************************
 * Qualified short authentic effects on BEEP. Playback remains untouched.
 ***************************************************************************/

#include "maker_lite.h"

#define ML_AUDIO_HEADER_SIZE 128

static uint16_t audio_u16(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t audio_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool audio_read_exact(int fd, void *destination, size_t size)
{
    unsigned char *out = destination;

    while (size)
    {
        ssize_t count = rb->read(fd, out, size);

        if (count <= 0)
            return false;
        out += count;
        size -= count;
    }
    return true;
}

bool maker_lite_audio_init(void)
{
    unsigned char header[ML_AUDIO_HEADER_SIZE];
    char path[MAX_PATH];
    unsigned char *data;
    off_t file_size;
    uint32_t payload_crc;
    uint32_t rate;
    unsigned count;
    unsigned index;
    int fd;

    rb->snprintf(path, sizeof(path), MAKER_LITE_KIT_ROOT "/%s/audio.mla",
                 maker_lite.level.kit_id);
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;
    file_size = rb->filesize(fd);
    if (file_size <= ML_AUDIO_HEADER_SIZE ||
        file_size > (off_t)(maker_lite.arena_size - maker_lite.arena_used) ||
        !audio_read_exact(fd, header, sizeof(header)))
    {
        rb->close(fd);
        return false;
    }
    rate = audio_u32(header + 8);
    count = audio_u16(header + 6);
    payload_crc = audio_u32(header + 12);
    if (rb->memcmp(header, "MLAU", 4) || audio_u16(header + 4) != 1 ||
        count > MAKER_LITE_AUDIO_EFFECTS ||
        rate != rb->mixer_get_frequency() ||
        rb->strncmp((const char *)header + 16, maker_lite.level.kit_id,
                    ML_ID_SIZE))
    {
        rb->close(fd);
        return false;
    }
    maker_lite.arena_used = (maker_lite.arena_used + 3) & ~(size_t)3;
    if ((size_t)(file_size - ML_AUDIO_HEADER_SIZE) >
        maker_lite.arena_size - maker_lite.arena_used)
    {
        rb->close(fd);
        return false;
    }
    data = maker_lite.arena + maker_lite.arena_used;
    if (!audio_read_exact(fd, data, file_size - ML_AUDIO_HEADER_SIZE))
    {
        rb->close(fd);
        return false;
    }
    rb->close(fd);
    if (ml_crc32(data, file_size - ML_AUDIO_HEADER_SIZE) != payload_crc)
        return false;
    maker_lite.arena_used += file_size - ML_AUDIO_HEADER_SIZE;
    maker_lite.audio_data = data;
    for (index = 0; index < count; index++)
    {
        uint32_t offset = audio_u32(header + 48 + index * 8);
        uint32_t length = audio_u32(header + 52 + index * 8);

        if (offset > (uint32_t)(file_size - ML_AUDIO_HEADER_SIZE) ||
            length > (uint32_t)(file_size - ML_AUDIO_HEADER_SIZE) - offset)
            return false;
        maker_lite.audio_offsets[index] = offset;
        maker_lite.audio_lengths[index] = length;
    }
    rb->mixer_channel_set_amplitude(PCM_MIXER_CHAN_BEEP, MIX_AMP_UNITY);
    maker_lite.audio_ready = true;
    return true;
}

void maker_lite_audio_play(unsigned effect)
{
    if (!maker_lite.audio_ready || effect >= MAKER_LITE_AUDIO_EFFECTS ||
        maker_lite.audio_lengths[effect] == 0)
        return;
    /* The first qualification is intentionally conservative: never overlay
     * game effects on a user's active music session. */
    if (rb->audio_status() & AUDIO_STATUS_PLAY)
        return;
    rb->mixer_channel_stop(PCM_MIXER_CHAN_BEEP);
    rb->mixer_channel_play_data(
        PCM_MIXER_CHAN_BEEP, NULL,
        maker_lite.audio_data + maker_lite.audio_offsets[effect],
        maker_lite.audio_lengths[effect]);
}

void maker_lite_audio_shutdown(void)
{
    long deadline = *rb->current_tick + HZ / 4;

    if (!maker_lite.audio_ready)
        return;
    rb->mixer_channel_stop(PCM_MIXER_CHAN_BEEP);
    rb->mixer_channel_set_buffer_hook(PCM_MIXER_CHAN_BEEP, NULL);
    while (rb->mixer_channel_status(PCM_MIXER_CHAN_BEEP) != CHANNEL_STOPPED &&
           TIME_BEFORE(*rb->current_tick, deadline))
        rb->sleep(1);
    maker_lite.audio_ready = false;
    maker_lite.audio_data = NULL;
}
