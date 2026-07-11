#include "plugin.h"
#include "smsgg_platform.h"

void *smsgg_alloc_base;
unsigned char *smsgg_alloc_ptr;
size_t smsgg_alloc_free;
static size_t smsgg_alloc_size;

void smsgg_platform_init_alloc(void *base, size_t size)
{
    smsgg_alloc_base = base;
    smsgg_alloc_ptr = base;
    smsgg_alloc_free = size;
    smsgg_alloc_size = size;
}

void smsgg_platform_reset_temp(void)
{
    smsgg_alloc_ptr = smsgg_alloc_base;
    smsgg_alloc_free = smsgg_alloc_size;
}

void *smsgg_malloc(size_t size)
{
    void *ptr;

    size = (size + 3) & ~((size_t)3);
    if (size == 0 || size > smsgg_alloc_free)
        return NULL;

    ptr = smsgg_alloc_ptr;
    smsgg_alloc_ptr += size;
    smsgg_alloc_free -= size;
    return ptr;
}

void *smsgg_calloc(size_t nmemb, size_t size)
{
    size_t bytes = nmemb * size;
    void *ptr = smsgg_malloc(bytes);

    if (ptr != NULL)
        rb->memset(ptr, 0, bytes);

    return ptr;
}

void smsgg_free(void *ptr)
{
    (void)ptr;
}

void smsgg_abort(void)
{
    rb->splash(HZ, "SMSGG core abort");
}

int smsgg_log(const char *fmt, ...)
{
    (void)fmt;
    return 0;
}

unsigned int smsgg_crc32_le(unsigned int crc, const unsigned char *buf,
                            unsigned int len)
{
    return rb->crc_32(buf, len, crc);
}

static void mkdir_if_needed(const char *path)
{
    if (!rb->dir_exists(path))
        rb->mkdir(path);
}

bool smsgg_ensure_dirs(void)
{
    mkdir_if_needed(ROCKBOX_DIR "/games");
    mkdir_if_needed(SMSGG_BASE_DIR);
    mkdir_if_needed(SMSGG_ROM_DIR);
    mkdir_if_needed(SMSGG_SAVE_DIR);
    mkdir_if_needed(SMSGG_STATE_DIR);
    return rb->dir_exists(SMSGG_BASE_DIR) && rb->dir_exists(SMSGG_ROM_DIR);
}

const char *smsgg_basename(const char *path)
{
    const char *slash = rb->strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

void smsgg_make_safe_name(char *dst, size_t dst_size, const char *src)
{
    size_t i;

    if (dst_size == 0)
        return;

    for (i = 0; i + 1 < dst_size && src[i] != '\0'; i++)
    {
        char c = src[i];

        if (c == '.')
            break;

        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z') || c == '-' || c == '_'))
            c = '_';

        dst[i] = c;
    }

    dst[i] = '\0';
}
