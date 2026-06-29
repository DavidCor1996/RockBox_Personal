/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/                \/
 *
 * Minimal C++ runtime hooks for plugins.
 *
 ****************************************************************************/

#ifndef PLUGIN_CXX_H
#define PLUGIN_CXX_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void plugin_cxx_init(void *buffer, size_t buffer_size);
size_t plugin_cxx_available(void);

#ifdef __cplusplus
}
#endif

#endif
