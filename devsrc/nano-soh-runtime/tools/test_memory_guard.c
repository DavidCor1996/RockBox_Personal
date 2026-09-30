#include "global.h"
#include "nano_memory.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static _Alignas(16) uint8_t buffer[64];
static TwoHeadArena arena;
static int reached;

static void no_failure(void *arg)
{
    (void)arg;
    reached++;
}

static void fail_end(void *arg)
{
    THA_AllocEndAlign16(&arena, (uintptr_t)arg);
    reached++;
}

static void nested(void *arg)
{
    struct nano_memory_error error;
    (void)arg;
    assert(nano_memory_guard(no_failure, NULL, &error) == -1);
}

int main(void)
{
    struct nano_memory_error error;
    for (unsigned i = 0; i < 1000; ++i) {
        memset(buffer, 0xa5, sizeof(buffer));
        THA_Ct(&arena, buffer, sizeof(buffer));
        TwoHeadArena before = arena;
        int count = reached;
        assert(nano_memory_guard(fail_end, (void*)UINTPTR_MAX, &error) == -1);
        assert(error.phase == NANO_MEMORY_SCENE);
        assert(reached == count && memcmp(&arena, &before, sizeof(arena)) == 0);
        for (unsigned j = 0; j < sizeof(buffer); ++j) assert(buffer[j] == 0xa5);
        assert(nano_memory_guard(no_failure, NULL, &error) == 0);
        assert(error.phase == NANO_MEMORY_NONE && reached == count + 1);
        assert(nano_memory_guard(nested, NULL, &error) == 0);
    }
    puts("PASS: 1000 guarded failures, preserved arenas, nested-guard rejection and recovery");
}
