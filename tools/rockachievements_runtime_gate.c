#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rc_runtime.h"

static uint8_t memory_bytes[16];
static unsigned triggered_count;
static uint32_t triggered_id;

static uint32_t peek(uint32_t address, uint32_t num_bytes, void *userdata)
{
    uint32_t value = 0;
    uint32_t index;

    (void)userdata;
    for (index = 0; index < num_bytes && index < 4; ++index)
        if (address + index < sizeof(memory_bytes))
            value |= (uint32_t)memory_bytes[address + index] << (index * 8);
    return value;
}

static void event_handler(const rc_runtime_event_t *event)
{
    if (event->type == RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED)
    {
        ++triggered_count;
        triggered_id = event->id;
    }
}

int main(void)
{
    rc_runtime_t runtime;
    uint8_t *progress;
    uint32_t progress_size;
    int result;

    memset(&runtime, 0, sizeof(runtime));
    rc_runtime_init(&runtime);
    result = rc_runtime_activate_achievement(
        &runtime, 77, "0xH0001=5", NULL, 0);
    if (result != RC_OK)
    {
        fprintf(stderr, "activation failed: %d\n", result);
        return 1;
    }
    rc_runtime_do_frame(&runtime, event_handler, peek, NULL, NULL);
    if (triggered_count != 0)
    {
        fputs("achievement triggered before condition was true\n", stderr);
        return 2;
    }
    memory_bytes[1] = 5;
    rc_runtime_do_frame(&runtime, event_handler, peek, NULL, NULL);
    if (triggered_count != 1 || triggered_id != 77)
    {
        fputs("achievement did not trigger on the emulated frame\n", stderr);
        return 3;
    }
    rc_runtime_do_frame(&runtime, event_handler, peek, NULL, NULL);
    if (triggered_count != 1)
    {
        fputs("achievement triggered more than once\n", stderr);
        return 4;
    }

    progress_size = rc_runtime_progress_size(&runtime, NULL);
    progress = malloc(progress_size);
    if (!progress || rc_runtime_serialize_progress_sized(
            progress, progress_size, &runtime, NULL) != RC_OK)
    {
        fputs("progress serialization failed\n", stderr);
        return 5;
    }
    rc_runtime_reset(&runtime);
    if (rc_runtime_deserialize_progress_sized(
            &runtime, progress, progress_size, NULL) != RC_OK)
    {
        fputs("progress deserialization failed\n", stderr);
        return 6;
    }
    free(progress);
    rc_runtime_destroy(&runtime);
    puts("RockAchievements runtime gate: PASS");
    return 0;
}
