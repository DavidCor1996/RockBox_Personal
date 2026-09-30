/***************************************************************************
 * Simulator-only iPodJS display-state trace.
 *
 * This records read-only UI and playback observations for regression tests.
 * It never issues playback, playlist, buffer, or album-art commands.
 ****************************************************************************/
#ifndef IPODJS_TRACE_H
#define IPODJS_TRACE_H

#include "config.h"

struct gui_synclist;

#if defined(HAVE_IPODJS_UI) && defined(SIMULATOR)
void ipodjs_trace_surface(int x, int y, int width, int height);
void ipodjs_trace_list(const struct gui_synclist *list, const char *title,
                       const char *update, int x, int y, int width,
                       int height);
void ipodjs_trace_wps(const char *update, int x, int y, int width,
                      int height, const void *art_data, int art_width,
                      int art_height);
void ipodjs_trace_screen(const char *name, const char *update, int selected,
                         int first, int items, int x, int y, int width,
                         int height);
void ipodjs_trace_origin(const char *event, int screen, int origin,
                         int depth);
void ipodjs_trace_fast_scroll(const char *label, const char *event);
#else
static inline void ipodjs_trace_list(const struct gui_synclist *list,
                                     const char *title, const char *update,
                                     int x, int y, int width, int height)
{
    (void)list;
    (void)title;
    (void)update;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}

static inline void ipodjs_trace_wps(const char *update, int x, int y,
                                    int width, int height,
                                    const void *art_data, int art_width,
                                    int art_height)
{
    (void)update;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)art_data;
    (void)art_width;
    (void)art_height;
}

static inline void ipodjs_trace_origin(const char *event, int screen,
                                       int origin, int depth)
{
    (void)event;
    (void)screen;
    (void)origin;
    (void)depth;
}

static inline void ipodjs_trace_fast_scroll(const char *label,
                                            const char *event)
{
    (void)label;
    (void)event;
}

static inline void ipodjs_trace_screen(const char *name, const char *update,
                                       int selected, int first, int items,
                                       int x, int y, int width, int height)
{
    (void)name;
    (void)update;
    (void)selected;
    (void)first;
    (void)items;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}
#endif

#endif
