#ifndef ROCKBOX_POKEMINI_LIBRETRO_H
#define ROCKBOX_POKEMINI_LIBRETRO_H

#include <stdarg.h>

enum retro_log_level
{
    RETRO_LOG_DEBUG = 0,
    RETRO_LOG_INFO,
    RETRO_LOG_WARN,
    RETRO_LOG_ERROR,
};

typedef void (*retro_log_printf_t)(enum retro_log_level level,
                                   const char *fmt, ...);

#endif
