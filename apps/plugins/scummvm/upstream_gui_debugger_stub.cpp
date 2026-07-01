/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_ /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 Rockpod contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/

#include "lib/plugin_cxx_compat.h"
#include "gui/debugger.h"

namespace GUI {

Debugger::Debugger()
    : _frameCountdown(0), _isActive(false), _errStr(0), _firstTime(false)
#ifndef USE_TEXT_CONSOLE_FOR_DEBUGGER
    , _debuggerDialog(0)
#endif
{
}

Debugger::~Debugger()
{
}

int Debugger::getCharsPerLine()
{
    return 40;
}

int Debugger::debugPrintf(const char *format, ...)
{
    (void)format;
    return 0;
}

void Debugger::debugPrintColumns(const Common::StringArray &list)
{
    (void)list;
}

void Debugger::onFrame()
{
}

void Debugger::attach(const char *entry)
{
    (void)entry;
}

void Debugger::registerVar(const Common::String &varname, void *variable,
                           VarType type, int arraySize)
{
    (void)varname;
    (void)variable;
    (void)type;
    (void)arraySize;
}

void Debugger::registerCmd(const Common::String &cmdname,
                           Debuglet *debuglet)
{
    (void)cmdname;
    delete debuglet;
}

void Debugger::preEnter()
{
}

void Debugger::postEnter()
{
}

void Debugger::detach()
{
    _isActive = false;
}

void Debugger::enter()
{
}

bool Debugger::parseCommand(const char *input)
{
    (void)input;
    return false;
}

bool Debugger::tabComplete(const char *input,
                           Common::String &completion) const
{
    (void)input;
    (void)completion;
    return false;
}

bool Debugger::handleCommand(int argc, const char **argv, bool &keepRunning)
{
    (void)argc;
    (void)argv;
    keepRunning = false;
    return false;
}

bool Debugger::cmdExit(int argc, const char **argv)
{
    (void)argc;
    (void)argv;
    return false;
}

bool Debugger::cmdHelp(int argc, const char **argv)
{
    (void)argc;
    (void)argv;
    return false;
}

bool Debugger::cmdOpenLog(int argc, const char **argv)
{
    (void)argc;
    (void)argv;
    return false;
}

} /* namespace GUI */
