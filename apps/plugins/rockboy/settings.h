#ifndef ROCKBOY_SETTINGS_H
#define ROCKBOY_SETTINGS_H

#include "plugin.h"

#define ROCKBOY_SAVE_DIR ROCKBOX_DIR "/rockboy"
#define ROCKBOY_OPTIONS_FILE "options"

struct options {
   int A, B, START, SELECT, MENU;
   int UP, DOWN, LEFT, RIGHT;
   int frameskip, fps, maxskip;
   int sound, scaling, showstats;
   int autosave;
   int rotate;
   int pal;
   int dirty;
   int control_preset;
   int performance_preset;
   int profile;
};

enum rockboy_control_preset {
    ROCKBOY_CTRL_CLASSIC = 0,
    ROCKBOY_CTRL_IPOD5G = 1,
};

enum rockboy_performance_preset {
    ROCKBOY_PERF_BALANCED = 0,
    ROCKBOY_PERF_PERFORMANCE = 1,
    ROCKBOY_PERF_QUALITY = 2,
};

#endif
