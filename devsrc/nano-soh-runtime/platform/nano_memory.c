#include "nano_memory.h"
#include <setjmp.h>
#include <stdlib.h>

static jmp_buf boundary;
static int active;
static struct nano_memory_error failure;

int nano_memory_guard(void (*run)(void *), void *arg,
                      struct nano_memory_error *error)
{
    if (active || !run || !error)
        return -1;
    failure = (struct nano_memory_error){0};
    active = 1;
    if (setjmp(boundary) == 0)
        run(arg);
    active = 0;
    *error = failure;
    return failure.phase == NANO_MEMORY_NONE ? 0 : -1;
}

_Noreturn void nano_memory_fail(enum nano_memory_phase phase,
                                size_t requested, size_t available)
{
    failure = (struct nano_memory_error){phase, requested, available};
    if (active)
        longjmp(boundary, 1);
    /* Launching without the required boundary is an integration error. */
    abort();
}
