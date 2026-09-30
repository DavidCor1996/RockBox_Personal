#ifndef NANO_ACTOR_MODULES_H
#define NANO_ACTOR_MODULES_H

#include "z64actor.h"

/* acquire owns a module reference (and its dependency closure) until release.
 * NULL leaves no ownership behind. Calls and returned profiles must remain
 * valid while any instance of the actor exists. No eviction by age is allowed. */
typedef const ActorInit *(*nano_actor_acquire)(void *, int, const char *);
typedef void (*nano_actor_release)(void *, int);

/* Configure only before any module is acquired. Returns zero if busy. */
int nano_actor_modules_configure(nano_actor_acquire, nano_actor_release, void *);
void nano_actor_module_release(int);
/* Renderer calls this ONLY after submitted draws no longer reference actor
 * module data. Without a proven GPU fence, retired code must stay allocated. */
void nano_actor_modules_collect(void);

#endif
