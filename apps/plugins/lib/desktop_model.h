/* Desktop geometry and navigation. GPL-2.0-or-later. */
#ifndef DESKTOP_MODEL_H
#define DESKTOP_MODEL_H
#include <stdbool.h>
#include <stdint.h>

struct desktop_rect { int x, y, width, height; };
enum desktop_scale { DESKTOP_FILL, DESKTOP_FIT, DESKTOP_CENTER };
/* Source crop and destination rectangle, with integer pixel coordinates. */
bool desktop_image_layout(int sw, int sh, int dw, int dh,
                          enum desktop_scale mode,
                          struct desktop_rect *src, struct desktop_rect *dst);
bool desktop_hit(struct desktop_rect r, int x, int y);
void desktop_damage(struct desktop_rect *damage, struct desktop_rect add);

enum desktop_media { DESKTOP_SONGS, DESKTOP_ALBUMS, DESKTOP_ARTISTS,
                     DESKTOP_GENRES, DESKTOP_VIDEOS, DESKTOP_PLAYLISTS };
struct desktop_library_level
{
    enum desktop_media source;
    int32_t artist, album, genre;
    int page, selected;
};
struct desktop_library
{
    struct desktop_library_level current, history[3];
    int depth;
};
void desktop_library_root(struct desktop_library *library,
                          enum desktop_media source);
bool desktop_library_open(struct desktop_library *library, int32_t id);
bool desktop_library_back(struct desktop_library *library);

enum desktop_event_type { DESKTOP_POINTER_MOVE, DESKTOP_POINTER_BUTTON,
                          DESKTOP_POINTER_WHEEL, DESKTOP_KEY, DESKTOP_TEXT };
enum desktop_key { DESKTOP_TAB = 256, DESKTOP_ENTER, DESKTOP_ESCAPE,
                   DESKTOP_UP, DESKTOP_DOWN, DESKTOP_LEFT, DESKTOP_RIGHT,
                   DESKTOP_BACKSPACE };
#define DESKTOP_MOD_SHIFT 1
#define DESKTOP_MOD_CTRL 2
struct desktop_event
{
    enum desktop_event_type type;
    int x, y, value;
    unsigned int modifiers;
    bool down;
};
#endif
