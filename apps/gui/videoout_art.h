#ifndef GUI_VIDEOOUT_ART_H
#define GUI_VIDEOOUT_ART_H
#include "config.h"
#include <stdbool.h>
#include "lcd.h"
#if defined(VIDEOOUT_ENHANCED_TEST) && defined(HAVE_COMPOSITE_VIDEO_OUT) && !defined(SIMULATOR)
void tv_art_draw(const char *path, const fb_data *pixels, int stride,
                 int x, int y);
void tv_art_service(bool idle);
void tv_art_leave(void);
bool tv_art_changed(void);
#else
static inline void tv_art_draw(const char *path, const fb_data *pixels,
                               int stride, int x, int y)
{ (void)path; (void)pixels; (void)stride; (void)x; (void)y; }
static inline void tv_art_service(bool idle) { (void)idle; }
static inline void tv_art_leave(void) { }
static inline bool tv_art_changed(void) { return false; }
#endif
#endif
