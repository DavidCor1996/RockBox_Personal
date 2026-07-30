#ifndef ALBUMLIST_ART_H
#define ALBUMLIST_ART_H

#include "config.h"

#if defined(HAVE_TAGCACHE) && defined(HAVE_LCD_COLOR)

#include "list.h"

struct bitmap;

void albumlist_setup_list(struct gui_synclist *list);
bool albumlist_art_service_pending(void);
bool albumlist_art_service_one(void);
void albumlist_art_prefetch_direction(struct gui_synclist *list,
                                      int direction);
void albumlist_art_draw_item(struct list_putlineinfo_t *list_info);
struct bitmap *albumlist_art_get_thumb(const char *album, const char *artist,
                                       int size);
void albumlist_slideshow_set_paused(bool paused);
bool albumlist_slideshow_service(void);
bool albumlist_slideshow_frame_changed(void);
bool albumlist_draw_slideshow_cached(struct screen *display, int x, int y,
                                     int width, int height);
bool albumlist_draw_slideshow(struct screen *display, int x, int y,
                              int width, int height);

#endif

#endif
