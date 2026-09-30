#include "nano_module_pages.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct nano_module_pages pages;
static unsigned calls;
#define FILE_SIZE (65536u * 2u + 37u)

static int read_file(void *unused, const char *name, void *out,
                      uint32_t capacity, uint32_t *got)
{
    const char *part = strchr(name, '/');
    unsigned long index;
    unsigned id, i;
    uint32_t at;
    (void)unused;
    assert(part && capacity == 65536);
    id = name[0] == 'r' ? 0 : 1;
    index = strtoul(part + 1, NULL, 16);
    calls++;
    at = (uint32_t)index * 65536;
    if (at >= FILE_SIZE) return -1;
    *got = FILE_SIZE - at;
    if (*got > capacity) *got = capacity;
    for (i = 0; i < *got; i++) ((unsigned char *)out)[i] = (unsigned char)((at + i) ^ id);
    return 0;
}

int main(void)
{
    const char *names[] = {"resident", "actor-000"};
    unsigned char out[1300];
    uint32_t at, length, state = 42;
    unsigned i, j, id;
    pages.read_file = read_file; pages.names = names; pages.count = 2;
    assert(!nano_module_pages_read(&pages, 0, 10, out, 100));
    assert(!nano_module_pages_read(&pages, 0, 20, out, 100));
    assert(calls == 1);
    assert(!nano_module_pages_read(&pages, 0, 65530, out, 100));
    assert(calls == 2);
    for (i = 0; i < 5000; i++) {
        state = state * 1664525 + 1013904223;
        at = state % FILE_SIZE;
        length = (state >> 16) % sizeof out;
        if (length > FILE_SIZE - at) length = FILE_SIZE - at;
        id = i & 1;
        assert(!nano_module_pages_read(&pages, (uint16_t)id, at, out, length));
        for (j = 0; j < length; j++) assert(out[j] == (unsigned char)((at + j) ^ id));
    }
    assert(nano_module_pages_read(&pages, 0, FILE_SIZE, out, 1));
    assert(nano_module_pages_read(&pages, 0, FILE_SIZE - 4, out, 5));
    assert(nano_module_pages_read(&pages, 0, UINT32_MAX - 1, out, 3));
    assert(nano_module_pages_read(&pages, 2, 0, out, 1));
    nano_module_pages_invalidate(&pages);
    assert(!pages.valid);
    names[0] = "../resident";
    assert(nano_module_pages_read(&pages, 0, 0, out, 1));
    puts("Whole-file page adapter passed 5000 ranges, cross-page reads, cache switches and bounds under ASan/UBSan.");
    return 0;
}
