#ifndef FLASHPLAYER_GAMESWF_COMPAT_MALLOC_H
#define FLASHPLAYER_GAMESWF_COMPAT_MALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mallinfo {
    int arena;
    int ordblks;
    int smblks;
    int hblks;
    int hblkhd;
    int usmblks;
    int fsmblks;
    int uordblks;
    int fordblks;
    int keepcost;
};

#ifdef __cplusplus
}
#endif

#endif
