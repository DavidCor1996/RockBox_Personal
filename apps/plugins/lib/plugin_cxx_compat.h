/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/                \/
 *
 * C++ wrapper for Rockbox's C plugin API headers.
 *
 ****************************************************************************/

#ifndef PLUGIN_CXX_COMPAT_H
#define PLUGIN_CXX_COMPAT_H

extern "C" {
/*
 * Some Rockbox C APIs use identifiers that are C++ keywords or collide with
 * host C++ overloads. Rename only while parsing the C headers.
 */
#define new rb_cxx_keyword_new
#define old rb_cxx_keyword_old
#define __STRLCASESTR_H__
#include "plugin.h"
#undef __STRLCASESTR_H__
#undef old
#undef new
}

#endif
