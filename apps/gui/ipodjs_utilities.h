/***************************************************************************
 * Native iPodJS Clock utilities.
 ***************************************************************************/

#ifndef IPODJS_UTILITIES_H
#define IPODJS_UTILITIES_H

#include <stdbool.h>
#include <stddef.h>

enum ipodjs_utility
{
    IPODJS_UTILITY_WORLD_CLOCK = 0,
    IPODJS_UTILITY_STOPWATCH,
    IPODJS_UTILITY_TIMER,
#ifdef HAVE_RTC_ALARM
    IPODJS_UTILITY_ALARMS,
#endif
};

enum ipodjs_utilities_result
{
    IPODJS_UTILITIES_BACK = 0,
    IPODJS_UTILITIES_WPS,
};

enum ipodjs_utilities_result ipodjs_utilities_open(
    enum ipodjs_utility utility, bool edit_immediately);
bool ipodjs_utilities_prepare_classic_preview(void);
bool ipodjs_utilities_draw_classic_preview_map(int x, int y,
                                               int width, int height);
bool ipodjs_utilities_draw_classic_preview(int x, int y,
                                           int width, int height);
void ipodjs_utilities_service(void);
void ipodjs_utilities_timer_summary(char *buffer, size_t size);

#endif
