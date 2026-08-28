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
#define NETFLIX_INTRO_SOURCE_WIDTH 320
#define NETFLIX_INTRO_SOURCE_HEIGHT 180
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
    bitmap->width = NETFLIX_INTRO_SOURCE_WIDTH;
    bitmap->height = NETFLIX_INTRO_SOURCE_HEIGHT;
    bitmap->format = FORMAT_NATIVE;
    bitmap->data = buffer;
    result = rb->read_bmp_file(path, bitmap, (int)buffer_size,
                               FORMAT_NATIVE, NULL);

    return result > 0 && bitmap->width == NETFLIX_INTRO_SOURCE_WIDTH &&
           bitmap->height == NETFLIX_INTRO_SOURCE_HEIGHT;
}

static void netflix_intro_scale_fullscreen(const fb_data *source,
                                           fb_data *output,
                                           const unsigned short *source_x_map)
{
    const int crop_y = 0;
    const int crop_height = NETFLIX_INTRO_SOURCE_HEIGHT;

    for (int y = 0; y < LCD_HEIGHT; ++y)
    {
        int source_y = crop_y + y * crop_height / LCD_HEIGHT;
        const fb_data *source_row = source +
            (size_t)source_y * NETFLIX_INTRO_SOURCE_WIDTH;
        fb_data *output_row = output + (size_t)y * LCD_WIDTH;

        for (int x = 0; x < LCD_WIDTH; ++x)
            output_row[x] = source_row[source_x_map[x]];
    }
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
    fb_data *output_frame;
    unsigned short source_x_map[LCD_WIDTH];
    size_t frame_bytes;
    size_t all_frame_bytes;
    size_t output_frame_bytes;
    size_t frame_offset;
    off_t pcm_size;
    unsigned int old_frequency;
    long next_frame;
    long deadline;
    int fd;
    int frame;

    if (audio_pool == NULL)
        return;

    frame_bytes = BM_SIZE(NETFLIX_INTRO_SOURCE_WIDTH,
                          NETFLIX_INTRO_SOURCE_HEIGHT,
                          FORMAT_NATIVE, false);
    all_frame_bytes = frame_bytes * NETFLIX_INTRO_FRAMES;
    output_frame_bytes = BM_SIZE(LCD_WIDTH, LCD_HEIGHT,
                                 FORMAT_NATIVE, false);
    fd = rb->open(NETFLIX_INTRO_PCM, O_RDONLY);
    if (fd < 0)
        return;

    pcm_size = rb->filesize(fd);
    if (pcm_size <= 0 || (pcm_size & 3) != 0 ||
        (off_t)(size_t)pcm_size != pcm_size)
    {
        rb->close(fd);
        return;
    }

    frame_offset = ((size_t)pcm_size + 3) & ~(size_t)3;
    if (frame_offset > pool_size ||
        all_frame_bytes > pool_size - frame_offset ||
        output_frame_bytes > pool_size - frame_offset - all_frame_bytes)
    {
        rb->close(fd);
        return;
    }
    frame_buffer = pcm + frame_offset;
    output_frame = (fb_data *)(frame_buffer + all_frame_bytes);
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

    /* The 16:9 frames are aspect-filled into the 4:3 LCD: keep all 180
     * source lines and center-crop 40 pixels from each horizontal edge. */
    for (int x = 0; x < LCD_WIDTH; ++x)
        source_x_map[x] = 40 + x * 240 / LCD_WIDTH;

    rb->button_clear_queue();
    rb->lcd_set_backdrop(NULL);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_clear_display();
    netflix_intro_scale_fullscreen((const fb_data *)frame_buffer,
                                   output_frame, source_x_map);
    rb->lcd_bitmap(output_frame, 0, 0, LCD_WIDTH, LCD_HEIGHT);
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

        netflix_intro_scale_fullscreen(
            (const fb_data *)(frame_buffer + frame * frame_bytes),
            output_frame, source_x_map);
        rb->lcd_bitmap(output_frame, 0, 0, LCD_WIDTH, LCD_HEIGHT);
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
