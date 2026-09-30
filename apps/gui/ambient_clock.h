/* Docked idle display. All entry points run in the owning UI thread. */
#ifndef AMBIENT_CLOCK_H
#define AMBIENT_CLOCK_H
#include "config.h"
#include <stdbool.h>
#ifdef HAVE_DOCKED_AMBIENT_CLOCK
/* Poll only from explicitly eligible host loops. Activity resets the delay. */
bool ambient_clock_ready(bool activity);
/* Returns a transport/system action, or ACTION_NONE for a consumed wake. */
int ambient_clock_run(bool preview);
/* Copy colors from completed artwork; never retain the bitmap or metadata. */
void ambient_clock_palette(unsigned int dominant, unsigned int accent);
#else
static inline bool ambient_clock_ready(bool activity)
{ (void)activity; return false; }
static inline int ambient_clock_run(bool preview)
{ (void)preview; return 0; }
static inline void ambient_clock_palette(unsigned int a, unsigned int b)
{ (void)a; (void)b; }
#endif
#endif
