/***************************************************************************
 * Personal-use DIRECTV receiver boot ident for the Live TV launcher.
 *
 * Launching Live TV used to hold a black screen while the guide file was
 * parsed - 16k lines of it on a full week's schedule - and then again while
 * the tuned channel filled the player's buffers. This plays the genuine
 * receiver start-up animation across that whole wait, from frames captured
 * off the source recording by tools/prepare_directv_boot_assets.sh, and
 * leaves the final frame on the LCD so the part of the wait that outlasts
 * the clip still shows the boot screen rather than black.
 *
 * The first frame is painted as soon as its own bytes have arrived rather
 * than after the whole pack has loaded, because reading a megabyte off the
 * player is itself long enough to see.
 *
 * The source clip is silent, so nothing here touches the mixer, playback
 * state, or the ownership of the shared audio buffer beyond the
 * plugin_get_audio_buffer() call the video player makes moments later
 * anyway. Frames are only ever painted; the ident never reads the Live TV
 * schedule or any tagcache data.
 ****************************************************************************/

#ifndef DIRECTV_BOOT_H
#define DIRECTV_BOOT_H

#define DIRECTV_BOOT_DIR    ROCKBOX_DIR "/ipodjs/livetv/boot"
#define DIRECTV_BOOT_PACK   DIRECTV_BOOT_DIR "/boot-320x240.nfr"
#define DIRECTV_BOOT_WIDTH  320
#define DIRECTV_BOOT_HEIGHT 240
#define DIRECTV_BOOT_HEADER 16
#define DIRECTV_BOOT_MAX_FRAMES 240

#if defined(HAVE_LCD_COLOR) && LCD_WIDTH == DIRECTV_BOOT_WIDTH && \
    LCD_HEIGHT == DIRECTV_BOOT_HEIGHT

#define DIRECTV_BOOT_PIXELS (DIRECTV_BOOT_WIDTH * DIRECTV_BOOT_HEIGHT)
#define DIRECTV_BOOT_CANVAS_BYTES (DIRECTV_BOOT_PIXELS * sizeof(fb_data))

static const unsigned char *directv_boot_pack;
static size_t directv_boot_pack_size;
static int directv_boot_frame_count;
static int directv_boot_frame_ms;

static unsigned int directv_boot_le16(const unsigned char *data)
{
    return data[0] | ((unsigned int)data[1] << 8);
}

static size_t directv_boot_le32(const unsigned char *data)
{
    return (size_t)data[0] | ((size_t)data[1] << 8) |
           ((size_t)data[2] << 16) | ((size_t)data[3] << 24);
}

static bool directv_boot_read_exact(int fd, unsigned char *buffer,
                                    size_t bytes)
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

/* Applies one delta frame to the running canvas. Runs with the high bit
 * set repeat what is already there, which is most of a boot screen. */
static bool directv_boot_decode_frame(int frame, fb_data *canvas)
{
    size_t cursor;
    size_t end;
    size_t position = 0;

    cursor = directv_boot_le32(directv_boot_pack + DIRECTV_BOOT_HEADER +
                               frame * 4);
    end = directv_boot_le32(directv_boot_pack + DIRECTV_BOOT_HEADER +
                            (frame + 1) * 4);
    if (cursor >= end || end > directv_boot_pack_size)
        return false;

    while (cursor + 2 <= end && position < DIRECTV_BOOT_PIXELS)
    {
        unsigned int token = directv_boot_le16(directv_boot_pack + cursor);
        size_t count = token & 0x7fff;
        size_t pixel;

        cursor += 2;
        if (count == 0 || position + count > DIRECTV_BOOT_PIXELS)
            return false;

        if (token & 0x8000)
        {
            position += count;
            continue;
        }

        if (cursor + count * 2 > end)
            return false;

        for (pixel = 0; pixel < count; pixel++)
        {
            canvas[position + pixel] = (fb_data)
                directv_boot_le16(directv_boot_pack + cursor + pixel * 2);
        }
        cursor += count * 2;
        position += count;
    }

    return position == DIRECTV_BOOT_PIXELS && cursor == end;
}

static void directv_boot_show(fb_data *canvas)
{
    rb->lcd_bitmap(canvas, 0, 0, DIRECTV_BOOT_WIDTH, DIRECTV_BOOT_HEIGHT);
    rb->lcd_update();
}

/* Plays the ident, then returns with its last frame still on the LCD so
 * the loading that follows is covered too. Any button held once the clip
 * has settled skips straight to that held frame. */
static void directv_boot_run(void *scratch, size_t scratch_size)
{
    fb_data *canvas;
    unsigned char *pack;
    size_t capacity;
    size_t table_bytes;
    size_t first_end;
    off_t length;
    long started;
    int fd;
    int frame;

    if (scratch == NULL || scratch_size <= DIRECTV_BOOT_CANVAS_BYTES)
        return;

    canvas = (fb_data *)scratch;
    pack = (unsigned char *)scratch + DIRECTV_BOOT_CANVAS_BYTES;
    capacity = scratch_size - DIRECTV_BOOT_CANVAS_BYTES;
    directv_boot_pack = pack;
    directv_boot_pack_size = 0;

    fd = rb->open(DIRECTV_BOOT_PACK, O_RDONLY);
    if (fd < 0)
        return;

    length = rb->filesize(fd);
    if (length <= DIRECTV_BOOT_HEADER || (size_t)length > capacity ||
        !directv_boot_read_exact(fd, pack, DIRECTV_BOOT_HEADER) ||
        rb->memcmp(pack, "NFR1", 4) ||
        directv_boot_le16(pack + 4) != DIRECTV_BOOT_WIDTH ||
        directv_boot_le16(pack + 6) != DIRECTV_BOOT_HEIGHT ||
        directv_boot_le16(pack + 12) != 0)
    {
        rb->close(fd);
        return;
    }

    directv_boot_frame_count = (int)directv_boot_le16(pack + 8);
    directv_boot_frame_ms = (int)directv_boot_le16(pack + 10);
    if (directv_boot_frame_count <= 0 ||
        directv_boot_frame_count > DIRECTV_BOOT_MAX_FRAMES ||
        directv_boot_frame_ms < 20 || directv_boot_frame_ms > 500)
    {
        rb->close(fd);
        return;
    }

    table_bytes = ((size_t)directv_boot_frame_count + 1) * 4;
    if ((size_t)DIRECTV_BOOT_HEADER + table_bytes > (size_t)length ||
        !directv_boot_read_exact(fd, pack + DIRECTV_BOOT_HEADER,
                                 table_bytes))
    {
        rb->close(fd);
        return;
    }

    /* Read only as far as the first frame's payload, paint it, and pick the
     * rest up afterwards - a megabyte of pack would otherwise be another
     * second of the black screen this exists to remove. */
    first_end = directv_boot_le32(pack + DIRECTV_BOOT_HEADER + 4);
    if (first_end <= DIRECTV_BOOT_HEADER + table_bytes ||
        first_end > (size_t)length ||
        !directv_boot_read_exact(fd, pack + DIRECTV_BOOT_HEADER + table_bytes,
                                 first_end - DIRECTV_BOOT_HEADER -
                                     table_bytes))
    {
        rb->close(fd);
        return;
    }
    directv_boot_pack_size = first_end;

    rb->memset(canvas, 0, DIRECTV_BOOT_CANVAS_BYTES);
    rb->button_clear_queue();
    rb->lcd_set_viewport(NULL);
    rb->lcd_set_backdrop(NULL);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_clear_display();

    if (!directv_boot_decode_frame(0, canvas))
    {
        rb->close(fd);
        return;
    }
    directv_boot_show(canvas);

    if (!directv_boot_read_exact(fd, pack + first_end,
                                 (size_t)length - first_end))
    {
        rb->close(fd);
        return;
    }
    rb->close(fd);
    directv_boot_pack_size = (size_t)length;

    started = *rb->current_tick;
    for (frame = 1; frame < directv_boot_frame_count; frame++)
    {
        long target = started + frame * directv_boot_frame_ms * HZ / 1000;

        while (TIME_BEFORE(*rb->current_tick, target))
        {
            /* Ignore the press that launched Live TV; a button still held
             * half a second in is a deliberate skip. */
            if (*rb->current_tick - started > HZ / 2 &&
                rb->button_status() != BUTTON_NONE)
                goto done;
            rb->sleep(1);
        }

        if (!directv_boot_decode_frame(frame, canvas))
            break;
        directv_boot_show(canvas);
    }

done:
    rb->button_clear_queue();
}

#else

static void directv_boot_run(void *scratch, size_t scratch_size)
{
    (void)scratch;
    (void)scratch_size;
}

#endif

#endif
