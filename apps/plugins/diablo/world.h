#ifndef DIABLO_WORLD_H
#define DIABLO_WORLD_H

#include "plugin.h"

/* Player/entity positions are fixed-point, in 1/16ths of a dPiece grid
 * cell (dPiece cells are half of one DUN/world tile -- see town.py).
 * Sub-cell precision keeps walking from looking like a 32px teleport
 * per step. */
#define WORLD_FP_BITS 4
#define WORLD_FP_ONE (1 << WORLD_FP_BITS)

/* Loads "<name>.meta" + "<name>.cel.raw" from PLUGIN_GAMES_DATA_DIR
 * "/diablo/" (name is "town" or "l1"). Unloads any previously loaded
 * level first. */
bool world_load(const char *name);
void world_unload(void);

/* Composites the tile background for a viewport centered on the given
 * fixed-point world position directly into the LCD driver's back
 * buffer via lcd_bitmap(), but does NOT call lcd_update() -- callers
 * draw entity overlays on top (see world_screen_pos()) and then call
 * world_present() once, so a moving monster doesn't cost a second full
 * tile recomposite. */
void world_render_background(int cam_x_fp, int cam_y_fp);

/* Presents whatever has been drawn (background + overlays) since the
 * last world_render_background() call. */
void world_present(void);

/* Converts a fixed-point world position to LCD pixel coordinates using
 * the camera established by the last world_render_background() call.
 * Always succeeds (out-of-view positions just return off-screen
 * coordinates) -- callers should bounds-check before drawing. */
void world_screen_pos(int x_fp, int y_fp, int *out_sx, int *out_sy);

/* True if the dPiece cell containing fixed-point (x_fp, y_fp) is free of
 * the Solid flag (and in-bounds). Used for movement collision. */
bool world_position_passable(int x_fp, int y_fp);

#endif
