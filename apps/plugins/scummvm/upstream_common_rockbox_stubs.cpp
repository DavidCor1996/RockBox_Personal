/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
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
#include "common/events.h"
#include "common/savefile.h"

namespace Common {

EventDispatcher::EventDispatcher() : _autoFreeMapper(false), _mapper(0)
{
}

EventDispatcher::~EventDispatcher()
{
}

void EventDispatcher::dispatch()
{
}

void EventDispatcher::registerMapper(EventMapper *mapper, bool autoFree)
{
	(void)mapper;
	(void)autoFree;
}

List<Event> DefaultEventMapper::mapEvent(const Event &ev, EventSource *source)
{
	(void)source;
	List<Event> events;
	events.push_back(ev);
	return events;
}

List<Event> DefaultEventMapper::getDelayedEvents()
{
	return List<Event>();
}

void DefaultEventMapper::addDelayedEvent(uint32 millis, Event ev)
{
	(void)millis;
	(void)ev;
}

void EventDispatcher::registerSource(EventSource *source, bool autoFree)
{
	(void)source;
	(void)autoFree;
}

void EventDispatcher::unregisterSource(EventSource *source)
{
	(void)source;
}

void EventDispatcher::registerObserver(EventObserver *obs, uint priority,
                                       bool autoFree, bool listenPolls)
{
	(void)obs;
	(void)priority;
	(void)autoFree;
	(void)listenPolls;
}

void EventDispatcher::unregisterObserver(EventObserver *obs)
{
	(void)obs;
}

void EventDispatcher::dispatchEvent(const Event &event)
{
	(void)event;
}

void EventDispatcher::dispatchPoll()
{
}

String SaveFileManager::popErrorDesc()
{
	String desc = _errorDesc;
	clearError();
	return desc;
}

OutSaveFile::OutSaveFile(WriteStream *w) : _wrapped(w)
{
}

OutSaveFile::~OutSaveFile()
{
	finalize();
}

bool OutSaveFile::err() const
{
	return !_wrapped || _wrapped->err();
}

void OutSaveFile::clearErr()
{
	if (_wrapped)
		_wrapped->clearErr();
}

void OutSaveFile::finalize()
{
	if (_wrapped)
		_wrapped->flush();
}

bool OutSaveFile::flush()
{
	return _wrapped && _wrapped->flush();
}

uint32 OutSaveFile::write(const void *dataPtr, uint32 dataSize)
{
	return _wrapped ? _wrapped->write(dataPtr, dataSize) : 0;
}

int32 OutSaveFile::pos() const
{
	return _wrapped ? _wrapped->pos() : 0;
}

bool SaveFileManager::renameSavefile(const String &oldName,
                                     const String &newName)
{
	(void)oldName;
	(void)newName;
	return false;
}

bool SaveFileManager::copySavefile(const String &oldName,
                                   const String &newName)
{
	(void)oldName;
	(void)newName;
	return false;
}

} // End of namespace Common
