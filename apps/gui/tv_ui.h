/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TV_UI_H
#define TV_UI_H
#include "config.h"
#include <stdbool.h>
struct gui_synclist;
struct bitmap;
enum tv_section { TV_APPS, TV_VIDEOS, TV_MUSIC, TV_SETTINGS, TV_SECTION_COUNT };
#ifdef HAVE_COMPOSITE_VIDEO_OUT
bool tv_ui_active(void);
bool tv_ui_remote_volume_locked(void);
void tv_ui_set_section(enum tv_section section);
void tv_ui_set_home(bool active, bool tabs);
void tv_ui_set_brand(const struct bitmap *logo, unsigned color);
void tv_ui_release(void);
void tv_ui_batch(bool begin, bool wps);
void tv_list_draw(struct gui_synclist *list);
void tv_list_art_draw(struct gui_synclist *list, const struct bitmap *cover);
void tv_grid_draw(struct gui_synclist *list, const struct bitmap * const *icons,
                  int first, int columns);
void tv_home_videos_draw(struct gui_synclist *list,
                        const struct bitmap * const *icons,
                        const struct bitmap *banner, const char *title,
                        bool focused, int selected, int count);
void tv_shelf_draw(const char *title, const char *name,
                   const struct bitmap * const *posters,
                   const struct bitmap *banner, const char *footer,
                   int selected, int count, unsigned watched,
                   const struct bitmap *watched_badge);
void tv_detail_draw(const char *title, const struct bitmap *poster,
                    const struct bitmap *banner, const char *metadata, const char *plot, const char *footer, int choice);
void tv_wps_enter(void);
void tv_wps_draw(void);
bool tv_video_prepare(const unsigned char * const planes[3],
    int width, int height, int stride, int dar_n, int dar_d,
    unsigned long elapsed_ms, unsigned long duration_ms,
    bool paused, bool show_status, const unsigned char *caption);
void tv_wps_service(bool idle);
void tv_ui_leave(void);
void tv_dialog_draw(const char *title, const char * const *lines, int count,
                    const char *footer);
int tv_test_screen(void);
#else
static inline bool tv_ui_active(void) { return false; }
static inline bool tv_ui_remote_volume_locked(void) { return false; }
static inline void tv_ui_set_section(enum tv_section s) { (void)s; }
static inline void tv_ui_set_home(bool active, bool tabs) { (void)active; (void)tabs; }
static inline void tv_ui_set_brand(const struct bitmap *logo, unsigned color)
{ (void)logo; (void)color; }
static inline void tv_ui_release(void) {}
static inline void tv_ui_batch(bool b, bool w) { (void)b; (void)w; }
static inline void tv_list_draw(struct gui_synclist *l) { (void)l; }
static inline void tv_list_art_draw(struct gui_synclist *l,const struct bitmap *b)
{ (void)l; (void)b; }
static inline void tv_grid_draw(struct gui_synclist *l,const struct bitmap * const *b,int f,int c)
{ (void)l; (void)b; (void)f; (void)c; }
static inline void tv_home_videos_draw(struct gui_synclist*l,
    const struct bitmap*const*i,const struct bitmap*b,const char*t, bool f,int s,int c)
{ (void)l; (void)i; (void)b; (void)t; (void)f; (void)s; (void)c; }
static inline void tv_shelf_draw(const char*t,const char*n,const struct bitmap*const*p,const struct bitmap*b,const char*f,int selected,int count,unsigned watched,const struct bitmap*badge)
{ (void)t; (void)n; (void)p; (void)b; (void)f; (void)selected; (void)count; (void)watched; (void)badge; }
static inline void tv_detail_draw(const char*t,const struct bitmap*p,const struct bitmap*h,const char*m,const char*b,const char*f,int choice)
{ (void)t; (void)p; (void)h; (void)m; (void)b; (void)f; (void)choice; }
static inline void tv_wps_enter(void) {}
static inline void tv_wps_draw(void) {}
static inline void tv_wps_service(bool idle) { (void)idle; }
static inline void tv_ui_leave(void) {}
static inline void tv_dialog_draw(const char *t, const char * const *l,
                                 int n, const char *f)
{ (void)t; (void)l; (void)n; (void)f; }
#endif
#endif
