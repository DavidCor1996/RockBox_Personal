/* Shared Photos collection discovery. GPL-2.0-or-later. */
#ifndef PHOTO_LIBRARY_H
#define PHOTO_LIBRARY_H
#include <stdbool.h>
#include <stddef.h>
bool photo_library_supported(const char *name);
/* Caller owns bounded scratch. Never requests plugin or playback memory. */
bool photo_library_root(char *root, size_t root_size,
                        void *scratch, size_t scratch_size);
#endif
