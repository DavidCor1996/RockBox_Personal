#ifndef MICROUI_CONFIG_H
#define MICROUI_CONFIG_H

#include <stdint.h>

/* Bounded Rockbox profile. Keep upstream defaults available to consumers
 * that include microui.h without this compatibility header. */
#define MU_COMMANDLIST_SIZE     (48 * 1024)
#define MU_ROOTLIST_SIZE        8
#define MU_CONTAINERSTACK_SIZE  16
#define MU_CLIPSTACK_SIZE       16
#define MU_IDSTACK_SIZE         16
#define MU_LAYOUTSTACK_SIZE     16
#define MU_CONTAINERPOOL_SIZE   16
#define MU_TREENODEPOOL_SIZE    24
#define MU_MAX_WIDTHS           16
#define MU_REAL                 int32_t
#define MU_REAL_FMT             "%ld"
#define MU_SLIDER_FMT           "%ld"
#define MU_MAX_FMT              31
#define MU_CONTROL_HOOK(ctx, id, rect, opt) \
    mu_rb_control_hook((ctx), (id), (rect), (opt))

#endif
