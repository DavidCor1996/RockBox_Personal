#ifndef ALBUMLIST_ART_H
#define ALBUMLIST_ART_H

#include "config.h"

#if defined(HAVE_TAGCACHE) && defined(HAVE_LCD_COLOR)

#include "list.h"

void albumlist_setup_list(struct gui_synclist *list);
void albumlist_art_draw_item(struct list_putlineinfo_t *list_info);

#endif

#endif
