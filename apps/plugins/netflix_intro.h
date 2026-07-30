/***************************************************************************
 * Personal-use Netflix launch ident shared by the video players.
 *
 * The caller must already own the shared plugin audio buffer. The intro uses
 * the primary playback mixer channel, stops it before returning, and restores
 * the prior mixer rate before the selected video initializes its own output.
 ****************************************************************************/

#ifndef NETFLIX_INTRO_H
#define NETFLIX_INTRO_H

#define NETFLIX_PARAMETER_PREFIX "netflix:"
#define NETFLIX_PARAMETER_PREFIX_LEN 8
#define NETFLIX_RESTART_PARAMETER_PREFIX "netflix-restart:"
#define NETFLIX_RESTART_PARAMETER_PREFIX_LEN 16
#define NETFLIX_INTRO_DIR ROCKBOX_DIR "/ipodjs/netflix/video-launch"
#define NETFLIX_INTRO_PCM \
    NETFLIX_INTRO_DIR "/intro-44100-stereo.pcm"
#define NETFLIX_INTRO_FRAMES 12
#define NETFLIX_INTRO_WIDTH 320
#define NETFLIX_INTRO_HEIGHT 180
#define NETFLIX_INTRO_RATE 44100
#define NETFLIX_INTRO_FRAME_TICKS MAX(1, HZ / 10)
#define NETFLIX_INTRO_TIMEOUT (HZ * 4)

#if defined(HAVE_LCD_COLOR) && LCD_WIDTH == 320 && LCD_HEIGHT == 240

static bool netflix_intro_read_all(int fd, unsigned char *buffer, size_t bytes)
{
    size_t done = 0;

    while (done < bytes)
    {
        ssize_t count = rb->read(fd, buffer + done, bytes - done);

        if (count <= 0)
            return false;
        done += (size_t)count;
    }

    return true;
}

static bool netflix_intro_load_frame(int frame, struct bitmap *bitmap,
                                     unsigned char *buffer,
                                     size_t buffer_size)
{
    char path[MAX_PATH];
    int result;

    rb->snprintf(path, sizeof(path),
                 NETFLIX_INTRO_DIR "/frame-%02d.320x180x24.bmp", frame);
    rb->memset(bitmap, 0, sizeof(*bitmap));
    bitmap->width = NETFLIX_INTRO_WIDTH;
    bitmap->height = NETFLIX_INTRO_HEIGHT;
    bitmap->format = FORMAT_NATIVE;
    bitmap->data = buffer;
    result = rb->read_bmp_file(path, bitmap, (int)buffer_size,
                               FORMAT_NATIVE, NULL);

    return result > 0 && bitmap->width == NETFLIX_INTRO_WIDTH &&
           bitmap->height == NETFLIX_INTRO_HEIGHT;
}

static bool netflix_intro_input_pending(void)
{
    int button = rb->button_get_w_tmo(0);

    if (button == BUTTON_NONE)
        return false;

    rb->default_event_handler(button);
    return true;
}

static void netflix_intro_run(void *audio_pool, size_t pool_size)
{
    struct bitmap frame_bitmap;
    unsigned char *pcm = audio_pool;
    unsigned char *frame_buffer;
    size_t frame_bytes;
    size_t all_frame_bytes;
    off_t pcm_size;
    unsigned int old_frequency;
    long next_frame;
    long deadline;
    int fd;
    int frame;

    if (audio_pool == NULL)
        return;

    frame_bytes = BM_SIZE(NETFLIX_INTRO_WIDTH, NETFLIX_INTRO_HEIGHT,
                          FORMAT_NATIVE, false);
    all_frame_bytes = frame_bytes * NETFLIX_INTRO_FRAMES;
    fd = rb->open(NETFLIX_INTRO_PCM, O_RDONLY);
    if (fd < 0)
        return;

    pcm_size = rb->filesize(fd);
    if (pcm_size <= 0 || (pcm_size & 3) != 0 ||
        (off_t)(size_t)pcm_size != pcm_size ||
        (size_t)pcm_size + all_frame_bytes + 4 > pool_size)
    {
        rb->close(fd);
        return;
    }

    frame_buffer = pcm + (size_t)pcm_size;
    frame_buffer = (unsigned char *)
        (((uintptr_t)frame_buffer + 3) & ~(uintptr_t)3);
    if (!netflix_intro_read_all(fd, pcm, (size_t)pcm_size))
    {
        rb->close(fd);
        return;
    }
    rb->close(fd);

    for (frame = 0; frame < NETFLIX_INTRO_FRAMES; frame++)
    {
        if (!netflix_intro_load_frame(
                frame, &frame_bitmap, frame_buffer + frame * frame_bytes,
                frame_bytes))
            return;
    }

    rb->button_clear_queue();
    rb->lcd_set_backdrop(NULL);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_clear_display();
    rb->lcd_bitmap((const fb_data *)frame_buffer, 0,
                   (LCD_HEIGHT - NETFLIX_INTRO_HEIGHT) / 2,
                   NETFLIX_INTRO_WIDTH, NETFLIX_INTRO_HEIGHT);
    rb->lcd_update();

    old_frequency = rb->mixer_get_frequency();
    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->mixer_set_frequency(NETFLIX_INTRO_RATE);
    rb->mixer_channel_set_amplitude(PCM_MIXER_CHAN_PLAYBACK, MIX_AMP_UNITY);
    rb->mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK, NULL,
                                pcm, (size_t)pcm_size);

    next_frame = *rb->current_tick + NETFLIX_INTRO_FRAME_TICKS;
    deadline = *rb->current_tick + NETFLIX_INTRO_TIMEOUT;
    for (frame = 1; frame < NETFLIX_INTRO_FRAMES; frame++)
    {
        while (TIME_BEFORE(*rb->current_tick, next_frame))
        {
            if (netflix_intro_input_pending())
                goto stop;
            rb->sleep(1);
        }

        rb->lcd_bitmap((const fb_data *)
                           (frame_buffer + frame * frame_bytes), 0,
                       (LCD_HEIGHT - NETFLIX_INTRO_HEIGHT) / 2,
                       NETFLIX_INTRO_WIDTH, NETFLIX_INTRO_HEIGHT);
        rb->lcd_update();
        next_frame += NETFLIX_INTRO_FRAME_TICKS;
    }

    while (TIME_BEFORE(*rb->current_tick, deadline) &&
           rb->mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK) !=
               CHANNEL_STOPPED)
    {
        if (netflix_intro_input_pending())
            break;
        rb->sleep(1);
    }

stop:
    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->mixer_set_frequency(old_frequency);
    rb->button_clear_queue();
}

#else

static void netflix_intro_run(void *audio_pool, size_t pool_size)
{
    (void)audio_pool;
    (void)pool_size;
}

#endif

#endif
