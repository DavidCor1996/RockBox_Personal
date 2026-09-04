/***************************************************************************
 * Apple VideoCore H.264/AAC player for iPod Video (5G/5.5G).
 *
 * This is the host half of the media path used by Apple's 1.3 firmware.  It
 * boots the retail VMCS image, implements its read-only VLL file service and
 * uses the passthru service to deliver MP4 samples to Apple's MPlayer,
 * h264dec and aacdec VideoCore modules.  No Apple binary is built into
 * Rockbox; the owner installs the files extracted from Apple's iPod updater.
 ***************************************************************************/
#include "config.h"

#if defined(IPOD_VIDEO) && !defined(SIMULATOR)

#include "bcm2722.h"
#include "button.h"
#include "cpu.h"
#include "file.h"
#include "font.h"
#include "kernel.h"
#include "lcd.h"
#include "mp4_demux.h"
#include "rbpaths.h"
#include "settings.h"
#include "sound.h"
#include "splash.h"
#include "string-extra.h"
#include "system.h"
#include "timefuncs.h"
#include "video_playback.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VIDEO5_VMCS             ROCKBOX_DIR "/videocore/vmcs.bin"
#define VIDEO5_VLL_DIR          ROCKBOX_DIR "/videocore"
#define VIDEO5_VMCS_BYTES       201376u
#define VIDEO5_FRAME_MAGIC      0xf1a55a1fu
#define VIDEO5_MAX_FRAME        1408u
#define VIDEO5_MAX_SAMPLES      400000u
#define VIDEO5_MAX_AUDIO_SAMPLES 600000u
#define VIDEO5_MAX_SAMPLE_BYTES (2u * 1024u * 1024u)
#define VIDEO5_SERVICE_TIMEOUT  (HZ * 8)
#define VIDEO5_COMMAND_TIMEOUT  (HZ * 12)

#define VIDEO5_TAG_GENCMD       1
#define VIDEO5_TAG_DISPLAY      2
#define VIDEO5_TAG_HOSTFS       5
#define VIDEO5_TAG_PASSTHRU     7

#define VIDEO5_DISPLAY_LCD      0
#define VIDEO5_DISPLAY_TV       2

#define VIDEO5_DISPLAY_START    1
#define VIDEO5_DISPLAY_SUBMIT   2
#define VIDEO5_DISPLAY_ADD      3
#define VIDEO5_DISPLAY_REMOVE   4
#define VIDEO5_DISPLAY_CREATE   8
#define VIDEO5_DISPLAY_DELETE   9
#define VIDEO5_MAX_SURFACES     4

#define VIDEO5_FS_CLOSE         0x41
#define VIDEO5_FS_SEEK          0x43
#define VIDEO5_FS_READ          0x44
#define VIDEO5_FS_OPEN          0x4c

#define VIDEO5_PT_SETUP         0x60
#define VIDEO5_PT_SAMPLE        0x61
#define VIDEO5_PT_STOP          0x62
#define VIDEO5_PT_STATUS        0x40
#define VIDEO5_PT_SETUP_REPLY   0x41
#define VIDEO5_PT_NEXT_SAMPLE   0x42
#define VIDEO5_PT_COMPLETE      0x44

struct video5_channel
{
    uint32_t base;
    uint32_t record;
    uint16_t tx_start;
    uint16_t tx_end;
    uint16_t rx_start;
    uint16_t rx_end;
    uint32_t sequence;
    bool valid;
};

struct video5_frame
{
    uint32_t sequence;
    uint32_t opcode;
    uint16_t length;
    uint8_t payload[VIDEO5_MAX_FRAME - 16];
};

struct video5_pool
{
    uint8_t *cursor;
    uint8_t *end;
};

struct video5_timing
{
    uint32_t run;
    uint32_t in_run;
    uint64_t ticks;
};

struct video5_track
{
    bool audio;
    uint32_t sample;
    uint32_t count;
    struct video5_timing timing;
    uint8_t *data;
    uint32_t capacity;
    uint32_t size;
    uint32_t sent;
    uint32_t sequence;
    bool pending;
};

struct video5_surface
{
    uint32_t resource;
    uint32_t address;
    uint32_t element;
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    int16_t x;
    int16_t y;
    bool visible;
};

struct video5_player
{
    struct video5_channel gencmd;
    struct video5_channel display;
    struct video5_channel hostfs;
    struct video5_channel passthru;
    struct mp4v_demux_res *demux;
    int media_fd;
    struct video5_track audio;
    struct video5_track video;
    uint32_t slot_address[2];
    uint32_t slot_capacity[2];
    uint8_t output_display;
    bool setup_complete;
    bool finished;
    bool eof_sent;
    bool tv_dac_enabled;
    bool vc_policy_set;
};

enum video5_style
{
    VIDEO5_STYLE_STOCK = 0,
    VIDEO5_STYLE_YOUTUBE,
    VIDEO5_STYLE_YOUTUBE_LIVE,
    VIDEO5_STYLE_NETFLIX,
    VIDEO5_STYLE_INSTAGRAM,
    VIDEO5_STYLE_INSTAGRAM_FEED,
    VIDEO5_STYLE_TIKTOK,
    VIDEO5_STYLE_MAPS,
};

struct video5_launch
{
    const char *path;
    enum video5_style style;
    bool live;
    bool allow_seek;
    uint32_t live_epoch;
    bool tiktok;
    bool instagram_feed;
    bool fill;
    bool instagram_feed_expanded;
};

struct video5_host_file
{
    int fd;
};

static struct video5_host_file video5_files[8];

static uint16_t video5_get_u16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t video5_get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void video5_put_u16(uint8_t *p, uint16_t value)
{
    p[0] = value;
    p[1] = value >> 8;
}

static void video5_put_u32(uint8_t *p, uint32_t value)
{
    p[0] = value;
    p[1] = value >> 8;
    p[2] = value >> 16;
    p[3] = value >> 24;
}

static void *video5_pool_take(struct video5_pool *pool, size_t size,
                              size_t alignment)
{
    uintptr_t current = (uintptr_t)pool->cursor;
    uintptr_t aligned;

    if (alignment == 0)
        return NULL;
    aligned = (current + alignment - 1) & ~(uintptr_t)(alignment - 1);
    if (aligned < current || aligned > (uintptr_t)pool->end ||
        size > (size_t)(pool->end - (uint8_t *)aligned))
        return NULL;
    pool->cursor = (uint8_t *)aligned + size;
    return (void *)aligned;
}

static uint16_t video5_ring_advance(uint16_t pointer, uint16_t amount,
                                    uint16_t start, uint16_t end)
{
    uint32_t next = pointer + amount;

    while (next >= end)
        next = start + next - end;
    return (uint16_t)next;
}

static uint16_t video5_ring_used(uint16_t read_pointer,
                                 uint16_t write_pointer,
                                 uint16_t start, uint16_t end)
{
    if (write_pointer >= read_pointer)
        return write_pointer - read_pointer;
    return (end - read_pointer) + (write_pointer - start);
}

static bool video5_ring_read(const struct video5_channel *channel,
                             uint16_t pointer, void *destination,
                             uint16_t length, bool tx)
{
    uint16_t start = tx ? channel->tx_start : channel->rx_start;
    uint16_t end = tx ? channel->tx_end : channel->rx_end;
    uint8_t *out = destination;

    while (length != 0)
    {
        uint16_t chunk = MIN(length, (uint16_t)(end - pointer));

        if (!bcm2722_read_buffer(channel->base + pointer, out, chunk))
            return false;
        out += chunk;
        length -= chunk;
        pointer = pointer + chunk == end ? start : pointer + chunk;
    }
    return true;
}

static bool video5_ring_write(const struct video5_channel *channel,
                              uint16_t pointer, const void *source,
                              uint16_t length)
{
    const uint8_t *in = source;

    while (length != 0)
    {
        uint16_t chunk = MIN(length,
                             (uint16_t)(channel->tx_end - pointer));

        if (!bcm2722_write_buffer(channel->base + pointer, in, chunk))
            return false;
        in += chunk;
        length -= chunk;
        pointer = pointer + chunk == channel->tx_end ?
                  channel->tx_start : pointer + chunk;
    }
    return true;
}

static bool video5_channel_send_sequence(struct video5_channel *channel,
                                         uint32_t sequence,
                                         uint32_t opcode,
                                         const void *payload,
                                         uint16_t length)
{
    uint8_t frame[VIDEO5_MAX_FRAME] CACHEALIGN_ATTR;
    uint16_t read_pointer;
    uint16_t write_pointer;
    uint16_t used;
    uint16_t total = 16 + ((length + 15) & ~15);
    uint16_t ring_size;

    if (!channel->valid || total > sizeof(frame))
        return false;
    read_pointer = bcm2722_read16(channel->record + 0x10);
    write_pointer = bcm2722_read16(channel->record + 0x20);
    ring_size = channel->tx_end - channel->tx_start;
    if (read_pointer < channel->tx_start || read_pointer >= channel->tx_end ||
        write_pointer < channel->tx_start || write_pointer >= channel->tx_end)
        return false;
    used = video5_ring_used(read_pointer, write_pointer,
                            channel->tx_start, channel->tx_end);
    if (total + 16 > ring_size - used)
        return false;

    memset(frame, 0, total);
    video5_put_u32(frame, VIDEO5_FRAME_MAGIC);
    video5_put_u32(frame + 4, sequence);
    video5_put_u32(frame + 8, opcode);
    video5_put_u16(frame + 12, length);
    if (length != 0 && payload != NULL)
        memcpy(frame + 16, payload, length);
    if (!video5_ring_write(channel, write_pointer, frame, total))
        return false;
    write_pointer = video5_ring_advance(write_pointer, total,
                                        channel->tx_start, channel->tx_end);
    bcm2722_write16(channel->record + 0x20, write_pointer);
    bcm2722_notify();
    return true;
}

static bool video5_channel_send(struct video5_channel *channel,
                                uint32_t opcode, const void *payload,
                                uint16_t length, uint32_t *sequence_out)
{
    uint32_t sequence = (channel->sequence + 1) & 0x7fffffffu;

    if (!video5_channel_send_sequence(channel, sequence, opcode,
                                      payload, length))
        return false;
    channel->sequence = sequence;
    if (sequence_out != NULL)
        *sequence_out = sequence;
    return true;
}

static bool video5_channel_receive(struct video5_channel *channel,
                                   struct video5_frame *frame)
{
    uint8_t header[16] CACHEALIGN_ATTR;
    uint16_t read_pointer;
    uint16_t write_pointer;
    uint16_t available;
    uint16_t total;

    if (!channel->valid)
        return false;
    read_pointer = bcm2722_read16(channel->record + 0x30);
    write_pointer = bcm2722_read16(channel->record + 0x40);
    if (read_pointer < channel->rx_start || read_pointer >= channel->rx_end ||
        write_pointer < channel->rx_start || write_pointer >= channel->rx_end)
        return false;
    available = video5_ring_used(read_pointer, write_pointer,
                                 channel->rx_start, channel->rx_end);
    if (available < 16 ||
        !video5_ring_read(channel, read_pointer, header, sizeof(header), false))
        return false;
    if (video5_get_u32(header) != VIDEO5_FRAME_MAGIC)
        return false;
    frame->sequence = video5_get_u32(header + 4);
    frame->opcode = video5_get_u32(header + 8);
    frame->length = video5_get_u16(header + 12);
    total = 16 + ((frame->length + 15) & ~15);
    if (total > available || total > VIDEO5_MAX_FRAME)
        return false;
    if (frame->length != 0 &&
        !video5_ring_read(channel,
            video5_ring_advance(read_pointer, 16,
                                channel->rx_start, channel->rx_end),
            frame->payload, (frame->length + 15) & ~15, false))
        return false;
    read_pointer = video5_ring_advance(read_pointer, total,
                                       channel->rx_start, channel->rx_end);
    bcm2722_write16(channel->record + 0x30, read_pointer);
    return true;
}

static bool video5_channel_from_tag(uint32_t base, unsigned tag,
                                    struct video5_channel *channel)
{
    unsigned i;

    memset(channel, 0, sizeof(*channel));
    for (i = 0; i < 8; i++)
    {
        uint16_t offset = bcm2722_read16(base + i * 2);
        uint32_t record;

        if (offset == 0)
            continue;
        record = base + offset;
        if (bcm2722_read16(record + 4) != tag)
            continue;
        channel->base = base;
        channel->record = record;
        channel->tx_start = bcm2722_read16(record + 6);
        channel->tx_end = bcm2722_read16(record + 8);
        channel->rx_start = bcm2722_read16(record + 10);
        channel->rx_end = bcm2722_read16(record + 12);
        channel->valid = channel->tx_start < channel->tx_end &&
                         channel->rx_start < channel->rx_end &&
                         (channel->tx_start & 15) == 0 &&
                         (channel->tx_end & 15) == 0 &&
                         (channel->rx_start & 15) == 0 &&
                         (channel->rx_end & 15) == 0;
        return channel->valid;
    }
    return false;
}

static bool video5_discover_services(struct video5_player *player)
{
    long deadline = current_tick + VIDEO5_SERVICE_TIMEOUT;

    while (TIME_BEFORE(current_tick, deadline))
    {
        if (bcm2722_read32(0x1f0 + 8) == 1)
        {
            uint32_t base = bcm2722_read32(0x1f0 + 12);

            if (base != 0 && (base & 3) == 0 &&
                video5_channel_from_tag(base, VIDEO5_TAG_GENCMD,
                                        &player->gencmd) &&
                video5_channel_from_tag(base, VIDEO5_TAG_DISPLAY,
                                        &player->display) &&
                video5_channel_from_tag(base, VIDEO5_TAG_HOSTFS,
                                        &player->hostfs) &&
                video5_channel_from_tag(base, VIDEO5_TAG_PASSTHRU,
                                        &player->passthru))
                return true;
        }
        sleep(1);
    }
    return false;
}

static const char *video5_basename(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');

    if (backslash != NULL && (slash == NULL || backslash > slash))
        slash = backslash;
    return slash == NULL ? path : slash + 1;
}

static const char *video5_vll_name(const char *name)
{
    static const char * const allowed[] = {
        "mplayer.vll", "h264dec.vll", "aacdec.vll",
        "passthruhandler.vll", "mpg4dec.vll",
    };
    unsigned i;

    name = video5_basename(name);
    for (i = 0; i < ARRAYLEN(allowed); i++)
        if (!strcasecmp(name, allowed[i]))
            return allowed[i];
    return NULL;
}

static int video5_host_open(const char *requested)
{
    char path[MAX_PATH];
    const char *name = video5_vll_name(requested);
    unsigned i;

    if (name == NULL)
        return -1;
    for (i = 0; i < ARRAYLEN(video5_files); i++)
    {
        if (video5_files[i].fd >= 0)
            continue;
        snprintf(path, sizeof(path), "%s/%s", VIDEO5_VLL_DIR, name);
        video5_files[i].fd = open(path, O_RDONLY);
        if (video5_files[i].fd < 0)
            return -1;
        return (int)i + 1;
    }
    return -1;
}

static int video5_host_fd(uint32_t handle)
{
    if (handle == 0 || handle > ARRAYLEN(video5_files))
        return -1;
    return video5_files[handle - 1].fd;
}

static void video5_host_close_all(void)
{
    unsigned i;

    for (i = 0; i < ARRAYLEN(video5_files); i++)
    {
        if (video5_files[i].fd >= 0)
            close(video5_files[i].fd);
        video5_files[i].fd = -1;
    }
}

static bool video5_pump_hostfs(struct video5_player *player)
{
    struct video5_frame frame;
    uint8_t reply[1040] CACHEALIGN_ATTR;
    uint16_t reply_length = 4;
    int32_t result = -1;

    if (!video5_channel_receive(&player->hostfs, &frame))
        return true;
    memset(reply, 0, sizeof(reply));
    if (frame.opcode == VIDEO5_FS_OPEN && frame.length > 16 &&
        memchr(frame.payload + 16, '\0', frame.length - 16) != NULL)
    {
        result = video5_host_open((const char *)frame.payload + 16);
    }
    else if (frame.opcode == VIDEO5_FS_READ && frame.length >= 12)
    {
        int fd = video5_host_fd(video5_get_u32(frame.payload));
        uint32_t count = video5_get_u32(frame.payload + 4);
        uint32_t size = video5_get_u32(frame.payload + 8);
        uint64_t requested = (uint64_t)count * size;

        if (fd >= 0)
        {
            ssize_t bytes;

            if (requested > sizeof(reply) - 16)
                requested = sizeof(reply) - 16;
            bytes = read(fd, reply + 16, (size_t)requested);
            result = bytes < 0 ? -1 : bytes;
            reply_length = bytes < 0 ? 16 : 16 + bytes;
        }
        else
            reply_length = 16;
    }
    else if (frame.opcode == VIDEO5_FS_SEEK && frame.length >= 12)
    {
        int fd = video5_host_fd(video5_get_u32(frame.payload));
        int32_t offset = (int32_t)video5_get_u32(frame.payload + 4);
        uint32_t whence = video5_get_u32(frame.payload + 8);
        off_t position = fd < 0 ? -1 : lseek(fd, offset, whence);

        result = position < 0 ? -1 : position;
    }
    else if (frame.opcode == VIDEO5_FS_CLOSE && frame.length >= 4)
    {
        uint32_t handle = video5_get_u32(frame.payload);
        int fd = video5_host_fd(handle);

        if (fd >= 0)
        {
            result = close(fd);
            video5_files[handle - 1].fd = -1;
        }
    }
    video5_put_u32(reply, (uint32_t)result);
    return video5_channel_send_sequence(&player->hostfs, frame.sequence,
                                        frame.opcode, reply, reply_length);
}

static bool video5_gencmd(struct video5_player *player, const char *command)
{
    struct video5_frame frame;
    uint32_t sequence;
    long deadline;

    if (!video5_channel_send(&player->gencmd, 1, command,
                             strlen(command) + 1, &sequence))
        return false;
    deadline = current_tick + VIDEO5_COMMAND_TIMEOUT;
    while (TIME_BEFORE(current_tick, deadline))
    {
        if (!video5_pump_hostfs(player))
            return false;
        if (video5_channel_receive(&player->gencmd, &frame))
        {
            if (frame.sequence != sequence)
                continue;
            if (frame.length >= sizeof(frame.payload))
                return false;
            frame.payload[frame.length] = '\0';
            return strstr((const char *)frame.payload, "error=") == NULL ||
                   strstr((const char *)frame.payload, "error=0") != NULL;
        }
        sleep(1);
    }
    return false;
}

static uint8_t video5_detect_output_display(void)
{
    /* The 5G diagnostic screen identifies GPIOA bit 4 as dock mode. Apple's
     * player selects one MPlayer display at launch: 0 for LCD or 2 for TV. */
    return (GPIOA_INPUT_VAL & 0x10) != 0 ?
           VIDEO5_DISPLAY_TV : VIDEO5_DISPLAY_LCD;
}

static bool video5_set_region(struct video5_player *player, bool fill)
{
    char command[80];

    snprintf(command, sizeof(command),
             "mp_region display=%u dest=fullscreen mode=%s",
             player->output_display, fill ? "fill" : "letterbox");
    return video5_gencmd(player, command);
}

static bool video5_enable_output(struct video5_player *player, bool fill)
{
    if (!video5_set_region(player, fill))
        return false;

    if (player->output_display == VIDEO5_DISPLAY_TV)
    {
        if (!video5_gencmd(player, "display_control 2 dac=1 encoding=5"))
            return false;
        player->tv_dac_enabled = true;
    }
    return true;
}

static bool video5_set_vc_playback_power(struct video5_player *player)
{
    if (player->vc_policy_set)
        return true;

    /* Apple's 5G MPlayer installs this VideoCore policy before select/play.
     * The VMCS session ends at teardown, so it only needs to be set once. */
    if (!video5_gencmd(player, "pm_set_policy min"))
        return false;
    player->vc_policy_set = true;
    return true;
}

static uint32_t video5_serialize_descriptor(uint32_t *out, uint8_t id,
                                            uint32_t fourcc,
                                            uint32_t bitrate,
                                            const uint8_t *codecdata,
                                            uint32_t codecdata_length,
                                            uint32_t value18,
                                            uint32_t value1c)
{
    uint32_t word = 0;
    unsigned i;

    out[word++] = id;
    out[word++] = fourcc;
    out[word++] = bitrate;
    out[word++] = 0;
    out[word++] = codecdata_length;
    for (i = 0; i < codecdata_length; i++)
        out[word++] = codecdata[i];
    out[word++] = value18;
    out[word++] = value1c;
    out[word++] = 1;
    out[word++] = 0;
    /* The remaining fields are the stream-crypto contract.  Apple's
     * ordinary iTunes-created movies are unencrypted, so the crypto type,
     * key length, key material, IV and final control word are all zero. */
    out[word++] = 0;
    for (i = 0; i < 32 + 16; i++)
        out[word++] = 0;
    out[word++] = 0;
    return word;
}

static uint32_t video5_average_bitrate(const uint32_t *sizes,
                                       uint32_t sample_count,
                                       const struct mp4v_stts_entry *stts,
                                       uint32_t stts_count,
                                       uint32_t timescale)
{
    uint64_t bytes = 0;
    uint64_t ticks = 0;
    uint64_t bits_per_second;
    uint32_t i;

    if (sizes == NULL || sample_count == 0 || stts == NULL ||
        stts_count == 0 || timescale == 0)
        return 0;
    for (i = 0; i < sample_count; i++)
        bytes += sizes[i];
    for (i = 0; i < stts_count; i++)
        ticks += (uint64_t)stts[i].sample_count * stts[i].sample_delta;
    if (ticks == 0)
        return 0;
    bits_per_second = bytes * 8u * timescale / ticks;
    return bits_per_second > UINT32_MAX ? UINT32_MAX : bits_per_second;
}

static bool video5_setup_passthru(struct video5_player *player,
                                  uint8_t *workspace)
{
    uint32_t *setup = (uint32_t *)workspace;
    struct video5_frame frame;
    uint32_t word = 8;
    long deadline;

    if (8u + 118u + player->demux->audio_codecdata_len +
        player->demux->codecdata_len > 0x520u / sizeof(*setup))
        return false;
    memset(setup, 0, 0x520);
    setup[0] = 2;
    setup[2] = 1;
    setup[4] = 1;
    setup[5] = 1;
    word += video5_serialize_descriptor(
        setup + word, 0, 0x00414143u,
        video5_average_bitrate(
            player->demux->audio_sample_sizes,
            player->demux->audio_num_samples,
            player->demux->audio_stts,
            player->demux->audio_num_stts,
            player->demux->audio_timescale),
        player->demux->audio_codecdata,
        player->demux->audio_codecdata_len,
        player->demux->audio_sample_rate,
        player->demux->audio_channels);
    word += video5_serialize_descriptor(
        setup + word, 1, 0x48323634u,
        video5_average_bitrate(
            player->demux->sample_sizes,
            player->demux->num_samples,
            player->demux->stts,
            player->demux->num_stts,
            player->demux->timescale),
        player->demux->codecdata, player->demux->codecdata_len,
        player->demux->width, player->demux->height);
    if (word * sizeof(*setup) > 0x520 ||
        !video5_channel_send(&player->passthru, VIDEO5_PT_SETUP,
                             setup, 0x520, NULL))
        return false;

    deadline = current_tick + VIDEO5_COMMAND_TIMEOUT;
    while (TIME_BEFORE(current_tick, deadline))
    {
        if (!video5_pump_hostfs(player))
            return false;
        if (video5_channel_receive(&player->passthru, &frame))
        {
            if (frame.opcode == VIDEO5_PT_SETUP_REPLY && frame.length >= 24)
            {
                player->slot_address[1] = video5_get_u32(frame.payload + 8);
                player->slot_capacity[1] = video5_get_u32(frame.payload + 12);
                player->slot_address[0] = video5_get_u32(frame.payload + 16);
                player->slot_capacity[0] = video5_get_u32(frame.payload + 20);
                player->setup_complete =
                    player->slot_address[0] != 0 &&
                    player->slot_address[1] != 0 &&
                    player->slot_capacity[0] != 0 &&
                    player->slot_capacity[1] != 0 &&
                    (player->slot_address[0] & 15) == 0 &&
                    (player->slot_address[1] & 15) == 0 &&
                    player->slot_capacity[0] >= 16 &&
                    player->slot_capacity[1] >= 16;
                return player->setup_complete;
            }
        }
        sleep(1);
    }
    return false;
}

static void video5_timing_for_sample(const struct mp4v_stts_entry *entries,
                                     uint32_t entry_count,
                                     uint32_t sample,
                                     struct video5_timing *timing)
{
    uint32_t run;
    uint32_t remaining = sample;

    memset(timing, 0, sizeof(*timing));
    for (run = 0; run < entry_count; run++)
    {
        uint32_t count = entries[run].sample_count;

        if (remaining < count)
        {
            timing->run = run;
            timing->in_run = remaining;
            timing->ticks += (uint64_t)remaining * entries[run].sample_delta;
            return;
        }
        timing->ticks += (uint64_t)count * entries[run].sample_delta;
        remaining -= count;
    }
    timing->run = entry_count;
}

static void video5_timing_advance(const struct mp4v_stts_entry *entries,
                                  uint32_t entry_count,
                                  struct video5_timing *timing)
{
    if (timing->run >= entry_count)
        return;
    timing->ticks += entries[timing->run].sample_delta;
    timing->in_run++;
    if (timing->in_run >= entries[timing->run].sample_count)
    {
        timing->run++;
        timing->in_run = 0;
    }
}

static uint32_t video5_sample_for_ms(const struct mp4v_stts_entry *entries,
                                     uint32_t entry_count,
                                     uint32_t timescale, uint32_t count,
                                     uint32_t target_ms)
{
    uint64_t target;
    uint64_t ticks = 0;
    uint32_t sample = 0;
    uint32_t run;

    if (timescale == 0 || count == 0)
        return 0;
    target = (uint64_t)target_ms * timescale / 1000u;
    for (run = 0; run < entry_count; run++)
    {
        uint64_t run_ticks = (uint64_t)entries[run].sample_count *
                             entries[run].sample_delta;

        if (target < ticks + run_ticks)
        {
            if (entries[run].sample_delta != 0)
                sample += (target - ticks) / entries[run].sample_delta;
            break;
        }
        ticks += run_ticks;
        sample += entries[run].sample_count;
    }
    return sample < count ? sample : count - 1;
}

static uint32_t video5_duration_ms(const struct mp4v_demux_res *demux)
{
    uint64_t ticks = 0;
    uint32_t i;

    if (demux->timescale == 0)
        return 0;
    for (i = 0; i < demux->num_stts; i++)
        ticks += (uint64_t)demux->stts[i].sample_count *
                 demux->stts[i].sample_delta;
    return ticks * 1000u / demux->timescale;
}

static void video5_seek(struct video5_player *player, uint32_t target_ms)
{
    uint32_t video_sample = video5_sample_for_ms(
        player->demux->stts, player->demux->num_stts,
        player->demux->timescale, player->demux->num_samples, target_ms);
    uint32_t audio_sample = video5_sample_for_ms(
        player->demux->audio_stts, player->demux->audio_num_stts,
        player->demux->audio_timescale, player->demux->audio_num_samples,
        target_ms);

    while (video_sample > 0 &&
           !mp4v_is_keyframe(player->demux, video_sample))
        video_sample--;
    player->video.sample = video_sample;
    player->video.pending = false;
    player->video.sent = 0;
    video5_timing_for_sample(player->demux->stts,
                             player->demux->num_stts, video_sample,
                             &player->video.timing);
    player->audio.sample = audio_sample;
    player->audio.pending = false;
    player->audio.sent = 0;
    video5_timing_for_sample(player->demux->audio_stts,
                             player->demux->audio_num_stts, audio_sample,
                             &player->audio.timing);
    player->eof_sent = false;
}

static bool video5_load_sample(struct video5_player *player,
                               struct video5_track *track)
{
    uint32_t offset;
    uint32_t size;
    int status;

    if (track->sample >= track->count)
        return false;
    status = track->audio ?
        mp4v_get_audio_sample_offset(player->demux, track->sample,
                                     &offset, &size) :
        mp4v_get_sample_offset(player->demux, track->sample, &offset, &size);
    if (status < 0 || size == 0 || size > track->capacity ||
        lseek(player->media_fd, offset, SEEK_SET) < 0 ||
        read(player->media_fd, track->data, size) != (ssize_t)size)
        return false;
    memset(track->data + size, 0, ((size + 15) & ~15) - size);
    track->size = size;
    track->sent = 0;
    track->pending = true;
    return true;
}

static bool video5_send_eof(struct video5_player *player)
{
    uint32_t payload[2] = { 0, 0 };

    if (player->eof_sent)
        return true;
    player->eof_sent = video5_channel_send(
        &player->passthru, VIDEO5_PT_SAMPLE,
        payload, sizeof(payload), NULL);
    return player->eof_sent;
}

static bool video5_send_sample(struct video5_player *player,
                               struct video5_track *track)
{
    uint8_t payload[48] CACHEALIGN_ATTR;
    unsigned slot = track->audio ? 0 : 1;
    uint32_t capacity = player->slot_capacity[slot];
    uint32_t remaining;
    uint32_t bytes;
    uint32_t padded;
    uint32_t flag;
    uint32_t timescale;
    uint64_t timestamp_ms;

    if (!track->pending && !video5_load_sample(player, track))
        return false;
    remaining = track->size - track->sent;
    /* RetailOS rounds every BCM bulk write up to a 16-byte boundary. */
    capacity &= ~15u;
    bytes = MIN(remaining, capacity);
    padded = (bytes + 15) & ~15;
    if (bytes == 0 || padded > track->capacity ||
        !bcm2722_write_buffer(player->slot_address[slot],
                              track->data + track->sent, padded))
        return false;
    flag = bytes == remaining ? (track->sent == 0 ? 2 : 3) :
                                (track->sent == 0 ? 0 : 1);
    memset(payload, 0, sizeof(payload));
    video5_put_u32(payload, track->audio ? 0 : 1);
    /* RetailOS marks every AAC access unit with one and forwards the video
     * sample's sync flag in this halfword-derived field. */
    video5_put_u32(payload + 4,
                   track->audio || mp4v_is_keyframe(player->demux,
                                                    track->sample) ? 1 : 0);
    /* RetailOS's MP4 readers normalize both tracks to one millisecond clock
     * before the passthru call.  This is independent of the container's mvhd
     * edit timescale: the stock seek paths subtract literal 2000 and 20000
     * windows from this same normalized clock.  Raw MP4 media ticks are not
     * interchangeable (AAC is 44.1 kHz while video commonly uses 30 kHz);
     * forwarding them directly makes MPlayer race the tracks and produces
     * sped-up/cut audio. */
    timescale = track->audio ? player->demux->audio_timescale :
                               player->demux->timescale;
    timestamp_ms = timescale == 0 ? 0 :
        track->timing.ticks * 1000u / timescale;
    video5_put_u32(payload + 8,
                   timestamp_ms > UINT32_MAX ? UINT32_MAX : timestamp_ms);
    video5_put_u32(payload + 12, track->sequence++);
    video5_put_u32(payload + 20, bytes);
    video5_put_u32(payload + 24, flag);
    video5_put_u32(payload + 28, player->slot_address[slot]);
    /* These four words are stream-crypto metadata, not a clock.  RetailOS
     * sends zeroes for the unencrypted files produced by iTunes. */
    video5_put_u32(payload + 32, 0);
    video5_put_u32(payload + 36, 0);
    video5_put_u32(payload + 40, 0);
    video5_put_u32(payload + 44, 0);
    if (!video5_channel_send(&player->passthru, VIDEO5_PT_SAMPLE,
                             payload, sizeof(payload), NULL))
        return false;
    track->sent += bytes;
    if (track->sent == track->size)
    {
        track->pending = false;
        track->sample++;
        if (track->audio)
            video5_timing_advance(player->demux->audio_stts,
                                  player->demux->audio_num_stts,
                                  &track->timing);
        else
            video5_timing_advance(player->demux->stts,
                                  player->demux->num_stts,
                                  &track->timing);
    }
    return true;
}

static struct video5_track *video5_next_track(struct video5_player *player,
                                               int requested)
{
    bool audio_left = player->audio.sample < player->audio.count ||
                      player->audio.pending;
    bool video_left = player->video.sample < player->video.count ||
                      player->video.pending;

    if (requested == 0 && audio_left)
        return &player->audio;
    if (requested == 1 && video_left)
        return &player->video;
    if (!audio_left)
        return video_left ? &player->video : NULL;
    if (!video_left)
        return &player->audio;
    return player->audio.timing.ticks * player->demux->timescale <=
           player->video.timing.ticks * player->demux->audio_timescale ?
           &player->audio : &player->video;
}

static bool video5_pump_passthru(struct video5_player *player)
{
    struct video5_frame frame;

    if (!video5_channel_receive(&player->passthru, &frame))
        return true;
    if (frame.opcode == VIDEO5_PT_STATUS)
    {
        uint32_t status = 4;

        return video5_channel_send_sequence(&player->passthru,
                                             frame.sequence, 1,
                                             &status, sizeof(status));
    }
    if (frame.opcode == VIDEO5_PT_NEXT_SAMPLE)
    {
        int requested = frame.length >= 8 ?
            (int32_t)video5_get_u32(frame.payload + 4) : -1;
        struct video5_track *track = video5_next_track(player, requested);

        return track == NULL ? video5_send_eof(player) :
                               video5_send_sample(player, track);
    }
    if (frame.opcode == VIDEO5_PT_COMPLETE)
        player->finished = true;
    return true;
}

static bool video5_display_call(struct video5_player *player,
                                uint32_t opcode, const void *payload,
                                uint16_t length, uint32_t *result,
                                uint32_t *address)
{
    struct video5_frame frame;
    uint32_t sequence;
    long deadline;

    if (!video5_channel_send(&player->display, opcode, payload, length,
                             &sequence))
        return false;
    deadline = current_tick + VIDEO5_COMMAND_TIMEOUT;
    while (TIME_BEFORE(current_tick, deadline))
    {
        if (!video5_pump_hostfs(player) ||
            !video5_pump_passthru(player))
            return false;
        if (video5_channel_receive(&player->display, &frame))
        {
            if (frame.sequence != sequence || frame.opcode != opcode)
                continue;
            if (frame.length < 4)
                return false;
            if (result != NULL)
                *result = video5_get_u32(frame.payload);
            if (address != NULL)
            {
                if (frame.length < 8)
                    return false;
                *address = video5_get_u32(frame.payload + 4);
            }
            return true;
        }
        sleep(1);
    }
    return false;
}

static bool video5_surface_create(struct video5_player *player,
                                  struct video5_surface *surface,
                                  int x, int y, int width, int height)
{
    uint8_t payload[32] CACHEALIGN_ATTR;

    if (width <= 0 || height <= 0 || width > LCD_WIDTH ||
        height > LCD_HEIGHT || x < 0 || y < 0 ||
        x + width > LCD_WIDTH || y + height > LCD_HEIGHT)
        return false;
    if (surface->resource != 0)
        return surface->x == x && surface->y == y &&
               surface->width == width && surface->height == height;

    memset(payload, 0, sizeof(payload));
    /* This is RetailOS FUN_00286ca8's exact tag-2 opcode-8 request.
     * Type 1 is RGB565 and its pitch is aligned to four bytes by
     * FUN_00286a1c.  A zero address asks VideoCore to allocate storage. */
    video5_put_u32(payload, (uint32_t)(uintptr_t)&surface->resource);
    payload[4] = 1;
    video5_put_u32(payload + 8, width);
    video5_put_u32(payload + 12, height);
    surface->pitch = (width * 2u + 3u) & ~3u;
    video5_put_u32(payload + 16, surface->pitch);
    payload[28] = 1;
    if (!video5_display_call(player, VIDEO5_DISPLAY_CREATE, payload,
                             sizeof(payload), &surface->resource,
                             &surface->address) ||
        surface->resource == 0 || surface->address == 0)
    {
        memset(surface, 0, sizeof(*surface));
        return false;
    }
    surface->x = x;
    surface->y = y;
    surface->width = width;
    surface->height = height;
    return true;
}

static bool video5_surface_remove(struct video5_player *player,
                                  struct video5_surface *surface)
{
    uint8_t payload[16] CACHEALIGN_ATTR;
    uint32_t ignored;

    if (!surface->visible)
        return true;
    memset(payload, 0, sizeof(payload));
    video5_put_u32(payload, surface->element);
    if (!video5_display_call(player, VIDEO5_DISPLAY_REMOVE, payload,
                             sizeof(payload), &ignored, NULL))
        return false;
    surface->visible = false;
    surface->element = 0;
    return true;
}

static bool video5_surface_add(struct video5_player *player,
                               struct video5_surface *surface)
{
    uint8_t payload[32] CACHEALIGN_ATTR;

    memset(payload, 0, sizeof(payload));
    /* RetailOS FUN_00286b6c serializes x/y, the resource handle, source
     * origin and size in this order.  Zeroes retain the stock no-transform,
     * no-crop behavior. */
    video5_put_u32(payload, (uint32_t)(int32_t)surface->x);
    video5_put_u32(payload + 4, (uint32_t)(int32_t)surface->y);
    video5_put_u32(payload + 12, surface->resource);
    video5_put_u16(payload + 24, surface->width);
    video5_put_u16(payload + 26, surface->height);
    if (!video5_display_call(player, VIDEO5_DISPLAY_ADD, payload,
                             sizeof(payload), &surface->element, NULL) ||
        surface->element == 0)
        return false;
    surface->visible = true;
    return true;
}

static fb_data video5_rgb(int red, int green, int blue)
{
    return LCD_RGBPACK(red, green, blue);
}

static void video5_rgb_rect(fb_data *pixels, int pitch, int surface_width,
                            int surface_height, int x, int y,
                            int width, int height, fb_data color)
{
    int row;
    int column;

    for (row = MAX(0, y); row < MIN(surface_height, y + height); row++)
        for (column = MAX(0, x);
             column < MIN(surface_width, x + width); column++)
            pixels[row * pitch + column] = color;
}

static int video5_rgb_text_width(const char *text)
{
    struct font *font = font_get(FONT_SYSFIXED);
    int width = 0;

    if (font == NULL)
        return 0;
    while (*text != '\0')
        width += font_get_width(font, (unsigned char)*text++);
    return width;
}

static void video5_rgb_text(fb_data *pixels, int pitch, int surface_width,
                            int surface_height, int x, int y,
                            const char *text, fb_data color)
{
    struct font *font = font_get(FONT_SYSFIXED);

    if (font == NULL || font->depth != 0)
        return;
    while (*text != '\0' && x < surface_width)
    {
        unsigned char ch = *text++;
        int glyph_width = font_get_width(font, ch);
        const unsigned char *bits = font_get_bits(font, ch);
        int column;

        for (column = 0; column < glyph_width; column++)
        {
            const unsigned char *source = bits + column;
            int row;

            for (row = 0; row < (int)font->height; row++)
                if (source[(row >> 3) * glyph_width] &
                    (1u << (row & 7)) && x + column >= 0 &&
                    x + column < surface_width &&
                    y + row >= 0 && y + row < surface_height)
                    pixels[(y + row) * pitch + x + column] = color;
        }
        x += glyph_width;
    }
}

static void video5_time_text(uint32_t milliseconds, char *text, size_t size)
{
    uint32_t seconds = milliseconds / 1000u;

    if (seconds >= 3600u)
        snprintf(text, size, "%lu:%02lu:%02lu",
                 (unsigned long)(seconds / 3600u),
                 (unsigned long)((seconds / 60u) % 60u),
                 (unsigned long)(seconds % 60u));
    else
        snprintf(text, size, "%lu:%02lu",
                 (unsigned long)(seconds / 60u),
                 (unsigned long)(seconds % 60u));
}

static void video5_surface_render(struct video5_surface *surface,
                                  fb_data *pixels,
                                  const struct video5_launch *launch,
                                  bool paused, uint32_t position_ms,
                                  uint32_t duration_ms)
{
    char current[20];
    char duration[20];
    int pitch = surface->pitch / sizeof(*pixels);
    int bar_width = surface->width - 16;
    int fill_width = duration_ms == 0 ? 0 :
        (int)((uint64_t)MIN(position_ms, duration_ms) * bar_width /
              duration_ms);
    fb_data white = video5_rgb(255, 255, 255);

    memset(pixels, 0, surface->pitch * surface->height);
    video5_time_text(position_ms, current, sizeof(current));
    video5_time_text(duration_ms, duration, sizeof(duration));
    if (launch->style == VIDEO5_STYLE_YOUTUBE_LIVE)
    {
        video5_rgb_rect(pixels, pitch, surface->width, surface->height,
                        0, 0, surface->width, surface->height,
                        video5_rgb(204, 0, 0));
        video5_rgb_text(pixels, pitch, surface->width, surface->height,
                        6, 3, "LIVE", white);
    }
    else if (launch->style == VIDEO5_STYLE_YOUTUBE)
    {
        int duration_width = video5_rgb_text_width(duration);

        video5_rgb_rect(pixels, pitch, surface->width, surface->height,
                        0, 0, surface->width, surface->height,
                        video5_rgb(35, 35, 35));
        video5_rgb_text(pixels, pitch, surface->width, surface->height,
                        6, 7, paused ? "PAUSE" : ">", white);
        video5_rgb_text(pixels, pitch, surface->width, surface->height,
                        34, 7, current, white);
        video5_rgb_text(pixels, pitch, surface->width, surface->height,
                        surface->width - 6 - duration_width, 7,
                        duration, white);
        video5_rgb_rect(pixels, pitch, surface->width, surface->height,
                        56, surface->height - 8,
                        surface->width - 112, 3,
                        video5_rgb(92, 92, 92));
        video5_rgb_rect(pixels, pitch, surface->width, surface->height,
                        56, surface->height - 8,
                        (surface->width - 112) * fill_width / bar_width,
                        3, video5_rgb(204, 0, 0));
    }
    else if (launch->style == VIDEO5_STYLE_NETFLIX)
    {
        int duration_width = video5_rgb_text_width(duration);

        video5_rgb_rect(pixels, pitch, surface->width, surface->height,
                        0, 0, surface->width, surface->height,
                        video5_rgb(20, 20, 20));
        video5_rgb_rect(pixels, pitch, surface->width, surface->height,
                        0, 0, surface->width, 3,
                        video5_rgb(180, 19, 29));
        video5_rgb_text(pixels, pitch, surface->width, surface->height,
                        7, 9, "NETFLIX", video5_rgb(180, 19, 29));
        video5_rgb_text(pixels, pitch, surface->width, surface->height,
                        8, 28, current, white);
        video5_rgb_text(pixels, pitch, surface->width, surface->height,
                        surface->width - 8 - duration_width, 28,
                        duration, white);
        video5_rgb_rect(pixels, pitch, surface->width, surface->height,
                        8, surface->height - 10, bar_width, 4,
                        video5_rgb(72, 72, 72));
        video5_rgb_rect(pixels, pitch, surface->width, surface->height,
                        8, surface->height - 10, fill_width, 4,
                        video5_rgb(180, 19, 29));
    }
    else if (launch->style == VIDEO5_STYLE_TIKTOK)
    {
        video5_rgb_rect(pixels, pitch, surface->width, surface->height,
                        0, 0, surface->width, surface->height,
                        video5_rgb(12, 12, 16));
        if (surface->y == 0)
        {
            video5_rgb_text(pixels, pitch, surface->width, surface->height,
                            9, 8, "TikTok", white);
            video5_rgb_text(pixels, pitch, surface->width, surface->height,
                            126, 8, "Following  |  For You", white);
        }
        else
        {
            video5_rgb_text(pixels, pitch, surface->width, surface->height,
                            9, 9, "@creator", white);
            video5_rgb_text(pixels, pitch, surface->width, surface->height,
                            9, 28, "Video", white);
            video5_rgb_text(pixels, pitch, surface->width, surface->height,
                            surface->width - 52, 9, "LIKE", white);
        }
    }
    else if (launch->style == VIDEO5_STYLE_INSTAGRAM ||
             launch->style == VIDEO5_STYLE_INSTAGRAM_FEED)
    {
        video5_rgb_rect(pixels, pitch, surface->width, surface->height,
                        0, 0, surface->width, surface->height,
                        video5_rgb(43, 79, 107));
        video5_rgb_text(pixels, pitch, surface->width, surface->height,
                        8, 4, surface->y == 0 ? "Instagram" :
                        "Home       Favorites       Profile", white);
    }
    else
    {
        int duration_width = video5_rgb_text_width(duration);

        video5_rgb_rect(pixels, pitch, surface->width, surface->height,
                        0, 0, surface->width, surface->height,
                        video5_rgb(25, 21, 35));
        video5_rgb_text(pixels, pitch, surface->width, surface->height,
                        8, 7, paused ? "PAUSED" : "PLAYING", white);
        video5_rgb_text(pixels, pitch, surface->width, surface->height,
                        surface->width - 8 - duration_width, 7,
                        duration, white);
        video5_rgb_rect(pixels, pitch, surface->width, surface->height,
                        8, surface->height - 10, bar_width, 4,
                        video5_rgb(70, 65, 80));
        video5_rgb_rect(pixels, pitch, surface->width, surface->height,
                        8, surface->height - 10, fill_width, 4,
                        video5_rgb(220, 220, 225));
    }
}

static unsigned video5_overlay_layout(const struct video5_launch *launch,
                                      bool visible,
                                      struct video5_surface *surfaces)
{
    if (launch->style == VIDEO5_STYLE_MAPS)
        return 0;
    if (launch->style == VIDEO5_STYLE_YOUTUBE_LIVE)
    {
        surfaces[0].x = 6;
        surfaces[0].y = LCD_HEIGHT - 22;
        surfaces[0].width = 48;
        surfaces[0].height = 17;
        return 1;
    }
    if (launch->style == VIDEO5_STYLE_TIKTOK)
    {
        surfaces[0].x = 0;
        surfaces[0].y = 0;
        surfaces[0].width = LCD_WIDTH;
        surfaces[0].height = 35;
        surfaces[1].x = 0;
        surfaces[1].y = LCD_HEIGHT - 66;
        surfaces[1].width = LCD_WIDTH;
        surfaces[1].height = 66;
        return 2;
    }
    if (launch->style == VIDEO5_STYLE_INSTAGRAM ||
        launch->style == VIDEO5_STYLE_INSTAGRAM_FEED)
    {
        surfaces[0].x = 0;
        surfaces[0].y = 0;
        surfaces[0].width = LCD_WIDTH;
        surfaces[0].height = 24;
        surfaces[1].x = 0;
        surfaces[1].y = LCD_HEIGHT - 18;
        surfaces[1].width = LCD_WIDTH;
        surfaces[1].height = 18;
        return 2;
    }
    if (!visible)
        return 0;
    surfaces[0].x = 0;
    surfaces[0].width = LCD_WIDTH;
    if (launch->style == VIDEO5_STYLE_NETFLIX)
    {
        surfaces[0].y = LCD_HEIGHT - 58;
        surfaces[0].height = 58;
    }
    else if (launch->style == VIDEO5_STYLE_YOUTUBE)
    {
        surfaces[0].y = LCD_HEIGHT - 28;
        surfaces[0].height = 28;
    }
    else
    {
        surfaces[0].y = LCD_HEIGHT - 34;
        surfaces[0].height = 34;
    }
    return 1;
}

static bool video5_overlay_update(struct video5_player *player,
                                  struct video5_surface *surfaces,
                                  fb_data *pixels,
                                  const struct video5_launch *launch,
                                  bool paused, uint32_t position_ms,
                                  uint32_t duration_ms, bool visible)
{
    struct video5_surface wanted[VIDEO5_MAX_SURFACES];
    uint32_t ignored;
    unsigned count;
    unsigned i;

    memset(wanted, 0, sizeof(wanted));
    count = video5_overlay_layout(launch, visible, wanted);
    for (i = 0; i < count; i++)
    {
        if (!video5_surface_create(player, &surfaces[i],
                                   wanted[i].x, wanted[i].y,
                                   wanted[i].width, wanted[i].height))
            return false;
    }
    if (!video5_display_call(player, VIDEO5_DISPLAY_START,
                             NULL, 0, &ignored, NULL))
        return false;
    for (i = 0; i < VIDEO5_MAX_SURFACES; i++)
        if (!video5_surface_remove(player, &surfaces[i]))
            return false;
    for (i = 0; i < count; i++)
    {
        video5_surface_render(&surfaces[i], pixels, launch, paused,
                              position_ms, duration_ms);
        if (!bcm2722_write_buffer(surfaces[i].address, pixels,
                                  surfaces[i].pitch * surfaces[i].height) ||
            !video5_surface_add(player, &surfaces[i]))
            return false;
    }
    return video5_display_call(player, VIDEO5_DISPLAY_SUBMIT,
                               NULL, 0, &ignored, NULL);
}

static void video5_overlay_destroy(struct video5_player *player,
                                   struct video5_surface *surfaces)
{
    uint8_t payload[16] CACHEALIGN_ATTR;
    uint32_t ignored;
    unsigned i;
    bool any = false;

    for (i = 0; i < VIDEO5_MAX_SURFACES; i++)
        any |= surfaces[i].resource != 0;
    if (!any)
        return;

    if (player->display.valid &&
        video5_display_call(player, VIDEO5_DISPLAY_START,
                            NULL, 0, &ignored, NULL))
    {
        for (i = 0; i < VIDEO5_MAX_SURFACES; i++)
            video5_surface_remove(player, &surfaces[i]);
        video5_display_call(player, VIDEO5_DISPLAY_SUBMIT,
                            NULL, 0, &ignored, NULL);
    }
    for (i = 0; i < VIDEO5_MAX_SURFACES; i++)
    {
        if (surfaces[i].resource == 0)
            continue;
        memset(payload, 0, sizeof(payload));
        video5_put_u32(payload, surfaces[i].resource);
        video5_display_call(player, VIDEO5_DISPLAY_DELETE, payload,
                            sizeof(payload), &ignored, NULL);
        memset(&surfaces[i], 0, sizeof(surfaces[i]));
    }
}

static bool video5_parse_launch(const char *parameter,
                                struct video5_launch *launch)
{
    memset(launch, 0, sizeof(*launch));
    launch->path = parameter;
    launch->style = VIDEO5_STYLE_STOCK;
    launch->allow_seek = true;
    if (!strncmp(parameter, "youtube-live:", 13))
    {
        const char *value = parameter + 13;
        const char *separator = strchr(value, ':');

        if (separator == NULL || separator[1] == '\0')
            return false;
        launch->live = true;
        launch->allow_seek = false;
        launch->style = VIDEO5_STYLE_YOUTUBE_LIVE;
        launch->live_epoch = strtoul(value, NULL, 10);
        launch->path = separator + 1;
        return true;
    }
    if (!strncmp(parameter, "youtube-app:", 12))
    {
        launch->style = VIDEO5_STYLE_YOUTUBE;
        launch->path = parameter + 12;
    }
    else if (!strncmp(parameter, "youtube:", 8))
    {
        launch->style = VIDEO5_STYLE_YOUTUBE;
        launch->path = parameter + 8;
    }
    else if (!strncmp(parameter, "netflix-restart:", 16))
    {
        launch->style = VIDEO5_STYLE_NETFLIX;
        launch->path = parameter + 16;
    }
    else if (!strncmp(parameter, "netflix:", 8))
    {
        launch->style = VIDEO5_STYLE_NETFLIX;
        launch->path = parameter + 8;
    }
    else if (!strncmp(parameter, "instagram-feed:", 15))
    {
        launch->style = VIDEO5_STYLE_INSTAGRAM_FEED;
        launch->instagram_feed = true;
        launch->allow_seek = false;
        launch->path = parameter + 15;
    }
    else if (!strncmp(parameter, "instagram-app:", 14))
    {
        launch->style = VIDEO5_STYLE_INSTAGRAM;
        launch->path = parameter + 14;
    }
    else if (!strncmp(parameter, "tiktok-app:", 11))
    {
        launch->style = VIDEO5_STYLE_TIKTOK;
        launch->tiktok = true;
        launch->allow_seek = false;
        launch->path = parameter + 11;
    }
    else if (!strncmp(parameter, "-mapsdash:", 10))
    {
        launch->style = VIDEO5_STYLE_MAPS;
        launch->fill = true;
        launch->path = parameter + 10;
    }
    else if (!strncmp(parameter, "reddit-app:", 11))
        launch->path = parameter + 11;
    else if (!strncmp(parameter, "onlyfans-app:", 13))
        launch->path = parameter + 13;
    else if (!strncmp(parameter, "spotify-wrapped:", 16))
        launch->path = parameter + 16;
    return launch->path != NULL && launch->path[0] != '\0';
}

static void video5_volume_change(int delta)
{
    int minimum = sound_min(SOUND_VOLUME);
    int maximum = sound_max(SOUND_VOLUME);
    int volume = global_status.volume + delta;

    if (global_settings.volume_limit >= minimum &&
        global_settings.volume_limit < maximum)
        maximum = global_settings.volume_limit;
    global_status.volume = MIN(maximum, MAX(minimum, volume));
    sound_set_volume(global_status.volume);
}

static bool video5_tables_sane(const struct mp4v_demux_res *demux)
{
    return demux->num_samples > 0 &&
           demux->num_samples <= VIDEO5_MAX_SAMPLES &&
           demux->num_stco > 0 && demux->num_stco <= demux->num_samples &&
           demux->num_stsc > 0 && demux->num_stsc <= demux->num_stco &&
           demux->audio_num_samples > 0 &&
           demux->audio_num_samples <= VIDEO5_MAX_AUDIO_SAMPLES &&
           demux->audio_num_stco > 0 &&
           demux->audio_num_stco <= demux->audio_num_samples &&
           demux->audio_num_stsc > 0 &&
           demux->audio_num_stsc <= demux->audio_num_stco;
}

static bool video5_stsc_sane(const struct mp4v_stsc_entry *entries,
                             uint32_t entry_count, uint32_t chunk_count,
                             uint32_t sample_count)
{
    uint64_t mapped = 0;
    uint32_t i;

    for (i = 0; i < entry_count; i++)
    {
        uint32_t next = i + 1 < entry_count ?
            entries[i + 1].first_chunk : chunk_count + 1;

        if ((i == 0 && entries[i].first_chunk != 1) ||
            entries[i].first_chunk == 0 ||
            entries[i].first_chunk > chunk_count ||
            next <= entries[i].first_chunk || next > chunk_count + 1 ||
            entries[i].samples_per_chunk == 0 ||
            entries[i].sample_desc_index == 0)
            return false;
        mapped += (uint64_t)(next - entries[i].first_chunk) *
                  entries[i].samples_per_chunk;
        if (mapped > sample_count)
            return false;
    }
    return mapped == sample_count;
}

static uint32_t video5_max_sample(const uint32_t *sizes, uint32_t count)
{
    uint32_t maximum = 0;
    uint32_t i;

    for (i = 0; i < count; i++)
        maximum = MAX(maximum, sizes[i]);
    return maximum;
}

int video_h264_play(const char *parameter, void *buffer, size_t buffer_size)
{
    static struct mp4v_demux_res demux;
    static uint32_t probe_video_sample[1];
    static uint32_t probe_video_chunk[1];
    static uint32_t probe_audio_sample[1];
    static uint32_t probe_audio_chunk[1];
    struct video5_launch launch;
    struct video5_pool pool;
    struct video5_player player;
    struct video5_surface surfaces[VIDEO5_MAX_SURFACES];
    struct mp4v_stsc_entry *video_stsc;
    struct mp4v_stsc_entry *audio_stsc;
    uint32_t *video_samples;
    uint32_t *video_chunks;
    uint32_t *audio_samples;
    uint32_t *audio_chunks;
    uint8_t *vmcs;
    uint8_t *setup;
    fb_data *overlay_pixels;
    uint32_t video_max;
    uint32_t audio_max;
    uint32_t duration_ms;
    uint32_t position_ms = 0;
    uint32_t clock_base_ms = 0;
    uint32_t overlay_second = UINT32_MAX;
    long clock_start_tick = 0;
    long overlay_until = 0;
    int vmcs_fd = -1;
    bool bcm_started = false;
    bool paused = false;
    bool cpu_boosted = false;
    bool overlay_visible = false;
    bool overlay_dirty = true;
    int result = -1;
    const char *error = NULL;
    unsigned i;

    if (parameter == NULL || buffer == NULL || buffer_size == 0 ||
        !video5_parse_launch(parameter, &launch))
        return -1;
    memset(&demux, 0, sizeof(demux));
    if (mp4v_demux_open(launch.path, &demux,
                        probe_video_sample, 1, probe_video_chunk, 1,
                        NULL, 0, probe_audio_sample, 1,
                        probe_audio_chunk, 1, NULL, 0) < 0 ||
        demux.format != MAKEFOURCC('a', 'v', 'c', '1') ||
        demux.audio_format != MAKEFOURCC('m', 'p', '4', 'a') ||
        demux.avc_profile != 66 || demux.avc_level > 13 ||
        demux.width == 0 || demux.height == 0 ||
        demux.width > 320 || demux.height > 240 ||
        demux.audio_codecdata_len == 0 || !video5_tables_sane(&demux))
    {
        splash(HZ * 3, "5G needs Apple H.264\n320x240 L1.3 + AAC");
        return -1;
    }

    pool.cursor = buffer;
    pool.end = (uint8_t *)buffer + buffer_size;
    vmcs = video5_pool_take(&pool, VIDEO5_VMCS_BYTES, 32);
    setup = video5_pool_take(&pool, 0x520, 32);
    overlay_pixels = video5_pool_take(
        &pool, LCD_WIDTH * LCD_HEIGHT * sizeof(*overlay_pixels), 32);
    video_samples = video5_pool_take(
        &pool, demux.num_samples * sizeof(*video_samples), 32);
    video_chunks = video5_pool_take(
        &pool, demux.num_stco * sizeof(*video_chunks), 32);
    video_stsc = video5_pool_take(
        &pool, demux.num_stsc * sizeof(*video_stsc), 32);
    audio_samples = video5_pool_take(
        &pool, demux.audio_num_samples * sizeof(*audio_samples), 32);
    audio_chunks = video5_pool_take(
        &pool, demux.audio_num_stco * sizeof(*audio_chunks), 32);
    audio_stsc = video5_pool_take(
        &pool, demux.audio_num_stsc * sizeof(*audio_stsc), 32);
    if (vmcs == NULL || setup == NULL || overlay_pixels == NULL ||
        video_samples == NULL ||
        video_chunks == NULL || video_stsc == NULL || audio_samples == NULL ||
        audio_chunks == NULL || audio_stsc == NULL ||
        mp4v_demux_open(launch.path, &demux,
                        video_samples, demux.num_samples,
                        video_chunks, demux.num_stco,
                        video_stsc, demux.num_stsc,
                        audio_samples, demux.audio_num_samples,
                        audio_chunks, demux.audio_num_stco,
                        audio_stsc, demux.audio_num_stsc) < 0 ||
        !video5_tables_sane(&demux) ||
        !video5_stsc_sane(video_stsc, demux.num_stsc,
                          demux.num_stco, demux.num_samples) ||
        !video5_stsc_sane(audio_stsc, demux.audio_num_stsc,
                          demux.audio_num_stco, demux.audio_num_samples))
    {
        splash(HZ * 2, "Not enough 5G movie memory");
        return -1;
    }
    video_max = video5_max_sample(video_samples, demux.num_samples);
    audio_max = video5_max_sample(audio_samples, demux.audio_num_samples);
    if (video_max == 0 || audio_max == 0 ||
        video_max > VIDEO5_MAX_SAMPLE_BYTES ||
        audio_max > VIDEO5_MAX_SAMPLE_BYTES)
    {
        splash(HZ * 2, "Apple sample exceeds safe limit");
        return -1;
    }

    memset(&player, 0, sizeof(player));
    player.output_display = video5_detect_output_display();
    memset(surfaces, 0, sizeof(surfaces));
    for (i = 0; i < ARRAYLEN(video5_files); i++)
        video5_files[i].fd = -1;
    player.demux = &demux;
    player.media_fd = open(launch.path, O_RDONLY);
    player.video.data = video5_pool_take(&pool, (video_max + 15) & ~15, 32);
    player.audio.data = video5_pool_take(&pool, (audio_max + 15) & ~15, 32);
    player.video.capacity = (video_max + 15) & ~15;
    player.audio.capacity = (audio_max + 15) & ~15;
    player.video.count = demux.num_samples;
    player.audio.count = demux.audio_num_samples;
    player.audio.audio = true;
    if (player.media_fd < 0 || player.video.data == NULL ||
        player.audio.data == NULL)
    {
        error = "Not enough sample memory";
        goto cleanup;
    }
    vmcs_fd = open(VIDEO5_VMCS, O_RDONLY);
    if (vmcs_fd < 0 ||
        read(vmcs_fd, vmcs, VIDEO5_VMCS_BYTES) != VIDEO5_VMCS_BYTES)
    {
        error = "Install Apple 5G VideoCore files";
        goto cleanup;
    }
    close(vmcs_fd);
    vmcs_fd = -1;
    if (!bcm2722_video_start(vmcs, VIDEO5_VMCS_BYTES))
    {
        error = "VideoCore boot failed";
        goto cleanup;
    }
    bcm_started = true;
    if (!video5_discover_services(&player))
    {
        error = "Apple VideoCore services missing";
        goto cleanup;
    }
    if (!video5_gencmd(&player, "set_vll_dir /VIDEOCORE/Library") ||
        !video5_gencmd(&player, "load_application mplayer.vll"))
    {
        error = "Apple MPlayer load failed";
        goto cleanup;
    }
    if (!video5_setup_passthru(&player, setup))
    {
        error = "Apple codec setup failed";
        goto cleanup;
    }

    duration_ms = video5_duration_ms(&demux);
    if (launch.live && duration_ms > 0)
    {
        time_t now = mktime(get_time());
        uint64_t elapsed = now > (time_t)launch.live_epoch ?
            (uint64_t)(now - launch.live_epoch) * 1000u : 0;

        position_ms = elapsed % duration_ms;
    }
    clock_base_ms = position_ms;
    clock_start_tick = current_tick;
    overlay_until = current_tick + HZ * 2;
    video5_seek(&player, position_ms);
    cpu_boost(true);
    cpu_boosted = true;
    if (!video5_set_vc_playback_power(&player) ||
        !video5_gencmd(&player, "mp_selectplay passthru:rockpod 0") ||
        !video5_enable_output(&player, launch.fill) ||
        !video5_gencmd(&player, "mp_play"))
    {
        error = "Apple MPlayer start failed";
        goto cleanup;
    }
    if (!video5_overlay_update(&player, surfaces, overlay_pixels,
                               &launch, paused, position_ms,
                               duration_ms, true))
    {
        error = "Apple display overlay failed";
        goto cleanup;
    }
    overlay_visible = true;
    overlay_second = position_ms / 1000u;
    overlay_dirty = false;

    while (!player.finished)
    {
        int button;
        bool visible;
        bool timed_overlay = launch.style == VIDEO5_STYLE_STOCK ||
                             launch.style == VIDEO5_STYLE_YOUTUBE ||
                             launch.style == VIDEO5_STYLE_NETFLIX;

        if (!video5_pump_hostfs(&player) ||
            !video5_pump_passthru(&player))
        {
            error = "Apple movie transport failed";
            goto cleanup;
        }
        if (!paused)
        {
            uint64_t elapsed = (uint64_t)(current_tick - clock_start_tick) *
                               1000u / HZ;

            position_ms = elapsed > UINT32_MAX - clock_base_ms ?
                          UINT32_MAX : clock_base_ms + elapsed;
            if (duration_ms > 0 && position_ms >= duration_ms)
                position_ms = duration_ms - 1;
        }
        visible = TIME_BEFORE(current_tick, overlay_until);
        if (overlay_dirty || visible != overlay_visible ||
            (timed_overlay && visible &&
             overlay_second != position_ms / 1000u))
        {
            if (!video5_overlay_update(&player, surfaces, overlay_pixels,
                                       &launch, paused, position_ms,
                                       duration_ms, visible))
            {
                error = "Apple display overlay failed";
                goto cleanup;
            }
            overlay_visible = visible;
            overlay_second = position_ms / 1000u;
            overlay_dirty = false;
        }
        button = button_get_w_tmo(0);
        if (button == BUTTON_MENU || button == (BUTTON_MENU | BUTTON_REL))
        {
            result = 1;
            break;
        }
        if (button == BUTTON_SCROLL_FWD ||
            button == (BUTTON_SCROLL_FWD | BUTTON_REPEAT))
        {
            if (launch.tiktok || launch.instagram_feed)
            {
                result = 3;
                break;
            }
            video5_volume_change(1);
            overlay_until = current_tick + HZ * 2;
            overlay_dirty = true;
        }
        else if (button == BUTTON_SCROLL_BACK ||
                 button == (BUTTON_SCROLL_BACK | BUTTON_REPEAT))
        {
            if (launch.tiktok || launch.instagram_feed)
            {
                result = 2;
                break;
            }
            video5_volume_change(-1);
            overlay_until = current_tick + HZ * 2;
            overlay_dirty = true;
        }
        else if (button == BUTTON_LEFT ||
                 button == (BUTTON_LEFT | BUTTON_REL))
        {
            if (launch.tiktok)
            {
                result = 2;
                break;
            }
            if (launch.allow_seek)
            {
                position_ms = position_ms > 10000 ? position_ms - 10000 : 0;
                video5_seek(&player, position_ms);
                clock_base_ms = position_ms;
                clock_start_tick = current_tick;
                overlay_until = current_tick + HZ * 2;
                overlay_dirty = true;
                if (!video5_gencmd(&player, "mp_seek -10"))
                {
                    error = "Apple seek failed";
                    goto cleanup;
                }
            }
        }
        else if (button == BUTTON_RIGHT ||
                 button == (BUTTON_RIGHT | BUTTON_REL))
        {
            if (launch.tiktok || launch.instagram_feed)
            {
                result = 4;
                break;
            }
            if (launch.allow_seek)
            {
                position_ms = duration_ms == 0 ? position_ms :
                    MIN(duration_ms - 1, position_ms + 10000);
                video5_seek(&player, position_ms);
                clock_base_ms = position_ms;
                clock_start_tick = current_tick;
                overlay_until = current_tick + HZ * 2;
                overlay_dirty = true;
                if (!video5_gencmd(&player, "mp_seek 10"))
                {
                    error = "Apple seek failed";
                    goto cleanup;
                }
            }
        }
        else if (button == (BUTTON_SELECT | BUTTON_REL))
        {
            if (launch.style == VIDEO5_STYLE_YOUTUBE)
            {
                launch.fill = !launch.fill;
                if (!video5_set_region(&player, launch.fill))
                {
                    error = "Apple region change failed";
                    goto cleanup;
                }
            }
            else if (launch.style == VIDEO5_STYLE_INSTAGRAM_FEED)
                launch.instagram_feed_expanded = true;
            overlay_until = current_tick + HZ * 3;
            overlay_dirty = true;
        }
        else if (button == (BUTTON_PLAY | BUTTON_REL) && !launch.live)
        {
            bool pause = !paused;

            clock_base_ms = position_ms;
            clock_start_tick = current_tick;
            overlay_until = current_tick + HZ * 2;
            overlay_dirty = true;
            if (!pause && !cpu_boosted)
            {
                cpu_boost(true);
                cpu_boosted = true;
            }
            if ((!pause && !video5_set_vc_playback_power(&player)) ||
                !video5_gencmd(&player, pause ? "mp_pause" : "mp_play"))
            {
                error = "Apple playback power failed";
                goto cleanup;
            }
            if (pause && cpu_boosted)
            {
                cpu_boost(false);
                cpu_boosted = false;
            }
            paused = pause;
        }
        if (player.eof_sent && !player.finished)
            sleep(1);
        else
            yield();
    }
    if (result < 0)
        result = 0;

cleanup:
    if (bcm_started)
        video5_overlay_destroy(&player, surfaces);
    if (player.tv_dac_enabled)
    {
        video5_gencmd(&player, "display_control 2 dac=0");
        player.tv_dac_enabled = false;
    }
    if (player.passthru.valid)
    {
        video5_gencmd(&player, "mp_stop");
        video5_channel_send(&player.passthru, VIDEO5_PT_STOP, NULL, 0, NULL);
    }
    video5_host_close_all();
    if (bcm_started)
        bcm2722_video_stop();
    if (vmcs_fd >= 0)
        close(vmcs_fd);
    if (player.media_fd >= 0)
        close(player.media_fd);
    if (cpu_boosted)
        cpu_boost(false);
    button_clear_queue();
    if (error != NULL)
    {
        splash(HZ * 3, error);
        return -1;
    }
    return result;
}

#endif /* IPOD_VIDEO && !SIMULATOR */
