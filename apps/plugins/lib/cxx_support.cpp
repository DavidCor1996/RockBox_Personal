/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/                \/
 *
 * Minimal C++ runtime hooks for plugins.
 *
 ****************************************************************************/

#include "plugin_cxx_compat.h"

extern "C" {
#include "plugin_cxx.h"
}

#include <stdarg.h>

static bool cxx_ready;

struct cxx_block {
    size_t size;
    bool free;
    cxx_block *prev;
    cxx_block *next;
};

static cxx_block *cxx_head;
static uintptr_t cxx_start;
static uintptr_t cxx_end;

static size_t cxx_block_overhead(void)
{
    return (sizeof(cxx_block) + 15U) & ~(size_t)15U;
}

static void *cxx_payload(cxx_block *block)
{
    return (unsigned char *)block + cxx_block_overhead();
}

static cxx_block *cxx_payload_block(void *ptr)
{
    return (cxx_block *)((unsigned char *)ptr - cxx_block_overhead());
}

static bool cxx_block_in_heap(cxx_block *block)
{
    uintptr_t pos = (uintptr_t)block;
    return pos >= cxx_start && pos + cxx_block_overhead() <= cxx_end;
}

static bool cxx_block_valid(cxx_block *block)
{
    uintptr_t payload;

    if (!cxx_block_in_heap(block))
        return false;

    payload = (uintptr_t)cxx_payload(block);
    return payload <= cxx_end && block->size <= cxx_end - payload;
}

extern "C" void plugin_cxx_init(void *buffer, size_t buffer_size)
{
    uintptr_t start = ((uintptr_t)buffer + 15U) & ~(uintptr_t)15U;
    uintptr_t end = ((uintptr_t)buffer + buffer_size) & ~(uintptr_t)15U;
    size_t overhead = cxx_block_overhead();
    cxx_head = NULL;
    cxx_start = start;
    cxx_end = end;
    if (end > start + overhead)
    {
        cxx_head = (cxx_block *)start;
        cxx_head->size = end - start - overhead;
        cxx_head->free = true;
        cxx_head->prev = NULL;
        cxx_head->next = NULL;
    }
    cxx_ready = cxx_head != NULL;
}

extern "C" size_t plugin_cxx_available(void)
{
    size_t total = 0;
    cxx_block *block;

    if (!cxx_ready)
        return 0;

    for (block = cxx_head; block; block = block->next)
    {
        if (!cxx_block_valid(block))
            break;
        if (block->free)
            total += block->size;
    }
    return total;
}

static void *plugin_cxx_alloc(size_t size)
{
    cxx_block *block;
    size_t overhead = cxx_block_overhead();
    unsigned int guard = 0;

    if (!cxx_ready || size == 0)
        return 0;

    size = (size + 15U) & ~(size_t)15U;
    for (block = cxx_head; block; block = block->next)
    {
        if (++guard > 262144 || !cxx_block_valid(block))
            return 0;

        if (!block->free || block->size < size)
            continue;

        if (block->size >= size + overhead + 16U)
        {
            cxx_block *split = (cxx_block *)
                ((unsigned char *)cxx_payload(block) + size);
            if ((uintptr_t)split + overhead > cxx_end)
                return 0;
            split->size = block->size - size - overhead;
            split->free = true;
            split->prev = block;
            split->next = block->next;
            if (split->next)
                split->next->prev = split;
            block->next = split;
            block->size = size;
        }

        block->free = false;
        return cxx_payload(block);
    }

    return 0;
}

static void plugin_cxx_free(void *ptr)
{
    cxx_block *block;
    size_t overhead = cxx_block_overhead();

    if (!ptr)
        return;

    if ((uintptr_t)ptr < cxx_start + overhead || (uintptr_t)ptr >= cxx_end)
        return;

    block = cxx_payload_block(ptr);
    if (!cxx_block_valid(block))
        return;
    if (block->free)
        return;

    block->free = true;

    if (block->next && cxx_block_valid(block->next))
    {
        unsigned char *block_end = (unsigned char *)cxx_payload(block) + block->size;
        if (block->free && block->next->free &&
            block_end == (unsigned char *)block->next)
        {
            cxx_block *next = block->next;
            block->size += overhead + next->size;
            block->next = next->next;
            if (block->next)
                block->next->prev = block;
        }
    }

    if (block->prev && cxx_block_valid(block->prev))
    {
        cxx_block *prev = block->prev;
        unsigned char *prev_end = (unsigned char *)cxx_payload(prev) + prev->size;
        if (prev->free && prev_end == (unsigned char *)block)
        {
            prev->size += overhead + block->size;
            prev->next = block->next;
            if (prev->next)
                prev->next->prev = prev;
        }
    }
}

static void *plugin_cxx_realloc(void *ptr, size_t size)
{
    if (!ptr)
        return plugin_cxx_alloc(size);
    if (size == 0)
    {
        plugin_cxx_free(ptr);
        return NULL;
    }

    cxx_block *block = cxx_payload_block(ptr);
    size_t old_size = cxx_block_valid(block) ? block->size : 0;
    void *new_ptr = plugin_cxx_alloc(size);
    if (!new_ptr)
        return NULL;

    size_t copy_size = old_size < size ? old_size : size;
    unsigned char *dst = (unsigned char *)new_ptr;
    unsigned char *src = (unsigned char *)ptr;
    for (size_t i = 0; i < copy_size; ++i)
        dst[i] = src[i];

    plugin_cxx_free(ptr);
    return new_ptr;
}

void *operator new(size_t size) throw()
{
    return plugin_cxx_alloc(size);
}

void *operator new[](size_t size) throw()
{
    return plugin_cxx_alloc(size);
}

void operator delete(void *ptr) throw()
{
    plugin_cxx_free(ptr);
}

void operator delete[](void *ptr) throw()
{
    plugin_cxx_free(ptr);
}

void operator delete(void *ptr, size_t) throw()
{
    plugin_cxx_free(ptr);
}

void operator delete[](void *ptr, size_t) throw()
{
    plugin_cxx_free(ptr);
}

#ifndef SIMULATOR
extern "C" void *malloc(size_t size)
{
    return plugin_cxx_alloc(size);
}

extern "C" void free(void *ptr)
{
    plugin_cxx_free(ptr);
}

extern "C" void *calloc(size_t nmemb, size_t size)
{
    if (size && nmemb > (size_t)-1 / size)
        return NULL;

    size_t bytes = nmemb * size;
    unsigned char *ptr = (unsigned char *)plugin_cxx_alloc(bytes);
    if (!ptr)
        return NULL;

    for (size_t i = 0; i < bytes; ++i)
        ptr[i] = 0;
    return ptr;
}

extern "C" void *realloc(void *ptr, size_t size)
{
    return plugin_cxx_realloc(ptr, size);
}

extern "C" size_t strlen(const char *s)
{
    const char *p = s;
    while (*p)
        ++p;
    return (size_t)(p - s);
}

extern "C" char *strcpy(char *dst, const char *src)
{
    char *out = dst;
    while ((*dst++ = *src++))
        ;
    return out;
}

extern "C" char *strcat(char *dst, const char *src)
{
    strcpy(dst + strlen(dst), src);
    return dst;
}

extern "C" char *strncat(char *dst, const char *src, size_t n)
{
    char *out = dst;
    dst += strlen(dst);
    while (n-- && *src)
        *dst++ = *src++;
    *dst = '\0';
    return out;
}

extern "C" int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

extern "C" char *strchr(const char *s, int c)
{
    do {
        if (*s == (char)c)
            return (char *)s;
    } while (*s++);
    return NULL;
}

extern "C" char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    do {
        if (*s == (char)c)
            last = s;
    } while (*s++);
    return (char *)last;
}

extern "C" char *strstr(const char *haystack, const char *needle)
{
    if (!*needle)
        return (char *)haystack;

    for (; *haystack; ++haystack) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n) {
            ++h;
            ++n;
        }
        if (!*n)
            return (char *)haystack;
    }
    return NULL;
}

extern "C" int atoi(const char *s)
{
    int sign = 1;
    int value = 0;

    while (*s == ' ' || *s == '\t' || *s == '\n' ||
           *s == '\r' || *s == '\f' || *s == '\v')
        ++s;

    if (*s == '-' || *s == '+') {
        if (*s == '-')
            sign = -1;
        ++s;
    }

    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s - '0');
        ++s;
    }

    return value * sign;
}

extern "C" long strtol(const char *s, char **endptr, int base)
{
    int sign = 1;
    long value = 0;

    while (*s == ' ' || *s == '\t' || *s == '\n' ||
           *s == '\r' || *s == '\f' || *s == '\v')
        ++s;

    if (*s == '-' || *s == '+') {
        if (*s == '-')
            sign = -1;
        ++s;
    }

    if ((base == 0 || base == 16) && s[0] == '0' &&
        (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
    } else if (base == 0) {
        base = (s[0] == '0') ? 8 : 10;
    }

    const char *last = s;
    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9')
            digit = *s - '0';
        else if (*s >= 'a' && *s <= 'z')
            digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z')
            digit = *s - 'A' + 10;
        else
            break;

        if (digit >= base)
            break;

        value = value * base + digit;
        last = ++s;
    }

    if (endptr)
        *endptr = (char *)last;
    return value * sign;
}

extern "C" int isalnum(int c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z');
}

extern "C" int isalpha(int c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

extern "C" int isdigit(int c)
{
    return c >= '0' && c <= '9';
}

extern "C" int islower(int c)
{
    return c >= 'a' && c <= 'z';
}

extern "C" int isspace(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\f' || c == '\v';
}

extern "C" int isupper(int c)
{
    return c >= 'A' && c <= 'Z';
}

extern "C" int isprint(int c)
{
    return c >= 0x20 && c < 0x7f;
}

extern "C" int ispunct(int c)
{
    return isprint(c) && !isalnum(c) && !isspace(c);
}

extern "C" int tolower(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

extern "C" int toupper(int c)
{
    return (c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c;
}

extern "C" void qsort(void *base, size_t nmemb, size_t size,
                      int (*compar)(const void *, const void *))
{
    unsigned char *data = (unsigned char *)base;

    for (size_t i = 1; i < nmemb; ++i) {
        size_t j = i;
        while (j > 0 &&
               compar(data + j * size, data + (j - 1) * size) < 0) {
            for (size_t k = 0; k < size; ++k) {
                unsigned char tmp = data[j * size + k];
                data[j * size + k] = data[(j - 1) * size + k];
                data[(j - 1) * size + k] = tmp;
            }
            --j;
        }
    }
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = rb->vsnprintf(buf, 32767, fmt, ap);
    va_end(ap);
    return ret;
}

int scummvm_cxx_sprintf(char *buf, const char *fmt, ...)
    asm("_Z7sprintfPcPKcz");
int scummvm_cxx_sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = rb->vsnprintf(buf, 32767, fmt, ap);
    va_end(ap);
    return ret;
}

int scummvm_cxx_snprintf(char *buf, unsigned int size, const char *fmt, ...)
    asm("_Z8snprintfPcjPKcz");
int scummvm_cxx_snprintf(char *buf, unsigned int size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = rb->vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return ret;
}

int scummvm_cxx_vsnprintf(char *buf, unsigned int size, const char *fmt,
                          va_list ap)
    asm("_Z9vsnprintfPcjPKcSt9__va_list");
int scummvm_cxx_vsnprintf(char *buf, unsigned int size, const char *fmt,
                          va_list ap)
{
    return rb->vsnprintf(buf, size, fmt, ap);
}
#endif
