#include "nano_module.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct storage { unsigned char *data; uint32_t size; };
static union { long double align; unsigned char bytes[4096]; } ram;

static int read_data(void *context, uint32_t at, void *out, uint32_t count)
{
    struct storage *s = context;
    if (at > s->size || count > s->size - at) return -1;
    memcpy(out, s->data + at, count);
    return 0;
}

static int resolve(void *unused, uint64_t key, struct nano_module_symbol *out)
{
    (void)unused; (void)key;
    out->address = 0x09100000;
    out->thumb = 1;
    return 0;
}

static int sync_code(void *unused, void *memory, uint32_t size)
{
    (void)unused;
    assert(memory == ram.bytes && size <= sizeof ram.bytes);
    return 0;
}

int main(int argc, char **argv)
{
    FILE *f;
    long length;
    uint32_t i;
    struct storage s;
    struct nano_module_info info;
    assert(argc == 2);
    f = fopen(argv[1], "rb");
    assert(f && !fseek(f, 0, SEEK_END));
    length = ftell(f);
    assert(length > 0 && length < 4096);
    rewind(f);
    s.data = malloc((size_t)length);
    assert(s.data && fread(s.data, 1, (size_t)length, f) == (size_t)length);
    fclose(f);
    s.size = (uint32_t)length;
    assert(!nano_module_load(read_data, &s, resolve, NULL, sync_code, NULL,
                            ram.bytes, sizeof ram.bytes, 0x09000000, &info));
    for (i = 0; i < (uint32_t)length; i++) {
        s.size = i;
        assert(nano_module_load(read_data, &s, resolve, NULL, sync_code, NULL,
                               ram.bytes, sizeof ram.bytes, 0x09000000, &info));
        assert(!info.ready);
        s.size = (uint32_t)length;
        s.data[i] ^= 0x80;
        assert(nano_module_load(read_data, &s, resolve, NULL, sync_code, NULL,
                               ram.bytes, sizeof ram.bytes, 0x09000000, &info));
        assert(!info.ready);
        s.data[i] ^= 0x80;
    }
    free(s.data);
    puts("Module bounds and every fixture truncation/corruption passed ASan/UBSan.");
    return 0;
}
