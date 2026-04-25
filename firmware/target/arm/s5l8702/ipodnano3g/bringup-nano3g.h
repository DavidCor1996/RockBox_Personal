#ifndef IPODNANO3G_BRINGUP_H
#define IPODNANO3G_BRINGUP_H

#include "config.h"
#include <stdbool.h>

#if defined(IPOD_NANO3G)
void nano3g_boottrace_reset(void);
void nano3g_boottrace_log(const char *stage);
void nano3g_boottrace_enable_lcd(bool enable);
int nano3g_boottrace_count(void);
const char *nano3g_boottrace_get(int index);
bool nano3g_safe_mode_enabled(void);
void nano3g_failsafe_halt(const char *reason);
#else
static inline void nano3g_boottrace_reset(void) {}
static inline void nano3g_boottrace_log(const char *stage) {(void)stage;}
static inline void nano3g_boottrace_enable_lcd(bool enable) {(void)enable;}
static inline int nano3g_boottrace_count(void) { return 0; }
static inline const char *nano3g_boottrace_get(int index)
{
    (void)index;
    return "";
}
static inline bool nano3g_safe_mode_enabled(void) { return false; }
static inline void nano3g_failsafe_halt(const char *reason)
{
    (void)reason;
}
#endif

#endif
