#!/usr/bin/env python3
"""Execute the production quick-scroll index and expiry code on the host."""
import pathlib
import re
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


class FastScrollTest(unittest.TestCase):
    def test_index_and_finger_lift(self):
        source = (ROOT / 'apps/gui/list.c').read_text()
        index = source[source.index('#define IPODJS_FAST_SCROLL_MIN_ITEMS'):
                       source.index('bool gui_synclist_do_button(')]
        ui = (ROOT / 'apps/gui/ipodjs_ui.c').read_text()
        expiry = ui[ui.index('bool ipodjs_ui_fast_scroll_take_expired(void)'):
                    ui.index('void ipodjs_ui_draw_fast_scroll(')]
        languages = '\n'.join(f'#define {name} "{name}"' for name in
                              sorted(set(re.findall(r'LANG_\w+', index))))
        harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#define HAVE_WHEEL_POSITION
#define HZ 100
#define TIME_AFTER(a,b) ((long)((b)-(a)) < 0)
#define VOICEONLY_DELIMITER 9999
#define P2ID(x) (-1)
#define P2STR(x) (x)
#define str(x) (x)
#define strmemccpy(dst,src,n) snprintf(dst,n,"%s",src)
struct gui_synclist {
    void *data;
    int nb_items, selected_item, selected_size;
    const char *title;
    const unsigned char *(*callback_get_item_name)(int, void *,
                                                  unsigned char *, size_t);
};
static bool ipodjs_ui_fast_scroll_visible;
static long current_tick, ipodjs_ui_fast_scroll_deadline;
static char ipodjs_ui_fast_scroll_label[8];
static int position, queued, calls;
static int wheel_status(void) { return position; }
static bool button_queue_empty(void) { return !queued; }
static void ipodjs_trace_fast_scroll(const char *s, const char *t)
{ (void)s; (void)t; }
static void ipodjs_ui_fast_scroll_clear(void)
{ ipodjs_ui_fast_scroll_visible = false; }
static void ipodjs_ui_fast_scroll_show(const char *s)
{
    strcpy(ipodjs_ui_fast_scroll_label, s);
    ipodjs_ui_fast_scroll_visible = true;
}
static int ipodjs_ui_fast_scroll_bucket(const char *s)
{ return *s >= 'A' && *s <= 'Z' ? *s - 'A' + 1 : 0; }
static void gui_synclist_select_item(struct gui_synclist *list, int i)
{ assert(i >= 0 && i < list->nb_items); list->selected_item = i; }
static const unsigned char *name(int i, void *data,
                                 unsigned char *buffer, size_t size)
{
    (void)buffer; (void)size; calls++;
    return (const unsigned char *)((const char **)data)[i];
}
'''
        cases = r'''
int main(void)
{
    const char *names[] = {"Aster", "Aster two", "Cedar", "Zinnia"};
    struct gui_synclist list = {names, 4, 0, 1, "Album Artist", name};
    assert(list_ipodjs_fast_scroll_eligible(&list));
    assert(list_ipodjs_fast_scroll_step(&list, 1));
    assert(list.selected_item == 0);
    assert(list_ipodjs_fast_scroll_step(&list, 1));
    assert(list.selected_item == 2);
    assert(!strcmp(ipodjs_ui_fast_scroll_label, "C"));
    assert(list_ipodjs_fast_scroll_step(&list, -1));
    assert(list.selected_item == 0);
    assert(list_ipodjs_fast_scroll_step(&list, -1));
    assert(list.selected_item == 0);

    /* Reload at the same address, title and count with new bucket offsets. */
    names[1] = "Birch";
    list_ipodjs_fast_scroll_invalidate();
    assert(!ipodjs_ui_fast_scroll_visible);
    assert(list_ipodjs_fast_scroll_step(&list, 1));
    assert(list_ipodjs_fast_scroll_step(&list, 1));
    assert(list.selected_item == 1);

    /* Failed name retrieval must not index out of bounds or rescan per tick. */
    names[2] = NULL;
    list_ipodjs_fast_scroll_invalidate();
    assert(!list_ipodjs_fast_scroll_step(&list, 1));
    int previous = calls;
    assert(!list_ipodjs_fast_scroll_step(&list, 1));
    assert(calls == previous);
    names[2] = "Cedar";
    list_ipodjs_fast_scroll_invalidate();
    assert(list_ipodjs_fast_scroll_step(&list, 1));

    current_tick = 10;
    ipodjs_ui_fast_scroll_deadline = 110;
    position = 40;
    assert(!ipodjs_ui_fast_scroll_take_expired());
    position = -1;
    queued = 1;
    assert(!ipodjs_ui_fast_scroll_take_expired());
    queued = 0;
    assert(ipodjs_ui_fast_scroll_take_expired());
    assert(!ipodjs_ui_fast_scroll_visible);
    ipodjs_ui_fast_scroll_show("C");
    position = 40;
    current_tick = 111;
    assert(ipodjs_ui_fast_scroll_take_expired());
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            c = pathlib.Path(tmp) / 'fast.c'
            binary = pathlib.Path(tmp) / 'fast'
            c.write_text(harness + languages + '\n' + index + expiry + cases)
            subprocess.run(['cc', '-std=gnu99', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=undefined,address', str(c), '-o',
                            str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
