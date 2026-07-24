/***************************************************************************
 * Simulator-only iPodJS display-state trace.
 ****************************************************************************/
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "config.h"
#include "audio.h"
#include "core_alloc.h"
#include "file.h"
#include "kernel.h"
#include "list.h"
#include "playlist.h"
#include "rbpaths.h"
#include "string.h"
#include "ipodjs_trace.h"

#define IPODJS_TRACE_PATH ROCKBOX_DIR "/ipodjs-trace.tsv"

static int ipodjs_trace_fd = -2;
static unsigned long ipodjs_trace_sequence;

static int ipodjs_trace_open(void)
{
    const char *enabled;

    if (ipodjs_trace_fd != -2)
        return ipodjs_trace_fd;

    enabled = getenv("ROCKPOD_SIM_IPODJS_TRACE");
    if (!enabled || strcmp(enabled, "1"))
    {
        ipodjs_trace_fd = -1;
        return -1;
    }

    ipodjs_trace_fd = open(IPODJS_TRACE_PATH,
                           O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (ipodjs_trace_fd >= 0)
    {
        fdprintf(ipodjs_trace_fd,
                 "seq\ttick\tkind\tname\tupdate\tx\ty\tw\th\tselected\t"
                 "first\titems\taudio\tplaylist_index\tplaylist_count\t"
                 "elapsed_ms\tduration_ms\tart\tart_w\tart_h\tpath\t"
                 "core_available\tcore_allocatable\trockbox_open_files\n");
    }
    return ipodjs_trace_fd;
}

static void ipodjs_trace_clean(char *dst, size_t dst_size, const char *src)
{
    size_t i = 0;

    if (!dst_size)
        return;
    if (!src)
        src = "";
    while (*src && i + 1 < dst_size)
    {
        char ch = *src++;

        dst[i++] = ch == '\t' || ch == '\r' || ch == '\n' ? ' ' : ch;
    }
    dst[i] = '\0';
}

static void ipodjs_trace_write(const char *kind, const char *name,
                               const char *update, int x, int y, int width,
                               int height, int selected, int first, int items,
                               const void *art_data, int art_width,
                               int art_height)
{
    struct mp3entry *id3;
    char clean_name[96];
    char clean_path[MAX_PATH];
    int fd = ipodjs_trace_open();

    if (fd < 0)
        return;

    id3 = audio_current_track();
    ipodjs_trace_clean(clean_name, sizeof(clean_name), name);
    ipodjs_trace_clean(clean_path, sizeof(clean_path),
                       id3 ? id3->path : "");
    fdprintf(fd,
             "%lu\t%ld\t%s\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t"
             "%d\t%d\t%d\t%lu\t%lu\t%lx\t%d\t%d\t%s\t%zu\t%zu\t%d\n",
             ++ipodjs_trace_sequence, current_tick, kind, clean_name,
             update ? update : "", x, y, width, height, selected, first,
             items, audio_status(), playlist_get_display_index(),
             playlist_amount(), id3 ? id3->elapsed : 0,
             id3 ? id3->length : 0,
             (unsigned long)(uintptr_t)art_data, art_width, art_height,
             clean_path, core_available(), core_allocatable(),
             sim_file_open_count());
}

void ipodjs_trace_list(const struct gui_synclist *list, const char *title,
                       const char *update, int x, int y, int width,
                       int height)
{
    if (!list)
        return;
    ipodjs_trace_write("list", title, update, x, y, width, height,
                       list->selected_item, list->start_item[SCREEN_MAIN],
                       list->nb_items, NULL, 0, 0);
}

void ipodjs_trace_wps(const char *update, int x, int y, int width,
                      int height, const void *art_data, int art_width,
                      int art_height)
{
    ipodjs_trace_write("wps", "Now Playing", update, x, y, width, height,
                       -1, -1, -1, art_data, art_width, art_height);
}

void ipodjs_trace_screen(const char *name, const char *update, int selected,
                         int first, int items, int x, int y, int width,
                         int height)
{
    ipodjs_trace_write("screen", name, update, x, y, width, height,
                       selected, first, items, NULL, 0, 0);
}

void ipodjs_trace_origin(const char *event, int screen, int origin,
                         int depth)
{
    char name[64];

    snprintf(name, sizeof(name), "%s screen=%d origin=%d depth=%d",
             event ? event : "origin", screen, origin, depth);
    ipodjs_trace_write("origin", name, "state", 0, 0, 0, 0,
                       -1, -1, -1, NULL, 0, 0);
}

void ipodjs_trace_fast_scroll(const char *label, const char *event)
{
    ipodjs_trace_write("fast-scroll", label ? label : "",
                       event ? event : "state", 0, 0, 95, 82,
                       -1, -1, -1, NULL, 0, 0);
}
