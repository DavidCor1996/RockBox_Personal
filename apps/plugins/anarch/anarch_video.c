#include "anarch_platform.h"

#define ANARCH_GUARD_WORDS 16
#define ANARCH_GUARD_VALUE ((fb_data)0x5aa5)

static fb_data *framebuffer;
static fb_data *guard;
static size_t arena_bytes;
static size_t arena_used;

bool anarch_video_init(void)
{
    size_t size;
    size_t pixels = ANARCH_WIDTH * ANARCH_HEIGHT;
    fb_data *arena = rb->plugin_get_buffer(&size);
    size_t needed = (pixels + ANARCH_GUARD_WORDS) * sizeof(fb_data);
    int i;

    if (arena == NULL || size < needed)
        return false;
    arena_bytes = size;
    arena_used = needed;
    framebuffer = arena;
    guard = framebuffer + pixels;
    for (i = 0; i < ANARCH_GUARD_WORDS; ++i)
        guard[i] = ANARCH_GUARD_VALUE;
    anarch_video_clear();
    return true;
}

void anarch_video_clear(void)
{
    if (framebuffer != NULL)
        rb->memset(framebuffer, 0,
                   ANARCH_WIDTH * ANARCH_HEIGHT * sizeof(fb_data));
}

void anarch_video_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (framebuffer != NULL && x < ANARCH_WIDTH && y < ANARCH_HEIGHT)
        framebuffer[y * ANARCH_WIDTH + x] = (fb_data)color;
}

void anarch_video_present(void)
{
    rb->lcd_bitmap(framebuffer, 0, 0, ANARCH_WIDTH, ANARCH_HEIGHT);
    rb->lcd_update();
}

bool anarch_video_guards_ok(void)
{
    int i;

    for (i = 0; i < ANARCH_GUARD_WORDS; ++i)
        if (guard[i] != ANARCH_GUARD_VALUE)
            return false;
    return true;
}

uint32_t anarch_video_hash(void)
{
    return rb->crc_32(framebuffer,
                      ANARCH_WIDTH * ANARCH_HEIGHT * sizeof(fb_data),
                      0xffffffffu);
}

size_t anarch_video_arena_used(void)
{
    return arena_used;
}

size_t anarch_video_arena_free(void)
{
    return arena_bytes - arena_used;
}

#ifdef SIMULATOR
bool anarch_video_dump(void)
{
    static const char path[] = ROCKBOX_DIR "/games/anarch/sim-frame.ppm";
    uint8_t row[ANARCH_WIDTH * 3];
    int fd = rb->open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    int x;
    int y;

    if (fd < 0)
        return false;
    rb->fdprintf(fd, "P6\n%d %d\n255\n", ANARCH_WIDTH, ANARCH_HEIGHT);
    for (y = 0; y < ANARCH_HEIGHT; ++y)
    {
        for (x = 0; x < ANARCH_WIDTH; ++x)
        {
            uint16_t pixel = framebuffer[y * ANARCH_WIDTH + x];

            row[x * 3] = (uint8_t)(((pixel >> 11) & 0x1f) * 255 / 31);
            row[x * 3 + 1] = (uint8_t)(((pixel >> 5) & 0x3f) * 255 / 63);
            row[x * 3 + 2] = (uint8_t)((pixel & 0x1f) * 255 / 31);
        }
        if (rb->write(fd, row, sizeof(row)) != (ssize_t)sizeof(row))
        {
            rb->close(fd);
            return false;
        }
    }
    rb->close(fd);
    return true;
}
#endif
