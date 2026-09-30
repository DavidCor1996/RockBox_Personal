/***************************************************************************
 * Completion policy for Netflix video playback.
 ****************************************************************************/
#ifndef VIDEO_COMPLETION_H
#define VIDEO_COMPLETION_H

#include <stdbool.h>
#include <stdint.h>

/* All arguments use the same units. Allow leaving at known credits or during
 * the final 5%. Unknown durations never imply completion.
 * Widen the fallback calculation so long videos cannot overflow. */
static inline bool video_at_completion(uint32_t position, uint32_t duration,
                                       uint32_t credits_start)
{
    if (duration == 0 || duration == UINT32_MAX)
        return false;
    if (credits_start > 0 && credits_start < duration &&
        position >= credits_start)
        return true;
    return (uint64_t)position * 100 >= (uint64_t)duration * 95;
}

#endif /* VIDEO_COMPLETION_H */
