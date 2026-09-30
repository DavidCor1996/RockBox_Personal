#ifndef NANO_MEMORY_H
#define NANO_MEMORY_H

#include <stddef.h>
#include <stdint.h>

/* Keep the original 1 MiB game-state arena. The obsolete 1 MiB ROM object
 * reservation is removed separately; decoded resources need their own budget.
 * The extra 256 KiB covers the game-state object and system allocations. This
 * is a development admission limit, not a measured whole-game peak. */
#define NANO_SCENE_BYTES (1024u * 1024u)
#define NANO_SYSTEM_BYTES (NANO_SCENE_BYTES + 256u * 1024u)
#define NANO_OBJECT_TOKEN_BYTES 16u

enum nano_memory_phase {
    NANO_MEMORY_NONE, NANO_MEMORY_SYSTEM, NANO_MEMORY_SCENE,
    NANO_MEMORY_AUDIO, NANO_MEMORY_OBJECT, NANO_MEMORY_RESOURCE
};

struct nano_memory_error {
    enum nano_memory_phase phase;
    size_t requested;
    size_t available;
};

/* Single application thread, synchronous game calls only. Failure unwinds to
 * the launcher without returning a NULL pointer to unchecked engine callers.
 * No memory is freed on failure: the caller must stop/join audio and fence GPU
 * work before cleanup. The guard is not an audio callback or a cleanup API. */
int nano_memory_guard(void (*run)(void *), void *arg,
                      struct nano_memory_error *error);
_Noreturn void nano_memory_fail(enum nano_memory_phase phase,
                                size_t requested, size_t available);

#endif
