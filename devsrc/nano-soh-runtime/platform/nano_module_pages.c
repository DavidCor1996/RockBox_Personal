#include "nano_module_pages.h"
#include <limits.h>
#include <string.h>

void nano_module_pages_invalidate(struct nano_module_pages *p)
{
    if (p) p->valid = 0;
}

int nano_module_pages_read(void *context, uint16_t id, uint32_t at,
                           void *out, uint32_t size)
{
    struct nano_module_pages *p = context;
    uint8_t *dst = out;
    uint32_t page, within, count, got;
    unsigned n, i;
    const char *name;
    char path[40];
    const char hex[] = "0123456789abcdef";
    if (!p || !p->read_file || !p->names || id >= p->count ||
        (!out && size) || size > UINT32_MAX - at) return -1;
    while (size) {
        page = at / 65536; within = at % 65536;
        if (!p->valid || p->cached_module != id || p->cached_page != page) {
            p->valid = 0;
            name = p->names[id];
            if (!name) return -1;
            n = 0;
            while (*name) {
                char c = *name++;
                if (n >= 24 || !((c >= 'a' && c <= 'z') ||
                                (c >= '0' && c <= '9') || c == '-')) return -1;
                path[n++] = c;
            }
            if (!n) return -1;
            path[n++] = '/';
            for (i = 0; i < 6; i++) path[n++] = hex[(page >> (20 - i * 4)) & 15];
            memcpy(path + n, ".nmp", 5);
            got = 0;
            if (p->read_file(p->context, path, p->bytes, sizeof p->bytes, &got) ||
                !got || got > sizeof p->bytes) return -1;
            p->cached_module = id;
            p->cached_page = page;
            p->cached_bytes = got;
            p->valid = 1;
        }
        if (within >= p->cached_bytes) return -1;
        count = p->cached_bytes - within;
        if (count > size) count = size;
        memcpy(dst, p->bytes + within, count);
        dst += count; at += count; size -= count;
        if (size && p->cached_bytes < sizeof p->bytes) return -1;
    }
    return 0;
}
