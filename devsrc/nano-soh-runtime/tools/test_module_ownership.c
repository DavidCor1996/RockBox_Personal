#include "nano_modules.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *file_data;
static uint32_t file_size, allocation_bytes, allocations, releases, next_base;
static int fail_id = -1;

static int read_module(void *unused, uint16_t id, uint32_t at,
                       void *out, uint32_t count)
{
    (void)unused;
    if (id == fail_id || at > file_size || count > file_size - at) return -1;
    memcpy(out, file_data + at, count);
    return 0;
}

static int read_one(void *unused, uint32_t at, void *out, uint32_t count)
{
    return read_module(unused, 0, at, out, count);
}

static void *allocate(void *unused, uint32_t size, uint32_t *base)
{
    void *memory;
    (void)unused;
    memory = aligned_alloc(16, size);
    assert(memory);
    *base = next_base;
    next_base += 4096;
    allocations++;
    allocation_bytes += size;
    return memory;
}

static void release(void *unused, void *memory, uint32_t size)
{
    (void)unused;
    assert(memory && allocation_bytes >= size);
    allocation_bytes -= size;
    releases++;
    free(memory);
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
    (void)unused; (void)memory; (void)size;
    return 0;
}

int main(int argc, char **argv)
{
    const uint16_t root_deps[] = {1, 2}, shared[] = {3}, cycle_a[] = {5}, cycle_b[] = {4};
    struct nano_module_desc descriptors[6];
    struct nano_module_slot slots[6] = {{0}};
    struct nano_modules m = {0};
    struct nano_module_info info;
    unsigned i;
    FILE *file;
    assert(argc == 2);
    file = fopen(argv[1], "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    file_size = (uint32_t)ftell(file);
    rewind(file);
    file_data = malloc(file_size);
    assert(file_data && fread(file_data, 1, file_size, file) == file_size);
    fclose(file);
    assert(!nano_module_inspect(read_one, NULL, &info));
    for (i = 0; i < 6; i++) {
        descriptors[i].memory_bytes = info.memory_bytes;
        descriptors[i].crc32 = info.crc32;
        descriptors[i].dependencies = NULL;
        descriptors[i].dependency_count = 0;
    }
    descriptors[0].dependencies = root_deps;
    descriptors[0].dependency_count = 2;
    descriptors[1].dependencies = descriptors[2].dependencies = shared;
    descriptors[1].dependency_count = descriptors[2].dependency_count = 1;
    descriptors[4].dependencies = cycle_a;
    descriptors[5].dependencies = cycle_b;
    descriptors[4].dependency_count = descriptors[5].dependency_count = 1;
    m.descriptors = descriptors; m.slots = slots; m.count = 6;
    m.budget = info.memory_bytes * 4;
    m.read = read_module; m.allocate = allocate; m.release = release;
    m.external = resolve; m.sync = sync_code;
    next_base = 0x09000000;
    assert(!nano_modules_acquire(&m, 0));
    assert(m.used == m.budget && slots[3].references == 2);
    assert(!nano_modules_acquire(&m, 0) && allocations == 4);
    nano_modules_release(&m, 0);
    assert(slots[0].references == 1 && releases == 0);
    assert(!nano_modules_acquire(&m, 3));
    nano_modules_release(&m, 0);
    assert(m.used == info.memory_bytes && slots[3].references == 1);
    nano_modules_release(&m, 3);
    assert(m.used == 0 && allocations == releases && allocation_bytes == 0);
    for (i = 0; i < 100; i++) {
        m.budget = info.memory_bytes * 3;
        assert(nano_modules_acquire(&m, 0) == NANO_MODULE_CAPACITY);
        assert(m.used == 0 && allocation_bytes == 0 && allocations == releases);
        m.budget = info.memory_bytes * 4;
        fail_id = 2;
        assert(nano_modules_acquire(&m, 0) == NANO_MODULE_IO);
        assert(m.used == 0 && allocation_bytes == 0 && allocations == releases);
        fail_id = -1;
    }
    assert(nano_modules_acquire(&m, 4) == NANO_MODULE_FORMAT);
    assert(!slots[4].loading && !slots[5].loading && !m.used);
    assert(nano_modules_acquire(&m, 6) == NANO_MODULE_FORMAT);
    descriptors[3].crc32 ^= 1;
    assert(nano_modules_acquire(&m, 3) == NANO_MODULE_FORMAT && !m.used);
    descriptors[3].crc32 ^= 1;
    assert(!nano_modules_acquire(&m, 3));
    m.budget = info.memory_bytes * 3;
    assert(nano_modules_acquire(&m, 0) == NANO_MODULE_CAPACITY);
    assert(slots[3].references == 1 && m.used == info.memory_bytes);
    nano_modules_release(&m, 3);
    assert(!allocation_bytes && allocations == releases);
    free(file_data);
    puts("Module dependency sharing, last-reference release, budget/I/O rollback and cycles passed ASan/UBSan.");
    return 0;
}
